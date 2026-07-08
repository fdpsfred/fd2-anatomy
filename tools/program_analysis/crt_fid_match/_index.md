# tools/program_analysis/crt_fid_match/

Ghidra Function ID 比對 pipeline，識別 FD2.LE 內的 Watcom CRT 函式。
分兩段：(1) 建跨版本 `.fidb`（一次性），(2) 對 raw FidQuery 結果做行為
驗證，產出可信的 address ↔ Watcom CRT symbol 對照表。

詳細結果見 `rebuild_info/crt/fid_match.md`。

## Fidb 建構 pipeline

| script | 用途 |
|---|---|
| `omf_lib_extract.py` | `wlib -q -x` wrapper，把每個 Watcom 版本 × lib 拆成 .obj 進 `workspace/crt_fid_match/extracted/` |
| `omf_patch_segdef.py` | 修 Watcom Easy OMF-386 quirky record |
| `build_manifest.py` | 建跨版本 dedup 索引 → `manifest.json` |
| `build_dedup_dir.py` | 把 dedup 後的 .obj 集中到一個 flat dir → `dedup/` |
| `compare_results.py` | 從 `results/` 的多版本 query JSON 生 markdown 比對報告 |
| `ghidra_scripts/FidWipeFolder.java` | 清 Ghidra project folder |
| `ghidra_scripts/FidImportBatch.java` | batch import .obj，setLanguage `x86:LE:32:watcom` |
| `ghidra_scripts/FidAnalyzeAll.java` | 對 import 的 program 跑 auto-analysis |
| `ghidra_scripts/FidPopulate.java` | 從 program 集合建 `.fidb` → `fidb/` |
| `ghidra_scripts/FidQuery.java` | 對 target program 跑 query → `results/matches_*.json` |

## Lookup 生成 pipeline

| script | 用途 |
|---|---|
| `build_crt_lookup.py` | 讀 `workspace/crt_fid_match/results/matches_9.5a.json`（FidQuery 輸出，pipeline intermediate，不入 git），按 `AUTO_THRESHOLD` 分流出 `auto_candidates` / `sample` / `conflict` / `manual` 四個 verify queue |
| `build_final_lookup.py` | 整合 `auto_candidates.json` + manual observations + 衝突解決 → `lookup_9.5a.json`（rejected entry 拒絕原因 inline 到 `notes`） |
| `find_crt_to_nongame_calls.py` | 對 lookup CRT function audit 它們呼叫的 non-CRT callee（catch FidDb misses + misclassifications） |

每個 lookup entry 標 `verified` ∈ `{auto_threshold, conflict_resolved, manual, byte_match, byte_match_disputed}`。
observation 中加 `manual_verdict: "REJECT" + manual_reason` 可顯式覆寫 PASS
結果。

## Generated view

| script | 用途 |
|---|---|
| `gen_matched_sources.py` | 讀 `rebuild_info/crt/lookup_9.5a.json` 的 `by_address`，生成 `rebuild_info/crt/matched_function_sources.md`（逐列 address ↔ lib symbol 表 + 依 source lib/版本的彙總表）。表格內容全由 JSON 機械產生，勿手改。 |

```bash
# 重生 matched_function_sources.md（lookup_9.5a.json 更新後要跑）
python tools/program_analysis/crt_fid_match/gen_matched_sources.py

# regression：確認現檔與重生結果一致（不寫檔）
python tools/program_analysis/crt_fid_match/gen_matched_sources.py --check
```

逐列表的 `lib symbol` 欄用 entry 的凍結 `name`（disputed entry 的現用名記在
`current_name`）；`source lib(s) / versions` 欄直接取 `source_summary`；彙總每個
function 依其 `source_libs` 內每個唯一 (lib, version) 計一次。正典計數以
`by_address` 條目數為準。

## 典型 session

```bash
# 0. （需要重跑時）透過 Ghidra MCP 跑 FidQuery 對 FD2.LE 命中
#    Watcom 9.5a fidb，輸出寫到 workspace/crt_fid_match/results/matches_9.5a.json
#    （raw 輸出屬 pipeline intermediate，不入 git）

# 1. 分流
python tools/program_analysis/crt_fid_match/build_crt_lookup.py

# 2. Claude Code 透過 Ghidra MCP 收集 disasm + callees + decomp，寫入
#    tools/program_analysis/crt_fid_match/data/observations_*.json
#    （每 entry 含 callees / asm / key_instructions / notes；
#    REJECT 案另加 manual_verdict + manual_reason）

# 3. 出最終 lookup
python tools/program_analysis/crt_fid_match/build_final_lookup.py
```

Verify queue 的 PASS/REJECT 判斷由 reviewer 親自讀 disasm + decomp 後
寫進 observation 的 `manual_verdict` 欄位，再由 `build_final_lookup.py`
依此決定 inclusion / rejection。
