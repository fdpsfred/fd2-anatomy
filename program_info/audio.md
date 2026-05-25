# audio

FD2 game-side 自寫的 audio 派遣層（BGM dispatcher、SFX trigger）。Miles AIL
(Audio Interface Library) vendor library 的完整 inventory / 抽 `.obj` 工作的
前置資料 / driver 與 patch 檔對應，詳見 `rebuild_info/ail/inventory.md` 與
`rebuild_info/ail/_index.md`。

## FD2 自寫的 BGM 派遣器

`fd2_set_bgm_track_with_fade @ 0x25977` — FDMUS 音樂的 dispatcher。

- `track_id` 經 ECX register 傳入 (Watcom 4-arg register convention)
- `track_id` 直接對應 FDMUS idx (沒有 lookup table)
- `track_id == 0xFFFFFFFF` → 4 秒 fade-out 然後停
- `track_id == 0x10` 或 `0x11` → 立即音量切換 (無 fade-in)
- 主選單 BGM = 0x12 (在 `fd2_main`)

每章兩個 BGM 表：

- `data_fd2_audio_per_chapter_player_turn_bgm_track[30] @ 0x51E63` — 每章玩家回合 BGM track_id
- `data_fd2_audio_per_chapter_enemy_turn_bgm_track[30] @ 0x51E81` — 每章敵方回合 BGM track_id
- `fd2_run_full_turn_cycle` 把這兩個 byte cast 為 BGM track_id 餵給 `fd2_set_bgm_track_with_fade`

## SFX 觸發

`fd2_play_sfx_with_handle @ 0x25A96` — 通用 AIL SFX 播放器，UI 各處呼叫
(cursor 移動 sfx 0、確認 sfx 7、取消等)。3 個 gate flag：
`audio_master_enable @ 0x53EF1`、`sample_system_ready @ 0x51E62`、
`audio_mute_flag @ 0x540FF`。

## 隨片附的 AIL driver / patch

`SETSOUND.EXE`（FLAME2 目錄裡的工具）是 Miles AIL 的設定程式，會產生
`.MDI` / `.DIG` driver 設定。`AILDRVR.LST` 為 driver 清單，
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
