# FD2.LE 的 LE binary layout

FD2.LE 是 DOS 32-bit Linear Executable，由 Watcom C/C++ 9.5a 編譯、以 DOS/4GW
protected-mode extender 在 386 以上環境執行。本文件是 FD2.LE 靜態結構的唯一正典：
LE header、object table、DGROUP/FAR_DATA 內部排列、page map、fixup section、以及外層
FD2.EXE 的包裝方式。對應的 `wlink` 連結命令重建寫在 `wlink_settings.md`；程式從 entry
point 開始的 startup 與遊戲主迴圈流程寫在 `../../program_info/overview.md`。

原始 LE 模組（不含外層 stub）約 346,650 byte。

## LE Header

| 欄位 | 值 | 說明 |
|---|---|---|
| signature | `'LE'` | Linear Executable（不是 LX）。Watcom 9.5a wlink `system dos4g` 的預設格式 |
| cpu_type | 2 | i386 |
| os_type | 1 | OS/2（LE format 規範要求；DOS/4GW 模組仍標 OS/2）|
| module_flags | `0x00000200` | bits 17:15 = 0 -> Program / executable；bits 9:8 = 2 -> PM-compatible（wlink dos4g 預設旗標）|
| page_size | 0x1000 (4096) | 標準 LE page |
| num_pages | 71 | 全 71 個 data page（檔內 70 個全 page + 1 個 0x4D2 byte 尾 page）|
| last_page_size | 0x4D2 (1234) | object 3 尾 page 有效 byte 數 |
| autodata_obj | 2 | DGROUP = object 2 |
| EIP_obj / EIP | 1 / 0x2C964 | 啟動位址 = 0x10000 + 0x2C964 = `0x3C964`（`_cstart_`）|
| ESP_obj / ESP | 2 / 0x56B0 | 初始 ESP = 0x50000 + 0x56B0 = `0x556B0`（object 2 頂端）|
| stack_size (header) | 0 | LE header 欄位沒用，stack 包在 DGROUP 內，實際大小 4K（見下文）|
| heap_size (header) | 0 | LE header 欄位沒用，heap 由 DOS/4GW DPMI 動態 alloc |
| import_mod_count | 0 | 沒有 DLL imports（全走 INT 21h / DPMI INT 31h call）|
| resource_table_count | 0 | 無 resource section |
| resident_name | `"f2"` (ordinal 0) | 模組內部名 = 「f2」（來自主 .obj 的 basename），與外部檔名 FD2.LE 不同 |
| entry_table | empty | 無 exported entry point |
| debug_info | 0 | 無 debug section（連結時未開 `debug all`，或事後 `wstrip`）|
| nonresident_name | 0 | 無 |

## Object Table（3 objects）

| obj | base | virtual_size | flags | 內容 | wlink 對應 |
|---|---|---|---|---|---|
| 1 | `0x10000` | `0x3EBD9`（256985 B / ~257 KB）| R+X+Preload+32-bit | `_TEXT`（CGROUP）-- 全部 code | `format LE` / `system dos4g` 預設 |
| 2 | `0x50000` | `0x56B0`（22192 B / ~22 KB）| R+W+Preload+32-bit | DGROUP = _DATA + CONST + _BSS + STACK | autodata，由 wlink 自動 group |
| 3 | `0x60000` | `0x34D2`（13522 B）| R+W+Preload+32-bit | FAR_DATA / 非 DGROUP 大型 data tables | 由 source-level segment rename 推到自己的 group |

三個 object 的 base 與 virtual_size 與 Ghidra 段界一致（`.object1: 0x10000..0x4EBD8`、
`.object2: 0x50000..0x556AF`、`.object3: 0x60000..0x634D1`）。Object 2 與 object 3 中間有
0xA950 byte 的 unused 虛擬地址空間（wlink 對每個 group 對齊到 64K 邊界）。

### 不能用位址範圍判斷 function 屬於哪一類

Object 1 是連續的一整塊 code，內部把三類來源的 function **互相交錯擺放**，沒有清楚的
library / 遊戲分區：FD2 自寫遊戲邏輯、Watcom CRT helper、Miles AIL library 三類函式散布
於整個 object 1。可觀察到某些區段某一類較密集（例如 `0x37000..0x3C2E6` 是 AIL 函式較密集
的段落，`__CHK @ 0x36CD7` 附近是 CRT helper 較密集的段落），但每個密集段內部仍混入其他類別。

因此判別任一 function 屬於哪類，**不能依 address range**，只能看：function 名稱前綴
（`AIL_*` / `crt_*` / `crt_equivalent_*` / 已命名的 `fd2_*` game function）、callee 模式、
以及字串引用。四 pool 的分類法與各 pool 的判別規則見 `../equivalence/pool_classification.md`。

### Object 1（_TEXT, 0x10000..0x4EBD8）

全部是 code。code 區內混四個 pool：fd2 / ail / crt / binary_artifact，路由規則見
`../equivalence/pool_classification.md`。Object 1 內也存放部分由 Watcom CRT（MATH387 系列）
emit 的「const data 嵌在 code segment」table -- 例如
`data_crt_emu387_internal_constant_database_174b @ 0x49A06`、
`data_crt_emu387_x87_opcode_dispatch_table_176ptrs @ 0x49AB4`、
`data_crt_trig387_sin_octant_dispatch_table @ 0x3C796`。這些 const 屬 Watcom 9.5a 提供的
lib obj，不是 FD2 自寫。

Object 1 內未定型的 padding 區段（約 27 KB）已依緊鄰的命名來源分類並套上 `byte[N]` data
type，避免 Ghidra listing 留下 undefined byte。分類明細（binary_artifact NOP/padding 事實）
見 `../equivalence/pool_classification.md`。

### Object 2（DGROUP, 0x50000..0x556AF）

從低位址到高位址依 Watcom 預設 DGROUP 順序排列：

```
0x50000..~0x537E0   CONST / CONST2     (字串字面值 "rb"/"wb"/"FD2.SAV"/"FDICON.B24"/"FD2.TMP"/
                                        "Out of Memory ..."/CRT error messages/dispatch tables...)
~0x537E0..0x539F1   _DATA              (CRT iomode/scanf/printf ptr、math name table、tzname、
                                        matherr default thunk、最後一筆 = XI ctor table @ 0x539A0..0x539F1)
0x539F2..0x53FFF    page padding       (file image 內，全 0)
0x54000..0x546AF    _BSS               (zero-fill at load；FD2/CRT/AIL globals 約 1712 B)
0x546B0..0x556AF    STACK + cmdline    (4096 B，與 cstart cmdline buffer 重疊使用，見下節)
                                        ^ Initial ESP = 0x556B0 (top exclusive)
```

LE 檔案實際存了 4 個 page（16384 B = 0x4000）of object 2，剩下的 0x16B0 byte（5808 B）由 LE
loader zero-fill。檔內 `0x539F2..0x53FFF` 已經是 0（位於 file image 內、僅做 padding）；
`0x54000..0x556AF` 在檔內不存在，由 loader 補 0。

#### Stack 與 cmdline buffer 重疊

`data_crt_cmdline_and_env_path_buffer_4kb @ 0x546B0` 是 cstart 在 startup 早期填入 PSP
cmdline + env path 的 4 KB 緩衝區。它的虛擬地址 `0x546B0..0x556AF` 與 initial ESP = 0x556B0
開始往下長的 stack 完全重疊。Watcom 9.5a `system dos4g` cstart 的設計是：先把 PSP cmdline
解析後寫進 buffer（起點等於 stack 底），main 把 cmdline 複製到自己的 argv 後，stack 用滿即
自動覆蓋這塊 buffer。實際 stack size = 4 KB，對應 wlink `option stack=4K`（Watcom dos4g 預設
是 8K，FD2 顯式縮成 4K）。

### Object 3（FAR_DATA, 0x60000..0x634D1）

非 DGROUP 的獨立 data group。內容全為 FD2 工程師寫的初始化遊戲資料 + 一小部分 runtime state：

| 範圍 | 內容 |
|---|---|
| `0x60000..0x6017C` | 雜項：palette cycle state、pathfind state、sprite blit dims |
| `0x6017D..0x602AB` | orphan / padding（303 byte）|
| `0x602AC..0x615FC` | `data_fd2_battle_item_effect_table`（215 entries × 23 byte = 4945 byte）|
| `0x615FD..0x61954` | orphan dead payload（856 byte）|
| `0x61955..0x619A8` | `data_fd2_battle_weapon_attack_anim_pattern_ptr_table_21`（21 × 4）+ `_script_pool_84b`（84 byte）|
| `0x619A9..0x619FC` | weapon attack anim script pool 尾 + 對齊 |
| `0x619FD..0x61AF8` | `data_fd2_battle_spell_effect_table`（36 × 7）|
| `0x61AF9..0x61DA0` | `data_fd2_battle_enemy_data_table`（68 × 10）|
| `0x61DA1..0x620A0` | `data_fd2_battle_character_base_table`（32 × 24）|
| `0x620A1..0x626B2` | `data_fd2_battle_character_growth_table`（68 × 11）|
| `0x626B3..0x627A2` | `data_fd2_battle_spell_learning_table`（20 × 12）+ 對齊 |
| `0x627A3..0x627C4` | `data_fd2_graphics_glyph_blit_state` + RNG seed |
| `0x627D8..0x6297F` | `data_fd2_chapter_cutscene_event_script_ptr_table_106`（106 × 4 = 424 byte，無尾端對齊）|
| `0x62980..0x634D1` | 106 個 cutscene script blob 連續排放（共 2898 byte，緊接表尾，entry 0 = 0x62980），至 object 3 結尾 |

Object 3 混了大資料表（4945 B item table）與小狀態變數（30 B sprite blit state），
**不能用 size threshold 解釋**。最合理的 source-level 結構是：FD2 工程師在這些 source
file 顯式把 segment 改名到 `FAR_DATA` class，wlink 自動把所有 `FAR_DATA` class segment
group 起來變成 object 3（重建做法見 `wlink_settings.md`）。

Object 3 內的 pointer table（cutscene script ptr table、weapon attack anim ptr table）含
889 byte 的 internal fixup record，由 LE loader 在 load time 把 relocation base 加進去。

## Page Map / Fixup Section / Data Pages

| 區段 | file offset | size | 內容 |
|---|---|---|---|
| LE header | `0x00..0xC3` | 196 B | 上表所有欄位 |
| Object Table | `0xC4..0x10B` | 72 B (3×24) | 3 個 object descriptor |
| Object Page Map | `0x10C..0x227` | 284 B (71×4) | 每 page：3-byte BE page# + 1-byte flag（全 LEGAL=0）|
| Resident Name Table | `0x228..0x22D` | 6 B | `"f2"` (ordinal 0) + terminator |
| Entry Table | `0x22E` | 1 B | terminator（無 export）|
| Fixup Page Table | `0x22F..0x34E` | 288 B (72×4) | 每 page 開頭的 fixup record offset |
| Fixup Record Table | `0x34F..0xE378` | 57386 B | 大量 fixup record（obj1=54004 B、obj2=2493 B、obj3=889 B）|
| Padding | `0xE379..0xE547` | 463 B | data_pages_off 對齊到 file boundary |
| Data Pages | `0xE548..0x54A19` | 287954 B (70×4096 + 1×1234) | obj1 63 page + obj2 4 page + obj3 4 page（尾 page 1234 B）|

全部 71 page 的 page-map flag 都是 `LEGAL`（0）-- 沒用 iterated/RLE compressed
（`ITERATED=1`）、沒用 zero-fill 標記（`INVALID=2` / `ZEROED=3`）。zero-fill 區域
（`0x54000..0x556AF`）是靠「virtual_size > file pages × page_size」自動補 0，不靠 page flag。

## Fixup 統計

- 大宗 src_type = `0x07`（32-bit offset，flat memory model 標準）+ `0x17`（帶 src list 的
  32-bit offset，wlink 對同一 target 多 source 的批次紀錄）。
- Target type 幾乎全為 `internal ref`（low 2 bits = 00）-- 沒有 imported by name / ordinal
  （與 `import_mod_count=0` 一致）。
- Internal ref 的 target obj 分布：obj 1 <- obj 2 / obj 3（function pointer tables）；
  obj 1 <- obj 1（data pointers 嵌在 code 內的 const）；obj 2 <-> obj 3（cutscene script
  ptr table 等）。

## 入口點

LE loader 把三個 object 載到 linear 0x10000 / 0x50000 / 0x60000，套用 fixup section 對
internal ref 加上 relocation base，zero-fill object 2 的 0x54000..0x556AF，然後設
CS:EIP = obj1:0x2C964 = `0x3C964`、SS:ESP = obj2:0x56B0 = `0x556B0`，JMP 到 entry。

Entry `0x3C964` 是 stock Watcom 9.5a `_cstart_`（由 `system dos4g` 從 cstart.obj 連入，非
FD2 自寫）。`_cstart_` 做 DPMI host / extender 偵測、cmdline 與環境變數解析、BSS zero-init、
執行 XI constructor chain（`__InitRtns`），再進 `__CMain` 呼叫 `main`。逐步的 startup 流程與
其後的遊戲主迴圈（`main` -> AIL 初始化 -> 載入資源 -> 章節迴圈）見
`../../program_info/overview.md`。

## 與 LE 規範的對齊驗證

- `loader_section_size = 0xE2B6` matches `fixup_section_off + fixup_section_size - obj_table_off` = `0x22F + 0xE14B - 0xC4`
- `data_pages_off = 0xE548` matches end of fixup section + 463 byte alignment padding
- `virtual_size(obj 1) = 0x3EBD9` matches code area Ghidra `.object1: 0x10000..0x4EBD8`
- `virtual_size(obj 2) = 0x56B0` matches `.object2: 0x50000..0x556AF`（initial ESP = 0x556B0 = top+1）
- `virtual_size(obj 3) = 0x34D2` matches `.object3: 0x60000..0x634D1`（= 3×4096 + 1234）
- `last_page_size = 0x4D2` matches obj 3 尾 page 有效 byte 數
- Page map flag 全 LEGAL + virtual_size > file_pages×page_size 兩個訊號合在一起 -> obj 2
  zero-fill 區從 0x54000 開始

## FD2.EXE 包裝

`fd2_game_files/FD2.EXE`（357074 byte）= 10424 byte MZ-format DOS bind stub + FD2.LE 模組
（從 file offset 0x28B8 開始）。

- MZ stub 的 strings 含 `DOS4GPATH` / `dos4gw.exe` / `dos4g.exe` / `WATCOM C Run-Time
  system code is provided ... (c) Copyright by WATCOM Systems Inc. 1988-1992`。
- 行為：使用者在 DOS 直接執行 FD2.EXE -> MZ stub 找 `DOS4GPATH` env var 或 PATH 上的
  `dos4gw.exe` / `dos4g.exe` -> `EXEC` 它並把 FD2.EXE 自己當第一個 argument 傳入。
- DOS/4GW.EXE（244716 byte，獨立檔）接管後讀 FD2.EXE 內部的 LE 模組（從 e_lfanew = 0x28B8
  開始），做 protected mode setup + load LE。
- 此 stub 是 Watcom 9.5a wlink `system dos4g` 預設打包進去的「small loader stub」，不是把
  整個 DOS/4GW.EXE 嵌進來。
- Stub 的 MZ header：pages=21, last_page=178, hdrsize=6 para（96 byte），CS:IP=0000:0210,
  SS:SP=028B:0800, n_relocs=6。SHA-1 = `b3abb6da8acecec47dae9b3a454aa5aad88c3d16`。

開發者只需把 FD2.EXE 和 DOS4GW.EXE 都放在遊戲目錄，user 執行 FD2.EXE 即可。

### DOS/4GW 是什麼

DOS/4GW 是 Tenberry Software 的 32-bit protected-mode extender（Rational DOS/4G 家族的
Watcom 綁定版）。它把 16-bit DOS BIOS 與 32-bit flat memory model 接起來，讓 FD2 能以單一
平坦定址空間直接讀寫 BIOS data area（例如 `0x40:0x1A` 的鍵盤 buffer pointer），並用 DPMI
INT 31h 動態配置記憶體。`_cstart_` 的 extender 偵測就是為了在 DOS/4G、Phar Lap 386|DOS、
Intel Code Builder 等不同 extender 下都能定位 PSP 與環境區塊（FD2 走 DOS/4G 這條路徑）。

### FD2.LE 位址 <-> 另一發行版 FD2.EXE 檔案 offset 對照（跨版本）

另有一組把 FD2.LE 內大型資料表對應到另一個 FD2 發行版 FD2.EXE 檔案 offset 的
對照，供跨版本定位遊戲資料（例如共用的存檔編輯器）使用。兩組 object 用不同的對應偏移：

| 位址段 | FD2.LE 位址 | 另一版 FD2.EXE file offset |
|---|---|---|
| object 2 小資料表（job_magic_resist / job_crit 等 u8/u32 array）| `0x00051xxx` | `0x76xxx`..`0x77xxx` |
| object 3 大資料表（item / spell / character / enemy / growth / cutscene）| `0x00060xxx`..`0x00063xxx` | `0x792xx`..`0x7Bxxx` |

此表指向一個**與本專案 Ghidra 分析對象（FD2.LE）不同的 FD2.EXE 發行版**，未在本專案內獨立
複核，故僅作跨版本參考、不是 FD2.LE 自身的 file offset。可驗證且有價值的結論是：Watcom 把
「含巨量靜態資料的 struct」放進 object 3（FAR_DATA initialization image），把「簡單的 u8/u32
array」留在 object 2 與 CONST 字串混放 -- 這正是上面兩組 object 用不同對應偏移的原因，也與
object table 觀察一致。

## 與其他 KB 文件交互

- wlink 連結命令重建與 wcc386 旗標：`wlink_settings.md`
- 編譯器版本與 lib 構成：`../crt/fid_match.md`
- 四 pool 分類法與 binary_artifact padding 事實：`../equivalence/pool_classification.md`
- Watcom Easy OMF-386 格式 quirks：`omf_386.md`
- entry point 之後的 startup 與遊戲主迴圈：`../../program_info/overview.md`
- DOS/4G hook `__hook387 @ 0x498D6`（body 內於 `0x49955` 引用字串 `RATIONAL DOS/4G` @ `0x51760`）負責 387
  emulator 對 DOS/4G extender 的 INT vector hooking
