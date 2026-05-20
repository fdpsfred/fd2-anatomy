# tools/program_analysis/jump_table_audit/

全 binary indirect-JMP / orphan code / fragmented body audit 工具。
三段流程：

- **Phase 1**：Ghidra-side detection（沒有獨立 .py，用 `run_script_inline`）
- **Phase 2**：lookup vs lib `.obj` size diff regression（`compare_lookup_sizes.py`）
- **Phase 3**：orphan / gap scan + 修（沒有獨立 .py，用 `run_script_inline`）

## Scripts

| script | 用途 |
|---|---|
| `compare_lookup_sizes.py` | 對 `crt_lookup_9.5a.json` 全 entry 跑 `lookup body_size` vs lib `.obj` size 全面 diff，輸出 `workspace/jump_table_audit/lookup_size_diff.{json,md}` |

## Phase 1 (Ghidra-side detection)

`run_script_inline` 流程：

1. 用 `Listing.getInstructions(true)` 走全 program instruction，篩
   `FlowType.isJump() && isComputed() && !isConditional()`
2. 對每個 indirect JMP 取 `Instruction.getReferencesFrom()` 拿 Ghidra 已
   解析的 target 集合，比對是否 `function.getBody().contains(target)`
3. 旗標 `out_of_body > 0` 的 case；輸出 byte hex / asm / function 名

## Phase 3 (orphan / gap fix)

`run_script_inline` 掃描三項：

- **orphan instruction**：有 disassembly 但不在任何 function 內
- **abnormally small body**：function body < 5 byte 但前後 byte 是真實 code
- **fragmented body**：`AddressSetView.getNumAddressRanges() > 1` 且 hole
  在 entry instruction 跨度內

修法：

- `disassemble_bytes_mcp` 補 disassembly
- `Function.setBody(AddressSet)` 直接呼叫修 body（注意：
  `SetFunctionBodyCmd` class 在當前 Ghidra MCP 版本不存在）

## Phase 2 執行

```bash
python tools/program_analysis/jump_table_audit/compare_lookup_sizes.py
```

依賴輸入：

- `rebuild_info/crt/lookup_9.5a.json` — CRT lookup table
- `workspace/crt_callee_match/obj_funcs_clib3s.json` 或 `obj_funcs.json`
  — `extract_obj_funcs.py` 產出的 .obj 內 PUBDEF + size 索引

正常情境 `mismatch=0`；`name_not_in_obj` 多為 `L$N` anonymous static
（lib 端無 PUBDEF）。

## 何時跑

- 新增 / 調整 `crt_lookup_9.5a.json` entry 後做 regression
- 對 Ghidra `setBody` 修完後做 sanity check
- AIL / 遊戲端大規模重 disassemble 後

## 已知 Ghidra MCP `run_script_inline` 陷阱

- `ghidra.app.cmd.function.SetFunctionBodyCmd` 在當前版本不存在，必須用
  `Function.setBody(AddressSetView)` 直接呼叫
- `FunctionManager.getFunctionAfter(addr)` / `getFunctionBefore(addr)`
  不存在，用 `getFunctionAt` / `getFunctionContaining` 替代
- `run_script_inline` 偶爾 `GhidraScriptLoadException class not found`，
  重試即可（OSGi bundle reload race）
