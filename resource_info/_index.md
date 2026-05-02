# resource_info/

對 FD2 各遊戲資源檔的格式解析。

## 檔案

- `overview.md` — 11 個 LLLLLL DAT 統一格式 + FDICON.B24 + FD2.SAV 總覽
- `fd2_le_format.md` — FD2.LE binary 結構 (DOS LE / DOS4GW)、三大 object segment、
  與 FD2.EXE 跨版本偏移、library 邊界
- `save_format.md` — FD2.SAV 22987 bytes layout、save header、4-slot snapshot、
  XOR 加密與 4-byte checksum
- `fdtxt.md` — FDTXT.DAT 對話文字 (34 entries, 1016 pages)、bytecode VM
  10 個 control opcode、entry 0..33 用途對照
- `fdfield.md` — FDFIELD.DAT (99 entries) 章節地圖 + tile_event + char_spawn
  完整 byte-level layout
- `fdshap.md` — FDSHAP.DAT (66 entries) 戰鬥 snapshot + tile_attribute
  33 章 × 2 idx 對照
- `fdmus.md` — FDMUS.DAT (20 entries: 15 XMI + 5 placeholder)、
  `set_bgm_track_with_fade` dispatcher、per_chapter BGM 表
- `fdother.md` — FDOTHER.DAT 103 entries (含 29 nested sub-archive)、
  分類統計、21 個 confirmed_dead idx
- `dato.md` — DATO.DAT 80×80 portrait sprite (136 entries × 4 view)
- `figani.md` — FIGANI.DAT (408 entries) 必殺技 / 召喚動畫 byte-stream
- `bg.md` — BG.DAT (56 entries) 320×100 cinematic / battle BG (count/color RLE)
- `tai.md` — TAI.DAT (56 entries) terrain overlay / AI 配對資料 (與 BG 配對)
- `ani.md` — ANI.DAT (9 entries) 多 frame RLE delta 動畫序列
- `title.md` — TITLE.DAT (7 entries) — 對 FD2 主遊戲是 dead resource
- `fdicon.md` — FDICON.B24 (1680 個 24×24 8bpp icon)，唯一非 LLLLLL 資源
- `chinese_glyph_encoding.md` — FDOTHER.DAT[4] 1bpp 字模 atlas (1824 glyphs)、
  渲染管線、ET3 STDFONT.15 lookup
