#include "domain_interface_state_binding.h"

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
    bool available;
    uint32_t calls;
} TestClock_t;

typedef struct
{
    VehicleStateManager_t manager;
    VehicleStateManager_Slot_t slots[SLOT_COUNT];
    VehicleStateManager_SlotConfig_t configs[SLOT_COUNT];
    DoorState_t doorStorage;
    DomainIf_BcmStatusReport_t bcmStatusStorage;
    CisRearState_t cisRearStorage;
    DomainIf_CisStatusReport_t cisStatusStorage;
    DomainIfStateBinding_SlotMap_t slotMap;
    TestClock_t clock;
} Fixture_t;

static Fixture_t *g_observerFixture;
static DomainIf_Status_t g_observerReturn;
static uint32_t g_bcmObserverCalls;
static uint32_t g_cisObserverCalls;
static bool g_observerSawCommittedState;

static bool TestClock_GetNow(void *context, uint32_t *outNowMs)
{
    TestClock_t *clock = (TestClock_t *)context;

    ++clock->calls;
    if (!clock->available)
    {
        return false;
    }
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

static DomainIf_BcmStatusReport_t BcmStatus(
    UpdateSequence_t sequence,
    SourceGeneration_t generation)
{
    DomainIf_BcmStatusReport_t state;

    (void)memset(&state, 0, sizeof(state));
    state.ecuState = ECU_STATE_READY;
    state.sourceGeneration = generation;
    state.meta = Meta(DATA_QUALITY_OK, 4U, sequence);
    return state;
}

static CisRearState_t CisRear(
    UpdateSequence_t sequence,
    SourceGeneration_t generation)
{
    CisRearState_t state;

    (void)memset(&state, 0, sizeof(state));
    state.rearDistanceMm = 1234U;
    state.measurementState = REAR_MEASUREMENT_VALID_DISTANCE;
    state.distanceQuality.validity = VALUE_VALIDITY_VALID;
    state.distanceQuality.reason = QUALITY_REASON_NONE;
    state.distanceQuality.meta = Meta(DATA_QUALITY_OK, 8U, sequence);
    state.sourceGeneration = generation;
    return state;
}

static DomainIf_CisStatusReport_t CisStatus(
    UpdateSequence_t sequence,
    SourceGeneration_t generation)
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
    state.sourceGeneration = generation;
    state.meta = Meta(DATA_QUALITY_OK, 6U, sequence);
    return state;
}

static void PrepareFixtureStorage(Fixture_t *fixture)
{
    (void)memset(fixture, 0, sizeof(*fixture));
    fixture->configs[SLOT_BCM_DOOR].valueStorage = &fixture->doorStorage;
    fixture->configs[SLOT_BCM_DOOR].valueCapacity = sizeof(fixture->doorStorage);
    fixture->configs[SLOT_BCM_STATUS].valueStorage =
        &fixture->bcmStatusStorage;
    fixture->configs[SLOT_BCM_STATUS].valueCapacity =
        sizeof(fixture->bcmStatusStorage);
    fixture->configs[SLOT_CIS_REAR].valueStorage = &fixture->cisRearStorage;
    fixture->configs[SLOT_CIS_REAR].valueCapacity =
        sizeof(fixture->cisRearStorage);
    fixture->configs[SLOT_CIS_STATUS].valueStorage =
        &fixture->cisStatusStorage;
    fixture->configs[SLOT_CIS_STATUS].valueCapacity =
        sizeof(fixture->cisStatusStorage);
    fixture->slotMap.bcmDoorState = SLOT_BCM_DOOR;
    fixture->slotMap.bcmStatus = SLOT_BCM_STATUS;
    fixture->slotMap.cisRearState = SLOT_CIS_REAR;
    fixture->slotMap.cisStatus = SLOT_CIS_STATUS;
    fixture->clock.nowMs = 100U;
    fixture->clock.available = true;
}

static int ConfigureFixture(Fixture_t *fixture)
{
    PrepareFixtureStorage(fixture);
    ASSERT_EQ(VEHICLE_STATE_MANAGER_OK,
              VehicleStateManager_Init(&fixture->manager, fixture->slots,
                                       fixture->configs, SLOT_COUNT));
    ASSERT_EQ(DOMAIN_IF_OK, DomainIf_Init());
    ASSERT_EQ(DOMAIN_IF_OK, DomainIf_SetRxObserver(NULL));
    ASSERT_EQ(DOMAIN_IF_OK,
              DomainIfStateBinding_Configure(
                  &fixture->manager, &fixture->slotMap,
                  TestClock_GetNow, &fixture->clock));
    return 0;
}

static int TestPreInitAndConfigurationValidation(void)
{
    Fixture_t fixture;
    DoorState_t door = DoorState(DATA_QUALITY_OK, 1U, 1U);

    PrepareFixtureStorage(&fixture);
    ASSERT_EQ(DOMAIN_IF_INVALID_ARGUMENT, DomainIf_RxBcmDoorState(NULL));
    ASSERT_EQ(DOMAIN_IF_NOT_INITIALIZED,
              DomainIf_RxBcmDoorState(&door));
    ASSERT_EQ(DOMAIN_IF_NOT_INITIALIZED,
              DomainIfStateBinding_Configure(
                  &fixture.manager, &fixture.slotMap,
                  TestClock_GetNow, &fixture.clock));
    ASSERT_EQ(DOMAIN_IF_OK, DomainIf_Init());
    ASSERT_EQ(DOMAIN_IF_NOT_AVAILABLE,
              DomainIf_RxBcmDoorState(&door));
    ASSERT_EQ(DOMAIN_IF_INVALID_ARGUMENT,
              DomainIfStateBinding_Configure(
                  NULL, &fixture.slotMap, TestClock_GetNow, &fixture.clock));
    ASSERT_EQ(DOMAIN_IF_INVALID_ARGUMENT,
              DomainIfStateBinding_Configure(
                  &fixture.manager, NULL, TestClock_GetNow, &fixture.clock));
    ASSERT_EQ(DOMAIN_IF_INVALID_ARGUMENT,
              DomainIfStateBinding_Configure(
                  &fixture.manager, &fixture.slotMap, NULL, &fixture.clock));
    ASSERT_EQ(DOMAIN_IF_NOT_AVAILABLE,
              DomainIfStateBinding_Configure(
                  &fixture.manager, &fixture.slotMap,
                  TestClock_GetNow, &fixture.clock));
    ASSERT_EQ(VEHICLE_STATE_MANAGER_OK,
              VehicleStateManager_Init(&fixture.manager, fixture.slots,
                                       fixture.configs, SLOT_COUNT));
    fixture.slotMap.cisStatus = fixture.slotMap.cisRearState;
    ASSERT_EQ(DOMAIN_IF_INVALID_ARGUMENT,
              DomainIfStateBinding_Configure(
                  &fixture.manager, &fixture.slotMap,
                  TestClock_GetNow, &fixture.clock));
    return 0;
}

static int TestDoorSnapshotCopiesValueAndMetadata(void)
{
    Fixture_t fixture;
    DoorState_t input = DoorState(DATA_QUALITY_OK, 9U, 3U);
    DoorState_t output;
    VehicleStateManager_Snapshot_t snapshot;

    ASSERT_EQ(0, ConfigureFixture(&fixture));
    fixture.clock.nowMs = 123U;
    ASSERT_EQ(DOMAIN_IF_OK, DomainIf_RxBcmDoorState(&input));
    (void)memset(&output, 0, sizeof(output));
    ASSERT_EQ(VEHICLE_STATE_MANAGER_OK,
              VehicleStateManager_GetSnapshot(
                  &fixture.manager, SLOT_BCM_DOOR, 123U, UINT32_MAX,
                  &output, sizeof(output), &snapshot));
    ASSERT_TRUE(memcmp(&input, &output, sizeof(input)) == 0);
    ASSERT_EQ(VEHICLE_STATE_QUALITY_OK, snapshot.quality);
    ASSERT_EQ(7U, snapshot.sourceAgeMs);
    ASSERT_EQ(9U, snapshot.updateSequence);
    ASSERT_EQ(3U, snapshot.sourceGeneration);
    ASSERT_EQ(123U, snapshot.receivedAtMs);
    ASSERT_EQ(sizeof(input), snapshot.valueSize);
    return 0;
}

static int TestQualityAgeAndNoDataPreserved(void)
{
    Fixture_t fixture;
    DoorState_t state = DoorState(DATA_QUALITY_STALE, 1U, 1U);
    DoorState_t output;
    VehicleStateManager_Snapshot_t snapshot;

    ASSERT_EQ(0, ConfigureFixture(&fixture));
    state.meta.ageMs = UINT32_MAX - 2U;
    fixture.clock.nowMs = UINT32_MAX - 1U;
    ASSERT_EQ(DOMAIN_IF_OK, DomainIf_RxBcmDoorState(&state));
    ASSERT_EQ(VEHICLE_STATE_MANAGER_OK,
              VehicleStateManager_GetSnapshot(
                  &fixture.manager, SLOT_BCM_DOOR, 3U, UINT32_MAX,
                  &output, sizeof(output), &snapshot));
    ASSERT_EQ(VEHICLE_STATE_QUALITY_STALE, snapshot.quality);
    ASSERT_EQ(UINT32_MAX, snapshot.effectiveAgeMs);

    state = DoorState(DATA_QUALITY_NO_DATA, 2U, 2U);
    state.meta.ageMs = 55U;
    fixture.clock.nowMs = 4U;
    ASSERT_EQ(DOMAIN_IF_OK, DomainIf_RxBcmDoorState(&state));
    ASSERT_EQ(VEHICLE_STATE_MANAGER_OK,
              VehicleStateManager_GetSnapshot(
                  &fixture.manager, SLOT_BCM_DOOR, 4U, UINT32_MAX,
                  NULL, 0U, &snapshot));
    ASSERT_TRUE(snapshot.hasObservation);
    ASSERT_TRUE(!snapshot.hasValue);
    ASSERT_EQ(VEHICLE_STATE_QUALITY_NO_DATA, snapshot.quality);
    ASSERT_EQ(55U, snapshot.sourceAgeMs);
    ASSERT_EQ(2U, snapshot.updateSequence);
    ASSERT_EQ(2U, snapshot.sourceGeneration);
    ASSERT_TRUE(snapshot.sourceGenerationChanged);
    return 0;
}

static int TestSequenceDuplicateReverseAndRollover(void)
{
    Fixture_t fixture;
    DoorState_t state = DoorState(DATA_QUALITY_OK, UINT32_MAX - 1U, 5U);
    DoorState_t output;
    VehicleStateManager_Snapshot_t snapshot;

    ASSERT_EQ(0, ConfigureFixture(&fixture));
    fixture.clock.nowMs = 10U;
    ASSERT_EQ(DOMAIN_IF_OK, DomainIf_RxBcmDoorState(&state));
    state.meta.updateSequence = UINT32_MAX;
    state.lockState = DOOR_LOCK_STATE_UNLOCKED;
    fixture.clock.nowMs = 11U;
    ASSERT_EQ(DOMAIN_IF_OK, DomainIf_RxBcmDoorState(&state));
    state.meta.updateSequence = 0U;
    state.openState = DOOR_OPEN_STATE_OPEN;
    fixture.clock.nowMs = 12U;
    ASSERT_EQ(DOMAIN_IF_OK, DomainIf_RxBcmDoorState(&state));

    fixture.clock.nowMs = 13U;
    ASSERT_EQ(DOMAIN_IF_OK, DomainIf_RxBcmDoorState(&state));
    state.meta.updateSequence = UINT32_MAX;
    state.openState = DOOR_OPEN_STATE_CLOSED;
    fixture.clock.nowMs = 14U;
    ASSERT_EQ(DOMAIN_IF_REJECTED, DomainIf_RxBcmDoorState(&state));
    ASSERT_EQ(VEHICLE_STATE_MANAGER_OK,
              VehicleStateManager_GetSnapshot(
                  &fixture.manager, SLOT_BCM_DOOR, 14U, UINT32_MAX,
                  &output, sizeof(output), &snapshot));
    ASSERT_EQ(0U, snapshot.updateSequence);
    ASSERT_EQ(12U, snapshot.receivedAtMs);
    ASSERT_EQ(DOOR_OPEN_STATE_OPEN, output.openState);
    return 0;
}

static int TestGenerationChangeAcceptsNewSequence(void)
{
    Fixture_t fixture;
    DoorState_t state = DoorState(DATA_QUALITY_OK, 100U, 7U);
    DoorState_t output;
    VehicleStateManager_Snapshot_t snapshot;

    ASSERT_EQ(0, ConfigureFixture(&fixture));
    ASSERT_EQ(DOMAIN_IF_OK, DomainIf_RxBcmDoorState(&state));
    state.sourceGeneration = 8U;
    state.meta.updateSequence = 1U;
    state.lockState = DOOR_LOCK_STATE_UNLOCKED;
    fixture.clock.nowMs = 101U;
    ASSERT_EQ(DOMAIN_IF_OK, DomainIf_RxBcmDoorState(&state));
    ASSERT_EQ(VEHICLE_STATE_MANAGER_OK,
              VehicleStateManager_GetSnapshot(
                  &fixture.manager, SLOT_BCM_DOOR, 101U, UINT32_MAX,
                  &output, sizeof(output), &snapshot));
    ASSERT_EQ(8U, snapshot.sourceGeneration);
    ASSERT_EQ(1U, snapshot.updateSequence);
    ASSERT_TRUE(snapshot.sourceGenerationChanged);
    ASSERT_EQ(DOOR_LOCK_STATE_UNLOCKED, output.lockState);
    return 0;
}

static int TestInvalidInputAndClockFailureDoNotMutate(void)
{
    Fixture_t fixture;
    DoorState_t state = DoorState(DATA_QUALITY_OK, 1U, 1U);
    DoorState_t output;
    VehicleStateManager_Snapshot_t snapshot;
    uint32_t clockCalls;

    ASSERT_EQ(0, ConfigureFixture(&fixture));
    ASSERT_EQ(DOMAIN_IF_OK, DomainIf_RxBcmDoorState(&state));
    clockCalls = fixture.clock.calls;
    state.lockState = (DoorLockState_t)99;
    ASSERT_EQ(DOMAIN_IF_INVALID_ARGUMENT,
              DomainIf_RxBcmDoorState(&state));
    ASSERT_EQ(clockCalls, fixture.clock.calls);

    state = DoorState(DATA_QUALITY_OK, 2U, 1U);
    state.lockState = DOOR_LOCK_STATE_UNLOCKED;
    fixture.clock.available = false;
    ASSERT_EQ(DOMAIN_IF_NOT_AVAILABLE,
              DomainIf_RxBcmDoorState(&state));
    ASSERT_EQ(VEHICLE_STATE_MANAGER_OK,
              VehicleStateManager_GetSnapshot(
                  &fixture.manager, SLOT_BCM_DOOR, 100U, UINT32_MAX,
                  &output, sizeof(output), &snapshot));
    ASSERT_EQ(1U, snapshot.updateSequence);
    ASSERT_EQ(DOOR_LOCK_STATE_LOCKED, output.lockState);
    return 0;
}

static DomainIf_Status_t ObserveBcmStatus(
    const DomainIf_BcmStatusReport_t *status)
{
    DomainIf_BcmStatusReport_t stored;
    VehicleStateManager_Snapshot_t snapshot;

    ++g_bcmObserverCalls;
    g_observerSawCommittedState =
        (g_observerFixture != NULL) &&
        (VehicleStateManager_GetSnapshot(
             &g_observerFixture->manager, SLOT_BCM_STATUS,
             g_observerFixture->clock.nowMs, UINT32_MAX,
             &stored, sizeof(stored), &snapshot) == VEHICLE_STATE_MANAGER_OK) &&
        (stored.meta.updateSequence == status->meta.updateSequence);
    return g_observerReturn;
}

static DomainIf_Status_t ObserveCisStatus(
    const DomainIf_CisStatusReport_t *status)
{
    DomainIf_CisStatusReport_t stored;
    VehicleStateManager_Snapshot_t snapshot;

    ++g_cisObserverCalls;
    g_observerSawCommittedState =
        (g_observerFixture != NULL) &&
        (VehicleStateManager_GetSnapshot(
             &g_observerFixture->manager, SLOT_CIS_STATUS,
             g_observerFixture->clock.nowMs, UINT32_MAX,
             &stored, sizeof(stored), &snapshot) == VEHICLE_STATE_MANAGER_OK) &&
        (stored.meta.updateSequence == status->meta.updateSequence);
    return g_observerReturn;
}

static int TestBcmStatusCommitBeforeObserverAndNoReplay(void)
{
    Fixture_t fixture;
    DomainIf_BcmStatusReport_t state = BcmStatus(1U, 10U);
    DomainIf_BcmStatusReport_t output;
    VehicleStateManager_Snapshot_t snapshot;
    DomainIf_RxObserver_t observer;

    ASSERT_EQ(0, ConfigureFixture(&fixture));
    (void)memset(&observer, 0, sizeof(observer));
    observer.onBcmStatus = ObserveBcmStatus;
    ASSERT_EQ(DOMAIN_IF_OK, DomainIf_SetRxObserver(&observer));
    g_observerFixture = &fixture;
    g_observerReturn = DOMAIN_IF_REJECTED;
    g_bcmObserverCalls = 0U;
    g_observerSawCommittedState = false;

    ASSERT_EQ(DOMAIN_IF_REJECTED, DomainIf_RxBcmStatus(&state));
    ASSERT_EQ(1U, g_bcmObserverCalls);
    ASSERT_TRUE(g_observerSawCommittedState);
    ASSERT_EQ(DOMAIN_IF_OK, DomainIf_RxBcmStatus(&state));
    ASSERT_EQ(1U, g_bcmObserverCalls);

    state.meta.updateSequence = 0U;
    state.ecuState = ECU_STATE_FAULT;
    ASSERT_EQ(DOMAIN_IF_REJECTED, DomainIf_RxBcmStatus(&state));
    ASSERT_EQ(1U, g_bcmObserverCalls);
    ASSERT_EQ(VEHICLE_STATE_MANAGER_OK,
              VehicleStateManager_GetSnapshot(
                  &fixture.manager, SLOT_BCM_STATUS, fixture.clock.nowMs,
                  UINT32_MAX, &output, sizeof(output), &snapshot));
    ASSERT_EQ(ECU_STATE_READY, output.ecuState);
    return 0;
}

static int TestCisStatusCommitThenObserver(void)
{
    Fixture_t fixture;
    DomainIf_CisStatusReport_t state = CisStatus(3U, 20U);
    DomainIf_RxObserver_t observer;

    ASSERT_EQ(0, ConfigureFixture(&fixture));
    (void)memset(&observer, 0, sizeof(observer));
    observer.onCisStatus = ObserveCisStatus;
    ASSERT_EQ(DOMAIN_IF_OK, DomainIf_SetRxObserver(&observer));
    g_observerFixture = &fixture;
    g_observerReturn = DOMAIN_IF_OK;
    g_cisObserverCalls = 0U;
    g_observerSawCommittedState = false;

    ASSERT_EQ(DOMAIN_IF_OK, DomainIf_RxCisStatus(&state));
    ASSERT_EQ(1U, g_cisObserverCalls);
    ASSERT_TRUE(g_observerSawCommittedState);
    state.canInterfaceStatus = (DomainIf_CisInterfaceStatus_t)1;
    ASSERT_EQ(DOMAIN_IF_INVALID_ARGUMENT,
              DomainIf_RxCisStatus(&state));
    ASSERT_EQ(1U, g_cisObserverCalls);
    return 0;
}

static int TestCisRearCopyAndValidation(void)
{
    Fixture_t fixture;
    CisRearState_t state = CisRear(4U, 30U);
    CisRearState_t output;
    VehicleStateManager_Snapshot_t snapshot;

    ASSERT_EQ(0, ConfigureFixture(&fixture));
    ASSERT_EQ(DOMAIN_IF_OK, DomainIf_RxCisRear(&state));
    ASSERT_EQ(VEHICLE_STATE_MANAGER_OK,
              VehicleStateManager_GetSnapshot(
                  &fixture.manager, SLOT_CIS_REAR, fixture.clock.nowMs,
                  UINT32_MAX, &output, sizeof(output), &snapshot));
    ASSERT_TRUE(memcmp(&state, &output, sizeof(state)) == 0);
    ASSERT_EQ(8U, snapshot.sourceAgeMs);
    ASSERT_EQ(4U, snapshot.updateSequence);
    ASSERT_EQ(30U, snapshot.sourceGeneration);

    state.measurementState = (RearMeasurementState_t)99;
    ASSERT_EQ(DOMAIN_IF_INVALID_ARGUMENT, DomainIf_RxCisRear(&state));
    state = CisRear(5U, 30U);
    state.distanceQuality.validity = (ValueValidity_t)99;
    ASSERT_EQ(DOMAIN_IF_INVALID_ARGUMENT, DomainIf_RxCisRear(&state));
    return 0;
}

static int TestCisRearSameSequenceQualityWithdrawal(void)
{
    Fixture_t fixture;
    CisRearState_t state = CisRear(4U, 30U);
    CisRearState_t output;
    VehicleStateManager_Snapshot_t snapshot;

    ASSERT_EQ(0, ConfigureFixture(&fixture));
    fixture.clock.nowMs = 100U;
    ASSERT_EQ(DOMAIN_IF_OK, DomainIf_RxCisRear(&state));

    fixture.clock.nowMs = 105U;
    ASSERT_EQ(DOMAIN_IF_OK, DomainIf_RxCisRear(&state));
    ASSERT_EQ(VEHICLE_STATE_MANAGER_OK,
              VehicleStateManager_GetSnapshot(
                  &fixture.manager, SLOT_CIS_REAR, 105U, UINT32_MAX,
                  &output, sizeof(output), &snapshot));
    ASSERT_EQ(100U, snapshot.receivedAtMs);
    ASSERT_EQ(13U, snapshot.effectiveAgeMs);

    state.distanceQuality.validity = VALUE_VALIDITY_INVALID;
    state.distanceQuality.reason = QUALITY_REASON_SENSOR_FAULT;
    state.distanceQuality.meta.quality = DATA_QUALITY_INVALID;
    state.distanceQuality.meta.ageMs = 1U;
    fixture.clock.nowMs = 110U;
    ASSERT_EQ(DOMAIN_IF_OK, DomainIf_RxCisRear(&state));
    ASSERT_EQ(VEHICLE_STATE_MANAGER_OK,
              VehicleStateManager_GetSnapshot(
                  &fixture.manager, SLOT_CIS_REAR, 110U, UINT32_MAX,
                  &output, sizeof(output), &snapshot));
    ASSERT_EQ(VEHICLE_STATE_QUALITY_INVALID, snapshot.quality);
    ASSERT_EQ(VALUE_VALIDITY_INVALID, output.distanceQuality.validity);
    ASSERT_EQ(QUALITY_REASON_SENSOR_FAULT, output.distanceQuality.reason);
    ASSERT_EQ(18U, snapshot.sourceAgeMs);
    ASSERT_EQ(4U, snapshot.updateSequence);
    ASSERT_EQ(30U, snapshot.sourceGeneration);
    return 0;
}

static int TestCisRearSupportedMeasurementStatesPreserved(void)
{
    static const RearMeasurementState_t states[] = {
        REAR_MEASUREMENT_VALID_DISTANCE,
        REAR_MEASUREMENT_NO_OBJECT,
        REAR_MEASUREMENT_UNAVAILABLE,
        REAR_MEASUREMENT_FAULT,
        REAR_MEASUREMENT_RECOVERING,
        REAR_MEASUREMENT_INACTIVE
    };
    Fixture_t fixture;
    CisRearState_t state = CisRear(1U, 30U);
    CisRearState_t output;
    VehicleStateManager_Snapshot_t snapshot;
    size_t index;

    ASSERT_EQ(0, ConfigureFixture(&fixture));
    for (index = 0U; index < (sizeof(states) / sizeof(states[0])); ++index)
    {
        state.measurementState = states[index];
        state.rearDistanceMm = (states[index] == REAR_MEASUREMENT_NO_OBJECT)
                                   ? 0U
                                   : 1234U;
        state.distanceQuality.meta.updateSequence = (UpdateSequence_t)(index + 1U);
        ASSERT_EQ(DOMAIN_IF_OK, DomainIf_RxCisRear(&state));
        ASSERT_EQ(VEHICLE_STATE_MANAGER_OK,
                  VehicleStateManager_GetSnapshot(
                      &fixture.manager, SLOT_CIS_REAR, fixture.clock.nowMs,
                      UINT32_MAX, &output, sizeof(output), &snapshot));
        ASSERT_EQ(states[index], output.measurementState);
        ASSERT_EQ(VEHICLE_STATE_QUALITY_OK, snapshot.quality);
    }
    return 0;
}

static int TestCisRearStaleIsNotPromoted(void)
{
    Fixture_t fixture;
    CisRearState_t state = CisRear(4U, 30U);
    CisRearState_t output;
    VehicleStateManager_Snapshot_t snapshot;

    ASSERT_EQ(0, ConfigureFixture(&fixture));
    state.distanceQuality.validity = VALUE_VALIDITY_INVALID;
    state.distanceQuality.reason = QUALITY_REASON_STALE;
    state.distanceQuality.meta.quality = DATA_QUALITY_STALE;
    ASSERT_EQ(DOMAIN_IF_OK, DomainIf_RxCisRear(&state));
    ASSERT_EQ(VEHICLE_STATE_MANAGER_OK,
              VehicleStateManager_GetSnapshot(
                  &fixture.manager, SLOT_CIS_REAR, fixture.clock.nowMs,
                  UINT32_MAX, &output, sizeof(output), &snapshot));
    ASSERT_EQ(VEHICLE_STATE_QUALITY_STALE, snapshot.quality);
    ASSERT_EQ(DATA_QUALITY_STALE, output.distanceQuality.meta.quality);
    ASSERT_EQ(REAR_MEASUREMENT_VALID_DISTANCE, output.measurementState);
    return 0;
}

static int TestCisRearSameSequenceNoDataClearsValue(void)
{
    Fixture_t fixture;
    CisRearState_t state = CisRear(4U, 30U);
    VehicleStateManager_Snapshot_t snapshot;

    ASSERT_EQ(0, ConfigureFixture(&fixture));
    ASSERT_EQ(DOMAIN_IF_OK, DomainIf_RxCisRear(&state));
    state.distanceQuality.validity = VALUE_VALIDITY_INVALID;
    state.distanceQuality.reason = QUALITY_REASON_NO_DATA;
    state.distanceQuality.meta.quality = DATA_QUALITY_NO_DATA;
    fixture.clock.nowMs = 101U;
    ASSERT_EQ(DOMAIN_IF_OK, DomainIf_RxCisRear(&state));
    ASSERT_EQ(VEHICLE_STATE_MANAGER_OK,
              VehicleStateManager_GetSnapshot(
                  &fixture.manager, SLOT_CIS_REAR, 101U, UINT32_MAX,
                  NULL, 0U, &snapshot));
    ASSERT_TRUE(!snapshot.hasValue);
    ASSERT_EQ(VEHICLE_STATE_QUALITY_NO_DATA, snapshot.quality);
    return 0;
}

static int TestCisRearSameSequenceRecoveryAndConflictRejected(void)
{
    Fixture_t fixture;
    CisRearState_t state = CisRear(4U, 30U);
    CisRearState_t output;
    VehicleStateManager_Snapshot_t snapshot;

    ASSERT_EQ(0, ConfigureFixture(&fixture));
    state.distanceQuality.validity = VALUE_VALIDITY_INVALID;
    state.distanceQuality.reason = QUALITY_REASON_SENSOR_FAULT;
    state.distanceQuality.meta.quality = DATA_QUALITY_INVALID;
    ASSERT_EQ(DOMAIN_IF_OK, DomainIf_RxCisRear(&state));

    state.distanceQuality.validity = VALUE_VALIDITY_VALID;
    state.distanceQuality.reason = QUALITY_REASON_NONE;
    state.distanceQuality.meta.quality = DATA_QUALITY_OK;
    ASSERT_EQ(DOMAIN_IF_REJECTED, DomainIf_RxCisRear(&state));
    state.distanceQuality.validity = VALUE_VALIDITY_INVALID;
    state.distanceQuality.reason = QUALITY_REASON_SENSOR_FAULT;
    state.distanceQuality.meta.quality = DATA_QUALITY_INVALID;
    state.rearDistanceMm = 4321U;
    ASSERT_EQ(DOMAIN_IF_REJECTED, DomainIf_RxCisRear(&state));

    ASSERT_EQ(VEHICLE_STATE_MANAGER_OK,
              VehicleStateManager_GetSnapshot(
                  &fixture.manager, SLOT_CIS_REAR, fixture.clock.nowMs,
                  UINT32_MAX, &output, sizeof(output), &snapshot));
    ASSERT_EQ(1234U, output.rearDistanceMm);
    ASSERT_EQ(VEHICLE_STATE_QUALITY_INVALID, snapshot.quality);
    return 0;
}

static int TestAllRxNullInputs(void)
{
    Fixture_t fixture;

    ASSERT_EQ(0, ConfigureFixture(&fixture));
    ASSERT_EQ(DOMAIN_IF_INVALID_ARGUMENT, DomainIf_RxBcmDoorState(NULL));
    ASSERT_EQ(DOMAIN_IF_INVALID_ARGUMENT, DomainIf_RxBcmStatus(NULL));
    ASSERT_EQ(DOMAIN_IF_INVALID_ARGUMENT, DomainIf_RxCisRear(NULL));
    ASSERT_EQ(DOMAIN_IF_INVALID_ARGUMENT, DomainIf_RxCisStatus(NULL));
    ASSERT_EQ(0U, fixture.clock.calls);
    return 0;
}

int main(void)
{
    int failures = 0;

    failures += TestPreInitAndConfigurationValidation();
    failures += TestDoorSnapshotCopiesValueAndMetadata();
    failures += TestQualityAgeAndNoDataPreserved();
    failures += TestSequenceDuplicateReverseAndRollover();
    failures += TestGenerationChangeAcceptsNewSequence();
    failures += TestInvalidInputAndClockFailureDoNotMutate();
    failures += TestBcmStatusCommitBeforeObserverAndNoReplay();
    failures += TestCisStatusCommitThenObserver();
    failures += TestCisRearCopyAndValidation();
    failures += TestCisRearSameSequenceQualityWithdrawal();
    failures += TestCisRearSameSequenceNoDataClearsValue();
    failures += TestCisRearSameSequenceRecoveryAndConflictRejected();
    failures += TestCisRearSupportedMeasurementStatesPreserved();
    failures += TestCisRearStaleIsNotPromoted();
    failures += TestAllRxNullInputs();

    if (failures == 0)
    {
        printf("domain_interface_state_binding: 15 tests passed\n");
    }
    return failures;
}
