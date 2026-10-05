/**
 * @file InteriorLightManager.h
 * @brief Central interior-light arbitration and BCM final-target generation
 *
 * Priority:
 *   FAULT > WARNING > GOODBYE > NORMAL
 *
 * Key rules:
 * - User OFF applies only to NORMAL.
 * - Safety/situation indications ignore the user's NORMAL OFF setting.
 * - The winner is re-selected from the current active set every evaluation.
 * - GOODBYE is temporary but its duration is TBD in SysRS.
 * - If GOODBYE is preempted by WARNING/FAULT, it is discarded and must not
 *   resume automatically after the higher-priority indication clears.
 * - NORMAL user brightness overrides illuminance-based automatic brightness.
 */

#ifndef INTERIOR_LIGHT_MANAGER_H
#define INTERIOR_LIGHT_MANAGER_H

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
    INTERIOR_LIGHT_MANAGER_STATUS_OK = 0,
    INTERIOR_LIGHT_MANAGER_STATUS_INVALID_ARGUMENT,
    INTERIOR_LIGHT_MANAGER_STATUS_NOT_INITIALIZED,
    INTERIOR_LIGHT_MANAGER_STATUS_DEPENDENCY_NOT_READY,
    INTERIOR_LIGHT_MANAGER_STATUS_CONFIGURATION_REQUIRED,
    INTERIOR_LIGHT_MANAGER_STATUS_COMMAND_ERROR
} InteriorLightManager_Status_t;

typedef enum
{
    INTERIOR_LIGHT_DECISION_NONE = 0,
    INTERIOR_LIGHT_DECISION_SETTING_CONFIRMED,
    INTERIOR_LIGHT_DECISION_TARGET_UNCHANGED,
    INTERIOR_LIGHT_DECISION_NORMAL_DISABLED,
    INTERIOR_LIGHT_DECISION_ILLUMINANCE_UNTRUSTED,
    INTERIOR_LIGHT_DECISION_CONFIGURATION_REQUIRED,
    INTERIOR_LIGHT_DECISION_PERMISSION_DENIED,
    INTERIOR_LIGHT_DECISION_COMMAND_DISPATCHED,
    INTERIOR_LIGHT_DECISION_COMMAND_DISPATCH_FAILED,
    INTERIOR_LIGHT_DECISION_COMMAND_PENDING,
    INTERIOR_LIGHT_DECISION_RESULT_TIMEOUT,
    INTERIOR_LIGHT_DECISION_GOODBYE_SUPPRESSED
} InteriorLightManager_Decision_t;

typedef struct
{
    bool supported;
    FunctionAvailability_t initialAvailability;
    PermissionDegradedPolicy_t degradedPolicy;

    /**
     * CIS illuminance raw scale. Current SysRS says the actual unit/scaling is TBD.
     * Automatic NORMAL brightness requires this to be configured.
     *
     * Example: 1 => raw value is lux.
     */
    uint16_t illuminanceUnitsPerLux;
} InteriorLightManager_Config_t;

typedef struct
{
    InteriorLightType_t type;
    uint8_t levelPercent;
    RgbColor_t color;
} InteriorLightTarget_t;

typedef struct
{
    FunctionAvailability_t availability;

    bool warningActive;
    bool faultActive;
    bool goodbyeActive;

    bool hasAutoBrightnessMemory;
    uint8_t autoBrightnessPercent;

    InteriorLightTarget_t desiredTarget;

    bool hasIssuedTarget;
    InteriorLightTarget_t lastIssuedTarget;

    bool hasPendingCommand;
    DomainCommandId_t pendingCommandId;
    uint32_t commandIssuedAtMs;

    bool hasAppliedTarget;
    InteriorLightTarget_t lastAppliedTarget;
} InteriorLightManager_Snapshot_t;

typedef struct
{
    InteriorLightManager_Decision_t decision;
    SettingsApplyResult_t settingApply;
    PermissionResult_t permission;

    InteriorLightTarget_t selectedTarget;

    bool commandCreated;
    bool commandDispatched;
    DomainCommandId_t commandId;
    DomainIf_Status_t txStatus;
} InteriorLightManager_Output_t;


/* Init / availability */
void InteriorLightManager_LoadCurrentProjectDefaults(
    InteriorLightManager_Config_t *outConfig);

InteriorLightManager_Status_t InteriorLightManager_Init(
    const InteriorLightManager_Config_t *config);

bool InteriorLightManager_IsInitialized(void);

InteriorLightManager_Status_t InteriorLightManager_SetAvailability(
    FunctionAvailability_t availability);


/* Upstream semantic demands */
InteriorLightManager_Status_t InteriorLightManager_SetWarningActive(
    bool active,
    uint32_t nowMs,
    InteriorLightManager_Output_t *outResult);

InteriorLightManager_Status_t InteriorLightManager_SetFaultActive(
    bool active,
    uint32_t nowMs,
    InteriorLightManager_Output_t *outResult);

/**
 * GOODBYE duration is TBD. VehicleEventManager/integration owns the semantic
 * lifetime and calls active=false to end it.
 *
 * If WARNING/FAULT is already active when active=true arrives, the temporary
 * GOODBYE is discarded rather than queued for later replay.
 */
InteriorLightManager_Status_t InteriorLightManager_SetGoodbyeActive(
    bool active,
    uint32_t nowMs,
    InteriorLightManager_Output_t *outResult);


/* MOBILE setting requests */
InteriorLightManager_Status_t InteriorLightManager_HandleMobileRequest(
    const DomainIf_MobileRequest_t *request,
    uint32_t sessionGeneration,
    uint32_t nowMs,
    InteriorLightManager_Output_t *outResult);


/* Periodic/state-driven evaluation */
InteriorLightManager_Status_t InteriorLightManager_Process(
    uint32_t nowMs,
    InteriorLightManager_Output_t *outResult);

/**
 * Call after VehicleStateManager receives BCM InteriorLightState.
 * `commandApplied` confirms logical application only; it must not be reported
 * as physical light confirmation when `physicalFeedbackSupported == false`.
 */
InteriorLightManager_Status_t InteriorLightManager_OnBcmState(
    const InteriorLightState_t *state,
    uint32_t nowMs,
    InteriorLightManager_Output_t *outResult);

InteriorLightManager_Status_t InteriorLightManager_GetSnapshot(
    InteriorLightManager_Snapshot_t *outSnapshot);

#ifdef __cplusplus
}
#endif

#endif /* INTERIOR_LIGHT_MANAGER_H */
