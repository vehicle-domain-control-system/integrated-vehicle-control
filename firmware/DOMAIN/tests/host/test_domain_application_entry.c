#include "domain_application_entry.h"

#include <stdio.h>
#include <string.h>

static int g_failures;
static int g_tests;
static uint32_t g_initCalls;
static uint32_t g_pollCalls;
static DomainBoardRuntime_Status_t g_initStatus = DOMAIN_BOARD_RUNTIME_OK;
static DomainBoardRuntime_Status_t g_pollStatus = DOMAIN_BOARD_RUNTIME_OK;
static bool g_boardReady;
static DomainBoardRuntime_Config_t g_capturedConfig;

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

static DomainBoardRuntime_Config_t MakeConfig(void)
{
    DomainBoardRuntime_Config_t config;

    (void)memset(&config, 0, sizeof(config));
    config.stm0InputClockHz = DOMAIN_STM0_INPUT_CLOCK_HZ;
    config.runtimeConfig.firstCommandId = 1U;
    return config;
}

DomainBoardRuntime_Status_t DomainBoardRuntime_Init(
    DomainBoardRuntime_t *boardRuntime,
    const DomainBoardRuntime_Config_t *config)
{
    CHECK(boardRuntime != NULL);
    CHECK(config != NULL);
    ++g_initCalls;
    g_capturedConfig = *config;
    g_boardReady = (g_initStatus == DOMAIN_BOARD_RUNTIME_OK);
    return g_initStatus;
}

DomainBoardRuntime_Status_t DomainBoardRuntime_Poll(
    DomainBoardRuntime_t *boardRuntime)
{
    CHECK(boardRuntime != NULL);
    ++g_pollCalls;
    return g_pollStatus;
}

bool DomainBoardRuntime_IsReady(const DomainBoardRuntime_t *boardRuntime)
{
    return (boardRuntime != NULL) && g_boardReady;
}

static void Test_PreInitIsFailClosed(void)
{
    DomainApplicationEntry_Snapshot_t snapshot;
    DomainApplicationEntry_Snapshot_t saved;
    int before = g_failures;

    BeginTest("Test_PreInitIsFailClosed");
    (void)memset(&snapshot, 0xA5, sizeof(snapshot));
    saved = snapshot;
    CHECK(!DomainApplicationEntry_IsReady());
    CHECK(DomainApplicationEntry_Poll() ==
          DOMAIN_APPLICATION_ENTRY_NOT_INITIALIZED);
    CHECK(DomainApplicationEntry_GetSnapshot(&snapshot) ==
          DOMAIN_APPLICATION_ENTRY_NOT_INITIALIZED);
    CHECK(memcmp(&snapshot, &saved, sizeof(snapshot)) == 0);
    CHECK(g_pollCalls == 0U);
    EndTest(before);
}

static void Test_InvalidArgumentsDoNotStartRuntime(void)
{
    int before = g_failures;

    BeginTest("Test_InvalidArgumentsDoNotStartRuntime");
    CHECK(DomainApplicationEntry_Init(NULL) ==
          DOMAIN_APPLICATION_ENTRY_INVALID_ARGUMENT);
    CHECK(DomainApplicationEntry_GetSnapshot(NULL) ==
          DOMAIN_APPLICATION_ENTRY_INVALID_ARGUMENT);
    CHECK(g_initCalls == 0U);
    CHECK(!DomainApplicationEntry_IsReady());
    EndTest(before);
}

static void Test_InitOwnsAndStartsBoardRuntime(void)
{
    DomainBoardRuntime_Config_t config = MakeConfig();
    DomainApplicationEntry_Snapshot_t snapshot;
    int before = g_failures;

    BeginTest("Test_InitOwnsAndStartsBoardRuntime");
    CHECK(DomainApplicationEntry_Init(&config) ==
          DOMAIN_APPLICATION_ENTRY_OK);
    CHECK(g_initCalls == 1U);
    CHECK(g_capturedConfig.stm0InputClockHz ==
          DOMAIN_STM0_INPUT_CLOCK_HZ);
    CHECK(DomainApplicationEntry_IsReady());
    CHECK(DomainApplicationEntry_GetSnapshot(&snapshot) ==
          DOMAIN_APPLICATION_ENTRY_OK);
    CHECK(snapshot.initAttempted);
    CHECK(snapshot.ready);
    CHECK(!snapshot.faultBlocked);
    EndTest(before);
}

static void Test_DuplicateInitDoesNotRestartRuntime(void)
{
    DomainBoardRuntime_Config_t config = MakeConfig();
    int before = g_failures;

    BeginTest("Test_DuplicateInitDoesNotRestartRuntime");
    CHECK(DomainApplicationEntry_Init(&config) ==
          DOMAIN_APPLICATION_ENTRY_ALREADY_INITIALIZED);
    CHECK(g_initCalls == 1U);
    CHECK(DomainApplicationEntry_IsReady());
    EndTest(before);
}

static void Test_PollRunsOnlyWhenReady(void)
{
    int before = g_failures;

    BeginTest("Test_PollRunsOnlyWhenReady");
    CHECK(DomainApplicationEntry_Poll() == DOMAIN_APPLICATION_ENTRY_OK);
    CHECK(g_pollCalls == 1U);
    CHECK(DomainApplicationEntry_IsReady());
    EndTest(before);
}

static void Test_PollFailureIsStickyAndFailClosed(void)
{
    DomainApplicationEntry_Snapshot_t snapshot;
    DomainBoardRuntime_Config_t config = MakeConfig();
    int before = g_failures;

    BeginTest("Test_PollFailureIsStickyAndFailClosed");
    g_pollStatus = DOMAIN_BOARD_RUNTIME_RUNTIME_POLL_FAILED;
    CHECK(DomainApplicationEntry_Poll() ==
          DOMAIN_APPLICATION_ENTRY_POLL_FAILED);
    CHECK(!DomainApplicationEntry_IsReady());
    CHECK(DomainApplicationEntry_GetSnapshot(&snapshot) ==
          DOMAIN_APPLICATION_ENTRY_OK);
    CHECK(snapshot.faultBlocked);
    CHECK(!snapshot.ready);
    CHECK(snapshot.lastBoardStatus ==
          DOMAIN_BOARD_RUNTIME_RUNTIME_POLL_FAILED);
    CHECK(DomainApplicationEntry_Poll() ==
          DOMAIN_APPLICATION_ENTRY_FAULT_BLOCKED);
    CHECK(g_pollCalls == 2U);
    CHECK(DomainApplicationEntry_Init(&config) ==
          DOMAIN_APPLICATION_ENTRY_FAULT_BLOCKED);
    CHECK(g_initCalls == 1U);
    EndTest(before);
}

static void Test_InitFailureIsStickyAndFailClosed(void)
{
    DomainBoardRuntime_Config_t config = MakeConfig();
    DomainApplicationEntry_Snapshot_t snapshot;
    int before = g_failures;

    BeginTest("Test_InitFailureIsStickyAndFailClosed");
    g_initStatus = DOMAIN_BOARD_RUNTIME_STM0_INIT_FAILED;
    CHECK(DomainApplicationEntry_Init(&config) ==
          DOMAIN_APPLICATION_ENTRY_BOARD_INIT_FAILED);
    CHECK(g_initCalls == 1U);
    CHECK(!DomainApplicationEntry_IsReady());
    CHECK(DomainApplicationEntry_GetSnapshot(&snapshot) ==
          DOMAIN_APPLICATION_ENTRY_OK);
    CHECK(snapshot.initAttempted);
    CHECK(snapshot.faultBlocked);
    CHECK(!snapshot.ready);
    CHECK(snapshot.lastBoardStatus ==
          DOMAIN_BOARD_RUNTIME_STM0_INIT_FAILED);
    CHECK(DomainApplicationEntry_Poll() ==
          DOMAIN_APPLICATION_ENTRY_FAULT_BLOCKED);
    CHECK(g_pollCalls == 0U);
    CHECK(DomainApplicationEntry_Init(&config) ==
          DOMAIN_APPLICATION_ENTRY_FAULT_BLOCKED);
    CHECK(g_initCalls == 1U);
    EndTest(before);
}

int main(int argc, char **argv)
{
    if ((argc == 2) && (strcmp(argv[1], "init-failure") == 0))
    {
        Test_InitFailureIsStickyAndFailClosed();
        (void)printf("%d tests, %d failures\n", g_tests, g_failures);
        return (g_failures == 0) ? 0 : 1;
    }

    Test_PreInitIsFailClosed();
    Test_InvalidArgumentsDoNotStartRuntime();
    Test_InitOwnsAndStartsBoardRuntime();
    Test_DuplicateInitDoesNotRestartRuntime();
    Test_PollRunsOnlyWhenReady();
    Test_PollFailureIsStickyAndFailClosed();

    (void)printf("%d tests, %d failures\n", g_tests, g_failures);
    return (g_failures == 0) ? 0 : 1;
}
