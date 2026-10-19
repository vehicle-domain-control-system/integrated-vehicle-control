#include "cis_occupant_manager.h"

#include <limits.h>
#include <string.h>

#define CIS_OCCUPANT_SEQUENCE_HALF_RANGE UINT32_C(0x80000000)

static bool CisOccupantManager_IsDataQualityValid(DataQuality_t quality)
{
    return (quality == DATA_QUALITY_OK) ||
           (quality == DATA_QUALITY_STALE) ||
           (quality == DATA_QUALITY_INVALID) ||
           (quality == DATA_QUALITY_NO_DATA);
}

static bool CisOccupantManager_IsValidityValid(ValueValidity_t validity)
{
    return (validity == VALUE_VALIDITY_INVALID) ||
           (validity == VALUE_VALIDITY_VALID);
}

static bool CisOccupantManager_IsReasonValid(QualityReason_t reason)
{
    switch (reason)
    {
        case QUALITY_REASON_NONE:
        case QUALITY_REASON_NOT_READY:
        case QUALITY_REASON_OUT_OF_RANGE:
        case QUALITY_REASON_SENSOR_FAULT:
        case QUALITY_REASON_VISION_FAULT:
        case QUALITY_REASON_STALE:
        case QUALITY_REASON_NO_DATA:
        case QUALITY_REASON_COMMUNICATION_FAULT:
        case QUALITY_REASON_RECOVERING:
        case QUALITY_REASON_DATA_INVALID:
            return true;
        default:
            return false;
    }
}

static bool CisOccupantManager_IsFunctionStatusValid(
    CisFunctionStatus_t status)
{
    return (status == CIS_FUNCTION_STATUS_NOT_READY) ||
           (status == CIS_FUNCTION_STATUS_READY) ||
           (status == CIS_FUNCTION_STATUS_ACTIVE) ||
           (status == CIS_FUNCTION_STATUS_FAULT) ||
           (status == CIS_FUNCTION_STATUS_RECOVERING);
}

static bool CisOccupantManager_IsValueQualityValid(
    const ValueQuality_t *quality)
{
    return CisOccupantManager_IsValidityValid(quality->validity) &&
           CisOccupantManager_IsReasonValid(quality->reason) &&
           CisOccupantManager_IsDataQualityValid(quality->meta.quality);
}

static bool CisOccupantManager_IsInputValid(
    const CisOccupantState_t *state)
{
    return CisOccupantManager_IsValueQualityValid(&state->presenceQuality) &&
           CisOccupantManager_IsValueQualityValid(&state->countQuality) &&
           CisOccupantManager_IsFunctionStatusValid(
               state->visionFunctionStatus) &&
           ((state->countQuality.validity != VALUE_VALIDITY_VALID) ||
            (state->occupantCount <= 5U)) &&
           (state->presenceQuality.meta.updateSequence ==
            state->countQuality.meta.updateSequence);
}

static uint32_t CisOccupantManager_SaturatingAddU32(
    uint32_t left,
    uint32_t right)
{
    if (right > (UINT32_MAX - left))
    {
        return UINT32_MAX;
    }
    return left + right;
}

static bool CisOccupantManager_IsQualityUsable(
    const ValueQuality_t *quality)
{
    return (quality->validity == VALUE_VALIDITY_VALID) &&
           (quality->reason == QUALITY_REASON_NONE) &&
           (quality->meta.quality == DATA_QUALITY_OK);
}

static bool CisOccupantManager_IsQualityMeaningEqual(
    const ValueQuality_t *left,
    const ValueQuality_t *right)
{
    return (left->validity == right->validity) &&
           (left->reason == right->reason) &&
           (left->meta.quality == right->meta.quality) &&
           (left->meta.updateSequence == right->meta.updateSequence);
}

static bool CisOccupantManager_IsQualityEqual(
    const ValueQuality_t *left,
    const ValueQuality_t *right)
{
    return CisOccupantManager_IsQualityMeaningEqual(left, right) &&
           (left->meta.ageMs == right->meta.ageMs);
}

static ValueQuality_t CisOccupantManager_NoDataQuality(void)
{
    ValueQuality_t quality;

    quality.validity = VALUE_VALIDITY_INVALID;
    quality.reason = QUALITY_REASON_NO_DATA;
    quality.meta.quality = DATA_QUALITY_NO_DATA;
    quality.meta.ageMs = 0U;
    quality.meta.updateSequence = 0U;
    return quality;
}

static CisOccupantManager_Status_t CisOccupantManager_CheckSequence(
    const CisOccupantManager_t *manager,
    const CisOccupantState_t *state)
{
    uint32_t delta;

    if (!manager->hasAdmittedSourceGeneration ||
        (state->sourceGeneration != manager->admittedSourceGeneration))
    {
        return CIS_OCCUPANT_MANAGER_NOT_ADMITTED;
    }

    if (!manager->hasObservation || manager->invalidated)
    {
        return CIS_OCCUPANT_MANAGER_OK;
    }

    if (manager->state.sourceGeneration != state->sourceGeneration)
    {
        return CIS_OCCUPANT_MANAGER_NOT_ADMITTED;
    }

    delta = state->presenceQuality.meta.updateSequence -
            manager->state.presenceQuality.meta.updateSequence;
    if (delta == 0U)
    {
        return CIS_OCCUPANT_MANAGER_DUPLICATE;
    }
    if (delta >= CIS_OCCUPANT_SEQUENCE_HALF_RANGE)
    {
        return CIS_OCCUPANT_MANAGER_OUT_OF_ORDER;
    }
    return CIS_OCCUPANT_MANAGER_OK;
}

static bool CisOccupantManager_IsObservationIdentityEqual(
    const CisOccupantState_t *left,
    const CisOccupantState_t *right)
{
    return (left->occupantPresent == right->occupantPresent) &&
           (left->occupantCount == right->occupantCount) &&
           (left->sourceGeneration == right->sourceGeneration) &&
           (left->visionFunctionStatus == right->visionFunctionStatus) &&
           (left->presenceQuality.meta.updateSequence ==
            right->presenceQuality.meta.updateSequence) &&
           (left->countQuality.meta.updateSequence ==
            right->countQuality.meta.updateSequence);
}

static CisOccupantManager_Status_t CisOccupantManager_ApplyQualityWithdrawal(
    CisOccupantManager_t *manager,
    const CisOccupantState_t *state,
    uint32_t receivedAtMs)
{
    bool presenceMeaningChanged;
    bool countMeaningChanged;
    uint32_t presenceEffectiveAge;
    uint32_t countEffectiveAge;
    CisOccupantManager_t next;

    if (!CisOccupantManager_IsObservationIdentityEqual(
            &manager->state, state))
    {
        return CIS_OCCUPANT_MANAGER_CONFLICT;
    }

    if (CisOccupantManager_IsQualityEqual(
            &manager->state.presenceQuality, &state->presenceQuality) &&
        CisOccupantManager_IsQualityEqual(
            &manager->state.countQuality, &state->countQuality))
    {
        return CIS_OCCUPANT_MANAGER_DUPLICATE;
    }

    presenceMeaningChanged = !CisOccupantManager_IsQualityMeaningEqual(
        &manager->state.presenceQuality, &state->presenceQuality);
    countMeaningChanged = !CisOccupantManager_IsQualityMeaningEqual(
        &manager->state.countQuality, &state->countQuality);

    /* AGE-only retransmission does not create a new observation. */
    if (!presenceMeaningChanged && !countMeaningChanged)
    {
        return CIS_OCCUPANT_MANAGER_DUPLICATE;
    }

    /*
     * No total order is invented among STALE, INVALID and NO_DATA. Any
     * transition that remains non-usable is accepted as a withdrawal-state
     * update; a same-sequence transition back to usable is always rejected.
     */
    if ((presenceMeaningChanged &&
         !CisOccupantManager_IsQualityUsable(
             &manager->state.presenceQuality) &&
         CisOccupantManager_IsQualityUsable(&state->presenceQuality)) ||
        (countMeaningChanged &&
         !CisOccupantManager_IsQualityUsable(
             &manager->state.countQuality) &&
         CisOccupantManager_IsQualityUsable(&state->countQuality)))
    {
        return CIS_OCCUPANT_MANAGER_CONFLICT;
    }

    presenceEffectiveAge = CisOccupantManager_SaturatingAddU32(
        manager->state.presenceQuality.meta.ageMs,
        receivedAtMs - manager->presenceReceivedAtMs);
    countEffectiveAge = CisOccupantManager_SaturatingAddU32(
        manager->state.countQuality.meta.ageMs,
        receivedAtMs - manager->countReceivedAtMs);
    if ((presenceMeaningChanged &&
         (state->presenceQuality.meta.ageMs < presenceEffectiveAge)) ||
        (countMeaningChanged &&
         (state->countQuality.meta.ageMs < countEffectiveAge)))
    {
        return CIS_OCCUPANT_MANAGER_CONFLICT;
    }

    next = *manager;
    if (presenceMeaningChanged)
    {
        next.state.presenceQuality = state->presenceQuality;
        next.presenceReceivedAtMs = receivedAtMs;
    }
    if (countMeaningChanged)
    {
        next.state.countQuality = state->countQuality;
        next.countReceivedAtMs = receivedAtMs;
    }
    *manager = next;
    return CIS_OCCUPANT_MANAGER_OK;
}

static void CisOccupantManager_ProjectQuality(
    const ValueQuality_t *original,
    bool hasObservation,
    uint32_t receivedAtMs,
    uint32_t nowMs,
    uint32_t ttlMs,
    ValueQuality_t *outEffective,
    CisOccupantManager_ValueTiming_t *outTiming)
{
    uint32_t localElapsedMs = 0U;
    uint32_t effectiveAgeMs = original->meta.ageMs;

    if (hasObservation)
    {
        localElapsedMs = nowMs - receivedAtMs;
        effectiveAgeMs = CisOccupantManager_SaturatingAddU32(
            original->meta.ageMs, localElapsedMs);
    }

    *outEffective = *original;
    outEffective->meta.ageMs = effectiveAgeMs;
    if (hasObservation &&
        (outEffective->meta.quality == DATA_QUALITY_OK) &&
        (effectiveAgeMs >= ttlMs))
    {
        outEffective->meta.quality = DATA_QUALITY_STALE;
    }

    outTiming->hasObservation = hasObservation;
    outTiming->receivedAtMs = receivedAtMs;
    outTiming->localElapsedMs = localElapsedMs;
    outTiming->effectiveAgeMs = effectiveAgeMs;
}

CisOccupantManager_Status_t CisOccupantManager_Init(
    CisOccupantManager_t *manager,
    const CisOccupantManager_Policy_t *policy)
{
    ValueQuality_t noData;

    if ((manager == NULL) || (policy == NULL) ||
        (policy->presenceTtlMs == 0U) || (policy->countTtlMs == 0U))
    {
        return CIS_OCCUPANT_MANAGER_INVALID_ARGUMENT;
    }

    (void)memset(manager, 0, sizeof(*manager));
    noData = CisOccupantManager_NoDataQuality();
    manager->state.presenceQuality = noData;
    manager->state.countQuality = noData;
    manager->state.visionFunctionStatus = CIS_FUNCTION_STATUS_NOT_READY;
    manager->policy = *policy;
    manager->invalidated = true;
    manager->initialized = true;
    return CIS_OCCUPANT_MANAGER_OK;
}

CisOccupantManager_Status_t CisOccupantManager_Invalidate(
    CisOccupantManager_t *manager,
    SourceGeneration_t admittedSourceGeneration)
{
    CisOccupantManager_t next;

    if ((manager == NULL) || (admittedSourceGeneration == 0U))
    {
        return CIS_OCCUPANT_MANAGER_INVALID_ARGUMENT;
    }
    if (!manager->initialized)
    {
        return CIS_OCCUPANT_MANAGER_NOT_INITIALIZED;
    }

    next = *manager;
    next.admittedSourceGeneration = admittedSourceGeneration;
    next.hasAdmittedSourceGeneration = true;
    next.invalidated = true;
    next.sourceGenerationChanged = false;
    *manager = next;
    return CIS_OCCUPANT_MANAGER_OK;
}

CisOccupantManager_Status_t CisOccupantManager_Store(
    CisOccupantManager_t *manager,
    const CisOccupantState_t *state,
    uint32_t receivedAtMs)
{
    CisOccupantManager_Status_t sequenceStatus;
    CisOccupantManager_t next;

    if ((manager == NULL) || (state == NULL))
    {
        return CIS_OCCUPANT_MANAGER_INVALID_ARGUMENT;
    }
    if (!manager->initialized)
    {
        return CIS_OCCUPANT_MANAGER_NOT_INITIALIZED;
    }
    if (!CisOccupantManager_IsInputValid(state))
    {
        return CIS_OCCUPANT_MANAGER_INVALID_ARGUMENT;
    }

    sequenceStatus = CisOccupantManager_CheckSequence(manager, state);
    if (sequenceStatus == CIS_OCCUPANT_MANAGER_DUPLICATE)
    {
        return CisOccupantManager_ApplyQualityWithdrawal(
            manager, state, receivedAtMs);
    }
    if (sequenceStatus != CIS_OCCUPANT_MANAGER_OK)
    {
        return sequenceStatus;
    }

    next = *manager;
    next.sourceGenerationChanged = manager->hasObservation &&
        (manager->state.sourceGeneration != state->sourceGeneration);
    next.state = *state;
    next.presenceReceivedAtMs = receivedAtMs;
    next.countReceivedAtMs = receivedAtMs;
    next.hasObservation = true;
    next.invalidated = false;
    *manager = next;
    return CIS_OCCUPANT_MANAGER_OK;
}

CisOccupantManager_Status_t CisOccupantManager_GetSnapshot(
    const CisOccupantManager_t *manager,
    uint32_t nowMs,
    CisOccupantManager_Snapshot_t *outSnapshot)
{
    CisOccupantManager_Snapshot_t snapshot;

    if ((manager == NULL) || (outSnapshot == NULL))
    {
        return CIS_OCCUPANT_MANAGER_INVALID_ARGUMENT;
    }
    if (!manager->initialized)
    {
        return CIS_OCCUPANT_MANAGER_NOT_INITIALIZED;
    }

    (void)memset(&snapshot, 0, sizeof(snapshot));
    snapshot.original = manager->state;
    snapshot.effective = manager->state;
    snapshot.hasObservation = manager->hasObservation;
    snapshot.sourceGenerationChanged = manager->sourceGenerationChanged;
    CisOccupantManager_ProjectQuality(
        &manager->state.presenceQuality,
        manager->hasObservation,
        manager->presenceReceivedAtMs,
        nowMs,
        manager->policy.presenceTtlMs,
        &snapshot.effective.presenceQuality,
        &snapshot.presenceTiming);
    CisOccupantManager_ProjectQuality(
        &manager->state.countQuality,
        manager->hasObservation,
        manager->countReceivedAtMs,
        nowMs,
        manager->policy.countTtlMs,
        &snapshot.effective.countQuality,
        &snapshot.countTiming);

    snapshot.invalidated = manager->invalidated;
    if (manager->invalidated)
    {
        snapshot.effective.occupantPresent = false;
        snapshot.effective.occupantCount = 0U;
        snapshot.effective.presenceQuality = CisOccupantManager_NoDataQuality();
        snapshot.effective.countQuality = CisOccupantManager_NoDataQuality();
        snapshot.effective.visionFunctionStatus =
            CIS_FUNCTION_STATUS_NOT_READY;
    }

    *outSnapshot = snapshot;
    return CIS_OCCUPANT_MANAGER_OK;
}
