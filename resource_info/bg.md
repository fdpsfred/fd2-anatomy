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

BG entry 由 `fd2_rle_blit_sprite @ 0x4E63D` 直接繪製 (`fd2_execute_summon_spell_cast`
等把 BG.DAT 載入 `data_fd2_battle_special_cinematic_bg_layers[]` 再 blit)。編碼是
RLE 4-op，與 FIGANI / FDICON / FDSHAP tile / TAI 共用同一格式，詳見
`resource_info/codecs.md`。

## Placeholder marker

10 個 BG entries 是 7-byte placeholder：`0A 00 03 00 C9 C9 C9` (與 TAI.DAT
placeholder 完全相同，兩 DAT 共用此 constant)。

Placeholder idx：11, 12, 13, 15, 20, ... (共 10 個)。

## idx 公式 (dynamic domain)

主要 caller：

- `fd2_execute_special_attack_skill` (4 callsites: idx 0/1/2 + dynamic terrain_id)
- `fd2_execute_summon_spell_cast` (1 dynamic — terrain from tile_attribute)
- `fd2_play_full_combat_cinematic` (3 + dynamic — terrain dispatch)
- `fd2_play_class_promotion_cinematic` (3 + dynamic)
- `fd2_play_figani_char_intro_animation` (1 dynamic — terrain)

Domain：`terrain_id` derived from
`tile_attribute_flags_buffer[(tile_id & 0x3FF) × 4]` 查 BG idx。每章 tile 系
有自己的 terrain palette。

## Static idx (4 個 + 1 endgame)

| idx | callsite | 用途 |
|---|---|---|
| 0x00 | `fd2_execute_special_attack_skill` / `fd2_execute_summon_spell_cast` / `fd2_play_figani_char_intro_animation` 等 (5 callsites) | 通用基礎 BG |
| 0x01 | `fd2_execute_special_attack_skill` / `fd2_play_full_combat_cinematic` / `fd2_play_class_promotion_cinematic` | 通用變體 |
| 0x02 | 同上 | 通用變體 2 |
| 0x38 | `fd2_play_class_promotion_cinematic` | 特殊 spell BG |

## 完整分類

| 分類 | 計數 | 比例 |
|---|---|---|
| documented_static | 4 | 7.1% |
| documented_dynamic_domain (terrain_id) | 42 | 75.0% |
| placeholder (`0A 00 03 00 C9 C9 C9`) | 10 | 17.9% |
| **TOTAL** | **56** | **100%** |

## 工具

- 解碼：`tools/decoders/bg_tai_decoder.py` (與 TAI.DAT 共用)
