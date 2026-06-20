# tools/fd2_play — FD2 playthrough 整合測試工具

決定論 replay/capture 測試系統的 host 端工具。驅動重建版遊戲(FD2RP.EXE)在
DOSBox-X silent 跑腳本、在邏輯檢查點擷取輸出、對 golden / 期望值比對。

## Scripts

| Script | 用途 |
| ------ | ---- |
| `build_replay.py` | 以 `build_fd2.py` 為範本，加 `-DFD2_REPLAY` 並把 `tests/play/*.c` 一起編譯連結，產出 `workspace/fd2_play/exe/out/FD2RP.EXE`。ABI flag 與生產版一致，輸出隔離。 |
| `run_play.py` | 跑一個 scenario:把遊戲環境 + `SCRIPT.TXT` + 起始 `FD2.SAV` + `FD2RP.EXE` staging 進 `workspace/fd2_play/run/<out>/`，launch DOSBox-X，以三訊號(DONE.TXT / proc exit / HB.TXT 停滯)判完成，掃 protected-mode fault，收集 `FBnn/STnn` dump，寫 `run.json`。`--out` 可指定 run 目錄名(雙跑做 determinism 用)。 |
| `compare.py` | 比對 dump:framebuffer 走 Hamming distance、state 走 byte-equality。預設比 `tests/play/golden/<scenario>`;`--against DIR` 比另一 run 目錄(determinism);`--bless` 把 run dump 複製成 golden。 |

## 使用

```
python tools/fd2_play/build_replay.py
python tools/fd2_play/run_play.py --scenario boot
python tools/fd2_play/compare.py  --scenario boot --bless     # 首次凍結 golden
python tools/fd2_play/compare.py  --scenario boot             # regression 比對
```

scenario 定義與 guest harness 在 `tests/play/`。中間/輸出產物在 `workspace/fd2_play/`(scratch)。
