#include "network/net_runtime.h"
#include "network/net_host_mock.h"
#include <stdio.h>
#include <string.h>

#define CHECK(x) do { if (!(x)) { fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #x); return 1; } } while (0)
typedef struct {
    unsigned calls;
    NetFrame last;
    NetStatus response;
    NetRuntime *reenter;
    NetStatus nested_result;
} Spy;
typedef struct { NetRuntime runtime; NetHostMock mock; Spy spy; } Fixture;
static NetStatus receive(void *context, const NetFrame *frame)
{
    Spy *spy = context;
    ++spy->calls;
    spy->last = *frame;
    if (spy->reenter != NULL) spy->nested_result = net_runtime_step(spy->reenter, 1u);
    return spy->response;
}
static int setup(Fixture *f)
{
    NetRuntimeConfig config;
    memset(f, 0, sizeof(*f));
    net_host_mock_init(&f->mock);
    config.port = net_host_mock_port(&f->mock);
    config.local_epoch = 7u;
    config.consumer = receive;
    config.consumer_context = &f->spy;
    config.host_fixture_enabled = true;
    CHECK(net_runtime_init(&f->runtime, &config) == NET_OK);
    return 0;
}
static NetFrame frame_for(NetLink link)
{
    NetFrame frame = {0};
    frame.link = link;
    frame.id = 0x123u;
    frame.length = 2u;
    frame.bytes[0] = 0x31u;
    frame.bytes[1] = 0x32u;
    frame.captured_at_ms = 42u;
    return frame;
}
static NetStatus submit(Fixture *f, uint64_t id, NetTxKey *key)
{
    NetFrame frame = frame_for(NET_LINK_CAN);
    NetSourceStamp source = {id, 0u, 0u};
    NetAgePolicy policy = {true, 1000u, true};
    return net_runtime_submit(&f->runtime, &frame, &source, &policy, key);
}
static NetStatus tx_event(Fixture *f, NetTxKey key, bool admission, bool accepted,
                          NetTxOutcome outcome, bool release)
{
    NetTxEvent event = {key, admission, accepted, release, outcome};
    NetStatus status = net_host_mock_inject_tx(&f->mock, &event);
    if (status != NET_OK) return status;
    return net_runtime_step(&f->runtime, 0u);
}
static int rx_stable_deferred(void)
{
    Fixture f;
    NetFrame frame;
    unsigned i;
    CHECK(setup(&f) == 0);
    for (i = 0u; i < 3u; ++i) {
        frame = frame_for((NetLink)i);
        CHECK(net_host_mock_inject_rx(&f.mock, &frame) == NET_OK);
        frame.bytes[0] = 0xffu;
        frame.captured_at_ms = 999u;
        CHECK(f.spy.calls == i);
        CHECK(net_host_mock_set_time(&f.mock, 10000u) == NET_OK);
        CHECK(net_runtime_step(&f.runtime, 1u) == NET_OK);
        CHECK(f.spy.calls == i + 1u && f.spy.last.bytes[0] == 0x31u);
        CHECK(f.spy.last.captured_at_ms == 42u && f.spy.last.link == (NetLink)i);
    }
    CHECK(f.runtime.observation.rx_delivered == 3u);
    return 0;
}
static int rx_ring_overflow(void)
{
    Fixture f;
    NetFrame frame = frame_for(NET_LINK_CAN);
    size_t i;
    CHECK(setup(&f) == 0);
    for (i = 0u; i < NET_RX_CAPACITY; ++i) {
        frame.bytes[0] = (uint8_t)i;
        CHECK(net_runtime_capture_rx(&f.runtime, &frame) == NET_OK);
    }
    CHECK(net_runtime_capture_rx(&f.runtime, &frame) == NET_NO_CAPACITY);
    CHECK(f.spy.calls == 0u && f.runtime.observation.rx_dropped == 1u);
    for (i = 0u; i < NET_RX_CAPACITY; ++i) {
        CHECK(net_runtime_step(&f.runtime, 1u) == NET_OK);
        CHECK(f.spy.last.bytes[0] == (uint8_t)i);
        CHECK(net_runtime_capture_rx(&f.runtime, &frame) == NET_OK);
        frame.bytes[0] = 99u;
    }
    CHECK(net_runtime_step(&f.runtime, NET_RX_CAPACITY) == NET_OK);
    CHECK(net_runtime_step(&f.runtime, 1u) == NET_NO_WORK);
    return 0;
}
static int rx_invalid_lengths(void)
{
    Fixture f;
    NetFrame frame = frame_for(NET_LINK_CAN);
    CHECK(setup(&f) == 0);
    frame.length = NET_FRAME_CAPACITY + 1u;
    CHECK(net_runtime_capture_rx(&f.runtime, &frame) == NET_INVALID_ARGUMENT);
    frame.length = 65u;
    CHECK(net_host_mock_inject_rx(&f.mock, &frame) == NET_OK);
    CHECK(net_runtime_step(&f.runtime, 1u) == NET_REJECTED);
    CHECK(f.runtime.observation.rx_invalid == 2u);
    CHECK(f.spy.calls == 0u);
    frame.length = 0u;
    CHECK(net_runtime_capture_rx(&f.runtime, &frame) == NET_OK);
    CHECK(net_runtime_step(&f.runtime, 1u) == NET_OK);
    frame.link = NET_LINK_UART_M;
    CHECK(net_runtime_capture_rx(&f.runtime, &frame) == NET_INVALID_ARGUMENT);
    frame.link = (NetLink)99;
    CHECK(net_runtime_capture_rx(&f.runtime, &frame) == NET_INVALID_ARGUMENT);
    CHECK(f.spy.calls == 1u);
    return 0;
}
static int age_boundaries(void)
{
    Fixture f;
    NetFrame frame = frame_for(NET_LINK_CAN);
    NetSourceStamp source = {1u, 100u, 20u};
    NetAgePolicy policy = {true, 70u, true};
    NetTxKey key = {88u, 99u};
    NetTxRecord record;
    CHECK(setup(&f) == 0);
    CHECK(net_host_mock_set_time(&f.mock, 150u) == NET_OK);
    CHECK(net_runtime_submit(&f.runtime, &frame, &source, &policy, &key) == NET_EXPIRED);
    CHECK(key.epoch == 88u && key.token == 99u);
    policy.expires_at_equality = false;
    CHECK(net_runtime_submit(&f.runtime, &frame, &source, &policy, &key) == NET_OK);
    CHECK(net_runtime_tx_snapshot(&f.runtime, key, &record) == NET_OK);
    CHECK(record.source.original_age_ms == 20u && record.source.observed_at_ms == 100u);
    CHECK(net_host_mock_set_time(&f.mock, 151u) == NET_OK);
    source.observation_id = 2u;
    CHECK(net_runtime_submit(&f.runtime, &frame, &source, &policy, &key) == NET_EXPIRED);
    return 0;
}
static int time_hold(void)
{
    Fixture f;
    uint64_t age = 77u;
    NetSourceStamp source = {1u, 100u, 20u};
    NetFrame frame = frame_for(NET_LINK_CAN);
    NetAgePolicy policy = {false, 1000u, true};
    NetTxKey key;
    CHECK(setup(&f) == 0);
    CHECK(net_source_age(&source, 99u, &age) == NET_HOLD && age == 77u);
    source.original_age_ms = UINT64_MAX;
    CHECK(net_source_age(&source, 101u, &age) == NET_HOLD && age == 77u);
    CHECK(net_runtime_submit(&f.runtime, &frame, &source, &policy, &key) == NET_HOLD);
    CHECK(net_host_mock_set_time(&f.mock, 100u) == NET_OK);
    CHECK(submit(&f, 1u, &key) == NET_OK);
    CHECK(net_host_mock_set_time(&f.mock, 99u) == NET_OK);
    CHECK(submit(&f, 2u, &key) == NET_HOLD);
    return 0;
}
static int tx_release_before_terminal(void)
{
    Fixture f;
    NetTxKey key;
    NetTxRecord record;
    CHECK(setup(&f) == 0 && submit(&f, 1u, &key) == NET_OK);
    CHECK(tx_event(&f, key, true, true, NET_TX_UNKNOWN, true) == NET_OK);
    CHECK(net_runtime_retire(&f.runtime, key) == NET_HOLD);
    CHECK(net_runtime_tx_snapshot(&f.runtime, key, &record) == NET_OK);
    CHECK(record.outcome == NET_TX_UNKNOWN && record.reference_released);
    CHECK(net_host_mock_borrowed_frame(&f.mock, key) == NULL);
    CHECK(f.runtime.observation.tx_succeeded == 0u);
    CHECK(tx_event(&f, key, false, false, NET_TX_SUCCEEDED, false) == NET_OK);
    CHECK(net_runtime_retire(&f.runtime, key) == NET_OK);
    CHECK(net_runtime_tx_snapshot(&f.runtime, key, &record) == NET_STALE);
    return 0;
}
static int tx_terminal_before_release(void)
{
    Fixture f;
    NetTxKey key;
    const NetFrame *borrow;
    NetFrame input = frame_for(NET_LINK_UART_C);
    NetSourceStamp source = {1u, 0u, 0u};
    NetAgePolicy policy = {true, 1000u, true};
    CHECK(setup(&f) == 0);
    CHECK(net_runtime_submit(&f.runtime, &input, &source, &policy, &key) == NET_OK);
    borrow = net_host_mock_borrowed_frame(&f.mock, key);
    CHECK(borrow != NULL && borrow != &input);
    input.bytes[0] = 99u;
    CHECK(borrow->bytes[0] == 0x31u);
    CHECK(tx_event(&f, key, true, true, NET_TX_FAILED, false) == NET_OK);
    CHECK(net_runtime_retire(&f.runtime, key) == NET_HOLD);
    CHECK(net_host_mock_borrowed_frame(&f.mock, key) == borrow);
    CHECK(tx_event(&f, key, false, false, NET_TX_FAILED, false) == NET_DUPLICATE);
    CHECK(f.runtime.observation.tx_failed == 1u);
    CHECK(tx_event(&f, key, false, false, NET_TX_UNKNOWN, true) == NET_OK);
    CHECK(net_runtime_retire(&f.runtime, key) == NET_OK);
    return 0;
}
static int tx_all_event_orders(void)
{
    const unsigned orders[6][3] = {{0,1,2},{0,2,1},{1,0,2},{1,2,0},{2,0,1},{2,1,0}};
    size_t p, j;
    unsigned outcome;
    for (outcome = NET_TX_SUCCEEDED; outcome <= NET_TX_FAILED; ++outcome) {
        for (p = 0u; p < 6u; ++p) {
            Fixture f;
            NetTxKey key;
            CHECK(setup(&f) == 0 && submit(&f, 1u, &key) == NET_OK);
            for (j = 0u; j < 3u; ++j) {
                unsigned kind = orders[p][j];
                CHECK(tx_event(&f, key, kind == 0u, true,
                    kind == 1u ? (NetTxOutcome)outcome : NET_TX_UNKNOWN, kind == 2u) == NET_OK);
                if (j < 2u) CHECK(net_runtime_retire(&f.runtime, key) == NET_HOLD);
            }
            CHECK(net_runtime_retire(&f.runtime, key) == NET_OK);
            CHECK(f.runtime.observation.tx_released == 1u && f.runtime.observation.tx_admitted == 1u);
        }
    }
    return 0;
}
static int tx_capacity_and_stale(void)
{
    Fixture f;
    NetTxKey keys[NET_TX_CAPACITY], newer, stale;
    size_t i;
    CHECK(setup(&f) == 0);
    for (i = 0u; i < NET_TX_CAPACITY; ++i) CHECK(submit(&f, i+1u, &keys[i]) == NET_OK);
    CHECK(submit(&f, 99u, &newer) == NET_NO_CAPACITY);
    CHECK(tx_event(&f, keys[0], true, true, NET_TX_SUCCEEDED, true) == NET_OK);
    CHECK(net_runtime_retire(&f.runtime, keys[0]) == NET_OK);
    CHECK(submit(&f, 99u, &newer) == NET_OK && newer.token != keys[0].token);
    CHECK(tx_event(&f, keys[0], true, true, NET_TX_FAILED, true) == NET_STALE);
    stale = newer;
    ++stale.epoch;
    CHECK(tx_event(&f, stale, true, true, NET_TX_SUCCEEDED, true) == NET_STALE);
    CHECK(net_host_mock_borrowed_frame(&f.mock, newer) != NULL);
    CHECK(net_runtime_retire(&f.runtime, newer) == NET_HOLD);
    return 0;
}
static int tx_rejection_and_conflicts(void)
{
    Fixture f;
    NetTxKey key = {11u, 12u};
    NetTxRecord record;
    CHECK(setup(&f) == 0);
    CHECK(net_host_mock_set_submit_status(&f.mock, NET_BUSY) == NET_OK);
    CHECK(submit(&f, 1u, &key) == NET_BUSY && key.token == 12u);
    CHECK(net_host_mock_pending_tx_count(&f.mock) == 0u);
    CHECK(net_host_mock_set_submit_status(&f.mock, NET_REJECTED) == NET_OK);
    CHECK(submit(&f, 1u, &key) == NET_REJECTED && key.token == 12u);
    CHECK(net_host_mock_set_submit_status(&f.mock, NET_OK) == NET_OK);
    CHECK(submit(&f, 1u, &key) == NET_OK);
    CHECK(tx_event(&f, key, true, false, NET_TX_UNKNOWN, false) == NET_OK);
    CHECK(net_runtime_retire(&f.runtime, key) == NET_HOLD);
    CHECK(tx_event(&f, key, false, false, NET_TX_UNKNOWN, true) == NET_OK);
    CHECK(net_runtime_retire(&f.runtime, key) == NET_OK);
    CHECK(submit(&f, 2u, &key) == NET_OK);
    CHECK(tx_event(&f, key, true, true, NET_TX_SUCCEEDED, false) == NET_OK);
    CHECK(tx_event(&f, key, false, false, NET_TX_FAILED, true) == NET_HOLD);
    CHECK(net_runtime_retire(&f.runtime, key) == NET_HOLD);
    CHECK(net_runtime_tx_snapshot(&f.runtime, key, &record) == NET_OK && record.conflict);
    CHECK(record.outcome == NET_TX_SUCCEEDED && f.runtime.observation.tx_failed == 0u);
    return 0;
}
static int source_reservation_and_token_wrap(void)
{
    Fixture f;
    NetTxKey key, other;
    CHECK(setup(&f) == 0 && submit(&f, 1u, &key) == NET_OK);
    CHECK(tx_event(&f, key, true, true, NET_TX_UNKNOWN, true) == NET_OK);
    CHECK(submit(&f, 1u, &other) == NET_BUSY);
    CHECK(tx_event(&f, key, false, false, NET_TX_SUCCEEDED, false) == NET_OK);
    CHECK(submit(&f, 1u, &other) == NET_BUSY); /* Explicit observer retirement required. */
    CHECK(net_runtime_retire(&f.runtime, key) == NET_OK);
    f.runtime.next_token = UINT64_MAX;
    CHECK(submit(&f, 2u, &key) == NET_OK && key.token == UINT64_MAX);
    CHECK(submit(&f, 3u, &other) == NET_HOLD);
    return 0;
}
static int consumer_and_product_hold(void)
{
    Fixture f;
    NetFrame frame = frame_for(NET_LINK_CAN);
    NetTxKey key;
    CHECK(setup(&f) == 0);
    f.runtime.config.host_fixture_enabled = false;
    CHECK(net_runtime_capture_rx(&f.runtime, &frame) == NET_OK);
    CHECK(net_runtime_step(&f.runtime, 1u) == NET_HOLD && f.spy.calls == 0u);
    CHECK(submit(&f, 1u, &key) == NET_HOLD);
    f.runtime.config.host_fixture_enabled = true;
    f.spy.response = NET_REJECTED;
    CHECK(net_runtime_capture_rx(&f.runtime, &frame) == NET_OK);
    CHECK(net_runtime_step(&f.runtime, 1u) == NET_REJECTED);
    CHECK(f.runtime.observation.consumer_rejected == 1u && f.runtime.observation.rx_delivered == 0u);
    return 0;
}
static int port_errors_and_saturation(void)
{
    Fixture f;
    NetObservation observation;
    CHECK(setup(&f) == 0);
    f.runtime.observation.port_errors = UINT32_MAX - 1u;
    CHECK(net_host_mock_inject_error(&f.mock, 123u) == NET_OK);
    CHECK(net_host_mock_inject_error(&f.mock, 456u) == NET_OK);
    CHECK(net_runtime_step(&f.runtime, 0u) == NET_OK);
    CHECK(net_runtime_observation(&f.runtime, &observation) == NET_OK);
    CHECK(observation.port_errors == UINT32_MAX && observation.rx_delivered == 0u);
    return 0;
}
typedef struct { NetRuntime *runtime; NetStatus nested; } ClockReentry;
static NetStatus reentrant_clock(void *context, uint64_t *out)
{
    ClockReentry *state = context;
    NetFrame frame = frame_for(NET_LINK_CAN);
    state->nested = net_runtime_capture_rx(state->runtime, &frame);
    *out = 0u;
    return NET_OK;
}
static NetStatus reject_submit(void *context, const NetFrame *frame, NetTxKey key)
{
    (void)context; (void)frame; (void)key;
    return NET_REJECTED;
}
static NetStatus empty_poll(void *context, NetPortEvent *out)
{
    (void)context; (void)out;
    return NET_NO_WORK;
}
static int callback_reentry(void)
{
    Fixture f;
    NetFrame frame = frame_for(NET_LINK_CAN);
    const NetPortOps clock_ops = {reentrant_clock, reject_submit, empty_poll};
    ClockReentry state;
    NetTxKey key;
    CHECK(setup(&f) == 0);
    f.spy.reenter = &f.runtime;
    CHECK(net_runtime_capture_rx(&f.runtime, &frame) == NET_OK);
    CHECK(net_runtime_step(&f.runtime, 1u) == NET_OK);
    CHECK(f.spy.nested_result == NET_BUSY && !f.runtime.in_call);
    state.runtime = &f.runtime;
    state.nested = NET_OK;
    f.runtime.config.port.ops = &clock_ops;
    f.runtime.config.port.context = &state;
    CHECK(submit(&f, 1u, &key) == NET_REJECTED);
    CHECK(state.nested == NET_BUSY && f.runtime.rx_count == 0u && !f.runtime.in_call);
    return 0;
}
static NetStatus unavailable_now(void *ctx, uint64_t *out) { (void)ctx; (void)out; return NET_NOT_CONFIGURED; }
static NetStatus unavailable_submit(void *ctx, const NetFrame *f, NetTxKey k) { (void)ctx; (void)f; (void)k; return NET_NOT_CONFIGURED; }
static NetStatus unavailable_poll(void *ctx, NetPortEvent *out) { (void)ctx; (void)out; return NET_NOT_CONFIGURED; }
static int port_swap(void)
{
    const NetPortOps ops = {unavailable_now, unavailable_submit, unavailable_poll};
    Fixture f;
    NetRuntime other;
    NetRuntimeConfig config;
    NetTxKey key;
    NetFrame frame = frame_for(NET_LINK_CAN);
    NetSourceStamp source = {1u, 0u, 0u};
    NetAgePolicy policy = {true, 100u, true};
    CHECK(setup(&f) == 0);
    config = f.runtime.config;
    config.port.ops = &ops;
    config.port.context = NULL;
    CHECK(net_runtime_init(&other, &config) == NET_OK);
    CHECK(net_runtime_step(&other, 1u) == NET_NOT_CONFIGURED);
    CHECK(net_runtime_submit(&other, &frame, &source, &policy, &key) == NET_NOT_CONFIGURED);
    CHECK(submit(&f, 1u, &key) == NET_OK);
    return 0;
}
int main(int argc, char **argv)
{
    const struct { const char *name; int (*run)(void); } tests[] = {
        {"rx_stable_deferred", rx_stable_deferred}, {"rx_ring_overflow", rx_ring_overflow},
        {"rx_invalid_lengths", rx_invalid_lengths}, {"age_boundaries", age_boundaries},
        {"time_hold", time_hold}, {"tx_release_before_terminal", tx_release_before_terminal},
        {"tx_terminal_before_release", tx_terminal_before_release}, {"tx_all_event_orders", tx_all_event_orders},
        {"tx_capacity_and_stale", tx_capacity_and_stale}, {"tx_rejection_and_conflicts", tx_rejection_and_conflicts},
        {"source_reservation_and_token_wrap", source_reservation_and_token_wrap},
        {"consumer_and_product_hold", consumer_and_product_hold}, {"port_errors_and_saturation", port_errors_and_saturation},
        {"callback_reentry", callback_reentry}, {"port_swap", port_swap}
    };
    size_t i;
    unsigned ran = 0u;
    for (i = 0u; i < sizeof(tests)/sizeof(tests[0]); ++i) {
        if (argc > 1 && strcmp(argv[1], tests[i].name) != 0) continue;
        if (tests[i].run() != 0) return 1;
        printf("PASS %s\n", tests[i].name);
        ++ran;
    }
    return ran == 0u ? 2 : 0;
}
