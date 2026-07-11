#!/usr/bin/env python3
"""A5.C — pack ailv3.lib + fd2common.lib via wlib 9.5a in DOSBox-X.

WLIB 9.5a (v3.0) does not honour LFN under DOSBox-X, so this script first
copies every `objs/*.obj` to an 8.3 short alias under `build/objshort/`
(A0001.OBJ ... A0526.OBJ for AIL objects; F0001.OBJ ... F0008.OBJ for
fd2common), then drives wlib through a DOSBox-X silent-mode session.

Inputs  : workspace/ail_extract/objs/{ail_*,fd2common_*}.obj
Outputs : workspace/ail_extract/build/objshort/        — 8.3 aliases
          workspace/ail_extract/build/alias_mapping.json — short ↔ long
          workspace/ail_extract/build/AILV3.RSP / FD2C.RSP
          workspace/ail_extract/build/pack_libs.conf    — DOSBox-X conf
          workspace/ail_extract/out/ailv3.lib   ailv3.lst
          workspace/ail_extract/out/fd2common.lib fd2common.lst
          workspace/ail_extract/out/pack_libs_summary.json
"""

import argparse
import hashlib
import json
import os
import shutil
import subprocess
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
WORKSPACE = ROOT / "workspace" / "ail_extract"
OBJS_DIR = WORKSPACE / "objs"
BUILD_DIR = WORKSPACE / "build"
BUILD_SHORT = BUILD_DIR / "objshort"  # 8.3 friendly (wlib 9.5a doesn't honour LFN)
OUT_DIR = WORKSPACE / "out"

# Watcom install (external to repo): honour %WATCOM%, else the home-dir default.
WATCOM = Path(os.environ.get("WATCOM") or Path.home() / "Documents" / "WATCOM_9.5a")
DOSBOX = "dosbox-x"


def md5_of(path: Path) -> str:
    return hashlib.md5(path.read_bytes()).hexdigest()


def build_aliases(limit_ail=None, limit_fd2c=None):
    """Copy objs to 8.3 aliases and return mapping dict.

    `limit_*` lets the caller restrict counts for a small smoke test.
    """
    BUILD_SHORT.mkdir(parents=True, exist_ok=True)
    BUILD_DIR.mkdir(parents=True, exist_ok=True)
    # Wipe stale aliases to keep numbering deterministic
    for p in BUILD_SHORT.glob("*.OBJ"):
        p.unlink()

    ail_objs = sorted(p.name for p in OBJS_DIR.glob("ail_*.obj"))
    fd2c_objs = sorted(p.name for p in OBJS_DIR.glob("fd2common_*.obj"))
    if limit_ail is not None:
        ail_objs = ail_objs[:limit_ail]
    if limit_fd2c is not None:
        fd2c_objs = fd2c_objs[:limit_fd2c]

    mapping = {"ail": {}, "fd2c": {}}
    for i, name in enumerate(ail_objs, start=1):
        short = f"A{i:04d}.OBJ"
        mapping["ail"][short] = name
        shutil.copy2(OBJS_DIR / name, BUILD_SHORT / short)
    for i, name in enumerate(fd2c_objs, start=1):
        short = f"F{i:04d}.OBJ"
        mapping["fd2c"][short] = name
        shutil.copy2(OBJS_DIR / name, BUILD_SHORT / short)

    # Sanity: alias byte-identical original
    for group in mapping.values():
        for short, orig in group.items():
            if md5_of(BUILD_SHORT / short) != md5_of(OBJS_DIR / orig):
                raise RuntimeError(f"alias {short} != {orig} byte mismatch")

    (BUILD_DIR / "alias_mapping.json").write_text(json.dumps(mapping, indent=2))

    ail_rsp = "\n".join(f"+{s}" for s in sorted(mapping["ail"].keys())) + "\n"
    fd2c_rsp = "\n".join(f"+{s}" for s in sorted(mapping["fd2c"].keys())) + "\n"
    (BUILD_DIR / "AILV3.RSP").write_text(ail_rsp)
    (BUILD_DIR / "FD2C.RSP").write_text(fd2c_rsp)

    return mapping


def write_dosbox_conf(conf_path: Path):
    """DOSBox-X conf mounts WATCOM as C:, build/ as D:, runs wlib twice, exits."""
    conf = (
        "[sdl]\n"
        "autolock=false\n\n"
        "[render]\n"
        "aspect=false\n\n"
        "[dos]\n"
        "lfn=true\n\n"
        "[autoexec]\n"
        f"mount c {WATCOM}\n"
        f"mount d {BUILD_DIR}\n"
        "set WATCOM=c:\\\n"
        "set PATH=c:\\binb;c:\\bin\n"
        "d:\n"
        "cd objshort\n"
        "c:\\binb\\wlib.exe -n -q -l=d:\\AILV3.LST d:\\AILV3.LIB @d:\\AILV3.RSP > d:\\wlib_ailv3.log\n"
        "c:\\binb\\wlib.exe -n -q -l=d:\\FD2C.LST d:\\FD2C.LIB @d:\\FD2C.RSP > d:\\wlib_fd2c.log\n"
        "exit\n"
    )
    conf_path.write_text(conf)


def run_dosbox(conf_path: Path):
    cmd = [DOSBOX, "-silent", "-conf", str(conf_path)]
    return subprocess.run(cmd, capture_output=True, text=True, timeout=600)


def collect_errors(log_path: Path):
    if not log_path.exists():
        return ["LOG MISSING"]
    return [ln for ln in log_path.read_text(errors="replace").splitlines() if "Error!" in ln]


def sanity_check(mapping):
    out = {}
    out["expect_ail_modules"] = len(mapping["ail"])
    out["expect_fd2c_modules"] = len(mapping["fd2c"])
    out["ailv3_lib_size"] = (OUT_DIR / "ailv3.lib").stat().st_size if (OUT_DIR / "ailv3.lib").exists() else None
    out["fd2common_lib_size"] = (OUT_DIR / "fd2common.lib").stat().st_size if (OUT_DIR / "fd2common.lib").exists() else None
    out["ailv3_lst_size"] = (OUT_DIR / "ailv3.lst").stat().st_size if (OUT_DIR / "ailv3.lst").exists() else None
    out["fd2common_lst_size"] = (OUT_DIR / "fd2common.lst").stat().st_size if (OUT_DIR / "fd2common.lst").exists() else None
    out["wlib_ailv3_errors"] = collect_errors(BUILD_DIR / "wlib_ailv3.log")
    out["wlib_fd2c_errors"] = collect_errors(BUILD_DIR / "wlib_fd2c.log")
    return out


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--smoke", action="store_true",
                    help="only stage 5 AIL + 2 fd2c objs for a small batch test")
    ap.add_argument("--skip-alias", action="store_true")
    args = ap.parse_args()

    if not args.skip_alias:
        print("[1/4] building 8.3 aliases ...")
        if args.smoke:
            mapping = build_aliases(limit_ail=5, limit_fd2c=2)
        else:
            mapping = build_aliases()
        print(f"  ail  : {len(mapping['ail'])} aliases")
        print(f"  fd2c : {len(mapping['fd2c'])} aliases")
    else:
        mapping = json.loads((BUILD_DIR / "alias_mapping.json").read_text())

    conf_path = BUILD_DIR / "pack_libs.conf"
    write_dosbox_conf(conf_path)

    # Wipe stale outputs from BUILD_DIR (rerun-safe)
    for fname in ("AILV3.LIB", "FD2C.LIB", "AILV3.LST", "FD2C.LST",
                  "wlib_ailv3.log", "wlib_fd2c.log"):
        (BUILD_DIR / fname).unlink(missing_ok=True)

    print("[2/4] running DOSBox-X + wlib 9.5a ...")
    res = run_dosbox(conf_path)
    if res.returncode != 0:
        print(f"  dosbox-x exit={res.returncode}")

    OUT_DIR.mkdir(parents=True, exist_ok=True)
    print("[3/4] moving libs to out/ ...")
    for src_name, dst_name in (
        ("AILV3.LIB", "ailv3.lib"),
        ("FD2C.LIB", "fd2common.lib"),
        ("AILV3.LST", "ailv3.lst"),
        ("FD2C.LST", "fd2common.lst"),
    ):
        src = BUILD_DIR / src_name
        if src.exists():
            shutil.copy2(src, OUT_DIR / dst_name)
            print(f"  {src_name} -> out/{dst_name} ({src.stat().st_size} bytes)")
        else:
            print(f"  {src_name} MISSING")

    print("[4/4] sanity ...")
    summary = sanity_check(mapping)
    print(json.dumps(summary, indent=2))
    (OUT_DIR / "pack_libs_summary.json").write_text(json.dumps(summary, indent=2))


if __name__ == "__main__":
    main()
