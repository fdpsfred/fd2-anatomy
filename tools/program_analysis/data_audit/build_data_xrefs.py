"""Manage data-item xref dumps for the data_audit pipeline.

Two modes:

A. `--prep-batches [N]` (default N=100) — read latest
   `workspace/data_audit/ghidra_data_dump_*.json`, split its address list
   into N-sized batches, and print one comma-separated address line per
   batch to stdout. Claude Code feeds each line to
   `mcp__ghidra__get_bulk_xrefs(addresses=<line>)` and writes the result
   to `workspace/data_audit/raw_xrefs_batch_<NN>.json` (1-indexed; same
   order as printed).

B. (default, no flag) — consolidate every
   `workspace/data_audit/raw_xrefs_batch_*.json` (MCP envelope `{"result":
   "..."}`) into one normalised file `ghidra_data_xrefs_<utc>.json` ready
   for `build_xref_graph.py`. Steps:

   1. Build sorted function-entry index from
      `workspace/call_graph/raw/list_functions_enhanced.txt` (MCP envelope
      also handled).
   2. For each xref's instruction address, bisect-resolve into its
      containing function entry.
   3. Write `workspace/data_audit/ghidra_data_xrefs_<utc>.json`:

          {"xrefs": {
            "<data_addr>": [
              {"source_addr": "<instr_addr>",
               "source_fn":   "<fn_entry_addr>",
               "type":        "DATA|READ|WRITE"},
              ...
            ],
            ...
          }}

This script does NOT call MCP — `get_bulk_xrefs` calls run in Claude Code
between modes A and B.
"""

from __future__ import annotations

import argparse
import json
import sys
from bisect import bisect_right
from datetime import datetime, timezone
from pathlib import Path

REPO = Path(__file__).resolve().parents[3]
WORKSPACE = REPO / "workspace" / "data_audit"
CALL_GRAPH_RAW = REPO / "workspace" / "call_graph" / "raw" / "list_functions_enhanced.txt"


def find_latest_data_dump() -> Path:
    dumps = sorted(WORKSPACE.glob("ghidra_data_dump_*.json"))
    if not dumps:
        raise SystemExit(
            "ERROR: no ghidra_data_dump_*.json in workspace/data_audit/. "
            "Dump via `mcp__ghidra__list_data_items(limit=10000)` first."
        )
    return dumps[-1]


def cmd_prep_batches(size: int) -> int:
    dump = find_latest_data_dump()
    payload = _unwrap(dump.read_text(encoding="utf-8"))
    items = payload["items"]
    addrs = [it["addr"].lower().zfill(8) for it in items]
    n_batches = (len(addrs) + size - 1) // size
    for i in range(0, len(addrs), size):
        print(",".join(addrs[i:i + size]))
    # Hints printed to stderr so stdout stays usable for piping.
    print(f"# {len(addrs)} addrs split into {n_batches} batch(es) of "
          f"≤{size}; save each result to raw_xrefs_batch_<NN>.json (01..{n_batches:02d})",
          file=sys.stderr)
    return 0


def _unwrap(text: str):
    """Tolerate MCP envelope `{"result": "..."}` or raw JSON."""
    blob = json.loads(text)
    if isinstance(blob, dict) and "result" in blob and isinstance(blob["result"], str):
        return json.loads(blob["result"])
    return blob


def load_function_index() -> tuple[list[int], list[str]]:
    """Return (sorted entry addrs as int, parallel entry addrs as 8-hex string)."""
    if not CALL_GRAPH_RAW.exists():
        raise SystemExit(
            f"ERROR: {CALL_GRAPH_RAW.relative_to(REPO)} missing. Dump via "
            "`mcp__ghidra__list_functions_enhanced(limit=10000)` first."
        )
    payload = _unwrap(CALL_GRAPH_RAW.read_text(encoding="utf-8"))
    fns = payload["functions"]
    entries = sorted((int(f["address"], 16), f["address"].lower().zfill(8))
                     for f in fns)
    return [e[0] for e in entries], [e[1] for e in entries]


def resolve_fn(instr_int: int, fn_addrs: list[int], fn_hex: list[str]) -> str | None:
    idx = bisect_right(fn_addrs, instr_int) - 1
    if idx < 0:
        return None
    return fn_hex[idx]


def load_batches() -> tuple[dict[str, list[dict]], list[Path]]:
    """Merge every raw_xrefs_batch_*.json under WORKSPACE.

    Returns ({data_addr: [{from, type}, ...]}, sorted batch paths).
    """
    batch_files = sorted(WORKSPACE.glob("raw_xrefs_batch_*.json"))
    if not batch_files:
        raise SystemExit(
            "ERROR: no raw_xrefs_batch_*.json in workspace/data_audit/. "
            "Dump batches via `mcp__ghidra__get_bulk_xrefs` first."
        )
    merged: dict[str, list[dict]] = {}
    for p in batch_files:
        payload = _unwrap(p.read_text(encoding="utf-8"))
        # payload schema: {data_addr_hex: [{"from": instr, "type": kind}, ...]}
        for addr, xs in payload.items():
            merged.setdefault(addr.lower().zfill(8), []).extend(xs)
    return merged, batch_files


def cmd_consolidate() -> int:
    fn_addrs, fn_hex = load_function_index()
    print(f"loaded {len(fn_addrs)} function entries for instr→fn resolution")

    raw_xrefs, batch_files = load_batches()
    print(f"merged {len(batch_files)} raw batch(es) covering {len(raw_xrefs)} data items")

    out: dict[str, list[dict]] = {}
    unresolved = 0
    total_edges = 0
    for data_addr, xs in raw_xrefs.items():
        rec = []
        for x in xs:
            instr_hex = x["from"].lower().zfill(8)
            src_fn = resolve_fn(int(instr_hex, 16), fn_addrs, fn_hex)
            if src_fn is None:
                unresolved += 1
            rec.append({
                "source_addr": instr_hex,
                "source_fn":   src_fn,
                "type":        x.get("type"),
            })
            total_edges += 1
        if rec:
            out[data_addr] = rec

    ts = datetime.now(timezone.utc).strftime("%Y%m%dT%H%M%SZ")
    out_path = WORKSPACE / f"ghidra_data_xrefs_{ts}.json"
    out_path.write_text(
        json.dumps({"xrefs": out}, indent=2, ensure_ascii=False),
        encoding="utf-8",
    )

    print(f"\nwrote {out_path.relative_to(REPO)}")
    print(f"  data items with xrefs : {len(out)}")
    print(f"  total xref edges      : {total_edges}")
    print(f"  unresolved instr→fn   : {unresolved}")
    return 0


def main(argv: list[str]) -> int:
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--prep-batches", nargs="?", const=100, type=int, metavar="N",
                    help="print N-sized batch address lines from the latest "
                         "ghidra_data_dump_*.json (default N=100), then exit")
    args = ap.parse_args(argv[1:])
    if args.prep_batches is not None:
        return cmd_prep_batches(args.prep_batches)
    return cmd_consolidate()


if __name__ == "__main__":
    sys.exit(main(sys.argv))
