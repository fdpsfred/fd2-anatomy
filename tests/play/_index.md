# tests/play — FD2 playthrough 整合測試(guest 端 + scenario + golden)

決定論 replay/capture 系統的 guest 端程式碼與測試資料。只編進 replay build
(FD2RP.EXE，`-DFD2_REPLAY`)，**永不進生產 FD2.EXE**(所有 src/ hook 都在
`#ifdef FD2_REPLAY` 內;已 hash 驗證生產版 byte-identical)。

## Guest 模組(8.3 檔名、ASCII-only)

| 檔案 | 用途 |
| ---- | ---- |
| `playharn.h` | harness 共用宣告(`fd2_play_capture` / `fd2_play_heartbeat`)。 |
| `replay.c` | 讀 `SCRIPT.TXT`、把 scancode 注入 BIOS 鍵盤環驅動既有輸入路徑、處理 `CAP`/`SEED`/`END`、寫 `DONE.TXT`。idle 迴圈因鍵已備妥而跑 0 圈 → 決定論。 |
| `capture.c` | 在檢查點 dump `FBnn.BIN`(0xA0000 framebuffer 64000B)+ `STnn.BIN`(16 個 int32 關鍵全域 + `party_count × 0x50` runtime_char)。 |

src/ 端 gated hook:`life/main.c`(init + warmup pin)、`input/input.c`
(buffer-check pump)、`anim/aniend.c` 與 `save/save.c`(兩處非輪詢讀取前的 pump)。

## SCRIPT.TXT 命令

`SEED <hex16>` / `KEY <hexSc> [hexAsc]` / `CAP` / `END`(EOF 等同 END;`#` 為註解)。

## 目錄

| 路徑 | 內容 |
| ---- | ---- |
| `scenarios/*.json` | scenario 定義(`script` 行清單 + 選擇性 `sav` 起始存檔)。 |
| `golden/<scenario>/` | 凍結的 `FBnn/STnn` golden。 |

## Scenario(P0)

| 名稱 | 用途 |
| ---- | ---- |
| `boot` | 開機到首個輸入等待畫面(主選單)擷取一次。已 bless golden。 |
| `bootdown` | 鑑別力探針:按下後再擷取,畫面應與 `boot` golden 不同(驗證比對器會變紅)。 |
