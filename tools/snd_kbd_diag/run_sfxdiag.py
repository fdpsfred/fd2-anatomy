#!/usr/bin/env python3
"""run_sfxdiag.py -- build + run sfxdiag.c under DOSBox-X, play one FDOTHER SFX
via FD2's AIL_install_DIG_INI path, record the mixer output to WAV, and report
whether real sound came out (non-silent waveform) plus the AIL_sample_status
poll. This is a programmatic "did PCM actually play" check (no human listening),
so it is the kind of DOSBox test that stays on the tooling side.

Reuses ail_extract's proven SB16/BLASTER env. Output ->
workspace/snd_kbd_diag/sfxdiag_stage/{SFXDIAG.LOG, captures/*.wav}.
"""
import io, os, shutil, struct, subprocess, sys, time, wave

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
TOOLS = os.path.join(ROOT, "tools", "snd_kbd_diag")
AIL_OUT = os.path.join(ROOT, "workspace", "ail_extract", "out")
GAME = os.path.join(ROOT, "fd2_game_files")
WATCOM = r"C:\Users\fdpsf\Documents\WATCOM_9.5a"
STAGE = os.path.join(ROOT, "workspace", "snd_kbd_diag", "sfxdiag_stage")
CAPS = os.path.join(STAGE, "captures")

LNK = """system dos4g
name sfxdiag.exe
option stack=32K
file sfxdiag.obj
library ailv3.lib
library fd2common.lib
library C:\\LIB386\\DOS\\CLIB3S.LIB
library C:\\LIB386\\MATH387S.LIB
library C:\\LIB386\\DOS\\EMU387.LIB
"""

BUILDBAT = r"""echo BUILD_START > build.log
C:\BIN\WCC386.EXE -bt=dos4g -3s -ms -s -zp4 -d0 -i=. -i=C:\H -fo=sfxdiag.obj sfxdiag.c >> build.log
C:\BIN\WLINK.EXE @sfxdiag.lnk >> build.log
echo BUILD_END >> build.log
"""

# DIG.INI is IRQ=-1/DMA=-1 -> AIL reads IRQ/DMA from BLASTER; I5/D1 matches the
# [sblaster] irq=5 dma=1 below (the known-good ail_extract combo).
CONF = """[sdl]
autolock=false
[render]
aspect=false
[dosbox]
captures={caps}
[dos]
lfn=true
[sblaster]
sbtype=sb16
sbbase=220
irq=5
dma=1
hdma=5
mixer=true
[midi]
mpu401=intelligent
mpubase=330
mididevice=default
[autoexec]
mount c {watcom}
mount d {stage}
set WATCOM=C:\\
set PATH=Z:\\;C:\\BIN;C:\\BINB
set INCLUDE=C:\\H
d:
cd \\
set BLASTER=A220 I5 D1 H5 P330 T6
call buildsfx.bat
if not exist sfxdiag.exe goto done
set DOS4G=quiet
mixer wavstart
sfxdiag.exe
mixer wavstop
:done
echo done > run.done
exit
"""


def extract_sfx_bank():
    fp = os.path.join(GAME, "FDOTHER.DAT")
    data = io.open(fp, "rb").read()
    if data[:6] != b"LLLLLL":
        raise SystemExit("FDOTHER.DAT missing LLLLLL magic")
    idx = 0x1F
    off0 = struct.unpack_from("<I", data, 6 + idx * 4)[0]
    off1 = struct.unpack_from("<I", data, 6 + (idx + 1) * 4)[0]
    io.open(os.path.join(STAGE, "SFXBANK.DAT"), "wb").write(data[off0:off1])
    return off1 - off0


def analyze_wav():
    wavs = [f for f in os.listdir(CAPS) if f.lower().endswith(".wav")] if os.path.isdir(CAPS) else []
    if not wavs:
        return "NO WAV captured"
    wavs.sort()
    path = os.path.join(CAPS, wavs[-1])
    w = wave.open(path, "rb")
    n, sw, fr, nf = w.getnchannels(), w.getsampwidth(), w.getframerate(), w.getnframes()
    raw = w.readframes(nf)
    w.close()
    # peak abs amplitude over 16-bit samples
    peak = 0
    rms_acc = 0
    cnt = 0
    if sw == 2:
        for i in range(0, len(raw) - 1, 2):
            v = struct.unpack_from("<h", raw, i)[0]
            a = abs(v)
            if a > peak:
                peak = a
            rms_acc += v * v
            cnt += 1
    rms = (rms_acc / cnt) ** 0.5 if cnt else 0
    return ("WAV %s: ch=%d %dbit %dHz frames=%d  PEAK=%d RMS=%.1f  -> %s"
            % (wavs[-1], n, sw * 8, fr, nf, peak, rms,
               "SOUND PRESENT" if peak > 200 else "SILENT"))


def main():
    if os.path.isdir(STAGE):
        shutil.rmtree(STAGE)
    os.makedirs(CAPS)

    shutil.copy2(os.path.join(TOOLS, "sfxdiag.c"), os.path.join(STAGE, "sfxdiag.c"))
    for lib in ("ailv3.lib", "fd2common.lib", "ailv3.h"):
        shutil.copy2(os.path.join(AIL_OUT, lib), os.path.join(STAGE, lib))
    io.open(os.path.join(STAGE, "sfxdiag.lnk"), "w", newline="\r\n").write(LNK)
    io.open(os.path.join(STAGE, "buildsfx.bat"), "w", newline="\r\n").write(BUILDBAT)

    for fn in ("DIG.INI", "MDI.INI", "SB16.DIG", "SBPRO2.MDI", "OPL3.MDI",
               "PCSPKR.MDI", "SAMPLE.AD", "SAMPLE.OPL", "SAMPLE.BNK",
               "AILDRVR.LST", "DOS4GW.EXE"):
        src = os.path.join(GAME, fn)
        if os.path.isfile(src):
            shutil.copy2(src, os.path.join(STAGE, fn))
    sz = extract_sfx_bank()
    print("staged SFXBANK.DAT (%d bytes)" % sz)

    conf = os.path.join(STAGE, "sfxdiag.conf")
    io.open(conf, "w", newline="\n").write(
        CONF.format(caps=CAPS, watcom=WATCOM, stage=STAGE))

    done = os.path.join(STAGE, "run.done")
    # non-silent so SB hardware emulation + mixer wave capture work
    proc = subprocess.Popen(["dosbox-x", "-conf", conf])
    t0 = time.time()
    while time.time() - t0 < 150:
        if os.path.exists(done) or proc.poll() is not None:
            break
        time.sleep(1)
    time.sleep(2)
    if proc.poll() is None:
        try:
            proc.terminate()
        except Exception:
            pass

    print("===== SFXDIAG.LOG =====")
    sl = os.path.join(STAGE, "SFXDIAG.LOG")
    print(io.open(sl, encoding="latin-1", errors="replace").read() if os.path.isfile(sl) else "(none)")
    print("===== WAV analysis =====")
    print(analyze_wav())


if __name__ == "__main__":
    main()
