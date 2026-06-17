#ifndef GLOBALS_H
#define GLOBALS_H

#include "types.h"
#include <i86.h>   /* union REGS for the shared int386 INT-call scratch below */

/*
 * FD2 global variable extern declarations.
 * Organized by system. Addresses in comments are LE runtime addresses.
 * Expand this file during emission as new globals are encountered.
 */

/* ---- runtime char & party ---- */
extern uint32 data_fd2_runtime_battle_state_ptr;                        /* 0x53A4D */
extern uint8  data_fd2_chapter_chapter_init_done_flag;                  /* 0x53A44 */
extern runtime_char *data_fd2_battle_runtime_char_array_ptr;            /* 0x53A45 */
extern uint32 data_fd2_battle_party_member_count;                       /* 0x53BEB */
extern uint32 data_fd2_battle_current_active_char_idx;                  /* 0x53AE9 */

/* ---- chapter state ---- */
extern uint32 data_fd2_chapter_current_chapter_id;                      /* 0x53C03 */
extern uint32 data_fd2_chapter_event_or_battle_end_code;                /* 0x53ECC */
extern uint32 data_fd2_chapter_cutscene_event_state;                    /* 0x53AFB */

/* ch20 end-scene char position tables (private to fd2_chapter_20_end) */
extern const uint8 data_fd2_chapter_ch20_end_scene1_char_pos_x_table[16];  /* 0x521F6 */
extern const uint8 data_fd2_chapter_ch20_end_scene1_char_pos_y_table[16];  /* 0x52206 */
extern const uint8 data_fd2_chapter_ch20_end_scene2_char_pos_x_table[9];   /* 0x52216 */
extern const uint8 data_fd2_chapter_ch20_end_scene2_char_pos_y_table[9];   /* 0x5221F */

/* ch21 end-scene char tables (private to fd2_chapter_21_end); 7 chars x 4B
 * minus 1B = 25-byte stride, copied onto stack before the camera/intro setup */
extern const uint8 data_fd2_chapter_ch21_end_scene_char_pos_x_table[25];   /* 0x52228 */
extern const uint8 data_fd2_chapter_ch21_end_scene_char_pos_y_table[25];   /* 0x52241 */
extern const uint8 data_fd2_chapter_ch21_end_scene_char_facing_table[25];  /* 0x5225A */

/* ch22 end-scene char tables (private to fd2_chapter_22_end); 16-byte tables
 * (one byte per char slot, 4 chars placed), copied onto stack as 4 dwords
 * before fd2_setup_chars_and_camera_for_intro indexes them by char slot */
extern const uint8 data_fd2_chapter_ch22_end_scene_char_pos_x_table[16];   /* 0x52273 */
extern const uint8 data_fd2_chapter_ch22_end_scene_char_pos_y_table[16];   /* 0x52283 */
extern const uint8 data_fd2_chapter_ch22_end_scene_char_facing_table[16];  /* 0x52293 */

/* ch23 end-scene char tables (private to fd2_chapter_23_end); 17-byte extent
 * (one byte per char slot, 5 chars placed), copied onto stack as 4 dwords + 1
 * byte before fd2_setup_chars_and_camera_for_intro indexes them by char slot */
extern const uint8 data_fd2_chapter_ch23_end_scene_char_pos_x_table[17];   /* 0x522A3 */
extern const uint8 data_fd2_chapter_ch23_end_scene_char_pos_y_table[17];   /* 0x522B4 */
extern const uint8 data_fd2_chapter_ch23_end_scene_char_facing_table[17];  /* 0x522C5 */

/* ch26 end-scene char tables (private to fd2_chapter_26_end); 16-byte tables
 * (one byte per char slot), copied onto stack as 4 dwords before
 * fd2_setup_chars_and_camera_for_intro indexes them by char slot */
extern const uint8 data_fd2_chapter_ch26_end_scene_char_pos_x_table[16];   /* 0x522D6 */
extern const uint8 data_fd2_chapter_ch26_end_scene_char_pos_y_table[16];   /* 0x522E6 */
extern const uint8 data_fd2_chapter_ch26_end_scene_char_facing_table[16];  /* 0x522F6 */

/* ch27 end-scene char tables (private to fd2_chapter_27_end); two 16-byte
 * tables (one byte per char slot, slots 0..15), copied onto stack as 4 dwords
 * before fd2_setup_chars_and_camera_for_intro indexes them by char slot. The
 * trailing byte is a vestigial 1-byte scratch handed by-address to
 * fd2_animate_status_effect_overlay_flicker (pointee ignored). */
extern const uint8 data_fd2_chapter_ch27_end_scene_char_pos_x_table[16];   /* 0x52306 */
extern const uint8 data_fd2_chapter_ch27_end_scene_char_pos_y_table[16];   /* 0x52316 */
extern const uint8 data_fd2_chapter_ch27_end_scene_vestigial_byte;         /* 0x52326 */

/* ch30 end-scene char tables (private to fd2_chapter_30_end); three 20-byte
 * tables (one byte per char slot, slots 0..0x13), copied onto stack as 5 dwords
 * each before fd2_setup_chars_and_camera_for_intro indexes them by char slot */
extern const uint8 data_fd2_chapter_ch30_end_scene_char_pos_x_table[20];   /* 0x52327 */
extern const uint8 data_fd2_chapter_ch30_end_scene_char_pos_y_table[20];   /* 0x5233B */
extern const uint8 data_fd2_chapter_ch30_end_scene_char_facing_table[20];  /* 0x5234F */

/* Per-chapter combat-cinematic terrain override byte for immune (flying/
 * lifted) classes; indexed by chapter id. (.object2 const, byte[30]) */
extern const uint8 data_fd2_chapter_combat_cinematic_mode_per_chapter[30];   /* 0x52363 */

/* ---- ending cinematic scripted-frame table (.object2 const) ---- */
extern const int32 data_fd2_chapter_ending_music_trigger_frames[15];    /* 0x5204E  scroll-row thresholds for SFX/palette swaps */

/* ---- game-clear credit-roll per-duel tables (.object2 const, 20 bytes each) ---- */
extern const uint8 data_fd2_chapter_ending_credit_roll_top_portrait_id_table[20];    /* 0x525DC  top-half portrait ids */
extern const uint8 data_fd2_chapter_ending_credit_roll_bottom_portrait_id_table[20]; /* 0x525F0  bottom-half portrait ids */
extern const uint8 data_fd2_chapter_ending_credit_roll_scripted_outcome_table[20];   /* 0x52604  scripted_cinematic mode per duel */

/* ---- chapter 3 end recruit-scene char placement tables (.object2 const) ---- */
extern const uint8 data_fd2_chapter_ch03_end_scene_char_pos_x_table[7];      /* 0x520BA  7 chars X */
extern const uint8 data_fd2_chapter_ch03_end_scene_char_pos_y_table[7];   /* 0x520C1  7 chars Y */
extern const uint8 data_fd2_chapter_ch03_end_scene_char_facing_table[7];     /* 0x520C8  7 chars facing */

/* ---- chapter 5 end recruit-scene char placement tables (.object2 const) ---- */
extern const uint8 data_fd2_chapter_ch05_end_scene_char_pos_x_table[7]; /* 0x520CF  7 chars X */
extern const uint8 data_fd2_chapter_ch05_end_scene_char_pos_y_table[7]; /* 0x520D6  7 chars Y */
extern const uint8 data_fd2_chapter_ch05_end_scene_char_facing_table[7]; /* 0x520DD  7 chars facing */

/* ---- chapter 7 end recruit-scene char placement tables (.object2 const) ---- */
extern const uint8 data_fd2_chapter_ch07_end_scene_char_pos_x_table[9];      /* 0x520E4  9 chars X */
extern const uint8 data_fd2_chapter_ch07_end_scene_char_pos_y_table[9];      /* 0x520ED  9 chars Y */
extern const uint8 data_fd2_chapter_ch07_end_scene_char_facing_table[9]; /* 0x520F6  9 chars facing */

/* ---- chapter 8 end recruit-scene char placement tables (.object2 const;
 *      facing is an inline fixed value (2), so there is no facing table) ---- */
extern const uint8 data_fd2_chapter_ch08_end_scene_char_pos_x_table[10]; /* 0x520FF  10 chars X */
extern const uint8 data_fd2_chapter_ch08_end_scene_char_pos_y_table[10];   /* 0x52109  10 chars Y */

/* ---- chapter 10 end scene char placement tables (.object2 const;
 *      facing is an inline fixed value (2), so there is no facing table) ---- */
extern const uint8 data_fd2_chapter_ch10_end_scene_char_pos_x_table[11]; /* 0x52113  11 chars X */
extern const uint8 data_fd2_chapter_ch10_end_scene_char_pos_y_table[11]; /* 0x5211E  11 chars Y */

/* ---- chapter 12 end scene char placement tables (.object2 const) ---- */
extern const uint8 data_fd2_chapter_ch12_end_scene_char_pos_x_table[14];  /* 0x52129  14 chars X */
extern const uint8 data_fd2_chapter_ch12_end_scene_char_pos_y_table[14];  /* 0x52137  14 chars Y */
extern const uint8 data_fd2_chapter_ch12_end_scene_char_facing_table[14]; /* 0x52145  14 chars facing */

/* ---- chapter 14 end scene char placement tables (.object2 const) ---- */
extern const uint8 data_fd2_chapter_ch14_end_scene_char_pos_x_table[16];     /* 0x52153  16 chars X */
extern const uint8 data_fd2_chapter_ch14_end_scene_char_pos_y_table[16];     /* 0x52163  16 chars Y */
extern const uint8 data_fd2_chapter_ch14_end_scene_char_facing_table[16];    /* 0x52173  16 chars facing */

/* ---- chapter 16 end scene char placement tables (.object2 const) ---- */
extern const uint8 data_fd2_chapter_ch16_end_scene_char_pos_x_table[16];     /* 0x52183  16 chars X */
extern const uint8 data_fd2_chapter_ch16_end_scene_char_pos_y_table[16];     /* 0x52193  16 chars Y */

/* ---- chapter 17 end scene char placement tables (.object2 const) ---- */
extern const uint8 data_fd2_chapter_ch17_end_scene_char_pos_x_table[16];     /* 0x521A3  16 chars X */
extern const uint8 data_fd2_chapter_ch17_end_scene_char_pos_y_table[16];     /* 0x521B3  16 chars Y */

/* ---- chapter 18 end scene char placement tables (.object2 const) ---- */
extern const uint8 data_fd2_chapter_ch18_end_scene_char_pos_x_table[17];     /* 0x521C3  17 chars X */
extern const uint8 data_fd2_chapter_ch18_end_scene_char_pos_y_table[17];     /* 0x521D4  17 chars Y */
extern const uint8 data_fd2_chapter_ch18_end_scene_char_facing_table[17];    /* 0x521E5  17 chars facing */

/* ---- battle state ---- */
extern uint32 data_fd2_battle_anim_phase;                               /* 0x51A83 */
extern uint32 data_fd2_battle_ai_post_action_consequence_idx;           /* 0x51A8F */
extern uint32 data_fd2_battle_teleport_dest_world_x;                    /* 0x51CF9 */
extern uint32 data_fd2_battle_teleport_dest_world_y;                    /* 0x51CFD */
extern uint32 data_fd2_battle_player_action_result_code;                /* 0x53C53 */
extern uint8  data_fd2_battle_last_hit_or_miss_flag;                    /* 0x53C6B */
extern uint32 data_fd2_battle_pending_xp_credit;                       /* 0x53EC8 */
extern uint32 data_fd2_battle_tile_map_anim_frame_counter;              /* 0x53C1F */
extern uint32 data_fd2_battle_spell_aoe_count_and_fx_queue_idx;         /* 0x53EC4 */
extern const uint8 data_fd2_battle_damage_number_format_buffer[5];      /* 0x52045  "    \0" template */
extern const uint8 data_fd2_battle_miss_indicator_sprite_ids[4];        /* 0x5204A  adjacent to format buffer; 4 sprite ids for the MISS indicator */
extern uint8  data_fd2_battle_floating_damage_sprite_id_queue[200];     /* 0x53C6C */
extern uint8  data_fd2_battle_floating_damage_x_offset_queue[200];      /* 0x53D34 */
extern uint8  data_fd2_battle_floating_damage_target_char_idx_queue[200]; /* 0x53DFC */
extern uint32 data_fd2_battle_scripted_cinematic_mode_or_terrain_idx;   /* 0x540FF */
extern uint32 data_fd2_battle_combat_cinematic_split_bg_b_buf_ptr;      /* 0x54103 */
extern uint32 data_fd2_battle_combat_cinematic_spotlight_bg_buf_ptr;    /* 0x54107 */
extern uint32 data_fd2_battle_special_cinematic_bg_layer_0_buf_ptr;     /* 0x5410B */
extern uint32 data_fd2_battle_special_cinematic_bg_layer_1_buf_ptr;     /* 0x5410F */
extern uint32 data_fd2_battle_special_cinematic_bg_layer_2_buf_ptr;     /* 0x54113 */
extern uint32 data_fd2_battle_fast_mode_walk_overlay_ptr;               /* 0x53B0F */
/* Per-subframe defender-sprite shake offsets for the combat-hit cinematic;
 * indexed by a decaying shake counter (5..0). (.object2 const int[6]) */
extern const int32 data_fd2_battle_combat_hit_shake_x_offset_table[6];  /* 0x5255F */
extern const int32 data_fd2_battle_combat_hit_shake_y_offset_table[6];  /* 0x52577 */

/* ---- battle spell-effect constants ---- */
extern const double data_fd2_battle_spell_ap_boost_factor_015;          /* 0x50210  const 0.15 */
extern const double data_fd2_battle_spell_dp_boost_factor_015;          /* 0x50218  const 0.15 */
extern int32  data_fd2_battle_combat_speech_bubble_pos_pairs[4];        /* 0x53A30  attacker/counter bubble (x,y) pairs; [2]=-1 means no counter */

/* ---- battle AI scoring ---- */
extern const double data_fd2_battle_ai_enemy_spell_score_multiplier_15; /* 0x50144  const 1.5 */
extern int32 data_fd2_battle_ai_best_spell_score;                       /* 0x53C23  signed max accumulator (writer @0x15AE8 uses JG) */
extern uint32 data_fd2_battle_ai_best_spell_target_x;                   /* 0x53C27 */
extern uint32 data_fd2_battle_ai_best_spell_target_y;                   /* 0x53C2B */
extern uint32 data_fd2_battle_ai_best_spell_id;                         /* 0x53C2F */
extern int32 data_fd2_battle_ai_best_item_score;                        /* 0x53C33 */
extern uint32 data_fd2_battle_ai_best_item_target_x;                    /* 0x53C37 */
extern uint32 data_fd2_battle_ai_best_item_target_y;                    /* 0x53C3B */
extern uint32 data_fd2_battle_ai_best_item_slot;                        /* 0x53C3F */
extern uint32 data_fd2_battle_ai_best_physical_target_x;                /* 0x53C43 */
extern uint32 data_fd2_battle_ai_best_physical_target_y;                /* 0x53C47 */
extern uint32 data_fd2_battle_ai_best_physical_target_idx;              /* 0x53C4B */
extern int32 data_fd2_battle_ai_best_physical_score;                    /* 0x53C4F */

/* ---- battle tile map ---- */
extern uint32 data_fd2_battle_tile_map_ptr;                             /* 0x53A51 */
extern uint32 data_fd2_tile_event_data_table_ptr;                       /* 0x53A55 */
extern uint32 data_fd2_chapter_portrait_load_buffer;                             /* 0x53A59 */
extern uint32 data_fd2_battle_scene_snapshot;                                    /* 0x53A5D */
extern uint32 data_fd2_tile_attribute_flags_buffer_ptr;                 /* 0x53A69 */
extern uint32 data_fd2_current_chapter_text;                                     /* 0x53A79 */

/* ---- cursor & map viewport ---- */
extern uint32 data_fd2_battle_view_window_origin_x;                     /* 0x53AA9 */
extern uint32 data_fd2_battle_view_window_origin_y;                     /* 0x53AAD */
extern uint32 data_fd2_battle_cursor_world_x;                           /* 0x53AB1 */
extern uint32 data_fd2_battle_cursor_world_y;                           /* 0x53AB5 */
extern uint32 data_fd2_battle_cursor_screen_x;                          /* 0x53AB9 */
extern uint32 data_fd2_battle_cursor_screen_y;                          /* 0x53ABD */
extern uint32 data_fd2_battle_map_width_tiles;                          /* 0x53AC1 */
extern uint32 data_fd2_battle_map_height_tiles;                         /* 0x53AC5 */
extern uint8  data_fd2_chapter_init_phase_flag;                         /* 0x53AFA */

/* ---- UI ---- */
extern uint8  data_fd2_ui_click_debounce_skip_count;                    /* 0x51A42 */
extern uint8  data_fd2_ui_terrain_hud_user_enabled;                     /* 0x51AAB */
extern uint8  data_fd2_ui_play_active_flag;                             /* 0x51AAC */
extern uint32 data_fd2_ui_terrain_hud_panel_offset_51a0c;               /* 0x51A0C */
extern uint8  data_fd2_ui_game_speed_flag;                              /* 0x53AF9 */
extern uint32 data_fd2_ui_menu_cursor_idx;                              /* 0x53C57 */
extern uint32 data_fd2_ui_menu_scroll_offset;                           /* 0x5412F */
extern uint32 data_fd2_ui_menu_visible_item_count;                      /* 0x5413F */
extern uint32 data_fd2_ui_menu_saved_cursor_idx;                        /* 0x5414B */
extern uint32 data_fd2_ui_menu_saved_scroll_offset;                     /* 0x5414F */
extern const int32  data_fd2_ui_field_command_menu_options_template[4]; /* 0x51E9F */
extern int32  data_fd2_ui_field_command_menu_state_template[4];         /* 0x53EF2 */
extern int32  data_fd2_ui_player_action_menu_state_template[4];         /* 0x53F12 */
extern const int32 data_fd2_ui_inline_action_menu_template[4];          /* 0x51ED5 */
extern const int32 data_fd2_ui_game_options_menu_slots_template[4];     /* 0x51EAF */
extern int32  data_fd2_ui_game_options_menu_state_template[4];          /* 0x53F02 */
extern const int32 data_fd2_dialog_advance_collapse_template[4];        /* 0x51EE5 */
extern const int32 data_fd2_ui_save_load_newgame_menu_template[4];      /* 0x51EF5 */
extern int32  data_fd2_ui_save_load_menu_state_template[4];             /* 0x53F22 */
extern const int32 data_fd2_ui_tactical_overview_team_colors_table[3];  /* 0x5208a  3 x 4B per-team color base */
extern const int32 data_fd2_ui_item_command_menu_template[4];          /* 0x51F05  const {8,9,10,11} */
extern int32  data_fd2_ui_item_command_menu_state_template[4];          /* 0x53F32 */
extern uint32 data_fd2_ui_slide_anim_accumulator_buf_ptr;               /* 0x53C5B */
extern uint32 data_fd2_ui_slide_bg_snapshot_buf_ptr;                    /* 0x53C5F */
extern uint32 data_fd2_ui_slide_composed_target_buf_ptr;                /* 0x53C63 */

/* ---- graphics ---- */
extern uint8  data_fd2_graphics_text_scroll_pending_line_count;         /* 0x51A10 */
extern const uint32 data_fd2_battle_view_window_max_x;                        /* 0x51A87 */
extern const uint32 data_fd2_battle_view_window_max_y;                        /* 0x51A8B */
extern uint32 data_fd2_graphics_forced_tile_anim_frame;                 /* 0x51A93 */
extern const uint8 data_fd2_graphics_tile_anim_palette_phase_lookup[20]; /* 0x51A97 */
extern uint32 data_fd2_graphics_bg_animation_frame_idx;                 /* 0x539FC */
/* per-frame BIOS-tick latches owned by fd2_composite_battle_tile_map (mutable) */
extern uint32 data_fd2_battle_tile_anim_last_advance_tick;              /* 0x539F4 */
extern uint32 data_fd2_battle_bg_anim_last_advance_tick;                /* 0x539F8 */
extern uint32 data_fd2_graphics_battle_compose_flip_tick_latch;         /* 0x53A00 */
extern uint32 data_fd2_graphics_bg_anim_flip_flag;                      /* 0x53A40 */
extern uint32 data_fd2_battle_compose_left_edge_clip_offset;            /* 0x53AED */
extern uint32 data_fd2_battle_compose_parallax_scroll_y_rows;          /* 0x53AF1 */
extern uint32 data_fd2_battle_compose_walk_step_y_sub_pixel_offset;     /* 0x53AF5 */
extern uint32 data_fd2_graphics_static_bg_buffer_ptr;                   /* 0x53AFF */
extern uint32 data_fd2_graphics_animated_bg_buffer_ptr;                 /* 0x53B03 */
extern int    data_fd2_battle_walk_anim_x_scroll_offset;                /* 0x53B07 -- signed: reader does signed /2 (SAR) and left-scroll stores negatives */
extern int    data_fd2_battle_walk_anim_y_scroll_rows;                  /* 0x53B0B -- signed: reader does SAR EDX,0x1f + IDIV signed divide-by-3 */
extern uint8  data_fd2_graphics_char_sprite_shake_jitter_bit;          /* 0x53A04 */
extern int32  data_fd2_graphics_char_sprite_paint_jitter_tick_latch;    /* 0x53A08 */
extern uint32 data_fd2_graphics_chapter_walk_anim_alt_palette_idx;      /* 0x53C07 */
extern int32  data_fd2_graphics_chapter_ambient_palette_anim_idx;       /* 0x53C0B  signed: reader@0x121CF SAR/2 idiom */
extern uint32 data_fd2_graphics_chapter_ambient_palette_anim_tick_latch; /* 0x53C0F */
/* blit scratch state (verified game-side WRITE xrefs -> mutable, zero-init) */
extern glyph_blit_state data_fd2_graphics_glyph_blit_state;             /* 0x627A3 */
extern uint16 data_fd2_graphics_sprite_mask_blit_width;                 /* 0x6017B */
extern uint16 data_fd2_graphics_rle_blit_cur_width;                     /* 0x627B4 */
extern uint16 data_fd2_graphics_rle_blit_remaining_rows;                /* 0x627B6 */
extern uint8  data_fd2_graphics_sprite_blit_scaler_loop_state[6];       /* 0x627BA */
extern uint16 data_fd2_graphics_sprite_blit_src_width;                  /* 0x627C0 */
extern uint16 data_fd2_graphics_sprite_blit_src_height;                 /* 0x627C2 */
extern uint16 data_fd2_graphics_sprite_blit_scale_num;                  /* 0x627C4 */
extern uint16 data_fd2_graphics_sprite_blit_scale_den;                  /* 0x627C6 */
/* Per-row x displacement table for the background shimmer / heat-haze blit
 * (fd2_blit_buffer_with_per_row_offset); a smooth 0..4..0..1 up/down ramp
 * cycled by row. Two readers, no writers -> read-only const.
 * {2,3,3,4,4,4,3,3,2,1,1,0,0,0,1,1} */
extern const uint8 data_fd2_graphics_shimmer_offset_table_16b[16];      /* 0x627C8 */
extern const double data_fd2_graphics_radian_per_degree_const;          /* 0x501F8  const 0.0174532 (deg->rad) */
extern const double data_fd2_graphics_scatter_y_offset_neg8;            /* 0x50200  const -8.0 (AoE scatter Y skew) */
extern const double data_fd2_graphics_circle_anim_div_10;               /* 0x501F0  const 10.0 */
extern const double data_fd2_graphics_circle_band_radius_scale_16;      /* 0x50208  const 1.6 */

/* ---- chapter intro dialog corner offsets (.object2 const) ----
 * table_a is signed (used with IDIV in the wing slide-in/out animation:
 * base + corner_offs[i]/divisor); table_b is sign-agnostic (additive). */
extern const int32 data_fd2_ui_chapter_intro_dialog_corner_offset_table_a[4]; /* 0x526DA */
extern const int32 data_fd2_ui_chapter_intro_dialog_corner_offset_table_b[4]; /* 0x526EA */
extern const uint8 data_fd2_chapter_intro_menu_speaker_portrait_id_table[6];  /* 0x52659 */

/* Per-chapter shop "inventory full" FDTXT dialog-id table (int16, indexed
 * by chapter cursor state in the buy/sell menus). The church-revive menu
 * aliases the same bytes as a per-job revive-price multiplier table,
 * accessed as [bJob_id + 5]. (vendor data overlap) */
extern const int16 data_fd2_dialog_shop_inventory_full_dialog_text_id_table[6]; /* 0x5265F */

/* Per-basic-class required key-item id for class change, indexed directly by
 * runtime_char.portrait_id (basic classes 0..0x11). 18 bytes. (= 0x5266B+0x3C) */
extern const uint8 data_fd2_ui_per_basic_portrait_class_change_key_item_id_table[18]; /* 0x526A7 */

/* ---- per-chapter transition tables (.object2 const) ----
 * category: 0 = story (intro panel + radio menu), nonzero = battle (save
 * prompt + recruitment). intro panel resource idx is selected by the
 * chapter-intro category byte (metadata[0]). */
extern const uint8 data_fd2_chapter_per_chapter_category_table[30];                  /* 0x526B9 */
extern const uint8  data_fd2_chapter_intro_panel_resource_idx_per_metadata_category_table[3]; /* 0x526D7 */

/* chapter-intro pose target position tables, indexed by
 * (transition_state + metadata_category*6). The "y_row" table feeds the
 * blit's X arg (-0x96, *0x80, +0x5000); the "x_column" table feeds the Y
 * arg (-0x64, *0x80, +0x3200) -- the column/row naming is the world-grid
 * axis the byte selects, not the screen axis it scales into. */
extern const uint8 data_fd2_chapter_intro_portrait_pose_y_row_table[18];     /* 0x52635 */
extern const uint8 data_fd2_chapter_intro_portrait_pose_x_column_table[18];  /* 0x52647 */

/* ---- recruitment screen ---- */
extern uint32 data_fd2_ui_recruitment_screen_repaint_tick_latch;         /* 0x54127 */

/* ---- chapter intro / transition menu state ---- */
extern uint32 data_fd2_chapter_intro_menu_cursor_state;                 /* 0x5412B  selects intro BG idx / portrait / roster layout / return flag */
extern uint32 data_fd2_chapter_intro_menu_overlay_buf_ptr;              /* 0x5413B */
extern uint32 data_fd2_chapter_intro_dialog_anim_frame_idx;             /* 0x54133 */
extern uint32 data_fd2_chapter_intro_active_metadata_entry_ptr;         /* 0x54137  cached chapter-intro metadata entry pointer (weapons/items/mystery byte slices) */
extern uint32 data_fd2_chapter_intro_dialog_subframe_anim_counter;      /* 0x54153 */
extern uint32 data_fd2_ui_menu_screen_sprite_atlas_buf_ptr;             /* 0x54147 */
extern uint8 *data_fd2_ui_menu_candidate_array_ptr;                     /* 0x54143 */
/* per-job revive/promote price multiplier (signed int16), indexed by
 * job_id-1; promote/revive grid price = char.level * table[job_id-1]. */
extern const int16 data_fd2_ui_per_job_revive_or_promote_cost_table[30]; /* 0x5266B  int16 per job */

/* inline 3-byte battle-drop entry blob for chapter-event handler 0x27
 * (type byte + LE uint16 value); read only by
 * fd2_chapter_event_handler_27__unref_drop. */
extern const uint8 data_fd2_chapter_event_handler_27_drop_entry_inline[3]; /* 0x52742 */
/* inline 3-byte battle-drop entry blob for chapter-event handler 0x29
 * (type byte + LE uint16 value); read only by
 * fd2_chapter_event_handler_29__unref_drop. */
extern const uint8 data_fd2_chapter_event_handler_29_drop_entry_inline[3]; /* 0x52745 */

/* ---- text / dialog ---- */
extern uint32 data_fd2_all_game_text_ptr;                               /* 0x53A7D */
extern uint32 data_fd2_dialog_current_speaker_char_ptr;                 /* 0x53C1B */
extern uint32 data_fd2_dialog_last_action_sprite_id_param;              /* 0x53AD9 */
extern uint32 data_fd2_dialog_drop_swap_text_id_param;                  /* 0x53ADD */
extern uint32 data_fd2_dialog_last_action_value_param;                  /* 0x53AE1 */
extern uint32 data_fd2_dialog_active_portrait_blit_offset;              /* 0x53C67 */

/* ---- per-shop-tier dialog text-id tables (short[6], indexed by
 *      data_fd2_chapter_intro_menu_cursor_state) ---- */
extern const int16 data_fd2_dialog_shop_buy_for_dialog_text_id_table[6];        /* 0x526FA */
extern const int16 data_fd2_dialog_shop_no_money_dialog_text_id_table[6];       /* 0x52706 */
extern const int16 data_fd2_dialog_shop_no_equip_dialog_text_id_table[6];       /* 0x52712 */
extern const int16 data_fd2_dialog_shop_auto_equip_dialog_text_id_table[6];     /* 0x5271E */
extern const int16 data_fd2_dialog_shop_sell_for_dialog_text_id_table[6];       /* 0x5272A */
extern const int16 data_fd2_dialog_shop_sell_nothing_to_sell_text_id_table[6];  /* 0x52736 */
extern void  *data_fd2_dialog_dialog_frame_layer_save_buffer_ptrs[5];   /* 0x53A18 */
extern uint32 data_fd2_dialog_portrait_blink_frame_idx;                 /* 0x53A10 */
extern uint32 data_fd2_dialog_portrait_blink_subtick_counter;           /* 0x53A14 */
extern void  *data_fd2_dialog_area_backup_buffer;                       /* 0x53A71 */

/* ---- VGA palette ---- */
extern uint32 data_fd2_vga_palette_data_ptr;                            /* 0x53A65 */

/* ---- palette cycle animation (.object3) ---- */
extern uint16 data_fd2_animation_palette_cycle_last_tick;               /* 0x60000 */
extern uint8  data_fd2_animation_palette_cycle_frame_idx;               /* 0x60002 */
extern const uint8 data_fd2_animation_palette_cycle_rgb_table[93];      /* 0x60003  93B */

/* ---- large game state buffer ---- */
extern uint32 data_fd2_large_game_state_buffer_ptr;                     /* 0x53A49 */

/* ---- dialog blink oscillator ---- */
extern uint32 data_fd2_dialog_blink_phase_oscillator;                   /* 0x53C13 */
extern uint32 data_fd2_dialog_blink_phase_oscillator_tick_latch;        /* 0x53C17 */

/* ---- UI sprite sheet ---- */
extern uint32 data_fd2_ui_anim_sprite_sheet_ptr;                        /* 0x53A81 */

/* ---- resource buffers (loaded from DAT files) ---- */
extern uint8 *data_fd2_portrait_sprite_buffer;                          /* 0x53A85 */
extern uint32 data_fd2_menu_dialog_state_handle;                        /* 0x53A89 */
extern uint32 data_fd2_tile_anim_table_base;                            /* 0x53A6D */
extern uint32 data_fd2_chinese_font_sheet;                              /* 0x53A75 */

/* ---- resource filename strings (.object2 const) ---- */
extern const char data_fd2_string_resource_filename_fdtxt_dat[10];      /* 0x51A43  "FDTXT.DAT" */
extern const char data_fd2_string_resource_filename_fdother_dat[12];    /* 0x51A4D  "FDOTHER.DAT" */
extern const char data_fd2_string_resource_filename_fdfield_dat_51a59[12]; /* 0x51A59  "FDFIELD.DAT" */
extern const char data_fd2_string_resource_filename_fdshap_dat_51a65[11];  /* 0x51A65  "FDSHAP.DAT" */
extern const char data_fd2_string_resource_filename_dato_dat_51a70[9];  /* 0x51A70  "DATO.DAT" */
extern const char data_fd2_string_resource_filename_bg_dat_52381[7];    /* 0x52381  "BG.DAT" */
extern const char data_fd2_string_resource_filename_figani_dat_52388[11]; /* 0x52388  "FIGANI.DAT" */
extern const char data_fd2_string_resource_filename_tai_dat[8];         /* 0x52393  "TAI.DAT" */

/* ---- UI render format strings (.object2 const) ---- */
extern const char data_fd2_string_ui_render_decimal_format_template[6]; /* 0x51EBF  "%0.5d" */

/* ---- save/load OOM message strings (.object2 const, 3 cross-.obj copies) ---- */
extern const char data_fd2_string_save_load_oom_msg_load_pbuf_50004[20];   /* 0x50004  " Out of Memory !!!\n" */
extern const char data_fd2_string_save_load_oom_msg_tile_event_50023[20];  /* 0x50023  " Out of Memory !!!\n" */
extern const char data_fd2_string_save_load_oom_msg_runtime_char_50037[20]; /* 0x50037  " Out of Memory !!!\n" */

/* ---- chapter battle-data load error strings (.object2 const) ---- */
extern const char data_fd2_string_field_map_oom_msg_chapter_runtime_50064[20]; /* 0x50064  " Out of Memory !!!\n" */
extern const char data_fd2_string_field_map_fdicon_not_found_err_50086[35];    /* 0x50086  "\n\n File not found 'FDICON.B24!! \n\n" */

/* ---- resource / portrait cache ---- */
extern uint32 data_fd2_resource_portrait_cache_buffer_used;             /* 0x539EC */
extern uint32 data_fd2_resource_portrait_sheet_ptr;                     /* 0x53AD1 */
extern uint32 data_fd2_resource_portrait_cache_count;                   /* 0x53BDF */
extern uint32 data_fd2_resource_portrait_cache_alloc_offset;            /* 0x53BE3 */
extern uint32 data_fd2_resource_portrait_cache_total_size;              /* 0x53BE7 */
extern uint32 data_fd2_portrait_sprite_cache;                                    /* 0x53A61 */
extern uint8  data_fd2_resource_portrait_cache_id_list_base[160];       /* 0x53B17 */
extern uint32 data_fd2_resource_last_loaded_resource_size;              /* 0x53BFF */
extern uint32 data_fd2_field_map_tile_event_consumed_flags_ptr;         /* 0x53AD5 */

/* ---- battle turn counter (serialized to FD2.SAV header +0x30C3) ---- */
extern uint32 data_fd2_battle_turn_counter;                             /* 0x53BEF */

/* ---- save/load ---- */
extern int32  data_fd2_shared_party_total_gold;                         /* 0x53BF3 */
extern uint32 data_fd2_shared_menu_party_roster_buffer_ptr;             /* 0x53BF7 */
extern uint32 data_fd2_shared_menu_party_member_count;                  /* 0x53BFB */

/* ---- AIL function pointers ---- */
extern uint32 data_ail_alloc_fnptr;                                     /* 0x52758 */
extern uint32 data_ail_free_fnptr;                                      /* 0x5275C */

/* ---- audio ---- */
extern uint8  data_fd2_audio_bgm_last_set_track_id;                     /* 0x51A11 */
extern uint8  data_fd2_audio_bgm_enabled_flag;                          /* 0x51E61 */
extern uint8  data_fd2_audio_sfx_enabled_flag;                          /* 0x51E62 */
extern const uint8  data_fd2_audio_per_chapter_player_turn_bgm_track[30];     /* 0x51E63 */
extern const uint8  data_fd2_audio_per_chapter_enemy_turn_bgm_track[30];      /* 0x51E81 */
extern uint32 data_fd2_audio_bgm_sequence_handle;                       /* 0x53ED0 */
extern void  *data_fd2_audio_bgm_driver_handle;                         /* 0x53ED8 */
extern uint32 data_fd2_audio_bgm_sequence_data_buf_ptr;                 /* 0x53EE0 */
extern uint32 data_fd2_audio_sfx_dig_driver_handle;                     /* 0x53EDC */
extern uint8  data_fd2_audio_bgm_driver_available_flag;                  /* 0x53EF0 */
extern uint8  data_fd2_audio_sfx_driver_available_flag;                  /* 0x53EF1 */
extern uint32 data_fd2_audio_sfx_sample_handle_0;                       /* 0x53EE4 */
extern uint32 data_fd2_audio_sfx_sample_handle_1;                       /* 0x53EE8 */
extern uint32 data_fd2_audio_fdother_sfx_bank_buf_ptr;                  /* 0x53EEC */
extern uint32 data_fd2_audio_status_effect_sfx_handle_ptr;              /* 0x53B13 */
extern uint32 data_fd2_audio_figani_sfx_bank_buf_ptr;                  /* 0x54117 */
extern const uint8  data_fd2_audio_figani_sfx_bank_fdother_index_lut[6];      /* 0x525D6  6B; 1-based id->FDOTHER idx */
extern uint8  data_fd2_audio_walk_step_sfx_cadence_counter;             /* 0x540FE */
extern uint32 data_fd2_audio_figani_sfx_bank_defender_buf_ptr;          /* 0x5411B */
extern const char data_fd2_string_resource_filename_fdmus_dat[10];                        /* 0x51A79  "FDMUS.DAT" */

/* ---- animation tables (.object2 const) ---- */
extern const uint8  data_fd2_audio_footstep_sfx_per_job_cadence_class_table[29]; /* 0x52618  29B */
extern const int32 data_fd2_animation_earthquake_screen_shake_params_table[9]; /* 0x52096  3 X-off + 3 Y-off + 3 scale */
extern const uint8 data_fd2_battle_special_attack_shake_x_offset_table[6];   /* 0x52549  per-sub-frame X-offset cache */
extern const uint8 data_fd2_animation_status_overlay_flicker_color_template[30]; /* 0x51F15 */
extern const uint8 data_fd2_animation_spell_palette_flash_table[108];   /* 0x51AAD  36*3 RGB planes R/G/B */
extern const uint8 data_fd2_animation_spell_sprite_offset_table[33];    /* 0x51F33 */
extern const uint8 data_fd2_animation_spell_frame_count_table[33];      /* 0x51F54 */
extern const uint8 data_fd2_animation_spell_sfx_frame_table[33];        /* 0x51F75 */
extern const uint8 data_fd2_animation_spell_overlay_blink_mask_table[30]; /* 0x52006 */
extern const uint8 data_fd2_animation_spell_projectile_y_offset_table[25]; /* 0x5202C */
extern uint8  data_fd2_battle_summon_minor_anim_state5_frame_counter;    /* 0x540FA */
extern uint8  data_fd2_battle_summon_minor_anim_alternating_blit_toggle; /* 0x540FB */
extern uint8  data_fd2_graphics_figani_pose_anim_subframe_idx;           /* 0x540FC */
extern uint8  data_fd2_graphics_figani_pose_anim_pose_idx;               /* 0x540FD */
extern uint32 data_fd2_audio_summon_spell_sfx_bank_buf_ptr;             /* 0x5411F */
extern int32  data_fd2_battle_summon_spell_shared_15slot_frame_counter_array[15]; /* 0x53F42 */
extern const uint8  data_fd2_battle_summon_spell_8slot_visibility_table[7];            /* 0x523E1 */
extern const uint32 data_fd2_battle_summon_spell_8slot_y_offset_table[7];              /* 0x523E8 */
extern const int32  data_fd2_battle_summon_spell_8slot_row_multiplier_table[7];        /* 0x52404 */
extern const int32 data_fd2_battle_summon_aura_ring_8slot_x_offset_table[8];     /* 0x52420 */
extern const int32 data_fd2_battle_summon_aura_ring_8slot_row_multiplier_table[8];    /* 0x52440 */
extern int32  data_fd2_battle_summon_main_anim_12slot_frame_counter_array[12];    /* 0x53F81 */
extern int32  data_fd2_battle_summon_main_anim_12slot_color_idx_array[12];       /* 0x53FB1 */
extern uint8  data_fd2_battle_summon_main_anim_color_rotation_counter;           /* 0x53FE1 */
extern uint8  data_fd2_battle_summon_main_anim_terminate_flag;                   /* 0x53FE2 */
extern uint8  data_fd2_battle_summon_main_anim_odd_even_frame_toggle;            /* 0x53FE3 */
extern const int32 data_fd2_battle_summon_main_anim_12slot_y_offset_table[12];        /* 0x52460 */
extern const uint8 data_fd2_battle_summon_main_anim_12color_v_offset_table[12];       /* 0x52490 */
extern const uint8 data_fd2_battle_summon_main_anim_12color_sprite_offset_table[12];  /* 0x5249C */
extern uint8  data_fd2_battle_summon_spell_anim_phase_byte;                      /* 0x53F7E */
extern uint8  data_fd2_battle_summon_spell_anim_aux_state_byte_unread;           /* 0x53F7F */
extern uint8  data_fd2_battle_summon_spell_sprite_anim_tick_counter;             /* 0x53F80 */
extern int32  data_fd2_battle_summon_anim_variant_a_6slot_frame_counter_array[6]; /* 0x53FE4 */
extern int32  data_fd2_battle_summon_anim_variant_a_6slot_color_idx_array[6];    /* 0x53FFC */
extern uint8  data_fd2_battle_summon_anim_variant_a_6slot_jitter_byte_array[6];  /* 0x54014 */
extern uint8  data_fd2_battle_summon_anim_variant_a_color_rotation_counter;      /* 0x5401A */
extern uint8  data_fd2_battle_summon_anim_variant_a_terminate_flag;              /* 0x5401B */
extern const int32 data_fd2_battle_summon_anim_variant_a_10color_y_offset_table[10];  /* 0x524A8 */
extern int32  data_fd2_battle_summon_anim_variant_b_6slot_frame_counter_array[6]; /* 0x5401C */
extern int32  data_fd2_battle_summon_anim_variant_b_6slot_color_idx_array[6];    /* 0x54034 */
extern uint8  data_fd2_battle_summon_anim_variant_b_6slot_jitter_byte_array[6];  /* 0x5404C */
extern uint8  data_fd2_battle_summon_anim_variant_b_color_rotation_counter;      /* 0x54052 */
extern uint8  data_fd2_battle_summon_anim_variant_b_terminate_flag;              /* 0x54053 */
extern const int32 data_fd2_battle_summon_anim_variant_b_10color_y_offset_table[10];  /* 0x524D0 */
extern int32  data_fd2_battle_summon_anim_variant_d_4slot_frame_counter_array[4]; /* 0x54097 */
extern int32  data_fd2_battle_summon_anim_variant_d_4slot_color_idx_array[4];    /* 0x540A7 */
extern uint8  data_fd2_battle_summon_anim_variant_d_color_rotation_counter;      /* 0x540B7 */
extern uint8  data_fd2_battle_summon_anim_variant_d_terminate_flag;              /* 0x540B8 */
extern uint8  data_fd2_battle_summon_anim_variant_d_odd_even_frame_toggle;       /* 0x540B9 */
extern const int32 data_fd2_animation_summon_variant_d_3slot_color_row_offsets[10]; /* 0x52511 */
extern const uint8 data_fd2_animation_summon_variant_e_16slot_sprite_base_table[16]; /* 0x52539 */
extern int32  data_fd2_battle_summon_anim_variant_e_16slot_frame_counter_array[16]; /* 0x540BA */
extern int32  data_fd2_battle_summon_anim_variant_c_5slot_x_coord_array[5];      /* 0x54054 */
extern int32  data_fd2_battle_summon_anim_variant_c_5slot_y_coord_array[5];      /* 0x54068 */
extern int32  data_fd2_battle_summon_anim_variant_c_5slot_frame_counter_array[5]; /* 0x5407C */
extern uint8  data_fd2_battle_summon_anim_variant_c_5slot_blit_counter_array[5]; /* 0x54090 */
extern uint8  data_fd2_battle_summon_anim_variant_c_angle_accumulator;           /* 0x54095 */
extern uint8  data_fd2_battle_summon_anim_variant_c_swap_done_latch;             /* 0x54096 */
extern const int32 data_fd2_animation_summon_variant_c_radial_5slot_offsets[5];  /* 0x524F8 */
extern const uint8 data_fd2_animation_summon_variant_c_radial_5slot_byte_offsets[5]; /* 0x5250C */
extern const double data_fd2_animation_summon_radial_angle_step_12;              /* 0x5022B (1.2 y-amplitude) */
extern const double data_fd2_animation_summon_radial_radius_30;                  /* 0x50233 (30.0) */
extern const uint32 data_fd2_battle_summon_spell_palette_r_table;       /* 0x5254F = {3F,33,35,35} LE-packed; byte-indexed by summon_idx in caller's local dword copy */
extern const uint32 data_fd2_battle_summon_spell_palette_g_table;       /* 0x52553 */
extern const uint32 data_fd2_battle_summon_spell_palette_b_table;       /* 0x52557 = {3F,3F,00,09} LE-packed = 0x09003F3F; byte-indexed by summon_idx in caller's local dword copy */
extern const uint32 data_fd2_battle_summon_spell_sfx_bank_index_table;  /* 0x5255B = {5B,5C,5D,5E} LE-packed; byte-indexed by summon_idx in caller's local dword copy */

/* ---- dispatch tables (.object2 const) ---- */
extern void (*const data_fd2_chapter_init_handler_table[30])(void);     /* 0x51D71 */
extern void (*const data_fd2_chapter_end_handler_table[30])(void);      /* 0x51DE9 */
extern void (*const data_fd2_chapter_post_action_handler_table[30])(uint32);  /* 0x51B19 */
extern void (*data_fd2_battle_ai_post_action_consequence_table[90])(uint32); /* 0x51B91 */
extern void (*data_fd2_battle_spell_handler_table[28])(uint32, uint32, uint8 *); /* 0x51D01 */
/* 10-entry summon-spell tick dispatch table. Each entry takes
 * (sprite_handle, sprite_atlas, dst, stride, phase_code) and returns an int
 * frame count (e.g. fd2_tick_summon_spell_minor_animation_state @ 0x275D6,
 * entry #9). Dispatched by spell_type_idx for per-element palette flash /
 * tick advance in the spell-cast cinematic. */
extern int (*data_fd2_battle_spell_cast_cinematic_phase_handler_table[10])(
    uint32, uint32, uint32, uint32, uint32); /* 0x523B9 */
extern uint16 data_fd2_animation_ani_decoder_target_width;              /* 0x52760 */
extern uint32 data_fd2_animation_ani_decoder_dst_buf;                   /* 0x52762 */
extern uint32 data_fd2_animation_ani_decoder_src_buf;                   /* 0x52766 */
extern void (*data_fd2_animation_ani_decoder_frame_dispatch_table[10])(void);  /* 0x5276A */

/* ---- .object3 data tables ---- */
extern const item_effect data_fd2_battle_item_effect_table[215];        /* 0x602AC */
extern const spell_effect data_fd2_battle_spell_effect_table[36];       /* 0x619FD */
extern const enemy_data  data_fd2_battle_enemy_data_table[68];          /* 0x61AF9 */
extern const character_base data_fd2_battle_character_base_table[32];   /* 0x61DA1 */
extern const character_growth  data_fd2_battle_character_growth_table[68];    /* 0x620A1 */

/* ---- input state ---- */
extern uint16 data_fd2_input_idle_current_bios_tick_word;               /* 0x539F0 */
extern uint16 data_fd2_input_idle_last_rendered_tick_word;               /* 0x539F2 */
/* Shared INT 10h/16h REGS scratch (orig 0x53A8D), one 28-byte union REGS. The
 * game casts &data_fd2_input_last_key_pressed to union REGS* and hands it to
 * int386(); last_key aliases byte 0 (AL / ASCII), input_key_mode aliases byte 1
 * (AH / scancode). They MUST be two adjacent bytes of ONE scratch: int386 reads/
 * writes the whole REGS and the AH result must land at last_key+1. Emitting them
 * as two separate uint8 globals lets the linker (a) drop input_key_mode far from
 * byte 1 and (b) place data_fd2_audio_sfx_driver_available_flag exactly at byte 1,
 * where `*(uint16*)&last_key = AX` clobbers it (= dead SFX) while the scancode
 * lands in the wrong byte (= dead keyboard). Macros over one union REGS preserve
 * the vendor union-REGS overlap and keep all call sites unchanged. */
extern union REGS data_fd2_input_int16_regs;                           /* 0x53A8D */
#define data_fd2_input_last_key_pressed (data_fd2_input_int16_regs.h.al)
#define data_fd2_input_key_input_mode   (data_fd2_input_int16_regs.h.ah)

/* ---- timing state ---- */
extern uint32 data_fd2_engine_wait_one_bios_tick_last_seen;             /* 0x53A0C */
extern uint32 data_fd2_engine_wait_n_bios_ticks_last_seen;              /* 0x53A2C */

/* ---- tile attribute modifier tables (.object2 const) ---- */
extern const int32  data_fd2_battle_tile_attr_mv_modifier_table[6];           /* 0x51A12 */
extern const int32  data_fd2_battle_tile_attr_def_modifier_table[6];          /* 0x51A2A */

/* ---- RNG ---- */
extern uint16 data_fd2_shared_rng_seed;                                 /* 0x627B8 (.object3) */

/* ---- battle misc const tables ---- */
extern const uint32 data_fd2_battle_job_magic_resist_table[28];               /* 0x51F96 */
extern const uint8  data_fd2_battle_job_crit_rate_table[27];                  /* 0x5239B */

/* ---- .object3 tables accessed by table accessors (live sub-regions of former orphan blob) ---- */
extern uint32 data_fd2_battle_pathfind_tile_cost_table_ptr;               /* 0x60060 */
extern uint32 data_fd2_battle_pathfind_battle_tile_map_ptr;              /* 0x60064 */
extern uint8  data_fd2_battle_pathfind_map_width;                        /* 0x60068 */
extern uint8  data_fd2_battle_pathfind_map_height;                       /* 0x60069 */
extern uint32 data_fd2_battle_pathfind_caller_context;                   /* 0x6006A */
extern uint8  data_fd2_battle_pathfind_floodfill_seed_x;                 /* 0x6006E */
extern uint8  data_fd2_battle_pathfind_floodfill_seed_y;                 /* 0x6006F */
extern uint8  data_fd2_battle_pathfind_floodfill_max_steps;              /* 0x60070 */
extern uint8  data_fd2_battle_pathfind_dst_x;                            /* 0x60071 */
extern uint8  data_fd2_battle_pathfind_dst_y;                            /* 0x60072 */
extern uint32 data_fd2_battle_pathfind_path_output_buffer_ptr;           /* 0x60073 */
extern uint8  data_fd2_battle_pathfind_current_depth;                    /* 0x60077 */
extern uint8  data_fd2_battle_pathfind_best_path_length;                 /* 0x60078 */
extern uint8  data_fd2_battle_pathfind_step_stack[256];                  /* 0x60079 scratch recursion step-stack, 8B(path)/7B(floodfill) frames */
extern uint8  data_fd2_battle_pathfind_mode_flags;                       /* 0x6017A  0/1/2 tiebreak/dst-record mode */
extern const uint8 data_fd2_battle_class_promotion_data_table[72];             /* 0x615FE  2B per entry */
extern const uint8 data_fd2_battle_movement_cost_table[580];                   /* 0x61646  20B per job, 29 rows */
extern const uint8 data_fd2_battle_job_allowed_items_table[29 * 7];            /* 0x6188A  7B per job, 29 rows (27 logical jobs + 2 reserved) */

/* ---- .object3 pointer tables ---- */
extern const uint8 data_fd2_battle_weapon_attack_anim_pattern_script_pool_84b[84]; /* 0x619A9 */
extern const uint8 *data_fd2_battle_weapon_attack_anim_pattern_ptr_table_21[21]; /* 0x61955 */
extern const uint8 *data_fd2_chapter_cutscene_event_script_ptr_table_106[106];   /* 0x627D8 */

/* ---- .object3 additional tables ---- */
extern const uint8 data_fd2_chapter_intro_metadata_table[26 * 31];      /* 0x6238D  31B per ch */
extern const uint8 data_fd2_battle_spell_learning_table[20 * 12];              /* 0x626B3  12B per entry */
extern const uint8 data_fd2_orphan_table_60181[299];                   /* 0x60181  3B per entry (orphan) */

#endif /* GLOBALS_H */
