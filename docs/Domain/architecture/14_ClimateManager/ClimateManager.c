/**
 * @file ClimateManager.c
 */

#include "ClimateManager.h"

#include <stddef.h>
#include <string.h>

#include "DomainLifecycleManager.h"
#include "Domain_PolicyConfig.h"
#include "VehicleStateManager.h"

typedef struct
{
    bool initialized;
    ClimateManager_Config_t config;
    ClimateManager_Snapshot_t snapshot;
} ClimateContext_t;

static ClimateContext_t g_climate;

static void Climate_ClearOutput(ClimateManager_Output_t *out)
{
    (void)memset(out, 0, sizeof(*out));
    out->decision = CLIMATE_DECISION_NONE;
    out->txStatus = DOMAIN_IF_NOT_AVAILABLE;
    out->permission.decision = PERMISSION_DECISION_DENY;
}

static bool Climate_TemperatureTrusted(
    const CisCabinEnvironmentState_t *cabin)
{
    return ((cabin != NULL) &&
            (cabin->temperatureQuality.validity == VALUE_VALIDITY_VALID) &&
            (cabin->temperatureQuality.meta.quality == DATA_QUALITY_OK));
}

static int32_t Climate_Abs32(int32_t value)
{
    return (value < 0) ? -value : value;
}

static int32_t Climate_ToMilliCFromRawDelta(
    int32_t rawDelta)
{
    return (rawDelta * 1000) /
        (int32_t)g_climate.config.temperatureUnitsPerDegC;
}

static FanLevel_t Climate_SelectAutoFanInitial(
    int32_t absMilliC)
{
    if (absMilliC >= 4000)
        return FAN_LEVEL_HIGH;
    if (absMilliC >= 2000)
        return FAN_LEVEL_MEDIUM;
    if (absMilliC >= 500)
        return FAN_LEVEL_LOW;
    return FAN_LEVEL_OFF;
}

static FanLevel_t Climate_SelectAutoFanHysteresis(
    FanLevel_t current,
    int32_t absMilliC)
{
    /*
     * SysRS A.BCM.4.1:
     * rise: 0.5 / 2.0 / 4.0 C
     * fall: 0.2 / 1.7 / 3.7 C
     * Large changes may cross multiple boundaries.
     */
    FanLevel_t level = current;
    bool changed = true;

    if (level == FAN_LEVEL_UNKNOWN)
        return Climate_SelectAutoFanInitial(absMilliC);

    while (changed)
    {
        changed = false;

        switch (level)
        {
            case FAN_LEVEL_OFF:
                if (absMilliC >= 500)
                {
                    level = FAN_LEVEL_LOW;
                    changed = true;
                }
                break;

            case FAN_LEVEL_LOW:
                if (absMilliC >= 2000)
                {
                    level = FAN_LEVEL_MEDIUM;
                    changed = true;
                }
                else if (absMilliC < 200)
                {
                    level = FAN_LEVEL_OFF;
                    changed = true;
                }
                break;

            case FAN_LEVEL_MEDIUM:
                if (absMilliC >= 4000)
                {
                    level = FAN_LEVEL_HIGH;
                    changed = true;
                }
                else if (absMilliC < 1700)
                {
                    level = FAN_LEVEL_LOW;
                    changed = true;
                }
                break;

            case FAN_LEVEL_HIGH:
                if (absMilliC < 3700)
                {
                    level = FAN_LEVEL_MEDIUM;
                    changed = true;
                }
                break;

            case FAN_LEVEL_UNKNOWN:
            default:
                level = Climate_SelectAutoFanInitial(absMilliC);
                changed = false;
                break;
        }
    }

    return level;
}

static void Climate_SelectThermal(
    int32_t deltaMilliC,
    ThermalDirection_t *outDirection,
    uint8_t *outPercent)
{
    int32_t absDelta = Climate_Abs32(deltaMilliC);

    if (absDelta <= 500)
    {
        *outDirection = THERMAL_DIRECTION_IDLE;
        *outPercent = 0U;
        return;
    }

    *outDirection =
        (deltaMilliC > 0) ?
        THERMAL_DIRECTION_COOL :
        THERMAL_DIRECTION_HEAT;

    if (absDelta < 2000)
    {
        *outPercent = 30U;
    }
    else if (absDelta < 4000)
    {
        *outPercent = 50U;
    }
    else
    {
        *outPercent =
            g_climate.config.maxThermalOutputPercent;
    }
}

static bool Climate_StateMatches(
    const ClimateState_t *state,
    FanLevel_t fan,
    ThermalDirection_t thermal,
    uint8_t percent,
    bool requireMeasuredFan)
{
    if ((state == NULL) ||
        (state->meta.quality != DATA_QUALITY_OK))
        return false;

    if (state->commandedFanLevel != fan)
        return false;

    if (requireMeasuredFan &&
        (state->measuredFanLevel != fan))
        return false;

    if (state->thermalDirection != thermal)
        return false;

    if (state->thermalOutputLevelPercent != percent)
        return false;

    return true;
}

static ClimateManager_Status_t Climate_EvaluatePermission(
    bool requireTemperature,
    PermissionResult_t *out)
{
    PermissionEvaluation_t e;
    DomainLifecycleSnapshot_t life;
    VehicleStateSnapshot_t vehicle;

    (void)memset(&e, 0, sizeof(e));

    e.requirements =
        PERMISSION_REQUIRE_DOMAIN_OPERATIONAL |
        PERMISSION_REQUIRE_FUNCTION_SUPPORTED |
        PERMISSION_REQUIRE_FUNCTION_AVAILABLE;

    if (requireTemperature)
        e.requirements |= PERMISSION_REQUIRE_INPUT_QUALITY;

    e.facts.lifecycleState = DOMAIN_LIFECYCLE_STARTUP;
    if (DomainLifecycle_GetSnapshot(&life) == DLM_STATUS_OK)
        e.facts.lifecycleState = life.state;

    e.facts.functionSupported = g_climate.config.supported;
    e.facts.functionAvailability = g_climate.snapshot.availability;
    e.facts.degradedPolicy = g_climate.config.degradedPolicy;

    if (VehicleStateManager_GetSnapshot(&vehicle) != VSM_STATUS_OK)
        return CLIMATE_MANAGER_STATUS_DEPENDENCY_NOT_READY;

    e.facts.requiredInputQuality =
        vehicle.vehicle.environment.cabin.temperatureQuality.meta.quality;

    if (PermissionManager_Evaluate(&e, out) != PERMISSION_STATUS_OK)
        return CLIMATE_MANAGER_STATUS_DEPENDENCY_NOT_READY;

    return CLIMATE_MANAGER_STATUS_OK;
}

static ClimateManager_Status_t Climate_SendCommand(
    FanLevel_t fan,
    ThermalDirection_t thermal,
    uint8_t percent,
    CommandOrigin_t origin,
    bool hasRequest,
    DeviceContextId_t device,
    const RequestContext_t *requestContext,
    uint32_t nowMs,
    ClimateManager_Phase_t phase,
    ClimateManager_Output_t *out)
{
    CommandCreateRequest_t create;
    CommandCreateResult_t created;
    DomainIf_BcmClimateCommand_t tx;
    CommandManager_Status_t cs;
    DomainIf_Status_t txStatus;

    (void)memset(&create, 0, sizeof(create));
    (void)memset(&created, 0, sizeof(created));
    (void)memset(&tx, 0, sizeof(tx));

    create.target = COMMAND_TARGET_BCM;
    create.functionId = DOMAIN_FUNCTION_CLIMATE;
    create.type = COMMAND_TYPE_BCM_CLIMATE;
    create.origin = origin;
    create.hasRequestContext = hasRequest;
    if (hasRequest)
    {
        create.requestDeviceContextId = device;
        create.requestContext = *requestContext;
    }
    create.sourceMeta.quality = DATA_QUALITY_OK;

    cs = CommandManager_Create(&create, nowMs, &created);
    if (cs != COMMAND_MANAGER_STATUS_OK)
        return CLIMATE_MANAGER_STATUS_COMMAND_ERROR;

    out->commandCreated = true;
    out->commandId = created.commandId;

    tx.context = created.interfaceContext;
    tx.fanTarget = fan;
    tx.thermalDirection = thermal;
    tx.thermalTargetLevelPercent = percent;

    txStatus = DomainIf_TxBcmClimateCommand(&tx);
    out->txStatus = txStatus;

    if (txStatus != DOMAIN_IF_OK)
    {
        (void)CommandManager_AbortBeforeDispatch(
            created.commandId,
            REQUEST_RESULT_FAILED,
            RESULT_REASON_OTHER,
            nowMs);
        out->decision = CLIMATE_DECISION_COMMAND_DISPATCH_FAILED;
        return CLIMATE_MANAGER_STATUS_OK;
    }

    g_climate.snapshot.hasPendingCommand = true;
    g_climate.snapshot.pendingCommandId = created.commandId;
    g_climate.snapshot.commandIssuedAtMs = nowMs;
    g_climate.snapshot.phase = phase;
    g_climate.snapshot.desiredFan = fan;
    g_climate.snapshot.desiredThermalDirection = thermal;
    g_climate.snapshot.desiredThermalPercent = percent;

    out->commandDispatched = true;
    return CLIMATE_MANAGER_STATUS_OK;
}

static ClimateManager_Status_t Climate_StartAuto(
    uint32_t nowMs,
    ClimateManager_Output_t *out)
{
    ClimateUserSettings_t settings;
    VehicleStateSnapshot_t vehicle;
    PermissionResult_t permission;
    int32_t rawDelta;
    int32_t deltaMilliC;
    int32_t absMilliC;
    FanLevel_t fan;
    ThermalDirection_t thermal;
    uint8_t percent;

    if ((g_climate.config.temperatureUnitsPerDegC < 2U) ||
        (!g_climate.config.maxThermalOutputConfigured))
    {
        out->decision = CLIMATE_DECISION_CONFIGURATION_REQUIRED;
        return CLIMATE_MANAGER_STATUS_CONFIGURATION_REQUIRED;
    }

    if (SettingsManager_GetClimate(&settings) != SETTINGS_STATUS_OK ||
        VehicleStateManager_GetSnapshot(&vehicle) != VSM_STATUS_OK)
        return CLIMATE_MANAGER_STATUS_DEPENDENCY_NOT_READY;

    if ((!settings.hasTargetTemperature) ||
        (!Climate_TemperatureTrusted(&vehicle.vehicle.environment.cabin)))
    {
        out->decision = CLIMATE_DECISION_REQUIRED_TEMPERATURE_UNTRUSTED;

        /*
         * Required automatic input lost -> automatic target OFF.
         * Send OFF only when no command is already pending.
         */
        if (!g_climate.snapshot.hasPendingCommand)
        {
            return Climate_SendCommand(
                FAN_LEVEL_OFF,
                THERMAL_DIRECTION_IDLE,
                0U,
                COMMAND_ORIGIN_CLIMATE_POLICY,
                false, 0U, NULL,
                nowMs,
                CLIMATE_PHASE_AUTO_FAN_PENDING,
                out);
        }
        return CLIMATE_MANAGER_STATUS_OK;
    }

    if (Climate_EvaluatePermission(true, &permission) !=
        CLIMATE_MANAGER_STATUS_OK)
        return CLIMATE_MANAGER_STATUS_DEPENDENCY_NOT_READY;

    out->permission = permission;
    if (permission.decision != PERMISSION_DECISION_ALLOW)
    {
        out->decision = CLIMATE_DECISION_PERMISSION_DENIED;
        return CLIMATE_MANAGER_STATUS_OK;
    }

    rawDelta =
        (int32_t)vehicle.vehicle.environment.cabin.cabinTemperature -
        (int32_t)settings.targetTemperature;
    deltaMilliC = Climate_ToMilliCFromRawDelta(rawDelta);
    absMilliC = Climate_Abs32(deltaMilliC);

    if (g_climate.snapshot.phase == CLIMATE_PHASE_IDLE ||
        g_climate.snapshot.selectedMode != SETTINGS_CLIMATE_MODE_AUTO)
        fan = Climate_SelectAutoFanInitial(absMilliC);
    else
        fan = Climate_SelectAutoFanHysteresis(
            g_climate.snapshot.autoFanMemory,
            absMilliC);

    Climate_SelectThermal(
        deltaMilliC,
        &thermal,
        &percent);

    g_climate.snapshot.autoFanMemory = fan;
    g_climate.snapshot.selectedMode = SETTINGS_CLIMATE_MODE_AUTO;

    if (g_climate.snapshot.hasPendingCommand)
    {
        out->decision = CLIMATE_DECISION_COMMAND_PENDING;
        return CLIMATE_MANAGER_STATUS_OK;
    }

    /*
     * If thermal output is needed, first command Fan + Thermal OFF.
     * If no thermal output is needed, a single Fan/IDLE command is enough.
     */
    if ((thermal != THERMAL_DIRECTION_IDLE) &&
        (percent > 0U))
    {
        out->decision = CLIMATE_DECISION_AUTO_FAN_COMMAND_DISPATCHED;
        return Climate_SendCommand(
            fan,
            THERMAL_DIRECTION_IDLE,
            0U,
            COMMAND_ORIGIN_CLIMATE_POLICY,
            false, 0U, NULL,
            nowMs,
            CLIMATE_PHASE_AUTO_FAN_PENDING,
            out);
    }

    out->decision = CLIMATE_DECISION_AUTO_IDLE_COMMAND_DISPATCHED;
    return Climate_SendCommand(
        fan,
        THERMAL_DIRECTION_IDLE,
        0U,
        COMMAND_ORIGIN_CLIMATE_POLICY,
        false, 0U, NULL,
        nowMs,
        CLIMATE_PHASE_AUTO_FAN_PENDING,
        out);
}

void ClimateManager_LoadCurrentProjectDefaults(
    ClimateManager_Config_t *outConfig)
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
     * Temperature wire/internal scale is NOT DEFINED in current SysRS.
     * Do not guess.
     */
    outConfig->temperatureUnitsPerDegC = 0U;

    /*
     * 100% exists only as a candidate maximum; integration must opt in.
     */
    outConfig->maxThermalOutputConfigured = false;
    outConfig->maxThermalOutputPercent = 100U;
}

ClimateManager_Status_t ClimateManager_Init(
    const ClimateManager_Config_t *config)
{
    ClimateManager_Config_t defaults;

    if ((!VehicleStateManager_IsInitialized()) ||
        (!SettingsManager_IsInitialized()) ||
        (!PermissionManager_IsInitialized()) ||
        (!CommandManager_IsInitialized()) ||
        (!ResultManager_IsInitialized()) ||
        (DomainPolicyConfig_Get() == NULL))
        return CLIMATE_MANAGER_STATUS_DEPENDENCY_NOT_READY;

    if (config == NULL)
    {
        ClimateManager_LoadCurrentProjectDefaults(&defaults);
        config = &defaults;
    }

    if (config->maxThermalOutputPercent > 100U)
        return CLIMATE_MANAGER_STATUS_INVALID_ARGUMENT;

    (void)memset(&g_climate, 0, sizeof(g_climate));
    g_climate.config = *config;
    g_climate.snapshot.availability =
        config->initialAvailability;
    g_climate.snapshot.phase = CLIMATE_PHASE_IDLE;
    g_climate.snapshot.selectedMode = SETTINGS_CLIMATE_MODE_NONE;
    g_climate.snapshot.autoFanMemory = FAN_LEVEL_UNKNOWN;
    g_climate.initialized = true;

    return CLIMATE_MANAGER_STATUS_OK;
}

bool ClimateManager_IsInitialized(void)
{
    return g_climate.initialized;
}

ClimateManager_Status_t ClimateManager_SetAvailability(
    FunctionAvailability_t availability)
{
    if (!g_climate.initialized)
        return CLIMATE_MANAGER_STATUS_NOT_INITIALIZED;

    g_climate.snapshot.availability = availability;
    return CLIMATE_MANAGER_STATUS_OK;
}

ClimateManager_Status_t ClimateManager_HandleMobileRequest(
    const DomainIf_MobileRequest_t *request,
    uint32_t sessionGeneration,
    uint32_t nowMs,
    ClimateManager_Output_t *outResult)
{
    SettingsApplyResult_t apply;
    SettingsManager_Status_t ss;
    PermissionResult_t permission;

    if ((request == NULL) || (outResult == NULL))
        return CLIMATE_MANAGER_STATUS_INVALID_ARGUMENT;

    if (!g_climate.initialized)
        return CLIMATE_MANAGER_STATUS_NOT_INITIALIZED;

    Climate_ClearOutput(outResult);

    if ((request->type != MOBILE_REQUEST_CLIMATE_TARGET_TEMPERATURE) &&
        (request->type != MOBILE_REQUEST_CLIMATE_AUTO_ENABLE) &&
        (request->type != MOBILE_REQUEST_FAN_LEVEL))
        return CLIMATE_MANAGER_STATUS_INVALID_ARGUMENT;

    /* Manual fan requires execution permission before staging. */
    if (request->type == MOBILE_REQUEST_FAN_LEVEL)
    {
        if (Climate_EvaluatePermission(false, &permission) !=
            CLIMATE_MANAGER_STATUS_OK)
            return CLIMATE_MANAGER_STATUS_DEPENDENCY_NOT_READY;

        outResult->permission = permission;
        if (permission.decision != PERMISSION_DECISION_ALLOW)
        {
            outResult->decision = CLIMATE_DECISION_PERMISSION_DENIED;
            return CLIMATE_MANAGER_STATUS_OK;
        }

        if (g_climate.snapshot.hasPendingCommand)
        {
            outResult->decision = CLIMATE_DECISION_COMMAND_PENDING;
            return CLIMATE_MANAGER_STATUS_BUSY;
        }
    }

    ss = SettingsManager_ApplyMobileSettingRequest(
        request,
        sessionGeneration,
        nowMs,
        &apply);

    outResult->settingApply = apply;

    if (ss == SETTINGS_STATUS_CONFIG_REQUIRED)
    {
        outResult->decision = CLIMATE_DECISION_CONFIGURATION_REQUIRED;
        return CLIMATE_MANAGER_STATUS_CONFIGURATION_REQUIRED;
    }

    if ((ss != SETTINGS_STATUS_OK) &&
        (ss != SETTINGS_STATUS_UNSUPPORTED))
        return CLIMATE_MANAGER_STATUS_INVALID_ARGUMENT;

    if (request->type == MOBILE_REQUEST_CLIMATE_TARGET_TEMPERATURE)
    {
        outResult->decision = CLIMATE_DECISION_SETTING_CONFIRMED;

        {
            ClimateUserSettings_t s;
            if (SettingsManager_GetClimate(&s) == SETTINGS_STATUS_OK &&
                s.activeMode == SETTINGS_CLIMATE_MODE_AUTO)
                return Climate_StartAuto(nowMs, outResult);
        }

        outResult->decision = CLIMATE_DECISION_TARGET_ONLY_NO_START;
        return CLIMATE_MANAGER_STATUS_OK;
    }

    if (request->type == MOBILE_REQUEST_CLIMATE_AUTO_ENABLE)
    {
        outResult->decision = CLIMATE_DECISION_SETTING_CONFIRMED;

        if (!request->payload.enable)
        {
            g_climate.snapshot.selectedMode = SETTINGS_CLIMATE_MODE_NONE;
            g_climate.snapshot.autoFanMemory = FAN_LEVEL_UNKNOWN;

            if (!g_climate.snapshot.hasPendingCommand)
            {
                return Climate_SendCommand(
                    FAN_LEVEL_OFF,
                    THERMAL_DIRECTION_IDLE,
                    0U,
                    COMMAND_ORIGIN_CLIMATE_POLICY,
                    false, 0U, NULL,
                    nowMs,
                    CLIMATE_PHASE_AUTO_FAN_PENDING,
                    outResult);
            }
            return CLIMATE_MANAGER_STATUS_OK;
        }

        return Climate_StartAuto(nowMs, outResult);
    }

    /* Manual fan: SettingsManager staged it; execute Fan + Thermal OFF. */
    g_climate.snapshot.pendingManualRequest = true;
    g_climate.snapshot.pendingManualDevice =
        request->deviceContextId;
    g_climate.snapshot.pendingManualRequestContext =
        request->context;
    g_climate.snapshot.pendingManualSessionGeneration =
        sessionGeneration;

    outResult->decision = CLIMATE_DECISION_MANUAL_COMMAND_DISPATCHED;

    return Climate_SendCommand(
        request->payload.fanLevel,
        THERMAL_DIRECTION_IDLE,
        0U,
        COMMAND_ORIGIN_MOBILE_REQUEST,
        true,
        request->deviceContextId,
        &request->context,
        nowMs,
        CLIMATE_PHASE_MANUAL_FAN_PENDING,
        outResult);
}

ClimateManager_Status_t ClimateManager_Process(
    uint32_t nowMs,
    ClimateManager_Output_t *outResult)
{
    const DomainPolicyConfig_t *policy;
    ResultManager_Output_t resultOut;
    ClimateUserSettings_t settings;

    if (outResult == NULL)
        return CLIMATE_MANAGER_STATUS_INVALID_ARGUMENT;

    if (!g_climate.initialized)
        return CLIMATE_MANAGER_STATUS_NOT_INITIALIZED;

    Climate_ClearOutput(outResult);

    if (g_climate.snapshot.hasPendingCommand)
    {
        policy = DomainPolicyConfig_Get();
        if (policy == NULL)
            return CLIMATE_MANAGER_STATUS_DEPENDENCY_NOT_READY;

        if ((uint32_t)(nowMs -
            g_climate.snapshot.commandIssuedAtMs) >=
            policy->climateResultWaitMs.value)
        {
            (void)ResultManager_MarkCommandUnconfirmed(
                g_climate.snapshot.pendingCommandId,
                nowMs,
                &resultOut);

            g_climate.snapshot.hasPendingCommand = false;

            if (g_climate.snapshot.pendingManualRequest)
            {
                (void)SettingsManager_CancelPendingManualFan(
                    g_climate.snapshot.pendingManualDevice,
                    &g_climate.snapshot.pendingManualRequestContext,
                    g_climate.snapshot.pendingManualSessionGeneration);
                g_climate.snapshot.pendingManualRequest = false;
            }

            g_climate.snapshot.phase = CLIMATE_PHASE_IDLE;
            outResult->decision = CLIMATE_DECISION_RESULT_TIMEOUT;
            return CLIMATE_MANAGER_STATUS_OK;
        }

        outResult->decision = CLIMATE_DECISION_COMMAND_PENDING;
        return CLIMATE_MANAGER_STATUS_OK;
    }

    if (SettingsManager_GetClimate(&settings) != SETTINGS_STATUS_OK)
        return CLIMATE_MANAGER_STATUS_DEPENDENCY_NOT_READY;

    if ((settings.activeMode == SETTINGS_CLIMATE_MODE_AUTO) &&
        settings.autoClimateEnabled)
        return Climate_StartAuto(nowMs, outResult);

    return CLIMATE_MANAGER_STATUS_OK;
}

ClimateManager_Status_t ClimateManager_OnBcmClimateState(
    const ClimateState_t *state,
    uint32_t nowMs,
    ClimateManager_Output_t *outResult)
{
    ResultManager_Output_t resultOut;
    ClimateUserSettings_t settings;
    VehicleStateSnapshot_t vehicle;
    int32_t rawDelta;
    int32_t deltaMilliC;
    ThermalDirection_t thermal;
    uint8_t percent;
    ResultManager_Status_t rs;

    if ((state == NULL) || (outResult == NULL))
        return CLIMATE_MANAGER_STATUS_INVALID_ARGUMENT;

    if (!g_climate.initialized)
        return CLIMATE_MANAGER_STATUS_NOT_INITIALIZED;

    Climate_ClearOutput(outResult);

    if (!g_climate.snapshot.hasPendingCommand)
        return CLIMATE_MANAGER_STATUS_OK;

    if (state->meta.quality != DATA_QUALITY_OK)
        return CLIMATE_MANAGER_STATUS_OK;

    if (g_climate.snapshot.phase == CLIMATE_PHASE_MANUAL_FAN_PENDING)
    {
        if (!Climate_StateMatches(
                state,
                g_climate.snapshot.desiredFan,
                THERMAL_DIRECTION_IDLE,
                0U,
                true))
            return CLIMATE_MANAGER_STATUS_OK;

        rs = ResultManager_ConfirmObservedState(
            g_climate.snapshot.pendingCommandId,
            REQUEST_RESULT_DONE,
            RESULT_REASON_NONE,
            nowMs,
            &resultOut);

        if (rs != RESULT_MANAGER_STATUS_OK)
            return CLIMATE_MANAGER_STATUS_COMMAND_ERROR;

        if (g_climate.snapshot.pendingManualRequest)
        {
            if (SettingsManager_CommitManualFanSelection(
                    g_climate.snapshot.pendingManualDevice,
                    &g_climate.snapshot.pendingManualRequestContext,
                    g_climate.snapshot.pendingManualSessionGeneration,
                    nowMs) != SETTINGS_STATUS_OK)
                return CLIMATE_MANAGER_STATUS_COMMAND_ERROR;
        }

        g_climate.snapshot.pendingManualRequest = false;
        g_climate.snapshot.hasPendingCommand = false;
        g_climate.snapshot.selectedMode = SETTINGS_CLIMATE_MODE_MANUAL;
        g_climate.snapshot.phase = CLIMATE_PHASE_IDLE;

        outResult->decision = CLIMATE_DECISION_SETTING_CONFIRMED;
        return CLIMATE_MANAGER_STATUS_OK;
    }

    if (g_climate.snapshot.phase == CLIMATE_PHASE_AUTO_FAN_PENDING)
    {
        /*
         * Fan must be both commanded and measured at target, and heat removal normal,
         * before thermal output can start.
         */
        if ((state->commandedFanLevel !=
             g_climate.snapshot.desiredFan) ||
            (state->measuredFanLevel !=
             g_climate.snapshot.desiredFan))
        {
            outResult->decision = CLIMATE_DECISION_FAN_NOT_CONFIRMED;
            return CLIMATE_MANAGER_STATUS_OK;
        }

        if (state->heatRemovalState != HEAT_REMOVAL_STATE_NORMAL)
        {
            outResult->decision =
                CLIMATE_DECISION_HEAT_REMOVAL_NOT_NORMAL;
            return CLIMATE_MANAGER_STATUS_OK;
        }

        rs = ResultManager_ConfirmObservedState(
            g_climate.snapshot.pendingCommandId,
            REQUEST_RESULT_DONE,
            RESULT_REASON_NONE,
            nowMs,
            &resultOut);

        if (rs != RESULT_MANAGER_STATUS_OK)
            return CLIMATE_MANAGER_STATUS_COMMAND_ERROR;

        g_climate.snapshot.hasPendingCommand = false;

        if (SettingsManager_GetClimate(&settings) != SETTINGS_STATUS_OK ||
            VehicleStateManager_GetSnapshot(&vehicle) != VSM_STATUS_OK)
            return CLIMATE_MANAGER_STATUS_DEPENDENCY_NOT_READY;

        if ((!settings.autoClimateEnabled) ||
            (settings.activeMode != SETTINGS_CLIMATE_MODE_AUTO))
        {
            g_climate.snapshot.phase = CLIMATE_PHASE_IDLE;
            return CLIMATE_MANAGER_STATUS_OK;
        }

        if (!Climate_TemperatureTrusted(&vehicle.vehicle.environment.cabin))
        {
            g_climate.snapshot.phase = CLIMATE_PHASE_IDLE;
            outResult->decision =
                CLIMATE_DECISION_REQUIRED_TEMPERATURE_UNTRUSTED;
            return CLIMATE_MANAGER_STATUS_OK;
        }

        rawDelta =
            (int32_t)vehicle.vehicle.environment.cabin.cabinTemperature -
            (int32_t)settings.targetTemperature;
        deltaMilliC = Climate_ToMilliCFromRawDelta(rawDelta);

        Climate_SelectThermal(
            deltaMilliC,
            &thermal,
            &percent);

        if ((thermal == THERMAL_DIRECTION_IDLE) ||
            (percent == 0U))
        {
            g_climate.snapshot.phase = CLIMATE_PHASE_AUTO_ACTIVE;
            return CLIMATE_MANAGER_STATUS_OK;
        }

        outResult->decision =
            CLIMATE_DECISION_AUTO_THERMAL_COMMAND_DISPATCHED;

        return Climate_SendCommand(
            g_climate.snapshot.autoFanMemory,
            thermal,
            percent,
            COMMAND_ORIGIN_CLIMATE_POLICY,
            false, 0U, NULL,
            nowMs,
            CLIMATE_PHASE_AUTO_THERMAL_PENDING,
            outResult);
    }

    if (g_climate.snapshot.phase == CLIMATE_PHASE_AUTO_THERMAL_PENDING)
    {
        if (!Climate_StateMatches(
                state,
                g_climate.snapshot.desiredFan,
                g_climate.snapshot.desiredThermalDirection,
                g_climate.snapshot.desiredThermalPercent,
                true))
            return CLIMATE_MANAGER_STATUS_OK;

        rs = ResultManager_ConfirmObservedState(
            g_climate.snapshot.pendingCommandId,
            REQUEST_RESULT_DONE,
            RESULT_REASON_NONE,
            nowMs,
            &resultOut);

        if (rs != RESULT_MANAGER_STATUS_OK)
            return CLIMATE_MANAGER_STATUS_COMMAND_ERROR;

        g_climate.snapshot.hasPendingCommand = false;
        g_climate.snapshot.phase = CLIMATE_PHASE_AUTO_ACTIVE;
        return CLIMATE_MANAGER_STATUS_OK;
    }

    return CLIMATE_MANAGER_STATUS_OK;
}

ClimateManager_Status_t ClimateManager_GetSnapshot(
    ClimateManager_Snapshot_t *outSnapshot)
{
    if (outSnapshot == NULL)
        return CLIMATE_MANAGER_STATUS_INVALID_ARGUMENT;

    if (!g_climate.initialized)
        return CLIMATE_MANAGER_STATUS_NOT_INITIALIZED;

    *outSnapshot = g_climate.snapshot;
    return CLIMATE_MANAGER_STATUS_OK;
}
