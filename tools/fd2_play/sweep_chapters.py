#!/usr/bin/env python3
"""sweep_chapters.py -- P2 chapter-init smoke sweep over all 30 chapters.

For each chapter_id 0..29: synthesize a CONTINUE save at that chapter
(gen_scenario), quick-load it, capture one checkpoint, and report whether it
loaded the right chapter without a DOSBox fault/hang. This proves every chapter's
FDFIELD/FDSHAP loader + init path runs headless -- the P2 acceptance question
("does any chapter's loader trip a wild access"). It does NOT bless per-chapter
goldens (party is the stock roster, not the correct per-chapter one); that is the
play-through mother-chain's job. A faulting chapter is a real bug to root-cause.

Usage: python tools/fd2_play/sweep_chapters.py [--chapters 0-29] [--timeout 150]
"""
import argparse
import json
import struct
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
SCEN = ROOT / "tests" / "play" / "scenarios" / "_sweep.json"
RUN_BASE = ROOT / "workspace" / "fd2_play" / "run"
PY = sys.executable

CONTINUE_SCRIPT = ["KEY 1C 0D", "KEY 1C 0D", "KEY 1C 0D",
                   "KEY 50", "KEY 50", "KEY 1C 0D", "CAP", "END"]


def parse_range(s):
    out = []
    for part in s.split(","):
        if "-" in part:
            a, b = part.split("-")
            out += list(range(int(a), int(b) + 1))
        else:
            out.append(int(part))
    return out


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--chapters", default="0-29")
    ap.add_argument("--timeout", type=int, default=150)
    args = ap.parse_args()

    rows = []
    for n in parse_range(args.chapters):
        SCEN.write_text(json.dumps({"jump_chapter": n, "script": CONTINUE_SCRIPT}),
                        encoding="utf-8")
        out = "sweep_%02d" % n
        rc, so = 1, ""
        p = subprocess.run([PY, str(ROOT / "tools" / "fd2_play" / "run_play.py"),
                            "--scenario", "_sweep", "--out", out,
                            "--timeout", str(args.timeout)],
                           capture_output=True, text=True)
        try:
            rj = json.loads(p.stdout)
            mode, fault = rj.get("failure_mode"), rj.get("dosbox_fault")
        except Exception:
            mode, fault = "no-json", None
        # read the loaded chapter_id from the last ST dump
        got = None
        rd = RUN_BASE / out
        sts = sorted(rd.glob("ST*.BIN"))
        if sts:
            b = sts[-1].read_bytes()
            if len(b) >= 4:
                got = struct.unpack("<i", b[:4])[0]
        ok = (mode == "completed" and got == n and not fault)
        rows.append((n, ok, mode, got, fault))
        print("  ch %2d  %-4s  mode=%-9s loaded_ch=%s%s"
              % (n, "OK" if ok else "BAD", mode, got,
                 (" FAULT" if fault else "")))

    if SCEN.exists():
        SCEN.unlink()
    nok = sum(1 for r in rows if r[1])
    print("\n[sweep] %d/%d chapters loaded clean" % (nok, len(rows)))
    bad = [r[0] for r in rows if not r[1]]
    if bad:
        print("[sweep] needs root-cause: chapters %s" % bad)
    return 0 if nok == len(rows) else 1


if __name__ == "__main__":
    sys.exit(main())
