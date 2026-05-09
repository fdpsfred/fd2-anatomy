# program_info/

對 FD2 遊戲程式系統的解析。

## 檔案

- `overview.md` — 整體架構：FD2.LE binary segments、entry chain、12 systems 總覽、
  runtime_char struct (80 bytes) 完整 layout、`.object3` 資料表清單、關鍵遊戲機制
- `call_graph.md` + `call_graph.json` + `call_graph.dot` — FD2.LE 1343 個
  function 的全程式 call graph，nodes 含 category（ail 283 / crt 324 / game 736）
  與 thunk flag，edges 4262 條
- `lifecycle.md` — Watcom CRT layer：47 個 Watcom 公開符號（malloc/fread/...）+
  178 個自寫 `crt_*` wrapper + 256 個 `crt_dpmi_int_NN` 軟中斷 dispatch table +
  74 個 `align_nop_*` Watcom alignment fill +  ~226 個 `crt_helper_*` /
  `crt_*_helper_<addr>` vendor library helper + entry/cutscene glue + ending sequence
- `resource.md` — `load_dat_resource @ 0x111BA` 與 32 個 caller 的歸屬原則
- `save_load.md` — FD2.SAV 存讀寫 8 個 helper、4-slot 選擇器、checksum/加密用途
- `field_map.md` — 30 章 init/end handler、4 張 chapter jump table、
  `per_chapter_post_action_handler` 5 大模式、`save_metadata_block` turn counter、
  跨章機制總覽
- `battle.md` — fd2_main + game_main_loop 戰鬥架構、damage pipeline、enemy AI
  主架構、AI 評分三路 (物理/法術/道具)、12 種 AI behavior class semantic
- `ui_menu.md` — `game_main_loop` per-frame scancode 分派、3 層 cursor 座標、
  `player_action_menu_loop` UI orchestrator、`field_command_menu_loop` modal、
  status screen 動畫
- `text_dialog.md` — `display_dialog_scene` VM、portrait 200 KB 快取、
  字模 2bpp 渲染、17-tile 9-slice 對話框、speaker mirror blit、▼ 按鍵動畫
- `animation.md` — FIGANI byte-stream pose-based 動畫、per-spell 視覺三段管線、
  panel/dialog slide、死亡/爆炸動畫、召喚 palette-cycle FX
- `graphics.md` — DOS mode13h 320×200 + RLE blit、tile_attribute_flags、
  palette FX、composite_battle_frame finalizer
- `audio.md` — Miles AIL 共 287 個 function（103 個 `AIL_*` 公開 API + 184 個
  `AIL_internal_*`）；其中 9 個從 `crt_*` 反向 reclassify 為 AIL，36 個 BFS
  不可達 orphan 加 plate 標 sub-case；另保留 10 個 Watcom CRT primitive
  （DPMI region/size lock/unlock/alloc/free、filesize_path、abort helper、
  get_eflags）；自寫 BGM dispatcher (`set_bgm_track_with_fade @ 0x25977`)、
  `play_sfx_with_handle`、driver/patch 檔
- `input.md` — BIOS keyboard area direct access、scancode 表、
  `wait_for_input_with_idle` poll loop、為何不用 INT 16h
- `table_accessor.md` — 5 個 `get_*_entry` helper (`.object1` 末端)
- `chapter_event_dispatch.md` — FDFIELD turn-event hook 機制、
  `ai_post_action_consequence_table @ 0x51B91` 90-entry handler 對照表、
  動態 turn-event 啟動機制 (ch27..30)、tile-step-event hooks
- `calling_convention.md` — Watcom 32-bit cc 的 ABI 規則（`__watcall` /
  `__cdecl` / `__stdcall`）、判斷訊號 (caller ADD ESP / RET N /
  EAX/EDX/EBX/ECX 設定)、pinned 真實 callee-cleanup function、param 數量
  推論公式、最終 cc 分布（`__cdecl` 957 / `__watcall` 42 / `__stdcall` 1）
- `emit_pipeline_spec.md` — emit C source 的 pool 路由規則 + 25 個 fall-through
  pattern 的強制處理規則（SHARED EPILOGUE / SHARED BODY / HEADER-ONLY ENTRY /
  DEAD FALL-THROUGH / DATA TABLE FRAGMENT / STATE-MACHINE INIT-ENTRY），
  以及三層 binary 等價不變式（specification-exact / functionally-exact /
  byte-exact）

## chapters/ 子資料夾

每章 init/end handler 的函數呼叫流程、char_id 初始化序列、cutscene events、
post_action handler、FDFIELD event script。

詳 `chapters/_index.md`。
