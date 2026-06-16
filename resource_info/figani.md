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
| frame_a (×3 + 0) | basic animation | 136 active | ~25 KB |
| frame_b (×3 + 1) | extended (spell cast / special attack) | 128 active + 8 placeholder | ~80 KB |
| frame_c (×3 + 2) | placeholder (3-byte `00 00 0A`) | 136 placeholder | 3 bytes |

Total placeholder (3-byte): 144 entries (= 136 frame_c slots + 8 extra slot
對應沒有 special anim 的 portrait)。

## Frame format

每個 active frame 是 byte-stream 動畫 sequence：

```
+0x00  u16 LE  pose_count                  (典型 4..16)
+0x02  u16 LE  ???_count                   (可能 sub_pose_count 或 alt_count)
+0x04  u16 LE  sfx_bank_id                 (索引 → 0x525DA → FDOTHER sub-archive)
+0x06  u16 LE  reserved
+0x08  u32 LE × pose_count    pose_offsets (each pointing to pose payload)
+payload                                    per-pose data
```

per pose entry (per `fd2_step_figani_pose_animation @ 0x2B9A1`):

- byte +4: type (1 = spell-cast frame，會觸發 `deduct_caster_mp` + flash)
- byte +5: sfx_hook_id (0 = no sfx; non-0 = index into data_fd2_audio_figani_sfx_bank_buf_ptr)
- byte +6: sub_frame_count
- byte +8 onwards: sub-frame sprite indices

idx 0 sample header (portrait 0 frame_a)：

```
04 00      pose_count = 4
04 00      ??? = 4
00 00      sfx_bank_id = 0
00 00      reserved
18 00 00 00  pose_offset[0] = 0x18 (= 24，header 之後)
91 14 00 00  pose_offset[1] = 0x1491 (5265)
0D 29 00 00  pose_offset[2] = 0x290D (10509)
A0 3D 00 00  pose_offset[3] = 0x3DA0 (15776)
```

## per-spell animation 三個平行 byte-table

對 spell_id 0..35 的視覺效果由 3 個平行 byte tables 驅動 (見
`program_info/animation.md`)：

| Table | Address | 用途 |
|---|---|---|
| `data_fd2_animation_spell_sprite_offset_table` | 0x51F33 | sprite frame 偏移 |
| `data_fd2_animation_spell_frame_count_table` | 0x51F54 | frame count |
| `data_fd2_animation_spell_sfx_frame_table` | 0x51F75 | SFX 觸發 frame index |

這三張 table 並非直接 index FIGANI，而是控制 `animate_spell_impact_per_target`
內 per-spell sprite frame loop 的參數（FIGANI 載入由 `fd2_play_spell_cast_sequence`
等 cinematic function 動態做）。

## Caller 分布

| Caller | 推測 idx 公式 |
|---|---|
| `fd2_execute_special_attack_skill` | caster_portrait × 3 + 0/1, target_portrait × 3 + 0/1 |
| `fd2_execute_summon_spell_cast` | caster_portrait × 3 (basic) + spell_id-derived |
| `fd2_play_full_combat_cinematic` | defender_portrait × 3, attacker_portrait × 3 |
| `fd2_play_spell_cast_sequence` | char_portrait × 3 (caster), char_portrait × 3 + 2 (alt) |
| `fd2_play_spell_cast_cinematic` | spell_id × 3 |
| `fd2_play_figani_char_intro_animation` | figani_idx (caller-passed) |
| `fd2_play_final_chapter_30_ending` | iVar6 (loop-based portrait sequence) |

## 完整分類

| 分類 | 計數 | 比例 |
|---|---|---|
| documented_dynamic_domain (portrait_id × 3 + offset) | 264 | 64.7% |
| placeholder (3-byte `00 00 0A`) | 144 | 35.3% |
| **TOTAL** | **408** | **100%** |
