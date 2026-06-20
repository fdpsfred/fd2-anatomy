# tests/play — FD2 playthrough 整合測試(guest 端 + scenario + golden)

決定論 replay/capture 系統的 guest 端程式碼與測試資料。只編進 replay build
(FD2RP.EXE，`-DFD2_REPLAY`)，**永不進生產 FD2.EXE**(所有 src/ hook 都在
`#ifdef FD2_REPLAY` 內;已 hash 驗證生產版 byte-identical)。

## Guest 模組(8.3 檔名、ASCII-only)

| 檔案 | 用途 |
| ---- | ---- |
| `playharn.h` | harness 共用宣告(`fd2_play_capture` / `fd2_play_heartbeat`)。 |
| `replay.c` | 讀 `SCRIPT.TXT`、把 scancode 注入 BIOS 鍵盤環驅動既有輸入路徑、處理 `CAP`/`SEED`/`END`、寫 `DONE.TXT`。idle 迴圈因鍵已備妥而跑 0 圈 → 決定論。 |
| `capture.c` | 在檢查點 dump `FBnn.BIN`(0xA0000 framebuffer 64000B)+ `PALnn.BIN`(DAC 256 色,供 fb2png 還原)+ `STnn.BIN`(16 個 int32 關鍵全域 + `party_count × 0x50` runtime_char)。 |

src/ 端 gated hook:`life/main.c`(init + warmup pin)、`input/input.c`
(buffer-check pump)、`anim/aniend.c` 與 `save/save.c`(兩處非輪詢讀取前的 pump)。

## SCRIPT.TXT 命令

`SEED <hex16>` / `KEY <hexSc> [hexAsc]` / `CAP` / `END`(EOF 等同 END;`#` 為註解)。

## 目錄

| 路徑 | 內容 |
| ---- | ---- |
| `scenarios/*.json` | scenario 定義(`script` 行清單 + 選擇性 `sav` 起始存檔)。 |
| `golden/<scenario>/` | 凍結的 `FBnn/STnn` golden。 |

## Scenario

| 名稱 | golden | 用途 |
| ---- | ------ | ---- |
| `boot` | 有 | 開機到標題畫面(首個輸入等待)擷取一次。 |
| `ch01_intro` | 有 | NEW GAME -> 第一章 prologue(map 0x20,party 21):送 4 個 Enter(`1C 0D`)穿過開場+選單 START,擷取 prologue 開場 3 拍。 |
| `bootdown` | 無 | 鑑別力探針:按下後再擷取,畫面應與 `boot` golden 不同(驗證比對器會變紅)。 |

`run_all.py` 只跑「有 golden」者。新增章節 scenario 的範式即 `ch01_intro`:用 `KEY` 行驅動、`CAP` 在穩定輸入等待點擷取,`fb2png.py` 看畫面確認後 `compare.py --bless` 凍結。

## 進度(各 phase 機制驗證狀態)

引擎與各核心機制已驗證並 commit;其餘為依既有範式擴充的內容編寫工作。

| Phase 機制 | 狀態 | 證據 |
| ---------- | ---- | ---- |
| P0 引擎(注入/擷取/比對/決定論/生產純度) | ✅ 驗證 | boot;雙跑 byte-identical;鑑別力;FD2.EXE byte-identical |
| P1 NEW GAME -> 章節 | ✅ 驗證 | `ch01_intro`(ch1 prologue,blessed) |
| P2 任意章節 init/render(save-jump) | ✅ 機制驗證 | `ch05_jump`;sweep 28/30 章 headless 乾淨(ch7/ch22 見 open_issues) |
| P3 戰鬥操作(游標/選取/動作選單) | ✅ 機制驗證 | `combat_move`(游標座標斷言,blessed) |
| P4 存檔載入(CONTINUE) | ✅ 驗證 | `continue_load`(blessed) |
| P3-E AI/傷害狀態斷言 | ⬜ 待做 | 需在戰場驅動攻擊 + host LFSR 預算期望值(expect.py) |
| P4 招募/兌換/結局/商店/options | ⬜ 待做 | 需到達對應章節點(母本鏈或 jump)+ 狀態斷言 |
| P5 cinematic golden + 原版差分背書 | ⬜ 待做 | dialog/FIGANI/spell/ending golden;diff_original.py |
| P6 舊 spy 測試退役 + 文件 | ⬜ 待做 | 見計畫第三部分判準 |

**待續重點**:(1) 建母本鏈(從 ch1 連續通關逐章擷取一致 entry SAV),解 open_issues 的 ch7/ch22 並當跨章期望值母本;(2) `expect.py` 用 KB 公式 + LFSR 預算戰鬥傷害定值;(3) 逐章/逐動作 scenario 依 `ch01_intro`/`combat_move` 範式擴充並 bless。
