# rebuild_info/link/

FD2.LE 連結環境的解析：LE binary layout、wlink 命令列重建、DOS/4GW bind stub。

## 文件

- `le_layout.md` — FD2.LE 的 LE header 全欄位 + 3 個 object (CGROUP _TEXT / DGROUP /
  FAR_DATA) 的 base / virtual_size / flags + DGROUP 內部排列 (CONST → _DATA →
  _BSS → STACK + cmdline buffer 共用 4K)、page map / fixup section 統計、入口流程
  (`crt_equivalent_entry_start` → `crt_equivalent_dos_main_bootstrap` cstart →
  `__InitRtns` + `__CMain` → `fd2_main`)、FD2.EXE 的 10424-byte Watcom DOS bind stub
- `wlink_settings.md` — 從 binary 反推的 wlink directive (`system dos4g` +
  `name FD2.EXE` + `option stack=4K` + Miles lib + Watcom CRT auto-pull)、每條
  directive 對 binary 內哪個特徵負責的證據鏈、source-side `#pragma data_seg("FAR_DATA")`
  把大型 data table 推進 object 3 的推測、`wcc386` 編譯旗標、重建驗證流程

## 結論摘要

| 項目 | 值 |
|---|---|
| wlink system | `system dos4g`（多 extender 自動偵測：DOS/4G / DOS/4GW / Phar Lap）|
| LE format | `'LE'` signature；page size 4096；71 個 page；3 個 object |
| 輸出 | `FD2.EXE`（10424-byte Watcom 預設 DOS bind stub + LE 模組）|
| 模組內部名 | `"f2"`（resident name @ ordinal 0；來自 main `.obj` basename `f2.obj`）|
| Code 區 | obj 1 @ 0x10000，257 KB，全 1342 個 function |
| DGROUP | obj 2 @ 0x50000，22 KB；CONST + _DATA + _BSS + 4 KB STACK (與 cmdline buffer 共用) |
| FAR_DATA | obj 3 @ 0x60000，13.5 KB；FD2 工程師用 `#pragma data_seg` 把大型 data table 推進這個 group |
| Stack | 4096 byte (`option stack=4K`，比 dos4g 預設 8K 小)；初始 ESP = `0x556B0` |
| Heap | LE header heap=0；DOS/4GW DPMI INT 31h 動態 alloc |
| Imports | 0 個 DLL import；全 INT 21h / DPMI / Miles static lib |
| Resources | 0 個 |
| Debug info | 0 byte（連結時關掉或事後 `wstrip`）|
| Watcom 版本 | 9.5a (per `../crt/fid_match.md`) |
| Watcom CRT lib | CLIB3S + EMU387 + GRAPH + MATH387S |
| Audio lib | Miles AIL3DIG + AIL3MDI static lib |
