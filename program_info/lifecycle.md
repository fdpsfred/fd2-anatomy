# lifecycle

包含三類 function：

1. **Watcom C runtime (CRT)**：fopen/fread/fclose/fseek/fwrite, memset/memmove,
   malloc/free，全部 `crt_*` 前綴。
2. **Entry point / startup / exit**：`crt_entry_start @ 0x3C964` →
   `crt_main_trampoline @ 0x45D4B` → `fd2_main @ 0x25BF4`。
3. **結局 cinematic**：`play_ending_and_record_clear`（行為屬「程式生命週期最後階段」）。

## CRT 層 (13 個)

| 位址 | 名稱 | 標準對應 |
|---|---|---|
| `0x00036CD7` | `crt_frame_setup` | Watcom stack-check helper（XCHG framesize→EAX, CALL stack-overflow check, RET 4） |
| `0x00036D16` | `crt_malloc_track` | malloc wrapper + tracking |
| `0x00036D26` | `crt_malloc_track_impl` | 內部 impl |
| `0x00036FA1` | `crt_fopen_impl` | fopen 底層 |
| `0x00036FCC` | `crt_fopen_read` | `fopen(name, "rb")` |
| `0x00037072` | `crt_fread` | fread |
| `0x00037244` | `crt_fclose` | fclose |
| `0x000373C4` | `crt_memmove` | memmove (處理 overlap) |
| `0x00037416` | `crt_free` | free |
| `0x00037426` | `crt_free_impl` | 內部 impl |
| `0x0003744B` | `crt_fwrite` | fwrite |
| `0x000375C0` | `crt_memset` | memset |
| `0x000375F0` | `crt_fseek` | fseek |

`crt_open_files_list @ 0x000541AC` 是 `crt_fclose` 用來找 stream 的 FILE* 鏈頭。

## CRT 程式碼地理位置

Watcom CRT 函式分散在 `.object1` 各處（與 game logic、Miles AIL library
**互相交錯**，不是連續區段）。已命名的 13 個 CRT helper 集中出現在
`0x36000-0x37700` 與 `0x3D???-0x3E???` 附近（如 errno getter @ 0x3D7F6、delay
@ 0x3DCCD、memset_impl @ 0x3DD10），但這只是觀察到的密集區，**不能用 address
range 來判定某 function 是否屬於 CRT**。判別方式見 `overview.md` 的 library
boundary 段。

## 結局 cinematic

`play_ending_and_record_clear @ 0x0001F894` — 結局動畫播放，並寫入通關記錄到
FD2.SAV。為何屬 lifecycle 而非 field_map：它是 game session 的最後一段流程，與
`fd2_main` 退出條件直接耦合。

## 不細究的 library helpers

`FUN_0003DCCD` 是 Watcom CRT `_sleep` style helper，FD2 透過此 thunk 進入 BIOS
interrupt。`rand` / `srand` 是 Watcom CRT RNG；FD2 game-logic 的隨機性
（attack roll、spell 命中等）走自己的 named wrapper，不直接命名 CRT RNG。
