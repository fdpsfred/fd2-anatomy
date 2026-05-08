# FD2.LE Watcom 9.5a CRT verification — rejected matches

Entries below were FidQuery candidates that failed behavior verification (score < auto threshold + observed assembly disagrees with the Watcom CRT symbol's expected operation) and are therefore **excluded** from `rebuild_info/crt_lookup_9.5a.json`. Each entry records the FidDb-claimed name, the observed behavior, and the reason rejection was warranted.

Total rejected: **3** of 131 input candidates.

## `000353cc` — claimed `fgetchar` (score 3.00, body 14)

- current_name: `delay_400ms_via_idle_thunk`
- source_obj: `df7f6053275e_fgetchar.obj`
- observed key instructions: PUSH 0x190 (=400); CALL 0x375b2 (a delay function, NOT fgetc); ADD ESP,0x4; RET

**Notes**: Game-side delay(400ms) wrapper for cinematic chapter portrait flash effect. Single caller is a game cinematic function — no CRT pathway. Watcom fgetchar would be `return fgetc(stdin)` and would be called from user main loop or other stdio.

**Reason**: Calls a delay function with arg 400ms; no FILE/_iob touch, no fgetc call. Caller is game cinematic logic, not stdio. Behavior fundamentally inconsistent with matched_name.

## `000462d1` — claimed `__EINVAL` (score 4.34, body 17)

- current_name: `crt_helper_462d1`
- source_obj: `6e73902af0df_dosret.obj`
- observed key instructions: CALL __get_errno_ptr; STORE 0x9 (= EBADF in Watcom errno.h); return -1

**Notes**: This is the __EBADF helper, NOT __EINVAL. Watcom errno.h defines EBADF=9 and EINVAL=22 (0x16). FidDb labelled it __EINVAL because the family of set-errno helpers in dosret.obj all hash to identical bytes (the immediate is masked).

**Reason**: Function sets errno=9 (EBADF), but matched_name is __EINVAL (would store 0x16). Behavior matches __EBADF in the same dosret.obj family. Mislabeled by FidDb due to imm32-masking.

## `0004d8ea` — claimed `__nmemneed` (score 5.00, body 7)

- current_name: `crt_helper_4d8ea`
- source_obj: `e82fe0e54cb7_nmemneed.obj`
- observed key instructions: PUSH EBP / MOV EBP,ESP / XOR EAX,EAX / POP EBP / RET — return 0

**Notes**: Body byte-identical to 0x3d6f2 (the real __nmemneed). FidDb hash family contains every CLIB3S 7-byte XOR-RET stub (signal default, nmemneed default, etc.) since they're literally the same bytes. Caller analysis is the discriminator: 0x3d6f2 is called directly by `_nmalloc` (heap path); 0x4d8ea has no direct caller — only an UNCONDITIONAL_JUMP from `0x4d340` which is itself called by `crt_signal_handler_print` (signal subsystem path). This entry is a different default-zero stub in the signal subsystem, not __nmemneed.

**Reason**: Caller analysis: 0x4d8ea reached only via JMP from 0x4d340 -> crt_signal_handler_print (signal subsystem), not from _nmalloc / heap allocator path. Real __nmemneed (0x3d6f2) IS called by _nmalloc. Same byte pattern, different role. Renamed in Ghidra to noop_stub_4d8ea_zero to match its sibling thunk noop_stub_4d340_zero.
