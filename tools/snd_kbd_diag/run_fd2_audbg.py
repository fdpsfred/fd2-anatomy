#!/usr/bin/env python3
"""run_fd2_audbg.py -- launch the (temporarily instrumented) FD2.EXE under
DOSBox-X in the real game dir, let main() reach its one-shot AUDDBG.TXT write
(AIL init + video-mode set done), then read back the audio-state values. This
is a numeric diagnostic (driver handle / sfx flag / sample handles), not an
audio-content check, so it stays on the tooling side.

Backs up the game-dir FD2.EXE, drops in the instrumented build, runs ~12s,
kills DOSBox, reads AUDDBG.TXT, then restores the original FD2.EXE.
"""
import io, os, shutil, subprocess, time

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
GAME = os.path.join(ROOT, "fd2_game_files")
SRC_EXE = os.path.join(ROOT, "tests", "OUT", "FD2.EXE")
WS = os.path.join(ROOT, "workspace", "snd_kbd_diag")
WATCOM = r"C:\Users\fdpsf\Documents\WATCOM_9.5a"

CONF = """[sdl]
autolock=false
[render]
aspect=false
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
mount c {game}
c:
set BLASTER=A220 I5 D1 H5 P330 T6
set DOS4G=quiet
FD2.EXE
"""


def main():
    os.makedirs(WS, exist_ok=True)
    game_exe = os.path.join(GAME, "FD2.EXE")
    backup = os.path.join(WS, "FD2_gamedir_backup.exe")
    audbg = os.path.join(GAME, "AUDDBG.TXT")

    shutil.copy2(game_exe, backup)               # back up whatever is there
    shutil.copy2(SRC_EXE, game_exe)              # drop in instrumented build
    if os.path.exists(audbg):
        os.remove(audbg)

    conf = os.path.join(WS, "fd2_audbg.conf")
    io.open(conf, "w", newline="\n").write(CONF.format(game=GAME))

    proc = subprocess.Popen(["dosbox-x", "-conf", conf])
    t0 = time.time()
    while time.time() - t0 < 14:
        if os.path.exists(audbg) and (time.time() - t0) > 6:
            break
        if proc.poll() is not None:
            break
        time.sleep(1)
    time.sleep(1)
    if proc.poll() is None:
        try:
            proc.terminate()
        except Exception:
            pass
    subprocess.run(["taskkill", "/F", "/IM", "dosbox-x.exe"],
                   capture_output=True)

    print("===== AUDDBG.TXT =====")
    if os.path.exists(audbg):
        print(io.open(audbg, encoding="latin-1", errors="replace").read())
        shutil.copy2(audbg, os.path.join(WS, "AUDDBG.TXT"))
    else:
        print("(AUDDBG.TXT not written -- main() may not have reached it)")

    shutil.copy2(backup, game_exe)               # restore original
    print("restored game-dir FD2.EXE from backup")


if __name__ == "__main__":
    main()
