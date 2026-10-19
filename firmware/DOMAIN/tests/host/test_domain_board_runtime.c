#include "domain_board_runtime.h"
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
static uint32_t g_eventSequence;
static uint32_t g_stmInitEvent;
static uint32_t g_stmChannelEvent;
static uint32_t g_stmStartEvent;
static uint32_t g_runtimeInitEvent;
static uint32_t g_stmInitCalls;
static uint32_t g_stmReadCalls;
static uint32_t g_runtimeInitCalls;
static uint32_t g_runtimePollCalls;
static uint32_t g_counterValue;
static uint32_t g_lastNowMs;
static DomainRuntime_Status_t g_runtimeInitStatus;
static DomainRuntime_Status_t g_runtimePollStatus;
static bool g_runtimeReady;
static DomainRuntime_Config_t g_capturedRuntimeConfig;

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

static void ResetFakes(void)
{
    g_eventSequence = 0U;
    g_stmInitEvent = 0U;
    g_stmChannelEvent = 0U;
    g_stmStartEvent = 0U;
    g_runtimeInitEvent = 0U;
    g_stmInitCalls = 0U;
    g_stmReadCalls = 0U;
    g_runtimeInitCalls = 0U;
    g_runtimePollCalls = 0U;
    g_counterValue = 0U;
    g_lastNowMs = UINT32_MAX;
    g_runtimeInitStatus = DOMAIN_RUNTIME_OK;
    g_runtimePollStatus = DOMAIN_RUNTIME_OK;
    g_runtimeReady = false;
    (void)memset(&g_capturedRuntimeConfig, 0, sizeof(g_capturedRuntimeConfig));
}

static DomainBoardRuntime_Config_t MakeConfig(void)
{
    DomainBoardRuntime_Config_t config;

    (void)memset(&config, 0, sizeof(config));
    config.stm0InputClockHz = DOMAIN_STM0_INPUT_CLOCK_HZ;
    config.runtimeConfig.firstCommandId = 1U;
    return config;
}

void Stm_Ip_Init(uint8 instance, const Stm_Ip_InstanceConfigType *configPtr)
{
    CHECK(instance == 0U);
    CHECK(configPtr == &STM_0_InitConfig_PB);
    ++g_stmInitCalls;
    g_stmInitEvent = ++g_eventSequence;
}

void Stm_Ip_InitChannel(uint8 instance, const Stm_Ip_ChannelConfigType *configPtr)
{
    CHECK(instance == 0U);
    CHECK(configPtr == &STM_0_ChannelConfig_PB[0]);
    g_stmChannelEvent = ++g_eventSequence;
}

void Stm_Ip_StartTimer(uint8 instance, uint32 startValue)
{
    CHECK(instance == 0U);
    CHECK(startValue == 0U);
    g_stmStartEvent = ++g_eventSequence;
}

uint32 Stm_Ip_GetCounterValue(uint8 instance)
{
    CHECK(instance == 0U);
    ++g_stmReadCalls;
    return g_counterValue;
}

DomainRuntime_Status_t DomainRuntime_Init(
    DomainRuntime_t *runtime,
    const DomainRuntime_Config_t *config)
{
    uint32_t nowMs = UINT32_MAX;

    CHECK(runtime != NULL);
    CHECK(config != NULL);
    ++g_runtimeInitCalls;
    g_runtimeInitEvent = ++g_eventSequence;
    g_capturedRuntimeConfig = *config;
    CHECK(config->timeProvider != NULL);
    CHECK(config->timeProviderContext != NULL);
    CHECK(config->timeProvider(config->timeProviderContext, &nowMs));
    CHECK(nowMs == 0U);
    g_lastNowMs = nowMs;
    g_runtimeReady = (g_runtimeInitStatus == DOMAIN_RUNTIME_OK);
    return g_runtimeInitStatus;
}

DomainRuntime_Status_t DomainRuntime_Poll(DomainRuntime_t *runtime)
{
    uint32_t nowMs = UINT32_MAX;

    CHECK(runtime != NULL);
    ++g_runtimePollCalls;
    if (g_runtimePollStatus != DOMAIN_RUNTIME_OK)
    {
        return g_runtimePollStatus;
    }
    CHECK(g_capturedRuntimeConfig.timeProvider(
        g_capturedRuntimeConfig.timeProviderContext,
        &nowMs));
    g_lastNowMs = nowMs;
    return DOMAIN_RUNTIME_OK;
}

bool DomainRuntime_IsReady(const DomainRuntime_t *runtime)
{
    return (runtime != NULL) && g_runtimeReady;
}

static void Test_PreInitAndInvalidArguments(void)
{
    DomainBoardRuntime_t boardRuntime;
    DomainBoardRuntime_Config_t config = MakeConfig();
    DomainBoardRuntime_Snapshot_t snapshot;
    DomainBoardRuntime_Snapshot_t saved;
    int before = g_failures;

    BeginTest("Test_PreInitAndInvalidArguments");
    ResetFakes();
    (void)memset(&boardRuntime, 0, sizeof(boardRuntime));
    (void)memset(&snapshot, 0xA5, sizeof(snapshot));
    saved = snapshot;
    CHECK(DomainBoardRuntime_Init(NULL, &config) ==
          DOMAIN_BOARD_RUNTIME_INVALID_ARGUMENT);
    CHECK(DomainBoardRuntime_Init(&boardRuntime, NULL) ==
          DOMAIN_BOARD_RUNTIME_INVALID_ARGUMENT);
    CHECK(DomainBoardRuntime_Poll(NULL) ==
          DOMAIN_BOARD_RUNTIME_INVALID_ARGUMENT);
    CHECK(DomainBoardRuntime_Poll(&boardRuntime) ==
          DOMAIN_BOARD_RUNTIME_NOT_INITIALIZED);
    CHECK(DomainBoardRuntime_GetSnapshot(&boardRuntime, &snapshot) ==
          DOMAIN_BOARD_RUNTIME_NOT_INITIALIZED);
    CHECK(memcmp(&snapshot, &saved, sizeof(snapshot)) == 0);
    CHECK(!DomainBoardRuntime_IsReady(&boardRuntime));
    CHECK(DomainBoardRuntime_GetDomainRuntime(&boardRuntime) == NULL);
    EndTest(before);
}

static void Test_RejectsExternalTimeProvider(void)
{
    DomainBoardRuntime_t boardRuntime;
    DomainBoardRuntime_Config_t config = MakeConfig();
    int before = g_failures;

    BeginTest("Test_RejectsExternalTimeProvider");
    ResetFakes();
    (void)memset(&boardRuntime, 0, sizeof(boardRuntime));
    config.runtimeConfig.timeProvider = DomainBoardTimebase_GetNowMs;
    CHECK(DomainBoardRuntime_Init(&boardRuntime, &config) ==
          DOMAIN_BOARD_RUNTIME_INVALID_ARGUMENT);
    CHECK(g_stmInitCalls == 0U);
    CHECK(g_runtimeInitCalls == 0U);
    EndTest(before);
}

static void Test_ClockMismatchFaultBlocks(void)
{
    DomainBoardRuntime_t boardRuntime;
    DomainBoardRuntime_Config_t config = MakeConfig();
    DomainBoardRuntime_Snapshot_t snapshot;
    int before = g_failures;

    BeginTest("Test_ClockMismatchFaultBlocks");
    ResetFakes();
    (void)memset(&boardRuntime, 0, sizeof(boardRuntime));
    config.stm0InputClockHz = 40000000U;
    CHECK(DomainBoardRuntime_Init(&boardRuntime, &config) ==
          DOMAIN_BOARD_RUNTIME_STM0_INIT_FAILED);
    CHECK(DomainBoardRuntime_GetSnapshot(&boardRuntime, &snapshot) ==
          DOMAIN_BOARD_RUNTIME_OK);
    CHECK(snapshot.stage == DOMAIN_BOARD_RUNTIME_STAGE_FAULT_BLOCKED);
    CHECK(snapshot.faultBlocked);
    CHECK(!snapshot.ready);
    CHECK(g_stmInitCalls == 0U);
    CHECK(DomainBoardRuntime_Init(&boardRuntime, &config) ==
          DOMAIN_BOARD_RUNTIME_FAULT_BLOCKED);
    CHECK(DomainBoardRuntime_Poll(&boardRuntime) ==
          DOMAIN_BOARD_RUNTIME_FAULT_BLOCKED);
    EndTest(before);
}

static void Test_InitOrderAndReadyAccess(void)
{
    DomainBoardRuntime_t boardRuntime;
    DomainBoardRuntime_Config_t config = MakeConfig();
    DomainBoardRuntime_Snapshot_t snapshot;
    int before = g_failures;

    BeginTest("Test_InitOrderAndReadyAccess");
    ResetFakes();
    (void)memset(&boardRuntime, 0, sizeof(boardRuntime));
    g_counterValue = 1234U;
    CHECK(DomainBoardRuntime_Init(&boardRuntime, &config) ==
          DOMAIN_BOARD_RUNTIME_OK);
    CHECK(g_stmInitEvent < g_stmChannelEvent);
    CHECK(g_stmChannelEvent < g_stmStartEvent);
    CHECK(g_stmStartEvent < g_runtimeInitEvent);
    CHECK(g_stmReadCalls == 2U);
    CHECK(DomainBoardRuntime_IsReady(&boardRuntime));
    CHECK(DomainBoardRuntime_GetDomainRuntime(&boardRuntime) ==
          &boardRuntime.runtime);
    CHECK(DomainBoardRuntime_GetSnapshot(&boardRuntime, &snapshot) ==
          DOMAIN_BOARD_RUNTIME_OK);
    CHECK(snapshot.stage == DOMAIN_BOARD_RUNTIME_STAGE_DOMAIN_READY);
    CHECK(snapshot.ready);
    CHECK(!snapshot.faultBlocked);
    CHECK(snapshot.lastRuntimeStatus == DOMAIN_RUNTIME_OK);
    EndTest(before);
}

static void Test_DuplicateInitDoesNotRestartHardware(void)
{
    DomainBoardRuntime_t boardRuntime;
    DomainBoardRuntime_Config_t config = MakeConfig();
    int before = g_failures;

    BeginTest("Test_DuplicateInitDoesNotRestartHardware");
    ResetFakes();
    (void)memset(&boardRuntime, 0, sizeof(boardRuntime));
    CHECK(DomainBoardRuntime_Init(&boardRuntime, &config) ==
          DOMAIN_BOARD_RUNTIME_OK);
    CHECK(DomainBoardRuntime_Init(&boardRuntime, &config) ==
          DOMAIN_BOARD_RUNTIME_ALREADY_INITIALIZED);
    CHECK(g_stmInitCalls == 1U);
    CHECK(g_runtimeInitCalls == 1U);
    EndTest(before);
}

static void Test_RuntimeInitFailureIsSticky(void)
{
    DomainBoardRuntime_t boardRuntime;
    DomainBoardRuntime_Config_t config = MakeConfig();
    DomainBoardRuntime_Snapshot_t snapshot;
    int before = g_failures;

    BeginTest("Test_RuntimeInitFailureIsSticky");
    ResetFakes();
    (void)memset(&boardRuntime, 0, sizeof(boardRuntime));
    g_runtimeInitStatus = DOMAIN_RUNTIME_MANAGER_INIT_FAILED;
    CHECK(DomainBoardRuntime_Init(&boardRuntime, &config) ==
          DOMAIN_BOARD_RUNTIME_DOMAIN_INIT_FAILED);
    CHECK(DomainBoardRuntime_GetSnapshot(&boardRuntime, &snapshot) ==
          DOMAIN_BOARD_RUNTIME_OK);
    CHECK(snapshot.faultBlocked);
    CHECK(snapshot.lastRuntimeStatus == DOMAIN_RUNTIME_MANAGER_INIT_FAILED);
    CHECK(DomainBoardRuntime_Init(&boardRuntime, &config) ==
          DOMAIN_BOARD_RUNTIME_FAULT_BLOCKED);
    CHECK(g_stmInitCalls == 1U);
    CHECK(g_runtimeInitCalls == 1U);
    EndTest(before);
}

static void Test_PollUsesRolloverSafeTimebase(void)
{
    DomainBoardRuntime_t boardRuntime;
    DomainBoardRuntime_Config_t config = MakeConfig();
    int before = g_failures;

    BeginTest("Test_PollUsesRolloverSafeTimebase");
    ResetFakes();
    (void)memset(&boardRuntime, 0, sizeof(boardRuntime));
    g_counterValue = UINT32_MAX - 23999U;
    CHECK(DomainBoardRuntime_Init(&boardRuntime, &config) ==
          DOMAIN_BOARD_RUNTIME_OK);
    g_counterValue = 24000U;
    CHECK(DomainBoardRuntime_Poll(&boardRuntime) ==
          DOMAIN_BOARD_RUNTIME_OK);
    CHECK(g_runtimePollCalls == 1U);
    CHECK(g_lastNowMs == 1U);
    CHECK(DomainBoardRuntime_IsReady(&boardRuntime));
    EndTest(before);
}

static void Test_PollFailureFaultBlocks(void)
{
    DomainBoardRuntime_t boardRuntime;
    DomainBoardRuntime_Config_t config = MakeConfig();
    DomainBoardRuntime_Snapshot_t snapshot;
    int before = g_failures;

    BeginTest("Test_PollFailureFaultBlocks");
    ResetFakes();
    (void)memset(&boardRuntime, 0, sizeof(boardRuntime));
    CHECK(DomainBoardRuntime_Init(&boardRuntime, &config) ==
          DOMAIN_BOARD_RUNTIME_OK);
    g_runtimePollStatus = DOMAIN_RUNTIME_TIME_UNAVAILABLE;
    CHECK(DomainBoardRuntime_Poll(&boardRuntime) ==
          DOMAIN_BOARD_RUNTIME_RUNTIME_POLL_FAILED);
    CHECK(!DomainBoardRuntime_IsReady(&boardRuntime));
    CHECK(DomainBoardRuntime_GetSnapshot(&boardRuntime, &snapshot) ==
          DOMAIN_BOARD_RUNTIME_OK);
    CHECK(snapshot.faultBlocked);
    CHECK(snapshot.lastRuntimeStatus == DOMAIN_RUNTIME_TIME_UNAVAILABLE);
    CHECK(DomainBoardRuntime_Poll(&boardRuntime) ==
          DOMAIN_BOARD_RUNTIME_FAULT_BLOCKED);
    CHECK(g_runtimePollCalls == 1U);
    EndTest(before);
}

int main(void)
{
    Test_PreInitAndInvalidArguments();
    Test_RejectsExternalTimeProvider();
    Test_ClockMismatchFaultBlocks();
    Test_InitOrderAndReadyAccess();
    Test_DuplicateInitDoesNotRestartHardware();
    Test_RuntimeInitFailureIsSticky();
    Test_PollUsesRolloverSafeTimebase();
    Test_PollFailureFaultBlocks();

    (void)printf("%d tests, %d failures\n", g_tests, g_failures);
    return (g_failures == 0) ? 0 : 1;
}
