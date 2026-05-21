# FD2.LE × Watcom CRT 識別

| 項目 | 結論 |
|---|---|
| FD2.LE 編譯器版本 | **Watcom C/C++ 9.5a** (DOS 32-bit DPMI) |
| 連結的 lib | **CLIB3S.LIB** (stack-call ABI) + **EMU387.LIB** + **GRAPH.LIB** + **MATH387S.LIB** |
| DOS extender | **DOS/4G** (`BINW/dos4gw.exe` as stub，FD2.LE strings 含 `RATIONAL DOS/4G`，無 Phar Lap) |
| 識別為 CRT 的 function 數 | **193**（102 auto_threshold + 3 conflict_resolved + 35 manual + 52 byte_match + 1 byte_match_disputed）|
| 主資料檔 | `lookup_9.5a.json` (address ↔ Watcom symbol 對照，193 entries，含 source_libs 欄位) |

完整 address ↔ symbol ↔ source_obj 對照見 `lookup_9.5a.json` 與 human-readable
view `matched_function_sources.md`。FidDB query / OMF quirky record patcher
pipeline 細節落地於 `tools/program_analysis/crt_fid_match/`；byte_match audit / sliding-window
補抓 / splitter false positive 修復 / 132 個 AIL DIG mixer callback 命名等
audit 結果已寫回 Ghidra plate comment 與 `lookup_9.5a.json` 內。

## Watcom CRT lib 構成

Watcom 32-bit DOS 在 `LIB386/DOS/` 下提供：

| LIB | 內容 | ABI | 對 FD2 用否 |
|---|---|---|---|
| `CLIB3R.LIB` | 標準 C runtime | **R**egister-call (`__watcall`) | ✗ |
| `CLIB3S.LIB` | 標準 C runtime | **S**tack-call (`__cdecl`) | **✓** |
| `EMU387.LIB` | x87 FPU 模擬器 + helper | — | ✓ |
| `GRAPH.LIB` | Watcom graphics primitives | — | ✓ |
| `MATH3R/S.LIB` | 純 forward stub（≤2 KB），實際指向 CLIB3R/S | — | ✗（空殼） |
| `MATH387R.LIB` / `MATH387S.LIB` | x87 + softfp 數學函式 (sqrt/sin/log/strtod/cvt) + DOS extender 8087 emulator init/fini（`dosinite.obj`） | 與 R/S 對齊 | **✓** (`MATH387S.LIB`) |
| `PLIB*` / `PLBX*` / `CPLX*` | C++ stream / exception / complex / pcomp | — | ✗（FD2 是純 C） |
| `NOEMU387.LIB` | 無模擬版 387 stub | — | ✗ |
| `ADIESTRT / ADIFSTRT / ADSSTART.OBJ` | AutoCAD ADI/ADS target startup | — | ✗ |

FD2 連結 CLIB3S 的證據：`memcpy @ 0x3cbd6` (42 byte) byte-identical 對到
CLIB3S `memcpy.obj` (42 byte)；CLIB3R 版本只 37 byte 且 prologue 不同。
編譯選項相當於 `-ec=cdecl` 或 `-mf`。

DOS extender：FD2.LE 經 `wlink ... -l=dos4g` 連結 + Watcom 9.5a
`BINW/dos4gw.exe` 作 stub。所有 LE-side runtime 都從上述 Watcom static lib
解析（cstart.obj、dosinite.obj、chk8087.obj 等），DOS/4G **沒有獨立 client
static lib** — extender 本體 dos4gw.exe 在執行時動態載入。

## 9.5a 是 FD2 的編譯器版本

對 4 版本 fidb (9.5 / 9.5a / 9.5b / 9.5c) 跑 FidQuery，9.5a 命中數最多
（131 高分 match），且 9.5b / 9.5c 的所有 match 都是 9.5a match 的子集。
跨版本 .obj SHA256 比對下，FD2.LE 內 `fread @ 0x37072` / `mktime @ 0x46e03`
/ `__ExpandDGROUP @ 0x3d4f2` 三個函式只命中 9.5a 獨有 hash；此三筆組合
無法由 9.5 / 9.5b / 9.5c 任一版本同時滿足，唯一相容版本是 9.5a。

## 無法 import 的 3 個 .obj

`fpeinth.obj` (x87 FPE handler)、`font8x8.obj`×2 (CP437 8×8 字型) 因
Ghidra OmfLoader EOF bug 無法 import。三個都不影響 FD2 識別 — FD2 沒
註冊 SIGFPE handler、沒呼叫 GRAPH 的 `_outtext` / `_grtext` (FD2 用自家
中文字型 STDFONT.15 / ASCFONT.15)；byte pattern 搜尋在 FD2.LE 內也都搜
不到對應 signature。

## Lookup table

`lookup_9.5a.json` schema 摘要：

```jsonc
{
  "by_address": {
    "0003cbd6": {
      "name":         "memcpy",             // Watcom lib PUBDEF 名（主鍵）
      "current_name": "memcpy",             // Ghidra 內現有名
      "score":        346.4,
      "body_size":    42,
      "source_obj":   "59aa526358b1_memcpy.obj",
      "verified":     "auto_threshold",     // auto_threshold / conflict_resolved
                                            // / manual / byte_match
      "source_libs":  [ {version, lib, module}, ... ],
      "aliases":      [],
      "notes":        "..."
    }
  },
  "by_name": { "memcpy": ["0003cbd6"], ... }
}
```

`verified` 5 種來源：

| verified | 條件 | 數量 |
|---|---|---:|
| `auto_threshold` | FidDB score ≥ 30 且 current_name 與 matched_name 無語意衝突 | 102 |
| `conflict_resolved` | FidDB score ≥ 30，name 衝突經行為驗證 | 3 |
| `manual` | FidDB score < 30 / 漏抓 / callee match 等逐筆讀 disasm + callees 後 PASS | 35 |
| `byte_match` | FIXUPP-aware byte-exact 比對全 LIB386 1548 obj × 7040 lib func 後 PASS | 52 |
| `byte_match_disputed` | byte_match 命中但語意 disputed（如 immediate masking 後 PUSH/PUSH/JMP 多 obj 通用） | 1 |

**所有 FIXUPP-aware byte-match 比對**（lib FIXUPP 標記的 reference 位置遮罩、
遮罩外完全相等）；caller 路徑作為 hash-identical helper family 的最終
discriminator（例 `__nmemneed` 與 `_matherr` default-return-zero stub
byte-identical，唯一可區分的是 caller 路徑：真 `__nmemneed` 被 `_nmalloc`
呼叫，0x4d8ea 唯一 caller 是 `_matherr → matherr_default_thunk`，故為
`crt_equivalent_matherr_default_return_zero_4d8ea`）。

## 命名規範

- Watcom 真符號 → 與 lib PUBDEF byte-identical（`malloc` / `memset` /
  `__filbuf` / `IF@COS` / `__CHK` 等）
- Anonymous static (lib 端 `L$1`) → `L$N_<obj>_<purpose>`
  （例 `L$1_stk_save_ss` / `L$1_sprintf_put_char` /
  `L$1_ioexit_close_streams_with_mask`）
- Watcom CRT 行為等價但 byte 不 match → `crt_equivalent_*`（emit FD2 source）
- FD2 自寫的 CRT-style primitive（DPMI wrapper 等）→ `fd2_*`（emit FD2 source）
