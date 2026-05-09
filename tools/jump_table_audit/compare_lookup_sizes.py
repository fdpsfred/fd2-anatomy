"""compare_lookup_sizes.py — Phase 2 of issue #30 audit.

對 `rebuild_info/crt_lookup_9.5a.json` 全 140 個 entry 跑「lookup body_size 與 lib
.obj 對應 function size」的全面 diff。size 不一致的 entry 是潛在的 jump-table
漏抓 — Ghidra 把 lib function 一部分切出 body 外，造成 lookup 紀錄的 body_size
比 lib 真實 size 小。

Output: workspace/jump_table_audit/lookup_size_diff.json + .md
"""
from __future__ import annotations

import json
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
LOOKUP_PATH = ROOT / "rebuild_info" / "crt_lookup_9.5a.json"
OBJ_FUNCS_PATH = ROOT / "workspace" / "crt_callee_match" / "obj_funcs_clib3s.json"
OUT_DIR = ROOT / "workspace" / "jump_table_audit"
OUT_DIR.mkdir(exist_ok=True, parents=True)


def main() -> None:
    lookup = json.loads(LOOKUP_PATH.read_text(encoding="utf-8"))
    obj_funcs = json.loads(OBJ_FUNCS_PATH.read_text(encoding="utf-8"))

    by_addr: dict = lookup["by_address"]
    diffs = []
    matched = 0
    missing_obj = 0
    missing_func = 0
    for addr, entry in by_addr.items():
        name = entry.get("name")
        body_size = entry.get("body_size")
        source_obj = entry.get("source_obj")
        if not source_obj:
            missing_obj += 1
            continue
        funcs = obj_funcs.get(source_obj)
        if funcs is None:
            missing_obj += 1
            diffs.append({
                "addr": addr, "name": name, "body_size": body_size,
                "source_obj": source_obj,
                "issue": "source_obj_not_in_index",
            })
            continue
        # find by name; if duplicate names exist take first
        candidates = [f for f in funcs if f.get("name") == name]
        if not candidates:
            # try with leading underscore variants
            candidates = [f for f in funcs if f.get("name") == "_" + (name or "")]
        if not candidates:
            missing_func += 1
            diffs.append({
                "addr": addr, "name": name, "body_size": body_size,
                "source_obj": source_obj,
                "issue": "name_not_in_obj",
                "obj_funcs": [f.get("name") for f in funcs],
            })
            continue
        lib_size = candidates[0].get("size")
        if lib_size != body_size:
            diffs.append({
                "addr": addr, "name": name,
                "lookup_body_size": body_size, "lib_size": lib_size,
                "delta": (lib_size or 0) - (body_size or 0),
                "source_obj": source_obj,
                "issue": "size_mismatch",
            })
        else:
            matched += 1

    summary = {
        "total_entries": len(by_addr),
        "size_match": matched,
        "size_mismatch": sum(1 for d in diffs if d["issue"] == "size_mismatch"),
        "name_not_in_obj": missing_func,
        "source_obj_missing": missing_obj,
        "diffs": diffs,
    }
    (OUT_DIR / "lookup_size_diff.json").write_text(
        json.dumps(summary, indent=2), encoding="utf-8")

    # Markdown report
    lines = []
    lines.append("# CRT Lookup body_size vs lib .obj size — Phase 2 audit\n")
    lines.append(f"- Total entries: {summary['total_entries']}")
    lines.append(f"- size match: {summary['size_match']}")
    lines.append(f"- size mismatch: {summary['size_mismatch']}")
    lines.append(f"- name not found in obj: {summary['name_not_in_obj']}")
    lines.append(f"- source_obj missing: {summary['source_obj_missing']}")
    lines.append("")
    if summary["size_mismatch"]:
        lines.append("## Size mismatches\n")
        lines.append("| addr | name | lookup body_size | lib size | delta | source_obj |")
        lines.append("|---|---|---:|---:|---:|---|")
        for d in diffs:
            if d["issue"] != "size_mismatch":
                continue
            lines.append(f"| `{d['addr']}` | `{d['name']}` | {d['lookup_body_size']} | "
                         f"{d['lib_size']} | {d['delta']:+d} | {d['source_obj']} |")
        lines.append("")
    if summary["name_not_in_obj"]:
        lines.append("## Name not in obj (lookup name doesn't match .obj PUBDEF)\n")
        for d in diffs:
            if d["issue"] != "name_not_in_obj":
                continue
            lines.append(f"- `{d['addr']}` `{d['name']}` (.obj has: {d['obj_funcs']})")
    if summary["source_obj_missing"]:
        lines.append("\n## source_obj missing or null\n")
        for d in diffs:
            if d["issue"] not in ("source_obj_not_in_index",):
                continue
            lines.append(f"- `{d['addr']}` `{d['name']}` source={d.get('source_obj')}")

    (OUT_DIR / "lookup_size_diff.md").write_text("\n".join(lines), encoding="utf-8")
    print(f"summary: match={summary['size_match']} mismatch={summary['size_mismatch']} "
          f"name_not_in_obj={summary['name_not_in_obj']} obj_missing={summary['source_obj_missing']}")


if __name__ == "__main__":
    main()
