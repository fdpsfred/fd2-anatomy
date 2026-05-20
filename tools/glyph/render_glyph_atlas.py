"""Render the FD2 chinese_font_sheet (FDOTHER.DAT[4]) 1824 glyphs as PNG atlas
images. Each glyph is 1bpp 16x16 (32 bytes packed MSB first).

Outputs to --out-dir:
    - font_atlas_1x.png         32 cols x 57 rows grid (16x16 cells)
    - font_atlas_8x.png         8x upscaled version for OCR / human review
    - font_atlas_page_NN.png    19 paginated atlases (8x12 = 96 glyphs each,
                                4x upscale, with glyph_id labels)
    - glyph_id_index.csv        glyph_id -> (page_no, row_in_page, col_in_page)

Requires Pillow (`pip install pillow`).

CLI:
    python tools/glyph/render_glyph_atlas.py --out-dir ./glyph_atlas
"""
from __future__ import annotations

import argparse
import csv
import struct
import sys
from pathlib import Path

# tools/glyph/render_glyph_atlas.py -> glyph -> tools -> repo root
REPO_ROOT = Path(__file__).resolve().parents[2]
FDOTHER_PATH = REPO_ROOT / "fd2_game_files" / "FDOTHER.DAT"
DEFAULT_OUT_DIR = REPO_ROOT / "workspace" / "glyph" / "atlas"

GLYPH_W = 16
GLYPH_H = 16
GLYPH_BYTES = 32
TOTAL_GLYPHS = 1824
GRID_COLS = 32
GRID_ROWS = (TOTAL_GLYPHS + GRID_COLS - 1) // GRID_COLS  # 57

PAGE_COLS = 8
PAGE_ROWS = 12
PAGE_GLYPHS = PAGE_COLS * PAGE_ROWS  # 96
N_PAGES = (TOTAL_GLYPHS + PAGE_GLYPHS - 1) // PAGE_GLYPHS  # 19


def read_dat_entry(path: Path, idx: int) -> bytes:
    raw = path.read_bytes()
    first_off = struct.unpack_from("<I", raw, 6)[0]
    n = (first_off - 6) // 4
    if idx >= n - 1:
        raise ValueError(f"idx {idx} out of range (n={n})")
    start = struct.unpack_from("<I", raw, 6 + idx * 4)[0]
    end = struct.unpack_from("<I", raw, 6 + (idx + 1) * 4)[0]
    return raw[start:end]


def main() -> int:
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("--out-dir", type=Path, default=DEFAULT_OUT_DIR,
                   help="directory to write atlas PNGs and index CSV. "
                   f"default: workspace/glyph/atlas")
    args = p.parse_args()

    try:
        from PIL import Image, ImageDraw, ImageFont
    except ImportError:
        sys.stderr.write(
            "ERROR: Pillow not installed. Install with: pip install pillow\n"
        )
        return 2

    args.out_dir.mkdir(parents=True, exist_ok=True)

    raw = read_dat_entry(FDOTHER_PATH, 4)
    print(f"chinese_font_sheet: {len(raw)} bytes")
    if len(raw) != TOTAL_GLYPHS * GLYPH_BYTES:
        print(f"WARN: expected {TOTAL_GLYPHS * GLYPH_BYTES}, got {len(raw)}")

    glyphs = []
    for i in range(TOTAL_GLYPHS):
        gb = raw[i * GLYPH_BYTES:(i + 1) * GLYPH_BYTES]
        img = Image.new("L", (GLYPH_W, GLYPH_H), 0)
        pixels = img.load()
        for row in range(GLYPH_H):
            b1 = gb[row * 2]
            b2 = gb[row * 2 + 1]
            word = (b1 << 8) | b2  # MSB first
            for col in range(GLYPH_W):
                if word & (1 << (15 - col)):
                    pixels[col, row] = 255
        glyphs.append(img)

    atlas_1x = Image.new("L", (GRID_COLS * GLYPH_W, GRID_ROWS * GLYPH_H), 0)
    for i, g in enumerate(glyphs):
        col = i % GRID_COLS
        row = i // GRID_COLS
        atlas_1x.paste(g, (col * GLYPH_W, row * GLYPH_H))
    atlas_1x.save(args.out_dir / "font_atlas_1x.png", "PNG")
    print(f"  wrote font_atlas_1x.png ({atlas_1x.width}x{atlas_1x.height})")

    atlas_8x = atlas_1x.resize((atlas_1x.width * 8, atlas_1x.height * 8),
                                Image.NEAREST)
    atlas_8x.save(args.out_dir / "font_atlas_8x.png", "PNG")
    print(f"  wrote font_atlas_8x.png ({atlas_8x.width}x{atlas_8x.height})")

    SCALE = 4
    CELL = GLYPH_W * SCALE
    LABEL_H = 14
    PAGE_W = PAGE_COLS * CELL
    PAGE_H = PAGE_ROWS * (CELL + LABEL_H)

    csv_path = args.out_dir / "glyph_id_index.csv"
    with open(csv_path, "w", newline="", encoding="utf-8") as csvf:
        writer = csv.writer(csvf)
        writer.writerow(["glyph_id_dec", "glyph_id_hex", "page_no",
                         "row_in_page", "col_in_page"])
        try:
            font = ImageFont.truetype("arial.ttf", 11)
        except (IOError, OSError):
            font = ImageFont.load_default()
        for page_no in range(N_PAGES):
            img = Image.new("RGB", (PAGE_W, PAGE_H), (255, 255, 255))
            draw = ImageDraw.Draw(img)
            for slot in range(PAGE_GLYPHS):
                gidx = page_no * PAGE_GLYPHS + slot
                if gidx >= TOTAL_GLYPHS:
                    break
                col_in_page = slot % PAGE_COLS
                row_in_page = slot // PAGE_COLS
                gimg = glyphs[gidx].resize((CELL, CELL), Image.NEAREST).convert("RGB")
                x = col_in_page * CELL
                y = row_in_page * (CELL + LABEL_H) + LABEL_H
                img.paste(gimg, (x, y))
                draw.text((x, row_in_page * (CELL + LABEL_H)),
                          f"0x{gidx:04X}", fill=(0, 0, 0), font=font)
                writer.writerow([gidx, f"0x{gidx:04X}", page_no,
                                 row_in_page, col_in_page])
            img.save(args.out_dir / f"font_atlas_page_{page_no:02d}.png", "PNG")
    print(f"  wrote {N_PAGES} per-page atlases (font_atlas_page_NN.png)")
    print(f"  wrote {csv_path}")

    nonblank = sum(1 for g in glyphs if any(g.getdata()))
    print(f"  non-blank glyphs: {nonblank}/{TOTAL_GLYPHS}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
