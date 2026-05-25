"""A4 — partition AIL fns and data items into per-.obj groups.

Per the plan A4 spec:
  - every AIL fn becomes its own .obj  (name: ail_<entry_lower_no0x>_<fn_name>.obj)
  - data items owned by a single AIL fn join that fn's .obj
  - data items owned by ≥2 AIL fns become a shared .obj
    (name: ail_shared_<addr_lower_no0x>_<name>.obj)
  - data items not referenced by any AIL fn (orphan, e.g. PUSH imm32 fixup
    target / dead state struct) also become shared .obj — they are still
    part of the vendor library export surface per the complete-lib goal,
    so the lib must contain them; PUBDEF lets external callers (or LE FIXUP
    records pointing at them) resolve at link time.

Output:
  workspace/ail_extract/ail_obj_grouping.json with shape:
  {
    "summary": { "fn_obj_count", "shared_obj_count", "single_owner_data",
                 "multi_owner_data", "orphan_data", "bss_items_in_objs" },
    "fn_objs": [
      { "obj_name": "ail_<entry>_<name>.obj",
        "fn_name", "fn_entry", "fn_cc", "fn_signature", "is_public",
        "body_size", "body_ranges", "owned_data": [
            { "addr", "name", "type", "size", "segment": "data"|"bss" }
        ] }
    ],
    "shared_objs": [
      { "obj_name": "ail_shared_<addr>_<name>.obj",
        "data_addr", "data_name", "data_type", "data_size",
        "segment": "data"|"bss", "owners": [fn names] }
    ]
  }

Inputs (live or live-derived; no KB hardcoded values):
  - workspace/ail_extract/ail_inventory.json     (functions + data_items)
  - workspace/ail_extract/ail_layout.json        (data[] / bss[] segment tag)
  - workspace/ail_extract/raw/ail_data_owners.json
                                                 (addr -> {name, owners[]})

Per [[feedback_no_kb_hardcoded_values]]: no hardcoded address ranges /
counts. All ownership comes from a live Ghidra Java-script xref walk
written to raw/ail_data_owners.json by the dump_ail_data_owners inline
script (see workspace/ail_extract/audit/stale_audit_report.md for the
trigger conditions to re-dump).

Per [[feedback_script_review_then_test]]: assert invariants at the end
(see Validation section). HARD-FAIL on any.

Per [[feedback_per_item_progress_display]]: for N≥20 work, print n/N
progress lines while emitting fn_objs.
"""
from __future__ import annotations

import json
import re
from collections import Counter
from pathlib import Path

REPO = Path(__file__).resolve().parents[2]
WS = REPO / "workspace" / "ail_extract"
INVENTORY = WS / "ail_inventory.json"
LAYOUT = WS / "ail_layout.json"
OWNERS = WS / "raw" / "ail_data_owners.json"
OUT = WS / "ail_obj_grouping.json"


def slugify(name: str) -> str:
    """Make a name safe for a filesystem .obj path.

    Replace anything that's not [A-Za-z0-9_] with '_'. Watcom wlib/wlink
    accept dots and underscores in obj filenames; we play conservative.
    """
    return re.sub(r"[^A-Za-z0-9_]", "_", name)


def main():
    inv = json.loads(INVENTORY.read_text(encoding="utf-8"))
    layout = json.loads(LAYOUT.read_text(encoding="utf-8"))
    owners_raw = json.loads(OWNERS.read_text(encoding="utf-8"))

    fns = inv["functions"]
    data_items = inv["data_items"]

    # Build addr -> segment ("data"|"bss") from layout
    addr_to_seg = {}
    for d in layout["data"]:
        addr_to_seg[d["addr"]] = "data"
    for b in layout["bss"]:
        addr_to_seg[b["addr"]] = "bss"

    # Validate every inventory data_item appears in layout (sanity)
    for di in data_items:
        if di["addr"] not in addr_to_seg:
            raise SystemExit(
                f"FAIL: data item {di['addr']} {di['name']} not in ail_layout.json"
            )

    # Validate owners dump matches inventory data items 1:1
    owners_addrs = set(owners_raw.keys())
    inv_addrs = {di["addr"] for di in data_items}
    if owners_addrs != inv_addrs:
        only_owners = sorted(owners_addrs - inv_addrs)
        only_inv = sorted(inv_addrs - owners_addrs)
        raise SystemExit(
            f"FAIL: owners ({len(owners_addrs)}) ≠ inventory data_items "
            f"({len(inv_addrs)}); only_in_owners={only_owners[:5]} "
            f"only_in_inv={only_inv[:5]}"
        )

    # Categorise each data item
    single_owner = {}    # addr -> owner_fn_name
    multi_owner = []     # list of addrs
    orphan = []          # list of addrs
    for addr, rec in owners_raw.items():
        owners = rec["owners"]
        if len(owners) == 1:
            single_owner[addr] = owners[0]
        elif len(owners) >= 2:
            multi_owner.append(addr)
        else:
            orphan.append(addr)

    # Build fn_obj entries
    fn_data_attach = {fn["name"]: [] for fn in fns}
    di_by_addr = {di["addr"]: di for di in data_items}
    for addr, owner_fn in single_owner.items():
        if owner_fn not in fn_data_attach:
            raise SystemExit(
                f"FAIL: single-owner data {addr} attached to unknown fn '{owner_fn}'"
            )
        fn_data_attach[owner_fn].append(addr)

    fn_objs = []
    n = len(fns)
    for i, fn in enumerate(fns, 1):
        entry_lower = fn["entry"].lstrip("0").lower() or "0"
        slug = slugify(fn["name"])
        obj_name = f"ail_{entry_lower}_{slug}.obj"
        owned_data = []
        for addr in sorted(fn_data_attach[fn["name"]],
                           key=lambda a: int(a, 16)):
            di = di_by_addr[addr]
            owned_data.append({
                "addr": addr,
                "name": di["name"],
                "type": di["type"],
                "size": di["size"],
                "segment": addr_to_seg[addr],
            })
        fn_objs.append({
            "obj_name": obj_name,
            "fn_name": fn["name"],
            "fn_entry": fn["entry"],
            "fn_cc": fn["cc"],
            "fn_signature": fn["signature"],
            "is_public": not fn["name"].startswith("AIL_internal_"),
            "body_size": fn["body_size"],
            "body_ranges": fn["body_ranges"],
            "owned_data": owned_data,
        })
        if i % 50 == 0 or i == n:
            print(f"  [{i}/{n}] fn_obj built")

    # Build shared_obj entries (multi + orphan)
    shared_objs = []
    for addr in sorted(multi_owner + orphan, key=lambda a: int(a, 16)):
        di = di_by_addr[addr]
        addr_lower = addr.lstrip("0").lower() or "0"
        slug = slugify(di["name"])
        obj_name = f"ail_shared_{addr_lower}_{slug}.obj"
        shared_objs.append({
            "obj_name": obj_name,
            "data_addr": addr,
            "data_name": di["name"],
            "data_type": di["type"],
            "data_size": di["size"],
            "segment": addr_to_seg[addr],
            "owners": owners_raw[addr]["owners"],
            "owner_count": len(owners_raw[addr]["owners"]),
        })

    # Validate: every data item appears in exactly one .obj
    appearance = Counter()
    for obj in fn_objs:
        for d in obj["owned_data"]:
            appearance[d["addr"]] += 1
    for obj in shared_objs:
        appearance[obj["data_addr"]] += 1
    bad = [(a, c) for a, c in appearance.items() if c != 1]
    if bad:
        raise SystemExit(f"FAIL: {len(bad)} data items not exactly 1 obj: {bad[:5]}")
    if set(appearance.keys()) != inv_addrs:
        missing = inv_addrs - set(appearance.keys())
        extra = set(appearance.keys()) - inv_addrs
        raise SystemExit(f"FAIL: data items not 1:1 between inv and grouping; "
                         f"missing={list(missing)[:5]} extra={list(extra)[:5]}")

    # Validate: fn obj name uniqueness
    fn_obj_names = [o["obj_name"] for o in fn_objs]
    if len(set(fn_obj_names)) != len(fn_obj_names):
        dup = [n for n, c in Counter(fn_obj_names).items() if c > 1]
        raise SystemExit(f"FAIL: duplicate fn_obj names: {dup}")
    shared_obj_names = [o["obj_name"] for o in shared_objs]
    if len(set(shared_obj_names)) != len(shared_obj_names):
        dup = [n for n, c in Counter(shared_obj_names).items() if c > 1]
        raise SystemExit(f"FAIL: duplicate shared_obj names: {dup}")
    overlap = set(fn_obj_names) & set(shared_obj_names)
    if overlap:
        raise SystemExit(f"FAIL: fn_obj/shared_obj name overlap: {overlap}")

    # bss items counted in fn_objs
    bss_in_objs = sum(1 for obj in fn_objs for d in obj["owned_data"]
                      if d["segment"] == "bss") \
                + sum(1 for o in shared_objs if o["segment"] == "bss")

    summary = {
        "fn_obj_count": len(fn_objs),
        "shared_obj_count": len(shared_objs),
        "single_owner_data": len(single_owner),
        "multi_owner_data": len(multi_owner),
        "orphan_data": len(orphan),
        "bss_items_in_objs": bss_in_objs,
        "total_data_items_covered": sum(appearance.values()),
    }

    OUT.write_text(
        json.dumps(
            {"summary": summary, "fn_objs": fn_objs, "shared_objs": shared_objs},
            indent=2, ensure_ascii=False,
        ),
        encoding="utf-8",
    )
    print()
    print(f"Wrote {OUT}")
    print(f"  fn .obj:           {summary['fn_obj_count']}")
    print(f"  shared-data .obj:  {summary['shared_obj_count']}")
    print(f"  single-owner data: {summary['single_owner_data']}")
    print(f"  multi-owner data:  {summary['multi_owner_data']}")
    print(f"  orphan data:       {summary['orphan_data']}")
    print(f"  bss items grouped: {summary['bss_items_in_objs']}")
    print(f"  total .obj count:  {summary['fn_obj_count'] + summary['shared_obj_count']}")


if __name__ == "__main__":
    main()
