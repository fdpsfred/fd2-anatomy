# tools/data_emit/ -- 真實 global data 從 FD2.LE 落地 src/ 的盤點與遷移工具

把目前散在 `tests/testglob.c`（假 zero-fill）與 Ghidra（真 bytes）之間的遊戲 global data，
系統性地以 Ghidra 為權威來源抽出真值、落到所屬 `src/*.c`，並從 testglob 移除假版，
最終讓 src-only 能 link 出 `fd2.exe`。

## 元件

| 檔案 | 用途 |
|---|---|
| `reconcile.py` | 對帳器。讀 Ghidra data-symbol dump（`workspace/data_emit/ghidra_data_symbols.tsv`，由 `run_script_inline` 產），掃 `src/**/*.c` 與 `tests/testglob.c` 的 file-scope `data_fd2_*` 定義，把每個 Ghidra 符號標成 `real_in_src` / `fake_in_testglob` / `undefined` / `sublabel`，輸出 `workspace/data_emit/worklist.tsv` + 摘要。會對「testglob 有定義但 Ghidra 無同名符號」拋 drift 警告。**注意 `real_in_src` 只代表「src/ 有同名 file-scope 定義」（name-match），不代表內容正確 —— 內容正確性由 `verify_real.py` 另證。** |
| `verify_real.py` | **byte-equality 驗證器（內容正解的唯一憑據）**。對每個 `real_in_src` 符號，從 Ghidra `read_memory` 抽真 bytes（dump 進 `real_in_src_ghidra_bytes.tsv`），解析 src/ 的 C initializer，逐 byte + 長度比對，輸出 PASS/FAIL（FAIL 附首個 diff 位置）。全 PASS 才能把 `real_in_src` 當可信。**Phase 1 每 emit 一個 data table 都要過此 gate（抽 Ghidra → emit → re-verify byte-identical）—— 它是所有 emitted data 的 regression 關卡，不只首批 28 個。** 自帶負控（同長度不同內容須區分、A 的 C 值比 B 的 Ghidra bytes 須 FAIL）以防假 PASS。 |
| `home_map.py` | 為每個待遷 data 符號定 **home src 檔** + **emit_class**。emit_class 用鐵證(symbol 位址的 bytes 非 0 = 有真初值)× write-xref 分:`const`(無 writer + 非 0 → `const T[]={bytes}`)/ `init-data`(有 writer + 非 0 → `T name={bytes};` 可變)/ `zero-bss`(全 0 → `T name;`,執行期或經 memcpy/ptr 間接寫)。`needs_bytes` = const∪init-data,須過 `verify_real`。home:有 writer → 該 owner 函式的 routing target 檔;否則 `const-data:<subsystem>`(Phase 1 取 8.3 檔名)。讀 `worklist.tsv` + `data_xref_owners.tsv` + `fake_bytes_nonzero.tsv`(皆 Ghidra dump)+ `routing.json`。 |
| `rename_global.py` | 安全 whole-word 全域改名的**機械套用器**（caller 逐一決定 old→new，工具不 derive 名）。whole-word boundary 避免誤傷子字串（如 local `orig_<name>`）；掃 `src/`+`tests/` 的 `.c/.h` **與 KB `.md`（program_info / resource_info / rebuild_info / assets + index.md / open_issues.md）做 code+KB 同步**；套用後驗證舊名殘留 = 0。Ghidra 端另改（per 命名規則 [[feedback_game_data_symbol_naming]]）。 |
| `mk_routing.py` | 產 **`src/data_routing.json`**（Phase 1 的進度＋home 事實來源，mirrors function 的 routing.json）。join `home_map.tsv`+`worklist.tsv`，把 `const-data:<sub>` 桶解析成 `table/<sub>tab.c` 8.3 檔，每符號輸出 {addr/segment/len/datatype/kind/emit_class/needs_bytes/home/writers/emitted/reviewed/commit}。 |
| `mkpart.py [N]` | 把 worklist 切成 **N 個 file-disjoint 分區**（greedy-LPT，needs_bytes 權重 ×2）供並行 worktree emit。過大的 `table/*` 桶依位址切編號子檔（`btltab.c`/`btltab2.c`…，CAP 18 syms / 12000 bytes），split home 回寫 `data_routing.json`，分區寫 `workspace/data_emit/partitions/part_{1..N}.json`。idempotent。 |
| `scout.py` | 為一個分區產 `data_emit.wf.js` 的 `args` JSON（跳過 `reviewed=true`）。`scout.py <manifest> <worktree_abs_root> [label]` → 印 args；`scout.py --stats` 看覆蓋率（reviewed / needs_bytes / pending by home）。 |
| `data_emit.wf.js` | **Phase 1 主 workflow（per-symbol commit 版）**。per-symbol〔emitter：caller 分析定真型別/維度 + `read_memory` 抽真 byte + `verify_real --one` byte gate（append-only 防覆寫）→ 獨立 reviewer 自抓三源復核 → ≤10 round → **lander**：改 globals.h extern + 移 testglob 假版 + 標 `data_routing` reviewed + clobber 防線 + `git commit`，**不 build**〕→ **per-home-file buildGate**〔`genbuild --apply` + `build_test` 0err/0warn + 純機械修正（extern 對齊 / const-writer `#if0` SKIP，回報 `skipped_tests`）〕。**每個符號一完成就 commit**，撞 limit/529/斷線零浪費、re-scout 跳過 reviewed 續跑（見 memory `feedback_per_symbol_commit_durability`）。內含 budget guard / Ghidra 斷線偵測。 |

## 跑法

```bash
# 1.（Ghidra 端，run_script_inline）dump 全 data_fd2_ 符號 -> workspace/data_emit/ghidra_data_symbols.tsv
#    欄位：addr segment name datatype len kind(ptr_table/byte_data/sublabel) xrefs primary
# 2. 對帳產 worklist（real_in_src = name-match，未證內容）
python tools/data_emit/reconcile.py
# 3.（Ghidra 端）read_memory dump real_in_src 真 bytes -> real_in_src_ghidra_bytes.tsv
# 4. byte-equality 驗證（內容正解的唯一憑據；Phase 1 每表 emit 後都跑）
python tools/data_emit/verify_real.py
# 5.（Ghidra 端）dump 每符號的 write/read xref owner -> data_xref_owners.tsv
#    + dump fake_in_testglob 符號 bytes 是否全 0 -> fake_bytes_nonzero.tsv
# 6. home + emit_class 對映（Phase 1 worklist 定案）
python tools/data_emit/home_map.py
```

中間檔（`ghidra_data_symbols.tsv` / `worklist.tsv`）寫到 `workspace/data_emit/`（scratch）。

## Phase 1 emit run（四路並行，主 session 統籌）

進度單一事實來源 = `src/data_routing.json` 的 `reviewed` 欄；per-home-file commit = 斷點，中斷零成本續做。

```bash
# 1. 重生事實來源 + 分區（idempotent，任何時候可重跑）
python tools/data_emit/mk_routing.py && python tools/data_emit/mkpart.py 4
# 2. 建 4 worktree（from integ HEAD）：N = 1..4
git worktree add ../fd2-wt/dpN -b data-pN integ
# 3. 每路取 args 並啟動背景 workflow（主 session 同時 4 個並行）：
python tools/data_emit/scout.py workspace/data_emit/partitions/part_N.json <ABS path ../fd2-wt/dpN> data-pN
#    把輸出 JSON 當 args -> Workflow(scriptPath:"tools/data_emit/data_emit.wf.js", args:<scout 輸出>)
# 4. 進度 / 收尾
python tools/data_emit/scout.py --stats        # reviewed 應收斂到 347
```

- **中斷重跑**：先 `git -C ../fd2-wt/dpN checkout -- src tests` 清掉半成品 def（否則 emitter 會重複 append），再重跑 scout（自動跳過 `reviewed=true`）+ Workflow。
- **全部完成** → merge cascade 併回 `integ`（testglob/globals union，沿 `src/handoff.md` §3 方法論）→ 驗收：`scout.py --stats` reviewed=347、`verify_real` 全批 PASS、`build_test` 0err/0warn → hard-stop 等使用者再進 Phase 2。
- **emitter/reviewer/finalizer 前景跑 `build_test`，嚴禁 `run_in_background`**（同 emit pipeline 鐵則）。

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
- **全 data_fd2_ 帳目（563 = 28 real_in_src + 347 fake_in_testglob + 187 undefined + 1 sublabel）**：
  `fake_in_testglob`（347，含 6 個 fn-ptr handler 表）= Phase 1 worklist（`home_map.tsv`）。`undefined`
  187 個 = Ghidra 有、C 兩端皆無 file-scope 定義，**經神諭驗證後分三類**：(a) 106 個
  `cutscene_event_script_NNN` —— 只經 cutscene ptr table 間接引用，隨「正確 emit 該 ptr table（具名目標）」
  帶出；(b) 58 個 `data_fd2_string_*` —— **emit 端已是 inline 字面值、非缺口**（src 無具名引用）；
  (c) ~23 個 graphics/battle state —— 隨尚未 emit 的函式（blit / cinematic，Phase 2）落地。
- **scan_defs 須涵蓋 fn-ptr 陣列定義**（`RET (*data_fd2_name[N])(params)`）—— DEF_RE 的前導型別 pattern
  跨不過 `(*`，需 `FNPTR_RE` 補抓，否則 testglob 的 handler 表會被誤判 undefined。
