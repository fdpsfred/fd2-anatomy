#!/usr/bin/env python3
"""lib_probe.py -- dump the module/symbol listing of the rebuilt AIL libs
(fd2common.lib, ailv3.lib) and disassemble fd2common.lib, to compare what the
vendor libs PROVIDE against what src/ defines.

Background: the "BGM plays but SFX is silent" investigation. The AIL allocator
slots data_ail_alloc_fnptr/data_ail_free_fnptr (orig statically = malloc/free)
are NOT defined anywhere in src/, so they must come from a vendor lib. This
probe shows (a) which lib owns them and with what initial value, and (b) the
full fd2common PUBDEF set so we can confirm src/ covers every AIL-referenced
fd2common symbol with matching behaviour.

Self-contained DOSBox-X env (D: = Watcom 9.5a). Output -> workspace/snd_kbd_diag/.
"""
import io, os, shutil, subprocess, time

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
TESTS = os.path.join(ROOT, "tests")
OUT = os.path.join(TESTS, "OUT")
WS = os.path.join(ROOT, "workspace", "snd_kbd_diag")
AIL_SRC = os.path.join(ROOT, "workspace", "ail_extract", "out")
CONF_TMPL = os.path.join(TESTS, "dosbox.conf")


def main():
    os.makedirs(WS, exist_ok=True)
    for lib in ("fd2common.lib", "ailv3.lib"):
        shutil.copy2(os.path.join(AIL_SRC, lib), os.path.join(OUT, lib))

    bat = os.path.join(TESTS, "libprobe.bat")
    L = [
        r"echo === lib probe === > E:\out\libprobe.out",
        # wlib listing: modules + the PUBDEF symbols each exports
        r"D:\BIN\WLIB.EXE E:\out\fd2common.lib >> E:\out\libprobe.out",
        r"echo ===== AILV3 ===== >> E:\out\libprobe.out",
        r"D:\BIN\WLIB.EXE E:\out\ailv3.lib >> E:\out\libprobe.out",
        # extract fd2common modules and disassemble them
        r"cd \out",
        r"D:\BIN\WLIB.EXE E:\out\fd2common.lib *",
        r"echo ===== DISASM fd2common modules ===== >> E:\out\libprobe.out",
        r"for %%f in (E:\out\*.obj) do D:\BIN\WDIS.EXE -a -l=E:\out\dis_%%~nf.txt %%f",
        r"echo done > E:\out\libprobe.done",
        "exit",
    ]
    io.open(bat, "w", encoding="latin-1", newline="\r\n").write("\n".join(L) + "\n")

    out = []
    for ln in io.open(CONF_TMPL, encoding="latin-1").read().splitlines():
        if ln.strip().lower() == r"e:\build.bat".lower():
            out.append(r"E:\libprobe.bat")
        else:
            out.append(ln)
    conf = os.path.join(WS, "libprobe.conf")
    io.open(conf, "w", encoding="latin-1", newline="\n").write("\n".join(out) + "\n")

    done = os.path.join(OUT, "libprobe.done")
    outf = os.path.join(OUT, "libprobe.out")
    for f in (done, outf):
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

    if os.path.exists(outf):
        shutil.copy2(outf, os.path.join(WS, "libprobe.out"))
        print("listing ->", os.path.join(WS, "libprobe.out"))
    # collect disassemblies
    n = 0
    for f in os.listdir(OUT):
        if f.lower().startswith("dis_") and f.lower().endswith(".txt"):
            shutil.copy2(os.path.join(OUT, f), os.path.join(WS, f))
            n += 1
    print("copied %d fd2common disassembly files" % n)


if __name__ == "__main__":
    main()
