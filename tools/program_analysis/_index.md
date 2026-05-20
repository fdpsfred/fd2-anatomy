# tools/program_analysis/

FD2.LE 結構性分析工具集合 — 5 個 audit pipeline + 1 個 call graph build
script。所有 script 只做機械處理（dump / classify / 套用 verdict /
categorise）；命名與分類決策一律由人親自讀 disasm / decomp / bytes 後判定。

## Audit pipeline (subfolders)

| Subfolder | 用途 |
|---|---|
| `crt_fid_match/` | Ghidra Function ID 比對識別 FD2.LE 內 Watcom CRT 函式；產出 `lookup_9.5a.json` |
| `crt_callee_match/` | 對 lookup 內 CRT function 間呼叫到的未識別 callee 做 byte-level FIXUPP-aware 比對，補抓 splitter false positive 與 small helper |
| `function_audit/` | 全 function 結構性 audit (9-group G1→G9)：body 邊界 / cc / shared prologue / jump-into-middle / plate / name 六類問題 |
| `data_audit/` | 全 data item 結構性 audit (12-group D1→D12)：name / type / boundary / caller association / plate 五類問題 |
| `jump_table_audit/` | indirect-JMP / orphan code / fragmented body / lookup-vs-lib size diff regression |

## Top-level script

| Script | 用途 |
|---|---|
| `build_call_graph.py` | 從 Ghidra MCP raw dump 建 4-pool 分類 + emit_action，輸出 `workspace/call_graph/{json,dot,md}`。`categorise()` / `emit_action_for()` 是 4-pool 分類的 source of truth |

## 共用設計慣例

- Source of truth = Ghidra MCP live dump，不從 stale registry 推
- Audit / rename 一個 message 處理一個 item
- Rename / 修改後立即同步 plate comment + KB doc + scripts
- 長 workflow 每組結束等使用者確認再繼續
