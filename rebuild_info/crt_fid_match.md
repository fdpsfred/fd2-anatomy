# FD2.LE × Watcom CRT 識別

| 項目 | 結論 |
|---|---|
| FD2.LE 編譯器版本 | **Watcom C/C++ 9.5a** (DOS 32-bit DPMI) |
| 連結的 lib | **CLIB3S.LIB** (stack-call ABI) + **EMU387.LIB** + **GRAPH.LIB** |
| FD2.LE function 總數 | 1692 |
| 識別為 CRT 的 function 數 | 131 FidDB match + 13 補抓 = 140 |
| 主資料檔 | `crt_matches_9.5a.json`（FidDB raw）+ `crt_lookup_9.5a.json`（驗證後 140 entries） |
| 對照 fidb | `crt_fidb/watcom_<ver>.fidb` × 4 (9.5 / 9.5a / 9.5b / 9.5c) |

---

## 1. Watcom CRT lib 構成

Watcom 32-bit DOS 在 `LIB386/DOS/` 下提供：

| LIB | 內容 | ABI | 對 FD2 用否 |
|---|---|---|---|
| `CLIB3R.LIB` | 標準 C runtime | **R**egister-call (`__watcall`)：args in EAX/EDX/EBX/ECX | ✗ |
| `CLIB3S.LIB` | 標準 C runtime | **S**tack-call (`__cdecl`)：args on stack `[EBP+0x10/14/18]` | **✓** |
| `EMU387.LIB` | x87 FPU 模擬器 + helper | (無 ABI 變體) | ✓ |
| `GRAPH.LIB` | Watcom graphics primitives | (無 ABI 變體) | ✓ |

CLIB3R 與 CLIB3S 是並列的兩份 CRT — 同一個 `memcpy_` 在兩邊 byte-level
**完全不同**：register-call 版 prologue 是 `MOV EDI,EAX; MOV ESI,EDX;
MOV ECX,EBX`，stack-call 版是 `PUSH EBP; MOV EBP,ESP; MOV ECX,[EBP+0x18]
...`。FD2.LE 的 `memcpy @ 0x3cbd6` (42 byte) byte-identical 對到
CLIB3S 的 `memcpy.obj` (42 byte)，CLIB3R 的版本只有 37 byte 且 prologue
不同 → FD2 連結 CLIB3S，編譯選項相當於 `-ec=cdecl` 或 `-mf`。

每版 (9.5 / 9.5a / 9.5b / 9.5c) 三個 lib 共拆出約 2530 個 .obj，dedup
後 770 個 unique。

## 2. Ghidra Function ID 機制

Ghidra `FidService` 是兩階比對：

```
                每個 function 算 FidHashQuad
                (fullHash + specificHash + 兩個長度欄位)
                            │
                            ▼
            ┌───────────────────────────────┐
            │ stage 1: byte-level hash 相等  │  64-bit hash 完全相等
            └───────────────┬───────────────┘   不相等 → 沒 candidate
                            │
                            ▼
            ┌───────────────────────────────┐
            │ stage 2: score 加權             │  primary code units
            │  = self body × hash spec.       │  + matched parents body
            │  + matched parent units         │  + matched children body
            │  + matched child units          │
            └───────────────┬───────────────┘
                            │ score ≥ threshold (預設 14.6)
                            ▼
                       confirmed match
```

`FidHashQuad` 4 欄位是 (fullHash, codeUnitSize, specificHash,
specificHashAdditionalSize) — 即 2 個 hash + 2 個長度 metadata，並非
4 個 hash。parent / child 不是另外兩個 hash 而是 score 階段透過 FidDb
存的 caller / callee 關係表動態加分。

stage 1 對「未連結 lib .obj」與「已連結 binary」都成立，是因為
FidHasher 用 `Instruction.getReferences()` + `Program.getRelocationTable()`
把 reference operand byte (call / jmp 的 4-byte rel32、mem ref 的
4-byte abs32) 遮成 wildcard 後再 hash。lib .obj 的 reference 槽
是 0x00 + OMF FIXUPP 標記，linked binary 的 reference 槽是真正位址 +
Ghidra disassembler inferred reference，遮罩後產生相同 hash。

## 3. Watcom Easy OMF-386 quirky record

CLIB3S / GRAPH 的 770 個 dedup .obj 中 85 個含 Watcom 32-bit DOS
編譯器在 16-bit 段 (USE16) 用「16-bit OMF record type 配上 32-bit
length / offset 欄位」的 Watcom-only OMF 延伸。Ghidra OmfLoader 不接受
這個格式，會丟 `IllegalArgumentException: Invalid block name` 或
`IOException: Unable to read past EOF`。Watcom 的 wlib / wlink 接受。

| 標準 OMF | Watcom Easy OMF-386 quirky 寫法 | Patcher 改成 |
|---|---|---|
| 0x98 SEGDEF (length 2 byte) | 0x98 SEGDEF 配 length 4 byte | 0x99 SEGDEF32 |
| 0x90 PUBDEF (offset 2 byte) | 0x90 PUBDEF 配 offset 4 byte | 0x91 PUBDEF32 |
| 0xA0 LEDATA (offset 2 byte) | 0xA0 LEDATA 配 offset 4 byte | 0xA1 LEDATA32 |
| 0xA2 LIDATA (同) | 0xA2 LIDATA 配 offset 4 byte | 0xA3 LIDATA32 |
| 0x9C FIXUPP | 0x9C FIXUPP with 32-bit fields | 0x9D FIXUPP32 |
| 0x8A MODEND-16 | (有時候配 0x8B MODEND-32 預期格式) | 0x8B MODEND-32 |

`omf_patch_segdef.py` 偵測規則：任何 .obj 含 0x98 SEGDEF 且
`content[3] == 0` (在標準 2-byte length 解讀下會把這 byte 當成
`name_idx`，OMF spec 規定 `name_idx ≥ 1`，所以 `0` 代表那 byte 其實是
4-byte length 的高位)；命中即整檔升級全部 quirky record，重算每筆
OMF checksum (`sum mod 256 == 0`)。ACBP 的 P-bit (USE16/USE32 屬性)
不動，16-bit 段保留 16-bit 屬性。

770 .obj 經 patch 後成功 import 進 Ghidra 的數量是 **767/770**。

## 4. Ghidra LanguageID 對齊規則

FD2.LE 的 LanguageID 是 `x86:LE:32:watcom` — 非標準的 Ghidra 別名，
LanguageDescription 仍是 "Intel/AMD 32-bit x86" (跟 `x86:LE:32:default`
描述相同)，搭配 `compilerSpec=watcomcpp`。OmfLoader import 一個
.obj 時自動偵測為 `x86:LE:32:default`。

FidDb 比對要求 lib 端 (FunctionRecord 的 library 記錄) 與 target 端
program 的 LanguageID **完全相等**。處理：每個 import 完的 .obj
program 在 `FidImportBatch.java` 內呼叫 `Program.setLanguage(watcom,
watcomcpp, false, monitor)` 切到 `x86:LE:32:watcom`。兩個語言 SLA 完全
相同，setLanguage 是純改 ID label，無 disasm 變動。

## 5. 4 版本 fidb 對 FD2.LE match 結果

| 版本 | match 數 | fidb 內 hash 數 |
|---|---:|---:|
| 9.5 | 113 | 953 |
| **9.5a** | **131** | **958** |
| 9.5b | 124 | 984 |
| 9.5c | 119 | 985 |

Query 用 threshold = 0 拉全部 candidate；若改用 Ghidra 預設 14.6
門檻，9.5a 仍命中 106 筆，仍是 4 版中最多。

## 6. 9.5a 是 FD2 的編譯器版本

證據三層：

### 6.1 Superset 假設成立

9.5b、9.5c 的所有 match 都是 9.5a match 的子集，沒有反例 — 9.5a 的
958 個 hash 涵蓋了「FD2 binary 內所有可被任一版本 fidb 識別的 CRT
function」。

### 6.2 9.5a 獨有 match (其他 3 版都沒中)

| FD2 addr | 當前命名 | body | matched lib symbol | score |
|---|---|---:|---|---:|
| `0x37072` | `fread` | 466 | `fread` | **332.6** |
| `0x3d4f2` | `crt_heap_grow` | 512 | `__ExpandDGROUP` | **310.9** |
| `0x46e03` | `mktime` | 284 | `mktime` | **473.1** |

3 個全是大函式 + 高 score (`mktime` 的 473 分相當於 ≈300 個 code unit
吻合 + caller / callee 也命中)，純 short-function hash 巧合不可能解釋。

### 6.3 跨版本 .obj SHA256 比對

```
fread.obj:        9.5/9.5a   = hash A
                  9.5b/9.5c  = hash B (≠ A)
mktime.obj:       9.5         = hash C
                  9.5a       = hash D (≠ C)
                  9.5b/9.5c  = hash E (≠ D)
__ExpandDGROUP:   9.5/9.5a   = hash F
                  9.5b/9.5c  = hash G (≠ F)
```

`mktime` 的版本只在 9.5a 出現，其他三版各自不同；FD2.LE 同時匹中 `fread`
（9.5/9.5a 共用版本）、`mktime`（9.5a 獨有）、`__ExpandDGROUP`（9.5/9.5a
共用版本）這個組合，唯一相容版本是 9.5a。

### 6.4 9.5 比 9.5a 少 18 個 match 的組成

9.5 與 9.5a 共用大量 .obj 但不全部相同。差距 18 筆中 17 筆 score ≥
14.6 的 high-confidence match (主要是 9.5a 改寫過的 stream / time / 部分
helper)、1 筆 score 5–14.6、0 筆 score < 5；換算成預設門檻下 9.5 比
9.5a 少 17 個 high-confidence 命中。

## 7. 無法 import 的 3 個 .obj

| .obj | 來源 lib | 內容 | FD2 用否 |
|---|---|---|---|
| `fpeinth.obj` (1022B) | 9.5b/9.5c CLIB3S | x87 FPE interrupt handler (`__FPEHandler_`、`__FPE2Handler_`、`__Enable_FPE`)，安裝 SIGFPE 路由 | ✗ |
| `font8x8.obj` (3279B) | 9.5/GRAPH | IBM CP437 8×8 ROM 字型 bitmap (`__8x8Font`)，給 GRAPH lib 的 `_outtext` / `_grtext` 用 | ✗ |
| `font8x8.obj` (3302B) | 9.5a/9.5b/9.5c GRAPH | 同上 + `__8x8BitMap` (9.5a 起新增的 raw bitmap export) | ✗ |

3 個 .obj 都觸發 Ghidra OmfLoader 在處理完 MODEND 後仍試圖多讀
1 byte 的 EOF bug，patcher 的 0x8A→0x8B MODEND-32 升級救不了。

FD2 不連結它們的證據：
- `__FPE_handler` 沒 import，FD2 沒呼叫 `signal(SIGFPE, ...)` 註冊 handler
- FD2 用自家的中文字型基礎建設 (從磁碟讀漢堂 `STDFONT.15` (15×15 中文)
  + `ASCFONT.15` (8×15 半形)，VGA mode 13h 自家 sprite blitter)，不
  呼叫 GRAPH 的 `_outtext` / `_grtext`
- byte pattern 搜尋 — 笑臉 `7e 81 a5 81 bd 99 81 7e` (CP437 0x01)、FPE
  handler prologue `83 ec 04 9b d9 3c 24 9b 66 83 24 24` 在 FD2.LE 內
  都搜不到

故此 3 個 .obj 缺席不影響 CRT 識別與版本判定結果。

## 8. CRT 識別結果

### 8.1 `crt_matches_9.5a.json` schema

每筆 FD2 function 帶所有 candidate (Ghidra `FidQuery` 的 raw 輸出，
threshold=0)：

```jsonc
{
  "version": "9.5a",
  "threshold": 0.0,
  "match_count": 131,
  "matches": [
    {
      "address": "0003cbd6",        // FD2.LE function 入口
      "current_name": "memcpy",     // 當前 Ghidra 內的命名
      "body_size": 42,              // 函式大小 (bytes)
      "candidates": [
        {
          "matched_name": "memcpy", // lib 端的 PUBDEF symbol 名
          "score": 346.4,
          "source_obj": "/watcom_libs/<hash>_memcpy.obj",
          "library_family": "watcom_9.5a",
          "library_version": "9.5a",
          "library_variant": "DOS-32",
          "force_specific": false
        }
        // 多 candidate 出現於同 hash 對到多個 lib symbol 時
      ]
    }
  ]
}
```

### 8.2 高信心 match top 抽樣

| score | body | FD2 addr | matched lib symbol |
|---:|---:|---|---|
| 712.1 | 119 | `0x3d919` | `__ioalloc` |
| 616.1 | 157 | `0x3db16` | `__flush` |
| 591.7 | 112 | `0x462e2` | `_set_errno` |
| 522.4 | 82  | `0x46352` | `__IOMode` |
| 479.2 | 359 | `0x3744b` | `fwrite` |
| 473.1 | 284 | `0x46e03` | `mktime` |
| 441.0 | 171 | `0x3da65` | `__fill_buffer` |
| 421.4 | 52  | `0x4733d` | `__leapyear` |
| 387.3 | 473 | `0x3d095` | `write` |
| 380.4 | 114 | `0x36d26` | `_nmalloc` |
| 376.9 | 421 | `0x375f0` | `fseek` |
| 375.5 | 609 | `0x47371` | `__isindst` |
| 373.1 | 128 | `0x3d761` | `__fprtf` |
| 367.3 | 37  | `0x37426` | `_nfree` |
| 360.7 | 85  | `0x3fb90` | `_localtime` |
| 346.4 | 42  | `0x3cbd6` | `memcpy` |
| 332.6 | 466 | `0x37072` | `fread` |
| 310.9 | 512 | `0x3d4f2` | `__ExpandDGROUP` |

下方還有 ~110 筆 score < 300 的 match (含 `memmove` 52、`strlen` 55、
`strcpy` 23、`sprintf` 220、`fopen` 27、`fclose` 33、`malloc` 188、
`vfprintf` 373、`__prtf` 335、`putc` 321、`sopen` 320、`__brktime` 294、
`__brk` 273、`__qread` 265、`__doclose` 255、…等)。完整列表見 JSON。

### 8.3 低信心 match (score < 30) 實測誤判率

把門檻設在 score < 30 拉出 26 筆候選，逐筆對 disassembly + decompile +
callees + **callers** 與 Watcom CRT 預期行為比對後，實測誤判率
**4/26 = 15.4%**（細節見 §11.4 與 `crt_verify_rejected.md`）。誤判模式三種：

- **行為完全無關的 hash 碰撞**：4-instruction stub 在 reference 遮罩後
  hash 與某 CRT symbol 巧合（例 `0x353cc` 的 `delay(400)` wrapper 被標
  為 `fgetchar`，score 3.0）
- **same-family hash identical (immediate-distinguished)**：dosret.obj 內
  `__EINVAL` / `__EBADF` / `__ENOENT` 等 set-errno-X helper 的位元組型樣
  只在 32-bit immediate 上不同，遮罩後完全 identical；FidDb 從 family 中
  挑一個 symbol 名作為標籤，例 `0x462d1` 真實是 `__EBADF`（store errno=9）
  但被標為 `__EINVAL`（store errno=0x16）
- **same-family hash identical (caller-distinguished)**：CLIB3S 內若干
  default-zero stub（`PUSH EBP; MOV EBP,ESP; XOR EAX,EAX; POP EBP; RET`
  共 7 byte）byte-identical 跨多個 .obj — 例如 `__nmemneed` 預設 stub 與
  signal subsystem 的 default zero callback。FidDb 標其中一個 symbol，
  唯一可區分的是 caller 路徑：真 `__nmemneed` 被 `_nmalloc` (heap path)
  呼叫；signal default 被 `crt_signal_handler_print` 路徑觸達。例
  `0x4d8ea` 標為 `__nmemneed` 但 caller 是 signal 路徑而非 heap，故為
  false positive
- **thin-wrapper hash 巧合**：`PUSH imm + CALL helper + RET` 這類短
  pattern 在多個短 CRT 函式間 hash 同型；例 `0x4694c` body 11 與
  `fcloseall` 簽名碰撞但實際是 close-streams-with-mask wrapper 的某個
  變體

22 筆 PASS 的 score < 30 entries 多半是 Watcom CRT 內合法的 thin
wrapper（abs/labs 14、outp 12、toupper 21、__nmemneed 7、ctime 25、
freopen 44、fopen 21、_dosret0 24 等）— 它們 score 低是因為 FidHasher
對短函式分配的 code-unit 分數自然偏低（待 score 的 instruction 少），
不是因為 hash 巧合。

判斷準則：**body size + 行為簽章 + caller 路徑** 三檢查。`__nmemneed`
案例證明前兩條件不夠（0x4d8ea body 7 + leaf XOR-RET 完全 fit signature
但 caller 是 signal 不是 heap → 仍是 false positive）。caller 路徑是
discriminator of last resort for hash-identical helper families。詳見
`tools/crt_fid_match/verify_rules.py` 內 RULES 表（含 `callers_required_any`
等欄位）與 `apply_rule()` 套用器。

## 9. Pipeline 步驟與工具

| # | 動作 | 工具 |
|---|---|---|
| 1 | 拆 12 個 .lib 成 .obj (4 版本 × 3 lib) | `tools/crt_fid_match/omf_lib_extract.py` (`wlib -q -x` wrapper) |
| 2 | 建跨版本 dedup 索引 (770 unique .obj) | `build_manifest.py` + `build_dedup_dir.py` |
| 3 | 修補 Watcom Easy OMF-386 quirky record (85/770 命中) | `omf_patch_segdef.py --in-place` |
| 4 | Ghidra 端清空目標 folder | `ghidra_scripts/FidWipeFolder.java` |
| 5 | Ghidra batch import .obj，含 setLanguage 到 watcom | `ghidra_scripts/FidImportBatch.java` |
| 6 | 對 import 的 program 跑 auto-analysis | `ghidra_scripts/FidAnalyzeAll.java` |
| 7 | 建 4 個 .fidb (依版本) | `ghidra_scripts/FidPopulate.java` |
| 8 | 對 FD2.LE 跑 4 次 query | `ghidra_scripts/FidQuery.java` |
| 9 | 跨版本比較、出 markdown 報告 | `compare_results.py` |

## 10. 檔案清單

`rebuild_info/` 下：

| 檔案 | 說明 |
|---|---|
| `crt_fid_match.md` | 本檔 |
| `crt_matches_9.5a.json` | FidQuery 對 9.5a fidb 的 raw 輸出 (131 個 match) |
| `crt_lookup_9.5a.json` | 經行為驗證後的 address ↔ Watcom CRT symbol 對照表 (§11) |
| `crt_verify_report.md` | Phase B + Phase D 31 個受驗 entry 的逐筆紀錄 |
| `crt_verify_rejected.md` | 3 個未通過 candidate 的拒絕原因 |

`tools/crt_fid_match/` 下：

| 檔案 | 說明 |
|---|---|
| `omf_lib_extract.py` | wlib `-q -x` wrapper，把 .lib 拆成 .obj |
| `omf_patch_segdef.py` | 修 Watcom Easy OMF-386 quirky record (§3) |
| `build_manifest.py` | 建跨版本 dedup 索引 |
| `build_dedup_dir.py` | 把 dedup 後的 .obj 集中到一個 flat dir |
| `compare_results.py` | 從 4 份 query JSON 生 markdown 比對報告 |
| `build_crt_lookup.py` | 從 `crt_matches_9.5a.json` 分流 4 個 verify queue (§11) |
| `verify_rules.py` | 每個 Watcom CRT symbol 的行為簽章規則表 (§11) |
| `verify_crt_samples.py` | 套 verify_rules 對受驗 entry 出 PASS/FAIL 報告 (§11) |
| `build_final_lookup.py` | 整合 auto + manual + 衝突解決 → `crt_lookup_9.5a.json` (§11) |
| `ghidra_scripts/FidWipeFolder.java` | 清 Ghidra project folder |
| `ghidra_scripts/FidImportBatch.java` | batch import .obj，含 setLanguage |
| `ghidra_scripts/FidAnalyzeAll.java` | 對 import 的 program 跑 auto-analysis |
| `ghidra_scripts/FidPopulate.java` | 從 program 集合建 .fidb |
| `ghidra_scripts/FidQuery.java` | 對 target program 跑 query → JSON |
| `crt_fidb/watcom_9.5.fidb` | 9.5 Function ID database (113693 byte) |
| `crt_fidb/watcom_9.5a.fidb` | 9.5a (115002 byte) — **正本** |
| `crt_fidb/watcom_9.5b.fidb` | 9.5b (117574 byte) |
| `crt_fidb/watcom_9.5c.fidb` | 9.5c (117087 byte) |

## 11. 已驗證 lookup table

`crt_lookup_9.5a.json` 是 §8 raw FidQuery 輸出經行為驗證 + §12 callee 比對
補抓後的精煉版，收 **140 個確認的** FD2 function ↔ Watcom CLIB3S symbol 對照
（127 FidDB-driven + 13 byte-level callee match），作為後續 rename audit /
calling convention 補齊 / CRT 行為復刻工作的快速查表來源。

### 11.1 Schema

```jsonc
{
  "version": "9.5a",
  "auto_threshold": 30.0,
  "stats": { "total_input": 131, "auto_pass": 102,
             "conflict_resolved": 3, "manual_pass": 22, "rejected": 4,
             "in_lookup": 127 },
  "by_address": {
    "0003cbd6": {
      "name":         "memcpy",       // Watcom lib PUBDEF 名（主鍵）
      "current_name": "memcpy",       // Ghidra 內現有名（audit 輔欄）
      "score":        346.4,
      "body_size":    42,
      "source_obj":   "59aa526358b1_memcpy.obj",
      "verified":     "auto_threshold",
      "aliases":      []              // 同 addr 多 candidate 時填
    }
  },
  "by_name": { "memcpy": ["0003cbd6"], "abs": ["000375e2"], "labs": ["000375e2"] }
}
```

`name` 一律以 Watcom lib PUBDEF 為主、`current_name` 保留 Ghidra 內名以利
audit。0x375e2 abs/labs 是 32-bit Watcom 唯一同 addr 同 score 雙 candidate
案，`aliases` 列雙名、`by_name` 兩個 key 反向索引到同一 addr。`__nmemneed`
有 2 個 addr (`0x3d6f2` / `0x4d8ea`) 因 binary 內存在 weak stub 雙拷貝，
`by_name["__nmemneed"]` 雙條目反映這個事實。

### 11.2 收錄條件

| verified | 條件 | 數量 |
|---|---|---:|
| `auto_threshold` | score ≥ 30 且 current_name 與 matched_name 無語意衝突 | 102 |
| `conflict_resolved` | score ≥ 30 但 current_name 與 matched_name 衝突；經行為驗證確認 matched_name 才正確 | 3 |
| `manual` (FidDB) | score < 30，逐筆套 verify_rules 行為簽章規則（含 caller 路徑檢查）PASS | 22 |
| `manual` (callee match) | FidDB 沒抓到（hash 太短或 lib 端無 PUBDEF），用 §12 byte-level pipeline 補抓 PASS | 13 |
| _rejected_ | 行為與 matched_name 不符；不入表，僅紀錄於 `crt_verify_rejected.md` | 3 |

`auto_threshold = 30` 由「在 [20, 712.1] 範圍按等距取 10 個樣本驗證
(10/10 PASS)」+「對 score < 30 的 26 筆全部逐筆 caller-aware 驗證」共同
建立。Threshold 從 20 提升到 30 的觸發點是 `__nmemneed` family 的 caller
分析發現 — 證明 score-only 對 hash-identical helper family 不夠強，需用
caller 路徑作為最終 discriminator。

3 個 conflict_resolved（`0x36dc1 printf` / `0x3dbe7 remove` / `0x46a80
unlink`）的 Ghidra 現有名 (`crt_fprintf_stderr` / `crt_putc_tty` /
`crt_putc_dos`) 是先前命名者誤判：例如 0x36dc1 push 的 stream constant
0x5285a 經 `workspace/ail_audit/crt_globals_map.json` 計算
`(0x5285a − 0x52840) / 0x1A = 1` 證明是 __iob[1] = stdout 不是 stderr。

### 11.3 驗證規則

`tools/crt_fid_match/verify_rules.py` 內 `RULES` 對 60+ Watcom CRT symbol
各寫一條 PASS criteria，由六類條件 AND 起來：

- `body_size_range` — function body 落在 [lo, hi] 之內
- `callees_required_any` / `_all` / `_forbidden` — callee 名單檢查（callee
  地址需先用 `crt_matches_9.5a.json` 翻譯回 Watcom 符號）
- `callers_required_any` / `_all` / `_forbidden` — caller 路徑檢查
  （discriminator of last resort for hash-identical helper families；例
  `__nmemneed` rule 要求 caller 含 `_nmalloc` 才 PASS）
- `instructions_any` / `_all` — assembly 內必須出現的指令模式（如
  `STOSB` / `MOVSB` / `IDIV` / `OUT`）
- `int21_ah_any` — DOS INT 21h 的 AH 值（如 unlink 是 0x41，getch 是 0x08）
- `is_leaf` — 是否為 leaf function

任一子條件 fail 即整體 fail；不開「callee 名 plausible 即過」的後門。
`apply_rule()` 回傳 failure 列表，空 list 才 PASS。observation 中可加
`manual_verdict: "REJECT" + manual_reason` 顯式覆寫規則結果，用於規則
PASS 但語意實際不符的案例（例如 dosret.obj 內 set-errno-X helper family
因 imm 遮罩 hash identical，需用實際 errno 立即值區分）。

### 11.4 四個 rejected 案例

- **`0x353cc` matched=fgetchar** — 實際 `PUSH 0x190; CALL delay; RET`，是
  delay(400ms) wrapper；fgetchar 應 `fgetc(stdin)`，行為完全無關。score
  3.0 是 4-instruction stub 巧合命中。caller 是 game cinematic function，
  非 stdio 路徑
- **`0x462d1` matched=__EINVAL** — 實際 store `errno = 9`，但 Watcom errno.h
  定義 EBADF=9 / EINVAL=22 (0x16)；故為 `__EBADF` helper，FidDb 因
  dosret.obj 內 set-errno-X family（CALL get_errno_ptr; MOV [EAX],imm32;
  MOV EAX,-1; RET）在 imm 遮罩後位元組型樣 identical 而誤標。同 family
  case 用 immediate value 區分
- **`0x4694c` matched=fcloseall** — body 11 的 thin wrapper（PUSH 0x5;
  CALL helper; RET），未 loop _iob[]；與 fcloseall 標準實作（迴圈 fclose
  每個 open 流）不符；score 4.34 低於默認 14.6 門檻
- **`0x4d8ea` matched=__nmemneed** — body 7 與真 `__nmemneed` (0x3d6f2)
  byte-identical 的 `XOR EAX,EAX; RET` stub，且 score 5.0、leaf、body
  fit — 完全 fit `__nmemneed` rule 的 instruction-level signature。但
  caller 分析顯示 0x4d8ea 唯一 inbound chain 是 `crt_signal_handler_print`
  經 `0x4d340` (一條 `JMP 0x4d8ea` thunk) 進入，不是 heap allocator
  路徑。真 `__nmemneed` (0x3d6f2) 由 `_nmalloc` 直接呼叫。同 7-byte
  XOR-RET pattern 在 CLIB3S 內被 nmemneed.obj 與 signal subsystem
  default callback 重複使用，hash 完全 identical，唯一可區分的是 caller
  路徑。Ghidra 內 0x4d8ea 改名為 `noop_stub_4d8ea_zero` 與 sibling
  thunk `noop_stub_4d340_zero` 對齊

### 11.5 重產流程

```bash
# 1. 從 raw FidQuery 結果分流出 verify queue（不需 Ghidra；threshold 默認 30.0）
python tools/crt_fid_match/build_crt_lookup.py

# 2. 在 Ghidra MCP 環境收集 disasm + callees + callers 寫成 observations JSON
#    每筆 entry: {callees, callers, asm, key_instructions, notes,
#                 manual_verdict?, manual_reason?}
#    callees / callers 名稱需用 Watcom matched_name (Ghidra 內 callee 地址
#    用 crt_matches_9.5a.json 翻譯)。此步由 Claude Code 完成，不能 EXE-execute

# 3. 套規則出 verify report (sample queue 是 threshold validation only,
#    不入 final report)
python tools/crt_fid_match/verify_crt_samples.py \
    --queue workspace/crt_fid_match/conflict_queue.json \
    --queue workspace/crt_fid_match/manual_queue.json \
    --observations workspace/crt_fid_match/observations_all.json \
    --out rebuild_info/crt_verify_report.md

# 4. 整合成最終 lookup
python tools/crt_fid_match/build_final_lookup.py
```

## 12. Callee-driven 補抓（FidDB 漏抓的 small helper / split fragment）

§11 lookup 完成後，對「已識別 CRT function 之間的呼叫關係」做反向 audit：
若 callee 不在 lookup 內，要嘛是 Ghidra 把一個 lib function 切成多塊（splitter
false positive），要嘛是 lib 端的 small helper（例如 `__get_errno_ptr` 6 byte）
被 FidDB 因 hash 過短而漏抓，要嘛是 lib 端 anonymous static（無 PUBDEF，FidDB
本來就不收）。

對 FD2.LE 跑這個 audit 找出 28 個未識別 callee。經 byte-level 比對全部 PASS：

- **14 個 split fragment**（Ghidra 把一個 lib function 切成 parent + tail；
  parent 的 lookup `body_size` 比 lib total 小，差額 = tail fragment size）。
  典型案例：`__open_flags` lookup 175 + tail 0x36ebc 229 = lib 404 ✓；
  `_DoINTR_` lookup 74 + 內部多個 chunk = lib 893 ✓（內含 256 個 INT 0x00..0xff
  jump-table entry，3 byte 一個）；`__int7` lookup 28 + 多 chunk = lib 11830 ✓
  （x87 emulator 主體）。處置：刪掉 fragment、用 `Function.setBody()` 把
  parent body 強制延伸到 lib total
- **13 個獨立 function 補入 lookup**：`exit` / `_exit` / `__get_errno_ptr` /
  `__get_doserrno_ptr` / `__STKOVERFLOW` / `stackavail` / `getpid` /
  `__CommonInit` / `fcloseall`（從 rejected reinstate）/ `__GRO`（stk.obj 第三
  個 PUBDEF）+ 3 個 lib 端 anonymous static（lib 標 `L$1`，合成命名為
  `L$1_<obj>_<purpose>`：`L$1_stk_save_ss` / `L$1_rand_seed_ptr` /
  `L$1_asctime_fmt2`）

驗證方法：對每筆 (FD2 byte range, lib .obj function)，用 lib FIXUPP 標記的
reference 位置遮罩，遮罩外 byte 必須完全相等。Pipeline 在
`tools/crt_callee_match/`（見該目錄 `_index.md`）。

### 12.1 發現的 OMF parser quirk

Ghidra OmfLoader 的 §3 quirky-record 修補只是入門。本次補寫的
`extract_obj_bytes.py` 還補上兩個 byte-level 比對才會踩到的細節：

- **SEGDEF USE32 判斷**：32-bit segment 由 record type 0x99（或 quirky 0x98）
  標示，**不能**靠 ACBP P-bit。實測 stk.obj 所有 SEGDEF 的 P-bit 都是 0，
  但這些 segment 全部是 USE32（從程式內 32-bit 暫存器使用可推斷）
- **FIXUPP LOCAT field 解析**：byte0 = `1 M LLLL OO`（M=mode bit6,
  location_type=bits5..2, offset hi=bits1..0），byte1 = offset low 8 bit；
  total offset 是 10-bit 不是 12-bit
- **Fixup width**：32-bit segment 內 `location_type=1` / `5` 是 4-byte fixup
  （標準 OMF 是 2-byte）；要從 SEGDEF type / quirky 推得，否則 mask 抓錯位置
  造成 byte 比對誤判

### 12.2 Jump table 漏抓警示

A 類 14 個 fragment 都是 Watcom 9.5a 對 ≥4 case `switch` 編成 jump table 後
Ghidra 沒識別 jump table 造成的。Ghidra 沿著直接 control flow 只能 trace 到
第一條 RET 路徑，從 jump table 進入的 case body 全部漏掉。

實作 fix：刪 fragment + `Function.setBody(AddressSet)` 強制把所有 byte 塞回
parent body。Ghidra 的 control-flow analysis 不會自動延伸（`removeFunction`
不會 trigger 重 analyze），必須手動 setBody。

對遊戲端 / AIL 端的影響：同樣的 jump-table 漏抓也會發生在遊戲程式（章節
event dispatcher / 戰鬥 AI / FDFIELD opcode interpreter）和 AIL 函式。建議
未來 AIL 抽 lib 工作開始前先做一次「indirect-JMP target 是否落在 body 外」
的全域 audit。

### 12.3 非 CLIB3S 函式

`0x45fb6` 在 `__FiniRtns` 的 finalize table 內登記為 callback（`__FiniRtns` →
0x3cbd1 5-byte JMP thunk → 0x45fb6 122 byte function）。byte 內含 `B4 F3 CD 21`
(`MOV AH,0xF3; INT 21h`)。掃過 CLIB3S / CLIB3R 的 770 / 770 個 dedup .obj 都
找不到此 byte sequence — 證實 `0x45fb6` **不是 Watcom CRT function**。

`INT 21h AH=0xF3` 是 Phar Lap 386|DOS-Extender 的 "Switch to real mode" service。
故 0x45fb6 屬於 FD2.LE 連結進來的 Phar Lap LE runtime（不是 Watcom CLIB），
不收進 `crt_lookup_9.5a.json`。後續若要識別 Phar Lap runtime 函式需要另一份
fidb（建在 `tools/` 下另開資料夾）。
