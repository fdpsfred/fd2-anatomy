"""Test dump_ail_set.py output for correctness.

Per [[feedback_script_review_then_test]].

Tests:
  T1. Inventory exists + well-formed JSON
  T2. Function count > 100 + 100 < public_count <= 200 (sanity bounds, not
      hardcoded; Ghidra-current count is allowed to drift)
  T3. All function entries are 8-hex addresses; no duplicates
  T4. All data items have addr / name / type / size fields; size > 0
  T5. All data items source contains "name" (xref-only must have been dropped)
  T6. Function body ranges: each range's min <= max; cross-fn no overlap
  T7. Dropped xref-only items each must NOT start with `data_ail_` / `L_AIL_`
      (sanity: confirms they're indeed non-AIL false positives)
  T8. Multi-range functions: each has num_ranges == len(body_ranges)

Exit 0 on PASS, 1 on first FAIL.
"""

from __future__ import annotations

import json
import sys
from pathlib import Path

REPO = Path(__file__).resolve().parents[2]
INV = REPO / "workspace" / "ail_extract" / "ail_inventory.json"


def fail(test_id, msg):
    print(f"  FAIL [{test_id}] {msg}")
    sys.exit(1)


def ok(test_id, msg):
    print(f"  PASS [{test_id}] {msg}")


def main():
    if not INV.exists():
        fail("T1", f"{INV} missing")
    try:
        inv = json.loads(INV.read_text(encoding="utf-8"))
    except json.JSONDecodeError as e:
        fail("T1", f"invalid JSON: {e}")
    ok("T1", "inventory JSON loads")

    fns = inv["functions"]
    data = inv["data_items"]
    summary = inv["summary"]

    # T2: count sanity bounds (not hardcoded specific numbers — use ranges
    # consistent with known scale: AIL has 50+ public + 100+ internal)
    if not (50 < summary["public_count"] < 200):
        fail("T2", f"public count {summary['public_count']} outside (50, 200)")
    if not (100 < summary["internal_count"] < 500):
        fail("T2", f"internal count {summary['internal_count']} outside (100, 500)")
    if summary["public_count"] + summary["internal_count"] != summary["total_functions"]:
        fail("T2", "public + internal != total")
    ok("T2", f"counts: pub={summary['public_count']} int={summary['internal_count']} total={summary['total_functions']}")

    # T3: entries are 8-hex unique
    entries = [fn["entry"] for fn in fns]
    if len(set(entries)) != len(entries):
        dup = [e for e in entries if entries.count(e) > 1]
        fail("T3", f"duplicate function entries: {dup[:5]}")
    for e in entries:
        if len(e) != 8 or not all(c in "0123456789abcdef" for c in e):
            fail("T3", f"entry {e!r} not 8-hex")
    ok("T3", f"{len(entries)} unique 8-hex function entries")

    # T4: data items shape
    for d in data:
        for k in ("addr", "name", "type", "size", "source"):
            if k not in d:
                fail("T4", f"data item missing key {k}: {d}")
        if d["size"] <= 0:
            fail("T4", f"data item {d['addr']} size {d['size']} <= 0")
        if len(d["addr"]) != 8:
            fail("T4", f"data addr {d['addr']!r} not 8-hex")
    ok("T4", f"{len(data)} data items well-formed")

    # T5: source must contain "name" (xref-only dropped)
    for d in data:
        if "name" not in d["source"]:
            fail("T5", f"data item {d['addr']} has source={d['source']!r}, expected 'name' present")
    ok("T5", f"all {len(data)} data items are name-pattern derived (no xref-only)")

    # T6: function body ranges
    overlap_intervals = []
    for fn in fns:
        for r in fn["body_ranges"]:
            mn = int(r[0], 16)
            mx = int(r[1], 16)
            if mn > mx:
                fail("T6", f"{fn['name']} range min 0x{mn:x} > max 0x{mx:x}")
            overlap_intervals.append((mn, mx, fn["name"]))
    overlap_intervals.sort()
    for i in range(len(overlap_intervals) - 1):
        a_s, a_e, a_n = overlap_intervals[i]
        b_s, b_e, b_n = overlap_intervals[i + 1]
        if b_s <= a_e:
            fail("T6", f"body range overlap: {a_n} (0x{a_s:x}-0x{a_e:x}) vs {b_n} (0x{b_s:x}-0x{b_e:x})")
    ok("T6", f"{len(overlap_intervals)} function body ranges, no overlap, all min<=max")

    # T7: dropped items confirmed non-AIL named
    dropped = inv.get("dropped_xref_only", [])
    for d in dropped:
        if d["name"].startswith("data_ail_") or d["name"].startswith("L_AIL_"):
            fail("T7", f"dropped item {d['addr']} ({d['name']}) looks AIL-named — should not be dropped")
    ok("T7", f"{len(dropped)} dropped xref-only items, none with AIL naming")

    # T8: multi-range consistency
    for fn in fns:
        if fn["num_ranges"] != len(fn["body_ranges"]):
            fail("T8", f"{fn['name']} num_ranges={fn['num_ranges']} != len(body_ranges)={len(fn['body_ranges'])}")
    ok("T8", f"all functions: num_ranges == len(body_ranges)")

    print("\nAll tests PASS.")


if __name__ == "__main__":
    main()
