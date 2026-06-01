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
extern void run_table_tests(void);
extern void run_battle_tests(void);
extern void run_ui_tests(void);
extern void run_spell_tests(void);
extern void run_anim_tests(void);

int main(void)
{
    printf("FD2 Unit Test Runner\n");
    printf("========================================\n\n");

    run_table_tests();
    run_battle_tests();
    run_ui_tests();
    run_spell_tests();
    run_anim_tests();

    test_print_summary();

    return g_test_fail_count > 0 ? 1 : 0;
}
