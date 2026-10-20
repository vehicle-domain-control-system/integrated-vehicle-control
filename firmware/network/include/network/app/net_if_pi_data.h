#ifndef NETWORK_APP_NET_IF_PI_DATA_H
#define NETWORK_APP_NET_IF_PI_DATA_H

/* UART-C records owned by the Pi, transcribed from the interface contract:
 *   docs/network/interface/02_NODES/PI/10_DATA.md
 * The CIS data contract (02_NODES/CIS/10_DATA.md) defines these once, in the
 * Pi record, and the CIS Application Port uses them as they are. Field order
 * and codes are the document's; conversion from the wire DTO is field by
 * field in the Network facade. */
#include "network/app/net_if_common.h"

typedef uint16_t NetVisionFaultCode;
enum NetVisionFaultCodeCode
{
    NET_VISION_FAULT_NONE = 0,
    NET_VISION_FAULT_INITIALIZATION = 1,
    NET_VISION_FAULT_CAMERA = 2,
    NET_VISION_FAULT_INFERENCE = 3,
    NET_VISION_FAULT_DATA_INVALID = 4
};

typedef struct NetVisionResult
{
    uint32_t pi_boot_id;            /* Pi runtime boot, kept as received */
    uint32_t vision_sequence;       /* new camera judgement number */
    uint16_t vision_age_ms;         /* age of the judgement (RX basis: at meta.received_at_ms) */
    NetTriState occupant_presence;  /* 0 none, 1 present, 255 unknown */
    NetValidity presence_validity;
    NetQualityReason presence_reason;
    uint8_t occupant_count;         /* 0..5, 255 unknown */
    NetValidity count_validity;
    NetQualityReason count_reason;
    NetSensorFunctionState vision_status; /* Pi vision function state */
    NetVisionFaultCode vision_fault_code;
} NetVisionResult;

typedef struct NetVisionStatus
{
    uint32_t pi_boot_id;
    uint32_t status_sequence;       /* new function-state evaluation number */
    NetSensorFunctionState vision_status;
    NetVisionFaultCode active_fault;
    NetVisionFaultCode last_fault;  /* most recent fault since the Pi boot */
    uint16_t status_age_ms;         /* RX basis: at meta.received_at_ms */
    uint8_t affected_function;      /* bit0 occupant presence, bit1 occupant count */
} NetVisionStatus;

typedef struct NetPiPermission
{
    uint32_t cis_boot_id;           /* CIS relay generation, filled by the CIS Network */
    uint32_t update_sequence;       /* relay evaluation number, kept on repetitions */
    NetTriState vehicle_power_permission; /* 0 denied, 1 allowed, 255 unknown */
    NetValueQuality value_quality;
    NetQualityReason quality_reason;
    uint16_t use_limit_ms;          /* draft 1500 ms */
    uint16_t source_age_ms;         /* age since the DOMAIN decision */
} NetPiPermission;

#endif
