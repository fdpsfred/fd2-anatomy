# rebuild_info/build_test/

把 `src/`（emit 出來的 C source）實際 compile + link 出可在 DOS 跑的 FD2.EXE、放進完整遊戲環境對照
原版實機 playtest 的工作流程與踩過的坑。

`ail/` `crt/` `emission/` `link/` 四個資料夾是「重建 FD2.LE 需要哪些東西」的**靜態解析**；本資料夾是
「真的把它建出來、跑起來、抓 bug」的**實作經驗**。內容涉及開發過程的歷史與踩坑紀錄（這是本資料夾的
刻意定位）。

## 檔案

| 檔案 | 內容 |
|---|---|
| `playtest_bugs.md` | 實機 playtest 解過的 rebuild bug，按五個根因類別整理（暫存器 clobber / BSS 相鄰 / 號性 / 熱迴圈時序 / 硬編位址），每筆含症狀 / 根因 / 修法 / 教訓 / commit |
| `workflow.md` | 建置 / 連結 / playtest 方法論：兩個建置目標（TEST / FD2.EXE）+ 雙 main、src-only fd2.lnk 神諭、實際連結設定（`system dos4g` 不自動 pull CRT 的實測）、host WDISASM 反組譯比對法、DOSBox-X fault logging、診斷工具 |
| `toolchain_quirks.md` | 重建任何 FD2 object 共通的低階 Watcom / DOS / DOSBox-X 陷阱（C89 / LFN / batch / rename / DOS4GW 啟動 / Ghidra label 重複）；AIL audio 專屬的見 `../ail/build_quirks.md` |

## 對應工具

- src-only 神諭與最終建置：`tools/fd2_build/_index.md`
- 實機 playtest 診斷腳本：`tools/snd_kbd_diag/_index.md`
- build_test gate：`tools/emit/_index.md`
