# gfx

DOS mode 13h (320×200×256-color)，primary surface @ `0xA0000` (linear)。
戰鬥畫面在一塊 456-byte pitch 的 render workspace 內合成，最後才 blit 到可見的
mode13h primary。工作 buffer 多用 0x140 (320)、0x1C8 (456) 或 0x280 (640) 寬度。

## 驗證對象

- 對應 src：`src/gfx/rndscene.c`（戰鬥 frame 合成/finalizer）、`src/gfx/blitspr.c`
  （RLE sprite decoder + primitive）、`src/gfx/blittile.c`（24×24 tile blitter）、
  `src/gfx/palette.c`（VGA palette）、`src/gfx/rndmenu.c`（選單/面板繪製）、
  `src/gfx/rndstat.c`（狀態面板繪製）。
- 主要 Ghidra 對象（位址即時核對）：`fd2_composite_battle_frame @ 0x11CAC`、
  `fd2_composite_battle_tile_map @ 0x11EEE`、`fd2_rle_blit_sprite @ 0x4E63D`、
  `fd2_blit_rectangle @ 0x11EB0`、`fd2_update_palette_cycle_anim @ 0x4DFCC`、
  `fd2_set_vga_palette_range @ 0x11D40`。
- 相關資源檔：FDSHAP.DAT（戰鬥場景 tile 與角色 sprite 圖庫）、FDOTHER.DAT（游標與
  UI overlay sprite），格式細節見 resource_info/。

## Render buffer 階層

```
0xA0000     VGA mode13h primary (visible)
0x53A49     large_game_state_buffer (0x25680 = 152 KB)
0x53A5D     data_fd2_battle_scene_tile_gfx_ptr (FDSHAP.DAT)
0x53AFF     static_bg_buffer (chapter background)
0x53B03     animated_bg_buffer (cycling chapters 9 / 0x18 / ...)
0x53C5B/5F/63   render_workspace_a/b/c (3 × 64000 bytes UI render)
```

## 核心 RLE 解碼器：`fd2_rle_blit_sprite @ 0x4E63D`

所有 sprite 繪製的核心。每個 byte 高 2 bit 編碼操作 + 低 6 bit 編碼長度 (+1)：

| Code | 操作 |
|---|---|
| `0b00xxxxxx` | RLE fill：後續 1 byte 重複填 len 個像素 |
| `0b01xxxxxx` | stretched fill：後續 1 byte 寫入 len 個「隔一」像素 (寫 dst+1、dst 每次 +2)，佔 2×len 欄 |
| `0b10xxxxxx` | literal copy：從串流複製 len 個 byte |
| `0b11xxxxxx` | skip len 個像素 (透明) |

每行寬度由 `data_fd2_graphics_rle_blit_cur_width @ 0x627B4` 計數，遇 0 換行並
`data_fd2_graphics_rle_blit_remaining_rows @ 0x627B6 --`。

### Palette 模式 (`fd2_rle_blit_sprite` 的 `param_6`)

| 值 | 模式 |
|---|---|
| `0xFFFFFFFF` | passthrough (直接複製) |
| `> 0xFF` | translucent overlay：`((src + (op>>8)) & 7) + (op & 0xFF)` |
| `≤ 0xFF` | silhouette：所有不透明像素都換成 `op & 0xFF` 的單一顏色 |

## 通用 primitive

| 位址 | 名稱 | 簽章 / 用途 |
|---|---|---|
| `0x11EB0` | `fd2_blit_rectangle` | 通用 2D copy `(dst, dst_pitch, src, src_pitch, w_bytes, h)` |
| `0x2935B` | `fd2_blit_indexed_sprite` | sheet+offset_table → `fd2_rle_blit_sprite` dispatch |
| `0x4E63D` | `fd2_rle_blit_sprite` | 核心 RLE decoder |
| `0x4E583` | `fd2_rle_blit_with_palette_remap` | RLE + 256-byte palette indirection |
| `0x4DCC6` | `fd2_tile_blit_24x24_remap` | 24×24 tile 專用 (含 palette table) |
| `0x4DEDA` | `fd2_tile_blit_24x24_passthrough` | 24×24 tile 專用 (透明變體) |
| `0x4E8AF` | `fd2_dialog_sprite_blit_normal` | dialog 專用左→右 pixel order |
| `0x4E8E1` | `fd2_dialog_sprite_blit_mirrored` | 右→左 pixel order (盟友面向左) |
| `0x4E916` | `fd2_decode_dialog_pixel_byte` | dialog 格式 per-pixel state machine |
| `0x15E9E` | `fd2_blit_indexed_sprite_with_alloc` | 含 malloc 的 wrapper |
| `0x15E71` | `fd2_cleanup_dialog_sprite_buffer` | 配對 free |

## Tile-map 渲染：`fd2_composite_battle_tile_map @ 0x11EEE`

戰鬥背景的主渲染器，含 chapter-aware 邏輯：

- 章節 9 / 0x18 / 0x19 / 0x1C / 0x1D：使用 `animated_bg_buffer`，每 BIOS tick 透過
  `fd2_blit_buffer_with_per_row_offset @ 0x4EB90` 以 per-row offset 表推進 16-frame
  循環 (frame idx 0..15，`data_fd2_graphics_bg_animation_frame_idx @ 0x539FC`)
- 章節 0x11 / 0x15 / 0x16 / 0x1B：extra-wide parallax 版本 (src stride 0x1CE 用於
  0x11/0x1B、0x198 用於 0x15/0x16；依 walk_anim x/y scroll 取來源偏移)
- 章節 0x17：特殊，背景是文字畫面，每 tick 呼叫
  `fd2_scroll_text_screen_up_by_lines(0) @ 0x24D22` 自動向上捲動文字背景
- 預設：靜態背景，跳過此 pass

每個 tile 4 bytes 元資料 (位於 `battle_tile_map`)；屬性位元 (從
`tile_attribute_flags_buffer @ 0x53A69`) 控制：

- bit 0x04 → 每隔 frame 切換 +1 (一般動畫)
- bit 0x08 → 每隔 frame 切換 +2 (雙速動畫)
- bit 0x10 → chapter palette half-step

Tile-anim 計數器：`data_fd2_battle_tile_map_anim_frame_counter @ 0x53C1F` (0..0x14，
每 3 BIOS tick 進 1、於 0x14 回捲)，做為 tile palette-remap phase 查表索引；可由
`data_fd2_graphics_forced_tile_anim_frame @ 0x51A93` (預設 sentinel 0xFFFFFFFF)
強制鎖定到特定 frame。`data_fd2_graphics_bg_anim_flip_flag @ 0x53A40` 是「每隔一
frame 翻轉」flip flag。

## Palette FX

| 位址 | 名稱 | 用途 |
|---|---|---|
| `0x11D40` | `fd2_set_vga_palette_range` | 寫 0x3C8/0x3C9 ports；含 brightness subtract |
| `0x1F882` | `fd2_play_palette_fade_to_black` | 64-frame 淡出 (~128 ms) |
| `0x4DFCC` | `fd2_update_palette_cycle_anim` | 16-color top-range 循環 (水/熔岩/火) |

VGA palette 來源：`vga_palette_data @ 0x53A65` — 768-byte 標準 256×3 RGB。

## Battle frame finalizer：`fd2_composite_battle_frame @ 0x11CAC`

戰鬥畫面的 frame finalizer，幾乎每個 UI 狀態變更後都被呼叫。合成目標是 render
workspace `ws = data_fd2_large_game_state_buffer_ptr + 0x8088`，最後才 blit 到 mode13h
primary。唯一引數是 `skip_palette_cycle`：0 = 本 frame 推進 palette 循環動畫；非 0 =
跳過（呼叫端自行掌控 palette 時序）。Pipeline（依 asm 順序）：

1. `fd2_tick_chapter_palette_animation @ 0x1297D` — 推進章節綁定的 palette 動畫計數
2. 若 `skip_palette_cycle == 0`：`fd2_update_palette_cycle_anim @ 0x4DFCC` — 推進
   水/熔岩/火的 VGA palette 循環（見下方 Palette FX）
3. `fd2_composite_battle_tile_map(ws, 456, 13, 8, origin_x, origin_y)` — 畫 13×8
   tile 背景到 workspace
4. `fd2_paint_cursor_overlay_pattern @ 0x122DC` — 游標高亮 / 範圍指示 overlay
5. `fd2_composite_all_chars_overlay @ 0x127A9` — 所有存活角色/敵人 sprite 層（含朝向
   與 status icon）
6. `fd2_render_terrain_info_hud_panel(ws, 456) @ 0x1ACF3` — 地形資訊 HUD 面板
7. `fd2_blit_rectangle(0xA0504, 320, ws, 456, 312, 192)` — 把合成好的 312×192 可見區
   blit 到 mode13h primary

像素常數：
- `0x8088` = render workspace 在 large_game_state_buffer 內的起點 (ws)
- `0x1C8 = 456` = render workspace pitch (含 padding)
- `0x140 = 320` = mode13h stride
- `0x138 = 312` = visible width clipping
- `0xC0 = 192` = visible height clipping
- `0xA0504 = 0xA0000 + 0x504` = HUD strip 之後的可見區起點
