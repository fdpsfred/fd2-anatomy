# rsrc

章節資源載入模組：把 FD2 的封裝 DAT 檔（FDTXT / FDOTHER / FDFIELD / FDSHAP /
DATO / FDMUS）與 FDICON.B24 立繪、FD2.TMP 交換檔搬進遊戲的執行期緩衝區。所有從
資源檔取資料的路徑最終都經過本模組的核心載入器；各資源檔本身的檔案格式與 index
對照表見 `resource_info/overview.md` 及對應 DAT 檔的 `.md`。

## 驗證對象

- src：`src/rsrc/rsrc.c`
- Ghidra function（name@addr，已即時核對）：
  - `fd2_load_dat_resource @ 0x000111BA`（核心 DAT 載入器，3 參數）
  - `fd2_load_chapter_battle_data @ 0x0001088D`
  - `fd2_load_chapter_background_layers @ 0x00010652`
  - `fd2_load_portrait_to_cache @ 0x00011019`
  - `fd2_load_chapter_portraits_and_dump_tmp @ 0x00010B4E`
  - `fd2_dialog_open_speaker_portrait @ 0x0001956B`
  - `fd2_load_and_fade_in_cinematic_image @ 0x0001F81E`
  - `fd2_restore_portrait_cache_from_tmp @ 0x00029117`
  - `fd2_load_chapter_shop_item_ids @ 0x0002D392`
- Ghidra data（BSS，位址已核）：
  - `data_fd2_resource_last_loaded_resource_size @ 0x00053BFF`
  - `data_fd2_portrait_sprite_cache @ 0x00053A61`
- 相關資源檔：FDTXT、FDOTHER、FDFIELD、FDSHAP、DATO、FDMUS（皆走核心載入器）；
  FDICON.B24（立繪，直接 fopen）；FD2.TMP（立繪快取交換檔，格式見 `resource_info/`）

## 主要 functions

| 位址 | 名稱 | 角色 |
|---|---|---|
| `0x000111BA` | `fd2_load_dat_resource` | 核心 DAT 載入器；依 index 取一段資源到新 malloc 緩衝區 |
| `0x0001088D` | `fd2_load_chapter_battle_data` | 每章開戰前總管：載入該章全部戰鬥資料並建 runtime_char array |
| `0x00010652` | `fd2_load_chapter_background_layers` | 依章載 FDOTHER 背景圖層（單圖／寬幅雙圖／捲動過場三型） |
| `0x00011019` | `fd2_load_portrait_to_cache` | 把單一立繪的 12 幀從 FDICON.B24 塞進 200KB 立繪快取（有快取則直接回傳既有 idx） |
| `0x00010B4E` | `fd2_load_chapter_portraits_and_dump_tmp` | 依 race id 載該章立繪，並把整個立繪快取寫進 FD2.TMP |
| `0x00029117` | `fd2_restore_portrait_cache_from_tmp` | 從 FD2.TMP 讀回整個立繪快取 |
| `0x0001956B` | `fd2_dialog_open_speaker_portrait` | 從 DATO.DAT 載說話者立繪並播對話框滑入 |
| `0x0001F81E` | `fd2_load_and_fade_in_cinematic_image` | 載 FDOTHER palette、播 ANI 動畫、淡出到黑（結局過場） |
| `0x0002D392` | `fd2_load_chapter_shop_item_ids` | 從章節 intro metadata 取當前商店層的品項 id 清單 |

## 核心載入器 `fd2_load_dat_resource @ 0x111BA`

DAT 檔是簡單的封裝格式：檔頭 6-byte 前綴，之後接一組 32-bit 絕對位移（每個資源
一個，外加一個結尾標），再接封裝好的各資源負載。載入器以 index 定位並取出對應段落。

```c
uint32 fd2_load_dat_resource(uint32 fname, uint32 old_buf, uint32 index);
```

演算法：

```
if (old_buf) free(old_buf);                 // 呼叫端可傳前一次的緩衝區交回收
fp = fopen(fname, "rb");                     // 失敗 -> printf "File not found" + exit(1)
fseek(fp, index*4 + 6, SEEK_SET);            // +6 跳過 6-byte 檔頭前綴
fread(header, 1, 8, fp);                     // 讀 (start, end) 兩個 u32
size = end - start;
data_fd2_resource_last_loaded_resource_size = size;   // 寫 0x53BFF 供呼叫端沿用
buf = malloc(size);                          // 失敗 -> printf "Out of Memory" + exit(1)
fseek(fp, start, SEEK_SET);
fread(buf, 1, size, fp);
fclose(fp);
return buf;
```

`data_fd2_resource_last_loaded_resource_size @ 0x53BFF` 在每次載入時被寫入 `end -
start`，呼叫端不必自行追蹤 size；`fd2_set_bgm_track_with_fade` 就直接讀它，把剛載入
的 FDMUS 序列長度傳給 DPMI lock。兩個錯誤路徑（開檔失敗、記憶體不足）各自 inline 呼叫 `printf`
（File not found／Out of Memory）後，跳到 `fd2_load_save_and_init_engine` 內近函式開頭的共用
`exit(1)` stub（`@0x1005E`：`PUSH 1; JMP exit`）；僅 `exit(1)` tail 共用，`printf` 不共用、也與 `main` 無關。

## 章節資源總管 `fd2_load_chapter_battle_data @ 0x1088D`

每章開戰前呼叫一次，把該章的所有戰鬥資料一次載齊並建出 runtime_char array。呼叫端為
`fd2_init_battle_state_for_chapter`、`fd2_chapter_30_end`、`fd2_play_final_chapter_30_ending`。
流程：

1. `fd2_load_chapter_background_layers()`。
2. FDTXT.DAT[`chapter_id + 1`] -> 章節對話文字指標（本章戰鬥文字為 entry `chapter_id+1`）。
3. FDFIELD triplet，索引 `chapter_id*3 + {2,1,0}`：`+2` -> char_spawn_pos_table、
   `+1` -> tile_event_data_table、`+0` -> battle_tile_map。地圖寬高取自 tile_map 開頭兩個 u16。
4. `scene_id = tile_event[0]`；FDSHAP.DAT[`scene_id*2`] -> tile gfx、`[scene_id*2 + 1]`
   -> tile 屬性旗標緩衝區；接著 `fd2_battle_reset_tile_transient_state`。
5. 出戰人數 = tile_event[1]、field char 記錄數 = tile_event[2]。
6. 釋放舊立繪快取與舊 runtime_char array，`malloc(0x1E00)`（96 格 × 0x50-byte runtime_char）。
7. 開 FDICON.B24，逐格從共用選單隊伍樣板複製角色、填出戰座標、經
   `fd2_load_portrait_to_cache` 載立繪並 `fd2_recalculate_combat_stats`；空格清零並標記為死。
8. 收尾呼叫 `fd2_load_chapter_portraits_and_dump_tmp(0)`。

FDFIELD 各區段（turn hook / tile-step hook / pickup table / char_spawn_record）的完整
layout 見 `resource_info/fdfield.md`；章節生命週期與事件派遣見 `field.md`。malloc 或開
FDICON.B24 失敗時會先 INT 10h 切回文字模式再 printf + exit。

## 章節背景圖層 `fd2_load_chapter_background_layers @ 0x10652`

先 free 並清空 static 與 animated 兩個背景緩衝區，再依當前章號選三種載入型態之一（來源皆
FDOTHER.DAT）：

- 單圖預設：章 9/0x18/0x19 用 idx 0xF、章 0x1C/0x1D 用 idx 0x37、其餘用 idx 0x10；載一張
  sprite 進 static 緩衝區並另配一塊 320×200 工作緩衝區。
- 寬幅雙圖：章 0x11/0x15/0x16/0x1B；依章決定 `bg_width*bg_height`，配 static 緩衝區後把上半
  圖（y=0）與下半圖（y=bg_height/2）兩張 sprite 拼上去。
- 捲動過場：章 0x17；配 312×192 緩衝區（malloc 0xEA00 = 59904 bytes）、blit sheet idx 0x2A，並啟動文字捲動過場
  `fd2_scroll_text_screen_up_by_lines`。

static/animated 兩個緩衝區指標由本函式獨占管理（`data_fd2_graphics_static_bg_buffer_ptr
@ 0x53AFF`、`data_fd2_graphics_animated_bg_buffer_ptr @ 0x53B03`），消費端為
`fd2_composite_battle_tile_map`（背景合成）與 `fd2_scroll_text_screen_up_by_lines`；細節見 `gfx.md`。

## 立繪快取與 FD2.TMP 往返

立繪快取是一塊 `malloc(0x32A00)`（約 207KB）的堆積緩衝區 `data_fd2_portrait_sprite_cache
@ 0x53A61`，佈局：`[0..0x77F]` 為幀位移查找表（40 個立繪 × 12 幀 × 4-byte 絕對位移，共
0x780 bytes），`[0x780..]` 為逐立繪附加的封裝 sprite 負載。伴隨三個 BSS 標量：
`data_fd2_resource_portrait_cache_count`（目前快取數，0..40）、
`data_fd2_resource_portrait_cache_buffer_used`（負載尾端寫入游標）、
`data_fd2_resource_portrait_cache_id_list_base`（與快取格平行的 portrait_id 陣列，供命中查找）。

`fd2_load_portrait_to_cache` 從 FDICON.B24 讀 0x1A40-byte 的 sprite 標頭表（跳 6-byte magic），
取該立繪的 13 個 int（12 幀位移 + 1 結尾標），首次載入時初始化快取、否則附加到尾端；若
portrait_id 已在快取則直接回傳既有格 idx、不做 I/O。

FD2.TMP 是跨章節的立繪快取交換檔：`fd2_load_chapter_portraits_and_dump_tmp` 在章節 init 與
各章事件 handler 載完立繪後，把整個 0x32A00-byte 快取以 `fopen("FD2.TMP","wb")` +
`fwrite` 覆寫落盤；`fd2_restore_portrait_cache_from_tmp` 則對稱地把同樣 0x32A00 bytes 讀回一塊
新 malloc 的快取。還原路徑在 FIGANI 戰鬥過場（必殺技 / 全戰鬥過場 / 施法過場）釋放並改寫遊戲內
立繪與 tile 快取之後被呼叫，用預先落盤的檔案復原工作立繪集。FD2.TMP 的完整檔案格式見
`resource_info/`。

## 其他資源載入路徑

- `fd2_dialog_open_speaker_portrait @ 0x1956B`：配三塊 64000-byte 渲染工作區、快照當前畫面、
  組出對話框，再從 DATO.DAT 依 portrait_id 載說話者立繪（0x80..0x84 為五個固定座標的劇情角色特例，
  其餘走預設槽），最後播 6 幀滑入。對話框繪製與打字機文字見 `dialog.md`。
- `fd2_load_and_fade_in_cinematic_image @ 0x1F81E`：`palette_idx != -1` 時清 mode-13h 畫面並從
  FDOTHER.DAT 載該 palette，全亮度套用後播 ANI 動畫，尾端 fall-through 進
  `fd2_play_palette_fade_to_black` 淡出到黑；用於結局過場。
- `fd2_load_chapter_shop_item_ids @ 0x2D392`：依 intro 選單游標狀態選商店層（武器 / 道具 / 神秘），
  從已快取的章節 intro metadata entry 取品項 id 清單、遇 0xFF 空槽哨兵或層上限停止。商店選單樹見
  `town_menu.md`。

## 資源檔名常數

核心載入器的 `fname` 引數是各 DAT 檔名字串常數指標，命名為 `data_fd2_string_resource_filename_*`
（FDTXT / FDOTHER / FDFIELD / FDSHAP / DATO 等，DATO.DAT 字串 @ 0x51A70）；字串常數與各 DAT 的 index
域對照的正典見 `resource_info/overview.md` 與 `table.md`。

## 為何不把所有 caller 都歸到資源系統

歸屬看 function 的「主業」，不看它「用到什麼資源」。例如 `fd2_load_save_and_init_engine` 主業是
存讀檔，雖然會呼叫 `fd2_load_dat_resource("FDOTHER.DAT", ...)` 載立繪，仍歸 save 模組（見 `save.md`）。
核心載入器的 caller 散布於 save、CRT layer、battle、ui_menu、anim、field 各模組，本模組只保留「載入
機制」本身為正典，各 caller 的用途歸各自模組。
