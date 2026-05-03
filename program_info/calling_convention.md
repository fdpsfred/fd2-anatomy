# Calling Convention (FD2.LE)

FD2.LE 是 Borland C++ 32-bit DOS LE executable，1000 個 function 全部 cc 與
parameter 數量已校正至 ABI 正確、可以重新編譯產生對等 binary 的狀態。本文是
ABI 結論與規則參考。

## Final cc 分佈

| cc | 數量 | 用途 |
|---|---|---|
| `__cdecl` | 869 | Borland 32-bit 預設；caller cleanup，args 從右到左 push 到 stack |
| `__fastcall` | 130 | 前 K 個 args 在 EAX/EDX/ECX (依序，K=1..3)，餘下 args on stack；callee cleanup |
| `__stdcall` | 1 | 僅 `crt_frame_setup`；全 stack args，callee cleanup |

`__thiscall` 不使用 — Borland 32-bit thiscall 等價於 `__fastcall` + 顯式 `this`
為第一參數，故 C++ member-like function 全部宣告為 free function 加 explicit
`this` 配 `__fastcall`。可用 cc 縮減為三種。

## Borland 32-bit ABI 細節

每個 Borland-compiled function 開頭都有 stack probe 序列：

```
PUSH <framesize_imm>            ; 預留的 frame 大小
CALL 0x36cd7  (crt_frame_setup) ; runtime 端 stack-check helper
```

`crt_frame_setup` 的內部邏輯：

```
00036cd7  XCHG dword ptr [ESP + 0x4], EAX   ; 把 framesize 換到 EAX
00036cdb  CALL 0x36cea                       ; 真正的 stack-probe routine
00036ce0  MOV EAX, dword ptr [ESP + 0x4]    ; 還原 EAX
00036ce4  RET 0x4                            ; pop 那個 framesize arg
```

probe 完成後 callee 才接 callee-saved register push (`PUSH EBX/ESI/EDI/EBP`) 與
local-variable allocation (`SUB ESP, N`)。**辨識 prologue 時要跳過這 2 條 stack
probe 指令** — 它們不是 cc 訊號。

CALL 會 clobber EAX/EDX/ECX (caller-saved + return value register)，所以 prologue
偵測「callee 在 frame setup 前讀取 EAX/EDX/ECX」必須把每個 CALL 視為對這 3 個
register 的 implicit write。

## ABI 判斷規則 (caller-side signal-based)

| 情境 | cc 判定 |
|---|---|
| 末指令 `RET 0` (caller cleanup) + caller 一致做 `ADD ESP, K` | `__cdecl`，N args = K/4 |
| 末指令 `RET 0` + caller 不做 cleanup + caller 一致設 EAX/EDX/ECX | `__fastcall`，K reg args |
| 末指令 `RET N` + callee prologue 讀 EAX/EDX/ECX | `__fastcall`，含 reg + N/4 stack |
| 末指令 `RET N` + 無 reg evidence | `__stdcall`，N/4 stack args |
| 末指令 `TAIL_JMP` 直接到已知 function | 沿用 jump target 的 cc |
| 末指令 `OTHER` (fall-through) / `NONE` (空 body) | `__cdecl` (default，cc 對非真正 return 的 function 不影響 codegen) |
| Thunk | 沿用 thunked target 的 cc |
| 名稱含 `printf|sprintf|scanf|format` 或 varargs 證據 | 強制 `__cdecl` 並設 VarArgs flag |

判斷信號**強度排序**：

1. Caller 一致 `ADD ESP, K` → `__cdecl` 最強訊號 (caller cleanup 是 ABI 強約束)
2. Callee 末指令 `RET N` → callee-cleanup 訊號 (`__stdcall` 或 `__fastcall`)
3. Caller pre-CALL 設定 EAX/EDX/ECX → `__fastcall` 訊號
4. Callee prologue 讀 EAX/EDX/ECX (在 frame setup 之前) → `__fastcall` 訊號 (僅做 fallback，會被 CALL clobber 干擾)

## 真實 callee-cleanup function (pinned)

整個 binary 只有 2 個 function 是真正 callee-cleanup (RET N)：

| 地址 | 名稱 | cc | 說明 |
|---|---|---|---|
| `0x36cd7` | `crt_frame_setup` | `__stdcall` | Borland CRT 的 stack-probe helper，每個 function 都會呼叫；1 個 stack arg = framesize |
| `0x4b502` | `FUN_0004b502` | `__fastcall` | Borland soft-FP: 80-bit long double in-place add of immediate constant (`*operand_a += B_imm`)。EAX = `long_double_80 *operand_a_inout`；3 stack args = B 的 `dwMantissa_lo / dwMantissa_hi / wSign_exp`；caller 須設 EBX = 同 EAX 作 result_ptr (內部 PUSH/POP 暫存，CALL 期間 EBX 被 overwrite 為 B mantissa_lo)。`RET 0xc`. 完整 register-level ABI 見函式 plate comment。 |

兩者在 cc classifier (`tools/calling_convention_audit/classify.py`) 內
hardcode 為 `pinned: true`，避免 re-classify 被誤判。

## Parameter 數量規則 (caller-derived)

ABI 要求宣告的 param 數量與 caller 行為一致：

```
__cdecl    : N_params = max(callers_add_esp_K) / 4
             若無 caller cleanup 訊號 → N_params = 0
__stdcall  : N_params = RET_N / 4
__fastcall : N_reg   = max consecutive prefix of (EAX, EDX, ECX) that ALL callers agree on
             N_stack = RET_N / 4
             N_params = N_reg + N_stack
```

宣告 param 數**少於**實際 ABI → callee 從 stack 讀垃圾，crash。  
宣告 param 數**多於**實際 ABI → caller 多 push 不會被 callee 用到的 args，浪費但
不破。**少報比多報危險。**

校正後 469 個 function 的 param 數量已對齊 caller 訊號 (1220 phantom params 移除
+ 60 個遺漏 param 補齊)。277 個無 caller / mixed-signal function 保留 Ghidra 預設
數量 (見 `open_issues.md`)。

## 已校正狀態快速表

| 項目 | 數值 |
|---|---|
| Function 總數 | 1000 |
| 套用後 cc 分佈 | `__cdecl` 869 / `__fastcall` 130 / `__stdcall` 1 |
| 校正前 cc 分佈 (Ghidra auto-analysis) | `__cdecl` 242 / `__fastcall` 659 / `__stdcall` 99 |
| cc 校正套用 | 722 |
| Function 重命名 (cc 名稱不一致) | 1 (`wrapper_clear_keyboard_buffer_stdcall` → `wrapper_clear_keyboard_buffer`) |
| Param 名稱清理 (`arg_eax_in/edx_in/ecx_in` → `param_N`) | 1684 個 / 565 functions |
| Param 數量校正 | 469 functions (1220 removes + 60 adds) |

## 工具 (tools/calling_convention_audit/)

完整 pipeline 的 scripts 與用法見 `tools/calling_convention_audit/_index.md`：

| 角色 | Script | 說明 |
|---|---|---|
| dump | `ghidra_dump.java` | 對 1000 個 function dump per-function ABI 證據 |
| classify | `classify.py` | 用上述規則決定每個 function 的推薦 cc |
| apply | `apply_batch.py` + `ghidra_apply.java` | 跨 session 批次套用 cc/rename |
| verify | `verify.py` | 重 dump + diff + bad-instr check |
| param-name cleanup | `ghidra_param_cleanup.java` | `arg_*_in` → `param_N` |
| param-count apply | `param_count_classify.py` + `ghidra_param_count_apply.java` | 用 caller 訊號推論真實 param 數量並套用 |

dump / classify / apply / verify 是 cc 校正主線 (跨 session 可恢復，
progress.json checkpoint)；param-name cleanup 與 param-count apply 是 cc
修正後的 cleanup pass，one-shot single-transaction。

## Function-pointer dispatch table callees 的 0-arg signature

176 個 function 屬於 function-pointer table 的 dispatch callee（caller_count=0，
透過 `(*table[idx])()` 0-arg 呼叫），全部設 `void __cdecl func(void)`：

| 群組 | 函式數 | dispatch 表 |
|---|---|---|
| `chapter_NN_post_action` | 17 | `per_chapter_post_action_handler[30]` |
| `chapter_NN_init` | 26 | per-chapter init 表 |
| `chapter_NN_end` | 30 | per-chapter end 表 |
| `chapter_event_handler_*` | 89 | `ai_post_action_consequence_table @ 0x51B91` |
| `cast_*` | 13 | spell-cast helpers |
| 其他 | 1 | sample fix |

驗證：所有 176 個 function 的 audit 訊號都是 `reads_eax / reads_edx / reads_ecx
= False`，dispatch site `(*table[idx])()` 也無 push 任何 stack arg，故 callee
應為 0-arg signature。

LOW-confidence 102 個 function (caller signal 不確定，auto param-count
classifier 跳過；audit 重新清點數，原估計 ~101) 已全部逐一 disasm + 必要時
caller call site cross-check 後處理：47 個 apply 新 prototype、55 個 ratify
(Ghidra cc-correction 後計數已正確或 Borland CRT 自訂 ABI)。

常見模式：

- **Borland stack-probe prologue 函式** (有 `PUSH framesize_imm; CALL 0x36cd7
  (crt_frame_setup)`)：3 個 phantom reg slot 來自舊 fastcall 預設，body 不讀
  EAX/EDX/ECX 作 input；strip 3 phantom + 保留 N cdecl stack args。包含
  `spell_handler_id_*` × 11 (3 args: caster_unit_id, num_targets, target_id_array)、
  `execute_*` × 7 (各 2-7 args)、`tick_summon` family × 7 (5 args)、章節共用 init
  / handler 與 tick_chapter_palette_animation / tick_tile_event_animations 等
  0-arg cdecl × 多筆。
- **標準 EBP prologue 函式** (有 `PUSH EBP; MOV EBP, ESP`)：N cdecl stack args
  讀自 `[EBP+8/c/10/...]`，無 phantom reg；Ghidra cc-correction 已正確處理，
  只需 ratify (例：`memcpy` @ 0x3cbd6、`itoa` @ 0x46bba 等)。
- **Borland CRT soft-FP / long-double family** (`FUN_0004b761` divide、
  `FUN_0004cb34` mantissa add、`FUN_0004cb86`、`FUN_0004d53c`)：custom ABI
  (EBX/ESI/EDI 也帶輸入)，標準 `__fastcall` 無法精確建模。defer 到 build
  pipeline 站起來再 byte-level 比對驗證。

工具：`tools/lowconf_signature/` (`inventory.py` 抽 LOW set + signal、
`plan_apply.py` 規則化 plan 產出後**未**直接套用，per-function disasm 驗證
後逐一 apply)。

## Decompiler fragments — 不可獨立宣告的「函式」

Ghidra 自動分析把某些 parent function 的 epilogue 或 prologue adapter 拆成
獨立 function。這些不是真實 callable entity，無法獨立編譯。caller 透過
TAIL JMP (`e9` rel32) 進入 fragment，Ghidra 把它顯示為 `UNCONDITIONAL_CALL`
是因為 JMP target 落在 function entry 上的 display quirk。

每個 fragment 在 plate comment 內標 `DECOMPILER FRAGMENT — DO NOT DECLARE
INDEPENDENTLY`，emit pipeline 必須跳過這些 address，把 logic 收回 parent。

### Epilogue cluster (parent stack cleanup 共用)

| 地址 | 內容 | 對應 parent locals + saved regs |
|---|---|---|
| `0x114fb` | `set_runtime_char_evade` (1 logic + ADD ESP 0x10 + POP EDI/ESI/EBX + RET) | recalculate_combat_stats (locals=0x10) |
| `0x10b43` | ADD ESP 0x4 + ADD ESP 0x8 + POP EBP/EDI/ESI/EBX + RET | locals=0xC + 4 saved regs |
| `0x10c49` | ADD ESP 0x4 + POP EDI/ESI/EBX + RET | locals=0x4 + 3 saved regs |
| `0x11011` | ADD ESP 0x34 + POP EBP/EDI/ESI/EBX + RET | locals=0x34 + 4 saved regs |
| `0x11452` | ADD ESP 0x20 + POP EBP/EDI/ESI/EBX + RET | locals=0x20 + 4 saved regs |
| `0x13994` | ADD ESP 0x5C + POP EBP/EDI/ESI/EBX + RET | locals=0x5C + 4 saved regs |

Borland C++ 用這個共用 epilogue 機制節省 code size — 多個 stack frame layout
相同的 parent 共用同一段 epilogue。

### Prologue adapter (tail JMP thunk)

| 地址 | 內容 | 用途 |
|---|---|---|
| `0x15983` | `MOV EAX, EDI` + `JMP 0x22bbe` | 把 caller 的 EDI 移到 EAX（標準 reg-arg slot），然後 tail call 真實實作 FUN_00022bbe |

Emit pipeline 看到 plate comment 內 `DECOMPILER FRAGMENT — DO NOT DECLARE
INDEPENDENTLY` 字串自動跳過該位址，logic 收回 parent。

## 已建立的 helper 型別

- `long_double_80` (10 bytes) — Borland C++ 80-bit extended precision long double
  的 struct 定義 (`dwMantissa_lo: uint32 @+0`，`dwMantissa_hi: uint32 @+4`，
  `wSign_exp: uint16 @+8`)。soft-FP CRT helpers (`FUN_0004b502` add-immediate /
  `FUN_0004b532` mantissa add / `FUN_0004b761` / `FUN_0004b936` mantissa shift /
  `FUN_0004c00a`..`0x4cb34` transcendentals) 都對這個型別操作。

## 寫程式碼時的速查

要復刻 FD2 function 的 C 簽名時：

1. **預設 `__cdecl`** — 87% 的 function 是這個。除非有以下訊號：
2. 看 caller assembly：caller 在 CALL 前用 `MOV EAX, ...` / `MOV EDX, ...` /
   `MOV ECX, ...` 設 register → `__fastcall`，前 K 個 arg 是這些 register
3. 看 callee 末指令：`RET N` (而非 `RET`) → callee cleanup → `__fastcall` 或
   `__stdcall`
4. 看 prologue：跳過 `PUSH framesize; CALL 0x36cd7` 的 stack probe，再看 callee
   是否在第一個 PUSH/SUB ESP 之前讀取 EAX/EDX/ECX (注意排除 CALL 的回傳值汙染)
5. C++ member function 不要用 `__thiscall` — 一律宣告為 free function 加
   `void *this` (或 struct pointer) 第一參數，cc 用 `__fastcall`
