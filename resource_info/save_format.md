# FD2.SAV 存檔格式

存檔大小固定 22987 bytes（`0x59CB`）。整個檔案在寫入前先算 checksum、再做一次
XOR involution 加擾，讀入時反向解擾並比對 checksum；兩者只是完整性檢查與
cheat deterrent，**非 cryptographic 安全**。checksum 與加解密的演算法本體
（純加總 byte-sum、`0xA5` seed 的 keystream）見 `program_info/save.md`，本檔只記
byte-level 位置。

檔案分兩個獨立區段：前段 `0x0000..0x312A` 是「CONTINUE／遊戲中存檔」用的 live-state
序列化區（存的是當前這一局的完整狀態），後段 `0x312B` 起是 4 個獨立的存檔 slot。
兩段的寫入者不同：遊戲中選單的「存檔」寫 live-state 區
（`fd2_field_menu_status_save_load_quit_dispatch @ 0x19DF7`），章節選單的 4-slot
存檔寫 slot 區（`fd2_save_current_state_to_slot @ 0x30012`）。載入端
`fd2_load_save_and_init_engine @ 0x10010` 把 live-state 區反序列化回各 global，是
還原整個引擎狀態的路徑。

## 整體 layout

```
offset     size      內容
+0x0000    0x312B    live-state header (下表)
+0x312B    0xA28     slot 0 (2600 bytes)
+0x3B53    0xA28     slot 1
+0x457B    0xA28     slot 2
+0x4FA3    0xA28     slot 3
+0x59C7    u32       checksum (EOF 之前 4 bytes)
+0x59CB    EOF
```

實際大小：`0x312B + 4 × 0xA28 = 0x312B + 0x28A0 = 0x59CB` ✓
（來源 `save.c:278` malloc(0x59cb)、`save.c:293` slot_base = pBuf + idx*0xA28 + 0x312B）

`+0x59C7` 的 checksum u32 實際落在 slot 3 尾端的保留 padding 內（slot 3 base `0x4FA3`、
跨 `0x4FA3..0x59CA`，checksum 對應 slot-local `+0xA24..+0xA27`），並非 slot 之後的獨立區段。

## live-state header 區（0x0000 .. 0x312A）

CONTINUE／遊戲中存檔的完整狀態序列化。欄位對應以載入端
`fd2_load_save_and_init_engine @ 0x10010` 與遊戲中存檔端
`fd2_field_menu_status_save_load_quit_dispatch @ 0x19DF7` 兩邊的 memmove／逐 byte
還原互相印證，每個位址都用 Ghidra 語意 global 名。

```
offset     size      global / 內容
+0x0000    0x8A3     data_fd2_tile_event_data_table_ptr 內容 (tile-event 資料表)
+0x08A3    0xA00     data_fd2_shared_menu_party_roster_buffer_ptr 內容 (選單隊伍 roster)
+0x12A3    0x1E00    data_fd2_battle_runtime_char_array_ptr (runtime 角色陣列;
                     實寫 data_fd2_battle_party_member_count × 0x50，容量 96 entry)
+0x30A3    0x20      data_fd2_field_map_tile_event_consumed_flags_ptr 內容 (事件已觸發旗標)
+0x30C3    u8        data_fd2_battle_turn_counter
+0x30C4    u8        data_fd2_battle_party_member_count
+0x30C5    u8        data_fd2_chapter_current_chapter_id
+0x30C6    u8        data_fd2_battle_view_window_origin_x
+0x30C7    u8        data_fd2_battle_view_window_origin_y
+0x30C8    u8        data_fd2_battle_cursor_world_x
+0x30C9    u8        data_fd2_battle_cursor_world_y
+0x30CA    u8        data_fd2_battle_cursor_screen_x
+0x30CB    u8        data_fd2_battle_cursor_screen_y
+0x30CC    u8        data_fd2_shared_menu_party_member_count
+0x30CD    u32       data_fd2_shared_party_total_gold
+0x30D1    u8        data_fd2_ui_game_speed_flag
+0x30D2    u8        data_fd2_ui_terrain_hud_user_enabled
+0x30D3    u8        data_fd2_audio_bgm_enabled_flag
+0x30D4    u8        data_fd2_audio_sfx_enabled_flag
+0x30D5    0x56      unused / reserved padding (86 bytes，存讀兩端皆不觸碰，
                     直到 slot 區起點 0x312B)
```

`+0x08A3` 這段與 slot 內的 `[0..0x9FF]` 是同一份 menu 隊伍 roster
（`data_fd2_shared_menu_party_roster_buffer_ptr`），不是地圖／地形資料。

## Slot layout（0xA28 bytes per slot）

4 個 slot 各 `0xA28` bytes，`slot_base = pBuf + slot_idx * 0xA28 + 0x312B`
（`save.c:293`）。空 slot 以 `+0xA00 == 0xFF` 標記。

```
offset     size      global / 內容
+0x000     0xA00     data_fd2_shared_menu_party_roster_buffer_ptr 內容
                     (選單隊伍 roster template;memmove 存入/取出)
+0xA00     u8        data_fd2_chapter_current_chapter_id
+0xA01     u8        data_fd2_shared_menu_party_member_count
+0xA02     u32       data_fd2_shared_party_total_gold
+0xA06     u8        data_fd2_ui_terrain_hud_user_enabled
+0xA07     u8        data_fd2_ui_game_speed_flag
+0xA08     u8        data_fd2_audio_bgm_enabled_flag
+0xA09     u8        data_fd2_audio_sfx_enabled_flag
+0xA0A     0x1E      unused / reserved padding (30 bytes;writer 只寫到 +0xA09)
```

slot 前 `0xA00` bytes 是選單隊伍 roster template，不是地圖／地形快照；scalar 欄位只到
`+0xA09`，其後 `0x1E`（30）bytes 為未使用的保留 padding
（`fd2_save_current_state_to_slot` 寫入序列見 `save.c:294..303`，載入序列見
`fd2_load_state_from_selected_slot @ 0x301F4` `save.c:393..402`）。

## 加密與 checksum

checksum 欄位在 `+0x59C7`（u32，EOF 之前 4 bytes），涵蓋範圍是 `buf[0 .. 0x59C6]`
（即 `0x59CB - 4`）；整檔加解密涵蓋全部 `0x59CB` bytes。演算法本體
（`fd2_save_compute_checksum @ 0x4DBB9` 純加總 byte-sum、`fd2_save_crypt_buffer @ 0x4DBD8`
XOR involution 與 `0xA5` seed keystream）見 `program_info/save.md`。

寫入流程：填好 buffer → 算 checksum 寫進 `+0x59C7` → 整檔加擾 → fwrite。
讀入流程：fread → 整檔解擾 → 重算 checksum 與 `+0x59C7` 比對，不符則顯示錯誤對話框。

## 與 program 端的對應

完整 fopen call sites、9 處字串 occurrence 對應的 6 個 function、以及 checksum／crypt
演算法本體見 `program_info/save.md`。live-state 與 slot 兩條存讀寫路徑的程式行為也在該檔。
