#include "domain_runtime.h"
#include "domain_interface_core_internal.h"

#include <limits.h>
#include <stdio.h>
#include <string.h>

typedef struct
{
    uint32_t nowMs;
    bool available;
    uint32_t calls;
} TestClock_t;

static DomainRuntime_t g_runtime;
static TestClock_t g_clock;
static int g_failures;
static int g_tests;
static uint32_t g_permissionEvaluationCalls;
static uint32_t g_permissionTxCalls;
static DomainIf_Status_t g_permissionTxStatus;

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

static bool TestClock_GetNow(void *context, uint32_t *outNowMs)
{
    TestClock_t *clock = (TestClock_t *)context;

    if ((clock == NULL) || (outNowMs == NULL))
    {
        return false;
    }
    ++clock->calls;
    if (!clock->available)
    {
        return false;
    }
    *outNowMs = clock->nowMs;
    return true;
}

static PermissionCoordinator_EvaluationResult_t TestPermission_Evaluate(
    void *context,
    uint32_t nowMs,
    PermissionManager_Evaluation_t *outEvaluation)
{
    (void)context;
    (void)nowMs;
    ++g_permissionEvaluationCalls;
    outEvaluation->permission = PERMISSION_MANAGER_PERMISSION_ALLOW;
    outEvaluation->sourceQuality = DATA_QUALITY_OK;
    outEvaluation->sourceAgeMs = 0U;
    return PERMISSION_COORDINATOR_EVALUATION_SUCCEEDED;
}

static DomainIf_Status_t TestPermission_Transmit(
    const DomainIf_CisPowerPermission_t *permission)
{
    ++g_permissionTxCalls;
    CHECK(permission != NULL);
    return g_permissionTxStatus;
}

static DomainRuntime_Config_t NormalConfig(void)
{
    DomainRuntime_Config_t config;

    (void)memset(&config, 0, sizeof(config));
    config.timeProvider = TestClock_GetNow;
    config.timeProviderContext = &g_clock;
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

static ValueQuality_t Quality(
    DataQuality_t quality,
    uint32_t ageMs,
    uint32_t sequence)
{
    ValueQuality_t value;

    value.validity = (quality == DATA_QUALITY_OK)
                         ? VALUE_VALIDITY_VALID
                         : VALUE_VALIDITY_INVALID;
    value.reason = (quality == DATA_QUALITY_OK)
                       ? QUALITY_REASON_NONE
                       : QUALITY_REASON_NO_DATA;
    value.meta.quality = quality;
    value.meta.ageMs = ageMs;
    value.meta.updateSequence = sequence;
    return value;
}

static CisCabinEnvironmentState_t NormalEnvironment(void)
{
    CisCabinEnvironmentState_t state;

    state.cabinTemperature = 2300;
    state.temperatureQuality = Quality(DATA_QUALITY_OK, 3U, 1U);
    state.cabinHumidity = 4500U;
    state.humidityQuality = Quality(DATA_QUALITY_OK, 4U, 2U);
    state.cabinIlluminance = 120U;
    state.illuminanceQuality = Quality(DATA_QUALITY_OK, 5U, 3U);
    return state;
}

static CisOccupantState_t NormalOccupant(void)
{
    CisOccupantState_t state;

    state.occupantPresent = true;
    state.occupantCount = 1U;
    state.presenceQuality = Quality(DATA_QUALITY_OK, 0U, 1U);
    state.countQuality = Quality(DATA_QUALITY_OK, 0U, 1U);
    state.sourceGeneration = 1U;
    state.visionFunctionStatus = CIS_FUNCTION_STATUS_ACTIVE;
    return state;
}

static void Test_NullAndPreInitialization(void)
{
    DomainRuntime_t runtime;
    DomainRuntime_Config_t config = NormalConfig();
    DomainRuntime_Snapshot_t output;
    DomainRuntime_Snapshot_t saved;
    int before = g_failures;

    BeginTest("Test_NullAndPreInitialization");
    (void)memset(&runtime, 0, sizeof(runtime));
    (void)memset(&output, 0xA5, sizeof(output));
    saved = output;
    CHECK(DomainRuntime_Init(NULL, &config) ==
          DOMAIN_RUNTIME_INVALID_ARGUMENT);
    CHECK(DomainRuntime_Init(&runtime, NULL) ==
          DOMAIN_RUNTIME_INVALID_ARGUMENT);
    config.timeProvider = NULL;
    CHECK(DomainRuntime_Init(&runtime, &config) ==
          DOMAIN_RUNTIME_INVALID_ARGUMENT);
    CHECK(DomainRuntime_Poll(NULL) == DOMAIN_RUNTIME_INVALID_ARGUMENT);
    CHECK(DomainRuntime_Poll(&runtime) == DOMAIN_RUNTIME_NOT_INITIALIZED);
    CHECK(!DomainRuntime_IsReady(NULL));
    CHECK(!DomainRuntime_IsReady(&runtime));
    CHECK(DomainRuntime_GetSnapshot(&runtime, &output) ==
          DOMAIN_RUNTIME_NOT_INITIALIZED);
    CHECK(memcmp(&output, &saved, sizeof(output)) == 0);
    CHECK(DomainRuntime_GetSnapshot(NULL, &output) ==
          DOMAIN_RUNTIME_INVALID_ARGUMENT);
    CHECK(DomainRuntime_GetSnapshot(&runtime, NULL) ==
          DOMAIN_RUNTIME_INVALID_ARGUMENT);
    EndTest(before);
}

static void Test_FailedInitIsFailClosed(void)
{
    DomainRuntime_Config_t config = NormalConfig();
    DomainRuntime_Snapshot_t snapshot;
    int before = g_failures;

    BeginTest("Test_FailedInitIsFailClosed");
    (void)memset(&g_runtime, 0, sizeof(g_runtime));
    (void)memset(&g_clock, 0, sizeof(g_clock));
    g_clock.available = false;
    CHECK(DomainRuntime_Init(&g_runtime, &config) ==
          DOMAIN_RUNTIME_TIME_UNAVAILABLE);
    CHECK(!DomainRuntime_IsReady(&g_runtime));
    CHECK(DomainRuntime_GetSnapshot(&g_runtime, &snapshot) ==
          DOMAIN_RUNTIME_OK);
    CHECK(!snapshot.initialized);
    CHECK(!snapshot.ready);
    CHECK(snapshot.initStage == DOMAIN_RUNTIME_INIT_STAGE_NONE);
    CHECK(DomainRuntime_GetVehicleStateManager(&g_runtime) == NULL);
    CHECK(DomainRuntime_Poll(&g_runtime) == DOMAIN_RUNTIME_NOT_INITIALIZED);
    EndTest(before);
}

static void Test_RetryAndInitializationOrder(void)
{
    DomainRuntime_Config_t config = NormalConfig();
    DomainRuntime_Snapshot_t snapshot;
    int before = g_failures;

    BeginTest("Test_RetryAndInitializationOrder");
    g_clock.available = true;
    g_clock.nowMs = 100U;
    CHECK(DomainRuntime_Init(&g_runtime, &config) == DOMAIN_RUNTIME_OK);
    CHECK(DomainRuntime_IsReady(&g_runtime));
    CHECK(DomainRuntime_GetSnapshot(&g_runtime, &snapshot) ==
          DOMAIN_RUNTIME_OK);
    CHECK(snapshot.initialized);
    CHECK(snapshot.ready);
    CHECK(snapshot.cisOccupantEnabled);
    CHECK(snapshot.initStage == DOMAIN_RUNTIME_INIT_STAGE_READY);
    CHECK(DomainIf_IsInitialized());
    CHECK(g_runtime.vehicleStateManager.initialized);
    CHECK(g_runtime.commandManager.initialized);
    CHECK(g_runtime.resultManager.initialized);
    CHECK(g_runtime.permissionManager.initialized);
    CHECK(g_runtime.cisOccupantManager.initialized);
    CHECK(g_runtime.cisOccupantPrivate.initialized);
    CHECK(g_runtime.cisOccupantPrivate.occupantManager ==
          &g_runtime.cisOccupantManager);
    CHECK(g_runtime.cisEnvironmentManager.initialized);
    CHECK(g_runtime.cisEnvironmentPrivate.initialized);
    CHECK(g_runtime.cisEnvironmentPrivate.environmentManager ==
          &g_runtime.cisEnvironmentManager);
    EndTest(before);
}

static void Test_ManagerStorageOwnershipAndAliases(void)
{
    int before = g_failures;

    BeginTest("Test_ManagerStorageOwnershipAndAliases");
    CHECK(g_runtime.vehicleStateManager.slots == g_runtime.vehicleStateSlots);
    CHECK(g_runtime.vehicleStateManager.slotCount ==
          DOMAIN_RUNTIME_VEHICLE_STATE_SLOT_COUNT);
    CHECK(g_runtime.vehicleStateSlots[DOMAIN_RUNTIME_STATE_SLOT_BCM_DOOR]
              .valueStorage ==
          (uint8_t *)&g_runtime.vehicleStateStorage.bcmDoor);
    CHECK(g_runtime.commandManager.slots == g_runtime.commandSlots);
    CHECK(g_runtime.commandManager.capacity == DOMAIN_RUNTIME_COMMAND_CAPACITY);
    CHECK(g_runtime.resultManager.slots == g_runtime.resultSlots);
    CHECK(g_runtime.resultManager.capacity == DOMAIN_RUNTIME_RESULT_CAPACITY);
    CHECK(DomainRuntime_GetVehicleStateManager(&g_runtime) ==
          &g_runtime.vehicleStateManager);
    CHECK(DomainRuntime_GetCommandManager(&g_runtime) ==
          &g_runtime.commandManager);
    CHECK(DomainRuntime_GetResultManager(&g_runtime) ==
          &g_runtime.resultManager);
    CHECK(DomainRuntime_GetPermissionManager(&g_runtime) ==
          &g_runtime.permissionManager);
    CHECK(DomainRuntime_GetCisOccupantManager(&g_runtime) ==
          &g_runtime.cisOccupantManager);
    CHECK(DomainRuntime_GetCisOccupantPrivate(&g_runtime) ==
          &g_runtime.cisOccupantPrivate);
    CHECK(DomainRuntime_GetCisEnvironmentManager(&g_runtime) ==
          &g_runtime.cisEnvironmentManager);
    CHECK(DomainRuntime_GetCisEnvironmentPrivate(&g_runtime) ==
          &g_runtime.cisEnvironmentPrivate);
    EndTest(before);
}

static void Test_InitialUnknownAndNoData(void)
{
    PermissionManager_Snapshot_t permission;
    CisCabinEnvironmentState_t environment;
    CisOccupantState_t occupant;
    size_t index;
    int before = g_failures;

    BeginTest("Test_InitialUnknownAndNoData");
    for (index = 0U; index < DOMAIN_RUNTIME_VEHICLE_STATE_SLOT_COUNT; ++index)
    {
        CHECK(!g_runtime.vehicleStateSlots[index].hasObservation);
        CHECK(g_runtime.vehicleStateSlots[index].quality ==
              VEHICLE_STATE_QUALITY_NO_DATA);
    }
    CHECK(PermissionManager_GetSnapshot(
              &g_runtime.permissionManager, 100U, &permission) ==
          PERMISSION_MANAGER_OK);
    CHECK(!permission.hasEvaluation);
    CHECK(permission.effectivePermission ==
          PERMISSION_MANAGER_PERMISSION_UNKNOWN);
    CHECK(permission.effectiveQuality == DATA_QUALITY_NO_DATA);
    CHECK(DomainIf_QueryGetCisEnvironment(100U, &environment) ==
          DOMAIN_IF_OK);
    CHECK(environment.temperatureQuality.meta.quality ==
          DATA_QUALITY_NO_DATA);
    CHECK(environment.humidityQuality.meta.quality == DATA_QUALITY_NO_DATA);
    CHECK(environment.illuminanceQuality.meta.quality ==
          DATA_QUALITY_NO_DATA);
    CHECK(DomainIf_QueryGetCisOccupant(100U, &occupant) == DOMAIN_IF_OK);
    CHECK(occupant.presenceQuality.meta.quality == DATA_QUALITY_NO_DATA);
    CHECK(occupant.countQuality.meta.quality == DATA_QUALITY_NO_DATA);
    EndTest(before);
}

static void Test_ObserverAndTxPortRemainUnregistered(void)
{
    DomainIf_CisPowerPermission_t permission;
    int before = g_failures;

    BeginTest("Test_ObserverAndTxPortRemainUnregistered");
    (void)memset(&permission, 0, sizeof(permission));
    permission.vehiclePowerAllowed = false;
    permission.quality = DATA_QUALITY_NO_DATA;
    permission.meta.quality = DATA_QUALITY_NO_DATA;
    CHECK(!DomainIfCore_TestHasRxObserver());
    CHECK(!DomainIfCore_TestHasTxPort());
    CHECK(DomainIf_TxCisPowerPermission(&permission) ==
          DOMAIN_IF_NOT_AVAILABLE);
    EndTest(before);
}

static void Test_DuplicateInitChangesNothing(void)
{
    DomainRuntime_Config_t config = NormalConfig();
    DomainRuntime_t saved = g_runtime;
    uint32_t calls = g_clock.calls;
    int before = g_failures;

    BeginTest("Test_DuplicateInitChangesNothing");
    CHECK(DomainRuntime_Init(&g_runtime, &config) ==
          DOMAIN_RUNTIME_ALREADY_INITIALIZED);
    CHECK(memcmp(&g_runtime, &saved, sizeof(g_runtime)) == 0);
    CHECK(g_clock.calls == calls);
    CHECK(!DomainIfCore_TestHasRxObserver());
    CHECK(!DomainIfCore_TestHasTxPort());
    EndTest(before);
}

static void Test_FirstAndRepeatedPoll(void)
{
    DomainRuntime_Snapshot_t snapshot;
    int before = g_failures;

    BeginTest("Test_FirstAndRepeatedPoll");
    g_clock.nowMs = 200U;
    CHECK(DomainRuntime_Poll(&g_runtime) == DOMAIN_RUNTIME_OK);
    CHECK(DomainRuntime_GetSnapshot(&g_runtime, &snapshot) ==
          DOMAIN_RUNTIME_OK);
    CHECK(snapshot.hasPolled);
    CHECK(snapshot.pollCount == 1U);
    CHECK(snapshot.lastPollAtMs == 200U);
    CHECK(snapshot.lastPollElapsedMs == 0U);
    g_clock.nowMs = 225U;
    CHECK(DomainRuntime_Poll(&g_runtime) == DOMAIN_RUNTIME_OK);
    CHECK(DomainRuntime_GetSnapshot(&g_runtime, &snapshot) ==
          DOMAIN_RUNTIME_OK);
    CHECK(snapshot.pollCount == 2U);
    CHECK(snapshot.lastPollAtMs == 225U);
    CHECK(snapshot.lastPollElapsedMs == 25U);
    EndTest(before);
}

static void Test_PollRollover(void)
{
    DomainRuntime_Snapshot_t snapshot;
    int before = g_failures;

    BeginTest("Test_PollRollover");
    g_clock.nowMs = UINT32_MAX - 5U;
    CHECK(DomainRuntime_Poll(&g_runtime) == DOMAIN_RUNTIME_OK);
    g_clock.nowMs = 4U;
    CHECK(DomainRuntime_Poll(&g_runtime) == DOMAIN_RUNTIME_OK);
    CHECK(DomainRuntime_GetSnapshot(&g_runtime, &snapshot) ==
          DOMAIN_RUNTIME_OK);
    CHECK(snapshot.lastPollAtMs == 4U);
    CHECK(snapshot.lastPollElapsedMs == 10U);
    EndTest(before);
}

static void Test_TimeFailureDoesNotMutatePollState(void)
{
    DomainRuntime_Snapshot_t beforeSnapshot;
    DomainRuntime_Snapshot_t afterSnapshot;
    int before = g_failures;

    BeginTest("Test_TimeFailureDoesNotMutatePollState");
    CHECK(DomainRuntime_GetSnapshot(&g_runtime, &beforeSnapshot) ==
          DOMAIN_RUNTIME_OK);
    g_clock.available = false;
    CHECK(DomainRuntime_Poll(&g_runtime) == DOMAIN_RUNTIME_TIME_UNAVAILABLE);
    CHECK(DomainRuntime_GetSnapshot(&g_runtime, &afterSnapshot) ==
          DOMAIN_RUNTIME_OK);
    CHECK(memcmp(&beforeSnapshot, &afterSnapshot, sizeof(beforeSnapshot)) ==
          0);
    g_clock.available = true;
    EndTest(before);
}

static void Test_PollHasNoManagerOrNetworkSideEffects(void)
{
    PermissionManager_t permission = g_runtime.permissionManager;
    CommandManager_t command = g_runtime.commandManager;
    ResultManager_t result = g_runtime.resultManager;
    CisEnvironmentManager_t environment = g_runtime.cisEnvironmentManager;
    CisOccupantManager_t occupant = g_runtime.cisOccupantManager;
    DomainCisOccupantPriv_t occupantPrivate =
        g_runtime.cisOccupantPrivate;
    int before = g_failures;

    BeginTest("Test_PollHasNoManagerOrNetworkSideEffects");
    g_clock.nowMs = 10U;
    CHECK(DomainRuntime_Poll(&g_runtime) == DOMAIN_RUNTIME_OK);
    CHECK(memcmp(&permission, &g_runtime.permissionManager,
                 sizeof(permission)) == 0);
    CHECK(memcmp(&command, &g_runtime.commandManager, sizeof(command)) == 0);
    CHECK(memcmp(&result, &g_runtime.resultManager, sizeof(result)) == 0);
    CHECK(memcmp(&environment, &g_runtime.cisEnvironmentManager,
                 sizeof(environment)) == 0);
    CHECK(memcmp(&occupant, &g_runtime.cisOccupantManager,
                 sizeof(occupant)) == 0);
    CHECK(memcmp(&occupantPrivate, &g_runtime.cisOccupantPrivate,
                 sizeof(occupantPrivate)) == 0);
    CHECK(!DomainIfCore_TestHasRxObserver());
    CHECK(!DomainIfCore_TestHasTxPort());
    EndTest(before);
}

static void Test_CisPrivateAndBindingShareOwnedContext(void)
{
    DomainCisEnvironmentPriv_Transition_t transition;
    DomainCisEnvironmentPriv_TransitionReceipt_t receipt;
    CisCabinEnvironmentState_t input = NormalEnvironment();
    CisCabinEnvironmentState_t output;
    int before = g_failures;

    BeginTest("Test_CisPrivateAndBindingShareOwnedContext");
    transition.admittedCisBootId = 77U;
    transition.transitionEpoch = UINT64_C(1);
    CHECK(DomainIf_RxCisCabinEnvironment(&input) == DOMAIN_IF_REJECTED);
    CHECK(DomainCisEnvironmentPriv_ApplyBootTransition(
              &g_runtime.cisEnvironmentPrivate, &transition, &receipt) ==
          DOMAIN_CIS_ENVIRONMENT_PRIV_APPLIED);
    g_clock.nowMs = 1000U;
    CHECK(DomainIf_RxCisCabinEnvironment(&input) == DOMAIN_IF_OK);
    CHECK(DomainIf_QueryGetCisEnvironment(1000U, &output) == DOMAIN_IF_OK);
    CHECK(output.cabinTemperature == input.cabinTemperature);
    CHECK(output.temperatureQuality.meta.updateSequence == 1U);
    CHECK(g_runtime.cisEnvironmentManager.temperature.hasObservation);
    EndTest(before);
}

static void Test_GetSnapshotIsReadOnly(void)
{
    DomainRuntime_t saved = g_runtime;
    DomainRuntime_Snapshot_t output;
    int before = g_failures;

    BeginTest("Test_GetSnapshotIsReadOnly");
    CHECK(DomainRuntime_GetSnapshot(&g_runtime, &output) ==
          DOMAIN_RUNTIME_OK);
    CHECK(memcmp(&g_runtime, &saved, sizeof(g_runtime)) == 0);
    EndTest(before);
}

static void Test_OccupantFreshnessAndNewRuntimeReset(void)
{
    DomainRuntime_t replacement;
    DomainRuntime_Config_t config = NormalConfig();
    CisOccupantState_t input = NormalOccupant();
    CisOccupantState_t output;
    DomainCisOccupantPriv_Transition_t transition;
    DomainCisOccupantPriv_TransitionReceipt_t receipt;
    int before = g_failures;

    BeginTest("Test_OccupantFreshnessAndNewRuntimeReset");
    transition.admittedSourceGeneration = input.sourceGeneration;
    transition.transitionEpoch = UINT64_C(1);
    CHECK(DomainIf_RxCisOccupant(&input) == DOMAIN_IF_REJECTED);
    CHECK(DomainCisOccupantPriv_ApplyBootTransition(
              &g_runtime.cisOccupantPrivate, &transition, &receipt) ==
          DOMAIN_CIS_OCCUPANT_PRIV_APPLIED);
    g_clock.nowMs = 3000U;
    CHECK(DomainIf_RxCisOccupant(&input) == DOMAIN_IF_OK);
    CHECK(DomainIf_QueryGetCisOccupant(3499U, &output) == DOMAIN_IF_OK);
    CHECK(output.presenceQuality.meta.quality == DATA_QUALITY_OK);
    g_clock.nowMs = 3500U;
    CHECK(DomainRuntime_Poll(&g_runtime) == DOMAIN_RUNTIME_OK);
    CHECK(DomainIf_QueryGetCisOccupant(3500U, &output) == DOMAIN_IF_OK);
    CHECK(output.presenceQuality.meta.quality == DATA_QUALITY_STALE);
    CHECK(output.countQuality.meta.quality == DATA_QUALITY_STALE);

    /* Network ingress is assumed closed before replacing the owner context. */
    (void)memset(&replacement, 0, sizeof(replacement));
    CHECK(DomainRuntime_Init(&replacement, &config) == DOMAIN_RUNTIME_OK);
    CHECK(DomainIf_QueryGetCisOccupant(3500U, &output) == DOMAIN_IF_OK);
    CHECK(output.presenceQuality.meta.quality == DATA_QUALITY_NO_DATA);
    CHECK(output.countQuality.meta.quality == DATA_QUALITY_NO_DATA);
    EndTest(before);
}

static void Test_ZeroOccupantTtlPreventsRuntimeReady(void)
{
    DomainRuntime_t runtime;
    DomainRuntime_Config_t config = NormalConfig();
    CisOccupantManager_Snapshot_t occupant;
    int before = g_failures;

    BeginTest("Test_ZeroOccupantTtlPreventsRuntimeReady");
    (void)memset(&runtime, 0xA5, sizeof(runtime));
    config.cisOccupantPolicy.presenceTtlMs = 0U;
    CHECK(DomainRuntime_Init(&runtime, &config) ==
          DOMAIN_RUNTIME_MANAGER_INIT_FAILED);
    CHECK(!DomainRuntime_IsReady(&runtime));
    CHECK(!runtime.cisOccupantManager.initialized);
    CHECK(DomainIf_QueryGetCisOccupant(0U, &occupant.effective) ==
          DOMAIN_IF_NOT_AVAILABLE);
    CHECK(CisOccupantManager_GetSnapshot(
              &runtime.cisOccupantManager, 0U, &occupant) ==
          CIS_OCCUPANT_MANAGER_NOT_INITIALIZED);

    (void)memset(&runtime, 0xA5, sizeof(runtime));
    config = NormalConfig();
    config.cisOccupantPolicy.countTtlMs = 0U;
    CHECK(DomainRuntime_Init(&runtime, &config) ==
          DOMAIN_RUNTIME_MANAGER_INIT_FAILED);
    CHECK(!DomainRuntime_IsReady(&runtime));
    EndTest(before);
}

static void Test_DisabledOccupantIsUnavailableAndRuntimeReady(void)
{
    DomainRuntime_t runtime;
    DomainRuntime_Config_t config = NormalConfig();
    DomainRuntime_Snapshot_t runtimeSnapshot;
    CisOccupantState_t input = NormalOccupant();
    CisOccupantState_t output;
    CisOccupantState_t saved;
    int before = g_failures;

    BeginTest("Test_DisabledOccupantIsUnavailableAndRuntimeReady");
    (void)memset(&runtime, 0, sizeof(runtime));
    config.cisOccupantEnabled = false;
    config.cisOccupantPolicy.presenceTtlMs = 0U;
    config.cisOccupantPolicy.countTtlMs = 0U;
    CHECK(DomainRuntime_Init(&runtime, &config) == DOMAIN_RUNTIME_OK);
    CHECK(DomainRuntime_IsReady(&runtime));
    CHECK(DomainRuntime_GetSnapshot(&runtime, &runtimeSnapshot) ==
          DOMAIN_RUNTIME_OK);
    CHECK(!runtimeSnapshot.cisOccupantEnabled);
    CHECK(!runtime.cisOccupantManager.initialized);
    CHECK(!runtime.cisOccupantPrivate.initialized);
    CHECK(DomainRuntime_GetCisOccupantManager(&runtime) == NULL);
    CHECK(DomainRuntime_GetCisOccupantPrivate(&runtime) == NULL);

    (void)memset(&output, 0xA5, sizeof(output));
    saved = output;
    CHECK(DomainIf_RxCisOccupant(&input) == DOMAIN_IF_NOT_AVAILABLE);
    CHECK(DomainIf_QueryGetCisOccupant(0U, &output) ==
          DOMAIN_IF_NOT_AVAILABLE);
    CHECK(memcmp(&output, &saved, sizeof(output)) == 0);
    CHECK(!DomainIfCore_TestHasRxObserver());
    CHECK(!DomainIfCore_TestHasTxPort());
    EndTest(before);
}

static void Test_PermissionCoordinatorRunsFromPoll(void)
{
    DomainRuntime_t runtime;
    DomainRuntime_Config_t config = NormalConfig();
    DomainIf_TxPort_t port;
    DomainRuntime_Snapshot_t runtimeSnapshot;
    PermissionCoordinator_Snapshot_t coordinatorSnapshot;
    PermissionManager_Snapshot_t permissionSnapshot;
    int before = g_failures;

    BeginTest("Test_PermissionCoordinatorRunsFromPoll");
    (void)memset(&runtime, 0, sizeof(runtime));
    config.permissionEvaluator = TestPermission_Evaluate;
    config.permissionCoordinatorEnabled = true;
    g_clock.available = true;
    g_clock.nowMs = 100U;
    g_permissionEvaluationCalls = 0U;
    g_permissionTxCalls = 0U;
    g_permissionTxStatus = DOMAIN_IF_OK;
    CHECK(DomainRuntime_Init(&runtime, &config) == DOMAIN_RUNTIME_OK);

    (void)memset(&port, 0, sizeof(port));
    port.txCisPowerPermission = TestPermission_Transmit;
    CHECK(DomainIf_SetTxPort(&port) == DOMAIN_IF_OK);
    CHECK(DomainRuntime_Poll(&runtime) == DOMAIN_RUNTIME_OK);
    CHECK(g_permissionEvaluationCalls == 1U);
    CHECK(g_permissionTxCalls == 1U);

    g_clock.nowMs = 299U;
    CHECK(DomainRuntime_Poll(&runtime) == DOMAIN_RUNTIME_OK);
    CHECK(g_permissionEvaluationCalls == 1U);
    g_clock.nowMs = 300U;
    CHECK(DomainRuntime_Poll(&runtime) == DOMAIN_RUNTIME_OK);
    CHECK(g_permissionEvaluationCalls == 2U);
    CHECK(g_permissionTxCalls == 2U);

    g_permissionTxStatus = DOMAIN_IF_REJECTED;
    g_clock.nowMs = 500U;
    CHECK(DomainRuntime_Poll(&runtime) == DOMAIN_RUNTIME_OK);
    CHECK(PermissionCoordinator_GetSnapshot(
              &runtime.permissionCoordinator, &coordinatorSnapshot) ==
          PERMISSION_COORDINATOR_OK);
    CHECK(coordinatorSnapshot.lastTxStatus == DOMAIN_IF_REJECTED);
    CHECK(coordinatorSnapshot.submissionSuccessCount == 2U);
    CHECK(DomainRuntime_GetSnapshot(&runtime, &runtimeSnapshot) ==
          DOMAIN_RUNTIME_OK);
    CHECK(runtimeSnapshot.lastPermissionCoordinatorStatus ==
          PERMISSION_COORDINATOR_TX_FAILED);
    CHECK(PermissionManager_GetSnapshot(
              &runtime.permissionManager, 500U, &permissionSnapshot) ==
          PERMISSION_MANAGER_OK);
    CHECK(permissionSnapshot.updateSequence == 3U);
    EndTest(before);
}

static void Test_InvalidPermissionScheduleFailsAndCanRetry(void)
{
    DomainRuntime_t runtime;
    DomainRuntime_Config_t config = NormalConfig();
    int before = g_failures;

    BeginTest("Test_InvalidPermissionScheduleFailsAndCanRetry");
    (void)memset(&runtime, 0, sizeof(runtime));
    g_clock.available = true;
    g_clock.nowMs = 0U;
    config.permissionPolicy.reevaluationPeriodMs = 199U;
    config.permissionCoordinatorEnabled = true;
    CHECK(DomainRuntime_Init(&runtime, &config) ==
          DOMAIN_RUNTIME_MANAGER_INIT_FAILED);
    CHECK(!DomainRuntime_IsReady(&runtime));
    CHECK(DomainRuntime_Poll(&runtime) == DOMAIN_RUNTIME_NOT_INITIALIZED);

    config.permissionPolicy.reevaluationPeriodMs =
        PERMISSION_COORDINATOR_EVALUATION_PERIOD_MS;
    CHECK(DomainRuntime_Init(&runtime, &config) == DOMAIN_RUNTIME_OK);
    CHECK(DomainRuntime_IsReady(&runtime));
    EndTest(before);
}

static void Test_DisabledPermissionCoordinatorPreservesRuntimeReady(void)
{
    DomainRuntime_t runtime;
    DomainRuntime_Config_t config = NormalConfig();
    DomainRuntime_Snapshot_t runtimeSnapshot;
    PermissionManager_Snapshot_t permissionSnapshot;
    int before = g_failures;

    BeginTest("Test_DisabledPermissionCoordinatorPreservesRuntimeReady");
    (void)memset(&runtime, 0, sizeof(runtime));
    config.permissionCoordinatorEnabled = false;
    config.permissionPolicy.reevaluationPeriodMs = 0U;
    config.permissionPolicy.sourceValidityMs = 0U;
    config.permissionEvaluator = TestPermission_Evaluate;
    g_clock.available = true;
    g_clock.nowMs = 0U;
    g_permissionEvaluationCalls = 0U;
    g_permissionTxCalls = 0U;

    CHECK(DomainRuntime_Init(&runtime, &config) == DOMAIN_RUNTIME_OK);
    CHECK(DomainRuntime_IsReady(&runtime));
    CHECK(!runtime.permissionCoordinator.snapshot.initialized);
    CHECK(DomainRuntime_Poll(&runtime) == DOMAIN_RUNTIME_OK);
    CHECK(g_permissionEvaluationCalls == 0U);
    CHECK(g_permissionTxCalls == 0U);
    CHECK(DomainRuntime_GetSnapshot(&runtime, &runtimeSnapshot) ==
          DOMAIN_RUNTIME_OK);
    CHECK(!runtimeSnapshot.permissionCoordinatorEnabled);
    CHECK(runtimeSnapshot.lastPermissionCoordinatorStatus ==
          PERMISSION_COORDINATOR_NOT_INITIALIZED);
    CHECK(PermissionManager_GetSnapshot(
              &runtime.permissionManager, 0U, &permissionSnapshot) ==
          PERMISSION_MANAGER_OK);
    CHECK(!permissionSnapshot.hasEvaluation);
    CHECK(permissionSnapshot.effectivePermission ==
          PERMISSION_MANAGER_PERMISSION_UNKNOWN);
    CHECK(permissionSnapshot.effectiveQuality == DATA_QUALITY_NO_DATA);
    EndTest(before);
}

int main(void)
{
    Test_NullAndPreInitialization();
    Test_FailedInitIsFailClosed();
    Test_RetryAndInitializationOrder();
    Test_ManagerStorageOwnershipAndAliases();
    Test_InitialUnknownAndNoData();
    Test_ObserverAndTxPortRemainUnregistered();
    Test_DuplicateInitChangesNothing();
    Test_FirstAndRepeatedPoll();
    Test_PollRollover();
    Test_TimeFailureDoesNotMutatePollState();
    Test_PollHasNoManagerOrNetworkSideEffects();
    Test_CisPrivateAndBindingShareOwnedContext();
    Test_GetSnapshotIsReadOnly();
    Test_OccupantFreshnessAndNewRuntimeReset();
    Test_ZeroOccupantTtlPreventsRuntimeReady();
    Test_DisabledOccupantIsUnavailableAndRuntimeReady();
    Test_PermissionCoordinatorRunsFromPoll();
    Test_InvalidPermissionScheduleFailsAndCanRetry();
    Test_DisabledPermissionCoordinatorPreservesRuntimeReady();

    (void)printf("%d tests, %d failures\n", g_tests, g_failures);
    return (g_failures == 0) ? 0 : 1;
}
