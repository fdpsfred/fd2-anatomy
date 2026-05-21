# 未解問題與未做分析

整理自比對所有 program_info / resource_info / assets 文件後仍存在的「未確定」、
「待驗證」與「待做」項目。每條描述：現狀 + 為什麼還沒解 + 解需要做什麼。

## 資源檔未完整解析的格式段落

### 3. TAI.DAT byte-stream payload format

- **現狀**：每 entry 已知 `+0x00..+0x03` 是 width/height (u16 LE × 2)，後續
  payload 是 opcode/payload 序列。具體 opcode 解碼未做。
- **為什麼還沒解**：BG/TAI 配對載入後實際呈現的視覺由 `fd2_play_full_combat_cinematic`
  / `fd2_execute_summon_spell_cast` 等 caller 控制，TAI 內容是輔助資料。
- **解需要做什麼**：在 in-game 呈現時 trace `fd2_play_spell_cast_sequence` 內如何
  消費 TAI buffer，比對 byte stream 與 visual output。

### 4. FDOTHER nested sub-archive 的 sub-entry 用途

- **現狀**：29 個 outer FDOTHER entries 自身為 LLLLLL sub-archive，共 166 個
  sub-entries。其中 sub-archive 0x07/0x1F/0x40/0x4D/0x4E/0x50/0x51/0x58/0x5F
  已對應到具體 caller。其餘 dynamic-recovered 與 confirmed-dead 的 nested
  sub-entries 內容未個別分析。
- **解需要做什麼**：對每個 nested sub-archive parse 出 sub-entry 後在
  binary 內字串比對 / call-site 觀察推測用途。

## Data-only / 不影響 emission

caller 端已確認不對 unknown bytes 做條件分支，純 decoder semantic 層空白；
emit C source → Watcom 編譯成 DOS executable 不受影響。等 build pipeline
站起來再做 backlog。

### 1. ANI.DAT entry header `+0xA5..+0xA6` 後的 byte 用途

- **現狀**：ANI.DAT 每 entry 含 0xAD byte header，只解出 `+0xA5..+0xA6` 是
  `frame_count`。其餘 `0..0xA4` 與 `0xA7..0xAC` 用途未明。
- **解需要做什麼**：對 9 個 entries 統計各 byte 分布，對照 in-game 觀察推測
  metadata 含義 (e.g. palette index / loop flags / frame size)。
- **Status:** data-only — `fd2_play_ani_file_animation_sequence @ 0x20421` 讀
  全 0xAD header 但只用 `+0xA5..0xA6` (frame_count)，其餘 byte 無條件分支。

### 2. ANI.DAT per-frame metadata `+0x04..+0x07`

- **現狀**：每 frame `frame_header` 8 bytes 中 `+0x00..+0x01 data_size` 與
  `+0x02..+0x03 decoded_size` 已解。`+0x04..+0x07` 4 bytes 用途待確認。
- **解需要做什麼**：對多 frame 統計這 4 bytes 模式；可能是 timing override
  或 frame-specific palette。
- **Status:** data-only — `fd2_play_ani_file_animation_sequence` 不讀 +0x04..+0x07，
  emission 不受影響。

### 5. FD2.SAV slot trailer `+0xA0A..+0xA28` (30 bytes)

- **現狀**：每個 4-slot snapshot 結尾 30 bytes 未細分 sub-field。
- **解需要做什麼**：trace `fd2_save_current_state_to_slot @ 0x30012` 寫入這段時
  的 source globals。
- **Status:** data-only — `fd2_save_current_state_to_slot @ 0x30012` 寫 0xA00
  map + 9 個 scalar (gold/chapter/speed/sfx flags)，+0xA0A..0xA28 未被操作；
  存檔讀寫走 memcpy 整段保留，emission 不受影響。

### 6. tile_attribute_flags 4-byte/tile 中 +0/+1/+3 byte 用途

- **現狀**：FDSHAP `shap_id × 2 + 1` 提供 4-byte/tile attribute；只有 `+2 byte`
  的 animation/palette flag bits 已解 (`0x04` / `0x08` / `0x10`)。其餘 byte 未明。
- **解需要做什麼**：對多章 tile_attribute_flags dump 後 cross-tile 比對；
  可能含 terrain_id / movement cost / passability。
- **Status:** data-only — `fd2_composite_battle_tile_map @ 0x11eee` 與
  `fd2_read_tile_attribute_at_pos @ 0x12e38` 只讀 +0 byte 的 bit 0x04/0x08/0x10
  (animation/palette flag)；+0/+1/+3 其他 byte 不做條件分支。

### 7. FIGANI per-pose metadata 細節

- **現狀**：per pose `+4 type` (1 = spell-cast)、`+5 sfx_hook_id`、`+6 sub_frame_count`
  已解；`+7..` (sub-frame data 之前) 用途與 sub-frame 之間的 inter-frame timing
  未細究。
- **解需要做什麼**：對 multiple poses 統計 byte 分布，trace
  `fd2_step_figani_pose_animation @ 0x2B9A1` 詳細 state machine。
- **Status:** data-only — `fd2_step_figani_pose_animation @ 0x2B9A1` 只讀 per-pose
  +6 (sub_frame_count)，+7.. 未被讀取。

## 章節中未確認的機制

### 8. 各章寶物清單與 enemy 配置的詳細內容

- **現狀**：`assets/chapters/chapter_NN.md` 中部分章節 (ch16..ch29) 寫
  「待解：寶物清單需從 FDFIELD.DAT tile_event 解析。」與類似標記。
- **為什麼還沒解**：寶箱與 enemy 配置寫在 FDFIELD.DAT 的 char_spawn_records
  與 tile_pickup_table，未對 30 章全部展開。
- **解需要做什麼**：跑 `tools/decoders/fdfield_event_decoder.py` 與
  `fdfield_char_spawn_decoder.py` 對 30 章 dump 全 entries，把結果填回
  各 chapter assets。

## Data audit phase D8 deferred / pending verify

### D8-1. 0x49a06 `data_crt_emu387_internal_constant_database_174b` per-sub-table semantic 未逐一解碼

- **現狀**：174B multi-region const/state table 在 __int7 (Watcom 387 emulator) 內被 14 個 DATA read site 引用，分布在 9 個 sub-offset：+0/+2/+4 (前 0x50 block) / +0x50/+0x52/+0x56/+0x58 (中 0x50 block) / +0xA0/+0xA4 (尾 ~46B block)。已命名 + plate 描述整體結構，**但每個 sub-region 的具體 semantic 未拆解**。Content patterns 觀察到算術級數（10s / 2048s）+ opcode-byte 樣式。
- **解需要做什麼**：對 14 個 __int7 read site 逐一 disasm，搭配 x87 spec 推斷每個 sub-table 的角色（control word / status word / exception mask / precision mode / opcode dispatch sub-table 等）。屬「emu387 sub-system 深度 audit」獨立工作項，data audit 範圍外。

### D8-2. 0x49ab4 `data_crt_emu387_x87_opcode_dispatch_table_176ptrs` per-slot opcode 未解碼

- **現狀**：176-slot pointer table (704B) 為 __int7 x87 opcode dispatch；主要 access site `CALL [EBX*4 + 0x49ab4]` @ 0x49dfe 用 mask `0x1807` + `BH=0` 取低 3 bits = 8 slots。其他 reach 點未識別。256-slot full opcode space 中 80 個 NULL (= illegal opcodes)。**Per-slot opcode → handler mapping 未逐一解碼**。
- **解需要做什麼**：列出 __int7 內所有 indirect-CALL/JMP via 0x49ab4 base 的 access sites，反推每個 mask + offset 組合對應的 x87 opcode 範圍；建立 256-entry full opcode → handler 對照表（x87 instruction encoding 已標準化，能對應 FADD/FMUL/...）。同樣屬「emu387 sub-system 深度 audit」獨立工作項。

## Data audit phase D7 deferred / pending verify

### D7-2. `0x527B0..0x527B7` Watcom near-heap descriptor 前 8 bytes 語意

- **現狀**：Watcom near-heap descriptor struct @ 0x527B0 經 __MemAllocator 訪問 `[EBX+0x8..0x24]` 已對應到 head_search_ptr / max_free_hint / max_free_cap / grow_counter / node_count / sentinel.size / sentinel.prev / sentinel.next。但 +0x0..+0x7（前 8 bytes）無直接 asm 訪問。
- **推測**：Watcom 標準 near-heap layout 通常含 heap_top (+0) + heap_limit (+4) 兩個 dword，但 FD2.LE 內未見直接讀寫。Plate 標 `inferred ... pending verify`.
- **解需要做什麼**：(1) 在 v2 RTL source 查 `__nheap` descriptor struct 完整定義；(2) 對其他類似 binary（同 Watcom 11 版本）比對 0x527B0 起始 byte 在啟動後的填入值 — 如執行期 dump 是 `heap_top` 則確認。Static-only 分析無法確定，需要 emulator-level 驗證。

## 程式行為未完全理解的段落

### 13. runtime_char `+0x4E wStat4_current` 的真實語意

- **現狀**：已知 `wStat4_current = DX_total + sum item.short@+7`，由
  `fd2_recalculate_combat_stats @ 0x1b750` 寫入。但這個 stat 的遊戲意義 (魔抗 / 命中 / 迴避 /
  其他) 未 emulator 驗證。
- **解需要做什麼**：emulator 觀察戰鬥中此值如何影響擊中率 / 傷害計算。

## 未做的批次分析

### 17. FDOTHER 21 個 confirmed_dead idx 的 binary content

- **現狀**：21 個 outer entries 經 binary no-ref proof 確認 dead。內容在 binary
  仍存在但無 caller。
- **解需要做什麼**：對 21 個 entries 解 RLE / structure 看是否有 cut content
  線索。

### 18. ANI.DAT 各 entry 對應的具體 cinematic 場景

- **現狀**：9 個 entries 已知 idx 1 是 intro animation；其餘 8 個用途未對應
  in-game 場景。
- **解需要做什麼**：in-game 觀察各 cinematic 觸發時 caller 傳入哪個 idx，
  反查對應內容。

## 與攻略本對照差異

### 21. ch23 羅德曼「15 回合內」與 binary `< 15` 的對齊

- **現狀**：binary gate `save_metadata_block < 15` 表示 turn 1..14 羅德曼可加入。
  攻略本「15 回合內」可能對應 turn 1..15。可能是攻略本筆誤 (應為「14 回合內」)，
  也可能 binary off-by-one。
- **解需要做什麼**：emulator 實測 turn 14 (binary <15 PASS / 攻略≤15 PASS) 與
  turn 15 (binary <15 FAIL / 攻略≤15 PASS) 的羅德曼加入結果。

## 重建相關 backlog

### 29. 手動 patch 3 個無法 import 的 .obj

- **現狀**：770 個 dedup 後的 Watcom CRT .obj 中 3 個觸發 Ghidra OmfLoader
  的 EOF bug 而 import 失敗：`fpeinth.obj` (FPE handler)、`font8x8.obj` ×2
  (VGA ROM 字型 bitmap)。FD2 都不連結這 3 個，所以對版本判定與 CRT
  識別結果無影響，但理論完整度上仍是缺口。
- **解需要做什麼**：trace Ghidra OmfLoader 為何在處理完 MODEND 後仍試圖
  多讀 1 byte，patch loader 或重組 .obj 結構。

### 31. crt_lookup_9.5a.json `byte_match` 旗標的 false-positive 風險（PUSH/PUSH/JMP-to-exit-with-msg 結構類）

- **現狀**：lookup entry 0x46b41 原 `name` = `_Not_Enough_Memory` / `verified` =
  `byte_match` / `source_obj` = `9d1f5c0476e0_nomem.obj`，但 function body 的
  `PUSH 0x1; PUSH 0x516cc; JMP __exit_with_msg` 的 immediate `0x516cc` 指向
  字串 `"Floating-point support not loaded
"`，與 `nomem.obj` 的 "Not enough
  memory
" 完全不同 — 該 entry 實為 Watcom `__FpAbort`（FP-not-loaded
  abort thunk）。byte_match 之所以命中是因為 lookup pipeline 對 immediate 做
  masking 後比對，三條 instruction 的 opcode 序列 `6A 01 ; 68 ?? ?? ?? ?? ; E9 ?? ?? ?? ??`
  在 nomem.obj 與 fpabort.obj 中完全相同。已更新 0x46b41 entry
  `current_name` = `__FpAbort` / `verified` = `byte_match_disputed`，frozen
  `name` 欄保留；Ghidra 函式同步 rename。
- **為什麼還沒解**：本次 audit 只處理當前 fn (0x46b41)，未對全 188 個
  `verified: byte_match` entry 做 system-wide re-verification。
- **解需要做什麼**：寫 audit script 把所有 verified=byte_match entry 與
  body 中 hardcoded immediate（字串 ptr / data ptr / call target）抽出，
  比對對應 lib `.obj` 的相同 offset 上 FIXUPP 後的真實 target，找出其他可能
  的 false positive；對每筆 disputed entry 走完整 re-identification 流程。
  可作為 lookup 維護 regression script 重建（原 crt_audit pipeline 已移除），
  避免未來新增 byte_match entry 時再現此問題。

## 已解問題（記錄為基線）

- ✅ #26 auto-classifier 加進去的 61 個 `uint` param 型別 — 由廣域 function re-review 覆蓋解：56 個 chapter_NN_init/end 的 spurious passthrough param 全部移除（per「Function-pointer dispatch table callees 的 0-arg signature」項，confirm 為 `void __cdecl func(void)`，0 args by dispatch site analysis）；11 個 misc function `FUN_*` 全部更名為語意名 + 正確型別（如 `FUN_000361a5` → `AIL_internal_decommit_and_free(void *, uint)`、`fd2_noop_stub_*` 系列 → `void(void)`）。最終 FD2.LE 內 `FUN_*` 計數 = 0，所有 signature 由「全 function re-review 完成」項逐一讀 asm/decomp 校正
- ✅ #28 FD2 連結時的 wlink linker 設定 — 從 LE header / object table / page map / 入口流程 + Watcom 9.5a CRT 識別結論反推完整 wlink directive：`system dos4g` + `name FD2.EXE` + `option stack=4K` + main `file f2.obj` (決定模組名 "f2")。LE binary layout 3 個 object（`_TEXT` @ 0x10000 / `DGROUP` @ 0x50000 含 22 KB CONST+DATA+BSS+4KB STACK / `FAR_DATA` @ 0x60000 含 13.5 KB FD2 大型 data tables）、入口 chain (`crt_equivalent_entry_start @ 0x3C964` → `crt_equivalent_dos_main_bootstrap @ 0x3C9DE` cstart → `__InitRtns + __CMain` → `fd2_main`)、Watcom 9.5a 多 extender 偵測 (DOS/4G "DX" / DOS/4GW "CB" / Phar Lap)、FD2.EXE 10424-byte Watcom DOS bind stub (找 `dos4gw.exe`/`dos4g.exe` exec FD2.EXE)、stack 與 cmdline buffer 共用 4 KB region 機制、Object 3 推測由 `#pragma data_seg("FAR_DATA")` source-level 顯式分組（非 `-zdt=N` threshold）。詳見 `rebuild_info/link/le_layout.md` + `rebuild_info/link/wlink_settings.md`
- ✅ #20 ch20 達可塞「15 回合內」與 binary `< 16` — binary turn 1..15 PASS、攻略「15 回合內」對應 turn 1..15，精準對齊無差異
- ✅ #22 ch13 攻略「哈瓦諾」vs binary char_id 3 — binary char_id 3 = 哈瓦特 (per `assets/text/global_text.md` page 4)，**結論：攻略筆誤**，正確角色名應為哈瓦特
- ✅ #9 ch1 哈瓦特 / 哈諾 char_spawn_record 列舉 — 30 records 中唯二的 `team=2 player_class` 是 record[8] (char_id 0x03 哈瓦特) 與 record[9] (char_id 0x01 哈諾)；三 byte AI override `+0x11/+0x12/+0x13` 均為 (0,0,0)，protective AI 行為實際來源是 record[8] `+0x02 ai_target_id=0x01` 指向哈諾。詳見 `assets/chapters/chapter_01.md`「哈瓦特暴走」段
- ✅ #23 tools/decoders/ round-trip 驗證 — 修正全 12 個 decoder 的 `REPO_ROOT = parents[4]` (舊路徑常數) → `parents[2]` + DAT 檔位置補上 `fd2_game_files/` prefix；smoke-test 全 12 個 decoder 跑 `--list` / `--info` / `--self-test` 通過
- ✅ D7-3 ANI.DAT decoder dst_buf unaligned dword (`0x52762`) — 規則寫進 `rebuild_info/emission/pipeline_spec.md` §規則 E-9；全 DGROUP 已知唯一一筆 unaligned 4-byte global，emit 預設選 byte stream + bit-cast (`memcpy` access)
- ✅ D7-4 AIL timer 16 vs 15 slot drain asymmetry — by design 非 bug：`AIL_internal_register_timer_inner @ 0x3eee6` 的 slot 分配 loop bound `< 0x3c` 表 slot 15 永不被 register_timer 分配，accumulate loop 跑到 slot 15 時不會更新 `pending_trigger_count[15]`（= nested counter @ 0x52B90）
- ✅ D8-6 audit-spawned data items 全部回填 worklist — 3 個 split sub-item (`data_align_41558_lea_nop_pad_8b`、`data_fd2_battle_weapon_attack_anim_pattern_ptr_table_21 @ 0x61955`、`data_fd2_battle_weapon_attack_anim_pattern_script_pool_84b @ 0x619A9`) 已在 worklist + Ghidra 對齊
- ✅ D8-7 CRT ctor table 邊界與內容 — `data_crt_xi_init_table @ 0x539A0..0x539F1` (82 bytes) = 16B header (`__xi_begin/__xi_end` sentinels + matherr default thunk ptr + control flag dword 0x11) + 10 ctor entries (6B/entry: 2B priority + 4B fn ptr) + 6B NULL terminator；10 個 entries 與 callee 對照（含 `__sys_init/fini_387_emulator` jmp_thunk 對應）完整寫進 Ghidra plate；emit_action: link_vendor_lib（各 entry 由相應 .obj 透過 wlink XI 段 merge）
- ✅ 全 function re-review 完成 — FD2.LE 1342 個 function 端到端親自讀過
  asm + decomp 後做 name / plate / cc 校正：
  - **rename 9 個** — Group 2 LX module loader / GETIP / uint64 ASCII converter
    系列 6 個（`crt_equivalent_lx_chunk_read_36107` / `_lx_header_reader_36344` /
    `_lx_module_loader_3647b` / `_uint64_to_decimal_ascii_4d9e1` / `_getip_4da53` /
    `_getip_body_4db08`，原名 close_helper / softfp 系列名稱與 body 不符），
    Group 4 `AIL_internal_helper_0003e73e` → `AIL_internal_timer_isr_master`，
    Group 5 `AIL_internal_mix_loop_8bit_stereo` → `_6f`（與其他 71 個 mixer
    callback 統一 idx 命名）
  - **cc 校正 3 個** — Group 2 LX loader 系列 0x36107 / 0x36344 / 0x3647b
    從 `__watcall` 改 `__cdecl`（disasm: RET no N + 純 stack args + caller cleanup）。
    最終 cc 分布 `__cdecl` 1047 / `__watcall` 294 / `__stdcall` 1
  - **plate 整理 ~770 個** — 移除全 1342 個 function 內所有 phase / date / task ID /
    audit-log 流水帳（per [[feedback_kb_writing_rules]]）：(a) 79 binary_artifact
    unified plate（Group 1）；(b) 15 crt_equivalent rewrite 含修正 LX loader 描述（Group 2）；
    (c) 4 AIL public stale helper ref 同步（Group 3）；(d) 30 AIL_internal phase-ref /
    audit-block 改成 unified sub-case template（Group 4）；(e) 132 mixer callback 已詳細
    描述（Group 5）；(f) 111 CRT lookup/PUBLIC_CRT_SYMBOLS 補上 vendor symbol plate（Group 6）；
    (g) 621 fd2 audit-log 尾巴 strip + 25 Phase 6 plate refresh annotation 移除（Group 7）；
    (h) 33 cross-pool AIL/CRT promotion/reclassification 流水帳改為精煉版
  - **KB sync** — `rebuild_info/crt/symbol_inventory.md` LX loader / softfp 命名同步、
    `rebuild_info/ail/inventory.md` static-link "thunk + body pair" 修正 + mixer 命名
    規約更新、`rebuild_info/emission/calling_convention.md` cc 分布同步、
    `rebuild_info/crt/fid_match.md` crt_equivalent_* 列表更新、
    `rebuild_info/crt/lookup_9.5a.json` 不變（Group 2 rename 只影響 plate 不影響 lookup name）、
    `open_issues.md` (本檔) 同步
  - **Ghidra state fix** — 0x4cbce 1B `crt_equivalent_linker_padding_4cbce` body
    force-disassembled 為 `RET` instruction（之前 listing.getInstructions 返回空集合）
  - **驗證** — Bad Instruction = 0、find_code_gaps min_size=1 = 0、
    categorise() 0 raise、call_graph 1342 nodes / 4254 edges 全程不變、
    audit-marker 全 pool 殘留 = 0
  - workspace artifact 保留於 `workspace/function_rereview/group_01..07/` 作 audit trail

- ✅ 四 pool + 兩維度分類架構落地 — FD2.LE 全 1342 個 function 改用單一命名規則
  分四 pool：`AIL_*` (422) + `crt_equivalent_*` / `crt_*` / `L$*` / lookup / PUBLIC_CRT_SYMBOLS
  (209) + `fd2_*` (632) + `binary_artifact_*` (79)，UNCLASSIFIED = 0。
  `tools/program_analysis/build_call_graph.py` 的 `categorise()` / `emit_action_for()`
  從 name 機械推出 `category` × `emit_action`；輸出 `workspace/call_graph/call_graph.json`
  每個 node 含 `category` / `emit_action` 兩欄。
  改動範圍：(a) `IF@DABS @ 0x4d45e` byte_match 入 lookup；(b) 79 個 `align_nop_*`
  → `binary_artifact_align_nop_*`（一次性 batch rename）；(c) 630 個
  fd2 candidate 1-by-1 manual audit（10 batch × 63 ≈ 8-12 hr）
  得出 621 confirmed_fd2 + 9 reclassified_to_crt（其中 `__CHP @ 0x377a4` /
  `__GETDS @ 0x3cbc4` / `fabs @ 0x4d450` byte_match 命中 Watcom lib obj，加進
  lookup 走 link_vendor_lib；另 6 個維持 `crt_equivalent_*` 命名走 emit_fd2_source）；
  (d) `build_call_graph.py` 改 categorise() 四分類 + 新增 emit_action_for() +
  DOT CATEGORY_FILL 加 binary_artifact 淺粉 + fd2 維持淺灰；call_graph
  重 build 後 0 UNCLASSIFIED + 0 Bad Instruction。256 個 `crt_dpmi_int_<NN>` /
  256 stub 並非獨立 function（屬 `_DoINTR_ @ 0x4657B` body 內部位元組）的舊認知
  已澄清。crt_lookup_9.5a.json `in_lookup: 186→190 (+IF@DABS / __CHP / __GETDS / fabs)`，
  `byte_match: 45→49`
- ✅ 哈瓦特暴走機制 (ch1) — char_spawn_record +0x94/0x95/0x96 → protective AI fall-through
- ✅ ch9 reinforcement waves — handler_1F state machine + race_id increment
- ✅ ch20 Stage C 沼澤怪物排除 win override — 純 static 解
- ✅ ch15/17 conditional dialog — `check_party_has_char_id(0x0C 凱麗 / 0x12 蜜蒂)`
- ✅ FDFIELD entry layout — `0x83 byte header + N × 0x1A char_spawn_records`
- ✅ chapter event 不是 bytecode — 直接函數 dispatch via `data_fd2_battle_ai_post_action_consequence_table @ 0x51B91`
- ✅ char_id namespace — `< 0x44` player class、`≥ 0x44` enemy class (`enemy_id = char_id - 0x44`)
- ✅ 11 個 LLLLLL DAT 統一格式 (FDTXT/FDOTHER/FDFIELD/FDSHAP/DATO/FDMUS/FIGANI/BG/TAI/TITLE/ANI)
- ✅ FDSHAP idx 公式 — `shap_id = tile_event_data_table[0]`，`shap_id × 2` 取 snapshot/+1 取 attribute
- ✅ chinese_glyph_table — 1824 glyphs full ET3 STDFONT 比對 + 人工校正完成
- ✅ runtime_char struct layout (80 bytes) — 主要 fields 全部 confirmed
- ✅ Enemy AI 12 種 behavior class semantic
- ✅ AI kill-shot 加權 (物理 0x12 / item 0x12 / spell 0x18)
- ✅ 20% HP_max idle heal mechanism
- ✅ Two-pass enemy phase (smart caster 先動)
- ✅ FD2.SAV 主要 layout (header / map / runtime_char / 4 slots / checksum)
- ✅ runtime_char +0x09 — `pChar_identity_combat_byte[1]` (split 為
  `bChar_id` + `bReserved_padding_09`)；reserved padding，僅兩個 init
  函式寫 0，AI / combat / save / death / XP / item / cutscene 等 paths
  皆無讀取
- ✅ runtime_char +0x27 pCombat_aux_block[1..9] — reserved padding (9 bytes)；
  loader / runtime 無讀寫，save/load 走 memcpy 整段保留
- ✅ FUN_0004b502 (0x4b502) — Watcom soft-FP 80-bit long double in-place add
  of immediate constant；helper struct `long_double_80` (10 bytes:
  `dwMantissa_lo / dwMantissa_hi / wSign_exp`)
- ✅ Decompiler fragments (6 個 epilogue clusters + 1 tail JMP thunk) — caller
  透過 TAIL JMP 進入，Ghidra UNCONDITIONAL_CALL 是 display quirk；plate comment
  標 `DECOMPILER FRAGMENT — DO NOT DECLARE INDEPENDENTLY`，emit pipeline 跳過
- ✅ ch9 `tile_event_consumed_flags[0x10]` 起始值 = 0 — 由
  `fd2_init_battle_state_for_chapter @ 0x205DA` 的 `crt_memset(flags, 0, 0x20)` 清 0；
  handler_1F 從 race_id=0 遞增
- ✅ pickup_kind=2 path — `process_battle_drop_entries` type 2 case 直接
  dispatch via `data_fd2_battle_ai_post_action_consequence_table[ushort_value]()`，不寫
  `data_fd2_battle_ai_post_action_consequence_idx` global；tile-step trigger (Path 1) 與
  death-drop (Path 2) 不共用 state variable
- ✅ endgame_ch32 char_spawn_count = 30 vs 40 records — `char_spawn_count`
  (header byte +2) 絕對控制 loader 讀取範圍；`load_chapter_portraits_and_dump_tmp
  @ 0x10b4e` 的 loop 只跑 30 iterations，10 個額外 records 是 dead payload
- ✅ ch23 mid-handler reload — 用 `load_dat_resource` 手動 reload (FDFIELD
  idx 0x45 + FDSHAP 0x2E/0x2F)，不是 `load_chapter_battle_data`；trigger
  hardcoded 在 dialog page 0x10 + 第 3 次 rising effect + 64-step palette
  fade-out 之後 unconditional 執行；無 byte/flag 條件
- ✅ 28 個 unref chapter event handlers — 全部 decompile 並 categorize：
  8 sentinel + 4 state_machine_mutator + 4 dialog_with_state + 3 drop_dialog +
  2 major_endgame_cinematic + 2 dialog_only + 1 each of first_time_gated /
  char_conditional / turn_conditional / item_pickup / ai_setup；cut content
  集中在 endgame (idx ≥ 0x4D) sentinel slots
- ✅ Function-pointer dispatch table callees 的 0-arg signature — 176 個
  function (chapter_NN_post_action × 17 + chapter_NN_init × 26 + chapter_NN_end
  × 30 + chapter_event_handler_* × 89 + cast_* × 13 + 1) 確認 caller_count=0
  且 dispatch site `(*table[idx])()` 無 args，全部 `void __cdecl func(void)`
- ✅ Watcom cc 重校正 — 全 121 個 `__fastcall` 標籤重新分配為
  `__cdecl` 73 + `__watcall` 26 + fragment/dispatch `__cdecl pc=0` 22；最終
  cc 分布 `__cdecl` 1047 / `__watcall` 294 / `__stdcall` 1；soft-FP family custom
  ABI 標 `__watcall` 加 plate 註明 register layout，build pipeline 階段再
  byte-level 比對
- ✅ AIL 內部 helper 命名 — 46 個 entry-point 之外的 helper 全部命名：~93 個
  人工命名 `*_inner` worker / ISR / log / timer / DIG mixer / MDI sequence /
  XMIDI parser，54 對 vendor-internal log-wrapped public API（`AIL_xxx` +
  `AIL_xxx_inner` 配對，從 fprintf format string 自動抽取名稱，47 對有獨立
  inner function），38 個 `AIL_helper_<addr>` / `AIL_<descriptor>_<addr>`
  best-effort placeholder（plate comment 紀錄 callees）。3 對 helper 因
  AIL3DIG / AIL3MDI 兩 .obj 各帶一份而出現 same-name duplicate
  （`AIL_log_lock_acquire / release / get_isr_lock_count`），Ghidra unique key
  是 name+address 故保留兩份原名
- ✅ AIL function body 內 fall-through dead-code stub — 剩 2 個 dead stub
  (`AIL_resume_sample @ 0x39522` / `AIL_set_sequence_tempo @ 0x3AD52`，
  caller_count=0)，emit pipeline 連結 Watcom AIL 後對 binary 影響為 0
- ✅ Function-boundary fall-through audit — 全 function 跑「prev_fn 最後 inst
  有 fall-through 進 this_fn entry」audit，扣除 7 個已結構性修復的案例後共
  98 個 candidate，個別驗證後分為 6 種 benign 模式：73 個
  `binary_artifact_align_nop_*` (zero ref 確認，entry/body 任何 byte 都無
  CALL/JUMP/DATA/INDIRECTION 等 reference) + 8 個 SHARED EPILOGUE STUB
  (`fd2_noop_stub_b43` / `fd2_noop_stub_c49` / `fd2_noop_stub_1011` /
  `fd2_noop_stub_1452` / `fd2_noop_stub_13994` /
  `fd2_set_battle_anim_phase_to_1` / `AIL_log_decrement_nesting` / `__GETDS`
  （後者 byte_match 命中 Watcom CLIB3S `cstart.obj`，已歸 crt pool 但仍是
  SHARED EPILOGUE 形態的 fall-through 對象），共用 epilogue 由多個 source
  function 經 fall-through 或 tail-JMP 進入) + 3 個 SHARED BODY 多 entry
  (`fd2_play_palette_fade_to_black` / `fd2_check_battle_end_condition` /
  `crt_softfp_uint32_to_ld`，與其同伴 entry 共用邏輯主體) + 4 個
  HEADER-ONLY ENTRY (`fd2_chapter_event_handler_*` 系列，prev 只 PUSH args /
  frame_size，fall-through 進真正執行的 this) + 5 個 DEAD FALL-THROUGH
  (prev 末尾的 fall-through 在執行流上死掉，例如 `exit` / `crt_terminate`) +
  4 個 DATA TABLE FRAGMENT (jump table 區段被 Phase F 誤 disassemble 為 code) +
  1 個 STATE-MACHINE INIT-ENTRY (AIL_internal 系列 init → loop pair @ 0x41834
  / 0x4183d)。emulator-level 驗證 (`fd2_noop_stub_b43`) 確認 ESP 平衡邏輯正確。
  emit pipeline 對每個模式的處理規則寫在
  `rebuild_info/emission/pipeline_spec.md` §模式 A..F；6 種模式詳細 disasm 與
  分類證據編碼在 Ghidra plate comments 內，可從 Ghidra 直接 query。

- ✅ Ghidra jump table 漏抓 audit (issue #30) — 全 binary 對 indirect JMP /
  orphan code / 異常小 body / fragmented body 做 audit。**結論**：63 個
  indirect JMP 全 target 在 body 內 (AIL 4 + CRT 59 + GAME 0)；遊戲端 0 個
  indirect JMP（switch 用 if/else 鏈分派，不走 jump table）；CRT lookup 140
  entry vs lib `.obj` size 全面 diff 在 3 個 mismatch 修完後 0 不一致。修復
  項目：(a) `AIL_internal_mix_loop_6f @ 0x49306` body 1→58 byte
  (bytes 已存在但未 disassembled)、(b) `AIL_set_sequence_volume @ 0x3add4`
  130 byte function 全部 bytes-cleared，re-disassemble 還原、(c) 兩個遺漏
  setter `crt_set_word_global_52758 @ 0x3615e` / `crt_set_word_global_5275c
  @ 0x3616e` (各 16 byte，get-and-set helper) create_function 補齊、(d)
  `chapter_01_init` body 2-range→1-range (entry 5-byte instruction 跨越 hole)、
  (e) `crt_abort_thunk` body 2-range→1-range (同類 hole)、(f) `__MemAllocator`
  body 171→176 (5-byte 尾段 alternate-exit thunk)、(g) `__MemFree` body 272
  →267 (前述 5 byte 從 MemFree 改歸 MemAllocator)、(h) `__STOSB` body 49
  →55 (含 6-byte 尾段 alignment NOPs)。Ghidra category=Bad Instruction 0、
  find_code_gaps min_size=1 為 0。新工具 `tools/program_analysis/jump_table_audit/`
  compare_lookup_sizes.py 可重複跑做 regression。詳見
  `tools/program_analysis/jump_table_audit/_index.md`
- ✅ 全 binary function 完整化 + 命名審視 — 1699 個 function 100% 命名
  （FUN_* 0 個、vendor_* 0 個、Bad Instruction bookmark 0 個、
  find_code_gaps min_size=1 為 0、`.object1` uncovered instruction byte
  全部歸屬於某 function；兩個例外是 0x4A8E8 與 0x3CBD1 的 5-byte JMP thunk
  （label `crt_dpmi_signal_handler_jmp_thunk` 與 `crt_phar_lap_exit_jmp_thunk`），
  Ghidra createFunction 拒絕單 JMP-into-existing-function，保留 disassembled
  並標 label，emit pipeline 階段交給 vendor relink；後者由 §12 callee 比對
  時刪除原本錯誤的 1-byte function 後留下，target 0x45fb6 是 Phar Lap
  DOS-extender exit dispatcher (非 Watcom CRT)）。category 分布：ail 278 /
  crt 783（含 178 個 `crt_*` 前綴 +
  47 個 Watcom 公開符號 + 256 個 `crt_dpmi_int_NN` DPMI 軟中斷 stub +
  74 個 `align_nop_*` Watcom alignment fill + ~226 個 `crt_helper_*` /
  `crt_<descriptor>_<addr>` vendor placeholder）/ game 638；call graph 由
  `tools/program_analysis/build_call_graph.py` 即時重生到
  `workspace/call_graph/`。
  `AIL_end_sample @ 0x3958f` 涵蓋 PUSH EDI prologue（body 0x3958f-0x395FB，
  3 caller，對應 `AIL_end_sample_inner @ 0x41460` 1 caller）；
  `chapter_01_init` / `chapter_08_end` / `chapter_29_end` /
  `crt_capture_ss_for_stkchk` / `crt_sin_inner` / `crt_abort_thunk` /
  `save_runtime_char_to_template` 等 function body 涵蓋全部 reachable 指令
- ✅ `crt_*` function classification audit — 205 個非 lookup 的 `crt_*` candidate
  經 byte-exact lib 比對（全 LIB386 1548-obj pool，比初版 770 obj 多收進 MATH /
  PLIB / PLBX / CPLX / NOEMU387 系列）+ 雙向 xref + cluster ancestor propagation
  + decompile 行為三層證據逐一判定 + splitter false positive 修復 + dispatch-table
  反推。`rebuild_info/crt/lookup_9.5a.json` 從 141 → 174 entries（+33 verified=byte_match
  含 4 splitter 修復 + `_set_matherr` + `strcmp` (off-by-one entry 修正)）。每筆 entry 加 `source_libs` 欄位記錄
  Watcom 9.5/9.5a/9.5b/9.5c 內各 lib appearance；對照表寫在
  `rebuild_info/crt/matched_function_sources.md`。重要 reclassification：(a)
  `0x45e36..0x4608a` 是 `MATH387S/dosinite.obj` 的 `__sys_init/fini_387_emulator`
  （DOS/4G 8087 emulator init/fini，size 384/213 確認 lib version 為 9.5 或 9.5a）；
  (b) `0x4cd73..0x4cf0a` 是 `MATH387S/ftos.obj` 的 `_SetMaxPrec`；splitter false
  positive 全部用 setBody/createFunction 修復；(c) `0x47638..0x495FF` 整段
  AIL DIG mixer engine：兩個 128-entry × 4-byte 的 dispatch table（`ail_dig_mix_finalize_table`
  + `ail_dig_mix_sample_table`）+ 132 個 mix callbacks + dispatch 函式 +
  `AIL_internal_register_mix_globals` 註冊。callbacks 統一 rename 為 `AIL_internal_mix_finalize_<idx>`
  / `AIL_internal_mix_loop_<idx>`，移到 ail 分類；call_graph ail/crt/game = 422/288/633。
  最終所有 `crt_orphan_*` placeholder 清空，剩餘 28 個 `crt_*` 全部 byte-level 或
  decompile 確認的 Watcom CRT primitive。前期 KB §12.3 「0x45fb6 是 Phar Lap LE
  runtime」結論已撤回（FD2.LE strings 是 RATIONAL DOS/4G，無 Phar Lap）。pipeline
  與 reusable scripts 已執行完畢（pipeline 已移除，結果落地在 Ghidra plate
  comment 與 `lookup_9.5a.json`）
- ✅ byte_match pipeline sliding-window + jump-into-middle 全 binary 100% audit —
  對全 binary size≥8 function 對 entry offset 0..16 跑 FIXUPP-aware byte_match
  against 1548-obj × 7040 lib func；filter 為 strong (padding/mode-setter first
  byte + PUBDEF nonzero-O 命中) / weak (anonymous-static only)。對全 `.object1`
  內所有直接 JMP/CALL 的 target 做 cross-function mid-entry 檢查
  (538 cross-fn branches / 157 unique mid-entry targets / 115 unique tgt functions)。

  **結構性修復**（CRT 範圍，sliding strong + byte_match 機會）：
  (a) SQRT split (IF@SQRT 2B + __@DSQRT 62B)
  (b) COS/SIN/FPU 5-fragment 合併為 IF@SIN 182B + IF@COS 25B + 5 個內部 alt-entry label
  (c) seterrno trio split (__set_ERANGE 5B + __set_errno 12B)
  (d) log triplet rename (crt_softfp_log_validate → IF@LOG 76B + thunk_mode9 → IF@LOG2
      + 補建 IF@LOG10 function 4B 從 orphan disasm)
  (e) DPMI lock/unlock SHARED BODY plate

  **alt-entry label 完整化**（全 157 個 unique mid-entry target，無 caller_count threshold）：
  生成 fix plan 後批次套用：143 個新增 `L_<tgtfn>_alt_<offset>` label + 14 個既有
  label 保留；115 個 unique tgt function plate comment 加 `=== jmp_target_audit
  alt-entries ===` 區段（per-offset caller summary）。涵蓋 CRT 10 / AIL 21 / game 126。
  emit pipeline 對 alt-entry label 對應為 `goto label;` 或抽 helper
  （`rebuild_info/emission/pipeline_spec.md` §模式 A / B）。

  Lookup 174 → 186 (+12 byte_match: IF@SQRT / __@DSQRT / IF@COS / IF@SIN / IF@TAN /
  IF@LOG / IF@LOG2 / IF@LOG10 / __set_ERANGE / __set_errno / __FPE_exception_ /
  flushall)，byte_match 33 → 45。Bad Instruction = 0；find_code_gaps min_size=1 = 0；
  sliding actionable_strong = 0。執行此 audit 用的 crt_audit pipeline 已移除。

  **剩餘 19 個 `crt_*` 重新分類重命名**（依「行為對齊 Watcom CRT vs FD2 自寫」維度）：
  - **9 個 `crt_equivalent_*`** — Watcom CRT 行為等價但 byte 不 match 任一 lib obj
    (`entry_start` / `dos_main_bootstrap` / `get_eflags` / `get_eflags_thunk` /
    `getip_4da53` / `uint64_to_decimal_ascii_4d9e1` / `lx_header_reader_36344` /
    `lx_module_loader_3647b` / `lx_chunk_read_36107`)。emit pipeline 必須 emit 為
    FD2 C source。call_graph category 仍為 `crt`。
  - **10 個 `fd2_*`** — FD2 工程師自寫的 CRT-style primitive：6 DPMI primitive
    (為 Miles AIL callback 提供) + 1 file helper (`fd2_filesize_path`) + 3 個 global
    accessor (`fd2_get_word_global_52754` / `fd2_set_word_global_52758` /
    `fd2_set_word_global_5275c`)。call_graph category 改歸 `game` pool。
  call_graph 計數從 (crt 287 / game 633) → (crt 277 / game 643)。對應
  `rebuild_info/crt/symbol_inventory.md` 「CRT-equivalent / FD2-specific wrapper」段落。

- ✅ AIL function classification — 422 個 AIL ecosystem function 完成分類：
  103 個 `AIL_*` 公開 API + 187 個 `AIL_internal_*` (含 144 BFS-reached + 4
  promoted + 39 BFS-unreached) + 132 個 DIG mixer dispatch table callbacks
  (`AIL_internal_mix_finalize_<idx>` 60 + `AIL_internal_mix_loop_<idx>` 72)。
  原 9 個 `crt_*` 經三向誤分類 audit（xref-source / orphan-disasm / reverse
  caller-set）reclassify 為 AIL：4 個 BFS-reached 的 caller-全 AIL + 引用 AIL
  global（`alloc_and_commit` / `decommit_and_free` / `load_file_to_memory` /
  `parse_int_with_base`），另 5 個 BFS-unreached 的 vtable-indirect / orphan
  placeholder（3 個 dpmi_unlock helper unlock 特定 AIL region 0x3f190.. /
  0x41dc0.. / 0x45320..；2 個 wave-synth instrument lookup helper —
  `helper_454fd` byte-level 確認 770 個 CLIB3S obj + CLIB3R obj 全部 0 match）。
  另 3 個 zero-xref orphan reclassify 為 AIL_internal_* (USE16 ISR IRETD 尾巴 +
  USE32↔USE16 stack switch pair)。10 個 `crt_*` 確認為 Watcom CRT primitive 保留：
  6 個 DPMI region/size lock/unlock + alloc/free（`lock_size` 有 game caller
  `set_bgm_track_with_fade`）、`filesize_path`、2 個 abort helper（`with_log` /
  `thunk`，CRT path caller `__prtf` / `__STKOVERFLOW`）、`get_eflags`
  （`pushfd; pop eax; cli; ret` = Watcom `_disable` 4-byte primitive）。40 個
  BFS 不可達 AIL_internal_* 經 xref pattern 分為 vtable_indirect 11 +
  cluster_member 8 + tail_call_target 1 + dead_code_stub 15 + 5 crt-audit
  promotions，全部以 plate comment 標明 sub-case。14 個 placeholder helper 名
  （AIL_internal_helper_<addr> / `<desc>_helper_<addr>`）全部改為語意命名；
  1 個誤分類為 AIL 的 `helper_377c3` 經 byte-level 比對 Watcom CLIB3S
  sprintf.obj leading anonymous static 完全相符（22 bytes，0 FIXUPP）→ 重歸
  `L$1_sprintf_put_char` 並加進 crt_lookup_9.5a.json (in_lookup 140→141,
  manual_pass 35→36, L$1 entries 3→4)。執行此 audit 用的 ail_audit pipeline
  已移除（per-function inventory 改用 Ghidra MCP `search_functions` 即時拉取）
