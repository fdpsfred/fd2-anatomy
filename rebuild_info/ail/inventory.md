# AIL library inventory

FD2 的音效系統使用 Sonic Foundry 的 **Miles Sound System** (AIL = Audio
Interface Library)，是 1990 年代 DOS 遊戲業界標準 (Warcraft II / Fallout /
Command & Conquer 等用同一套)。所有 `AIL_*` 函式從 AIL3DIG / AIL3MDI 靜態
library 連進 FD2.LE。

`SETSOUND.EXE`（FLAME2 目錄裡的工具）就是 AIL 的設定程式，會產生 `.MDI` /
`.DIG` driver 設定。

本檔記錄 FD2.LE 內 AIL ecosystem 的 428 個 function inventory、靜態鏈接 thunk
配對、命名規約、library 邊界。FD2 自寫的 game-side audio 層（BGM
dispatcher、SFX trigger）與 DOS-side driver / patch 檔清單見
`program_info/audio.md`。

## AIL function inventory

FD2.LE 內 AIL ecosystem 共 428 個 function：

| 群組 | 數量 | 命名 / 進入點 |
|---|---:|---|
| 公開 API | 106 | `AIL_*`（非 `AIL_internal_*`），每個都有 self-print `fprintf(log, "AIL_xxx(...)\n", ...)` |
| 由 public BFS 可達的 internal | 144 | `AIL_internal_*` worker / ISR / log / timer / DIG mixer / MDI sequence / XMIDI parser，以及 54 對 vendor-internal log-wrapped API 的 inner pair |
| DIG mixer dispatch table callbacks | 132 | `AIL_internal_mix_finalize_<idx>` (60) + `AIL_internal_mix_loop_<idx>` (72)；indirect dispatch via `AIL_internal_mix_dispatch_format` / `AIL_internal_mix_dispatch_sample` (詳見下方 DIG mixer dispatch tables 段落) |
| BFS-reached 的 promoted internal | 4 | `AIL_internal_alloc_and_commit` / `AIL_internal_decommit_and_free` / `AIL_internal_load_file_to_memory` / `AIL_internal_parse_int_with_base`（caller 全 AIL + 引用 AIL global） |
| BFS 不可達的 internal | 39 | 11 vtable_indirect + 8 cluster_member + 1 tail_call_target + 14 dead_code_stub + 5 reclassify-from-crt (`AIL_internal_timer_isr_master @ 0x3e73e` (29 個 AIL globals；AIL 主時鐘 ISR，掃 16 個 timer slot + PIC EOI + nested-ISR 重入防護) / `AIL_internal_use16_isr_3eaa8` 被 `AIL_internal_set_USE16_ISR_inner` 透過 DATA xref 註冊為 USE16 ISR / `AIL_internal_use16_isr_iret_tail @ 0x3eaf2` 是 `AIL_internal_use16_isr_3eaa8` 的 RETF→IRETD 後尾巴 / `AIL_internal_dpmi_use32_save_jmp_3ed71` + `AIL_internal_dpmi_use32_restore_jmp_3eda7` stack-switch 配對，位於 AIL ISR 區段 zero xref)；plate comment 註明 sub-case。內含 5 個（3 個 dpmi_unlock 特定 AIL region + `AIL_internal_wave_synth_program_lookup_454fd` byte-level 確認非 CLIB3S + 其 wrapper `_program_exists_456a5`） |

被 AIL 使用的 helper 詳細歸類與 emit 路徑見 `./extraction_prep.md` 「AIL 共用 /
邊界 helper（後續 reclassify 結果）」段。摘要：6 個 DPMI 原語為 FD2 自寫
（`fd2_dpmi_alloc_dos_memory` / `_free_dos_memory` / `_lock_region` /
`_unlock_region` / `_lock_size` / `_unlock_size`，其中 `fd2_dpmi_lock_size` 直接被
game `fd2_set_bgm_track_with_fade` 呼叫）；`AIL_internal_filesize_path` 與
`AIL_get_last_error_code` 屬 AIL pool；`__FpAbort @ 0x46b41` 走 Watcom CLIB
EXTDEF；`crt_equivalent_get_eflags` + `_thunk` 為 `_disable` primitive 行為等價
但 byte 不 match。

per-function 對照表透過 Ghidra MCP `search_functions(name_pattern="^AIL_")`
即時拉取；三向誤分類 audit（正向 xref-source / 反向 caller-set / orphan disasm
三道交叉驗證）已執行完畢，分類結果落地在 Ghidra plate comment 內。

## AIL 函式辨識方式

每個 **AIL 公開 entry-point** 啟動時會走 `AIL_DEBUG` flag 檢查並印出如
`"AIL_startup()\n"` 的字串，binary 內這些字串可精確定位每個 entry function。
FD2 自身直接呼叫的 AIL entry-point 有 46 個（FD2 game-logic 端的 caller）；
另有 54 個 AIL public API 是 vendor library 內部互呼但 FD2 source 端從不呼叫
的 logged entry，全部以「`fprintf(log, "AIL_xxx(...)\n", ...)` + 呼叫
`AIL_xxx_inner` 實作 + decrement nesting」三段式組成（透過 fprintf format
string 自動命名）。AIL 內部 helper（vendor 自己的私有 worker function、ISR
相關、format-specific mixer routine 等）沒有 debug printf 字串，命名依
callees / data ref / 結構推敲決定，全部完整命名為 `AIL_internal_<descriptor>`
或 `AIL_internal_<X>_inner` — 不留 `_helper_<addr>` 形式的 placeholder。

剩餘 dead-code stub function（Ghidra 把它們切成獨立 function 並命名，但無 caller）：
`AIL_resume_sample @ 0x39522` / `AIL_set_sequence_tempo @ 0x3AD52`。另兩個
完全沒 caller 也沒 xref 的 driver dispatch trampoline 構成 start/stop pair：
`AIL_internal_driver_start_output_3fe6b @ 0x3FE6B`（`AIL_call_driver(drv, 0x401)`
配 `drv->state[0x15]: 0→1`）與 `AIL_internal_driver_stop_output_3feb3 @ 0x3FEB3`
（`AIL_call_driver(drv, 0x402)` 配 `drv->state[0x15]: 1→0`），linker 從
AIL3DIG/AIL3MDI 帶入但 binary 從未引用。emit pipeline 連結 Watcom AIL 後對
binary 影響為 0。另有 57 個 function-name 字串沒被任何 code site 引用（dead code，
linker 帶入但 printf 整個被 elide），以及 2 個字串引用點落在已命名 AIL function
的 fall-through dead-code 區段：`AIL_resume_sample` 字串 @ 0x508A8 引用點
0x3956C 位於 `AIL_stop_sample @ 0x394B5` body 內，`AIL_set_sequence_tempo` 字串
@ 0x50CE8 引用點 0x3ADA7 位於 `AIL_end_sequence @ 0x3ACE5` body 內。

每個 entry-point 命名都透過 `FUN_0003f11b(..., "AIL_xxx(...)\n", ...)` debug
printf 親自驗證；當函式 body 含多個 AIL 字串引用時，**以 entry-point 第一條
printf 為準**（前述 2 段 dead-code stub 不可作為命名根據）。

逆向目標是辨認 AIL 層的邊界：看到 FD2 遊戲邏輯呼叫 `AIL_start_sequence(seq_handle)`
就理解意圖即可，AIL 內部邏輯不深究 (vendor SDK 文件可查)。

完整 46 個 FD2 直接呼叫的 AIL public API 位址與分群清單，透過 Ghidra MCP
`search_functions(name_pattern="^AIL_")` 即時 dump 取得；本檔不重複維護
人寫清單。

## Static-link thunk + body pairs

3 對 helper 因 AIL3DIG 與 AIL3MDI 兩個靜態 .obj 各帶一份而被連結兩次。一個是
**5-byte JMP thunk**（指向另一個的位址），一個是**完整 body**（PUSH/INC global/POP/RET 8 個 instr）。
兩份位於不同 address，bodies 不同（thunk vs full impl）：

| 邏輯名稱 | 5-byte JMP thunk addr | 8-instr full body addr | 當前 Ghidra 命名 (thunk / body) |
|---|---|---|---|
| `AIL_log_lock_acquire` | `0x37D1A` | `0x3E724` | `AIL_internal_log_lock_acquire` / `AIL_internal_log_lock_acquire_3e724` |
| `AIL_log_lock_release` | `0x37D1F` | `0x3E731` | `AIL_internal_log_lock_release` / `AIL_internal_log_lock_release_3e731` |
| `AIL_get_isr_lock_count` | `0x3810E` | `0x3EEDA` | `AIL_internal_get_isr_lock_count` / `AIL_internal_get_isr_lock_count_3eeda` |

Body 端加 `_<addr>` 後綴是為了在 Ghidra 命名空間中區分 thunk 與 body（兩者
都歸 `AIL_internal_*` 而非公開 `AIL_*`，因為 vendor 內部 helper 性質）。emit
pipeline 連結 Watcom AIL3DIG/AIL3MDI 後，這兩份依靜態 link 順序自然出現
in-binary，FD2 source 端不重 emit。

## Vendor-internal AIL public API (54 對 wrapper + inner)

AIL3DIG / AIL3MDI 靜態 library 內共 54 個 logged public API（FD2 source 端不
直接呼叫，但 vendor library 內部呼叫鏈會走到，且每個都有自己的 `AIL_xxx(...)\n`
log printf）。每個 wrapper 的名稱由其 fprintf format string 抽取得出，並配對其
對應 `AIL_xxx_inner` 實作（47 對有獨立 inner function；其餘 inner 為已命名
vendor helper 或共用 implementation）。

涵蓋類別：

- **Sample lifecycle / I/O** — `AIL_set_USE16_ISR` / `AIL_uninstall_DIG_driver` /
  `AIL_allocate_file_sample` / `AIL_set_sample_file` / `AIL_sample_status` /
  `AIL_sample_playback_rate` / `AIL_sample_volume` / `AIL_sample_pan` /
  `AIL_sample_loop_count` / `AIL_install_DIG_driver_image` /
  `AIL_minimum_sample_buffer_size` / `AIL_sample_buffer_ready` /
  `AIL_load_sample_buffer` / `AIL_set_sample_position` / `AIL_sample_position` /
  `AIL_register_SOB_callback` / `AIL_register_EOB_callback` /
  `AIL_register_EOS_callback` / `AIL_register_EOF_callback` /
  `AIL_set_sample_user_data` / `AIL_sample_user_data` / `AIL_active_sample_count`
- **Timer** — `AIL_set_timer_divisor` / `AIL_interrupt_divisor`
- **Sequence lifecycle** — `AIL_uninstall_MDI_driver` /
  `AIL_release_sequence_handle` / `AIL_resume_sequence` / `AIL_sequence_status` /
  `AIL_sequence_tempo` / `AIL_sequence_volume` / `AIL_sequence_loop_count` /
  `AIL_install_MDI_driver_image` / `AIL_set_GTL_filename_prefix` /
  `AIL_active_sequence_count` / `AIL_sequence_position` /
  `AIL_set_sequence_user_data` / `AIL_sequence_user_data`
- **Timbre** — `AIL_timbre_status` / `AIL_install_timbre` / `AIL_protect_timbre` /
  `AIL_unprotect_timbre`
- **Channel events** — `AIL_controller_value` / `AIL_channel_notes` /
  `AIL_register_prefix_callback` / `AIL_register_trigger_callback` /
  `AIL_register_sequence_callback` / `AIL_register_event_callback` /
  `AIL_register_timbre_callback` / `AIL_register_ICA_array` /
  `AIL_true_sequence_channel` / `AIL_send_channel_voice_message` /
  `AIL_send_sysex_message`
- **Wave synthesizer** — `AIL_create_wave_synthesizer` /
  `AIL_destroy_wave_synthesizer`

每個 wrapper 的 plate comment 紀錄 fprintf 模板與 inner function 配對；inner
的 plate comment 反過來指向 wrapper。emit pipeline 階段這些函式全由 vendor
relink 解析，不需要 FD2 自寫實作。

## AIL 內部 helper 函式 (~93 個)

AIL library 內部 helper、ISR / timer / mixer / sequence worker。命名以「entry
function 對應 worker → `_inner` 後綴」與「私有功能 → 動詞短語」為原則。所有
這些 helper 在 emit pipeline 階段不會以 FD2 source 形式重新輸出，**Watcom
linker 直接 link AIL3DIG / AIL3MDI** 即可解析。下列分類用於閱讀導引，非
strict subsystem boundary（多數 helper 會被多個 entry-point 共用）。

### Public entry-point inner workers (`*_inner` 後綴, ~38 個)

每個 public AIL function 多半把實作切到一個 inner worker，public function 只負責
debug printf + 參數轉發。常見 pattern：`AIL_xxx` body = `printf("AIL_xxx()\n");
return AIL_xxx_inner(...)`。

涵蓋：`AIL_set_preference_inner`、`AIL_install_driver_inner`、
`AIL_uninstall_driver_inner`、`AIL_query_io_environment_inner`、
`AIL_call_driver_inner`、`AIL_get_int_vector_inner` /
`AIL_set_int_vector_inner` / `AIL_restore_isr_inner`、
`AIL_release_all_timers_inner`、`AIL_release_channel_inner` /
`AIL_lock_channel_inner` / `AIL_branch_index_inner` /
`AIL_map_sequence_channel_inner`、各 sample/sequence I/O setter `_inner`、各
timer setter `_inner`、`AIL_install_DIG_INI_inner` /
`AIL_install_DIG_driver_file_inner` / 同樣 MDI 版、`AIL_init_sample_inner` /
`AIL_init_sequence_inner` / `AIL_start_sample_inner` /
`AIL_start_sequence_inner` / `AIL_stop_sample_inner` /
`AIL_stop_sequence_inner` / `AIL_end_sequence_inner`、
`AIL_release_sample_handle_inner` / `AIL_allocate_sample_handle_inner` 同序列
版、`AIL_shutdown_inner`、`AIL_timer_release_slot_inner` /
`AIL_timer_set_user_inner`、`AIL_start_all_timers_inner` /
`AIL_stop_all_timers_inner` / `AIL_start_timer_inner` /
`AIL_stop_timer_inner`、`AIL_resume_sample_inner`、
`AIL_sequence_release_channel_inner`。

### Initialization & state globals (8 個)

- `AIL_internal_init_globals_once` — once-only init guard，多 entry 統一進入
- `AIL_internal_init_runtime_defaults` — runtime 初始 config (e.g. ISR lock counter 清 0)
- `AIL_internal_init_state_arrays` — 通用 state slot table 初始化
- `AIL_internal_init_mdi_state_arrays` — MDI-specific state 初始化
- `AIL_internal_register_state_globals` / `AIL_internal_register_mix_globals` — 把 state /
  mixer global 指標表 register 進 driver dispatch
- INI driver-config 解析 — 走 `AIL_API_read_INI` + `AIL_internal_API_read_INI_inner` 路徑
- Timer-slot 配發 — 透過 `AIL_register_timer` + slot table

### Logging & ISR re-entry guard (5 個)

- `AIL_log_lock_acquire` / `AIL_log_lock_release` — debug-print 互斥旗 (2 對 copy 見上節)
- `AIL_log_print_timestamp_prefix` / `AIL_log_decrement_nesting` —
  printf prefix 與 nesting 計
- `AIL_get_isr_lock_count` — ISR re-entry counter，public function 在 ISR 內
  時略過 debug 工作（1 對 copy 見上節）

### Timer / PIT helpers (5 個)

- `AIL_internal_set_pit_divisor` / `AIL_internal_recompute_pit_divisor` — 直接寫 8254 PIT
- `AIL_internal_set_timer_divisor_inner` — 從 frequency 推 divisor（public wrapper `AIL_interrupt_divisor` 經此 inner 執行）
- `AIL_internal_busy_wait_vsync` — VGA vertical retrace 同步
- `AIL_internal_uninstall_timer_isr` — ISR vector 還原

### DIG mixer / playback engine (10 個)

- `AIL_internal_dig_driver_setup_full` / `AIL_internal_dig_driver_configure` /
  `AIL_internal_dig_apply_io_parms` — driver 一次性設定
- `AIL_internal_dig_apply_pitch_bend` / `AIL_internal_dig_apply_sample_volume_pan` —
  per-sample runtime 控制
- `AIL_internal_clear_dma_buffer` — DMA buffer 清 0
- `AIL_internal_build_pan_volume_table` — pan + volume → 8-bit/16-bit lookup table
- `AIL_internal_mix_dispatch_format` / `AIL_internal_mix_dispatch_sample` —
  sample-format-specific mixer dispatch (132 callbacks 詳見下方 DIG mixer dispatch tables 段落)

### DIG mixer dispatch tables + 132 callbacks

兩個 function-pointer dispatch table 各 128 entries × 4 bytes = 512 bytes，
被 `AIL_internal_register_mix_globals @ 0x495FF` 用 `fd2_dpmi_lock_region` lock
住整個 `0x47638..0x495FF` mix-loop code+data 區段（DPMI page-lock 保證 ISR
ctx 不缺頁）。

| Table | 標籤 | base | dispatch 函式 | 角色 | valid entries |
|---|---|---|---|---|---:|
| A | `ail_dig_mix_finalize_table` | `0x47638` | `AIL_internal_mix_dispatch_format @ 0x49541` | 把 mix accumulator 寫到 driver 輸出 buffer 的 per-output-format finaliser | 60 |
| B | `ail_dig_mix_sample_table`   | `0x47838` | `AIL_internal_mix_dispatch_sample @ 0x49340` | 讀 source PCM、依 per-channel volume table 縮放、累加進 mix accumulator 的 per-sample-format inner mixer | 72 |

兩 table 共 use 同一個 `ail_mix_format_flags @ 0x538a0` (uint，bits 0..3 driver
格式碼 + bits 4..6 channel/pan/16-bit 標記) 當 7-bit index：`(table[flags])(...)`。
128 個 slot 中 132 個 valid (60 + 72)，其餘 NULL = 不支援的格式組合。

132 callback 命名規約：

- Table A entry [idx]: `AIL_internal_mix_finalize_<idx_2digit_hex>`
- Table B entry [idx]: `AIL_internal_mix_loop_<idx_2digit_hex>`
- 全 132 個 callback 統一以 idx 命名（包含 0x6f 的 8-bit stereo mixer entry 也使用 `AIL_internal_mix_loop_6f`，semantic info 寫在 plate comment）

每個 callback 的 plate comment 註明所屬 table、idx、dispatch 函式、DPMI lock 範圍。
callback semantic 為 inline PCM format converter (XOR 0x80/0x8000 sign flip、
saturation clip、stereo↔mono pack/unpack、8↔16 bit 轉換、stereo volume table lookup
+ accumulate)。

### MDI / sequence engine (12 個)

- `AIL_internal_mdi_driver_setup_full` / `AIL_internal_mdi_apply_io_parms` — driver 設定
- `AIL_internal_midi_send_message` / `AIL_internal_midi_flush_pending` — 送 MIDI byte 到 driver
- `AIL_internal_sequence_controller_write` / `AIL_internal_sequence_handle_midi_event` — 處理
  sequence 事件
- `AIL_internal_sequence_send_volumes` / `AIL_internal_sequence_silence_active_notes` /
  `AIL_internal_sequence_reset_state` / `AIL_internal_sequence_restore_channel_state` /
  `AIL_internal_sequence_release_channel_inner` — channel state 操作

### XMIDI 解析 (3 個)

- `AIL_internal_xmidi_find_chunk` — 在 .XMI byte stream 中找到 EVNT chunk
- `AIL_internal_xmidi_read_vlq` — variable-length quantity 解
- `AIL_internal_xmidi_handle_meta_event` — meta-event (tempo / loop) 處理

## DOS-side driver 檔案

FD2 隨片附的 `.MDI` / `.DIG` driver 與 `AILDRVR.LST` / `SAMPLE.AD/OPL/BNK`
instrument patch 屬於 game-shipped runtime 資源，清單見
`program_info/audio.md`。

## Library 邊界

AIL 函式在 `.object1` 內 **不是** 連續區段 — Watcom linker 把 AIL code 與
FD2 game / Watcom CRT interleave，[[project_function_interleave]] 描述同
現象。分類只能靠命名前綴 + caller/callee + content evidence，**不能用
address range 判定**。

統計觀察：第一個 AIL 函式 @ `0x379EE`、最後一個 @ `0x3C2E6`、共 ~19 KB
library 程式碼。entry point @ `0x3C964` 在這段密集區之後。
