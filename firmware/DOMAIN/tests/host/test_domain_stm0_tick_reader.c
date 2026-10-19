#include "domain_stm0_tick_reader.h"
#include "domain_board_timebase.h"
#include "Stm_Ip.h"

#include <limits.h>
#include <stdio.h>
#include <string.h>

const Stm_Ip_InstanceConfigType STM_0_InitConfig_PB = {false, 0U};
const Stm_Ip_ChannelConfigType STM_0_ChannelConfig_PB[1U] =
{
    {0U, NULL, 0U, 0U}
};

static int g_failures;
static int g_tests;
static uint32_t g_counterValue;
static uint32_t g_initCalls;
static uint32_t g_channelInitCalls;
static uint32_t g_startCalls;
static uint32_t g_readCalls;
static uint8 g_lastInstance;
static uint32 g_lastStartValue;
static const Stm_Ip_InstanceConfigType *g_lastInstanceConfig;
static const Stm_Ip_ChannelConfigType *g_lastChannelConfig;

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

static void ResetRtdFake(void)
{
    g_counterValue = 0U;
    g_initCalls = 0U;
    g_channelInitCalls = 0U;
    g_startCalls = 0U;
    g_readCalls = 0U;
    g_lastInstance = UINT8_MAX;
    g_lastStartValue = UINT32_MAX;
    g_lastInstanceConfig = NULL;
    g_lastChannelConfig = NULL;
}

void Stm_Ip_Init(uint8 instance, const Stm_Ip_InstanceConfigType *configPtr)
{
    ++g_initCalls;
    g_lastInstance = instance;
    g_lastInstanceConfig = configPtr;
}

void Stm_Ip_InitChannel(uint8 instance, const Stm_Ip_ChannelConfigType *configPtr)
{
    ++g_channelInitCalls;
    g_lastInstance = instance;
    g_lastChannelConfig = configPtr;
}

void Stm_Ip_StartTimer(uint8 instance, uint32 startValue)
{
    ++g_startCalls;
    g_lastInstance = instance;
    g_lastStartValue = startValue;
}

uint32 Stm_Ip_GetCounterValue(uint8 instance)
{
    ++g_readCalls;
    g_lastInstance = instance;
    return g_counterValue;
}

static void Test_PreInitAndNull(void)
{
    DomainStm0TickReader_t reader;
    uint32_t output = 77U;
    int before = g_failures;

    BeginTest("Test_PreInitAndNull");
    ResetRtdFake();
    (void)memset(&reader, 0, sizeof(reader));
    CHECK(DomainStm0TickReader_Init(NULL, DOMAIN_STM0_INPUT_CLOCK_HZ) ==
          DOMAIN_STM0_TICK_READER_INVALID_ARGUMENT);
    CHECK(!DomainStm0TickReader_IsInitialized(NULL));
    CHECK(!DomainStm0TickReader_IsInitialized(&reader));
    CHECK(!DomainStm0TickReader_Read(NULL, &output));
    CHECK(!DomainStm0TickReader_Read(&reader, &output));
    CHECK(!DomainStm0TickReader_Read(&reader, NULL));
    CHECK(!DomainStm0TickReader_GetTicksPerMillisecond(&reader, &output));
    CHECK(output == 77U);
    CHECK(g_initCalls == 0U);
    EndTest(before);
}

static void Test_ClockMismatchIsAtomic(void)
{
    DomainStm0TickReader_t reader;
    DomainStm0TickReader_t saved;
    int before = g_failures;

    BeginTest("Test_ClockMismatchIsAtomic");
    ResetRtdFake();
    (void)memset(&reader, 0, sizeof(reader));
    saved = reader;
    CHECK(DomainStm0TickReader_Init(&reader, 0U) ==
          DOMAIN_STM0_TICK_READER_CLOCK_MISMATCH);
    CHECK(DomainStm0TickReader_Init(&reader, 40000000U) ==
          DOMAIN_STM0_TICK_READER_CLOCK_MISMATCH);
    CHECK(memcmp(&reader, &saved, sizeof(reader)) == 0);
    CHECK(g_initCalls == 0U);
    CHECK(g_startCalls == 0U);
    EndTest(before);
}

static void Test_InitUsesGeneratedStm0Configuration(void)
{
    DomainStm0TickReader_t reader;
    uint32_t ticksPerMillisecond = 0U;
    int before = g_failures;

    BeginTest("Test_InitUsesGeneratedStm0Configuration");
    ResetRtdFake();
    (void)memset(&reader, 0, sizeof(reader));
    CHECK(DomainStm0TickReader_Init(&reader, DOMAIN_STM0_INPUT_CLOCK_HZ) ==
          DOMAIN_STM0_TICK_READER_OK);
    CHECK(DomainStm0TickReader_IsInitialized(&reader));
    CHECK(reader.counterFrequencyHz == 48000000U);
    CHECK(DomainStm0TickReader_GetTicksPerMillisecond(
        &reader, &ticksPerMillisecond));
    CHECK(ticksPerMillisecond == 48000U);
    CHECK(g_initCalls == 1U);
    CHECK(g_channelInitCalls == 1U);
    CHECK(g_startCalls == 1U);
    CHECK(g_lastInstance == 0U);
    CHECK(g_lastInstanceConfig == &STM_0_InitConfig_PB);
    CHECK(g_lastChannelConfig == &STM_0_ChannelConfig_PB[0]);
    CHECK(g_lastStartValue == 0U);
    EndTest(before);
}

static void Test_ReadReturnsExactCounterAndSupportsRollover(void)
{
    DomainStm0TickReader_t reader;
    uint32_t output = 0U;
    int before = g_failures;

    BeginTest("Test_ReadReturnsExactCounterAndSupportsRollover");
    ResetRtdFake();
    (void)memset(&reader, 0, sizeof(reader));
    CHECK(DomainStm0TickReader_Init(&reader, DOMAIN_STM0_INPUT_CLOCK_HZ) ==
          DOMAIN_STM0_TICK_READER_OK);
    g_counterValue = UINT32_MAX;
    CHECK(DomainStm0TickReader_Read(&reader, &output));
    CHECK(output == UINT32_MAX);
    g_counterValue = 0U;
    CHECK(DomainStm0TickReader_Read(&reader, &output));
    CHECK(output == 0U);
    CHECK(g_readCalls == 2U);
    CHECK(g_lastInstance == 0U);
    EndTest(before);
}

static void Test_ReinitializationDoesNotRestartCounter(void)
{
    DomainStm0TickReader_t reader;
    DomainStm0TickReader_t saved;
    int before = g_failures;

    BeginTest("Test_ReinitializationDoesNotRestartCounter");
    ResetRtdFake();
    (void)memset(&reader, 0, sizeof(reader));
    CHECK(DomainStm0TickReader_Init(&reader, DOMAIN_STM0_INPUT_CLOCK_HZ) ==
          DOMAIN_STM0_TICK_READER_OK);
    saved = reader;
    CHECK(DomainStm0TickReader_Init(&reader, DOMAIN_STM0_INPUT_CLOCK_HZ) ==
          DOMAIN_STM0_TICK_READER_ALREADY_INITIALIZED);
    CHECK(memcmp(&reader, &saved, sizeof(reader)) == 0);
    CHECK(g_initCalls == 1U);
    CHECK(g_channelInitCalls == 1U);
    CHECK(g_startCalls == 1U);
    EndTest(before);
}

static void Test_DomainBoardTimebaseIntegration(void)
{
    DomainStm0TickReader_t reader;
    DomainBoardTimebase_t timebase;
    DomainBoardTimebase_Config_t timebaseConfig;
    uint32_t ticksPerMillisecond = 0U;
    uint32_t nowMs = UINT32_MAX;
    int before = g_failures;

    BeginTest("Test_DomainBoardTimebaseIntegration");
    ResetRtdFake();
    (void)memset(&reader, 0, sizeof(reader));
    (void)memset(&timebase, 0, sizeof(timebase));
    CHECK(DomainStm0TickReader_Init(&reader, DOMAIN_STM0_INPUT_CLOCK_HZ) ==
          DOMAIN_STM0_TICK_READER_OK);
    CHECK(DomainStm0TickReader_GetTicksPerMillisecond(
        &reader, &ticksPerMillisecond));
    g_counterValue = UINT32_MAX - 23999U;
    timebaseConfig.tickReader = DomainStm0TickReader_Read;
    timebaseConfig.tickReaderContext = &reader;
    timebaseConfig.ticksPerMillisecond = ticksPerMillisecond;
    CHECK(DomainBoardTimebase_Init(&timebase, &timebaseConfig) ==
          DOMAIN_BOARD_TIMEBASE_OK);
    g_counterValue = 24000U;
    CHECK(DomainBoardTimebase_GetNowMs(&timebase, &nowMs));
    CHECK(nowMs == 1U);
    EndTest(before);
}

int main(void)
{
    Test_PreInitAndNull();
    Test_ClockMismatchIsAtomic();
    Test_InitUsesGeneratedStm0Configuration();
    Test_ReadReturnsExactCounterAndSupportsRollover();
    Test_ReinitializationDoesNotRestartCounter();
    Test_DomainBoardTimebaseIntegration();

    (void)printf("%d tests, %d failures\n", g_tests, g_failures);
    return (g_failures == 0) ? 0 : 1;
}
