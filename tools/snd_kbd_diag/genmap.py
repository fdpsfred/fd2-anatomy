#!/usr/bin/env python3
"""genmap.py -- produce a wlink map of the src-only FD2.EXE to inspect BSS
symbol placement.

One-shot diagnostic for the "no SFX / dead keyboard" investigation: the
INT-16h/INT-10h input path casts &data_fd2_input_last_key_pressed (a 1-byte
global) to union REGS* and hands it to int386(), which reads/writes the full
28-byte REGS struct. This map reveals (a) whether data_fd2_input_key_input_mode
actually lands at last_key+1 (the AH scancode slot) and (b) which other globals
fall inside the 28-byte window int386 overruns -- in particular the audio
driver/enabled flags defined in the same lifemain.obj BSS.

Reuses link_oracle's proven DOSBox-X flow but appends `option map` to the link
directive. Final map is read back from tests/OUT/fd2.map; intermediate
conf/bat/map copies go under workspace/snd_kbd_diag/.
"""
import io, os, shutil, subprocess, time

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
TESTS = os.path.join(ROOT, "tests")
OUT = os.path.join(TESTS, "OUT")
WS = os.path.join(ROOT, "workspace", "snd_kbd_diag")
AIL_SRC = os.path.join(ROOT, "workspace", "ail_extract", "out")
CONF_TMPL = os.path.join(TESTS, "dosbox.conf")
AIL_LIBS = ["ailv3.lib", "fd2common.lib"]


def main():
    os.makedirs(WS, exist_ok=True)
    for lib in AIL_LIBS:
        shutil.copy2(os.path.join(AIL_SRC, lib), os.path.join(OUT, lib))

    lnk = io.open(os.path.join(TESTS, "fd2.lnk"), encoding="latin-1").read()
    lnk = lnk.rstrip("\n") + "\noption map=E:\\out\\fd2.map\n"
    io.open(os.path.join(TESTS, "fd2map.lnk"), "w",
            encoding="latin-1", newline="\r\n").write(lnk)

    bat = os.path.join(TESTS, "fd2map.bat")
    L = [
        r"echo === fd2 map === > E:\out\fd2map.out",
        r"C:",
        "cd \\",
        r"D:\BIN\WCC386.EXE life\main.c %CF% -fo=E:\out\obj\lifemain.obj >> E:\out\fd2map.out",
        r"D:\BIN\WLINK.EXE @E:\fd2map.lnk >> E:\out\fd2map.out",
        r"echo done > E:\out\fd2map.done",
        "exit",
    ]
    io.open(bat, "w", encoding="latin-1", newline="\r\n").write("\n".join(L) + "\n")

    out = []
    for ln in io.open(CONF_TMPL, encoding="latin-1").read().splitlines():
        if ln.strip().lower() == r"e:\build.bat".lower():
            out.append(r"E:\fd2map.bat")
        else:
            out.append(ln)
    conf = os.path.join(WS, "fd2map.conf")
    io.open(conf, "w", encoding="latin-1", newline="\n").write("\n".join(out) + "\n")

    done = os.path.join(OUT, "fd2map.done")
    mp = os.path.join(OUT, "fd2.map")
    for f in (done, mp):
        if os.path.exists(f):
            os.remove(f)

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

    print("map exists:", os.path.exists(mp))
    if os.path.exists(mp):
        shutil.copy2(mp, os.path.join(WS, "fd2.map"))
        print("copied to", os.path.join(WS, "fd2.map"))
    outf = os.path.join(OUT, "fd2map.out")
    if os.path.exists(outf):
        shutil.copy2(outf, os.path.join(WS, "fd2map.out"))


if __name__ == "__main__":
    main()
