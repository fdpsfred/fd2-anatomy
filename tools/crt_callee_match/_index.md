# tools/crt_callee_match/

支援「找出 CRT function 呼叫到的未識別 callee 是哪個 Watcom CRT lib symbol」工作流的 script。
四個 script 串成 OMF parser → candidate 篩選 → byte-level 比對 → 結論的 pipeline。

工作流文檔：`rebuild_info/crt_fid_match.md` §12。

## Pipeline

| script | 用途 |
|---|---|
| `extract_obj_funcs.py` | OMF parser：對 dedup 內每個 .obj 抽 PUBDEF 名稱、segment 內 offset、function size |
| `extract_obj_bytes.py` | OMF parser：對指定 .obj 抽出每個 function 的 hex bytes + FIXUPP 位置（reference operand 標記）|
| `match_callees.py` | 用 callee size + caller `source_obj` heuristic 找出每個 callee 的 lib candidate（三層 tier 報告）|
| `verify_pairs.py` | byte-level 比對 (FD2 vs lib，FIXUPP 位置 wildcard)；出 PASS/FAIL 結果 |

## OMF parser 重要 quirk（兩支 extract script 都有）

Watcom Easy OMF-386 與標準 OMF 在 32-bit 段有以下差異，未處理會 mis-parse：

- **SEGDEF**：32-bit segment 由 record type 0x99（不是 0x98）或 quirky-detected 0x98 標示，**不是**靠 ACBP P-bit；P-bit 在 Watcom 不可信
- **FIXUPP LOCAT field 解析**：byte0 = `1 M LLLL OO`（M=mode bit6, location_type=bits5..2, offset hi=bits1..0），byte1 = offset low 8 bit；total offset 是 10-bit
- **Fixup width**：32-bit segment 內 `location_type=1`/`5` 是 4-byte fixup（標準 OMF 是 2-byte）；要從 SEGDEF type/quirky 推得

## 一個典型驗證 session

```bash
# 1. 對 dedup 內所有 .obj 建 size 索引（一次性）
python tools/crt_callee_match/extract_obj_funcs.py \
    --in-dir workspace/fid_match/dedup \
    --out workspace/crt_callee_match/obj_funcs.json

# 2. 對特定 caller .obj 抽 byte + FIXUPP（要驗哪些 .obj 就帶哪些）
python tools/crt_callee_match/extract_obj_bytes.py \
    --in-dir workspace/fid_match/dedup \
    --out workspace/crt_callee_match/lib_bytes.json \
    --objs 046c7fbb59e1_errno.obj 71a26af32e1b_stk.obj ...

# 3. (Ghidra MCP) 跑 script 對每個 callee/parent 拉 FD2 byte，輸出 fd2_bytes.json
#    格式：{addr_hex8: {len, hex}}

# 4. 跑 candidate 篩選（出 markdown）
python tools/crt_callee_match/match_callees.py \
    --callees workspace/crt_callee_match/callees.json \
    --obj-funcs workspace/crt_callee_match/obj_funcs.json \
    --lookup rebuild_info/crt_lookup_9.5a.json \
    --out workspace/crt_callee_match/candidates.md

# 5. 跑 byte-level 比對（出 pass/fail report）
python tools/crt_callee_match/verify_pairs.py \
    --pairs workspace/crt_callee_match/pairs.json \
    --lib-bytes workspace/crt_callee_match/lib_bytes.json \
    --fd2-bytes workspace/crt_callee_match/fd2_bytes.json
```

## 輸入 JSON schema

`callees.json`（match_callees.py 輸入）：
```json
[
  {"addr": "0003d7f6", "size": 6,
   "callers": [{"addr": "00036e0d", "name": "__open_flags"}, ...]},
  ...
]
```

`pairs.json`（verify_pairs.py 輸入）：
```json
[
  {"label": "0003d7f6", "kind": "C", "fd2_start": "0x3d7f6",
   "fd2_len": 6, "lib_obj": "046c7fbb59e1_errno.obj",
   "lib_func": "__get_errno_ptr", "notes": "errno ptr getter"},
  ...
]
```

`fd2_bytes.json`（從 Ghidra 端產生）：
```json
{"0003d7f6": {"len": 6, "hex": "b8a4410500c3"}, ...}
```

## 何時用這套工具

當 FidDB 識別出來的 CRT function 之間呼叫關係出現「呼叫到非 CRT function」的 case，且該 callee 是 Ghidra 切錯（splitter false positive）或 lib 端有但 FidDB 沒對到（hash 太短或 PUBDEF 缺失）時，用這個 pipeline：
1. `match_callees.py` 收 candidate
2. `extract_obj_bytes.py` + `verify_pairs.py` 做 byte-identical 驗證
3. 結果整合進 `crt_lookup_9.5a.json`

注意：FixupLib byte 用 wildcard mask 處理；如果你看到 FAIL 但 mismatch 集中在特定 4-byte 區域，先檢查 FIXUPP 抽得對不對（quirky 32-bit fix 可能漏 patch）。
