# AIL 相關的 build / runtime quirks

AIL audio driver 與 AIL `.obj` 重組專屬的 Watcom 9.5a / DOSBox-X 陷阱。重建任何 FD2 object 共通的
低階 toolchain 陷阱（C89 / LFN / batch / rename / DOS4GW 啟動 / Ghidra label 重複）見
`../build_test/toolchain_quirks.md`。

## BLASTER env 必須匹配 SB conf IRQ

DOSBox-X SB16 IRQ 由 `[sblaster] irq=N` 決定。AIL SB-class driver 在
`AIL_install_DIG_INI` 走 BLASTER env autodetect（INI 內 IRQ=-1）。
`BLASTER` 的 `IN` 必須匹配 conf IRQ，否則 PCM playback silent fail。

```
[sblaster] sbtype=sb16 sbbase=220 irq=5 dma=1 hdma=5
set BLASTER=A220 I5 D1 H5 P330 T6
```

86Box 等其他 emulator 的 IRQ 可能不同（如 IRQ 7），調整 BLASTER 的 `I` 即可。
（API 層的 IRQ autodetect 行為見 `public_api_semantics.md`。）

## DOSBox-X 對 AIL_DEBUG long path 不友善

AIL_startup 若 `getenv("AIL_DEBUG")` 非 NULL，走 log path 並透過
`INT 21h ax=2508h` 註冊 log timer ISR。DOSBox-X DPMI host 對 USE32
selector 的 ISR handler 觸發 GP fault。

解法：不 set `AIL_DEBUG` env，AIL_startup 走 short path。

## wlib 9.5a record buffer 限制

wlib 9.5a 對 EXTDEF / PUBDEF32 record buffer 有 ~1KB 限制。
`omf_writer.py` 對 payload ≥1000B 自動切多個 record（AIL `.obj` 重組 pipeline 用）。
