# program_info/

對 FD2 遊戲程式系統的解析。

## 檔案

- `overview.md` — 整體架構：FD2.LE binary segments、entry chain、12 systems 總覽、
  runtime_char struct (80 bytes) 完整 layout、`.object3` 資料表清單、關鍵遊戲機制
- `resource.md` — `fd2_load_dat_resource @ 0x111BA` 與 32 個 caller 的歸屬原則
- `save_load.md` — FD2.SAV 存讀寫 8 個 helper、4-slot 選擇器、checksum/加密用途
- `field_map.md` — 30 章 init/end handler、4 張 chapter jump table、
  `data_fd2_chapter_post_action_handler_table` 5 大模式、`save_metadata_block` turn counter、
  跨章機制總覽
- `battle.md` — main + fd2_game_main_loop 戰鬥架構、damage pipeline、enemy AI
  主架構、AI 評分三路 (物理/法術/道具)、12 種 AI behavior class semantic
- `ui_menu.md` — `fd2_game_main_loop` per-frame scancode 分派、3 層 cursor 座標、
  `fd2_player_action_menu_loop` UI orchestrator、`fd2_field_command_menu_loop` modal、
  status screen 動畫
- `text_dialog.md` — `fd2_display_dialog_scene` VM、portrait 200 KB 快取、
  字模 2bpp 渲染、17-tile 9-slice 對話框、speaker mirror blit、▼ 按鍵動畫
- `animation.md` — FIGANI byte-stream pose-based 動畫、per-spell 視覺三段管線、
  panel/dialog slide、死亡/爆炸動畫、召喚 palette-cycle FX
- `graphics.md` — DOS mode13h 320×200 + RLE blit、tile_attribute_flags、
  palette FX、fd2_composite_battle_frame finalizer
- `audio.md` — FD2 game-side BGM dispatcher (`fd2_set_bgm_track_with_fade @ 0x25977`) +
  SFX trigger (`fd2_play_sfx_with_handle @ 0x25A96`)；Miles AIL vendor library
  inventory / extraction prep / driver/patch 檔詳見 `rebuild_info/ail/`
- `input.md` — BIOS keyboard area direct access、scancode 表、
  `fd2_wait_for_input_with_idle` poll loop、為何不用 INT 16h
- `table_accessor.md` — 5 個 `get_*_entry` helper (`.object1` 末端)
- `chapter_event_dispatch.md` — FDFIELD turn-event hook 機制、
  `data_fd2_battle_ai_post_action_consequence_table @ 0x51B91` 90-entry handler 對照表、
  動態 turn-event 啟動機制 (ch27..30)、tile-step-event hooks

## chapters/ 子資料夾

每章 init/end handler 的函數呼叫流程、char_id 初始化序列、cutscene events、
post_action handler、FDFIELD event script。

詳 `chapters/_index.md`。

## 相關文件

整體 entry chain / startup / exit、CRT layer、pool routing、call graph、calling convention、
等價鐵則與 pool 分類詳見 `rebuild_info/equivalence/` 與 `rebuild_info/crt/`。
