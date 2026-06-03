---
name: ghidra-usage
description: >-
  Battle-tested Ghidra MCP workflows for reverse-engineering binaries:
  documenting functions (V5 7-step process), naming variables / globals /
  strings with Hungarian notation, investigating and creating data-type
  structures, discovering orphaned code in the gaps between known functions,
  and matching functions across binary versions. Use whenever you work with the
  Ghidra MCP tools (decompile_function, set_function_prototype, rename_variables,
  apply_data_type, rename_or_label, batch_set_comments, create_struct,
  analyze_function_completeness, emulate_function, analyze_dataflow, etc.) to
  analyze or annotate a binary. Encodes the correct retry-free tool patterns and
  the ordering rules that stop later steps from clobbering earlier work.
---

# Ghidra MCP Reverse-Engineering Workflows

A library of prompts refined across thousands of functions for documenting and
analyzing binaries in Ghidra through the MCP tool surface. This file is the
entry point: it tells you which workflow file to open for the task at hand,
and inlines the universal rules and tool patterns that every workflow assumes.

The worked examples in the reference files use Diablo II module names
(`D2Client.dll`, `UnitAny`, ordinals, …) because that is where the prompts were
hardened — but the workflows are binary-agnostic. Apply them to whatever program
is open in the current Ghidra instance.

## When to use this skill

Reach for it the moment a task touches Ghidra annotation or analysis:

- Documenting a function: name, prototype, variable types, plate + inline comments.
- Naming data: globals, strings, lookup tables, struct instances, function pointers.
- Discovering a data structure's layout and applying it across every accessor.
- Finding functions auto-analysis missed (orphaned code between known boundaries).
- Propagating documentation across versions of the same binary.
- Any time you are unsure which MCP tool to use, or hit a tool that errors on retry.

## Choosing a workflow

| Goal | Open |
|------|------|
| **Document one function** (primary workflow) | `FUNCTION_DOC_WORKFLOW_V5.md` |
| **Find undiscovered / orphaned code** | `ORPHANED_CODE_DISCOVERY_WORKFLOW.md` |
| **Investigate a struct / parameter type** (full) | `DATA_TYPE_INVESTIGATION_WORKFLOW.md` |
| **Investigate a simple type** (abbreviated) | `DATA_TYPE_INVESTIGATION_QUICK.md` |
| **Document `.data` / `.rdata` globals** | `DATA_SECTION_WORKFLOW.md`, `GLOBAL_DATA_ANALYSIS_WORKFLOW.md` |
| **Label every defined string** | `STRING_LABELING_CONVENTION.md` |
| **Match functions across binary versions** (full) | `CROSS_VERSION_MATCHING_COMPREHENSIVE.md` |
| **Match functions across versions** (quick) | `CROSS_VERSION_FUNCTION_MATCHING.md` |
| **Order for documenting a binary family** | `BINARY_DOCUMENTATION_ORDER.md` |
| **MCP tool reference & type-application patterns** | `TOOL_USAGE_GUIDE.md` |
| **Change the enforced naming conventions** | `CUSTOMIZING_CONVENTIONS.md` |

`README.md` carries the same index in its original form. Read the chosen file in
full before acting — these workflows are precise about ordering and tool choice.

## Universal rules (every workflow assumes these)

1. **Ordering — name/type before comment.** Complete ALL naming, prototype, and
   type changes BEFORE writing the plate comment and inline comments.
   `set_function_prototype` wipes existing plate comments, so commenting first
   loses the work.
2. **Type-first naming.** Never give a variable a Hungarian prefix
   (`dw`, `n`, `p`, `sz`, …) while its type is still `undefined*`. Resolve the
   type with `set_local_variable_type` first, then rename. If the type is
   genuinely unknown, use a descriptive name with no type prefix
   (`questBits`, not `dwQuestBits`).
3. **Prefix ↔ type consistency.** A parameter named `pGame` typed as `int` is a
   violation — fix the type to a pointer. The prefix must always match the type.
4. **Batch, don't loop.** Use one `rename_variables` call (single dict) for all
   variables and one `batch_set_comments` call (plate + PRE + EOL together).
   Never loop individual rename/comment calls.
5. **Phantoms are artifacts.** `extraout_*` and `in_*` variables with `undefined`
   types come from the decompiler, not the code. Note them in the plate comment's
   Special Cases section; do not retry type-setting on them.
6. **Reprocessing overwrites.** When re-documenting, overwrite existing
   names/comments if your analysis is better — even custom values. A pre-existing
   custom name can still be wrong; verify it describes what the code actually does.
7. **Verify-fix loop.** End with `analyze_function_completeness`. If fixable
   deductions exceed 10 points (undocumented magic numbers, `undefined` types,
   missing plate), fix them and re-verify before reporting DONE. Acceptable
   unfixable deductions: phantoms, API-mandated `void*` params, standard `lp`/`h`
   API parameter names.

## Reliable tool patterns

The single most common source of wasted retries is reaching for a fragile tool.
Prefer the proven ones.

**Apply a data type in three separate steps** — never `create_and_apply_data_type`
(its `type_definition` param rejects strings and forces a retry loop):

```python
apply_data_type(address, "char[6]")        # 1. set the type
rename_or_label(address, "szVideoSection") # 2. rename (auto-detects code vs data)
set_decompiler_comment(address, "…")       # 3. document (only after type + name)
```

`apply_data_type` type names: primitives `dword word byte int short char float
double pointer qword longlong bool`; arrays/strings `char[N] word[N] dword[N]
byte[N] pointer[N]`. Use hex sizes for padding (`_1[0x158]`, not `_1[344]`).

**Prefer native MCP tools over scripting.** Use `rename_function_by_address`,
`set_function_prototype`, `rename_variables`, `set_local_variable_type`,
`batch_set_comments`, etc. Do **not** reach for `run_script_inline` /
`run_ghidra_script` for routine documentation — and note that as of v5.4.1 both
are gated behind `GHIDRA_MCP_ALLOW_SCRIPTS=1` and return 403 by default. (The
orphaned-code scanner is the one workflow that does ship a Java script.)

**Validation / inspection helpers:** `validate_data_type_exists`,
`can_rename_at_address`, `get_function_variables` (the only source of *actual*
storage types — `analyze_for_documentation` is not), `analyze_data_region`,
`inspect_memory_content`, `get_bulk_xrefs`.

## Hungarian notation (quick reference)

```
b:byte   c:char   f:bool(fn-level)   n:int/short   dw:uint/DWORD   w:ushort   l:long
fl:float d:double ll:longlong  qw:ulonglong  ld:float10  h:HANDLE
p:void*/ptr  pb:byte*  pw:ushort*  pdw:uint*  pn:int*  pp:void**
sz:char*(local)  lpsz:char*(param)  wsz:wchar_t*  lpcsz:const char*(param)
ab:byte[N]  aw:ushort[N]  ad:uint[N]  an:int[N]
g_:mutable global   k_:rdata constant   pfn:function pointer (PascalCase, no g_)
struct pointers: p + StructName  (pUnit, pInventory; ppItem for double pointer)
struct field bool uses b: (bActive, bVisible)
```

**Type normalization:** `undefined1`→byte, `undefined2`→ushort,
`undefined4`→uint/int/float/ptr (by usage), `undefined8`→double/longlong. Use
Ghidra builtins (`dword`, `byte`, `ushort`) for `set_local_variable_type`, not
Windows typedefs (`DWORD`, `BYTE`).

**Function names:** PascalCase, verb-first, specific —
`GetPlayerHealth`, `ProcessInputEvent`, `ValidateItemSlot`. Fix
`SKILLS_GetLevel`→`GetSkillLevel`, `processData`→`ProcessData`.

**String labels** follow `sz<Category>_<Description>` (`szApi_GetTickCount`,
`szErr_FileOpen`, `szPath_GlobalMonsters`); the full category set lives in
`STRING_LABELING_CONVENTION.md`. **Globals** use `g_` (mutable, `.data`) or `k_`
(read-only, `.rdata`): `g_dwPlayerHealthMax`, `k_pszWelcomeMessage`.

## Dynamic-analysis cross-checks

When static decompilation is ambiguous, three endpoints run or trace code
directly. Use them to *falsify* a wrong claim before marking work DONE — they are
cross-checks, not replacements. Best for leaf functions (hashes, CRC/checksum,
bit-packing); skip for anything with heap/syscall side effects.

- **`analyze_dataflow(address, variable, direction)`** — `backward` walks
  producers (where a return/sink value came from); `forward` walks consumers
  (every place a parameter flows to). `variable` is a register (`EAX`), a
  HighVariable (`param_1`, `local_14`), or empty for the first PcodeOp's output.
- **`emulate_function(address, registers, memory, …)`** — pure P-code execution,
  no process. **Format matters** (getting it wrong reads garbage args and hangs):
  - `registers` is a JSON object: `{"ECX": "0x10", "EDX": "0x20"}`.
  - `memory` needs the `regions` wrapper:
    `{"regions": [{"address": "0x...", "hex": "DEC0ADDE"}]}` (regions take
    `hex`, `data` base64, or `string`).
  - Stack is auto-initialized at `0x7FFF0000` with a `0xDEADBEEF` return
    sentinel; cdecl arguments go at `[0x7FFF0004]`, `[0x7FFF0008]`, …
  - `hit_return: true` means it ran to RET (didn't hit `max_steps`).
- **`emulate_hash_batch(...)`** — brute-force API-hash resolution: iterate a
  candidate string list through a hash function, return ALL collisions (check the
  full `matches` array, not just `best_match`).

## Function tagging

Lightweight, program-wide, persistent labels for carving curated subsets across
long sessions (`crypto`, `parser`, `reviewed`, `todo`). Attach with
`add_function_tag` (auto-creates the definition) or `batch_add_function_tags`
(one transaction for a whole sweep); recall with `search_functions_by_tag`. Tags
are case-sensitive and survive save/checkin.

## Reference files

| File | Contents |
|------|----------|
| `README.md` | Original index of all prompts |
| `FUNCTION_DOC_WORKFLOW_V5.md` | 7-step function documentation: classify → rename+prototype → type audit → comments → verify, with the dynamic cross-check option |
| `ORPHANED_CODE_DISCOVERY_WORKFLOW.md` | Java gap scanner + Type A–G candidate triage + batch `create_function` |
| `DATA_TYPE_INVESTIGATION_WORKFLOW.md` | 7-phase struct discovery: offset-map every accessor, find/create the struct, apply across all functions, verify |
| `DATA_TYPE_INVESTIGATION_QUICK.md` | Single-paragraph version for simple types |
| `DATA_SECTION_WORKFLOW.md` | Enumerate → type → name → document `.data`/`.rdata` globals |
| `GLOBAL_DATA_ANALYSIS_WORKFLOW.md` | `g_`/`k_` segmentation and naming of mutable vs read-only globals |
| `STRING_LABELING_CONVENTION.md` | `sz<Category>_` taxonomy + `batch_create_labels` workflow + decision tree |
| `CROSS_VERSION_MATCHING_COMPREHENSIVE.md` | Version clusters, DLL-migration awareness, 6-tier matching, confidence tracking |
| `CROSS_VERSION_FUNCTION_MATCHING.md` | Quick 4-method matching guide (hash / string / call-graph / ordinal) |
| `BINARY_DOCUMENTATION_ORDER.md` | Dependency-tier order for documenting a whole binary family |
| `TOOL_USAGE_GUIDE.md` | MCP tool reference: reliable type application, doc templates, hashing/propagation, dynamic + debugger tools, security env vars |
| `CUSTOMIZING_CONVENTIONS.md` | `conventions.json` schema, Tool-Option/per-call `strict_mode` overrides |
