# text_dialog

對話框系統：portrait 載入 → 對話框組裝動畫 → 字模渲染 → 等按鍵繼續。
所有對話文字由 FDTXT.DAT 提供，文字實體格式詳見 `resource_info/fdtxt.md`。

## 對話流程

```
display_dialog_scene(text_resource, page_id, font_sheet, ...)
  ├─ load_chapter_portrait        ← 載入講者 DATO.DAT 資料
  ├─ play_dialog_open_animation   ← 5-stage 對話框組裝 (含 cursor→origin 插值)
  ├─ blit_glyph_2bpp_with_outline ← 逐字繪製 (中文 + ASCII)
  ├─ paint_portrait_to_dialog_area ← 切換講者時的肖像
  └─ wait_for_input_dialog_with_blink ← ▼ 按任意鍵繼續 (18.2Hz 動畫提示)
```

## 主要 functions

| 位址 | 名稱 | 角色 |
|---|---|---|
| `0x11019` | `load_portrait_to_cache` | 200 KB linear-probe 肖像快取 |
| `0x15F84` | `display_dialog_scene` | FDTXT page bytecode VM dispatcher |
| `0x165AC` | `play_dialog_open_animation` | 5-stage 對話框組裝動畫 |
| `0x16559` | `paint_portrait_to_dialog_area` | speaker 切換 blit |
| `0x16C57` | `wait_for_input_dialog_with_blink` | ▼ 按鍵提示動畫 |
| `0x1956B` | `load_chapter_portrait` | DATO.DAT 載入 + slide-down |
| `0x4EA2A` | `blit_glyph_2bpp_with_outline` | 16×16 字模渲染含 outline |
| `0x168B6` | `assemble_dialog_frame_layered` | 17-tile 9-slice 對話框組裝 |
| `0x1366A` | `cutscene_event_trigger` | 按章節小整數觸發音樂/動畫/事件 milestone |

## display_dialog_scene 內部 VM

`display_dialog_scene` 內部是 do-while 迴圈，逐 u16 讀取 FDTXT page bytecode
並依 opcode 走分支：

- `0xFFFF` END：page 終止 (parser 用此偵測 page boundary)
- `0xFFFE` PAGE_BREAK：等使用者按鍵後繼續
- `0xFFFD..0xFFFC` 系列 / `0xFFEC..0xFFEF` 系列：portrait swap、sub-dialog 遞迴等
- 任何 < 0xFFEC 的 u16 = 直接的 glyph_id，索引 `chinese_font_sheet`
  (FDOTHER.DAT[4]) 中對應字模，呼 `blit_glyph_2bpp_with_outline` 渲染

完整 opcode 清單與每個 opcode 的語意見 `resource_info/fdtxt.md`。

## Portrait 快取

`load_portrait_to_cache` 維護 200 KB portrait sprite cache
(`portrait_sprite_cache @ 0x53A61`)：

```
Bytes [0..0x77F]    = 12 sprite-offset entries × 0xC bytes per portrait (前置查表)
Bytes [0x780..]     = 連續排列的 sprite raw data
DAT_000539EC        = 已使用 byte count (= 尾端 offset)
DAT_00053BDF        = portrait_cache_count
DAT_00053B17[N]     = 第 N 個 cached portrait_id (線性比對用)
```

第一次載入：malloc 200 KB，從檔頭 0x780 後開始排放，建 12 個 sprite offset entry。
之後線性掃 `0x53B17` 找快取命中；命中則直接回 idx，未命中則 append。

## 字模渲染

`blit_glyph_2bpp_with_outline` 接 16×16 字模，從 packed 2bpp 格式渲染：
1. 第 1 階段：可選背景色填 16×16 矩形
2. 第 2 階段：scan 16 rows × 16 bits；每 set bit 寫 `fill_color`，並在右下角加
   `outline_color` 像素 (產生 1-pixel outline 立體效果)

中文字模來源：`chinese_font_sheet @ 0x53A75`，從 FDOTHER.DAT[4] 載入。

## 對話框組裝 (17-tile 9-slice)

`assemble_dialog_frame_layered` 用 17 個 sprite tile 組成可變大小對話框：
- 4 corners + 4 secondary corners (sprite_idx 1, 2, 6, 14-17)
- 4 stretchable edges (3, 4, 5, 7, 8)
- 5 inner fills (9, 10, 11, 12, 13)
- middle-row stretching: `n_cols-2` 次平鋪
- middle-col stretching: `n_rows-2` 次平鋪

任意大小對話框都從這 17 個 tile 組合，sprite 來源 `0x53A81` (與 death_anim
共用 sheet)。

## Speaker 切換

`paint_portrait_to_dialog_area(portrait_idx)`：
- 從 `portrait_sprite_buffer @ 0x53A85` 抽 sprite
- 依 `dialog_portrait_mode @ 0x53C67` 決定目標位置：
  - `0x9017` (友方) → mirror blit (`dialog_sprite_blit_mirrored`，向左面向)
  - 其他 → normal blit (`dialog_sprite_blit_normal`，向右面向)

這就是 FD2「兩人面對面對話」的 blit 機制：用 sprite mirroring 實現視覺對稱。

## ▼ 按鍵提示動畫

`wait_for_input_dialog_with_blink(animated_cursor_mode)`：
- 讀 BIOS tick (0x46C)；每 30 ticks (~1.65 秒) 切換文字 blink state
- `animated_cursor_mode == 1`：每 3 ticks 在 sprite 0x12 / 0x13 之間切換對話框
  「向下箭頭」frame
- 等 `check_input_ready` 回報有輸入後，呼叫 `int386(0x16, ...)` → scancode 標準化 → return

## 對話框幾何相關 globals

| 位址 | 名稱 |
|---|---|
| `0x53A18..0x53A28` | `dialog_frame_layers` (5 個 0x682C-byte sprite buffer) |
| `0x53C67` | `dialog_portrait_mode` (0x9017=ally, 0x728=enemy, 其他=各章專屬) |
| `0x53AD9` | `drop_dialog_item_text_id` (item drop 場合用) |
| `0x53AE1` | `drop_dialog_gold_amount` (gold drop 場合用) |
| `0x53C57` | `current_menu_cursor_idx` (modal 共用) |

各章節有 5 個專屬 portrait 位置 (kind 0x80-0x84) 對應 boss 等特殊角色，見
`load_chapter_portrait` 的 switch。

## 文字資源 globals

| 位址 | 名稱 |
|---|---|
| `0x53A7D` | `all_game_text` (fd2_main 載入的全文字 buffer，FDTXT idx 0，661 pages 全遊戲共用) |
| `0x53A79` | `current_chapter_text` (每章自己的文字資源，FDTXT idx = chapter_id+1) |

`display_dialog_scene` 第 5 個參數 = `page_id`，索引到對應 chapter_text 中的對話頁。

## cutscene_event_trigger

`cutscene_event_trigger(event_id)` 是「按章節小整數觸發音樂/動畫/事件
milestone」的 helper。chapter init 流程內常見：例如 `cutscene_event_trigger(99)`
觸發 prologue 開場音樂、`cutscene_event_trigger(0x5A..0x69)` 對應每段 prologue
event。
