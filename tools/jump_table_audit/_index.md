# tools/jump_table_audit/

全 binary indirect-JMP / orphan code / fragmented body audit 工具。對應
`rebuild_info/crt_fid_match.md` §12.2.1 的 Phase 1+2+3 流程。

## Pipeline

| script | 用途 |
|---|---|
| `compare_lookup_sizes.py` | Phase 2：對 `crt_lookup_9.5a.json` 全 entry 跑 lookup body_size vs lib `.obj` size 全面 diff，輸出 `workspace/jump_table_audit/lookup_size_diff.{json,md}` |

## Phase 1 (Ghidra-side detection)

Phase 1 用 Ghidra `run_script_inline` 一次性掃完，沒有獨立 .py。腳本流程：

1. 用 `Listing.getInstructions(true)` 走全 program instruction，篩 `FlowType.isJump() && isComputed() && !isConditional()`
2. 對每個 indirect JMP 取 `Instruction.getReferencesFrom()` 拿 Ghidra 已解析的
   target 集合，比對是否 `function.getBody().contains(target)`
3. 旗標 `out_of_body > 0` 的 case；同步輸出 byte hex / asm / function 名

跑完正常結果：63 個 indirect JMP，AIL 4 / CRT 59 / GAME 0，全部 target 在 body 內。

## Phase 3 (orphan / gap fix)

Phase 3 也是 Ghidra 側一次性掃 + 修，沒有獨立 .py。掃描三項：

- orphan instruction：有 disassembly 但不在任何 function 內
- abnormally small body：function body < 5 byte 但前後 byte 是真實 code
- fragmented body：`AddressSetView.getNumAddressRanges() > 1` 且 hole 在 entry instruction 跨度內

修法：
- `disassemble_bytes_mcp` 補 disassembly
- `Function.setBody(AddressSet)` 修 body（注意：Ghidra `Function.setBody` 直接呼叫，
  不是 `SetFunctionBodyCmd` —— 該 class 在當前 Ghidra 版本不存在）

## 執行方式

```bash
# Phase 2 regression（任何時候安全跑）
python tools/jump_table_audit/compare_lookup_sizes.py
# 預期 summary：match=137 mismatch=0 name_not_in_obj=3 obj_missing=0
# 3 個 name_not_in_obj 為 L$1 anonymous static (lib 端無 PUBDEF)，正常
```

## 依賴的輸入檔

- `rebuild_info/crt_lookup_9.5a.json` — 140-entry CRT lookup table
- `workspace/crt_callee_match/obj_funcs_clib3s.json` — 770 個 .obj 內 PUBDEF 與 size 索引

## 何時跑

- 新增/調整 `crt_lookup_9.5a.json` entry 後做 regression
- 對 Ghidra setBody 後做 sanity check
- AIL / 遊戲端再次大規模重 disassemble 後（雖然目前 GAME 0 indirect JMP）

## 已知 Ghidra MCP run_script_inline 坑

- `ghidra.app.cmd.function.SetFunctionBodyCmd` 在當前 Ghidra MCP 版本**不存在** —
  必須用 `Function.setBody(AddressSetView)` 直接呼叫
- `FunctionManager.getFunctionAfter(addr)` / `getFunctionBefore(addr)` 不存在 —
  用 `getFunctionAt` / `getFunctionContaining` 替代
- `run_script_inline` 偶爾出 `GhidraScriptLoadException class not found`，重試
  即可（為 OSGi bundle reload race）
