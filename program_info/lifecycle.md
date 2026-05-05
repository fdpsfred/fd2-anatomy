# lifecycle

包含五類 function：

1. **Watcom C runtime (CRT)**：純 Watcom 公開 / hidden 符號（47 個，例如
   `malloc` / `free` / `fread` / `fwrite` / `memset` / `memmove`、Watcom
   near-pointer 內部 `_nmalloc` / `_nfree`、Watcom hidden `__filbuf`、
   `__get_errno_ptr` 等。本 binary 的 `fopen` 入口走 FD2 wrapper
   `crt_fopen_read`，無公開 `fopen` 符號）+ FD2 / Watcom internal wrapper
   （178 個 `crt_*` 前綴，包括 DPMI / file-stream / heap / format engine /
   soft-FP / signal / 80-bit long double 等）。
2. **DPMI INT vector dispatch table**（256 個 `crt_dpmi_int_<NN>` stub +
   2 個 dispatcher trampoline）— 從 protected mode 觸發 real-mode INT N
   的 Watcom int86/int386 機制。
3. **Watcom alignment NOP fill**（74 個 `align_nop_<addr>`）— compiler
   在 function 之間插入的 LEA EAX,[EAX] / MOV EDX,EDX 等多位元組 NOP
   填充，永不被執行；建為獨立 Function entity 以滿足「每個 instruction
   byte 都歸屬於一個 function」的不變式。
4. **Vendor library helper placeholder**（約 226 個
   `crt_helper_<addr>` / `crt_<callee_descriptor>_<addr>`）— 從 Watcom CRT
   靜態 link 帶入的 vendor helper，命名以「主要 callee 行為」為後綴；
   完整語意分析待後續逐步補齊，plate comment 已記錄 callees 與 caller 數。
5. **Entry point / startup / exit**：`crt_entry_start @ 0x3C964` →
   `crt_main_trampoline @ 0x45D4B` → `fd2_main @ 0x25BF4`。
6. **結局 cinematic**：`play_ending_and_record_clear`（行為屬「程式生命週期最後階段」）。

## CRT 命名約定

最終 emit 出的 game source 直接 link Open Watcom v2 CRT，因此命名以「能被
Watcom linker 直接解析」為目標：

- **純 Watcom 公開 / 內部符號**（行為 1:1 對應 Watcom 標準 C 或 Watcom 已知
  hidden symbol）→ 使用 Watcom 真符號（`malloc` / `memset` / `fopen` 公開
  名；`_nmalloc` / `_nfree` / `__filbuf` 等 Watcom hidden）。re-emit 後
  Watcom 直接 link Watcom CRT 提供的同名函式。
- **FD2 / Watcom internal wrapper**（行為與 Watcom 風格一致但已知不是公開符號，
  或是 FD2 自寫的 CRT 風 wrapper）→ 保留 `crt_*` 前綴。emit 時這些函式作為
  FD2 source 一部分輸出，自身呼叫 Watcom CRT。

## Static-link duplicates

幾個 helper 因 Watcom RTL 內多個 .obj 各帶一份而出現 2 個 address。Ghidra
接受同名 function（unique key 是 name+address），KB 不另加 suffix。本 binary
觀察到的 case：

| 名稱 | copy A 位址 | copy A callers | copy B 位址 | copy B callers |
|---|---|---|---|---|
| `delay` | `0x3DCCD` | 71 | `0x375B2` | 0 |
| `crt_terminate` | `0x3CBB6` | 1 | `0x3CB91` | 0 |
| `crt_get_eflags` | `0x3ED58` | 0 | `0x37F86` | 0 |

`crt_get_eflags` 兩份都無 caller — 純 vendor library dead code；`delay` /
`crt_terminate` 各有一份 active（被實際呼叫）+ 一份死碼。emit pipeline 連結
Watcom CRT 後對 binary 影響為 0。

## CRT 公開 / hidden 符號 (47 個)

按用途分組；位址順序見 `call_graph.json` / `program_info/call_graph.dot`。

### 記憶體分配 (4)

| 名稱 | 位址 | 說明 |
|---|---|---|
| `malloc` | `0x36D16` | 公開 malloc；one-liner thunk to `_nmalloc` |
| `_nmalloc` | `0x36D26` | Watcom near-pointer malloc 內部，try-grow-retry |
| `free` | `0x37416` | 公開 free；one-liner thunk to `_nfree` |
| `_nfree` | `0x37426` | Watcom near-pointer free 內部 |

### File / Stream I/O (公開層 fread / fwrite / fopen 系) (10)

| 名稱 | 位址 | 說明 |
|---|---|---|
| `fread` | `0x37072` | 公開 fread，含 binary / text-mode CRLF 翻譯 |
| `fwrite` | `0x3744B` | 公開 fwrite，含 binary / text-mode CRLF 翻譯 |
| `fclose` | `0x37244` | 公開 fclose，走 `crt_open_files_list` 鏈頭 |
| `fseek` | `0x375F0` | 公開 fseek，SET / CUR / END whence |
| `fgets` | `0x46C4C` | 公開 fgets |
| `fputs` | `0x4D345` | 公開 fputs |
| `getc` | `0x3D9C1` | 公開 getc，refill via `__filbuf` |
| `putc` | `0x3DBF7` | 公開 putc |
| `fprintf` | `0x3F11B` | 公開 fprintf，走 `crt_format_engine` |
| `vfprintf` | `0x3D761` | 公開 vfprintf |

### POSIX-style low-level I/O (7)

| 名稱 | 位址 | 說明 |
|---|---|---|
| `open` | `0x3CD24` | 公開 open，走 `crt_open_impl` |
| `close` | `0x3D01C` | 公開 close |
| `read` | `0x3D990` | 公開 read，走 `crt_read_with_text_xlate` |
| `write` | （內聯於 `crt_write_with_text_xlate`） | — |
| `lseek` | `0x3CC00` | 公開 lseek |
| `_tell` | `0x3DDB3` | Watcom hidden 取目前 file pointer |
| `_filelength` | `0x3D056` | Watcom hidden 取 file size |

### 字串 / 記憶體拷貝 (10)

| 名稱 | 位址 | 說明 |
|---|---|---|
| `memcpy` | `0x3CBD6` | 公開 memcpy |
| `memmove` | `0x373C4` | 公開 memmove，overlap-safe |
| `memset` | `0x375C0` | 公開 memset，one-liner 到 32-bit fast fill |
| `_memset_inner` | `0x3DD10` | 32-bit fast fill 中段 worker |
| `_memset_bulk` | `0x3DD47` | bulk fill loop |
| `strcpy` | `0x3FD7C` | 公開 strcpy |
| `strncpy` | `0x46CB6` | 公開 strncpy |
| `strncmp` | `0x4962E` | 公開 strncmp |
| `strnicmp` | `0x46BFF` | 公開 strnicmp (Watcom 大小寫無關比較) |
| `strlen` | `0x37805` | 公開 strlen |

### ctype (2)

`tolower @ 0x3D7E1`、`toupper @ 0x46BEA` — Watcom 公開 ctype。

### 數學 (3)

`sin @ 0x3C898`、`cos @ 0x3C885`、`strtod @ 0x4D14C` — 公開三角函式 + 字串轉 double。

### 時間 / 日期 (3)

`time @ 0x3FB57`、`asctime @ 0x3FC3B`、`mktime @ 0x46E03`。

### 環境 / 退出 (2)

`getenv @ 0x3F13B`、`exit @ 0x36DE4`（exit 寫成 `crt_exit` 也行，但本 binary
此 address 對應公開 `exit` 語意，不取 prefix）。

### x86 port I/O (1)

`outp @ 0x37795` — Watcom 公開 outp。

### 雜項 (4)

| 名稱 | 位址 | 說明 |
|---|---|---|
| `delay` | `0x3DCCD`（active）/ `0x375B2`（dead） | Watcom CRT delay (DOS 21h tick wait) |
| `sprintf` | `0x377D9` | 公開 sprintf |
| `__filbuf` | `0x3DA65` | Watcom hidden file buffer refill |
| `__get_errno_ptr` / `__get_doserrno_ptr` | `0x3D7F6` / `0x3D7FC` | Watcom hidden errno getter |

## CRT internal wrapper (178 個 `crt_*`)

按用途分組；不羅列每個位址，需要時查 `program_info/call_graph.json`
(`category=="crt"` 過濾) 或 Ghidra `search_functions_enhanced(name_pattern="crt_")`。

### Entry / startup / exit (8)

| 位址 | 名稱 | 說明 |
|---|---|---|
| `0x3C964` | `crt_entry_start` | DOS LE entry point；偵測 DPMI / DOS4GW / 真實模式，parse PSP @ 0x81/0x2C，zero-init bss 0x331 dwords，跑 `crt_run_init_list` → `crt_main_trampoline` → `crt_run_exit_list` → DOS exit。reference 了 FD2 bss globals 故保留 `crt_*` |
| `0x3C9DE` | `crt_entry_start_dup` | static-link 第二份 entry stub（dead-code 旁路） |
| `0x45D4B` | `crt_main_trampoline` | stack guard (`crt_get_stack_avail`) + stream init (`crt_init_stream_threshold`) + `fd2_main(argc, argv)` + tail `exit` |
| `0x45D9A` | `crt_run_init_list` | XI-segment runner，priority 升序 |
| `0x45DDD` | `crt_run_exit_list` | YI-segment runner，priority 降序（LIFO atexit semantics） |
| `0x3CB91`, `0x3CBB6` | `crt_terminate` | 終止 wrapper（兩份 copy） |
| `0x3CB93` | `crt_abort_with_log` | abort + log |
| `0x3CBD1` | `crt_dos_mode_exit_cleanup` | 退出前 DOS mode 還原 |

### Stack / frame helpers (3)

`crt_capture_ss_for_stkchk @ 0x36CD0`、`crt_frame_setup @ 0x36CD7`（compiler-emitted
prologue：`PUSH framesize_imm; CALL crt_frame_setup`）、`crt_check_stack_overflow_inner @ 0x36CEA`、
`crt_get_stack_avail @ 0x463BC`。emit pipeline 跳過 frame_setup（compiler 會
自己 emit）。

### DPMI / DOS interface (10+)

`crt_dpmi_alloc_dos_memory / crt_dpmi_free_dos_memory / crt_dpmi_lock_region /
crt_dpmi_unlock_region / crt_dpmi_lock_size / crt_dpmi_unlock_size /
crt_dpmi_alloc_memory / crt_dpmi_or_dos_alloc / crt_dos_resize_alloc /
crt_alloc_and_commit / crt_decommit_and_free` — DPMI 0x100 系列 + DOS 0x21 配發。
`crt_install_critical_error_handler / crt_get_dpmi_handler_info /
crt_dos_set_int_vector / crt_dpmi_signal_handler_dispatch /
crt_dpmi_signal_emulate / crt_dpmi_exception_dispatch / crt_dpmi_raise_signal /
crt_signal_handler_print` — DPMI signal / handler 機制。

### File / Stream wrapper (15+)

`crt_fopen_impl @ 0x36FA1` / `crt_fopen_read @ 0x36FCC`（FD2 hardcoded "rb" 的
fopen wrapper） / `crt_fopen_open_helper @ 0x36EBC` / `crt_parse_fopen_mode @ 0x36E0D` /
`crt_fclose_impl @ 0x37270` / `crt_fclose_inner @ 0x37326` /
`crt_classify_stream_handle / crt_init_file_buffer / crt_filbuf_first_byte /
crt_flush_stream_buffer / crt_lseek_current_pos / crt_open_impl /
crt_read_with_text_xlate / crt_write_with_text_xlate / crt_alloc_file_entry /
crt_free_file_entry / crt_get_fd_entry / crt_set_fd_entry /
crt_isatty_dos / crt_setvbuf_default / crt_setvbuf_internal /
crt_init_stream_threshold / crt_close_streams_for_pgrp /
crt_flush_streams_matching_mask / crt_dos_get_char / crt_dos_get_char_dup /
crt_dos_write_handle / crt_putc_dos / crt_putc_tty / crt_filesize_path /
crt_load_file_to_memory / crt_make_temp_filename / crt_int_to_hex_char`。
`crt_open_files_list @ 0x541AC` 是 `fclose` 用來找 stream 的 FILE* 鏈頭。

### Heap (5)

`crt_heap_alloc_block / crt_heap_free_block / crt_heap_get_last_segment /
crt_heap_grow / crt_heap_oom_handler_default / crt_heap_query_d726`。

### Format engine (printf / float printing)

`crt_format_engine @ 0x3DDC7`（fprintf / sprintf 共用 worker） /
`crt_parse_format_spec / crt_parse_format_flags / crt_strnlen_ascii /
crt_strnlen_wide / crt_format_hex_fixed_width / crt_format_fixed_decimal /
crt_format_float_dispatch / crt_format_dispatch_one_spec / crt_format_float_spec /
crt_format_float_strip_zeros / crt_format_float_main / crt_format_exponent /
crt_format_mantissa_with_dp / crt_format_right_align_pad / crt_format_nan_inf /
crt_format_2digit_at / crt_format_time_local / crt_format_time_default_buf /
crt_asctime_default_buf / crt_int_to_string_base / crt_int32_to_string_base /
crt_uint_to_string_base / crt_uint32_to_string_base / crt_parse_int_with_base /
crt_parse_uint_decimal`。

### Soft-FP / 80-bit long double (46 個)

Watcom 32-bit target 不開 FPU 時所有 long double / extended-precision math
走 soft-FP RTL。命名以 `crt_softfp_*` 前綴：

- 基本算術：`crt_softfp_ld_add / crt_softfp_ld_add_inplace / crt_softfp_ld_add_imm /
  crt_softfp_ld_add_worker / crt_softfp_ld_mul_wrapper / crt_softfp_ld_mul_inplace /
  crt_softfp_ld_mul_worker / crt_softfp_ld_div_worker`
- 範圍 / 分類：`crt_softfp_ld_check_nan_inf / crt_softfp_ld_range_check /
  crt_softfp_fp_class_set_flags / crt_softfp_log_validate / crt_softfp_math_error /
  crt_softfp_special_dispatch / crt_softfp_exception_dispatch`
- 轉換：`crt_softfp_ld_to_int32 / crt_softfp_int32_to_ld / crt_softfp_uint32_to_ld /
  crt_softfp_double_to_ld / crt_softfp_ld_to_double_round / crt_softfp_float_to_ld /
  crt_softfp_ld_round_to_int / crt_softfp_floor_pass / crt_softfp_double_get_exp`
- 三角 / 指對：`crt_softfp_atan2_inner / crt_softfp_atan_worker /
  crt_softfp_sin_normalize / crt_softfp_sincos_polynomial / crt_softfp_tan_worker /
  crt_softfp_tan_thunk / crt_softfp_exp_worker / crt_softfp_log10`
- 多項式 / mod：`crt_softfp_horner_loop / crt_softfp_horner_wrap /
  crt_softfp_fmod_wrapper / crt_softfp_fmod_worker / crt_softfp_pow_helper`
- BCD / decimal printing：`crt_softfp_format_digits / crt_softfp_scale_by_pow10 /
  crt_softfp_double_to_digits / crt_softfp_digits_to_bcd64 /
  crt_softfp_normalize_bcd64 / crt_softfp_pow10_table_search /
  crt_softfp_double_to_scaled_bcd64 / crt_softfp_bcd64_to_ascii /
  crt_softfp_thunk_4da53`

`crt_softfp_*` 屬 helper struct `long_double_80` (10 bytes:
`dwMantissa_lo / dwMantissa_hi / wSign_exp`)；ABI 走 Watcom soft-FP register
convention（`__watcall` + plate 註明 register layout，emit pipeline 階段
byte-level 比對）。

### 數學 / RNG / FPU (4)

`crt_sqrt_helper @ 0x3C6FC` / `crt_cos_inner @ 0x3C7B6` / `crt_sin_inner @ 0x3C7CF` /
`crt_fpu_wait_busy @ 0x3C859` / `crt_get_rand_seed_ptr / crt_rand_update_seed`。
FD2 game-logic 的隨機性（attack roll、spell 命中等）走自己的 named wrapper，
不直接命名 CRT RNG。

### Time / locale (8)

`crt_dos_gettime_components / crt_init_timezone / crt_parse_tz_offset /
crt_parse_tz_dst_rule / crt_parse_tz_string / crt_seconds_to_tm / crt_is_leap_year /
crt_compute_is_dst / crt_tm_time_less / crt_init_clock_globals / crt_divmod_helper`。

### Errno / abort / signal (8)

`crt_abort_thunk / crt_abort_thunk2 / crt_dos_to_errno / crt_set_errno_eacces /
crt_set_errno_erange / crt_get_eflags`（兩份 copy） / `crt_capture_regs /
crt_capture_edi_helper / crt_save_segment_regs`、Windows-notification helpers
`crt_init_windows_notification / crt_deinit_windows_notification`、env init
`crt_init_argv / crt_parse_cmdline_args / crt_init_env_array`。

### 其他 (5)

`crt_get_dgroup_segment @ 0x3DB10`、`crt_strupr @ 0x3E702`、`crt_fprintf_stderr @ 0x36DC1`。

## DPMI INT vector dispatch (256 個 `crt_dpmi_int_<NN>` + 2 dispatcher)

Watcom CRT 在 protected mode 下實作 `int86` / `int386` 等 BIOS interrupt 觸發
機制需要切回 real mode 執行 `INT N`。FD2.LE 以 256 個固定 3-byte stub 排成一張
table 從 `0x465F8` 開始，每個 stub 內容為 `INT 0xNN; RET`（NN 從 `0x00` 到
`0xFF`，slot `0x03` 用 1-byte `CC`/INT3 編碼省 1 byte）。

兩個 dispatcher：
- `crt_dpmi_int_invoke_inner @ 0x465C5` — trampoline，收 interrupt number 計算
  `(intnum & 0xff) * 3 + 0x465F8` 取得 stub 位址，PUSH 為 fake return address，
  從 regs 結構回填全部 GPR + segment register，最後 `RET` 跳進指定的
  `crt_dpmi_int_NN` stub
- `crt_dpmi_int_invoke @ 0x4657B` — outer wrapper（Watcom int386 等價），
  呼叫 trampoline 後從 CPU state 抓回 EAX / EFLAGS / DS / ES / FS / GS 寫回
  regs 結構

## Watcom alignment NOP fill (74 個 `align_nop_<addr>`)

Watcom v2 compiler 為了讓 hot function 的 entry 對齊到 16-byte 邊界，會在
function 之間插入多位元組 NOP 指令當 padding。常見 encoding：

- `8d 80 00 00 00 00` = `LEA EAX,[EAX]` (6-byte NOP)
- `8d 40 00` = `LEA EAX,[EAX+0]` (3-byte NOP)
- `89 d2` = `MOV EDX,EDX` (2-byte NOP)
- `89 c0` = `MOV EAX,EAX` (2-byte NOP)
- `89 c9` = `MOV ECX,ECX` (2-byte NOP)
- `90` = `NOP` (1-byte)

這些位置都緊貼下個 function 的 entry，且自身 0 caller — 純粹是 alignment
padding，永不被執行。建為獨立 Function entity 是為了滿足「`.object1` 每個
instruction byte 都歸屬於一個 function」這個不變式。

emit pipeline 階段：直接讓 Watcom v2 重新 compile 即可自動生成 alignment
padding，這 74 個 stub **不需要** 出現在 FD2 source 中。

## Vendor library helper placeholder (約 226 個)

Watcom CRT 靜態 link 帶入但未對應到公開符號的 vendor helper，以主要 callee
行為作後綴生成 best-effort placeholder 名稱：

- `crt_<descriptor>_<addr>` — 有明確主要 callee（如
  `crt_softfp_signal_49ee8` 的主要 callee 是 `crt_dpmi_signal_handler_dispatch`、
  `crt_dpmi_unlock_helper_3fa8c` 的主要 callee 是 `crt_dpmi_unlock_region`）
- `crt_helper_<addr>` — 沒有清楚的單一 callee 訊號

每個 placeholder 的 plate comment 紀錄 caller 數、callee list、function 大小，
作為後續精細命名的起點。emit pipeline 階段一律由 Watcom CRT 連結解析，FD2
source 不需要重寫這些 helper。

## CRT 程式碼地理位置

Watcom CRT 函式分散在 `.object1` 各處（與 game logic、Miles AIL library
**互相交錯**，不是連續區段）。CRT 集中區大致落在 `0x36000-0x37700`、
`0x3C000-0x40000`、`0x45D00-0x4E000`，但 emit pipeline **不能用 address range
判定某 function 是否屬於 CRT**。判別方式：function 名前綴 `crt_` 或屬於上面
47 個公開 / hidden 符號清單，否則屬 game logic。

## 結局 cinematic

`play_ending_and_record_clear @ 0x0001F894` — 結局動畫播放，並寫入通關記錄到
FD2.SAV。為何屬 lifecycle 而非 field_map：它是 game session 的最後一段流程，與
`fd2_main` 退出條件直接耦合。
