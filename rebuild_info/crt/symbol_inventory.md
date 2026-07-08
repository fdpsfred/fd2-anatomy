# CRT 符號 inventory

FD2.LE 內 Watcom 9.5a C runtime 的命名約定，以及兩組必須在 `src/` 內以 C
重寫（不從 vendor lib 連結）的 CRT 角色函式的具名清單：`crt_equivalent_*`
與 CRT-style 的 `fd2_*` primitive。完整的 Watcom 真符號 inventory（address ↔
lib symbol）在同層 `lookup_9.5a.json`，人類可讀版見 `matched_function_sources.md`。
四 pool 分類與 emit routing 的機制見 `rebuild_info/equivalence/pool_classification.md`。

## CRT 命名約定

`src/` 直接連結 Watcom 9.5a CLIB3S（與原 binary 同版），因此命名以「能被
Watcom linker 直接解析」為目標，分三類：

- **Watcom 真符號** — 行為 byte-match Watcom 9.5/9.5a/9.5b/9.5c 的 lib obj，或
  語意 1:1 對應。一律沿用 Watcom 原名（`malloc` / `memset` / `fopen` /
  `_nmalloc` / `__filbuf` / `IF@COS` / `__CHK` 等）。lib 端匿名 static（`L$1`）
  合成為 `L$N_<obj>_<purpose>`（如 `L$1_stk_save_ss` / `L$1_sprintf_put_char` /
  `L$1_ioexit_close_streams_with_mask`）。這一整組由 `lookup_9.5a.json` 維護，
  re-emit 後 Watcom 直接連結同名函式（emit_class = link_vendor_lib）。
- **`crt_equivalent_*`** — 行為等價 Watcom CRT 但 byte 不 match 任一 lib obj
  版本，wlink 無法用 lib 解析，因此在 `src/` 端以 behaviour-equivalent 的 C
  函式重寫（全部落在 `src/crt/crt.c`）。
- **`fd2_*` 中的 CRT-style primitive** — FD2 工程師自寫的 CRT 風格 helper，
  主要為 Miles AIL callback 提供 DPMI / global accessor，同樣在 `src/` 端以 C
  重寫。

以上兩組「重寫」清單就是本檔的主體，逐一列於下。DOS LE 入口暨 cstart 啟動碼
`_cstart_ @ 0x3C964` 是與 Watcom 9.5a `CSTART3S.ASM` 逐指令相同的 stock vendor
code，走 lookup（由 `system dos4g` 連入 cstart.obj），不屬下列重寫清單。

## `crt_equivalent_*` 具名清單

Ghidra 以 `search_functions_enhanced(name_pattern="^crt_equivalent_", regex=true)`
即得完整清單，逐一如下（依 CRT 角色分組），全部在 `src/crt/crt.c` 以 C 重寫：

**EFLAGS / `_disable` primitive**：
- `crt_equivalent_get_eflags @ 0x3ED58` — `pushfd; pop eax; cli; ret`
  （Watcom `_disable`）。捕捉當前 EFLAGS 到 EAX 並清 IF，供 AIL ISR 進入
  critical section。
- `crt_equivalent_get_eflags_thunk @ 0x37F86` — FD2.LE 中此符號是 5-byte JMP
  進上者的 4-byte primitive。兩個 AIL ISR 以 near CALL 到達，取回先前 EFLAGS 後於離開
  critical section 時 PUSH+POPFD 還原。

**LX/LE module loader 三件組**（`crt_equivalent_lx_*`，見下節行為說明）：
- `crt_equivalent_lx_chunk_read @ 0x36107`
- `crt_equivalent_lx_header_reader @ 0x36344`
- `crt_equivalent_lx_module_loader @ 0x3647B`

**FPE / matherr / linker padding stub**：
- `crt_equivalent_atexit_default_stub @ 0x36DE3` — atexit chain 的預設 1-byte
  RET no-op；三個 atexit slot（0x527D8 / 0x527DC / 0x527E0）初值指向它，未註冊
  的 slot 落在此 RET 即無害返回。位址被取用（DATA ref），必須是真正可呼叫的
  函式，不能被摺除。
- `crt_equivalent_fpe_default_handler @ 0x3D26E` — SIGFPE / FPU 例外的預設
  1-byte RET no-op；FPE dispatch slot（0x5283C）初值指向它，未安裝
  signal(SIGFPE) 時的例外落在此 RET 即無害返回。位址被 DATA ref，須保留為真函式。
- `crt_equivalent_linker_padding_4cbce @ 0x4CBCE` — linker 留下的 padding，
  zero xref。
- `crt_equivalent_matherr_default_thunk @ 0x4D340` — matherr 使用者 handler slot
  （0x539A8）的預設值；此 slot 值在 FD2.LE 中是 5-byte `JMP 0x4D8EA`。`_matherr` 讀 slot 並 CALL
  它；回傳 0 代表「未處理」，`_matherr` 續走預設行為。位址被 DATA ref，須保留為
  真函式。
- `crt_equivalent_matherr_default_return_zero @ 0x4D8EA` — 上者 JMP 的目標，
  「return 0」primitive（`push ebp; mov ebp,esp; xor eax,eax; pop ebp; ret`）。

CRT XI 建構子表 `@ 0x539A0..0x539F1`（82 bytes，`_DATA` 最後一筆，見 `../link/le_layout.md`）——
`__InitRtns` / `__FiniRtns` 走訪的 startup constructor chain：**16-byte header**（`0x4CBCE`
sentinel dword ×2 + matherr handler slot `@0x539A8`（預設值 = 上述 `_matherr_default_thunk @ 0x4D340`）
+ control-flag dword `0x11`）＋ **10 個 6-byte ctor entry**（2-byte priority + 4-byte fn ptr）
＋ **6-byte NULL 終止**。10 個 fn ptr 指向 CRT 啟動期 init 常式（如 `__InitFiles @ 0x468F8` /
`__setenvp @ 0x4CBFD` / `__full_io_exit @ 0x4693D` / `__Init_Argv @ 0x46114`，以及 sys_init/fini_387
emulator 的 jmp thunk `@0x3CBCC` / `@0x3CBD1`），完整 10 筆依位址對回 `matched_function_sources.md`。
emit_action = link_vendor_lib（各 entry 的 fn 由其 `.obj` 經 wlink XI 段 merge，不 emit C source）。

上述兩個 thunk（`_get_eflags_thunk` / `_matherr_default_thunk`）在原 binary 是
tail-JMP 進另一個獨立符號，其相對位移無法用純 `#pragma aux` 位元組編碼；`src/`
以只被呼叫、不被取址的 in-line `#pragma aux` helper 承載原始 opcode / JMP，再由
真正 out-of-line 的 wrapper 提供 PUBDEF。這屬 Layer 2（功能等價）——byte-exact
去重的 JMP-to-shared-target 結構是 Layer 3 細節，本專案不追求。細節見
`src/crt/crt.c` 每個函式的註解。

## crt.c LX module loader 三件組行為說明

FD2 內含一組 LX/LE 可執行檔載入常式，行為像 CRT 的 overlay/dynamic-load loader，
但不 byte-match 任何 CLIB3S obj，故以 C 重寫於 `src/crt/crt.c`。三件組在 FD2 內
**實際是死碼**：`crt_equivalent_lx_module_loader` 為 0 caller，`_lx_header_reader`
的唯一 caller 是 `_lx_module_loader`，`_lx_chunk_read` 只被前兩者於自身 body 內
呼叫（Ghidra 上的高 xref 全是這些內部呼叫）。載入器對 store 的抽象由 mode/flags
的 bit0 決定：bit0=0 走檔案（open + lseek + read），bit0=1 則把傳入的 handle 當
記憶體基底、每次讀取變成 memcpy。

- **`crt_equivalent_lx_chunk_read @ 0x36107`** — 底層 chunk 讀取器，依 mode bit0
  在「檔案讀」與「記憶體 memcpy」之間分派，回傳 `offset+length`（結束位置）讓
  呼叫者鏈接連續讀取。`__cdecl`，5 個引數，呼叫端 `ADD ESP,0x14` 清棧。
- **`crt_equivalent_lx_header_reader @ 0x36344`** — LX 表頭巡覽：讀 MZ+0x3C 的
  `e_lfanew`、比對 2-byte LX magic（`strcmp` 對 "LX"）、讀入 0xAC 表頭，再逐一
  讀 object table 每筆 0x18-byte 記錄累加其 virtual size。回傳
  `number_of_objects*15 + Σ(virtual_size)`，是 FD2 特有的聚合值，非標準 loader
  API 結果。magic 不符時無條件 `close` 並回 0。
- **`crt_equivalent_lx_module_loader @ 0x3647B`** — 完整 LX/LE 載入器：解析
  object table + page table + fixup 記錄，把載入影像寫進呼叫者提供或（flags bit2）
  經 AIL alloc fnptr（0x52758）配置的緩衝區。逐 page 讀
  `min(remaining_obj_size, page_byte_count)` 位元組到輸出游標，對 flag-3 物件的
  首 page 做 16-byte 物件間對齊跳過；逐筆 fixup 驗證 src/target 型別 bit（非法即
  中止、close、回 0）並套用 32-bit 重定位（`obj_base + target_disp`）。回傳輸出
  緩衝區，任一失敗（open/alloc 失敗、magic 不符、壞 fixup）回 0。

## `fd2_*` CRT-style primitive 具名清單

Ghidra 以 `search_functions_enhanced(name_pattern="^fd2_(dpmi_|ail_set_)")`
即得完整清單，逐一如下：

**DPMI region/size primitive**（`src/util/dpmi.c`）— 為 Miles AIL callback 提供
DPMI INT 31h fn 0x100/0x101/0x600/0x601 的 wrapper：
- `fd2_dpmi_alloc_dos_memory @ 0x361CC`
- `fd2_dpmi_free_dos_memory @ 0x36255`
- `fd2_dpmi_lock_region @ 0x36284`
- `fd2_dpmi_unlock_region @ 0x362F1`
- `fd2_dpmi_lock_size @ 0x36316`
- `fd2_dpmi_unlock_size @ 0x3632D`

**AIL alloc/free fnptr setter**（`src/util/misc.c`）— 寫入 AIL 記憶體配置
callback 的 global fnptr：
- `fd2_ail_set_alloc_fnptr @ 0x3615E`
- `fd2_ail_set_free_fnptr @ 0x3616E`

## fptan worker 歸屬（正典）

`crt_emu387_int7_fptan_opcode_worker_4c630 @ 0x4C630`（503 bytes）是 x87 `FPTAN`
opcode 的軟體模擬 worker，位在 `__int7`（EMU387 software-FPU emulator，`emu387.obj`）
已 byte-match 的 PUBDEF body（0x49D98..0x4CBCD）內部。它無獨立 PUBDEF，是 `__int7`
的內部 subroutine（Ghidra 只是額外把它 carve 成一個 Function entity），靠連結同一
`__int7` module 解析（emit_class = link_vendor_lib），**不 emit C source，也不屬
`crt_equivalent_*`**。它與走硬體 FPTAN 的 trig387 public entry `IF@TAN @ 0x3C8AB`
無關（無硬體 387 時 FPTAN 觸發 INT 7 → `__int7` opcode dispatch）。routing 由 build
call-graph 工具的 EMU387 internal-subroutine 清單導向 link_vendor_lib（重生見
`tools/program_analysis/_index.md`），不進 emit worklist。

## Static-link duplicates

少數 helper 因 Watcom RTL 內多個 .obj 各帶一份而在 FD2.LE 出現多個 address。
Ghidra 以 name+address 為 unique key，接受同名 function，KB 不另加 suffix：

- `__delay @ 0x3DCCD`（71 callers，active）/ `fd2_delay_ms @ 0x375B2`（0 caller，
  dead）— Watcom CRT delay（DOS 21h tick wait）兩份 obj 各帶一份。
- `__exit @ 0x3CB91` / `__exit_with_msg @ 0x3CB93` — terminate wrapper 系列，
  連結 Watcom CRT 後對 binary 影響為 0。
- `crt_equivalent_get_eflags @ 0x3ED58`（0 caller）/
  `crt_equivalent_get_eflags_thunk @ 0x37F86`（2 callers）— `_disable` primitive
  加 thunk，兩份都行為等價，在 `src/` 端以 C 重寫。
