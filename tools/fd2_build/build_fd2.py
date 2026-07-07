#!/usr/bin/env python3
"""build_fd2.py -- build the production FD2.EXE from src/ ONLY.

Self-contained: depends on NOTHING under tests/. The test build
(tools/code_emit/build_test.py) compiles src/ + tests/, links TEST.EXE, runs the
suite, and wipes tests/OUT each run. This tool instead:
  - scans src/ for *.c and compiles each (no tests/, no testmain/testglob)
  - compiles life/main.c WITHOUT -Dmain so the entry stays `main` (the Watcom
    CRT cmain386 contract). The test build aliases that one file to fd2_main to
    coexist with testmain.c's own main(); the production EXE must NOT.
  - links a src-only directive (ailv3.lib via F:, no fd2common.lib, + CRT)
  - writes everything to its OWN dir (workspace/fd2_build/exe), so a test build
    can never clobber FD2.EXE and vice versa.

Everything the build needs (compiler flags, mount paths, obj naming, the link
directive) is defined HERE, not borrowed from tests/. The flags below MUST stay
ABI-identical to the test build's FD2 settings (-3s stack-call, -ms, -zp4, 387)
so the two executables are binary-compatible -- the ONLY intentional difference
is that production drops `-i=E:\\include` (that path is tests/include, a
test-fixture override dir the production build must not see) and drops -Dmain.

Output: workspace/fd2_build/exe/out/FD2.EXE  (+ obj/*.obj, build.out; fd2.map with --map)
Usage : python tools/fd2_build/build_fd2.py [--timeout 300] [--map]
Exit  : 0 iff FD2.EXE was produced with 0 undefined symbols, else 1.
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
LIBS = ROOT / "libs"
WATCOM = Path(r"C:\Users\fdpsf\Documents\WATCOM_9.5a")
EXE_DIR = ROOT / "workspace" / "fd2_build" / "exe"   # isolated; NOT tests/OUT
OUT = EXE_DIR / "out"
OBJ = OUT / "obj"
DOSBOX = "dosbox-x"

# Compile flags -- ABI-identical to the original FD2 settings. -3s = 386
# stack-call cdecl (matches CLIB3S + the extracted AIL lib); -ms small model;
# -zp4 pack. -i=include is C:\include = src/include
# (protos.h/types.h); -i=F:\ailv3 is the vendor header.
# -fp5 -fpi87: original FD2 FP model (inline hardware 387 x87). WITHOUT it,
# wcc386 defaults to -fpi (emulator-aware FP), which routes transcendentals
# through the emulator-dispatch helper IF@DSQRT instead of the hardware __sqrt.
# The game calls sqrt() in exactly one place -- fd2_render_circle_anim_row, the
# white-pillar (filled-circle-band) spell effect shared by heal / teleport /
# ch30 summon -- so -fpi's IF@DSQRT faults on a strict/accurate x87 emulator
# (86Box) while a lenient one (DOSBox-X) tolerates it. -fpi87 restores the
# original raw-x87 sqrt path and matches the original codegen.
# NO -s: the original binary carries a stack probe (PUSH n / CALL __CHK) in
# every function prologue; -s would strip them from the rebuild. With a 4KB
# DGROUP stack whose bottom sits right above game globals, an unprobed
# overflow silently corrupts state instead of halting cleanly like the
# original, so stack checking stays ON to match vendor behavior.
CF = r"-bt=dos4g -fp5 -fpi87 -3s -ms -zp4 -i=include -i=F:\ailv3"

# Link directive pieces. ailv3.lib from libs/ (mounted F:); CRT by full Watcom
# path (D:). fd2common.lib deliberately absent -- src/util/dpmi.c + src/crt/crt.c
# already define its 8 bare-name symbols (file objs satisfy them before any
# library is pulled).
AIL_LIB = r"F:\ailv3\ailv3.lib"
CRT_LIBS = [r"D:\LIB386\DOS\CLIB3S.LIB",
            r"D:\LIB386\MATH387S.LIB",
            r"D:\LIB386\DOS\EMU387.LIB"]

# Object-stem naming = leaf filename stem minus underscores, first 8 chars,
# collision -> trailing digit. One curated exception: life/main.c -> lifemain
# (the rule would give `main`; lifemain is domain-qualified and avoids confusion
# with the C entry symbol). main module is linked first (determines module name).
OBJ_OVERRIDE = {"life/main.c": "lifemain"}
MAIN_OBJ = "lifemain"


def src_obj_list():
    """[(rel, obj)] for every src/*.c, obj stems unique <=8 chars. No tests/ dep."""
    items, reserved = [], set()
    for p in sorted(SRC.rglob("*.c")):
        rel = p.relative_to(SRC).as_posix()
        if rel in OBJ_OVERRIDE:
            obj = OBJ_OVERRIDE[rel]
        else:
            stem = rel.rsplit("/", 1)[-1][:-2]      # leaf, drop '.c'
            base = stem.replace("_", "")[:8]
            obj, i = base, 0
            while obj in reserved or not obj:
                i += 1
                obj = base[:8 - len(str(i))] + str(i)
        if obj in reserved:
            raise SystemExit("obj stem collision: %s -> %s" % (rel, obj))
        reserved.add(obj)
        items.append((rel, obj))
    return items


def gen_lnk(src_list, with_map=False):
    """src-only wlink directive: every src .obj (main first) + ailv3.lib + CRT.
    with_map appends `option map` so wlink also emits a symbol map (BSS/COMDEF
    layout inspection; emit-pipeline rule E-8b)."""
    ordered = sorted(src_list, key=lambda t: t[1] != MAIN_OBJ)
    L = ["system dos4g", r"name E:\out\FD2.EXE"]
    L += [r"file E:\out\obj\%s.obj" % obj for _, obj in ordered]
    L.append("library %s" % AIL_LIB)
    L += ["library %s" % lib for lib in CRT_LIBS]
    if with_map:
        L.append(r"option map=E:\out\fd2.map")
    return "\n".join(L) + "\n"


def gen_build_bat(src_list):
    """Compile every src .c with %CF% (main.c WITHOUT -Dmain), then link."""
    ordered = sorted(src_list, key=lambda t: t[1] != MAIN_OBJ)   # main first
    L = [r"echo === compile src (FD2.EXE, no tests) === > E:\out\build.out"]
    for rel, obj in ordered:
        L.append(r"D:\BIN\WCC386.EXE %s %%CF%% -fo=E:\out\obj\%s.obj >> E:\out\build.out"
                 % (rel.replace("/", "\\"), obj))
    L.append(r"echo === link FD2.EXE === >> E:\out\build.out")
    L.append(r"D:\BIN\WLINK.EXE @E:\fd2.lnk >> E:\out\build.out")
    L.append(r"echo done > E:\out\build.done")
    L.append("exit")
    return "\n".join(L) + "\n"


def gen_conf():
    """Independent DOSBox conf: C:=src, D:=Watcom, E:=our isolated EXE_DIR,
    F:=libs. Autoexec sets the env + CF and runs our src-only build.bat."""
    lines = [
        "[sdl]", "output=surface",
        "[cpu]", "core=auto", "cputype=pentium_mmx", "cycles=max",
        "[autoexec]",
        'mount C "%s"' % str(SRC),
        'mount E "%s"' % str(EXE_DIR),
        'mount D "%s"' % str(WATCOM),
        'mount F "%s"' % str(LIBS),
        "C:",
        r"if not exist E:\out md E:\out",
        r"if not exist E:\out\obj md E:\out\obj",
        "set WATCOM=D:\\",
        "set PATH=Z:\\;D:\\BIN;D:\\BINB",
        "set INCLUDE=D:\\H",
        "set CF=" + CF,
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
    ap.add_argument("--timeout", type=int, default=300,
                    help="backstop seconds to wait for build.done")
    ap.add_argument("--map", action="store_true",
                    help="also emit a wlink symbol map (out/fd2.map) for BSS/COMDEF "
                         "layout inspection (emit-pipeline rule E-8b)")
    args = ap.parse_args()

    if not (LIBS / "ailv3" / "ailv3.lib").is_file():
        raise SystemExit("missing libs/ailv3/ailv3.lib")
    OBJ.mkdir(parents=True, exist_ok=True)
    src_list = src_obj_list()

    io.open(EXE_DIR / "fd2.lnk", "w", encoding="latin-1", newline="\n").write(gen_lnk(src_list, args.map))
    io.open(EXE_DIR / "build.bat", "w", encoding="latin-1", newline="\r\n").write(gen_build_bat(src_list))
    conf = EXE_DIR / "fd2build.conf"
    io.open(conf, "w", encoding="latin-1", newline="\n").write(gen_conf())

    exe, done, bout, mp = OUT / "FD2.EXE", OUT / "build.done", OUT / "build.out", OUT / "fd2.map"
    for f in (exe, done, bout, mp):
        if f.exists():
            f.unlink()
    if OBJ.is_dir():
        # case-insensitive FS: a single .obj suffix check (globbing *.obj AND
        # *.OBJ would list each file twice on Windows -> double-unlink crash).
        for p in OBJ.iterdir():
            if p.is_file() and p.suffix.lower() == ".obj":
                p.unlink()

    print("[build_fd2] compiling %d src .obj + linking -> %s" % (len(src_list), OUT))
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
    exe_ok = exe.exists()
    print("\n===== wlink / build tail =====")
    print("\n".join(txt.splitlines()[-15:]) if txt else "(no build.out)")
    print("\n===== FD2.EXE: %s ====="
          % ("OK (%d bytes) -> %s" % (exe.stat().st_size, exe) if exe_ok
             else "NOT PRODUCED"))
    print("===== %d undefined symbols =====" % len(syms))
    for s in syms[:40]:
        print("  ", s)
    if len(syms) > 40:
        print("  ... (%d more)" % (len(syms) - 40))
    if args.map:
        print("===== map: %s =====" % (mp if mp.exists() else "NOT PRODUCED"))
    return 0 if (exe_ok and not syms) else 1


if __name__ == "__main__":
    sys.exit(main())
