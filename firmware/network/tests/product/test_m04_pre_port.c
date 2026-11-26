/* M04 publish slots: a frame the service drops before the Port uses no number.
 * Expected values come from the contract, not from the engine:
 *   - sw_detail/functions/CIS_REAR_TX_DISPATCH.md: a number is used up only by
 *     a frame offered to a device; a frame never handed to the driver (encode
 *     failure) consumes no number; a number a device clearly refused stays
 *     used (a gap is allowed, a reuse is not).
 *   - docs/network/network_sw_architecture.md 5.2: the Message Service picks
 *     LINK_SEQUENCE and commits it when the frame goes to the driver.
 *   - interface/03_COMMUNICATION/10_CAN_FD.md common header: TX_SEQUENCE per
 *     message, first 1, +1 for every frame on the bus.
 * The registrations are test-only (no product profile table): BCM_STATUS 0x230
 * (BCM) and P_VISION_RESULT 0x60 (Pi side of UART-C), one frame per
 * submission. The service drops the queued frame before the Port because its
 * AGE reached the policy limit while it waited (the dispatch runs later than
 * the M04 pass that queued it). This file implements only a fake Port; the
 * service, codec and the publish slots are product code. One case per
 * process. */
#include "network/net_upper_publish.h"
#include <stdio.h>
#include <string.h>

#define CHECK(x) do { if (!(x)) { fprintf(stderr, "%s:%d: %s\n", case_name, __LINE__, #x); return 1; } } while (0)
#define BOOT 0x42434D31u
#define LIMIT 2000u
#define MAX_FRAMES 16u

static const char *case_name;

typedef struct {
    NetTxKey key;
    NetUpperPacket packet;
    bool open;
} Taken;
typedef struct {
    uint64_t now;
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
    if (p->count >= MAX_FRAMES) return NET_NO_CAPACITY;
    p->taken[p->count].key = key;
    p->taken[p->count].packet = *packet;
    p->taken[p->count].open = true;
    ++p->count;
    return NET_OK;
}
static NetStatus fake_poll(void *context, NetUpperEvent *out) { (void)context; (void)out; return NET_NO_WORK; }
static const NetUpperPortOps port_ops = { fake_now, fake_submit, fake_poll };

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
        e.tx.outcome = NET_TX_SUCCEEDED;
        e.tx.release_observed = true;
        (void)net_upper_capture(&svc, &e);
        port.taken[i].open = false;
    }
}
/* One superloop pass: step (TX facts), M04 publish service, step (dispatch). */
static void pass(void)
{
    port.now += 1u;
    settle_all();
    (void)net_upper_step(&svc, NET_UPPER_POLL_BUDGET);
    (void)net_upper_publish_service(&pub);
    (void)net_upper_step(&svc, NET_UPPER_POLL_BUDGET);
}
/* M04 queues the submission, then the dispatch runs only after `late` ms:
 * the frame reaches the dispatch with AGE >= LIMIT and never goes to the Port. */
static void pass_dispatch_late(uint64_t late)
{
    port.now += 1u;
    settle_all();
    (void)net_upper_step(&svc, NET_UPPER_POLL_BUDGET);
    (void)net_upper_publish_service(&pub);
    port.now += late;
    (void)net_upper_step(&svc, NET_UPPER_POLL_BUDGET);
}

static NetStatus start(uint32_t node, const NetUpperPublishEntry *entries, size_t n)
{
    NetUpperConfig c;
    NetUpperPublishConfig t;
    size_t i;
    NetStatus st;
    memset(&svc, 0, sizeof(svc));
    memset(&pub, 0, sizeof(pub));
    memset(&port, 0, sizeof(port));
    port.now = 1000u;
    memset(&c, 0, sizeof(c));
    c.node = node;
    c.local_epoch = 7u;
    c.profile = NET_WIRE_CANDIDATE_ONLY;
    c.port.ops = &port_ops;
    c.port.context = &port;
    for (i = 0u; i < 3u; ++i) c.port_epoch[i] = 1u;
    c.uart_sequence_bound[1] = true;
    c.uart_initial_sequence[1] = 1u;
    st = net_upper_init(&svc, &c);
    if (st != NET_OK) return st;
    memset(&t, 0, sizeof(t));
    t.can_boot_id = BOOT;
    t.entries = entries;
    t.entry_count = n;
    return net_upper_publish_init(&pub, &svc, &t);
}

static NetStatus publish_status(uint32_t update)
{
    NetUpperCanMessage m;
    memset(&m, 0, sizeof(m));
    m.id = 0x230u;
    m.data.bcm_status.bcm_ecu_state = 1u;
    m.data.bcm_status.update_sequence = update;
    m.data.bcm_status.source_age_ms = 0u;
    return net_upper_publish_can(&pub, &m, NULL, NULL);
}
static bool can_sequence(size_t k, uint32_t *tx_sequence, uint32_t *update)
{
    NetUpperCanMessage m;
    if (k >= port.count || port.taken[k].packet.link != NET_WIRE_CAN ||
        net_upper_can_decode(NET_WIRE_CANDIDATE_ONLY, &port.taken[k].packet.can, &m) != NET_WIRE_OK ||
        m.id != 0x230u) return false;
    *tx_sequence = m.data.bcm_status.tx_sequence;
    *update = m.data.bcm_status.update_sequence;
    return true;
}
static NetStatus publish_result(uint32_t vision_sequence)
{
    NetUpperUartMessage m;
    memset(&m, 0, sizeof(m));
    m.link = NET_WIRE_UART_C;
    m.type = 0x60u;
    m.data.p_vision_result.pi_boot_id = 1432778632u;
    m.data.p_vision_result.vision_sequence = vision_sequence;
    m.data.p_vision_result.vision_age_ms = 0u;
    m.data.p_vision_result.occupant_presence = 1u;
    m.data.p_vision_result.presence_validity = 1u;
    m.data.p_vision_result.occupant_count = 1u;
    m.data.p_vision_result.count_validity = 1u;
    m.data.p_vision_result.vision_status = 2u;
    return net_upper_publish_uart(&pub, &m, NULL, NULL);
}
static bool uart_sequence(size_t k, uint16_t *sequence, uint32_t *vision_sequence)
{
    NetUpperUartMessage m;
    if (k >= port.count || port.taken[k].packet.link != NET_WIRE_UART_C ||
        net_upper_uart_decode(NET_WIRE_CANDIDATE_ONLY, NET_WIRE_UART_C, port.taken[k].packet.direction,
                              &port.taken[k].packet.uart, &m) != NET_WIRE_OK || m.type != 0x60u) return false;
    *sequence = m.sequence;
    *vision_sequence = m.data.p_vision_result.vision_sequence;
    return true;
}
static NetUpperPublishCounters counters(NetWireLink link, uint16_t id)
{
    NetUpperPublishView v;
    memset(&v, 0, sizeof(v));
    (void)net_upper_publish_view(&pub, link, id, &v);
    return v.counters;
}

/* CAN: the dropped frame's TX_SEQUENCE is carried by the next frame on the bus. */
static int m18_can_pre_port_drop_gives_back(void)
{
    static const NetUpperPublishEntry once[] = { { NET_WIRE_CAN, 0x230u, 0u, { true, LIMIT, true } } };
    uint32_t seq, update;
    CHECK(start(NET_WIRE_NODE_BCM, once, 1u) == NET_OK);
    CHECK(publish_status(1u) == NET_OK);
    pass();
    CHECK(port.count == 1u && can_sequence(0u, &seq, &update) && seq == 1u && update == 1u);
    CHECK(publish_status(2u) == NET_OK);
    pass_dispatch_late(LIMIT);
    CHECK(port.count == 1u); /* expired before the Port */
    pass();
    CHECK(counters(NET_WIRE_CAN, 0x230u).not_sent == 1u);
    CHECK(publish_status(3u) == NET_OK);
    pass();
    CHECK(port.count == 2u && can_sequence(1u, &seq, &update) && update == 3u);
    CHECK(seq == 2u); /* no gap: number 2 never reached the bus */
    return 0;
}

/* UART: the dropped frame's LINK_SEQUENCE is carried by the next frame of the
 * direction (no later frame took a number in between). */
static int m19_uart_pre_port_drop_gives_back(void)
{
    static const NetUpperPublishEntry once[] = { { NET_WIRE_UART_C, 0x60u, 0u, { true, LIMIT, true } } };
    uint16_t seq;
    uint32_t vision;
    CHECK(start(NET_WIRE_NODE_PI, once, 1u) == NET_OK);
    CHECK(publish_result(1u) == NET_OK);
    pass();
    CHECK(port.count == 1u && uart_sequence(0u, &seq, &vision) && seq == 1u && vision == 1u);
    CHECK(publish_result(2u) == NET_OK);
    pass_dispatch_late(LIMIT);
    CHECK(port.count == 1u);
    pass();
    CHECK(counters(NET_WIRE_UART_C, 0x60u).not_sent == 1u);
    CHECK(publish_result(3u) == NET_OK);
    pass();
    CHECK(port.count == 2u && uart_sequence(1u, &seq, &vision) && vision == 3u);
    CHECK(seq == 2u);
    return 0;
}

int main(int argc, char **argv)
{
    static const struct { const char *name; int (*fn)(void); } cases[] = {
        { "m18_can_pre_port_drop_gives_back", m18_can_pre_port_drop_gives_back },
        { "m19_uart_pre_port_drop_gives_back", m19_uart_pre_port_drop_gives_back },
    };
    size_t i;
    if (argc != 2) return 2;
    for (i = 0u; i < sizeof(cases) / sizeof(cases[0]); ++i) {
        if (strcmp(argv[1], cases[i].name) == 0) {
            case_name = cases[i].name;
            return cases[i].fn();
        }
    }
    fprintf(stderr, "unknown case %s\n", argv[1]);
    return 2;
}
