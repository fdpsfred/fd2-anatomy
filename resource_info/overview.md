# 資源檔總覽

FD2 的遊戲內容散布在 11 個 LLLLLL archive、1 個自定 archive (FDICON.B24)、
FD2.SAV 存檔、FD2.LE binary 本身，以及 Miles Sound System 用的 .MDI / .DIG /
.AD / .OPL / .BNK driver/patch 檔。

## LLLLLL archive 統一格式

11 個資源檔共用一個簡單的 LLLLLL archive 格式：

```
+0x00     6 bytes      signature = b"LLLLLL"  (0x4C × 6)
+0x06     u32 LE × N   offset[]               (last entry = sentinel = file_size)
+0x06+N×4 payload bytes                       (concatenated entries)
```

讀取 index `i` 的資源：start = offset[i]、end = offset[i+1]、size = end − start、
內容 = file[start..end]。entry_count = N − 1（最後一個 u32 是 sentinel = file_size）。

`fd2_load_dat_resource @ 0x111BA` 是統一 loader，演算法與 caller 細節見
`program_info/resource.md`。

## 11 個 LLLLLL 資源檔

| 檔案 | size | entries | 格式詳細 | 內容 |
|---|---|---|---|---|
| FDTXT.DAT | 120,502 | 34 | `fdtxt.md` | 對話文字 (中文 dialog VM bytecode) |
| FDOTHER.DAT | 3,382,481 | 103 (+166 nested) | `fdother.md` | UI sprite / portrait / SFX / cinematic image / palette |
| FDFIELD.DAT | 243,169 | 99 | `fdfield.md` | 章節地圖 + tile_event + char spawn |
| FDSHAP.DAT | 3,557,794 | 66 | `fdshap.md` | 戰鬥背景 snapshot + tile attribute |
| DATO.DAT | 1,979,029 | 136 | `dato.md` | 80×80 4-view portrait sprite |
| FDMUS.DAT | 80,367 | 20 | `fdmus.md` | Miles XMI MIDI 音樂 |
| FIGANI.DAT | 15,279,582 | 408 | `figani.md` | 必殺技 / 召喚動畫 byte-stream |
| BG.DAT | 624,564 | 56 | `bg.md` | 320×100 cinematic / battle BG (count/color RLE) |
| TAI.DAT | 94,917 | 56 | `tai.md` | terrain overlay / AI 配對資料 (與 BG 配對) |
| TITLE.DAT | 23,377 | 7 | `title.md` | dead resource (FD2 從未載入) |
| ANI.DAT | 2,437,547 | 9 | `ani.md` | 多 frame RLE delta 動畫序列 |

走 `fd2_load_dat_resource` 的 9 個：FDTXT、FDOTHER、FDFIELD、FDSHAP、DATO、FDMUS、
FIGANI、BG、TAI。走獨立 fopen (但同 LLLLLL 格式) 的 2 個：TITLE、ANI。

## 非 LLLLLL 資源檔

- **FDICON.B24** — 唯一非 LLLLLL，由 `fopen` 直接讀。1680 個 24×24 8bpp
  RLE icon。詳 `fdicon.md`。
- **FD2.SAV** — 22987-byte 存檔，header + map snapshot + runtime_char_array +
  4 個 slot snapshot + checksum。詳 `save_format.md`。
- **FD2.TMP** — runtime swap file (portrait cache dump，由 chapter handler 使用，
  不是靜態資源檔)。

## 與 program 端的對應

每個 chapter 開戰前 `fd2_load_chapter_battle_data @ 0x1088D` 載入該章對應的：
- FDFIELD `chapter_id × 3 + 0` / `+1` / `+2` → tile_map / tile_event / portrait_load_buffer
- FDSHAP `shap_id × 2 + 0` / `+1` → battle_scene_snapshot / tile_attribute_flags
  (其中 `shap_id = tile_event_data_table[0]`)
- FDTXT `chapter_id + 1` → current_chapter_text
- FDICON.B24 全檔 → portrait cache 載入該章用到的 24×24 icons

完整的 chapter ↔ DAT idx 對照見 `fdfield.md` 與 `fdshap.md`。

## Miles Sound System driver/patch 檔

FLAME2 目錄下的 `.MDI` / `.DIG` 是 Miles AIL 的 driver；`.AD` / `.OPL` / `.BNK`
是 instrument patch；`AILDRVR.LST` 是 driver 清單；`SETSOUND.EXE` 是 vendor
配置工具。完整清單見 `rebuild_info/ail/inventory.md` §DOS-side driver 檔案。
