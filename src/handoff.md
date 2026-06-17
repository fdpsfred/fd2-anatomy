# FD2 Rebuild — Handoff

目標：`src/` 自身 compile+link 出可正確執行的 `fd2.exe`；`src/`+`tests/` compile 出測試執行檔。
**全 650 個遊戲 function 已 emit+review+commit。Phase 1+2 全部工作已從 `integ` 用 `--no-ff` merge 進 `main`（merge commit `89268a4`，main tree == integ）；當前 branch ＝ `main`，`integ` 已整合（下方 §0-§7 與本段以下對 `integ` / `data-pN` / `emit-pN` 的引用皆為歷史記錄）。當前在「真實資料落地 + 測試重寫 + 收斂 fd2.exe」收斂計畫（data-first）。**
**Phase 1（真資料落地，commit `cd2c0a8`）+ Phase 2（補完 22 個 blit/pathfind/composite coordinated landing）皆已完成。** routing 650/650 emit+reviewed、await_emit 0、build gate 0 err/0 warn、Ghidra Bad Instruction 0、FD2.LE 已存。**Phase 3（測試重寫到實際全綠）進行中 — 見下方「Phase 3 進度」。**

---

## ⚑ 實機 playtest debug（最新，最先讀）

把 src-only 連出的 FD2.EXE 放進 `fd2_game_files/`（完整遊戲環境）對照原版 `~FD2.EXE` 跑出來的問題。
**DOSBox 由使用者跑。** 鐵則（使用者定）：要「當場聽音效內容對不對」的驗證由使用者跑；「看數值 / 看
畫面行為的診斷」由 AI 做；build/link 產 FD2.EXE 由 AI 做。原本三個問題（音效全靜音、開場 hang、開場
顯示錯亂）**全部找到根因、修復、commit、使用者實機確認**。後續實機再發現「炙焰刀（鐵諾劍聖必殺技）
施法平移 crash」，已找到根因並修復、build/relink 完成，**待使用者實機確認後 commit**（見下方第一條）。

### 已修復並 commit

- **炙焰刀（熾炎刀＝鐵諾劍聖必殺技 spell 0x1D）施法平移 crash → 已修復（待實機確認 + 待 commit）**
  - **根因**：`fd2_animate_bg_zoom_transition_in`（往左平移顯示敵人受攻擊；0x1D 走此路、0x1C 跳過）用
    `bg_layer[idx%3]` 把三個 BG layer 指標當 `uint32[3]` 索引（@0x5410B/0F/13）。src/ 原 emit 成三個獨立
    tentative scalar，但 wlink map 證實 Watcom 對 BSS/COMDEF 是**反序** layout（layer_2 在最低位址），故
    reader 的 `[1]`/`[2]` 讀到鄰居 spotlight_bg / split_bg_b 的 garbage 指標 → 餵 `fd2_rle_blit_sprite`
    wild read → protected-mode fault；zoom phase1 第二格（idx 2）即引爆，正是「畫面開始往左平移」瞬間。
  - **修法（root-cause）**：三個 scalar 併成單一 `uint32[3]` array
    `data_fd2_battle_special_cinematic_bg_layers`（C 保證升序相鄰），所有引用明確改 array index（甲案、
    無 macro）。涉 globals.h / anicine.c / anispell.c / spellcin.c / tests + Ghidra 0x5410B 改 `dword[3]`
    + emit_issues 三條 RESOLVED。emit pipeline 教訓：**BSS/COMDEF tentative scalar 的相鄰與順序 linker 不
    保證，凡 reader 把多個 scalar 當 array 索引者一律 emit 成真 array**。
  - **驗證**：build 0err/0warn；wlink map 證 array 升序（base+0/4/8、spotlight 緊接其後）；FD2.EXE relink
    0 undefined 已複製進 `fd2_game_files/`。**待使用者實機測炙焰刀**。

- **開場 scene 顯示錯亂（NEW GAME → chapter 1 prologue）→ 解決（commit `a9b772e`，使用者實機確認開場顯示恢復正常）**
  - **根因**：`data_fd2_battle_cursor_screen_x/y`（0x53AB9 / 0x53ABD）src/ 誤宣告 `uint32`，原版是**有號
    `int`**。4 個 `fd2_cursor_move_up/down/left/right` 拿它和視窗邊緣比較，原版編成有號分支（JGE/JLE），
    rebuild 因 uint32 編成無號（JAE/JB）；兩者只在 cursor_screen 為負時分歧。開場走到地圖頂時 `origin_y`
    到 0、`fd2_walk_step_up` 的 scroll 分支被 `origin_y != 0` 守衛擋掉而改走 inner 分支持續 `cursor_screen_y--`
    →壓成負值；對話框 `fd2_play_dialog_open_animation` → `fd2_pan_cursor_to_tile_animated` pan 到講話者時，
    `fd2_cursor_move_down` 的無號 `< 6` 把負值座標當成極大值→誤取 `origin_y++` scroll 分支把畫面捲到地圖底
    （「對話框跳出瞬間畫面跳回底部」）。pan 仍朝講話者移動（cursor_move 照樣呼叫），故「移動方向/量看似正常」；
    fade 後 `fd2_init_battle_state_for_chapter` 歸零 cursor_screen_y→後續 scene 正常。
  - **修法（root-cause，非 workaround）**：`src/include/globals.h` + `src/ui_menu/cursor.c` 把這兩個 global
    型別 `uint32`→`int`（並改掉 cursor.c 兩段註解原本「never negative」的錯誤前提）。其餘用點全是 ++/--/×/＋
    算術，int/uint32 位元相同、不受影響（spellcin.c local 像素換算不動）。
  - **驗證**：重編 cursor.obj，WDISASM 確認 4 個比較從 JAE/JB 變回 **JGE/JL（有號，與原版一致）**；build
    0 err / 0 warn（無新轉換 warning）。Ghidra 0x53AB9/0x53ABD 型別改 `int` + 加 data plate + 0 Bad
    Instruction + 已存。FD2.EXE 已 relink（0 undefined）並複製進 `fd2_game_files/`。
  - **定位法記錄**：使用者精修症狀（捲動正確→對話框跳出瞬間跳底→pan 相對量正確→走回見 scene 2 緊貼→fade 後
    正常）一路排除：walk 用 `fd2_composite_battle_tile_map` 直繪且正確 → 排除原 handoff 頭號嫌疑（885B composite）；
    dialog 走 `fd2_composite_battle_frame` 另一路；逐一反組譯比對 walk_step_up / composite_battle_frame 皆等價、
    origin_y 在 walk 後正確 → 收斂到 cursor_move 的號性。

- **音效全靜音（SFX 啞、BGM 正常）→ 解決（commit `e8dc10e`，使用者實機確認 SFX 恢復、BGM 維持正常）**

- **音效全靜音（SFX 啞、BGM 正常）→ 解決（commit `e8dc10e`，使用者實機確認 SFX 恢復、BGM 維持正常）**
  - 根因：`protos.h` 的 AIL 函式宣告缺 clobber pragma。Watcom `-3s` 預設視 EBX callee-saved，編譯器把
    sample offset 留在 EBX 跨 `AIL_init_sample`（實際會 clobber EBX）→ `set_sample_address` 拿到垃圾
    bank 位址 → 全 SFX 靜音。BGM 正常是因第一首走的 `AIL_stop_sequence` 被條件跳過、offset 未被破壞。
    原版把值 spill 到 stack 規避，rebuild 在 emit 時遺失了 clobber 資訊。
  - 修法（統一到 vendor header）：`gen_ailv3_h.py` 的 pragma 改 `modify [eax ebx ecx edx]`（實測 Watcom
    把 modify 當精確集合，少列會把污染轉到未列的 volatile；`-3r` client 因 eax/ebx/ecx/edx 皆 arg-volatile
    而倖免，`-3s` 不然，所以 ail_extract 的 `-3r` test_audio.c 一直能播）；`src/include/protos.h` 改
    `#include "ailv3.h"`、移除 16 個 plain AIL 宣告；AIL handle 全域 `uint32`→`void *`（Ghidra 證實 handle
    是指標：`AIL_allocate_sample_handle` 回 ptr-to-slot、worker 0x41250 寫 `[handle+8]`）；`set_sample_address`
    首參 `int`→`HSAMPLE`。data-buffer 參數維持 `uint32`（遊戲 resource 層慣例）。詳見
    `rebuild_info/ail/calling_convention.md`。
  - 驗證：build_test table 18 + audio 16 測試 0err/0warn；audio.obj disasm 確認 offset 改放 ESI（AIL
    保留）跨呼叫存活；FD2.EXE 重連結 0 undefined。
- **開場 hang（第一個 scene 後黑畫面）→ 已解**（使用者確認不再卡死，先前 fname / union REGS 修復連帶解決）。
- **鍵盤失效 + sfx_flag 被清零 → union REGS scratch 拆散修復**（commit `4ca8ad0`）：原版 0x53A8D 是 28-byte
  `union REGS` 共用 INT scratch，`last_key`/`key_input_mode` 是其相鄰 byte，rebuild 拆成獨立 uint8 被
  linker 拆散。合回單一 `data_fd2_input_int16_regs` union（globals.h + `<i86.h>`），兩符號變 macro。
- **"File not found" 開場退出 → 9 處 hardcoded 字串位址改 symbol**（commit `f44a0a1`）：`fd2_load_dat_resource`
  把原版字串位址寫死成 immediate；rebuild linker 把字串擺別處 → fopen 空檔名失敗。改用 symbol。

> **重編 FD2.EXE 給實機測試**：`python tools/emit/build_test.py`（編 src obj）→ `python tools/fd2_build/`
> `{mklnk.py --apply, link_oracle.py, analyze_undefined.py}`（重連、確認 0 undefined）→ 複製
> `tests/OUT/FD2.EXE` 到 `fd2_game_files/FD2.EXE`。link_oracle 會不帶 `-Dmain` 重編 lifemain。

### 診斷工具（`tools/snd_kbd_diag/`；中間檔在 `workspace/snd_kbd_diag/`）

- **host 反組譯比對（找 codegen/layout bug 的主力，免 DOSBox）**：`WATCOM_9.5a\BINNT\WDISASM.EXE` 直接在
  Windows 跑，反組譯 `tests/OUT/obj/*.obj`（先複製到**無 `-` 的暫存目錄**再跑、用相對檔名，repo 路徑含 `-`
  會被當 option）；對照 Ghidra MCP `disassemble_function` 即為「rebuild vs 原版」差異定位法——SFX bug
  （offset 留 EBX、原版 spill stack）與開場 scene 跳底 bug（cursor_screen 號性：rebuild 無號 JAE/JB、
  原版有號 JGE/JLE）都是這樣抓到的。
- `run_fd2_audbg.py` — 把臨時 instrument 的 FD2.EXE 放遊戲目錄 headless 跑、讀 main 寫的 log 檔、還原原檔。
- `sfxdiag.c` + `run_sfxdiag.py` — 用 FD2 編譯參數重現 AIL init（install_DIG_INI）+ 依序播多個 FDOTHER SFX
  （可聽測試）；`run_sfxdiag.py N` 指定 BLASTER IRQ（負對照用，曾證 status 4→2 不可靠：IRQ 不匹配也照樣
  到 DONE）。SFX 已解，此工具留作 AIL 回歸測試。
- `genmap.py` — 重連 FD2.EXE + 產 wlink map（看 BSS symbol 實際擺放）。
- `lib_probe.py` — dump fd2common.lib / ailv3.lib 的 module+symbol。

---

### Phase 3 進度（current，最先讀）

**✅ Phase 4 LINK 里程碑達成（commit `17e7a8d`，HEAD）**：src-only `fd2.lnk` 神諭已連結出 **FD2.EXE（349 KB、合法 MZ/DOS4GW）、0 undefined** —— `src/` 已能自給自足連出遊戲執行檔（Phase 4 只剩 DOSBox 實機 playtest 對照原版，尚未做）。補掉神諭最後缺口的三件事：(1) `fd2_main`→`main`（CRT cmain386 進入點契約；是唯一豁免 `fd2_` 前綴的 game function，見 memory [[project_fd2_function_prefix_main_exempt]]）；(2) `__delay_thunk_375b2`→`fd2_delay_ms` 落地 `src/util/misc.c`（真函式 `void fd2_delay_ms(uint32 ms){delay(ms);}`，routing 651 筆）；(3) `mklnk.py` 在 `fd2.lnk` 顯式列 Watcom CRT（CLIB3S/MATH387S/EMU387；`system dos4g` 不自動 pull）。雙 main 處置：`genbuild` 在 test build 只對 `life/main.c` 加 `-Dmain=fd2_main`、`link_oracle` 不帶 define 重編 lifemain 給 FD2.EXE。TEST build 仍綠（`build_test --only table` 18/18）、Ghidra 已存、0 Bad Instruction。**重跑神諭/最終建置**：先 `python tools/emit/build_test.py`（編 src obj）→ `python tools/fd2_build/{mklnk.py --apply, link_oracle.py, analyze_undefined.py}`。

**⏸ Phase 3 暫停點（OPEN，仍未完成）**：Phase 3「測試實際全綠」尚未做完（Phase 4 LINK 是這次順著使用者提問先完成的支線）。基礎工具齊備（`--only`、minip safe-fixture；commits `e280ff8`/`762b2b1`/`57b826e`）。攻 anicine1 範本已**校準出 Phase 3b 真實難度**（見下兩條「發現」）。**正等使用者裁示 Phase 3b 排序**，使用者要先釐清問題再決定 → 新 session 先把此決定談定再動手：
- **A（建議）**：可解的非-cinematic suites 先衝綠拿動能（battle/spell 邏輯、table、save、input 等真值觀測、不依賴 display spy）＋ 建中央 `tg_install_cinematic_safe_atlases()`＋清 testglob 殘留 spy＋產出完整「spy-now-real」清單；~30 個 cinematic 法醫重設計留最後一波集中做。
- **B**：照原訂先把最難的 anicine1 整支重設計到綠（chit 相對-HP + zoom/flash safe-atlas + 4 個 #if0 重啟），確立完整法醫範本再 fan-out。
- **C**：先只做中央基建（safe-atlas + 清 spy + spy-now-real 清單）這一步，再評估下一波。

**關鍵認知：`--only` 已讓任一 suite 可獨立攻、不再被執行順序（frontier）綁住** — 故不必卡在最難的 anicine1，可任選 suite。

**使用者鐵則（覆寫一切）**：`src/` 內容絕對不可改動。唯一已授權例外 ＝ Phase 3a 補的 3 個 graphics global（見下）。目標：`src/` 每個 function 都被測到、邊界 case 完整、測試實際跑全綠。

- **Phase 3a — build 連結修復（已 commit `e280ff8`）**：3 個被 src/ 引用卻從未定義的 global（連結期 undefined，Phase 2 gate 只看編譯期 warning 而漏掉，`build_ok` 一直是 false）以 Ghidra 真值落地：`data_fd2_graphics_shimmer_offset_table_16b[16]`（const，blitspr.c）、`data_fd2_graphics_bg_animation_frame_idx`(=0) + `data_fd2_graphics_forced_tile_anim_frame`(=0xFFFFFFFF)（rndscene.c）。使用者核准的唯一 src/ 例外。結果：0 undefined、TEST.EXE 可建。
- **單 suite 驅動工具（已 commit `762b2b1`）**：`build_test.py --only <substr>` 暫濾 testmain.c 只跑指定 suite，繞過「執行順序在前的 suite hang 擋住後面全部」；跑完保證還原、不碰 src/。逐 suite 修復與 fan-out 的前提。
- **關鍵發現（決定 fan-out 策略）**：Phase 2 把 `fd2_blit_indexed_sprite` / `fd2_rle_blit_sprite` / `fd2_dialog_sprite_blit_normal` 等 spy 換成 real，但測試的**共享 fixture（`tests/include/minipfix.h` 的 `minip_setup_env`、`blitprob.h` 的 compositor-safe atlas、testglob recorder）仍是 spy 時代為「只記錄、不解碼」設計的**。real blitter 會真的解碼：minip 的 sheet offset table（`table[i]=i`）讓 mini-panel 的 bg sprite 解析到 header `rows=0` 的位置 → `fd2_dialog_sprite_blit_normal` 寫 65535 列暴衝 spin（anicine1 test #4 真正卡點，在 figani blit 之前）；digit glyph 走 real `fd2_rle_blit_sprite` 同理。**結論：Phase 3b 不能純逐 suite fan-out — 必須先「集中修共享 fixture」讓 real blit 變有界 no-op（minip sheet 種合法小 sprite、digit-glyph 源、compositor-safe atlas、cinematic figani safe-sprite），再逐 suite fan-out 改斷言 + 解 #if 0**（並行編輯共享檔會衝突，且每 suite 否則重撞同一 spin）。安全 sprite 配方：`fd2_rle_blit_sprite` 用 `[W,H>0, 每列一個 SKIP 命令 0xC0|(W-1)]`（透明 no-op）；`fd2_dialog_sprite_blit_normal` 用小 `[W=4,rows=1,+cursor bytes]`（寫 W*rows bytes，要有界）。
- **更深一層發現（決定 Phase 3b 真實難度）**：不只共享 fixture spin。**測試當初拿來當觀測 seam 的「內層呼叫 spy」有不少已被 emit 成 real**（routing done=True，link 序 src 在前 → real 勝出、testglob 的殘留 spy 被 W1027 忽略、其 recorder 永遠不被填）。實例：`test_chit_*` 用 `fd2_animate_bg_zoom_transition_in`(anispell.c, real)的 `g_zoom_in_calls` 數 strike，但該 spy 已死、且 real zoom 會 blit `data_fd2_battle_special_cinematic_bg_layers[3]`(@0x5410B)+ spotlight(@0x54107)未設 → spin（在 flash 之前）；備援觀測 `g_sfx_id_count` 也死（`fd2_play_sfx_with_handle` 亦 real, audio.c）。**testglob.c 有殘留 spy（如 1486 行 zoom_in，與 1985 行「已移除」註解自相矛盾）需清**。**結論升級**：每個 cinematic 測試是「法醫級重設計」——(a) 重建觀測手段（多數 spy 已 real）；(b) 馴服巢狀 real cinematic 的多個 sprite-source 全域。chit 可行重設計：`frame_iter` 每 do-while 迭代重置(anicine.c:846) → 傷害跨 strike 複利 → **用非零傷害 + 相對斷言（低 roll 種子最終 HP < 高 roll 種子最終 HP）**驗 3% gate，免精確傷害值；前提是 zoom/flash 全 safe-atlas。建議做一個集中的 `tg_install_cinematic_safe_atlases()`（仿 blitprob 的 compositor 版，涵蓋 bg_layer/spotlight/split_bg/anim-sheet）讓所有 real cinematic blit 變有界 no-op。
- **WIP（未 commit，皆正確的建構塊但未綠）**：`tests/include/minipfix.h`（sheet 全 offset → 合法 4x1 safe sprite，對 rle + dialog 兩解碼器都有界；shared，惠及所有用 minip 的 suite）；`tests/anim/anicine1.c` 的 `chit_plant_safe_sprite`（figani frame safe sprite）。兩者正確但不足以讓 chit 轉綠（real zoom 在更前面 spin + 觀測點已死，需上述完整重設計）。
- **覆蓋現況（reconcile 自 routing + tests grep）**：641 個真實 function，**142 個目前零有效測試**（98 個測試被 `#if 0`、44 個從未寫）；其餘 499 個邊界完整度待逐一查核。`#if 0` 主因 ＝ 寫 now-const 表（改讀固定 const 值）+ cinematic hang。9 個讀檔 function 測試被標 reverted（用假檔）需改真檔。
完整計畫：`C:\Users\fdpsf\.claude\plans\plan-plan-soft-dongarra.md`（**新 session 先讀它 + 下面這段**）。

**溝通方式（使用者要求）**：給使用者的所有文字（含對話回覆，不只文件）一律用淺白通順的繁體中文完整句子，
只有專有名詞、程式碼識別字，或絕對必要時才夾英文單字。詳見 memory `feedback_chinese_prose_readability`。

---

## 當前斷點（收斂計畫 — 最重要，先讀）

**5 階段**（每階段 hard-stop 等使用者評估；批次工作用 workflow 多代理，但每 item 由獨立 agent
親自 read_memory/disasm 檢視後個別套用，不 bulk derive）：
`Step 0 神諭+盤點 → Phase 1 真資料落地 src/ → Phase 2 補完 21 fn → Phase 3 測試重寫到全綠 →
Phase 4 收斂 fd2.exe + 實機對照`。

**Step 0 — 完成**（工具操作見 `tools/fd2_build/_index.md`、`tools/data_emit/_index.md`）
- ✅ **神諭** `tools/fd2_build/`：`mklnk.py` 產 `tests/fd2.lnk`（src-only FD2.EXE wlink）；
  `link_oracle.py` 在 DOSBox 跑 `wlink`；`analyze_undefined.py` 分類。**src-only link 的 undefined
  symbol = 「fd2.exe 還缺什麼在 src/」的權威 worklist**（以 linker 符號引用為準，比 name-grep 可靠）。
- ✅ **盤點 + 驗證器** `tools/data_emit/`：`reconcile.py` 對帳 Ghidra↔testglob↔src；
  **`verify_real.py` = byte-equality gate**（emitted data 每筆都要過；已證 28 個 real_in_src 全
  byte-identical）；`rename_global.py` 安全 whole-word 全域改名（caller 決定 old→new，工具只機械套用）。
- ✅ **改名 4 個 plain-named 全域進 data_fd2_**（commit `95e6e64`）：`battle_scene_snapshot` /
  `chapter_portrait_load_buffer` / `current_chapter_text` / `portrait_sprite_cache`（@0x53A5x cluster）。
- ✅ **11 個命名 drift 全對齊乾淨 `data_fd2_`**（commit `682444a`，`drift=0`、build 0err/0warn）：4 battle
  表統一 `data_fd2_battle_*`（spell_learning / class_promotion / movement_cost / job_allowed_items）、
  miss_indicator + orphan_table 建 Ghidra label、portrait_sprite_buffer + tile_event_data_table_ptr 加前綴、
  3 個 resource-filename string 兩邊乾淨名。**命名規則（使用者定）**：game data 一律乾淨 `data_fd2_`（battle
  核心表 `data_fd2_battle_*`、不帶 Ghidra 自動 `_<addr>` 後綴），見 memory `feedback_game_data_symbol_naming`。
  **⚠ g_ gate 陷阱**：`rename_data` / `rename_or_label` 對已定型 data（string/struct 型別）強制 `g_`、拒
  `data_fd2_`；**正解＝`run_script_inline` 跑 `symbol.setName("data_fd2_...", SourceType.USER_DEFINED)`
  （逐一、包 transaction），絕不退讓改用 Ghidra 爛名**（label 創建 / undefined-data rename 不受 gate 影響）。
- ✅ **Task #2 收尾完成**（commits `4a97fdd` `7ba6569` `28e8f2e` `b378dfa`）：
  - **stub-only 函式**：`fd2_composite_battle_tile_map`（真 884B 函式，從基線就被誤標 done、所有 branch 都查無 body）
    改 `done=false` 歸 Phase 2（await_emit 21→22）；`fd2_set_runtime_char_evade` / `fd2_wrapper_clear_keyboard_buffer`
    是已被 parent inline 的 shared-epilogue fragment，只把 routing target 修成 `<fragment:inline-epilogue>`（不動 src）；
    `fd2_delay_ticks`（battle.c）是 emitter 捏的假名，統一成全 codebase 用的 `fd2_delay_ms`，刪掉死掉的 stub/proto。
  - **dangling ref**：`crt_equivalent_dos_main_bootstrap` 早已被 Unit C（`447c37c`）修掉，新鮮 oracle 確認 crt_/AIL_ undefined=0。
  - **home-file 對映**：見下方定案 worklist。

**Phase 1（真資料落地 src/）— 四路並行 data emit 主體完成（per-symbol commit 版）**。操作指南：`tools/data_emit/_index.md`。
4 個 worktree（`../fd2-wt/dp1..dp4`，branch `data-p1..p4`，皆從 integ@`786fdc7` 切）各自把分區 data **逐 symbol commit** 落地。

> **接手鐵則：所有狀態一律從 git / 各 worktree 的 `src/data_routing.json` / `scout.py` 即時推導，不要死記下面的數字快照。**
> 每路即時查剩餘：`python ../fd2-wt/dpN/tools/data_emit/scout.py workspace/data_emit/partitions/part_N.json C:/Users/fdpsf/Documents/fd2-wt/dpN data-pN`
> （印剩餘 pending；該輸出 JSON 同時就是 mop-up 的 `Workflow` args）。

### Phase 1 + Phase 2 完成（記錄）

**Phase 1（真資料落地）✅** — 343/343 data symbol emit+review+commit、data-p1..p4 merge cascade 落入 `integ`（`cd2c0a8`）、`verify_real.py` 28/28 byte-identical、9 個誤 demote 真 const 已改回 const（原則鎖在 memory `feedback_const_data_never_demote_for_tests`）。

**Phase 2（補完 22 函式）✅** — 3 個 coordinated landing，用新工具 `tools/emit/coland.wf.js`（emit 序列 / review 並行雙模式；orchestrator 擁有 spy 刪除 + data-land + build gate + commit，sub-agent 只做單函式三源 emit / 唯讀 review）：
- **pathfind**（`2643652`）：`fd2_init_movement_range_floodfill` + `fd2_pathfind_to_destination` 真 body，刪 2 spy。
- **blit**（`59dd5e4` landing + `41750d3` reviewed）：19 函式 + 9 個 graphics blit-state 全域（`glyph_blit_state` struct〔types.h packed 17B〕+ 8 scalar/array，全 mutable zero-init）+ 13 spy 刪。
- **composite**（`42a2dd0`）：`fd2_composite_battle_tile_map`（885B 熱路徑）+ 3 個 BIOS-tick-latch 全域 + spy 刪。
- 全程：每函式獨立 reviewer 三源復驗（**22/22 approved，零 blocking**）、所有 12 個新 data 全域逐一 write-xref 定 const-ness（全有遊戲 WRITE→mutable）、保留全部 `g_*` recorder（依賴測試斷言重寫 → Phase 3）、build gate 全 0err/0warn。**Phase 2 收斂 → hard-stop 等使用者再進 Phase 3。**

### 工具現況（接手必懂；與上一版 handoff 不同處）

- **workflow 已改 per-symbol commit**（commit `2220dbb`）：emit → 獨立 reviewer 三源復核 → **lander**（per-symbol：改 globals.h extern、移 testglob 假版、`data_routing` reviewed=true、clobber 防線、`git commit`，**不 build**）→ 每 home 檔最後一次 **buildGate**（`build_test` 0err/0warn + 純機械修正：extern 對齊 / const-writer `#if0` SKIP）。**撞 limit/529/斷線零浪費**，re-scout 跳過 reviewed 續跑。見 memory `feedback_per_symbol_commit_durability`。
- **emitter anti-clobber**（commit `356b824`）：append-only、嚴禁 Write 整檔覆寫、寫完 grep 自驗檔案沒變短（防同檔其他定義被抹）。⚠ **re-emit 已有 def 的符號（半落地）要先手刪舊 def**，emitter 目前沒有「偵測既有 def 先刪再 emit」的邏輯。
- **scout 讀哪個 routing**：必須跑 **該 worktree 的** `dpN/tools/data_emit/scout.py`（由 `__file__` 解析 ROOT＝該 worktree，才讀該 worktree routing、跳過已 commit）；跑主 repo 的 scout 讀 integ routing（reviewed=4，尚未 merge）。
- **leftover 從何來 / 怎麼補**：Anthropic 529 overload 打死零星 reviewer/lander → 那些符號成為上面 6 個 leftover。workflow 對 529/limit 的行為：sub-agent retry 用盡回 null → 該符號記 needs_user/land_failed、迴圈續跑，**不崩、不停整批**；已 commit 零損失，re-scout 跳過 reviewed 補 leftover 即可。

### Phase 3 待辦（累積的測試債）

- **Phase 2 函式的依賴測試重寫（最大宗，新增）**：22 個 blit/pathfind/composite 落地後刪了共享 spy、保留 recorder（不填值），故依賴 spy recorder 斷言的套件 runtime-fail/hang（pathfind ~26 套件、blit ~40 套件〔g_rle_blit_* 10 + g_blit_indexed_* 30〕、composite ~7 套件，多有重疊）。Phase 3 逐套件改「seed 真輸入 → 驅動真函式 → 斷言真像素 / 真演算法結果」（範本 `tests/gfx/rndstat.c`、`tests/include/blitprob.h`）。**特例**：composite spy 兼 idle-loop break seam（`g_repaint_flip_buffer_after`，menu/idle-loop 測試靠它翻 BIOS 鍵盤 buffer 中斷迴圈）→ 改直接 BIOS 鍵盤注入。已知 hang：pathfind `test_ai_walk_no_path` 等。
- **const-writer SKIP**：多個 const 表的寫入測試被 `#if0` SKIP（各 build gate 回報的 `skipped_tests`，如 dp1 uitab → `tests/gfx/rndmenu.c` 13 個 promo 測試）。**最大宗＝dp4 const-fix（commit `ff05f00`）**：255 測試 + ~53 helper 跨 24 檔被 whole-function `#if0`（marker `SKIP (Phase 3): writes now-const <table>` 可 grep），涵蓋寫 view_window_max / job_magic_resist / job_crit_rate / tile_attr_mv-def / summon_spell_8slot×3 的 fixture 級聯。Phase 3 逐一還原成「讀固定 const 值佈置情境」而非寫 const。
- **dp2 `data_fd2_chapter_intro_metadata_table`（chtab3.c，維持 const）**：被共享 fixture（`tests/gfx/rndmenu.c::intro_setup()`、`tests/save/save.c::scs_setup/teardown`）寫入 → chtab3.c build gate 為保綠對這兩個 fixture 做過處置（最終 build 綠）。**接手要 review dp2 該檔相關 commits 看它具體改了什麼**（可能 SKIP/註解 fixture 寫入），Phase 3 還原。
- **dp1 `life/main.c` 自癒符號**：第一輪（commit `9691ff0`）finalizer 自補 16 個未經獨立 reviewer 的 def（4 init-data 已對 binary 驗、12 zero-bss 為 `T name;`），Phase 2.6 復驗一併過。

### 帳目（即時重算：`python tools/data_emit/reconcile.py`，勿抄）

全 563 `data_fd2_` 符號（Ghidra 即時查證一致）post-merge 分流：**real_in_src 376**（已落地 src/ file-scope，含初值表 / bss tentative / `void (*const tbl[])()` 派遣表）、**undefined 186**、**sublabel 1**（`chapter_intro_menu_typeC_portrait_id`，母表帶出）、fake_in_testglob 0。186 undefined 拆解：**106 cutscene**（chtab3.c 已 emit 單一 pool `cutscene_event_script_data` + offset 指標表，資料已落地、非待辦）+ **58 string**（使用點 inline 字面值 / strtab.c，資料已落地）+ **22 真待落地**（21 blit/pathfind/spell anim state + `stat_buff_multiplier_115` const）→ 全部隨 Phase 2 的 21 函式 emit 一起落地。**權威缺口以 src-only `fd2.lnk` 神諭的 undefined symbol 為準（Phase 4）。** reconcile.py 正確計入 tentative/bss 定義與 const 函式指標表（DEF_RE 含 `;` 結尾、FNPTR_RE 含 `(*const tbl[])`）。fd2_ 函式缺口：**0（全 650 emit+reviewed，Phase 2 完成）**；Phase 2 一併 land 12 個 graphics/compose-state 全域（blit 9 + composite 3，全 mutable zero-init）；vendor 60 + `fd2_delay_ms` = Phase 4 link。

**Phase 4 連結（LINK 步驟已完成，commit `17e7a8d`；0 undefined、FD2.EXE 產出）**：`fd2.lnk` 已由 `mklnk.py` 顯式列 `library` CLIB3S/MATH387S/EMU387（全路徑 `D:\LIB386\...`，`system dos4g` 不自動 pull）；AIL lib（`workspace/ail_extract/out/{ailv3,fd2common}.lib`）由 `link_oracle.py` 每次 stage 進 `E:\out`（`build_test.py` 會清 `tests/OUT`，故每跑 oracle 前都 re-stage）；`link_oracle` 另不帶 `-Dmain` 重編 lifemain.obj 給 FD2.EXE（test build 帶 `-Dmain=fd2_main`）。已知小 warning：曾出現一次 `cannot open fd2common.lib`（staging 時序；該 lib 不被引用、連結仍 0 undefined，非阻斷）。**Phase 4 剩：DOSBox 實機跑 FD2.EXE 對照原版。**

---

> **以下 §0–§7 為 emit pipeline / merge cascade / build 的既有 handoff（下層參考）。**
> merge cascade 已完成（historical）；§3–§5 的 merge how-to 不再需要。但 **Phase 3 測試重寫請讀
> §1 的「系統性修復階段診斷備忘（cinematic spin/fault 根因）」+ partial-skip 紀錄**（那是輸入，保留）；
> **Phase 2 的 21-fn coordinated landing 配方在 `open_issues.md` #32/#33**；§6 build 知識 + §7 鐵則仍適用。

---

## 0. 開工確認

**必讀（依序）**：本檔 → `tools/emit/_index.md`（emit-review workflow 操作）→ 需要時
`rebuild_info/emission/`、`rebuild_info/link/wlink_settings.md`。（`CLAUDE.md` / `index.md` /
`MEMORY.md` 由 session 自動載入。）

**確認工具**：Ghidra MCP 已開 FD2.LE（1375 functions）、DOSBox-X 在 PATH、Watcom 9.5a。
不可用就停下問使用者。Ghidra wedge 可自助重啟（kill 那個 ghidra javaw → `ghidraRun.bat` →
等 port 8089 → `open_program('/FD2.LE')`；詳見 memory `feedback_ghidra_disconnect_handling`）。

---

## 1. 精確斷點（最重要，先讀這段）

**Phase 0 並行基礎建設 — 完成**（commit `28e1cd6` on `main`）
- `tools/emit/mkpart.py` → `tools/emit/partitions/branch_{1..4}.json`：4 路 file-disjoint 分區
  （依 `routing.json` target 子檔切，每路 93 個未 emit function、不跨同一 `.c`）。
- `tools/emit/build_test.py` 已支援 worktree 隔離：依各 checkout 的 `REPO_ROOT` 生成
  `workspace/emit_drive/run.conf`（改寫 C:/E: mount），遊戲檔經 `$FD2_GAME_DIR`→local→主 repo fallback。
- `tools/emit/next_batch.py --partition <branch_N.json>`：限定分區 scout/stats。
- 4 worktree：`../fd2-wt/p1..p4`（branch `emit-p1..p4`，皆由 `28e1cd6`）。

**Phase 1 四路並行 emit — 完成**
- 各 branch 在自己的 worktree session 用 `--partition` 跑 emit-review，per-function commit。
- **p1 / p2 / p3：各 93 全完成**（該分區 `await_emit=0`）。
- **p4：71/93 完成，deferred 22 個 coordinated-landing function**（19 `gfx/blitspr.c` + 2
  `util/pathfnd.c` + 1 `crt/crt.c`；crt 已於 Phase 2.5 Unit C 落地，剩 21）—— **deferred 到 Phase 2.5**：其 test spy 住在共享
  `tests/testglob.c` 且被跨 branch + base 套件依賴，無法單分支落地。完整依據在 p4 worktree 的
  `open_issues.md` #32 / #33 / #34 與 `src/emit_issues.json` `0002935b`。

**Phase 2 手動 merge cascade — merge 1/2/3 全部完成（cascade 收尾）。下一步＝Phase 2.5（§4）**
- 順序：`integ`(=p1) ← `emit-p2` ← `emit-p3` ← `emit-p4`，在主 repo 工作目錄序列做。
- **merge 1（`integ` ← `emit-p2`）：完成** — commit `588d8a6`（merge 本體）+ `b6d7f13`（DOSBox-X fault
  logging + handoff 新作業方式）。
- **merge 2（`integ` ← `emit-p3`）：完成** — commit `98ea47b`。跨 branch 簽名衝突（23 個 E1071 + 10 個
  W101）依各函式 Ghidra body 用法以「真型別」收斂：A. 純轉手位址型參數改 uint32（`step_figani` dst_stride、
  `play_figani` dst_buf/bg_layer_a/bg_layer_b、`animate_spell_hit` bg_workbuf，改 `anicine.c` def + `protos.h`、
  body 不動）；B. 真指標維持指標、caller 加 cast（`resolve_terrain` uint8*、`play_figani` caster/target_figani
  uint8*、`rle_blit` rle_stream uint16*，改 `anispell.c`/`spellcin.c` caller）；C. shop item-id 陣列全鏈統一
  uint8*（`run_buy`/`open_shop_dialog_panel`/`shop_menu_input_loop` + `protos.h`，`run_sell`/`run_give` 去掉
  `(uint32)` cast；使用者選定「全鏈 uint8*」）。Watcom 顯式 cast 不報 W101（只對 call-site 隱式轉換報），
  故 A2 body 的 `(uint16*)bg_layer_a` 顯式 cast 無新 warning。測試 caller 同步對齊
  （`tests/anim/anicine2.c`、`tests/ui_menu/shop1.c`）；testglob.c stub 不 include protos.h、無須改。

- **merge 3（`integ` ← `emit-p4`）：完成** — commit `bed6602`（parents `98a6aaa` + `483c37a`）。p4 帶入 71 個
  已完成 function；22 個 coordinated-landing function（1 crt + 19 blitspr + 2 pathfnd）維持 `done=false` 留 Phase
  2.5（crt 已於 Unit C 改 link_vendor_lib 落地，剩 21）。要點：(a) p4 把 base 版測試檔 split 成 `anicomb1/2/3.c`，與 p2 的 silhouette-probe migration 三方
  reconcile；`fd2_check_char_is_dead`／`fd2_blit_indexed_sprite`／`fd2_setup_chars_and_camera_for_intro` 三個 stub
  合併兩 branch 的 recording 行為到同一份。(b) 跨 branch 簽名衝突依 Ghidra body 真型別收斂：
  `fd2_load_chapter_party_roster` out_buf→`uint8 *`（rsrc.c def + proto；使用者核准）、
  `fd2_roll_stat_gain_and_show_message` stat_ptr→`short *`（asm `ADD word ptr [EDX],AX` 證 16-bit；改 btl_turn.c
  def+caller + proto + 測試，promote.c 原本就 short*；使用者定「先讀 Ghidra 判真型別」）；重複的 setup_chars／
  display_cinematic proto dedup 成 real-def 簽名（chtrans.c／anicine.c）。**唯二改的 `src/*.c`＝rsrc.c + btl_turn.c**，
  diff 只含該型別改動。(c) integ migration 移除、p4 測試仍需的 recorder（`g_blitpass_*`／`g_blitsolid_*`／
  `g_has_char_*`）補回 testglob 定義（runtime 未填值，留系統性修復）。Ghidra `roll_stat` 函式簽名仍 stale，本次
  未改 FD2.LE，留 review/Phase 8。

- **⚑⚑ 當前操作策略（使用者定，覆寫 §3 的「逐一遷移到全綠才前進」與舊的 SKIP-to-green 做法）**：
  每個 merge 的 gate = **compile + link 通過、`error_count=0`、`warning_count=0`**（`warning_count` 指
  build_test.py 解析 build.out 的 Watcom 編譯 warning；link 階段的 **W1027 redefinition** 是 cascade 期間
  stub+real 並存、real 勝出的預期產物，**不計入 `warning_count`**、合法留存）。**merge 階段完全不追
  run-green、不為 hang/fail 去 skip 測試、不陷進任何單一 cinematic 測試的修復**。理由：等所有 branch 都
  merge 完，預期所有 fake/stub/spy 都被 real function 取代（除 p4 那批 coordinated function，見 §4），
  **屆時才開「系統性修復階段」**——一次性恢復所有已 skip 的測試 + 連同所有 merged 測試實際跑起來 + 系統性
  修復全部。merge 階段某測試 skip 與否、run 會不會 hang，**都不影響 commit gate**。
- **src/ 鐵則（使用者定，強化）**：每個 merge **絕不更動 src/ 下的 code**；變動的 `src/*.c` 必須
  byte-identical 於某一 branch（用 `git diff <branch> -- <file>` 驗證）。真有跨 branch 衝突需改 src/ 的，
  **先提出等使用者確認**，不可自行改。已核准例外（皆跨 branch 真型別收斂）：merge 1 `promote.c` 6 處指標
  cast（→`uint8 *`，配 p1 `rndmenu.c` owner `fd2_render_promote_*_grid` 簽名 + globals.h `candidate_array_ptr`
  dedup）；merge 3 `rsrc.c`（`fd2_load_chapter_party_roster` out_buf→`uint8 *`）+ `btl_turn.c`
  （`fd2_roll_stat_gain_and_show_message` stat_ptr→`short *` def+5 caller，配 `ADD word ptr` 16-bit asm 真型別）。
  （headers `globals.h`/`protos.h` 的重複 extern dedup / 型別 union 屬 merge 機制，不算「改 code」。）

- **新 session 起手（Phase 2.5）**：`git status`（乾淨、HEAD=`2063071`、branch `integ`；近期 commit＝
  `447c37c` Unit C cstart 落地 → handoff → `2063071` emu387 routing-drift 修正）。接 §4 — 剩 Unit B
  （pathfind 2 entries）→ Unit A（blit 19）共 21 個，真 body 落地需碰共享 testglob spy + 多套件，不可用
  孤立 per-function workflow。之後 Phase 2.6（review-mode 復驗 21 個）、Phase 3（收斂 main）。
  - **Unit C（crt cstart）已落地（commit `447c37c`）**：經與 Watcom 9.5a `CSTART3S.ASM` 逐指令比對，確認
    `crt_equivalent_entry_start` + `crt_equivalent_dos_main_bootstrap` 為 stock vendor cstart `_cstart_`（非 FD2
    自寫），**改走 `link_vendor_lib` 非 emit**：Ghidra 合併為單一 `_cstart_ @ 0x3c964`、加進 lookup、從 routing
    移除（653→651）、刪 entry_start emit + bootstrap stub + 2 測試。詳見 open_issues ✅#34。**Unit B/A 仍是真 emit**
    （blit/pathfind 是 FD2 自寫，逐 function 三源 emit 真 body，與 cstart 不同）。

- **merge 1 已完成遷移（committed `588d8a6`；src 僅 `promote.c` 一處、其餘全 tests/）**：
  - `src/ui_menu/promote.c`：6 個指標 cast `(int)/(uint32)`→`uint8 *`（**唯一手改的 src/*.c**，使用者
    核准 option A）。根因＝跨 branch 簽名衝突：`fd2_render_promote_*_grid` owner 是 p1 `src/gfx/rndmenu.c`，
    body 把 param 4 當 `uint8 *` 陣列索引（語意正解），p2 caller 用了較鬆的 int。（Ghidra 內這兩函式
    param 4 仍 stale 標 `int`，未同步成 uint8*，minor。）
  - `tests/ui_menu/status.c`：`fd2_run_status_screen_member_menu` 測試改注入 BIOS Esc 驅動真
    `fd2_party_roster_single_select_loop`（複用該檔既有 `kbd_inject_scancode`）。
  - `tests/gfx/rndstat.c`：刪 4 行 dead `g_blitpass_*` extern（p2 早遷成 probe、merge 漏刪）。
  - `tests/ui_menu/promote.c`：`fd2_check_char_is_dead`（real）測試改 seed `g_test_rc_array[].flags`；
    **recruit ESC 兩測試 DEFER Phase 9**（render 變 real 後 busy-wait 無 in-process 注入 seam，使用者核准）。
  - `tests/ui_menu/chintro.c`：刪冗餘 `g_blitbgfill_calls` 斷言（grid 渲染已由 g_dlg_glyph 證）。
  - `tests/gfx/rndmenu.c`：**全 9 個 render-group 測試已遷移完成**（roster/promo/cand + chapter_intro_overlay
    /dialog_panels/preview/recruit/battlescene + cand_row_offset 漏網），**compile + link 全綠**；dead
    `g_blitpass_*`/`g_blitbgfill_*`/`g_blitdim_*`/`g_battlegrid_*` extern 已清。新增共用 VGA-primary
    readback（`VGA_PRIMARY`/`vga_clear`/`vga_count_value`）：dialog_panels mode2 icons 寫死 `0xA000E`、
    mode3 roster 與 battlescene 最終 memmove 到 `0xA0000`，故裝 probe → 驅動真 render → 讀回 0xA0000；
    battlescene 用 `bp_build_atlas1`（tile id i→畫值 i+1）+ 獨立 state_atlas（highlight tile0→哨兵 0xFE）。
    recruit 寫真 `g_recr_surface`，dimmed blitter 畫 `(value&7)+0x18` 用來與 passthrough 區分。

- **probe-sprite 遷移知識（接續必讀）**：真 blitter 在 `src/gfx/blittile.c`（W1027 勝出），spy stub
  已移除。改用 probe：裝特製 sprite → 驅動真 render → 讀 painted byte（value 驗 src、offset 驗 dst、
  數不同 value 驗 count）。基礎設施 `tests/include/blitprob.h`（`bp_probe1`/`bp_count_value`/
  `bp_build_atlas1`，body 在 testglob.c）；範本 `tests/gfx/rndscene.c`。
  - **bg_fill**（`fd2_tile_blit_24x24_with_dialog_bg_fill`）：`0xC0`=填 0x49 背景（非 SKIP），bp_probe1
    畫成「(0,0)=value、其餘填 0x49」，value 仍唯一可驗。passthrough/dimmed 的 `0xC0`=SKIP，bp_probe1 原生適用。
  - rndmenu.c 已建共用 portrait-grid harness：`g_roster_surface[64000]`、`ROSTER_PROBE_OFF`、
    `roster_plant`/`roster_portrait_off`/`roster_portrait_count`、`grid_install_portrait_probes`
    （roster/promo/cand 共用）；`g_portrait_cache` 已 256→4096。
  - **關鍵**：舊測試傳假 dst（0x1000）給只記錄的 spy；真 blitter 會實寫 → 必須傳真 surface buffer。

- **merge 1：已 commit（`588d8a6`）**。以下（partial skip 紀錄 + cinematic 診斷）全部非 merge blocker、
  留待系統性修復階段；merge 2/3 也會累積同類 skip。

- **partial skip 紀錄（merge 1 期間做的；systematic phase 須重跑全套、勿當完整清單）**：因 stub→real cascade
  無法在 host 跑的 cinematic 測試已在各 suite `run_*_tests()` 註解掉（附 ASCII `/* SKIP: … */`）——
  `tests/field/chend2.c` 5 個（ch23×3 / ch27 / ch29 cinematic chapter-end）、`chevt21.c` 6 個
  （portrait_index×2 + h36_raw_counter + h34 white-flash×3）、`chevt24.c` 3 個（h46×3）。**未 skip 但已知
  待修**：chevt22/23/25/26 等仍有同類 real-cinematic spinner；assertion FAIL（如 chend2 ch21×2：
  `fd2_find_inventory_slot_with_item` 變 real → stub recorder `g_ce_find_calls` 失效，expected 96 got 0）。

- **partial skip 紀錄（merge 2 期間做的）**：`tests/field/chpost.c` 的 chapter-17 suite 4 個測試在
  `run_field_chpost_tests()` 註解掉（附 `/* SKIP */`）並把 `g_has_char_*` 改為檔內 placeholder 讓它 link——
  它們驅動的 `fd2_check_party_has_char_id` fake 已隨該函式 emit real（`src/util/misc.c`）被移除，systematic phase
  改成驅動真函式（seed `data_fd2_shared_menu_party_roster_buffer_ptr`，仿 `tests/gfx/rndstat.c`）。另 merge 2
  後 run 階段在 `tests/field/chend2.c` test 433 `test_ch26_end_positions_robot_and_advances`（ch26 cinematic）
  hang，同上述 real-cinematic 讀 garbage 根因、非 merge blocker。

- **partial skip 紀錄（merge 3 期間做的）**：`tests/life/main.c` 取 p4 側（移除 `test_main_menu_new_game`／
  `_fallback`／`_continue_quit` 三個 dispatcher 測試 + externs）——merged run function 本來就只呼叫 load_save
  兩測試，保留定義會變 unused-static W113；p4 NOTE 載明 `fd2_play_ending_and_record_clear` emit real 後 dispatcher
  路徑 block on INT 16h、屬 Phase 9 integration。systematic phase 改注入 BIOS Esc 驅動真 dispatcher。補回 testglob
  的 `g_blitpass_*`／`g_blitsolid_*`／`g_has_char_*` recorder（未填值）讓 p4 的 anicomb1/2、rndscene、chend1 測試
  link，systematic phase 改讀 real 函式真實輸出。run 階段仍有 cinematic hang（gate 忽略）。

- **系統性修復階段診斷備忘（cinematic 測試 spin/fault 根因）**：這些測試驅動 merge 後變 real 的 cinematic
  （composite / camera pan / white-flash / rising-pre-cast），但 fixture 沒餵對 input → real RLE blitter 讀
  garbage sprite source → **wild access → DOSBox-X「illegal descriptor」protected-mode fault**（彈 modal →
  卡住 → 被 build_test.py 判 hang；可由 `tests/OUT/dosbox.log` / 結果 JSON 的 `dosbox_fault` 證實）。修法＝
  逐類補 fixture：①camera origin（否則 `fd2_pan_cursor_and_window` 的 `while(target!=origin){origin±1;
  composite;}` 從 garbage 起跑要 ~2³¹ 次；`fd2_chapter_20_end` 測試已過＝證 `ce_install_safe_env` 能驅動真
  cinematic，差別在 ch20 handler 自己 re-aim camera、ch23/27/29 靠 no-op stub re-aim 沒設 origin）；②餵
  terminating SKIP sprite atlas（`tg_install_compositor_safe_atlases` 對 sweep+party>0 仍不足；h34 的
  `atlas_tbl[i]=i` 是原始索引非 SKIP → spin）；③`data_fd2_tile_anim_table_base`、真 caster screen pos
  （pos0−origin 0xE unsigned underflow）、真 tile-map（避免 `fd2_load_dat_resource` free 靜態 buffer 損 heap）。
  W1027 runtime 層：`tests/ui_menu/promote.c` `g_promote_grid_*`（37 處）stub 被 real 忽略 → runtime-fail。
  已保留的正確 stub 修正：testglob.c `fd2_setup_chars_and_camera_for_intro` 補回 real 版 camera re-aim（設
  `view_window_origin/cursor_world = camera_world`）；chend2.c calloc tile-map 修復**已 revert**（免動共享
  `ce_install_safe_env` 影響 ch20）。
- **要整批重來此 merge**：`git merge --abort` 後依 §3 重跑。

---

## 2. 整體路線

`A. 並行 emit（完成）→ B. merge cascade（Phase 2，完成）→ C. coordinated landings（Phase 2.5，
剩 21 個，Unit C cstart 已 link 落地）→ D. review-mode 復驗（Phase 2.6）→ E. 收斂 main（Phase 3）`。之後才是完整 `fd2.exe` 的
data emit（~1300 items）+ wlink 整合（`src/*.obj` + Watcom CLIB3S + AIL lib → LE）+ Layer-1
DOSBox playtest 對比；規格見 `rebuild_info/emission/`。

---

## 3. Phase 2 — merge cascade 怎麼做（methodology + gotchas）

**每個 merge step**（主 repo 工作目錄）：
1. `git merge --no-ff --no-commit emit-pX`。
2. 逐衝突檔親自手解（下列規則）；**不靠任何 script / merge-driver auto-resolve**。
3. genbuild 三檔重生（不 merge）：`git checkout --ours tests/build.bat tests/test.lnk
   tests/testmain.c` 清 marker → `python tests/genbuild.py --apply`。
4. `python tools/emit/build_test.py` 前景跑，**gate = compile+link 通過、`error_count=0`、`warning_count=0`**
   （run 階段 hang/fail/skip **一律忽略**，見 §1 ⚑⚑）。compile-error（如移除 stub 造成 `E1011 未宣告`）要
   修到 0；link 階段 W1027 redefinition 不計入 `warning_count`、合法留存。
5. gate 過即 commit（merge commit），進下一個 branch。**所有 run-green / 測試遷移工作延到「系統性修復階段」**
   （所有 branch merge 完後一次做），merge 期間不碰。

**逐衝突類別規則**：
- **`src/*.c`（emitted code）**：分區 file-disjoint → git 自動合，**零手改**。**絕不手改任何
  `src/*.c`**；驗證方式：變動的 `.c` 應全部 byte-identical（EOL-safe `git diff`）於某一 branch。
- **`tests/testglob.c`（核心）**：以 `routing.json`（working tree = merge 後）的 `done` 為每個 stub
  去留依據 —— `done=true`（已被某 branch emit 成 real）→ **移除 stub**；`done=false`（await_emit）
  → **保留**；另**永遠保留 4 個 coordinated spy**：blit `fd2_blit_indexed_sprite` /
  `fd2_rle_blit_sprite`、pathfind `fd2_pathfind_to_destination` /
  `fd2_init_movement_range_floodfill`（crt `_cstart_` 已於 Phase 2.5 Unit C 改 link_vendor_lib、stub 移除）。git 衝突上下文常
  錯位（共用結尾括號其實只屬單邊），逐 hunk 按 routing 親手重建，別盲取單邊。
- **重複定義（globals + functions）**：git 把兩 branch 各自的定義都帶入非衝突區 →
  `E1129 / E1034 / E1068` → **dedup**：保留型別／真實值正確的一份（如型別衝突保留與 `globals.h`
  一致者、有真實值版優於 zero-fill 版）；**若兩 branch 的 stub 記錄不同全域且各被自己測試依賴 →
  合併兩種記錄行為到同一份**。`globals.h` 自己也會有重複 extern，一併 dedup（用全檔掃描找）。
- **`src/routing.json`**：各 branch 翻 disjoint entry，git 多自動合；手解 conflict hunk 取聯集
  （任一 true 即 true）；驗 `reviewed` 數。
- **`src/include/protos.h` / `globals.h`**：取 extern 聯集。**「型別相同／僅參數名異」的合法 C 重複 prototype
  是 build-safe**（留到四方全 merge 後一次性 dedup）；**但「參數型別不同」的重複 prototype 會 `E1071`、必須當下
  dedup**。⚠ **跨 branch 型別衝突的 dedup 不能盲信 owner/definer 簽名**——integ 與 p3 對「位址型參數」常用不同
  慣例（uint32 vs uint8*/uint16*），且 **Ghidra 宣告 type、definer、caller 三方都可能標錯**（merge 2 實證：
  `step_figani` 的 stride 被 Ghidra 誤 type 成 `byte*`、p3 照抄；`run_buy` 的 byte 陣列被 p3 誤標 `uint32`）。
  **正解＝decompile 看 function body 怎麼用該值定真型別**（body 當 typed-array deref → 指標；當
  stride/count/flag/純轉手位址用 → uint32），proto/def/caller 三方統一成真型別，只有真指標的跨 branch caller
  才在呼叫點加 cast。完整實例見 §1 merge 2 完成摘要與 commit `98ea47b`。**globals.h dedup defrx 要涵蓋 fn-ptr-array
  定義**（`int (*name[N])(...)`）——一般 `type name=` 正則會漏（merge 2 漏過一次 → E1068）。
- **`src/emit_issues.json`**：key 聯集（8-hex）。
- **`open_issues.md`**：彙整各 branch（目前只 p4 改過 → merge p4 時帶入）。

**級聯遷移＝系統性修復階段的工作（非 merge 期間）**：移除 `done=true` stub 後，其他 branch 用該 stub
recording 全域的測試會 compile-error（`E1011 未宣告`）或 runtime-fail/spin。**只有 compile-error 要在
merge 期間修**（移除 stub 時連帶清掉只剩它在用的 recorder extern / dead 斷言，或保留必要 recorder）讓
gate 過；**runtime-fail / spin 一律延到系統性修復階段**逐一遷移成「驅動真函式 + 斷言真實輸出」
（blocking-input 類改注入 BIOS 鍵盤 buffer @0x41A/0x41C/0x41E 的 Esc/scancode 讓真迴圈離開）。

**gotchas（關鍵認知）**：
- **W1027 redefinition 不計入 `warning_count`、gate 容許**：真函式 + 殘留 stub 並存 link 時 real 勝出，
  `warning_count` 仍為 0、gate 過；依賴該 stub recording 全域的測試會 runtime-fail/spin（real 不填那些
  全域），**但那屬系統性修復階段、merge 期間不處理**。少數 stub 是 pipeline 刻意保留的 load-bearing
  （如 `fd2_play_palette_fade_in`，見其 `emit_issues` 條目），**別誤刪**。
- clang linter 對 `tests/*.c` 報的 `types.h not found` / `uint32 unknown` 等是缺 Watcom include
  路徑的環境假象，**非真錯**（Watcom build 正常）；以 `build_test.py` 為準。

---

## 4. Phase 2.5 / 2.6 / 3（remaining）

**Phase 2.5 — post-merge coordinated landings（剩 21 個）**：
- **Unit C（crt cstart，#34）已完成（commit `447c37c`）** —— 非 emit：確認 `_cstart_` 為 stock Watcom
  cstart，改走 `link_vendor_lib`（Ghidra 合併 `_cstart_`、加 lookup、routing 653→651、刪 entry_start emit
  + stub + 2 測試）。是「先讀三源判 emit-vs-link」的範例：reclassification 也是合法的 landing 結果。
- **順手修 emu387 routing-drift**：`crt_emu387_int7_fptan_opcode_worker_4c630 @ 0x4c630`（__int7/emu387.obj
  內部 subroutine，早已 link_vendor_lib 但 routing.json 殘留 entry）移除，routing 651→650；它曾是 routing 內
  唯一的 link_vendor_lib 異類（link 函式只由 lookup / `EMU387_INTERNAL_SUBROUTINES` 追蹤，不入 routing）。
- **Unit B（pathfind 2 entries，#33，~25 套件）→ Unit A（blit 19 fn，#32，~325 site / ~8 套件）** ——
  真 emit：blit/pathfind 是 FD2 自寫，逐 function 三源 emit 真 body + 刪共享 spy/stub + `g_*` recorder + 把
  依賴套件改真實 Layer-2 斷言（真像素 byte / 真演算法結果）→ build-gate 0err/0warn/全過。**不可用孤立
  per-function workflow**（刻意碰共享檔 + 多套件）。落地後在 `routing.json` 設 `done=true, reviewed=false`。

**Phase 2.6 — 21 個 coordinated function review-mode 復驗**（blit 19 + pathfind 2；Unit C cstart 走 link_vendor_lib 不經此）：它們是 coordinated 落地、未經獨立
reviewer。`python tools/emit/next_batch.py --mode review` 掃出 +
`Workflow(scriptPath:"tools/emit/emit_review.wf.js", args:…)` review 模式逐一三源復驗 → approved →
per-function commit 設 `reviewed=true`。

**收斂 main（舊 emit-pipeline 計畫的最終步；`integ`→`main` merge 已完成 `89268a4`，main tree == integ）**：剩餘 → 最終 `build_test.py` 全綠 →
`next_batch.py --stats` 應 `reviewed=650 / await_emit=0 / await_review=0` →
`list_bookmarks(category="Bad Instruction")`=0 → 清 worktree（`git worktree remove ../fd2-wt/p1..p4`）
+ branch（`emit-p1..p4` / `integ`）→ 無 dosbox 孤兒 → 回寫本檔（完工時清空 §1 斷點 + protos.h 做
那次一次性 dedup）。

---

## 5. emit-review workflow（供 Phase 2.6 review + 任何補 emit）

進度單一事實來源 = `src/routing.json` 的 `reviewed`（+`done`）。scout：`python
tools/emit/next_batch.py --mode review|emit [--partition <branch_N.json>] --limit 12`（輸出即
Workflow 的 `args.functions`）。`Workflow(scriptPath:"tools/emit/emit_review.wf.js", args:<JSON>)`：
序列一次一個 function；`emit` 模式先 emitter；reviewer 獨立三源復驗 → 迭代（≤10 round）→ approved
→ bookkeeper per-function commit（code + test + KB + `routing.reviewed=true` + emit_issues）。中斷
（token/usage limit）零成本續：`reviewed` 欄 + per-function commit = 斷點。禁忌與細節見
`tools/emit/_index.md`。

`src/emit_issues.json` 累積「需實際編譯才能確認的等價性疑慮」（FPU rounding / word width /
table-copy / fragment 等價轉移到 parent…），留待全 function 完成後（Phase 8）統一用 Watcom 9.5a
編譯 + disasm 比對解決，不在 review 階段處理。key 用 8-hex（如 `00010b43`）、utf-8。

---

## 6. build / test 知識

- build gate：`python tools/emit/build_test.py`（前景跑，回 JSON `{gate_pass, build_ok, done, failure_mode, hung_test, crash_dump, dosbox_fault, errors, warnings, tests_passed, tests_failed}`）。**merge gate 看 `error_count==0 && warning_count==0`**（不是 `gate_pass`，那含 test 結果；run 階段忽略）。
- **DOSBox-X fault logging**：`gen_run_conf()` 注入 `[log] logfile=…/dosbox.log`（在 `[autoexec]` 前，後者須最後），且 Popen 把 dosbox stdout/stderr 導到 `tests/OUT/dosbox_stdio.log`；run 後掃這兩檔的 protected-mode fault（`illegal descriptor` / GP / invalid opcode…）放進結果 `dosbox_fault`。用途：cinematic 測試驅動 real composite 讀 garbage sprite → wild access → DOSBox-X 彈「illegal descriptor」modal → 卡住被判 `hang`；`dosbox_fault` 揭露「hang 其實是 fault」。兩 log 每 run 重生（OUT 清檔不留），`-silent` 不抑制 `[log]` 檔。
- **結束偵測無固定等待**：三訊號擇一 —— `DONE.TXT` 出現（正常完成）／ DOSBox process 退出（`proc.poll()`，涵蓋正常完成與會交回 batch 的 crash 如 DOS/4GW GP fault，~2s 偵測）／ heartbeat 停滯（`tests/OUT/HB.TXT` 每個 test 重寫；run 階段停滯 `--hang-stall` 秒〔預設 20s〕且 proc 存活 → hang，`hung_test` 指出卡住的 test）。`failure_mode` ∈ completed/crash/hang/aborted/timeout。**無 stale-cache / DPMI-OOM 問題**；不要加 copy→rename / sleep / 兩段式 session 等 workaround。
- **worktree 隔離**：`build_test.py` 依各 checkout 的 `REPO_ROOT` 生成 `workspace/emit_drive/run.conf`（重寫 C:/E: mount 指向該 checkout 的 src/tests），故主 repo 與任一 worktree 都能各自正確 build；遊戲檔來源 `$FD2_GAME_DIR`→`REPO_ROOT/fd2_game_files`→主 repo 絕對路徑。`tests/dosbox.conf` 只是模板。
- heartbeat 機制：`testharn.h::TEST_BEGIN` → `test_heartbeat()`（`testglob.c`）每 test 用 fopen/fprintf/**fclose** 寫 `E:\OUT\HB.TXT`；close 才讓 DOSBox commit 到 host（`fflush` 不夠，DOSBox 快取重導向 stdout 到 file close）。DOSBox crash/hang 行為實證：`tools/hangprobe/`。
- 路徑佈局：compile cwd=`C:\`(=src，故相對源碼 + `-i=include` 可解析)，但 `.obj` 全進 `tests/OUT/obj\`、`TEST.EXE` 進 `tests/OUT`、run 段 `cd \out` 讓 TEST.EXE 以 `tests/OUT` 為 cwd。**src/ 保持乾淨**。`build.bat`/`test.lnk`/`testmain.c` 由 `tests/genbuild.py` 全產生（含 run tail），勿手改。
- **真實檔案測試（讀檔 function 鐵則）**：build_test.py 啟動前把 8 個遊戲檔（FDICON.B24 / FDFIELD/FDSHAP/FDOTHER/FDTXT/FDMUS/DATO.DAT / FD2.SAV）從 `fd2_game_files/` stage 到 `tests/OUT`（= cwd，缺或 size 不符才複製，不掛載）。讀檔 function 的 test 必須讀這些 staged 真檔並對真實解析值斷言，**禁** `write_fake_dat`/`write_fake_fdicon` 捏造 stand-in、**禁** remove() staged 真檔。reviewer checklist 7b 強制此 gate。詳見 memory `feedback_real_file_tests_mandatory`。
- C89：變數宣告在 block 開頭。8.3：檔名/目錄 ≤ 8.3（Watcom 9.5a 無 LFN）。
- `testglob.c`：fake global / stub 集中；function pointer table 必須初始化指向 noop（否則 NULL call → DOS4GW crash）；emit 真實 function 後移除對應 stub（避免 linker redefinition；但見 §3 gotchas 的 W1027-filtered 例外）。
- 測試鏡像 src 子檔（`tests/<domain>/<stem>.c`，每檔 ≤1000 行；落點查 `tests/where.py`）。新建任何 src 或測試 `.c` 檔後，跑一次 `python tests/genbuild.py --apply`（掃 `src/`+`tests/` 重產 `build.bat`/`test.lnk`/`testmain.c`，**嚴禁手改這三檔**）。

---

## 7. 嚴格鐵則（workflow 已結構性落實；人工 merge / landing 時亦遵守）

- 一次一個 function（workflow 序列保證）；三源不省略（plate / disasm / decomp），即使極簡 thunk。
- 語意完全保留；Ghidra 系統性 EAX-tracking bug（CALL 後 EAX return value 常被誤標）必對 assembly 核對。
- cc / param 從 caller 推，少報比多報危險（少報 → 讀 stack 垃圾 → crash）。
- 符號名與 Ghidra byte-identical；C89（變數宣告在 block 開頭）；檔名 8.3。
- **程式碼內文字一律 ASCII**（`src`/`tests` 的 .c/.h 註解與字串）；唯中文角色/道具/法術專名先維持中文；em-dash `—`→`--`、`→`→`->`、中文標點→ASCII、非專名中文→英文（見 memory `feedback_code_ascii_only`）。
- 絕不半成品（改名 / static / 空殼 / `_impl`）；絕不為遷就 test 而扭曲 emit code；**merge / landing 時絕不更動任何 `src/*.c` 的 code**——變動的 `src/*.c` 必須 byte-identical 於某一 branch，真有跨 branch 衝突需改 src/ 的**先提出等使用者確認**。已核准唯一例外：`promote.c` 6 處指標 cast（→`uint8 *`，配 p1 rndmenu owner 簽名）。
- emitter / reviewer **前景**跑 `build_test.py`，嚴禁 `run_in_background`（subagent 背景跑不閉環 + 留 dosbox 孤兒）。
- 風險導向 test 覆蓋：數值 / 複雜分支 / RNG / EAX-bug 風險 / 狀態轉移強制測；純 blit/display 副作用延 Phase 9 integration。
- Ghidra plate / name / data symbol 與 assembly 事實不符 → 當場 `set_plate_comment` / `rename` + 同步 KB / globals.h / testglob.c。
- merge / landing 完成後若改過 FD2.LE：檢查並修復 error bookmark（`list_bookmarks(category="Bad Instruction")`）、確認 cc 正確、儲存變更。
