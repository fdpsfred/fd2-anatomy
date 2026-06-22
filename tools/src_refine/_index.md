# tools/src_refine/ -- src/ per-symbol refine pipeline

對 `src/` 內每個 symbol（function + global）逐一解析用途/邏輯，refine 名稱判定與註解，並把結論
同步回 Ghidra（src/ 為準、一致即可）。**只改名稱與註解，遊戲邏輯不動**；硬性驗收＝重建的
production FD2.EXE 與 baseline byte-identical。

## 元件

| 檔案 | 用途 |
|---|---|
| `build_worklist.py` | 從 routing.json(function done&!skip&target=.c) + worklist.tsv(global real_in_src) + call_graph.json，與 live Ghidra dump 對齊，產 worklist.json + coverage_reconcile_start.md（可重生暫存；live name 為準、coverage gap 只報不自動加）。 |
| `partition.py` | 把 worklist 以 .c 檔為單位、weight-balanced 切 N(預設4) 個 file-disjoint partition manifest（可重生暫存；確定性 LPT）。 |
| `scout.py` | 某 partition 的「下一批未 refine」work-list → args（餵給 workflow）。done 判定＝該 symbol 的 shard 檔已存在；可重跑續做。 |
| `src_refine.wf.js` | **Stage 1 workflow**（一 worktree 一實例）。serial 逐檔逐 symbol，每 symbol 一個 refiner agent：分析→refine src 註解+同步 Ghidra plate→**只記錄** 符號名與**每個參數名**改名判定（不 rename）→記 logic issue→寫 per-symbol shard→per-symbol commit（含 clobber 防線）。參數名改名同 protos.h/簽章（跨檔）故與符號名一樣延 Stage 2 套用。 |
| `hash_check.py` | build gate：sha256(build 出的 FD2.EXE) 必 == `data/baseline_hash.txt`。byte-identical＝沒改到 code。 |
| `merge_shards.py` | 所有 shard → `data/src_info.json`(address 主鍵) + `data/src_info_by_name.json`(name→addr，current+final) + `data/src_issues.json`(ISS-####) + global 的 reader/writer_fns 反向關聯。 |

## 狀態 source of truth / 續跑

- **durable 全在 tracked `data/`**：`baseline_hash.txt`、`shards/rpN/<addr8>.json`（per-symbol，refiner 在 worktree 寫+commit）、merge 後的 `src_info.json` / `src_info_by_name.json` / `src_issues.json`。
- **per-symbol commit + shard 是斷點**：任何中斷後重跑零成本續做；`scout.py` 永遠回「下一批未做」。
- 可重生暫存（不追蹤）：Ghidra dump、worklist.json、coverage 報告、partition manifest、每批 args；分別由 `run_script_inline` / `build_worklist.py` / `partition.py` / `scout.py` 重產。

## 流程

1. **dump live Ghidra**（`run_script_inline` enumerate functions + named primary symbols 寫成 TSV）。
2. `python tools/src_refine/build_worklist.py` → worklist.json + reconcile 報告（檢視 drift / coverage gap）。
3. `python tools/src_refine/partition.py` → partition manifests rp1..rpN。
4. **commit tooling 到 main**，再建 `fd2-wt/rpN`（branch `refine-pN`）worktree（自 main，繼承 tooling/baseline/partitions）。
5. 每個 partition、每批：`scout.py --partition rpN --root <worktree> --shards-dir <worktree>/tools/src_refine/data/shards/rpN --limit 50` → `Workflow({scriptPath:"tools/src_refine/src_refine.wf.js", args:<scout 輸出>})` → 前景 `build_fd2.py` + `hash_check.py` gate（須 ==baseline）→ 下一批。每 50 symbol hard-stop。
6. partition 全完成且 build gate 過 → merge `refine-pN` 回 main。
7. 四 partition 都 merge 後：`merge_shards.py` 彙整 → src_info / src_issues。
8. **Stage 2（序列，在 main，手動 per-item）**：套用所有 rename（兩類，皆 byte-identical）：
   - **符號名** `name_verdict==rename`：個別 `get_xrefs_to` 確認 call site → 改 def+protos.h/globals.h+所有 call site → Ghidra `rename_function`/`rename_global_variable` → 同步 routing.json/worklist lookup。
   - **參數名** `params[].verdict==rename`：改該 function 的 def 簽章 + body 內該參數所有使用處 + protos.h 該 prototype 的參數名 → Ghidra `set_function_prototype`（參數名同步）。
   per-rename commit → 每 50 build+hash（須 ==baseline）。src_info 的 `name_final` / `params[].proposed` 是清單來源。
9. 最終：`build_fd2.py`+`hash_check.py` 必 ==baseline；收尾 live Ghidra reconcile 驗 name.final==live；逐筆處理 `src_issues.json`。

## 硬性注意

- workflow 的 refiner **嚴禁 Write 整檔**（用 Edit 改自己 symbol 的註解區塊）；**嚴禁在 Stage 1 rename**（只記 proposed）。
- 名稱位址殘留 vs 領域 ID 靠**語意人工判定**（領域 ID 範圍見遊戲 KB / fd2-knowledge skill），**嚴禁 regex 機械剝除**。
- emitter/build 一律前景跑（嚴禁背景），否則 subagent 不閉環。
- 並發呼叫同一 Ghidra instance OK；Ghidra 斷線靠 refiner 設 `ghidra_unreachable` → workflow fast-stop。
- **撞 usage limit 自動續跑**：agent() 撞 limit 回傳 null（workflow JS 拿不到 "resets HH:MM" 訊息、也無 sleep/Date.now），故 workflow 連續 3 個 null 就 **fast-stop** 回報 `stopped=usage_limit_suspected`（不空轉幾百個 null）；**sleep+resume 在 orchestrator 層**：從完成通知的 `<failures>` 讀 "resets H:MMam" → `python tools/src_refine/limit_wait.py "<msg>"` 得等待秒數 → 背景 `sleep <秒>`（完成會通知）→ 醒來重啟尚未完成的 partition（scout 跳過已 commit shard）。循環直到 4 partition 全 complete。
