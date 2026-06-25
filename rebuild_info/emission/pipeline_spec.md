# Emit pipeline 規格

emit pipeline 的目標：從 Ghidra 1375 個 function 的 decompiled state 產出 C
source code，經 Watcom C/C++ 9.5a（原 binary 同版編譯器，見
`../crt/fid_match.md`）重新 compile / link 出 functionally equivalent 的
DOS executable（FD2.LE）。目標等價層級為 **Layer 2（functionally-exact）**，
不追求 Layer 3（byte-exact）。

本檔規範 emit pipeline 在處理 function boundary 與 fall-through 模式時必須
遵守的規則，避免把 Ghidra 的 Function entity 直接當 C function 輸出而破壞
binary 行為。

## Pool × emit_action 路由

每個 function 依 (category × emit_action) 走不同 emit 路徑：

| category            | emit_action         | 數量           | emit 策略                                                                                                       |
| ------------------- | ------------------- | -------------- | --------------------------------------------------------------------------------------------------------------- |
| `ail`             | `link_vendor_lib` | (即時 dump)    | **不 emit**。Watcom AIL3DIG / AIL3MDI 靜態 library 直接 link，FD2 source 端只保留 `extern` declaration  |
| `crt`             | `link_vendor_lib` | 202            | **不 emit**。Watcom 9.5a CLIB3S / EMU387 直接 link（193 個 lookup-resolved Watcom 真符號 + 8 個 fast-path `PUBLIC_CRT_SYMBOLS` 不在 lookup + 1 個 `__int7` 內部 subroutine `crt_emu387_int7_fptan_opcode_worker_4c630`）          |
| `crt`             | `emit_fd2_source` | 12             | **emit 為 C source**。涵蓋 12 個 `crt_equivalent_*`（Watcom CRT 行為等價但 byte 不 match 任一 lib obj） |
| `fd2`             | `emit_fd2_source` | 640            | **emit 為 C source**。game logic / glue / dispatch / wrapper / dead code / 8 個 CRT-style primitive       |
| `binary_artifact` | `skip_artifact`   | 93             | **不 emit**。Watcom 9.5a 重 compile 自動生成 alignment NOP padding                                          |
| **合計**      |                     | **1375** |                                                                                                                 |

路由規則由 `tools/program_analysis/build_call_graph.py` 內 `categorise()` /
`emit_action_for()` 機械決定（純看 name 前綴 + lookup 表）。emit pipeline
即時跑 `build_call_graph.py` 後讀 `workspace/call_graph/call_graph.json`，
每個 node 的 `category` / `emit_action` 兩欄即可分流。

## Data emit 路由

data 端 1300 items 的 emit 路徑由 verdict 內 `caller_pool` + `actions` 決定，
與 function 端類似但有 data-specific 規則：

| `caller_pool` (verdict 欄)   | emit_action           | emit 策略                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                 |
| ------------------------------ | --------------------- | --------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| `ail`                        | `link_vendor`       | AIL3DIG/AIL3MDI 靜態 lib byte-preserve；string / table / lookup 全部從 lib 帶入，FD2 source 端只寫 `extern char *` declaration                                                                                                                                                                                                                                                                                                                                                                                                                                                                          |
| `crt`                        | `link_vendor`       | Watcom 9.5a CLIB3S 對應的 .obj 含相同字串 / table 與 state：fp_not_loaded、matherr msg、math fn name table、asctime packed table、stack_overflow 字串、heap descriptor (`0x527B0..0x527D7`)、FILE pool (`0x52840..0x52A47`)、atexit chain (`0x527D8..0x527E0`)、bootstrap state (`0x52800..0x52833`)、DOS extender callback ptr (`0x527EC`)、env block selector (`0x52834..0x52838`)、rand seed (`0x527E8`)、getch pushback (`0x52824`)、_fmode (`0x52A49`)、387 emulator state (`0x53770..0x53776`、`0x527F4..0x527F5`) 等全部 vendor-resolved；rebuild 時 EXTDEF 到 9.5a CLIB3S 對應 symbol |
| `fd2` (const)                | `emit_c_const`      | game-side 字串 / 常數 emit 為 C source 字面值（`static const char *` / array literal）；典型例：FD2 .DAT filename 8 個 / OOM msgs / debug fmts / RGB palette tables / shake offset tables / 各 spell 表                                                                                                                                                                                                                                                                                                                                                                                                 |
| `fd2` (BSS-style state)      | `emit_c_const_zero` | game-side zero-init writable globals emit 為 `static <type> <name>;`（隱含 zero-init，C standard 保證），不寫 explicit `= 0`；典型例：runtime_char_array_ptr / cursor state / portrait cache count                                                                                                                                                                                                                                                                                                                                                                                                    |
| `none` (padding)             | `skip_align`        | wlink 自動 emit segment alignment，不需 source 端寫                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                       |
| `fd2` (explicit align bytes) | `emit_align_pad`    | 顯式 byte 序列 padding（如 `byte[3] = {0,0,0}` 不對齊到 4-byte boundary）；非 `skip_align` 因 source 端 emit declaration 順序須精確控制 byte offset；典型例：spell_cast_cinematic_phase_handler_table 前 3-byte pad                                                                                                                                                                                                                                                                                                                                                                                   |

### 規則 E-2: Watcom 9.5a string-pool 不跨 `.obj` boundary dedup

Watcom 9.5a 編譯每個 `.obj` 時內部會 dedup string literals，但**不跨 `.obj` 去重**。
同字串若在多個 `.obj` 用到，binary 中會有多 copy。emit pipeline 必須對應：

- 每個 fd2 `.obj` source 各自 emit 自己的字串 literal，不做 source-tree 全域 pool
- vendor `.obj` 的多 copy 字串透過 `link_vendor` 自動帶入，FD2 source 不重新 declare

FD2.LE 觀察到的 cross-`.obj` 多 copy 字串：

| 字串                                         | 位址                        | 所在 `.obj`                                  |
| -------------------------------------------- | --------------------------- | ---------------------------------------------- |
| `"Out of timer handles\n"`                 | 0x5128c + 0x51580           | install_driver.OBJ + mdi_driver_setup.OBJ      |
| `"Unrecognized digital audio file type\n"` | 0x51428 + 0x5146d           | allocate_file_sample.OBJ + set_sample_file.OBJ |
| `" Out of Memory !!!\n"`                   | 0x50004 + 0x50023 + 0x50037 | 3 fd2 .obj copies                              |

### 規則 E-3: Sub-string anchor pointer（mid-string pointer offsets）

caller 透過 `BYTE_ARRAY_<addr>` 形式的 pointer 引用 string item 內部 offset
（非從 item start），典型例：

- `"FD2.SAV\0rb\0"` 的 `"rb\0"` 部分（fopen mode）被另外 PUSH 為 mode 字串
- `"IO_ADDR\0IRQ\0"` 的 `"IRQ\0"` 部分被 strnicmp 取 4 byte 作 INI key 比對
- AIL log nesting indent prefix 的 `+0` / `+2` byte offset

emit pipeline 必須：

- 對每個 sub-string anchor，emit 的 C source 寫成 `(parent_string + offset)`
  或直接寫該子字串的 literal（Watcom 9.5a string-pool 會在 `.obj` 內部 dedup
  使其指向相同的字串末尾）
- **不**單獨 emit 為 named global — Ghidra 內 `BYTE_ARRAY_<addr>` symbol 是
  audit 工具的 disambiguation label，非真正獨立 data item
- 9 個已觀察的 sub-string anchor 列於 D3 group_3 proposal 詳細 KB sync

### 規則 E-3b: 絕對位址引用一律改 symbol（禁 hardcoded immediate）

原版 code 常以絕對位址 immediate 常數引用字串 / 資料（例如把某 `.DAT` 檔名字串的位址直接寫死成
`push 0x51a4d`）。Layer-2 不追求 byte-exact、linker 會自由擺放資料，所以**任何寫死的絕對位址在
rebuild 都會指錯**。emit pipeline 必須把這類引用改成 **symbol 引用**，讓 linker 自己填正確位址。

**實證（"File not found" 開場退出）**：`fd2_load_dat_resource` 把原版字串位址寫死成 immediate，
rebuild linker 把字串擺到別處 → `fopen` 拿到空檔名失敗，開場就退出。修法是把 9 處 hardcoded 字串
位址全改 symbol。詳見 `../build_test/playtest_bugs.md` E 類。這條與上面 E-2 / E-3 的 string-pool
處理同源 —— 字串一律以 source-level literal / named symbol emit，絕不以絕對位址 immediate 引用。

### 規則 E-4: FD2 game-side `.DAT` filename strings 走 emit_c_const

8 個 FD2 game `.DAT` filename strings 在 `.object2` string 區（0x51a43..0x52388）
與 vendor strings 鄰接，audit bucket 因 segment 落入 D3「vendor_string」group，
但 caller decomp 證實 100 % 由 `fd2_*` callers 引用：

| addr    | filename    | 主要 callers                                                            |
| ------- | ----------- | ----------------------------------------------------------------------- |
| 0x51a43 | FDTXT.DAT   | fd2_load_save_and_init_engine / main / fd2_load_chapter_battle_data |
| 0x51a4d | FDOTHER.DAT | 50+ fd2_* callers (palette + cinematic + sprite assets)                 |
| 0x51a59 | FDFIELD.DAT | fd2_load_save_and_init_engine / fd2_load_chapter_*                      |
| 0x51a65 | FDSHAP.DAT  | fd2_play_full_combat_cinematic / fd2_execute_special_attack_skill / ... |
| 0x51a70 | DATO.DAT    | fd2_display_dialog_scene / fd2_run_status_screen_member_menu            |
| 0x51a79 | FDMUS.DAT   | fd2_set_bgm_track_with_fade                                             |
| 0x52381 | BG.DAT      | fd2_play_full_combat_cinematic / fd2_play_spell_cast_cinematic          |
| 0x52388 | FIGANI.DAT  | 19 callers (combat / spell / special skill / chapter intro)             |

**emit_action = emit_c_const**（**非** link_vendor）。emit pipeline 一律以
`verdict.emit_action` 為單一事實來源，不要靠 audit bucket label 推 emit 路徑。

### 規則 E-5: data type 與 caller-side 用法不一致時即時修正

當 Ghidra data type 與 caller 端實際語意不符（例如 string type 但 caller 端
視為 writable buffer），須就地用 `apply_data_type` 修正。已修正案例：

- `0x53698` — Ghidra 原 `string` size=8（"SAMPLE\0"），caller
  `AIL_internal_set_GTL_filename_prefix_inner` 證實是 128B writable buffer
  被 strcpy 覆寫，audit 內就地套 `char[128]`

### 規則 E-6: Boundary unification（多 stale items 合併為 single struct/array）

當多個鄰接的 Ghidra-auto-split items 實際是 caller copy loop 取 single
struct / array 時，用 `apply_data_type(addr, type, clear_existing=true)`
unify boundary，把原本分散的 items 合併為 single semantic item。已修正案例：

- `0x526DA dword[4]` ← 合併自 `0x526D7 byte[5] tail` (truncated to byte[3]) +
  `0x526DC..0x526DD unmapped` + `0x526DE byte[4]` + `0x526E2 BYTE_ARRAY[8]`
  → `data_fd2_ui_chapter_intro_dialog_corner_offset_table_a` (corner offset
  table, 4 個 dword 連續被 caller copy loop 拿)
- `0x526EA dword[4]` ← 合併自 `0x526EA byte[4]` + `0x526EE byte[4]` +
  `0x526F2 BYTE_ARRAY[8]` → `_table_b` (identical content, separate copy
  for fd2_wait_input_with_chapter_dialog_blink caller)
- `0x53A18 pointer[5]` ← 合併自 5 個獨立 dword (dialog_frame_layers /
  dialog_frame_layer_extra_a / dialog_open_anim_state / _b / _c) →
  `data_fd2_dialog_dialog_frame_layer_save_buffer_ptrs`
- `0x53A30 dword[4]` ← 合併自 4 個獨立 dword (combat_speech_bubble_state_a/b/c/d)
  → `data_fd2_battle_combat_speech_bubble_pos_pairs` (4 個 (x, y) 對戰泡位)
- `0x53EF2 byte[16]` ← 合併自 4 個 `undefined4` (zero-init menu state template) →
  `data_fd2_ui_field_command_menu_state_template`；同類 sibling templates 5 個
  集中於 0x53EF2..0x53F32 (4 個 int[4] + 1 個 int[4] for item command menu)
- `0x523E1 byte[7]` ← 合併自 `undefined4(0x523e1) + undefined2(0x523e5) + undefined1(0x523e7)` → `data_fd2_battle_summon_spell_8slot_visibility_table`
  (per-slot visibility mask 0/1, 跨 state 4/5 inverse gate)
- `0x523E8 dword[7]` / `0x52404 int[7]` / `0x52420 int[8]` / `0x52440 int[8]` /
  `0x52460 int[12]` / `0x524A8 int[10]` / `0x524D0 int[10]` ← 同類 boundary
  fix，全部是 summon-spell animation 系列 const tables 被 Ghidra 切成
  `undefined4 + undefined4 + byte[N-8]` 三段、各自還原為單一連續 array。
- `0x524F8 int[5]` / `0x52511 int[10]` / `0x52539 byte[16]` / `0x5266B short[30]` /
  `0x526A7 byte[18]` / `0x5274E byte[5]` / `0x52840 byte[520]` (CRT FILE pool) /
  `0x5360C byte[128]` (AIL pan_volume LUT) / `0x53720 byte[80]` (AIL MIDI timbre
  packet) ← boundary fix，summon spell variant C/D/E offset tables +
  revive/promote cost table + class-change key item table + CRT FILE pool + AIL
  vendor data tables。同模式：Watcom emit const array → Ghidra split as
  `undefined4 + undefined4 + byte[tail]` → unify via `apply_data_type(type, clear=true)`.
- `0x52659 byte[6]` ← misclassification fix: 原 `typeB_portrait_id` (byte) +
  `typeC_portrait_id` (byte) 各為單獨 byte，實際 caller 透過
  `(&typeB_portrait_id)[chapter_intro_menu_cursor_state]` 索引 6 byte 表 →
  `data_fd2_chapter_intro_menu_speaker_portrait_id_table`. 原 typeB/typeC
  literal access (state==0, state==4) 等效於 array[0], array[4]，所以
  rename + boundary unify 不影響語意.

#### Multi-field heterogeneous boundary split

某些 Ghidra-auto-merged 大 lump 實際是多個**不同 type** 連續 field（非單一 array）。
已觀察案例：

- `0x52760 byte[52]` ← split 為 `ushort target_width + uint dst_buf + uint src_buf + pointer[10] frame_dispatch_table + 2B trailing zeros` (ANI.DAT decoder state block) — 4 個獨立 field + dispatch table，必須 split。
- `0x5266B byte[78]` ← split 為 `short[30] cost_table + byte[18] key_item_table` (two distinct access patterns: short indexed by `(job_id-1)*2`, byte indexed by `portrait_id + 0x3C`)。
- `0x52828 byte[10]` ← split 為 `byte[8] reserved + ushort math_init_skip_word @ +0x8`，其中 reserved 8B 為 Watcom CRT 結構性 padding 但 byte[10] 大 lump 在 access via array idx [8] 時實際命中 word.

修復 SOP：apply 個別 type 到每個 sub-region (clear_existing=true 自第一個位址，後續 type apply 自動取代覆蓋)；plate 內標明 split 邊界。新 split-出來的 sub-items 在下個 worklist rebuild 時會自動被識別為新 defined data。

已觀察模式：Watcom 9.5a emit `static const int arr[N] = {...};` 在 .object2
.rodata 區段時，Ghidra auto-analyzer 看到頭兩個 dword 因配對寫入 stack copy
loop (decomp 顯示為 dword copy `*piVar5 = *puVar4;`) 而 disassemble 為 dword，
然後第 3 個 dword 起的 trailing bytes 因落在 anonymous byte 區被合併為單一
byte[N-8] block。修復 SOP：

1. 從 caller decomp 抽 const array 預期 size N
2. `apply_data_type(addr, int[N] or byte[N], clear_existing=true)` 一次 unify
3. 對每個被消除的 sibling addr 記 `merged_boundary` verdict (parent_struct
   欄填 unified item 命名)

Mass-merge 機制 (per item)：

1. `apply_data_type(target_addr, new_type, clear_existing=true)` — 清既有
   overlapping items + apply new type
2. `rename_data(target_addr, new_semantic_name)` — 給新 unified item 命名
3. `set_decompiler_comment(target_addr, plate)` — 寫完整 plate 包含子 field
   原 stale 名稱與用途
4. 對每個被消除的舊 item, `record_verdict.py <old_addr> status=fixed actions=["merge_boundary"] notes="merged into <new_addr>"`

emit pipeline 對 unified item emit 為 single `static <type> <name>[N] = {...};`
declaration；不為被消除的 sub-items emit 任何 source-level declaration。

### 規則 E-7a: LE FIXUP-only data refs from CRT ctor / init table

有一類「無直接 code xref，僅透過 CRT auto-init constructor table
的 4-byte pointer entry 在 load time relocate」的 data items。典型例：

- `data_crt_jmp_thunk_to_sys_init_387_emulator @ 0x3cbcc` — 5B `E9 disp32` JMP thunk → `__sys_init_387_emulator @ 0x45e36`；
  唯一 access 來自 CRT ctor table entry @ 0x539c4 透過 LE FIXUP 在 load time 寫入 0x3cbcc 值
- `__delay_init @ 0x3dc9f` — 46B 函數；
  唯一 caller 是 CRT ctor table entry @ 0x539b8（同樣只 load-time fixup）

`get_xrefs_to(addr)` 在這類 case 會返回 1 個 `[DATA]` xref 指向 ctor table，
看似很弱但實為 CRT init pathway 的正規 invocation。emit pipeline 規則：

- ctor table entry 須 emit 為 `static void (*ctor_table[])(void) = { fn1, fn2, ... };` 或
  Watcom-equivalent `#pragma initialize` declaration，由 wlink 自動產生 init
  invocation
- thunk function (data type byte[5]) 須 byte-preserve `E9 disp32` 形式；C source
  不易直接表達，emit pipeline 可選擇 (a) 把 thunk + target 合併 emit、(b) inline
  asm `__asm jmp target` 或 (c) 直接讓 ctor table 指向 target 跳過 thunk
- 不可用「無 caller = dead code」啟發式刪除這類 function — load-time fixup 是
  Watcom CRT init 唯一 invocation channel

### 規則 E-7b: 387 emulator state init constants → link_vendor_lib

Watcom 387 software emulator (`__int7 @ 0x49d98`) 的初始狀態
常數，分散於 `.object1` 0x499fc..0x49a05 共 4 個 items（uint32 @+6c / uint32
@+70 / uint16 @+74 + 1 個 174B 多 sub-table constant database @0x49a06）。
__int7 在 INT 7 (Coprocessor-Not-Available) exception entry 把這些常數
load 到 FPU emulator state struct（DGROUP-resident，由 `__sys_init_387_emulator`
分配）作 default-clear init。

emit_action 全部 = `link_vendor_lib`（Watcom CLIB3S `__int7.obj` 的內嵌
常數，FD2 不直接寫；rebuild 時 EXTDEF 到 9.5a CLIB3S 的 `__int7` symbol）。

注意：FD2.LE 是 DOS/4G hardware-FPU 環境，CPU 不會觸發 INT 7，所以這條 path
runtime 不可達；emit 仍須完整保留以維持 byte-identical `.obj` size match
（部分 CRT lookup 用 obj size 作 verified=byte_match 條件）。

### 規則 E-7c: Ghidra immediate-vs-data xref false positive

已觀察 case：Ghidra 對 instruction immediate constant 與
.object1 base address 碰撞時，會把 immediate 誤分類為 [DATA] xref。例：

- `ADD EAX, 0x10000` (16-bit wrap-around adjust)
- `CMP [EBP+0x14], 0x10000` (64K size threshold)
- `MOV EBP, 0x10000` (64K counter init)

這些 instruction operand 是純 numeric constant，不是 data pointer，但因
立即值剛好 = .object1 base (0x10000)，Ghidra get_xrefs_to(0x10000) 會列出
所有這類 instruction site。

emit pipeline 規則：

- **不可信賴 Ghidra [DATA] xref count 作 emit_action routing 判據** — 須結合
  指令類型 (ADD/CMP/MOV with immediate operand) 過濾掉 false positive
- 對 0x10000 (.object1 base) 與 0x50000 (.object2 base) 與 0x60000 (.object3 base)
  等 segment-base-address 的 xref 須額外驗證 immediate-vs-data 屬性

對應 `record_verdict.py` 內 `notes` 欄須 explicitly 標 false-positive 即可，
emit pipeline 從 verdict 取 `actions=["rename_data"]` 即 (不依 xref count) 排
程 emit。

### 規則 E-7d: Dangling LE-FIXUP-patched pointer storage（loader 寫 0 reader）

有一類「LE FIXUP 表確有 source 記錄、但 binary 內 0 function 讀取該 storage」
的 data items。典型例：

- `data_orphan_52a4d_dangling_ptr_to_open_files_list_head_final_unreachable_unknown @ 0x52A4D` (4B,
  unaligned) — LE FIXUP src 0x52A4D → trg 0x541AC (= `data_crt_open_files_list_head_ptr`
  本身 12 callers live)。但 0x52A4D 這個 storage slot 從未被任何 instruction read。
  推測：Watcom CRT `.obj` declared 一個 alias pointer 變數
  (`static FILE **open_files_alias = &open_files_list_head_ptr;`)，使用該 alias 的
  function 被 DCE 但 linker 仍 emit storage + fixup table 仍 patch value。

判定條件：

- LE FIXUP target_addr_to_sources 內有此 storage 為 source（loader 會寫）
- Ghidra `get_xrefs_to(addr)` 0 READ refs
- byte pattern search 0 in-code immediate

emit pipeline 規則：

- **不可信賴「有 LE FIXUP record = live data」啟發式** — fixup 只代表 loader
  會在 load time 寫 value，不代表任何 runtime code 會 read
- 須以 `get_xrefs_to(addr)` 過濾出真實 reader 後再判斷 emit_action
- 對 dangling case 走 `skip_unreachable_data` (或 BSS-preserve with explicit
  "// dangling: loader-patched but unread" comment)
- 對應 verdict `indirection_chase_method` 標 `B_target_identified_but_no_in_binary_reader`

### 規則 E-7e: Co-dead data + accessor chain（同一 .obj 內互引用但無外部 caller）

有「data + 專屬 accessor function 互相 reference，整個 .obj
無外部 caller」的死碼鏈結。典型例：

- `data_fd2_orphan_packed3_table @ 0x60181` (99×3-byte entries; 位於 303B data
  unit @ 0x6017D 的 +4 偏移) +
  `fd2_get_orphan_packed3_table_entry @ 0x4DB84` (accessor returning
  `&table[idx*3]`) — 兩者互引用但 accessor 本身 0 callers，0 LE FIXUP target。
  Watcom linker 因 mutual reference 保留兩者，但外部 caller 已被 DCE。

判定條件：

- data item 唯一 caller 是某個 accessor function
- 該 accessor 本身的 callers/xrefs/LE-FIXUP-target 全部為 0

emit pipeline 規則：

- **EMIT 整個 (data + accessor) 鏈**：這條死碼鏈實際存在於原始 FD2.LE（Watcom 因
  mutual reference 未 dead-strip，rule 上方已述），byte-faithful 等價要求 rebuild
  重現它；skip 會讓 rebuild 少掉這些 bytes、與原版 binary 發散（eqcheck FAIL）。
  data 走 `emit_c_const`、accessor 走正常 function emit。
  src 現況：src/table/orphan.c 定義 table、src/table/table.c 定義 accessor，整體
  eqcheck PASS 驗證 emit 正確。
- 對應 verdict `indirection_chase_method` 標 `B_C_resolved_codead_chain`（分類保留，
  動作為 emit 而非 skip）

### 規則 E-7f: 大型 "blob orphan" 必須先做 internal LE FIXUP target probe

`data_orphan_615fd_unknown_blob_1024b` (1024B, 標為 orphan) 內部包含一個
LIVE pointer table + script pool 子區段：

- 0x615FD..0x61954 (856B): 真 orphan leading prefix（已標 final_unreachable_unknown）
- 0x61955..0x619A8 (84B): `data_fd2_battle_weapon_attack_anim_pattern_ptr_table_21`
  (pointer[21] indexed by weapon_type，consumer = `fd2_get_attack_anim_pattern_for_weapon`)
- 0x619A9..0x619FC (84B): `data_fd2_battle_weapon_attack_anim_pattern_script_pool_84b`
  (6 個 variable-length scripts shared by 21 weapon types)

Lesson：大型 blob (>500B) 若全部標 orphan 而未做 internal probe，會錯失內含
live sub-region。

audit 規則（落到 plan §H' Method B 補充）：

- 對 size ≥ 500B 的 orphan，須在 `target_addr_to_sources` 內列舉「blob range
  內任一 aligned 4-byte address 是否為 fixup target」
- 若內部任一 offset 有 fixup target，須拆分為 sub-regions 並對每段獨立評估
- emit pipeline 對 split-out sub-region 各自獨立 routing (live = `emit_c_const`,
  dead = `skip_unreachable_data`)

### 規則 E-7g: Pointer-array jump/dispatch table 之 fix_off32_* 命名共存

**強制 invariant**：每個 LE FIXUP-populated 指標陣列（jump table / dispatch
table / ctor pointer list）必須在 Ghidra 套上 `pointer[N]` 型別覆蓋完整 N×4 byte
範圍（或對 inline jump table 用 `byte[N*4]` 等效覆蓋）。**只套 4B `undefined *`
等於只定義 entry[0] 一個 pointer**，其餘 N−1 個 entries 仍是獨立 LE FIXUP
auto-label，會出現在 `list_data_items` 內並造成 audit registry false negative。

**設計**: 套 `pointer[N]` 後，原本的 `fix_off32_*` Ghidra symbol **不會自動消失** —
array indexing 與 fix_off32_* label 兩者**共存於 symbol table**（但**不再出現於
list_data_items**，因被 array extent 吸收為 child entry）。共存是 known
exception，per Ghidra LE FIXUP analyzer 行為設計。

#### FD2.LE 內所有已正確處理的 pointer array

| base    | array name                                                    | total slots |     absorbed non-NULL |
| ------- | ------------------------------------------------------------- | ----------: | --------------------: |
| 0x3C796 | `data_crt_trig387_sin_octant_dispatch_table`                |           8 |        7 (除 slot[0]) |
| 0x40334 | `data_ail_dig_driver_configure_format_swap_jump_table`      |           4 |                     3 |
| 0x40344 | `data_ail_dig_driver_configure_channel_init_jump_table`     |           4 |                     3 |
| 0x41548 | `L_AIL_min_sample_buf_switchtable_41548`                    |           4 |                     3 |
| 0x4180C | `data_ail_voc_dispatcher_v2_chunk_type_jump_table`          |          10 |                     9 |
| 0x47638 | `data_ail_dig_mixer_format_finaliser_dispatch_table_a`      |         128 | 59（其餘 68 為 NULL） |
| 0x47838 | `data_ail_dig_mixer_per_sample_dispatch_table_b`            |         128 | 71（其餘 56 為 NULL） |
| 0x49AB4 | `data_crt_emu387_x87_opcode_dispatch_table_176ptrs`         |         176 |                   175 |
| 0x4AA34 | `data_crt_emu387_int7_inline_dispatch_table_8ptrs_at_4aa34` |           8 |                     7 |
| 0x4ACD0 | `data_crt_emu387_int7_inline_dispatch_table_8ptrs_at_4acd0` |           8 |                     7 |
| 0x4B184 | `data_crt_emu387_int7_inline_dispatch_table_8ptrs_at_4b184` |           8 |                     7 |
| 0x61955 | `data_fd2_battle_weapon_attack_anim_pattern_ptr_table_21`   |          21 |                    20 |

合計 **12 個 pointer-array dispatch tables**。每個 array 的 base plate
（透過 `set_decompiler_comment` PRE_COMMENT）末段須附「Audit naming convention
for the N absorbed slots (known exception)」段落。

#### 套 `pointer[N]` 對 list_data_items 與 symbol table 的影響

| 套型別後                   | list_data_items                         | symbol table                                 |
| -------------------------- | --------------------------------------- | -------------------------------------------- |
| array base @ 0x{base}      | 1 個 entry (array, N×4 B 大小)         | primary =`data_*` semantic name            |
| 各 child slot 0x{base+i*4} | **不出現** (被 array extent 吸收) | `fix_off32_*` (secondary, ANALYSIS source) |

#### 典型例

- 0x49AB4..0x49D73 範圍 = `pointer[176]`，array name `data_crt_emu387_x87_opcode_dispatch_table_176ptrs`
- 0x49AB4 (slot[0]) symbols：primary = array base name；secondary = `fix_off32_00049ab4`
- 0x49AC8 (slot[5]) symbol：`fix_off32_00049ac8`（secondary，未刪）
- worklist `current_name` for 0x49AC8：`data_crt_emu387_x87_opcode_dispatch_table_176ptrs[5]`（邏輯 array index）

#### Emit pipeline 規則

- 對 absorbed slot 一律以 **array indexing 為 canonical access** — emit 源碼用
  `dispatch_table[N]` 不用獨立 variable
- `fix_off32_*` symbol 是 Ghidra LE FIXUP metadata 不是 semantic label，emit
  pipeline 忽略
- 不對 absorbed slot 跑 sync_check 比對 worklist vs Ghidra symbol（會出
  false positive：worklist 寫 `[N]` 樣式但 Ghidra 維持 fix_off32_*）
- 若手動 `delete_label fix_off32_*`：可選但無強制需求；下次 Ghidra reanalyze
  可能還原
- audit registry 內每個 absorbed slot 在 `verdicts.jsonl` 留一筆獨立 entry：
  (a) 4-byte pointer value (= LE FIXUP target = handler 位址)、
  (b) `actions = ["absorbed_into_array_at_<base_addr>"]`、
  (c) `parent_struct = <array name>`
- parent array 的 plate (PRE_COMMENT @ array base) 須包含此 known exception
  段落，給 reviewer 在 Ghidra UI 直接看到雙重命名說明

#### 驗證 invariant 的 audit 步驟

每次新增/修改 pointer-array 後執行：

1. `list_data_items` 後 grep `fix_off32_*` — 若 array 範圍內仍有 leftover，表示
   parent type 沒套到完整大小（典型錯誤：套了 `undefined *` 4B 只覆蓋 entry[0]）
2. 對該 base 用 `apply_data_type pointer[N]` 或 `byte[N*4]` 套完整大小
3. 重新 grep verify leftover = 0
4. **不要** 刪除 `fix_off32_*` symbol 本身（known exception，是 Ghidra 設計）

對應違反場景：若有人 grep `fix_off32_*` 並把它當「未處理 data」誤判，須查
array containing 該 addr 是否已有 `..._<N>ptrs` 樣式 array type **且 size = N×4**。
若 size 不足（如只 4B），對 base 補套完整型別；若 size 已對則 fix_off32_* 是
無害 secondary symbol，emit pipeline 忽略。

### 規則 E-7h: Inline jump table within vendor function body

某些 vendor (CRT / AIL) function body 內含 inline 跳轉表 (`switch/case`
dispatch)，table base 是 4-byte aligned 連續 pointer 陣列，caller 指令是
`JMP CS:[reg*4+<table_base>]` (8-byte FF /4 indirect jump)。Table 不在 function
prologue 前後，而是嵌在 function code 中段、由附近的指令直接索引。

**FD2.LE 內已 audit 的 vendor-function inline jump tables**（__int7 共 8 個）：

| table base | unaligned? | entries | distinct vs default                                                                      | indirect jump addr |
| ---------- | ---------- | ------: | ---------------------------------------------------------------------------------------- | ------------------ |
| 0x4A8F5    | yes (1)    |       8 | 1 (0x4A915) + 7 default (0x4A8E8)                                                        | 0x4A8ED            |
| 0x4AA34    | no  (0)    |       8 | 4 (0x4AA54/0x4AA61/0x4AA6E/0x4AA9F) + 4 default                                          | 0x4AA2C            |
| 0x4AAB5    | yes (1)    |       8 | 7 (0x4AAD5/0x4AB18/0x4AB5D/0x4ABA2/0x4ABE7/0x4AC2C/0x4AC71) + 1 default                  | 0x4AAAD            |
| 0x4ACD0    | no  (0)    |       8 | 8 distinct (0x4ACF0/0x4ACFD/0x4AD44/0x4B359/0x4B3A2/0x4B407/0x4AD90/0x4ADB1, no default) | 0x4ACC8            |
| 0x4ADDA    | yes (2)    |       8 | 8 distinct (0x4ADFA/0x4AE24/0x4AE6B/0x4AE78/0x4B431/0x4B449/0x4AED0/0x4AEDD)             | 0x4ADD2            |
| 0x4AEF2    | yes (2)    |       8 | 1 (0x4AF12) + 7 default                                                                  | 0x4AEEA            |
| 0x4B184    | no  (0)    |       8 | 1 (0x4B1A4) + 7 default                                                                  | 0x4B17C            |
| 0x4B339    | yes (1)    |       8 | 1 (0x4B4BA) + 7 default                                                                  | 0x4B331            |

All parent function = __int7 (0x49d98..0x4cbcd, Watcom MATH387S `IF@LOG`/`IF@LOG2`/`IF@LOG10` 等
math routine 的共用體 dispatcher) ・全部 caller 指令格式 `2E FF 24 9D <base 4B>` =
`JMP CS:[EBX*4+<base>]` (8-byte FF /4 indirect jump using EBX as 0..7 index)。

**Unaligned-base case** (0x4AAB5, 0x4ADDA, 0x4AEF2, 0x4AA88-style)：當 table base 不是
4-byte aligned 時，Ghidra LE FIXUP 啟發式只給 base 一個 fix_off32_* label，後續 7 個 entries
不獲得獨立 fix_off32_* — 但實際還是有 LE FIXUP 寫入 entry 的 4 byte pointer。Worklist 內
**只有 base 一筆 row**，audit 期間 base plate 內列全 8 個 entries layout 即可，無需個別
absorbed verdict。

**Aligned-base case** (0x4A8F5, 0x4AA34, 0x4ACD0, 0x4B184)：每個 entry 都有獨立 fix_off32_*，
worklist 內 base 一筆 + 7 個 absorbed slots 各一筆（共 8 row per table）；absorbed slot verdict
`actions = ["absorbed_into_array_at_<base>"]`。

**例外 (0x4B184)**：base 4-byte aligned 但 Ghidra 沒給 fix_off32_* label 也沒 defined_data —
audit 期間 manually `create_label`。entry[7] high byte (0x4B1A3) 因 pointer 高位元組為 `00`
被 Ghidra split 成獨立 byte CU，命名為
`data_crt_emu387_int7_inline_dispatch_table_4b184_entry7_high_byte_at_4b1a3`，歸 base table 語意子節點。

特徵與 E-7g (array absorbed slot) 之差異：

- E-7g 的 array 是 **獨立 data item**（被外部 function pointer call）；E-7h 的
  jump table 是 **同一 vendor function body 內部** 的局部分支控制流
- E-7g array base 通常獨立位於 `.object1` data subregion；E-7h table 與 parent
  function body 連續（table 前 8 bytes 是 `2E FF 24 9D <table_base 4B>` indirect jump
  instruction，table 後緊接 case body code）
- E-7g 在 worklist 內 array base 一筆、absorbed slots 各一筆；E-7h 同樣處理 —
  table base 命名為 `data_<vendor>_<func>_inline_dispatch_table_<N>ptrs_at_<addr>`，
  table entries 在 verdict 內 `actions = ["absorbed_into_array_at_<table_base>"]`

命名規則：

- table base: `data_crt_<func>_inline_dispatch_table_<N>ptrs_at_<addr>` 或
  `data_ail_<func>_inline_dispatch_table_<N>ptrs_at_<addr>`
- **必須含 `_at_<addr>` suffix**：同一 vendor function 可能有多個 inline jump
  table（如 __int7 至少 2 個），用 base address 作 disambiguator

emit pipeline 規則：

- emit_action = `byte_preserve_within_vendor_function`：parent function 整體
  emit_action 通常是 `link_vendor_lib` (Watcom MATH387S / AIL .obj resolves at
  link time)；inline jump table 是該 .obj bytes 的一部分，自動隨之 byte-preserve
- 不需要 emit C source 重建 — 整個 parent function 從 lib 連結
- 不影響 ail extraction (vendor 內部資料不外部 reference)

### 規則 E-8: BSS-style global declaration order matters

FD2 .object2 BSS 區的 globals 是 Watcom 編譯時按 source declaration order
連續排列的結果，混合 type (byte / word / dword / pointer / array) 不會自動
插入 alignment padding 來把 4-byte 變數對齊到 4-byte boundary。例如
`chapter_init_done_flag` (byte) @ 0x53A44 後緊接 `runtime_char_array_ptr`
(pointer) @ 0x53A45 — pointer 位於 non-4-byte-aligned addr。x86 unaligned MOV
完全合法，runtime 正常。這是 Watcom 預設行為，不是 issue。

emit pipeline 對 BSS group 只需要：

- 不需要 `#pragma pack` 或特殊 wlink config — Watcom default 行為已正確
- 若以 single struct 包裹 BSS group 則須 `#pragma pack(1)` 避免 struct member
  alignment 干擾；但個別 global declarations 自然 byte-pack，不需特殊處理

**⚠ 順序與相鄰只對「已定義 / 已初始化」的 global 成立，不適用 tentative BSS scalar** ——
見下方 E-8b。

### 規則 E-8b: BSS/COMDEF tentative scalar 不保證順序與相鄰 —— reader-as-array 必 emit 真 array

未初始化的 global（`int x;` 無初值）在 Watcom 是 **tentative definition**，編成 COMDEF/COMMON
record 交給 linker 合併。**linker 不保證這些 tentative scalar 的擺放順序與相鄰關係** —— 實測甚至是
**反序**（wlink map 證實）。因此原版若把多個相鄰全域當成一塊連續記憶體存取（array 索引
`(&g0)[i]`、或對相鄰全域做 struct punning），rebuild 一旦把它們 emit 成多個獨立 tentative scalar，
linker 就會把它們拆散、塞進別的全域之間，reader 讀到的是鄰居的 garbage。

**實證（炙焰刀施法平移 crash）**：`fd2_animate_bg_zoom_transition_in` 用 `bg_layer[idx % 3]` 把三個
BG layer 指標當 `uint32[3]`（@0x5410B/0F/13）。emit 成三個獨立 tentative scalar 後，wlink map 顯示
Watcom 反序擺放（layer_2 在最低位址），reader 的 `[1]`/`[2]` 讀到鄰居 spotlight_bg / split_bg_b 的
garbage 指標 → 餵 `fd2_rle_blit_sprite` wild read → protected-mode fault。另一實證是 28-byte
`union REGS` INT scratch 被拆成獨立 `uint8` 全域，相鄰 byte（`last_key` / `key_input_mode`）被
linker 拆散。詳見 `../build_test/playtest_bugs.md` B 類。

**規則**：**凡 reader 把多個 global 當 array 索引、或對相鄰全域做 struct punning，一律 emit 成單一的
真 array / struct**（C 標準保證 array element 與 struct member 升序相鄰），不可拆成多個 scalar。
Ghidra 端同步成 `dword[N]` / 對應 struct 型別，並用 wlink map（`tools/fd2_build/build_fd2.py --map`）驗證
實際 layout。

### 規則 E-9: Unaligned dword global access (Watcom default-alignment override)

當某個 4-byte global 的 binary 位址不滿足 4-byte alignment（addr % 4 ≠ 0），
但 caller 用 `MOV [addr], reg32` 整 dword access 時，emit C source 必須避免
Watcom 9.5a 預設 4-byte align 變數位址 — 否則 declared variable 與 binary
位址會錯位。

**FD2.LE 觀察案例**（單一已知 instance）：

- `data_fd2_animation_ani_decoder_dst_buf @ 0x52762`：4-byte uint global，
  `0x52762 % 4 = 2` (2-byte aligned)。由 `fd2_ani_decoder_set_target_buffer`
  以 `*(uint *)(0x52762) = dst_buf` 整 dword 寫入。x86 runtime 完全合法
  (unaligned MOV 沒有 fault)，但 emit 階段須處理。

emit 三種可選處理（pipeline 擇一）：

1. **`#pragma pack(1)` + struct wrap**：把 unaligned global 包進
   `#pragma pack(push,1) struct { ... } #pragma pack(pop)`，強制 byte-pack
   layout 使 dword field 落在指定位址。
2. **Byte stream + bit-cast**：宣告為 `byte[4]` 並用 `memcpy(&dst_buf, ptr, 4)`
   / `memcpy(ptr, &src, 4)` 做 access。avoids alignment concern entirely,
   compiler 可以選擇 unaligned MOV 或 4×byte 拆分。
3. **Linker placement**：用 wlink `ORDER` directive 強制把 global emit 到
   特定 byte offset；C declaration 仍是普通 `unsigned int`。需要 wlink config
   完成（issue #28 解後可用）。

預設選擇 **方法 2 (byte stream + bit-cast)** — 最 portable、不依賴 pragma、
不需要 wlink ORDER。

emit pipeline 對該 global 套用前須先掃描整個 DGROUP 找其他 unaligned 4-byte
globals (`addr % 4 != 0` AND `type == uint/int/ptr`)；目前已知只 0x52762 一筆。

## Fall-through 模式必須特別處理

對 FD2.LE 全 function 跑 fall-through audit（prev_fn 末尾 inst 有
fall-through 進 this_fn entry），共 25 個非 align_nop 案例分 6 種模式。emit
pipeline 對每個模式有強制處理規則。

### 模式 A: SHARED EPILOGUE STUB

**識別**: prev_fn 末尾無 RET，直接 fall-through 進 this_fn；this_fn 是 RET-only
共用 epilogue（純 stack cleanup + RET），多個 source function 也透過 tail-JMP
進入此 epilogue。

**例子**: `fd2_noop_stub_b43` 被 `fd2_load_chapter_battle_data`（fall-through）+
`fd2_play_rising_pre_cast_effect`（JMP）+ `fd2_play_variant_b_slide_pre_effect`（JMP）共用。

**完整清單**: 8 個 case — `fd2_noop_stub_b43` / `fd2_noop_stub_c49` /
`fd2_noop_stub_1011` / `fd2_noop_stub_1452` / `fd2_noop_stub_13994` /
`fd2_set_battle_anim_phase_to_1` / `AIL_internal_log_decrement_nesting` / `__GETDS`
（後者 byte_match 命中 Watcom CLIB3S `cstart.obj`，已歸 crt pool 但仍是
SHARED EPILOGUE 形態的 fall-through 對象）。

**emit 規則 A-1**：把 SHARED EPILOGUE 視為 fragment，**不**獨立 emit 為 C function。
改為：

1. 把 epilogue 程式碼 inline 到每個 source function 末尾（複製 `ADD ESP / POP regs / RET` 那 N 個 instruction 的等效 C 描述）
2. 或 emit 為 `__declspec(naked)` + 內嵌 `__asm` block（Watcom 支援）：

```c
__declspec(naked) void fd2_noop_stub_b43(void) {
    __asm {
        add esp, 0xc
        pop ebp
        pop edi
        pop esi
        pop ebx
        ret
    }
}
```

3. 在 source function 結尾改用 `goto epilogue_label;` 然後 label 寫 inline epilogue
   程式碼（純 C，無 inline asm）

### 模式 B: MULTIPLE ENTRY POINTS / SHARED BODY

**識別**: 兩個函式共用同一段邏輯主體，以不同 entry 進入做不同 setup，最終 JMP
進共用 body。prev_fn body 涵蓋共用 body 區段，this_fn body 只涵蓋 entry-specific
setup + JMP 進共用 body。

**例子**: `fd2_load_and_fade_in_cinematic_image`（載圖 + JMP fade loop）/
`fd2_play_palette_fade_to_black`（直接 JMP fade loop）共用 0x1f51e 起的 fade
主迴圈。

**完整清單**: 1 個 case — `fd2_play_palette_fade_to_black`（與 `fd2_load_and_fade_in_cinematic_image` 共用 0x1f51e 起 fade 主迴圈）。

（先前列入的 `crt_softfp_uint32_to_ld` 已透過 byte_match audit 證實為單一
PUBDEF `__Bin2String @ 0x4d9e1`（CLIB3S `i64tos.obj`，297B），其內部 CALL/POP EDI
idiom 屬 position-independent code 控制流，非 SHARED BODY 多 entry。已從清單移除。）

（`fd2_check_battle_end_condition @ 0x205be` 不屬模式 B：其 tail `0x2067d..0x206c4` 僅由本函式
entry JMP（0x205d5）與自身迴圈 back-edge 進入，xref 無其他來源；`fd2_init_battle_state_for_chapter @ 0x205da`
結尾為 unconditional JMP 0x17ee8（clear_keyboard_buffer wrapper，見 calling_convention.md 0x17ee8 列），
不 fall-through 進該 tail。這是 function-body interleave / out-of-line tail —— entry JMP 跳過 interleaved
的 init 函式、接續本函式自身 tail，re-emit 為單一 self-contained 函式，非多 entry shared body。）

**emit 規則 B-1**: 抽出共用 body 為 internal helper function，兩個 entry 各別
emit 為 wrapper：

```c
static void palette_fade_to_black_loop(int param_1, int param_2) {
    /* 共用 fade 主迴圈 */
}

void fd2_play_palette_fade_to_black(int frame_arg) {
    palette_fade_to_black_loop(frame_arg, 0);
}

void fd2_load_and_fade_in_cinematic_image(uint chapter_id, uint flag, uint frame_arg) {
    /* 載圖 setup */
    fd2_set_vga_palette_range(...);
    fd2_load_dat_resource(0x51a4d, 0x53a65, chapter_id);
    memset(0xa0000, 0xff, 0xfa00);
    fd2_play_ani_file_animation_sequence(...);
    palette_fade_to_black_loop(frame_arg, 0);
}
```

### 模式 C: HEADER-ONLY ENTRY

**識別**: prev_fn 只有「pre-PUSH 一些常數作為 args / 推 frame_size」這類 header
工作（prev body ≤ 15 bytes，無實質運算），fall-through 進真正做事的 this_fn。

**完整清單**: 4 個 case — `fd2_chapter_event_handler_18__unref_dialog` /
`fd2_chapter_event_handler_20__ch10_dialog` /
`fd2_chapter_event_handler_34__ch23_ai_ctrl` /
`fd2_chapter_event_handler_36__ch24_cinematic`。

**emit 規則 C-1**: prev 視為「先做 setup 然後 tail-call this_fn」的 wrapper，
explicit emit 兩個 function：

```c
void fd2_chapter_event_handler_20__ch10_dialog(void) {
    /* prev 只 PUSH 0x28 表示要把 frame size 0x28 傳給 this 的 __CHK */
    fd2_show_chapter_dialog_with_portrait_set_1(/* args derived from prev's pushes */);
}
```

**Frame_size mismatch 限制**: 這類 case 的 prev 通常 frame_size 不同於 this
（prev 自己 PUSH 一個 frame_size 給 `__CHK` 後 fall-through 進 this，this 自己
也會 PUSH 另一個 frame_size）。Watcom 編譯器自動產生
`PUSH <framesize>; CALL __CHK` prologue 時的 `<framesize>` 由 callee 自身
local 大小決定，**無法直接在 source 端為「同一個 wrapper function」指定與
callee 不同的 frame_size**。三種實作選項：

1. **Compiler pragma**：驗證 Watcom 9.5a 是否有 compiler-specific pragma 或
   `#pragma aux` 標記可控制單一 function 的 `__CHK` frame_size。優先選此 path。
2. **Inline asm prologue**：若 Watcom 無 pragma 支援，為這 4 個 case 手寫
   `__declspec(naked)` + `__asm` 顯式 emit `PUSH <prev_framesize>; CALL __CHK;
   ... ; JMP this_fn` 的 wrapper bytes，繞過 compiler 自動 prologue。
3. **Shared body helper**：把 this_fn body 抽成 internal helper，prev 與 this
   各別 emit 為 wrapper（呼叫 helper）。需 Layer 2 functional equivalence
   驗證 — wrapper 的 frame layout 與原 binary 不同，但行為等價。

實際選擇須在 wlink + Watcom 9.5a build pipeline 起來後 byte-level 比對驗證。

### 模式 D: DEAD FALL-THROUGH

**識別**: prev_fn 末尾的 fall-through 在執行流上死掉（INT/CALL 不返回 / 條件分支
全部把控制流帶回別處），this_fn 雖然是真 function 但這個 fall-through 路徑用不到。

**清單**: Watcom CRT noreturn 函數家族（`_exit @ 0x36df6` / `__exit @ 0x3cb91` /
`__exit_with_msg @ 0x3cb93` — 三者結尾 INT 21h AH=4Ch DOS terminate 不返回）+
`_cstart_ @ 0x3c964`（entry 2B JMP，後接 114B Watcom 版權字串 data，
fall-through 路徑物理上不可達）— 這幾類 prev_fn 的 fall-through 區段皆不可達；
個別案例的 prev → this pairing 待從 `tools/program_analysis/function_audit/data/verdicts.jsonl`
重新枚舉並 cross-check 對應 `data_align_*` boundary（見 open_issues.md DEAD
FALL-THROUGH re-enumerate 條目）。

**emit 規則 D-1**: prev 與 this 各自 emit 為一般 C function。Compiler 可能會
optimize 掉 fall-through 的「死碼」，但因為 dead 所以行為不變。

### 模式 E: DATA TABLE FRAGMENT

**識別**: function 之間的 byte 區段是 data table（jump table / 常數表 /
align fill）的 byte 區段，被 Ghidra 自動分析誤判 disassemble 為 code。
歷史上曾以 `AIL_internal_helper_<addr>` 命名為 function entity，當前已全部
移除 function entity 並重新 mark 為對應的 data 型別。

**當前清單**（全部已 mark 為 data，不再以 function entity 存在）：

| addr        | size    | 內容                                                                                                 | data 型別                                  | 標籤                                                                                              |
| ----------- | ------- | ---------------------------------------------------------------------------------------------------- | ------------------------------------------ | ------------------------------------------------------------------------------------------------- |
| `0x3c776` | 64 byte | IF@COS 前置 CRT MATH387S 常數池（含 32B byte constants + 4×4B fix_off32_* pointer + 16B byte tail） | `byte[32]` + 4×`dword` + `byte[16]` | (no fn-style label)                                                                               |
| `0x3c962` | 2 byte  | `00 00` zero pad（wlink segment 對齊 fill）                                                        | `byte[2]`                                | `data_align_3c962`                                                                              |
| `0x41548` | 24 byte | `AIL_internal_minimum_sample_buffer_size_inner @ 0x41560` 的 switch jump table                     | `dword[6]`                               | `L_AIL_min_sample_buf_switchtable_41548` + `LE_PAGE_52` + `switchdataD_*` + `fix_off32_*` |
| `0x41db7` | 8 byte  | `00 00 00 00 00 00 00 00` zero pad（AIL DPMI helper 後 wlink fill）                                | `byte[8]`                                | `data_align_41db7`                                                                              |

**emit 規則 E-1**: 因為已 mark 為 data 且不在任何 function entity 內，
emit pipeline iterate 函式時自然不會 emit 這些 byte：

- `0x41548` switch jump table：以 `dword[6]` array 存在 Ghidra 內，emit
  pipeline 對應 `AIL_internal_minimum_sample_buffer_size_inner` 的 switch
  分派可由 Watcom 9.5a 自行生 jump table（C source 寫 switch 即可）
- `0x3c962` / `0x41db7` align pad：完全 skip（Watcom 9.5a 重新對齊）
- `0x3c776` MATH387S 常數池：屬 CRT pool 走 `link_vendor_lib`，Watcom 9.5a
  CRT 自帶等效常數，無須 emit

### 模式 F: STATE-MACHINE INIT-ENTRY

**識別**: prev_fn 是 state-machine 的「初次進入 setup」（PUSH regs、初始化某 reg
為 0），fall-through 進 loop body that uses 該 reg as state；loop body 用完一輪
JMP 回自己。

**完整清單**: 1 個 case — `AIL_internal_voc_dispatcher @ 0x41834` (init: EBP=0) →
`AIL_internal_voc_dispatcher_v2 @ 0x4183d` (loop with state in EBP)。

**emit 規則 F-1**: 合併兩函式為一個 driver function：

```c
void AIL_state_driver(seq_state *st) {
    int phase = 0;
    while (1) {
        switch (st->next_event_byte) {
            case 0: /* ... */ break;
            ...
            default: return;
        }
        phase = next_phase;
    }
}
```

## 不變式（按等價程度分層）

emit pipeline 完成後 re-link 出的 binary 必須滿足下列三層 invariant。每層適用
範圍不同，對應驗證手段也不同。

### Layer 1: specification-exact（最低保證，全範圍）

重 link 出的 .EXE 在 DOSBox-X 內跑時，下列**外顯行為**必須與原 FD2.LE 完全一致：

- 30 章劇情的對話 / 過場 / 戰鬥流程
- FD2.SAV byte-level 兼容（讀原版存檔可繼續、新存的存檔原版可讀）
- BGM / SFX 觸發時機與曲目選擇
- 螢幕 pixel output（同 input scancode 序列下，每一 frame 的 mode13h buffer 內容相同）

驗證手段：DOSBox-X silent mode 跑 scripted gameplay session（按既定 input
scancode 序列），dump screen buffer / FD2.SAV / 觸發的 BGM track ID 與原版對比。

### Layer 2: functionally-exact（emit_action = emit_fd2_source 全部 652 個 function）

對於這 652 個 emit-out-of-source 的 function（640 個 `fd2_*` + 12 個
`crt_equivalent_*`），每個 function 在「相同 input register / stack / memory
state」下執行完，必須產出「相同的 return value / register state /
寫入 memory 的 bytes」。

不要求 instruction 級別 byte-相同 —— register allocation / instruction
selection / scheduling 細節由 source code 結構 + 編譯器旗標決定，emit pipeline
產出的 C source 可能與原 1998 年 FD2 source 結構不同，導致部分 function 即使
用同版編譯器（Watcom 9.5a）也 emit 出不同 instruction sequence。

**⚠ 例外：時序敏感的純 CPU 熱迴圈，codegen 要對齊原版指令數。** 功能等價的 codegen
簡化（例如有號 `/128` → 算術 `>>7`，in-bounds guard 保證 `src >= 0` 時結果相同）會改變
指令數，而純 CPU 熱迴圈（縮放 / blit，每 frame 數萬像素）的執行時間隨指令數變動，會改變
動畫 / 過場的牆鐘時長，進而破壞遊戲隱性的時序平衡。實證（商店進入腳步聲被對話音效打斷）：
`fd2_blit_scaled_chapter_pose` 的 pose 縮放除法 emit 成 `>>7`（2 指令）而非原版 `/128`
（6 指令），每像素少約 4 指令 → 過場從約 0.93 秒縮到約 0.80 秒 → 短於固定 0.9 秒的腳步聲
SFX（AIL DMA real-time、不隨 DOSBox cycles）→ 後續 SFX 的 `AIL_stop_sample` 把腳步聲切掉。
所以時序敏感熱迴圈不能為了「等價且更短」而簡化，要對齊原版 codegen（wdis 逐指令比指令數驗
證）。詳見 `../build_test/playtest_bugs.md` D 類。

驗證手段：對 pure-compute leaf function（damage 計算、softfp、decoder helper、
hash / checksum）跑 emulator 雙邊 trace（原 FD2.LE vs 重建版），對相同 input
比對 final state。

### Layer 3: byte-exact（不追求）

**本專案不追求 byte-exact。** Layer 2 (functionally-exact) 為 emit pipeline
的最高目標。即使 rebuild 用原版 Watcom 9.5a 同版編譯器，以下因素使 byte-exact
不切實際且無必要：

- emit pipeline 產出的 C source 結構與原 1998 年 FD2 source 不同（local
  variable 順序 / temp 拆分 / loop unroll 寫法），影響 register allocation
  與 instruction selection
- jump-into-middle / fall-through / shared-epilogue 等 pattern 的 C 表達方式
  改變 call/jump 結構，instruction sequence 必然不同
- function 排列順序由 linker 決定，alignment padding 由 wlink 策略決定

對於 `link_vendor_lib` pool（ail + crt lookup-resolved），byte-exact 是同版
vendor lib 直接 link 的自然結果，不需額外努力。

範圍說明（依 emit_action 分組）：

- **`link_vendor_lib`**：Layer 2 由 vendor lib 保證；byte-exact 為自然副產物
- **`skip_artifact` (93 個 binary_artifact)**：Watcom 9.5a 重 compile 自動
  產生 alignment padding；只需 Layer 1
- **`emit_fd2_source` (652 個 = fd2 640 + crt_equivalent_* 12)**：**Layer 2
  為目標**，不追求 Layer 3

### 結構性不變式（與 binary 等價無關）

emit pipeline 還必須滿足：

1. **0 個 vendor_* / FUN_* 殘留** — emit 時所有 function 都已有 best-effort
   邏輯名稱，無 vendor placeholder
2. **fall-through chain 全部 emit 為 explicit C 控制流** — 不允許依賴 C source
   檔內 function 之間的 declaration 順序（Watcom 不保證 source 順序 = link 順序）
3. **DATA TABLE FRAGMENT 不 emit 為 function** — 已全部 mark 為 data，不再以
   function entity 存在於 Ghidra；emit pipeline iterate 函式時自動跳過（見模式 E）

開放問題（HEADER-ONLY ENTRY frame_size 實作驗證等）見 `open_issues.md`。

## 引用

- 25-case fall-through 與 audit 結果由 Ghidra plate comments 編碼，emit
  pipeline 階段直接 query Ghidra 取得
- 6 種模式的識別規則與處理動作：本文件 §模式 A..F
