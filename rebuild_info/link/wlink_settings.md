# 重建 FD2.LE 的 wlink 連結設定

從 LE binary layout (`le_layout.md`)、CRT 識別結論 (`../crt/fid_match.md`)、pool routing
(`../emission/pool_routing.md`) 反推出的 Watcom 9.5a `wlink` 連結命令重建（rebuild
toolchain 用 9.5a，與原 binary 同版，避免引入 `_iobuf` layout 等 ABI 差異）。

## 結論：wlink 命令列骨架

```sh
wlink @fd2.lnk
```

`fd2.lnk` directive 檔內容（最小可重建 FD2.LE binary layout 的 wlink 設定）：

```
# ---- 系統與輸出 ----
system dos4g                       # format LE + 預設 Watcom CRT lib + cstart.obj + dos4gw stub
name FD2.EXE                       # 輸出檔名（內含 DOS bind stub + LE 模組）
option stack=4K                    # 覆寫 dos4g 預設 8K，FD2 用 4K（與 cmdline buffer 共享）
option quiet                       # 安靜輸出（推測，無證據要求）

# ---- 主入口物件（決定模組內部名 "f2"）----
file f2.obj                        # 含 main；模組名取 file 列表第一個的 basename

# ---- 全 FD2 source objects (≈652 個 emit_fd2_source function 分散在多個 .obj) ----
file <chapter_*.obj>               # ch01..ch30 init/end/post_action handler
file <battle_*.obj>                # 戰鬥流程、AI、damage、傷害數字
file <ui_*.obj>                    # 選單、cursor、portrait blit
file <resource_*.obj>              # FDFIELD / FDSHAP / FDOTHER / FDTXT decoder + loader
file <save_*.obj>                  # 存讀檔 + map snapshot + slot
file <dialog_*.obj>                # 對話 VM + shop dialog text id table
file <animation_*.obj>             # spell / figani / ani.dat decoder + figani pose state machine
file <graphics_*.obj>              # palette cycle / tile compose / sprite mask blit / rle
file <audio_*.obj>                 # bgm / sfx / FDMUS handle
file <chapter_data.obj>            # cutscene script blob + 106-entry script ptr table
                                   # (`#pragma data_seg("FAR_DATA")` 在此 obj 把 data 推到 obj 3)
file <battle_data.obj>             # item/spell/character/enemy/growth/cutscene 大型 table
                                   # (同樣 `#pragma data_seg("FAR_DATA")`)
file <fd2_crt_wrapper.obj>         # 15 個 `crt_equivalent_*` + 10 個 `fd2_*` CRT-style primitive

# ---- Miles AIL static lib ----
library miles.lib                  # AIL3DIG + AIL3MDI merged (Miles Sound System 3.x for Watcom)

# ---- Watcom 9.5a CRT (必須顯式列；`system dos4g` 不會自己加 C runtime) ----
library clib3s.lib                 # 標準 C runtime (stack-call ABI，配 -3s)
library math387s.lib               # x87 數學 + 387 emu init/fini + softfp
library emu387.lib                 # x87 FPU emulator
# library graph.lib                # Watcom graphics primitives；src-only Layer-2 建置實測不需要
```

> **`system dos4g` 不自動加 C runtime**：早期從 binary 反推時曾推測「CRT 由 `system dos4g` 自動
> pull、不用顯式列」，但實際連結 src-only FD2.EXE 證明這是錯的（見下節「實測」）。`system dos4g`
> 只負責 format LE + cstart + dos4g stub；C runtime 是靠每個 `.obj` 的 **default-library 記錄**
> 在 wlink 找得到 libpath 時才解析，建置環境沒設好 libpath 就全 undefined。所以 `fd2.lnk` 顯式
> `library` 列出 CLIB3S / MATH387S / EMU387。

執行（在 DOSBox-X 內跑 Watcom 9.5a `wlink`，bin 路徑 `C:\Users\fdpsf\Documents\WATCOM_9.5a\BIN`）：

```sh
wlink @fd2.lnk
```

## 實測：src-only Layer-2 連結（已驗證 0 undefined）

上面的骨架是從 binary 反推、用邏輯主題分組 `.obj` 名的**理論重建**（目標 byte-exact 還原原版）。
本專案實際做的是 **Layer-2 src-only 連結**：把 `src/` emit 出來的 C source 編成 `.obj`、加 vendor lib
連出可跑的 FD2.EXE，讓 linker 自由擺放資料（不下 FAR_DATA / object layout directive）。這份連結已驗證
連到 0 undefined，是目前的 ground truth。`fd2.lnk` 由 `tools/fd2_build/mklnk.py` 產生：

```
system dos4g
name E:\out\FD2.EXE              # 全路徑；輸出含 DOS bind stub 的 FD2.EXE
file E:\out\obj\lifemain.obj      # 含 main，擺第一 -> 決定模組內部名
file E:\out\obj\<...>.obj         # 其餘全部 src .obj（重用 tests/genbuild.src_compile_list）
library E:\out\ailv3.lib          # Miles AIL（每次 link 前 stage 進 E:\out）
library E:\out\fd2common.lib      # FD2 自寫的 AIL-support helper
library D:\LIB386\DOS\CLIB3S.LIB  # Watcom 9.5a CRT，顯式全路徑（D: = 掛載的 Watcom 樹）
library D:\LIB386\MATH387S.LIB
library D:\LIB386\DOS\EMU387.LIB
```

實測得到的兩個關鍵結論：

1. **`system dos4g` 不自動加 C runtime**（見上一節的說明框）。遊戲只透過 `crt_*` wrapper、不直接呼叫
   libc 時，`.obj` 的 default-library 記錄在沒有 libpath 的情況下不會被解析，CLIB3S / MATH387S /
   EMU387 的符號全部 undefined。必須顯式 `library` 列出（全路徑）。Miles AIL lib 又會引用真 libc
   （`strcpy` / `memset` / `sprintf` / `malloc`）與 math387s/emu387 的 `__8087` / `__hook387`，所以這
   三個 CRT lib 一定要連。
2. **GRAPH.LIB 不需要**：Layer-2 src-only 建置只需 CLIB3S / MATH387S / EMU387 三個 CRT lib 即達 0
   undefined，沒有未解析的 GRAPH 符號。

src-only 連結還身兼**神諭**：資料 / 函式沒補齊時它 link 不過，回報的 undefined symbol 就是「FD2.EXE
還缺哪些東西在 `src/`」的權威 worklist（以 linker 符號引用為準，比 name-grep 可靠）。完整建置流程、
雙 main 處置、AIL lib staging、實機 playtest 見 `../build_test/workflow.md` 與 `tools/fd2_build/_index.md`。

## 證據鏈：directive ↔ binary 對照

每條 wlink directive 對 binary 內哪個特徵負責：

### `system dos4g`

| 來自 binary | 證據 |
|---|---|
| LE format (`'LE'` signature) | `system dos4g` 預設 format LE |
| module_flags `0x200` (PM-compatible bit) | wlink dos4g 預設旗標 |
| obj 1 base = `0x10000` | wlink dos4g 預設 code base |
| EIP = `0x3C964` 指向 `_cstart_`（stock Watcom cstart）| 對應 Watcom 9.5a `cstart.obj _cstart_`，由 `system dos4g` 預設 `libfile` 連入（`link_vendor_lib`）|
| 引用 `data_crt_emu387_*` / `__sys_init_387_emulator` / `__hook387` | 用到 math387s + emu387 lib（屬 Watcom CRT，須顯式 `library` 連入，見「實測」節，非 `system dos4g` 自帶）|
| 字串 `"RATIONAL DOS/4G"` @ `0x51760` (被 `__hook387` 引用) | DOS/4G 認證字串，emu387 內 |
| FD2.EXE 含 Watcom DOS bind stub (10424 byte) | `system dos4g` 預設打包 stub |

### `option stack=4K`

| 來自 binary | 證據 |
|---|---|
| Object 2 頂端 `0x556B0` 同時是 ESP 初值與 cmdline_buffer `[0x546B0..0x556AF]` 上限 | stack 區大小 = 0x556B0 − 0x546B0 = 0x1000 = 4 KB（cstart 把 cmdline buffer 設成 stack 底起 4K） |
| Object 2 virtual_size 22192 = 16384 file image + 5808 byte zero-fill | 5808 byte = BSS (1712 B) + stack (4096 B) |

Watcom dos4g 預設 stack 是 8K；FD2 比預設小，所以一定有顯式 `option stack=4K` (或 `=4096` / `=0x1000`).

### `name FD2.EXE`

| 來自 binary | 證據 |
|---|---|
| 包裝過的輸出檔是 FD2.EXE（含 stub + LE）| wlink 預設把 `system dos4g` 的輸出包成 .EXE |
| 內部模組名 = `"f2"`（resident name @ ordinal 0）| 模組名取自 `file` 列表第一個 obj 的 basename，不是 `name` 參數。第一個 obj 是 `f2.obj` |

註：`name FD2.LE` 會輸出 raw LE 不含 stub（與 binary 證據衝突）；`name FD2.EXE` 才會觸發 stub binding。

### 沒有 `system dos32a` / `system pmodew` / `system causeway`

| 來自 binary | 證據 |
|---|---|
| `__hook387` 引用 `"RATIONAL DOS/4G"` | DOS/4G family（非 PMODE/W、非 CauseWay、非 DOS/32A）|
| FD2.EXE stub 用 `DOS4GPATH` env var、找 `dos4gw.exe`/`dos4g.exe` | DOS/4G family 專用 stub |

### 沒有 `debug all` / `debug watcom`

| 來自 binary | 證據 |
|---|---|
| LE header `debug_info_off = 0`, `debug_info_len = 0` | 完全無 debug section（連結時關掉或事後 `wstrip`）|

### 沒有 `option map=`

無證據可推。一般遊戲發行版會省 `.map` 檔，但這不會影響 LE binary（map 只是文字輸出）。

### 沒有 `option dosseg`

`option dosseg` 是 16-bit MSC DOSSEG 相容 directive，對 32-bit flat 模型無意義；Watcom dos4g 不需要。

### 沒有 `runtime windows xxx` 等

LE 是 DOS protected mode，不是 Windows VxD / NT。`system dos4g` 已含對應的 runtime。

## Source-side compile 設定（不在 wlink 但會影響 LE layout）

下面是 source file 必須含的 `wcc386` 編譯設定 / pragma，否則無法重建 object 3：

### 把大型遊戲 data table 放進 object 3 (FAR_DATA)

兩種等效手段：

**A. Source-level `#pragma data_seg`（推測 FD2 的做法）**

在含大型 data table 的 source file（item/spell/character/enemy/cutscene 等）開頭：

```c
#pragma data_seg("FAR_DATA", "FAR_DATA")
const item_entry items[215] = { ... };
const spell_entry spells[36] = { ... };
const char_base_entry char_base[32] = { ... };
const char_growth_entry char_growth[68] = { ... };
const enemy_entry enemies[68] = { ... };
const unsigned char cutscene_script_xxx[N] = { ... };
const unsigned char* cutscene_script_ptr_table[106] = { ... };
#pragma data_seg()
```

wlink 預設會把所有 `FAR_DATA` class segment group 起來變成獨立 LE object。

**B. compile-line `-zdf` / `-zdt=N`**

`-zdf` 強制所有 static data 進 FAR_DATA；`-zdt=N` 設 threshold (大於 N byte 的 data item 進 FAR_DATA)。但 FD2 object 3 混了 30 byte 的小 state 變數（如 `data_fd2_graphics_glyph_blit_state @ 0x627A3`）與 5 KB 的 item table — 無法用單一 threshold 解釋，所以**推測 FD2 用 (A)** 手動 pragma 而非 (B) threshold。

### `wcc386` 旗標

以下旗標是「不會與 binary 證據衝突」的合理重建（不是唯一解）：

```sh
wcc386 -bt=dos4g          # build target = DOS/4G (LE format)
       -fp5 -fpi87        # x87 instructions + 387 emulator fallback
       -3r? -3s? -4r? -4s? # CPU: 386 or 486？(無證據區分；3 系列即可)
       -ms                # stack-call ABI (CLIB3S)；對應 default __cdecl
       -ot -oh -ol+ -oi   # optimizations: time + loops; FD2 是 release build
       -s                 # skip stack overflow check（FD2 自帶 stkchk via `__STKOVERFLOW`）
       -zq                # quiet
       -zp4               # struct align 4 byte（FD2 struct field offset 均為 4 倍數）
       -d0                # no debug info
```

關鍵旗標證據：

| 旗標 | 證據 |
|---|---|
| `-bt=dos4g` | LE format + dos4g cstart |
| `-ms` (memory model = small stack) | byte_match `memcpy.obj` (42 byte版本) 命中 CLIB3S — `-mf` 也產同檔，兩者皆可 |
| `-fpi87` | 大量 x87 指令 + emu387 link（純 hardware FPU 模式不會 link emu387）|
| `-3?` or `-4?` | 無證據區分 386 vs 486；CPU 模式不影響 LE header（cpu_type=2 表 ≥386）|
| `-d0` | LE debug_info=0 |
| `-zp4` | 多個 16-byte stride struct align 到 4 byte 邊界（item_entry 23 byte 但 array stride 是 23 byte 緊排） — 實際是 1-byte pack 的 struct (`-zp1`)？需個別 struct 驗證 |
| `-or` | 由於 stack frame `EBP-relative` 而非 omit-frame-pointer → 沒用 `-ofr`；optimization 屬 release-grade 但非極端壓榨 |

`-zp` 對 FD2 自家 struct 的具體值（1 / 2 / 4）尚待 source emission 階段針對每個 struct 個別決定（不影響 wlink）。

## 等價的另一種寫法（不用 directive file）

```sh
wlink system dos4g name FD2.EXE option stack=4K \
      file f2.obj, chapter_xx.obj, ..., battle_data.obj \
      library miles.lib
```

效果等同 directive file。

## 已知不確定 / 推測項

| 項目 | 推測值 | 不確定原因 |
|---|---|---|
| 全部 source `.obj` 檔名與分組 | 用「邏輯主題 prefix」(chapter_/battle_/ui_...) | LE binary 不保留 source filename；只能用 function/data 主題分群推估 |
| `miles.lib` 內 obj 是否來自 AIL3DIG.LIB + AIL3MDI.LIB 各自 import 還是預先 merge | 預先 merge 成單一 `miles.lib`，或 wlink 分兩 `library` directive 各別讀 | LE binary 兩種寫法產出 byte-identical；無法區分 |
| `option stack=4K` 還是 `option stack=4096` 或 `option stack=0x1000` | 三者等效 | wlink 對三者輸出相同 binary |
| Watcom DOS bind stub 是否走預設 (`bind.exe` 的內建 stub) 還是 `option stub=...` 顯式指定 | 預設 | Stub 的 1988-1992 copyright + 標準 4GWBIND 字串組合 = Watcom 9.5a 預設 stub |
| compile 時 `-or` / `-ot` / `-os` 等 optimization 細部 | 看每個 function 的指令重排可推；本文件不展開 | 屬 emission 階段細節，寫在 `../emission/pipeline_spec.md` |

## 重建驗證流程

1. 把全 1342 個 function 的 emit_action=`emit_fd2_source` 那 647 個從 Ghidra emit 成 C source（`../emission/pool_routing.md` §emit_action）。
2. 用上述 wlink directive + wcc386 旗標 + Watcom 9.5a + Miles AIL static lib 重新 link。
3. 對重 build 的 FD2.LE 與原版做 byte-level diff：
   - LE header 71 個固定欄位完全相等
   - Object table 3 個 entry 完全相等
   - Page map 全 LEGAL flag、page number 1..71 順序
   - Data pages 內 70 個 4 KB page + 1 個 1234 byte page byte-identical
   - Fixup section size 與 record 序列 byte-identical
4. 若 byte 不 match：先比 LE header 與 object table（決定 wlink 設定），再比 data pages（決定 emit 順序與 source obj 內容）。

具體 emission rule 與三層 binary 等價不變式（specification-exact / functionally-exact / byte-exact）見 `../emission/pipeline_spec.md`。

## 與其他 KB 交互

- LE binary 細部 layout：`le_layout.md`
- CRT lib 構成與符號 lookup：`../crt/fid_match.md`、`../crt/symbol_inventory.md`、`../crt/lookup_9.5a.json`
- Miles AIL lib inventory：`../ail/inventory.md`、`../ail/_index.md`
- emit_action 路由與 pipeline rule：`../emission/pool_routing.md`、`../emission/pipeline_spec.md`
- Calling convention 規則：`../emission/calling_convention.md`
