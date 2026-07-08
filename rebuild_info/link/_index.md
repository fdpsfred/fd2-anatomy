# rebuild_info/link/

FD2.LE 連結環境的解析：LE binary 靜態 layout、wlink 命令列重建、Watcom Easy OMF-386 格式、
DOS/4GW bind stub。

## 文件

- `le_layout.md` -- FD2.LE 靜態 layout 唯一正典：LE header 全欄位 + 3 個 object（`_TEXT` /
  DGROUP / FAR_DATA）的 base / virtual_size / flags + DGROUP 內部排列（CONST -> _DATA -> _BSS
  -> STACK + cmdline buffer 共用 4K）、object 3 大型 data table 的位址分布、page map / fixup
  section 統計、entry point（`_cstart_ @ 0x3C964`）、FD2.EXE 的 10424-byte Watcom DOS bind
  stub 與 DOS/4GW、「不能用位址範圍判斷 function 類別」的 interleave 原則、FD2.LE <-> 另一
  發行版 FD2.EXE 的跨版本 offset 對照
- `wlink_settings.md` -- 實測 `fd2.lnk` directive 與 `wcc386` 編譯旗標的唯一正典：`system
  dos4g` + `name FD2.EXE` + 顯式列 Miles AIL / CLIB3S / MATH387S / EMU387、`-s`（stack-check）
  與 `__NO_MATH_OPS` 兩個 load-bearing 旗標政策、每條 directive 對 binary 內哪個特徵負責的
  證據鏈；`system dos4g` 不自動加 C runtime 的結論
- `omf_386.md` -- Watcom Easy OMF-386 目標檔格式的 quirks（32-bit SEGDEF record type、FIXUPP
  LOCAT 10-bit offset、32-bit segment 內 4-byte fixup width），讀寫 `.obj` 共用的格式結論

## 結論摘要

| 項目 | 值 |
|---|---|
| wlink system | `system dos4g`（cstart 內建多 extender 偵測：DOS/4G / Phar Lap 386&#124;DOS / Intel Code Builder）|
| LE format | `'LE'` signature；page size 4096；71 個 page；3 個 object |
| 輸出 | `FD2.EXE`（10424-byte Watcom 預設 DOS bind stub + LE 模組）|
| 模組內部名 | `"f2"`（resident name @ ordinal 0；來自 main `.obj` 的 basename）|
| Code 區 | obj 1 @ 0x10000，~257 KB，全部 code（function 總數以 Ghidra `get_function_count` 即時取得）|
| DGROUP | obj 2 @ 0x50000，~22 KB；CONST + _DATA + _BSS + 4 KB STACK（與 cmdline buffer 共用）|
| FAR_DATA | obj 3 @ 0x60000，~13.5 KB；FD2 大型遊戲 data table（source-level segment rename 推入）|
| Stack | 4096 byte（原版 `option stack=4K`，比 dos4g 預設 8K 小）；初始 ESP = `0x556B0` |
| Heap | LE header heap=0；DOS/4GW DPMI INT 31h 動態 alloc |
| Imports | 0 個 DLL import；全 INT 21h / DPMI / Miles static lib |
| Resources | 0 個 |
| Debug info | 0 byte（連結時關掉或事後 `wstrip`）|
| Watcom 版本 | 9.5a（見 `../crt/fid_match.md`）|
| Watcom CRT lib | CLIB3S + MATH387S + EMU387（src-only 建置實測不需 GRAPH）|
| Audio lib | Miles AIL（AIL3DIG + AIL3MDI）static lib |
