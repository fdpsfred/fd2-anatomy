# FDMUS.DAT — Miles XMI MIDI 音樂

20 entries 中 15 個是 Miles XMIDI (.XMI) IFF-wrapped sequences、5 個是
3-byte 編譯期 placeholder。
file size 80,367 bytes。

## 檔案格式

LLLLLL archive (詳 `overview.md`)，搭配兩種 entry：

### Placeholder (idx 0, 2, 5, 7, 9)

5 個 3-byte slot，內容固定 `20 0D 0A` (` \r\n`)。編譯期填充 / 預留 slot，
不可作 BGM 播放。

### XMI sequence (idx 1, 3, 4, 6, 8, 10..19)

15 個 IFF XMIDI 序列：

```
+0x00  4 bytes  "FORM"           (IFF magic)
+0x04  u32 BE   chunk_size       (例 0x0E for XDIR header)
+0x08  4 bytes  "XDIR"           (XMIDI directory chunk)
+0x0C  bytes    further IFF chunks ("INFO", "CAT ", 等)
```

## BGM dispatch

- Dispatcher：`set_bgm_track_with_fade @ 0x25977`
- `track_id` (4th arg via ECX register, Watcom convention) 直接當 FDMUS idx
- **無 lookup table** — track_id ≡ FDMUS idx (1:1)
- Loop count：5th arg via stack
- 特殊規則：
  - `track_id == 0xFFFFFFFF` → stop with 4-second fade
  - `track_id == 0x10` 或 `0x11` → instant volume change (no fade-in)
  - 其他 → 2000ms fade-in 至 0x7F volume

## 全 idx 對照與用途

| idx | size | 類型 | 用途 |
|---|---|---|---|
| 0x00 | 3 | placeholder | 編譯期填充 |
| 0x01 | 6164 | XMI | enemy_turn ch3 / ch8 / ch12 / ch14 / ch26 / ch28 / ch29 |
| 0x02 | 3 | placeholder | 編譯期填充 |
| 0x03 | 6796 | XMI | player_turn ch5 / ch9 / ch15 / ch20 / ch22 / ch25 |
| 0x04 | 6264 | XMI | player_turn ch10 / ch17 / ch23 / ch27 + enemy_turn ch10 |
| 0x05 | 3 | placeholder | 編譯期填充 |
| 0x06 | 9660 | XMI | enemy_turn ch5 / ch9 / ch15 / ch20 / ch22 / ch25 |
| 0x07 | 3 | placeholder | 編譯期填充 |
| 0x08 | 12784 | XMI | player_turn ch30 + enemy_turn ch17 / ch23 / ch27 / ch30 |
| 0x09 | 3 | placeholder | 編譯期填充 |
| 0x0A | 5316 | XMI | dynamic-only (cinematic / event-handler register-passed) |
| 0x0B | 3152 | XMI | dynamic-only |
| 0x0C | 2542 | XMI | enemy_turn ch1 / ch2 / ch4 / ch6 / ch7 / ch11 / ch13 / ch16 / ch18 / ch19 / ch21 / ch24 |
| 0x0D | 6966 | XMI | dynamic-only |
| 0x0E | 2924 | XMI | dynamic-only |
| 0x0F | 4128 | XMI | dynamic-only |
| 0x10 | 562 | XMI | special (instant fade-in, no smooth ramp) — 推測 victory fanfare 或 short stinger |
| 0x11 | 1530 | XMI | special (instant fade-in) — 推測 defeat / chapter clear stinger |
| 0x12 | 9884 | XMI | main_menu (`fd2_main` 0x25BF4 `set_bgm_track_with_fade(0x12, 0)`) |
| 0x13 | 1590 | XMI | player_turn ch1 / ch2 / ch3 / ch4 / ch6 / ch7 / ch8 / ch11..14 / ch16 / ch18..21 / ch24 / ch26 / ch28 / ch29 |

## per_chapter BGM 表 (30 章 dump)

`per_chapter_player_turn_bgm[30] @ 0x51E63` (30 bytes)：

| ch | 1 | 2 | 3 | 4 | 5 | 6 | 7 | 8 | 9 | 10 | 11 | 12 | 13 | 14 | 15 | 16 | 17 | 18 | 19 | 20 | 21 | 22 | 23 | 24 | 25 | 26 | 27 | 28 | 29 | 30 |
|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|
| FDMUS | 13 | 13 | 13 | 13 | 03 | 13 | 13 | 13 | 03 | 04 | 13 | 13 | 13 | 13 | 03 | 13 | 04 | 13 | 13 | 03 | 13 | 03 | 04 | 13 | 03 | 13 | 04 | 13 | 13 | 08 |

`per_chapter_enemy_turn_bgm[30] @ 0x51E81` (30 bytes)：

| ch | 1 | 2 | 3 | 4 | 5 | 6 | 7 | 8 | 9 | 10 | 11 | 12 | 13 | 14 | 15 | 16 | 17 | 18 | 19 | 20 | 21 | 22 | 23 | 24 | 25 | 26 | 27 | 28 | 29 | 30 |
|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|
| FDMUS | 0C | 0C | 01 | 0C | 06 | 0C | 0C | 01 | 06 | 04 | 0C | 01 | 0C | 01 | 06 | 0C | 08 | 0C | 0C | 06 | 0C | 06 | 08 | 0C | 06 | 01 | 08 | 01 | 01 | 08 |

兩張表共用 7 個 distinct XMI: `{0x01, 0x03, 0x04, 0x06, 0x08, 0x0C, 0x13}`。

## XMI → MID 轉換

`.XMI` 是 IFF wrapper。轉 `.MID` 需要 xmi2mid converter (例如 Wildmidi、munt、
vinyl XMI converter)。round-trip artifacts 證明 binary content 完整保留 (IFF
magic + chunk structure 完整)。

## 工具

- 抽取 XMI：`tools/decoders/fdmus_xmi_extract.py`
