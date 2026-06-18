# tools/fd2_build/ -- src-only FD2.EXE 連結（神諭 + 最終建置）

把 `src/` 自己（加 vendor lib、不含 `tests/`）連結成 `FD2.EXE`。在資料/函式尚未補齊的當下，
這份連結會 link 不過，而它回報的 **undefined symbol 就是「fd2.exe 還缺哪些東西在 src/」的權威
worklist**（比 name-pattern grep 可靠，因為以 linker 的符號引用為準）。補齊後同一套工具即 Phase 4
的最終建置。

## 元件

| 檔案 | 用途 |
|---|---|
| `mklnk.py` | 產 `tests/fd2.lnk`：`system dos4g` + `name FD2.EXE` + 全 51 個 src `.obj`（重用 `tests/genbuild.src_compile_list()`，含 `main` 的 `lifemain.obj` 擺第一）+ Miles AIL libs（`ailv3.lib` / `fd2common.lib`）+ **顯式列 Watcom CRT（`CLIB3S` / `MATH387S` / `EMU387`，全路徑）—— `system dos4g` 不自動 pull CRT**（實測，見 `rebuild_info/link/wlink_settings.md`）。Layer-2：不下 FAR_DATA / object layout directive，讓 linker 自由擺放。`--apply` 才寫檔。 |
| `link_oracle.py` | 在 `tests/dosbox.conf` 的已驗證 DOSBox 環境（mounts + `WATCOM`/`PATH`，僅把 autoexec 末行 `build.bat` 換成 link-only `fd2link.bat`）裡跑 `wlink @fd2.lnk`。前置：src `.obj` 已由 `build_test.py` 編好在 `tests/OUT/obj`。stage AIL libs 進 `E:\out`，跑完解析 undefined → `workspace/fd2_build/{fd2link.out, undefined.txt}`。 |
| `analyze_undefined.py` | 把 `fd2link.out` 的 undefined 分類（vendor libc/math、`data_fd2_*`、`fd2_*`、`crt_*/AIL_*`、plain-named）並帶「誰引用」脈絡（home-file 提示）；對帳 `workspace/data_emit/worklist.tsv`，輸出 `workspace/fd2_build/game_worklist.tsv`。 |

## 跑法

```bash
python tools/code_emit/build_test.py            # 先把 src .obj 編到 tests/OUT/obj
python tools/fd2_build/mklnk.py --apply    # 產 tests/fd2.lnk
python tools/fd2_build/link_oracle.py      # 跑 wlink，取 undefined（前景）
python tools/fd2_build/analyze_undefined.py
```

中間檔（`fd2link.out` / `undefined.txt` / `game_worklist.tsv`）寫到 `workspace/fd2_build/`（scratch）。

## 連結收尾現況

- **Vendor CRT wiring（已解）**：`system dos4g` 不會在「遊戲只用 `crt_*` wrapper、不直接呼叫 libc」時
  自動 pull CLIB3S（`.obj` 的 default-library 記錄沒 libpath 不解析）；且 Miles AIL lib 引用真 libc
  （`strcpy`/`memset`/`sprintf`/`malloc`…）+ math387s/emu387 的 `__8087`/`__hook387`。`mklnk.py` 已
  顯式列 `CLIB3S`/`MATH387S`/`EMU387`（全路徑）解掉這約 60 個 vendor undefined，連結達 0 undefined。
- **AIL lib 落點**：`ailv3.lib`/`fd2common.lib` 由 `link_oracle.py` 每次 stage 到 `E:\out`
  （`build_test.py` 會清 `tests/OUT`，故每跑前 re-stage）。
