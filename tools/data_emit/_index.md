# tools/data_emit/ -- 真實 global data 從 FD2.LE 落地 src/ 的盤點與遷移工具

把目前散在 `tests/testglob.c`（假 zero-fill）與 Ghidra（真 bytes）之間的遊戲 global data，
系統性地以 Ghidra 為權威來源抽出真值、落到所屬 `src/*.c`，並從 testglob 移除假版，
最終讓 src-only 能 link 出 `fd2.exe`。

## 元件

| 檔案 | 用途 |
|---|---|
| `reconcile.py` | 對帳器。讀 Ghidra data-symbol dump（`workspace/data_emit/ghidra_data_symbols.tsv`，由 `run_script_inline` 產），掃 `src/**/*.c` 與 `tests/testglob.c` 的 file-scope `data_fd2_*` 定義，把每個 Ghidra 符號標成 `real_in_src` / `fake_in_testglob` / `undefined` / `sublabel`，輸出 `workspace/data_emit/worklist.tsv` + 摘要。會對「testglob 有定義但 Ghidra 無同名符號」拋 drift 警告。**注意 `real_in_src` 只代表「src/ 有同名 file-scope 定義」（name-match），不代表內容正確 —— 內容正確性由 `verify_real.py` 另證。** |
| `verify_real.py` | **byte-equality 驗證器（內容正解的唯一憑據）**。對每個 `real_in_src` 符號，從 Ghidra `read_memory` 抽真 bytes（dump 進 `real_in_src_ghidra_bytes.tsv`），解析 src/ 的 C initializer，逐 byte + 長度比對，輸出 PASS/FAIL（FAIL 附首個 diff 位置）。全 PASS 才能把 `real_in_src` 當可信。**Phase 1 每 emit 一個 data table 都要過此 gate（抽 Ghidra → emit → re-verify byte-identical）—— 它是所有 emitted data 的 regression 關卡，不只首批 28 個。** 自帶負控（同長度不同內容須區分、A 的 C 值比 B 的 Ghidra bytes 須 FAIL）以防假 PASS。 |
| `rename_global.py` | 安全 whole-word 全域改名的**機械套用器**（caller 逐一決定 old→new，工具不 derive 名）。whole-word boundary 避免誤傷子字串（如 local `orig_<name>`）；掃 `src/`+`tests/` 的 `.c/.h` **與 KB `.md`（program_info / resource_info / rebuild_info / assets + index.md / open_issues.md）做 code+KB 同步**；套用後驗證舊名殘留 = 0。Ghidra 端另改（per 命名規則 [[feedback_game_data_symbol_naming]]）。 |

## 跑法

```bash
# 1.（Ghidra 端，run_script_inline）dump 全 data_fd2_ 符號 -> workspace/data_emit/ghidra_data_symbols.tsv
#    欄位：addr segment name datatype len kind(ptr_table/byte_data/sublabel) xrefs primary
# 2. 對帳產 worklist（real_in_src = name-match，未證內容）
python tools/data_emit/reconcile.py
# 3.（Ghidra 端）read_memory dump real_in_src 真 bytes -> real_in_src_ghidra_bytes.tsv
# 4. byte-equality 驗證（內容正解的唯一憑據；Phase 1 每表 emit 後都跑）
python tools/data_emit/verify_real.py
```

中間檔（`ghidra_data_symbols.tsv` / `worklist.tsv`）寫到 `workspace/data_emit/`（scratch）。

## 已知分類陷阱（reconcile 後逐項處理時注意）

- `kind=ptr_table` 混三種：真 function-pointer 表（handler tables，emit 成函式名初始化列）、
  data-pointer 表（`weapon_attack_anim_pattern_ptr_table` / `cutscene_event_script_ptr_table`，
  指向其他 object3 資料 → 連目標一起 emit + 符號引用）、單一指標狀態（`runtime_char_array_ptr` 等，
  bss 0-init）。要看實際定義（`= {...}` vs `= 0`）區分。
- 命名規則：所有 game global data 一律乾淨 `data_fd2_`（battle 核心表 `data_fd2_battle_*`），
  Ghidra+C 一致、無 Ghidra 自動 `_<addr>` 後綴。詳見 [[feedback_game_data_symbol_naming]]。
  （11 個 drift / 無符號 / plain-named 已對齊完畢，drift=0。）
- **g_ gate 陷阱**：`rename_data` / `rename_or_label` 對已定型 data（string / struct 型別）強制 `g_`
  前綴、拒 `data_fd2_`。**正解＝`run_script_inline` 跑 `symbol.setName("data_fd2_...", SourceType.USER_DEFINED)`
  （包 transaction、逐一）**，絕不可退讓去用 Ghidra 爛名。label 創建 / undefined-data rename 不受 gate 影響。
- `undefined`（Ghidra 有、C 兩端皆無定義）多為只經 ptr table 間接引用的真資料（如 106 個
  `cutscene_event_script_NNN`）；其落地由「正確 emit 對應 ptr table（具名目標）」帶出。
