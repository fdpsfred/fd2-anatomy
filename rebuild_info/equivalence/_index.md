# rebuild_info/equivalence/

`src/` 是已可用 Watcom C/C++ 9.5a 編譯、通過遊玩測試的重建成品，是本專案的 ground
truth。本資料夾收錄維護 `src/` 時要守住的等價鐵則，以及從 Ghidra 把 FD2.LE 對映回
C source 所需的分類與 ABI 參考。等價目標是 Layer 2（functionally-exact），不追求
Layer 3（byte-exact）。

## 檔案

- `rules.md` — 等價鐵則與結構分類參考：三層等價 invariant（Layer 1/2/3）、時序敏感
  熱迴圈例外、math intrinsic 例外、fall-through 的 6 種模式（A..F）、絕對位址引用改
  symbol、BSS tentative scalar-as-array、Watcom string-pool dedup、手寫組語政策。
- `pool_classification.md` — 四 pool（ail / crt / fd2 / binary_artifact）的命名分類法，
  以及 binary_artifact pool 的 compiler-emit NOP 與 wlink alignment fill 事實。
- `watcom_abi.md` — Watcom 32-bit ABI 結論：三種 calling convention 差異、`__CHK`
  stack-probe 機制、disasm signal-based 的 cc 判斷規則、function-pointer dispatch
  callee 的 signature 推導、shared epilogue / out-of-line tail fragment 完整清單。

## 全程式 call graph

四 pool 分類與 emit_action 的 source of truth 是
`tools/program_analysis/build_call_graph.py` 的 `categorise()`（四 pool 分類的 source of truth；重生步驟見 `tools/program_analysis/_index.md`）。
call graph 不在本資料夾常駐，要用時即時重生 —— 重生步驟與輸出位置見
`tools/program_analysis/_index.md`。
