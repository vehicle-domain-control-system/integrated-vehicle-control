#ifndef UART_M_PARSER_H
#define UART_M_PARSER_H

#include <stddef.h>
#include <stdint.h>

#include "uart_m_frame.h"

#ifdef __cplusplus
extern "C" {
#endif

/* frame is borrowed and remains valid only for the duration of the callback. */
typedef void (*uart_m_parser_frame_handler_t)(const uart_m_frame_t *frame, void *user_context);

typedef enum
{
    UART_M_PARSER_STATUS_OK = 0,
    UART_M_PARSER_STATUS_INVALID_ARGUMENT
} uart_m_parser_status_t;

typedef struct
{
    uint32_t frames_decoded;
    uint32_t bytes_discarded;
    uint32_t version_errors;
    uint32_t length_errors;
    uint32_t crc_errors;
    uint32_t overflow_errors;
} uart_m_parser_counters_t;

typedef struct
{
    uint8_t buffer[UART_M_MAX_FRAME_SIZE];
    size_t buffered_length;
    uart_m_parser_counters_t counters;
} uart_m_parser_t;

void uart_m_parser_init(uart_m_parser_t *parser);
/* The owner calls this when its externally enforced assembly deadline expires. */
void uart_m_parser_reset_partial(uart_m_parser_t *parser);
size_t uart_m_parser_pending_bytes(const uart_m_parser_t *parser);

uart_m_parser_status_t uart_m_parser_feed(
    uart_m_parser_t *parser,
    const uint8_t *data,
    size_t data_length,
    uart_m_parser_frame_handler_t frame_handler,
    void *user_context,
    size_t *frames_emitted);

#ifdef __cplusplus
}
#endif

#endif
