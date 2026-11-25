#include "vss_warning_manager.h"

#include <stdio.h>
#include <string.h>

#define CHECK(c) do { if (!(c)) { \
    (void)printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #c); return 1; \
} } while (0)

static DomainIf_VssWarningState_t g_last;
static unsigned g_txCount;
static DomainIf_Status_t g_txStatus;

static DomainIf_Status_t Capture(const DomainIf_VssWarningState_t *state)
{
    g_last = *state;
    ++g_txCount;
    return g_txStatus;
}

static CisRearState_t Rear(
    uint16_t distance,
    RearMeasurementState_t measurement,
    uint32_t generation,
    uint32_t sequence,
    DataQuality_t quality)
{
    CisRearState_t rear;
    (void)memset(&rear, 0, sizeof(rear));
    rear.rearDistanceMm = distance;
    rear.measurementState = measurement;
    rear.distanceQuality.validity = VALUE_VALIDITY_VALID;
    rear.distanceQuality.reason = QUALITY_REASON_NONE;
    rear.distanceQuality.meta.quality = quality;
    rear.distanceQuality.meta.updateSequence = sequence;
    rear.sourceGeneration = generation;
    return rear;
}

static int Put(
    VehicleStateManager_t *vsm,
    CisRearState_t *rear,
    uint32_t receivedAt)
{
    VehicleStateManager_Update_t update;
    update.value = rear;
    update.valueSize = sizeof(*rear);
    update.quality = (VehicleStateQuality_t)rear->distanceQuality.meta.quality;
    update.sourceAgeMs = rear->distanceQuality.meta.ageMs;
    update.updateSequence = rear->distanceQuality.meta.updateSequence;
    update.sourceGeneration = rear->sourceGeneration;
    update.receivedAtMs = receivedAt;
    return VehicleStateManager_Update(vsm, 0U, &update) ==
           VEHICLE_STATE_MANAGER_OK ? 0 : 1;
}

static int Setup(
    VehicleStateManager_t *vsm,
    VehicleStateManager_Slot_t *slot,
    VehicleStateManager_SlotConfig_t *config,
    CisRearState_t *storage,
    VssWarningManager_t *manager)
{
    config->valueStorage = storage;
    config->valueCapacity = sizeof(*storage);
    CHECK(VehicleStateManager_Init(vsm, slot, config, 1U) ==
          VEHICLE_STATE_MANAGER_OK);
    CHECK(VssWarningManager_Init(manager, vsm, 0U, Capture) ==
          VSS_WARNING_MANAGER_OK);
    g_txCount = 0U;
    g_txStatus = DOMAIN_IF_OK;
    return 0;
}

static int Boundaries(void)
{
    static const struct { uint16_t mm; RearObstacleState_t state; } cases[] = {
        {100U, REAR_OBSTACLE_STATE_EMERGENCY},
        {400U, REAR_OBSTACLE_STATE_EMERGENCY},
        {401U, REAR_OBSTACLE_STATE_CAUTION},
        {700U, REAR_OBSTACLE_STATE_CAUTION},
        {701U, REAR_OBSTACLE_STATE_CLEAR},
        {1000U, REAR_OBSTACLE_STATE_CLEAR}
    };
    VehicleStateManager_t vsm;
    VehicleStateManager_Slot_t slot;
    VehicleStateManager_SlotConfig_t config;
    VssWarningManager_t manager;
    CisRearState_t storage;
    size_t i;
    CHECK(Setup(&vsm, &slot, &config, &storage, &manager) == 0);
    for (i = 0U; i < sizeof(cases) / sizeof(cases[0]); ++i)
    {
        CisRearState_t rear = Rear(cases[i].mm,
            REAR_MEASUREMENT_VALID_DISTANCE, 7U, (uint32_t)i + 1U,
            DATA_QUALITY_OK);
        CHECK(Put(&vsm, &rear, (uint32_t)i * 10U) == 0);
        CHECK(VssWarningManager_Poll(&manager, (uint32_t)i * 10U) ==
              VSS_WARNING_MANAGER_OK);
        CHECK(g_last.rearObstacleState == cases[i].state);
        CHECK(g_last.rearEvidence.quality == DATA_QUALITY_OK);
        CHECK(g_last.rearEvidence.source.generation == 7U);
        CHECK(g_last.rearEvidence.source.sequence == (uint32_t)i + 1U);
    }
    CHECK(g_last.occupantHazardState == OCCUPANT_HAZARD_UNCONFIRMED);
    CHECK(g_last.occupantEvidence.quality == DATA_QUALITY_NO_DATA);
    CHECK(g_last.antiPinchEvidence.availability ==
          FUNCTION_AVAILABILITY_UNSUPPORTED);
    return 0;
}

static int NoObjectInvalidAndTtl(void)
{
    VehicleStateManager_t vsm;
    VehicleStateManager_Slot_t slot;
    VehicleStateManager_SlotConfig_t config;
    VssWarningManager_t manager;
    CisRearState_t storage;
    CisRearState_t rear;
    CHECK(Setup(&vsm, &slot, &config, &storage, &manager) == 0);
    rear = Rear(0U, REAR_MEASUREMENT_NO_OBJECT, 3U, 1U,
                DATA_QUALITY_OK);
    CHECK(Put(&vsm, &rear, 100U) == 0);
    CHECK(VssWarningManager_Poll(&manager, 100U) == VSS_WARNING_MANAGER_OK);
    CHECK(g_last.rearObstacleState == REAR_OBSTACLE_STATE_CLEAR);
    CHECK(g_last.rearEvidence.quality == DATA_QUALITY_OK);
    CHECK(VssWarningManager_Poll(&manager, 249U) == VSS_WARNING_MANAGER_OK);
    CHECK(g_txCount == 1U);
    CHECK(VssWarningManager_Poll(&manager, 250U) == VSS_WARNING_MANAGER_OK);
    CHECK(g_txCount == 2U);
    CHECK(g_last.rearEvidence.quality == DATA_QUALITY_STALE);
    CHECK(g_last.rearEvidence.reason == QUALITY_REASON_STALE);

    rear = Rear(99U, REAR_MEASUREMENT_VALID_DISTANCE, 3U, 2U,
                DATA_QUALITY_OK);
    CHECK(Put(&vsm, &rear, 260U) == 0);
    CHECK(VssWarningManager_Poll(&manager, 260U) == VSS_WARNING_MANAGER_OK);
    CHECK(g_last.rearEvidence.quality == DATA_QUALITY_INVALID);
    CHECK(g_last.rearEvidence.reason == QUALITY_REASON_OUT_OF_RANGE);
    CHECK(g_last.rearObstacleState == REAR_OBSTACLE_STATE_CLEAR);

    rear = Rear(300U, REAR_MEASUREMENT_FAULT, 3U, 3U,
                DATA_QUALITY_OK);
    CHECK(Put(&vsm, &rear, 270U) == 0);
    CHECK(VssWarningManager_Poll(&manager, 270U) == VSS_WARNING_MANAGER_OK);
    CHECK(g_last.rearEvidence.quality == DATA_QUALITY_INVALID);
    return 0;
}

static int PendingAndRollover(void)
{
    VehicleStateManager_t vsm;
    VehicleStateManager_Slot_t slot;
    VehicleStateManager_SlotConfig_t config;
    VssWarningManager_t manager;
    CisRearState_t storage;
    CisRearState_t rear;
    CHECK(Setup(&vsm, &slot, &config, &storage, &manager) == 0);
    rear = Rear(500U, REAR_MEASUREMENT_VALID_DISTANCE, 9U, 8U,
                DATA_QUALITY_OK);
    CHECK(Put(&vsm, &rear, UINT32_MAX - 50U) == 0);
    g_txStatus = DOMAIN_IF_NOT_AVAILABLE;
    CHECK(VssWarningManager_Poll(&manager, UINT32_MAX - 40U) ==
          VSS_WARNING_MANAGER_TX_PENDING);
    CHECK(g_txCount == 1U);
    g_txStatus = DOMAIN_IF_OK;
    CHECK(VssWarningManager_Poll(&manager, 20U) == VSS_WARNING_MANAGER_OK);
    CHECK(g_txCount == 2U);
    CHECK(g_last.rearEvidence.source.sequence == 8U);
    CHECK(g_last.rearEvidence.source.ageMs == 71U);
    return 0;
}

static int OccupantPolicyIsFailClosed(void)
{
    VssWarningManager_OccupantInput_t input;
    OccupantHazardState_t state;
    DomainIf_WarningInputEvidence_t evidence;

    (void)memset(&input, 0, sizeof(input));
    input.vehicleUseEnded = true;
    input.userDeparted = true;
    input.occupantPresent = true;
    input.combinedEvidence.quality = DATA_QUALITY_OK;
    input.combinedEvidence.reason = QUALITY_REASON_NONE;
    input.combinedEvidence.availability = FUNCTION_AVAILABILITY_AVAILABLE;
    input.combinedEvidence.source.generation = 9U;
    input.combinedEvidence.source.sequence = 7U;
    input.combinedEvidence.source.ageMs = 999U;
    CHECK(VssWarningManager_EvaluateOccupant(&input, &state, &evidence) ==
          VSS_WARNING_MANAGER_OK);
    CHECK(state == OCCUPANT_HAZARD_ACTIVE);
    CHECK(evidence.source.generation == 9U);
    CHECK(evidence.source.sequence == 7U);

    input.combinedEvidence.source.ageMs = 1000U;
    CHECK(VssWarningManager_EvaluateOccupant(&input, &state, &evidence) ==
          VSS_WARNING_MANAGER_NOT_AVAILABLE);
    CHECK(state == OCCUPANT_HAZARD_UNCONFIRMED);

    input.combinedEvidence.source.ageMs = 0U;
    input.userDeparted = false;
    CHECK(VssWarningManager_EvaluateOccupant(&input, &state, &evidence) ==
          VSS_WARNING_MANAGER_OK);
    CHECK(state == OCCUPANT_HAZARD_CLEAR);
    return 0;
}

int main(void)
{
    int failures = 0;
    failures += Boundaries();
    failures += NoObjectInvalidAndTtl();
    failures += PendingAndRollover();
    failures += OccupantPolicyIsFailClosed();
    if (failures == 0) (void)printf("PASS vss_warning_manager\n");
    return failures == 0 ? 0 : 1;
}
