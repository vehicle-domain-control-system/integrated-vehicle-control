/* SW2 debounce: one sample every 4 ticks, 4 equal samples to accept a level. */

#include "test_util.h"
#include "hal_mock.h"

#include "../../src/driver/input/sw2_button.h"

/* Runs Process once per tick from 'from' up to and including 'to'. */
static void RunTicks(uint32_t from, uint32_t to)
{
    uint32_t t;

    for (t = from; t <= to; ++t)
    {
        Sw2Button_Process(t);
    }
}

static void test_press_after_four_stable_samples(void)
{
    HalMock_Reset();
    Sw2Button_Init(100u);
    HalMock_SetSw2(true);

    /* Samples at 104, 108, 112: three equal samples are not enough. */
    RunTicks(101u, 115u);
    CHECK(!Sw2Button_TakePressEvent());
    /* Fourth sample at 116. */
    RunTicks(116u, 116u);
    CHECK(Sw2Button_TakePressEvent());
    CHECK(!Sw2Button_TakePressEvent());

    /* Holding produces no further press; release produces none either. */
    RunTicks(117u, 200u);
    HalMock_SetSw2(false);
    RunTicks(201u, 260u);
    CHECK(!Sw2Button_TakePressEvent());

    /* Second press. */
    HalMock_SetSw2(true);
    RunTicks(261u, 300u);
    CHECK(Sw2Button_TakePressEvent());
}

static void test_no_sample_within_four_ticks(void)
{
    HalMock_Reset();
    Sw2Button_Init(0u);
    HalMock_SetSw2(true);
    /* Calling every tick or only on sample ticks gives the same result. */
    Sw2Button_Process(3u);
    Sw2Button_Process(4u);
    Sw2Button_Process(7u);
    Sw2Button_Process(8u);
    Sw2Button_Process(12u);
    CHECK(!Sw2Button_TakePressEvent());
    Sw2Button_Process(15u);
    CHECK(!Sw2Button_TakePressEvent());
    Sw2Button_Process(16u);
    CHECK(Sw2Button_TakePressEvent());
}

static void test_chatter_is_ignored(void)
{
    uint32_t t;
    bool level = false;

    HalMock_Reset();
    Sw2Button_Init(0u);
    /* The level changes between every sample: never stable. */
    for (t = 4u; t <= 200u; t += 4u)
    {
        level = !level;
        HalMock_SetSw2(level);
        Sw2Button_Process(t);
    }
    CHECK(!Sw2Button_TakePressEvent());
}

static void test_held_at_boot_is_not_a_press(void)
{
    HalMock_Reset();
    HalMock_SetSw2(true);
    Sw2Button_Init(0u);
    RunTicks(1u, 100u);
    CHECK(!Sw2Button_TakePressEvent());

    HalMock_SetSw2(false);
    RunTicks(101u, 150u);
    HalMock_SetSw2(true);
    RunTicks(151u, 200u);
    CHECK(Sw2Button_TakePressEvent());
}

static void test_tick_wraparound(void)
{
    HalMock_Reset();
    Sw2Button_Init(0xFFFFFFF0u);
    HalMock_SetSw2(true);
    {
        uint32_t i;

        /* Samples at 0xFFFFFFF4, 0xFFFFFFF8, 0xFFFFFFFC and 0 (after the wrap). */
        for (i = 1u; i <= 15u; ++i)
        {
            Sw2Button_Process(0xFFFFFFF0u + i);
        }
        CHECK(!Sw2Button_TakePressEvent());
        Sw2Button_Process(0u);
        CHECK(Sw2Button_TakePressEvent());
    }
}

int main(void)
{
    test_press_after_four_stable_samples();
    test_no_sample_within_four_ticks();
    test_chatter_is_ignored();
    test_held_at_boot_is_not_a_press();
    test_tick_wraparound();
    TEST_FINISH();
}
