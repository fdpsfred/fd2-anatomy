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

### 26. ✅ LOW-confidence (caller-signal-unreliable) 277 個 function param 數量 — RESOLVED

- 277 個 LOW-confidence 已全部處理：176 個 function-pointer dispatch table
  callee 在前期 bulk-fix 為 `void __cdecl func(void)`；剩 102 個 (audit 重新清點數)
  逐一 disasm 驗證，47 個 `set_function_prototype` 補正、55 個 ratify (Ghidra
  cc-correction 後計數已正確或 Borland CRT 自訂 ABI deferred)。
- 47 個 apply 分布：`spell_handler_id_*` × 11 (3 cdecl args，移除 3 phantom reg)、
  `execute_*` × 7 (各 2-7 cdecl + 移除 3 phantom)、`tick_summon` family × 7
  (5 cdecl args)、`tick_chapter_palette_animation` / `tick_tile_event_animations`
  / `chapter_19_20_21_init_shared` 等 0-arg cdecl × 多筆、其他單獨 case × 多筆。
- 0 emission blocker 全程維持：`list_bookmarks(category="Bad Instruction")` = 0
  在所有 `set_function_prototype` apply 之間皆 0。
- Borland CRT soft-FP / long-double family (`FUN_0004b761` divide、
  `FUN_0004cb34` mantissa add、`FUN_0004cb86`、`FUN_0004d53c` 等) 用 custom
  ABI (EBX/ESI/EDI 也帶輸入)，Ghidra 標準 fastcall 無法精確建模 — defer 到
  build pipeline 站起來再 byte-level 比對。
- 工具：`tools/lowconf_signature/` (`inventory.py` 抽 LOW set + signal、
  `plan_apply.py` 規則化 apply plan 產出後**未**直接套用，per-function disasm
  驗證後逐一 apply)。

### 27. 新加入 param 的型別都是 `unsigned int` (部分 audit，留 backlog)

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
- ✅ FUN_0004b502 (0x4b502) — Borland soft-FP 80-bit long double in-place add
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
- ✅ LOW-confidence (caller-signal-unreliable) 102 個 function 全部逐一審完
  — 47 個 `set_function_prototype` apply、55 個 ratify；常見模式為
  spell_handler_id / execute / tick_summon family 共用「Borland stack-probe
  prologue + 3 phantom reg + N cdecl stack args」結構；Borland CRT soft-FP /
  long-double family custom ABI (EBX/ESI/EDI 帶輸入) 留 backlog
