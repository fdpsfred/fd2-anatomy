# tools/program_analysis/crt_callee_match/

對 lookup 內 CRT function 之間呼叫到的「未識別 callee」做 byte-level
FIXUPP-aware 比對。補抓 FidDb 漏抓的 small helper、lib 端 anonymous
static (`L$N`)、Ghidra 誤切的 split fragment 與 merger false positive，
逐個驗證後寫回 `lookup_9.5a.json`。

## Pipeline

| script | 用途 |
|---|---|
| `extract_obj_funcs.py` | OMF parser：對 dedup 內每個 .obj 抽 PUBDEF 名稱 / segment offset / function size |
| `extract_obj_bytes.py` | OMF parser：對指定 .obj 抽出每個 function 的 hex bytes + FIXUPP 位置 |
| `match_callees.py` | 用 callee size + caller `source_obj` heuristic 找 lib candidate（三層 tier 報告） |
| `verify_pairs.py` | byte-level 比對 (FD2 vs lib，FIXUPP 位置 wildcard)；出 PASS/FAIL 結果 |

## 典型 session

```bash
# 1. 建 size 索引
python tools/program_analysis/crt_callee_match/extract_obj_funcs.py \
    --in-dir <dedup_pool_dir>

# 2. 對指定 caller .obj 抽 byte + FIXUPP
python tools/program_analysis/crt_callee_match/extract_obj_bytes.py \
    --in-dir <dedup_pool_dir> --objs <obj1> <obj2> ...

# 3. (Ghidra MCP) 對每個 callee / parent 拉 FD2 byte → fd2_bytes.json
#    格式：{addr_hex8: {len, hex}}

# 4. Candidate 篩選
python tools/program_analysis/crt_callee_match/match_callees.py \
    --callees callees.json \
    --obj-funcs workspace/crt_callee_match/obj_funcs.json \
    --lookup rebuild_info/crt/lookup_9.5a.json

# 5. byte-level 比對
python tools/program_analysis/crt_callee_match/verify_pairs.py \
    --pairs pairs.json \
    --lib-bytes workspace/crt_callee_match/lib_bytes.json \
    --fd2-bytes fd2_bytes.json
```

## OMF parser quirks（Watcom Easy OMF-386）

- **SEGDEF**：32-bit segment 由 record type `0x99`（不是 `0x98`）或
  quirky-detected `0x98` 標示，**不是**靠 ACBP P-bit；P-bit 在 Watcom 不可信
- **FIXUPP LOCAT field**：byte0 = `1 M LLLL OO`（M=mode bit6,
  location_type=bits5..2, offset hi=bits1..0），byte1 = offset low 8 bit；
  total offset 是 10-bit
- **Fixup width**：32-bit segment 內 `location_type=1`/`5` 是 4-byte fixup
  （標準 OMF 是 2-byte）；要從 SEGDEF type/quirky 推得

## 輸入 JSON schema

`callees.json`（`match_callees.py` 輸入）：
```json
[
  {"addr": "0003d7f6", "size": 6,
   "callers": [{"addr": "00036e0d", "name": "__open_flags"}, ...]}
]
```

`pairs.json`（`verify_pairs.py` 輸入）：
```json
[
  {"label": "0003d7f6", "kind": "C", "fd2_start": "0x3d7f6", "fd2_len": 6,
   "lib_obj": "046c7fbb59e1_errno.obj", "lib_func": "__get_errno_ptr",
   "notes": "errno ptr getter"}
]
```

`fd2_bytes.json`（從 Ghidra 端產生）：
```json
{"0003d7f6": {"len": 6, "hex": "b8a4410500c3"}}
```

## 已知陷阱

FIXUPP byte 用 wildcard mask 處理；若看到 FAIL 但 mismatch 集中在特定
4-byte 區域，先檢查 FIXUPP 抽得對不對（quirky 32-bit fix 可能漏 patch）。
