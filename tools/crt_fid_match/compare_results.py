"""Cross-version analysis of FD2.LE FidDb match results.

Inputs: workspace/fid_match/results/matches_<ver>.json (one per Watcom 9.5
version, produced by ghidra_scripts/FidQuery.java).

Computes:
- Per-version match count and the matched FD2 addresses
- Set-comparison: matches that are in <max-count version> but not in others,
  and vice versa (the user's "superset hypothesis" check — there should be no
  match present in some lower-count version but missing from the highest one)
- Per-FD2-address aggregation showing which versions matched the same function
  and what library symbol they pointed to

Writes a human-readable report to workspace/fid_match/results/report.md.
"""
from __future__ import annotations

import argparse
import json
from collections import defaultdict
from pathlib import Path


VERSIONS = ["9.5", "9.5a", "9.5b", "9.5c"]


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--results-dir", type=Path,
                    default=Path("workspace/fid_match/results"))
    ap.add_argument("--out", type=Path,
                    default=Path("workspace/fid_match/results/report.md"))
    args = ap.parse_args()

    by_ver: dict[str, dict] = {}
    for v in VERSIONS:
        p = args.results_dir / f"matches_{v}.json"
        by_ver[v] = json.loads(p.read_text())

    # Set of FD2 addresses matched per version
    addr_sets: dict[str, set[str]] = {
        v: {m["address"] for m in by_ver[v]["matches"]}
        for v in VERSIONS
    }
    # Per-version count
    counts = {v: len(addr_sets[v]) for v in VERSIONS}
    max_count = max(counts.values())
    leaders = [v for v, c in counts.items() if c == max_count]

    # Aggregate per FD2 address: which versions matched and to what symbol
    by_addr: dict[str, dict] = defaultdict(lambda: {
        "current_name": None,
        "body_size": None,
        "versions": {},  # version -> [(matched_name, score, source_obj), ...]
    })
    for v in VERSIONS:
        for m in by_ver[v]["matches"]:
            rec = by_addr[m["address"]]
            rec["current_name"] = m["current_name"]
            rec["body_size"] = m["body_size"]
            rec["versions"][v] = [
                (c["matched_name"], c["score"], (c["source_obj"] or "").replace("/watcom_libs/", ""))
                for c in m["candidates"]
            ]

    # Superset hypothesis check: for each leader version, list any address that
    # other versions matched but the leader did not.
    violations: dict[str, list[tuple[str, list[str]]]] = {}
    for leader in leaders:
        leader_set = addr_sets[leader]
        bad = []
        for addr, rec in sorted(by_addr.items()):
            if addr in leader_set:
                continue
            others = [v for v in VERSIONS if v != leader and addr in rec["versions"]]
            if others:
                bad.append((addr, others))
        violations[leader] = bad

    # Write report
    lines: list[str] = []
    lines.append("# FD2.LE × Watcom 9.5(x) FidDb match report\n")
    lines.append(f"Source: workspace/fid_match/results/matches_*.json (threshold=0)\n")

    lines.append("## Per-version match count\n")
    lines.append("| version | matched FD2 functions |")
    lines.append("| --- | ---: |")
    for v in VERSIONS:
        marker = "  ← leader" if v in leaders else ""
        lines.append(f"| {v} | {counts[v]}{marker} |")
    lines.append("")
    lines.append(f"**Leader**: {', '.join(leaders)} (each match {max_count} FD2 functions)\n")

    lines.append("## Superset-hypothesis check\n")
    lines.append("For the leader version to be a true superset, no other version "
                 "should match an FD2 function the leader missed.\n")
    any_violation = False
    for leader, bad in violations.items():
        if not bad:
            lines.append(f"- **{leader}**: ✓ holds — no FD2 function matched by another version was missed by {leader}.")
        else:
            any_violation = True
            lines.append(f"- **{leader}**: ✗ violated — the following FD2 functions were matched by other version(s) but **not** by {leader}:")
            for addr, others in bad:
                rec = by_addr[addr]
                lines.append(f"   - `{addr}` `{rec['current_name']}` (body {rec['body_size']}) — matched in {', '.join(others)}")
    lines.append("")
    if not any_violation:
        lines.append("**Conclusion**: the superset hypothesis holds for the leader(s).\n")
    else:
        lines.append("**Conclusion**: the superset hypothesis is violated — see above.\n")

    lines.append("## Per-FD2-function detail\n")
    lines.append("Address | current name | body | 9.5 | 9.5a | 9.5b | 9.5c | matched lib symbol")
    lines.append("--- | --- | ---: | :---: | :---: | :---: | :---: | ---")
    for addr in sorted(by_addr.keys()):
        rec = by_addr[addr]
        # Pick a representative matched_name (first across versions)
        sym = ""
        score = ""
        for v in VERSIONS:
            if v in rec["versions"] and rec["versions"][v]:
                top = rec["versions"][v][0]
                sym = f"`{top[0]}` (score {top[1]})"
                break
        marks = []
        for v in VERSIONS:
            marks.append("✓" if v in rec["versions"] else " ")
        lines.append(f"`{addr}` | `{rec['current_name']}` | {rec['body_size']} | "
                     f"{marks[0]} | {marks[1]} | {marks[2]} | {marks[3]} | {sym}")
    lines.append("")

    lines.append("## Match candidate detail per version\n")
    for v in VERSIONS:
        lines.append(f"### {v}\n")
        for m in by_ver[v]["matches"]:
            lines.append(f"- `{m['address']}` `{m['current_name']}` (body {m['body_size']}):")
            for c in m["candidates"]:
                src = (c.get("source_obj") or "").replace("/watcom_libs/", "")
                lines.append(f"   - `{c.get('matched_name','')}` score={c.get('score')} src=`{src}`")
        lines.append("")

    args.out.parent.mkdir(parents=True, exist_ok=True)
    args.out.write_text("\n".join(lines), encoding="utf-8")
    print(f"Wrote {args.out}")
    print(f"Per-version count: {counts}")
    print(f"Leader: {leaders}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
