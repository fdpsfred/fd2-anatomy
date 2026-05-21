# ANI.DAT — 多 frame RLE delta 動畫序列

cinematic 用的多 frame 串流動畫。
file size 2,437,547 bytes (~2.4 MB)，9 entries (idx 0..8)。

## 檔案格式

LLLLLL archive (詳 `overview.md`)，但 **不**經 `fd2_load_dat_resource @ 0x111BA`，
有獨立的 `fd2_play_ani_file_animation_sequence @ 0x20421` fopen pipeline (因為
ANI.DAT 動畫是 frame-by-frame streamed，不一次性 malloc 整個 entry)。

## Loader：`fd2_play_ani_file_animation_sequence @ 0x20421`

```c
fd2_play_ani_file_animation_sequence(_, _, _, ani_idx, ms_per_frame, skip_on_key)
```

```c
fopen("ANI.DAT", &DAT_000501c4);
fseek(file, ani_idx * 4 + 6, SEEK_SET);
fread(start_end_pair, 8, 1, file);
fseek(file, *frame_buf, SEEK_SET);     // jump to entry start
fread(header, 173, 1, file);            // 0xAD byte header
frame_count = *(u16*)(header + 0xA5);
for i in 0..frame_count:
    fread(frame_header, 8, 1, file);    // (data_size, decoded_size, ...)
    fread(frame_bitmap, data_size, 1, file);
    ani_decoder_decode_frame_bytes(decoded_size, bitmap);
    delay(ms_per_frame);
    if skip_on_key: check_keyboard_buffer_nonempty break;
```

`ani_idx * 4 + 6` 與 LLLLLL archive 結構吻合 (6-byte signature + offset[] table)。

## Entry format

```
+0x00     u8[0xAD]  header
            +0xA5..+0xA6  u16  frame_count
            其餘 byte 用途待確認
+0xAD..   per-frame loop:
    +0x00..+0x07  u8[8]   frame_header
                  +0x00..+0x01  u16  data_size (compressed bytes)
                  +0x02..+0x03  u16  decoded_size (target buffer bytes)
                  +0x04..+0x07  ?    other frame metadata
    +0x08..       u8[data_size]  RLE-delta encoded bitmap
        經 ani_decoder_decode_frame_bytes 解到 0xA0000 framebuffer
```

## 9 entries 用途

| idx | 用途 | 備註 |
|---|---|---|
| 0 | (cinematic, 待 in-game observation 確認具體場景) | 結構 100% decoded |
| 1 | 開場動畫 / intro animation | 特殊：載入 FDOTHER[0x4E] 為配套 SFX |
| 2..8 | 其他 cinematic animations | format 100% decoded，具體場景 deferred |

Caller：全 ANI.DAT 載入皆透過 `fd2_play_ani_file_animation_sequence`，由 cinematic
chain 函式 (chapter intro / outro / endgame) 按需呼叫。`fd2_load_dat_resource("FDOTHER",
0x4E)` 是 ANI 配套 (nested archive 1 sub-entry)。
