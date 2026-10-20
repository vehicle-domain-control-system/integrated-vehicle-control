#include "network/net_upper.h"
#include <stdio.h>
#include <string.h>

/* Only the physical Port and external Owner are faked. No Network routing,
 * lifetime state machine, codec, Query, or adapter is implemented here. */
#define CHECK(x) do { if (!(x)) { \
    printf("{\"case\":\"%s\",\"line\":%d,\"assertion\":\"%s\",\"result\":\"FAIL\"}\n", case_name, __LINE__, #x); \
    return 1; } } while (0)
static const char *case_name;
typedef struct {
    uint64_t now;
    NetStatus clock_status, submit_status;
    NetUpperEvent events[128];
    size_t head, count, submissions;
    const NetUpperPacket *borrowed[NET_UPPER_TX_CAPACITY];
    NetTxKey keys[NET_UPPER_TX_CAPACITY];
    NetUpperPacket first_copy;
    NetUpper *reenter;
    NetStatus nested_step, nested_cancel, nested_submit;
    NetWireCisRearRaw nested_rear;
    NetSourceStamp nested_source;
    NetAgePolicy nested_policy;
    NetTxKey nested_key;
} FakePort;
typedef struct {
    size_t calls, can_calls, uart_calls;
    NetUpperRear rear;
    NetUpperCanMessage can;
    NetUpperUartDelivery uart;
    NetUpperDelivery delivery;
    NetStatus status;
} FakeDomainIf;
static NetUpper sender, receiver;
static FakePort sender_port, receiver_port;
static FakeDomainIf domain;

static int same_key(NetTxKey a, NetTxKey b)
{ return a.epoch == b.epoch && a.token == b.token; }
static NetStatus fake_now(void *context, uint64_t *out)
{
    FakePort *p = context;
    if (p->clock_status != NET_OK) return p->clock_status;
    *out = p->now;
    return NET_OK;
}
static NetStatus fake_submit(void *context, const NetUpperPacket *packet, NetTxKey key)
{
    FakePort *p = context;
    size_t i;
    if (p->submit_status != NET_OK) return p->submit_status;
    for (i = 0u; i < NET_UPPER_TX_CAPACITY; ++i) if (p->borrowed[i] == NULL) break;
    if (i == NET_UPPER_TX_CAPACITY) return NET_NO_CAPACITY;
    p->borrowed[i] = packet; p->keys[i] = key;
    if (p->submissions == 0u) p->first_copy = *packet;
    ++p->submissions;
    if (p->reenter != NULL) {
        p->nested_step = net_upper_step(p->reenter, 1u);
        p->nested_cancel = net_upper_cancel(p->reenter, key);
        p->nested_submit = net_upper_cis_publish_rear(p->reenter, &p->nested_rear,
            &p->nested_source, &p->nested_policy, &p->nested_key);
    }
    return NET_OK;
}
static NetStatus fake_poll(void *context, NetUpperEvent *out)
{
    FakePort *p = context;
    size_t i;
    if (p->count == 0u) return NET_NO_WORK;
    *out = p->events[p->head];
    p->head = (p->head + 1u) % 128u; --p->count;
    if (out->kind == NET_UPPER_TX_EVENT && out->tx.release_observed)
        for (i = 0u; i < NET_UPPER_TX_CAPACITY; ++i)
            if (p->borrowed[i] != NULL && same_key(p->keys[i], out->tx.key))
                p->borrowed[i] = NULL;
    return NET_OK;
}
static const NetUpperPortOps port_ops = {fake_now, fake_submit, fake_poll};
static NetStatus fake_rear(void *context, const NetUpperRear *rear,
                           const NetUpperDelivery *delivery)
{
    FakeDomainIf *f = context;
    ++f->calls; f->rear = *rear; f->delivery = *delivery;
    return f->status;
}
static NetStatus fake_can(void *context, const NetUpperCanMessage *message,
                          const NetUpperDelivery *delivery)
{
    FakeDomainIf *f=context;
    ++f->can_calls;f->can=*message;f->delivery=*delivery;
    return f->status;
}
static NetStatus fake_uart(void *context,const NetUpperUartDelivery *message,
                           const NetUpperDelivery *delivery)
{
    FakeDomainIf *f=context;
    ++f->uart_calls;f->uart=*message;f->delivery=*delivery;
    return f->status;
}
static int push(FakePort *p, const NetUpperEvent *event)
{
    if (p->count == 128u) return 0;
    p->events[(p->head + p->count) % 128u] = *event; ++p->count;
    return 1;
}
static NetStatus init_service(NetUpper *s, FakePort *p, uint32_t node,
                              FakeDomainIf *owner)
{
    NetUpperConfig c = {0};
    size_t i;
    memset(s, 0, sizeof(*s)); memset(p, 0, sizeof(*p));
    p->now = 100u;
    c.node = node; c.local_epoch = 9u;
    c.profile = NET_WIRE_CANDIDATE_ONLY;
    c.port.ops = &port_ops; c.port.context = p;
    for (i = 0u; i < 3u; ++i) c.port_epoch[i] = 1u;
    for (i = 0u; i < NET_UPPER_ROUTE_COUNT; ++i) {
        c.rx_policy[i].bound = true;
        c.rx_policy[i].maximum_age_ms = 500u;
        c.rx_policy[i].expires_at_equality = true;
    }
    for (i = 0u; i < 2u; ++i) {
        c.uart_policy[i].candidate_time_policy_selected = true;
        c.uart_policy[i].assembly_limit_ms = 1000u;
        c.uart_policy[i].expires_at_equality = true;
    }
    if (owner != NULL) {
        memset(owner, 0, sizeof(*owner));
        c.owner.context = owner; c.owner.rear = fake_rear; c.owner.can = fake_can;
        c.owner.uart = fake_uart;
    }
    return net_upper_init(s, &c);
}
static NetWireCisRearRaw rear_value(uint32_t sequence, uint16_t distance)
{
    NetWireCisRearRaw r = {0};
    r.boot_id = 41u; r.tx_sequence = sequence; r.update_sequence = 77u;
    r.rear_distance_deci_cm = distance; r.validity = 1u;
    return r;
}
static NetSourceStamp stamp(uint64_t id)
{
    NetSourceStamp s = {0}; s.observation_id = id; s.observed_at_ms = 100u;
    return s;
}
static NetAgePolicy policy(void)
{
    NetAgePolicy p = {true, 500u, true}; return p;
}
static NetUpperEvent tx_event(NetTxKey key, unsigned part)
{
    NetUpperEvent e = {0};
    e.kind = NET_UPPER_TX_EVENT; e.link = NET_WIRE_CAN;
    e.tx.key = key;
    if (part == 0u || part == 3u) { e.tx.admission_observed = true; e.tx.accepted = true; }
    if (part == 1u || part == 3u) e.tx.outcome = NET_TX_SUCCEEDED;
    if (part == 2u || part == 3u) e.tx.release_observed = true;
    return e;
}
static NetUpperEvent rear_event(const NetWireCisRearRaw *r)
{
    NetUpperEvent e = {0};
    e.kind = NET_UPPER_CAN_RX; e.link = NET_WIRE_CAN;
    e.capture.port_epoch = 1u; e.capture.timestamp_known = true;
    e.capture.captured_at_ms = 100u;
    (void)net_wire_can_encode_rear(NET_WIRE_CANDIDATE_ONLY, r, &e.can);
    return e;
}
static int product_rear(void)
{
    NetWireCisRearRaw r = rear_value(1u, 123u), decoded;
    NetSourceStamp s = stamp(1u); NetAgePolicy p = policy();
    NetTxKey key; NetUpperEvent e; NetUpperTxSnapshot state;
    CHECK(init_service(&sender, &sender_port, NET_WIRE_NODE_CIS, NULL) == NET_OK);
    CHECK(init_service(&receiver, &receiver_port, NET_WIRE_NODE_DOMAIN, &domain) == NET_OK);
    CHECK(net_upper_cis_publish_rear(&sender, &r, &s, &p, &key) == NET_OK);
    CHECK(net_upper_tx_snapshot(&sender, key, &state) == NET_OK && state.queued);
    r.rear_distance_deci_cm = 999u; s.original_age_ms = 999u;
    sender_port.now = 103u;
    (void)net_upper_step(&sender, 32u);
    CHECK(sender_port.submissions == 1u && sender_port.borrowed[0] != NULL);
    CHECK(net_wire_can_decode_rear(NET_WIRE_CANDIDATE_ONLY,
        &sender_port.borrowed[0]->can, &decoded) == NET_WIRE_OK);
    CHECK(decoded.rear_distance_deci_cm == 123u && decoded.source_age_ms == 3u);
    CHECK(sender_port.borrowed[0]->can.standard_id == 0x0a0u);
    CHECK(sender_port.borrowed[0]->can.dlc == 11u && sender_port.borrowed[0]->can.length == 20u);
    e = rear_event(&decoded); e.capture.captured_at_ms = 103u;
    CHECK(push(&receiver_port, &e));
    receiver_port.now = 104u; (void)net_upper_step(&receiver, 32u);
    CHECK(domain.calls == 1u && domain.rear.rear_distance_mm == 123u);
    CHECK(domain.rear.source_age_ms == 4u && domain.delivery.capture.captured_at_ms == 103u);
    CHECK(net_upper_tx_snapshot(&sender, key, &state) == NET_OK && state.outcome == NET_TX_UNKNOWN);
    CHECK(net_upper_retire(&sender, key) == NET_HOLD);
    printf("{\"case\":\"%s\",\"entry\":\"net_upper_cis_publish_rear\",\"wire_id\":160,\"wire_age\":3,\"owner\":\"FakeDomainIf\",\"rear_mm\":123,\"received_age\":4,\"remote_execution_claimed\":false,\"result\":\"PASS\"}\n", case_name);
    return 0;
}
static int event_orders(void)
{
    static const unsigned orders[6][3] = {{0,1,2},{0,2,1},{1,0,2},{1,2,0},{2,0,1},{2,1,0}};
    size_t i,j;
    for (i = 0u; i < 6u; ++i) {
        NetWireCisRearRaw r=rear_value(1u,123u); NetSourceStamp s=stamp(1u);
        NetAgePolicy p=policy(); NetTxKey key; NetUpperTxSnapshot snap;
        CHECK(init_service(&sender,&sender_port,NET_WIRE_NODE_CIS,NULL)==NET_OK);
        CHECK(net_upper_cis_publish_rear(&sender,&r,&s,&p,&key)==NET_OK);
        (void)net_upper_step(&sender,32u);
        for(j=0u;j<3u;++j) {
            NetUpperEvent e=tx_event(key,orders[i][j]);
            CHECK(push(&sender_port,&e)); (void)net_upper_step(&sender,32u);
            CHECK(net_upper_tx_snapshot(&sender,key,&snap)==NET_OK);
            if(j<2u) CHECK(net_upper_retire(&sender,key)==NET_HOLD);
        }
        CHECK(snap.admission_known&&snap.accepted&&snap.reference_released&&snap.outcome==NET_TX_SUCCEEDED);
        CHECK(net_upper_retire(&sender,key)==NET_OK);
        CHECK(net_upper_tx_snapshot(&sender,key,&snap)==NET_STALE);
        {
            NetUpperObservation before, after;
            NetUpperEvent late=tx_event(key,3u);
            CHECK(net_upper_observation(&sender,&before)==NET_OK);
            CHECK(push(&sender_port,&late));(void)net_upper_step(&sender,32u);
            CHECK(net_upper_observation(&sender,&after)==NET_OK);
            CHECK(after.tx_stale==before.tx_stale+1u&&after.tx_succeeded==before.tx_succeeded);
            CHECK(net_upper_tx_snapshot(&sender,key,&snap)==NET_STALE);
        }
        printf("{\"case\":\"%s\",\"order\":\"%u%u%u\",\"retire_only_after_all_facts\":true,\"result\":\"PASS\"}\n",case_name,orders[i][0],orders[i][1],orders[i][2]);
    }
    return 0;
}
static int capacity(void)
{
    NetWireCisRearRaw r; NetSourceStamp s; NetAgePolicy p=policy();
    NetTxKey keys[NET_UPPER_TX_CAPACITY], rejected={1234u,5678u};
    NetUpperTxSnapshot snap; size_t i;
    CHECK(init_service(&sender,&sender_port,NET_WIRE_NODE_CIS,NULL)==NET_OK);
    for(i=0u;i<NET_UPPER_TX_CAPACITY;++i) {
        r=rear_value((uint32_t)i+1u,123u);s=stamp(i+1u);
        CHECK(net_upper_cis_publish_rear(&sender,&r,&s,&p,&keys[i])==NET_OK);
    }
    r=rear_value(100u,123u);s=stamp(100u);
    CHECK(net_upper_cis_publish_rear(&sender,&r,&s,&p,&rejected)==NET_NO_CAPACITY);
    CHECK(rejected.epoch==1234u&&rejected.token==5678u);
    (void)net_upper_step(&sender,32u);
    CHECK(sender_port.submissions>0u);
    {
        NetUpperEvent release=tx_event(keys[0],2u);
        CHECK(push(&sender_port,&release));(void)net_upper_step(&sender,32u);
        CHECK(net_upper_retire(&sender,keys[0])==NET_HOLD);
        CHECK(net_upper_cis_publish_rear(&sender,&r,&s,&p,&rejected)==NET_NO_CAPACITY);
        release=tx_event(keys[0],0u);CHECK(push(&sender_port,&release));
        release=tx_event(keys[0],1u);CHECK(push(&sender_port,&release));
        (void)net_upper_step(&sender,32u);
    }
    CHECK(net_upper_tx_snapshot(&sender,keys[0],&snap)==NET_OK&&snap.outcome==NET_TX_SUCCEEDED);
    CHECK(net_upper_retire(&sender,keys[0])==NET_OK);
    CHECK(net_upper_cis_publish_rear(&sender,&r,&s,&p,&rejected)==NET_OK);
    CHECK(rejected.token==keys[NET_UPPER_TX_CAPACITY-1u].token+1u);
    return 0;
}
static int reentry(void)
{
    NetWireCisRearRaw r=rear_value(1u,123u);NetSourceStamp s=stamp(1u);
    NetAgePolicy p=policy();NetTxKey key;
    CHECK(init_service(&sender,&sender_port,NET_WIRE_NODE_CIS,NULL)==NET_OK);
    sender_port.reenter=&sender;sender_port.nested_rear=rear_value(2u,321u);
    sender_port.nested_source=stamp(2u);sender_port.nested_policy=p;
    sender_port.nested_key.epoch=77u;sender_port.nested_key.token=88u;
    CHECK(net_upper_cis_publish_rear(&sender,&r,&s,&p,&key)==NET_OK);
    (void)net_upper_step(&sender,32u);
    CHECK(sender_port.nested_step==NET_BUSY&&sender_port.nested_cancel==NET_BUSY&&sender_port.nested_submit==NET_BUSY);
    CHECK(sender_port.nested_key.epoch==77u&&sender_port.nested_key.token==88u);
    CHECK(sender_port.submissions==1u);
    return 0;
}
static int can_metadata(void)
{
    unsigned n;
    for(n=0u;n<11u;++n) {
        NetWireCisRearRaw r=rear_value(1u,123u);NetUpperEvent e=rear_event(&r);
        CHECK(init_service(&receiver,&receiver_port,NET_WIRE_NODE_DOMAIN,&domain)==NET_OK);
        switch(n) {
        case 0:e.can.dlc=10u;break; case 1:e.can.fdf=false;break;
        case 2:e.can.brs=false;break;case 3:e.can.extended=true;break;
        case 4:e.can.rtr=true;break;case 5:e.can.length=19u;break;
        case 6:e.can.bytes[19]=1u;break;case 7:e.can.driver_error=true;break;
        case 8:e.capture.timestamp_known=false;break;case 9:e.capture.errors=1u;break;
        default:e.capture.overflow=1u;break;
        }
        CHECK(push(&receiver_port,&e));(void)net_upper_step(&receiver,32u);
        CHECK(domain.calls==0u);
    }
    return 0;
}
static int receive_order(void)
{
    NetWireCisRearRaw r=rear_value(2u,123u);NetUpperEvent e;
    NetUpperCanMessage saved;NetUpperDelivery delivery;NetSourceStamp source;
    CHECK(init_service(&receiver,&receiver_port,NET_WIRE_NODE_DOMAIN,&domain)==NET_OK);
    e=rear_event(&r);CHECK(push(&receiver_port,&e));(void)net_upper_step(&receiver,32u);CHECK(domain.calls==1u);
    CHECK(push(&receiver_port,&e));(void)net_upper_step(&receiver,32u);CHECK(domain.calls==1u);
    r.tx_sequence=1u;e=rear_event(&r);CHECK(push(&receiver_port,&e));(void)net_upper_step(&receiver,32u);CHECK(domain.calls==1u);
    r.tx_sequence=3u;r.boot_id=42u;e=rear_event(&r);CHECK(push(&receiver_port,&e));(void)net_upper_step(&receiver,32u);CHECK(domain.calls==1u);
    CHECK(net_upper_received_can(&receiver,0x0a0u,&saved,&delivery,&source)==NET_OK);
    CHECK(saved.data.cis_rear.boot_id==41u&&saved.data.cis_rear.tx_sequence==2u);
    return 0;
}
static int owner_reject(void)
{
    NetWireCisRearRaw r=rear_value(1u,123u);NetUpperEvent e;
    NetUpperCanMessage saved;NetUpperDelivery delivery;NetSourceStamp source;
    NetUpperObservation observation;
    CHECK(init_service(&receiver,&receiver_port,NET_WIRE_NODE_DOMAIN,&domain)==NET_OK);
    domain.status=NET_REJECTED;e=rear_event(&r);
    CHECK(push(&receiver_port,&e));(void)net_upper_step(&receiver,32u);
    CHECK(domain.calls==1u);
    CHECK(net_upper_received_can(&receiver,0x0a0u,&saved,&delivery,&source)!=NET_OK);
    domain.status=NET_OK;
    CHECK(push(&receiver_port,&e));(void)net_upper_step(&receiver,32u);
    CHECK(domain.calls==2u);
    CHECK(net_upper_received_can(&receiver,0x0a0u,&saved,&delivery,&source)==NET_OK);
    CHECK(net_upper_observation(&receiver,&observation)==NET_OK && observation.consumer_rejected==1u);
    r.tx_sequence=2u;r.update_sequence=78u;e=rear_event(&r);domain.status=NET_BUSY;
    CHECK(push(&receiver_port,&e));CHECK(net_upper_step(&receiver,32u)==NET_BUSY);
    CHECK(domain.calls==3u);
    CHECK(net_upper_observation(&receiver,&observation)==NET_OK && observation.consumer_rejected==2u);
    CHECK(net_upper_received_can(&receiver,0x0a0u,&saved,&delivery,&source)==NET_OK);
    CHECK(saved.data.cis_rear.update_sequence==77u); /* Rejected Owner did not replace receipt. */
    return 0;
}
static int age_boundaries(void)
{
    unsigned n;
    for(n=0u;n<3u;++n) {
        NetWireCisRearRaw r=rear_value(1u,123u);NetUpperEvent e;
        CHECK(init_service(&receiver,&receiver_port,NET_WIRE_NODE_DOMAIN,&domain)==NET_OK);
        r.source_age_ms=(uint16_t)(499u+n);e=rear_event(&r);
        CHECK(push(&receiver_port,&e));(void)net_upper_step(&receiver,32u);
        CHECK(domain.calls==(n==0u?1u:0u));
    }
    {
        NetWireCisRearRaw r=rear_value(1u,0u);NetUpperEvent e;
        CHECK(init_service(&receiver,&receiver_port,NET_WIRE_NODE_DOMAIN,&domain)==NET_OK);
        e=rear_event(&r);CHECK(push(&receiver_port,&e));(void)net_upper_step(&receiver,32u);
        CHECK(domain.calls==1u&&domain.rear.rear_distance_mm==0u);
        r.tx_sequence=2u;r.rear_distance_deci_cm=0u;
        e=rear_event(&r);
        e.can.bytes[14]=255u;e.can.bytes[15]=255u;e.can.bytes[16]=1u;
        CHECK(push(&receiver_port,&e));(void)net_upper_step(&receiver,32u);
        CHECK(domain.calls==1u);
    }
    return 0;
}
static int time_failures(void)
{
    NetWireCisRearRaw r=rear_value(1u,123u);NetSourceStamp s=stamp(1u);
    NetAgePolicy p=policy();NetTxKey key={77u,88u};
    CHECK(init_service(&sender,&sender_port,NET_WIRE_NODE_CIS,NULL)==NET_OK);
    sender_port.clock_status=NET_HOLD;
    CHECK(net_upper_cis_publish_rear(&sender,&r,&s,&p,&key)!=NET_OK);
    CHECK(key.epoch==77u&&key.token==88u&&sender_port.submissions==0u);
    sender_port.clock_status=NET_OK;sender_port.now=99u;
    CHECK(net_upper_cis_publish_rear(&sender,&r,&s,&p,&key)!=NET_OK);
    sender_port.now=100u;CHECK(net_upper_cis_publish_rear(&sender,&r,&s,&p,&key)==NET_OK);
    sender_port.now=99u;(void)net_upper_step(&sender,32u);CHECK(sender_port.submissions==0u);
    return 0;
}
static int overflow_health(void)
{
    NetWireCisRearRaw r=rear_value(1u,123u);NetUpperEvent e=rear_event(&r);
    NetUpperObservation o;size_t i;
    CHECK(init_service(&receiver,&receiver_port,NET_WIRE_NODE_DOMAIN,&domain)==NET_OK);
    for(i=0u;i<NET_UPPER_RX_CAPACITY;++i) CHECK(net_upper_capture(&receiver,&e)==NET_OK);
    CHECK(net_upper_capture(&receiver,&e)==NET_NO_CAPACITY);
    CHECK(net_upper_observation(&receiver,&o)==NET_OK&&o.rx_dropped==1u);
    receiver.observation.rx_dropped=UINT32_MAX;
    CHECK(net_upper_capture(&receiver,&e)==NET_NO_CAPACITY);
    CHECK(net_upper_observation(&receiver,&o)==NET_OK&&o.rx_dropped==UINT32_MAX);
    (void)net_upper_step(&receiver,NET_UPPER_RX_CAPACITY);
    CHECK(domain.calls==1u);
    return 0;
}
static int reset_borrow(void)
{
    NetWireCisRearRaw r=rear_value(1u,123u);NetSourceStamp s=stamp(1u);
    NetAgePolicy p=policy();NetTxKey key;NetUpperEvent e={0};
    NetUpperPacket saved;const NetUpperPacket *address;
    CHECK(init_service(&sender,&sender_port,NET_WIRE_NODE_CIS,NULL)==NET_OK);
    CHECK(net_upper_cis_publish_rear(&sender,&r,&s,&p,&key)==NET_OK);
    (void)net_upper_step(&sender,32u);
    address=sender_port.borrowed[0];CHECK(address!=NULL);saved=*address;
    e.kind=NET_UPPER_LINK_RESET;e.link=NET_WIRE_CAN;e.capture.port_epoch=2u;
    e.capture.timestamp_known=true;e.capture.captured_at_ms=100u;
    CHECK(push(&sender_port,&e));(void)net_upper_step(&sender,32u);
    CHECK(sender_port.borrowed[0]==address&&memcmp(address,&saved,sizeof(saved))==0);
    CHECK(net_upper_retire(&sender,key)==NET_HOLD);
    CHECK(net_upper_cancel(&sender,key)!=NET_OK);
    return 0;
}
static int conflict(void)
{
    NetWireCisRearRaw r=rear_value(1u,123u);NetSourceStamp s=stamp(1u);
    NetAgePolicy p=policy();NetTxKey key;NetUpperTxSnapshot snap;NetUpperEvent e;
    CHECK(init_service(&sender,&sender_port,NET_WIRE_NODE_CIS,NULL)==NET_OK);
    CHECK(net_upper_cis_publish_rear(&sender,&r,&s,&p,&key)==NET_OK);(void)net_upper_step(&sender,32u);
    e=tx_event(key,3u);CHECK(push(&sender_port,&e));(void)net_upper_step(&sender,32u);
    CHECK(push(&sender_port,&e));(void)net_upper_step(&sender,32u);
    e.tx.outcome=NET_TX_FAILED;CHECK(push(&sender_port,&e));(void)net_upper_step(&sender,32u);
    CHECK(net_upper_tx_snapshot(&sender,key,&snap)==NET_OK&&snap.conflict);
    CHECK(net_upper_retire(&sender,key)==NET_HOLD);
    return 0;
}
static int permission_revoke(void)
{
    NetWireDomainPermissionRaw permission={0};NetUpperEvent e={0};
    NetUpperCanMessage accepted;NetUpperDelivery d;NetSourceStamp source;
    NetUpperUartMessage relay={0};NetAgePolicy p=policy();NetTxKey key;
    NetUpperTxSnapshot state;
    CHECK(init_service(&sender,&sender_port,NET_WIRE_NODE_CIS,&domain)==NET_OK);
    /* Owner-provided prior ALLOW is a fixture input; Network cannot create it. */
    permission.boot_id=51u;permission.tx_sequence=1u;permission.update_sequence=7u;
    permission.vehicle_power_permission=1u;
    e.kind=NET_UPPER_CAN_RX;e.link=NET_WIRE_CAN;e.capture.port_epoch=1u;
    e.capture.timestamp_known=true;e.capture.captured_at_ms=100u;
    CHECK(net_wire_can_encode_permission(NET_WIRE_CANDIDATE_ONLY,&permission,&e.can)==NET_WIRE_OK);
    CHECK(push(&sender_port,&e));(void)net_upper_step(&sender,32u);
    CHECK(domain.can_calls==1u);
    CHECK(net_upper_received_can(&sender,0x080u,&accepted,&d,&source)==NET_OK);
    relay.link=NET_WIRE_UART_C;relay.type=0x62u;relay.sequence=1u;
    relay.data.p_permission.cis_boot_id=41u;
    relay.data.p_permission.update_sequence=permission.update_sequence;
    relay.data.p_permission.vehicle_power_permission=1u;
    relay.data.p_permission.use_limit_ms=1500u;
    CHECK(net_upper_submit_uart(&sender,&relay,&source,&p,&key)==NET_OK);
    permission.tx_sequence=2u;permission.vehicle_power_permission=0u;
    permission.update_sequence=8u;
    /* Restriction has a new CAN composite revision. The original Owner success
     * may be unchanged, but reusing CAN UPDATE for different semantics is invalid. */
    CHECK(net_wire_can_encode_permission(NET_WIRE_CANDIDATE_ONLY,&permission,&e.can)==NET_WIRE_OK);
    CHECK(push(&sender_port,&e));(void)net_upper_step(&sender,32u);
    CHECK(domain.can_calls==2u);
    CHECK(sender_port.submissions==0u);
    CHECK(net_upper_tx_snapshot(&sender,key,&state)==NET_OK&&!state.queued&&!state.submitted);
    CHECK(state.admission_known&&!state.accepted&&state.reference_released);
    return 0;
}
static int source_rejuvenation(void)
{
    NetWireCisRearRaw r=rear_value(1u,123u);NetUpperEvent e;
    CHECK(init_service(&receiver,&receiver_port,NET_WIRE_NODE_DOMAIN,&domain)==NET_OK);
    r.source_age_ms=100u;e=rear_event(&r);
    CHECK(push(&receiver_port,&e));(void)net_upper_step(&receiver,32u);
    CHECK(domain.calls==1u&&domain.rear.source_age_ms==100u);
    r.tx_sequence=2u;r.source_age_ms=0u;
    e=rear_event(&r);e.capture.captured_at_ms=101u;receiver_port.now=101u;
    CHECK(push(&receiver_port,&e));(void)net_upper_step(&receiver,32u);
    CHECK(domain.calls==1u); /* New transport sequence cannot make update77 young. */
    r.tx_sequence=3u;r.update_sequence=78u;e=rear_event(&r);e.capture.captured_at_ms=101u;
    CHECK(push(&receiver_port,&e));(void)net_upper_step(&receiver,32u);
    CHECK(domain.calls==2u&&domain.rear.update_sequence==78u&&domain.rear.source_age_ms==0u);
    return 0;
}
static NetUpperUartMessage vision_value(uint16_t sequence)
{
    NetUpperUartMessage m={0};m.link=NET_WIRE_UART_C;m.type=0x60u;m.sequence=sequence;
    m.data.p_vision_result.pi_boot_id=41u;m.data.p_vision_result.vision_sequence=77u;
    m.data.p_vision_result.occupant_presence=1u;m.data.p_vision_result.presence_validity=1u;
    m.data.p_vision_result.occupant_count=3u;m.data.p_vision_result.count_validity=1u;
    m.data.p_vision_result.vision_status=2u;
    return m;
}
static int tx_age_mismatch(void)
{
    NetWireCisRearRaw r=rear_value(1u,123u);NetSourceStamp s=stamp(1u);
    NetAgePolicy p=policy();NetTxKey key={88u,99u};NetUpperUartMessage m;
    NetWireCisRearRaw decoded;NetWirePVisionResult vision;uint16_t sequence;
    CHECK(init_service(&sender,&sender_port,NET_WIRE_NODE_CIS,NULL)==NET_OK);
    r.source_age_ms=100u;
    CHECK(net_upper_cis_publish_rear(&sender,&r,&s,&p,&key)==NET_HOLD);
    CHECK(key.epoch==88u && key.token==99u && sender.next_token==1u);
    CHECK(sender_port.submissions==0u);
    s.original_age_ms=100u;
    CHECK(net_upper_cis_publish_rear(&sender,&r,&s,&p,&key)==NET_OK && key.token==1u);
    sender_port.now=110u;CHECK(net_upper_step(&sender,32u)==NET_OK);
    CHECK(net_wire_can_decode_rear(NET_WIRE_CANDIDATE_ONLY,&sender_port.borrowed[0]->can,&decoded)==NET_WIRE_OK);
    CHECK(decoded.source_age_ms==110u);
    CHECK(init_service(&sender,&sender_port,NET_WIRE_NODE_PI,NULL)==NET_OK);
    m=vision_value(1u);m.data.p_vision_result.vision_age_ms=100u;s=stamp(2u);
    key.epoch=88u;key.token=99u;
    CHECK(net_upper_submit_uart(&sender,&m,&s,&p,&key)==NET_HOLD);
    CHECK(key.epoch==88u && key.token==99u && sender.next_token==1u && sender_port.submissions==0u);
    s.original_age_ms=100u;
    CHECK(net_upper_submit_uart(&sender,&m,&s,&p,&key)==NET_OK && key.token==1u);
    sender_port.now=110u;CHECK(net_upper_step(&sender,32u)==NET_OK);
    CHECK(net_wire_p_vision_result_decode(NET_WIRE_CANDIDATE_ONLY,&sender_port.borrowed[0]->uart,&vision,&sequence)==NET_WIRE_OK);
    CHECK(vision.vision_age_ms==110u && sequence==1u);
    return 0;
}
static int feed_vision(const NetUpperUartMessage *m,uint64_t captured)
{
    NetWireUartFrame frame;NetUpperEvent e={0};size_t i;
    CHECK(net_upper_uart_encode(NET_WIRE_CANDIDATE_ONLY,m,&frame)==NET_WIRE_OK);
    e.kind=NET_UPPER_UART_BYTE;e.link=NET_WIRE_UART_C;
    e.capture.timestamp_known=true;e.capture.port_epoch=1u;e.capture.captured_at_ms=captured;
    receiver_port.now=captured;
    for(i=0u;i<frame.length;++i){e.byte=frame.bytes[i];CHECK(net_upper_capture(&receiver,&e)==NET_OK);}
    (void)net_upper_step(&receiver,32u);
    return 0;
}
static int uart_source_rejuvenation(void)
{
    NetUpperUartMessage m=vision_value(1u);
    CHECK(init_service(&receiver,&receiver_port,NET_WIRE_NODE_CIS,&domain)==NET_OK);
    m.data.p_vision_result.vision_age_ms=100u;CHECK(feed_vision(&m,100u)==0);
    CHECK(domain.uart_calls==1u && domain.delivery.age_ms==100u);
    m.sequence=2u;m.data.p_vision_result.vision_age_ms=0u;CHECK(feed_vision(&m,101u)==0);
    CHECK(domain.uart_calls==1u);
    m.sequence=3u;m.data.p_vision_result.vision_age_ms=100u;CHECK(feed_vision(&m,101u)==0);
    CHECK(domain.uart_calls==1u); /* Same observation must include elapsed 1 ms. */
    m.sequence=4u;m.data.p_vision_result.vision_age_ms=101u;CHECK(feed_vision(&m,101u)==0);
    CHECK(domain.uart_calls==2u && domain.delivery.age_ms==101u);
    m.sequence=5u;m.data.p_vision_result.vision_sequence=78u;m.data.p_vision_result.vision_age_ms=0u;
    CHECK(feed_vision(&m,101u)==0);CHECK(domain.uart_calls==3u && domain.delivery.age_ms==0u);
    return 0;
}
static int wrong_link_outcome(void)
{
    NetWireCisRearRaw r=rear_value(1u,123u);NetSourceStamp s=stamp(1u);
    NetAgePolicy p=policy();NetTxKey key;NetUpperEvent e;NetUpperTxSnapshot state;
    NetUpperObservation before,after;
    CHECK(init_service(&sender,&sender_port,NET_WIRE_NODE_CIS,NULL)==NET_OK);
    CHECK(net_upper_cis_publish_rear(&sender,&r,&s,&p,&key)==NET_OK);
    CHECK(net_upper_step(&sender,32u)==NET_OK);
    e=tx_event(key,3u);e.link=NET_WIRE_UART_C;
    CHECK(net_upper_capture(&sender,&e)==NET_OK);
    CHECK(net_upper_step(&sender,32u)==NET_REJECTED);
    CHECK(net_upper_tx_snapshot(&sender,key,&state)==NET_OK);
    CHECK(!state.admission_known && state.outcome==NET_TX_UNKNOWN && !state.reference_released && !state.conflict);
    CHECK(net_upper_retire(&sender,key)==NET_HOLD);
    CHECK(net_upper_observation(&sender,&before)==NET_OK);
    e.tx.key.token+=100u;CHECK(net_upper_capture(&sender,&e)==NET_OK);
    CHECK(net_upper_step(&sender,32u)==NET_STALE);
    CHECK(net_upper_observation(&sender,&after)==NET_OK && after.tx_stale==before.tx_stale+1u);
    e=tx_event(key,3u);CHECK(net_upper_capture(&sender,&e)==NET_OK);
    CHECK(net_upper_step(&sender,32u)==NET_OK);
    CHECK(net_upper_tx_snapshot(&sender,key,&state)==NET_OK && state.accepted && state.outcome==NET_TX_SUCCEEDED && state.reference_released);
    CHECK(net_upper_retire(&sender,key)==NET_OK);
    return 0;
}
int main(int argc,char **argv)
{
    int result=1;
    if(argc!=2)return 2;
    case_name=argv[1];
    if(strcmp(case_name,"product_rear")==0)result=product_rear();
    else if(strcmp(case_name,"event_orders")==0)result=event_orders();
    else if(strcmp(case_name,"capacity")==0)result=capacity();
    else if(strcmp(case_name,"reentry")==0)result=reentry();
    else if(strcmp(case_name,"can_metadata")==0)result=can_metadata();
    else if(strcmp(case_name,"receive_order")==0)result=receive_order();
    else if(strcmp(case_name,"owner_reject")==0)result=owner_reject();
    else if(strcmp(case_name,"age_boundaries")==0)result=age_boundaries();
    else if(strcmp(case_name,"time_failures")==0)result=time_failures();
    else if(strcmp(case_name,"overflow_health")==0)result=overflow_health();
    else if(strcmp(case_name,"reset_borrow")==0)result=reset_borrow();
    else if(strcmp(case_name,"conflict")==0)result=conflict();
    else if(strcmp(case_name,"permission_revoke")==0)result=permission_revoke();
    else if(strcmp(case_name,"source_rejuvenation")==0)result=source_rejuvenation();
    else if(strcmp(case_name,"tx_age_mismatch")==0)result=tx_age_mismatch();
    else if(strcmp(case_name,"uart_source_rejuvenation")==0)result=uart_source_rejuvenation();
    else if(strcmp(case_name,"wrong_link_outcome")==0)result=wrong_link_outcome();
    else return 2;
    if(result==0)printf("{\"case\":\"%s\",\"entry\":\"product_net_upper\",\"fake_scope\":\"Port_and_external_Owner_only\",\"result\":\"PASS\"}\n",case_name);
    return result;
}
