# Watcom 9.5a / DOS / DOSBox-X 通用 toolchain 陷阱

重建任何 FD2 object（src-only FD2.EXE、TEST build、AIL `.obj` 重組）共通會踩到的低階 toolchain 已知
陷阱。AIL audio driver 專屬的陷阱（BLASTER IRQ / AIL_DEBUG / wlib OMF record buffer）見
`../ail/build_quirks.md`。

## Watcom 9.5a `wcc386` 嚴格 C89

block 內 declaration 必須在第一個 statement 前。混 declaration/statement
→ `E1077: Missing '}'` 連鎖 error。

## File name 限制（LFN 不通用）

| Tool | LFN cmdline | LFN cmdfile | 對策 |
|---|---|---|---|
| `wcc386` (BIN) | ✓ | n/a | source .c 用 ≤8 char basename |
| `wlink` (BIN) | ✓ | ✗ | `.lnk` directive 用 8.3 |
| `wlib` (BINB) | ✓ | ✗ | `.rsp` + `.obj` 預先 copy 為 8.3 alias |

所有 `src/` 的 `.c` / `.h` basename 一律 ≤ 8.3（Watcom 9.5a 無 LFN）。AIL lib 打包時
`pack_libs.py` 把 `objs/*.obj` copy 為 `A0001.OBJ` 等 8.3 alias；lib 內 module name 取自 THEADR
（仍 LFN）。

## DOS COMMAND.COM batch 限制

不支援 multi-line `if () else ()` block。必須用 `goto :label`。

## Watcom `rename` 不允許 dst 已存在

DOSBox-X COMMAND.COM 的 `rename` 若 dst 已存在會 silent fail。
build script 每次 rename 前 `if exist new del new`。

## DOS/4G LE binary 啟動需求

`wlink system dos4g` 產 LE binary。執行時需 `DOS4GW.EXE` 在 PATH 或 cwd。

## Ghidra label-duplication 陷阱

vendor binary 可能有不同 address 共用同名 Ghidra label。wlink 的
"redefinition of \<symbol\> ignored" warning 是偵測條件。已修正案例：

- 0x53604 `data_ail_driver_timer_isr_reentry_guard` 與 0x54354 原本同名
  → 0x54354 rename 為 `data_ail_driver_timer_isr_saved_eflags`

（注意這與 emit pipeline merge 期間 stub + real 並存的 W1027 redefinition 是兩回事 —— 後者是預期
產物、real 勝出、合法留存，見 `tools/code_emit/_index.md`。）
