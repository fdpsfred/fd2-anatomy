# data_fd2_battle_enemy_data_table

`.object3 @ 0x61AF9`，68 entries × 10 bytes = 680 bytes。entry 0 前 6 byte
`01 02 12 00 00 05` 可當定位 anchor。位址空間換算見 `assets/tables/_index.md`。

Ghidra type `enemy_entry[68]`；C struct `enemy_data` 定義在 `src/include/types.h`。

## struct layout（10 B）

HP / MP / AP / DP / DX / EX 六欄都是**每等級係數**（per-level 值），不是最終數值。

| offset | size | types.h 欄名 | 攻略縮寫 | 意義 |
|---|---|---|---|---|
| +0 | 1 | race_id | RA | 種族 ID（見 `assets/races.md`）；生成時直接複製 |
| +1 | 1 | class_id | CL | 職業 ID；直接複製 |
| +2 | 2 | hp | HP | 每等級 HP 係數（u16 LE）；生成 HP = 係數 × level |
| +4 | 1 | mp | MP | 每等級 MP 係數；× level |
| +5 | 1 | ap | AP | 每等級 AP 係數；× level |
| +6 | 1 | dp | DP | 每等級 DP 係數；× level |
| +7 | 1 | dx | DX | 每等級 DX 係數；× level |
| +8 | 1 | mv | MV | **移動力（非魔法抗性）**；直接複製，不乘 level |
| +9 | 1 | exp_reward | EX | 每等級擊殺 XP 係數 |

## 生成公式（係數 × level）

敵 / 友軍 NPC 單位（char_id ≥ 0x44）在戰鬥生成時
`fd2_init_runtime_char_for_battle @ 0x10C50` 讀 FDFIELD per-char record 的 level 做純乘法：

```
HP = hp × level;  MP = mp × level;  AP = ap × level;  DP = dp × level;  DX = dx × level
MV(+8)、RA(+0)、CL(+1) 直接複製，不乘 level
```

玩家角色（char_id < 0x44）走 base + growth 成長公式（見 `character_base.md`），不用本表。
因此本表數值（如空魔神 HP 300）都是每等級係數，實際出場數值 = 係數 × 該章 FDFIELD 記錄
給的 level。

## +8 MV 消費鏈（與魔法抗性無關）

`mv(+8)` 原值寫入 runtime_char `+0x3B`（combat_aux_block[0x14] 移動力欄，玩家的同一欄來自
`character_base.mv`）。所有讀取端都把它當「移動距離預算」：玩家行動選單與 AI 走位 / 施法選點
把它傳給 `fd2_init_movement_range_floodfill` 與 `fd2_pathfind_to_destination` 當最大步數；
升職時把 `class_promotion` 表 byte[1] 加到此欄（text page 0x254「移動力增加N點」）。魔法傷害
的抗性另查 per-job 的 `data_fd2_battle_job_magic_resist_table @ 0x51F96`（見
`job_magic_resist.md`），與本欄完全無關。資料佐證：光束炮座、火龍、雷龍、暗黑龍等不能移動的
單位 `mv = 0`，騎兵系 7–9。

## +9 EX 擊殺 XP 係數

擊殺結算：XP = `exp_reward` × 被擊殺者 level（runtime +0x21）。物理攻擊路徑
（`fd2_calculate_combat_hit_outcome @ 0x29F72`、`fd2_execute_attack_damage_calculation @ 0x1ECC7`）
再 ÷ 攻擊者 level（攻擊者為進階職業 class_id 9..0x18 或 char_id 0x1C 時，攻擊者 level 先
+30）；法術 / 間接傷害路徑（`fd2_apply_damage_and_award_xp @ 0x1C81F`）不除。未擊殺時兩路徑
皆按 damage / hp_max 比例折算部分 XP。

## char_id namespace

`enemy_id = char_id − 0x44`。`fd2_init_runtime_char_for_battle @ 0x10C50`：char_id < 0x44
走 `fd2_get_char_base_entry`，≥ 0x44 走 `fd2_get_enemy_data_entry(char_id − 0x44)`。

## entry sample

Entry 0（士兵 友方）：`01 02 12 00 00 05 02 01 04 1E`

```
race_id=1 class_id=2 hp係數=18 mp=0 ap係數=5 dp係數=2 dx係數=1 mv=4 exp係數=30
level 3 出場即 HP 54 / AP 15 / DP 6 / DX 3、MV 4
```

## 邊界

`data_fd2_battle_character_base_table` 在 `0x61DA1`，enemy_data 大小 =
`0x61DA1 − 0x61AF9 = 0x2A8 = 680 = 68 × 10`；兩表零 padding 緊鄰。

## 全 68 entries

數值見 `assets/enemies.md`。
