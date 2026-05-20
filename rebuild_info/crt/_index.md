# rebuild_info/crt/

Watcom v2 CRT 在 FD2.LE 內的符號 inventory、與重建 EXTDEF 對照。

## 文件

- `fid_match.md` — 編譯器版本結論（Watcom 9.5a / CLIB3S+EMU387+GRAPH+MATH387S /
  DOS/4G）、lib 構成、9.5a 版本判定要點、`lookup_9.5a.json` schema 與
  verified 來源分布、命名規範。識別 pipeline 細節寫在 `tools/program_analysis/crt_fid_match/_index.md`
- `symbol_inventory.md` — CRT 命名約定（Watcom 真符號 / `crt_equivalent_*` /
  `fd2_*` CRT-style primitive）、static-link duplicates、15 個
  `crt_equivalent_*` 與 10 個 `fd2_*` CRT-style helper 名單與用途

## Lookup 資料

- `lookup_9.5a.json` — 行為驗證後的 address ↔ Watcom CRT symbol 對照表
  （186 entries）。`by_address` / `by_name` 雙向索引，`source_libs` 欄位記錄
  每個 obj 在 Watcom 9.5/9.5a/9.5b/9.5c × 各 lib 的 appearances；命名規則為
  「lookup `name` 與 Ghidra function 名 byte-identical」
- `matched_function_sources.md` — 每個 lookup entry 的 source 版本/lib 對照
  表（從 `lookup_9.5a.json` 的 `source_libs` 欄位產生），用於 build pipeline
  決定哪個 .obj 該 EXTDEF 哪個 lib 解析

FidQuery raw 輸出 (`matches_9.5a.json`) 屬 pipeline intermediate，不入 git；
重跑時由 `tools/program_analysis/crt_fid_match/ghidra_scripts/FidQuery.java`
寫到 `workspace/crt_fid_match/results/`。
