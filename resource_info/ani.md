# ANI.DAT — 多 frame RLE delta 動畫序列

cinematic 用的多 frame 串流動畫。
file size 2,437,547 bytes (~2.4 MB)，9 entries (idx 0..8)。

## 檔案格式

LLLLLL archive (詳 `overview.md`)，但 **不**經 `fd2_load_dat_resource @ 0x111BA`，
有獨立的 `fd2_play_ani_file_animation_sequence @ 0x20421` fopen pipeline (因為
ANI.DAT 動畫是 frame-by-frame streamed，不一次性 malloc 整個 entry)。

## Loader：`fd2_play_ani_file_animation_sequence @ 0x20421`

```c
fd2_play_ani_file_animation_sequence(ani_idx, ms_per_frame, skip_on_key)
```

```c
fopen("ANI.DAT", &DAT_000501c4);
fseek(file, ani_idx * 4 + 6, SEEK_SET);
fread(start_end_pair, 8, 1, file);
fseek(file, *frame_buf, SEEK_SET);     // jump to entry start
fread(header, 173, 1, file);            // 0xAD byte header
frame_count = *(u16*)(header + 0xA5);
for i in 0..frame_count:
    fread(frame_header, 8, 1, file);    // (data_size, chunk_count, +4..+7 恆 0)
    fread(frame_bitmap, data_size, 1, file);
    fd2_ani_decoder_decode_frame_bytes(chunk_count, bitmap);
    delay(ms_per_frame);
    if skip_on_key: check_keyboard_buffer_nonempty break;
```

`ani_idx * 4 + 6` 與 LLLLLL archive 結構吻合 (6-byte signature + offset[] table)。

## Entry format

每個 entry 由一個 0xAD-byte 檔頭起頭，其後接 `frame_count` 個 frame。

```
+0x00     u8[0xAD]  header (AFM 工具檔頭，見下)
            +0xA5..+0xA6  u16  frame_count
+0xAD..   per-frame loop:
    +0x00..+0x07  u8[8]   frame_header
                  +0x00..+0x01  u16  data_size (本 frame 壓縮後 byte 數)
                  +0x02..+0x03  u16  chunk_count (傳給 fd2_ani_decoder_decode_frame_bytes 的
                                       byte_count = 要 dispatch 的 chunk-type opcode 數，非輸出 byte 數)
                  +0x04..+0x07  u8[4]  恆為 0（reserved padding，decoder 從不讀取）
    +0x08..       u8[data_size]  RLE-delta encoded bitmap
        經 fd2_ani_decoder_decode_frame_bytes 解到 0xA0000 framebuffer
```

### 0xAD-byte header

檔頭前段是 AFM (Animation File Manager) 製作工具寫入的 ASCII 橫幅字串
`"AFM - Animation File Manager Version 1.00 Copyright (C) 1993 Lo Yuan Tsung 09/29"`
（結尾 0x1A EOF），接一個以空白補滿的 `".Empty Title."` 標題欄位（9 個 entry 皆為
"Empty Title"）。檔頭尾端 `+0xA2` 起是二進位描述子：

| offset | 型別 | 值 | 意義 |
|---|---|---|---|
| `+0xA2` | u16 | 0x0141（9 entry 皆同） | AFM 格式常數 |
| `+0xA5` | u16 | 逐 entry 不同 | **frame_count**（loader 唯一讀取的檔頭欄位） |
| `+0xA7` | u16 | 0x0140 = 320 | frame 寬 |
| `+0xA9` | u16 | 0x00C8 = 200 | frame 高 |
| `+0xAC` | u8 | 0xFA（9 entry 皆同） | AFM 格式常數 |

loader 只讀 `+0xA5` 的 frame_count；frame 幾何 320×200 是全螢幕固定值，程式直接以
0xA0000 mode-13h framebuffer 為輸出目標，不參考檔頭裡的寬高欄位。per-frame header 的
`+0x04..+0x07` 四個 byte 在全部 9 個 entry 的每一 frame 都恆為 0，decoder 也從不讀取，
屬純 reserved padding。

## 9 entries 對應場景

全 9 個 entry (idx 0..8) 都由 cinematic 函式引用，逐一對應如下（frame 數為實檔統計）：

| idx | frame 數 | 播放場景 | caller |
|---|---|---|---|
| 0 | 96 | 第 21 章隱藏關解鎖 cinematic 中段白閃過場；亦作 title-attract credit 捲動的 cinematic 插入 | `fd2_play_chapter_21_hidden_stage_unlock_cinematic`、`fd2_title_attract_and_main_menu` |
| 1 | 51 | title-attract 序列 Phase 7 的 clear-status 面板 zoom-in reveal；唯一會觸發 FDOTHER[0x4E] chime SFX 的 idx | `fd2_title_attract_and_main_menu` |
| 2 | 26 | game-ending cinematic 中段 ANI（title frame 淡入後） | `fd2_play_game_ending_cinematic` |
| 3 | 28 | title-attract 序列 Phase 3 中段 ANI（配 palette FDOTHER[0x63]） | `fd2_title_attract_and_main_menu` |
| 4 | 12 | title-attract credit 捲動 cinematic 插入（scroll row 0x14A） | `fd2_title_attract_and_main_menu` → `fd2_load_and_fade_in_cinematic_image` |
| 5 | 35 | 同上（row 0x14A 第二段） | 同上 |
| 6 | 12 | 同上（row 0xD2） | 同上 |
| 7 | 17 | 同上（row 0xD2 第二段） | 同上 |
| 8 | 12 | 同上（row 0x6E） | 同上 |

全 ANI.DAT 載入皆透過 `fd2_play_ani_file_animation_sequence`。player 對 `anim_idx == 1`
特別載入 FDOTHER[0x4E]（nested archive 1 sub-entry 的 chime SFX），在 frame 0 播放、
結束時停播並釋放；其餘 idx 不帶 SFX。idx 4..8 皆走 `fd2_load_and_fade_in_cinematic_image`
包裝（載入 palette → 播 ANI → 淡出），在 ending credit 捲動計數到特定 row 時插播。
