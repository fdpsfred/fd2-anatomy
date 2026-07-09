# tests/ — FD2 測試架構

兩層測試，各自獨立建置與執行：

## 一、決定論 playthrough 整合測試（主力）

用腳本 scancode 驅動重建版遊戲在 DOSBox-X silent 全自動跑真實內容，在邏輯檢查點擷取
framebuffer + 遊戲狀態，對照 golden 與 KB 推導的期望值。這是行為等同原版的主要驗證手段。

- 程式：`tests/play/`（`replay.c` / `capture.c` / `probe.c` / `playharn.h`，只進 replay build）。詳見
  `tests/play/_index.md`。
- 工具：`tools/fd2_play/`（`build_replay.py` / `run_play.py` / `compare.py` / `expect.py` /
  `run_all.py` …）。詳見 `tools/fd2_play/_index.md`。
- 執行：

  ```
  python tools/fd2_play/build_replay.py          # 編 FD2RP.EXE（src + -DFD2_REPLAY + tests/play）
  python tools/fd2_play/run_play.py --scenario <name>
  python tools/fd2_play/run_all.py               # 跑整個 regression 套件
  ```

## 二、凍結 logic regression net（純計算地基）

只保留 callee ≤2、純函數、無 spy、不碰 `0xA0000`/AIL/BIOS 的 leaf 測試，當 determinism /
存檔加解密 / 查表這三類基礎的回歸防線。不再隨新 function 擴充。

| 測試檔 | 受測 src | 內容 |
|---|---|---|
| `battle/btlrng.c` | `src/battle/battle.c` | `fd2_advance_rng_state`（LFSR 決定論，replay 系統的時序基礎） |
| `save/savecsum.c` | `src/save/save.c` | `fd2_save_compute_checksum` + `fd2_save_crypt_buffer`（byte-sum 與 involution 串流加密，FD2.SAV round-trip 基礎） |
| `table/table.c` | `src/table/table.c` | item/spell/enemy/char/job/movement 等表查詢 accessor（含越界邊界） |

- 執行：`python tools/code_emit/build_test.py`（末行應為 `Results: N passed, 0 failed`）。
- 期望值皆 host 端以同一公式/LFSR 預算或硬算，非寫死常數。

## 退役：舊 per-function spy 單元測試

繪圖 / 走位 / 音效 / 戰鬥分派計數 / 各章 init·end / dialog 等高層 orchestrator 的 per-function
spy 單元測試已退役到 `legacy/tests_unit_spy/`（保留可追溯）。它們靠 link-time spy —— 在共享的
`testglob.c` 內以同名 stub 覆蓋真函數來觀察呼叫；當對應 function 全部正式 emit 後，stub 與真
body 形成 Watcom W1027 redefinition、且跨套件互相堆疊而集體失效（即舊 open_issues #32/#33 的
coordinated-landing，其 Phase 3「斷言重寫」隨退役一併作廢）。這些 domain 的覆蓋改由第一層的整合
scenario 承擔。新文件不引用 `legacy/`。

## 基礎建設（兩層共用或第二層用）

| 路徑 | 用途 |
|---|---|
| `include/testharn.h` | 測試框架巨集（`ASSERT_EQ` / `ASSERT_NE` / `ASSERT_MEM_EQ` / `RUN_TEST` / `SUITE_BEGIN` / `SUITE_END`） |
| `testmain.c` | 唯一 `main()` 與計分全域，依序呼叫各 logic-net runner（呼叫清單由 `genbuild.py` 自動維護） |
| `testglob.c` | logic-net 連結所需的共用 fake 全域與 stub（仍含退役套件用的 stub，無害；redefinition 為 W1027 警告，不影響 leaf 測試解析到真函數） |
| `genbuild.py` / `naming.py` / `where.py` | 掃 `src/` 與 `tests/` 重生 `build.bat` / `test.lnk` / `testmain.c`（嚴禁手改這三個生成檔） |
| `dosbox.conf` | DOSBox-X 設定：autoexec 只 mount + 設環境，再呼叫 `build.bat` |
| `dosbox_dbg.conf` | 前者的可視 debug 變體（`output=surface` 非 silent）：mount 後直接跑已建好的 `out/TEST.EXE` 供人工觀察，未被自動化工具引用 |

新建或移除 logic-net 測試檔後，跑 `python tests/genbuild.py --apply` 重接 build，再 `build_test.py` 過 gate。
