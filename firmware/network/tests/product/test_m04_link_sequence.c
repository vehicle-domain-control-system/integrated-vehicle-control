/* M04 publish slots on UART: when a frame uses up its LINK_SEQUENCE.
 * Expected values come from the contract, not from the engine:
 *   - docs/network/network_sw_architecture.md 5.2: the Message Service picks
 *     LINK_SEQUENCE and commits it when the driver accepts the frame. A frame
 *     the device had no room for never reached the line, so the next frame of
 *     the direction carries its number and the far end sees no gap.
 *   - interface/03_COMMUNICATION/20_UART_M.md shared framing (also the UART-C
 *     frame): LINK_SEQUENCE is the per-direction frame order modulo 65536.
 *     On the line the numbers of a direction only move forward; a number
 *     behind the last one is stale for the far end, the same number twice a
 *     duplicate.
 *   - interface/04_COMMON/10_DATA_AND_ID_RULES.md sequence table: +1 for every
 *     frame sent, a resend too. A frame a device took used its number,
 *     whatever its outcome.
 *   - sw_detail/functions/CIS_REAR_TX_DISPATCH.md: a number a device clearly
 *     refused stays used (a gap is allowed, a reuse is not).
 * The registrations are test-only (no product profile table): the Pi side of
 * UART-C, P_VISION_RESULT 0x60 and P_VISION_STATUS 0x61, one frame per
 * submission. This file implements only a fake Port (clock + device); the
 * service, codec and the publish slots are product code. One case per
 * process. */
#include "network/net_upper_publish.h"
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define CHECK(x) do { if (!(x)) { fprintf(stderr, "%s:%d: %s\n", case_name, __LINE__, #x); return 1; } } while (0)
#define STEP_MS 1u
#define MAX_FRAMES 64u

static const char *case_name;

/* ------------------------------------------------------------ fake Port -- */
typedef struct {
    NetTxKey key;
    NetUpperPacket packet;
    bool open; /* the device still owns it: no facts reported yet */
} Taken;
typedef struct {
    uint64_t now;
    size_t room;         /* frames the device holds at once; more: NO_CAPACITY, no borrow */
    uint8_t no_room_for; /* UART MESSAGE_TYPE refused with NO_CAPACITY, 0 for none */
    bool reject;         /* device clearly refuses the frame: HOLD, no borrow */
    NetTxOutcome outcome;
    size_t count;
    Taken taken[MAX_FRAMES];
} FakePort;

static NetUpper svc;
static NetUpperPublisher pub;
static FakePort port;

static bool decode(const NetUpperPacket *packet, NetUpperUartMessage *out)
{
    return packet->link == NET_WIRE_UART_C &&
           net_upper_uart_decode(NET_WIRE_CANDIDATE_ONLY, NET_WIRE_UART_C, packet->direction, &packet->uart, out) ==
               NET_WIRE_OK;
}
static size_t open_frames(const FakePort *p)
{
    size_t i, n = 0u;
    for (i = 0u; i < p->count; ++i) n += p->taken[i].open ? 1u : 0u;
    return n;
}
static NetStatus fake_now(void *context, uint64_t *out) { *out = ((FakePort *)context)->now; return NET_OK; }
static NetStatus fake_submit(void *context, const NetUpperPacket *packet, NetTxKey key)
{
    FakePort *p = context;
    NetUpperUartMessage m;
    if (p->count >= MAX_FRAMES || open_frames(p) >= p->room) return NET_NO_CAPACITY;
    if (p->no_room_for != 0u && decode(packet, &m) && m.type == p->no_room_for) return NET_NO_CAPACITY;
    if (p->reject) return NET_HOLD;
    p->taken[p->count].key = key;
    p->taken[p->count].packet = *packet;
    p->taken[p->count].open = true;
    ++p->count;
    return NET_OK;
}
static NetStatus fake_poll(void *context, NetUpperEvent *out) { (void)context; (void)out; return NET_NO_WORK; }
static const NetUpperPortOps port_ops = { fake_now, fake_submit, fake_poll };

/* Device reports admission, final outcome and release of every open frame. */
static void settle_all(void)
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
        e.tx.outcome = port.outcome;
        e.tx.release_observed = true;
        (void)net_upper_capture(&svc, &e);
        port.taken[i].open = false;
    }
}

/* One superloop pass in the node's order: step (RX, TX facts), M04 publish
 * service, step (dispatch of what M04 offered). The device settles what it
 * took at the start of the next pass. */
static void pass(void)
{
    port.now += STEP_MS;
    settle_all();
    (void)net_upper_step(&svc, NET_UPPER_POLL_BUDGET);
    (void)net_upper_publish_service(&pub);
    (void)net_upper_step(&svc, NET_UPPER_POLL_BUDGET);
}
static void passes(unsigned n)
{
    while (n-- != 0u) pass();
}

static const NetUpperPublishEntry table[] = {
    { NET_WIRE_UART_C, 0x60u, 0u, { true, 2000u, true } },
    { NET_WIRE_UART_C, 0x61u, 0u, { true, 2000u, true } },
};

static NetStatus start(size_t entries)
{
    NetUpperConfig c;
    NetUpperPublishConfig t;
    size_t i;
    NetStatus st;
    memset(&svc, 0, sizeof(svc));
    memset(&pub, 0, sizeof(pub));
    memset(&port, 0, sizeof(port));
    port.now = 1000u;
    port.room = SIZE_MAX;
    port.outcome = NET_TX_SUCCEEDED;
    memset(&c, 0, sizeof(c));
    c.node = NET_WIRE_NODE_PI;
    c.local_epoch = 7u;
    c.profile = NET_WIRE_CANDIDATE_ONLY;
    c.port.ops = &port_ops;
    c.port.context = &port;
    for (i = 0u; i < 3u; ++i) c.port_epoch[i] = 1u;
    c.uart_sequence_bound[1] = true; /* UART-C LINK_SEQUENCE allocated by the service */
    c.uart_initial_sequence[1] = 1u;
    st = net_upper_init(&svc, &c);
    if (st != NET_OK) return st;
    memset(&t, 0, sizeof(t));
    t.entries = table;
    t.entry_count = entries;
    return net_upper_publish_init(&pub, &svc, &t);
}

static NetStatus publish_result(uint32_t vision_sequence)
{
    NetUpperUartMessage m;
    memset(&m, 0, sizeof(m));
    m.link = NET_WIRE_UART_C;
    m.type = 0x60u;
    m.data.p_vision_result.pi_boot_id = 1432778632u;
    m.data.p_vision_result.vision_sequence = vision_sequence;
    m.data.p_vision_result.vision_age_ms = 15u;
    m.data.p_vision_result.occupant_presence = 1u;
    m.data.p_vision_result.presence_validity = 1u;
    m.data.p_vision_result.occupant_count = 3u;
    m.data.p_vision_result.count_validity = 1u;
    m.data.p_vision_result.vision_status = 2u;
    return net_upper_publish_uart(&pub, &m, NULL, NULL);
}
static NetStatus publish_status(uint32_t status_sequence)
{
    NetUpperUartMessage m;
    memset(&m, 0, sizeof(m));
    m.link = NET_WIRE_UART_C;
    m.type = 0x61u;
    m.data.p_vision_status.pi_boot_id = 1432778632u;
    m.data.p_vision_status.status_sequence = status_sequence;
    m.data.p_vision_status.vision_status = 1u;
    m.data.p_vision_status.status_age_ms = 12u;
    return net_upper_publish_uart(&pub, &m, NULL, NULL);
}

/* LINK_SEQUENCE and MESSAGE_TYPE of the k-th frame the device took. */
static bool line(size_t k, uint8_t *type, uint16_t *sequence)
{
    NetUpperUartMessage m;
    if (k >= port.count || !decode(&port.taken[k].packet, &m)) return false;
    *type = m.type;
    *sequence = m.sequence;
    return true;
}
static NetUpperPublishCounters counters(uint16_t type)
{
    NetUpperPublishView v;
    memset(&v, 0, sizeof(v));
    (void)net_upper_publish_view(&pub, NET_WIRE_UART_C, type, &v);
    return v.counters;
}

/* The device has no room for a while: the submission waits in its slot and,
 * once the device takes it, carries the first number of the direction; the
 * next frame carries the one after it. */
static int m14_uart_no_room_keeps_link_sequence(void)
{
    uint8_t type;
    uint16_t seq;
    CHECK(start(1u) == NET_OK);
    port.room = 0u;
    CHECK(publish_result(1u) == NET_OK);
    passes(31u); /* the first pass offers, every later one finds the refusal */
    CHECK(port.count == 0u);
    CHECK(counters(0x60u).queue_full >= 30u && counters(0x60u).not_sent == 0u);
    port.room = SIZE_MAX;
    pass();
    CHECK(port.count == 1u && line(0u, &type, &seq) && type == 0x60u && seq == 1u);
    CHECK(publish_result(2u) == NET_OK);
    pass();
    CHECK(port.count == 2u && line(1u, &type, &seq) && seq == 2u);
    CHECK(counters(0x60u).sent == 2u && counters(0x60u).not_sent == 0u && counters(0x60u).tx_failed == 0u);
    return 0;
}

/* A frame the device took used its number, sent or not; a frame the device
 * clearly refused used its number too. Neither number is ever carried again. */
static int m15_uart_taken_or_refused_number_used(void)
{
    uint8_t type;
    uint16_t seq;
    CHECK(start(1u) == NET_OK);
    port.outcome = NET_TX_FAILED; /* the device took the frame, then reported it not sent */
    CHECK(publish_result(1u) == NET_OK);
    pass();
    pass();
    CHECK(port.count == 1u && line(0u, &type, &seq) && seq == 1u);
    CHECK(counters(0x60u).tx_failed == 1u);
    port.outcome = NET_TX_SUCCEEDED;
    CHECK(publish_result(2u) == NET_OK);
    pass();
    CHECK(port.count == 2u && line(1u, &type, &seq) && seq == 2u);
    port.reject = true;
    CHECK(publish_result(3u) == NET_OK);
    pass();
    pass();
    CHECK(port.count == 2u && counters(0x60u).not_sent == 1u);
    port.reject = false;
    CHECK(publish_result(4u) == NET_OK);
    pass();
    CHECK(port.count == 3u && line(2u, &type, &seq) && seq == 4u); /* 3 stays used: a gap, no reuse */
    return 0;
}

/* Two messages of one direction offered in the same pass take two numbers.
 * The device has no room for the later one: it gets its number back and the
 * line shows no gap. The device has no room for the earlier one while it
 * takes the later one: the earlier number is not given back (it would go out
 * behind the later number), the frame waits a pass and takes a new number. */
static int m16_uart_give_back_only_newest(void)
{
    uint8_t type;
    uint16_t seq;
    CHECK(start(2u) == NET_OK);
    port.no_room_for = 0x61u;
    CHECK(publish_result(1u) == NET_OK);
    CHECK(publish_status(1u) == NET_OK);
    pass();
    CHECK(port.count == 1u && line(0u, &type, &seq) && type == 0x60u && seq == 1u);
    port.no_room_for = 0u;
    pass();
    CHECK(port.count == 2u && line(1u, &type, &seq) && type == 0x61u && seq == 2u);
    CHECK(counters(0x61u).queue_full == 1u);

    port.no_room_for = 0x60u;
    CHECK(publish_result(2u) == NET_OK);
    CHECK(publish_status(2u) == NET_OK);
    pass();
    CHECK(port.count == 3u && line(2u, &type, &seq) && type == 0x61u && seq == 4u);
    port.no_room_for = 0u;
    pass();
    CHECK(port.count == 4u && line(3u, &type, &seq) && type == 0x60u && seq == 5u);
    CHECK(counters(0x60u).queue_full == 1u);
    CHECK(counters(0x60u).not_sent == 0u && counters(0x61u).not_sent == 0u);
    {
        /* On the line the direction only moves forward, no number twice. */
        uint16_t last = 0u;
        size_t k;
        for (k = 0u; k < port.count; ++k) {
            CHECK(line(k, &type, &seq));
            CHECK(k == 0u || seq > last);
            last = seq;
        }
    }
    return 0;
}

/* 65535 given back stays 65535: the next frame after it carries 0. */
static int m17_uart_give_back_wrap(void)
{
    uint8_t type;
    uint16_t seq;
    CHECK(start(1u) == NET_OK);
    /* Boundary set-up: 65534 frames are not produced in a test, so the
     * service-owned counter is placed at the last number. */
    svc.next_uart_sequence[1] = 65535u;
    port.room = 0u;
    CHECK(publish_result(1u) == NET_OK);
    passes(5u);
    CHECK(port.count == 0u);
    port.room = SIZE_MAX;
    pass();
    CHECK(port.count == 1u && line(0u, &type, &seq) && seq == 65535u);
    CHECK(publish_result(2u) == NET_OK);
    pass();
    CHECK(port.count == 2u && line(1u, &type, &seq) && seq == 0u);
    return 0;
}

int main(int argc, char **argv)
{
    static const struct { const char *name; int (*run)(void); } cases[] = {
        { "m14_uart_no_room_keeps_link_sequence", m14_uart_no_room_keeps_link_sequence },
        { "m15_uart_taken_or_refused_number_used", m15_uart_taken_or_refused_number_used },
        { "m16_uart_give_back_only_newest", m16_uart_give_back_only_newest },
        { "m17_uart_give_back_wrap", m17_uart_give_back_wrap }
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
