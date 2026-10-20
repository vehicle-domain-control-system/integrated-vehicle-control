#include "net_wave3a_test_tx.h"
#include "network/net_host_mock.h"
#include "network/net_runtime.h"
#include "network/net_wire_can.h"
#include <stdio.h>
#include <string.h>

#define CHECK(x) do { if (!(x)) { \
    fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #x); return 1; \
} } while (0)
#define SIDECAR_CAPACITY 6u
#define CANDIDATE NET_WIRE_CANDIDATE_ONLY

/* Independent frozen LOCKED_SEEDS/CAN_P01_P02_LITERAL_GOLDEN.json bytes.
 * Raw DENY is a manually fixed test fixture; raw1 is a separate historical
 * Host structural vector. Neither is a product permission/safety decision. */
static const uint8_t deny_literal[20] = {
    0x40,0x30,0x20,0x10,0x80,0x70,0x60,0x50,0x00,0x00,
    0xff,0x00,0x03,0xef,0xcd,0xab,0x89,0xdc,0xfe,0x00
};
static const uint8_t raw1_literal[20] = {
    0x40,0x30,0x20,0x10,0x80,0x70,0x60,0x50,0x01,0x00,
    0xff,0x00,0x03,0xef,0xcd,0xab,0x89,0xdc,0xfe,0x00
};

typedef enum { SIDECAR_FREE, SIDECAR_RESERVED, SIDECAR_ATTACHED,
               SIDECAR_RETIRED } SidecarState;
typedef struct {
    SidecarState state;
    NetTxKey key;
    NetWireCanEnvelope envelope;
    NetSourceStamp source;
    NetAgePolicy policy;
    uint64_t capture_ms;
    uint16_t wire_age_ms;
    const char *literal_id;
} TestSidecar;
typedef struct {
    NetHostMock mock;
    NetRuntime runtime;
    NetPort delegate;
    size_t port_submit_calls, runtime_submit_calls;
    NetStatus now_status;
    TestSidecar sidecars[SIDECAR_CAPACITY];
    NetWireDomainPermissionRaw raw;
    NetSourceStamp source;
    NetAgePolicy policy;
    uint64_t capture_ms;
    NetWireStatus last_wire;
    NetStatus last_local, last_retire;
    NetWireCanEnvelope output;
    NetFrame caller_frame;
    const char *case_name;
} TxFixture;

static NetStatus counted_now(void *context, uint64_t *out)
{
    TxFixture *f = context;
    if (f->now_status != NET_OK) return f->now_status;
    return net_port_now(&f->delegate, out);
}
static NetStatus counted_submit(void *context, const NetFrame *frame, NetTxKey key)
{
    TxFixture *f = context;
    ++f->port_submit_calls;
    return net_port_submit(&f->delegate, frame, key);
}
static NetStatus counted_poll(void *context, NetPortEvent *event)
{
    TxFixture *f = context;
    return net_port_poll(&f->delegate, event);
}
static const NetPortOps counted_ops = {counted_now, counted_submit, counted_poll};

static bool key_equal(NetTxKey a, NetTxKey b)
{
    return a.epoch == b.epoch && a.token == b.token;
}
static TestSidecar *sidecar_find(TxFixture *f, NetTxKey key)
{
    size_t i;
    for (i = 0u; i < SIDECAR_CAPACITY; ++i)
        if (f->sidecars[i].state >= SIDECAR_ATTACHED &&
            key_equal(f->sidecars[i].key, key)) return &f->sidecars[i];
    return NULL;
}
static size_t attached_count(const TxFixture *f)
{
    size_t i, n = 0u;
    for (i = 0u; i < SIDECAR_CAPACITY; ++i)
        if (f->sidecars[i].state == SIDECAR_ATTACHED ||
            f->sidecars[i].state == SIDECAR_RESERVED) ++n;
    return n;
}
static size_t runtime_used(const TxFixture *f)
{
    size_t i, n = 0u;
    for (i = 0u; i < NET_TX_CAPACITY; ++i)
        if (f->runtime.tx[i].used) ++n;
    return n;
}
static int fixture_init(TxFixture *f, const char *name)
{
    static uint64_t fixture_epoch = UINT64_C(9000);
    NetRuntimeConfig config = {0};
    memset(f, 0, sizeof(*f));
    net_host_mock_init(&f->mock);
    f->delegate = net_host_mock_port(&f->mock);
    f->case_name = name;
    f->last_retire = NET_NO_WORK;
    config.port.ops = &counted_ops;
    config.port.context = f;
    config.local_epoch = ++fixture_epoch; /* Unique local fixture lifecycle. */
    config.host_fixture_enabled = true;
    CHECK(net_runtime_init(&f->runtime, &config) == NET_OK);
    CHECK(net_host_mock_set_time(&f->mock, 100u) == NET_OK);
    f->raw.boot_id = UINT32_C(0x10203040);
    f->raw.tx_sequence = UINT32_C(0x50607080);
    f->raw.power_operation_state = UINT8_MAX;
    f->raw.operation_quality = 3u;
    f->raw.update_sequence = UINT32_C(0x89abcdef);
    f->raw.source_age_ms = UINT16_C(0xfedc);
    f->source.observation_id = 700u;
    f->source.observed_at_ms = 80u;
    f->source.original_age_ms = 7u;
    f->policy.bound = true;
    f->policy.maximum_age_ms = 30u;
    f->policy.expires_at_equality = true;
    f->capture_ms = 91u; /* Wire AGE, local observation and capture differ. */
    f->output.standard_id = 0x1f0u; f->output.dlc = 3u; f->output.length = 3u;
    f->output.extended = true;
    memset(f->output.bytes, 0xa5, sizeof(f->output.bytes));
    return 0;
}

static void trace(TxFixture *f, NetTxKey key, const char *phase, NetStatus status)
{
    NetTxRecord record = {0};
    NetStatus snap = net_runtime_tx_snapshot(&f->runtime, key, &record);
    TestSidecar *sidecar = sidecar_find(f, key);
    const NetFrame *borrow = net_host_mock_borrowed_frame(&f->mock, key);
    const NetWireCanEnvelope *envelope = sidecar != NULL ? &sidecar->envelope : NULL;
    const NetSourceStamp *source = sidecar != NULL ? &sidecar->source : &f->source;
    const NetAgePolicy *policy = sidecar != NULL ? &sidecar->policy : &f->policy;
    uint64_t age = UINT64_MAX;
    NetStatus age_status = net_source_age(source, f->mock.now_ms, &age);
    size_t i;
    printf("W3A_TRACE {\"test\":\"wave3a.%s\",\"phase\":\"%s\","
           "\"fixture\":\"%s\",\"clock_domain\":\"host_fixture_monotonic\","
           "\"wire_return\":%d,\"local_return\":%d,\"last_submit_return\":%d,"
           "\"epoch\":%llu,\"token\":%llu,\"source_observation\":%llu,"
           "\"source_observed_at_ms\":%llu,\"original_age_ms\":%llu,"
           "\"wire_age_ms\":%u,\"capture_ms\":%llu,\"now_ms\":%llu,"
           "\"runtime_submit_calls\":%llu,\"port_submit_calls\":%llu,"
           "\"sidecar_state\":%d,\"sidecar_live_count\":%llu,"
           "\"runtime_used\":%llu,\"snapshot_return\":%d,"
           "\"mock_borrow_count\":%llu,\"mock_pointer_present\":%d,"
           "\"mock_events\":%llu,\"admission_known\":%d,\"accepted\":%d,"
           "\"outcome\":%d,\"runtime_release\":%d,\"conflict\":%d,"
           "\"retire_return\":%d,\"duplicate_events\":%lu,\"stale_events\":%lu,"
           "\"conflicting_events\":%lu,\"id\":%lu,\"dlc\":%u,"
           "\"fdf\":%d,\"brs\":%d,\"rtr\":%d,\"extended\":%d,"
           "\"driver_error\":%d,\"length\":%llu,\"bytes_hex\":\"",
           f->case_name, phase, sidecar != NULL ? sidecar->literal_id : "unpublished",
           (int)f->last_wire, (int)status, (int)f->last_local,
           (unsigned long long)key.epoch, (unsigned long long)key.token,
           (unsigned long long)source->observation_id,
           (unsigned long long)source->observed_at_ms,
           (unsigned long long)source->original_age_ms,
           (unsigned)(sidecar != NULL ? sidecar->wire_age_ms : f->raw.source_age_ms),
           (unsigned long long)(sidecar != NULL ? sidecar->capture_ms : f->capture_ms),
           (unsigned long long)f->mock.now_ms,
           (unsigned long long)f->runtime_submit_calls,
           (unsigned long long)f->port_submit_calls,
           sidecar != NULL ? (int)sidecar->state : 0,
           (unsigned long long)attached_count(f), (unsigned long long)runtime_used(f),
           (int)snap, (unsigned long long)net_host_mock_pending_tx_count(&f->mock),
           borrow != NULL, (unsigned long long)f->mock.event_count,
           record.admission_known, record.accepted, (int)record.outcome,
           record.reference_released, record.conflict, (int)f->last_retire,
           (unsigned long)f->runtime.observation.duplicate_events,
           (unsigned long)f->runtime.observation.stale_events,
           (unsigned long)f->runtime.observation.conflicting_events,
           (unsigned long)(envelope != NULL ? envelope->standard_id : 0u),
           (unsigned)(envelope != NULL ? envelope->dlc : 0u),
           envelope != NULL && envelope->fdf, envelope != NULL && envelope->brs,
           envelope != NULL && envelope->rtr, envelope != NULL && envelope->extended,
           envelope != NULL && envelope->driver_error,
           (unsigned long long)(envelope != NULL ? envelope->length : 0u));
    if (envelope != NULL)
        for (i = 0u; i < envelope->length; ++i) printf("%02x", envelope->bytes[i]);
    printf("\",\"input_boot\":%lu,\"input_tx_sequence\":%lu,"
           "\"input_update_sequence\":%lu,\"input_permission\":%u,"
           "\"input_power_quality\":%u,\"input_operation_state\":%u,"
           "\"input_operation_allowed\":%u,\"input_operation_quality\":%u,"
           "\"policy_bound\":%d,\"policy_max_age_ms\":%llu,\"policy_expires_at_equality\":%d,"
           "\"age_return\":%d,\"computed_local_age_ms\":%llu,"
           "\"mock_capture_ms\":%llu,\"mock_id\":%lu,\"mock_length\":%llu,"
           "\"tx_submitted\":%lu,\"tx_admitted\":%lu,\"tx_rejected\":%lu,"
           "\"tx_succeeded\":%lu,\"tx_failed\":%lu,\"tx_released\":%lu,"
           "\"mock_bytes_hex\":\"",
           (unsigned long)f->raw.boot_id, (unsigned long)f->raw.tx_sequence,
           (unsigned long)f->raw.update_sequence, (unsigned)f->raw.vehicle_power_permission,
           (unsigned)f->raw.power_permission_quality, (unsigned)f->raw.power_operation_state,
           (unsigned)f->raw.operation_allowed, (unsigned)f->raw.operation_quality,
           policy->bound, (unsigned long long)policy->maximum_age_ms,
           policy->expires_at_equality, (int)age_status, (unsigned long long)age,
           (unsigned long long)(borrow != NULL ? borrow->captured_at_ms : 0u),
           (unsigned long)(borrow != NULL ? borrow->id : 0u),
           (unsigned long long)(borrow != NULL ? borrow->length : 0u),
           (unsigned long)f->runtime.observation.tx_submitted,
           (unsigned long)f->runtime.observation.tx_admitted,
           (unsigned long)f->runtime.observation.tx_rejected,
           (unsigned long)f->runtime.observation.tx_succeeded,
           (unsigned long)f->runtime.observation.tx_failed,
           (unsigned long)f->runtime.observation.tx_released);
    if (borrow != NULL)
        for (i = 0u; i < borrow->length; ++i) printf("%02x", borrow->bytes[i]);
    printf("\"}\n");
}

/* Reserve test metadata before encode and Runtime submit. Failed calls publish
 * neither envelope nor key, and release only the unborrowed reserved sidecar.
 * Successful metadata lives until explicit Runtime retire; it is not hidden in
 * NetFrame padding/unused bytes. Host Mock never consumes these CAN flags. */
static NetStatus submit(TxFixture *f, NetWireProfile profile,
                        const NetWireDomainPermissionRaw *raw, NetTxKey *out)
{
    size_t i;
    TestSidecar *sidecar = NULL;
    NetWireCanEnvelope envelope;
    NetTxKey key = {0, 0};
    NetStatus status;
    if (out == NULL) return NET_INVALID_ARGUMENT;
    for (i = 0u; i < SIDECAR_CAPACITY; ++i)
        if (f->sidecars[i].state == SIDECAR_FREE ||
            f->sidecars[i].state == SIDECAR_RETIRED) {
            sidecar = &f->sidecars[i]; break;
        }
    if (sidecar == NULL) return NET_NO_CAPACITY;
    memset(sidecar, 0, sizeof(*sidecar));
    sidecar->state = SIDECAR_RESERVED;
    f->last_wire = net_wire_can_encode_permission(profile, raw, &envelope);
    if (f->last_wire != NET_WIRE_OK) {
        memset(sidecar, 0, sizeof(*sidecar));
        f->last_local = f->last_wire == NET_WIRE_INVALID_ARGUMENT ?
            NET_INVALID_ARGUMENT : NET_HOLD;
        return f->last_local;
    }
    memset(&f->caller_frame, 0, sizeof(f->caller_frame));
    f->caller_frame.link = NET_LINK_CAN;
    f->caller_frame.id = envelope.standard_id;
    f->caller_frame.length = envelope.length;
    f->caller_frame.captured_at_ms = f->capture_ms;
    memcpy(f->caller_frame.bytes, envelope.bytes, envelope.length);
    ++f->runtime_submit_calls;
    status = net_runtime_submit(&f->runtime, &f->caller_frame, &f->source, &f->policy, &key);
    f->last_local = status;
    if (status != NET_OK) {
        memset(sidecar, 0, sizeof(*sidecar));
        return status;
    }
    sidecar->state = SIDECAR_ATTACHED;
    sidecar->key = key;
    sidecar->envelope = envelope;
    sidecar->source = f->source;
    sidecar->policy = f->policy;
    sidecar->capture_ms = f->capture_ms;
    sidecar->wire_age_ms = raw->source_age_ms;
    sidecar->literal_id = raw->vehicle_power_permission == 0u ?
        "permission_deny_fixture" : "permission_golden";
    f->output = envelope;
    *out = key;
    return status;
}
static NetStatus retire(TxFixture *f, NetTxKey key)
{
    TestSidecar *sidecar = sidecar_find(f, key);
    NetStatus status;
    trace(f, key, "before_retire", NET_NO_WORK);
    status = net_runtime_retire(&f->runtime, key);
    f->last_retire = status;
    if (status == NET_OK && sidecar != NULL) sidecar->state = SIDECAR_RETIRED;
    trace(f, key, "retire", status);
    return status;
}
static NetStatus event(TxFixture *f, NetTxEvent e, const char *phase)
{
    NetStatus status = net_host_mock_inject_tx(&f->mock, &e);
    if (status == NET_OK) status = net_runtime_step(&f->runtime, 0u);
    trace(f, e.key, phase, status);
    return status;
}
static int finish(TxFixture *f, NetTxKey key, NetTxOutcome outcome)
{
    NetTxEvent e = {key, true, true, true, outcome};
    CHECK(event(f, e, "host_combined_terminal_release") == NET_OK);
    CHECK(retire(f, key) == NET_OK);
    CHECK(net_host_mock_borrowed_frame(&f->mock, key) == NULL);
    return 0;
}
static int envelope_literal(const NetWireCanEnvelope *e, const uint8_t *literal)
{
    CHECK(e->standard_id == 0x080u && e->dlc == 11u && e->length == 20u);
    CHECK(e->fdf && e->brs && !e->rtr && !e->extended && !e->driver_error);
    CHECK(memcmp(e->bytes, literal, 20u) == 0 && e->bytes[19] == 0u);
    return 0;
}
static bool envelope_equal(const NetWireCanEnvelope *a, const NetWireCanEnvelope *b)
{
    return a->standard_id == b->standard_id && a->extended == b->extended &&
        a->fdf == b->fdf && a->brs == b->brs && a->rtr == b->rtr &&
        a->driver_error == b->driver_error && a->dlc == b->dlc &&
        a->length == b->length && memcmp(a->bytes, b->bytes, sizeof(a->bytes)) == 0;
}
static bool sidecar_equal(const TestSidecar *a, const TestSidecar *b)
{
    return a->state == b->state && key_equal(a->key, b->key) &&
        envelope_equal(&a->envelope, &b->envelope) &&
        a->source.observation_id == b->source.observation_id &&
        a->source.observed_at_ms == b->source.observed_at_ms &&
        a->source.original_age_ms == b->source.original_age_ms &&
        a->policy.bound == b->policy.bound &&
        a->policy.maximum_age_ms == b->policy.maximum_age_ms &&
        a->policy.expires_at_equality == b->policy.expires_at_equality &&
        a->capture_ms == b->capture_ms && a->wire_age_ms == b->wire_age_ms &&
        strcmp(a->literal_id, b->literal_id) == 0;
}
static int borrowed_literal(TxFixture *f, NetTxKey key, const uint8_t *literal)
{
    const NetFrame *borrow = net_host_mock_borrowed_frame(&f->mock, key);
    NetTxRecord r;
    CHECK(borrow != NULL && borrow != &f->caller_frame);
    CHECK(borrow->link == NET_LINK_CAN && borrow->id == 0x080u && borrow->length == 20u);
    CHECK(borrow->captured_at_ms == 91u && memcmp(borrow->bytes, literal, 20u) == 0);
    CHECK(net_runtime_tx_snapshot(&f->runtime, key, &r) == NET_OK);
    CHECK(r.source.observation_id == 700u && r.source.observed_at_ms == 80u &&
          r.source.original_age_ms == 7u);
    return 0;
}

static int golden_borrow(const char *name)
{
    unsigned variant;
    for (variant = 0u; variant < 2u; ++variant) {
        TxFixture f;
        NetTxKey key = {77u, 88u};
        NetTxRecord record;
        const uint8_t *literal = variant == 0u ? deny_literal : raw1_literal;
        CHECK(fixture_init(&f, name) == 0);
        f.raw.vehicle_power_permission = (uint8_t)variant;
        CHECK(submit(&f, CANDIDATE, &f.raw, &key) == NET_OK);
        CHECK(envelope_literal(&f.output, literal) == 0);
        CHECK(borrowed_literal(&f, key, literal) == 0);
        CHECK(f.port_submit_calls == 1u && f.runtime_submit_calls == 1u);
        CHECK(net_runtime_tx_snapshot(&f.runtime, key, &record) == NET_OK);
        CHECK(!record.admission_known && !record.accepted && !record.reference_released &&
              !record.conflict && record.outcome == NET_TX_UNKNOWN);
        CHECK(f.mock.event_count == 0u && f.runtime.observation.tx_submitted == 1u);
        CHECK(f.runtime.observation.tx_admitted == 0u && f.runtime.observation.tx_succeeded == 0u);
        trace(&f, key, "golden_borrow_without_admission", NET_OK);
        CHECK(retire(&f, key) == NET_HOLD);
        CHECK(finish(&f, key, NET_TX_SUCCEEDED) == 0);
        trace(&f, key, "case_pass", NET_OK);
    }
    /* These scalar combinations remain raw structure only, with no submission
     * or ALLOW synthesis and no RX-derived permission input. */
    {
        TxFixture f;
        NetWireCanEnvelope out;
        CHECK(fixture_init(&f, name) == 0);
        f.raw.vehicle_power_permission = 1u; f.raw.power_permission_quality = 2u;
        f.last_wire = net_wire_can_encode_permission(CANDIDATE, &f.raw, &out);
        CHECK(f.last_wire == NET_WIRE_OK && out.bytes[8] == 1u && out.bytes[9] == 2u);
        f.raw.vehicle_power_permission = UINT8_MAX; f.raw.power_permission_quality = 0u;
        f.last_wire = net_wire_can_encode_permission(CANDIDATE, &f.raw, &out);
        CHECK(f.last_wire == NET_WIRE_OK && out.bytes[8] == UINT8_MAX && out.bytes[9] == 0u);
        CHECK(f.port_submit_calls == 0u && f.runtime_submit_calls == 0u);
        trace(&f, (NetTxKey){0u,0u}, "raw_only_semantics_no_permission_decision", NET_OK);
    }
    return 0;
}
static int caller_mutation(const char *name)
{
    TxFixture f;
    NetTxKey key = {0u,0u};
    TestSidecar before;
    CHECK(fixture_init(&f, name) == 0);
    CHECK(submit(&f, CANDIDATE, &f.raw, &key) == NET_OK);
    before = *sidecar_find(&f, key);
    memset(&f.raw, 0x33, sizeof(f.raw));
    memset(f.output.bytes, 0x55, sizeof(f.output.bytes));
    f.output.standard_id = 0u; f.output.fdf = false;
    memset(&f.caller_frame, 0x66, sizeof(f.caller_frame));
    f.capture_ms = 0u;
    f.source.observation_id = 555u; f.source.observed_at_ms = 0u;
    CHECK(borrowed_literal(&f, key, deny_literal) == 0);
    CHECK(sidecar_equal(sidecar_find(&f, key), &before));
    CHECK(net_host_mock_set_time(&f.mock, 10000u) == NET_OK);
    CHECK(net_runtime_step(&f.runtime, 0u) == NET_NO_WORK);
    CHECK(borrowed_literal(&f, key, deny_literal) == 0);
    trace(&f, key, "caller_mutated_queue_dwell_no_auto_expiry_claim", NET_NO_WORK);
    CHECK(retire(&f, key) == NET_HOLD);
    CHECK(finish(&f, key, NET_TX_FAILED) == 0);
    trace(&f, key, "case_pass", NET_OK);
    return 0;
}
static int six_outcome_orders(const char *name)
{
    static const unsigned orders[6][3] = {{0,1,2},{0,2,1},{1,0,2},{1,2,0},{2,0,1},{2,1,0}};
    unsigned terminal, order, step;
    for (terminal = 1u; terminal <= 2u; ++terminal)
        for (order = 0u; order < 6u; ++order) {
            TxFixture f;
            NetTxKey key = {0u,0u};
            NetTxRecord r;
            unsigned seen = 0u;
            CHECK(fixture_init(&f, name) == 0);
            CHECK(submit(&f, CANDIDATE, &f.raw, &key) == NET_OK);
            CHECK(retire(&f, key) == NET_HOLD);
            for (step = 0u; step < 3u; ++step) {
                NetTxEvent e = {key, false, false, false, NET_TX_UNKNOWN};
                char phase[80];
                unsigned kind = orders[order][step];
                if (kind == 0u) { e.admission_observed = true; e.accepted = true; }
                if (kind == 1u) e.outcome = (NetTxOutcome)terminal;
                if (kind == 2u) e.release_observed = true;
                seen |= 1u << kind;
                (void)snprintf(phase, sizeof(phase), "terminal_%u_order_%u_step_%u_event_%u", terminal, order, step, kind);
                CHECK(event(&f, e, phase) == NET_OK);
                CHECK(net_runtime_tx_snapshot(&f.runtime, key, &r) == NET_OK);
                CHECK(r.admission_known == ((seen & 1u) != 0u));
                CHECK(r.outcome == (((seen & 2u) != 0u) ? (NetTxOutcome)terminal : NET_TX_UNKNOWN));
                CHECK(r.reference_released == ((seen & 4u) != 0u) && !r.conflict);
                CHECK((net_host_mock_borrowed_frame(&f.mock, key) == NULL) == ((seen & 4u) != 0u));
                CHECK(retire(&f, key) == (step == 2u ? NET_OK : NET_HOLD));
            }
            CHECK(f.runtime.observation.tx_admitted == 1u && f.runtime.observation.tx_released == 1u);
            CHECK(f.runtime.observation.tx_succeeded == (terminal == 1u ? 1u : 0u));
            CHECK(f.runtime.observation.tx_failed == (terminal == 2u ? 1u : 0u));
            trace(&f, key, "permutation_pass", NET_OK);
        }
    for (terminal = 1u; terminal <= 2u; ++terminal) {
        TxFixture f;
        NetTxKey key = {0u,0u};
        CHECK(fixture_init(&f, name) == 0);
        CHECK(submit(&f, CANDIDATE, &f.raw, &key) == NET_OK);
        CHECK(finish(&f, key, (NetTxOutcome)terminal) == 0);
        trace(&f, key, "combined_case_pass", NET_OK);
    }
    return 0;
}
static int rejection_release(const char *name)
{
    unsigned reverse, step;
    for (reverse = 0u; reverse < 2u; ++reverse) {
        TxFixture f;
        NetTxKey key = {0u,0u};
        NetTxRecord r;
        CHECK(fixture_init(&f, name) == 0);
        CHECK(submit(&f, CANDIDATE, &f.raw, &key) == NET_OK);
        for (step = 0u; step < 2u; ++step) {
            bool release = (step != reverse);
            NetTxEvent e = {key, !release, false, release, NET_TX_UNKNOWN};
            CHECK(event(&f, e, release ? "release_only" : "admission_rejected") == NET_OK);
            CHECK(event(&f, e, "duplicate_rejection_or_release") == NET_DUPLICATE);
            CHECK(net_runtime_tx_snapshot(&f.runtime, key, &r) == NET_OK);
            CHECK(r.outcome == NET_TX_UNKNOWN && !r.conflict && !r.accepted);
            if (step == 0u) CHECK(retire(&f, key) == NET_HOLD);
        }
        CHECK(f.runtime.observation.tx_rejected == 1u && f.runtime.observation.tx_released == 1u);
        CHECK(f.runtime.observation.duplicate_events == 2u && f.runtime.observation.tx_admitted == 0u);
        CHECK(f.runtime.observation.tx_succeeded == 0u && f.runtime.observation.tx_failed == 0u);
        CHECK(retire(&f, key) == NET_OK);
        trace(&f, key, "case_pass", NET_OK);
    }
    return 0;
}

static int stale_conflict(const char *name)
{
    unsigned variant;
    for (variant = 0u; variant < 3u; ++variant) {
        TxFixture f;
        NetTxKey key = {0u,0u};
        NetTxRecord r;
        NetTxEvent e;
        CHECK(fixture_init(&f, name) == 0);
        CHECK(submit(&f, CANDIDATE, &f.raw, &key) == NET_OK);
        e = (NetTxEvent){key, true, true, false, NET_TX_UNKNOWN};
        CHECK(event(&f, e, "admission") == NET_OK);
        CHECK(event(&f, e, "duplicate_admission") == NET_DUPLICATE);
        e.key.epoch++;
        e.release_observed = true;
        CHECK(event(&f, e, "wrong_epoch_release") == NET_STALE);
        CHECK(net_host_mock_borrowed_frame(&f.mock, key) != NULL);
        e.key = key; e.key.token++;
        CHECK(event(&f, e, "wrong_token_release") == NET_STALE);
        CHECK(net_host_mock_borrowed_frame(&f.mock, key) != NULL);
        CHECK(f.runtime.observation.stale_events == 2u && f.runtime.observation.duplicate_events == 1u);
        e = (NetTxEvent){key, false, false, false, NET_TX_SUCCEEDED};
        if (variant == 0u) {
            CHECK(event(&f, e, "terminal_success") == NET_OK);
            CHECK(event(&f, e, "duplicate_terminal") == NET_DUPLICATE);
            e.outcome = NET_TX_FAILED;
        } else {
            e.admission_observed = true; e.accepted = false;
            e.outcome = variant == 1u ? NET_TX_UNKNOWN : NET_TX_FAILED;
        }
        e.release_observed = true;
        CHECK(event(&f, e, "contradiction_mock_release_runtime_pinned") == NET_HOLD);
        CHECK(net_host_mock_borrowed_frame(&f.mock, key) == NULL);
        CHECK(net_runtime_tx_snapshot(&f.runtime, key, &r) == NET_OK);
        CHECK(r.conflict && !r.reference_released && r.admission_known && r.accepted);
        CHECK(r.outcome == (variant == 0u ? NET_TX_SUCCEEDED : NET_TX_UNKNOWN));
        CHECK(f.runtime.observation.conflicting_events == 1u && f.runtime.observation.tx_released == 0u);
        CHECK(retire(&f, key) == NET_HOLD);
        e = (NetTxEvent){key, false, false, true, NET_TX_UNKNOWN};
        CHECK(event(&f, e, "later_release_still_pinned") == NET_HOLD);
        CHECK(retire(&f, key) == NET_HOLD);
        CHECK(runtime_used(&f) == 1u && attached_count(&f) == 1u &&
              net_host_mock_pending_tx_count(&f.mock) == 0u && f.mock.event_count == 0u);
        trace(&f, key, "isolated_conflict_case_pass_no_cleanup_or_reuse", NET_HOLD);
        /* Deliberately end isolated lifetime with pinned Runtime/sidecar evidence.
         * Mock has released; no memset/reinit/force-retire claims recovery. */
    }
    return 0;
}

static int failure_atomic(TxFixture *f, NetWireProfile profile,
                          const NetWireDomainPermissionRaw *raw, NetStatus expected,
                          const char *phase)
{
    NetTxKey key = {UINT64_C(0xaabb), UINT64_C(0xccdd)}, sentinel = key;
    NetWireCanEnvelope before = f->output;
    size_t sidecars_before = attached_count(f), records_before = runtime_used(f);
    size_t borrows_before = net_host_mock_pending_tx_count(&f->mock);
    NetStatus result = submit(f, profile, raw, &key);
    trace(f, key, phase, result);
    CHECK(result == expected && key_equal(key, sentinel));
    CHECK(envelope_equal(&f->output, &before));
    CHECK(attached_count(f) == sidecars_before && runtime_used(f) == records_before);
    CHECK(net_host_mock_pending_tx_count(&f->mock) == borrows_before && f->mock.event_count == 0u);
    return 0;
}
static int submit_failures(const char *name)
{
    unsigned kind;
    for (kind = 0u; kind < 11u; ++kind) {
        TxFixture f;
        NetStatus expected = NET_HOLD;
        NetWireProfile profile = CANDIDATE;
        const NetWireDomainPermissionRaw *input;
        char phase[48];
        CHECK(fixture_init(&f, name) == 0);
        input = &f.raw;
        if (kind == 0u) CHECK(net_host_mock_set_submit_status(&f.mock, NET_BUSY) == NET_OK);
        if (kind == 1u) CHECK(net_host_mock_set_submit_status(&f.mock, NET_REJECTED) == NET_OK);
        if (kind == 0u) expected = NET_BUSY;
        if (kind == 1u) expected = NET_REJECTED;
        if (kind == 2u) profile = NET_WIRE_NOT_BOUND;
        if (kind == 3u) f.raw.boot_id = 0u;
        if (kind == 4u) { input = NULL; expected = NET_INVALID_ARGUMENT; }
        if (kind == 5u) f.policy.bound = false;
        if (kind == 6u) f.runtime.config.host_fixture_enabled = false;
        if (kind == 7u) f.runtime.next_token = 0u;
        if (kind == 8u) { f.now_status = NET_NOT_CONFIGURED; expected = NET_NOT_CONFIGURED; }
        if (kind == 9u) { f.source.observation_id = 0u; expected = NET_INVALID_ARGUMENT; }
        if (kind == 10u) f.raw.vehicle_power_permission = 2u;
        (void)snprintf(phase, sizeof(phase), "failed_submit_variant_%u", kind);
        CHECK(failure_atomic(&f, profile, input, expected, phase) == 0);
        CHECK(f.port_submit_calls == (kind < 2u ? 1u : 0u));
        CHECK(f.runtime_submit_calls == ((kind == 2u || kind == 3u || kind == 4u || kind == 10u) ? 0u : 1u));
        if (kind == 2u) CHECK(f.last_wire == NET_WIRE_NOT_BOUND_STATUS);
        if (kind == 3u || kind == 10u) CHECK(f.last_wire == NET_WIRE_MALFORMED);
        if (kind == 4u) CHECK(f.last_wire == NET_WIRE_INVALID_ARGUMENT);
        CHECK(f.runtime.observation.tx_submitted == 0u && f.runtime.observation.tx_admitted == 0u);
        CHECK(f.runtime.observation.tx_rejected == (kind == 1u ? 1u : 0u));
    }
    {
        TxFixture f;
        NetTxKey keys[NET_TX_CAPACITY];
        size_t i;
        CHECK(fixture_init(&f, name) == 0);
        CHECK(submit(&f, CANDIDATE, &f.raw, &keys[0]) == NET_OK);
        CHECK(failure_atomic(&f, CANDIDATE, &f.raw, NET_BUSY, "same_source_observation_busy") == 0);
        CHECK(f.port_submit_calls == 1u);
        for (i = 1u; i < NET_TX_CAPACITY; ++i) {
            f.source.observation_id++;
            CHECK(submit(&f, CANDIDATE, &f.raw, &keys[i]) == NET_OK);
        }
        f.source.observation_id++;
        CHECK(failure_atomic(&f, CANDIDATE, &f.raw, NET_NO_CAPACITY, "runtime_capacity_full") == 0);
        CHECK(f.port_submit_calls == NET_TX_CAPACITY && attached_count(&f) == NET_TX_CAPACITY);
        for (i = 0u; i < NET_TX_CAPACITY; ++i) CHECK(finish(&f, keys[i], NET_TX_FAILED) == 0);
        CHECK(attached_count(&f) == 0u && runtime_used(&f) == 0u);
        trace(&f, keys[3], "case_pass", NET_OK);
    }
    return 0;
}

static int age_boundaries(const char *name)
{
    unsigned kind;
    for (kind = 0u; kind < 7u; ++kind) {
        TxFixture f;
        NetTxKey key = {0u,0u};
        NetStatus expected = NET_OK, age_status;
        uint64_t age = UINT64_C(0xabcdef);
        CHECK(fixture_init(&f, name) == 0);
        if (kind == 0u) f.policy.maximum_age_ms = 28u; /* actual local age27 */
        if (kind == 1u) { f.policy.maximum_age_ms = 27u; expected = NET_EXPIRED; }
        if (kind == 2u) { f.policy.maximum_age_ms = 27u; f.policy.expires_at_equality = false; }
        if (kind == 3u) { f.policy.maximum_age_ms = 26u; expected = NET_EXPIRED; }
        if (kind == 4u) { f.source.observed_at_ms = 101u; expected = NET_HOLD; }
        if (kind == 5u) { f.source.original_age_ms = UINT64_MAX - 19u; expected = NET_HOLD; }
        if (kind == 6u) {
            f.source.original_age_ms = UINT64_MAX - 20u;
            f.policy.maximum_age_ms = UINT64_MAX; f.policy.expires_at_equality = false;
        }
        age_status = net_source_age(&f.source, f.mock.now_ms, &age);
        CHECK(age_status == ((kind == 4u || kind == 5u) ? NET_HOLD : NET_OK));
        CHECK(age == ((kind == 4u || kind == 5u) ? UINT64_C(0xabcdef) :
                      (kind == 6u ? UINT64_MAX : 27u)));
        if (expected == NET_OK) {
            CHECK(submit(&f, CANDIDATE, &f.raw, &key) == NET_OK);
            CHECK(envelope_literal(&f.output, deny_literal) == 0);
            CHECK(f.output.bytes[17] == 0xdcu && f.output.bytes[18] == 0xfeu);
            trace(&f, key, kind == 6u ? "local_age_max_no_wire_age_rewrite" : "local_age_accepted", NET_OK);
            CHECK(finish(&f, key, NET_TX_SUCCEEDED) == 0);
        } else {
            char phase[48];
            (void)snprintf(phase, sizeof(phase), "age_rejected_variant_%u", kind);
            CHECK(failure_atomic(&f, CANDIDATE, &f.raw, expected, phase) == 0);
            CHECK(f.port_submit_calls == 0u);
        }
        trace(&f, key, "age_subcase_pass", expected);
    }
    {
        TxFixture f;
        NetTxKey key = {0u,0u};
        CHECK(fixture_init(&f, name) == 0);
        CHECK(submit(&f, CANDIDATE, &f.raw, &key) == NET_OK);
        CHECK(finish(&f, key, NET_TX_FAILED) == 0);
        CHECK(net_host_mock_set_time(&f.mock, 99u) == NET_OK);
        CHECK(failure_atomic(&f, CANDIDATE, &f.raw, NET_HOLD, "runtime_clock_backward") == 0);
        CHECK(f.port_submit_calls == 1u && f.runtime.last_now_ms == 100u);
        CHECK(net_host_mock_set_time(&f.mock, UINT64_MAX) == NET_OK);
        CHECK(net_host_mock_advance_time(&f.mock, 1u) == NET_INVALID_ARGUMENT);
        CHECK(f.mock.now_ms == UINT64_MAX);
        trace(&f, key, "clock_advance_overflow_rejected_case_pass", NET_INVALID_ARGUMENT);
    }
    return 0;
}

static int retire_reuse(const char *name)
{
    TxFixture f;
    NetTxKey old = {0u,0u}, newer = {0u,0u}, max_key = {0u,0u};
    NetTxEvent e;
    NetTxRecord r;
    CHECK(fixture_init(&f, name) == 0);
    CHECK(submit(&f, CANDIDATE, &f.raw, &old) == NET_OK);
    CHECK(retire(&f, old) == NET_HOLD);
    CHECK(failure_atomic(&f, CANDIDATE, &f.raw, NET_BUSY, "source_reserved_while_borrowed") == 0);
    e = (NetTxEvent){old, false, false, true, NET_TX_UNKNOWN};
    CHECK(event(&f, e, "release_without_admission") == NET_OK);
    CHECK(retire(&f, old) == NET_HOLD);
    CHECK(failure_atomic(&f, CANDIDATE, &f.raw, NET_BUSY, "source_reserved_after_release_before_retire") == 0);
    e = (NetTxEvent){old, true, true, false, NET_TX_UNKNOWN};
    CHECK(event(&f, e, "admission_without_terminal") == NET_OK);
    CHECK(retire(&f, old) == NET_HOLD);
    e = (NetTxEvent){old, false, false, false, NET_TX_FAILED};
    CHECK(event(&f, e, "terminal_completes_evidence") == NET_OK);
    CHECK(retire(&f, old) == NET_OK);
    CHECK(retire(&f, old) == NET_STALE);
    CHECK(submit(&f, CANDIDATE, &f.raw, &newer) == NET_OK);
    CHECK(newer.epoch == old.epoch && newer.token > old.token);
    e = (NetTxEvent){old, true, false, true, NET_TX_UNKNOWN};
    CHECK(event(&f, e, "late_retired_key_release") == NET_STALE);
    CHECK(net_host_mock_borrowed_frame(&f.mock, newer) != NULL);
    CHECK(net_runtime_tx_snapshot(&f.runtime, newer, &r) == NET_OK);
    CHECK(!r.admission_known && !r.reference_released && !r.conflict && r.outcome == NET_TX_UNKNOWN);
    CHECK(finish(&f, newer, NET_TX_SUCCEEDED) == 0);
    /* White-box exhaustion is an isolated Host fault injection, not allocation
     * policy. Runtime's existing final UINT64_MAX token is issued once. */
    f.runtime.next_token = UINT64_MAX;
    CHECK(submit(&f, CANDIDATE, &f.raw, &max_key) == NET_OK);
    CHECK(max_key.token == UINT64_MAX && f.runtime.next_token == 0u);
    CHECK(finish(&f, max_key, NET_TX_FAILED) == 0);
    CHECK(failure_atomic(&f, CANDIDATE, &f.raw, NET_HOLD, "exhausted_token_no_wrap_or_reuse") == 0);
    CHECK(f.port_submit_calls == 3u && runtime_used(&f) == 0u && attached_count(&f) == 0u);
    CHECK(net_host_mock_pending_tx_count(&f.mock) == 0u && f.mock.event_count == 0u);
    trace(&f, max_key, "case_pass", NET_OK);
    return 0;
}

int net_wave3a_test_tx(const char *case_name)
{
    if (case_name == NULL) return 1;
    if (strcmp(case_name, "permission_golden_borrow") == 0) return golden_borrow(case_name);
    if (strcmp(case_name, "permission_caller_mutation") == 0) return caller_mutation(case_name);
    if (strcmp(case_name, "permission_six_outcome_orders") == 0) return six_outcome_orders(case_name);
    if (strcmp(case_name, "permission_rejection_release") == 0) return rejection_release(case_name);
    if (strcmp(case_name, "permission_stale_conflict") == 0) return stale_conflict(case_name);
    if (strcmp(case_name, "permission_submit_failures") == 0) return submit_failures(case_name);
    if (strcmp(case_name, "permission_age_boundaries") == 0) return age_boundaries(case_name);
    if (strcmp(case_name, "permission_retire_reuse") == 0) return retire_reuse(case_name);
    fprintf(stderr, "Unknown Wave3A P02 case: %s\n", case_name);
    return 1;
}
