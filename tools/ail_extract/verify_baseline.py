#!/usr/bin/env python3
"""Baseline verification: confirm FD2.EXE itself plays BGM under the same
DOSBox-X silent-mode config that test_aud.exe uses.

Method:
  1. Stage workspace/ail_extract/baseline_stage/ with FD2.EXE + DOS4GW.EXE +
     all FD2 game data files + .MDI / .DIG drivers + the same MDI.INI /
     DIG.INI that tau.c uses.
  2. DOSBox-X autoexec runs `mixer wavstart` then `fd2.exe`. Captured WAV
     accumulates in D:\\captures\\ while the game's main menu BGM plays.
  3. subprocess.run times out after N seconds, killing DOSBox-X. The
     WAV file is flushed by the kill.
  4. Inspect the captured WAV: non-silent → baseline OK (driver probe
     works in this DOSBox-X config); silent → baseline assumption wrong,
     handoff next-step debug path needs revision.

Run: python tools/ail_extract/verify_baseline.py
"""
from __future__ import annotations

import shutil
import struct
import subprocess
import sys
from pathlib import Path

REPO = Path(__file__).resolve().parents[2]
WS = REPO / "workspace" / "ail_extract"
STAGE = WS / "baseline_stage"
CAPTURE = STAGE / "captures"
TOOLS = REPO / "tools" / "ail_extract"
GAME = REPO / "fd2_game_files"
WATCOM = Path(r"C:\Users\fdpsf\Documents\WATCOM_9.5a")
DOSBOX = "dosbox-x"


def stage_assets():
    STAGE.mkdir(parents=True, exist_ok=True)
    CAPTURE.mkdir(parents=True, exist_ok=True)

    # All FD2 game files needed for FD2.EXE to boot.
    fd2_files = [
        "FD2.EXE", "DOS4GW.EXE",
        "FDMUS.DAT", "FDOTHER.DAT", "FDFIELD.DAT", "FDSHAP.DAT",
        "FDTXT.DAT", "FDICON.B24", "TITLE.DAT", "DATO.DAT",
        "BG.DAT", "ANI.DAT", "FIGANI.DAT", "TAI.DAT",
        "SAMPLE.AD", "SAMPLE.OPL", "SAMPLE.BNK",
        "SBLASTER.MDI", "OPL3.MDI", "SB16.DIG", "AILDRVR.LST",
        "FD2.SAV",
    ]
    for fname in fd2_files:
        src = GAME / fname
        if src.exists():
            shutil.copy2(src, STAGE / fname)
        else:
            print(f"WARNING: optional asset {src} not found")

    # SETSOUND-equivalent INI files (same as tau.c uses).
    for ini in ("MDI.INI", "DIG.INI"):
        shutil.copy2(TOOLS / ini, STAGE / ini)


def write_conf(conf_path: Path, run_secs: int = 10):
    autoexec_lines = [
        f"mount c {WATCOM}",
        f"mount d {STAGE}",
        "d:",
        "cd \\",
        "set DOS4G=quiet",
        "echo M1_BEFORE_WAVSTART > marker.log",
        "mixer wavstart",
        "echo M2_AFTER_WAVSTART >> marker.log",
        "fd2.exe > fd2_stdout.log 2> fd2_stderr.log",
        "echo M3_AFTER_FD2 >> marker.log",
        "mixer wavstop",
        "echo M4_AFTER_WAVSTOP >> marker.log",
        "exit",
    ]

    # captures dir is relative to the DOSBox-X working dir; use absolute host
    # path. DOSBox-X recognises Windows paths in [dosbox] captures.
    conf = (
        "[sdl]\nautolock=false\n\n"
        "[render]\naspect=false\n\n"
        f"[dosbox]\ncaptures={CAPTURE}\n\n"
        "[dos]\nlfn=true\n\n"
        "[sblaster]\nsbtype=sb16\nsbbase=220\nirq=5\ndma=1\nhdma=5\nmixer=true\n\n"
        "[midi]\nmpu401=intelligent\nmpubase=330\nmididevice=default\n\n"
        "[autoexec]\n" + "\n".join(autoexec_lines) + "\n"
    )
    conf_path.write_text(conf)


def run_dosbox(conf_path: Path, timeout_secs: int):
    # Note: `-silent` suppresses DOSBox-X startup output AND apparently
    # disables the mixer's WAV capture path; running non-silent here so
    # `mixer wavstart` actually writes a WAV under captures/.
    cmd = [DOSBOX, "-fastlaunch", "-conf", str(conf_path)]
    try:
        return subprocess.run(cmd, capture_output=True, text=True, timeout=timeout_secs)
    except subprocess.TimeoutExpired as e:
        # DOSBox-X was killed; partial WAV should still be written.
        print(f"  DOSBox-X killed after {timeout_secs}s (FD2 does not exit naturally)")
        return e


def analyze_wav(wav_path: Path) -> tuple[bool, str]:
    """Return (has_signal, info_string). has_signal=True if the WAV PCM
    data contains non-trivial samples (variance > threshold)."""
    if not wav_path.exists():
        return False, "WAV file missing"

    data = wav_path.read_bytes()
    if len(data) < 44:
        return False, f"WAV too small ({len(data)} bytes)"

    if data[:4] != b"RIFF" or data[8:12] != b"WAVE":
        return False, "not a RIFF/WAVE file"

    # Find 'data' chunk.
    pos = 12
    while pos < len(data) - 8:
        chunk_id = data[pos:pos + 4]
        chunk_size = struct.unpack("<I", data[pos + 4:pos + 8])[0]
        if chunk_id == b"data":
            payload = data[pos + 8:pos + 8 + chunk_size]
            break
        pos += 8 + chunk_size
    else:
        return False, "no 'data' chunk"

    # Quick heuristic: count non-zero samples vs total. For 16-bit PCM the
    # silent baseline is interleaved zeros / near-zeros. Real audio has wide
    # dynamic range so non-zero samples should be ≥10% of payload.
    nonzero = sum(1 for b in payload if b != 0 and b != 0xff)
    pct = (100 * nonzero) // max(1, len(payload))
    return pct > 5, f"data={len(payload)}B, nonzero={pct}%"


def main():
    print("[1/4] staging FD2.EXE + assets")
    stage_assets()

    # Clear any prior captures so we only see this run's WAV.
    for old_wav in CAPTURE.glob("*.wav"):
        old_wav.unlink()

    conf_path = STAGE / "dosbox.conf"
    print("[2/4] writing DOSBox-X conf")
    write_conf(conf_path)

    print("[3/4] running DOSBox-X (kills after 12s — FD2 menu BGM should play)")
    run_dosbox(conf_path, timeout_secs=12)

    print("[4/4] analyzing captured WAV")
    wavs = sorted(CAPTURE.glob("*.wav"))
    if not wavs:
        print("  FAIL: no WAV captured (mixer wavstart may not have run)")
        sys.exit(2)
    for wav in wavs:
        has_signal, info = analyze_wav(wav)
        marker = "BASELINE OK" if has_signal else "SILENT (baseline NOT verified)"
        print(f"  {wav.name}: {info} -> {marker}")


if __name__ == "__main__":
    main()
