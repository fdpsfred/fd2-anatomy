# FD2.LE 連結時的 LE binary layout

從 FD2.LE 的 LE header / object table / page map / 入口程式 (`crt_equivalent_entry_start` + `crt_equivalent_dos_main_bootstrap`) 反推出的 wlink 連結結果。本文件描述「FD2.LE 長甚麼樣」，對應的 wlink 命令列重建寫在 `wlink_settings.md`。

## LE Header

| 欄位 | 值 | 說明 |
|---|---|---|
| signature | `'LE'` | Linear Executable (不是 LX)。Watcom v2 wlink `system dos4g` 的預設格式 |
| cpu_type | 2 | i386 |
| os_type | 1 | OS/2（LE format 規範要求；DOS/4GW 模組仍標 OS/2）|
| module_flags | `0x00000200` | bits 17:15 = 0 → Program / executable；bits 9:8 = 2 → PM-compatible (wlink dos4g 預設旗標) |
| page_size | 0x1000 (4096) | 標準 LE page |
| num_pages | 71 | 全 71 個 data page（檔內 70 個全 page + 1 個 0x4D2 byte 尾 page）|
| last_page_size | 0x4D2 (1234) | object 3 尾 page 有效 byte 數 |
| autodata_obj | 2 | DGROUP = object 2 |
| EIP_obj / EIP | 1 / 0x2C964 | 啟動位址 = 0x10000 + 0x2C964 = `0x3C964` (`crt_equivalent_entry_start`) |
| ESP_obj / ESP | 2 / 0x56B0 | 初始 ESP = 0x50000 + 0x56B0 = `0x556B0` (object 2 頂端)|
| stack_size (header) | 0 | LE header 欄位沒用，stack 包在 DGROUP 內，實際大小 4K (見下文) |
| heap_size (header) | 0 | LE header 欄位沒用，heap 由 DOS/4GW DPMI 動態 alloc |
| import_mod_count | 0 | 沒 DLL imports（DOS/4GW 全 INT 21h / DPMI INT 31h call）|
| resource_table_count | 0 | 無 resource section |
| resident_name | `"f2"` (ordinal 0) | 模組內部名 = 「f2」(來自主 .obj 檔名)，與外部檔名 FD2.LE 不同 |
| entry_table | empty | 無 exported entry point |
| debug_info | 0 | 無 debug section（連結時未開 `debug all`，或事後 `wstrip`）|
| nonresident_name | 0 | 無 |

## Object Table (3 objects)

| obj | base | virtual_size | flags | 內容 | wlink 對應 |
|---|---|---|---|---|---|
| 1 | `0x10000` | `0x3EBD9` (256985 B / ~257 KB) | R+X+Preload+32-bit | `_TEXT` (CGROUP) — 全部 code | `format LE` / `system dos4g` 預設 |
| 2 | `0x50000` | `0x56B0` (22192 B / ~22 KB) | R+W+Preload+32-bit | DGROUP = _DATA + CONST + _BSS + STACK | autodata，由 wlink 自動 group |
| 3 | `0x60000` | `0x34D2` (13522 B) | R+W+Preload+32-bit | FAR_DATA / 非 DGROUP 大型 data tables | 由 `#pragma data_seg` 或 source-level segment rename 推到自己的 group |

Object 2 與 object 3 中間有 0xA950 byte 的 unused 虛擬地址空間（wlink 對每個 group 對齊到 64K 邊界）。

### Object 1 (_TEXT, 0x10000..0x4EBD8)

全部是 code。code 區內混 4 個 pool：fd2 / ail / crt / binary_artifact。詳細路由規則寫在 `../emission/pool_routing.md`。Object 1 內也存放部分由 Watcom CRT (MATH387 系列) emit 的「const data 嵌在 code segment」table — 例如 `data_crt_emu387_internal_constant_database_174b @ 0x49A06`、`data_crt_emu387_x87_opcode_dispatch_table_176ptrs @ 0x49AB4`、`data_crt_trig387_sin_octant_dispatch_table @ 0x3C796`。這些 const 屬 Watcom 9.5a 提供的 lib obj，不是 FD2 自寫。

### Object 2 (DGROUP, 0x50000..0x556AF)

從低位址到高位址依 Watcom 預設 DGROUP 順序排列：

```
0x50000..~0x537E0   CONST / CONST2     (字串字面值 "rb"/"wb"/"FD2.SAV"/"FDICON.B24"/"FD2.TMP"/
                                        "Out of Memory ..."/CRT error messages/dispatch tables...)
~0x537E0..0x539F1   _DATA              (CRT iomode/scanf/printf ptr、math name table、tzname、
                                        matherr default thunk、最後一筆 = XI ctor table @ 0x539A0..0x539F1)
0x539F2..0x53FFF    page padding       (file image 內，全 0)
0x54000..0x546AF    _BSS               (zero-fill at load；FD2/CRT/AIL globals 約 1712 B)
0x546B0..0x556AF    STACK + cmdline    (4096 B，與 cstart cmdline buffer 重疊使用，見下節)
                                        ↑ Initial ESP = 0x556B0 (top exclusive)
```

LE 檔案實際存了 4 個 page (16384 B = 0x4000) of object 2，剩下的 0x16B0 byte (5808 B) 由 LE loader zero-fill。檔內 `0x539F2..0x53FFF` 已經是 0（位於 file image 內、僅做 padding）；`0x54000..0x556AF` 在檔內不存在，由 loader 補 0。

#### Stack 與 cmdline buffer 重疊

`data_crt_cmdline_and_env_path_buffer_4kb @ 0x546B0` 是 cstart 在 startup 早期填入 PSP cmdline + env path 的 4 KB 緩衝區。它的虛擬地址 `0x546B0..0x556AF` 與 initial ESP=0x556B0 開始往下長的 stack 完全重疊。

Watcom v2 `system dos4g` 的 cstart 設計：
1. LE loader 把 ESP 設成 obj2 頂端 0x556B0。
2. cstart 用 EBP-relative 暫存器存取自己的 frame，把 PSP cmdline 解析後寫進 `[0x546B0..]`（buffer 起點等於「stack 底」）。
3. 解析完 cmdline 後 `__InitRtns` 與 `__CMain` 把 stack 用滿，buffer 自動被覆蓋 — 由於 cmdline 已經被 main 函數複製到自己的 argv 陣列，buffer 不再需要。

實際 stack size = 4 KB；對應 wlink `option stack=4K`（Watcom dos4g 預設是 8K，FD2 顯式縮成 4K）。

### Object 3 (FAR_DATA, 0x60000..0x634D1)

非 DGROUP 的獨立 data group。內容全為 FD2 工程師寫的初始化遊戲資料 + 一小部分 runtime state：

| 範圍 | 內容 |
|---|---|
| `0x60000..0x6017C` | 雜項：palette cycle state、pathfind state、sprite blit dims |
| `0x6017D..0x602AB` | orphan / padding (303 byte) |
| `0x602AC..0x615FC` | `data_fd2_battle_item_effect_table` (215 entries × 23 byte = 4945 byte) |
| `0x615FD..0x61954` | orphan dead payload (856 byte) |
| `0x61955..0x619A8` | `data_fd2_battle_weapon_attack_anim_pattern_ptr_table_21` (21 × 4) + `_script_pool_84b` (84 byte) |
| `0x619A9..0x619FC` | weapon attack anim script pool 尾 + 對齊 |
| `0x619FD..0x61AF8` | `data_fd2_battle_spell_effect_table` (36 × 7) |
| `0x61AF9..0x61DA0` | `data_fd2_battle_enemy_data_table` (68 × 10) |
| `0x61DA1..0x620A0` | `data_fd2_battle_character_base_table` (32 × 24) |
| `0x620A1..0x626B2` | `data_fd2_battle_character_growth_table` (68 × 11) |
| `0x626B3..0x627A2` | `data_fd2_battle_spell_learning_table` (20 × 12) + 對齊 |
| `0x627A3..0x627C4` | `data_fd2_graphics_glyph_blit_state` + RNG seed |
| `0x627D8..0x62FFF` | `data_fd2_chapter_cutscene_event_script_ptr_table_106` (106 × 4) + 對齊 |
| `0x63000..0x634D1` | `data_fd2_chapter_cutscene_event_script_NNN` 106 個 script blob |

混了大資料表（4945 B item table）與小狀態變數（30 B sprite blit state）— **不能用 size threshold 解釋**。最合理的 source-level 結構：FD2 工程師在這些 source file 用 `#pragma data_seg("FAR_DATA")` 顯式把 segment 改名，wlink 自動把所有 `FAR_DATA` class segment group 起來變成 object 3。

Object 3 內的 pointer table（cutscene script ptr table、weapon attack anim ptr table）含 889 byte 的 internal fixup record，由 LE loader 在 load time 把 relocation base 加進去。

## Page Map / Fixup Section / Data Pages

| 區段 | file offset | size | 內容 |
|---|---|---|---|
| LE header | `0x00..0xC3` | 196 B | 上表所有欄位 |
| Object Table | `0xC4..0x10B` | 72 B (3×24) | 3 個 object descriptor |
| Object Page Map | `0x10C..0x227` | 284 B (71×4) | 每 page: 3-byte BE page# + 1-byte flag (全 LEGAL=0) |
| Resident Name Table | `0x228..0x22D` | 6 B | `"f2"` (ordinal 0) + terminator |
| Entry Table | `0x22E` | 1 B | terminator (無 export) |
| Fixup Page Table | `0x22F..0x34E` | 288 B (72×4) | 每 page 開頭的 fixup record offset |
| Fixup Record Table | `0x34F..0xE378` | 57386 B | 8000+ 條 fixup record（obj1=54004 B、obj2=2493 B、obj3=889 B）|
| Padding | `0xE379..0xE547` | 463 B | data_pages_off 對齊到 file boundary |
| Data Pages | `0xE548..0x546A1` | 287954 B (70×4096 + 1×1234) | obj1 63 page + obj2 4 page + obj3 4 page (尾 page 1234 B) |

70 個 4 KB page + 1 個 1234 byte page = 287954 byte data pages 區。

全部 71 page 的 page-map flag 都是 `LEGAL`（0）— 沒用 iterated/RLE compressed (`ITERATED=1`)、沒用 zero-fill 標記 (`INVALID=2` / `ZEROED=3`)。zero-fill 區域 (`0x54000..0x556AF`) 是靠「virtual_size > file pages × page_size」自動補 0，不靠 page flag。

## Fixup 統計

- 全部 8000+ 條 fixup，大宗 src_type = `0x07` (32-bit offset，flat memory model 標準) + `0x17` (帶 src list 的 32-bit offset，wlink 對同一 target 多 source 的批次紀錄)。
- Target type 幾乎全為 `internal ref`（low 2 bits = 00）— 沒有 imported by name / ordinal（與 `import_mod_count=0` 一致）。
- Internal ref 的 target obj 分布：obj 1 ← obj 2 / obj 3 (function pointer tables)；obj 1 ← obj 1 (data pointers 嵌在 code 內的 const)；obj 2 ↔ obj 3 (cutscene script ptr table 等)。

## 入口流程

1. **LE loader** (DOS/4GW.EXE 載入 LE 模組後):
   - 把 3 個 object 載到 linear 0x10000 / 0x50000 / 0x60000
   - 套用 fixup section 對 internal ref 加上 relocation base
   - zero-fill object 2 的 0x54000..0x556AF
   - 設 CS:EIP = obj1:0x2C964 = `0x3C964`，SS:ESP = obj2:0x56B0 = `0x556B0`
   - JMP 到 entry

2. **`crt_equivalent_entry_start @ 0x3C964`** (2-byte JMP thunk):
   - JMP `crt_equivalent_dos_main_bootstrap @ 0x3C9DE`

3. **`crt_equivalent_dos_main_bootstrap @ 0x3C9DE`** (Watcom 9.5a cstart.obj 對應 `_cstart_`):
   - DPMI host detect：INT 21 AX=3000h 取回 EAX 高 16-bit signature
     - `0x4458 "DX"` → DOS/4G，variant_id=0x22，env selector=0x2C
     - `0x4243 "CB"` → DOS/4GW，variant_id=9，PSP via EDX+0x10
     - 其他 → 試 INT 21 AX=FF00h DX=0x78 (Phar Lap 386|DOS 偵測)
       - 成功 → variant_id=1，ES=PSP
       - 失敗 → variant_id=0 (PMODE/W 或 generic)
   - 抓 DOS major.minor 存進 `data_crt_dos_version_major/minor`
   - 從 PSP cmdline (`PSP[0x80]` 長度、`PSP[0x81..]` 字串) trim leading space 後 copy 到 `0x546B0`
   - 掃 env 找 `NO87=`/`no87=` (case-fold via OR 0x20202020) → 命中數存 `data_crt_math_init_skip_word`
   - 把 env 後段 program path 接在 cmdline 後面（共享 4 KB buffer）
   - **BSS zero-init**：`REP STOSD` 從 `0x539EC` 寫 0x331 個 dword (0xCC4 byte) 到 `0x546B0`
   - CALL `__InitRtns @ 0x45D9A` (跑 XI ctor chain @ 0x539A0..0x539F1 的 10 個 entries)
   - JMP `__CMain @ 0x45D4B` (call `fd2_main`，main return 後 `__FiniRtns` + INT 21 AH=4Ch exit)

4. **`fd2_main`** — FD2 遊戲主邏輯入口。

## 與 LE 規範的對齊驗證

- `loader_section_size = 0xE2B6` ✓ matches `fixup_section_off + fixup_section_size - obj_table_off` = `0x22F + 0xE14B - 0xC4`
- `data_pages_off = 0xE548` ✓ matches end of fixup section + 463 byte alignment padding
- `virtual_size(obj 1) = 0x3EBD9` ✓ matches code area Ghidra `.object1: 0x10000..0x4EBD8`
- `virtual_size(obj 2) = 0x56B0` ✓ matches `.object2: 0x50000..0x556AF` (initial ESP=0x556B0 = top+1)
- `virtual_size(obj 3) = 0x34D2` ✓ matches `.object3: 0x60000..0x634D1` (= 3*4096 + 1234)
- `last_page_size = 0x4D2` ✓ matches obj 3 尾 page 有效 byte 數
- Page map flag 全 LEGAL + virtual_size > file_pages*page_size 兩個訊號合在一起 → obj 2 zero-fill 區從 0x54000 開始

## FD2.EXE 包裝

`fd2_game_files/FD2.EXE` (357074 byte) = 10424 byte MZ-format DOS bind stub + FD2.LE 模組 (從 file offset 0x28B8 開始).

- MZ stub 的 strings 含 `DOS4GPATH` / `dos4gw.exe` / `dos4g.exe` / `WATCOM C Run-Time system code is provided ... (c) Copyright by WATCOM Systems Inc. 1988-1992`
- 行為：使用者在 DOS 直接執行 FD2.EXE → MZ stub 找 `DOS4GPATH` env var 或 PATH 上的 `dos4gw.exe` / `dos4g.exe` → `EXEC` 它把 FD2.EXE 自己當第一個 argument 傳入
- DOS/4GW.EXE (244716 byte，獨立檔) 接管後讀 FD2.EXE 內部的 LE 模組 (從 e_lfanew=0x28B8 開始)，做 protected mode setup + load LE
- 此 stub 是 Watcom v2 wlink `system dos4g` 預設打包進去的「small loader stub」，不是把整個 DOS/4GW.EXE 嵌進來
- Stub 的 MZ header: pages=21, last_page=178, hdrsize=6 para (96 byte), CS:IP=0000:0210, SS:SP=028B:0800, n_relocs=6
- Stub 的 SHA-1: `b3abb6da8acecec47dae9b3a454aa5aad88c3d16`

開發者只需要把 FD2.EXE 和 DOS4GW.EXE 都放在遊戲目錄，user 執行 FD2.EXE 即可。

## 與其他 KB 文件交互

- 編譯器版本與 lib 構成見 `../crt/fid_match.md`
- 1342 個 function pool 路由 + emit_action 規則見 `../emission/pool_routing.md`
- DOS/4G hook `__hook387 @ 0x49955`（引用字串 `RATIONAL DOS/4G` @ `0x51760`）負責 387 emulator 對 DOS/4G extender 的 INT vector hooking
