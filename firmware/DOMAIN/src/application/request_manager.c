#include "request_manager.h"

#include <stddef.h>

static void RequestManager_SetDecision(
    RequestManager_Decision_t *decision,
    RequestManager_Verdict_t verdict,
    RequestManager_Reason_t reason,
    RequestManager_Action_t action)
{
    decision->verdict = verdict;
    decision->reason = reason;
    decision->action = action;
}

static bool RequestManager_IsQualityValid(VehicleStateQuality_t quality)
{
    return (quality == VEHICLE_STATE_QUALITY_OK) ||
           (quality == VEHICLE_STATE_QUALITY_STALE) ||
           (quality == VEHICLE_STATE_QUALITY_INVALID) ||
           (quality == VEHICLE_STATE_QUALITY_NO_DATA);
}

static bool RequestManager_IsFanLevelCommandValue(
    RequestManager_FanLevel_t fanLevel)
{
    switch (fanLevel)
    {
        case REQUEST_MANAGER_FAN_LEVEL_OFF:
        case REQUEST_MANAGER_FAN_LEVEL_LOW:
        case REQUEST_MANAGER_FAN_LEVEL_MEDIUM:
        case REQUEST_MANAGER_FAN_LEVEL_HIGH:
            return true;

        case REQUEST_MANAGER_FAN_LEVEL_UNKNOWN:
        default:
            return false;
    }
}

static bool RequestManager_RequireQuality(
    VehicleStateQuality_t quality,
    RequestManager_Decision_t *decision)
{
    switch (quality)
    {
        case VEHICLE_STATE_QUALITY_OK:
            return true;

        case VEHICLE_STATE_QUALITY_NO_DATA:
            RequestManager_SetDecision(decision,
                                       REQUEST_MANAGER_VERDICT_UNAVAILABLE,
                                       REQUEST_MANAGER_REASON_STATE_NO_DATA,
                                       REQUEST_MANAGER_ACTION_NONE);
            break;

        case VEHICLE_STATE_QUALITY_STALE:
            RequestManager_SetDecision(decision,
                                       REQUEST_MANAGER_VERDICT_UNAVAILABLE,
                                       REQUEST_MANAGER_REASON_STATE_STALE,
                                       REQUEST_MANAGER_ACTION_NONE);
            break;

        case VEHICLE_STATE_QUALITY_INVALID:
            RequestManager_SetDecision(decision,
                                       REQUEST_MANAGER_VERDICT_UNAVAILABLE,
                                       REQUEST_MANAGER_REASON_STATE_INVALID,
                                       REQUEST_MANAGER_ACTION_NONE);
            break;

        default:
            RequestManager_SetDecision(decision,
                                       REQUEST_MANAGER_VERDICT_REJECT,
                                       REQUEST_MANAGER_REASON_ARGUMENT_OUT_OF_RANGE,
                                       REQUEST_MANAGER_ACTION_NONE);
            break;
    }

    return false;
}

static const RequestManager_FunctionGate_t *RequestManager_GetGate(
    RequestManager_RequestKind_t kind,
    const RequestManager_Context_t *context)
{
    switch (kind)
    {
        case REQUEST_MANAGER_KIND_DOOR:
            return &context->door;
        case REQUEST_MANAGER_KIND_CLIMATE:
            return &context->climate;
        case REQUEST_MANAGER_KIND_INTERIOR_LIGHT:
            return &context->interiorLight;
        case REQUEST_MANAGER_KIND_DIGITAL_KEY_SETTING:
            return &context->digitalKeySetting;
        default:
            return NULL;
    }
}

static bool RequestManager_ValidateOperationAndArgument(
    const RequestManager_Request_t *request,
    const RequestManager_Policy_t *policy,
    RequestManager_Decision_t *decision)
{
    switch (request->kind)
    {
        case REQUEST_MANAGER_KIND_DOOR:
            if ((request->operation != REQUEST_MANAGER_OPERATION_DOOR_LOCK) &&
                (request->operation != REQUEST_MANAGER_OPERATION_DOOR_UNLOCK))
            {
                RequestManager_SetDecision(decision,
                                           REQUEST_MANAGER_VERDICT_REJECT,
                                           REQUEST_MANAGER_REASON_UNSUPPORTED_OPERATION,
                                           REQUEST_MANAGER_ACTION_NONE);
                return false;
            }
            return true;

        case REQUEST_MANAGER_KIND_CLIMATE:
            switch (request->operation)
            {
                case REQUEST_MANAGER_OPERATION_CLIMATE_SET_TARGET_TEMPERATURE:
                    if (!policy->hasTargetTemperatureRange)
                    {
                        RequestManager_SetDecision(decision,
                                                   REQUEST_MANAGER_VERDICT_UNAVAILABLE,
                                                   REQUEST_MANAGER_REASON_POLICY_UNAVAILABLE,
                                                   REQUEST_MANAGER_ACTION_NONE);
                        return false;
                    }
                    if ((request->argument.targetTemperatureCentiC <
                         policy->minimumTargetTemperatureCentiC) ||
                        (request->argument.targetTemperatureCentiC >
                         policy->maximumTargetTemperatureCentiC))
                    {
                        RequestManager_SetDecision(decision,
                                                   REQUEST_MANAGER_VERDICT_REJECT,
                                                   REQUEST_MANAGER_REASON_ARGUMENT_OUT_OF_RANGE,
                                                   REQUEST_MANAGER_ACTION_NONE);
                        return false;
                    }
                    return true;

                case REQUEST_MANAGER_OPERATION_CLIMATE_SET_AUTO_ENABLED:
                    return true;

                case REQUEST_MANAGER_OPERATION_CLIMATE_SET_FAN_LEVEL:
                    if (!RequestManager_IsFanLevelCommandValue(
                            request->argument.fanLevel))
                    {
                        RequestManager_SetDecision(decision,
                                                   REQUEST_MANAGER_VERDICT_REJECT,
                                                   REQUEST_MANAGER_REASON_ARGUMENT_OUT_OF_RANGE,
                                                   REQUEST_MANAGER_ACTION_NONE);
                        return false;
                    }
                    return true;

                default:
                    RequestManager_SetDecision(decision,
                                               REQUEST_MANAGER_VERDICT_REJECT,
                                               REQUEST_MANAGER_REASON_UNSUPPORTED_OPERATION,
                                               REQUEST_MANAGER_ACTION_NONE);
                    return false;
            }

        case REQUEST_MANAGER_KIND_INTERIOR_LIGHT:
            switch (request->operation)
            {
                case REQUEST_MANAGER_OPERATION_INTERIOR_LIGHT_SET_ENABLED:
                case REQUEST_MANAGER_OPERATION_INTERIOR_LIGHT_SET_COLOR:
                    return true;

                case REQUEST_MANAGER_OPERATION_INTERIOR_LIGHT_SET_LEVEL:
                    if (request->argument.levelPercent > 100U)
                    {
                        RequestManager_SetDecision(decision,
                                                   REQUEST_MANAGER_VERDICT_REJECT,
                                                   REQUEST_MANAGER_REASON_ARGUMENT_OUT_OF_RANGE,
                                                   REQUEST_MANAGER_ACTION_NONE);
                        return false;
                    }
                    return true;

                default:
                    RequestManager_SetDecision(decision,
                                               REQUEST_MANAGER_VERDICT_REJECT,
                                               REQUEST_MANAGER_REASON_UNSUPPORTED_OPERATION,
                                               REQUEST_MANAGER_ACTION_NONE);
                    return false;
            }

        case REQUEST_MANAGER_KIND_DIGITAL_KEY_SETTING:
            if (request->operation != REQUEST_MANAGER_OPERATION_DIGITAL_KEY_SET_ENABLED)
            {
                RequestManager_SetDecision(decision,
                                           REQUEST_MANAGER_VERDICT_REJECT,
                                           REQUEST_MANAGER_REASON_UNSUPPORTED_OPERATION,
                                           REQUEST_MANAGER_ACTION_NONE);
                return false;
            }
            return true;

        default:
            RequestManager_SetDecision(decision,
                                       REQUEST_MANAGER_VERDICT_REJECT,
                                       REQUEST_MANAGER_REASON_UNKNOWN_REQUEST_KIND,
                                       REQUEST_MANAGER_ACTION_NONE);
            return false;
    }
}

static bool RequestManager_ValidateRequestQuality(
    VehicleStateQuality_t quality,
    RequestManager_Decision_t *decision)
{
    switch (quality)
    {
        case VEHICLE_STATE_QUALITY_OK:
            return true;
        case VEHICLE_STATE_QUALITY_NO_DATA:
            RequestManager_SetDecision(decision, REQUEST_MANAGER_VERDICT_UNAVAILABLE,
                                       REQUEST_MANAGER_REASON_REQUEST_NO_DATA,
                                       REQUEST_MANAGER_ACTION_NONE);
            return false;
        case VEHICLE_STATE_QUALITY_STALE:
            RequestManager_SetDecision(decision, REQUEST_MANAGER_VERDICT_UNAVAILABLE,
                                       REQUEST_MANAGER_REASON_REQUEST_STALE,
                                       REQUEST_MANAGER_ACTION_NONE);
            return false;
        case VEHICLE_STATE_QUALITY_INVALID:
            RequestManager_SetDecision(decision, REQUEST_MANAGER_VERDICT_REJECT,
                                       REQUEST_MANAGER_REASON_REQUEST_INVALID,
                                       REQUEST_MANAGER_ACTION_NONE);
            return false;
        default:
            RequestManager_SetDecision(decision, REQUEST_MANAGER_VERDICT_REJECT,
                                       REQUEST_MANAGER_REASON_ARGUMENT_OUT_OF_RANGE,
                                       REQUEST_MANAGER_ACTION_NONE);
            return false;
    }
}

static bool RequestManager_ValidateAuth(
    const RequestManager_Request_t *request,
    const RequestManager_AuthContext_t *auth,
    RequestManager_Decision_t *decision)
{
    if (!auth->registrationVerified)
    {
        RequestManager_SetDecision(decision, REQUEST_MANAGER_VERDICT_UNAVAILABLE,
                                   REQUEST_MANAGER_REASON_REGISTRATION_UNAVAILABLE,
                                   REQUEST_MANAGER_ACTION_NONE);
        return false;
    }
    if (!auth->registered)
    {
        RequestManager_SetDecision(decision, REQUEST_MANAGER_VERDICT_REJECT,
                                   REQUEST_MANAGER_REASON_DEVICE_NOT_REGISTERED,
                                   REQUEST_MANAGER_ACTION_NONE);
        return false;
    }
    if (!auth->connectionVerified)
    {
        RequestManager_SetDecision(decision, REQUEST_MANAGER_VERDICT_UNAVAILABLE,
                                   REQUEST_MANAGER_REASON_CONNECTION_UNAVAILABLE,
                                   REQUEST_MANAGER_ACTION_NONE);
        return false;
    }
    if (!auth->connected)
    {
        RequestManager_SetDecision(decision, REQUEST_MANAGER_VERDICT_REJECT,
                                   REQUEST_MANAGER_REASON_NOT_CONNECTED,
                                   REQUEST_MANAGER_ACTION_NONE);
        return false;
    }
    if (!auth->sessionVerified)
    {
        RequestManager_SetDecision(decision, REQUEST_MANAGER_VERDICT_UNAVAILABLE,
                                   REQUEST_MANAGER_REASON_SESSION_UNAVAILABLE,
                                   REQUEST_MANAGER_ACTION_NONE);
        return false;
    }
    if (request->deviceContextId != auth->currentDeviceContextId)
    {
        RequestManager_SetDecision(decision, REQUEST_MANAGER_VERDICT_REJECT,
                                   REQUEST_MANAGER_REASON_DEVICE_MISMATCH,
                                   REQUEST_MANAGER_ACTION_NONE);
        return false;
    }
    if (request->sessionId != auth->currentSessionId)
    {
        RequestManager_SetDecision(decision, REQUEST_MANAGER_VERDICT_REJECT,
                                   REQUEST_MANAGER_REASON_SESSION_MISMATCH,
                                   REQUEST_MANAGER_ACTION_NONE);
        return false;
    }
    return true;
}

static bool RequestManager_ValidateGate(
    const RequestManager_FunctionGate_t *gate,
    RequestManager_Decision_t *decision)
{
    if (!gate->supportKnown)
    {
        RequestManager_SetDecision(decision, REQUEST_MANAGER_VERDICT_UNAVAILABLE,
                                   REQUEST_MANAGER_REASON_FUNCTION_SUPPORT_UNAVAILABLE,
                                   REQUEST_MANAGER_ACTION_NONE);
        return false;
    }
    if (!gate->supported)
    {
        RequestManager_SetDecision(decision, REQUEST_MANAGER_VERDICT_REJECT,
                                   REQUEST_MANAGER_REASON_FUNCTION_UNSUPPORTED,
                                   REQUEST_MANAGER_ACTION_NONE);
        return false;
    }
    if (!gate->permissionKnown)
    {
        RequestManager_SetDecision(decision, REQUEST_MANAGER_VERDICT_UNAVAILABLE,
                                   REQUEST_MANAGER_REASON_PERMISSION_UNAVAILABLE,
                                   REQUEST_MANAGER_ACTION_NONE);
        return false;
    }
    if (!gate->operationAllowed)
    {
        RequestManager_SetDecision(decision, REQUEST_MANAGER_VERDICT_REJECT,
                                   REQUEST_MANAGER_REASON_OPERATION_NOT_ALLOWED,
                                   REQUEST_MANAGER_ACTION_NONE);
        return false;
    }
    return true;
}

static bool RequestManager_IsDoorLockStateValid(RequestManager_DoorLockState_t state)
{
    return (state == REQUEST_MANAGER_DOOR_LOCK_UNKNOWN) ||
           (state == REQUEST_MANAGER_DOOR_LOCKED) ||
           (state == REQUEST_MANAGER_DOOR_UNLOCKED);
}

static bool RequestManager_IsDoorOpenStateValid(RequestManager_DoorOpenState_t state)
{
    return (state == REQUEST_MANAGER_DOOR_OPEN_UNKNOWN) ||
           (state == REQUEST_MANAGER_DOOR_CLOSED) ||
           (state == REQUEST_MANAGER_DOOR_OPEN);
}

static bool RequestManager_EvaluateDoor(
    const RequestManager_Request_t *request,
    const RequestManager_Context_t *context,
    RequestManager_Decision_t *decision)
{
    RequestManager_DoorLockState_t targetState;

    if (!RequestManager_RequireQuality(context->doorLockQuality, decision))
    {
        return false;
    }
    if (!RequestManager_IsDoorLockStateValid(context->doorLockState))
    {
        RequestManager_SetDecision(decision, REQUEST_MANAGER_VERDICT_REJECT,
                                   REQUEST_MANAGER_REASON_ARGUMENT_OUT_OF_RANGE,
                                   REQUEST_MANAGER_ACTION_NONE);
        return false;
    }
    if (context->doorLockState == REQUEST_MANAGER_DOOR_LOCK_UNKNOWN)
    {
        RequestManager_SetDecision(decision, REQUEST_MANAGER_VERDICT_UNAVAILABLE,
                                   REQUEST_MANAGER_REASON_STATE_VALUE_UNKNOWN,
                                   REQUEST_MANAGER_ACTION_NONE);
        return false;
    }

    targetState = (request->operation == REQUEST_MANAGER_OPERATION_DOOR_LOCK)
                      ? REQUEST_MANAGER_DOOR_LOCKED
                      : REQUEST_MANAGER_DOOR_UNLOCKED;

    if (request->operation == REQUEST_MANAGER_OPERATION_DOOR_LOCK)
    {
        if (!RequestManager_RequireQuality(context->doorOpenQuality, decision))
        {
            return false;
        }
        if (!RequestManager_IsDoorOpenStateValid(context->doorOpenState))
        {
            RequestManager_SetDecision(decision, REQUEST_MANAGER_VERDICT_REJECT,
                                       REQUEST_MANAGER_REASON_ARGUMENT_OUT_OF_RANGE,
                                       REQUEST_MANAGER_ACTION_NONE);
            return false;
        }
        if (context->doorOpenState == REQUEST_MANAGER_DOOR_OPEN_UNKNOWN)
        {
            RequestManager_SetDecision(decision, REQUEST_MANAGER_VERDICT_UNAVAILABLE,
                                       REQUEST_MANAGER_REASON_STATE_VALUE_UNKNOWN,
                                       REQUEST_MANAGER_ACTION_NONE);
            return false;
        }
        if (context->doorOpenState == REQUEST_MANAGER_DOOR_OPEN)
        {
            RequestManager_SetDecision(decision, REQUEST_MANAGER_VERDICT_REJECT,
                                       REQUEST_MANAGER_REASON_DOOR_OPEN,
                                       REQUEST_MANAGER_ACTION_NONE);
            return false;
        }
    }

    RequestManager_SetDecision(
        decision,
        REQUEST_MANAGER_VERDICT_ALLOW,
        REQUEST_MANAGER_REASON_NONE,
        (context->doorLockState == targetState)
            ? REQUEST_MANAGER_ACTION_NO_COMMAND_REQUIRED
            : REQUEST_MANAGER_ACTION_FORWARD_DOOR_INTENT);
    return true;
}

static bool RequestManager_EvaluateFeatureState(
    const RequestManager_Request_t *request,
    const RequestManager_Context_t *context,
    RequestManager_Decision_t *decision)
{
    if (request->kind == REQUEST_MANAGER_KIND_DOOR)
    {
        return RequestManager_EvaluateDoor(request, context, decision);
    }

    if ((request->kind == REQUEST_MANAGER_KIND_CLIMATE) &&
        (request->operation == REQUEST_MANAGER_OPERATION_CLIMATE_SET_FAN_LEVEL))
    {
        if (!RequestManager_RequireQuality(context->currentFanQuality, decision))
        {
            return false;
        }
        if (!RequestManager_IsFanLevelCommandValue(context->currentFanLevel))
        {
            RequestManager_SetDecision(decision, REQUEST_MANAGER_VERDICT_UNAVAILABLE,
                                       REQUEST_MANAGER_REASON_STATE_VALUE_UNKNOWN,
                                       REQUEST_MANAGER_ACTION_NONE);
            return false;
        }
        RequestManager_SetDecision(
            decision,
            REQUEST_MANAGER_VERDICT_ALLOW,
            REQUEST_MANAGER_REASON_NONE,
            (context->currentFanLevel == request->argument.fanLevel)
                ? REQUEST_MANAGER_ACTION_NO_COMMAND_REQUIRED
                : REQUEST_MANAGER_ACTION_FORWARD_CLIMATE_INTENT);
        return true;
    }

    if (request->kind == REQUEST_MANAGER_KIND_CLIMATE)
    {
        RequestManager_SetDecision(decision, REQUEST_MANAGER_VERDICT_ALLOW,
                                   REQUEST_MANAGER_REASON_NONE,
                                   REQUEST_MANAGER_ACTION_FORWARD_CLIMATE_INTENT);
        return true;
    }

    if (request->kind == REQUEST_MANAGER_KIND_INTERIOR_LIGHT)
    {
        RequestManager_SetDecision(decision, REQUEST_MANAGER_VERDICT_ALLOW,
                                   REQUEST_MANAGER_REASON_NONE,
                                   REQUEST_MANAGER_ACTION_FORWARD_INTERIOR_LIGHT_INTENT);
        return true;
    }

    /* Setting acceptance never authorizes or emits an automatic Unlock. */
    RequestManager_SetDecision(decision, REQUEST_MANAGER_VERDICT_ALLOW,
                               REQUEST_MANAGER_REASON_NONE,
                               REQUEST_MANAGER_ACTION_UPDATE_DIGITAL_KEY_SETTING_ONLY);
    return true;
}

RequestManager_Status_t RequestManager_Evaluate(
    const RequestManager_Request_t *request,
    const RequestManager_AuthContext_t *auth,
    const RequestManager_Context_t *context,
    const RequestManager_Policy_t *policy,
    RequestManager_Decision_t *outDecision)
{
    const RequestManager_FunctionGate_t *gate;

    if ((request == NULL) || (auth == NULL) || (context == NULL) ||
        (policy == NULL) || (outDecision == NULL))
    {
        return REQUEST_MANAGER_INVALID_ARGUMENT;
    }

    if ((request->kind == REQUEST_MANAGER_KIND_CLIMATE) &&
        (request->operation ==
         REQUEST_MANAGER_OPERATION_CLIMATE_SET_TARGET_TEMPERATURE) &&
        policy->hasTargetTemperatureRange &&
        (policy->minimumTargetTemperatureCentiC >
         policy->maximumTargetTemperatureCentiC))
    {
        return REQUEST_MANAGER_INVALID_ARGUMENT;
    }

    RequestManager_SetDecision(outDecision, REQUEST_MANAGER_VERDICT_REJECT,
                               REQUEST_MANAGER_REASON_ARGUMENT_OUT_OF_RANGE,
                               REQUEST_MANAGER_ACTION_NONE);

    if (!RequestManager_IsQualityValid(request->quality))
    {
        return REQUEST_MANAGER_INVALID_ARGUMENT;
    }
    if (!RequestManager_ValidateOperationAndArgument(request, policy, outDecision))
    {
        return REQUEST_MANAGER_OK;
    }
    if (!RequestManager_ValidateRequestQuality(request->quality, outDecision))
    {
        return REQUEST_MANAGER_OK;
    }
    if (!RequestManager_ValidateAuth(request, auth, outDecision))
    {
        return REQUEST_MANAGER_OK;
    }

    gate = RequestManager_GetGate(request->kind, context);
    if ((gate == NULL) || !RequestManager_ValidateGate(gate, outDecision))
    {
        return REQUEST_MANAGER_OK;
    }

    (void)RequestManager_EvaluateFeatureState(request, context, outDecision);
    return REQUEST_MANAGER_OK;
}
