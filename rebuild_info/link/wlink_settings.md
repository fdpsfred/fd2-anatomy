# 重建 FD2.LE 的 wlink 連結設定

`src/` 已可編譯連結成可玩的 FD2.EXE，是 ground truth。本文件是實際連結所用 `fd2.lnk`
directive 與 `wcc386` 編譯旗標的唯一正典（`fd2.lnk` 由 `tools/fd2_build/build_fd2.py` 產生，
完整建置流程與實機 playtest 見 `../build_test/`）。連結用的 toolchain 是 Watcom 9.5a，與原
binary 同版，避免引入 `_iobuf` layout 等 ABI 差異。

## fd2.lnk（實測，已驗證 0 undefined）

專案採 Layer-2 src-only 連結：把 `src/` 的 C source 編成 `.obj`、加 vendor lib 連出可跑的
FD2.EXE，讓 linker 自由擺放資料（不下 FAR_DATA / object layout directive）。directive 的
邏輯結構（省略環境相關的絕對路徑）：

```
system dos4g                 # format LE + cstart.obj + dos4gw bind stub
name FD2.EXE                 # 輸出含 DOS bind stub 的 FD2.EXE
file lifemain.obj            # 含 main，擺第一 -> 決定模組內部名
file <...>.obj               # 其餘全部 src .obj
library ailv3.lib            # Miles AIL（從 libs/ailv3 連入）
library CLIB3S.LIB           # Watcom 9.5a 標準 C runtime（stack-call ABI，配 -3s）
library MATH387S.LIB         # x87 數學 + 387 emu init/fini + softfp
library EMU387.LIB           # x87 FPU emulator
```

執行（在 DOSBox-X 內跑 Watcom 9.5a `wlink`）：`wlink @fd2.lnk`。

幾個 load-bearing 的實測結論：

- **`system dos4g` 不自動連 C runtime**。它只負責 format LE + cstart + dos4gw stub；
  CLIB3S / MATH387S / EMU387 這三個 CRT lib 必須顯式 `library` 列出。遊戲只透過 `crt_*`
  wrapper、不直接呼叫 libc 時，`.obj` 內的 default-library 記錄在沒有 libpath 的環境下不會
  被解析，缺這三個 lib 就會全部 undefined。Miles AIL lib 又會引用真 libc（`strcpy` /
  `memset` / `sprintf` / `malloc`）與 math387s/emu387 的 `__8087` / `__hook387`，所以這三個
  CRT lib 一定要連。
- **GRAPH.LIB 不需要**：Layer-2 src-only 建置只需 CLIB3S / MATH387S / EMU387 三個 CRT lib
  即達 0 undefined，沒有未解析的 GRAPH 符號。
- **不連 fd2common.lib**：6 個 `fd2_dpmi_*` 與 `crt_equivalent_get_eflags`/`_thunk` 已由
  `src/util/dpmi.c` + `src/crt/crt.c` 以裸名 PUBDEF 定義（file obj 在任何 library 被 pull 前
  就滿足這些符號），故該 lib 不會被拉進來。

src-only 連結還身兼**神諭**：資料 / 函式沒補齊時它 link 不過，回報的 undefined symbol 就是
「FD2.EXE 還缺哪些東西在 `src/`」的權威 worklist（以 linker 符號引用為準，比 name-grep 可靠）。

## wcc386 編譯旗標（實測）

`src/` 每個 `.c` 都用同一組旗標編譯（透過 `WCC386` 環境變數傳入）：

```sh
wcc386 -bt=dos4g          # build target = DOS/4G（LE format）
       -fp5 -fpi87        # 原版 FP 模型：inline raw 387 x87 指令，無 emulator fixup record
       -3s                # 386 stack-call ABI（對應 CLIB3S）
       -ms                # small memory model
       -zp4               # struct pack 4 byte
       -i=include -i=...  # header search path（src/include + vendor AIL header）
```

- **不加任何 `-o*` 最佳化旗標**。全部 emit 函數的 asm review 都以此組態（預設最佳化）與原版
  逐指令對照通過。9.5a 不接受 `-oh` / `-ol+`（那是後續 Watcom 版本語法，會報 "Invalid
  optimization option"）。
- **不用 `-s`**（stack-check 旗標）。原版遊戲 function prologue 都有 `PUSH n` / `CALL __CHK`
  stack probe（wcc386 預設的 stack-overflow check）；加 `-s` 會拿掉它。4K stack 底下緊鄰
  DGROUP 資料，未偵測的溢位會無聲改寫全域，所以 stack check 維持開啟以對齊原版。
  例外：CRT-equivalent / DPMI 支援單元在原版全部無探測，在 `src/crt/crt.c` 與
  `src/util/dpmi.c` 用整檔 `#pragma off (check_stack)` 對齊。這是 load-bearing --
  `crt_equivalent_get_eflags_thunk` 會被 AIL timer / audio-mix ISR 在 AIL 私有 DGROUP 堆疊
  （低於 `_STACKLOW`、SS 同 flat selector）上呼叫，帶探測必誤發 "Stack Overflow!" 終止
  （CLIB3S 的 SS 逃生門在 flat model 下永不生效）。`src/gfx/palette.c` 與 `src/save/save.c`
  另有以 `#pragma off/on (check_stack)` 包住的手寫組語 primitive wrapper，但那些 wrapper 在
  `#ifdef FD2_ASM_PRIMITIVES` 之下，預設不編（見下）。
- **math intrinsic 必須停用**。`math.h` 預設 `#pragma intrinsic` 標記 sqrt/sin/cos 等，
  wcc386（`-fpi` 與 `-fpi87` 皆然）會對它們 emit `IF@D*` helper call、把引數留在 ST(0) 跨
  call 邊界。原版遊戲碼一律呼叫 CRT `sqrt` / `sin` / `cos` 真函數（引數走堆疊、double 以
  EDX:EAX 回傳；原 binary 內 IF@* stub 零 xref，僅隨 sqrt387 / trig387 module 連帶進入）。
  其中 IF@SQRT 的 FTST/FSTSW/SAHF 負數檢查是原版從未執行的指令路徑，在 86Box-macOS dynarec
  上會誤執行（sqrt 回傳 0.0 -> 白光柱 remap count 變 0 -> runaway page fault，見
  `../build_test/playtest_bugs.md` F 類）。重建以在 `#include <math.h>` 前定義
  `__NO_MATH_OPS` 對齊（`src/gfx/rndscene.c`、`src/spell/spellcin.c`、`src/anim/anisummn.c`）。
- **`FD2_ASM_PRIMITIVES` 預設不定義**。原版三個手寫組語 primitive（`fd2_apply_palette_remap_run`
  @ palette.c、`fd2_save_compute_checksum` + `fd2_save_crypt_buffer` @ save.c）預設編成其
  portable-C reference 分支（Layer-2 功能等價，與其他手寫組語葉子同政策）。byte-for-byte
  對齊 vendor 迴圈的 `#pragma aux` 版本保留在 `#ifdef FD2_ASM_PRIMITIVES` 之下，要重啟就在
  compile 與 replay build 都加 `-DFD2_ASM_PRIMITIVES`（旗標必須一致）。

等價鐵則（Layer 1/2/3、時序熱迴圈、math intrinsic 例外、手寫組語政策）見
`../equivalence/rules.md`。

## 大型遊戲 data table 放進 object 3（FAR_DATA）

原版 object 3 混了 30 byte 的小 state 變數與 5 KB 的 item table，**無法用單一 size threshold
（`-zdt=N`）解釋**，最合理的原版做法是 source-level 顯式把 segment 改名到 `FAR_DATA` class，
wlink 自動把所有 `FAR_DATA` class segment group 起來變成獨立 LE object（object 3 的形成見
`le_layout.md`）。這屬於 byte-exact 還原原版 layout 才需要的手段；Layer-2 src-only 建置不下
任何 FAR_DATA / object layout directive，直接讓 linker 擺放資料即可（見 `../equivalence/rules.md`
的 Layer-2 政策）。

## 證據鏈：directive <-> binary 對照

每條 wlink directive 對 binary 內哪個特徵負責：

### `system dos4g`

| 來自 binary | 證據 |
|---|---|
| LE format（`'LE'` signature）| `system dos4g` 預設 format LE |
| module_flags `0x200`（PM-compatible bit）| wlink dos4g 預設旗標 |
| obj 1 base = `0x10000` | wlink dos4g 預設 code base |
| EIP = `0x3C964` 指向 `_cstart_`（stock Watcom cstart）| 對應 Watcom 9.5a `cstart.obj _cstart_`，由 `system dos4g` 預設連入 |
| 引用 `data_crt_emu387_*` / `__sys_init_387_emulator` / `__hook387` | 用到 math387s + emu387 lib（須顯式 `library` 連入，非 `system dos4g` 自帶）|
| 字串 `"RATIONAL DOS/4G"` @ `0x51760`（被 `__hook387` 引用）| DOS/4G 認證字串，emu387 內 |
| FD2.EXE 含 Watcom DOS bind stub（10424 byte）| `system dos4g` 預設打包 stub |

### `option stack=4K`

| 來自 binary | 證據 |
|---|---|
| Object 2 頂端 `0x556B0` 同時是 ESP 初值與 cmdline_buffer `[0x546B0..0x556AF]` 上限 | stack 區大小 = 0x556B0 - 0x546B0 = 0x1000 = 4 KB |
| Object 2 virtual_size 22192 = 16384 file image + 5808 byte zero-fill | 5808 byte = BSS (1712 B) + stack (4096 B) |

Watcom dos4g 預設 stack 是 8K；FD2 比預設小，所以一定有顯式 `option stack=4K`（或 `=4096` /
`=0x1000`，三者等效）。註：src-only Layer-2 建置不顯式下 `option stack`，讓 linker 用預設值；
`option stack=4K` 屬還原原版 stack size 才需要的 directive。

### `name FD2.EXE`

| 來自 binary | 證據 |
|---|---|
| 包裝過的輸出檔是 FD2.EXE（含 stub + LE）| wlink 預設把 `system dos4g` 的輸出包成 .EXE |
| 內部模組名 = `"f2"`（resident name @ ordinal 0）| 模組名取自 `file` 列表第一個 obj 的 basename，不是 `name` 參數 |

`name FD2.LE` 會輸出 raw LE 不含 stub（與 binary 證據衝突）；`name FD2.EXE` 才會觸發 stub
binding。

### 沒有 `system dos32a` / `system pmodew` / `system causeway`

| 來自 binary | 證據 |
|---|---|
| `__hook387` 引用 `"RATIONAL DOS/4G"` | DOS/4G family（非 PMODE/W、非 CauseWay、非 DOS/32A）|
| FD2.EXE stub 用 `DOS4GPATH` env var、找 `dos4gw.exe` / `dos4g.exe` | DOS/4G family 專用 stub |

### 沒有 `debug all` / `debug watcom`

| 來自 binary | 證據 |
|---|---|
| LE header `debug_info_off = 0`, `debug_info_len = 0` | 完全無 debug section（連結時關掉或事後 `wstrip`）|

### 其他不下的 directive

- **`option map=`**：無證據可推；map 只是文字輸出，不影響 LE binary（`build_fd2.py --map`
  可選擇性產生 symbol map 供 BSS/COMDEF layout 檢查）。
- **`option dosseg`**：16-bit MSC DOSSEG 相容 directive，對 32-bit flat 模型無意義。
- **`runtime windows xxx`**：LE 是 DOS protected mode，不是 Windows VxD / NT。

## 關鍵 compile 旗標的 binary 證據

| 旗標 | 證據 |
|---|---|
| `-bt=dos4g` | LE format + dos4g cstart |
| `-ms`（small memory model）| `memcpy.obj`（42 byte 版本）byte_match CLIB3S -- `-mf` 也產同檔，兩者皆可 |
| `-fpi87` | 大量 x87 指令 + emu387 link（純 hardware FPU 模式不會 link emu387）|
| `-d0` | LE debug_info=0（src-only 建置不顯式加，debug 預設關）|
| `-zp4` | struct pack 4 byte |

## 對原版 .obj 分組的推論（非重建目標）

原版把 source 分成哪些 `.obj`、用什麼檔名分組，LE binary 不保留 source filename，只能從
function / data 的主題推估（chapter / battle / ui / resource / save / dialog / animation /
graphics / audio 等主題群）。Miles AIL 究竟是 AIL3DIG + AIL3MDI 預先 merge 成單一 lib 還是
分兩個 `library` directive 各別讀，兩種寫法產出 byte-identical，無法區分。這些屬「還原原版
連結細節」的推論，**不是本專案 Layer-2 重建的目標**；重建只需上面實測的 src-only fd2.lnk。

## 與其他 KB 交互

- LE binary 細部 layout：`le_layout.md`
- Watcom Easy OMF-386 格式 quirks：`omf_386.md`
- CRT lib 構成與符號 lookup：`../crt/fid_match.md`、`../crt/symbol_inventory.md`
- Miles AIL lib inventory：`../ail/_index.md`
- 四 pool 分類與 emit 路由：`../equivalence/pool_classification.md`
- 等價鐵則與 emission 規則：`../equivalence/rules.md`
- 驗證手段（eqcheck / playthrough golden / 原版差分 等）：`../verification.md`
- 完整建置流程、雙 main 處置、AIL lib staging、實機 playtest：`../build_test/`、`tools/fd2_build/_index.md`
