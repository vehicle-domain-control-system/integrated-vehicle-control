/**
 * @file VehicleStateManager.c
 * @brief VehicleStateManager v0.2 implementation
 */

#include "VehicleStateManager.h"

#include <string.h>

typedef struct
{
    bool initialized;
    VehicleStateManager_Config_t config;
    VehicleStateSnapshot_t snapshot;
} VehicleStateManager_Context_t;

static VehicleStateManager_Context_t g_vsm;

static bool Vsm_IsValidGroup(VsmInterfaceGroup_t group)
{
    return ((uint32_t)group < (uint32_t)VSM_GROUP_COUNT);
}

static uint32_t Vsm_ElapsedMs(uint32_t nowMs, uint32_t thenMs)
{
    return (uint32_t)(nowMs - thenMs);
}

static void Vsm_InitSignalMetaNoData(SignalMeta_t *meta)
{
    if (meta == NULL)
    {
        return;
    }

    meta->quality = DATA_QUALITY_NO_DATA;
    meta->ageMs = 0U;
    meta->updateSequence = 0U;
}

static void Vsm_InitValueQualityNoData(ValueQuality_t *quality)
{
    if (quality == NULL)
    {
        return;
    }

    quality->validity = VALUE_VALIDITY_INVALID;
    quality->reason = QUALITY_REASON_NO_DATA;
    Vsm_InitSignalMetaNoData(&quality->meta);
}

static void Vsm_SetValueQualityStale(ValueQuality_t *quality)
{
    if (quality == NULL)
    {
        return;
    }

    quality->validity = VALUE_VALIDITY_INVALID;
    quality->reason = QUALITY_REASON_STALE;
    quality->meta.quality = DATA_QUALITY_STALE;
}

static void Vsm_MarkGroupUpdated(
    VsmInterfaceGroup_t group,
    uint32_t nowMs)
{
    VsmGroupHealth_t *health = &g_vsm.snapshot.groupHealth[group];

    health->state = VSM_FRESHNESS_FRESH;
    health->lastUpdateMs = nowMs;
    health->elapsedSinceUpdateMs = 0U;
}

static void Vsm_InitializeState(void)
{
    VehicleState_t *vehicle = &g_vsm.snapshot.vehicle;
    DigitalKeyInput_t *key = &g_vsm.snapshot.digitalKeyInput;

    (void)memset(vehicle, 0, sizeof(*vehicle));
    (void)memset(key, 0, sizeof(*key));

    vehicle->door.lockState = DOOR_LOCK_STATE_UNKNOWN;
    vehicle->door.openState = DOOR_OPEN_STATE_UNKNOWN;
    vehicle->door.compositeState = DOOR_COMPOSITE_STATE_UNTRUSTED;
    Vsm_InitSignalMetaNoData(&vehicle->door.meta);

    vehicle->climate.commandedFanLevel = FAN_LEVEL_UNKNOWN;
    vehicle->climate.measuredFanLevel = FAN_LEVEL_UNKNOWN;
    vehicle->climate.thermalDirection = THERMAL_DIRECTION_IDLE;
    vehicle->climate.heatRemovalState = HEAT_REMOVAL_STATE_UNMEASURABLE;
    Vsm_InitSignalMetaNoData(&vehicle->climate.meta);

    vehicle->interiorLight.type = INTERIOR_LIGHT_TYPE_NORMAL;
    Vsm_InitSignalMetaNoData(&vehicle->interiorLight.meta);

    Vsm_InitValueQualityNoData(&vehicle->environment.occupant.quality);
    Vsm_InitValueQualityNoData(&vehicle->environment.cabin.temperatureQuality);
    Vsm_InitValueQualityNoData(&vehicle->environment.cabin.humidityQuality);
    Vsm_InitValueQualityNoData(&vehicle->environment.cabin.illuminanceQuality);

    vehicle->environment.rear.measurementState =
        REAR_MEASUREMENT_UNAVAILABLE;
    Vsm_InitValueQualityNoData(&vehicle->environment.rear.distanceQuality);

    vehicle->window.motionState = WINDOW_MOTION_UNKNOWN;
    vehicle->window.ecuState = ECU_STATE_INIT;
    vehicle->window.antiPinchState = ANTIPINCH_STATE_CLEAR;
    Vsm_InitSignalMetaNoData(&vehicle->window.meta);

    vehicle->vss.state = VSS_STATE_STARTUP;
    vehicle->vss.availability = VSS_AVAILABILITY_UNAVAILABLE;
    vehicle->vss.acceptingEvents = false;
    vehicle->vss.faultActive = false;
    Vsm_InitSignalMetaNoData(&vehicle->vss.meta);

    vehicle->digitalKey.setting = DIGITAL_KEY_SETTING_OFF;
    vehicle->digitalKey.availability = FUNCTION_AVAILABILITY_UNAVAILABLE;
    vehicle->digitalKey.lastUnlockOrigin = UNLOCK_ORIGIN_USER_REQUEST;

    key->connection.registration = DEVICE_REGISTRATION_UNKNOWN;
    key->connection.btConnection = BT_CONNECTION_UNKNOWN;
    key->connection.appActive = APP_ACTIVE_UNKNOWN;
    key->connection.sessionId = 0U;
    Vsm_InitSignalMetaNoData(&key->connection.meta);

    key->proximity.state = PROXIMITY_UNKNOWN;
    Vsm_InitSignalMetaNoData(&key->proximity.meta);
}

static void Vsm_ApplyGroupStalePolicy(VsmInterfaceGroup_t group)
{
    switch (group)
    {
        case VSM_GROUP_BCM_DOOR:
            g_vsm.snapshot.vehicle.door.meta.quality = DATA_QUALITY_STALE;
            break;

        case VSM_GROUP_BCM_CLIMATE:
            g_vsm.snapshot.vehicle.climate.meta.quality = DATA_QUALITY_STALE;
            break;

        case VSM_GROUP_BCM_INTERIOR_LIGHT:
            g_vsm.snapshot.vehicle.interiorLight.meta.quality =
                DATA_QUALITY_STALE;
            break;

        case VSM_GROUP_CIS_OCCUPANT:
            Vsm_SetValueQualityStale(
                &g_vsm.snapshot.vehicle.environment.occupant.quality);
            break;

        case VSM_GROUP_CIS_CABIN:
            Vsm_SetValueQualityStale(
                &g_vsm.snapshot.vehicle.environment.cabin.temperatureQuality);
            Vsm_SetValueQualityStale(
                &g_vsm.snapshot.vehicle.environment.cabin.humidityQuality);
            Vsm_SetValueQualityStale(
                &g_vsm.snapshot.vehicle.environment.cabin.illuminanceQuality);
            break;

        case VSM_GROUP_CIS_REAR:
            Vsm_SetValueQualityStale(
                &g_vsm.snapshot.vehicle.environment.rear.distanceQuality);
            g_vsm.snapshot.vehicle.environment.rear.measurementState =
                REAR_MEASUREMENT_UNAVAILABLE;
            break;

        case VSM_GROUP_WINDOW_STATE:
            g_vsm.snapshot.vehicle.window.meta.quality = DATA_QUALITY_STALE;
            break;

        case VSM_GROUP_VSS_STATE:
            g_vsm.snapshot.vehicle.vss.meta.quality = DATA_QUALITY_STALE;
            g_vsm.snapshot.vehicle.vss.availability =
                VSS_AVAILABILITY_UNAVAILABLE;
            g_vsm.snapshot.vehicle.vss.acceptingEvents = false;
            break;

        case VSM_GROUP_ESP32_CONNECTION:
            g_vsm.snapshot.digitalKeyInput.connection.registration =
                DEVICE_REGISTRATION_UNKNOWN;
            g_vsm.snapshot.digitalKeyInput.connection.btConnection =
                BT_CONNECTION_UNKNOWN;
            g_vsm.snapshot.digitalKeyInput.connection.appActive =
                APP_ACTIVE_UNKNOWN;
            g_vsm.snapshot.digitalKeyInput.connection.meta.quality =
                DATA_QUALITY_STALE;
            break;

        case VSM_GROUP_ESP32_PROXIMITY:
            /*
             * STALE/통신 상실을 FAR로 바꾸지 않는다.
             * 실제 FAR이 확인된 경우만 FAR이다.
             */
            g_vsm.snapshot.digitalKeyInput.proximity.state =
                PROXIMITY_UNKNOWN;
            g_vsm.snapshot.digitalKeyInput.proximity.meta.quality =
                DATA_QUALITY_STALE;
            break;

        case VSM_GROUP_COUNT:
        default:
            break;
    }
}

VsmStatus_t VehicleStateManager_Init(
    const VehicleStateManager_Config_t *config,
    uint32_t nowMs)
{
    uint32_t i;

    (void)memset(&g_vsm, 0, sizeof(g_vsm));

    if (config != NULL)
    {
        g_vsm.config = *config;
    }

    Vsm_InitializeState();

    for (i = 0U; i < (uint32_t)VSM_GROUP_COUNT; ++i)
    {
        g_vsm.snapshot.groupHealth[i].state = VSM_FRESHNESS_NO_DATA;
        g_vsm.snapshot.groupHealth[i].lastUpdateMs = nowMs;
        g_vsm.snapshot.groupHealth[i].elapsedSinceUpdateMs = 0U;
    }

    g_vsm.initialized = true;
    return VSM_STATUS_OK;
}

bool VehicleStateManager_IsInitialized(void)
{
    return g_vsm.initialized;
}

VsmStatus_t VehicleStateManager_ProcessFreshness(uint32_t nowMs)
{
    uint32_t i;

    if (!g_vsm.initialized)
    {
        return VSM_STATUS_NOT_INITIALIZED;
    }

    for (i = 0U; i < (uint32_t)VSM_GROUP_COUNT; ++i)
    {
        VsmGroupHealth_t *health = &g_vsm.snapshot.groupHealth[i];
        const uint32_t timeoutMs = g_vsm.config.timeoutMs[i];

        if (health->state == VSM_FRESHNESS_NO_DATA)
        {
            continue;
        }

        health->elapsedSinceUpdateMs =
            Vsm_ElapsedMs(nowMs, health->lastUpdateMs);

        if ((timeoutMs > 0U) &&
            (health->elapsedSinceUpdateMs >= timeoutMs) &&
            (health->state != VSM_FRESHNESS_STALE))
        {
            health->state = VSM_FRESHNESS_STALE;
            Vsm_ApplyGroupStalePolicy((VsmInterfaceGroup_t)i);
        }
    }

    return VSM_STATUS_OK;
}

VsmStatus_t VehicleStateManager_UpdateDoor(
    const DoorState_t *state,
    uint32_t nowMs)
{
    if (state == NULL) return VSM_STATUS_INVALID_ARGUMENT;
    if (!g_vsm.initialized) return VSM_STATUS_NOT_INITIALIZED;

    g_vsm.snapshot.vehicle.door = *state;
    Vsm_MarkGroupUpdated(VSM_GROUP_BCM_DOOR, nowMs);
    return VSM_STATUS_OK;
}

VsmStatus_t VehicleStateManager_UpdateClimate(
    const ClimateState_t *state,
    uint32_t nowMs)
{
    if (state == NULL) return VSM_STATUS_INVALID_ARGUMENT;
    if (!g_vsm.initialized) return VSM_STATUS_NOT_INITIALIZED;

    g_vsm.snapshot.vehicle.climate = *state;
    Vsm_MarkGroupUpdated(VSM_GROUP_BCM_CLIMATE, nowMs);
    return VSM_STATUS_OK;
}

VsmStatus_t VehicleStateManager_UpdateInteriorLight(
    const InteriorLightState_t *state,
    uint32_t nowMs)
{
    if (state == NULL) return VSM_STATUS_INVALID_ARGUMENT;
    if (!g_vsm.initialized) return VSM_STATUS_NOT_INITIALIZED;

    g_vsm.snapshot.vehicle.interiorLight = *state;
    Vsm_MarkGroupUpdated(VSM_GROUP_BCM_INTERIOR_LIGHT, nowMs);
    return VSM_STATUS_OK;
}

VsmStatus_t VehicleStateManager_UpdateCisOccupant(
    const CisOccupantState_t *state,
    uint32_t nowMs)
{
    if (state == NULL) return VSM_STATUS_INVALID_ARGUMENT;
    if (!g_vsm.initialized) return VSM_STATUS_NOT_INITIALIZED;

    g_vsm.snapshot.vehicle.environment.occupant = *state;
    Vsm_MarkGroupUpdated(VSM_GROUP_CIS_OCCUPANT, nowMs);
    return VSM_STATUS_OK;
}

VsmStatus_t VehicleStateManager_UpdateCisCabin(
    const CisCabinEnvironmentState_t *state,
    uint32_t nowMs)
{
    if (state == NULL) return VSM_STATUS_INVALID_ARGUMENT;
    if (!g_vsm.initialized) return VSM_STATUS_NOT_INITIALIZED;

    g_vsm.snapshot.vehicle.environment.cabin = *state;
    Vsm_MarkGroupUpdated(VSM_GROUP_CIS_CABIN, nowMs);
    return VSM_STATUS_OK;
}

VsmStatus_t VehicleStateManager_UpdateCisRear(
    const CisRearState_t *state,
    uint32_t nowMs)
{
    if (state == NULL) return VSM_STATUS_INVALID_ARGUMENT;
    if (!g_vsm.initialized) return VSM_STATUS_NOT_INITIALIZED;

    g_vsm.snapshot.vehicle.environment.rear = *state;
    Vsm_MarkGroupUpdated(VSM_GROUP_CIS_REAR, nowMs);
    return VSM_STATUS_OK;
}

VsmStatus_t VehicleStateManager_UpdateWindow(
    const WindowState_t *state,
    uint32_t nowMs)
{
    if (state == NULL) return VSM_STATUS_INVALID_ARGUMENT;
    if (!g_vsm.initialized) return VSM_STATUS_NOT_INITIALIZED;

    g_vsm.snapshot.vehicle.window = *state;
    Vsm_MarkGroupUpdated(VSM_GROUP_WINDOW_STATE, nowMs);
    return VSM_STATUS_OK;
}

VsmStatus_t VehicleStateManager_UpdateVss(
    const VssServiceState_t *state,
    uint32_t nowMs)
{
    if (state == NULL) return VSM_STATUS_INVALID_ARGUMENT;
    if (!g_vsm.initialized) return VSM_STATUS_NOT_INITIALIZED;

    g_vsm.snapshot.vehicle.vss = *state;
    Vsm_MarkGroupUpdated(VSM_GROUP_VSS_STATE, nowMs);
    return VSM_STATUS_OK;
}

VsmStatus_t VehicleStateManager_UpdateDigitalKeyState(
    const DigitalKeyState_t *state)
{
    if (state == NULL) return VSM_STATUS_INVALID_ARGUMENT;
    if (!g_vsm.initialized) return VSM_STATUS_NOT_INITIALIZED;

    g_vsm.snapshot.vehicle.digitalKey = *state;
    return VSM_STATUS_OK;
}

VsmStatus_t VehicleStateManager_UpdateDigitalKeyConnection(
    const DigitalKeyConnectionState_t *state,
    uint32_t nowMs)
{
    if (state == NULL) return VSM_STATUS_INVALID_ARGUMENT;
    if (!g_vsm.initialized) return VSM_STATUS_NOT_INITIALIZED;

    g_vsm.snapshot.digitalKeyInput.connection = *state;
    Vsm_MarkGroupUpdated(VSM_GROUP_ESP32_CONNECTION, nowMs);
    return VSM_STATUS_OK;
}

VsmStatus_t VehicleStateManager_UpdateProximity(
    const ProximityInput_t *state,
    uint32_t nowMs)
{
    if (state == NULL) return VSM_STATUS_INVALID_ARGUMENT;
    if (!g_vsm.initialized) return VSM_STATUS_NOT_INITIALIZED;

    g_vsm.snapshot.digitalKeyInput.proximity = *state;
    Vsm_MarkGroupUpdated(VSM_GROUP_ESP32_PROXIMITY, nowMs);
    return VSM_STATUS_OK;
}

VsmStatus_t VehicleStateManager_GetSnapshot(
    VehicleStateSnapshot_t *outSnapshot)
{
    if (outSnapshot == NULL) return VSM_STATUS_INVALID_ARGUMENT;
    if (!g_vsm.initialized) return VSM_STATUS_NOT_INITIALIZED;

    *outSnapshot = g_vsm.snapshot;
    return VSM_STATUS_OK;
}

VsmStatus_t VehicleStateManager_GetDoor(DoorState_t *outState)
{
    if (outState == NULL) return VSM_STATUS_INVALID_ARGUMENT;
    if (!g_vsm.initialized) return VSM_STATUS_NOT_INITIALIZED;

    *outState = g_vsm.snapshot.vehicle.door;
    return VSM_STATUS_OK;
}

VsmStatus_t VehicleStateManager_GetClimate(ClimateState_t *outState)
{
    if (outState == NULL) return VSM_STATUS_INVALID_ARGUMENT;
    if (!g_vsm.initialized) return VSM_STATUS_NOT_INITIALIZED;

    *outState = g_vsm.snapshot.vehicle.climate;
    return VSM_STATUS_OK;
}

VsmStatus_t VehicleStateManager_GetCisEnvironment(
    CisEnvironmentState_t *outState)
{
    if (outState == NULL) return VSM_STATUS_INVALID_ARGUMENT;
    if (!g_vsm.initialized) return VSM_STATUS_NOT_INITIALIZED;

    *outState = g_vsm.snapshot.vehicle.environment;
    return VSM_STATUS_OK;
}

VsmStatus_t VehicleStateManager_GetWindow(WindowState_t *outState)
{
    if (outState == NULL) return VSM_STATUS_INVALID_ARGUMENT;
    if (!g_vsm.initialized) return VSM_STATUS_NOT_INITIALIZED;

    *outState = g_vsm.snapshot.vehicle.window;
    return VSM_STATUS_OK;
}

VsmStatus_t VehicleStateManager_GetVss(VssServiceState_t *outState)
{
    if (outState == NULL) return VSM_STATUS_INVALID_ARGUMENT;
    if (!g_vsm.initialized) return VSM_STATUS_NOT_INITIALIZED;

    *outState = g_vsm.snapshot.vehicle.vss;
    return VSM_STATUS_OK;
}

VsmStatus_t VehicleStateManager_GetDigitalKeyInput(
    DigitalKeyInput_t *outInput)
{
    if (outInput == NULL) return VSM_STATUS_INVALID_ARGUMENT;
    if (!g_vsm.initialized) return VSM_STATUS_NOT_INITIALIZED;

    *outInput = g_vsm.snapshot.digitalKeyInput;
    return VSM_STATUS_OK;
}

VsmStatus_t VehicleStateManager_GetGroupHealth(
    VsmInterfaceGroup_t group,
    VsmGroupHealth_t *outHealth)
{
    if ((outHealth == NULL) || (!Vsm_IsValidGroup(group)))
    {
        return VSM_STATUS_INVALID_ARGUMENT;
    }

    if (!g_vsm.initialized)
    {
        return VSM_STATUS_NOT_INITIALIZED;
    }

    *outHealth = g_vsm.snapshot.groupHealth[group];
    return VSM_STATUS_OK;
}
