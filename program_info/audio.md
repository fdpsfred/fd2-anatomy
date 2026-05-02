# audio

FD2 的音效系統使用 Sonic Foundry 的 **Miles Sound System** (AIL = Audio
Interface Library)，是 1990 年代 DOS 遊戲業界標準 (Warcraft II / Fallout /
Command & Conquer 等用同一套)。所有 `AIL_*` 函式從 AIL3DIG / AIL3MDI 靜態
library 連進 FD2.LE。

`SETSOUND.EXE`（FLAME2 目錄裡的工具）就是 AIL 的設定程式，會產生 `.MDI` /
`.DIG` driver 設定。

## AIL 函式辨識方式

每個 AIL 函式啟動時會走 `AIL_DEBUG` flag 檢查並印出如 `"AIL_startup()\n"` 的字串，
binary 內這些字串可精確定位每個函式位址。FD2 自身呼叫的 AIL 函式有 46 個，
另有 59 個 AIL 字串對應的 function 沒被 FD2 呼叫 (linker 帶進來但 dead code)。

逆向目標是辨認 AIL 層的邊界：看到 FD2 遊戲邏輯呼叫 `AIL_start_sequence(seq_handle)`
就理解意圖即可，AIL 內部邏輯不深究 (vendor SDK 文件可查)。

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
- `0x000394B5` `AIL_resume_sample`
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
- entry point @ `0x3C964` 在 AIL 區塊之後 (Borland linker 把 library 放在 main 之前)
