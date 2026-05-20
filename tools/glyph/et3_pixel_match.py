"""ET3 STDFONT.15 + ASCFONT.15 pixel-match glyph identifier — for each FD2
chinese_font_sheet glyph (1bpp 16x16), find the closest matching ET3 font
glyph and emit a CSV of glyph_id -> Big5 character + confidence.

Strategy:
    1. Parse ET3 STDFONT.15 (13094 x 16x15 bitmaps).
    2. Parse ET3 ASCFONT.15 (256 x 8x15) padded to 16-wide.
    3. For each FD2 glyph (16x16, uint16[16]), drop top OR bottom row,
       producing a 16x15 candidate.
    4. Take min Hamming distance against every ET3 glyph; the best match
       gives the matched ET3 index.
    5. Map matched ET3 STDFONT index -> Big5 codepoint via the verified
       layout (Lead 0xA4..0xC5 157 each, Lead 0xC6 63, Lead 0xC9..0xF8 157
       each, Lead 0xF9 116, then 41 ETEN extension chars in PUA).

Confidence = 1 - hamming_distance / total_pixels (15 * 16 = 240).

ET3 fonts must live in tools/glyph/ET3_fonts/ (ASCFONT.15 + STDFONT.15).

CLI:
    python tools/glyph/et3_pixel_match.py --out-csv ./match.csv
    python tools/glyph/et3_pixel_match.py --out-csv ./match.csv --limit 100
"""
from __future__ import annotations

import argparse
import csv
import struct
import sys
from pathlib import Path

# tools/glyph/et3_pixel_match.py -> glyph -> tools -> repo root
REPO_ROOT = Path(__file__).resolve().parents[2]
FDOTHER_PATH = REPO_ROOT / "fd2_game_files" / "FDOTHER.DAT"
DEFAULT_OUT_CSV = REPO_ROOT / "workspace" / "glyph" / "match" / "glyph_match.csv"

# ET3 fonts ship next to this script
ET3_DIR = Path(__file__).resolve().parent / "ET3_fonts"
ASCFONT_PATH = ET3_DIR / "ASCFONT.15"
STDFONT_PATH = ET3_DIR / "STDFONT.15"

GLYPH_W = 16
GLYPH_H_FD2 = 16
GLYPH_H_ET3 = 15
GLYPH_BYTES_FD2 = 32
GLYPH_BYTES_ET3 = 30
TOTAL_GLYPHS = 1824


def read_dat_entry(path: Path, idx: int) -> bytes:
    raw = path.read_bytes()
    first_off = struct.unpack_from("<I", raw, 6)[0]
    n = (first_off - 6) // 4
    if idx >= n - 1:
        raise ValueError(f"idx {idx} out of range (n={n})")
    start = struct.unpack_from("<I", raw, 6 + idx * 4)[0]
    end = struct.unpack_from("<I", raw, 6 + (idx + 1) * 4)[0]
    return raw[start:end]


def fd2_glyph_to_rows(glyph_bytes: bytes) -> list[int]:
    return [(glyph_bytes[r * 2] << 8) | glyph_bytes[r * 2 + 1]
            for r in range(GLYPH_H_FD2)]


def et3_std_glyph_to_rows(glyph_bytes: bytes) -> list[int]:
    return [(glyph_bytes[r * 2] << 8) | glyph_bytes[r * 2 + 1]
            for r in range(GLYPH_H_ET3)]


def et3_asc_glyph_to_rows(glyph_bytes: bytes) -> list[int]:
    # ASCII glyph 8x15 -> shift left into top byte of u16 (padded right)
    return [glyph_bytes[r] << 8 for r in range(GLYPH_H_ET3)]


def hamming_rows(a: list[int], b: list[int]) -> int:
    d = 0
    for x, y in zip(a, b):
        d += bin(x ^ y).count("1")
    return d


def build_et3_stdfont_index_table(num_slots: int) -> list[tuple[int, int]]:
    """Returns (lead, tail) Big5 codepoints in ET3 STDFONT.15 layout order.

    Verified via bitmap inspection of idx 0 = '一', idx 66 = '中':
        idx 0..5400      = Big5 Level 1 (5401 chars, 0xA440..0xC67E)
                            Lead 0xA4..0xC5 = 157 entries each
                            Lead 0xC6 = 63 entries (tails 0x40..0x7E only)
        idx 5401..13052  = Big5 Level 2 (7652 chars, 0xC940..0xF9D5)
                            Lead 0xC9..0xF8 = 157 entries each
                            Lead 0xF9 = 116 entries (0x40..0x7E + 0xA1..0xD5)
        idx 13053..13093 = 41 ETEN extension chars (PUA codepoints)
    """
    out = []
    for lead in range(0xA4, 0xC6):
        for tail in list(range(0x40, 0x7F)) + list(range(0xA1, 0xFF)):
            out.append((lead, tail))
    for tail in range(0x40, 0x7F):
        out.append((0xC6, tail))
    for lead in range(0xC9, 0xF9):
        for tail in list(range(0x40, 0x7F)) + list(range(0xA1, 0xFF)):
            out.append((lead, tail))
    for tail in range(0x40, 0x7F):
        out.append((0xF9, tail))
    for tail in range(0xA1, 0xD6):
        out.append((0xF9, tail))
    while len(out) < num_slots:
        out.append((0, 0))
    return out[:num_slots]


def lead_tail_to_unicode(lead: int, tail: int) -> str:
    if lead == 0 and tail == 0:
        return "?"
    try:
        return bytes([lead, tail]).decode("cp950")
    except UnicodeDecodeError:
        try:
            return bytes([lead, tail]).decode("big5")
        except UnicodeDecodeError:
            return "?"


def main() -> int:
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("--out-csv", type=Path, default=DEFAULT_OUT_CSV,
                   help="output CSV (glyph_id_hex, character, confidence, alternatives). "
                   f"default: workspace/glyph/match/glyph_match.csv")
    p.add_argument("--limit", type=int, default=0,
                   help="for quick testing, only process first N glyphs (0=all)")
    args = p.parse_args()

    if not ASCFONT_PATH.exists() or not STDFONT_PATH.exists():
        sys.stderr.write(
            f"ERROR: ET3 fonts not found at {ET3_DIR}\n"
            f"  expected: ASCFONT.15 + STDFONT.15\n"
        )
        return 2

    print("Loading FD2 chinese_font_sheet (FDOTHER[4])...")
    fd2_raw = read_dat_entry(FDOTHER_PATH, 4)
    fd2_glyphs = []
    for i in range(TOTAL_GLYPHS):
        gb = fd2_raw[i * GLYPH_BYTES_FD2:(i + 1) * GLYPH_BYTES_FD2]
        fd2_glyphs.append(fd2_glyph_to_rows(gb))
    print(f"  loaded {len(fd2_glyphs)} FD2 glyphs (16x16)")

    print(f"Loading ET3 STDFONT.15 from {STDFONT_PATH}...")
    std_raw = STDFONT_PATH.read_bytes()
    std_count = len(std_raw) // GLYPH_BYTES_ET3
    std_glyphs = [et3_std_glyph_to_rows(std_raw[i * GLYPH_BYTES_ET3:(i + 1) * GLYPH_BYTES_ET3])
                  for i in range(std_count)]
    print(f"  loaded {std_count} STDFONT glyphs (16x15)")

    print(f"Loading ET3 ASCFONT.15 from {ASCFONT_PATH}...")
    asc_raw = ASCFONT_PATH.read_bytes()
    asc_count = len(asc_raw) // 15
    asc_glyphs = [et3_asc_glyph_to_rows(asc_raw[i * 15:(i + 1) * 15])
                  for i in range(asc_count)]
    print(f"  loaded {asc_count} ASCFONT glyphs (8x15)")

    big5_table = build_et3_stdfont_index_table(std_count)
    print(f"  ET3 codepoint table built: {len(big5_table)} entries")

    args.out_csv.parent.mkdir(parents=True, exist_ok=True)
    f = open(args.out_csv, "w", encoding="utf-8", newline="")
    writer = csv.writer(f)
    writer.writerow(["glyph_id_hex", "character", "confidence", "alternatives"])

    perfect = high = low = blank = 0
    n = args.limit if args.limit > 0 else TOTAL_GLYPHS
    print(f"Matching {n} FD2 glyphs against {std_count} STDFONT + {asc_count} ASCFONT...")

    for fid in range(n):
        fd2_rows = fd2_glyphs[fid]
        if not any(fd2_rows):
            writer.writerow([f"0x{fid:04X}", "", "1.0", "(blank)"])
            blank += 1
            continue
        candidates = [
            ("drop_top", fd2_rows[1:16]),
            ("drop_bot", fd2_rows[0:15]),
        ]
        best_dist = 99999
        best_kind = None
        best_idx = -1
        best_alignment = None
        for align_name, rows15 in candidates:
            for sid, std_rows in enumerate(std_glyphs):
                d = hamming_rows(rows15, std_rows)
                if d < best_dist:
                    best_dist = d
                    best_kind = "std"
                    best_idx = sid
                    best_alignment = align_name
            for aid, asc_rows in enumerate(asc_glyphs):
                d = hamming_rows(rows15, asc_rows)
                if d < best_dist:
                    best_dist = d
                    best_kind = "asc"
                    best_idx = aid
                    best_alignment = align_name
        total_pixels = 15 * 16
        confidence = 1.0 - best_dist / total_pixels
        if best_kind == "std":
            lead, tail = big5_table[best_idx]
            ch = lead_tail_to_unicode(lead, tail)
            alt = (f"std[{best_idx}]=Big5(0x{lead:02X}{tail:02X}),"
                   f"align={best_alignment},ham={best_dist}")
        elif best_kind == "asc":
            ch = chr(best_idx) if 32 <= best_idx < 127 else f"<{best_idx}>"
            alt = f"asc[{best_idx}],align={best_alignment},ham={best_dist}"
        else:
            ch = ""
            alt = "no_match"
        writer.writerow([f"0x{fid:04X}", ch, f"{confidence:.4f}", alt])
        if best_dist == 0:
            perfect += 1
        elif confidence >= 0.95:
            high += 1
        elif confidence < 0.7:
            low += 1
        if (fid + 1) % 100 == 0:
            print(f"  [{fid+1:>4d}/{n}] perfect={perfect} high(>=0.95)={high} "
                  f"low(<0.7)={low}")
    f.close()
    print()
    print(f"FINAL: total={n}")
    print(f"  perfect (hamming=0)   : {perfect}")
    print(f"  high (conf >= 0.95)   : {high}")
    print(f"  low  (conf <  0.7)    : {low}")
    print(f"  blank                 : {blank}")
    print(f"  output: {args.out_csv}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
