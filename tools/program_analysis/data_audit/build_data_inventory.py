"""Generate the per-segment data 主索引 markdown.

Reads:
- Latest `workspace/data_audit/ghidra_data_dump_*.json` (Ghidra MCP dump)
- `workspace/data_audit/worklist.json` (verdicts + caller_pool + subsystem)

Output:
- `workspace/data_audit/data_inventory.md`

Per-segment complete data table with each `data_*` label's addr / size /
type / emit_action / pool / subsystem / brief. Regenerated on demand;
not published to KB.
"""

import json
import re
import sys
from pathlib import Path

REPO = Path(__file__).resolve().parents[3]
WORKSPACE = REPO / "workspace" / "data_audit"
WORKLIST = WORKSPACE / "worklist.json"
OUT = WORKSPACE / "data_inventory.md"


def parse_list_data_items(text):
    rows = []
    pat = re.compile(r"^(\S+)\s+@\s+([0-9a-fA-F]+)\s+\[(.+?)\]\s+\((\d+)\s+bytes?\)\s*$")
    for line in text.split("\n"):
        line = line.strip()
        if not line:
            continue
        m = pat.match(line)
        if not m:
            continue
        rows.append({
            "name": m.group(1),
            "addr": m.group(2).lower().zfill(8),
            "type": m.group(3),
            "size": int(m.group(4)),
        })
    return rows


def segment_of(addr_int):
    if addr_int < 0x10000:
        return ".image"
    if 0x10000 <= addr_int < 0x4FFFF:
        return ".object1"
    if 0x50000 <= addr_int < 0x55FFF:
        return ".object2"
    if 0x60000 <= addr_int < 0x65FFF:
        return ".object3"
    return ".other"


def derive_emit_action(name, data_kind, caller_pool):
    n = name.lower()
    if "data_le_" in n or n.startswith("img_"):
        return "link_le_structure"
    if "data_align_" in n or "align_nop" in n or "stray" in n:
        return "skip_artifact"
    if "data_orphan_" in n or "unreachable" in n:
        return "skip_unreachable_data"
    inline_kinds = {
        "inline_jump_table", "absorbed_array_slot",
        "x87_long_double_constant", "x87_log_exp_reduction_constant",
        "inline_code_data", "misclassified_inline_code",
        "stray_byte_within_x87_long_double", "align_nop_intra_function",
        "align_null_inter_function", "watcom_math_function_double_wrapper",
        "inline_code_alt_entry", "word_lookup_table",
        "bit_mask_lookup_table", "numeric_constant_table",
    }
    if data_kind in inline_kinds and caller_pool in ("crt", "ail"):
        return "byte_preserve_within_vendor_function"
    if caller_pool == "ail" or "data_ail_" in n:
        return "link_vendor_lib"
    if caller_pool == "crt" or "data_crt_" in n:
        return "link_vendor_lib"
    if "data_fd2_" in n:
        if "blob" in n or "payload" in n or "image" in n or "_b24" in n:
            return "emit_fd2_image_blob"
        return "emit_c_const"
    if "data_string_" in n:
        if "ail" in n or "crt" in n:
            return "link_vendor_lib"
        return "emit_c_const"
    return "unknown"


def _latest_data_dump() -> Path:
    dumps = sorted(WORKSPACE.glob("ghidra_data_dump_*.json"))
    if not dumps:
        raise SystemExit("ERROR: no ghidra_data_dump_*.json in workspace/data_audit/")
    return dumps[-1]


def _unwrap(text: str):
    """Tolerate three formats:

    - `{"items": [...]}` — normalised dump (returned as-is)
    - `{"result": "...json string..."}` — MCP envelope around JSON
    - `{"result": "...plain text..."}` — MCP envelope around plain
      `<name> @ <addr> [<type>] (<size> bytes)` lines

    Returns either a dict (`{"items":...}`) or the inner string for the
    caller to parse via `parse_list_data_items`.
    """
    blob = json.loads(text)
    if isinstance(blob, dict) and "result" in blob and isinstance(blob["result"], str):
        inner = blob["result"]
        try:
            return json.loads(inner)
        except json.JSONDecodeError:
            return inner
    return blob


def main():
    dump_path = _latest_data_dump()
    print("Reading " + dump_path.name + " ...")
    payload = _unwrap(dump_path.read_text(encoding="utf-8"))
    # Either {"items": [...]} or {"result": "..."} formats handled.
    if isinstance(payload, dict) and "items" in payload:
        items = [
            {
                "name": it["name"],
                "addr": it["addr"].lower().zfill(8),
                "type": it["type"],
                "size": it["size"],
            }
            for it in payload["items"]
        ]
    else:
        # Fall back to plain-text MCP result wrapped in {"result": "..."}.
        items = parse_list_data_items(payload if isinstance(payload, str)
                                      else payload.get("result", ""))
    print("  parsed " + str(len(items)) + " data items from Ghidra dump")

    print("Reading " + str(WORKLIST.relative_to(REPO)) + " ...")
    wl = json.loads(WORKLIST.read_text(encoding="utf-8"))
    wl_by_addr = {r["addr"]: r for r in wl["rows"]}
    print("  loaded " + str(len(wl_by_addr)) + " worklist rows")

    by_seg = {".image": [], ".object1": [], ".object2": [], ".object3": [], ".other": []}
    for it in items:
        addr_int = int(it["addr"], 16)
        seg = segment_of(addr_int)
        wlr = wl_by_addr.get(it["addr"])
        if wlr and wlr.get("verdict"):
            v = wlr["verdict"]
            it["caller_pool"] = wlr.get("caller_pool") or "unknown"
            it["subsystem"] = wlr.get("subsystem") or ""
            it["data_kind"] = v.get("data_kind") or ""
        else:
            it["caller_pool"] = "unknown"
            it["subsystem"] = ""
            it["data_kind"] = ""
        it["emit_action"] = derive_emit_action(
            it["name"], it["data_kind"], it["caller_pool"]
        )
        by_seg[seg].append(it)

    for seg in by_seg:
        by_seg[seg].sort(key=lambda r: int(r["addr"], 16))

    lines = []
    lines.append("# FD2.LE Data Inventory — per-segment 主索引")
    lines.append("")
    lines.append("Generated from `tools/program_analysis/data_audit/build_data_inventory.py`. 讀 Ghidra")
    lines.append("`list_data_items` live dump，join `workspace/data_audit/worklist.json`")
    lines.append("verdict metadata (caller_pool / subsystem / data_kind / emit_action)。")
    lines.append("")
    lines.append("Total: **" + str(len(items)) + " defined data items** across **4 segments**.")
    lines.append("")
    lines.append("Emit pipeline 用此表決定每個 data item 的 routing：")
    lines.append("")
    lines.append("| emit_action | 處理 |")
    lines.append("|---|---|")
    lines.append("| `link_vendor_lib` | vendor (AIL / CRT) data bytes 從相應 .obj 連結，不重新 emit C source |")
    lines.append("| `emit_c_const` | fd2 own data，重新 emit 為 C const declaration |")
    lines.append("| `emit_c_bss` | fd2 runtime state，emit 為 BSS / 全域變數 |")
    lines.append("| `emit_fd2_image_blob` | 大型 raw resource blob (cutscene script / shimmer offset 等) |")
    lines.append("| `byte_preserve_within_vendor_function` | vendor function body 內 inline 資料 (跳轉表 / 常數 / padding) |")
    lines.append("| `skip_artifact` | alignment / padding / stray label，emit 階段交給 wlink |")
    lines.append("| `skip_unreachable_data` | 無 caller / 無 LE FIXUP 反查命中的真孤兒 |")
    lines.append("| `link_le_structure` | LE header / loader / fixup / image，直接從 LE 檔複製 |")
    lines.append("")

    seg_descs = {
        ".image": ".image — LE file structure (header / loader / fixup / image)",
        ".object1": ".object1 — code segment (主要為 function bytes，少量 inline data：vendor jump tables / mantissa masks / x87 LD constants)",
        ".object2": ".object2 — DGROUP (CRT/AIL globals + fd2 runtime state + BSS)",
        ".object3": ".object3 — fd2 game data tables + chapter cutscene scripts + image blobs",
        ".other": ".other — outside known segments (應為 0)",
    }

    lines.append("## Segment 統計")
    lines.append("")
    lines.append("| Segment | items | description |")
    lines.append("|---|---:|---|")
    for seg in [".image", ".object1", ".object2", ".object3", ".other"]:
        cnt = len(by_seg[seg])
        if cnt == 0:
            continue
        desc = seg_descs[seg].split(" — ", 1)[1]
        lines.append("| `" + seg + "` | " + str(cnt) + " | " + desc + " |")
    lines.append("")

    # emit_action distribution
    from collections import Counter
    ea_counter = Counter(it["emit_action"] for it in items)
    lines.append("## emit_action 分布")
    lines.append("")
    lines.append("| emit_action | items |")
    lines.append("|---|---:|")
    for ea, cnt in ea_counter.most_common():
        lines.append("| `" + ea + "` | " + str(cnt) + " |")
    lines.append("")

    for seg in [".image", ".object1", ".object2", ".object3"]:
        rows = by_seg[seg]
        if not rows:
            continue
        lines.append("## " + seg_descs[seg])
        lines.append("")
        lines.append("**Items: " + str(len(rows)) + "**")
        lines.append("")
        lines.append("| addr | name | type | size | pool | subsystem | emit_action | data_kind |")
        lines.append("|---|---|---|---:|---|---|---|---|")
        for r in rows:
            name = r["name"]
            if len(name) > 60:
                name = name[:60] + "…"
            typ = r["type"]
            if len(typ) > 30:
                typ = typ[:30] + "…"
            subsys = r["subsystem"][:20]
            dk = r["data_kind"][:25]
            lines.append(
                "| `" + r["addr"] + "` | `" + name + "` | `" + typ + "` | "
                + str(r["size"]) + " | " + r["caller_pool"] + " | " + subsys
                + " | " + r["emit_action"] + " | " + dk + " |"
            )
        lines.append("")

    OUT.parent.mkdir(parents=True, exist_ok=True)
    OUT.write_text("\n".join(lines), encoding="utf-8")
    print("")
    print("written: " + str(OUT.relative_to(REPO)))
    print("  total items: " + str(len(items)))
    for seg in [".image", ".object1", ".object2", ".object3", ".other"]:
        if by_seg[seg]:
            print("  " + seg + ": " + str(len(by_seg[seg])))


if __name__ == "__main__":
    main()
