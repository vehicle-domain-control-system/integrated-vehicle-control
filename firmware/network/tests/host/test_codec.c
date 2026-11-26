#include "network/net_codec.h"

#include <stdio.h>
#include <string.h>

/* These are arbitrary HOST FIXTURE choices, not UART-M or UART-C values. */
static uint16_t fixture_checksum(const uint8_t *bytes, size_t length)
{
    uint16_t sum = 0u;
    uint16_t running = 0u;
    size_t i;
    for (i = 0u; i < length; ++i) {
        sum = (uint16_t)((sum + bytes[i]) & 0xffu);
        running = (uint16_t)((running + sum) & 0xffu);
    }
    return (uint16_t)(sum | (uint16_t)(running << 8u));
}

static NetUartFixtureProfile fixture_profile(void)
{
    NetUartFixtureProfile profile = {{0xd3u, 0x91u}, 32u, 10u, true, fixture_checksum};
    return profile;
}

static void set_times(uint64_t *times, size_t count, uint64_t at)
{
    size_t i;
    for (i = 0u; i < count; ++i) {
        times[i] = at;
    }
}

static NetUartStreamInput stream_input(const uint8_t *bytes, const uint64_t *times,
                                      size_t count, uint64_t now, bool timer)
{
    NetUartStreamInput input = {bytes, times, count, now, timer, true};
    return input;
}

#define CHECK(condition) do { \
    if (!(condition)) { \
        fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #condition); \
        return 1; \
    } \
} while (0)

static int test_can_capture_bounds(void)
{
    NetFrame frame = {0};
    frame.link = NET_LINK_CAN;
    frame.id = UINT32_C(0x1fffffff);
    frame.length = 64u;
    CHECK(net_can_validate_capture(&frame) == NET_OK);
    frame.length = 65u;
    CHECK(net_can_validate_capture(&frame) == NET_REJECTED);
    frame.length = NET_FRAME_CAPACITY + 1u;
    CHECK(net_can_validate_capture(&frame) == NET_REJECTED);
    frame.length = 0u;
    CHECK(net_can_validate_capture(&frame) == NET_OK);
    frame.id = UINT32_C(0x20000000);
    CHECK(net_can_validate_capture(&frame) == NET_REJECTED);
    frame.link = NET_LINK_UART_M;
    CHECK(net_can_validate_capture(&frame) == NET_INVALID_ARGUMENT);
    CHECK(net_can_validate_capture(NULL) == NET_INVALID_ARGUMENT);
    return 0;
}

static int test_explicit_configuration(void)
{
    NetUartFixtureProfile profile = fixture_profile();
    NetUartStream stream = {0};
    NetUartStream before;
    NetFrame out = {0};
    NetUartStreamInput input = stream_input(NULL, NULL, 0u, 0u, false);
    size_t consumed = 99u;
    CHECK(net_uart_stream_next(&stream, &input, &consumed, &out) == NET_NOT_CONFIGURED);
    CHECK(consumed == 0u);
    CHECK(net_uart_stream_init(&stream, NET_LINK_UART_M, NULL) == NET_NOT_CONFIGURED);
    CHECK(net_uart_stream_init(&stream, NET_LINK_CAN, &profile) == NET_INVALID_ARGUMENT);
    CHECK(net_uart_stream_init(&stream, NET_LINK_UART_M, &profile) == NET_OK);
    memcpy(&before, &stream, sizeof(before));
    profile.max_payload = NET_FRAME_CAPACITY;
    CHECK(net_uart_stream_init(&stream, NET_LINK_UART_M, &profile) == NET_INVALID_ARGUMENT);
    CHECK(memcmp(&stream, &before, sizeof(stream)) == 0);
    profile = fixture_profile();
    profile.checksum = NULL;
    CHECK(net_uart_stream_init(&stream, NET_LINK_UART_C, &profile) == NET_INVALID_ARGUMENT);
    profile = fixture_profile();
    profile.assembly_limit_ms = 0u;
    CHECK(net_uart_stream_init(&stream, NET_LINK_UART_M, &profile) == NET_INVALID_ARGUMENT);
    return 0;
}

static int test_fixture_golden_bytes_and_atomic_output(void)
{
    const uint8_t payload[] = {0x10u, 0x20u, 0x30u};
    /* Calculated independently: sum=99 (0x63), running sum=175 (0xaf). */
    const uint8_t expected[] = {0xd3u, 0x91u, 3u, 0u, 0x10u, 0x20u, 0x30u, 0x63u, 0xafu};
    NetUartFixtureProfile profile = fixture_profile();
    NetFrame out;
    NetFrame before;
    CHECK(net_uart_fixture_encode(&profile, NET_LINK_UART_M, payload,
                                   sizeof(payload), 40u, &out) == NET_OK);
    CHECK(out.length == sizeof(expected));
    CHECK(out.captured_at_ms == 40u && out.link == NET_LINK_UART_M);
    CHECK(memcmp(out.bytes, expected, sizeof(expected)) == 0);
    memcpy(&before, &out, sizeof(before));
    CHECK(net_uart_fixture_encode(NULL, NET_LINK_UART_M, payload, 3u, 1u, &out)
          == NET_NOT_CONFIGURED);
    CHECK(memcmp(&out, &before, sizeof(out)) == 0);
    CHECK(net_uart_fixture_encode(&profile, NET_LINK_UART_M, NULL, 1u, 1u, &out)
          == NET_INVALID_ARGUMENT);
    CHECK(memcmp(&out, &before, sizeof(out)) == 0);
    CHECK(net_uart_fixture_encode(&profile, NET_LINK_UART_M, payload, 33u, 1u, &out)
          == NET_REJECTED);
    CHECK(memcmp(&out, &before, sizeof(out)) == 0);
    return 0;
}

static int test_byte_fragments_and_worker_delay(void)
{
    const uint8_t golden[] = {0xd3u, 0x91u, 3u, 0u, 0x10u, 0x20u, 0x30u, 0x63u, 0xafu};
    NetUartFixtureProfile profile = fixture_profile();
    NetUartStream stream;
    NetFrame out;
    NetFrame before;
    size_t i;
    CHECK(net_uart_stream_init(&stream, NET_LINK_UART_M, &profile) == NET_OK);
    memset(&out, 0x6a, sizeof(out));
    memcpy(&before, &out, sizeof(before));
    for (i = 0u; i < sizeof(golden); ++i) {
        uint64_t at = 100u + i;
        size_t consumed = 0u;
        NetUartStreamInput input = stream_input(&golden[i], &at, 1u, 9999u,
                                               i + 1u == sizeof(golden));
        NetStatus status = net_uart_stream_next(&stream, &input, &consumed, &out);
        CHECK(consumed == 1u);
        if (i + 1u < sizeof(golden)) {
            CHECK(status == NET_NO_WORK);
            CHECK(memcmp(&out, &before, sizeof(out)) == 0);
        } else {
            CHECK(status == NET_OK);
        }
    }
    CHECK(out.captured_at_ms == 108u);
    CHECK(out.length == sizeof(golden));
    CHECK(memcmp(out.bytes, golden, sizeof(golden)) == 0);
    CHECK(stream.assembly_timeouts == 0u && stream.count == 0u);
    return 0;
}

static int test_concatenation_and_stable_output(void)
{
    NetUartFixtureProfile profile = fixture_profile();
    NetUartStream stream;
    NetFrame first;
    NetFrame second;
    NetFrame out;
    NetFrame stable;
    uint8_t one = 5u;
    uint8_t two = 9u;
    uint8_t batch[2u * NET_FRAME_CAPACITY];
    uint64_t times[2u * NET_FRAME_CAPACITY];
    NetUartStreamInput input;
    size_t consumed;
    CHECK(net_uart_fixture_encode(&profile, NET_LINK_UART_M, &one, 1u, 0u, &first) == NET_OK);
    CHECK(net_uart_fixture_encode(&profile, NET_LINK_UART_M, &two, 1u, 0u, &second) == NET_OK);
    memcpy(batch, first.bytes, first.length);
    memcpy(&batch[first.length], second.bytes, second.length);
    set_times(times, first.length, 20u);
    set_times(&times[first.length], second.length, 23u);
    input = stream_input(batch, times, first.length + second.length, 500u, true);
    CHECK(net_uart_stream_init(&stream, NET_LINK_UART_M, &profile) == NET_OK);
    CHECK(net_uart_stream_next(&stream, &input, &consumed, &out) == NET_OK);
    CHECK(consumed == first.length && out.captured_at_ms == 20u);
    stable = out;
    input = stream_input(&batch[consumed], &times[consumed], second.length, 500u, true);
    CHECK(net_uart_stream_next(&stream, &input, &consumed, &out) == NET_OK);
    CHECK(consumed == second.length && out.captured_at_ms == 23u);
    CHECK(out.bytes[4] == two && stable.bytes[4] == one);
    CHECK(memcmp(stable.bytes, first.bytes, first.length) == 0);
    return 0;
}

static int test_uart_m_c_independent_instances(void)
{
    NetUartFixtureProfile m_profile = fixture_profile();
    NetUartFixtureProfile c_profile = fixture_profile();
    NetUartStream m;
    NetUartStream c;
    NetFrame m_frame;
    NetFrame c_frame;
    NetFrame out;
    uint64_t times[NET_FRAME_CAPACITY];
    NetUartStreamInput input;
    size_t consumed;
    c_profile.sync[0] = 0x42u;
    c_profile.sync[1] = 0x17u;
    CHECK(net_uart_stream_init(&m, NET_LINK_UART_M, &m_profile) == NET_OK);
    CHECK(net_uart_stream_init(&c, NET_LINK_UART_C, &c_profile) == NET_OK);
    CHECK(net_uart_fixture_encode(&m_profile, NET_LINK_UART_M, NULL, 0u, 0u, &m_frame) == NET_OK);
    CHECK(net_uart_fixture_encode(&c_profile, NET_LINK_UART_C, NULL, 0u, 0u, &c_frame) == NET_OK);
    set_times(times, NET_FRAME_CAPACITY, 30u);
    input = stream_input(m_frame.bytes, times, 3u, 30u, false);
    CHECK(net_uart_stream_next(&m, &input, &consumed, &out) == NET_NO_WORK);
    input = stream_input(c_frame.bytes, times, c_frame.length, 30u, false);
    CHECK(net_uart_stream_next(&c, &input, &consumed, &out) == NET_OK);
    CHECK(out.link == NET_LINK_UART_C && m.count == 3u && c.count == 0u);
    input = stream_input(&m_frame.bytes[3], times, m_frame.length - 3u, 30u, false);
    CHECK(net_uart_stream_next(&m, &input, &consumed, &out) == NET_OK);
    CHECK(out.link == NET_LINK_UART_M && m.count == 0u);
    return 0;
}

static int test_noise_and_overlapping_sync(void)
{
    NetUartFixtureProfile profile = fixture_profile();
    NetUartStream stream;
    NetFrame frame;
    NetFrame out;
    uint8_t batch[NET_FRAME_CAPACITY];
    uint64_t times[NET_FRAME_CAPACITY];
    NetUartStreamInput input;
    size_t consumed;
    CHECK(net_uart_fixture_encode(&profile, NET_LINK_UART_M, NULL, 0u, 0u, &frame) == NET_OK);
    batch[0] = 0x7fu;
    batch[1] = profile.sync[0];
    memcpy(&batch[2], frame.bytes, frame.length);
    set_times(times, frame.length + 2u, 15u);
    input = stream_input(batch, times, frame.length + 2u, 15u, false);
    CHECK(net_uart_stream_init(&stream, NET_LINK_UART_M, &profile) == NET_OK);
    CHECK(net_uart_stream_next(&stream, &input, &consumed, &out) == NET_OK);
    CHECK(consumed == frame.length + 2u && stream.discarded_bytes == 2u);
    CHECK(memcmp(out.bytes, frame.bytes, frame.length) == 0);
    return 0;
}

static int test_bad_checksum_preserves_valid_suffix(void)
{
    NetUartFixtureProfile profile = fixture_profile();
    NetUartStream stream;
    NetFrame inner;
    NetFrame outer;
    NetFrame out;
    uint64_t times[NET_FRAME_CAPACITY];
    NetUartStreamInput input;
    size_t consumed;
    uint8_t payload = 0x11u;
    CHECK(net_uart_fixture_encode(&profile, NET_LINK_UART_M, &payload, 1u, 0u, &inner) == NET_OK);
    CHECK(net_uart_fixture_encode(&profile, NET_LINK_UART_M, inner.bytes, inner.length, 0u, &outer) == NET_OK);
    outer.bytes[outer.length - 1u] ^= 1u;
    set_times(times, outer.length, 100u);
    set_times(&times[4], inner.length, 102u);
    set_times(&times[4u + inner.length], 2u, 103u);
    input = stream_input(outer.bytes, times, outer.length, 9999u, true);
    CHECK(net_uart_stream_init(&stream, NET_LINK_UART_M, &profile) == NET_OK);
    CHECK(net_uart_stream_next(&stream, &input, &consumed, &out) == NET_OK);
    CHECK(consumed == outer.length && stream.bad_checksums == 1u);
    CHECK(out.length == inner.length && out.captured_at_ms == 102u);
    CHECK(memcmp(out.bytes, inner.bytes, inner.length) == 0);
    input = stream_input(NULL, NULL, 0u, 9999u, false);
    CHECK(net_uart_stream_next(&stream, &input, &consumed, &out) == NET_NO_WORK);
    CHECK(consumed == 0u && stream.count == 0u);
    return 0;
}

static int test_valid_boundary_keeps_payload_sync_opaque(void)
{
    NetUartFixtureProfile profile = fixture_profile();
    NetUartStream stream;
    NetFrame inner;
    NetFrame outer;
    NetFrame out;
    uint64_t times[NET_FRAME_CAPACITY];
    NetUartStreamInput input;
    size_t consumed;
    CHECK(net_uart_fixture_encode(&profile, NET_LINK_UART_M, NULL, 0u, 0u, &inner) == NET_OK);
    CHECK(net_uart_fixture_encode(&profile, NET_LINK_UART_M, inner.bytes, inner.length, 0u, &outer) == NET_OK);
    set_times(times, outer.length, 10u);
    input = stream_input(outer.bytes, times, outer.length, 10u, false);
    CHECK(net_uart_stream_init(&stream, NET_LINK_UART_M, &profile) == NET_OK);
    CHECK(net_uart_stream_next(&stream, &input, &consumed, &out) == NET_OK);
    CHECK(out.length == outer.length && stream.count == 0u);
    CHECK(memcmp(out.bytes, outer.bytes, outer.length) == 0);
    input = stream_input(NULL, NULL, 0u, 10u, false);
    CHECK(net_uart_stream_next(&stream, &input, &consumed, &out) == NET_NO_WORK);
    return 0;
}

static int test_bad_length_rejected_before_body(void)
{
    NetUartFixtureProfile profile = fixture_profile();
    NetUartStream stream;
    NetFrame valid;
    NetFrame out;
    uint8_t batch[NET_FRAME_CAPACITY];
    uint64_t times[NET_FRAME_CAPACITY];
    NetUartStreamInput input;
    size_t consumed;
    CHECK(net_uart_fixture_encode(&profile, NET_LINK_UART_M, NULL, 0u, 0u, &valid) == NET_OK);
    batch[0] = profile.sync[0];
    batch[1] = profile.sync[1];
    batch[2] = 0xffu;
    batch[3] = 0xffu;
    memcpy(&batch[4], valid.bytes, valid.length);
    set_times(times, valid.length + 4u, 10u);
    input = stream_input(batch, times, valid.length + 4u, 10u, false);
    CHECK(net_uart_stream_init(&stream, NET_LINK_UART_M, &profile) == NET_OK);
    CHECK(net_uart_stream_next(&stream, &input, &consumed, &out) == NET_OK);
    CHECK(consumed == valid.length + 4u && stream.malformed_lengths == 1u);
    CHECK(out.length == valid.length && stream.discarded_bytes == 4u);
    return 0;
}

static int test_timeout_suffix_keeps_capture_time(void)
{
    NetUartFixtureProfile profile = fixture_profile();
    NetUartStream stream;
    NetFrame inner;
    NetFrame out;
    uint8_t partial[NET_FRAME_CAPACITY];
    uint64_t times[NET_FRAME_CAPACITY];
    NetUartStreamInput input;
    size_t consumed;
    CHECK(net_uart_fixture_encode(&profile, NET_LINK_UART_M, NULL, 0u, 0u, &inner) == NET_OK);
    partial[0] = profile.sync[0];
    partial[1] = profile.sync[1];
    partial[2] = 32u;
    partial[3] = 0u;
    memcpy(&partial[4], inner.bytes, inner.length);
    set_times(times, 4u, 0u);
    set_times(&times[4], inner.length, 8u);
    input = stream_input(partial, times, inner.length + 4u, 8u, false);
    CHECK(net_uart_stream_init(&stream, NET_LINK_UART_M, &profile) == NET_OK);
    CHECK(net_uart_stream_next(&stream, &input, &consumed, &out) == NET_NO_WORK);
    CHECK(stream.count == inner.length + 4u);
    input = stream_input(NULL, NULL, 0u, 12u, true);
    CHECK(net_uart_stream_next(&stream, &input, &consumed, &out) == NET_OK);
    CHECK(consumed == 0u && stream.assembly_timeouts == 1u);
    CHECK(out.captured_at_ms == 8u && out.length == inner.length);
    return 0;
}

static int test_expiry_equality_is_explicit(void)
{
    NetUartFixtureProfile profile = fixture_profile();
    NetUartStream stream;
    NetFrame frame;
    NetFrame out;
    uint64_t times[NET_FRAME_CAPACITY];
    NetUartStreamInput input;
    size_t consumed;
    CHECK(net_uart_fixture_encode(&profile, NET_LINK_UART_M, NULL, 0u, 0u, &frame) == NET_OK);
    set_times(times, frame.length, 0u);
    times[frame.length - 1u] = 10u;
    input = stream_input(frame.bytes, times, frame.length, 1000u, true);
    CHECK(net_uart_stream_init(&stream, NET_LINK_UART_M, &profile) == NET_OK);
    CHECK(net_uart_stream_next(&stream, &input, &consumed, &out) == NET_NO_WORK);
    CHECK(stream.assembly_timeouts == 1u);
    profile.expires_at_equality = false;
    CHECK(net_uart_stream_init(&stream, NET_LINK_UART_M, &profile) == NET_OK);
    CHECK(net_uart_stream_next(&stream, &input, &consumed, &out) == NET_OK);
    CHECK(out.captured_at_ms == 10u && stream.assembly_timeouts == 0u);
    return 0;
}

static int test_ambiguous_or_backward_time_holds(void)
{
    NetUartFixtureProfile profile = fixture_profile();
    NetUartStream stream;
    NetFrame out;
    NetFrame before;
    uint8_t bytes[] = {0xd3u, 0x91u};
    uint64_t times[] = {10u, 9u};
    NetUartStreamInput input = stream_input(bytes, times, 2u, 10u, false);
    size_t consumed;
    CHECK(net_uart_stream_init(&stream, NET_LINK_UART_M, &profile) == NET_OK);
    memset(&out, 0x5a, sizeof(out));
    memcpy(&before, &out, sizeof(before));
    input.timestamps_unambiguous = false;
    CHECK(net_uart_stream_next(&stream, &input, &consumed, &out) == NET_HOLD);
    CHECK(consumed == 0u && stream.count == 0u);
    input.timestamps_unambiguous = true;
    CHECK(net_uart_stream_next(&stream, &input, &consumed, &out) == NET_HOLD);
    CHECK(consumed == 1u && stream.count == 1u);
    CHECK(memcmp(&out, &before, sizeof(out)) == 0);
    input = stream_input(NULL, NULL, 0u, 9u, true);
    CHECK(net_uart_stream_next(&stream, &input, &consumed, &out) == NET_HOLD);
    CHECK(stream.count == 1u);
    return 0;
}

static int test_gap_reset_and_counters(void)
{
    NetUartFixtureProfile profile = fixture_profile();
    NetUartStream stream;
    NetFrame frame;
    NetFrame out;
    uint64_t times[NET_FRAME_CAPACITY];
    NetUartStreamInput input;
    size_t consumed;
    CHECK(net_uart_fixture_encode(&profile, NET_LINK_UART_M, NULL, 0u, 0u, &frame) == NET_OK);
    CHECK(net_uart_stream_init(&stream, NET_LINK_UART_M, &profile) == NET_OK);
    set_times(times, frame.length, 90u);
    input = stream_input(frame.bytes, times, 3u, 90u, false);
    CHECK(net_uart_stream_next(&stream, &input, &consumed, &out) == NET_NO_WORK);
    stream.bad_checksums = 3u;
    CHECK(net_uart_stream_reset(&stream, true) == NET_OK);
    CHECK(stream.count == 0u && stream.byte_losses == 1u && stream.bad_checksums == 3u);
    set_times(times, frame.length, 1u);
    input = stream_input(&frame.bytes[3], times, frame.length - 3u, 1u, false);
    CHECK(net_uart_stream_next(&stream, &input, &consumed, &out) == NET_NO_WORK);
    input = stream_input(frame.bytes, times, frame.length, 1u, false);
    CHECK(net_uart_stream_next(&stream, &input, &consumed, &out) == NET_OK);
    stream.byte_losses = UINT32_MAX;
    CHECK(net_uart_stream_reset(&stream, true) == NET_OK);
    CHECK(stream.byte_losses == UINT32_MAX && stream.bad_checksums == 3u);
    return 0;
}

static int test_maximum_frame_and_bounds_guard(void)
{
    NetUartFixtureProfile profile = fixture_profile();
    NetUartStream stream;
    NetFrame frame;
    NetFrame out;
    uint8_t payload[NET_FRAME_CAPACITY - NET_UART_FIXTURE_OVERHEAD];
    uint64_t times[NET_FRAME_CAPACITY];
    NetUartStreamInput input;
    size_t consumed;
    memset(payload, 0x3c, sizeof(payload));
    profile.max_payload = sizeof(payload);
    CHECK(net_uart_fixture_encode(&profile, NET_LINK_UART_C, payload, sizeof(payload), 0u, &frame) == NET_OK);
    CHECK(frame.length == NET_FRAME_CAPACITY);
    CHECK(net_uart_stream_init(&stream, NET_LINK_UART_C, &profile) == NET_OK);
    set_times(times, frame.length, 15u);
    input = stream_input(frame.bytes, times, frame.length, 15u, false);
    CHECK(net_uart_stream_next(&stream, &input, &consumed, &out) == NET_OK);
    CHECK(consumed == NET_FRAME_CAPACITY && stream.count == 0u);
    CHECK(out.length == frame.length && memcmp(out.bytes, frame.bytes, frame.length) == 0);
    stream.count = NET_FRAME_CAPACITY + 1u;
    CHECK(net_uart_stream_next(&stream, &input, &consumed, &out) == NET_INVALID_ARGUMENT);
    CHECK(consumed == 0u);
    return 0;
}

static int test_null_input_and_checksum_reject_output(void)
{
    NetUartFixtureProfile profile = fixture_profile();
    NetUartStream stream;
    NetFrame frame;
    NetFrame out;
    NetFrame before;
    uint64_t times[NET_FRAME_CAPACITY];
    NetUartStreamInput input = stream_input(NULL, NULL, 1u, 1u, false);
    size_t consumed;
    CHECK(net_uart_stream_init(&stream, NET_LINK_UART_M, &profile) == NET_OK);
    CHECK(net_uart_stream_next(&stream, &input, &consumed, &out) == NET_INVALID_ARGUMENT);
    CHECK(consumed == 0u && stream.count == 0u);
    CHECK(net_uart_fixture_encode(&profile, NET_LINK_UART_M, NULL, 0u, 0u, &frame) == NET_OK);
    frame.bytes[frame.length - 1u] ^= 1u;
    set_times(times, frame.length, 1u);
    input = stream_input(frame.bytes, times, frame.length, 1u, false);
    memset(&out, 0x77, sizeof(out));
    memcpy(&before, &out, sizeof(before));
    CHECK(net_uart_stream_next(&stream, &input, &consumed, &out) == NET_NO_WORK);
    CHECK(stream.bad_checksums == 1u && consumed == frame.length);
    CHECK(memcmp(&out, &before, sizeof(out)) == 0);
    return 0;
}

int main(int argc, char **argv)
{
    const struct { const char *name; int (*run)(void); } tests[] = {
        {"can_capture_bounds", test_can_capture_bounds},
        {"explicit_configuration", test_explicit_configuration},
        {"fixture_golden_and_atomic_output", test_fixture_golden_bytes_and_atomic_output},
        {"byte_fragments_and_worker_delay", test_byte_fragments_and_worker_delay},
        {"concatenation_and_stable_output", test_concatenation_and_stable_output},
        {"uart_m_c_independent_instances", test_uart_m_c_independent_instances},
        {"noise_and_overlapping_sync", test_noise_and_overlapping_sync},
        {"bad_checksum_valid_suffix", test_bad_checksum_preserves_valid_suffix},
        {"valid_boundary_payload_sync_opaque", test_valid_boundary_keeps_payload_sync_opaque},
        {"bad_length_before_body", test_bad_length_rejected_before_body},
        {"timeout_suffix_original_capture", test_timeout_suffix_keeps_capture_time},
        {"explicit_expiry_equality", test_expiry_equality_is_explicit},
        {"ambiguous_or_backward_time_hold", test_ambiguous_or_backward_time_holds},
        {"gap_reset_and_saturating_counters", test_gap_reset_and_counters},
        {"maximum_frame_and_bounds_guard", test_maximum_frame_and_bounds_guard},
        {"null_input_and_checksum_reject_output", test_null_input_and_checksum_reject_output}
    };
    size_t i;
    size_t selected = 0u;
    if (argc > 2) {
        fprintf(stderr, "usage: %s [case_name]\n", argv[0]);
        return 1;
    }
    for (i = 0u; i < sizeof(tests) / sizeof(tests[0]); ++i) {
        if (argc == 2 && strcmp(argv[1], tests[i].name) != 0) {
            continue;
        }
        ++selected;
        if (tests[i].run() != 0) {
            fprintf(stderr, "FAIL %s\n", tests[i].name);
            return 1;
        }
        printf("PASS %s\n", tests[i].name);
    }
    if (selected == 0u) {
        fprintf(stderr, "unknown codec test case: %s\n", argv[1]);
        return 1;
    }
    printf("%lu codec host fixture scenarios passed; product wire contracts remain HOLD.\n",
           (unsigned long)selected);
    return 0;
}
