/**
 * @file ClimateManager.h
 * @brief Central climate policy: manual fan and automatic temperature control
 *
 * Core rules from SysRS:
 * - Central selects one climate mode and creates final Fan/Thermal targets.
 * - Manual fan terminates automatic temperature control; thermal output is OFF.
 * - Auto climate uses valid cabin temperature + target temperature.
 * - Target-temperature change updates an active AUTO mode but does not start an OFF mode.
 * - Loss of required AUTO temperature input turns the automatic target OFF;
 *   unrelated manual fan control is not blocked.
 * - Thermal output is not started until required Fan operation is confirmed.
 */

#ifndef CLIMATE_MANAGER_H
#define CLIMATE_MANAGER_H

#include <stdbool.h>
#include <stdint.h>

#include "CommandManager.h"
#include "Domain_Interface.h"
#include "PermissionManager.h"
#include "ResultManager.h"
#include "SettingsManager.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum
{
    CLIMATE_MANAGER_STATUS_OK = 0,
    CLIMATE_MANAGER_STATUS_INVALID_ARGUMENT,
    CLIMATE_MANAGER_STATUS_NOT_INITIALIZED,
    CLIMATE_MANAGER_STATUS_DEPENDENCY_NOT_READY,
    CLIMATE_MANAGER_STATUS_CONFIGURATION_REQUIRED,
    CLIMATE_MANAGER_STATUS_BUSY,
    CLIMATE_MANAGER_STATUS_COMMAND_ERROR
} ClimateManager_Status_t;

typedef enum
{
    CLIMATE_PHASE_IDLE = 0,
    CLIMATE_PHASE_MANUAL_FAN_PENDING,
    CLIMATE_PHASE_AUTO_FAN_PENDING,
    CLIMATE_PHASE_AUTO_THERMAL_PENDING,
    CLIMATE_PHASE_AUTO_ACTIVE
} ClimateManager_Phase_t;

typedef enum
{
    CLIMATE_DECISION_NONE = 0,
    CLIMATE_DECISION_SETTING_CONFIRMED,
    CLIMATE_DECISION_TARGET_ONLY_NO_START,
    CLIMATE_DECISION_MANUAL_COMMAND_DISPATCHED,
    CLIMATE_DECISION_AUTO_FAN_COMMAND_DISPATCHED,
    CLIMATE_DECISION_AUTO_THERMAL_COMMAND_DISPATCHED,
    CLIMATE_DECISION_AUTO_IDLE_COMMAND_DISPATCHED,
    CLIMATE_DECISION_REQUIRED_TEMPERATURE_UNTRUSTED,
    CLIMATE_DECISION_FAN_NOT_CONFIRMED,
    CLIMATE_DECISION_HEAT_REMOVAL_NOT_NORMAL,
    CLIMATE_DECISION_PERMISSION_DENIED,
    CLIMATE_DECISION_COMMAND_PENDING,
    CLIMATE_DECISION_COMMAND_DISPATCH_FAILED,
    CLIMATE_DECISION_RESULT_TIMEOUT,
    CLIMATE_DECISION_CONFIGURATION_REQUIRED
} ClimateManager_Decision_t;

/**
 * Temperature values in Vehicle_Types are int16_t but wire/internal scale is TBD.
 * This config makes the scale explicit instead of assuming 23 == 23.0C.
 *
 * Example:
 *   1  unit/degC  -> integer Celsius (cannot represent 0.5C precisely)
 *   10 units/degC -> deci-Celsius
 *   100 units/degC -> centi-Celsius
 *
 * AUTO climate requires temperatureUnitsPerDegC >= 2.
 */
typedef struct
{
    bool supported;
    FunctionAvailability_t initialAvailability;
    PermissionDegradedPolicy_t degradedPolicy;

    uint16_t temperatureUnitsPerDegC;

    /**
     * Configured safe maximum thermal electrical output.
     * SysRS 100% is a candidate only; integration must explicitly provide it.
     */
    bool maxThermalOutputConfigured;
    uint8_t maxThermalOutputPercent;
} ClimateManager_Config_t;

typedef struct
{
    ClimateManager_Phase_t phase;
    FunctionAvailability_t availability;

    SettingsClimateMode_t selectedMode;

    FanLevel_t desiredFan;
    ThermalDirection_t desiredThermalDirection;
    uint8_t desiredThermalPercent;

    bool hasPendingCommand;
    DomainCommandId_t pendingCommandId;
    uint32_t commandIssuedAtMs;

    bool pendingManualRequest;
    DeviceContextId_t pendingManualDevice;
    RequestContext_t pendingManualRequestContext;
    uint32_t pendingManualSessionGeneration;

    FanLevel_t autoFanMemory;
} ClimateManager_Snapshot_t;

typedef struct
{
    ClimateManager_Decision_t decision;

    SettingsApplyResult_t settingApply;
    PermissionResult_t permission;

    bool commandCreated;
    bool commandDispatched;
    DomainCommandId_t commandId;
    DomainIf_Status_t txStatus;
} ClimateManager_Output_t;


/* Init */
void ClimateManager_LoadCurrentProjectDefaults(
    ClimateManager_Config_t *outConfig);

ClimateManager_Status_t ClimateManager_Init(
    const ClimateManager_Config_t *config);

bool ClimateManager_IsInitialized(void);

ClimateManager_Status_t ClimateManager_SetAvailability(
    FunctionAvailability_t availability);


/* MOBILE requests */
ClimateManager_Status_t ClimateManager_HandleMobileRequest(
    const DomainIf_MobileRequest_t *request,
    uint32_t sessionGeneration,
    uint32_t nowMs,
    ClimateManager_Output_t *outResult);


/* State-driven processing */
ClimateManager_Status_t ClimateManager_Process(
    uint32_t nowMs,
    ClimateManager_Output_t *outResult);

/**
 * Call after VehicleStateManager receives a new BCM ClimateState.
 * Confirms Fan/thermal application and advances two-phase AUTO flow.
 */
ClimateManager_Status_t ClimateManager_OnBcmClimateState(
    const ClimateState_t *state,
    uint32_t nowMs,
    ClimateManager_Output_t *outResult);

ClimateManager_Status_t ClimateManager_GetSnapshot(
    ClimateManager_Snapshot_t *outSnapshot);

#ifdef __cplusplus
}
#endif

#endif /* CLIMATE_MANAGER_H */
