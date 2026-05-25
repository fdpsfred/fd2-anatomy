"""Consolidate Ghidra raw dumps into AIL inventory.

Per [[feedback_no_kb_hardcoded_values]]: no hardcoded address ranges or counts.
All AIL ownership data is from Ghidra (live symbol table + xref walk).

Per [[feedback_script_review_then_test]]: must be reviewed + tested before
its output is trusted; see test_dump_ail_set.py.

Inputs (raw Ghidra dumps from run_script_inline, written to
`workspace/ail_extract/raw/` by `dump_ail_inventory.java`):
  - `ail_fn_metadata.json` : 428 AIL fn metadata (entry/body/cc/signature)
  - `ail_data_items.json`  : combined name-pattern + xref data items with
                             source label per item

Output:
  - `workspace/ail_extract/ail_inventory.json`

Filtering rules:
  - Functions: keep all (search by name prefix "AIL_" is the Ghidra authority).
  - Data items: keep items whose source contains "name" (Ghidra USER_DEFINED
    `data_ail_*` / `L_AIL_*` / `ail_*` prefix). Items without a name match
    become `dropped_xref_only`. Among the dropped, two sub-classes exist:
      • Genuine Ghidra immediate-vs-data false positives (per
        pipeline_spec §E-7c) — e.g. `MOV EAX,0x10000` mis-classified as a
        data ref. Verified: `data_crt_obj1_head_int3_trap_nop_pad` and
        `data_crt_emu387_func_name_table`.
      • Mid-data references — addr falls inside an AIL data item's body
        (e.g. `data_ail_preferences_state_buffer[4]` for offset +4 of the
        base buffer at 0x5430c). These are NOT separate items; they are
        resolved at OMF emit time via the "data 方法 B" alt-offset PUBDEF
        convention (see bin_to_omf.py:resolve_le_target /
        build_alt_data_labels). Dropping them at inventory level is
        correct — they live inside their container item.
"""

from __future__ import annotations

import json
from pathlib import Path

REPO = Path(__file__).resolve().parents[2]
RAW = REPO / "workspace" / "ail_extract" / "raw"
OUT = REPO / "workspace" / "ail_extract" / "ail_inventory.json"

FN_META = RAW / "ail_fn_metadata.json"
DATA_ITEMS = RAW / "ail_data_items.json"


def main():
    fns = json.loads(FN_META.read_text(encoding="utf-8"))
    items = json.loads(DATA_ITEMS.read_text(encoding="utf-8"))

    # Filter data items: name-pattern only.
    # Xref-only items split into two classes — both stay in dropped_xref_only
    # at inventory level; mid-data refs are recovered later at OMF emit time.
    data_kept = [d for d in items if "name" in d["source"]]
    data_dropped = [d for d in items if "name" not in d["source"]]

    # Categorise functions
    publics = [fn for fn in fns if not fn["name"].startswith("AIL_internal_")]
    internals = [fn for fn in fns if fn["name"].startswith("AIL_internal_")]

    # CC distribution
    cc_dist = {}
    for fn in fns:
        cc = fn["cc"] or "<none>"
        cc_dist[cc] = cc_dist.get(cc, 0) + 1

    # Multi-range detection
    multi_range = [fn for fn in fns if fn["num_ranges"] > 1]

    inventory = {
        "summary": {
            "total_functions": len(fns),
            "public_count": len(publics),
            "internal_count": len(internals),
            "cc_distribution": cc_dist,
            "multi_range_functions": len(multi_range),
            "data_item_count": len(data_kept),
            "data_xref_only_dropped": len(data_dropped),
        },
        "functions": sorted(fns, key=lambda f: int(f["entry"], 16)),
        "data_items": sorted(data_kept, key=lambda d: int(d["addr"], 16)),
        "dropped_xref_only": sorted(data_dropped, key=lambda d: int(d["addr"], 16)),
        "multi_range_diagnostics": [
            {"name": fn["name"], "entry": fn["entry"], "num_ranges": fn["num_ranges"], "ranges": fn["body_ranges"]}
            for fn in multi_range
        ],
    }

    OUT.parent.mkdir(parents=True, exist_ok=True)
    OUT.write_text(json.dumps(inventory, indent=2, ensure_ascii=False), encoding="utf-8")

    print(f"Wrote {OUT}")
    print(f"  total functions:        {len(fns)}")
    print(f"  public:                 {len(publics)}")
    print(f"  internal:               {len(internals)}")
    print(f"  cc distribution:        {cc_dist}")
    print(f"  multi-range bodies:     {len(multi_range)}")
    print(f"  data items (kept):      {len(data_kept)}")
    print(f"  data items (dropped):   {len(data_dropped)}  (xref-only false positives)")
    if multi_range:
        print("  multi-range list:")
        for fn in multi_range:
            print(f"    {fn['name']} @ {fn['entry']} -> {fn['num_ranges']} ranges")
    if data_dropped:
        print("  dropped xref-only items (verify they're non-AIL):")
        for d in data_dropped:
            print(f"    {d['addr']}  {d['name']}  ({d['type']}, {d['size']}B)")


if __name__ == "__main__":
    main()
