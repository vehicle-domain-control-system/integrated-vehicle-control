#ifndef UART_M_FRAME_H
#define UART_M_FRAME_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* UART-M v2 framing is WIRE_PROPOSED in 20_UART_M.md. */
#define UART_M_SYNC_FIRST UINT8_C(0xA5)
#define UART_M_SYNC_SECOND UINT8_C(0x5A)
#define UART_M_VERSION UINT8_C(0x02)

#define UART_M_MAX_PAYLOAD_SIZE 128U
#define UART_M_HEADER_SIZE 8U
#define UART_M_CRC_SIZE 2U
#define UART_M_MIN_FRAME_SIZE (UART_M_HEADER_SIZE + UART_M_CRC_SIZE)
#define UART_M_MAX_FRAME_SIZE (UART_M_MIN_FRAME_SIZE + UART_M_MAX_PAYLOAD_SIZE)

typedef enum
{
    UART_M_FRAME_STATUS_OK = 0,
    UART_M_FRAME_STATUS_INVALID_ARGUMENT,
    UART_M_FRAME_STATUS_OUTPUT_TOO_SMALL,
    UART_M_FRAME_STATUS_INCOMPLETE,
    UART_M_FRAME_STATUS_INVALID_SYNC,
    UART_M_FRAME_STATUS_INVALID_VERSION,
    UART_M_FRAME_STATUS_INVALID_LENGTH,
    UART_M_FRAME_STATUS_INVALID_CRC
} uart_m_frame_status_t;

typedef struct
{
    uint8_t message_type;
    uint16_t payload_length;
    uint16_t link_sequence;
    uint8_t payload[UART_M_MAX_PAYLOAD_SIZE];
} uart_m_frame_t;

typedef struct
{
    /* Create one tracker per transmission direction. */
    uint16_t next_value;
} uart_m_sequence_t;

uart_m_frame_status_t uart_m_frame_encode(
    uint8_t message_type,
    uint16_t link_sequence,
    const uint8_t *payload,
    size_t payload_length,
    uint8_t *output,
    size_t output_capacity,
    size_t *output_length);

uart_m_frame_status_t uart_m_frame_decode(
    const uint8_t *frame,
    size_t frame_length,
    uart_m_frame_t *decoded);

void uart_m_sequence_init(uart_m_sequence_t *sequence, uint16_t first_value);
uint16_t uart_m_sequence_take(uart_m_sequence_t *sequence);

#ifdef __cplusplus
}
#endif

#endif
