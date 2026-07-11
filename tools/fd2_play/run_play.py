#!/usr/bin/env python3
"""run_play.py -- run one FD2 replay scenario and collect its checkpoint dumps.

Builds nothing; uses workspace/fd2_play/exe/out/FD2RP.EXE (from build_replay.py).
For a scenario it: stages a self-contained run dir (game files + DOS4GW.EXE +
FD2RP.EXE + SCRIPT.TXT + optional starting FD2.SAV), launches DOSBox-X -silent,
and waits with the same three end-of-run signals as the test gate
(tools/code_emit/build_test.py): DONE.TXT appears | DOSBox exits | heartbeat
(HB.TXT) stalls. It then collects FBnn.BIN / STnn.BIN dumps and writes run.json.

A scenario is tests/play/scenarios/<name>.json:
  {
    "script": ["CAP", "KEY 50", "CAP", "END"],   # SCRIPT.TXT lines (see replay.c)
    "sav":    "optional/path/to/starting.sav"     # else the stock FD2.SAV is used
  }

Usage: python tools/fd2_play/run_play.py --scenario boot [--timeout 120]
Exit : 0 if DONE.TXT seen, 2 otherwise. Pass/fail vs golden is compare.py's job.
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

ROOT = Path(__file__).resolve().parents[2]
PLAY = ROOT / "tests" / "play"
SCEN_DIR = PLAY / "scenarios"
EXE = ROOT / "workspace" / "fd2_play" / "exe" / "out" / "FD2RP.EXE"
RUN_BASE = ROOT / "workspace" / "fd2_play" / "run"

# Full game environment copied into each run dir so FD2RP.EXE's bare-name
# fopen()s resolve and the DOS/4GW stub finds its extender. The run dir is the
# EXE cwd; dumps (FBnn/STnn/DONE/HB) are written there.
DOS4GW = "DOS4GW.EXE"


def resolve_game_dir():
    env = os.environ.get("FD2_GAME_DIR")
    if env and Path(env).is_dir():
        return Path(env)
    local = ROOT / "fd2_game_files"
    if local.is_dir():
        return local
    return Path.home() / "Documents" / "fd2-anatomy" / "fd2_game_files"


GAME_DIR = resolve_game_dir()


def resolve_dosbox():
    for cand in ("dosbox-x", "dosbox-x.exe"):
        hit = shutil.which(cand)
        if hit:
            return hit
    fallback = Path(r"C:\DOSBox-X\dosbox-x.exe")
    if fallback.exists():
        return str(fallback)
    raise SystemExit("dosbox-x not found on PATH or C:\\DOSBox-X\\")


def stage(run_dir, scenario):
    """Copy game env + EXE into run_dir (size-checked), write SCRIPT.TXT, and
    overlay a scenario-specific starting FD2.SAV if given. Returns the staged
    game-file count."""
    run_dir.mkdir(parents=True, exist_ok=True)
    staged = 0
    # copy every game file (data + audio drivers + DOS4GW.EXE + stock FD2.SAV).
    # FD2.SAV is game-WRITABLE, so size-check would let a stale save survive in a
    # reused run dir -> always overwrite it (and the scenario may override below).
    # Big read-only DATs keep the size-check.
    for src in GAME_DIR.iterdir():
        if not src.is_file():
            continue
        dst = run_dir / src.name
        if dst.is_file() and dst.stat().st_size == src.stat().st_size \
                and src.name.upper() != "FD2.SAV":
            continue
        shutil.copyfile(src, dst)
        staged += 1
    # the replay EXE
    shutil.copyfile(EXE, run_dir / EXE.name)
    # scenario-specific starting save overrides the stock one
    sav = scenario.get("sav")
    if sav:
        sp = Path(sav)
        if not sp.is_absolute():
            sp = ROOT / sav
        shutil.copyfile(sp, run_dir / "FD2.SAV")
    # jump_chapter: synthesize a CONTINUE-loadable save at chapter N from the
    # stock save (gen_scenario ports the game's crypt+checksum). No sav file to
    # version -- regenerated deterministically each run.
    jc = scenario.get("jump_chapter")
    if jc is not None:
        import gen_scenario as _gs
        stock = _gs.resolve_stock_sav().read_bytes()
        (run_dir / "FD2.SAV").write_bytes(_gs.make_jump(stock, int(jc)))
    # the input script
    lines = scenario.get("script", [])
    (run_dir / "SCRIPT.TXT").write_text("\n".join(lines) + "\n", encoding="latin-1")
    return staged


def clean_outputs(run_dir):
    """Remove this-scenario artifacts AND game-generated scratch so every run
    starts from a pristine, identical dir (else a leftover FD2.TMP / AUDDBG.TXT
    from a prior run perturbs the next run -> non-determinism across reused dirs).
    Staged game files + EXE stay (re-staged by stage())."""
    scratch = ("DONE.TXT", "HB.TXT", "DOSBOX.LOG", "DBSTDIO.LOG",
               "FD2.TMP", "AUDDBG.TXT")
    for p in run_dir.iterdir():
        if not p.is_file():
            continue
        nm = p.name.upper()
        if nm in scratch \
                or (nm.startswith("FB") and nm.endswith(".BIN")) \
                or (nm.startswith("ST") and nm.endswith(".BIN")) \
                or (nm.startswith("PAL") and nm.endswith(".BIN")) \
                or (nm.startswith("PNG") and nm.endswith(".PNG")):
            try:
                p.unlink()
            except OSError:
                pass


def gen_conf(run_dir, log_path):
    lines = [
        "[sdl]", "output=surface",
        "[cpu]", "core=auto", "cputype=pentium_mmx", "cycles=max",
        "[log]", "logfile=%s" % str(log_path),
        "[autoexec]",
        'mount C "%s"' % str(run_dir),
        "C:",
        "FD2RP.EXE",
        "exit",
    ]
    return "\n".join(lines) + "\n"


def find_ci(run_dir, name):
    low = name.lower()
    for p in run_dir.iterdir():
        if p.is_file() and p.name.lower() == low:
            return p
    return None


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--scenario", required=True)
    ap.add_argument("--out", default="",
                    help="run dir name under workspace/fd2_play/run/ (default = "
                         "scenario name); use distinct names to keep two runs for "
                         "a determinism diff")
    ap.add_argument("--timeout", type=int, default=120,
                    help="backstop seconds (compile-phase style hang)")
    ap.add_argument("--poll", type=int, default=2)
    ap.add_argument("--hang-stall", type=int, default=20,
                    help="seconds with no HB.TXT change before declaring a hang")
    args = ap.parse_args()

    if not EXE.is_file():
        raise SystemExit("FD2RP.EXE missing -- run build_replay.py first: %s" % EXE)
    scen_path = SCEN_DIR / (args.scenario + ".json")
    if not scen_path.is_file():
        raise SystemExit("scenario not found: %s" % scen_path)
    scenario = json.loads(scen_path.read_text(encoding="utf-8"))

    run_dir = RUN_BASE / (args.out or args.scenario)
    staged = stage(run_dir, scenario)
    clean_outputs(run_dir)

    dosbox = resolve_dosbox()
    log_path = run_dir / "dosbox.log"
    conf = run_dir / "run.conf"
    conf.write_text(gen_conf(run_dir, log_path), encoding="latin-1")

    stdio_log = run_dir / "dbstdio.log"
    try:
        stdio_fp = open(str(stdio_log), "wb")
    except OSError:
        stdio_fp = None
    proc = subprocess.Popen([dosbox, "-silent", "-conf", str(conf)],
                            stdout=stdio_fp, stderr=subprocess.STDOUT)

    start = time.time()
    done = hang = False
    hung = None
    last_hb = last_hb_change = None
    deadline = start + args.timeout
    while time.time() < deadline:
        if find_ci(run_dir, "done.txt"):
            done = True
            break
        if proc.poll() is not None:
            break
        hb = find_ci(run_dir, "hb.txt")
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
                hung = (last_hb or "").strip()
                break
        time.sleep(args.poll)
    if find_ci(run_dir, "done.txt"):
        done = True
    elapsed = int(time.time() - start)

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

    # protected-mode fault scan (same patterns as build_test.py)
    fault = None
    frx = re.compile(r"illegal descriptor|general protection|invalid opcode|"
                     r"privileged instruction|paging fault|cpu exception|fatal", re.I)
    for nm in ("dosbox.log", "dbstdio.log"):
        lp = run_dir / nm
        if not lp.is_file():
            continue
        try:
            hits = [l.strip() for l in lp.read_text(encoding="latin-1", errors="replace").splitlines()
                    if l.strip() and frx.search(l)]
        except OSError:
            hits = []
        if hits:
            fault = "\n".join(hits[-6:])
            break

    dumps = sorted(p.name for p in run_dir.iterdir()
                   if p.is_file() and re.match(r"(FB|ST)\d\d\.BIN", p.name.upper()))
    mode = ("completed" if done else "hang" if hang
            else "fault" if fault else "aborted" if exited else "timeout")
    result = {
        "scenario": args.scenario,
        "done": done,
        "failure_mode": mode,
        "hung_step": hung,
        "dosbox_fault": fault,
        "staged_game_files": staged,
        "dumps": dumps,
        "elapsed_sec": elapsed,
        "run_dir": str(run_dir),
    }
    out = json.dumps(result, indent=2, ensure_ascii=False)
    (run_dir / "run.json").write_text(out, encoding="utf-8")
    print(out)
    return 0 if done else 2


if __name__ == "__main__":
    sys.exit(main())
