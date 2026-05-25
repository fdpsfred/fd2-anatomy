# Build / runtime toolchain quirks

Watcom 9.5a + DOS/4G + DOSBox-X 環境下的已知陷阱。

## Watcom 9.5a `wcc386` 嚴格 C89

block 內 declaration 必須在第一個 statement 前。混 declaration/statement
→ `E1077: Missing '}'` 連鎖 error。

## File name 限制（LFN 不通用）

| Tool | LFN cmdline | LFN cmdfile | 對策 |
|---|---|---|---|
| `wcc386` (BIN) | ✓ | n/a | source .c 用 ≤8 char basename |
| `wlink` (BIN) | ✓ | ✗ | `.lnk` directive 用 8.3 |
| `wlib` (BINB) | ✓ | ✗ | `.rsp` + `.obj` 預先 copy 為 8.3 alias |

`pack_libs.py` 把 `objs/*.obj` copy 為 `A0001.OBJ` 等 8.3 alias。
lib 內 module name 取自 THEADR（仍 LFN）。

## BLASTER env 必須匹配 SB conf IRQ

DOSBox-X SB16 IRQ 由 `[sblaster] irq=N` 決定。AIL SB-class driver 在
`AIL_install_DIG_INI` 走 BLASTER env autodetect（INI 內 IRQ=-1）。
`BLASTER` 的 `IN` 必須匹配 conf IRQ，否則 PCM playback silent fail。

```
[sblaster] sbtype=sb16 sbbase=220 irq=5 dma=1 hdma=5
set BLASTER=A220 I5 D1 H5 P330 T6
```

86Box 等其他 emulator 的 IRQ 可能不同（如 IRQ 7），調整 BLASTER 的 `I` 即可。

## DOS COMMAND.COM batch 限制

不支援 multi-line `if () else ()` block。必須用 `goto :label`。

## Watcom `rename` 不允許 dst 已存在

DOSBox-X COMMAND.COM 的 `rename` 若 dst 已存在會 silent fail。
build script 每次 rename 前 `if exist new del new`。

## DOS/4G LE binary 啟動需求

`wlink system dos4g` 產 LE binary。執行時需 `DOS4GW.EXE` 在 PATH 或 cwd。

## DOSBox-X 對 AIL_DEBUG long path 不友善

AIL_startup 若 `getenv("AIL_DEBUG")` 非 NULL，走 log path 並透過
`INT 21h ax=2508h` 註冊 log timer ISR。DOSBox-X DPMI host 對 USE32
selector 的 ISR handler 觸發 GP fault。

解法：不 set `AIL_DEBUG` env，AIL_startup 走 short path。

## Ghidra label-duplication 陷阱

vendor binary 可能有不同 address 共用同名 Ghidra label。wlink 的
"redefinition of \<symbol\> ignored" warning 是偵測條件。已修正案例：

- 0x53604 `data_ail_driver_timer_isr_reentry_guard` 與 0x54354 原本同名
  → 0x54354 rename 為 `data_ail_driver_timer_isr_saved_eflags`

## wlib 9.5a record buffer 限制

wlib 9.5a 對 EXTDEF / PUBDEF32 record buffer 有 ~1KB 限制。
`omf_writer.py` 對 payload ≥1000B 自動切多個 record。
