# rebuild_info/

支撐「把 FD2.LE 重建成可在 DOS 環境下重新編譯出等價執行檔」這個目標
的研究紀錄。`program_info/` 著重在「FD2 現在做什麼」，本資料夾著重
在「重建這個 binary 需要哪些 toolchain / lib / 連結環境細節，以及實際建出來、
跑起來、抓 bug 的經驗」。等價層級的目標是 Layer 2（functionally-exact），
不追求 Layer 3（byte-exact），詳見 `emission/pipeline_spec.md`。

## 結構

| Sub-folder | 內容 |
|---|---|
| `ail/` | Miles AIL audio library 在 FD2.LE 內的 inventory、抽 `.obj` 邊界、CRT 替換 EXTDEF、ABI 兼容性 |
| `crt/` | Watcom 9.5a CRT 的命名約定、符號 inventory、`crt_*` wrapper、Function ID lookup table（驗證細節 inline 進 lookup entry notes） |
| `emission/` | 把全 function 的 decompiled state 產出 C source 並重新 compile 為 functionally-equivalent FD2.LE 的 pool 路由、calling convention、fall-through pattern、全程式 call graph |
| `link/` | FD2.LE 的 LE binary layout (3 object / DGROUP 內部排列 / fixup section 統計 / 入口流程) 與從 binary 反推的 `wlink` 連結命令、`system dos4g` directive、`option stack=4K`、DOS bind stub |
| `build_test/` | 把 `src/` 實際 compile + link 出 FD2.EXE、放進完整遊戲環境實機 playtest 的工作流程與解過的 bug（暫存器 clobber / BSS 相鄰 / 號性 / 熱迴圈時序 / 硬編位址五類根因） |

各 sub-folder 內含自己的 `_index.md`。對照用 4 個版本 fidb (9.5a 正本 + 9.5/9.5b/9.5c 對照組) 放在 `workspace/crt_fid_match/fidb/`。
