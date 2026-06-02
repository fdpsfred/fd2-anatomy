# FDOTHER.DAT — UI / portrait / SFX / cinematic 雜項資源

最大、最雜的資源檔。outer 103 entries 內含 29 個 nested LLLLLL sub-archive，
真實 distinct resource 約 240。
file size 3,382,481 bytes，103 outer entries (idx 0..102) + 166 sub-entries。

## 檔案格式

LLLLLL archive (outer)。29 個 outer entries 自身又是 LLLLLL sub-archive，
sub-entries 各自獨立索引。

## 啟動載入靜態 (fd2_main 序列)

`fd2_main @ 0x25BF4` 啟動時依序載入 8 個 FDOTHER + 1 個 FDTXT entry：

| idx | 全域變數 | 用途 | 大小 |
|---|---|---|---|
| 0x01 | `runtime_battle_state_at_53a4d @ 0x53A4D` | battle tile sprite table | 33,415 |
| 0x02 | `menu_dialog_state_handle @ 0x53A89` | menu dialog state | 37,680 |
| 0x03 | `tile_anim_table_base @ 0x53A6D` | tile 動畫表 (LMI1 magic) | 5,990 |
| **0x04** | `chinese_font_sheet @ 0x53A75` | **1bpp 中文字模 (1824 glyphs × 32 bytes)** | 58,368 |
| 0x05 | `ui_and_anim_sprite_sheet @ 0x53A81` | UI / 動畫 sprite sheet (LMI1 magic) | 44,181 |
| **0x06** | `portrait_sheet @ 0x53AD1` | portrait sheet (LMI1 magic) | 33,415 |
| **0x1F** | `data_fd2_audio_fdother_sfx_bank_buf_ptr @ 0x53EEC` | nested archive (13 sub-entries) UI sprite + sfx | 31,771 |

`chinese_font_sheet` 是 **1bpp** (58368 ÷ 1824 ÷ 32 = 1.0)。
`fd2_blit_glyph_2bpp_with_outline @ 0x4EA2A` 命名指 **output buffer** 是 2bpp
(fill + outline 兩 channel)，input glyph 是 1bpp。

## 章節載入靜態 (chapter_id-dispatched)

| idx | caller | 用途 |
|---|---|---|
| 0x09 | `fd2_animate_party_addition_with_appear_effect` | 角色加入動畫 |
| 0x0A | `fd2_chapter_transition_menu` | chapter_transition_menu_panel_buffer |
| 0x0D | `fd2_chapter_transition_menu` / `fd2_main_menu_continue_dispatcher` / `fd2_run_chapter_intro_menu_typeB` | chapter intro sprite atlas |
| 0x0E | `fd2_run_chapter_intro_menu_typeC` | chapter intro typeC sprites |
| 0x22 | `fd2_play_chapter_intro_sprite_slideshow` | chapter intro slideshow |
| 0x2A | `fd2_load_chapter_background_layers` | chapter background |
| 0x2D | `fd2_chapter_event_handler_3d__ch26_pickup` | ch26 pickup 動畫 |
| 0x4F | `fd2_play_chapter_clear_fanfare` | chapter clear fanfare |
| 0x58 | `fd2_chapter_25_init` | earthquake_sfx (nested archive 2 sub-entries) |

## Cinematic / Ending 序列靜態

| idx | caller | 用途 |
|---|---|---|
| 0x07 | `fd2_play_ending_and_record_clear` | nested archive 7 sub-entries — ending sprite group |
| 0x08 | `fd2_play_ending_and_record_clear` | ending sprite group |
| 0x36 | `fd2_play_game_ending_cinematic` | 263 KB RLE 320×200 cinematic image |
| 0x38 | `fd2_play_final_chapter_30_ending` | final chapter 30 ending image |
| 0x39, 0x3A, 0x3B, 0x3C | `fd2_play_game_ending_cinematic` | game ending cinematic 4 連續 idx |
| 0x4A, 0x4C | `fd2_play_ending_and_record_clear` | ending sequence images |
| 0x4D | `fd2_play_ending_and_record_clear` | nested archive 4 sub-entries — ending image bank |
| 0x4E | `fd2_play_ani_file_animation_sequence` | nested archive 1 sub-entry — ANI 配套 SFX |
| 0x63 | `fd2_play_ending_and_record_clear` | ending text/banner image |
| **0x65** | `fd2_display_cinematic_image_with_fade` (×4) + `fd2_play_ending_and_record_clear` | VGA palette (768 bytes = 256 × 3 RGB DAC) |
| 0x66 | `fd2_play_ending_and_record_clear` | ending image |

## SFX / Animation 群組靜態

| idx | caller | 用途 |
|---|---|---|
| 0x40 | `fd2_maybe_load_speed_mode_overlay` | nested archive 6 sub-entries — speed mode overlay |
| 0x50 | `fd2_load_status_effect_sfx` | nested archive 16 sub-entries — status effect SFX bank |
| 0x51 | `fd2_animate_warp_teleport_char` | nested archive 2 sub-entries — warp teleport 動畫 |
| 0x5F | `fd2_animate_party_addition_with_appear_effect` | nested archive 1 sub-entry |

## Dynamic-domain 公式

### chapter-id-dispatched (`fd2_load_chapter_background_layers`)

`fd2_load_chapter_background_layers` 內部用 `CMP [current_chapter_id], 0xNN` 派發
FDOTHER bg image idx：

| idx range | dispatch 條件 |
|---|---|
| 0x0B | chapter_id 0 (ch1) - heuristic |
| 0x10..0x1E | chapter_id 0x10..0x1E mapping (各章 BG 變體) |
| 0x20, 0x23, 0x24 | chapter-state-dependent BG variant |
| 0x27, 0x28, 0x2E, 0x2F | chapter-state BG |
| 0x37 | `fd2_load_chapter_background_layers` explicit static |
| 0x64 | chapter ending state BG |

### spell_id derived (`fd2_execute_summon_spell_cast`)

`fd2_load_dat_resource(... "FDOTHER.DAT", NULL, spell_id + 0x21)` 對 summon spells
spell_id ∈ {0x20, 0x21, 0x22, 0x23} → FDOTHER idx **0x41 / 0x42 / 0x43 / 0x44**。

### main_iter loop (`fd2_play_ending_and_record_clear`)

ending 序列 loop `for(main_iter=0..8) load("FDOTHER", main_iter+0x45)` →
FDOTHER idx **0x45..0x4D** (9 entries 連續 image sequence)。

## 21 個 confirmed dead idx

binary 內 immediate value **從未** 出現在 `fd2_load_dat_resource` 任何 callsite 50
instruction 範圍內：

| idx | total uses elsewhere | classification |
|---|---|---|
| 0x25 | 18 | unrelated literals (loop counter) |
| 0x26 | 10 | unrelated |
| 0x29 | 6 | unrelated |
| 0x2B | 19 | unrelated |
| 0x2C | 61 | unrelated |
| 0x31 | 19 | nested archive (7 sub-entries), no caller |
| 0x33 | 3 | nested archive (5 sub-entries), no caller |
| 0x34 | 28 | nested archive (6 sub-entries), no caller |
| 0x35 | 5 | nested archive (4 sub-entries), no caller |
| 0x3D | 8 | unrelated |
| 0x3E | 11 | unrelated |
| 0x52 | 16 | nested archive (2 sub-entries), no caller |
| 0x53 | 12 | nested archive (4 sub-entries), no caller |
| 0x55 | 4 | nested archive (2 sub-entries), no caller |
| 0x56 | 21 | nested archive (2 sub-entries), no caller |
| 0x57 | 1 | nested archive (4 sub-entries), no caller |
| 0x5B | 6 | nested archive (3 sub-entries), no caller |
| 0x5D | 7 | nested archive (2 sub-entries), no caller |
| 0x60 | 9 | unrelated |
| 0x61 | 6 | unrelated |
| 0x62 | 3 | unrelated |

判定：cut content / 編譯殘留 / 開發期 placeholder slot。

## 完整分類

| 分類 | 計數 |
|---|---|
| documented_static | 38 |
| documented_dynamic_recovered_via_binary_immediate_search | 33 |
| documented_dynamic_domain (formula explicit) | 11 |
| confirmed_dead_with_binary_no_ref_proof | 21 |
| **TOTAL** | **103** |

## 29 個 nested sub-archive

| outer idx | size (bytes) | sub-entries | 用途 |
|---|---|---|---|
| 0x07 | 23377 | 7 | ending sprite (`fd2_play_ending_and_record_clear`) |
| 0x0C | 51759 | 28 | dynamic |
| 0x1F | 31771 | 13 | UI sprite + sfx (`fd2_main` 啟動) |
| 0x30 | 24183 | 6 | dynamic |
| 0x31 | 27871 | 7 | confirmed_dead |
| 0x32 | 31429 | 5 | dynamic |
| 0x33 | 28106 | 5 | confirmed_dead |
| 0x34 | 26164 | 6 | confirmed_dead |
| 0x35 | 19394 | 4 | confirmed_dead |
| 0x3F | 60972 | 30 | dynamic |
| 0x40 | 18791 | 6 | speed_mode_overlay |
| 0x4D | 52031 | 4 | play_ending |
| 0x4E | 6492 | 1 | ANI 配套 |
| 0x50 | 116165 | 16 | status_effect_sfx |
| 0x51 | 18710 | 2 | warp_teleport |
| 0x52 | 20003 | 2 | confirmed_dead |
| 0x53 | 33848 | 4 | confirmed_dead |
| 0x54 | 24389 | 3 | dynamic |
| 0x55 | 13959 | 2 | confirmed_dead |
| 0x56 | 11670 | 2 | confirmed_dead |
| 0x57 | 20143 | 4 | confirmed_dead |
| 0x58 | 14953 | 2 | chapter_25_init earthquake |
| 0x59 | 15308 | 3 | dynamic |
| 0x5A | 26591 | 3 | dynamic |
| 0x5B | 33581 | 3 | confirmed_dead |
| 0x5C | 20247 | 2 | dynamic |
| 0x5D | 20247 | 2 | confirmed_dead |
| 0x5E | 33581 | 3 | dynamic |
| 0x5F | 10458 | 1 | fd2_animate_party_addition_with_appear_effect |

## 工具

- 解碼：`tools/decoders/fdother_decoder.py`
