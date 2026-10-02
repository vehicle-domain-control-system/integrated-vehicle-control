/**
 * @file InteriorLightManager.c
 */

#include "InteriorLightManager.h"

#include <stddef.h>
#include <string.h>

#include "DomainLifecycleManager.h"
#include "Domain_PolicyConfig.h"
#include "VehicleStateManager.h"

typedef struct
{
    bool initialized;
    InteriorLightManager_Config_t config;
    InteriorLightManager_Snapshot_t snapshot;
} InteriorLightManager_Context_t;

static InteriorLightManager_Context_t g_light;

static void Ilm_ClearOutput(
    InteriorLightManager_Output_t *out)
{
    (void)memset(out, 0, sizeof(*out));
    out->decision = INTERIOR_LIGHT_DECISION_NONE;
    out->txStatus = DOMAIN_IF_NOT_AVAILABLE;
    out->permission.decision = PERMISSION_DECISION_DENY;
}

static RgbColor_t Ilm_Color(
    uint8_t r,
    uint8_t g,
    uint8_t b)
{
    RgbColor_t color;
    color.red = r;
    color.green = g;
    color.blue = b;
    return color;
}

static RgbColor_t Ilm_DefaultColor(
    InteriorLightType_t type)
{
    switch (type)
    {
        case INTERIOR_LIGHT_TYPE_GOODBYE:
            return Ilm_Color(0U, 70U, 70U);

        case INTERIOR_LIGHT_TYPE_WARNING:
            return Ilm_Color(100U, 70U, 0U);

        case INTERIOR_LIGHT_TYPE_FAULT:
            return Ilm_Color(100U, 0U, 0U);

        case INTERIOR_LIGHT_TYPE_NORMAL:
        default:
            return Ilm_Color(100U, 100U, 100U);
    }
}

static bool Ilm_ColorEqual(
    RgbColor_t a,
    RgbColor_t b)
{
    return ((a.red == b.red) &&
            (a.green == b.green) &&
            (a.blue == b.blue));
}

static bool Ilm_TargetEqual(
    const InteriorLightTarget_t *a,
    const InteriorLightTarget_t *b)
{
    return ((a != NULL) &&
            (b != NULL) &&
            (a->type == b->type) &&
            (a->levelPercent == b->levelPercent) &&
            Ilm_ColorEqual(a->color, b->color));
}

static bool Ilm_IlluminanceTrusted(
    const CisCabinEnvironmentState_t *cabin)
{
    return ((cabin != NULL) &&
            (cabin->illuminanceQuality.validity ==
             VALUE_VALIDITY_VALID) &&
            (cabin->illuminanceQuality.meta.quality ==
             DATA_QUALITY_OK));
}

static uint32_t Ilm_ToLux(
    uint16_t raw)
{
    return ((uint32_t)raw /
            (uint32_t)g_light.config.illuminanceUnitsPerLux);
}

static uint8_t Ilm_InitialAutoBrightness(
    uint32_t lux)
{
    if (lux >= 3000U)
        return 100U;
    if (lux >= 300U)
        return 75U;
    if (lux >= 30U)
        return 50U;
    return 25U;
}

static uint8_t Ilm_AutoBrightnessWithHysteresis(
    uint8_t current,
    uint32_t lux)
{
    uint8_t level = current;
    bool changed = true;

    while (changed)
    {
        changed = false;

        switch (level)
        {
            case 25U:
                if (lux >= 39U) /* 30 * 1.3 */
                {
                    level = 50U;
                    changed = true;
                }
                break;

            case 50U:
                if (lux >= 390U) /* 300 * 1.3 */
                {
                    level = 75U;
                    changed = true;
                }
                else if (lux < 21U) /* 30 * 0.7 */
                {
                    level = 25U;
                    changed = true;
                }
                break;

            case 75U:
                if (lux >= 3900U) /* 3000 * 1.3 */
                {
                    level = 100U;
                    changed = true;
                }
                else if (lux < 210U) /* 300 * 0.7 */
                {
                    level = 50U;
                    changed = true;
                }
                break;

            case 100U:
                if (lux < 2100U) /* 3000 * 0.7 */
                {
                    level = 75U;
                    changed = true;
                }
                break;

            default:
                level = Ilm_InitialAutoBrightness(lux);
                changed = false;
                break;
        }
    }

    return level;
}

static InteriorLightManager_Status_t Ilm_EvaluatePermission(
    bool requireIlluminance,
    DataQuality_t illuminanceQuality,
    PermissionResult_t *out)
{
    PermissionEvaluation_t e;
    DomainLifecycleSnapshot_t life;

    (void)memset(&e, 0, sizeof(e));

    e.requirements =
        PERMISSION_REQUIRE_DOMAIN_OPERATIONAL |
        PERMISSION_REQUIRE_FUNCTION_SUPPORTED |
        PERMISSION_REQUIRE_FUNCTION_AVAILABLE;

    if (requireIlluminance)
        e.requirements |= PERMISSION_REQUIRE_INPUT_QUALITY;

    e.facts.lifecycleState = DOMAIN_LIFECYCLE_STARTUP;
    if (DomainLifecycle_GetSnapshot(&life) == DLM_STATUS_OK)
        e.facts.lifecycleState = life.state;

    e.facts.functionSupported = g_light.config.supported;
    e.facts.functionAvailability =
        g_light.snapshot.availability;
    e.facts.degradedPolicy =
        g_light.config.degradedPolicy;
    e.facts.requiredInputQuality = illuminanceQuality;

    if (PermissionManager_Evaluate(&e, out) !=
        PERMISSION_STATUS_OK)
        return INTERIOR_LIGHT_MANAGER_STATUS_DEPENDENCY_NOT_READY;

    return INTERIOR_LIGHT_MANAGER_STATUS_OK;
}

static InteriorLightManager_Status_t Ilm_SelectTarget(
    InteriorLightTarget_t *outTarget,
    bool *outRequireIlluminance,
    DataQuality_t *outIlluminanceQuality,
    InteriorLightManager_Decision_t *outDecision)
{
    InteriorLightUserSettings_t settings;
    VehicleStateSnapshot_t vehicle;
    uint32_t lux;

    if ((outTarget == NULL) ||
        (outRequireIlluminance == NULL) ||
        (outIlluminanceQuality == NULL) ||
        (outDecision == NULL))
        return INTERIOR_LIGHT_MANAGER_STATUS_INVALID_ARGUMENT;

    *outRequireIlluminance = false;
    *outIlluminanceQuality = DATA_QUALITY_OK;
    *outDecision = INTERIOR_LIGHT_DECISION_NONE;

    /*
     * Re-evaluate current active set every time.
     * Higher priority indications do not depend on NORMAL user OFF.
     */
    if (g_light.snapshot.faultActive)
    {
        outTarget->type = INTERIOR_LIGHT_TYPE_FAULT;
        outTarget->levelPercent = 100U;
        outTarget->color =
            Ilm_DefaultColor(INTERIOR_LIGHT_TYPE_FAULT);
        return INTERIOR_LIGHT_MANAGER_STATUS_OK;
    }

    if (g_light.snapshot.warningActive)
    {
        outTarget->type = INTERIOR_LIGHT_TYPE_WARNING;
        outTarget->levelPercent = 100U;
        outTarget->color =
            Ilm_DefaultColor(INTERIOR_LIGHT_TYPE_WARNING);
        return INTERIOR_LIGHT_MANAGER_STATUS_OK;
    }

    if (g_light.snapshot.goodbyeActive)
    {
        outTarget->type = INTERIOR_LIGHT_TYPE_GOODBYE;
        outTarget->levelPercent = 100U;
        outTarget->color =
            Ilm_DefaultColor(INTERIOR_LIGHT_TYPE_GOODBYE);
        return INTERIOR_LIGHT_MANAGER_STATUS_OK;
    }

    if (SettingsManager_GetInteriorLight(&settings) !=
        SETTINGS_STATUS_OK)
        return INTERIOR_LIGHT_MANAGER_STATUS_DEPENDENCY_NOT_READY;

    outTarget->type = INTERIOR_LIGHT_TYPE_NORMAL;

    /*
     * No explicit user enable setting is treated as NORMAL not requested.
     * This avoids inventing a default ON policy absent from SysRS.
     */
    if ((!settings.hasUserEnableSetting) ||
        (!settings.userEnabled))
    {
        outTarget->levelPercent = 0U;
        outTarget->color = settings.hasColorSetting ?
            settings.color :
            Ilm_DefaultColor(INTERIOR_LIGHT_TYPE_NORMAL);

        *outDecision =
            INTERIOR_LIGHT_DECISION_NORMAL_DISABLED;
        return INTERIOR_LIGHT_MANAGER_STATUS_OK;
    }

    outTarget->color = settings.hasColorSetting ?
        settings.color :
        Ilm_DefaultColor(INTERIOR_LIGHT_TYPE_NORMAL);

    if (settings.hasBrightnessSetting)
    {
        outTarget->levelPercent =
            settings.brightnessPercent;
        return INTERIOR_LIGHT_MANAGER_STATUS_OK;
    }

    /*
     * No user brightness -> automatic illuminance candidate.
     * Actual CIS illuminance scale is still TBD.
     */
    if (g_light.config.illuminanceUnitsPerLux == 0U)
    {
        outTarget->levelPercent = 0U;
        *outDecision =
            INTERIOR_LIGHT_DECISION_CONFIGURATION_REQUIRED;
        return INTERIOR_LIGHT_MANAGER_STATUS_CONFIGURATION_REQUIRED;
    }

    if (VehicleStateManager_GetSnapshot(&vehicle) != VSM_STATUS_OK)
        return INTERIOR_LIGHT_MANAGER_STATUS_DEPENDENCY_NOT_READY;

    *outRequireIlluminance = true;
    *outIlluminanceQuality =
        vehicle.vehicle.environment.cabin.
            illuminanceQuality.meta.quality;

    if (!Ilm_IlluminanceTrusted(
            &vehicle.vehicle.environment.cabin))
    {
        outTarget->levelPercent = 0U;
        *outDecision =
            INTERIOR_LIGHT_DECISION_ILLUMINANCE_UNTRUSTED;
        return INTERIOR_LIGHT_MANAGER_STATUS_OK;
    }

    lux = Ilm_ToLux(
        vehicle.vehicle.environment.cabin.cabinIlluminance);

    if (!g_light.snapshot.hasAutoBrightnessMemory)
    {
        g_light.snapshot.autoBrightnessPercent =
            Ilm_InitialAutoBrightness(lux);
        g_light.snapshot.hasAutoBrightnessMemory = true;
    }
    else
    {
        g_light.snapshot.autoBrightnessPercent =
            Ilm_AutoBrightnessWithHysteresis(
                g_light.snapshot.autoBrightnessPercent,
                lux);
    }

    outTarget->levelPercent =
        g_light.snapshot.autoBrightnessPercent;

    return INTERIOR_LIGHT_MANAGER_STATUS_OK;
}

static InteriorLightManager_Status_t Ilm_SupersedePending(
    uint32_t nowMs)
{
    if (!g_light.snapshot.hasPendingCommand)
        return INTERIOR_LIGHT_MANAGER_STATUS_OK;

    if (CommandManager_CancelByPolicy(
            g_light.snapshot.pendingCommandId,
            RESULT_REASON_OTHER,
            nowMs) != COMMAND_MANAGER_STATUS_OK)
        return INTERIOR_LIGHT_MANAGER_STATUS_COMMAND_ERROR;

    g_light.snapshot.hasPendingCommand = false;
    return INTERIOR_LIGHT_MANAGER_STATUS_OK;
}

static InteriorLightManager_Status_t Ilm_SendTarget(
    const InteriorLightTarget_t *target,
    uint32_t nowMs,
    InteriorLightManager_Output_t *out)
{
    CommandCreateRequest_t create;
    CommandCreateResult_t created;
    DomainIf_BcmInteriorLightCommand_t tx;
    DomainIf_Status_t txStatus;

    if (g_light.snapshot.hasPendingCommand)
    {
        if (Ilm_TargetEqual(
                target,
                &g_light.snapshot.lastIssuedTarget))
        {
            out->decision =
                INTERIOR_LIGHT_DECISION_COMMAND_PENDING;
            return INTERIOR_LIGHT_MANAGER_STATUS_OK;
        }

        if (Ilm_SupersedePending(nowMs) !=
            INTERIOR_LIGHT_MANAGER_STATUS_OK)
            return INTERIOR_LIGHT_MANAGER_STATUS_COMMAND_ERROR;
    }

    (void)memset(&create, 0, sizeof(create));
    (void)memset(&created, 0, sizeof(created));
    (void)memset(&tx, 0, sizeof(tx));

    create.target = COMMAND_TARGET_BCM;
    create.functionId = DOMAIN_FUNCTION_INTERIOR_LIGHT;
    create.type = COMMAND_TYPE_BCM_INTERIOR_LIGHT;
    create.origin = COMMAND_ORIGIN_INTERIOR_LIGHT_POLICY;
    create.sourceMeta.quality = DATA_QUALITY_OK;

    if (CommandManager_Create(
            &create,
            nowMs,
            &created) != COMMAND_MANAGER_STATUS_OK)
        return INTERIOR_LIGHT_MANAGER_STATUS_COMMAND_ERROR;

    out->commandCreated = true;
    out->commandId = created.commandId;

    tx.context = created.interfaceContext;
    tx.type = target->type;
    tx.levelPercent = target->levelPercent;
    tx.color = target->color;

    txStatus =
        DomainIf_TxBcmInteriorLightCommand(&tx);

    out->txStatus = txStatus;

    if (txStatus != DOMAIN_IF_OK)
    {
        (void)CommandManager_AbortBeforeDispatch(
            created.commandId,
            REQUEST_RESULT_FAILED,
            RESULT_REASON_OTHER,
            nowMs);

        out->decision =
            INTERIOR_LIGHT_DECISION_COMMAND_DISPATCH_FAILED;
        return INTERIOR_LIGHT_MANAGER_STATUS_OK;
    }

    g_light.snapshot.hasIssuedTarget = true;
    g_light.snapshot.lastIssuedTarget = *target;
    g_light.snapshot.hasPendingCommand = true;
    g_light.snapshot.pendingCommandId =
        created.commandId;
    g_light.snapshot.commandIssuedAtMs = nowMs;

    out->commandDispatched = true;
    out->decision =
        INTERIOR_LIGHT_DECISION_COMMAND_DISPATCHED;

    return INTERIOR_LIGHT_MANAGER_STATUS_OK;
}

void InteriorLightManager_LoadCurrentProjectDefaults(
    InteriorLightManager_Config_t *outConfig)
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
     * CIS §5.14 says actual illuminance measurement unit (lux etc.) is TBD.
     */
    outConfig->illuminanceUnitsPerLux = 0U;
}

InteriorLightManager_Status_t InteriorLightManager_Init(
    const InteriorLightManager_Config_t *config)
{
    InteriorLightManager_Config_t defaults;

    if ((!VehicleStateManager_IsInitialized()) ||
        (!SettingsManager_IsInitialized()) ||
        (!PermissionManager_IsInitialized()) ||
        (!CommandManager_IsInitialized()) ||
        (!ResultManager_IsInitialized()) ||
        (DomainPolicyConfig_Get() == NULL))
        return INTERIOR_LIGHT_MANAGER_STATUS_DEPENDENCY_NOT_READY;

    if (config == NULL)
    {
        InteriorLightManager_LoadCurrentProjectDefaults(
            &defaults);
        config = &defaults;
    }

    (void)memset(&g_light, 0, sizeof(g_light));
    g_light.config = *config;
    g_light.snapshot.availability =
        config->initialAvailability;
    g_light.snapshot.desiredTarget.type =
        INTERIOR_LIGHT_TYPE_NORMAL;
    g_light.snapshot.desiredTarget.levelPercent = 0U;
    g_light.snapshot.desiredTarget.color =
        Ilm_DefaultColor(INTERIOR_LIGHT_TYPE_NORMAL);

    g_light.initialized = true;
    return INTERIOR_LIGHT_MANAGER_STATUS_OK;
}

bool InteriorLightManager_IsInitialized(void)
{
    return g_light.initialized;
}

InteriorLightManager_Status_t InteriorLightManager_SetAvailability(
    FunctionAvailability_t availability)
{
    if (!g_light.initialized)
        return INTERIOR_LIGHT_MANAGER_STATUS_NOT_INITIALIZED;

    g_light.snapshot.availability = availability;
    return INTERIOR_LIGHT_MANAGER_STATUS_OK;
}

InteriorLightManager_Status_t InteriorLightManager_SetWarningActive(
    bool active,
    uint32_t nowMs,
    InteriorLightManager_Output_t *outResult)
{
    if (outResult == NULL)
        return INTERIOR_LIGHT_MANAGER_STATUS_INVALID_ARGUMENT;

    if (!g_light.initialized)
        return INTERIOR_LIGHT_MANAGER_STATUS_NOT_INITIALIZED;

    g_light.snapshot.warningActive = active;

    /*
     * A higher-priority indication that interrupts GOODBYE discards it.
     */
    if (active && g_light.snapshot.goodbyeActive)
        g_light.snapshot.goodbyeActive = false;

    return InteriorLightManager_Process(
        nowMs,
        outResult);
}

InteriorLightManager_Status_t InteriorLightManager_SetFaultActive(
    bool active,
    uint32_t nowMs,
    InteriorLightManager_Output_t *outResult)
{
    if (outResult == NULL)
        return INTERIOR_LIGHT_MANAGER_STATUS_INVALID_ARGUMENT;

    if (!g_light.initialized)
        return INTERIOR_LIGHT_MANAGER_STATUS_NOT_INITIALIZED;

    g_light.snapshot.faultActive = active;

    if (active && g_light.snapshot.goodbyeActive)
        g_light.snapshot.goodbyeActive = false;

    return InteriorLightManager_Process(
        nowMs,
        outResult);
}

InteriorLightManager_Status_t InteriorLightManager_SetGoodbyeActive(
    bool active,
    uint32_t nowMs,
    InteriorLightManager_Output_t *outResult)
{
    if (outResult == NULL)
        return INTERIOR_LIGHT_MANAGER_STATUS_INVALID_ARGUMENT;

    if (!g_light.initialized)
        return INTERIOR_LIGHT_MANAGER_STATUS_NOT_INITIALIZED;

    Ilm_ClearOutput(outResult);

    if (active &&
        (g_light.snapshot.warningActive ||
         g_light.snapshot.faultActive))
    {
        /*
         * Do not queue a temporary GOODBYE underneath a higher-priority winner.
         */
        g_light.snapshot.goodbyeActive = false;
        outResult->decision =
            INTERIOR_LIGHT_DECISION_GOODBYE_SUPPRESSED;
        return INTERIOR_LIGHT_MANAGER_STATUS_OK;
    }

    g_light.snapshot.goodbyeActive = active;

    return InteriorLightManager_Process(
        nowMs,
        outResult);
}

InteriorLightManager_Status_t InteriorLightManager_HandleMobileRequest(
    const DomainIf_MobileRequest_t *request,
    uint32_t sessionGeneration,
    uint32_t nowMs,
    InteriorLightManager_Output_t *outResult)
{
    SettingsManager_Status_t ss;
    SettingsApplyResult_t apply;

    if ((request == NULL) || (outResult == NULL))
        return INTERIOR_LIGHT_MANAGER_STATUS_INVALID_ARGUMENT;

    if (!g_light.initialized)
        return INTERIOR_LIGHT_MANAGER_STATUS_NOT_INITIALIZED;

    if ((request->type != MOBILE_REQUEST_INTERIOR_LIGHT_ENABLE) &&
        (request->type != MOBILE_REQUEST_INTERIOR_LIGHT_LEVEL) &&
        (request->type != MOBILE_REQUEST_INTERIOR_LIGHT_COLOR))
        return INTERIOR_LIGHT_MANAGER_STATUS_INVALID_ARGUMENT;

    Ilm_ClearOutput(outResult);

    ss = SettingsManager_ApplyMobileSettingRequest(
        request,
        sessionGeneration,
        nowMs,
        &apply);

    outResult->settingApply = apply;

    if (ss != SETTINGS_STATUS_OK)
        return INTERIOR_LIGHT_MANAGER_STATUS_INVALID_ARGUMENT;

    outResult->decision =
        INTERIOR_LIGHT_DECISION_SETTING_CONFIRMED;

    /*
     * Setting DONE and actual BCM application are separate.
     * Re-evaluate current winner and optionally issue a policy command.
     */
    return InteriorLightManager_Process(
        nowMs,
        outResult);
}

InteriorLightManager_Status_t InteriorLightManager_Process(
    uint32_t nowMs,
    InteriorLightManager_Output_t *outResult)
{
    InteriorLightTarget_t target;
    bool requireIlluminance;
    DataQuality_t illumQuality;
    InteriorLightManager_Decision_t selectDecision;
    PermissionResult_t permission;
    const DomainPolicyConfig_t *policy;
    ResultManager_Output_t unconfirmed;
    InteriorLightManager_Status_t selectStatus;

    if (outResult == NULL)
        return INTERIOR_LIGHT_MANAGER_STATUS_INVALID_ARGUMENT;

    if (!g_light.initialized)
        return INTERIOR_LIGHT_MANAGER_STATUS_NOT_INITIALIZED;

    Ilm_ClearOutput(outResult);

    /*
     * Pending command timeout: do not assume FAILED and do not auto-retry
     * the same target solely because the result timed out.
     */
    if (g_light.snapshot.hasPendingCommand)
    {
        policy = DomainPolicyConfig_Get();

        if (policy == NULL)
            return INTERIOR_LIGHT_MANAGER_STATUS_DEPENDENCY_NOT_READY;

        if ((uint32_t)(nowMs -
            g_light.snapshot.commandIssuedAtMs) >=
            policy->lightApplyResultWaitMs.value)
        {
            (void)ResultManager_MarkCommandUnconfirmed(
                g_light.snapshot.pendingCommandId,
                nowMs,
                &unconfirmed);

            g_light.snapshot.hasPendingCommand = false;
            outResult->decision =
                INTERIOR_LIGHT_DECISION_RESULT_TIMEOUT;

            /*
             * Continue selecting current target, but lastIssuedTarget remains.
             * Therefore identical target is not automatically re-executed.
             */
        }
    }

    selectStatus = Ilm_SelectTarget(
        &target,
        &requireIlluminance,
        &illumQuality,
        &selectDecision);

    outResult->selectedTarget = target;
    g_light.snapshot.desiredTarget = target;

    if (selectStatus ==
        INTERIOR_LIGHT_MANAGER_STATUS_CONFIGURATION_REQUIRED)
    {
        outResult->decision =
            INTERIOR_LIGHT_DECISION_CONFIGURATION_REQUIRED;
        return selectStatus;
    }

    if (selectStatus != INTERIOR_LIGHT_MANAGER_STATUS_OK)
        return selectStatus;

    if (Ilm_EvaluatePermission(
            requireIlluminance,
            illumQuality,
            &permission) != INTERIOR_LIGHT_MANAGER_STATUS_OK)
        return INTERIOR_LIGHT_MANAGER_STATUS_DEPENDENCY_NOT_READY;

    outResult->permission = permission;

    if (permission.decision != PERMISSION_DECISION_ALLOW)
    {
        outResult->decision =
            INTERIOR_LIGHT_DECISION_PERMISSION_DENIED;
        return INTERIOR_LIGHT_MANAGER_STATUS_OK;
    }

    if (g_light.snapshot.hasIssuedTarget &&
        Ilm_TargetEqual(
            &target,
            &g_light.snapshot.lastIssuedTarget) &&
        (!g_light.snapshot.hasPendingCommand))
    {
        outResult->decision =
            (selectDecision != INTERIOR_LIGHT_DECISION_NONE) ?
            selectDecision :
            INTERIOR_LIGHT_DECISION_TARGET_UNCHANGED;
        return INTERIOR_LIGHT_MANAGER_STATUS_OK;
    }

    if (g_light.snapshot.hasPendingCommand &&
        Ilm_TargetEqual(
            &target,
            &g_light.snapshot.lastIssuedTarget))
    {
        outResult->decision =
            INTERIOR_LIGHT_DECISION_COMMAND_PENDING;
        return INTERIOR_LIGHT_MANAGER_STATUS_OK;
    }

    return Ilm_SendTarget(
        &target,
        nowMs,
        outResult);
}

InteriorLightManager_Status_t InteriorLightManager_OnBcmState(
    const InteriorLightState_t *state,
    uint32_t nowMs,
    InteriorLightManager_Output_t *outResult)
{
    ResultManager_Output_t resultOut;

    if ((state == NULL) || (outResult == NULL))
        return INTERIOR_LIGHT_MANAGER_STATUS_INVALID_ARGUMENT;

    if (!g_light.initialized)
        return INTERIOR_LIGHT_MANAGER_STATUS_NOT_INITIALIZED;

    Ilm_ClearOutput(outResult);

    if ((!g_light.snapshot.hasPendingCommand) ||
        (state->meta.quality != DATA_QUALITY_OK) ||
        (!state->commandApplied))
        return INTERIOR_LIGHT_MANAGER_STATUS_OK;

    if ((state->type !=
         g_light.snapshot.lastIssuedTarget.type) ||
        (state->levelPercent !=
         g_light.snapshot.lastIssuedTarget.levelPercent) ||
        (!Ilm_ColorEqual(
            state->color,
            g_light.snapshot.lastIssuedTarget.color)))
        return INTERIOR_LIGHT_MANAGER_STATUS_OK;

    if (ResultManager_ConfirmObservedState(
            g_light.snapshot.pendingCommandId,
            REQUEST_RESULT_DONE,
            RESULT_REASON_NONE,
            nowMs,
            &resultOut) != RESULT_MANAGER_STATUS_OK)
        return INTERIOR_LIGHT_MANAGER_STATUS_COMMAND_ERROR;

    g_light.snapshot.hasPendingCommand = false;
    g_light.snapshot.hasAppliedTarget = true;
    g_light.snapshot.lastAppliedTarget =
        g_light.snapshot.lastIssuedTarget;

    outResult->selectedTarget =
        g_light.snapshot.lastAppliedTarget;
    outResult->decision =
        INTERIOR_LIGHT_DECISION_TARGET_UNCHANGED;

    /*
     * `commandApplied` confirms logical application only.
     * physicalFeedbackSupported is intentionally not converted into
     * a physical ON/OFF claim here.
     */

    return INTERIOR_LIGHT_MANAGER_STATUS_OK;
}

InteriorLightManager_Status_t InteriorLightManager_GetSnapshot(
    InteriorLightManager_Snapshot_t *outSnapshot)
{
    if (outSnapshot == NULL)
        return INTERIOR_LIGHT_MANAGER_STATUS_INVALID_ARGUMENT;

    if (!g_light.initialized)
        return INTERIOR_LIGHT_MANAGER_STATUS_NOT_INITIALIZED;

    *outSnapshot = g_light.snapshot;
    return INTERIOR_LIGHT_MANAGER_STATUS_OK;
}
