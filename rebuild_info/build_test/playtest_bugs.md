# 實機 playtest 解過的 rebuild bug

把 `src/` 自給自足連結出來的 src-only FD2.EXE 放進完整遊戲環境（`fd2_game_files/`），
對照原版 `~FD2.EXE` 實機跑出來、再逐一定位修掉的問題。這些 bug 全部是「Layer-2 功能等價」
emit 在編譯層面看起來沒問題（build 0 error / 0 warning、測試綠）、卻在真實遊戲執行時悄悄
偏離原版行為的案例。本文件按**根因類別**整理，作為後續 emit / data-land 要避開的坑。

每個 bug 都已使用者實機確認修復。完整建置與定位流程見 `workflow.md`；牽涉的 emit pipeline
規則精煉寫在 `../emission/pipeline_spec.md`（E-8b array、Layer-2 熱迴圈時序、E-3b 硬編位址）。

## 分工鐵則（使用者定）

實機 playtest debug 的分工是固定的：

- **要「當場聽音效內容對不對」的驗證由使用者跑** —— DOSBox 由使用者啟動，AI 不能代聽。
- **「看數值 / 看畫面行為」的診斷由 AI 做** —— 反組譯比對、runtime 狀態 dump、邏輯推導。
- **build / link 產 FD2.EXE 由 AI 做**。

## 根因類別總覽

| 類別 | bug | 一句話根因 | commit |
|---|---|---|---|
| A. 跨 vendor 呼叫的暫存器 clobber | 音效全靜音（SFX 啞、BGM 正常） | AIL 宣告缺 clobber pragma，`-3s` 下 EBX 被 `AIL_init_sample` 破壞 | `e8dc10e` |
| B. Watcom BSS/COMDEF 相鄰與順序不保證 | 炙焰刀施法平移 crash | 三個 scalar 被 reader 當 `uint32[3]` 索引，linker 反序擺放 → 讀到鄰居 garbage 指標 | `329ca2a` |
| B. 同上 | 鍵盤失效 + sfx_flag 被清零 | 28-byte `union REGS` INT scratch 拆成獨立 scalar，相鄰 byte 被 linker 拆散 | `4ca8ad0` |
| C. 資料型別號性 | 開場 scene 顯示錯亂 | cursor 螢幕座標誤宣告 `uint32`，原版是有號 `int`，比較分支號性相反 | `a9b772e` |
| D. 熱迴圈 codegen 時序 | 商店進入腳步聲被對話音效打斷 | pose 縮放取整除法 emit 成 `>>7`（2 指令），原版有號 `/128`（6 指令）→ 過場變快、SFX 被切 | `a195696` |
| E. 硬編絕對位址 | "File not found" 開場退出 | `fd2_load_dat_resource` 把字串位址寫死成 immediate，linker 把字串擺別處 → fopen 空檔名 | `f44a0a1` |

其中**開場 hang（第一個 scene 後黑畫面）**不是獨立 bug，是 E（fname）與 B（union REGS）兩個修復
連帶解決的，不另列。

下面逐一展開。

---

## A. 跨 vendor 呼叫的暫存器 clobber —— 音效全靜音

**症狀**：所有 SFX 啞掉，BGM 正常。

**根因**：`src/include/protos.h` 的 AIL 函式宣告缺 clobber pragma。Watcom `-3s`（FD2 遊戲端採用的
stack-call ABI）預設把 EBX 當 callee-saved，編譯器於是把 sample offset 留在 EBX 跨過
`AIL_init_sample` 呼叫 —— 但 `AIL_init_sample` 內部實際會 clobber EBX（vendor optimizer 移除了它
用不到的 push/pop）。結果 `AIL_set_sample_address` 拿到的是被破壞的垃圾 bank 位址，全部 SFX 靜音。
BGM 之所以正常，是因為第一首 BGM 走的 `AIL_stop_sequence` 被條件跳過、offset 沒被破壞。原版把這個值
spill 到 stack 規避，rebuild 在 emit 時遺失了 clobber 資訊。

**修法**：`gen_ailv3_h.py` 產生的每個 public AIL 宣告都加上
`#pragma aux AIL_<fn> "*" modify [eax ebx ecx edx];`，把四個 caller-saved 暫存器全部列出。
`protos.h` 改成 `#include "ailv3.h"`、移除 16 個手寫的 plain AIL 宣告；AIL handle 全域型別由
`uint32` 改 `void *`；`AIL_set_sample_address` 首參由 `int` 改 `HSAMPLE`。

**關鍵認知**：**Watcom 把 `modify` list 當「精確集合」解讀** —— 只寫 `modify [ebx]` 反而會讓編譯器
誤以為 EAX/ECX/EDX 被保留，污染只是從 EBX 搬到沒列的那顆暫存器，沒真正修好（已用三個 pragma 變體
實測證實）。`-3r`（register-call）的 client 因為 EAX/EBX/ECX/EDX 本來就都是 arg-volatile 而倖免，
`-3s` 不然 —— 這也是為什麼 ail_extract 用 `-3r` 的 `test_audio.c` 一直能正常播、遊戲端卻啞。

**教訓**：vendor library function 的 clobber 行為是 ABI 契約的一部分。凡 client 跨 vendor 呼叫存活
某個值，宣告就必須完整列出 vendor 真正破壞的全部 caller-saved 暫存器，且在 `-3s` 下「精確集合」語意
要求一個都不能漏。

**完整細節**（pragma 機制、handle typedef、buffer-pointer 型別區分）見 `../ail/calling_convention.md`。
驗證：audio.obj disasm 確認 offset 改放 ESI（AIL 有保留）跨呼叫存活。

---

## B. Watcom BSS/COMDEF 相鄰與順序不保證

這一類有兩個獨立案例，共同根因：**Watcom 對 BSS/COMDEF tentative 定義的記憶體擺放順序與相鄰關係，
linker 不保證**（實測甚至是反序）。凡原版把多個相鄰全域當成一塊連續記憶體（array 索引、或 struct
punning）來存取，rebuild 若把它們 emit 成多個獨立 scalar，linker 就會把它們拆散、塞進別的全域，
讀取時拿到鄰居的 garbage。

### B1. 炙焰刀（熾炎刀＝鐵諾劍聖必殺技 spell 0x1D）施法平移 crash

**症狀**：施放炙焰刀、畫面開始往左平移顯示敵人受攻擊的瞬間，protected-mode fault crash
（0x1D 走此路、0x1C 跳過）。

**根因**：`fd2_animate_bg_zoom_transition_in` 用 `bg_layer[idx % 3]` 把三個 BG layer 指標當
`uint32[3]` 索引（位址 @0x5410B / 0x5410F / 0x54113）。`src/` 原本 emit 成三個獨立的 tentative
scalar，但 wlink map 證實 Watcom 對 BSS/COMDEF 是**反序** layout（layer_2 反而在最低位址），於是
reader 的 `[1]` / `[2]` 讀到鄰居 spotlight_bg / split_bg_b 的 garbage 指標，餵給
`fd2_rle_blit_sprite` 造成 wild read → fault。zoom phase1 第二格（idx 2）就引爆，正好是「畫面開始
往左平移」那一刻。

**修法**：三個 scalar 併成單一 `uint32[3]` array `data_fd2_battle_special_cinematic_bg_layers`
（C 標準保證 array element 升序相鄰），所有引用明確改成 array index（不用 macro）。涉及
`globals.h` / `anicine.c` / `anispell.c` / `spellcin.c` + 測試，Ghidra 端 0x5410B 改成 `dword[3]`。
驗證：wlink map 證實 array 升序排列（base+0/4/8、spotlight 緊接其後），實機確認炙焰刀（以及一併修好的
zoom_out / 轉職動畫）正常。

### B2. 鍵盤失效 + sfx_driver_flag 被清零

**症狀**：鍵盤輸入失效，且音效 driver flag 被莫名清零。

**根因**：原版 0x53A8D 是一塊 28-byte 的 `union REGS`，給 INT 呼叫共用的 scratch 區。`last_key` 與
`key_input_mode` 其實是這塊 union 內的相鄰 byte。rebuild 把它們拆成獨立的 `uint8` 全域，被 linker
拆散到不相鄰的位址，於是 INT 呼叫寫 scratch 時連帶覆蓋的相鄰 byte 對不上、互相踩踏。

**修法**：合回單一 `data_fd2_input_int16_regs` union（`globals.h` + `<i86.h>`），`last_key` /
`key_input_mode` 兩個符號變成指向 union 內 byte 的 macro。

**複查結論**：已全盤確認**無其他** union REGS / int scratch 被拆散的問題 —— 23 個 `int386` call
site 全部用這塊 0x53A8D scratch（input / video 路徑）或 dpmi 的 stack-local union；0x53A8D 的
28-byte REGS 範圍內沒有其他全域（`view_window_origin_x` @0x53AA9 在 REGS 之後、不受影響）；
`int386x` 遊戲沒用到；跨相鄰 punning 只有 `last_key`（union 已涵蓋）和 `scaler_loop_state`
（已是單一 `uint8[6]` array）兩處，皆安全。

### B 類教訓

**凡 reader 把多個 global 當 array 索引（`(&g0)[i]`）或對相鄰全域做 struct punning，就必須 emit
成單一的真 array / struct，不能拆成多個 scalar。** Watcom 的 BSS/COMDEF tentative scalar 既不保證
宣告順序、也不保證相鄰（實測反序）。Ghidra 端要同步成 `dword[N]` / 對應 struct 型別，並用 wlink map
（`build_fd2.py --map`）驗證實際 layout。此規則寫進 emit pipeline `../emission/pipeline_spec.md` 規則 E-8b。

---

## C. 資料型別號性 —— 開場 scene 顯示錯亂

**症狀**：NEW GAME → chapter 1 prologue 開場，捲動正確，但對話框跳出的瞬間畫面整個跳回地圖底部。

**根因**：`data_fd2_battle_cursor_screen_x` / `_y`（0x53AB9 / 0x53ABD）在 `src/` 誤宣告成
`uint32`，原版是**有號 `int`**。四個 `fd2_cursor_move_up/down/left/right` 拿它和視窗邊緣比較，原版
編成有號分支（JGE/JLE），rebuild 因為宣告無號而編成無號分支（JAE/JB）；兩者只在 cursor_screen 為負
時分歧。開場走到地圖頂端時 `origin_y` 到 0，`fd2_walk_step_up` 的 scroll 分支被 `origin_y != 0`
守衛擋掉、改走 inner 分支持續 `cursor_screen_y--`，把它壓成負值；接著對話框
`fd2_play_dialog_open_animation` → `fd2_pan_cursor_to_tile_animated` pan 到講話者時，
`fd2_cursor_move_down` 的無號 `< 6` 比較把負值座標當成極大值，誤取 `origin_y++` 的 scroll 分支，
把畫面捲到地圖底（症狀就是「對話框跳出瞬間畫面跳回底部」）。pan 本身仍朝講話者移動（cursor_move 照常
被呼叫），所以「移動方向 / 量看似正常」；fade 後 `fd2_init_battle_state_for_chapter` 把
cursor_screen_y 歸零，後續 scene 又正常。

**修法**：`globals.h` + `cursor.c` 把這兩個 global 型別由 `uint32` 改回 `int`（並改掉 cursor.c
兩段註解原本「never negative」的錯誤前提）。其餘用點全是 `++` / `--` / `×` / `+` 算術，int 與 uint32
位元表示相同、不受影響。WDISASM 確認四個比較從 JAE/JB 變回 JGE/JL（有號，與原版一致）。Ghidra 端
0x53AB9 / 0x53ABD 型別改 `int` 並加 data plate。

**教訓**：資料全域的**號性**是行為的一部分，不能只看「位元表示一不一樣」。只要該值會進入比較
（有號/無號分支選擇），號性錯了就在邊界值（負值、跨零）行為分歧。emit data 型別要從 Ghidra 的 signed
判定取，懷疑時用 WDISASM 比對條件跳轉助記碼（JGE/JL vs JAE/JB）。

**定位法記錄**：使用者精修症狀（捲動正確 → 對話框跳出瞬間跳底 → pan 相對量正確 → 走回看見 scene 2
緊貼 → fade 後正常）一路排除：walk 用 `fd2_composite_battle_tile_map` 直繪且正確 → 排除原本頭號嫌疑
（885B composite）；dialog 走 `fd2_composite_battle_frame` 另一條路；逐一反組譯比對 walk_step_up /
composite_battle_frame 皆等價、origin_y 在 walk 後正確 → 收斂到 cursor_move 的號性。

---

## D. 熱迴圈 codegen 時序 —— 商店進入腳步聲被對話音效打斷

**症狀**：進入村莊商店時，腳步聲還沒播完就被店主的問候對話音效切斷。純功能看一切正常 —— 這是時序問題。

**根因**：`fd2_blit_scaled_chapter_pose`（chapter-intro 的 pose zoom 過場最大頭，10/11 個 frame、
每 frame 64000 像素的純 CPU 縮放 loop）的 fixed-point 取整除法，`src/` 誤 emit 成 `(int) >> 7`
（算術右移，2 指令），原版是有號 `/128`（Watcom 的 signed div-by-2^n idiom，6 指令）。in-bounds guard
保證 `src >= 0`，所以兩者結果相同（功能等價）—— 但 `>>7` 每像素少約 4 條指令，pose 動畫快了約 0.13
秒，進場過場從約 0.93 秒縮到約 0.80 秒，低於「腳步聲」SFX 的固定時長（FDOTHER[0x1F] id1 loop3，
約 0.9 秒的 real-time DMA）。於是店主問候的 typewriter SFX（id2，與腳步聲同樣走
`fd2_play_sfx_with_handle` 的 sample handle_0）一啟動，它的 `AIL_stop_sample` 就把還沒播完的腳步聲
切掉。原版過場 >= 0.9 秒（這個脆弱的時序平衡剛好夠），讓腳步聲先播完。

**修法（對齊原版 codegen）**：`src/gfx/blittile.c` 的 `fd2_blit_scaled_chapter_pose` 兩處
`((int)src_x_fp >> 7)` / `((int)src_y_fp >> 7)` 改回 `(int)... / 128`，Watcom 即 emit 出 signed
div（與原版逐指令相同），恢復 pose 每 frame 的時長。同時修正該 function 原本誤稱
">>7 reproduces the signed-shift idiom exactly" 的 plate 註解。純恢復時序、功能不變。

**教訓**：**功能等價的 codegen 簡化（有號 `/128` → 算術 `>>7`）會改變指令數，而純 CPU 熱迴圈
（縮放 / blit）的執行時間隨指令數變動，會改動畫 / 過場時長、進而破壞遊戲隱性的時序平衡。** 時序敏感的
熱迴圈 emit 必須對齊原版 codegen，不能為了「等價且更短」而簡化。關鍵差異在於：SFX 時長是固定的
（AIL DMA 走 real-time、不隨 DOSBox cycles），過場繪圖時長卻隨 cycles 縮放，所以高 cycles 下這種
時序失衡尤其脆弱。

**反向矛盾的 debug 心法**：當「遊戲 code byte-identical + delay 實測正常、行為卻異常」這種反向矛盾
出現時（這個案例裡整條過場路徑 src + codegen 都 byte-identical、`fd2_delay_ms` 實測 `delay(1000)`
=21 ticks 甚至略長、AIL 兩個 sample handle 不撞、sample rate 0x2b11 一致），就要去 wdis 熱迴圈、
逐指令比指令數，而不是再找 delay 或邏輯 bug。

對應 emit pipeline 規則見 `../emission/pipeline_spec.md` Layer-2 的「時序敏感熱迴圈」例外段。

---

## E. 硬編絕對位址 —— "File not found" 開場退出

**症狀**：開場就跳 "File not found" 退出。

**根因**：`fd2_load_dat_resource` 把原版的字串（檔名）位址直接寫死成 immediate 常數。rebuild 的
linker 把字串擺到別的位址，於是這些寫死的 immediate 指到錯誤記憶體，`fopen` 拿到空檔名而失敗。

**修法**：9 處 hardcoded 字串位址全部改用 symbol 引用（讓 linker 自己填正確位址）。

**教訓**：原版以絕對位址 immediate 引用字串 / 資料時，rebuild **必須改成 symbol 引用** —— Layer-2
不追求 byte-exact、linker 會自由擺放資料，任何寫死的絕對位址在 rebuild 都會指錯。這條和 emit pipeline
的 string-pool 規則（`../emission/pipeline_spec.md` E-2 / E-3）同源。

---

## 通用教訓

這六個 bug 揭示了「Layer-2 功能等價」emit 會在五個面向悄悄偏離原版、且只在完整遊戲執行時才浮現的
盲點。編譯綠、單元測試綠都驗不出來，因為它們不是「這個 function 算錯」，而是「這個 function 在真實
環境的某個隱性契約上和原版不一致」：

1. **暫存器保存契約（A）** —— 跨 vendor 呼叫的 clobber list 要完整、且在 `-3s` 的精確集合語意下一個
   都不能漏。
2. **資料相鄰與順序（B）** —— Watcom BSS/COMDEF 不保證 tentative scalar 的順序與相鄰，reader 當 array
   / struct 用的就必須 emit 成真 array / struct。
3. **資料號性（C）** —— 進入比較的全域，號性錯了就在邊界值分歧。
4. **熱迴圈時序（D）** —— 純 CPU 熱迴圈的指令數決定執行時間，時序敏感處的 codegen 要對齊原版，不能
   做等價簡化。
5. **絕對位址引用（E）** —— 原版寫死的位址在 rebuild 一律改 symbol。

定位這類 bug 的主力手段是**host WDISASM 反組譯比對**（把 `tests/OUT/obj/*.obj` 用
`WATCOM_9.5a\BINNT\WDISASM.EXE` 直接在 Windows 反組譯，對照 Ghidra 的原版反組譯），免 DOSBox。
A（offset 留 EBX vs spill stack）與 C（號性 JAE/JB vs JGE/JLE）都是這樣抓到的。完整方法見
`workflow.md`。
