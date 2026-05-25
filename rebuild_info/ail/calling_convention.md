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

## EBX-clobber convention（多數 fn）

大量 AIL internal helper（含 cdecl 公開的 wrapper）內部 clobber EBX 不
push/pop — vendor optimizer 移除了 unused-by-wrapper 的 push。

對 client 影響：wcc386 `-3s` cdecl 預期 EBX callee-saved，client code 可能
register-allocate local 到 EBX。AIL fn call 後 EBX corrupted → 用 EBX 為
local pointer / counter 時失敗。

**Fix**：`gen_ailv3_h.py` 對每個 public AIL fn emit：

```c
extern <ret> __cdecl AIL_<fn>(<args>);
#pragma aux AIL_<fn> "*" modify [ebx];
```

- `"*"` 抑制 cdecl 預設的 `_` prefix/suffix 對 PUBDEF 名（讓 lib EXTDEF 對得上）
- `modify [ebx]` 告訴 wcc386 該 fn 不 preserve EBX

這是 1990 年代 Watcom 生態系的標準做法 — `#pragma aux` 是 Watcom 設計
用來描述非標準 ABI 的機制。Miles vendor SDK 的 Watcom 版 header 同樣使用此 pattern。

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
