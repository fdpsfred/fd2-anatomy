# tests/ — FD2 Unit Test 架構

## 執行方式

```
dosbox-x -silent -conf tests/dosbox.conf
```

DOSBox-X 會自動：
1. 編譯 `src/` 下所有 .c → `tests/OUT/*.obj`
2. 編譯 `tests/` 下所有 test*.c → `tests/OUT/*.obj`
3. Link 成 `tests/OUT/TEST.EXE`（依 `tests/test.lnk`）
4. 執行 TEST.EXE，結果寫到 `tests/OUT/test.out`

確認結果：讀取 `tests/OUT/test.out`，末行應為 `Results: N passed, 0 failed`。

## 檔案結構

| 檔案 | 用途 |
|---|---|
| `dosbox.conf` | DOSBox-X 自動化 conf：compile + link + run |
| `test.lnk` | wlink directive，列出所有 .obj（src + test） |
| `include/testharn.h` | Test harness macro：ASSERT_EQ / ASSERT_TRUE / RUN_TEST 等 |
| `testmain.c` | 唯一 `main()`，定義 harness globals，依序呼叫各 `run_*_tests()` |
| `testglob.c` | 所有 fake globals 和 stub functions 集中定義 |
| `testtbl.c` | table accessor test cases |
| `testbtl.c` | battle core test cases (RNG / damage / heal / counter) |
| `testui.c` | cursor + pan + input test cases |
| `testspel.c` | spell handler test cases |

## 新增 function 時的 test 修改步驟

1. **src/ 新增 .c 檔時**：在 `dosbox.conf` 的 `compile src` 區塊加 WCC386 行，在 `test.lnk` 加 `file C:\OUT\xxx.obj`
2. **新 function 需要外部 stub**：在 `testglob.c` 加 stub function 定義
3. **新 function 使用新 global**：在 `testglob.c` 加 fake global 定義（名稱必須與 Ghidra 一致）
4. **寫 test case**：在對應的 `test*.c` 內加 `static void test_xxx(void)` + 在 `run_*_tests()` 內加 `RUN_TEST(test_xxx);`
5. **新增 test suite**：新建 `testXXX.c`（≤ 8.3），export `run_XXX_tests()`，在 `testmain.c` 加 `extern + 呼叫`，在 `dosbox.conf` 加 compile 行，在 `test.lnk` 加 obj

## 規範

- test 檔名 ≤ 8.3 DOS 格式（Watcom 9.5a 限制）
- 各 test*.c 只含 `static` test functions + 一個 `run_*_tests()` export
- 不在 test*.c 定義任何 global 或 stub — 全部放 `testglob.c`
- fake global 名稱必須與 Ghidra / globals.h 完全一致
- `extern runtime_char g_test_rc_array[8]` 用於測試需要操作 runtime_char 的場景
