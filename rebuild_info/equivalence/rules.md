# 等價鐵則與分類參考

`src/` 是已可用 Watcom C/C++ 9.5a 編譯、通過遊玩測試的重建成品，是本專案的 ground
truth。本檔記錄 `src/` 已滿足、且維護時必須繼續守住的**等價鐵則**，以及從 Ghidra
把 FD2.LE 對映回 C source 時需要的**結構分類知識**（fall-through 模式、字串 / 資料 emit
規則）。與 pool 分類、Watcom ABI 相關的細節分別以單向引用指向 `pool_classification.md`
與 `watcom_abi.md`，本檔不重述。

## 等價層級

重建後 re-link 出的 binary 依三層 invariant 判定等價，每層適用範圍與驗證手段不同。

### Layer 1：specification-exact（最低保證，全範圍）

re-link 出的 .EXE 在 DOSBox-X 內的外顯行為必須與原 FD2.LE 完全一致：

- 30 章劇情的對話 / 過場 / 戰鬥流程
- FD2.SAV byte-level 兼容（讀原版存檔可繼續、新存的存檔原版可讀）
- BGM / SFX 觸發時機與曲目選擇
- 螢幕 pixel output（同一 input scancode 序列下，每一 frame 的 mode13h buffer 內容相同）

### Layer 2：functionally-exact（所有以 C source 重建的 function）

以 C source 重建的 function 分兩類：game-logic 的 `fd2_*`，以及行為等價於 Watcom CRT
但 byte 不 match 任一 lib obj 的 `crt_equivalent_*`（具名清單見 `../crt/symbol_inventory.md`；
各類的即時數量以 Ghidra `list_functions` / `get_function_count` 取得，本檔不硬編）。每個這類
function 在「相同 input register / stack / memory state」下執行完，必須產出「相同的 return
value / register state / 寫入 memory 的 bytes」。

不要求 instruction 級 byte 相同 —— register allocation / instruction selection / scheduling
由 source 結構 + 編譯器旗標決定。重建的 C source 結構與原 1998 年 FD2 source 不同，即使用同版
Watcom 9.5a，部分 function 也會 emit 出不同的 instruction sequence，仍屬 Layer 2 等價。
**但下列三種情形是例外，`src/` 必須對齊原版 codegen，見「Layer 2 的三個例外」。**

驗證手段：對 pure-compute leaf function（damage 計算 / softfp / decoder helper / hash /
checksum）跑 emulator 雙邊 trace（原 FD2.LE vs 重建版），對相同 input 比對 final state。

### Layer 3：byte-exact（不追求）

本專案不追求 byte-exact。register allocation、fall-through 的 C 表達方式、function 排列
順序（由 linker 決定）、alignment padding（由 wlink 策略決定）等差異，使 byte-exact 既不
切實際也無必要。對 `link_vendor_lib` pool（ail + crt lookup-resolved），byte-exact 是同版
vendor lib 直接 link 的自然副產物，不需額外努力。

## Layer 2 的三個例外（必須對齊原版 codegen）

以下三類的「功能等價但 codegen 不同」會產生實際可觀察的行為偏差，因此 `src/` 不能為了
簡潔或簡化而偏離原版。

### 時序敏感的純 CPU 熱迴圈

功能等價的 codegen 簡化（例如有號 `/128` 化簡為算術 `>>7`，因 in-bounds guard 保證
`src >= 0` 時結果相同）會改變指令數。純 CPU 熱迴圈（縮放 / blit，每 frame 數萬像素）的
牆鐘執行時間隨指令數變動，一旦縮短就破壞遊戲隱性的時序平衡。

實證（商店進入腳步聲被對話音效打斷）：`fd2_blit_scaled_chapter_pose` 的 pose 縮放除法若
emit 成 `>>7`（2 指令）而非原版 `/128`（6 指令），每像素少約 4 指令，過場從約 0.93 秒縮到
約 0.80 秒，短於固定 0.9 秒的商店腳步聲 SFX（AIL DMA real-time、不隨 DOSBox cycles），
導致後續 SFX 的 `AIL_stop_sample` 把腳步聲切掉。`src/gfx/blittile.c` 的
`fd2_blit_scaled_chapter_pose` 因此保留 `/128` 形式。時序敏感熱迴圈一律以 wdis 逐指令比對
指令數驗證。詳見 `../build_test/playtest_bugs.md` D 類。

### math intrinsic 呼叫形式

原版遊戲碼一律呼叫 CRT `sqrt` / `sin` / `cos` 的 named 真函數（`sqrt @ 0x3c6fc`、
`sin @ 0x3c898`、`cos @ 0x3c885`，皆被遊戲碼呼叫）。危險的是 `sqrt` 的 compiler-intrinsic
變體 `__@DSQRT @ 0x3c738`：它在原 binary 零 xref、從未執行，其負數 domain 檢查走
FTST/FSTSW/SAHF，而 named `sqrt` 改用整數 sign-bit 檢查 + `FSQRT`、不走這條。`math.h` 預設會把
`sqrt` 標成 compiler intrinsic，使 wcc386 改 emit `CALL __@DSQRT` —— ST(0) 跨 call 邊界、
且走原版從未執行的 FTST/FSTSW/SAHF 路徑；86Box-macOS 的 dynarec 會誤執行這條死路徑，令
`sqrt` 回傳 0.0、進而 `count=0` 觸發白光柱特效的 runaway page fault crash。（`sin` / `cos` 的
named wrapper 內部本就 `CALL IF@SIN` / `IF@COS`，故那兩個 stub 有 xref、會執行，不同於零 xref
的 `__@DSQRT`；`__NO_MATH_OPS` 對三者一併強制 named call form。）

`src/` 在每個用到 math 的 .c 於 `#include <math.h>` 前 `#define __NO_MATH_OPS`，強制走真
CRT call form。實作見 `src/anim/anisummn.c`、`src/gfx/rndscene.c`、`src/spell/spellcin.c`。
旗標設定見 `../link/wlink_settings.md`，根因與診斷見 `../build_test/playtest_bugs.md` F 類。

### 手寫組語函數

原版有少數 game-logic 函數不是 wcc386 編譯的 C。辨識特徵＝無 `PUSH n / CALL __CHK`
stack-probe prologue（wcc386 編譯的 FD2 C 必有），且用字串指令 / 硬體 ROL 慣用法（wcc386
9.5a 從可攜 C 的任何旗標組合都產不出 LODSB/STOSB/LOOP 形式，恆為 MOVZX/DEC/JNE）。

這些函數（位址連續 cluster：`0x4DB9C` palette remap、`0x4DBB9` save checksum、`0x4DBD8`
save crypt，以及 RLE blitter 家族）一律以可攜 C emit 維持 Layer-2 功能等價，建置預設走這條
C 分支。三函數 cluster 另在 `#ifdef FD2_ASM_PRIMITIVES` 下保留與原版迴圈體 byte-for-byte
相同的 `#pragma aux` inline-asm 分支，僅供 A/B 診斷（預設不定義此 macro，兩支 build script
的旗標必須同開同關）。實作見 `src/save/save.c`、`src/gfx/blittile.c`、`src/gfx/blitspr.c`、
`src/gfx/palette.c`。

## 字串與位址引用鐵則

### E-2：Watcom 9.5a string-pool 不跨 `.obj` boundary dedup

Watcom 9.5a 編譯每個 `.obj` 時內部會 dedup string literals，但**不跨 `.obj` 去重**。同一
字串若在多個 `.obj` 用到，binary 中會有多份 copy。`src/` 對應做法：每個 fd2 `.obj` source
各自 emit 自己的字串 literal，不做整個 source-tree 的全域字串 pool；vendor `.obj` 的多 copy
字串由 `link_vendor` 自動帶入，FD2 source 不重新 declare。

FD2.LE 觀察到的 cross-`.obj` 多 copy 字串：

| 字串                                         | 位址                        | 所在 `.obj`                                    |
| -------------------------------------------- | --------------------------- | ---------------------------------------------- |
| `"Out of timer handles\n"`                   | 0x5128c + 0x51580           | install_driver.OBJ + mdi_driver_setup.OBJ      |
| `"Unrecognized digital audio file type\n"`   | 0x51428 + 0x5146d           | allocate_file_sample.OBJ + set_sample_file.OBJ |
| `" Out of Memory !!!\n"`                     | 0x50004 + 0x50023 + 0x50037 | 3 個 fd2 `.obj` copy                           |

### E-3：Sub-string anchor（指向字串內部 offset 的指標）

caller 有時以指向 string item 內部 offset 的指標引用子字串（非從 item start），典型例：

- `"FD2.SAV\0rb\0"` 的 `"rb\0"` 部分（fopen mode）被另外當 mode 字串 PUSH
- `"IO_ADDR\0IRQ\0"` 的 `"IRQ\0"` 部分被 strnicmp 取來當 INI key 比對

`src/` 一律把這類引用寫成 `(parent_string + offset)`，或直接寫該子字串的 literal（Watcom
9.5a 的 `.obj` 內部 string-pool dedup 會使其指向相同的字串末尾）。不為子字串另 emit 獨立的
named global。

### E-3b：絕對位址引用一律改 symbol（禁 hardcoded immediate）

原版 code 常以絕對位址 immediate 常數引用字串 / 資料（例如把某 `.DAT` 檔名字串的位址直接
寫死成 `push 0x51a4d`）。Layer 2 不追求 byte-exact、linker 會自由擺放資料，所以**任何寫死的
絕對位址在 rebuild 都會指錯**。`src/` 必須把這類引用改成 **symbol 引用**，讓 linker 自己
填正確位址。

實證（"File not found" 開場退出）：`fd2_load_dat_resource` 若把原版字串位址寫死成 immediate，
rebuild linker 把字串擺到別處 → `fopen` 拿到空檔名失敗、開場就退出；修法是把 hardcoded 字串
位址全改 symbol。這條與 E-2 / E-3 的 string-pool 處理同源 —— 字串一律以 source-level literal /
named symbol 引用，絕不以絕對位址 immediate 引用。詳見 `../build_test/playtest_bugs.md` E 類。

## 資料相鄰性與 layout 鐵則

Watcom 對已初始化 / 已定義的 global 按 source declaration order 連續排列，不會自動插
alignment padding；混合 type（byte / word / dword / pointer / array）自然 byte-pack。x86
unaligned MOV 完全合法，所以 4-byte global 落在非 4-byte-aligned 位址（例如
`data_fd2_animation_ani_decoder_dst_buf @ 0x52762`，`0x52762 % 4 = 2`）runtime 正常。`src/`
對這類 global 直接 emit 一般 typed 宣告（`uint32 name;`），不需要 `#pragma pack`、byte-stream
bit-cast 或 wlink ORDER。

### 相鄰性不變式（必守）

**凡 reader 把多個相鄰 global 當 array 索引（`(&g0)[i]`）、或對相鄰 global 做 struct
punning，一律 emit 成單一的真 array / struct，不可拆成多個 scalar。** 原因：未初始化的 global
在 Watcom 是 tentative definition（編成 COMDEF/COMMON record 交給 linker 合併），linker
**不保證這些 tentative scalar 的擺放順序與相鄰關係** —— 實測甚至是**反序**（wlink map 證實）。
拆成獨立 scalar 後，reader 的 `[1]` / `[2]` 會讀到鄰居 global 的 garbage 指標。

實證（炙焰刀施法平移 crash）：`fd2_animate_bg_zoom_transition_in` 用 `bg_layer[idx % 3]` 把
三個 BG layer 指標當 `uint32[3]`。emit 成三個獨立 tentative scalar 後 wlink 反序擺放，
reader 讀到鄰居 garbage 指標 → 餵 `fd2_rle_blit_sprite` wild read → protected-mode fault。
`src/` 因此把它 emit 成真 array `uint32 data_fd2_battle_special_cinematic_bg_layers[3];`
（`src/anim/anicine.c`）。同理，28-byte `union REGS` INT scratch 必 emit 成單一 struct，不可
拆成獨立 `uint8` 全域。Ghidra 端同步成 `dword[N]` / 對應 struct 型別，並用 wlink map
（`tools/fd2_build/build_fd2.py --map`）驗證實際 layout。詳見 `../build_test/playtest_bugs.md` B 類。

### Boundary-merge 不變式

當多個鄰接的 Ghidra-auto-split data items 實際是 caller 以 copy loop / array 索引取用的
**單一** struct / array 時，Ghidra 端須用 `apply_data_type(clear_existing=true)` 把 boundary
合併為單一 semantic item，`src/` 端 emit 為單一 `static <type> name[N] = {...};`，不為被合併
掉的 sub-items 各自 emit 宣告。此不變式與上面相鄰性不變式同源：C 標準保證 array element /
struct member 升序相鄰，把「被當成連續記憶體取用」的區段 emit 成單一 aggregate 才能保證 layout。
判斷準則是 caller 端的取用形態（copy loop / index / struct punning），不是 Ghidra 的自動切分。

## Fall-through 六模式

Ghidra 把每段連續 bytes 當獨立 Function entity，但原 binary 有多處「prev function 末尾
fall-through 進 next function entry」的結構。重建時不能把 Ghidra 的 Function entity 直接當
C function 逐一輸出，否則會破壞 fall-through 語意。這些 fall-through 分六種模式，`src/` 對
每種的處理如下。

### 模式 A：SHARED EPILOGUE STUB

prev function 末尾無 RET，直接 fall-through 進一個 RET-only 的共用 epilogue（純 stack
cleanup + RET），其他 function 也以 tail-JMP 進同一 epilogue；典型是 free-wrapper epilogue
（`CALL free; ADD ESP,4; RET`）。`src/` 不把 epilogue 獨立 emit 成 C function，而是把它的
stack-cleanup / free 效果直接 reproduce 在每個 source function 末尾（多為一句 return，或一個
plain `free()` call）。共用 epilogue fragment 的完整具名清單見 `watcom_abi.md`。實作範例見
`src/anim/anicombt.c` 的 "reproduced here as the return" 與 `src/anim/aniend.c` 的
"the plain call below is the functional equivalent" 註解。

### 模式 B：MULTIPLE ENTRY POINTS / SHARED BODY

兩個 function 共用同一段邏輯主體，以不同 entry 做不同 setup 後 JMP 進共用 body。典型：
`fd2_play_palette_fade_to_black` 在 binary 內是 shared-body wrapper（把 counter XOR 成 0 後
JMP 進 darken loop，而該 loop body 實際 living inside `fd2_load_and_fade_in_cinematic_image`）。
`src/` 把每個 entry 各 emit 成獨立 standalone function，各自 reproduce 共用 body 的行為
（Watcom 重生等價的 standalone loop）。實作見 `src/gfx/palette.c`。

### 模式 C：HEADER-ONLY ENTRY

prev function 只做少量 header 工作（pre-PUSH 幾個常數當 args、推 frame_size），fall-through
進真正做事的 next function。典型：四個 chapter event handler
（`fd2_chapter_event_handler_18__ch7_captain_defeat` / `_20__ch10_dialog` /
`_34__ch23_ai_ctrl` / `_36__ch24_cinematic`）。`src/` 把每個 header-only entry 各 emit 成一般
C function；原版「prev 與 next 各自 PUSH 不同 frame_size 給 `__CHK`」的 prologue 差異在
Layer 2 無關緊要 —— wcc386 會為每個 function 各自注入自己的 `__CHK` stack-probe prologue，
功能等價成立。body 內被其他 handler 借用的共用 entry point 一律 reproduce inline 在各
consumer。實作見 `src/field/chevt1.c`（handler 18/20）與 `src/field/chevt2.c`（handler 34/36）。

### 模式 D：DEAD FALL-THROUGH

prev function 末尾的 fall-through 在執行流上死掉（結尾是不返回的指令，或條件分支把控制流
全帶回別處）。典型是 Watcom CRT noreturn 家族：`_exit` / `__exit` / `__exit_with_msg`（結尾
INT 21h AH=4Ch DOS terminate 不返回）+ `_cstart_`（entry 2B JMP，後接 Watcom 版權字串 data，
fall-through 路徑物理不可達）。`src/` 把 prev 與 next 各自 emit 成一般 C function；dead 路徑
就是 dead，行為不變。

### 模式 E：DATA TABLE FRAGMENT

function 之間的 byte 區段其實是 data table（jump table / 常數表 / align fill），被 Ghidra
自動分析誤判 disassemble 成 code。這些區段已全部在 Ghidra mark 成對應 data 型別、不再以
Function entity 存在，重建時 iterate function 自然跳過。switch jump table 由 Watcom 9.5a
自行從 C 的 `switch` 生成、align pad 由 wlink 重新對齊、CRT MATH387S 常數池走 `link_vendor`。
這些 fragment 的具名清單與 data 型別見 `pool_classification.md`。

> **遊戲端 codegen 佐證**：全 binary 僅 63 個 indirect JMP，全部落在 CRT（57，全在 `__int7`）/
> AIL（6，含 dpmi use32 save/restore 兩個 register-indirect `JMP EDX`/`ECX` thunk），
> **遊戲端 0 個**——FD2 的遊戲 `switch` 一律編成 if/else 鏈、不生 compiler jump table；章節與
> 施法的 function-pointer dispatch 表走 indirect CALL（非 JMP，見 `watcom_abi.md` §dispatch callees）。
> 故模式 E 的 jump-table fragment 只出現在 CRT 段。全 binary indirect-JMP / orphan-code audit 工具與
> 結論見 `tools/program_analysis/jump_table_audit/`。

### 模式 F：STATE-MACHINE INIT-ENTRY

prev function 是 state-machine 的「初次進入 setup」（初始化某 reg 為 0），fall-through 進以該
reg 當 state 的 loop body（用完一輪 JMP 回自己）。唯一案例是 AIL voc dispatcher（init EBP=0
→ loop with state in EBP）。此 function 屬 vendor AIL driver，走 `link_vendor_lib` 從靜態 lib
連結，不 emit C source。

## 結構性不變式

重建出的 source 還必須滿足（與 binary 等價無關的結構要求）：

1. **0 個 `vendor_*` / `FUN_*` 殘留名** —— 所有 emit 的 function 都有 best-effort 邏輯名稱。
2. **fall-through chain 全部以 explicit C 控制流表達** —— 不依賴 C 檔內 function 之間的
   declaration 順序（Watcom 不保證 source 順序 = link 順序）。
3. **DATA TABLE FRAGMENT 不 emit 為 function** —— 已全部 mark 為 data，重建時自動跳過（見模式 E）。

## 引用

- pool 分類（四 pool、`binary_artifact` NOP/padding、wlink align fill、data-table fragment
  型別）：`pool_classification.md`
- Watcom ABI（三種 cc、`__CHK`、dispatch 簽名、共用 epilogue fragment 清單）：`watcom_abi.md`
- `crt_equivalent_*` 具名清單：`../crt/symbol_inventory.md`
- 四種驗證手段（eqcheck / FD2_REPLAY / playthrough golden / 原版差分）：`../verification.md`
- 各實證 bug（音效 clobber / 相鄰性 / 型別正確性 / 時序 / 硬編位址 / math intrinsic /
  stack-probe）：`../build_test/playtest_bugs.md`
