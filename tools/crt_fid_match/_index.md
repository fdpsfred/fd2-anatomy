# tools/crt_fid_match/

支援「FD2.LE 對 Watcom 9.5a CLIB3S 的 CRT function 識別」工作流的 script。
分兩段：(1) 建 4 個版本 fidb（一次性），(2) 對 raw FidQuery 結果做行為驗證
產出可信的 address ↔ Watcom CRT symbol 對照表（每次 Ghidra 重新分析後可重跑）。

工作流文檔：`rebuild_info/crt_fid_match.md`。

## fidb 建構 pipeline (§9 of crt_fid_match.md)

| script | 用途 |
|---|---|
| `omf_lib_extract.py` | wlib `-q -x` wrapper，把 4 個 Watcom 版本 × 3 個 lib 拆成 .obj |
| `omf_patch_segdef.py` | 修 Watcom Easy OMF-386 quirky record（85/770 .obj 命中） |
| `build_manifest.py` | 建跨版本 dedup 索引（770 unique .obj） |
| `build_dedup_dir.py` | 把 dedup 後的 .obj 集中到一個 flat dir |
| `compare_results.py` | 從 4 份 query JSON 生 markdown 比對報告（驗證 9.5a superset 假設） |
| `ghidra_scripts/FidWipeFolder.java` | 清 Ghidra project folder |
| `ghidra_scripts/FidImportBatch.java` | batch import .obj，含 setLanguage 切到 `x86:LE:32:watcom` |
| `ghidra_scripts/FidAnalyzeAll.java` | 對 import 的 program 跑 auto-analysis |
| `ghidra_scripts/FidPopulate.java` | 從 program 集合建 `.fidb` |
| `ghidra_scripts/FidQuery.java` | 對 target program 跑 query → JSON |

## Lookup table 生成 pipeline (§11 of crt_fid_match.md)

| script | 用途 |
|---|---|
| `build_crt_lookup.py` | 讀 `rebuild_info/crt_matches_9.5a.json`，分流出 4 個 verify queue (`auto_candidates` / `sample` / `conflict` / `manual`) 到 `workspace/crt_fid_match/` |
| `verify_rules.py` | 60+ 個 Watcom CRT symbol 的行為簽章 RULES 表 + `apply_rule()` 套用器；五類條件（body_size / callees / instructions / int21_ah / is_leaf）AND 起來 |
| `verify_crt_samples.py` | 接 queue + observations JSON，套 `verify_rules` 出 markdown verify report；`--observations` 缺則出 scaffold 模式（待填空白） |
| `build_final_lookup.py` | 整合 `auto_candidates.json` + manual observations + 衝突解決 → `rebuild_info/crt_lookup_9.5a.json` + `rebuild_info/crt_verify_rejected.md` |

每個 entry 在 lookup 內標 `verified` ∈ `{auto_threshold, conflict_resolved,
manual}`。`auto_threshold = 20` 的可靠性由 10 個等距樣本（10/10 PASS）轉移
成立。observation 中加 `manual_verdict: "REJECT" + manual_reason` 可顯式覆寫
規則 PASS 結果，用於 set-errno-X family 等 hash-identical 但語意不符的案例。

## 一個典型驗證 session 流程

```bash
# 1. 分流（不需 Ghidra）
python tools/crt_fid_match/build_crt_lookup.py

# 2. 在 Claude Code 內透過 Ghidra MCP 收集 disasm + callees + decomp，
#    手動或半自動寫入 workspace/crt_fid_match/observations_phaseB.json
#    (10 sample + 3 conflict) 與 observations_phaseD.json (18 manual)
#    每個 entry 含 `callees`（已翻譯成 Watcom 名）+ `asm`（disasm 文字）+
#    `key_instructions`（人類摘要）+ `notes`；REJECT 案另加 `manual_verdict`
#    + `manual_reason`。

# 3. 合併 observations → finalize report
python tools/crt_fid_match/verify_crt_samples.py \
    --queue workspace/crt_fid_match/sample_queue.json \
    --queue workspace/crt_fid_match/conflict_queue.json \
    --queue workspace/crt_fid_match/manual_queue.json \
    --observations workspace/crt_fid_match/observations_all.json \
    --out rebuild_info/crt_verify_report.md

# 4. 出最終 lookup table + rejected.md
python tools/crt_fid_match/build_final_lookup.py
```

## Verify_rules schema 摘要

```python
@dataclass
class Rule:
    notes: str = ""
    body_size_range: tuple[int, int] | None = None
    callees_required_any: list[str] = field(default_factory=list)
    callees_required_all: list[str] = field(default_factory=list)
    callees_forbidden: list[str] = field(default_factory=list)
    instructions_any: list[str] = field(default_factory=list)
    instructions_all: list[str] = field(default_factory=list)
    int21_ah_any: list[int] = field(default_factory=list)
    is_leaf: bool | None = None
```

`apply_rule(matched_name, asm, callee_names, body_size, decomp="")` 回傳
failure 字串列表（空 list = PASS）。

## 既知問題

- callee 名翻譯：Ghidra 內 callee 多半是描述性名稱（如 `crt_fprintf_stderr`），
  套規則前須先用 `crt_matches_9.5a.json` 的 address → matched_name 映射轉
  回 Watcom 符號名。`observations_*.json` 內的 `callees` 欄已是翻譯後結果。
- `is_leaf=True` 對 wrapper-pattern function 太嚴 — 例如 Watcom 9.5a 的
  memset 是「broadcast val + CALL __STOSB」wrapper、rand 是「CALL seed_ptr_getter
  + LCG」wrapper、__CHK 是「XCHG + CALL __STK」wrapper；rule 已分別放寬。
  新增 rule 時不要無條件加 `is_leaf=True`。
- set-errno-X helper family（dosret.obj 的 __EINVAL/__EBADF/__ENOENT/...）
  byte signature identical，FidDb 只能標出 family 不能區分具體 errno。
  rule 若要區分需加 `instructions_any=["MOV dword ptr [EAX] ,0x16"]` 之類的
  immediate-aware pattern。
