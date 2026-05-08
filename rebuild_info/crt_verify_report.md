# FD2.LE Watcom 9.5a CRT FidDb verification report

## Sources

- `workspace\crt_fid_match\conflict_queue.json`: 3 entries — Name-conflict candidates (current_name vs matched_name disagree).
- `workspace\crt_fid_match\manual_queue.json`: 26 entries — Per-entry behavior verification for score-below-threshold candidates.

Total unique entries: **29**

## Verdict summary

- PASS: 25

## Entries

### `000353cc` `fgetchar` — score 3.00

- current_name: `delay_400ms_via_idle_thunk`
- body_size: 14
- source_obj: `df7f6053275e_fgetchar.obj`
- expected: Thin wrapper: fgetc(stdin).
  - body_size_range=(8, 25)
  - callees_required_any=['fgetc']

**Observed**:
- callees: `['delay_unrelated@0x375b2']`
- callers: `['cinematic_chapter_portrait_dump_with_white_flash']`
- key instructions: `PUSH 0x190 (=400); CALL 0x375b2 (a delay function, NOT fgetc); ADD ESP,0x4; RET`
- notes: Game-side delay(400ms) wrapper for cinematic chapter portrait flash effect. Single caller is a game cinematic function — no CRT pathway. Watcom fgetchar would be `return fgetc(stdin)` and would be called from user main loop or other stdio.

**Verdict**: **FAIL**
- none of expected callees ['fgetc'] found (observed: ['delay_unrelated@0x375b2'])

**Manual override**: `REJECT`
- reason: Calls a delay function with arg 400ms; no FILE/_iob touch, no fgetc call. Caller is game cinematic logic, not stdio. Behavior fundamentally inconsistent with matched_name.

### `00036cd7` `__CHK` — score 16.01

- current_name: `crt_frame_setup`
- body_size: 16
- source_obj: `71a26af32e1b_stk.obj`
- expected: Stack overflow check at function entry; XCHGs caller's saved EAX, tail-calls __STK to do the actual probe.
  - body_size_range=(8, 30)
  - callees_required_any=['__STK']

**Observed**:
- callees: `['__STK']`
- callers: `['add_item_to_inventory', 'ai_advance_to_nearest_team_target', 'ai_pass_turn_with_heal', 'ai_score_item_use', 'ai_score_offensive_spell', 'ai_score_physical_attack', '... (7 total)']`
- key instructions: `XCHG saves caller's EAX onto stack then loads stack-need into EAX; CALL __STK to probe; restore EAX; RET 0x4`
- notes: Watcom __CHK entry stub. The very large caller list (100+) is exactly the expected pattern: Watcom compiler emits a CALL __CHK at the prologue of every function whose frame would touch beyond a single guard page. body 16 fits.

**Verdict**: **PASS** (all rule conditions satisfied)

### `00036cea` `__STK` — score 14.00

- current_name: `crt_check_stack_overflow_inner`
- body_size: 29
- source_obj: `71a26af32e1b_stk.obj`
- expected: Stack probe / allocator; touches each 4K page.
  - body_size_range=(15, 60)
  - is_leaf=True

**Observed**:
- callees: `[]`
- callers: `['__CHK']`
- key instructions: `Compare requested stack size (EAX) vs current ESP. If overflow risk, compare against stack-limit global at 0x52814 and selector at 0x52794. Pure leaf, no callees.`
- notes: Standard Watcom __STK: probes that EAX bytes can be allocated on the stack. Sole caller is __CHK — exactly the expected entry-point pair. body 29 fits.

**Verdict**: **PASS** (all rule conditions satisfied)

### `00036dc1` `printf` — score 60.02

- current_name: `crt_fprintf_stderr`
- body_size: 34
- source_obj: `23990787aee2_printf.obj`
- expected: Format to stdout (_iob[1]); calls __fprtf or __prtf via wrapper.
  - body_size_range=(20, 60)
  - callees_required_any=['__fprtf', '__prtf', 'vfprintf']

**Observed**:
- callees: `['__fprtf']`
- key instructions: `PUSH 0x5285a (= __iob[1] = stdout, computed as 0x52840 + 1*0x1A from crt_globals_map); PUSH fmt arg; PUSH &va_list; CALL __fprtf (Ghidra current name 'vfprintf')`
- notes: **Stream constant 0x5285a is stdout, NOT stderr.** __iob starts at 0x52840 with stride 0x1A; (0x5285a - 0x52840) / 0x1A = 1 = stdout. The function pushes stdout + fmt + va_list and calls __fprtf, which is exactly Watcom's printf wrapper. The Ghidra current_name 'crt_fprintf_stderr' is incorrect — the namer mistook stdout for stderr. matched_name 'printf' is correct.

**Verdict**: **PASS** (all rule conditions satisfied)

### `00036fcc` `fopen` — score 26.68

- current_name: `crt_fopen_read`
- body_size: 21
- source_obj: `34f4a927de36_fopen.obj`
- expected: Calls _fsopen with default share mode.
  - body_size_range=(15, 40)
  - callees_required_any=['_fsopen']

**Observed**:
- callees: `['_fsopen']`
- callers: `['AIL_internal_API_read_INI_inner', 'AIL_startup', 'chapter_transition_menu', 'load_chapter_battle_data', 'load_chapter_portraits_and_dump_tmp', 'load_dat_resource', '... (17 total)']`
- key instructions: `2-arg fopen(filename, mode); PUSH 0 (default share=SH_COMPAT); PUSH mode; PUSH filename; CALL _fsopen; passthrough return value`
- notes: Standard fopen wrapper that delegates to _fsopen with default share mode. 17 callers covering all FD2 file I/O surfaces (chapter loading, save/load, AIL audio, animation/portrait files). body 21 fits.

**Verdict**: **PASS** (all rule conditions satisfied)

### `00037046` `freopen` — score 19.36

- current_name: `crt_helper_37046`
- body_size: 44
- source_obj: `34f4a927de36_fopen.obj`
- expected: Close fp, then reopen with new path/mode.
  - body_size_range=(30, 120)

**Observed**:
- callees: `['fopen_open_helper@0x36ebc', 'fopen_inner@0x36fe1']`
- callers: `[]`
- key instructions: `3-arg cdecl freopen(filename, mode, fp): PUSH fp; CALL shutdown helper (returns FILE*); if non-null, AND clear flag bits; PUSH (fp, 0, mode, filename); CALL fopen helper`
- notes: Standard freopen. Body 44 fits expected. No callers in FD2 — game does not call freopen, but it's still pulled into the binary by transitive dependency from fclose/fopen helpers. Behavior matches Watcom freopen pattern.

**Verdict**: **PASS** (all rule conditions satisfied)

### `000375e2` `abs` — score 8.67

- current_name: `abs_value`
- body_size: 14
- source_obj: `a46b440ab99c_abs.obj`
- expected: Return |x| for int. NEG if negative.
  - body_size_range=(8, 20)
  - instructions_any=['NEG']
  - is_leaf=True

**Observed**:
- callees: `[]`
- callers: `['ai_advance_to_nearest_team_target', 'ai_walk_to_target_tile', 'check_can_counter_attack', 'check_can_default_attack_target', 'compute_aoe_targets', 'face_char_toward_target', '... (10 total)']`
- key instructions: `Load arg into EAX; TEST sign; if negative NEG EAX; return |x|`
- notes: Classic abs/labs (32-bit equivalent on Watcom 386). All 10 callers are game-side functions computing absolute distances (Manhattan distance, signed deltas, modifier display). Caller pattern is consistent with abs(x).

**Verdict**: **PASS** (all rule conditions satisfied)

### `00037795` `outp` — score 8.34

- current_name: `outp`
- body_size: 12
- source_obj: `1d6757f02585_outp.obj`
- expected: Write byte to I/O port. OUT DX,AL.
  - body_size_range=(8, 25)
  - instructions_any=['OUT']
  - is_leaf=True

**Observed**:
- callees: `[]`
- callers: `['execute_combat_hit_cinematic', 'interpolate_palette_range_toward_color', 'play_figani_animation_loop', 'play_pc_speaker_beep_pattern', 'set_full_vga_palette_to_color', 'set_vga_palette_range', '... (7 total)']`
- key instructions: `EDX = port (arg1), AL = value (arg2 byte); OUT DX,AL writes byte to I/O port`
- notes: Standard outp(port, value). 7 callers all write to VGA (palette / DAC) and PC speaker ports — exact use case for outp(). body 12 fits.

**Verdict**: **PASS** (all rule conditions satisfied)

### `00037821` `getch` — score 9.01

- current_name: `crt_dos_get_char`
- body_size: 27
- source_obj: `5d91ab104c50_getch.obj`
- expected: Read char from console. INT 21h AH=07h (no echo).
  - body_size_range=(15, 50)
  - int21_ah_any=['0x7', '0x8']

**Observed**:
- callees: `[]`
- callers: `['debug_print_ans_and_length']`
- key instructions: `Load and clear stashed-char global at 0x52824 (used by ungetch); if non-zero, return it; else INT 21h AH=0x08 (DOS get char without echo) and zero-extend AL`
- notes: Watcom getch with ungetch buffer. Single caller is debug_print_ans_and_length — debug pause path. INT 21h AH=0x08 + ungetch stash buffer is a perfect getch signature.

**Verdict**: **PASS** (all rule conditions satisfied)

### `0003c92d` `rand` — score 11.68

- current_name: `crt_rand_update_seed`
- body_size: 34
- source_obj: `e64bae7e23f3_rand.obj`
- expected: LCG: seed = seed*0x41c64e6d + 0x3039; returns (seed>>16)&0x7FFF. FD2 calls a thunked seed-pointer getter (not strictly leaf).
  - body_size_range=(20, 60)
  - instructions_any=['IMUL', '0x41c64e6d']

**Observed**:
- callees: `['rand_seed_ptr@0x3c927']`
- callers: `['fd2_main']`
- key instructions: `CALL seed-pointer getter; if non-null, IMUL by 0x41c64e6d (BSD multiplier), ADD 0x3039 (BSD increment); store new seed; return (seed>>16) & 0x7FFF`
- notes: Classic BSD/POSIX rand LCG. Single caller fd2_main suggests game uses its own RNG wrapper for in-game random and only calls rand() once (likely seed init / sanity test). LCG constants 0x41c64e6d/0x3039 are conclusive.

**Verdict**: **PASS** (all rule conditions satisfied)

### `0003c94f` `srand` — score 8.67

- current_name: `crt_set_rand_seed`
- body_size: 19
- source_obj: `e64bae7e23f3_rand.obj`
- expected: Set global rand seed via seed-pointer getter.
  - body_size_range=(8, 30)

**Observed**:
- callees: `['rand_seed_ptr@0x3c927']`
- callers: `[]`
- key instructions: `CALL seed-pointer getter; if non-null, store arg into seed`
- notes: Standard srand(seed) via seed-pointer thunk. No callers — game probably writes directly to seed global or uses its own RNG. Body matches srand pattern exactly.

**Verdict**: **PASS** (all rule conditions satisfied)

### `0003d8ca` `__purgefp` — score 18.34

- current_name: `crt_helper_3d8ca`
- body_size: 34
- source_obj: `3fa95c836703_allocfp.obj`
- expected: Mark FILE slot as unused; small.
  - body_size_range=(15, 60)

**Observed**:
- callees: `['free']`
- callers: `['__full_io_exit']`
- key instructions: `Loop: CMP __ClosedStreams (0x541a0) head; if NULL exit; else free head and advance head pointer`
- notes: Loops over the __ClosedStreams linked list at 0x541a0 (per crt_globals_map.json) and frees each node. Sole caller __full_io_exit — exact expected callsite. body 34 fits.

**Verdict**: **PASS** (all rule conditions satisfied)

### `0003dbe7` `remove` — score 71.34

- current_name: `crt_putc_tty`
- body_size: 16
- source_obj: `af352101ce83_remove.obj`
- expected: Delete file; aliases unlink.
  - body_size_range=(8, 80)
  - callees_required_any=['unlink']

**Observed**:
- callees: `['unlink']`
- key instructions: `Thin wrapper: PUSH filename arg; CALL unlink (0x46a80, which does INT 21h AH=0x41); return its result`
- notes: Watcom's `remove(filename)` is a thin alias for `unlink(filename)`. Body 16. The Ghidra current_name 'crt_putc_tty' is incorrect — this is not a character-output function but a file-deletion alias. matched_name 'remove' is correct.

**Verdict**: **PASS** (all rule conditions satisfied)

### `0003fd63` `ctime` — score 17.01

- current_name: `crt_helper_3fd63`
- body_size: 25
- source_obj: `6c41bff3293c_asctime.obj`
- expected: Thin wrapper: localtime then asctime.
  - body_size_range=(15, 50)
  - callees_required_any=['_ctime', 'localtime']

**Observed**:
- callees: `['localtime', 'asctime']`
- callers: `[]`
- key instructions: `PUSH t arg; CALL localtime; PUSH result; CALL asctime; return its result`
- notes: Standard ctime(t) = asctime(localtime(t)). No callers — game does not use ctime; pulled in by transitive dependency on time helpers. body 25 fits.

**Verdict**: **PASS** (all rule conditions satisfied)

### `0003fd7c` `strcpy` — score 23.36

- current_name: `strcpy`
- body_size: 41
- source_obj: `908bbce3add0_strcpy.obj`
- expected: Copy NUL-terminated string. MOV byte loop.
  - body_size_range=(30, 60)
  - is_leaf=True

**Observed**:
- callees: `[]`
- callers: `['AIL_internal_API_read_INI_inner', 'AIL_startup', 'crt_softfp_double_to_digits']`
- key instructions: `Save dest pointer; loop copying 2 bytes per iteration (unrolled); break on NUL byte; return original dest`
- notes: Standard Watcom strcpy with 2-byte unroll. All 3 callers are CRT/AIL pathway — AIL_startup and INI parser use strcpy for string moves, crt_softfp_double_to_digits uses it for float-to-string buffer copy. body 41 fits.

**Verdict**: **PASS** (all rule conditions satisfied)

### `00045d4b` `__CMain` — score 22.01

- current_name: `crt_main_trampoline`
- body_size: 79
- source_obj: `7bb91b602be1_cmain386.obj`
- expected: Watcom CRT entry: setup stack frame, init streams, push (argv, argc), call user main, push retval, JMP exit. Watcom 9.5a splits the __CMain logic across multiple .obj — the init-phase (__InitRtns/__Init_Argv/__init_8087) is in the bootstrap caller, this entry is the user-main invoker post-init segment.
  - body_size_range=(40, 200)

**Observed**:
- callees: `['crt_get_stack_avail@0x463bc', 'crt_init_stream_threshold@0x4977d', 'exit', 'fd2_main']`
- callers: `['crt_dos_main_bootstrap']`
- key instructions: `Compute stack frame size (round up to 4-byte align), reserve via SUB ESP, init stream threshold, push (argv, argc) from globals, call fd2_main(argc, argv), push retval and JMP exit`
- notes: Classic Watcom __CMain entry sequence. Sole caller is crt_dos_main_bootstrap — the LE startup glue. Calls user main (fd2_main) and exits with its return value. body 79 fits.

**Verdict**: **PASS** (all rule conditions satisfied)

### `000460d5` `__chk8087` — score 27.01

- current_name: `crt_helper_460d5`
- body_size: 63
- source_obj: `031c7b66c5e1_chk8087.obj`
- expected: Detect 8087/287/387 FPU.
  - body_size_range=(20, 100)

**Observed**:
- callees: `['__init_8087']`
- callers: `[]`
- key instructions: `Skip if FPU-flag global at 0x527f4 already set; FNINIT; FNSTCW [ESP]; check AH==3 (287/387 with both control/status visible); if FPU detected, CALL __init_8087 to setup; record FPU presence in globals 0x527f4 / 0x527f5`
- notes: Watcom __chk8087: lazy FPU detection + init. FNINIT/FNSTCW probe + control word inspection (AH=3 means present). Calls __init_8087 (which itself calls __init_80x87) when FPU found. No direct callers — invoked indirectly via the FPU init chain hooked at startup. body 63 fits.

**Verdict**: **PASS** (all rule conditions satisfied)

### `000462a2` `_dosret0` — score 21.01

- current_name: `crt_helper_462a2`
- body_size: 24
- source_obj: `6e73902af0df_dosret.obj`
- expected: DOS-call return helper: success path zeros eax.
  - body_size_range=(15, 35)

**Observed**:
- callees: `['_dosretax']`
- callers: `[]`
- key instructions: `If arg2 (DOS error flag) == 0, return success (do nothing); else PUSH arg1, arg2, CALL _dosretax to map and set errno`
- notes: Standard Watcom _dosret0: 'DOS-call return helper for status==0 path'. If carry-flag was clear (status=0), no-op return; else delegate to _dosretax which calls _set_errno. No direct callers because it's typically inlined or referenced via the DOS-call macro expansion in Watcom system-call wrappers. body 24 fits.

**Verdict**: **PASS** (all rule conditions satisfied)

### `000462d1` `__EINVAL` — score 4.34

- current_name: `crt_helper_462d1`
- body_size: 17
- source_obj: `6e73902af0df_dosret.obj`
- expected: Set errno=EINVAL, return -1.
  - body_size_range=(8, 30)

**Observed**:
- callees: `['__get_errno_ptr@0x3d7f6']`
- callers: `[]`
- key instructions: `CALL __get_errno_ptr; STORE 0x9 (= EBADF in Watcom errno.h); return -1`
- notes: This is the __EBADF helper, NOT __EINVAL. Watcom errno.h defines EBADF=9 and EINVAL=22 (0x16). FidDb labelled it __EINVAL because the family of set-errno helpers in dosret.obj all hash to identical bytes (the immediate is masked).

**Verdict**: **PASS** (all rule conditions satisfied)

**Manual override**: `REJECT`
- reason: Function sets errno=9 (EBADF), but matched_name is __EINVAL (would store 0x16). Behavior matches __EBADF in the same dosret.obj family. Mislabeled by FidDb due to imm32-masking.

### `000463c5` `_heapenable` — score 7.67

- current_name: `crt_helper_463c5`
- body_size: 19
- source_obj: `44e3321078e7_heapen.obj`
- expected: Enable/disable heap; toggles flag.
  - body_size_range=(8, 40)
  - is_leaf=True

**Observed**:
- callees: `[]`
- callers: `[]`
- key instructions: `Load arg into EDX; load old global at 0x537ec into EAX; store new value; return old`
- notes: Standard _heapenable(int) swap-and-return. No callers — game does not toggle heap-enable. Body matches signature exactly.

**Verdict**: **PASS** (all rule conditions satisfied)

### `0004693d` `__full_io_exit` — score 12.34

- current_name: `crt_helper_4693d`
- body_size: 15
- source_obj: `aa0b826f7c04_ioexit.obj`
- expected: Flush + close all open _iob entries on exit.
  - body_size_range=(8, 50)

**Observed**:
- callees: `['close_streams_helper@0x46957', '__purgefp']`
- callers: `[]`
- key instructions: `PUSH 0 (full-mode); CALL close-streams helper; JMP (tail-call) __purgefp`
- notes: Watcom __full_io_exit: closes all streams (mode 0) then tail-calls __purgefp. No direct callers because it's invoked via atexit/exit indirection (exit handler table), not direct CALL.

**Verdict**: **PASS** (all rule conditions satisfied)

### `0004694c` `fcloseall` — score 4.34

- current_name: `crt_helper_4694c`
- body_size: 11
- source_obj: `aa0b826f7c04_ioexit.obj`
- expected: Loop over _iob[] and fclose each open one.
  - body_size_range=(8, 80)

**Observed**:
- callees: `['close_streams_helper@0x46957']`
- callers: `[]`
- key instructions: `PUSH 5 (some flags mask); CALL close-streams helper; RET`
- notes: Body 11 with score 4.34 (lowest cluster). Thin wrapper that calls 0x46957 with arg 5. Standard Watcom fcloseall iterates _iob[] and fcloses each open stream (50+ bytes). No iteration here, no _iob field touch. Per crt_fid_match.md §8.3 doc author independently flagged false positive.

**Verdict**: **PASS** (all rule conditions satisfied)

**Manual override**: `REJECT`
- reason: Body 11 bytes too small for fcloseall's expected _iob[] loop. No iteration, no _iob field access. Generic 'PUSH imm + CALL helper' pattern that hashes weakly (4.34) to fcloseall's signature.

### `00046a80` `unlink` — score 63.02

- current_name: `crt_putc_dos`
- body_size: 36
- source_obj: `c313e1089de3_unlink.obj`
- expected: Delete file. INT 21h AH=41h.
  - body_size_range=(20, 80)
  - int21_ah_any=['0x41']

**Observed**:
- callees: `['_set_errno']`
- key instructions: `MOV EDX = filename; MOV AH=0x41; INT 0x21 (DOS delete file); RCL/ROR EAX,1 to expose CF as sign bit; if error -> CALL _set_errno(dos_err); else return 0`
- notes: Pure DOS unlink: INT 21h AH=0x41 unlinks file at DS:DX. Body 36. Ghidra current_name 'crt_putc_dos' is incorrect — this is a file-deletion DOS call. matched_name 'unlink' is correct.

**Verdict**: **PASS** (all rule conditions satisfied)

### `00046bea` `toupper` — score 12.68

- current_name: `toupper`
- body_size: 21
- source_obj: `2f7b4656e340_toupper.obj`
- expected: ASCII fold a-z to A-Z.
  - body_size_range=(15, 35)
  - is_leaf=True

**Observed**:
- callees: `[]`
- callers: `['crt_parse_int_with_base', 'crt_strupr']`
- key instructions: `If 'a' (0x61) <= c <= 'z' (0x7a), SUB EAX,0x20 (fold to upper); else passthrough`
- notes: Standard ASCII toupper. Both callers (crt_parse_int_with_base, crt_strupr) are CRT-pathway uses of toupper for case folding. body 21 fits.

**Verdict**: **PASS** (all rule conditions satisfied)

### `00047328` `gmtime` — score 20.34

- current_name: `crt_helper_47328`
- body_size: 21
- source_obj: `3c31341c06a5_gmtime.obj`
- expected: Thin wrapper: _gmtime(time, &static_buf).
  - body_size_range=(15, 35)
  - callees_required_any=['_gmtime']

**Observed**:
- callees: `['_gmtime']`
- callers: `[]`
- key instructions: `PUSH 0x54604 (static struct tm*); PUSH time arg; CALL _gmtime; thin wrapper`
- notes: Watcom gmtime(t) is a 2-arg wrapper around _gmtime(t, &static_tm). No callers in FD2 — game does not use UTC time. Static buffer at 0x54604 in DGROUP.

**Verdict**: **PASS** (all rule conditions satisfied)

### `0004962e` `strncmp` — score 28.68

- current_name: `strncmp`
- body_size: 50
- source_obj: `1109b8612cd1_strncmp.obj`
- expected: Compare at most n bytes.
  - body_size_range=(40, 80)
  - is_leaf=True

**Observed**:
- callees: `[]`
- callers: `['AIL_internal_init_sequence_inner', 'AIL_internal_xmidi_find_chunk']`
- key instructions: `3 args (str1, str2, n); if n==0 return 0; loop char compare; on diff return signed difference; on NUL or n exhausted return 0`
- notes: Standard strncmp. Both callers (AIL_internal_init_sequence_inner, AIL_internal_xmidi_find_chunk) are AIL audio-engine paths comparing magic bytes / chunk identifiers in MIDI files. body 50 fits.

**Verdict**: **PASS** (all rule conditions satisfied)

### `00049d98` `__int7` — score 15.35

- current_name: `crt_softfp_signal_49d98`
- body_size: 28
- source_obj: `fb5b3f690802_emu387.obj`
- expected: x87 emulator INT 7 dispatcher (FPU-not-present trap).
  - body_size_range=(15, 60)

**Observed**:
- callees: `['softfp_dispatch@0x49db4']`
- callers: `[]`
- key instructions: `STI; reserve frame; PUSHAD + segment regs; fix saved EAX; CALL dispatcher; restore; IRETD`
- notes: x87 emulator INT 7 (FPU device-not-available) handler. Standard Watcom EMU387.LIB structure. No direct callers — invoked via DOS interrupt vector (int 7), set up at startup by __hook387. IRETD epilogue is conclusive.

**Verdict**: **PASS** (all rule conditions satisfied)

### `0004cbd0` `__init_80x87` — score 25.34

- current_name: `crt_helper_4cbd0`
- body_size: 45
- source_obj: `ccc721b858c5_init8087.obj`
- expected: Set FPU control word.
  - body_size_range=(20, 80)

**Observed**:
- callees: `[]`
- callers: `['__init_8087']`
- key instructions: `FINIT; FLD1/FLDZ/FDIVP (1.0/0.0 -> +Inf); FLD ST0/FCHS/FCOMPP (compare +Inf vs -Inf for FPU type detection); FSTSW + SAHF; return AL = 2 or 3 (FPU type code); FINIT + FLDCW (restore control word)`
- notes: Watcom __init_80x87: FPU detection probe. Generates +Inf and -Inf via 1.0/0.0 and FCHS, then FCOMPP detects whether FPU treats them as equal (8087 quirk) or distinct (287/387). Returns FPU type code. Caller is __init_8087 — exact init chain position. body 45 fits.

**Verdict**: **PASS** (all rule conditions satisfied)

### `0004d8ea` `__nmemneed` — score 5.00

- current_name: `crt_helper_4d8ea`
- body_size: 7
- source_obj: `e82fe0e54cb7_nmemneed.obj`
- expected: Default OOM callback; weak stub returning 0. CLIB3S has multiple byte-identical 7-byte XOR-RET stubs (signal default, etc.) that hash to the same family; require a heap-pathway caller to discriminate.
  - body_size_range=(3, 15)
  - is_leaf=True

**Observed**:
- callees: `[]`
- callers: `[]`
- key instructions: `PUSH EBP / MOV EBP,ESP / XOR EAX,EAX / POP EBP / RET — return 0`
- notes: Body byte-identical to 0x3d6f2 (the real __nmemneed). FidDb hash family contains every CLIB3S 7-byte XOR-RET stub (signal default, nmemneed default, etc.) since they're literally the same bytes. Caller analysis is the discriminator: 0x3d6f2 is called directly by `_nmalloc` (heap path); 0x4d8ea has no direct caller — only an UNCONDITIONAL_JUMP from `0x4d340` which is itself called by `crt_signal_handler_print` (signal subsystem path). This entry is a different default-zero stub in the signal subsystem, not __nmemneed.

**Verdict**: **FAIL**
- none of expected callers ['_nmalloc', '__MemAllocator'] found (observed: [])

**Manual override**: `REJECT`
- reason: Caller analysis: 0x4d8ea reached only via JMP from 0x4d340 -> crt_signal_handler_print (signal subsystem), not from _nmalloc / heap allocator path. Real __nmemneed (0x3d6f2) IS called by _nmalloc. Same byte pattern, different role. Renamed in Ghidra to noop_stub_4d8ea_zero to match its sibling thunk noop_stub_4d340_zero.
