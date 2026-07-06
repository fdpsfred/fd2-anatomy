"""FDSHAP.DAT decoder — extracts the per-shap battle terrain TILE SHEET and the
tile_attribute_flags table for any shap_id.

Each shap_id occupies 2 FDSHAP entries:
    FDSHAP[shap_id*2 + 0]  battle terrain tile sheet
    FDSHAP[shap_id*2 + 1]  tile_attribute_flags  (4 bytes per tile)

The even entry is NOT a single 320x200 image: it is a sheet of 24x24 terrain
tiles (see fd2_convert_battle_tiles_to_24px @ src/battle/btl_init.c). The battle
map compositor places these tiles per the FDFIELD tile map. Layout:
    +0  u16 LE  tile width  (= 0x18 = 24)
    +2  u16 LE  tile height (= 0x18 = 24)
    +4  u16 LE  tile_count
    +6  int32 LE [tile_count]  per-tile byte offsets from the sheet base
    each tile: a command-only fd2_rle_blit_sprite stream, fixed 24x24
               (decoded in-game by fd2_tile_blit_24x24_passthrough).

CLI:
    python tools/decoders/fdshap_decoder.py --list                 # entry count + sizes
    python tools/decoders/fdshap_decoder.py --shap-id 0            # decode shap_id 0
    python tools/decoders/fdshap_decoder.py --shap-id 0 --out-dir ./out
"""
from __future__ import annotations

import argparse
import json
import struct
import sys
from pathlib import Path

# tools/decoders/fdshap_decoder.py -> decoders -> tools -> repo root
REPO_ROOT = Path(__file__).resolve().parents[2]
FDSHAP_PATH = REPO_ROOT / "fd2_game_files" / "FDSHAP.DAT"
FDOTHER_PATH = REPO_ROOT / "fd2_game_files" / "FDOTHER.DAT"
DEFAULT_OUT_DIR = REPO_ROOT / "workspace" / "decoders" / "fdshap"

sys.path.insert(0, str(Path(__file__).resolve().parent))
from dat_header_parser import parse_dat_header, read_dat_entry  # noqa: E402
from rle_decoder import (rle_decode, write_pgm_indexed,  # noqa: E402
                         load_vga_palette_6bit, write_ppm_color)


def decode_tile_sheet(snap: bytes):
    """Decode a FDSHAP terrain tile sheet -> (tile_w, tile_h, [tile pixels]).
    Each tile is a command-only rle_blit stream of tile_w*tile_h indexed px."""
    tw, th = struct.unpack_from("<2H", snap, 0)
    tile_count = struct.unpack_from("<H", snap, 4)[0]
    tiles = []
    for i in range(tile_count):
        off = struct.unpack_from("<i", snap, 6 + i * 4)[0]
        px = rle_decode(snap[off:], max_pixels=tw * th)
        if len(px) < tw * th:
            px = px + bytes(tw * th - len(px))
        tiles.append(px[:tw * th])
    return tw, th, tiles


def tiles_to_atlas(tw: int, th: int, tiles: list, cols: int = 16):
    """Pack tiles into a cols-wide grid -> (atlas_w, atlas_h, indexed pixels)."""
    rows = (len(tiles) + cols - 1) // cols
    aw, ah = cols * tw, rows * th
    atlas = bytearray(aw * ah)
    for i, tile in enumerate(tiles):
        cx, cy = (i % cols) * tw, (i // cols) * th
        for y in range(th):
            atlas[(cy + y) * aw + cx:(cy + y) * aw + cx + tw] = tile[y * tw:(y + 1) * tw]
    return aw, ah, bytes(atlas)


def cmd_list() -> int:
    hdr = parse_dat_header(FDSHAP_PATH)
    print(f"FDSHAP.DAT: {hdr['file_size']} bytes, {hdr['entry_count']} entries")
    print(f"Implied shap_id range: 0..{hdr['entry_count'] // 2 - 1}")
    print()
    print(f"{'idx':>4} {'size':>8} {'role':<24} first16")
    for e in hdr["entries"]:
        role = "tile_sheet" if e["idx"] % 2 == 0 else "tile_attr_flags"
        print(f"{e['idx']:>4} {e['size']:>8} {role:<24} {e['first16_hex']}")
    return 0


def cmd_shap(shap_id: int, out_dir: Path | None, cols: int = 16) -> int:
    snap_idx = shap_id * 2
    attr_idx = shap_id * 2 + 1
    snap = read_dat_entry(FDSHAP_PATH, snap_idx)
    attr = read_dat_entry(FDSHAP_PATH, attr_idx)
    print(f"FDSHAP[{snap_idx}] tile_sheet (shap_id={shap_id}): {len(snap)} bytes; "
          f"first16={snap[:16].hex().upper()}")
    print(f"FDSHAP[{attr_idx}] tile_attr  (shap_id={shap_id}): {len(attr)} bytes; "
          f"first16={attr[:16].hex().upper()}")

    tw, th, tiles = decode_tile_sheet(snap)
    print(f"  tile sheet: {len(tiles)} tiles of {tw}x{th}")
    aw, ah, atlas = tiles_to_atlas(tw, th, tiles, cols)

    tile_count = len(attr) // 4
    print(f"  tile_attribute_flags: {tile_count} tiles x 4 bytes/tile")
    for i in range(min(8, tile_count)):
        f = attr[i * 4:i * 4 + 4]
        print(f"    tile {i:>3d}: {' '.join(f'{b:02X}' for b in f)}  "
              f"flags=[anim+1={'Y' if f[0] & 0x04 else '.'}, "
              f"anim+2={'Y' if f[0] & 0x08 else '.'}, "
              f"pal_half={'Y' if f[0] & 0x10 else '.'}]")

    if out_dir is not None:
        out_dir.mkdir(parents=True, exist_ok=True)
        base = f"fdshap_idx_{snap_idx:02d}_shap{shap_id:02d}_tiles_{aw}x{ah}"
        write_pgm_indexed(atlas, aw, ah, out_dir / (base + ".pgm"))
        print(f"  wrote {out_dir / (base + '.pgm')}")
        if FDOTHER_PATH.exists():
            raw = FDOTHER_PATH.read_bytes()
            pal_off = struct.unpack_from("<I", raw, 6)[0]
            pal = load_vga_palette_6bit(raw[pal_off:pal_off + 768])
            write_ppm_color(atlas, aw, ah, pal, out_dir / (base + ".ppm"))
            print(f"  wrote {out_dir / (base + '.ppm')} (color)")
        attr_json = out_dir / f"fdshap_idx_{attr_idx:02d}_shap{shap_id:02d}_tile_attr.json"
        attr_json.write_text(json.dumps({
            "tile_count": tile_count,
            "tile_flags": [list(attr[i * 4:i * 4 + 4]) for i in range(tile_count)],
        }, indent=2), encoding="utf-8")
        print(f"  wrote {attr_json}")
    return 0


def main() -> int:
    p = argparse.ArgumentParser(description="FDSHAP.DAT terrain tile-sheet decoder")
    g = p.add_mutually_exclusive_group(required=True)
    g.add_argument("--list", action="store_true",
                   help="list FDSHAP entries with their sizes")
    g.add_argument("--shap-id", type=lambda s: int(s, 0),
                   help="decode the tile-sheet + tile_attr pair for this shap_id")
    p.add_argument("--out-dir", type=Path, default=DEFAULT_OUT_DIR,
                   help="directory to write PGM/PPM + JSON artifacts "
                        "(default: workspace/decoders/fdshap)")
    p.add_argument("--cols", type=int, default=16,
                   help="tiles per row in the output atlas (default 16)")
    args = p.parse_args()
    if args.list:
        return cmd_list()
    return cmd_shap(args.shap_id, args.out_dir, args.cols)


if __name__ == "__main__":
    sys.exit(main())
