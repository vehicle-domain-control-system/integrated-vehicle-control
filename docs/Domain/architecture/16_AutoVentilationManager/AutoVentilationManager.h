/**
 * @file AutoVentilationManager.h
 * @brief Domain-owned stateful automatic ventilation job
 *
 * SysRS baseline:
 * - Explicit local/demo ventilation enable is required.
 * - After completion/cancel/failure, a new job requires valid OFF -> new ON.
 * - Reboot/recovery/temperature rise alone must not restart the job.
 * - Start requires:
 *     valid vehicle-use ended,
 *     no occupant,
 *     cabin temperature >= 30 C [provisional],
 *     other climate stopped,
 *     trusted WINDOW position/protection,
 *     trusted actual operation permission.
 * - Target: 80% closed (0=open, 100=closed).
 *   If already <=80%, do not move/close further.
 * - Normal end: valid temperature <=28 C OR 5 minutes.
 * - Never auto-close after ventilation.
 * - Cancel: manual window/STOP, disable, operation permission withdrawn,
 *   vehicle use resumed, occupant detected, other climate starts.
 * - Fail: required input untrusted, operation permission untrusted,
 *   window position/protection fault, ventilation move failed.
 * - On end/cancel/fail, send STOP only if the window is still moving because
 *   of this ventilation job.
 */

#ifndef AUTO_VENTILATION_MANAGER_H
#define AUTO_VENTILATION_MANAGER_H

#include <stdbool.h>
#include <stdint.h>

#include "CommandManager.h"
#include "Domain_Interface.h"
#include "PermissionManager.h"
#include "ResultManager.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum
{
    AUTO_VENT_STATUS_OK = 0,
    AUTO_VENT_STATUS_INVALID_ARGUMENT,
    AUTO_VENT_STATUS_NOT_INITIALIZED,
    AUTO_VENT_STATUS_DEPENDENCY_NOT_READY,
    AUTO_VENT_STATUS_CONFIGURATION_REQUIRED,
    AUTO_VENT_STATUS_COMMAND_ERROR
} AutoVentilationManager_Status_t;

typedef enum
{
    AUTO_VENT_STATE_WAIT_ENABLE_OFF = 0,
    AUTO_VENT_STATE_READY_FOR_ENABLE,
    AUTO_VENT_STATE_ARMED,
    AUTO_VENT_STATE_RUNNING,
    AUTO_VENT_STATE_STOPPING
} AutoVentilationState_t;

typedef enum
{
    AUTO_VENT_JOB_NONE = 0,
    AUTO_VENT_JOB_RUNNING,
    AUTO_VENT_JOB_COMPLETED,
    AUTO_VENT_JOB_CANCELLED,
    AUTO_VENT_JOB_FAILED
} AutoVentilationJobResult_t;

typedef enum
{
    AUTO_VENT_REASON_NONE = 0,

    AUTO_VENT_REASON_WAIT_ENABLE_OFF,
    AUTO_VENT_REASON_ARMED_WAIT_CONDITIONS,
    AUTO_VENT_REASON_STARTED_NO_MOVE_NEEDED,
    AUTO_VENT_REASON_MOVE_COMMAND_DISPATCHED,

    AUTO_VENT_REASON_NORMAL_TEMP_REACHED,
    AUTO_VENT_REASON_NORMAL_MAX_DURATION,

    AUTO_VENT_REASON_CANCEL_ENABLE_OFF,
    AUTO_VENT_REASON_CANCEL_MANUAL_WINDOW,
    AUTO_VENT_REASON_CANCEL_OPERATION_WITHDRAWN,
    AUTO_VENT_REASON_CANCEL_VEHICLE_USE_RESUMED,
    AUTO_VENT_REASON_CANCEL_OCCUPANT_DETECTED,
    AUTO_VENT_REASON_CANCEL_OTHER_CLIMATE,

    AUTO_VENT_REASON_FAIL_REQUIRED_INPUT_UNTRUSTED,
    AUTO_VENT_REASON_FAIL_OPERATION_PERMISSION_UNTRUSTED,
    AUTO_VENT_REASON_FAIL_WINDOW_STATE,
    AUTO_VENT_REASON_FAIL_WINDOW_PROTECTION,
    AUTO_VENT_REASON_FAIL_MOVE_COMMAND,
    AUTO_VENT_REASON_FAIL_MOVE_RESULT_TIMEOUT,

    AUTO_VENT_REASON_STOP_COMMAND_DISPATCHED,
    AUTO_VENT_REASON_STOP_CONFIRMED,
    AUTO_VENT_REASON_STOP_UNCONFIRMED,
    AUTO_VENT_REASON_COMMAND_DISPATCH_FAILED,
    AUTO_VENT_REASON_CONFIGURATION_REQUIRED
} AutoVentilationReason_t;

typedef enum
{
    AUTO_VENT_PENDING_NONE = 0,
    AUTO_VENT_PENDING_MOVE,
    AUTO_VENT_PENDING_STOP
} AutoVentilationPendingCommand_t;

typedef struct
{
    bool supported;
    FunctionAvailability_t initialAvailability;
    PermissionDegradedPolicy_t degradedPolicy;

    /**
     * Same unresolved issue as ClimateManager:
     * CIS cabinTemperature is int16 but actual scale is not fixed in SysRS.
     *
     * Example:
     *   10 => raw 300 means 30.0 C.
     *
     * Auto ventilation requires this to be non-zero.
     */
    uint16_t temperatureUnitsPerDegC;

    /**
     * Current demo uses one representative window channel.
     * Actual multi-channel mapping is a later integration decision.
     */
    uint8_t windowChannel;
} AutoVentilationManager_Config_t;

/**
 * Semantic external inputs whose physical producers are still TBD.
 * These APIs preserve the requirement boundary without inventing a wire signal.
 */
typedef struct
{
    bool ventilationEnabled;
    DataQuality_t enableQuality;

    bool vehicleUseActive;
    DataQuality_t vehicleUseQuality;

    bool windowOperationAllowed;
    DataQuality_t operationPermissionQuality;
} AutoVentilationExternalInputs_t;

typedef struct
{
    AutoVentilationState_t state;
    FunctionAvailability_t availability;

    AutoVentilationExternalInputs_t externalInputs;

    bool hasCurrentJob;
    DomainJobId_t currentJobId;
    uint32_t jobStartedAtMs;

    AutoVentilationJobResult_t lastJobResult;
    AutoVentilationReason_t lastJobReason;
    DomainJobId_t lastJobId;

    bool ownsWindowMotion;

    AutoVentilationPendingCommand_t pendingType;
    DomainCommandId_t pendingCommandId;
    uint32_t commandIssuedAtMs;

    ResultConfirmation_t lastStopConfirmation;

    bool manualWindowOverridePending;

    uint32_t nextJobId;
} AutoVentilationManager_Snapshot_t;

typedef struct
{
    AutoVentilationReason_t reason;
    AutoVentilationState_t state;

    bool jobStarted;
    bool jobEnded;
    DomainJobId_t jobId;
    AutoVentilationJobResult_t jobResult;

    bool commandCreated;
    bool commandDispatched;
    DomainCommandId_t commandId;
    WindowCommand_t windowCommand;
    uint8_t targetPositionPercent;
    DomainIf_Status_t txStatus;

    PermissionResult_t permission;
} AutoVentilationManager_Output_t;


/* Init / availability */
void AutoVentilationManager_LoadCurrentProjectDefaults(
    AutoVentilationManager_Config_t *outConfig);

AutoVentilationManager_Status_t AutoVentilationManager_Init(
    const AutoVentilationManager_Config_t *config);

bool AutoVentilationManager_IsInitialized(void);

AutoVentilationManager_Status_t AutoVentilationManager_SetAvailability(
    FunctionAvailability_t availability);


/* Semantic inputs; producer/path remains integration TBD. */
AutoVentilationManager_Status_t AutoVentilationManager_SetEnableInput(
    bool enabled,
    DataQuality_t quality);

AutoVentilationManager_Status_t AutoVentilationManager_SetVehicleUseInput(
    bool active,
    DataQuality_t quality);

AutoVentilationManager_Status_t AutoVentilationManager_SetWindowOperationPermission(
    bool allowed,
    DataQuality_t quality);

/**
 * Local/manual WINDOW action or explicit STOP that replaces the ventilation job.
 * If no job is active this notification is ignored.
 */
AutoVentilationManager_Status_t AutoVentilationManager_NotifyManualWindowOverride(void);


/* Main periodic/state-driven processing */
AutoVentilationManager_Status_t AutoVentilationManager_Process(
    uint32_t nowMs,
    AutoVentilationManager_Output_t *outResult);

/**
 * Call after Domain_Interface/ResultManager has processed the same WINDOW result.
 */
AutoVentilationManager_Status_t AutoVentilationManager_OnWindowCommandResult(
    const DomainIf_WindowCommandResult_t *result,
    uint32_t nowMs,
    AutoVentilationManager_Output_t *outResult);

AutoVentilationManager_Status_t AutoVentilationManager_GetSnapshot(
    AutoVentilationManager_Snapshot_t *outSnapshot);

#ifdef __cplusplus
}
#endif

#endif /* AUTO_VENTILATION_MANAGER_H */
