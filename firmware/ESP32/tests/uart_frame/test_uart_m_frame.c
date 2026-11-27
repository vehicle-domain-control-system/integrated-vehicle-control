#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "uart_m_crc16.h"
#include "uart_m_frame.h"
#include "uart_m_parser.h"

#define ARRAY_SIZE(array) (sizeof(array) / sizeof((array)[0]))

#define CHECK(condition)                                                                    \
    do {                                                                                    \
        if (!(condition)) {                                                                 \
            printf("CHECK failed at %s:%d: %s\n", __FILE__, __LINE__, #condition);         \
            return false;                                                                   \
        }                                                                                   \
    } while (0)

typedef struct
{
    uart_m_frame_t frames[8];
    size_t count;
} frame_collector_t;

static void collect_frame(const uart_m_frame_t *frame, void *user_context)
{
    frame_collector_t *collector = (frame_collector_t *)user_context;

    if (collector->count < ARRAY_SIZE(collector->frames)) {
        collector->frames[collector->count] = *frame;
        collector->count++;
    }
}

static void fill_payload(uint8_t *payload, size_t length, uint8_t seed)
{
    for (size_t i = 0U; i < length; ++i) {
        payload[i] = (uint8_t)(seed + (uint8_t)i);
    }
}

static bool encode_frame(
    uint8_t message_type,
    uint16_t sequence,
    const uint8_t *payload,
    size_t payload_length,
    uint8_t *encoded,
    size_t *encoded_length)
{
    return uart_m_frame_encode(
               message_type,
               sequence,
               payload,
               payload_length,
               encoded,
               UART_M_MAX_FRAME_SIZE,
               encoded_length) == UART_M_FRAME_STATUS_OK;
}

static bool test_crc_known_vector(void)
{
    static const uint8_t vector[] = "123456789";

    CHECK(uart_m_crc16_ccitt_false(vector, sizeof(vector) - 1U) == UINT16_C(0x29B1));
    CHECK(uart_m_crc16_ccitt_false(NULL, 0U) == UINT16_C(0xFFFF));
    return true;
}

static bool test_encode_decode_payload_lengths(void)
{
    static const size_t lengths[] = {0U, 24U, 38U, 128U};
    uint8_t payload[UART_M_MAX_PAYLOAD_SIZE];
    uint8_t encoded[UART_M_MAX_FRAME_SIZE];

    fill_payload(payload, sizeof(payload), UINT8_C(0x10));
    for (size_t i = 0U; i < ARRAY_SIZE(lengths); ++i) {
        const size_t payload_length = lengths[i];
        const uint8_t *payload_pointer = (payload_length == 0U) ? NULL : payload;
        size_t encoded_length = 0U;
        uart_m_frame_t decoded;
        uint16_t stored_crc;
        uint16_t calculated_crc;

        CHECK(encode_frame(UINT8_C(0x20), UINT16_C(0x1234), payload_pointer,
                           payload_length, encoded, &encoded_length));
        CHECK(encoded_length == payload_length + UART_M_MIN_FRAME_SIZE);
        CHECK(encoded[0] == UART_M_SYNC_FIRST);
        CHECK(encoded[1] == UART_M_SYNC_SECOND);
        CHECK(encoded[2] == UART_M_VERSION);
        CHECK(encoded[4] == (uint8_t)(payload_length & 0xFFU));
        CHECK(encoded[5] == (uint8_t)(payload_length >> 8U));
        CHECK(encoded[6] == UINT8_C(0x34));
        CHECK(encoded[7] == UINT8_C(0x12));

        stored_crc = (uint16_t)((uint16_t)encoded[8U + payload_length] |
                                ((uint16_t)encoded[9U + payload_length] << 8U));
        calculated_crc = uart_m_crc16_ccitt_false(&encoded[2], 6U + payload_length);
        CHECK(stored_crc == calculated_crc);

        CHECK(uart_m_frame_decode(encoded, encoded_length, &decoded) == UART_M_FRAME_STATUS_OK);
        CHECK(decoded.message_type == UINT8_C(0x20));
        CHECK(decoded.link_sequence == UINT16_C(0x1234));
        CHECK(decoded.payload_length == payload_length);
        if (payload_length != 0U) {
            CHECK(memcmp(decoded.payload, payload, payload_length) == 0);
        }
    }

    return true;
}

static bool test_encode_rejects_invalid_sizes(void)
{
    uint8_t payload[UART_M_MAX_PAYLOAD_SIZE + 1U] = {0};
    uint8_t encoded[UART_M_MAX_FRAME_SIZE];
    size_t encoded_length = 0U;

    CHECK(uart_m_frame_encode(UINT8_C(0x10), 0U, payload, sizeof(payload), encoded,
                              sizeof(encoded), &encoded_length) ==
          UART_M_FRAME_STATUS_INVALID_LENGTH);
    CHECK(uart_m_frame_encode(UINT8_C(0x10), 0U, payload, 24U, encoded, 33U,
                              &encoded_length) == UART_M_FRAME_STATUS_OUTPUT_TOO_SMALL);
    return true;
}

static bool test_decode_validation(void)
{
    uint8_t encoded[UART_M_MAX_FRAME_SIZE];
    uint8_t payload[24];
    size_t encoded_length = 0U;
    uart_m_frame_t decoded;

    fill_payload(payload, sizeof(payload), UINT8_C(0x31));
    CHECK(encode_frame(UINT8_C(0x10), 7U, payload, sizeof(payload), encoded, &encoded_length));

    encoded[0] = 0U;
    CHECK(uart_m_frame_decode(encoded, encoded_length, &decoded) == UART_M_FRAME_STATUS_INVALID_SYNC);
    encoded[0] = UART_M_SYNC_FIRST;

    encoded[2] = 1U;
    CHECK(uart_m_frame_decode(encoded, encoded_length, &decoded) == UART_M_FRAME_STATUS_INVALID_VERSION);
    encoded[2] = UART_M_VERSION;

    encoded[4] = UINT8_C(0x81);
    encoded[5] = 0U;
    CHECK(uart_m_frame_decode(encoded, encoded_length, &decoded) == UART_M_FRAME_STATUS_INVALID_LENGTH);

    CHECK(encode_frame(UINT8_C(0x10), 7U, payload, sizeof(payload), encoded, &encoded_length));
    CHECK(uart_m_frame_decode(encoded, encoded_length - 1U, &decoded) == UART_M_FRAME_STATUS_INCOMPLETE);
    encoded[8] ^= UINT8_C(0x01);
    CHECK(uart_m_frame_decode(encoded, encoded_length, &decoded) == UART_M_FRAME_STATUS_INVALID_CRC);
    return true;
}

static bool test_sequence_wraparound(void)
{
    uart_m_sequence_t sequence;

    uart_m_sequence_init(&sequence, UINT16_C(0xFFFE));
    CHECK(uart_m_sequence_take(&sequence) == UINT16_C(0xFFFE));
    CHECK(uart_m_sequence_take(&sequence) == UINT16_C(0xFFFF));
    CHECK(uart_m_sequence_take(&sequence) == UINT16_C(0x0000));
    CHECK(uart_m_sequence_take(&sequence) == UINT16_C(0x0001));
    return true;
}

static bool test_stream_one_byte_at_a_time(void)
{
    uint8_t payload[38];
    uint8_t encoded[UART_M_MAX_FRAME_SIZE];
    size_t encoded_length = 0U;
    uart_m_parser_t parser;
    frame_collector_t collector = {0};

    fill_payload(payload, sizeof(payload), UINT8_C(0x40));
    CHECK(encode_frame(UINT8_C(0x01), UINT16_C(0x2222), payload, sizeof(payload),
                       encoded, &encoded_length));
    uart_m_parser_init(&parser);

    for (size_t i = 0U; i < encoded_length; ++i) {
        size_t emitted = 0U;
        CHECK(uart_m_parser_feed(&parser, &encoded[i], 1U, collect_frame, &collector,
                                 &emitted) == UART_M_PARSER_STATUS_OK);
        CHECK(emitted == ((i + 1U == encoded_length) ? 1U : 0U));
    }

    CHECK(collector.count == 1U);
    CHECK(collector.frames[0].payload_length == sizeof(payload));
    CHECK(memcmp(collector.frames[0].payload, payload, sizeof(payload)) == 0);
    CHECK(uart_m_parser_pending_bytes(&parser) == 0U);
    return true;
}

static bool test_stream_multiple_frames(void)
{
    uint8_t first[UART_M_MAX_FRAME_SIZE];
    uint8_t second[UART_M_MAX_FRAME_SIZE];
    uint8_t third[UART_M_MAX_FRAME_SIZE];
    uint8_t payload_24[24];
    uint8_t payload_128[128];
    uint8_t stream[UART_M_MAX_FRAME_SIZE * 3U];
    size_t first_length = 0U;
    size_t second_length = 0U;
    size_t third_length = 0U;
    size_t stream_length;
    size_t emitted = 0U;
    uart_m_parser_t parser;
    frame_collector_t collector = {0};

    fill_payload(payload_24, sizeof(payload_24), UINT8_C(0x20));
    fill_payload(payload_128, sizeof(payload_128), UINT8_C(0x80));
    CHECK(encode_frame(UINT8_C(0x02), 1U, NULL, 0U, first, &first_length));
    CHECK(encode_frame(UINT8_C(0x10), 2U, payload_24, sizeof(payload_24), second, &second_length));
    CHECK(encode_frame(UINT8_C(0x22), 3U, payload_128, sizeof(payload_128), third, &third_length));

    memcpy(stream, first, first_length);
    memcpy(&stream[first_length], second, second_length);
    memcpy(&stream[first_length + second_length], third, third_length);
    stream_length = first_length + second_length + third_length;

    uart_m_parser_init(&parser);
    CHECK(uart_m_parser_feed(&parser, stream, stream_length, collect_frame, &collector,
                             &emitted) == UART_M_PARSER_STATUS_OK);
    CHECK(emitted == 3U);
    CHECK(collector.count == 3U);
    CHECK(collector.frames[0].link_sequence == 1U);
    CHECK(collector.frames[1].link_sequence == 2U);
    CHECK(collector.frames[2].link_sequence == 3U);
    CHECK(collector.frames[2].payload_length == sizeof(payload_128));
    return true;
}

static bool test_incomplete_frame(void)
{
    uint8_t payload[24];
    uint8_t encoded[UART_M_MAX_FRAME_SIZE];
    size_t encoded_length = 0U;
    size_t emitted = 0U;
    uart_m_parser_t parser;
    frame_collector_t collector = {0};

    fill_payload(payload, sizeof(payload), UINT8_C(0x11));
    CHECK(encode_frame(UINT8_C(0x10), 9U, payload, sizeof(payload), encoded, &encoded_length));
    uart_m_parser_init(&parser);

    CHECK(uart_m_parser_feed(&parser, encoded, encoded_length - 1U, collect_frame, &collector,
                             &emitted) == UART_M_PARSER_STATUS_OK);
    CHECK(emitted == 0U);
    CHECK(collector.count == 0U);
    CHECK(uart_m_parser_pending_bytes(&parser) == encoded_length - 1U);

    CHECK(uart_m_parser_feed(&parser, &encoded[encoded_length - 1U], 1U, collect_frame,
                             &collector, &emitted) == UART_M_PARSER_STATUS_OK);
    CHECK(emitted == 1U);
    CHECK(collector.count == 1U);
    CHECK(uart_m_parser_pending_bytes(&parser) == 0U);
    return true;
}

static bool test_crc_error_then_recovery(void)
{
    uint8_t payload[24];
    uint8_t corrupt[UART_M_MAX_FRAME_SIZE];
    uint8_t valid[UART_M_MAX_FRAME_SIZE];
    uint8_t stream[UART_M_MAX_FRAME_SIZE * 2U];
    size_t corrupt_length = 0U;
    size_t valid_length = 0U;
    size_t emitted = 0U;
    uart_m_parser_t parser;
    frame_collector_t collector = {0};

    fill_payload(payload, sizeof(payload), UINT8_C(0x01));
    CHECK(encode_frame(UINT8_C(0x10), 10U, payload, sizeof(payload), corrupt, &corrupt_length));
    CHECK(encode_frame(UINT8_C(0x10), 11U, payload, sizeof(payload), valid, &valid_length));
    corrupt[UART_M_HEADER_SIZE + 3U] ^= UINT8_C(0x80);
    memcpy(stream, corrupt, corrupt_length);
    memcpy(&stream[corrupt_length], valid, valid_length);

    uart_m_parser_init(&parser);
    CHECK(uart_m_parser_feed(&parser, stream, corrupt_length + valid_length, collect_frame,
                             &collector, &emitted) == UART_M_PARSER_STATUS_OK);
    CHECK(emitted == 1U);
    CHECK(collector.count == 1U);
    CHECK(collector.frames[0].link_sequence == 11U);
    CHECK(parser.counters.crc_errors == 1U);
    return true;
}

static bool test_bad_version_and_length_then_recovery(void)
{
    static const uint8_t bad_version[] = {
        UART_M_SYNC_FIRST, UART_M_SYNC_SECOND, 1U, UINT8_C(0x10)};
    static const uint8_t bad_length[] = {
        UART_M_SYNC_FIRST, UART_M_SYNC_SECOND, UART_M_VERSION, UINT8_C(0x10),
        UINT8_C(0x81), 0U};
    uint8_t valid[UART_M_MAX_FRAME_SIZE];
    uint8_t stream[64];
    size_t valid_length = 0U;
    size_t stream_length = 0U;
    size_t emitted = 0U;
    uart_m_parser_t parser;
    frame_collector_t collector = {0};

    CHECK(encode_frame(UINT8_C(0x02), 12U, NULL, 0U, valid, &valid_length));
    memcpy(&stream[stream_length], bad_version, sizeof(bad_version));
    stream_length += sizeof(bad_version);
    memcpy(&stream[stream_length], bad_length, sizeof(bad_length));
    stream_length += sizeof(bad_length);
    memcpy(&stream[stream_length], valid, valid_length);
    stream_length += valid_length;

    uart_m_parser_init(&parser);
    CHECK(uart_m_parser_feed(&parser, stream, stream_length, collect_frame, &collector,
                             &emitted) == UART_M_PARSER_STATUS_OK);
    CHECK(emitted == 1U);
    CHECK(collector.frames[0].link_sequence == 12U);
    CHECK(parser.counters.version_errors == 1U);
    CHECK(parser.counters.length_errors == 1U);
    return true;
}

static bool test_sync_inside_payload_and_overlap(void)
{
    static const uint8_t payload[] = {
        0U, UART_M_SYNC_FIRST, UART_M_SYNC_SECOND, 1U,
        UART_M_SYNC_FIRST, UART_M_SYNC_SECOND, 2U};
    static const uint8_t junk[] = {0U, UINT8_C(0x33), UART_M_SYNC_FIRST};
    uint8_t encoded[UART_M_MAX_FRAME_SIZE];
    uint8_t stream[UART_M_MAX_FRAME_SIZE + sizeof(junk)];
    size_t encoded_length = 0U;
    size_t emitted = 0U;
    uart_m_parser_t parser;
    frame_collector_t collector = {0};

    CHECK(encode_frame(UINT8_C(0x21), 13U, payload, sizeof(payload), encoded, &encoded_length));
    memcpy(stream, junk, sizeof(junk));
    memcpy(&stream[sizeof(junk)], encoded, encoded_length);

    uart_m_parser_init(&parser);
    CHECK(uart_m_parser_feed(&parser, stream, sizeof(junk) + encoded_length, collect_frame,
                             &collector, &emitted) == UART_M_PARSER_STATUS_OK);
    CHECK(emitted == 1U);
    CHECK(collector.count == 1U);
    CHECK(collector.frames[0].payload_length == sizeof(payload));
    CHECK(memcmp(collector.frames[0].payload, payload, sizeof(payload)) == 0);
    CHECK(parser.counters.bytes_discarded == sizeof(junk));
    return true;
}

typedef bool (*test_function_t)(void);

typedef struct
{
    const char *name;
    test_function_t function;
} test_case_t;

int main(void)
{
    static const test_case_t tests[] = {
        {"crc_known_vector", test_crc_known_vector},
        {"encode_decode_payload_lengths", test_encode_decode_payload_lengths},
        {"encode_rejects_invalid_sizes", test_encode_rejects_invalid_sizes},
        {"decode_validation", test_decode_validation},
        {"sequence_wraparound", test_sequence_wraparound},
        {"stream_one_byte_at_a_time", test_stream_one_byte_at_a_time},
        {"stream_multiple_frames", test_stream_multiple_frames},
        {"incomplete_frame", test_incomplete_frame},
        {"crc_error_then_recovery", test_crc_error_then_recovery},
        {"bad_version_and_length_then_recovery", test_bad_version_and_length_then_recovery},
        {"sync_inside_payload_and_overlap", test_sync_inside_payload_and_overlap},
    };

    for (size_t i = 0U; i < ARRAY_SIZE(tests); ++i) {
        if (!tests[i].function()) {
            printf("[FAIL] %s\n", tests[i].name);
            return 1;
        }
        printf("[PASS] %s\n", tests[i].name);
    }

    printf("All %zu UART-M frame tests passed.\n", ARRAY_SIZE(tests));
    return 0;
}
