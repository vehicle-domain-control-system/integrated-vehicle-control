#include "Domain_Interface_v0.5_C2_target.h"
#include "domain_interface_core_internal.h"

#include <string.h>

/*
 * Lifecycle and registration are intentionally single-owner operations.
 * The caller must serialize Init/registration with all future Rx/Tx calls.
 * ISR use, concurrent registration, and registration while communication is
 * active are outside this Core's contract.
 */
typedef struct
{
    bool initialized;
    /* A bound table does not imply that every callback member is non-NULL. */
    bool hasRxObserver;
    bool hasTxPort;
    DomainIf_RxObserver_t rxObserver;
    DomainIf_TxPort_t txPort;
} DomainIf_CoreContext_t;

static DomainIf_CoreContext_t g_domainIfCore;

static bool DomainIf_IsDataQualityValid(DataQuality_t value)
{
    return (value == DATA_QUALITY_OK) ||
           (value == DATA_QUALITY_STALE) ||
           (value == DATA_QUALITY_INVALID) ||
           (value == DATA_QUALITY_NO_DATA);
}

static bool DomainIf_IsQualityReasonValid(QualityReason_t value)
{
    switch (value)
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

static bool DomainIf_IsAvailabilityValid(FunctionAvailability_t value)
{
    return (value == FUNCTION_AVAILABILITY_AVAILABLE) ||
           (value == FUNCTION_AVAILABILITY_DEGRADED) ||
           (value == FUNCTION_AVAILABILITY_UNAVAILABLE) ||
           (value == FUNCTION_AVAILABILITY_UNSUPPORTED);
}

static bool DomainIf_IsSignalMetaValid(const SignalMeta_t *meta)
{
    return DomainIf_IsDataQualityValid(meta->quality);
}

static bool DomainIf_IsValueQualityValid(const ValueQuality_t *quality)
{
    return ((quality->validity == VALUE_VALIDITY_INVALID) ||
            (quality->validity == VALUE_VALIDITY_VALID)) &&
           DomainIf_IsQualityReasonValid(quality->reason) &&
           DomainIf_IsSignalMetaValid(&quality->meta);
}

static bool DomainIf_IsResultValid(RequestResult_t value)
{
    switch (value)
    {
        case REQUEST_RESULT_ACCEPTED:
        case REQUEST_RESULT_IN_PROGRESS:
        case REQUEST_RESULT_DONE:
        case REQUEST_RESULT_REJECTED:
        case REQUEST_RESULT_CANCELLED:
        case REQUEST_RESULT_FAILED:
            return true;
        default:
            return false;
    }
}

static bool DomainIf_IsResultReasonValid(ResultReason_t value)
{
    switch (value)
    {
        case RESULT_REASON_NONE:
        case RESULT_REASON_LOCAL_OVERRIDE:
        case RESULT_REASON_STOP_REQUESTED:
        case RESULT_REASON_SUPERSEDED:
        case RESULT_REASON_ANTIPINCH:
        case RESULT_REASON_NOT_ALLOWED:
        case RESULT_REASON_INVALID_COMMAND:
        case RESULT_REASON_STATE_UNTRUSTED:
        case RESULT_REASON_NO_FEEDBACK:
        case RESULT_REASON_DRIVE_LIMIT_EXCEEDED:
        case RESULT_REASON_MOTOR_FAULT:
        case RESULT_REASON_SENSOR_FAULT:
        case RESULT_REASON_TIMEOUT:
        case RESULT_REASON_OTHER:
        case RESULT_REASON_NOT_REGISTERED:
        case RESULT_REASON_LINK_LOST:
        case RESULT_REASON_SESSION_INVALID:
        case RESULT_REASON_DOOR_OPEN:
        case RESULT_REASON_ALREADY_AT_TARGET:
        case RESULT_REASON_FAULT:
        case RESULT_REASON_STALE:
        case RESULT_REASON_NO_DATA:
        case RESULT_REASON_NOT_READY:
        case RESULT_REASON_OUT_OF_RANGE:
        case RESULT_REASON_SOURCE_FAILURE:
        case RESULT_REASON_UNKNOWN_REQUEST:
        case RESULT_REASON_EXPIRED:
        case RESULT_REASON_ID_CONFLICT:
        case RESULT_REASON_POWER_NOT_ALLOWED:
        case RESULT_REASON_OVERHEAT:
        case RESULT_REASON_FAN_MISMATCH:
            return true;
        default:
            return false;
    }
}

static bool DomainIf_IsConfirmationValid(ResultConfirmation_t value)
{
    return (value == RESULT_CONFIRMATION_UNCONFIRMED) ||
           (value == RESULT_CONFIRMATION_CONFIRMED);
}

static bool DomainIf_IsCommandOriginValid(CommandOrigin_t value)
{
    switch (value)
    {
        case COMMAND_ORIGIN_MOBILE_REQUEST:
        case COMMAND_ORIGIN_DIGITAL_KEY:
        case COMMAND_ORIGIN_CLIMATE_POLICY:
        case COMMAND_ORIGIN_INTERIOR_LIGHT_POLICY:
        case COMMAND_ORIGIN_AUTO_VENTILATION:
        case COMMAND_ORIGIN_SYSTEM:
            return true;
        default:
            return false;
    }
}

static bool DomainIf_IsCommandContextValid(
    const DomainIf_CommandContext_t *context)
{
    bool requestPresent;
    bool requestCleared;

    requestPresent = (context->requestDeviceContextId != 0U) &&
                     (context->requestContext.sessionId != 0U) &&
                     (context->requestContext.requestId != 0U);
    requestCleared = (context->requestDeviceContextId == 0U) &&
                     (context->requestContext.sessionId == 0U) &&
                     (context->requestContext.requestId == 0U);

    return (context->commandId != 0U) &&
           DomainIf_IsCommandOriginValid(context->origin) &&
           DomainIf_IsSignalMetaValid(&context->meta) &&
           ((context->hasRequestContext && requestPresent) ||
            (!context->hasRequestContext && requestCleared)) &&
           ((context->origin != COMMAND_ORIGIN_MOBILE_REQUEST) ||
            context->hasRequestContext);
}

static bool DomainIf_IsDoorTargetValid(DoorLockTarget_t value)
{
    return (value == DOOR_TARGET_LOCK) || (value == DOOR_TARGET_UNLOCK);
}

static bool DomainIf_IsFanLevelValid(FanLevel_t value, bool allowUnknown)
{
    switch (value)
    {
        case FAN_LEVEL_OFF:
        case FAN_LEVEL_LOW:
        case FAN_LEVEL_MEDIUM:
        case FAN_LEVEL_HIGH:
            return true;
        case FAN_LEVEL_UNKNOWN:
            return allowUnknown;
        default:
            return false;
    }
}

static bool DomainIf_IsThermalDirectionValid(ThermalDirection_t value)
{
    return (value == THERMAL_DIRECTION_IDLE) ||
           (value == THERMAL_DIRECTION_COOL) ||
           (value == THERMAL_DIRECTION_HEAT);
}

static bool DomainIf_IsInteriorLightTypeValid(InteriorLightType_t value)
{
    return (value == INTERIOR_LIGHT_TYPE_NORMAL) ||
           (value == INTERIOR_LIGHT_TYPE_GOODBYE) ||
           (value == INTERIOR_LIGHT_TYPE_WARNING) ||
           (value == INTERIOR_LIGHT_TYPE_FAULT);
}

static bool DomainIf_IsDoorCommandValid(
    const DomainIf_BcmDoorCommand_t *command)
{
    return DomainIf_IsCommandContextValid(&command->context) &&
           DomainIf_IsDoorTargetValid(command->target);
}

static bool DomainIf_IsClimateCommandValid(
    const DomainIf_BcmClimateCommand_t *command)
{
    return DomainIf_IsCommandContextValid(&command->context) &&
           DomainIf_IsFanLevelValid(command->fanTarget, false) &&
           DomainIf_IsThermalDirectionValid(command->thermalDirection) &&
           (command->thermalTargetLevelPercent <= 100U);
}

static bool DomainIf_IsInteriorLightCommandValid(
    const DomainIf_BcmInteriorLightCommand_t *command)
{
    return DomainIf_IsCommandContextValid(&command->context) &&
           DomainIf_IsInteriorLightTypeValid(command->type) &&
           (command->levelPercent <= 100U);
}

static bool DomainIf_IsCisPowerPermissionValid(
    const DomainIf_CisPowerPermission_t *permission)
{
    return DomainIf_IsDataQualityValid(permission->quality) &&
           DomainIf_IsSignalMetaValid(&permission->meta) &&
           (permission->quality == permission->meta.quality);
}

static bool DomainIf_IsWindowCommandValid(
    const DomainIf_WindowCommand_t *command)
{
    bool commandValid;

    switch (command->command)
    {
        case WINDOW_COMMAND_OPEN:
        case WINDOW_COMMAND_CLOSE:
        case WINDOW_COMMAND_STOP:
        case WINDOW_COMMAND_VENT:
        case WINDOW_COMMAND_MOVE_TO_POSITION:
            commandValid = true;
            break;
        default:
            commandValid = false;
            break;
    }
    return DomainIf_IsCommandContextValid(&command->context) &&
           commandValid && (command->targetPositionPercent <= 100U);
}

static bool DomainIf_IsVssEventValid(const DomainIf_VssOneShotEvent_t *event)
{
    bool eventValid;

    switch (event->event)
    {
        case VSS_EVENT_VEHICLE_WELCOME:
        case VSS_EVENT_VEHICLE_GOODBYE:
        case VSS_EVENT_DOOR_LOCK_COMPLETE:
        case VSS_EVENT_DOOR_UNLOCK_COMPLETE:
        case VSS_EVENT_DOOR_LOCK_ERROR:
            eventValid = true;
            break;
        default:
            eventValid = false;
            break;
    }
    return eventValid && DomainIf_IsSignalMetaValid(&event->meta) &&
           DomainIf_IsAvailabilityValid(event->availability) &&
           ((event->hasRelatedCommand &&
             (event->relatedCommandId != 0U)) ||
            (!event->hasRelatedCommand &&
             (event->relatedCommandId == 0U)));
}

static bool DomainIf_IsWarningEvidenceValid(
    const DomainIf_WarningInputEvidence_t *evidence)
{
    return DomainIf_IsDataQualityValid(evidence->quality) &&
           DomainIf_IsQualityReasonValid(evidence->reason) &&
           DomainIf_IsAvailabilityValid(evidence->availability);
}

static bool DomainIf_IsVssWarningStateValid(
    const DomainIf_VssWarningState_t *state)
{
    bool antiPinchValid;
    bool occupantValid;
    bool rearValid;
    bool activationValid;

    antiPinchValid = (state->windowAntiPinchState == ANTIPINCH_STATE_CLEAR) ||
                     (state->windowAntiPinchState == ANTIPINCH_STATE_ACTIVE);
    occupantValid =
        (state->occupantHazardState == OCCUPANT_HAZARD_CLEAR) ||
        (state->occupantHazardState == OCCUPANT_HAZARD_ACTIVE) ||
        (state->occupantHazardState == OCCUPANT_HAZARD_UNCONFIRMED);
    rearValid = (state->rearObstacleState == REAR_OBSTACLE_STATE_CLEAR) ||
                (state->rearObstacleState == REAR_OBSTACLE_STATE_CAUTION) ||
                (state->rearObstacleState == REAR_OBSTACLE_STATE_EMERGENCY);
    activationValid =
        (state->rearDetectionActivation == REAR_DETECTION_DISABLED) ||
        (state->rearDetectionActivation == REAR_DETECTION_ACTIVE);

    return antiPinchValid && occupantValid && rearValid && activationValid &&
           DomainIf_IsWarningEvidenceValid(&state->antiPinchEvidence) &&
           DomainIf_IsWarningEvidenceValid(&state->occupantEvidence) &&
           DomainIf_IsWarningEvidenceValid(&state->rearEvidence) &&
           DomainIf_IsSignalMetaValid(&state->rearActivationMeta) &&
           DomainIf_IsQualityReasonValid(state->rearActivationReason);
}

static bool DomainIf_IsMobileRequestResultValid(
    const DomainIf_MobileRequestResult_t *result)
{
    bool targetValid;
    bool resultFieldsValid;
    bool commandValid;

    targetValid = (result->target == MOBILE_RESULT_TARGET_DOMAIN_SETTING) ||
                  (result->target == MOBILE_RESULT_TARGET_BCM) ||
                  (result->target == MOBILE_RESULT_TARGET_WINDOW);
    resultFieldsValid = !result->hasVehicleResult ||
        (DomainIf_IsResultValid(result->resultInfo.result) &&
         DomainIf_IsResultReasonValid(result->resultInfo.reason) &&
         DomainIf_IsConfirmationValid(result->resultInfo.confirmation));
    if (!result->hasVehicleResult)
    {
        resultFieldsValid = resultFieldsValid &&
            (result->resultInfo.confirmation ==
             RESULT_CONFIRMATION_UNCONFIRMED);
    }
    commandValid =
        ((result->target == MOBILE_RESULT_TARGET_DOMAIN_SETTING) &&
         (result->commandId == 0U)) ||
        (((result->target == MOBILE_RESULT_TARGET_BCM) ||
          (result->target == MOBILE_RESULT_TARGET_WINDOW)) &&
         (result->commandId != 0U));

    return (result->deviceContextId != 0U) &&
           (result->resultInfo.context.sessionId != 0U) &&
           (result->resultInfo.context.requestId != 0U) &&
           targetValid && resultFieldsValid && commandValid &&
           DomainIf_IsCommandOriginValid(result->origin);
}

static bool DomainIf_IsDoorStateValid(const DoorState_t *state)
{
    bool lockValid;
    bool openValid;
    bool compositeValid;

    lockValid = (state->lockState == DOOR_LOCK_STATE_UNKNOWN) ||
                (state->lockState == DOOR_LOCK_STATE_LOCKED) ||
                (state->lockState == DOOR_LOCK_STATE_UNLOCKED);
    openValid = (state->openState == DOOR_OPEN_STATE_UNKNOWN) ||
                (state->openState == DOOR_OPEN_STATE_CLOSED) ||
                (state->openState == DOOR_OPEN_STATE_OPEN);
    compositeValid =
        (state->compositeState == DOOR_COMPOSITE_STATE_NORMAL) ||
        (state->compositeState == DOOR_COMPOSITE_STATE_INCONSISTENT) ||
        (state->compositeState == DOOR_COMPOSITE_STATE_UNTRUSTED);
    return lockValid && openValid && compositeValid &&
           DomainIf_IsDataQualityValid(state->lockQuality) &&
           DomainIf_IsDataQualityValid(state->openQuality) &&
           DomainIf_IsDataQualityValid(state->compositeQuality) &&
           DomainIf_IsSignalMetaValid(&state->meta);
}

static bool DomainIf_IsClimateStateValid(const ClimateState_t *state)
{
    bool heatRemovalValid;

    heatRemovalValid =
        (state->heatRemovalState == HEAT_REMOVAL_STATE_NORMAL) ||
        (state->heatRemovalState == HEAT_REMOVAL_STATE_BLOCKED_OVERHEAT) ||
        (state->heatRemovalState == HEAT_REMOVAL_STATE_UNMEASURABLE);
    return DomainIf_IsFanLevelValid(state->commandedFanLevel, true) &&
           DomainIf_IsFanLevelValid(state->measuredFanLevel, true) &&
           DomainIf_IsThermalDirectionValid(state->thermalDirection) &&
           (state->thermalOutputLevelPercent <= 100U) &&
           heatRemovalValid &&
           DomainIf_IsDataQualityValid(state->commandedFanQuality) &&
           DomainIf_IsDataQualityValid(state->measuredFanQuality) &&
           DomainIf_IsDataQualityValid(state->thermalDirectionQuality) &&
           DomainIf_IsDataQualityValid(state->thermalOutputQuality) &&
           DomainIf_IsDataQualityValid(state->heatRemovalQuality) &&
           DomainIf_IsSignalMetaValid(&state->meta);
}

static bool DomainIf_IsInteriorLightStateValid(
    const InteriorLightState_t *state)
{
    bool applyValid;

    applyValid = (state->applyState == INTERIOR_LIGHT_APPLY_UNKNOWN) ||
                 (state->applyState == INTERIOR_LIGHT_APPLY_APPLIED) ||
                 (state->applyState == INTERIOR_LIGHT_APPLY_FAILED);
    return DomainIf_IsInteriorLightTypeValid(state->type) &&
           (state->levelPercent <= 100U) && applyValid &&
           DomainIf_IsDataQualityValid(state->typeQuality) &&
           DomainIf_IsDataQualityValid(state->levelQuality) &&
           DomainIf_IsDataQualityValid(state->colorQuality) &&
           DomainIf_IsDataQualityValid(state->applyQuality) &&
           DomainIf_IsSignalMetaValid(&state->meta);
}

static bool DomainIf_IsCommonEcuStateValid(CommonEcuState_t value)
{
    return (value == ECU_STATE_INIT) || (value == ECU_STATE_READY) ||
           (value == ECU_STATE_DEGRADED) || (value == ECU_STATE_FAULT);
}

static bool DomainIf_IsFaultInfoValid(const FaultInfo_t *fault)
{
    if (fault->faultCode == 0U)
    {
        return true;
    }
    return ((fault->category == FAULT_CATEGORY_SENSOR) ||
            (fault->category == FAULT_CATEGORY_COMM) ||
            (fault->category == FAULT_CATEGORY_FUNCTION)) &&
           ((fault->state == FAULT_STATE_INACTIVE) ||
            (fault->state == FAULT_STATE_ACTIVE) ||
            (fault->state == FAULT_STATE_RECOVERING)) &&
           DomainIf_IsDataQualityValid(fault->quality);
}

static bool DomainIf_IsBcmStatusValid(
    const DomainIf_BcmStatusReport_t *state)
{
    return DomainIf_IsCommonEcuStateValid(state->ecuState) &&
           DomainIf_IsFaultInfoValid(&state->activeFault) &&
           DomainIf_IsSignalMetaValid(&state->meta);
}

static bool DomainIf_IsCisFunctionStatusValid(CisFunctionStatus_t value)
{
    return (value == CIS_FUNCTION_STATUS_NOT_READY) ||
           (value == CIS_FUNCTION_STATUS_READY) ||
           (value == CIS_FUNCTION_STATUS_ACTIVE) ||
           (value == CIS_FUNCTION_STATUS_FAULT) ||
           (value == CIS_FUNCTION_STATUS_RECOVERING);
}

static bool DomainIf_IsCisEnvironmentValid(
    const CisCabinEnvironmentState_t *state)
{
    return DomainIf_IsValueQualityValid(&state->temperatureQuality) &&
           DomainIf_IsValueQualityValid(&state->humidityQuality) &&
           DomainIf_IsValueQualityValid(&state->illuminanceQuality) &&
           ((state->humidityQuality.validity != VALUE_VALIDITY_VALID) ||
            (state->cabinHumidity <= 10000U));
}

bool DomainIfCore_IsCisOccupantValid(const CisOccupantState_t *state)
{
    return DomainIf_IsValueQualityValid(&state->presenceQuality) &&
           DomainIf_IsValueQualityValid(&state->countQuality) &&
           DomainIf_IsCisFunctionStatusValid(state->visionFunctionStatus) &&
           ((state->countQuality.validity != VALUE_VALIDITY_VALID) ||
            (state->occupantCount <= 5U));
}

static bool DomainIf_IsCisRearValid(const CisRearState_t *state)
{
    bool measurementValid;

    switch (state->measurementState)
    {
        case REAR_MEASUREMENT_VALID_DISTANCE:
        case REAR_MEASUREMENT_NO_OBJECT:
        case REAR_MEASUREMENT_UNAVAILABLE:
        case REAR_MEASUREMENT_FAULT:
        case REAR_MEASUREMENT_RECOVERING:
        case REAR_MEASUREMENT_INACTIVE:
            measurementValid = true;
            break;
        default:
            measurementValid = false;
            break;
    }
    return measurementValid &&
           DomainIf_IsValueQualityValid(&state->distanceQuality);
}

static bool DomainIf_IsCisInterfaceStatusValid(
    DomainIf_CisInterfaceStatus_t value)
{
    return (value == CIS_INTERFACE_AVAILABLE) ||
           (value == CIS_INTERFACE_UNAVAILABLE) ||
           (value == CIS_INTERFACE_RECOVERING);
}

static bool DomainIf_IsCisStatusValid(
    const DomainIf_CisStatusReport_t *state)
{
    return ((state->ecuState == CIS_STATE_STARTUP) ||
            (state->ecuState == CIS_STATE_READY) ||
            (state->ecuState == CIS_STATE_ACTIVE) ||
            (state->ecuState == CIS_STATE_FAULT)) &&
           DomainIf_IsCisFunctionStatusValid(state->occupantFunctionStatus) &&
           DomainIf_IsCisFunctionStatusValid(state->temperatureFunctionStatus) &&
           DomainIf_IsCisFunctionStatusValid(state->humidityFunctionStatus) &&
           DomainIf_IsCisFunctionStatusValid(state->illuminanceFunctionStatus) &&
           DomainIf_IsCisFunctionStatusValid(state->rearFunctionStatus) &&
           DomainIf_IsCisInterfaceStatusValid(state->piUartStatus) &&
           DomainIf_IsCisInterfaceStatusValid(state->canInterfaceStatus) &&
           DomainIf_IsSignalMetaValid(&state->meta);
}

static bool DomainIf_IsVssStatusValid(const DomainIf_VssStateReport_t *state)
{
    bool serviceStateValid;
    bool serviceAvailabilityValid;

    serviceStateValid = (state->service.state == VSS_STATE_STARTUP) ||
                        (state->service.state == VSS_STATE_READY) ||
                        (state->service.state == VSS_STATE_PLAYING) ||
                        (state->service.state == VSS_STATE_FAULT);
    serviceAvailabilityValid =
        (state->service.availability == VSS_AVAILABILITY_FULL) ||
        (state->service.availability == VSS_AVAILABILITY_DEGRADED) ||
        (state->service.availability == VSS_AVAILABILITY_UNAVAILABLE);
    return serviceStateValid && serviceAvailabilityValid &&
           DomainIf_IsSignalMetaValid(&state->service.meta) &&
           DomainIf_IsDataQualityValid(state->faultQuality);
}

static bool DomainIf_IsWarningValid(const DomainIf_WarningInfo_t *warning)
{
    bool typeValid;

    switch (warning->type)
    {
        case DOMAIN_WARNING_WINDOW_ANTIPINCH:
        case DOMAIN_WARNING_OCCUPANT_HAZARD:
        case DOMAIN_WARNING_REAR_OBSTACLE:
        case DOMAIN_WARNING_DOOR_LEFT_OPEN:
        case DOMAIN_WARNING_BCM_FAULT:
        case DOMAIN_WARNING_CIS_FAULT:
        case DOMAIN_WARNING_WINDOW_FAULT:
        case DOMAIN_WARNING_VSS_FAULT:
            typeValid = true;
            break;
        case DOMAIN_WARNING_COUNT:
        default:
            typeValid = false;
            break;
    }
    return typeValid &&
           ((warning->severity == DOMAIN_WARNING_SEVERITY_INFO) ||
            (warning->severity == DOMAIN_WARNING_SEVERITY_CAUTION) ||
            (warning->severity == DOMAIN_WARNING_SEVERITY_EMERGENCY)) &&
           ((warning->state == DOMAIN_WARNING_STATE_CLEAR) ||
            (warning->state == DOMAIN_WARNING_STATE_ACTIVE) ||
            (warning->state == DOMAIN_WARNING_STATE_UNCONFIRMED)) &&
           DomainIf_IsDataQualityValid(warning->quality) &&
           DomainIf_IsQualityReasonValid(warning->qualityReason);
}

static bool DomainIf_IsFunctionAvailabilitySetValid(
    const DomainIf_FunctionAvailabilitySet_t *availability)
{
    static const DomainFunctionId_t expectedIds[DOMAIN_FUNCTION_COUNT] =
    {
        DOMAIN_FUNCTION_DOOR,
        DOMAIN_FUNCTION_CLIMATE,
        DOMAIN_FUNCTION_INTERIOR_LIGHT,
        DOMAIN_FUNCTION_DIGITAL_KEY,
        DOMAIN_FUNCTION_WINDOW,
        DOMAIN_FUNCTION_REAR_WARNING,
        DOMAIN_FUNCTION_OCCUPANT_WARNING,
        DOMAIN_FUNCTION_VSS,
        DOMAIN_FUNCTION_ENVIRONMENT
    };
    size_t index;

    for (index = 0U; index < (size_t)DOMAIN_FUNCTION_COUNT; ++index)
    {
        if ((availability->items[index].functionId != expectedIds[index]) ||
            !DomainIf_IsAvailabilityValid(
                availability->items[index].availability) ||
            !DomainIf_IsResultReasonValid(availability->items[index].reason))
        {
            return false;
        }
    }
    return DomainIf_IsSignalMetaValid(&availability->meta);
}

static bool DomainIf_IsDigitalKeyStateValid(const DigitalKeyState_t *state)
{
    bool settingValid;
    bool originValid;

    settingValid = (state->setting == DIGITAL_KEY_SETTING_OFF) ||
                   (state->setting == DIGITAL_KEY_SETTING_ON);
    originValid = (state->lastUnlockOrigin == UNLOCK_ORIGIN_USER_REQUEST) ||
                  (state->lastUnlockOrigin == UNLOCK_ORIGIN_PROXIMITY_AUTO);
    return settingValid && DomainIf_IsAvailabilityValid(state->availability) &&
           originValid && DomainIf_IsResultReasonValid(state->reason) &&
           DomainIf_IsSignalMetaValid(&state->meta);
}

static bool DomainIf_IsDigitalKeyResultValid(
    const DomainIf_DigitalKeyResult_t *result)
{
    bool originValid;

    originValid = (result->origin == UNLOCK_ORIGIN_USER_REQUEST) ||
                  (result->origin == UNLOCK_ORIGIN_PROXIMITY_AUTO);
    if (!originValid || !DomainIf_IsResultReasonValid(result->reason) ||
        !DomainIf_IsConfirmationValid(result->confirmation))
    {
        return false;
    }
    if (!result->hasVehicleResult)
    {
        return (result->confirmation == RESULT_CONFIRMATION_UNCONFIRMED) &&
               !result->actualUnlockTransition;
    }
    if (!DomainIf_IsResultValid(result->result) ||
        (result->relatedDoorCommandId == 0U))
    {
        return false;
    }
    return !result->actualUnlockTransition ||
           ((result->result == REQUEST_RESULT_DONE) &&
            (result->confirmation == RESULT_CONFIRMATION_CONFIRMED));
}

static bool DomainIf_IsUserSettingsValid(
    const DomainIf_UserSettingsState_t *settings)
{
    return (!settings->hasUserFanLevel ||
            DomainIf_IsFanLevelValid(settings->userFanLevel, false)) &&
           (!settings->hasNormalLightLevel ||
            (settings->normalLightLevelPercent <= 100U)) &&
           DomainIf_IsSignalMetaValid(&settings->meta);
}

static bool DomainIf_IsWindowStateValid(const WindowState_t *state)
{
    bool motionValid;
    bool antiPinchValid;

    motionValid = (state->motionState == WINDOW_MOTION_UNKNOWN) ||
                  (state->motionState == WINDOW_MOTION_STOPPED) ||
                  (state->motionState == WINDOW_MOTION_OPENING) ||
                  (state->motionState == WINDOW_MOTION_CLOSING) ||
                  (state->motionState == WINDOW_MOTION_ANTIPINCH_REVERSING);
    antiPinchValid = (state->antiPinchState == ANTIPINCH_STATE_CLEAR) ||
                     (state->antiPinchState == ANTIPINCH_STATE_ACTIVE);
    return motionValid && DomainIf_IsCommonEcuStateValid(state->ecuState) &&
           (state->positionPercent <= 100U) && antiPinchValid &&
           DomainIf_IsSignalMetaValid(&state->meta);
}

static bool DomainIf_IsWindowFaultStateValid(
    const DomainIf_WindowFaultState_t *state)
{
    return DomainIf_IsFaultInfoValid(&state->fault) &&
           DomainIf_IsSignalMetaValid(&state->meta);
}

DomainIf_Status_t DomainIf_Init(void)
{
    /*
     * Repeated initialization is idempotent. In particular, it must not
     * silently remove bindings that may already belong to active owners.
     * Explicit NULL registration is the only supported unbind operation.
     */
    if (g_domainIfCore.initialized)
    {
        return DOMAIN_IF_OK;
    }

    (void)memset(&g_domainIfCore, 0, sizeof(g_domainIfCore));
    g_domainIfCore.initialized = true;
    return DOMAIN_IF_OK;
}

bool DomainIf_IsInitialized(void)
{
    return g_domainIfCore.initialized;
}

DomainIf_Status_t DomainIf_SetRxObserver(
    const DomainIf_RxObserver_t *observer)
{
    if (!g_domainIfCore.initialized)
    {
        return DOMAIN_IF_NOT_INITIALIZED;
    }

    if (observer == NULL)
    {
        (void)memset(&g_domainIfCore.rxObserver, 0,
                     sizeof(g_domainIfCore.rxObserver));
        g_domainIfCore.hasRxObserver = false;
        return DOMAIN_IF_OK;
    }

    /* The table is borrowed only for this call; no caller pointer is kept. */
    g_domainIfCore.rxObserver = *observer;
    g_domainIfCore.hasRxObserver = true;
    return DOMAIN_IF_OK;
}

DomainIf_Status_t DomainIf_SetTxPort(const DomainIf_TxPort_t *port)
{
    if (!g_domainIfCore.initialized)
    {
        return DOMAIN_IF_NOT_INITIALIZED;
    }

    if (port == NULL)
    {
        (void)memset(&g_domainIfCore.txPort, 0,
                     sizeof(g_domainIfCore.txPort));
        g_domainIfCore.hasTxPort = false;
        return DOMAIN_IF_OK;
    }

    /* The table is borrowed only for this call; no caller pointer is kept. */
    g_domainIfCore.txPort = *port;
    g_domainIfCore.hasTxPort = true;
    return DOMAIN_IF_OK;
}

DomainIf_Status_t DomainIfCore_NotifyBcmCommandResult(
    const DomainIf_BcmCommandResult_t *result)
{
    if (result == NULL)
    {
        return DOMAIN_IF_INVALID_ARGUMENT;
    }
    if (!g_domainIfCore.initialized)
    {
        return DOMAIN_IF_NOT_INITIALIZED;
    }
    if (!g_domainIfCore.hasRxObserver ||
        (g_domainIfCore.rxObserver.onBcmCommandResult == NULL))
    {
        return DOMAIN_IF_OK;
    }

    return g_domainIfCore.rxObserver.onBcmCommandResult(result);
}

bool DomainIfCore_IsDoorStateValid(const DoorState_t *state)
{
    return (state != NULL) && DomainIf_IsDoorStateValid(state);
}

bool DomainIfCore_IsBcmStatusValid(
    const DomainIf_BcmStatusReport_t *state)
{
    return (state != NULL) && DomainIf_IsBcmStatusValid(state);
}

bool DomainIfCore_IsCisRearValid(const CisRearState_t *state)
{
    return (state != NULL) && DomainIf_IsCisRearValid(state);
}

bool DomainIfCore_IsCisStatusValid(
    const DomainIf_CisStatusReport_t *state)
{
    return (state != NULL) && DomainIf_IsCisStatusValid(state);
}

DomainIf_Status_t DomainIfCore_NotifyBcmStatus(
    const DomainIf_BcmStatusReport_t *status)
{
    if (status == NULL)
    {
        return DOMAIN_IF_INVALID_ARGUMENT;
    }
    if (!g_domainIfCore.initialized)
    {
        return DOMAIN_IF_NOT_INITIALIZED;
    }
    if (!g_domainIfCore.hasRxObserver ||
        (g_domainIfCore.rxObserver.onBcmStatus == NULL))
    {
        return DOMAIN_IF_OK;
    }

    return g_domainIfCore.rxObserver.onBcmStatus(status);
}

DomainIf_Status_t DomainIfCore_NotifyCisStatus(
    const DomainIf_CisStatusReport_t *status)
{
    if (status == NULL)
    {
        return DOMAIN_IF_INVALID_ARGUMENT;
    }
    if (!g_domainIfCore.initialized)
    {
        return DOMAIN_IF_NOT_INITIALIZED;
    }
    if (!g_domainIfCore.hasRxObserver ||
        (g_domainIfCore.rxObserver.onCisStatus == NULL))
    {
        return DOMAIN_IF_OK;
    }

    return g_domainIfCore.rxObserver.onCisStatus(status);
}

#define DOMAIN_IF_FORWARD_TX(value_, valid_, callback_)                        \
    do                                                                         \
    {                                                                          \
        if ((value_) == NULL)                                                   \
        {                                                                      \
            return DOMAIN_IF_INVALID_ARGUMENT;                                 \
        }                                                                      \
        if (!(valid_))                                                         \
        {                                                                      \
            return DOMAIN_IF_INVALID_ARGUMENT;                                 \
        }                                                                      \
        if (!g_domainIfCore.initialized)                                       \
        {                                                                      \
            return DOMAIN_IF_NOT_INITIALIZED;                                  \
        }                                                                      \
        if (!g_domainIfCore.hasTxPort ||                                       \
            (g_domainIfCore.txPort.callback_ == NULL))                         \
        {                                                                      \
            return DOMAIN_IF_NOT_AVAILABLE;                                    \
        }                                                                      \
        return g_domainIfCore.txPort.callback_(value_);                         \
    } while (0)

#define DOMAIN_IF_UNSUPPORTED_WINDOW_TX(value_, valid_)                        \
    do                                                                         \
    {                                                                          \
        if ((value_) == NULL)                                                   \
        {                                                                      \
            return DOMAIN_IF_INVALID_ARGUMENT;                                 \
        }                                                                      \
        if (!(valid_))                                                         \
        {                                                                      \
            return DOMAIN_IF_INVALID_ARGUMENT;                                 \
        }                                                                      \
        if (!g_domainIfCore.initialized)                                       \
        {                                                                      \
            return DOMAIN_IF_NOT_INITIALIZED;                                  \
        }                                                                      \
        return DOMAIN_IF_UNSUPPORTED;                                          \
    } while (0)

DomainIf_Status_t DomainIf_TxBcmDoorCommand(
    const DomainIf_BcmDoorCommand_t *command)
{
    DOMAIN_IF_FORWARD_TX(command, DomainIf_IsDoorCommandValid(command),
                         txBcmDoorCommand);
}

DomainIf_Status_t DomainIf_TxBcmClimateCommand(
    const DomainIf_BcmClimateCommand_t *command)
{
    DOMAIN_IF_FORWARD_TX(command, DomainIf_IsClimateCommandValid(command),
                         txBcmClimateCommand);
}

DomainIf_Status_t DomainIf_TxBcmInteriorLightCommand(
    const DomainIf_BcmInteriorLightCommand_t *command)
{
    DOMAIN_IF_FORWARD_TX(command,
                         DomainIf_IsInteriorLightCommandValid(command),
                         txBcmInteriorLightCommand);
}

DomainIf_Status_t DomainIf_TxCisPowerPermission(
    const DomainIf_CisPowerPermission_t *permission)
{
    DOMAIN_IF_FORWARD_TX(permission,
                         DomainIf_IsCisPowerPermissionValid(permission),
                         txCisPowerPermission);
}

DomainIf_Status_t DomainIf_TxWindowCommand(
    const DomainIf_WindowCommand_t *command)
{
    DOMAIN_IF_UNSUPPORTED_WINDOW_TX(command,
                                    DomainIf_IsWindowCommandValid(command));
}

DomainIf_Status_t DomainIf_TxWindowOperationPermission(
    const DomainIf_WindowOperationPermission_t *permission)
{
    DOMAIN_IF_UNSUPPORTED_WINDOW_TX(
        permission,
        DomainIf_IsDataQualityValid(permission->quality) &&
        DomainIf_IsSignalMetaValid(&permission->meta) &&
        (permission->quality == permission->meta.quality));
}

DomainIf_Status_t DomainIf_TxWindowDomainAlive(
    const DomainIf_WindowDomainAlive_t *alive)
{
    DOMAIN_IF_UNSUPPORTED_WINDOW_TX(
        alive, DomainIf_IsSignalMetaValid(&alive->meta));
}

DomainIf_Status_t DomainIf_TxVssOneShotEvent(
    const DomainIf_VssOneShotEvent_t *event)
{
    DOMAIN_IF_FORWARD_TX(event, DomainIf_IsVssEventValid(event),
                         txVssOneShotEvent);
}

DomainIf_Status_t DomainIf_TxVssWarningState(
    const DomainIf_VssWarningState_t *state)
{
    DOMAIN_IF_FORWARD_TX(state, DomainIf_IsVssWarningStateValid(state),
                         txVssWarningState);
}

DomainIf_Status_t DomainIf_TxMobileRequestResult(
    const DomainIf_MobileRequestResult_t *result)
{
    if (result == NULL)
    {
        return DOMAIN_IF_INVALID_ARGUMENT;
    }
    if (!DomainIf_IsMobileRequestResultValid(result))
    {
        return DOMAIN_IF_INVALID_ARGUMENT;
    }
    if (!g_domainIfCore.initialized)
    {
        return DOMAIN_IF_NOT_INITIALIZED;
    }
    if (result->target == MOBILE_RESULT_TARGET_WINDOW)
    {
        return DOMAIN_IF_UNSUPPORTED;
    }
    if (!g_domainIfCore.hasTxPort ||
        (g_domainIfCore.txPort.txMobileRequestResult == NULL))
    {
        return DOMAIN_IF_NOT_AVAILABLE;
    }
    return g_domainIfCore.txPort.txMobileRequestResult(result);
}

DomainIf_Status_t DomainIf_TxMobileBcmDoorState(const DoorState_t *state)
{
    DOMAIN_IF_FORWARD_TX(state, DomainIf_IsDoorStateValid(state),
                         txMobileBcmDoorState);
}

DomainIf_Status_t DomainIf_TxMobileBcmClimateState(
    const ClimateState_t *state)
{
    DOMAIN_IF_FORWARD_TX(state, DomainIf_IsClimateStateValid(state),
                         txMobileBcmClimateState);
}

DomainIf_Status_t DomainIf_TxMobileBcmInteriorLightState(
    const InteriorLightState_t *state)
{
    DOMAIN_IF_FORWARD_TX(state, DomainIf_IsInteriorLightStateValid(state),
                         txMobileBcmInteriorLightState);
}

DomainIf_Status_t DomainIf_TxMobileBcmStatus(
    const DomainIf_BcmStatusReport_t *state)
{
    DOMAIN_IF_FORWARD_TX(state, DomainIf_IsBcmStatusValid(state),
                         txMobileBcmStatus);
}

DomainIf_Status_t DomainIf_TxMobileCisEnvironment(
    const CisCabinEnvironmentState_t *state)
{
    DOMAIN_IF_FORWARD_TX(state, DomainIf_IsCisEnvironmentValid(state),
                         txMobileCisEnvironment);
}

DomainIf_Status_t DomainIf_TxMobileCisOccupant(
    const CisOccupantState_t *state)
{
    DOMAIN_IF_FORWARD_TX(state, DomainIfCore_IsCisOccupantValid(state),
                         txMobileCisOccupant);
}

DomainIf_Status_t DomainIf_TxMobileCisRear(const CisRearState_t *state)
{
    DOMAIN_IF_FORWARD_TX(state, DomainIf_IsCisRearValid(state),
                         txMobileCisRear);
}

DomainIf_Status_t DomainIf_TxMobileCisStatus(
    const DomainIf_CisStatusReport_t *state)
{
    DOMAIN_IF_FORWARD_TX(state, DomainIf_IsCisStatusValid(state),
                         txMobileCisStatus);
}

DomainIf_Status_t DomainIf_TxMobileVssStatus(
    const DomainIf_VssStateReport_t *state)
{
    DOMAIN_IF_FORWARD_TX(state, DomainIf_IsVssStatusValid(state),
                         txMobileVssStatus);
}

DomainIf_Status_t DomainIf_TxMobileWindowState(const WindowState_t *state)
{
    DOMAIN_IF_UNSUPPORTED_WINDOW_TX(state,
                                    DomainIf_IsWindowStateValid(state));
}

DomainIf_Status_t DomainIf_TxMobileWindowFault(
    const DomainIf_WindowFaultState_t *state)
{
    DOMAIN_IF_UNSUPPORTED_WINDOW_TX(
        state, DomainIf_IsWindowFaultStateValid(state));
}

DomainIf_Status_t DomainIf_TxMobileWarning(
    const DomainIf_WarningInfo_t *warning)
{
    DOMAIN_IF_FORWARD_TX(warning, DomainIf_IsWarningValid(warning),
                         txMobileWarning);
}

DomainIf_Status_t DomainIf_TxMobileFunctionAvailability(
    const DomainIf_FunctionAvailabilitySet_t *availability)
{
    DOMAIN_IF_FORWARD_TX(
        availability,
        DomainIf_IsFunctionAvailabilitySetValid(availability),
        txMobileFunctionAvailability);
}

DomainIf_Status_t DomainIf_TxMobileDigitalKeyState(
    const DigitalKeyState_t *state)
{
    DOMAIN_IF_FORWARD_TX(state, DomainIf_IsDigitalKeyStateValid(state),
                         txMobileDigitalKeyState);
}

DomainIf_Status_t DomainIf_TxMobileDigitalKeyResult(
    const DomainIf_DigitalKeyResult_t *result)
{
    DOMAIN_IF_FORWARD_TX(result, DomainIf_IsDigitalKeyResultValid(result),
                         txMobileDigitalKeyResult);
}

DomainIf_Status_t DomainIf_TxMobileUserSettings(
    const DomainIf_UserSettingsState_t *settings)
{
    DOMAIN_IF_FORWARD_TX(settings, DomainIf_IsUserSettingsValid(settings),
                         txMobileUserSettings);
}

#undef DOMAIN_IF_FORWARD_TX
#undef DOMAIN_IF_UNSUPPORTED_WINDOW_TX

#if defined(DOMAIN_IF_CORE_TEST)
bool DomainIfCore_TestHasRxObserver(void)
{
    return g_domainIfCore.hasRxObserver;
}

bool DomainIfCore_TestHasTxPort(void)
{
    return g_domainIfCore.hasTxPort;
}

void DomainIfCore_TestCopyRxObserver(DomainIf_RxObserver_t *outObserver)
{
    if (outObserver != NULL)
    {
        *outObserver = g_domainIfCore.rxObserver;
    }
}

void DomainIfCore_TestCopyTxPort(DomainIf_TxPort_t *outPort)
{
    if (outPort != NULL)
    {
        *outPort = g_domainIfCore.txPort;
    }
}
#endif
