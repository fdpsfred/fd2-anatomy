# FDICON.B24 — 24×24 indexed portrait-frame sprites

唯一的非 LLLLLL 資源檔。內含 1680 個 24×24 8bpp RLE sprite，file size 624,010 bytes
(0x9858A)。

## 檔案格式

```
+0x00       u16 LE  width        = 0x0018 = 24
+0x02       u16 LE  height       = 0x0018 = 24
+0x04       u16 LE  active_count = 0x0690 = 1680
+0x06       u32 LE × (count+1)  offset[]    (last = sentinel = file_size)
                                            header_size = 6 + 1681 × 4 = 0x1A4A
+0x1A4A..   payload  1680 個 RLE-compressed 24×24 8bpp sprite
```

第一 sprite offset[0] = 0x1A4A；末 sprite end = file size 0x9858A。全 1681 offset
monotonic non-decreasing。header/height/count 欄位與 offset 表尺寸來源見 loader
`fd2_load_portrait_to_cache`（fseek 6、fread 0x1A40，src/rsrc/rsrc.c:206）。

## 載入方式

由 `fopen("FDICON.B24", "rb")` 直接開檔，不走 LLLLLL `fd2_load_dat_resource`，是全遊戲
唯一的非 LLLLLL 資源。全遊戲共 **8 處** `fopen("FDICON.B24","rb")`（8 個 plain-filename
字串 literal），各自開檔後對該章要用到的每個 portrait 呼叫 loader 重建 portrait cache、
載完即 `fclose`：章節 init `fd2_load_chapter_battle_data @ 0x1088D`、portrait 重載
`fd2_load_chapter_portraits_and_dump_tmp @ 0x10B4E`，以及 save/load 還原
（`fd2_load_save_and_init_engine`、`fd2_load_state_from_selected_slot`）、章節轉場
（`fd2_chapter_transition_menu`）、轉職（`fd2_run_class_promotion_menu_main`）、招募／分歧
（`fd2_run_recruitment_or_branch_screen`）、必上場角色 pin（`fd2_pin_required_char_to_party_slot1`）。

## sprite namespace：140 portrait-set × 12 frame

1680 個 sprite 是 140 組 portrait，每組 12 個 frame。索引公式為
`icon_idx = portrait_id × 12 + frame_idx`（portrait_id = 0..139，frame_idx = 0..11）。

loader `fd2_load_portrait_to_cache @ 0x11019`（src/rsrc/rsrc.c:206）的取表方式證實此佈局：
`fseek(fp, 6, SEEK_SET)` 跳過 6-byte magic 後 `fread(hdr_buf, 1, 0x1A40, fp)` 讀入 6720 bytes
= 1680 個 int32 offset，再對指定 portrait 抽出 13 個 int32：

```c
for (i = 0; i < 0xd; i++)                       /* 12 frame offset + 1 end-mark */
    sprite_offsets[i] = ((int32 *)hdr_buf)[portrait_id * 0xc + i];
data_size = sprite_offsets[12] - sprite_offsets[0];   /* 該 portrait 全部 sprite 位元組數 */
```

即每個 portrait 對應表中連續 12 個 frame offset，第 13 個（`portrait_id×12 + 12`，等於下一個
portrait 的首 offset）作為 end-mark 計算資料長度。抽出的 sprite bytes 存進
`data_fd2_portrait_sprite_cache` 這個 malloc(0x32A00) 的 200KB 快取。

快取結構容量為 40 個 portrait：快取前 0x780 bytes 是 frame-offset lookup table
（40 × 12 sprite × 4-byte 絕對 offset），0x780 之後接 packed sprite payload。任一時刻快取只放
該章實際用到的 portrait 工作子集（loader 對重複 portrait_id idempotent，命中直接回既有 idx，
不做 I/O）。快取被完整 dump 成 FD2.TMP swap 檔，格式見 `fd2_tmp.md`。

## 編碼

sprite payload 用 RLE 4-op sprite 編碼（`fd2_rle_blit_sprite`），opcode 與 palette_op 模式見
`codecs.md`。

## 副檔名「.B24」

漢堂自家命名，與 LLLLLL DAT 命名同源。內容是 8bpp indexed（非 24-bit color、無 BMP `BM`
magic）。

## 工具

- 解碼：`tools/decoders/fdicon_decoder.py`
