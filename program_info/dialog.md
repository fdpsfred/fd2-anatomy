# dialog

對話框系統：載入講者肖像 -> 對話框組裝進場動畫 -> 逐字字模渲染 -> 等按鍵翻頁。
所有對話文字由 FDTXT.DAT 提供，逐頁 opcode 位元組格式見 `resource_info/fdtxt.md`；
runtime_char 欄位（portrait_id、pos_x/y 等）佈局見 `overview.md`。

## 驗證對象

- **src**：`src/dialog/dialog.c`（對話框繪製 / 對話 VM / Yes-No 確認框 / save-under）。
- **主要 Ghidra 對象**（位址即時核對）：
  - `fd2_display_dialog_scene @ 0x15F84` — 對話 VM，逐 int16 opcode 解譯
  - `fd2_play_dialog_open_animation @ 0x165AC` — 對話框開場（游標插值 + 5 段組裝）
  - `fd2_assemble_dialog_frame_layered @ 0x168B6` — 17-tile 9-slice 對話框組裝
  - `fd2_portrait_blink_animation_step @ 0x164E8` — 講者嘴巴動畫一步
  - `fd2_scroll_portrait_dialog_text_up_one_line @ 0x16E24` — 有肖像時文字區上捲一行
  - `fd2_close_dialog_panels_then_slide_out_to_cursor @ 0x16B43` — 關框並滑回游標
  - `fd2_text_dialog_typewriter_loop @ 0x19953` / `fd2_animate_dialog_page_advance_collapse @ 0x197E5` — Yes/No 確認框（打字動畫 + 角落內折）
  - `fd2_backup_dialog_area_to_buffer @ 0x175A9` / `fd2_restore_dialog_area_from_buffer @ 0x17643` — 設定/狀態框的 save-under
  - `fd2_show_portrait_dialog_with_input @ 0x2C39B`、`fd2_close_intro_dialog_with_slide_out @ 0x2D31B`、`fd2_scroll_text_screen_up_by_lines @ 0x24D22`、`fd2_cleanup_dialog_sprite_buffer @ 0x15E71`
- **相依（非 dialog.c）Ghidra 對象**：`fd2_blit_glyph_1bpp_with_outline @ 0x4EA2A`（字模渲染）、
  `fd2_paint_portrait_to_dialog_area @ 0x16559`（肖像 blit）、`fd2_wait_for_input_dialog_with_blink @ 0x16C57`
  （翻頁提示）、`fd2_dialog_open_speaker_portrait @ 0x1956B`（開講者肖像框）、
  `fd2_load_portrait_to_cache @ 0x11019`（FDICON.B24 肖像快取）。
- **相關資源檔**：FDTXT.DAT（對話文字 / opcode 流）、FDOTHER.DAT[4]（中文字模 sheet）、
  DATO.DAT（對話講者肖像 sprite）、FDICON.B24（角色肖像 sprite 快取來源）。

## 對話流程

對話系統有兩層入口。外層由選單 / 過場 / 章節事件先開好講者肖像框，內層是 VM 逐字播文字：

```
外層（章節事件 / 商店 / 招募 / 升職 / save-load 等）
  fd2_dialog_open_speaker_portrait(portrait_kind)
    ├─ malloc 3 個 64000B slide workspace（0x53C5B/0x53C5F/0x53C63）
    ├─ fd2_assemble_dialog_frame_layered  ← 在 overlay 上組出對話框
    ├─ fd2_load_dat_resource("DATO.DAT", …, portrait_kind)  ← 載講者肖像
    └─ fd2_slide_panel_down_step ×6       ← 對話框滑下進場

內層 VM
  fd2_display_dialog_scene(text_base, page_idx, render_pos, …)
    ├─ fd2_blit_glyph_1bpp_with_outline    ← 逐字繪製（中文 + 數字）
    ├─ fd2_portrait_blink_animation_step   ← 每字推進講者嘴巴動畫
    ├─ fd2_play_dialog_open_animation      ← 遇肖像切換 opcode 時重開框
    ├─ fd2_paint_portrait_to_dialog_area   ← 翻頁 / 收框時重畫肖像
    ├─ fd2_wait_for_input_dialog_with_blink← ▼ 翻頁提示，等按鍵
    └─ fd2_scroll_portrait_dialog_text_up_one_line ← 第 3 行時捲動
```

Yes/No 確認框走另一組（`fd2_text_dialog_typewriter_loop` +
`fd2_animate_dialog_page_advance_collapse`），詳「Yes/No 確認框」節。

## 主要 functions

| 位址 | 名稱 | 角色 |
|---|---|---|
| `0x15F84` | `fd2_display_dialog_scene` | FDTXT page opcode 流的 VM dispatcher |
| `0x165AC` | `fd2_play_dialog_open_animation` | 游標插值 + 5 段對話框組裝、回傳 restore handle |
| `0x168B6` | `fd2_assemble_dialog_frame_layered` | 17-tile 9-slice 可變大小對話框 |
| `0x16559` | `fd2_paint_portrait_to_dialog_area` | 單張肖像 sprite blit（normal / mirrored）|
| `0x164E8` | `fd2_portrait_blink_animation_step` | 講者嘴巴動畫一步（frame 0/1/2/1）|
| `0x16C57` | `fd2_wait_for_input_dialog_with_blink` | ▼ 翻頁提示動畫 + 等鍵 |
| `0x16E24` | `fd2_scroll_portrait_dialog_text_up_one_line` | 有肖像時文字區上捲一行 |
| `0x16B43` | `fd2_close_dialog_panels_then_slide_out_to_cursor` | 拆 5 層框並滑回游標 |
| `0x1956B` | `fd2_dialog_open_speaker_portrait` | DATO.DAT 載肖像 + 組框 + slide-down |
| `0x11019` | `fd2_load_portrait_to_cache` | FDICON.B24 肖像 200KB 線性比對快取 |
| `0x4EA2A` | `fd2_blit_glyph_1bpp_with_outline` | 16x16 字模渲染含 outline |

## fd2_display_dialog_scene 的 opcode VM

簽名（9 個 `__cdecl` stack 參數，回傳最後 render_pos）：

```
fd2_display_dialog_scene(text_base, page_idx, render_pos, render_pitch,
                         glyph_fill, glyph_outline, glyph_bg, glyph_height, blink_flag)
```

`text_base` 通常是 `data_fd2_current_chapter_text_ptr`（本章 FDTXT），`page_idx` 是頁起始索引；
起點 `cur_op = text_base + (int16)text_base[page_idx]`（表頭 word 是 byte offset）。字模三色
（fill / outline / bg）直接轉交 `fd2_blit_glyph_1bpp_with_outline`。

VM 是 do-while，逐 int16 讀 opcode，負值為控制碼：

- `0xFFFF` END：若有 active 肖像則收框，回傳 render_pos
- `0xFFFE` LINE ADVANCE：推進一行、不等鍵；肖像在場且 line_count==3 時先上捲一行
- `0xFFFD` PAGE BREAK：推進一行、若肖像在場則重畫肖像，並 `fd2_wait_for_input_dialog_with_blink(1)` 等鍵
- `0xFFFC` / `0xFFFB`：遞迴 sub-dialog，text_base 換成 `data_fd2_all_game_text_ptr`，page 分別取
  `data_fd2_dialog_last_action_text_id_param` / `data_fd2_dialog_drop_swap_text_id_param`
- `0xFFFA`：代入整數 —— `sprintf("%d", data_fd2_dialog_last_action_value_param)` 後逐 digit blit（如金錢 / 數量）
- `0xFFEF` / `0xFFEE`：載敵方 sprite 到肖像槽 slot1(0x728,render base 0xA0B4F) / slot2(0x9017,0xA951F)
- `0xFFED` / `0xFFEC`：載友方 sprite（取自 `data_fd2_battle_runtime_char_array_ptr[id]`）到 slot1 / slot2
- 其餘（非控制碼，實務上為小的非負 glyph_id）：索引 `data_fd2_chinese_font_sheet` 的字模，
  呼 `fd2_blit_glyph_1bpp_with_outline` 渲染，render_pos += 0x10

肖像載入四碼（`0xFFEF..0xFFEC`）都經 `fd2_load_dat_resource(0x51A70 = "DATO.DAT", …)` 取 sprite；
slot 決定 normal 還是 mirrored blit（見「Speaker 切換」）。每頁 opcode 完整語意見
`resource_info/fdtxt.md`。

## 字模渲染（1bpp）

`fd2_blit_glyph_1bpp_with_outline` 從 **1bpp** 字模 sheet 渲染 16x16 字元：每字模 0x20 bytes =
16 列 × 2 bytes = 每列 16 bit（1 bit/pixel）。流程：

1. 若 bg_color != 0：先以 bg_color 填 16x16 底色（每列 16 bytes，推進 pitch）。
2. 若 glyph_idx != 10（空白跳過）：逐列讀 byte-swap 後的 16 bit，MSB 先出；每個 set bit 寫
   `fill_color`，並在其正下方與左下方各補一個 `outline_color` 像素，形成 1-pixel 立體陰影。

7 個參數進入時先寫進 gfx 端狀態結構 `data_fd2_graphics_glyph_blit_state @ 0x627A3`（wPitch /
bFill_color / bBg_color / bOutline_color / pDst_buf / pFont_data / nGlyph_idx），供後續只更新單一
欄位的呼叫端沿用。中文字模來源 `data_fd2_chinese_font_sheet @ 0x53A75`（FDOTHER.DAT[4]）。

## 對話框組裝（17-tile 9-slice）

`fd2_assemble_dialog_frame_layered(dst, pitch, col_offset, row_offset, n_cols, n_rows)` 以 17 個
sprite tile 組出任意 (n_cols, n_rows) 大小的對話框，來源 sheet
`data_fd2_ui_anim_sprite_sheet_ptr @ 0x53A81`（與 UI / death anim 共用）。tile 分工：

- 外框角 1 / 2（上左 / 上右）、3 / 4（下左 / 下右）
- 內框角 5 / 6（上）、7 / 8（下）
- 可延展邊：9（上內邊，橫向平鋪 n_cols-2）、A（左邊，縱向平鋪 n_rows-2）、B（右邊）、C（下內邊）
- D 中央底填（鋪滿 n_cols × n_rows 內部）
- E / F 中列邊（左 / 右）、10 / 11 底列次要角

`fd2_play_dialog_open_animation` 以 5 段（top edge / top inner / middle / lower inner / bottom edge）
呼叫本函數，每段前把該畫面帶存進
`data_fd2_dialog_frame_layer_save_buffer_ptrs[0..4] @ 0x53A18`（5 個 `malloc(0x682C)` buffer），
關框時反 Z 序還原。同一組裝亦供 `fd2_dialog_open_speaker_portrait`、
`fd2_play_final_chapter_30_ending`、`fd2_render_status_screen_static_layout` 使用。

## Speaker 切換與面對面對話

`fd2_paint_portrait_to_dialog_area(portrait_idx)` 從
`data_fd2_portrait_sprite_buf_ptr @ 0x53A85`（目前 DATO.DAT sprite blob，首 byte = header 大小）
抽出第 `portrait_idx` 幀，依 `data_fd2_dialog_active_portrait_blit_offset @ 0x53C67` 決定 blit 方式：

- `== 0x9017`（右側槽 slot2）：`fd2_dialog_sprite_blit_mirrored`，水平翻轉、面向左
- 其餘（主要是 `0x728` 左側槽 slot1）：`fd2_dialog_sprite_blit_normal`，面向右

`0x53C67` 是位置 offset 兼「有無 active dialog」旗標，非敵我旗標：左槽(0x728) 正放、右槽(0x9017)
鏡射，正是「兩人面對面」的實作 —— 左邊角色面向右、右邊角色鏡射面向左。另有五個章節 intro 專屬
定位由 `fd2_dialog_open_speaker_portrait` 依 portrait_kind 設定：`0x80->0x10BB`、`0x81->0x6AB`、
`0x82->0xF63`、`0x83->0x576`、`0x84->0xE3C`（其餘 kind 落到預設右槽 0x9017）；收框時寫 0。

## ▼ 按鍵提示動畫

`fd2_wait_for_input_dialog_with_blink(animated_cursor_mode)`：讀 BIOS midnight tick（`0x46C`），
當 tick 相對上次前進 > 1 才推進一格動畫：

- `animated_cursor_mode == 1`：每 3 個推進格切換對話框「向下箭頭」frame（sprite `0x12` <-> `0x13`），
  畫在 `pos_offset + blink_period + 0x640`（pos_offset 依 slot 取 0xA0B4F / 0xA951F）。
- 講者嘴巴閃動採亂數倒數：countdown 用完時 `fd2_paint_portrait_to_dialog_area(3)`（張嘴），
  下一輪 `fd2_paint_portrait_to_dialog_area(0)`（閉嘴）並以 `fd2_advance_rng_state() % 30 + 2`
  重設倒數（亂數 2..31 tick，避免節奏過於規律）。

等到 `fd2_check_keyboard_buffer_nonempty` 有輸入後，`int386(0x16, …)` 讀 scancode 存入
`data_fd2_input_key_input_mode @ 0x53A8E` 並標準化：`0xE0` / `0x52` -> `0x1C`（Enter）、
`0x53` -> `0x01`（Esc），回傳該碼。

## Yes/No 確認框

問答框由兩個函數合成，兩者都以「角落 sprite 內折 / 外展」動畫呈現框收放：

- `fd2_text_dialog_typewriter_loop @ 0x19953`：打字機式逐字動畫 + 輸入迴圈。以 BIOS tick 節流
  （相差 >= 2 tick 才推進），左右鍵移游標 `data_fd2_ui_menu_cursor_idx`（正典見 `ui_menu.md`），
  Enter/Space/`0x39` 回 1（Yes/確定）、Esc/`0x53` 回 -1（取消）。
- `fd2_animate_dialog_page_advance_collapse @ 0x197E5`：確定後播 4 幀「兩角落向中內折」過場，
  底層戰場 / 下一頁同時重繪。

戰場中（`data_fd2_battle_tile_map_ptr > 1`）兩者都會先重建戰場底景（palette tick + tile map
composite + 角色 overlay），對應 pipeline 見 `gfx.md`。

## save-under（設定 / 狀態框）

覆蓋式設定 / 狀態小框以 save-under 保存底圖：`fd2_backup_dialog_area_to_buffer` 釋放舊 buffer 後
`malloc(0x1440)`（0x48 × 0x48 = 5184 bytes），自游標左上一格起逐列（stride 0x1C8）拷入
`data_fd2_dialog_area_backup_buffer @ 0x53A71`；關框時 `fd2_restore_dialog_area_from_buffer`
把同一塊貼回。

## Portrait 快取（FDICON.B24）

`fd2_load_portrait_to_cache(portrait_id, file_handle)` 把某 portrait 的 12 個 sprite frame 從
FDICON.B24 載入全域 200KB（`0x32A00`）快取，重複呼叫同 id 直接回既有 idx（idempotent）。章節載入時
`fd2_load_chapter_battle_data` 對每個上場角色呼叫它，回傳的 idx 存進 runtime_char.pSprite_state[0]。

快取結構（`data_fd2_portrait_sprite_cache @ 0x53A61`）：

```
[0 .. 0x77F]   sprite-offset 查表：容量 40 portrait × 12 sprite × 4-byte 絕對 offset（= 0x780）
[0x780 ..]     依序排放的 sprite raw data
data_fd2_resource_portrait_cache_buffer_used  @ 0x539EC  已用 byte 數（= data 尾端 offset）
data_fd2_resource_portrait_cache_count        @ 0x53BDF  已快取 portrait 數
data_fd2_resource_portrait_cache_id_list_base @ 0x53B17  第 N 個快取的 portrait_id（線性比對用）
```

流程：先 `fseek` 跳 6-byte 檔頭，讀 `0x1A40`（= 140 × 12 × 4）header 表取本 portrait 的 12 個
frame offset（+1 個尾標）。首次呼叫 `malloc` 200KB，sprite data 放到 buffer + `0x780`，建 12 個
offset entry。之後線性掃 `0x53B17` 找命中；命中回 idx，未命中則 append 到尾端。此快取來自
FDICON.B24，與對話 VM 用 DATO.DAT 載入的講者肖像（`data_fd2_portrait_sprite_buf_ptr`）是兩套資源。

## 對話相關 globals

| 位址 | 名稱 | 用途 |
|---|---|---|
| `0x53C67` | `data_fd2_dialog_active_portrait_blit_offset` | 肖像 mode13h 線性 offset 兼 active 旗標：0=無、0x728=左槽、0x9017=右槽(mirror)、0xC88=狀態畫面、0x10BB/0x6AB/0xF63/0x576/0xE3C=五個 intro 定位 |
| `0x53A85` | `data_fd2_portrait_sprite_buf_ptr` | 目前 DATO.DAT 講者肖像 sprite blob（首 byte = header 大小）|
| `0x53A75` | `data_fd2_chinese_font_sheet` | 中文字模 sheet（FDOTHER.DAT[4]，每字 0x20 bytes）|
| `0x53A7D` | `data_fd2_all_game_text_ptr` | 全遊戲共用文字 buffer；-4/-5 sub-dialog 的 text_base |
| `0x53A79` | `data_fd2_current_chapter_text_ptr` | 本章文字（FDTXT.DAT[chapter_id+1]，`fd2_load_chapter_battle_data` 載入），VM 典型 text_base |
| `0x53A81` | `data_fd2_ui_anim_sprite_sheet_ptr` | 對話框 17-tile + ▼ 箭頭 sprite sheet（與 UI / death anim 共用）|
| `0x53A18` | `data_fd2_dialog_frame_layer_save_buffer_ptrs` | 5 個 frame-layer save buffer 指標（void*[5]，各 `malloc(0x682C)`）|
| `0x53A71` | `data_fd2_dialog_area_backup_buffer` | save-under 快照（0x48×0x48 = 0x1440 bytes）|
| `0x53AD9` | `data_fd2_dialog_last_action_text_id_param` | opcode -4 sub-dialog 的 page index |
| `0x53ADD` | `data_fd2_dialog_drop_swap_text_id_param` | opcode -5 sub-dialog 的 page index |
| `0x53AE1` | `data_fd2_dialog_last_action_value_param` | opcode -6 代入的整數（金錢 / 數量 等）|
| `0x53A10` | `data_fd2_dialog_portrait_blink_frame_idx` | 講者嘴巴動畫 frame（畫面序列 0/1/2/1）|
| `0x53A14` | `data_fd2_dialog_portrait_blink_subtick_counter` | 嘴巴動畫 2-call 分頻器 |

肖像快取的四個全域見「Portrait 快取」節。Yes/No 游標 `data_fd2_ui_menu_cursor_idx`、slide workspace
`data_fd2_ui_slide_*_buf_ptr`（0x53C5B/0x53C5F/0x53C63）為 UI 共用，正典見 `ui_menu.md`。
