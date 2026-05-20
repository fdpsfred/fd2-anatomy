"""Build the 12-group worklist for the data-item structural audit.

Inputs:
* `workspace/data_audit/ghidra_data_dump_<utc>.json` — latest fresh Ghidra
  dump (single source of truth; produced by Claude Code running
  MCP `list_data_items` and writing the result here)
* `workspace/data_audit/le_fixups.json` — optional; LE FIXUP reverse
  index produced by `parse_le_fixup.py` (used for B5 classification). If
  absent, every 0-xref item is provisionally B6 and re-classified once
  parse_le_fixup runs.
* `workspace/data_audit/xref_graph.json` — optional; precomputed direct
  + indirect xref bipartite (produced by `build_xref_graph.py`). If
  absent, bucket B2-B4 fall back to "needs_live_xref" and the per-item
  reviewer pulls `get_xrefs_to` live.
* `program_info/**/*.md` — fd2 subsystem mapping
* `workspace/data_audit/worklist.json` — previous worklist (verdicts
  preserved across rebuilds)

Output:
* `workspace/data_audit/worklist.json` — one row per Ghidra data item
  `{addr, current_name, segment, bucket, group, group_label, subsystem,
    locked, status, problem_classes, verdict, notes}`
* stdout diff report — Ghidra-only / worklist-only entries flagged as
  warnings.

Mechanical only: no verdict derivation, no name judgement. Group is
assigned from name prefix + segment + bucket. Bucket is assigned from
xref topology (see _index.md). Per-item review writes status/verdict
back via `record_verdict.py`.
"""

from __future__ import annotations

import json
import re
import sys
from pathlib import Path

REPO = Path(__file__).resolve().parents[3]
WORKSPACE = REPO / "workspace" / "data_audit"
PROGRAM_INFO = REPO / "program_info"
WORKLIST = WORKSPACE / "worklist.json"
LE_FIXUPS = WORKSPACE / "le_fixups.json"
XREF_GRAPH = WORKSPACE / "xref_graph.json"

GROUP_LABELS = {
    "D1": "padding",
    "D2": "le_struct",
    "D3": "vendor_string",
    "D4": "fd2_game_table",
    "D5": "fd2_o3_state",
    "D6": "fd2_o2_lookup",
    "D7": "fd2_o2_runtime",
    "D8": "fd2_o1_inline",
    "D9": "ail_o1_dispatch",
    "D10": "ail_o2_globals",
    "D11": "orphan_indirect",
    "D12": "residual",
}

# Address-range hints for fd2 game data tables (D4) — these are LOCKED:
# struct types are correct and only need rename + plate.
FD2_GAME_TABLE_ADDRS = {
    "000602ac": ("data_fd2_battle_item_effect_table",     "battle", 215, 23),
    "000619fd": ("data_fd2_battle_spell_effect_table",    "battle",  36,  7),
    "00061af9": ("data_fd2_battle_enemy_data_table",      "battle",  68, 10),
    "00061da1": ("data_fd2_battle_character_base_table",  "battle",  32, 24),
    "000620a1": ("data_fd2_battle_character_growth_table","battle",  68, 11),
    "0006238d": ("data_fd2_chapter_intro_metadata_table", "chapter", 26, 31),
    "000626b3": ("data_fd2_battle_spell_learning_table",  "battle",  20, 12),
}

# .object3 boundary (game data lives here; runtime state in .object2)
OBJ3_START = 0x00060000
OBJ3_END   = 0x000634d1
OBJ2_START = 0x00050000
OBJ2_END   = 0x000556af
OBJ1_START = 0x00010000
OBJ1_END   = 0x0004ebd8


def _unwrap(text: str):
    blob = json.loads(text)
    if isinstance(blob, dict) and "result" in blob and isinstance(blob["result"], str):
        return json.loads(blob["result"])
    return blob


def normalise_addr(s) -> str:
    if isinstance(s, int):
        return f"{s:08x}"
    s = s.strip().lower()
    if s.startswith("0x"):
        s = s[2:]
    return s.zfill(8)


def find_latest_dump() -> Path:
    WORKSPACE.mkdir(parents=True, exist_ok=True)
    dumps = sorted(WORKSPACE.glob("ghidra_data_dump_*.json"))
    if not dumps:
        raise SystemExit("ERROR: no ghidra_data_dump_*.json in workspace/data_audit/")
    return dumps[-1]


def load_xref_graph() -> dict:
    if not XREF_GRAPH.exists():
        return {}
    try:
        return json.loads(XREF_GRAPH.read_text(encoding="utf-8"))
    except json.JSONDecodeError:
        return {}


def load_le_fixups() -> dict:
    if not LE_FIXUPS.exists():
        return {}
    try:
        return json.loads(LE_FIXUPS.read_text(encoding="utf-8"))
    except json.JSONDecodeError:
        return {}


def classify_caller_pool(caller_name: str) -> str:
    """Return 'ail' / 'crt' / 'fd2' / 'binary_artifact' from a caller
    function name."""
    if caller_name.startswith("AIL_"):
        return "ail"
    if (caller_name.startswith("crt_")
            or caller_name.startswith("L$")
            or caller_name.startswith("IF@")
            or caller_name.startswith("__")
            or caller_name.startswith("_")):
        return "crt"
    if caller_name.startswith("fd2_"):
        return "fd2"
    if caller_name.startswith("binary_artifact_"):
        return "binary_artifact"
    return "fd2"


def assign_bucket(addr_int: int, name: str,
                  direct_xref_callers: list[str] | None,
                  indirect_xref_count: int) -> tuple[str, str]:
    """Return (bucket, caller_pool).

    bucket ∈ B0..B6 per plan §B'.
    caller_pool ∈ ail / crt / fd2 / mixed / vendor_lib / unknown.
    """
    if name.startswith("IMG_"):
        return ("B0", "le_loader")

    callers = direct_xref_callers or []
    pools = {classify_caller_pool(c) for c in callers}

    if not callers:
        if indirect_xref_count > 0:
            return ("B5", "indirect")
        return ("B6", "unknown")

    fd2_callers = [c for c in callers if classify_caller_pool(c) == "fd2"]
    vendor_callers = [c for c in callers
                      if classify_caller_pool(c) in {"ail", "crt"}]

    if vendor_callers and not fd2_callers:
        if pools == {"ail"}:
            return ("B1", "ail")
        if pools == {"crt"}:
            return ("B1", "crt")
        return ("B1", "vendor_lib")

    if len(fd2_callers) == 1 and not vendor_callers:
        return ("B2", "fd2")
    if len(fd2_callers) >= 2 and not vendor_callers:
        return ("B3", "fd2")
    if fd2_callers and vendor_callers:
        return ("B3", "mixed")
    return ("B2", "fd2")


def parse_program_info() -> dict[str, tuple[str, str]]:
    """Scan program_info/**/*.md for `@ 0x<addr>` patterns referencing
    data items, returning addr (8-hex lowercase) → (group, subsystem).

    Mirrors tools/program_analysis/function_audit/build_worklist.py SYSTEM_GROUP_MAP but
    targets data addresses (.object2 / .object3) rather than functions.
    """
    SYSTEM_GROUP_MAP = {
        "battle.md":      ("D7", "battle"),
        "input.md":       ("D7", "input"),
        "animation.md":   ("D7", "animation"),
        "graphics.md":    ("D7", "graphics"),
        "audio.md":       ("D7", "audio"),
        "resource.md":    ("D7", "resource"),
        "save_load.md":   ("D7", "save_load"),
        "field_map.md":   ("D7", "field_map"),
        "table_accessor.md": ("D7", "field_map"),
        "ui_menu.md":     ("D7", "ui"),
        "text_dialog.md": ("D7", "ui"),
        "chapter_event_dispatch.md": ("D7", "chapter"),
    }
    mapping: dict[str, tuple[str, str]] = {}
    pat_addr = re.compile(r"@\s*`?\s*0x([0-9a-fA-F]+)\b")

    for md in PROGRAM_INFO.rglob("*.md"):
        rel = md.name
        if md.parent.name == "chapters":
            group, subsystem = "D7", "chapter"
        elif rel in SYSTEM_GROUP_MAP:
            group, subsystem = SYSTEM_GROUP_MAP[rel]
        else:
            continue
        text = md.read_text(encoding="utf-8", errors="ignore")
        for m in pat_addr.finditer(text):
            addr = m.group(1).lower().zfill(8)
            if addr not in mapping:
                mapping[addr] = (group, subsystem)
    return mapping


def assign_group(addr_int: int, name: str, segment: str, bucket: str,
                 caller_pool: str,
                 sys_by_addr: dict[str, tuple[str, str]]
                 ) -> tuple[str, str, str, bool]:
    """Return (group, group_label, subsystem, locked). Group precedence:

    1. align_fill_* / data_align_* / binary_artifact_align_* → D1 padding
    2. IMG_* → D2 LE structures (LOCKED layout)
    3. game-data tables (7 hard-coded addrs) → D4 (LOCKED)
    4. s_*  vendor strings → D3
    5. AIL/CRT pool caller-only → D9 (.object1) / D10 (.object2)
    6. fd2 named (non-DAT_ / non-BYTE_ARRAY_) in .object3 → D5
    7. fd2 named (non-DAT_/non-BYTE_) in .object2 → D6
    8. fd2 auto-named (DAT_ / BYTE_ARRAY_) in .object2 → D7
    9. fd2 in .object1 inline → D8
   10. B5 indirect → D11
   11. B6 orphan → D12

    Subsystem default 'unknown' unless program_info @ 0xADDR mapped.
    """
    addr = normalise_addr(addr_int)

    # 1. padding
    if (name.startswith("align_fill_")
            or name.startswith("data_align_")
            or name.startswith("binary_artifact_align_")):
        return ("D1", "padding", "shared", False)

    # 2. LE structures (IMG_* original, or data_le_* renamed)
    if name.startswith("IMG_") or name.startswith("data_le_"):
        return ("D2", "le_struct", "le_loader", True)

    # 3. fd2 game data tables — addr-locked
    if addr in FD2_GAME_TABLE_ADDRS:
        canon_name, subsys, _count, _stride = FD2_GAME_TABLE_ADDRS[addr]
        return ("D4", "fd2_game_table", subsys, True)

    # 4. strings (s_* prefix original, or data_string_* renamed)
    if name.startswith("s_") or name.startswith("data_string_"):
        # Derive caller_pool from new name when reverse-resolving
        if name.startswith("data_string_ail_"):
            return ("D3", "vendor_string", "ail", False)
        if name.startswith("data_string_crt_"):
            return ("D3", "vendor_string", "crt", False)
        if name.startswith("data_string_fd2_") or name.startswith("data_string_orphan_"):
            return ("D3", "vendor_string", "fd2", False)
        # Routed by caller_pool: if vendor, treat as D3 vendor string,
        # else fall through to fd2 routing below (string used by game)
        if caller_pool in {"ail", "crt", "vendor_lib"}:
            return ("D3", "vendor_string", caller_pool, False)
        # fd2-consumed strings still join D3 (renamed `data_string_fd2_*`)
        return ("D3", "vendor_string", "fd2", False)

    # subsystem hint (program_info /…/*.md)
    subsys_hint = sys_by_addr.get(addr, (None, None))[1]

    # 5-6. vendor pool (ail/crt-only caller)
    if caller_pool in {"ail", "crt", "vendor_lib"}:
        if OBJ1_START <= addr_int <= OBJ1_END:
            return ("D9", "ail_o1_dispatch", caller_pool, False)
        if OBJ2_START <= addr_int <= OBJ2_END:
            return ("D10", "ail_o2_globals", caller_pool, False)
        # vendor pool but lives in .object3 — atypical; fall through to D11/D12
        return ("D11", "orphan_indirect", caller_pool, False)

    # Named vs auto-named in fd2 domain
    is_auto_name = (name.startswith("DAT_")
                    or name.startswith("BYTE_ARRAY_")
                    or name.startswith("fix_off32_")
                    or name.startswith("switchD")
                    or name.startswith("switchdataD_")
                    or name.startswith("LE_PAGE_")
                    or name.startswith("L_")
                    or name.startswith("LAB_"))

    # 7-10. segment-based routing for fd2-domain items (precedes B5/B6 fallthrough
    # so named .object3/.object2/.object1 items aren't mis-routed to D11/D12 when
    # xref_graph.json isn't available yet)
    if OBJ3_START <= addr_int <= OBJ3_END:
        return ("D5", "fd2_o3_state", subsys_hint or "shared", False)
    if OBJ2_START <= addr_int <= OBJ2_END:
        if is_auto_name:
            return ("D7", "fd2_o2_runtime", subsys_hint or "shared", False)
        return ("D6", "fd2_o2_lookup", subsys_hint or "shared", False)
    if OBJ1_START <= addr_int <= OBJ1_END:
        return ("D8", "fd2_o1_inline", subsys_hint or "shared", False)

    # 11-12. bucket-driven catch-all for items outside .object1/2/3 segments
    if bucket == "B5":
        return ("D11", "orphan_indirect", subsys_hint or "unknown", False)
    if bucket == "B6":
        return ("D12", "residual", subsys_hint or "unknown", False)

    return ("D12", "residual", subsys_hint or "unknown", False)


def load_existing_worklist() -> dict[str, dict]:
    if not WORKLIST.exists():
        return {}
    try:
        blob = json.loads(WORKLIST.read_text(encoding="utf-8"))
    except json.JSONDecodeError:
        return {}
    return {row["addr"]: row for row in blob.get("rows", [])}


def main() -> int:
    dump_path = find_latest_dump()
    print(f"reading dump: {dump_path.name}")
    payload = _unwrap(dump_path.read_text(encoding="utf-8"))
    items = payload["items"]
    print(f"  {len(items)} items in Ghidra dump")

    xref_graph = load_xref_graph()
    le_fixups = load_le_fixups()
    data_to_fn = xref_graph.get("data_to_fn", {})
    target_to_src = le_fixups.get("target_addr_to_sources", {})

    sys_by_addr = parse_program_info()
    print(f"  {len(sys_by_addr)} addresses → subsystem mapped from program_info/")

    existing = load_existing_worklist()
    rows = []
    new_count = 0
    diff_warnings = []
    seen_addrs: set[str] = set()

    for it in items:
        addr = normalise_addr(it["addr"])
        addr_int = int(addr, 16)
        name = it["name"]
        seen_addrs.add(addr)

        direct_callers = data_to_fn.get(addr)  # may be None if no graph yet
        indirect_count = len(target_to_src.get(addr, []))
        bucket, caller_pool = assign_bucket(addr_int, name,
                                            direct_callers, indirect_count)
        group, label, subsystem, locked = assign_group(
            addr_int, name, it["segment"], bucket, caller_pool, sys_by_addr)

        row = {
            "addr": addr,
            "current_name": name,
            "type": it.get("type", ""),
            "size": it.get("size", 0),
            "segment": it["segment"],
            "bucket": bucket,
            "caller_pool": caller_pool,
            "group": group,
            "group_label": label,
            "subsystem": subsystem,
            "locked": locked,
            "direct_caller_count": (len(direct_callers)
                                    if direct_callers is not None else None),
            "indirect_xref_count": indirect_count,
        }

        if addr in existing:
            prev = existing[addr]
            row["status"] = prev.get("status", "pending")
            row["problem_classes"] = prev.get("problem_classes", [])
            row["verdict"] = prev.get("verdict")
            row["notes"] = prev.get("notes", "")
            if prev.get("current_name") != name:
                row["notes"] = (
                    f"renamed from {prev.get('current_name')} "
                    f"(prev status={prev.get('status')}); {row['notes']}"
                ).strip("; ")
        else:
            new_count += 1
            diff_warnings.append(f"NEW in Ghidra (not in prev worklist): {addr} {name}")
            row["status"] = "pending"
            row["problem_classes"] = []
            row["verdict"] = None
            row["notes"] = ""

        rows.append(row)

    deleted_count = 0
    for addr, prev in existing.items():
        if addr not in seen_addrs:
            deleted_count += 1
            diff_warnings.append(
                f"DELETED in Ghidra (was in prev worklist): {addr} "
                f"{prev.get('current_name')} (prev status={prev.get('status')})"
            )
            row = dict(prev)
            row["status"] = "deleted_in_ghidra"
            row["notes"] = (f"vanished from Ghidra dump; was "
                            f"{prev.get('current_name')} prev status="
                            f"{prev.get('status')}")
            rows.append(row)

    # Group / bucket counts (live only)
    group_counts: dict[str, int] = {}
    bucket_counts: dict[str, int] = {}
    for row in rows:
        if row["status"] == "deleted_in_ghidra":
            continue
        group_counts[row["group"]] = group_counts.get(row["group"], 0) + 1
        bucket_counts[row["bucket"]] = bucket_counts.get(row["bucket"], 0) + 1

    blob = {
        "ghidra_dump": dump_path.name,
        "data_item_count_in_ghidra": len(items),
        "row_count": len(rows),
        "group_counts": group_counts,
        "bucket_counts": bucket_counts,
        "xref_graph_loaded": bool(xref_graph),
        "le_fixups_loaded": bool(le_fixups),
        "rows": rows,
    }
    WORKLIST.write_text(json.dumps(blob, indent=2, ensure_ascii=False), encoding="utf-8")
    print(f"\nworklist written: {WORKLIST.relative_to(REPO)}")
    print(f"  rows:     {len(rows)}")
    print(f"  new:      {new_count}")
    print(f"  deleted:  {deleted_count}")
    print(f"  group split (live only):")
    for g in sorted(group_counts):
        label = GROUP_LABELS.get(g, "?")
        print(f"    {g:4s} {label:18s} {group_counts[g]:4d}")
    print(f"  bucket split (live only):")
    for b in sorted(bucket_counts):
        print(f"    {b}                  {bucket_counts[b]:4d}")

    if not xref_graph:
        print("\n  NOTE: xref_graph.json absent — bucket classification is "
              "structural-only (B0/B6 deterministic; B1-B5 require live "
              "get_xrefs_to per item). Run build_xref_graph.py before D7.")
    if not le_fixups:
        print("  NOTE: le_fixups.json absent — B5 indirect-only items cannot "
              "be discriminated from B6 true orphan. Run parse_le_fixup.py "
              "before D11.")

    if diff_warnings:
        print(f"\nWARNINGS ({len(diff_warnings)}):")
        for w in diff_warnings[:20]:
            print(f"  {w}")
        if len(diff_warnings) > 20:
            print(f"  ... ({len(diff_warnings) - 20} more)")

    return 0


if __name__ == "__main__":
    sys.exit(main())
