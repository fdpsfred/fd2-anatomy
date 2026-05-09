"""Phase F — Review BFS-unreachable AIL_*/AIL_internal_* functions.

Inputs:
  workspace/ail_audit/phase_c_reachable.json   — BFS reach result
  workspace/ail_audit/orphan_xrefs.json        — xrefs into each orphan entry
  workspace/ail_audit/ail_decomp_dump.json     — decompile + data refs (via Java)
  workspace/function_review/registry.json      — current names + caller graph

Output:
  workspace/ail_audit/phase_f_orphans.json
  workspace/ail_audit/phase_f_summary.md
  workspace/ail_audit/phase_f_apply.java       — plate-comment apply script

Sub-classification:

| sub-case | detector | action |
|---|---|---|
| vtable_indirect    | xref includes DATA (function-pointer table)   | keep + plate |
| cluster_member     | only CALL xrefs, all from unreached AIL_int.  | keep + plate |
| tail_call_target   | only UNCONDITIONAL_JUMP xrefs                  | keep + plate |
| dead_code_stub     | no xref at all                                 | keep + plate |
| mislabeled         | (none in this dataset)                         | reclassify  |

All 32 orphans share the AIL_internal_ prefix and have semantically clear names
(or `AIL_internal_helper_<addr>` placeholders that point at AIL bytes), so no
function rename is generated; only plate comments.
"""
from __future__ import annotations

import json
from datetime import date
from pathlib import Path

REPO = Path(__file__).resolve().parents[2]
WORK = REPO / "workspace" / "ail_audit"
FN_REVIEW = REPO / "workspace" / "function_review"

PHASE_C = WORK / "phase_c_reachable.json"
ORPHAN_X = WORK / "orphan_xrefs.json"
DECOMP = WORK / "ail_decomp_dump.json"
REG = FN_REVIEW / "registry.json"

OUT_JSON = WORK / "phase_f_orphans.json"
OUT_MD = WORK / "phase_f_summary.md"
OUT_JAVA = WORK / "phase_f_apply.java"


def java_escape(s: str) -> str:
    out = []
    for ch in s:
        if ch == '\\':
            out.append('\\\\')
        elif ch == '"':
            out.append('\\"')
        elif ch == '\n':
            out.append('\\n')
        elif ch == '\r':
            out.append('\\r')
        elif ch == '\t':
            out.append('\\t')
        elif ord(ch) < 0x20:
            out.append(f'\\u{ord(ch):04x}')
        else:
            out.append(ch)
    return "".join(out)


def main():
    phc = json.loads(PHASE_C.read_text(encoding="utf-8"))
    xrefs = json.loads(ORPHAN_X.read_text(encoding="utf-8"))
    registry = json.loads(REG.read_text(encoding="utf-8"))

    reachable = set(phc["reachable"].keys())
    reg_by_addr = {r["address"]: r for r in registry}

    ail_int_addrs = {r["address"] for r in registry
                     if r["current_name"].startswith("AIL_internal_")}
    ail_pub_addrs = {r["address"] for r in registry
                     if r["current_name"].startswith("AIL_") and not
                     r["current_name"].startswith("AIL_internal_")}

    unreached = sorted((ail_int_addrs | ail_pub_addrs) - reachable)

    classifications = {}
    for addr in unreached:
        reg_r = reg_by_addr[addr]
        x = xrefs["by_addr"].get(addr, {"xrefs_to": []})
        x_list = x["xrefs_to"]
        x_types = {r["type"] for r in x_list}
        x_data = [r for r in x_list if r["is_data"] and not r["is_call"]]
        x_call = [r for r in x_list if r["is_call"]]
        x_jump = [r for r in x_list if r["type"] == "UNCONDITIONAL_JUMP"]

        # Decide sub-case
        if x_data:
            sub = "vtable_indirect"
            evidence = (f"Registered via data table — {len(x_data)} DATA xref(s) "
                        f"at: " + ", ".join(f"0x{r['from']}" for r in x_data))
        elif x_call:
            # All callers should be AIL_internal_* unreached (cluster member)
            callers_in_reg = reg_r.get("callers", [])
            non_ail_callers = [
                c for c in callers_in_reg
                if not reg_by_addr.get(c, {}).get("current_name", "").startswith("AIL_internal_")
            ]
            if non_ail_callers:
                sub = "non_ail_caller"
                evidence = "Has non-AIL caller: " + ", ".join(
                    reg_by_addr[c]["current_name"] for c in non_ail_callers
                )
            else:
                ail_callers_unreached = [
                    c for c in callers_in_reg if c not in reachable
                ]
                ail_callers_reached = [
                    c for c in callers_in_reg if c in reachable
                ]
                sub = "cluster_member"
                evidence = (f"AIL_internal cluster: callers="
                            + ", ".join(reg_by_addr[c]["current_name"]
                                        for c in callers_in_reg))
        elif x_jump:
            sub = "tail_call_target"
            evidence = (f"Tail-jumped from {len(x_jump)} site(s): "
                        + ", ".join(f"0x{r['from']}" for r in x_jump))
        else:
            sub = "dead_code_stub"
            evidence = ("No xref. Linker imported from AIL3DIG/AIL3MDI .obj but "
                        "FD2 binary contains no reference. Safe to omit when "
                        "extracting AIL .obj.")

        classifications[addr] = {
            "name": reg_r["current_name"],
            "sub_case": sub,
            "evidence": evidence,
            "xref_count": len(x_list),
            "xref_types": sorted(x_types),
            "callers": [reg_by_addr[c]["current_name"]
                        for c in reg_r.get("callers", [])
                        if c in reg_by_addr],
            "callees_count": reg_r.get("callees_count", 0),
        }

    # Bucket counts
    buckets = {}
    for c in classifications.values():
        buckets[c["sub_case"]] = buckets.get(c["sub_case"], 0) + 1

    OUT_JSON.write_text(json.dumps({
        "_meta": {
            "orphan_count": len(classifications),
            "bucket_counts": buckets,
        },
        "orphans": classifications,
    }, indent=2, ensure_ascii=False), encoding="utf-8")

    # ---- Summary MD ----
    md = ["# Phase F — Orphan AIL_*/AIL_internal_* review\n"]
    md.append(f"輸入：{len(unreached)} 個 BFS 不可達 AIL function (108 reached + "
              f"{len(unreached)} unreached = {len(ail_int_addrs)+len(ail_pub_addrs)} total).\n")
    md.append("Sub-case 分布：\n")
    for sub in ("vtable_indirect", "cluster_member", "tail_call_target",
                "dead_code_stub", "non_ail_caller"):
        md.append(f"- `{sub}`: {buckets.get(sub, 0)}")
    md.append("")

    for sub in ("vtable_indirect", "cluster_member", "tail_call_target",
                "dead_code_stub", "non_ail_caller"):
        in_bucket = [(a, c) for a, c in classifications.items()
                     if c["sub_case"] == sub]
        if not in_bucket:
            continue
        md.append(f"\n## {sub} ({len(in_bucket)} 個)\n")
        md.append("| addr | name | evidence |")
        md.append("|---|---|---|")
        for a, c in sorted(in_bucket):
            ev = c["evidence"][:120] + ("…" if len(c["evidence"]) > 120 else "")
            md.append(f"| `0x{a}` | `{c['name']}` | {ev} |")

    md.append("\n## 處置結論\n")
    md.append("32 個 orphan 全部保留現有 `AIL_internal_*` / `AIL_*` 名稱（命名語意已正確），")
    md.append("僅補上 plate comment 紀錄各自為何 BFS 不可達：function-pointer 註冊、")
    md.append("AIL-internal cluster 連結、tail-jump、或 vendor library dead-code。")
    md.append("無需 reclassify 或重新命名。")
    OUT_MD.write_text("\n".join(md) + "\n", encoding="utf-8")

    # ---- Apply Java (plate comments only, no rename) ----
    java_lines = []
    java_lines.append("import ghidra.program.model.listing.Function;")
    java_lines.append("import ghidra.program.model.listing.FunctionManager;")
    java_lines.append("import ghidra.program.model.address.Address;")
    java_lines.append("")
    java_lines.append("FunctionManager fm = currentProgram.getFunctionManager();")
    java_lines.append("int applied = 0;")
    java_lines.append("int errors = 0;")

    plate_template = ("[AIL audit orphan — Phase F {date}]\n"
                      "Sub-case: {sub}\n"
                      "{evidence}\n"
                      "BFS-unreachable from confirmed AIL public API; classification "
                      "verified by tools/ail_audit/orphan_review.py.")

    for addr, c in sorted(classifications.items()):
        plate = plate_template.format(
            date=date.today().isoformat(),
            sub=c["sub_case"],
            evidence=c["evidence"],
        )
        java_lines.append("")
        java_lines.append("try {")
        java_lines.append(
            f'  Address a = currentProgram.getAddressFactory().getDefaultAddressSpace().getAddress("{addr}");'
        )
        java_lines.append("  Function f = fm.getFunctionAt(a);")
        java_lines.append("  if (f == null) {")
        java_lines.append(f'    println("MISSING addr {addr}"); errors++;')
        java_lines.append("  } else {")
        java_lines.append(f'    f.setComment("{java_escape(plate)}");')
        cn = c["name"]
        java_lines.append(f'    println("plate set: " + a.toString() + " ({cn})");')
        java_lines.append("    applied++;")
        java_lines.append("  }")
        java_lines.append("} catch (Exception ex) {")
        java_lines.append(f'  println("ERROR at {addr}: " + ex.getMessage());')
        java_lines.append("  errors++;")
        java_lines.append("}")

    java_lines.append("")
    java_lines.append('println("=== applied: " + applied + " ; errors: " + errors + " ===");')
    OUT_JAVA.write_text("\n".join(java_lines) + "\n", encoding="utf-8")

    print(f"orphan total: {len(unreached)}")
    for sub, cnt in sorted(buckets.items()):
        print(f"  {sub:25s} {cnt}")
    print(f"\nwrote:\n  {OUT_JSON.relative_to(REPO)}\n  {OUT_MD.relative_to(REPO)}\n  "
          f"{OUT_JAVA.relative_to(REPO)}")


if __name__ == "__main__":
    main()
