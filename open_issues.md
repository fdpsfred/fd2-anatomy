# 未解問題與未做分析

整理自比對所有 program_info / resource_info / assets 文件後仍存在的「未確定」、
「待驗證」與「待做」項目。每條描述：現狀 + 為什麼還沒解 + 解需要做什麼。

## 資源檔未完整解析的格式段落

### 3. TAI.DAT byte-stream payload format

- **現狀**：每 entry 已知 `+0x00..+0x03` 是 width/height (u16 LE × 2)，後續
  payload 是 opcode/payload 序列。具體 opcode 解碼未做。
- **為什麼還沒解**：BG/TAI 配對載入後實際呈現的視覺由 `play_full_combat_cinematic`
  / `execute_summon_spell_cast` 等 caller 控制，TAI 內容是輔助資料。
- **解需要做什麼**：在 in-game 呈現時 trace `play_spell_cast_sequence` 內如何
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
- **Status:** data-only — `play_ani_file_animation_sequence @ 0x20421` 讀
  全 0xAD header 但只用 `+0xA5..0xA6` (frame_count)，其餘 byte 無條件分支。

### 2. ANI.DAT per-frame metadata `+0x04..+0x07`

- **現狀**：每 frame `frame_header` 8 bytes 中 `+0x00..+0x01 data_size` 與
  `+0x02..+0x03 decoded_size` 已解。`+0x04..+0x07` 4 bytes 用途待確認。
- **解需要做什麼**：對多 frame 統計這 4 bytes 模式；可能是 timing override
  或 frame-specific palette。
- **Status:** data-only — `play_ani_file_animation_sequence` 不讀 +0x04..+0x07，
  emission 不受影響。

### 5. FD2.SAV slot trailer `+0xA0A..+0xA28` (30 bytes)

- **現狀**：每個 4-slot snapshot 結尾 30 bytes 未細分 sub-field。
- **解需要做什麼**：trace `save_current_state_to_slot @ 0x30012` 寫入這段時
  的 source globals。
- **Status:** data-only — `save_current_state_to_slot @ 0x30012` 寫 0xA00
  map + 9 個 scalar (gold/chapter/speed/sfx flags)，+0xA0A..0xA28 未被操作；
  存檔讀寫走 memcpy 整段保留，emission 不受影響。

### 6. tile_attribute_flags 4-byte/tile 中 +0/+1/+3 byte 用途

- **現狀**：FDSHAP `shap_id × 2 + 1` 提供 4-byte/tile attribute；只有 `+2 byte`
  的 animation/palette flag bits 已解 (`0x04` / `0x08` / `0x10`)。其餘 byte 未明。
- **解需要做什麼**：對多章 tile_attribute_flags dump 後 cross-tile 比對；
  可能含 terrain_id / movement cost / passability。
- **Status:** data-only — `composite_battle_tile_map @ 0x12247` 與
  `read_tile_attribute_at_pos` 只讀 +0 byte 的 bit 0x04/0x08/0x10
  (animation/palette flag)；+0/+1/+3 其他 byte 不做條件分支。

### 7. FIGANI per-pose metadata 細節

- **現狀**：per pose `+4 type` (1 = spell-cast)、`+5 sfx_hook_id`、`+6 sub_frame_count`
  已解；`+7..` (sub-frame data 之前) 用途與 sub-frame 之間的 inter-frame timing
  未細究。
- **解需要做什麼**：對 multiple poses 統計 byte 分布，trace
  `step_figani_pose_animation @ 0x2B9A1` 詳細 state machine。
- **Status:** data-only — `step_figani_pose_animation @ 0x2B9A1` 只讀 per-pose
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

### 9. ch1 哈瓦特 char_spawn_record `+0x94/+0x95/+0x96` 三 byte 確切值

- **現狀**：「哈瓦特暴走」機制已確認是 char_spawn_record `+0x94/+0x95/+0x96`
  → `pCombat_aux_block[0xD/E/F]` 拷貝 + 哈諾死亡後 protective AI fall-through
  為 default attacker。但 ch1 30 個 records 中具體哪一個是哈瓦特、哈諾，及
  各自 `ai_class` 數值未列舉。
- **解需要做什麼**：用 `fdfield_char_spawn_decoder.py` dump ch1 entry 1 的 30
  records，比對 char_id 與 ai_class。

## 程式行為未完全理解的段落

### 13. runtime_char `+0x4E wStat4_current` 的真實語意

- **現狀**：已知 `wStat4_current = DX_total + sum item.short@+7`，由
  `recalculate_combat_stats` 寫入。但這個 stat 的遊戲意義 (魔抗 / 命中 / 迴避 /
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

### 20. ch20 達可塞「15 回合內」與 binary `< 16` 的對齊

- **現狀**：binary gate `save_metadata_block < 16` 表示 turn 1..15 達可塞可加入。
  攻略本「15 回合內」對應 turn 1..15。**精準對齊，無差異**。記錄此項作為「已驗證
  匹配」。
- **解需要做什麼**：無，已 resolved。

### 21. ch23 羅德曼「15 回合內」與 binary `< 15` 的對齊

- **現狀**：binary gate `save_metadata_block < 15` 表示 turn 1..14 羅德曼可加入。
  攻略本「15 回合內」可能對應 turn 1..15。可能是攻略本筆誤 (應為「14 回合內」)，
  也可能 binary off-by-one。
- **解需要做什麼**：emulator 實測 turn 14 (binary <15 PASS / 攻略≤15 PASS) 與
  turn 15 (binary <15 FAIL / 攻略≤15 PASS) 的羅德曼加入結果。

### 22. ch13 攻略「哈瓦諾」vs binary `init_runtime_char_from_base_growth(3) (哈瓦特)`

- **現狀**：攻略 ch13 寫「加入：戰士哈瓦諾」；binary char_id 3 對應「哈瓦特」
  (per `assets/text/global_text.md` page 4)。**結論：攻略筆誤，正確角色名應為
  哈瓦特**。
- **解需要做什麼**：無，已 resolved。

## 工具相關未解項

### 23. tools/decoders/ 對個別 DAT 的端到端 round-trip 驗證

- **現狀**：FDFIELD / FDTXT / FDSHAP / DATO / FDMUS / FDOTHER / FDICON / BG / TAI
  / ANI 各 decoder 在 cluster phase 階段已通過 sample round-trip。但 `tools/`
  reorganize 後是否能各自獨立執行成功 (新自帶路徑常數)，待 final 驗證。
- **解需要做什麼**：跑 `tools/README.md` 內列出的各 script 至少一次。

## Calling convention 校正後的剩餘限制

### 26. 新加入 param 的型別都是 `unsigned int` (部分 audit，留 backlog)

- **現狀**：auto param-count classifier 為 41 個 function 補了 61 個遺漏 param，
  型別一律 `uint` (4 bytes)。抽樣分析發現：
  - 30 個 chapter_NN_init / chapter_NN_end 的 added param 多為 unused
    passthrough (body 不讀，僅是 caller pre-CALL EAX 訊號的反映)；改型別
    無 codegen 影響，僅 readability。
  - 11 個 misc functions (FUN_xxxxx / noop_stub) 含部分指標型別已正確
    (e.g. FUN_000361a5 已是 `uint *`)，少數需個別 decomp 推 ptr/struct type。
- **狀態**：對 ABI 正確性無影響 (4-byte stack slot 大小一致即可正確編譯)，
  純粹是 decompile 可讀性。Per-function manual analysis 工作量大，列為
  backlog；當前編譯目標不受影響。

## 重建相關 backlog

### 27. 套 `crt_matches_9.5a.json` 的 high-confidence 命名回 FD2.LE

- **現狀**：`rebuild_info/crt_matches_9.5a.json` 列 131 個 FD2 function 對應的
  Watcom 9.5a CRT symbol，其中 score ≥ 14.6 的 106 筆是高信心命中。FD2.LE
  內這批位址多數仍是 `crt_helper_*` 佔位命名。
- **解需要做什麼**：批次把 high-confidence 命中套回 Ghidra symbol table，
  並對 score < 14.6 的 12 筆用 §7.2 (body size vs lib symbol expected size)
  準則逐筆人工判斷。

### 28. 找出 FD2 連結時的 wlink linker 設定

- **現狀**：已知 FD2.LE 是 LE format + DOS/4GW DPMI extender，但詳細的
  wlink script (segment ordering / DGROUP layout / stack size / heap
  setup / `runtime` 連結選項等) 還沒拆出來。
- **解需要做什麼**：對 FD2.LE 的 LE header / segment table / DGROUP 配置做
  static 分析，反推 wlink 命令列。

### 29. 手動 patch 3 個無法 import 的 .obj

- **現狀**：770 個 dedup 後的 Watcom CRT .obj 中 3 個觸發 Ghidra OmfLoader
  的 EOF bug 而 import 失敗：`fpeinth.obj` (FPE handler)、`font8x8.obj` ×2
  (VGA ROM 字型 bitmap)。FD2 都不連結這 3 個，所以對版本判定與 CRT
  識別結果無影響，但理論完整度上仍是缺口。
- **解需要做什麼**：trace Ghidra OmfLoader 為何在處理完 MODEND 後仍試圖
  多讀 1 byte，patch loader 或重組 .obj 結構。

## 已解問題（記錄為基線）

- ✅ 哈瓦特暴走機制 (ch1) — char_spawn_record +0x94/0x95/0x96 → protective AI fall-through
- ✅ ch9 reinforcement waves — handler_1F state machine + race_id increment
- ✅ ch20 Stage C 沼澤怪物排除 win override — 純 static 解
- ✅ ch15/17 conditional dialog — `check_party_has_char_id(0x0C 凱麗 / 0x12 蜜蒂)`
- ✅ FDFIELD entry layout — `0x83 byte header + N × 0x1A char_spawn_records`
- ✅ chapter event 不是 bytecode — 直接函數 dispatch via `ai_post_action_consequence_table @ 0x51B91`
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
  `init_battle_state_for_chapter @ 0x205DA` 的 `crt_memset(flags, 0, 0x20)` 清 0；
  handler_1F 從 race_id=0 遞增
- ✅ pickup_kind=2 path — `process_battle_drop_entries` type 2 case 直接
  dispatch via `ai_post_action_consequence_table[ushort_value]()`，不寫
  `ai_post_action_consequence_idx` global；tile-step trigger (Path 1) 與
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
  cc 分布 `__cdecl` 957 / `__watcall` 42 / `__stdcall` 1；soft-FP family custom
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
- ✅ Function-boundary fall-through audit — 對 1699 個 function 跑「prev_fn
  最後 inst 有 fall-through 進 this_fn entry」audit，扣除 7 個已結構性修復
  的案例後共 98 個 candidate，個別驗證後分為 6 種 benign 模式：73 個
  `align_nop_*` (zero ref 確認 — entry/body 任何 byte 都無 CALL/JUMP/DATA/
  INDIRECTION 等 reference) + 8 個 SHARED EPILOGUE STUB (`noop_stub_b43` /
  `noop_stub_c49` / `noop_stub_1011` / `noop_stub_1452` / `noop_stub_13994` /
  `set_battle_anim_phase_to_1` / `AIL_log_decrement_nesting` / `noop_stub_3cbc4`，
  共用 epilogue 由多個 source function 經 fall-through 或 tail-JMP 進入) +
  3 個 SHARED BODY 多 entry (`play_palette_fade_to_black` /
  `check_battle_end_condition` / `crt_softfp_uint32_to_ld`，與其同伴 entry 共用
  邏輯主體) + 4 個 HEADER-ONLY ENTRY (chapter_event_handler 系列，prev 只 PUSH
  args / frame_size，fall-through 進真正執行的 this) + 5 個 DEAD FALL-THROUGH
  (prev 末尾的 fall-through 在執行流上死掉，例如 `exit` / `crt_terminate`) +
  4 個 DATA TABLE FRAGMENT (jump table 區段被 Phase F 誤 disassemble 為 code) +
  1 個 STATE-MACHINE INIT-ENTRY (`AIL_helper_41834` → `AIL_helper_4183d`)。
  emulator-level 驗證（noop_stub_b43）確認 ESP 平衡邏輯正確。詳細逐 case
  分析見 `workspace/function_review/phase_g_25_classification.md`，
  emit pipeline 對每個模式的處理規則寫在 `program_info/emit_pipeline_spec.md`

- ✅ Ghidra jump table 漏抓 audit (issue #30) — 全 binary 對 indirect JMP /
  orphan code / 異常小 body / fragmented body 做 audit。**結論**：63 個
  indirect JMP 全 target 在 body 內 (AIL 4 + CRT 59 + GAME 0)；遊戲端 0 個
  indirect JMP（switch 用 if/else 鏈分派，不走 jump table）；CRT lookup 140
  entry vs lib `.obj` size 全面 diff 在 3 個 mismatch 修完後 0 不一致。修復
  項目：(a) `AIL_internal_mix_loop_8bit_stereo @ 0x49306` body 1→58 byte
  (bytes 已存在但未 disassembled)、(b) `AIL_set_sequence_volume @ 0x3add4`
  130 byte function 全部 bytes-cleared，re-disassemble 還原、(c) 兩個遺漏
  setter `crt_set_word_global_52758 @ 0x3615e` / `crt_set_word_global_5275c
  @ 0x3616e` (各 16 byte，get-and-set helper) create_function 補齊、(d)
  `chapter_01_init` body 2-range→1-range (entry 5-byte instruction 跨越 hole)、
  (e) `crt_abort_thunk` body 2-range→1-range (同類 hole)、(f) `__MemAllocator`
  body 171→176 (5-byte 尾段 alternate-exit thunk)、(g) `__MemFree` body 272
  →267 (前述 5 byte 從 MemFree 改歸 MemAllocator)、(h) `__STOSB` body 49
  →55 (含 6-byte 尾段 alignment NOPs)。Ghidra category=Bad Instruction 0、
  find_code_gaps min_size=1 為 0。新工具 `tools/jump_table_audit/`
  compare_lookup_sizes.py 可重複跑做 regression。詳見 `rebuild_info/crt_fid_match.md`
  §12.2、`tools/jump_table_audit/_index.md`
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
  `crt_<descriptor>_<addr>` vendor placeholder）/ game 638；call graph
  凍結於 `program_info/call_graph.{json,dot,md}`，4370 條 edge。
  `AIL_end_sample @ 0x3958f` 涵蓋 PUSH EDI prologue（body 0x3958f-0x395FB，
  3 caller，對應 `AIL_end_sample_inner @ 0x41460` 1 caller）；
  `chapter_01_init` / `chapter_08_end` / `chapter_29_end` /
  `crt_capture_ss_for_stkchk` / `crt_sin_inner` / `crt_abort_thunk` /
  `save_runtime_char_to_template` 等 function body 涵蓋全部 reachable 指令
- ✅ AIL function classification — 287 個 AIL ecosystem function 完成分類：
  103 個 `AIL_*` 公開 API + 184 個 `AIL_internal_*`。9 個 `crt_*` 經三向誤分類
  audit（xref-source / orphan-disasm / reverse caller-set）reclassify 為
  AIL：4 個 BFS-reached 的 caller-全 AIL + 引用 AIL global（`alloc_and_commit` /
  `decommit_and_free` / `load_file_to_memory` / `parse_int_with_base`），
  另 5 個 BFS-unreached 的 vtable-indirect / orphan placeholder（3 個
  dpmi_unlock helper unlock 特定 AIL region 0x3f190.. / 0x41dc0.. / 0x45320..；
  2 個 wave-synth instrument lookup helper — `helper_454fd` byte-level 確認
  770 個 CLIB3S obj + CLIB3R obj 全部 0 match）。10 個 `crt_*` 確認為 Watcom
  CRT primitive 保留：6 個 DPMI region/size lock/unlock + alloc/free（`lock_size`
  有 game caller `set_bgm_track_with_fade`）、`filesize_path`、2 個 abort
  helper（`with_log` / `thunk`，CRT path caller `__prtf` / `__STKOVERFLOW`）、
  `get_eflags`（`pushfd; pop eax; cli; ret` = Watcom `_disable` 4-byte primitive）。
  36 個 BFS 不可達 AIL_internal_* 經 xref pattern 分為 vtable_indirect 11 /
  cluster_member 9 / tail_call_target 1 / dead_code_stub 15，全部以 plate
  comment 標明 sub-case。14 個 placeholder helper 名（AIL_internal_helper_<addr>
  / `<desc>_helper_<addr>`）全部改為語意命名；1 個誤分類為 AIL 的 `helper_377c3`
  經 byte-level 比對 Watcom CLIB3S sprintf.obj leading anonymous static 完全
  相符（22 bytes，0 FIXUPP）→ 重歸 `L$1_sprintf_put_char` 並加進
  crt_lookup_9.5a.json (in_lookup 140→141, manual_pass 35→36, L$1 entries 3→4)。
  reusable script 於 `tools/ail_audit/`（xref_source_audit / reverse_classification_audit
  / orphan_review / build_inventory），per-function inventory 由
  `tools/ail_audit/build_inventory.py` 從 Ghidra 即時產生
