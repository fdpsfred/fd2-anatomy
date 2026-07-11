# battle

戰鬥系統：戰鬥數值 pipeline、三層敵方 AI、回合循環、共用亂數器。runtime_char 的 80-byte
佈局、entry chain 與主迴圈骨幹見 `overview.md`；玩家法術選單／施法動畫／effect applier
見 `spell.md`；移動範圍 floodfill 與尋路見 `pathfind.md`。

## 驗證對象

- src：`battle/btl_ai.c`、`battle/btl_aitg.c`、`battle/btl_aisc.c`、`battle/btl_init.c`、
  `battle/btl_turn.c`、`battle/battle.c`；亂數暖機另涉 `life/main.c`。
- 主要 Ghidra 對象（name@addr，位址即時核對）：
  - `fd2_run_full_turn_cycle @ 0x1A30B`（回合循環總排程）
  - `fd2_enemy_turn_phase_team0 @ 0x1D8BA`、`fd2_npc_turn_phase_team1 @ 0x1D80B`
  - `fd2_enemy_turn_action_dispatcher @ 0x13A9F`、`fd2_attack_action_dispatch @ 0x14EF0`
  - `fd2_ai_score_physical_attack @ 0x14237`、`fd2_ai_score_offensive_spell @ 0x1598A`、
    `fd2_ai_score_item_use @ 0x1567E`
  - `fd2_score_spell_candidate @ 0x15B77`、`fd2_score_item_candidate @ 0x15880`、
    `fd2_tally_chars_with_zero_at_field @ 0x15DA2`
  - `fd2_check_char_status_immunity @ 0x1F183`
  - `fd2_execute_attack_damage_calculation @ 0x1ECC7`、`fd2_calculate_combat_hit_outcome @ 0x29F72`、
    `fd2_execute_combat_hit_cinematic @ 0x2939D`
  - `fd2_calc_magic_damage @ 0x1C75E`、`fd2_apply_damage_and_award_xp @ 0x1C81F`
  - `fd2_init_runtime_char_for_battle @ 0x10C50`（spawn 時算數值）
  - `fd2_process_xp_and_level_up_for_char @ 0x1E292`、`fd2_roll_stat_gain_and_show_message @ 0x1E529`
  - `fd2_advance_rng_state @ 0x4E893`（狀態 `data_fd2_shared_rng_seed @ 0x627B8`）
- 相關資源：FDFIELD.DAT（per-chapter tile map ＋ char_spawn record，見 resource_info/fdfield.md）。
  角色／敵人／職業數值表的欄位與數值在 assets/（enemies、tables）。

## 架構概觀

FD2 沒有獨立的 `battle_turn_loop()` 或 `start_battle()`。戰鬥狀態由 chapter init 把地圖與
runtime_char 佈好之後，靠 `main` 外迴圈反覆呼叫 `fd2_game_main_loop` 推進；玩家全部單位行動完
會觸發 `fd2_run_full_turn_cycle`（@ 0x1A30B）跑完 NPC／敵方回合再回到玩家回合。「回合」概念分散在
`fd2_game_main_loop` 的鍵盤分派、`fd2_enemy_turn_phase_team0` / `fd2_npc_turn_phase_team1`，以及
底下依 `combat_aux_block[0xD]` 低 nibble 分派 AI behavior class 的
`fd2_enemy_turn_action_dispatcher`。

## 亂數產生器 (RNG)

戰鬥所有機率判定（命中、爆擊、雙擊、傷害 jitter、法術命中、升級與轉職的屬性 roll）都抽同一個共用
亂數器 `fd2_advance_rng_state @ 0x4E893`：

- 演算法：`seed = ROL16(seed + 0x9014, 3)` —— 16-bit 先加 0x9014，再向左循環移位 3 bit。
- 回傳的是新 seed **零擴展**成的 32-bit 值：函式進入時先 `XOR EAX,EAX` 再 `MOV AX,seed`，故高 16 bit
  恆為 0、回傳值恆非負。這正是升級 roll `gain = min + rng % range` 必落在 `[min, min+range)` 的前提。
- 狀態存 `data_fd2_shared_rng_seed @ 0x627B8`，`fd2_advance_rng_state` 是唯一存取者；BSS 清 0 起始，
  第一次前進得 `ROL16(0x9014, 3) = 0x80A4`。

暖機：`main`（@ 0x25BF4）啟動時先把 BIOS 午夜 tick 計數器（BDA `0040:006C`，線性位址 0x46C 的
16-bit word）讀進章節環境 palette 動畫的節流 latch（見 overview.md），緊接著跑一段亂數暖機迴圈：
以 Watcom C 標準庫 `rand()`（@ 0x3C92D）取值對 0x100 取餘，把 `fd2_advance_rng_state()` 連續前進
`rand() % 0x100` 次，先推進共用 seed。此 build 未呼叫 `srand`，`rand()` 是預設種子的決定性 LCG。

## 出場數值 pipeline

角色／敵人在 spawn 時由 `fd2_init_runtime_char_for_battle @ 0x10C50` 依 FDFIELD per-char record
的 level 算出戰鬥數值，寫進 runtime_char slot：

- 玩家陣營單位（record char_id < 0x44）走 base + growth 成長公式（HP/MP = base + growth×(level-1)；
  AP/DP/DX = base + growth×level）。
- 敵方／友軍 NPC 單位（record char_id >= 0x44）不用 growth 表，HP/MP/AP/DP/DX 五欄都是「每等級係數 ×
  level」的純乘法；MV（+8）、RA（+0）、CL（+1）直接複製、不乘 level。因此敵人表列的數值都是每等級係數，
  實際出場值 = 係數 × 該章 record 給的 level。**數值細節不在此**：欄位語意與 68 筆係數見
  assets/enemies.md 與 assets/tables/enemy_data.md。

擊殺經驗：EX（enemy_data +9）同樣是每等級 XP 係數。法術／間接傷害路徑（`fd2_apply_damage_and_award_xp`）
給 `XP = EX × 被殺者 level`；物理攻擊路徑（`fd2_execute_attack_damage_calculation` /
`fd2_calculate_combat_hit_outcome`）再 `÷ 攻擊者 level`（攻擊者為進階職業 class 9..0x18 或 char_id
0x1C 時，攻擊者 level 先 +30）。未擊殺時皆按 `damage / hp_max` 比例折算部分 XP。

移動力欄（runtime_char +0x3B）就是各 AI／玩家走位與施法選點傳給
`fd2_init_movement_range_floodfill` / `fd2_pathfind_to_destination` 的最大步數預算（見 pathfind.md）；
player 來源 `character_base +7`、enemy 來源 `enemy_data +8`，升職再加 promotion 表 byte[1]。

## 回合循環

`fd2_run_full_turn_cycle @ 0x1A30B` 是「玩家 → NPC → 敵方 → 新玩家回合」的總排程，由
`fd2_check_all_player_acted_or_incapacitated`（所有玩家單位都死亡／已行動／麻痹時）與 field command menu
觸發。各 phase 之間任一 `data_fd2_chapter_event_or_battle_end_code != 0` 就提前結束：

1. **Phase A**：玩家陣營自動回血 —— 每個合格單位（team 2、未死未行動、無中毒無麻痹、HP 未滿）
   回 `hp_max/5`（夾到上限）並標記已行動。
2. **Phase B**：`fd2_fire_chapter_turn_events_for_phase(1)`（end-of-player-turn 章節事件）＋
   `fd2_tick_status_effects_and_show_messages(1)`。
3. **Phase C**：NPC（team 1）回合 `fd2_npc_turn_phase_team1`。
4. **Phase D**：敵方回合 banner ＋ phase-0 章節事件 ＋ 狀態 tick(0)。
5. **Phase E**：敵方（team 0）回合 `fd2_enemy_turn_phase_team0`。
6. **Phase F**：turn counter +1、「TURN N」揭示動畫、phase-2 章節事件、重置游標到玩家單位 0。

狀態 tick（`fd2_tick_status_effects_and_show_messages @ 0x1A866`）分兩趟：先對中毒單位（+0x25 != 0）
扣 `hp_max/10`、播中毒訊息；再把 +0x22..+0x27 六個計時槽各減 1，歸零時播解除訊息並
`fd2_recalculate_combat_stats` 拿掉到期加成。FD2 沒有「睡眠」狀態，+0x26 是麻痹。

## 物理傷害公式

AI 與玩家共用同一條物理傷害公式，由兩個等價函式實作（同公式、不同用途）：

| 位址 | 名稱 | 用途 |
|---|---|---|
| `0x0001ECC7` | `fd2_execute_attack_damage_calculation` | 直接套用：自己寫 `hp_current`，含命中／爆擊動畫 |
| `0x00029F72` | `fd2_calculate_combat_hit_outcome` | 只算 outcome（不寫 HP），由 cinematic 跨 frame 漸進套用 |

公式（單次命中）：

```
AP_eff = AP + (data_fd2_battle_tile_attr_mv_modifier_table[tile_id]  * AP) / 100   # 攻擊者免疫則跳過
DP_eff = DP + (data_fd2_battle_tile_attr_def_modifier_table[tile_id] * DP) / 100   # 防禦者免疫則跳過
命中: rng % 100 < attacker.+0x4C - defender.+0x4E                  # 不命中則傷害 0
爆擊: rng % 100 < data_fd2_battle_job_crit_rate_table[attacker.job_id - 1]  # 爆擊把 DP_eff 折半
damage = (AP_eff - DP_eff) * 9 / 10           # < 0 取 0
jitter_range = damage / 9
if jitter_range != 0: damage += rng % jitter_range
hp_current -= damage                          # 夾到 0
```

整數除法皆截尾向零，故小 AP/DP（如 16/4）時 ±% 的地形修正會整除歸零、傷害與地形無關。tile_id 取自
`fd2_read_tile_attribute_at_pos` 回填的 buffer +5。武器 class（item_effect +9）另有分支：class 4 =
額外爆擊率（+ weapon_elem）、class 3 = 必雙擊、class 2 = 命中前先 roll 下毒（`rng%100 < weapon_elem`
成立則寫毒 duration `rng%4 + 2` 到 defender +0x25，miss 也可能中毒）。

**runtime_char 命中/迴避 stat**：物理命中判定為 `攻擊者 +0x4C(dx_current) - 防禦者 +0x4E(stat4_current)`，
故 **+0x4C = 物理命中率、+0x4E = 物理迴避率**（兩者皆由 `fd2_recalculate_combat_stats` 從 DX 基底 +
不同裝備加成欄位算出）。

**玩家攻擊路徑**：`fd2_game_main_loop`（游標在我方單位上按 Space/Enter）→
`fd2_player_action_menu_loop`（選目的地 tile，可不移動）→ `fd2_player_inline_action_menu_dispatch`
（動作選單 Attack=0/Spell=1/Item=2/Wait=3）→ 選 Attack 後 `fd2_wait_for_action_target_input` 選目標 →
`fd2_play_full_combat_cinematic @ 0x28A6C` → `fd2_execute_combat_hit_cinematic @ 0x2939D`。後者先抽
**1 次 RNG 做 3%（`rng%100 < 3`）雙擊判定**（命中 2 下），武器 class 3 也會強制雙擊；再每下呼叫
`fd2_calculate_combat_hit_outcome` 算傷害並跨 hit-frame 漸進寫入目標 HP。命中後若防禦者近戰且相鄰會接
一段反擊 cinematic（`fd2_check_can_counter_attack`，武器 `item_effect.range_min == 1`）。固定 seed 下
整條 RNG 序列為：雙擊 → 命中 → 爆擊 → jitter（每項依分支決定是否抽）。

## 法術傷害公式

`fd2_calc_magic_damage @ 0x1C75E`（傷害「數字」計算，不含施法動畫；施法動畫與 effect applier 見
spell.md）：

```c
target_job = runtime_char[target].job_id (+0x20);
spell      = fd2_get_spell_effect_entry(spell_id);
base_power = *(int16*)spell;                                  // spell_effect[0]
resist     = data_fd2_battle_job_magic_resist_table[target_job - 1];
damage     = base_power * resist / 10;                        // 線性衰減
chance_pct = spell[2];                                        // 命中率欄
if (spell_id 在 10..12) 且 fd2_check_char_status_immunity(target) → return 0;   // 免疫者無效
rng = fd2_advance_rng_state();
if (rng % 100 >= chance_pct) return 0;                        // miss
return fd2_apply_damage_and_award_xp(target, damage);
```

傷害以「base_power × job_magic_resist[target_job-1] / 10」線性折算：resist 欄經 `/10` 縮放傷害
（resist = 10 為全額、0 為完全抵抗），與攻略「法術攻擊 = 最大傷害 × 0.X」吻合。
`data_fd2_battle_job_magic_resist_table` 的每職業 resist 值見 assets/tables。

## Enemy AI 主架構

```
[Per-frame from fd2_run_full_turn_cycle, 敵方 / NPC phase]

  fd2_enemy_turn_phase_team0 @ 0x1D8BA        兩趟 enemy AI 主迴圈
    ├─ Pass 1：smart caster 優先
    │     for char in team 0（未死未行動、無麻痹）:
    │        fd2_ai_score_offensive_spell + fd2_ai_score_item_use
    │        if best_spell_score >= 6 OR best_item_score >= 6: enemy_turn_action_dispatcher(i, 0)
    └─ Pass 2：全員無條件 dispatch（pass 1 已動者被 acted bit 擋掉）

  fd2_npc_turn_phase_team1 @ 0x1D80B          友軍 NPC AI 單一迴圈
    └─ for char in team 1: enemy_turn_action_dispatcher(i, 1)

  fd2_enemy_turn_action_dispatcher @ 0x13A9F  依 combat_aux_block[0xD] & 0x0F 分派 AI behavior class
    └─ 開頭 if (flags(+5) & 0x05) return  跳過已死/特定狀態單位

  fd2_attack_action_dispatch @ 0x14EF0        三路評分動作選擇器
    ├─ fd2_ai_score_physical_attack → best_physical_score @ 0x53C4F + tile/idx @ 0x53C43/47/4B
    ├─ fd2_ai_score_offensive_spell → best_spell_score    @ 0x53C23 + @ 0x53C27/2B/2F
    ├─ fd2_ai_score_item_use         → best_item_score    @ 0x53C33 + @ 0x53C37/3B/3F
    └─ 取 max 執行 fd2_execute_ai_physical_attack / _offensive_spell / _item_use

  Helpers（scoring 失敗 / 走位用）:
    fd2_ai_seek_optimal_position @ 0x14121      job-aware pathfind（見 pathfind.md）
    fd2_ai_walk_to_target_tile   @ 0x14B78      path execute
    fd2_ai_advance_to_nearest_team_target @ 0x13E9C
    fd2_ai_pass_turn_with_heal   @ 0x13FD4      passing-turn 回 hp_max/5
    fd2_count_usable_inventory_slots @ 0x1B8A6
    fd2_get_inventory_slot_item_id   @ 0x1B722
    fd2_score_item_candidate         @ 0x15880
    fd2_score_spell_candidate        @ 0x15B77

  Per-action postlude（phase 迴圈內）:
    if (data_fd2_battle_ai_post_action_consequence_idx != 0xFF):
        data_fd2_battle_ai_post_action_consequence_table[idx](i)  ← 反擊 / 死亡 / 狀態
    data_fd2_chapter_post_action_handler_table[current_chapter_id](i) ← 章節觸發事件
    if (data_fd2_chapter_event_or_battle_end_code != 0): break loop
```

## AI 評分三路

`fd2_attack_action_dispatch` 依序呼叫三個 scorer，各把最佳候選寫進自己的一組全域，再挑分數最高者執行：

| Scorer | 類別 | score global | target_x/y | id/slot global |
|---|---|---|---|---|
| `fd2_ai_score_physical_attack @ 0x14237` | 物理攻擊 | `0x53C4F` | 0x53C43 / 0x53C47 | 0x53C4B (target_idx) |
| `fd2_ai_score_offensive_spell @ 0x1598A` | 法術攻擊 | `0x53C23` | 0x53C27 / 0x53C2B | 0x53C2F (spell_id) |
| `fd2_ai_score_item_use @ 0x1567E` | 道具使用 | `0x53C33` | 0x53C37 / 0x53C3B | 0x53C3F (slot_idx) |

### 物理評分公式

```
effective_AP/target_DP = 各自 base + 地形 % 修正（僅在該單位「狀態免疫」時套用）
raw_dmg     = effective_AP - target_DP
score_class = 0     若 raw_dmg <= 2
            = 8     一般命中
            = 0x12  若 raw_dmg > target.HP_current(+0x40)  → 同時 raw_dmg *= 2 (kill shot，最高優先)
反擊加成: 若 fd2_check_can_default_attack_target(target, tile) == 1 → raw_dmg += effective_DP - target_AP
主角加成: 若 target.char_id(+8) == 0（主角索爾）→ raw_dmg = raw_dmg * 3 / 2
best 由 (score_class, 再 raw_dmg tie-break) 決定
```

注意：此 scorer 的地形修正只在 `fd2_check_char_status_immunity != 0`（該單位狀態免疫，如 job 0x13）時套用，與實際傷害函式 `fd2_execute_attack_damage_calculation`（免疫則跳過、未免疫才套地形）方向相反——AI 評分與真正傷害結算對地形加成的處理並不一致。

### 三類 executor

| Winner | Executor | 備註 |
|---|---|---|
| 物理 | `fd2_execute_ai_physical_attack @ 0x1548E` | 全套物理動畫；快慢速由 `data_fd2_ui_game_speed_flag @ 0x53AF9` 切換 |
| 法術 | `fd2_execute_ai_offensive_spell @ 0x15311` | best_spell_score < 6 則不施法 |
| 道具 | `fd2_execute_ai_item_use @ 0x15055` | 短距 vs 長距投射由 `item_effect.range_class`（offset 0x10）切換 |

### Tie-break 規則

- 三者皆 `score < 6`：abort（return 0），AI 本回合無動作。
- `物理 == 法術 > 道具`：`spell_id < 0xB` 時比 `spell.base_damage` 與 `caster.AP - target.DP`（後者大於
  前者才改用物理，否則施法）；`spell_id >= 0xB` 時由 `caster.combat_aux_block[0xD] & 0x40` 旗標決定
  施法 or 物理。
- `物理 == 道具 > 法術`：同樣由 `combat_aux_block[0xD] & 0x40` 旗標決定物理 or 道具。
- 其餘取嚴格最大者對應的 executor。

## Per-candidate scorer

### `fd2_score_item_candidate @ 0x15880` 依 `item_effect[0xD]` effect-code

- `0x05` / `0x0D`（HP-damage 型）：per target，HP/max 比例決定 0/3/8 分（`hp <= max/3` → 8、
  `hp > max/2` → 0、其餘 3）。`combat_aux_block[0xD] & 0x80` 旗標成立則 ×3（高價值目標）。
- `0x14` / `0x15` / `0x18`（spell-wrapper items）：門檻取 wrapped spell 的 base damage
  （effect-code 0x18 時取 item +0xE）；`門檻 >= 目標 HP` → 0x12 (kill shot)，否則 8。
- 其他 effect-code：不計分（0）。

### `fd2_score_spell_candidate @ 0x15B77` 依 `spell_id` 分段

| spell_id | 類別 | 評分邏輯 |
|---|---|---|
| 0..9 | 基本攻擊法術 | 每目標：`HP_current(+0x40) < spell base_power` → **0x18** (kill shot)，否則 8；目標為主角（char_id 0，`pChar[8]==0`）→ ×`data_fd2_battle_ai_enemy_spell_score_multiplier_15`（≈1.5）|
| 10..12 | 攻擊法術（排除免疫目標）| 同 0..9，但每個目標先過 `fd2_check_char_status_immunity`；免疫者直接跳過不計分 |
| 13..16 (0xD..0x10) | 治療法術 | `HP < max/3` → 8；`HP < max/2` → 3；heal-boost 旗標（`pChar[0x34] & 1`）×2 |
| 17..19 (0x11..0x13) | 強化 buff | `fd2_tally_chars_with_zero_at_field(n_targets, targets, spell_id+0x11, 3)`：對尚未帶該 buff（runtime_char +0x22/+0x23/+0x24 欄為 0）的每個目標各 +3 |
| 0x14 | 解毒 | 每目標：`+0x25(毒) != 0` → +6 |
| 0x15 | 解麻痹 | 每目標：`+0x26(麻痹) != 0` → +6 |
| 0x16 | 沉默 | 目標未沉默（`+0x27 == 0`）且有可用法術 → +6（優先沉默詠唱者）|
| 0x1A | 下毒攻擊 | `fd2_tally_chars_with_zero_at_field(n_targets, targets, 0x25, 4)`：對 `+0x25(毒)` 欄為 0 的每個目標各 +4 |
| 0x1B | 麻痹攻擊 | `fd2_tally_chars_with_zero_at_field(n_targets, targets, 0x26, 4)`：對 `+0x26(麻痹)` 欄為 0 的每個目標各 +4 |
| 其他 | 無 | return 0 |

其中 `fd2_check_char_status_immunity @ 0x1F183`（**狀態免疫檢查**，非視線 LoS）：`job_id == 0x13` 或
`archetype_flag` 為 4/5（portrait 0x1C 例外）時回 1，代表該單位免疫狀態效果與地形加成。
`fd2_tally_chars_with_zero_at_field @ 0x15DA2` 的實簽名是
`(int len, char_idx_arr, int field_offset, int weight)`：對 `char_idx_arr[0..len-1]` 中該欄為 0 的每個
單位累加 `weight` 並回傳總分（一個清空的狀態欄代表效果尚未施加，該目標值得計分）。

## 關鍵數值結論

- **AI kill-shot 權重**：物理 `0x12`(18)、item `0x12`(18)、**spell `0x18`(24)** → AI 顯著偏好用法術秒殺。
- **Pass-turn auto heal**：`fd2_ai_pass_turn_with_heal` 在 AI 沒有有效行動時自動回 `hp_max/5`；
  FD2 重型敵人「拖不死」的程式根源。
- **Two-pass enemy phase**：smart caster 第一輪先動（佔 AoE 位置與秒殺），melee 第二輪。AI 戰術設計，非 bug。
- **AI behavior class**（`combat_aux_block[0xD]` 低 nibble）= 每個單位的「個性」，見下表。

## AI behavior class semantic

`fd2_enemy_turn_action_dispatcher @ 0x13A9F` 依 `combat_aux_block[0xD] & 0x0F`（值域 0..15）分派；有明確
行為的是 class 0,1,2,3,4,5,7,8,9,10,11，其餘（6 與 12..15）落到共用 postlude、本回合不行動。走位 helper
的細節見 pathfind.md。

| class | 名稱 | 行為 |
|---|---|---|
| 0 | default_attacker | `attack_action_dispatch(0)`；失敗 → seek_optimal → advance_to_nearest → pass_with_heal |
| 1 | defensive_kiter | `attack(1)`；失敗 → `fd2_ai_seek_optimal_position` → pass_with_heal |
| 2 | aggressive_physical | `attack(2)`；失敗 → 重跑 `fd2_ai_score_physical_attack` 後 pass_with_heal |
| 3 | targeted_approach | `attack(3)`；失敗 → `fd2_find_char_by_id_or_template(ai_aux)` 走向該角色，找不到則 seek → advance |
| 4 | scripted_move | pan + `fd2_ai_walk_to_target_tile` 走向 `(aux[0x35], aux[0x36])`；到不了 → pass_with_heal（不攻擊）|
| 5 | item_pickup | `attack(5)`；失敗 → 找 pickup tile、走過去撿取，之後把自身 class 改成 7 |
| 7 | charge_dash | pan + 走向 `(aux[0x35], aux[0x36])`；抵達即 `fd2_mark_char_as_dead`（kamikaze / 劇本退場）|
| 8 | hard_skip | 純 return（stunned / scripted-passive）|
| 9 | advance_to_char | `fd2_find_char_by_id_or_template(ai_aux)` 走向該角色；找不到則退回 class 0 attack |
| 10 | hardcoded_attack | `attack(10)`；失敗 → pan + 走向 `(aux[0x35], aux[0x36])` |
| 11 | smart_caster | 先評分＋施攻擊法術（score>=6），再評分＋物理（>=6）；都不行 → seek → pass_with_heal（boss 級）|

`fd2_set_combat_aux_block_byte_d_low4_for_char_range @ 0x3419C` 供章節事件在劇情節點批次改寫一段單位的
AI class（常見 0/3/7）。

## 死亡掉落與 post-action consequence dispatch

兩條獨立 path 都會 dispatch `data_fd2_battle_ai_post_action_consequence_table @ 0x51B91` 內的 handler，
但機制不同：

### Path 1 — Tile-step trigger（透過全域 state 延遲觸發）

`fd2_check_tile_event_post_action @ 0x13A44` 在 player/AI 走到或在 tile 上行動時呼叫：

1. 讀 tile attribute；`(tile_buf[4] & 0x60) == 0` 且 terrain_class != 0 才處理。
2. 從 `tile_event_data_table + (terrain_class - 1) * 2` 取 record，讀 `+0x33` = consequence_idx、
   `+0x34` = event_type。
3. `consequence_idx != 0xFF` 且 event_type 匹配（0 = 走進 tile、1 = 在 tile 行動）→ 把 idx latch 進
   `data_fd2_battle_ai_post_action_consequence_idx`。
4. 下一輪 phase 迴圈看到該 global != 0xFF，dispatch
   `data_fd2_battle_ai_post_action_consequence_table[idx]()` 後 reset 為 0xFF。

### Path 2 — Death drop（`fd2_process_battle_drop_entries` 直接觸發）

殺死敵人後，`fd2_collect_pending_death_drops` / `fd2_collect_dead_char_drops` 掃 HP=0 且
`combat_aux_block[10] != 0xFF` 的 3-byte drop block，交給 `fd2_process_battle_drop_entries(recipient,
count, drops)` 逐筆依 type byte 派發：

- `0` = ITEM：dialog 0x1B0 / `fd2_add_item_to_inventory`（team==2 才收）。
- `1` = GOLD：dialog 0x1B3 / `party_total_gold += value`（team==2 才收）。
- `2` = BATTLE EVENT：**直接** `data_fd2_battle_ai_post_action_consequence_table[value](recipient)`，
  **不**經過 `data_fd2_battle_ai_post_action_consequence_idx` global。
- `3` = SCRIPTED DIALOG：`fd2_display_dialog_scene(current_chapter_text, page=value, ...)`。

### 為什麼 type 2 不走 Path 1？

Path 1 是「玩家踩到 tile → 下回合 dispatch」的 deferred 模式，需回到 main loop 才 fire。Death-drop type 2
是「殺死敵人 → 立即 dispatch」的 in-flight 模式（killer 已在動畫流程中）。兩者刻意不共用 state variable。
多數 consequence entry 是 Path 1 的 tile-step trigger；type 2 少見，專給敵人「死亡觸發劇本事件」用。
各章 handler 明細見 field.md 與 chapters/。

## 升級與屬性成長

`fd2_process_xp_and_level_up_for_char @ 0x1E292` 在累積 XP 足夠且未達等級上限時逐級升級：每級呼叫
`fd2_roll_stat_gain_and_show_message @ 0x1E529` roll 5 個 stat 槽、學會該等級的法術、`recalculate_combat_stats`，
再扣 100 XP。成長 roll：`range = growth_pair[1] - growth_pair[0]`（第二 byte 是 exclusive 上界＝最大成長
+1）；`range == 0` 時不抽 RNG、`gain` 恆為 min，否則 `gain = min + fd2_advance_rng_state() % range`，落在
`[min, min+range)`。剩餘 XP 存 runtime_char +0x3C（EX carry）帶到下次——但一般角色升到 30 級時這段剩餘 XP
會被強制歸零而非帶走（per-call 停止門檻 30 與真正上限 40 不一致所致），見 known_bugs.md #2。growth 表欄位語意與數值、
升級 roll 的組語證據、以及一支「升級最大值修改版」exe 每級誤給 `_max`（比原版真正最大多 1）的
off-by-one，見 assets/tables/character_growth.md。

此結算掛在 AI 物理攻擊的「目標」上（`fd2_execute_ai_physical_attack` 結尾 `process_xp(target_idx)`），且不檢查
對象陣營、early-return 時漏清 `pending_xp_credit`；當 team 1 友軍攻擊敵人、又逢殘留經驗外洩時，敵人會被灌經驗
而升級，並因 portrait ≥ 0x44 越界查成長表而屬性暴增，見 known_bugs.md #3。

## Battle lifecycle（章節層）

| 位址 | 名稱 |
|---|---|
| `0x00022E5C` | `fd2_play_game_over_sequence` |
| `0x00010B4E` | `fd2_load_chapter_portraits_and_dump_tmp` |
| `0x000205DA` | `fd2_init_battle_state_for_chapter`（清旗標、載 battle data、首次 paint、fade-in、turn=1）|

## runtime_char 存取 helper

`fd2_get_inventory_slot_item_id @ 0x1B722`（回傳 `runtime_char[char_idx].inventory_slots[slot*2+1]`
= 該 inventory slot 的 item_id）。玩家法術選單、施法動畫、item/spell effect applier 見 spell.md。
runtime_char struct 完整 layout 見 overview.md。
