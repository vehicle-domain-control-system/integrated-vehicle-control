#ifndef GATEWAY_MAPPER_H
#define GATEWAY_MAPPER_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "uart_m_codec.h"

#ifdef __cplusplus
extern "C" {
#endif

#define GATEWAY_MAPPER_MAX_BLE_PAYLOAD 62U
#define GATEWAY_MAPPER_CONTEXT_PAYLOAD_LENGTH 38U

typedef enum
{
    GATEWAY_MAPPER_OK = 0,
    GATEWAY_MAPPER_INVALID_ARGUMENT,
    GATEWAY_MAPPER_UNSUPPORTED_TYPE,
    GATEWAY_MAPPER_INVALID_LENGTH,
    GATEWAY_MAPPER_INVALID_CONTEXT,
    GATEWAY_MAPPER_INVALID_VALUE,
    GATEWAY_MAPPER_OUTPUT_TOO_SMALL,
    GATEWAY_MAPPER_PRIVATE_MESSAGE,
    GATEWAY_MAPPER_SCHEMA_HOLD
} gateway_mapper_status_t;

/* Supplied only by a registration/session owner or an isolated Host mock. */
typedef struct
{
    uint32_t device_context_id;
    uint64_t session_id;
    bool verified;
} gateway_verified_context_t;

typedef struct
{
    uint8_t message_type;
    size_t payload_length;
    uint8_t payload[GATEWAY_MAPPER_MAX_BLE_PAYLOAD];
} gateway_ble_message_t;

/*
 * Converts an already reassembled BLE semantic payload (without the 8-byte
 * BLE fragment header) to a validated UART-M typed message. No UART frame is
 * created and no transport is called.
 */
gateway_mapper_status_t gateway_mapper_ble_uplink_to_uart(
    uint8_t message_type,
    const uint8_t *payload,
    size_t payload_length,
    const gateway_verified_context_t *context,
    uart_m_message_t *out_message);

/*
 * Converts a validated UART-M typed downlink to the BLE semantic payload.
 * UART frame bytes, CRC and link sequence are never copied to BLE.
 * Private M_DOMAIN_ALIVE and schema-only WINDOW messages are rejected.
 */
gateway_mapper_status_t gateway_mapper_uart_downlink_to_ble(
    const uart_m_message_t *message,
    gateway_ble_message_t *out_message);

/*
 * Schema-only helper for the proposed 38-byte M_CONTEXT payload. The caller
 * must explicitly supply all producer-owned sequence/age fields. This helper
 * does not enqueue or transmit the result.
 */
gateway_mapper_status_t gateway_mapper_encode_context_schema(
    const uart_m_context_t *context,
    uint8_t *payload,
    size_t payload_capacity,
    size_t *payload_length);

#ifdef __cplusplus
}
#endif

#endif
