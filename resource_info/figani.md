# FIGANI.DAT — 必殺技 / 召喚動畫 byte-stream

戰鬥 / 特殊技 / cinematic 用的動畫 frame。
file size 15,279,582 bytes (~15 MB)，408 entries (idx 0..407)。

## 檔案格式

LLLLLL archive (詳 `overview.md`)。

## idx 公式

```
FIGANI idx = portrait_id × 3 + offset
  + 0 = frame_a (basic animation, ~17-30 KB)
  + 1 = frame_b (extended: spell cast / special attack, ~30-200+ KB)
  + 2 = frame_c (mostly placeholder: 3-byte `00 00 0A`)
```

408 / 3 = 136 portraits ✓ (與 DATO 136 一致；同 portrait_id namespace)。

## Frame 分布統計

| Frame slot | 內容 | 計數 | 平均 size |
|---|---|---|---|
| frame_a (×3 + 0) | basic animation | 125 active + 11 placeholder | active ~23 KB |
| frame_b (×3 + 1) | extended (spell cast / special attack) | 122 active + 14 placeholder | active ~88 KB |
| frame_c (×3 + 2) | mostly placeholder (3-byte `00 00 0A`) | 17 active + 119 placeholder | active ~77 KB |

Total placeholder (3-byte): 144 entries (= 11 frame_a + 14 frame_b + 119 frame_c)。

## Frame format

每個 active frame 是 byte-stream 動畫 sequence：

```
+0x00  u16 LE  pose_count                  (典型 4..16；作 target 被包夾時的 wrap 界)
+0x02  u16 LE  pose_count (caster 迴圈上界) (`fd2_play_figani_animation_loop` 讀低 byte 當
                                             pose 迴圈上界，通常 == +0)
+0x04  u16 LE  sfx_bank_id                 (低 byte 由 `fd2_load_figani_sfx_bank` 1-based
                                             索引 `data_fd2_audio_figani_sfx_bank_fdother_index_lut
                                             @ 0x525D6` (byte[6] = {30..35}) 得 FDOTHER entry idx)
+0x06  u16 LE  reserved
+0x08  u32 LE × pose_count    pose_offsets (each pointing to pose payload)
+payload                                    per-pose data
```

每個 pose entry 本身是一張 sprite block（由 `fd2_blit_indexed_sprite` 解）。欄位 +4/+5/+6
由 `fd2_play_figani_animation_loop @ 0x2B659` 讀取觸發；`fd2_step_figani_pose_animation
@ 0x2B9A1` 只負責推進 pose × sub-frame（讀 +0 pose count、+6 sub_frame_count、header +8
offset 表）：

- +0 u16: width
- +2 u16: height
- byte +4: type (1 = spell-cast frame，`fd2_play_figani_animation_loop` 觸發 `fd2_deduct_caster_mp` + flash)
- byte +5: sfx_hook_id (0 = no sfx; non-0 = index into data_fd2_audio_figani_sfx_bank_buf_ptr)
- byte +6: sub_frame_count (該 pose 停留的 sub-frame 數；每 sub-frame 重繪整張 pose)
- byte +9 onwards: RLE-packed pixel stream (由 `fd2_rle_blit_sprite` 解)

idx 0 sample header (portrait 0 frame_a)：

```
04 00      pose_count = 4
04 00      pose_count (caster 迴圈上界) = 4
00 00      sfx_bank_id = 0
00 00      reserved
18 00 00 00  pose_offset[0] = 0x18 (= 24，header 之後)
91 14 00 00  pose_offset[1] = 0x1491 (5265)
0D 29 00 00  pose_offset[2] = 0x290D (10509)
A0 3D 00 00  pose_offset[3] = 0x3DA0 (15776)
```

## per-spell animation 三個平行 byte-table

對 spell_id 0..35 的視覺效果由 3 個平行 byte tables 驅動 (見
`program_info/anim.md`)：

| Table | Address | 用途 |
|---|---|---|
| `data_fd2_animation_spell_sprite_offset_table` | 0x51F33 | sprite frame 偏移 |
| `data_fd2_animation_spell_frame_count_table` | 0x51F54 | frame count |
| `data_fd2_animation_spell_sfx_id_table` | 0x51F75 | SFX 觸發 frame index |

這三張 table 並非直接 index FIGANI，而是控制 `fd2_animate_spell_impact_per_target`
內 per-spell sprite frame loop 的參數（FIGANI 載入由 `fd2_play_spell_cast_sequence`
等 cinematic function 動態做）。

## Caller 分布

各 caller 的 idx 引數由 runtime 值 (施法者/目標 portrait_id、spell_id) 決定；下表
列出依 caller 結構推導的 idx 公式，個別 callsite 的精確引數尚未逐一 byte 定案
(pending)。

| Caller | idx 公式 (caller-derived) |
|---|---|
| `fd2_execute_special_attack_skill` | caster_portrait × 3 + 0/1, target_portrait × 3 + 0/1 |
| `fd2_execute_summon_spell_cast` | caster_portrait × 3 (basic) + spell_id-derived |
| `fd2_play_full_combat_cinematic` | defender_portrait × 3, attacker_portrait × 3 |
| `fd2_play_spell_cast_sequence` | char_portrait × 3 (caster), char_portrait × 3 + 2 (alt) |
| `fd2_play_class_promotion_cinematic` | spell_id × 3 |
| `fd2_play_figani_char_intro_animation` | portrait_id × 3 (+1)；portrait_id 由 char_unit_id 引數內部取 `runtime_char[char_unit_id].bPortrait_id` |
| `fd2_play_final_chapter_30_ending` | iVar6 (loop-based portrait sequence) |

## 完整分類

| 分類 | 計數 | 比例 |
|---|---|---|
| documented_dynamic_domain (portrait_id × 3 + offset) | 264 | 64.7% |
| placeholder (3-byte `00 00 0A`) | 144 | 35.3% |
| **TOTAL** | **408** | **100%** |
