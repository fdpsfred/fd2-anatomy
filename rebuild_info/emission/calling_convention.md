# Calling Convention (FD2.LE)

FD2.LE 是 **Open Watcom C++ 編譯**的 32-bit DOS LE executable。本檔案是 Watcom
ABI 結論與規則參考，作為復刻 / 重新編譯時的 C signature 推導依據。

1000 函式的 cc 已全部對齊 Watcom ABI，本檔末段含最終分布統計。

## Watcom 32-bit Register-Based ABI 摘要

Open Watcom 對 32-bit DOS / OS/2 / Windows target 的預設呼叫慣例是 `__watcall`
（register-based）。FD2.LE 觀察到的實際情況是 **混用**：

- 自寫 game logic：多數採 cdecl 風格（純 stack args + caller cleanup）—
  原始碼推測編譯時用 `-3s` (stack-based) 或函式級指定 `__cdecl`
- soft-FP / 部分 helper：採 watcall 風格（register input + callee cleanup `RET N`）
- 標準 C runtime 入口（fopen/fread/malloc/printf 等）：cdecl，與 ANSI C 約定相容

故 FD2 的修正策略是 **per-function disasm 判定**，不能假設整體使用同一個 cc。

### 三種 Watcom cc 的核心差異

| 項目 | `__watcall` (Watcom register) | `__cdecl` (stack, caller cleanup) | `__stdcall` (stack, callee cleanup) |
| --- | --- | --- | --- |
| 前 4 個整數/指標 args | EAX, EDX, EBX, ECX（依序） | 全部 push 到 stack | 全部 push 到 stack |
| 第 5 個及以後 args | push 到 stack（右到左） | 同 | 同 |
| Stack cleanup | callee (`RET N`) | caller (`ADD ESP, K`) | callee (`RET N`) |
| Args push 順序 | 右到左 | 右到左 | 右到左 |
| Return value (int / pointer) | EAX | EAX | EAX |
| Return value (long long) | EDX:EAX | EDX:EAX | EDX:EAX |
| Return value (float / double) | 8087 stack（ST(0)） | 同 | 同 |
| Callee-saved registers | EBP, ESI, EDI（**EBX 不保**，是 arg reg） | EBP, EBX, ESI, EDI | EBP, EBX, ESI, EDI |
| `this` (C++ member) | EAX = `this`（同 watcall 第一 arg） | 第 0 個 stack arg | 第 0 個 stack arg |

### 可變參數函式 (varargs)

任何含 varargs（`...`）的函式必須使用 `__cdecl` — caller cleanup 是 ANSI C
varargs 的 ABI 強制要求。

## Stack-probe Helper (`__CHK` @ 0x36cd7)

Watcom CRT 在每個有 stack frame 的函式入口會插入一段 stack-overflow check 序列：

```
PUSH <framesize_imm>            ; 預留的 frame 大小
CALL 0x36cd7                    ; CRT 端 stack-check helper
```

`0x36cd7` 的內部邏輯（disasm 已驗證）：

```
00036cd7  XCHG dword ptr [ESP + 0x4], EAX   ; 把 framesize 換到 EAX，原 EAX 保到 stack
00036cdb  CALL 0x36cea                       ; 真正的 stack-check routine
00036ce0  MOV EAX, dword ptr [ESP + 0x4]    ; 還原 EAX
00036ce4  RET 0x4                            ; pop 那個 framesize arg
```

`0x36cea` 內部用 `CMP AX, SS` + 與 stack-limit global 比較，溢出時 trap。
這個 helper 是 Watcom CRT 的 stack-overflow probe，public symbol `__CHK`（內部
inner check 為 `__STK` @ 0x36cea，saved SS global `data_crt_stk_check_saved_ss` @ 0x52794，
真正的 stack-limit global 為 `data_crt_stk_check_stack_limit` @ 0x52814）。

probe 完成後 callee 才接 callee-saved register push (`PUSH EBX/ESI/EDI/EBP`) 與
local-variable allocation (`SUB ESP, N`)。**辨識 prologue 時要跳過這 2 條 stack
probe 指令** — 它們不是 cc 訊號。

CALL 會 clobber EAX/EDX/ECX/EBX (caller-saved + watcall reg arg 全在內)，所以
prologue 偵測「callee 在 frame setup 前讀取 EAX/EDX/EBX/ECX」必須把每個 CALL
視為對這 4 個 register 的 implicit write。

## ABI 判斷規則 (disasm signal-based)

| 情境 | cc 判定 |
| --- | --- |
| 末指令 `RET 0` (caller cleanup) + caller 一致做 `ADD ESP, K` | `__cdecl`，N stack args = K/4 |
| 末指令 `RET 0` + caller 不做 cleanup + caller 一致設 EAX/EDX/EBX/ECX | `__watcall`，K reg args |
| 末指令 `RET N` + callee prologue 讀 EAX/EDX/EBX/ECX | `__watcall`，含 reg + N/4 stack |
| 末指令 `RET N` + 無 reg evidence | `__stdcall`，N/4 stack args |
| 末指令 `TAIL_JMP` 直接到已知 function | 沿用 jump target 的 cc |
| Thunk | 沿用 thunked target 的 cc |
| 名稱含 `printf|sprintf|scanf|format` 或 varargs 證據 | 強制 `__cdecl` 並設 VarArgs flag |

判斷信號**強度排序**：

1. **Caller 一致 `ADD ESP, K`** → `__cdecl` 最強訊號（caller cleanup 是 ABI 強約束）
2. **Callee 末指令 `RET N`** → callee-cleanup 訊號（`__stdcall` 或 `__watcall`）
3. **Caller pre-CALL 設定 EAX / EDX / EBX / ECX**（不是 push 出來再讀的）→ `__watcall` 訊號
4. **Callee prologue 在 stack-probe 後立刻讀 EAX/EDX/EBX/ECX**（且不是先被 CALL 汙染）→ `__watcall` 訊號（fallback）
5. **EBX 在 entry 被當輸入讀（沒有先 PUSH EBX 保留）** → 強烈 `__watcall` 信號
   （cdecl/stdcall callee 視 EBX 為 callee-saved，一定先 `PUSH EBX` 再讀；
   只有 watcall 把 EBX 當第 3 個 reg arg 直接讀）

### Watcall 的特例：register count 由 caller 集合決定

Watcom 的 `__watcall` 允許函式只用前 K 個 register（K = 0..4）作 reg arg，
其餘走 stack。判定 K 時要看所有 caller 對 EAX/EDX/EBX/ECX 的設定一致性：

- 若所有 caller 都設 EAX 但不設 EDX → K = 1
- 若所有 caller 都設 EAX, EDX, EBX 但不設 ECX → K = 3
- 若 caller 對某 register 有時設有時不設 → 該 register 不算 reg arg

## 已知 special-case function (pinned)

| 地址 | 名稱 | cc | 說明 |
| --- | --- | --- | --- |
| `0x36cd7` | `__CHK` | `__stdcall` | Watcom CRT stack-check helper；1 個 stack arg = framesize；`RET 4` |

`__CHK` 在 cc classifier 內 hardcode 為 `pinned: true`，避免 re-classify 被誤判。

(原列入的 `FUN_0004b502 @ 0x4b502` Watcom soft-FP 80-bit long double 加法
helper 已被 Ghidra 併入 `__int7 @ 0x49d98` 的 body 範圍 0x49d98..0x4cbcd 內，
不再是獨立 function entity。其行為仍存在於 __int7 內部 — emit 時隨整個
__int7 走 link_vendor_lib（Watcom CLIB3S `__int7.obj`），無須單獨 pin。)

## Parameter 數量規則 (caller-derived)

ABI 要求宣告的 param 數量與 caller 行為一致：

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

## Function-pointer dispatch table callees 的 0-arg signature

176 個 function 屬於 function-pointer table 的 dispatch callee（caller_count=0，
透過 `(*table[idx])()` 0-arg 呼叫），全部設 `void __cdecl func(void)`：

| 群組 | 函式數 | dispatch 表 |
| --- | --- | --- |
| `chapter_NN_post_action` | 17 | `data_fd2_chapter_post_action_handler_table[30]` |
| `chapter_NN_init` | 26 | per-chapter init 表 |
| `chapter_NN_end` | 30 | per-chapter end 表 |
| `chapter_event_handler_*` | 89 | `data_fd2_battle_ai_post_action_consequence_table @ 0x51B91` |
| `cast_*` | 13 | spell-cast helpers |
| 其他 | 1 | sample fix |

驗證：所有 176 個 function 的 audit 訊號都是 `reads_eax / reads_edx /
reads_ebx / reads_ecx = False`，dispatch site `(*table[idx])()` 也無 push 任
何 stack arg，故 callee 應為 0-arg signature。

## Decompiler fragments — 不可獨立宣告的「函式」

Ghidra 自動分析把某些 parent function 的 epilogue 或 prologue adapter 拆成
獨立 function。這些不是真實 callable entity，無法獨立編譯。caller 透過
TAIL JMP (`e9` rel32) 進入 fragment，Ghidra 把它顯示為 `UNCONDITIONAL_CALL`
是因為 JMP target 落在 function entry 上的 display quirk。

每個 fragment 在 plate comment 內標 `DECOMPILER FRAGMENT — DO NOT DECLARE
INDEPENDENTLY`，emit pipeline 必須跳過這些 address，把 logic 收回 parent。

### Epilogue cluster (parent stack cleanup 共用)

| 地址 | 內容 | 對應 parent locals + saved regs |
| --- | --- | --- |
| `0x114fb` | `set_runtime_char_evade` (1 logic + ADD ESP 0x10 + POP EDI/ESI/EBX + RET) | recalculate_combat_stats (locals=0x10) |
| `0x10b43` | ADD ESP 0x4 + ADD ESP 0x8 + POP EBP/EDI/ESI/EBX + RET | locals=0xC + 4 saved regs |
| `0x10c49` | ADD ESP 0x4 + POP EDI/ESI/EBX + RET | locals=0x4 + 3 saved regs |
| `0x11011` | ADD ESP 0x34 + POP EBP/EDI/ESI/EBX + RET | locals=0x34 + 4 saved regs |
| `0x11452` | ADD ESP 0x20 + POP EBP/EDI/ESI/EBX + RET | locals=0x20 + 4 saved regs |
| `0x13994` | ADD ESP 0x5C + POP EBP/EDI/ESI/EBX + RET | locals=0x5C + 4 saved regs |

Watcom C 對於有相同 frame layout 的多個函式會共用同一段 epilogue 來節省 code
size — emit pipeline 須把 logic 還原到各 parent。

### Prologue adapter (tail JMP thunk)

| 地址 | 內容 | 用途 |
| --- | --- | --- |
| `0x15983` | `MOV EAX, EDI` + `JMP 0x22bbe` | 把 caller 的 EDI 移到 EAX（標準 reg-arg slot），然後 tail call 進 `fd2_composite_battle_frame_zero @ 0x22bb7` 的 +7 alt-entry（跳過 `__CHK` prologue 部分） |

Emit pipeline 看到 plate comment 內 `DECOMPILER FRAGMENT — DO NOT DECLARE
INDEPENDENTLY` 字串自動跳過該位址，logic 收回 parent。

## 已建立的 helper 型別

- `long_double_80` (10 bytes) — Watcom C++ 80-bit extended precision long double
  的 struct 定義 (`dwMantissa_lo: uint32 @+0`，`dwMantissa_hi: uint32 @+4`，
  `wSign_exp: uint16 @+8`)。原列為 soft-FP CRT helpers (`FUN_0004b502` add-immediate /
  `FUN_0004b532` mantissa add / `FUN_0004b761` / `FUN_0004b936` mantissa shift /
  `FUN_0004c00a`..`0x4cb34` transcendentals) 的 5 個 helper 已全部被 Ghidra merge
  進 `__int7 @ 0x49d98` (body 0x49d98..0x4cbcd) 內，不再是獨立 function entity；
  仍保留 `long_double_80` 型別供 emit 階段在 __int7 內部使用。

## 寫程式碼時的速查

要復刻 FD2 function 的 C 簽名時：

1. **看 callee 末指令**：
   - `RET 0` → 候選 cdecl 或 watcall(0 stack arg)
   - `RET N` → 候選 stdcall 或 watcall(N/4 stack args + 可能 reg)
2. **看 caller 端**：
   - CALL 前用 `MOV EAX/EDX/EBX/ECX` 設定 register（不是先 PUSH 再 POP 的）→ watcall，前 K 個 arg 是這些 register
   - CALL 後 `ADD ESP, K` → caller cleanup，K/4 = stack args
3. **看 callee 入口**：跳過 `PUSH framesize; CALL 0x36cd7` 的 stack probe，再看
   callee 是否在第一個 PUSH/SUB ESP 之前讀取 EAX/EDX/EBX/ECX（注意排除 CALL 的
   回傳值汙染）。**EBX 被讀但沒 PUSH EBX 保留** → 強烈 watcall 信號
4. **C++ member function**：用 watcall + EAX = `this`（Watcom 把 `this` 當第一
   個 reg arg）；或宣告為 free function 加顯式 `void *this` 第一參數
5. **varargs (`...`)**：強制 `__cdecl`，禁用 watcall（callee 無法知道 stack arg
   數量無法 cleanup）

## cc 分布（觀察結論）

FD2 編譯時整體偏向 `-3s`（stack-based）—— ~95% function 是 `__cdecl`。
`__watcall` 集中出現於 Watcom CRT soft-FP / long-double family
（~21 個位於 `0x4b400-0x4dfff` 附近）、AIL internal `*_inner` helper、
以及少數手寫 register-passing helper。`__stdcall` 全 binary 只有 1 個
（`__CHK @ 0x36cd7`）。C++ `__thiscall` 不採用（FD2 是純 C）。

實時 cc 分布 grep `call_graph.json` 或直接從 Ghidra 拉，不固定數字以
免 stale。
