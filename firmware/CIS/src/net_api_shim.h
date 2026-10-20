/*
 * Temporary copy of the Network interface declarations the CIS uses
 * (docs/network/interface/02_NODES/CIS/10_DATA.md and 20_API.md, Interface v2).
 * Nothing here is invented: when the Network stack delivers its real header, delete this
 * file and include the real one in cis_net_adapter.c.
 */
#ifndef NET_API_SHIM_H
#define NET_API_SHIM_H

#include <stdint.h>

typedef uint8_t NetValidity;                /* 0 INVALID, 1 VALID */
typedef uint8_t NetQualityReason;           /* 0 NONE .. 9 DATA_INVALID (same numbers as cis_reason_t) */
typedef uint8_t NetTriState;                /* 0 no, 1 yes, 255 unknown */
typedef uint8_t NetFlag;                    /* 0 no, 1 yes */
typedef uint8_t NetSensorFunctionState;     /* 0 NOT_READY .. 4 RECOVERING */
typedef uint8_t NetProximityStatus;         /* 0 VALID_DISTANCE .. 5 INACTIVE */
typedef uint8_t NetCisState;                /* 0 STARTUP .. 3 FAULT */
typedef uint8_t NetInterfaceState;          /* 0 READY, 1 UNAVAILABLE, 2 RECOVERING */
typedef uint8_t NetValueQuality;            /* 0 OK, 1 STALE, 2 INVALID, 3 NO_DATA */
typedef uint8_t NetCanSource;               /* [TBD] declared by the Network stack */
typedef uint8_t NetVisionFaultCode;

#define NET_VALIDITY_INVALID    0u
#define NET_VALIDITY_VALID      1u
#define NET_TRISTATE_UNKNOWN    255u

typedef enum NetSubmitResult {
    NET_SUBMIT_OK = 0,
    NET_SUBMIT_INVALID_ARGUMENT = 1,
    NET_SUBMIT_NOT_READY = 2,
    NET_SUBMIT_NO_CAPACITY = 3
} NetSubmitResult;

typedef struct NetCanRxMeta {
    NetCanSource source;
    uint32_t received_at_ms;
} NetCanRxMeta;

typedef struct NetUartRxMeta {
    uint16_t link_sequence;
    uint32_t received_at_ms;
} NetUartRxMeta;

typedef struct NetDomainPermission {
    NetTriState vehicle_power_permission;   /* 0 deny, 1 allow, 255 unknown */
    NetValueQuality power_permission_quality;
    NetTriState power_operation_state;
    NetFlag operation_allowed;
    NetValueQuality operation_quality;
    uint32_t update_sequence;
    uint16_t source_age_ms;
} NetDomainPermission;

typedef struct NetVisionResult {
    uint32_t pi_boot_id;
    uint32_t vision_sequence;
    uint16_t vision_age_ms;
    NetTriState occupant_presence;
    NetValidity presence_validity;
    NetQualityReason presence_reason;
    uint8_t occupant_count;
    NetValidity count_validity;
    NetQualityReason count_reason;
    NetSensorFunctionState vision_status;
    NetVisionFaultCode vision_fault_code;
} NetVisionResult;

typedef struct NetVisionStatus {
    uint32_t pi_boot_id;
    uint32_t status_sequence;
    NetSensorFunctionState vision_status;
    NetVisionFaultCode active_fault;
    NetVisionFaultCode last_fault;
    uint16_t status_age_ms;
    uint8_t affected_function;
} NetVisionStatus;

typedef struct NetCisEnvironment {
    int16_t temperature_centi_c;            /* -32768 = invalid */
    uint32_t temperature_sequence;
    uint16_t temperature_age_ms;
    NetValidity temperature_validity;
    NetQualityReason temperature_reason;
    uint16_t humidity_centi_percent;        /* 65535 = invalid */
    uint32_t humidity_sequence;
    uint16_t humidity_age_ms;
    NetValidity humidity_validity;
    NetQualityReason humidity_reason;
    uint32_t illuminance_lux;               /* 0xFFFFFFFF = invalid */
    uint32_t illuminance_sequence;
    uint16_t illuminance_age_ms;
    NetValidity illuminance_validity;
    NetQualityReason illuminance_reason;
} NetCisEnvironment;

typedef struct NetCisOccupant {
    uint32_t pi_boot_id;
    uint32_t vision_sequence;
    uint16_t vision_age_ms;
    NetTriState occupant_presence;
    NetValidity presence_validity;
    NetQualityReason presence_reason;
    uint8_t occupant_count;
    NetValidity count_validity;
    NetQualityReason count_reason;
    NetSensorFunctionState vision_function_status;
} NetCisOccupant;

typedef struct NetCisRear {
    uint32_t update_sequence;
    uint16_t source_age_ms;
    uint16_t rear_distance_deci_cm;         /* 0.1 cm per count (= 1 mm); 65535 = no distance */
    NetValidity validity;
    NetQualityReason quality_reason;
    NetProximityStatus proximity_status;
} NetCisRear;

typedef struct NetCisStatus {
    NetCisState cis_state;
    NetSensorFunctionState vision_status;
    NetSensorFunctionState temperature_status;
    NetSensorFunctionState humidity_status;
    NetSensorFunctionState illuminance_status;
    NetSensorFunctionState rear_status;
    NetInterfaceState pi_uart_status;
    NetInterfaceState can_interface_status;
    uint16_t fault_mask;
    uint16_t active_fault;
    NetFlag recovering;
    uint16_t last_fault;
    uint32_t last_fault_occurrence;
    uint8_t affected_function;
    uint32_t update_sequence;
    uint16_t source_age_ms;
} NetCisStatus;

/* Provided by the Network stack (CIS Network). */
NetSubmitResult CisNet_PublishRear(const NetCisRear *message);
NetSubmitResult CisNet_PublishEnvironment(const NetCisEnvironment *message);
NetSubmitResult CisNet_PublishOccupant(const NetCisOccupant *message);
NetSubmitResult CisNet_PublishStatus(const NetCisStatus *message);

/* Provided by the CIS (this adapter); called by the Network stack in the owner context. */
void CisApp_OnDomainPermission(const NetDomainPermission *message, const NetCanRxMeta *meta);
void CisApp_OnPiVisionResult(const NetVisionResult *message, const NetUartRxMeta *meta);
void CisApp_OnPiVisionStatus(const NetVisionStatus *message, const NetUartRxMeta *meta);

#endif /* NET_API_SHIM_H */
