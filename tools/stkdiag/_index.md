# tools/stkdiag/

Watcom `__CHK` 堆疊探測診斷模組產生器。

| Script | 用途 |
|---|---|
| `gen_stkdiag.py` | 產 `STKDIAG.OBJ`（寫入 `workspace/fd2_build/exe/out/obj/`）：以手工機器碼 + `tools/ail_extract/omf_writer.py` 生成取代 CLIB3S(stk) 的 `__CHK`/`__GRO`/`__STK`/`__STKOVERFLOW`。語意與 CLIB 相同（省略 flat model 下永不生效的 SS 逃生門），但觸發時先印 `PROBE=xxxxxxxx ESP=xxxxxxxx`（PROBE−10 = 觸發探測的函數起點；ESP 判斷所在堆疊）再印 `Stack Overflow!` 終止。 |

## 用法

1. `python tools/stkdiag/gen_stkdiag.py`
2. 把 `STKDIAG.OBJ` 以 `file` 形式插進 wlink directive（在 `library` 行之前），
   重新連結即可（範例流程：複製 `workspace/fd2_build/exe/fd2.lnk` 改 `name` +
   插入 file 行，DOSBox-X 內跑 `WLINK @diag.lnk`）。
3. 觸發後用該 build 的 wlink map 對 PROBE−10 查函數符號。

## 背景

FD2 原版把遊戲碼編成帶 `__CHK`、把 CRT-equivalent / DPMI 支援單元編成無探測
（`src/crt/crt.c`、`src/util/dpmi.c` 以 `#pragma off (check_stack)` 對齊）。
AIL timer/audio-mix ISR 會在 AIL 私有 DGROUP 堆疊（低於 `_STACKLOW`、SS 同為
flat selector）上呼叫 `crt_equivalent_get_eflags_thunk`，帶探測的版本在該處
必誤發 Stack Overflow——本工具即為定位此類誤發/真溢位而作。
