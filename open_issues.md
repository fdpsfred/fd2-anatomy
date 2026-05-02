# 未解問題與未做分析

整理自比對所有 program_info / resource_info / assets 文件後仍存在的「未確定」、
「待驗證」與「待做」項目。每條描述：現狀 + 為什麼還沒解 + 解需要做什麼。

## 資源檔未完整解析的格式段落

### 1. ANI.DAT entry header `+0xA5..+0xA6` 後的 byte 用途

- **現狀**：ANI.DAT 每 entry 含 0xAD byte header，只解出 `+0xA5..+0xA6` 是
  `frame_count`。其餘 `0..0xA4` 與 `0xA7..0xAC` 用途未明。
- **為什麼還沒解**：ANI.DAT 是 cinematic 動畫，frame_count 已足夠播放；其餘
  metadata 不影響 decoder 正確運作。
- **解需要做什麼**：對 9 個 entries 統計各 byte 分布，與 `play_ani_file_animation_sequence`
  其他用途的 byte 對齊；對照 in-game 觀察推測 metadata 含義 (e.g. palette index
  / loop flags / frame size)。

### 2. ANI.DAT per-frame metadata `+0x04..+0x07`

- **現狀**：每 frame `frame_header` 8 bytes 中 `+0x00..+0x01 data_size` 與
  `+0x02..+0x03 decoded_size` 已解。`+0x04..+0x07` 4 bytes 用途待確認。
- **解需要做什麼**：對多 frame 統計這 4 bytes 模式；可能是 timing override
  或 frame-specific palette。

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

### 5. FD2.SAV slot trailer `+0xA0A..+0xA28` (30 bytes)

- **現狀**：每個 4-slot snapshot 結尾 30 bytes 未細分 sub-field。
- **解需要做什麼**：trace `save_current_state_to_slot @ 0x30012` 寫入這段時
  的 source globals。

### 6. tile_attribute_flags 4-byte/tile 中 +0/+1/+3 byte 用途

- **現狀**：FDSHAP `shap_id × 2 + 1` 提供 4-byte/tile attribute；只有 `+2 byte`
  的 animation/palette flag bits 已解 (`0x04` / `0x08` / `0x10`)。其餘 byte 未明。
- **解需要做什麼**：對多章 tile_attribute_flags dump 後 cross-tile 比對；
  可能含 terrain_id / movement cost / passability。

### 7. FIGANI per-pose metadata 細節

- **現狀**：per pose `+4 type` (1 = spell-cast)、`+5 sfx_hook_id`、`+6 sub_frame_count`
  已解；`+7..` (sub-frame data 之前) 用途與 sub-frame 之間的 inter-frame timing
  未細究。
- **解需要做什麼**：對 multiple poses 統計 byte 分布，trace
  `step_figani_pose_animation @ 0x2B9A1` 詳細 state machine。

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

### 10. ch9 boss 死亡 → tile_event_consumed_flags[0x10] 起始值的設定點

- **現狀**：ch9 reinforcement chain 機制已確認 (handler_1F state-machine
  spawner)。`tile_event_consumed_flags[0x10]` 起始為 1 推測，但具體 ch9_init
  哪行設定這個 flag 未確認。
- **解需要做什麼**：trace `chapter_09_init` decompile 看是否有
  `tile_event_consumed_flags[0x10] = 1` 寫入。可能是 `init_battle_state_for_chapter`
  全 zero 後遞增。

### 11. attack_action_dispatch 內 pickup_kind=2 的 ai_post_action 寫入

- **現狀**：ch9 機制描述「enemy 死亡時 attack_action_dispatch 內某 hook 把
  `ai_post_action_consequence_idx` 設為 pickup_param 對應 event_code」，但具體
  寫入 path 未 trace 完。
- **解需要做什麼**：在 `attack_action_dispatch` 與 `execute_ai_*` executors 內
  搜尋 `pickup_kind == 2` 的條件分支。

### 12. ch23 mid-handler reload 的具體 byte trigger

- **現狀**：ch23_end 中段 `current_chapter_id += 1` 然後 `load_chapter_battle_data(24)`
  載入 ch24 場景。已知是「30 章中唯一」。
- **解需要做什麼**：trace ch23_end 哪一步觸發此 reload (cutscene 完成後？
  特定 dialog page 後？)。

## 程式行為未完全理解的段落

### 13. runtime_char `+0x4E wStat4_current` 的真實語意

- **現狀**：已知 `wStat4_current = DX_total + sum item.short@+7`，由
  `recalculate_combat_stats` 寫入。但這個 stat 的遊戲意義 (魔抗 / 命中 / 迴避 /
  其他) 未 emulator 驗證。
- **解需要做什麼**：emulator 觀察戰鬥中此值如何影響擊中率 / 傷害計算。

### 14. runtime_char `+0x08[1]` byte 用途

- **現狀**：`pChar_identity_combat_byte[1]` init=0；在 `ai_score_physical_attack`
  / `score_spell_candidate` 等 path 未見 read/write，推測是 reserved padding。
- **解需要做什麼**：全 binary 搜 `runtime_char[N].pChar_identity_combat_byte[1]`
  的 read/write 點。若全無，標 confirmed reserved。

### 15. runtime_char `+0x27` 21-byte block 內未識別 sub-field

- **現狀**：已知 `[0]`, `[0xA-0xC]`, `[0xD-0xF]`, `[0x10-0x14]` 共約 13 bytes。
  其餘 7-8 bytes (`[1..9]`, `[0x15..]`) 未解。
- **解需要做什麼**：emulator trace + 全 binary write 點 search。

## 未做的批次分析

### 16. 28 個 unref chapter event handlers 的 binary content 用途

- **現狀**：`ai_post_action_consequence_table @ 0x51B91` 內 28 個 handler 在
  binary 內 exactly 2 hits = LE reloc fixup + table entry。確定無 caller，
  歸類為 cut content / 編譯殘留。但每個 handler 內部邏輯仍在 .text 中。
- **解需要做什麼**：對 28 個 unref handler 解 decompile 看是否有 cut feature
  線索 (e.g. 特定 race_id reinforcement、未使用的 dialog pattern)。

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

### 19. fdfield_entry_layout endgame_ch32 char_spawn_count 異常

- **現狀**：endgame_ch32 (FDFIELD idx 96..98) entry char_spawn_count = 30 但
  實際內容含 40 records (260 bytes 額外)。
- **解需要做什麼**：deep dump endgame_ch32 entry 看 count 與 actual record
  數的差異意義 (可能是 count 不計 reserved / inactive records，或 binary 設計
  上的 sentinel marker)。

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

### 24. Ghidra 拆出的 inlined fragment 難以獨立編譯

- **現狀**：`set_runtime_char_evade @ 0x114fb` 之類由 Ghidra 拆出的小 function
  (body 只有 5~6 條指令)，body 依賴 EDI 等 callee-saved register，但這些
  register 不是 cc 認可的傳參通道，是 parent function 留下的 register state。
  目前 cc 已標 `__fastcall` (符合 ABI 觀察)，但若直接編譯這些小 function 會
  讀到未初始化的值。
- **為什麼還沒解**：這類 function 不是真正的獨立 callable entity，是
  decompiler 的拆解產物。要真正可編譯需把它們 inline 回 parent。
- **解需要做什麼**：找 caller 為 fall-through (而非 CALL) 的 fragment，標記
  為「不要獨立宣告」；或是在 source 層級把它們手動 inline 回 parent。

### 25. 0x4b502 FUN_0004b502 的 struct 型別未還原

- **現狀**：cc 已 pin 為 `__fastcall` (EAX = 結構指標，讀
  `[EAX+0]/[EAX+4]/[EAX+8]`，加 3 個 stack args，`RET 0xc`)。decomp 仍顯示
  `in_ECX/unaff_EBX/extraout_EDX/unaff_ESI` 等 register ghost — Ghidra 的型別
  還沒理解 struct layout。recommendations.json 已標
  `needs_signature_review:true`。
- **解需要做什麼**：emulator trace + 對 Borland CRT 反查，決定 EAX 指向的
  struct 是哪一個 (可能是某種 SI:DI far pointer 包裝或 long long ops)；建立
  對應 struct datatype 後重設 prototype。

### 26. Phase 7 跳過的 277 個 LOW-confidence function 的 param 數量

- **現狀**：這 277 個 function 因為「無 caller 樣本」或「caller 訊號不一致」
  而被 Phase 7 跳過，保留 Ghidra 自動推斷的 param 數量 (大多是 0..3)。可能
  含 phantom params 或漏報 params。
- **為什麼還沒解**：caller 訊號是 ABI 推論的最強依據，沒有訊號或訊號矛盾時
  自動裁決會破壞 ABI。
- **解需要做什麼**：對這些 function 個別 decompile 比對 — body 內實際讀取了
  幾個 stack offset (`[ESP+4]`, `[ESP+8]`, ...)；對於透過 function pointer
  間接呼叫的 case，找出 function pointer 指向的所有可能值並合併 caller 訊號。

### 27. 新加入 param 的型別都是 `unsigned int`

- **現狀**：Phase 7 為 60 個 function 補了遺漏的 param，型別一律是 `uint`
  (4 bytes)。實際型別 (`char *` / `struct foo *` / `byte` / `int *` 等) 未
  細化。對 ABI 正確性無影響 (4-byte stack slot 大小一致即可正確編譯)，但
  decompile 可讀性受影響。
- **解需要做什麼**：對每個新加 param 看 callee 內部如何使用 (deref？算術？
  傳給已知 prototype 的 callee？) 推斷型別；或對 caller pre-CALL push 的
  source 推斷型別。屬可選 readability 工作，不阻擋編譯。

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
