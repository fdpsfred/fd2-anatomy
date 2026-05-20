"""Build bipartite xref graph: data_addr ↔ function_addr ↔ data_addr.

Output (`workspace/data_audit/xref_graph.json`):

    {
      "data_to_fn":  {<data_addr>: [<fn_addr>, ...]},
      "fn_to_data":  {<fn_addr>:   [<data_addr>, ...]},
      "data_to_data_via_fixup":   {<data_addr_holder>: [<data_addr_target>, ...]},
      "stats": {"data_with_direct_xref": N, "data_with_indirect_only": M,
                "data_orphan": K, ...}
    }

Used by `build_worklist.py` bucket classification (B2/B3 vs B5/B6) and
per-item reviewer for caller-pool / subsystem inference.

## Inputs

* `workspace/data_audit/ghidra_data_dump_<utc>.json` (latest) — canonical
  source of data addresses
* `workspace/function_audit/ghidra_dump_<utc>.json` (latest) — resolves
  caller fn name from xref source
* `workspace/data_audit/le_fixups.json` — for data→data indirect edges
* `workspace/data_audit/ghidra_data_xrefs_<utc>.json` (latest) — produced
  by `build_data_xrefs.py`; if absent the `data_to_fn` direction is empty

The xref dump schema:

    {
      "xrefs": {
        "<data_addr_8hex>": [
           {"source_addr": "<8hex>", "source_fn": "<fn_addr_8hex>",
            "type": "DATA|READ|WRITE"},
           ...
        ],
        ...
      }
    }
"""

from __future__ import annotations

import json
import sys
from pathlib import Path

REPO = Path(__file__).resolve().parents[3]
WORKSPACE = REPO / "workspace" / "data_audit"
FN_WORKSPACE = REPO / "workspace" / "function_audit"
OUT = WORKSPACE / "xref_graph.json"


def _unwrap(text: str):
    blob = json.loads(text)
    if isinstance(blob, dict) and "result" in blob and isinstance(blob["result"], str):
        return json.loads(blob["result"])
    return blob


def latest(glob_pat: str, root: Path) -> Path | None:
    matches = sorted(root.glob(glob_pat))
    return matches[-1] if matches else None


def normalise_addr(s: str) -> str:
    s = s.strip().lower()
    if s.startswith("0x"):
        s = s[2:]
    return s.zfill(8)


def load_function_dump() -> dict[str, str]:
    """Return fn_addr (8-hex) → fn_name."""
    fdump = latest("ghidra_dump_*.json", FN_WORKSPACE)
    if fdump is None:
        print("  WARN: no function_audit ghidra_dump_*.json found; "
              "fn addr→name mapping unavailable")
        return {}
    payload = _unwrap(fdump.read_text(encoding="utf-8"))
    return {normalise_addr(fn["address"]): fn["name"]
            for fn in payload["functions"]}


def load_data_dump() -> list[dict]:
    ddump = latest("ghidra_data_dump_*.json", WORKSPACE)
    if ddump is None:
        raise SystemExit("ERROR: no ghidra_data_dump_*.json under "
                         "workspace/data_audit/")
    payload = _unwrap(ddump.read_text(encoding="utf-8"))
    return payload["items"]


def load_xref_dump() -> dict:
    xdump = latest("ghidra_data_xrefs_*.json", WORKSPACE)
    if xdump is None:
        print("  WARN: no ghidra_data_xrefs_*.json under "
              "workspace/data_audit/ — data_to_fn empty")
        return {"xrefs": {}}
    return _unwrap(xdump.read_text(encoding="utf-8"))


def load_le_fixups() -> dict:
    p = WORKSPACE / "le_fixups.json"
    if not p.exists():
        return {}
    try:
        return json.loads(p.read_text(encoding="utf-8"))
    except json.JSONDecodeError:
        return {}


def main() -> int:
    WORKSPACE.mkdir(parents=True, exist_ok=True)
    items = load_data_dump()
    fn_addr_to_name = load_function_dump()
    xref_blob = load_xref_dump()
    fixups = load_le_fixups()

    data_addrs = {normalise_addr(it["addr"]) for it in items}

    data_to_fn: dict[str, list[str]] = {}
    fn_to_data: dict[str, list[str]] = {}
    for data_addr, xrefs in xref_blob.get("xrefs", {}).items():
        da = normalise_addr(data_addr)
        for x in xrefs:
            src_fn = x.get("source_fn")
            if src_fn is None:
                continue
            sf = normalise_addr(src_fn)
            data_to_fn.setdefault(da, []).append(sf)
            fn_to_data.setdefault(sf, []).append(da)

    # Dedup
    for k in data_to_fn:
        data_to_fn[k] = sorted(set(data_to_fn[k]))
    for k in fn_to_data:
        fn_to_data[k] = sorted(set(fn_to_data[k]))

    # Indirect via LE fixup: a data_item X holds a pointer to another
    # data_item Y, recorded in fixups.target_addr_to_sources[Y].
    # We invert that so the holder data lists its targets.
    data_to_data: dict[str, list[str]] = {}
    target_to_src = fixups.get("target_addr_to_sources", {})
    for tgt, sources in target_to_src.items():
        tgt_n = normalise_addr(tgt)
        for s in sources:
            # parse_le_fixup writes source addresses as plain hex strings
            # (legacy schema used dicts with "source_addr"; tolerate both)
            if isinstance(s, dict):
                src_addr = s.get("source_addr")
                if src_addr is None:
                    continue
            else:
                src_addr = s
            # If the fixup source falls inside any known data item, it
            # represents data→data indirection
            sa = normalise_addr(src_addr)
            # Find the containing data item by address-range walk
            for it in items:
                base = int(normalise_addr(it["addr"]), 16)
                if base <= int(sa, 16) < base + int(it["size"]):
                    holder = normalise_addr(it["addr"])
                    data_to_data.setdefault(holder, []).append(tgt_n)
                    break

    for k in data_to_data:
        data_to_data[k] = sorted(set(data_to_data[k]))

    n_direct = len(data_to_fn)
    n_indirect_only = sum(1 for da in data_addrs
                          if da not in data_to_fn
                          and da in target_to_src)
    n_orphan = sum(1 for da in data_addrs
                   if da not in data_to_fn
                   and da not in target_to_src)

    out = {
        "data_to_fn":  data_to_fn,
        "fn_to_data":  fn_to_data,
        "data_to_data_via_fixup": data_to_data,
        "stats": {
            "data_with_direct_xref":   n_direct,
            "data_with_indirect_only": n_indirect_only,
            "data_orphan":             n_orphan,
            "total_data":              len(data_addrs),
            "fn_addrs_loaded":         len(fn_addr_to_name),
        },
    }
    OUT.write_text(json.dumps(out, indent=2, ensure_ascii=False), encoding="utf-8")
    print(f"written: {OUT.relative_to(REPO)}")
    print(f"  data with direct xref:   {n_direct}")
    print(f"  data indirect-only:      {n_indirect_only}")
    print(f"  data orphan:             {n_orphan}")
    print(f"  total data items:        {len(data_addrs)}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
