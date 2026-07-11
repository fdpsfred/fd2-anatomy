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
Windows DOSBox-X + Watcom toolchain. tests/dosbox.conf is a TEMPLATE: each run
regenerates workspace/emit_drive/run.conf from it, rewriting the C:/E: mounts to
THIS checkout's REPO_ROOT so a git worktree builds its own src/tests tree.

Usage:
    python tools/code_emit/build_test.py [--changed "src/gfx/blit.c,tests/testgfx.c"]
                                    [--timeout 300] [--poll 5]
Exit code: 0 if the run completed (DONE.TXT seen), 2 on timeout. The pass/fail
verdict is in the JSON (gate_pass / build_ok), not the exit code.
"""
import argparse
import json
import os
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
RUN_CONF = DRIVE_DIR / "run.conf"  # per-worktree dosbox conf, regenerated each run


def _resolve_game_dir():
    """fd2_game_files is gitignored, so a fresh git worktree won't contain it.
    Resolve order: $FD2_GAME_DIR -> this checkout's fd2_game_files -> the main
    checkout (absolute; the one place the gitignored game files actually live)."""
    env = os.environ.get("FD2_GAME_DIR")
    if env and Path(env).is_dir():
        return Path(env)
    local = REPO_ROOT / "fd2_game_files"
    if local.is_dir():
        return local
    return Path.home() / "Documents" / "fd2-anatomy" / "fd2_game_files"


GAME_DIR = _resolve_game_dir()


def _resolve_watcom():
    """Watcom install (external to the repo). Honour %WATCOM%; else the default
    install under the user's home dir. gen_run_conf mounts this as D:."""
    env = os.environ.get("WATCOM")
    if env and Path(env).is_dir():
        return Path(env)
    return Path.home() / "Documents" / "WATCOM_9.5a"


WATCOM = _resolve_watcom()


# Real game files staged into tests/OUT (= TEST.EXE's cwd) so the resource
# loaders' bare-name fopen() reads the genuine bytes. Per project owner: check
# presence and copy from fd2_game_files/ only when missing/stale — no DOSBox
# mount. tests/OUT is gitignored, so src/ stays clean.
GAME_FILES = ["FDICON.B24", "FDFIELD.DAT", "FDSHAP.DAT", "FDOTHER.DAT",
              "FDTXT.DAT", "FDMUS.DAT", "DATO.DAT", "FD2.SAV",
              "FIGANI.DAT", "BG.DAT", "TAI.DAT"]


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


def gen_run_conf():
    """Generate a per-worktree dosbox conf from the committed template, rewriting
    ONLY the repo-relative mounts (C:=src, E:=tests) to THIS checkout's REPO_ROOT
    so a git worktree builds its own tree, not the main checkout. The Watcom mount
    (D:) and the entire [autoexec] tail are copied verbatim from the template.

    A [log] section is injected just before [autoexec] (which must stay last, as it
    consumes every following line as a guest command) so DOSBox-X writes its log to
    a host file we can parse. A test that drives a real cinematic over an untamed
    sprite source can trip an "Illegal descriptor" / GP fault that DOSBox-X surfaces
    as a modal dialog -> the process blocks -> the run is flagged as a hang; the log
    lets us see the fault was real (and where), independent of -silent."""
    src_mount = str(REPO_ROOT / "src")
    tests_mount = str(REPO_ROOT / "tests")
    libs_mount = str(REPO_ROOT / "libs")
    log_path = str(OUT_DIR / "dosbox.log")
    out = []
    for ln in CONF.read_text(encoding="latin-1").splitlines():
        s = ln.strip().lower()
        if s.startswith("[autoexec]"):
            out.append("[log]")
            out.append("logfile=%s" % log_path)
            out.append("")
            out.append(ln)
        elif s.startswith("mount c "):
            out.append('mount C "%s"' % src_mount)
        elif s.startswith("mount e "):
            out.append('mount E "%s"' % tests_mount)
        elif s.startswith("mount f "):
            out.append('mount F "%s"' % libs_mount)
        elif s.startswith("mount d "):
            out.append('mount D "%s"' % str(WATCOM))
        else:
            out.append(ln)
    RUN_CONF.write_text("\n".join(out) + "\n", encoding="latin-1")
    return RUN_CONF


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--changed", default="",
                    help="comma-separated changed paths (recorded; v1 ignores for compile)")
    ap.add_argument("--timeout", type=int, default=300, help="backstop max seconds (compile-phase hang only; run-phase hang fires far sooner)")
    ap.add_argument("--poll", type=int, default=2, help="poll interval seconds")
    ap.add_argument("--hang-stall", type=int, default=20,
                    help="seconds with no test heartbeat change (run phase) before declaring a hang")
    ap.add_argument("--only", default="",
                    help="run only the test suite(s) whose runner name or source path "
                         "contains this substring (e.g. 'anicine1' or 'gfx/rndstat'); "
                         "still compiles everything, but TEST.EXE runs only the matched "
                         "runner(s) so an earlier suite's hang can't block verification. "
                         "tests/testmain.c is filtered for the run and restored after. "
                         "Does NOT modify src/.")
    args = ap.parse_args()

    if not CONF.is_file():
        raise SystemExit("dosbox.conf (template) not found: %s" % CONF)
    DRIVE_DIR.mkdir(parents=True, exist_ok=True)
    OUT_DIR.mkdir(parents=True, exist_ok=True)
    OBJ_DIR.mkdir(parents=True, exist_ok=True)

    # --only: temporarily rewrite tests/testmain.c's GENBUILD calls block to invoke
    # only the matched suite runner(s), so the run completes for that suite instead
    # of stalling on an earlier suite's hang. The full file (all runners) is restored
    # in every exit path below. The build still compiles all of src/ + tests/; only
    # which runners main() calls changes. src/ is never touched.
    testmain_backup = None
    testmain_path = None
    only_runners = []
    if args.only:
        sys.path.insert(0, str(TESTS_DIR))
        import genbuild as _gb
        only_runners = [s["run"] for s in _gb.suite_files()
                        if args.only in s["run"] or args.only in s["rel"]]
        if not only_runners:
            raise SystemExit("--only '%s' matched no test suite runner" % args.only)
        testmain_path = _gb.TESTMAIN
        testmain_backup = testmain_path.read_text(encoding="utf-8")
        _calls = "\n".join("    %s();" % r for r in only_runners)
        _filtered = _gb._replace_between(testmain_backup, _gb.CALL_START,
                                         _gb.CALL_END, _calls)
        # NB: the filtered file is WRITTEN later (just before launch), after the host
        # prep that can fail safely with testmain.c still in its full committed form.

    dosbox = resolve_dosbox()
    run_conf = gen_run_conf()   # mounts point at THIS checkout (worktree-safe)

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

    # NOW write the --only-filtered testmain.c: every earlier step that could fail
    # (resolve_dosbox / gen_run_conf / OUT clean / stage_game_files) has run with the
    # file in its full committed form, so a failure there leaves testmain.c intact.
    # It is restored on both exit paths below. Belt-and-suspenders: if a hard
    # interrupt ever leaves it filtered, `python tests/genbuild.py --apply`
    # regenerates the full runner list from the marker block.
    if testmain_backup is not None:
        testmain_path.write_text(_filtered, encoding="utf-8")

    # launch DOSBox-X (non-blocking); build.bat ends with `exit` so DOSBox closes
    # ONLY when the batch completes. Empirically, a normal run
    # AND a hard DOS/4GW crash (e.g. NULL call -> GP fault) both return to the
    # batch and DOSBox exits within ~2s; only a TRUE hang (infinite loop in the
    # test) leaves DOSBox alive forever. So:
    #   * proc exit         -> run is over (normal or crash-returned); zero wait.
    #   * heartbeat stalled  -> true hang (HB.TXT frozen on the hung test's name).
    # A host-side test.out-growth heartbeat is NOT usable: DOSBox caches the
    # redirected stdout until file close, so test.out stays empty mid-run.
    start = time.time()
    # Capture DOSBox-X's own stdout/stderr too (belt-and-suspenders alongside the
    # [log] logfile): whichever channel carries the protected-mode fault text, we
    # keep it on the host for post-run parsing.
    stdio_log = OUT_DIR / "dosbox_stdio.log"
    try:
        stdio_fp = open(str(stdio_log), "wb")
    except OSError:
        stdio_fp = None
    try:
        proc = subprocess.Popen([dosbox, "-silent", "-conf", str(run_conf)],
                                stdout=stdio_fp, stderr=subprocess.STDOUT)
    except OSError as e:
        if stdio_fp:
            stdio_fp.close()
        if testmain_backup is not None:
            testmain_path.write_text(testmain_backup, encoding="utf-8")
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
    if stdio_fp:
        try:
            stdio_fp.close()
        except OSError:
            pass

    # restore the full-runner testmain.c (compile is long finished by now). Done on
    # every path: normal here, launch-failure above. Keeps --only side-effect-free.
    if testmain_backup is not None:
        testmain_path.write_text(testmain_backup, encoding="utf-8")

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

    # scan the DOSBox-X host log (logfile=) + captured stdio for a protected-mode
    # fault. A test that drives a real cinematic over an untamed sprite source can
    # trip an "Illegal descriptor" / GP fault that DOSBox-X surfaces as a modal
    # dialog; the dialog blocks the process, so the run is flagged as a hang while
    # the real cause is a fault. Surfacing the fault line (and any guest address)
    # tells the caller a "hang" is really a fault.
    dosbox_fault = None
    fault_rx = re.compile(
        r"illegal descriptor|general protection|invalid opcode|"
        r"privileged instruction|paging fault|cpu exception|fatal", re.I)
    for logname in ("dosbox.log", "dosbox_stdio.log"):
        lp = OUT_DIR / logname
        if not lp.is_file():
            continue
        try:
            llines = lp.read_text(encoding="latin-1", errors="replace").splitlines()
        except OSError:
            continue
        hits = [l.strip() for l in llines if l.strip() and fault_rx.search(l)]
        if hits:
            dosbox_fault = "\n".join(hits[-6:])
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
        "only_runners": only_runners,
        "error_count": len(errors),
        "warning_count": len(warnings),
        "errors": errors,
        "warnings": warnings,
        "tests_passed": tests_passed,
        "tests_failed": tests_failed,
        "crash_dump": crash_dump,
        "dosbox_fault": dosbox_fault,
        "test_out_tail": test_tail,
        "build_out_tail": build_tail,
    }
    out_json = json.dumps(result, indent=2, ensure_ascii=False)
    (DRIVE_DIR / "last_build.json").write_text(out_json, encoding="utf-8")
    print(out_json)
    return 0 if done else 2


if __name__ == "__main__":
    sys.exit(main())
