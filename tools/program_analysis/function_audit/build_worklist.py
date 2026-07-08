"""Build the 9-group worklist for the full-function structural audit.

Inputs:
* `workspace/function_audit/ghidra_dump_<utc>.json` — latest fresh Ghidra dump
  (single source of truth; produced by Claude Code running MCP
  `list_functions_enhanced` and writing the raw JSON here)
* `rebuild_info/crt/lookup_9.5a.json` — locked CRT names (G2)
* `program_info/**/*.md` — fd2_* function → system mapping (G5–G8)
* `workspace/function_audit/worklist.json` — previous worklist (verdicts
  preserved across rebuilds)

Output:
* `workspace/function_audit/worklist.json` — one row per Ghidra function
  `{addr, current_name, group, locked, status, problem_classes, verdict, notes}`
* stdout diff report — Ghidra-only / worklist-only entries, both flagged as
  warnings; script returns 1 if any diff is unhandled

Mechanical only: no verdict derivation, no name judgement. Group assignment
is purely from name prefix + lookup membership + program_info filename
membership. Per-function review writes status/verdict back via
`record_verdict.py`.
"""

from __future__ import annotations

import json
import re
import sys
from pathlib import Path

REPO = Path(__file__).resolve().parents[3]
WORKSPACE = REPO / "workspace" / "function_audit"
PROGRAM_INFO = REPO / "program_info"
CRT_LOOKUP = REPO / "rebuild_info" / "crt" / "lookup_9.5a.json"
WORKLIST = WORKSPACE / "worklist.json"

# Mirror tools/program_analysis/build_call_graph.py PUBLIC_CRT_SYMBOLS.
# Kept inline (not imported) so build_worklist.py has no run-time dep on
# the call-graph build script; changes there don't silently shift group
# boundaries.
PUBLIC_CRT_SYMBOLS = {
    "malloc", "free", "_nmalloc", "_nfree",
    "fread", "fwrite", "fopen", "fclose", "fseek", "fgets", "fputs",
    "getc", "putc", "vfprintf", "fprintf", "sprintf",
    "open", "close", "read", "write", "lseek", "_tell", "_filelength",
    "memcpy", "memmove", "memset", "_memset_bulk", "_memset_inner",
    "strcpy", "strncpy", "strncmp", "strnicmp", "strlen",
    "tolower", "toupper",
    "strtod", "sin", "cos",
    "time", "asctime", "mktime",
    "exit", "getenv",
    "outp",
    "delay",
    "__filbuf", "__get_doserrno_ptr", "__get_errno_ptr",
    "__exit", "_fpreset", "__EINVAL", "__set_EDOM", "__setEFGfmt",
    "_Not_Enough_Memory",
}

# program_info filename → group label. fd2_* fn referenced in these .md
# files is assigned to the corresponding group. Anything not referenced
# defaults to G9 glue.
SYSTEM_GROUP_MAP = {
    "battle.md":      ("G5", "engine"),
    "input.md":       ("G5", "engine"),
    "animation.md":   ("G5", "engine"),
    "graphics.md":    ("G5", "engine"),
    "audio.md":       ("G5", "engine"),
    "resource.md":    ("G6", "data"),
    "save_load.md":   ("G6", "data"),
    "field_map.md":   ("G6", "data"),
    "table_accessor.md": ("G6", "data"),
    "ui_menu.md":     ("G7", "ui"),
    "text_dialog.md": ("G7", "ui"),
    "chapter_event_dispatch.md": ("G8", "event"),
}

GROUP_LABELS = {
    "G1": "binary_artifact",
    "G2": "crt_lookup",
    "G3": "watcom_rtl",
    "G4": "ail",
    "G5": "engine",
    "G6": "data",
    "G7": "ui",
    "G8": "event",
    "G9": "glue",
}


def _unwrap(text: str):
    blob = json.loads(text)
    if isinstance(blob, dict) and "result" in blob and isinstance(blob["result"], str):
        return json.loads(blob["result"])
    return blob


def find_latest_dump() -> Path:
    WORKSPACE.mkdir(parents=True, exist_ok=True)
    dumps = sorted(WORKSPACE.glob("ghidra_dump_*.json"))
    if not dumps:
        raise SystemExit("ERROR: no ghidra_dump_*.json in workspace/function_audit/")
    return dumps[-1]


def normalise_addr(s: str) -> str:
    """Strip 0x prefix, lowercase, 8-hex padded."""
    s = s.strip().lower()
    if s.startswith("0x"):
        s = s[2:]
    return s.zfill(8)


def load_crt_lookup() -> tuple[set[str], dict[str, str]]:
    """Return (locked_addrs, addr→lookup_name) from crt_lookup_9.5a.json."""
    blob = json.loads(CRT_LOOKUP.read_text(encoding="utf-8"))
    by_address = blob.get("by_address", {})
    locked = {normalise_addr(a) for a in by_address.keys()}
    addr_to_name = {normalise_addr(a): e["name"] for a, e in by_address.items()}
    return locked, addr_to_name


def parse_program_info() -> dict[str, tuple[str, str]]:
    """Scan program_info/**/*.md for `@ 0x<addr>` patterns and return
    addr (8-hex lowercase) → (group, system).

    Address-based (not name-based) because program_info/ uses historical
    game-domain identifiers like `enemy_turn_action_dispatcher` while
    Ghidra now stores `fd2_*`-prefixed names. The address is stable;
    the name has been refined across phases.

    Sources:
    - program_info/*.md root-level system files (per SYSTEM_GROUP_MAP)
    - chapters/*.md → all G8 event
    """
    mapping: dict[str, tuple[str, str]] = {}
    # `@ 0xADDR` style reference; capture the addr. Allow leading spaces /
    # backtick wrap. Trailing must not be alnum (so 0xADDR_foo isn't matched).
    pat_addr = re.compile(r"@\s*`?\s*0x([0-9a-fA-F]+)\b")

    for md in PROGRAM_INFO.rglob("*.md"):
        rel = md.name
        if md.parent.name == "chapters":
            group, system = "G8", "event"
        elif rel in SYSTEM_GROUP_MAP:
            group, system = SYSTEM_GROUP_MAP[rel]
        else:
            continue
        text = md.read_text(encoding="utf-8", errors="ignore")
        for m in pat_addr.finditer(text):
            addr = m.group(1).lower().zfill(8)
            # First .md wins (system-specific .md before chapter files
            # because rglob order isn't deterministic; use SYSTEM_GROUP_MAP
            # as priority, chapters as fallback)
            if addr not in mapping:
                mapping[addr] = (group, system)
            elif mapping[addr][0] == "G8" and group != "G8":
                # Replace G8-chapter assignment if a system .md also lists it
                mapping[addr] = (group, system)
    return mapping


def assign_group(addr: str, name: str,
                 locked_addrs: set[str],
                 sys_by_addr: dict[str, tuple[str, str]]) -> tuple[str, str, bool]:
    """Return (group, label, locked). Group precedence:

    1. AIL_*  → G4 (Miles SDK, locked)
    2. binary_artifact_* → G1 (NOP padding, free)
    3. addr in crt_lookup → G2 (locked, lookup `name` canonical)
    4. fd2_* (or non-prefixed game-domain) at addr in program_info/*.md → G5..G8
    5. Watcom RTL prefixes (crt_*, __*, _*, L$*, PUBLIC_CRT_SYMBOLS,
       IF@*, single-name math/io like sin/printf) → G3
    6. fd2_* not in program_info/ → G9 glue
    7. fallback → G9 glue
    """
    if name.startswith("AIL_"):
        return ("G4", "ail", True)
    if name.startswith("binary_artifact_"):
        return ("G1", "binary_artifact", False)
    if addr in locked_addrs:
        return ("G2", "crt_lookup", True)
    if name.startswith("L$"):
        return ("G2", "crt_lookup", True)
    # fd2 systems by address (program_info @ 0xADDR mapping)
    if addr in sys_by_addr:
        group, system = sys_by_addr[addr]
        return (group, system, False)
    # Watcom RTL non-lookup
    if (name.startswith("crt_")
            or name.startswith("__")
            or name.startswith("_")
            or name.startswith("IF@")
            or name in PUBLIC_CRT_SYMBOLS):
        return ("G3", "watcom_rtl", name in PUBLIC_CRT_SYMBOLS)
    if name.startswith("fd2_"):
        return ("G9", "glue", False)
    return ("G9", "glue", False)


def load_existing_worklist() -> dict[str, dict]:
    """Return addr → row dict from previous worklist if it exists."""
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
    fns = payload["functions"]
    print(f"  {len(fns)} functions in Ghidra dump")

    locked_addrs, addr_to_lookup_name = load_crt_lookup()
    print(f"  {len(locked_addrs)} addresses locked by crt_lookup_9.5a.json")

    sys_by_addr = parse_program_info()
    print(f"  {len(sys_by_addr)} addresses → system mapped from program_info/")

    existing = load_existing_worklist()

    rows = []
    new_count = 0
    diff_warnings = []

    seen_addrs: set[str] = set()
    for fn in fns:
        addr = normalise_addr(fn["address"])
        name = fn["name"]
        seen_addrs.add(addr)

        group, label, locked = assign_group(addr, name, locked_addrs, sys_by_addr)
        lookup_name = addr_to_lookup_name.get(addr)
        lookup_mismatch = (lookup_name is not None and lookup_name != name)

        if addr in existing:
            prev = existing[addr]
            row = {
                "addr": addr,
                "current_name": name,
                "group": group,
                "group_label": label,
                "locked": locked,
                "lookup_name": lookup_name,
                "lookup_mismatch": lookup_mismatch,
                "is_thunk": fn.get("isThunk", False),
                "status": prev.get("status", "pending"),
                "problem_classes": prev.get("problem_classes", []),
                "verdict": prev.get("verdict"),
                "notes": prev.get("notes", ""),
            }
            # Detect rename since last worklist
            if prev.get("current_name") != name:
                row["notes"] = (
                    f"renamed from {prev.get('current_name')} "
                    f"(prev status={prev.get('status')}); {row['notes']}"
                ).strip("; ")
        else:
            new_count += 1
            diff_warnings.append(f"NEW in Ghidra (not in prev worklist): {addr} {name}")
            row = {
                "addr": addr,
                "current_name": name,
                "group": group,
                "group_label": label,
                "locked": locked,
                "lookup_name": lookup_name,
                "lookup_mismatch": lookup_mismatch,
                "is_thunk": fn.get("isThunk", False),
                "status": "pending",
                "problem_classes": [],
                "verdict": None,
                "notes": "",
            }
        rows.append(row)

    # Detect worklist-only entries (Ghidra-deleted)
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
            row["notes"] = (f"vanished from Ghidra dump; was {prev.get('current_name')}"
                            f" prev status={prev.get('status')}")
            rows.append(row)

    # Group counts
    group_counts: dict[str, int] = {}
    for row in rows:
        group_counts[row["group"]] = group_counts.get(row["group"], 0) + 1

    blob = {
        "ghidra_dump": dump_path.name,
        "function_count_in_ghidra": len(fns),
        "row_count": len(rows),
        "group_counts": group_counts,
        "rows": rows,
    }
    WORKLIST.write_text(json.dumps(blob, indent=2, ensure_ascii=False), encoding="utf-8")
    print(f"\nworklist written: {WORKLIST.relative_to(REPO)}")
    print(f"  rows:       {len(rows)}")
    print(f"  new:        {new_count}")
    print(f"  deleted:    {deleted_count}")
    print(f"  group split (Ghidra-live only):")
    live_counts: dict[str, int] = {}
    for row in rows:
        if row["status"] != "deleted_in_ghidra":
            live_counts[row["group"]] = live_counts.get(row["group"], 0) + 1
    for g in sorted(live_counts):
        label = GROUP_LABELS.get(g, "?")
        print(f"    {g} {label:14s}  {live_counts[g]:4d}")

    # Lookup mismatch summary
    mismatches = [r for r in rows if r.get("lookup_mismatch")
                  and r["status"] != "deleted_in_ghidra"]
    if mismatches:
        print(f"\n  crt_lookup name mismatches (Ghidra current_name != lookup name):"
              f" {len(mismatches)}")
        for r in mismatches[:10]:
            print(f"    {r['addr']}  Ghidra={r['current_name']:30s}  "
                  f"lookup={r['lookup_name']}")
        if len(mismatches) > 10:
            print(f"    ... ({len(mismatches) - 10} more)")

    if diff_warnings:
        print(f"\nWARNINGS ({len(diff_warnings)}):")
        for w in diff_warnings[:20]:
            print(f"  {w}")
        if len(diff_warnings) > 20:
            print(f"  ... ({len(diff_warnings) - 20} more)")
        print("\nReview the diff above. If acceptable, re-run; "
              "build_worklist exits with code 0 always (warnings are "
              "informational, the worklist is the source of truth).")

    return 0


if __name__ == "__main__":
    sys.exit(main())
