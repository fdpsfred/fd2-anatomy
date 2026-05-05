# Emit pipeline 規格

emit pipeline 的目標：從 Ghidra 1699 個 function 的 decompiled state 產出 C
source code，經 Open Watcom v2 重新 compile / link 出 byte-for-byte 等價的
DOS executable（FD2.LE）。

本檔規範 emit pipeline 在處理 function boundary 與 fall-through 模式時必須
遵守的規則，避免把 Ghidra 的 Function entity 直接當 C function 輸出而破壞
binary 行為。

## Pool 路由

每個 function 依 category 走不同 emit 路徑：

| Category | 數量 | emit 策略 |
|---|---|---|
| `ail` (`AIL_*` 名稱) | 278 | **不 emit**。Watcom AIL3DIG / AIL3MDI 靜態 library 直接 link，FD2 source 端只保留 `extern` declaration |
| `crt`（純 Watcom 公開符號 `malloc` / `fread` / `sin` 等 47 個） | 47 | **不 emit**。Watcom v2 RTL 直接 link |
| `crt_dpmi_int_<NN>` (DPMI INT vector dispatch table) | 256 | **不 emit**。Watcom v2 重 compile 會自動生對應 dispatch table |
| `align_nop_*` (Watcom alignment NOP fill) | 74 | **不 emit**。Watcom v2 compile 自動生 alignment padding |
| `crt_*` 自寫 wrapper / helper | ~178 + 226 | **emit 為 C source**，照 Ghidra 的 function boundary 直接 emit |
| `game` (game logic) | 638 | **emit 為 C source** |

## Fall-through 模式必須特別處理

對 FD2.LE 全 1699 個 function 跑 fall-through audit（prev_fn 末尾 inst 有
fall-through 進 this_fn entry），共 25 個非 align_nop 案例（詳見
`workspace/function_review/phase_g_25_classification.md`），分 6 種模式。emit
pipeline 對每個模式有強制處理規則。

### 模式 A: SHARED EPILOGUE STUB

**識別**: prev_fn 末尾無 RET，直接 fall-through 進 this_fn；this_fn 是 RET-only
共用 epilogue（純 stack cleanup + RET），多個 source function 也透過 tail-JMP
進入此 epilogue。

**例子**: `noop_stub_b43` 被 `load_chapter_battle_data`（fall-through）+
`play_rising_pre_cast_effect`（JMP）+ `play_variant_b_slide_pre_effect`（JMP）共用。

**完整清單**: 8 個 case — `noop_stub_b43` / `noop_stub_c49` / `noop_stub_1011` /
`noop_stub_1452` / `noop_stub_13994` / `set_battle_anim_phase_to_1` /
`AIL_log_decrement_nesting` / `noop_stub_3cbc4`。

**emit 規則 A-1**：把 SHARED EPILOGUE 視為 fragment，**不**獨立 emit 為 C function。
改為：

1. 把 epilogue 程式碼 inline 到每個 source function 末尾（複製 `ADD ESP / POP regs /
   RET` 那 N 個 instruction 的等效 C 描述）
2. 或 emit 為 `__declspec(naked)` + 內嵌 `__asm` block（Watcom 支援）：

```c
__declspec(naked) void noop_stub_b43(void) {
    __asm {
        add esp, 0xc
        pop ebp
        pop edi
        pop esi
        pop ebx
        ret
    }
}
```

3. 在 source function 結尾改用 `goto epilogue_label;` 然後 label 寫 inline epilogue
   程式碼（純 C，無 inline asm）

### 模式 B: MULTIPLE ENTRY POINTS / SHARED BODY

**識別**: 兩個函式共用同一段邏輯主體，以不同 entry 進入做不同 setup，最終 JMP
進共用 body。prev_fn body 涵蓋共用 body 區段，this_fn body 只涵蓋 entry-specific
setup + JMP 進共用 body。

**例子**: `load_and_fade_in_cinematic_image`（載圖 + JMP fade loop）/
`play_palette_fade_to_black`（直接 JMP fade loop）共用 0x1f51e 起的 fade 主迴圈。

**完整清單**: 3 個 case — `play_palette_fade_to_black` /
`check_battle_end_condition` / `crt_softfp_uint32_to_ld`（與其同伴 entry 配對）

**emit 規則 B-1**: 抽出共用 body 為 internal helper function，兩個 entry 各別
emit 為 wrapper：

```c
static void palette_fade_to_black_loop(int param_1, int param_2) {
    /* 共用 fade 主迴圈 */
}

void play_palette_fade_to_black(int frame_arg) {
    palette_fade_to_black_loop(frame_arg, 0);
}

void load_and_fade_in_cinematic_image(uint chapter_id, uint flag, uint frame_arg) {
    /* 載圖 setup */
    set_vga_palette_range(...);
    load_dat_resource(0x51a4d, 0x53a65, chapter_id);
    crt_memset(0xa0000, 0xff, 0xfa00);
    play_ani_file_animation_sequence(...);
    palette_fade_to_black_loop(frame_arg, 0);
}
```

### 模式 C: HEADER-ONLY ENTRY

**識別**: prev_fn 只有「pre-PUSH 一些常數作為 args / 推 frame_size」這類 header
工作（prev body ≤ 15 bytes，無實質運算），fall-through 進真正做事的 this_fn。

**完整清單**: 4 個 case — `chapter_event_handler_18__unref_dialog` /
`chapter_event_handler_20__ch10_dialog` / `chapter_event_handler_34__ch23_ai_ctrl` /
`chapter_event_handler_36__ch24_cinematic`。

**emit 規則 C-1**: prev 視為「先做 setup 然後 tail-call this_fn」的 wrapper，
explicit emit 兩個 function：

```c
void chapter_event_handler_20__ch10_dialog(void) {
    /* prev 只 PUSH 0x28 表示要把 frame size 0x28 傳給 this 的 crt_frame_setup */
    show_chapter_dialog_with_portrait_set_1(/* args derived from prev's pushes */);
}
```

注意：這類 case 的 prev 通常 frame_size 不同於 this，emit 時要保留 frame_size
差異（透過 caller 推得）。

### 模式 D: DEAD FALL-THROUGH

**識別**: prev_fn 末尾的 fall-through 在執行流上死掉（INT/CALL 不返回 / 條件分支
全部把控制流帶回別處），this_fn 雖然是真 function 但這個 fall-through 路徑用不到。

**完整清單**: 5 個 case — `set_runtime_char_evade` /
`init_chapter_misc_state_block` / `crt_parse_fopen_mode` / `crt_terminate`（從
crt_entry_start_dup）/ `crt_terminate`（從 crt_abort_with_log）。

**emit 規則 D-1**: prev 與 this 各自 emit 為一般 C function。Compiler 可能會
optimize 掉 fall-through 的「死碼」，但因為 dead 所以行為不變。

**emit 規則 D-2**（特殊：interleaved bodies，case 9）: tick_summon_spell_animation_state
與 init_chapter_misc_state_block 的 body 地理交錯，要把 init 的工作 inline 進 tick
的對應 state branch，並且 init 的 JMP-back 改為 tick 內部的 goto label。

### 模式 E: DATA TABLE FRAGMENT

**識別**: prev_fn 的 body 是 data table（jump table / 常數表）的 byte 區段，
被誤判 disassemble 為 code（Ghidra 的 Function entity 是建在 data 上）。

**完整清單**: 4 個 case — `AIL_helper_3c7b4` (2-byte tail) / `AIL_helper_3c962`
(2-byte zero pad) / `AIL_helper_41548` (24-byte jump table) / `AIL_helper_44548`
(8-byte jump table)。

**emit 規則 E-1**: emit pipeline **不**把這些當 function emit。應該在 emit pass
之前把這些 byte 區段重新 mark 為 data，並 emit 為對應的 data table。具體而言：

- 對應到 jump table 的：emit 為 C array 或 Watcom-specific switch jump（或讓
  Watcom v2 自己生 jump table，只要 source 寫對 switch 結構）
- 對應到 alignment / padding 的：完全 skip（Watcom v2 重新對齊）

### 模式 F: STATE-MACHINE INIT-ENTRY

**識別**: prev_fn 是 state-machine 的「初次進入 setup」（PUSH regs、初始化某 reg
為 0），fall-through 進 loop body that uses 該 reg as state；loop body 用完一輪
JMP 回自己。

**完整清單**: 1 個 case — `AIL_helper_41834` (init: EBP=0) → `AIL_helper_4183d`
(loop with state in EBP)。

**emit 規則 F-1**: 合併兩函式為一個 driver function：

```c
void AIL_state_driver(seq_state *st) {
    int phase = 0;
    while (1) {
        switch (st->next_event_byte) {
            case 0: /* ... */ break;
            ...
            default: return;
        }
        phase = next_phase;
    }
}
```

## 不變式（按等價程度分層）

emit pipeline 完成後 re-link 出的 binary 必須滿足下列三層 invariant。每層適用
範圍不同，對應驗證手段也不同。

### Layer 1: specification-exact（最低保證，全範圍）

重 link 出的 .EXE 在 DOSBox-X 內跑時，下列**外顯行為**必須與原 FD2.LE 完全一致：

- 30 章劇情的對話 / 過場 / 戰鬥流程
- FD2.SAV byte-level 兼容（讀原版存檔可繼續、新存的存檔原版可讀）
- BGM / SFX 觸發時機與曲目選擇
- 螢幕 pixel output（同 input scancode 序列下，每一 frame 的 mode13h buffer 內容相同）

驗證手段：DOSBox-X silent mode 跑 scripted gameplay session（按既定 input
scancode 序列），dump screen buffer / FD2.SAV / 觸發的 BGM track ID 與原版對比。

### Layer 2: functionally-exact（game pool + crt 自寫 wrapper，~1044 個 function）

對於這 ~1044 個 emit-out-of-source 的 function，每個 function 在「相同 input
register / stack / memory state」下執行完，必須產出「相同的 return value /
register state / 寫入 memory 的 bytes」。

不要求 instruction 級別 byte-相同 —— 因為 Open Watcom v2 的 register
allocation / instruction selection / scheduling 與 1998 年 Watcom 不可能完全
一致。

驗證手段：對 pure-compute leaf function（damage 計算、softfp、decoder helper、
hash / checksum）跑 emulator 雙邊 trace（原 FD2.LE vs 重建版），對相同 input
比對 final state。

### Layer 3: byte-exact（aspirational，無強制範圍）

`.object1` section 在某段 address range 內 byte 完全相同。**這是 nice-to-have
而非要求**——只在某個 function 真的編出 byte-exact 時當作額外信心指標，不能 byte-exact
也不算 emit pipeline 失敗。

不可強求 byte-exact 的原因：

- Watcom v11/12 (1998) → Open Watcom v2 (現代) 之間的 instruction selection /
  register allocation / scheduling 差異
- function 排列順序由 linker 決定（`.obj` 順序、CRT `.obj` 插入點影響相對 jump offset）
- alignment padding byte 內容（Watcom 不同版本選不同 NOP encoding）
- jump table vs branch tree 等 switch 實作策略差異

範圍說明：

- **AIL pool（278 個）**: 不適用 byte-exact（vendor static lib 重 link）；Layer 2
  也不適用（vendor 不重新實作）；只需要 Layer 1
- **Watcom RTL（47 公開符號 + 256 DPMI INT + 74 align_nop = 377 個）**: 不適用
  byte-exact；Layer 2 由 Watcom v2 RTL 自身保證；只需要 Layer 1
- **game + crt 自寫 wrapper（~1044 個）**: 強制 Layer 2，期望 Layer 3 但不強求

### 結構性不變式（與 binary 等價無關）

emit pipeline 還必須滿足：

1. **0 個 vendor_* / FUN_* 殘留** — emit 時所有 function 都已有 best-effort
   邏輯名稱，無 vendor placeholder
2. **fall-through chain 全部 emit 為 explicit C 控制流** — 不允許依賴 C source
   檔內 function 之間的 declaration 順序（Watcom 不保證 source 順序 = link 順序）
3. **DATA TABLE FRAGMENT 不 emit 為 function** — 4 個 case 必須在 emit 前重新
   mark 為 data，或在 emit logic 內特判 skip（見模式 E 與下方「開放問題」）

## 開放問題

1. **DATA TABLE FRAGMENT** 的 4 個 case 目前在 Ghidra 還是 Function entity，
   `program_info` 的 KB 描述中已標 `AIL_helper_<addr>` placeholder。emit pipeline
   啟動前需要把這 4 個區段重 mark 為 data（或在 emit logic 內特判 skip）。

2. **interleaved body** (case 9) 的 inline 重組策略需要在 emit 設計階段詳細規劃。
   tick_summon_spell_animation_state 與 init_chapter_misc_state_block 必須以
   單一 C function 形式輸出，且狀態機 fall-through / JMP-back 邏輯透過 goto label
   或 switch 重組。

3. **HEADER-ONLY ENTRY** (cases 10-13) 的 chapter_event_handler 系列 entry，
   它們的 frame_size 差異 emit 時如何體現需驗證（Watcom 編譯器是否能透過
   compiler-specific pragma 控制 frame_size，否則需要手寫 inline asm prologue）。

## 引用

- 25-case 詳細分析：`workspace/function_review/phase_g_25_classification.md`
- align_nop xref check：`workspace/function_review/phase_g_align_nop_xrefs.json`
- audit script 邏輯：`workspace/function_review/phase_g_wrong_split_classified.json`
