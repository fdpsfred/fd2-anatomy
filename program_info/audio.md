# audio

FD2 game-side 自寫的 audio 派遣層（BGM dispatcher、SFX trigger）。這層之下的
Miles AIL (Audio Interface Library) vendor library 本體——完整函式 inventory、
抽 `.obj` 重建、library 邊界——見 `rebuild_info/ail/_index.md` 與
`rebuild_info/ail/inventory.md`。

## 驗證對象

- src：`src/audio/audio.c`
- 主要 Ghidra 對象（位址即時核）：
  - `fd2_set_bgm_track_with_fade @ 0x25977` — BGM dispatcher
  - `fd2_play_sfx_with_handle @ 0x25A96` — SFX 播放器（sample slot 0）
  - `fd2_play_sfx_sample_from_bank @ 0x25B45` — SFX 播放器（sample slot 1）
  - `fd2_load_status_effect_sfx @ 0x1D4CB` / `fd2_stop_and_free_status_effect_sfx @ 0x1D4F6`
    — 狀態/法術 SFX bank 的載入與釋放配對
  - `fd2_load_figani_sfx_bank @ 0x2BC9A` — FIGANI 動畫 SFX bank 載入
  - BGM 表 `data_fd2_audio_per_chapter_player_turn_bgm_track[30] @ 0x51E63`、
    `data_fd2_audio_per_chapter_enemy_turn_bgm_track[30] @ 0x51E81`
  - Gate flags `data_fd2_audio_bgm_enabled_flag @ 0x51E61`、
    `data_fd2_audio_sfx_driver_available_flag @ 0x53EF1`、
    `data_fd2_audio_sfx_enabled_flag @ 0x51E62`、
    `data_fd2_battle_scripted_cinematic_mode_or_terrain_idx @ 0x540FF`
- 相關資源檔：`FDMUS.DAT`（BGM sequence）、`FDOTHER.DAT`（SFX sample bank）

## FD2 自寫的 BGM 派遣器

`fd2_set_bgm_track_with_fade @ 0x25977` — FDMUS 音樂的 dispatcher。cdecl，兩個
stack 引數 `(track_id, loop_count)`：

- `track_id` 直接對應 FDMUS idx（沒有 lookup table）。
- 以 `data_fd2_audio_bgm_last_set_track_id @ 0x51A11` 快取「目前這首」。若請求的
  `track_id` 與快取相同就是 no-op（同一首正在播不重載）。
- `track_id == 0xFFFFFFFF` -> 4 秒 fade-out 然後停。
- 其餘 track 只在 MDI driver 存在（`data_fd2_audio_bgm_driver_available_flag @ 0x53EF0 != 0`）
  時才動作：停舊 sequence、以 `fd2_load_dat_resource` 載 `FDMUS.DAT[track_id]`、
  DPMI-lock、`AIL_init_sequence` + `AIL_start_sequence`，再設初始音量與淡入：
  - `data_fd2_audio_bgm_enabled_flag @ 0x51E61 == 0` -> 音量 0（靜音）。
  - `track_id == 0x10` 或 `0x11` -> 音量 0x7F、fade 0（立即，特殊 cue）。
  - 其他 track -> 先設音量 0 當 anchor，再音量 0x7F、fade 2000ms。
- `loop_count` 直接傳給 `AIL_set_sequence_loop_count`（依 AIL 慣例 0 = 無限循環）。
- 主選單 BGM = `0x12`（`main` 迴圈以 `fd2_set_bgm_track_with_fade(0x12, 0)` 起播）。

每章兩個 BGM 表（both `.object2`、const、無 writer；index 0 = 第 1 章 .. 29 = 第 30 章，
每 byte 是 8-bit FDMUS track_id）：

- `data_fd2_audio_per_chapter_player_turn_bgm_track[30] @ 0x51E63` — 每章玩家回合 BGM
- `data_fd2_audio_per_chapter_enemy_turn_bgm_track[30] @ 0x51E81` — 每章敵方回合 BGM
- `fd2_run_full_turn_cycle` 用當前章 index 讀這兩張表：先比對玩家/敵方兩個 track_id
  決定回合切換時要不要 fade BGM，再把對應 byte 餵給 `fd2_set_bgm_track_with_fade`。

## SFX 觸發

`fd2_play_sfx_with_handle @ 0x25A96` — 通用 AIL 一次性 SFX 播放器，UI／戰鬥／選單各處
呼叫（游標移動、確認、取消等）。cdecl，三個引數 `(sfx_table_base, sfx_id, loop_count)`。
三個 gate（任一不過就 silent return）：

- `data_fd2_audio_sfx_driver_available_flag @ 0x53EF1 != 0`（DIG sample driver 已安裝）
- `data_fd2_audio_sfx_enabled_flag @ 0x51E62 != 0`（玩家「音效開」選項）
- `data_fd2_battle_scripted_cinematic_mode_or_terrain_idx @ 0x540FF == 0`（scripted
  cinematic／tutorial 期間靜音）

通過後先 `AIL_stop_sample` 停 slot；`sfx_id == -1` 是純停止路徑（停完就 return）。
否則解出 sample bank entry（`entry = sfx_table_base + sfx_id*4`）：`entry+6` 的 dword 是
sample 相對 bank base 的 byte offset、`entry+10` 的 dword 是結束 offset，長度 = 兩者之差，
再重設 sample slot 並 `AIL_start_sample` 播放。SFX sample bank 來自 `FDOTHER.DAT`。

`fd2_play_sfx_sample_from_bank @ 0x25B45` 結構與上者相同，只是驅動第二個 sample slot；
兩個 slot 讓不同 channel 的 SFX 不會互相截斷。狀態/法術 SFX bank 由
`fd2_load_status_effect_sfx @ 0x1D4CB` 載入（`FDOTHER.DAT` entry 0x50）、由
`fd2_stop_and_free_status_effect_sfx @ 0x1D4F6` 停止並釋放。動畫用的 SFX bank 由
`fd2_load_figani_sfx_bank @ 0x2BC9A` 依 FIGANI header 的參照 byte 查一張 6-byte 索引表
後載入對應 `FDOTHER.DAT` entry。

## 隨片附的 AIL driver / patch 檔

以下是遊戲隨片附的 DOS 端 runtime driver／instrument patch 資源清單（vendor library
本體另見 `rebuild_info/ail/`）。`SETSOUND.EXE`（FLAME2 目錄裡的工具）是 Miles AIL 的
設定程式，會產生 `.MDI` / `.DIG` driver 設定；`AILDRVR.LST` 為 driver 清單，
`SAMPLE.AD/OPL/BNK` 為 instrument patch。

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
