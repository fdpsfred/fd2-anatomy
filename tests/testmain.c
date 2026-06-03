#include <stdio.h>
#include "testharn.h"

int g_test_pass_count = 0;
int g_test_fail_count = 0;
const char *g_current_test_name = "";

void test_print_summary(void)
{
    printf("\n========================================\n");
    printf("Results: %d passed, %d failed\n",
           g_test_pass_count, g_test_fail_count);
    printf("========================================\n");
}

/* Forward declarations for test suites */
/* >>> GENBUILD externs >>> */
extern void run_anim_anidec_tests(void);
extern void run_anim_anisummn1_tests(void);
extern void run_anim_anisummn2_tests(void);
extern void run_anim_aniui_tests(void);
extern void run_anim_aniwalk1_tests(void);
extern void run_anim_aniwalk2_tests(void);
extern void run_audio_audio_tests(void);
extern void run_battle_battle1_tests(void);
extern void run_battle_battle2_tests(void);
extern void run_battle_btl_ai_tests(void);
extern void run_battle_btl_aisc1_tests(void);
extern void run_battle_btl_aisc2_tests(void);
extern void run_battle_btl_aitg_tests(void);
extern void run_battle_btl_init_tests(void);
extern void run_battle_btl_turn_tests(void);
extern void run_dialog_dialog_tests(void);
extern void run_field_chtrans_tests(void);
extern void run_gfx_blitspr_tests(void);
extern void run_gfx_blittile_tests(void);
extern void run_gfx_palette_tests(void);
extern void run_gfx_rndscene_tests(void);
extern void run_gfx_rndstat_tests(void);
extern void run_input_input_tests(void);
extern void run_life_main_tests(void);
extern void run_rsrc_rsrc_tests(void);
extern void run_save_save_tests(void);
extern void run_spell_spell_tests(void);
extern void run_spell_spelleff_tests(void);
extern void run_table_table_tests(void);
extern void run_ui_menu_cursor_tests(void);
extern void run_ui_menu_menu_tests(void);
extern void run_ui_menu_status_tests(void);
extern void run_util_pathfnd_tests(void);
/* <<< GENBUILD externs <<< */

int main(void)
{
    printf("FD2 Unit Test Runner\n");
    printf("========================================\n\n");

    /* >>> GENBUILD calls >>> */
    run_anim_anidec_tests();
    run_anim_anisummn1_tests();
    run_anim_anisummn2_tests();
    run_anim_aniui_tests();
    run_anim_aniwalk1_tests();
    run_anim_aniwalk2_tests();
    run_audio_audio_tests();
    run_battle_battle1_tests();
    run_battle_battle2_tests();
    run_battle_btl_ai_tests();
    run_battle_btl_aisc1_tests();
    run_battle_btl_aisc2_tests();
    run_battle_btl_aitg_tests();
    run_battle_btl_init_tests();
    run_battle_btl_turn_tests();
    run_dialog_dialog_tests();
    run_field_chtrans_tests();
    run_gfx_blitspr_tests();
    run_gfx_blittile_tests();
    run_gfx_palette_tests();
    run_gfx_rndscene_tests();
    run_gfx_rndstat_tests();
    run_input_input_tests();
    run_life_main_tests();
    run_rsrc_rsrc_tests();
    run_save_save_tests();
    run_spell_spell_tests();
    run_spell_spelleff_tests();
    run_table_table_tests();
    run_ui_menu_cursor_tests();
    run_ui_menu_menu_tests();
    run_ui_menu_status_tests();
    run_util_pathfnd_tests();
/* <<< GENBUILD calls <<< */

    test_print_summary();

    return g_test_fail_count > 0 ? 1 : 0;
}
