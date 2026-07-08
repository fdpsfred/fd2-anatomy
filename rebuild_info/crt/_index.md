# rebuild_info/crt/

FD2.LE 內 Watcom 9.5a CRT 的符號 inventory 與 rebuild EXTDEF 對照（rebuild
toolchain 用 9.5a，與原 binary 同版）。

## 文件

- `fid_match.md` — 編譯器版本結論（Watcom 9.5a / CLIB3S+EMU387+GRAPH+MATH387S /
  DOS/4G）、lib 構成、9.5a 版本判定要點、`lookup_9.5a.json` schema 與 verified
  來源分布、命名規範。識別 pipeline 細節見
  `tools/program_analysis/crt_fid_match/_index.md`。
- `symbol_inventory.md` — CRT 命名約定（Watcom 真符號 / `crt_equivalent_*` /
  `fd2_*` CRT-style primitive），以及兩組必須在 `src/` 以 C 重寫的函式具名清單、
  LX loader 三件組行為、fptan worker 歸屬、static-link duplicates。

## Lookup 資料

- `lookup_9.5a.json` — 行為驗證後的 address ↔ Watcom CRT symbol 對照表。
  `by_address` / `by_name` 雙向索引，`source_libs` 欄記錄每個 obj 在 Watcom
  9.5/9.5a/9.5b/9.5c × 各 lib 的 appearances；命名規則為「lookup `name` 與
  Ghidra function 名 byte-identical」。收錄的 CRT function 數即該檔 `by_address`
  的條目數（現值見 `fid_match.md` 的識別結論）。
- `matched_function_sources.md` — 由 `lookup_9.5a.json` 的 `source_libs` 欄產生的
  generated view，把每個 address 對應到含 matching obj 的 Watcom lib / 版本，供
  build pipeline 決定哪個 .obj 該 EXTDEF 哪個 lib 解析。重生方式見
  `tools/program_analysis/crt_fid_match/_index.md`。

FidQuery raw 輸出（`matches_9.5a.json`）屬 pipeline intermediate，不入 git。fidb
對照組與 FidQuery 結果的重生見 `tools/program_analysis/crt_fid_match/_index.md`。
