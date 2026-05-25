# tools/

可重複利用的 Python script。每個 script self-contained — 自帶路徑常數、不
import shared lib、不依賴 `legacy/`。

CLI 用法看 `python <script> --help`；預設輸出路徑都已標註在 help 文字內。

## 資料儲放慣例

| 位置                      | 用途                                                                                                  |
| ------------------------- | ----------------------------------------------------------------------------------------------------- |
| `tools/{tool}/data/`    | primary input data only：無法靠 script 重產的資料（外部 dependency snapshot、人工 authored verdicts） |
| `workspace/{工作名稱}/` | script 一切新增/編輯檔案的目的地：pipeline intermediate / output、audit state、KB-freeze 候選稿       |

KB（`rebuild_info` / `program_info` / `resource_info` / `assets`）不引用
`workspace/` path。

兩類 script 處理方式不同：

- **即時重生** — `build_call_graph.py` / `build_data_inventory.py` 寫到
  `workspace/{call_graph,data_audit}/`，不留 KB snapshot；要 fresh 資料時
  直接重跑。KB 文件引用 script 名稱，不引用輸出檔。
- **KB-freeze 候選稿** — `build_final_lookup.py` 寫到 workspace，要 publish
  到 `rebuild_info/crt/lookup_9.5a.json` 時人工複製（lookup 需要混合多個
  pipeline 結果，不適合純機械重生）。

工作名稱 ↔ workspace 路徑：

| Subfolder                                      | Workspace                       |
| ---------------------------------------------- | ------------------------------- |
| `tools/decoders/`                            | `workspace/decoders/<sub>/`   |
| `tools/glyph/`                               | `workspace/glyph/<sub>/`      |
| `tools/program_analysis/build_call_graph.py` | `workspace/call_graph/`       |
| `tools/program_analysis/crt_fid_match/`      | `workspace/crt_fid_match/`    |
| `tools/program_analysis/crt_callee_match/`   | `workspace/crt_callee_match/` |
| `tools/program_analysis/data_audit/`         | `workspace/data_audit/`       |
| `tools/program_analysis/function_audit/`     | `workspace/function_audit/`   |
| `tools/program_analysis/jump_table_audit/`   | `workspace/jump_table_audit/` |
| `tools/ail_extract/`                         | `workspace/ail_extract/`        |

`tools/program_analysis/crt_fid_match/data/` 內的 primary input：

- `observations_*.json` — 人工 verify observation（manual verdict 來源；
  build_final_lookup.py 依此決定 inclusion / rejection）

FidQuery raw 輸出 (`matches_9.5a.json`) 屬 pipeline intermediate，不入 git；
重跑時由 `ghidra_scripts/FidQuery.java` 寫到 `workspace/crt_fid_match/results/`

## 子資料夾

- `decoders/` — LLLLLL DAT archive parser + 各資源檔解碼器
- `glyph/` — 中文字 glyph atlas 渲染 + ET3 STDFONT pixel-match
- `program_analysis/` — FD2.LE 結構性分析工具集合（5 audit pipeline + call_graph builder）
- `ail_extract/` — Miles AIL audio library 從 FD2.LE 抽出重建為 ailv3.lib + ailv3.h + fd2common.lib + test_audio.exe（Watcom 9.5a target，在 DOSBox-X 內 build / run）
