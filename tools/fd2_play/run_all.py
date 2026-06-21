#!/usr/bin/env python3
"""run_all.py -- run the FD2 playthrough regression suite.

Runs every scenario that has a blessed golden (tests/play/golden/<name>/),
comparing each run's checkpoint dumps against its golden, and prints a one-line
verdict per scenario plus a summary. Scenarios without a golden (exploration /
discrimination probes) are skipped. This is the regression entry point that
replaces the old per-function unit suite for integration coverage.

Assumes FD2RP.EXE is already built (run build_replay.py first; --build does it).

Usage:
  python tools/fd2_play/build_replay.py        # once, after src/ or harness edits
  python tools/fd2_play/run_all.py             # run the whole blessed suite
  python tools/fd2_play/run_all.py --build     # rebuild first, then run
  python tools/fd2_play/run_all.py --only ch01 # filter by substring
Exit: 0 if every scenario passes, 1 otherwise.
"""
import argparse
import json
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
PLAY = ROOT / "tests" / "play"
SCEN_DIR = PLAY / "scenarios"
GOLDEN_DIR = PLAY / "golden"
PY = sys.executable


def blessed_scenarios():
    out = []
    for sp in sorted(SCEN_DIR.glob("*.json")):
        name = sp.stem
        if (GOLDEN_DIR / name).is_dir():
            out.append(name)
    return out


def run(cmd):
    p = subprocess.run([PY] + cmd, capture_output=True, text=True)
    return p.returncode, p.stdout, p.stderr


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--only", default="")
    ap.add_argument("--build", action="store_true")
    ap.add_argument("--timeout", type=int, default=180)
    args = ap.parse_args()

    base = str(ROOT / "tools" / "fd2_play")
    if args.build:
        print("[run_all] building FD2RP.EXE ...")
        rc, so, se = run([base + "/build_replay.py", "--timeout", "300"])
        if rc != 0:
            print(so[-800:])
            print("[run_all] BUILD FAILED")
            return 1

    names = [n for n in blessed_scenarios() if args.only in n]
    if not names:
        print("[run_all] no blessed scenarios match")
        return 1

    results = []
    for n in names:
        rrc, rso, _ = run([base + "/run_play.py", "--scenario", n,
                           "--out", n, "--timeout", str(args.timeout)])
        try:
            rjson = json.loads(rso)
            mode = rjson.get("failure_mode")
        except Exception:
            mode = "no-json"
        if rrc != 0 or mode != "completed":
            results.append((n, "RUN-FAIL", mode))
            print("  %-16s RUN-FAIL (%s)" % (n, mode))
            continue
        crc, cso, _ = run([base + "/compare.py", "--scenario", n, "--out", n])
        ok = (crc == 0)
        extra = ""
        if crc != 0:
            try:
                cj = json.loads(cso)
                bad = [c["file"] for c in cj.get("checks", []) if not c["pass"]]
                extra = " <- " + ",".join(bad)
            except Exception:
                pass
        # Scenarios with an "oracle" block also get a formula-predicted
        # assertion (expect.py): the golden proves the capture is byte-stable,
        # the oracle proves the captured numbers match the game's formula.
        scen = json.loads((SCEN_DIR / (n + ".json")).read_text("utf-8"))
        if scen.get("oracle"):
            erc, eso, _ = run([base + "/expect.py", "--scenario", n, "--out", n])
            if erc != 0:
                ok = False
                try:
                    ej = json.loads(eso)
                    extra += " <- oracle pred=%s act=%s" % (
                        ej.get("predicted_damage"), ej.get("actual_damage"))
                except Exception:
                    extra += " <- oracle FAIL"
            else:
                extra += " [oracle ok]"
        verdict = "PASS" if ok else "DIFF"
        results.append((n, verdict, mode))
        print("  %-16s %s%s" % (n, verdict, extra))

    npass = sum(1 for _, v, _ in results if v == "PASS")
    print("\n[run_all] %d/%d passed" % (npass, len(results)))
    return 0 if npass == len(results) else 1


if __name__ == "__main__":
    sys.exit(main())
