#ifndef NETWORK_APP_NET_IF_BCM_H
#define NETWORK_APP_NET_IF_BCM_H

/* BCM Application Port, transcribed from the interface contract:
 *   docs/network/interface/02_NODES/BCM/10_DATA.md, 20_API.md
 * Common types (NetCommandMeta, NetRgb, NetVehicleResult, ...) are in
 * network/app/net_if_common.h.
 *
 * Direction / owner:
 *   BcmApp_OnDomainDoorCommand / ClimateCommand / LightCommand
 *       Network -> Application. IMPLEMENTED BY THE BCM APPLICATION.
 *       Called inside Net_MainFunction() (serialized superloop), never from an ISR.
 *       message/meta are borrowed for the call only: copy what you need.
 *       void return: delivery is not execution, acceptance or DONE.
 *   BcmNet_SendResult / SendEvent
 *       Application -> Network. IMPLEMENTED BY THE NETWORK. Discrete records:
 *       each accepted call is kept (bounded, in call order) and sent once;
 *       a full store returns NO_CAPACITY and nothing earlier is overwritten.
 *   BcmNet_PublishDoorState / ClimateState / LightState / Status
 *       Application -> Network. IMPLEMENTED BY THE NETWORK. Current values:
 *       a record no device has taken yet is replaced by a newer one.
 *   For every BcmNet_* call: NET_SUBMIT_OK = bounded local copy only (not CAN
 *   TX, not DOMAIN acceptance, not vehicle DONE). The application never fills
 *   CAN BOOT_ID / TX_SEQUENCE; Network owns them and adds only the local wait
 *   to the submitted AGE. A call made inside a BcmApp_* callback is kept and
 *   offered in the same Net_MainFunction() pass.
 *   BcmNetPriv_OnDomainPermission
 *       NETWORK_PRIVATE: implemented AND called by the Network (supervision
 *       record of DOMAIN_PERMISSION). Declared here because 20_API.md lists
 *       it; the application never calls it.
 *   Net_Init / Net_MainFunction
 *       Application -> Network entry points (network/app/net_if_entry.h). */
#include "network/app/net_if_common.h"
#include "network/app/net_if_entry.h"

typedef uint8_t NetDoorTarget;
enum NetDoorTargetCode
{
    NET_DOOR_TARGET_LOCK = 0,
    NET_DOOR_TARGET_UNLOCK = 1,
    NET_DOOR_TARGET_NOT_APPLICABLE = 255
};

typedef uint8_t NetDoorLockState;
enum NetDoorLockStateCode
{
    NET_DOOR_LOCK_LOCKED = 0,
    NET_DOOR_LOCK_UNLOCKED = 1,
    NET_DOOR_LOCK_UNKNOWN = 255
};

typedef uint8_t NetDoorOpenState;
enum NetDoorOpenStateCode
{
    NET_DOOR_OPEN_CLOSED = 0,
    NET_DOOR_OPEN_OPEN = 1,
    NET_DOOR_OPEN_UNKNOWN = 255
};

typedef uint8_t NetDoorCompositeState;
enum NetDoorCompositeStateCode
{
    NET_DOOR_COMPOSITE_NORMAL = 0,
    NET_DOOR_COMPOSITE_INCONSISTENT = 1,
    NET_DOOR_COMPOSITE_UNTRUSTED = 2
};

typedef uint8_t NetHeatRemovalState;
enum NetHeatRemovalStateCode
{
    NET_HEAT_REMOVAL_NORMAL = 0,
    NET_HEAT_REMOVAL_OVERHEAT_CUTOFF = 1,
    NET_HEAT_REMOVAL_UNAVAILABLE = 255
};

typedef uint8_t NetLightApplyResult;
enum NetLightApplyResultCode
{
    NET_LIGHT_APPLY_APPLIED = 0,
    NET_LIGHT_APPLY_FAILED = 1,
    NET_LIGHT_APPLY_UNKNOWN = 255
};

typedef uint8_t NetBcmCommandKind;
enum NetBcmCommandKindCode
{
    NET_BCM_COMMAND_DOOR = 1,
    NET_BCM_COMMAND_CLIMATE = 2,
    NET_BCM_COMMAND_LIGHT = 3
};

typedef uint8_t NetDoorEvent;
enum NetDoorEventCode
{
    NET_DOOR_EVENT_NONE = 0,
    NET_DOOR_EVENT_LOCK_COMPLETED = 1,
    NET_DOOR_EVENT_UNLOCK_COMPLETED = 2,
    NET_DOOR_EVENT_ALREADY_AT_TARGET = 3,
    NET_DOOR_EVENT_REQUEST_REJECTED = 4,
    NET_DOOR_EVENT_REQUEST_FAILED = 5
};

typedef uint8_t NetBcmEventKind;
enum NetBcmEventKindCode
{
    NET_BCM_EVENT_OVERHEAT_DETECTED = 1,
    NET_BCM_EVENT_FAN_MISMATCH_DETECTED = 2,
    NET_BCM_EVENT_RECOVERY_CONFIRMED = 3
};

enum NetBcmFaultMask
{
    NET_BCM_FAULT_DOOR_SENSOR_FAULT = (1u << 0),
    NET_BCM_FAULT_FAN_SENSOR_FAULT = (1u << 1),
    NET_BCM_FAULT_TEMPERATURE_SENSOR_FAULT = (1u << 2),
    NET_BCM_FAULT_OVERHEAT_FAULT = (1u << 3),
    NET_BCM_FAULT_FAN_FAULT = (1u << 4),
    NET_BCM_FAULT_LOCK_ACTUATOR_FAULT = (1u << 5),
    NET_BCM_FAULT_LIGHT_APPLY_FAULT = (1u << 6),
    NET_BCM_FAULT_COMM_TIMEOUT_FAULT = (1u << 7)
};

enum NetBcmFaultCode
{
    NET_BCM_FAULT_CODE_NONE = 0,
    NET_BCM_FAULT_CODE_DOOR_SENSOR_FAULT = 1,
    NET_BCM_FAULT_CODE_FAN_SENSOR_FAULT = 2,
    NET_BCM_FAULT_CODE_TEMPERATURE_SENSOR_FAULT = 3,
    NET_BCM_FAULT_CODE_OVERHEAT_FAULT = 4,
    NET_BCM_FAULT_CODE_FAN_FAULT = 5,
    NET_BCM_FAULT_CODE_LOCK_ACTUATOR_FAULT = 6,
    NET_BCM_FAULT_CODE_LIGHT_APPLY_FAULT = 7,
    NET_BCM_FAULT_CODE_COMM_TIMEOUT_FAULT = 8
};

typedef struct NetBcmDoorCommand
{
    NetCommandMeta command;             /* original command common part */
    NetDoorTarget door_lock_target;     /* 0 LOCK, 1 UNLOCK */
    NetOrigin origin;                   /* request origin */
} NetBcmDoorCommand;

typedef struct NetBcmClimateCommand
{
    NetCommandMeta command;
    NetFanLevel fan_target_level;       /* FAN code; UNKNOWN not allowed */
    NetThermalDirection thermal_direction; /* 0 IDLE, 1 COOL, 2 HEAT */
    uint8_t thermal_target_percent;     /* 0..100 %, 1 %/raw */
} NetBcmClimateCommand;

typedef struct NetBcmLightCommand
{
    NetCommandMeta command;
    NetLightType interior_light_type;   /* LIGHT_TYPE code */
    uint8_t light_level_percent;        /* 0..100 %, 1 %/raw */
    NetRgb interior_light_color;        /* R/G/B 0..255 each */
} NetBcmLightCommand;

typedef struct NetBcmDoorState
{
    NetDoorLockState door_lock_state;   /* 0 LOCKED, 1 UNLOCKED, 255 UNKNOWN */
    NetDoorOpenState door_open_state;   /* 0 CLOSED, 1 OPEN, 255 UNKNOWN */
    NetDoorCompositeState door_composite_state; /* 0 NORMAL, 1 INCONSISTENT, 2 UNTRUSTED */
    NetValueQuality lock_quality;
    NetValueQuality open_quality;
    NetValueQuality composite_quality;
    uint32_t update_sequence;           /* door state evaluation number */
    uint16_t source_age_ms;             /* since the evaluation */
} NetBcmDoorState;

typedef struct NetBcmClimateState
{
    NetFanLevel fan_command_level;      /* 0 OFF, 1 LOW, 2 MEDIUM, 3 HIGH */
    NetFanLevel fan_measured_level;     /* FAN code and 255 UNKNOWN */
    NetThermalDirection thermal_direction_state; /* 0 IDLE, 1 COOL, 2 HEAT */
    uint8_t thermal_output_percent;     /* 0..100 %, 1 %/raw */
    NetHeatRemovalState heat_removal_state; /* 0 normal, 1 overheat cut-off, 255 unavailable */
    NetValueQuality fan_command_quality;
    NetValueQuality fan_measured_quality;
    NetValueQuality thermal_direction_quality;
    NetValueQuality thermal_output_quality;
    NetValueQuality heat_removal_quality;
    uint32_t update_sequence;           /* climate state evaluation number */
    uint16_t source_age_ms;             /* since the evaluation */
} NetBcmClimateState;

typedef struct NetBcmLightState
{
    NetLightType interior_light_type_state; /* LIGHT_TYPE code */
    uint8_t light_level_percent;        /* 0..100 % */
    NetRgb interior_light_color_state;  /* R/G/B 0..255 each */
    NetLightApplyResult interior_light_apply_result; /* 0 applied, 1 failed, 255 UNKNOWN */
    NetFlag light_physical_feedback_capability; /* 0 not supported, 1 supported; currently 0 */
    NetValueQuality light_type_quality;
    NetValueQuality light_level_quality;
    NetValueQuality light_color_quality;
    NetValueQuality light_apply_quality;
    uint32_t update_sequence;           /* light apply state evaluation number */
    uint16_t source_age_ms;             /* since the evaluation */
} NetBcmLightState;

typedef struct NetBcmResult
{
    uint32_t command_domain_boot;       /* DOMAIN boot that created the original command */
    uint32_t command_id;                /* DOMAIN command id processed */
    NetBcmCommandKind command_kind;     /* 1 door, 2 climate, 3 light */
    NetVehicleResult result;
    NetReason result_reason;
    NetDoorEvent door_event;            /* 0 none .. 5 REQUEST_FAILED */
    uint32_t occurrence_id;             /* door event number; 0 when none */
    uint16_t event_age_ms;              /* since the event */
    NetValueQuality event_quality;
    NetDoorTarget door_target;          /* 0 LOCK, 1 UNLOCK, 255 not a door command */
} NetBcmResult;

typedef struct NetBcmEvent
{
    NetBcmEventKind event_kind;         /* 1 OVERHEAT, 2 FAN_MISMATCH, 3 RECOVERY_CONFIRMED */
    uint32_t occurrence_id;             /* event identity */
    uint16_t event_age_ms;              /* since the occurrence */
    NetValueQuality event_quality;
    uint32_t related_domain_boot;       /* DOMAIN boot of the related command; 0 none */
    uint32_t related_command_id;        /* related command; 0 none */
    uint8_t affected_function;          /* BCM function bits */
} NetBcmEvent;

typedef struct NetBcmStatus
{
    NetControllerState bcm_ecu_state;   /* 0 INIT, 1 READY, 2 DEGRADED, 3 FAULT */
    uint16_t fault_mask;                /* NetBcmFaultMask bits 0..7 */
    NetFaultCategory active_fault_category; /* 0 none, 1 SENSOR, 2 FUNCTION, 3 COMM */
    uint16_t active_fault_code;         /* main current fault: bit number + 1 */
    uint8_t affected_function;          /* bit0 door, bit1 climate, bit2 light */
    NetFlag recovering;
    uint32_t update_sequence;           /* status evaluation number, not CAN TX_SEQUENCE */
    uint16_t source_age_ms;             /* since the evaluation */
} NetBcmStatus;

/* Implemented by the BCM application. */
void BcmApp_OnDomainDoorCommand(const NetBcmDoorCommand *message, const NetCanRxMeta *meta);
void BcmApp_OnDomainClimateCommand(const NetBcmClimateCommand *message, const NetCanRxMeta *meta);
void BcmApp_OnDomainLightCommand(const NetBcmLightCommand *message, const NetCanRxMeta *meta);

/* Implemented by the Network. Serialized superloop context only, never an ISR. */
NetSubmitResult BcmNet_SendResult(const NetBcmResult *message);
NetSubmitResult BcmNet_SendEvent(const NetBcmEvent *message);
NetSubmitResult BcmNet_PublishDoorState(const NetBcmDoorState *message);
NetSubmitResult BcmNet_PublishClimateState(const NetBcmClimateState *message);
NetSubmitResult BcmNet_PublishLightState(const NetBcmLightState *message);
NetSubmitResult BcmNet_PublishStatus(const NetBcmStatus *message);

/* NETWORK_PRIVATE (see above). */
void BcmNetPriv_OnDomainPermission(const NetDomainPermission *permission, const NetCanRxMeta *meta);

#endif
