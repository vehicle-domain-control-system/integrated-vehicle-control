#ifndef NETWORK_NET_WIRE_UART_M_CORE_H
#define NETWORK_NET_WIRE_UART_M_CORE_H
#include "network/net_wire_uart.h"

/* Host raw candidates from frozen UART-M v2, not C2/Gateway ABI or product
 * adoption. The caller supplies every identity, age and sequence unchanged.
 * Session lifetime, warning actions, availability decisions, query completion
 * and final-TX counts are outside these codecs. Failure leaves all outputs
 * unchanged; inputs and outputs must not overlap. */
typedef struct {
    uint32_t device_context_id;
    uint64_t session_id;
    uint32_t query_id;
    uint8_t query_scope, reserved;
    uint64_t request_session;
    uint32_t request_id;
    uint16_t cursor;
} NetWireMQueryRaw;

typedef struct {
    uint32_t device_context_id;
    uint64_t session_id;
    uint8_t warning_type;
    uint32_t occurrence_id, warning_domain_boot;
} NetWireMWarningAckRaw;

typedef struct {
    uint32_t domain_boot_id, update_sequence;
    uint16_t source_age_ms;
    uint8_t domain_state, value_quality;
} NetWireMDomainAliveRaw;

typedef struct {
    uint32_t device_context_id;
    uint64_t session_id;
    uint32_t domain_boot_id, query_id;
    uint8_t warning_type;
    uint32_t occurrence_id;
    uint8_t severity, warning_state, read_state, value_quality, quality_reason;
    uint32_t source_boot, source_sequence;
    uint16_t source_age_ms;
} NetWireMWarningRaw;

typedef struct {
    uint8_t function_id, availability;
    uint16_t reason;
} NetWireMAvailabilityItemRaw;
typedef struct {
    uint32_t device_context_id;
    uint64_t session_id;
    uint32_t domain_boot_id, query_id;
    /* Required wire IDs 1..9 in this order, not C2 enum indices 0..8. */
    NetWireMAvailabilityItemRaw items[9];
    uint32_t update_sequence;
    uint16_t source_age_ms;
} NetWireMAvailabilityRaw;

typedef struct {
    uint32_t device_context_id;
    uint64_t session_id;
    uint32_t domain_boot_id, query_id;
    uint8_t digital_key_setting_state, digital_key_availability;
    uint16_t reason;
    uint32_t update_sequence;
    uint16_t source_age_ms;
} NetWireMDigitalStatusRaw;

typedef struct {
    uint32_t device_context_id;
    uint64_t session_id;
    uint32_t domain_boot_id, query_id;
    uint32_t auto_occurrence_id, related_door_command_id;
    uint8_t origin, result;
    uint16_t result_reason;
    uint8_t result_confirmation;
    uint16_t result_age_ms;
} NetWireMDigitalResultRaw;

typedef struct {
    uint32_t device_context_id;
    uint64_t session_id;
    uint32_t domain_boot_id, query_id;
    int16_t target_temperature_centi_c;
    uint8_t climate_auto, user_fan_level, interior_light_enabled;
    uint8_t normal_light_level_percent, normal_light_rgb[3], value_quality;
    uint32_t update_sequence;
    uint16_t source_age_ms;
} NetWireMUserSettingsRaw;

typedef struct {
    uint32_t device_context_id;
    uint64_t session_id;
    uint32_t domain_boot_id, query_id;
    uint8_t query_scope, query_status;
    /* Caller-provided raw count; codec success never proves final UART TX. */
    uint16_t item_count;
} NetWireMQueryEndRaw;

NetWireStatus net_wire_m_query_encode(NetWireProfile profile, uint16_t sequence,
    const NetWireMQueryRaw *message, NetWireUartFrame *out);
NetWireStatus net_wire_m_query_decode(NetWireProfile profile,
    const NetWireUartFrame *frame, NetWireMQueryRaw *out, uint16_t *sequence_out);
NetWireStatus net_wire_m_warning_ack_encode(NetWireProfile profile, uint16_t sequence,
    const NetWireMWarningAckRaw *message, NetWireUartFrame *out);
NetWireStatus net_wire_m_warning_ack_decode(NetWireProfile profile,
    const NetWireUartFrame *frame, NetWireMWarningAckRaw *out, uint16_t *sequence_out);
NetWireStatus net_wire_m_domain_alive_encode(NetWireProfile profile, uint16_t sequence,
    const NetWireMDomainAliveRaw *message, NetWireUartFrame *out);
NetWireStatus net_wire_m_domain_alive_decode(NetWireProfile profile,
    const NetWireUartFrame *frame, NetWireMDomainAliveRaw *out, uint16_t *sequence_out);
NetWireStatus net_wire_m_warning_encode(NetWireProfile profile, uint16_t sequence,
    const NetWireMWarningRaw *message, NetWireUartFrame *out);
NetWireStatus net_wire_m_warning_decode(NetWireProfile profile,
    const NetWireUartFrame *frame, NetWireMWarningRaw *out, uint16_t *sequence_out);
NetWireStatus net_wire_m_availability_encode(NetWireProfile profile, uint16_t sequence,
    const NetWireMAvailabilityRaw *message, NetWireUartFrame *out);
NetWireStatus net_wire_m_availability_decode(NetWireProfile profile,
    const NetWireUartFrame *frame, NetWireMAvailabilityRaw *out, uint16_t *sequence_out);
NetWireStatus net_wire_m_digital_status_encode(NetWireProfile profile, uint16_t sequence,
    const NetWireMDigitalStatusRaw *message, NetWireUartFrame *out);
NetWireStatus net_wire_m_digital_status_decode(NetWireProfile profile,
    const NetWireUartFrame *frame, NetWireMDigitalStatusRaw *out, uint16_t *sequence_out);
NetWireStatus net_wire_m_digital_result_encode(NetWireProfile profile, uint16_t sequence,
    const NetWireMDigitalResultRaw *message, NetWireUartFrame *out);
NetWireStatus net_wire_m_digital_result_decode(NetWireProfile profile,
    const NetWireUartFrame *frame, NetWireMDigitalResultRaw *out, uint16_t *sequence_out);
NetWireStatus net_wire_m_user_settings_encode(NetWireProfile profile, uint16_t sequence,
    const NetWireMUserSettingsRaw *message, NetWireUartFrame *out);
NetWireStatus net_wire_m_user_settings_decode(NetWireProfile profile,
    const NetWireUartFrame *frame, NetWireMUserSettingsRaw *out, uint16_t *sequence_out);
NetWireStatus net_wire_m_query_end_encode(NetWireProfile profile, uint16_t sequence,
    const NetWireMQueryEndRaw *message, NetWireUartFrame *out);
NetWireStatus net_wire_m_query_end_decode(NetWireProfile profile,
    const NetWireUartFrame *frame, NetWireMQueryEndRaw *out, uint16_t *sequence_out);

/* USER_SETTINGS has no has=false encoding for NORMAL brightness/RGB. Callers
 * must not synthesize values to populate this raw DTO; composite omission and
 * Query partial reporting remain an external producer contract HOLD. */
#endif
