# FD2 (炎龍騎士團 2) 逆向工程資料庫

漢堂遊戲公司 1998 年發行遊戲「炎龍騎士團合輯版」裡面的二代 (Flame Dragon Knights 2 / FD2) 的逆向工程
專案。內容包含重建後可編譯的遊戲原始碼，以及對遊戲程式與資源的完整解析。依用途分為以下幾個 folder：

## Folder 用途

| Folder | 內容 |
| --- | --- |
| [`src/`](src) | 逆向工程重建的完整 C 原始碼，64 個 `.c` 依 anim / battle / field / gfx / spell / ui_menu / table / save 等子系統目錄組織，用 Watcom 9.5a 可編譯成在 DOS 下正確執行的 `FD2.EXE`。解析遊戲資訊時以 src/ 為主要依據，Ghidra 反組譯為輔 |
| [`program_info/`](program_info) | 對遊戲程式系統的解析。包含整體架構、12 個 system (battle / animation / save_load / ...)、章節生命週期與事件派遣機制 (field) |
| [`resource_info/`](resource_info) | 對每一個遊戲資源檔案格式的解析。FD2.LE 結構、FD2.SAV 存檔、11 個 LLLLLL DAT (FDTXT / FDFIELD / FDSHAP / FDOTHER / DATO / FDMUS / ...)、FDICON.B24、中文字編碼 |
| [`rebuild_info/`](rebuild_info) | 重建 FD2.LE / FD2.EXE 所需的 toolchain / lib / 連結環境資料，含等價鐵則、AIL 抽取、wlink 設定與實機 build test |
| [`libs/`](libs) | 重建連結所需的第三方 vendor lib（AIL v3 音訊函式庫 `ailv3.lib` 與標頭） |
| [`tests/`](tests) | src/ 的決定論 playthrough 整合測試（注入鍵盤事件驅動遊戲、擷取 framebuffer + state 比對 golden）；詳見 [`tests/_index.md`](tests/_index.md) |
| [`chapters/`](chapters) | 30 章唯一文件，每章一檔。劇情概要、加入角色、敵人/寶物/商店、特殊機制、init/end/post/event handler 流程、FDFIELD hook、FDTXT 對白全文；跨章機制 (天空之鑰、招募矩陣、結局分歧) 與 30 章 handler 總表在 [`_index.md`](chapters/_index.md) |
| [`assets/`](assets) | 從程式和資源檔解析出的遊戲數值內容。32 角色、215 道具、36 法術、68 敵人、27 職業、數值表、結局文字、字模對應表 |
| [`tools/`](tools) | 重複利用的 Python script。各資源檔的 parser/decoder、glyph lookup table 建表工具、CRT FidDb pipeline |

## 額外檔案

- [`open_issues.md`](open_issues.md) — 整理所有當前未解問題與未做分析，分 5 類列出
- [`docs/`](docs) — GitHub Pages 發佈目錄（非知識庫），目前放角色成長數值比較頁，由 [`tools/growth_table/`](tools/growth_table) 產生
- `workspace/` — 真 scratch 區域，POC 與一次性 sanity test 才放這。**KB / tool script / _index.md 都不能引用 workspace/ path**
- `legacy/` — 凍結的舊資料 (workflow 過程紀錄、舊 catalog、舊 ground_truth)；
  新文件不引用此目錄，裡面的所有內容都已過時，工作時絕對不能閱讀和參考

## 從哪裡開始讀

- 想了解遊戲整體架構：[`program_info/overview.md`](program_info/overview.md)
- 想讀重建的遊戲原始碼 / 編譯 FD2.EXE：[`src/`](src)（依子系統分目錄） + [`rebuild_info/build_test/_index.md`](rebuild_info/build_test/_index.md)
- 想實作存檔修改：[`resource_info/save_format.md`](resource_info/save_format.md) + [`assets/items.md`](assets/items.md)
- 想看遊戲劇情：[`chapters/_index.md`](chapters/_index.md) 然後依章閱讀
- 想寫資源檔解碼器：[`resource_info/overview.md`](resource_info/overview.md) + 對應檔案的 `.md`
- 想理解戰鬥 AI：[`program_info/battle.md`](program_info/battle.md)
- 想知道 FD2 用哪個編譯器和 CRT lib：[`rebuild_info/crt/fid_match.md`](rebuild_info/crt/fid_match.md)
- 想理解等價鐵則 / pool 分類 / fall-through pattern：[`rebuild_info/equivalence/_index.md`](rebuild_info/equivalence/_index.md)
- 想抽 AIL `.obj` 重建：[`rebuild_info/ail/_index.md`](rebuild_info/ail/_index.md) + [`tools/ail_extract/_index.md`](tools/ail_extract/_index.md)
- 想知道 FD2.LE 怎麼連結出來 / wlink 設定：[`rebuild_info/link/wlink_settings.md`](rebuild_info/link/wlink_settings.md) + [`rebuild_info/link/le_layout.md`](rebuild_info/link/le_layout.md)
- 想知道實機 playtest 解過哪些 rebuild bug / 怎麼建置測試 src-only FD2.EXE：[`rebuild_info/build_test/_index.md`](rebuild_info/build_test/_index.md)
- 想看每個 folder 的檔案清單：各 folder 內的 `_index.md`
