#include "vss_warning_manager.h"

#include <string.h>

static DataQuality_t VssWarningManager_MapQuality(
    VehicleStateQuality_t quality)
{
    switch (quality)
    {
        case VEHICLE_STATE_QUALITY_OK:
            return DATA_QUALITY_OK;
        case VEHICLE_STATE_QUALITY_STALE:
            return DATA_QUALITY_STALE;
        case VEHICLE_STATE_QUALITY_INVALID:
            return DATA_QUALITY_INVALID;
        case VEHICLE_STATE_QUALITY_NO_DATA:
        default:
            return DATA_QUALITY_NO_DATA;
    }
}

static QualityReason_t VssWarningManager_ReasonForQuality(
    DataQuality_t quality)
{
    switch (quality)
    {
        case DATA_QUALITY_STALE:
            return QUALITY_REASON_STALE;
        case DATA_QUALITY_INVALID:
            return QUALITY_REASON_DATA_INVALID;
        case DATA_QUALITY_NO_DATA:
            return QUALITY_REASON_NO_DATA;
        case DATA_QUALITY_OK:
        default:
            return QUALITY_REASON_NONE;
    }
}

static void VssWarningManager_SetUnsupportedAntiPinch(
    DomainIf_VssWarningState_t *output)
{
    output->windowAntiPinchState = ANTIPINCH_STATE_CLEAR;
    output->antiPinchEvidence.quality = DATA_QUALITY_NO_DATA;
    output->antiPinchEvidence.reason = QUALITY_REASON_NO_DATA;
    output->antiPinchEvidence.availability =
        FUNCTION_AVAILABILITY_UNSUPPORTED;
    output->antiPinchEvidence.source.ageMs = UINT32_MAX;
}

static void VssWarningManager_SetUnconfirmedOccupant(
    DomainIf_VssWarningState_t *output)
{
    output->occupantHazardState = OCCUPANT_HAZARD_UNCONFIRMED;
    output->occupantEvidence.quality = DATA_QUALITY_NO_DATA;
    output->occupantEvidence.reason = QUALITY_REASON_NO_DATA;
    output->occupantEvidence.availability =
        FUNCTION_AVAILABILITY_UNAVAILABLE;
    output->occupantEvidence.source.ageMs = UINT32_MAX;
}

static bool VssWarningManager_IsUsableRear(
    const CisRearState_t *rear,
    const VehicleStateManager_Snapshot_t *snapshot)
{
    return snapshot->hasValue &&
           (snapshot->quality == VEHICLE_STATE_QUALITY_OK) &&
           (rear->distanceQuality.validity == VALUE_VALIDITY_VALID) &&
           (rear->distanceQuality.reason == QUALITY_REASON_NONE) &&
           (rear->distanceQuality.meta.quality == DATA_QUALITY_OK);
}

static void VssWarningManager_EvaluateRear(
    const CisRearState_t *rear,
    const VehicleStateManager_Snapshot_t *snapshot,
    DomainIf_VssWarningState_t *output,
    QualityReason_t *outReason)
{
    bool usable;

    usable = VssWarningManager_IsUsableRear(rear, snapshot);
    output->rearEvidence.source.generation = snapshot->sourceGeneration;
    output->rearEvidence.source.sequence = snapshot->updateSequence;
    output->rearEvidence.source.ageMs = snapshot->effectiveAgeMs;
    output->rearActivationMeta.updateSequence = snapshot->updateSequence;
    output->rearActivationMeta.ageMs = snapshot->effectiveAgeMs;

    if (!usable)
    {
        output->rearObstacleState = REAR_OBSTACLE_STATE_CLEAR;
        output->rearEvidence.quality =
            VssWarningManager_MapQuality(snapshot->quality);
        *outReason = snapshot->hasValue
                         ? rear->distanceQuality.reason
                         : VssWarningManager_ReasonForQuality(
                               output->rearEvidence.quality);
        if (*outReason == QUALITY_REASON_NONE)
        {
            *outReason = VssWarningManager_ReasonForQuality(
                output->rearEvidence.quality);
        }
        output->rearEvidence.reason = *outReason;
        output->rearEvidence.availability =
            FUNCTION_AVAILABILITY_UNAVAILABLE;
        output->rearDetectionActivation = REAR_DETECTION_DISABLED;
        output->rearActivationMeta.quality = output->rearEvidence.quality;
        output->rearActivationReason = *outReason;
        return;
    }

    if (rear->measurementState == REAR_MEASUREMENT_NO_OBJECT)
    {
        output->rearObstacleState = REAR_OBSTACLE_STATE_CLEAR;
    }
    else if (rear->measurementState != REAR_MEASUREMENT_VALID_DISTANCE)
    {
        output->rearObstacleState = REAR_OBSTACLE_STATE_CLEAR;
        output->rearEvidence.quality = DATA_QUALITY_INVALID;
        output->rearEvidence.reason = QUALITY_REASON_DATA_INVALID;
        output->rearEvidence.availability =
            FUNCTION_AVAILABILITY_UNAVAILABLE;
        output->rearDetectionActivation = REAR_DETECTION_DISABLED;
        output->rearActivationMeta.quality = DATA_QUALITY_INVALID;
        output->rearActivationReason = QUALITY_REASON_DATA_INVALID;
        *outReason = QUALITY_REASON_DATA_INVALID;
        return;
    }
    else if ((rear->rearDistanceMm >= 100U) &&
             (rear->rearDistanceMm <= 400U))
    {
        output->rearObstacleState = REAR_OBSTACLE_STATE_EMERGENCY;
    }
    else if ((rear->rearDistanceMm > 400U) &&
             (rear->rearDistanceMm <= 700U))
    {
        output->rearObstacleState = REAR_OBSTACLE_STATE_CAUTION;
    }
    else if ((rear->rearDistanceMm > 700U) &&
             (rear->rearDistanceMm <= 1000U))
    {
        output->rearObstacleState = REAR_OBSTACLE_STATE_CLEAR;
    }
    else
    {
        output->rearObstacleState = REAR_OBSTACLE_STATE_CLEAR;
        output->rearEvidence.quality = DATA_QUALITY_INVALID;
        output->rearEvidence.reason = QUALITY_REASON_OUT_OF_RANGE;
        output->rearEvidence.availability =
            FUNCTION_AVAILABILITY_UNAVAILABLE;
        output->rearDetectionActivation = REAR_DETECTION_DISABLED;
        output->rearActivationMeta.quality = DATA_QUALITY_INVALID;
        output->rearActivationReason = QUALITY_REASON_OUT_OF_RANGE;
        *outReason = QUALITY_REASON_OUT_OF_RANGE;
        return;
    }

    output->rearEvidence.quality = DATA_QUALITY_OK;
    output->rearEvidence.reason = QUALITY_REASON_NONE;
    output->rearEvidence.availability = FUNCTION_AVAILABILITY_AVAILABLE;
    output->rearDetectionActivation = REAR_DETECTION_ACTIVE;
    output->rearActivationMeta.quality = DATA_QUALITY_OK;
    output->rearActivationReason = QUALITY_REASON_NONE;
    *outReason = QUALITY_REASON_NONE;
}

static bool VssWarningManager_EvaluationChanged(
    const VssWarningManager_t *manager,
    const CisRearState_t *rear,
    const VehicleStateManager_Snapshot_t *snapshot,
    QualityReason_t reason)
{
    return !manager->hasLastEvaluation ||
           (manager->lastGeneration != snapshot->sourceGeneration) ||
           (manager->lastSequence != snapshot->updateSequence) ||
           (manager->lastQuality !=
            VssWarningManager_MapQuality(snapshot->quality)) ||
           (manager->lastReason != reason) ||
           (snapshot->hasValue &&
            ((manager->lastMeasurementState != rear->measurementState) ||
             (manager->lastDistanceMm != rear->rearDistanceMm)));
}

VssWarningManager_Status_t VssWarningManager_Init(
    VssWarningManager_t *manager,
    VehicleStateManager_t *vehicleStateManager,
    VehicleStateManager_SlotId_t rearSlot,
    VssWarningManager_Tx_t tx)
{
    if ((manager == NULL) || (vehicleStateManager == NULL) ||
        (tx == NULL) || !vehicleStateManager->initialized ||
        (rearSlot >= vehicleStateManager->slotCount))
    {
        return VSS_WARNING_MANAGER_INVALID_ARGUMENT;
    }
    (void)memset(manager, 0, sizeof(*manager));
    manager->vehicleStateManager = vehicleStateManager;
    manager->rearSlot = rearSlot;
    manager->tx = tx;
    manager->initialized = true;
    return VSS_WARNING_MANAGER_OK;
}

VssWarningManager_Status_t VssWarningManager_EvaluateOccupant(
    const VssWarningManager_OccupantInput_t *input,
    OccupantHazardState_t *outState,
    DomainIf_WarningInputEvidence_t *outEvidence)
{
    bool evidenceUsable;

    if ((input == NULL) || (outState == NULL) || (outEvidence == NULL))
    {
        return VSS_WARNING_MANAGER_INVALID_ARGUMENT;
    }

    *outEvidence = input->combinedEvidence;
    evidenceUsable =
        (input->combinedEvidence.quality == DATA_QUALITY_OK) &&
        (input->combinedEvidence.reason == QUALITY_REASON_NONE) &&
        (input->combinedEvidence.availability ==
         FUNCTION_AVAILABILITY_AVAILABLE) &&
        (input->combinedEvidence.source.ageMs <
         VSS_WARNING_MANAGER_OCCUPANT_TTL_MS);
    if (!evidenceUsable)
    {
        *outState = OCCUPANT_HAZARD_UNCONFIRMED;
        return VSS_WARNING_MANAGER_NOT_AVAILABLE;
    }

    *outState = (input->vehicleUseEnded && input->userDeparted &&
                 input->occupantPresent)
                    ? OCCUPANT_HAZARD_ACTIVE
                    : OCCUPANT_HAZARD_CLEAR;
    return VSS_WARNING_MANAGER_OK;
}

VssWarningManager_Status_t VssWarningManager_Poll(
    VssWarningManager_t *manager,
    uint32_t nowMs)
{
    CisRearState_t rear;
    VehicleStateManager_Snapshot_t snapshot;
    VehicleStateManager_Status_t stateStatus;
    DomainIf_VssWarningState_t output;
    DomainIf_Status_t txStatus;
    QualityReason_t reason;

    if (manager == NULL)
    {
        return VSS_WARNING_MANAGER_INVALID_ARGUMENT;
    }
    if (!manager->initialized)
    {
        return VSS_WARNING_MANAGER_NOT_INITIALIZED;
    }

    (void)memset(&rear, 0, sizeof(rear));
    (void)memset(&snapshot, 0, sizeof(snapshot));
    stateStatus = VehicleStateManager_GetSnapshot(
        manager->vehicleStateManager, manager->rearSlot, nowMs,
        VSS_WARNING_MANAGER_REAR_TTL_MS, &rear, sizeof(rear), &snapshot);
    if ((stateStatus != VEHICLE_STATE_MANAGER_OK) &&
        (stateStatus != VEHICLE_STATE_MANAGER_NO_DATA))
    {
        return VSS_WARNING_MANAGER_NOT_AVAILABLE;
    }

    (void)memset(&output, 0, sizeof(output));
    VssWarningManager_SetUnsupportedAntiPinch(&output);
    VssWarningManager_SetUnconfirmedOccupant(&output);
    VssWarningManager_EvaluateRear(&rear, &snapshot, &output, &reason);

    if (VssWarningManager_EvaluationChanged(
            manager, &rear, &snapshot, reason))
    {
        manager->pending = output;
        manager->pendingValid = true;
        manager->lastGeneration = snapshot.sourceGeneration;
        manager->lastSequence = snapshot.updateSequence;
        manager->lastQuality = VssWarningManager_MapQuality(snapshot.quality);
        manager->lastReason = reason;
        manager->lastMeasurementState = rear.measurementState;
        manager->lastDistanceMm = rear.rearDistanceMm;
        manager->hasLastEvaluation = true;
    }
    else if (manager->pendingValid)
    {
        /* A failed local handoff retains identity and payload while AGE
         * continues from the same original observation. */
        manager->pending.rearEvidence.source.ageMs =
            output.rearEvidence.source.ageMs;
        manager->pending.rearActivationMeta.ageMs =
            output.rearActivationMeta.ageMs;
    }

    if (!manager->pendingValid)
    {
        return VSS_WARNING_MANAGER_OK;
    }
    txStatus = manager->tx(&manager->pending);
    if (txStatus == DOMAIN_IF_OK)
    {
        manager->pendingValid = false;
        return VSS_WARNING_MANAGER_OK;
    }
    return VSS_WARNING_MANAGER_TX_PENDING;
}
