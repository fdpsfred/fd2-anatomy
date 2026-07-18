# tools/oob_index_audit/

`src/` 全域表越界索引靜態掃描器 —— 抓「折疊基底被歸錯符號」這一類還原陷阱。

| Script | 用途 |
|---|---|
| `scan_oob_index.py` | 掃過 `src/` 每一個對 `data_fd2_*` 陣列的 subscript，把索引可能離開宣告界線的全部列出。輸出 `workspace/oob_index_audit/findings.json`。 |

## 用法

```
python tools/oob_index_audit/scan_oob_index.py            # 掃 src/
python tools/oob_index_audit/scan_oob_index.py --selftest # 內建交叉驗證
```

`--src` / `--out` 可改掃描來源與輸出目錄（例如把某個 commit 的 `src/` 展開後回頭掃）。
`--show-pointers` 才會列出 POINTER 桶。

## 為什麼要掃

編譯器會把常數索引調整折進位址位移：`cost_table[job_id - 1]`（`cost_table @ 0x5266B`）
編成 `MOVSX ESI, word ptr [job_id*2 + 0x52669]`。折疊後的基底落在**前一個**符號體內，
Ghidra 於是把它呈現成 `prev_table[job_id + 5]`。原版兩個符號相鄰所以照字面抄也碰巧正確，
rebuild 的 linker 一把它們隔開就會讀到不相干的資料。完整鐵則見
`../../rebuild_info/equivalence/rules.md` 的「跨符號讀取不變式」。

## 分類桶

| 桶 | 意義 |
|---|---|
| `OOB_CONST` | 常數索引 ≥ 宣告元素數 |
| `OOB_OFFSET` | `[x + N]` 且 `N ≥ 元素數 - 1`，折疊基底的典型特徵 |
| `NEAR_END` | `[x + N]` 且 N 落在末端附近（折疊常數 k = 2..4 的情形） |
| `UNPARSED` | 中括號沒有閉合，需人工看 |
| `UNKNOWN_SIZE` | 解析不出宣告大小的 `data_fd2_*` subscript |
| `POINTER` | 對 runtime 指標的 subscript，沒有靜態界線，非 finding |

`UNKNOWN_SIZE` 歸零代表**每一個** subscript 都對應到已解析的宣告，是覆蓋無缺口的判準。
命中的項目一律要對照 disasm 人工裁決 —— 索引變數本身的值域若使存取留在界內，正的常數項
是合法的。

掃描器只能處理折疊基底這一類；「宣告元素數短於索引定義域」需要 reader 索引值域的知識，
無法靜態判定，只能逐一從 disasm 與平行表確認。

## 驗證

`--selftest` 涵蓋宣告解析（各種修飾詞 / 多字型別 / 函式指標陣列 / 指標 / 無大小）、
分類判斷、頂層 `+N` 切分（不可被巢狀中括號或括號內的 `+N` 誤導），以及一則端對端案例：
符號名與 `[` 之間換行、索引內含巢狀 `[]` 的真實 bug 形狀必須被命中，而宣告本身不得自我命中。
