#ifndef NETWORK_APP_NET_IF_COMMON_H
#define NETWORK_APP_NET_IF_COMMON_H

/* Application Port common types, transcribed from the interface contract:
 *   docs/network/interface/04_COMMON/10_DATA_AND_ID_RULES.md
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

/* Fixed-width invalid values (interface/04_COMMON/10_DATA_AND_ID_RULES.md,
 * "고정 폭 무효값"). */
enum { NET_AVAILABILITY_ITEM_COUNT = 9 };
#define NET_AGE_SATURATED_MS UINT16_MAX
#define NET_UNKNOWN_U8 UINT8_MAX
#define NET_INVALID_TEMPERATURE INT16_MIN
#define NET_INVALID_U16 UINT16_MAX
#define NET_INVALID_ILLUMINANCE UINT32_MAX

#endif
