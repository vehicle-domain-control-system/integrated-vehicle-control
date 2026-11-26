#ifndef NETWORK_APP_NET_IF_COMMON_H
#define NETWORK_APP_NET_IF_COMMON_H

/* Application Port common types, transcribed from the interface contract:
 *   docs/network/interface/04_COMMON/10_DATA_AND_ID_RULES.md
 * The common types the node headers of this firmware use (VSS, CIS, BCM, Pi
 * UART-C records). The DOMAIN-only types of that document (NetResultConfirmation,
 * NetWarningType, NetSeverity, NetFunctionId) are not in this node header.
 * These are the ONLY Network types an ECU application includes. Engine,
 * wire DTO (NetWire*Raw) and SDK types stay below this boundary.
 * Numeric codes equal the CAN wire codes; conversion is still field by field
 * (no struct memcpy, no enum cast between unrelated enums). */
#include <stdint.h>

typedef enum NetSubmitResult
{
    NET_SUBMIT_OK = 0,              /* bounded local copy accepted; NOT TX/peer/ECU DONE */
    NET_SUBMIT_INVALID_ARGUMENT = 1,/* NULL, reserved code, range/combination error */
    NET_SUBMIT_NOT_READY = 2,       /* link/context/owner not ready */
    NET_SUBMIT_NO_CAPACITY = 3      /* no bounded local slot; nothing overwritten */
} NetSubmitResult;

typedef struct NetCanSource
{
    uint32_t boot_id;     /* sender node boot number, 0 reserved */
    uint32_t tx_sequence; /* per-message transmission order, not an observation id */
} NetCanSource;

typedef struct NetCanRxMeta
{
    NetCanSource source;
    uint32_t received_at_ms; /* local monotonic ms of RX capture; never compared across ECUs */
} NetCanRxMeta;

/* UART receive meta (10_DATA_AND_ID_RULES.md NetUartRxMeta). Session and
 * request identities travel in the message's own context. */
typedef struct NetUartRxMeta
{
    uint16_t link_sequence;  /* per-direction transmission order, not a vehicle request id */
    uint32_t received_at_ms; /* local monotonic ms of RX capture (last byte of the frame) */
} NetUartRxMeta;

typedef uint8_t NetFlag;
enum NetFlagCode { NET_FLAG_NO = 0, NET_FLAG_YES = 1 };

typedef uint8_t NetTriState;
enum NetTriStateCode { NET_TRISTATE_NO = 0, NET_TRISTATE_YES = 1, NET_TRISTATE_UNKNOWN = 255 };

typedef uint8_t NetValueQuality;
enum NetValueQualityCode
{
    NET_QUALITY_OK = 0,
    NET_QUALITY_STALE = 1,
    NET_QUALITY_INVALID = 2,
    NET_QUALITY_NO_DATA = 3
};

typedef uint8_t NetQualityReason;
enum NetQualityReasonCode
{
    NET_QUALITY_REASON_NONE = 0,
    NET_QUALITY_REASON_NOT_READY = 1,
    NET_QUALITY_REASON_OUT_OF_RANGE = 2,
    NET_QUALITY_REASON_SENSOR_FAULT = 3,
    NET_QUALITY_REASON_VISION_FAULT = 4,
    NET_QUALITY_REASON_STALE = 5,
    NET_QUALITY_REASON_NO_DATA = 6,
    NET_QUALITY_REASON_COMMUNICATION_FAULT = 7,
    NET_QUALITY_REASON_RECOVERING = 8,
    NET_QUALITY_REASON_DATA_INVALID = 9
};

typedef uint8_t NetValidity;
enum NetValidityCode
{
    NET_VALIDITY_INVALID = 0,
    NET_VALIDITY_VALID = 1
};

/* CIS / Pi function state: readiness, not evidence of a new observation. */
typedef uint8_t NetSensorFunctionState;
enum NetSensorFunctionStateCode
{
    NET_SENSOR_FUNCTION_NOT_READY = 0,
    NET_SENSOR_FUNCTION_READY = 1,
    NET_SENSOR_FUNCTION_ACTIVE = 2,
    NET_SENSOR_FUNCTION_FAULT = 3,
    NET_SENSOR_FUNCTION_RECOVERING = 4
};

typedef uint8_t NetAvailability;
enum NetAvailabilityCode
{
    NET_AVAILABILITY_AVAILABLE = 0,
    NET_AVAILABILITY_LIMITED = 1,
    NET_AVAILABILITY_UNAVAILABLE = 2
};

typedef struct NetDomainPermission
{
    NetTriState vehicle_power_permission;
    NetValueQuality power_permission_quality;
    NetTriState power_operation_state;
    NetFlag operation_allowed;
    NetValueQuality operation_quality;
    uint32_t update_sequence;
    uint16_t source_age_ms;
} NetDomainPermission;

/* Command common part of a DOMAIN command (BCM / WINDOW). The CAN BOOT_ID /
 * TX_SEQUENCE of the frame are in NetCanRxMeta, not here. */
typedef struct NetCommandMeta
{
    uint32_t target_boot_id;  /* target ECU boot DOMAIN confirmed from its last state */
    uint32_t command_id;      /* original DOMAIN C2 decision id, kept on retransmission */
    uint16_t command_age_ms;  /* birth AGE plus Network wait / transfer delay */
    uint16_t accept_limit_ms; /* proposed acceptance bound (200 ms vs 1 s still open) */
} NetCommandMeta;

typedef struct NetRgb
{
    uint8_t r; /* 0..255 */
    uint8_t g; /* 0..255 */
    uint8_t b; /* 0..255 */
} NetRgb;

typedef uint8_t NetVehicleResult;
enum NetVehicleResultCode
{
    NET_RESULT_ACCEPTED = 0,
    NET_RESULT_IN_PROGRESS = 1,
    NET_RESULT_DONE = 2,
    NET_RESULT_REJECTED = 3,
    NET_RESULT_CANCELLED = 4,
    NET_RESULT_FAILED = 5,
    NET_RESULT_NOT_PRESENT = 255
};

typedef uint16_t NetReason;
enum NetReasonCode
{
    NET_REASON_NONE = 0,
    NET_REASON_NOT_REGISTERED = 1,
    NET_REASON_LINK_LOST = 2,
    NET_REASON_SESSION_INVALID = 3,
    NET_REASON_DOOR_OPEN = 4,
    NET_REASON_STATE_UNTRUSTED = 5,
    NET_REASON_CMD_INVALID = 6,
    NET_REASON_NO_FEEDBACK = 7,
    NET_REASON_DRIVE_LIMIT_EXCEEDED = 8,
    NET_REASON_ALREADY_AT_TARGET = 9,
    NET_REASON_LOCAL_OVERRIDE = 10,
    NET_REASON_STOP_REQUESTED = 11,
    NET_REASON_SUPERSEDED = 12,
    NET_REASON_ANTIPINCH = 13,
    NET_REASON_FAULT = 14,
    NET_REASON_STALE = 15,
    NET_REASON_NO_DATA = 16,
    NET_REASON_NOT_READY = 17,
    NET_REASON_OUT_OF_RANGE = 18,
    NET_REASON_SOURCE_FAILURE = 19,
    NET_REASON_UNKNOWN_REQUEST = 20,
    NET_REASON_EXPIRED = 21,
    NET_REASON_ID_CONFLICT = 22,
    NET_REASON_POWER_NOT_ALLOWED = 23,
    NET_REASON_OVERHEAT = 24,
    NET_REASON_FAN_MISMATCH = 25
};

typedef uint8_t NetFanLevel;
enum NetFanLevelCode
{
    NET_FAN_OFF = 0,
    NET_FAN_LOW = 1,
    NET_FAN_MEDIUM = 2,
    NET_FAN_HIGH = 3,
    NET_FAN_UNKNOWN = 255
};

typedef uint8_t NetThermalDirection;
enum NetThermalDirectionCode
{
    NET_THERMAL_IDLE = 0,
    NET_THERMAL_COOL = 1,
    NET_THERMAL_HEAT = 2
};

typedef uint8_t NetLightType;
enum NetLightTypeCode
{
    NET_LIGHT_NORMAL = 0,
    NET_LIGHT_GOODBYE = 1,
    NET_LIGHT_WARNING = 2,
    NET_LIGHT_FAULT = 3
};

typedef uint8_t NetOrigin;
enum NetOriginCode
{
    NET_ORIGIN_NONE = 0,
    NET_ORIGIN_MOBILE_MANUAL = 1,
    NET_ORIGIN_PROXIMITY_AUTO = 2,
    NET_ORIGIN_LOCAL = 3,
    NET_ORIGIN_TEST = 4,
    NET_ORIGIN_VEHICLE_POLICY = 5
};

typedef uint8_t NetFaultCategory;
enum NetFaultCategoryCode
{
    NET_FAULT_CATEGORY_NONE = 0,
    NET_FAULT_CATEGORY_SENSOR = 1,
    NET_FAULT_CATEGORY_FUNCTION = 2,
    NET_FAULT_CATEGORY_COMM = 3
};

typedef uint8_t NetControllerState;
enum NetControllerStateCode
{
    NET_CONTROLLER_INIT = 0,
    NET_CONTROLLER_READY = 1,
    NET_CONTROLLER_DEGRADED = 2,
    NET_CONTROLLER_FAULT = 3
};

/* Fixed-width invalid values (interface/04_COMMON/10_DATA_AND_ID_RULES.md,
 * "고정 폭 무효값"). */
enum { NET_AVAILABILITY_ITEM_COUNT = 9 };
#define NET_AGE_SATURATED_MS UINT16_MAX
#define NET_UNKNOWN_U8 UINT8_MAX
#define NET_INVALID_TEMPERATURE INT16_MIN
#define NET_INVALID_U16 UINT16_MAX
#define NET_INVALID_ILLUMINANCE UINT32_MAX

#endif
