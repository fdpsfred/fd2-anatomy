"""DATO.DAT portrait decoder — decodes the 4-frame 80x80 portrait sprite stored
in each entry (136 entries total).

DATO entry layout:
    +0x00  u32 LE  offset_a   (frame A start)
    +0x04  u32 LE  offset_b   (frame B start)
    +0x08  u32 LE  offset_c   (frame C start)
    +0x0C  u32 LE  offset_d   (frame D start)
    +0x10  u16 LE  width      (= 0x50 = 80)
    +0x12  u16 LE  height     (= 0x50 = 80)
    +0x14  RLE-encoded 80x80 portrait frames (4 frames per entry)

Each frame uses the rle_blit_sprite RLE format. The 4 frames likely
correspond to (face_normal, face_smile, face_sad, face_special) variants.

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
REPO_ROOT = Path(__file__).resolve().parents[4]
DATO_PATH = REPO_ROOT / "DATO.DAT"

sys.path.insert(0, str(Path(__file__).resolve().parent))
from dat_header_parser import parse_dat_header, read_dat_entry  # noqa: E402
from rle_decoder import rle_decode  # noqa: E402


def parse_dato_entry(entry_bytes: bytes) -> dict:
    offsets = struct.unpack_from("<4I", entry_bytes, 0)
    width, height = struct.unpack_from("<2H", entry_bytes, 0x10)
    header = {
        "offsets": list(offsets),
        "width": width,
        "height": height,
    }
    frames = []
    for i in range(4):
        start = offsets[i]
        end = offsets[i + 1] if i < 3 else len(entry_bytes)
        compressed = entry_bytes[start:end]
        decoded = rle_decode(compressed, max_pixels=width * height)
        frames.append({
            "frame_idx": i,
            "compressed_size": len(compressed),
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
    p.add_argument("--out-dir", type=Path,
                   help="directory to write PGM + JSON artifacts")
    args = p.parse_args()
    if args.list:
        return cmd_list()
    if args.idx is not None:
        return cmd_decode([args.idx], args.out_dir)
    indices = [int(s, 0) for s in args.samples.split(",")]
    return cmd_decode(indices, args.out_dir)


if __name__ == "__main__":
    sys.exit(main())
