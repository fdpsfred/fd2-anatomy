# tools/calling_convention_audit/

把 FD2.LE 的 calling convention 從 Ghidra 自動推斷錯誤狀態校正成 ABI 正確、可
重新編譯的 5-phase pipeline。每個 script self-contained、自帶完整 docstring。

ABI 規則與最終 cc 分佈見 `program_info/calling_convention.md`。

跨 session 進度由 workdir (預設 `<repo>/workspace/calling_convention_audit/`)
裡的 `progress.json` 追蹤；`audit_sha` / `recs_sha` 防止上游檔案 drift 後盲目
續跑。

## Pipeline 與 script 對應

| Phase | Script | 角色 |
|---|---|---|
| 1 | `ghidra_dump.java` | 對全 1000 function dump per-function ABI 證據 → `audit.json` |
| 2 | `classify.py` | 用確定性規則把 audit.json → `recommendations.json` (cc + rename) |
| 4 | `apply_batch.py` (orchestrator) + `ghidra_apply.java` (worker) | 跨 session 批次套用 cc/rename，progress.json checkpoint |
| 5 | `verify.py` (重跑 `ghidra_dump.java` 後 diff) | 比對 audit_after.json vs recommendations.json |
| 6 | `ghidra_param_cleanup.java` | `arg_eax_in/edx_in/ecx_in` 殘影參數名 → `param_N` |
| 7 | `param_count_classify.py` + `ghidra_param_count_apply.java` | 從 caller 訊號推論真實 param 數量並套用 |

Phase 1+2+4+5 是 cc 校正主線 (cross-session resumable)；6 與 7 是 cc 修正後的
cleanup pass，one-shot single-transaction。

## 檔案

- `ghidra_dump.java` — Phase 1 / 5 全 function dump：cc、last_insn、prologue
  reads、tail-jmp target、multi-caller pre/post-CALL aggregate (ADD ESP /
  reg-set)、existing signature。寫 `audit.json`。重跑 Phase 5 時改頂端
  `OUT_PATH` 指向 `audit_after.json`。
- `classify.py` — Phase 2：first-match 規則 cascade (RET N + reg → fastcall /
  RET N - reg → stdcall / 全 caller cleanup → cdecl / TAIL_JMP 沿用 / 預設
  cdecl)，pinned 0x36cd7=stdcall + 0x4b502=fastcall，名稱含 cc 字樣的 mismatch
  改名建議。寫 `recommendations.json`。
- `apply_batch.py` — Phase 4 orchestrator：`prepare` 切 50/100 一批寫
  `batch_input.tsv`；orchestrator 跑 ghidra_apply.java；`consume` 解析
  `batch_result.tsv` 更新 progress.json + rename_log.json + errors.json。
  跨 session 由 `last_processed_idx` 接續。
- `ghidra_apply.java` — Phase 4 worker：單 Ghidra transaction 處理一批，
  `setCustomVariableStorage(false)` + `setCallingConvention()` 讓 Ghidra
  重算 storage；`setName(USER_DEFINED)` 處理 rename。
- `verify.py` — Phase 5：diff `audit_after.json` 與 `recommendations.json`，
  確認 cc 一致、pinned 正確、rename 套用、Bad Instruction bookmark 無新增；
  全通過則 progress.phase = "done"。
- `ghidra_param_cleanup.java` — Phase 6：regex 掃 parameter 名稱
  `^arg_(e?ax|...)_in(_\d+)?$` 與 `^in_(EAX|...)(_\d+)?$`，改成
  `param_<ordinal+1>`。idempotent。寫 `param_rename_log.json`。
- `param_count_classify.py` — Phase 7：用 caller `ADD ESP K` / RET N /
  reg-set 訊號推論真實 param 數量；HIGH (callers 一致)、MEDIUM (≥半數一致)、
  LOW (跳過保留現狀)。寫 `param_count_recommendations.json` +
  `param_count_input.tsv`。
- `ghidra_param_count_apply.java` — Phase 7 worker：reduce 用
  `removeParameter` 從尾部刪、expand 用 `addParameter` 加 `unsigned int`
  名稱 `param_N`。single transaction。

## 操作 (一次完整跑完)

```bash
# Phase 1：在 Ghidra 內 run_script_inline 跑 ghidra_dump.java
# Phase 2:
python tools/calling_convention_audit/classify.py
# Phase 4 (loop until done):
python tools/calling_convention_audit/apply_batch.py prepare --batch-size 100
# (run ghidra_apply.java via run_script_inline)
python tools/calling_convention_audit/apply_batch.py consume
# (repeat prepare/consume until apply_batch.py status reports remaining=0)
# Phase 5：改 ghidra_dump.java 的 OUT_PATH 為 audit_after.json，再 run_script_inline
python tools/calling_convention_audit/verify.py
# Phase 6：在 Ghidra 內 run_script_inline 跑 ghidra_param_cleanup.java
# Phase 7:
python tools/calling_convention_audit/param_count_classify.py
# (run ghidra_param_count_apply.java via run_script_inline)
```

跨 session 中斷恢復：所有 Python script 直接重跑即可，會從 progress.json
讀取上次斷點繼續。Java script 無內部狀態，每次重跑 idempotent。

## 自訂 workdir

所有 Python script 接受 `--workdir DIR` 改寫工作目錄；Java script 在頂端
有清楚標注的 `IN_PATH/OUT_PATH/LOG_PATH` 常數可直接編輯 (workdir 改變很罕見)。
