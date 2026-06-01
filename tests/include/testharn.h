#ifndef TESTHARN_H
#define TESTHARN_H

#include <stdio.h>
#include <string.h>

extern int g_test_pass_count;
extern int g_test_fail_count;
extern const char *g_current_test_name;

#define TEST_BEGIN(name) \
    g_current_test_name = #name; \
    printf("  TEST %s ... ", #name);

#define TEST_PASS() \
    do { g_test_pass_count++; printf("PASS\n"); } while(0)

#define ASSERT_EQ(actual, expected) \
    do { \
        long _a = (long)(actual); \
        long _e = (long)(expected); \
        if (_a != _e) { \
            printf("FAIL\n    %s:%d: expected %ld, got %ld\n", \
                   __FILE__, __LINE__, _e, _a); \
            g_test_fail_count++; \
            return; \
        } \
    } while(0)

#define ASSERT_NE(actual, not_expected) \
    do { \
        long _a = (long)(actual); \
        long _n = (long)(not_expected); \
        if (_a == _n) { \
            printf("FAIL\n    %s:%d: got unexpected %ld\n", \
                   __FILE__, __LINE__, _a); \
            g_test_fail_count++; \
            return; \
        } \
    } while(0)

#define ASSERT_MEM_EQ(ptr_a, ptr_b, size) \
    do { \
        if (memcmp((ptr_a), (ptr_b), (size)) != 0) { \
            printf("FAIL\n    %s:%d: memory mismatch (%u bytes)\n", \
                   __FILE__, __LINE__, (unsigned)(size)); \
            g_test_fail_count++; \
            return; \
        } \
    } while(0)

#define ASSERT_TRUE(cond) \
    do { \
        if (!(cond)) { \
            printf("FAIL\n    %s:%d: condition false\n", \
                   __FILE__, __LINE__); \
            g_test_fail_count++; \
            return; \
        } \
    } while(0)

#define RUN_TEST(fn) \
    do { TEST_BEGIN(fn); fn(); \
         if (g_test_fail_count == _prev_fails) TEST_PASS(); \
         _prev_fails = g_test_fail_count; \
    } while(0)

#define SUITE_BEGIN(name) \
    do { \
        int _prev_fails = g_test_fail_count; \
        printf("Suite: %s\n", #name);

#define SUITE_END() \
    } while(0)

void test_print_summary(void);

#endif /* TESTHARN_H */
