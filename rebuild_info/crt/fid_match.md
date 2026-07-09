# FD2.LE × Watcom CRT 識別

| 項目 | 結論 |
|---|---|
| FD2.LE 編譯器版本 | **Watcom C/C++ 9.5a**（DOS 32-bit DPMI）|
| 連結的 lib | **CLIB3S.LIB**（stack-call ABI）+ **EMU387.LIB** + **GRAPH.LIB** + **MATH387S.LIB** |
| DOS extender | **DOS/4G**（FD2.LE strings 含 `RATIONAL DOS/4G`，無 Phar Lap；連結/stub/layout 機制見 `rebuild_info/link/wlink_settings.md` 與 `rebuild_info/link/le_layout.md`）|
| 識別為 CRT 的 function | `lookup_9.5a.json` 收錄 194 筆（102 auto_threshold + 3 conflict_resolved + 36 manual + 52 byte_match + 1 byte_match_disputed）|
| 主資料檔 | `lookup_9.5a.json`（address ↔ Watcom symbol 對照，含 `source_libs` 欄）|

完整 address ↔ symbol ↔ source_obj 對照見 `lookup_9.5a.json` 與其 generated view
`matched_function_sources.md`。識別 pipeline（FidDB query / OMF quirky record
patcher / byte_match audit）的操作細節見
`tools/program_analysis/crt_fid_match/_index.md`；audit 結論（含 byte_match 覆核、
AIL DIG mixer callback 命名）已寫回 Ghidra plate comment 與 `lookup_9.5a.json`。

## Watcom CRT lib 構成

Watcom 32-bit DOS 在 `LIB386/DOS/` 下提供：

| LIB | 內容 | ABI | 對 FD2 用否 |
|---|---|---|---|
| `CLIB3R.LIB` | 標準 C runtime | **R**egister-call (`__watcall`) | ✗ |
| `CLIB3S.LIB` | 標準 C runtime | **S**tack-call (`__cdecl`) | **✓** |
| `EMU387.LIB` | x87 FPU 模擬器 + helper | — | ✓ |
| `GRAPH.LIB` | Watcom graphics primitives | — | ✓ |
| `MATH3R/S.LIB` | 純 forward stub（≤2 KB），實際指向 CLIB3R/S | — | ✗（空殼）|
| `MATH387R.LIB` / `MATH387S.LIB` | x87 + softfp 數學函式（sqrt/sin/log/strtod/cvt）+ DOS extender 8087 emulator init/fini（`dosinite.obj`）| 與 R/S 對齊 | **✓**（`MATH387S.LIB`）|
| `PLIB*` / `PLBX*` / `CPLX*` | C++ stream / exception / complex / pcomp | — | ✗（FD2 是純 C）|
| `NOEMU387.LIB` | 無模擬版 387 stub | — | ✗ |
| `ADIESTRT / ADIFSTRT / ADSSTART.OBJ` | AutoCAD ADI/ADS target startup | — | ✗ |

FD2 連結 CLIB3S 的證據：`memcpy @ 0x3cbd6`（42 byte）byte-identical 對到 CLIB3S
`memcpy.obj`（42 byte），而 CLIB3R 版本只 37 byte 且 prologue 不同；編譯選項相當於
`-ec=cdecl` 或 `-mf`。DOS/4G 沒有獨立 client static lib，所有 LE-side runtime 都從
上述 Watcom static lib 解析（cstart.obj、dosinite.obj、chk8087.obj 等），extender
本體 dos4gw.exe 於執行時動態載入。

## 9.5a 是 FD2 的編譯器版本

跨版本 .obj SHA256 比對下，FD2.LE 內 `fread @ 0x37072` / `mktime @ 0x46e03` /
`__ExpandDGROUP @ 0x3d4f2` 三個函式只命中 9.5a 獨有 hash；此三筆組合無法由 9.5 /
9.5b / 9.5c 任一版本同時滿足，唯一相容版本是 9.5a。四版本 fidb（9.5 / 9.5a /
9.5b / 9.5c）的 FidQuery 命中中，9.5a 命中數最多，且 9.5b / 9.5c 的 match 都是
9.5a match 的子集。`MATH387S/dosinite.obj`（`__sys_init_387_emulator` /
`__sys_fini_387_emulator`）與 `MATH387S/ftos.obj`（`_FtoS`）的 obj 只出現在
9.5+9.5a，與上述三個 9.5a-only 函式一併把版本收斂到 9.5a。

## 無法 import 的 3 個 .obj

`fpeinth.obj`（x87 FPE handler）、`font8x8.obj` ×2（CP437 8×8 字型）因 Ghidra
OmfLoader EOF bug 無法 import，但都不影響 FD2 識別：FD2 沒註冊 SIGFPE handler、
沒呼叫 GRAPH 的 `_outtext` / `_grtext`（FD2 用自家中文字型 STDFONT.15 /
ASCFONT.15），byte pattern 搜尋在 FD2.LE 內也搜不到對應 signature。

## Lookup table

`lookup_9.5a.json` schema 摘要：

```jsonc
{
  "by_address": {
    "0003cbd6": {
      "name":         "memcpy",             // Watcom lib PUBDEF 名（主鍵）
      "current_name": "memcpy",             // Ghidra 內現有名
      "score":        346.35,
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

`verified` 標示每筆的驗證來源，收錄門檻如下：

| verified | 條件 | 數量 |
|---|---|---:|
| `auto_threshold` | FidDB score ≥ 30 且 current_name 與 matched_name 無語意衝突 | 102 |
| `conflict_resolved` | FidDB score ≥ 30，name 衝突經行為驗證 | 3 |
| `manual` | FidDB score < 30 / 漏抓 / callee match 等逐筆讀 disasm + callees 後 PASS | 36 |
| `byte_match` | FIXUPP-aware byte-exact 比對全 LIB386 obj × lib func 後 PASS | 52 |
| `byte_match_disputed` | byte_match 命中但語意 disputed（如 immediate masking 後 PUSH/PUSH/JMP 多 obj 通用）| 1 |

byte-match 一律 FIXUPP-aware：對 lib FIXUPP 標記的 reference 位置遮罩、遮罩外完全
相等。當多個 hash-identical 的 helper family 無法只靠 byte 區分時，caller 路徑是
最終 discriminator——例如 `__nmemneed` 與 `crt_equivalent_matherr_default_return_zero`
stub byte-identical，唯一可區分的是 caller：真 `__nmemneed` 被 `_nmalloc` 呼叫，而
0x4d8ea 的唯一 caller 是 `_matherr → matherr_default_thunk`。

## 命名規範

CRT 符號的三類命名約定（Watcom 真符號 / `L$N_<obj>_<purpose>` 匿名 static /
`crt_equivalent_*` / `fd2_*`）見 `symbol_inventory.md`。
