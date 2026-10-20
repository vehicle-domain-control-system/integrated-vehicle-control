#include "network/net_codec.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

/* Adapted from the supplied independent review fixture:
 * inputs/09_INDEPENDENT_UART_RANDOM_STRESS.c
 * SHA256: A2B8349A74FEA4E1BF64A124A7F2D6F05C3FA430AF86C79EAB6DA9E95CD4A941
 *
 * Preserve the original PRNG, seed, call order, 500 rounds, payload generation,
 * noise, and chunk boundaries so the run still generates exactly 12628 frames.
 * Every framing/checksum/timing value below is an artificial HOST TEST FIXTURE
 * choice. UART-M23 / UART-C3 product wire contracts remain HOLD.
 */
enum {
    FIXTURE_ROUNDS = 500,
    FIXTURE_EXPECTED_TOTAL = 12628,
    FIXTURE_STREAM_CAPACITY = 6000,
    FIXTURE_FRAME_CAPACITY = 35,
    FIXTURE_PAYLOAD_CAPACITY = 122
};

static uint32_t random_state = UINT32_C(0xB5E1);

static uint32_t next_random(void)
{
    random_state ^= random_state << 13u;
    random_state ^= random_state >> 17u;
    random_state ^= random_state << 5u;
    return random_state;
}

/* This additive fixture checksum is deliberately not a product CRC. */
static uint16_t fixture_checksum(const uint8_t *bytes, size_t length)
{
    uint32_t sum = UINT32_C(0xCAFE);
    size_t i;
    for (i = 0u; i < length; ++i) {
        sum = (sum + bytes[i]) & UINT32_C(0xFFFF);
    }
    return (uint16_t)sum;
}

/* These checks remain active in Release builds where NDEBUG may be defined. */
#define VERIFY(condition) do { \
    if (!(condition)) { \
        fprintf(stderr, "%s:%d round=%lu frame=%lu random_state=%lu: %s\n", \
                __FILE__, __LINE__, (unsigned long)round, \
                (unsigned long)expected, (unsigned long)random_state, #condition); \
        return 1; \
    } \
} while (0)

int main(void)
{
    const NetUartFixtureProfile profile = {
        {0xA5u, 0x5Au}, 154u, 1000000u, false, fixture_checksum
    };
    size_t generated_total = 0u;
    size_t decoded_total = 0u;
    size_t uart_m_total = 0u;
    size_t uart_c_total = 0u;
    size_t round;
    size_t expected = 0u;

    for (round = 0u; round < FIXTURE_ROUNDS; ++round) {
        uint8_t stream_bytes[FIXTURE_STREAM_CAPACITY] = {0};
        uint64_t captured_at[FIXTURE_STREAM_CAPACITY] = {0};
        NetFrame frames[FIXTURE_FRAME_CAPACITY] = {0};
        uint64_t expected_at[FIXTURE_FRAME_CAPACITY] = {0};
        NetUartStream parser = {0};
        const NetLink link = (round % 2u) != 0u ? NET_LINK_UART_C : NET_LINK_UART_M;
        size_t stream_size = 0u;
        size_t noise_total = 0u;
        size_t parser_calls = 0u;
        size_t position = 0u;
        size_t frame_count;
        size_t frame_index;
        size_t drain_calls;
        bool drained = false;

        expected = 0u;
        VERIFY(net_uart_stream_init(&parser, link, &profile) == NET_OK);
        frame_count = 20u + (next_random() % 12u);
        VERIFY(frame_count <= FIXTURE_FRAME_CAPACITY);
        generated_total += frame_count;

        for (frame_index = 0u; frame_index < frame_count; ++frame_index) {
            uint8_t payload[FIXTURE_PAYLOAD_CAPACITY];
            const size_t payload_length = next_random() % 122u;
            size_t noise;
            size_t i;
            for (i = 0u; i < payload_length; ++i) {
                payload[i] = (uint8_t)next_random();
            }
            if (payload_length > 3u && frame_index % 3u == 0u) {
                payload[1] = 0xA5u;
                payload[2] = 0x5Au;
            }
            VERIFY(net_uart_fixture_encode(&profile, link, payload, payload_length,
                                            100u, &frames[frame_index]) == NET_OK);
            noise = next_random() % 4u;
            noise_total += noise;
            for (i = 0u; i < noise; ++i) {
                VERIFY(stream_size < sizeof(stream_bytes));
                stream_bytes[stream_size] = 0x33u;
                captured_at[stream_size] = 10u + stream_size;
                ++stream_size;
            }
            for (i = 0u; i < frames[frame_index].length; ++i) {
                VERIFY(stream_size < sizeof(stream_bytes));
                stream_bytes[stream_size] = frames[frame_index].bytes[i];
                captured_at[stream_size] = 10u + stream_size;
                ++stream_size;
            }
            /* The stream's original last-byte time, not encoder time or now. */
            VERIFY(stream_size != 0u);
            expected_at[frame_index] = captured_at[stream_size - 1u];
        }

        while (position < stream_size) {
            size_t chunk = 1u + (next_random() % 19u);
            size_t position_before = position;
            NetUartStreamInput input;
            if (chunk > stream_size - position) {
                chunk = stream_size - position;
            }
            input = (NetUartStreamInput){
                stream_bytes + position, captured_at + position, chunk,
                100000u, false, true
            };
            for (;;) {
                NetFrame output;
                size_t consumed = 999u;
                NetStatus status;
                /* At most one NO_WORK per nonempty chunk and one OK per
                 * expected frame, plus the final empty drain call. */
                VERIFY(parser_calls < stream_size + frame_count + 1u);
                ++parser_calls;
                status = net_uart_stream_next(&parser, &input, &consumed, &output);
                VERIFY(consumed <= input.length);
                input.bytes += consumed;
                input.captured_at_ms += consumed;
                input.length -= consumed;
                position += consumed;
                VERIFY(position <= stream_size);
                if (status == NET_OK) {
                    VERIFY(expected < frame_count);
                    VERIFY(output.link == link);
                    VERIFY(output.length == frames[expected].length);
                    VERIFY(output.captured_at_ms == expected_at[expected]);
                    VERIFY(memcmp(output.bytes, frames[expected].bytes,
                                  output.length) == 0);
                    ++expected;
                    ++decoded_total;
                    continue;
                }
                VERIFY(status == NET_NO_WORK);
                VERIFY(input.length == 0u);
                break;
            }
            VERIFY(position > position_before);
        }

        /* A bounded zero-input drain also detects an endlessly repeated frame. */
        for (drain_calls = 0u; drain_calls <= frame_count; ++drain_calls) {
            const NetUartStreamInput input = {NULL, NULL, 0u, 100000u, false, true};
            NetFrame output;
            size_t consumed = 999u;
            NetStatus status;
            VERIFY(parser_calls < stream_size + frame_count + 1u);
            ++parser_calls;
            status = net_uart_stream_next(&parser, &input, &consumed, &output);
            VERIFY(consumed == 0u);
            if (status == NET_NO_WORK) {
                drained = true;
                break;
            }
            VERIFY(status == NET_OK);
            VERIFY(expected < frame_count);
            VERIFY(output.link == link);
            VERIFY(output.length == frames[expected].length);
            VERIFY(output.captured_at_ms == expected_at[expected]);
            VERIFY(memcmp(output.bytes, frames[expected].bytes, output.length) == 0);
            ++expected;
            ++decoded_total;
        }
        VERIFY(drained);
        VERIFY(expected == frame_count);
        VERIFY(position == stream_size);
        VERIFY(parser.count == 0u);
        VERIFY(parser.discarded_bytes == noise_total);
        VERIFY(parser.malformed_lengths == 0u);
        VERIFY(parser.bad_checksums == 0u);
        VERIFY(parser.assembly_timeouts == 0u);
        VERIFY(parser.byte_losses == 0u);
        if (link == NET_LINK_UART_M) {
            uart_m_total += expected;
        } else {
            uart_c_total += expected;
        }
    }

    VERIFY(generated_total == FIXTURE_EXPECTED_TOTAL);
    VERIFY(decoded_total == generated_total);
    VERIFY(uart_m_total != 0u && uart_c_total != 0u);
    VERIFY(uart_m_total + uart_c_total == decoded_total);
    printf("PASS randomized UART fixture frames=%lu rounds=%lu initial_seed=0xB5E1 "
           "UART-M=%lu UART-C=%lu\n",
           (unsigned long)decoded_total, (unsigned long)round,
           (unsigned long)uart_m_total, (unsigned long)uart_c_total);
    puts("Sanitizer instrumentation is external build/run configuration; "
         "this test does not attest ASan/UBSan. Product wire contracts remain HOLD.");
    return 0;
}
