# 城鎮 / 章節交接選單樹 (town_menu)

章節與章節之間的非戰鬥服務畫面。玩家在此買賣道具、整備裝備、贈送物品、到教會復活陣亡
夥伴、轉職、查看角色狀態，並在戰鬥章節開打前決定出戰名單。整棵選單樹由章節交接流程
(`fd2_chapter_transition_menu`) 進入，依「該章是劇情章或戰鬥章」分成兩條路：劇情章顯示
intro 面板與服務選單樹、戰鬥章只顯示存檔提示後直接進編成畫面。

章節本身的生命週期 (init / end / post-action) 屬 `field.md`；本檔只涵蓋玩家實際操作的選單。

## 驗證對象

**對應 src**：`src/ui_menu/chintro.c`、`src/ui_menu/shop.c`、`src/ui_menu/promote.c`、
`src/ui_menu/status.c`（章節交接進入點 `fd2_chapter_transition_menu` 與其 intro 分派器
`fd2_chapter_transition_with_intro` 定義在 `src/field/chtrans.c`，屬 `field.md` 正典，本檔以進入點
角度引用）。

**主要 Ghidra 對象（位址即時核對）**

intro / 交接選單（chintro.c）
- `fd2_chapter_transition_menu` @ 0x2CAD7 — 章節交接分派 + intro/存檔選單（chtrans.c）
- `fd2_chapter_intro_menu_input_loop` @ 0x2D7BD — intro 四選單的左右鍵輸入迴圈
- `fd2_run_chapter_intro_menu_main` @ 0x2E341 — 商店型 intro 選單（買/賣/裝備/贈）
- `fd2_run_chapter_intro_menu_typeB` @ 0x2FC85 — 戰鬥章 intro 選單（狀態/存檔/讀檔/開戰）
- `fd2_run_chapter_intro_menu_typeC` @ 0x3072F — 城鎮服務 intro 選單（狀態/贈/復活/轉職）
- `fd2_party_roster_single_select_loop` @ 0x2E6B8 — 2 欄 3 列夥伴選擇格
- `fd2_party_roster_class_select_loop` @ 0x2E8CF — 單欄夥伴選擇格 + 裝備後屬性預覽

商店（shop.c）
- `fd2_open_shop_dialog_panel` @ 0x2E0BD — 開商品格面板（6 格滑入）
- `fd2_shop_menu_input_loop` @ 0x2DF6B — 商品格游標導航 + 分頁捲動
- `fd2_pick_stat_compare_color` @ 0x2EF8F — 屬性預覽的比較色
- `fd2_run_buy_item_menu` @ 0x2F0B0 — 買
- `fd2_run_sell_item_menu` @ 0x2F642 — 賣
- `fd2_run_equip_member_menu` @ 0x2F883 — 裝備
- `fd2_run_give_item_menu` @ 0x2F8EA — 贈

轉職 / 復活 / 編成（promote.c）
- `fd2_run_class_promotion_menu_main` @ 0x31385 — 轉職主迴圈
- `fd2_execute_class_promotion_with_dialog` @ 0x31602 — 轉職結算（屬性成長 + 移動力加成）
- `fd2_promote_member_select_loop` @ 0x311DC — 轉職候選格
- `fd2_build_promotion_candidates_with_targets` @ 0x31793 — 建轉職候選 + 目標職業表
- `fd2_run_revive_menu_main` @ 0x30DC3 — 教會復活主迴圈
- `fd2_revive_member_select_loop` @ 0x30C22 — 復活候選格
- `fd2_build_dead_chars_list_for_revive` @ 0x309FF — 建陣亡名單
- `fd2_run_recruitment_or_branch_screen` @ 0x318AD — 出戰編成畫面（10x3 格）

狀態畫面（status.c；面板繪製 `fd2_render_full_char_stat_panel` 定義在 `src/gfx/rndstat.c`）
- `fd2_run_status_screen_member_menu` @ 0x2FFA5 — 逐一挑夥伴看狀態的迴圈
- `fd2_open_char_status_screen` @ 0x17AED — 開單一角色狀態畫面（含法術頁）
- `fd2_open_status_screen_with_slide_in` @ 0x17E0B — 狀態面板 12 幀滑入
- `fd2_render_full_char_stat_panel` @ 0x17FC0 — 狀態面板數值/血條/圖示繪製

存取器與資料表（欄位細節見 `table.md` 與 `assets/`）
- `fd2_get_chapter_intro_metadata_entry` @ 0x4E4B9
- `fd2_get_class_promotion_data_entry` @ 0x4E48D
- `data_fd2_chapter_per_chapter_category_table` @ 0x526B9（byte[30]，劇情/戰鬥章旗標）
- `data_fd2_chapter_intro_panel_resource_idx_per_metadata_category_table` @ 0x526D7（byte[3] = 0x0B, 0x3D, 0x3E）
- `data_fd2_chapter_intro_portrait_pose_x_column_table` @ 0x52635（byte[18] = [3][6]）
- `data_fd2_chapter_intro_portrait_pose_y_row_table` @ 0x52647（byte[18] = [3][6]）
- `data_fd2_chapter_intro_menu_speaker_portrait_id_table` @ 0x52659（byte[6] = 0x81, 0x80, 0x00, 0x82, 0x83, 0x84）

**相關資源檔**：FDOTHER.DAT（intro 面板背景 RLE、選單 overlay、存檔用 sprite、UI 音效 bank）、
DATO.DAT（80x80 角色 portrait）、FDICON.B24（portrait 快取來源）、FDTXT（`all_game_text` 對話頁）。

## 章節交接進入點與劇情/戰鬥章分派

主迴圈在 `game_event_flag == 2` 時透過每章 dispatch 表叫到 `fd2_chapter_transition_menu`。它先
釋放上一章的 runtime 狀態（runtime_char array、tile event 表、tile map、portrait 快取），從
FDICON.B24 依當前出戰名單重建 portrait 快取，然後依
`data_fd2_chapter_per_chapter_category_table[current_chapter_id]` 分成兩條路：

- **值為 0（劇情章）**：載入 intro 面板背景、淡入、進 intro 服務選單樹。
- **值非 0（戰鬥章）**：清畫面、以 portrait 0x4B 顯示存檔提示對話（FDTXT 0x19A），玩家選「是」
  就存到 slot 0，接著進 `fd2_run_recruitment_or_branch_screen` 出戰編成，不顯示服務選單。

這張 30-byte 表是**劇情/戰鬥章的分派依據**，與下述的 intro 外觀變體碼 `bCategory` 是兩件事、
用不同表；不要把 `bCategory` 當成劇情/戰鬥旗標。

## intro 外觀變體碼 bCategory

劇情章的 intro 面板外觀由 chapter-intro metadata 的第 0 個 byte 決定。
`fd2_chapter_transition_menu` 以 `fd2_get_chapter_intro_metadata_entry(current_chapter_id)` 取得
metadata entry，`bCategory = metadata[0]`，值域 0/1/2。它是純外觀變體碼，有兩個用途：

1. **選 intro 面板背景資源**：以 `bCategory` 直接索引
   `data_fd2_chapter_intro_panel_resource_idx_per_metadata_category_table` = {0x0B, 0x3D, 0x3E}，
   取得 FDOTHER.DAT 的 intro 背景 RLE 資源編號（0->0x0B、1->0x3D、2->0x3E）。
2. **決定過場 portrait 目標座標**：以 `bCategory*6 + cursor_state` 索引 [3][6] 的
   `data_fd2_chapter_intro_portrait_pose_x_column_table` / `_y_row_table`，得到主角 portrait 在
   intro overlay 上的位置，也是三個 intro 選單退出時 zoom-out 動畫的目標座標。

metadata 表的 `bCategory` 欄位語意與 26-entry 逐章值見 `assets/tables/chapter_intro_metadata.md`。

`fd2_chapter_transition_menu` 內的 intro 選單是一個 radio 游標
`data_fd2_chapter_intro_menu_cursor_state`（0..4，特殊 hotkey 提交時為 5；左右鍵循環、Enter/Space
提交），提交時呼叫 `fd2_chapter_transition_with_intro`（chtrans.c，見 `field.md`）依 cursor_state
分派到下述三個 intro 服務選單變體之一：state 3/5 走 main、state 0 走 typeB、state 4 走 typeC、
state 2 是存檔（不開服務選單）。

## 三個 intro 服務選單變體

三者 orchestrator 形狀相同：載入固定或依 state 決定的 FDOTHER 背景、淡入、畫講者 portrait、顯示
問候對話，然後跑共用的四方向輸入迴圈 `fd2_chapter_intro_menu_input_loop`（左右鍵移動 0..3 游標、
Enter/Space 提交回 1、Esc 取消回 -1），提交時依游標分派到子選單，取消則播 zoom-out 過場並返回。
講者 portrait 由 `data_fd2_chapter_intro_menu_speaker_portrait_id_table` 提供（main 用
`[cursor_state]`、typeB 用 `[0]`、typeC 用 `[4]`）。

| 變體 | Ghidra | 背景資源 | 四方向分派（游標 0/1/2/3） |
|---|---|---|---|
| main | `fd2_run_chapter_intro_menu_main` @ 0x2E341 | FDOTHER 0x1D/0x3F/0x0C 依 state | 買 / 賣 / 裝備 / 贈 |
| typeB | `fd2_run_chapter_intro_menu_typeB` @ 0x2FC85 | FDOTHER 0x0D（固定）| 狀態 / 存檔 slot 1 / 讀檔 / 開戰 |
| typeC | `fd2_run_chapter_intro_menu_typeC` @ 0x3072F | FDOTHER 0x0E（固定）| 狀態 / 贈 / 復活 / 轉職 |

- main 是有商店的劇情章服務選單，四個分支對應 `fd2_run_buy_item_menu` / `fd2_run_sell_item_menu`
  / `fd2_run_equip_member_menu` / `fd2_run_give_item_menu`。開場先把隊伍金錢面板（sprite 取自
  atlas 的 +0x0A 記錄）blit 到 0xA76C5 並以 `fd2_render_decimal_number_to_buffer` 印出金額。
- typeB 是戰鬥章 intro（cursor_state == 0）用的無商店選單。游標 3「開戰」會顯示確認對話
  （FDTXT 0x19F），玩家選「是」（確認游標 0）就顯示「戰鬥開始」（0x1A0）並回 1，代表開始戰鬥。
- typeC 是中段城鎮據點（cursor_state == 4）的純服務選單，四分支對應狀態畫面、贈物、教會復活
  (`fd2_run_revive_menu_main`)、轉職 (`fd2_run_class_promotion_menu_main`)，沒有開戰分支，恆回 0。

三者退出時都跑 11 幀（iVar5 10..0）的 zoom-out：以 `bCategory*6 + cursor_state` 查 pose 表得到
主角 portrait 目標座標，逐幀縮放 `fd2_blit_scaled_chapter_pose` 並降亮度淡出。

## 夥伴選擇格

服務選單的子流程都先讓玩家選一名夥伴或一件道具，用兩種選擇格：

- `fd2_party_roster_single_select_loop` @ 0x2E6B8：2 欄 3 列、一次顯示 6 名的夥伴格，游標在
  0..count-1 移動、以 2 為步進分頁捲動；買（消耗品收件人）、賣（賣方）、贈（來源/目標）、狀態
  都用它。
- `fd2_party_roster_class_select_loop` @ 0x2E8CF：單欄、一次顯示 3 名的夥伴格，只上下移動、以 1
  為步進捲動，每列預覽「裝上這件裝備後的屬性」；只有買裝備分支用它。

兩者都分配三個 64000-byte（320x200 mode 13h）工作緩衝
(`data_fd2_ui_slide_anim_accumulator_buf_ptr` / `_bg_snapshot_buf_ptr` / `_composed_target_buf_ptr`)，
快照 VGA、滑入面板，清理由呼叫端的 `fd2_close_intro_dialog_with_slide_out` 負責。

## 商店：買 / 賣 / 裝備 / 贈

商品格由 `fd2_open_shop_dialog_panel` 開（6 格滑入）、`fd2_shop_menu_input_loop` 導航（2 欄格、
一次 6 格、以 2 分頁）。每筆道具的類別與價格取自 `fd2_get_item_effect_entry(item_id)`：類別
byte 在 entry +0、16-bit 價格在 entry +0x13（欄位表見 `assets/items.md`）。

- **買** (`fd2_run_buy_item_menu`)：商品的類別 byte < 0x20 視為裝備（要先掃出能裝的角色、走
  屬性預覽選擇格、購入後可選自動裝備），>= 0x20 視為消耗品（全隊皆可、走一般選擇格、不裝備）。
  流程為「篩可裝對象 -> 買給誰確認 -> 金錢是否足夠 -> 選收件人 -> 背包是否已滿 -> 加入道具 ->
  可選自動裝備 -> 扣款與掉錢動畫」。各商店階（0..5，即 cursor_state）的對話頁取自五張 short[6]
  文字 id 表。
- **賣** (`fd2_run_sell_item_menu`)：先選賣方、建該角色的道具清單、跑賣出模式的商品格；賣價為
  基準價的 75%（`price * 3 >> 2`）。
- **裝備** (`fd2_run_equip_member_menu`)：選夥伴後進 `fd2_equip_unequip_inventory_menu`（見下述
  背包模型），返回後重載該章講者 portrait。
- **贈** (`fd2_run_give_item_menu`)：兩次夥伴選擇（來源、目標）之間夾一次道具選擇，把道具從來源
  slot 移到目標背包（以未裝備狀態加入），只重算來源的戰鬥屬性。main 的游標 3 與 typeC 的游標 1
  都叫它。

裝備屬性預覽（`fd2_compute_equipped_stats_with_item_preview` @ 0x2EFB7）算出「換上候選道具後」的
AP/DP/命中/迴避：以候選道具類別 byte <= 0x14 為武器、> 0x14 為防具，只替換同類別的已裝道具、保留
另一類別的加成。`fd2_pick_stat_compare_color` 依現值與預覽值比較給出數字顏色。

## 轉職

`fd2_run_class_promotion_menu_main`（typeC 游標 3）先用
`fd2_build_promotion_candidates_with_targets` 建候選：只收「等級 >= 20 且 portrait_id 為基本職
(< 0x12) 且非蘭斯洛特 (portrait_id != 7)」的夥伴，並算出各人的目標職業（預設 portrait_id+0x20；
持有對應轉職道具時改 portrait_id+0x32；悠妮 portrait_id==9 持精靈契印 0x5A 時改召喚師 0x34）。
玩家經 `fd2_promote_member_select_loop` 選人、確認後，扣除對應轉職道具（tier-1 升級不耗物）、
播轉職過場，寫回新 job_id / portrait_id、重建 portrait 快取，最後呼叫
`fd2_execute_class_promotion_with_dialog` 結算。轉職候選的等級/職業細節見 `assets/classes.md`。

`fd2_execute_class_promotion_with_dialog` @ 0x31602 的結算順序：

1. 顯示「成為[職業]」對話（FDTXT 0x253）。
2. 依新職業的成長表逐一 roll 五項屬性成長（AP/DP/DX/HP/MP），對話頁 0x1EA..0x1EE 逐列往下堆；
   成長公式與 exclusive 上界語意見 `battle.md`（`fd2_roll_stat_gain_and_show_message`）。
3. 取 `fd2_get_class_promotion_data_entry(portrait_id)`，**若 promotion 表 byte[1] 非 0（此職業轉職
   時給移動力加成）**：顯示「移動力增加[N]點」對話（FDTXT **page 0x254**），等待按鍵，再把該值
   加進 runtime_char +0x3B（`combat_aux_block[0x14]`，MV 移動力欄）。此 byte[1] 是**移動力加成、
   不是所學法術**，也與 +0x1A 的法術 bitmap 無關。
4. 重算戰鬥屬性、滑出對話，把新職業重設成 1 級狀態：level = 1、EX 經驗餘額 (+0x3C) 歸零、HP/MP
   補滿。

promotion 表本身（`data_fd2_get_class_promotion_data_entry` 讀的表）欄位細節屬 `table.md` 與
`assets/`；此處只記其消費行為：byte[0] = 新 job_id、byte[1] = 升職移動力加成。

## 教會復活

`fd2_run_revive_menu_main`（typeC 游標 2）以 `fd2_build_dead_chars_list_for_revive` 建陣亡名單
（runtime_char bFlags bit0 = 死亡），選一名後，費用 = 該角色等級 ×「per-job 費率表」對應項，玩家
付得起且選「是」就扣款、清 bFlags、HP 補滿，播復活音效（BGM 0x11 -> 0x0B）後繼續。相關對話頁：
無人陣亡 0x24C、復活誰 0x24D、付款確認 0x24E、金錢不足 0x1F8。

## 出戰編成畫面

戰鬥章開打前的 10x3 選格由 `fd2_run_recruitment_or_branch_screen` @ 0x318AD 呈現：玩家勾選出戰
名單（`current_chapter_id > 0x1A` 時上限 0x13 人，否則 0x0F 人），勾滿即
`fd2_reorder_party_by_selection` 重排隊伍。此畫面無法用 Esc 略過（兩個呼叫端都以 `TEST EAX / JZ`
在回 0 時重跑）。勾滿後有 per-chapter 的必帶角色 gate 與 pin：確認名單含必帶角色，再
`fd2_pin_required_char_to_party_slot1` 把必帶角色釘進 roster slot 1，最後顯示「準備好開戰了嗎」
對話（FDTXT 0x292），選「是」回 1。逐章 pin 表與 slot-1 身分（如 ch22 希爾法、ch26 亞奇梅吉/
悠妮、ch27/28 悠妮）見 `field.md` 與各 `assets/chapters/`。

## 角色狀態畫面

`fd2_run_status_screen_member_menu`（三個 intro 選單的「狀態」分支）逐一讓玩家挑夥伴、開該角色的
狀態畫面、再重載被覆蓋的 DATO portrait，直到 Esc。單一角色狀態畫面
`fd2_open_char_status_screen` @ 0x17AED 以 `fd2_open_status_screen_with_slide_in` 滑入面板、等按鍵，
若該角色有可用法術再滑入唯讀法術頁（法術清單 `fd2_build_usable_spell_list` 見 `spell.md`），最後
滑出還原畫面。

面板數值由 `fd2_render_full_char_stat_panel` @ 0x17FC0 繪製（定義在 `src/gfx/rndstat.c`，屬
`gfx.md`）。面板上三格白色 2 位數依序是 **LV / EX / MV**：

- LV：`status_flags_block[0]`（runtime_char **+0x21**，等級），繪到 +0x29DD。
- EX：runtime_char **+0x3C**（升級經驗餘額 XP carry），繪到 +0x379D。
- MV：`combat_aux_block[0x14]`（runtime_char **+0x3B**，移動力），繪到 +0x455D。

其餘為 HP/MP 血條與數字、AP/DP/DX 基準值/現值/迴避（依 boost 旗標紅字），以及隊伍/狀態圖示與
角色名/職業/原型文字標籤。runtime_char 欄位佈局的正典在 `overview.md`。

## 背包模型與共用道具原語

背包是 8 個 slot，每 slot 2 byte：flag byte（bit0x80 = 空、bit0x40 = 已裝備、0 = 佔用未裝備）與
item_id byte。status.c 收錄一組被城鎮選單與戰鬥道具指令共用的原語：

- `fd2_count_usable_inventory_slots` @ 0x1B8A6 — 數非空 slot。
- `fd2_add_item_to_inventory` @ 0x1BB8C — 加到第一個空 slot（滿則回 -1）。
- `fd2_remove_inventory_slot_at` @ 0x1B8E7 — 移除並把後續 slot 往前補齊、slot 7 標空（背包保持
  無縫，故壓縮清單索引恆等於原 slot 索引）。
- `fd2_equip_item_in_slot` @ 0x1C142 — 標記已裝備，先自動卸下同類別的已裝道具（item_id < 0x80 為
  物理、>= 0x80 為魔法，同類別最多一件，實現「一武器 + 一法術書」規則）。
- `fd2_check_job_can_equip_item` @ 0x1C1C3 — 職業能否裝備（掃 `fd2_get_job_allowed_items_table_entry`
  的前 6 項允許類別）。
- `fd2_equip_unequip_inventory_menu` @ 0x1BFFE — 裝備/卸下互動模態（城鎮「裝備」與戰鬥道具指令
  的 SORT/EQUIP 都用它）。
- `fd2_inventory_selection_modal_dispatch` @ 0x1B932 / `fd2_inventory_grid_input_step` @ 0x1B9DE —
  背包格選取模態與逐幀輸入。
- `fd2_find_inventory_slot_with_item` @ 0x31860 — 找持有指定道具的 slot（轉職金鑰道具判定用）。

戰鬥回合中的道具指令 4 選單 `fd2_item_command_menu_dispatch` @ 0x1BBDC（使用/贈與/整理裝備/丟棄）
也定義在此 TU 並共用上述原語，但其戰鬥情境（AoE 目標選取、施法書 teleport 分支）屬 `battle.md`。
另全螢幕「軍隊狀態」總覽 `fd2_open_party_status_overview_screen` @ 0x1B1E7 由戰場欄選單叫用，
細節見 `ui_menu.md`。

## 本檔擁有的 global 狀態

- `data_fd2_ui_menu_cursor_idx` @ 0x53C57、`data_fd2_ui_menu_scroll_offset` @ 0x5412F、
  `data_fd2_ui_menu_visible_item_count` @ 0x5413F、`data_fd2_ui_menu_candidate_array_ptr` @ 0x54143：
  各模態選單共用的游標 / 捲動 / 可見列數 / 候選陣列指標（皆 BSS，開單時歸零）。
- `data_fd2_ui_menu_saved_cursor_idx` @ 0x5414B、`data_fd2_ui_menu_saved_scroll_offset` @ 0x5414F：
  買道具面板重開之間持存游標/捲動的存檔副本。
- `data_fd2_ui_menu_screen_sprite_atlas_buf_ptr` @ 0x54147：從 FDOTHER.DAT 載入的選單 sprite atlas
  緩衝（用畢 free、寫回 0）。
- `data_fd2_ui_slide_anim_accumulator_buf_ptr` @ 0x53C5B、`data_fd2_ui_slide_bg_snapshot_buf_ptr`
  @ 0x53C5F、`data_fd2_ui_slide_composed_target_buf_ptr` @ 0x53C63：滑入/滑出過場的三個 64000-byte
  工作緩衝（開單時 malloc、關單時 free）。
- `data_fd2_dialog_last_action_text_id_param` @ 0x53AD9：對話 VM 的暫存文字頁參數，商店/贈/轉職/
  復活各路徑在叫對話前寫入（如 item_id+0xB5、portrait_id+1、char_id+1），語意見 `dialog.md`。
