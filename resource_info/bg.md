# BG.DAT — 320×100 cinematic / battle background

戰鬥 / cinematic 用的 320×100 indexed 8bpp 背景圖，count/color pair RLE 編碼。
file size 624,564 bytes，56 entries (idx 0..55)。

## 檔案格式

LLLLLL archive (詳 `overview.md`)。

## Entry format

```
+0x00  u16 LE  width    (= 0x0140 = 320)
+0x02  u16 LE  height   (= 0x0064 = 100)
+0x04  bytes   count/color pair RLE
                每對 2 bytes: (count, color)
                count 通常 ≤ 63 (= 0x3F)，可能跨多 pair 描繪同色
```

注意：BG 的 RLE 格式與 `rle_blit_sprite @ 0x4E63D` 的 opcode-based RLE **不同**。
BG 的 (count, color) 配對更簡單，每對 2 bytes 直接展開為 count 個 color 像素。

## Placeholder marker

10 個 BG entries 是 7-byte placeholder：`0A 00 03 00 C9 C9 C9` (與 TAI.DAT
placeholder 完全相同，兩 DAT 共用此 constant)。

Placeholder idx：11, 12, 13, 15, 20, ... (共 10 個)。

## idx 公式 (dynamic domain)

主要 caller：

- `execute_special_attack_skill` (4 callsites: idx 0/1/2 + dynamic terrain_id)
- `execute_summon_spell_cast` (1 dynamic — terrain from tile_attribute)
- `play_full_combat_cinematic` (3 + dynamic — terrain dispatch)
- `play_spell_cast_cinematic` (3 + dynamic)
- `play_figani_char_intro_animation` (1 dynamic — terrain)

Domain：`terrain_id` derived from
`tile_attribute_flags_buffer[(tile_id & 0x3FF) × 4]` 查 BG idx。每章 tile 系
有自己的 terrain palette。

## Static idx (4 個 + 1 endgame)

| idx | callsite | 用途 |
|---|---|---|
| 0x00 | `execute_special_attack_skill` / `execute_summon_spell_cast` / `play_figani_char_intro_animation` 等 (5 callsites) | 通用基礎 BG |
| 0x01 | `execute_special_attack_skill` / `play_full_combat_cinematic` / `play_spell_cast_cinematic` | 通用變體 |
| 0x02 | 同上 | 通用變體 2 |
| 0x38 | `play_spell_cast_cinematic` | 特殊 spell BG |

## 完整分類

| 分類 | 計數 | 比例 |
|---|---|---|
| documented_static | 4 | 7.1% |
| documented_dynamic_domain (terrain_id) | 42 | 75.0% |
| placeholder (`0A 00 03 00 C9 C9 C9`) | 10 | 17.9% |
| **TOTAL** | **56** | **100%** |

## 工具

- 解碼：`tools/decoders/bg_tai_decoder.py` (與 TAI.DAT 共用)
