#include "network/net_wire_can.h"
#include <stdio.h>
#include <string.h>

#define CHECK(condition) do { if (!(condition)) { \
    fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #condition); return 1; \
} } while (0)
#define CANDIDATE NET_WIRE_CANDIDATE_ONLY

/* New manually checked vectors. These literals are independent of Encode;
 * they are not the unverified historical I1/product golden vectors. */
static const uint8_t rear_golden[20] = {
    0x04,0x03,0x02,0x01, 0x44,0x33,0x22,0x11,
    0xd4,0xc3,0xb2,0xa1, 0x34,0x12, 0x7b,0x00, 0x01,0x00,0x00,0x00
};
static const uint8_t permission_golden[20] = {
    0x40,0x30,0x20,0x10, 0x80,0x70,0x60,0x50,
    0x01,0x00,0xff,0x00,0x03, 0xef,0xcd,0xab,0x89, 0xdc,0xfe,0x00
};

static NetWireCisRearRaw rear_example(void)
{
    NetWireCisRearRaw r = {0};
    r.boot_id = UINT32_C(0x01020304); r.tx_sequence = UINT32_C(0x11223344);
    r.update_sequence = UINT32_C(0xa1b2c3d4); r.source_age_ms = UINT16_C(0x1234);
    r.rear_distance_deci_cm = 123u; r.validity = 1u;
    return r;
}

static NetWireDomainPermissionRaw permission_example(void)
{
    NetWireDomainPermissionRaw p = {0};
    p.boot_id = UINT32_C(0x10203040); p.tx_sequence = UINT32_C(0x50607080);
    p.vehicle_power_permission = 1u; p.power_operation_state = UINT8_MAX;
    p.operation_quality = 3u; p.update_sequence = UINT32_C(0x89abcdef);
    p.source_age_ms = UINT16_C(0xfedc);
    return p;
}

static NetWireCanEnvelope literal_frame(uint32_t id, const uint8_t bytes[20])
{
    NetWireCanEnvelope f = {0};
    f.standard_id = id; f.fdf = true; f.brs = true; f.dlc = 11u; f.length = 20u;
    memcpy(f.bytes, bytes, 20u);
    return f;
}

static int rear_equal(const NetWireCisRearRaw *a, const NetWireCisRearRaw *b)
{
    return a->boot_id == b->boot_id && a->tx_sequence == b->tx_sequence &&
        a->update_sequence == b->update_sequence && a->source_age_ms == b->source_age_ms &&
        a->rear_distance_deci_cm == b->rear_distance_deci_cm &&
        a->validity == b->validity && a->quality_reason == b->quality_reason &&
        a->proximity_status == b->proximity_status;
}

static int permission_equal(const NetWireDomainPermissionRaw *a,
    const NetWireDomainPermissionRaw *b)
{
    return a->boot_id == b->boot_id && a->tx_sequence == b->tx_sequence &&
        a->vehicle_power_permission == b->vehicle_power_permission &&
        a->power_permission_quality == b->power_permission_quality &&
        a->power_operation_state == b->power_operation_state &&
        a->operation_allowed == b->operation_allowed &&
        a->operation_quality == b->operation_quality &&
        a->update_sequence == b->update_sequence && a->source_age_ms == b->source_age_ms;
}

static int golden_rear(void)
{
    NetWireCisRearRaw expected = rear_example(), decoded = {0};
    NetWireCanEnvelope encoded = {0}, from_literal = literal_frame(0x0a0u, rear_golden);
    CHECK(net_wire_can_decode_rear(CANDIDATE, &from_literal, &decoded) == NET_WIRE_OK);
    CHECK(rear_equal(&expected, &decoded));
    CHECK(decoded.rear_distance_deci_cm == 123u); /* Exactly 123 mm, not 12 or 120. */
    CHECK(net_wire_can_encode_rear(CANDIDATE, &expected, &encoded) == NET_WIRE_OK);
    CHECK(encoded.standard_id == 0x0a0u && encoded.dlc == 11u && encoded.length == 20u);
    CHECK(!encoded.extended && encoded.fdf && encoded.brs && !encoded.rtr && !encoded.driver_error);
    CHECK(memcmp(encoded.bytes, rear_golden, sizeof(rear_golden)) == 0);
    CHECK(encoded.bytes[19] == 0u); /* Padding, not application CRC. */
    from_literal.bytes[20] = 0xaau; /* Not part of actual 20-byte wire frame. */
    CHECK(net_wire_can_decode_rear(CANDIDATE, &from_literal, &decoded) == NET_WIRE_OK);
    CHECK(rear_equal(&expected, &decoded));
    return 0;
}

static int golden_permission(void)
{
    NetWireDomainPermissionRaw expected = permission_example(), decoded = {0};
    NetWireCanEnvelope encoded = {0}, from_literal = literal_frame(0x080u, permission_golden);
    CHECK(net_wire_can_decode_permission(CANDIDATE, &from_literal, &decoded) == NET_WIRE_OK);
    CHECK(permission_equal(&expected, &decoded));
    CHECK(net_wire_can_encode_permission(CANDIDATE, &expected, &encoded) == NET_WIRE_OK);
    CHECK(encoded.standard_id == 0x080u && encoded.dlc == 11u && encoded.length == 20u);
    CHECK(!encoded.extended && encoded.fdf && encoded.brs && !encoded.rtr && !encoded.driver_error);
    CHECK(memcmp(encoded.bytes, permission_golden, sizeof(permission_golden)) == 0);
    return 0;
}

static void corrupt_envelope(NetWireCanEnvelope *f, unsigned index)
{
    switch (index) {
    case 0: f->standard_id ^= 0x20u; break;
    case 1: f->extended = true; break;
    case 2: f->fdf = false; break;
    case 3: f->brs = false; break;
    case 4: f->rtr = true; break;
    case 5: f->driver_error = true; break;
    case 6: f->dlc = 10u; break;
    case 7: f->dlc = 12u; break;
    case 8: f->length = 19u; break;
    case 9: f->length = 21u; break;
    case 10: f->dlc = 20u; break; /* bytes are not DLC. */
    case 11: f->length = SIZE_MAX; break;
    case 12: f->bytes[19] = 1u; break;
    case 13: f->standard_id |= 0x800u; break;
    default: f->length = 0u; break;
    }
}

static int envelope_rejection(void)
{
    unsigned i;
    for (i = 0; i < 15u; ++i) {
        NetWireCanEnvelope r = literal_frame(0x0a0u, rear_golden);
        NetWireCanEnvelope p = literal_frame(0x080u, permission_golden);
        NetWireCisRearRaw rd, rb;
        NetWireDomainPermissionRaw pd, pb;
        memset(&rd, 0xa6, sizeof(rd)); memcpy(&rb, &rd, sizeof(rb));
        memset(&pd, 0xb7, sizeof(pd)); memcpy(&pb, &pd, sizeof(pb));
        corrupt_envelope(&r, i); corrupt_envelope(&p, i);
        CHECK(net_wire_can_decode_rear(CANDIDATE, &r, &rd) == NET_WIRE_MALFORMED);
        CHECK(net_wire_can_decode_permission(CANDIDATE, &p, &pd) == NET_WIRE_MALFORMED);
        CHECK(memcmp(&rd, &rb, sizeof(rd)) == 0);
        CHECK(memcmp(&pd, &pb, sizeof(pd)) == 0);
    }
    return 0;
}

static int rear_failure(const NetWireCisRearRaw *r, NetWireStatus expected)
{
    NetWireCanEnvelope f, before, raw = literal_frame(0x0a0u, rear_golden);
    NetWireCisRearRaw decoded, db;
    unsigned i;
    memset(&f, 0x82, sizeof(f)); memcpy(&before, &f, sizeof(before));
    memset(&decoded, 0x37, sizeof(decoded)); memcpy(&db, &decoded, sizeof(db));
    CHECK(net_wire_can_encode_rear(CANDIDATE, r, &f) == expected);
    CHECK(memcmp(&f, &before, sizeof(f)) == 0);
    /* Independent direct raw mutation tests decode rejection as well. */
    for (i = 0; i < 4u; ++i) raw.bytes[i] = (uint8_t)(r->boot_id >> (8u * i));
    raw.bytes[14] = (uint8_t)r->rear_distance_deci_cm;
    raw.bytes[15] = (uint8_t)(r->rear_distance_deci_cm >> 8u);
    raw.bytes[16] = r->validity; raw.bytes[17] = r->quality_reason;
    raw.bytes[18] = r->proximity_status;
    CHECK(net_wire_can_decode_rear(CANDIDATE, &raw, &decoded) == expected);
    CHECK(memcmp(&decoded, &db, sizeof(decoded)) == 0);
    return 0;
}

static int rear_semantics(void)
{
    NetWireCisRearRaw r = rear_example(), d = {0};
    NetWireCanEnvelope f = {0};
    unsigned i;
    r.rear_distance_deci_cm = UINT16_MAX;
    CHECK(rear_failure(&r, NET_WIRE_MALFORMED) == 0); /* VALID_DISTANCE + sentinel. */
    r.proximity_status = 1u; /* Canonical valid NO_OBJECT preserves sentinel. */
    CHECK(net_wire_can_encode_rear(CANDIDATE, &r, &f) == NET_WIRE_OK);
    CHECK(f.bytes[14] == 0xffu && f.bytes[15] == 0xffu && f.bytes[18] == 1u);
    CHECK(net_wire_can_decode_rear(CANDIDATE, &f, &d) == NET_WIRE_OK);
    CHECK(rear_equal(&r, &d));
    r.proximity_status = 3u; r.validity = 0u; r.quality_reason = 3u;
    r.source_age_ms = UINT16_MAX;
    CHECK(net_wire_can_encode_rear(CANDIDATE, &r, &f) == NET_WIRE_OK);
    CHECK(net_wire_can_decode_rear(CANDIDATE, &f, &d) == NET_WIRE_OK);
    CHECK(rear_equal(&r, &d)); /* FAULT is not rewritten to NO_OBJECT/CLEAR. */
    for (i = 0; i < 4u; ++i) {
        r = rear_example();
        if (i == 0u) r.boot_id = 0u;
        if (i == 1u) r.validity = 2u;
        if (i == 2u) r.quality_reason = 10u;
        if (i == 3u) r.proximity_status = 6u;
        CHECK(rear_failure(&r, NET_WIRE_MALFORMED) == 0);
    }
    for (i = 0; i < 4u; ++i) {
        r = rear_example();
        if (i == 0u) r.quality_reason = 3u;
        if (i == 1u) r.validity = 0u;
        if (i == 2u) r.proximity_status = 1u;
        if (i == 3u) r.proximity_status = 2u;
        CHECK(rear_failure(&r, NET_WIRE_CONTRACT_HOLD) == 0);
    }
    r = rear_example(); r.rear_distance_deci_cm = 0u;
    r.source_age_ms = UINT16_MAX; r.tx_sequence = 0u; r.update_sequence = UINT32_MAX;
    CHECK(net_wire_can_encode_rear(CANDIDATE, &r, &f) == NET_WIRE_OK);
    CHECK(net_wire_can_decode_rear(CANDIDATE, &f, &d) == NET_WIRE_OK);
    CHECK(rear_equal(&r, &d)); /* Encoding is not order/TTL acceptance. */
    return 0;
}

static int permission_raw(void)
{
    NetWireDomainPermissionRaw p, decoded, before;
    NetWireCanEnvelope f, fb, literal;
    unsigned i;
    for (i = 0; i < 6u; ++i) {
        p = permission_example();
        if (i == 0u) p.boot_id = 0u;
        if (i == 1u) p.vehicle_power_permission = 2u;
        if (i == 2u) p.power_permission_quality = 4u;
        if (i == 3u) p.power_operation_state = 254u;
        if (i == 4u) p.operation_allowed = 2u;
        if (i == 5u) p.operation_quality = 255u;
        memset(&f, 0x53, sizeof(f)); memcpy(&fb, &f, sizeof(fb));
        CHECK(net_wire_can_encode_permission(CANDIDATE, &p, &f) == NET_WIRE_MALFORMED);
        CHECK(memcmp(&f, &fb, sizeof(f)) == 0);
        literal = literal_frame(0x080u, permission_golden);
        if (i == 0u) memset(literal.bytes, 0, 4u);
        literal.bytes[8] = p.vehicle_power_permission;
        literal.bytes[9] = p.power_permission_quality;
        literal.bytes[10] = p.power_operation_state;
        literal.bytes[11] = p.operation_allowed;
        literal.bytes[12] = p.operation_quality;
        memset(&decoded, 0x91, sizeof(decoded)); memcpy(&before, &decoded, sizeof(before));
        CHECK(net_wire_can_decode_permission(CANDIDATE, &literal, &decoded) == NET_WIRE_MALFORMED);
        CHECK(memcmp(&decoded, &before, sizeof(decoded)) == 0);
    }
    p = permission_example(); p.source_age_ms = UINT16_MAX;
    p.power_permission_quality = 2u; p.operation_allowed = 1u;
    CHECK(net_wire_can_encode_permission(CANDIDATE, &p, &f) == NET_WIRE_OK);
    CHECK(net_wire_can_decode_permission(CANDIDATE, &f, &decoded) == NET_WIRE_OK);
    CHECK(permission_equal(&p, &decoded)); /* raw 1/INVALID is not ALLOW synthesis. */
    p.vehicle_power_permission = UINT8_MAX; p.power_permission_quality = 0u;
    CHECK(net_wire_can_encode_permission(CANDIDATE, &p, &f) == NET_WIRE_OK);
    CHECK(net_wire_can_decode_permission(CANDIDATE, &f, &decoded) == NET_WIRE_OK);
    CHECK(permission_equal(&p, &decoded)); /* UNKNOWN+OK projection stays external HOLD. */
    p.source_age_ms = 501u; p.update_sequence = 17u;
    CHECK(net_wire_can_encode_permission(CANDIDATE, &p, &f) == NET_WIRE_OK);
    p.tx_sequence += 1u;
    CHECK(net_wire_can_encode_permission(CANDIDATE, &p, &literal) == NET_WIRE_OK);
    CHECK(memcmp(f.bytes + 8u, literal.bytes + 8u, 12u) == 0);
    return 0;
}

static int arguments_alias(void)
{
    NetWireCisRearRaw r = rear_example(), rd, rb;
    NetWireDomainPermissionRaw p = permission_example(), pd, pb;
    NetWireCanEnvelope rf = literal_frame(0x0a0u, rear_golden);
    NetWireCanEnvelope pf = literal_frame(0x080u, permission_golden), f, fb;
    union { max_align_t align; uint8_t bytes[256]; } overlap;
    uint8_t saved[256];
    unsigned i;
    const NetWireProfile profiles[] = {NET_WIRE_NOT_BOUND, (NetWireProfile)99};
    const NetWireStatus statuses[] = {NET_WIRE_NOT_BOUND_STATUS, NET_WIRE_INVALID_ARGUMENT};
    memset(&f, 0x3b, sizeof(f)); memcpy(&fb, &f, sizeof(fb));
    memset(&rd, 0x2c, sizeof(rd)); memcpy(&rb, &rd, sizeof(rb));
    memset(&pd, 0x1d, sizeof(pd)); memcpy(&pb, &pd, sizeof(pb));
    for (i = 0; i < 2u; ++i) {
        CHECK(net_wire_can_encode_rear(profiles[i], &r, &f) == statuses[i]);
        CHECK(net_wire_can_decode_rear(profiles[i], &rf, &rd) == statuses[i]);
        CHECK(net_wire_can_encode_permission(profiles[i], &p, &f) == statuses[i]);
        CHECK(net_wire_can_decode_permission(profiles[i], &pf, &pd) == statuses[i]);
        CHECK(memcmp(&f, &fb, sizeof(f)) == 0);
        CHECK(memcmp(&rd, &rb, sizeof(rd)) == 0);
        CHECK(memcmp(&pd, &pb, sizeof(pd)) == 0);
    }
    CHECK(net_wire_can_encode_rear(CANDIDATE, NULL, &f) == NET_WIRE_INVALID_ARGUMENT);
    CHECK(net_wire_can_encode_rear(CANDIDATE, &r, NULL) == NET_WIRE_INVALID_ARGUMENT);
    CHECK(net_wire_can_decode_rear(CANDIDATE, NULL, &rd) == NET_WIRE_INVALID_ARGUMENT);
    CHECK(net_wire_can_decode_rear(CANDIDATE, &rf, NULL) == NET_WIRE_INVALID_ARGUMENT);
    CHECK(net_wire_can_encode_permission(CANDIDATE, NULL, &f) == NET_WIRE_INVALID_ARGUMENT);
    CHECK(net_wire_can_encode_permission(CANDIDATE, &p, NULL) == NET_WIRE_INVALID_ARGUMENT);
    CHECK(net_wire_can_decode_permission(CANDIDATE, NULL, &pd) == NET_WIRE_INVALID_ARGUMENT);
    CHECK(net_wire_can_decode_permission(CANDIDATE, &pf, NULL) == NET_WIRE_INVALID_ARGUMENT);
    CHECK(memcmp(&f, &fb, sizeof(f)) == 0);
    CHECK(memcmp(&rd, &rb, sizeof(rd)) == 0);
    CHECK(memcmp(&pd, &pb, sizeof(pd)) == 0);
    for (i = 0; i < 2u; ++i) {
        size_t offset = (size_t)i * _Alignof(max_align_t);
        NetWireCanEnvelope *out = (NetWireCanEnvelope *)(void *)overlap.bytes;
        NetWireCisRearRaw *rear = (NetWireCisRearRaw *)(void *)(overlap.bytes + offset);
        NetWireDomainPermissionRaw *permission = (NetWireDomainPermissionRaw *)(void *)(overlap.bytes + offset);
        memset(overlap.bytes, 0x69, sizeof(overlap.bytes));
        memcpy(saved, overlap.bytes, sizeof(saved));
        CHECK(net_wire_can_encode_rear(CANDIDATE, rear, out) == NET_WIRE_INVALID_ARGUMENT);
        CHECK(net_wire_can_decode_rear(CANDIDATE, out, rear) == NET_WIRE_INVALID_ARGUMENT);
        CHECK(net_wire_can_encode_permission(CANDIDATE, permission, out) == NET_WIRE_INVALID_ARGUMENT);
        CHECK(net_wire_can_decode_permission(CANDIDATE, out, permission) == NET_WIRE_INVALID_ARGUMENT);
        CHECK(memcmp(saved, overlap.bytes, sizeof(saved)) == 0);
    }
    return 0;
}

static uint32_t random_next(uint32_t *state)
{
    uint32_t x = *state;
    x ^= x << 13u; x ^= x >> 17u; x ^= x << 5u;
    *state = x; return x;
}

static int randomized(void)
{
    uint32_t rng = UINT32_C(0x4319a277);
    const uint8_t tri[] = {0u, 1u, UINT8_MAX};
    unsigned i;
    for (i = 0; i < 2000u; ++i) {
        NetWireCisRearRaw r = rear_example(), rd = {0};
        NetWireDomainPermissionRaw p = permission_example(), pd = {0};
        NetWireCanEnvelope frame = {0}, repeated = {0};
        r.boot_id = random_next(&rng) | 1u; r.tx_sequence = random_next(&rng);
        r.update_sequence = random_next(&rng); r.source_age_ms = (uint16_t)random_next(&rng);
        r.rear_distance_deci_cm = (uint16_t)(random_next(&rng) % UINT16_MAX);
        if ((i % 3u) != 0u) {
            r.rear_distance_deci_cm = UINT16_MAX;
            r.proximity_status = 1u;
            if ((i % 3u) == 2u) {r.proximity_status = 3u; r.validity = 0u; r.quality_reason = 3u;}
        }
        CHECK(net_wire_can_encode_rear(CANDIDATE, &r, &frame) == NET_WIRE_OK);
        CHECK(net_wire_can_decode_rear(CANDIDATE, &frame, &rd) == NET_WIRE_OK);
        CHECK(rear_equal(&r, &rd));
        CHECK(net_wire_can_encode_rear(CANDIDATE, &rd, &repeated) == NET_WIRE_OK);
        CHECK(memcmp(frame.bytes, repeated.bytes, 20u) == 0);
        p.boot_id = random_next(&rng) | 1u; p.tx_sequence = random_next(&rng);
        p.vehicle_power_permission = tri[random_next(&rng) % 3u];
        p.power_permission_quality = (uint8_t)(random_next(&rng) % 4u);
        p.power_operation_state = tri[random_next(&rng) % 3u];
        p.operation_allowed = (uint8_t)(random_next(&rng) % 2u);
        p.operation_quality = (uint8_t)(random_next(&rng) % 4u);
        p.update_sequence = random_next(&rng); p.source_age_ms = (uint16_t)random_next(&rng);
        CHECK(net_wire_can_encode_permission(CANDIDATE, &p, &frame) == NET_WIRE_OK);
        CHECK(net_wire_can_decode_permission(CANDIDATE, &frame, &pd) == NET_WIRE_OK);
        CHECK(permission_equal(&p, &pd));
        CHECK(net_wire_can_encode_permission(CANDIDATE, &pd, &repeated) == NET_WIRE_OK);
        CHECK(memcmp(frame.bytes, repeated.bytes, 20u) == 0);
    }
    puts("CAN candidate randomized seed=0x4319a277 iterations=2000 messages=4000");
    return 0;
}

int main(int argc, char **argv)
{
    struct Case { const char *name; int (*run)(void); };
    static const struct Case cases[] = {
        {"can_golden_rear", golden_rear}, {"can_golden_permission", golden_permission},
        {"can_envelope_rejection", envelope_rejection}, {"can_rear_semantics", rear_semantics},
        {"can_permission_raw", permission_raw}, {"can_arguments_alias", arguments_alias},
        {"can_randomized", randomized}
    };
    size_t i;
    int ran = 0;
    if (argc > 2) return 2;
    for (i = 0; i < sizeof(cases) / sizeof(cases[0]); ++i) {
        if (argc == 1 || strcmp(argv[1], cases[i].name) == 0) {
            if (cases[i].run() != 0) return 1;
            printf("PASS %s (Host structural/typed candidate only)\n", cases[i].name);
            ran = 1;
        }
    }
    return ran ? 0 : 2;
}
