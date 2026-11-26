#ifndef NETWORK_NET_WIRE_UART_DTO_H
#define NETWORK_NET_WIRE_UART_DTO_H

#include "network/net_wire_uart.h"

/* Candidate raw records, never C2 ABI or permission to execute a request.
 * Struct padding is not wire data. Values, sequences and ages are preserved.
 * All IN/OUT ranges and the two decode OUTs must be disjoint. Failure leaves
 * every OUT unchanged. Session admission and all product bindings remain HOLD.
 */
typedef struct {
    uint32_t pi_boot_id, vision_sequence;
    uint16_t vision_age_ms;
    uint8_t occupant_presence, presence_validity, presence_reason;
    uint8_t occupant_count, count_validity, count_reason, vision_status;
    uint16_t vision_fault_code;
} NetWirePVisionResult;

typedef struct {
    uint32_t cis_boot_id, update_sequence;
    uint8_t vehicle_power_permission, value_quality, quality_reason;
    uint16_t use_limit_ms, source_age_ms;
} NetWirePPermission;

typedef struct {
    uint32_t device_context_id;
    uint64_t session_id;
    uint8_t device_registration_state, bt_connection_state, app_active_state;
    uint8_t app_quality;
    uint32_t app_update_sequence;
    uint16_t app_age_ms;
    uint8_t proximity_state, proximity_validity, proximity_reason;
    uint32_t proximity_update_sequence;
    uint16_t proximity_age_ms;
    uint8_t registration_connection_quality;
    uint32_t registration_connection_update_sequence;
    uint16_t registration_connection_age_ms;
} NetWireMContext;

typedef struct {
    uint32_t device_context_id;
    uint64_t session_id;
    uint32_t request_id;
    uint8_t kind, operation;
    /* Raw ARG0..5. Unused arguments must already be zero; never normalized. */
    uint8_t args[6];
} NetWireMRequest;

typedef struct {
    uint32_t device_context_id;
    uint64_t session_id; /* Current recipient, separate from request_session. */
    uint32_t domain_boot_id, query_id;
    uint64_t request_session;
    uint32_t request_id;
    uint8_t result;
    uint16_t result_reason;
    uint8_t result_confirmation, command_target;
    uint32_t command_id;
    uint8_t origin;
    uint16_t result_age_ms;
} NetWireMResult;

NetWireStatus net_wire_p_vision_result_encode(NetWireProfile profile,
    uint16_t sequence, const NetWirePVisionResult *message, NetWireUartFrame *out);
NetWireStatus net_wire_p_vision_result_decode(NetWireProfile profile,
    const NetWireUartFrame *frame, NetWirePVisionResult *out, uint16_t *sequence_out);
NetWireStatus net_wire_p_permission_encode(NetWireProfile profile,
    uint16_t sequence, const NetWirePPermission *message, NetWireUartFrame *out);
NetWireStatus net_wire_p_permission_decode(NetWireProfile profile,
    const NetWireUartFrame *frame, NetWirePPermission *out, uint16_t *sequence_out);
NetWireStatus net_wire_m_context_encode(NetWireProfile profile,
    uint16_t sequence, const NetWireMContext *message, NetWireUartFrame *out);
NetWireStatus net_wire_m_context_decode(NetWireProfile profile,
    const NetWireUartFrame *frame, NetWireMContext *out, uint16_t *sequence_out);
NetWireStatus net_wire_m_request_encode(NetWireProfile profile,
    uint16_t sequence, const NetWireMRequest *message, NetWireUartFrame *out);
NetWireStatus net_wire_m_request_decode(NetWireProfile profile,
    const NetWireUartFrame *frame, NetWireMRequest *out, uint16_t *sequence_out);
NetWireStatus net_wire_m_result_encode(NetWireProfile profile,
    uint16_t sequence, const NetWireMResult *message, NetWireUartFrame *out);
NetWireStatus net_wire_m_result_decode(NetWireProfile profile,
    const NetWireUartFrame *frame, NetWireMResult *out, uint16_t *sequence_out);

/* Only kind2/op1 has a signed i16 centi-C argument. This helper validates its
 * raw form, performs a portable sign conversion, and never approves a setpoint.
 * Full i16 range is preserved; the functional setting range is CONTRACT_OPEN.
 */
NetWireStatus net_wire_m_request_temperature(const NetWireMRequest *message,
    int16_t *temperature_centi_c_out);

/* Wave2B raw candidates share framing only; no runtime or product binding. */
#include "network/net_wire_uart_m_core.h"
#include "network/net_wire_uart_m_projection.h"
#include "network/net_wire_uart_c_status.h"

#endif
