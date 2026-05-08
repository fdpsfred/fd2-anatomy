# rebuild_info/

支撐「把 FD2.LE 重建成可在 DOS 環境下重新編譯出等價執行檔」這個目標
的研究紀錄。`program_info/` 著重在「FD2 現在做什麼」，本資料夾著重
在「重建這個 binary 需要哪些 toolchain / lib / 連結環境細節」。

## 內容

- `crt_fid_match.md` — Ghidra Function ID 機制把 FD2.LE 內 Watcom CRT
  函式準確識別出來。確定編譯器是 **Watcom 9.5a**、連結的是 CLIB3S +
  EMU387 + GRAPH。內含完整 pipeline 說明、Watcom Easy OMF-386
  quirky record patcher 細節、版本判定證據、CRT 識別結果樣本、lookup
  table 產生流程
- `crt_matches_9.5a.json` — FidQuery 對 9.5a fidb 的 raw 輸出
  (131 個 FD2 function 被識別，含完整 candidate / score / source obj)
- `crt_lookup_9.5a.json` — 經 Phase B/D 驗證後的 address ↔ Watcom CRT
  symbol 對照表（128 entries：110 auto_threshold + 3 conflict_resolved
  + 15 manual）。`by_address` / `by_name` 雙向索引，`current_name` 欄保留
  Ghidra 內現有名以利 audit
- `crt_verify_report.md` — Phase B + Phase D 全部 31 個受驗 entry 的逐筆
  紀錄（assembly + decomp + callees + 規則套用結果），28 PASS / 3 REJECT
- `crt_verify_rejected.md` — 3 個未通過的 candidate 與拒絕原因（fgetchar
  / __EINVAL / fcloseall — 全部是 hash 巧合或 set-errno helper 名稱
  family 內錯標）

對照用 4 個版本 fidb (9.5a 正本 + 9.5/9.5b/9.5c 對照組) 放在
`tools/crt_fid_match/crt_fidb/` (跟產生它們的 pipeline 同位置)。

## 後續預定主題

當對應分析完成後會在這個資料夾擴增：

- AIL audio library 重建 (Miles Audio Interface Library — 已知
  FD2 的音樂/音效系統用 AIL，例如 `AIL_sequence_volume @ 0x3b096`)
- 連結環境 (wlink linker 設定、LE format DOS/4GW extender、
  segment ordering、DGROUP layout)
- source 拆分策略 (從 1 個 LE 倒推回 .c 檔結構)
