#include "network/net_wire_uart_dto.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CHECK(x) do { if (!(x)) { fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #x); return 1; } } while (0)
#define PROFILE NET_WIRE_CANDIDATE_ONLY

typedef union {
    NetWirePVisionResult vision;
    NetWirePPermission permission;
    NetWireMContext context;
    NetWireMRequest request;
    NetWireMResult result;
} Dto;

/* Independent new Wave1 fixtures, not the unavailable historic I1 vectors.
 * Generated from documented offsets with Python struct.pack('<...') and
 * binascii.crc_hqx(frame[2:], 0xffff), then frozen as literal full frames.
 * Sequence 0x5aa5 deliberately embeds SYNC in the header; CRC is little endian.
 */
static const uint8_t golden_vision[] = {
    0xa5U, 0x5aU, 0x01U, 0x60U, 0x13U, 0x00U, 0xa5U, 0x5aU,
    0x78U, 0x56U, 0x34U, 0x12U, 0xefU, 0xcdU, 0xabU, 0x90U,
    0x34U, 0x12U, 0x01U, 0x01U, 0x00U, 0x03U, 0x01U, 0x00U,
    0x02U, 0x00U, 0x00U, 0xa7U, 0x60U
};
static const uint8_t golden_permission[] = {
    0xa5U, 0x5aU, 0x01U, 0x62U, 0x0fU, 0x00U, 0xa5U, 0x5aU,
    0x78U, 0x56U, 0x34U, 0x12U, 0xefU, 0xcdU, 0xabU, 0x90U,
    0x01U, 0x00U, 0x00U, 0xdcU, 0x05U, 0xf3U, 0x01U, 0xcbU, 0xd2U
};
static const uint8_t golden_context[] = {
    0xa5U, 0x5aU, 0x02U, 0x01U, 0x26U, 0x00U, 0xa5U, 0x5aU,
    0x78U, 0x56U, 0x34U, 0x12U, 0xefU, 0xcdU, 0xabU, 0x89U,
    0x67U, 0x45U, 0x23U, 0x01U, 0x01U, 0x01U, 0x00U, 0x00U,
    0x40U, 0x30U, 0x20U, 0x10U, 0x34U, 0x12U, 0x01U, 0x01U,
    0x00U, 0x80U, 0x70U, 0x60U, 0x50U, 0x78U, 0x56U, 0x00U,
    0xefU, 0xcdU, 0xabU, 0x90U, 0xbcU, 0x9aU, 0x3dU, 0xfdU
};
static const uint8_t golden_request[] = {
    0xa5U, 0x5aU, 0x02U, 0x10U, 0x18U, 0x00U, 0xa5U, 0x5aU,
    0x78U, 0x56U, 0x34U, 0x12U, 0xefU, 0xcdU, 0xabU, 0x89U,
    0x67U, 0x45U, 0x23U, 0x01U, 0xefU, 0xcdU, 0xabU, 0x90U,
    0x02U, 0x01U, 0x2eU, 0xfbU, 0x00U, 0x00U, 0x00U, 0x00U,
    0x81U, 0x68U
};
static const uint8_t golden_result[] = {
    0xa5U, 0x5aU, 0x02U, 0x20U, 0x2cU, 0x00U, 0xa5U, 0x5aU,
    0x78U, 0x56U, 0x34U, 0x12U, 0x0cU, 0x00U, 0x00U, 0x00U,
    0x00U, 0x00U, 0x00U, 0x00U, 0x40U, 0x30U, 0x20U, 0x10U,
    0x80U, 0x70U, 0x60U, 0x50U, 0x07U, 0x00U, 0x00U, 0x00U,
    0x00U, 0x00U, 0x00U, 0x00U, 0xefU, 0xcdU, 0xabU, 0x90U,
    0x02U, 0x09U, 0x00U, 0x01U, 0x01U, 0x44U, 0x33U, 0x22U,
    0x11U, 0x01U, 0x34U, 0x12U, 0xa2U, 0x5dU
};
static const struct { const uint8_t *bytes; size_t length; } vectors[] = {
    {golden_vision, sizeof(golden_vision)}, {golden_permission, sizeof(golden_permission)},
    {golden_context, sizeof(golden_context)}, {golden_request, sizeof(golden_request)},
    {golden_result, sizeof(golden_result)}
};

/* Algebraic byte-at-a-time CCITT reference, independent of the production CRC. */
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

static void fix_crc(NetWireUartFrame *frame)
{
    uint16_t crc = reference_crc(frame->bytes + 2, frame->length - 4U);
    frame->bytes[frame->length - 2U] = (uint8_t)crc;
    frame->bytes[frame->length - 1U] = (uint8_t)(crc >> 8U);
}

static NetWireUartFrame fixture_frame(size_t kind)
{
    NetWireUartFrame frame = {0};
    frame.length = vectors[kind].length;
    frame.captured_at_ms = UINT64_C(9876543210);
    memcpy(frame.bytes, vectors[kind].bytes, frame.length);
    return frame;
}

static Dto fixture(size_t kind)
{
    Dto d;
    memset(&d, 0, sizeof(d));
    switch (kind) {
    case 0U:
        d.vision.pi_boot_id = 0x12345678U; d.vision.vision_sequence = 0x90abcdefU;
        d.vision.vision_age_ms = 0x1234U; d.vision.occupant_presence = 1U;
        d.vision.presence_validity = 1U; d.vision.occupant_count = 3U;
        d.vision.count_validity = 1U; d.vision.vision_status = 2U;
        break;
    case 1U:
        d.permission.cis_boot_id = 0x12345678U; d.permission.update_sequence = 0x90abcdefU;
        d.permission.vehicle_power_permission = 1U; d.permission.use_limit_ms = 1500U;
        d.permission.source_age_ms = 499U;
        break;
    case 2U:
        d.context.device_context_id = 0x12345678U;
        d.context.session_id = UINT64_C(0x0123456789abcdef);
        d.context.device_registration_state = 1U; d.context.bt_connection_state = 1U;
        d.context.app_update_sequence = 0x10203040U; d.context.app_age_ms = 0x1234U;
        d.context.proximity_state = 1U; d.context.proximity_validity = 1U;
        d.context.proximity_update_sequence = 0x50607080U; d.context.proximity_age_ms = 0x5678U;
        d.context.registration_connection_update_sequence = 0x90abcdefU;
        d.context.registration_connection_age_ms = 0x9abcU;
        break;
    case 3U:
        d.request.device_context_id = 0x12345678U;
        d.request.session_id = UINT64_C(0x0123456789abcdef); d.request.request_id = 0x90abcdefU;
        d.request.kind = 2U; d.request.operation = 1U;
        d.request.args[0] = 0x2eU; d.request.args[1] = 0xfbU; /* -1234 centi-C */
        break;
    default:
        d.result.device_context_id = 0x12345678U; d.result.session_id = 12U;
        d.result.domain_boot_id = 0x10203040U; d.result.query_id = 0x50607080U;
        d.result.request_session = 7U; d.result.request_id = 0x90abcdefU;
        d.result.result = 2U; d.result.result_reason = 9U; d.result.result_confirmation = 1U;
        d.result.command_target = 1U; d.result.command_id = 0x11223344U;
        d.result.origin = 1U; d.result.result_age_ms = 0x1234U;
        break;
    }
    return d;
}

static NetWireStatus encode(size_t kind, NetWireProfile profile, const Dto *d, NetWireUartFrame *out)
{
    switch (kind) {
    case 0U: return net_wire_p_vision_result_encode(profile, 0x5aa5U, d ? &d->vision : NULL, out);
    case 1U: return net_wire_p_permission_encode(profile, 0x5aa5U, d ? &d->permission : NULL, out);
    case 2U: return net_wire_m_context_encode(profile, 0x5aa5U, d ? &d->context : NULL, out);
    case 3U: return net_wire_m_request_encode(profile, 0x5aa5U, d ? &d->request : NULL, out);
    default: return net_wire_m_result_encode(profile, 0x5aa5U, d ? &d->result : NULL, out);
    }
}

static NetWireStatus decode(size_t kind, NetWireProfile profile, const NetWireUartFrame *f,
    Dto *out, uint16_t *sequence)
{
    switch (kind) {
    case 0U: return net_wire_p_vision_result_decode(profile, f, out ? &out->vision : NULL, sequence);
    case 1U: return net_wire_p_permission_decode(profile, f, out ? &out->permission : NULL, sequence);
    case 2U: return net_wire_m_context_decode(profile, f, out ? &out->context : NULL, sequence);
    case 3U: return net_wire_m_request_decode(profile, f, out ? &out->request : NULL, sequence);
    default: return net_wire_m_result_decode(profile, f, out ? &out->result : NULL, sequence);
    }
}

static int golden(size_t kind)
{
    Dto input = fixture(kind), output;
    NetWireUartFrame frame, independent = fixture_frame(kind);
    uint16_t sequence = 0U;
    uint16_t crc = reference_crc(independent.bytes + 2, independent.length - 4U);
    memset(&output, 0xc7, sizeof(output));
    CHECK(reference_crc((const uint8_t *)"123456789", 9U) == 0x29b1U);
    CHECK(independent.bytes[independent.length - 2U] == (uint8_t)crc);
    CHECK(independent.bytes[independent.length - 1U] == (uint8_t)(crc >> 8U));
    CHECK(encode(kind, PROFILE, &input, &frame) == NET_WIRE_OK);
    CHECK(frame.length == independent.length);
    CHECK(memcmp(frame.bytes, independent.bytes, frame.length) == 0);
    CHECK(decode(kind, PROFILE, &independent, &output, &sequence) == NET_WIRE_OK);
    CHECK(sequence == 0x5aa5U);
    CHECK(encode(kind, PROFILE, &output, &frame) == NET_WIRE_OK);
    CHECK(memcmp(frame.bytes, independent.bytes, frame.length) == 0);
    switch (kind) {
    case 0U:
        CHECK(output.vision.pi_boot_id == 0x12345678U && output.vision.vision_sequence == 0x90abcdefU);
        CHECK(output.vision.vision_age_ms == 0x1234U && output.vision.occupant_count == 3U);
        break;
    case 1U:
        CHECK(output.permission.cis_boot_id == 0x12345678U && output.permission.update_sequence == 0x90abcdefU);
        CHECK(output.permission.use_limit_ms == 1500U && output.permission.source_age_ms == 499U);
        break;
    case 2U:
        CHECK(output.context.session_id == UINT64_C(0x0123456789abcdef));
        CHECK(output.context.app_age_ms == 0x1234U && output.context.proximity_age_ms == 0x5678U);
        CHECK(output.context.registration_connection_age_ms == 0x9abcU);
        CHECK(output.context.registration_connection_update_sequence == 0x90abcdefU);
        break;
    case 3U: {
        int16_t temperature = 0;
        CHECK(net_wire_m_request_temperature(&output.request, &temperature) == NET_WIRE_OK);
        CHECK(temperature == -1234 && output.request.request_id == 0x90abcdefU);
        break;
    }
    default:
        CHECK(output.result.session_id == 12U && output.result.request_session == 7U);
        CHECK(output.result.domain_boot_id == 0x10203040U && output.result.query_id == 0x50607080U);
        CHECK(output.result.request_id == 0x90abcdefU && output.result.command_id == 0x11223344U);
        CHECK(output.result.result_age_ms == 0x1234U && output.result.origin == 1U);
        break;
    }
    return 0;
}

static int rejected(size_t kind, const Dto *input, size_t offset, uint32_t value,
    size_t width, NetWireStatus expected)
{
    NetWireUartFrame out, before, malformed = fixture_frame(kind);
    Dto decoded, decoded_before;
    uint16_t sequence = 0x9876U;
    size_t i;
    memset(&out, 0xa9, sizeof(out)); memcpy(&before, &out, sizeof(out));
    memset(&decoded, 0x7d, sizeof(decoded)); memcpy(&decoded_before, &decoded, sizeof(decoded));
    CHECK(encode(kind, PROFILE, input, &out) == expected);
    CHECK(memcmp(&out, &before, sizeof(out)) == 0);
    for (i = 0U; i < width; ++i) malformed.bytes[8U + offset + i] = (uint8_t)(value >> (8U * i));
    fix_crc(&malformed);
    CHECK(decode(kind, PROFILE, &malformed, &decoded, &sequence) == expected);
    CHECK(memcmp(&decoded, &decoded_before, sizeof(decoded)) == 0 && sequence == 0x9876U);
    return 0;
}

#define BAD(k, member, value, offset, width, status) do { Dto d = fixture(k); d.member = value; CHECK(rejected(k, &d, offset, value, width, status) == 0); } while (0)
static int negative(void)
{
    size_t kind;
    BAD(0U, vision.pi_boot_id, 0U, 0U, 4U, NET_WIRE_MALFORMED);
    BAD(0U, vision.occupant_presence, 2U, 10U, 1U, NET_WIRE_MALFORMED);
    BAD(0U, vision.presence_validity, 2U, 11U, 1U, NET_WIRE_MALFORMED);
    BAD(0U, vision.presence_reason, 10U, 12U, 1U, NET_WIRE_MALFORMED);
    BAD(0U, vision.occupant_count, 6U, 13U, 1U, NET_WIRE_MALFORMED);
    BAD(0U, vision.count_validity, 2U, 14U, 1U, NET_WIRE_MALFORMED);
    BAD(0U, vision.count_reason, 10U, 15U, 1U, NET_WIRE_MALFORMED);
    BAD(0U, vision.vision_status, 5U, 16U, 1U, NET_WIRE_MALFORMED);
    BAD(0U, vision.vision_fault_code, 5U, 17U, 2U, NET_WIRE_MALFORMED);
    BAD(0U, vision.vision_fault_code, 256U, 17U, 2U, NET_WIRE_MALFORMED);
    BAD(0U, vision.occupant_presence, 255U, 10U, 1U, NET_WIRE_CONTRACT_HOLD);
    BAD(0U, vision.occupant_presence, 0U, 10U, 1U, NET_WIRE_CONTRACT_HOLD);
    BAD(0U, vision.presence_reason, 1U, 12U, 1U, NET_WIRE_CONTRACT_HOLD);
    BAD(0U, vision.vision_status, 3U, 16U, 1U, NET_WIRE_CONTRACT_HOLD);
    BAD(0U, vision.vision_fault_code, 2U, 17U, 2U, NET_WIRE_CONTRACT_HOLD);
    BAD(1U, permission.cis_boot_id, 0U, 0U, 4U, NET_WIRE_MALFORMED);
    BAD(1U, permission.vehicle_power_permission, 2U, 8U, 1U, NET_WIRE_MALFORMED);
    BAD(1U, permission.value_quality, 4U, 9U, 1U, NET_WIRE_MALFORMED);
    BAD(1U, permission.quality_reason, 10U, 10U, 1U, NET_WIRE_MALFORMED);
    BAD(1U, permission.vehicle_power_permission, 255U, 8U, 1U, NET_WIRE_CONTRACT_HOLD);
    BAD(1U, permission.quality_reason, 6U, 10U, 1U, NET_WIRE_CONTRACT_HOLD);
    BAD(2U, context.device_context_id, 0U, 0U, 4U, NET_WIRE_MALFORMED);
    BAD(2U, context.device_registration_state, 2U, 12U, 1U, NET_WIRE_MALFORMED);
    BAD(2U, context.bt_connection_state, 2U, 13U, 1U, NET_WIRE_MALFORMED);
    BAD(2U, context.app_active_state, 2U, 14U, 1U, NET_WIRE_MALFORMED);
    BAD(2U, context.app_quality, 4U, 15U, 1U, NET_WIRE_MALFORMED);
    BAD(2U, context.proximity_state, 2U, 22U, 1U, NET_WIRE_MALFORMED);
    BAD(2U, context.proximity_validity, 2U, 23U, 1U, NET_WIRE_MALFORMED);
    BAD(2U, context.proximity_reason, 10U, 24U, 1U, NET_WIRE_MALFORMED);
    BAD(2U, context.registration_connection_quality, 4U, 31U, 1U, NET_WIRE_MALFORMED);
    BAD(2U, context.app_active_state, 255U, 14U, 1U, NET_WIRE_CONTRACT_HOLD);
    BAD(2U, context.device_registration_state, 255U, 12U, 1U, NET_WIRE_CONTRACT_HOLD);
    BAD(2U, context.proximity_state, 255U, 22U, 1U, NET_WIRE_CONTRACT_HOLD);
    BAD(3U, request.device_context_id, 0U, 0U, 4U, NET_WIRE_MALFORMED);
    BAD(3U, request.kind, 5U, 16U, 1U, NET_WIRE_MALFORMED);
    BAD(3U, request.operation, 4U, 17U, 1U, NET_WIRE_MALFORMED);
    BAD(3U, request.args[5], 1U, 23U, 1U, NET_WIRE_MALFORMED);
    BAD(4U, result.device_context_id, 0U, 0U, 4U, NET_WIRE_MALFORMED);
    BAD(4U, result.domain_boot_id, 0U, 12U, 4U, NET_WIRE_MALFORMED);
    BAD(4U, result.result, 6U, 32U, 1U, NET_WIRE_MALFORMED);
    BAD(4U, result.result, 255U, 32U, 1U, NET_WIRE_MALFORMED);
    BAD(4U, result.result_reason, 26U, 33U, 2U, NET_WIRE_MALFORMED);
    BAD(4U, result.result_confirmation, 2U, 35U, 1U, NET_WIRE_MALFORMED);
    BAD(4U, result.command_target, 3U, 36U, 1U, NET_WIRE_MALFORMED);
    BAD(4U, result.command_target, 0U, 36U, 1U, NET_WIRE_CONTRACT_HOLD);
    BAD(4U, result.origin, 6U, 41U, 1U, NET_WIRE_MALFORMED);
    for (kind = 0U; kind < 5U; ++kind) {
        NetWireUartFrame frame = fixture_frame(kind), saved;
        Dto input = fixture(kind), out, prior;
        uint16_t sequence = 77U;
        memset(&out, 0x86, sizeof(out)); memcpy(&prior, &out, sizeof(out));
        memcpy(&saved, &frame, sizeof(frame));
        CHECK(encode(kind, NET_WIRE_NOT_BOUND, &input, &frame) == NET_WIRE_NOT_BOUND_STATUS);
        CHECK(memcmp(&frame, &saved, sizeof(frame)) == 0);
        CHECK(decode(kind, NET_WIRE_NOT_BOUND, &frame, &out, &sequence) == NET_WIRE_NOT_BOUND_STATUS);
        CHECK(decode(kind, (NetWireProfile)99, &frame, &out, &sequence) == NET_WIRE_INVALID_ARGUMENT);
        frame.bytes[frame.length - 1U] ^= 0x80U;
        CHECK(decode(kind, PROFILE, &frame, &out, &sequence) != NET_WIRE_OK);
        CHECK(memcmp(&out, &prior, sizeof(out)) == 0 && sequence == 77U);
        frame = fixture_frame(kind); --frame.length;
        CHECK(decode(kind, PROFILE, &frame, &out, &sequence) != NET_WIRE_OK);
        CHECK(memcmp(&out, &prior, sizeof(out)) == 0 && sequence == 77U);
    }
    return 0;
}

static int arguments(void)
{
    static const struct { uint8_t kind, op, used, args[6]; } cases[] = {
        {1U, 0U, 0U, {0U}}, {1U, 1U, 0U, {0U}},
        {2U, 1U, 2U, {0x2eU, 0xfbU}}, {2U, 2U, 1U, {1U}}, {2U, 3U, 1U, {3U}},
        {3U, 1U, 1U, {1U}}, {3U, 2U, 1U, {100U}}, {3U, 3U, 3U, {0U, 128U, 255U}},
        {4U, 1U, 1U, {1U}}
    };
    size_t i, j;
    for (i = 0U; i < sizeof(cases) / sizeof(cases[0]); ++i) {
        Dto in = fixture(3U), out;
        NetWireUartFrame frame;
        uint16_t sequence = 0U;
        in.request.kind = cases[i].kind; in.request.operation = cases[i].op;
        memcpy(in.request.args, cases[i].args, 6U);
        CHECK(encode(3U, PROFILE, &in, &frame) == NET_WIRE_OK);
        CHECK(frame.bytes[24] == cases[i].kind && frame.bytes[25] == cases[i].op);
        CHECK(memcmp(frame.bytes + 26, cases[i].args, 6U) == 0);
        CHECK(decode(3U, PROFILE, &frame, &out, &sequence) == NET_WIRE_OK);
        CHECK(memcmp(out.request.args, cases[i].args, 6U) == 0);
        for (j = cases[i].used; j < 6U; ++j) {
            NetWireUartFrame bad = frame, before;
            Dto bad_in = in, before_out;
            bad_in.request.args[j] = 1U;
            memcpy(&before, &frame, sizeof(frame));
            CHECK(encode(3U, PROFILE, &bad_in, &frame) == NET_WIRE_MALFORMED);
            CHECK(memcmp(&before, &frame, sizeof(frame)) == 0);
            bad.bytes[26U + j] = 1U; fix_crc(&bad);
            memcpy(&before_out, &out, sizeof(out));
            CHECK(decode(3U, PROFILE, &bad, &out, &sequence) == NET_WIRE_MALFORMED);
            CHECK(memcmp(&before_out, &out, sizeof(out)) == 0);
        }
    }
    /* Every illegal operation and boundary boolean/fan/percentage argument. */
    for (i = 0U; i < 256U; ++i) {
        Dto in = fixture(3U);
        NetWireUartFrame frame;
        in.request.kind = 2U; in.request.operation = 3U;
        memset(in.request.args, 0, 6U); in.request.args[0] = (uint8_t)i;
        CHECK(encode(3U, PROFILE, &in, &frame) == (i <= 3U ? NET_WIRE_OK : NET_WIRE_MALFORMED));
        in.request.operation = 2U;
        CHECK(encode(3U, PROFILE, &in, &frame) == (i <= 1U ? NET_WIRE_OK : NET_WIRE_MALFORMED));
        in.request.kind = 3U;
        CHECK(encode(3U, PROFILE, &in, &frame) == (i <= 100U ? NET_WIRE_OK : NET_WIRE_MALFORMED));
        in.request.operation = 1U;
        CHECK(encode(3U, PROFILE, &in, &frame) == (i <= 1U ? NET_WIRE_OK : NET_WIRE_MALFORMED));
        in.request.kind = 4U;
        CHECK(encode(3U, PROFILE, &in, &frame) == (i <= 1U ? NET_WIRE_OK : NET_WIRE_MALFORMED));
    }
    /* All 65536 raw signed representations, including INT16_MIN, are byte
     * values only. No product setpoint range is invented. */
    for (i = 0U; i <= UINT16_MAX; ++i) {
        Dto in = fixture(3U), out;
        NetWireUartFrame frame;
        uint16_t sequence;
        int16_t temperature = 0;
        int32_t expected = i <= INT16_MAX ? (int32_t)i : (int32_t)i - INT32_C(65536);
        in.request.args[0] = (uint8_t)i; in.request.args[1] = (uint8_t)(i >> 8U);
        CHECK(net_wire_m_request_temperature(&in.request, &temperature) == NET_WIRE_OK);
        CHECK(temperature == expected);
        CHECK(encode(3U, PROFILE, &in, &frame) == NET_WIRE_OK);
        CHECK(decode(3U, PROFILE, &frame, &out, &sequence) == NET_WIRE_OK);
        CHECK(out.request.args[0] == (uint8_t)i && out.request.args[1] == (uint8_t)(i >> 8U));
    }
    return 0;
}

static int alias_atomicity(void)
{
    size_t kind;
    for (kind = 0U; kind < 5U; ++kind) {
        union { NetWireUartFrame frame; Dto dto; max_align_t align; } overlap, saved;
        Dto in = fixture(kind), out, before_out;
        NetWireUartFrame frame = fixture_frame(kind), before_frame;
        uint16_t sequence = 71U;
        memset(&overlap, 0x5c, sizeof(overlap)); memcpy(&saved, &overlap, sizeof(overlap));
        CHECK(encode(kind, PROFILE, &overlap.dto, &overlap.frame) == NET_WIRE_INVALID_ARGUMENT);
        CHECK(decode(kind, PROFILE, &overlap.frame, &overlap.dto, &sequence) == NET_WIRE_INVALID_ARGUMENT);
        CHECK(encode(kind, PROFILE, (const Dto *)(const void *)overlap.frame.bytes,
            &overlap.frame) == NET_WIRE_INVALID_ARGUMENT);
        CHECK(decode(kind, PROFILE, &overlap.frame,
            (Dto *)(void *)overlap.frame.bytes, &sequence) == NET_WIRE_INVALID_ARGUMENT);
        CHECK(memcmp(&overlap, &saved, sizeof(overlap)) == 0 && sequence == 71U);
        memset(&out, 0x9a, sizeof(out)); memcpy(&before_out, &out, sizeof(out));
        memcpy(&before_frame, &frame, sizeof(frame));
        CHECK(decode(kind, PROFILE, &frame, &out, (uint16_t *)(void *)&out) == NET_WIRE_INVALID_ARGUMENT);
        CHECK(decode(kind, PROFILE, &frame, &out, (uint16_t *)(void *)frame.bytes) == NET_WIRE_INVALID_ARGUMENT);
        CHECK(memcmp(&out, &before_out, sizeof(out)) == 0);
        CHECK(memcmp(&frame, &before_frame, sizeof(frame)) == 0);
        CHECK(encode(kind, PROFILE, NULL, &frame) == NET_WIRE_INVALID_ARGUMENT);
        CHECK(encode(kind, PROFILE, &in, NULL) == NET_WIRE_INVALID_ARGUMENT);
        CHECK(decode(kind, PROFILE, NULL, &out, &sequence) == NET_WIRE_INVALID_ARGUMENT);
        CHECK(decode(kind, PROFILE, &frame, NULL, &sequence) == NET_WIRE_INVALID_ARGUMENT);
        CHECK(decode(kind, PROFILE, &frame, &out, NULL) == NET_WIRE_INVALID_ARGUMENT);
        CHECK(memcmp(&out, &before_out, sizeof(out)) == 0 && sequence == 71U);
    }
    {
        NetWireMRequest request = fixture(3U).request, before;
        int16_t temperature = 35;
        memcpy(&before, &request, sizeof(before));
        CHECK(net_wire_m_request_temperature(&request, (int16_t *)(void *)&request) == NET_WIRE_INVALID_ARGUMENT);
        CHECK(memcmp(&request, &before, sizeof(request)) == 0);
        request.kind = 1U; request.operation = 0U; memset(request.args, 0, 6U);
        CHECK(net_wire_m_request_temperature(&request, &temperature) == NET_WIRE_UNSUPPORTED);
        CHECK(temperature == 35);
    }
    return 0;
}

static int sentinels(void)
{
    Dto d = fixture(0U), out;
    NetWireUartFrame frame, frame2;
    uint16_t sequence = 0U;
    size_t i;
    d.vision.occupant_presence = 0U; d.vision.occupant_count = 255U;
    d.vision.count_validity = 0U; d.vision.count_reason = 6U;
    d.vision.vision_age_ms = UINT16_MAX;
    CHECK(encode(0U, PROFILE, &d, &frame) == NET_WIRE_OK);
    CHECK(decode(0U, PROFILE, &frame, &out, &sequence) == NET_WIRE_OK);
    CHECK(out.vision.occupant_presence == 0U && out.vision.presence_validity == 1U);
    CHECK(out.vision.occupant_count == 255U && out.vision.vision_age_ms == UINT16_MAX);
    d.vision.occupant_presence = 255U; d.vision.presence_validity = 0U; d.vision.presence_reason = 6U;
    CHECK(encode(0U, PROFILE, &d, &frame) == NET_WIRE_OK);
    CHECK(decode(0U, PROFILE, &frame, &out, &sequence) == NET_WIRE_OK);
    CHECK(out.vision.occupant_presence == 255U && out.vision.occupant_count == 255U);
    d = fixture(1U);
    for (i = 499U; i <= 501U; ++i) {
        d.permission.source_age_ms = (uint16_t)i;
        CHECK(encode(1U, PROFILE, &d, &frame) == NET_WIRE_OK);
        CHECK(decode(1U, PROFILE, &frame, &out, &sequence) == NET_WIRE_OK);
        CHECK(out.permission.source_age_ms == i && out.permission.use_limit_ms == 1500U);
        CHECK(net_wire_p_permission_encode(PROFILE, 9U, &d.permission, &frame2) == NET_WIRE_OK);
        CHECK(memcmp(frame.bytes + 8, frame2.bytes + 8, 15U) == 0);
    }
    d.permission.use_limit_ms = 500U;
    CHECK(encode(1U, PROFILE, &d, &frame) == NET_WIRE_OK);
    CHECK(frame.bytes[19] == 0xf4U && frame.bytes[20] == 1U);
    d.permission.vehicle_power_permission = 255U; d.permission.value_quality = 3U;
    d.permission.quality_reason = 6U; d.permission.source_age_ms = UINT16_MAX;
    CHECK(encode(1U, PROFILE, &d, &frame) == NET_WIRE_OK);
    CHECK(decode(1U, PROFILE, &frame, &out, &sequence) == NET_WIRE_OK);
    CHECK(out.permission.vehicle_power_permission == 255U && out.permission.source_age_ms == UINT16_MAX);
    d = fixture(2U);
    d.context.app_active_state = 255U; d.context.app_quality = 3U; d.context.app_age_ms = UINT16_MAX;
    d.context.proximity_state = 255U; d.context.proximity_validity = 0U; d.context.proximity_reason = 6U;
    d.context.proximity_age_ms = UINT16_MAX; d.context.device_registration_state = 255U;
    d.context.bt_connection_state = 255U; d.context.registration_connection_quality = 3U;
    d.context.registration_connection_age_ms = UINT16_MAX;
    CHECK(encode(2U, PROFILE, &d, &frame) == NET_WIRE_OK);
    CHECK(decode(2U, PROFILE, &frame, &out, &sequence) == NET_WIRE_OK);
    CHECK(out.context.app_active_state == 255U && out.context.proximity_state == 255U);
    CHECK(out.context.app_age_ms == UINT16_MAX && out.context.registration_connection_age_ms == UINT16_MAX);
    d = fixture(4U);
    d.result.result = 255U; d.result.result_confirmation = 0U;
    d.result.result_age_ms = UINT16_MAX; d.result.command_target = 0U; d.result.command_id = 0U;
    CHECK(encode(4U, PROFILE, &d, &frame) == NET_WIRE_OK);
    CHECK(decode(4U, PROFILE, &frame, &out, &sequence) == NET_WIRE_OK);
    CHECK(out.result.result == 255U && out.result.result_confirmation == 0U);
    CHECK(out.result.request_session == 7U && out.result.session_id == 12U);
    CHECK(out.result.result_age_ms == UINT16_MAX && out.result.command_id == 0U);
    return 0;
}

int main(int argc, char **argv)
{
    static const char *names[] = {
        "p_vision_result_golden", "p_permission_golden", "m_context_golden",
        "m_request_golden", "m_result_golden"
    };
    size_t i;
    int result;
    if (argc != 2) return 2;
    for (i = 0U; i < 5U; ++i) if (strcmp(argv[1], names[i]) == 0) {
        result = golden(i); if (result == 0) printf("PASS %s\n", argv[1]); return result;
    }
    if (strcmp(argv[1], "uart_dto_negative") == 0) result = negative();
    else if (strcmp(argv[1], "m_request_arguments") == 0) result = arguments();
    else if (strcmp(argv[1], "uart_dto_alias_atomicity") == 0) result = alias_atomicity();
    else if (strcmp(argv[1], "uart_dto_sentinels") == 0) result = sentinels();
    else return 2;
    if (result == 0) printf("PASS %s\n", argv[1]);
    return result;
}
