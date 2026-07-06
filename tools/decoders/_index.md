# tools/decoders/

LLLLLL DAT archive parser + 各遊戲資源檔解碼器。

## 檔案

- `dat_header_parser.py` — 通用 LLLLLL DAT archive header parser；提供
  `parse_dat_header()` + `read_dat_entry()` helpers + CLI summary。
- `rle_decoder.py` — `fd2_rle_blit_sprite` RLE 解碼 library + CLI（忠實對齊
  `src/gfx/blitspr.c`：`len=(cmd&0x3F)+1`；高 2 bits 選 op — `0b00` RLE fill /
  `0b01` stretched fill / `0b10` literal copy / `0b11` skip）。提供
  `rle_decode()`（command-only 串流 + `max_pixels`）、`rle_decode_sized()`
  （parse `[w u16][h u16]` self-describing 前綴，回傳 w/h/pixels/mask）、
  `write_pgm_indexed()`、`load_vga_palette_6bit()` + `write_ppm_color()`（用
  FDOTHER[0] 6-bit VGA palette 上色）、`palette_to_color_ppm()`。另提供
  `dialog_pixel_decode()`（DATO / dialog portrait 用的 `fd2_decode_dialog_pixel_byte`
  run 格式，非 rle_blit）。rle_blit 適用資源：FIGANI / BG / FDICON / FDOTHER sprite /
  FDSHAP tile（皆 round-trip render 驗證）。
- `fdtxt_parser.py` — FDTXT.DAT parser + dialog VM tokeniser。
- `fdtxt_dialog_decoder.py` — 從 FDTXT entry 渲染為 markdown，含 `[OPCODE]` 標註；
  optional `--readable` 模式可帶 `--glyph-table` 指定 lookup JSON。
- `fdfield_event_decoder.py` — FDFIELD turn-event hook table parser (16 × 3 byte)。
- `fdfield_char_spawn_decoder.py` — FDFIELD char_spawn_record (0x1A bytes/record) parser。
- `fdfield_layout_inspector.py` — FDFIELD per-chapter entry layout 統計與驗證。
- `fdshap_decoder.py` — FDSHAP.DAT 戰鬥地形 24×24 tile sheet decoder（解 `[24][24]
  [count][int32 offsets]` + 各 tile 的 command-only rle_blit 指令流，輸出 tile
  atlas PGM/PPM）+ 4-byte/tile attribute。
- `dato_decoder.py` — DATO.DAT 80×80 portrait decoder（4 表情 frame，每 frame
  `[w][h]` + **dialog-pixel** 格式 `fd2_decode_dialog_pixel_byte`，非 rle_blit）。
- `fdmus_xmi_extractor.py` — FDMUS.DAT XMI sequence 抽取工具。
- `bg_tai_decoder.py` — BG.DAT 320×100 `fd2_rle_blit_sprite` RLE 背景 + TAI.DAT 配對 decoder。
- `fdicon_decoder.py` — FDICON.B24 1680 個 24×24 8bpp icon RLE decoder。

每個 script 的完整 CLI 用法看 `python <script> --help`；所有檔案輸出預設目的地為 `workspace/decoders/<sub>/`。
