#!/usr/bin/env python3
"""compare.py -- compare FD2 replay checkpoint dumps against a reference.

Surfaces the two oracle comparisons the plan calls for:
  - framebuffer (FBnn.BIN): Hamming distance = differing bytes / total. The
    mode-13h buffer holds palette INDICES, so an exact match (0 diff) is the
    expected result for a deterministic rendering checkpoint; --fb-tol allows a
    small ratio if ever needed.
  - state (STnn.BIN): exact byte-equality (key-global header + runtime_char
    array). Any diff fails.

Modes:
  (default)        compare workspace run dir vs tests/play/golden/<scenario>
  --against DIR    compare run dir vs another run dir (determinism double-run)
  --bless          copy the run dir's dumps into tests/play/golden/<scenario>

Usage:
  python tools/fd2_play/compare.py --scenario boot --bless
  python tools/fd2_play/compare.py --scenario boot
  python tools/fd2_play/compare.py --scenario boot --against <other_run_dir>
Exit: 0 if all checkpoints match (or bless done), 1 otherwise.
"""
import argparse
import json
import re
import shutil
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
PLAY = ROOT / "tests" / "play"
GOLDEN_DIR = PLAY / "golden"
RUN_BASE = ROOT / "workspace" / "fd2_play" / "run"

DUMP_RE = re.compile(r"^(FB|ST)\d\d\.BIN$", re.I)


def dump_names(d):
    if not d.is_dir():
        return []
    return sorted(p.name for p in d.iterdir()
                  if p.is_file() and DUMP_RE.match(p.name))


def hamming(a, b):
    """(differing_bytes, total) over the shared length; size mismatch counts the
    tail as fully differing."""
    n = min(len(a), len(b))
    diff = sum(1 for i in range(n) if a[i] != b[i])
    diff += abs(len(a) - len(b))
    return diff, max(len(a), len(b))


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--scenario", required=True)
    ap.add_argument("--out", default="", help="run dir name (default = scenario)")
    ap.add_argument("--against", default="",
                    help="compare vs this run dir instead of golden (determinism)")
    ap.add_argument("--bless", action="store_true",
                    help="copy run dumps into the golden dir")
    ap.add_argument("--fb-tol", type=float, default=0.0,
                    help="max framebuffer Hamming ratio still counted as a match")
    args = ap.parse_args()

    run_dir = RUN_BASE / (args.out or args.scenario)
    golden = GOLDEN_DIR / args.scenario

    if args.bless:
        golden.mkdir(parents=True, exist_ok=True)
        for nm in dump_names(run_dir):
            shutil.copyfile(run_dir / nm, golden / nm)
        names = dump_names(run_dir)
        print(json.dumps({"blessed": args.scenario, "files": names,
                          "golden": str(golden)}, indent=2, ensure_ascii=False))
        return 0

    ref = Path(args.against) if args.against else golden
    if not ref.is_dir():
        raise SystemExit("reference dir missing: %s (bless a golden first?)" % ref)

    run_names = dump_names(run_dir)
    ref_names = dump_names(ref)
    checks = []
    ok = True

    missing = sorted(set(ref_names) - set(run_names))
    extra = sorted(set(run_names) - set(ref_names))
    if missing or extra:
        ok = False

    for nm in sorted(set(run_names) & set(ref_names)):
        a = (run_dir / nm).read_bytes()
        b = (ref / nm).read_bytes()
        if nm.upper().startswith("FB"):
            diff, total = hamming(a, b)
            ratio = (diff / total) if total else 0.0
            passed = ratio <= args.fb_tol
            checks.append({"file": nm, "kind": "framebuffer",
                           "hamming_bytes": diff, "total": total,
                           "ratio": round(ratio, 6), "pass": passed})
        else:
            passed = (a == b)
            diff, total = hamming(a, b)
            checks.append({"file": nm, "kind": "state",
                           "diff_bytes": diff, "total": total, "pass": passed})
        ok = ok and passed

    result = {
        "scenario": args.scenario,
        "reference": "determinism:" + str(ref) if args.against else "golden",
        "pass": ok,
        "missing_vs_ref": missing,
        "extra_vs_ref": extra,
        "checks": checks,
    }
    print(json.dumps(result, indent=2, ensure_ascii=False))
    return 0 if ok else 1


if __name__ == "__main__":
    sys.exit(main())
