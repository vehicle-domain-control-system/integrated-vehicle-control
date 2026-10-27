/* M04 service queue on CAN: frames of one CAN ID keep their TX_SEQUENCE order
 * when the device has no room for one of them.
 * Expected values come from the contract, not from the engine:
 *   - interface/03_COMMUNICATION/10_CAN_FD.md common header: TX_SEQUENCE per
 *     message, +1 for every frame on the bus; a receiver drops a number behind
 *     the last one it took (stale), so an earlier number may not reach the
 *     bus after a later one of the same message.
 *   - docs/network/network_sw_architecture.md 5.2: a frame the device had no
 *     room for never reached the line; it waits with its number.
 * The sender is the DOMAIN (BCM door command 0x100, climate command 0x110,
 * direct submissions as the C2 adapter makes them). The fake Port refuses one
 * chosen frame with NO_CAPACITY and takes every other frame at once, which a
 * real device cannot do in one call: it shows that the service itself holds
 * the later frames of that ID back. This file implements only the fake Port;
 * the service and codec are product code. One case per process. */
#include "network/net_upper.h"
#include "network/net_upper_can.h"
#include <stdio.h>
#include <string.h>

#define CHECK(x) do { if (!(x)) { fprintf(stderr, "%s:%d: %s\n", case_name, __LINE__, #x); return 1; } } while (0)
#define MAX_FRAMES 16u
#define BOOT 0x444F4D31u
#define BCM_BOOT 0x42434D31u

static const char *case_name;

typedef struct {
    uint64_t now;
    uint16_t refuse_id;      /* CAN ID of the frame refused with NO_CAPACITY, 0 for none */
    uint32_t refuse_seq;     /* its TX_SEQUENCE */
    unsigned refusals;
    size_t count;
    NetTxKey key[MAX_FRAMES];
    NetUpperCanMessage taken[MAX_FRAMES];
} FakePort;

static NetUpper svc;
static FakePort port;

static NetStatus fake_now(void *context, uint64_t *out) { *out = ((FakePort *)context)->now; return NET_OK; }
static NetStatus fake_submit(void *context, const NetUpperPacket *packet, NetTxKey key)
{
    FakePort *p = context;
    NetUpperCanMessage m;
    uint32_t boot = 0u, seq = 0u;
    if (packet->link != NET_WIRE_CAN || p->count >= MAX_FRAMES) return NET_NO_CAPACITY;
    if (net_upper_can_decode(NET_WIRE_CANDIDATE_ONLY, &packet->can, &m) != NET_WIRE_OK) return NET_REJECTED;
    (void)net_upper_can_identity(&m, &boot, &seq);
    if (m.id == p->refuse_id && seq == p->refuse_seq) { ++p->refusals; return NET_NO_CAPACITY; }
    p->key[p->count] = key;
    p->taken[p->count] = m;
    ++p->count;
    return NET_OK;
}
static NetStatus fake_poll(void *context, NetUpperEvent *out) { (void)context; (void)out; return NET_NO_WORK; }
static const NetUpperPortOps port_ops = { fake_now, fake_submit, fake_poll };

static NetStatus start(void)
{
    NetUpperConfig c;
    size_t i;
    memset(&svc, 0, sizeof(svc));
    memset(&port, 0, sizeof(port));
    port.now = 1000u;
    memset(&c, 0, sizeof(c));
    c.node = NET_WIRE_NODE_DOMAIN;
    c.local_epoch = 7u;
    c.profile = NET_WIRE_CANDIDATE_ONLY;
    c.port.ops = &port_ops;
    c.port.context = &port;
    for (i = 0u; i < 3u; ++i) c.port_epoch[i] = 1u;
    return net_upper_init(&svc, &c);
}

static NetStatus submit_door(uint32_t seq, uint32_t command)
{
    NetUpperCanMessage m;
    NetSourceStamp source = { 0u, 0u, 0u };
    NetAgePolicy age = { true, 1000u, true };
    NetTxKey key;
    memset(&m, 0, sizeof(m));
    m.id = 0x100u;
    m.data.bcm_door_command.boot_id = BOOT;
    m.data.bcm_door_command.tx_sequence = seq;
    m.data.bcm_door_command.target_boot_id = BCM_BOOT;
    m.data.bcm_door_command.command_id = command;
    m.data.bcm_door_command.accept_limit_ms = 500u;
    m.data.bcm_door_command.door_lock_target = 1u;
    m.data.bcm_door_command.origin = 5u;
    source.observation_id = svc.next_token;
    source.observed_at_ms = port.now;
    return net_upper_submit_can(&svc, &m, &source, &age, &key);
}
static NetStatus submit_climate(uint32_t seq, uint32_t command)
{
    NetUpperCanMessage m;
    NetSourceStamp source = { 0u, 0u, 0u };
    NetAgePolicy age = { true, 1000u, true };
    NetTxKey key;
    memset(&m, 0, sizeof(m));
    m.id = 0x110u;
    m.data.bcm_climate_command.boot_id = BOOT;
    m.data.bcm_climate_command.tx_sequence = seq;
    m.data.bcm_climate_command.target_boot_id = BCM_BOOT;
    m.data.bcm_climate_command.command_id = command;
    m.data.bcm_climate_command.accept_limit_ms = 500u;
    m.data.bcm_climate_command.fan_target_level = 3u;
    m.data.bcm_climate_command.thermal_direction = 2u;
    m.data.bcm_climate_command.thermal_target_percent = 100u;
    source.observation_id = svc.next_token;
    source.observed_at_ms = port.now;
    return net_upper_submit_can(&svc, &m, &source, &age, &key);
}
static uint32_t seq_of(size_t k)
{
    uint32_t boot = 0u, seq = 0u;
    (void)net_upper_can_identity(&port.taken[k], &boot, &seq);
    return seq;
}

/* The device has no room for door command TX_SEQUENCE 1. Door command 2 is
 * not offered behind it (one refusal counted, the held record not counted);
 * the climate command of another ID still goes. Once the device has room,
 * 1 then 2 go out. */
static int m21_can_no_room_keeps_id_order(void)
{
    CHECK(start() == NET_OK);
    port.refuse_id = 0x100u;
    port.refuse_seq = 1u;
    CHECK(submit_door(1u, 77u) == NET_OK);
    CHECK(submit_door(2u, 78u) == NET_OK);
    CHECK(submit_climate(1u, 79u) == NET_OK);
    port.now += 1u;
    (void)net_upper_step(&svc, NET_UPPER_POLL_BUDGET);
    CHECK(port.refusals == 1u && svc.observation.tx_deferred == 1u);
    CHECK(port.count == 1u && port.taken[0].id == 0x110u);
    port.refuse_id = 0u;
    port.now += 1u;
    (void)net_upper_step(&svc, NET_UPPER_POLL_BUDGET);
    CHECK(port.count == 3u);
    CHECK(port.taken[1].id == 0x100u && seq_of(1u) == 1u && port.taken[1].data.bcm_door_command.command_id == 77u);
    CHECK(port.taken[2].id == 0x100u && seq_of(2u) == 2u && port.taken[2].data.bcm_door_command.command_id == 78u);
    CHECK(svc.observation.tx_deferred == 1u);
    return 0;
}

int main(int argc, char **argv)
{
    static const struct { const char *name; int (*run)(void); } cases[] = {
        { "m21_can_no_room_keeps_id_order", m21_can_no_room_keeps_id_order }
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
