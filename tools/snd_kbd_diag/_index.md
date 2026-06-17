# tools/snd_kbd_diag/ — 實機 playtest debug 的診斷工具

把 src-only 連出的 FD2.EXE 放進完整遊戲環境跑、對照原版發現的問題（鍵盤、音效、開場
scene）的診斷腳本。完整脈絡與待解問題見 `src/handoff.md` 開頭的「實機 playtest debug」段。
中間檔一律寫 `workspace/snd_kbd_diag/`。

| 檔案 | 用途 |
|---|---|
| `genmap.py` | 重連 FD2.EXE 並產 wlink map（在 `fd2.lnk` 後加 `option map`），看 BSS symbol 實際擺放。重編 lifemain（不帶 `-Dmain`）→ link → map 複製到 `workspace/snd_kbd_diag/fd2.map`。union REGS 修復的驗證工具（確認 scratch 獨佔 28 bytes、相鄰全域擺放）。 |
| `lib_probe.py` | 在 DOSBox 用 wlib/wdisasm dump fd2common.lib / ailv3.lib（DOS 版工具在 `D:\BINB`）。**更快的做法是直接用 host 版 `WATCOM_9.5a\BINNT\WLIB.EXE` / `WDISASM.EXE`**（免 DOSBox；路徑含 `-` 會被當 option，先 `cd` 進目錄用相對檔名）。 |
| `sfxdiag.c` + `run_sfxdiag.py` | 用 FD2 編譯參數（`-bt=dos4g -3s -ms -zp4`）+ 同一套 ailv3.lib / fd2common.lib 重現 FD2 的 AIL 初始化（`install_DIG_INI`），印 driver handle / error / `alloc_fnptr`、播一個 FDOTHER[0x1F] SFX 並輪詢 `AIL_sample_status`。`run_sfxdiag.py` 複用 ail_extract 的 SB16/BLASTER 環境（`irq=5` + `BLASTER ... I5`）；想用 `mixer wavstart` 錄 WAV 但該命令在現行 DOSBox-X 無效（待換正確錄音機制）。 |
| `run_fd2_audbg.py` | 把（臨時 instrument 過的）FD2.EXE 放遊戲目錄 headless 跑，讀 main 一次性寫出的 `AUDDBG.TXT`（dig handle / sfx flag / sample handles / sfx bank / enabled），跑完還原原 FD2.EXE。用來取 runtime audio 狀態值（屬「看數值」診斷）。 |

## 已得結論（截至本 session）

- 鍵盤失效 + sfx_driver_flag 被清零 = union REGS scratch 被拆散 → 已修（`globals.h`/`main.c`/`input.c`）。
- "File not found" 開場退出 = 9 處 `fd2_load_dat_resource` 用 hardcoded 字串位址 → 已修（改 symbol）。
- 音效無聲：FD2.EXE runtime 的 SFX 三道 gate 全過、driver/handle/bank 全正確（`run_fd2_audbg.py` 證），
  根因縮到 **SB DMA/IRQ 實際送聲層**（最可能 `install_DIG_INI` 的 IRQ autodetect 路徑）。下一步要錄 WAV
  對照 `install_DIG_INI` vs `install_DIG_driver_file` 是否真送出聲波 → 卡在 DOSBox-X 錄音命令。
