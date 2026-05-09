"""Verify each AIL function's xref sources are AIL-side, not CRT/game.

Reads:
  workspace/ail_audit/ail_xref_source_audit.json   (Ghidra-side dump:
    addr -> {name, xrefs_to: [{from, type, container, container_addr, ...}]})

Writes:
  workspace/ail_audit/xref_source_audit_report.json
  workspace/ail_audit/xref_source_audit_report.md

Flag rule:
  For each AIL function (name starts with AIL_):
    examine each xref source's containing function:
      AIL    — name starts with AIL_ → OK
      align_nop_* — alignment fall-through, ignored
      data section / external — container == null, flagged as "raw_data_xref"
      CRT/game — container starts with crt_, or other prefix → flagged
"""
from __future__ import annotations

import json
from pathlib import Path

REPO = Path(__file__).resolve().parents[2]
WORK = REPO / "workspace" / "ail_audit"
IN_JSON = WORK / "ail_xref_source_audit.json"
OUT_JSON = WORK / "xref_source_audit_report.json"
OUT_MD = WORK / "xref_source_audit_report.md"


def is_ail(name: str | None) -> bool:
    return bool(name) and name.startswith("AIL_")


def is_align_nop(name: str | None) -> bool:
    return bool(name) and name.startswith("align_nop_")


def classify_container(name: str | None) -> str:
    if name is None:
        return "raw_data"           # no enclosing function (data section / table)
    if is_align_nop(name):
        return "align_nop"          # alignment fill, not a real reference
    if is_ail(name):
        return "ail"
    if name.startswith("crt_"):
        return "crt_prefix"
    # any other named function — could be CRT public (sprintf, fread, etc.) or game
    return "non_ail_named"


def main():
    data = json.loads(IN_JSON.read_text(encoding="utf-8"))
    by_addr = data["by_addr"]

    flagged = []           # AIL functions with at least one suspicious xref
    summary_counts = {
        "ail_total": len(by_addr),
        "fully_clean": 0,
        "has_raw_data_only": 0,
        "has_align_nop_only": 0,
        "has_non_ail_named": 0,    # bad: container is named CRT/game function
        "has_crt_prefix": 0,
    }

    for addr, entry in sorted(by_addr.items()):
        name = entry["name"]
        xrefs = entry.get("xrefs_to", [])

        # Bucket xref containers
        by_class = {"ail": [], "align_nop": [], "raw_data": [],
                    "crt_prefix": [], "non_ail_named": []}
        for x in xrefs:
            cont = x.get("container")
            cls = classify_container(cont)
            x_record = {
                "from": x["from"],
                "type": x["type"],
                "container": cont,
                "container_addr": x.get("container_addr"),
            }
            by_class[cls].append(x_record)

        # Flag if any container is non_ail_named or crt_prefix
        is_flagged = bool(by_class["non_ail_named"]) or bool(by_class["crt_prefix"])
        is_raw = bool(by_class["raw_data"]) and not by_class["ail"] and not is_flagged

        if is_flagged:
            flagged.append({
                "addr": addr,
                "name": name,
                "xref_buckets": by_class,
                "total_xrefs": len(xrefs),
            })
            if by_class["non_ail_named"]:
                summary_counts["has_non_ail_named"] += 1
            if by_class["crt_prefix"]:
                summary_counts["has_crt_prefix"] += 1
        elif is_raw:
            summary_counts["has_raw_data_only"] += 1
        elif by_class["align_nop"] and not by_class["ail"] and not by_class["raw_data"]:
            summary_counts["has_align_nop_only"] += 1
        elif not xrefs:
            # zero xref — orphan dead-code, OK
            summary_counts["fully_clean"] += 1
        else:
            # has at least one ail xref, no flagged sources → clean
            summary_counts["fully_clean"] += 1

    OUT_JSON.write_text(json.dumps({
        "_meta": summary_counts,
        "flagged": flagged,
    }, indent=2, ensure_ascii=False), encoding="utf-8")

    md = ["# AIL function xref-source 復查 report\n"]
    md.append("檢查每個 AIL function 所有 xref source（DATA + CALL）的 containing function；")
    md.append("flag 任何 source container 是非 AIL（即不以 `AIL_` 開頭，且非 `align_nop_*`）的 entry。\n")
    md.append("## 統計\n")
    for k, v in summary_counts.items():
        md.append(f"- `{k}`: {v}")
    md.append("")

    if flagged:
        md.append(f"\n## flagged AIL functions（{len(flagged)} 個）— 需個別 review\n")
        md.append("| addr | name | flagged xrefs |")
        md.append("|---|---|---|")
        for f in flagged:
            parts = []
            for cls in ("crt_prefix", "non_ail_named"):
                for x in f["xref_buckets"][cls]:
                    parts.append(f"{x['from']} in `{x['container']}`")
            md.append(f"| `0x{f['addr']}` | `{f['name']}` | "
                      f"{'; '.join(parts) if parts else '(none)'} |")
    else:
        md.append("\n## flagged AIL functions: 0\n\n所有 AIL function 的 xref source 全部來自 AIL 命名空間（或 align_nop_*/raw_data），無誤判。")

    OUT_MD.write_text("\n".join(md) + "\n", encoding="utf-8")

    print(f"AIL total:           {summary_counts['ail_total']}")
    print(f"fully_clean:         {summary_counts['fully_clean']}")
    print(f"has_raw_data_only:   {summary_counts['has_raw_data_only']}")
    print(f"has_align_nop_only:  {summary_counts['has_align_nop_only']}")
    print(f"has_crt_prefix:      {summary_counts['has_crt_prefix']}")
    print(f"has_non_ail_named:   {summary_counts['has_non_ail_named']}")
    print(f"flagged total:       {len(flagged)}")
    print(f"\nwrote:\n  {OUT_JSON.relative_to(REPO)}\n  {OUT_MD.relative_to(REPO)}")


if __name__ == "__main__":
    main()
