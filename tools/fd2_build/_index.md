# tools/fd2_build/ -- src-only FD2.EXE 正式建置

把 `src/` 自己（加 vendor lib、不含 `tests/`）建置成 `FD2.EXE`。完全與 test 建置分開、**零 `tests/`
依賴**。`build_fd2.py` 自編全部 src + link，產出實機 `FD2.EXE`，並同時報告 undefined symbol（若有，
就是「fd2.exe 還缺哪些在 src/」的權威 worklist，以 linker 符號引用為準）。

## 元件

| 檔案 | 用途 |
|---|---|
| `build_fd2.py` | **正式 FD2.EXE 建置（零 `tests/` 依賴，完全自包含）**：自掃 `src/*.c` 編譯（`life/main.c` **不帶** `-Dmain`，進入點保持 `main`）+ link，輸出 `workspace/fd2_build/exe/out/FD2.EXE`。CF、mount、obj 命名規則（leaf stem 去底線取 8；唯一 curated 例外 `life/main.c -> lifemain`）、`fd2.lnk` 格式全部定義在自身，**不讀 `tests/` 任何檔**。CF 比 test build 少 `-i=E:\include`（那是 test fixture override 目錄，正式 build 不該見）。不編/不跑 test、不碰 `tests/OUT`。確定性建置（同設定 byte-identical）。退出碼 0 = EXE 產出且 0 undefined。 |
| `analyze_undefined.py` | 把 `build_fd2.py` 輸出（`workspace/fd2_build/exe/out/build.out`）的 undefined 分類（vendor libc/math、`data_fd2_*`、`fd2_*`、`crt_*/AIL_*`、plain-named）並帶「誰引用」脈絡（home-file 提示）；對帳 `workspace/data_emit/worklist.tsv`，輸出 `workspace/fd2_build/game_worklist.tsv`。0 undefined 時為空。 |

## 跑法

```bash
python tools/fd2_build/build_fd2.py            # 自編 src + link -> workspace/fd2_build/exe/out/FD2.EXE
python tools/fd2_build/analyze_undefined.py    # （可選）有 undefined 時分類成 worklist
```

`build_fd2.py` 一步到位、完全獨立於 test：不經 `build_test.py`、不碰 `tests/OUT`、不讀 `tests/` 任何檔。

## 連結設定（`build_fd2.py` 自產的 fd2.lnk）

- **Vendor CRT wiring**：`system dos4g` 不會在「遊戲只用 `crt_*` wrapper、不直接呼叫 libc」時自動
  pull CLIB3S（`.obj` 的 default-library 記錄沒 libpath 不解析）；且 Miles AIL lib 引用真 libc
  （`strcpy`/`memset`/`sprintf`/`malloc`…）+ math387s/emu387 的 `__8087`/`__hook387`。所以 `fd2.lnk`
  顯式列 `CLIB3S`/`MATH387S`/`EMU387`（全路徑指向掛載的 Watcom 樹 `D:`），連結達 0 undefined（實測，
  見 `rebuild_info/link/wlink_settings.md`）。Layer-2：不下 FAR_DATA / object layout directive，讓
  linker 自由擺放。
- **AIL lib / header 落點**：`ailv3.lib` + `ailv3.h` 置於 `libs/ailv3/`，DOSBox 掛成 `F:`。Link 直接
  `library F:\ailv3\ailv3.lib`；compile 以 `-i=F:\ailv3` 找 `ailv3.h`（`src/include/` 不放 `ailv3.h`）。
- **`fd2common.lib` 不參與連結**：它的 8 個 `fd2_dpmi_*` / `crt_equivalent_get_eflags` 已由
  `src/util/dpmi.c` + `src/crt/crt.c` 以裸名 PUBDEF 定義，`file` obj 排在 `library` 之前先滿足
  ailv3.lib 的引用，library 不被 pull（實測：不連 `fd2common.lib` 時 FD2.EXE 仍 0 undefined）。
  `fd2common.lib` 只供「獨立使用 ailv3.lib、自身無 dpmi/crt 定義」的 client（AIL 測試 `tau.c`、
  `snd_kbd_diag`）。
