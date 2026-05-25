# OMF emit 規約

`bin_to_omf.py` emit `.obj` 時必須遵守的規約。違反會造成 wlink 後 silent
corruption（無 warning 但 runtime wild jump / dos4gw hang / hardware probe fail）。

## R-1: Segment 名用 Watcom default

AIL .obj SEGDEF 名必須是 `_TEXT` (CODE) / `_DATA` (DATA)，不可用自訂名
如 `AIL_CODE`。自訂段名會落入獨立 selector，DS implicit 訪問的 imm32
base 不對 → wild access fault。

## R-2: BSS items emit 成 file-backed zero-init `_DATA`

`segment="bss"` 的 data 不走 OMF BSS class SEGDEF。改 emit `_DATA` class
+ `LEDATA32` 全零。standalone BSS selector 會觸發 dos4gw unbounded page
commit → DOSBox-X hang。

## R-3: LEDATA bytes 必須 zero at FIXUPP32 site

每筆 FIXUPP32（LOC=9）的 LEDATA 4 bytes 必須 overwrite 為 `00 00 00 00`。
wlink 用 **addend semantics**：`final = addend + target [- src - 4]`。
保留 binary-time disp32 會造成雙倍 offset → wild jump。

## R-4: mid-fn / mid-data PUBDEF 必須與 EXTDEF 對齊

`resolve_le_target()` 走 path 3（fn body 非 entry）或 path 4（data mid-array）
產生的 EXTDEF `L_<name>_alt_<off>`，target .obj **必須 emit 同名 PUBDEF**。
PUBDEF set 來自 4 個 source 的 union：midfn.jsonl + LE FIXUP + synth.jsonl +
fd2common rel32 sites。

## R-5: mid-data end-exclusive sentinel 也算 alt label

`fd2_dpmi_lock_region(start, end)` 的 `end = start + size`。
`d_addr + d_size` 是合法 alt-offset，emit PUBDEF `L_<data>_alt_<size>`。
`_record_mid_data` 條件含等號：`d_addr < tgt <= d_addr + d_size`。

## R-6: 全 fn entry 都 emit PUBDEF

不只 `AIL_*` public fn，所有 `AIL_internal_*` 與 `fd2common_*` fn entry
都必須 PUBDEF — vendor binary 內 AIL fn 互相 cross-fn rel32 reference。
漏 PUBDEF → wlink silent unresolved → EXE 內 garbage offset → wild jump。

client header 暴露名單由 `gen_ailv3_h.py` 的 name filter 控制。

## OMF FIXUPP32 方法 B（mid-fn jump-into-middle）

Watcom 9.5a optimizer 做 tail merging — 多個 fn 共用尾段。per-fn .obj 下
此類跨 .obj mid-fn reference 透過方法 B 處理：

| | target .obj | caller .obj |
|---|---|---|
| PUBDEF | entry + `L_<fn>_alt_<offset>` 各一條 | — |
| EXTDEF | — | `L_<fn>_alt_<offset>` |
| FIXUPP32 | — | TARGT=6, P=1 (no displacement) |

命名規約：`L_<target_fn_name>_alt_<offset_hex_lower>`（hex 不含 0x，不 zero-pad，字母小寫）。

## CRT EXTDEF

AIL fn body 內 E8 disp32 CALL 到 CRT 的 target 對應 CLIB3S 9.5a 真符號
（lookup 在 `rebuild_info/crt/lookup_9.5a.json`）。9.5a 與原 binary 同版，
symbol name byte-identical，EXTDEF 直接解析。

CRT data extern（`data_crt_*` → Watcom `_<name>`）同理。
