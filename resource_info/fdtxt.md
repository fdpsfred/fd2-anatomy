# FDTXT.DAT — 對話文字資源檔

FD2 全遊戲對話文字 (中文角色字幕、選單訊息、結局文字) 都打包在此檔。
file size 120,502 bytes，34 entries (idx 0..33)，1016 個 dialog page，共 51,155
個 glyph 引用。

## 檔案格式

LLLLLL archive (詳 `overview.md`):

```
+0x00..+0x05    "LLLLLL" (6 × 0x4C)
+0x06..+0x91    35 × u32 LE offsets (offsets[0..33] = entry 起點, offsets[34] = file size sentinel)
+0x92..EOF      34 個 entry 的 payload, contiguous
```

`fd2_load_dat_resource(idx)` 從 offset `idx*4 + 6` 讀 8 bytes 拿 `(start, end)` 配對
然後 fread `end-start` bytes。

## Entry idx → 用途對照表

| idx | chapter_id | 用途 | size (bytes) | pages |
|---|---|---|---|---|
| 0  | —    | `data_fd2_all_game_text_ptr` (global)   | 7636  | 661 |
| 1  | 0    | chapter 1 dialog           | 4360  | 12  |
| 2  | 1    | chapter 2 dialog           | 3498  | 17  |
| 3  | 2    | chapter 3 dialog           | 2458  | 10  |
| 4  | 3    | chapter 4 dialog           | 864   | 7   |
| 5  | 4    | chapter 5 dialog           | 3106  | 12  |
| 6  | 5    | chapter 6 dialog           | 3208  | 8   |
| 7  | 6    | chapter 7 dialog           | 2022  | 6   |
| 8  | 7    | chapter 8 dialog           | 1896  | 5   |
| 9  | 8    | chapter 9 dialog           | 2412  | 5   |
| 10 | 9    | chapter 10 dialog          | 4494  | 6   |
| 11 | 10   | chapter 11 dialog          | 2448  | 4   |
| 12 | 11   | chapter 12 dialog          | 1550  | 5   |
| 13 | 12   | chapter 13 dialog          | 2360  | 12  |
| 14 | 13   | chapter 14 dialog          | 1866  | 4   |
| 15 | 14   | chapter 15 dialog          | 4774  | 14  |
| 16 | 15   | chapter 16 dialog          | 3258  | 5   |
| 17 | 16   | chapter 17 dialog          | 4434  | 9   |
| 18 | 17   | chapter 18 dialog          | 4094  | 11  |
| 19 | 18   | chapter 19 dialog          | 1910  | 4   |
| 20 | 19   | chapter 20 dialog          | 3836  | 17  |
| 21 | 20   | chapter 21 dialog          | 4660  | 11  |
| 22 | 21   | chapter 22 dialog          | 2742  | 7   |
| 23 | 22   | chapter 23 dialog          | 7168  | 18  |
| 24 | 23   | chapter 24 dialog          | 1722  | 4   |
| 25 | 24   | chapter 25 dialog          | 3350  | 8   |
| 26 | 25   | chapter 26 dialog          | 5004  | 12  |
| 27 | 26   | chapter 27 dialog          | 5064  | 24  |
| 28 | 27   | chapter 28 dialog          | 1628  | 8   |
| 29 | 28   | chapter 29 dialog          | 4792  | 16  |
| 30 | 29   | chapter 30 dialog          | 5762  | 11  |
| 31 | 30   | endgame epilogue           | 6756  | 46  |
| 32 | 31   | endgame extras             | 2204  | 11  |
| 33 | 32   | endgame extras             | 3020  | 6   |

`chapter_id` 是 0-indexed (ch1 = 0, ch30 = 29)。FDTXT idx = `chapter_id + 1`。

## 載入時機 (3 個 callsite)

| Callsite | 函式 | 條件 | 目標 buffer |
|---|---|---|---|
| `0x25D07` | `main` | 程式啟動 (一次性) | `data_fd2_all_game_text_ptr @ 0x53A7D` ← idx 0 |
| `0x108B7` | `fd2_load_chapter_battle_data` | 每章開戰前 | `data_fd2_current_chapter_text_ptr @ 0x53A79` ← idx = chapter_id + 1 |
| `0x101E9` | `fd2_load_save_and_init_engine` | save 載入 | `data_fd2_current_chapter_text_ptr` ← idx = chapter_id + 1 |

## Entry payload layout

每個 entry 內部：

```
+0x00 .. +(2N-1)    N × u16 LE   page offsets (entry-relative)
+page_offsets[i] .. END opcode    page i 的 bytecode
```

關鍵：`N = page_offsets[0] / 2`（offset table 自身結束 = 第一個 page 起點）。
每個 page 以 `0xFFFF` (END) 終止；page 之間偶有少量 padding byte，parser 一律以
`page_offsets[]` 定位每頁起點，不受 padding 影響。

範例 entry 1 (ch1, 4360 bytes) 開頭 16 bytes：
`18 00 3C 01 CC 01 86 04 60 07 A8 07 90 0A 32 0C` →
- offsets[0] = 0x0018 → N = 12 pages
- offsets[1] = 0x013C, offsets[2] = 0x01CC, ..., offsets[11] = 0x10F4

## Dialog VM bytecode

每個 page 是 little-endian u16 stream，由 `fd2_display_dialog_scene @ 0x15F84`
逐一 dispatch 直到 `0xFFFF` END。

### 10 個 control opcodes

| u16 value | 名稱 | args | 語意 |
|---|---|---|---|
| `0xFFFF` | `END` | 0 | 終止 page (return from `fd2_display_dialog_scene`) |
| `0xFFFE` | `LINE_ADVANCE` | 0 | 推進到下一行 (line_count++ 後重算 render 位置)，不等待按鍵；portrait active 且 line_count==3 時觸發 cinematic scroll |
| `0xFFFD` | `PAGE_BREAK` | 0 | 推進到下一行後 paint portrait (若 active) 並等待玩家按鍵 (`fd2_wait_for_input_dialog_with_blink(1)`)；同樣有 line_count==3 的 cinematic scroll |
| `0xFFFC` | `SUB_DIALOG_A` | 0 | 遞迴呼叫 `fd2_display_dialog_scene` 載入 `data_fd2_all_game_text_ptr[data_fd2_dialog_last_action_text_id_param]` 的 page |
| `0xFFFB` | `SUB_DIALOG_B` | 0 | 遞迴載入 `data_fd2_all_game_text_ptr[data_fd2_dialog_drop_swap_text_id_param]` 的 page |
| `0xFFFA` | `NUMBER` | 0 | runtime 數字代入（`sprintf @ 0x377D9` 以格式字串 `"%d" @ 0x5014C` 格式化 `data_fd2_dialog_last_action_value_param`，再逐位 blit 數字字模）|
| `0xFFEF` | `PORTRAIT_LEFT_BY_ID` | 1 (portrait_id) | 左側 portrait (`data_fd2_dialog_active_portrait_blit_offset = 0x728`) |
| `0xFFEE` | `PORTRAIT_RIGHT_BY_ID` | 1 (portrait_id) | 右側 portrait (`data_fd2_dialog_active_portrait_blit_offset = 0x9017`) |
| `0xFFED` | `PORTRAIT_LEFT_BY_CHAR` | 1 (runtime_char_array idx) | 左側 portrait (用 `runtime_char[idx].bPortrait_id`) |
| `0xFFEC` | `PORTRAIT_RIGHT_BY_CHAR` | 1 (runtime_char_array idx) | 右側 portrait (同上) |

任何 < `0xFFEC` 的 u16 都被解讀為 `TEXT_CHARACTER`，直接傳入
`fd2_blit_glyph_1bpp_with_outline(code = u16, atlas = data_fd2_chinese_font_sheet, ...)`
渲染一個字模。

### 控制碼出現次數 (across 1016 pages, 51155 glyphs)

| 名稱 | count |
|---|---|
| `LINE_ADVANCE` (0xFFFE)   | 3620 |
| `END`                     | 1016 |
| `PORTRAIT_RIGHT_BY_ID`    | 813  |
| `PAGE_BREAK` (0xFFFD)     | 435  |
| `PORTRAIT_LEFT_BY_ID`     | 364  |
| `PORTRAIT_LEFT_BY_CHAR`   | 231  |
| `PORTRAIT_RIGHT_BY_CHAR`  | 42   |
| `SUB_DIALOG_A`            | 18   |
| `NUMBER`                  | 17   |
| `SUB_DIALOG_B`            | 1    |
| `TEXT_CHARACTER` (glyph)  | 51155 |

## Glyph 編碼

任何 < `0xFFEC` 的 u16 (TEXT_CHARACTER) 就是 glyph_id，直接索引
`data_fd2_chinese_font_sheet @ 0x53A75` (= FDOTHER.DAT[4]) 這張 atlas 的 fixed-size sprite。
`NUMBER` opcode (`0xFFFA`) 把 `data_fd2_dialog_last_action_value_param` 交給
`sprintf @ 0x377D9`（格式字串 `"%d" @ 0x5014C`），再把輸出 buffer 逐位（char − 0x30）
轉成 atlas 索引 0..9 的數字字模。

編碼性質 (direct atlas index、非 Big5)、glyph_id 範圍、ASCII 對應、字模 16×16 1bpp
規格，以及完整 glyph_id ↔ 中文字 lookup，統一見 `chinese_glyph_encoding.md`。

## Dialog rendering pipeline

`fd2_display_dialog_scene` 是完整 VM (do-while loop byte-by-byte u16 dispatch)。
下游組件由 dialog system 詳述 (`program_info/dialog.md`)：

- `fd2_play_dialog_open_animation` — 5-stage 對話框 slide-in
- `fd2_paint_portrait_to_dialog_area` — speaker 切換 (mirrored vs normal blit)
- `fd2_wait_for_input_dialog_with_blink` — ▼ 按鍵提示動畫
- `fd2_blit_glyph_1bpp_with_outline` — 16×16 字模渲染 (含 outline)
- `fd2_scroll_portrait_dialog_text_up_one_line` — `LINE_ADVANCE` / `PAGE_BREAK` 在 portrait active 且 line_count==3 時觸發的 scroll-up
- `fd2_close_dialog_panels_then_slide_out_to_cursor` — `END` 後 slide-out

## 內容 dump

每個 entry 的解碼結果 (中文已 substitute) 落在 assets/：
- entry 0  → `assets/text/global_text.md`
- entry 1..30 → `chapters/chapter_NN.md` 的「對話」段
- entry 31..33 → `assets/text/endgame_text.md`

## 未使用頁（開發者佔位／彩蛋台詞）

entry 1..6（ch1..ch6 戰鬥對話）各自恰好含 **1 頁永遠不會被顯示的孤兒頁**：程式碼沒有任何路徑會把該頁 index 傳給 `fd2_display_dialog_scene(data_fd2_current_chapter_text_ptr, …)`。內容都是主角索爾（`PORTRAIT_RIGHT_BY_ID=0x0000`）的一句無厘頭諧音獨白，兩種台詞輪流出現：

| entry | 章 | 孤兒頁 index | entry 總頁數 | 台詞 |
|---|---|---|---|---|
| 1 | 1 | 10 | 12 | 奈野啊捏？ |
| 2 | 2 | 5  | 17 | 奈野啊捏？ |
| 3 | 3 | 8  | 10 | 這‥‥這是什麼碗糕！ |
| 4 | 4 | 5  | 7  | 這‥‥這是什麼碗糕！ |
| 5 | 5 | 10 | 12 | 這‥‥這是什麼碗糕！ |
| 6 | 6 | 7  | 8  | 奈野啊捏？ |

判定依據：一個 entry 的每一頁只能由四種派遣面之一以頁碼引用——章節 init/end/post handler、16 個 FDFIELD turn-event hook、16 個 tile-step + 16 個 tile_pickup 事件格（皆經 consequence table 派到 `chevt1/chevt2` handler），以及每筆 char_spawn 的 kill-drop。頁碼幾乎都是 handler 內寫死的常數；**唯一以執行期資料當頁碼**的是 type-3「DIALOG」kill-drop（`pickup_kind=3 param=K` 的單位死亡時，`fd2_process_battle_drop_entries` 顯示該章第 K 頁，即頭目／友軍臨終台詞，如 ch1 p8、ch2 p11–16、ch3 p5/p9、ch4 p6、ch5 p8）。把每章 FDFIELD.DAT 的 spawn 記錄全解碼比對後，上表這些頁的 param 值不被任何一筆記錄命中，也沒有 handler 傳入其常數，故確定不可達。對話 VM 的遞迴 opcode（`SUB_DIALOG_A/B`）只索引 `data_fd2_all_game_text_ptr`（全域 blob），碰不到章節指標；唯一隱藏熱鍵 Shift+F1 只開神秘商店、不呼叫對話顯示。頁碼各章不同（10/5/8/5/10/7）排除「湊頁數」填充，性質是寫劇本時用主角 portrait 測渲染路徑後留下的佔位字串。

## 工具

- Parser：`tools/decoders/fdtxt_parser.py` (FdtxtArchive class, tokenize generator, CLI)
- Dump 產生器：`tools/decoders/fdtxt_dialog_decoder.py` (中文 substitute markdown 輸出)
