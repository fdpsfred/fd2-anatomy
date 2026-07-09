# pool 分類

FD2.LE 內每個 function 都歸入四個 pool 之一：`ail` / `crt` / `fd2` /
`binary_artifact`。分類由 `tools/program_analysis/build_call_graph.py` 的
`categorise()` 依「命名前綴 + CRT lookup 命中 + `PUBLIC_CRT_SYMBOLS` 集合」決定。

判別**不能用 address range**：四個 pool 的 function 在 `.object1` 內互相交錯，
CRT 與 game code 並非各佔連續區段（跨類別 interleave 的成因見
`../link/le_layout.md`）。唯一可靠的判別依據是命名前綴，輔以 caller/callee 與
byte 內容。各 pool 的即時數量一律用 Ghidra 依前綴 dump（見下表指令），本檔不固定
數字。

## 四個 pool

| pool | 命名前綴 | 內容 | 即時 count |
|---|---|---|---|
| `ail` | `AIL_*` | Miles AIL3DIG / AIL3MDI 音訊 static lib 的 mixer / sequencer / driver wrapper / ISR helper / DPMI thunk | `search_functions_enhanced(name_pattern="^AIL_", regex=true)` 的 `total` |
| `crt` | `crt_equivalent_*` / `L$*` / Watcom 真符號 / `PUBLIC_CRT_SYMBOLS` | Watcom 9.5a C runtime（CLIB3S + MATH387S + EMU387 + GRAPH） | 全體扣除其他三個 pool |
| `fd2` | `fd2_*`（外加 `main`） | FD2 工程師自寫的 game logic / glue / dispatch / wrapper / dead code | `search_functions_enhanced(name_pattern="^fd2_", regex=true)` 的 `total`，再加 `main` |
| `binary_artifact` | `binary_artifact_*` | Watcom compiler 在 function 之間插入的 alignment NOP padding，0 caller、永不執行 | `search_functions_enhanced(name_pattern="^binary_artifact_", regex=true)` 的 `total` |

## ail pool

Miles AIL 音訊函式庫（AIL3DIG digital / AIL3MDI MIDI）在 FD2.LE 內的全部
function，涵蓋 mixer、sequencer、driver wrapper、ISR helper 與 DPMI thunk。
AIL 的 register clobber 契約見 `../ail/calling_convention.md`。

## crt pool

Watcom 9.5a C runtime（CLIB3S + MATH387S + EMU387 + GRAPH）在 FD2.LE 內的全部
function。命名分三類：

- **Watcom 真符號** — byte-match Watcom lib obj 或語意 1:1 對應者，使用 Watcom
  原名（`malloc` / `memset` / `fopen` / `_nmalloc` / `__filbuf` / `IF@COS` /
  `__CHK` / `L$1_*` 等），address ↔ symbol 對照存於 `../crt/lookup_9.5a.json`
  （human-readable view 為 `../crt/matched_function_sources.md`）。
- **`crt_equivalent_*`** — 行為等價於 Watcom CRT，但 byte 不 match 任一 lib obj
  版本，必須以 FD2 source 端 emit 一個等價 C function。
- **`PUBLIC_CRT_SYMBOLS`** — hard-coded 在 `build_call_graph.py` 的 Watcom 公開
  符號 fast-path，用於 Ghidra 已還原公開符號但未進 lookup 的情形。

`crt_equivalent_*` 具名清單、`fd2_*` 中屬 CRT-style primitive 那批 helper 的歸屬，以及
EMU387 `__int7` 內部 subroutine（含 x87 `FPTAN` opcode 的軟體模擬 worker）的
歸屬，一律以 `../crt/symbol_inventory.md` 為正典。

## fd2 pool

FD2 工程師自寫的 function：載檔 / 存檔、章節 init/end/post_action handler、
spell handler、戰鬥流程、AI 控制、地圖渲染、portrait / sprite blit、
FDFIELD / FDSHAP / FDOTHER / FDTXT 資源解碼、cursor / menu、chapter event
handler，以及一批 shared-epilogue / tail-JMP stub（`fd2_noop_stub_*`）、
一批 DPMI region/size primitive 與 FD2 global accessor（具名清單見 `../crt/symbol_inventory.md`）。C 進入點 `main` 依
CRT 契約保留原名，歸此 pool。`fd2_*` 中屬 CRT-style primitive 的那批 helper
清單見 `../crt/symbol_inventory.md`。

## Entry / startup / exit

DOS LE entry + Watcom CRT startup + FD2 main 的 chain（`_cstart_ @ 0x3C964` →
`__CMain` → `main`）見 `program_info/overview.md`。pool 歸屬上，`_cstart_` 與
`__CMain` 屬 crt pool（stock Watcom vendor code，link vendor lib 解析），`main`
屬 fd2 pool。

## binary_artifact pool（NOP / padding 事實）

`binary_artifact_*` 是 Watcom 工具鏈在 `_TEXT` segment 內插入的 padding，被 Ghidra
建為 Function entity 但 0 caller、永不執行。它們在 Watcom 9.5a 重新 compile + wlink
重新 link 時自動重生，不需在 source 端寫。

### Compiler alignment NOP（`binary_artifact_align_nop_<addr>`）

Watcom 9.5a compiler 為了讓 hot function 的 entry 對齊 16-byte 邊界，在 function
之間插入多位元組 NOP 指令當 padding。常見 encoding：

- `8d 80 00 00 00 00` = `LEA EAX,[EAX]`（6-byte NOP）
- `8d 40 00` = `LEA EAX,[EAX+0]`（3-byte NOP）
- `8d 92 00 00 00 00` = `LEA EDX,[EDX+0x00000000]`（6-byte NOP）
- `8b c0` = `MOV EAX,EAX`（2-byte NOP）
- `8b c9` = `MOV ECX,ECX`（2-byte NOP）
- `8b d2` = `MOV EDX,EDX`（2-byte NOP）
- `8b db` = `MOV EBX,EBX`（2-byte NOP）
- `90` = `NOP`（1-byte）

這些位置都緊貼下一個 function 的 entry，且自身 0 caller。建為獨立 Function entity
是為了滿足「`.object1` 每個 instruction byte 都歸屬於一個 function 或 align_fill」
這個不變式。`categorise()` 用名稱前綴 `binary_artifact_` 直接把它們歸 binary_artifact
pool。

### Wlink segment alignment fill（`data_align_<addr>`，非 Function entity）

`wlink` 把不同 obj 的 `_TEXT` segment 接在一起時做 segment 邊界對齊（通常 16-byte），
用 default fill byte `0x00` 補齊。這與 compiler-emit 的 NOP 來源不同，兩者對照：

| 屬性 | `binary_artifact_align_nop_*`（compiler） | `data_align_*`（linker） |
|---|---|---|
| 來源 | Watcom 編譯器在 obj 內 function 之間插入 | wlink 把 obj 拼起來時補 segment 邊界 |
| 內容 | 有效的 NOP 指令（`90` / `89 c0` / `8d 40 00` …） | 純 `0x00` byte |
| 可 disassemble | 是（decoded as NOP） | 否（`00` 與下個 instruction 互相 overlap） |
| Ghidra 表示 | Function entity | Data byte + label（非 Function entity） |
| 四 pool 歸屬 | `binary_artifact` | 不在 four-pool 範圍 |

`data_align_*` label 用 `list_globals(name_substring="data_align_")` 即時列出，分布於
`.object1`（0x36ccf..0x4db62）與 `.object2`（0x523b6..0x54157）兩段。此 label 集
內容混雜：wlink segment 邊界 `0x00` fill、inter-function zero pad、`__int7` 內部
NOP pad、`.object2` data alignment 都用同一命名。它們都不屬 four-pool 範圍。

### DPMI INT vector dispatch table（非獨立 function）

Watcom CRT 在 protected mode 下用 `_DoINTR_ @ 0x4657B`（893B body）切回 real mode
觸發 `INT N`。body 內部含 256 個 3-byte `INT NN; RET` stub，但這 256 段 byte 都是
`_DoINTR_` body 的內部位元組，**不是**獨立 Function entity（`search_functions("crt_dpmi_int")`
回 0）。`_DoINTR_` 本身透過 byte-match audit 歸 lookup 真名（`../crt/lookup_9.5a.json`），
以 vendor lib 解析，不需在 source 端另寫這 256 個 stub。
