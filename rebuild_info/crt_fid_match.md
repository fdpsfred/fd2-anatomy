# FD2.LE × Watcom CRT 識別

| 項目 | 結論 |
|---|---|
| FD2.LE 編譯器版本 | **Watcom C/C++ 9.5a** (DOS 32-bit DPMI) |
| 連結的 lib | **CLIB3S.LIB** (stack-call ABI) + **EMU387.LIB** + **GRAPH.LIB** |
| FD2.LE function 總數 | 1692 |
| 識別為 CRT 的 function 數 | 131，其中 106 筆 score ≥ 14.6 (Ghidra 預設信心門檻) |
| 主資料檔 | `crt_matches_9.5a.json` |
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

### 8.3 低信心 match (score < 14.6) 判讀準則

threshold=0 拉出來的 12 筆 score < 14.6 中約一半是真誤判（短函式
hash 碰撞），一半實際對（函式本來就只有 5–10 條 instruction）。

判斷準則：**body size 跟該 lib symbol 預期實作大小是否吻合**。

| addr | 當前命名 | body | 命中 (score) | 判斷 |
|---|---|---:|---|---|
| `0x4694c` | `crt_helper_4694c` | 11 | `fcloseall` (4.3) | ✗ 假 — `fcloseall` 真正實作數十 byte，不可能只有 11 byte |
| `0x4d8ea` | `crt_helper_4d8ea` | 7 | `__nmemneed` (5.0) | ✓ 真 — `__nmemneed` 預設 stub 就是 `xor eax,eax; ret` 的 4–7 byte |
| `0x462d1` | `crt_helper_462d1` | 17 | `__EINVAL` (4.3) | ✓ 真 — set-errno helper，body size 吻合 |
| `0x37795` | `outp` | 12 | `outp` (8.3) | ✓ 真 — 名字本來就同步 |

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

`tools/crt_fid_match/` 下：

| 檔案 | 說明 |
|---|---|
| `omf_lib_extract.py` | wlib `-q -x` wrapper，把 .lib 拆成 .obj |
| `omf_patch_segdef.py` | 修 Watcom Easy OMF-386 quirky record (§3) |
| `build_manifest.py` | 建跨版本 dedup 索引 |
| `build_dedup_dir.py` | 把 dedup 後的 .obj 集中到一個 flat dir |
| `compare_results.py` | 從 4 份 query JSON 生 markdown 比對報告 |
| `ghidra_scripts/FidWipeFolder.java` | 清 Ghidra project folder |
| `ghidra_scripts/FidImportBatch.java` | batch import .obj，含 setLanguage |
| `ghidra_scripts/FidAnalyzeAll.java` | 對 import 的 program 跑 auto-analysis |
| `ghidra_scripts/FidPopulate.java` | 從 program 集合建 .fidb |
| `ghidra_scripts/FidQuery.java` | 對 target program 跑 query → JSON |
| `crt_fidb/watcom_9.5.fidb` | 9.5 Function ID database (113693 byte) |
| `crt_fidb/watcom_9.5a.fidb` | 9.5a (115002 byte) — **正本** |
| `crt_fidb/watcom_9.5b.fidb` | 9.5b (117574 byte) |
| `crt_fidb/watcom_9.5c.fidb` | 9.5c (117087 byte) |
