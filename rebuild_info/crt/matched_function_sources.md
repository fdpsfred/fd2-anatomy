# Matched Function Sources

Each row maps a FD2.LE function address to the Watcom 9.5 lib that
contains the matching obj. Generated from `rebuild_info/crt/lookup_9.5a.json` (read field `source_libs`).

Total entries: **192**

| FD2 addr | lib symbol | body | verified | source obj | source lib(s) / versions |
|---|---|---:|---|---|---|
| `0x00036cd0` | `L$1_stk_save_ss` | 7 | manual | `71a26af32e1b_stk.obj` | CLIB3S:9.5+9.5a+9.5b+9.5c |
| `0x00036cd7` | `__CHK` | 16 | manual | `71a26af32e1b_stk.obj` | CLIB3S:9.5+9.5a+9.5b+9.5c |
| `0x00036ce7` | `__GRO` | 3 | manual | `71a26af32e1b_stk.obj` | CLIB3S:9.5+9.5a+9.5b+9.5c |
| `0x00036cea` | `__STK` | 29 | manual | `71a26af32e1b_stk.obj` | CLIB3S:9.5+9.5a+9.5b+9.5c |
| `0x00036d07` | `__STKOVERFLOW` | 15 | manual | `71a26af32e1b_stk.obj` | CLIB3S:9.5+9.5a+9.5b+9.5c |
| `0x00036d16` | `malloc` | 16 | auto_threshold | `9afea117227c_nmalloc.obj` | CLIB3S:9.5+9.5a+9.5b |
| `0x00036d26` | `_nmalloc` | 114 | auto_threshold | `9afea117227c_nmalloc.obj` | CLIB3S:9.5+9.5a+9.5b |
| `0x00036d98` | `int386` | 41 | auto_threshold | `d85276984e7d_int386.obj` | CLIB3S:9.5+9.5a+9.5b+9.5c |
| `0x00036dc1` | `printf` | 34 | conflict_resolved | `23990787aee2_printf.obj` | CLIB3S:9.5+9.5a+9.5b+9.5c |
| `0x00036de4` | `exit` | 18 | manual | `93ce9e3ff4ac_exit.obj` | CLIB3S:9.5+9.5a |
| `0x00036df6` | `_exit` | 23 | manual | `93ce9e3ff4ac_exit.obj` | CLIB3S:9.5+9.5a |
| `0x00036e0d` | `__open_flags` | 404 | auto_threshold | `34f4a927de36_fopen.obj` | CLIB3S:9.5+9.5a+9.5b+9.5c |
| `0x00036fa1` | `_fsopen` | 43 | auto_threshold | `34f4a927de36_fopen.obj` | CLIB3S:9.5+9.5a+9.5b+9.5c |
| `0x00036fcc` | `fopen` | 122 | manual | `34f4a927de36_fopen.obj` | CLIB3S:9.5+9.5a+9.5b+9.5c |
| `0x00037046` | `freopen` | 44 | manual | `34f4a927de36_fopen.obj` | CLIB3S:9.5+9.5a+9.5b+9.5c |
| `0x00037072` | `fread` | 466 | auto_threshold | `e4cfbf258e44_fread.obj` | CLIB3S:9.5a |
| `0x00037244` | `fclose` | 44 | auto_threshold | `0aa510ba4f71_fclose.obj` | CLIB3S:9.5+9.5a+9.5b |
| `0x00037270` | `__shutdown_stream` | 55 | auto_threshold | `0aa510ba4f71_fclose.obj` | CLIB3S:9.5+9.5a+9.5b |
| `0x000372a7` | `__MkTmpFile` | 127 | auto_threshold | `0aa510ba4f71_fclose.obj` | CLIB3S:9.5+9.5a+9.5b |
| `0x00037326` | `__doclose` | 158 | auto_threshold | `0aa510ba4f71_fclose.obj` | CLIB3S:9.5+9.5a+9.5b |
| `0x000373c4` | `memmove` | 82 | auto_threshold | `1481a08b40ca_memmove.obj` | CLIB3S:9.5+9.5a+9.5b+9.5c |
| `0x00037416` | `free` | 16 | auto_threshold | `01d4e7789167_nfree.obj` | CLIB3S:9.5+9.5a+9.5b+9.5c |
| `0x00037426` | `_nfree` | 37 | auto_threshold | `01d4e7789167_nfree.obj` | CLIB3S:9.5+9.5a+9.5b+9.5c |
| `0x0003744b` | `fwrite` | 359 | auto_threshold | `184e37b9c956_fwrite.obj` | CLIB3S:9.5a+9.5b+9.5c |
| `0x000375c0` | `memset` | 34 | auto_threshold | `26c70b6cd69b_memset.obj` | CLIB3S:9.5+9.5a+9.5b+9.5c |
| `0x000375e2` | `abs` | 14 | manual | `a46b440ab99c_abs.obj` | CLIB3S:9.5+9.5a+9.5b+9.5c |
| `0x000375f0` | `fseek` | 421 | auto_threshold | `1ce0e32395b8_fseek.obj` | CLIB3S:9.5+9.5a |
| `0x00037795` | `outp` | 12 | manual | `1d6757f02585_outp.obj` | CLIB3S:9.5+9.5a+9.5b+9.5c |
| `0x000377a4` | `__CHP` | 31 | byte_match | `5a2db35937a4_fchop.obj` | MATH387R+S:9.5+9.5a+9.5b+9.5c |
| `0x000377c3` | `L$1_sprintf_put_char` | 22 | manual | `aade3259b5a5_sprintf.obj` | CLIB3S:9.5+9.5a+9.5b+9.5c |
| `0x000377d9` | `sprintf` | 44 | auto_threshold | `aade3259b5a5_sprintf.obj` | CLIB3S:9.5+9.5a+9.5b+9.5c |
| `0x00037805` | `strlen` | 28 | auto_threshold | `4720124ddae2_strlen.obj` | CLIB3S:9.5+9.5a+9.5b+9.5c |
| `0x00037821` | `getch` | 27 | manual | `5d91ab104c50_getch.obj` | CLIB3S:9.5+9.5a+9.5b+9.5c |
| `0x0003c6fc` | `sqrt` | 58 | byte_match | `aa20ff98fe90_sqrt387.obj` | MATH387S:9.5+9.5a+9.5b+9.5c |
| `0x0003c736` | `IF@SQRT` | 2 | byte_match | `aa20ff98fe90_sqrt387.obj` | MATH387S:9.5+9.5a+9.5b+9.5c |
| `0x0003c738` | `__@DSQRT` | 62 | byte_match | `aa20ff98fe90_sqrt387.obj` | MATH387S:9.5+9.5a+9.5b+9.5c |
| `0x0003c7b6` | `IF@COS` | 25 | byte_match | `b70811dbd1b6_trig387.obj` | MATH387S:9.5+9.5a+9.5b+9.5c |
| `0x0003c7cf` | `IF@SIN` | 182 | byte_match | `b70811dbd1b6_trig387.obj` | MATH387S:9.5+9.5a+9.5b+9.5c |
| `0x0003c8ab` | `IF@TAN` | 105 | byte_match | `b70811dbd1b6_trig387.obj` | MATH387S:9.5+9.5a+9.5b+9.5c |
| `0x0003c914` | `tan` | 19 | byte_match | `b70811dbd1b6_trig387.obj` | MATH387S:9.5+9.5a+9.5b+9.5c |
| `0x0003c927` | `L$1_rand_seed_ptr` | 6 | manual | `e64bae7e23f3_rand.obj` | CLIB3S:9.5+9.5a+9.5b+9.5c |
| `0x0003c92d` | `rand` | 34 | manual | `e64bae7e23f3_rand.obj` | CLIB3S:9.5+9.5a+9.5b+9.5c |
| `0x0003c94f` | `srand` | 19 | manual | `e64bae7e23f3_rand.obj` | CLIB3S:9.5+9.5a+9.5b+9.5c |
| `0x0003cb91` | `__exit` | 2 | byte_match | `ec67ee7a9d15_cstart.obj` | CLIB3S:9.5+9.5a |
| `0x0003cb93` | `__exit_with_msg` | 49 | byte_match | `ec67ee7a9d15_cstart.obj` | CLIB3S:9.5+9.5a |
| `0x0003cbc4` | `__GETDS` | 8 | byte_match | `73721e07b4f4_cstart.obj` | CLIB3S:9.5+9.5a+9.5b+9.5c (also in adiestrt/adifstrt/adsstart variants) |
| `0x0003cbd6` | `memcpy` | 42 | auto_threshold | `59aa526358b1_memcpy.obj` | CLIB3S:9.5+9.5a+9.5b+9.5c |
| `0x0003cc00` | `lseek` | 75 | auto_threshold | `d0b66a0d4814_lseek.obj` | CLIB3S:9.5a+9.5b+9.5c |
| `0x0003cc4b` | `read` | 217 | auto_threshold | `c8c9f304ef5c_read.obj` | CLIB3S:9.5+9.5a+9.5b |
| `0x0003cd24` | `open` | 31 | auto_threshold | `478979bcfaf2_open.obj` | CLIB3S:9.5+9.5a+9.5b+9.5c |
| `0x0003cd43` | `sopen` | 448 | auto_threshold | `478979bcfaf2_open.obj` | CLIB3S:9.5+9.5a+9.5b+9.5c |
| `0x0003cf03` | `__set_binary` | 108 | auto_threshold | `478979bcfaf2_open.obj` | CLIB3S:9.5+9.5a+9.5b+9.5c |
| `0x0003cf70` | `strcmp` | 172 | byte_match | `f8ea754154cb__strcmp.obj` | CLIB3S:9.5+9.5a+9.5b+9.5c |
| `0x0003d01c` | `close` | 58 | auto_threshold | `659aec4b3766_close.obj` | CLIB3S:9.5+9.5a+9.5b |
| `0x0003d056` | `filelength` | 63 | auto_threshold | `29c1008fc4ff_filelen.obj` | CLIB3S:9.5+9.5a+9.5b+9.5c |
| `0x0003d095` | `write` | 473 | auto_threshold | `e73a1f1394c4_write.obj` | CLIB3S:9.5a+9.5b+9.5c |
| `0x0003d270` | `__MemAllocator` | 176 | auto_threshold | `59c35008174c_memalloc.obj` | CLIB3S:9.5+9.5a+9.5b+9.5c |
| `0x0003d320` | `__MemFree` | 267 | auto_threshold | `59c35008174c_memalloc.obj` | CLIB3S:9.5+9.5a+9.5b+9.5c |
| `0x0003d42b` | `__LastFree` | 199 | auto_threshold | `57a37044e08d_grownear.obj` | CLIB3S:9.5a |
| `0x0003d4f2` | `__ExpandDGROUP` | 512 | auto_threshold | `57a37044e08d_grownear.obj` | CLIB3S:9.5a |
| `0x0003d6f2` | `__nmemneed` | 7 | auto_threshold | `e82fe0e54cb7_nmemneed.obj` | CLIB3S:9.5+9.5a+9.5b+9.5c |
| `0x0003d6f9` | `segread` | 45 | auto_threshold | `018a8423f53c_segread.obj` | CLIB3S:9.5+9.5a+9.5b+9.5c |
| `0x0003d726` | `int386x` | 33 | auto_threshold | `45eb6cdde4d8_int386x.obj` | CLIB3S:9.5+9.5a+9.5b+9.5c |
| `0x0003d747` | `L$1_fprtf_fprtf` | 26 | byte_match | `2584dd7771f2_fprtf.obj` | CLIB3S:9.5+9.5a+9.5b+9.5c |
| `0x0003d761` | `__fprtf` | 128 | auto_threshold | `2584dd7771f2_fprtf.obj` | CLIB3S:9.5+9.5a+9.5b+9.5c |
| `0x0003d7e1` | `tolower` | 21 | auto_threshold | `379418752ba8_tolower.obj` | CLIB3S:9.5+9.5a+9.5b+9.5c |
| `0x0003d7f6` | `__get_errno_ptr` | 6 | manual | `046c7fbb59e1_errno.obj` | CLIB3S:9.5+9.5a+9.5b+9.5c |
| `0x0003d7fc` | `__get_doserrno_ptr` | 6 | manual | `046c7fbb59e1_errno.obj` | CLIB3S:9.5+9.5a+9.5b+9.5c |
| `0x0003d802` | `__allocfp` | 149 | auto_threshold | `3fa95c836703_allocfp.obj` | CLIB3S:9.5+9.5a |
| `0x0003d897` | `__freefp` | 51 | auto_threshold | `3fa95c836703_allocfp.obj` | CLIB3S:9.5+9.5a |
| `0x0003d8ca` | `__purgefp` | 34 | manual | `3fa95c836703_allocfp.obj` | CLIB3S:9.5+9.5a |
| `0x0003d8ec` | `__chktty` | 45 | auto_threshold | `6e9aaef6bf71_chktty.obj` | CLIB3S:9.5+9.5a+9.5b+9.5c |
| `0x0003d919` | `__ioalloc` | 119 | auto_threshold | `cbc3461dce6c_ioalloc.obj` | CLIB3S:9.5+9.5a+9.5b+9.5c |
| `0x0003d990` | `__qread` | 49 | auto_threshold | `d2e9bd1cd5ea_qread.obj` | CLIB3S:9.5+9.5a+9.5b+9.5c |
| `0x0003d9c1` | `fgetc` | 121 | auto_threshold | `0b3778e9e319_fgetc.obj` | CLIB3S:9.5+9.5a+9.5b+9.5c |
| `0x0003da3a` | `__filbuf` | 43 | auto_threshold | `0b3778e9e319_fgetc.obj` | CLIB3S:9.5+9.5a+9.5b+9.5c |
| `0x0003da65` | `__fill_buffer` | 171 | auto_threshold | `0b3778e9e319_fgetc.obj` | CLIB3S:9.5+9.5a+9.5b+9.5c |
| `0x0003db10` | `getpid` | 6 | manual | `0afbbbd060a4_getpid.obj` | CLIB3S:9.5+9.5a+9.5b+9.5c |
| `0x0003db16` | `__flush` | 157 | auto_threshold | `4b8db5c65d34_flush.obj` | CLIB3S:9.5+9.5a+9.5b+9.5c |
| `0x0003dbb3` | `ftell` | 52 | auto_threshold | `e2f62781b3ef_ftell.obj` | CLIB3S:9.5+9.5a+9.5b+9.5c |
| `0x0003dbe7` | `remove` | 16 | conflict_resolved | `af352101ce83_remove.obj` | CLIB3S:9.5+9.5a+9.5b+9.5c |
| `0x0003dbf7` | `fputc` | 168 | auto_threshold | `1b6f918a9f82_fputc.obj` | CLIB3S:9.5+9.5a+9.5b+9.5c |
| `0x0003dc9f` | `__delay_init` | 46 | byte_match | `7f8f24524bd7_delay.obj` | CLIB3S:9.5+9.5a+9.5b+9.5c |
| `0x0003dccd` | `__delay` | 59 | auto_threshold | `7f8f24524bd7_delay.obj` | CLIB3S:9.5+9.5a+9.5b+9.5c |
| `0x0003dd10` | `__STOSB` | 55 | auto_threshold | `f955fd443227___stos.obj` | CLIB3S:9.5+9.5a+9.5b+9.5c |
| `0x0003dd47` | `__STOSD` | 108 | auto_threshold | `f955fd443227___stos.obj` | CLIB3S:9.5+9.5a+9.5b+9.5c |
| `0x0003ddb3` | `tell` | 20 | auto_threshold | `b7ff66273954_tell.obj` | CLIB3S:9.5a+9.5b+9.5c |
| `0x0003ddc7` | `__prtf` | 2395 | auto_threshold | `cd8d6fb39de4_prtf.obj` | CLIB3S:9.5a+9.5b+9.5c |
| `0x0003f11b` | `fprintf` | 32 | auto_threshold | `6aba693ddccb_fprintf.obj` | CLIB3S:9.5+9.5a+9.5b+9.5c |
| `0x0003f13b` | `getenv` | 85 | auto_threshold | `d77a5300be03_getenv.obj` | CLIB3S:9.5+9.5a+9.5b+9.5c |
| `0x0003fb0f` | `isatty` | 32 | auto_threshold | `41fec40d99c4_isatty.obj` | CLIB3S:9.5+9.5a+9.5b+9.5c |
| `0x0003fb2f` | `setbuf` | 40 | auto_threshold | `16f30c7a4117_setbuf.obj` | CLIB3S:9.5+9.5a+9.5b+9.5c |
| `0x0003fb57` | `time` | 57 | auto_threshold | `0fc969d455a2_time.obj` | CLIB3S:9.5+9.5a+9.5b+9.5c |
| `0x0003fb90` | `_localtime` | 85 | auto_threshold | `a5a707b79833_localtim.obj` | CLIB3S:9.5a+9.5b+9.5c |
| `0x0003fbe5` | `localtime` | 21 | auto_threshold | `a5a707b79833_localtim.obj` | CLIB3S:9.5a+9.5b+9.5c |
| `0x0003fbfa` | `L$1_asctime_fmt2` | 65 | manual | `6c41bff3293c_asctime.obj` | CLIB3S:9.5+9.5a+9.5b+9.5c |
| `0x0003fc3b` | `_asctime` | 240 | auto_threshold | `6c41bff3293c_asctime.obj` | CLIB3S:9.5+9.5a+9.5b+9.5c |
| `0x0003fd2b` | `asctime` | 21 | auto_threshold | `6c41bff3293c_asctime.obj` | CLIB3S:9.5+9.5a+9.5b+9.5c |
| `0x0003fd40` | `_ctime` | 35 | auto_threshold | `6c41bff3293c_asctime.obj` | CLIB3S:9.5+9.5a+9.5b+9.5c |
| `0x0003fd63` | `ctime` | 25 | manual | `6c41bff3293c_asctime.obj` | CLIB3S:9.5+9.5a+9.5b+9.5c |
| `0x0003fd7c` | `strcpy` | 41 | manual | `908bbce3add0_strcpy.obj` | CLIB3S:9.5+9.5a+9.5b+9.5c |
| `0x00045cc3` | `__math87_err` | 136 | byte_match | `532db80487ba_math87e.obj` | MATH387S:9.5+9.5a+9.5b+9.5c |
| `0x00045d4b` | `__CMain` | 79 | manual | `7bb91b602be1_cmain386.obj` | CLIB3S:9.5+9.5a+9.5b+9.5c |
| `0x00045d9a` | `__InitRtns` | 67 | auto_threshold | `e51bb5866685_initrtns.obj` | CLIB3S:9.5+9.5a+9.5b+9.5c |
| `0x00045ddd` | `__FiniRtns` | 67 | auto_threshold | `e51bb5866685_initrtns.obj` | CLIB3S:9.5+9.5a+9.5b+9.5c |
| `0x00045e20` | `__setEFGfmt` | 21 | byte_match | `ab4e21c8ec05_setefg.obj` | CLIB3S:9.5+9.5a+9.5b+9.5c |
| `0x00045e36` | `__sys_init_387_emulator` | 384 | byte_match | `0a80153ccce8_dosinite.obj` | MATH387R:9.5+9.5a, MATH387S:9.5+9.5a |
| `0x00045fb6` | `__sys_fini_387_emulator` | 213 | byte_match | `0a80153ccce8_dosinite.obj` | MATH387R:9.5+9.5a, MATH387S:9.5+9.5a |
| `0x0004608b` | `L$1_chk8087_save_fpu_env` | 12 | byte_match | `031c7b66c5e1_chk8087.obj` | CLIB3S:9.5+9.5a+9.5b+9.5c |
| `0x00046097` | `L$2_chk8087_load_fpu_env` | 11 | byte_match | `031c7b66c5e1_chk8087.obj` | CLIB3S:9.5+9.5a+9.5b+9.5c |
| `0x000460a2` | `__init_8087` | 41 | auto_threshold | `031c7b66c5e1_chk8087.obj` | CLIB3S:9.5+9.5a+9.5b+9.5c |
| `0x000460cb` | `_fpreset` | 10 | byte_match | `031c7b66c5e1_chk8087.obj` | CLIB3S:9.5+9.5a+9.5b+9.5c |
| `0x000460d5` | `__chk8087` | 63 | manual | `031c7b66c5e1_chk8087.obj` | CLIB3S:9.5+9.5a+9.5b+9.5c |
| `0x00046114` | `__Init_Argv` | 398 | auto_threshold | `8b7a6a85015f_initargv.obj` | CLIB3S:9.5+9.5a+9.5b+9.5c |
| `0x000462a2` | `_dosret0` | 24 | manual | `6e73902af0df_dosret.obj` | CLIB3S:9.5+9.5a+9.5b+9.5c |
| `0x000462ba` | `_dosretax` | 23 | auto_threshold | `6e73902af0df_dosret.obj` | CLIB3S:9.5+9.5a+9.5b+9.5c |
| `0x000462d1` | `__EINVAL` | 17 | byte_match | `6e73902af0df_dosret.obj` | CLIB3S:9.5+9.5a+9.5b+9.5c |
| `0x000462e2` | `_set_errno` | 112 | auto_threshold | `6e73902af0df_dosret.obj` | CLIB3S:9.5+9.5a+9.5b+9.5c |
| `0x00046352` | `__IOMode` | 82 | auto_threshold | `37c54b985f96_iomode.obj` | CLIB3S:9.5+9.5a+9.5b+9.5c |
| `0x000463a4` | `__SetIOMode` | 24 | auto_threshold | `37c54b985f96_iomode.obj` | CLIB3S:9.5+9.5a+9.5b+9.5c |
| `0x000463bc` | `stackavail` | 9 | manual | `64e136f79228_stack386.obj` | CLIB3S:9.5+9.5a+9.5b+9.5c |
| `0x000463c5` | `_heapenable` | 19 | manual | `44e3321078e7_heapen.obj` | CLIB3S:9.5a+9.5b+9.5c |
| `0x000463d8` | `sbrk` | 157 | auto_threshold | `c0313e8c4d0d_sbrk386.obj` | CLIB3S:9.5+9.5a+9.5b |
| `0x00046475` | `__brk` | 170 | auto_threshold | `c0313e8c4d0d_sbrk386.obj` | CLIB3S:9.5+9.5a+9.5b |
| `0x00046520` | `__int386x_` | 91 | auto_threshold | `168bb609d2af_int386xa.obj` | CLIB3S:9.5+9.5a+9.5b+9.5c |
| `0x0004657b` | `_DoINTR_` | 893 | auto_threshold | `168bb609d2af_int386xa.obj` | CLIB3S:9.5+9.5a+9.5b+9.5c |
| `0x000468f8` | `__InitFiles` | 69 | auto_threshold | `912c4d366640_initfile.obj` | CLIB3S:9.5a+9.5b+9.5c |
| `0x0004693d` | `__full_io_exit` | 15 | manual | `aa0b826f7c04_ioexit.obj` | CLIB3S:9.5+9.5a+9.5b+9.5c |
| `0x0004694c` | `fcloseall` | 89 | manual | `aa0b826f7c04_ioexit.obj` | CLIB3S:9.5+9.5a+9.5b+9.5c |
| `0x000469a5` | `flushall` | 11 | byte_match | `d8d0c9de78e5_flushall.obj` | CLIB3S:9.5+9.5a+9.5b+9.5c |
| `0x000469b0` | `__flushall` | 56 | auto_threshold | `d8d0c9de78e5_flushall.obj` | CLIB3S:9.5+9.5a+9.5b+9.5c |
| `0x000469e8` | `getche` | 27 | auto_threshold | `3dc313a87fa1_getche.obj` | CLIB3S:9.5+9.5a+9.5b+9.5c |
| `0x00046a03` | `__qwrite` | 125 | auto_threshold | `0c455ab70502_qwrite.obj` | CLIB3S:9.5+9.5a+9.5b+9.5c |
| `0x00046a80` | `unlink` | 36 | conflict_resolved | `c313e1089de3_unlink.obj` | CLIB3S:9.5+9.5a+9.5b+9.5c |
| `0x00046ac9` | `utoa` | 72 | auto_threshold | `8456c2248f37_itoa.obj` | CLIB3S:9.5a+9.5b+9.5c |
| `0x00046b11` | `itoa` | 48 | auto_threshold | `8456c2248f37_itoa.obj` | CLIB3S:9.5a+9.5b+9.5c |
| `0x00046b41` | `_Not_Enough_Memory` | 12 | byte_match_disputed | `9d1f5c0476e0_nomem.obj` | CLIB3S:9.5+9.5a+9.5b+9.5c |
| `0x00046b72` | `ultoa` | 72 | auto_threshold | `aea270c9f81a_ltoa.obj` | CLIB3S:9.5a+9.5b+9.5c |
| `0x00046bba` | `ltoa` | 48 | auto_threshold | `aea270c9f81a_ltoa.obj` | CLIB3S:9.5a+9.5b+9.5c |
| `0x00046bea` | `toupper` | 21 | manual | `2f7b4656e340_toupper.obj` | CLIB3S:9.5+9.5a+9.5b+9.5c |
| `0x00046bff` | `strnicmp` | 77 | auto_threshold | `0c403d1d9661_strnicmp.obj` | CLIB3S:9.5+9.5a+9.5b+9.5c |
| `0x00046c4c` | `fgets` | 106 | auto_threshold | `935078b71f33_fgets.obj` | CLIB3S:9.5a+9.5b+9.5c |
| `0x00046cb6` | `strncpy` | 51 | auto_threshold | `881ce259ef4c_strncpy.obj` | CLIB3S:9.5+9.5a+9.5b+9.5c |
| `0x00046ce9` | `setvbuf` | 120 | auto_threshold | `81fd4203f5f1_setvbuf.obj` | CLIB3S:9.5+9.5a+9.5b+9.5c |
| `0x00046d61` | `__getctime` | 162 | auto_threshold | `f196f48e7372_getctime.obj` | CLIB3S:9.5+9.5a+9.5b+9.5c |
| `0x00046e03` | `mktime` | 284 | auto_threshold | `42f83915bcc9_mktime.obj` | CLIB3S:9.5a |
| `0x00046f1f` | `tzset` | 537 | auto_threshold | `3940ec1c1105_tzset.obj` | CLIB3S:9.5+9.5a |
| `0x00047138` | `__parse_tz` | 157 | auto_threshold | `3940ec1c1105_tzset.obj` | CLIB3S:9.5+9.5a |
| `0x000471d5` | `__brktime` | 303 | auto_threshold | `3c31341c06a5_gmtime.obj` | CLIB3S:9.5a+9.5b+9.5c |
| `0x00047304` | `_gmtime` | 36 | auto_threshold | `3c31341c06a5_gmtime.obj` | CLIB3S:9.5a+9.5b+9.5c |
| `0x00047328` | `gmtime` | 21 | manual | `3c31341c06a5_gmtime.obj` | CLIB3S:9.5a+9.5b+9.5c |
| `0x0004733d` | `__leapyear` | 52 | auto_threshold | `fee92bb59e09_timeutil.obj` | CLIB3S:9.5+9.5a+9.5b |
| `0x00047371` | `__isindst` | 657 | auto_threshold | `fee92bb59e09_timeutil.obj` | CLIB3S:9.5+9.5a+9.5b |
| `0x00047602` | `div` | 51 | auto_threshold | `bb9fedfb9a05_div.obj` | CLIB3S:9.5+9.5a+9.5b+9.5c |
| `0x0004962e` | `strncmp` | 50 | manual | `1109b8612cd1_strncmp.obj` | CLIB3S:9.5+9.5a+9.5b+9.5c |
| `0x00049660` | `__math1err` | 42 | byte_match | `adf8cd710dd5_math2err.obj` | MATH387S:9.5+9.5a+9.5b+9.5c |
| `0x0004968a` | `__math2err` | 243 | byte_match | `adf8cd710dd5_math2err.obj` | MATH387S:9.5+9.5a+9.5b+9.5c |
| `0x0004977d` | `__CommonInit` | 11 | manual | `396ee5d8b8ff_cinit.obj` | CLIB3S:9.5+9.5a+9.5b+9.5c |
| `0x00049788` | `_EFG_Format` | 299 | auto_threshold | `c44ebd7c06f7_efgfmt.obj` | CLIB3S:9.5+9.5a+9.5b+9.5c |
| `0x000498b3` | `__cnvs2d` | 35 | byte_match | `b0e463be2159_cnvs2d.obj` | MATH387S:9.5+9.5a+9.5b+9.5c |
| `0x000498d6` | `__hook387` | 207 | auto_threshold | `59ab203f79f5_hook387.obj` | CLIB3S:9.5a+9.5b+9.5c |
| `0x000499a5` | `__unhook387` | 85 | auto_threshold | `59ab203f79f5_hook387.obj` | CLIB3S:9.5a+9.5b+9.5c |
| `0x00049d98` | `__int7` | 11830 | manual | `fb5b3f690802_emu387.obj` | EMU387:9.5+9.5a |
| `0x0004cbd0` | `__init_80x87` | 45 | manual | `ccc721b858c5_init8087.obj` | CLIB3S:9.5+9.5a+9.5b+9.5c |
| `0x0004cbfd` | `__setenvp` | 228 | auto_threshold | `698cb0f41935_setenvp.obj` | CLIB3S:9.5a+9.5b+9.5c |
| `0x0004cce1` | `_set_matherr` | 13 | byte_match | `c45d6324f6e1__matherr.obj` | MATH3S+MATH387S:9.5+9.5a+9.5b+9.5c |
| `0x0004ccee` | `_matherr` | 133 | byte_match | `f3c15156dffb__matherr.obj` | MATH387S:9.5+9.5a+9.5b |
| `0x0004cd73` | `_SetMaxPrec` | 408 | byte_match | `a95bc9fb1bf2_ftos.obj` | MATH387S:9.5+9.5a+9.5b+9.5c, MATH3S:9.5+9.5a+9.5b+9.5c |
| `0x0004cf0b` | `_FtoS` | 577 | byte_match | `f534ecb54a35_ftos.obj` | MATH387S:9.5+9.5a |
| `0x0004d306` | `_dos_setvect` | 55 | auto_threshold | `14f37d2448a8_d_setvec.obj` | CLIB3S:9.5+9.5a+9.5b+9.5c |
| `0x0004d345` | `fputs` | 121 | auto_threshold | `d14e2fe74712_fputs.obj` | CLIB3S:9.5+9.5a+9.5b |
| `0x0004d3be` | `__set_EDOM` | 7 | byte_match | `9ea3227de9a5_seterrno.obj` | CLIB3S:9.5+9.5a+9.5b+9.5c |
| `0x0004d3c5` | `__set_ERANGE` | 5 | byte_match | `9ea3227de9a5_seterrno.obj` | CLIB3S:9.5+9.5a+9.5b+9.5c |
| `0x0004d3ca` | `__set_errno` | 12 | byte_match | `9ea3227de9a5_seterrno.obj` | CLIB3S:9.5+9.5a+9.5b+9.5c |
| `0x0004d3d6` | `__FPE_exception_` | 11 | byte_match | `9ea3227de9a5_seterrno.obj` | CLIB3S:9.5+9.5a+9.5b+9.5c |
| `0x0004d3e1` | `__Nan_Inf` | 110 | byte_match | `6de6ff4cc6b8_nan_inf.obj` | MATH387S:9.5+9.5a+9.5b+9.5c |
| `0x0004d450` | `fabs` | 14 | byte_match | `8c7557b22166_fabs387.obj` | MATH387S:9.5+9.5a+9.5b+9.5c |
| `0x0004d45e` | `IF@DABS` | 3 | byte_match | `8c7557b22166_fabs387.obj` | MATH387S:9.5+9.5a+9.5b+9.5c |
| `0x0004d464` | `IF@LOG` | 76 | byte_match | `d9a05561b4f2_log387.obj` | MATH387S:9.5+9.5a+9.5b+9.5c |
| `0x0004d4b0` | `IF@LOG2` | 4 | byte_match | `d9a05561b4f2_log387.obj` | MATH387S:9.5+9.5a+9.5b+9.5c |
| `0x0004d4b4` | `IF@LOG10` | 4 | byte_match | `d9a05561b4f2_log387.obj` | MATH387S:9.5+9.5a+9.5b+9.5c |
| `0x0004d4cb` | `log10` | 19 | byte_match | `d9a05561b4f2_log387.obj` | MATH387S:9.5+9.5a+9.5b+9.5c |
| `0x0004d4f1` | `_Scale` | 75 | byte_match | `ab3b63c7c67c_scale.obj` | MATH387S:9.5+9.5a+9.5b+9.5c |
| `0x0004d53c` | `_Scale10V` | 218 | byte_match | `ab3b63c7c67c_scale.obj` | MATH387S:9.5+9.5a+9.5b+9.5c |
| `0x0004d616` | `__cvt` | 420 | byte_match | `5e6dafa551e3_cvt.obj` | MATH387S:9.5a |
| `0x0004d84c` | `__ZBuf2F` | 158 | auto_threshold | `904423069b96_amodf.obj` | CLIB3S:9.5+9.5a |
| `0x0004d8f1` | `__log87_err` | 72 | byte_match | `e8a3e61d00b6_log87e.obj` | MATH387S:9.5+9.5a+9.5b+9.5c |
| `0x0004d93c` | `__CmpBigInt` | 58 | byte_match | `a0b712117bdb_i64tos.obj` | MATH387R:9.5+9.5a+9.5b+9.5c, MATH387S:9.5+9.5a+9.5b+9.5c, MATH3R:9.5+9.5a+9.5b+9.5c, MATH3S:9.5+9.5a+9.5b+9.5c |
| `0x0004d976` | `__Rnd2Int` | 107 | byte_match | `a0b712117bdb_i64tos.obj` | MATH387R:9.5+9.5a+9.5b+9.5c, MATH387S:9.5+9.5a+9.5b+9.5c, MATH3R:9.5+9.5a+9.5b+9.5c, MATH3S:9.5+9.5a+9.5b+9.5c |
| `0x0004d9e1` | `__Bin2String` | 297 | byte_match | `a0b712117bdb_i64tos.obj` | MATH387R:9.5+9.5a+9.5b+9.5c, MATH387S:9.5+9.5a+9.5b+9.5c, MATH3R:9.5+9.5a+9.5b+9.5c, MATH3S:9.5+9.5a+9.5b+9.5c |
| `0x0004db0a` | `frexp` | 88 | byte_match | `3bf50f39eafd_frexp.obj` | MATH387S:9.5+9.5a+9.5b+9.5c |
| `0x0004db64` | `modf` | 32 | byte_match | `3ede649993e4_modf387.obj` | MATH387S:9.5+9.5a+9.5b+9.5c |

## Aggregate by source lib + version

Each (lib, version) pair lists the FD2 functions that came from a obj in that lib for that Watcom 9.5x sub-version.

| lib | version | function count | examples |
|---|---|---:|---|
| CLIB3S | 9.5 | 133 | `L$1_stk_save_ss`, `__CHK`, `__GRO`, `__STK`, `__STKOVERFLOW` (+128 more) |
| CLIB3S | 9.5a | 157 | `L$1_stk_save_ss`, `__CHK`, `__GRO`, `__STK`, `__STKOVERFLOW` (+152 more) |
| CLIB3S | 9.5b | 142 | `L$1_stk_save_ss`, `__CHK`, `__GRO`, `__STK`, `__STKOVERFLOW` (+137 more) |
| CLIB3S | 9.5c | 129 | `L$1_stk_save_ss`, `__CHK`, `__GRO`, `__STK`, `__STKOVERFLOW` (+124 more) |
| EMU387 | 9.5 | 1 | `__int7` |
| EMU387 | 9.5a | 1 | `__int7` |
| MATH387R | 9.5 | 6 | `__CHP`, `__sys_init_387_emulator`, `__sys_fini_387_emulator`, `__CmpBigInt`, `__Rnd2Int` (+1 more) |
| MATH387R | 9.5a | 6 | `__CHP`, `__sys_init_387_emulator`, `__sys_fini_387_emulator`, `__CmpBigInt`, `__Rnd2Int` (+1 more) |
| MATH387R | 9.5b | 4 | `__CHP`, `__CmpBigInt`, `__Rnd2Int`, `__Bin2String` |
| MATH387R | 9.5c | 4 | `__CHP`, `__CmpBigInt`, `__Rnd2Int`, `__Bin2String` |
| MATH387S | 9.5 | 33 | `__CHP`, `sqrt`, `IF@SQRT`, `__@DSQRT`, `IF@COS` (+28 more) |
| MATH387S | 9.5a | 34 | `__CHP`, `sqrt`, `IF@SQRT`, `__@DSQRT`, `IF@COS` (+29 more) |
| MATH387S | 9.5b | 30 | `__CHP`, `sqrt`, `IF@SQRT`, `__@DSQRT`, `IF@COS` (+25 more) |
| MATH387S | 9.5c | 29 | `__CHP`, `sqrt`, `IF@SQRT`, `__@DSQRT`, `IF@COS` (+24 more) |
| MATH3R | 9.5 | 3 | `__CmpBigInt`, `__Rnd2Int`, `__Bin2String` |
| MATH3R | 9.5a | 3 | `__CmpBigInt`, `__Rnd2Int`, `__Bin2String` |
| MATH3R | 9.5b | 3 | `__CmpBigInt`, `__Rnd2Int`, `__Bin2String` |
| MATH3R | 9.5c | 3 | `__CmpBigInt`, `__Rnd2Int`, `__Bin2String` |
| MATH3S | 9.5 | 5 | `_set_matherr`, `_SetMaxPrec`, `__CmpBigInt`, `__Rnd2Int`, `__Bin2String` |
| MATH3S | 9.5a | 5 | `_set_matherr`, `_SetMaxPrec`, `__CmpBigInt`, `__Rnd2Int`, `__Bin2String` |
| MATH3S | 9.5b | 5 | `_set_matherr`, `_SetMaxPrec`, `__CmpBigInt`, `__Rnd2Int`, `__Bin2String` |
| MATH3S | 9.5c | 5 | `_set_matherr`, `_SetMaxPrec`, `__CmpBigInt`, `__Rnd2Int`, `__Bin2String` |

## Notes

- A single FD2 function maps to ≥ 1 (lib, version) appearance if the same obj SHA appears in multiple Watcom 9.5x versions (typical when obj bytes are unchanged across versions).
- A FD2 function whose obj SHA is **only** in 9.5+9.5a is a strong signal that FD2 was linked with that specific lib version. `MATH387S/dosinite.obj` (`__sys_init_387_emulator` / `__sys_fini_387_emulator`) and `MATH387S/ftos.obj` (`_FtoS`) are 9.5+9.5a-only — confirming FD2's compiler/lib version.
- A FD2 function whose obj appears in MULTIPLE libs (e.g., `dosinite.obj` in both MATH387R and MATH387S — same byte content, ABI-independent) means the obj is shared across register-call and stack-call lib variants. Either lib in the build pipeline EXTDEF resolves it correctly.
