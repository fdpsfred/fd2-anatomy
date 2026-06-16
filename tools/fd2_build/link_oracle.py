#!/usr/bin/env python3
"""link_oracle.py -- run `wlink @fd2.lnk` (src-only) in DOSBox-X and report the
undefined symbols. Those symbols are the authoritative worklist of what real
data/functions are still missing from src/ for a self-contained FD2.EXE.

Prereq: the src .obj files already exist in tests/OUT/obj (run a normal
`python tools/emit/build_test.py` first if not -- this script does NOT compile).

Reuses the proven DOSBox-X environment from tests/dosbox.conf (mounts C:=src,
E:=tests, D:=Watcom; WATCOM/PATH/INCLUDE env that lets `system dos4g` auto-pull
the Watcom CRT libs). Only the final autoexec line is swapped from build.bat to
a link-only fd2link.bat. Stages the Miles AIL libs into E:\\out first.
"""
import io, os, re, shutil, subprocess, sys, time

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
TESTS = os.path.join(ROOT, "tests")
OUT = os.path.join(TESTS, "OUT")
OBJ = os.path.join(OUT, "obj")
WS = os.path.join(ROOT, "workspace", "fd2_build")
AIL_SRC = os.path.join(ROOT, "workspace", "ail_extract", "out")
CONF_TMPL = os.path.join(TESTS, "dosbox.conf")
AIL_LIBS = ["ailv3.lib", "fd2common.lib"]


def stage_libs():
    staged = []
    for lib in AIL_LIBS:
        src = os.path.join(AIL_SRC, lib)
        if not os.path.isfile(src):
            raise SystemExit("AIL lib missing: %s" % src)
        shutil.copy2(src, os.path.join(OUT, lib))
        staged.append(lib)
    return staged


def check_objs():
    # objs are produced as .OBJ (case-insensitive FS); count via listdir
    objs = [f for f in os.listdir(OBJ) if f.lower().endswith(".obj")] if os.path.isdir(OBJ) else []
    return objs


def write_batch():
    bat = os.path.join(TESTS, "fd2link.bat")
    L = [
        r"echo === fd2 build === > E:\out\fd2link.out",
        # The TEST build (build_test.py) compiles src/life/main.c with
        # -Dmain=fd2_main so its `main` does not clash with tests/testmain.c's
        # runner main(); that leaves tests/OUT/obj/lifemain.obj exporting
        # fd2_main, NOT main. For the src-only FD2.EXE the entry MUST be `main`
        # (the CRT cmain386 calls it), so recompile this one file here WITHOUT
        # the define before linking. Every other obj is -D-free already and is
        # reused as-is. (cwd is C: = src, set by the dosbox.conf autoexec.)
        r"C:",
        "cd \\",
        r"D:\BIN\WCC386.EXE life\main.c %CF% -fo=E:\out\obj\lifemain.obj >> E:\out\fd2link.out",
        r"D:\BIN\WLINK.EXE @E:\fd2.lnk >> E:\out\fd2link.out",
        r"echo done > E:\out\fd2link.done",
        "exit",
    ]
    io.open(bat, "w", encoding="latin-1", newline="\r\n").write("\n".join(L) + "\n")


def write_conf():
    out = []
    for ln in io.open(CONF_TMPL, encoding="latin-1").read().splitlines():
        if ln.strip().lower() == r"e:\build.bat".lower():
            out.append(r"E:\fd2link.bat")
        else:
            out.append(ln)
    conf = os.path.join(WS, "fd2link.conf")
    io.open(conf, "w", encoding="latin-1", newline="\n").write("\n".join(out) + "\n")
    return conf


def parse_undefined(txt):
    """Extract undefined symbol names. Watcom wlink emits one line per
    reference: 'file <lib>(<obj>): undefined symbol <name>' (and occasionally
    '<name> is an undefined reference')."""
    syms = set()
    for line in txt.splitlines():
        m = re.search(r"undefined symbol\s+(\S+)", line)
        if not m:
            m = re.search(r"(\S+)\s+is an undefined reference", line)
        if m:
            syms.add(m.group(1).strip())
    return sorted(syms)


def main():
    os.makedirs(WS, exist_ok=True)
    done = os.path.join(OUT, "fd2link.done")
    outf = os.path.join(OUT, "fd2link.out")
    for f in (done, outf):
        if os.path.exists(f):
            os.remove(f)

    objs = check_objs()
    if len(objs) < 40:
        print("WARNING: only %d .obj in tests/OUT/obj -- run build_test.py first." % len(objs))
    staged = stage_libs()
    print("staged AIL libs: %s ; %d objs present" % (staged, len(objs)))
    write_batch()
    conf = write_conf()

    proc = subprocess.Popen(["dosbox-x", "-silent", "-conf", conf])
    t0 = time.time()
    while time.time() - t0 < 120:
        if os.path.exists(done) or proc.poll() is not None:
            break
        time.sleep(1)
    time.sleep(1)
    if proc.poll() is None:
        try:
            proc.terminate()
        except Exception:
            pass

    if not os.path.exists(outf):
        print("NO OUTPUT -- link did not run (check DOSBox).")
        return 2
    txt = io.open(outf, encoding="latin-1").read()
    syms = parse_undefined(txt)
    io.open(os.path.join(WS, "fd2link.out"), "w", encoding="utf-8").write(txt)
    io.open(os.path.join(WS, "undefined.txt"), "w", encoding="utf-8").write("\n".join(syms) + "\n")
    print("\n===== wlink output (tail) =====")
    print("\n".join(txt.splitlines()[-40:]))
    print("\n===== %d undefined symbols (saved workspace/fd2_build/undefined.txt) =====" % len(syms))
    for s in syms[:60]:
        print("  ", s)
    if len(syms) > 60:
        print("  ... (%d more)" % (len(syms) - 60))
    return 0


if __name__ == "__main__":
    sys.exit(main())
