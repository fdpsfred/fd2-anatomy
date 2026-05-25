#!/usr/bin/env python3
"""Phase C/D/E driver: stage assets, build inside DOSBox-X, run, capture log.

Stages:
  1. Stage workspace/ail_extract/run_stage/  with:
       tau.c, taudio.lnk, ailv3.h, ailv3.lib, fd2common.lib,
       build_test.bat, FDMUS.DAT, FDOTHER.DAT, *.MDI / *.DIG driver binaries,
       MDI.INI + DIG.INI (SETSOUND-equivalent text-INIs sourced from
       tools/ail_extract/).
  2. Generate DOSBox-X conf that mounts WATCOM_9.5a + run_stage/ and runs
       build_test.bat then test_audio.exe.
  3. Launch dosbox-x -silent.
  4. Collect build_result.txt + test_audio.log + WAV (if mixer wavstart ran).

Run with `python tools/ail_extract/run_test.py [--build-only|--run-only]`.
"""
from __future__ import annotations

import argparse
import shutil
import subprocess
from pathlib import Path

REPO = Path(__file__).resolve().parents[2]
WS = REPO / "workspace" / "ail_extract"
STAGE = WS / "run_stage"
OUT = WS / "out"
TOOLS = REPO / "tools" / "ail_extract"
GAME = REPO / "fd2_game_files"
WATCOM = Path(r"C:\Users\fdpsf\Documents\WATCOM_9.5a")
DOSBOX = "dosbox-x"


def stage_assets():
    STAGE.mkdir(parents=True, exist_ok=True)
    # Source files
    sources = [
        (TOOLS / "tau.c", "tau.c"),
        (TOOLS / "taudio.lnk", "taudio.lnk"),
        (TOOLS / "build_test.bat", "build_test.bat"),
        # dpmis.c not staged — diagnostic mode uses real DPMI lock/unlock
        (TOOLS / "MDI.INI", "MDI.INI"),
        (TOOLS / "DIG.INI", "DIG.INI"),
        (OUT / "ailv3.h", "ailv3.h"),
        (OUT / "ailv3.lib", "ailv3.lib"),
        (OUT / "fd2common.lib", "fd2common.lib"),
    ]
    if (TOOLS / "smoke.c").exists():
        sources.append((TOOLS / "smoke.c", "smoke.c"))
    for src, dst_name in sources:
        if not src.exists():
            raise FileNotFoundError(f"missing source: {src}")
        shutil.copy2(src, STAGE / dst_name)

    # FD2 asset extracts. Instead of staging full FDMUS.DAT / FDOTHER.DAT
    # (and forcing tau.c to parse the LLLLLL container at runtime), pre-extract
    # the single test BGM and SFX entries into 8.3 standalone files. This
    # avoids long synchronous fopen/fread on the multi-MB container during a
    # critical window where the PIT timer ISR is firing async — making it
    # easier to isolate any ISR-related wild jump.
    import struct
    fdmus_path = GAME / "FDMUS.DAT"
    fdother_path = GAME / "FDOTHER.DAT"
    if not fdmus_path.exists() or not fdother_path.exists():
        raise FileNotFoundError(f"missing FDMUS.DAT or FDOTHER.DAT")
    fdmus = fdmus_path.read_bytes()
    fdother = fdother_path.read_bytes()
    if fdmus[:6] != b"LLLLLL" or fdother[:6] != b"LLLLLL":
        raise ValueError("FDMUS/FDOTHER missing LLLLLL magic")
    bgm_idx = 0x12
    sfx_bank_idx = 0x1F  # FDOTHER[0x1F] = LLLLLL sub-archive of SFX entries
    bgm_off0 = struct.unpack_from("<I", fdmus, 6 + bgm_idx * 4)[0]
    bgm_off1 = struct.unpack_from("<I", fdmus, 6 + (bgm_idx + 1) * 4)[0]
    sfx_off0 = struct.unpack_from("<I", fdother, 6 + sfx_bank_idx * 4)[0]
    sfx_off1 = struct.unpack_from("<I", fdother, 6 + (sfx_bank_idx + 1) * 4)[0]
    (STAGE / "BGM12.XMI").write_bytes(fdmus[bgm_off0:bgm_off1])
    (STAGE / "SFX_BANK.DAT").write_bytes(fdother[sfx_off0:sfx_off1])
    print(f"  staged BGM12.XMI ({bgm_off1 - bgm_off0} bytes) "
          f"SFX_BANK.DAT ({sfx_off1 - sfx_off0} bytes, "
          f"contains {struct.unpack_from('<I', fdother, sfx_off0 + 6 + 4)[0] // 4} SFX entries)")

    # MDI + DIG drivers (test_audio.c hardcodes the IO_PARMS for these).
    # Timbre patches (.AD/.OPL/.BNK) get auto-loaded by OPL3.MDI from the
    # working dir; copy them so the FM bank is available.
    for fname in ("OPL3.MDI", "SBPRO2.MDI", "SBLASTER.MDI", "PCSPKR.MDI", "SB16.DIG",
                  "SAMPLE.AD", "SAMPLE.OPL", "SAMPLE.BNK", "AILDRVR.LST"):
        src = GAME / fname
        if not src.exists():
            print(f"WARNING: optional asset {src} not found")
            continue
        shutil.copy2(src, STAGE / fname)

    # DOS/4GW loader stub (LE executables need this in PATH at runtime).
    dos4gw = WATCOM / "BIN" / "DOS4GW.EXE"
    if dos4gw.exists():
        shutil.copy2(dos4gw, STAGE / "DOS4GW.EXE")


def write_conf(conf_path: Path, build_only: bool, run_only: bool):
    autoexec_lines = [
        f"mount c {WATCOM}",
        f"mount d {STAGE}",
        "d:",
        "cd \\",
        # Dump pre-set env so we know what DOSBox-X auto-sets (for SB-class
        # driver compatibility check).
        "set > envdump.log",
        # BLASTER env var: required by AIL SB-class drivers' fn 0x303 env-parse
        # path (.E BLASTER directive in AILDRVR.LST). MUST match [sblaster]
        # conf (irq=5, dma=1, hdma=5, sb16=T6) — IRQ mismatch silently breaks
        # PCM playback (driver hooks IRQ N but emulator fires on IRQ M).
        "set BLASTER=A220 I5 D1 H5 P330 T6",
    ]
    if not run_only:
        autoexec_lines.append("call build_test.bat")
    if not build_only:
        autoexec_lines += [
            "if not exist test_aud.exe goto skip_run",
            "set DOS4G=quiet",
            "echo PRE_TEST_AUD > shellmark.log",
            "test_aud.exe > test_aud.stdout.log 2> test_aud.stderr.log",
            "echo POST_TEST_AUD >> shellmark.log",
            ":skip_run",
        ]
    autoexec_lines.append("echo PRE_EXIT >> shellmark.log")
    autoexec_lines.append("exit")

    conf = (
        "[sdl]\nautolock=false\n\n"
        "[render]\naspect=false\n\n"
        "[dos]\nlfn=true\n\n"
        "[sblaster]\nsbtype=sb16\nsbbase=220\nirq=5\ndma=1\nhdma=5\nmixer=true\n\n"
        "[midi]\nmpu401=intelligent\nmpubase=330\nmididevice=default\n\n"
        "[autoexec]\n" + "\n".join(autoexec_lines) + "\n"
    )
    conf_path.write_text(conf)


def run_dosbox(conf_path: Path, silent: bool = False, timeout: int = 180):
    # -silent suppresses startup output AND may disable SB hardware
    # emulation responses (probe fail observed under silent mode).
    # Default to non-silent so SB-class driver probes work; user can pass
    # silent=True for CI/headless context if needed.
    cmd = [DOSBOX]
    if silent:
        cmd.append("-silent")
    cmd += ["-conf", str(conf_path)]
    return subprocess.run(cmd, capture_output=True, text=True, timeout=timeout)


def report():
    print("--- build_result.txt ---")
    bres = STAGE / "build_result.txt"
    print(bres.read_text() if bres.exists() else "(missing)")
    for name in ("wcc386.log", "wlink.log", "test_aud.stdout.log", "test_aud.stderr.log", "test_audio.log", "trace.log"):
        p = STAGE / name
        if p.exists():
            print(f"--- {name} ({p.stat().st_size} bytes) ---")
            text = p.read_text(errors="replace")
            print(text[:3000])
            if len(text) > 3000:
                print("... [truncated]")
        else:
            print(f"--- {name}: missing ---")


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--build-only", action="store_true")
    ap.add_argument("--run-only", action="store_true")
    ap.add_argument("--timeout", type=int, default=180,
                    help="DOSBox-X subprocess timeout in seconds (default 180)")
    args = ap.parse_args()

    print(f"[1/3] staging assets to {STAGE}")
    stage_assets()

    conf_path = STAGE / "dosbox.conf"
    print(f"[2/3] writing DOSBox-X conf")
    write_conf(conf_path, args.build_only, args.run_only)

    print(f"[3/3] running DOSBox-X")
    try:
        res = run_dosbox(conf_path, timeout=args.timeout)
        if res.returncode != 0:
            print(f"  dosbox-x exit={res.returncode}")
        # Surface E_Exit / IRET / fault lines from DOSBox-X stderr — these
        # are emulator-level panics not captured by test_aud.stderr.log.
        if res.stderr and any(kw in res.stderr for kw in ("E_Exit", "IRET", "fault", "panic")):
            print(f"  dosbox stderr (panic excerpt): {res.stderr[-2000:]}")
    except subprocess.TimeoutExpired as ex:
        # When test_aud.exe IRETDs or hangs the IPC inside DPMI, DOSBox-X
        # stays alive emulating the failed state. Force-kill so subsequent
        # runs can stage cleanly.
        print(f"  dosbox-x TIMEOUT after {ex.timeout}s — likely test_aud.exe IRETD/hang")
        subprocess.run(["taskkill", "/F", "/IM", "dosbox-x.exe"], capture_output=True)

    print()
    report()


if __name__ == "__main__":
    main()
