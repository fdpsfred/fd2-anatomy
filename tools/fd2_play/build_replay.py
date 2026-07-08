#!/usr/bin/env python3
"""build_replay.py -- build the FD2 replay/capture EXE (FD2RP.EXE).

Same toolchain + ABI flags as the production build (tools/fd2_build/build_fd2.py)
so the game codegen under test is identical -- the ONLY differences are:
  - adds -DFD2_REPLAY, which turns on the gated hooks in src/ (life/main.c,
    input/input.c, anim/aniend.c, save/save.c) and pulls in the guest harness.
  - also compiles + links tests/play/*.c (replay.c, capture.c), the deterministic
    input driver + checkpoint dumper. These never enter the production EXE.
  - keeps the real `main` entry (no -Dmain): the replay EXE has no testmain.c, so
    src/life/main.c is the C entry just like production.

Output: workspace/fd2_play/exe/out/FD2RP.EXE (+ obj/*.obj, build.out).
Isolated dir -- never touches tests/OUT or the production FD2.EXE.

Usage: python tools/fd2_play/build_replay.py [--timeout 300]
Exit : 0 iff FD2RP.EXE produced with 0 undefined symbols, else 1.
"""
import argparse
import io
import re
import subprocess
import sys
import time
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
SRC = ROOT / "src"
PLAY = ROOT / "tests" / "play"
LIBS = ROOT / "libs"
WATCOM = Path(r"C:\Users\fdpsf\Documents\WATCOM_9.5a")
EXE_DIR = ROOT / "workspace" / "fd2_play" / "exe"      # isolated; NOT tests/OUT
OUT = EXE_DIR / "out"
OBJ = OUT / "obj"
DOSBOX = "dosbox-x"

# ABI-identical to production (build_fd2.py CF) PLUS -DFD2_REPLAY and the extra
# include dirs: -i=include is C:\include = src/include; -i=G: is tests/play (for
# playharn.h); -i=F:\ailv3 is the vendor header.
# Delivered via the WCC386 env var (the Watcom-native default-options channel),
# NOT expanded inline with %CF% in build.bat: COMMAND.COM truncates batch lines
# past ~176 chars after %VAR% expansion, silently mangling the trailing -fo=
# object path (observed: lifemain->MAIN.OBJ etc., then wlink file-not-found).
# FD2_ASM_PRIMITIVES not defined, matching build_fd2.py: the three hand-asm
# primitives compile as their portable-C reference branches (asm bodies stay
# in-source behind the ifdef; re-enable in BOTH scripts together).
CF = r"-bt=dos4g -fp5 -fpi87 -3s -ms -zp4 -DFD2_REPLAY -i=include -i=G: -i=F:\ailv3"

AIL_LIB = r"F:\ailv3\ailv3.lib"
CRT_LIBS = [r"D:\LIB386\DOS\CLIB3S.LIB",
            r"D:\LIB386\MATH387S.LIB",
            r"D:\LIB386\DOS\EMU387.LIB"]

# main first (determines module name). main.c -> lifemain (avoids `main` obj stem
# clash with the C entry symbol), matching build_fd2.py.
OBJ_OVERRIDE = {"life/main.c": "lifemain"}
MAIN_OBJ = "lifemain"
EXE_NAME = "FD2RP.EXE"


def _obj_for(stem, reserved):
    base = stem.replace("_", "")[:8]
    obj, i = base, 0
    while obj in reserved or not obj:
        i += 1
        obj = base[:8 - len(str(i))] + str(i)
    return obj


def src_obj_list():
    """[(drive_rel, obj)] for every src/*.c then every tests/play/*.c.
    drive_rel is the path as the compile line will reference it: src files are
    'life\\main.c' (cwd = C: = src); play files are 'G:\\replay.c'."""
    items, reserved = [], set()
    for p in sorted(SRC.rglob("*.c")):
        rel = p.relative_to(SRC).as_posix()
        if rel in OBJ_OVERRIDE:
            obj = OBJ_OVERRIDE[rel]
        else:
            obj = _obj_for(rel.rsplit("/", 1)[-1][:-2], reserved)
        if obj in reserved:
            raise SystemExit("obj stem collision: %s -> %s" % (rel, obj))
        reserved.add(obj)
        items.append((rel.replace("/", "\\"), obj, False))
    for p in sorted(PLAY.glob("*.c")):
        obj = _obj_for(p.stem, reserved)
        if obj in reserved:
            raise SystemExit("play obj stem collision: %s -> %s" % (p.name, obj))
        reserved.add(obj)
        items.append(("G:\\" + p.name, obj, True))
    return items


def gen_lnk(items):
    ordered = sorted(items, key=lambda t: t[1] != MAIN_OBJ)   # main first
    L = ["system dos4g", r"name E:\out\%s" % EXE_NAME]
    L += [r"file E:\out\obj\%s.obj" % obj for _, obj, _ in ordered]
    L.append("library %s" % AIL_LIB)
    L += ["library %s" % lib for lib in CRT_LIBS]
    return "\n".join(L) + "\n"


def gen_build_bat(items):
    ordered = sorted(items, key=lambda t: t[1] != MAIN_OBJ)
    L = [r"echo === compile (FD2RP.EXE, replay) === > E:\out\build.out"]
    for path, obj, _is_play in ordered:
        L.append(r"D:\BIN\WCC386.EXE %s -fo=E:\out\obj\%s.obj >> E:\out\build.out"
                 % (path, obj))
    L.append(r"echo === link === >> E:\out\build.out")
    L.append(r"D:\BIN\WLINK.EXE @E:\fd2rp.lnk >> E:\out\build.out")
    L.append(r"echo done > E:\out\build.done")
    L.append("exit")
    return "\n".join(L) + "\n"


def gen_conf():
    lines = [
        "[sdl]", "output=surface",
        "[cpu]", "core=auto", "cputype=pentium_mmx", "cycles=max",
        "[autoexec]",
        'mount C "%s"' % str(SRC),
        'mount E "%s"' % str(EXE_DIR),
        'mount D "%s"' % str(WATCOM),
        'mount F "%s"' % str(LIBS),
        'mount G "%s"' % str(PLAY),
        "C:",
        r"if not exist E:\out md E:\out",
        r"if not exist E:\out\obj md E:\out\obj",
        "set WATCOM=D:\\",
        "set PATH=Z:\\;D:\\BIN;D:\\BINB",
        "set INCLUDE=D:\\H",
        "set WCC386=" + CF,
        r"E:\build.bat",
    ]
    return "\n".join(lines) + "\n"


def parse_undefined(txt):
    syms = set()
    for line in txt.splitlines():
        m = re.search(r"undefined symbol\s+(\S+)", line) \
            or re.search(r"(\S+)\s+is an undefined reference", line)
        if m:
            syms.add(m.group(1).strip())
    return sorted(syms)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--timeout", type=int, default=300)
    args = ap.parse_args()

    if not (LIBS / "ailv3" / "ailv3.lib").is_file():
        raise SystemExit("missing libs/ailv3/ailv3.lib")
    if not PLAY.is_dir():
        raise SystemExit("missing tests/play guest harness dir")
    OBJ.mkdir(parents=True, exist_ok=True)
    items = src_obj_list()

    io.open(EXE_DIR / "fd2rp.lnk", "w", encoding="latin-1", newline="\n").write(gen_lnk(items))
    io.open(EXE_DIR / "build.bat", "w", encoding="latin-1", newline="\r\n").write(gen_build_bat(items))
    conf = EXE_DIR / "fd2rp.conf"
    io.open(conf, "w", encoding="latin-1", newline="\n").write(gen_conf())

    exe, done, bout = OUT / EXE_NAME, OUT / "build.done", OUT / "build.out"
    for f in (exe, done, bout):
        if f.exists():
            f.unlink()
    if OBJ.is_dir():
        for p in OBJ.iterdir():
            if p.is_file() and p.suffix.lower() == ".obj":
                p.unlink()

    n_play = sum(1 for _, _, is_play in items if is_play)
    print("[build_replay] compiling %d src + %d play .obj -> %s"
          % (len(items) - n_play, n_play, OUT))
    proc = subprocess.Popen([DOSBOX, "-silent", "-conf", str(conf)])
    t0 = time.time()
    while time.time() - t0 < args.timeout:
        if done.exists() or proc.poll() is not None:
            break
        time.sleep(1)
    time.sleep(1)
    if proc.poll() is None:
        try:
            proc.terminate()
        except Exception:
            pass

    txt = bout.read_text(encoding="latin-1", errors="replace") if bout.exists() else ""
    syms = parse_undefined(txt)
    errs = [l.strip() for l in txt.splitlines() if "Error!" in l]
    exe_ok = exe.exists()
    print("\n===== wlink / build tail =====")
    print("\n".join(txt.splitlines()[-15:]) if txt else "(no build.out)")
    print("\n===== %s: %s ====="
          % (EXE_NAME, "OK (%d bytes)" % exe.stat().st_size if exe_ok else "NOT PRODUCED"))
    if errs:
        print("===== %d compile errors =====" % len(errs))
        for e in errs[:20]:
            print("  ", e)
    print("===== %d undefined symbols =====" % len(syms))
    for s in syms[:40]:
        print("  ", s)
    return 0 if (exe_ok and not syms and not errs) else 1


if __name__ == "__main__":
    sys.exit(main())
