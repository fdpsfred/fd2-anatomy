# tools/decoders/

LLLLLL DAT archive parser + 各遊戲資源檔解碼器。

## 檔案

- `dat_header_parser.py` — 通用 LLLLLL DAT archive header parser；提供
  `parse_dat_header()` + `read_dat_entry()` helpers + CLI summary。
- `rle_decoder.py` — `rle_blit_sprite` RLE 解碼 library + CLI；提供
  `rle_decode()`, `write_pgm_indexed()`, `palette_to_color_ppm()`。
- `fdtxt_parser.py` — FDTXT.DAT parser + dialog VM tokeniser。
- `fdtxt_dialog_decoder.py` — 從 FDTXT entry 渲染為 markdown，含 `[OPCODE]` 標註；
  optional `--readable` 模式可帶 `--glyph-table` 指定 lookup JSON。
- `fdfield_event_decoder.py` — FDFIELD turn-event hook table parser (16 × 3 byte)。
- `fdfield_char_spawn_decoder.py` — FDFIELD char_spawn_record (0x1A bytes/record) parser。
- `fdfield_layout_inspector.py` — FDFIELD per-chapter entry layout 統計與驗證。
- `fdshap_decoder.py` — FDSHAP.DAT 320×200 RLE snapshot + 4-byte/tile attribute decoder。
- `dato_decoder.py` — DATO.DAT 80×80 4-view portrait sprite decoder。
- `fdmus_xmi_extractor.py` — FDMUS.DAT XMI sequence 抽取工具。
- `bg_tai_decoder.py` — BG.DAT count/color RLE + TAI.DAT 配對 decoder。
- `fdicon_decoder.py` — FDICON.B24 1680 個 24×24 8bpp icon RLE decoder。

完整 CLI 用法與驗證範例見 `tools/README.md`。
