# BG.DAT — 320×100 cinematic / battle background

戰鬥 / cinematic 用的 320×100 indexed 8bpp 背景圖，`fd2_rle_blit_sprite` RLE 編碼。
file size 624,564 bytes，56 entries (idx 0..55)。

## 檔案格式

LLLLLL archive (詳 `overview.md`)。

## Entry format

```
+0x00  u16 LE  width    (= 0x0140 = 320)
+0x02  u16 LE  height   (= 0x0064 = 100)
+0x04  bytes   RLE 4-op 指令流 (編碼見 resource_info/codecs.md)
```

BG entry 由 `fd2_rle_blit_sprite @ 0x4E63D` 繪製。多數 caller 把單張 BG 載入區域變數後
直接 blit；`fd2_play_class_promotion_cinematic` / `fd2_execute_special_attack_skill` /
`fd2_play_full_combat_cinematic` 另把 BG.DAT[0/1/2] 載入
`data_fd2_battle_special_cinematic_bg_layers[]` 當 3 層 parallax 背景。編碼是 RLE 4-op，
與 FIGANI / FDICON / FDSHAP tile / TAI 共用同一格式，詳見 `resource_info/codecs.md`。

## Placeholder marker

10 個 BG entries 是 7-byte placeholder：`0A 00 03 00 C9 C9 C9` (與 TAI.DAT
placeholder 完全相同，兩 DAT 共用此 constant)。

Placeholder idx：11, 12, 13, 15, 20, ... (共 10 個)。

## idx 公式 (dynamic domain)

主要 caller（`0x52381` = BG.DAT 共 16 個 load site，橫跨 6 個 function）：

- `fd2_execute_special_attack_skill` (5 sites：static idx 0/1/2 + 2 dynamic：caster tile_attr、resolved AoE terrain)
- `fd2_play_full_combat_cinematic` (5 sites：static idx 0/1/2 + 2 dynamic：spotlight terrain、defender split terrain)
- `fd2_play_class_promotion_cinematic` (3 sites：static idx 0/1/2；無 dynamic terrain)
- `fd2_play_spell_cast_sequence` (1 dynamic — team-swap 後的 terrain BG)
- `fd2_execute_summon_spell_cast` (1 dynamic — 腳下 tile attribute terrain)
- `fd2_play_figani_char_intro_animation` (1 dynamic — 腳下 tile attribute terrain)

Domain：`terrain_id` derived from
`tile_attribute_flags_buffer[(tile_id & 0x3FF) × 4]` 查 BG idx。每章 tile 系
有自己的 terrain palette。

## Static idx (3 個)

`fd2_execute_special_attack_skill` / `fd2_play_class_promotion_cinematic` /
`fd2_play_full_combat_cinematic` 各以 literal idx 0/1/2 載入 3 層 parallax 背景到
`data_fd2_battle_special_cinematic_bg_layers[0/1/2]`。

| idx | callsite | 用途 |
|---|---|---|
| 0x00 | 上述 3 caller | parallax layer 0 (通用基礎 BG) |
| 0x01 | 同上 3 caller | parallax layer 1 |
| 0x02 | 同上 3 caller | parallax layer 2 |

## 完整分類

| 分類 | 計數 | 比例 |
|---|---|---|
| documented_static | 3 | 5.4% |
| documented_dynamic_domain (terrain_id) | 43 | 76.8% |
| placeholder (`0A 00 03 00 C9 C9 C9`) | 10 | 17.9% |
| **TOTAL** | **56** | **100%** |

## 工具

- 解碼：`tools/decoders/bg_tai_decoder.py` (與 TAI.DAT 共用)
