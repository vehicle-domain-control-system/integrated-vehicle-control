/**
 * @file AutoVentilationManager.c
 */

#include "AutoVentilationManager.h"

#include <stddef.h>
#include <string.h>

#include "DomainLifecycleManager.h"
#include "Domain_PolicyConfig.h"
#include "SettingsManager.h"
#include "VehicleStateManager.h"

typedef struct
{
    bool initialized;
    AutoVentilationManager_Config_t config;
    AutoVentilationManager_Snapshot_t snapshot;
} AutoVentilationContext_t;

static AutoVentilationContext_t g_vent;

static void Vent_ClearOutput(
    AutoVentilationManager_Output_t *out)
{
    (void)memset(out, 0, sizeof(*out));
    out->reason = AUTO_VENT_REASON_NONE;
    out->state = g_vent.snapshot.state;
    out->txStatus = DOMAIN_IF_NOT_AVAILABLE;
    out->permission.decision = PERMISSION_DECISION_DENY;
}

static bool Vent_ValueQualityTrusted(
    const ValueQuality_t *quality)
{
    return ((quality != NULL) &&
            (quality->validity == VALUE_VALIDITY_VALID) &&
            (quality->meta.quality == DATA_QUALITY_OK));
}

static bool Vent_WindowStateTrusted(
    const WindowState_t *window)
{
    return ((window != NULL) &&
            (window->meta.quality == DATA_QUALITY_OK) &&
            (window->positionPercent <= 100U) &&
            (window->ecuState == ECU_STATE_READY) &&
            (window->motionState != WINDOW_MOTION_UNKNOWN));
}

static bool Vent_ClimateStopped(
    const ClimateUserSettings_t *settings,
    const ClimateState_t *climate)
{
    if ((settings == NULL) || (climate == NULL))
        return false;

    if (settings->activeMode != SETTINGS_CLIMATE_MODE_NONE)
        return false;

    if (climate->meta.quality != DATA_QUALITY_OK)
        return false;

    return ((climate->commandedFanLevel == FAN_LEVEL_OFF) &&
            (climate->measuredFanLevel == FAN_LEVEL_OFF) &&
            (climate->thermalDirection == THERMAL_DIRECTION_IDLE) &&
            (climate->thermalOutputLevelPercent == 0U));
}

static int32_t Vent_DegCToRaw(
    int32_t degC)
{
    return degC *
        (int32_t)g_vent.config.temperatureUnitsPerDegC;
}

static AutoVentilationManager_Status_t Vent_EvaluateCommonPermission(
    PermissionResult_t *out)
{
    PermissionEvaluation_t e;
    DomainLifecycleSnapshot_t life;

    (void)memset(&e, 0, sizeof(e));

    e.requirements =
        PERMISSION_REQUIRE_DOMAIN_OPERATIONAL |
        PERMISSION_REQUIRE_FUNCTION_SUPPORTED |
        PERMISSION_REQUIRE_FUNCTION_AVAILABLE;

    e.facts.lifecycleState = DOMAIN_LIFECYCLE_STARTUP;
    if (DomainLifecycle_GetSnapshot(&life) == DLM_STATUS_OK)
        e.facts.lifecycleState = life.state;

    e.facts.functionSupported =
        g_vent.config.supported;
    e.facts.functionAvailability =
        g_vent.snapshot.availability;
    e.facts.degradedPolicy =
        g_vent.config.degradedPolicy;

    if (PermissionManager_Evaluate(
            &e,
            out) != PERMISSION_STATUS_OK)
        return AUTO_VENT_STATUS_DEPENDENCY_NOT_READY;

    return AUTO_VENT_STATUS_OK;
}

static DomainJobId_t Vent_AllocateJobId(void)
{
    DomainJobId_t id = g_vent.snapshot.nextJobId;

    ++g_vent.snapshot.nextJobId;
    if (g_vent.snapshot.nextJobId == 0U)
        g_vent.snapshot.nextJobId = 1U;

    if (id == 0U)
    {
        id = g_vent.snapshot.nextJobId;
        ++g_vent.snapshot.nextJobId;
        if (g_vent.snapshot.nextJobId == 0U)
            g_vent.snapshot.nextJobId = 1U;
    }

    return id;
}

static AutoVentilationManager_Status_t Vent_SendWindowCommand(
    WindowCommand_t commandType,
    uint8_t targetPositionPercent,
    AutoVentilationPendingCommand_t pendingType,
    uint32_t nowMs,
    AutoVentilationManager_Output_t *out)
{
    CommandCreateRequest_t create;
    CommandCreateResult_t created;
    DomainIf_WindowCommand_t tx;
    DomainIf_Status_t txStatus;

    (void)memset(&create, 0, sizeof(create));
    (void)memset(&created, 0, sizeof(created));
    (void)memset(&tx, 0, sizeof(tx));

    create.target = COMMAND_TARGET_WINDOW;
    create.functionId = DOMAIN_FUNCTION_WINDOW;
    create.type =
        (commandType == WINDOW_COMMAND_STOP) ?
        COMMAND_TYPE_WINDOW_STOP :
        COMMAND_TYPE_WINDOW_MOVE;
    create.origin = COMMAND_ORIGIN_AUTO_VENTILATION;
    create.hasJobContext = true;
    create.jobId = g_vent.snapshot.currentJobId;
    create.sourceMeta.quality = DATA_QUALITY_OK;

    if (CommandManager_Create(
            &create,
            nowMs,
            &created) != COMMAND_MANAGER_STATUS_OK)
        return AUTO_VENT_STATUS_COMMAND_ERROR;

    out->commandCreated = true;
    out->commandId = created.commandId;
    out->windowCommand = commandType;
    out->targetPositionPercent = targetPositionPercent;

    tx.context = created.interfaceContext;
    tx.channel = g_vent.config.windowChannel;
    tx.command = commandType;
    tx.targetPositionPercent = targetPositionPercent;

    txStatus = DomainIf_TxWindowCommand(&tx);
    out->txStatus = txStatus;

    if (txStatus != DOMAIN_IF_OK)
    {
        (void)CommandManager_AbortBeforeDispatch(
            created.commandId,
            REQUEST_RESULT_FAILED,
            RESULT_REASON_OTHER,
            nowMs);

        out->reason =
            AUTO_VENT_REASON_COMMAND_DISPATCH_FAILED;
        return AUTO_VENT_STATUS_OK;
    }

    g_vent.snapshot.pendingType = pendingType;
    g_vent.snapshot.pendingCommandId =
        created.commandId;
    g_vent.snapshot.commandIssuedAtMs = nowMs;

    out->commandDispatched = true;

    if (pendingType == AUTO_VENT_PENDING_MOVE)
    {
        g_vent.snapshot.ownsWindowMotion = true;
        out->reason =
            AUTO_VENT_REASON_MOVE_COMMAND_DISPATCHED;
    }
    else
    {
        out->reason =
            AUTO_VENT_REASON_STOP_COMMAND_DISPATCHED;
    }

    return AUTO_VENT_STATUS_OK;
}

static void Vent_SetPostJobArmState(void)
{
    /*
     * A valid OFF observed at termination can satisfy the required disable step.
     * Otherwise require a future valid OFF before another ON can arm.
     */
    if ((g_vent.snapshot.externalInputs.enableQuality ==
         DATA_QUALITY_OK) &&
        (!g_vent.snapshot.externalInputs.ventilationEnabled))
    {
        g_vent.snapshot.state =
            AUTO_VENT_STATE_READY_FOR_ENABLE;
    }
    else
    {
        g_vent.snapshot.state =
            AUTO_VENT_STATE_WAIT_ENABLE_OFF;
    }
}

static void Vent_RecordJobEnd(
    AutoVentilationJobResult_t result,
    AutoVentilationReason_t reason,
    AutoVentilationManager_Output_t *out)
{
    g_vent.snapshot.lastJobResult = result;
    g_vent.snapshot.lastJobReason = reason;
    g_vent.snapshot.lastJobId =
        g_vent.snapshot.currentJobId;

    out->jobEnded = true;
    out->jobId = g_vent.snapshot.currentJobId;
    out->jobResult = result;
    out->reason = reason;

    g_vent.snapshot.hasCurrentJob = false;
}

static AutoVentilationManager_Status_t Vent_EndJob(
    AutoVentilationJobResult_t result,
    AutoVentilationReason_t reason,
    uint32_t nowMs,
    AutoVentilationManager_Output_t *out)
{
    WindowState_t window;
    bool needsStop = false;

    /*
     * If a ventilation MOVE command is still pending and we are terminating,
     * the new STOP (if needed) supersedes that unfinished policy command.
     */
    if ((g_vent.snapshot.pendingType ==
         AUTO_VENT_PENDING_MOVE) &&
        (g_vent.snapshot.pendingCommandId != 0U))
    {
        (void)CommandManager_CancelByPolicy(
            g_vent.snapshot.pendingCommandId,
            RESULT_REASON_OTHER,
            nowMs);

        g_vent.snapshot.pendingType =
            AUTO_VENT_PENDING_NONE;
        g_vent.snapshot.pendingCommandId = 0U;
    }

    if (VehicleStateManager_GetWindow(&window) == VSM_STATUS_OK)
    {
        needsStop =
            g_vent.snapshot.ownsWindowMotion &&
            (window.meta.quality == DATA_QUALITY_OK) &&
            ((window.motionState == WINDOW_MOTION_OPENING) ||
             (window.motionState == WINDOW_MOTION_CLOSING));
    }

    Vent_RecordJobEnd(result, reason, out);

    if (needsStop)
    {
        /*
         * currentJobId is intentionally retained until STOP completes so
         * STOP command provenance remains linked to the ended ventilation job.
         */
        g_vent.snapshot.state =
            AUTO_VENT_STATE_STOPPING;
        g_vent.snapshot.lastStopConfirmation =
            RESULT_CONFIRMATION_UNCONFIRMED;

        return Vent_SendWindowCommand(
            WINDOW_COMMAND_STOP,
            window.positionPercent,
            AUTO_VENT_PENDING_STOP,
            nowMs,
            out);
    }

    g_vent.snapshot.ownsWindowMotion = false;
    g_vent.snapshot.pendingType =
        AUTO_VENT_PENDING_NONE;
    g_vent.snapshot.pendingCommandId = 0U;
    g_vent.snapshot.lastStopConfirmation =
        RESULT_CONFIRMATION_CONFIRMED;

    Vent_SetPostJobArmState();
    return AUTO_VENT_STATUS_OK;
}

static AutoVentilationManager_Status_t Vent_StartJob(
    const WindowState_t *window,
    uint32_t nowMs,
    AutoVentilationManager_Output_t *out)
{
    const DomainPolicyConfig_t *policy =
        DomainPolicyConfig_Get();

    if (policy == NULL)
        return AUTO_VENT_STATUS_DEPENDENCY_NOT_READY;

    g_vent.snapshot.currentJobId =
        Vent_AllocateJobId();
    g_vent.snapshot.hasCurrentJob = true;
    g_vent.snapshot.jobStartedAtMs = nowMs;
    g_vent.snapshot.state = AUTO_VENT_STATE_RUNNING;
    g_vent.snapshot.lastStopConfirmation =
        RESULT_CONFIRMATION_UNCONFIRMED;

    out->jobStarted = true;
    out->jobId = g_vent.snapshot.currentJobId;
    out->jobResult = AUTO_VENT_JOB_RUNNING;

    /*
     * 0=open / 100=closed.
     * If already open enough (<=80), never close it back to 80.
     */
    if (window->positionPercent <=
        policy->autoVentTargetClosedPct.value)
    {
        g_vent.snapshot.ownsWindowMotion = false;
        out->reason =
            AUTO_VENT_REASON_STARTED_NO_MOVE_NEEDED;
        return AUTO_VENT_STATUS_OK;
    }

    return Vent_SendWindowCommand(
        WINDOW_COMMAND_VENT,
        policy->autoVentTargetClosedPct.value,
        AUTO_VENT_PENDING_MOVE,
        nowMs,
        out);
}

static AutoVentilationManager_Status_t Vent_CheckPendingTimeout(
    uint32_t nowMs,
    AutoVentilationManager_Output_t *out)
{
    const DomainPolicyConfig_t *policy =
        DomainPolicyConfig_Get();
    uint32_t timeoutMs;
    ResultManager_Output_t unconfirmed;

    if (policy == NULL)
        return AUTO_VENT_STATUS_DEPENDENCY_NOT_READY;

    if (g_vent.snapshot.pendingType == AUTO_VENT_PENDING_NONE)
        return AUTO_VENT_STATUS_OK;

    timeoutMs =
        (g_vent.snapshot.pendingType == AUTO_VENT_PENDING_STOP) ?
        policy->windowStopResultWaitMs.value :
        policy->windowMoveResultWaitMs.value;

    if ((uint32_t)(nowMs -
        g_vent.snapshot.commandIssuedAtMs) < timeoutMs)
        return AUTO_VENT_STATUS_OK;

    (void)ResultManager_MarkCommandUnconfirmed(
        g_vent.snapshot.pendingCommandId,
        nowMs,
        &unconfirmed);

    if (g_vent.snapshot.pendingType == AUTO_VENT_PENDING_STOP)
    {
        g_vent.snapshot.pendingType =
            AUTO_VENT_PENDING_NONE;
        g_vent.snapshot.pendingCommandId = 0U;
        g_vent.snapshot.ownsWindowMotion = false;
        g_vent.snapshot.lastStopConfirmation =
            RESULT_CONFIRMATION_UNCONFIRMED;

        Vent_SetPostJobArmState();

        out->reason =
            AUTO_VENT_REASON_STOP_UNCONFIRMED;
        out->state = g_vent.snapshot.state;
        return AUTO_VENT_STATUS_OK;
    }

    /*
     * MOVE result timeout is a ventilation job failure.
     * Mark pending cleared before ending; EndJob may issue STOP if state says
     * the window is still moving under our ownership.
     */
    g_vent.snapshot.pendingType =
        AUTO_VENT_PENDING_NONE;
    g_vent.snapshot.pendingCommandId = 0U;

    return Vent_EndJob(
        AUTO_VENT_JOB_FAILED,
        AUTO_VENT_REASON_FAIL_MOVE_RESULT_TIMEOUT,
        nowMs,
        out);
}

static AutoVentilationManager_Status_t Vent_LoadRuntimeInputs(
    CisEnvironmentState_t *cis,
    WindowState_t *window,
    ClimateUserSettings_t *settings,
    ClimateState_t *climate)
{
    if ((VehicleStateManager_GetCisEnvironment(cis) != VSM_STATUS_OK) ||
        (VehicleStateManager_GetWindow(window) != VSM_STATUS_OK) ||
        (SettingsManager_GetClimate(settings) != SETTINGS_STATUS_OK) ||
        (VehicleStateManager_GetClimate(climate) != VSM_STATUS_OK))
        return AUTO_VENT_STATUS_DEPENDENCY_NOT_READY;

    return AUTO_VENT_STATUS_OK;
}

static bool Vent_StartInputsTrusted(
    const CisEnvironmentState_t *cis,
    const WindowState_t *window,
    const ClimateState_t *climate)
{
    return (
        (g_vent.snapshot.externalInputs.enableQuality ==
         DATA_QUALITY_OK) &&
        (g_vent.snapshot.externalInputs.vehicleUseQuality ==
         DATA_QUALITY_OK) &&
        (g_vent.snapshot.externalInputs.operationPermissionQuality ==
         DATA_QUALITY_OK) &&
        Vent_ValueQualityTrusted(&cis->occupant.quality) &&
        Vent_ValueQualityTrusted(&cis->cabin.temperatureQuality) &&
        Vent_WindowStateTrusted(window) &&
        (climate->meta.quality == DATA_QUALITY_OK)
    );
}

void AutoVentilationManager_LoadCurrentProjectDefaults(
    AutoVentilationManager_Config_t *outConfig)
{
    if (outConfig == NULL)
        return;

    (void)memset(outConfig, 0, sizeof(*outConfig));

    outConfig->supported = true;
    outConfig->initialAvailability =
        FUNCTION_AVAILABILITY_AVAILABLE;
    outConfig->degradedPolicy =
        PERMISSION_DEGRADED_DENY;

    /*
     * Temperature raw scale is still TBD in SysRS.
     */
    outConfig->temperatureUnitsPerDegC = 0U;

    /*
     * Current low-voltage demo is treated as one representative channel.
     * Actual channel mapping is integration TBD.
     */
    outConfig->windowChannel = 0U;
}

AutoVentilationManager_Status_t AutoVentilationManager_Init(
    const AutoVentilationManager_Config_t *config)
{
    AutoVentilationManager_Config_t defaults;

    if ((!VehicleStateManager_IsInitialized()) ||
        (!SettingsManager_IsInitialized()) ||
        (!PermissionManager_IsInitialized()) ||
        (!CommandManager_IsInitialized()) ||
        (!ResultManager_IsInitialized()) ||
        (DomainPolicyConfig_Get() == NULL))
        return AUTO_VENT_STATUS_DEPENDENCY_NOT_READY;

    if (config == NULL)
    {
        AutoVentilationManager_LoadCurrentProjectDefaults(
            &defaults);
        config = &defaults;
    }

    (void)memset(&g_vent, 0, sizeof(g_vent));

    g_vent.config = *config;
    g_vent.snapshot.availability =
        config->initialAvailability;

    /*
     * Boot does NOT arm an already-ON enable input.
     * A valid OFF must be observed first, preventing reboot-only restart.
     */
    g_vent.snapshot.state =
        AUTO_VENT_STATE_WAIT_ENABLE_OFF;
    g_vent.snapshot.lastJobResult =
        AUTO_VENT_JOB_NONE;
    g_vent.snapshot.nextJobId = 1U;
    g_vent.snapshot.externalInputs.enableQuality =
        DATA_QUALITY_NO_DATA;
    g_vent.snapshot.externalInputs.vehicleUseQuality =
        DATA_QUALITY_NO_DATA;
    g_vent.snapshot.externalInputs.operationPermissionQuality =
        DATA_QUALITY_NO_DATA;
    g_vent.snapshot.lastStopConfirmation =
        RESULT_CONFIRMATION_UNCONFIRMED;

    g_vent.initialized = true;
    return AUTO_VENT_STATUS_OK;
}

bool AutoVentilationManager_IsInitialized(void)
{
    return g_vent.initialized;
}

AutoVentilationManager_Status_t AutoVentilationManager_SetAvailability(
    FunctionAvailability_t availability)
{
    if (!g_vent.initialized)
        return AUTO_VENT_STATUS_NOT_INITIALIZED;

    g_vent.snapshot.availability = availability;
    return AUTO_VENT_STATUS_OK;
}

AutoVentilationManager_Status_t AutoVentilationManager_SetEnableInput(
    bool enabled,
    DataQuality_t quality)
{
    if (!g_vent.initialized)
        return AUTO_VENT_STATUS_NOT_INITIALIZED;

    g_vent.snapshot.externalInputs.ventilationEnabled =
        enabled;
    g_vent.snapshot.externalInputs.enableQuality =
        quality;

    if (quality != DATA_QUALITY_OK)
        return AUTO_VENT_STATUS_OK;

    if (!enabled)
    {
        /*
         * If a job is active, Process() performs the cancellation and STOP
         * decision. Otherwise this valid OFF satisfies re-arm prerequisite.
         */
        if ((g_vent.snapshot.state != AUTO_VENT_STATE_RUNNING) &&
            (g_vent.snapshot.state != AUTO_VENT_STATE_STOPPING))
        {
            g_vent.snapshot.state =
                AUTO_VENT_STATE_READY_FOR_ENABLE;
        }
    }
    else if (g_vent.snapshot.state ==
             AUTO_VENT_STATE_READY_FOR_ENABLE)
    {
        g_vent.snapshot.state =
            AUTO_VENT_STATE_ARMED;
    }

    return AUTO_VENT_STATUS_OK;
}

AutoVentilationManager_Status_t AutoVentilationManager_SetVehicleUseInput(
    bool active,
    DataQuality_t quality)
{
    if (!g_vent.initialized)
        return AUTO_VENT_STATUS_NOT_INITIALIZED;

    g_vent.snapshot.externalInputs.vehicleUseActive =
        active;
    g_vent.snapshot.externalInputs.vehicleUseQuality =
        quality;

    return AUTO_VENT_STATUS_OK;
}

AutoVentilationManager_Status_t AutoVentilationManager_SetWindowOperationPermission(
    bool allowed,
    DataQuality_t quality)
{
    if (!g_vent.initialized)
        return AUTO_VENT_STATUS_NOT_INITIALIZED;

    g_vent.snapshot.externalInputs.windowOperationAllowed =
        allowed;
    g_vent.snapshot.externalInputs.operationPermissionQuality =
        quality;

    return AUTO_VENT_STATUS_OK;
}

AutoVentilationManager_Status_t AutoVentilationManager_NotifyManualWindowOverride(void)
{
    if (!g_vent.initialized)
        return AUTO_VENT_STATUS_NOT_INITIALIZED;

    if (g_vent.snapshot.state == AUTO_VENT_STATE_RUNNING)
    {
        g_vent.snapshot.manualWindowOverridePending = true;

        /*
         * The new local/manual action has replaced our motion ownership.
         * Therefore ventilation termination must not send STOP that could
         * interrupt the new owner.
         */
        g_vent.snapshot.ownsWindowMotion = false;
    }

    return AUTO_VENT_STATUS_OK;
}

AutoVentilationManager_Status_t AutoVentilationManager_Process(
    uint32_t nowMs,
    AutoVentilationManager_Output_t *outResult)
{
    const DomainPolicyConfig_t *policy;
    CisEnvironmentState_t cis;
    WindowState_t window;
    ClimateUserSettings_t settings;
    ClimateState_t climate;
    PermissionResult_t permission;
    AutoVentilationManager_Status_t status;
    int32_t temperatureRaw;

    if (outResult == NULL)
        return AUTO_VENT_STATUS_INVALID_ARGUMENT;

    if (!g_vent.initialized)
        return AUTO_VENT_STATUS_NOT_INITIALIZED;

    Vent_ClearOutput(outResult);

    policy = DomainPolicyConfig_Get();
    if (policy == NULL)
        return AUTO_VENT_STATUS_DEPENDENCY_NOT_READY;

    if (g_vent.config.temperatureUnitsPerDegC == 0U)
    {
        outResult->reason =
            AUTO_VENT_REASON_CONFIGURATION_REQUIRED;
        return AUTO_VENT_STATUS_CONFIGURATION_REQUIRED;
    }

    status = Vent_CheckPendingTimeout(
        nowMs,
        outResult);

    if (status != AUTO_VENT_STATUS_OK)
        return status;

    if (outResult->reason ==
        AUTO_VENT_REASON_STOP_UNCONFIRMED)
        return AUTO_VENT_STATUS_OK;

    if (g_vent.snapshot.state ==
        AUTO_VENT_STATE_STOPPING)
    {
        outResult->state = g_vent.snapshot.state;
        return AUTO_VENT_STATUS_OK;
    }

    status = Vent_LoadRuntimeInputs(
        &cis,
        &window,
        &settings,
        &climate);

    if (status != AUTO_VENT_STATUS_OK)
        return status;

    /*
     * Running job: evaluate termination before any new start logic.
     */
    if (g_vent.snapshot.state == AUTO_VENT_STATE_RUNNING)
    {
        if (g_vent.snapshot.manualWindowOverridePending)
        {
            g_vent.snapshot.manualWindowOverridePending = false;
            return Vent_EndJob(
                AUTO_VENT_JOB_CANCELLED,
                AUTO_VENT_REASON_CANCEL_MANUAL_WINDOW,
                nowMs,
                outResult);
        }

        if (g_vent.snapshot.externalInputs.enableQuality !=
            DATA_QUALITY_OK)
        {
            return Vent_EndJob(
                AUTO_VENT_JOB_FAILED,
                AUTO_VENT_REASON_FAIL_REQUIRED_INPUT_UNTRUSTED,
                nowMs,
                outResult);
        }

        if (!g_vent.snapshot.externalInputs.ventilationEnabled)
        {
            return Vent_EndJob(
                AUTO_VENT_JOB_CANCELLED,
                AUTO_VENT_REASON_CANCEL_ENABLE_OFF,
                nowMs,
                outResult);
        }

        if (g_vent.snapshot.externalInputs.vehicleUseQuality !=
            DATA_QUALITY_OK)
        {
            return Vent_EndJob(
                AUTO_VENT_JOB_FAILED,
                AUTO_VENT_REASON_FAIL_REQUIRED_INPUT_UNTRUSTED,
                nowMs,
                outResult);
        }

        if (g_vent.snapshot.externalInputs.vehicleUseActive)
        {
            return Vent_EndJob(
                AUTO_VENT_JOB_CANCELLED,
                AUTO_VENT_REASON_CANCEL_VEHICLE_USE_RESUMED,
                nowMs,
                outResult);
        }

        if (g_vent.snapshot.externalInputs.operationPermissionQuality !=
            DATA_QUALITY_OK)
        {
            return Vent_EndJob(
                AUTO_VENT_JOB_FAILED,
                AUTO_VENT_REASON_FAIL_OPERATION_PERMISSION_UNTRUSTED,
                nowMs,
                outResult);
        }

        if (!g_vent.snapshot.externalInputs.windowOperationAllowed)
        {
            return Vent_EndJob(
                AUTO_VENT_JOB_CANCELLED,
                AUTO_VENT_REASON_CANCEL_OPERATION_WITHDRAWN,
                nowMs,
                outResult);
        }

        if (!Vent_ValueQualityTrusted(&cis.occupant.quality) ||
            !Vent_ValueQualityTrusted(&cis.cabin.temperatureQuality))
        {
            return Vent_EndJob(
                AUTO_VENT_JOB_FAILED,
                AUTO_VENT_REASON_FAIL_REQUIRED_INPUT_UNTRUSTED,
                nowMs,
                outResult);
        }

        if (cis.occupant.occupantPresent)
        {
            return Vent_EndJob(
                AUTO_VENT_JOB_CANCELLED,
                AUTO_VENT_REASON_CANCEL_OCCUPANT_DETECTED,
                nowMs,
                outResult);
        }

        if (!Vent_WindowStateTrusted(&window))
        {
            return Vent_EndJob(
                AUTO_VENT_JOB_FAILED,
                AUTO_VENT_REASON_FAIL_WINDOW_STATE,
                nowMs,
                outResult);
        }

        if (window.antiPinchState != ANTIPINCH_STATE_CLEAR)
        {
            return Vent_EndJob(
                AUTO_VENT_JOB_FAILED,
                AUTO_VENT_REASON_FAIL_WINDOW_PROTECTION,
                nowMs,
                outResult);
        }

        /*
         * Another climate mode starts after ventilation began -> cancel.
         * Also use actual BCM state so a non-setting external climate start
         * is not silently ignored.
         */
        if (!Vent_ClimateStopped(&settings, &climate))
        {
            return Vent_EndJob(
                AUTO_VENT_JOB_CANCELLED,
                AUTO_VENT_REASON_CANCEL_OTHER_CLIMATE,
                nowMs,
                outResult);
        }

        temperatureRaw =
            (int32_t)cis.cabin.cabinTemperature;

        if (temperatureRaw <=
            Vent_DegCToRaw(
                policy->autoVentStopTempDegC.value))
        {
            return Vent_EndJob(
                AUTO_VENT_JOB_COMPLETED,
                AUTO_VENT_REASON_NORMAL_TEMP_REACHED,
                nowMs,
                outResult);
        }

        if ((uint32_t)(nowMs -
            g_vent.snapshot.jobStartedAtMs) >=
            policy->autoVentMaxDurationMs.value)
        {
            return Vent_EndJob(
                AUTO_VENT_JOB_COMPLETED,
                AUTO_VENT_REASON_NORMAL_MAX_DURATION,
                nowMs,
                outResult);
        }

        outResult->state = g_vent.snapshot.state;
        outResult->jobId = g_vent.snapshot.currentJobId;
        outResult->jobResult = AUTO_VENT_JOB_RUNNING;
        return AUTO_VENT_STATUS_OK;
    }

    /*
     * Not active: state machine requires valid OFF -> new ON.
     */
    if (g_vent.snapshot.state ==
        AUTO_VENT_STATE_WAIT_ENABLE_OFF)
    {
        outResult->reason =
            AUTO_VENT_REASON_WAIT_ENABLE_OFF;
        outResult->state = g_vent.snapshot.state;
        return AUTO_VENT_STATUS_OK;
    }

    if (g_vent.snapshot.state ==
        AUTO_VENT_STATE_READY_FOR_ENABLE)
    {
        outResult->reason =
            AUTO_VENT_REASON_WAIT_ENABLE_OFF;
        outResult->state = g_vent.snapshot.state;
        return AUTO_VENT_STATUS_OK;
    }

    if (g_vent.snapshot.state !=
        AUTO_VENT_STATE_ARMED)
        return AUTO_VENT_STATUS_OK;

    /*
     * Before a job exists, untrusted conditions simply prevent starting.
     * They do not invent a FAILED job.
     */
    if (!Vent_StartInputsTrusted(
            &cis,
            &window,
            &climate))
    {
        outResult->reason =
            AUTO_VENT_REASON_ARMED_WAIT_CONDITIONS;
        return AUTO_VENT_STATUS_OK;
    }

    if ((!g_vent.snapshot.externalInputs.ventilationEnabled) ||
        g_vent.snapshot.externalInputs.vehicleUseActive ||
        cis.occupant.occupantPresent ||
        (!g_vent.snapshot.externalInputs.windowOperationAllowed) ||
        (!Vent_ClimateStopped(&settings, &climate)) ||
        (window.antiPinchState != ANTIPINCH_STATE_CLEAR) ||
        (window.motionState != WINDOW_MOTION_STOPPED))
    {
        outResult->reason =
            AUTO_VENT_REASON_ARMED_WAIT_CONDITIONS;
        return AUTO_VENT_STATUS_OK;
    }

    if (Vent_EvaluateCommonPermission(&permission) !=
        AUTO_VENT_STATUS_OK)
        return AUTO_VENT_STATUS_DEPENDENCY_NOT_READY;

    outResult->permission = permission;

    if (permission.decision != PERMISSION_DECISION_ALLOW)
    {
        outResult->reason =
            AUTO_VENT_REASON_ARMED_WAIT_CONDITIONS;
        return AUTO_VENT_STATUS_OK;
    }

    temperatureRaw =
        (int32_t)cis.cabin.cabinTemperature;

    if (temperatureRaw <
        Vent_DegCToRaw(
            policy->autoVentStartTempDegC.value))
    {
        outResult->reason =
            AUTO_VENT_REASON_ARMED_WAIT_CONDITIONS;
        return AUTO_VENT_STATUS_OK;
    }

    return Vent_StartJob(
        &window,
        nowMs,
        outResult);
}

AutoVentilationManager_Status_t AutoVentilationManager_OnWindowCommandResult(
    const DomainIf_WindowCommandResult_t *result,
    uint32_t nowMs,
    AutoVentilationManager_Output_t *outResult)
{
    AutoVentilationPendingCommand_t pending;

    (void)nowMs;

    if ((result == NULL) || (outResult == NULL))
        return AUTO_VENT_STATUS_INVALID_ARGUMENT;

    if (!g_vent.initialized)
        return AUTO_VENT_STATUS_NOT_INITIALIZED;

    Vent_ClearOutput(outResult);

    if ((g_vent.snapshot.pendingType == AUTO_VENT_PENDING_NONE) ||
        (result->commandId != g_vent.snapshot.pendingCommandId))
        return AUTO_VENT_STATUS_OK;

    pending = g_vent.snapshot.pendingType;

    if ((result->result == REQUEST_RESULT_ACCEPTED) ||
        (result->result == REQUEST_RESULT_IN_PROGRESS))
    {
        outResult->state = g_vent.snapshot.state;
        return AUTO_VENT_STATUS_OK;
    }

    if (pending == AUTO_VENT_PENDING_MOVE)
    {
        if (result->result == REQUEST_RESULT_DONE)
        {
            g_vent.snapshot.pendingType =
                AUTO_VENT_PENDING_NONE;
            g_vent.snapshot.pendingCommandId = 0U;
            g_vent.snapshot.ownsWindowMotion = false;

            outResult->state = g_vent.snapshot.state;
            outResult->jobId =
                g_vent.snapshot.currentJobId;
            outResult->jobResult =
                AUTO_VENT_JOB_RUNNING;

            /*
             * Move DONE != ventilation job DONE.
             */
            return AUTO_VENT_STATUS_OK;
        }

        if ((result->result == REQUEST_RESULT_REJECTED) ||
            (result->result == REQUEST_RESULT_CANCELLED) ||
            (result->result == REQUEST_RESULT_FAILED))
        {
            g_vent.snapshot.pendingType =
                AUTO_VENT_PENDING_NONE;
            g_vent.snapshot.pendingCommandId = 0U;

            return Vent_EndJob(
                AUTO_VENT_JOB_FAILED,
                AUTO_VENT_REASON_FAIL_MOVE_COMMAND,
                nowMs,
                outResult);
        }

        return AUTO_VENT_STATUS_OK;
    }

    /* STOP result */
    if (pending == AUTO_VENT_PENDING_STOP)
    {
        g_vent.snapshot.pendingType =
            AUTO_VENT_PENDING_NONE;
        g_vent.snapshot.pendingCommandId = 0U;
        g_vent.snapshot.ownsWindowMotion = false;

        if (result->result == REQUEST_RESULT_DONE)
        {
            g_vent.snapshot.lastStopConfirmation =
                RESULT_CONFIRMATION_CONFIRMED;
            outResult->reason =
                AUTO_VENT_REASON_STOP_CONFIRMED;
        }
        else
        {
            /*
             * Job end reason remains the previously recorded one.
             * STOP itself was not confirmed successful.
             */
            g_vent.snapshot.lastStopConfirmation =
                RESULT_CONFIRMATION_UNCONFIRMED;
            outResult->reason =
                AUTO_VENT_REASON_STOP_UNCONFIRMED;
        }

        Vent_SetPostJobArmState();
        outResult->state = g_vent.snapshot.state;
        outResult->jobId = g_vent.snapshot.lastJobId;
        outResult->jobResult =
            g_vent.snapshot.lastJobResult;

        return AUTO_VENT_STATUS_OK;
    }

    return AUTO_VENT_STATUS_OK;
}

AutoVentilationManager_Status_t AutoVentilationManager_GetSnapshot(
    AutoVentilationManager_Snapshot_t *outSnapshot)
{
    if (outSnapshot == NULL)
        return AUTO_VENT_STATUS_INVALID_ARGUMENT;

    if (!g_vent.initialized)
        return AUTO_VENT_STATUS_NOT_INITIALIZED;

    *outSnapshot = g_vent.snapshot;
    return AUTO_VENT_STATUS_OK;
}
