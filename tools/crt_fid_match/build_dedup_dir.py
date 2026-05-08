"""Materialise the dedup'd unique .obj set under a single flat directory using
the manifest's `key` field as filename (e.g. `0011794f219b_gtxtexts.obj`).

This is the directory Ghidra's AutoImporter consumes during FID library
preparation; flat unique names avoid collisions when many same-named modules
(e.g. `fclose.obj`) come from different lib versions.
"""
from __future__ import annotations

import argparse
import json
import shutil
from pathlib import Path


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--manifest", required=True, type=Path)
    ap.add_argument("--out", required=True, type=Path)
    args = ap.parse_args()

    manifest = json.loads(args.manifest.read_text())
    args.out.mkdir(parents=True, exist_ok=True)
    n = 0
    for m in manifest["modules"]:
        dst = args.out / f"{m['key']}.obj"
        if dst.exists() and dst.stat().st_size > 0:
            continue
        shutil.copyfile(m["src"], dst)
        n += 1
    print(f"Materialised {n} new files into {args.out} (total: {len(manifest['modules'])})")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
