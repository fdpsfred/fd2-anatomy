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
OBJ_DIR = OUT_DIR / "obj"          # compile intermediates (.obj)
CONF = TESTS_DIR / "dosbox.conf"
DRIVE_DIR = REPO_ROOT / "workspace" / "emit_drive"
GAME_DIR = REPO_ROOT / "fd2_game_files"

# Real game files staged into tests/OUT (= TEST.EXE's cwd) so the resource
# loaders' bare-name fopen() reads the genuine bytes. Per project owner: check
# presence and copy from fd2_game_files/ only when missing/stale — no DOSBox
# mount. tests/OUT is gitignored, so src/ stays clean.
GAME_FILES = ["FDICON.B24", "FDFIELD.DAT", "FDSHAP.DAT", "FDOTHER.DAT",
              "FDTXT.DAT", "FDMUS.DAT", "DATO.DAT", "FD2.SAV"]


def stage_game_files():
    """Copy each real game file into tests/OUT iff absent or wrong size (a wrong
    size means a test left a fabricated stand-in behind — replace it)."""
    staged, missing_src = [], []
    for n in GAME_FILES:
        src = GAME_DIR / n
        dst = OUT_DIR / n
        if not src.is_file():
            missing_src.append(n)
            continue
        if dst.is_file() and dst.stat().st_size == src.stat().st_size:
            continue
        shutil.copyfile(src, dst)
        staged.append(n)
    return staged, missing_src


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
    ap.add_argument("--timeout", type=int, default=300, help="backstop max seconds (compile-phase hang only; run-phase hang fires far sooner)")
    ap.add_argument("--poll", type=int, default=2, help="poll interval seconds")
    ap.add_argument("--hang-stall", type=int, default=20,
                    help="seconds with no test heartbeat change (run phase) before declaring a hang")
    args = ap.parse_args()

    if not CONF.is_file():
        raise SystemExit("dosbox.conf not found: %s" % CONF)
    DRIVE_DIR.mkdir(parents=True, exist_ok=True)
    OUT_DIR.mkdir(parents=True, exist_ok=True)
    OBJ_DIR.mkdir(parents=True, exist_ok=True)

    dosbox = resolve_dosbox()

    # clean so the polled DONE.TXT is unambiguously from THIS run and the full
    # rebuild has no stale influence: wipe OUT/obj entirely, and remove OUT root
    # files EXCEPT the staged game files (large, kept across runs per stage policy).
    for p in OBJ_DIR.iterdir():
        if p.is_file():
            try:
                p.unlink()
            except OSError:
                pass
    keep = set(GAME_FILES)
    for p in OUT_DIR.iterdir():
        if p.is_file() and p.name not in keep:
            try:
                p.unlink()
            except OSError:
                pass

    # stage real game files into tests/OUT (TEST.EXE's cwd) before launch.
    staged_game, missing_game = stage_game_files()

    # launch DOSBox-X (non-blocking); build.bat ends with `exit` so DOSBox closes
    # ONLY when the batch completes. Empirically (tools/hangprobe): a normal run
    # AND a hard DOS/4GW crash (e.g. NULL call -> GP fault) both return to the
    # batch and DOSBox exits within ~2s; only a TRUE hang (infinite loop in the
    # test) leaves DOSBox alive forever. So:
    #   * proc exit         -> run is over (normal or crash-returned); zero wait.
    #   * heartbeat stalled  -> true hang (HB.TXT frozen on the hung test's name).
    # A host-side test.out-growth heartbeat is NOT usable: DOSBox caches the
    # redirected stdout until file close, so test.out stays empty mid-run.
    start = time.time()
    try:
        proc = subprocess.Popen([dosbox, "-silent", "-conf", str(CONF)])
    except OSError as e:
        raise SystemExit("failed to launch dosbox-x: %s" % e)

    done = False
    hang = False
    hung_test = None
    last_hb = None
    last_hb_change = None
    deadline = start + args.timeout
    while time.time() < deadline:
        if find_out("done.txt"):
            done = True
            break
        if proc.poll() is not None:   # DOSBox closed -> batch finished (normal/crash)
            break
        # run-phase hang detection: HB.TXT appears once TEST.EXE starts and is
        # rewritten (open/write/close) at every test; if it stops changing while
        # DOSBox is still alive, a test is hung.
        hb = find_out("hb.txt")
        if hb is not None:
            try:
                cur = hb.read_text(encoding="latin-1", errors="replace")
            except OSError:
                cur = last_hb
            now = time.time()
            if cur != last_hb:
                last_hb, last_hb_change = cur, now
            elif last_hb_change is not None and now - last_hb_change > args.hang_stall:
                hang = True
                hung_test = (last_hb or "").strip()
                break
        time.sleep(args.poll)
    # DOSBox may have written DONE.TXT just before exiting.
    if find_out("done.txt"):
        done = True
    elapsed = int(time.time() - start)

    # capture exit state BEFORE killing (the kill below would make poll() != None
    # unconditionally and hide a real timeout).
    exited = proc.poll() is not None
    if not exited:
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

    # parse test.out ("Results: N passed, M failed"); also detect a DOS/4GW crash
    # dump (captured because TEST.EXE's stdout is redirected to test.out).
    tests_passed = tests_failed = None
    test_tail = ""
    crash_dump = None
    to = find_out("test.out")
    if to:
        lines = to.read_text(encoding="latin-1", errors="replace").splitlines()
        test_tail = "\n".join(lines[-8:])
        rx = re.compile(r"Results:\s*(\d+)\s+passed,\s*(\d+)\s+failed")
        for ln in lines:
            m = rx.search(ln)
            if m:
                tests_passed, tests_failed = int(m.group(1)), int(m.group(2))
        for i, ln in enumerate(lines):
            if "DOS/4G" in ln or "exception" in ln:
                crash_dump = "\n".join(lines[i:i + 6])
                break

    build_ok = bool(done and not errors and tests_passed is not None and tests_failed == 0)
    gate_pass = bool(build_ok and not warnings)

    # failure_mode classifies WHY a non-passing run ended, so the caller need not
    # re-derive it: completed (done; may still have errors/test failures) | hang
    # (heartbeat stalled) | crash (DOS/4GW fault dump) | aborted (DOSBox exited
    # without DONE.TXT) | timeout (backstop hit, likely a compile-phase hang).
    clean_pass = done and not errors and tests_passed is not None and tests_failed == 0
    if clean_pass:
        failure_mode = "completed"
    elif crash_dump:
        failure_mode = "crash"     # DOS/4GW fault dump; crash returns to batch so done.txt may exist
    elif hang:
        failure_mode = "hang"
    elif done:
        failure_mode = "completed"  # ran to summary but with build errors / test failures
    elif exited:
        failure_mode = "aborted"    # DOSBox closed without DONE.TXT and no crash dump
    else:
        failure_mode = "timeout"    # backstop deadline hit, proc still alive (compile hang)

    result = {
        "gate_pass": gate_pass,
        "build_ok": build_ok,
        "done": done,
        "failure_mode": failure_mode,
        "hung_test": hung_test,
        "staged_game_files": staged_game,
        "missing_game_files": missing_game,
        "elapsed_sec": elapsed,
        "changed": args.changed,
        "error_count": len(errors),
        "warning_count": len(warnings),
        "errors": errors,
        "warnings": warnings,
        "tests_passed": tests_passed,
        "tests_failed": tests_failed,
        "crash_dump": crash_dump,
        "test_out_tail": test_tail,
        "build_out_tail": build_tail,
    }
    out_json = json.dumps(result, indent=2, ensure_ascii=False)
    (DRIVE_DIR / "last_build.json").write_text(out_json, encoding="utf-8")
    print(out_json)
    return 0 if done else 2


if __name__ == "__main__":
    sys.exit(main())
