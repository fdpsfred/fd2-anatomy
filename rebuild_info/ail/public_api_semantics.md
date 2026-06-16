# AIL public API runtime semantics

vendor binary disasm audit 確認的 API 行為，client 必須照辦否則 playback 異常。
Function inventory 見 `inventory.md`，calling convention 見 `calling_convention.md`。

## Driver init 流程

### 路線 A: INI wrapper（推薦）

```c
HMDIDRIVER hmdi = (HMDIDRIVER)AIL_install_MDI_INI();
HDIGDRIVER hdig = (HDIGDRIVER)AIL_install_DIG_INI();
```

內部讀 `MDI.INI` / `DIG.INI`，return driver handle。

### 路線 B: 手動 read_INI

```c
char buf[264];
AIL_API_read_INI(buf, "DIG.INI");
HDIGDRIVER hdig = (HDIGDRIVER)AIL_install_DIG_driver_file(buf + 0x80, buf + 0x100);
```

### INI IRQ autodetect

INI 內 `IRQ -1` / `DMA_8_BIT -1` / `DMA_16_BIT -1` 觸發 SB-class driver 從
`BLASTER` env var 解析。`BLASTER` 的 `IN` 必須匹配實際 SB IRQ，否則 PCM
playback silent fail（無 error code）。

## AIL_API_read_INI buffer layout

| offset | size | field |
|---|---|---|
| 0x000 | 128 | `char Device[128]` |
| 0x080 | 128 | `char Driver[128]` (driver filename) |
| 0x100 | 2 | `short IO` (I/O base) |
| 0x102 | 2 | `short IRQ` |
| 0x104 | 2 | `short DMA_8` |
| 0x106 | 2 | `short DMA_16` |

Total 264 bytes。`AIL_install_DIG_driver_file` 第二個參數 = `buf + 0x100`。

## AIL_delay(N) — N 是 VGA vertical retrace count

`AIL_delay` 內部是 VBLANK polling loop。每個 iteration = 一次 VGA vert
retrace ≈ 16.67ms @ 60Hz。

換算：1 秒 ≈ `AIL_delay(60)`，1.5 秒 ≈ `AIL_delay(90)`。

## AIL_sequence_status(handle) — bitflag

| 值 | 意義 |
|---|---|
| 1 | SEQ_FREE |
| 2 | SEQ_DONE |
| 4 | SEQ_PLAYING |
| 8 | SEQ_STOPPED |

注意 4 是 PLAYING，不是 2。

## AIL_set_sequence_loop_count(handle, N)

N > 0 是 loop 次數（含第一次）。N = 99 常用作「持續播」。

## Sample setup — AIL 預設足以播 FDOTHER 8-bit PCM

FD2 的 `fd2_play_sfx_with_handle` 不呼叫 `AIL_set_sample_type` 或
`AIL_set_sample_playback_rate`，只用 init + set_address + set_loop_count +
start_sample。FDOTHER SFX 是 raw 8-bit unsigned mono PCM (~11025 Hz)，
AIL preferences default 已涵蓋此格式。

## FDOTHER SFX bank container

FDOTHER.DAT 是 LLLLLL outer container。`FDOTHER[0x1F]` = nested LLLLLL
sub-archive，每個 entry 是一個 SFX 的 PCM bytes。

```c
u8 *entry = bank + sfx_id * 4;
u32 off   = *(u32 *)(entry + 6);      // +6 skips LLLLLL magic
u32 end   = *(u32 *)(entry + 10);
u8 *sample     = bank + off;
u32 sample_len = end - off;
```

## FD2 game 的 AIL API 使用模式

| Caller | AIL APIs |
|---|---|
| `main` | startup, shutdown, install_DIG/MDI_INI, allocate_sample_handle ×2, allocate_sequence_handle |
| `fd2_play_sfx_with_handle` | stop_sample, init_sample, set_sample_address, set_sample_loop_count, start_sample |
| `fd2_play_sfx_sample_from_bank` | 同上（使用第二個 sample handle） |
| `fd2_set_bgm_track_with_fade` | stop_sequence, init_sequence, start_sequence, set_sequence_volume (fade-in), set_sequence_loop_count |
| `fd2_game_options_menu_loop` | set_sequence_volume (BGM on/off toggle with 1s fade) |

`main` 在 shutdown 時**只呼叫 `AIL_shutdown()`**，不先 uninstall driver。
