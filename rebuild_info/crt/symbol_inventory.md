# CRT 符號 inventory

Watcom v2 C runtime 在 FD2.LE 內的命名約定、static-link duplicates、13 個
`crt_equivalent_*` 與 8 個 `fd2_*` CRT-style primitive 對照。完整 Watcom
真符號 inventory 見同層 `lookup_9.5a.json` / `matched_function_sources.md`
（個別 entry 的 verify 紀錄合併進 `notes` 欄位）。

## CRT 命名約定

最終 emit 出的 game source 直接 link Open Watcom v2 CRT，因此命名以「能被
Watcom linker 直接解析」為目標：

- **Watcom 真符號**（公開或 hidden PUBDEF；行為 byte-match Watcom 9.5/9.5a/9.5b/9.5c
  lib obj 或語意 1:1 對應）→ 使用 Watcom 原名（`malloc` / `memset` / `fopen` /
  `_nmalloc` / `__filbuf` / `IF@COS` / `__CHK` / `L$1_*` 等）。在
  `rebuild_info/crt/lookup_9.5a.json.by_address` 內有 193 個 entry。re-emit 後
  Watcom 直接 link 同名函式。
- **`crt_equivalent_*` (13 個)** — 行為等價 Watcom CRT 但 byte 不 match 任一
  lib obj 版本。emit_action = `emit_fd2_source`（FD2 source 端 emit 一個
  behaviour-equivalent C function；wlink 無法 lib resolve）。Ghidra 內以
  `search_functions_enhanced(name_pattern="^crt_equivalent_")` 列出。

## Static-link duplicates

幾個 helper 因 Watcom RTL 內多個 .obj 各帶一份而出現多個 address。Ghidra
接受同名 function（unique key 是 name+address），KB 不另加 suffix。本 binary
觀察到的 case：

- `__delay` @ `0x3DCCD` (71 callers, active) / `__delay_thunk_375b2` @ `0x375B2` (0 caller, dead)
  — Watcom CRT delay (DOS 21h tick wait) 兩份 obj 各帶一份
- `__exit` family @ `0x3CB91` / `0x3CB93` (`__exit_with_msg`) — terminate wrapper
  系列；emit pipeline 連結 Watcom CRT 後對 binary 影響為 0
- `crt_equivalent_get_eflags` @ `0x3ED58` (0 caller) /
  `crt_equivalent_get_eflags_thunk` @ `0x37F86` (2 callers) — `_disable` primitive
  + thunk，兩份都行為等價，emit 為 FD2 source

## CRT 公開 / hidden 符號清單

全部 193 個 byte-match 確認的 Watcom 真符號（公開 + hidden PUBDEF）按
address ↔ lib symbol 對照存於 `lookup_9.5a.json`，human-readable view 見
`matched_function_sources.md`。本檔不重複維護分群子集。

## CRT-equivalent / FD2-specific wrapper

本段把 Watcom CRT 相關但 lookup 沒命中（=必須 emit 為 FD2 source）的 function
按 CRT 角色分組。涵蓋兩類命名：

- **`crt_equivalent_*` (13 個)** — 行為等價於 Watcom CRT 但 byte 不 match
  任一 lib obj。`categorise()` 歸 `crt` pool,emit_action = `emit_fd2_source`。
- **`fd2_*` 中的 8 個 CRT-style primitive** — FD2 工程師自寫的 helper,
  主要為 Miles AIL callback 提供 DPMI / global accessor。
  `categorise()` 歸 `fd2` pool,emit_action = `emit_fd2_source`。

當前 Ghidra 內所有其他 `crt_*` 系列函式（softfp / format / fopen / heap /
dpmi / init / time / errno / signal / stream I/O / math 等）皆已歸 lookup
真名（Watcom 9.5/9.5a 公開或 hidden PUBDEF）,由 `rebuild_info/crt/lookup_9.5a.json`
維護。需要列出時查 `mcp__ghidra__search_functions_enhanced` 或 lookup file。

### 13 個 `crt_equivalent_*`（依角色分組）

**Startup / entry / exit (2)**：
- `crt_equivalent_entry_start @ 0x3C964` — DOS LE entry point；JMP 到 dos_main_bootstrap
- `crt_equivalent_dos_main_bootstrap @ 0x3C9DE` — Watcom 9.5a `cstart` startup body
  （435B；DPMI/DOS4GW detect、PSP parse、bss 0x331 dwords zero-init、run init/exit list）

**EFLAGS / `_disable` primitive (2)**：
- `crt_equivalent_get_eflags @ 0x3ED58` — `pushfd; pop eax; cli; ret` (Watcom `_disable`)
- `crt_equivalent_get_eflags_thunk @ 0x37F86`

**LX module loader chain (3)** — Watcom CRT 帶入但 FD2 從未呼叫的 dead loader code (LX format magic "LX\0\0" at [0x502f0])：
- `crt_equivalent_lx_chunk_read_36107 @ 0x36107` — 87B chunk reader, dual-source dispatch (memcpy 或 lseek+read)
- `crt_equivalent_lx_header_reader_36344 @ 0x36344` — 311B LX header reader (open + 0x40-byte MZ + 4-byte LX magic + 0xac LX header + 0x18-byte object table)
- `crt_equivalent_lx_module_loader_3647b @ 0x3647B` — 1151B 完整 LX loader (header + page table + fixup application + buffer alloc via [0x52758])

**Softfp (1)**：
- `crt_equivalent_softfp_tan_worker_4c630 @ 0x4C630` — soft-FP tan worker（被 `IF@TAN` 引用）

註：原 KB 列入此段的 `crt_equivalent_uint64_to_decimal_ascii_4d9e1` / `_getip_4da53` /
`_getip_body_4db08` 三個已 byte-match 為 Watcom `__Bin2String` 主體（297B 跨
0x4D9E1..0x4DB09，Ghidra 分成 entry + 兩個 alt-entry label `L_Bin2String_alt_72_getip_call`
@ 0x4DA53 / `L_Bin2String_alt_127_getip_landing` @ 0x4DB08），歸 lookup 真名，不再屬 crt_equivalent。

**FPE / matherr / linker padding stub (5)**：
- `crt_equivalent_exit_chain_stub_36de3 @ 0x36DE3` — Watcom CRT atexit chain 1B RET
- `crt_equivalent_fpe_default_handler_3d26e @ 0x3D26E` — FPE exception default 1B RET
- `crt_equivalent_linker_padding_4cbce @ 0x4CBCE` — linker leftover, zero xref
- `crt_equivalent_matherr_default_thunk_4d340 @ 0x4D340` — 5B JMP thunk to matherr_default_return_zero
- `crt_equivalent_matherr_default_return_zero_4d8ea @ 0x4D8EA` — `_matherr` default "ignore" path

### 8 個 `fd2_*` CRT-style primitive

**DPMI region/size primitives (6)** — 為 Miles AIL callback 提供 DPMI INT 31h fn 0x100/0x101/0x600/0x601 wrapper：
- `fd2_dpmi_alloc_dos_memory @ 0x361CC`
- `fd2_dpmi_free_dos_memory @ 0x36255`
- `fd2_dpmi_lock_region @ 0x36284`
- `fd2_dpmi_unlock_region @ 0x362F1`
- `fd2_dpmi_lock_size @ 0x36316`
- `fd2_dpmi_unlock_size @ 0x3632D`

**FD2 global accessors (2)**：
- `fd2_set_word_global_52758 @ 0x3615E`
- `fd2_set_word_global_5275c @ 0x3616E`

註：原 `fd2_filesize_path @ 0x36900` 與 `fd2_get_word_global_52754 @ 0x368FA`
已 reclassify 為 AIL pool（`AIL_internal_filesize_path` / `AIL_get_last_error_code`），
詳見 `rebuild_info/ail/extraction_prep.md` AIL 共用 / 邊界 helper 段。
