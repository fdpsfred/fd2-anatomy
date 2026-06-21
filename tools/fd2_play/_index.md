# tools/fd2_play — FD2 playthrough 整合測試工具

決定論 replay/capture 測試系統的 host 端工具。驅動重建版遊戲(FD2RP.EXE)在
DOSBox-X silent 跑腳本、在邏輯檢查點擷取輸出、對 golden / 期望值比對。

## Scripts

| Script | 用途 |
| ------ | ---- |
| `build_replay.py` | 以 `build_fd2.py` 為範本，加 `-DFD2_REPLAY` 並把 `tests/play/*.c` 一起編譯連結，產出 `workspace/fd2_play/exe/out/FD2RP.EXE`。ABI flag 與生產版一致，輸出隔離。 |
| `run_play.py` | 跑一個 scenario:把遊戲環境 + `SCRIPT.TXT` + 起始 `FD2.SAV` + `FD2RP.EXE` staging 進 `workspace/fd2_play/run/<out>/`，launch DOSBox-X，以三訊號(DONE.TXT / proc exit / HB.TXT 停滯)判完成，掃 protected-mode fault，收集 `FBnn/STnn` dump，寫 `run.json`。`--out` 可指定 run 目錄名(雙跑做 determinism 用)。 |
| `compare.py` | 比對 dump:framebuffer 走 Hamming distance、state 走 byte-equality。預設比 `tests/play/golden/<scenario>`;`--against DIR` 比另一 run 目錄(determinism);`--bless` 把 run dump 複製成 golden。 |
| `fb2png.py` | 把 `FBnn.BIN`(調色盤索引)+ `PALnn.BIN`(DAC 256 色)還原成 `PNGnn.png`(自帶 zlib PNG 編碼器,不依賴 PIL)。**編寫/除錯腳本時用來實際看畫面**。 |
| `run_all.py` | regression 套件入口:跑所有「有 golden」的 scenario 並逐一比對,印 per-scenario verdict + 總結。`--build` 先重建、`--only <substr>` 過濾。取代舊 per-function 套件做整合覆蓋。 |
| `gen_scenario.py` | 移植遊戲存檔 codec(crypt XOR involution + checksum,save.c)。`--validate` round-trip 內建存檔自我驗證;`--chapter N` 合成跳章存檔。scenario 用 `"jump_chapter": N` 即由 run_play 即時合成(免版控 sav)。 |
| `sweep_chapters.py` | P2 章節 init smoke sweep:0..29 章逐一 jump+CONTINUE+擷取,驗證每章 loader headless 不 fault + chapter_id 正確。 |
| `expect.py` | 戰鬥傷害 oracle:讀 scenario 的 `oracle` 區塊(attacker/defender idx、before/after 擷取點),從 before 擷取取攻防 runtime_char + seed,port `fd2_calculate_combat_hit_outcome`(雙擊判定 + 命中/爆擊/jitter)用同一 LFSR 預算傷害,斷言目標 hp_current 減少量 == 預算值。weapon class / crit rate / terrain modifier 表即時從原 binary `fd2_game_files/FD2.LE`(自解 LE vaddr->檔案偏移)讀,不硬編。`--golden` 比 golden 擷取。 |
| `st_dump.py` | 把 `STnn.BIN` 解成可讀欄位:16 個 int32 header(chapter/turn/gold/cursor/rng_seed/AI score)+ 每單位一列(pos/team/portrait/job/lvl/HP/MP/AP/DP/hit/evade)。`--raw IDX` 印某單位 0x50 byte hex dump。編寫戰鬥腳本必用。 |

## 使用

```
python tools/fd2_play/build_replay.py                          # 改 src/harness 後重建
python tools/fd2_play/run_play.py --scenario boot              # 跑單一 scenario
python tools/fd2_play/fb2png.py   --scenario boot              # 還原 PNG 來看畫面
python tools/fd2_play/st_dump.py  workspace/fd2_play/run/boot/ST00.BIN   # 看狀態欄位
python tools/fd2_play/compare.py  --scenario boot --bless      # 首次凍結 golden
python tools/fd2_play/expect.py   --scenario combat_attack     # 戰鬥傷害 oracle 斷言
python tools/fd2_play/run_all.py                               # 跑整個 regression 套件
```

`run_all.py` 對帶 `oracle` 區塊的 scenario(如 `combat_attack`)會在 golden 比對外**再跑 `expect.py`**:golden 保證擷取 byte 穩定,oracle 保證擷取的數值符合遊戲公式。兩者皆過才算該 scenario PASS。

scenario 定義與 guest harness 在 `tests/play/`。中間/輸出產物在 `workspace/fd2_play/`(scratch)。

## 決定論關鍵(實作要點)

- **虛擬時鐘**:replay 下 `BIOS_TICK_*`(consts.h,gated)改成每讀遞增的計數器,讓
  palette/blink/typewriter 等 tick 推導的動畫相位變決定論(否則 framebuffer 隨 wall-clock 飄)。
- **選單/對話確認鍵看 ASCII**:送 `KEY 1C 0D`(Enter)或 `KEY 39 20`(Space);只送 scancode 無效。
- **每次 run pristine 起始**:`run_play.py` 清掉遊戲生成的 FD2.TMP/AUDDBG.TXT 並一律覆寫 FD2.SAV,
  否則重用 run 目錄會被上一輪殘留污染。
