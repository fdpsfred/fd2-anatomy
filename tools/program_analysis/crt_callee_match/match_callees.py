"""Find candidate Watcom CRT lib functions that match each unidentified callee.

For each callee (address + size + list of caller CRT functions), reports three
tiers of lib candidate:
  Tier A — funcs in any caller's `source_obj` (same translation unit; high
           confidence for CRT internal helpers)
  Tier B — exact size match in other .obj
  Tier C — size within ±25% in other .obj (only if A+B are sparse)

Inputs:
  --callees    JSON list of {addr, size, callers: [{addr, name}]}.
  --obj-funcs  JSON output from `extract_obj_funcs.py` (per-.obj func list).
  --lookup     `rebuild_info/crt/lookup_9.5a.json` (used for caller→source_obj).
  --out        Output markdown report path.
"""
from __future__ import annotations
import argparse, json, sys
from pathlib import Path


def main() -> int:
    repo = Path(__file__).resolve().parents[3]
    default_out = repo / "workspace" / "crt_callee_match" / "candidates.md"
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--callees", type=Path, required=True)
    ap.add_argument("--obj-funcs", type=Path, required=True)
    ap.add_argument("--lookup", type=Path, required=True)
    ap.add_argument("--out", type=Path, default=default_out,
                    help=f"output markdown path. default: workspace/crt_callee_match/candidates.md")
    args = ap.parse_args()

    callees = json.loads(args.callees.read_text())
    obj_funcs = json.loads(args.obj_funcs.read_text())
    lookup = json.loads(args.lookup.read_text())["by_address"]

    by_size: dict[int, list[tuple]] = {}
    for obj, funcs in obj_funcs.items():
        for f in funcs:
            by_size.setdefault(f["size"], []).append((obj, f["name"], f["offset"], f["seg_len"]))

    lines = ["# Non-CRT callee → lib .obj candidate matches\n"]
    lines.append(f"Source: {len(obj_funcs)} .obj / {sum(len(v) for v in obj_funcs.values())} funcs\n")

    for c in callees:
        callee_addr = c["addr"]
        callee_size = c["size"]
        callers = c.get("callers", [])

        lines.append(f"\n## `{callee_addr}` (size = {callee_size} bytes)")
        lines.append("")
        lines.append("**Callers (CRT funcs):**")
        for ca in callers:
            src = lookup.get(ca["addr"], {}).get("source_obj", "?")
            lines.append(f"- `{ca['addr']}` `{ca['name']}` ← from `{src}`")

        caller_objs = set()
        for ca in callers:
            so = lookup.get(ca["addr"], {}).get("source_obj")
            if so:
                caller_objs.add(so)
        tier_a: list[tuple] = []
        for obj in caller_objs:
            for f in obj_funcs.get(obj, []):
                tier_a.append((obj, f["name"], f["size"], f["offset"], f["seg_len"]))
        tier_a.sort(key=lambda r: abs(r[2] - callee_size))

        lines.append("")
        lines.append("**Tier A — funcs in caller's source .obj** (sorted by size diff):")
        if not tier_a:
            lines.append("- (none)")
        else:
            for obj, name, sz, ofs, seg_len in tier_a[:15]:
                marker = " ✱" if sz == callee_size else ""
                lines.append(f"- `{obj}` :: `{name}` size={sz}, offset={ofs}, seg_len={seg_len}{marker}")

        exact = by_size.get(callee_size, [])
        exact_other = [r for r in exact if r[0] not in caller_objs]
        lines.append("")
        lines.append(f"**Tier B — exact size match in OTHER .obj** ({len(exact_other)} matches):")
        if not exact_other:
            lines.append("- (none)")
        else:
            for obj, name, ofs, seg_len in exact_other[:20]:
                lines.append(f"- `{obj}` :: `{name}` offset={ofs}, seg_len={seg_len}")

        if not tier_a and len(exact_other) < 5:
            tol = max(2, callee_size // 4)
            lo, hi = max(0, callee_size - tol), callee_size + tol
            close: list[tuple] = []
            for sz, lst in by_size.items():
                if lo <= sz <= hi and sz != callee_size:
                    for r in lst:
                        if r[0] not in caller_objs:
                            close.append((sz, *r))
            close.sort(key=lambda r: abs(r[0] - callee_size))
            lines.append("")
            lines.append(f"**Tier C — ±{tol}-byte size in OTHER .obj** ({len(close)} matches; top 10):")
            for sz, obj, name, ofs, seg_len in close[:10]:
                lines.append(f"- `{obj}` :: `{name}` size={sz} offset={ofs} seg_len={seg_len}")

    args.out.parent.mkdir(parents=True, exist_ok=True)
    args.out.write_text("\n".join(lines) + "\n", encoding="utf-8")
    print(f"Wrote {args.out} ({len(lines)} lines)")
    return 0


if __name__ == "__main__":
    sys.exit(main())
