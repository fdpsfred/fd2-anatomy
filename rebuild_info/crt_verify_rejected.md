# FD2.LE Watcom 9.5a CRT verification — rejected matches

Entries below were FidQuery candidates that failed behavior verification (score < auto threshold + observed assembly disagrees with the Watcom CRT symbol's expected operation) and are therefore **excluded** from `rebuild_info/crt_lookup_9.5a.json`. Each entry records the FidDb-claimed name, the observed behavior, and the reason rejection was warranted.

Total rejected: **3** of 131 input candidates.

## `000353cc` — claimed `fgetchar` (score 3.00, body 14)

- current_name: `delay_400ms_via_idle_thunk`
- source_obj: `df7f6053275e_fgetchar.obj`
- observed key instructions: PUSH 0x190 (=400); CALL 0x375b2 (a delay function, NOT fgetc); ADD ESP,0x4; RET

**Notes**: This is a `delay(400)` wrapper for a 400ms idle thunk. Watcom fgetchar would be `return fgetc(stdin)` — push __iob[0] then call fgetc. The two have nothing in common semantically. Score 3.0 is FidDb's lowest among 131 matches and is clearly a hash collision against a generic 4-instruction stub.

**Reason**: Calls a delay function with arg 400ms; no FILE/_iob touch, no fgetc call. fgetchar must read a byte from stdin. Behavior fundamentally inconsistent with matched_name.

## `000462d1` — claimed `__EINVAL` (score 4.34, body 17)

- current_name: `crt_helper_462d1`
- source_obj: `6e73902af0df_dosret.obj`
- observed key instructions: CALL __get_errno_ptr; STORE 0x9 (= EBADF in Watcom errno.h); return -1

**Notes**: This is the __EBADF helper, NOT __EINVAL. Watcom errno.h defines EBADF=9 and EINVAL=22 (0x16). The function stores 9, so it's __EBADF. FidDb labelled it __EINVAL because the family of set-errno helpers in dosret.obj all hash to identical bytes (the immediate is masked).

**Reason**: Function sets errno=9 (EBADF), but matched_name is __EINVAL (which would store 0x16). Behavior matches __EBADF, not __EINVAL. Safe-list rejects this specific match; the function itself is a real CRT helper but mislabeled by FidDb.

## `0004694c` — claimed `fcloseall` (score 4.34, body 11)

- current_name: `crt_helper_4694c`
- source_obj: `aa0b826f7c04_ioexit.obj`
- observed key instructions: PUSH 5 (some flags mask); CALL close-streams helper; RET

**Notes**: Body 11 with score 4.34 (lowest cluster). The function is just a thin wrapper that calls 0x46957 with arg 5. The standard Watcom fcloseall is a loop over _iob[] that fcloses each open stream — typically 50+ bytes. This entry is too short to be the real fcloseall body, and per crt_fid_match.md §8.3 the doc author independently flagged this as a hash collision.

**Reason**: Body 11 bytes is too small for fcloseall's expected _iob[] loop. No iteration, no _iob field touch. Matches a generic 'PUSH imm + CALL helper' pattern that hashes weakly (score 4.34) to fcloseall's signature. Per crt_fid_match.md §8.3, this match is documented as false positive.
