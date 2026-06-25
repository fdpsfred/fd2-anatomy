# graphics

DOS mode 13h (320×200×256-color)，primary surface @ `0xA0000` (linear)。
工作 buffer 多用 0x140 (320) 或 0x280 (640) 寬度。

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
| `0b00xxxxxx` | literal copy len 個 byte |
| `0b01xxxxxx` | stretched literal: 1 src byte → 2 dst pixels (×len 次) |
| `0b10xxxxxx` | RLE run: 後續 1 byte 重複 len 次 |
| `0b11xxxxxx` | skip len 個像素 (透明) |

每行寬度由 `data_fd2_graphics_rle_blit_cur_width @ 0x627B4` 計數，遇 0 換行並
`data_fd2_graphics_rle_blit_remaining_rows @ 0x627B6 --`。

### Palette 模式 (`fd2_rle_blit_sprite` 的 `param_6`)

| 值 | 模式 |
|---|---|
| `0xFFFFFFFF` | passthrough (直接複製) |
| `> 0xFF` | translucent overlay：`(byte + (op>>8)) & 7) + (op & 0xFF)` |
| `≤ 0xFF` | silhouette：所有不透明像素都換成 `(op>>8)` 的單一顏色 |

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

- 章節 9 / 0x18 / 0x19 / 0x1C / 0x1D：使用 `animated_bg_buffer`，每 BIOS tick
  透過 `FUN_0004EB90` 切換 16-frame 循環
- 章節 0x11 / 0x15 / 0x16 / 0x1B：寬螢幕版本 (stride 0x1CE 或 0x198)
- 章節 0x17：特殊，每 tick 呼叫 `FUN_00024D22` 更新背景
- 預設：靜態背景，跳過此 pass

每個 tile 4 bytes 元資料 (位於 `battle_tile_map`)；屬性位元 (從
`tile_attribute_flags_buffer @ 0x53A69`) 控制：

- bit 0x04 → 每隔 frame 切換 +1 (一般動畫)
- bit 0x08 → 每隔 frame 切換 +2 (雙速動畫)
- bit 0x10 → chapter palette half-step

Animation 計數器：`bg_anim_frame_idx @ 0x53C1F` (0..0x14)，可由
`data_fd2_graphics_forced_tile_anim_frame @ 0x51A93` 強制鎖定到特定 frame；
`bg_anim_flip_flag @ 0x53A40` 是「每隔一 frame 翻轉」flip flag。

## Palette FX

| 位址 | 名稱 | 用途 |
|---|---|---|
| `0x11D40` | `fd2_set_vga_palette_range` | 寫 0x3C8/0x3C9 ports；含 brightness subtract |
| `0x1F882` | `fd2_play_palette_fade_to_black` | 64-frame 淡出 (~128 ms) |
| `0x4DFCC` | `fd2_update_palette_cycle_anim` | 16-color top-range 循環 (水/熔岩/火) |

VGA palette 來源：`vga_palette_data @ 0x53A65` — 768-byte 標準 256×3 RGB。

## Battle frame finalizer：`fd2_composite_battle_frame @ 0x11CAC`

戰鬥畫面的 frame finalizer，幾乎每個 UI 狀態變更後都被呼叫。Pipeline：

1. `FUN_0001297D` — tile render state setup
2. 若 `skip_decompress_flag == 0`：`FUN_0004DFCC` 解壓 snapshot
3. `fd2_composite_battle_tile_map` — paint 背景
4. `FUN_000122DC` — 角色 sprite 層
5. `FUN_000127A9` — HP bar / status icon overlay
6. `FUN_0001ACF3` — UI text overlay
7. `fd2_blit_rectangle` → `0xA0504` (mode13h primary 視窗起點)

像素常數：
- `0x1C8 = 456` = render workspace pitch (含 padding)
- `0x140 = 320` = mode13h stride
- `0x138 = 312` = visible width clipping
- `0xC0 = 192` = visible height clipping
- `0xA0504 = 0xA0000 + 0x504` = HUD strip 之後的可見區起點
