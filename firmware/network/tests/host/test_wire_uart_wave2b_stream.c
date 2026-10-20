#include "network/net_wire_uart.h"
#include "fixtures/wave2b_structural_vectors.h"
#include <stdio.h>
#include <string.h>

#define CHECK(x) do { if (!(x)) { fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #x); return 1; } } while (0)
#define PROFILE NET_WIRE_CANDIDATE_ONLY
#define COUNT (sizeof(structural_vectors) / sizeof(structural_vectors[0]))

static NetWireStatus init(NetWireUartStream *s, const Wave2BStructuralVector *v)
{
    const NetWireStreamPolicy policy = {true, 50u, false};
    return net_wire_uart_stream_init(s, PROFILE, v->link, v->direction, &policy);
}

static NetWireStatus feed(NetWireUartStream *s, const uint8_t *bytes,
    const uint64_t *times, size_t length, uint64_t now, bool timer,
    size_t *used, NetWireUartFrame *out)
{
    const NetWireStreamInput input = {bytes, times, length, now, true, timer};
    return net_wire_uart_stream_next(s, &input, used, out);
}

static int structural42(void)
{
    size_t i;
    CHECK(COUNT == 42u);
    for (i = 0u; i < COUNT; ++i) {
        const Wave2BStructuralVector *v = &structural_vectors[i];
        NetWireUartFrame frame = {0}, encoded;
        NetWireUartPayload decoded, before;
        size_t payload_length = v->length - 10u;
        frame.length = v->length;
        frame.captured_at_ms = UINT64_C(0x123456789);
        memcpy(frame.bytes, v->bytes, v->length);
        CHECK(net_wire_uart_encode(PROFILE, v->link, v->direction,
            v->bytes[3], v->sequence, v->bytes + 8u, payload_length, &encoded) == NET_WIRE_OK);
        CHECK(encoded.length == v->length && memcmp(encoded.bytes, v->bytes, v->length) == 0);
        CHECK(net_wire_uart_decode(PROFILE, v->link, v->direction, &frame, &decoded) == NET_WIRE_OK);
        CHECK(decoded.type == v->bytes[3] && decoded.sequence == v->sequence && decoded.length == payload_length);
        CHECK(decoded.captured_at_ms == frame.captured_at_ms && memcmp(decoded.bytes, v->bytes + 8u, payload_length) == 0);
        before = decoded;
        frame.bytes[v->length - 1u] ^= 1u;
        CHECK(net_wire_uart_decode(PROFILE, v->link, v->direction, &frame, &decoded) == NET_WIRE_MALFORMED);
        CHECK(memcmp(&decoded, &before, sizeof(decoded)) == 0);
    }
    puts("PASS 42 structural references; not typed positive vectors");
    return 0;
}

static int fragmented42(void)
{
    size_t i, j, used;
    for (i = 0u; i < COUNT; ++i) {
        const Wave2BStructuralVector *v = &structural_vectors[i];
        NetWireUartStream stream;
        NetWireUartFrame out, before;
        const uint64_t original_capture = 100u + i;
        CHECK(init(&stream, v) == NET_WIRE_OK);
        memset(&out, 0x69, sizeof(out)); before = out;
        for (j = 0u; j < v->length; ++j) {
            NetWireStatus expected = j + 1u == v->length ? NET_WIRE_OK : NET_WIRE_NEED_MORE;
            CHECK(feed(&stream, v->bytes + j, &original_capture, 1u, 1000000u, false, &used, &out) == expected);
            CHECK(used == 1u);
            if (expected == NET_WIRE_NEED_MORE) CHECK(memcmp(&out, &before, sizeof(out)) == 0);
        }
        CHECK(out.length == v->length && memcmp(out.bytes, v->bytes, v->length) == 0);
        CHECK(out.captured_at_ms == original_capture);
    }
    return 0;
}

static int suffix_and_timeout42(void)
{
    size_t i, j, used, offset;
    uint8_t bytes[NET_WIRE_UART_FRAME_MAX * 3u + 1u];
    uint64_t times[NET_WIRE_UART_FRAME_MAX * 3u + 1u];
    for (i = 0u; i < COUNT; ++i) {
        const Wave2BStructuralVector *v = &structural_vectors[i];
        NetWireUartStream stream;
        NetWireUartFrame out, before;
        size_t length = 1u + 3u * v->length;
        CHECK(init(&stream, v) == NET_WIRE_OK);
        /* Noise + CRC-bad frame + two concatenated independent references. */
        bytes[0] = 0xa5u;
        memcpy(bytes + 1u, v->bytes, v->length);
        bytes[v->length] ^= 1u;
        memcpy(bytes + 1u + v->length, v->bytes, v->length);
        memcpy(bytes + 1u + 2u * v->length, v->bytes, v->length);
        for (j = 0u; j < length; ++j) times[j] = 100u;
        CHECK(feed(&stream, bytes, times, length, 1000000u, false, &used, &out) == NET_WIRE_OK);
        CHECK(used == 1u + 2u * v->length && stream.bad_crc >= 1u);
        CHECK(out.length == v->length && memcmp(out.bytes, v->bytes, v->length) == 0 && out.captured_at_ms == 100u);
        offset = used;
        CHECK(feed(&stream, bytes + offset, times + offset, length - offset, 1000000u, false, &used, &out) == NET_WIRE_OK);
        CHECK(used == v->length && out.captured_at_ms == 100u);
        /* Valid suffix begins inside the CRC-bad outer frame. */
        CHECK(init(&stream, v) == NET_WIRE_OK);
        memcpy(bytes, v->bytes, 8u); memcpy(bytes + 8u, v->bytes, v->length);
        CHECK(feed(&stream, bytes, times, 8u + v->length, 100u, true, &used, &out) == NET_WIRE_OK);
        CHECK(stream.bad_crc >= 1u && memcmp(out.bytes, v->bytes, v->length) == 0);
        /* Original capture time must survive resync; a late final byte expires. */
        CHECK(init(&stream, v) == NET_WIRE_OK);
        times[8u + v->length - 1u] = 151u;
        memset(&out, 0x69, sizeof(out)); before = out;
        CHECK(feed(&stream, bytes, times, 8u + v->length, 151u, true, &used, &out) == NET_WIRE_NEED_MORE);
        CHECK(stream.assembly_timeouts >= 1u && memcmp(&out, &before, sizeof(out)) == 0);
    }
    return 0;
}

int main(int argc, char **argv)
{
    static const struct { const char *name; int (*run)(void); } cases[] = {
        {"structural42", structural42}, {"fragmented42", fragmented42},
        {"suffix_and_timeout42", suffix_and_timeout42}
    };
    size_t i;
    for (i = 0u; i < sizeof(cases) / sizeof(cases[0]); ++i) {
        if (argc == 1 || strcmp(argv[1], cases[i].name) == 0) {
            int result = cases[i].run();
            if (result != 0) return result;
            if (argc > 1) return 0;
        }
    }
    return argc == 1 ? 0 : 2;
}
