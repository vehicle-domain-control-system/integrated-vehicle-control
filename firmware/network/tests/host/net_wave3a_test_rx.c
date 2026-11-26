#include "net_wave3a_test_rx.h"
#include "network/net_host_mock.h"
#include "network/net_runtime.h"
#include "network/net_wire_can.h"
#include <inttypes.h>
#include <stdio.h>
#include <string.h>

/* NON_PRODUCT_HOST_REFERENCE: literal from the supplied, SHA-checked
 * CAN_P01_P02_LITERAL_GOLDEN.json. No encoder generates this RX oracle.
 * Single owner, one pending RX, no ISR/driver/clock-domain product binding. */
static const uint8_t net_wave3a_test_rear_literal[20] = {
    0x04,0x03,0x02,0x01,0x44,0x33,0x22,0x11,0xd4,0xc3,
    0xb2,0xa1,0x34,0x12,0x7b,0x00,0x01,0x00,0x00,0x00
};

#define NET_WAVE3A_TEST_CHECK(x) do { if (!(x)) { \
    fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #x); return 1; \
} } while (0)

typedef enum {
    NET_WAVE3A_TEST_EMPTY, NET_WAVE3A_TEST_RESERVED,
    NET_WAVE3A_TEST_CALLBACK_DONE, NET_WAVE3A_TEST_DISPOSED
} net_wave3a_test_sidecar_state;

typedef struct {
    NetWireCanEnvelope envelope;
    uint64_t first_capture_ms, fixture_token;
    uint32_t clock_domain;
    net_wave3a_test_sidecar_state state;
} net_wave3a_test_sidecar;

typedef struct {
    NetWireCisRearRaw dto;
    uint64_t capture_ms, worker_ms, fixture_token;
    uint32_t clock_domain;
} net_wave3a_test_observer;

typedef struct {
    NetRuntime runtime;
    NetHostMock mock;
    net_wave3a_test_sidecar sidecar;
    net_wave3a_test_observer observer;
    const char *case_name;
    NetWireProfile profile;
    NetWireStatus wire_status;
    NetStatus local_status, nested_step, nested_capture;
    uint32_t worker_domain;
    uint64_t last_worker_ms;
    bool worker_seen, wire_called, glue_time_hold, quarantined;
    bool isolated_input_closed, reenter, decode_output_unchanged;
    unsigned raw_calls, typed_accepts, drains;
    int drain_result;
    uint32_t callback_id;
    size_t callback_length;
    uint64_t callback_capture_ms;
} net_wave3a_test_rx_fixture;

static bool net_wave3a_test_dto_equal(const NetWireCisRearRaw *a,
    const NetWireCisRearRaw *b)
{
    return a->boot_id == b->boot_id && a->tx_sequence == b->tx_sequence &&
        a->update_sequence == b->update_sequence &&
        a->source_age_ms == b->source_age_ms &&
        a->rear_distance_deci_cm == b->rear_distance_deci_cm &&
        a->validity == b->validity && a->quality_reason == b->quality_reason &&
        a->proximity_status == b->proximity_status;
}

static bool net_wave3a_test_observer_equal(const net_wave3a_test_observer *a,
    const net_wave3a_test_observer *b)
{
    return net_wave3a_test_dto_equal(&a->dto, &b->dto) &&
        a->capture_ms == b->capture_ms && a->worker_ms == b->worker_ms &&
        a->fixture_token == b->fixture_token && a->clock_domain == b->clock_domain;
}

/* Explicit field copies; neither struct layout nor padding is a contract. */
static NetWireCanEnvelope net_wave3a_test_copy_envelope(const NetWireCanEnvelope *in)
{
    NetWireCanEnvelope out = {0};
    size_t i;
    out.standard_id = in->standard_id;
    out.extended = in->extended; out.fdf = in->fdf; out.brs = in->brs;
    out.rtr = in->rtr; out.driver_error = in->driver_error;
    out.dlc = in->dlc; out.length = in->length;
    for (i = 0u; i < sizeof(out.bytes); ++i) out.bytes[i] = in->bytes[i];
    return out;
}

static NetWireCanEnvelope net_wave3a_test_rear_envelope(void)
{
    NetWireCanEnvelope out = {0};
    out.standard_id = 0x0a0u; out.dlc = 11u; out.length = 20u;
    out.fdf = true; out.brs = true;
    memcpy(out.bytes, net_wave3a_test_rear_literal, sizeof(net_wave3a_test_rear_literal));
    return out;
}

static NetFrame net_wave3a_test_raw_frame(const NetWireCanEnvelope *in,
    uint64_t capture_ms)
{
    NetFrame out = {0};
    size_t i;
    out.link = NET_LINK_CAN; out.id = in->standard_id;
    out.length = in->length; out.captured_at_ms = capture_ms;
    for (i = 0u; i < in->length && i < sizeof(in->bytes); ++i)
        out.bytes[i] = in->bytes[i];
    return out;
}

/* Explicit test-only status mapping, never an enum cast or C2 admission. */
static NetStatus net_wave3a_test_map_wire(NetWireStatus status)
{
    switch (status) {
    case NET_WIRE_OK: return NET_OK;
    case NET_WIRE_INVALID_ARGUMENT: return NET_INVALID_ARGUMENT;
    case NET_WIRE_NOT_BOUND_STATUS:
    case NET_WIRE_CONTRACT_HOLD:
    case NET_WIRE_TIME_HOLD: return NET_HOLD;
    case NET_WIRE_MALFORMED:
    case NET_WIRE_UNSUPPORTED: return NET_REJECTED;
    case NET_WIRE_NEED_MORE:
    case NET_WIRE_NO_CAPACITY: return NET_HOLD;
    default: return NET_HOLD;
    }
}

static void net_wave3a_test_trace(const net_wave3a_test_rx_fixture *f,
    const char *phase, NetStatus actual)
{
    const NetWireCanEnvelope *e = &f->sidecar.envelope;
    const NetWireCisRearRaw *d = &f->observer.dto;
    size_t i, records = 0u;
    for (i = 0u; i < NET_TX_CAPACITY; ++i) if (f->runtime.tx[i].used) ++records;
    printf("W3A_TRACE {\"test\":\"wave3a.%s\",\"phase\":\"%s\","
        "\"actual_return\":%d,\"local_status\":%d,\"wire_called\":%u,"
        "\"wire_status\":", f->case_name, phase, (int)actual,
        (int)f->local_status, f->wire_called ? 1u : 0u);
    if (f->wire_called) printf("%d", (int)f->wire_status); else printf("null");
    printf(",\"glue_time_hold\":%u,\"fixture_token\":%" PRIu64
        ",\"clock_domain\":%" PRIu32 ",\"worker_domain\":%" PRIu32
        ",\"capture_ms\":%" PRIu64 ",\"now_ms\":%" PRIu64
        ",\"last_worker_ms\":%" PRIu64 ",\"worker_seen\":%u"
        ",\"id\":%" PRIu32 ",\"length\":%zu,\"dlc\":%u,"
        "\"fdf\":%u,\"brs\":%u,\"rtr\":%u,\"extended\":%u,\"driver_error\":%u,"
        "\"raw_calls\":%u,\"typed_accepts\":%u,\"sidecar_state\":%d,"
        "\"quarantined\":%u,\"pending_events\":%zu,\"pending_rx\":%zu,"
        "\"drain_result\":%d,\"drain_steps\":%u,\"input_closed\":%u,"
        "\"borrow_count\":%zu,\"tx_records\":%zu,\"runtime_rx_delivered\":%" PRIu32
        ",\"runtime_rx_held\":%" PRIu32 ",\"runtime_rx_invalid\":%" PRIu32
        ",\"runtime_rx_dropped\":%" PRIu32 ",\"runtime_consumer_rejected\":%" PRIu32
        ",\"callback_id\":%" PRIu32 ",\"callback_length\":%zu,"
        "\"callback_capture_ms\":%" PRIu64 ",\"copied_boot\":%" PRIu32
        ",\"copied_tx\":%" PRIu32 ",\"copied_update\":%" PRIu32
        ",\"copied_age\":%u,\"copied_mm\":%u,\"copied_capture_ms\":%" PRIu64
        ",\"copied_worker_ms\":%" PRIu64 ",\"copied_token\":%" PRIu64
        ",\"decode_output_unchanged\":%u,\"nested_step\":%d,\"nested_capture\":%d}\n",
        f->glue_time_hold ? 1u : 0u, f->sidecar.fixture_token,
        f->sidecar.clock_domain, f->worker_domain, f->sidecar.first_capture_ms,
        f->mock.now_ms, f->last_worker_ms, f->worker_seen ? 1u : 0u,
        e->standard_id, e->length, (unsigned)e->dlc,
        e->fdf ? 1u : 0u, e->brs ? 1u : 0u, e->rtr ? 1u : 0u,
        e->extended ? 1u : 0u, e->driver_error ? 1u : 0u,
        f->raw_calls, f->typed_accepts, (int)f->sidecar.state,
        f->quarantined ? 1u : 0u, f->mock.event_count, f->runtime.rx_count,
        f->drain_result, f->drains, f->isolated_input_closed ? 1u : 0u,
        net_host_mock_pending_tx_count(&f->mock), records,
        f->runtime.observation.rx_delivered, f->runtime.observation.rx_held,
        f->runtime.observation.rx_invalid, f->runtime.observation.rx_dropped,
        f->runtime.observation.consumer_rejected, f->callback_id,
        f->callback_length, f->callback_capture_ms, d->boot_id, d->tx_sequence,
        d->update_sequence, (unsigned)d->source_age_ms,
        (unsigned)d->rear_distance_deci_cm, f->observer.capture_ms,
        f->observer.worker_ms, f->observer.fixture_token,
        f->decode_output_unchanged ? 1u : 0u, (int)f->nested_step,
        (int)f->nested_capture);
}

static NetStatus net_wave3a_test_receive(void *context, const NetFrame *raw)
{
    net_wave3a_test_rx_fixture *f = context;
    NetWireCanEnvelope envelope;
    NetWireCisRearRaw decoded;
    NetWireCisRearRaw before;
    uint64_t now;
    size_t i;
    if (f == NULL || raw == NULL) return NET_INVALID_ARGUMENT;
    ++f->raw_calls;
    f->callback_id = raw->id; f->callback_length = raw->length;
    f->callback_capture_ms = raw->captured_at_ms;
    f->wire_called = false; f->glue_time_hold = false;
    f->local_status = NET_HOLD;
    if (f->quarantined || f->sidecar.state != NET_WAVE3A_TEST_RESERVED ||
        f->sidecar.fixture_token == 0u) return f->local_status;
    /* This fixture supports exactly one raw input. A second queued frame
     * invalidates association even when its ID/time/bytes are identical.
     * Consume it without accepting either frame; never route metadata by ID. */
    if (!f->isolated_input_closed || f->runtime.rx_count != 0u ||
        f->mock.event_count != 0u) {
        f->sidecar.state = NET_WAVE3A_TEST_CALLBACK_DONE;
        return f->local_status;
    }
    /* Full raw association before any metadata is reconstructed. No IDs,
     * flags, timestamps, DLC or correlation values are guessed from bytes. */
    if (raw->link != NET_LINK_CAN || raw->id != f->sidecar.envelope.standard_id ||
        raw->length != f->sidecar.envelope.length || raw->length > 64u ||
        raw->captured_at_ms != f->sidecar.first_capture_ms) {
        f->sidecar.state = NET_WAVE3A_TEST_CALLBACK_DONE;
        f->local_status = NET_REJECTED;
        return f->local_status;
    }
    for (i = 0u; i < raw->length; ++i) {
        if (raw->bytes[i] != f->sidecar.envelope.bytes[i]) {
            f->sidecar.state = NET_WAVE3A_TEST_CALLBACK_DONE;
            f->local_status = NET_REJECTED;
            return f->local_status;
        }
    }
    if (net_port_now(&f->runtime.config.port, &now) != NET_OK ||
        f->worker_domain == 0u || f->sidecar.clock_domain == 0u ||
        f->worker_domain != f->sidecar.clock_domain ||
        now < f->sidecar.first_capture_ms ||
        (f->worker_seen && now < f->last_worker_ms)) {
        f->glue_time_hold = true;
        f->sidecar.state = NET_WAVE3A_TEST_CALLBACK_DONE;
        return f->local_status;
    }
    f->last_worker_ms = now; f->worker_seen = true;
    envelope = net_wave3a_test_copy_envelope(&f->sidecar.envelope);
    /* Runtime owns only these fields; explicit sidecar still supplies flags. */
    envelope.standard_id = raw->id; envelope.length = raw->length;
    for (i = 0u; i < raw->length; ++i) envelope.bytes[i] = raw->bytes[i];
    decoded = f->observer.dto; before = decoded;
    f->wire_called = true;
    f->wire_status = net_wire_can_decode_rear(f->profile, &envelope, &decoded);
    f->local_status = net_wave3a_test_map_wire(f->wire_status);
    f->decode_output_unchanged = net_wave3a_test_dto_equal(&before, &decoded);
    if (f->wire_status == NET_WIRE_OK) {
        /* Owned observer copies survive the callback. Never store raw pointer. */
        f->observer.dto = decoded;
        f->observer.capture_ms = raw->captured_at_ms;
        f->observer.worker_ms = now;
        f->observer.clock_domain = f->sidecar.clock_domain;
        f->observer.fixture_token = f->sidecar.fixture_token;
        ++f->typed_accepts;
        if (f->reenter) {
            f->nested_step = net_runtime_step(&f->runtime, 1u);
            f->nested_capture = net_runtime_capture_rx(&f->runtime, raw);
        }
    }
    f->sidecar.state = NET_WAVE3A_TEST_CALLBACK_DONE;
    return f->local_status;
}

static int net_wave3a_test_setup(net_wave3a_test_rx_fixture *f, const char *name)
{
    NetRuntimeConfig config = {0};
    memset(f, 0, sizeof(*f));
    net_host_mock_init(&f->mock);
    f->case_name = name; f->profile = NET_WIRE_CANDIDATE_ONLY;
    f->worker_domain = 37u; f->isolated_input_closed = true;
    f->local_status = NET_NO_WORK; f->nested_step = NET_NO_WORK;
    f->nested_capture = NET_NO_WORK; f->decode_output_unchanged = true;
    f->drain_result = -1;
    f->observer.dto.boot_id = UINT32_C(0xf1f2f3f4);
    f->observer.dto.tx_sequence = 17u; f->observer.dto.update_sequence = 23u;
    f->observer.dto.source_age_ms = 99u; f->observer.dto.rear_distance_deci_cm = 987u;
    f->observer.dto.validity = 7u; f->observer.dto.quality_reason = 8u;
    f->observer.dto.proximity_status = 9u;
    f->observer.capture_ms = 71u; f->observer.worker_ms = 72u;
    f->observer.fixture_token = 73u; f->observer.clock_domain = 74u;
    config.port = net_host_mock_port(&f->mock);
    config.local_epoch = UINT64_C(0x300a01);
    config.consumer = net_wave3a_test_receive; config.consumer_context = f;
    config.host_fixture_enabled = true;
    NET_WAVE3A_TEST_CHECK(net_runtime_init(&f->runtime, &config) == NET_OK);
    NET_WAVE3A_TEST_CHECK(net_host_mock_set_time(&f->mock, 1200u) == NET_OK);
    return 0;
}

static NetStatus net_wave3a_test_reserve(net_wave3a_test_rx_fixture *f,
    const NetWireCanEnvelope *envelope, uint64_t capture, uint32_t domain,
    uint64_t token)
{
    uint64_t now;
    NetStatus clock_status;
    if (envelope == NULL || envelope->length > sizeof(envelope->bytes) ||
        token == 0u) return NET_INVALID_ARGUMENT;
    if (f->quarantined || !f->isolated_input_closed || domain == 0u ||
        !f->runtime.config.host_fixture_enabled || f->runtime.config.consumer == NULL)
        return NET_HOLD;
    if (f->sidecar.state != NET_WAVE3A_TEST_EMPTY ||
        f->mock.event_count != 0u || f->runtime.rx_count != 0u ||
        net_host_mock_pending_tx_count(&f->mock) != 0u) return NET_BUSY;
    clock_status = net_port_now(&f->runtime.config.port, &now);
    if (clock_status != NET_OK) return clock_status;
    f->sidecar.envelope = net_wave3a_test_copy_envelope(envelope);
    f->sidecar.first_capture_ms = capture; f->sidecar.clock_domain = domain;
    f->sidecar.fixture_token = token;
    f->sidecar.state = NET_WAVE3A_TEST_RESERVED;
    f->last_worker_ms = now; f->worker_seen = true;
    return NET_OK;
}

/* Only this proof disposes a pending sidecar, including callback-free drops.
 * On failed proof retain evidence, quarantine fixture, and fail the test.
 * Eight slots + bounded poll means four serialized steps are sufficient for
 * the fixed isolated fixtures. No external producer is admitted during drain. */
static int net_wave3a_test_drain(net_wave3a_test_rx_fixture *f)
{
    unsigned attempt;
    NetStatus status = NET_NO_WORK;
    if (!f->isolated_input_closed) {
        f->quarantined = true;
        f->drain_result = 1;
        return 1;
    }
    for (attempt = 0u; attempt < 4u; ++attempt) {
        status = net_runtime_step(&f->runtime, NET_RX_CAPACITY);
        ++f->drains;
        net_wave3a_test_trace(f, "bounded_drain", status);
        if (f->mock.event_count == 0u && f->runtime.rx_count == 0u &&
            net_host_mock_pending_tx_count(&f->mock) == 0u &&
            !f->runtime.in_call) {
            f->sidecar.state = NET_WAVE3A_TEST_DISPOSED;
            f->drain_result = 0;
            return 0;
        }
    }
    f->quarantined = true;
    f->drain_result = 1;
    return 1;
}

static int net_wave3a_test_assert_golden(const net_wave3a_test_rx_fixture *f,
    uint64_t worker)
{
    const NetWireCisRearRaw *d = &f->observer.dto;
    NET_WAVE3A_TEST_CHECK(f->raw_calls == 1u && f->typed_accepts == 1u);
    NET_WAVE3A_TEST_CHECK(f->wire_called && f->wire_status == NET_WIRE_OK);
    NET_WAVE3A_TEST_CHECK(f->local_status == NET_OK);
    NET_WAVE3A_TEST_CHECK(d->boot_id == UINT32_C(0x01020304));
    NET_WAVE3A_TEST_CHECK(d->tx_sequence == UINT32_C(0x11223344));
    NET_WAVE3A_TEST_CHECK(d->update_sequence == UINT32_C(0xa1b2c3d4));
    NET_WAVE3A_TEST_CHECK(d->source_age_ms == 0x1234u);
    NET_WAVE3A_TEST_CHECK(d->rear_distance_deci_cm == 123u);
    NET_WAVE3A_TEST_CHECK(d->validity == 1u && d->quality_reason == 0u &&
        d->proximity_status == 0u);
    NET_WAVE3A_TEST_CHECK(f->observer.capture_ms == 1000u &&
        f->observer.worker_ms == worker && f->observer.clock_domain == 37u &&
        f->observer.fixture_token == 900001u);
    NET_WAVE3A_TEST_CHECK(f->runtime.observation.rx_delivered == 1u);
    NET_WAVE3A_TEST_CHECK(f->runtime.observation.tx_submitted == 0u &&
        net_host_mock_pending_tx_count(&f->mock) == 0u);
    return 0;
}

static int net_wave3a_test_golden(const char *name, bool lifetime, bool reentry)
{
    net_wave3a_test_rx_fixture f;
    NetWireCanEnvelope envelope = net_wave3a_test_rear_envelope();
    NetFrame raw = net_wave3a_test_raw_frame(&envelope, 1000u);
    net_wave3a_test_observer saved;
    NetStatus status;
    uint64_t worker = lifetime ? UINT64_C(900000) : UINT64_C(1200);
    NET_WAVE3A_TEST_CHECK(net_wave3a_test_setup(&f, name) == 0);
    f.reenter = reentry;
    status = net_wave3a_test_reserve(&f, &envelope, 1000u, 37u, 900001u);
    NET_WAVE3A_TEST_CHECK(status == NET_OK);
    net_wave3a_test_trace(&f, "reserve", status);
    status = net_host_mock_inject_rx(&f.mock, &raw);
    NET_WAVE3A_TEST_CHECK(status == NET_OK && f.raw_calls == 0u);
    net_wave3a_test_trace(&f, "injected_copy", status);
    if (lifetime) {
        raw.id = 0x777u; raw.length = 1u; raw.captured_at_ms = UINT64_MAX;
        memset(raw.bytes, 0x55, sizeof(raw.bytes));
        envelope.standard_id = 0x999u; envelope.fdf = false; envelope.dlc = 0u;
        memset(envelope.bytes, 0x66, sizeof(envelope.bytes));
        NET_WAVE3A_TEST_CHECK(f.sidecar.envelope.standard_id == 0xa0u &&
            f.sidecar.envelope.fdf && f.sidecar.envelope.dlc == 11u);
        NET_WAVE3A_TEST_CHECK(memcmp(f.sidecar.envelope.bytes,
            net_wave3a_test_rear_literal, 20u) == 0);
    }
    NET_WAVE3A_TEST_CHECK(net_host_mock_set_time(&f.mock, worker) == NET_OK);
    status = net_runtime_step(&f.runtime, 1u);
    net_wave3a_test_trace(&f, "callback_complete", status);
    NET_WAVE3A_TEST_CHECK(status == NET_OK);
    NET_WAVE3A_TEST_CHECK(net_wave3a_test_assert_golden(&f, worker) == 0);
    NET_WAVE3A_TEST_CHECK(f.sidecar.state == NET_WAVE3A_TEST_CALLBACK_DONE);
    if (reentry) {
        NET_WAVE3A_TEST_CHECK(f.nested_step == NET_BUSY && f.nested_capture == NET_BUSY);
        NET_WAVE3A_TEST_CHECK(!f.runtime.in_call && f.runtime.rx_count == 0u);
        NET_WAVE3A_TEST_CHECK(f.runtime.observation.rx_captured == 1u);
    }
    saved = f.observer;
    NET_WAVE3A_TEST_CHECK(net_wave3a_test_drain(&f) == 0);
    if (lifetime) {
        /* Overwrite released fixture-owned backing stores, not any borrow. */
        memset(f.mock.events, 0xa5, sizeof(f.mock.events));
        memset(f.runtime.rx, 0x5a, sizeof(f.runtime.rx));
        memset(f.sidecar.envelope.bytes, 0xcc, sizeof(f.sidecar.envelope.bytes));
        f.sidecar.first_capture_ms = 0u; f.sidecar.fixture_token = 0u;
        NET_WAVE3A_TEST_CHECK(net_wave3a_test_observer_equal(&saved, &f.observer));
        NET_WAVE3A_TEST_CHECK(net_wave3a_test_assert_golden(&f, worker) == 0);
    }
    net_wave3a_test_trace(&f, "final", status);
    return 0;
}

static int net_wave3a_test_missing_mismatch(const char *name)
{
    static const char *phases[] = {"missing_sidecar", "raw_wrong_id",
        "raw_wrong_length", "raw_wrong_capture", "raw_wrong_bytes",
        "raw_wrong_link", "duplicate_reserve", "ambiguous_duplicate_raw"};
    unsigned variant;
    for (variant = 0u; variant < 8u; ++variant) {
        net_wave3a_test_rx_fixture f;
        NetWireCanEnvelope e = net_wave3a_test_rear_envelope();
        NetFrame raw = net_wave3a_test_raw_frame(&e, 1000u);
        net_wave3a_test_observer before;
        NetStatus status;
        NET_WAVE3A_TEST_CHECK(net_wave3a_test_setup(&f, name) == 0);
        before = f.observer;
        if (variant != 0u) {
            NET_WAVE3A_TEST_CHECK(net_wave3a_test_reserve(&f, &e, 1000u, 37u, 900001u) == NET_OK);
        }
        if (variant == 1u) ++raw.id;
        if (variant == 2u) --raw.length;
        if (variant == 3u) ++raw.captured_at_ms;
        if (variant == 4u) raw.bytes[7] ^= 1u;
        if (variant == 5u) raw.link = NET_LINK_UART_M;
        if (variant == 6u) {
            status = net_wave3a_test_reserve(&f, &e, 1001u, 37u, 900002u);
            NET_WAVE3A_TEST_CHECK(status == NET_BUSY);
            NET_WAVE3A_TEST_CHECK(f.sidecar.first_capture_ms == 1000u &&
                f.sidecar.fixture_token == 900001u && f.typed_accepts == 0u);
            net_wave3a_test_trace(&f, phases[variant], status);
            /* No second injection: reject before raw append; drain original. */
            NET_WAVE3A_TEST_CHECK(net_host_mock_inject_rx(&f.mock, &raw) == NET_OK);
            status = net_runtime_step(&f.runtime, 1u);
            NET_WAVE3A_TEST_CHECK(status == NET_OK);
            NET_WAVE3A_TEST_CHECK(net_wave3a_test_assert_golden(&f, 1200u) == 0);
        } else {
            NET_WAVE3A_TEST_CHECK(net_host_mock_inject_rx(&f.mock, &raw) == NET_OK);
            if (variant == 7u)
                NET_WAVE3A_TEST_CHECK(net_host_mock_inject_rx(&f.mock, &raw) == NET_OK);
            status = net_runtime_step(&f.runtime, NET_RX_CAPACITY);
            NET_WAVE3A_TEST_CHECK(status == (variant == 0u || variant == 7u ? NET_HOLD : NET_REJECTED));
            NET_WAVE3A_TEST_CHECK(f.raw_calls == (variant == 7u ? 2u : 1u) &&
                f.typed_accepts == 0u && !f.wire_called);
            NET_WAVE3A_TEST_CHECK(net_wave3a_test_observer_equal(&before, &f.observer));
        }
        net_wave3a_test_trace(&f, phases[variant], status);
        NET_WAVE3A_TEST_CHECK(net_wave3a_test_drain(&f) == 0);
        net_wave3a_test_trace(&f, "final", status);
    }
    return 0;
}

static int net_wave3a_test_flags(const char *name)
{
    static const char *phases[] = {"wire_wrong_id", "wire_wrong_dlc", "wire_no_fdf",
        "wire_no_brs", "wire_rtr", "wire_extended", "wire_driver_error",
        "wire_nonzero_padding", "wire_short_length", "wire_long_length"};
    unsigned variant;
    for (variant = 0u; variant < 10u; ++variant) {
        net_wave3a_test_rx_fixture f;
        NetWireCanEnvelope e = net_wave3a_test_rear_envelope();
        NetFrame raw;
        net_wave3a_test_observer before;
        NetStatus status;
        NET_WAVE3A_TEST_CHECK(net_wave3a_test_setup(&f, name) == 0);
        before = f.observer;
        switch (variant) {
        case 0u: ++e.standard_id; break;
        case 1u: e.dlc = 10u; break;
        case 2u: e.fdf = false; break;
        case 3u: e.brs = false; break;
        case 4u: e.rtr = true; break;
        case 5u: e.extended = true; break;
        case 6u: e.driver_error = true; break;
        case 7u: e.bytes[19] = 1u; break;
        case 8u: e.length = 19u; break;
        default: e.length = 21u; break;
        }
        raw = net_wave3a_test_raw_frame(&e, 1000u);
        NET_WAVE3A_TEST_CHECK(net_wave3a_test_reserve(&f, &e, 1000u, 37u, 900001u) == NET_OK);
        NET_WAVE3A_TEST_CHECK(net_host_mock_inject_rx(&f.mock, &raw) == NET_OK);
        status = net_runtime_step(&f.runtime, 1u);
        net_wave3a_test_trace(&f, phases[variant], status);
        NET_WAVE3A_TEST_CHECK(status == NET_REJECTED && f.local_status == NET_REJECTED);
        NET_WAVE3A_TEST_CHECK(f.wire_called && f.wire_status == NET_WIRE_MALFORMED);
        NET_WAVE3A_TEST_CHECK(f.raw_calls == 1u && f.typed_accepts == 0u);
        NET_WAVE3A_TEST_CHECK(f.decode_output_unchanged &&
            net_wave3a_test_observer_equal(&before, &f.observer));
        NET_WAVE3A_TEST_CHECK(f.runtime.observation.consumer_rejected == 1u);
        NET_WAVE3A_TEST_CHECK(net_wave3a_test_drain(&f) == 0);
        net_wave3a_test_trace(&f, "final", status);
    }
    return 0;
}

static int net_wave3a_test_holds(const char *name)
{
    static const char *phases[] = {"profile_not_bound", "quality_contradiction",
        "boot_zero", "validity_range", "quality_range", "status_range",
        "no_object_sentinel", "unknown_distance_status", "invalid_without_reason"};
    unsigned variant;
    for (variant = 0u; variant < 9u; ++variant) {
        net_wave3a_test_rx_fixture f;
        NetWireCanEnvelope e = net_wave3a_test_rear_envelope();
        NetFrame raw;
        net_wave3a_test_observer before;
        NetWireStatus expected_wire = NET_WIRE_MALFORMED;
        NetStatus status;
        NET_WAVE3A_TEST_CHECK(net_wave3a_test_setup(&f, name) == 0);
        before = f.observer;
        switch (variant) {
        case 0u: f.profile = NET_WIRE_NOT_BOUND; expected_wire = NET_WIRE_NOT_BOUND_STATUS; break;
        case 1u: e.bytes[17] = 1u; expected_wire = NET_WIRE_CONTRACT_HOLD; break;
        case 2u: memset(e.bytes, 0, 4u); break;
        case 3u: e.bytes[16] = 2u; break;
        case 4u: e.bytes[17] = 10u; break;
        case 5u: e.bytes[18] = 6u; break;
        case 6u: e.bytes[14] = 0xffu; e.bytes[15] = 0xffu; break;
        case 7u: e.bytes[18] = 1u; expected_wire = NET_WIRE_CONTRACT_HOLD; break;
        default: e.bytes[16] = 0u; expected_wire = NET_WIRE_CONTRACT_HOLD; break;
        }
        raw = net_wave3a_test_raw_frame(&e, 1000u);
        NET_WAVE3A_TEST_CHECK(net_wave3a_test_reserve(&f, &e, 1000u, 37u, 900001u) == NET_OK);
        NET_WAVE3A_TEST_CHECK(net_host_mock_inject_rx(&f.mock, &raw) == NET_OK);
        status = net_runtime_step(&f.runtime, 1u);
        net_wave3a_test_trace(&f, phases[variant], status);
        NET_WAVE3A_TEST_CHECK(status == net_wave3a_test_map_wire(expected_wire));
        NET_WAVE3A_TEST_CHECK(f.wire_called && f.wire_status == expected_wire);
        NET_WAVE3A_TEST_CHECK(f.raw_calls == 1u && f.typed_accepts == 0u);
        NET_WAVE3A_TEST_CHECK(f.decode_output_unchanged &&
            net_wave3a_test_observer_equal(&before, &f.observer));
        NET_WAVE3A_TEST_CHECK(net_wave3a_test_drain(&f) == 0);
        net_wave3a_test_trace(&f, "final", status);
    }
    return 0;
}

static int net_wave3a_test_clock(const char *name)
{
    static const char *phases[] = {"worker_delay", "worker_before_capture",
        "worker_clock_backward", "worker_domain_unknown", "different_domain",
        "capture_domain_unknown", "maximum_worker_time"};
    unsigned variant;
    for (variant = 0u; variant < 7u; ++variant) {
        net_wave3a_test_rx_fixture f;
        NetWireCanEnvelope e = net_wave3a_test_rear_envelope();
        NetFrame raw = net_wave3a_test_raw_frame(&e, 1000u);
        net_wave3a_test_observer before;
        uint64_t now = 900000u;
        NetStatus status;
        NET_WAVE3A_TEST_CHECK(net_wave3a_test_setup(&f, name) == 0);
        before = f.observer;
        if (variant == 2u)
            NET_WAVE3A_TEST_CHECK(net_host_mock_set_time(&f.mock, 900001u) == NET_OK);
        NET_WAVE3A_TEST_CHECK(net_wave3a_test_reserve(&f, &e, 1000u, 37u, 900001u) == NET_OK);
        switch (variant) {
        case 1u: now = 999u; break;
        case 2u: break; /* reserve read actual Mock time 900001, now regresses */
        case 3u: f.worker_domain = 0u; break;
        case 4u: f.worker_domain = 38u; break;
        case 5u: f.sidecar.clock_domain = 0u; break; /* explicit corrupt metadata */
        case 6u: now = UINT64_MAX; break;
        default: break;
        }
        NET_WAVE3A_TEST_CHECK(net_host_mock_inject_rx(&f.mock, &raw) == NET_OK);
        NET_WAVE3A_TEST_CHECK(net_host_mock_set_time(&f.mock, now) == NET_OK);
        status = net_runtime_step(&f.runtime, 1u);
        net_wave3a_test_trace(&f, phases[variant], status);
        if (variant == 0u || variant == 6u) {
            NET_WAVE3A_TEST_CHECK(status == NET_OK);
            NET_WAVE3A_TEST_CHECK(net_wave3a_test_assert_golden(&f, now) == 0);
        } else {
            NET_WAVE3A_TEST_CHECK(status == NET_HOLD && f.glue_time_hold && !f.wire_called);
            NET_WAVE3A_TEST_CHECK(f.raw_calls == 1u && f.typed_accepts == 0u);
            NET_WAVE3A_TEST_CHECK(net_wave3a_test_observer_equal(&before, &f.observer));
        }
        NET_WAVE3A_TEST_CHECK(f.sidecar.first_capture_ms == 1000u);
        NET_WAVE3A_TEST_CHECK(f.sidecar.envelope.bytes[12] == 0x34u &&
            f.sidecar.envelope.bytes[13] == 0x12u);
        NET_WAVE3A_TEST_CHECK(net_wave3a_test_drain(&f) == 0);
        net_wave3a_test_trace(&f, "final", status);
    }
    return 0;
}

static int net_wave3a_test_capacity(const char *name)
{
    static const char *phases[] = {"second_pending", "mock_queue_full",
        "runtime_ring_full", "generic_invalid_id", "generic_invalid_length",
        "fixture_disabled", "consumer_null", "mock_invalid_argument",
        "reserve_unknown_domain", "reserve_unknown_token",
        "drain_unproven_input_closed", "drain_busy_exhausted"};
    unsigned variant;
    for (variant = 0u; variant < 12u; ++variant) {
        net_wave3a_test_rx_fixture f;
        NetWireCanEnvelope e = net_wave3a_test_rear_envelope();
        NetFrame raw = net_wave3a_test_raw_frame(&e, 1000u);
        net_wave3a_test_observer before;
        NetStatus status;
        size_t i;
        NET_WAVE3A_TEST_CHECK(net_wave3a_test_setup(&f, name) == 0);
        before = f.observer;
        if (variant >= 10u) {
            NET_WAVE3A_TEST_CHECK(net_wave3a_test_reserve(&f, &e, 1000u, 37u, 900001u) == NET_OK);
            NET_WAVE3A_TEST_CHECK(net_host_mock_inject_rx(&f.mock, &raw) == NET_OK);
            if (variant == 10u) f.isolated_input_closed = false;
            else f.runtime.in_call = true; /* explicit serialized-owner fault */
            /* Expected cleanup FAILURE is the observation. Do not restore,
             * reuse, or report the pending fixture as successfully drained. */
            NET_WAVE3A_TEST_CHECK(net_wave3a_test_drain(&f) == 1);
            NET_WAVE3A_TEST_CHECK(f.quarantined && f.drain_result == 1);
            NET_WAVE3A_TEST_CHECK(f.sidecar.state == NET_WAVE3A_TEST_RESERVED &&
                f.sidecar.fixture_token == 900001u && f.mock.event_count == 1u &&
                f.runtime.rx_count == 0u && net_host_mock_pending_tx_count(&f.mock) == 0u);
            NET_WAVE3A_TEST_CHECK(f.drains == (variant == 10u ? 0u : 4u));
            NET_WAVE3A_TEST_CHECK(f.raw_calls == 0u && f.typed_accepts == 0u &&
                net_wave3a_test_observer_equal(&before, &f.observer));
            status = net_wave3a_test_reserve(&f, &e, 1001u, 37u, 900002u);
            NET_WAVE3A_TEST_CHECK(status == NET_HOLD && f.sidecar.fixture_token == 900001u);
            net_wave3a_test_trace(&f, phases[variant], status);
            net_wave3a_test_trace(&f, "final", status);
            continue;
        }
        if (variant >= 8u) {
            status = net_wave3a_test_reserve(&f, &e, 1000u,
                variant == 8u ? 0u : 37u, variant == 9u ? 0u : 900001u);
            NET_WAVE3A_TEST_CHECK(status == (variant == 8u ? NET_HOLD : NET_INVALID_ARGUMENT));
            NET_WAVE3A_TEST_CHECK(f.sidecar.state == NET_WAVE3A_TEST_EMPTY);
            net_wave3a_test_trace(&f, phases[variant], status);
        } else {
            NET_WAVE3A_TEST_CHECK(net_wave3a_test_reserve(&f, &e, 1000u, 37u, 900001u) == NET_OK);
            if (variant == 0u) {
                status = net_wave3a_test_reserve(&f, &e, 1001u, 37u, 900002u);
                NET_WAVE3A_TEST_CHECK(status == NET_BUSY && f.mock.event_count == 0u);
                net_wave3a_test_trace(&f, phases[variant], status);
                /* Explicit disabled fixture permits callback-free drain proof. */
                f.runtime.config.host_fixture_enabled = false;
                NET_WAVE3A_TEST_CHECK(net_host_mock_inject_rx(&f.mock, &raw) == NET_OK);
                status = net_runtime_step(&f.runtime, 1u);
                NET_WAVE3A_TEST_CHECK(status == NET_HOLD);
            } else if (variant == 1u) {
                for (i = 0u; i < NET_HOST_EVENT_CAPACITY; ++i)
                    NET_WAVE3A_TEST_CHECK(net_host_mock_inject_error(&f.mock, (uint32_t)i) == NET_OK);
                status = net_host_mock_inject_rx(&f.mock, &raw);
                NET_WAVE3A_TEST_CHECK(status == NET_NO_CAPACITY &&
                    net_host_mock_dropped_events(&f.mock) == 1u);
            } else if (variant == 2u) {
                /* Deliberate isolated capacity fault injection; callbacks are
                 * disabled before queue fill, so no metadata reassociation. */
                f.runtime.config.host_fixture_enabled = false;
                for (i = 0u; i < NET_RX_CAPACITY; ++i)
                    NET_WAVE3A_TEST_CHECK(net_runtime_capture_rx(&f.runtime, &raw) == NET_OK);
                NET_WAVE3A_TEST_CHECK(net_host_mock_inject_rx(&f.mock, &raw) == NET_OK);
                status = net_runtime_step(&f.runtime, 0u);
                NET_WAVE3A_TEST_CHECK(status == NET_NO_CAPACITY &&
                    f.runtime.observation.rx_dropped == 1u);
            } else if (variant == 7u) {
                raw.length = NET_FRAME_CAPACITY + 1u;
                status = net_host_mock_inject_rx(&f.mock, &raw);
                NET_WAVE3A_TEST_CHECK(status == NET_INVALID_ARGUMENT);
            } else {
                if (variant == 3u) raw.id = UINT32_C(0x20000000);
                if (variant == 4u) raw.length = 65u;
                if (variant == 5u) f.runtime.config.host_fixture_enabled = false;
                if (variant == 6u) f.runtime.config.consumer = NULL;
                NET_WAVE3A_TEST_CHECK(net_host_mock_inject_rx(&f.mock, &raw) == NET_OK);
                status = net_runtime_step(&f.runtime, 1u);
                NET_WAVE3A_TEST_CHECK(status == (variant <= 4u ? NET_REJECTED : NET_HOLD));
            }
            net_wave3a_test_trace(&f, phases[variant], status);
            NET_WAVE3A_TEST_CHECK(f.sidecar.state == NET_WAVE3A_TEST_RESERVED);
        }
        NET_WAVE3A_TEST_CHECK(f.raw_calls == 0u && f.typed_accepts == 0u && !f.wire_called);
        NET_WAVE3A_TEST_CHECK(net_wave3a_test_observer_equal(&before, &f.observer));
        NET_WAVE3A_TEST_CHECK(net_wave3a_test_drain(&f) == 0);
        NET_WAVE3A_TEST_CHECK(f.mock.event_count == 0u && f.runtime.rx_count == 0u &&
            net_host_mock_pending_tx_count(&f.mock) == 0u && !f.quarantined);
        NET_WAVE3A_TEST_CHECK(f.raw_calls == 0u && f.typed_accepts == 0u &&
            net_wave3a_test_observer_equal(&before, &f.observer));
        NET_WAVE3A_TEST_CHECK(f.sidecar.state == NET_WAVE3A_TEST_DISPOSED);
        /* This single-flight fixture is not reused after disposal. */
        NET_WAVE3A_TEST_CHECK(net_wave3a_test_reserve(&f, &e, 1001u, 37u, 900002u) != NET_OK);
        net_wave3a_test_trace(&f, "final", status);
    }
    return 0;
}

int net_wave3a_test_rx(const char *case_name)
{
    if (case_name == NULL) return 2;
    if (strncmp(case_name, "wave3a.", 7u) == 0) case_name += 7;
    if (strcmp(case_name, "rear_golden_observer") == 0)
        return net_wave3a_test_golden(case_name, false, false);
    if (strcmp(case_name, "rear_copy_lifetime") == 0)
        return net_wave3a_test_golden(case_name, true, false);
    if (strcmp(case_name, "rear_sidecar_missing_or_mismatch") == 0)
        return net_wave3a_test_missing_mismatch(case_name);
    if (strcmp(case_name, "rear_envelope_flags") == 0)
        return net_wave3a_test_flags(case_name);
    if (strcmp(case_name, "rear_hold_and_not_bound") == 0)
        return net_wave3a_test_holds(case_name);
    if (strcmp(case_name, "rear_clock_and_backlog") == 0)
        return net_wave3a_test_clock(case_name);
    if (strcmp(case_name, "rear_singleflight_capacity") == 0)
        return net_wave3a_test_capacity(case_name);
    if (strcmp(case_name, "callback_reentry") == 0)
        return net_wave3a_test_golden(case_name, false, true);
    return 2;
}
