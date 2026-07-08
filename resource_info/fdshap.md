# FDSHAP.DAT — 戰鬥地形 tile sheet + tile attribute

每章戰鬥場景的 24×24 地形 tile sheet 與 4-byte/tile 屬性資料。戰鬥地圖由
compositor 依 FDFIELD tile map 逐格擺放這些 tile 組成，並非單張預算圖。
file size 3,557,794 bytes，66 entries (idx 0..65)。

## 檔案格式

LLLLLL archive (詳 `overview.md`)。

## idx 公式

```
shap_id = tile_event_data_table[0]   // FDFIELD chapter_id × 3 + 1 entry 的 first byte
FDSHAP[shap_id × 2 + 0] = data_fd2_battle_scene_tile_gfx_ptr   (24×24 tile sheet)
FDSHAP[shap_id × 2 + 1] = tile_attribute_flags    (4 bytes/tile)
```

## 33 章 × 2 = 66 idx 完整對照表

| Chapter | tile_event idx | shap_id | FDSHAP[tile_sheet] | FDSHAP[attr] |
|---|---|---|---|---|
| ch1  |  1 | 0x00 |  0 |  1 |
| ch2  |  4 | 0x01 |  2 |  3 |
| ch3  |  7 | 0x02 |  4 |  5 |
| ch4  | 10 | 0x03 |  6 |  7 |
| ch5  | 13 | 0x04 |  8 |  9 |
| ch6  | 16 | 0x05 | 10 | 11 |
| ch7  | 19 | 0x06 | 12 | 13 |
| ch8  | 22 | 0x07 | 14 | 15 |
| ch9  | 25 | 0x08 | 16 | 17 |
| ch10 | 28 | 0x09 | 18 | 19 |
| ch11 | 31 | 0x0A | 20 | 21 |
| ch12 | 34 | 0x0B | 22 | 23 |
| ch13 | 37 | 0x0C | 24 | 25 |
| ch14 | 40 | 0x0D | 26 | 27 |
| ch15 | 43 | 0x0E | 28 | 29 |
| ch16 | 46 | 0x0F | 30 | 31 |
| ch17 | 49 | 0x10 | 32 | 33 |
| ch18 | 52 | 0x11 | 34 | 35 |
| ch19 | 55 | 0x12 | 36 | 37 |
| ch20 | 58 | 0x13 | 38 | 39 |
| ch21 | 61 | 0x14 | 40 | 41 |
| ch22 | 64 | 0x15 | 42 | 43 |
| ch23 | 67 | 0x16 | 44 | 45 |
| ch24 | 70 | 0x17 | 46 | 47 |
| ch25 | 73 | 0x18 | 48 | 49 |
| ch26 | 76 | 0x19 | 50 | 51 |
| ch27 | 79 | 0x1A | 52 | 53 |
| ch28 | 82 | 0x1B | 54 | 55 |
| ch29 | 85 | 0x1C | 56 | 57 |
| ch30 | 88 | 0x1D | 58 | 59 |
| endgame_ch30 | 91 | 0x1E | 60 | 61 |
| endgame_ch31 | 94 | 0x1F | 62 | 63 |
| endgame_ch32 | 97 | 0x20 | 64 | 65 |

特性：33 章 × 2 = 66 FDSHAP idx **完整 1:1 對應**；shap_id 線性連續 0x00..0x20；
0 unused、0 共用 — 每章獨佔一對。

ch23 mid-switch 切到 ch24 用 ch24 自己的 shap_id 0x17 → FDSHAP[46/47]
(load `current_chapter_id = 24` 然後 `fd2_load_chapter_battle_data(24)` 讀 FDFIELD ch24
tile_event[0] = 0x17)。

## data_fd2_battle_scene_tile_gfx_ptr (`shap_id × 2 + 0`)

24×24 地形 tile sheet，供戰鬥地圖 compositor 依 tile map 逐格擺放。layout
(`fd2_convert_battle_tiles_to_24px @ 0x1399C` 消費)：

```
+0  u16 LE  tile width  (= 0x18 = 24)
+2  u16 LE  tile height (= 0x18 = 24)
+4  u16 LE  tile_count
+6  int32 LE [tile_count]  各 tile 相對 sheet base 的 byte offset
各 tile：command-only 的 RLE 4-op 指令流，固定 24×24
        (in-game 由 `fd2_convert_battle_tiles_to_24px @ 0x1399C` 消費)
```

每個 tile 的 RLE 4-op 編碼與 `fd2_rle_blit_sprite @ 0x4E63D` 相同 (詳見
`resource_info/codecs.md`)，FDSHAP tile 專屬點：固定 24×24、指令流無 `[w][h]`
header (寬高由 sheet header 的 `0x18/0x18` 統一決定)。

ch1 sample (FDSHAP[0])：147,740 bytes，288 個 24×24 tile。

## tile_attribute_flags (`shap_id × 2 + 1`)

- 4 bytes per tile，無 header
- 每 4-byte 結構：

```
+0  byte  ?
+1  byte  ?
+2  byte  animation/palette flag bits:
            0x04 = animated frame +1/tick
            0x08 = animated frame +2/tick
            0x10 = palette half-step
+3  byte  ?
```

ch1 sample: 1200 bytes / 4 = 300 tiles。

## 驗證

以 offset 表逐 tile 解碼 (每 tile 為固定 24×24 的 command-only RLE 4-op 指令流)，
ch1 sheet 的 288 個 tile 全數還原成連貫的地形圖磚 (草地 / 水 / 泥路 / 岩石 /
屋頂 / 木橋 等)。

## 工具

- 解碼：`tools/decoders/fdshap_decoder.py` (tile sheet + 4-byte/tile attribute)
