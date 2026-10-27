/* M04 Message Service: Network-owned BOOT_ID / TX_SEQUENCE, waiting AGE and
 * the periodic repetition driver (net_upper_publish.h).
 * Expected values come from the contract, not from the engine:
 *   - interface/03_COMMUNICATION/10_CAN_FD.md common header: BOOT_ID is the
 *     sending Network's; TX_SEQUENCE per message, first 1, +1 for every frame
 *     including a repetition of the same original.
 *   - interface/04_COMMON/10_DATA_AND_ID_RULES.md: a repetition keeps the
 *     original value, UPDATE_SEQUENCE and stamp; the current AGE keeps growing
 *     and saturates at 65535.
 *   - sw_detail/functions/CIS_REAR_TX_DISPATCH.md: the wait not yet counted is
 *     added to AGE once; a number is used up only by a frame offered to a
 *     device; after 0xFFFFFFFF the sequence is HOLD.
 * The registrations below are test-only (no product profile table): BCM_STATUS
 * 0x230 with a 250 ms period and P_VISION_RESULT 0x60 with 100 ms. This file
 * implements only a fake Port (clock + device); the service, codec and the
 * publish slots are product code. One case per process. */
#include "network/net_upper_publish.h"
#include <stdio.h>
#include <string.h>

#define CHECK(x) do { if (!(x)) { fprintf(stderr, "%s:%d: %s\n", case_name, __LINE__, #x); return 1; } } while (0)
#define STEP_MS 1u      /* pass period of the simulated superloop = timing tolerance */
#define PERIOD 250u
#define BOOT 0x42434D31u
#define MAX_FRAMES 1024u

static const char *case_name;

/* ------------------------------------------------------------ fake Port -- */
typedef struct {
    NetTxKey key;
    uint64_t at;
    NetUpperPacket packet;
    bool open; /* the device still owns it: no facts reported yet */
} Taken;
typedef struct {
    uint64_t now;
    bool auto_settle; /* healthy bus: every frame is settled on the next pass */
    bool refuse;      /* device has no room: NO_CAPACITY, no borrow */
    bool reject;      /* device clearly refuses the frame: HOLD, no borrow */
    NetTxOutcome outcome;
    size_t count;
    Taken taken[MAX_FRAMES];
} FakePort;

static NetUpper svc;
static NetUpperPublisher pub;
static FakePort port;

static NetStatus fake_now(void *context, uint64_t *out) { *out = ((FakePort *)context)->now; return NET_OK; }
static NetStatus fake_submit(void *context, const NetUpperPacket *packet, NetTxKey key)
{
    FakePort *p = context;
    if (p->refuse || p->count >= MAX_FRAMES) return NET_NO_CAPACITY;
    if (p->reject) return NET_HOLD;
    p->taken[p->count].key = key;
    p->taken[p->count].at = p->now;
    p->taken[p->count].packet = *packet;
    p->taken[p->count].open = true;
    ++p->count;
    return NET_OK;
}
static NetStatus fake_poll(void *context, NetUpperEvent *out) { (void)context; (void)out; return NET_NO_WORK; }
static const NetUpperPortOps port_ops = { fake_now, fake_submit, fake_poll };

/* Device reports admission, final outcome and release of every open frame. */
static void settle_all(NetTxOutcome outcome)
{
    size_t i;
    for (i = 0u; i < port.count; ++i) {
        NetUpperEvent e;
        if (!port.taken[i].open) continue;
        memset(&e, 0, sizeof(e));
        e.kind = NET_UPPER_TX_EVENT;
        e.link = port.taken[i].packet.link;
        e.capture.port_epoch = 1u;
        e.capture.captured_at_ms = port.now;
        e.capture.timestamp_known = true;
        e.tx.key = port.taken[i].key;
        e.tx.admission_observed = true;
        e.tx.accepted = true;
        e.tx.outcome = outcome;
        e.tx.release_observed = true;
        (void)net_upper_capture(&svc, &e);
        port.taken[i].open = false;
    }
}

/* One superloop pass in the node's order: step (RX, TX facts), M04 publish
 * service, step (dispatch of what M04 offered). */
static void pass(void)
{
    if (port.auto_settle) settle_all(port.outcome);
    (void)net_upper_step(&svc, NET_UPPER_POLL_BUDGET);
    (void)net_upper_publish_service(&pub);
    (void)net_upper_step(&svc, NET_UPPER_POLL_BUDGET);
}
static void run_ms(uint64_t ms)
{
    uint64_t i;
    for (i = 0u; i < ms; i += STEP_MS) { port.now += STEP_MS; pass(); }
}

static NetStatus start_service(uint32_t node, bool uart_bound)
{
    NetUpperConfig c;
    size_t i;
    memset(&svc, 0, sizeof(svc));
    memset(&pub, 0, sizeof(pub));
    memset(&port, 0, sizeof(port));
    port.now = 1000u;
    port.auto_settle = true;
    port.outcome = NET_TX_SUCCEEDED;
    memset(&c, 0, sizeof(c));
    c.node = node;
    c.local_epoch = 7u;
    c.profile = NET_WIRE_CANDIDATE_ONLY;
    c.port.ops = &port_ops;
    c.port.context = &port;
    for (i = 0u; i < 3u; ++i) c.port_epoch[i] = 1u;
    c.uart_sequence_bound[1] = uart_bound; /* UART-C LINK_SEQUENCE allocated by the service */
    c.uart_initial_sequence[1] = 1u;
    return net_upper_init(&svc, &c);
}
static NetStatus start_table(const NetUpperPublishEntry *entries, size_t n)
{
    NetUpperPublishConfig c;
    memset(&c, 0, sizeof(c));
    c.can_boot_id = BOOT;
    c.entries = entries;
    c.entry_count = n;
    return net_upper_publish_init(&pub, &svc, &c);
}
static NetStatus start(const NetUpperPublishEntry *entries, size_t n)
{
    NetStatus st = start_service(NET_WIRE_NODE_BCM, true);
    return st == NET_OK ? start_table(entries, n) : st;
}

static const NetUpperPublishEntry periodic[] = { { NET_WIRE_CAN, 0x230u, PERIOD, { true, 2000u, true } } };
static const NetUpperPublishEntry once[] = { { NET_WIRE_CAN, 0x230u, 0u, { true, 2000u, true } } };

static NetUpperCanMessage status(uint32_t update, uint16_t age)
{
    NetUpperCanMessage m;
    memset(&m, 0, sizeof(m));
    m.id = 0x230u;
    m.data.bcm_status.bcm_ecu_state = 3u;
    m.data.bcm_status.fault_mask = 165u;
    m.data.bcm_status.active_fault_category = 3u;
    m.data.bcm_status.active_fault_code = 8u;
    m.data.bcm_status.affected_function = 7u;
    m.data.bcm_status.recovering = 1u;
    m.data.bcm_status.update_sequence = update;
    m.data.bcm_status.source_age_ms = age;
    /* The caller's identity is ignored: M04 owns both values. */
    m.data.bcm_status.boot_id = 0xDEADBEEFu;
    m.data.bcm_status.tx_sequence = 0x7777u;
    return m;
}
static NetStatus publish(uint32_t update, uint16_t age)
{
    NetUpperCanMessage m = status(update, age);
    return net_upper_publish_can(&pub, &m, NULL, NULL);
}
static bool frame(size_t i, NetWireBcmStatusRaw *out)
{
    NetUpperCanMessage m;
    if (i >= port.count || port.taken[i].packet.link != NET_WIRE_CAN) return false;
    if (net_upper_can_decode(NET_WIRE_CANDIDATE_ONLY, &port.taken[i].packet.can, &m) != NET_WIRE_OK ||
        m.id != 0x230u) return false;
    *out = m.data.bcm_status;
    return true;
}
static NetUpperPublishView view(void)
{
    NetUpperPublishView v;
    memset(&v, 0, sizeof(v));
    (void)net_upper_publish_view(&pub, NET_WIRE_CAN, 0x230u, &v);
    return v;
}
static bool same_content(const NetWireBcmStatusRaw *r, const NetUpperCanMessage *m)
{
    const NetWireBcmStatusRaw *e = &m->data.bcm_status;
    return r->bcm_ecu_state == e->bcm_ecu_state && r->fault_mask == e->fault_mask &&
        r->active_fault_category == e->active_fault_category && r->active_fault_code == e->active_fault_code &&
        r->affected_function == e->affected_function && r->recovering == e->recovering &&
        r->update_sequence == e->update_sequence;
}

/* -------------------------------------------------------------- cases ---- */
static int m01_no_submission(void)
{
    NetUpperPublishView v;
    CHECK(start(periodic, 1u) == NET_OK);
    run_ms(5u * PERIOD);
    CHECK(port.count == 0u);
    v = view();
    CHECK(!v.has_source && v.period_ms == PERIOD && v.boot_id == BOOT && v.next_sequence == 1u);
    CHECK(v.counters.periodic_sent == 0u && v.counters.sent == 0u);
    return 0;
}

static int m02_submit_rephase(void)
{
    NetWireBcmStatusRaw r;
    uint64_t t0, t1;
    CHECK(start(periodic, 1u) == NET_OK);
    run_ms(10u);
    t0 = port.now;
    CHECK(publish(100u, 5u) == NET_OK);
    pass(); /* next pass, same millisecond: sent at once */
    CHECK(port.count == 1u && frame(0u, &r) && port.taken[0].at == t0);
    CHECK(r.boot_id == BOOT && r.tx_sequence == 1u && r.update_sequence == 100u && r.source_age_ms == 5u);
    run_ms(PERIOD);
    CHECK(port.count == 2u && frame(1u, &r) && port.taken[1].at == t0 + PERIOD);
    CHECK(r.tx_sequence == 2u && r.update_sequence == 100u && r.source_age_ms == 5u + PERIOD);
    run_ms(50u);
    t1 = port.now; /* t0 + 300: off the old phase */
    CHECK(publish(101u, 7u) == NET_OK);
    pass();
    CHECK(port.count == 3u && frame(2u, &r) && port.taken[2].at == t1);
    CHECK(r.tx_sequence == 3u && r.update_sequence == 101u && r.source_age_ms == 7u);
    /* New phase: nothing at the old due time t0 + 500, next frame at t1 + 250. */
    run_ms(PERIOD - STEP_MS);
    CHECK(port.count == 3u);
    run_ms(STEP_MS);
    CHECK(port.count == 4u && frame(3u, &r) && port.taken[3].at == t1 + PERIOD);
    CHECK(r.tx_sequence == 4u && r.update_sequence == 101u && r.source_age_ms == 7u + PERIOD);
    return 0;
}

static int m03_repeat_same_stamp(void)
{
    NetUpperCanMessage m = status(77u, 20u);
    NetWireBcmStatusRaw r;
    NetUpperPublishView v;
    uint64_t t0;
    size_t k;
    CHECK(start(periodic, 1u) == NET_OK);
    run_ms(3u);
    t0 = port.now;
    CHECK(net_upper_publish_can(&pub, &m, NULL, NULL) == NET_OK);
    pass();
    run_ms(8u * PERIOD);
    CHECK(port.count == 9u);
    for (k = 0u; k < 9u; ++k) {
        uint64_t due = t0 + (uint64_t)k * PERIOD, at = port.taken[k].at;
        CHECK(frame(k, &r));
        CHECK(at >= due && at - due <= STEP_MS);               /* period within one pass */
        CHECK(r.boot_id == BOOT && r.tx_sequence == k + 1u);     /* +1 per frame */
        CHECK(same_content(&r, &m));                            /* same content, UPDATE kept */
        CHECK(r.source_age_ms == 20u + (at - t0));              /* AGE from the original stamp */
    }
    v = view();
    CHECK(v.counters.sent == 1u && v.counters.periodic_sent == 8u && v.counters.not_sent == 0u);
    CHECK(v.next_sequence == 10u);
    return 0;
}

static int m04_skip_pending(void)
{
    NetWireBcmStatusRaw r;
    uint64_t t0;
    CHECK(start(periodic, 1u) == NET_OK);
    port.auto_settle = false; /* the device keeps the first frame */
    run_ms(2u);
    t0 = port.now;
    CHECK(publish(9u, 0u) == NET_OK);
    pass();
    CHECK(port.count == 1u && port.taken[0].open);
    run_ms(PERIOD);
    CHECK(port.count == 1u && view().counters.periodic_skipped_pending == 1u);
    run_ms(PERIOD);
    CHECK(port.count == 1u && view().counters.periodic_skipped_pending == 2u);
    settle_all(NET_TX_SUCCEEDED);
    run_ms(STEP_MS);
    CHECK(port.count == 1u); /* a skipped period is not caught up */
    port.auto_settle = true;
    run_ms(PERIOD - STEP_MS);
    CHECK(port.count == 2u && frame(1u, &r) && port.taken[1].at == t0 + 3u * PERIOD);
    CHECK(r.tx_sequence == 2u && r.update_sequence == 9u && r.source_age_ms == 3u * PERIOD);
    CHECK(view().counters.periodic_skipped_pending == 2u && view().counters.periodic_sent == 1u);
    return 0;
}

static int m05_saturated_age_keeps_sending(void)
{
    NetWireBcmStatusRaw r;
    uint64_t t0;
    size_t k, saturated = 0u;
    CHECK(start(periodic, 1u) == NET_OK);
    t0 = port.now;
    CHECK(publish(5u, 0u) == NET_OK);
    pass();
    run_ms(70000u);
    CHECK(port.count == 1u + 70000u / PERIOD);
    for (k = 0u; k < port.count; ++k) {
        uint64_t elapsed = port.taken[k].at - t0;
        CHECK(frame(k, &r));
        CHECK(r.tx_sequence == k + 1u && r.update_sequence == 5u);
        CHECK(r.source_age_ms == (elapsed >= 65535u ? 65535u : elapsed));
        if (r.source_age_ms == 65535u) ++saturated;
    }
    CHECK(saturated >= 15u);                 /* still sent well after saturation */
    CHECK(port.taken[port.count - 1u].at - t0 == 70000u);
    CHECK(view().counters.not_sent == 0u);
    return 0;
}

static int m06_sequence_continuous(void)
{
    static const uint32_t updates[] = { 1u, 2u, 2u, 4u, 4u, 5u, 5u };
    NetWireBcmStatusRaw r;
    size_t k;
    CHECK(start(periodic, 1u) == NET_OK);
    CHECK(publish(1u, 0u) == NET_OK);
    pass();                                   /* tx1 (submission) */
    run_ms(100u);
    CHECK(publish(2u, 0u) == NET_OK);
    pass();                                   /* tx2 (submission, new phase) */
    run_ms(PERIOD);                           /* tx3 (repetition) */
    run_ms(100u);
    CHECK(publish(3u, 0u) == NET_OK);
    CHECK(publish(4u, 0u) == NET_OK);          /* replaces 3 before any device took it */
    pass();                                   /* tx4 */
    run_ms(PERIOD);                           /* tx5 (repetition) */
    run_ms(PERIOD - STEP_MS);
    port.now += STEP_MS;                      /* exactly on the due time */
    CHECK(publish(5u, 0u) == NET_OK);
    pass();                                   /* tx6: the submission, not an extra repetition */
    run_ms(PERIOD);                           /* tx7 */
    CHECK(port.count == 7u);
    for (k = 0u; k < port.count; ++k) {
        CHECK(frame(k, &r));
        CHECK(r.tx_sequence == k + 1u && r.update_sequence == updates[k]);
    }
    CHECK(view().counters.replaced == 1u && view().counters.sent == 4u && view().counters.periodic_sent == 3u);
    return 0;
}

static int m07_tx_failure_keeps_schedule(void)
{
    NetWireBcmStatusRaw r;
    size_t k;
    CHECK(start(periodic, 1u) == NET_OK);
    port.outcome = NET_TX_FAILED; /* the device took each frame, then reported it not sent */
    CHECK(publish(3u, 0u) == NET_OK);
    pass();
    run_ms(4u * PERIOD);
    CHECK(port.count == 5u);
    run_ms(STEP_MS);
    CHECK(view().counters.tx_failed == 5u);
    /* The device has no room for one due repetition: the frame waits in the
     * service queue (no number used, nothing caught up) and goes out as soon
     * as the device has room, with the next number. */
    port.outcome = NET_TX_SUCCEEDED;
    port.refuse = true;
    run_ms(PERIOD - STEP_MS);
    CHECK(port.count == 5u && view().in_flight && svc.observation.tx_deferred >= 1u);
    port.refuse = false;
    run_ms(STEP_MS);
    CHECK(port.count == 6u);
    run_ms(PERIOD);
    CHECK(port.count == 7u);
    for (k = 0u; k < port.count; ++k) {
        CHECK(frame(k, &r));
        CHECK(r.tx_sequence == k + 1u && r.update_sequence == 3u);
    }
    /* The device clearly refuses one repetition: that number stays used (a
     * gap is allowed, a reuse is not) and the schedule goes on. */
    port.reject = true;
    run_ms(PERIOD);
    CHECK(port.count == 7u);
    port.reject = false;
    run_ms(PERIOD);
    CHECK(port.count == 8u && frame(7u, &r) && r.tx_sequence == 9u);
    CHECK(view().counters.not_sent == 1u);   /* the clear refusal only */
    return 0;
}

static int m08_sequence_wrap_hold(void)
{
    NetWireBcmStatusRaw r;
    NetUpperPublishView v;
    size_t k;
    uint32_t holds;
    CHECK(start(periodic, 1u) == NET_OK);
    /* Boundary set-up: 2^32 frames cannot be produced in a test, so the
     * caller-owned slot is placed two numbers before the end. */
    pub.slot[0].next_sequence = UINT32_MAX - 1u;
    CHECK(publish(1u, 0u) == NET_OK);
    pass();
    run_ms(PERIOD);
    CHECK(port.count == 2u);
    CHECK(frame(0u, &r) && r.tx_sequence == UINT32_MAX - 1u);
    CHECK(frame(1u, &r) && r.tx_sequence == UINT32_MAX);
    v = view();
    CHECK(v.sequence_held);
    run_ms(PERIOD);
    CHECK(port.count == 2u);              /* HOLD: no wrapped or restarted number */
    holds = view().counters.sequence_hold;
    CHECK(holds >= 1u);
    CHECK(publish(2u, 0u) == NET_HOLD);   /* app sees NOT_READY, never a silent loss */
    CHECK(view().counters.sequence_hold == holds + 1u);
    run_ms(4u * PERIOD);
    CHECK(port.count == 2u);
    for (k = 0u; k < port.count; ++k) CHECK(frame(k, &r) && r.tx_sequence >= UINT32_MAX - 1u);
    return 0;
}

static int m09_pending_age(void)
{
    NetWireBcmStatusRaw r;
    uint64_t t1;
    CHECK(start(once, 1u) == NET_OK);
    port.auto_settle = false;
    CHECK(publish(1u, 10u) == NET_OK);
    pass();
    CHECK(port.count == 1u && frame(0u, &r) && r.source_age_ms == 10u); /* nothing to add */
    CHECK(view().counters.pending_age_added_ms_max == 0u);
    run_ms(20u);
    t1 = port.now;
    CHECK(publish(2u, 30u) == NET_OK);    /* the previous frame is still in the device */
    run_ms(40u);
    CHECK(port.count == 1u && view().fresh && view().in_flight);
    settle_all(NET_TX_SUCCEEDED);
    run_ms(STEP_MS);
    CHECK(port.count == 2u && frame(1u, &r));
    CHECK(r.tx_sequence == 2u && r.update_sequence == 2u);
    CHECK(r.source_age_ms == 30u + (port.taken[1].at - t1)); /* submitted AGE + wait */
    CHECK(port.taken[1].at - t1 == 41u);
    CHECK(view().counters.pending_age_added_ms_max == 41u);
    port.auto_settle = true;
    run_ms(2000u);
    CHECK(port.count == 2u);              /* never repeated */
    CHECK(view().counters.periodic_sent == 0u);
    return 0;
}

static int m10_uart_link_sequence(void)
{
    static const NetUpperPublishEntry vision[] = { { NET_WIRE_UART_C, 0x60u, 100u, { true, 2000u, true } } };
    NetUpperUartMessage m, out;
    uint64_t t0, t1;
    size_t k;
    CHECK(start_service(NET_WIRE_NODE_PI, true) == NET_OK);
    CHECK(start_table(vision, 1u) == NET_OK);
    memset(&m, 0, sizeof(m));
    m.link = NET_WIRE_UART_C;
    m.type = 0x60u;
    m.sequence = 0x5555u; /* ignored: the service owns LINK_SEQUENCE */
    m.data.p_vision_result.pi_boot_id = 1432778632u;
    m.data.p_vision_result.vision_sequence = 2578103244u;
    m.data.p_vision_result.vision_age_ms = 15u;
    m.data.p_vision_result.occupant_presence = 1u;
    m.data.p_vision_result.presence_validity = 1u;
    m.data.p_vision_result.occupant_count = 3u;
    m.data.p_vision_result.count_validity = 1u;
    m.data.p_vision_result.vision_status = 2u;
    t0 = port.now;
    CHECK(net_upper_publish_uart(&pub, &m, NULL, NULL) == NET_OK);
    pass();
    run_ms(300u);
    CHECK(port.count == 4u);
    for (k = 0u; k < 4u; ++k) {
        const Taken *f = &port.taken[k];
        CHECK(f->packet.link == NET_WIRE_UART_C);
        CHECK(net_upper_uart_decode(NET_WIRE_CANDIDATE_ONLY, NET_WIRE_UART_C, f->packet.direction,
                                    &f->packet.uart, &out) == NET_WIRE_OK);
        CHECK(out.type == 0x60u && out.sequence == k + 1u);                     /* +1 per frame */
        CHECK(out.data.p_vision_result.vision_sequence == 2578103244u);         /* original kept */
        CHECK(out.data.p_vision_result.vision_age_ms == 15u + (f->at - t0));   /* AGE keeps growing */
    }
    run_ms(50u);
    t1 = port.now;
    m.data.p_vision_result.vision_sequence = 2578103245u;
    m.data.p_vision_result.vision_age_ms = 5u;
    CHECK(net_upper_publish_uart(&pub, &m, NULL, NULL) == NET_OK);
    pass();
    CHECK(port.count == 5u && port.taken[4].at == t1);
    CHECK(net_upper_uart_decode(NET_WIRE_CANDIDATE_ONLY, NET_WIRE_UART_C, port.taken[4].packet.direction,
                                &port.taken[4].packet.uart, &out) == NET_WIRE_OK);
    CHECK(out.sequence == 5u && out.data.p_vision_result.vision_sequence == 2578103245u);
    CHECK(out.data.p_vision_result.vision_age_ms == 5u);
    {
        NetUpperPublishView v;
        CHECK(net_upper_publish_view(&pub, NET_WIRE_UART_C, 0x60u, &v) == NET_OK);
        CHECK(v.next_sequence == 6u && v.boot_id == 0u && v.counters.periodic_sent == 3u);
    }
    return 0;
}

/* 20_UART_M.md shared framing (also the UART-C frame): LINK_SEQUENCE is the
 * per-direction frame order modulo 65536, so after 65535 the next frame
 * carries 0 and the link goes on (CAN TX_SEQUENCE keeps its HOLD, m08). */
static int m13_uart_link_sequence_wrap(void)
{
    static const NetUpperPublishEntry vision[] = { { NET_WIRE_UART_C, 0x60u, 100u, { true, 2000u, true } } };
    static const uint16_t want[] = { 65534u, 65535u, 0u, 1u, 2u };
    NetUpperUartMessage m, out;
    NetUpperPublishView v;
    size_t k;
    CHECK(start_service(NET_WIRE_NODE_PI, true) == NET_OK);
    CHECK(start_table(vision, 1u) == NET_OK);
    /* Boundary set-up: 65533 frames are not produced in a test, so the
     * service-owned counter is placed two numbers before the end. */
    svc.next_uart_sequence[1] = 65534u;
    memset(&m, 0, sizeof(m));
    m.link = NET_WIRE_UART_C;
    m.type = 0x60u;
    m.data.p_vision_result.pi_boot_id = 1432778632u;
    m.data.p_vision_result.vision_sequence = 77u;
    m.data.p_vision_result.vision_age_ms = 15u;
    m.data.p_vision_result.occupant_presence = 1u;
    m.data.p_vision_result.presence_validity = 1u;
    m.data.p_vision_result.occupant_count = 3u;
    m.data.p_vision_result.count_validity = 1u;
    m.data.p_vision_result.vision_status = 2u;
    CHECK(net_upper_publish_uart(&pub, &m, NULL, NULL) == NET_OK);
    pass();
    run_ms(200u);                                  /* two repetitions */
    m.data.p_vision_result.vision_sequence = 78u;  /* new submissions keep numbering */
    CHECK(net_upper_publish_uart(&pub, &m, NULL, NULL) == NET_OK);
    pass();
    m.data.p_vision_result.vision_sequence = 79u;
    run_ms(10u);
    CHECK(net_upper_publish_uart(&pub, &m, NULL, NULL) == NET_OK);
    pass();
    CHECK(port.count == 5u);
    for (k = 0u; k < 5u; ++k) {
        const Taken *f = &port.taken[k];
        CHECK(net_upper_uart_decode(NET_WIRE_CANDIDATE_ONLY, NET_WIRE_UART_C, f->packet.direction,
                                    &f->packet.uart, &out) == NET_WIRE_OK);
        CHECK(out.sequence == want[k]);
    }
    CHECK(net_upper_publish_view(&pub, NET_WIRE_UART_C, 0x60u, &v) == NET_OK);
    CHECK(v.next_sequence == 3u && !v.sequence_held && v.counters.sequence_hold == 0u);
    CHECK(v.counters.not_sent == 0u);
    return 0;
}

static int m11_registration(void)
{
    static const NetUpperPublishEntry unknown[] = { { NET_WIRE_CAN, 0x7FFu, 0u, { true, 10u, true } } };
    static const NetUpperPublishEntry not_sender[] = { { NET_WIRE_CAN, 0x340u, 0u, { true, 10u, true } } };
    static const NetUpperPublishEntry twice[] = { { NET_WIRE_CAN, 0x230u, 0u, { true, 10u, true } },
                                                  { NET_WIRE_CAN, 0x230u, PERIOD, { true, 10u, true } } };
    static const NetUpperPublishEntry unbound[] = { { NET_WIRE_CAN, 0x230u, 0u, { false, 10u, true } } };
    static const NetUpperPublishEntry vision[] = { { NET_WIRE_UART_C, 0x60u, 0u, { true, 10u, true } } };
    NetUpperPublishConfig c;
    NetUpperCanMessage m;
    NetUpperEvent e;
    CHECK(start_service(NET_WIRE_NODE_BCM, true) == NET_OK);
    CHECK(start_table(unknown, 1u) == NET_UNSUPPORTED);
    CHECK(start_table(not_sender, 1u) == NET_REJECTED);
    CHECK(start_table(twice, 2u) == NET_INVALID_ARGUMENT);
    CHECK(start_table(unbound, 1u) == NET_INVALID_ARGUMENT);
    memset(&c, 0, sizeof(c));
    c.entries = once;
    c.entry_count = 1u;
    CHECK(net_upper_publish_init(&pub, &svc, &c) == NET_INVALID_ARGUMENT); /* BOOT_ID 0 is reserved */
    c.can_boot_id = BOOT;
    c.entry_count = NET_UPPER_PUBLISH_CAPACITY + 1u;
    CHECK(net_upper_publish_init(&pub, &svc, &c) == NET_INVALID_ARGUMENT);
    CHECK(start_service(NET_WIRE_NODE_PI, false) == NET_OK);
    CHECK(start_table(vision, 1u) == NET_NOT_CONFIGURED); /* LINK_SEQUENCE allocation not bound */

    CHECK(start(once, 1u) == NET_OK);
    m = status(1u, 0u);
    m.id = 0x200u;
    CHECK(net_upper_publish_can(&pub, &m, NULL, NULL) == NET_UNSUPPORTED); /* not registered */
    m = status(1u, 0u);
    m.data.bcm_status.bcm_ecu_state = 0xEEu;
    CHECK(net_upper_publish_can(&pub, &m, NULL, NULL) == NET_REJECTED);    /* codec refuses the content */
    m = status(1u, 2001u);
    CHECK(net_upper_publish_can(&pub, &m, NULL, NULL) == NET_EXPIRED);     /* older than the bound */
    CHECK(view().counters.submitted == 0u && !view().has_source);
    run_ms(10u);
    CHECK(port.count == 0u);
    /* Link failed: the submission is refused (NOT_READY for the application). */
    memset(&e, 0, sizeof(e));
    e.kind = NET_UPPER_PORT_ERROR;
    e.link = NET_WIRE_CAN;
    e.capture.port_epoch = 1u;
    e.capture.timestamp_known = true;
    CHECK(net_upper_capture(&svc, &e) == NET_OK);
    run_ms(1u);
    CHECK(publish(1u, 0u) == NET_HOLD);
    run_ms(10u);
    CHECK(port.count == 0u);
    return 0;
}

static int m12_queue_full_keeps_submission(void)
{
    NetWireBcmStatusRaw r;
    NetUpperPublishView v;
    uint64_t t0;
    CHECK(start(once, 1u) == NET_OK);
    port.refuse = true;
    t0 = port.now;
    CHECK(publish(1u, 4u) == NET_OK);
    pass();
    run_ms(30u);
    CHECK(port.count == 0u);
    v = view();
    CHECK(v.in_flight);                     /* kept in the service queue, not dropped */
    CHECK(v.counters.not_sent == 0u && svc.observation.tx_deferred >= 30u);
    port.refuse = false;
    run_ms(STEP_MS);
    CHECK(port.count == 1u && frame(0u, &r));
    CHECK(r.tx_sequence == 1u);             /* refused offers used no number */
    CHECK(r.source_age_ms == 4u + (port.taken[0].at - t0)); /* the wait in the service queue is in AGE */
    CHECK(view().counters.sent == 1u && view().counters.pending_age_added_ms_max == 0u);
    return 0;
}

int main(int argc, char **argv)
{
    static const struct { const char *name; int (*run)(void); } cases[] = {
        { "m01_no_submission", m01_no_submission }, { "m02_submit_rephase", m02_submit_rephase },
        { "m03_repeat_same_stamp", m03_repeat_same_stamp }, { "m04_skip_pending", m04_skip_pending },
        { "m05_saturated_age_keeps_sending", m05_saturated_age_keeps_sending },
        { "m06_sequence_continuous", m06_sequence_continuous },
        { "m07_tx_failure_keeps_schedule", m07_tx_failure_keeps_schedule },
        { "m08_sequence_wrap_hold", m08_sequence_wrap_hold }, { "m09_pending_age", m09_pending_age },
        { "m10_uart_link_sequence", m10_uart_link_sequence }, { "m11_registration", m11_registration },
        { "m12_queue_full_keeps_submission", m12_queue_full_keeps_submission },
        { "m13_uart_link_sequence_wrap", m13_uart_link_sequence_wrap }
    };
    size_t i;
    if (argc != 2) { fprintf(stderr, "usage: %s <case>\n", argv[0]); return 2; }
    for (i = 0u; i < sizeof(cases) / sizeof(cases[0]); ++i) {
        if (strcmp(argv[1], cases[i].name) == 0) {
            case_name = cases[i].name;
            return cases[i].run();
        }
    }
    fprintf(stderr, "unknown case %s\n", argv[1]);
    return 2;
}
