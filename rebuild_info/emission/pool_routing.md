# pool routing

FD2.LE 全 1375 個 function 依 **四 pool + 兩維度** 分類（命名規範強制由
`tools/program_analysis/build_call_graph.py` 的 `categorise()` /
`emit_action_for()` 推導）：

| pool (category) | 命名前綴 | 數量 | emit_action |
|---|---|---|---|
| `ail` | `AIL_*` | (即時 dump) | `link_vendor_lib` |
| `crt` | `crt_equivalent_*` / `crt_*` / `L$*` / Watcom natural name (`cos`/`fopen`/...) / lookup-resolved / `PUBLIC_CRT_SYMBOLS` | 214 | `link_vendor_lib`（lookup-resolved + PUBLIC_CRT_SYMBOLS）或 `emit_fd2_source`（`crt_equivalent_*`） |
| `fd2` | `fd2_*` | 640 | `emit_fd2_source` |
| `binary_artifact` | `binary_artifact_*` | 93 | `skip_artifact` |
| **小計** |  | **1375** |  |

emit_action 對應 wlink / Watcom 9.5a recompile pipeline 的處理：

- `link_vendor_lib` — wlink 從 Miles AIL static lib 或 Watcom 9.5a CLIB3S
  直接解析（ail 全部 + crt 內 lookup-resolved + PUBLIC_CRT_SYMBOLS），
  FD2 source 端只保留 `extern` declaration
- `emit_fd2_source` — FD2 source 端 emit C function。涵蓋全部
  `fd2_*` + 12 個 `crt_equivalent_*`
- `skip_artifact` (93) — Watcom 9.5a 重 compile 自動生成 alignment padding，
  FD2 source 不需要寫

四 pool 的具體分布：

1. **AIL pool** (`AIL_*`，count 即時透過 `search_functions("^AIL_")` dump) — Miles AIL3DIG / AIL3MDI audio static lib
   的 mixer / sequencer / driver wrapper / ISR helper / DPMI thunk。全部走
   `link_vendor_lib`，FD2 source 不需重寫。
2. **CRT pool** (`crt_equivalent_*` / `L$*` / Watcom natural name / lookup / PUBLIC_CRT_SYMBOLS,
   214 個) — Watcom 9.5a C runtime (CLIB3S + MATH387S + EMU387 + GRAPH)：
   - 193 個 lookup-resolved Watcom 真符號（`malloc` / `free` / `fread` /
     `fwrite` / `memset` / `memmove` / `strcpy` 等公開符號 + Watcom near-pointer
     內部 `_nmalloc` / `_nfree` + Watcom hidden `__filbuf` / `__get_errno_ptr` /
     `__sys_init/fini_387_emulator` / `_SetMaxPrec` / `_set_matherr` +
     MATH387S 系列 `IF@COS` / `IF@SIN` / `IF@TAN` / `IF@LOG` / `IF@LOG2` /
     `IF@LOG10` / `IF@SQRT` / `IF@DABS` / `__@DSQRT` 等 trig/log/sqrt/fabs
     public entry + thunks + CLIB3S seterrno trio `__set_EDOM` / `__set_ERANGE` /
     `__set_errno` + `__FPE_exception_` / `flushall` + Watcom 387 helper
     `__CHP` / `__GETDS` / `fabs`；本 binary 的 `fopen` 入口走 FD2 wrapper
     `fopen` lookup-resolved at 0x36FCC（FD2 hardcoded "rb" 用 wrapper），無另外的公開 wrapper 符號）
   - 56 個 `PUBLIC_CRT_SYMBOLS` (hard-coded set 在 `build_call_graph.py`，用於
     Ghidra 已還原 Watcom 公開符號但未進 lookup 的 fast-path：`malloc` / `free` /
     `fread` / `fwrite` / `sprintf` / `vfprintf` 等)
   - 12 個 `crt_equivalent_*` — 行為等價 Watcom CRT 但 byte 不 match 任一 lib obj：
     `entry_start` / `dos_main_bootstrap` (cstart pair)、`get_eflags` /
     `get_eflags_thunk` (`_disable` primitive)、LX module loader chain
     (`lx_header_reader_36344` / `lx_module_loader_3647b` / `lx_chunk_read_36107`)、
     Watcom CRT exit chain / FPE default /
     linker padding / matherr default 系列 stub
     (`exit_chain_stub_36de3` / `fpe_default_handler_3d26e` /
     `linker_padding_4cbce` / `matherr_default_thunk_4d340` /
     `matherr_default_return_zero_4d8ea`)
   - `crt_emu387_int7_fptan_opcode_worker_4c630 @ 0x4C630` — **不是** emit 目標。
     它是 `__int7`（EMU387 software-FPU emulator，`emu387.obj`）的內部 subroutine：
     bytes 落在已 byte-match 的 `__int7` PUBDEF body（0x49D98..0x4CBCD）內部，無獨立
     PUBDEF，靠 link 該單一 `__int7` module 解析（`link_vendor_lib`），不重 emit C source。
     原 Ghidra 把它 carve 成獨立 Function entity（analysis artifact，是 `__int7` 內唯一
     一個被 carve 出來的；其 6 個 sibling helper 與 8 個 inline x87-opcode jump table 都
     無 Function entity），曾被誤列為第 13 個 `crt_equivalent_*`。其角色是 x87 `FPTAN`
     opcode 的軟體模擬 worker（math_mode != 3、無硬體 387 時 FPTAN 觸發 INT 7，由 `__int7`
     opcode dispatch 進入；與走硬體 FPTAN 的 trig387 public entry `IF@TAN @ 0x3C8AB` 無關）。
     routing 由 `build_call_graph.py` 的 `EMU387_INTERNAL_SUBROUTINES` set 導向 link_vendor_lib。
   - 上述 lookup 真符號 + PUBLIC_CRT_SYMBOLS 涵蓋 Watcom CRT 提供的全部 FD2.LE
     使用的 RTL 函式（softfp / format / fopen / heap / dpmi / init / time /
     errno / signal / stream I/O / math 系列）。當前 Ghidra 內 `crt_*` 前綴有
     **12 個 `crt_equivalent_*`（emit_fd2_source）+ 1 個
     `crt_emu387_int7_fptan_opcode_worker_4c630`（link_vendor_lib，`__int7` 內部
     subroutine，非 emit 目標）**；其餘 CRT-style 函式都已歸 lookup 真名
     （以 Watcom 9.5a CRT 公開或 hidden symbol 命名）。
3. **FD2 pool** (`fd2_*`, 640 個) — FD2 工程師自寫的 game logic / glue / dispatch /
   wrapper / dead code。涵蓋：載 / 存檔、章節 init/end/post_action handler、
   spell handler、戰鬥流程、AI 控制、地圖渲染、portrait / sprite blit、
   FDFIELD / FDSHAP / FDOTHER / FDTXT 資源解碼、cursor / menu、chapter
   event handler、SHARED EPILOGUE / TAIL JMP THUNK stub (`fd2_noop_stub_*`)、
   6 個 DPMI region/size primitive (`fd2_dpmi_*`)、2 個 FD2 global accessor、`fd2_main`。
4. **binary_artifact pool** (`binary_artifact_*`, 93 個) — Watcom 9.5a compiler
   在 function 之間插入的多位元組 NOP padding (`LEA EAX,[EAX]` / `MOV EDX,EDX`
   等)，建為 Function entity 但 0 caller、永不執行。詳見下文「binary_artifact
   pool」段落。

## Entry / startup / exit

DOS LE entry + Watcom CRT startup + FD2 main 的 chain
（`crt_equivalent_entry_start @ 0x3C964` → `crt_equivalent_dos_main_bootstrap`
→ `__CMain` → `fd2_main`）見 `program_info/overview.md` §「執行流程」。
emit pipeline 只需知道前三個屬 crt pool，後者屬 fd2 pool。

## binary_artifact pool

兩種 Watcom 工具鏈在 `_TEXT` segment 內 emit 的 padding，emit_action 都是
`skip_artifact`（Watcom 9.5a 重 compile + wlink 重 link 自動產生，FD2 source
不需要寫）：

### Watcom compiler alignment NOP (93 個 `binary_artifact_align_nop_<addr>`，屬 binary_artifact pool)

Watcom 9.5a compiler 為了讓 hot function 的 entry 對齊到 16-byte 邊界，在
function 之間插入多位元組 NOP 指令當 padding。常見 encoding：

- `8d 80 00 00 00 00` = `LEA EAX,[EAX]` (6-byte NOP)
- `8d 40 00` = `LEA EAX,[EAX+0]` (3-byte NOP)
- `8d 92 00 00 00 00` = `LEA EDX,[EDX+0x00000000]` (6-byte NOP)
- `89 d2` = `MOV EDX,EDX` (2-byte NOP)
- `89 c0` = `MOV EAX,EAX` (2-byte NOP)
- `89 c9` = `MOV ECX,ECX` (2-byte NOP)
- `8b c0` = `MOV EAX,EAX` (2-byte NOP, alternate encoding)
- `90` = `NOP` (1-byte)

這些位置都緊貼下個 function 的 entry，且自身 0 caller。建為獨立 Function
entity 是為了滿足「`.object1` 每個 instruction byte 都歸屬於一個 function
或 align_fill」這個不變式。`categorise()` 用名稱前綴 `binary_artifact_`
直接把它們歸 binary_artifact pool。

### Wlink segment alignment fill (16 個 `data_align_<addr>`，非 Function entity)

`wlink` 連結器把不同 obj 的 `_TEXT` segment 接在一起時做 segment 邊界對齊
（通常 16-byte），用 default fill byte `0x00` 補齊。與 compiler-emit 的
NOP 來源不同：

| 屬性 | `binary_artifact_align_nop_*`（compiler）| `data_align_*`（linker）|
|---|---|---|
| 來源 | Watcom 編譯器在 obj 內 function 之間插入 | wlink 把 obj 拼起來時補 segment 邊界 |
| 內容 | 有效的 NOP 指令 (`90` / `89 c0` / `8d 40 00` ...) | 純 `0x00` byte |
| 可 disassemble | 是 (decoded as NOP) | 否 (`00` 與下個 instruction 互相 overlap) |
| Ghidra 表示 | Function entity | Data byte + label（非 Function entity）|
| 四 pool 歸屬 | `binary_artifact` | 不在 four-pool 範圍 |

`data_align_*` label 共 34 個（透過 `list_globals(filter="data_align_")` 列出），
分布於 `.object1`（0x36ccf..0x4db62）和 `.object2`（0x523b6..0x54157）兩段。
其中原始 16 個為 wlink segment 邊界 `0x00` fill，其餘為後續 audit 新增的
inter-function zero pad、__int7 內部 NOP pad、.object2 data alignment 等。

### DPMI INT vector dispatch table（非獨立 function）

Watcom CRT 在 protected mode 下用 `_DoINTR_ @ 0x4657B` (893B body) 切回
real mode 觸發 `INT N`。原以為 256 個 3-byte `INT NN; RET` stub 是 256 個
獨立 Function entity 的舊認知**已過時**：實際 256 個 stub byte 是
`_DoINTR_` body 內部位元組，並非獨立 function entity（Ghidra
`search_functions(name_pattern="crt_dpmi_int")` 回 0）。`_DoINTR_` 本身已透過
byte_match audit 在 `rebuild_info/crt/lookup_9.5a.json` 內歸 lookup name，走
`link_vendor_lib`。

## CRT 程式碼地理位置

Watcom CRT 函式分散在 `.object1` 各處（與 fd2 / ail / binary_artifact pool
**互相交錯**，不是連續區段）。CRT 集中區大致落在 `0x36000-0x37700`、
`0x3C000-0x40000`、`0x45D00-0x4E000`，但分類**不能用 address range 判定**。
判別方式由 `tools/program_analysis/build_call_graph.py` 的 `categorise()`
決定：name 前綴 (`AIL_*` / `binary_artifact_*` / `fd2_*` / `crt_*` /
`crt_equivalent_*` / `L$*`) 或落入 `rebuild_info/crt/lookup_9.5a.json` /
`PUBLIC_CRT_SYMBOLS`。
