# AIL calling convention 與 ABI

AIL vendor library 在 FD2.LE 中的 calling convention 例外和 client header
對策。完整 function inventory 見 `inventory.md`。

## `__watcall` 例外（少數 fn）

部分 AIL function 違反 Watcom 9.5a `__cdecl` EBX preservation 約定（內部用
EBX 為 loop counter，prologue 無 `push ebx` / epilogue 無 `pop ebx`），且
整個 fn 簽名用 register-pass convention（args 在 EAX/EDX/EBX 而非 stack）。
client C declaration 應標 `__watcall` 而非 `__cdecl`。

Watcom 9.5a **不認** `__watcall` 為 inline keyword（OpenWatcom 才加），
所以 `ailv3.h` 對 `__watcall` fn 不 emit cc keyword（用 Watcom default
register-cc）；對 `__cdecl` fn emit `__cdecl` keyword。

`__watcall` fn 清單透過 Ghidra MCP 即時取得：
```
search_functions(name_pattern="^AIL_")  # 過濾 cc == __watcall
```

## Caller-saved 暫存器 clobber convention

AIL 的 helper 內部 clobber EBX 卻不 push/pop（vendor optimizer 移除了 wrapper
不用到的 push），而且身為一般呼叫，也會破壞 volatile 的 EAX/ECX/EDX。client 端
是否出錯，取決於它用哪種 calling convention：

- `-3r`（register-cc，`test_audio.c` / `tau.c` 採用）：EAX/EBX/ECX/EDX 本來就是
  傳引數的 volatile 暫存器，編譯器一定把跨呼叫存活的值放到 ESI/EDI/EBP（AIL 有
  保留），所以即使只標 `modify [ebx]` 也安全。
- `-3s`（stack-cc，**FD2 遊戲採用**）：EBX 預設是 callee-saved，編譯器會把跨呼叫
  存活的值放進它，AIL 破壞 EBX → 該值損毀。**而且 Watcom 把 modify list 當「精確
  集合」解讀**——只寫 `modify [ebx]` 反而會讓編譯器誤以為 EAX/ECX/EDX 被保留，
  污染只是從 EBX 搬到 EDX/ECX，沒有真正修好（已用三個 pragma 變體實測證實）。

**Fix**：`gen_ailv3_h.py` 對每個 public AIL fn emit：

```c
extern <ret> __cdecl AIL_<fn>(<args>);
#pragma aux AIL_<fn> "*" modify [eax ebx ecx edx];
```

- `"*"` 抑制 cdecl 預設的 `_` prefix/suffix 對 PUBDEF 名（讓 lib EXTDEF 對得上）
- `modify [eax ebx ecx edx]` 列出全部四個 caller-saved 暫存器，在 `-3r` 與 `-3s`
  下都正確；少列任何一個在 `-3s` 下都會把污染轉移到沒列的那顆暫存器

FD2 遊戲端的 `src/include/protos.h` 直接 `#include "ailv3.h"`，所以這份 clobber
資訊對遊戲所有 AIL 呼叫生效（`fd2_play_sfx_with_handle` 把 sample offset 跨
`AIL_init_sample` 存活，正是靠它才不被破壞、避免 SFX 靜音）。

## Handle 與 buffer-pointer 型別

`ailv3.h` 的 handle typedef（`HSAMPLE` / `HDIGDRIVER` / ...）是 `void *`：Ghidra
確認 AIL 內部把 handle 當指標 dereference（`AIL_allocate_sample_handle` 回傳 slot
指標，`AIL_set_sample_address` 的 worker 0x41250 寫 `[handle+8]=address`）。遊戲端
對應的 handle 全域（`data_fd2_audio_sfx_sample_handle_0/1`、driver / sequence
handle）因此也用 `void *`。

但 buffer-pointer 型參數（如 `AIL_init_sequence` 的 XMI data、`AIL_set_sample_address`
的 sample 位址）在 ailv3.h 用 `unsigned int` 而非 `void *`：FD2 的 resource 層
（`fd2_load_dat_resource`）一律用 32-bit 值（`uint32`）表示載入緩衝的指標，client
照此傳遞，配 `unsigned int` 才不噴 W113，也不必把 `void *` 擴散進整個 resource 層。
handle 用 `void *`、data buffer 用 `uint32`，這個區分是刻意的。

## Handle typedef

`ailv3.h` 定義 7 個 opaque handle typedef（Miles SDK convention）：

| Typedef | 底層型別 | 來源 API |
|---|---|---|
| `HDIGDRIVER` | `void *` | `AIL_install_DIG_*` |
| `HMDIDRIVER` | `void *` | `AIL_install_MDI_*` |
| `HSAMPLE` | `void *` | `AIL_allocate_sample_handle` |
| `HSEQUENCE` | `void *` | `AIL_allocate_sequence_handle` |
| `HTIMER` | `unsigned int` | `AIL_register_timer` |
| `HDRIVER` | `void *` | `AIL_install_driver` |
| `HWAVESYNTH` | `void *` | `AIL_create_wave_synthesizer` |

Watcom 32-bit flat model 下 `int` 和 `void *` 都是 32-bit，底層表示可互換。

## Vendor data symbol 暴露

`data_ail_alloc_fnptr` / `data_ail_free_fnptr` 是 AIL 內部 allocator slot。
FD2 game main 啟動時 patch 為 CRT `malloc / free`。AIL self-contained client
必須在 `AIL_startup` 前手動 patch：

```c
extern void *data_ail_alloc_fnptr;
extern void *data_ail_free_fnptr;
#pragma aux data_ail_alloc_fnptr "*";
#pragma aux data_ail_free_fnptr "*";
data_ail_alloc_fnptr = (void *)malloc;
data_ail_free_fnptr  = (void *)free;
```

## fd2common pool

FD2 自寫的 helper，AIL vendor code 透過 EXTDEF reference：

| 群組 | Function | 用途 |
|---|---|---|
| DPMI wrapper | `fd2_dpmi_alloc_dos_memory` | INT 31h fn 0x100 |
| | `fd2_dpmi_free_dos_memory` | INT 31h fn 0x101 |
| | `fd2_dpmi_lock_region` | INT 31h fn 0x600 |
| | `fd2_dpmi_unlock_region` | INT 31h fn 0x601（short JMP 共用 lock_region tail） |
| | `fd2_dpmi_lock_size` / `_unlock_size` | (base, size) → (start, end) wrapper |
| EFLAGS | `crt_equivalent_get_eflags` | PUSHFD; POP EAX; CLI; RET（4B） |
| | `crt_equivalent_get_eflags_thunk` | JMP 到上者（5B） |

Step 1 CRT 識別確認：Watcom 9.5a 全 19 lib 掃描 0 byte-match。
全部是 FD2 原創或 CRT 行為等價，不是 Watcom lib symbol。
