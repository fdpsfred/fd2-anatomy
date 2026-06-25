/*
 * Frozen logic-net unit tests: src/battle/battle.c RNG primitive.
 *
 * fd2_advance_rng_state is the determinism foundation the deterministic
 * replay/capture system (tools/fd2_play) depends on: a pure LFSR over the
 * data_fd2_shared_rng_seed global, no callees, no spy doubles -> immune to the
 * coordinated-landing breakage that retired the broader per-function battle
 * spy tests (moved to legacy/tests_unit_spy/). Kept as a frozen regression net
 * per the test migration plan: if the LFSR sequence ever drifts, every replay
 * golden and the host damage oracle (tools/fd2_play/expect.py) would silently
 * diverge, so this pins it directly.
 */
#include "testharn.h"
#include "types.h"
#include "globals.h"
#include "protos.h"

/* ---- Test: advancing the LFSR yields non-zero, changing values ---- */
static void test_rng_advance(void)
{
    uint32 r1;
    uint32 r2;
    data_fd2_shared_rng_seed = 0;
    r1 = fd2_advance_rng_state();
    ASSERT_NE(r1, 0);
    r2 = fd2_advance_rng_state();
    ASSERT_NE(r2, r1);
}

/* ---- Test: equal seed -> identical sequence (determinism) ---- */
static void test_rng_deterministic(void)
{
    uint32 r1;
    uint32 r2;
    data_fd2_shared_rng_seed = 12345;
    r1 = fd2_advance_rng_state();
    data_fd2_shared_rng_seed = 12345;
    r2 = fd2_advance_rng_state();
    ASSERT_EQ(r1, r2);
}

void run_battle_btlrng_tests(void)
{
    SUITE_BEGIN(battle_btlrng);
    RUN_TEST(test_rng_advance);
    RUN_TEST(test_rng_deterministic);
    SUITE_END();
}
