/**
 * @file VehicleStateManager.h
 * @brief Domain 현재 상태 저장/조회 및 Interface Group별 Freshness 관리
 *
 * v0.2 핵심 변경:
 * - ECU 단위 Freshness -> Interface Group 단위 Freshness
 * - BCM Door/Climate/Light 개별 감시
 * - CIS Occupant/Cabin/Rear 개별 감시
 * - ESP32 Connection/Proximity 개별 감시
 */

#ifndef VEHICLE_STATE_MANAGER_H
#define VEHICLE_STATE_MANAGER_H

#include <stdbool.h>
#include <stdint.h>

#include "Vehicle_Types.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum
{
    VSM_STATUS_OK = 0,
    VSM_STATUS_INVALID_ARGUMENT,
    VSM_STATUS_NOT_INITIALIZED
} VsmStatus_t;

typedef enum
{
    VSM_GROUP_BCM_DOOR = 0,
    VSM_GROUP_BCM_CLIMATE,
    VSM_GROUP_BCM_INTERIOR_LIGHT,
    VSM_GROUP_CIS_OCCUPANT,
    VSM_GROUP_CIS_CABIN,
    VSM_GROUP_CIS_REAR,
    VSM_GROUP_WINDOW_STATE,
    VSM_GROUP_VSS_STATE,
    VSM_GROUP_ESP32_CONNECTION,
    VSM_GROUP_ESP32_PROXIMITY,
    VSM_GROUP_COUNT
} VsmInterfaceGroup_t;

typedef enum
{
    VSM_FRESHNESS_NO_DATA = 0,
    VSM_FRESHNESS_FRESH,
    VSM_FRESHNESS_STALE
} VsmFreshnessState_t;

typedef struct
{
    uint32_t timeoutMs[VSM_GROUP_COUNT];
} VehicleStateManager_Config_t;

typedef struct
{
    VsmFreshnessState_t state;
    uint32_t lastUpdateMs;
    uint32_t elapsedSinceUpdateMs;
} VsmGroupHealth_t;

typedef struct
{
    VehicleState_t vehicle;
    DigitalKeyInput_t digitalKeyInput;
    VsmGroupHealth_t groupHealth[VSM_GROUP_COUNT];
} VehicleStateSnapshot_t;

VsmStatus_t VehicleStateManager_Init(
    const VehicleStateManager_Config_t *config,
    uint32_t nowMs);

bool VehicleStateManager_IsInitialized(void);

VsmStatus_t VehicleStateManager_ProcessFreshness(uint32_t nowMs);

/* BCM */
VsmStatus_t VehicleStateManager_UpdateDoor(
    const DoorState_t *state,
    uint32_t nowMs);

VsmStatus_t VehicleStateManager_UpdateClimate(
    const ClimateState_t *state,
    uint32_t nowMs);

VsmStatus_t VehicleStateManager_UpdateInteriorLight(
    const InteriorLightState_t *state,
    uint32_t nowMs);

/* CIS */
VsmStatus_t VehicleStateManager_UpdateCisOccupant(
    const CisOccupantState_t *state,
    uint32_t nowMs);

VsmStatus_t VehicleStateManager_UpdateCisCabin(
    const CisCabinEnvironmentState_t *state,
    uint32_t nowMs);

VsmStatus_t VehicleStateManager_UpdateCisRear(
    const CisRearState_t *state,
    uint32_t nowMs);

/* WINDOW / VSS */
VsmStatus_t VehicleStateManager_UpdateWindow(
    const WindowState_t *state,
    uint32_t nowMs);

VsmStatus_t VehicleStateManager_UpdateVss(
    const VssServiceState_t *state,
    uint32_t nowMs);

/* Domain-owned state */
VsmStatus_t VehicleStateManager_UpdateDigitalKeyState(
    const DigitalKeyState_t *state);

/* ESP32 inputs */
VsmStatus_t VehicleStateManager_UpdateDigitalKeyConnection(
    const DigitalKeyConnectionState_t *state,
    uint32_t nowMs);

VsmStatus_t VehicleStateManager_UpdateProximity(
    const ProximityInput_t *state,
    uint32_t nowMs);

/* Getters */
VsmStatus_t VehicleStateManager_GetSnapshot(
    VehicleStateSnapshot_t *outSnapshot);

VsmStatus_t VehicleStateManager_GetDoor(
    DoorState_t *outState);

VsmStatus_t VehicleStateManager_GetClimate(
    ClimateState_t *outState);

VsmStatus_t VehicleStateManager_GetCisEnvironment(
    CisEnvironmentState_t *outState);

VsmStatus_t VehicleStateManager_GetWindow(
    WindowState_t *outState);

VsmStatus_t VehicleStateManager_GetVss(
    VssServiceState_t *outState);

VsmStatus_t VehicleStateManager_GetDigitalKeyInput(
    DigitalKeyInput_t *outInput);

VsmStatus_t VehicleStateManager_GetGroupHealth(
    VsmInterfaceGroup_t group,
    VsmGroupHealth_t *outHealth);

#ifdef __cplusplus
}
#endif

#endif /* VEHICLE_STATE_MANAGER_H */
