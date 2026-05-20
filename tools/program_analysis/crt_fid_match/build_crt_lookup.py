"""Build verification queues from FD2.LE × Watcom 9.5a FidQuery raw output.

Reads `workspace/crt_fid_match/results/matches_9.5a.json` (produced by
`ghidra_scripts/FidQuery.java`; not checked into the repo) and splits the
candidate matches into 4 queues that drive the verification pipeline.

Outputs (under --out-dir, default workspace/crt_fid_match/):

  auto_candidates.json   - All entries with top score >= AUTO_THRESHOLD.
                            Slated for batch insertion into the final lookup
                            table once sample verification confirms the
                            threshold is reliable.

  sample_queue.json      - 10 entries chosen from auto_candidates by
                            equal-distance score sampling across
                            [min, max]. Drives threshold validation.

  conflict_queue.json    - Hand-flagged entries where current_name (Ghidra) and
                            matched_name (Watcom lib PUBDEF) refer to
                            semantically different operations. Verified
                            individually regardless of score.

  manual_queue.json      - All entries with top score < AUTO_THRESHOLD.
                            Each verified one-by-one with manual observation.

Each entry is normalized to:

    {
      "address":         "<8-hex>",
      "current_name":    "<Ghidra current symbol>",
      "body_size":       <int>,
      "top_score":       <float>,
      "top_matched_name":"<Watcom lib PUBDEF>",
      "top_source_obj":  "<short obj filename>",
      "candidates":      [<full candidate list, score-descending>],
    }
"""
from __future__ import annotations

import argparse
import json
from pathlib import Path


AUTO_THRESHOLD = 30.0
N_SAMPLES = 10

# Hand-identified conflict cases — current_name and matched_name diverge in
# operation kind (e.g., character output vs file delete), not just in style.
# Verified by direct inspection of the raw FidQuery output.
CONFLICT_ADDRS = {
    "00036dc1",  # current=crt_fprintf_stderr  matched=printf   (output sink differs)
    "0003dbe7",  # current=crt_putc_tty        matched=remove   (write vs delete)
    "00046a80",  # current=crt_putc_dos        matched=unlink   (write vs delete)
}


def normalize(matches: list[dict]) -> list[dict]:
    """Sort each entry's candidates by score-descending, expose top fields."""
    out: list[dict] = []
    for m in matches:
        cands = sorted(m["candidates"], key=lambda c: -c["score"])
        top = cands[0]
        out.append({
            "address": m["address"],
            "current_name": m["current_name"],
            "body_size": m["body_size"],
            "top_score": top["score"],
            "top_matched_name": top["matched_name"],
            "top_source_obj": (top.get("source_obj") or "").replace("/watcom_libs/", ""),
            "candidates": cands,
        })
    return out


def select_samples(entries: list[dict], n: int) -> list[dict]:
    """Pick n entries by equal-distance score sampling across [min, max].

    For each of n target scores, choose the still-unused entry with score
    closest to the target. Result is sorted by ascending score.
    """
    if not entries or n <= 0:
        return []
    sorted_entries = sorted(entries, key=lambda e: e["top_score"])
    smin = sorted_entries[0]["top_score"]
    smax = sorted_entries[-1]["top_score"]
    if n == 1:
        targets = [(smin + smax) / 2]
    else:
        targets = [smin + (smax - smin) * i / (n - 1) for i in range(n)]
    used: set[str] = set()
    picked: list[dict] = []
    for t in targets:
        best = None
        best_diff = None
        for e in sorted_entries:
            if e["address"] in used:
                continue
            d = abs(e["top_score"] - t)
            if best_diff is None or d < best_diff:
                best_diff = d
                best = e
        if best is not None:
            picked.append(best)
            used.add(best["address"])
    picked.sort(key=lambda e: e["top_score"])
    return picked


def main() -> int:
    ap = argparse.ArgumentParser(
        description=__doc__,
        formatter_class=argparse.RawDescriptionHelpFormatter,
    )
    ap.add_argument("--in", dest="inp", type=Path,
                    default=Path("workspace/crt_fid_match/results/matches_9.5a.json"))
    ap.add_argument("--out-dir", type=Path,
                    default=Path("workspace/crt_fid_match"))
    ap.add_argument("--threshold", type=float, default=AUTO_THRESHOLD)
    ap.add_argument("--samples", type=int, default=N_SAMPLES)
    args = ap.parse_args()

    raw = json.loads(args.inp.read_text(encoding="utf-8"))
    entries = normalize(raw["matches"])

    auto = [e for e in entries if e["top_score"] >= args.threshold]
    manual = [e for e in entries if e["top_score"] < args.threshold]
    samples = select_samples(auto, args.samples)
    conflicts = [e for e in auto if e["address"] in CONFLICT_ADDRS]

    args.out_dir.mkdir(parents=True, exist_ok=True)

    def dump(name: str, payload: dict) -> Path:
        p = args.out_dir / name
        p.write_text(json.dumps(payload, indent=2), encoding="utf-8")
        return p

    dump("auto_candidates.json", {
        "version": raw["version"],
        "threshold": args.threshold,
        "count": len(auto),
        "entries": auto,
    })
    dump("sample_queue.json", {
        "version": raw["version"],
        "purpose": "Threshold validation by equal-distance score sampling.",
        "count": len(samples),
        "entries": samples,
    })
    dump("conflict_queue.json", {
        "version": raw["version"],
        "purpose": "Name-conflict candidates (current_name vs matched_name disagree).",
        "count": len(conflicts),
        "entries": conflicts,
    })
    dump("manual_queue.json", {
        "version": raw["version"],
        "threshold": args.threshold,
        "purpose": "Per-entry behavior verification for score-below-threshold candidates.",
        "count": len(manual),
        "entries": manual,
    })

    print(f"Total input: {len(entries)}")
    print(f"  Auto candidates (score >= {args.threshold}): {len(auto)}")
    print(f"  Samples ({args.samples}): {len(samples)}")
    print(f"  Name conflicts (hand-flagged): {len(conflicts)}")
    print(f"  Manual queue (score < {args.threshold}): {len(manual)}")
    print()
    print("Sample selection (ascending score):")
    for s in samples:
        print(f"  {s['address']}  score={s['top_score']:7.2f}  "
              f"matched={s['top_matched_name']:<20s}  body={s['body_size']}")
    print()
    print("Conflict queue:")
    for c in conflicts:
        print(f"  {c['address']}  score={c['top_score']:7.2f}  "
              f"matched={c['top_matched_name']:<12s}  "
              f"current={c['current_name']}")

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
