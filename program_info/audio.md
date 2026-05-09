# audio

FD2 的音效系統使用 Sonic Foundry 的 **Miles Sound System** (AIL = Audio
Interface Library)，是 1990 年代 DOS 遊戲業界標準 (Warcraft II / Fallout /
Command & Conquer 等用同一套)。所有 `AIL_*` 函式從 AIL3DIG / AIL3MDI 靜態
library 連進 FD2.LE。

`SETSOUND.EXE`（FLAME2 目錄裡的工具）就是 AIL 的設定程式，會產生 `.MDI` /
`.DIG` driver 設定。

## AIL function inventory

FD2.LE 內 AIL ecosystem 共 287 個 function：

| 群組 | 數量 | 命名 / 進入點 |
|---|---:|---|
| 公開 API | 103 | `AIL_*`（非 `AIL_internal_*`），每個都有 self-print `fprintf(log, "AIL_xxx(...)\n", ...)` |
| 由 public BFS 可達的 internal | 144 | `AIL_internal_*` worker / ISR / log / timer / DIG mixer / MDI sequence / XMIDI parser，以及 54 對 vendor-internal log-wrapped API 的 inner pair |
| BFS-reached 的 promoted internal | 4 | 從 `crt_*` 改名（`alloc_and_commit` / `decommit_and_free` / `load_file_to_memory` / `parse_int_with_base`；caller 全 AIL + 引用 AIL global） |
| BFS 不可達的 internal | 36 | 11 vtable_indirect + 9 cluster_member + 1 tail_call_target + 15 dead_code_stub；plate comment 註明 sub-case。內含 5 個從 `crt_*` reclassify 的（3 個 dpmi_unlock 特定 AIL region + `wave_synth_program_lookup_454fd` byte-level 確認非 CLIB3S + 其 wrapper `_program_exists_456a5`） |

被 AIL 使用但確認為 Watcom CRT primitive 的 helper 保留 `crt_*` 命名（不抽進
AIL，build pipeline 經 EXTDEF 由 v2 CLIB 解析）共 10 個：6 個 DPMI 原語
（`crt_dpmi_lock_region` / `_unlock_region` / `_lock_size` / `_unlock_size` /
`_alloc_dos_memory` / `_free_dos_memory`，其中 `_lock_size` 直接被 game
`set_bgm_track_with_fade` 呼叫）、`crt_filesize_path`（open + filelength + close
組合）、2 個 abort helper（`crt_abort_with_log` / `crt_abort_thunk`，caller path
含 `__prtf` / `__STKOVERFLOW`）、`crt_get_eflags`（4-byte `pushfd; pop eax; cli;
ret` = Watcom `_disable` primitive）。

per-function 對照表可由 `tools/ail_audit/build_inventory.py` 重新產生（讀
Ghidra 當前狀態與 call graph）；三向誤分類 audit（正向 xref-source / 反向
caller-set / orphan disasm 三道交叉驗證）的 reusable script 與分類偵測規則
寫在 `tools/ail_audit/`（見該目錄 `_index.md`）。

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

## Static-link duplicates

3 對 helper 因 AIL3DIG 與 AIL3MDI 兩個靜態 .obj 各帶一份而被連結兩次，body
相同但位於不同 address。Ghidra 接受同名 function（unique key 是 name+address），
保留兩份原名而不加 suffix：

| 名稱 | 5-byte 5-byte stub copy | 8-instr full copy |
|---|---|---|
| `AIL_log_lock_acquire` | `0x37D1A` | `0x3E724` |
| `AIL_log_lock_release` | `0x37D1F` | `0x3E731` |
| `AIL_get_isr_lock_count` | `0x3810E` | `0x3EEDA` |

## 命名的 46 個 AIL 函式 (按位址排序)

### 系統啟動/關閉

- `0x000379EE` `AIL_startup`
- `0x00037B88` `AIL_shutdown`
- `0x00037C20` `AIL_set_preference`

### 中斷 / 底層 I/O

- `0x00037D24` `AIL_get_real_vect`
- `0x00037E0F` `AIL_set_real_vect`
- `0x00037F12` `AIL_restore_USE16_ISR`
- `0x00037F99` `AIL_call_driver`
- `0x000380A1` `AIL_delay`
- `0x00038113` `AIL_API_read_INI`

### Timer

- `0x0003846C` `AIL_register_timer`
- `0x00038557` `AIL_set_timer_user`
- `0x0003864A` `AIL_set_timer_period`
- `0x000386C0` `AIL_set_timer_frequency`
- `0x00038889` `AIL_start_timer`
- `0x00038958` `AIL_stop_timer`
- `0x00038A27` `AIL_release_timer_handle`
- `0x00038A94` `AIL_release_all_timers`

### Driver 管理

- `0x00038AF6` `AIL_get_IO_environment`
- `0x00038BDB` `AIL_install_driver`
- `0x00038CCE` `AIL_uninstall_driver`

### DIG (數位音效)

- `0x00038D3B` `AIL_install_DIG_INI`
- `0x00038E26` `AIL_install_DIG_driver_file`
- `0x00038F80` `AIL_allocate_sample_handle`
- `0x00039164` `AIL_release_sample_handle`
- `0x000391D1` `AIL_init_sample`
- `0x00039344` `AIL_set_sample_address`
- `0x000393C6` `AIL_set_sample_type`
- `0x00039448` `AIL_start_sample`
- `0x000394B5` `AIL_stop_sample`
- `0x000395FC` `AIL_set_sample_playback_rate`
- `0x00039672` `AIL_set_sample_volume`
- `0x000396E8` `AIL_set_sample_pan`
- `0x0003975E` `AIL_set_sample_loop_count`

### MDI (MIDI 音樂)

- `0x0003A722` `AIL_install_MDI_INI`
- `0x0003A7F9` `AIL_install_MDI_driver_file`
- `0x0003A953` `AIL_allocate_sequence_handle`
- `0x0003AAA5` `AIL_init_sequence`
- `0x0003AB9E` `AIL_start_sequence`
- `0x0003AC0B` `AIL_stop_sequence`
- `0x0003ACE5` `AIL_end_sequence`
- `0x0003ADD4` `AIL_set_sequence_volume`
- `0x0003AE56` `AIL_set_sequence_loop_count`

### MIDI 通道 / 其他

- `0x0003BA8F` `AIL_branch_index`
- `0x0003C18B` `AIL_lock_channel`
- `0x0003C270` `AIL_release_channel`
- `0x0003C2E6` `AIL_map_sequence_channel`

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

- `AIL_init_globals_once` — once-only init guard，多 entry 統一進入
- `AIL_init_runtime_defaults` — runtime 初始 config (e.g. ISR lock counter 清 0)
- `AIL_init_state_arrays` — 通用 state slot table 初始化
- `AIL_init_mdi_state_arrays` — MDI-specific state 初始化
- `AIL_register_state_globals` / `AIL_register_mix_globals` — 把 state /
  mixer global 指標表 register 進 driver dispatch
- `AIL_parse_INI_driver_config` — 從 `.MDI` / `.DIG` INI 解析 driver 參數
- `AIL_timer_alloc_slot` — 配發空 timer slot

### Logging & ISR re-entry guard (5 個)

- `AIL_log_lock_acquire` / `AIL_log_lock_release` — debug-print 互斥旗 (2 對 copy 見上節)
- `AIL_log_print_timestamp_prefix` / `AIL_log_decrement_nesting` —
  printf prefix 與 nesting 計
- `AIL_get_isr_lock_count` — ISR re-entry counter，public function 在 ISR 內
  時略過 debug 工作（1 對 copy 見上節）

### Timer / PIT helpers (5 個)

- `AIL_set_pit_divisor` / `AIL_recompute_pit_divisor` — 直接寫 8254 PIT
- `AIL_get_interrupt_divisor` / `AIL_set_timer_divisor_inner` — 從 frequency
  推 divisor
- `AIL_busy_wait_vsync` — VGA vertical retrace 同步
- `AIL_uninstall_timer_isr` — ISR vector 還原

### DIG mixer / playback engine (10 個)

- `AIL_dig_driver_setup_full` / `AIL_dig_driver_configure` /
  `AIL_dig_apply_io_parms` — driver 一次性設定
- `AIL_dig_apply_pitch_bend` / `AIL_dig_apply_sample_volume_pan` —
  per-sample runtime 控制
- `AIL_clear_dma_buffer` — DMA buffer 清 0
- `AIL_build_pan_volume_table` — pan + volume → 8-bit/16-bit lookup table
- `AIL_mix_dispatch_format` / `AIL_mix_dispatch_sample` /
  `AIL_mix_loop_8bit_stereo` — sample-format-specific mixer

### MDI / sequence engine (12 個)

- `AIL_mdi_driver_setup_full` / `AIL_mdi_apply_io_parms` — driver 設定
- `AIL_midi_send_message` / `AIL_midi_flush_pending` — 送 MIDI byte 到 driver
- `AIL_sequence_controller_write` / `AIL_sequence_handle_midi_event` — 處理
  sequence 事件
- `AIL_sequence_send_volumes` / `AIL_sequence_silence_active_notes` /
  `AIL_sequence_reset_state` / `AIL_sequence_restore_channel_state` /
  `AIL_sequence_release_channel_inner` — channel state 操作

### XMIDI 解析 (3 個)

- `AIL_xmidi_find_chunk` — 在 .XMI byte stream 中找到 EVNT chunk
- `AIL_xmidi_read_vlq` — variable-length quantity 解
- `AIL_xmidi_handle_meta_event` — meta-event (tempo / loop) 處理

## FD2 自寫的 BGM 派遣器

`set_bgm_track_with_fade @ 0x25977` — FDMUS 音樂的 dispatcher。

- `track_id` 經 ECX register 傳入 (Watcom 4-arg register convention)
- `track_id` 直接對應 FDMUS idx (沒有 lookup table)
- `track_id == 0xFFFFFFFF` → 4 秒 fade-out 然後停
- `track_id == 0x10` 或 `0x11` → 立即音量切換 (無 fade-in)
- 主選單 BGM = 0x12 (在 `fd2_main`)

每章兩個 BGM 表：

- `per_chapter_player_turn_bgm[30] @ 0x51E63` — 每章玩家回合 BGM track_id
- `per_chapter_enemy_turn_bgm[30] @ 0x51E81` — 每章敵方回合 BGM track_id
- `run_full_turn_cycle` 把這兩個 byte cast 為 BGM track_id 餵給 `set_bgm_track_with_fade`

## SFX 觸發

`play_sfx_with_handle @ 0x25A96` — 通用 AIL SFX 播放器，UI 各處呼叫
(cursor 移動 sfx 0、確認 sfx 7、取消等)。3 個 gate flag：
`audio_master_enable @ 0x53EF1`、`sample_system_ready @ 0x51E62`、
`audio_mute_flag @ 0x540FF`。

## DOS-side driver 檔案 (與 AIL 配對)

| MDI files        | 用途                               |
| ---------------- | ---------------------------------- |
| `ADLIB.MDI`    | AdLib card                         |
| `ADLIBG.MDI`   | AdLib Gold                         |
| `MPU401.MDI`   | Roland MPU-401                     |
| `MT32MPU.MDI`  | Roland MT-32                       |
| `OPL3.MDI`     | OPL3 (Sound Blaster Pro 2 / AWE32) |
| `PAS.MDI`      | Pro Audio Spectrum                 |
| `PASPLUS.MDI`  | Pro Audio Spectrum Plus            |
| `PCSPKR.MDI`   | PC Speaker                         |
| `SBAWE32.MDI`  | Sound Blaster AWE32                |
| `SBLASTER.MDI` | Sound Blaster                      |
| `SBPRO1.MDI`   | Sound Blaster Pro 1                |
| `SBPRO2.MDI`   | Sound Blaster Pro 2                |
| `SNDSCAPE.MDI` | SoundScape                         |
| `TANDY.MDI`    | Tandy                              |
| `ULTRA.MDI`    | Gravis UltraSound                  |
| `NULL.MDI`     | No MIDI output                     |

| DIG files        | 用途                       |
| ---------------- | -------------------------- |
| `ADRV688.DIG`  | Covox/SoundMaster          |
| `JAMMER.DIG`   | Jammer                     |
| `PROAUDIO.DIG` | Pro Audio Spectrum digital |
| `RAP10.DIG`    | Roland RAP-10              |
| `SB16.DIG`     | Sound Blaster 16           |
| `SBLASTER.DIG` | Sound Blaster digital      |
| `SBPRO.DIG`    | Sound Blaster Pro digital  |
| `SNDSCAPE.DIG` | SoundScape digital         |
| `ULTRA.DIG`    | Gravis UltraSound digital  |

`AILDRVR.LST` 是 driver 清單，`SAMPLE.AD/OPL/BNK` 是 instrument patch。

## Library 邊界

`.object1` 範圍 `0x37000-0x3C963` 幾乎完全是 AIL library：

- 第一個 AIL 函式 @ `0x379EE`
- 最後一個 @ `0x3C2E6`
- 約 19 KB library 程式碼
- entry point @ `0x3C964` 在這段 AIL 集中區之後（Watcom linker 把 library code
  放在 main 之前；但要注意 game logic / library 在 `.object1` 中整體仍是
  interleaved，0x37000-0x3C963 只是 AIL 函式較密集的觀察區段）
