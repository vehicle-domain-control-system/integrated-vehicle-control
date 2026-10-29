#ifndef NETWORK_NET_WIRE_CAN_H
#define NETWORK_NET_WIRE_CAN_H

#include "network/net_wire.h"

/* Explicit captured CAN boundary; a NetFrame without flags is insufficient.
 * The array owns its bytes. Bytes after length are outside the wire frame.
 * driver_error represents a driver-reported invalid frame, not an app CRC. */
typedef struct {
    uint32_t standard_id;
    bool extended;
    bool fdf;
    bool brs;
    bool rtr;
    bool driver_error;
    bool esi; /* Captured CAN FD error-state indicator; not a payload validity verdict. */
    uint8_t dlc;
    size_t length;
    uint8_t bytes[64];
} NetWireCanEnvelope;

/* Candidate raw DTOs, NOT C2/product ABI. Metadata belongs to this wire copy.
 * No sequence/BOOT admission, clock, TTL, permission or liveness policy. */
typedef struct {
    uint32_t boot_id;
    uint32_t tx_sequence;
    uint32_t update_sequence;
    uint16_t source_age_ms;
    uint16_t rear_distance_deci_cm; /* 1 raw = 1 mm; UINT16_MAX is not distance. */
    uint8_t validity;
    uint8_t quality_reason;
    uint8_t proximity_status;
} NetWireCisRearRaw;

typedef struct {
    uint32_t boot_id;
    uint32_t tx_sequence;
    uint8_t vehicle_power_permission;
    uint8_t power_permission_quality;
    uint8_t power_operation_state;
    uint8_t operation_allowed;
    uint8_t operation_quality;
    uint32_t update_sequence;
    uint16_t source_age_ms;
} NetWireDomainPermissionRaw;

/* Direction fixed by API: CIS_REAR is CIS -> DOMAIN; DOMAIN_PERMISSION is
 * DOMAIN -> BCM/CIS/WINDOW/VSS. No runtime delivery/receiver binding occurs.
 * Every failure preserves output bytes. NULL or ANY input/output overlap is
 * INVALID_ARGUMENT. An unselected profile is NOT_BOUND, never automatic opt-in.
 * OK means only candidate raw structure validation; product use remains HOLD. */
NetWireStatus net_wire_can_encode_rear(NetWireProfile profile,
    const NetWireCisRearRaw *input, NetWireCanEnvelope *output);
NetWireStatus net_wire_can_decode_rear(NetWireProfile profile,
    const NetWireCanEnvelope *input, NetWireCisRearRaw *output);
NetWireStatus net_wire_can_encode_permission(NetWireProfile profile,
    const NetWireDomainPermissionRaw *input, NetWireCanEnvelope *output);
NetWireStatus net_wire_can_decode_permission(NetWireProfile profile,
    const NetWireCanEnvelope *input, NetWireDomainPermissionRaw *output);

/* Wave2A raw candidate declarations. They do not enable node execution. */
#include "network/net_wire_can_bcm.h"
#include "network/net_wire_can_nodes.h"

#endif
