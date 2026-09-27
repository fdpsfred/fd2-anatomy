---
name: ghidra-usage
description: Ghidra MCP reverse engineering — use when documenting decompiled functions, typing variables or discovering structs, naming globals in .data/.rdata, labeling strings, hunting orphaned code, or customizing naming conventions in a Ghidra program.
---

# Ghidra MCP reverse engineering

Conventions and workflows for annotating a binary through the Ghidra MCP server, refined across thousands of functions. Everything on this page holds in every pass; each pass's own procedure sits behind a pointer in **Pick the pass** at the bottom.

## Before the first write

1. `check_connection`. On refusal, stop and tell the user to open a program in CodeBrowser and start the server (Tools ▸ GhidraMCP ▸ Start MCP Server), or run `ghidra-mcp-setup.ps1 -Deploy`. Dispatch no subagents past a refused connection — they fail identically.
2. `get_current_program_info`. `program` is a *query* parameter on every write endpoint: omit it and the write lands on whichever program is active, which is how annotations leak into the wrong binary during multi-version work.

## Rules

- **In-place.** Changes land in the Ghidra database through MCP tools. Write no filesystem files.
- **Native tools.** `run_script_inline` and `run_ghidra_script` return 403 unless `GHIDRA_MCP_ALLOW_SCRIPTS=1` (they execute arbitrary Java against the Ghidra process). Do the work with native MCP tools.
- **Tool names drift.** These files span many server versions. The loaded MCP tool list is the authority — when a documented name is absent, find its live equivalent there. Known drift: 7.0.0 collapsed `set_plate_comment` / `set_decompiler_comment` / `set_disassembly_comment` into `set_comment(address, comment, type=…)`, and `set_variable_type` appears as `set_local_variable_type` on older builds.
- **Type-first.** Resolve a symbol's type before naming it — a Hungarian prefix on an `undefined*` variable is a claim the storage does not support. When the type stays unknowable, give a descriptive name with no type prefix (`questBits`, not `dwQuestBits`).
- **Prefix matches type.** After every prototype or type write, check each name's prefix against its actual storage type. `pGame` typed `int` is a violation: fix the type to a pointer. A variable that is dereferenced or does offset arithmetic is a pointer, whatever the decompiler displays.
- **Storage, not display.** The decompiler shows `int` or `FILE*` while storage remains `undefined4`. Only `get_function_variables` reveals real storage types — call it explicitly rather than trusting an analysis summary, and call it again after type changes to catch new SSA variables.
- **Batch.** One `rename_variables` dict, one `batch_set_comments` (plate + PRE + EOL together), one `create_label` array. Never loop single-item calls. Retry a network timeout up to 3 times, then shrink the batch.
- **Overwrite.** On a re-pass, replace existing names and comments whenever the fresh analysis is better — custom values included.
- **The disassembly is the authority.** `analyze_function_completeness` scores *hygiene*: documentation present and well-formed. It is computed from the documentation, so a confidently wrong plate still scores 100. Truth is a separate axis, checked mechanically by `fun-doc/falsify.py` — declared calling convention vs the callee's actual `RET n`, plate parameters vs the live signature, `Get*`/`Is*` names on functions that write globals, plate/prototype return contradictions. A tier-1 contradiction stamps `DOC_REFUTED`, flags the plate `[AUDIT falsify:*]`, and re-queues the function regardless of score. So assert only what the disassembly supports — a bare `RET` with stack args is cdecl no matter what the decompiler guessed. When a plate carries an `[AUDIT falsify:*]` flag, resolving it is the pass's first job, and you correct the documentation to match the disassembly, never the reverse.

## Hungarian notation

Authoritative where the workflow files' variant tables disagree.

```
b:byte  by:byte  c:char  f:bool(local)  n:int/short  dw:uint/DWORD  w:ushort  l:long
fl:float  d:double  ll:longlong  qw:ulonglong  ld:float10  h:HANDLE  cb:byte count
p:void*/ptr  pb:byte*  pw:ushort*  pdw:uint*  pn:int*  pp:void**  lp:ptr(legacy Win32)
sz:char*(local)  lpsz:char*(param)  wsz:wchar_t*  lpcsz:const char*(param)
ab:byte[N]  aw:ushort[N]  ad:uint[N]  an:int[N]  ap:ptr[N]
g_:mutable global (.data)   k_:read-only constant (.rdata)   pfn:func ptr (PascalCase, no g_)
Struct pointer: p+StructName (pUnit, ppItem)    Struct-field bool: b    Global array: g_ap*
```

Functions are PascalCase, verb-first: `GetPlayerHealth`, `ValidateItemSlot` — `SKILLS_GetLevel` becomes `GetSkillLevel`. Labels are snake_case and name the purpose: `loop_start`, `validation_failed`, `state_1_processing`.

**Type normalization** for `set_variable_type` / `apply_data_type`: `undefined1`→byte, `undefined2`→ushort, `undefined4`→uint/int/float/ptr by usage, `undefined8`→double/longlong. Use Ghidra builtins (`dword`, `byte`, `ushort`), not Windows types (`DWORD`, `BYTE`). Sizes in hex: `_1[0x158]`.

## Plate comments

Plain text only — Ghidra draws the borders and `/* */` markers itself, so supplying your own corrupts the rendering.

```
One-line summary.

Algorithm:
1. [one action per step, magic numbers in hex and decimal: type == 0x4E (78)]

Parameters:
  name: Type - purpose [IMPLICIT EDX when register-passed]

Returns:
  type: meaning, covering every return path

Special Cases:
  - edge cases, phantom variables, sentinel values, decompiler discrepancies

Structure Layout: (when the function walks a struct)
  Offset | Size | Field  | Type | Description
  +0x00  | 4    | dwType | uint | ...
```

Worked plates for validation, init, table-walk, string, and trivial-getter shapes: [PLATE_COMMENT_EXAMPLES.md](PLATE_COMMENT_EXAMPLES.md).

`set_comment` writes any of `plate|pre|eol|post|repeatable` at **any** address — data and undefined bytes included, not just function entries. `decompiler` aliases `pre`, `disassembly` aliases `eol`, and an empty comment clears that type. Put PRE comments at block starts (~60 chars, algorithm context) and EOL comments at instructions (≤32 chars, naming every hex constant).

## Phantoms and unfixable deductions

These resist typing by design. Note them in the plate's Special Cases and move on — retrying burns turns and the completeness scorer already discounts them.

- `extraout_*` / `in_*` variables with `undefined` types: decompiler artifacts.
- Register-only SSA variables (`pDVar1`): absent from `getLocalVariables()`, so unrenameable and untypeable. Document the intended type in a PRE comment instead — `nIterator: int - loop counter (register-only)`.
- `set_local_variable_type` returning "No HighVariable found": stack arrays (`ushort[6]`) and decompiler-inferred composites. Skip on first failure.
- `firstUseOffset` blocks on stack SSA variables at non-zero offsets.
- `this` as `void *` in `__thiscall`, and API-mandated `void *` params (`DllMain pvReserved`).

## Pick the pass

| When | Read |
|---|---|
| Document one function end to end | [FUNCTION_DOC_WORKFLOW_V5.md](FUNCTION_DOC_WORKFLOW_V5.md) — the primary workflow: classify, rename + prototype, type audit, comments, verify-fix loop |
| Fan out over many functions | [FUNCTION_DOC_WORKFLOW_V5_BATCH.md](FUNCTION_DOC_WORKFLOW_V5_BATCH.md) — target selection, subagent dispatch (max 3 concurrent; MCP serializes at the Ghidra HTTP layer), model choice, recurring failure modes |
| An untyped `int *` / `void *` parameter needs its real struct | [DATA_TYPE_INVESTIGATION_QUICK.md](DATA_TYPE_INVESTIGATION_QUICK.md) — offset-map every accessor, match or create the struct, apply it everywhere |
| Globals, tables, and vtables in .data/.rdata | [DATA_SECTION_WORKFLOW.md](DATA_SECTION_WORKFLOW.md) — enumerate by xref count, type before naming, ownership notes, validation pass. [GLOBAL_DATA_ANALYSIS_WORKFLOW.md](GLOBAL_DATA_ANALYSIS_WORKFLOW.md) is the same pass compressed into one dispatchable paragraph |
| Label every defined string | [STRING_LABELING_CONVENTION.md](STRING_LABELING_CONVENTION.md) — `sz[Category]_[Description]` with a content→category decision tree |
| Functions auto-analysis never marked | [ORPHANED_CODE_DISCOVERY_WORKFLOW.md](ORPHANED_CODE_DISCOVERY_WORKFLOW.md) — gap scanner, seven candidate types, triage plate, iterative re-scan. Its scanner runs through `run_script_inline`, so it needs `GHIDRA_MCP_ALLOW_SCRIPTS=1`; ask the user to enable it before starting |
| Tool-call patterns, dynamic analysis, or server config | [TOOL_USAGE_GUIDE.md](TOOL_USAGE_GUIDE.md) — the reliable three-step data pattern, `analyze_dataflow` / `emulate_function` / `emulate_hash_batch`, both `debugger_*` families, function tags, per-program options and property maps, cross-binary hash propagation, auth env vars |
| House style differs from these conventions | [CUSTOMIZING_CONVENTIONS.md](CUSTOMIZING_CONVENTIONS.md) — `<project>/.ghidra-mcp/conventions.json`, the Tool Option, per-call `strict_mode` |

`QUICK_START_PROMPT.md` is the pre-V5 monolithic prompt, kept for history — V5 supersedes it for function work.
