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
extern void run_battle_btlrng_tests(void);
extern void run_save_savecsum_tests(void);
extern void run_table_table_tests(void);
/* <<< GENBUILD externs <<< */

int main(void)
{
    printf("FD2 Unit Test Runner\n");
    printf("========================================\n\n");

    /* >>> GENBUILD calls >>> */
    run_battle_btlrng_tests();
    run_save_savecsum_tests();
    run_table_table_tests();
/* <<< GENBUILD calls <<< */

    test_print_summary();

    return g_test_fail_count > 0 ? 1 : 0;
}
