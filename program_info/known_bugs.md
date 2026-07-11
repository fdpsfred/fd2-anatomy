# 原版遊戲已知 bug

炎龍騎士團合輯版「二代」(FD2) 原版執行檔 `FD2.LE` 中，經逆向工程確認的**遊戲程式／資料缺陷**。
本檔只收錄已完整分析、成因明確的原版 bug；尚未釐清的逆向問題見 `open_issues.md`，重建過程遇到的
build/runtime bug 見 `rebuild_info/build_test/`。

每筆列出：現象、成因、資料誤植的本質、影響與迴避，以及可逐條核對的驗證對象。

## 清單

| # | 名稱 | 系統 | 後果 |
|---|---|---|---|
| 1 | 凱拉斯幽靈轉職（聖戰士） | 轉職選單 (`promote.c`) | 選單出現無效轉職，確認後遊戲 crash |
| 2 | 升到 30 級時剩餘經驗值歸零 | 升級結算 (`btl_turn.c`) | 一般角色跨入 30 級時溢出的經驗值被丟棄，且該次結算無法再升級 |
| 3 | 敵人升級、屬性暴增 | AI 物理攻擊 / 升級結算 (`btl_ai.c` / `btl_turn.c`) | NPC 友軍攻擊敵人時，殘留經驗值灌進敵方目標使其升級，越界成長表令屬性暴增 |

---

## 1. 凱拉斯幽靈轉職成「聖戰士」→ crash

### 現象
凱拉斯（char_id 0x10，本職龍劍士）練到 LV20 以後，會出現在城鎮的轉職選單，並可選擇轉職成
「聖戰士」。一旦確認轉職，遊戲當掉。其餘進階職業角色（包含蘭斯洛特）都不會出現在轉職選單。

### 成因
轉職候選人的資格由 `fd2_build_promotion_candidates_with_targets`（`src/ui_menu/promote.c`）以三個
條件判定：等級 ≥ 20、`portrait_id < 0x12`、`portrait_id != 7`。`portrait_id` 在角色被招募進隊時等於
`char_id`（`fd2_init_runtime_char_from_base_growth`，`src/battle/btl_init.c`，寫入
`runtime_char +0x07`）；轉職目標則算成 `portrait_id + 0x20`。

程式假設 char_id `0x00–0x11` 這一段**全是可轉職的基礎職業角色**（同函式註解即寫
「basic classes 0..0x11 only」）。但這一段裡混入了兩個**進階職業**角色：

- 蘭斯洛特（char_id 7，聖騎士）—— 以 `portrait_id != 7` **明確排除**（註解稱其為「a fixed class
  with no promotion path」）。
- 凱拉斯（char_id 0x10，龍劍士）—— **同類狀況，卻沒有對應的 `!= 0x10` 排除**。

凱拉斯因此通過資格判定，目標算成 `0x10 + 0x20 = 0x30`。轉職結果表
`data_fd2_battle_class_promotion_data_table`（`src/table/btltab3.c`，`@0x615FE`）第 16 筆（class 0x30）
存的是 job `0x0A`（聖戰士）——這是「戰士 → 聖戰士」的轉職，與凱拉斯的龍劍士本職完全不符，是該
槽位殘留的錯配佔位資料。對應的成長 entry `data_fd2_battle_character_growth_table[0x30]`
（raw `0a 0c 06 08 02 03 0f 14 00 00 ff`，即 AP 10–12 / DP 6–8 / DX 2–3 / HP 15–20 / MP 0，無習得
法術）同樣既不是標準聖戰士成長、也不是凱拉斯本職成長。

轉職流程從頭到尾**不驗證目標槽位是否為該角色的合法轉職**，只信任前面的 char_id 範圍判定，因此把
這筆錯配的「聖戰士」當成正常選項提供。

### 資料誤植的本質
不是任何單一數值打錯，而是兩件事疊加：

1. **凱拉斯被指派到 char_id `0x10`**，落在程式視為「基礎職業、可轉職」的 `0x00–0x11` 範圍內，儘管他
   本職是進階的龍劍士。
2. **排除清單漏列**：處理同類狀況（進階職業角色誤落基礎職段）的排除，只寫了蘭斯洛特（`!= 7`），
   漏了凱拉斯（`!= 0x10`）。

槽位 0x30 殘留的「聖戰士」是這個漏洞被觸發時，凱拉斯依 `char_id + 0x20` 落到的錯配轉職資料。

### crash 本身
確認轉職會使遊戲當掉（實機確認）。crash 成因**不是** portrait 缺圖：portrait 0x30 是 `FDICON.B24`
中真實存在的圖，轉職流程用到的 portrait / cinematic（FIGANI）/ 成長表 / 轉職表索引皆在界內。確切
faulting 點尚未靜態定位。

### 影響與迴避
- 正常遊玩勿讓凱拉斯轉職。
- 角色成長數值表已將此路線標為無效並排除：凱拉斯只列龍劍士單段（`tools/growth_table/`）。

### 驗證對象
- src：`ui_menu/promote.c`（`fd2_build_promotion_candidates_with_targets`＝資格與目標；
  `fd2_run_class_promotion_menu_main`、`fd2_execute_class_promotion_with_dialog`＝轉職執行）、
  `battle/btl_init.c`（`fd2_init_runtime_char_from_base_growth`＝`portrait_id = char_id`）、
  `table/btltab3.c`（`data_fd2_battle_class_promotion_data_table`，class 0x30 = 聖戰士）。
- Ghidra：`fd2_build_promotion_candidates_with_targets @0x31793`、轉職表 `@0x615FE`、成長表
  entry 0x30 `@0x622B1`、base 表凱拉斯（char 16）`@0x61DA1 + 16*24`。
- 相關 KB：`assets/tables/class_promotion.md`、`assets/characters.md`、`assets/jobs.md`、
  轉職機制見 `program_info/town_menu.md`。

---

## 2. 升到 30 級時剩餘經驗值歸零

### 現象
一般角色（`portrait_id` 不是主角索爾系 0x1E/0x1F 的角色）在某一次戰鬥結算中升到 30 級的當下，會有兩件事：

- 升到 30 級所需以外的剩餘經驗值被直接清成 0，不會像平常那樣以「EX carry」保留到下一場戰鬥。
- 即使該次累積的經驗值原本足夠再升一級以上，也會被迫停在 30 級，多出來的部分同樣丟失。

之後的戰鬥不再受影響：角色以 30 級進入結算，升級迴圈先把等級加到 31，判斷式不再命中，經驗值恢復正常累積，
仍可一路練到真正的等級上限 40。因此這是「跨入 30 級」當下的一次性經驗值損失，並非把等級鎖在 30。

### 成因
升級結算由 `fd2_process_xp_and_level_up_for_char`（`src/battle/btl_turn.c`，`@0x1E292`）處理。它先算
`remaining_xp = pending_xp_credit + exp_carry`（`exp_carry` ＝ `runtime_char +0x3C`，上一場帶過來的剩餘經驗），
再跑升級迴圈：只要 `remaining_xp > 99` 就升一級（等級存 `status_flags_block[0]`）、roll 屬性、學該等級法術、
重算屬性，並扣掉 100。每扣完 100 之後有一段 per-call 上限判斷：

```c
remaining_xp = remaining_xp - 100;
if ((((portrait_id == 0x1E) || (portrait_id == 0x1F)) &&
     (status_flags_block[0] == 99)) ||
    (status_flags_block[0] == 0x1E)) {   /* 0x1E = 30 */
    remaining_xp = 0;
}
```

對主角（portrait 0x1E/0x1F）而言 99 就是等級上限，把到頂後的剩餘經驗歸零沒有副作用。問題出在第二個條件
`status_flags_block[0] == 0x1E`（30 級）：它對**所有角色**在升到 30 級時套用同一段「剩餘經驗歸零」，但一般
角色的真正等級上限是 40（0x28）——這由函式開頭的進入判斷 `at_level_cap = (status_flags_block[0] == 0x28)`
決定。迴圈結束後 `exp_carry = (uint8)remaining_xp`，因為 `remaining_xp` 已被歸 0，剩餘經驗就此丟失。

### 本質
per-call 的停止門檻（一般角色 30 級）與真正的等級上限（40 級）不一致。歸零這段的用意是把**主角**夾在其真正
上限 99；`== 0x1E` 這個分支卻把同樣的「丟棄剩餘」clamp 錯用到所有角色的 30 級，而 30 對一般角色並不是上限。
於是一般角色在剛好跨入 30 級時被扣掉一次溢出的經驗值。

### 影響與迴避
- 一次性：只在 29→30 這一步觸發，之後仍能正常累積、練到 40 級上限。
- 損失量 ＝ 當下的 `remaining_xp`（該次殺敵／施法給的經驗值加上先前 carry，扣掉升到 30 所耗的整級份），
  最多可接近甚至超過一整級（100），視該擊給的經驗而定。
- 純遊玩無法迴避，但也不影響最終能否練滿 40 級，只是在 30 級門檻少拿一點經驗。

### 驗證對象
- src：`battle/btl_turn.c`（`fd2_process_xp_and_level_up_for_char`：升級迴圈末端 `status_flags_block[0] == 0x1E`
  時 `remaining_xp = 0`；進入判斷 `== 0x28` 才是真正上限）、`include/types.h`（`runtime_char`：
  `status_flags_block[0]` ＝等級 `@+0x21`、`exp_carry` ＝剩餘經驗 `@+0x3C`）。
- Ghidra：`fd2_process_xp_and_level_up_for_char @0x1E292`（per-call cap 判斷；top gate `== 0x28` 40 vs
  迴圈 `== 0x1E` 30）。
- 相關 KB：`program_info/battle.md`（升級與屬性成長）、`assets/tables/character_growth.md`。

---

## 3. NPC 友軍攻擊敵人時，殘留經驗值灌進敵方目標使其升級、屬性暴增

### 現象
在有「我方 AI 友軍」（team 1，例如聯合作戰的王國士兵）參戰的章節，偶爾會看到一個敵方單位在一次戰鬥交換
後獲得經驗值、升級，且 AP／DP／DX／HP／MP 全部暴增到異常數值。純由玩家操作、沒有 AI 友軍的戰鬥不會出現。

### 成因
四個因素疊加：

1. **升級結算對象是攻擊的「目標」而非攻擊者。** AI 物理攻擊 `fd2_execute_ai_physical_attack`
   （`src/battle/btl_ai.c`，`@0x1548E`）在結尾一律呼叫 `fd2_process_xp_and_level_up_for_char(target_idx)`，
   把累積經驗結算給被攻擊的一方（`target_idx`）。這是為了處理「敵人攻擊玩家、玩家反擊殺敵而得經驗」的
   情形——該情形下 target 是玩家。但這個函式同時是**所有** AI 單位（team 0 敵人與 team 1 友軍）共用的
   物理攻擊執行體。當 team 1 友軍攻擊時，`target_idx` 指向的是**敵人**。

2. **升級結算不檢查對象陣營。** `fd2_process_xp_and_level_up_for_char`（`btl_turn.c`，`@0x1E292`）只用
   「`pending_xp_credit != 0`」「非死亡」「未滿級」把關，完全不檢查對象是敵是友，因此會對敵方單位照常
   跑升級流程。

3. **`pending_xp_credit` 會殘留外洩。** 經驗值只有「玩家（`TEAM_PLAYER`＝2）攻擊敵人（portrait ≥ 0x44）」
   時才累進 `data_fd2_battle_pending_xp_credit`（`fd2_execute_attack_damage_calculation` 的
   `attacker.team == TEAM_PLAYER && defender.portrait_id > 0x43` 判斷；`TEAM_NPC`＝1 的友軍攻擊敵人並不
   累進，物理傷害是唯一經此判斷的路徑）。這個全域本應由結算函式在結尾 `pending_xp_credit = 0` 清掉，但
   該函式的兩條 early-return（對象已死、或已滿級）都在清除**之前**就 return。當一個**已達等級上限**（一般
   角色 40 級、主角 99 級）的玩家單位殺敵得到經驗，`process_xp` 對它 early-return、不清除，`pending_xp_credit`
   就殘留下來。回合循環從玩家回合到 team 1 友軍回合之間沒有任何重設（`btl_turn.c` Phase A→B→C），殘留值
   因此存活到友軍回合。

4. **敵人查成長表越界。** 升級每級用 `fd2_get_char_growth_entry(portrait_id)` 取成長資料，該函式即
   `&character_growth_table[portrait_id]`（stride 11，無邊界檢查）。成長表只有 68 筆，索引 0x00–0x43，剛好
   對應所有非敵方 portrait；敵人 portrait_id ≥ 0x44 一律**越界**，把表尾之後的相鄰記憶體當成長 min／max。
   `fd2_roll_stat_gain_and_show_message` 以 `range = growth[1] - growth[0]`、`gain = min + rng % range` 計算，
   用這些垃圾 byte 就會 roll 出極大的屬性成長；`fd2_recalculate_combat_stats` 再把它們傳導到衍生數值。
   `btl_init.c` 對成長表的存取本身就有 `if (record_char_id < 0x44)` 守門，正說明 ≥0x44 沒有合法成長資料。

合起來：一個滿級玩家在玩家回合殺敵留下未清除的 `pending_xp_credit` → 緊接的 team 1 友軍回合裡，友軍對某
敵人發動物理攻擊，結算函式把殘留經驗灌進那個**敵人** → 敵人未死、未滿 40 級，於是照常升級 → 用越界的
垃圾成長 roll，屬性全部暴增。

### 本質
`fd2_execute_ai_physical_attack` 把升級結算掛在「攻擊目標」上、且結算函式沒有陣營防護，兩者本身只在「玩家
反擊」情境下剛好正確；一旦換成 team 1 友軍攻擊，目標變敵人就出錯。真正讓它被觸發的是 `process_xp` 在
early-return 時漏清 `pending_xp_credit`，使玩家的經驗殘留並外洩到不該拿經驗的敵人身上。屬性暴增則是「敵人
portrait ≥ 0x44 沒有成長表資料卻仍被拿去查表」的越界後果。

### 觸發條件（罕見）
必須同時滿足：(a) 有 team 1 AI 友軍且該友軍對敵人發動物理攻擊（只在少數聯合作戰章節）；(b) 該次結算時
`pending_xp_credit` 為非 0，通常來自同一回合稍早一個已滿級玩家單位的殺敵殘留。殘留的經驗值先進入敵人的
`+0x3C`（EX carry）欄位累積，跨過 100 才真正升級，因此更加零星。

### 影響與迴避
- 敵人升級後屬性可能暴增到難以擊殺，屬於負面故障。
- 只出現在有 AI 友軍的章節；一般戰鬥不受影響。
- 純遊玩無穩定迴避法；避免讓已滿級單位在有友軍的回合搶尾刀可降低機率。

### 驗證對象
- src：`battle/btl_ai.c`（`fd2_execute_ai_physical_attack` 結尾 `fd2_process_xp_and_level_up_for_char(target_idx)`；
  `fd2_npc_turn_phase_team1` 派遣 team 1 友軍）、`battle/btl_turn.c`（`fd2_process_xp_and_level_up_for_char`
  兩條 early-return 未清 `pending_xp_credit`；回合循環 Phase A→C 無重設）、`battle/battle.c`
  （`fd2_execute_attack_damage_calculation` 的 XP 累進判斷 `attacker.team == TEAM_PLAYER && defender.portrait_id > 0x43`）、
  `table/table.c`（`fd2_get_char_growth_entry` 無邊界檢查）、`battle/btl_init.c`（`record_char_id < 0x44` 守門）、
  `include/consts.h`（`TEAM_ENEMY 0 / TEAM_NPC 1 / TEAM_PLAYER 2`）、`include/globals.h`
  （`data_fd2_battle_character_growth_table[68] @0x620A1`）。
- Ghidra：`fd2_execute_ai_physical_attack @0x1548E`、`fd2_process_xp_and_level_up_for_char @0x1E292`、
  `fd2_npc_turn_phase_team1 @0x1D80B`、`fd2_get_char_growth_entry @0x4E4D1`、成長表 `@0x620A1`（68 × 11）。
- 相關 KB：`program_info/battle.md`（升級與屬性成長、AI 回合）。
