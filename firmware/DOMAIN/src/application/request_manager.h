#ifndef REQUEST_MANAGER_H
#define REQUEST_MANAGER_H

#include <stdbool.h>
#include <stdint.h>

#include "vehicle_state_manager.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Application-internal request policy types. They are deliberately not C2
 * wire/API replacements. A later integration adapter must map the real C2
 * request and verified context into these values explicitly.
 */

typedef enum
{
    REQUEST_MANAGER_OK = 0,
    REQUEST_MANAGER_INVALID_ARGUMENT
} RequestManager_Status_t;

typedef enum
{
    REQUEST_MANAGER_KIND_DOOR = 0,
    REQUEST_MANAGER_KIND_CLIMATE,
    REQUEST_MANAGER_KIND_INTERIOR_LIGHT,
    REQUEST_MANAGER_KIND_DIGITAL_KEY_SETTING
} RequestManager_RequestKind_t;

typedef enum
{
    REQUEST_MANAGER_OPERATION_DOOR_LOCK = 0x0100,
    REQUEST_MANAGER_OPERATION_DOOR_UNLOCK,
    REQUEST_MANAGER_OPERATION_CLIMATE_SET_TARGET_TEMPERATURE = 0x0200,
    REQUEST_MANAGER_OPERATION_CLIMATE_SET_AUTO_ENABLED,
    REQUEST_MANAGER_OPERATION_CLIMATE_SET_FAN_LEVEL,
    REQUEST_MANAGER_OPERATION_INTERIOR_LIGHT_SET_ENABLED = 0x0300,
    REQUEST_MANAGER_OPERATION_INTERIOR_LIGHT_SET_LEVEL,
    REQUEST_MANAGER_OPERATION_INTERIOR_LIGHT_SET_COLOR,
    REQUEST_MANAGER_OPERATION_DIGITAL_KEY_SET_ENABLED = 0x0400
} RequestManager_Operation_t;

typedef enum
{
    REQUEST_MANAGER_FAN_LEVEL_OFF = 0,
    REQUEST_MANAGER_FAN_LEVEL_LOW,
    REQUEST_MANAGER_FAN_LEVEL_MEDIUM,
    REQUEST_MANAGER_FAN_LEVEL_HIGH,
    REQUEST_MANAGER_FAN_LEVEL_UNKNOWN
} RequestManager_FanLevel_t;

typedef enum
{
    REQUEST_MANAGER_DOOR_LOCK_UNKNOWN = 0,
    REQUEST_MANAGER_DOOR_LOCKED,
    REQUEST_MANAGER_DOOR_UNLOCKED
} RequestManager_DoorLockState_t;

typedef enum
{
    REQUEST_MANAGER_DOOR_OPEN_UNKNOWN = 0,
    REQUEST_MANAGER_DOOR_CLOSED,
    REQUEST_MANAGER_DOOR_OPEN
} RequestManager_DoorOpenState_t;

typedef struct
{
    uint8_t red;
    uint8_t green;
    uint8_t blue;
} RequestManager_RgbColor_t;

typedef union
{
    int16_t targetTemperatureCentiC;
    bool enabled;
    RequestManager_FanLevel_t fanLevel;
    uint8_t levelPercent;
    RequestManager_RgbColor_t color;
} RequestManager_RequestArgument_t;

typedef struct
{
    uint32_t deviceContextId;
    uint64_t sessionId;
    RequestManager_RequestKind_t kind;
    RequestManager_Operation_t operation;
    RequestManager_RequestArgument_t argument;
    VehicleStateQuality_t quality;
} RequestManager_Request_t;

typedef struct
{
    bool registrationVerified;
    bool registered;
    bool connectionVerified;
    bool connected;
    bool sessionVerified;
    uint32_t currentDeviceContextId;
    uint64_t currentSessionId;
} RequestManager_AuthContext_t;

typedef struct
{
    bool supportKnown;
    bool supported;
    bool permissionKnown;
    bool operationAllowed;
} RequestManager_FunctionGate_t;

typedef struct
{
    RequestManager_FunctionGate_t door;
    RequestManager_FunctionGate_t climate;
    RequestManager_FunctionGate_t interiorLight;
    RequestManager_FunctionGate_t digitalKeySetting;

    RequestManager_DoorLockState_t doorLockState;
    VehicleStateQuality_t doorLockQuality;
    RequestManager_DoorOpenState_t doorOpenState;
    VehicleStateQuality_t doorOpenQuality;

    RequestManager_FanLevel_t currentFanLevel;
    VehicleStateQuality_t currentFanQuality;

    /* Kept independent so unrelated sensor faults cannot become global gates. */
    VehicleStateQuality_t cabinTemperatureQuality;
    VehicleStateQuality_t cabinIlluminanceQuality;
} RequestManager_Context_t;

typedef struct
{
    bool hasTargetTemperatureRange;
    int16_t minimumTargetTemperatureCentiC;
    int16_t maximumTargetTemperatureCentiC;
} RequestManager_Policy_t;

typedef enum
{
    REQUEST_MANAGER_VERDICT_ALLOW = 0,
    REQUEST_MANAGER_VERDICT_REJECT,
    REQUEST_MANAGER_VERDICT_UNAVAILABLE
} RequestManager_Verdict_t;

typedef enum
{
    REQUEST_MANAGER_REASON_NONE = 0,
    REQUEST_MANAGER_REASON_UNKNOWN_REQUEST_KIND,
    REQUEST_MANAGER_REASON_UNSUPPORTED_OPERATION,
    REQUEST_MANAGER_REASON_ARGUMENT_OUT_OF_RANGE,
    REQUEST_MANAGER_REASON_REQUEST_NO_DATA,
    REQUEST_MANAGER_REASON_REQUEST_STALE,
    REQUEST_MANAGER_REASON_REQUEST_INVALID,
    REQUEST_MANAGER_REASON_REGISTRATION_UNAVAILABLE,
    REQUEST_MANAGER_REASON_DEVICE_NOT_REGISTERED,
    REQUEST_MANAGER_REASON_CONNECTION_UNAVAILABLE,
    REQUEST_MANAGER_REASON_NOT_CONNECTED,
    REQUEST_MANAGER_REASON_SESSION_UNAVAILABLE,
    REQUEST_MANAGER_REASON_DEVICE_MISMATCH,
    REQUEST_MANAGER_REASON_SESSION_MISMATCH,
    REQUEST_MANAGER_REASON_FUNCTION_SUPPORT_UNAVAILABLE,
    REQUEST_MANAGER_REASON_FUNCTION_UNSUPPORTED,
    REQUEST_MANAGER_REASON_PERMISSION_UNAVAILABLE,
    REQUEST_MANAGER_REASON_OPERATION_NOT_ALLOWED,
    REQUEST_MANAGER_REASON_STATE_NO_DATA,
    REQUEST_MANAGER_REASON_STATE_STALE,
    REQUEST_MANAGER_REASON_STATE_INVALID,
    REQUEST_MANAGER_REASON_STATE_VALUE_UNKNOWN,
    REQUEST_MANAGER_REASON_DOOR_OPEN,
    REQUEST_MANAGER_REASON_POLICY_UNAVAILABLE
} RequestManager_Reason_t;

typedef enum
{
    REQUEST_MANAGER_ACTION_NONE = 0,
    REQUEST_MANAGER_ACTION_NO_COMMAND_REQUIRED,
    REQUEST_MANAGER_ACTION_FORWARD_DOOR_INTENT,
    REQUEST_MANAGER_ACTION_FORWARD_CLIMATE_INTENT,
    REQUEST_MANAGER_ACTION_FORWARD_INTERIOR_LIGHT_INTENT,
    REQUEST_MANAGER_ACTION_UPDATE_DIGITAL_KEY_SETTING_ONLY
} RequestManager_Action_t;

typedef struct
{
    RequestManager_Verdict_t verdict;
    RequestManager_Reason_t reason;
    RequestManager_Action_t action;
} RequestManager_Decision_t;

RequestManager_Status_t RequestManager_Evaluate(
    const RequestManager_Request_t *request,
    const RequestManager_AuthContext_t *auth,
    const RequestManager_Context_t *context,
    const RequestManager_Policy_t *policy,
    RequestManager_Decision_t *outDecision);

#ifdef __cplusplus
}
#endif

#endif /* REQUEST_MANAGER_H */
