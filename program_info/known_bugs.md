# 原版遊戲已知 bug

炎龍騎士團合輯版「二代」(FD2) 原版執行檔 `FD2.LE` 中，經逆向工程確認的**遊戲程式／資料缺陷**。
本檔只收錄已完整分析、成因明確的原版 bug；尚未釐清的逆向問題見 `open_issues.md`，重建過程遇到的
build/runtime bug 見 `rebuild_info/build_test/`。

每筆列出：現象、成因、資料誤植的本質、影響與迴避，以及可逐條核對的驗證對象。

## 清單

| # | 名稱 | 系統 | 後果 |
|---|---|---|---|
| 1 | 凱拉斯幽靈轉職（聖戰士） | 轉職選單 (`promote.c`) | 選單出現無效轉職，確認後遊戲 crash |

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
