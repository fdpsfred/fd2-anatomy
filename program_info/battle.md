# battle

FD2 沒有獨立的 `battle_turn_loop()` 或 `start_battle()` function。戰鬥狀態
由 chapter init 把地圖設定好之後，透過 `main` 外迴圈反覆呼叫
`fd2_game_main_loop` 推進。「回合」概念分散在 `fd2_game_main_loop` 的鍵盤分派、
`fd2_enemy_turn_phase_team0` / `fd2_npc_turn_phase_team1` 與底下的
`fd2_enemy_turn_action_dispatcher` (12-case AI behavior class lookup)。

## 戰鬥指令分派

| 位址 | 名稱 | 角色 |
|---|---|---|
| `0x00014EF0` | `fd2_attack_action_dispatch` | 三路評分動作選擇器 |
| `0x00015311` | `fd2_execute_ai_offensive_spell` | 法術 winner 執行 |
| `0x0001548E` | `fd2_execute_ai_physical_attack` | 物理 winner 執行 |
| `0x00015055` | `fd2_execute_ai_item_use` | 道具 winner 執行 |
| `0x0001CA89` | `fd2_deduct_caster_mp` | 法術消耗 MP |

## Spell 選單 (modal UI)

| 位址 | 名稱 |
|---|---|
| `0x0001C269` | `fd2_build_usable_spell_list` |
| `0x0001CEED` | `fd2_draw_spell_selection_list` |
| `0x0001CFF0` | `fd2_spell_selection_menu_main` |
| `0x0001D51D` | `fd2_spell_select_input_loop` |

## Damage pipeline

| 位址 | 名稱 | 角色 |
|---|---|---|
| `0x0001C75E` | `fd2_calc_magic_damage` | 法術傷害計算 |
| `0x0002A6BD` | `fd2_play_spell_cast_sequence` | 2 KB 大函式，spell cast 全套動畫 |
| `0x00020C6F` | `fd2_apply_use_effect_dispatch` | 17 個 effect code dispatcher |
| `0x0002111A` | `fd2_apply_attack_spell_damage` | 攻擊型法術傷害 apply |
| `0x00014818` | `fd2_compute_aoe_targets` | AoE 範圍目標計算 |
| `0x0001E0DB` | `fd2_show_damage_number` | 浮動傷害數字 |
| `0x0001E1DC` | `fd2_show_miss_indicator` | miss 顯示 |

## Battle lifecycle (章節層)

| 位址 | 名稱 |
|---|---|
| `0x00022E5C` | `fd2_play_chapter_clear_fanfare` |
| `0x00010B4E` | `fd2_load_chapter_portraits_and_dump_tmp` |

## runtime_char 存取 helper

`fd2_get_inventory_slot_item_id @ 0x0001B722` (回傳 `runtime_char[char_idx].pInventory_slots[slot_idx].bItem_id`)。runtime_char struct 完整 layout
見 `overview.md`。

## 攻擊傷害公式

### 物理 (inline in attack_action_dispatch)

```c
raw_damage = attacker.wAP - target.wDP;     // offset +0x48 - +0x4A
// 三個 AI 候選 score 全域:
//   ai_best_physical_score @ 0x53C4F
//   ai_best_spell_score    @ 0x53C23
//   ai_best_item_score     @ 0x53C33
// 三個各有 target_x/y/id 全域 (C43/C47/C4B、C27/C2B/C2F、C37/C3B/C3F)。
// dispatch 選 score 最大者執行對應 executor。
```

### 魔法 (calc_magic_damage)

```c
adStack[28] = copy(data_fd2_battle_job_magic_resist_table);   // 攻擊對目標 job 的抗性
caster_ap = runtime_char[caster].AP (+0x48);
target_job = runtime_char[target].bJob_id (+0x20);
target_resist = adStack[target_job];
spell = fd2_get_spell_effect_entry(spell_id);
damage = spell.DA * target_resist / 10;       // 線性
hit_roll = uVar4 % 100;
if (hit_roll > spell.HT) return 0;            // miss
```

「魔法傷害 × job_magic_resist[target_job] / 10」與遊戲攻略「法術攻擊 = 最大
傷害 × 0.X」吻合（0.X = resist/10，故 job_resist 值 0..10 代表 0..100% 線性
衰減）。

## 物理攻擊結算（實際套用傷害）

AI 與玩家共用同一條物理傷害公式，由兩個等價函式實作（同公式、不同用途）：

| 位址 | 名稱 | 用途 |
|---|---|---|
| `0x0001ECC7` | `fd2_execute_attack_damage_calculation` | 直接套用：自己寫 `hp_current`，含命中爆擊動畫 |
| `0x00029F72` | `fd2_calculate_combat_hit_outcome` | 只算 outcome（不寫 HP），結果由 cinematic 跨 frame 套用 |

公式（單次命中）：

```
AP_eff = AP + (tile_attr_mv_modifier_table[tile_id]  * AP) / 100   # 攻擊者免疫則跳過
DP_eff = DP + (tile_attr_def_modifier_table[tile_id] * DP) / 100   # 防禦者免疫則跳過
命中: rng % 100 < attacker.+0x4C - defender.+0x4E                  # 不命中則傷害 0
爆擊: rng % 100 < job_crit_rate_table[attacker.job_id - 1]         # 爆擊把 DP_eff 折半
damage = (AP_eff - DP_eff) * 9 / 10           # < 0 取 0
jitter_range = damage / 9
if jitter_range != 0: damage += rng % jitter_range
hp_current -= damage                          # 夾到 0
```

整數除法皆截尾向零，故小 AP/DP（如 16/4）時 ±5/+10% 的地形修正會整除歸零、
傷害與地形無關。

**runtime_char 命中/迴避 stat**：物理命中判定為
`攻擊者 +0x4C(dx_current) - 防禦者 +0x4E(stat4_current)`，故 **+0x4C = 物理命中率、
+0x4E = 物理迴避率**（兩者皆由 `fd2_recalculate_combat_stats` 從 DX 基底 + 不同
裝備加成欄位算出）。

**玩家攻擊路徑**：`fd2_game_main_loop`（游標在我方單位上按 Space/Enter）→
`fd2_player_action_menu_loop`（選目的地 tile，可不移動）→
`fd2_player_inline_action_menu_dispatch`（動作選單 Attack=0/Spell=1/Item=2/Wait=3）→
選 Attack 後 `fd2_wait_for_action_target_input` 選目標 →
`fd2_play_full_combat_cinematic` → `fd2_execute_combat_hit_cinematic`。後者先抽
**1 次 RNG 做 3% 雙擊（double-strike）判定**（命中 2 下），再每下呼叫
`fd2_calculate_combat_hit_outcome` 算傷害並跨 hit-frame 寫入目標 HP；命中後若防禦者
近戰且相鄰會接一段反擊 cinematic（`fd2_check_can_counter_attack`，武器
`item_effect.range_min == 1`）。固定 seed 下整條 RNG 序列為：雙擊 → 命中 → 爆擊 →
jitter（每項依分支決定是否抽）。

## Enemy AI 主架構

```
[Per-frame from main game_main_loop, when 「enemy phase」 token active]

  enemy_turn_phase_team0 @ 0x1D8BA          兩階段 enemy AI 主迴圈
    ├─ Pass 1：smart caster 優先
    │     for char in team 0:
    │        score = ai_score_offensive_spell + ai_score_item_use
    │        if score > 5: enemy_turn_action_dispatcher(...)
    └─ Pass 2：fallback 全員
          for char in team 0: enemy_turn_action_dispatcher(...)

  npc_turn_phase_team1 @ 0x1D80B            友軍 NPC AI 單一迴圈
    └─ for char in team 1: enemy_turn_action_dispatcher(...)

  enemy_turn_action_dispatcher @ 0x13A9F    12 種 AI behavior class 分派
    └─ class = runtime_char[i].pCombat_aux_block[0xD] & 0x0F
    └─ Case 0/1/2/3/5/10: 各自 attack_action_dispatch(class) + fallback
    └─ Case 4: pass turn (face only)
    └─ Case 7: charge dash
    └─ Case 8: hard skip
    └─ Case 9: targeted approach
    └─ Case 11: smart caster (spell-then-physical)

  attack_action_dispatch @ 0x14EF0          三路評分動作選擇器
    ├─ ai_score_physical_attack → C4F + C43/C47/C4B
    ├─ ai_score_offensive_spell → C23 + C27/C2B/C2F
    ├─ ai_score_item_use         → C33 + C37/C3B/C3F
    └─ Pick max → execute_ai_physical_attack / execute_ai_offensive_spell /
                  execute_ai_item_use

  Helpers (when scoring fails / for movement):
    fd2_ai_seek_optimal_position @ 0x14121      job-aware pathfind
    fd2_ai_walk_to_target_tile  @ 0x14B78       path execute
    fd2_ai_pass_turn_with_heal  @ 0x13FD4       passing-turn 回 +20% HP_max
    fd2_count_usable_inventory_slots @ 0x1B8A6
    fd2_get_inventory_slot_item_id   @ 0x1B722
    fd2_score_item_candidate         @ 0x15880
    fd2_score_spell_candidate        @ 0x15B77

  Per-action postlude (in phase loops):
    if (DAT_00051A8F != 0xFF):
        data_fd2_battle_ai_post_action_consequence_table[DAT_00051A8F]()  ← 反擊 / 死亡 / 狀態
    data_fd2_chapter_post_action_handler_table[current_chapter_id]() ← 章節觸發事件
    if (game_event_flag != 0): break loop
```

## AI 評分三路

| Scorer | 類別 | score global | target_x/y | id global |
|---|---|---|---|---|
| `fd2_ai_score_physical_attack` @ `0x14237` | 物理攻擊 | `0x53C4F` | C43 / C47 | C4B (target_idx) |
| `fd2_ai_score_offensive_spell` @ `0x1598A` | 法術攻擊 | `0x53C23` | C27 / C2B | C2F (spell_id) |
| `fd2_ai_score_item_use`        @ `0x1567E` | 道具使用 | `0x53C33` | C37 / C3B | C3F (slot_idx) |

### 物理評分公式

```
raw_dmg = (attacker.AP + terrain_bonus) - (target.DP + terrain_penalty)
score_class = 0     若 raw_dmg < 3
            = 8     一般命中
            = 0x12  若 target.HP_current < raw_dmg  (kill shot — 最高優先)
tie-break = raw_dmg 數值；側背 (+0x08[0]==0) 再 ×1.5
```

### 三類 executor

| Winner | Executor | 備註 |
|---|---|---|
| C4F (物理) | `fd2_execute_ai_physical_attack @ 0x1548E` | 全套物理動畫；plain vs animated 由 `physical_attack_fx_preset @ 0x53AF9` 切換 |
| C23 (法術) | `fd2_execute_ai_offensive_spell @ 0x15311` |  |
| C33 (道具) | `fd2_execute_ai_item_use @ 0x15055` | 短距 vs 長距由 `data_fd2_battle_item_effect_table.range_class` (offset 0x10) 切換 |

### Tie-break 規則

- `C4F == C23 > C33`：比較 `spell.base_damage` 與 `raw_dmg`，高者勝；
  `spell_id ≥ 11` 時由 `runtime_char[caster].pCombat_aux_block[0xD] & 0x40` 旗標決定。
- `C4F == C33 > C23`：同樣旗標決定道具 or 物理。
- 三者 `score < 6`：abort (return 0)，AI 本回合無動作。

## Per-candidate scorer

### `fd2_score_item_candidate @ 0x15880` 依 `item.b0D` effect-code

- `0x05` / `0x0D` (HP-damage type)：per target，HP/max 比例決定 0/3/8 分。
  `pCombat_aux_block[0xD] & 0x80` 旗標 ×3 加成（高價值目標）。
- `0x14` / `0x15` / `0x18` (spell-wrapper items)：查 spell 基礎傷害，
  `HP < 傷害` → 0x12 (kill shot)，否則 8。

### `fd2_score_spell_candidate @ 0x15B77` 依 `spell_id` 分段

| spell_id 區段 | 類別 | 評分邏輯 |
|---|---|---|
| 0..9 | 基本攻擊法術 | HP < dmg → **0x18** (kill shot)；否則 8；敵方 × `_DAT_00050144` (float) |
| 10..12 | 攻擊法術 (帶 LoS) | 同上，但需先過 `FUN_0001F183` LoS 檢查 |
| 13..16 | 治療法術 | HP < max/3 → 8；HP < max/2 → 3；heal-boost 旗標 (`flag_27[0xD]&1`) ×2 |
| 17..19 | 變體 | `FUN_00015DA2(spell_id+0x11, targets, 3)` — 狀態類 |
| 0x14 | 解狀態 A | `pStatus_flags_block[4] != 0` → +6 (可能解毒) |
| 0x15 | 解狀態 B | `bStatus_sleep_flag != 0` → +6 (可能解睡眠) |
| 0x16 | 沉默 | 目標未沉默 AND 有可用法術 → +6 (優先沉默詠唱者) |
| 0x1A / 0x1B | 召喚系 | `FUN_00015DA2(0x25/0x26, targets, 4)` |
| 其他 | 無 | return 0 |

## 關鍵數值結論

- **AI kill-shot 權重**：物理 `0x12` (18)、item `0x12` (18)、**spell `0x18` (24)**
  → AI 顯著偏好用法術秒殺。
- **Pass-turn auto heal**：`fd2_ai_pass_turn_with_heal` 在 AI 沒有有效行動時自動回
  20% HP_max；FD2 重型敵人「拖不死」的程式根源。
- **Two-pass enemy phase**：smart casters 第一輪先動 (佔 AoE 位置與秒殺)，melee
  第二輪。AI 戰術設計，不是 bug。
- **AI behavior class 0..11** (`pCombat_aux_block[0xD]` 低 nibble) = 每個敵人的
  「個性」。Class 11 = 智者 (spell-first)、class 7 = 衝鋒、class 4 = 守備、
  class 8 = 完全不動。對應遊戲攻略中「某些敵人很兇、某些只是站著」的觀察。

## 12 個 AI behavior class semantic

(per `fd2_enemy_turn_action_dispatcher @ 0x13A9F`)

| class | 名稱 | 行為 |
|---|---|---|
| 0 | default_attacker | `fd2_attack_action_dispatch(0)`；fall to `ai_seek/advance` |
| 1 | defensive_kiter | `attack(1)`；fail → seek + pass_with_heal |
| 2 | aggressive_physical | `attack(2)`；fail → `fd2_ai_score_physical_attack` 強制 |
| 3 | targeted_approach | `attack(3)`；fail → `fd2_find_char_by_id_or_template(0,...)` pathfind |
| 4 | pass_turn | 只 face direction；exit |
| 5 | item_pickup | `attack(5)`；fail → find_tile + pickup；下 turn class=7 |
| 7 | charge_dash | pan + `fd2_ai_walk_to_target_tile` (kamikaze) |
| 8 | hard_skip | 純 return (stunned/scripted-passive) |
| 9 | advance_to_char | `fd2_find_char_by_id_or_template(9,...)` pathfind |
| 10 | hardcoded_attack | `attack(10)`；fail → 同 class 4 (pass) |
| 11 | smart_caster | spell-then-physical fallback (boss-tier) |

## 死亡掉落與 post-action consequence dispatch

兩條獨立 path 都會 dispatch `data_fd2_battle_ai_post_action_consequence_table @ 0x51B91` 內的
handler，但機制完全不同：

### Path 1 — Tile-step trigger (deferred via global state)

`fd2_check_tile_event_post_action @ 0x13A44` 在 player/AI 走到 trigger tile 時呼叫：

1. 讀 tile attribute；若 tile 帶 event flag 且未消耗
2. 從 `tile_event_data_table + (tile_event_id - 1) * 2 + 0x33` 取 byte
3. 若 byte != 0xFF 且 event_type 匹配 → `data_fd2_battle_ai_post_action_consequence_idx = byte`
4. 下一輪 `fd2_game_main_loop` 看到 `data_fd2_battle_ai_post_action_consequence_idx != 0xFF`，
   dispatch `data_fd2_battle_ai_post_action_consequence_table[idx]()`，然後 reset 為 0xFF

### Path 2 — Death drop (direct via fd2_process_battle_drop_entries)

當 enemy 死亡 (HP_current = 0)：

1. **Pre-death**：
   - AI class 5 (item_pickup) 撿 pickup tile 時，`fd2_enemy_turn_action_dispatcher`
     case 5 把 `tile_event_data_table` 的 `(kind, param)` 抄到 enemy 自己的
     `pCombat_aux_block[10..12]` (= `bPickup_kind` + `wPickup_param`)
   - 或 chapter init / FDFIELD char_spawn record 直接初始化這 3 byte
2. **Death**：`fd2_collect_pending_death_drops` / `fd2_collect_dead_char_drops` 掃描所有
   alive char，挑 (`HP_current == 0` AND `pCombat_aux_block[10] != 0xFF`) 的
   3-byte block 抄到 caller-提供的 `drops_buffer`
3. **Process**：caller (typically `execute_ai_*` / `fd2_apply_use_effect_dispatch`)
   呼叫 `fd2_process_battle_drop_entries(killer_idx, count, drops_array)`
4. **Per-entry dispatch by drop_type byte**:
   - `0` = ITEM：dialog 0x1B0 / `fd2_add_item_to_inventory`
   - `1` = GOLD：dialog 0x1B3 / `party_total_gold += amount`
   - `2` = BATTLE EVENT CONSEQUENCE：**直接** `(*data_fd2_battle_ai_post_action_consequence_table[ushort_value])()`
     呼叫 handler，**不**寫入 `data_fd2_battle_ai_post_action_consequence_idx` global
   - `3` = SCRIPTED DIALOG：`fd2_display_dialog_scene(data_fd2_current_chapter_text_ptr, page=ushort_value, ...)`

### 為什麼 type 2 不走 Path 1？

Tile-step trigger (Path 1) 是「玩家踩到 tile → 下回合 dispatch」的 deferred
模式，需要回到 main loop 才會 fire。Death-drop type 2 (Path 2) 是「殺死敵人 →
立即 dispatch」的 in-flight 模式，因為 killer 已經在動畫流程中。兩者不共用
state variable 是設計上的分離。

ch9 援軍 (handler_1F)、ch7/13/14/25/27/28/29 各種 cinematic / dyn-turn-event
都有 entry 進這個 90-entry table；Path 2 (death drop type 2) 是給敵人「死亡
觸發劇本事件」用的（少見，多數 entry 是 Path 1 的 tile-step trigger）。
