# rebuild_info/ail/

Miles AIL audio library (AIL3DIG + AIL3MDI) 在 FD2.LE 內的 inventory 與重建抽取 prep。

## 檔案

- `inventory.md` — FD2.LE 內 AIL ecosystem 共 422 個 function 的完整 inventory：103 public API、184 internal helper、132 DIG mixer dispatch callbacks、static-link thunk + body pair、`AIL_DEBUG` 字串辨識方式、命名規約、Vendor-internal public API (54 對 wrapper + inner)、library 邊界。DOS-side driver / patch 檔（.MDI / .DIG / AILDRVR.LST / SAMPLE.AD/OPL/BNK）見 `program_info/audio.md`
- `extraction_prep.md` — 抽 AIL `.obj` 工作的前置資料：283 個 AIL function 邊界、8 個違反 cdecl EBX preservation 的 `__watcall` 例外、9 個 game/CRT 共享 helper 的處置策略、CLIB3S 9.5a EXTDEF map、build pipeline 草案（Watcom 9.5a toolchain）
