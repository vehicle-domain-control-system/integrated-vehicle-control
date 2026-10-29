#ifndef NETWORK_NET_WIRE_CAN_NODES_H
#define NETWORK_NET_WIRE_CAN_NODES_H
#include "network/net_wire_can.h"

/* Host-only raw candidates; neither C2 ABI nor product execution APIs.
 * Fields are literal wire quantities. No AGE reset, identity allocation,
 * inference of permission, unit conversion, command dispatch or admission.
 * WINDOW_EXECUTION_HOLD applies to every WINDOW type, including raw VENT.
 * Failures preserve all output bytes; input/output overlap is rejected.
 * Only explicit NET_WIRE_CANDIDATE_ONLY opts into these byte contracts. */

typedef struct {
    uint32_t boot_id;
    uint32_t tx_sequence;
    uint32_t target_boot_id;
    uint32_t command_id;
    uint16_t command_age_ms;
    uint16_t accept_limit_ms;
    uint8_t window_command;
    uint8_t target_channel;
    uint8_t target_position;
    uint8_t source_context;
} NetWireWindowCommandRaw;

NetWireStatus net_wire_can_encode_window_command(NetWireProfile profile,
    const NetWireWindowCommandRaw *input, NetWireCanEnvelope *output);
NetWireStatus net_wire_can_decode_window_command(NetWireProfile profile,
    const NetWireCanEnvelope *input, NetWireWindowCommandRaw *output);

typedef struct {
    uint32_t boot_id;
    uint32_t tx_sequence;
    uint8_t motion_state;
    uint8_t window_ecu_state;
    uint8_t window_position;
    uint8_t window_value_quality;
    uint8_t fully_open_state;
    uint8_t fully_closed_state;
    uint8_t antipinch_protection_status;
    uint8_t reverse_state;
    uint32_t update_sequence;
    uint16_t source_age_ms;
} NetWireWindowStateRaw;

NetWireStatus net_wire_can_encode_window_state(NetWireProfile profile,
    const NetWireWindowStateRaw *input, NetWireCanEnvelope *output);
NetWireStatus net_wire_can_decode_window_state(NetWireProfile profile,
    const NetWireCanEnvelope *input, NetWireWindowStateRaw *output);

typedef struct {
    uint32_t boot_id;
    uint32_t tx_sequence;
    uint8_t event_kind;
    uint8_t target_channel;
    uint32_t occurrence_id;
    uint32_t related_domain_boot;
    uint32_t related_request_sequence;
    uint8_t origin;
    uint16_t event_age_ms;
    uint8_t event_quality;
} NetWireWindowEventRaw;

NetWireStatus net_wire_can_encode_window_event(NetWireProfile profile,
    const NetWireWindowEventRaw *input, NetWireCanEnvelope *output);
NetWireStatus net_wire_can_decode_window_event(NetWireProfile profile,
    const NetWireCanEnvelope *input, NetWireWindowEventRaw *output);

typedef struct {
    uint32_t boot_id;
    uint32_t tx_sequence;
    uint32_t request_domain_boot;
    uint32_t request_sequence;
    uint8_t target_channel;
    uint8_t result;
    uint16_t result_reason;
} NetWireWindowResultRaw;

NetWireStatus net_wire_can_encode_window_result(NetWireProfile profile,
    const NetWireWindowResultRaw *input, NetWireCanEnvelope *output);
NetWireStatus net_wire_can_decode_window_result(NetWireProfile profile,
    const NetWireCanEnvelope *input, NetWireWindowResultRaw *output);

typedef struct {
    uint32_t boot_id;
    uint32_t tx_sequence;
    uint8_t fault_category;
    uint16_t fault_code;
    uint8_t fault_status;
    uint32_t occurrence_id;
    uint32_t update_sequence;
    uint16_t source_age_ms;
} NetWireWindowFaultRaw;

NetWireStatus net_wire_can_encode_window_fault(NetWireProfile profile,
    const NetWireWindowFaultRaw *input, NetWireCanEnvelope *output);
NetWireStatus net_wire_can_decode_window_fault(NetWireProfile profile,
    const NetWireCanEnvelope *input, NetWireWindowFaultRaw *output);

typedef struct {
    uint32_t boot_id;
    uint32_t tx_sequence;
    int16_t temperature_centi_c;
    uint32_t temperature_sequence;
    uint16_t temperature_age_ms;
    uint8_t temperature_validity;
    uint8_t temperature_reason;
    uint16_t humidity_centi_percent;
    uint32_t humidity_sequence;
    uint16_t humidity_age_ms;
    uint8_t humidity_validity;
    uint8_t humidity_reason;
    uint32_t illuminance_lux;
    uint32_t illuminance_sequence;
    uint16_t illuminance_age_ms;
    uint8_t illuminance_validity;
    uint8_t illuminance_reason;
} NetWireCisEnvironmentRaw;

NetWireStatus net_wire_can_encode_cis_environment(NetWireProfile profile,
    const NetWireCisEnvironmentRaw *input, NetWireCanEnvelope *output);
NetWireStatus net_wire_can_decode_cis_environment(NetWireProfile profile,
    const NetWireCanEnvelope *input, NetWireCisEnvironmentRaw *output);

typedef struct {
    uint32_t boot_id;
    uint32_t tx_sequence;
    uint32_t pi_boot_id;
    uint32_t vision_sequence;
    uint16_t vision_age_ms;
    uint8_t occupant_presence;
    uint8_t presence_validity;
    uint8_t presence_reason;
    uint8_t occupant_count;
    uint8_t count_validity;
    uint8_t count_reason;
    uint8_t vision_function_status;
} NetWireCisOccupantRaw;

NetWireStatus net_wire_can_encode_cis_occupant(NetWireProfile profile,
    const NetWireCisOccupantRaw *input, NetWireCanEnvelope *output);
NetWireStatus net_wire_can_decode_cis_occupant(NetWireProfile profile,
    const NetWireCanEnvelope *input, NetWireCisOccupantRaw *output);

typedef struct {
    uint32_t boot_id;
    uint32_t tx_sequence;
    uint8_t cis_state;
    uint8_t vision_status;
    uint8_t temperature_status;
    uint8_t humidity_status;
    uint8_t illuminance_status;
    uint8_t rear_status;
    uint8_t pi_uart_status;
    uint8_t can_interface_status;
    uint16_t fault_mask;
    uint16_t active_fault;
    uint8_t recovering;
    uint16_t last_fault;
    uint32_t last_fault_occurrence;
    uint8_t affected_function;
    uint32_t update_sequence;
    uint16_t source_age_ms;
} NetWireCisStatusRaw;

NetWireStatus net_wire_can_encode_cis_status(NetWireProfile profile,
    const NetWireCisStatusRaw *input, NetWireCanEnvelope *output);
NetWireStatus net_wire_can_decode_cis_status(NetWireProfile profile,
    const NetWireCanEnvelope *input, NetWireCisStatusRaw *output);

typedef struct {
    uint32_t boot_id;
    uint32_t tx_sequence;
    uint8_t event_kind;
    uint32_t occurrence_id;
    uint16_t original_age_ms;
    uint16_t use_limit_ms;
    uint8_t value_quality;
    uint8_t input_availability;
    uint32_t related_command_id;
} NetWireVssEventRaw;

NetWireStatus net_wire_can_encode_vss_event(NetWireProfile profile,
    const NetWireVssEventRaw *input, NetWireCanEnvelope *output);
NetWireStatus net_wire_can_decode_vss_event(NetWireProfile profile,
    const NetWireCanEnvelope *input, NetWireVssEventRaw *output);

typedef struct {
    uint32_t boot_id;
    uint32_t tx_sequence;
    uint32_t antipinch_source_boot;
    uint32_t antipinch_source_sequence;
    uint16_t antipinch_original_age_ms;
    uint8_t antipinch_state;
    uint8_t antipinch_quality;
    uint8_t antipinch_reason;
    uint8_t antipinch_availability;
    uint32_t occupant_hazard_source_boot;
    uint32_t occupant_hazard_source_sequence;
    uint16_t occupant_hazard_original_age_ms;
    uint8_t occupant_hazard_state;
    uint8_t occupant_hazard_quality;
    uint8_t occupant_hazard_reason;
    uint8_t occupant_hazard_availability;
    uint32_t rear_obstacle_source_boot;
    uint32_t rear_obstacle_source_sequence;
    uint16_t rear_obstacle_original_age_ms;
    uint8_t rear_obstacle_state;
    uint8_t rear_obstacle_quality;
    uint8_t rear_obstacle_reason;
    uint8_t rear_obstacle_availability;
    uint8_t rear_detection_activation;
    uint8_t activation_meta_format;
    uint8_t rear_activation_quality;
    uint8_t rear_activation_reason;
    uint32_t rear_activation_sequence;
    uint16_t rear_activation_age_ms;
} NetWireVssWarningStateRaw;

NetWireStatus net_wire_can_encode_vss_warning_state(NetWireProfile profile,
    const NetWireVssWarningStateRaw *input, NetWireCanEnvelope *output);
NetWireStatus net_wire_can_decode_vss_warning_state(NetWireProfile profile,
    const NetWireCanEnvelope *input, NetWireVssWarningStateRaw *output);

typedef struct {
    uint32_t boot_id;
    uint32_t tx_sequence;
    uint8_t vss_state;
    uint8_t vss_availability;
    uint8_t vss_accepting_events;
    uint8_t vss_fault_active;
    uint8_t fault_mask;
    uint16_t vss_last_fault;
    uint32_t last_fault_occurrence;
    uint16_t last_fault_age_ms;
    uint8_t last_fault_quality;
    uint32_t update_sequence;
    uint16_t source_age_ms;
} NetWireVssStatusRaw;

NetWireStatus net_wire_can_encode_vss_status(NetWireProfile profile,
    const NetWireVssStatusRaw *input, NetWireCanEnvelope *output);
NetWireStatus net_wire_can_decode_vss_status(NetWireProfile profile,
    const NetWireCanEnvelope *input, NetWireVssStatusRaw *output);

#endif
