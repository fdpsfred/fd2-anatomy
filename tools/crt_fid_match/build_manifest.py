"""Builds a JSON manifest mapping each unique .obj (by SHA256) to:
- a stable Ghidra project filename (sha-prefixed)
- the list of (version, lib, original module name) where it appears

The manifest drives subsequent Ghidra-side import + FidDb population. Dedup
across the 4 Watcom versions saves ~70% of imports.

Output schema:
    {
      "extracted_root": "<absolute path>",
      "modules": [
        {
          "key":  "<short hash>_<module name>",      // unique Ghidra filename
          "sha":  "<full sha256>",
          "src":  "<absolute path of one representative .obj>",
          "appearances": [
            {"version":"9.5",  "lib":"CLIB3R", "module":"fclose"},
            {"version":"9.5a", "lib":"CLIB3R", "module":"fclose"},
            ...
          ]
        },
        ...
      ]
    }
"""
from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--extracted", required=True, type=Path,
                    help="root of workspace/fid_match/extracted/")
    ap.add_argument("--out", required=True, type=Path)
    args = ap.parse_args()

    by_hash: dict[str, dict] = {}
    for ver_dir in sorted(args.extracted.iterdir()):
        if not ver_dir.is_dir():
            continue
        for lib_dir in sorted(ver_dir.iterdir()):
            if not lib_dir.is_dir():
                continue
            for obj in sorted(lib_dir.glob("*.obj")):
                data = obj.read_bytes()
                h = hashlib.sha256(data).hexdigest()
                short = h[:12]
                module = obj.stem
                rec = by_hash.setdefault(h, {
                    "sha": h,
                    "key": f"{short}_{module}",
                    "src": str(obj.resolve()).replace("\\", "/"),
                    "appearances": [],
                })
                rec["appearances"].append({
                    "version": ver_dir.name,
                    "lib": lib_dir.name,
                    "module": module,
                })

    modules = list(by_hash.values())
    modules.sort(key=lambda m: m["key"])

    manifest = {
        "extracted_root": str(args.extracted.resolve()).replace("\\", "/"),
        "modules": modules,
    }
    args.out.parent.mkdir(parents=True, exist_ok=True)
    args.out.write_text(json.dumps(manifest, indent=2))
    print(f"Wrote {len(modules)} unique modules to {args.out}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
