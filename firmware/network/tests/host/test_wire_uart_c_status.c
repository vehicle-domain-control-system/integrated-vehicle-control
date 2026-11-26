#include "network/net_wire_uart_c_status.h"
#include <stdio.h>
#include <string.h>

#define CHECK(x) do { if (!(x)) { fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #x); return 1; } } while (0)
#define PROFILE NET_WIRE_CANDIDATE_ONLY

/* Frozen document-driven Python struct.pack('<IIBHHHB') + binascii.crc_hqx.
 * READY/current NONE with retained last DATA_INVALID is deliberately valid.
 * Pi BOOT and LINK_SEQUENCE contain embedded A5 5A. */
static const uint8_t golden_vision_status[] = {
    0xa5U, 0x5aU, 0x01U, 0x61U, 0x10U, 0x00U, 0xa5U, 0x5aU,
    0x78U, 0xa5U, 0x5aU, 0x12U, 0xefU, 0xcdU, 0xabU, 0x90U,
    0x01U, 0x00U, 0x00U, 0x04U, 0x00U, 0x34U, 0x12U, 0x00U,
    0xe6U, 0x74U
};

static uint16_t reference_crc(const uint8_t *p, size_t length)
{
    uint32_t crc = 0xffffU;
    size_t i;
    for (i = 0U; i < length; ++i) {
        uint32_t x = (crc >> 8U) ^ p[i];
        x ^= x >> 4U;
        crc = ((crc << 8U) ^ (x << 12U) ^ (x << 5U) ^ x) & 0xffffU;
    }
    return (uint16_t)crc;
}

static void put_le(uint8_t *p, uint32_t value, size_t width)
{
    size_t i;
    for (i = 0U; i < width; ++i) p[i] = (uint8_t)(value >> (8U * i));
}

static void fix_crc(NetWireUartFrame *f)
{
    put_le(f->bytes + f->length - 2U,
        reference_crc(f->bytes + 2, f->length - 4U), 2U);
}

static NetWirePVisionStatusRaw fixture(void)
{
    NetWirePVisionStatusRaw m = {0};
    m.pi_boot_id = 0x125aa578U;
    m.status_sequence = 0x90abcdefU;
    m.vision_status = 1U;
    m.last_fault = 4U;
    m.status_age_ms = 0x1234U;
    return m;
}

static NetWireUartFrame fixture_frame(void)
{
    NetWireUartFrame f = {0};
    f.length = sizeof(golden_vision_status);
    f.captured_at_ms = UINT64_C(0x1122334455667788);
    memcpy(f.bytes, golden_vision_status, sizeof(golden_vision_status));
    return f;
}

/* This reference packs documented fields directly, without a codec call. */
static NetWireUartFrame reference_frame(const NetWirePVisionStatusRaw *m,
    uint16_t sequence)
{
    NetWireUartFrame f = fixture_frame();
    put_le(f.bytes + 6, sequence, 2U);
    put_le(f.bytes + 8, m->pi_boot_id, 4U);
    put_le(f.bytes + 12, m->status_sequence, 4U);
    f.bytes[16] = m->vision_status;
    put_le(f.bytes + 17, m->active_fault, 2U);
    put_le(f.bytes + 19, m->last_fault, 2U);
    put_le(f.bytes + 21, m->status_age_ms, 2U);
    f.bytes[23] = m->affected_function;
    fix_crc(&f);
    return f;
}

static int same_fields(const NetWirePVisionStatusRaw *a,
    const NetWirePVisionStatusRaw *b)
{
    return a->pi_boot_id == b->pi_boot_id &&
        a->status_sequence == b->status_sequence &&
        a->vision_status == b->vision_status &&
        a->active_fault == b->active_fault && a->last_fault == b->last_fault &&
        a->status_age_ms == b->status_age_ms &&
        a->affected_function == b->affected_function;
}

static int golden(void)
{
    NetWirePVisionStatusRaw m = fixture(), input_before = m, decoded;
    NetWireUartFrame expected = fixture_frame(), encoded, before = expected;
    uint16_t sequence = 0U;
    memset(&encoded, 0xa6, sizeof(encoded));
    CHECK(net_wire_p_vision_status_encode(PROFILE, 0x5aa5U, &m, &encoded) == NET_WIRE_OK);
    CHECK(encoded.length == sizeof(golden_vision_status));
    CHECK(memcmp(encoded.bytes, golden_vision_status, sizeof(golden_vision_status)) == 0);
    CHECK(encoded.captured_at_ms == 0U);
    CHECK(memcmp(&m, &input_before, sizeof(m)) == 0);
    memset(&decoded, 0xa6, sizeof(decoded));
    CHECK(net_wire_p_vision_status_decode(PROFILE, &expected, &decoded, &sequence) == NET_WIRE_OK);
    CHECK(same_fields(&decoded, &m) && sequence == 0x5aa5U);
    CHECK(memcmp(&expected, &before, sizeof(expected)) == 0);
    CHECK(decoded.active_fault == 0U && decoded.last_fault == 4U);
    return 0;
}

static int rejects(NetWirePVisionStatusRaw m, NetWireUartFrame f,
    NetWireStatus expected)
{
    NetWireUartFrame out, before, input_before = f;
    NetWirePVisionStatusRaw decoded, dto_before;
    uint16_t sequence = 0xbeefU;
    memset(&out, 0xa6, sizeof(out)); before = out;
    memset(&decoded, 0xb7, sizeof(decoded)); dto_before = decoded;
    CHECK(net_wire_p_vision_status_encode(PROFILE, 0x5aa5U, &m, &out) == expected);
    CHECK(memcmp(&out, &before, sizeof(out)) == 0);
    CHECK(net_wire_p_vision_status_decode(PROFILE, &f, &decoded, &sequence) == expected);
    CHECK(memcmp(&decoded, &dto_before, sizeof(decoded)) == 0 && sequence == 0xbeefU);
    CHECK(memcmp(&f, &input_before, sizeof(f)) == 0);
    return 0;
}

static int ranges(void)
{
    unsigned i;
    for (i = 0U; i < 8U; ++i) {
        NetWirePVisionStatusRaw m = fixture();
        switch (i) {
        case 0U: m.pi_boot_id = 0U; break;
        case 1U: m.vision_status = 5U; break;
        case 2U: m.vision_status = UINT8_MAX; break;
        case 3U: m.active_fault = 5U; break;
        case 4U: m.last_fault = UINT16_MAX; break;
        case 5U: m.affected_function = 4U; break;
        case 6U: m.active_fault = 0x0100U; break;
        default: m.affected_function = 0x80U; break;
        }
        CHECK(rejects(m, reference_frame(&m, 0x5aa5U), NET_WIRE_MALFORMED) == 0);
    }
    return 0;
}

static int framing(void)
{
    unsigned i;
    for (i = 0U; i < 11U; ++i) {
        NetWireUartFrame f = fixture_frame(), before;
        NetWirePVisionStatusRaw decoded, dto_before;
        uint16_t sequence = 0xbeefU;
        NetWireStatus status;
        memset(&decoded, 0xb7, sizeof(decoded)); dto_before = decoded;
        switch (i) {
        case 0U: f.bytes[0] ^= 1U; break;
        case 1U: f.bytes[1] ^= 1U; break;
        case 2U: f.bytes[2] = 2U; fix_crc(&f); break;
        case 3U: f.bytes[3] = 0x60U; fix_crc(&f); break;
        case 4U: f.bytes[3] = 0x62U; fix_crc(&f); break;
        case 5U: f.bytes[4] = 15U; fix_crc(&f); break;
        case 6U: f.bytes[5] = 1U; fix_crc(&f); break;
        case 7U: f.bytes[24] ^= 1U; break;
        case 8U: f.length = 25U; break;
        case 9U: f.length = NET_WIRE_UART_FRAME_MAX + 1U; break;
        default: f.bytes[3] = 0x7fU; fix_crc(&f); break;
        }
        before = f;
        status = net_wire_p_vision_status_decode(PROFILE, &f, &decoded, &sequence);
        CHECK(status == (i == 10U ? NET_WIRE_UNSUPPORTED : NET_WIRE_MALFORMED));
        CHECK(memcmp(&decoded, &dto_before, sizeof(decoded)) == 0 && sequence == 0xbeefU);
        CHECK(memcmp(&f, &before, sizeof(f)) == 0);
    }
    {
        NetWireUartFrame f = fixture_frame();
        NetWireUartPayload payload, before;
        memset(&payload, 0x95, sizeof(payload)); before = payload;
        CHECK(net_wire_uart_decode(PROFILE, NET_WIRE_UART_C, NET_WIRE_DOWNLINK,
            &f, &payload) == NET_WIRE_MALFORMED);
        CHECK(memcmp(&payload, &before, sizeof(payload)) == 0);
        CHECK(net_wire_uart_decode(PROFILE, NET_WIRE_UART_M, NET_WIRE_UPLINK,
            &f, &payload) == NET_WIRE_MALFORMED);
        CHECK(memcmp(&payload, &before, sizeof(payload)) == 0);
    }
    return 0;
}

static int arguments(void)
{
    NetWirePVisionStatusRaw m = fixture(), out, before;
    NetWireUartFrame f = fixture_frame(), frame_before = f, encoded;
    uint16_t sequence = 0xbeefU;
    memset(&out, 0xb7, sizeof(out)); before = out;
    encoded = f;
    CHECK(net_wire_p_vision_status_encode(PROFILE, 1U, NULL, &encoded) == NET_WIRE_INVALID_ARGUMENT);
    CHECK(net_wire_p_vision_status_encode(PROFILE, 1U, &m, NULL) == NET_WIRE_INVALID_ARGUMENT);
    CHECK(net_wire_p_vision_status_encode(NET_WIRE_NOT_BOUND, 1U, &m, &encoded) == NET_WIRE_NOT_BOUND_STATUS);
    CHECK(net_wire_p_vision_status_encode((NetWireProfile)99, 1U, &m, &encoded) == NET_WIRE_INVALID_ARGUMENT);
    CHECK(memcmp(&encoded, &frame_before, sizeof(encoded)) == 0);
    CHECK(net_wire_p_vision_status_decode(PROFILE, NULL, &out, &sequence) == NET_WIRE_INVALID_ARGUMENT);
    CHECK(net_wire_p_vision_status_decode(PROFILE, &f, NULL, &sequence) == NET_WIRE_INVALID_ARGUMENT);
    CHECK(net_wire_p_vision_status_decode(PROFILE, &f, &out, NULL) == NET_WIRE_INVALID_ARGUMENT);
    CHECK(net_wire_p_vision_status_decode(NET_WIRE_NOT_BOUND, &f, &out, &sequence) == NET_WIRE_NOT_BOUND_STATUS);
    CHECK(net_wire_p_vision_status_decode((NetWireProfile)99, &f, &out, &sequence) == NET_WIRE_INVALID_ARGUMENT);
    CHECK(memcmp(&out, &before, sizeof(out)) == 0 && sequence == 0xbeefU);
    CHECK(memcmp(&f, &frame_before, sizeof(f)) == 0);
    return 0;
}

static int overlap(void)
{
    union Storage { NetWireUartFrame frame; NetWirePVisionStatusRaw dto;
        uint64_t align; unsigned char bytes[2U * sizeof(NetWireUartFrame)]; } a, before;
    NetWirePVisionStatusRaw out, dto_before;
    uint16_t sequence = 0xbeefU;
    size_t offset;
    memset(&a, 0x96, sizeof(a)); a.frame = fixture_frame(); before = a;
    memset(&out, 0xb7, sizeof(out)); dto_before = out;
    for (offset = 0U; offset <= 8U; offset += 8U) {
        const NetWirePVisionStatusRaw *input = (const NetWirePVisionStatusRaw *)(const void *)(a.bytes + offset);
        NetWirePVisionStatusRaw *output = (NetWirePVisionStatusRaw *)(void *)(a.bytes + offset);
        CHECK(net_wire_p_vision_status_encode(PROFILE, 1U, input, &a.frame) == NET_WIRE_INVALID_ARGUMENT);
        CHECK(memcmp(&a, &before, sizeof(a)) == 0);
        CHECK(net_wire_p_vision_status_decode(PROFILE, &a.frame, output, &sequence) == NET_WIRE_INVALID_ARGUMENT);
        CHECK(memcmp(&a, &before, sizeof(a)) == 0 && sequence == 0xbeefU);
        CHECK(net_wire_p_vision_status_decode(PROFILE, &a.frame, &out,
            (uint16_t *)(void *)(a.bytes + offset)) == NET_WIRE_INVALID_ARGUMENT);
        CHECK(memcmp(&a, &before, sizeof(a)) == 0);
        CHECK(memcmp(&out, &dto_before, sizeof(out)) == 0);
    }
    CHECK(net_wire_p_vision_status_decode(PROFILE, &a.frame, &out,
        (uint16_t *)(void *)&out) == NET_WIRE_INVALID_ARGUMENT);
    CHECK(net_wire_p_vision_status_decode(PROFILE, &a.frame, &out,
        &out.status_age_ms) == NET_WIRE_INVALID_ARGUMENT);
    CHECK(memcmp(&out, &dto_before, sizeof(out)) == 0);
    CHECK(memcmp(&a, &before, sizeof(a)) == 0);
    return 0;
}

static int boundaries(void)
{
    NetWirePVisionStatusRaw m = fixture(), decoded;
    NetWireUartFrame expected, encoded;
    uint16_t sequence;
    unsigned state, active, last, affected, limit;
    /* Independent valid enum ranges: no invented cross-field policy. A READY
     * report cannot mutate a historical P_VISION_RESULT through this API. */
    for (state = 0U; state <= 4U; ++state)
        for (active = 0U; active <= 4U; ++active)
            for (last = 0U; last <= 4U; ++last)
                for (affected = 0U; affected <= 3U; ++affected)
                    for (limit = 0U; limit < 2U; ++limit) {
                        m.pi_boot_id = limit == 0U ? 1U : UINT32_MAX;
                        m.status_sequence = limit == 0U ? 0U : UINT32_MAX;
                        m.vision_status = (uint8_t)state;
                        m.active_fault = (uint16_t)active;
                        m.last_fault = (uint16_t)last;
                        m.affected_function = (uint8_t)affected;
                        m.status_age_ms = limit == 0U ? 0U : UINT16_MAX;
                        sequence = limit == 0U ? 0U : UINT16_MAX;
                        expected = reference_frame(&m, sequence);
                        CHECK(net_wire_p_vision_status_encode(PROFILE, sequence, &m, &encoded) == NET_WIRE_OK);
                        CHECK(encoded.length == expected.length);
                        CHECK(memcmp(encoded.bytes, expected.bytes, expected.length) == 0);
                        CHECK(net_wire_p_vision_status_decode(PROFILE, &expected, &decoded, &sequence) == NET_WIRE_OK);
                        CHECK(same_fields(&m, &decoded));
                        CHECK(sequence == (limit == 0U ? 0U : UINT16_MAX));
                    }
    return 0;
}

int main(int argc, char **argv)
{
    int result;
    if (argc != 2) return 2;
    if (strcmp(argv[1], "p_vision_status_golden") == 0) result = golden();
    else if (strcmp(argv[1], "p_vision_status_ranges") == 0) result = ranges();
    else if (strcmp(argv[1], "p_vision_status_framing") == 0) result = framing();
    else if (strcmp(argv[1], "p_vision_status_arguments") == 0) result = arguments();
    else if (strcmp(argv[1], "p_vision_status_overlap") == 0) result = overlap();
    else if (strcmp(argv[1], "p_vision_status_boundaries") == 0) result = boundaries();
    else return 2;
    if (result == 0) printf("PASS %s\n", argv[1]);
    return result;
}
