#ifndef NETWORK_NET_UPPER_CAN_H
#define NETWORK_NET_UPPER_CAN_H
#include "network/net_wire_can.h"
/* Private Network typed routing boundary. External Owner ABI remains HOLD. */
typedef struct {
    uint16_t id;
    union {
        NetWireDomainPermissionRaw domain_permission;
        NetWireBcmDoorCommandRaw bcm_door_command;
        NetWireBcmClimateCommandRaw bcm_climate_command;
        NetWireBcmLightCommandRaw bcm_light_command;
        NetWireWindowCommandRaw window_command;
        NetWireBcmDoorStateRaw bcm_door_state;
        NetWireBcmClimateStateRaw bcm_climate_state;
        NetWireBcmLightStateRaw bcm_light_state;
        NetWireBcmResultRaw bcm_result;
        NetWireBcmEventRaw bcm_event;
        NetWireBcmStatusRaw bcm_status;
        NetWireCisEnvironmentRaw cis_environment;
        NetWireCisOccupantRaw cis_occupant;
        NetWireCisRearRaw cis_rear;
        NetWireCisStatusRaw cis_status;
        NetWireWindowStateRaw window_state;
        NetWireWindowEventRaw window_event;
        NetWireWindowResultRaw window_result;
        NetWireWindowFaultRaw window_fault;
        NetWireVssEventRaw vss_event;
        NetWireVssWarningStateRaw vss_warning_state;
        NetWireVssStatusRaw vss_status;
    } data;
} NetUpperCanMessage;
NetWireStatus net_upper_can_encode(NetWireProfile profile, const NetUpperCanMessage *message, NetWireCanEnvelope *out);
NetWireStatus net_upper_can_decode(NetWireProfile profile, const NetWireCanEnvelope *frame, NetUpperCanMessage *out);
NetWireStatus net_upper_can_add_age(NetUpperCanMessage *message, uint64_t elapsed_ms);
NetWireStatus net_upper_can_identity(const NetUpperCanMessage *message, uint32_t *boot, uint32_t *sequence);
/* Writes the common CAN header (BOOT_ID, TX_SEQUENCE) of any of the 22 typed
 * messages; every other field is unchanged. Used by the M04 publish slots,
 * which own both values per frame (net_upper_publish.h). */
NetWireStatus net_upper_can_set_identity(NetUpperCanMessage *message, uint32_t boot, uint32_t sequence);
uint16_t net_upper_can_max_age(const NetUpperCanMessage *message);
#endif
