# 實機 playtest 解過的 rebuild bug

把 `src/` 自給自足連結出來的 src-only FD2.EXE 放進完整遊戲環境（`fd2_game_files/`），對照原版
`~FD2.EXE` 實機跑、再逐一定位修掉的問題。這些 bug 的共同特徵是「Layer-2 功能等價」emit 在編譯層面
看起來乾淨（build 0 error / 0 warning、單元測試綠），卻在真實遊戲執行時悄悄偏離原版行為 —— 它們
不是「某個 function 算錯」，而是「某個 function 在真實環境的某個隱性契約上和原版不一致」。本檔按根因
類別 A–H 整理，作為後續 emit / data-land 要避開的坑；每類的機制細節指向對應正典，這裡只留症狀、
一句根因、教訓與排障經驗。

每個 bug 都已使用者實機確認修復。完整建置與定位流程見
`workflow.md`。

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
| F. math intrinsic 呼叫形式 | 白光柱特效在 86Box-macOS runaway page fault | math.h intrinsic 使 sqrt() emit 成 `CALL IF@DSQRT`（原版從未執行的路徑），86Box dynarec 誤執行回 0.0 → remap count=0 下溢 | `15d32073` |
| G. stack-probe 分佈 | 音效初始化時 "Stack Overflow!" 終止 | crt/dpmi 支援單元帶了探測，AIL ISR 在私有堆疊（低於 _STACKLOW）呼叫 get_eflags thunk → 探測誤判溢位 | `395221d7` |
| H. 折疊基底歸錯符號 | 教會復活：清單金額正確，確認對白與實際扣款金額卻不同 | 價格讀取抄成 `inventory_full_table[job_id + 5]`（該表只有 6 元素），那其實是原版折疊基底 `0x52669` 被歸給前一個符號；linker 把兩符號隔開後讀到鄰居 dialog-id 表 | `0c3ffe12` |

**開場 hang（第一個 scene 後黑畫面）不另列** —— 它不是獨立 bug，是 E（fname）與 B（union REGS）
兩個修復連帶解決的。

---

## A. 跨 vendor 呼叫的暫存器 clobber -- 音效全靜音

**症狀**：所有 SFX 啞掉，BGM 正常。

**根因（一句）**：`src/include/protos.h` 的 AIL 宣告缺 clobber pragma，Watcom `-3s` 把 EBX 當
callee-saved、於是把 sample offset 留在 EBX 跨過 `AIL_init_sample` 呼叫；但該 vendor function 內部
實際會 clobber EBX，`AIL_set_sample_address` 拿到被破壞的垃圾 bank 位址，全部 SFX 靜音。

**修法**（commit `e8dc10e`）：`gen_ailv3_h.py` 產生的每個 public AIL 宣告加上
`#pragma aux AIL_<fn> "*" modify [eax ebx ecx edx];`，`protos.h` 改 `#include "ailv3.h"`。

**教訓**：vendor library function 的 clobber 行為是 ABI 契約的一部分。凡 client 跨 vendor 呼叫存活
某個值，宣告就必須完整列出 vendor 真正破壞的全部 caller-saved 暫存器；在 `-3s` 的「精確集合」語意下
一個都不能漏（只列部分反而把污染搬到沒列的暫存器）。與 G 類同源 —— 都是「被 vendor lib 以特殊
context 呼叫」的隱性契約。

**正典**：pragma 機制、handle typedef、buffer-pointer 型別區分、`-3s`/`-3r` client 差異的完整說明
見 `../ail/calling_convention.md`。

---

## B. Watcom BSS/COMDEF tentative scalar 不保證順序與相鄰

兩個獨立案例，共同根因：Watcom 對 BSS/COMDEF tentative 定義的擺放順序與相鄰關係不保證（實測反序）。
凡原版把多個相鄰全域當一塊連續記憶體存取（array 索引、或 struct punning），rebuild 若把它們 emit
成多個獨立 scalar，linker 就把它們拆散、reader 讀到鄰居的 garbage。

**B1 炙焰刀（劍聖必殺技 spell 0x1D）施法平移 crash**：`fd2_animate_bg_zoom_transition_in` 用
`bg_layer[idx % 3]` 把三個 BG layer 指標當 `uint32[3]`（0x5410B / 0x5410F / 0x54113）索引；emit
成三個獨立 tentative scalar 後 linker 反序擺放（layer_2 反而在最低位址），reader 的 `[1]` / `[2]`
讀到鄰居 spotlight_bg / split_bg_b 的 garbage 指標 → 餵 `fd2_rle_blit_sprite` wild read →
protected-mode fault（0x1D 走此路、0x1C 跳過）。修法（commit `329ca2a`）：三 scalar 併成單一
`uint32[3]` array `data_fd2_battle_special_cinematic_bg_layers`，所有引用改成 array index。

**B2 鍵盤失效 + sfx_driver_flag 被清零**：原版 0x53A8D 是一塊 28-byte `union REGS`，給 INT 呼叫
共用的 scratch；`last_key` 與 `key_input_mode` 是這塊 union 內的相鄰 byte。rebuild 把它們拆成獨立
`uint8` 全域被 linker 拆散，INT 呼叫寫 scratch 時連帶覆蓋的相鄰 byte 對不上、互相踩踏。修法（commit
`4ca8ad0`）：合回單一 `data_fd2_input_int16_regs` union，`last_key` / `key_input_mode` 變成指向
union 內 byte 的 macro。

**覆蓋結論**（排障經驗，正典不收）：已全盤確認無其他 union REGS / int scratch 被拆散 —— 23 個
`int386` call site 全用這塊 0x53A8D scratch（input / video 路徑）或 dpmi 的 stack-local union；
0x53A8D 的 28-byte 範圍內沒有其他全域（`view_window_origin_x` 在 REGS 之後、不受影響）；`int386x`
遊戲沒用到；跨相鄰 punning 只有 `last_key`（union 已涵蓋）與 `scaler_loop_state`（已是單一
`uint8[6]` array）兩處，皆安全。

**教訓 / 正典**：凡 reader 把多個 global 當 array 索引（`(&g0)[i]`）或對相鄰全域做 struct punning，
就必須 emit 成單一的真 array / struct，Ghidra 端同步成 `dword[N]` / 對應 struct 型別，並用
`build_fd2.py --map` 驗證實際 layout。完整規則見 `../equivalence/rules.md`「相鄰性不變式」段。

---

## C. 資料型別號性 -- 開場 scene 顯示錯亂

（此類無對應正典，機制與定位法完整保留於此。）

**症狀**：NEW GAME → chapter 1 prologue 開場，捲動正確，但對話框跳出的瞬間畫面整個跳回地圖底部。

**根因**：`data_fd2_battle_cursor_screen_x` / `_y`（0x53AB9 / 0x53ABD）在 `src/` 誤宣告成 `uint32`，
原版是**有號 `int`**。四個 `fd2_cursor_move_up/down/left/right` 拿它和視窗邊緣比較，原版編成有號分支
（JGE/JLE），rebuild 因宣告無號而編成無號分支（JAE/JB）；兩者只在 cursor_screen 為負時分歧。開場走
到地圖頂端時 `origin_y` 到 0，scroll 分支被守衛擋掉、改走 inner 分支持續 `cursor_screen_y--` 把它壓
成負值；接著對話框 pan 到講話者時，`fd2_cursor_move_down` 的無號 `< 6` 比較把負值座標當成極大值、
誤取 `origin_y++` 的 scroll 分支，把畫面捲到地圖底（症狀就是「對話框跳出瞬間畫面跳回底部」）。fade 後
`fd2_init_battle_state_for_chapter` 把 cursor_screen_y 歸零，後續 scene 又正常。

**修法**（commit `a9b772e`）：`globals.h` + `cursor.c` 把這兩個 global 型別由 `uint32` 改回 `int`。
其餘用點全是 `++` / `--` / `×` / `+` 算術，int 與 uint32 位元表示相同、不受影響。WDISASM 確認四個
比較從 JAE/JB 變回 JGE/JLE（與原版一致），Ghidra 端 0x53AB9 / 0x53ABD 型別改 `int`。

**教訓**：資料全域的**號性**是行為的一部分，不能只看「位元表示一不一樣」。只要該值會進入比較（有號/
無號分支選擇），號性錯了就在邊界值（負值、跨零）行為分歧。emit data 型別要從 Ghidra 的 signed 判定
取，懷疑時用 WDISASM 比對條件跳轉助記碼（JGE/JLE vs JAE/JB）。

**定位法**（排障經驗）：使用者精修症狀（捲動正確 → 對話框跳出瞬間跳底 → pan 相對量正確 → fade 後
正常）一路排除 —— walk 用 `fd2_composite_battle_tile_map` 直繪且正確 → 排除頭號嫌疑（composite）；
dialog 走另一條 `fd2_composite_battle_frame`；逐一反組譯比對 walk / composite 皆等價、origin_y 在
walk 後正確 → 收斂到 cursor_move 的號性。

---

## D. 熱迴圈 codegen 時序 -- 商店進入腳步聲被對話音效打斷

**症狀**：進入村莊商店時，腳步聲還沒播完就被店主問候的對話音效切斷。純功能看一切正常 —— 這是時序問題。

**根因（一句）**：`fd2_blit_scaled_chapter_pose`（chapter-intro pose zoom 過場的純 CPU 縮放 loop）的
取整除法，`src/` 誤 emit 成 `>>7`（2 指令）而非原版有號 `/128`（6 指令）；in-bounds guard 保證結果
相同（功能等價），但每像素少約 4 指令 → 進場過場變快、短於固定時長的腳步聲 SFX → 店主問候的
typewriter SFX 一啟動，它的 `AIL_stop_sample` 就把還沒播完的腳步聲切掉。

**修法**（commit `a195696`）：兩處 `((int)... >> 7)` 改回 `(int)... / 128`，Watcom 即 emit 出與原版
逐指令相同的 signed div，恢復 pose 每 frame 的時長。

**教訓 / 正典**：時序敏感的純 CPU 熱迴圈不能為了「等價且更短」而簡化 codegen，要對齊原版指令數。機制
與規則見 `../equivalence/rules.md` Layer-2 的「時序敏感熱迴圈」例外段。

**反向矛盾 debug 心法**（排障經驗，正典不收）：當「遊戲 code byte-identical + delay 實測正常、行為卻
異常」這種反向矛盾出現時（此案整條過場路徑 src + codegen 皆 byte-identical、`fd2_delay_ms` 實測正常、
兩個 AIL sample handle 不撞、sample rate 一致），就要去 wdis 熱迴圈逐指令比指令數，而不是再找 delay
或邏輯 bug。關鍵在於：SFX 時長是固定的（AIL DMA 走 real-time、不隨 DOSBox cycles），過場繪圖時長卻
隨 cycles 縮放，所以高 cycles 下這種時序失衡尤其脆弱。

---

## E. 硬編絕對位址 -- "File not found" 開場退出

**症狀**：開場就跳 "File not found" 退出。

**根因（一句）**：`fd2_load_dat_resource` 把原版字串（檔名）位址直接寫死成 immediate 常數，rebuild
linker 把字串擺到別處 → 這些寫死的 immediate 指到錯誤記憶體，`fopen` 拿到空檔名而失敗。

**修法**（commit `f44a0a1`）：9 處 hardcoded 字串位址全部改用 symbol 引用（讓 linker 自己填正確位址）。

**教訓 / 正典**：原版以絕對位址 immediate 引用字串 / 資料時，rebuild 必須改成 symbol 引用 —— Layer-2
不追求 byte-exact、linker 會自由擺放資料，任何寫死的絕對位址都會指錯。完整規則見
`../equivalence/rules.md` 規則 E-3b（與 string-pool 規則 E-2 / E-3 同源）。

---

## F. math intrinsic 呼叫形式 -- 白光柱特效在 86Box-macOS runaway page fault

**症狀**：只在 86Box-macOS（Apple Silicon）dynarec 上，白光柱（filled-circle band）法術特效（治療 /
傳送 / 第 30 章召喚共用）觸發 runaway page fault：`fd2_apply_palette_remap_run` 以 count=0 進入、
LOOP 下溢跑到 ESI/EDI 撞未映射頁。DOSBox-X 與 86Box interpreter（關 recompiler）皆正常，原版在同一
環境免疫。remap 迴圈體以 `#pragma aux` 對齊 byte-for-byte 後 crash 依舊 —— 腐蝕點不在迴圈，在上游。

**根因（一句）**：math.h 預設的 intrinsic pragma 令 wcc386 把 `sqrt()` emit 成 `CALL __@DSQRT`
（`__@DSQRT @ 0x3c738`；引數留在 ST(0) 跨 call 邊界，含原版從未執行的 FTST/FSTSW/SAHF 負數檢查
路徑）；86Box-macOS dynarec 誤執行該路徑 → sqrt 回傳 matherr DOMAIN 預設值 0.0 → half_width = 0
→ remap count = 0 下溢。原版遊戲碼一律呼叫 named CRT `sqrt @ 0x3c6fc` 真函數（call form：整數
sign-bit 檢查 + `FSQRT`）；其 intrinsic 變體 `IF@SQRT @ 0x3c736` / `__@DSQRT @ 0x3c738` 在原
binary 零 xref、只隨 sqrt387/trig387 module 連帶進入、從不執行。sin/cos 的對應 intrinsic stub
真名是 `IF@SIN @ 0x3c7cf` / `IF@COS @ 0x3c7b6`（非 `IF@DSIN` / `IF@DCOS`）；但與 sqrt 不同，
named `sin @ 0x3c898` / `cos @ 0x3c885` 內部本就 `CALL IF@SIN` / `IF@COS`，故該 stub 在原版有
xref、會執行（未觀察到 86Box crash），`__NO_MATH_OPS` 仍對 sin/cos 一併套用作預防。

**修法**（commit `15d32073`）：在 `#include <math.h>` 前定義 `__NO_MATH_OPS`（src/gfx/rndscene.c、
src/spell/spellcin.c、src/anim/anisummn.c），sqrt/sin/cos 全部回到真 CRT 呼叫、與原版逐指令同形。

**教訓**：「同版編譯器＋同旗標」不保證呼叫形式對齊 —— header 的 intrinsic pragma 也是 codegen 的一部分。
凡 helper 呼叫形式（intrinsic vs 真函數）偏離原版，就會把原版從未執行過的 vendor lib 程式碼帶進
runtime，任何 emulator 對那段碼的缺陷都只咬重建版。

**診斷心法**（排障經驗）：對「只在單一 emulator 崩潰」的 case，歸因不能停在與崩潰點的形式相關性（迴圈
形式差異），要沿資料流上溯到腐蝕值的產生點（count ← half_width ← sqrt 回傳值）。

**驗證**：全 obj 零 IF@ 引用、map 內 IF@* 全數 unreferenced（同原版）、golden 5/5 PASS。
**86Box 實機驗證：已由使用者實機確認修復。**

**正典**：`__NO_MATH_OPS` 旗標與機制見 `../link/wlink_settings.md` 的「math intrinsic 必須停用」段；
Layer-2 例外亦見 `../equivalence/rules.md`。

---

## G. stack-probe 分佈 -- 音效初始化時 "Stack Overflow!" 終止

**症狀**：帶音效初始化啟動時，程式立即印 "Stack Overflow!" 終止。

**根因（一句）**：wcc386 預設給每個函數插 `PUSH n / CALL __CHK` 堆疊探測，拿 ESP 與主堆疊底線
（`_STACKLOW`）比較；AIL 的 timer/audio-mix ISR 進中斷後切到 AIL 私有 DGROUP 堆疊（位址低於
`_STACKLOW`）再呼叫 `crt_equivalent_get_eflags_thunk`，thunk 若帶探測，ESP 必低於主堆疊底線 → 誤判
溢位 → 終止（CLIB3S(stk) 的 SS 逃生門在 flat model 下永不生效）。

**修法**（commit `395221d7`）：不用全域 `-s`（那會拿掉原版本有的溢位防護 —— 4K 堆疊底下緊鄰 DGROUP
全域，未偵測溢位會無聲改寫遊戲狀態），改在 `src/crt/crt.c`、`src/util/dpmi.c` 以
`#pragma off (check_stack)` 對齊原版分佈（遊戲碼全帶 `__CHK`、crt/dpmi 支援單元全不帶）。診斷工具
`tools/stkdiag/`。

**教訓 / 正典**：stack-probe 的「哪些單元有、哪些沒有」是原版的隱性契約、load-bearing、兩邊都不能動
（全關失去防護，全開被中斷的私有堆疊誤殺）。與 A 類同源（都是被 vendor lib 以特殊 context 呼叫的隱性
契約）。詳細機制與 `-s` 決策見 `../link/wlink_settings.md` 的 `-s` 段。

---

## H. 折疊基底歸錯符號 -- 教會復活扣款金額與清單不符

**症狀**：教會復活選單裡，候選人清單上每個人顯示的所需金額是對的；選定某人之後，教會人員
確認對白講出的金額卻是另一個數字，按下確認實際扣掉的也是這個錯的金額。

**根因（一句）**：`fd2_run_revive_menu_main` 的價格讀取被抄成
`data_fd2_dialog_shop_inventory_full_dialog_text_id_table[job_id + 5]`，但該表只有 6 個元素 ——
這個索引其實是原版折疊後的基底 `0x52669`（＝真正的 cost table `0x5266B` 減 2）被 Ghidra 歸給
前一個符號的結果；linker 把兩個符號隔開 0x2E4 bytes 之後就讀到鄰居的 dialog-id 表。

原版兩處讀的是**同一個位址**，所以報價與扣款不可能不一致：

| 位置 | 指令 | 索引暫存器 |
|---|---|---|
| 清單 `fd2_render_promote_members_grid` @ `0x30B5A` | `MOVSX EAX, word ptr [EAX*2 + 0x5266B]` | `job_id - 1`（另有 DEC） |
| 確認 `fd2_run_revive_menu_main` @ `0x30EE1` | `MOVSX ESI, word ptr [EAX*2 + 0x52669]` | `job_id`（-1 已折進基底） |

兩式都等於 `0x52669 + 2 * job_id`。重建版讀到的鄰居值使金額完全走樣：`job_id = 1` 讀到
`0x0001` → 只收「等級 × 1」；`job_id = 2` 讀到 `0x01F6` → 收「等級 × 502」（正解是 × 150）；
`job_id` 再大就一路讀進浮點常數的位元組。

**修法**（commit `0c3ffe12`）：改成與清單端同表同索引的
`data_fd2_ui_per_job_revive_or_promote_cost_table[job_id - 1]`。改動只影響 EXE 4 個 byte、
兩處（該 load 的 disp32 與其 fixup record 的 target）。

**教訓 / 正典**：**eqcheck 驗不出這一類修正** —— 改動落在 fixup site 與其 record 上，正好是
RELOC 層比對前會塗白的兩處，換掉 fixup 指向的符號會被判成 PASS[RELOC]。驗證要靠 WDISASM 反
組譯 `.obj` 直接讀出 fixup 的符號名。與 E 類（硬編絕對位址）同源，都是「原版的位址在 rebuild
不再成立」；差別在 E 是位址寫死成 immediate，H 是位址雖已 symbol 化、但綁到了錯的 symbol。
判準與掃描器見 `../equivalence/rules.md` 的「跨符號讀取不變式」與 `../../tools/oob_index_audit/`。

---

## 通用教訓

這八類 bug 的共同點：編譯綠、單元測試綠都驗不出來，因為它們不是「function 算錯」，而是「function 在
真實環境的某個隱性契約上和原版不一致」—— 暫存器保存契約（A）、資料相鄰與順序（B）、資料號性（C）、
熱迴圈時序（D）、絕對位址引用（E）、helper 呼叫形式（F）、stack-probe 分佈（G）、位址所屬符號（H）。
定位這類 bug 的主力手段是 host WDISASM 反組譯比對（把 `tests/OUT/obj/*.obj` 直接在 Windows 反組譯、
對照 Ghidra 的原版反組譯），免 DOSBox；A（offset 留 EBX vs spill stack）、C（號性 JAE/JB vs
JGE/JLE）、D（除法指令數）、H（fixup 的符號名）都是這樣抓到的。完整方法見 `workflow.md`。

B / E / H 三類還有一個共同的排障啟發：**症狀是「數值或指標讀到不相干的東西」時，先問這個讀取在
原版是不是靠 image layout 才成立的** —— 相鄰 scalar 被當 array（B）、位址寫死成 immediate（E）、
位址綁到錯的 symbol（H），rebuild 的 linker 一重新擺放就全部失效。
