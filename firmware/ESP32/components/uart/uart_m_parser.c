#include "uart_m_parser.h"

#include <string.h>

static uint16_t read_u16_le(const uint8_t *bytes)
{
    return (uint16_t)((uint16_t)bytes[0] | ((uint16_t)bytes[1] << 8U));
}

static void remove_prefix(uart_m_parser_t *parser, size_t count, int discarded)
{
    if (count >= parser->buffered_length) {
        if (discarded != 0) {
            parser->counters.bytes_discarded += (uint32_t)parser->buffered_length;
        }
        parser->buffered_length = 0U;
        return;
    }

    if (discarded != 0) {
        parser->counters.bytes_discarded += (uint32_t)count;
    }
    memmove(parser->buffer, &parser->buffer[count], parser->buffered_length - count);
    parser->buffered_length -= count;
}

static size_t process_buffer(
    uart_m_parser_t *parser,
    uart_m_parser_frame_handler_t frame_handler,
    void *user_context)
{
    size_t emitted = 0U;

    for (;;) {
        uint16_t payload_length;
        size_t expected_length;
        uart_m_frame_t decoded;
        uart_m_frame_status_t decode_status;

        if (parser->buffered_length == 0U) {
            break;
        }
        if (parser->buffer[0] != UART_M_SYNC_FIRST) {
            remove_prefix(parser, 1U, 1);
            continue;
        }
        if (parser->buffered_length == 1U) {
            break;
        }
        if (parser->buffer[1] != UART_M_SYNC_SECOND) {
            remove_prefix(parser, 1U, 1);
            continue;
        }
        if (parser->buffered_length < 3U) {
            break;
        }
        if (parser->buffer[2] != UART_M_VERSION) {
            parser->counters.version_errors++;
            remove_prefix(parser, 1U, 1);
            continue;
        }
        if (parser->buffered_length < 6U) {
            break;
        }

        payload_length = read_u16_le(&parser->buffer[4]);
        if (payload_length > UART_M_MAX_PAYLOAD_SIZE) {
            parser->counters.length_errors++;
            remove_prefix(parser, 1U, 1);
            continue;
        }

        expected_length = UART_M_MIN_FRAME_SIZE + (size_t)payload_length;
        if (parser->buffered_length < expected_length) {
            break;
        }

        decode_status = uart_m_frame_decode(parser->buffer, expected_length, &decoded);
        if (decode_status == UART_M_FRAME_STATUS_OK) {
            parser->counters.frames_decoded++;
            emitted++;
            if (frame_handler != NULL) {
                frame_handler(&decoded, user_context);
            }
            remove_prefix(parser, expected_length, 0);
            continue;
        }
        if (decode_status == UART_M_FRAME_STATUS_INVALID_CRC) {
            parser->counters.crc_errors++;
        } else if (decode_status == UART_M_FRAME_STATUS_INVALID_VERSION) {
            parser->counters.version_errors++;
        } else {
            parser->counters.length_errors++;
        }
        remove_prefix(parser, 1U, 1);
    }

    return emitted;
}

void uart_m_parser_init(uart_m_parser_t *parser)
{
    if (parser != NULL) {
        memset(parser, 0, sizeof(*parser));
    }
}

void uart_m_parser_reset_partial(uart_m_parser_t *parser)
{
    if (parser != NULL) {
        parser->counters.bytes_discarded += (uint32_t)parser->buffered_length;
        parser->buffered_length = 0U;
    }
}

size_t uart_m_parser_pending_bytes(const uart_m_parser_t *parser)
{
    return (parser == NULL) ? 0U : parser->buffered_length;
}

uart_m_parser_status_t uart_m_parser_feed(
    uart_m_parser_t *parser,
    const uint8_t *data,
    size_t data_length,
    uart_m_parser_frame_handler_t frame_handler,
    void *user_context,
    size_t *frames_emitted)
{
    size_t emitted = 0U;

    if ((parser == NULL) || (frames_emitted == NULL) ||
        ((data == NULL) && (data_length != 0U))) {
        return UART_M_PARSER_STATUS_INVALID_ARGUMENT;
    }

    for (size_t i = 0U; i < data_length; ++i) {
        if (parser->buffered_length == UART_M_MAX_FRAME_SIZE) {
            parser->counters.overflow_errors++;
            remove_prefix(parser, 1U, 1);
        }
        parser->buffer[parser->buffered_length] = data[i];
        parser->buffered_length++;
        emitted += process_buffer(parser, frame_handler, user_context);
    }

    *frames_emitted = emitted;
    return UART_M_PARSER_STATUS_OK;
}
