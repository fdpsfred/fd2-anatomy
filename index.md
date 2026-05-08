# FD2 (炎龍騎士團 2) 逆向工程資料庫

漢堂遊戲公司 1998 年發行遊戲「炎龍騎士團合輯版」裡面的二代 (Flame Dragon Knights 2 / FD2) 的逆向工程
資料整理。資料分五類，依用途決定該看哪個 folder：

## Folder 用途

| Folder             | 內容                                                                                                                                                          |
| ------------------ | ------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| `fd2_game_files/`  | 所有遊戲檔案。本逆向工程的對象檔案是炎龍騎士團合輯版裡面的二代。|
| `program_info/`  | 對遊戲程式系統的解析。包含整體架構、12 個 system (battle / animation / save_load / ...)、章節 init/end handler 函數流程、章節事件派遣機制                     |
| `resource_info/` | 對每一個遊戲資源檔案格式的解析。FD2.LE 結構、FD2.SAV 存檔、11 個 LLLLLL DAT (FDTXT / FDFIELD / FDSHAP / FDOTHER / DATO / FDMUS / ...)、FDICON.B24、中文字編碼 |
| `rebuild_info/`  | 重建 FD2.LE 為可重新編譯執行檔所需的 toolchain / lib / 連結環境資料 |
| `assets/`        | 從程式和資源檔解析出的遊戲內容。32 角色、215 道具、36 法術、68 敵人、27 職業、30 章劇情/招募/敵人/對話、結局文字、字模對應表                                  |
| `tools/`         | 重複利用的 Python script。各資源檔的 parser/decoder、glyph lookup table 建表工具、CRT FidDb pipeline                                                          |

## 額外檔案

- `open_issues.md` — 整理所有當前未解問題與未做分析，分 5 類列出
- `workspace/` — 暫時性檔案目錄
- `legacy/` — 凍結的舊資料 (workflow 過程紀錄、舊 catalog、舊 ground_truth)；
  新文件不引用此目錄，裡面的所有內容都已過時，工作時絕對不能閱讀和參考

## 從哪裡開始讀

- 想了解遊戲整體架構：`program_info/overview.md`
- 想實作存檔修改：`resource_info/save_format.md` + `assets/items.md`
- 想看遊戲劇情：`assets/chapters/_index.md` 然後依章閱讀
- 想寫資源檔解碼器：`resource_info/overview.md` + 對應檔案的 `.md`
- 想理解戰鬥 AI：`program_info/battle.md`
- 想知道 FD2 用哪個編譯器和 CRT lib：`rebuild_info/crt_fid_match.md`
- 想看每個 folder 的檔案清單：各 folder 內的 `_index.md`
