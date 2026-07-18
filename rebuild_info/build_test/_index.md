# rebuild_info/build_test/

把 `src/`（emit 出來的 C source）實際 compile + link 出可在 DOS 跑的 FD2.EXE、放進完整遊戲環境對照
原版實機 playtest 的工作流程與踩過的坑。

`ail/` `crt/` `equivalence/` `link/` 四個資料夾是「重建 FD2.LE 需要哪些東西」的**靜態解析**；本資料夾是
「真的把它建出來、跑起來、抓 bug」的**實作經驗**。這是本資料夾刻意的定位：允許受控的歷史 / 避坑敘述
（症狀→根因→教訓），但機制細節一律指向對應正典。

## 檔案

| 檔案 | 內容 |
|---|---|
| `playtest_bugs.md` | 實機 playtest 解過的 rebuild bug，按八個根因類別 A–H 整理（暫存器 clobber / BSS tentative scalar 相鄰 / 資料號性 / 熱迴圈時序 / 硬編位址 / math intrinsic 呼叫形式 / stack-probe 分佈 / 折疊基底歸錯符號），每筆含症狀 / 一句根因 / 教訓 / 排障經驗 / commit，機制細節指向正典 |
| `workflow.md` | 建置 / 連結 / playtest 方法論：兩個建置目標（TEST / FD2.EXE）+ 雙 main、undefined symbol worklist 神諭、host WDISASM 反組譯比對法、DOSBox-X fault logging、結束偵測、真實檔案測試、診斷工具 |
| `toolchain_quirks.md` | 重建任何 FD2 object 共通的低階 Watcom / DOS / DOSBox-X 陷阱（C89 / LFN / batch / rename / DOS4GW 啟動 / Ghidra label 重複）；AIL audio 專屬的見 `../ail/build_quirks.md` |

## 對應工具

- src-only 神諭與最終建置：`tools/fd2_build/_index.md`
- 實機 playtest 診斷腳本：`tools/snd_kbd_diag/_index.md`、`tools/stkdiag/_index.md`
- build_test gate：`tools/code_emit/_index.md`
