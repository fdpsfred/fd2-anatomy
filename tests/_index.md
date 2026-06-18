# tests/ — FD2 單元測試架構

每個測試檔對應一個 src 子檔：`tests/<domain>/<stem>.c` 測試 `src/<domain>/<stem>.c` 裡的 function。要找某個 function 的測試，照它的 src 路徑去同名測試檔即可。每個測試檔都維持在 1000 行以內；某個 src 子檔的測試太多時，會再切成 `<stem>1.c`、`<stem>2.c`。

## 執行方式

```
python tools/emit/build_test.py
```

這會啟動 DOSBox-X 跑 `tests/dosbox.conf`，編譯 `src/` 與 `tests/` 下所有檔案、連結成 `tests/OUT/TEST.EXE`、執行並把結果寫到 `tests/OUT/test.out`，最後回傳 JSON（`gate_pass` / 通過數 / 失敗數 / 警告）。也可以直接 `dosbox-x -silent -conf tests/dosbox.conf` 後讀 `tests/OUT/test.out`，末行應為 `Results: N passed, 0 failed`。

## 檔案結構

| 路徑 | 用途 |
|---|---|
| `tests/<domain>/<stem>.c` | 對應 `src/<domain>/<stem>.c` 的測試，匯出 `run_<domain>_<stem>_tests()` |
| `testmain.c` | 唯一的 `main()` 與計分用全域變數，依序呼叫各測試檔的 runner（呼叫清單由工具自動維護） |
| `testglob.c` | 所有受測函式依賴的假全域變數與 stub；function pointer table 必須初始化指向 noop |
| `include/testharn.h` | 測試框架巨集（`ASSERT_EQ` / `RUN_TEST` / `SUITE_BEGIN` 等） |
| `include/<domain>fix.h` | 跨多個測試檔共用的輔助函式與緩衝區（例如 `battlfix.h`） |
| `dosbox.conf` | DOSBox-X 設定：autoexec 只做 mount 與環境變數，再呼叫 `build.bat` |
| `build.bat` | 實際的編譯／連結／執行指令清單（放在磁碟檔，沒有 autoexec 的行數上限） |
| `test.lnk` | wlink 設定，列出所有 .obj（src 與 test） |
| `where.py` | 給一個 src target，回報新測試該寫進哪個測試檔與 runner（自動處理已切分的檔）。emit workflow 用它決定落點 |
| `genbuild.py` | 掃描 `src/` 與 `tests/`，重新產生 `build.bat` 的 src 與 test 兩個編譯區、`test.lnk`、`testmain.c` 的 runner 清單。新建任何 src 或測試 .c 檔後跑它接上 build |
| `naming.py` | `where.py` / `genbuild.py` 共用的名稱推導（8.3 檔名、唯一 obj 名、runner 名、共用標頭名） |

`build.bat` 的 src/test 編譯區、`test.lnk` 的所有 obj、`testmain.c` 的 runner 清單，都由 `python tests/genbuild.py --apply` 從 `src/` 與 `tests/` 自動產生，不必手動維護（嚴禁手改這三個檔）。現有 src obj 名與順序會被保留，新檔以 stem 去底線（≤8、唯一）命名 append。

## 新增一個 function 的測試

1. 看 function 在哪個 `src/<domain>/<stem>.c`。
2. 跑 `python tests/where.py <domain>/<stem>.c`，它會直接回報該寫進哪個測試檔與哪個 `run_*_tests()`（自動處理已切分的檔，並在需要新建檔時提示）。在該檔加入 `static void test_xxx(void)`，並用 `RUN_TEST(test_xxx)` 在回報的 runner 註冊。
3. 新的 stub 或假全域加到 `testglob.c`（名稱必須與 Ghidra 一致；function pointer table 初始化指向 noop）。
4. 要被多個測試檔共用的輔助函式或緩衝區，放到 `tests/include/<domain>fix.h`。
5. 若新建了測試檔，跑一次 `python tests/genbuild.py --apply`，`build.bat`、`test.lnk`、`testmain.c` 都會自動更新。
6. 跑 `python tools/emit/build_test.py` 過 build gate。

## 規範

- 檔名與標頭都要符合 DOS 8.3（Watcom 9.5a 無 LFN）。子檔切分用 `<stem>` 前 7 字元加序號；共用標頭用 domain 前 5 字元加 `fix`（如 `battlfix.h`）。
- 每個測試檔只含 `static` 的 `test_*` 函式、它們用到的輔助碼、以及一個匯出的 `run_*_tests()`。
- 受測函式的外部依賴（假全域、stub）集中在 `testglob.c`；只在單一測試檔用到的輔助碼就放該檔，跨檔共用的才進 `tests/include/<domain>fix.h`。
- 落點查詢與 build 接線用 `tests/where.py`、`tests/genbuild.py`。
