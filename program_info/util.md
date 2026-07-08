# Util：雜項工具、DPMI 記憶體鎖定、no-op stub

`src/util/` 底下三個小檔收容不屬於任何大系統的支援常式：`misc.c` 是一組彼此無關的雜項工具
（除錯殘留、AIL 配置器 slot 換手、隊伍名冊掃描與重排、延遲包裝），`dpmi.c` 是遊戲端 INT 31h
記憶體管理 wrapper（配置 DOS 常規記憶體並把 AIL 音訊緩衝的分頁鎖住），`noop.c` 只有一個
連結產生的空 RET stub。這些函數多半被其他系統當工具呼叫，本檔只記錄它們自身的行為，牽涉到的
resource / runtime 結構細節一律指向對應正典檔。

## 驗證對象

**src**

- `src/util/misc.c` — 雜項工具（除錯 print、AIL fnptr slot、名冊掃描/重排、延遲 wrapper）。
- `src/util/dpmi.c` — DPMI INT 31h 記憶體 wrapper（配置 / 鎖定 / 解鎖）。
- `src/util/noop.c` — 單一 RET stub。
- 消費端：`src/crt/crt.c`（透過 AIL alloc slot 配置）、`src/audio/audio.c` 與 `src/rsrc/rsrc.c`
  （呼叫 `fd2_dpmi_lock_size` 鎖 AIL 緩衝）、`src/ui_menu/promote.c`（编成/招募畫面用名冊 helper）、
  各章節 handler（劇情道具/角色 predicate）。

**Ghidra 對象（名稱＠位址即時核對）**

misc.c：

| 函數 | 位址 | 角色 |
|---|---|---|
| `fd2_debug_print_ans_and_length` | 0x16F0B | 開發期除錯 print，dead code（0 xref） |
| `fd2_ail_set_alloc_fnptr` | 0x3615E | 換手 AIL 配置器 fnptr slot，回傳舊值 |
| `fd2_ail_set_free_fnptr` | 0x3616E | 換手 AIL 釋放 fnptr slot，回傳舊值 |
| `fd2_any_char_has_item` | 0x24B14 | 掃 16 個 runtime char 是否有人持某道具 |
| `fd2_find_template_char_by_id` | 0x24BDE | 掃 menu 名冊有無某 char_id（battle/spell 位址段的孿生副本） |
| `fd2_check_party_has_char_id` | 0x33499 | 掃 menu 名冊有無某 char_id（menu/chapter 位址段的孿生副本） |
| `fd2_require_char_id_in_active_party` | 0x31DBE | 檢查現役隊伍缺角則跳錯誤對話 |
| `fd2_count_selected_chars` | 0x320CE | 數招募畫面已選人數 |
| `fd2_reorder_party_by_selection` | 0x320FC | 依選取旗標把名冊已選者集中到前段 |
| `fd2_pin_required_char_to_party_slot1` | 0x321C8 | 把必帶角色釘進名冊 slot 1 並重載 portrait 快取 |
| `fd2_delay_ms` | 0x375B2 | 毫秒延遲（body 為 JMP CRT `__delay` @ 0x3DCCD） |
| `fd2_delay_400ms` | 0x353CC | 固定 400ms 延遲 |

dpmi.c：

| 函數 | 位址 | DPMI 功能 |
|---|---|---|
| `fd2_dpmi_alloc_dos_memory` | 0x361CC | fn 0x100：配置 DOS 常規記憶體區塊 |
| `fd2_dpmi_free_dos_memory` | 0x36255 | fn 0x101：依 selector 釋放 |
| `fd2_dpmi_lock_region` | 0x36284 | fn 0x600：鎖定線性位址區間 |
| `fd2_dpmi_unlock_region` | 0x362F1 | fn 0x601：解鎖線性位址區間 |
| `fd2_dpmi_lock_size` | 0x36316 | wrapper：鎖 `[base, base+size]` |
| `fd2_dpmi_unlock_size` | 0x3632D | wrapper：解鎖 `[base, base+size]` |

noop.c：

| 函數 | 位址 | 角色 |
|---|---|---|
| `fd2_noop_ret_pad` | 0x4E915 | 1-byte RET stub，連結 padding（0 xref） |

**相關資源**

- `FDICON.B24`：`fd2_pin_required_char_to_party_slot1` 釘完角色後會重開此檔重載 portrait 快取
  （格式見 resource_info/fdicon.md）。
- `FDMUS.DAT` 音樂資料緩衝與各 `.DAT` 資源緩衝：由 `dpmi.c` 的 lock wrapper 鎖定分頁（載入路徑
  見 program_info/audio.md、program_info/rsrc.md）。

## misc.c

### 開發期除錯殘留

`fd2_debug_print_ans_and_length(value)` 把整數格式化成字串，`printf` 印出數值與字串長度後
`getch()` 等待按鍵。全遊戲無任何呼叫端（dead code），是開發期留下的除錯輸出。

### AIL 配置器 slot 換手

AIL 音訊子系統的內部載入/配置常式不直接呼叫 CRT `malloc`/`free`，而是透過兩個可換手的
函數指標 slot 間接呼叫：`data_ail_alloc_fnptr` @ 0x52758（配置）與 `data_ail_free_fnptr` @ 0x5275C
（釋放，預設值為 CRT `free` @ 0x37416）。`fd2_ail_set_alloc_fnptr(new_fnptr)` @ 0x3615E 與
`fd2_ail_set_free_fnptr(new_free_fnptr)` @ 0x3616E 是這兩個 slot 的 get-and-set helper：各自把
新指標寫入 slot、回傳舊指標（供呼叫端事後還原或比對），dword（32-bit 指標）值、無堆疊框、
`__cdecl` 單一堆疊引數。兩者在靜態 xref 中都無呼叫端（屬 API 進入點）；真正消費 slot 的是
AIL 內部載入路徑——`src/crt/crt.c` 在服務一次 AIL 載入時，透過 `data_ail_alloc_fnptr` 指標
配置緩衝。AIL 供應商（Miles）內部細節見 rebuild_info/ail/。

### 隊伍名冊掃描與重排

以下 helper 共用同一份「menu/template 隊伍名冊」：base 指標 `data_fd2_shared_menu_party_roster_buffer_ptr`
@ 0x53BF7、成員數 `data_fd2_shared_menu_party_member_count` @ 0x53BFB、每 entry 0x50 bytes，
entry 內 char_id 在 +0x08、portrait_id 在 +0x07（名冊 entry 與 runtime_char 佈局共用，欄位定義
見 program_info/overview.md）。名冊 slot 0 固定是主角（羅特/索爾），helper 一律不動 slot 0。

- **`fd2_any_char_has_item(item_id)`** @ 0x24B14：對 runtime char 0..15 逐一呼叫
  `fd2_find_inventory_slot_with_item(char_idx, item_id)`，任一角色持有即回 1，全部落空回 -1。
  用於劇情關鍵道具的持有判定（如天空之鑰 / item 100），呼叫端為 `fd2_chapter_23_end`、
  `fd2_chapter_27_end`、`fd2_chapter_27_init`。
- **`fd2_find_template_char_by_id` @ 0x24BDE 與 `fd2_check_party_has_char_id` @ 0x33499**：
  兩者是 byte-identical 孿生副本（Watcom 把同一 body 展成兩個 translation unit，各落在不同
  位址段），行為相同——線性掃名冊，找到 char_id 相符的 entry 回 1、掃完落空回 0；引數是 char_id
  （角色加入時傳給 `fd2_init_runtime_char_from_base_growth` 的初始值），不是 job/class。前者
  供 `fd2_chapter_23_end` 判蜜蒂（char_id 0x12）在隊；後者供第 15/17 章 init/end 對話分支
  （凱麗 0xC、蜜蒂 0x12）、`fd2_chapter_17_post_action` 判負、招募畫面必帶角色 gate 與
  `fd2_render_party_status_overview_content`。兩者皆無副作用。
- **`fd2_require_char_id_in_active_party(max_chars, req_char_id)`** @ 0x31DBE：檢查的是**現役
  戰鬥隊伍**（`data_fd2_battle_runtime_char_array_ptr` 的 slot 1..max_chars，slot 0 主角不檢查）
  而非 template 名冊。缺角時載入章節 portrait 0x4B、把動態 per-char portrait slot 設為
  `(req_char_id & 0xFF) + 1`、渲染「你需要[角色名]」對話場景、拉起 tile-map 輸入 guard、等待
  帶閃爍的輸入對話後放下 guard、滑出對話，回傳 0；在隊則無對話回傳 1。呼叫端為招募/分歧畫面
  `fd2_run_recruitment_or_branch_screen`（必帶角色 gate）。與 `fd2_check_party_has_char_id`
  的差異即在此：後者查 template 名冊且無副作用，本函數查現役戰鬥範圍且會跳錯誤對話。
- **`fd2_count_selected_chars(sel_state)`** @ 0x320CE：數 per-member 選取旗標陣列 `sel_state`
  在 `[0, member_count-1)` 範圍內的非零 byte 數（略過最後的哨兵 slot），回傳已選人數。供招募
  畫面顯示「已選 N / 剩餘 max-N」計數，以及選滿 max_chars 時自動 commit 的判斷。
- **`fd2_reorder_party_by_selection(sel_state)`** @ 0x320FC：依 `sel_state` 把名冊重排——
  已選成員全部搬到前段（slot 1..K）、未選成員落到後段（slot K+1..），slot 0 不動。作法是先
  把原名冊 snapshot 進 0xA00-byte 暫存（0xA00/0x50 = 32 entries），再對 `[0, member_count-1)`
  跑兩趟（第一趟搬已選、第二趟搬未選），共用同一輸出 slot 游標故已選塊在前、未選塊在後；來源
  索引取 `iter+1` 略過 snapshot 的 slot 0。暫存於返回前 `free`。供招募畫面 Enter commit 完整選取時
  呼叫。
- **`fd2_pin_required_char_to_party_slot1(char_id)`** @ 0x321C8：把 char_id 相符的角色釘進名冊
  slot 1、其餘非主角成員順移到 slot 2 起，slot 0 不動。先掃現役 roster
  （`data_fd2_battle_runtime_char_array_ptr` slot 1..member_count）找 char_id 相符的
  match_idx（迴圈不早退、以最後一個 match 為準；實務上 char_id 唯一故只有 0 或 1 個 match），
  snapshot template 名冊進 0xA00-byte 暫存，把 match entry 複製進 slot 1，再把其餘 entry
  （略過 match_idx）依序填 slot 2..，暫存 `free`。之後重載 portrait 快取以對齊新順序：
  free 舊快取、重開 `FDICON.B24`、快取計數歸 0、對每個名冊 slot 呼叫
  `fd2_load_portrait_to_cache(roster[slot].portrait_id, fp)`、`fclose`。呼叫端為
  `fd2_run_recruitment_or_branch_screen`（確認必帶角色後）。此「per-chapter pin 必帶角色」機制
  決定了各章開戰時 battle `chars[i]` 的身分（例：第 22/23 章 slot 1 = 希爾法、第 27/28 章 = 悠妮、
  第 26 章先釘悠妮再釘亞奇梅吉使 slot 1 = 亞奇梅吉、slot 2 = 悠妮）；per-chapter 對照見
  program_info/field.md 與各章 chapters/。

### 延遲包裝

`fd2_delay_ms(ms)` @ 0x375B2 是全遊戲通用的毫秒延遲：binary body 只有一條 tail-call
`JMP` 到 Watcom CRT `__delay` @ 0x3DCCD。城鎮選單、對話/戰鬥/AI/法術動畫、存讀檔、回合流程
到處都直接呼叫它做固定停頓（如 `fd2_delay_ms(0x50)` 約 80ms）。`fd2_delay_400ms()` @ 0x353CC
是固定 400ms 版：`fd2_delay_ms(400)`，供
`fd2_cinematic_chapter_portrait_dump_with_white_flash` 呼叫（同一段 body 也是
`fd2_chapter_event_handler_36__ch24_cinematic` 的 fall-through tail）。

## dpmi.c：DPMI（INT 31h）記憶體管理

這六個 wrapper 是**遊戲端**對 DPMI host 的 INT 31h 呼叫（經 CRT `int386`），用途集中在 AIL
音訊驅動設定：把載入的音樂/資源緩衝分頁鎖住，確保 mixer ISR 在中斷情境不會缺頁。

- **`fd2_dpmi_alloc_dos_memory(paragraphs, out_linear, out_segment, out_selector)`** @ 0x361CC：
  DPMI fn 0x100 配置一塊 DOS 常規記憶體。成功（CF=0）時把回傳的 real-mode segment、linear 位址、
  selector handle 拆出寫回三個輸出指標，並呼叫 `fd2_dpmi_lock_region` 鎖住配得的區間；回傳
  是否成功。
- **`fd2_dpmi_free_dos_memory(linear_unused, segment_unused, selector)`** @ 0x36255：DPMI fn 0x101
  依 selector 釋放。前兩個引數是為對稱 alloc 的 3-output API 保留的佔位，實際只讀第 3 引數
  （disasm 讀 [ESP+0x44]）。
- **`fd2_dpmi_lock_region(start, end)` @ 0x36284 / `fd2_dpmi_unlock_region(start, end)` @ 0x362F1**：
  DPMI fn 0x600 / 0x601 鎖定/解鎖線性位址區間。兩個引數是區間起訖的**線性 byte 位址**（會自動
  取小/大值，順序無所謂），鎖定大小 = (max-min)+1 bytes；打包成 DPMI 暫存器 BX:CX = 基底線性位址、
  SI:DI = size。回傳 CF 清除為 1（成功）、否則 0。
- **`fd2_dpmi_lock_size(base, size)` @ 0x36316 / `fd2_dpmi_unlock_size(base, size)` @ 0x3632D**：
  便捷 wrapper，轉呼叫 `..._region(base, base+size)`，callee 的成功/失敗結果直接回傳。遊戲端主要
  透過 `fd2_dpmi_lock_size` 使用：`src/audio/audio.c` 在載入 BGM sequence 後鎖住其緩衝再交
  `AIL_init_sequence`，`src/rsrc/rsrc.c` 同樣鎖資源緩衝供 AIL 消費。

這六個 function 在原版 binary 全部 probe-free（無 `__CHK` stack-probe prologue），src 端以
`#pragma off (check_stack)` 對齊；因為它們服務 AIL 驅動設定路徑，須在非正常堆疊情境下也安全
（stack-check 分佈的正典見 rebuild_info/link/wlink_settings.md）。

DOS extender（DOS/4G）本身提供的 DPMI host、protected-mode 中斷反射、以及 LE heap 的 INT 31h
動態配置屬 loader/CRT 層，不在遊戲碼內；見 rebuild_info/link/le_layout.md。這六個遊戲端 wrapper
在重建時的 pool 分類與逐一對照見 rebuild_info/crt/symbol_inventory.md 與
rebuild_info/ail/calling_convention.md。

## noop.c

`fd2_noop_ret_pad()` @ 0x4E915 是一個 1-byte RET stub、無堆疊框、0 xref。它夾在
`fd2_dialog_sprite_blit_mirrored` @ 0x4E8E1 與 `fd2_decode_dialog_pixel_byte` @ 0x4E916 之間，
是連結時遺留的 padding/佔位 slot，從未被賦予真正的定義，也沒有任何呼叫端。

## 交叉引用

- runtime_char / 名冊 entry 佈局（char_id +0x08、portrait_id +0x07、0x50 stride）：program_info/overview.md。
- 招募/分歧畫面 `fd2_run_recruitment_or_branch_screen` 與编成流程：program_info/town_menu.md。
- per-chapter pin 決定各章 battle `chars[i]` 身分的完整對照：program_info/field.md、chapters/。
- AIL 音訊緩衝鎖定的載入時序與 BGM dispatcher：program_info/audio.md；資源緩衝載入：program_info/rsrc.md。
- AIL 供應商（Miles）內部與 DPMI thunk：rebuild_info/ail/；DOS/4G extender 與 LE heap DPMI：rebuild_info/link/le_layout.md。
- DPMI wrapper 的重建 pool 分類與 stack-check 政策：rebuild_info/crt/symbol_inventory.md、rebuild_info/link/wlink_settings.md。
