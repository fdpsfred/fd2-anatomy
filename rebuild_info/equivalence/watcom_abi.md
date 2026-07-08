# Watcom ABI（FD2.LE）

FD2.LE 是 **Watcom C/C++ 9.5a 編譯**的 32-bit DOS LE executable。本檔是 Watcom
ABI 的結論參考，供維護 src/ 時推導每個 function 的 C signature（calling convention、
參數數量、回傳值放置、shared epilogue 的還原方式）。

## Watcom 32-bit register-based ABI 摘要

Watcom 對 32-bit DOS / OS/2 / Windows target 的預設呼叫慣例是 `__watcall`
（register-based）。FD2.LE 觀察到的實際情況是**混用**：

- 自寫 game logic：多數採 cdecl 風格（純 stack args + caller cleanup）——
  編譯時偏向 `-3s`（stack-based）或函式級指定 `__cdecl`
- soft-FP / 部分 helper：採 watcall 風格（register input + callee cleanup `RET N`）
- 標準 C runtime 入口（fopen / fread / malloc / printf 等）：cdecl，與 ANSI C 相容

因此 cc 必須 **per-function disasm 判定**，不能假設整體使用同一個 cc。

### 三種 Watcom cc 的核心差異

| 項目 | `__watcall`（Watcom register） | `__cdecl`（stack, caller cleanup） | `__stdcall`（stack, callee cleanup） |
| --- | --- | --- | --- |
| 前 4 個整數/指標 args | EAX, EDX, EBX, ECX（依序） | 全部 push 到 stack | 全部 push 到 stack |
| 第 5 個及以後 args | push 到 stack（右到左） | 同 | 同 |
| Stack cleanup | callee（`RET N`） | caller（`ADD ESP, K`） | callee（`RET N`） |
| Args push 順序 | 右到左 | 右到左 | 右到左 |
| Return value（int / pointer） | EAX | EAX | EAX |
| Return value（long long） | EDX:EAX | EDX:EAX | EDX:EAX |
| Return value（float / double） | 8087 stack（ST(0)） | 同 | 同 |
| Callee-saved registers | EBP, ESI, EDI（**EBX 不保**，是 arg reg） | EBP, EBX, ESI, EDI | EBP, EBX, ESI, EDI |
| `this`（C++ member） | EAX = `this`（同 watcall 第一 arg） | 第 0 個 stack arg | 第 0 個 stack arg |

### 可變參數函式（varargs）

任何含 varargs（`...`）的函式必須使用 `__cdecl` —— caller cleanup 是 ANSI C
varargs 的 ABI 強制要求（callee 不知道 stack arg 數量，無法自己 cleanup）。

## Stack-probe helper（`__CHK` @ 0x36cd7）

Watcom CRT 在每個有 stack frame 的函式入口插入一段 stack-overflow check 序列：

```
PUSH <framesize_imm>            ; 預留的 frame 大小
CALL 0x36cd7                    ; CRT 端 stack-check helper
```

`__CHK @ 0x36cd7` 的內部邏輯：

```
00036cd7  XCHG dword ptr [ESP + 0x4], EAX   ; 把 framesize 換到 EAX，原 EAX 保到 stack
00036cdb  CALL 0x36cea                       ; 真正的 stack-check routine
00036ce0  MOV EAX, dword ptr [ESP + 0x4]    ; 還原 EAX
00036ce4  RET 0x4                            ; pop 那個 framesize arg
```

`0x36cea` 是 inner check `__STK`（`void __watcall __STK(uint framesize)`），
用 `CMP AX, SS` + 與 stack-limit global 比較，溢出時 trap。相關 global：
saved SS 為 `data_crt_stk_check_saved_ss @ 0x52794`，stack limit 為
`data_crt_stk_check_stack_limit @ 0x52814`。

probe 完成後 callee 才接 callee-saved register push（`PUSH EBX/ESI/EDI/EBP`）與
local-variable allocation（`SUB ESP, N`）。**辨識 prologue 時要跳過這 2 條 stack
probe 指令** —— 它們不是 cc 訊號。

`CALL __CHK` 會 clobber EAX/EDX/ECX/EBX（caller-saved + watcall reg arg 全在內），
所以 prologue 偵測「callee 在 frame setup 前讀取 EAX/EDX/EBX/ECX」時，必須把每個 CALL
視為對這 4 個 register 的 implicit write。

（`src/` 的 stack-check 分布與 `__CHK` 誤觸 Stack Overflow 的來龍去脈見
`../build_test/`；本檔只描述 helper 的 ABI 機制。）

## ABI 判斷規則（disasm signal-based）

| 情境 | cc 判定 |
| --- | --- |
| 末指令 `RET 0`（caller cleanup）+ caller 一致做 `ADD ESP, K` | `__cdecl`，N stack args = K/4 |
| 末指令 `RET 0` + caller 不做 cleanup + caller 一致設 EAX/EDX/EBX/ECX | `__watcall`，K reg args |
| 末指令 `RET N` + callee prologue 讀 EAX/EDX/EBX/ECX | `__watcall`，含 reg + N/4 stack |
| 末指令 `RET N` + 無 reg evidence | `__stdcall`，N/4 stack args |
| 末指令 `TAIL_JMP` 直接到已知 function | 沿用 jump target 的 cc |
| Thunk | 沿用 thunked target 的 cc |
| 名稱含 `printf`\|`sprintf`\|`scanf`\|`format` 或 varargs 證據 | 強制 `__cdecl` 並設 VarArgs flag |

判斷信號**強度排序**：

1. **Caller 一致 `ADD ESP, K`** → `__cdecl` 最強訊號（caller cleanup 是 ABI 強約束）
2. **Callee 末指令 `RET N`** → callee-cleanup 訊號（`__stdcall` 或 `__watcall`）
3. **Caller pre-CALL 設定 EAX / EDX / EBX / ECX**（不是 push 出來再讀的）→ `__watcall` 訊號
4. **Callee prologue 在 stack-probe 後立刻讀 EAX/EDX/EBX/ECX**（且不是先被 CALL 汙染）→ `__watcall` 訊號（fallback）
5. **EBX 在 entry 被當輸入讀（沒有先 PUSH EBX 保留）** → 強烈 `__watcall` 信號
   （cdecl/stdcall callee 視 EBX 為 callee-saved，一定先 `PUSH EBX` 再讀；
   只有 watcall 把 EBX 當第 3 個 reg arg 直接讀）

### Watcall 的特例：register count 由 caller 集合決定

Watcom 的 `__watcall` 允許函式只用前 K 個 register（K = 0..4）作 reg arg，其餘走
stack。判定 K 時要看所有 caller 對 EAX/EDX/EBX/ECX 的設定一致性：

- 若所有 caller 都設 EAX 但不設 EDX → K = 1
- 若所有 caller 都設 EAX, EDX, EBX 但不設 ECX → K = 3
- 若 caller 對某 register 有時設有時不設 → 該 register 不算 reg arg

## Parameter 數量規則（caller-derived）

宣告的 param 數量必須與 caller 行為一致：

```
__cdecl    : N_params = max(callers_add_esp_K) / 4
             若無 caller cleanup 訊號 → N_params = 0
__stdcall  : N_params = RET_N / 4
__watcall  : N_reg   = max consecutive prefix of (EAX, EDX, EBX, ECX) that
                       ALL callers agree on (K = 0..4)
             N_stack = RET_N / 4
             N_params = N_reg + N_stack
```

宣告 param 數**少於**實際 ABI → callee 從 stack 讀垃圾，crash。
宣告 param 數**多於**實際 ABI → caller 多 push 不會被 callee 用到的 args，浪費但
不破。**少報比多報危險。**

## Special-case function（pinned）

| 地址 | 名稱 | cc | 說明 |
| --- | --- | --- | --- |
| `0x36cd7` | `__CHK` | `__stdcall` | Watcom CRT stack-check helper；1 個 stack arg = framesize；`RET 4` |

`__CHK` 在 cc classifier 內 hardcode 為 `pinned: true`，避免被 re-classify 誤判。

## Function-pointer dispatch table callees 的 signature

透過 dispatch table 間接呼叫的 callee，其 C signature 由 dispatch site 是否 push
args 決定。function pointer table 的型別必須統一，即使某些 handler 不讀該 arg 仍需
宣告成 table 的一致型別。各群組的成員數以
`search_functions_enhanced(name_pattern=..., regex=true)` 即時 dump，不在此固定。

### 0-arg dispatch callees — `void __cdecl func(void)`

| 群組 | dispatch 表 |
| --- | --- |
| `fd2_chapter_NN_init` | `data_fd2_chapter_init_handler_table @ 0x51D71`（30 slot） |
| `fd2_chapter_NN_end` | `data_fd2_chapter_end_handler_table @ 0x51DE9`（30 slot） |

dispatch sites（`fd2_main_menu_dispatcher @ 0x25f10/0x260f5`,
`main @ 0x25e3a/0x25e23`）皆 `CALL dword ptr [EAX*4 + table]`，無 PUSH / ADD ESP，
故 callee 為 0-arg signature。

### 1-arg dispatch callees — `void __cdecl func(uint event_arg)`

| 群組 | dispatch 表 |
| --- | --- |
| `fd2_chapter_NN_post_action` | `data_fd2_chapter_post_action_handler_table @ 0x51B19`（30 slot） |
| `fd2_chapter_event_handler_*` | `data_fd2_battle_ai_post_action_consequence_table @ 0x51B91` |

`0x51B19` dispatch sites（5）：`fd2_game_main_loop @ 0x1197b`,
`fd2_tick_status_effects_and_show_messages @ 0x1a94d`,
`fd2_npc_turn_phase_team1 @ 0x1d8a0`,
`fd2_enemy_turn_phase_team0 @ 0x1d96c/0x1d9fc`，皆 PUSH 1 arg + ADD ESP, 0x4（K=1 cdecl）。

`0x51B91` dispatch sites（3）：`fd2_handle_tile_event_interaction @ 0x19511`,
`fd2_fire_chapter_turn_events_for_phase @ 0x1a85a`,
`fd2_process_battle_drop_entries @ 0x1ac1a`，皆 PUSH 1 arg + ADD ESP, 0x4（K=1 cdecl）。

### 3-arg dispatch callees — `void __cdecl func(uint caster_unit_id, uint num_targets, byte *target_id_array)`

| 群組 | dispatch 表 |
| --- | --- |
| `fd2_cast_*` | `data_fd2_battle_spell_handler_table @ 0x51D01`（28 slot） |

dispatch sites：`fd2_execute_ai_offensive_spell @ 0x1541f`,
`fd2_spell_selection_menu_main @ 0x1d479`，皆 PUSH 3 args + ADD ESP, 0xc（K=3 cdecl）。

## Shared epilogue / out-of-line tail —— 不可獨立宣告的 function

Ghidra 自動分析把一批 parent function 的共用 epilogue 或 out-of-line tail 各 carve
成獨立 Function entity。它們不是真實 callable entity：caller 透過 fall-through 或
tail-JMP（`e9` rel32）進入，Ghidra 把 tail-JMP 顯示為 `UNCONDITIONAL_CALL` 只是
JMP target 落在 function entry 上的 display quirk。Watcom C 對相同 frame layout 的
多個函式會共用同一段 stack-cleanup 尾段以節省 code size，因此同一段 epilogue 常被
多個 source 共用。

還原成 C source 時：把這些 fragment 的 logic 收回各 parent 末尾（epilogue 的
`ADD ESP / POP regs / RET` 由各 parent 自己的 frame layout 重新生成，帶回傳值者由
各 parent 的 `return` 重新生成），**不**把 fragment 宣告成獨立 function。每個 fragment
的 plate comment 標了 `DECOMPILER FRAGMENT — DO NOT DECLARE INDEPENDENTLY`。

完整清單（合併純 RET-only shared epilogue stub 與帶少量 logic 的 out-of-line tail）：

| 地址 | 名稱 | pool | 結構 | 共用它的 source / 對應 parent frame |
| --- | --- | --- | --- | --- |
| `0x10b43` | `fd2_noop_stub_b43` | fd2 | ADD ESP 0x4 + ADD ESP 0x8 + POP EBP/EDI/ESI/EBX + RET（locals=0xC + 4 saved regs） | `fd2_load_chapter_battle_data`（fall-through）+ `fd2_play_rising_pre_cast_effect`（JMP）+ `fd2_play_variant_b_slide_pre_effect`（JMP） |
| `0x10c49` | `fd2_noop_stub_c49` | fd2 | ADD ESP 0x4 + POP EDI/ESI/EBX + RET（locals=0x4 + 3 saved regs） | 多 source 共用 |
| `0x11011` | `fd2_noop_stub_1011` | fd2 | ADD ESP 0x34 + POP EBP/EDI/ESI/EBX + RET（locals=0x34 + 4 saved regs） | 多 source 共用 |
| `0x11452` | `fd2_noop_stub_1452` | fd2 | ADD ESP 0x20 + POP EBP/EDI/ESI/EBX + RET（locals=0x20 + 4 saved regs） | 多 source 共用 |
| `0x13994` | `fd2_noop_stub_13994` | fd2 | ADD ESP 0x5C + POP EBP/EDI/ESI/EBX + RET（locals=0x5C + 4 saved regs） | 多 source 共用 |
| `0x35c15` | `fd2_set_battle_anim_phase_to_1` | fd2 | RET-only shared epilogue | 多 source 透過 fall-through / tail-JMP |
| `0x37f05` | `AIL_internal_log_decrement_nesting` | ail | RET-only shared epilogue | 多 source 透過 fall-through / tail-JMP |
| `0x3cbc4` | `__GETDS` | crt | RET-only shared epilogue（byte-match Watcom CLIB3S `cstart.obj`，vendor-linked） | 多 source；此段隨 vendor lib 連入，不重寫 |
| `0x114fb` | `fd2_set_runtime_char_evade` | fd2 | 1 logic op + ADD ESP 0x10 + POP EDI/ESI/EBX + RET | tail of `fd2_recalculate_combat_stats`（locals=0x10） |
| `0x17ee8` | `fd2_wrapper_clear_keyboard_buffer` | fd2 | `CALL fd2_clear_keyboard_buffer` + POP EBX + RET（locals=0 + 1 saved reg EBX） | `fd2_open_status_screen_with_slide_in @ 0x17e0b`（JL fall-through at 0x17ec8）+ `fd2_init_battle_state_for_chapter @ 0x205da`（tail JMP at 0x20678） |
| `0x15983` | `fd2_score_item_candidate_tail_15983` | fd2 | `MOV EAX,EDI` + `JMP 0x22bbe`（帶回傳值 tail）| `fd2_score_item_candidate @ 0x15880`（EDI=total_score）+ `fd2_alloc_and_blit_indexed_sprite_chunk @ 0x15f0e`（tail JMP at 0x15f7f，EDI=malloc buffer pointer） |

`0x15983` 是**帶回傳值**的 return-tail：`MOV EAX,EDI` 先把 parent 的回傳值載入 EAX，
再 `JMP 0x22bbe` 落入共用的 `ADD ESP,4 / POP×4 / RET` 尾段（該尾段亦為
`fd2_composite_battle_frame_zero @ 0x22bb7` 的 epilogue）。它不是「進入別的 function」的
prologue adapter，`0x22bbe` 只是被多個函式共用的 stack-cleanup 尾段。

## helper 型別：`long_double_80`

`long_double_80`（10 bytes）是 Watcom C++ 80-bit extended precision long double 的
struct 表示：`dwMantissa_lo: uint32 @+0`、`dwMantissa_hi: uint32 @+4`、
`wSign_exp: uint16 @+8`。它供 `__int7`（EMU387 software-FPU emulator，body
0x49d98..0x4cbcd）內部的 soft-FP long-double 運算使用；`__int7` 的歸屬與內部
subroutine 見 `../crt/symbol_inventory.md`。

## 寫程式碼時的速查

要復刻 FD2 function 的 C 簽名時：

1. **看 callee 末指令**：
   - `RET 0` → 候選 cdecl 或 watcall（0 stack arg）
   - `RET N` → 候選 stdcall 或 watcall（N/4 stack args + 可能 reg）
2. **看 caller 端**：
   - CALL 前用 `MOV EAX/EDX/EBX/ECX` 設定 register（不是先 PUSH 再 POP 的）→ watcall，前 K 個 arg 是這些 register
   - CALL 後 `ADD ESP, K` → caller cleanup，K/4 = stack args
3. **看 callee 入口**：跳過 `PUSH framesize; CALL 0x36cd7` 的 stack probe，再看
   callee 是否在第一個 PUSH/SUB ESP 之前讀取 EAX/EDX/EBX/ECX（注意排除 CALL 的
   回傳值汙染）。**EBX 被讀但沒 PUSH EBX 保留** → 強烈 watcall 信號
4. **C++ member function**：用 watcall + EAX = `this`；或宣告為 free function 加
   顯式 `void *this` 第一參數（FD2 是純 C，實際不會遇到）
5. **varargs（`...`）**：強制 `__cdecl`，禁用 watcall

## cc 分布（觀察結論）

FD2 編譯時整體偏向 `-3s`（stack-based），絕大多數 function 是 `__cdecl`。
`__watcall` 集中出現於 Watcom CRT soft-FP / long-double family（`0x4b400-0x4dfff`
一帶）、AIL internal `*_inner` helper，以及少數手寫 register-passing helper。
`__stdcall` 極罕見，`__CHK @ 0x36cd7` 是代表案例。C++ `__thiscall` 不採用（FD2 是純 C）。
實時 cc 分布直接從 Ghidra 拉或 grep `build_call_graph.py` 產出的 call graph，不固定
數字以免 stale。
