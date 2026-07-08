# Pathfind：移動範圍 flood fill 與目的地尋路

戰鬥地圖上「這個單位能走到哪些格」與「從 A 走到 B 的最短路徑」由 `util/pathfnd.c`
一組 register-passing 遞迴常式負責。整組程式分成兩個對稱的子系統，共用同一批狀態全域
（`.object3` 0x60060 起）：

- **移動範圍 flood fill**：把每一格能到達的「剩餘移動預算」塗進 tile map 的 marker
  byte，供玩家行動選單畫移動範圍、AI 評分時列舉可達格。
- **目的地尋路**：帶方向紀錄的 DFS，找出到指定目的地的最短步數並回填方向序列，供 AI
  走位與玩家移動實際照著走。

兩者都吃同一份「per-job 移動成本表」當地形通行成本，移動步數預算則來自行動單位的
runtime_char +0x3B（MV 移動力）。

## 驗證對象

**src**

- `src/util/pathfnd.c` — 本檔主體（flood fill 與尋路的 orchestrator、遞迴體、鄰格 step、
  路徑紀錄，以及全部狀態全域的定義與註解）。
- `src/table/table.c` — `fd2_get_movement_cost_table_for_job`（per-job 成本表 accessor）。
- 呼叫端 movement-class 選定邏輯：`src/battle/btl_aisc.c`、`src/battle/btl_aitg.c`、
  `src/ui_menu/menu.c`、`src/input/input.c`。

**Ghidra 對象（名稱＠位址即時核對）**

| 函數 | 位址 | 角色 |
|---|---|---|
| `fd2_init_movement_range_floodfill` | 0x4E040 | flood fill orchestrator（6 個 __cdecl 引數） |
| `fd2_flood_fill_movement_range_recursive` | 0x4E0DC | 四鄰格遞迴體 |
| `fd2_flood_fill_neighbor_step` | 0x4E16E | flood fill 鄰格 step（葉節點） |
| `fd2_pathfind_to_destination` | 0x4E1A6 | 尋路 orchestrator（10 個 __cdecl 引數，回傳 u8） |
| `fd2_pathfind_recursive_with_direction` | 0x4E27C | 帶方向紀錄的遞迴體 |
| `fd2_pathfind_neighbor_step_with_tiebreak` | 0x4E330 | 尋路鄰格 step（含 tiebreak） |
| `fd2_pathfind_count_unique_directions` | 0x4E3CF | 方向轉折 tiebreak 權重 |
| `fd2_pathfind_record_destination_xy` | 0x4E3B3 | mode 2 目的地即時快照 |
| `fd2_pathfind_check_destination_save_path` | 0x4E401 | 抵達判定＋回填方向序列 |
| `fd2_get_movement_cost_table_for_job` | 0x4E555 | per-job 移動成本表 accessor |

狀態全域佔用 0x60060..0x6017A（見下方「狀態全域」）；per-job 成本表本體
`data_fd2_battle_movement_cost_table` @ 0x61646。

**相關資源**

戰鬥 tile map 與地形屬性 buffer 於開戰時由章節戰鬥資料（FDFIELD.DAT）建成，pathfind 只讀
取這兩個 runtime buffer，不直接碰檔案格式（見 resource_info/fdfield.md、program_info/field.md）。

## 移動步數預算來源（runtime_char +0x3B，MV）

行動單位一次移動能花的步數預算 = runtime_char +0x3B（= combat_aux_block[0x14]，MV 移動力）。
玩家角色的此欄來自 character_base、敵方單位來自 enemy_data、升職時再加 promotion 加成
（欄位定義與來源見 program_info/overview.md）。各 pathfind 呼叫端一律直接讀 pCaster[0x3B]
當 orchestrator 的步數預算引數：`fd2_init_movement_range_floodfill` 的 `rng`、
`fd2_pathfind_to_destination` 的 `ms`。orchestrator 把它塞進 origin 格的 marker 當「最高
剩餘成本」，遞迴每踩一格就扣掉該格的地形成本，扣到不足以再走即停止擴張，因此 +0x3B 直接
決定移動範圍半徑。

## Per-job 移動成本表選定

`fd2_get_movement_cost_table_for_job(job_id)` @ 0x4E555 是純算術 accessor：回傳
`data_fd2_battle_movement_cost_table + job_id * 0x14`（base 0x61646、stride 0x14）。每個
job class 對應一列 20 bytes，每 byte 是某個地形類別的通行成本；回傳的指標直接當 flood
fill／尋路的 secondary 成本表引數。

要查哪一列的 movement class **由呼叫端決定，不在 pathfnd.c 內**，慣例為：

- 預設 = 該單位的 `job_id`（runtime_char +0x20）。
- 受狀態免疫者（`fd2_check_char_status_immunity` 非 0，見 program_info/battle.md）強制改用
  class 0x13（飛行類成本列）。
- 特定特殊單位（id 0x1C）改用專屬 class（0x10 特殊單位列）。

覆寫的欄位與優先序依呼叫端略有差異：AI 走位 `fd2_ai_walk_to_target_tile` 查 char_id
（+0x08）判 0x1C 後採 class 1、且此判斷排在狀態免疫之後（0x1C 蓋過免疫）；玩家行動選單
`fd2_player_action_menu_loop` 查 portrait_id（+0x07）判 0x1C 後採 class 0x10、且以 else-if
排在免疫之後（免疫優先）；target-input 檢查 `fd2_wait_for_action_target_input` 查
portrait_id 採 class 1 但免疫最後判（免疫優先）。物理攻擊評分
`fd2_ai_score_physical_attack` 只有免疫→0x13、否則 job_id 兩路，沒有 0x1C 分支。

（此 accessor 另有非 pathfind 用途：`fd2_wait_for_action_target_input` 用它取回單一格的
通行成本判斷該格能否落腳。）

## 移動範圍 flood fill

進入點 `fd2_init_movement_range_floodfill(ct, x, y, rng, tm, af)` @ 0x4E040。

**呼叫端（5 個）**：`fd2_player_action_menu_loop`（玩家行動選單畫移動範圍）、AI 的
`fd2_ai_score_physical_attack`、`fd2_compute_aoe_targets`、`fd2_ai_walk_to_target_tile`、
`fd2_ai_score_offensive_spell`。

**引數與狀態灌入**（orchestrator 進場先把六個引數 spill 進狀態全域）：

- `ct` → 0x6006A：per-job secondary 成本表 base（即 `fd2_get_movement_cost_table_for_job`
  的回傳值）。
- `x` / `y` → 0x6006E / 0x6006F：origin 格座標（seed_x / seed_y，各取低 byte）。
- `rng` → 0x60070：移動步數預算（max_steps，取低 byte，來自 +0x3B）。
- `tm` → 0x60064：戰鬥 tile map base；map_width = tm[0]、map_height = tm[2]。
- `af` → 0x60060：primary「地形屬性→成本索引」查表 base。

**tile map 佈局**：header 4 bytes（width＠+0、height＠+2），其後每格 4 bytes。格內
byte 佈局為 `[0..1]` 屬性 word（16-bit）、`[1]` 高 byte 兼放方向 nibble、`[2]` flags、
`[3]` marker（剩餘成本）。origin 格 marker 位址 =
`tm + ((uint16)(map_width * seed_y) + seed_x) * 4 + 7`（乘積以 8×8→16-bit 形成，兩運算元
皆 ≤255 不溢位）。orchestrator 把 max_steps 寫進 origin marker 後，交 seed_x/seed_y/
max_steps/origin_ptr 給遞迴體 `fd2_flood_fill_movement_range_recursive` @ 0x4E0DC 展開。

**遞迴展開**：對四方向依二進位順序 right / left / down / up 逐一嘗試，鄰格指標以 ±4
（左右）或 ±(map_width×4)（上下）位移；每個方向的 origin `cost`／座標在方向間與遞迴呼叫
後都不變（每個方向從同一剩餘量出發）。邊界檢查：right/down 用無號 `(coord+1) < extent`、
left/up 只要 `coord != 0`。

**鄰格 step**（`fd2_flood_fill_neighbor_step` @ 0x4E16E，兩級查表算地形成本）：

1. `attr_word = *(u16*)(btm-3)`；`cost_idx = primary[((attr_word & 0x3FF) << 2) + 1]`
   （屬性 word 低 10 bits ×4 索引 primary 表、取該 entry 的 +1 byte 當 secondary 索引）。
2. `tile_cost = secondary[cost_idx]`（secondary = per-job 成本表 base）。
3. `new_cost = remaining_cost - tile_cost`。
4. 只有同時滿足「成本負擔得起（`tile_cost <= remaining_cost`）」、「嚴格改善既有 marker
   （**有號** 比較 `(int8)marker < (int8)new_cost`）」、「未被標為不可通行（`flags & 0x40 == 0`）」
   三條件才改寫 marker 並回傳非零（要求遞迴）。`flags & 0x80` 的「移動 sink」格會被標為可達但
   剩餘量強制歸 0，遞迴到此即停、不再往外擴張。

## 目的地尋路

進入點 `fd2_pathfind_to_destination(ct, sx, sy, ms, db, f1, f2, md, tm, af)` @ 0x4E1A6，
回傳最短步數（0xFF = 走不到、0 = origin 即目的地、否則 N 步）。

**呼叫端（3 個）**：`fd2_ai_seek_optimal_position`、`fd2_ai_walk_to_target_tile`、
`fd2_player_action_menu_loop`。

它是 flood fill 的「帶方向」孿生：除了 flood fill 那批狀態外，額外灌入目的地座標
`f1/f2` → 0x60071/0x60072（dst_x/dst_y）、路徑輸出 buffer `db` → 0x60073、鄰格 step 模式
`md` → 0x6017A、遞迴深度 0x60077 歸 0、best-path-length 0x60078 設哨兵 0xFF。origin marker
的種入與 tile map 佈局同 flood fill。orchestrator 先跑一次抵達檢查處理「origin 即目的地」
短路，再啟動方向 DFS `fd2_pathfind_recursive_with_direction` @ 0x4E27C。

**方向 DFS 與 step stack**：遞迴體每進一層就在 step stack（0x60079，每 frame 8 bytes
`{x, y, cost, dir}`）寫入本層 frame 並把深度 0x60077 +1、離開時 -1。四方向順序同 flood
fill（right/left/down/up），對應方向碼 `3=right / 1=left / 0=down / 2=up` 寫入 frame[+3]。
與 flood fill 不同的是這個 step stack 是「真的被消費的資料」：方向碼要被 tiebreak 權重與
路徑回填讀回，故此處保留真實的 stack frame，不能像 flood fill 那樣純靠原生遞迴取代。

**鄰格 step 與模式**（`fd2_pathfind_neighbor_step_with_tiebreak` @ 0x4E330）：地形成本兩級
查表同 flood fill；改善判定同樣是有號比較。模式旗標 0x6017A：

- **0 標準**：嚴格較優才 commit，平手不算贏。
- **1 標準＋tiebreak**：平手時再比「方向轉折權重」——
  `fd2_pathfind_count_unique_directions`（走 step stack 數方向轉折次數 ×4，轉折越多權重越高，
  偏好自然的折線路徑而非直線斜切捷徑）嚴格勝過 marker `[-2]` byte 已存的權重才 commit。
- **2 忽略障礙＋記錄目的地**：跳過通行性 gate，每次 commit 都呼叫
  `fd2_pathfind_record_destination_xy` 記下目的地。

任何 commit 都把方向碼（count×4）OR 進 marker 的 `[-2]` byte（保留其低 2 bits ＝屬性高
2 bits，成本查表仍需用到）；marker 本體 `[0]` 存的是剩餘成本 CL（sink 為 0），方向碼另存
`[-2]`，兩者分離。模式 0/1 下 `flags & 0x40` 不可通行格會擋掉 marker 寫入與遞迴訊號（但方向
byte 已先寫），`flags & 0x80` sink 格則標為可達但剩餘量 0。

**抵達與路徑回填**（`fd2_pathfind_check_destination_save_path` @ 0x4E401）：當前格 == 目的地
且抵達深度不劣於目前最佳（無號比較）時，把當前深度記為新最佳，並把 step stack 各層 frame[+3]
方向 byte 依序 copy 進輸出 buffer。深度為 0 的抵達也會把最佳降到 0。orchestrator 最後回傳
best_path_length（0x60078）。

## 狀態全域（.object3, 0x60060 起）

flood fill 與尋路共用，每個欄位都由兩個 orchestrator 在任何讀取者跑之前寫入，故載入時值為 0
（BSS）。

| 位址 | 全域 | 意義 |
|---|---|---|
| 0x60060 | `data_fd2_battle_pathfind_tile_cost_table_ptr` | primary「屬性→成本索引」查表 base（`af`） |
| 0x60064 | `data_fd2_battle_pathfind_battle_tile_map_ptr` | 戰鬥 tile map base（`tm`） |
| 0x60068 | `data_fd2_battle_pathfind_map_width` | 每列格數（tm[0]） |
| 0x60069 | `data_fd2_battle_pathfind_map_height` | 列數（tm[2]） |
| 0x6006A | `data_fd2_battle_pathfind_move_cost_table_ptr` | per-job secondary 成本表 base（`ct`） |
| 0x6006E | `data_fd2_battle_pathfind_floodfill_seed_x` | origin 格 X |
| 0x6006F | `data_fd2_battle_pathfind_floodfill_seed_y` | origin 格 Y |
| 0x60070 | `data_fd2_battle_pathfind_floodfill_max_steps` | 移動步數預算（來自 +0x3B） |
| 0x60071 | `data_fd2_battle_pathfind_dst_x` | 目的地 X（僅尋路寫入） |
| 0x60072 | `data_fd2_battle_pathfind_dst_y` | 目的地 Y（僅尋路寫入） |
| 0x60073 | `data_fd2_battle_pathfind_path_output_buffer_ptr` | 路徑輸出 buffer base（僅尋路寫入） |
| 0x60077 | `data_fd2_battle_pathfind_current_depth` | 目前 DFS 遞迴深度（兼候選步數） |
| 0x60078 | `data_fd2_battle_pathfind_best_path_length` | 目前最佳步數，初值哨兵 0xFF，兼回傳值 |
| 0x60079 | `data_fd2_battle_pathfind_step_stack` | 方向 DFS 的 step stack（每 frame 8 bytes `{x, y, cost, dir}`） |
| 0x6017A | `data_fd2_battle_pathfind_mode_flags` | 尋路鄰格 step 模式（0/1/2；僅尋路寫入） |

## 交叉引用

- runtime_char +0x3B（MV）與 +0x20（job_id）等欄位定義：program_info/overview.md。
- `fd2_check_char_status_immunity` 與 AI 評分／走位如何呼叫本子系統：program_info/battle.md。
- 法師「移動後不能施法」的機制（施法者須原地）：program_info/spell.md。
- tile map／地形屬性 buffer 的來源（章節戰鬥資料）：program_info/field.md、resource_info/fdfield.md。
- per-job 成本表在資料表模組級索引中的位置：program_info/table.md。
