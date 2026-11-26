#ifndef NETWORK_NET_WIRE_CAN_BCM_H
#define NETWORK_NET_WIRE_CAN_BCM_H
#include "network/net_wire_can.h"

/* Wave 2A BCM raw candidates only; these types are NOT product/C2 ABI.
 * boot/sequence, original command identity, AGE and scalar quality remain
 * independent. No runtime delivery, admission, acknowledgement, confirmation,
 * physical feedback, fault synthesis, execution or timing policy occurs here.
 * All functions reject NULL/exact/partial input-output overlap and preserve
 * every output byte on failure. Explicit CANDIDATE_ONLY profile is required.
 * RESULT 255 and unresolved optional identity combinations return HOLD.
 * OK proves raw candidate shape only; all actual bindings remain HOLD. */

/* 0x100; used 22 / frame 24 / DLC 12. */
typedef struct {
    uint32_t boot_id; /* B0..3 */
    uint32_t tx_sequence; /* B4..7 */
    uint32_t target_boot_id; /* B8..11 */
    uint32_t command_id; /* B12..15 */
    uint16_t command_age_ms; /* B16..17 */
    uint16_t accept_limit_ms; /* B18..19 */
    uint8_t door_lock_target; /* B20 */
    uint8_t origin; /* B21 */
} NetWireBcmDoorCommandRaw;
NetWireStatus net_wire_can_encode_bcm_door_command(NetWireProfile profile,
    const NetWireBcmDoorCommandRaw *input, NetWireCanEnvelope *output);
NetWireStatus net_wire_can_decode_bcm_door_command(NetWireProfile profile,
    const NetWireCanEnvelope *input, NetWireBcmDoorCommandRaw *output);

/* 0x110; used 23 / frame 24 / DLC 12. */
typedef struct {
    uint32_t boot_id; /* B0..3 */
    uint32_t tx_sequence; /* B4..7 */
    uint32_t target_boot_id; /* B8..11 */
    uint32_t command_id; /* B12..15 */
    uint16_t command_age_ms; /* B16..17 */
    uint16_t accept_limit_ms; /* B18..19 */
    uint8_t fan_target_level; /* B20 */
    uint8_t thermal_direction; /* B21 */
    uint8_t thermal_target_percent; /* B22 */
} NetWireBcmClimateCommandRaw;
NetWireStatus net_wire_can_encode_bcm_climate_command(NetWireProfile profile,
    const NetWireBcmClimateCommandRaw *input, NetWireCanEnvelope *output);
NetWireStatus net_wire_can_decode_bcm_climate_command(NetWireProfile profile,
    const NetWireCanEnvelope *input, NetWireBcmClimateCommandRaw *output);

/* 0x120; used 25 / frame 32 / DLC 13. */
typedef struct {
    uint32_t boot_id; /* B0..3 */
    uint32_t tx_sequence; /* B4..7 */
    uint32_t target_boot_id; /* B8..11 */
    uint32_t command_id; /* B12..15 */
    uint16_t command_age_ms; /* B16..17 */
    uint16_t accept_limit_ms; /* B18..19 */
    uint8_t interior_light_type; /* B20 */
    uint8_t light_level_percent; /* B21 */
    uint8_t red; /* B22 */
    uint8_t green; /* B23 */
    uint8_t blue; /* B24 */
} NetWireBcmLightCommandRaw;
NetWireStatus net_wire_can_encode_bcm_light_command(NetWireProfile profile,
    const NetWireBcmLightCommandRaw *input, NetWireCanEnvelope *output);
NetWireStatus net_wire_can_decode_bcm_light_command(NetWireProfile profile,
    const NetWireCanEnvelope *input, NetWireBcmLightCommandRaw *output);

/* 0x200; used 20 / frame 20 / DLC 11. */
typedef struct {
    uint32_t boot_id; /* B0..3 */
    uint32_t tx_sequence; /* B4..7 */
    uint8_t door_lock_state; /* B8 */
    uint8_t door_open_state; /* B9 */
    uint8_t door_composite_state; /* B10 */
    uint8_t lock_quality; /* B11 */
    uint8_t open_quality; /* B12 */
    uint8_t composite_quality; /* B13 */
    uint32_t update_sequence; /* B14..17 */
    uint16_t source_age_ms; /* B18..19 */
} NetWireBcmDoorStateRaw;
NetWireStatus net_wire_can_encode_bcm_door_state(NetWireProfile profile,
    const NetWireBcmDoorStateRaw *input, NetWireCanEnvelope *output);
NetWireStatus net_wire_can_decode_bcm_door_state(NetWireProfile profile,
    const NetWireCanEnvelope *input, NetWireBcmDoorStateRaw *output);

/* 0x210; used 24 / frame 24 / DLC 12. */
typedef struct {
    uint32_t boot_id; /* B0..3 */
    uint32_t tx_sequence; /* B4..7 */
    uint8_t fan_command_level; /* B8 */
    uint8_t fan_measured_level; /* B9 */
    uint8_t thermal_direction_state; /* B10 */
    uint8_t thermal_output_percent; /* B11 */
    uint8_t heat_removal_state; /* B12 */
    uint8_t fan_command_quality; /* B13 */
    uint8_t fan_measured_quality; /* B14 */
    uint8_t thermal_direction_quality; /* B15 */
    uint8_t thermal_output_quality; /* B16 */
    uint8_t heat_removal_quality; /* B17 */
    uint32_t update_sequence; /* B18..21 */
    uint16_t source_age_ms; /* B22..23 */
} NetWireBcmClimateStateRaw;
NetWireStatus net_wire_can_encode_bcm_climate_state(NetWireProfile profile,
    const NetWireBcmClimateStateRaw *input, NetWireCanEnvelope *output);
NetWireStatus net_wire_can_decode_bcm_climate_state(NetWireProfile profile,
    const NetWireCanEnvelope *input, NetWireBcmClimateStateRaw *output);

/* 0x220; used 25 / frame 32 / DLC 13. */
typedef struct {
    uint32_t boot_id; /* B0..3 */
    uint32_t tx_sequence; /* B4..7 */
    uint8_t interior_light_type_state; /* B8 */
    uint8_t light_level_percent; /* B9 */
    uint8_t red; /* B10 */
    uint8_t green; /* B11 */
    uint8_t blue; /* B12 */
    uint8_t interior_light_apply_result; /* B13 */
    uint8_t light_physical_feedback_capability; /* B14 */
    uint8_t light_type_quality; /* B15 */
    uint8_t light_level_quality; /* B16 */
    uint8_t light_color_quality; /* B17 */
    uint8_t light_apply_quality; /* B18 */
    uint32_t update_sequence; /* B19..22 */
    uint16_t source_age_ms; /* B23..24 */
} NetWireBcmLightStateRaw;
NetWireStatus net_wire_can_encode_bcm_light_state(NetWireProfile profile,
    const NetWireBcmLightStateRaw *input, NetWireCanEnvelope *output);
NetWireStatus net_wire_can_decode_bcm_light_state(NetWireProfile profile,
    const NetWireCanEnvelope *input, NetWireBcmLightStateRaw *output);

/* 0x140; used 29 / frame 32 / DLC 13. */
typedef struct {
    uint32_t boot_id; /* B0..3 */
    uint32_t tx_sequence; /* B4..7 */
    uint32_t command_domain_boot; /* B8..11 */
    uint32_t command_id; /* B12..15 */
    uint8_t command_kind; /* B16 */
    uint8_t result; /* B17 */
    uint16_t result_reason; /* B18..19 */
    uint8_t door_event; /* B20 */
    uint32_t occurrence_id; /* B21..24 */
    uint16_t event_age_ms; /* B25..26 */
    uint8_t event_quality; /* B27 */
    uint8_t door_target; /* B28 */
} NetWireBcmResultRaw;
NetWireStatus net_wire_can_encode_bcm_result(NetWireProfile profile,
    const NetWireBcmResultRaw *input, NetWireCanEnvelope *output);
NetWireStatus net_wire_can_decode_bcm_result(NetWireProfile profile,
    const NetWireCanEnvelope *input, NetWireBcmResultRaw *output);

/* 0x141; used 25 / frame 32 / DLC 13. */
typedef struct {
    uint32_t boot_id; /* B0..3 */
    uint32_t tx_sequence; /* B4..7 */
    uint8_t event_kind; /* B8 */
    uint32_t occurrence_id; /* B9..12 */
    uint16_t event_age_ms; /* B13..14 */
    uint8_t event_quality; /* B15 */
    uint32_t related_domain_boot; /* B16..19 */
    uint32_t related_command_id; /* B20..23 */
    uint8_t affected_function; /* B24 */
} NetWireBcmEventRaw;
NetWireStatus net_wire_can_encode_bcm_event(NetWireProfile profile,
    const NetWireBcmEventRaw *input, NetWireCanEnvelope *output);
NetWireStatus net_wire_can_decode_bcm_event(NetWireProfile profile,
    const NetWireCanEnvelope *input, NetWireBcmEventRaw *output);

/* 0x230; used 22 / frame 24 / DLC 12. */
typedef struct {
    uint32_t boot_id; /* B0..3 */
    uint32_t tx_sequence; /* B4..7 */
    uint8_t bcm_ecu_state; /* B8 */
    uint16_t fault_mask; /* B9..10 */
    uint8_t active_fault_category; /* B11 */
    uint16_t active_fault_code; /* B12..13 */
    uint8_t affected_function; /* B14 */
    uint8_t recovering; /* B15 */
    uint32_t update_sequence; /* B16..19 */
    uint16_t source_age_ms; /* B20..21 */
} NetWireBcmStatusRaw;
NetWireStatus net_wire_can_encode_bcm_status(NetWireProfile profile,
    const NetWireBcmStatusRaw *input, NetWireCanEnvelope *output);
NetWireStatus net_wire_can_decode_bcm_status(NetWireProfile profile,
    const NetWireCanEnvelope *input, NetWireBcmStatusRaw *output);

#endif
