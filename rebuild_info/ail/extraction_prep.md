# AIL `.obj` extraction handoff

把 FD2.LE 內 Miles AIL ecosystem 抽出成獨立 `.obj` 並與 Watcom C/C++ 9.5a CRT
（原 binary 同版編譯器，見 `../crt/fid_match.md`）重新連結時需要的前置資料：
邊界、calling-convention 例外、CRT 替換對應、build pipeline 草案。AIL function
命名分類詳見 `./inventory.md`。

## 抽 `.obj` 的 function 邊界

要抽的 291 個 function 對應分布見 `./inventory.md` 的 inventory 段（另 5 個
`reclassify-from-crt` BFS-unreached internal 細項見 inventory.md L26）；
依用途決定如何寫進 .obj：

| 群組 | 數量 | .obj 處置 |
|---|---:|---|
| `AIL_*` 公開 API | 106 | 全部寫進 PUBDEF（外部 caller 透過這些名稱 link） |
| `AIL_internal_*` BFS 可達 + 4 個 promoted | 151 | 全部納入 .obj，**不**寫 PUBDEF（vendor 內部，linker 不需 export） |
| `AIL_internal_*` orphan — `vtable_indirect` | 11 | 必納入（function-pointer 註冊點仍指向它） |
| `AIL_internal_*` orphan — `cluster_member` | 8 | 必納入（被 vtable_indirect / 其他 internal call） |
| `AIL_internal_*` orphan — `tail_call_target` | 1 | 必納入 |
| `AIL_internal_*` orphan — `dead_code_stub` | 14 | **可省略** — linker 帶入但 binary 從未引用；省略不影響等價性 |

「可省略」的 14 個 dead_code_stub（plate comment 已標 sub-case）：
`AIL_internal_audio_mix_isr` / `_sequence_timer_isr` / `_driver_start_output_3fe6b` /
`_driver_stop_output_3feb3` / `_dig_driver_teardown` / `_set_sample_user_data_inner` /
`_dpmi_unlock_dig_mix_416e4` / `_midi_status_length` / `_dig_load_buffer_chunk_42088` /
`_byteswap_u32_4218d` / `_sequence_init` / `_dead_jmp_to_log_decrement_3a548` /
`_start_all_timers_inner` / `_stop_all_timers_inner`。可選擇：
- 完整保留 → 產生與原 binary byte-identical 的 .obj
- 全部省略 → 縮小 .obj，binary 行為等價（原 binary 也未引用）

位址範圍非連續區段。wlink 依 `.obj` include 順序混排（`MEMORY.md`
`project_function_interleave.md` 描述同一現象），AIL function 在 0x37000 附近
最密集但與 CRT helper interleave；抽取時必須依 inventory 列表（用 Ghidra MCP
`search_functions(name_pattern="^AIL_")` 從 Ghidra 即時生成）而非 address range。

## AIL 共用 / 邊界 helper

下列原以為「保留 `crt_*` 命名、由 CLIB3S 9.5a 直接 EXTDEF 解析」的 helper，經
byte_match + caller 分析後**多數歸 fd2 / AIL pool**（非 Watcom CRT primitive），
EXTDEF 路徑只剩 abort thunk 一個成立。emit pipeline 分流按下表處理：

- **FD2 自寫 DPMI primitive (6 個 `fd2_dpmi_*`)** — 為 Miles AIL callback 提供
  DPMI INT 31h fn 0x100 / 0x101 / 0x600 / 0x601 wrapper：
  `fd2_dpmi_alloc_dos_memory @ 0x361cc` / `fd2_dpmi_free_dos_memory @ 0x36255` /
  `fd2_dpmi_lock_region @ 0x36284` / `fd2_dpmi_unlock_region @ 0x362f1` /
  `fd2_dpmi_lock_size @ 0x36316` / `fd2_dpmi_unlock_size @ 0x3632d`。
  `fd2_dpmi_lock_size` 被 game `fd2_set_bgm_track_with_fade` 直接共享。emit_action =
  `emit_fd2_source`（FD2 source 端 emit），不是 CRT EXTDEF。
- **AIL-internal file/global accessor (2 個)** — 經 caller 分析升入 AIL pool：
  - `AIL_internal_filesize_path @ 0x36900`（open + filelength + close 組合 helper）
  - `AIL_get_last_error_code @ 0x368fa`（原 `crt_get_word_global_52754` /
    `fd2_get_word_global_52754` placeholder；caller 全 AIL）
  emit_action = `link_vendor_lib`（隨 AIL3DIG/AIL3MDI relink 帶入）。
- **Abort 路徑 (1 個 Watcom CRT EXTDEF)** — `__FpAbort @ 0x46b41`
  （原 `crt_abort_thunk`，caller path 含 `__prtf` / `__STKOVERFLOW`）。
  Watcom CLIB3S 9.5a 真符號 byte_match，emit_action = `link_vendor_lib`，由
  CLIB3S EXTDEF 解析。原 `crt_abort_with_log` placeholder 經 boundary cleanup
  後已併入 __FpAbort body 或同等 wrapper；不再單獨存在。
- **EFLAGS / `_disable` primitive (2 個 `crt_equivalent_*`)** — 行為等價於 Watcom
  `_disable` 但 byte 不 match 任何 lib obj：
  `crt_equivalent_get_eflags @ 0x3ed58` + `crt_equivalent_get_eflags_thunk @ 0x37f86`。
  emit_action = `emit_fd2_source`（FD2 source 端 emit behaviour-equivalent C
  function；wlink 無法 lib resolve）。AIL ISR 與 FD2 game-side 都會呼叫。

AIL `.obj` 抽取時的處置：上述 helper **都不抽進 AIL `.obj`**（fd2/crt 命名
都歸 FD2-source 端 emit，AIL 端走 EXTDEF reference）。但要注意只有 `__FpAbort`
是 Watcom CLIB3S EXTDEF；其他 fd2_* 與 crt_equivalent_* 都需 FD2 source 端先
emit 出來才能被 AIL `.obj` 的 EXTDEF reference 找到。

## Calling convention 例外

8 個違反 Watcom 9.5a `__cdecl` EBX preservation 約定的 AIL function（內部用
EBX 為 loop counter，prologue 無 `push ebx` / epilogue 無 `pop ebx`）：

| 函式 | 地址 |
|---|---|
| `AIL_get_real_vect` | 0x37D24 |
| `AIL_API_read_INI` | 0x38113 |
| `AIL_register_timer` | 0x3846C |
| `AIL_get_IO_environment` | 0x38AF6 |
| `AIL_install_DIG_INI` | 0x38D3B |
| `AIL_install_MDI_INI` | 0x3A722 |
| `AIL_allocate_sequence_handle` | 0x3A953 |
| `AIL_lock_channel` | 0x3C18B |

這 8 個函式在 client C declaration 應標 `__watcall` 而非 `__cdecl`，否則
wcc386 會 emit caller code 預期 EBX preservation 並產生實際執行錯誤。

## CRT EXTDEF map

AIL function body 內每個 `E8 disp32` CALL 到 CRT 的指令，target 對應 CLIB3S
9.5a 真符號（完整 lookup 在 `../crt/lookup_9.5a.json`）。因 rebuild 用 9.5a
（與原 binary 同版），symbol name 在 lib obj 內 byte-identical，EXTDEF 直接
解析無需 wrapper：

| FD2.LE addr | Ghidra 名 | CLIB3S 9.5a 符號 |
|---|---|---|
| `0x36CD7` | `crt_frame_setup` | `__CHK` |
| `0x36D16` | `crt_malloc_track` | `_nmalloc` (Watcom near-pointer alloc) |
| `0x36DE4` | `crt_exit` | `_exit` |
| `0x36FA1` | `crt_fopen_impl` | `_fopen` |
| `0x36FCC` | `crt_fopen_read` | `fopen("rb")` wrapper (FD2-source emit) |
| `0x37072` | `crt_fread` | `_fread` |
| `0x37244` | `crt_fclose` | `_fclose` |
| `0x373C4` | `crt_memmove` | `_memmove` |
| `0x37416` | `crt_free` | `_nfree` |
| `0x3744B` | `crt_fwrite` | `_fwrite` |
| `0x375C0` | `crt_memset` | `_memset` |
| `0x375F0` | `crt_fseek` | `_fseek` |
| `0x3D7F6` | `errno_addr` | `__get_errno_ptr` (CLIB3S 9.5a 真符號) |

## ABI 與 CRT global 位址

因 rebuild 用 Watcom 9.5a（FD2.LE 同版），`_iobuf` layout / `__iob[]` stride /
`errno` / `_ClosedStreams_head` / `_OpenStreams_head` 等 CRT struct 與 global
位址在 lib obj 內與原 binary byte-identical。AIL 函式不直接 absolute reference
這些 global（全部透過 fopen / fclose / `__get_errno_ptr` 等函式呼叫間接存取），
即使 wlink relink 後位址改變也不影響。

不需要跨版本 _iobuf shim：CRT global 位址（`__iob @ 0x52840..0x52A48` 520B、
`errno @ 0x541A4` 4B、`_ClosedStreams_head @ 0x541A0`、`_OpenStreams_head @
0x541AC`）由 9.5a CLIB3S 提供，重 link 自然會 patch 到對應位址，emit pipeline
不需要顯式處理。

## AIL string / data byte ranges

AIL ecosystem 在 `.object2` 字串區佔據 0x50000..0x53800 範圍（與 CRT 字串
interleave）。AIL extraction 必須帶入下列字串 group，rebuild 時 .obj 內部
emit 為 string literal（Watcom 9.5a string-pool 處理 dedup）。

### AIL log / API trace 字串（143 條，pool=ail）

每個 logged AIL public API 有對應 `data_ail_string_log_api_<api>_<addr>` 字串，
caller 端 fprintf inside `AIL_internal_log_print_timestamp_prefix` log-gate。
完整清單透過 verdict.jsonl filter `caller_pool=ail` + `data_kind=vendor_string`
取得，主要分布：

| sub-class | count | 範圍 | 典型內容 |
|---|---:|---|---|
| log_api entry fmt | 143 | 0x5042e..0x5118d 主體 + 0x50f1a 散件 | `"AIL_<api>(args)\n"` self-print |
| log_result fmt | 4 | 0x50f1a + others | `"Result = %d\n"` / `"%X"` / `"%d:%d"` shared |
| log_banner / log_timestamp / log_start | 3 | 0x5042e prefix | startup banner + `[%08X.%03d]` |
| log_separator (BYTE_ARRAY anchor) | 2 | 0x5030f / +2 | indent prefix bytes for log nesting |
| ini_key (strnicmp keywords) | 5 | 0x511c8..0x511ec | DRIVER / DEVICE / IO_ADDR / DMA_8_bit / DMA_16_bit |
| ini_key (BYTE_ARRAY anchor) | 1 | 0x511de | "IRQ\0" 4B in `"IO_ADDR\0IRQ\0"` |
| log_ini readout | 6 | (scattered) | Driver=%s / Device=%s / IO=0x%X / IRQ=%d / DMA_8=%d / DMA_16=%d |
| envvar_debug / envvar_sys | 2 | (scattered) | AIL_DEBUG / AIL_SYS_DEBUG getenv keys |
| signature (driver/file magic) | 3 | 0x51250 / 0x51258 / 0x5141a | AIL3DIG / AIL3MDI / Creative |
| err_msg (last_error buffer) | 17 | 0x511f7..0x516cc 主體 | Corrupted .INI / Insufficient memory / Out of driver handles / ... |
| filename | 1 | 0x515b0 | MDI.INI |
| hex_digit | 1 | 0x511b4 | "0123456789ABCDEF\0" (parse_int_with_base lookup) |
| gtl_filename | 1 | 0x53698 | 128B writable buffer, init="SAMPLE\0..." |

完整 AIL string item sub-class 分布從 `tools/program_analysis/data_audit/data/verdicts.jsonl`
filter `caller_pool=ail` + `data_kind=vendor_string` 取得（10+ sub-class：
log_api / log_result / log_separator / log_banner / log_timestamp / log_ini /
ini_key / signature_dig / signature_mdi / signature_voc / err_* / filename /
hex_digit / envvar_* / gtl_filename）。

### Watcom 9.5a `.obj`-boundary string-pool 不 dedup（rebuild 必須對應）

Watcom 9.5a 在編譯每個 `.obj` 時內部 dedup string literals，但**不跨 `.obj`
boundary 去重**。同字串若在多個 `.obj` source file 用，binary 內會有多 copy。
AIL extraction 必須對下列已觀察到的 cross-`.obj` 多 copy 字串保留各 copy：

| 字串 | 位址 | 所在 AIL `.obj` |
|---|---|---|
| `"Out of timer handles\n"` | 0x5128c + 0x51580 | install_driver.OBJ + mdi_driver_setup.OBJ |
| `"Unrecognized digital audio file type\n"` | 0x51428 + 0x5146d | allocate_file_sample.OBJ + set_sample_file.OBJ |

rebuild pipeline 對應規則：在 AIL 各 `.obj` 重新 compile 時，各 source file
內保留自己的 string literal（Watcom 9.5a 編譯器在 `.obj` 內部自動 dedup，
但不跨 `.obj`）。

### dead_code_stub 對應的 data items

兩個已知 dead AIL function（`AIL_resume_sample @ 0x39522` /
`AIL_set_sequence_tempo @ 0x3AD52`）各自帶一條 log_api fmt string：

| dead AIL fn | log fmt addr | 字串 |
|---|---|---|
| `AIL_resume_sample` | 0x508a8 | `"AIL_resume_sample(0x%X)\n"` |
| `AIL_set_sequence_tempo` | 0x50ce8 | `"AIL_set_sequence_tempo(0x%X,%d,%d)\n"` |

選擇：
- 完整保留 dead_code_stub function + 對應 fmt 字串 → byte-identical .obj
- 全部省略 → 縮小 .obj，binary 行為等價（caller 不可達）

兩條 fmt 字串也被相鄰活 function 透過 fall-through dead-tail 引用（`AIL_stop_sample`
的 tail 引 0x508a8、`AIL_end_sequence` 的 tail 引 0x50ce8）。若選「省略 dead function」
路徑，這些 fall-through tail 也要刪。

### AIL writable state buffer

`0x53698` 是 128B writable buffer，初始內容 `"SAMPLE\0..."`。caller
`AIL_internal_set_GTL_filename_prefix_inner` 用 strcpy 覆寫。AIL extraction
時：

- emit 為 `char ail_gtl_filename_prefix[128] = "SAMPLE";`（C 編譯器自動補
  尾部 zero fill 到 128B）
- 不可 emit 為 `const char *` — buffer 本身被覆寫，非 string literal

### AIL mixer dispatch state register bank (`0x538A0..0x538B7`)

24-byte 連續 BSS-style writable state，被 AIL mixer dispatch / mix-loop
kernel 集中讀寫。6 個 dword register，全 zero-init：

| Addr | Name | 用途 | 主要 caller |
|---|---|---|---|
| `0x538A0` | `data_ail_mix_format_flags` | 當前 mix-batch sample 格式 / 通道 flags | `AIL_internal_mix_dispatch_format` / `_sample` / `_register_mix_globals` |
| `0x538A4` | `data_ail_mix_sample_pos_current` | 當前 sample-buffer 讀指標（隨 mix step 推進） | `AIL_internal_mix_dispatch_sample` |
| `0x538A8` | `data_ail_mix_sample_pos_end` | sample-buffer 終止指標（內 loop 邊界） | `_mix_dispatch_sample` + ~74 個 `AIL_internal_mix_loop_XX` |
| `0x538AC` | `data_ail_mix_sample_data_start` | sample-buffer base 指標 | `_mix_dispatch_sample` |
| `0x538B0` | `data_ail_mix_sample_data_end` | sample-buffer 環繞邊界指標 | `_mix_dispatch_sample` + ~74 個 `_mix_loop_XX` |
| `0x538B4` | `data_ail_mix_pitch_low` | pitch accumulator 低位 (sample-rate conversion 小數部分) | `_mix_dispatch_sample` + ~36 個 interpolating `_mix_loop_2X..6X` |

AIL extraction 時：
- emit_action: `link_vendor` — 全部納入 AIL .obj 的 BSS section (24 byte zero-init),
  不寫 PUBDEF（mixer 內部 state，外部 caller 不直接讀寫）
- C source 等價: `static int ail_mix_format_flags; static int ail_mix_sample_pos_current;
  static int ail_mix_sample_pos_end; static int ail_mix_sample_data_start;
  static int ail_mix_sample_data_end; static int ail_mix_pitch_low;`
  emit 順序須 byte-exact 對應 0x538A0..0x538B7 layout

這 6 個 register 與 mix-loop dispatch table （`0x47638` / `0x47838`
data_ail_mixer_dispatch_table_a/b @ pointer[128]）共同構成 AIL mixer pipeline
的 stateful 核心。

### AIL ISR / timer manager / state-arrays cluster (`0x52A54..0x5376F`)

完整的 AIL 內部 ISR 與 timer-manager 狀態區，連續分布於 .object2 內 ~3.6KB
範圍。全部 zero-init（由 AIL `_register_state_globals` / `_init_state_arrays` /
`_init_runtime_defaults` 在 AIL_startup 階段 DPMI lock_region + 寫入初值）。

#### 16-slot timer SoA cluster (`0x52A54..0x52BD3`, 1408B)

AIL software timer manager 採 struct-of-arrays，6 個並行 array each 16 × u32：

| Addr | Name | 用途 |
|---|---|---|
| `0x52A54` | `data_ail_timer_slot_callback_fnptr_array_16` | code* 16 — 用戶 timer callback |
| `0x52A94` | `data_ail_timer_slot_state_array_16` | uint 16 — slot.state (0=inactive / 1=paused / 2=running) |
| `0x52AD4` | `data_ail_timer_slot_elapsed_accumulator_16` | uint 16 — PIT-cycles 累計 |
| `0x52B14` | `data_ail_timer_slot_period_pit_cycles_16` | uint 16 — 觸發周期 |
| `0x52B54` | `data_ail_timer_slot_pending_trigger_count_16` | **uint 15**（非 16；ISR drain bound `< 0x3c`） |
| `0x52B90` | `data_ail_isr_nested_pending_count` | uint scalar — 佔據 trigger_count 第 16 slot 位置 |
| `0x52B94` | `data_ail_timer_slot_user_data_word_16` | uint 16 — 傳給 callback 的 user data |

**ISR asymmetry**：accumulate loop bounds `< 0x40` (16 slots) 但 drain loop `< 0x3c` (15 slots)；slot 15 的 trigger_count 可被 ISR 寫但永不被 drain 讀（benign — 設計上保留 slot 15 給 `data_ail_isr_nested_pending_count` 共址）。

AIL extraction 時 6 個 array 須分別 emit 為 `static uint name[16];` / `static code *callback_array[16];`；`data_ail_isr_nested_pending_count` 必須**正好**放在 `pending_trigger_count_16` 後面（同位址 0x52B90 = pending_trigger_count + 0x3C），否則 ISR drain loop bound 失效。

#### AIL ISR ancillary state cluster (`0x52BD4..0x52BFB`, 40B)

| Addr | Type | Name | 用途 |
|---|---|---|---|
| `0x52BD4` | uint | `data_ail_timer_isr_pit_period_word` | PIT period 配置 |
| `0x52BD8` | ushort | `data_ail_isr_saved_original_vector_selector` | install 前原 IRQ vector selector |
| `0x52BDA` | uint | `data_ail_isr_saved_original_vector_offset` | install 前原 IRQ vector offset |
| `0x52BDE` | uint | `data_ail_pit_divisor_current` | PIT 8253 divisor mirror (port 0x40 寫入值) |
| `0x52BE2` | uint | `data_ail_isr_pit_period_per_tick` | 每 tick 累加到 elapsed counter 的 PIT cycles |
| `0x52BE6` | uint | `data_ail_isr_reentry_lock_count` | ISR 再入計數（0 = not in ISR） |
| `0x52BEA` | uint | `data_ail_log_lock_nesting_count` | log lock 巢套深度（gate drain loop） |
| `0x52BEE` | ushort | `data_ail_isr_ss_selector` | ISR install-time SS snapshot |
| `0x52BF0` | uint | `data_ail_use16_isr_install_state_flags` | USE16 ISR 安裝旗標 bitfield |
| `0x52BF4` | uint | `data_ail_use16_isr_saved_realmode_vector_offset` | 原 real-mode vector offset |
| `0x52BF8` | uint | `data_ail_use16_isr_saved_realmode_vector_segment` | 原 real-mode vector segment |

#### AIL USE16 ISR 16-bit stack buffer (`0x52BFC..0x535F3`, 2552B)

`data_ail_use16_isr_temp_stack_buffer_2552b` — 16-bit limit-friendly scratch
buffer used as stack for AIL's real-mode-callback ISR. 起始位址在 AIL
`_set_USE16_ISR_inner` 用 DPMI INT 31h AX=0x07 注入新 LDT selector 的 base。
SP 初值 = buffer base aligned + 0x200，被 patch 進 USE16 ISR template @ 0x3EAC4。

#### AIL ISR runtime callback context (`0x535F4..0x53603`, 16B)

| Addr | Type | Name | 用途 |
|---|---|---|---|
| `0x535F4` | uint | `data_ail_isr_callback_return_address_eip` | 從 user callback 返回後 ISR 的 resume EIP |
| `0x535F8` | uint | `data_ail_isr_callback_user_data_current` | 當前正在 dispatch 的 timer slot 的 user_data (從 timer_slot_user_data_word_16 拷貝) |
| `0x535FC` | ushort | `data_ail_isr_saved_caller_ss` | 被中斷 thread 的 SS |
| `0x535FE` | (align 2B) | `data_align_535fe` | 對齊 padding |
| `0x53600` | pointer | `data_ail_isr_saved_caller_esp_ptr` | 被中斷 thread 的 ESP |

#### AIL driver_timer_isr + state_arrays init guards (`0x53604..0x5360B`, 8B)

| Addr | Type | Name | 用途 |
|---|---|---|---|
| `0x53604` | uint | `data_ail_driver_timer_isr_reentry_guard` | AIL driver timer ISR (0x3F217) reentry counter |
| `0x53608` | uint | `data_ail_state_arrays_init_done_sentinel` | AIL data area DPMI lock 一次性 sentinel |

#### AIL pan-volume LUT (`0x5360C..0x5368B`, 128B)

`data_ail_pan_volume_lut_128b` — sample-mixing pan curve byte[128]。雙向存取：
- 正 pan (LEFT)：`(&LUT)[pan]` 從 base+pan 讀
- 負 pan (RIGHT)：`(&LUT_END)[-pan]` 從 0x5368B-pan 讀（同陣列反向）

由 `AIL_internal_build_pan_volume_table @ 0x3FEF0` 消費（pan_curve × volume → per-channel signed-int gain entries）。儲存在 .data 不是 .rodata，因為某些 init path 可能會根據 user prefs 重建 curve。

#### AIL digital mixer + VOC dispatcher sentinels (`0x5368C..0x53697`, 12B)

| Addr | Type | Name | 用途 |
|---|---|---|---|
| `0x5368C` | uint | `data_ail_audio_mix_isr_state_dword` | mixer-loop state (多寫入 site at audio_mix_isr) |
| `0x53690` | uint | `data_ail_audio_mix_isr_dpmi_init_done_sentinel` | clock+mix init 一次性 sentinel |
| `0x53694` | uint | `data_ail_voc_dispatcher_dpmi_lock_done_sentinel` | VOC sample dispatcher DPMI lock sentinel |

#### AIL MDI state + timbre packet (`0x5371C..0x5376F`, 84B)

| Addr | Type | Name | 用途 |
|---|---|---|---|
| `0x5371C` | uint | `data_ail_mdi_state_init_done_sentinel` | MDI driver state array DPMI lock sentinel |
| `0x53720` | byte[80] | `data_ail_midi_timbre_install_packet_buffer_80b` | MIDI driver request packet template; 12B header + 68B trailing; 初始 "TIMB" magic + size + version |

### AIL extraction implications

整個 0x52A54..0x5376F 範圍（~3.6KB）必須完整納入 `ail_data.bin` extraction。
每個 sub-cluster 對應一個 .obj 內的 BSS section（zero-init）或 .data section
（pre-initialized template，如 timbre packet 與 pan_volume_lut 與 GTL filename
prefix）。emit C source 等價：

```c
// SoA timer manager (AIL_TIMER.OBJ BSS):
static code *ail_timer_slot_callback[16];
static uint ail_timer_slot_state[16];
static uint ail_timer_slot_elapsed[16];
static uint ail_timer_slot_period[16];
static uint ail_timer_slot_pending_trigger[15];  // exactly 15, NOT 16
static uint ail_isr_nested_pending_count;        // occupies the 16th slot
static uint ail_timer_slot_user_data[16];
// AIL ISR ancillary state (AIL_TIMER.OBJ BSS):
static uint ail_timer_isr_pit_period;
static ushort ail_isr_saved_orig_vector_selector;
static uint ail_isr_saved_orig_vector_offset;
// ... (continued through 0x52BFB)
// USE16 ISR stack (AIL_USE16.OBJ BSS, 2552B):
static byte ail_use16_isr_stack[2552];
// pan_volume LUT (AIL_MIX.OBJ .data, pre-init):
static byte ail_pan_volume_lut[128] = { ... };  // 128 entries
// MIDI timbre packet template (AIL_MDI.OBJ .data, pre-init):
static byte ail_midi_timbre_packet[80] = { 0x54, 0x49, 0x4D, 0x42, ..., 0xFF, 0xFF, 0x00, ... };
```

新增字串完整邊界須與 `0x53698 ail_gtl_filename_prefix[128]`（已記載於上）合
併視為連續區塊；emit 順序須 byte-exact 對應 .obj 內 declaration order。

### AIL handle pool boundary pointers (`0x54170..0x54177`)

8-byte 連續 BSS-style 指標 pair，AIL 內部 handle pool（sample / sequence /
timer / driver handle 的 slab 區）的起迄邊界。被 ~95 個 AIL_* API 共用作
handle slot 走訪邊界。

| Addr | Name | 用途 |
|---|---|---|
| `0x54170` | `data_ail_sample_handle_pool_begin_ptr` | handle pool 起始指標（slab base） |
| `0x54174` | `data_ail_sample_handle_pool_end_ptr` | handle pool 結束指標（slab limit） |

Writers (init at AIL_startup):
- `AIL_startup` (entry @ 0x379ee) — body 內位址 0x37a01 / 0x37a35 / 0x379fb / 0x37b56 為 slab alloc + 設定 begin/end boundary 的 instruction sites
- `AIL_internal_init_globals_once` (entry @ 0x3783c) — body 內位址 0x3786d / 0x3785e DATA ref sites，alternative init path

Readers (~95 sites parallel for both pointers):
- 整套 sample API: alloc_sample_handle / init_sample / set_sample_* / start/stop/end/resume/release_sample / sample_status / sample_position / sample_volume / sample_pan / sample_loop_count / sample_playback_rate / set_sample_type / set_sample_file
- 整套 sequence API: alloc_sequence_handle / init_sequence / start/stop/end_sequence / set_sequence_volume / set_sequence_loop_count / set_sequence_tempo / branch_index / map_sequence_channel
- 整套 timer API: register_timer / set_timer_* / start_timer / stop_timer / release_timer_handle / release_all_timers / interrupt_divisor
- 整套 driver API: install_driver / uninstall_driver / install_DIG/MDI_INI / install_DIG/MDI_driver_file / call_driver / install_DIG_driver_image / uninstall_DIG_driver
- channel API: lock_channel / release_channel / install_timbre
- infra: set_real_vect / get_real_vect / set_USE16_ISR / restore_USE16_ISR / set_preference / set_timer_period / delay / API_read_INI / get_IO_environment

AIL extraction 時：
- emit_action: `link_vendor` — 兩個指標納入 AIL .obj 的 BSS section (8 byte zero-init),
  不寫 PUBDEF（純 AIL 內部 state，FD2 game-side 不直接讀寫）
- C source 等價: `static void *ail_sample_handle_pool_begin; static void *ail_sample_handle_pool_end;`
  emit 順序須 byte-exact 對應 0x54170..0x54177 layout
- 這兩個指標在 AIL startup 時指向 AIL 內部 malloc 的 handle slab；slab size 由 AIL_install_DIG_INI / MDI_INI 內估算決定

注：FD2.LE 內 0x54170/74 是 AIL 在編譯時的 .obj BSS 一部分，被 wlink 放在 `.object3` segment（與 game .object3 globals interleave）。

### AIL `.object1` inline data items

下列分布在 `.object1` 內、與 AIL 函式邊彼此 interleave 的 read-only 常數 /
dispatch table。全部 emit_action = `emit_fd2_source` (AIL-built 但 wlink 把
table 放回 obj1 code segment 旁，FD2 自家 re-build 須能精確重現 byte layout)。

#### AIL DIG mixer dispatch tables（pair）

| Addr | Name | Type | 用途 |
|---|---|---|---|
| `0x47638` | `data_ail_dig_mixer_format_finaliser_dispatch_table_a` | pointer[128] (512B) | 60 非零 pointers — 由 `AIL_internal_mix_dispatch_format` 用 `data_ail_mix_format_flags` 為 index 呼叫，作每 mix-batch 結束後 32-bit accumulator → 實際 DMA output format 的轉換 / clamp / byte-swap |
| `0x47838` | `data_ail_dig_mixer_per_sample_dispatch_table_b` | pointer[128] (512B) | 72 非零 pointers — 由 `AIL_internal_mix_dispatch_sample` 用同 index per source sample 呼叫，作 format-aware copy / pitch-shift / pan / signed-conversion |

`data_ail_mix_format_flags` 為 8-bit composite key：bit 0..3 = output bit-width / signedness、+0x08 = planar、+0x10 = stereo、+0x20 = quad、+0x40 = ulaw。共 256 種 flag combination 中 60+72 = 132 個合法配對；其他 124 個 slot 為 NULL (= 不支援格式)。LE FIXUP 只 emit 非零 slot 的 record。

`AIL_internal_register_mix_globals` 透過 `fd2_dpmi_lock_region(0x47638, 0x495ff)` 鎖整段 0x47638..0x495FF (含 dispatch table + 132 個 mix callback function bodies)。AIL extraction 時須**保留** dispatch table byte layout（非零 slot fixup 對 wlink relink 可正確還原）。

#### AIL DIG / VOC driver dispatch tables

| Addr | Name | Type | 用途 |
|---|---|---|---|
| `0x40334` | `data_ail_dig_driver_configure_format_swap_jump_table` | pointer[4] (16B) | `AIL_internal_dig_driver_configure` 內 format-swap switch (uVar2 ∈ 0..3 from prefs `_28*2 | _32`)，4 個 case-body 填入 local_34[0..3] 為 4 種 permutation (identity / swap-pairs / swap-halves / full-reverse) |
| `0x40344` | `data_ail_dig_driver_configure_channel_init_jump_table` | pointer[4] (16B) | 同函數 channel-init switch (param[6] ∈ 0..3) 為 channel-format 寫入 samples-per-block + channel multiplier (8m / 8s / 16m / 16s) |
| `0x40354` | `data_align_40354_lea_nop_pad_12b` | byte[12] | alignment NOPs (2× 6-byte LEA) 對齊下個 fn @0x40360 |
| `0x41548` | `L_AIL_min_sample_buf_switchtable_41548` | pointer[4] (16B) | `AIL_internal_minimum_sample_buffer_size_inner` 內 format switch (`format` ∈ 0..3)，case-body 設 in_EDX = bytes-per-sample 1/2/2/4 |
| `0x41558` | `data_align_41558_lea_nop_pad_8b` | byte[8] | alignment NOPs (6-byte LEA + 2-byte MOV EDX,EDX) 對齊下個 fn @0x41560 |
| `0x4180c` | `data_ail_voc_dispatcher_v2_chunk_type_jump_table` | pointer[10] (40B) | `AIL_internal_voc_dispatcher_v2` 內 VOC chunk-type switch (0..9)；含 3 個 slot (continuation / silence / text) 共享 no-op handler 0x41a72，其餘 7 個 unique handler |
| `0x45120` | `data_ail_dig_pitch_bend_freq_scale_lookup_uint32_127` | uint32[127] (508B) | `AIL_internal_dig_apply_pitch_bend` 內 lookup table，index ∈ [0, 0x7F]；3 read sites (base_attack / velocity-with-attack / divisor)；單調遞增 quasi-exponential curve (8, 17, 18, ..., 98 at idx 0..31)。Used in pitch/velocity-to-sample-rate conversion |

#### AIL `.object1` AIL state / alignment items

| Addr | Name | Size | 用途 |
|---|---|---|---|
| `0x41dbf` | `data_align_41dbf_inter_fn_zero_byte` | 1B | inter-fn alignment padding between `AIL_internal_dpmi_unlock_voc_dispatcher_41d91` and `AIL_internal_init_mdi_state_arrays` |
| `0x499fa` | `data_align_499fa_inter_block_zero_word` | 2B | alignment between `__unhook387` RET (0x499f9) and AIL post-function data block |

AIL extraction 時這些 alignment byte sequences 須 byte-preserve（wlink 會自動補但 source 端不需 emit specific declaration）。

#### 對應的 caller function（AIL extraction 須一併帶入）

| Function | Addr | 主要 access |
|---|---|---|
| `AIL_internal_mix_dispatch_format` | 0x495a0 (entry) | reads table A @ 0x47638 |
| `AIL_internal_mix_dispatch_sample` | 0x49340 | reads table B @ 0x47838 |
| `AIL_internal_register_mix_globals` | 0x49611 (entry) | DPMI-locks 0x47638..0x495FF and 0x538a0..0x538c0 |
| `AIL_internal_dig_driver_configure` | 0x40409 (xref site) | reads tables 0x40334 + 0x40344 |
| `AIL_internal_minimum_sample_buffer_size_inner` | 0x41560 | reads table 0x41548 |
| `AIL_internal_voc_dispatcher_v2` | 0x41834 (entry) / 0x4185a (table dispatch) | reads table 0x4180c |
| `AIL_internal_dig_apply_pitch_bend` | 0x45617 (entry) | reads table 0x45120 |
| `ail_internal_write_buffer_to_file_create` | 0x36a02 (new) | uses data_ail_last_error_code; open flags 0x262 |
| `ail_internal_write_buffer_to_file_truncate` | 0x36a71 (new) | uses data_ail_last_error_code; open flags 0x212 |

## Build pipeline 草案

```
1. tools/le_unpack/le_unpack.py  → object{1,2,3}.bin (raw byte images)
2. 新工具 extract_ail.py（從 Ghidra MCP 拉 AIL function 清單 + body bytes）
     → ail_code.bin: AIL functions byte image (concatenated)
     → ail_data.bin: AIL globals byte image (subrange of obj2)
     → ail_fixups.json: fixup records 從 LE format 抽出 + disasm 補
       同 obj1 內 E8/E9 disp32 的合成條目
3. tools/le_unpack/bin_to_omf.py (extended)
     → ail_<NN>_<group>.obj × ~30：SEGDEF AIL_CODE + AIL_DATA、
       PUBDEF for 106 public、EXTDEF for CLIB3S 9.5a 真符號 + 8 個
       FD2-side helper、FIXUPP32 records translated from LE fixups + 合成
4. DOSBox-X 內 wlib (9.5a) -b -t -q ailv3.lib +ail_*.obj
5. DOSBox-X 內 wcc386 -bt=dos -mf my_main.c → my_main.obj
6. DOSBox-X 內 wlink ailv3.lib my_main.obj clib3s.lib → poc.exe
7. 在 DOSBox-X 跑 → 驗證 OPL3 出聲
```

跑 build pipeline 之前的前置：
- 對 8 個 FD2-side helper（6 fd2_dpmi_* + 2 crt_equivalent_get_eflags）改寫
  為 FD2 source 端 emit；AIL 端走 EXTDEF reference
- 對 8 個 `__watcall` 例外函式在 client header 標對 calling convention
- 對 14 個 dead_code_stub 標 weak 或排除
