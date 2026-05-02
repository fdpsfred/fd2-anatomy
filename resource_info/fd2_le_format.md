# FD2.LE binary 結構

FD2.LE 是 DOS 32-bit Linear Executable，由 Borland C++ 編譯，搭配 DOS/4GW
Protected Mode Extender 在 386+ 環境執行。檔案大小約 346,650 bytes。

## 三大 object segment

執行時由 LE loader 配置：

```
0x00010000  .object1 (code, ~252 KB)
  0x00010000-0x00036000   FD2 遊戲邏輯 (~154 KB)
  0x00036000-0x00037000   Borland CRT 前段 (malloc/fopen/fclose 核心)
  0x00037000-0x0003C300   Miles Sound System library (~21 KB)
  0x0003C300-0x0003CAAA   雜項 CRT
  0x0003C964              entry point (crt_entry_start)
  0x0003CAAA-0x0004EBD8   更多 CRT + low-level helpers + table_accessor
0x00050000  .object2 (靜態資料 + runtime state)
  0x00050000-0x00053900   string tables, jump tables, lookup data
  0x00053A00-0x000543FF   runtime variables (cursor pos, char array 等)
0x00060000  .object3 (initialization image, 遊戲資料表)
  0x000602AC item_effect_table[215]
  0x000619FD spell_effect_table[36]
  0x00061AF9 enemy_data_table[68]
  0x00061DA1 character_base_table[32]
  0x000620A1 character_growth_table[68]
  0x00062390 shop_table[28]
  0x000626B3 spell_learning_table[20]
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
推測 Borland C++ 把「含巨量靜態資料的 struct」放到 `.object3`，而把「簡單的 u32
或 u8 array」留在 `.object2` 與 C runtime 字串混放。

## Library boundary

`.object1` 有清楚的 library / 遊戲 boundary：

- **FD2 自寫遊戲邏輯**：`0x10000-0x36000` (~154 KB)
- **Borland CRT 前段**：`0x36000-0x37000` (~4 KB) — malloc、fopen、fclose 核心
- **Miles AIL library**：`0x37000-0x3C2E6` (~21 KB) — 第一個 AIL 函式 @ `0x379EE`,
  最後一個 @ `0x3C2E6`
- **entry point**：`0x3C964` (`crt_entry_start`)
- **更多 CRT + helpers**：`0x3CAAA-0x4EBD8` (~74 KB)

整個 `0x36000-0x4EBD8` 共約 27 KB padding 經分類為 lib zone，已套 byte[N] data type
覆蓋未定型區段，避免 Ghidra listing 留下 undefined byte。Padding 分類：

| Category        |        Ranges | 來源                                                                 |
| --------------- | ------------: | -------------------------------------------------------------------- |
| miles           |            22 | 緊鄰 `AIL_*` named function                                        |
| borland_rtl     |             5 | 緊鄰 `crt_*` / `rtl_*` / `ThunkJmp` named function             |
| borland_unnamed |           137 | 緊鄰未命名 `FUN_*` (Borland helpers，FPU emulator / IO / 例外處理) |
| borland_other   |             5 | 其他名稱模式                                                         |
| **Total** | **169** | ~27.4 KB                                                             |

## DOS/4GW

DOS/4GW 是 Tenberry 的 32-bit Protected Mode Extender，FLAME2 目錄下的
`DOS4GW.EXE` 是 stub。它把 16-bit DOS BIOS 與 32-bit flat memory model 接起來，
讓 FD2 可以直接讀寫 BIOS data area (例如 `0x40:001A` 的鍵盤 buffer pointer)。

## 主要 entry chain

```
crt_entry_start @ 0x3C964
  └─ crt_main_trampoline @ 0x45D4B  (Borland CRT startup, _main argument parsing)
      └─ fd2_main @ 0x25BF4
          ├─ AIL_startup()                — Miles AIL init
          ├─ load .DAT resources           — 8 個 FDOTHER + FDTXT idx 0 等
          ├─ malloc 大型 buffer (game state 152 KB 等)
          └─ outer loop:
              ├─ draw_main_menu
              ├─ main_menu_continue_dispatcher
              └─ if entered game: chapter_init → game_main_loop → chapter_end → next_chapter
```
