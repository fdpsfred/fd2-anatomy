"""FD2 sprite RLE decoders (ports of src/gfx/blitspr.c). The data uses TWO
distinct stream formats; this module decodes both.

(1) fd2_rle_blit_sprite  -> rle_decode / rle_decode_sized
    Used by FDOTHER sprites, FIGANI, BG, FDICON, and the FDSHAP battle-tile
    sheets (each tile a command-only 24x24 stream). All verified by round-trip
    render. Details below.

(2) fd2_decode_dialog_pixel_byte  -> dialog_pixel_decode
    A different run encoding for portrait pixels: a byte <= 0xC0 is one literal
    pixel; a byte b in 0xC1..0xFF starts a run of the FOLLOWING byte's value,
    length (b-0xC1)+1 (1..63). Used by DATO portraits and dialog portraits.

Ground truth for (1): fd2_rle_blit_sprite @ 0x4E63D. A command byte's high 2
bits select the op; the low 6 bits are len-1, so len = (cmd & 0x3F) + 1:

    0b00  RLE fill        write the NEXT byte to `len` dst pixels
    0b01  stretched fill  write the NEXT byte to `len` pixels spaced every other
                          pixel (dst += 2 each), consuming 2*len row columns
    0b10  literal copy    copy `len` literal bytes from the stream
    0b11  skip            advance dst by `len` pixels (transparent; dst unchanged)

Decoding is the passthrough case (palette_op = 0xFFFFFFFF); the translucent /
silhouette palette ops of fd2_rle_blit_sprite are blit-time recolour effects,
not part of the stored pixels, so they are irrelevant to extracting a sprite.
A tight decode uses stride == width, so the 2D row loop collapses to a flat
pixel cursor (row_advance = stride - width = 0).

Two framings occur in the data; the RLE COMMAND format is identical in both:
  * self-describing  -- stream = [width u16 LE][height u16 LE][commands]
                        (native fd2_rle_blit_sprite; also the FIGANI pose
                        sub-sprite at pose+9). Use rle_decode_sized.
  * command-only     -- caller already knows width*height and passes just the
                        commands (BG strips its 4-byte [w][h] header; FDICON
                        uses a global 24x24 size; each FDSHAP tile is a fixed
                        24x24 stream). Use rle_decode.

Output is raw 8bpp indexed pixels; palette decoding is the caller's job
(FDOTHER[0] is the 768-byte 6-bit VGA base palette; FDOTHER[0x65] is the ending
palette). load_vga_palette_6bit + write_ppm_color render indexed -> RGB.

Usage:
    from rle_decoder import rle_decode, rle_decode_sized, write_pgm_indexed
    w, h, pixels, mask = rle_decode_sized(sprite_stream)          # [w][h][cmds]
    pixels = rle_decode(command_stream, max_pixels=width*height)  # cmds only

CLI:
    python tools/decoders/rle_decoder.py --self-test
    python tools/decoders/rle_decoder.py --decode-fdother 0x36 --sized --out out.pgm
"""
from __future__ import annotations

import argparse
import struct
import sys
from pathlib import Path

# tools/decoders/rle_decoder.py -> decoders -> tools -> repo root
REPO_ROOT = Path(__file__).resolve().parents[2]
FDOTHER_PATH = REPO_ROOT / "fd2_game_files" / "FDOTHER.DAT"
DEFAULT_OUT_PATH = REPO_ROOT / "workspace" / "decoders" / "rle" / "rle_decoded.pgm"


def _decode_core(cmds: bytes, max_pixels: int, transparent: int = 0):
    """Faithful passthrough command decode (see module docstring) into a tight
    row-major buffer (stride == width -> flat cursor). Returns
    (buf: bytearray[max_pixels], mask: bytearray[max_pixels], produced: int)
    where produced = pixel columns advanced (<= max_pixels) and mask[i] == 1
    marks a pixel actually written (0 = transparent / skipped)."""
    buf = bytearray([transparent & 0xFF]) * max_pixels
    mask = bytearray(max_pixels)
    n = len(cmds)
    i = 0
    dst = 0
    while dst < max_pixels and i < n:
        cmd = cmds[i]
        i += 1
        length = (cmd & 0x3F) + 1
        op = cmd & 0xC0
        if op == 0x00:                      # RLE fill: NEXT byte x len
            if i >= n:
                break
            b = cmds[i]
            i += 1
            hi = dst + length if dst + length < max_pixels else max_pixels
            for p in range(dst, hi):
                buf[p] = b
                mask[p] = 1
            dst += length
        elif op == 0x40:                    # stretched fill: NEXT byte, every other
            if i >= n:
                break
            b = cmds[i]
            i += 1
            for _ in range(length):
                p = dst + 1
                if p < max_pixels:
                    buf[p] = b
                    mask[p] = 1
                dst += 2
        elif op == 0x80:                    # literal copy: len bytes
            for _ in range(length):
                if dst >= max_pixels or i >= n:
                    break
                buf[dst] = cmds[i]
                mask[dst] = 1
                i += 1
                dst += 1
        else:                               # 0xC0 skip: advance len (transparent)
            dst += length
    return buf, mask, min(dst, max_pixels)


def rle_decode(src: bytes, max_pixels: int, transparent: int = 0) -> bytes:
    """Decode a header-less RLE COMMAND stream to indexed pixels.

    For callers that already know the pixel budget (max_pixels = width*height)
    and pass just the command bytes. Skipped / stretched-gap pixels take the
    `transparent` value (default 0). Returns bytes of length == columns
    produced (<= max_pixels), preserving the historical variable-length
    contract (callers pad to width*height as needed).
    """
    buf, _mask, produced = _decode_core(src, max_pixels, transparent)
    return bytes(buf[:produced])


def rle_decode_sized(stream: bytes, transparent: int = 0):
    """Decode a self-describing sprite stream: [width u16 LE][height u16 LE]
    [command bytes...] -- the native fd2_rle_blit_sprite framing (also each
    DATO frame and the FIGANI pose sub-sprite at pose+9).

    Returns (width, height, pixels: bytes[w*h], mask: bytes[w*h]). Skipped and
    stretched-gap pixels are transparent (mask byte 0); every written pixel has
    mask byte 1, which lets callers render true transparency (an opaque pixel
    can legitimately be palette index 0).
    """
    width = stream[0] | (stream[1] << 8)
    height = stream[2] | (stream[3] << 8)
    buf, mask, _ = _decode_core(stream[4:], width * height, transparent)
    return width, height, bytes(buf), bytes(mask)


def dialog_pixel_decode(stream: bytes, num_pixels: int) -> bytes:
    """Decode the portrait pixel-run encoding of fd2_decode_dialog_pixel_byte
    @ 0x4E916 (src/gfx/blitspr.c) -- a DIFFERENT format from fd2_rle_blit_sprite.

    A byte <= 0xC0 is one literal pixel of that value; a byte b in 0xC1..0xFF
    starts a run of the FOLLOWING byte's value, of length (b - 0xC1) + 1 (so
    1..63). Pixel values 0xC1..0xFF therefore only occur as a run's value byte,
    never as a bare literal. Used by DATO portrait frames (each frame is
    [w u16][h u16] then this stream) and by dialog portrait sprites.

    num_pixels = width*height. Returns bytes of length == pixels produced
    (<= num_pixels); a short stream simply yields fewer pixels.
    """
    out = bytearray(num_pixels)
    i = 0
    d = 0
    n = len(stream)
    while d < num_pixels and i < n:
        b = stream[i]
        i += 1
        if b <= 0xC0:
            out[d] = b
            d += 1
        else:
            if i >= n:
                break
            val = stream[i]
            i += 1
            run = (b - 0xC1) + 1
            hi = d + run if d + run < num_pixels else num_pixels
            for p in range(d, hi):
                out[p] = val
            d += run
    return bytes(out[:min(d, num_pixels)])


def write_pgm_indexed(pixels: bytes, width: int, height: int, out_path: Path) -> None:
    """Write raw indexed pixels as a PGM P5 (8-bit grayscale; palette undecoded)."""
    out_path.parent.mkdir(parents=True, exist_ok=True)
    n = width * height
    if len(pixels) < n:
        pixels = pixels + bytes(n - len(pixels))
    with open(out_path, "wb") as f:
        f.write(f"P5\n{width} {height}\n255\n".encode("ascii"))
        f.write(pixels[:n])


def load_vga_palette_6bit(raw768: bytes):
    """Turn a 768-byte 6-bit VGA DAC palette (e.g. FDOTHER[0]) into 256 8-bit
    (r, g, b) tuples via the standard 6->8 bit expansion c8 = c6<<2 | c6>>4."""
    pal = []
    for i in range(256):
        if i * 3 + 2 < len(raw768):
            r, g, b = raw768[i*3], raw768[i*3+1], raw768[i*3+2]
        else:
            r = g = b = 0
        pal.append(((r << 2) | (r >> 4), (g << 2) | (g >> 4), (b << 2) | (b >> 4)))
    return pal


def write_ppm_color(pixels: bytes, width: int, height: int, palette,
                    out_path: Path, mask: bytes | None = None,
                    transparent_rgb=(0, 0, 0)) -> None:
    """Render indexed pixels to a color PPM P6 through an (r,g,b) palette. Where
    a mask is given, mask byte 0 paints transparent_rgb (so transparency is not
    confused with palette index 0)."""
    out_path.parent.mkdir(parents=True, exist_ok=True)
    n = width * height
    out = bytearray(n * 3)
    for p in range(min(n, len(pixels))):
        if mask is not None and p < len(mask) and mask[p] == 0:
            r, g, b = transparent_rgb
        else:
            r, g, b = palette[pixels[p]]
        out[p*3] = r
        out[p*3+1] = g
        out[p*3+2] = b
    with open(out_path, "wb") as f:
        f.write(f"P6\n{width} {height}\n255\n".encode("ascii"))
        f.write(bytes(out))


def palette_to_color_ppm(palette_bytes: bytes, out_path: Path) -> None:
    """Render a 256-entry VGA palette (R6G6B6 DAC) as a 16x16 colour PPM swatch."""
    cell, grid = 16, 16
    w = h = grid * cell
    pal = load_vga_palette_6bit(palette_bytes)
    pixels = bytearray(w * h * 3)
    for i in range(256):
        r, g, b = pal[i]
        col, row = i % grid, i // grid
        for dy in range(cell):
            for dx in range(cell):
                off = ((row * cell + dy) * w + (col * cell + dx)) * 3
                pixels[off], pixels[off+1], pixels[off+2] = r, g, b
    out_path.parent.mkdir(parents=True, exist_ok=True)
    with open(out_path, "wb") as f:
        f.write(f"P6\n{w} {h}\n255\n".encode("ascii"))
        f.write(bytes(pixels))


def _read_dat_entry(path: Path, idx: int) -> bytes:
    """Local LLLLLL DAT extractor so this script is self-contained."""
    raw = path.read_bytes()
    first_off = struct.unpack_from("<I", raw, 6)[0]
    n = (first_off - 6) // 4
    if idx >= n - 1:
        raise ValueError(f"idx {idx} out of range (n={n - 1})")
    start = struct.unpack_from("<I", raw, 6 + idx * 4)[0]
    end = struct.unpack_from("<I", raw, 6 + (idx + 1) * 4)[0]
    return raw[start:end]


def _self_test() -> int:
    """Synthetic round-trips for the 4 ops (matching fd2_rle_blit_sprite), plus a
    live cross-check that the two framings agree on a real FIGANI sprite."""
    # skip: 0xFF -> len 64 transparent (0)
    assert rle_decode(bytes([0xFF]), 128) == b"\x00" * 64, "skip"
    # literal copy: 0x82 (0b10, len 3) + 3 bytes
    assert rle_decode(bytes([0x82, 0x11, 0x22, 0x33]), 128) == b"\x11\x22\x33", "literal"
    # RLE fill: 0x04 (0b00, len 5) + one byte -> 5 copies
    assert rle_decode(bytes([0x04, 0xAB]), 128) == b"\xAB" * 5, "rle-fill"
    # stretched fill: 0x42 (0b01, len 3) + one byte -> every other pixel
    assert rle_decode(bytes([0x42, 0x10]), 128) == b"\x00\x10\x00\x10\x00\x10", "stretched"
    # len = (cmd & 0x3F) + 1 boundary: 0x00 -> RLE fill of 1
    assert rle_decode(bytes([0x00, 0x77]), 8) == b"\x77", "len-min"
    # rle_decode_sized parses the [w][h] header
    w, h, px, mk = rle_decode_sized(bytes([4, 0, 1, 0, 0x83, 1, 2, 3, 4]))
    assert (w, h) == (4, 1) and px == b"\x01\x02\x03\x04" and mk == b"\x01\x01\x01\x01", "sized"
    # dialog-pixel: literal 0x05, then a run (0xC3 -> len 3) of value 0x2A
    assert dialog_pixel_decode(bytes([0x05, 0xC3, 0x2A]), 8) == b"\x05\x2A\x2A\x2A", "dialog-pixel"
    print("rle self-test: PASS (skip / literal / rle-fill / stretched / len / "
          "sized / dialog-pixel)")

    fig = REPO_ROOT / "fd2_game_files" / "FIGANI.DAT"
    if fig.exists():
        entry = _read_dat_entry(fig, 0x99)                 # Hano frame_a
        pose0 = struct.unpack_from("<I", entry, 8)[0]
        sub = entry[pose0 + 9:]                             # [w][h][cmds]
        w, h, px, mk = rle_decode_sized(sub)
        # command-only path over the same commands must agree on the sprite body
        cmd_only = rle_decode(entry[pose0 + 13:], w * h)
        assert (w, h) == (102, 140), f"figani dims {w}x{h}"
        assert cmd_only == px[:len(cmd_only)], "framing paths disagree"
        opaque = sum(mk)
        assert 0 < opaque < w * h, "sprite should be part opaque, part transparent"
        print(f"live cross-check: FIGANI[0x99] pose0 = {w}x{h}, "
              f"{opaque}/{w*h} opaque px, both framings agree")
    else:
        print("(FIGANI.DAT absent; skipped live cross-check)")
    return 0


def main() -> int:
    p = argparse.ArgumentParser(description="FD2 fd2_rle_blit_sprite decoder")
    g = p.add_mutually_exclusive_group(required=True)
    g.add_argument("--self-test", action="store_true",
                   help="synthetic op round-trips + live FIGANI cross-check")
    g.add_argument("--decode-fdother", type=lambda s: int(s, 0), metavar="IDX",
                   help="decode FDOTHER[IDX] and write a PGM")
    p.add_argument("--sized", action="store_true",
                   help="parse a leading [w u16][h u16] header (self-describing "
                        "stream) instead of --width/--height")
    p.add_argument("--width", type=int, default=320)
    p.add_argument("--height", type=int, default=200)
    p.add_argument("--palette", action="store_true",
                   help="also emit a color PPM using the FDOTHER[0] VGA palette")
    p.add_argument("--out", type=Path, default=DEFAULT_OUT_PATH,
                   help="output PGM path (default workspace/decoders/rle/)")
    args = p.parse_args()

    if args.self_test:
        return _self_test()

    blob = _read_dat_entry(FDOTHER_PATH, args.decode_fdother)
    if args.sized:
        w, h, pixels, mask = rle_decode_sized(blob)
    else:
        w, h = args.width, args.height
        pixels = rle_decode(blob, max_pixels=w * h)
        mask = None
    write_pgm_indexed(pixels, w, h, args.out)
    print(f"wrote {args.out} ({w}x{h}, {len(pixels)} decoded pixels)")
    if args.palette:
        pal = load_vga_palette_6bit(_read_dat_entry(FDOTHER_PATH, 0))
        ppm = args.out.with_suffix(".ppm")
        write_ppm_color(pixels, w, h, pal, ppm, mask)
        print(f"wrote {ppm} (color)")
    return 0


if __name__ == "__main__":
    sys.exit(main())
