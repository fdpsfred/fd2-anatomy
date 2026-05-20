# FDFIELD.DAT — 章節地圖 + tile event + char spawn

每章地圖、tile event 表、char spawn 與 portrait list 都在這。
file size 243,169 bytes，99 entries (idx 0..98)。

## 檔案格式

LLLLLL archive (詳 `overview.md`)。

## idx 公式

```
chapter_id × 3 + 0  →  tile_map               (battle_tile_map: map dimensions + tile data)
chapter_id × 3 + 1  →  tile_event             (tile_event_data_table: shap_id + event hooks + char spawns)
chapter_id × 3 + 2  →  portrait_load_buffer   (chapter_portrait_load_buffer: portrait sprite list)
```

`chapter_id` 是 0-indexed (ch1 = 0, ch30 = 29)。30 章 × 3 = 90 entries 對應正章。
另外 9 個 idx (90..98) 是 endgame cinematic：

| chapter_id | FDFIELD idx | tile_map | tile_event | portrait | 用途 |
|---|---|---|---|---|---|
| 30 (extra) | 90/91/92 | 6304 B | 157 B | 194 B | endgame cinematic map 1 |
| 31 (extra) | 93/94/95 | 4004 B | 937 B | 182 B | epilogue map (`ch30_end load_chapter_battle_data(31)` staff roll) |
| 32 (extra) | 96/97/98 | 3676 B | 1171 B | 182 B | endgame cinematic map 3 |

## 30 章 idx 對照表

| Chapter | binary id | tile_map | tile_event | portrait |
|---|---|---|---|---|
| ch1  | 0  | 0  | 1  | 2  |
| ch2  | 1  | 3  | 4  | 5  |
| ch3  | 2  | 6  | 7  | 8  |
| ch4  | 3  | 9  | 10 | 11 |
| ch5  | 4  | 12 | 13 | 14 |
| ch6  | 5  | 15 | 16 | 17 |
| ch7  | 6  | 18 | 19 | 20 |
| ch8  | 7  | 21 | 22 | 23 |
| ch9  | 8  | 24 | 25 | 26 |
| ch10 | 9  | 27 | 28 | 29 |
| ch11 | 10 | 30 | 31 | 32 |
| ch12 | 11 | 33 | 34 | 35 |
| ch13 | 12 | 36 | 37 | 38 |
| ch14 | 13 | 39 | 40 | 41 |
| ch15 | 14 | 42 | 43 | 44 |
| ch16 | 15 | 45 | 46 | 47 |
| ch17 | 16 | 48 | 49 | 50 |
| ch18 | 17 | 51 | 52 | 53 |
| ch19 | 18 | 54 | 55 | 56 |
| ch20 | 19 | 57 | 58 | 59 |
| ch21 | 20 | 60 | 61 | 62 |
| ch22 | 21 | 63 | 64 | 65 |
| ch23 | 22 | 66 | 67 | 68 |
| ch24 | 23 | 69 (0x45) | 70 (0x46) | 71 (0x47) |
| ch25 | 24 | 72 | 73 | 74 |
| ch26 | 25 | 75 | 76 | 77 |
| ch27 | 26 | 78 | 79 | 80 |
| ch28 | 27 | 81 | 82 | 83 |
| ch29 | 28 | 84 | 85 | 86 |
| ch30 | 29 | 87 | 88 | 89 |

ch23_end 中段 `current_chapter_id += 1` (22 → 23) 然後直接載入 FDFIELD idx 0x45
(= ch24 tile_map)，**共用 ch24 場景**作為 ch23 第二段戰場。30 章中**唯一**一個
章內 mid-handler reload FDFIELD 的章節。

## tile_event entry layout (chapter_id × 3 + 1)

每個 tile_event entry 包含 fixed `0x83` byte header + N × `0x1A` 個 char_spawn_records：

| Offset | Size | Field | Description |
|---|---|---|---|
| `+0x00` | 1 | `shap_id` | FDSHAP idx 計算用 (×2 / ×2+1) |
| `+0x01` | 1 | `party_member_count` | 玩家方角色數 |
| `+0x02` | 1 | `char_spawn_count` (N) | char_spawn_records[] 總數 |
| `+0x03..+0x32` | 48 | `turn_event_hooks[16]` | 16 × 3 byte (turn, event_code, phase) |
| `+0x33..+0x52` | 32 | `tile_step_event_hooks[16]` | 16 × 2 byte (consequence_idx, event_type) |
| `+0x53..+0x82` | 48 | `tile_pickup_table[16]` | 16 × 3 byte pickup records |
| `+0x83..` | N × 0x1A | `char_spawn_records[N]` | per-record 26 byte char setup |

Header total = 3 + 16×3 + 16×2 + 16×3 = **131 bytes = 0x83** ✓

### 33 章 layout 統計

| Chapter | shap_id | char_count | size | turn_hooks_active |
|---|---|---|---|---|
| ch1 | 0x00 | 30 | 937 (+1 reserved) | 4 |
| ch2 | 0x01 | 40 | 1171 | 1 |
| ch3 | 0x02 | 40 | 1171 | 1 |
| ch4 | 0x03 | 40 | 1171 | 1 |
| ch5 | 0x04 | 50 | 1431 | 4 |
| ch6 | 0x05 | 40 | 1171 | 3 |
| ch7 | 0x06 | 40 | 1171 | 1 |
| ch8 | 0x07 | 60 | 1691 | 7 |
| ch9 | 0x08 | 60 | 1691 | 2 |
| ch10 | 0x09 | 60 | 1691 | 2 |
| ch11 | 0x0A | 40 | 1171 | 0 |
| ch12 | 0x0B | 60 | 1691 | 2 |
| ch13 | 0x0C | 70 | 1951 | 2 |
| ch14 | 0x0D | 70 | 1951 | 0 |
| ch15 | 0x0E | 80 | 2211 | 3 |
| ch16 | 0x0F | 60 | 1691 | 0 |
| ch17 | 0x10 | 60 | 1691 | 1 |
| ch18 | 0x11 | 70 | 1951 | 2 |
| ch19 | 0x12 | 70 | 1951 | 3 |
| ch20 | 0x13 | 70 | 1951 | 0 |
| ch21 | 0x14 | 80 | 2211 | 5 |
| ch22 | 0x15 | 70 | 1951 | 3 |
| ch23 | 0x16 | 70 | 1951 | 4 |
| ch24 | 0x17 | 70 | 1951 | 4 |
| ch25 | 0x18 | 70 | 1951 | 1 |
| ch26 | 0x19 | 70 | 1951 | 9 |
| ch27 | 0x1A | 80 | 2211 | 2 |
| ch28 | 0x1B | 60 | 1691 | 3 |
| ch29 | 0x1C | 76 | 2107 | 3 |
| ch30 | 0x1D | 70 | 1951 | 1 |
| endgame_ch30 | 0x1E | 1 | 157 | 0 |
| endgame_ch31 | 0x1F | 30 | 937 (+1 reserved) | 4 |
| endgame_ch32 | 0x20 | 30 | 1171 (+10 records) | 0 |

ch1 與 endgame_ch31 各多 1 個 reserved record (race_id=0xFF 永不被 load)。
endgame_ch32 的 char_spawn_count = 30 但實際 file payload 含 40 records (10 個
額外 = 260 bytes)。

**Loader 行為**：`load_chapter_portraits_and_dump_tmp @ 0x10b4e` 的核心 loop
用 `portrait_cache_alloc_offset = char_spawn_count` (header byte +2) 當迭代
上限：

```
for (i = 0; i < char_spawn_count; i++) {
  if (records[i].race_id == target_race_id) init_runtime_char_for_battle(i, ...)
}
```

`char_spawn_count` 絕對控制讀取範圍 — 之後的 record bytes 完全不被存取。
endgame_ch32 的 10 個額外 records 是 dead payload（cut content / reserved
expansion / 編譯殘留），永遠不被 loader 觸碰，binary 行為上等於不存在。

全 33 章每章 16/16 tile_step_event_hooks active（**0 sentinel**）；16/16 pickup
entries active（**0 sentinel**）；turn_event_hooks variable (78 active 跨 33 章，
ch26=9 最多，多章 0)。全 char_spawn_records 共 1887 個。

## char_spawn_record layout (0x1A bytes)

per `init_runtime_char_for_battle @ 0x10C50`：

```c
struct char_spawn_record {
    uint8_t bTeam;              // +0x00 (0=team0/enemy, 1=NPC, 2=player)
    uint8_t char_id;            // +0x01 (< 0x44 = player class via char_base/growth;
                                //        ≥ 0x44 = data_fd2_battle_enemy_data_table[id-0x44])
    uint8_t ai_target_id;       // +0x02 (initial AI target char_idx)
    uint8_t _pad03;             // +0x03 (always observed = 1 in ch1)
    uint8_t level;              // +0x04
    uint8_t inv_slot_0_special; // +0x05 (若 0xFF, slot 0 takes item from +0x06,
                                //        slot 1 empty; else slot 0 = +0x05)
    uint8_t inv_slot_1;         // +0x06
    uint8_t inv_slot_2;         // +0x07 (loop; 0xFF = empty slot)
    uint8_t inv_slot_3;         // +0x08
    uint8_t inv_slot_4;         // +0x09
    uint8_t inv_slot_5;         // +0x0A
    uint8_t inv_slot_6;         // +0x0B
    uint8_t inv_slot_7;         // +0x0C
    uint8_t spell_bitmap[4];    // +0x0D..+0x10 (copied to runtime_char.pSpells_known_bitmap[0..3])
    uint8_t ai_class_flags;     // +0x11 (low 4 bits = ai_class 0..11; high 4 = flags)
    uint8_t ai_aux;             // +0x12 (e.g., target char_idx for class 1/9/11)
    uint8_t ai_target_pos;      // +0x13 (e.g., destination tile for class 7 charge)
    uint8_t _pad14;             // +0x14 (always 0 observed)
    uint8_t race_id;            // +0x15 (portrait_set filter — 見下方)
    uint8_t pickup_kind;        // +0x16 (-> runtime_char.pCombat_aux_block[0x0A])
    uint8_t pickup_param_lo;    // +0x17 (-> [0x0B])
    uint8_t pickup_param_hi;    // +0x18 (-> [0x0C])
    uint8_t _pad19;             // +0x19 (always 0 observed)
};
```

### race_id (+0x15)：conditional spawn 機制的關鍵

`load_chapter_portraits_and_dump_tmp(target_race_id)` 在 chapter init / turn-event
handler 內被呼叫時，loop 全 records 篩選 `record.race_id == target_race_id`，
匹配的 records 才呼 `init_runtime_char_for_battle` 載入到 battle map。

- chapter init 不同階段 (prologue/intro/start) 可呼叫不同 race_id 載 cinematic 角色
- turn-event handler 可在特定 turn 觸發新 race 載入 = 援軍 / 轉場
- ch1「turn 3 哈諾加入」 = handler_00 呼 `load_chapter_portraits_and_dump_tmp(3)`
  載 race=3 records (含 char_id 0x01 = 哈諾)

## turn-event hook table (offset +0x03..+0x32)

16 entries × 3 bytes：

```c
struct turn_event_hook {
    uint8_t turn;        // matches save_metadata_block (1-based player turn)
    uint8_t event_code;  // index into data_fd2_battle_ai_post_action_consequence_table @ 0x51B91
    uint8_t phase;       // 0=enemy_turn_intro, 1=end_of_player_turn,
                         // 2=new_player_turn_intro
};
```

Sentinel：`(turn=0xFF, event_code=0xFF, phase=0)`。30 章共 406 個 sentinel slot。
ch9 / ch27 / ch28 / ch29 / ch30 有 `(turn=0xFF, event_code≠0xFF)` 形態，是動態
啟動候選 (詳 `program_info/chapter_event_dispatch.md`)。

## tile-step-event hook (offset +0x33..+0x52)

16 entries × 2 bytes：

```c
struct tile_step_event_hook {
    uint8_t consequence_idx;   // event_code 索引 data_fd2_battle_ai_post_action_consequence_table
    uint8_t event_type;        // 觸發 context (0/1/2 對應 post-walk/post-attack/etc.)
};
```

機制：char 移動到一個有 `tile_event_id != 0` 的 tile 時，post-action 流程呼
`check_tile_event_post_action`。詳 `program_info/chapter_event_dispatch.md`。

## tile_pickup_table (offset +0x53..+0x82)

16 entries × 3 bytes (pickup_kind, pickup_param 2-byte LE)。寶箱、地圖物品由此
觸發。

## 工具

- Event 解碼：`tools/decoders/fdfield_event_decoder.py`
- char_spawn 解碼：`tools/decoders/fdfield_char_spawn_decoder.py`
