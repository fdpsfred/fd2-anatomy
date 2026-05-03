# tools/lowconf_signature/

Follow-on cleanup pass for the LOW-confidence subset that the auto
param-count classifier in `tools/calling_convention_audit/` deliberately
skipped. The classifier emits HIGH/MEDIUM caller signals into
`param_count_recommendations.json` and silently drops the remaining 277
LOW-confidence cases; this module re-discovers that LOW set with full
per-function signal context, splits into actionable categories, and
provides a planning aid for the per-function disasm verification step.

ABI rules and signal interpretation: see
`program_info/calling_convention.md`. Final-state summary of the LOW
work: see `open_issues.md` issue #26.

## Workflow

The intended workflow is **per-function disasm verification + apply**,
not bulk rule-based apply. Reads (`reads_eax/edx/ecx`) and caller signal
flags both fire false positives on globals / locals / register clobber
patterns / Borland CRT custom ABI; relying on them for bulk apply
mis-signs ~half of the LOW set.

```bash
# Step 1 — rebuild the LOW set with per-function signal
python tools/lowconf_signature/inventory.py
# writes workspace/lowconf_signature/lowconf_inventory.csv +
#        lowconf_inventory.json + lowconf_summary.txt

# Step 2 — (optional reference) generate a rule-based prototype plan
python tools/lowconf_signature/plan_apply.py
# writes workspace/lowconf_signature/apply_plan.tsv  (review only)

# Step 3 — for each row, verify per function:
#   - mcp__ghidra__disassemble_function: read prologue + body offsets
#   - mcp__ghidra__decompile_function:   confirm body uses
#   - mcp__ghidra__get_assembly_context: cross-check caller push count
#   - mcp__ghidra__set_function_prototype: apply verified signature
#   - mcp__ghidra__list_bookmarks(category="Bad Instruction") = 0
```

## Files

- `inventory.py` — re-runs the param-count classifier
  (`expected_param_count` function from
  `tools/calling_convention_audit/param_count_classify.py`) against
  `workspace/calling_convention_audit/audit.json` + `recommendations.json`,
  recovers the LOW set, and categorises into
  seven buckets (dispatch_callee / spell_handler_id / execute / fun_low /
  unmatched_no_caller / mixed_signal / other / fragment). Filters out
  174 already-confirmed dispatch callees + decompiler fragments +
  pinned addresses + thunks. Workdir defaults to
  `<repo>/workspace/lowconf_signature/`.

- `plan_apply.py` — rule-based prototype generator: under conservative
  ratify rules emits one suggested prototype per LOW entry. Decision
  rules: R-stdcall-zero (RET 0 + 0 args ratify), R-cdecl-strip3 (Borland
  prologue, body doesn't read EAX/EDX/ECX, strip 3 phantom reg from
  cdecl pcur ≥ 3), R-fastcall-FFF-3 / -strip / -min (cdecl-flip variants
  for fastcall functions whose body doesn't actually read regs),
  R-fastcall-K-ratify / R-cdecl-reads-true-ratify (preserve current
  count for cases where `reads_*` likely false-positives on globals
  rather than reflecting reg input). Output is **reference material
  only** — per-function disasm verification is required before apply.

## Common patterns observed

- **Borland stack-probe prologue** (`PUSH framesize_imm; CALL 0x36cd7
  (crt_frame_setup)`): 3 phantom reg slots from old fastcall default
  prepended to N real cdecl stack args. Body reads `[esp+K]` (not
  `[ebp+K]`). Strip 3 phantom + apply N cdecl. Typical members:
  `spell_handler_id_*`, `execute_*`, `tick_summon` family,
  `chapter_NN_post_action` shared handlers.

- **Standard EBP prologue** (`PUSH EBP; MOV EBP, ESP`): N cdecl stack
  args at `[EBP+8/c/10/...]`. No phantom reg. Ghidra usually has these
  correctly classified; ratify only.

- **Borland CRT soft-FP / long-double family** (`FUN_0004b761`,
  `FUN_0004cb34`, `FUN_0004cb86`, `FUN_0004d53c`): non-standard ABI
  passing inputs in EBX/ESI/EDI in addition to EAX/EDX/ECX. Standard
  fastcall classifier cannot model. Defer to backlog until build
  pipeline can byte-compare against Watcom output.
