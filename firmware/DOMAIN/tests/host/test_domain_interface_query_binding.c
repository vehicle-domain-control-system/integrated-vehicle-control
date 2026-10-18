#include "domain_interface_query_binding.h"

#include <limits.h>
#include <stdio.h>
#include <string.h>

#define ASSERT_TRUE(condition)                                                   \
    do                                                                           \
    {                                                                            \
        if (!(condition))                                                        \
        {                                                                        \
            printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #condition);          \
            return 1;                                                            \
        }                                                                        \
    } while (0)

#define ASSERT_EQ(expected, actual) ASSERT_TRUE((expected) == (actual))

enum
{
    SLOT_BCM_DOOR = 0,
    SLOT_BCM_STATUS,
    SLOT_CIS_REAR,
    SLOT_CIS_STATUS,
    SLOT_COUNT
};

typedef struct
{
    uint32_t nowMs;
} TestClock_t;

typedef struct
{
    VehicleStateManager_t stateManager;
    VehicleStateManager_Slot_t stateSlots[SLOT_COUNT];
    VehicleStateManager_SlotConfig_t stateConfigs[SLOT_COUNT];
    DoorState_t doorStorage;
    DomainIf_BcmStatusReport_t bcmStatusStorage;
    CisRearState_t cisRearStorage;
    DomainIf_CisStatusReport_t cisStatusStorage;
    DomainIfStateBinding_SlotMap_t slotMap;
    DomainIfQueryBinding_StatePolicy_t statePolicy;
    ResultManager_t resultManager;
    ResultManager_Slot_t resultSlots[10];
    TestClock_t clock;
} Fixture_t;

static uint32_t g_observerCalls;

static bool TestClock_GetNow(void *context, uint32_t *outNowMs)
{
    TestClock_t *clock = (TestClock_t *)context;

    *outNowMs = clock->nowMs;
    return true;
}

static SignalMeta_t Meta(
    DataQuality_t quality,
    uint32_t ageMs,
    UpdateSequence_t sequence)
{
    SignalMeta_t meta;

    meta.quality = quality;
    meta.ageMs = ageMs;
    meta.updateSequence = sequence;
    return meta;
}

static DoorState_t DoorState(
    DataQuality_t quality,
    UpdateSequence_t sequence,
    SourceGeneration_t generation)
{
    DoorState_t state;

    (void)memset(&state, 0, sizeof(state));
    state.lockState = DOOR_LOCK_STATE_LOCKED;
    state.openState = DOOR_OPEN_STATE_CLOSED;
    state.compositeState = DOOR_COMPOSITE_STATE_NORMAL;
    state.lockQuality = quality;
    state.openQuality = quality;
    state.compositeQuality = quality;
    state.sourceGeneration = generation;
    state.meta = Meta(quality, 7U, sequence);
    return state;
}

static DomainIf_BcmStatusReport_t BcmStatus(void)
{
    DomainIf_BcmStatusReport_t state;

    (void)memset(&state, 0, sizeof(state));
    state.ecuState = ECU_STATE_READY;
    state.sourceGeneration = 10U;
    state.meta = Meta(DATA_QUALITY_INVALID, 4U, 2U);
    return state;
}

static CisRearState_t CisRear(void)
{
    CisRearState_t state;

    (void)memset(&state, 0, sizeof(state));
    state.rearDistanceMm = 1234U;
    state.measurementState = REAR_MEASUREMENT_VALID_DISTANCE;
    state.distanceQuality.validity = VALUE_VALIDITY_VALID;
    state.distanceQuality.reason = QUALITY_REASON_NONE;
    state.distanceQuality.meta = Meta(DATA_QUALITY_OK, 8U, 3U);
    state.sourceGeneration = 20U;
    return state;
}

static DomainIf_CisStatusReport_t CisStatus(void)
{
    DomainIf_CisStatusReport_t state;

    (void)memset(&state, 0, sizeof(state));
    state.ecuState = CIS_STATE_READY;
    state.occupantFunctionStatus = CIS_FUNCTION_STATUS_READY;
    state.temperatureFunctionStatus = CIS_FUNCTION_STATUS_READY;
    state.humidityFunctionStatus = CIS_FUNCTION_STATUS_READY;
    state.illuminanceFunctionStatus = CIS_FUNCTION_STATUS_READY;
    state.rearFunctionStatus = CIS_FUNCTION_STATUS_READY;
    state.piUartStatus = CIS_INTERFACE_AVAILABLE;
    state.canInterfaceStatus = CIS_INTERFACE_AVAILABLE;
    state.sourceGeneration = 30U;
    state.meta = Meta(DATA_QUALITY_STALE, 6U, 4U);
    return state;
}

static ResultManager_Policy_t ResultPolicy(void)
{
    ResultManager_Policy_t policy;

    (void)memset(&policy, 0, sizeof(policy));
    policy.firstResponseTimeoutEnabled = true;
    policy.firstResponseTimeoutMs = 10U;
    return policy;
}

static DomainIf_MobileRequest_t DoorRequest(
    DeviceContextId_t device,
    SessionId_t session,
    RequestId_t requestId)
{
    DomainIf_MobileRequest_t request;

    (void)memset(&request, 0, sizeof(request));
    request.deviceContextId = device;
    request.context.sessionId = session;
    request.context.requestId = requestId;
    request.type = MOBILE_REQUEST_DOOR_LOCK;
    request.payload.doorTarget = DOOR_TARGET_LOCK;
    request.meta = Meta(DATA_QUALITY_OK, 0U, 1U);
    return request;
}

static DomainIf_Status_t ObserveBcmStatus(
    const DomainIf_BcmStatusReport_t *status)
{
    (void)status;
    ++g_observerCalls;
    return DOMAIN_IF_OK;
}

static void PrepareFixture(Fixture_t *fixture)
{
    (void)memset(fixture, 0, sizeof(*fixture));
    fixture->stateConfigs[SLOT_BCM_DOOR].valueStorage =
        &fixture->doorStorage;
    fixture->stateConfigs[SLOT_BCM_DOOR].valueCapacity =
        sizeof(fixture->doorStorage);
    fixture->stateConfigs[SLOT_BCM_STATUS].valueStorage =
        &fixture->bcmStatusStorage;
    fixture->stateConfigs[SLOT_BCM_STATUS].valueCapacity =
        sizeof(fixture->bcmStatusStorage);
    fixture->stateConfigs[SLOT_CIS_REAR].valueStorage =
        &fixture->cisRearStorage;
    fixture->stateConfigs[SLOT_CIS_REAR].valueCapacity =
        sizeof(fixture->cisRearStorage);
    fixture->stateConfigs[SLOT_CIS_STATUS].valueStorage =
        &fixture->cisStatusStorage;
    fixture->stateConfigs[SLOT_CIS_STATUS].valueCapacity =
        sizeof(fixture->cisStatusStorage);
    fixture->slotMap.bcmDoorState = SLOT_BCM_DOOR;
    fixture->slotMap.bcmStatus = SLOT_BCM_STATUS;
    fixture->slotMap.cisRearState = SLOT_CIS_REAR;
    fixture->slotMap.cisStatus = SLOT_CIS_STATUS;
    fixture->statePolicy.bcmDoorTtlMs = 100U;
    fixture->statePolicy.bcmStatusTtlMs = 100U;
    fixture->statePolicy.cisRearTtlMs = 100U;
    fixture->statePolicy.cisStatusTtlMs = 100U;
    fixture->clock.nowMs = 100U;
}

static int ConfigureFixture(Fixture_t *fixture)
{
    ResultManager_Policy_t resultPolicy = ResultPolicy();

    PrepareFixture(fixture);
    ASSERT_EQ(VEHICLE_STATE_MANAGER_OK,
              VehicleStateManager_Init(
                  &fixture->stateManager, fixture->stateSlots,
                  fixture->stateConfigs, SLOT_COUNT));
    ASSERT_EQ(RESULT_MANAGER_OK,
              ResultManager_Init(
                  &fixture->resultManager, fixture->resultSlots,
                  sizeof(fixture->resultSlots) /
                      sizeof(fixture->resultSlots[0]),
                  &resultPolicy));
    ASSERT_EQ(DOMAIN_IF_OK, DomainIf_Init());
    ASSERT_EQ(DOMAIN_IF_OK, DomainIf_SetRxObserver(NULL));
    ASSERT_EQ(DOMAIN_IF_OK,
              DomainIfStateBinding_Configure(
                  &fixture->stateManager, &fixture->slotMap,
                  TestClock_GetNow, &fixture->clock));
    ASSERT_EQ(DOMAIN_IF_OK,
              DomainIfQueryBinding_ConfigureState(
                  &fixture->stateManager, &fixture->slotMap,
                  &fixture->statePolicy));
    ASSERT_EQ(DOMAIN_IF_OK,
              DomainIfQueryBinding_ConfigureResults(
                  &fixture->resultManager));
    return 0;
}

static int TestPreInitAndUnconfigured(void)
{
    Fixture_t fixture;
    DoorState_t output;
    RequestContext_t request = {1U, 1U};
    DomainIf_MobileRequestResult_t result;
    DomainIf_MobileRequestResult_t recent[1];
    size_t recentCount = 77U;
    bool hasRecord = true;

    PrepareFixture(&fixture);
    (void)memset(&output, 0xA5, sizeof(output));
    (void)memset(&result, 0xA5, sizeof(result));
    ASSERT_EQ(DOMAIN_IF_INVALID_ARGUMENT,
              DomainIf_QueryGetBcmDoorState(0U, NULL));
    ASSERT_EQ(DOMAIN_IF_NOT_INITIALIZED,
              DomainIf_QueryGetBcmDoorState(0U, &output));
    ASSERT_EQ(DOMAIN_IF_NOT_INITIALIZED,
              DomainIf_QueryGetRequestResult(
                  1U, &request, 0U, &result, &hasRecord));
    ASSERT_EQ(DOMAIN_IF_NOT_INITIALIZED,
              DomainIf_QueryGetRecentRequestResults(
                  1U, 0U, recent, 1U, &recentCount));
    ASSERT_EQ(77U, recentCount);
    ASSERT_EQ(DOMAIN_IF_NOT_INITIALIZED,
              DomainIfQueryBinding_ConfigureState(
                  &fixture.stateManager, &fixture.slotMap,
                  &fixture.statePolicy));
    ASSERT_EQ(DOMAIN_IF_NOT_INITIALIZED,
              DomainIfQueryBinding_ConfigureResults(&fixture.resultManager));

    ASSERT_EQ(DOMAIN_IF_OK, DomainIf_Init());
    ASSERT_EQ(DOMAIN_IF_NOT_AVAILABLE,
              DomainIf_QueryGetBcmDoorState(0U, &output));
    ASSERT_EQ(DOMAIN_IF_NOT_AVAILABLE,
              DomainIf_QueryGetRequestResult(
                  1U, &request, 0U, &result, &hasRecord));
    ASSERT_EQ(DOMAIN_IF_NOT_AVAILABLE,
              DomainIf_QueryGetRecentRequestResults(
                  1U, 0U, recent, 1U, &recentCount));
    ASSERT_EQ(DOMAIN_IF_INVALID_ARGUMENT,
              DomainIfQueryBinding_ConfigureState(
                  NULL, &fixture.slotMap, &fixture.statePolicy));
    ASSERT_EQ(DOMAIN_IF_INVALID_ARGUMENT,
              DomainIfQueryBinding_ConfigureResults(NULL));
    return 0;
}

static int TestNoStateLeavesOutputUnchanged(void)
{
    Fixture_t fixture;
    DoorState_t output;
    DoorState_t before;

    ASSERT_EQ(0, ConfigureFixture(&fixture));
    (void)memset(&output, 0xA5, sizeof(output));
    before = output;
    ASSERT_EQ(DOMAIN_IF_NOT_AVAILABLE,
              DomainIf_QueryGetBcmDoorState(100U, &output));
    ASSERT_TRUE(memcmp(&before, &output, sizeof(output)) == 0);
    return 0;
}

static int TestDoorCopyAgeQualityAndPointerIsolation(void)
{
    Fixture_t fixture;
    DoorState_t input = DoorState(DATA_QUALITY_OK, 9U, 3U);
    DoorState_t first;
    DoorState_t second;

    ASSERT_EQ(0, ConfigureFixture(&fixture));
    fixture.clock.nowMs = UINT32_MAX - 3U;
    fixture.statePolicy.bcmDoorTtlMs = 13U;
    ASSERT_EQ(DOMAIN_IF_OK,
              DomainIfQueryBinding_ConfigureState(
                  &fixture.stateManager, &fixture.slotMap,
                  &fixture.statePolicy));
    ASSERT_EQ(DOMAIN_IF_OK, DomainIf_RxBcmDoorState(&input));
    ASSERT_EQ(DOMAIN_IF_OK,
              DomainIf_QueryGetBcmDoorState(2U, &first));
    ASSERT_EQ(DATA_QUALITY_STALE, first.meta.quality);
    ASSERT_EQ(13U, first.meta.ageMs);
    ASSERT_EQ(9U, first.meta.updateSequence);
    ASSERT_EQ(3U, first.sourceGeneration);
    ASSERT_EQ(DATA_QUALITY_OK, first.lockQuality);

    first.lockState = DOOR_LOCK_STATE_UNLOCKED;
    ASSERT_EQ(DOMAIN_IF_OK,
              DomainIf_QueryGetBcmDoorState(2U, &second));
    ASSERT_EQ(DOOR_LOCK_STATE_LOCKED, second.lockState);
    return 0;
}

static int TestAllOwnedStateGetters(void)
{
    Fixture_t fixture;
    DomainIf_BcmStatusReport_t bcm = BcmStatus();
    DomainIf_BcmStatusReport_t bcmOutput;
    CisRearState_t rear = CisRear();
    CisRearState_t rearOutput;
    DomainIf_CisStatusReport_t cis = CisStatus();
    DomainIf_CisStatusReport_t cisOutput;

    ASSERT_EQ(0, ConfigureFixture(&fixture));
    fixture.clock.nowMs = 50U;
    ASSERT_EQ(DOMAIN_IF_OK, DomainIf_RxBcmStatus(&bcm));
    ASSERT_EQ(DOMAIN_IF_OK, DomainIf_RxCisRear(&rear));
    ASSERT_EQ(DOMAIN_IF_OK, DomainIf_RxCisStatus(&cis));

    ASSERT_EQ(DOMAIN_IF_OK, DomainIf_QueryGetBcmStatus(60U, &bcmOutput));
    ASSERT_EQ(ECU_STATE_READY, bcmOutput.ecuState);
    ASSERT_EQ(DATA_QUALITY_INVALID, bcmOutput.meta.quality);
    ASSERT_EQ(14U, bcmOutput.meta.ageMs);
    ASSERT_EQ(10U, bcmOutput.sourceGeneration);

    ASSERT_EQ(DOMAIN_IF_OK, DomainIf_QueryGetCisRear(60U, &rearOutput));
    ASSERT_EQ(1234U, rearOutput.rearDistanceMm);
    ASSERT_EQ(18U, rearOutput.distanceQuality.meta.ageMs);
    ASSERT_EQ(20U, rearOutput.sourceGeneration);

    ASSERT_EQ(DOMAIN_IF_OK, DomainIf_QueryGetCisStatus(60U, &cisOutput));
    ASSERT_EQ(CIS_STATE_READY, cisOutput.ecuState);
    ASSERT_EQ(DATA_QUALITY_STALE, cisOutput.meta.quality);
    ASSERT_EQ(16U, cisOutput.meta.ageMs);
    ASSERT_EQ(30U, cisOutput.sourceGeneration);
    return 0;
}

static int TestDuplicateAndOldInputDoNotChangeGetter(void)
{
    Fixture_t fixture;
    DoorState_t input = DoorState(DATA_QUALITY_OK, 10U, 1U);
    DoorState_t output;

    ASSERT_EQ(0, ConfigureFixture(&fixture));
    ASSERT_EQ(DOMAIN_IF_OK, DomainIf_RxBcmDoorState(&input));
    input.lockState = DOOR_LOCK_STATE_UNLOCKED;
    ASSERT_EQ(DOMAIN_IF_OK, DomainIf_RxBcmDoorState(&input));
    input.meta.updateSequence = 9U;
    ASSERT_EQ(DOMAIN_IF_REJECTED, DomainIf_RxBcmDoorState(&input));
    ASSERT_EQ(DOMAIN_IF_OK,
              DomainIf_QueryGetBcmDoorState(100U, &output));
    ASSERT_EQ(DOOR_LOCK_STATE_LOCKED, output.lockState);
    ASSERT_EQ(10U, output.meta.updateSequence);
    return 0;
}

static int TestGetterDoesNotMutateOrNotify(void)
{
    Fixture_t fixture;
    DomainIf_BcmStatusReport_t input = BcmStatus();
    DomainIf_BcmStatusReport_t output;
    DomainIf_RxObserver_t observer;
    VehicleStateManager_Snapshot_t before;
    VehicleStateManager_Snapshot_t after;
    DomainIf_BcmStatusReport_t stored;

    ASSERT_EQ(0, ConfigureFixture(&fixture));
    (void)memset(&observer, 0, sizeof(observer));
    observer.onBcmStatus = ObserveBcmStatus;
    ASSERT_EQ(DOMAIN_IF_OK, DomainIf_SetRxObserver(&observer));
    g_observerCalls = 0U;
    ASSERT_EQ(DOMAIN_IF_OK, DomainIf_RxBcmStatus(&input));
    ASSERT_EQ(1U, g_observerCalls);
    ASSERT_EQ(VEHICLE_STATE_MANAGER_OK,
              VehicleStateManager_GetSnapshot(
                  &fixture.stateManager, SLOT_BCM_STATUS, 100U, UINT32_MAX,
                  &stored, sizeof(stored), &before));

    ASSERT_EQ(DOMAIN_IF_OK, DomainIf_QueryGetBcmStatus(150U, &output));
    ASSERT_EQ(1U, g_observerCalls);
    ASSERT_EQ(VEHICLE_STATE_MANAGER_OK,
              VehicleStateManager_GetSnapshot(
                  &fixture.stateManager, SLOT_BCM_STATUS, 100U, UINT32_MAX,
                  &stored, sizeof(stored), &after));
    ASSERT_TRUE(memcmp(&before, &after, sizeof(before)) == 0);
    return 0;
}

static int TestNoDataObservationIsNotFabricated(void)
{
    Fixture_t fixture;
    DoorState_t input = DoorState(DATA_QUALITY_NO_DATA, 1U, 1U);
    DoorState_t output;
    DoorState_t before;

    ASSERT_EQ(0, ConfigureFixture(&fixture));
    ASSERT_EQ(DOMAIN_IF_OK, DomainIf_RxBcmDoorState(&input));
    (void)memset(&output, 0x5A, sizeof(output));
    before = output;
    ASSERT_EQ(DOMAIN_IF_NOT_AVAILABLE,
              DomainIf_QueryGetBcmDoorState(100U, &output));
    ASSERT_TRUE(memcmp(&before, &output, sizeof(output)) == 0);
    return 0;
}

static int TestRequestResultFoundAndRollover(void)
{
    Fixture_t fixture;
    DomainIf_MobileRequest_t request = DoorRequest(11U, 22U, 33U);
    ResultManager_RequestKey_t key;
    DomainIf_MobileRequestResult_t output;
    ResultManager_Snapshot_t before;
    ResultManager_Snapshot_t after;
    bool hasRecord = false;

    ASSERT_EQ(0, ConfigureFixture(&fixture));
    key.deviceContextId = request.deviceContextId;
    key.context = request.context;
    ASSERT_EQ(RESULT_MANAGER_OK,
              ResultManager_RegisterRequest(
                  &fixture.resultManager, &request, UINT32_MAX - 2U));
    ASSERT_EQ(RESULT_MANAGER_OK,
              ResultManager_RecordDomainResult(
                  &fixture.resultManager, &key, REQUEST_RESULT_ACCEPTED,
                  RESULT_REASON_NONE, false, UINT32_MAX - 1U));
    ASSERT_EQ(RESULT_MANAGER_OK,
              ResultManager_GetSnapshot(
                  &fixture.resultManager, &key, &before));

    ASSERT_EQ(DOMAIN_IF_OK,
              DomainIf_QueryGetRequestResult(
                  11U, &request.context, 1U, &output, &hasRecord));
    ASSERT_TRUE(hasRecord);
    ASSERT_EQ(11U, output.deviceContextId);
    ASSERT_EQ(22U, output.resultInfo.context.sessionId);
    ASSERT_EQ(33U, output.resultInfo.context.requestId);
    ASSERT_EQ(REQUEST_RESULT_ACCEPTED, output.resultInfo.result);
    ASSERT_EQ(3U, output.resultAgeMs);
    ASSERT_EQ(RESULT_MANAGER_OK,
              ResultManager_GetSnapshot(
                  &fixture.resultManager, &key, &after));
    ASSERT_TRUE(memcmp(&before, &after, sizeof(before)) == 0);
    return 0;
}

static int TestMissingResultAndInvalidArgumentsPreserveOutputs(void)
{
    Fixture_t fixture;
    RequestContext_t request = {2U, 3U};
    DomainIf_MobileRequestResult_t output;
    DomainIf_MobileRequestResult_t before;
    bool hasRecord;

    ASSERT_EQ(0, ConfigureFixture(&fixture));
    (void)memset(&output, 0xA5, sizeof(output));
    before = output;
    hasRecord = true;
    ASSERT_EQ(DOMAIN_IF_OK,
              DomainIf_QueryGetRequestResult(
                  1U, &request, 10U, &output, &hasRecord));
    ASSERT_TRUE(!hasRecord);
    ASSERT_TRUE(memcmp(&before, &output, sizeof(output)) == 0);

    hasRecord = true;
    ASSERT_EQ(DOMAIN_IF_INVALID_ARGUMENT,
              DomainIf_QueryGetRequestResult(
                  0U, &request, 10U, &output, &hasRecord));
    ASSERT_TRUE(hasRecord);
    ASSERT_TRUE(memcmp(&before, &output, sizeof(output)) == 0);
    ASSERT_EQ(DOMAIN_IF_INVALID_ARGUMENT,
              DomainIf_QueryGetRequestResult(
                  1U, NULL, 10U, &output, &hasRecord));
    ASSERT_EQ(DOMAIN_IF_INVALID_ARGUMENT,
              DomainIf_QueryGetRequestResult(
                  1U, &request, 10U, NULL, &hasRecord));
    ASSERT_EQ(DOMAIN_IF_INVALID_ARGUMENT,
              DomainIf_QueryGetRequestResult(
                  1U, &request, 10U, &output, NULL));
    return 0;
}

static int TestTimedOutResultIsPresentButUnconfirmed(void)
{
    Fixture_t fixture;
    DomainIf_MobileRequest_t request = DoorRequest(7U, 8U, 9U);
    ResultManager_RequestKey_t key;
    ResultManager_TimeoutState_t timeout;
    DomainIf_MobileRequestResult_t output;
    bool hasRecord = false;

    ASSERT_EQ(0, ConfigureFixture(&fixture));
    key.deviceContextId = request.deviceContextId;
    key.context = request.context;
    ASSERT_EQ(RESULT_MANAGER_OK,
              ResultManager_RegisterRequest(
                  &fixture.resultManager, &request, 100U));
    ASSERT_EQ(RESULT_MANAGER_OK,
              ResultManager_EvaluateTimeout(
                  &fixture.resultManager, &key, 110U, &timeout));
    ASSERT_EQ(RESULT_MANAGER_TIMEOUT_FIRST_RESPONSE, timeout);
    ASSERT_EQ(DOMAIN_IF_OK,
              DomainIf_QueryGetRequestResult(
                  7U, &request.context, 111U, &output, &hasRecord));
    ASSERT_TRUE(hasRecord);
    ASSERT_TRUE(!output.hasVehicleResult);
    ASSERT_EQ(RESULT_CONFIRMATION_UNCONFIRMED,
              output.resultInfo.confirmation);
    ASSERT_EQ(11U, output.resultAgeMs);
    return 0;
}

static int TestRecentResultsLimitOrderingAndSingleConsistency(void)
{
    Fixture_t fixture;
    DomainIf_MobileRequest_t request;
    DomainIf_MobileRequestResult_t recent[8];
    DomainIf_MobileRequestResult_t single;
    bool hasRecord = false;
    size_t index;
    size_t count = 0U;

    ASSERT_EQ(0, ConfigureFixture(&fixture));
    for (index = 1U; index <= 8U; ++index)
    {
        request = DoorRequest(42U, 100U, (RequestId_t)index);
        ASSERT_EQ(RESULT_MANAGER_OK,
                  ResultManager_RegisterRequest(
                      &fixture.resultManager, &request, (uint32_t)index));
    }

    ASSERT_EQ(DOMAIN_IF_OK,
              DomainIf_QueryGetRecentRequestResults(
                  42U, 20U, recent, 8U, &count));
    ASSERT_EQ(5U, count);
    for (index = 0U; index < count; ++index)
    {
        ASSERT_EQ((RequestId_t)(8U - index),
                  recent[index].resultInfo.context.requestId);
        ASSERT_TRUE(!recent[index].hasVehicleResult);
        ASSERT_EQ(RESULT_CONFIRMATION_UNCONFIRMED,
                  recent[index].resultInfo.confirmation);
    }

    ASSERT_EQ(DOMAIN_IF_OK,
              DomainIf_QueryGetRequestResult(
                  42U, &recent[0].resultInfo.context, 20U,
                  &single, &hasRecord));
    ASSERT_TRUE(hasRecord);
    ASSERT_TRUE(memcmp(&single, &recent[0], sizeof(single)) == 0);

    count = 0U;
    ASSERT_EQ(DOMAIN_IF_OK,
              DomainIf_QueryGetRecentRequestResults(
                  42U, 20U, NULL, 0U, &count));
    ASSERT_EQ(5U, count);
    ASSERT_EQ(DOMAIN_IF_OK,
              DomainIf_QueryGetRecentRequestResults(
                  42U, 20U, recent, 1U, &count));
    ASSERT_EQ(1U, count);
    ASSERT_EQ(8U, recent[0].resultInfo.context.requestId);
    return 0;
}

static int TestRecentResultsEmptyIsolationAndFailureAtomicity(void)
{
    Fixture_t fixture;
    DomainIf_MobileRequest_t request = DoorRequest(9U, 10U, 11U);
    DomainIf_MobileRequestResult_t output[2];
    DomainIf_MobileRequestResult_t before[2];
    size_t count = 55U;

    ASSERT_EQ(0, ConfigureFixture(&fixture));
    ASSERT_EQ(RESULT_MANAGER_OK,
              ResultManager_RegisterRequest(
                  &fixture.resultManager, &request, 1U));
    (void)memset(output, 0xA5, sizeof(output));
    (void)memcpy(before, output, sizeof(output));

    ASSERT_EQ(DOMAIN_IF_OK,
              DomainIf_QueryGetRecentRequestResults(
                  10U, 2U, output, 2U, &count));
    ASSERT_EQ(0U, count);
    ASSERT_TRUE(memcmp(output, before, sizeof(output)) == 0);

    count = 55U;
    ASSERT_EQ(DOMAIN_IF_INVALID_ARGUMENT,
              DomainIf_QueryGetRecentRequestResults(
                  0U, 2U, output, 2U, &count));
    ASSERT_EQ(55U, count);
    ASSERT_TRUE(memcmp(output, before, sizeof(output)) == 0);
    ASSERT_EQ(DOMAIN_IF_INVALID_ARGUMENT,
              DomainIf_QueryGetRecentRequestResults(
                  9U, 2U, NULL, 1U, &count));
    ASSERT_EQ(DOMAIN_IF_INVALID_ARGUMENT,
              DomainIf_QueryGetRecentRequestResults(
                  9U, 2U, output, 1U, NULL));
    return 0;
}

static int TestRecentResultPreservesTimeoutAndGetterState(void)
{
    Fixture_t fixture;
    DomainIf_MobileRequest_t request = DoorRequest(7U, 8U, 9U);
    ResultManager_RequestKey_t key;
    ResultManager_Snapshot_t before;
    ResultManager_Snapshot_t after;
    ResultManager_TimeoutState_t timeout;
    DomainIf_MobileRequestResult_t output[1];
    size_t count = 0U;

    ASSERT_EQ(0, ConfigureFixture(&fixture));
    key.deviceContextId = request.deviceContextId;
    key.context = request.context;
    ASSERT_EQ(RESULT_MANAGER_OK,
              ResultManager_RegisterRequest(
                  &fixture.resultManager, &request, UINT32_MAX - 2U));
    ASSERT_EQ(RESULT_MANAGER_OK,
              ResultManager_EvaluateTimeout(
                  &fixture.resultManager, &key, 8U, &timeout));
    ASSERT_EQ(RESULT_MANAGER_TIMEOUT_FIRST_RESPONSE, timeout);
    ASSERT_EQ(RESULT_MANAGER_OK,
              ResultManager_GetSnapshot(
                  &fixture.resultManager, &key, &before));
    g_observerCalls = 0U;
    ASSERT_EQ(DOMAIN_IF_OK,
              DomainIf_QueryGetRecentRequestResults(
                  7U, 9U, output, 1U, &count));
    ASSERT_EQ(1U, count);
    ASSERT_TRUE(!output[0].hasVehicleResult);
    ASSERT_EQ(RESULT_CONFIRMATION_UNCONFIRMED,
              output[0].resultInfo.confirmation);
    ASSERT_EQ(12U, output[0].resultAgeMs);
    ASSERT_EQ(0U, g_observerCalls);
    ASSERT_EQ(RESULT_MANAGER_OK,
              ResultManager_GetSnapshot(
                  &fixture.resultManager, &key, &after));
    ASSERT_TRUE(memcmp(&before, &after, sizeof(before)) == 0);
    return 0;
}

static int TestAllStateNullOutputs(void)
{
    Fixture_t fixture;

    ASSERT_EQ(0, ConfigureFixture(&fixture));
    ASSERT_EQ(DOMAIN_IF_INVALID_ARGUMENT,
              DomainIf_QueryGetBcmDoorState(0U, NULL));
    ASSERT_EQ(DOMAIN_IF_INVALID_ARGUMENT,
              DomainIf_QueryGetBcmStatus(0U, NULL));
    ASSERT_EQ(DOMAIN_IF_INVALID_ARGUMENT,
              DomainIf_QueryGetCisRear(0U, NULL));
    ASSERT_EQ(DOMAIN_IF_INVALID_ARGUMENT,
              DomainIf_QueryGetCisStatus(0U, NULL));
    return 0;
}

int main(void)
{
    int failures = 0;

    failures += TestPreInitAndUnconfigured();
    failures += TestNoStateLeavesOutputUnchanged();
    failures += TestDoorCopyAgeQualityAndPointerIsolation();
    failures += TestAllOwnedStateGetters();
    failures += TestDuplicateAndOldInputDoNotChangeGetter();
    failures += TestGetterDoesNotMutateOrNotify();
    failures += TestNoDataObservationIsNotFabricated();
    failures += TestRequestResultFoundAndRollover();
    failures += TestMissingResultAndInvalidArgumentsPreserveOutputs();
    failures += TestTimedOutResultIsPresentButUnconfirmed();
    failures += TestRecentResultsLimitOrderingAndSingleConsistency();
    failures += TestRecentResultsEmptyIsolationAndFailureAtomicity();
    failures += TestRecentResultPreservesTimeoutAndGetterState();
    failures += TestAllStateNullOutputs();

    if (failures == 0)
    {
        printf("domain_interface_query_binding: 14 tests passed\n");
    }
    return failures;
}
