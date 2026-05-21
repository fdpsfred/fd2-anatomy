# FDICON.B24 — 24×24 indexed icon sprites

唯一的非 LLLLLL 資源檔。1680 個 24×24 8bpp RLE icon。
file size 624,010 bytes (0x9858A)。

## 檔案格式

```
+0x00       u16 LE  width        = 0x0018 = 24
+0x02       u16 LE  height       = 0x0018 = 24
+0x04       u16 LE  active_count = 0x0690 = 1680
+0x06       u32 LE × (count+1)  offset[]    (last = sentinel = file_size)
                                            header_size = 6 + 1681 × 4 = 0x1A4A
+0x1A4A..   payload  1680 個 RLE-compressed 24×24 8bpp icons
```

第一 icon offset[0] = 0x1A4A；末 icon end = file size 0x9858A。全 1681 offsets
monotonic non-decreasing。

## 載入方式

由 `fopen("FDICON.B24"...)` 直接 fopen — 不走 LLLLLL `fd2_load_dat_resource`。
唯一的非 LLLLLL 資源。

## 副檔名「.B24」推測

可能含義：
- "B24" = "Bitmap 24-pixel" 或 "B(itmap) 24-(byte aligned)"
- 不是 24-bit color (這是 8bpp indexed)
- 不是 BMP format (沒有 "BM" magic)
- 漢堂自家命名習慣，與 "LLLLLL" DAT format 同源

## RLE 格式

與 6 主 DAT 共用 `fd2_rle_blit_sprite @ 0x4E63D` 格式 (詳 `program_info/graphics.md`)：

- `0b00xxxxxx` = literal copy
- `0b01xxxxxx` = stretched literal (1 src → 2 dst)
- `0b10xxxxxx` = RLE run
- `0b11xxxxxx` = skip transparent

平均壓縮率：449 bytes per 576-pixel icon ≈ 78% retained (壓縮率 ~22%)。

## 1680 icon namespace

24×24 tile sprite 1680 個不直接對應 char_id：
- player char_id 0..0x43 (68 IDs)
- data_fd2_battle_enemy_data_table 68 entries (0x44+)
- 總 char namespace ~154

1680 likely 對應：
- field map 上的 24×24 mini-character sprite (多 frame per direction × walk cycle)
- 24×24 tile sprite (terrain decorations / NPC mini-icons)
- 多 portrait variant per char (idle / hurt / KO / status / direction × frame)

具體 char_id ↔ icon idx 對應邏輯在 `load_portrait_to_cache` decompile 中可見
portrait_id 如何映射到 FDICON idx。

## 載入時機

`fd2_load_chapter_battle_data @ 0x1088D` 內：

```c
fopen("FDICON.B24", &DAT_00050078);
for (char_iter = 0; char_iter < portrait_cache_total_size; char_iter++) {
    portrait_idx = fd2_load_portrait_to_cache(
        bPortrait_id, bPos_x, ECX, bPortrait_id, file_handle);
    pSlot_iter->pSprite_state[0] = (byte)portrait_idx;
}
crt_fclose(file_handle);
```

`fd2_load_portrait_to_cache @ 0x11019` 是 200 KB linear-probe 快取系統。每次 chapter
init 開啟 FDICON.B24 → 對該章每個 char 讀 portrait_id 對應的 24×24 icon → 存到
portrait_sprite_cache。

## 工具

- 解碼：`tools/decoders/fdicon_decoder.py`
