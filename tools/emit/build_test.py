#!/usr/bin/env python3
"""
build_test.py - FD2 emit pipeline build+test gate (single source of truth).

Runs the DOSBox-X compile + link + run of src/ + tests/ and prints a structured
JSON verdict. Used by the emit workflow's Emitter/Reviewer.

IMPORTANT (per project owner): there is NO DOSBox-X stale-cache problem and NO
DPMI-OOM problem. Earlier "two-stage session / copy-delete-rename / sleep"
workarounds were misdiagnoses; the real cause was reading output before the
DOSBox-X compile+run had finished. The ONLY correct completion signal is the
appearance of tests/OUT/DONE.TXT. This script: clean -> launch -> poll DONE.TXT
until it appears (generous timeout) -> parse BUILD.OUT + TEST.OUT.

v1: always does a clean FULL rebuild (deletes tests/OUT/* first) for maximum
determinism. --changed is recorded for a future incremental mode but ignored
for compilation in v1.

Cross-platform via shutil.which + pathlib; the actual build still needs the
Windows DOSBox-X + Watcom toolchain and the mount paths inside tests/dosbox.conf.

Usage:
    python tools/emit/build_test.py [--changed "src/gfx/blit.c,tests/testgfx.c"]
                                    [--timeout 300] [--poll 5]
Exit code: 0 if the run completed (DONE.TXT seen), 2 on timeout. The pass/fail
verdict is in the JSON (gate_pass / build_ok), not the exit code.
"""
import argparse
import json
import re
import shutil
import subprocess
import sys
import time
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parents[2]
TESTS_DIR = REPO_ROOT / "tests"
OUT_DIR = TESTS_DIR / "OUT"
CONF = TESTS_DIR / "dosbox.conf"
DRIVE_DIR = REPO_ROOT / "workspace" / "emit_drive"


def find_out(name):
    """Case-insensitive lookup of a file in OUT/ (DOS 8.3 names may be upper)."""
    if not OUT_DIR.is_dir():
        return None
    low = name.lower()
    for p in OUT_DIR.iterdir():
        if p.is_file() and p.name.lower() == low:
            return p
    return None


def resolve_dosbox():
    for cand in ("dosbox-x", "dosbox-x.exe"):
        hit = shutil.which(cand)
        if hit:
            return hit
    fallback = Path(r"C:\DOSBox-X\dosbox-x.exe")
    if fallback.exists():
        return str(fallback)
    raise SystemExit("dosbox-x not found on PATH or C:\\DOSBox-X\\")


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--changed", default="",
                    help="comma-separated changed paths (recorded; v1 ignores for compile)")
    ap.add_argument("--timeout", type=int, default=300, help="max seconds to wait for DONE.TXT")
    ap.add_argument("--poll", type=int, default=5, help="poll interval seconds")
    args = ap.parse_args()

    if not CONF.is_file():
        raise SystemExit("dosbox.conf not found: %s" % CONF)
    DRIVE_DIR.mkdir(parents=True, exist_ok=True)
    OUT_DIR.mkdir(parents=True, exist_ok=True)

    dosbox = resolve_dosbox()

    # clean OUT/ so the polled DONE.TXT is unambiguously from THIS run and the
    # full rebuild has no stale .obj/.exe influence.
    for p in OUT_DIR.iterdir():
        if p.is_file():
            try:
                p.unlink()
            except OSError:
                pass

    # launch DOSBox-X (non-blocking); dosbox.conf ends with `exit` so it self-closes.
    start = time.time()
    try:
        proc = subprocess.Popen([dosbox, "-silent", "-conf", str(CONF)])
    except OSError as e:
        raise SystemExit("failed to launch dosbox-x: %s" % e)

    # poll for DONE.TXT: the ONLY correct completion signal.
    done = False
    deadline = start + args.timeout
    while time.time() < deadline:
        time.sleep(args.poll)
        if find_out("done.txt"):
            done = True
            break
    elapsed = int(time.time() - start)

    if not done and proc.poll() is None:
        try:
            proc.kill()
        except OSError:
            pass

    # parse build.out (Watcom diagnostics: "Error!" / "Warning!")
    errors, warnings, build_tail = [], [], ""
    bo = find_out("build.out")
    if bo:
        lines = bo.read_text(encoding="latin-1", errors="replace").splitlines()
        for ln in lines:
            if "Error!" in ln:
                errors.append(ln.strip())
            elif "Warning!" in ln:
                warnings.append(ln.strip())
        build_tail = "\n".join(lines[-20:])

    # parse test.out ("Results: N passed, M failed")
    tests_passed = tests_failed = None
    test_tail = ""
    to = find_out("test.out")
    if to:
        lines = to.read_text(encoding="latin-1", errors="replace").splitlines()
        test_tail = "\n".join(lines[-8:])
        rx = re.compile(r"Results:\s*(\d+)\s+passed,\s*(\d+)\s+failed")
        for ln in lines:
            m = rx.search(ln)
            if m:
                tests_passed, tests_failed = int(m.group(1)), int(m.group(2))

    build_ok = bool(done and not errors and tests_passed is not None and tests_failed == 0)
    gate_pass = bool(build_ok and not warnings)

    result = {
        "gate_pass": gate_pass,
        "build_ok": build_ok,
        "done": done,
        "elapsed_sec": elapsed,
        "changed": args.changed,
        "error_count": len(errors),
        "warning_count": len(warnings),
        "errors": errors,
        "warnings": warnings,
        "tests_passed": tests_passed,
        "tests_failed": tests_failed,
        "test_out_tail": test_tail,
        "build_out_tail": build_tail,
    }
    out_json = json.dumps(result, indent=2, ensure_ascii=False)
    (DRIVE_DIR / "last_build.json").write_text(out_json, encoding="utf-8")
    print(out_json)
    return 0 if done else 2


if __name__ == "__main__":
    sys.exit(main())
