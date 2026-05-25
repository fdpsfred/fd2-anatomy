"""Test enumerate_pcrel32_sites.py output.

Per [[feedback_script_review_then_test]].

Tests:
  T1. Output files exist + JSON loadable
  T2. Total = intra + cross + no_parent (no items lost)
  T3. Opcode distribution: only {E8, E9, 0F80..0F8F}, sum = total
  T4. Every worklist line is well-formed JSON with required fields including
      inst_size + disp32_offset_in_inst (needed for OMF FIXUPP emit)
  T5. Every intra-fn site: target_addr is genuinely within parent fn body
  T6. Every cross-fn site: target_addr is genuinely OUTSIDE parent fn body
  T7. For each site, target_addr = inst_addr + inst_size + disp32 (decode sanity)
  T8. no_parent_info count == 0 (all sites should have parent in 428 fn set)
  T9. inst_size / disp32_offset_in_inst consistency per opcode:
      - E8/E9: inst_size=5, dispOff=1
      - 0F 8x: inst_size=6, dispOff=2
  T10. site_idx 1..N contiguous in worklist + ordered by inst_addr ascending
"""

from __future__ import annotations

import json
import sys
from pathlib import Path

REPO = Path(__file__).resolve().parents[2]
WS = REPO / "workspace" / "ail_extract"
SITES_RAW = WS / "raw" / "pcrel32_sites_raw.json"
INVENTORY = WS / "ail_inventory.json"
WORKLIST = WS / "pcrel32_worklist.jsonl"
SUMMARY = WS / "pcrel32_summary.json"


def fail(test_id, msg):
    print(f"  FAIL [{test_id}] {msg}")
    sys.exit(1)


def ok(test_id, msg):
    print(f"  PASS [{test_id}] {msg}")


def hex_int(s):
    return int(s, 16)


def main():
    # T1
    for p in (SITES_RAW, INVENTORY, WORKLIST, SUMMARY):
        if not p.exists():
            fail("T1", f"{p} missing")
    sites = json.loads(SITES_RAW.read_text(encoding="utf-8"))
    inv = json.loads(INVENTORY.read_text(encoding="utf-8"))
    summary = json.loads(SUMMARY.read_text(encoding="utf-8"))
    worklist = [json.loads(ln) for ln in WORKLIST.read_text(encoding="utf-8").splitlines() if ln.strip()]
    ok("T1", "all output files exist + loadable")

    # T2
    expected_total = summary["intra_fn_count"] + summary["cross_fn_count"] + summary["no_parent_info_count"]
    if expected_total != summary["total_sites"]:
        fail("T2", f"intra+cross+no_parent ({expected_total}) != total ({summary['total_sites']})")
    if len(sites) != summary["total_sites"]:
        fail("T2", f"raw sites len ({len(sites)}) != summary total ({summary['total_sites']})")
    ok("T2", f"total {summary['total_sites']} = intra {summary['intra_fn_count']} + cross {summary['cross_fn_count']} + no_parent {summary['no_parent_info_count']}")

    # T3
    od = summary["opcode_distribution"]
    expected_opcodes = {"E8", "E9"} | {f"0F{n:02X}" for n in range(0x80, 0x90)}
    found_opcodes = set(od.keys())
    unexpected = found_opcodes - expected_opcodes
    if unexpected:
        fail("T3", f"unexpected opcodes: {unexpected}")
    if sum(od.values()) != summary["total_sites"]:
        fail("T3", f"opcode sum {sum(od.values())} != total {summary['total_sites']}")
    ok("T3", f"opcodes subset of E8/E9/0F8x, sum {sum(od.values())} = total")

    # T4
    required = {"inst_addr", "opcode", "inst_size", "disp32_offset_in_inst",
                "disp32", "target_addr", "parent_fn", "parent_fn_entry", "site_idx"}
    for site in worklist:
        missing = required - set(site.keys())
        if missing:
            fail("T4", f"site missing keys {missing}: {site}")
    ok("T4", f"{len(worklist)} worklist sites all well-formed (incl inst_size + disp32_offset_in_inst)")

    # T5 + T6
    fn_ranges = {fn["entry"]: [(hex_int(r[0]), hex_int(r[1])) for r in fn["body_ranges"]] for fn in inv["functions"]}
    intra_recompute = []
    cross_recompute = []
    for site in sites:
        target = hex_int(site["target_addr"])
        parent_entry = site["parent_fn_entry"]
        ranges = fn_ranges.get(parent_entry)
        if ranges is None:
            continue
        if any(start <= target <= end for (start, end) in ranges):
            intra_recompute.append(site)
        else:
            cross_recompute.append(site)
    if len(intra_recompute) != summary["intra_fn_count"]:
        fail("T5", f"recompute intra {len(intra_recompute)} != summary {summary['intra_fn_count']}")
    ok("T5", f"intra-fn classification reproducible ({len(intra_recompute)})")
    if len(cross_recompute) != summary["cross_fn_count"]:
        fail("T6", f"recompute cross {len(cross_recompute)} != summary {summary['cross_fn_count']}")
    ok("T6", f"cross-fn classification reproducible ({len(cross_recompute)})")

    # T7
    for site in sites:
        inst_addr = hex_int(site["inst_addr"])
        inst_size = site["inst_size"]
        disp32 = site["disp32"]
        target = hex_int(site["target_addr"])
        expected = (inst_addr + inst_size + disp32) & 0xFFFFFFFF
        if expected != target:
            fail("T7", f"site {site['inst_addr']}: target {target:#x} != computed {expected:#x} (disp32={disp32}, inst_size={inst_size})")
    ok("T7", f"all {len(sites)} sites: target_addr = inst_addr + inst_size + disp32")

    # T8
    if summary["no_parent_info_count"] != 0:
        fail("T8", f"{summary['no_parent_info_count']} sites have no parent fn — orphan instructions?")
    ok("T8", "all sites have parent fn in inventory")

    # T9
    for site in sites:
        op = site["opcode"]
        if op in ("E8", "E9"):
            if site["inst_size"] != 5:
                fail("T9", f"{op} site {site['inst_addr']} has inst_size={site['inst_size']} (expected 5)")
            if site["disp32_offset_in_inst"] != 1:
                fail("T9", f"{op} site {site['inst_addr']} has disp32_offset_in_inst={site['disp32_offset_in_inst']} (expected 1)")
        elif op.startswith("0F"):
            if site["inst_size"] != 6:
                fail("T9", f"{op} site {site['inst_addr']} has inst_size={site['inst_size']} (expected 6)")
            if site["disp32_offset_in_inst"] != 2:
                fail("T9", f"{op} site {site['inst_addr']} has disp32_offset_in_inst={site['disp32_offset_in_inst']} (expected 2)")
    ok("T9", "inst_size / disp32_offset_in_inst consistent per opcode class")

    # T10
    for i, site in enumerate(worklist, 1):
        if site["site_idx"] != i:
            fail("T10", f"position {i} has site_idx={site['site_idx']}")
    addrs = [hex_int(s["inst_addr"]) for s in worklist]
    if addrs != sorted(addrs):
        fail("T10", "worklist not sorted by inst_addr ascending")
    ok("T10", f"site_idx 1..{len(worklist)} contiguous + inst_addr ascending")

    print("\nAll tests PASS.")


if __name__ == "__main__":
    main()
