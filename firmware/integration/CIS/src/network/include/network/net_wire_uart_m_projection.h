#ifndef NETWORK_NET_WIRE_UART_M_PROJECTION_H
#define NETWORK_NET_WIRE_UART_M_PROJECTION_H
#include "network/net_wire_uart.h"

/* Host-only raw candidates; never C2 ABI or an origin-join/runtime API.
 * DOWN_CONTEXT, source CAN identity and Pi observation identity stay separate.
 * Every AGE/sequence is raw; no freshness, TTL, boot/session admission or
 * product WINDOW/permission behavior is inferred. Struct padding is not wire.
 * IN/OUT and both decode OUT ranges must be disjoint, including partial
 * overlap. Every failure leaves all OUT bytes unchanged. */

typedef struct {
    uint32_t device_context_id; /* payload B0 */
    uint64_t session_id; /* payload B4 */
    uint32_t domain_boot_id; /* payload B12 */
    uint32_t query_id; /* payload B16 */
    uint32_t original_can_boot_id; /* payload B20 */
    uint32_t original_can_tx_sequence; /* payload B24 */
    uint8_t door_lock_state; /* payload B28 */
    uint8_t door_open_state; /* payload B29 */
    uint8_t door_composite_state; /* payload B30 */
    uint8_t lock_quality; /* payload B31 */
    uint8_t open_quality; /* payload B32 */
    uint8_t composite_quality; /* payload B33 */
    uint32_t update_sequence; /* payload B34 */
    uint16_t source_age_ms; /* payload B38 */
} NetWireMBcmDoorStateRaw;

NetWireStatus net_wire_m_bcm_door_state_encode(NetWireProfile profile,
    uint16_t link_sequence, const NetWireMBcmDoorStateRaw *message, NetWireUartFrame *out);
NetWireStatus net_wire_m_bcm_door_state_decode(NetWireProfile profile,
    const NetWireUartFrame *frame, NetWireMBcmDoorStateRaw *out, uint16_t *link_sequence_out);

typedef struct {
    uint32_t device_context_id; /* payload B0 */
    uint64_t session_id; /* payload B4 */
    uint32_t domain_boot_id; /* payload B12 */
    uint32_t query_id; /* payload B16 */
    uint32_t original_can_boot_id; /* payload B20 */
    uint32_t original_can_tx_sequence; /* payload B24 */
    uint8_t fan_command_level; /* payload B28 */
    uint8_t fan_measured_level; /* payload B29 */
    uint8_t thermal_direction_state; /* payload B30 */
    uint8_t thermal_output_percent; /* payload B31 */
    uint8_t heat_removal_state; /* payload B32 */
    uint8_t fan_command_quality; /* payload B33 */
    uint8_t fan_measured_quality; /* payload B34 */
    uint8_t thermal_direction_quality; /* payload B35 */
    uint8_t thermal_output_quality; /* payload B36 */
    uint8_t heat_removal_quality; /* payload B37 */
    uint32_t update_sequence; /* payload B38 */
    uint16_t source_age_ms; /* payload B42 */
} NetWireMBcmClimateStateRaw;

NetWireStatus net_wire_m_bcm_climate_state_encode(NetWireProfile profile,
    uint16_t link_sequence, const NetWireMBcmClimateStateRaw *message, NetWireUartFrame *out);
NetWireStatus net_wire_m_bcm_climate_state_decode(NetWireProfile profile,
    const NetWireUartFrame *frame, NetWireMBcmClimateStateRaw *out, uint16_t *link_sequence_out);

typedef struct {
    uint32_t device_context_id; /* payload B0 */
    uint64_t session_id; /* payload B4 */
    uint32_t domain_boot_id; /* payload B12 */
    uint32_t query_id; /* payload B16 */
    uint32_t original_can_boot_id; /* payload B20 */
    uint32_t original_can_tx_sequence; /* payload B24 */
    uint8_t interior_light_type_state; /* payload B28 */
    uint8_t light_level_percent; /* payload B29 */
    uint8_t color_r; /* payload B30 */
    uint8_t color_g; /* payload B31 */
    uint8_t color_b; /* payload B32 */
    uint8_t interior_light_apply_result; /* payload B33 */
    uint8_t light_physical_feedback_capability; /* payload B34 */
    uint8_t light_type_quality; /* payload B35 */
    uint8_t light_level_quality; /* payload B36 */
    uint8_t light_color_quality; /* payload B37 */
    uint8_t light_apply_quality; /* payload B38 */
    uint32_t update_sequence; /* payload B39 */
    uint16_t source_age_ms; /* payload B43 */
} NetWireMBcmLightStateRaw;

NetWireStatus net_wire_m_bcm_light_state_encode(NetWireProfile profile,
    uint16_t link_sequence, const NetWireMBcmLightStateRaw *message, NetWireUartFrame *out);
NetWireStatus net_wire_m_bcm_light_state_decode(NetWireProfile profile,
    const NetWireUartFrame *frame, NetWireMBcmLightStateRaw *out, uint16_t *link_sequence_out);

typedef struct {
    uint32_t device_context_id; /* payload B0 */
    uint64_t session_id; /* payload B4 */
    uint32_t domain_boot_id; /* payload B12 */
    uint32_t query_id; /* payload B16 */
    uint32_t original_can_boot_id; /* payload B20 */
    uint32_t original_can_tx_sequence; /* payload B24 */
    uint8_t bcm_ecu_state; /* payload B28 */
    uint16_t fault_mask; /* payload B29 */
    uint8_t active_fault_category; /* payload B31 */
    uint16_t active_fault_code; /* payload B32 */
    uint8_t affected_function; /* payload B34 */
    uint8_t recovering; /* payload B35 */
    uint32_t update_sequence; /* payload B36 */
    uint16_t source_age_ms; /* payload B40 */
} NetWireMBcmStatusRaw;

NetWireStatus net_wire_m_bcm_status_encode(NetWireProfile profile,
    uint16_t link_sequence, const NetWireMBcmStatusRaw *message, NetWireUartFrame *out);
NetWireStatus net_wire_m_bcm_status_decode(NetWireProfile profile,
    const NetWireUartFrame *frame, NetWireMBcmStatusRaw *out, uint16_t *link_sequence_out);

typedef struct {
    uint32_t device_context_id; /* payload B0 */
    uint64_t session_id; /* payload B4 */
    uint32_t domain_boot_id; /* payload B12 */
    uint32_t query_id; /* payload B16 */
    uint32_t original_can_boot_id; /* payload B20 */
    uint32_t original_can_tx_sequence; /* payload B24 */
    int16_t temperature_centi_c; /* payload B28 */
    uint32_t temperature_sequence; /* payload B30 */
    uint16_t temperature_age_ms; /* payload B34 */
    uint8_t temperature_validity; /* payload B36 */
    uint8_t temperature_reason; /* payload B37 */
    uint16_t humidity_centi_percent; /* payload B38 */
    uint32_t humidity_sequence; /* payload B40 */
    uint16_t humidity_age_ms; /* payload B44 */
    uint8_t humidity_validity; /* payload B46 */
    uint8_t humidity_reason; /* payload B47 */
    uint32_t illuminance_lux; /* payload B48 */
    uint32_t illuminance_sequence; /* payload B52 */
    uint16_t illuminance_age_ms; /* payload B56 */
    uint8_t illuminance_validity; /* payload B58 */
    uint8_t illuminance_reason; /* payload B59 */
} NetWireMCisEnvironmentRaw;

NetWireStatus net_wire_m_cis_environment_encode(NetWireProfile profile,
    uint16_t link_sequence, const NetWireMCisEnvironmentRaw *message, NetWireUartFrame *out);
NetWireStatus net_wire_m_cis_environment_decode(NetWireProfile profile,
    const NetWireUartFrame *frame, NetWireMCisEnvironmentRaw *out, uint16_t *link_sequence_out);

typedef struct {
    uint32_t device_context_id; /* payload B0 */
    uint64_t session_id; /* payload B4 */
    uint32_t domain_boot_id; /* payload B12 */
    uint32_t query_id; /* payload B16 */
    uint32_t original_can_boot_id; /* payload B20 */
    uint32_t original_can_tx_sequence; /* payload B24 */
    uint32_t pi_boot_id; /* payload B28 */
    uint32_t vision_sequence; /* payload B32 */
    uint16_t vision_age_ms; /* payload B36 */
    uint8_t occupant_presence; /* payload B38 */
    uint8_t presence_validity; /* payload B39 */
    uint8_t presence_reason; /* payload B40 */
    uint8_t occupant_count; /* payload B41 */
    uint8_t count_validity; /* payload B42 */
    uint8_t count_reason; /* payload B43 */
    uint8_t vision_function_status; /* payload B44 */
} NetWireMCisOccupantRaw;

NetWireStatus net_wire_m_cis_occupant_encode(NetWireProfile profile,
    uint16_t link_sequence, const NetWireMCisOccupantRaw *message, NetWireUartFrame *out);
NetWireStatus net_wire_m_cis_occupant_decode(NetWireProfile profile,
    const NetWireUartFrame *frame, NetWireMCisOccupantRaw *out, uint16_t *link_sequence_out);

typedef struct {
    uint32_t device_context_id; /* payload B0 */
    uint64_t session_id; /* payload B4 */
    uint32_t domain_boot_id; /* payload B12 */
    uint32_t query_id; /* payload B16 */
    uint32_t original_can_boot_id; /* payload B20 */
    uint32_t original_can_tx_sequence; /* payload B24 */
    uint32_t update_sequence; /* payload B28 */
    uint16_t source_age_ms; /* payload B32 */
    uint16_t rear_distance_deci_cm; /* payload B34 */
    uint8_t validity; /* payload B36 */
    uint8_t quality_reason; /* payload B37 */
    uint8_t proximity_status; /* payload B38 */
} NetWireMCisRearRaw;

NetWireStatus net_wire_m_cis_rear_encode(NetWireProfile profile,
    uint16_t link_sequence, const NetWireMCisRearRaw *message, NetWireUartFrame *out);
NetWireStatus net_wire_m_cis_rear_decode(NetWireProfile profile,
    const NetWireUartFrame *frame, NetWireMCisRearRaw *out, uint16_t *link_sequence_out);

typedef struct {
    uint32_t device_context_id; /* payload B0 */
    uint64_t session_id; /* payload B4 */
    uint32_t domain_boot_id; /* payload B12 */
    uint32_t query_id; /* payload B16 */
    uint32_t original_can_boot_id; /* payload B20 */
    uint32_t original_can_tx_sequence; /* payload B24 */
    uint8_t cis_state; /* payload B28 */
    uint8_t vision_status; /* payload B29 */
    uint8_t temperature_status; /* payload B30 */
    uint8_t humidity_status; /* payload B31 */
    uint8_t illuminance_status; /* payload B32 */
    uint8_t rear_status; /* payload B33 */
    uint8_t pi_uart_status; /* payload B34 */
    uint8_t can_interface_status; /* payload B35 */
    uint16_t fault_mask; /* payload B36 */
    uint16_t active_fault; /* payload B38 */
    uint8_t recovering; /* payload B40 */
    uint16_t last_fault; /* payload B41 */
    uint32_t last_fault_occurrence; /* payload B43 */
    uint8_t affected_function; /* payload B47 */
    uint32_t update_sequence; /* payload B48 */
    uint16_t source_age_ms; /* payload B52 */
} NetWireMCisStatusRaw;

NetWireStatus net_wire_m_cis_status_encode(NetWireProfile profile,
    uint16_t link_sequence, const NetWireMCisStatusRaw *message, NetWireUartFrame *out);
NetWireStatus net_wire_m_cis_status_decode(NetWireProfile profile,
    const NetWireUartFrame *frame, NetWireMCisStatusRaw *out, uint16_t *link_sequence_out);

typedef struct {
    uint32_t device_context_id; /* payload B0 */
    uint64_t session_id; /* payload B4 */
    uint32_t domain_boot_id; /* payload B12 */
    uint32_t query_id; /* payload B16 */
    uint32_t original_can_boot_id; /* payload B20 */
    uint32_t original_can_tx_sequence; /* payload B24 */
    uint8_t vss_state; /* payload B28 */
    uint8_t vss_availability; /* payload B29 */
    uint8_t vss_accepting_events; /* payload B30 */
    uint8_t vss_fault_active; /* payload B31 */
    uint8_t fault_mask; /* payload B32 */
    uint16_t vss_last_fault; /* payload B33 */
    uint32_t last_fault_occurrence; /* payload B35 */
    uint16_t last_fault_age_ms; /* payload B39 */
    uint8_t last_fault_quality; /* payload B41 */
    uint32_t update_sequence; /* payload B42 */
    uint16_t source_age_ms; /* payload B46 */
} NetWireMVssStatusRaw;

NetWireStatus net_wire_m_vss_status_encode(NetWireProfile profile,
    uint16_t link_sequence, const NetWireMVssStatusRaw *message, NetWireUartFrame *out);
NetWireStatus net_wire_m_vss_status_decode(NetWireProfile profile,
    const NetWireUartFrame *frame, NetWireMVssStatusRaw *out, uint16_t *link_sequence_out);

typedef struct {
    uint32_t device_context_id; /* payload B0 */
    uint64_t session_id; /* payload B4 */
    uint32_t domain_boot_id; /* payload B12 */
    uint32_t query_id; /* payload B16 */
    uint32_t original_can_boot_id; /* payload B20 */
    uint32_t original_can_tx_sequence; /* payload B24 */
    uint8_t motion_state; /* payload B28 */
    uint8_t window_ecu_state; /* payload B29 */
    uint8_t window_position; /* payload B30 */
    uint8_t window_value_quality; /* payload B31 */
    uint8_t fully_open_state; /* payload B32 */
    uint8_t fully_closed_state; /* payload B33 */
    uint8_t antipinch_protection_status; /* payload B34 */
    uint8_t reverse_state; /* payload B35 */
    uint32_t update_sequence; /* payload B36 */
    uint16_t source_age_ms; /* payload B40 */
} NetWireMWindowStateRaw;

NetWireStatus net_wire_m_window_state_encode(NetWireProfile profile,
    uint16_t link_sequence, const NetWireMWindowStateRaw *message, NetWireUartFrame *out);
NetWireStatus net_wire_m_window_state_decode(NetWireProfile profile,
    const NetWireUartFrame *frame, NetWireMWindowStateRaw *out, uint16_t *link_sequence_out);

typedef struct {
    uint32_t device_context_id; /* payload B0 */
    uint64_t session_id; /* payload B4 */
    uint32_t domain_boot_id; /* payload B12 */
    uint32_t query_id; /* payload B16 */
    uint32_t original_can_boot_id; /* payload B20 */
    uint32_t original_can_tx_sequence; /* payload B24 */
    uint8_t fault_category; /* payload B28 */
    uint16_t fault_code; /* payload B29 */
    uint8_t fault_status; /* payload B31 */
    uint32_t occurrence_id; /* payload B32 */
    uint32_t update_sequence; /* payload B36 */
    uint16_t source_age_ms; /* payload B40 */
} NetWireMWindowFaultRaw;

NetWireStatus net_wire_m_window_fault_encode(NetWireProfile profile,
    uint16_t link_sequence, const NetWireMWindowFaultRaw *message, NetWireUartFrame *out);
NetWireStatus net_wire_m_window_fault_decode(NetWireProfile profile,
    const NetWireUartFrame *frame, NetWireMWindowFaultRaw *out, uint16_t *link_sequence_out);

#endif
