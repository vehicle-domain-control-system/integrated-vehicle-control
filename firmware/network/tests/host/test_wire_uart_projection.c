#include "network/net_wire_uart_m_projection.h"
#include <stddef.h>
#include <stdio.h>
#include <string.h>

#define CHECK(x) do { if (!(x)) { fprintf(stderr,"%s:%d: %s\n",__FILE__,__LINE__,#x); return 1; } } while (0)
#define CANDIDATE NET_WIRE_CANDIDATE_ONLY
typedef NetWireStatus (*Encode)(NetWireProfile, uint16_t, const void *, NetWireUartFrame *);
typedef NetWireStatus (*Decode)(NetWireProfile, const NetWireUartFrame *, void *, uint16_t *);
typedef union { max_align_t alignment; uint8_t bytes[512]; } Aligned;

/* Test-only CRC reference, distinct from the product framer and Python oracle. */
static void repair_crc(NetWireUartFrame *f)
{
    unsigned crc = 0xffffu;
    size_t i;
    unsigned j;
    for (i = 2u; i < f->length - 2u; ++i) {
        crc ^= (unsigned)f->bytes[i] << 8u;
        for (j = 0u; j < 8u; ++j)
            crc = (crc & 0x8000u) != 0u ? ((crc << 1u) ^ 0x1021u) & 0xffffu : (crc << 1u) & 0xffffu;
    }
    f->bytes[f->length - 2u] = (uint8_t)crc;
    f->bytes[f->length - 1u] = (uint8_t)(crc >> 8u);
}
static NetWireUartFrame literal_frame(const uint8_t *bytes, size_t length)
{
    NetWireUartFrame f = {0};
    f.length = length;
    f.captured_at_ms = UINT64_C(0x123456789abcdef0);
    memcpy(f.bytes, bytes, length);
    return f;
}
static void patch_field(NetWireUartFrame *f, size_t offset, unsigned width, uint64_t raw)
{
    unsigned i;
    for (i = 0u; i < width; ++i) f->bytes[8u + offset + i] = (uint8_t)(raw >> (8u * i));
    repair_crc(f);
}
static int rejected(Encode encode, Decode decode, const void *message, size_t dto_size,
    const NetWireUartFrame *bad, NetWireStatus expected)
{
    Aligned output, saved, input_saved;
    NetWireUartFrame frame, frame_saved;
    uint16_t sequence = UINT16_C(0xface);
    memset(&frame, 0xa6, sizeof(frame)); frame_saved = frame;
    memset(&output, 0x7b, sizeof(output)); saved = output;
    CHECK(dto_size <= sizeof(output));
    memcpy(input_saved.bytes, message, dto_size);
    CHECK(encode(CANDIDATE, 0x5aa5u, message, &frame) == expected);
    CHECK(memcmp(&frame, &frame_saved, sizeof(frame)) == 0);
    CHECK(decode(CANDIDATE, bad, output.bytes, &sequence) == expected);
    CHECK(memcmp(&output, &saved, sizeof(output)) == 0 && sequence == 0xfaceu);
    CHECK(memcmp(message, input_saved.bytes, dto_size) == 0);
    return 0;
}
static int common_errors(Encode encode, Decode decode, const void *message, size_t size,
    const uint8_t *golden, size_t length)
{
    NetWireUartFrame f = literal_frame(golden, length), bad, out, saved_out;
    Aligned output, saved, arena, arena_saved;
    NetWireUartPayload payload, payload_saved;
    uint16_t sequence;
    unsigned scenario;
    memset(&out, 0x93, sizeof(out)); saved_out = out;
    memset(&output, 0x7c, sizeof(output)); saved = output;
    CHECK(encode(NET_WIRE_NOT_BOUND, 0u, message, &out) == NET_WIRE_NOT_BOUND_STATUS);
    CHECK(memcmp(&out, &saved_out, sizeof(out)) == 0);
    sequence = 0xfaceu;
    CHECK(decode(NET_WIRE_NOT_BOUND, &f, output.bytes, &sequence) == NET_WIRE_NOT_BOUND_STATUS);
    CHECK(memcmp(&output, &saved, sizeof(output)) == 0 && sequence == 0xfaceu);
    CHECK(encode(CANDIDATE, 0u, NULL, &out) == NET_WIRE_INVALID_ARGUMENT);
    CHECK(encode(CANDIDATE, 0u, message, NULL) == NET_WIRE_INVALID_ARGUMENT);
    CHECK(memcmp(&out, &saved_out, sizeof(out)) == 0);
    CHECK(decode(CANDIDATE, NULL, output.bytes, &sequence) == NET_WIRE_INVALID_ARGUMENT);
    CHECK(decode(CANDIDATE, &f, NULL, &sequence) == NET_WIRE_INVALID_ARGUMENT);
    CHECK(decode(CANDIDATE, &f, output.bytes, NULL) == NET_WIRE_INVALID_ARGUMENT);
    CHECK(memcmp(&output, &saved, sizeof(output)) == 0 && sequence == 0xfaceu);
    /* All pairwise alias relationships, exact and partial, with aligned storage. */
    for (scenario = 0u; scenario < 2u; ++scenario) {
        const size_t offset = scenario == 0u ? 0u : 16u;
        memset(&arena, 0x55, sizeof(arena)); memcpy(arena.bytes, message, size); arena_saved = arena;
        CHECK(encode(CANDIDATE, 0u, arena.bytes, (NetWireUartFrame *)(void *)(arena.bytes + offset)) == NET_WIRE_INVALID_ARGUMENT);
        CHECK(memcmp(&arena, &arena_saved, sizeof(arena)) == 0);
        memset(&arena, 0x55, sizeof(arena)); memcpy(arena.bytes, &f, sizeof(f)); arena_saved = arena;
        CHECK(decode(CANDIDATE, (const NetWireUartFrame *)(const void *)arena.bytes,
            arena.bytes + offset, &sequence) == NET_WIRE_INVALID_ARGUMENT);
        CHECK(memcmp(&arena, &arena_saved, sizeof(arena)) == 0 && sequence == 0xfaceu);
        CHECK(decode(CANDIDATE, (const NetWireUartFrame *)(const void *)arena.bytes,
            output.bytes, (uint16_t *)(void *)(arena.bytes + offset)) == NET_WIRE_INVALID_ARGUMENT);
        CHECK(memcmp(&arena, &arena_saved, sizeof(arena)) == 0);
        CHECK(memcmp(&output, &saved, sizeof(output)) == 0);
        CHECK(decode(CANDIDATE, &f, output.bytes,
            (uint16_t *)(void *)(output.bytes + offset)) == NET_WIRE_INVALID_ARGUMENT);
        CHECK(memcmp(&output, &saved, sizeof(output)) == 0);
    }
    for (scenario = 0u; scenario < 12u; ++scenario) {
        bad = f;
        switch (scenario) {
        case 0u: bad.bytes[0] = 0u; break;
        case 1u: bad.bytes[1] = 0u; break;
        case 2u: bad.bytes[2] = 1u; repair_crc(&bad); break;
        case 3u: bad.bytes[3] = 0x01u; repair_crc(&bad); break; /* uplink type */
        case 4u: bad.bytes[3] = 0x7fu; repair_crc(&bad); break;
        case 5u: bad.bytes[4] ^= 1u; repair_crc(&bad); break;
        case 6u: bad.length = 9u; break;
        case 7u: bad.length = NET_WIRE_UART_FRAME_MAX + 1u; break;
        case 8u: bad.length--; break;
        case 9u: bad.length++; break;
        case 10u: bad.bytes[bad.length - 1u] ^= 0x80u; break;
        default: bad.bytes[3] = f.bytes[3] == 0x33u ? 0x38u : 0x33u; repair_crc(&bad); break;
        }
        sequence = 0xfaceu;
        CHECK(decode(CANDIDATE, &bad, output.bytes, &sequence) != NET_WIRE_OK);
        CHECK(memcmp(&output, &saved, sizeof(output)) == 0 && sequence == 0xfaceu);
    }
    memset(&payload, 0x12, sizeof(payload)); payload_saved = payload;
    CHECK(net_wire_uart_decode(CANDIDATE, NET_WIRE_UART_M, NET_WIRE_UPLINK, &f, &payload) == NET_WIRE_MALFORMED);
    CHECK(memcmp(&payload, &payload_saved, sizeof(payload)) == 0);
    CHECK(net_wire_uart_decode(CANDIDATE, NET_WIRE_UART_C, NET_WIRE_DOWNLINK, &f, &payload) == NET_WIRE_MALFORMED);
    CHECK(memcmp(&payload, &payload_saved, sizeof(payload)) == 0);
    CHECK(net_wire_uart_encode(CANDIDATE, NET_WIRE_UART_M, NET_WIRE_UPLINK,
        f.bytes[3], 0u, golden + 8u, length - 10u, &out) == NET_WIRE_MALFORMED);
    CHECK(memcmp(&out, &saved_out, sizeof(out)) == 0);
    return 0;
}

/* FINAL_98/docs/network/interface/03_COMMUNICATION/20_UART_M.md:386; independent Python struct/CRC literal. */
static const uint8_t golden_m_bcm_door_state[50] = {
    0xa5,0x5a,0x02,0x30,0x28,0x00,0xa5,0x5a,0xa5,0x5a,0x22,0x11,0x01,0x02,0x03,0x04,
    0x05,0x06,0x07,0x08,0x44,0x33,0x22,0x11,0x99,0x88,0x77,0x66,0xd4,0xc3,0xb2,0xa1,
    0x40,0x30,0x20,0x10,0x01,0x00,0x00,0x00,0x01,0x02,0x55,0x66,0x77,0x88,0xff,0xff,
    0x5a,0xd9,
};
static NetWireMBcmDoorStateRaw example_m_bcm_door_state(void)
{
    NetWireMBcmDoorStateRaw m = {0};
    m.device_context_id = UINT32_C(287464101);
    m.session_id = UINT64_C(578437695752307201);
    m.domain_boot_id = UINT32_C(287454020);
    m.query_id = UINT32_C(1719109785);
    m.original_can_boot_id = UINT32_C(2712847316);
    m.original_can_tx_sequence = UINT32_C(270544960);
    m.door_lock_state = UINT32_C(1);
    m.door_open_state = UINT32_C(0);
    m.door_composite_state = UINT32_C(0);
    m.lock_quality = UINT32_C(0);
    m.open_quality = UINT32_C(1);
    m.composite_quality = UINT32_C(2);
    m.update_sequence = UINT32_C(2289526357);
    m.source_age_ms = UINT32_C(65535);
    return m;
}
static int equal_m_bcm_door_state(const NetWireMBcmDoorStateRaw *a, const NetWireMBcmDoorStateRaw *b)
{
    return a->device_context_id == b->device_context_id &&
        a->session_id == b->session_id &&
        a->domain_boot_id == b->domain_boot_id &&
        a->query_id == b->query_id &&
        a->original_can_boot_id == b->original_can_boot_id &&
        a->original_can_tx_sequence == b->original_can_tx_sequence &&
        a->door_lock_state == b->door_lock_state &&
        a->door_open_state == b->door_open_state &&
        a->door_composite_state == b->door_composite_state &&
        a->lock_quality == b->lock_quality &&
        a->open_quality == b->open_quality &&
        a->composite_quality == b->composite_quality &&
        a->update_sequence == b->update_sequence &&
        a->source_age_ms == b->source_age_ms;
}
static NetWireStatus encode_m_bcm_door_state(NetWireProfile p, uint16_t s, const void *m, NetWireUartFrame *f)
{
    return net_wire_m_bcm_door_state_encode(p, s, (const NetWireMBcmDoorStateRaw *)m, f);
}
static NetWireStatus decode_m_bcm_door_state(NetWireProfile p, const NetWireUartFrame *f, void *m, uint16_t *s)
{
    return net_wire_m_bcm_door_state_decode(p, f, (NetWireMBcmDoorStateRaw *)m, s);
}
static int test_m_bcm_door_state(void)
{
    NetWireMBcmDoorStateRaw m = example_m_bcm_door_state(), output;
    NetWireUartFrame expected = literal_frame(golden_m_bcm_door_state, sizeof(golden_m_bcm_door_state)), actual;
    uint16_t sequence = 0u;
    CHECK(net_wire_m_bcm_door_state_encode(CANDIDATE, 0x5aa5u, &m, &actual) == NET_WIRE_OK);
    CHECK(actual.length == expected.length && memcmp(actual.bytes, expected.bytes, expected.length) == 0);
    CHECK(net_wire_m_bcm_door_state_decode(CANDIDATE, &expected, &output, &sequence) == NET_WIRE_OK);
    CHECK(sequence == 0x5aa5u && equal_m_bcm_door_state(&m, &output));
    CHECK(common_errors(encode_m_bcm_door_state, decode_m_bcm_door_state, &m, sizeof(m), golden_m_bcm_door_state, sizeof(golden_m_bcm_door_state)) == 0);
    expected = literal_frame(golden_m_bcm_door_state, sizeof(golden_m_bcm_door_state));
    expected.bytes[6] = 0u; expected.bytes[7] = 0u; repair_crc(&expected);
    CHECK(net_wire_m_bcm_door_state_encode(CANDIDATE, 0u, &m, &actual) == NET_WIRE_OK);
    CHECK(actual.length == expected.length && memcmp(actual.bytes, expected.bytes, expected.length) == 0);
    CHECK(net_wire_m_bcm_door_state_decode(CANDIDATE, &expected, &output, &sequence) == NET_WIRE_OK);
    CHECK(sequence == 0u && equal_m_bcm_door_state(&m, &output));
    expected = literal_frame(golden_m_bcm_door_state, sizeof(golden_m_bcm_door_state));
    expected.bytes[6] = 255u; expected.bytes[7] = 255u; repair_crc(&expected);
    CHECK(net_wire_m_bcm_door_state_encode(CANDIDATE, 65535u, &m, &actual) == NET_WIRE_OK);
    CHECK(actual.length == expected.length && memcmp(actual.bytes, expected.bytes, expected.length) == 0);
    CHECK(net_wire_m_bcm_door_state_decode(CANDIDATE, &expected, &output, &sequence) == NET_WIRE_OK);
    CHECK(sequence == 65535u && equal_m_bcm_door_state(&m, &output));
    m = example_m_bcm_door_state();
    expected = literal_frame(golden_m_bcm_door_state, sizeof(golden_m_bcm_door_state));
    m.device_context_id = UINT32_C(0);
    patch_field(&expected, 0u, 4u, UINT64_C(0));
    CHECK(rejected(encode_m_bcm_door_state, decode_m_bcm_door_state, &m, sizeof(m), &expected, NET_WIRE_MALFORMED) == 0);
    m = example_m_bcm_door_state();
    expected = literal_frame(golden_m_bcm_door_state, sizeof(golden_m_bcm_door_state));
    m.domain_boot_id = UINT32_C(0);
    patch_field(&expected, 12u, 4u, UINT64_C(0));
    CHECK(rejected(encode_m_bcm_door_state, decode_m_bcm_door_state, &m, sizeof(m), &expected, NET_WIRE_MALFORMED) == 0);
    m = example_m_bcm_door_state();
    expected = literal_frame(golden_m_bcm_door_state, sizeof(golden_m_bcm_door_state));
    m.original_can_boot_id = UINT32_C(0);
    patch_field(&expected, 20u, 4u, UINT64_C(0));
    CHECK(rejected(encode_m_bcm_door_state, decode_m_bcm_door_state, &m, sizeof(m), &expected, NET_WIRE_MALFORMED) == 0);
    m = example_m_bcm_door_state();
    expected = literal_frame(golden_m_bcm_door_state, sizeof(golden_m_bcm_door_state));
    m.door_lock_state = UINT32_C(2);
    patch_field(&expected, 28u, 1u, UINT64_C(2));
    CHECK(rejected(encode_m_bcm_door_state, decode_m_bcm_door_state, &m, sizeof(m), &expected, NET_WIRE_MALFORMED) == 0);
    m = example_m_bcm_door_state();
    expected = literal_frame(golden_m_bcm_door_state, sizeof(golden_m_bcm_door_state));
    m.door_open_state = UINT32_C(2);
    patch_field(&expected, 29u, 1u, UINT64_C(2));
    CHECK(rejected(encode_m_bcm_door_state, decode_m_bcm_door_state, &m, sizeof(m), &expected, NET_WIRE_MALFORMED) == 0);
    m = example_m_bcm_door_state();
    expected = literal_frame(golden_m_bcm_door_state, sizeof(golden_m_bcm_door_state));
    m.door_composite_state = UINT32_C(3);
    patch_field(&expected, 30u, 1u, UINT64_C(3));
    CHECK(rejected(encode_m_bcm_door_state, decode_m_bcm_door_state, &m, sizeof(m), &expected, NET_WIRE_MALFORMED) == 0);
    m = example_m_bcm_door_state();
    expected = literal_frame(golden_m_bcm_door_state, sizeof(golden_m_bcm_door_state));
    m.lock_quality = UINT32_C(4);
    patch_field(&expected, 31u, 1u, UINT64_C(4));
    CHECK(rejected(encode_m_bcm_door_state, decode_m_bcm_door_state, &m, sizeof(m), &expected, NET_WIRE_MALFORMED) == 0);
    m = example_m_bcm_door_state();
    expected = literal_frame(golden_m_bcm_door_state, sizeof(golden_m_bcm_door_state));
    m.open_quality = UINT32_C(4);
    patch_field(&expected, 32u, 1u, UINT64_C(4));
    CHECK(rejected(encode_m_bcm_door_state, decode_m_bcm_door_state, &m, sizeof(m), &expected, NET_WIRE_MALFORMED) == 0);
    m = example_m_bcm_door_state();
    expected = literal_frame(golden_m_bcm_door_state, sizeof(golden_m_bcm_door_state));
    m.composite_quality = UINT32_C(4);
    patch_field(&expected, 33u, 1u, UINT64_C(4));
    CHECK(rejected(encode_m_bcm_door_state, decode_m_bcm_door_state, &m, sizeof(m), &expected, NET_WIRE_MALFORMED) == 0);
    m = example_m_bcm_door_state();
    expected = literal_frame(golden_m_bcm_door_state, sizeof(golden_m_bcm_door_state));
    m.door_lock_state = UINT32_C(255);
    patch_field(&expected, 28u, 1u, UINT64_C(255));
    m.door_open_state = UINT32_C(255);
    patch_field(&expected, 29u, 1u, UINT64_C(255));
    m.lock_quality = UINT32_C(3);
    patch_field(&expected, 31u, 1u, UINT64_C(3));
    m.open_quality = UINT32_C(3);
    patch_field(&expected, 32u, 1u, UINT64_C(3));
    CHECK(net_wire_m_bcm_door_state_encode(CANDIDATE, 0x5aa5u, &m, &actual) == NET_WIRE_OK);
    CHECK(actual.length == expected.length && memcmp(actual.bytes, expected.bytes, expected.length) == 0);
    CHECK(net_wire_m_bcm_door_state_decode(CANDIDATE, &expected, &output, &sequence) == NET_WIRE_OK);
    CHECK(sequence == 0x5aa5u && equal_m_bcm_door_state(&m, &output));
    m = example_m_bcm_door_state();
    expected = literal_frame(golden_m_bcm_door_state, sizeof(golden_m_bcm_door_state));
    m.door_lock_state = UINT32_C(0);
    patch_field(&expected, 28u, 1u, UINT64_C(0));
    m.door_open_state = UINT32_C(0);
    patch_field(&expected, 29u, 1u, UINT64_C(0));
    m.door_composite_state = UINT32_C(2);
    patch_field(&expected, 30u, 1u, UINT64_C(2));
    CHECK(net_wire_m_bcm_door_state_encode(CANDIDATE, 0x5aa5u, &m, &actual) == NET_WIRE_OK);
    CHECK(actual.length == expected.length && memcmp(actual.bytes, expected.bytes, expected.length) == 0);
    CHECK(net_wire_m_bcm_door_state_decode(CANDIDATE, &expected, &output, &sequence) == NET_WIRE_OK);
    CHECK(sequence == 0x5aa5u && equal_m_bcm_door_state(&m, &output));
    m = example_m_bcm_door_state();
    expected = literal_frame(golden_m_bcm_door_state, sizeof(golden_m_bcm_door_state));
    m.device_context_id = UINT32_C(1);
    patch_field(&expected, 0u, 4u, UINT64_C(1));
    CHECK(net_wire_m_bcm_door_state_encode(CANDIDATE, 0x5aa5u, &m, &actual) == NET_WIRE_OK);
    CHECK(actual.length == expected.length && memcmp(actual.bytes, expected.bytes, expected.length) == 0);
    CHECK(net_wire_m_bcm_door_state_decode(CANDIDATE, &expected, &output, &sequence) == NET_WIRE_OK);
    CHECK(sequence == 0x5aa5u && equal_m_bcm_door_state(&m, &output));
    m = example_m_bcm_door_state();
    expected = literal_frame(golden_m_bcm_door_state, sizeof(golden_m_bcm_door_state));
    m.device_context_id = UINT32_C(4294967295);
    patch_field(&expected, 0u, 4u, UINT64_C(4294967295));
    CHECK(net_wire_m_bcm_door_state_encode(CANDIDATE, 0x5aa5u, &m, &actual) == NET_WIRE_OK);
    CHECK(actual.length == expected.length && memcmp(actual.bytes, expected.bytes, expected.length) == 0);
    CHECK(net_wire_m_bcm_door_state_decode(CANDIDATE, &expected, &output, &sequence) == NET_WIRE_OK);
    CHECK(sequence == 0x5aa5u && equal_m_bcm_door_state(&m, &output));
    m = example_m_bcm_door_state();
    expected = literal_frame(golden_m_bcm_door_state, sizeof(golden_m_bcm_door_state));
    m.domain_boot_id = UINT32_C(1);
    patch_field(&expected, 12u, 4u, UINT64_C(1));
    CHECK(net_wire_m_bcm_door_state_encode(CANDIDATE, 0x5aa5u, &m, &actual) == NET_WIRE_OK);
    CHECK(actual.length == expected.length && memcmp(actual.bytes, expected.bytes, expected.length) == 0);
    CHECK(net_wire_m_bcm_door_state_decode(CANDIDATE, &expected, &output, &sequence) == NET_WIRE_OK);
    CHECK(sequence == 0x5aa5u && equal_m_bcm_door_state(&m, &output));
    m = example_m_bcm_door_state();
    expected = literal_frame(golden_m_bcm_door_state, sizeof(golden_m_bcm_door_state));
    m.domain_boot_id = UINT32_C(4294967295);
    patch_field(&expected, 12u, 4u, UINT64_C(4294967295));
    CHECK(net_wire_m_bcm_door_state_encode(CANDIDATE, 0x5aa5u, &m, &actual) == NET_WIRE_OK);
    CHECK(actual.length == expected.length && memcmp(actual.bytes, expected.bytes, expected.length) == 0);
    CHECK(net_wire_m_bcm_door_state_decode(CANDIDATE, &expected, &output, &sequence) == NET_WIRE_OK);
    CHECK(sequence == 0x5aa5u && equal_m_bcm_door_state(&m, &output));
    m = example_m_bcm_door_state();
    expected = literal_frame(golden_m_bcm_door_state, sizeof(golden_m_bcm_door_state));
    m.original_can_boot_id = UINT32_C(1);
    patch_field(&expected, 20u, 4u, UINT64_C(1));
    CHECK(net_wire_m_bcm_door_state_encode(CANDIDATE, 0x5aa5u, &m, &actual) == NET_WIRE_OK);
    CHECK(actual.length == expected.length && memcmp(actual.bytes, expected.bytes, expected.length) == 0);
    CHECK(net_wire_m_bcm_door_state_decode(CANDIDATE, &expected, &output, &sequence) == NET_WIRE_OK);
    CHECK(sequence == 0x5aa5u && equal_m_bcm_door_state(&m, &output));
    m = example_m_bcm_door_state();
    expected = literal_frame(golden_m_bcm_door_state, sizeof(golden_m_bcm_door_state));
    m.original_can_boot_id = UINT32_C(4294967295);
    patch_field(&expected, 20u, 4u, UINT64_C(4294967295));
    CHECK(net_wire_m_bcm_door_state_encode(CANDIDATE, 0x5aa5u, &m, &actual) == NET_WIRE_OK);
    CHECK(actual.length == expected.length && memcmp(actual.bytes, expected.bytes, expected.length) == 0);
    CHECK(net_wire_m_bcm_door_state_decode(CANDIDATE, &expected, &output, &sequence) == NET_WIRE_OK);
    CHECK(sequence == 0x5aa5u && equal_m_bcm_door_state(&m, &output));
    m = example_m_bcm_door_state();
    expected = literal_frame(golden_m_bcm_door_state, sizeof(golden_m_bcm_door_state));
    m.session_id = UINT64_C(0);
    patch_field(&expected, 4u, 8u, UINT64_C(0));
    CHECK(net_wire_m_bcm_door_state_encode(CANDIDATE, 0x5aa5u, &m, &actual) == NET_WIRE_OK);
    CHECK(actual.length == expected.length && memcmp(actual.bytes, expected.bytes, expected.length) == 0);
    CHECK(net_wire_m_bcm_door_state_decode(CANDIDATE, &expected, &output, &sequence) == NET_WIRE_OK);
    CHECK(sequence == 0x5aa5u && equal_m_bcm_door_state(&m, &output));
    m = example_m_bcm_door_state();
    expected = literal_frame(golden_m_bcm_door_state, sizeof(golden_m_bcm_door_state));
    m.session_id = UINT64_C(18446744073709551615);
    patch_field(&expected, 4u, 8u, UINT64_C(18446744073709551615));
    CHECK(net_wire_m_bcm_door_state_encode(CANDIDATE, 0x5aa5u, &m, &actual) == NET_WIRE_OK);
    CHECK(actual.length == expected.length && memcmp(actual.bytes, expected.bytes, expected.length) == 0);
    CHECK(net_wire_m_bcm_door_state_decode(CANDIDATE, &expected, &output, &sequence) == NET_WIRE_OK);
    CHECK(sequence == 0x5aa5u && equal_m_bcm_door_state(&m, &output));
    m = example_m_bcm_door_state();
    expected = literal_frame(golden_m_bcm_door_state, sizeof(golden_m_bcm_door_state));
    m.query_id = UINT32_C(0);
    patch_field(&expected, 16u, 4u, UINT64_C(0));
    CHECK(net_wire_m_bcm_door_state_encode(CANDIDATE, 0x5aa5u, &m, &actual) == NET_WIRE_OK);
    CHECK(actual.length == expected.length && memcmp(actual.bytes, expected.bytes, expected.length) == 0);
    CHECK(net_wire_m_bcm_door_state_decode(CANDIDATE, &expected, &output, &sequence) == NET_WIRE_OK);
    CHECK(sequence == 0x5aa5u && equal_m_bcm_door_state(&m, &output));
    m = example_m_bcm_door_state();
    expected = literal_frame(golden_m_bcm_door_state, sizeof(golden_m_bcm_door_state));
    m.query_id = UINT32_C(4294967295);
    patch_field(&expected, 16u, 4u, UINT64_C(4294967295));
    CHECK(net_wire_m_bcm_door_state_encode(CANDIDATE, 0x5aa5u, &m, &actual) == NET_WIRE_OK);
    CHECK(actual.length == expected.length && memcmp(actual.bytes, expected.bytes, expected.length) == 0);
    CHECK(net_wire_m_bcm_door_state_decode(CANDIDATE, &expected, &output, &sequence) == NET_WIRE_OK);
    CHECK(sequence == 0x5aa5u && equal_m_bcm_door_state(&m, &output));
    m = example_m_bcm_door_state();
    expected = literal_frame(golden_m_bcm_door_state, sizeof(golden_m_bcm_door_state));
    m.original_can_tx_sequence = UINT32_C(0);
    patch_field(&expected, 24u, 4u, UINT64_C(0));
    CHECK(net_wire_m_bcm_door_state_encode(CANDIDATE, 0x5aa5u, &m, &actual) == NET_WIRE_OK);
    CHECK(actual.length == expected.length && memcmp(actual.bytes, expected.bytes, expected.length) == 0);
    CHECK(net_wire_m_bcm_door_state_decode(CANDIDATE, &expected, &output, &sequence) == NET_WIRE_OK);
    CHECK(sequence == 0x5aa5u && equal_m_bcm_door_state(&m, &output));
    m = example_m_bcm_door_state();
    expected = literal_frame(golden_m_bcm_door_state, sizeof(golden_m_bcm_door_state));
    m.original_can_tx_sequence = UINT32_C(4294967295);
    patch_field(&expected, 24u, 4u, UINT64_C(4294967295));
    CHECK(net_wire_m_bcm_door_state_encode(CANDIDATE, 0x5aa5u, &m, &actual) == NET_WIRE_OK);
    CHECK(actual.length == expected.length && memcmp(actual.bytes, expected.bytes, expected.length) == 0);
    CHECK(net_wire_m_bcm_door_state_decode(CANDIDATE, &expected, &output, &sequence) == NET_WIRE_OK);
    CHECK(sequence == 0x5aa5u && equal_m_bcm_door_state(&m, &output));
    m = example_m_bcm_door_state();
    expected = literal_frame(golden_m_bcm_door_state, sizeof(golden_m_bcm_door_state));
    m.update_sequence = UINT32_C(0);
    patch_field(&expected, 34u, 4u, UINT64_C(0));
    CHECK(net_wire_m_bcm_door_state_encode(CANDIDATE, 0x5aa5u, &m, &actual) == NET_WIRE_OK);
    CHECK(actual.length == expected.length && memcmp(actual.bytes, expected.bytes, expected.length) == 0);
    CHECK(net_wire_m_bcm_door_state_decode(CANDIDATE, &expected, &output, &sequence) == NET_WIRE_OK);
    CHECK(sequence == 0x5aa5u && equal_m_bcm_door_state(&m, &output));
    m = example_m_bcm_door_state();
    expected = literal_frame(golden_m_bcm_door_state, sizeof(golden_m_bcm_door_state));
    m.update_sequence = UINT32_C(4294967295);
    patch_field(&expected, 34u, 4u, UINT64_C(4294967295));
    CHECK(net_wire_m_bcm_door_state_encode(CANDIDATE, 0x5aa5u, &m, &actual) == NET_WIRE_OK);
    CHECK(actual.length == expected.length && memcmp(actual.bytes, expected.bytes, expected.length) == 0);
    CHECK(net_wire_m_bcm_door_state_decode(CANDIDATE, &expected, &output, &sequence) == NET_WIRE_OK);
    CHECK(sequence == 0x5aa5u && equal_m_bcm_door_state(&m, &output));
    m = example_m_bcm_door_state();
    expected = literal_frame(golden_m_bcm_door_state, sizeof(golden_m_bcm_door_state));
    m.source_age_ms = UINT32_C(0);
    patch_field(&expected, 38u, 2u, UINT64_C(0));
    CHECK(net_wire_m_bcm_door_state_encode(CANDIDATE, 0x5aa5u, &m, &actual) == NET_WIRE_OK);
    CHECK(actual.length == expected.length && memcmp(actual.bytes, expected.bytes, expected.length) == 0);
    CHECK(net_wire_m_bcm_door_state_decode(CANDIDATE, &expected, &output, &sequence) == NET_WIRE_OK);
    CHECK(sequence == 0x5aa5u && equal_m_bcm_door_state(&m, &output));
    m = example_m_bcm_door_state();
    expected = literal_frame(golden_m_bcm_door_state, sizeof(golden_m_bcm_door_state));
    m.source_age_ms = UINT32_C(65535);
    patch_field(&expected, 38u, 2u, UINT64_C(65535));
    CHECK(net_wire_m_bcm_door_state_encode(CANDIDATE, 0x5aa5u, &m, &actual) == NET_WIRE_OK);
    CHECK(actual.length == expected.length && memcmp(actual.bytes, expected.bytes, expected.length) == 0);
    CHECK(net_wire_m_bcm_door_state_decode(CANDIDATE, &expected, &output, &sequence) == NET_WIRE_OK);
    CHECK(sequence == 0x5aa5u && equal_m_bcm_door_state(&m, &output));
    return 0;
}

/* FINAL_98/docs/network/interface/03_COMMUNICATION/20_UART_M.md:408; independent Python struct/CRC literal. */
static const uint8_t golden_m_bcm_climate_state[54] = {
    0xa5,0x5a,0x02,0x31,0x2c,0x00,0xa5,0x5a,0xa5,0x5a,0x22,0x11,0x01,0x02,0x03,0x04,
    0x05,0x06,0x07,0x08,0x44,0x33,0x22,0x11,0x99,0x88,0x77,0x66,0xd4,0xc3,0xb2,0xa1,
    0x40,0x30,0x20,0x10,0x03,0xff,0x01,0x64,0xff,0x00,0x03,0x00,0x01,0x03,0x78,0x56,
    0x34,0x12,0x34,0x12,0x49,0x88,
};
static NetWireMBcmClimateStateRaw example_m_bcm_climate_state(void)
{
    NetWireMBcmClimateStateRaw m = {0};
    m.device_context_id = UINT32_C(287464101);
    m.session_id = UINT64_C(578437695752307201);
    m.domain_boot_id = UINT32_C(287454020);
    m.query_id = UINT32_C(1719109785);
    m.original_can_boot_id = UINT32_C(2712847316);
    m.original_can_tx_sequence = UINT32_C(270544960);
    m.fan_command_level = UINT32_C(3);
    m.fan_measured_level = UINT32_C(255);
    m.thermal_direction_state = UINT32_C(1);
    m.thermal_output_percent = UINT32_C(100);
    m.heat_removal_state = UINT32_C(255);
    m.fan_command_quality = UINT32_C(0);
    m.fan_measured_quality = UINT32_C(3);
    m.thermal_direction_quality = UINT32_C(0);
    m.thermal_output_quality = UINT32_C(1);
    m.heat_removal_quality = UINT32_C(3);
    m.update_sequence = UINT32_C(305419896);
    m.source_age_ms = UINT32_C(4660);
    return m;
}
static int equal_m_bcm_climate_state(const NetWireMBcmClimateStateRaw *a, const NetWireMBcmClimateStateRaw *b)
{
    return a->device_context_id == b->device_context_id &&
        a->session_id == b->session_id &&
        a->domain_boot_id == b->domain_boot_id &&
        a->query_id == b->query_id &&
        a->original_can_boot_id == b->original_can_boot_id &&
        a->original_can_tx_sequence == b->original_can_tx_sequence &&
        a->fan_command_level == b->fan_command_level &&
        a->fan_measured_level == b->fan_measured_level &&
        a->thermal_direction_state == b->thermal_direction_state &&
        a->thermal_output_percent == b->thermal_output_percent &&
        a->heat_removal_state == b->heat_removal_state &&
        a->fan_command_quality == b->fan_command_quality &&
        a->fan_measured_quality == b->fan_measured_quality &&
        a->thermal_direction_quality == b->thermal_direction_quality &&
        a->thermal_output_quality == b->thermal_output_quality &&
        a->heat_removal_quality == b->heat_removal_quality &&
        a->update_sequence == b->update_sequence &&
        a->source_age_ms == b->source_age_ms;
}
static NetWireStatus encode_m_bcm_climate_state(NetWireProfile p, uint16_t s, const void *m, NetWireUartFrame *f)
{
    return net_wire_m_bcm_climate_state_encode(p, s, (const NetWireMBcmClimateStateRaw *)m, f);
}
static NetWireStatus decode_m_bcm_climate_state(NetWireProfile p, const NetWireUartFrame *f, void *m, uint16_t *s)
{
    return net_wire_m_bcm_climate_state_decode(p, f, (NetWireMBcmClimateStateRaw *)m, s);
}
static int test_m_bcm_climate_state(void)
{
    NetWireMBcmClimateStateRaw m = example_m_bcm_climate_state(), output;
    NetWireUartFrame expected = literal_frame(golden_m_bcm_climate_state, sizeof(golden_m_bcm_climate_state)), actual;
    uint16_t sequence = 0u;
    CHECK(net_wire_m_bcm_climate_state_encode(CANDIDATE, 0x5aa5u, &m, &actual) == NET_WIRE_OK);
    CHECK(actual.length == expected.length && memcmp(actual.bytes, expected.bytes, expected.length) == 0);
    CHECK(net_wire_m_bcm_climate_state_decode(CANDIDATE, &expected, &output, &sequence) == NET_WIRE_OK);
    CHECK(sequence == 0x5aa5u && equal_m_bcm_climate_state(&m, &output));
    CHECK(common_errors(encode_m_bcm_climate_state, decode_m_bcm_climate_state, &m, sizeof(m), golden_m_bcm_climate_state, sizeof(golden_m_bcm_climate_state)) == 0);
    expected = literal_frame(golden_m_bcm_climate_state, sizeof(golden_m_bcm_climate_state));
    expected.bytes[6] = 0u; expected.bytes[7] = 0u; repair_crc(&expected);
    CHECK(net_wire_m_bcm_climate_state_encode(CANDIDATE, 0u, &m, &actual) == NET_WIRE_OK);
    CHECK(actual.length == expected.length && memcmp(actual.bytes, expected.bytes, expected.length) == 0);
    CHECK(net_wire_m_bcm_climate_state_decode(CANDIDATE, &expected, &output, &sequence) == NET_WIRE_OK);
    CHECK(sequence == 0u && equal_m_bcm_climate_state(&m, &output));
    expected = literal_frame(golden_m_bcm_climate_state, sizeof(golden_m_bcm_climate_state));
    expected.bytes[6] = 255u; expected.bytes[7] = 255u; repair_crc(&expected);
    CHECK(net_wire_m_bcm_climate_state_encode(CANDIDATE, 65535u, &m, &actual) == NET_WIRE_OK);
    CHECK(actual.length == expected.length && memcmp(actual.bytes, expected.bytes, expected.length) == 0);
    CHECK(net_wire_m_bcm_climate_state_decode(CANDIDATE, &expected, &output, &sequence) == NET_WIRE_OK);
    CHECK(sequence == 65535u && equal_m_bcm_climate_state(&m, &output));
    m = example_m_bcm_climate_state();
    expected = literal_frame(golden_m_bcm_climate_state, sizeof(golden_m_bcm_climate_state));
    m.device_context_id = UINT32_C(0);
    patch_field(&expected, 0u, 4u, UINT64_C(0));
    CHECK(rejected(encode_m_bcm_climate_state, decode_m_bcm_climate_state, &m, sizeof(m), &expected, NET_WIRE_MALFORMED) == 0);
    m = example_m_bcm_climate_state();
    expected = literal_frame(golden_m_bcm_climate_state, sizeof(golden_m_bcm_climate_state));
    m.domain_boot_id = UINT32_C(0);
    patch_field(&expected, 12u, 4u, UINT64_C(0));
    CHECK(rejected(encode_m_bcm_climate_state, decode_m_bcm_climate_state, &m, sizeof(m), &expected, NET_WIRE_MALFORMED) == 0);
    m = example_m_bcm_climate_state();
    expected = literal_frame(golden_m_bcm_climate_state, sizeof(golden_m_bcm_climate_state));
    m.original_can_boot_id = UINT32_C(0);
    patch_field(&expected, 20u, 4u, UINT64_C(0));
    CHECK(rejected(encode_m_bcm_climate_state, decode_m_bcm_climate_state, &m, sizeof(m), &expected, NET_WIRE_MALFORMED) == 0);
    m = example_m_bcm_climate_state();
    expected = literal_frame(golden_m_bcm_climate_state, sizeof(golden_m_bcm_climate_state));
    m.fan_command_level = UINT32_C(255);
    patch_field(&expected, 28u, 1u, UINT64_C(255));
    CHECK(rejected(encode_m_bcm_climate_state, decode_m_bcm_climate_state, &m, sizeof(m), &expected, NET_WIRE_MALFORMED) == 0);
    m = example_m_bcm_climate_state();
    expected = literal_frame(golden_m_bcm_climate_state, sizeof(golden_m_bcm_climate_state));
    m.fan_measured_level = UINT32_C(4);
    patch_field(&expected, 29u, 1u, UINT64_C(4));
    CHECK(rejected(encode_m_bcm_climate_state, decode_m_bcm_climate_state, &m, sizeof(m), &expected, NET_WIRE_MALFORMED) == 0);
    m = example_m_bcm_climate_state();
    expected = literal_frame(golden_m_bcm_climate_state, sizeof(golden_m_bcm_climate_state));
    m.thermal_direction_state = UINT32_C(3);
    patch_field(&expected, 30u, 1u, UINT64_C(3));
    CHECK(rejected(encode_m_bcm_climate_state, decode_m_bcm_climate_state, &m, sizeof(m), &expected, NET_WIRE_MALFORMED) == 0);
    m = example_m_bcm_climate_state();
    expected = literal_frame(golden_m_bcm_climate_state, sizeof(golden_m_bcm_climate_state));
    m.thermal_output_percent = UINT32_C(101);
    patch_field(&expected, 31u, 1u, UINT64_C(101));
    CHECK(rejected(encode_m_bcm_climate_state, decode_m_bcm_climate_state, &m, sizeof(m), &expected, NET_WIRE_MALFORMED) == 0);
    m = example_m_bcm_climate_state();
    expected = literal_frame(golden_m_bcm_climate_state, sizeof(golden_m_bcm_climate_state));
    m.heat_removal_state = UINT32_C(2);
    patch_field(&expected, 32u, 1u, UINT64_C(2));
    CHECK(rejected(encode_m_bcm_climate_state, decode_m_bcm_climate_state, &m, sizeof(m), &expected, NET_WIRE_MALFORMED) == 0);
    m = example_m_bcm_climate_state();
    expected = literal_frame(golden_m_bcm_climate_state, sizeof(golden_m_bcm_climate_state));
    m.fan_command_quality = UINT32_C(4);
    patch_field(&expected, 33u, 1u, UINT64_C(4));
    CHECK(rejected(encode_m_bcm_climate_state, decode_m_bcm_climate_state, &m, sizeof(m), &expected, NET_WIRE_MALFORMED) == 0);
    m = example_m_bcm_climate_state();
    expected = literal_frame(golden_m_bcm_climate_state, sizeof(golden_m_bcm_climate_state));
    m.fan_measured_quality = UINT32_C(4);
    patch_field(&expected, 34u, 1u, UINT64_C(4));
    CHECK(rejected(encode_m_bcm_climate_state, decode_m_bcm_climate_state, &m, sizeof(m), &expected, NET_WIRE_MALFORMED) == 0);
    m = example_m_bcm_climate_state();
    expected = literal_frame(golden_m_bcm_climate_state, sizeof(golden_m_bcm_climate_state));
    m.thermal_direction_quality = UINT32_C(4);
    patch_field(&expected, 35u, 1u, UINT64_C(4));
    CHECK(rejected(encode_m_bcm_climate_state, decode_m_bcm_climate_state, &m, sizeof(m), &expected, NET_WIRE_MALFORMED) == 0);
    m = example_m_bcm_climate_state();
    expected = literal_frame(golden_m_bcm_climate_state, sizeof(golden_m_bcm_climate_state));
    m.thermal_output_quality = UINT32_C(4);
    patch_field(&expected, 36u, 1u, UINT64_C(4));
    CHECK(rejected(encode_m_bcm_climate_state, decode_m_bcm_climate_state, &m, sizeof(m), &expected, NET_WIRE_MALFORMED) == 0);
    m = example_m_bcm_climate_state();
    expected = literal_frame(golden_m_bcm_climate_state, sizeof(golden_m_bcm_climate_state));
    m.heat_removal_quality = UINT32_C(4);
    patch_field(&expected, 37u, 1u, UINT64_C(4));
    CHECK(rejected(encode_m_bcm_climate_state, decode_m_bcm_climate_state, &m, sizeof(m), &expected, NET_WIRE_MALFORMED) == 0);
    m = example_m_bcm_climate_state();
    expected = literal_frame(golden_m_bcm_climate_state, sizeof(golden_m_bcm_climate_state));
    m.fan_command_level = UINT32_C(0);
    patch_field(&expected, 28u, 1u, UINT64_C(0));
    m.fan_measured_level = UINT32_C(0);
    patch_field(&expected, 29u, 1u, UINT64_C(0));
    m.thermal_output_percent = UINT32_C(0);
    patch_field(&expected, 31u, 1u, UINT64_C(0));
    m.heat_removal_state = UINT32_C(0);
    patch_field(&expected, 32u, 1u, UINT64_C(0));
    CHECK(net_wire_m_bcm_climate_state_encode(CANDIDATE, 0x5aa5u, &m, &actual) == NET_WIRE_OK);
    CHECK(actual.length == expected.length && memcmp(actual.bytes, expected.bytes, expected.length) == 0);
    CHECK(net_wire_m_bcm_climate_state_decode(CANDIDATE, &expected, &output, &sequence) == NET_WIRE_OK);
    CHECK(sequence == 0x5aa5u && equal_m_bcm_climate_state(&m, &output));
    m = example_m_bcm_climate_state();
    expected = literal_frame(golden_m_bcm_climate_state, sizeof(golden_m_bcm_climate_state));
    m.fan_command_level = UINT32_C(3);
    patch_field(&expected, 28u, 1u, UINT64_C(3));
    m.fan_measured_level = UINT32_C(3);
    patch_field(&expected, 29u, 1u, UINT64_C(3));
    m.thermal_direction_state = UINT32_C(2);
    patch_field(&expected, 30u, 1u, UINT64_C(2));
    m.thermal_output_percent = UINT32_C(100);
    patch_field(&expected, 31u, 1u, UINT64_C(100));
    CHECK(net_wire_m_bcm_climate_state_encode(CANDIDATE, 0x5aa5u, &m, &actual) == NET_WIRE_OK);
    CHECK(actual.length == expected.length && memcmp(actual.bytes, expected.bytes, expected.length) == 0);
    CHECK(net_wire_m_bcm_climate_state_decode(CANDIDATE, &expected, &output, &sequence) == NET_WIRE_OK);
    CHECK(sequence == 0x5aa5u && equal_m_bcm_climate_state(&m, &output));
    m = example_m_bcm_climate_state();
    expected = literal_frame(golden_m_bcm_climate_state, sizeof(golden_m_bcm_climate_state));
    m.device_context_id = UINT32_C(1);
    patch_field(&expected, 0u, 4u, UINT64_C(1));
    CHECK(net_wire_m_bcm_climate_state_encode(CANDIDATE, 0x5aa5u, &m, &actual) == NET_WIRE_OK);
    CHECK(actual.length == expected.length && memcmp(actual.bytes, expected.bytes, expected.length) == 0);
    CHECK(net_wire_m_bcm_climate_state_decode(CANDIDATE, &expected, &output, &sequence) == NET_WIRE_OK);
    CHECK(sequence == 0x5aa5u && equal_m_bcm_climate_state(&m, &output));
    m = example_m_bcm_climate_state();
    expected = literal_frame(golden_m_bcm_climate_state, sizeof(golden_m_bcm_climate_state));
    m.device_context_id = UINT32_C(4294967295);
    patch_field(&expected, 0u, 4u, UINT64_C(4294967295));
    CHECK(net_wire_m_bcm_climate_state_encode(CANDIDATE, 0x5aa5u, &m, &actual) == NET_WIRE_OK);
    CHECK(actual.length == expected.length && memcmp(actual.bytes, expected.bytes, expected.length) == 0);
    CHECK(net_wire_m_bcm_climate_state_decode(CANDIDATE, &expected, &output, &sequence) == NET_WIRE_OK);
    CHECK(sequence == 0x5aa5u && equal_m_bcm_climate_state(&m, &output));
    m = example_m_bcm_climate_state();
    expected = literal_frame(golden_m_bcm_climate_state, sizeof(golden_m_bcm_climate_state));
    m.domain_boot_id = UINT32_C(1);
    patch_field(&expected, 12u, 4u, UINT64_C(1));
    CHECK(net_wire_m_bcm_climate_state_encode(CANDIDATE, 0x5aa5u, &m, &actual) == NET_WIRE_OK);
    CHECK(actual.length == expected.length && memcmp(actual.bytes, expected.bytes, expected.length) == 0);
    CHECK(net_wire_m_bcm_climate_state_decode(CANDIDATE, &expected, &output, &sequence) == NET_WIRE_OK);
    CHECK(sequence == 0x5aa5u && equal_m_bcm_climate_state(&m, &output));
    m = example_m_bcm_climate_state();
    expected = literal_frame(golden_m_bcm_climate_state, sizeof(golden_m_bcm_climate_state));
    m.domain_boot_id = UINT32_C(4294967295);
    patch_field(&expected, 12u, 4u, UINT64_C(4294967295));
    CHECK(net_wire_m_bcm_climate_state_encode(CANDIDATE, 0x5aa5u, &m, &actual) == NET_WIRE_OK);
    CHECK(actual.length == expected.length && memcmp(actual.bytes, expected.bytes, expected.length) == 0);
    CHECK(net_wire_m_bcm_climate_state_decode(CANDIDATE, &expected, &output, &sequence) == NET_WIRE_OK);
    CHECK(sequence == 0x5aa5u && equal_m_bcm_climate_state(&m, &output));
    m = example_m_bcm_climate_state();
    expected = literal_frame(golden_m_bcm_climate_state, sizeof(golden_m_bcm_climate_state));
    m.original_can_boot_id = UINT32_C(1);
    patch_field(&expected, 20u, 4u, UINT64_C(1));
    CHECK(net_wire_m_bcm_climate_state_encode(CANDIDATE, 0x5aa5u, &m, &actual) == NET_WIRE_OK);
    CHECK(actual.length == expected.length && memcmp(actual.bytes, expected.bytes, expected.length) == 0);
    CHECK(net_wire_m_bcm_climate_state_decode(CANDIDATE, &expected, &output, &sequence) == NET_WIRE_OK);
    CHECK(sequence == 0x5aa5u && equal_m_bcm_climate_state(&m, &output));
    m = example_m_bcm_climate_state();
    expected = literal_frame(golden_m_bcm_climate_state, sizeof(golden_m_bcm_climate_state));
    m.original_can_boot_id = UINT32_C(4294967295);
    patch_field(&expected, 20u, 4u, UINT64_C(4294967295));
    CHECK(net_wire_m_bcm_climate_state_encode(CANDIDATE, 0x5aa5u, &m, &actual) == NET_WIRE_OK);
    CHECK(actual.length == expected.length && memcmp(actual.bytes, expected.bytes, expected.length) == 0);
    CHECK(net_wire_m_bcm_climate_state_decode(CANDIDATE, &expected, &output, &sequence) == NET_WIRE_OK);
    CHECK(sequence == 0x5aa5u && equal_m_bcm_climate_state(&m, &output));
    m = example_m_bcm_climate_state();
    expected = literal_frame(golden_m_bcm_climate_state, sizeof(golden_m_bcm_climate_state));
    m.session_id = UINT64_C(0);
    patch_field(&expected, 4u, 8u, UINT64_C(0));
    CHECK(net_wire_m_bcm_climate_state_encode(CANDIDATE, 0x5aa5u, &m, &actual) == NET_WIRE_OK);
    CHECK(actual.length == expected.length && memcmp(actual.bytes, expected.bytes, expected.length) == 0);
    CHECK(net_wire_m_bcm_climate_state_decode(CANDIDATE, &expected, &output, &sequence) == NET_WIRE_OK);
    CHECK(sequence == 0x5aa5u && equal_m_bcm_climate_state(&m, &output));
    m = example_m_bcm_climate_state();
    expected = literal_frame(golden_m_bcm_climate_state, sizeof(golden_m_bcm_climate_state));
    m.session_id = UINT64_C(18446744073709551615);
    patch_field(&expected, 4u, 8u, UINT64_C(18446744073709551615));
    CHECK(net_wire_m_bcm_climate_state_encode(CANDIDATE, 0x5aa5u, &m, &actual) == NET_WIRE_OK);
    CHECK(actual.length == expected.length && memcmp(actual.bytes, expected.bytes, expected.length) == 0);
    CHECK(net_wire_m_bcm_climate_state_decode(CANDIDATE, &expected, &output, &sequence) == NET_WIRE_OK);
    CHECK(sequence == 0x5aa5u && equal_m_bcm_climate_state(&m, &output));
    m = example_m_bcm_climate_state();
    expected = literal_frame(golden_m_bcm_climate_state, sizeof(golden_m_bcm_climate_state));
    m.query_id = UINT32_C(0);
    patch_field(&expected, 16u, 4u, UINT64_C(0));
    CHECK(net_wire_m_bcm_climate_state_encode(CANDIDATE, 0x5aa5u, &m, &actual) == NET_WIRE_OK);
    CHECK(actual.length == expected.length && memcmp(actual.bytes, expected.bytes, expected.length) == 0);
    CHECK(net_wire_m_bcm_climate_state_decode(CANDIDATE, &expected, &output, &sequence) == NET_WIRE_OK);
    CHECK(sequence == 0x5aa5u && equal_m_bcm_climate_state(&m, &output));
    m = example_m_bcm_climate_state();
    expected = literal_frame(golden_m_bcm_climate_state, sizeof(golden_m_bcm_climate_state));
    m.query_id = UINT32_C(4294967295);
    patch_field(&expected, 16u, 4u, UINT64_C(4294967295));
    CHECK(net_wire_m_bcm_climate_state_encode(CANDIDATE, 0x5aa5u, &m, &actual) == NET_WIRE_OK);
    CHECK(actual.length == expected.length && memcmp(actual.bytes, expected.bytes, expected.length) == 0);
    CHECK(net_wire_m_bcm_climate_state_decode(CANDIDATE, &expected, &output, &sequence) == NET_WIRE_OK);
    CHECK(sequence == 0x5aa5u && equal_m_bcm_climate_state(&m, &output));
    m = example_m_bcm_climate_state();
    expected = literal_frame(golden_m_bcm_climate_state, sizeof(golden_m_bcm_climate_state));
    m.original_can_tx_sequence = UINT32_C(0);
    patch_field(&expected, 24u, 4u, UINT64_C(0));
    CHECK(net_wire_m_bcm_climate_state_encode(CANDIDATE, 0x5aa5u, &m, &actual) == NET_WIRE_OK);
    CHECK(actual.length == expected.length && memcmp(actual.bytes, expected.bytes, expected.length) == 0);
    CHECK(net_wire_m_bcm_climate_state_decode(CANDIDATE, &expected, &output, &sequence) == NET_WIRE_OK);
    CHECK(sequence == 0x5aa5u && equal_m_bcm_climate_state(&m, &output));
    m = example_m_bcm_climate_state();
    expected = literal_frame(golden_m_bcm_climate_state, sizeof(golden_m_bcm_climate_state));
    m.original_can_tx_sequence = UINT32_C(4294967295);
    patch_field(&expected, 24u, 4u, UINT64_C(4294967295));
    CHECK(net_wire_m_bcm_climate_state_encode(CANDIDATE, 0x5aa5u, &m, &actual) == NET_WIRE_OK);
    CHECK(actual.length == expected.length && memcmp(actual.bytes, expected.bytes, expected.length) == 0);
    CHECK(net_wire_m_bcm_climate_state_decode(CANDIDATE, &expected, &output, &sequence) == NET_WIRE_OK);
    CHECK(sequence == 0x5aa5u && equal_m_bcm_climate_state(&m, &output));
    m = example_m_bcm_climate_state();
    expected = literal_frame(golden_m_bcm_climate_state, sizeof(golden_m_bcm_climate_state));
    m.update_sequence = UINT32_C(0);
    patch_field(&expected, 38u, 4u, UINT64_C(0));
    CHECK(net_wire_m_bcm_climate_state_encode(CANDIDATE, 0x5aa5u, &m, &actual) == NET_WIRE_OK);
    CHECK(actual.length == expected.length && memcmp(actual.bytes, expected.bytes, expected.length) == 0);
    CHECK(net_wire_m_bcm_climate_state_decode(CANDIDATE, &expected, &output, &sequence) == NET_WIRE_OK);
    CHECK(sequence == 0x5aa5u && equal_m_bcm_climate_state(&m, &output));
    m = example_m_bcm_climate_state();
    expected = literal_frame(golden_m_bcm_climate_state, sizeof(golden_m_bcm_climate_state));
    m.update_sequence = UINT32_C(4294967295);
    patch_field(&expected, 38u, 4u, UINT64_C(4294967295));
    CHECK(net_wire_m_bcm_climate_state_encode(CANDIDATE, 0x5aa5u, &m, &actual) == NET_WIRE_OK);
    CHECK(actual.length == expected.length && memcmp(actual.bytes, expected.bytes, expected.length) == 0);
    CHECK(net_wire_m_bcm_climate_state_decode(CANDIDATE, &expected, &output, &sequence) == NET_WIRE_OK);
    CHECK(sequence == 0x5aa5u && equal_m_bcm_climate_state(&m, &output));
    m = example_m_bcm_climate_state();
    expected = literal_frame(golden_m_bcm_climate_state, sizeof(golden_m_bcm_climate_state));
    m.source_age_ms = UINT32_C(0);
    patch_field(&expected, 42u, 2u, UINT64_C(0));
    CHECK(net_wire_m_bcm_climate_state_encode(CANDIDATE, 0x5aa5u, &m, &actual) == NET_WIRE_OK);
    CHECK(actual.length == expected.length && memcmp(actual.bytes, expected.bytes, expected.length) == 0);
    CHECK(net_wire_m_bcm_climate_state_decode(CANDIDATE, &expected, &output, &sequence) == NET_WIRE_OK);
    CHECK(sequence == 0x5aa5u && equal_m_bcm_climate_state(&m, &output));
    m = example_m_bcm_climate_state();
    expected = literal_frame(golden_m_bcm_climate_state, sizeof(golden_m_bcm_climate_state));
    m.source_age_ms = UINT32_C(65535);
    patch_field(&expected, 42u, 2u, UINT64_C(65535));
    CHECK(net_wire_m_bcm_climate_state_encode(CANDIDATE, 0x5aa5u, &m, &actual) == NET_WIRE_OK);
    CHECK(actual.length == expected.length && memcmp(actual.bytes, expected.bytes, expected.length) == 0);
    CHECK(net_wire_m_bcm_climate_state_decode(CANDIDATE, &expected, &output, &sequence) == NET_WIRE_OK);
    CHECK(sequence == 0x5aa5u && equal_m_bcm_climate_state(&m, &output));
    return 0;
}

/* FINAL_98/docs/network/interface/03_COMMUNICATION/20_UART_M.md:434; independent Python struct/CRC literal. */
static const uint8_t golden_m_bcm_light_state[55] = {
    0xa5,0x5a,0x02,0x32,0x2d,0x00,0xa5,0x5a,0xa5,0x5a,0x22,0x11,0x01,0x02,0x03,0x04,
    0x05,0x06,0x07,0x08,0x44,0x33,0x22,0x11,0x99,0x88,0x77,0x66,0xd4,0xc3,0xb2,0xa1,
    0x40,0x30,0x20,0x10,0x02,0x2a,0x11,0x22,0xff,0x01,0x00,0x00,0x01,0x02,0x03,0x66,
    0x77,0x88,0x99,0xff,0xff,0xa8,0x29,
};
static NetWireMBcmLightStateRaw example_m_bcm_light_state(void)
{
    NetWireMBcmLightStateRaw m = {0};
    m.device_context_id = UINT32_C(287464101);
    m.session_id = UINT64_C(578437695752307201);
    m.domain_boot_id = UINT32_C(287454020);
    m.query_id = UINT32_C(1719109785);
    m.original_can_boot_id = UINT32_C(2712847316);
    m.original_can_tx_sequence = UINT32_C(270544960);
    m.interior_light_type_state = UINT32_C(2);
    m.light_level_percent = UINT32_C(42);
    m.color_r = UINT32_C(17);
    m.color_g = UINT32_C(34);
    m.color_b = UINT32_C(255);
    m.interior_light_apply_result = UINT32_C(1);
    m.light_physical_feedback_capability = UINT32_C(0);
    m.light_type_quality = UINT32_C(0);
    m.light_level_quality = UINT32_C(1);
    m.light_color_quality = UINT32_C(2);
    m.light_apply_quality = UINT32_C(3);
    m.update_sequence = UINT32_C(2575857510);
    m.source_age_ms = UINT32_C(65535);
    return m;
}
static int equal_m_bcm_light_state(const NetWireMBcmLightStateRaw *a, const NetWireMBcmLightStateRaw *b)
{
    return a->device_context_id == b->device_context_id &&
        a->session_id == b->session_id &&
        a->domain_boot_id == b->domain_boot_id &&
        a->query_id == b->query_id &&
        a->original_can_boot_id == b->original_can_boot_id &&
        a->original_can_tx_sequence == b->original_can_tx_sequence &&
        a->interior_light_type_state == b->interior_light_type_state &&
        a->light_level_percent == b->light_level_percent &&
        a->color_r == b->color_r &&
        a->color_g == b->color_g &&
        a->color_b == b->color_b &&
        a->interior_light_apply_result == b->interior_light_apply_result &&
        a->light_physical_feedback_capability == b->light_physical_feedback_capability &&
        a->light_type_quality == b->light_type_quality &&
        a->light_level_quality == b->light_level_quality &&
        a->light_color_quality == b->light_color_quality &&
        a->light_apply_quality == b->light_apply_quality &&
        a->update_sequence == b->update_sequence &&
        a->source_age_ms == b->source_age_ms;
}
static NetWireStatus encode_m_bcm_light_state(NetWireProfile p, uint16_t s, const void *m, NetWireUartFrame *f)
{
    return net_wire_m_bcm_light_state_encode(p, s, (const NetWireMBcmLightStateRaw *)m, f);
}
static NetWireStatus decode_m_bcm_light_state(NetWireProfile p, const NetWireUartFrame *f, void *m, uint16_t *s)
{
    return net_wire_m_bcm_light_state_decode(p, f, (NetWireMBcmLightStateRaw *)m, s);
}
static int test_m_bcm_light_state(void)
{
    NetWireMBcmLightStateRaw m = example_m_bcm_light_state(), output;
    NetWireUartFrame expected = literal_frame(golden_m_bcm_light_state, sizeof(golden_m_bcm_light_state)), actual;
    uint16_t sequence = 0u;
    CHECK(net_wire_m_bcm_light_state_encode(CANDIDATE, 0x5aa5u, &m, &actual) == NET_WIRE_OK);
    CHECK(actual.length == expected.length && memcmp(actual.bytes, expected.bytes, expected.length) == 0);
    CHECK(net_wire_m_bcm_light_state_decode(CANDIDATE, &expected, &output, &sequence) == NET_WIRE_OK);
    CHECK(sequence == 0x5aa5u && equal_m_bcm_light_state(&m, &output));
    CHECK(common_errors(encode_m_bcm_light_state, decode_m_bcm_light_state, &m, sizeof(m), golden_m_bcm_light_state, sizeof(golden_m_bcm_light_state)) == 0);
    expected = literal_frame(golden_m_bcm_light_state, sizeof(golden_m_bcm_light_state));
    expected.bytes[6] = 0u; expected.bytes[7] = 0u; repair_crc(&expected);
    CHECK(net_wire_m_bcm_light_state_encode(CANDIDATE, 0u, &m, &actual) == NET_WIRE_OK);
    CHECK(actual.length == expected.length && memcmp(actual.bytes, expected.bytes, expected.length) == 0);
    CHECK(net_wire_m_bcm_light_state_decode(CANDIDATE, &expected, &output, &sequence) == NET_WIRE_OK);
    CHECK(sequence == 0u && equal_m_bcm_light_state(&m, &output));
    expected = literal_frame(golden_m_bcm_light_state, sizeof(golden_m_bcm_light_state));
    expected.bytes[6] = 255u; expected.bytes[7] = 255u; repair_crc(&expected);
    CHECK(net_wire_m_bcm_light_state_encode(CANDIDATE, 65535u, &m, &actual) == NET_WIRE_OK);
    CHECK(actual.length == expected.length && memcmp(actual.bytes, expected.bytes, expected.length) == 0);
    CHECK(net_wire_m_bcm_light_state_decode(CANDIDATE, &expected, &output, &sequence) == NET_WIRE_OK);
    CHECK(sequence == 65535u && equal_m_bcm_light_state(&m, &output));
    m = example_m_bcm_light_state();
    expected = literal_frame(golden_m_bcm_light_state, sizeof(golden_m_bcm_light_state));
    m.device_context_id = UINT32_C(0);
    patch_field(&expected, 0u, 4u, UINT64_C(0));
    CHECK(rejected(encode_m_bcm_light_state, decode_m_bcm_light_state, &m, sizeof(m), &expected, NET_WIRE_MALFORMED) == 0);
    m = example_m_bcm_light_state();
    expected = literal_frame(golden_m_bcm_light_state, sizeof(golden_m_bcm_light_state));
    m.domain_boot_id = UINT32_C(0);
    patch_field(&expected, 12u, 4u, UINT64_C(0));
    CHECK(rejected(encode_m_bcm_light_state, decode_m_bcm_light_state, &m, sizeof(m), &expected, NET_WIRE_MALFORMED) == 0);
    m = example_m_bcm_light_state();
    expected = literal_frame(golden_m_bcm_light_state, sizeof(golden_m_bcm_light_state));
    m.original_can_boot_id = UINT32_C(0);
    patch_field(&expected, 20u, 4u, UINT64_C(0));
    CHECK(rejected(encode_m_bcm_light_state, decode_m_bcm_light_state, &m, sizeof(m), &expected, NET_WIRE_MALFORMED) == 0);
    m = example_m_bcm_light_state();
    expected = literal_frame(golden_m_bcm_light_state, sizeof(golden_m_bcm_light_state));
    m.interior_light_type_state = UINT32_C(4);
    patch_field(&expected, 28u, 1u, UINT64_C(4));
    CHECK(rejected(encode_m_bcm_light_state, decode_m_bcm_light_state, &m, sizeof(m), &expected, NET_WIRE_MALFORMED) == 0);
    m = example_m_bcm_light_state();
    expected = literal_frame(golden_m_bcm_light_state, sizeof(golden_m_bcm_light_state));
    m.light_level_percent = UINT32_C(101);
    patch_field(&expected, 29u, 1u, UINT64_C(101));
    CHECK(rejected(encode_m_bcm_light_state, decode_m_bcm_light_state, &m, sizeof(m), &expected, NET_WIRE_MALFORMED) == 0);
    m = example_m_bcm_light_state();
    expected = literal_frame(golden_m_bcm_light_state, sizeof(golden_m_bcm_light_state));
    m.interior_light_apply_result = UINT32_C(2);
    patch_field(&expected, 33u, 1u, UINT64_C(2));
    CHECK(rejected(encode_m_bcm_light_state, decode_m_bcm_light_state, &m, sizeof(m), &expected, NET_WIRE_MALFORMED) == 0);
    m = example_m_bcm_light_state();
    expected = literal_frame(golden_m_bcm_light_state, sizeof(golden_m_bcm_light_state));
    m.light_physical_feedback_capability = UINT32_C(2);
    patch_field(&expected, 34u, 1u, UINT64_C(2));
    CHECK(rejected(encode_m_bcm_light_state, decode_m_bcm_light_state, &m, sizeof(m), &expected, NET_WIRE_MALFORMED) == 0);
    m = example_m_bcm_light_state();
    expected = literal_frame(golden_m_bcm_light_state, sizeof(golden_m_bcm_light_state));
    m.light_type_quality = UINT32_C(4);
    patch_field(&expected, 35u, 1u, UINT64_C(4));
    CHECK(rejected(encode_m_bcm_light_state, decode_m_bcm_light_state, &m, sizeof(m), &expected, NET_WIRE_MALFORMED) == 0);
    m = example_m_bcm_light_state();
    expected = literal_frame(golden_m_bcm_light_state, sizeof(golden_m_bcm_light_state));
    m.light_level_quality = UINT32_C(4);
    patch_field(&expected, 36u, 1u, UINT64_C(4));
    CHECK(rejected(encode_m_bcm_light_state, decode_m_bcm_light_state, &m, sizeof(m), &expected, NET_WIRE_MALFORMED) == 0);
    m = example_m_bcm_light_state();
    expected = literal_frame(golden_m_bcm_light_state, sizeof(golden_m_bcm_light_state));
    m.light_color_quality = UINT32_C(4);
    patch_field(&expected, 37u, 1u, UINT64_C(4));
    CHECK(rejected(encode_m_bcm_light_state, decode_m_bcm_light_state, &m, sizeof(m), &expected, NET_WIRE_MALFORMED) == 0);
    m = example_m_bcm_light_state();
    expected = literal_frame(golden_m_bcm_light_state, sizeof(golden_m_bcm_light_state));
    m.light_apply_quality = UINT32_C(4);
    patch_field(&expected, 38u, 1u, UINT64_C(4));
    CHECK(rejected(encode_m_bcm_light_state, decode_m_bcm_light_state, &m, sizeof(m), &expected, NET_WIRE_MALFORMED) == 0);
    m = example_m_bcm_light_state();
    expected = literal_frame(golden_m_bcm_light_state, sizeof(golden_m_bcm_light_state));
    m.color_r = UINT32_C(255);
    patch_field(&expected, 30u, 1u, UINT64_C(255));
    m.color_g = UINT32_C(0);
    patch_field(&expected, 31u, 1u, UINT64_C(0));
    m.color_b = UINT32_C(127);
    patch_field(&expected, 32u, 1u, UINT64_C(127));
    m.light_level_percent = UINT32_C(0);
    patch_field(&expected, 29u, 1u, UINT64_C(0));
    CHECK(net_wire_m_bcm_light_state_encode(CANDIDATE, 0x5aa5u, &m, &actual) == NET_WIRE_OK);
    CHECK(actual.length == expected.length && memcmp(actual.bytes, expected.bytes, expected.length) == 0);
    CHECK(net_wire_m_bcm_light_state_decode(CANDIDATE, &expected, &output, &sequence) == NET_WIRE_OK);
    CHECK(sequence == 0x5aa5u && equal_m_bcm_light_state(&m, &output));
    m = example_m_bcm_light_state();
    expected = literal_frame(golden_m_bcm_light_state, sizeof(golden_m_bcm_light_state));
    m.light_level_percent = UINT32_C(100);
    patch_field(&expected, 29u, 1u, UINT64_C(100));
    m.interior_light_apply_result = UINT32_C(255);
    patch_field(&expected, 33u, 1u, UINT64_C(255));
    m.light_apply_quality = UINT32_C(3);
    patch_field(&expected, 38u, 1u, UINT64_C(3));
    m.light_physical_feedback_capability = UINT32_C(1);
    patch_field(&expected, 34u, 1u, UINT64_C(1));
    CHECK(net_wire_m_bcm_light_state_encode(CANDIDATE, 0x5aa5u, &m, &actual) == NET_WIRE_OK);
    CHECK(actual.length == expected.length && memcmp(actual.bytes, expected.bytes, expected.length) == 0);
    CHECK(net_wire_m_bcm_light_state_decode(CANDIDATE, &expected, &output, &sequence) == NET_WIRE_OK);
    CHECK(sequence == 0x5aa5u && equal_m_bcm_light_state(&m, &output));
    m = example_m_bcm_light_state();
    expected = literal_frame(golden_m_bcm_light_state, sizeof(golden_m_bcm_light_state));
    m.device_context_id = UINT32_C(1);
    patch_field(&expected, 0u, 4u, UINT64_C(1));
    CHECK(net_wire_m_bcm_light_state_encode(CANDIDATE, 0x5aa5u, &m, &actual) == NET_WIRE_OK);
    CHECK(actual.length == expected.length && memcmp(actual.bytes, expected.bytes, expected.length) == 0);
    CHECK(net_wire_m_bcm_light_state_decode(CANDIDATE, &expected, &output, &sequence) == NET_WIRE_OK);
    CHECK(sequence == 0x5aa5u && equal_m_bcm_light_state(&m, &output));
    m = example_m_bcm_light_state();
    expected = literal_frame(golden_m_bcm_light_state, sizeof(golden_m_bcm_light_state));
    m.device_context_id = UINT32_C(4294967295);
    patch_field(&expected, 0u, 4u, UINT64_C(4294967295));
    CHECK(net_wire_m_bcm_light_state_encode(CANDIDATE, 0x5aa5u, &m, &actual) == NET_WIRE_OK);
    CHECK(actual.length == expected.length && memcmp(actual.bytes, expected.bytes, expected.length) == 0);
    CHECK(net_wire_m_bcm_light_state_decode(CANDIDATE, &expected, &output, &sequence) == NET_WIRE_OK);
    CHECK(sequence == 0x5aa5u && equal_m_bcm_light_state(&m, &output));
    m = example_m_bcm_light_state();
    expected = literal_frame(golden_m_bcm_light_state, sizeof(golden_m_bcm_light_state));
    m.domain_boot_id = UINT32_C(1);
    patch_field(&expected, 12u, 4u, UINT64_C(1));
    CHECK(net_wire_m_bcm_light_state_encode(CANDIDATE, 0x5aa5u, &m, &actual) == NET_WIRE_OK);
    CHECK(actual.length == expected.length && memcmp(actual.bytes, expected.bytes, expected.length) == 0);
    CHECK(net_wire_m_bcm_light_state_decode(CANDIDATE, &expected, &output, &sequence) == NET_WIRE_OK);
    CHECK(sequence == 0x5aa5u && equal_m_bcm_light_state(&m, &output));
    m = example_m_bcm_light_state();
    expected = literal_frame(golden_m_bcm_light_state, sizeof(golden_m_bcm_light_state));
    m.domain_boot_id = UINT32_C(4294967295);
    patch_field(&expected, 12u, 4u, UINT64_C(4294967295));
    CHECK(net_wire_m_bcm_light_state_encode(CANDIDATE, 0x5aa5u, &m, &actual) == NET_WIRE_OK);
    CHECK(actual.length == expected.length && memcmp(actual.bytes, expected.bytes, expected.length) == 0);
    CHECK(net_wire_m_bcm_light_state_decode(CANDIDATE, &expected, &output, &sequence) == NET_WIRE_OK);
    CHECK(sequence == 0x5aa5u && equal_m_bcm_light_state(&m, &output));
    m = example_m_bcm_light_state();
    expected = literal_frame(golden_m_bcm_light_state, sizeof(golden_m_bcm_light_state));
    m.original_can_boot_id = UINT32_C(1);
    patch_field(&expected, 20u, 4u, UINT64_C(1));
    CHECK(net_wire_m_bcm_light_state_encode(CANDIDATE, 0x5aa5u, &m, &actual) == NET_WIRE_OK);
    CHECK(actual.length == expected.length && memcmp(actual.bytes, expected.bytes, expected.length) == 0);
    CHECK(net_wire_m_bcm_light_state_decode(CANDIDATE, &expected, &output, &sequence) == NET_WIRE_OK);
    CHECK(sequence == 0x5aa5u && equal_m_bcm_light_state(&m, &output));
    m = example_m_bcm_light_state();
    expected = literal_frame(golden_m_bcm_light_state, sizeof(golden_m_bcm_light_state));
    m.original_can_boot_id = UINT32_C(4294967295);
    patch_field(&expected, 20u, 4u, UINT64_C(4294967295));
    CHECK(net_wire_m_bcm_light_state_encode(CANDIDATE, 0x5aa5u, &m, &actual) == NET_WIRE_OK);
    CHECK(actual.length == expected.length && memcmp(actual.bytes, expected.bytes, expected.length) == 0);
    CHECK(net_wire_m_bcm_light_state_decode(CANDIDATE, &expected, &output, &sequence) == NET_WIRE_OK);
    CHECK(sequence == 0x5aa5u && equal_m_bcm_light_state(&m, &output));
    m = example_m_bcm_light_state();
    expected = literal_frame(golden_m_bcm_light_state, sizeof(golden_m_bcm_light_state));
    m.session_id = UINT64_C(0);
    patch_field(&expected, 4u, 8u, UINT64_C(0));
    CHECK(net_wire_m_bcm_light_state_encode(CANDIDATE, 0x5aa5u, &m, &actual) == NET_WIRE_OK);
    CHECK(actual.length == expected.length && memcmp(actual.bytes, expected.bytes, expected.length) == 0);
    CHECK(net_wire_m_bcm_light_state_decode(CANDIDATE, &expected, &output, &sequence) == NET_WIRE_OK);
    CHECK(sequence == 0x5aa5u && equal_m_bcm_light_state(&m, &output));
    m = example_m_bcm_light_state();
    expected = literal_frame(golden_m_bcm_light_state, sizeof(golden_m_bcm_light_state));
    m.session_id = UINT64_C(18446744073709551615);
    patch_field(&expected, 4u, 8u, UINT64_C(18446744073709551615));
    CHECK(net_wire_m_bcm_light_state_encode(CANDIDATE, 0x5aa5u, &m, &actual) == NET_WIRE_OK);
    CHECK(actual.length == expected.length && memcmp(actual.bytes, expected.bytes, expected.length) == 0);
    CHECK(net_wire_m_bcm_light_state_decode(CANDIDATE, &expected, &output, &sequence) == NET_WIRE_OK);
    CHECK(sequence == 0x5aa5u && equal_m_bcm_light_state(&m, &output));
    m = example_m_bcm_light_state();
    expected = literal_frame(golden_m_bcm_light_state, sizeof(golden_m_bcm_light_state));
    m.query_id = UINT32_C(0);
    patch_field(&expected, 16u, 4u, UINT64_C(0));
    CHECK(net_wire_m_bcm_light_state_encode(CANDIDATE, 0x5aa5u, &m, &actual) == NET_WIRE_OK);
    CHECK(actual.length == expected.length && memcmp(actual.bytes, expected.bytes, expected.length) == 0);
    CHECK(net_wire_m_bcm_light_state_decode(CANDIDATE, &expected, &output, &sequence) == NET_WIRE_OK);
    CHECK(sequence == 0x5aa5u && equal_m_bcm_light_state(&m, &output));
    m = example_m_bcm_light_state();
    expected = literal_frame(golden_m_bcm_light_state, sizeof(golden_m_bcm_light_state));
    m.query_id = UINT32_C(4294967295);
    patch_field(&expected, 16u, 4u, UINT64_C(4294967295));
    CHECK(net_wire_m_bcm_light_state_encode(CANDIDATE, 0x5aa5u, &m, &actual) == NET_WIRE_OK);
    CHECK(actual.length == expected.length && memcmp(actual.bytes, expected.bytes, expected.length) == 0);
    CHECK(net_wire_m_bcm_light_state_decode(CANDIDATE, &expected, &output, &sequence) == NET_WIRE_OK);
    CHECK(sequence == 0x5aa5u && equal_m_bcm_light_state(&m, &output));
    m = example_m_bcm_light_state();
    expected = literal_frame(golden_m_bcm_light_state, sizeof(golden_m_bcm_light_state));
    m.original_can_tx_sequence = UINT32_C(0);
    patch_field(&expected, 24u, 4u, UINT64_C(0));
    CHECK(net_wire_m_bcm_light_state_encode(CANDIDATE, 0x5aa5u, &m, &actual) == NET_WIRE_OK);
    CHECK(actual.length == expected.length && memcmp(actual.bytes, expected.bytes, expected.length) == 0);
    CHECK(net_wire_m_bcm_light_state_decode(CANDIDATE, &expected, &output, &sequence) == NET_WIRE_OK);
    CHECK(sequence == 0x5aa5u && equal_m_bcm_light_state(&m, &output));
    m = example_m_bcm_light_state();
    expected = literal_frame(golden_m_bcm_light_state, sizeof(golden_m_bcm_light_state));
    m.original_can_tx_sequence = UINT32_C(4294967295);
    patch_field(&expected, 24u, 4u, UINT64_C(4294967295));
    CHECK(net_wire_m_bcm_light_state_encode(CANDIDATE, 0x5aa5u, &m, &actual) == NET_WIRE_OK);
    CHECK(actual.length == expected.length && memcmp(actual.bytes, expected.bytes, expected.length) == 0);
    CHECK(net_wire_m_bcm_light_state_decode(CANDIDATE, &expected, &output, &sequence) == NET_WIRE_OK);
    CHECK(sequence == 0x5aa5u && equal_m_bcm_light_state(&m, &output));
    m = example_m_bcm_light_state();
    expected = literal_frame(golden_m_bcm_light_state, sizeof(golden_m_bcm_light_state));
    m.update_sequence = UINT32_C(0);
    patch_field(&expected, 39u, 4u, UINT64_C(0));
    CHECK(net_wire_m_bcm_light_state_encode(CANDIDATE, 0x5aa5u, &m, &actual) == NET_WIRE_OK);
    CHECK(actual.length == expected.length && memcmp(actual.bytes, expected.bytes, expected.length) == 0);
    CHECK(net_wire_m_bcm_light_state_decode(CANDIDATE, &expected, &output, &sequence) == NET_WIRE_OK);
    CHECK(sequence == 0x5aa5u && equal_m_bcm_light_state(&m, &output));
    m = example_m_bcm_light_state();
    expected = literal_frame(golden_m_bcm_light_state, sizeof(golden_m_bcm_light_state));
    m.update_sequence = UINT32_C(4294967295);
    patch_field(&expected, 39u, 4u, UINT64_C(4294967295));
    CHECK(net_wire_m_bcm_light_state_encode(CANDIDATE, 0x5aa5u, &m, &actual) == NET_WIRE_OK);
    CHECK(actual.length == expected.length && memcmp(actual.bytes, expected.bytes, expected.length) == 0);
    CHECK(net_wire_m_bcm_light_state_decode(CANDIDATE, &expected, &output, &sequence) == NET_WIRE_OK);
    CHECK(sequence == 0x5aa5u && equal_m_bcm_light_state(&m, &output));
    m = example_m_bcm_light_state();
    expected = literal_frame(golden_m_bcm_light_state, sizeof(golden_m_bcm_light_state));
    m.source_age_ms = UINT32_C(0);
    patch_field(&expected, 43u, 2u, UINT64_C(0));
    CHECK(net_wire_m_bcm_light_state_encode(CANDIDATE, 0x5aa5u, &m, &actual) == NET_WIRE_OK);
    CHECK(actual.length == expected.length && memcmp(actual.bytes, expected.bytes, expected.length) == 0);
    CHECK(net_wire_m_bcm_light_state_decode(CANDIDATE, &expected, &output, &sequence) == NET_WIRE_OK);
    CHECK(sequence == 0x5aa5u && equal_m_bcm_light_state(&m, &output));
    m = example_m_bcm_light_state();
    expected = literal_frame(golden_m_bcm_light_state, sizeof(golden_m_bcm_light_state));
    m.source_age_ms = UINT32_C(65535);
    patch_field(&expected, 43u, 2u, UINT64_C(65535));
    CHECK(net_wire_m_bcm_light_state_encode(CANDIDATE, 0x5aa5u, &m, &actual) == NET_WIRE_OK);
    CHECK(actual.length == expected.length && memcmp(actual.bytes, expected.bytes, expected.length) == 0);
    CHECK(net_wire_m_bcm_light_state_decode(CANDIDATE, &expected, &output, &sequence) == NET_WIRE_OK);
    CHECK(sequence == 0x5aa5u && equal_m_bcm_light_state(&m, &output));
    return 0;
}

/* FINAL_98/docs/network/interface/03_COMMUNICATION/20_UART_M.md:459; independent Python struct/CRC literal. */
static const uint8_t golden_m_bcm_status[52] = {
    0xa5,0x5a,0x02,0x33,0x2a,0x00,0xa5,0x5a,0xa5,0x5a,0x22,0x11,0x01,0x02,0x03,0x04,
    0x05,0x06,0x07,0x08,0x44,0x33,0x22,0x11,0x99,0x88,0x77,0x66,0xd4,0xc3,0xb2,0xa1,
    0x40,0x30,0x20,0x10,0x02,0x80,0x00,0x03,0x08,0x00,0x07,0x01,0x34,0x12,0xcd,0xab,
    0xff,0xff,0x8c,0x84,
};
static NetWireMBcmStatusRaw example_m_bcm_status(void)
{
    NetWireMBcmStatusRaw m = {0};
    m.device_context_id = UINT32_C(287464101);
    m.session_id = UINT64_C(578437695752307201);
    m.domain_boot_id = UINT32_C(287454020);
    m.query_id = UINT32_C(1719109785);
    m.original_can_boot_id = UINT32_C(2712847316);
    m.original_can_tx_sequence = UINT32_C(270544960);
    m.bcm_ecu_state = UINT32_C(2);
    m.fault_mask = UINT32_C(128);
    m.active_fault_category = UINT32_C(3);
    m.active_fault_code = UINT32_C(8);
    m.affected_function = UINT32_C(7);
    m.recovering = UINT32_C(1);
    m.update_sequence = UINT32_C(2882343476);
    m.source_age_ms = UINT32_C(65535);
    return m;
}
static int equal_m_bcm_status(const NetWireMBcmStatusRaw *a, const NetWireMBcmStatusRaw *b)
{
    return a->device_context_id == b->device_context_id &&
        a->session_id == b->session_id &&
        a->domain_boot_id == b->domain_boot_id &&
        a->query_id == b->query_id &&
        a->original_can_boot_id == b->original_can_boot_id &&
        a->original_can_tx_sequence == b->original_can_tx_sequence &&
        a->bcm_ecu_state == b->bcm_ecu_state &&
        a->fault_mask == b->fault_mask &&
        a->active_fault_category == b->active_fault_category &&
        a->active_fault_code == b->active_fault_code &&
        a->affected_function == b->affected_function &&
        a->recovering == b->recovering &&
        a->update_sequence == b->update_sequence &&
        a->source_age_ms == b->source_age_ms;
}
static NetWireStatus encode_m_bcm_status(NetWireProfile p, uint16_t s, const void *m, NetWireUartFrame *f)
{
    return net_wire_m_bcm_status_encode(p, s, (const NetWireMBcmStatusRaw *)m, f);
}
static NetWireStatus decode_m_bcm_status(NetWireProfile p, const NetWireUartFrame *f, void *m, uint16_t *s)
{
    return net_wire_m_bcm_status_decode(p, f, (NetWireMBcmStatusRaw *)m, s);
}
static int test_m_bcm_status(void)
{
    NetWireMBcmStatusRaw m = example_m_bcm_status(), output;
    NetWireUartFrame expected = literal_frame(golden_m_bcm_status, sizeof(golden_m_bcm_status)), actual;
    uint16_t sequence = 0u;
    CHECK(net_wire_m_bcm_status_encode(CANDIDATE, 0x5aa5u, &m, &actual) == NET_WIRE_OK);
    CHECK(actual.length == expected.length && memcmp(actual.bytes, expected.bytes, expected.length) == 0);
    CHECK(net_wire_m_bcm_status_decode(CANDIDATE, &expected, &output, &sequence) == NET_WIRE_OK);
    CHECK(sequence == 0x5aa5u && equal_m_bcm_status(&m, &output));
    CHECK(common_errors(encode_m_bcm_status, decode_m_bcm_status, &m, sizeof(m), golden_m_bcm_status, sizeof(golden_m_bcm_status)) == 0);
    expected = literal_frame(golden_m_bcm_status, sizeof(golden_m_bcm_status));
    expected.bytes[6] = 0u; expected.bytes[7] = 0u; repair_crc(&expected);
    CHECK(net_wire_m_bcm_status_encode(CANDIDATE, 0u, &m, &actual) == NET_WIRE_OK);
    CHECK(actual.length == expected.length && memcmp(actual.bytes, expected.bytes, expected.length) == 0);
    CHECK(net_wire_m_bcm_status_decode(CANDIDATE, &expected, &output, &sequence) == NET_WIRE_OK);
    CHECK(sequence == 0u && equal_m_bcm_status(&m, &output));
    expected = literal_frame(golden_m_bcm_status, sizeof(golden_m_bcm_status));
    expected.bytes[6] = 255u; expected.bytes[7] = 255u; repair_crc(&expected);
    CHECK(net_wire_m_bcm_status_encode(CANDIDATE, 65535u, &m, &actual) == NET_WIRE_OK);
    CHECK(actual.length == expected.length && memcmp(actual.bytes, expected.bytes, expected.length) == 0);
    CHECK(net_wire_m_bcm_status_decode(CANDIDATE, &expected, &output, &sequence) == NET_WIRE_OK);
    CHECK(sequence == 65535u && equal_m_bcm_status(&m, &output));
    m = example_m_bcm_status();
    expected = literal_frame(golden_m_bcm_status, sizeof(golden_m_bcm_status));
    m.device_context_id = UINT32_C(0);
    patch_field(&expected, 0u, 4u, UINT64_C(0));
    CHECK(rejected(encode_m_bcm_status, decode_m_bcm_status, &m, sizeof(m), &expected, NET_WIRE_MALFORMED) == 0);
    m = example_m_bcm_status();
    expected = literal_frame(golden_m_bcm_status, sizeof(golden_m_bcm_status));
    m.domain_boot_id = UINT32_C(0);
    patch_field(&expected, 12u, 4u, UINT64_C(0));
    CHECK(rejected(encode_m_bcm_status, decode_m_bcm_status, &m, sizeof(m), &expected, NET_WIRE_MALFORMED) == 0);
    m = example_m_bcm_status();
    expected = literal_frame(golden_m_bcm_status, sizeof(golden_m_bcm_status));
    m.original_can_boot_id = UINT32_C(0);
    patch_field(&expected, 20u, 4u, UINT64_C(0));
    CHECK(rejected(encode_m_bcm_status, decode_m_bcm_status, &m, sizeof(m), &expected, NET_WIRE_MALFORMED) == 0);
    m = example_m_bcm_status();
    expected = literal_frame(golden_m_bcm_status, sizeof(golden_m_bcm_status));
    m.bcm_ecu_state = UINT32_C(4);
    patch_field(&expected, 28u, 1u, UINT64_C(4));
    CHECK(rejected(encode_m_bcm_status, decode_m_bcm_status, &m, sizeof(m), &expected, NET_WIRE_MALFORMED) == 0);
    m = example_m_bcm_status();
    expected = literal_frame(golden_m_bcm_status, sizeof(golden_m_bcm_status));
    m.fault_mask = UINT32_C(256);
    patch_field(&expected, 29u, 2u, UINT64_C(256));
    CHECK(rejected(encode_m_bcm_status, decode_m_bcm_status, &m, sizeof(m), &expected, NET_WIRE_MALFORMED) == 0);
    m = example_m_bcm_status();
    expected = literal_frame(golden_m_bcm_status, sizeof(golden_m_bcm_status));
    m.active_fault_category = UINT32_C(4);
    patch_field(&expected, 31u, 1u, UINT64_C(4));
    CHECK(rejected(encode_m_bcm_status, decode_m_bcm_status, &m, sizeof(m), &expected, NET_WIRE_MALFORMED) == 0);
    m = example_m_bcm_status();
    expected = literal_frame(golden_m_bcm_status, sizeof(golden_m_bcm_status));
    m.active_fault_code = UINT32_C(9);
    patch_field(&expected, 32u, 2u, UINT64_C(9));
    CHECK(rejected(encode_m_bcm_status, decode_m_bcm_status, &m, sizeof(m), &expected, NET_WIRE_MALFORMED) == 0);
    m = example_m_bcm_status();
    expected = literal_frame(golden_m_bcm_status, sizeof(golden_m_bcm_status));
    m.affected_function = UINT32_C(8);
    patch_field(&expected, 34u, 1u, UINT64_C(8));
    CHECK(rejected(encode_m_bcm_status, decode_m_bcm_status, &m, sizeof(m), &expected, NET_WIRE_MALFORMED) == 0);
    m = example_m_bcm_status();
    expected = literal_frame(golden_m_bcm_status, sizeof(golden_m_bcm_status));
    m.recovering = UINT32_C(2);
    patch_field(&expected, 35u, 1u, UINT64_C(2));
    CHECK(rejected(encode_m_bcm_status, decode_m_bcm_status, &m, sizeof(m), &expected, NET_WIRE_MALFORMED) == 0);
    m = example_m_bcm_status();
    expected = literal_frame(golden_m_bcm_status, sizeof(golden_m_bcm_status));
    m.fault_mask = UINT32_C(255);
    patch_field(&expected, 29u, 2u, UINT64_C(255));
    m.active_fault_code = UINT32_C(8);
    patch_field(&expected, 32u, 2u, UINT64_C(8));
    CHECK(net_wire_m_bcm_status_encode(CANDIDATE, 0x5aa5u, &m, &actual) == NET_WIRE_OK);
    CHECK(actual.length == expected.length && memcmp(actual.bytes, expected.bytes, expected.length) == 0);
    CHECK(net_wire_m_bcm_status_decode(CANDIDATE, &expected, &output, &sequence) == NET_WIRE_OK);
    CHECK(sequence == 0x5aa5u && equal_m_bcm_status(&m, &output));
    m = example_m_bcm_status();
    expected = literal_frame(golden_m_bcm_status, sizeof(golden_m_bcm_status));
    m.fault_mask = UINT32_C(0);
    patch_field(&expected, 29u, 2u, UINT64_C(0));
    m.active_fault_code = UINT32_C(0);
    patch_field(&expected, 32u, 2u, UINT64_C(0));
    m.active_fault_category = UINT32_C(0);
    patch_field(&expected, 31u, 1u, UINT64_C(0));
    m.recovering = UINT32_C(0);
    patch_field(&expected, 35u, 1u, UINT64_C(0));
    m.affected_function = UINT32_C(0);
    patch_field(&expected, 34u, 1u, UINT64_C(0));
    CHECK(net_wire_m_bcm_status_encode(CANDIDATE, 0x5aa5u, &m, &actual) == NET_WIRE_OK);
    CHECK(actual.length == expected.length && memcmp(actual.bytes, expected.bytes, expected.length) == 0);
    CHECK(net_wire_m_bcm_status_decode(CANDIDATE, &expected, &output, &sequence) == NET_WIRE_OK);
    CHECK(sequence == 0x5aa5u && equal_m_bcm_status(&m, &output));
    m = example_m_bcm_status();
    expected = literal_frame(golden_m_bcm_status, sizeof(golden_m_bcm_status));
    m.device_context_id = UINT32_C(1);
    patch_field(&expected, 0u, 4u, UINT64_C(1));
    CHECK(net_wire_m_bcm_status_encode(CANDIDATE, 0x5aa5u, &m, &actual) == NET_WIRE_OK);
    CHECK(actual.length == expected.length && memcmp(actual.bytes, expected.bytes, expected.length) == 0);
    CHECK(net_wire_m_bcm_status_decode(CANDIDATE, &expected, &output, &sequence) == NET_WIRE_OK);
    CHECK(sequence == 0x5aa5u && equal_m_bcm_status(&m, &output));
    m = example_m_bcm_status();
    expected = literal_frame(golden_m_bcm_status, sizeof(golden_m_bcm_status));
    m.device_context_id = UINT32_C(4294967295);
    patch_field(&expected, 0u, 4u, UINT64_C(4294967295));
    CHECK(net_wire_m_bcm_status_encode(CANDIDATE, 0x5aa5u, &m, &actual) == NET_WIRE_OK);
    CHECK(actual.length == expected.length && memcmp(actual.bytes, expected.bytes, expected.length) == 0);
    CHECK(net_wire_m_bcm_status_decode(CANDIDATE, &expected, &output, &sequence) == NET_WIRE_OK);
    CHECK(sequence == 0x5aa5u && equal_m_bcm_status(&m, &output));
    m = example_m_bcm_status();
    expected = literal_frame(golden_m_bcm_status, sizeof(golden_m_bcm_status));
    m.domain_boot_id = UINT32_C(1);
    patch_field(&expected, 12u, 4u, UINT64_C(1));
    CHECK(net_wire_m_bcm_status_encode(CANDIDATE, 0x5aa5u, &m, &actual) == NET_WIRE_OK);
    CHECK(actual.length == expected.length && memcmp(actual.bytes, expected.bytes, expected.length) == 0);
    CHECK(net_wire_m_bcm_status_decode(CANDIDATE, &expected, &output, &sequence) == NET_WIRE_OK);
    CHECK(sequence == 0x5aa5u && equal_m_bcm_status(&m, &output));
    m = example_m_bcm_status();
    expected = literal_frame(golden_m_bcm_status, sizeof(golden_m_bcm_status));
    m.domain_boot_id = UINT32_C(4294967295);
    patch_field(&expected, 12u, 4u, UINT64_C(4294967295));
    CHECK(net_wire_m_bcm_status_encode(CANDIDATE, 0x5aa5u, &m, &actual) == NET_WIRE_OK);
    CHECK(actual.length == expected.length && memcmp(actual.bytes, expected.bytes, expected.length) == 0);
    CHECK(net_wire_m_bcm_status_decode(CANDIDATE, &expected, &output, &sequence) == NET_WIRE_OK);
    CHECK(sequence == 0x5aa5u && equal_m_bcm_status(&m, &output));
    m = example_m_bcm_status();
    expected = literal_frame(golden_m_bcm_status, sizeof(golden_m_bcm_status));
    m.original_can_boot_id = UINT32_C(1);
    patch_field(&expected, 20u, 4u, UINT64_C(1));
    CHECK(net_wire_m_bcm_status_encode(CANDIDATE, 0x5aa5u, &m, &actual) == NET_WIRE_OK);
    CHECK(actual.length == expected.length && memcmp(actual.bytes, expected.bytes, expected.length) == 0);
    CHECK(net_wire_m_bcm_status_decode(CANDIDATE, &expected, &output, &sequence) == NET_WIRE_OK);
    CHECK(sequence == 0x5aa5u && equal_m_bcm_status(&m, &output));
    m = example_m_bcm_status();
    expected = literal_frame(golden_m_bcm_status, sizeof(golden_m_bcm_status));
    m.original_can_boot_id = UINT32_C(4294967295);
    patch_field(&expected, 20u, 4u, UINT64_C(4294967295));
    CHECK(net_wire_m_bcm_status_encode(CANDIDATE, 0x5aa5u, &m, &actual) == NET_WIRE_OK);
    CHECK(actual.length == expected.length && memcmp(actual.bytes, expected.bytes, expected.length) == 0);
    CHECK(net_wire_m_bcm_status_decode(CANDIDATE, &expected, &output, &sequence) == NET_WIRE_OK);
    CHECK(sequence == 0x5aa5u && equal_m_bcm_status(&m, &output));
    m = example_m_bcm_status();
    expected = literal_frame(golden_m_bcm_status, sizeof(golden_m_bcm_status));
    m.session_id = UINT64_C(0);
    patch_field(&expected, 4u, 8u, UINT64_C(0));
    CHECK(net_wire_m_bcm_status_encode(CANDIDATE, 0x5aa5u, &m, &actual) == NET_WIRE_OK);
    CHECK(actual.length == expected.length && memcmp(actual.bytes, expected.bytes, expected.length) == 0);
    CHECK(net_wire_m_bcm_status_decode(CANDIDATE, &expected, &output, &sequence) == NET_WIRE_OK);
    CHECK(sequence == 0x5aa5u && equal_m_bcm_status(&m, &output));
    m = example_m_bcm_status();
    expected = literal_frame(golden_m_bcm_status, sizeof(golden_m_bcm_status));
    m.session_id = UINT64_C(18446744073709551615);
    patch_field(&expected, 4u, 8u, UINT64_C(18446744073709551615));
    CHECK(net_wire_m_bcm_status_encode(CANDIDATE, 0x5aa5u, &m, &actual) == NET_WIRE_OK);
    CHECK(actual.length == expected.length && memcmp(actual.bytes, expected.bytes, expected.length) == 0);
    CHECK(net_wire_m_bcm_status_decode(CANDIDATE, &expected, &output, &sequence) == NET_WIRE_OK);
    CHECK(sequence == 0x5aa5u && equal_m_bcm_status(&m, &output));
    m = example_m_bcm_status();
    expected = literal_frame(golden_m_bcm_status, sizeof(golden_m_bcm_status));
    m.query_id = UINT32_C(0);
    patch_field(&expected, 16u, 4u, UINT64_C(0));
    CHECK(net_wire_m_bcm_status_encode(CANDIDATE, 0x5aa5u, &m, &actual) == NET_WIRE_OK);
    CHECK(actual.length == expected.length && memcmp(actual.bytes, expected.bytes, expected.length) == 0);
    CHECK(net_wire_m_bcm_status_decode(CANDIDATE, &expected, &output, &sequence) == NET_WIRE_OK);
    CHECK(sequence == 0x5aa5u && equal_m_bcm_status(&m, &output));
    m = example_m_bcm_status();
    expected = literal_frame(golden_m_bcm_status, sizeof(golden_m_bcm_status));
    m.query_id = UINT32_C(4294967295);
    patch_field(&expected, 16u, 4u, UINT64_C(4294967295));
    CHECK(net_wire_m_bcm_status_encode(CANDIDATE, 0x5aa5u, &m, &actual) == NET_WIRE_OK);
    CHECK(actual.length == expected.length && memcmp(actual.bytes, expected.bytes, expected.length) == 0);
    CHECK(net_wire_m_bcm_status_decode(CANDIDATE, &expected, &output, &sequence) == NET_WIRE_OK);
    CHECK(sequence == 0x5aa5u && equal_m_bcm_status(&m, &output));
    m = example_m_bcm_status();
    expected = literal_frame(golden_m_bcm_status, sizeof(golden_m_bcm_status));
    m.original_can_tx_sequence = UINT32_C(0);
    patch_field(&expected, 24u, 4u, UINT64_C(0));
    CHECK(net_wire_m_bcm_status_encode(CANDIDATE, 0x5aa5u, &m, &actual) == NET_WIRE_OK);
    CHECK(actual.length == expected.length && memcmp(actual.bytes, expected.bytes, expected.length) == 0);
    CHECK(net_wire_m_bcm_status_decode(CANDIDATE, &expected, &output, &sequence) == NET_WIRE_OK);
    CHECK(sequence == 0x5aa5u && equal_m_bcm_status(&m, &output));
    m = example_m_bcm_status();
    expected = literal_frame(golden_m_bcm_status, sizeof(golden_m_bcm_status));
    m.original_can_tx_sequence = UINT32_C(4294967295);
    patch_field(&expected, 24u, 4u, UINT64_C(4294967295));
    CHECK(net_wire_m_bcm_status_encode(CANDIDATE, 0x5aa5u, &m, &actual) == NET_WIRE_OK);
    CHECK(actual.length == expected.length && memcmp(actual.bytes, expected.bytes, expected.length) == 0);
    CHECK(net_wire_m_bcm_status_decode(CANDIDATE, &expected, &output, &sequence) == NET_WIRE_OK);
    CHECK(sequence == 0x5aa5u && equal_m_bcm_status(&m, &output));
    m = example_m_bcm_status();
    expected = literal_frame(golden_m_bcm_status, sizeof(golden_m_bcm_status));
    m.update_sequence = UINT32_C(0);
    patch_field(&expected, 36u, 4u, UINT64_C(0));
    CHECK(net_wire_m_bcm_status_encode(CANDIDATE, 0x5aa5u, &m, &actual) == NET_WIRE_OK);
    CHECK(actual.length == expected.length && memcmp(actual.bytes, expected.bytes, expected.length) == 0);
    CHECK(net_wire_m_bcm_status_decode(CANDIDATE, &expected, &output, &sequence) == NET_WIRE_OK);
    CHECK(sequence == 0x5aa5u && equal_m_bcm_status(&m, &output));
    m = example_m_bcm_status();
    expected = literal_frame(golden_m_bcm_status, sizeof(golden_m_bcm_status));
    m.update_sequence = UINT32_C(4294967295);
    patch_field(&expected, 36u, 4u, UINT64_C(4294967295));
    CHECK(net_wire_m_bcm_status_encode(CANDIDATE, 0x5aa5u, &m, &actual) == NET_WIRE_OK);
    CHECK(actual.length == expected.length && memcmp(actual.bytes, expected.bytes, expected.length) == 0);
    CHECK(net_wire_m_bcm_status_decode(CANDIDATE, &expected, &output, &sequence) == NET_WIRE_OK);
    CHECK(sequence == 0x5aa5u && equal_m_bcm_status(&m, &output));
    m = example_m_bcm_status();
    expected = literal_frame(golden_m_bcm_status, sizeof(golden_m_bcm_status));
    m.source_age_ms = UINT32_C(0);
    patch_field(&expected, 40u, 2u, UINT64_C(0));
    CHECK(net_wire_m_bcm_status_encode(CANDIDATE, 0x5aa5u, &m, &actual) == NET_WIRE_OK);
    CHECK(actual.length == expected.length && memcmp(actual.bytes, expected.bytes, expected.length) == 0);
    CHECK(net_wire_m_bcm_status_decode(CANDIDATE, &expected, &output, &sequence) == NET_WIRE_OK);
    CHECK(sequence == 0x5aa5u && equal_m_bcm_status(&m, &output));
    m = example_m_bcm_status();
    expected = literal_frame(golden_m_bcm_status, sizeof(golden_m_bcm_status));
    m.source_age_ms = UINT32_C(65535);
    patch_field(&expected, 40u, 2u, UINT64_C(65535));
    CHECK(net_wire_m_bcm_status_encode(CANDIDATE, 0x5aa5u, &m, &actual) == NET_WIRE_OK);
    CHECK(actual.length == expected.length && memcmp(actual.bytes, expected.bytes, expected.length) == 0);
    CHECK(net_wire_m_bcm_status_decode(CANDIDATE, &expected, &output, &sequence) == NET_WIRE_OK);
    CHECK(sequence == 0x5aa5u && equal_m_bcm_status(&m, &output));
    return 0;
}

/* FINAL_98/docs/network/interface/03_COMMUNICATION/20_UART_M.md:481; independent Python struct/CRC literal. */
static const uint8_t golden_m_cis_environment[70] = {
    0xa5,0x5a,0x02,0x34,0x3c,0x00,0xa5,0x5a,0xa5,0x5a,0x22,0x11,0x01,0x02,0x03,0x04,
    0x05,0x06,0x07,0x08,0x44,0x33,0x22,0x11,0x99,0x88,0x77,0x66,0xd4,0xc3,0xb2,0xa1,
    0x40,0x30,0x20,0x10,0x2e,0xfb,0x04,0x03,0x02,0x01,0x00,0x00,0x01,0x00,0x10,0x27,
    0x44,0x33,0x22,0x11,0xff,0xff,0x01,0x00,0x40,0xe2,0x01,0x00,0xd4,0xc3,0xb2,0xa1,
    0x7b,0x00,0x01,0x00,0x23,0xab,
};
static NetWireMCisEnvironmentRaw example_m_cis_environment(void)
{
    NetWireMCisEnvironmentRaw m = {0};
    m.device_context_id = UINT32_C(287464101);
    m.session_id = UINT64_C(578437695752307201);
    m.domain_boot_id = UINT32_C(287454020);
    m.query_id = UINT32_C(1719109785);
    m.original_can_boot_id = UINT32_C(2712847316);
    m.original_can_tx_sequence = UINT32_C(270544960);
    m.temperature_centi_c = -1234;
    m.temperature_sequence = UINT32_C(16909060);
    m.temperature_age_ms = UINT32_C(0);
    m.temperature_validity = UINT32_C(1);
    m.temperature_reason = UINT32_C(0);
    m.humidity_centi_percent = UINT32_C(10000);
    m.humidity_sequence = UINT32_C(287454020);
    m.humidity_age_ms = UINT32_C(65535);
    m.humidity_validity = UINT32_C(1);
    m.humidity_reason = UINT32_C(0);
    m.illuminance_lux = UINT32_C(123456);
    m.illuminance_sequence = UINT32_C(2712847316);
    m.illuminance_age_ms = UINT32_C(123);
    m.illuminance_validity = UINT32_C(1);
    m.illuminance_reason = UINT32_C(0);
    return m;
}
static int equal_m_cis_environment(const NetWireMCisEnvironmentRaw *a, const NetWireMCisEnvironmentRaw *b)
{
    return a->device_context_id == b->device_context_id &&
        a->session_id == b->session_id &&
        a->domain_boot_id == b->domain_boot_id &&
        a->query_id == b->query_id &&
        a->original_can_boot_id == b->original_can_boot_id &&
        a->original_can_tx_sequence == b->original_can_tx_sequence &&
        a->temperature_centi_c == b->temperature_centi_c &&
        a->temperature_sequence == b->temperature_sequence &&
        a->temperature_age_ms == b->temperature_age_ms &&
        a->temperature_validity == b->temperature_validity &&
        a->temperature_reason == b->temperature_reason &&
        a->humidity_centi_percent == b->humidity_centi_percent &&
        a->humidity_sequence == b->humidity_sequence &&
        a->humidity_age_ms == b->humidity_age_ms &&
        a->humidity_validity == b->humidity_validity &&
        a->humidity_reason == b->humidity_reason &&
        a->illuminance_lux == b->illuminance_lux &&
        a->illuminance_sequence == b->illuminance_sequence &&
        a->illuminance_age_ms == b->illuminance_age_ms &&
        a->illuminance_validity == b->illuminance_validity &&
        a->illuminance_reason == b->illuminance_reason;
}
static NetWireStatus encode_m_cis_environment(NetWireProfile p, uint16_t s, const void *m, NetWireUartFrame *f)
{
    return net_wire_m_cis_environment_encode(p, s, (const NetWireMCisEnvironmentRaw *)m, f);
}
static NetWireStatus decode_m_cis_environment(NetWireProfile p, const NetWireUartFrame *f, void *m, uint16_t *s)
{
    return net_wire_m_cis_environment_decode(p, f, (NetWireMCisEnvironmentRaw *)m, s);
}
static int test_m_cis_environment(void)
{
    NetWireMCisEnvironmentRaw m = example_m_cis_environment(), output;
    NetWireUartFrame expected = literal_frame(golden_m_cis_environment, sizeof(golden_m_cis_environment)), actual;
    uint16_t sequence = 0u;
    CHECK(net_wire_m_cis_environment_encode(CANDIDATE, 0x5aa5u, &m, &actual) == NET_WIRE_OK);
    CHECK(actual.length == expected.length && memcmp(actual.bytes, expected.bytes, expected.length) == 0);
    CHECK(net_wire_m_cis_environment_decode(CANDIDATE, &expected, &output, &sequence) == NET_WIRE_OK);
    CHECK(sequence == 0x5aa5u && equal_m_cis_environment(&m, &output));
    CHECK(common_errors(encode_m_cis_environment, decode_m_cis_environment, &m, sizeof(m), golden_m_cis_environment, sizeof(golden_m_cis_environment)) == 0);
    expected = literal_frame(golden_m_cis_environment, sizeof(golden_m_cis_environment));
    expected.bytes[6] = 0u; expected.bytes[7] = 0u; repair_crc(&expected);
    CHECK(net_wire_m_cis_environment_encode(CANDIDATE, 0u, &m, &actual) == NET_WIRE_OK);
    CHECK(actual.length == expected.length && memcmp(actual.bytes, expected.bytes, expected.length) == 0);
    CHECK(net_wire_m_cis_environment_decode(CANDIDATE, &expected, &output, &sequence) == NET_WIRE_OK);
    CHECK(sequence == 0u && equal_m_cis_environment(&m, &output));
    expected = literal_frame(golden_m_cis_environment, sizeof(golden_m_cis_environment));
    expected.bytes[6] = 255u; expected.bytes[7] = 255u; repair_crc(&expected);
    CHECK(net_wire_m_cis_environment_encode(CANDIDATE, 65535u, &m, &actual) == NET_WIRE_OK);
    CHECK(actual.length == expected.length && memcmp(actual.bytes, expected.bytes, expected.length) == 0);
    CHECK(net_wire_m_cis_environment_decode(CANDIDATE, &expected, &output, &sequence) == NET_WIRE_OK);
    CHECK(sequence == 65535u && equal_m_cis_environment(&m, &output));
    m = example_m_cis_environment();
    expected = literal_frame(golden_m_cis_environment, sizeof(golden_m_cis_environment));
    m.device_context_id = UINT32_C(0);
    patch_field(&expected, 0u, 4u, UINT64_C(0));
    CHECK(rejected(encode_m_cis_environment, decode_m_cis_environment, &m, sizeof(m), &expected, NET_WIRE_MALFORMED) == 0);
    m = example_m_cis_environment();
    expected = literal_frame(golden_m_cis_environment, sizeof(golden_m_cis_environment));
    m.domain_boot_id = UINT32_C(0);
    patch_field(&expected, 12u, 4u, UINT64_C(0));
    CHECK(rejected(encode_m_cis_environment, decode_m_cis_environment, &m, sizeof(m), &expected, NET_WIRE_MALFORMED) == 0);
    m = example_m_cis_environment();
    expected = literal_frame(golden_m_cis_environment, sizeof(golden_m_cis_environment));
    m.original_can_boot_id = UINT32_C(0);
    patch_field(&expected, 20u, 4u, UINT64_C(0));
    CHECK(rejected(encode_m_cis_environment, decode_m_cis_environment, &m, sizeof(m), &expected, NET_WIRE_MALFORMED) == 0);
    m = example_m_cis_environment();
    expected = literal_frame(golden_m_cis_environment, sizeof(golden_m_cis_environment));
    m.temperature_centi_c = -4001;
    patch_field(&expected, 28u, 2u, UINT64_C(61535));
    CHECK(rejected(encode_m_cis_environment, decode_m_cis_environment, &m, sizeof(m), &expected, NET_WIRE_MALFORMED) == 0);
    m = example_m_cis_environment();
    expected = literal_frame(golden_m_cis_environment, sizeof(golden_m_cis_environment));
    m.temperature_validity = UINT32_C(2);
    patch_field(&expected, 36u, 1u, UINT64_C(2));
    CHECK(rejected(encode_m_cis_environment, decode_m_cis_environment, &m, sizeof(m), &expected, NET_WIRE_MALFORMED) == 0);
    m = example_m_cis_environment();
    expected = literal_frame(golden_m_cis_environment, sizeof(golden_m_cis_environment));
    m.temperature_reason = UINT32_C(10);
    patch_field(&expected, 37u, 1u, UINT64_C(10));
    CHECK(rejected(encode_m_cis_environment, decode_m_cis_environment, &m, sizeof(m), &expected, NET_WIRE_MALFORMED) == 0);
    m = example_m_cis_environment();
    expected = literal_frame(golden_m_cis_environment, sizeof(golden_m_cis_environment));
    m.humidity_centi_percent = UINT32_C(10001);
    patch_field(&expected, 38u, 2u, UINT64_C(10001));
    CHECK(rejected(encode_m_cis_environment, decode_m_cis_environment, &m, sizeof(m), &expected, NET_WIRE_MALFORMED) == 0);
    m = example_m_cis_environment();
    expected = literal_frame(golden_m_cis_environment, sizeof(golden_m_cis_environment));
    m.humidity_validity = UINT32_C(2);
    patch_field(&expected, 46u, 1u, UINT64_C(2));
    CHECK(rejected(encode_m_cis_environment, decode_m_cis_environment, &m, sizeof(m), &expected, NET_WIRE_MALFORMED) == 0);
    m = example_m_cis_environment();
    expected = literal_frame(golden_m_cis_environment, sizeof(golden_m_cis_environment));
    m.humidity_reason = UINT32_C(10);
    patch_field(&expected, 47u, 1u, UINT64_C(10));
    CHECK(rejected(encode_m_cis_environment, decode_m_cis_environment, &m, sizeof(m), &expected, NET_WIRE_MALFORMED) == 0);
    m = example_m_cis_environment();
    expected = literal_frame(golden_m_cis_environment, sizeof(golden_m_cis_environment));
    m.illuminance_lux = UINT32_C(200001);
    patch_field(&expected, 48u, 4u, UINT64_C(200001));
    CHECK(rejected(encode_m_cis_environment, decode_m_cis_environment, &m, sizeof(m), &expected, NET_WIRE_MALFORMED) == 0);
    m = example_m_cis_environment();
    expected = literal_frame(golden_m_cis_environment, sizeof(golden_m_cis_environment));
    m.illuminance_validity = UINT32_C(2);
    patch_field(&expected, 58u, 1u, UINT64_C(2));
    CHECK(rejected(encode_m_cis_environment, decode_m_cis_environment, &m, sizeof(m), &expected, NET_WIRE_MALFORMED) == 0);
    m = example_m_cis_environment();
    expected = literal_frame(golden_m_cis_environment, sizeof(golden_m_cis_environment));
    m.illuminance_reason = UINT32_C(10);
    patch_field(&expected, 59u, 1u, UINT64_C(10));
    CHECK(rejected(encode_m_cis_environment, decode_m_cis_environment, &m, sizeof(m), &expected, NET_WIRE_MALFORMED) == 0);
    m = example_m_cis_environment();
    expected = literal_frame(golden_m_cis_environment, sizeof(golden_m_cis_environment));
    m.temperature_centi_c = -4000;
    patch_field(&expected, 28u, 2u, UINT64_C(61536));
    m.humidity_centi_percent = UINT32_C(0);
    patch_field(&expected, 38u, 2u, UINT64_C(0));
    m.illuminance_lux = UINT32_C(0);
    patch_field(&expected, 48u, 4u, UINT64_C(0));
    CHECK(net_wire_m_cis_environment_encode(CANDIDATE, 0x5aa5u, &m, &actual) == NET_WIRE_OK);
    CHECK(actual.length == expected.length && memcmp(actual.bytes, expected.bytes, expected.length) == 0);
    CHECK(net_wire_m_cis_environment_decode(CANDIDATE, &expected, &output, &sequence) == NET_WIRE_OK);
    CHECK(sequence == 0x5aa5u && equal_m_cis_environment(&m, &output));
    m = example_m_cis_environment();
    expected = literal_frame(golden_m_cis_environment, sizeof(golden_m_cis_environment));
    m.temperature_centi_c = 12500;
    patch_field(&expected, 28u, 2u, UINT64_C(12500));
    m.humidity_centi_percent = UINT32_C(10000);
    patch_field(&expected, 38u, 2u, UINT64_C(10000));
    m.illuminance_lux = UINT32_C(200000);
    patch_field(&expected, 48u, 4u, UINT64_C(200000));
    CHECK(net_wire_m_cis_environment_encode(CANDIDATE, 0x5aa5u, &m, &actual) == NET_WIRE_OK);
    CHECK(actual.length == expected.length && memcmp(actual.bytes, expected.bytes, expected.length) == 0);
    CHECK(net_wire_m_cis_environment_decode(CANDIDATE, &expected, &output, &sequence) == NET_WIRE_OK);
    CHECK(sequence == 0x5aa5u && equal_m_cis_environment(&m, &output));
    m = example_m_cis_environment();
    expected = literal_frame(golden_m_cis_environment, sizeof(golden_m_cis_environment));
    m.temperature_centi_c = 12501;
    patch_field(&expected, 28u, 2u, UINT64_C(12501));
    CHECK(rejected(encode_m_cis_environment, decode_m_cis_environment, &m, sizeof(m), &expected, NET_WIRE_MALFORMED) == 0);
    m = example_m_cis_environment();
    expected = literal_frame(golden_m_cis_environment, sizeof(golden_m_cis_environment));
    m.temperature_centi_c = -32768;
    patch_field(&expected, 28u, 2u, UINT64_C(32768));
    CHECK(rejected(encode_m_cis_environment, decode_m_cis_environment, &m, sizeof(m), &expected, NET_WIRE_MALFORMED) == 0);
    m = example_m_cis_environment();
    expected = literal_frame(golden_m_cis_environment, sizeof(golden_m_cis_environment));
    m.humidity_centi_percent = UINT32_C(65535);
    patch_field(&expected, 38u, 2u, UINT64_C(65535));
    CHECK(rejected(encode_m_cis_environment, decode_m_cis_environment, &m, sizeof(m), &expected, NET_WIRE_MALFORMED) == 0);
    m = example_m_cis_environment();
    expected = literal_frame(golden_m_cis_environment, sizeof(golden_m_cis_environment));
    m.illuminance_lux = UINT32_C(4294967295);
    patch_field(&expected, 48u, 4u, UINT64_C(4294967295));
    CHECK(rejected(encode_m_cis_environment, decode_m_cis_environment, &m, sizeof(m), &expected, NET_WIRE_MALFORMED) == 0);
    m = example_m_cis_environment();
    expected = literal_frame(golden_m_cis_environment, sizeof(golden_m_cis_environment));
    m.temperature_reason = UINT32_C(1);
    patch_field(&expected, 37u, 1u, UINT64_C(1));
    CHECK(rejected(encode_m_cis_environment, decode_m_cis_environment, &m, sizeof(m), &expected, NET_WIRE_CONTRACT_HOLD) == 0);
    m = example_m_cis_environment();
    expected = literal_frame(golden_m_cis_environment, sizeof(golden_m_cis_environment));
    m.humidity_validity = UINT32_C(0);
    patch_field(&expected, 46u, 1u, UINT64_C(0));
    CHECK(rejected(encode_m_cis_environment, decode_m_cis_environment, &m, sizeof(m), &expected, NET_WIRE_CONTRACT_HOLD) == 0);
    m = example_m_cis_environment();
    expected = literal_frame(golden_m_cis_environment, sizeof(golden_m_cis_environment));
    m.illuminance_reason = UINT32_C(9);
    patch_field(&expected, 59u, 1u, UINT64_C(9));
    CHECK(rejected(encode_m_cis_environment, decode_m_cis_environment, &m, sizeof(m), &expected, NET_WIRE_CONTRACT_HOLD) == 0);
    m = example_m_cis_environment();
    expected = literal_frame(golden_m_cis_environment, sizeof(golden_m_cis_environment));
    m.temperature_centi_c = -32768;
    patch_field(&expected, 28u, 2u, UINT64_C(32768));
    m.temperature_validity = UINT32_C(0);
    patch_field(&expected, 36u, 1u, UINT64_C(0));
    m.temperature_reason = UINT32_C(6);
    patch_field(&expected, 37u, 1u, UINT64_C(6));
    m.humidity_centi_percent = UINT32_C(65535);
    patch_field(&expected, 38u, 2u, UINT64_C(65535));
    m.humidity_validity = UINT32_C(0);
    patch_field(&expected, 46u, 1u, UINT64_C(0));
    m.humidity_reason = UINT32_C(6);
    patch_field(&expected, 47u, 1u, UINT64_C(6));
    m.illuminance_lux = UINT32_C(4294967295);
    patch_field(&expected, 48u, 4u, UINT64_C(4294967295));
    m.illuminance_validity = UINT32_C(0);
    patch_field(&expected, 58u, 1u, UINT64_C(0));
    m.illuminance_reason = UINT32_C(6);
    patch_field(&expected, 59u, 1u, UINT64_C(6));
    CHECK(net_wire_m_cis_environment_encode(CANDIDATE, 0x5aa5u, &m, &actual) == NET_WIRE_OK);
    CHECK(actual.length == expected.length && memcmp(actual.bytes, expected.bytes, expected.length) == 0);
    CHECK(net_wire_m_cis_environment_decode(CANDIDATE, &expected, &output, &sequence) == NET_WIRE_OK);
    CHECK(sequence == 0x5aa5u && equal_m_cis_environment(&m, &output));
    m = example_m_cis_environment();
    expected = literal_frame(golden_m_cis_environment, sizeof(golden_m_cis_environment));
    m.temperature_sequence = UINT32_C(0);
    patch_field(&expected, 30u, 4u, UINT64_C(0));
    m.humidity_sequence = UINT32_C(4294967295);
    patch_field(&expected, 40u, 4u, UINT64_C(4294967295));
    m.illuminance_sequence = UINT32_C(17);
    patch_field(&expected, 52u, 4u, UINT64_C(17));
    m.temperature_age_ms = UINT32_C(65535);
    patch_field(&expected, 34u, 2u, UINT64_C(65535));
    m.humidity_age_ms = UINT32_C(0);
    patch_field(&expected, 44u, 2u, UINT64_C(0));
    m.illuminance_age_ms = UINT32_C(500);
    patch_field(&expected, 56u, 2u, UINT64_C(500));
    CHECK(net_wire_m_cis_environment_encode(CANDIDATE, 0x5aa5u, &m, &actual) == NET_WIRE_OK);
    CHECK(actual.length == expected.length && memcmp(actual.bytes, expected.bytes, expected.length) == 0);
    CHECK(net_wire_m_cis_environment_decode(CANDIDATE, &expected, &output, &sequence) == NET_WIRE_OK);
    CHECK(sequence == 0x5aa5u && equal_m_cis_environment(&m, &output));
    m = example_m_cis_environment();
    expected = literal_frame(golden_m_cis_environment, sizeof(golden_m_cis_environment));
    m.device_context_id = UINT32_C(1);
    patch_field(&expected, 0u, 4u, UINT64_C(1));
    CHECK(net_wire_m_cis_environment_encode(CANDIDATE, 0x5aa5u, &m, &actual) == NET_WIRE_OK);
    CHECK(actual.length == expected.length && memcmp(actual.bytes, expected.bytes, expected.length) == 0);
    CHECK(net_wire_m_cis_environment_decode(CANDIDATE, &expected, &output, &sequence) == NET_WIRE_OK);
    CHECK(sequence == 0x5aa5u && equal_m_cis_environment(&m, &output));
    m = example_m_cis_environment();
    expected = literal_frame(golden_m_cis_environment, sizeof(golden_m_cis_environment));
    m.device_context_id = UINT32_C(4294967295);
    patch_field(&expected, 0u, 4u, UINT64_C(4294967295));
    CHECK(net_wire_m_cis_environment_encode(CANDIDATE, 0x5aa5u, &m, &actual) == NET_WIRE_OK);
    CHECK(actual.length == expected.length && memcmp(actual.bytes, expected.bytes, expected.length) == 0);
    CHECK(net_wire_m_cis_environment_decode(CANDIDATE, &expected, &output, &sequence) == NET_WIRE_OK);
    CHECK(sequence == 0x5aa5u && equal_m_cis_environment(&m, &output));
    m = example_m_cis_environment();
    expected = literal_frame(golden_m_cis_environment, sizeof(golden_m_cis_environment));
    m.domain_boot_id = UINT32_C(1);
    patch_field(&expected, 12u, 4u, UINT64_C(1));
    CHECK(net_wire_m_cis_environment_encode(CANDIDATE, 0x5aa5u, &m, &actual) == NET_WIRE_OK);
    CHECK(actual.length == expected.length && memcmp(actual.bytes, expected.bytes, expected.length) == 0);
    CHECK(net_wire_m_cis_environment_decode(CANDIDATE, &expected, &output, &sequence) == NET_WIRE_OK);
    CHECK(sequence == 0x5aa5u && equal_m_cis_environment(&m, &output));
    m = example_m_cis_environment();
    expected = literal_frame(golden_m_cis_environment, sizeof(golden_m_cis_environment));
    m.domain_boot_id = UINT32_C(4294967295);
    patch_field(&expected, 12u, 4u, UINT64_C(4294967295));
    CHECK(net_wire_m_cis_environment_encode(CANDIDATE, 0x5aa5u, &m, &actual) == NET_WIRE_OK);
    CHECK(actual.length == expected.length && memcmp(actual.bytes, expected.bytes, expected.length) == 0);
    CHECK(net_wire_m_cis_environment_decode(CANDIDATE, &expected, &output, &sequence) == NET_WIRE_OK);
    CHECK(sequence == 0x5aa5u && equal_m_cis_environment(&m, &output));
    m = example_m_cis_environment();
    expected = literal_frame(golden_m_cis_environment, sizeof(golden_m_cis_environment));
    m.original_can_boot_id = UINT32_C(1);
    patch_field(&expected, 20u, 4u, UINT64_C(1));
    CHECK(net_wire_m_cis_environment_encode(CANDIDATE, 0x5aa5u, &m, &actual) == NET_WIRE_OK);
    CHECK(actual.length == expected.length && memcmp(actual.bytes, expected.bytes, expected.length) == 0);
    CHECK(net_wire_m_cis_environment_decode(CANDIDATE, &expected, &output, &sequence) == NET_WIRE_OK);
    CHECK(sequence == 0x5aa5u && equal_m_cis_environment(&m, &output));
    m = example_m_cis_environment();
    expected = literal_frame(golden_m_cis_environment, sizeof(golden_m_cis_environment));
    m.original_can_boot_id = UINT32_C(4294967295);
    patch_field(&expected, 20u, 4u, UINT64_C(4294967295));
    CHECK(net_wire_m_cis_environment_encode(CANDIDATE, 0x5aa5u, &m, &actual) == NET_WIRE_OK);
    CHECK(actual.length == expected.length && memcmp(actual.bytes, expected.bytes, expected.length) == 0);
    CHECK(net_wire_m_cis_environment_decode(CANDIDATE, &expected, &output, &sequence) == NET_WIRE_OK);
    CHECK(sequence == 0x5aa5u && equal_m_cis_environment(&m, &output));
    m = example_m_cis_environment();
    expected = literal_frame(golden_m_cis_environment, sizeof(golden_m_cis_environment));
    m.session_id = UINT64_C(0);
    patch_field(&expected, 4u, 8u, UINT64_C(0));
    CHECK(net_wire_m_cis_environment_encode(CANDIDATE, 0x5aa5u, &m, &actual) == NET_WIRE_OK);
    CHECK(actual.length == expected.length && memcmp(actual.bytes, expected.bytes, expected.length) == 0);
    CHECK(net_wire_m_cis_environment_decode(CANDIDATE, &expected, &output, &sequence) == NET_WIRE_OK);
    CHECK(sequence == 0x5aa5u && equal_m_cis_environment(&m, &output));
    m = example_m_cis_environment();
    expected = literal_frame(golden_m_cis_environment, sizeof(golden_m_cis_environment));
    m.session_id = UINT64_C(18446744073709551615);
    patch_field(&expected, 4u, 8u, UINT64_C(18446744073709551615));
    CHECK(net_wire_m_cis_environment_encode(CANDIDATE, 0x5aa5u, &m, &actual) == NET_WIRE_OK);
    CHECK(actual.length == expected.length && memcmp(actual.bytes, expected.bytes, expected.length) == 0);
    CHECK(net_wire_m_cis_environment_decode(CANDIDATE, &expected, &output, &sequence) == NET_WIRE_OK);
    CHECK(sequence == 0x5aa5u && equal_m_cis_environment(&m, &output));
    m = example_m_cis_environment();
    expected = literal_frame(golden_m_cis_environment, sizeof(golden_m_cis_environment));
    m.query_id = UINT32_C(0);
    patch_field(&expected, 16u, 4u, UINT64_C(0));
    CHECK(net_wire_m_cis_environment_encode(CANDIDATE, 0x5aa5u, &m, &actual) == NET_WIRE_OK);
    CHECK(actual.length == expected.length && memcmp(actual.bytes, expected.bytes, expected.length) == 0);
    CHECK(net_wire_m_cis_environment_decode(CANDIDATE, &expected, &output, &sequence) == NET_WIRE_OK);
    CHECK(sequence == 0x5aa5u && equal_m_cis_environment(&m, &output));
    m = example_m_cis_environment();
    expected = literal_frame(golden_m_cis_environment, sizeof(golden_m_cis_environment));
    m.query_id = UINT32_C(4294967295);
    patch_field(&expected, 16u, 4u, UINT64_C(4294967295));
    CHECK(net_wire_m_cis_environment_encode(CANDIDATE, 0x5aa5u, &m, &actual) == NET_WIRE_OK);
    CHECK(actual.length == expected.length && memcmp(actual.bytes, expected.bytes, expected.length) == 0);
    CHECK(net_wire_m_cis_environment_decode(CANDIDATE, &expected, &output, &sequence) == NET_WIRE_OK);
    CHECK(sequence == 0x5aa5u && equal_m_cis_environment(&m, &output));
    m = example_m_cis_environment();
    expected = literal_frame(golden_m_cis_environment, sizeof(golden_m_cis_environment));
    m.original_can_tx_sequence = UINT32_C(0);
    patch_field(&expected, 24u, 4u, UINT64_C(0));
    CHECK(net_wire_m_cis_environment_encode(CANDIDATE, 0x5aa5u, &m, &actual) == NET_WIRE_OK);
    CHECK(actual.length == expected.length && memcmp(actual.bytes, expected.bytes, expected.length) == 0);
    CHECK(net_wire_m_cis_environment_decode(CANDIDATE, &expected, &output, &sequence) == NET_WIRE_OK);
    CHECK(sequence == 0x5aa5u && equal_m_cis_environment(&m, &output));
    m = example_m_cis_environment();
    expected = literal_frame(golden_m_cis_environment, sizeof(golden_m_cis_environment));
    m.original_can_tx_sequence = UINT32_C(4294967295);
    patch_field(&expected, 24u, 4u, UINT64_C(4294967295));
    CHECK(net_wire_m_cis_environment_encode(CANDIDATE, 0x5aa5u, &m, &actual) == NET_WIRE_OK);
    CHECK(actual.length == expected.length && memcmp(actual.bytes, expected.bytes, expected.length) == 0);
    CHECK(net_wire_m_cis_environment_decode(CANDIDATE, &expected, &output, &sequence) == NET_WIRE_OK);
    CHECK(sequence == 0x5aa5u && equal_m_cis_environment(&m, &output));
    return 0;
}

/* FINAL_98/docs/network/interface/03_COMMUNICATION/20_UART_M.md:510; independent Python struct/CRC literal. */
static const uint8_t golden_m_cis_occupant[55] = {
    0xa5,0x5a,0x02,0x35,0x2d,0x00,0xa5,0x5a,0xa5,0x5a,0x22,0x11,0x01,0x02,0x03,0x04,
    0x05,0x06,0x07,0x08,0x44,0x33,0x22,0x11,0x99,0x88,0x77,0x66,0xd4,0xc3,0xb2,0xa1,
    0x40,0x30,0x20,0x10,0x88,0x77,0x66,0x55,0x44,0x33,0x22,0x11,0xff,0xff,0x01,0x01,
    0x00,0x03,0x01,0x00,0x02,0x88,0x2d,
};
static NetWireMCisOccupantRaw example_m_cis_occupant(void)
{
    NetWireMCisOccupantRaw m = {0};
    m.device_context_id = UINT32_C(287464101);
    m.session_id = UINT64_C(578437695752307201);
    m.domain_boot_id = UINT32_C(287454020);
    m.query_id = UINT32_C(1719109785);
    m.original_can_boot_id = UINT32_C(2712847316);
    m.original_can_tx_sequence = UINT32_C(270544960);
    m.pi_boot_id = UINT32_C(1432778632);
    m.vision_sequence = UINT32_C(287454020);
    m.vision_age_ms = UINT32_C(65535);
    m.occupant_presence = UINT32_C(1);
    m.presence_validity = UINT32_C(1);
    m.presence_reason = UINT32_C(0);
    m.occupant_count = UINT32_C(3);
    m.count_validity = UINT32_C(1);
    m.count_reason = UINT32_C(0);
    m.vision_function_status = UINT32_C(2);
    return m;
}
static int equal_m_cis_occupant(const NetWireMCisOccupantRaw *a, const NetWireMCisOccupantRaw *b)
{
    return a->device_context_id == b->device_context_id &&
        a->session_id == b->session_id &&
        a->domain_boot_id == b->domain_boot_id &&
        a->query_id == b->query_id &&
        a->original_can_boot_id == b->original_can_boot_id &&
        a->original_can_tx_sequence == b->original_can_tx_sequence &&
        a->pi_boot_id == b->pi_boot_id &&
        a->vision_sequence == b->vision_sequence &&
        a->vision_age_ms == b->vision_age_ms &&
        a->occupant_presence == b->occupant_presence &&
        a->presence_validity == b->presence_validity &&
        a->presence_reason == b->presence_reason &&
        a->occupant_count == b->occupant_count &&
        a->count_validity == b->count_validity &&
        a->count_reason == b->count_reason &&
        a->vision_function_status == b->vision_function_status;
}
static NetWireStatus encode_m_cis_occupant(NetWireProfile p, uint16_t s, const void *m, NetWireUartFrame *f)
{
    return net_wire_m_cis_occupant_encode(p, s, (const NetWireMCisOccupantRaw *)m, f);
}
static NetWireStatus decode_m_cis_occupant(NetWireProfile p, const NetWireUartFrame *f, void *m, uint16_t *s)
{
    return net_wire_m_cis_occupant_decode(p, f, (NetWireMCisOccupantRaw *)m, s);
}
static int test_m_cis_occupant(void)
{
    NetWireMCisOccupantRaw m = example_m_cis_occupant(), output;
    NetWireUartFrame expected = literal_frame(golden_m_cis_occupant, sizeof(golden_m_cis_occupant)), actual;
    uint16_t sequence = 0u;
    CHECK(net_wire_m_cis_occupant_encode(CANDIDATE, 0x5aa5u, &m, &actual) == NET_WIRE_OK);
    CHECK(actual.length == expected.length && memcmp(actual.bytes, expected.bytes, expected.length) == 0);
    CHECK(net_wire_m_cis_occupant_decode(CANDIDATE, &expected, &output, &sequence) == NET_WIRE_OK);
    CHECK(sequence == 0x5aa5u && equal_m_cis_occupant(&m, &output));
    CHECK(common_errors(encode_m_cis_occupant, decode_m_cis_occupant, &m, sizeof(m), golden_m_cis_occupant, sizeof(golden_m_cis_occupant)) == 0);
    expected = literal_frame(golden_m_cis_occupant, sizeof(golden_m_cis_occupant));
    expected.bytes[6] = 0u; expected.bytes[7] = 0u; repair_crc(&expected);
    CHECK(net_wire_m_cis_occupant_encode(CANDIDATE, 0u, &m, &actual) == NET_WIRE_OK);
    CHECK(actual.length == expected.length && memcmp(actual.bytes, expected.bytes, expected.length) == 0);
    CHECK(net_wire_m_cis_occupant_decode(CANDIDATE, &expected, &output, &sequence) == NET_WIRE_OK);
    CHECK(sequence == 0u && equal_m_cis_occupant(&m, &output));
    expected = literal_frame(golden_m_cis_occupant, sizeof(golden_m_cis_occupant));
    expected.bytes[6] = 255u; expected.bytes[7] = 255u; repair_crc(&expected);
    CHECK(net_wire_m_cis_occupant_encode(CANDIDATE, 65535u, &m, &actual) == NET_WIRE_OK);
    CHECK(actual.length == expected.length && memcmp(actual.bytes, expected.bytes, expected.length) == 0);
    CHECK(net_wire_m_cis_occupant_decode(CANDIDATE, &expected, &output, &sequence) == NET_WIRE_OK);
    CHECK(sequence == 65535u && equal_m_cis_occupant(&m, &output));
    m = example_m_cis_occupant();
    expected = literal_frame(golden_m_cis_occupant, sizeof(golden_m_cis_occupant));
    m.device_context_id = UINT32_C(0);
    patch_field(&expected, 0u, 4u, UINT64_C(0));
    CHECK(rejected(encode_m_cis_occupant, decode_m_cis_occupant, &m, sizeof(m), &expected, NET_WIRE_MALFORMED) == 0);
    m = example_m_cis_occupant();
    expected = literal_frame(golden_m_cis_occupant, sizeof(golden_m_cis_occupant));
    m.domain_boot_id = UINT32_C(0);
    patch_field(&expected, 12u, 4u, UINT64_C(0));
    CHECK(rejected(encode_m_cis_occupant, decode_m_cis_occupant, &m, sizeof(m), &expected, NET_WIRE_MALFORMED) == 0);
    m = example_m_cis_occupant();
    expected = literal_frame(golden_m_cis_occupant, sizeof(golden_m_cis_occupant));
    m.original_can_boot_id = UINT32_C(0);
    patch_field(&expected, 20u, 4u, UINT64_C(0));
    CHECK(rejected(encode_m_cis_occupant, decode_m_cis_occupant, &m, sizeof(m), &expected, NET_WIRE_MALFORMED) == 0);
    m = example_m_cis_occupant();
    expected = literal_frame(golden_m_cis_occupant, sizeof(golden_m_cis_occupant));
    m.occupant_presence = UINT32_C(2);
    patch_field(&expected, 38u, 1u, UINT64_C(2));
    CHECK(rejected(encode_m_cis_occupant, decode_m_cis_occupant, &m, sizeof(m), &expected, NET_WIRE_MALFORMED) == 0);
    m = example_m_cis_occupant();
    expected = literal_frame(golden_m_cis_occupant, sizeof(golden_m_cis_occupant));
    m.presence_validity = UINT32_C(2);
    patch_field(&expected, 39u, 1u, UINT64_C(2));
    CHECK(rejected(encode_m_cis_occupant, decode_m_cis_occupant, &m, sizeof(m), &expected, NET_WIRE_MALFORMED) == 0);
    m = example_m_cis_occupant();
    expected = literal_frame(golden_m_cis_occupant, sizeof(golden_m_cis_occupant));
    m.presence_reason = UINT32_C(10);
    patch_field(&expected, 40u, 1u, UINT64_C(10));
    CHECK(rejected(encode_m_cis_occupant, decode_m_cis_occupant, &m, sizeof(m), &expected, NET_WIRE_MALFORMED) == 0);
    m = example_m_cis_occupant();
    expected = literal_frame(golden_m_cis_occupant, sizeof(golden_m_cis_occupant));
    m.occupant_count = UINT32_C(6);
    patch_field(&expected, 41u, 1u, UINT64_C(6));
    CHECK(rejected(encode_m_cis_occupant, decode_m_cis_occupant, &m, sizeof(m), &expected, NET_WIRE_MALFORMED) == 0);
    m = example_m_cis_occupant();
    expected = literal_frame(golden_m_cis_occupant, sizeof(golden_m_cis_occupant));
    m.count_validity = UINT32_C(2);
    patch_field(&expected, 42u, 1u, UINT64_C(2));
    CHECK(rejected(encode_m_cis_occupant, decode_m_cis_occupant, &m, sizeof(m), &expected, NET_WIRE_MALFORMED) == 0);
    m = example_m_cis_occupant();
    expected = literal_frame(golden_m_cis_occupant, sizeof(golden_m_cis_occupant));
    m.count_reason = UINT32_C(10);
    patch_field(&expected, 43u, 1u, UINT64_C(10));
    CHECK(rejected(encode_m_cis_occupant, decode_m_cis_occupant, &m, sizeof(m), &expected, NET_WIRE_MALFORMED) == 0);
    m = example_m_cis_occupant();
    expected = literal_frame(golden_m_cis_occupant, sizeof(golden_m_cis_occupant));
    m.vision_function_status = UINT32_C(5);
    patch_field(&expected, 44u, 1u, UINT64_C(5));
    CHECK(rejected(encode_m_cis_occupant, decode_m_cis_occupant, &m, sizeof(m), &expected, NET_WIRE_MALFORMED) == 0);
    m = example_m_cis_occupant();
    expected = literal_frame(golden_m_cis_occupant, sizeof(golden_m_cis_occupant));
    m.occupant_presence = UINT32_C(0);
    patch_field(&expected, 38u, 1u, UINT64_C(0));
    m.occupant_count = UINT32_C(0);
    patch_field(&expected, 41u, 1u, UINT64_C(0));
    CHECK(net_wire_m_cis_occupant_encode(CANDIDATE, 0x5aa5u, &m, &actual) == NET_WIRE_OK);
    CHECK(actual.length == expected.length && memcmp(actual.bytes, expected.bytes, expected.length) == 0);
    CHECK(net_wire_m_cis_occupant_decode(CANDIDATE, &expected, &output, &sequence) == NET_WIRE_OK);
    CHECK(sequence == 0x5aa5u && equal_m_cis_occupant(&m, &output));
    m = example_m_cis_occupant();
    expected = literal_frame(golden_m_cis_occupant, sizeof(golden_m_cis_occupant));
    m.occupant_count = UINT32_C(5);
    patch_field(&expected, 41u, 1u, UINT64_C(5));
    CHECK(net_wire_m_cis_occupant_encode(CANDIDATE, 0x5aa5u, &m, &actual) == NET_WIRE_OK);
    CHECK(actual.length == expected.length && memcmp(actual.bytes, expected.bytes, expected.length) == 0);
    CHECK(net_wire_m_cis_occupant_decode(CANDIDATE, &expected, &output, &sequence) == NET_WIRE_OK);
    CHECK(sequence == 0x5aa5u && equal_m_cis_occupant(&m, &output));
    m = example_m_cis_occupant();
    expected = literal_frame(golden_m_cis_occupant, sizeof(golden_m_cis_occupant));
    m.occupant_presence = UINT32_C(255);
    patch_field(&expected, 38u, 1u, UINT64_C(255));
    m.presence_validity = UINT32_C(0);
    patch_field(&expected, 39u, 1u, UINT64_C(0));
    m.presence_reason = UINT32_C(6);
    patch_field(&expected, 40u, 1u, UINT64_C(6));
    m.occupant_count = UINT32_C(255);
    patch_field(&expected, 41u, 1u, UINT64_C(255));
    m.count_validity = UINT32_C(0);
    patch_field(&expected, 42u, 1u, UINT64_C(0));
    m.count_reason = UINT32_C(6);
    patch_field(&expected, 43u, 1u, UINT64_C(6));
    m.vision_function_status = UINT32_C(0);
    patch_field(&expected, 44u, 1u, UINT64_C(0));
    CHECK(net_wire_m_cis_occupant_encode(CANDIDATE, 0x5aa5u, &m, &actual) == NET_WIRE_OK);
    CHECK(actual.length == expected.length && memcmp(actual.bytes, expected.bytes, expected.length) == 0);
    CHECK(net_wire_m_cis_occupant_decode(CANDIDATE, &expected, &output, &sequence) == NET_WIRE_OK);
    CHECK(sequence == 0x5aa5u && equal_m_cis_occupant(&m, &output));
    m = example_m_cis_occupant();
    expected = literal_frame(golden_m_cis_occupant, sizeof(golden_m_cis_occupant));
    m.occupant_presence = UINT32_C(0);
    patch_field(&expected, 38u, 1u, UINT64_C(0));
    m.occupant_count = UINT32_C(1);
    patch_field(&expected, 41u, 1u, UINT64_C(1));
    CHECK(rejected(encode_m_cis_occupant, decode_m_cis_occupant, &m, sizeof(m), &expected, NET_WIRE_MALFORMED) == 0);
    m = example_m_cis_occupant();
    expected = literal_frame(golden_m_cis_occupant, sizeof(golden_m_cis_occupant));
    m.occupant_count = UINT32_C(255);
    patch_field(&expected, 41u, 1u, UINT64_C(255));
    CHECK(rejected(encode_m_cis_occupant, decode_m_cis_occupant, &m, sizeof(m), &expected, NET_WIRE_MALFORMED) == 0);
    m = example_m_cis_occupant();
    expected = literal_frame(golden_m_cis_occupant, sizeof(golden_m_cis_occupant));
    m.pi_boot_id = UINT32_C(0);
    patch_field(&expected, 28u, 4u, UINT64_C(0));
    CHECK(rejected(encode_m_cis_occupant, decode_m_cis_occupant, &m, sizeof(m), &expected, NET_WIRE_CONTRACT_HOLD) == 0);
    m = example_m_cis_occupant();
    expected = literal_frame(golden_m_cis_occupant, sizeof(golden_m_cis_occupant));
    m.presence_reason = UINT32_C(4);
    patch_field(&expected, 40u, 1u, UINT64_C(4));
    CHECK(rejected(encode_m_cis_occupant, decode_m_cis_occupant, &m, sizeof(m), &expected, NET_WIRE_CONTRACT_HOLD) == 0);
    m = example_m_cis_occupant();
    expected = literal_frame(golden_m_cis_occupant, sizeof(golden_m_cis_occupant));
    m.count_validity = UINT32_C(0);
    patch_field(&expected, 42u, 1u, UINT64_C(0));
    CHECK(rejected(encode_m_cis_occupant, decode_m_cis_occupant, &m, sizeof(m), &expected, NET_WIRE_CONTRACT_HOLD) == 0);
    m = example_m_cis_occupant();
    expected = literal_frame(golden_m_cis_occupant, sizeof(golden_m_cis_occupant));
    m.pi_boot_id = UINT32_C(4294967295);
    patch_field(&expected, 28u, 4u, UINT64_C(4294967295));
    m.vision_sequence = UINT32_C(0);
    patch_field(&expected, 32u, 4u, UINT64_C(0));
    m.vision_age_ms = UINT32_C(0);
    patch_field(&expected, 36u, 2u, UINT64_C(0));
    CHECK(net_wire_m_cis_occupant_encode(CANDIDATE, 0x5aa5u, &m, &actual) == NET_WIRE_OK);
    CHECK(actual.length == expected.length && memcmp(actual.bytes, expected.bytes, expected.length) == 0);
    CHECK(net_wire_m_cis_occupant_decode(CANDIDATE, &expected, &output, &sequence) == NET_WIRE_OK);
    CHECK(sequence == 0x5aa5u && equal_m_cis_occupant(&m, &output));
    m = example_m_cis_occupant();
    expected = literal_frame(golden_m_cis_occupant, sizeof(golden_m_cis_occupant));
    m.device_context_id = UINT32_C(1);
    patch_field(&expected, 0u, 4u, UINT64_C(1));
    CHECK(net_wire_m_cis_occupant_encode(CANDIDATE, 0x5aa5u, &m, &actual) == NET_WIRE_OK);
    CHECK(actual.length == expected.length && memcmp(actual.bytes, expected.bytes, expected.length) == 0);
    CHECK(net_wire_m_cis_occupant_decode(CANDIDATE, &expected, &output, &sequence) == NET_WIRE_OK);
    CHECK(sequence == 0x5aa5u && equal_m_cis_occupant(&m, &output));
    m = example_m_cis_occupant();
    expected = literal_frame(golden_m_cis_occupant, sizeof(golden_m_cis_occupant));
    m.device_context_id = UINT32_C(4294967295);
    patch_field(&expected, 0u, 4u, UINT64_C(4294967295));
    CHECK(net_wire_m_cis_occupant_encode(CANDIDATE, 0x5aa5u, &m, &actual) == NET_WIRE_OK);
    CHECK(actual.length == expected.length && memcmp(actual.bytes, expected.bytes, expected.length) == 0);
    CHECK(net_wire_m_cis_occupant_decode(CANDIDATE, &expected, &output, &sequence) == NET_WIRE_OK);
    CHECK(sequence == 0x5aa5u && equal_m_cis_occupant(&m, &output));
    m = example_m_cis_occupant();
    expected = literal_frame(golden_m_cis_occupant, sizeof(golden_m_cis_occupant));
    m.domain_boot_id = UINT32_C(1);
    patch_field(&expected, 12u, 4u, UINT64_C(1));
    CHECK(net_wire_m_cis_occupant_encode(CANDIDATE, 0x5aa5u, &m, &actual) == NET_WIRE_OK);
    CHECK(actual.length == expected.length && memcmp(actual.bytes, expected.bytes, expected.length) == 0);
    CHECK(net_wire_m_cis_occupant_decode(CANDIDATE, &expected, &output, &sequence) == NET_WIRE_OK);
    CHECK(sequence == 0x5aa5u && equal_m_cis_occupant(&m, &output));
    m = example_m_cis_occupant();
    expected = literal_frame(golden_m_cis_occupant, sizeof(golden_m_cis_occupant));
    m.domain_boot_id = UINT32_C(4294967295);
    patch_field(&expected, 12u, 4u, UINT64_C(4294967295));
    CHECK(net_wire_m_cis_occupant_encode(CANDIDATE, 0x5aa5u, &m, &actual) == NET_WIRE_OK);
    CHECK(actual.length == expected.length && memcmp(actual.bytes, expected.bytes, expected.length) == 0);
    CHECK(net_wire_m_cis_occupant_decode(CANDIDATE, &expected, &output, &sequence) == NET_WIRE_OK);
    CHECK(sequence == 0x5aa5u && equal_m_cis_occupant(&m, &output));
    m = example_m_cis_occupant();
    expected = literal_frame(golden_m_cis_occupant, sizeof(golden_m_cis_occupant));
    m.original_can_boot_id = UINT32_C(1);
    patch_field(&expected, 20u, 4u, UINT64_C(1));
    CHECK(net_wire_m_cis_occupant_encode(CANDIDATE, 0x5aa5u, &m, &actual) == NET_WIRE_OK);
    CHECK(actual.length == expected.length && memcmp(actual.bytes, expected.bytes, expected.length) == 0);
    CHECK(net_wire_m_cis_occupant_decode(CANDIDATE, &expected, &output, &sequence) == NET_WIRE_OK);
    CHECK(sequence == 0x5aa5u && equal_m_cis_occupant(&m, &output));
    m = example_m_cis_occupant();
    expected = literal_frame(golden_m_cis_occupant, sizeof(golden_m_cis_occupant));
    m.original_can_boot_id = UINT32_C(4294967295);
    patch_field(&expected, 20u, 4u, UINT64_C(4294967295));
    CHECK(net_wire_m_cis_occupant_encode(CANDIDATE, 0x5aa5u, &m, &actual) == NET_WIRE_OK);
    CHECK(actual.length == expected.length && memcmp(actual.bytes, expected.bytes, expected.length) == 0);
    CHECK(net_wire_m_cis_occupant_decode(CANDIDATE, &expected, &output, &sequence) == NET_WIRE_OK);
    CHECK(sequence == 0x5aa5u && equal_m_cis_occupant(&m, &output));
    m = example_m_cis_occupant();
    expected = literal_frame(golden_m_cis_occupant, sizeof(golden_m_cis_occupant));
    m.session_id = UINT64_C(0);
    patch_field(&expected, 4u, 8u, UINT64_C(0));
    CHECK(net_wire_m_cis_occupant_encode(CANDIDATE, 0x5aa5u, &m, &actual) == NET_WIRE_OK);
    CHECK(actual.length == expected.length && memcmp(actual.bytes, expected.bytes, expected.length) == 0);
    CHECK(net_wire_m_cis_occupant_decode(CANDIDATE, &expected, &output, &sequence) == NET_WIRE_OK);
    CHECK(sequence == 0x5aa5u && equal_m_cis_occupant(&m, &output));
    m = example_m_cis_occupant();
    expected = literal_frame(golden_m_cis_occupant, sizeof(golden_m_cis_occupant));
    m.session_id = UINT64_C(18446744073709551615);
    patch_field(&expected, 4u, 8u, UINT64_C(18446744073709551615));
    CHECK(net_wire_m_cis_occupant_encode(CANDIDATE, 0x5aa5u, &m, &actual) == NET_WIRE_OK);
    CHECK(actual.length == expected.length && memcmp(actual.bytes, expected.bytes, expected.length) == 0);
    CHECK(net_wire_m_cis_occupant_decode(CANDIDATE, &expected, &output, &sequence) == NET_WIRE_OK);
    CHECK(sequence == 0x5aa5u && equal_m_cis_occupant(&m, &output));
    m = example_m_cis_occupant();
    expected = literal_frame(golden_m_cis_occupant, sizeof(golden_m_cis_occupant));
    m.query_id = UINT32_C(0);
    patch_field(&expected, 16u, 4u, UINT64_C(0));
    CHECK(net_wire_m_cis_occupant_encode(CANDIDATE, 0x5aa5u, &m, &actual) == NET_WIRE_OK);
    CHECK(actual.length == expected.length && memcmp(actual.bytes, expected.bytes, expected.length) == 0);
    CHECK(net_wire_m_cis_occupant_decode(CANDIDATE, &expected, &output, &sequence) == NET_WIRE_OK);
    CHECK(sequence == 0x5aa5u && equal_m_cis_occupant(&m, &output));
    m = example_m_cis_occupant();
    expected = literal_frame(golden_m_cis_occupant, sizeof(golden_m_cis_occupant));
    m.query_id = UINT32_C(4294967295);
    patch_field(&expected, 16u, 4u, UINT64_C(4294967295));
    CHECK(net_wire_m_cis_occupant_encode(CANDIDATE, 0x5aa5u, &m, &actual) == NET_WIRE_OK);
    CHECK(actual.length == expected.length && memcmp(actual.bytes, expected.bytes, expected.length) == 0);
    CHECK(net_wire_m_cis_occupant_decode(CANDIDATE, &expected, &output, &sequence) == NET_WIRE_OK);
    CHECK(sequence == 0x5aa5u && equal_m_cis_occupant(&m, &output));
    m = example_m_cis_occupant();
    expected = literal_frame(golden_m_cis_occupant, sizeof(golden_m_cis_occupant));
    m.original_can_tx_sequence = UINT32_C(0);
    patch_field(&expected, 24u, 4u, UINT64_C(0));
    CHECK(net_wire_m_cis_occupant_encode(CANDIDATE, 0x5aa5u, &m, &actual) == NET_WIRE_OK);
    CHECK(actual.length == expected.length && memcmp(actual.bytes, expected.bytes, expected.length) == 0);
    CHECK(net_wire_m_cis_occupant_decode(CANDIDATE, &expected, &output, &sequence) == NET_WIRE_OK);
    CHECK(sequence == 0x5aa5u && equal_m_cis_occupant(&m, &output));
    m = example_m_cis_occupant();
    expected = literal_frame(golden_m_cis_occupant, sizeof(golden_m_cis_occupant));
    m.original_can_tx_sequence = UINT32_C(4294967295);
    patch_field(&expected, 24u, 4u, UINT64_C(4294967295));
    CHECK(net_wire_m_cis_occupant_encode(CANDIDATE, 0x5aa5u, &m, &actual) == NET_WIRE_OK);
    CHECK(actual.length == expected.length && memcmp(actual.bytes, expected.bytes, expected.length) == 0);
    CHECK(net_wire_m_cis_occupant_decode(CANDIDATE, &expected, &output, &sequence) == NET_WIRE_OK);
    CHECK(sequence == 0x5aa5u && equal_m_cis_occupant(&m, &output));
    return 0;
}

/* FINAL_98/docs/network/interface/03_COMMUNICATION/20_UART_M.md:534; independent Python struct/CRC literal. */
static const uint8_t golden_m_cis_rear[49] = {
    0xa5,0x5a,0x02,0x36,0x27,0x00,0xa5,0x5a,0xa5,0x5a,0x22,0x11,0x01,0x02,0x03,0x04,
    0x05,0x06,0x07,0x08,0x44,0x33,0x22,0x11,0x99,0x88,0x77,0x66,0xd4,0xc3,0xb2,0xa1,
    0x40,0x30,0x20,0x10,0xef,0xbe,0xad,0xde,0xff,0xff,0x7b,0x00,0x01,0x00,0x00,0xeb,
    0x32,
};
static NetWireMCisRearRaw example_m_cis_rear(void)
{
    NetWireMCisRearRaw m = {0};
    m.device_context_id = UINT32_C(287464101);
    m.session_id = UINT64_C(578437695752307201);
    m.domain_boot_id = UINT32_C(287454020);
    m.query_id = UINT32_C(1719109785);
    m.original_can_boot_id = UINT32_C(2712847316);
    m.original_can_tx_sequence = UINT32_C(270544960);
    m.update_sequence = UINT32_C(3735928559);
    m.source_age_ms = UINT32_C(65535);
    m.rear_distance_deci_cm = UINT32_C(123);
    m.validity = UINT32_C(1);
    m.quality_reason = UINT32_C(0);
    m.proximity_status = UINT32_C(0);
    return m;
}
static int equal_m_cis_rear(const NetWireMCisRearRaw *a, const NetWireMCisRearRaw *b)
{
    return a->device_context_id == b->device_context_id &&
        a->session_id == b->session_id &&
        a->domain_boot_id == b->domain_boot_id &&
        a->query_id == b->query_id &&
        a->original_can_boot_id == b->original_can_boot_id &&
        a->original_can_tx_sequence == b->original_can_tx_sequence &&
        a->update_sequence == b->update_sequence &&
        a->source_age_ms == b->source_age_ms &&
        a->rear_distance_deci_cm == b->rear_distance_deci_cm &&
        a->validity == b->validity &&
        a->quality_reason == b->quality_reason &&
        a->proximity_status == b->proximity_status;
}
static NetWireStatus encode_m_cis_rear(NetWireProfile p, uint16_t s, const void *m, NetWireUartFrame *f)
{
    return net_wire_m_cis_rear_encode(p, s, (const NetWireMCisRearRaw *)m, f);
}
static NetWireStatus decode_m_cis_rear(NetWireProfile p, const NetWireUartFrame *f, void *m, uint16_t *s)
{
    return net_wire_m_cis_rear_decode(p, f, (NetWireMCisRearRaw *)m, s);
}
static int test_m_cis_rear(void)
{
    NetWireMCisRearRaw m = example_m_cis_rear(), output;
    NetWireUartFrame expected = literal_frame(golden_m_cis_rear, sizeof(golden_m_cis_rear)), actual;
    uint16_t sequence = 0u;
    CHECK(net_wire_m_cis_rear_encode(CANDIDATE, 0x5aa5u, &m, &actual) == NET_WIRE_OK);
    CHECK(actual.length == expected.length && memcmp(actual.bytes, expected.bytes, expected.length) == 0);
    CHECK(net_wire_m_cis_rear_decode(CANDIDATE, &expected, &output, &sequence) == NET_WIRE_OK);
    CHECK(sequence == 0x5aa5u && equal_m_cis_rear(&m, &output));
    CHECK(common_errors(encode_m_cis_rear, decode_m_cis_rear, &m, sizeof(m), golden_m_cis_rear, sizeof(golden_m_cis_rear)) == 0);
    expected = literal_frame(golden_m_cis_rear, sizeof(golden_m_cis_rear));
    expected.bytes[6] = 0u; expected.bytes[7] = 0u; repair_crc(&expected);
    CHECK(net_wire_m_cis_rear_encode(CANDIDATE, 0u, &m, &actual) == NET_WIRE_OK);
    CHECK(actual.length == expected.length && memcmp(actual.bytes, expected.bytes, expected.length) == 0);
    CHECK(net_wire_m_cis_rear_decode(CANDIDATE, &expected, &output, &sequence) == NET_WIRE_OK);
    CHECK(sequence == 0u && equal_m_cis_rear(&m, &output));
    expected = literal_frame(golden_m_cis_rear, sizeof(golden_m_cis_rear));
    expected.bytes[6] = 255u; expected.bytes[7] = 255u; repair_crc(&expected);
    CHECK(net_wire_m_cis_rear_encode(CANDIDATE, 65535u, &m, &actual) == NET_WIRE_OK);
    CHECK(actual.length == expected.length && memcmp(actual.bytes, expected.bytes, expected.length) == 0);
    CHECK(net_wire_m_cis_rear_decode(CANDIDATE, &expected, &output, &sequence) == NET_WIRE_OK);
    CHECK(sequence == 65535u && equal_m_cis_rear(&m, &output));
    m = example_m_cis_rear();
    expected = literal_frame(golden_m_cis_rear, sizeof(golden_m_cis_rear));
    m.device_context_id = UINT32_C(0);
    patch_field(&expected, 0u, 4u, UINT64_C(0));
    CHECK(rejected(encode_m_cis_rear, decode_m_cis_rear, &m, sizeof(m), &expected, NET_WIRE_MALFORMED) == 0);
    m = example_m_cis_rear();
    expected = literal_frame(golden_m_cis_rear, sizeof(golden_m_cis_rear));
    m.domain_boot_id = UINT32_C(0);
    patch_field(&expected, 12u, 4u, UINT64_C(0));
    CHECK(rejected(encode_m_cis_rear, decode_m_cis_rear, &m, sizeof(m), &expected, NET_WIRE_MALFORMED) == 0);
    m = example_m_cis_rear();
    expected = literal_frame(golden_m_cis_rear, sizeof(golden_m_cis_rear));
    m.original_can_boot_id = UINT32_C(0);
    patch_field(&expected, 20u, 4u, UINT64_C(0));
    CHECK(rejected(encode_m_cis_rear, decode_m_cis_rear, &m, sizeof(m), &expected, NET_WIRE_MALFORMED) == 0);
    m = example_m_cis_rear();
    expected = literal_frame(golden_m_cis_rear, sizeof(golden_m_cis_rear));
    m.validity = UINT32_C(2);
    patch_field(&expected, 36u, 1u, UINT64_C(2));
    CHECK(rejected(encode_m_cis_rear, decode_m_cis_rear, &m, sizeof(m), &expected, NET_WIRE_MALFORMED) == 0);
    m = example_m_cis_rear();
    expected = literal_frame(golden_m_cis_rear, sizeof(golden_m_cis_rear));
    m.quality_reason = UINT32_C(10);
    patch_field(&expected, 37u, 1u, UINT64_C(10));
    CHECK(rejected(encode_m_cis_rear, decode_m_cis_rear, &m, sizeof(m), &expected, NET_WIRE_MALFORMED) == 0);
    m = example_m_cis_rear();
    expected = literal_frame(golden_m_cis_rear, sizeof(golden_m_cis_rear));
    m.proximity_status = UINT32_C(6);
    patch_field(&expected, 38u, 1u, UINT64_C(6));
    CHECK(rejected(encode_m_cis_rear, decode_m_cis_rear, &m, sizeof(m), &expected, NET_WIRE_MALFORMED) == 0);
    m = example_m_cis_rear();
    expected = literal_frame(golden_m_cis_rear, sizeof(golden_m_cis_rear));
    m.rear_distance_deci_cm = UINT32_C(0);
    patch_field(&expected, 34u, 2u, UINT64_C(0));
    CHECK(net_wire_m_cis_rear_encode(CANDIDATE, 0x5aa5u, &m, &actual) == NET_WIRE_OK);
    CHECK(actual.length == expected.length && memcmp(actual.bytes, expected.bytes, expected.length) == 0);
    CHECK(net_wire_m_cis_rear_decode(CANDIDATE, &expected, &output, &sequence) == NET_WIRE_OK);
    CHECK(sequence == 0x5aa5u && equal_m_cis_rear(&m, &output));
    m = example_m_cis_rear();
    expected = literal_frame(golden_m_cis_rear, sizeof(golden_m_cis_rear));
    m.rear_distance_deci_cm = UINT32_C(65534);
    patch_field(&expected, 34u, 2u, UINT64_C(65534));
    CHECK(net_wire_m_cis_rear_encode(CANDIDATE, 0x5aa5u, &m, &actual) == NET_WIRE_OK);
    CHECK(actual.length == expected.length && memcmp(actual.bytes, expected.bytes, expected.length) == 0);
    CHECK(net_wire_m_cis_rear_decode(CANDIDATE, &expected, &output, &sequence) == NET_WIRE_OK);
    CHECK(sequence == 0x5aa5u && equal_m_cis_rear(&m, &output));
    m = example_m_cis_rear();
    expected = literal_frame(golden_m_cis_rear, sizeof(golden_m_cis_rear));
    m.rear_distance_deci_cm = UINT32_C(65535);
    patch_field(&expected, 34u, 2u, UINT64_C(65535));
    CHECK(rejected(encode_m_cis_rear, decode_m_cis_rear, &m, sizeof(m), &expected, NET_WIRE_MALFORMED) == 0);
    m = example_m_cis_rear();
    expected = literal_frame(golden_m_cis_rear, sizeof(golden_m_cis_rear));
    m.rear_distance_deci_cm = UINT32_C(65535);
    patch_field(&expected, 34u, 2u, UINT64_C(65535));
    m.proximity_status = UINT32_C(1);
    patch_field(&expected, 38u, 1u, UINT64_C(1));
    m.validity = UINT32_C(0);
    patch_field(&expected, 36u, 1u, UINT64_C(0));
    m.quality_reason = UINT32_C(6);
    patch_field(&expected, 37u, 1u, UINT64_C(6));
    CHECK(net_wire_m_cis_rear_encode(CANDIDATE, 0x5aa5u, &m, &actual) == NET_WIRE_OK);
    CHECK(actual.length == expected.length && memcmp(actual.bytes, expected.bytes, expected.length) == 0);
    CHECK(net_wire_m_cis_rear_decode(CANDIDATE, &expected, &output, &sequence) == NET_WIRE_OK);
    CHECK(sequence == 0x5aa5u && equal_m_cis_rear(&m, &output));
    m = example_m_cis_rear();
    expected = literal_frame(golden_m_cis_rear, sizeof(golden_m_cis_rear));
    m.proximity_status = UINT32_C(1);
    patch_field(&expected, 38u, 1u, UINT64_C(1));
    CHECK(rejected(encode_m_cis_rear, decode_m_cis_rear, &m, sizeof(m), &expected, NET_WIRE_CONTRACT_HOLD) == 0);
    m = example_m_cis_rear();
    expected = literal_frame(golden_m_cis_rear, sizeof(golden_m_cis_rear));
    m.quality_reason = UINT32_C(3);
    patch_field(&expected, 37u, 1u, UINT64_C(3));
    CHECK(rejected(encode_m_cis_rear, decode_m_cis_rear, &m, sizeof(m), &expected, NET_WIRE_CONTRACT_HOLD) == 0);
    m = example_m_cis_rear();
    expected = literal_frame(golden_m_cis_rear, sizeof(golden_m_cis_rear));
    m.validity = UINT32_C(0);
    patch_field(&expected, 36u, 1u, UINT64_C(0));
    CHECK(rejected(encode_m_cis_rear, decode_m_cis_rear, &m, sizeof(m), &expected, NET_WIRE_CONTRACT_HOLD) == 0);
    m = example_m_cis_rear();
    expected = literal_frame(golden_m_cis_rear, sizeof(golden_m_cis_rear));
    m.proximity_status = UINT32_C(3);
    patch_field(&expected, 38u, 1u, UINT64_C(3));
    CHECK(rejected(encode_m_cis_rear, decode_m_cis_rear, &m, sizeof(m), &expected, NET_WIRE_CONTRACT_HOLD) == 0);
    m = example_m_cis_rear();
    expected = literal_frame(golden_m_cis_rear, sizeof(golden_m_cis_rear));
    m.device_context_id = UINT32_C(1);
    patch_field(&expected, 0u, 4u, UINT64_C(1));
    CHECK(net_wire_m_cis_rear_encode(CANDIDATE, 0x5aa5u, &m, &actual) == NET_WIRE_OK);
    CHECK(actual.length == expected.length && memcmp(actual.bytes, expected.bytes, expected.length) == 0);
    CHECK(net_wire_m_cis_rear_decode(CANDIDATE, &expected, &output, &sequence) == NET_WIRE_OK);
    CHECK(sequence == 0x5aa5u && equal_m_cis_rear(&m, &output));
    m = example_m_cis_rear();
    expected = literal_frame(golden_m_cis_rear, sizeof(golden_m_cis_rear));
    m.device_context_id = UINT32_C(4294967295);
    patch_field(&expected, 0u, 4u, UINT64_C(4294967295));
    CHECK(net_wire_m_cis_rear_encode(CANDIDATE, 0x5aa5u, &m, &actual) == NET_WIRE_OK);
    CHECK(actual.length == expected.length && memcmp(actual.bytes, expected.bytes, expected.length) == 0);
    CHECK(net_wire_m_cis_rear_decode(CANDIDATE, &expected, &output, &sequence) == NET_WIRE_OK);
    CHECK(sequence == 0x5aa5u && equal_m_cis_rear(&m, &output));
    m = example_m_cis_rear();
    expected = literal_frame(golden_m_cis_rear, sizeof(golden_m_cis_rear));
    m.domain_boot_id = UINT32_C(1);
    patch_field(&expected, 12u, 4u, UINT64_C(1));
    CHECK(net_wire_m_cis_rear_encode(CANDIDATE, 0x5aa5u, &m, &actual) == NET_WIRE_OK);
    CHECK(actual.length == expected.length && memcmp(actual.bytes, expected.bytes, expected.length) == 0);
    CHECK(net_wire_m_cis_rear_decode(CANDIDATE, &expected, &output, &sequence) == NET_WIRE_OK);
    CHECK(sequence == 0x5aa5u && equal_m_cis_rear(&m, &output));
    m = example_m_cis_rear();
    expected = literal_frame(golden_m_cis_rear, sizeof(golden_m_cis_rear));
    m.domain_boot_id = UINT32_C(4294967295);
    patch_field(&expected, 12u, 4u, UINT64_C(4294967295));
    CHECK(net_wire_m_cis_rear_encode(CANDIDATE, 0x5aa5u, &m, &actual) == NET_WIRE_OK);
    CHECK(actual.length == expected.length && memcmp(actual.bytes, expected.bytes, expected.length) == 0);
    CHECK(net_wire_m_cis_rear_decode(CANDIDATE, &expected, &output, &sequence) == NET_WIRE_OK);
    CHECK(sequence == 0x5aa5u && equal_m_cis_rear(&m, &output));
    m = example_m_cis_rear();
    expected = literal_frame(golden_m_cis_rear, sizeof(golden_m_cis_rear));
    m.original_can_boot_id = UINT32_C(1);
    patch_field(&expected, 20u, 4u, UINT64_C(1));
    CHECK(net_wire_m_cis_rear_encode(CANDIDATE, 0x5aa5u, &m, &actual) == NET_WIRE_OK);
    CHECK(actual.length == expected.length && memcmp(actual.bytes, expected.bytes, expected.length) == 0);
    CHECK(net_wire_m_cis_rear_decode(CANDIDATE, &expected, &output, &sequence) == NET_WIRE_OK);
    CHECK(sequence == 0x5aa5u && equal_m_cis_rear(&m, &output));
    m = example_m_cis_rear();
    expected = literal_frame(golden_m_cis_rear, sizeof(golden_m_cis_rear));
    m.original_can_boot_id = UINT32_C(4294967295);
    patch_field(&expected, 20u, 4u, UINT64_C(4294967295));
    CHECK(net_wire_m_cis_rear_encode(CANDIDATE, 0x5aa5u, &m, &actual) == NET_WIRE_OK);
    CHECK(actual.length == expected.length && memcmp(actual.bytes, expected.bytes, expected.length) == 0);
    CHECK(net_wire_m_cis_rear_decode(CANDIDATE, &expected, &output, &sequence) == NET_WIRE_OK);
    CHECK(sequence == 0x5aa5u && equal_m_cis_rear(&m, &output));
    m = example_m_cis_rear();
    expected = literal_frame(golden_m_cis_rear, sizeof(golden_m_cis_rear));
    m.session_id = UINT64_C(0);
    patch_field(&expected, 4u, 8u, UINT64_C(0));
    CHECK(net_wire_m_cis_rear_encode(CANDIDATE, 0x5aa5u, &m, &actual) == NET_WIRE_OK);
    CHECK(actual.length == expected.length && memcmp(actual.bytes, expected.bytes, expected.length) == 0);
    CHECK(net_wire_m_cis_rear_decode(CANDIDATE, &expected, &output, &sequence) == NET_WIRE_OK);
    CHECK(sequence == 0x5aa5u && equal_m_cis_rear(&m, &output));
    m = example_m_cis_rear();
    expected = literal_frame(golden_m_cis_rear, sizeof(golden_m_cis_rear));
    m.session_id = UINT64_C(18446744073709551615);
    patch_field(&expected, 4u, 8u, UINT64_C(18446744073709551615));
    CHECK(net_wire_m_cis_rear_encode(CANDIDATE, 0x5aa5u, &m, &actual) == NET_WIRE_OK);
    CHECK(actual.length == expected.length && memcmp(actual.bytes, expected.bytes, expected.length) == 0);
    CHECK(net_wire_m_cis_rear_decode(CANDIDATE, &expected, &output, &sequence) == NET_WIRE_OK);
    CHECK(sequence == 0x5aa5u && equal_m_cis_rear(&m, &output));
    m = example_m_cis_rear();
    expected = literal_frame(golden_m_cis_rear, sizeof(golden_m_cis_rear));
    m.query_id = UINT32_C(0);
    patch_field(&expected, 16u, 4u, UINT64_C(0));
    CHECK(net_wire_m_cis_rear_encode(CANDIDATE, 0x5aa5u, &m, &actual) == NET_WIRE_OK);
    CHECK(actual.length == expected.length && memcmp(actual.bytes, expected.bytes, expected.length) == 0);
    CHECK(net_wire_m_cis_rear_decode(CANDIDATE, &expected, &output, &sequence) == NET_WIRE_OK);
    CHECK(sequence == 0x5aa5u && equal_m_cis_rear(&m, &output));
    m = example_m_cis_rear();
    expected = literal_frame(golden_m_cis_rear, sizeof(golden_m_cis_rear));
    m.query_id = UINT32_C(4294967295);
    patch_field(&expected, 16u, 4u, UINT64_C(4294967295));
    CHECK(net_wire_m_cis_rear_encode(CANDIDATE, 0x5aa5u, &m, &actual) == NET_WIRE_OK);
    CHECK(actual.length == expected.length && memcmp(actual.bytes, expected.bytes, expected.length) == 0);
    CHECK(net_wire_m_cis_rear_decode(CANDIDATE, &expected, &output, &sequence) == NET_WIRE_OK);
    CHECK(sequence == 0x5aa5u && equal_m_cis_rear(&m, &output));
    m = example_m_cis_rear();
    expected = literal_frame(golden_m_cis_rear, sizeof(golden_m_cis_rear));
    m.original_can_tx_sequence = UINT32_C(0);
    patch_field(&expected, 24u, 4u, UINT64_C(0));
    CHECK(net_wire_m_cis_rear_encode(CANDIDATE, 0x5aa5u, &m, &actual) == NET_WIRE_OK);
    CHECK(actual.length == expected.length && memcmp(actual.bytes, expected.bytes, expected.length) == 0);
    CHECK(net_wire_m_cis_rear_decode(CANDIDATE, &expected, &output, &sequence) == NET_WIRE_OK);
    CHECK(sequence == 0x5aa5u && equal_m_cis_rear(&m, &output));
    m = example_m_cis_rear();
    expected = literal_frame(golden_m_cis_rear, sizeof(golden_m_cis_rear));
    m.original_can_tx_sequence = UINT32_C(4294967295);
    patch_field(&expected, 24u, 4u, UINT64_C(4294967295));
    CHECK(net_wire_m_cis_rear_encode(CANDIDATE, 0x5aa5u, &m, &actual) == NET_WIRE_OK);
    CHECK(actual.length == expected.length && memcmp(actual.bytes, expected.bytes, expected.length) == 0);
    CHECK(net_wire_m_cis_rear_decode(CANDIDATE, &expected, &output, &sequence) == NET_WIRE_OK);
    CHECK(sequence == 0x5aa5u && equal_m_cis_rear(&m, &output));
    m = example_m_cis_rear();
    expected = literal_frame(golden_m_cis_rear, sizeof(golden_m_cis_rear));
    m.update_sequence = UINT32_C(0);
    patch_field(&expected, 28u, 4u, UINT64_C(0));
    CHECK(net_wire_m_cis_rear_encode(CANDIDATE, 0x5aa5u, &m, &actual) == NET_WIRE_OK);
    CHECK(actual.length == expected.length && memcmp(actual.bytes, expected.bytes, expected.length) == 0);
    CHECK(net_wire_m_cis_rear_decode(CANDIDATE, &expected, &output, &sequence) == NET_WIRE_OK);
    CHECK(sequence == 0x5aa5u && equal_m_cis_rear(&m, &output));
    m = example_m_cis_rear();
    expected = literal_frame(golden_m_cis_rear, sizeof(golden_m_cis_rear));
    m.update_sequence = UINT32_C(4294967295);
    patch_field(&expected, 28u, 4u, UINT64_C(4294967295));
    CHECK(net_wire_m_cis_rear_encode(CANDIDATE, 0x5aa5u, &m, &actual) == NET_WIRE_OK);
    CHECK(actual.length == expected.length && memcmp(actual.bytes, expected.bytes, expected.length) == 0);
    CHECK(net_wire_m_cis_rear_decode(CANDIDATE, &expected, &output, &sequence) == NET_WIRE_OK);
    CHECK(sequence == 0x5aa5u && equal_m_cis_rear(&m, &output));
    m = example_m_cis_rear();
    expected = literal_frame(golden_m_cis_rear, sizeof(golden_m_cis_rear));
    m.source_age_ms = UINT32_C(0);
    patch_field(&expected, 32u, 2u, UINT64_C(0));
    CHECK(net_wire_m_cis_rear_encode(CANDIDATE, 0x5aa5u, &m, &actual) == NET_WIRE_OK);
    CHECK(actual.length == expected.length && memcmp(actual.bytes, expected.bytes, expected.length) == 0);
    CHECK(net_wire_m_cis_rear_decode(CANDIDATE, &expected, &output, &sequence) == NET_WIRE_OK);
    CHECK(sequence == 0x5aa5u && equal_m_cis_rear(&m, &output));
    m = example_m_cis_rear();
    expected = literal_frame(golden_m_cis_rear, sizeof(golden_m_cis_rear));
    m.source_age_ms = UINT32_C(65535);
    patch_field(&expected, 32u, 2u, UINT64_C(65535));
    CHECK(net_wire_m_cis_rear_encode(CANDIDATE, 0x5aa5u, &m, &actual) == NET_WIRE_OK);
    CHECK(actual.length == expected.length && memcmp(actual.bytes, expected.bytes, expected.length) == 0);
    CHECK(net_wire_m_cis_rear_decode(CANDIDATE, &expected, &output, &sequence) == NET_WIRE_OK);
    CHECK(sequence == 0x5aa5u && equal_m_cis_rear(&m, &output));
    return 0;
}

/* FINAL_98/docs/network/interface/03_COMMUNICATION/20_UART_M.md:554; independent Python struct/CRC literal. */
static const uint8_t golden_m_cis_status[64] = {
    0xa5,0x5a,0x02,0x37,0x36,0x00,0xa5,0x5a,0xa5,0x5a,0x22,0x11,0x01,0x02,0x03,0x04,
    0x05,0x06,0x07,0x08,0x44,0x33,0x22,0x11,0x99,0x88,0x77,0x66,0xd4,0xc3,0xb2,0xa1,
    0x40,0x30,0x20,0x10,0x02,0x02,0x02,0x01,0x00,0x04,0x01,0x02,0x00,0x01,0x09,0x00,
    0x01,0x07,0x00,0x21,0x43,0x65,0x87,0x3f,0xff,0xef,0xcd,0xab,0xff,0xff,0x17,0xc2,
};
static NetWireMCisStatusRaw example_m_cis_status(void)
{
    NetWireMCisStatusRaw m = {0};
    m.device_context_id = UINT32_C(287464101);
    m.session_id = UINT64_C(578437695752307201);
    m.domain_boot_id = UINT32_C(287454020);
    m.query_id = UINT32_C(1719109785);
    m.original_can_boot_id = UINT32_C(2712847316);
    m.original_can_tx_sequence = UINT32_C(270544960);
    m.cis_state = UINT32_C(2);
    m.vision_status = UINT32_C(2);
    m.temperature_status = UINT32_C(2);
    m.humidity_status = UINT32_C(1);
    m.illuminance_status = UINT32_C(0);
    m.rear_status = UINT32_C(4);
    m.pi_uart_status = UINT32_C(1);
    m.can_interface_status = UINT32_C(2);
    m.fault_mask = UINT32_C(256);
    m.active_fault = UINT32_C(9);
    m.recovering = UINT32_C(1);
    m.last_fault = UINT32_C(7);
    m.last_fault_occurrence = UINT32_C(2271560481);
    m.affected_function = UINT32_C(63);
    m.update_sequence = UINT32_C(2882400255);
    m.source_age_ms = UINT32_C(65535);
    return m;
}
static int equal_m_cis_status(const NetWireMCisStatusRaw *a, const NetWireMCisStatusRaw *b)
{
    return a->device_context_id == b->device_context_id &&
        a->session_id == b->session_id &&
        a->domain_boot_id == b->domain_boot_id &&
        a->query_id == b->query_id &&
        a->original_can_boot_id == b->original_can_boot_id &&
        a->original_can_tx_sequence == b->original_can_tx_sequence &&
        a->cis_state == b->cis_state &&
        a->vision_status == b->vision_status &&
        a->temperature_status == b->temperature_status &&
        a->humidity_status == b->humidity_status &&
        a->illuminance_status == b->illuminance_status &&
        a->rear_status == b->rear_status &&
        a->pi_uart_status == b->pi_uart_status &&
        a->can_interface_status == b->can_interface_status &&
        a->fault_mask == b->fault_mask &&
        a->active_fault == b->active_fault &&
        a->recovering == b->recovering &&
        a->last_fault == b->last_fault &&
        a->last_fault_occurrence == b->last_fault_occurrence &&
        a->affected_function == b->affected_function &&
        a->update_sequence == b->update_sequence &&
        a->source_age_ms == b->source_age_ms;
}
static NetWireStatus encode_m_cis_status(NetWireProfile p, uint16_t s, const void *m, NetWireUartFrame *f)
{
    return net_wire_m_cis_status_encode(p, s, (const NetWireMCisStatusRaw *)m, f);
}
static NetWireStatus decode_m_cis_status(NetWireProfile p, const NetWireUartFrame *f, void *m, uint16_t *s)
{
    return net_wire_m_cis_status_decode(p, f, (NetWireMCisStatusRaw *)m, s);
}
static int test_m_cis_status(void)
{
    NetWireMCisStatusRaw m = example_m_cis_status(), output;
    NetWireUartFrame expected = literal_frame(golden_m_cis_status, sizeof(golden_m_cis_status)), actual;
    uint16_t sequence = 0u;
    CHECK(net_wire_m_cis_status_encode(CANDIDATE, 0x5aa5u, &m, &actual) == NET_WIRE_OK);
    CHECK(actual.length == expected.length && memcmp(actual.bytes, expected.bytes, expected.length) == 0);
    CHECK(net_wire_m_cis_status_decode(CANDIDATE, &expected, &output, &sequence) == NET_WIRE_OK);
    CHECK(sequence == 0x5aa5u && equal_m_cis_status(&m, &output));
    CHECK(common_errors(encode_m_cis_status, decode_m_cis_status, &m, sizeof(m), golden_m_cis_status, sizeof(golden_m_cis_status)) == 0);
    expected = literal_frame(golden_m_cis_status, sizeof(golden_m_cis_status));
    expected.bytes[6] = 0u; expected.bytes[7] = 0u; repair_crc(&expected);
    CHECK(net_wire_m_cis_status_encode(CANDIDATE, 0u, &m, &actual) == NET_WIRE_OK);
    CHECK(actual.length == expected.length && memcmp(actual.bytes, expected.bytes, expected.length) == 0);
    CHECK(net_wire_m_cis_status_decode(CANDIDATE, &expected, &output, &sequence) == NET_WIRE_OK);
    CHECK(sequence == 0u && equal_m_cis_status(&m, &output));
    expected = literal_frame(golden_m_cis_status, sizeof(golden_m_cis_status));
    expected.bytes[6] = 255u; expected.bytes[7] = 255u; repair_crc(&expected);
    CHECK(net_wire_m_cis_status_encode(CANDIDATE, 65535u, &m, &actual) == NET_WIRE_OK);
    CHECK(actual.length == expected.length && memcmp(actual.bytes, expected.bytes, expected.length) == 0);
    CHECK(net_wire_m_cis_status_decode(CANDIDATE, &expected, &output, &sequence) == NET_WIRE_OK);
    CHECK(sequence == 65535u && equal_m_cis_status(&m, &output));
    m = example_m_cis_status();
    expected = literal_frame(golden_m_cis_status, sizeof(golden_m_cis_status));
    m.device_context_id = UINT32_C(0);
    patch_field(&expected, 0u, 4u, UINT64_C(0));
    CHECK(rejected(encode_m_cis_status, decode_m_cis_status, &m, sizeof(m), &expected, NET_WIRE_MALFORMED) == 0);
    m = example_m_cis_status();
    expected = literal_frame(golden_m_cis_status, sizeof(golden_m_cis_status));
    m.domain_boot_id = UINT32_C(0);
    patch_field(&expected, 12u, 4u, UINT64_C(0));
    CHECK(rejected(encode_m_cis_status, decode_m_cis_status, &m, sizeof(m), &expected, NET_WIRE_MALFORMED) == 0);
    m = example_m_cis_status();
    expected = literal_frame(golden_m_cis_status, sizeof(golden_m_cis_status));
    m.original_can_boot_id = UINT32_C(0);
    patch_field(&expected, 20u, 4u, UINT64_C(0));
    CHECK(rejected(encode_m_cis_status, decode_m_cis_status, &m, sizeof(m), &expected, NET_WIRE_MALFORMED) == 0);
    m = example_m_cis_status();
    expected = literal_frame(golden_m_cis_status, sizeof(golden_m_cis_status));
    m.cis_state = UINT32_C(4);
    patch_field(&expected, 28u, 1u, UINT64_C(4));
    CHECK(rejected(encode_m_cis_status, decode_m_cis_status, &m, sizeof(m), &expected, NET_WIRE_MALFORMED) == 0);
    m = example_m_cis_status();
    expected = literal_frame(golden_m_cis_status, sizeof(golden_m_cis_status));
    m.vision_status = UINT32_C(5);
    patch_field(&expected, 29u, 1u, UINT64_C(5));
    CHECK(rejected(encode_m_cis_status, decode_m_cis_status, &m, sizeof(m), &expected, NET_WIRE_MALFORMED) == 0);
    m = example_m_cis_status();
    expected = literal_frame(golden_m_cis_status, sizeof(golden_m_cis_status));
    m.temperature_status = UINT32_C(5);
    patch_field(&expected, 30u, 1u, UINT64_C(5));
    CHECK(rejected(encode_m_cis_status, decode_m_cis_status, &m, sizeof(m), &expected, NET_WIRE_MALFORMED) == 0);
    m = example_m_cis_status();
    expected = literal_frame(golden_m_cis_status, sizeof(golden_m_cis_status));
    m.humidity_status = UINT32_C(5);
    patch_field(&expected, 31u, 1u, UINT64_C(5));
    CHECK(rejected(encode_m_cis_status, decode_m_cis_status, &m, sizeof(m), &expected, NET_WIRE_MALFORMED) == 0);
    m = example_m_cis_status();
    expected = literal_frame(golden_m_cis_status, sizeof(golden_m_cis_status));
    m.illuminance_status = UINT32_C(5);
    patch_field(&expected, 32u, 1u, UINT64_C(5));
    CHECK(rejected(encode_m_cis_status, decode_m_cis_status, &m, sizeof(m), &expected, NET_WIRE_MALFORMED) == 0);
    m = example_m_cis_status();
    expected = literal_frame(golden_m_cis_status, sizeof(golden_m_cis_status));
    m.rear_status = UINT32_C(5);
    patch_field(&expected, 33u, 1u, UINT64_C(5));
    CHECK(rejected(encode_m_cis_status, decode_m_cis_status, &m, sizeof(m), &expected, NET_WIRE_MALFORMED) == 0);
    m = example_m_cis_status();
    expected = literal_frame(golden_m_cis_status, sizeof(golden_m_cis_status));
    m.pi_uart_status = UINT32_C(3);
    patch_field(&expected, 34u, 1u, UINT64_C(3));
    CHECK(rejected(encode_m_cis_status, decode_m_cis_status, &m, sizeof(m), &expected, NET_WIRE_MALFORMED) == 0);
    m = example_m_cis_status();
    expected = literal_frame(golden_m_cis_status, sizeof(golden_m_cis_status));
    m.can_interface_status = UINT32_C(3);
    patch_field(&expected, 35u, 1u, UINT64_C(3));
    CHECK(rejected(encode_m_cis_status, decode_m_cis_status, &m, sizeof(m), &expected, NET_WIRE_MALFORMED) == 0);
    m = example_m_cis_status();
    expected = literal_frame(golden_m_cis_status, sizeof(golden_m_cis_status));
    m.fault_mask = UINT32_C(512);
    patch_field(&expected, 36u, 2u, UINT64_C(512));
    CHECK(rejected(encode_m_cis_status, decode_m_cis_status, &m, sizeof(m), &expected, NET_WIRE_MALFORMED) == 0);
    m = example_m_cis_status();
    expected = literal_frame(golden_m_cis_status, sizeof(golden_m_cis_status));
    m.active_fault = UINT32_C(10);
    patch_field(&expected, 38u, 2u, UINT64_C(10));
    CHECK(rejected(encode_m_cis_status, decode_m_cis_status, &m, sizeof(m), &expected, NET_WIRE_MALFORMED) == 0);
    m = example_m_cis_status();
    expected = literal_frame(golden_m_cis_status, sizeof(golden_m_cis_status));
    m.recovering = UINT32_C(2);
    patch_field(&expected, 40u, 1u, UINT64_C(2));
    CHECK(rejected(encode_m_cis_status, decode_m_cis_status, &m, sizeof(m), &expected, NET_WIRE_MALFORMED) == 0);
    m = example_m_cis_status();
    expected = literal_frame(golden_m_cis_status, sizeof(golden_m_cis_status));
    m.last_fault = UINT32_C(10);
    patch_field(&expected, 41u, 2u, UINT64_C(10));
    CHECK(rejected(encode_m_cis_status, decode_m_cis_status, &m, sizeof(m), &expected, NET_WIRE_MALFORMED) == 0);
    m = example_m_cis_status();
    expected = literal_frame(golden_m_cis_status, sizeof(golden_m_cis_status));
    m.affected_function = UINT32_C(64);
    patch_field(&expected, 47u, 1u, UINT64_C(64));
    CHECK(rejected(encode_m_cis_status, decode_m_cis_status, &m, sizeof(m), &expected, NET_WIRE_MALFORMED) == 0);
    m = example_m_cis_status();
    expected = literal_frame(golden_m_cis_status, sizeof(golden_m_cis_status));
    m.fault_mask = UINT32_C(511);
    patch_field(&expected, 36u, 2u, UINT64_C(511));
    m.active_fault = UINT32_C(9);
    patch_field(&expected, 38u, 2u, UINT64_C(9));
    m.last_fault = UINT32_C(9);
    patch_field(&expected, 41u, 2u, UINT64_C(9));
    CHECK(net_wire_m_cis_status_encode(CANDIDATE, 0x5aa5u, &m, &actual) == NET_WIRE_OK);
    CHECK(actual.length == expected.length && memcmp(actual.bytes, expected.bytes, expected.length) == 0);
    CHECK(net_wire_m_cis_status_decode(CANDIDATE, &expected, &output, &sequence) == NET_WIRE_OK);
    CHECK(sequence == 0x5aa5u && equal_m_cis_status(&m, &output));
    m = example_m_cis_status();
    expected = literal_frame(golden_m_cis_status, sizeof(golden_m_cis_status));
    m.fault_mask = UINT32_C(0);
    patch_field(&expected, 36u, 2u, UINT64_C(0));
    m.active_fault = UINT32_C(0);
    patch_field(&expected, 38u, 2u, UINT64_C(0));
    m.recovering = UINT32_C(0);
    patch_field(&expected, 40u, 1u, UINT64_C(0));
    m.last_fault = UINT32_C(7);
    patch_field(&expected, 41u, 2u, UINT64_C(7));
    m.last_fault_occurrence = UINT32_C(4294967295);
    patch_field(&expected, 43u, 4u, UINT64_C(4294967295));
    CHECK(net_wire_m_cis_status_encode(CANDIDATE, 0x5aa5u, &m, &actual) == NET_WIRE_OK);
    CHECK(actual.length == expected.length && memcmp(actual.bytes, expected.bytes, expected.length) == 0);
    CHECK(net_wire_m_cis_status_decode(CANDIDATE, &expected, &output, &sequence) == NET_WIRE_OK);
    CHECK(sequence == 0x5aa5u && equal_m_cis_status(&m, &output));
    m = example_m_cis_status();
    expected = literal_frame(golden_m_cis_status, sizeof(golden_m_cis_status));
    m.device_context_id = UINT32_C(1);
    patch_field(&expected, 0u, 4u, UINT64_C(1));
    CHECK(net_wire_m_cis_status_encode(CANDIDATE, 0x5aa5u, &m, &actual) == NET_WIRE_OK);
    CHECK(actual.length == expected.length && memcmp(actual.bytes, expected.bytes, expected.length) == 0);
    CHECK(net_wire_m_cis_status_decode(CANDIDATE, &expected, &output, &sequence) == NET_WIRE_OK);
    CHECK(sequence == 0x5aa5u && equal_m_cis_status(&m, &output));
    m = example_m_cis_status();
    expected = literal_frame(golden_m_cis_status, sizeof(golden_m_cis_status));
    m.device_context_id = UINT32_C(4294967295);
    patch_field(&expected, 0u, 4u, UINT64_C(4294967295));
    CHECK(net_wire_m_cis_status_encode(CANDIDATE, 0x5aa5u, &m, &actual) == NET_WIRE_OK);
    CHECK(actual.length == expected.length && memcmp(actual.bytes, expected.bytes, expected.length) == 0);
    CHECK(net_wire_m_cis_status_decode(CANDIDATE, &expected, &output, &sequence) == NET_WIRE_OK);
    CHECK(sequence == 0x5aa5u && equal_m_cis_status(&m, &output));
    m = example_m_cis_status();
    expected = literal_frame(golden_m_cis_status, sizeof(golden_m_cis_status));
    m.domain_boot_id = UINT32_C(1);
    patch_field(&expected, 12u, 4u, UINT64_C(1));
    CHECK(net_wire_m_cis_status_encode(CANDIDATE, 0x5aa5u, &m, &actual) == NET_WIRE_OK);
    CHECK(actual.length == expected.length && memcmp(actual.bytes, expected.bytes, expected.length) == 0);
    CHECK(net_wire_m_cis_status_decode(CANDIDATE, &expected, &output, &sequence) == NET_WIRE_OK);
    CHECK(sequence == 0x5aa5u && equal_m_cis_status(&m, &output));
    m = example_m_cis_status();
    expected = literal_frame(golden_m_cis_status, sizeof(golden_m_cis_status));
    m.domain_boot_id = UINT32_C(4294967295);
    patch_field(&expected, 12u, 4u, UINT64_C(4294967295));
    CHECK(net_wire_m_cis_status_encode(CANDIDATE, 0x5aa5u, &m, &actual) == NET_WIRE_OK);
    CHECK(actual.length == expected.length && memcmp(actual.bytes, expected.bytes, expected.length) == 0);
    CHECK(net_wire_m_cis_status_decode(CANDIDATE, &expected, &output, &sequence) == NET_WIRE_OK);
    CHECK(sequence == 0x5aa5u && equal_m_cis_status(&m, &output));
    m = example_m_cis_status();
    expected = literal_frame(golden_m_cis_status, sizeof(golden_m_cis_status));
    m.original_can_boot_id = UINT32_C(1);
    patch_field(&expected, 20u, 4u, UINT64_C(1));
    CHECK(net_wire_m_cis_status_encode(CANDIDATE, 0x5aa5u, &m, &actual) == NET_WIRE_OK);
    CHECK(actual.length == expected.length && memcmp(actual.bytes, expected.bytes, expected.length) == 0);
    CHECK(net_wire_m_cis_status_decode(CANDIDATE, &expected, &output, &sequence) == NET_WIRE_OK);
    CHECK(sequence == 0x5aa5u && equal_m_cis_status(&m, &output));
    m = example_m_cis_status();
    expected = literal_frame(golden_m_cis_status, sizeof(golden_m_cis_status));
    m.original_can_boot_id = UINT32_C(4294967295);
    patch_field(&expected, 20u, 4u, UINT64_C(4294967295));
    CHECK(net_wire_m_cis_status_encode(CANDIDATE, 0x5aa5u, &m, &actual) == NET_WIRE_OK);
    CHECK(actual.length == expected.length && memcmp(actual.bytes, expected.bytes, expected.length) == 0);
    CHECK(net_wire_m_cis_status_decode(CANDIDATE, &expected, &output, &sequence) == NET_WIRE_OK);
    CHECK(sequence == 0x5aa5u && equal_m_cis_status(&m, &output));
    m = example_m_cis_status();
    expected = literal_frame(golden_m_cis_status, sizeof(golden_m_cis_status));
    m.session_id = UINT64_C(0);
    patch_field(&expected, 4u, 8u, UINT64_C(0));
    CHECK(net_wire_m_cis_status_encode(CANDIDATE, 0x5aa5u, &m, &actual) == NET_WIRE_OK);
    CHECK(actual.length == expected.length && memcmp(actual.bytes, expected.bytes, expected.length) == 0);
    CHECK(net_wire_m_cis_status_decode(CANDIDATE, &expected, &output, &sequence) == NET_WIRE_OK);
    CHECK(sequence == 0x5aa5u && equal_m_cis_status(&m, &output));
    m = example_m_cis_status();
    expected = literal_frame(golden_m_cis_status, sizeof(golden_m_cis_status));
    m.session_id = UINT64_C(18446744073709551615);
    patch_field(&expected, 4u, 8u, UINT64_C(18446744073709551615));
    CHECK(net_wire_m_cis_status_encode(CANDIDATE, 0x5aa5u, &m, &actual) == NET_WIRE_OK);
    CHECK(actual.length == expected.length && memcmp(actual.bytes, expected.bytes, expected.length) == 0);
    CHECK(net_wire_m_cis_status_decode(CANDIDATE, &expected, &output, &sequence) == NET_WIRE_OK);
    CHECK(sequence == 0x5aa5u && equal_m_cis_status(&m, &output));
    m = example_m_cis_status();
    expected = literal_frame(golden_m_cis_status, sizeof(golden_m_cis_status));
    m.query_id = UINT32_C(0);
    patch_field(&expected, 16u, 4u, UINT64_C(0));
    CHECK(net_wire_m_cis_status_encode(CANDIDATE, 0x5aa5u, &m, &actual) == NET_WIRE_OK);
    CHECK(actual.length == expected.length && memcmp(actual.bytes, expected.bytes, expected.length) == 0);
    CHECK(net_wire_m_cis_status_decode(CANDIDATE, &expected, &output, &sequence) == NET_WIRE_OK);
    CHECK(sequence == 0x5aa5u && equal_m_cis_status(&m, &output));
    m = example_m_cis_status();
    expected = literal_frame(golden_m_cis_status, sizeof(golden_m_cis_status));
    m.query_id = UINT32_C(4294967295);
    patch_field(&expected, 16u, 4u, UINT64_C(4294967295));
    CHECK(net_wire_m_cis_status_encode(CANDIDATE, 0x5aa5u, &m, &actual) == NET_WIRE_OK);
    CHECK(actual.length == expected.length && memcmp(actual.bytes, expected.bytes, expected.length) == 0);
    CHECK(net_wire_m_cis_status_decode(CANDIDATE, &expected, &output, &sequence) == NET_WIRE_OK);
    CHECK(sequence == 0x5aa5u && equal_m_cis_status(&m, &output));
    m = example_m_cis_status();
    expected = literal_frame(golden_m_cis_status, sizeof(golden_m_cis_status));
    m.original_can_tx_sequence = UINT32_C(0);
    patch_field(&expected, 24u, 4u, UINT64_C(0));
    CHECK(net_wire_m_cis_status_encode(CANDIDATE, 0x5aa5u, &m, &actual) == NET_WIRE_OK);
    CHECK(actual.length == expected.length && memcmp(actual.bytes, expected.bytes, expected.length) == 0);
    CHECK(net_wire_m_cis_status_decode(CANDIDATE, &expected, &output, &sequence) == NET_WIRE_OK);
    CHECK(sequence == 0x5aa5u && equal_m_cis_status(&m, &output));
    m = example_m_cis_status();
    expected = literal_frame(golden_m_cis_status, sizeof(golden_m_cis_status));
    m.original_can_tx_sequence = UINT32_C(4294967295);
    patch_field(&expected, 24u, 4u, UINT64_C(4294967295));
    CHECK(net_wire_m_cis_status_encode(CANDIDATE, 0x5aa5u, &m, &actual) == NET_WIRE_OK);
    CHECK(actual.length == expected.length && memcmp(actual.bytes, expected.bytes, expected.length) == 0);
    CHECK(net_wire_m_cis_status_decode(CANDIDATE, &expected, &output, &sequence) == NET_WIRE_OK);
    CHECK(sequence == 0x5aa5u && equal_m_cis_status(&m, &output));
    m = example_m_cis_status();
    expected = literal_frame(golden_m_cis_status, sizeof(golden_m_cis_status));
    m.update_sequence = UINT32_C(0);
    patch_field(&expected, 48u, 4u, UINT64_C(0));
    CHECK(net_wire_m_cis_status_encode(CANDIDATE, 0x5aa5u, &m, &actual) == NET_WIRE_OK);
    CHECK(actual.length == expected.length && memcmp(actual.bytes, expected.bytes, expected.length) == 0);
    CHECK(net_wire_m_cis_status_decode(CANDIDATE, &expected, &output, &sequence) == NET_WIRE_OK);
    CHECK(sequence == 0x5aa5u && equal_m_cis_status(&m, &output));
    m = example_m_cis_status();
    expected = literal_frame(golden_m_cis_status, sizeof(golden_m_cis_status));
    m.update_sequence = UINT32_C(4294967295);
    patch_field(&expected, 48u, 4u, UINT64_C(4294967295));
    CHECK(net_wire_m_cis_status_encode(CANDIDATE, 0x5aa5u, &m, &actual) == NET_WIRE_OK);
    CHECK(actual.length == expected.length && memcmp(actual.bytes, expected.bytes, expected.length) == 0);
    CHECK(net_wire_m_cis_status_decode(CANDIDATE, &expected, &output, &sequence) == NET_WIRE_OK);
    CHECK(sequence == 0x5aa5u && equal_m_cis_status(&m, &output));
    m = example_m_cis_status();
    expected = literal_frame(golden_m_cis_status, sizeof(golden_m_cis_status));
    m.source_age_ms = UINT32_C(0);
    patch_field(&expected, 52u, 2u, UINT64_C(0));
    CHECK(net_wire_m_cis_status_encode(CANDIDATE, 0x5aa5u, &m, &actual) == NET_WIRE_OK);
    CHECK(actual.length == expected.length && memcmp(actual.bytes, expected.bytes, expected.length) == 0);
    CHECK(net_wire_m_cis_status_decode(CANDIDATE, &expected, &output, &sequence) == NET_WIRE_OK);
    CHECK(sequence == 0x5aa5u && equal_m_cis_status(&m, &output));
    m = example_m_cis_status();
    expected = literal_frame(golden_m_cis_status, sizeof(golden_m_cis_status));
    m.source_age_ms = UINT32_C(65535);
    patch_field(&expected, 52u, 2u, UINT64_C(65535));
    CHECK(net_wire_m_cis_status_encode(CANDIDATE, 0x5aa5u, &m, &actual) == NET_WIRE_OK);
    CHECK(actual.length == expected.length && memcmp(actual.bytes, expected.bytes, expected.length) == 0);
    CHECK(net_wire_m_cis_status_decode(CANDIDATE, &expected, &output, &sequence) == NET_WIRE_OK);
    CHECK(sequence == 0x5aa5u && equal_m_cis_status(&m, &output));
    return 0;
}

/* FINAL_98/docs/network/interface/03_COMMUNICATION/20_UART_M.md:584; independent Python struct/CRC literal. */
static const uint8_t golden_m_vss_status[58] = {
    0xa5,0x5a,0x02,0x3a,0x30,0x00,0xa5,0x5a,0xa5,0x5a,0x22,0x11,0x01,0x02,0x03,0x04,
    0x05,0x06,0x07,0x08,0x44,0x33,0x22,0x11,0x99,0x88,0x77,0x66,0xd4,0xc3,0xb2,0xa1,
    0x40,0x30,0x20,0x10,0x01,0x01,0x01,0x00,0x00,0x03,0x00,0x32,0x54,0x76,0x98,0xff,
    0xff,0x00,0x40,0x30,0x20,0x10,0xd2,0x04,0x72,0x8a,
};
static NetWireMVssStatusRaw example_m_vss_status(void)
{
    NetWireMVssStatusRaw m = {0};
    m.device_context_id = UINT32_C(287464101);
    m.session_id = UINT64_C(578437695752307201);
    m.domain_boot_id = UINT32_C(287454020);
    m.query_id = UINT32_C(1719109785);
    m.original_can_boot_id = UINT32_C(2712847316);
    m.original_can_tx_sequence = UINT32_C(270544960);
    m.vss_state = UINT32_C(1);
    m.vss_availability = UINT32_C(1);
    m.vss_accepting_events = UINT32_C(1);
    m.vss_fault_active = UINT32_C(0);
    m.fault_mask = UINT32_C(0);
    m.vss_last_fault = UINT32_C(3);
    m.last_fault_occurrence = UINT32_C(2557891634);
    m.last_fault_age_ms = UINT32_C(65535);
    m.last_fault_quality = UINT32_C(0);
    m.update_sequence = UINT32_C(270544960);
    m.source_age_ms = UINT32_C(1234);
    return m;
}
static int equal_m_vss_status(const NetWireMVssStatusRaw *a, const NetWireMVssStatusRaw *b)
{
    return a->device_context_id == b->device_context_id &&
        a->session_id == b->session_id &&
        a->domain_boot_id == b->domain_boot_id &&
        a->query_id == b->query_id &&
        a->original_can_boot_id == b->original_can_boot_id &&
        a->original_can_tx_sequence == b->original_can_tx_sequence &&
        a->vss_state == b->vss_state &&
        a->vss_availability == b->vss_availability &&
        a->vss_accepting_events == b->vss_accepting_events &&
        a->vss_fault_active == b->vss_fault_active &&
        a->fault_mask == b->fault_mask &&
        a->vss_last_fault == b->vss_last_fault &&
        a->last_fault_occurrence == b->last_fault_occurrence &&
        a->last_fault_age_ms == b->last_fault_age_ms &&
        a->last_fault_quality == b->last_fault_quality &&
        a->update_sequence == b->update_sequence &&
        a->source_age_ms == b->source_age_ms;
}
static NetWireStatus encode_m_vss_status(NetWireProfile p, uint16_t s, const void *m, NetWireUartFrame *f)
{
    return net_wire_m_vss_status_encode(p, s, (const NetWireMVssStatusRaw *)m, f);
}
static NetWireStatus decode_m_vss_status(NetWireProfile p, const NetWireUartFrame *f, void *m, uint16_t *s)
{
    return net_wire_m_vss_status_decode(p, f, (NetWireMVssStatusRaw *)m, s);
}
static int test_m_vss_status(void)
{
    NetWireMVssStatusRaw m = example_m_vss_status(), output;
    NetWireUartFrame expected = literal_frame(golden_m_vss_status, sizeof(golden_m_vss_status)), actual;
    uint16_t sequence = 0u;
    CHECK(net_wire_m_vss_status_encode(CANDIDATE, 0x5aa5u, &m, &actual) == NET_WIRE_OK);
    CHECK(actual.length == expected.length && memcmp(actual.bytes, expected.bytes, expected.length) == 0);
    CHECK(net_wire_m_vss_status_decode(CANDIDATE, &expected, &output, &sequence) == NET_WIRE_OK);
    CHECK(sequence == 0x5aa5u && equal_m_vss_status(&m, &output));
    CHECK(common_errors(encode_m_vss_status, decode_m_vss_status, &m, sizeof(m), golden_m_vss_status, sizeof(golden_m_vss_status)) == 0);
    expected = literal_frame(golden_m_vss_status, sizeof(golden_m_vss_status));
    expected.bytes[6] = 0u; expected.bytes[7] = 0u; repair_crc(&expected);
    CHECK(net_wire_m_vss_status_encode(CANDIDATE, 0u, &m, &actual) == NET_WIRE_OK);
    CHECK(actual.length == expected.length && memcmp(actual.bytes, expected.bytes, expected.length) == 0);
    CHECK(net_wire_m_vss_status_decode(CANDIDATE, &expected, &output, &sequence) == NET_WIRE_OK);
    CHECK(sequence == 0u && equal_m_vss_status(&m, &output));
    expected = literal_frame(golden_m_vss_status, sizeof(golden_m_vss_status));
    expected.bytes[6] = 255u; expected.bytes[7] = 255u; repair_crc(&expected);
    CHECK(net_wire_m_vss_status_encode(CANDIDATE, 65535u, &m, &actual) == NET_WIRE_OK);
    CHECK(actual.length == expected.length && memcmp(actual.bytes, expected.bytes, expected.length) == 0);
    CHECK(net_wire_m_vss_status_decode(CANDIDATE, &expected, &output, &sequence) == NET_WIRE_OK);
    CHECK(sequence == 65535u && equal_m_vss_status(&m, &output));
    m = example_m_vss_status();
    expected = literal_frame(golden_m_vss_status, sizeof(golden_m_vss_status));
    m.device_context_id = UINT32_C(0);
    patch_field(&expected, 0u, 4u, UINT64_C(0));
    CHECK(rejected(encode_m_vss_status, decode_m_vss_status, &m, sizeof(m), &expected, NET_WIRE_MALFORMED) == 0);
    m = example_m_vss_status();
    expected = literal_frame(golden_m_vss_status, sizeof(golden_m_vss_status));
    m.domain_boot_id = UINT32_C(0);
    patch_field(&expected, 12u, 4u, UINT64_C(0));
    CHECK(rejected(encode_m_vss_status, decode_m_vss_status, &m, sizeof(m), &expected, NET_WIRE_MALFORMED) == 0);
    m = example_m_vss_status();
    expected = literal_frame(golden_m_vss_status, sizeof(golden_m_vss_status));
    m.original_can_boot_id = UINT32_C(0);
    patch_field(&expected, 20u, 4u, UINT64_C(0));
    CHECK(rejected(encode_m_vss_status, decode_m_vss_status, &m, sizeof(m), &expected, NET_WIRE_MALFORMED) == 0);
    m = example_m_vss_status();
    expected = literal_frame(golden_m_vss_status, sizeof(golden_m_vss_status));
    m.vss_state = UINT32_C(4);
    patch_field(&expected, 28u, 1u, UINT64_C(4));
    CHECK(rejected(encode_m_vss_status, decode_m_vss_status, &m, sizeof(m), &expected, NET_WIRE_MALFORMED) == 0);
    m = example_m_vss_status();
    expected = literal_frame(golden_m_vss_status, sizeof(golden_m_vss_status));
    m.vss_availability = UINT32_C(3);
    patch_field(&expected, 29u, 1u, UINT64_C(3));
    CHECK(rejected(encode_m_vss_status, decode_m_vss_status, &m, sizeof(m), &expected, NET_WIRE_MALFORMED) == 0);
    m = example_m_vss_status();
    expected = literal_frame(golden_m_vss_status, sizeof(golden_m_vss_status));
    m.vss_accepting_events = UINT32_C(2);
    patch_field(&expected, 30u, 1u, UINT64_C(2));
    CHECK(rejected(encode_m_vss_status, decode_m_vss_status, &m, sizeof(m), &expected, NET_WIRE_MALFORMED) == 0);
    m = example_m_vss_status();
    expected = literal_frame(golden_m_vss_status, sizeof(golden_m_vss_status));
    m.vss_fault_active = UINT32_C(2);
    patch_field(&expected, 31u, 1u, UINT64_C(2));
    CHECK(rejected(encode_m_vss_status, decode_m_vss_status, &m, sizeof(m), &expected, NET_WIRE_MALFORMED) == 0);
    m = example_m_vss_status();
    expected = literal_frame(golden_m_vss_status, sizeof(golden_m_vss_status));
    m.fault_mask = UINT32_C(32);
    patch_field(&expected, 32u, 1u, UINT64_C(32));
    CHECK(rejected(encode_m_vss_status, decode_m_vss_status, &m, sizeof(m), &expected, NET_WIRE_MALFORMED) == 0);
    m = example_m_vss_status();
    expected = literal_frame(golden_m_vss_status, sizeof(golden_m_vss_status));
    m.vss_last_fault = UINT32_C(6);
    patch_field(&expected, 33u, 2u, UINT64_C(6));
    CHECK(rejected(encode_m_vss_status, decode_m_vss_status, &m, sizeof(m), &expected, NET_WIRE_MALFORMED) == 0);
    m = example_m_vss_status();
    expected = literal_frame(golden_m_vss_status, sizeof(golden_m_vss_status));
    m.last_fault_quality = UINT32_C(2);
    patch_field(&expected, 41u, 1u, UINT64_C(2));
    CHECK(rejected(encode_m_vss_status, decode_m_vss_status, &m, sizeof(m), &expected, NET_WIRE_MALFORMED) == 0);
    m = example_m_vss_status();
    expected = literal_frame(golden_m_vss_status, sizeof(golden_m_vss_status));
    m.fault_mask = UINT32_C(31);
    patch_field(&expected, 32u, 1u, UINT64_C(31));
    m.vss_fault_active = UINT32_C(1);
    patch_field(&expected, 31u, 1u, UINT64_C(1));
    CHECK(net_wire_m_vss_status_encode(CANDIDATE, 0x5aa5u, &m, &actual) == NET_WIRE_OK);
    CHECK(actual.length == expected.length && memcmp(actual.bytes, expected.bytes, expected.length) == 0);
    CHECK(net_wire_m_vss_status_decode(CANDIDATE, &expected, &output, &sequence) == NET_WIRE_OK);
    CHECK(sequence == 0x5aa5u && equal_m_vss_status(&m, &output));
    m = example_m_vss_status();
    expected = literal_frame(golden_m_vss_status, sizeof(golden_m_vss_status));
    m.vss_last_fault = UINT32_C(0);
    patch_field(&expected, 33u, 2u, UINT64_C(0));
    m.last_fault_occurrence = UINT32_C(0);
    patch_field(&expected, 35u, 4u, UINT64_C(0));
    m.last_fault_age_ms = UINT32_C(0);
    patch_field(&expected, 39u, 2u, UINT64_C(0));
    m.last_fault_quality = UINT32_C(3);
    patch_field(&expected, 41u, 1u, UINT64_C(3));
    CHECK(net_wire_m_vss_status_encode(CANDIDATE, 0x5aa5u, &m, &actual) == NET_WIRE_OK);
    CHECK(actual.length == expected.length && memcmp(actual.bytes, expected.bytes, expected.length) == 0);
    CHECK(net_wire_m_vss_status_decode(CANDIDATE, &expected, &output, &sequence) == NET_WIRE_OK);
    CHECK(sequence == 0x5aa5u && equal_m_vss_status(&m, &output));
    m = example_m_vss_status();
    expected = literal_frame(golden_m_vss_status, sizeof(golden_m_vss_status));
    m.last_fault_quality = UINT32_C(3);
    patch_field(&expected, 41u, 1u, UINT64_C(3));
    CHECK(rejected(encode_m_vss_status, decode_m_vss_status, &m, sizeof(m), &expected, NET_WIRE_MALFORMED) == 0);
    m = example_m_vss_status();
    expected = literal_frame(golden_m_vss_status, sizeof(golden_m_vss_status));
    m.vss_last_fault = UINT32_C(0);
    patch_field(&expected, 33u, 2u, UINT64_C(0));
    CHECK(rejected(encode_m_vss_status, decode_m_vss_status, &m, sizeof(m), &expected, NET_WIRE_CONTRACT_HOLD) == 0);
    m = example_m_vss_status();
    expected = literal_frame(golden_m_vss_status, sizeof(golden_m_vss_status));
    m.fault_mask = UINT32_C(0);
    patch_field(&expected, 32u, 1u, UINT64_C(0));
    m.vss_fault_active = UINT32_C(0);
    patch_field(&expected, 31u, 1u, UINT64_C(0));
    m.vss_last_fault = UINT32_C(5);
    patch_field(&expected, 33u, 2u, UINT64_C(5));
    m.last_fault_age_ms = UINT32_C(65535);
    patch_field(&expected, 39u, 2u, UINT64_C(65535));
    CHECK(net_wire_m_vss_status_encode(CANDIDATE, 0x5aa5u, &m, &actual) == NET_WIRE_OK);
    CHECK(actual.length == expected.length && memcmp(actual.bytes, expected.bytes, expected.length) == 0);
    CHECK(net_wire_m_vss_status_decode(CANDIDATE, &expected, &output, &sequence) == NET_WIRE_OK);
    CHECK(sequence == 0x5aa5u && equal_m_vss_status(&m, &output));
    m = example_m_vss_status();
    expected = literal_frame(golden_m_vss_status, sizeof(golden_m_vss_status));
    m.device_context_id = UINT32_C(1);
    patch_field(&expected, 0u, 4u, UINT64_C(1));
    CHECK(net_wire_m_vss_status_encode(CANDIDATE, 0x5aa5u, &m, &actual) == NET_WIRE_OK);
    CHECK(actual.length == expected.length && memcmp(actual.bytes, expected.bytes, expected.length) == 0);
    CHECK(net_wire_m_vss_status_decode(CANDIDATE, &expected, &output, &sequence) == NET_WIRE_OK);
    CHECK(sequence == 0x5aa5u && equal_m_vss_status(&m, &output));
    m = example_m_vss_status();
    expected = literal_frame(golden_m_vss_status, sizeof(golden_m_vss_status));
    m.device_context_id = UINT32_C(4294967295);
    patch_field(&expected, 0u, 4u, UINT64_C(4294967295));
    CHECK(net_wire_m_vss_status_encode(CANDIDATE, 0x5aa5u, &m, &actual) == NET_WIRE_OK);
    CHECK(actual.length == expected.length && memcmp(actual.bytes, expected.bytes, expected.length) == 0);
    CHECK(net_wire_m_vss_status_decode(CANDIDATE, &expected, &output, &sequence) == NET_WIRE_OK);
    CHECK(sequence == 0x5aa5u && equal_m_vss_status(&m, &output));
    m = example_m_vss_status();
    expected = literal_frame(golden_m_vss_status, sizeof(golden_m_vss_status));
    m.domain_boot_id = UINT32_C(1);
    patch_field(&expected, 12u, 4u, UINT64_C(1));
    CHECK(net_wire_m_vss_status_encode(CANDIDATE, 0x5aa5u, &m, &actual) == NET_WIRE_OK);
    CHECK(actual.length == expected.length && memcmp(actual.bytes, expected.bytes, expected.length) == 0);
    CHECK(net_wire_m_vss_status_decode(CANDIDATE, &expected, &output, &sequence) == NET_WIRE_OK);
    CHECK(sequence == 0x5aa5u && equal_m_vss_status(&m, &output));
    m = example_m_vss_status();
    expected = literal_frame(golden_m_vss_status, sizeof(golden_m_vss_status));
    m.domain_boot_id = UINT32_C(4294967295);
    patch_field(&expected, 12u, 4u, UINT64_C(4294967295));
    CHECK(net_wire_m_vss_status_encode(CANDIDATE, 0x5aa5u, &m, &actual) == NET_WIRE_OK);
    CHECK(actual.length == expected.length && memcmp(actual.bytes, expected.bytes, expected.length) == 0);
    CHECK(net_wire_m_vss_status_decode(CANDIDATE, &expected, &output, &sequence) == NET_WIRE_OK);
    CHECK(sequence == 0x5aa5u && equal_m_vss_status(&m, &output));
    m = example_m_vss_status();
    expected = literal_frame(golden_m_vss_status, sizeof(golden_m_vss_status));
    m.original_can_boot_id = UINT32_C(1);
    patch_field(&expected, 20u, 4u, UINT64_C(1));
    CHECK(net_wire_m_vss_status_encode(CANDIDATE, 0x5aa5u, &m, &actual) == NET_WIRE_OK);
    CHECK(actual.length == expected.length && memcmp(actual.bytes, expected.bytes, expected.length) == 0);
    CHECK(net_wire_m_vss_status_decode(CANDIDATE, &expected, &output, &sequence) == NET_WIRE_OK);
    CHECK(sequence == 0x5aa5u && equal_m_vss_status(&m, &output));
    m = example_m_vss_status();
    expected = literal_frame(golden_m_vss_status, sizeof(golden_m_vss_status));
    m.original_can_boot_id = UINT32_C(4294967295);
    patch_field(&expected, 20u, 4u, UINT64_C(4294967295));
    CHECK(net_wire_m_vss_status_encode(CANDIDATE, 0x5aa5u, &m, &actual) == NET_WIRE_OK);
    CHECK(actual.length == expected.length && memcmp(actual.bytes, expected.bytes, expected.length) == 0);
    CHECK(net_wire_m_vss_status_decode(CANDIDATE, &expected, &output, &sequence) == NET_WIRE_OK);
    CHECK(sequence == 0x5aa5u && equal_m_vss_status(&m, &output));
    m = example_m_vss_status();
    expected = literal_frame(golden_m_vss_status, sizeof(golden_m_vss_status));
    m.session_id = UINT64_C(0);
    patch_field(&expected, 4u, 8u, UINT64_C(0));
    CHECK(net_wire_m_vss_status_encode(CANDIDATE, 0x5aa5u, &m, &actual) == NET_WIRE_OK);
    CHECK(actual.length == expected.length && memcmp(actual.bytes, expected.bytes, expected.length) == 0);
    CHECK(net_wire_m_vss_status_decode(CANDIDATE, &expected, &output, &sequence) == NET_WIRE_OK);
    CHECK(sequence == 0x5aa5u && equal_m_vss_status(&m, &output));
    m = example_m_vss_status();
    expected = literal_frame(golden_m_vss_status, sizeof(golden_m_vss_status));
    m.session_id = UINT64_C(18446744073709551615);
    patch_field(&expected, 4u, 8u, UINT64_C(18446744073709551615));
    CHECK(net_wire_m_vss_status_encode(CANDIDATE, 0x5aa5u, &m, &actual) == NET_WIRE_OK);
    CHECK(actual.length == expected.length && memcmp(actual.bytes, expected.bytes, expected.length) == 0);
    CHECK(net_wire_m_vss_status_decode(CANDIDATE, &expected, &output, &sequence) == NET_WIRE_OK);
    CHECK(sequence == 0x5aa5u && equal_m_vss_status(&m, &output));
    m = example_m_vss_status();
    expected = literal_frame(golden_m_vss_status, sizeof(golden_m_vss_status));
    m.query_id = UINT32_C(0);
    patch_field(&expected, 16u, 4u, UINT64_C(0));
    CHECK(net_wire_m_vss_status_encode(CANDIDATE, 0x5aa5u, &m, &actual) == NET_WIRE_OK);
    CHECK(actual.length == expected.length && memcmp(actual.bytes, expected.bytes, expected.length) == 0);
    CHECK(net_wire_m_vss_status_decode(CANDIDATE, &expected, &output, &sequence) == NET_WIRE_OK);
    CHECK(sequence == 0x5aa5u && equal_m_vss_status(&m, &output));
    m = example_m_vss_status();
    expected = literal_frame(golden_m_vss_status, sizeof(golden_m_vss_status));
    m.query_id = UINT32_C(4294967295);
    patch_field(&expected, 16u, 4u, UINT64_C(4294967295));
    CHECK(net_wire_m_vss_status_encode(CANDIDATE, 0x5aa5u, &m, &actual) == NET_WIRE_OK);
    CHECK(actual.length == expected.length && memcmp(actual.bytes, expected.bytes, expected.length) == 0);
    CHECK(net_wire_m_vss_status_decode(CANDIDATE, &expected, &output, &sequence) == NET_WIRE_OK);
    CHECK(sequence == 0x5aa5u && equal_m_vss_status(&m, &output));
    m = example_m_vss_status();
    expected = literal_frame(golden_m_vss_status, sizeof(golden_m_vss_status));
    m.original_can_tx_sequence = UINT32_C(0);
    patch_field(&expected, 24u, 4u, UINT64_C(0));
    CHECK(net_wire_m_vss_status_encode(CANDIDATE, 0x5aa5u, &m, &actual) == NET_WIRE_OK);
    CHECK(actual.length == expected.length && memcmp(actual.bytes, expected.bytes, expected.length) == 0);
    CHECK(net_wire_m_vss_status_decode(CANDIDATE, &expected, &output, &sequence) == NET_WIRE_OK);
    CHECK(sequence == 0x5aa5u && equal_m_vss_status(&m, &output));
    m = example_m_vss_status();
    expected = literal_frame(golden_m_vss_status, sizeof(golden_m_vss_status));
    m.original_can_tx_sequence = UINT32_C(4294967295);
    patch_field(&expected, 24u, 4u, UINT64_C(4294967295));
    CHECK(net_wire_m_vss_status_encode(CANDIDATE, 0x5aa5u, &m, &actual) == NET_WIRE_OK);
    CHECK(actual.length == expected.length && memcmp(actual.bytes, expected.bytes, expected.length) == 0);
    CHECK(net_wire_m_vss_status_decode(CANDIDATE, &expected, &output, &sequence) == NET_WIRE_OK);
    CHECK(sequence == 0x5aa5u && equal_m_vss_status(&m, &output));
    m = example_m_vss_status();
    expected = literal_frame(golden_m_vss_status, sizeof(golden_m_vss_status));
    m.update_sequence = UINT32_C(0);
    patch_field(&expected, 42u, 4u, UINT64_C(0));
    CHECK(net_wire_m_vss_status_encode(CANDIDATE, 0x5aa5u, &m, &actual) == NET_WIRE_OK);
    CHECK(actual.length == expected.length && memcmp(actual.bytes, expected.bytes, expected.length) == 0);
    CHECK(net_wire_m_vss_status_decode(CANDIDATE, &expected, &output, &sequence) == NET_WIRE_OK);
    CHECK(sequence == 0x5aa5u && equal_m_vss_status(&m, &output));
    m = example_m_vss_status();
    expected = literal_frame(golden_m_vss_status, sizeof(golden_m_vss_status));
    m.update_sequence = UINT32_C(4294967295);
    patch_field(&expected, 42u, 4u, UINT64_C(4294967295));
    CHECK(net_wire_m_vss_status_encode(CANDIDATE, 0x5aa5u, &m, &actual) == NET_WIRE_OK);
    CHECK(actual.length == expected.length && memcmp(actual.bytes, expected.bytes, expected.length) == 0);
    CHECK(net_wire_m_vss_status_decode(CANDIDATE, &expected, &output, &sequence) == NET_WIRE_OK);
    CHECK(sequence == 0x5aa5u && equal_m_vss_status(&m, &output));
    m = example_m_vss_status();
    expected = literal_frame(golden_m_vss_status, sizeof(golden_m_vss_status));
    m.source_age_ms = UINT32_C(0);
    patch_field(&expected, 46u, 2u, UINT64_C(0));
    CHECK(net_wire_m_vss_status_encode(CANDIDATE, 0x5aa5u, &m, &actual) == NET_WIRE_OK);
    CHECK(actual.length == expected.length && memcmp(actual.bytes, expected.bytes, expected.length) == 0);
    CHECK(net_wire_m_vss_status_decode(CANDIDATE, &expected, &output, &sequence) == NET_WIRE_OK);
    CHECK(sequence == 0x5aa5u && equal_m_vss_status(&m, &output));
    m = example_m_vss_status();
    expected = literal_frame(golden_m_vss_status, sizeof(golden_m_vss_status));
    m.source_age_ms = UINT32_C(65535);
    patch_field(&expected, 46u, 2u, UINT64_C(65535));
    CHECK(net_wire_m_vss_status_encode(CANDIDATE, 0x5aa5u, &m, &actual) == NET_WIRE_OK);
    CHECK(actual.length == expected.length && memcmp(actual.bytes, expected.bytes, expected.length) == 0);
    CHECK(net_wire_m_vss_status_decode(CANDIDATE, &expected, &output, &sequence) == NET_WIRE_OK);
    CHECK(sequence == 0x5aa5u && equal_m_vss_status(&m, &output));
    return 0;
}

/* FINAL_98/docs/network/interface/03_COMMUNICATION/20_UART_M.md:609; independent Python struct/CRC literal. */
static const uint8_t golden_m_window_state[52] = {
    0xa5,0x5a,0x02,0x38,0x2a,0x00,0xa5,0x5a,0xa5,0x5a,0x22,0x11,0x01,0x02,0x03,0x04,
    0x05,0x06,0x07,0x08,0x44,0x33,0x22,0x11,0x99,0x88,0x77,0x66,0xd4,0xc3,0xb2,0xa1,
    0x40,0x30,0x20,0x10,0x02,0x01,0x25,0x01,0x00,0x00,0x01,0x01,0xcd,0xab,0x34,0x12,
    0xff,0xff,0xe4,0x64,
};
static NetWireMWindowStateRaw example_m_window_state(void)
{
    NetWireMWindowStateRaw m = {0};
    m.device_context_id = UINT32_C(287464101);
    m.session_id = UINT64_C(578437695752307201);
    m.domain_boot_id = UINT32_C(287454020);
    m.query_id = UINT32_C(1719109785);
    m.original_can_boot_id = UINT32_C(2712847316);
    m.original_can_tx_sequence = UINT32_C(270544960);
    m.motion_state = UINT32_C(2);
    m.window_ecu_state = UINT32_C(1);
    m.window_position = UINT32_C(37);
    m.window_value_quality = UINT32_C(1);
    m.fully_open_state = UINT32_C(0);
    m.fully_closed_state = UINT32_C(0);
    m.antipinch_protection_status = UINT32_C(1);
    m.reverse_state = UINT32_C(1);
    m.update_sequence = UINT32_C(305441741);
    m.source_age_ms = UINT32_C(65535);
    return m;
}
static int equal_m_window_state(const NetWireMWindowStateRaw *a, const NetWireMWindowStateRaw *b)
{
    return a->device_context_id == b->device_context_id &&
        a->session_id == b->session_id &&
        a->domain_boot_id == b->domain_boot_id &&
        a->query_id == b->query_id &&
        a->original_can_boot_id == b->original_can_boot_id &&
        a->original_can_tx_sequence == b->original_can_tx_sequence &&
        a->motion_state == b->motion_state &&
        a->window_ecu_state == b->window_ecu_state &&
        a->window_position == b->window_position &&
        a->window_value_quality == b->window_value_quality &&
        a->fully_open_state == b->fully_open_state &&
        a->fully_closed_state == b->fully_closed_state &&
        a->antipinch_protection_status == b->antipinch_protection_status &&
        a->reverse_state == b->reverse_state &&
        a->update_sequence == b->update_sequence &&
        a->source_age_ms == b->source_age_ms;
}
static NetWireStatus encode_m_window_state(NetWireProfile p, uint16_t s, const void *m, NetWireUartFrame *f)
{
    return net_wire_m_window_state_encode(p, s, (const NetWireMWindowStateRaw *)m, f);
}
static NetWireStatus decode_m_window_state(NetWireProfile p, const NetWireUartFrame *f, void *m, uint16_t *s)
{
    return net_wire_m_window_state_decode(p, f, (NetWireMWindowStateRaw *)m, s);
}
static int test_m_window_state(void)
{
    NetWireMWindowStateRaw m = example_m_window_state(), output;
    NetWireUartFrame expected = literal_frame(golden_m_window_state, sizeof(golden_m_window_state)), actual;
    uint16_t sequence = 0u;
    CHECK(net_wire_m_window_state_encode(CANDIDATE, 0x5aa5u, &m, &actual) == NET_WIRE_OK);
    CHECK(actual.length == expected.length && memcmp(actual.bytes, expected.bytes, expected.length) == 0);
    CHECK(net_wire_m_window_state_decode(CANDIDATE, &expected, &output, &sequence) == NET_WIRE_OK);
    CHECK(sequence == 0x5aa5u && equal_m_window_state(&m, &output));
    CHECK(common_errors(encode_m_window_state, decode_m_window_state, &m, sizeof(m), golden_m_window_state, sizeof(golden_m_window_state)) == 0);
    expected = literal_frame(golden_m_window_state, sizeof(golden_m_window_state));
    expected.bytes[6] = 0u; expected.bytes[7] = 0u; repair_crc(&expected);
    CHECK(net_wire_m_window_state_encode(CANDIDATE, 0u, &m, &actual) == NET_WIRE_OK);
    CHECK(actual.length == expected.length && memcmp(actual.bytes, expected.bytes, expected.length) == 0);
    CHECK(net_wire_m_window_state_decode(CANDIDATE, &expected, &output, &sequence) == NET_WIRE_OK);
    CHECK(sequence == 0u && equal_m_window_state(&m, &output));
    expected = literal_frame(golden_m_window_state, sizeof(golden_m_window_state));
    expected.bytes[6] = 255u; expected.bytes[7] = 255u; repair_crc(&expected);
    CHECK(net_wire_m_window_state_encode(CANDIDATE, 65535u, &m, &actual) == NET_WIRE_OK);
    CHECK(actual.length == expected.length && memcmp(actual.bytes, expected.bytes, expected.length) == 0);
    CHECK(net_wire_m_window_state_decode(CANDIDATE, &expected, &output, &sequence) == NET_WIRE_OK);
    CHECK(sequence == 65535u && equal_m_window_state(&m, &output));
    m = example_m_window_state();
    expected = literal_frame(golden_m_window_state, sizeof(golden_m_window_state));
    m.device_context_id = UINT32_C(0);
    patch_field(&expected, 0u, 4u, UINT64_C(0));
    CHECK(rejected(encode_m_window_state, decode_m_window_state, &m, sizeof(m), &expected, NET_WIRE_MALFORMED) == 0);
    m = example_m_window_state();
    expected = literal_frame(golden_m_window_state, sizeof(golden_m_window_state));
    m.domain_boot_id = UINT32_C(0);
    patch_field(&expected, 12u, 4u, UINT64_C(0));
    CHECK(rejected(encode_m_window_state, decode_m_window_state, &m, sizeof(m), &expected, NET_WIRE_MALFORMED) == 0);
    m = example_m_window_state();
    expected = literal_frame(golden_m_window_state, sizeof(golden_m_window_state));
    m.original_can_boot_id = UINT32_C(0);
    patch_field(&expected, 20u, 4u, UINT64_C(0));
    CHECK(rejected(encode_m_window_state, decode_m_window_state, &m, sizeof(m), &expected, NET_WIRE_MALFORMED) == 0);
    m = example_m_window_state();
    expected = literal_frame(golden_m_window_state, sizeof(golden_m_window_state));
    m.motion_state = UINT32_C(4);
    patch_field(&expected, 28u, 1u, UINT64_C(4));
    CHECK(rejected(encode_m_window_state, decode_m_window_state, &m, sizeof(m), &expected, NET_WIRE_MALFORMED) == 0);
    m = example_m_window_state();
    expected = literal_frame(golden_m_window_state, sizeof(golden_m_window_state));
    m.window_ecu_state = UINT32_C(4);
    patch_field(&expected, 29u, 1u, UINT64_C(4));
    CHECK(rejected(encode_m_window_state, decode_m_window_state, &m, sizeof(m), &expected, NET_WIRE_MALFORMED) == 0);
    m = example_m_window_state();
    expected = literal_frame(golden_m_window_state, sizeof(golden_m_window_state));
    m.window_position = UINT32_C(101);
    patch_field(&expected, 30u, 1u, UINT64_C(101));
    CHECK(rejected(encode_m_window_state, decode_m_window_state, &m, sizeof(m), &expected, NET_WIRE_MALFORMED) == 0);
    m = example_m_window_state();
    expected = literal_frame(golden_m_window_state, sizeof(golden_m_window_state));
    m.window_value_quality = UINT32_C(4);
    patch_field(&expected, 31u, 1u, UINT64_C(4));
    CHECK(rejected(encode_m_window_state, decode_m_window_state, &m, sizeof(m), &expected, NET_WIRE_MALFORMED) == 0);
    m = example_m_window_state();
    expected = literal_frame(golden_m_window_state, sizeof(golden_m_window_state));
    m.fully_open_state = UINT32_C(2);
    patch_field(&expected, 32u, 1u, UINT64_C(2));
    CHECK(rejected(encode_m_window_state, decode_m_window_state, &m, sizeof(m), &expected, NET_WIRE_MALFORMED) == 0);
    m = example_m_window_state();
    expected = literal_frame(golden_m_window_state, sizeof(golden_m_window_state));
    m.fully_closed_state = UINT32_C(2);
    patch_field(&expected, 33u, 1u, UINT64_C(2));
    CHECK(rejected(encode_m_window_state, decode_m_window_state, &m, sizeof(m), &expected, NET_WIRE_MALFORMED) == 0);
    m = example_m_window_state();
    expected = literal_frame(golden_m_window_state, sizeof(golden_m_window_state));
    m.antipinch_protection_status = UINT32_C(4);
    patch_field(&expected, 34u, 1u, UINT64_C(4));
    CHECK(rejected(encode_m_window_state, decode_m_window_state, &m, sizeof(m), &expected, NET_WIRE_MALFORMED) == 0);
    m = example_m_window_state();
    expected = literal_frame(golden_m_window_state, sizeof(golden_m_window_state));
    m.reverse_state = UINT32_C(2);
    patch_field(&expected, 35u, 1u, UINT64_C(2));
    CHECK(rejected(encode_m_window_state, decode_m_window_state, &m, sizeof(m), &expected, NET_WIRE_MALFORMED) == 0);
    m = example_m_window_state();
    expected = literal_frame(golden_m_window_state, sizeof(golden_m_window_state));
    m.motion_state = UINT32_C(255);
    patch_field(&expected, 28u, 1u, UINT64_C(255));
    m.window_position = UINT32_C(255);
    patch_field(&expected, 30u, 1u, UINT64_C(255));
    m.window_value_quality = UINT32_C(3);
    patch_field(&expected, 31u, 1u, UINT64_C(3));
    m.fully_open_state = UINT32_C(255);
    patch_field(&expected, 32u, 1u, UINT64_C(255));
    m.fully_closed_state = UINT32_C(255);
    patch_field(&expected, 33u, 1u, UINT64_C(255));
    m.reverse_state = UINT32_C(255);
    patch_field(&expected, 35u, 1u, UINT64_C(255));
    CHECK(net_wire_m_window_state_encode(CANDIDATE, 0x5aa5u, &m, &actual) == NET_WIRE_OK);
    CHECK(actual.length == expected.length && memcmp(actual.bytes, expected.bytes, expected.length) == 0);
    CHECK(net_wire_m_window_state_decode(CANDIDATE, &expected, &output, &sequence) == NET_WIRE_OK);
    CHECK(sequence == 0x5aa5u && equal_m_window_state(&m, &output));
    m = example_m_window_state();
    expected = literal_frame(golden_m_window_state, sizeof(golden_m_window_state));
    m.window_position = UINT32_C(0);
    patch_field(&expected, 30u, 1u, UINT64_C(0));
    m.window_value_quality = UINT32_C(0);
    patch_field(&expected, 31u, 1u, UINT64_C(0));
    m.fully_open_state = UINT32_C(1);
    patch_field(&expected, 32u, 1u, UINT64_C(1));
    CHECK(net_wire_m_window_state_encode(CANDIDATE, 0x5aa5u, &m, &actual) == NET_WIRE_OK);
    CHECK(actual.length == expected.length && memcmp(actual.bytes, expected.bytes, expected.length) == 0);
    CHECK(net_wire_m_window_state_decode(CANDIDATE, &expected, &output, &sequence) == NET_WIRE_OK);
    CHECK(sequence == 0x5aa5u && equal_m_window_state(&m, &output));
    m = example_m_window_state();
    expected = literal_frame(golden_m_window_state, sizeof(golden_m_window_state));
    m.window_position = UINT32_C(100);
    patch_field(&expected, 30u, 1u, UINT64_C(100));
    m.window_value_quality = UINT32_C(0);
    patch_field(&expected, 31u, 1u, UINT64_C(0));
    m.fully_closed_state = UINT32_C(1);
    patch_field(&expected, 33u, 1u, UINT64_C(1));
    CHECK(net_wire_m_window_state_encode(CANDIDATE, 0x5aa5u, &m, &actual) == NET_WIRE_OK);
    CHECK(actual.length == expected.length && memcmp(actual.bytes, expected.bytes, expected.length) == 0);
    CHECK(net_wire_m_window_state_decode(CANDIDATE, &expected, &output, &sequence) == NET_WIRE_OK);
    CHECK(sequence == 0x5aa5u && equal_m_window_state(&m, &output));
    m = example_m_window_state();
    expected = literal_frame(golden_m_window_state, sizeof(golden_m_window_state));
    m.window_position = UINT32_C(255);
    patch_field(&expected, 30u, 1u, UINT64_C(255));
    m.window_value_quality = UINT32_C(0);
    patch_field(&expected, 31u, 1u, UINT64_C(0));
    CHECK(rejected(encode_m_window_state, decode_m_window_state, &m, sizeof(m), &expected, NET_WIRE_CONTRACT_HOLD) == 0);
    m = example_m_window_state();
    expected = literal_frame(golden_m_window_state, sizeof(golden_m_window_state));
    m.fully_closed_state = UINT32_C(1);
    patch_field(&expected, 33u, 1u, UINT64_C(1));
    m.window_value_quality = UINT32_C(0);
    patch_field(&expected, 31u, 1u, UINT64_C(0));
    CHECK(rejected(encode_m_window_state, decode_m_window_state, &m, sizeof(m), &expected, NET_WIRE_CONTRACT_HOLD) == 0);
    m = example_m_window_state();
    expected = literal_frame(golden_m_window_state, sizeof(golden_m_window_state));
    m.device_context_id = UINT32_C(1);
    patch_field(&expected, 0u, 4u, UINT64_C(1));
    CHECK(net_wire_m_window_state_encode(CANDIDATE, 0x5aa5u, &m, &actual) == NET_WIRE_OK);
    CHECK(actual.length == expected.length && memcmp(actual.bytes, expected.bytes, expected.length) == 0);
    CHECK(net_wire_m_window_state_decode(CANDIDATE, &expected, &output, &sequence) == NET_WIRE_OK);
    CHECK(sequence == 0x5aa5u && equal_m_window_state(&m, &output));
    m = example_m_window_state();
    expected = literal_frame(golden_m_window_state, sizeof(golden_m_window_state));
    m.device_context_id = UINT32_C(4294967295);
    patch_field(&expected, 0u, 4u, UINT64_C(4294967295));
    CHECK(net_wire_m_window_state_encode(CANDIDATE, 0x5aa5u, &m, &actual) == NET_WIRE_OK);
    CHECK(actual.length == expected.length && memcmp(actual.bytes, expected.bytes, expected.length) == 0);
    CHECK(net_wire_m_window_state_decode(CANDIDATE, &expected, &output, &sequence) == NET_WIRE_OK);
    CHECK(sequence == 0x5aa5u && equal_m_window_state(&m, &output));
    m = example_m_window_state();
    expected = literal_frame(golden_m_window_state, sizeof(golden_m_window_state));
    m.domain_boot_id = UINT32_C(1);
    patch_field(&expected, 12u, 4u, UINT64_C(1));
    CHECK(net_wire_m_window_state_encode(CANDIDATE, 0x5aa5u, &m, &actual) == NET_WIRE_OK);
    CHECK(actual.length == expected.length && memcmp(actual.bytes, expected.bytes, expected.length) == 0);
    CHECK(net_wire_m_window_state_decode(CANDIDATE, &expected, &output, &sequence) == NET_WIRE_OK);
    CHECK(sequence == 0x5aa5u && equal_m_window_state(&m, &output));
    m = example_m_window_state();
    expected = literal_frame(golden_m_window_state, sizeof(golden_m_window_state));
    m.domain_boot_id = UINT32_C(4294967295);
    patch_field(&expected, 12u, 4u, UINT64_C(4294967295));
    CHECK(net_wire_m_window_state_encode(CANDIDATE, 0x5aa5u, &m, &actual) == NET_WIRE_OK);
    CHECK(actual.length == expected.length && memcmp(actual.bytes, expected.bytes, expected.length) == 0);
    CHECK(net_wire_m_window_state_decode(CANDIDATE, &expected, &output, &sequence) == NET_WIRE_OK);
    CHECK(sequence == 0x5aa5u && equal_m_window_state(&m, &output));
    m = example_m_window_state();
    expected = literal_frame(golden_m_window_state, sizeof(golden_m_window_state));
    m.original_can_boot_id = UINT32_C(1);
    patch_field(&expected, 20u, 4u, UINT64_C(1));
    CHECK(net_wire_m_window_state_encode(CANDIDATE, 0x5aa5u, &m, &actual) == NET_WIRE_OK);
    CHECK(actual.length == expected.length && memcmp(actual.bytes, expected.bytes, expected.length) == 0);
    CHECK(net_wire_m_window_state_decode(CANDIDATE, &expected, &output, &sequence) == NET_WIRE_OK);
    CHECK(sequence == 0x5aa5u && equal_m_window_state(&m, &output));
    m = example_m_window_state();
    expected = literal_frame(golden_m_window_state, sizeof(golden_m_window_state));
    m.original_can_boot_id = UINT32_C(4294967295);
    patch_field(&expected, 20u, 4u, UINT64_C(4294967295));
    CHECK(net_wire_m_window_state_encode(CANDIDATE, 0x5aa5u, &m, &actual) == NET_WIRE_OK);
    CHECK(actual.length == expected.length && memcmp(actual.bytes, expected.bytes, expected.length) == 0);
    CHECK(net_wire_m_window_state_decode(CANDIDATE, &expected, &output, &sequence) == NET_WIRE_OK);
    CHECK(sequence == 0x5aa5u && equal_m_window_state(&m, &output));
    m = example_m_window_state();
    expected = literal_frame(golden_m_window_state, sizeof(golden_m_window_state));
    m.session_id = UINT64_C(0);
    patch_field(&expected, 4u, 8u, UINT64_C(0));
    CHECK(net_wire_m_window_state_encode(CANDIDATE, 0x5aa5u, &m, &actual) == NET_WIRE_OK);
    CHECK(actual.length == expected.length && memcmp(actual.bytes, expected.bytes, expected.length) == 0);
    CHECK(net_wire_m_window_state_decode(CANDIDATE, &expected, &output, &sequence) == NET_WIRE_OK);
    CHECK(sequence == 0x5aa5u && equal_m_window_state(&m, &output));
    m = example_m_window_state();
    expected = literal_frame(golden_m_window_state, sizeof(golden_m_window_state));
    m.session_id = UINT64_C(18446744073709551615);
    patch_field(&expected, 4u, 8u, UINT64_C(18446744073709551615));
    CHECK(net_wire_m_window_state_encode(CANDIDATE, 0x5aa5u, &m, &actual) == NET_WIRE_OK);
    CHECK(actual.length == expected.length && memcmp(actual.bytes, expected.bytes, expected.length) == 0);
    CHECK(net_wire_m_window_state_decode(CANDIDATE, &expected, &output, &sequence) == NET_WIRE_OK);
    CHECK(sequence == 0x5aa5u && equal_m_window_state(&m, &output));
    m = example_m_window_state();
    expected = literal_frame(golden_m_window_state, sizeof(golden_m_window_state));
    m.query_id = UINT32_C(0);
    patch_field(&expected, 16u, 4u, UINT64_C(0));
    CHECK(net_wire_m_window_state_encode(CANDIDATE, 0x5aa5u, &m, &actual) == NET_WIRE_OK);
    CHECK(actual.length == expected.length && memcmp(actual.bytes, expected.bytes, expected.length) == 0);
    CHECK(net_wire_m_window_state_decode(CANDIDATE, &expected, &output, &sequence) == NET_WIRE_OK);
    CHECK(sequence == 0x5aa5u && equal_m_window_state(&m, &output));
    m = example_m_window_state();
    expected = literal_frame(golden_m_window_state, sizeof(golden_m_window_state));
    m.query_id = UINT32_C(4294967295);
    patch_field(&expected, 16u, 4u, UINT64_C(4294967295));
    CHECK(net_wire_m_window_state_encode(CANDIDATE, 0x5aa5u, &m, &actual) == NET_WIRE_OK);
    CHECK(actual.length == expected.length && memcmp(actual.bytes, expected.bytes, expected.length) == 0);
    CHECK(net_wire_m_window_state_decode(CANDIDATE, &expected, &output, &sequence) == NET_WIRE_OK);
    CHECK(sequence == 0x5aa5u && equal_m_window_state(&m, &output));
    m = example_m_window_state();
    expected = literal_frame(golden_m_window_state, sizeof(golden_m_window_state));
    m.original_can_tx_sequence = UINT32_C(0);
    patch_field(&expected, 24u, 4u, UINT64_C(0));
    CHECK(net_wire_m_window_state_encode(CANDIDATE, 0x5aa5u, &m, &actual) == NET_WIRE_OK);
    CHECK(actual.length == expected.length && memcmp(actual.bytes, expected.bytes, expected.length) == 0);
    CHECK(net_wire_m_window_state_decode(CANDIDATE, &expected, &output, &sequence) == NET_WIRE_OK);
    CHECK(sequence == 0x5aa5u && equal_m_window_state(&m, &output));
    m = example_m_window_state();
    expected = literal_frame(golden_m_window_state, sizeof(golden_m_window_state));
    m.original_can_tx_sequence = UINT32_C(4294967295);
    patch_field(&expected, 24u, 4u, UINT64_C(4294967295));
    CHECK(net_wire_m_window_state_encode(CANDIDATE, 0x5aa5u, &m, &actual) == NET_WIRE_OK);
    CHECK(actual.length == expected.length && memcmp(actual.bytes, expected.bytes, expected.length) == 0);
    CHECK(net_wire_m_window_state_decode(CANDIDATE, &expected, &output, &sequence) == NET_WIRE_OK);
    CHECK(sequence == 0x5aa5u && equal_m_window_state(&m, &output));
    m = example_m_window_state();
    expected = literal_frame(golden_m_window_state, sizeof(golden_m_window_state));
    m.update_sequence = UINT32_C(0);
    patch_field(&expected, 36u, 4u, UINT64_C(0));
    CHECK(net_wire_m_window_state_encode(CANDIDATE, 0x5aa5u, &m, &actual) == NET_WIRE_OK);
    CHECK(actual.length == expected.length && memcmp(actual.bytes, expected.bytes, expected.length) == 0);
    CHECK(net_wire_m_window_state_decode(CANDIDATE, &expected, &output, &sequence) == NET_WIRE_OK);
    CHECK(sequence == 0x5aa5u && equal_m_window_state(&m, &output));
    m = example_m_window_state();
    expected = literal_frame(golden_m_window_state, sizeof(golden_m_window_state));
    m.update_sequence = UINT32_C(4294967295);
    patch_field(&expected, 36u, 4u, UINT64_C(4294967295));
    CHECK(net_wire_m_window_state_encode(CANDIDATE, 0x5aa5u, &m, &actual) == NET_WIRE_OK);
    CHECK(actual.length == expected.length && memcmp(actual.bytes, expected.bytes, expected.length) == 0);
    CHECK(net_wire_m_window_state_decode(CANDIDATE, &expected, &output, &sequence) == NET_WIRE_OK);
    CHECK(sequence == 0x5aa5u && equal_m_window_state(&m, &output));
    m = example_m_window_state();
    expected = literal_frame(golden_m_window_state, sizeof(golden_m_window_state));
    m.source_age_ms = UINT32_C(0);
    patch_field(&expected, 40u, 2u, UINT64_C(0));
    CHECK(net_wire_m_window_state_encode(CANDIDATE, 0x5aa5u, &m, &actual) == NET_WIRE_OK);
    CHECK(actual.length == expected.length && memcmp(actual.bytes, expected.bytes, expected.length) == 0);
    CHECK(net_wire_m_window_state_decode(CANDIDATE, &expected, &output, &sequence) == NET_WIRE_OK);
    CHECK(sequence == 0x5aa5u && equal_m_window_state(&m, &output));
    m = example_m_window_state();
    expected = literal_frame(golden_m_window_state, sizeof(golden_m_window_state));
    m.source_age_ms = UINT32_C(65535);
    patch_field(&expected, 40u, 2u, UINT64_C(65535));
    CHECK(net_wire_m_window_state_encode(CANDIDATE, 0x5aa5u, &m, &actual) == NET_WIRE_OK);
    CHECK(actual.length == expected.length && memcmp(actual.bytes, expected.bytes, expected.length) == 0);
    CHECK(net_wire_m_window_state_decode(CANDIDATE, &expected, &output, &sequence) == NET_WIRE_OK);
    CHECK(sequence == 0x5aa5u && equal_m_window_state(&m, &output));
    return 0;
}

/* FINAL_98/docs/network/interface/03_COMMUNICATION/20_UART_M.md:635; independent Python struct/CRC literal. */
static const uint8_t golden_m_window_fault[52] = {
    0xa5,0x5a,0x02,0x39,0x2a,0x00,0xa5,0x5a,0xa5,0x5a,0x22,0x11,0x01,0x02,0x03,0x04,
    0x05,0x06,0x07,0x08,0x44,0x33,0x22,0x11,0x99,0x88,0x77,0x66,0xd4,0xc3,0xb2,0xa1,
    0x40,0x30,0x20,0x10,0x03,0x05,0x00,0x02,0x40,0x30,0x20,0x10,0xd4,0xc3,0xb2,0xa1,
    0xff,0xff,0x11,0xee,
};
static NetWireMWindowFaultRaw example_m_window_fault(void)
{
    NetWireMWindowFaultRaw m = {0};
    m.device_context_id = UINT32_C(287464101);
    m.session_id = UINT64_C(578437695752307201);
    m.domain_boot_id = UINT32_C(287454020);
    m.query_id = UINT32_C(1719109785);
    m.original_can_boot_id = UINT32_C(2712847316);
    m.original_can_tx_sequence = UINT32_C(270544960);
    m.fault_category = UINT32_C(3);
    m.fault_code = UINT32_C(5);
    m.fault_status = UINT32_C(2);
    m.occurrence_id = UINT32_C(270544960);
    m.update_sequence = UINT32_C(2712847316);
    m.source_age_ms = UINT32_C(65535);
    return m;
}
static int equal_m_window_fault(const NetWireMWindowFaultRaw *a, const NetWireMWindowFaultRaw *b)
{
    return a->device_context_id == b->device_context_id &&
        a->session_id == b->session_id &&
        a->domain_boot_id == b->domain_boot_id &&
        a->query_id == b->query_id &&
        a->original_can_boot_id == b->original_can_boot_id &&
        a->original_can_tx_sequence == b->original_can_tx_sequence &&
        a->fault_category == b->fault_category &&
        a->fault_code == b->fault_code &&
        a->fault_status == b->fault_status &&
        a->occurrence_id == b->occurrence_id &&
        a->update_sequence == b->update_sequence &&
        a->source_age_ms == b->source_age_ms;
}
static NetWireStatus encode_m_window_fault(NetWireProfile p, uint16_t s, const void *m, NetWireUartFrame *f)
{
    return net_wire_m_window_fault_encode(p, s, (const NetWireMWindowFaultRaw *)m, f);
}
static NetWireStatus decode_m_window_fault(NetWireProfile p, const NetWireUartFrame *f, void *m, uint16_t *s)
{
    return net_wire_m_window_fault_decode(p, f, (NetWireMWindowFaultRaw *)m, s);
}
static int test_m_window_fault(void)
{
    NetWireMWindowFaultRaw m = example_m_window_fault(), output;
    NetWireUartFrame expected = literal_frame(golden_m_window_fault, sizeof(golden_m_window_fault)), actual;
    uint16_t sequence = 0u;
    CHECK(net_wire_m_window_fault_encode(CANDIDATE, 0x5aa5u, &m, &actual) == NET_WIRE_OK);
    CHECK(actual.length == expected.length && memcmp(actual.bytes, expected.bytes, expected.length) == 0);
    CHECK(net_wire_m_window_fault_decode(CANDIDATE, &expected, &output, &sequence) == NET_WIRE_OK);
    CHECK(sequence == 0x5aa5u && equal_m_window_fault(&m, &output));
    CHECK(common_errors(encode_m_window_fault, decode_m_window_fault, &m, sizeof(m), golden_m_window_fault, sizeof(golden_m_window_fault)) == 0);
    expected = literal_frame(golden_m_window_fault, sizeof(golden_m_window_fault));
    expected.bytes[6] = 0u; expected.bytes[7] = 0u; repair_crc(&expected);
    CHECK(net_wire_m_window_fault_encode(CANDIDATE, 0u, &m, &actual) == NET_WIRE_OK);
    CHECK(actual.length == expected.length && memcmp(actual.bytes, expected.bytes, expected.length) == 0);
    CHECK(net_wire_m_window_fault_decode(CANDIDATE, &expected, &output, &sequence) == NET_WIRE_OK);
    CHECK(sequence == 0u && equal_m_window_fault(&m, &output));
    expected = literal_frame(golden_m_window_fault, sizeof(golden_m_window_fault));
    expected.bytes[6] = 255u; expected.bytes[7] = 255u; repair_crc(&expected);
    CHECK(net_wire_m_window_fault_encode(CANDIDATE, 65535u, &m, &actual) == NET_WIRE_OK);
    CHECK(actual.length == expected.length && memcmp(actual.bytes, expected.bytes, expected.length) == 0);
    CHECK(net_wire_m_window_fault_decode(CANDIDATE, &expected, &output, &sequence) == NET_WIRE_OK);
    CHECK(sequence == 65535u && equal_m_window_fault(&m, &output));
    m = example_m_window_fault();
    expected = literal_frame(golden_m_window_fault, sizeof(golden_m_window_fault));
    m.device_context_id = UINT32_C(0);
    patch_field(&expected, 0u, 4u, UINT64_C(0));
    CHECK(rejected(encode_m_window_fault, decode_m_window_fault, &m, sizeof(m), &expected, NET_WIRE_MALFORMED) == 0);
    m = example_m_window_fault();
    expected = literal_frame(golden_m_window_fault, sizeof(golden_m_window_fault));
    m.domain_boot_id = UINT32_C(0);
    patch_field(&expected, 12u, 4u, UINT64_C(0));
    CHECK(rejected(encode_m_window_fault, decode_m_window_fault, &m, sizeof(m), &expected, NET_WIRE_MALFORMED) == 0);
    m = example_m_window_fault();
    expected = literal_frame(golden_m_window_fault, sizeof(golden_m_window_fault));
    m.original_can_boot_id = UINT32_C(0);
    patch_field(&expected, 20u, 4u, UINT64_C(0));
    CHECK(rejected(encode_m_window_fault, decode_m_window_fault, &m, sizeof(m), &expected, NET_WIRE_MALFORMED) == 0);
    m = example_m_window_fault();
    expected = literal_frame(golden_m_window_fault, sizeof(golden_m_window_fault));
    m.fault_category = UINT32_C(4);
    patch_field(&expected, 28u, 1u, UINT64_C(4));
    CHECK(rejected(encode_m_window_fault, decode_m_window_fault, &m, sizeof(m), &expected, NET_WIRE_MALFORMED) == 0);
    m = example_m_window_fault();
    expected = literal_frame(golden_m_window_fault, sizeof(golden_m_window_fault));
    m.fault_code = UINT32_C(7);
    patch_field(&expected, 29u, 2u, UINT64_C(7));
    CHECK(rejected(encode_m_window_fault, decode_m_window_fault, &m, sizeof(m), &expected, NET_WIRE_MALFORMED) == 0);
    m = example_m_window_fault();
    expected = literal_frame(golden_m_window_fault, sizeof(golden_m_window_fault));
    m.fault_status = UINT32_C(3);
    patch_field(&expected, 31u, 1u, UINT64_C(3));
    CHECK(rejected(encode_m_window_fault, decode_m_window_fault, &m, sizeof(m), &expected, NET_WIRE_MALFORMED) == 0);
    m = example_m_window_fault();
    expected = literal_frame(golden_m_window_fault, sizeof(golden_m_window_fault));
    m.fault_code = UINT32_C(255);
    patch_field(&expected, 29u, 2u, UINT64_C(255));
    CHECK(net_wire_m_window_fault_encode(CANDIDATE, 0x5aa5u, &m, &actual) == NET_WIRE_OK);
    CHECK(actual.length == expected.length && memcmp(actual.bytes, expected.bytes, expected.length) == 0);
    CHECK(net_wire_m_window_fault_decode(CANDIDATE, &expected, &output, &sequence) == NET_WIRE_OK);
    CHECK(sequence == 0x5aa5u && equal_m_window_fault(&m, &output));
    m = example_m_window_fault();
    expected = literal_frame(golden_m_window_fault, sizeof(golden_m_window_fault));
    m.fault_category = UINT32_C(0);
    patch_field(&expected, 28u, 1u, UINT64_C(0));
    m.fault_code = UINT32_C(0);
    patch_field(&expected, 29u, 2u, UINT64_C(0));
    m.fault_status = UINT32_C(0);
    patch_field(&expected, 31u, 1u, UINT64_C(0));
    m.occurrence_id = UINT32_C(4294967295);
    patch_field(&expected, 32u, 4u, UINT64_C(4294967295));
    CHECK(net_wire_m_window_fault_encode(CANDIDATE, 0x5aa5u, &m, &actual) == NET_WIRE_OK);
    CHECK(actual.length == expected.length && memcmp(actual.bytes, expected.bytes, expected.length) == 0);
    CHECK(net_wire_m_window_fault_decode(CANDIDATE, &expected, &output, &sequence) == NET_WIRE_OK);
    CHECK(sequence == 0x5aa5u && equal_m_window_fault(&m, &output));
    m = example_m_window_fault();
    expected = literal_frame(golden_m_window_fault, sizeof(golden_m_window_fault));
    m.device_context_id = UINT32_C(1);
    patch_field(&expected, 0u, 4u, UINT64_C(1));
    CHECK(net_wire_m_window_fault_encode(CANDIDATE, 0x5aa5u, &m, &actual) == NET_WIRE_OK);
    CHECK(actual.length == expected.length && memcmp(actual.bytes, expected.bytes, expected.length) == 0);
    CHECK(net_wire_m_window_fault_decode(CANDIDATE, &expected, &output, &sequence) == NET_WIRE_OK);
    CHECK(sequence == 0x5aa5u && equal_m_window_fault(&m, &output));
    m = example_m_window_fault();
    expected = literal_frame(golden_m_window_fault, sizeof(golden_m_window_fault));
    m.device_context_id = UINT32_C(4294967295);
    patch_field(&expected, 0u, 4u, UINT64_C(4294967295));
    CHECK(net_wire_m_window_fault_encode(CANDIDATE, 0x5aa5u, &m, &actual) == NET_WIRE_OK);
    CHECK(actual.length == expected.length && memcmp(actual.bytes, expected.bytes, expected.length) == 0);
    CHECK(net_wire_m_window_fault_decode(CANDIDATE, &expected, &output, &sequence) == NET_WIRE_OK);
    CHECK(sequence == 0x5aa5u && equal_m_window_fault(&m, &output));
    m = example_m_window_fault();
    expected = literal_frame(golden_m_window_fault, sizeof(golden_m_window_fault));
    m.domain_boot_id = UINT32_C(1);
    patch_field(&expected, 12u, 4u, UINT64_C(1));
    CHECK(net_wire_m_window_fault_encode(CANDIDATE, 0x5aa5u, &m, &actual) == NET_WIRE_OK);
    CHECK(actual.length == expected.length && memcmp(actual.bytes, expected.bytes, expected.length) == 0);
    CHECK(net_wire_m_window_fault_decode(CANDIDATE, &expected, &output, &sequence) == NET_WIRE_OK);
    CHECK(sequence == 0x5aa5u && equal_m_window_fault(&m, &output));
    m = example_m_window_fault();
    expected = literal_frame(golden_m_window_fault, sizeof(golden_m_window_fault));
    m.domain_boot_id = UINT32_C(4294967295);
    patch_field(&expected, 12u, 4u, UINT64_C(4294967295));
    CHECK(net_wire_m_window_fault_encode(CANDIDATE, 0x5aa5u, &m, &actual) == NET_WIRE_OK);
    CHECK(actual.length == expected.length && memcmp(actual.bytes, expected.bytes, expected.length) == 0);
    CHECK(net_wire_m_window_fault_decode(CANDIDATE, &expected, &output, &sequence) == NET_WIRE_OK);
    CHECK(sequence == 0x5aa5u && equal_m_window_fault(&m, &output));
    m = example_m_window_fault();
    expected = literal_frame(golden_m_window_fault, sizeof(golden_m_window_fault));
    m.original_can_boot_id = UINT32_C(1);
    patch_field(&expected, 20u, 4u, UINT64_C(1));
    CHECK(net_wire_m_window_fault_encode(CANDIDATE, 0x5aa5u, &m, &actual) == NET_WIRE_OK);
    CHECK(actual.length == expected.length && memcmp(actual.bytes, expected.bytes, expected.length) == 0);
    CHECK(net_wire_m_window_fault_decode(CANDIDATE, &expected, &output, &sequence) == NET_WIRE_OK);
    CHECK(sequence == 0x5aa5u && equal_m_window_fault(&m, &output));
    m = example_m_window_fault();
    expected = literal_frame(golden_m_window_fault, sizeof(golden_m_window_fault));
    m.original_can_boot_id = UINT32_C(4294967295);
    patch_field(&expected, 20u, 4u, UINT64_C(4294967295));
    CHECK(net_wire_m_window_fault_encode(CANDIDATE, 0x5aa5u, &m, &actual) == NET_WIRE_OK);
    CHECK(actual.length == expected.length && memcmp(actual.bytes, expected.bytes, expected.length) == 0);
    CHECK(net_wire_m_window_fault_decode(CANDIDATE, &expected, &output, &sequence) == NET_WIRE_OK);
    CHECK(sequence == 0x5aa5u && equal_m_window_fault(&m, &output));
    m = example_m_window_fault();
    expected = literal_frame(golden_m_window_fault, sizeof(golden_m_window_fault));
    m.session_id = UINT64_C(0);
    patch_field(&expected, 4u, 8u, UINT64_C(0));
    CHECK(net_wire_m_window_fault_encode(CANDIDATE, 0x5aa5u, &m, &actual) == NET_WIRE_OK);
    CHECK(actual.length == expected.length && memcmp(actual.bytes, expected.bytes, expected.length) == 0);
    CHECK(net_wire_m_window_fault_decode(CANDIDATE, &expected, &output, &sequence) == NET_WIRE_OK);
    CHECK(sequence == 0x5aa5u && equal_m_window_fault(&m, &output));
    m = example_m_window_fault();
    expected = literal_frame(golden_m_window_fault, sizeof(golden_m_window_fault));
    m.session_id = UINT64_C(18446744073709551615);
    patch_field(&expected, 4u, 8u, UINT64_C(18446744073709551615));
    CHECK(net_wire_m_window_fault_encode(CANDIDATE, 0x5aa5u, &m, &actual) == NET_WIRE_OK);
    CHECK(actual.length == expected.length && memcmp(actual.bytes, expected.bytes, expected.length) == 0);
    CHECK(net_wire_m_window_fault_decode(CANDIDATE, &expected, &output, &sequence) == NET_WIRE_OK);
    CHECK(sequence == 0x5aa5u && equal_m_window_fault(&m, &output));
    m = example_m_window_fault();
    expected = literal_frame(golden_m_window_fault, sizeof(golden_m_window_fault));
    m.query_id = UINT32_C(0);
    patch_field(&expected, 16u, 4u, UINT64_C(0));
    CHECK(net_wire_m_window_fault_encode(CANDIDATE, 0x5aa5u, &m, &actual) == NET_WIRE_OK);
    CHECK(actual.length == expected.length && memcmp(actual.bytes, expected.bytes, expected.length) == 0);
    CHECK(net_wire_m_window_fault_decode(CANDIDATE, &expected, &output, &sequence) == NET_WIRE_OK);
    CHECK(sequence == 0x5aa5u && equal_m_window_fault(&m, &output));
    m = example_m_window_fault();
    expected = literal_frame(golden_m_window_fault, sizeof(golden_m_window_fault));
    m.query_id = UINT32_C(4294967295);
    patch_field(&expected, 16u, 4u, UINT64_C(4294967295));
    CHECK(net_wire_m_window_fault_encode(CANDIDATE, 0x5aa5u, &m, &actual) == NET_WIRE_OK);
    CHECK(actual.length == expected.length && memcmp(actual.bytes, expected.bytes, expected.length) == 0);
    CHECK(net_wire_m_window_fault_decode(CANDIDATE, &expected, &output, &sequence) == NET_WIRE_OK);
    CHECK(sequence == 0x5aa5u && equal_m_window_fault(&m, &output));
    m = example_m_window_fault();
    expected = literal_frame(golden_m_window_fault, sizeof(golden_m_window_fault));
    m.original_can_tx_sequence = UINT32_C(0);
    patch_field(&expected, 24u, 4u, UINT64_C(0));
    CHECK(net_wire_m_window_fault_encode(CANDIDATE, 0x5aa5u, &m, &actual) == NET_WIRE_OK);
    CHECK(actual.length == expected.length && memcmp(actual.bytes, expected.bytes, expected.length) == 0);
    CHECK(net_wire_m_window_fault_decode(CANDIDATE, &expected, &output, &sequence) == NET_WIRE_OK);
    CHECK(sequence == 0x5aa5u && equal_m_window_fault(&m, &output));
    m = example_m_window_fault();
    expected = literal_frame(golden_m_window_fault, sizeof(golden_m_window_fault));
    m.original_can_tx_sequence = UINT32_C(4294967295);
    patch_field(&expected, 24u, 4u, UINT64_C(4294967295));
    CHECK(net_wire_m_window_fault_encode(CANDIDATE, 0x5aa5u, &m, &actual) == NET_WIRE_OK);
    CHECK(actual.length == expected.length && memcmp(actual.bytes, expected.bytes, expected.length) == 0);
    CHECK(net_wire_m_window_fault_decode(CANDIDATE, &expected, &output, &sequence) == NET_WIRE_OK);
    CHECK(sequence == 0x5aa5u && equal_m_window_fault(&m, &output));
    m = example_m_window_fault();
    expected = literal_frame(golden_m_window_fault, sizeof(golden_m_window_fault));
    m.update_sequence = UINT32_C(0);
    patch_field(&expected, 36u, 4u, UINT64_C(0));
    CHECK(net_wire_m_window_fault_encode(CANDIDATE, 0x5aa5u, &m, &actual) == NET_WIRE_OK);
    CHECK(actual.length == expected.length && memcmp(actual.bytes, expected.bytes, expected.length) == 0);
    CHECK(net_wire_m_window_fault_decode(CANDIDATE, &expected, &output, &sequence) == NET_WIRE_OK);
    CHECK(sequence == 0x5aa5u && equal_m_window_fault(&m, &output));
    m = example_m_window_fault();
    expected = literal_frame(golden_m_window_fault, sizeof(golden_m_window_fault));
    m.update_sequence = UINT32_C(4294967295);
    patch_field(&expected, 36u, 4u, UINT64_C(4294967295));
    CHECK(net_wire_m_window_fault_encode(CANDIDATE, 0x5aa5u, &m, &actual) == NET_WIRE_OK);
    CHECK(actual.length == expected.length && memcmp(actual.bytes, expected.bytes, expected.length) == 0);
    CHECK(net_wire_m_window_fault_decode(CANDIDATE, &expected, &output, &sequence) == NET_WIRE_OK);
    CHECK(sequence == 0x5aa5u && equal_m_window_fault(&m, &output));
    m = example_m_window_fault();
    expected = literal_frame(golden_m_window_fault, sizeof(golden_m_window_fault));
    m.source_age_ms = UINT32_C(0);
    patch_field(&expected, 40u, 2u, UINT64_C(0));
    CHECK(net_wire_m_window_fault_encode(CANDIDATE, 0x5aa5u, &m, &actual) == NET_WIRE_OK);
    CHECK(actual.length == expected.length && memcmp(actual.bytes, expected.bytes, expected.length) == 0);
    CHECK(net_wire_m_window_fault_decode(CANDIDATE, &expected, &output, &sequence) == NET_WIRE_OK);
    CHECK(sequence == 0x5aa5u && equal_m_window_fault(&m, &output));
    m = example_m_window_fault();
    expected = literal_frame(golden_m_window_fault, sizeof(golden_m_window_fault));
    m.source_age_ms = UINT32_C(65535);
    patch_field(&expected, 40u, 2u, UINT64_C(65535));
    CHECK(net_wire_m_window_fault_encode(CANDIDATE, 0x5aa5u, &m, &actual) == NET_WIRE_OK);
    CHECK(actual.length == expected.length && memcmp(actual.bytes, expected.bytes, expected.length) == 0);
    CHECK(net_wire_m_window_fault_decode(CANDIDATE, &expected, &output, &sequence) == NET_WIRE_OK);
    CHECK(sequence == 0x5aa5u && equal_m_window_fault(&m, &output));
    return 0;
}

int main(int argc, char **argv)
{
    if (argc != 2) return 2;
    if (strcmp(argv[1], "m_bcm_door_state") == 0) return test_m_bcm_door_state();
    if (strcmp(argv[1], "m_bcm_climate_state") == 0) return test_m_bcm_climate_state();
    if (strcmp(argv[1], "m_bcm_light_state") == 0) return test_m_bcm_light_state();
    if (strcmp(argv[1], "m_bcm_status") == 0) return test_m_bcm_status();
    if (strcmp(argv[1], "m_cis_environment") == 0) return test_m_cis_environment();
    if (strcmp(argv[1], "m_cis_occupant") == 0) return test_m_cis_occupant();
    if (strcmp(argv[1], "m_cis_rear") == 0) return test_m_cis_rear();
    if (strcmp(argv[1], "m_cis_status") == 0) return test_m_cis_status();
    if (strcmp(argv[1], "m_vss_status") == 0) return test_m_vss_status();
    if (strcmp(argv[1], "m_window_state") == 0) return test_m_window_state();
    if (strcmp(argv[1], "m_window_fault") == 0) return test_m_window_fault();
    return 2;
}
