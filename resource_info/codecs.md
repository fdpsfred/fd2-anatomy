# FD2 sprite 編碼 (codecs)

FD2 的所有 sprite 圖檔只用兩種像素編碼。本檔是這兩種編碼的唯一正典，兩者都
從 `src/gfx/blitspr.c` 定案。各資源檔 (bg / tai / fdshap / fdother / ani /
figani / fdicon / dato) 只描述自己專屬的 header / stride / index，像素解碼一律
引用本檔，不重述 opcode。

兩種編碼互不相容，不可混用：

| 編碼 | decoder | 典型用途 | 消費資源 |
|------|---------|------|---------|
| RLE 4-op | `fd2_rle_blit_sprite @ 0x4E63D` | 一般 sprite / tile / 背景 | FDOTHER sprite、FIGANI、BG、TAI、FDSHAP tile、FDICON |
| dialog-pixel | `fd2_decode_dialog_pixel_byte @ 0x4E916` | 對話 portrait + 戰鬥/動畫 sprite | DATO 對話 portrait、FDOTHER 及戰鬥/動畫 sprite |

---

## A. RLE 4-op sprite 編碼

主解碼 blit 是 `fd2_rle_blit_sprite @ 0x4E63D` (blitspr.c:470)，遊戲裡每一個可見
的非 tile 圖形 (角色、portrait、法術特效、UI sprite) 都經這裡上到畫面。姊妹版
`fd2_rle_blit_with_palette_remap @ 0x4E583` (blitspr.c:848) 用同一套 stream 格式與
opcode，差別只在輸出像素改走 256-entry 調色盤轉換表 (詳「palette remap 姊妹版」)。

### stream header

stream 前 4 個 byte 是尺寸標頭，之後接 command byte 流：

```
+0  u16 LE  width    (row 的像素寬度)
+2  u16 LE  height   (row 數 = 列數)
+4  ...     command byte 流
```

(blitspr.c:486-488。)

### command byte

每個 command byte：高 2 bit 選 op、低 6 bit 是 len-1，所以

```
len = (cmd & 0x3F) + 1        (1..64)
```

高 2 bit 對應四個 op (blitspr.c:432-439、497-535)：

| 高 2 bit | op | 行為 | row 欄消耗 |
|---------|-----|------|-----------|
| `0b00` | RLE fill | 讀下一個 byte，填 `len` 個 dst 像素 | `len` |
| `0b01` | stretched fill | 讀下一個 byte，寫到 `len` 個「隔一格」的 dst 像素 (`dst += 2`) | `2*len` |
| `0b10` | literal copy | 從 stream 複製 `len` 個 byte 到 dst | `len` |
| `0b11` | skip (透明) | dst 前進 `len` 像素，不寫入 | `len` |

### row 推進

decoder 維護兩個 state global (blitspr.c:19-20、486-487)：

- `data_fd2_graphics_rle_blit_cur_width @ 0x627B4` — 目前 row 寬度
- `data_fd2_graphics_rle_blit_remaining_rows @ 0x627B6` — 剩餘 row 數

每個 command 從 row 寬度計數扣掉它消耗的欄數 (stretched 扣 `2*len`，其餘扣
`len`)。計數歸零時，dst 前進到下一列 (`dst += stride - width`)，剩餘 row 數遞減，
共畫 `height` 列 (blitspr.c:534-539)。

### palette_op 三模式

`fd2_rle_blit_sprite` 的第 6 個引數 `palette_op` 選三種輸出模式 (blitspr.c:493、
543、595)。三模式的 stream 解析完全相同，只差每個不透明像素怎麼算出寫入值；即使
在會忽略 source byte 的模式，RLE fill / stretched / literal 也照樣把 source
byte 從 stream 讀掉，以維持 stream 同步 (blitspr.c:610、621、630)。

| `palette_op` 值 | 模式 | 輸出像素 |
|----------------|------|---------|
| `0xFFFFFFFF` | passthrough | source byte 原樣寫入 |
| `(u16) > 0xFF` | translucent overlay | `out = ((src + rotation_key) & 7) + color_base`，其中 `color_base = palette_op & 0xFF`、`rotation_key = (palette_op >> 8) & 0xFF` |
| `(u16) <= 0xFF` | silhouette fill | 每個不透明像素都寫成單一 byte `palette_op & 0xFF`；source byte 仍照消耗 |

- passthrough 是直接貼圖。
- translucent overlay 用於法術特效染色與章節 palette swap (blitspr.c:544-546、561)。
- silhouette fill 用於陰影、閃白、陣亡剪影 (blitspr.c:595-597、612)。

### palette remap 姊妹版

`fd2_rle_blit_with_palette_remap @ 0x4E583` 用同一套 header 與四 op，但輸出恆為

```
out = remap_table[src]
```

`remap_table` 是 256-entry 調色盤轉換表，用 source 像素 byte 當索引 (blitspr.c:887、
899、909)。四個 op 對 remap 的作用：RLE fill 讀 1 byte 寫 `remap[byte]` 到 `len`
像素、stretched fill 同理但隔格寫、literal copy 每個 byte 都經 `remap[byte]`、
skip 前進透明。用於特殊技 / 施法 cinematic 逐幀替換 backdrop 與名牌 sprite 的
配色 (blitspr.c:832-836)。

### 消費資源

FDOTHER sprite、FIGANI、BG、TAI、FDSHAP tile、FDICON 都是這套 RLE 4-op stream。
各檔的 archive / offset table / index 公式見各自的 resource_info 檔。

---

## B. dialog-pixel 編碼

逐像素 run 編碼，解碼器是 `fd2_decode_dialog_pixel_byte @ 0x4E916`
(blitspr.c:779)。它是逐像素 state machine，每次回傳一個 16-bit 打包值
`(run_remain << 8) | pixel`：高 byte 是這個像素之後還有幾個像素沿用同值、低 byte
是本次要寫的像素。呼叫端把回傳值當下一次的 `state` 續傳，逐像素寫低 byte。

矩形尺寸不寫死：由每個 sprite 自己的 header 決定（`+0 u16 width / +2 u16 height /
+4 stream`，與 RLE 同一套 header 佈局），所以同一套 codec 能解任意大小的圖，80×80
對話 portrait 只是其中一種消費者。

### stream byte 語意 (blitspr.c:790-796)

| byte 範圍 | 意義 |
|----------|------|
| `0x00..0xC0` | 直接像素值 (193 個相異值)，`run_remain = 0` |
| `0xC1..0xFF` | run marker：下一個 byte 是這段 run 的像素值，run 長度 = `(b - 0xC1) + 1` (1..63 像素) |

因此像素值 `0xC1..0xFF` 只會作為 run 的值出現在 run marker 的第二個 byte，永遠
不會是裸 literal。這是 portrait 大面積純色背景的壓縮手段。

### 消費資源

`fd2_decode_dialog_pixel_byte` 有 3 個直接 caller，此編碼**不限 DATO**：

- DATO 對話 portrait 走 `fd2_dialog_sprite_blit_normal @ 0x4E8AF` 與
  `fd2_dialog_sprite_blit_mirrored @ 0x4E8E1`；每個 portrait 有 4 個表情 frame，每
  frame 自帶 `+0 u16 width / +2 u16 height / +4 dialog-pixel stream`，archive 與
  entry 佈局見 `dato.md`。
- 第三個 caller `fd2_blit_sprite_with_decoded_pixels @ 0x4E85B` 是通用矩形 painter
  （尺寸讀自 sprite header），另有 8 個戰鬥／動畫 caller 用它解非 DATO sprite——法術
  命中／彈道／疊染特效、傳送門開啟／崩解、陣亡動畫、以及新角色登場的爆炸 sprite
  （`fd2_animate_party_addition_with_appear_effect` 以 `FDOTHER.DAT[9]` 經此 codec 解）。

---

## 工具

RLE 4-op 與 dialog-pixel 的參考解碼實作都在 `rle_decoder.py` (`rle_decode` /
`rle_decode_sized` 與 `dialog_pixel_decode`)，用法見 `tools/decoders/_index.md`。
