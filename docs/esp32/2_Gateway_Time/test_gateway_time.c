#include <assert.h>
#include <stdint.h>

#include "Gateway_Time.h"

static void test_elapsed_normal(void)
{
    assert(Gateway_Time_ElapsedMs(100U, 150U) == 50U);
}

static void test_elapsed_wraparound(void)
{
    const Gateway_TimeMs_t start_ms = UINT32_MAX - 9U;
    const Gateway_TimeMs_t now_ms = 15U;

    /*
     * Sequence:
     * start = 0xFFFFFFF6
     * ... 10 ticks to wrap at 0
     * ... 15 additional ticks
     * elapsed = 25 ms
     */
    assert(Gateway_Time_ElapsedMs(start_ms, now_ms) == 25U);
}

static void test_timeout(void)
{
    assert(Gateway_Time_HasElapsed(100U, 50U, 149U) == false);
    assert(Gateway_Time_HasElapsed(100U, 50U, 150U) == true);
    assert(Gateway_Time_HasElapsed(100U, 0U, 100U) == true);
}

static void test_timeout_wraparound(void)
{
    const Gateway_TimeMs_t start_ms = UINT32_MAX - 9U;

    assert(Gateway_Time_HasElapsed(start_ms, 25U, 14U) == false);
    assert(Gateway_Time_HasElapsed(start_ms, 25U, 15U) == true);
}

int main(void)
{
    test_elapsed_normal();
    test_elapsed_wraparound();
    test_timeout();
    test_timeout_wraparound();

    return 0;
}
