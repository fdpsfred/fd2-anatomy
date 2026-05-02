# DATO.DAT — 80×80 portrait sprites (4 views each)

對話 / 選單 / status screen 用的靜態 portrait。
file size 1,979,029 bytes，136 entries (idx 0..135)。

## 檔案格式

LLLLLL archive (詳 `overview.md`)。

## Entry format

每 entry = 1 portrait × 4 views，0x14 byte header + 4 個獨立 RLE-encoded frames：

```
+0x00  u32 LE  frame_a_offset   (relative to entry start; first frame = 0x14)
+0x04  u32 LE  frame_b_offset
+0x08  u32 LE  frame_c_offset
+0x0C  u32 LE  frame_d_offset
+0x10  u16 LE  width             (= 0x0050 = 80, 全 136 entries 相同)
+0x12  u16 LE  height            (= 0x0050 = 80)
+0x14  bytes   RLE-encoded frame A data
+...   bytes   frame B/C/D
```

每 frame 是獨立 RLE-encoded 80×80 pixel 8bpp indexed image (RLE format 同
`rle_blit_sprite @ 0x4E63D`，詳 `program_info/graphics.md`)。

4 frames 用途推測為 portrait 的 4 個表情或視角 (normal/smile/sad/special 或
north/south/east/west)。

## idx 公式

`DATO idx = portrait_id` (直接對應，全 136 entries 各為 1 個 portrait)。

Caller chain:
- `display_dialog_scene @ 0x16ABC` — dialog 講者 portrait blit
- `load_chapter_portrait @ 0x11019` — 200KB linear-probe portrait cache
- `render_status_screen_static_layout` — status screen char portrait
- `run_equip_member_menu` / `run_status_screen_member_menu` — menu portraits
- `play_final_chapter_30_ending` — endgame char portraits

`display_dialog_scene` 內部用 `portrait_id × 0x50` 做 stride 計算 (80-byte row)，
但 loader 端傳的 idx 是 `portrait_id` 直接。

## 與 FIGANI 的對應

- DATO 136 entries = 136 portraits (static, 4 views)
- FIGANI 408 entries = 136 portraits × 3 frame variants (animation)
- **FIGANI[portrait_id × 3 + 0/1/2] 對應 DATO[portrait_id]**

DATO 提供 dialog / menu / status screen 用的靜態 portrait；FIGANI 提供戰鬥 /
特殊技 / cinematic 用的動畫 frame。兩者共用同一 portrait_id namespace。

## Sample entry stats

| idx | size | frame offsets | unique colors |
|---|---|---|---|
| 0   | 14670 | [0x10, 0xE56, 0x1CA4, 0x2B0A] | 74 |
| 1   | 12665 | [0x10, 0xC5D, 0x18B8, 0x2528] | 83 |
| 2   | 15681 | [0x10, 0xF57, 0x1EA1, 0x2DFC] | 72 |
| 3   | 14059 | [0x10, 0xDC2, 0x1B7D, 0x293A] | 49 |
| 4   | 13424 | [0x10, 0xD1D, 0x1A3A, 0x276B] | 84 |
| 50  | 17706 | [0x10, 0x1151, 0x229D, 0x33E9] | 64 |
| 100 |  6165 | [0x10, 0x61D, 0xC2A, 0x1237] | 71 |
| 135 | 15083 | [0x10, 0xED8, 0x1D93, 0x2C30] | 101 |

每個 PGM dump 是 320×80 (4 views side-by-side) 的 8bpp indexed sprite (palette
為 chapter-dependent VGA palette)。

## 工具

- 解碼：`tools/decoders/dato_decoder.py`
