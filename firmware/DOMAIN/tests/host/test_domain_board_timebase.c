#include "domain_board_timebase.h"
#include "domain_runtime.h"

#include <limits.h>
#include <stdio.h>
#include <string.h>

typedef struct
{
    uint32_t ticks;
    bool available;
    uint32_t calls;
} FakeCounter_t;

static int g_failures;
static int g_tests;

#define CHECK(condition)                                                       \
    do                                                                         \
    {                                                                          \
        if (!(condition))                                                      \
        {                                                                      \
            (void)printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #condition); \
            ++g_failures;                                                      \
        }                                                                      \
    } while (0)

static void BeginTest(const char *name)
{
    ++g_tests;
    (void)printf("%s: ", name);
}

static void EndTest(int failuresBefore)
{
    (void)printf((g_failures == failuresBefore) ? "PASS\n" : "FAIL\n");
}

static bool FakeCounter_Read(void *context, uint32_t *outTicks)
{
    FakeCounter_t *counter = (FakeCounter_t *)context;

    if ((counter == NULL) || (outTicks == NULL))
    {
        return false;
    }
    counter->calls += 1U;
    if (!counter->available)
    {
        return false;
    }
    *outTicks = counter->ticks;
    return true;
}

static DomainRuntime_Config_t RuntimeConfig(
    DomainBoardTimebase_t *timebase)
{
    DomainRuntime_Config_t config;

    (void)memset(&config, 0, sizeof(config));
    config.timeProvider = DomainBoardTimebase_GetNowMs;
    config.timeProviderContext = timebase;
    config.firstCommandId = 1U;
    config.resultPolicy.firstResponseTimeoutEnabled = true;
    config.resultPolicy.firstResponseTimeoutMs = 100U;
    config.resultPolicy.finalResultTimeoutEnabled = true;
    config.resultPolicy.finalResultTimeoutMs = 1000U;
    config.resultPolicy.doorEvidenceTtlEnabled = true;
    config.resultPolicy.doorEvidenceTtlMs = 500U;
    config.permissionPolicy.reevaluationPeriodMs = 200U;
    config.permissionPolicy.sourceValidityMs = 500U;
    config.cisOccupantEnabled = true;
    config.cisOccupantPolicy.presenceTtlMs = 500U;
    config.cisOccupantPolicy.countTtlMs = 500U;
    config.stateQueryPolicy.bcmDoorTtlMs = 500U;
    config.stateQueryPolicy.bcmStatusTtlMs = 500U;
    config.stateQueryPolicy.cisRearTtlMs = 500U;
    config.stateQueryPolicy.cisStatusTtlMs = 500U;
    return config;
}

static void Test_PreInitAndNull(void)
{
    DomainBoardTimebase_t timebase;
    uint32_t output = 77U;
    int before = g_failures;

    BeginTest("Test_PreInitAndNull");
    (void)memset(&timebase, 0, sizeof(timebase));
    CHECK(!DomainBoardTimebase_IsInitialized(NULL));
    CHECK(!DomainBoardTimebase_IsInitialized(&timebase));
    CHECK(!DomainBoardTimebase_GetNowMs(NULL, &output));
    CHECK(!DomainBoardTimebase_GetNowMs(&timebase, &output));
    CHECK(!DomainBoardTimebase_GetNowMs(&timebase, NULL));
    CHECK(output == 77U);
    EndTest(before);
}

static void Test_InitValidationAndFailurePreservesContext(void)
{
    DomainBoardTimebase_t timebase;
    DomainBoardTimebase_t saved;
    DomainBoardTimebase_Config_t config;
    FakeCounter_t counter = {100U, false, 0U};
    int before = g_failures;

    BeginTest("Test_InitValidationAndFailurePreservesContext");
    (void)memset(&timebase, 0, sizeof(timebase));
    saved = timebase;
    (void)memset(&config, 0, sizeof(config));
    CHECK(DomainBoardTimebase_Init(NULL, &config) ==
          DOMAIN_BOARD_TIMEBASE_INVALID_ARGUMENT);
    CHECK(DomainBoardTimebase_Init(&timebase, NULL) ==
          DOMAIN_BOARD_TIMEBASE_INVALID_ARGUMENT);
    CHECK(DomainBoardTimebase_Init(&timebase, &config) ==
          DOMAIN_BOARD_TIMEBASE_INVALID_ARGUMENT);
    CHECK(memcmp(&timebase, &saved, sizeof(timebase)) == 0);

    config.tickReader = FakeCounter_Read;
    config.tickReaderContext = &counter;
    config.ticksPerMillisecond = 10U;
    CHECK(DomainBoardTimebase_Init(&timebase, &config) ==
          DOMAIN_BOARD_TIMEBASE_TICK_UNAVAILABLE);
    CHECK(memcmp(&timebase, &saved, sizeof(timebase)) == 0);
    CHECK(counter.calls == 1U);
    EndTest(before);
}

static void Test_MillisecondConversionAndRepeatedReads(void)
{
    DomainBoardTimebase_t timebase;
    DomainBoardTimebase_Config_t config;
    FakeCounter_t counter = {100U, true, 0U};
    uint32_t output = UINT32_MAX;
    int before = g_failures;

    BeginTest("Test_MillisecondConversionAndRepeatedReads");
    (void)memset(&timebase, 0, sizeof(timebase));
    config.tickReader = FakeCounter_Read;
    config.tickReaderContext = &counter;
    config.ticksPerMillisecond = 10U;
    CHECK(DomainBoardTimebase_Init(&timebase, &config) ==
          DOMAIN_BOARD_TIMEBASE_OK);
    CHECK(DomainBoardTimebase_IsInitialized(&timebase));
    CHECK(DomainBoardTimebase_GetNowMs(&timebase, &output));
    CHECK(output == 0U);

    counter.ticks = 109U;
    CHECK(DomainBoardTimebase_GetNowMs(&timebase, &output));
    CHECK(output == 0U);
    counter.ticks = 110U;
    CHECK(DomainBoardTimebase_GetNowMs(&timebase, &output));
    CHECK(output == 1U);
    counter.ticks = 135U;
    CHECK(DomainBoardTimebase_GetNowMs(&timebase, &output));
    CHECK(output == 3U);
    counter.ticks = 140U;
    CHECK(DomainBoardTimebase_GetNowMs(&timebase, &output));
    CHECK(output == 4U);
    EndTest(before);
}

static void Test_ReadFailureIsAtomic(void)
{
    DomainBoardTimebase_t timebase;
    DomainBoardTimebase_t saved;
    DomainBoardTimebase_Config_t config;
    FakeCounter_t counter = {500U, true, 0U};
    uint32_t output = 123U;
    int before = g_failures;

    BeginTest("Test_ReadFailureIsAtomic");
    (void)memset(&timebase, 0, sizeof(timebase));
    config.tickReader = FakeCounter_Read;
    config.tickReaderContext = &counter;
    config.ticksPerMillisecond = 20U;
    CHECK(DomainBoardTimebase_Init(&timebase, &config) ==
          DOMAIN_BOARD_TIMEBASE_OK);
    saved = timebase;
    counter.available = false;
    CHECK(!DomainBoardTimebase_GetNowMs(&timebase, &output));
    CHECK(output == 123U);
    CHECK(memcmp(&timebase, &saved, sizeof(timebase)) == 0);
    EndTest(before);
}

static void Test_TickCounterRollover(void)
{
    DomainBoardTimebase_t timebase;
    DomainBoardTimebase_Config_t config;
    FakeCounter_t counter = {UINT32_MAX - 4U, true, 0U};
    uint32_t output = 0U;
    int before = g_failures;

    BeginTest("Test_TickCounterRollover");
    (void)memset(&timebase, 0, sizeof(timebase));
    config.tickReader = FakeCounter_Read;
    config.tickReaderContext = &counter;
    config.ticksPerMillisecond = 5U;
    CHECK(DomainBoardTimebase_Init(&timebase, &config) ==
          DOMAIN_BOARD_TIMEBASE_OK);
    counter.ticks = 4U;
    CHECK(DomainBoardTimebase_GetNowMs(&timebase, &output));
    CHECK(output == 1U);
    CHECK(timebase.fractionalTicks == 4U);
    counter.ticks = 5U;
    CHECK(DomainBoardTimebase_GetNowMs(&timebase, &output));
    CHECK(output == 2U);
    CHECK(timebase.fractionalTicks == 0U);
    EndTest(before);
}

static void Test_MillisecondRollover(void)
{
    DomainBoardTimebase_t timebase;
    DomainBoardTimebase_Config_t config;
    FakeCounter_t counter = {1000U, true, 0U};
    uint32_t output = 0U;
    int before = g_failures;

    BeginTest("Test_MillisecondRollover");
    (void)memset(&timebase, 0, sizeof(timebase));
    config.tickReader = FakeCounter_Read;
    config.tickReaderContext = &counter;
    config.ticksPerMillisecond = 1U;
    CHECK(DomainBoardTimebase_Init(&timebase, &config) ==
          DOMAIN_BOARD_TIMEBASE_OK);
    timebase.nowMs = UINT32_MAX - 1U;
    counter.ticks = 1003U;
    CHECK(DomainBoardTimebase_GetNowMs(&timebase, &output));
    CHECK(output == 1U);
    EndTest(before);
}

static void Test_ReinitializationPreservesMonotonicEpoch(void)
{
    DomainBoardTimebase_t timebase;
    DomainBoardTimebase_t saved;
    DomainBoardTimebase_Config_t config;
    FakeCounter_t counter = {10U, true, 0U};
    uint32_t output = UINT32_MAX;
    int before = g_failures;

    BeginTest("Test_ReinitializationPreservesMonotonicEpoch");
    (void)memset(&timebase, 0, sizeof(timebase));
    config.tickReader = FakeCounter_Read;
    config.tickReaderContext = &counter;
    config.ticksPerMillisecond = 2U;
    CHECK(DomainBoardTimebase_Init(&timebase, &config) ==
          DOMAIN_BOARD_TIMEBASE_OK);
    counter.ticks = 20U;
    CHECK(DomainBoardTimebase_GetNowMs(&timebase, &output));
    CHECK(output == 5U);
    saved = timebase;
    CHECK(DomainBoardTimebase_Init(&timebase, &config) ==
          DOMAIN_BOARD_TIMEBASE_ALREADY_INITIALIZED);
    CHECK(memcmp(&timebase, &saved, sizeof(timebase)) == 0);
    counter.ticks = 22U;
    CHECK(DomainBoardTimebase_GetNowMs(&timebase, &output));
    CHECK(output == 6U);
    EndTest(before);
}

static void Test_RuntimeIntegration(void)
{
    DomainBoardTimebase_t timebase;
    DomainBoardTimebase_Config_t timebaseConfig;
    DomainRuntime_Config_t runtimeConfig;
    DomainRuntime_t runtime;
    DomainRuntime_Snapshot_t snapshot;
    FakeCounter_t counter = {400U, true, 0U};
    int before = g_failures;

    BeginTest("Test_RuntimeIntegration");
    (void)memset(&timebase, 0, sizeof(timebase));
    (void)memset(&runtime, 0, sizeof(runtime));
    timebaseConfig.tickReader = FakeCounter_Read;
    timebaseConfig.tickReaderContext = &counter;
    timebaseConfig.ticksPerMillisecond = 10U;
    CHECK(DomainBoardTimebase_Init(&timebase, &timebaseConfig) ==
          DOMAIN_BOARD_TIMEBASE_OK);
    runtimeConfig = RuntimeConfig(&timebase);
    CHECK(DomainRuntime_Init(&runtime, &runtimeConfig) == DOMAIN_RUNTIME_OK);
    counter.ticks = 425U;
    CHECK(DomainRuntime_Poll(&runtime) == DOMAIN_RUNTIME_OK);
    CHECK(DomainRuntime_GetSnapshot(&runtime, &snapshot) ==
          DOMAIN_RUNTIME_OK);
    CHECK(snapshot.lastPollAtMs == 2U);
    CHECK(snapshot.lastPollElapsedMs == 0U);
    counter.ticks = 440U;
    CHECK(DomainRuntime_Poll(&runtime) == DOMAIN_RUNTIME_OK);
    CHECK(DomainRuntime_GetSnapshot(&runtime, &snapshot) ==
          DOMAIN_RUNTIME_OK);
    CHECK(snapshot.lastPollAtMs == 4U);
    CHECK(snapshot.lastPollElapsedMs == 2U);
    EndTest(before);
}

int main(void)
{
    Test_PreInitAndNull();
    Test_InitValidationAndFailurePreservesContext();
    Test_MillisecondConversionAndRepeatedReads();
    Test_ReadFailureIsAtomic();
    Test_TickCounterRollover();
    Test_MillisecondRollover();
    Test_ReinitializationPreservesMonotonicEpoch();
    Test_RuntimeIntegration();

    (void)printf("%d tests, %d failures\n", g_tests, g_failures);
    return (g_failures == 0) ? 0 : 1;
}
