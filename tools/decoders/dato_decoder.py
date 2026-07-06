"""DATO.DAT portrait decoder — decodes the 4-frame 80x80 portrait sprite stored
in each entry (136 entries total).

DATO entry layout:
    +0x00  u32 LE x4   frame A/B/C/D byte offsets (offset_a == 0x10)
    each frame, at its offset:
        +0  u16 LE  width   (= 0x50 = 80)
        +2  u16 LE  height  (= 0x50 = 80)
        +4  dialog-pixel stream

Each frame uses the DIALOG-PIXEL format of fd2_decode_dialog_pixel_byte
(rle_decoder.dialog_pixel_decode), NOT the fd2_rle_blit_sprite format: a byte
<= 0xC0 is one literal pixel; a byte b in 0xC1..0xFF is a run of the next byte,
length (b-0xC1)+1. The 4 frames are portrait expression variants
(normal / smile / talk / closed-eyes).

CLI:
    python tools/decoders/dato_decoder.py --list
    python tools/decoders/dato_decoder.py --idx 0 --out-dir ./out
    python tools/decoders/dato_decoder.py --samples 0,1,2,50,100 --out-dir ./out
"""
from __future__ import annotations

import argparse
import json
import struct
import sys
from pathlib import Path

# tools/decoders/dato_decoder.py -> decoders -> tools -> fd2_reverse
REPO_ROOT = Path(__file__).resolve().parents[2]
DATO_PATH = REPO_ROOT / "fd2_game_files" / "DATO.DAT"
DEFAULT_OUT_DIR = REPO_ROOT / "workspace" / "decoders" / "dato"

sys.path.insert(0, str(Path(__file__).resolve().parent))
from dat_header_parser import parse_dat_header, read_dat_entry  # noqa: E402
from rle_decoder import dialog_pixel_decode  # noqa: E402


def parse_dato_entry(entry_bytes: bytes) -> dict:
    offsets = struct.unpack_from("<4I", entry_bytes, 0)
    # Each frame carries its own [width u16][height u16] header (offset_a points
    # at it, == 0x10), followed by a dialog-pixel stream.
    width, height = struct.unpack_from("<2H", entry_bytes, offsets[0])
    header = {
        "offsets": list(offsets),
        "width": width,
        "height": height,
    }
    frames = []
    for i in range(4):
        start = offsets[i]
        end = offsets[i + 1] if i < 3 else len(entry_bytes)
        fw, fh = struct.unpack_from("<2H", entry_bytes, start)
        commands = entry_bytes[start + 4:end]
        decoded = dialog_pixel_decode(commands, fw * fh)
        if len(decoded) < fw * fh:
            decoded = decoded + bytes(fw * fh - len(decoded))
        frames.append({
            "frame_idx": i,
            "compressed_size": len(commands),
            "decoded_pixels": len(decoded),
            "data": decoded,
        })
    return {"header": header, "frames": frames}


def write_4view_pgm(parsed: dict, out_path: Path) -> None:
    w = parsed["header"]["width"]
    h = parsed["header"]["height"]
    big_w = w * 4
    big = bytearray(big_w * h)
    for i, frame in enumerate(parsed["frames"]):
        fdata = frame["data"]
        if len(fdata) < w * h:
            fdata = fdata + bytes(w * h - len(fdata))
        for y in range(h):
            for x in range(w):
                big[y * big_w + i * w + x] = fdata[y * w + x]
    out_path.parent.mkdir(parents=True, exist_ok=True)
    with open(out_path, "wb") as f:
        f.write(f"P5\n{big_w} {h}\n255\n".encode("ascii"))
        f.write(bytes(big))


def cmd_list() -> int:
    hdr = parse_dat_header(DATO_PATH)
    print(f"DATO.DAT: {hdr['file_size']} bytes, {hdr['entry_count']} entries")
    print(f"{'idx':>4} {'size':>8} first16")
    for e in hdr["entries"]:
        print(f"{e['idx']:>4} {e['size']:>8} {e['first16_hex']}")
    return 0


def cmd_decode(idx_list: list[int], out_dir: Path | None) -> int:
    summary = {"samples": []}
    for idx in idx_list:
        try:
            entry = read_dat_entry(DATO_PATH, idx)
        except Exception as e:
            print(f"  idx {idx}: ERROR {e}")
            continue
        try:
            parsed = parse_dato_entry(entry)
        except Exception as e:
            print(f"  idx {idx}: parse error {e}")
            continue
        h = parsed["header"]
        all_pixels = b"".join(f["data"] for f in parsed["frames"])
        hist = [0] * 256
        for b in all_pixels:
            hist[b] += 1
        nonzero = sum(1 for c in hist if c > 0)
        print(f"DATO[{idx:>3d}] (0x{idx:02X})  entry_size={len(entry)}  "
              f"dim={h['width']}x{h['height']}  "
              f"frame_offsets={[f'0x{o:X}' for o in h['offsets']]}  "
              f"unique_colors={nonzero}")
        for fr in parsed["frames"]:
            print(f"    frame {fr['frame_idx']}  "
                  f"compressed={fr['compressed_size']:>5d}  "
                  f"decoded={fr['decoded_pixels']:>5d}/"
                  f"{h['width']*h['height']}")
        if out_dir is not None:
            out_dir.mkdir(parents=True, exist_ok=True)
            out_pgm = out_dir / f"dato_idx_{idx:03d}_4view.pgm"
            write_4view_pgm(parsed, out_pgm)
            print(f"    wrote {out_pgm}")
            summary["samples"].append({
                "idx": idx,
                "entry_size": len(entry),
                "header": h,
                "unique_colors": nonzero,
                "artifact": str(out_pgm),
            })
    if out_dir is not None and summary["samples"]:
        out_json = out_dir / "dato_decode_summary.json"
        out_json.write_text(json.dumps(summary, indent=2, ensure_ascii=False),
                            encoding="utf-8")
        print(f"\nwrote {out_json}")
    return 0


def main() -> int:
    p = argparse.ArgumentParser(description=__doc__)
    g = p.add_mutually_exclusive_group(required=True)
    g.add_argument("--list", action="store_true",
                   help="list all DATO entries with sizes")
    g.add_argument("--idx", type=lambda s: int(s, 0),
                   help="decode one portrait by idx")
    g.add_argument("--samples", type=str,
                   help="decode comma-separated idx list, e.g. '0,1,2,50,100'")
    p.add_argument("--out-dir", type=Path, default=DEFAULT_OUT_DIR,
                   help="directory to write PGM + JSON artifacts. "
                   f"default: workspace/decoders/dato")
    args = p.parse_args()
    if args.list:
        return cmd_list()
    if args.idx is not None:
        return cmd_decode([args.idx], args.out_dir)
    indices = [int(s, 0) for s in args.samples.split(",")]
    return cmd_decode(indices, args.out_dir)


if __name__ == "__main__":
    sys.exit(main())
