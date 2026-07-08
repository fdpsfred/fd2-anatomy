# src/ 模組地圖

`src/` 依系統切成 15 個模組資料夾。這份表把每個模組對回 Ghidra 內的符號體系與對應的
`program_info/` 解析文件，是新 session 的導航件，也是 program_info 各文件的骨幹依據。代表符號皆
可用 Ghidra `search_functions_enhanced` / `list_globals` 即時查證；位址是 FD2.LE 的 link-time
vaddr，會隨 Ghidra 重分析穩定不變，但若要引用具體數值請即時查、不從本表抄。

## 命名規則

| 規則 | 說明 |
|---|---|
| 8.3 檔名 | Watcom 9.5a 不支援長檔名，所有 `.c` / `.h` basename ≤ 8 字元、副檔名 ≤ 3 字元。obj 命名（leaf stem 去底線取 8）見 `tools/fd2_build/_index.md`。 |
| game-logic 前綴 | 遊戲邏輯 function 一律 `fd2_` 前綴。唯一豁免是 C 進入點 `main`（`src/life/main.c`）—— CRT `cmain386` 契約要求進入點叫 `main`。 |
| global data 前綴 | 遊戲 global data symbol 一律 `data_fd2_` 前綴（Ghidra 與 C 端 byte-identical）；battle 核心表用 `data_fd2_battle_*`，同檔同類表命名一致。 |
| vendor / CRT 前綴 | `crt_` / `crt_equivalent_` = Watcom CRT 層；`AIL_` = Miles AIL vendor。這兩類不套用 `fd2_` / `data_fd2_` 慣例。 |
| pool 分類 | 每個 symbol 歸 ail / crt / fd2 / binary_artifact 四 pool 之一（source of truth = `tools/program_analysis/build_call_graph.py` 的 `categorise()`）；分類法與 binary_artifact 的 NOP/padding 事實見 `equivalence/pool_classification.md`。 |

## 模組對照

| 模組 | `.c` 檔 | 代表 Ghidra 符號 | 對應 program_info |
|---|---|---|---|
| `anim` | anicine, anicombt, anidec, aniend, anispell, anisummn, aniui, aniwalk | `fd2_walk_path_animation_loop` @0x13488、`fd2_animate_spell_projectile_paths` @0x1df58、`fd2_animate_bg_zoom_transition_in` @0x29c90 | `animation.md` |
| `audio` | audio | `fd2_set_bgm_track_with_fade` @0x25977、`fd2_play_sfx_with_handle` @0x25a96 | `audio.md`（game 端）；Miles AIL vendor 見 `ail/` |
| `battle` | battle, btl_ai, btl_aisc, btl_aitg, btl_init, btl_turn | `fd2_game_main_loop` @0x117e7、`fd2_calculate_combat_hit_outcome` @0x29f72 | `battle.md` |
| `crt` | crt | `crt_equivalent_get_eflags` @0x3ed58、`crt_equivalent_get_eflags_thunk` @0x37f86 | 無（CRT 層，見 `crt/symbol_inventory.md`）|
| `dialog` | dialog | `fd2_display_dialog_scene` @0x15f84 | `text_dialog.md` |
| `field` | chinit, chend1, chend2, chevt1, chevt2, chpost, chtrans | `fd2_chapter_NN_post_action`（各章）、章節 init/end handler | `field_map.md` + `chapter_event_dispatch.md` + `chapters/` |
| `gfx` | blitspr, blittile, palette, rndmenu, rndscene, rndstat | `fd2_composite_battle_frame` @0x11cac、`fd2_rle_blit_sprite` @0x4e63d、`fd2_apply_palette_remap_run` @0x4db9c | `graphics.md` |
| `input` | input | `fd2_wait_for_input_with_idle` @0x11aa8 | `input.md` |
| `life` | main | `main`（C 進入點，`fd2_` 前綴豁免）| `overview.md`（entry chain / startup）|
| `rsrc` | rsrc | `fd2_load_dat_resource` @0x111ba | `resource.md` |
| `save` | save | `fd2_save_current_state_to_slot` @0x30012、`fd2_save_crypt_buffer` @0x4dbd8、`fd2_save_compute_checksum` @0x4dbb9 | `save_load.md` |
| `spell` | spell, spellcin, spelleff, spellsel | `fd2_cast_*` 系列（如 `fd2_cast_status_inflict_spell` @0x22d1b、`fd2_cast_screen_wide_spell_with_fade` @0x24618）| `battle.md`（法術效果/傷害）+ `animation.md`（法術視覺三段管線）|
| `table` | table, anitab, audtab, btltab, btltab2, btltab3, chtab, chtab2, chtab3, dlgtab, gfxtab, strtab, uitab, orphan | `fd2_get_*_entry` accessor（如 `fd2_get_item_effect_entry` @0x4e56c、`fd2_get_spell_effect_entry` @0x4e516）；資料表如 `data_fd2_battle_item_effect_table`、`data_fd2_battle_spell_effect_table` | `table_accessor.md`；表的數值內容見 `assets/` |
| `ui_menu` | menu, menucfg, menufld, cursor, chintro, promote, shop, status | `fd2_player_action_menu_loop` @0x18890、`fd2_cursor_move_up/down/left/right` @0x11b48.. | `ui_menu.md` |
| `util` | dpmi, misc, noop, pathfnd | `fd2_dpmi_*`（如 `fd2_dpmi_lock_size` @0x36316）、`fd2_pathfind_*`（如 `fd2_pathfind_to_destination` @0x4e1a6）| dpmi 見 `link/` + `ail/`（AIL ISR 記憶體鎖定）；pathfind 屬 field 移動子系統 |

## 資料表落點

`table` 模組定義遊戲的大型資料表（`data_fd2_*` 符號），分布在 DGROUP 與 `.object3`（FAR_DATA）；對應的
accessor helper（`fd2_get_*_entry`）是 code，落在 `.object1`（`_TEXT`）末端。表的 layout 與位址分布見
`link/le_layout.md`，表的數值內容（道具/法術/敵人/職業…）見 `assets/`。
