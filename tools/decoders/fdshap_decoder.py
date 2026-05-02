"""FDSHAP.DAT decoder — extracts the per-shap battle scene snapshot (RLE
320x200) and tile_attribute_flags table for any shap_id.

Each shap_id occupies 2 FDSHAP entries:
    FDSHAP[shap_id*2 + 0]  battle_scene_snapshot (RLE-encoded image)
    FDSHAP[shap_id*2 + 1]  tile_attribute_flags  (4 bytes per tile)

CLI:
    python tools/decoders/fdshap_decoder.py --list                 # entry count + sizes
    python tools/decoders/fdshap_decoder.py --shap-id 0            # decode shap_id 0
    python tools/decoders/fdshap_decoder.py --shap-id 0 --out-dir ./out
"""
from __future__ import annotations

import argparse
import json
import sys
from pathlib import Path

# tools/decoders/fdshap_decoder.py -> decoders -> tools -> fd2_reverse
REPO_ROOT = Path(__file__).resolve().parents[4]
FDSHAP_PATH = REPO_ROOT / "FDSHAP.DAT"

sys.path.insert(0, str(Path(__file__).resolve().parent))
from dat_header_parser import parse_dat_header, read_dat_entry  # noqa: E402
from rle_decoder import rle_decode, write_pgm_indexed  # noqa: E402


def cmd_list() -> int:
    hdr = parse_dat_header(FDSHAP_PATH)
    print(f"FDSHAP.DAT: {hdr['file_size']} bytes, {hdr['entry_count']} entries")
    print(f"Implied shap_id range: 0..{hdr['entry_count'] // 2 - 1}")
    print()
    print(f"{'idx':>4} {'size':>8} {'role':<24} first16")
    for e in hdr["entries"]:
        role = "snapshot" if e["idx"] % 2 == 0 else "tile_attr_flags"
        print(f"{e['idx']:>4} {e['size']:>8} {role:<24} {e['first16_hex']}")
    return 0


def cmd_shap(shap_id: int, out_dir: Path | None,
             width: int = 320, height: int = 200) -> int:
    snap_idx = shap_id * 2
    attr_idx = shap_id * 2 + 1
    snap = read_dat_entry(FDSHAP_PATH, snap_idx)
    attr = read_dat_entry(FDSHAP_PATH, attr_idx)
    print(f"FDSHAP[{snap_idx}] snapshot (shap_id={shap_id}): {len(snap)} bytes; "
          f"first16={snap[:16].hex().upper()}")
    print(f"FDSHAP[{attr_idx}] tile_attr   (shap_id={shap_id}): {len(attr)} bytes; "
          f"first16={attr[:16].hex().upper()}")

    decoded = rle_decode(snap, max_pixels=width * height)
    hist = [0] * 256
    for b in decoded[:width * height]:
        hist[b] += 1
    nonzero = sum(1 for c in hist if c > 0)
    print(f"  decoded {width}x{height}: {len(decoded)} pixels, "
          f"{nonzero} unique values")

    tile_count = len(attr) // 4
    print(f"  tile_attribute_flags: {tile_count} tiles x 4 bytes/tile")
    print("  first 8 tiles:")
    for i in range(min(8, tile_count)):
        f = attr[i * 4:i * 4 + 4]
        print(f"    tile {i:>3d}: {' '.join(f'{b:02X}' for b in f)}  "
              f"flags=[anim+1={'Y' if f[0] & 0x04 else '.'}, "
              f"anim+2={'Y' if f[0] & 0x08 else '.'}, "
              f"pal_half={'Y' if f[0] & 0x10 else '.'}]")

    if out_dir is not None:
        out_dir.mkdir(parents=True, exist_ok=True)
        snap_pgm = out_dir / f"fdshap_idx_{snap_idx:02d}_shap{shap_id:02d}_snapshot_{width}x{height}.pgm"
        write_pgm_indexed(decoded, width, height, snap_pgm)
        print(f"  wrote {snap_pgm}")
        attr_json = out_dir / f"fdshap_idx_{attr_idx:02d}_shap{shap_id:02d}_tile_attr.json"
        attr_json.write_text(json.dumps({
            "tile_count": tile_count,
            "tile_flags": [list(attr[i * 4:i * 4 + 4]) for i in range(tile_count)],
        }, indent=2), encoding="utf-8")
        print(f"  wrote {attr_json}")
    return 0


def main() -> int:
    p = argparse.ArgumentParser(description=__doc__)
    g = p.add_mutually_exclusive_group(required=True)
    g.add_argument("--list", action="store_true",
                   help="list FDSHAP entries with their sizes")
    g.add_argument("--shap-id", type=lambda s: int(s, 0),
                   help="decode the snapshot + tile_attr pair for this shap_id")
    p.add_argument("--out-dir", type=Path,
                   help="directory to write PGM + JSON artifacts")
    p.add_argument("--width", type=int, default=320)
    p.add_argument("--height", type=int, default=200)
    args = p.parse_args()
    if args.list:
        return cmd_list()
    return cmd_shap(args.shap_id, args.out_dir, args.width, args.height)


if __name__ == "__main__":
    sys.exit(main())
