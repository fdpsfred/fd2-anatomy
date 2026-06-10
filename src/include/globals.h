#ifndef GLOBALS_H
#define GLOBALS_H

#include "types.h"

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
extern uint8  data_fd2_chapter_per_chapter_category_table[];            /* 0x526B9  1B per chapter (idx by chapter_id; 0 = story) */

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
extern uint32 data_fd2_battle_scripted_cinematic_mode_or_terrain_idx;   /* 0x540FF */
extern uint32 data_fd2_battle_combat_cinematic_spotlight_bg_buf_ptr;    /* 0x54107 */
extern uint32 data_fd2_battle_special_cinematic_bg_layer_0_buf_ptr;     /* 0x5410B */
extern uint32 data_fd2_battle_special_cinematic_bg_layer_1_buf_ptr;     /* 0x5410F */
extern uint32 data_fd2_battle_fast_mode_walk_overlay_ptr;               /* 0x53B0F */

/* ---- battle AI scoring ---- */
extern double data_fd2_battle_ai_enemy_spell_score_multiplier_15;       /* 0x50144  const 1.5 */
extern uint32 data_fd2_battle_ai_best_spell_score;                      /* 0x53C23 */
extern uint32 data_fd2_battle_ai_best_spell_target_x;                   /* 0x53C27 */
extern uint32 data_fd2_battle_ai_best_spell_target_y;                   /* 0x53C2B */
extern uint32 data_fd2_battle_ai_best_spell_id;                         /* 0x53C2F */
extern uint32 data_fd2_battle_ai_best_item_score;                       /* 0x53C33 */
extern uint32 data_fd2_battle_ai_best_item_target_x;                    /* 0x53C37 */
extern uint32 data_fd2_battle_ai_best_item_target_y;                    /* 0x53C3B */
extern uint32 data_fd2_battle_ai_best_item_slot;                        /* 0x53C3F */
extern uint32 data_fd2_battle_ai_best_physical_target_x;                /* 0x53C43 */
extern uint32 data_fd2_battle_ai_best_physical_target_y;                /* 0x53C47 */
extern uint32 data_fd2_battle_ai_best_physical_target_idx;              /* 0x53C4B */
extern uint32 data_fd2_battle_ai_best_physical_score;                   /* 0x53C4F */

/* ---- battle tile map ---- */
extern uint32 data_fd2_battle_tile_map_ptr;                             /* 0x53A51 */
extern uint32 data_fd2_tile_event_data_table_ptr;                       /* 0x53A55 */
extern uint32 chapter_portrait_load_buffer;                             /* 0x53A59 */
extern uint32 battle_scene_snapshot;                                    /* 0x53A5D */
extern uint32 data_fd2_tile_attribute_flags_buffer_ptr;                 /* 0x53A69 */
extern uint32 current_chapter_text;                                     /* 0x53A79 */

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
extern int32  data_fd2_ui_field_command_menu_options_template[4];       /* 0x51E9F */
extern int32  data_fd2_ui_field_command_menu_state_template[4];         /* 0x53EF2 */
extern int32  data_fd2_ui_player_action_menu_state_template[4];         /* 0x53F12 */
extern int32  data_fd2_ui_inline_action_menu_template[4];               /* 0x51ED5 */
extern int32  data_fd2_ui_game_options_menu_slots_template[4];          /* 0x51EAF */
extern int32  data_fd2_ui_game_options_menu_state_template[4];          /* 0x53F02 */
extern int32  data_fd2_dialog_advance_collapse_template[4];             /* 0x51EE5 */
extern int32  data_fd2_ui_save_load_newgame_menu_template[4];           /* 0x51EF5 */
extern int32  data_fd2_ui_save_load_menu_state_template[4];             /* 0x53F22 */
extern int32  data_fd2_ui_item_command_menu_template[4];                /* 0x51F05  const {8,9,10,11} */
extern int32  data_fd2_ui_item_command_menu_state_template[4];          /* 0x53F32 */
extern uint32 data_fd2_ui_slide_anim_accumulator_buf_ptr;               /* 0x53C5B */
extern uint32 data_fd2_ui_slide_bg_snapshot_buf_ptr;                    /* 0x53C5F */
extern uint32 data_fd2_ui_slide_composed_target_buf_ptr;                /* 0x53C63 */

/* ---- graphics ---- */
extern uint32 data_fd2_battle_view_window_max_x;                        /* 0x51A87 */
extern uint32 data_fd2_battle_view_window_max_y;                        /* 0x51A8B */
extern uint32 data_fd2_graphics_forced_tile_anim_frame;                 /* 0x51A93 */
extern uint8  data_fd2_graphics_tile_anim_palette_phase_lookup[20];     /* 0x51A97 */
extern uint32 data_fd2_graphics_bg_animation_frame_idx;                 /* 0x539FC */
extern uint32 data_fd2_graphics_bg_anim_flip_flag;                      /* 0x53A40 */
extern uint32 data_fd2_battle_compose_left_edge_clip_offset;            /* 0x53AED */
extern uint32 data_fd2_battle_compose_parallax_scroll_y_rows;          /* 0x53AF1 */
extern uint32 data_fd2_battle_compose_walk_step_y_sub_pixel_offset;     /* 0x53AF5 */
extern uint32 data_fd2_graphics_static_bg_buffer_ptr;                   /* 0x53AFF */
extern uint32 data_fd2_graphics_animated_bg_buffer_ptr;                 /* 0x53B03 */
extern uint32 data_fd2_battle_walk_anim_x_scroll_offset;                /* 0x53B07 */
extern uint32 data_fd2_battle_walk_anim_y_scroll_rows;                  /* 0x53B0B */
extern uint8  data_fd2_graphics_char_sprite_shake_jitter_bit;          /* 0x53A04 */
extern int32  data_fd2_graphics_char_sprite_paint_jitter_tick_latch;    /* 0x53A08 */
extern uint32 data_fd2_graphics_chapter_walk_anim_alt_palette_idx;      /* 0x53C07 */
extern uint32 data_fd2_graphics_chapter_ambient_palette_anim_idx;       /* 0x53C0B */
extern uint32 data_fd2_graphics_chapter_ambient_palette_anim_tick_latch; /* 0x53C0F */

/* ---- chapter intro dialog corner offsets (.object2 const) ---- */
extern uint32 data_fd2_ui_chapter_intro_dialog_corner_offset_table_b[4]; /* 0x526EA */

/* ---- recruitment screen ---- */
extern uint32 data_fd2_ui_recruitment_screen_repaint_tick_latch;         /* 0x54127 */

/* ---- chapter intro dialog anim ---- */
extern uint32 data_fd2_chapter_intro_menu_cursor_state;                 /* 0x5412B */
extern uint32 data_fd2_chapter_intro_menu_overlay_buf_ptr;              /* 0x5413B */
extern uint32 data_fd2_chapter_intro_dialog_anim_frame_idx;             /* 0x54133 */
extern uint32 data_fd2_chapter_intro_active_metadata_entry_ptr;         /* 0x54137 */
extern uint32 data_fd2_chapter_intro_dialog_subframe_anim_counter;      /* 0x54153 */
extern uint32 data_fd2_ui_menu_screen_sprite_atlas_buf_ptr;             /* 0x54147 */
extern uint32 data_fd2_ui_menu_scroll_offset;                           /* 0x5412F */
extern uint32 data_fd2_ui_menu_saved_cursor_idx;                        /* 0x5414B */
extern uint32 data_fd2_ui_menu_saved_scroll_offset;                     /* 0x5414F */
extern uint32 data_fd2_ui_menu_visible_item_count;                      /* 0x5413F */
extern uint8 *data_fd2_ui_menu_candidate_array_ptr;                     /* 0x54143 */
/* per-chapter portrait pose tables (byte[18]), indexed by
 * chapter_category*6 + cursor_state. dst = lgsb + 0x8088
 * + pose_x_column[off]*stride + pose_y_row[off] (matches Ghidra symbols). */
extern uint8  data_fd2_chapter_intro_portrait_pose_y_row_table[18];     /* 0x52635 */
extern uint8  data_fd2_chapter_intro_portrait_pose_x_column_table[18];  /* 0x52647 */
/* speaker portrait id per chapter-intro menu variant, indexed by
 * chapter_intro_menu_cursor_state (0..5). */
extern uint8  data_fd2_chapter_intro_menu_speaker_portrait_id_table[6]; /* 0x52659 */
/* per-job revive/promote price multiplier (signed int16), indexed by
 * job_id-1; promote/revive grid price = char.level * table[job_id-1]. */
extern int16  data_fd2_ui_per_job_revive_or_promote_cost_table[];       /* 0x5266B  int16 per job */

/* inline 3-byte battle-drop entry blob for chapter-event handler 0x27
 * (type byte + LE uint16 value); read only by
 * fd2_chapter_event_handler_27__unref_drop. */
extern const uint8 data_fd2_chapter_event_handler_27_drop_entry_inline[3]; /* 0x52742 */

/* ---- text / dialog ---- */
extern uint32 data_fd2_all_game_text_ptr;                               /* 0x53A7D */
extern uint32 data_fd2_dialog_current_speaker_char_ptr;                 /* 0x53C1B */
extern uint32 data_fd2_dialog_last_action_sprite_id_param;              /* 0x53AD9 */
extern uint32 data_fd2_dialog_drop_swap_text_id_param;                  /* 0x53ADD */
extern uint32 data_fd2_dialog_last_action_value_param;                  /* 0x53AE1 */
extern uint32 data_fd2_dialog_active_portrait_blit_offset;              /* 0x53C67 */
extern void  *data_fd2_dialog_dialog_frame_layer_save_buffer_ptrs[5];   /* 0x53A18 */
extern uint32 data_fd2_dialog_portrait_blink_frame_idx;                 /* 0x53A10 */
extern uint32 data_fd2_dialog_portrait_blink_subtick_counter;           /* 0x53A14 */
extern void  *data_fd2_dialog_area_backup_buffer;                       /* 0x53A71 */

/* ---- VGA palette ---- */
extern uint32 data_fd2_vga_palette_data_ptr;                            /* 0x53A65 */

/* ---- palette cycle animation (.object3) ---- */
extern uint16 data_fd2_animation_palette_cycle_last_tick;               /* 0x60000 */
extern uint8  data_fd2_animation_palette_cycle_frame_idx;               /* 0x60002 */
extern uint8  data_fd2_animation_palette_cycle_rgb_table[];             /* 0x60003  93B */

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
extern char   data_fd2_string_resource_filename_fdtxt_dat[];            /* 0x51A43  "FDTXT.DAT" */
extern char   data_fd2_string_resource_filename_fdother_dat[];          /* 0x51A4D  "FDOTHER.DAT" */
extern char   data_fd2_string_resource_filename_fdfield_dat_51a59[];    /* 0x51A59  "FDFIELD.DAT" */
extern char   data_fd2_string_resource_filename_fdshap_dat_51a65[];     /* 0x51A65  "FDSHAP.DAT" */
extern char   data_fd2_string_resource_filename_dato_dat_51a70[];       /* 0x51A70  "DATO.DAT" */

/* ---- UI render format strings (.object2 const) ---- */
extern char   data_fd2_string_ui_render_decimal_format_template[6];     /* 0x51EBF  "%0.5d" */

/* ---- save/load OOM message strings (.object2 const, 3 cross-.obj copies) ---- */
extern char   data_fd2_string_save_load_oom_msg_load_pbuf_50004[];      /* 0x50004  " Out of Memory !!!\n" */
extern char   data_fd2_string_save_load_oom_msg_tile_event_50023[];     /* 0x50023  " Out of Memory !!!\n" */
extern char   data_fd2_string_save_load_oom_msg_runtime_char_50037[];   /* 0x50037  " Out of Memory !!!\n" */

/* ---- chapter battle-data load error strings (.object2 const) ---- */
extern char   data_fd2_string_field_map_oom_msg_chapter_runtime_50064[];   /* 0x50064  " Out of Memory !!!\n" */
extern char   data_fd2_string_field_map_fdicon_not_found_err_50086[];      /* 0x50086  "\n\n File not found 'FDICON.B24!! \n\n" */

/* ---- resource / portrait cache ---- */
extern uint32 data_fd2_resource_portrait_cache_buffer_used;             /* 0x539EC */
extern uint32 data_fd2_resource_portrait_sheet_ptr;                     /* 0x53AD1 */
extern uint32 data_fd2_resource_portrait_cache_count;                   /* 0x53BDF */
extern uint32 data_fd2_resource_portrait_cache_alloc_offset;            /* 0x53BE3 */
extern uint32 data_fd2_resource_portrait_cache_total_size;              /* 0x53BE7 */
extern uint32 portrait_sprite_cache;                                    /* 0x53A61 */
extern uint8  data_fd2_resource_portrait_cache_id_list_base[40];        /* 0x53B17 */
extern uint32 data_fd2_resource_last_loaded_resource_size;              /* 0x53BFF */
extern uint32 data_fd2_field_map_tile_event_consumed_flags_ptr;         /* 0x53AD5 */

/* ---- battle turn counter (serialized to FD2.SAV header +0x30C3) ---- */
extern uint32 data_fd2_battle_turn_counter;                             /* 0x53BEF */

/* ---- save/load ---- */
extern uint32 data_fd2_shared_party_total_gold;                         /* 0x53BF3 */
extern uint32 data_fd2_shared_menu_party_roster_buffer_ptr;             /* 0x53BF7 */
extern uint32 data_fd2_shared_menu_party_member_count;                  /* 0x53BFB */

/* ---- AIL function pointers ---- */
extern uint32 data_ail_alloc_fnptr;                                     /* 0x52758 */
extern uint32 data_ail_free_fnptr;                                      /* 0x5275C */

/* ---- audio ---- */
extern uint8  data_fd2_audio_bgm_last_set_track_id;                     /* 0x51A11 */
extern uint8  data_fd2_audio_bgm_enabled_flag;                          /* 0x51E61 */
extern uint8  data_fd2_audio_sfx_enabled_flag;                          /* 0x51E62 */
extern uint8  data_fd2_audio_per_chapter_player_turn_bgm_track[30];     /* 0x51E63 */
extern uint8  data_fd2_audio_per_chapter_enemy_turn_bgm_track[30];      /* 0x51E81 */
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
extern uint8  data_fd2_audio_walk_step_sfx_cadence_counter;             /* 0x540FE */
extern char   data_fd2_string_fdmus_dat[];                              /* 0x51A79  "FDMUS.DAT" */

/* ---- animation tables (.object2 const) ---- */
extern uint8  data_fd2_audio_footstep_sfx_per_job_cadence_class_table[]; /* 0x52618  29B */
extern uint8  data_fd2_animation_status_overlay_flicker_color_template[32]; /* 0x51F15 */
extern uint8  data_fd2_animation_spell_sprite_offset_table[33];         /* 0x51F33 */
extern uint8  data_fd2_animation_spell_frame_count_table[33];           /* 0x51F54 */
extern uint8  data_fd2_animation_spell_sfx_frame_table[33];             /* 0x51F75 */
extern uint8  data_fd2_animation_spell_overlay_blink_mask_table[28];    /* 0x52006 */
extern uint8  data_fd2_battle_summon_minor_anim_state5_frame_counter;    /* 0x540FA */
extern uint8  data_fd2_battle_summon_minor_anim_alternating_blit_toggle; /* 0x540FB */
extern uint32 data_fd2_audio_summon_spell_sfx_bank_buf_ptr;             /* 0x5411F */
extern int32  data_fd2_battle_summon_spell_shared_15slot_frame_counter_array[15]; /* 0x53F42 */
extern uint8  data_fd2_battle_summon_spell_8slot_visibility_table[7];            /* 0x523E1 */
extern uint32 data_fd2_battle_summon_spell_8slot_y_offset_table[7];              /* 0x523E8 */
extern int32  data_fd2_battle_summon_spell_8slot_row_multiplier_table[7];        /* 0x52404 */
extern int32  data_fd2_battle_summon_main_anim_12slot_frame_counter_array[12];    /* 0x53F81 */
extern int32  data_fd2_battle_summon_main_anim_12slot_color_idx_array[12];       /* 0x53FB1 */
extern uint8  data_fd2_battle_summon_main_anim_color_rotation_counter;           /* 0x53FE1 */
extern uint8  data_fd2_battle_summon_main_anim_terminate_flag;                   /* 0x53FE2 */
extern uint8  data_fd2_battle_summon_main_anim_odd_even_frame_toggle;            /* 0x53FE3 */
extern int32  data_fd2_battle_summon_main_anim_12slot_y_offset_table[12];        /* 0x52460 */
extern uint8  data_fd2_battle_summon_main_anim_12color_v_offset_table[12];       /* 0x52490 */
extern uint8  data_fd2_battle_summon_main_anim_12color_sprite_offset_table[12];  /* 0x5249C */
extern uint8  data_fd2_battle_summon_spell_anim_phase_byte;                      /* 0x53F7E */
extern uint8  data_fd2_battle_summon_spell_anim_aux_state_byte_unread;           /* 0x53F7F */
extern uint8  data_fd2_battle_summon_spell_sprite_anim_tick_counter;             /* 0x53F80 */
extern int32  data_fd2_battle_summon_anim_variant_a_6slot_frame_counter_array[6]; /* 0x53FE4 */
extern int32  data_fd2_battle_summon_anim_variant_a_6slot_color_idx_array[6];    /* 0x53FFC */
extern uint8  data_fd2_battle_summon_anim_variant_a_6slot_jitter_byte_array[6];  /* 0x54014 */
extern uint8  data_fd2_battle_summon_anim_variant_a_color_rotation_counter;      /* 0x5401A */
extern uint8  data_fd2_battle_summon_anim_variant_a_terminate_flag;              /* 0x5401B */
extern int32  data_fd2_battle_summon_anim_variant_a_10color_y_offset_table[10];  /* 0x524A8 */
extern int32  data_fd2_battle_summon_anim_variant_b_6slot_frame_counter_array[6]; /* 0x5401C */
extern int32  data_fd2_battle_summon_anim_variant_b_6slot_color_idx_array[6];    /* 0x54034 */
extern uint8  data_fd2_battle_summon_anim_variant_b_6slot_jitter_byte_array[6];  /* 0x5404C */
extern uint8  data_fd2_battle_summon_anim_variant_b_color_rotation_counter;      /* 0x54052 */
extern uint8  data_fd2_battle_summon_anim_variant_b_terminate_flag;              /* 0x54053 */
extern int32  data_fd2_battle_summon_anim_variant_b_10color_y_offset_table[10];  /* 0x524D0 */
extern int32  data_fd2_battle_summon_anim_variant_d_4slot_frame_counter_array[4]; /* 0x54097 */
extern int32  data_fd2_battle_summon_anim_variant_d_4slot_color_idx_array[4];    /* 0x540A7 */
extern uint8  data_fd2_battle_summon_anim_variant_d_color_rotation_counter;      /* 0x540B7 */
extern uint8  data_fd2_battle_summon_anim_variant_d_terminate_flag;              /* 0x540B8 */
extern uint8  data_fd2_battle_summon_anim_variant_d_odd_even_frame_toggle;       /* 0x540B9 */
extern int32  data_fd2_animation_summon_variant_d_3slot_color_row_offsets[10];   /* 0x52511 */
extern uint8  data_fd2_animation_summon_variant_e_16slot_sprite_base_table[16]; /* 0x52539 */
extern int32  data_fd2_battle_summon_anim_variant_e_16slot_frame_counter_array[16]; /* 0x540BA */
extern uint32 data_fd2_battle_summon_spell_palette_r_table;             /* 0x5254F */
extern uint32 data_fd2_battle_summon_spell_palette_g_table;             /* 0x52553 */
extern uint32 data_fd2_battle_summon_spell_palette_b_table;             /* 0x52557 */

/* ---- dispatch tables (.object2 const) ---- */
extern void (*data_fd2_chapter_init_handler_table[30])(void);           /* 0x51D71 */
extern void (*data_fd2_chapter_end_handler_table[30])(void);            /* 0x51DE9 */
extern void (*data_fd2_chapter_post_action_handler_table[30])(uint32);  /* 0x51B19 */
extern void (*data_fd2_battle_ai_post_action_consequence_table[90])(uint32); /* 0x51B91 */
extern void (*data_fd2_battle_spell_handler_table[28])(uint32, uint32, uint8 *); /* 0x51D01 */
extern void (*data_fd2_battle_spell_cast_cinematic_phase_handler_table[10])(void); /* 0x523B9 */
extern uint16 data_fd2_animation_ani_decoder_target_width;              /* 0x52760 */
extern uint32 data_fd2_animation_ani_decoder_dst_buf;                   /* 0x52762 */
extern uint32 data_fd2_animation_ani_decoder_src_buf;                   /* 0x52766 */
extern void  *data_fd2_animation_ani_decoder_frame_dispatch_table[10];  /* 0x5276A */

/* ---- .object3 data tables ---- */
extern item_effect       data_fd2_battle_item_effect_table[215];        /* 0x602AC */
extern spell_effect      data_fd2_battle_spell_effect_table[36];        /* 0x619FD */
extern enemy_data        data_fd2_battle_enemy_data_table[68];          /* 0x61AF9 */
extern character_base    data_fd2_battle_character_base_table[32];      /* 0x61DA1 */
extern character_growth  data_fd2_battle_character_growth_table[68];    /* 0x620A1 */

/* ---- input state ---- */
extern uint16 data_fd2_input_idle_current_bios_tick_word;               /* 0x539F0 */
extern uint16 data_fd2_input_idle_last_rendered_tick_word;               /* 0x539F2 */
extern uint8  data_fd2_input_last_key_pressed;                          /* 0x53A8D */
extern uint8  data_fd2_input_key_input_mode;                            /* 0x53A8E */

/* ---- timing state ---- */
extern uint32 data_fd2_engine_wait_one_bios_tick_last_seen;             /* 0x53A0C */
extern uint32 data_fd2_engine_wait_n_bios_ticks_last_seen;              /* 0x53A2C */

/* ---- tile attribute modifier tables (.object2 const) ---- */
extern uint32 data_fd2_battle_tile_attr_mv_modifier_table[];            /* 0x51A12 */
extern uint32 data_fd2_battle_tile_attr_def_modifier_table[];           /* 0x51A2A */

/* ---- RNG ---- */
extern uint16 data_fd2_shared_rng_seed;                                 /* 0x627B8 (.object3) */

/* ---- battle misc const tables ---- */
extern uint32 data_fd2_battle_job_magic_resist_table[27];               /* 0x51F96 */
extern uint8  data_fd2_battle_job_crit_rate_table[27];                  /* 0x5239B */

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
extern uint8  data_fd2_battle_pathfind_step_stack[];                     /* 0x60079  8B per frame */
extern uint8  data_fd2_class_promotion_data_table[];                    /* 0x615FE  2B per entry */
extern uint8  data_fd2_movement_cost_table[];                           /* 0x61646  20B per job */
extern uint8  data_fd2_job_allowed_items_table[27 * 7];                 /* 0x6188A  7B per job, 27 jobs */

/* ---- .object3 pointer tables ---- */
extern void  *data_fd2_battle_weapon_attack_anim_pattern_ptr_table_21[21]; /* 0x61955 */
extern void  *data_fd2_chapter_cutscene_event_script_ptr_table_106[106];   /* 0x627D8 */

/* ---- .object3 additional tables ---- */
extern uint8  data_fd2_chapter_intro_metadata_table[];                  /* 0x6238D  31B per ch */
extern uint8  data_fd2_spell_learning_table[];                          /* 0x626B3  12B per entry */
extern uint8  data_fd2_orphan_table_60181[];                           /* 0x60181  3B per entry (orphan) */

#endif /* GLOBALS_H */
