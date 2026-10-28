/* DOMAIN C2 TX records with a failure identity after a failure, on the C2
 * adapter and the message service with a test Port (no RTD backend):
 *   - the provisional local failure reason by cause
 *     (net_domain_c2_local_failure_reason_provisional, net_domain_c2.h);
 *   - an in-flight failure (the device took the frame, then FAILED) is retired
 *     only once the device released the frame: until then neither the C2 record
 *     nor the service slot is used again;
 *   - an event that arrives for a retired key after its slots were used again
 *     touches no new record (tx_stale);
 *   - a NET_STALE refusal of a record that is not on UART-M is held.
 * Basis: docs/network/interface/04_COMMON/20_API_USAGE_AND_FAILURE.md (closed
 * list of failures after admission). */
#include "network/net_domain_c2.h"
#include "net_upper_private.h"
#include <stdio.h>
#include <string.h>
#define CHECK(x) do{if(!(x)){fprintf(stderr,"line %d: %s\n",__LINE__,#x);return 1;}}while(0)

static NetUpper service;
static NetDomainC2 adapter;
static DomainIf_TxPort_t ports;
static uint64_t now = 1000u;
static unsigned failures;
static NetTxKey offered[8];
static unsigned offers;

static bool initialized(void) { return true; }
static DomainIf_Status_t set_port(const DomainIf_TxPort_t *p) { if (p) ports = *p; else memset(&ports, 0, sizeof(ports)); return DOMAIN_IF_OK; }
static DomainIf_Status_t rx_failure(const NetworkLocalFailure_t *p) { (void)p; ++failures; return DOMAIN_IF_OK; }
static NetStatus clock_read(void *c, uint64_t *out) { (void)c; *out = now; return NET_OK; }
static NetStatus submit(void *c, const NetUpperPacket *p, NetTxKey k) { (void)c; (void)p; if (offers < 8u) offered[offers] = k; ++offers; return NET_OK; }
static NetStatus poll(void *c, NetUpperEvent *out) { (void)c; (void)out; return NET_NO_WORK; }
static const NetUpperPortOps ops = { clock_read, submit, poll };

static int start(void)
{
    NetDomainC2Api api; NetDomainC2Config cfg; NetUpperConfig config; size_t i;
    memset(&api, 0, sizeof(api)); memset(&cfg, 0, sizeof(cfg)); memset(&config, 0, sizeof(config));
    api.IsInitialized = initialized; api.SetTxPort = set_port; api.RxNetworkLocalFailure = rx_failure;
    cfg.domain_boot = 301u; cfg.bcm_boot = 201u; cfg.first_can_sequence = 1u; cfg.command_accept_limit_ms = 100u;
    cfg.event_use_limit_ms = 100u; cfg.transmit_policy.bound = true; cfg.transmit_policy.maximum_age_ms = 1000u;
    cfg.transmit_policy.expires_at_equality = true; cfg.failure_sink_ready = true;
    cfg.local_failure_reason = net_domain_c2_local_failure_reason_provisional;
    CHECK(net_domain_c2_init(&adapter, &service, NULL, &api, &cfg) == NET_OK);
    config.node = NET_WIRE_NODE_DOMAIN; config.local_epoch = 3u; config.profile = NET_WIRE_CANDIDATE_ONLY;
    config.port.ops = &ops; config.owner = net_domain_c2_owner(&adapter);
    for (i = 0u; i < 3u; ++i) config.port_epoch[i] = 4u;
    for (i = 0u; i < NET_UPPER_ROUTE_COUNT; ++i) { config.rx_policy[i].bound = true; config.rx_policy[i].maximum_age_ms = 10000u; }
    for (i = 0u; i < 2u; ++i) {
        config.uart_policy[i].candidate_time_policy_selected = true; config.uart_policy[i].assembly_limit_ms = 100u;
        config.uart_policy[i].expires_at_equality = true; config.uart_sequence_bound[i] = true; config.uart_initial_sequence[i] = 1u;
    }
    CHECK(net_upper_init(&service, &config) == NET_OK);
    CHECK(net_domain_c2_register(&adapter) == NET_OK);
    return 0;
}

static DomainIf_Status_t door(uint32_t command)
{
    DomainIf_BcmDoorCommand_t c;
    memset(&c, 0, sizeof(c));
    c.context.commandId = command;
    c.context.origin = COMMAND_ORIGIN_MOBILE_REQUEST;
    c.target = DOOR_TARGET_UNLOCK;
    return ports.txBcmDoorCommand(&c);
}

/* A TX fact of the device for key, as the Port reports it. */
static NetStatus fact(NetTxKey key, bool admission, NetTxOutcome outcome, bool release)
{
    NetUpperEvent e;
    memset(&e, 0, sizeof(e));
    e.kind = NET_UPPER_TX_EVENT; e.link = NET_WIRE_CAN;
    e.capture.port_epoch = 4u; e.capture.captured_at_ms = now; e.capture.timestamp_known = true;
    e.tx.key = key; e.tx.admission_observed = admission; e.tx.accepted = admission;
    e.tx.outcome = outcome; e.tx.release_observed = release;
    if (net_upper_capture(&service, &e) != NET_OK) return NET_HOLD;
    (void)net_upper_step(&service, 1u);
    return NET_OK;
}

static int index_of(NetTxKey key)
{
    int i;
    for (i = 0; i < (int)NET_UPPER_TX_CAPACITY; ++i)
        if (adapter.tx[i].used && adapter.tx[i].key.epoch == key.epoch && adapter.tx[i].key.token == key.token) return i;
    return -1;
}
static int service_index_of(NetTxKey key)
{
    int i;
    for (i = 0; i < (int)NET_UPPER_TX_CAPACITY; ++i)
        if (service.tx[i].state.used && service.tx[i].state.key.epoch == key.epoch && service.tx[i].state.key.token == key.token) return i;
    return -1;
}

/* ------------------------------------------- the provisional reason --- */
static NetUpperTxSnapshot refused(bool offered_, bool submitted_, NetStatus local)
{
    NetUpperTxSnapshot s;
    memset(&s, 0, sizeof(s));
    s.used = true; s.admission_known = true; s.accepted = false; s.reference_released = true;
    s.offered = offered_; s.submitted = submitted_; s.outcome = NET_TX_UNKNOWN; s.local_status = local;
    return s;
}
static int reason_is(NetUpperTxSnapshot s, NetStatus want, NetworkLocalFailureReason_t reason)
{
    NetworkLocalFailureReason_t out, before;
    memset(&out, 0xA5, sizeof(out));
    before = out;
    if (net_domain_c2_local_failure_reason_provisional(NULL, &s, &out) != want) return 0;
    if (want == NET_OK) return out == reason;
    return memcmp(&out, &before, sizeof(out)) == 0;
}
static int reason_table(void)
{
    const NetworkLocalFailureReason_t CTX = NETWORK_LOCAL_FAILURE_CONTEXT_INVALIDATED_BEFORE_SEND;
    const NetworkLocalFailureReason_t DRV = NETWORK_LOCAL_FAILURE_DRIVER_SUBMISSION_FINAL;
    NetUpperTxSnapshot s;
    NetworkLocalFailureReason_t out;
    /* A closed / failed / reset link or a lost UART-M context: dropped in the
     * queue or returned by the Port, the same reason. */
    CHECK(reason_is(refused(false, false, NET_HOLD), NET_OK, CTX));
    CHECK(reason_is(refused(true, false, NET_HOLD), NET_OK, CTX));
    /* The UART-M device / session changed before the frame was offered. */
    CHECK(reason_is(refused(false, false, NET_STALE), NET_OK, CTX));
    CHECK(reason_is(refused(true, false, NET_STALE), NET_HOLD, CTX));
    /* The Port or the driver refused the offered frame for good. */
    CHECK(reason_is(refused(true, false, NET_REJECTED), NET_OK, DRV));
    CHECK(reason_is(refused(true, false, NET_NOT_CONFIGURED), NET_OK, DRV));
    CHECK(reason_is(refused(true, false, NET_INVALID_ARGUMENT), NET_OK, DRV));
    CHECK(reason_is(refused(true, false, NET_DUPLICATE), NET_OK, DRV));
    /* Never offered and refused for another reason (a cancel): no reason. */
    CHECK(reason_is(refused(false, false, NET_REJECTED), NET_HOLD, DRV));
    CHECK(reason_is(refused(false, false, NET_INVALID_ARGUMENT), NET_HOLD, DRV));
    /* Expiry is the adapter's own reason; other statuses are not guessed. */
    CHECK(reason_is(refused(true, false, NET_EXPIRED), NET_HOLD, DRV));
    CHECK(reason_is(refused(false, false, NET_EXPIRED), NET_HOLD, DRV));
    CHECK(reason_is(refused(false, false, NET_OK), NET_HOLD, DRV));
    CHECK(reason_is(refused(true, false, NET_UNSUPPORTED), NET_HOLD, DRV));
    /* Refused after it waited in the device (submitted): no cause. */
    CHECK(reason_is(refused(true, true, NET_OK), NET_HOLD, DRV));
    CHECK(reason_is(refused(true, true, NET_HOLD), NET_HOLD, DRV));
    /* Not a settled refusal before the device. */
    s = refused(false, false, NET_HOLD); s.accepted = true; CHECK(reason_is(s, NET_HOLD, CTX));
    s = refused(false, false, NET_HOLD); s.admission_known = false; CHECK(reason_is(s, NET_HOLD, CTX));
    s = refused(false, false, NET_HOLD); s.conflict = true; CHECK(reason_is(s, NET_HOLD, CTX));
    s = refused(false, false, NET_HOLD); s.reference_released = false; CHECK(reason_is(s, NET_HOLD, CTX));
    s = refused(false, false, NET_HOLD); s.outcome = NET_TX_FAILED; CHECK(reason_is(s, NET_HOLD, CTX));
    s = refused(false, false, NET_HOLD);
    CHECK(net_domain_c2_local_failure_reason_provisional(NULL, NULL, &out) == NET_INVALID_ARGUMENT);
    CHECK(net_domain_c2_local_failure_reason_provisional(NULL, &s, NULL) == NET_INVALID_ARGUMENT);
    return 0;
}

/* --------------------------------------- in-flight, late release ---- */
/* The device took a door command and reported it FAILED, but still holds the
 * buffer: the C2 record and its service slot stay in use, and the next command
 * takes other slots. Once the device releases it the record is retired without
 * a notice (in-flight), and its slots are free again. */
static int in_flight_release_late(void)
{
    NetTxKey first, second;
    int c2, svc;
    CHECK(door(77u) == DOMAIN_IF_OK);
    first = adapter.last_tx_key;
    CHECK(net_upper_step(&service, 0u) == NET_OK && offers == 1u);
    CHECK(fact(first, true, NET_TX_FAILED, false) == NET_OK);
    (void)net_domain_c2_step(&adapter);
    (void)net_domain_c2_step(&adapter);
    c2 = index_of(first); svc = service_index_of(first);
    CHECK(c2 >= 0 && svc >= 0 && adapter.tx_retired_in_flight == 0u && failures == 0u);
    CHECK(door(78u) == DOMAIN_IF_OK);
    second = adapter.last_tx_key;
    CHECK(index_of(second) >= 0 && index_of(second) != c2 && service_index_of(second) != svc);
    CHECK(fact(first, false, NET_TX_UNKNOWN, true) == NET_OK);
    (void)net_domain_c2_step(&adapter);
    CHECK(index_of(first) < 0 && service_index_of(first) < 0);
    CHECK(adapter.tx_retired_in_flight == 1u && adapter.tx_retired_last.cause == NET_DOMAIN_C2_RETIRED_IN_FLIGHT);
    CHECK(!adapter.tx_retired_last.has_reason && adapter.tx_retired_last.identity.commandId == 77u && adapter.tx_retired_last.outcome == NET_TX_FAILED);
    CHECK(failures == 0u && index_of(second) >= 0);
    return 0;
}

/* ------------------------------------- an old event after reuse ---- */
/* A door command fails in flight and is retired; the next command takes the
 * same C2 record and service slot. A late event of the retired key (the device
 * reports the old frame SUCCEEDED) finds no service record (tx_stale) and does
 * not complete or fail the new one. */
static int old_event_after_reuse(void)
{
    NetTxKey first, second;
    NetUpperTxSnapshot t;
    uint32_t stale;
    int c2, svc;
    CHECK(door(77u) == DOMAIN_IF_OK);
    first = adapter.last_tx_key;
    c2 = index_of(first); svc = service_index_of(first);
    CHECK(net_upper_step(&service, 0u) == NET_OK);
    CHECK(fact(first, true, NET_TX_FAILED, true) == NET_OK);
    (void)net_domain_c2_step(&adapter);
    CHECK(index_of(first) < 0 && adapter.tx_retired_in_flight == 1u);
    CHECK(door(78u) == DOMAIN_IF_OK);
    second = adapter.last_tx_key;
    CHECK(index_of(second) == c2 && service_index_of(second) == svc && second.token != first.token);
    CHECK(net_upper_step(&service, 0u) == NET_OK);
    stale = service.observation.tx_stale;
    CHECK(fact(first, true, NET_TX_SUCCEEDED, true) == NET_OK);
    CHECK(service.observation.tx_stale == stale + 1u);
    CHECK(net_upper_tx_snapshot(&service, second, &t) == NET_OK);
    CHECK(t.submitted && !t.admission_known && t.outcome == NET_TX_UNKNOWN && !t.reference_released);
    (void)net_domain_c2_step(&adapter);
    CHECK(index_of(second) == c2 && failures == 0u && adapter.tx_retired_in_flight == 1u);
    CHECK(fact(second, true, NET_TX_SUCCEEDED, true) == NET_OK);
    (void)net_domain_c2_step(&adapter);
    CHECK(index_of(second) < 0 && failures == 0u && adapter.tx_retired_in_flight == 1u);
    return 0;
}

/* ----------------------------- NET_STALE outside UART-M is held ---- */
/* The reason rule gives CONTEXT_INVALIDATED_BEFORE_SEND to a NET_STALE refusal
 * only for a UART-M record (the device / session check of an M_RESULT). A CAN
 * command refused with NET_STALE before it was offered (made here directly in
 * the service queue) is held: no notice, the record and its slot stay. */
static int stale_only_uart_m(void)
{
    const NetworkLocalFailureReason_t CTX = NETWORK_LOCAL_FAILURE_CONTEXT_INVALIDATED_BEFORE_SEND;
    NetTxKey first;
    NetUpperTx *r;
    NetUpperTxSnapshot t;
    int c2, svc;
    CHECK(door(77u) == DOMAIN_IF_OK);
    first = adapter.last_tx_key;
    c2 = index_of(first); svc = service_index_of(first);
    r = net_upper_find_tx(&service, first);
    CHECK(c2 >= 0 && svc >= 0 && r != NULL && r->state.queued && offers == 0u);
    net_upper_reject_queued(r, NET_STALE);
    CHECK(net_upper_tx_snapshot(&service, first, &t) == NET_OK);
    CHECK(t.admission_known && !t.accepted && !t.offered && t.local_status == NET_STALE);
    CHECK(reason_is(t, NET_OK, CTX));   /* the rule alone would give a reason */
    CHECK(net_domain_c2_step(&adapter) == NET_HOLD);
    CHECK(index_of(first) == c2 && service_index_of(first) == svc && failures == 0u);
    CHECK(adapter.tx_retired_in_flight == 0u && adapter.tx_retired_unsupported == 0u);
    CHECK(net_domain_c2_step(&adapter) == NET_HOLD && failures == 0u && index_of(first) == c2);
    return 0;
}

int main(int argc, char **argv)
{
    static const struct { const char *name; int (*run)(void); } cases[] = {
        { "reason_table", reason_table },
        { "in_flight_release_late", in_flight_release_late },
        { "old_event_after_reuse", old_event_after_reuse },
        { "stale_only_uart_m", stale_only_uart_m },
    };
    size_t i;
    if (argc != 2 || start() != 0) return 2;
    for (i = 0u; i < sizeof(cases) / sizeof(cases[0]); ++i) {
        if (strcmp(argv[1], cases[i].name) == 0) {
            int rc = cases[i].run();
            if (rc == 0) printf("C2 retire %s PASS\n", argv[1]);
            return rc;
        }
    }
    return 2;
}
