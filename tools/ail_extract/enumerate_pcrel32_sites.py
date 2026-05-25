"""Build PC-relative rel32 cross-fn worklist for A3b per-item fixup classification.

Scans all x86-32 PC-relative rel32 instructions inside AIL function bodies
that need cross-.obj FIXUPP32 when target is outside the parent fn:

  - E8 disp32   CALL near       (5-byte, disp32 at offset 1)
  - E9 disp32   JMP near        (5-byte, disp32 at offset 1)
  - 0F 8x disp32  Jcc near × 16  (6-byte, disp32 at offset 2)

Other PC-relative jump opcodes are not scanned because of empirical FD2.LE
verification (see commit history / sanity-check log):

  - EB rel8 (JMP short)          0/450 cross-fn — all intra
  - 70-7F rel8 (Jcc short × 16)  0/1796 cross-fn — all intra
  - E0-E3 rel8 (LOOPx / JECXZ)   0/2 cross-fn — all intra
  - EA ptr16:32 (JMP far)        0 instances in AIL bodies
  - 9A ptr16:32 (CALL far)       0 instances in AIL bodies

Indirect jumps (FF /2 CALL r/m32, FF /4 JMP r/m32, etc.) take their target
from a register or memory operand and need NO instruction-site FIXUPP. Their
cross-.obj resolution happens at the *data* — function pointers and dispatch
tables — handled by `tools/program_analysis/data_audit/parse_le_fixup.py`.

Per [[feedback_ail_fixup_synth_per_item]]: this tool ONLY enumerates; no
target classification, no Python-derived rule-based dispatch. Output is a
worklist for per-item per-message processing.

Per [[feedback_no_kb_hardcoded_values]]: all addresses come from Ghidra raw
dump (`workspace/ail_extract/raw/pcrel32_sites_raw.json`) + ail_inventory.json.
No KB hardcoded address ranges.

Mechanical pre-filter (pure address-range math):
  - intra_fn: target_addr ∈ parent_fn body_ranges → no FIXUPP needed,
              SEGDEF-internal relative stays valid regardless of obj layout
  - cross_fn: target_addr ∉ parent_fn body → needs A3b classification
              (AIL→AIL / AIL→CRT / AIL→fd2common / unknown)

Inputs:
  - workspace/ail_extract/raw/pcrel32_sites_raw.json (Ghidra dump)
  - workspace/ail_extract/ail_inventory.json (from dump_ail_set.py)

Outputs:
  - workspace/ail_extract/pcrel32_worklist.jsonl (cross-fn sites, one per line,
    ordered by inst_addr ascending; site_idx assigned 1..N)
  - workspace/ail_extract/pcrel32_summary.json (stats + samples)
"""

from __future__ import annotations

import json
from pathlib import Path

REPO = Path(__file__).resolve().parents[2]
WS = REPO / "workspace" / "ail_extract"
SITES_RAW = WS / "raw" / "pcrel32_sites_raw.json"
INVENTORY = WS / "ail_inventory.json"
WORKLIST_OUT = WS / "pcrel32_worklist.jsonl"
SUMMARY_OUT = WS / "pcrel32_summary.json"


def hex_int(s: str) -> int:
    return int(s, 16)


def main():
    sites = json.loads(SITES_RAW.read_text(encoding="utf-8"))
    inv = json.loads(INVENTORY.read_text(encoding="utf-8"))

    fn_ranges = {}
    for fn in inv["functions"]:
        fn_ranges[fn["entry"]] = [
            (hex_int(r[0]), hex_int(r[1])) for r in fn["body_ranges"]
        ]
    ail_fn_entries = {hex_int(fn["entry"]) for fn in inv["functions"]}

    intra_fn = []
    cross_fn = []
    no_parent_info = []

    for site in sites:
        target = hex_int(site["target_addr"])
        parent_entry = site["parent_fn_entry"]
        ranges = fn_ranges.get(parent_entry)
        if ranges is None:
            no_parent_info.append(site)
            continue
        in_parent = any(start <= target <= end for (start, end) in ranges)
        if in_parent:
            intra_fn.append(site)
        else:
            site["_diag_target_is_ail_fn_entry"] = target in ail_fn_entries
            cross_fn.append(site)

    # Sort cross_fn by inst_addr ascending (deterministic ordering)
    cross_fn.sort(key=lambda s: hex_int(s["inst_addr"]))

    # Assign site_idx 1..N
    with WORKLIST_OUT.open("w", encoding="utf-8") as f:
        for i, site in enumerate(cross_fn, 1):
            site["site_idx"] = i
            f.write(json.dumps(site, ensure_ascii=False) + "\n")

    # Stats
    opcode_dist = {}
    for site in sites:
        opcode_dist[site["opcode"]] = opcode_dist.get(site["opcode"], 0) + 1

    cross_opcode = {}
    for site in cross_fn:
        cross_opcode[site["opcode"]] = cross_opcode.get(site["opcode"], 0) + 1

    diag_hits_ail_fn = sum(1 for s in cross_fn if s.get("_diag_target_is_ail_fn_entry"))

    summary = {
        "total_sites": len(sites),
        "opcode_distribution": dict(sorted(opcode_dist.items())),
        "intra_fn_count": len(intra_fn),
        "cross_fn_count": len(cross_fn),
        "no_parent_info_count": len(no_parent_info),
        "cross_fn_opcode_distribution": dict(sorted(cross_opcode.items())),
        "diag_cross_fn_target_hits_ail_fn_entry": diag_hits_ail_fn,
        "samples_intra_fn": intra_fn[:3],
        "samples_cross_fn": cross_fn[:3],
        "samples_no_parent": no_parent_info[:3],
    }
    SUMMARY_OUT.write_text(json.dumps(summary, indent=2, ensure_ascii=False), encoding="utf-8")

    print(f"Wrote {WORKLIST_OUT}  ({len(cross_fn)} cross-fn sites)")
    print(f"Wrote {SUMMARY_OUT}")
    print(f"  total sites:          {len(sites)}")
    print(f"  intra-fn (filtered):  {len(intra_fn)}")
    print(f"  cross-fn (worklist):  {len(cross_fn)}")
    print(f"    of which target hits AIL fn entry: {diag_hits_ail_fn}")
    print(f"    of which target NOT AIL fn entry:  {len(cross_fn) - diag_hits_ail_fn}")
    print(f"  opcode dist (total):  {summary['opcode_distribution']}")
    print(f"  opcode dist (cross):  {summary['cross_fn_opcode_distribution']}")
    if no_parent_info:
        print(f"  WARN: {len(no_parent_info)} sites have no parent fn entry in inventory")


if __name__ == "__main__":
    main()
