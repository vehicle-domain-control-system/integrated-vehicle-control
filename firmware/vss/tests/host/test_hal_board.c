/* Board bring-up adapter against the recording vendor-driver stand-in: call
 * order, the bounded PLL lock poll before the PLL output is distributed, and
 * the result summary. Linked with hal_board_s32k344.c and
 * tests/rtd_mock/rtd_mock.c. */

#include "test_util.h"

#include "../rtd_mock/rtd_mock.h"
#include "../../src/hal/include/hal_board.h"
#include "../../src/hal/s32k344/hal_s32k344_cfg.h"

static bool CallIs(uint32 index, RtdMockCallKind kind, const void *ptr, uint32 a)
{
    const RtdMockCall *c = RtdMock_Call(index);

    return (c != NULL) && (c->kind == kind) && (c->ptr == (uintptr_t)ptr) && (c->a == a);
}

/* Expects Clock_Ip_Init, pllReads lock status reads, then the rest of the
 * bring-up in the verified order. */
static void ExpectSequence(uint32 pllReads, int line)
{
    uint32 i;
    uint32 n = 0U;
    bool ok = true;

    ok = ok && CallIs(n++, RTD_MOCK_CLOCK_INIT, &Clock_Ip_aClockConfig[0], 0U);
    for (i = 0U; i < pllReads; ++i)
    {
        ok = ok && CallIs(n++, RTD_MOCK_CLOCK_GET_PLL_STATUS, NULL, 0U);
    }
    ok = ok && CallIs(n++, RTD_MOCK_CLOCK_DISTRIBUTE_PLL, NULL, 0U);
    ok = ok && CallIs(n++, RTD_MOCK_PORT_INIT, g_pin_mux_InitConfigArr_PortContainer_0_VS_0,
                      NUM_OF_CONFIGURED_PINS_PortContainer_0_VS_0);
    ok = ok && CallIs(n++, RTD_MOCK_INTCTRL_INIT, &intCtrlConfig, 0U);
    ok = ok && CallIs(n++, RTD_MOCK_LPI2C_MASTER_INIT, &I2c_Lpi2cMaster_HwChannel0_Channel0_VS_0, 0U);
    ok = ok && (RtdMock_CallCount() == n);
    if (ok)
    {
        ++g_testPassed;
    }
    else
    {
        ++g_testFailed;
        printf("FAIL line %d: %u calls recorded, %u lock status reads expected\n", line,
               (unsigned)RtdMock_CallCount(), (unsigned)pllReads);
    }
}

static void test_locked_at_first_read(void)
{
    RtdMock_Reset();
    CHECK(HalBoard_Init());
    ExpectSequence(1U, __LINE__);
}

/* The vendor driver asks for the lock status to be polled until locked
 * before the PLL output is distributed. */
static void test_slow_lock_is_polled(void)
{
    RtdMock_Reset();
    RtdMock_SetPllStatus(HAL_BOARD_PLL_STATUS_READS - 1U, CLOCK_IP_PLL_LOCKED);
    CHECK(HalBoard_Init());
    ExpectSequence(HAL_BOARD_PLL_STATUS_READS, __LINE__);
}

/* The poll is bounded; every later step still runs and the result reports
 * the failure. */
static void test_lock_never_reported(void)
{
    RtdMock_Reset();
    RtdMock_SetPllStatus(1000U, CLOCK_IP_PLL_LOCKED);
    CHECK(!HalBoard_Init());
    ExpectSequence(HAL_BOARD_PLL_STATUS_READS, __LINE__);

    RtdMock_Reset();
    RtdMock_SetPllStatus(0U, CLOCK_IP_PLL_STATUS_UNDEFINED);
    CHECK(!HalBoard_Init());
    ExpectSequence(HAL_BOARD_PLL_STATUS_READS, __LINE__);
}

static void test_each_init_failure_is_reported(void)
{
    RtdMock_Reset();
    RtdMock_SetClockInitResult(CLOCK_IP_ERROR);
    CHECK(!HalBoard_Init());
    ExpectSequence(1U, __LINE__);

    RtdMock_Reset();
    RtdMock_SetPortInitResult(SIUL2_PORT_ERROR);
    CHECK(!HalBoard_Init());
    ExpectSequence(1U, __LINE__);

    RtdMock_Reset();
    RtdMock_SetIntCtrlInitResult(INTCTRL_IP_STATUS_ERROR);
    CHECK(!HalBoard_Init());
    ExpectSequence(1U, __LINE__);
}

int main(void)
{
    test_locked_at_first_read();
    test_slow_lock_is_polled();
    test_lock_never_reported();
    test_each_init_failure_is_reported();
    TEST_FINISH();
}
