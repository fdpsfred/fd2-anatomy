# tools/kb_overhaul/

KB 全面翻新（依 `src/` ground truth 徹底重寫知識庫）過程用的產生器。所有 name map 一律從
finalized `assets/` 即時解析、不硬編；tile_pickup / 資料表語意皆對照 Ghidra + `src/` 已驗證結論。

| Script | 用途 |
|---|---|
| `gen_ch_encounters.py` | 從 FDFIELD.DAT 解出各章 §敵人配置 / §寶物 結構化資料，建於 `tools/decoders/` 的 byte-verified 解碼器（record 欄位對照 `fd2_init_runtime_char_for_battle`、tile_pickup kind 語意對照 `src/ui_menu/menufld.c`）|
| `gen_ch_section3.py` | 把 encounter JSON 格式化為 `chapters/chapter_NN.md` §③（敵人配置 / 寶物）markdown；enemy / item / char 名稱從 `assets/` 即時解析（gap-aware，未記錄的槽位保留未對映）|
| `gen_ch_shops.py` | 由 `data_fd2_chapter_intro_metadata_table @ 0x6238D`（26×31B）產各章 §商店 markdown；entry_index = chapter_n−1，僅 STORY 章顯示商店 |
| `rebase_row_addr.py` | 一次性：把 `assets/characters.md` / `assets/enemies.md` 逐列位址從跨版本 FD2.EXE 空間（Ghidra VA + 0x19014）rebase 回正典 Ghidra VA |

輸出目的地：`chapters/`（成品）與 `workspace/kb_overhaul/`（中繼 JSON / 證據；KB 不引用此 path）。
