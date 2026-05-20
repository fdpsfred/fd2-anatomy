# FD2.LE binary 結構

FD2.LE 是 DOS 32-bit Linear Executable，由 Open Watcom C++ 編譯，搭配 DOS/4GW
Protected Mode Extender 在 386+ 環境執行。檔案大小約 346,650 bytes。

## 三大 object segment

執行時由 LE loader 配置：

```
0x00010000  .object1 (code, ~252 KB)
  0x00010000-0x0004EBD8   game logic + Watcom CRT + Miles AIL library
                          (interleaved；非分區擺放)
  0x0003C964              entry point (crt_entry_start)
0x00050000  .object2 (靜態資料 + runtime state)
  0x00050000-0x00053900   string tables, jump tables, lookup data
  0x00053A00-0x000543FF   runtime variables (cursor pos, char array 等)
0x00060000  .object3 (initialization image, 遊戲資料表)
  0x000602AC data_fd2_battle_item_effect_table[215]
  0x000619FD data_fd2_battle_spell_effect_table[36]
  0x00061AF9 data_fd2_battle_enemy_data_table[68]
  0x00061DA1 data_fd2_battle_character_base_table[32]
  0x000620A1 data_fd2_battle_character_growth_table[68]
  0x0006238D data_fd2_chapter_intro_metadata_table[26]
  0x000626B3 data_fd2_battle_spell_learning_table[20]
  0x00063400+ orphan / unused 資料區
```

`.object1` 是程式碼，`.object2` 是執行期讀寫資料，`.object3` 是 initialization
image (靜態 game data tables)。

## 與 FD2.EXE 的對應

| 位址段                                            | FD2.LE 位址                   | FD2.EXE file offset     | Δ      |
| ------------------------------------------------- | ----------------------------- | ----------------------- | ------- |
| `.object2` 小資料表 (job_*, ...)                | `0x00051xxx`                | `0x76xxx`-`0x77xxx` | +0x200  |
| `.object3` 大資料表 (item/spell/char/enemy/...) | `0x00060xxx`-`0x00063xxx` | `0x792xx`-`0x7Bxxx` | +0xC200 |

`.object2` 小表 (job_magic_resist, job_crit) 與 `.object3` 大表使用不同跨版本偏移，
推測 Open Watcom C++ 把「含巨量靜態資料的 struct」放到 `.object3`（initialization
image），而把「簡單的 u32 或 u8 array」留在 `.object2` 與 C runtime 字串混放。

## Library boundary

`.object1` **沒有**清楚的 library / 遊戲分區 — Watcom linker 把 FD2 自寫
遊戲邏輯、Watcom CRT、Miles AIL library 三類函式 **互相交錯擺放**。可觀察到的
只有「函式較密集」的區段：

- **FD2 自寫遊戲邏輯**：散布於整個 `.object1`，但 `0x10000-0x36000` 較密集
- **Watcom CRT helper**：散布於整個 `.object1`，但 `0x36000-0x37700` 與
  `0x3D000-0x3E000` 附近較密集（如 `__CHK @ 0x36cd7`）
- **Miles AIL library**：第一個 AIL 函式 @ `0x379EE`、最後一個 @ `0x3C2E6`，
  `0x37000-0x3C2E6` 是 AIL 函式較密集的區段，但區段內仍混入其他類別函式
- **entry point**：`0x3C964` (`crt_entry_start`)

判別任一函式屬於哪類，必須看：函式名稱前綴（`crt_*` / `AIL_*` / 已命名 game
function）、callee 模式、字串引用 — **不能依 address range**。

整個 `.object1` 共約 27 KB padding 經分類後已套 byte[N] data type 覆蓋未定型
區段，避免 Ghidra listing 留下 undefined byte。Padding 分類（依緊鄰命名來源）：

| Category        |        Ranges | 來源                                                                 |
| --------------- | ------------: | -------------------------------------------------------------------- |
| miles           |            22 | 緊鄰 `AIL_*` named function                                        |
| watcom_rtl     |             5 | 緊鄰 `crt_*` / `rtl_*` / `ThunkJmp` named function             |
| watcom_unnamed |           137 | 緊鄰未命名 `FUN_*` (Watcom CRT helpers，FPU emulator / IO / 例外處理) |
| watcom_other   |             5 | 其他名稱模式                                                         |
| **Total** | **169** | ~27.4 KB                                                             |

## DOS/4GW

DOS/4GW 是 Tenberry 的 32-bit Protected Mode Extender，FLAME2 目錄下的
`DOS4GW.EXE` 是 stub。它把 16-bit DOS BIOS 與 32-bit flat memory model 接起來，
讓 FD2 可以直接讀寫 BIOS data area (例如 `0x40:001A` 的鍵盤 buffer pointer)。

## 主要 entry chain

```
crt_entry_start @ 0x3C964
  └─ crt_main_trampoline @ 0x45D4B  (Watcom CRT startup, _main argument parsing)
      └─ fd2_main @ 0x25BF4
          ├─ AIL_startup()                — Miles AIL init
          ├─ load .DAT resources           — 8 個 FDOTHER + FDTXT idx 0 等
          ├─ malloc 大型 buffer (game state 152 KB 等)
          └─ outer loop:
              ├─ draw_main_menu
              ├─ main_menu_continue_dispatcher
              └─ if entered game: chapter_init → game_main_loop → chapter_end → next_chapter
```
