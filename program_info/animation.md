# animation

FD2 的特殊攻擊（必殺技 / 召喚）動畫由 `play_figani_animation_loop @ 0x2B659`
驅動。它讀取 FIGANI.DAT 的 byte-stream，按 pose 逐格播放。

## FIGANI 與 portrait_id 的對應

FIGANI.DAT 共 408 entries，公式：

```
FIGANI idx = portrait_id × 3 + offset
  + 0 = frame_a (basic animation, ~25 KB)
  + 1 = frame_b (extended: spell cast / special attack, ~80 KB)
  + 2 = frame_c (3-byte `00 00 0A` placeholder)
```

408 / 3 = 136 portraits，與 DATO.DAT 的 136 entries 一致 (共用 portrait_id
namespace)。詳細 entry index 表見 `resource_info/figani.md`。

## FIGANI 檔內格式

```
header:
  byte[0]   pose_count
  byte[2]   first_pose_offset
  byte[4]   sfx_bank_id        (索引 → 0x525DA → FDOTHER.DAT entry)
  byte[8]   offset_table[pose_count]   (每 entry 4 bytes 指向 pose 資料)

per pose entry:
  byte +4   type                 (1 = spell-cast frame，會觸發 deduct_caster_mp + flash)
  byte +5   sfx_hook_id          (0 = 無 sfx；非 0 = 從 special_attack_sfx_bank 取索引)
  byte +6   sub_frame_count
  byte +8.. sub-frame data       (sprite indices for 動畫子幀)
```

## 主要 functions

| 位址 | 名稱 | 角色 |
|---|---|---|
| `0x2B659` | `play_figani_animation_loop` | master pose 迭代器 |
| `0x29164` | `play_char_intro_zoom_anim` | 9-frame 角色登場 zoom-in |
| `0x2A289` | `flash_char_hit_sprite` | per-team 受擊閃光 |
| `0x2B9A1` | `step_figani_pose_animation` | per-frame state machine |
| `0x2BC9A` | `load_figani_sfx_bank` | SFX bank 抽取 |

## Per-spell 動畫參數表

`animate_spell_impact_per_target @ 0x1C4CC` 用 3 個平行 byte-table 驅動 spell
視覺效果 (spell_id 0..35 索引)：

- `data_fd2_animation_spell_sprite_offset_table @ 0x51F33`
- `data_fd2_animation_spell_frame_count_table @ 0x51F54`
- `data_fd2_animation_spell_sfx_frame_table @ 0x51F75`

## Panel / Dialog slide 動畫

| 位址 | 名稱 | 用途 |
|---|---|---|
| `0x1974C` | `slide_panel_down_step` | chapter portrait 下拉 (6 frame) |
| `0x1839B` | `slide_panel_up_partial_step` | status screen 上升 (7 frame) |
| `0x182AD` | `paint_status_panel_layer_left` | 86×86 row blit |
| `0x18312` | `paint_status_panel_layer_right` | 86×223 row blit |
| `0x18409` | `play_status_screen_outro_step` | 12-frame symmetric close |

## 死亡與爆炸動畫

`play_death_animation_and_mark_dead @ 0x1DB65`：13-frame 受擊閃光 + 12-frame
puff 爆炸；用 `death_anim_sprite_table @ 0x53A81` 作為 sprite 來源。

## Spell 視覺三段管線

| 位址 | 名稱 | 用途 |
|---|---|---|
| `0x1C4CC` | `animate_spell_impact_per_target` | per-target sprite frame loop |
| `0x1CAC7` | `animate_spell_full_screen_flash` | 4-cycle 720ms 全螢幕閃 |
| `0x1CD17` | `animate_spell_overlay_blink` | 10-frame fading overlay |

## 召喚 palette-cycle FX

`execute_summon_spell_cast @ 0x27FC9` 用 4 個 RGB 表 (`0x5254F / 0x52553 /
0x52557 / 0x5255B`) 為 spell_id 0x20-0x23 召喚產生獨特的 palette-cycle 顏色。
`FUN_000286BD` 做 0..0xFF 共 0x29 frame 的 palette 漸變。

## 行為相關

`ai_pass_turn_with_heal @ 0x13FD4` — 當 AI 沒可用動作時，自動恢復 20% HP_max
並播放動畫，是 FD2 重型敵人「拖不死」的程式根源。

`animate_spell_impact_per_target` 對 spell_id 8 / 9 / 0x12 / 0x13 / 0x16 / 0x19
有額外 SFX schedule（特定 frame 觸發）。
