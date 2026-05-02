# tools/

Self-contained utilities for working with FD2's resource archives, font/glyph
data, and runtime structures. Each script:

- has a one-line module docstring describing its purpose
- carries its own path constants (no shared `_paths.py`, no shared `lib/`)
- does not import anything from `legacy/tools/`
- does not read or write `catalog/*.json`

The scripts read the original `.DAT` / `.B24` / `.LE` resource files directly
from the FLAME2 game folder.

## decoders/

Parsers and decoders for the LLLLLL-format DAT archives shipped with FD2 plus
the FDICON.B24 icon archive.

- `dat_header_parser.py` - generic LLLLLL DAT archive header parser; provides
  `parse_dat_header()` + `read_dat_entry()` helpers and a CLI summary across
  all 11 known DAT files.

  ```bash
  python tools/decoders/dat_header_parser.py --list
  python tools/decoders/dat_header_parser.py --file FDOTHER.DAT --idx 4
  ```

  Verified: parses all 11 archives without errors; `--idx 4` of FDOTHER
  hexdumps the chinese_font_sheet first 64 bytes.
- `rle_decoder.py` - reusable rle_blit_sprite RLE decoder library + CLI.
  Includes `rle_decode()`, `write_pgm_indexed()`, `palette_to_color_ppm()`.

  ```bash
  python tools/decoders/rle_decoder.py --self-test
  python tools/decoders/rle_decoder.py --decode-fdother 0x36 --out /tmp/x.pgm
  ```

  Verified: self-test passes (skip/literal/rle/stretch modes round-trip);
  FDOTHER[0x65] palette confirmed at 768 bytes; FDOTHER[0x36] decodes to
  64000 pixels for 320x200 cinematic image.
- `fdtxt_parser.py` - FDTXT.DAT parser + dialog VM tokeniser.

  ```bash
  python tools/decoders/fdtxt_parser.py --info
  python tools/decoders/fdtxt_parser.py --pages 1
  python tools/decoders/fdtxt_parser.py --tokenize 1 0
  ```

  Verified: 34 entries, 146-byte header, sanity check PASS (payload + header
  == file size).
- `fdtxt_dialog_decoder.py` - renders any FDTXT entry as markdown with inline
  control-code annotations. Optional `--readable` mode substitutes Chinese
  characters via a user-supplied glyph lookup JSON.

  ```bash
  python tools/decoders/fdtxt_dialog_decoder.py --list
  python tools/decoders/fdtxt_dialog_decoder.py --idx 1
  python tools/decoders/fdtxt_dialog_decoder.py --idx 1 --readable \
      --glyph-table /path/to/glyph_lookup.json
  ```

  Verified: `--list` shows 34 entries with page counts; `--idx 1` produces
  valid markdown with [PORTRAIT], [PAGE_BREAK], `<NNNN>` glyph annotations.
- `fdfield_event_decoder.py` - extracts the 16 turn-event hooks from each
  chapter's FDFIELD tile_event entry and resolves event_code to handler
  addresses via the chapter_event jump table at 0x00051B91.

  ```bash
  python tools/decoders/fdfield_event_decoder.py --chapter 1
  python tools/decoders/fdfield_event_decoder.py --all
  python tools/decoders/fdfield_event_decoder.py --dump-opcodes
  ```

  Verified: chapter 1 decodes 4 active turn events at turns 3..6, mapped to
  handlers @ 0x000341DB / 0x000342B5 / 0x0003431D / 0x00034377.
- `fdfield_char_spawn_decoder.py` - decodes per-chapter character spawn
  records (stride 0x1A) including team/class/level/AI class/inventory.

  ```bash
  python tools/decoders/fdfield_char_spawn_decoder.py --chapter 1
  python tools/decoders/fdfield_char_spawn_decoder.py --all
  ```

  Verified: chapter 1 reports 30 char records starting at +0x83, including
  tile_pickup table and tile-step-event hooks.
- `fdfield_layout_inspector.py` - byte-level layout proof for every FDFIELD
  tile_event entry: counts active hooks/pickups/char records and validates
  size = 0x83 + N*0x1A.

  ```bash
  python tools/decoders/fdfield_layout_inspector.py --all
  python tools/decoders/fdfield_layout_inspector.py --chapter 5
  ```

  Verified: chapter 5 shows shap_id=4, char_count=50, size 1431 = expected
  exact; 31/33 chapters exact match, 1 +1 reserved record, 1 mismatch.
- `fdshap_decoder.py` - decodes the per-shap_id battle scene snapshot (RLE
  320x200) + tile_attribute_flags (4 bytes/tile).

  ```bash
  python tools/decoders/fdshap_decoder.py --list
  python tools/decoders/fdshap_decoder.py --shap-id 0 --out-dir /tmp/shap0
  ```

  Verified: 66 entries (shap_id 0..32 x 2); shap_id 0 snapshot decodes to
  64000 pixels with 250 unique palette indices.
- `dato_decoder.py` - decodes the 4-frame 80x80 portrait sprite stored in
  each DATO entry (136 portraits total).

  ```bash
  python tools/decoders/dato_decoder.py --list
  python tools/decoders/dato_decoder.py --idx 0 --out-dir /tmp/dato0
  python tools/decoders/dato_decoder.py --samples 0,1,50,100
  ```

  Verified: DATO[0] header offsets [0x10, 0xE56, 0x1CA4, 0x2B0A]; all 4
  frames decode to full 80x80 = 6400 pixels.
- `fdmus_xmi_extractor.py` - extracts Miles XMIDI sequences from FDMUS
  entries and prints the per-chapter BGM mapping table.

  ```bash
  python tools/decoders/fdmus_xmi_extractor.py --list
  python tools/decoders/fdmus_xmi_extractor.py --extract --out-dir /tmp/fdmus
  python tools/decoders/fdmus_xmi_extractor.py --bgm-table
  ```

  Verified: 14 IFF/XMI tracks + 6 placeholders extracted; 19 .xmi files
  written; BGM table prints all 30 chapters with player_turn/enemy_turn idx.
- `bg_tai_decoder.py` - decodes the 320x100 RLE background image from
  BG.DAT[0] and inspects TAI.DAT entry sizes.

  ```bash
  python tools/decoders/bg_tai_decoder.py --list
  python tools/decoders/bg_tai_decoder.py --decode-bg 0 --out-dir /tmp/bg
  python tools/decoders/bg_tai_decoder.py --tai-summary
  ```

  Verified: BG.DAT[0] 1004 bytes, header dim 320x100, decodes; TAI.DAT
  histogram shows 16 entries of size 7 (sentinels) plus larger entries.
- `fdicon_decoder.py` - decodes 24x24 RLE icons from FDICON.B24 (1680
  active entries).

  ```bash
  python tools/decoders/fdicon_decoder.py --info
  python tools/decoders/fdicon_decoder.py --samples 0,1,2,840,1679 --out-dir /tmp/fdicon
  ```

  Verified: header reports width=24, height=24, count=1680; offsets
  monotonic, sentinel matches file size; sample icons decode to 576 pixels.

## glyph/

Tools for working with the FD2 chinese_font_sheet (FDOTHER.DAT[4]) and ET3
font tables.

- `render_glyph_atlas.py` - renders the 1824 glyphs of FDOTHER[4] as PNG
  atlas images (1x grid, 8x upscaled, plus 19 paginated atlases with
  glyph_id labels). Requires Pillow.

  ```bash
  python tools/glyph/render_glyph_atlas.py --out-dir /tmp/glyph_atlas
  ```

  Verified: writes font_atlas_1x.png (512x912), font_atlas_8x.png
  (4096x7296), 19 per-page atlases, glyph_id_index.csv; reports 1823/1824
  non-blank glyphs.
- `et3_pixel_match.py` - for each FD2 glyph, finds the closest matching
  ET3 STDFONT.15 / ASCFONT.15 glyph and emits a CSV of glyph_id ->
  Big5 character + confidence.

  ```bash
  python tools/glyph/et3_pixel_match.py --out-csv /tmp/glyph_match.csv
  python tools/glyph/et3_pixel_match.py --out-csv /tmp/quick.csv --limit 50
  ```

  Verified: `--limit 50` finds 14 perfect (hamming=0) matches for the first
  50 glyphs against the bundled ET3_fonts/STDFONT.15 + ASCFONT.15.

### glyph/ET3_fonts/

Bundled ET3 font tables used by `et3_pixel_match.py`:

- `ASCFONT.15` - 256 x 8x15 ASCII bitmap glyphs (3840 bytes).
- `STDFONT.15` - 13094 x 16x15 Big5 bitmap glyphs (392820 bytes).

Layout discovered/verified during the matching project; see
`et3_pixel_match.py` docstring for the Big5 lead/tail mapping rules
(Lead 0xA4..0xC5 = 157 each; Lead 0xC6 = 63 only; Lead 0xC9..0xF8 = 157
each; Lead 0xF9 = 116; final 41 are ETEN extension chars in PUA).
