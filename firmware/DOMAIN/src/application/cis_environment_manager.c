#include "cis_environment_manager.h"

#include <limits.h>
#include <string.h>

static bool CisEnvironmentManager_IsDataQualityValid(DataQuality_t quality)
{
    switch (quality)
    {
        case DATA_QUALITY_OK:
        case DATA_QUALITY_STALE:
        case DATA_QUALITY_INVALID:
        case DATA_QUALITY_NO_DATA:
            return true;
        default:
            return false;
    }
}

static bool CisEnvironmentManager_IsValidityValid(ValueValidity_t validity)
{
    switch (validity)
    {
        case VALUE_VALIDITY_INVALID:
        case VALUE_VALIDITY_VALID:
            return true;
        default:
            return false;
    }
}

static bool CisEnvironmentManager_IsReasonValid(QualityReason_t reason)
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

static bool CisEnvironmentManager_IsValueQualityValid(
    const ValueQuality_t *quality)
{
    return CisEnvironmentManager_IsValidityValid(quality->validity) &&
           CisEnvironmentManager_IsReasonValid(quality->reason) &&
           CisEnvironmentManager_IsDataQualityValid(quality->meta.quality);
}

static bool CisEnvironmentManager_IsInputValid(
    const CisCabinEnvironmentState_t *state)
{
    return CisEnvironmentManager_IsValueQualityValid(
               &state->temperatureQuality) &&
           CisEnvironmentManager_IsValueQualityValid(
               &state->humidityQuality) &&
           CisEnvironmentManager_IsValueQualityValid(
               &state->illuminanceQuality) &&
           ((state->humidityQuality.validity != VALUE_VALIDITY_VALID) ||
            (state->cabinHumidity <= 10000U));
}

static uint32_t CisEnvironmentManager_SaturatingAddU32(
    uint32_t left,
    uint32_t right)
{
    if (right > (UINT32_MAX - left))
    {
        return UINT32_MAX;
    }
    return left + right;
}

static ValueQuality_t CisEnvironmentManager_NoDataQuality(void)
{
    ValueQuality_t quality;

    quality.validity = VALUE_VALIDITY_INVALID;
    quality.reason = QUALITY_REASON_NO_DATA;
    quality.meta.quality = DATA_QUALITY_NO_DATA;
    quality.meta.ageMs = 0U;
    quality.meta.updateSequence = 0U;
    return quality;
}

static void CisEnvironmentManager_ProjectQuality(
    const ValueQuality_t *original,
    bool hasObservation,
    uint32_t receivedAtMs,
    uint32_t nowMs,
    bool hasTtl,
    uint32_t ttlMs,
    ValueQuality_t *outEffective,
    CisEnvironmentManager_SensorTiming_t *outTiming)
{
    uint32_t localElapsedMs = 0U;
    uint32_t effectiveAgeMs = original->meta.ageMs;

    if (hasObservation)
    {
        localElapsedMs = nowMs - receivedAtMs;
        effectiveAgeMs = CisEnvironmentManager_SaturatingAddU32(
            original->meta.ageMs, localElapsedMs);
    }

    *outEffective = *original;
    outEffective->meta.ageMs = effectiveAgeMs;
    if (hasObservation && hasTtl &&
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

CisEnvironmentManager_Status_t CisEnvironmentManager_Init(
    CisEnvironmentManager_t *manager)
{
    ValueQuality_t noData;

    if (manager == NULL)
    {
        return CIS_ENVIRONMENT_MANAGER_INVALID_ARGUMENT;
    }

    (void)memset(manager, 0, sizeof(*manager));
    noData = CisEnvironmentManager_NoDataQuality();
    manager->temperature.quality = noData;
    manager->humidity.quality = noData;
    manager->illuminance.quality = noData;
    manager->initialized = true;
    return CIS_ENVIRONMENT_MANAGER_OK;
}

CisEnvironmentManager_Status_t CisEnvironmentManager_Invalidate(
    CisEnvironmentManager_t *manager)
{
    CisEnvironmentManager_t next;

    if (manager == NULL)
    {
        return CIS_ENVIRONMENT_MANAGER_INVALID_ARGUMENT;
    }
    if (!manager->initialized)
    {
        return CIS_ENVIRONMENT_MANAGER_NOT_INITIALIZED;
    }

    next = *manager;
    next.invalidated = true;
    *manager = next;
    return CIS_ENVIRONMENT_MANAGER_OK;
}

CisEnvironmentManager_Status_t CisEnvironmentManager_Store(
    CisEnvironmentManager_t *manager,
    const CisCabinEnvironmentState_t *state,
    uint32_t receivedAtMs)
{
    CisEnvironmentManager_t next;

    if ((manager == NULL) || (state == NULL))
    {
        return CIS_ENVIRONMENT_MANAGER_INVALID_ARGUMENT;
    }
    if (!manager->initialized)
    {
        return CIS_ENVIRONMENT_MANAGER_NOT_INITIALIZED;
    }
    if (!CisEnvironmentManager_IsInputValid(state))
    {
        return CIS_ENVIRONMENT_MANAGER_INVALID_ARGUMENT;
    }

    next = *manager;
    next.temperature.value = state->cabinTemperature;
    next.temperature.quality = state->temperatureQuality;
    next.temperature.receivedAtMs = receivedAtMs;
    next.temperature.hasObservation = true;

    next.humidity.value = state->cabinHumidity;
    next.humidity.quality = state->humidityQuality;
    next.humidity.receivedAtMs = receivedAtMs;
    next.humidity.hasObservation = true;

    next.illuminance.value = state->cabinIlluminance;
    next.illuminance.quality = state->illuminanceQuality;
    next.illuminance.receivedAtMs = receivedAtMs;
    next.illuminance.hasObservation = true;
    next.invalidated = false;

    *manager = next;
    return CIS_ENVIRONMENT_MANAGER_OK;
}

CisEnvironmentManager_Status_t CisEnvironmentManager_GetSnapshot(
    const CisEnvironmentManager_t *manager,
    uint32_t nowMs,
    CisEnvironmentManager_Snapshot_t *outSnapshot)
{
    CisEnvironmentManager_Snapshot_t snapshot;

    if ((manager == NULL) || (outSnapshot == NULL))
    {
        return CIS_ENVIRONMENT_MANAGER_INVALID_ARGUMENT;
    }
    if (!manager->initialized)
    {
        return CIS_ENVIRONMENT_MANAGER_NOT_INITIALIZED;
    }

    (void)memset(&snapshot, 0, sizeof(snapshot));
    snapshot.original.cabinTemperature = manager->temperature.value;
    snapshot.original.temperatureQuality = manager->temperature.quality;
    snapshot.original.cabinHumidity = manager->humidity.value;
    snapshot.original.humidityQuality = manager->humidity.quality;
    snapshot.original.cabinIlluminance = manager->illuminance.value;
    snapshot.original.illuminanceQuality = manager->illuminance.quality;
    snapshot.effective = snapshot.original;

    CisEnvironmentManager_ProjectQuality(
        &manager->temperature.quality,
        manager->temperature.hasObservation,
        manager->temperature.receivedAtMs,
        nowMs,
        true,
        CIS_ENVIRONMENT_TEMPERATURE_TTL_MS,
        &snapshot.effective.temperatureQuality,
        &snapshot.temperatureTiming);
    CisEnvironmentManager_ProjectQuality(
        &manager->humidity.quality,
        manager->humidity.hasObservation,
        manager->humidity.receivedAtMs,
        nowMs,
        true,
        CIS_ENVIRONMENT_HUMIDITY_TTL_MS,
        &snapshot.effective.humidityQuality,
        &snapshot.humidityTiming);
    /* Illumination TTL is intentionally open; only AGE is accumulated. */
    CisEnvironmentManager_ProjectQuality(
        &manager->illuminance.quality,
        manager->illuminance.hasObservation,
        manager->illuminance.receivedAtMs,
        nowMs,
        false,
        0U,
        &snapshot.effective.illuminanceQuality,
        &snapshot.illuminanceTiming);

    snapshot.invalidated = manager->invalidated;
    if (manager->invalidated)
    {
        snapshot.effective.cabinTemperature = 0;
        snapshot.effective.temperatureQuality =
            CisEnvironmentManager_NoDataQuality();
        snapshot.effective.cabinHumidity = 0U;
        snapshot.effective.humidityQuality =
            CisEnvironmentManager_NoDataQuality();
        snapshot.effective.cabinIlluminance = 0U;
        snapshot.effective.illuminanceQuality =
            CisEnvironmentManager_NoDataQuality();
    }

    *outSnapshot = snapshot;
    return CIS_ENVIRONMENT_MANAGER_OK;
}
