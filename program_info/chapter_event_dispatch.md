# chapter_event_dispatch

FD2 的「event scripts」**沒有自訂 bytecode interpreter**。每章 event 是「資料
驅動的函數 dispatch」：FDFIELD entry 內的 hook table 紀錄 (turn, event_code,
phase) 三元組，`event_code` 索引到一張 90-entry function pointer table，pointer
指向已編譯的 cinematic C 函數。

## 關鍵函數

| 位址 | 名稱 | 角色 |
|---|---|---|
| `0x1A813` | `fd2_fire_chapter_turn_events_for_phase` | turn-event dispatcher |
| `0x13A77` | `fd2_check_tile_event_post_action` | tile-step dispatcher |
| `0x1A30B` | `fd2_run_full_turn_cycle` | 6-phase turn loop，呼叫上述 dispatcher |
| `0x10C50` | `fd2_init_runtime_char_for_battle` | 從 char_spawn_record 創建 runtime_char |

## Dispatch table @ 0x51B91

`data_fd2_battle_ai_post_action_consequence_table @ 0x51B91`：90 entries (idx 0x00..0x59)，
4-byte LE function pointer，指向 .text `0x34000-0x36100` 範圍內的
`fd2_chapter_event_handler_*` 函數。**雙重用途**：FDFIELD chapter event hook
與 AI post-action consequence 共用同一張表。

## Turn-event 觸發機制

每 chapter `chapter_id*3+1` FDFIELD entry 內，offset `+0x03..+0x32` 是
16 entries × 3 bytes 的 turn-event hook table：

```c
struct turn_event_hook {
    uint8_t turn;        // matches save_metadata_block (1-based player turn)
    uint8_t event_code;  // index into chapter_event_jump_table @ 0x51B91
    uint8_t phase;       // 0 = enemy_turn_intro,
                         // 1 = end_of_player_turn,
                         // 2 = new_player_turn_intro
};
```

**Sentinel slot**：`(turn=0xFF, event_code=0xFF, phase=0)`。turn=0xFF 永遠不會
等於 `save_metadata_block`（最多 ~30），所以 sentinel 不會 fire。30 章共 406 個
sentinel slot。

**特例**：少數章節（ch9 / ch27 / ch28 / ch29 / ch30）有
`(turn=0xFF, event_code≠0xFF, phase=0/2)` 形態 — turn=0xFF 不會匹配，但
`event_code` 不是 0xFF。這是「動態啟動候選」：tile-step-event handler 會
re-write turn byte (見下節)。

## Phase 觸發時點 (per `fd2_run_full_turn_cycle`)

每完整 turn cycle 內呼叫順序：

| 內部 phase | 動作 | 對應 fire phase | save_metadata_block 值 |
|---|---|---|---|
| Phase A | player heal pass | — | turn N |
| Phase B | end-of-player-turn | **fire(phase=1)** | turn N |
| Phase C | NPC team turn | — | turn N |
| Phase D | enemy banner + intro | **fire(phase=0)** | turn N |
| Phase E | enemy team turn | — | turn N |
| Phase F | TURN N+1 reveal anim | save += 1, **fire(phase=2)** | turn N+1 |

`save_metadata_block` 是「即將進行的 player turn 編號」(從 1 起算)。turn 1 沒
phase 1/0 fire (沒有「上一回合 end」)，但有 phase 2 fire
(`fd2_init_battle_state_for_chapter` 設 1 之後立即進入 turn 1)。

## Tile-step-event hook (offset +0x33..+0x52)

16 × 2 bytes:

```c
struct tile_step_event_hook {
    uint8_t consequence_idx;   // event_code 索引到 data_fd2_battle_ai_post_action_consequence_table
    uint8_t event_type;        // 觸發 context 過濾 (0/1/2 對應 post-walk/post-attack/etc.)
};
```

機制：char 移動到一個有 `tile_event_id != 0` 的 tile 時，`fd2_check_tile_event_post_action`
查表後設 `data_fd2_battle_ai_post_action_consequence_idx`，下一 phase loop iteration 觸發
`data_fd2_battle_ai_post_action_consequence_table[consequence_idx]()`。

30 章 `+0x33..+0x52` 觀察：
- ch1-6 / 8-12 / 15-24：全 sentinel (`FF 00`) — 無 tile-step events
- ch7：1 active (entry 0 = `1A 00`)
- ch13：1 active (entry 0 = `08 01`)
- ch14：1 active (entry 0 = `0A 00`)
- ch25：1 active (entry 0 = `37 01`)
- ch26：3 active (entries 0/1/2 = `3B 00 / 3C 00 / 3D 01`)
- ch27：2 active (entries 0/1 = `3E 00 / 54 01`)
- ch28：2 active (entries 0/1 = `41 00 / 45 00`)
- ch29：1 active (entry 1 = `4B 01`)
- ch30：1 active (entry 0 = `50 01`)

## 動態 turn-event 啟動

ch27 / 28 / 29 / 30 的「turn=0xFF 但 event_code 非 0xFF」entries 是動態啟動
候選。實際觸發鏈：

```
1. char (典型為主角索爾) 走到 tile_event_id=N 的 tile
2. check_tile_event_post_action → 派發 data_fd2_battle_ai_post_action_consequence_table[idx]()
3. handler 內檢查 tile_event_consumed_flags[+0x11] (first-time flag)
4. 如果 first time，HANDLER 直接 WRITE 到 tile_event_data_table 的 turn-event 區段:
     tile_event_data_table[+3] = save_metadata_block      // turn_event[0].turn = current turn
     tile_event_data_table[+6] = save_metadata_block + 1  // turn_event[1].turn = next turn
   → turn-event 表 entries 0/1 從 turn=0xFF 變成 turn=N/N+1, 動態啟動
5. 同時 set tile_event_consumed_flags[+0x10] = race_id (for subsequent reinforcement)
6. 設 tile_event_consumed_flags[+0x11] = 1 (mark consumed; once-only)
7. 下一個 turn (Phase B/D/F)，剛被啟動的 turn-event hooks 會 fire 對應 event_code
   handler，繼續 cinematic 鏈
```

實例：
- **ch29 0x4B (event_type=1)**：handler 檢查 char_idx 是否 char_id=9（變身
  cinematic 主角），若是顯示 dialog page 1 + activate turn_events[0]/[1]
  (= 0x4A turn=0xFF entry → 啟動為 current+1 turn) + 設起始 race_id=4。
  下一回合 turn_events[0] event_code=0x4A handler 自動 fire，繼續變身序列。
- **ch30 0x52 (turn=0xFF)**：handler 用 stateful race_id
  (`tile_event_consumed_flags[+0x10]`) 控制 final boss 各階段 reinforcement 與
  cinematic。state=4 是最後階段，state<4 是中段 race-by-race spawn 與
  character warp。

## 90-entry handler 完整對照表

每個 handler 的命名 convention：`fd2_chapter_event_handler_NN__chC_<purpose>` /
`__shared_<purpose>` / `__unref_<purpose>` / `__sentinel`。

| idx | addr | 命名 | category | chapters | refs |
|---|---|---|---|---|---|
| 0x00 | `0x000341DB` | `fd2_chapter_event_handler_00__ch1_dialog_with_state` | dialog_with_state | ch1 | 1 |
| 0x01 | `0x000342B5` | `fd2_chapter_event_handler_01__ch1_dialog_with_state` | dialog_with_state | ch1 | 1 |
| 0x02 | `0x0003431D` | `fd2_chapter_event_handler_02__ch1_dialog_with_state` | dialog_with_state | ch1 | 1 |
| 0x03 | `0x00034377` | `fd2_chapter_event_handler_03__ch1_dialog_with_state` | dialog_with_state | ch1 | 1 |
| 0x04 | `0x000343E2` | `fd2_chapter_event_handler_04__unref_dialog_with_state` | dialog_with_state | - | 0 |
| 0x05 | `0x00034D68` | `fd2_chapter_event_handler_05__ch13_thunk` | thunk | ch13 | 1 |
| 0x06 | `0x00034422` | `fd2_chapter_event_handler_06__ch2_reinforcement` | reinforcement_spawner | ch2 | 1 |
| 0x07 | `0x00034D72` | `fd2_chapter_event_handler_07__ch13_dialog_with_state` | dialog_with_state | ch13 | 1 |
| 0x08 | `0x00034DCD` | `fd2_chapter_event_handler_08__ch13_first_time` | first_time_gated | ch13 | 1 |
| 0x09 | `0x000344C2` | `fd2_chapter_event_handler_09__ch3_char_cond` | char_conditional | ch3 | 1 |
| 0x0A | `0x00034E3B` | `fd2_chapter_event_handler_0a__ch14_first_time` | first_time_gated | ch14 | 1 |
| 0x0B | `0x00034565` | `fd2_chapter_event_handler_0b__ch4_dialog` | dialog_only | ch4 | 1 |
| 0x0C | `0x00034594` | `fd2_chapter_event_handler_0c__unref_first_time` | first_time_gated | - | 0 |
| 0x0D | `0x00034E90` | `fd2_chapter_event_handler_0d__ch15_dialog_with_state` | dialog_with_state | ch15 | 1 |
| 0x0E | `0x000345EA` | `fd2_chapter_event_handler_0e__ch5_dialog_with_state` | dialog_with_state | ch5 | 1 |
| 0x0F | `0x0003462E` | `fd2_chapter_event_handler_0f__ch5_dialog_with_state` | dialog_with_state | ch5 | 1 |
| 0x10 | `0x00034696` | `fd2_chapter_event_handler_10__ch5_dialog` | dialog_only | ch5 | 1 |
| 0x11 | `0x000346C8` | `fd2_chapter_event_handler_11__ch5_dialog_with_state` | dialog_with_state | ch5 | 1 |
| 0x12 | `0x00034F02` | `fd2_chapter_event_handler_12__ch15_dialog_with_state` | dialog_with_state | ch15 | 1 |
| 0x13 | `0x00034716` | `fd2_chapter_event_handler_13__unref_char_cond` | char_conditional | - | 0 |
| 0x14 | `0x000347B1` | `fd2_chapter_event_handler_14__ch6_dialog` | dialog_only | ch6 | 1 |
| 0x15 | `0x000347D9` | `fd2_chapter_event_handler_15__ch6_char_cond` | char_conditional | ch6 | 1 |
| 0x16 | `0x00034819` | `fd2_chapter_event_handler_16__ch6_char_cond` | char_conditional | ch6 | 1 |
| 0x17 | `0x00034844` | `fd2_chapter_event_handler_17__unref_turn_gated` | turn_conditional | - | 0 |
| 0x18 | `0x000348FC` | `fd2_chapter_event_handler_18__unref_dialog` | dialog_only | - | 0 |
| 0x19 | `0x00034924` | `fd2_chapter_event_handler_19__ch7_first_time` | first_time_gated | ch7 | 1 |
| 0x1A | `0x0003499B` | `fd2_chapter_event_handler_1a__ch7_char_cond` | char_conditional | ch7 | 1 |
| 0x1B | `0x000349D9` | `fd2_chapter_event_handler_1b__ch8_cinematic` | cinematic_no_dialog | ch8 | 6 |
| 0x1C | `0x00034A0E` | `fd2_chapter_event_handler_1c__ch8_ai_ctrl` | ai_setup | ch8 | 1 |
| 0x1D | `0x00034A3C` | `fd2_chapter_event_handler_1d__unref_dialog_with_state` | dialog_with_state | - | 0 |
| 0x1E | `0x00034A7A` | `fd2_chapter_event_handler_1e__unref_major_cinematic` | major_endgame_cinematic | - | 0 |
| 0x1F | `0x00034B5D` | `fd2_chapter_event_handler_1f__ch9_reinforcement` | reinforcement_spawner | ch9 | 2 |
| 0x20 | `0x00034BE2` | `fd2_chapter_event_handler_20__ch10_dialog` | dialog_only | ch10 | 1 |
| 0x21 | `0x00034C1E` | `fd2_chapter_event_handler_21__ch10_dialog_with_state` | dialog_with_state | ch10 | 1 |
| 0x22 | `0x00034C6C` | `fd2_chapter_event_handler_22__unref_dialog` | dialog_only | - | 0 |
| 0x23 | `0x00034C76` | `fd2_chapter_event_handler_23__ch12_cinematic` | cinematic_no_dialog | ch12 | 1 |
| 0x24 | `0x00034CB3` | `fd2_chapter_event_handler_24__ch12_ai_ctrl` | ai_setup | ch12 | 1 |
| 0x25 | `0x00034CCC` | `fd2_chapter_event_handler_25__unref_major_cinematic` | major_endgame_cinematic | - | 0 |
| 0x26 | `0x00034F42` | `fd2_chapter_event_handler_26__ch15_dialog` | dialog_only | ch15 | 1 |
| 0x27 | `0x00034F74` | `fd2_chapter_event_handler_27__unref_drop` | drop_dialog | - | 0 |
| 0x28 | `0x00034FCB` | `fd2_chapter_event_handler_28__ch17_dialog_with_state` | dialog_with_state | ch17 | 1 |
| 0x29 | `0x00034FF0` | `fd2_chapter_event_handler_29__unref_drop` | drop_dialog | - | 0 |
| 0x2A | `0x0003505F` | `fd2_chapter_event_handler_2a__ch18_dialog` | dialog_only | ch18 | 1 |
| 0x2B | `0x00035091` | `fd2_chapter_event_handler_2b__ch18_ai_ctrl` | ai_setup | ch18 | 1 |
| 0x2C | `0x000350A4` | `fd2_chapter_event_handler_2c__ch19_ai_ctrl` | ai_setup | ch19 | 1 |
| 0x2D | `0x000350B9` | `fd2_chapter_event_handler_2d__ch19_ai_ctrl` | ai_setup | ch19 | 1 |
| 0x2E | `0x000350CC` | `fd2_chapter_event_handler_2e__ch19_reinforcement` | reinforcement_spawner | ch19 | 1 |
| 0x2F | `0x00035112` | `fd2_chapter_event_handler_2f__ch21_turn_gated` | turn_conditional | ch21 | 4 |
| 0x30 | `0x000351C6` | `fd2_chapter_event_handler_30__ch21_ai_ctrl` | ai_setup | ch21 | 1 |
| 0x31 | `0x000351E9` | `fd2_chapter_event_handler_31__ch22_turn_gated` | turn_conditional | ch22 | 2 |
| 0x32 | `0x00035261` | `fd2_chapter_event_handler_32__ch22_reinforcement` | reinforcement_spawner | ch22 | 1 |
| 0x33 | `0x0003529A` | `fd2_chapter_event_handler_33__unref_drop` | drop_dialog | - | 0 |
| 0x34 | `0x000352E2` | `fd2_chapter_event_handler_34__ch23_ai_ctrl` | ai_setup | ch23 | 4 |
| 0x35 | `0x00035321` | `fd2_chapter_event_handler_35__unref_dialog_with_state` | dialog_with_state | - | 0 |
| 0x36 | `0x0003535D` | `fd2_chapter_event_handler_36__ch24_cinematic` | cinematic_no_dialog | ch24 | 4 |
| 0x37 | `0x000353DA` | `fd2_chapter_event_handler_37__ch25_first_time` | first_time_gated | ch25 | 1 |
| 0x38 | `0x00035487` | `fd2_chapter_event_handler_38__ch25_dialog_with_state` | dialog_with_state | ch25 | 1 |
| 0x39 | `0x000354DD` | `fd2_chapter_event_handler_39__ch26_cinematic` | cinematic_no_dialog | ch26 | 9 |
| 0x3A | `0x000354FE` | `fd2_chapter_event_handler_3a__unref_pickup` | item_pickup | - | 0 |
| 0x3B | `0x00035641` | `fd2_chapter_event_handler_3b__ch26_ai_ctrl` | ai_setup | ch26 | 1 |
| 0x3C | `0x00035675` | `fd2_chapter_event_handler_3c__ch26_ai_ctrl` | ai_setup | ch26 | 1 |
| 0x3D | `0x000356B7` | `fd2_chapter_event_handler_3d__ch26_pickup` | item_pickup | ch26 | 1 |
| 0x3E | `0x00035898` | `fd2_chapter_event_handler_3e__ch27_dyn_turn_event` | state_machine_mutator | ch27 | 1 |
| 0x3F | `0x000358C7` | `fd2_chapter_event_handler_3f__ch27_ai_ctrl` | ai_setup | ch27 | 1 |
| 0x40 | `0x000358EA` | `fd2_chapter_event_handler_40__unref_dyn_turn_event` | state_machine_mutator | - | 0 |
| 0x41 | `0x0003599B` | `fd2_chapter_event_handler_41__shared_dyn_turn_event` | state_machine_mutator | ch27, ch28 | 2 |
| 0x42 | `0x000359C8` | `fd2_chapter_event_handler_42__ch28_dialog_with_state` | dialog_with_state | ch28 | 1 |
| 0x43 | `0x00035A2F` | `fd2_chapter_event_handler_43__unref_dyn_turn_event` | state_machine_mutator | - | 0 |
| 0x44 | `0x00035A48` | `fd2_chapter_event_handler_44__ch28_dialog_with_state` | dialog_with_state | ch28 | 1 |
| 0x45 | `0x00035AB8` | `fd2_chapter_event_handler_45__ch28_dyn_turn_event` | state_machine_mutator | ch28 | 1 |
| 0x46 | `0x00035B05` | `fd2_chapter_event_handler_46__ch28_dialog_with_state` | dialog_with_state | ch28 | 1 |
| 0x47 | `0x00035B6B` | `fd2_chapter_event_handler_47__unref_dyn_turn_event` | state_machine_mutator | - | 0 |
| 0x48 | `0x00035BF2` | `fd2_chapter_event_handler_48__unref_ai_ctrl` | ai_setup | - | 0 |
| 0x49 | `0x00035C23` | `fd2_chapter_event_handler_49__unref_sentinel` | sentinel | - | 0 |
| 0x4A | `0x00035C32` | `fd2_chapter_event_handler_4a__ch29_dyn_turn_event` | state_machine_mutator | ch29 | 1 |
| 0x4B | `0x00035C79` | `fd2_chapter_event_handler_4b__ch29_major_cinematic` | major_endgame_cinematic | ch29 | 1 |
| 0x4C | `0x00035D60` | `fd2_chapter_event_handler_4c__ch29_major_cinematic` | major_endgame_cinematic | ch29 | 1 |
| 0x4D | `0x00035EBE` | `fd2_chapter_event_handler_4d__unref_sentinel` | sentinel | - | 0 |
| 0x4E | `0x00035ED2` | `fd2_chapter_event_handler_4e__unref_sentinel` | sentinel | - | 0 |
| 0x4F | `0x00035EE6` | `fd2_chapter_event_handler_4f__ch29_dyn_turn_event` | state_machine_mutator | ch29 | 1 |
| 0x50 | `0x00035F5A` | `fd2_chapter_event_handler_50__ch30_ai_ctrl` | ai_setup | ch30 | 1 |
| 0x51 | `0x00035F6F` | `fd2_chapter_event_handler_51__unref_dyn_turn_event` | state_machine_mutator | - | 0 |
| 0x52 | `0x00035F92` | `fd2_chapter_event_handler_52__ch30_major_cinematic` | major_endgame_cinematic | ch30 | 1 |
| 0x53 | `0x00036088` | `fd2_chapter_event_handler_53__unref_dialog_with_state` | dialog_with_state | - | 0 |
| 0x54 | `0x000360C0` | `fd2_chapter_event_handler_54__ch27_ai_ctrl` | ai_setup | ch27 | 1 |
| 0x55 | `0x000360D8` | `fd2_chapter_event_handler_55__unref_sentinel` | sentinel | - | 0 |
| 0x56 | `0x000360E3` | `fd2_chapter_event_handler_56__unref_sentinel` | sentinel | - | 0 |
| 0x57 | `0x000360EA` | `fd2_chapter_event_handler_57__unref_sentinel` | sentinel | - | 0 |
| 0x58 | `0x000360F1` | `fd2_chapter_event_handler_58__unref_sentinel` | sentinel | - | 0 |
| 0x59 | `0x000360F8` | `fd2_chapter_event_handler_59__unref_sentinel` | sentinel | - | 0 |

## Handler 重要結構性發現

1. **53 / 90 handler 用 `current_chapter_text`**：呼
   `fd2_display_dialog_scene(current_chapter_text, page_id)`，FDTXT 入口 idx 在
   chapter init 時設定。同一 handler 若被 N 個 chapter 用，產生 N 個 page→scene
   mapping。
2. **idx 0x3A 例外用 `all_game_text`**：唯一的 pickup 處理 handler，呼
   `fd2_display_dialog_scene(all_game_text, page=0x1e0)` (inv full) 與 page 0x1a6
   (pickup ok)。
3. **28 個 `unref_*` handler**：未在 30 章任何 FDFIELD turn-event/tile-step hook
   出現，binary 內 exactly 2 hits = LE reloc fixup record + 0x51B91 dispatch
   table entry。確定屬 cut content / 編譯殘留 / unreachable trigger path。

   **28 個 unref handler 的 cut-feature pattern 分布**：

   | category | 數量 | 含義 |
   |---|---|---|
   | sentinel | 8 | 7-byte 空 stub (`__CHK` + `RET`)；reserved table slots，沒有實際邏輯。idx 0x49 設 flag[0x12]=1 是唯一含寫入的 sentinel |
   | state_machine_mutator | 4 | 含 `flag[0x10]++` + `tile_event_data_table +3=save_meta+1` 等 turn-event 動態啟動邏輯；推測為 cut 章節的 dyn-turn-event 觸發 |
   | dialog_with_state | 4 | 純 dialog page + state mutation；推測為 cut dialog branch |
   | drop_dialog | 3 | 含 inventory full / pickup ok 的 dialog；推測為 cut item drop |
   | major_endgame_cinematic | 2 | 完整 cinematic 含 char spawn / dialog / portrait load；推測為 cut endgame variant |
   | dialog_only | 2 | 純 dialog page；推測為 cut 場景 |
   | first_time_gated | 1 | flag[0x10]=0 → AI ctrl + dialog + flag=1 模式；cut tile event |
   | char_conditional | 1 | check_char_is_dead loop + branch dialog；cut conditional |
   | turn_conditional | 1 | save_metadata<0xF gate + boss kill cinematic；cut turn-gated event |
   | item_pickup | 1 | tile_event_consumed_flags 寫入 + add_item；cut pickup |
   | ai_setup | 1 | 2× state_change + data_fd2_battle_anim_phase=1；cut AI setup |

   **觀察**：cut content 主要集中在 endgame (handler idx ≥ 0x4D) 的 sentinel slots
   — 連續 5 個 sentinel (0x55..0x59) + 散布的 0x49/0x4D/0x4E。這暗示 dispatch 表
   原本預留更多 endgame variant slots，最終發行版只用了部分。中段 unref
   handler 多為 dialog / state_machine 變體，可能是同一場景的不同腳本被換掉。

   每個 unref handler 的具體 dialog page、flag write、helper call 詳見其
   Ghidra plate comment（`get_plate_comment(addr)` 取得）。
4. **idx 0x05 (chapter_event_handler_05) 在 0x34D68**：由 ch13 turn-event
   使用，是 `fd2_chapter_event_handler_07__ch13_dialog_with_state` 的 thunk。
