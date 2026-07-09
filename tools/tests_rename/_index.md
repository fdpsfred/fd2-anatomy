# tools/tests_rename/

把 `tests/` 內殘留的 src_refine 前舊 symbol 名批次同步到新名，使 `tests/` 能對現行
`src/include/` 編譯。權威 old→new 對照唯讀取自 `tools/src_refine/data/rename_old2new.json`
（`symbols` 欄），recipe 見同目錄 `rename_explain.md`。此工具只讀 `tools/src_refine/`，絕不寫入該處。

| Script | 用途 |
|---|---|
| `sync_rename.py` | 對每個 `tests/**/*.{c,h}` 以**單次** atomic word-boundary 取代套用 old→new map（單次是為讓 pose-table 名稱 SWAP 正確解析——兩次序列取代會互相抵銷）|
| `verify.py` | rename 的獨立 cross-check（唯讀）：`--targets`（套用前）確認每個新名在 `src/` 存在、`tests/` 能解析；`--residual`（套用後）確認 `tests/` 零殘留舊名（pose pair 例外）|

註：此 tests/ 同步為 `open_issues.md` 的開放項（已批准、時機待使用者指示），工具已就緒但尚未套用。
