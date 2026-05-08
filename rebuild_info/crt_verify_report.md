# FD2.LE Watcom 9.5a CRT FidDb verification report

## Sources

- `workspace\crt_fid_match\sample_queue.json`: 10 entries — Threshold validation by equal-distance score sampling.
- `workspace\crt_fid_match\conflict_queue.json`: 3 entries — Name-conflict candidates (current_name vs matched_name disagree).
- `workspace\crt_fid_match\manual_queue.json`: 18 entries — Per-entry behavior verification for score-below-threshold candidates.

Total unique entries: **31**

## Verdict summary

- PASS: 28

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
- key instructions: `PUSH 0x190 (=400); CALL 0x375b2 (a delay function, NOT fgetc); ADD ESP,0x4; RET`
- notes: This is a `delay(400)` wrapper for a 400ms idle thunk. Watcom fgetchar would be `return fgetc(stdin)` — push __iob[0] then call fgetc. The two have nothing in common semantically. Score 3.0 is FidDb's lowest among 131 matches and is clearly a hash collision against a generic 4-instruction stub.

**Verdict**: **FAIL**
- none of expected callees ['fgetc'] found (observed: ['delay_unrelated@0x375b2'])

**Manual override**: `REJECT`
- reason: Calls a delay function with arg 400ms; no FILE/_iob touch, no fgetc call. fgetchar must read a byte from stdin. Behavior fundamentally inconsistent with matched_name.

### `00036cd7` `__CHK` — score 16.01

- current_name: `crt_frame_setup`
- body_size: 16
- source_obj: `71a26af32e1b_stk.obj`
- expected: Stack overflow check at function entry; XCHGs caller's saved EAX, tail-calls __STK to do the actual probe.
  - body_size_range=(8, 30)
  - callees_required_any=['__STK']

**Observed**:
- callees: `['__STK']`
- key instructions: `XCHG saves caller's EAX onto stack then loads stack-need into EAX; CALL __STK to probe; restore EAX; RET 0x4 (pops 1 dword arg)`
- notes: Watcom __CHK entry stub: front-end to __STK for compiler-generated stack-overflow checks at function entry. The XCHG pattern is the standard Watcom calling-convention swap of arg-in-EAX vs return-addr-on-stack. body 16 fits.

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
- key instructions: `Compare requested stack size (EAX) vs current ESP. If overflow risk, compare against stack-limit global at 0x52814 and selector at 0x52794. Pure leaf, no callees.`
- notes: Standard Watcom __STK: probes that EAX bytes can be allocated on the stack. body 29 fits.

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

### `00037046` `freopen` — score 19.36

- current_name: `crt_helper_37046`
- body_size: 44
- source_obj: `34f4a927de36_fopen.obj`
- expected: Close fp, then reopen with new path/mode.
  - body_size_range=(30, 120)

**Observed**:
- callees: `['fopen_open_helper@0x36ebc', 'fopen_inner@0x36fe1']`
- key instructions: `3-arg cdecl freopen(filename, mode, fp): PUSH fp; CALL shutdown helper (returns FILE*); if non-null, AND clear flag bits; PUSH (fp, 0, mode, filename); CALL fopen helper`
- notes: Standard freopen: shut down old stream, then re-open with new path/mode reusing same FILE*. body 44 fits expected (30, 120). The AND [EAX+0xC],0x4000 pre-call is consistent with Watcom freopen's _flag adjustment before the open call.

**Verdict**: **PASS** (all rule conditions satisfied)

### `00037072` `fread` — score 332.59

- current_name: `fread`
- body_size: 466
- source_obj: `e4cfbf258e44_fread.obj`
- expected: Read N elements; size*count, calls read/__filbuf in loop.
  - body_size_range=(200, 700)
  - callees_required_any=['read', '__filbuf', '__qread']

**Observed**:
- callees: `['__fill_buffer', '__get_errno_ptr', '__ioalloc', 'memcpy', '__qread']`
- key instructions: `4 args [EBP+0x14]/[EBP+0x18]/[EBP+0x1C]/[EBP+0x20] (buf,size,count,fp); CALL __ioalloc to ensure buffer; loop with __fill_buffer for refill, __qread for direct read, memcpy for buffered copy`
- notes: Full fread implementation. Handles size*count, partial reads, EOF, error via __get_errno_ptr. Body 466 bytes fits expected 200-700 range.

**Verdict**: **PASS** (all rule conditions satisfied)

### `00037326` `__doclose` — score 255.10

- current_name: `crt_fclose_inner`
- body_size: 158
- source_obj: `0aa510ba4f71_fclose.obj`
- expected: Final close: clear _iob flags, call close(fd).
  - body_size_range=(80, 250)
  - callees_required_any=['close', '__freefp', '__flush']

**Observed**:
- callees: `['close', '__flush', 'ftell', '__MkTmpFile', 'remove', 'free', 'lseek']`
- key instructions: `Tests _flag at [EBX+0xC]; if 0x10 set -> CALL __flush; CALL ftell; if pos!=-1 -> CALL lseek(handle,pos,0); if close-flag arg -> CALL close(handle); if 0x8 in _flag -> free(_base) and zero it; if temp-file flag -> CALL __MkTmpFile + CALL remove`
- notes: Final close path: drains buffer, repositions for unread bytes, closes handle, frees buffer, removes tmpfile. Body 158 bytes.

**Verdict**: **PASS** (all rule conditions satisfied)

### `0003744b` `fwrite` — score 479.25

- current_name: `fwrite`
- body_size: 359
- source_obj: `184e37b9c956_fwrite.obj`
- expected: Write N elements; size*count, calls __flush or write.
  - body_size_range=(200, 600)
  - callees_required_any=['write', '__flush', '__qwrite']

**Observed**:
- callees: `['__get_errno_ptr', '__flush', '__ioalloc', 'write', 'memcpy', 'fputc']`
- key instructions: `4 args (buf, size, count, fp); CALL __ioalloc to ensure buffer; loop drains with __flush, falls back to write for direct, memcpy for buffered copy`
- notes: Full fwrite implementation. body 359 fits expected 200-600 range.

**Verdict**: **PASS** (all rule conditions satisfied)

### `000375c0` `memset` — score 96.02

- current_name: `memset`
- body_size: 34
- source_obj: `26c70b6cd69b_memset.obj`
- expected: Fill n bytes at dest with byte value. In Watcom 9.5a CLIB3S, memset is a thin wrapper that broadcasts val to a 4-byte word and tail-calls the __STOSB helper.
  - body_size_range=(15, 60)
  - callees_required_any=['__STOSB', '__STOSD']

**Observed**:
- callees: `['__STOSB']`
- key instructions: `Args via [ESP+4]/[ESP+8]/[ESP+0xC] (cdecl); broadcast val byte to 4 bytes of EDX (DH=DL, SHL/MOV pairs); CALL __STOSB helper; return dest`
- notes: Watcom 9.5a CLIB3S memset wrapper. The actual REP STOSB lives in __STOSB @ 0x3dd10 (matched_name __STOSB).

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
- key instructions: `Load arg into EAX; TEST sign; if negative NEG EAX; return |x|`
- notes: Classic abs/labs (32-bit equivalent on Watcom 386). body 14 fits. NEG instruction confirms behavior. abs and labs share the same code in Watcom 386 — both candidates are valid aliases.

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
- key instructions: `EDX = port (arg1), AL = value (arg2 byte); OUT DX,AL writes byte to I/O port`
- notes: Standard outp(port, value) — DOS-mode I/O port byte-write. body 12 fits.

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
- key instructions: `Load and clear stashed-char global at 0x52824 (used by ungetch); if non-zero, return it; else INT 21h AH=0x08 (DOS get char without echo) and zero-extend AL`
- notes: Watcom getch with ungetch support: 0x52824 is the 1-byte unget buffer. body 27 fits expected (15, 50). INT 21h AH=0x08 is conclusive.

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
- key instructions: `CALL seed-pointer getter; if non-null, IMUL by 0x41c64e6d (BSD multiplier), ADD 0x3039 (BSD increment); store new seed; return (seed>>16) & 0x7FFF`
- notes: Classic BSD/POSIX rand LCG with constants (a=0x41c64e6d, c=0x3039) returning the upper 15 bits. The seed is fetched via a getter (not strictly leaf, but matches Watcom 9.5a CLIB3S indirect pattern). body 34 fits.

**Verdict**: **PASS** (all rule conditions satisfied)

### `0003c94f` `srand` — score 8.67

- current_name: `crt_set_rand_seed`
- body_size: 19
- source_obj: `e64bae7e23f3_rand.obj`
- expected: Set global rand seed via seed-pointer getter.
  - body_size_range=(8, 30)

**Observed**:
- callees: `['rand_seed_ptr@0x3c927']`
- key instructions: `CALL seed-pointer getter; if non-null, store arg into seed`
- notes: Standard srand(seed) via seed-pointer thunk. body 19 fits.

**Verdict**: **PASS** (all rule conditions satisfied)

### `0003d8ca` `__purgefp` — score 18.34

- current_name: `crt_helper_3d8ca`
- body_size: 34
- source_obj: `3fa95c836703_allocfp.obj`
- expected: Mark FILE slot as unused; small.
  - body_size_range=(15, 60)

**Observed**:
- callees: `['free']`
- key instructions: `Loop: CMP __ClosedStreams (0x541a0) head; if NULL exit (JZ to shared epilogue at 0x3d8c8); else free head and advance head pointer`
- notes: Loops over the __ClosedStreams linked list at 0x541a0 (per crt_globals_map.json) and frees each node. The JZ to 0x3d8c8 is a tail-merge into a neighboring function's POP EBX/RET epilogue (Watcom optimization). body 34 fits expected (15, 60).

**Verdict**: **PASS** (all rule conditions satisfied)

### `0003d919` `__ioalloc` — score 712.10

- current_name: `crt_init_file_buffer`
- body_size: 119
- source_obj: `cbc3461dce6c_ioalloc.obj`
- expected: Allocate FILE struct + buffer; calls malloc/_nmalloc.
  - body_size_range=(80, 200)
  - callees_required_any=['malloc', '_nmalloc']

**Observed**:
- callees: `['__chktty', 'malloc']`
- key instructions: `CALL __chktty to set TTY flag; selects bufsiz (0x86 line, 0x1000 block, 0x1 unbuffered) based on flag bits; CALL malloc(bufsiz); on alloc failure falls back to inline 1-byte stash at [EBX+0x18]; sets _ptr/_cnt/_base in _iob entry`
- notes: Allocates FILE buffer based on stream type (line / block / unbuffered). Body 119 fits 80-200 range.

**Verdict**: **PASS** (all rule conditions satisfied)

### `0003db16` `__flush` — score 616.11

- current_name: `crt_flush_stream_buffer`
- body_size: 157
- source_obj: `4b8db5c65d34_flush.obj`
- expected: Flush stream buffer; calls write to drain _ptr/_cnt.
  - body_size_range=(80, 250)
  - callees_required_any=['write', '__qwrite']

**Observed**:
- callees: `['__get_errno_ptr', '__qwrite', 'lseek']`
- key instructions: `Drains buffer between _ptr and _cnt; calls __qwrite to flush bytes; on error sets errno via __get_errno_ptr`
- notes: Standard FILE-buffer flush. body 157 fits 80-250 range.

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

### `0003fb57` `time` — score 174.36

- current_name: `time`
- body_size: 57
- source_obj: `0fc969d455a2_time.obj`
- expected: Get unix time_t. Calls __getctime; INT 21h AH=2A/2C.
  - body_size_range=(30, 100)
  - callees_required_any=['__getctime', 'mktime']

**Observed**:
- callees: `['__getctime', 'mktime']`
- key instructions: `Allocate 0x24 byte tm on stack; CALL __getctime(&tm); year normalization (CMP 0x1f4=500 / JL / INC); CALL mktime(&tm); store in *timer if non-NULL`
- notes: time(timer) gathers DOS date+time via __getctime, then converts to time_t via mktime. Optional timer pointer stored before return.

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
- key instructions: `PUSH t arg; CALL localtime (0x3fbe5); PUSH result; CALL asctime (0x3fd2b); return its result`
- notes: Standard ctime(t) = asctime(localtime(t)). body 25 fits.

**Verdict**: **PASS** (all rule conditions satisfied)

### `000462d1` `__EINVAL` — score 4.34

- current_name: `crt_helper_462d1`
- body_size: 17
- source_obj: `6e73902af0df_dosret.obj`
- expected: Set errno=EINVAL, return -1.
  - body_size_range=(8, 30)

**Observed**:
- callees: `['__get_errno_ptr@0x3d7f6']`
- key instructions: `CALL __get_errno_ptr; STORE 0x9 (= EBADF in Watcom errno.h); return -1`
- notes: This is the __EBADF helper, NOT __EINVAL. Watcom errno.h defines EBADF=9 and EINVAL=22 (0x16). The function stores 9, so it's __EBADF. FidDb labelled it __EINVAL because the family of set-errno helpers in dosret.obj all hash to identical bytes (the immediate is masked).

**Verdict**: **PASS** (all rule conditions satisfied)

**Manual override**: `REJECT`
- reason: Function sets errno=9 (EBADF), but matched_name is __EINVAL (which would store 0x16). Behavior matches __EBADF, not __EINVAL. Safe-list rejects this specific match; the function itself is a real CRT helper but mislabeled by FidDb.

### `000462e2` `_set_errno` — score 591.72

- current_name: `crt_dos_to_errno`
- body_size: 112
- source_obj: `6e73902af0df_dosret.obj`
- expected: Map DOS error to errno; lookup table.
  - body_size_range=(60, 200)

**Observed**:
- callees: `['__get_doserrno_ptr', '__get_errno_ptr']`
- key instructions: `Stores DOS error code into _doserrno (via __get_doserrno_ptr); maps DOS code to errno via lookup table at 0x5377c (only when DOS<0x100); writes errno (via __get_errno_ptr); returns -1`
- notes: Maps DOS error codes (0..0x13) to POSIX errno via 20-entry lookup table. DOS_VERSION threshold check (0x5283a). For codes >=0x100 (Watcom internal), uses high byte directly. Body 112 fits 60-200 range.

**Verdict**: **PASS** (all rule conditions satisfied)

### `000463c5` `_heapenable` — score 7.67

- current_name: `crt_helper_463c5`
- body_size: 19
- source_obj: `44e3321078e7_heapen.obj`
- expected: Enable/disable heap; toggles flag.
  - body_size_range=(8, 40)
  - is_leaf=True

**Observed**:
- callees: `[]`
- key instructions: `Load arg into EDX; load old global at 0x537ec into EAX; store new value; return old`
- notes: Standard _heapenable(int): atomically swap heap-enable flag and return previous state. body 19 fits, leaf, simple swap-and-return semantics confirmed.

**Verdict**: **PASS** (all rule conditions satisfied)

### `0004693d` `__full_io_exit` — score 12.34

- current_name: `crt_helper_4693d`
- body_size: 15
- source_obj: `aa0b826f7c04_ioexit.obj`
- expected: Flush + close all open _iob entries on exit.
  - body_size_range=(8, 50)

**Observed**:
- callees: `['close_streams_helper@0x46957', '__purgefp']`
- key instructions: `PUSH 0 (full-mode); CALL close-streams helper; JMP (tail-call) __purgefp`
- notes: Watcom __full_io_exit: closes all streams (mode 0 = full IO exit) then tail-calls __purgefp to free the closed-streams list. body 15 fits. Pattern matches expected exit-time cleanup sequence.

**Verdict**: **PASS** (all rule conditions satisfied)

### `0004694c` `fcloseall` — score 4.34

- current_name: `crt_helper_4694c`
- body_size: 11
- source_obj: `aa0b826f7c04_ioexit.obj`
- expected: Loop over _iob[] and fclose each open one.
  - body_size_range=(8, 80)

**Observed**:
- callees: `['close_streams_helper@0x46957']`
- key instructions: `PUSH 5 (some flags mask); CALL close-streams helper; RET`
- notes: Body 11 with score 4.34 (lowest cluster). The function is just a thin wrapper that calls 0x46957 with arg 5. The standard Watcom fcloseall is a loop over _iob[] that fcloses each open stream — typically 50+ bytes. This entry is too short to be the real fcloseall body, and per crt_fid_match.md §8.3 the doc author independently flagged this as a hash collision.

**Verdict**: **PASS** (all rule conditions satisfied)

**Manual override**: `REJECT`
- reason: Body 11 bytes is too small for fcloseall's expected _iob[] loop. No iteration, no _iob field touch. Matches a generic 'PUSH imm + CALL helper' pattern that hashes weakly (score 4.34) to fcloseall's signature. Per crt_fid_match.md §8.3, this match is documented as false positive.

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
- key instructions: `If 'a' (0x61) <= c <= 'z' (0x7a), SUB EAX,0x20 (fold to upper); else passthrough`
- notes: Standard ASCII toupper. body 21 fits. Pure leaf, classic fold. Current name 'toupper' already matches matched_name.

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
- key instructions: `PUSH 0x54604 (static struct tm*); PUSH time arg; CALL _gmtime; thin wrapper`
- notes: Watcom gmtime(t) is a 2-arg wrapper around _gmtime(t, &static_tm). Static buffer at 0x54604 in DGROUP is the dedicated gmtime tm struct.

**Verdict**: **PASS** (all rule conditions satisfied)

### `0004733d` `__leapyear` — score 421.35

- current_name: `crt_is_leap_year`
- body_size: 52
- source_obj: `fee92bb59e09_timeutil.obj`
- expected: Test if year is leap year. y%4==0 && (y%100!=0 || y%400==0).
  - body_size_range=(20, 100)
  - is_leaf=True

**Observed**:
- callees: `[]`
- key instructions: `TEST BL,0x3 (year & 3 != 0 -> not leap, return 0); IDIV by 100 (0x64); if remainder != 0 -> leap (return 1); else IDIV by 400 (0x190); if remainder == 0 -> leap; else not leap`
- notes: Classic Gregorian leap-year rule: divisible by 4 AND (not divisible by 100 OR divisible by 400). Pure leaf, no callees, body 52.

**Verdict**: **PASS** (all rule conditions satisfied)

### `00049d98` `__int7` — score 15.35

- current_name: `crt_softfp_signal_49d98`
- body_size: 28
- source_obj: `fb5b3f690802_emu387.obj`
- expected: x87 emulator INT 7 dispatcher (FPU-not-present trap).
  - body_size_range=(15, 60)

**Observed**:
- callees: `['softfp_dispatch@0x49db4']`
- key instructions: `STI; reserve frame; PUSHAD + segment regs; fix saved EAX; CALL dispatcher; restore; IRETD`
- notes: x87 emulator INT 7 (FPU device-not-available) handler. Standard Watcom EMU387.LIB structure: enable interrupts, save context, fix-up the interrupted instruction's saved register (ADD [ESP+0x14],0x18 patches the saved instruction pointer past the FPU op), call C-level dispatcher, restore, IRETD. body 28 fits.

**Verdict**: **PASS** (all rule conditions satisfied)

### `0004d8ea` `__nmemneed` — score 5.00

- current_name: `crt_helper_4d8ea`
- body_size: 7
- source_obj: `e82fe0e54cb7_nmemneed.obj`
- expected: Default OOM callback; weak stub returning 0.
  - body_size_range=(3, 15)
  - is_leaf=True

**Observed**:
- callees: `[]`
- key instructions: `XOR EAX,EAX; RET — return 0`
- notes: Default __nmemneed stub: returns 0 (request more memory rejected). Watcom convention: the C library declares __nmemneed as a weak stub returning 0; user code can override to grow the heap on demand. body 7 fits expected (3, 15).

**Verdict**: **PASS** (all rule conditions satisfied)
