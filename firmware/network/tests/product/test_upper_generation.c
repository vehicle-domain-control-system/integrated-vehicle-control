#include "network/net_upper.h"
#include <stdio.h>
#include <string.h>

/* Sender runs (BOOT_ID) and repetitions of one original observation at the
 * service level. Only the Port and the Owner are faked; every frame goes
 * through the real codec, admission and source rules.
 * docs/network/interface/04_COMMON/10_DATA_AND_ID_RULES.md:
 *   generation: an accepted change of the sending run invalidates its earlier
 *     context; an ended run is not accepted again (A -> B -> late A);
 *   repeat and current quality: a new TX_SEQUENCE / LINK_SEQUENCE carrying the
 *     same original observation is a normal repetition, an exact duplicate is
 *     not; quality and AGE: a re-reception never lowers the accumulated AGE.
 * VSS hand-over C4: the receiver keeps the 4 most recently replaced BOOTs. */
#define PROFILE NET_WIRE_CANDIDATE_ONLY
#define CHECK(x) do { if (!(x)) { printf("FAIL %s:%d %s\n",case_name,__LINE__,#x); return 1; } } while (0)
static const char *case_name;
typedef struct { uint64_t now; } FakePort;
typedef struct {
    unsigned calls, accepted;
    NetStatus status;
    NetUpperCanMessage can;
    NetUpperUartDelivery uart;
    NetUpperDelivery delivery;
} FakeOwner;
static NetUpper service;
static FakePort port;
static FakeOwner owner;

static NetStatus now_ms(void *p,uint64_t *out) { *out=((FakePort *)p)->now; return NET_OK; }
static NetStatus submit(void *p,const NetUpperPacket *packet,NetTxKey key) { (void)p;(void)packet;(void)key;return NET_BUSY; }
static NetStatus poll(void *p,NetUpperEvent *out) { (void)p;(void)out;return NET_NO_WORK; }
static const NetUpperPortOps ops={now_ms,submit,poll};
static NetStatus can_owner(void *p,const NetUpperCanMessage *m,const NetUpperDelivery *d)
{ FakeOwner *o=p;++o->calls;if(o->status==NET_OK){++o->accepted;o->can=*m;o->delivery=*d;}return o->status; }
static NetStatus uart_owner(void *p,const NetUpperUartDelivery *m,const NetUpperDelivery *d)
{ FakeOwner *o=p;++o->calls;if(o->status==NET_OK){++o->accepted;o->uart=*m;o->delivery=*d;}return o->status; }
static size_t route(NetWireLink link,uint16_t id)
{
    size_t i;for(i=0u;i<net_wire_registry_count();++i){const NetWireRegistryEntry *r=net_wire_registry_at(i);if(r->link==link&&r->id_type==id)return i;}
    return NET_UPPER_ROUTE_COUNT;
}
/* repeat_route: route that passes repetitions to the Owner, or ROUTE_COUNT for none. */
static int setup(uint32_t node,size_t repeat_route)
{
    NetUpperConfig c;size_t i;
    memset(&c,0,sizeof(c));
    memset(&service,0,sizeof(service));memset(&port,0,sizeof(port));memset(&owner,0,sizeof(owner));
    port.now=100u;owner.status=NET_OK;c.node=node;c.local_epoch=9u;c.profile=PROFILE;
    c.port.ops=&ops;c.port.context=&port;c.owner.context=&owner;
    c.owner.can=can_owner;c.owner.uart=uart_owner;
    for(i=0u;i<3u;++i)c.port_epoch[i]=1u;
    for(i=0u;i<NET_UPPER_ROUTE_COUNT;++i){c.rx_policy[i].bound=true;c.rx_policy[i].maximum_age_ms=5000u;c.rx_policy[i].expires_at_equality=true;}
    for(i=0u;i<2u;++i){c.uart_policy[i].candidate_time_policy_selected=true;c.uart_policy[i].assembly_limit_ms=1000u;c.uart_policy[i].expires_at_equality=true;}
    if(repeat_route<NET_UPPER_ROUTE_COUNT)c.rx_repeat_delivery[repeat_route]=true;
    CHECK(net_upper_init(&service,&c)==NET_OK);return 0;
}
static NetUpperObservation observed(void)
{ NetUpperObservation o;memset(&o,0,sizeof(o));(void)net_upper_observation(&service,&o);return o; }
static NetStatus can_message(const NetUpperCanMessage *m)
{
    NetUpperEvent e;NetStatus r;
    memset(&e,0,sizeof(e));e.kind=NET_UPPER_CAN_RX;e.link=NET_WIRE_CAN;
    e.capture.port_epoch=1u;e.capture.timestamp_known=true;e.capture.captured_at_ms=port.now;
    if(net_upper_can_encode(PROFILE,m,&e.can)!=NET_WIRE_OK)return NET_INVALID_ARGUMENT;
    r=net_upper_capture(&service,&e);return r==NET_OK?net_upper_step(&service,1u):r;
}
static NetStatus uart_message(const NetUpperUartMessage *m)
{
    NetWireUartFrame f;NetUpperEvent e;size_t i;NetStatus r=NET_NO_WORK;
    if(net_upper_uart_encode(PROFILE,m,&f)!=NET_WIRE_OK)return NET_INVALID_ARGUMENT;
    memset(&e,0,sizeof(e));e.kind=NET_UPPER_UART_BYTE;e.link=m->link;
    e.capture.port_epoch=1u;e.capture.timestamp_known=true;e.capture.captured_at_ms=port.now;
    for(i=0u;i<f.length;++i){e.byte=f.bytes[i];r=net_upper_capture(&service,&e);if(r!=NET_OK)return r;r=net_upper_step(&service,1u);if(i+1u<f.length&&r!=NET_NO_WORK)return r;}
    return r;
}
static NetUpperCanMessage event_of(uint32_t boot,uint32_t tx,uint32_t occurrence)
{
    NetUpperCanMessage m;memset(&m,0,sizeof(m));m.id=0x160u;
    m.data.vss_event.boot_id=boot;m.data.vss_event.tx_sequence=tx;m.data.vss_event.event_kind=1u;
    m.data.vss_event.occurrence_id=occurrence;m.data.vss_event.original_age_ms=10u;m.data.vss_event.use_limit_ms=500u;
    m.data.vss_event.value_quality=0u;m.data.vss_event.input_availability=0u;
    return m;
}
static NetUpperCanMessage warning_of(uint32_t boot,uint32_t tx,uint16_t age)
{
    NetUpperCanMessage m;NetWireVssWarningStateRaw *w=&m.data.vss_warning_state;
    memset(&m,0,sizeof(m));m.id=0x0c0u;
    w->boot_id=boot;w->tx_sequence=tx;
    w->antipinch_source_boot=0x57494E31u;w->antipinch_source_sequence=11u;w->antipinch_original_age_ms=age;
    w->antipinch_state=1u;w->antipinch_quality=0u;w->antipinch_reason=0u;w->antipinch_availability=0u;
    w->occupant_hazard_source_boot=0x43495331u;w->occupant_hazard_source_sequence=22u;w->occupant_hazard_original_age_ms=age;
    w->occupant_hazard_state=0u;w->occupant_hazard_quality=0u;w->occupant_hazard_reason=0u;w->occupant_hazard_availability=0u;
    w->rear_obstacle_source_boot=0x43495332u;w->rear_obstacle_source_sequence=33u;w->rear_obstacle_original_age_ms=age;
    w->rear_obstacle_state=1u;w->rear_obstacle_quality=0u;w->rear_obstacle_reason=0u;w->rear_obstacle_availability=0u;
    w->rear_detection_activation=1u;w->activation_meta_format=1u;w->rear_activation_quality=0u;
    w->rear_activation_reason=0u;w->rear_activation_sequence=44u;w->rear_activation_age_ms=age;
    return m;
}
static NetUpperUartMessage vision_of(uint32_t pi_boot,uint16_t link,uint32_t sequence,uint16_t age)
{
    NetUpperUartMessage m;NetWirePVisionResult *v=&m.data.p_vision_result;
    memset(&m,0,sizeof(m));m.link=NET_WIRE_UART_C;m.type=0x60u;m.sequence=link;
    v->pi_boot_id=pi_boot;v->vision_sequence=sequence;v->vision_age_ms=age;
    v->occupant_presence=1u;v->presence_validity=1u;v->presence_reason=0u;
    v->occupant_count=2u;v->count_validity=1u;v->count_reason=0u;v->vision_status=1u;v->vision_fault_code=0u;
    return m;
}

/* The ended runs of a key are kept 4 deep, the oldest leaves first. */
static int ended_depth(void)
{
    static const uint32_t runs[6]={0xA1u,0xB2u,0xC3u,0xD4u,0xE5u,0xF6u};
    NetUpperCanMessage m;NetUpperObservation o;size_t i;
    CHECK(setup(NET_WIRE_NODE_VSS,NET_UPPER_ROUTE_COUNT)==0);
    for(i=0u;i<6u;++i){m=event_of(runs[i],1u,(uint32_t)(i+1u));CHECK(can_message(&m)==NET_OK);}
    CHECK(owner.accepted==6u);
    o=observed();CHECK(o.generation_changes==5u&&o.old_generation==0u);
    /* B..E: ended, refused (TX far above anything they sent). */
    for(i=1u;i<5u;++i){m=event_of(runs[i],100u,(uint32_t)(10u+i));CHECK(can_message(&m)==NET_STALE);}
    CHECK(owner.calls==6u);
    o=observed();CHECK(o.old_generation==4u&&o.stale==4u);
    /* A left the list when the fifth run ended: no longer known as ended. */
    m=event_of(runs[0],2u,20u);CHECK(can_message(&m)==NET_OK);
    CHECK(owner.accepted==7u&&owner.delivery.source_boot==runs[0]);
    /* F is now ended in turn. */
    m=event_of(runs[5],2u,21u);CHECK(can_message(&m)==NET_STALE);
    printf("{\"case\":\"%s\",\"ended_depth\":%u,\"result\":\"PASS\"}\n",case_name,(unsigned)NET_UPPER_ENDED_BOOTS);
    return 0;
}

/* The new run is committed only with an accepted frame: an Owner refusal of
 * its first frame leaves the key on the old run (one frame, one result). */
static int owner_refused_no_switch(void)
{
    NetUpperCanMessage m;NetUpperObservation o;
    CHECK(setup(NET_WIRE_NODE_VSS,NET_UPPER_ROUTE_COUNT)==0);
    m=event_of(0xA1u,5u,1u);CHECK(can_message(&m)==NET_OK);
    owner.status=NET_REJECTED;
    m=event_of(0xB2u,1u,1u);CHECK(can_message(&m)==NET_REJECTED);
    owner.status=NET_OK;
    o=observed();CHECK(o.generation_changes==0u&&o.consumer_rejected==1u);
    /* Still run A: its next TX is taken in order. */
    m=event_of(0xA1u,6u,2u);CHECK(can_message(&m)==NET_OK);CHECK(owner.delivery.source_boot==0xA1u);
    /* Run B again: now taken, A ends. */
    m=event_of(0xB2u,1u,1u);CHECK(can_message(&m)==NET_OK);CHECK(owner.delivery.source_boot==0xB2u);
    m=event_of(0xA1u,7u,3u);CHECK(can_message(&m)==NET_STALE);
    o=observed();CHECK(o.generation_changes==1u&&o.old_generation==1u);
    return 0;
}

/* Repetitions reach the Owner only on an opted-in route; elsewhere (and on
 * every route without the opt-in, e.g. the DOMAIN profile) the same original
 * observation refreshes the receipt record without an Owner call. */
static int repeat_scope(void)
{
    NetUpperCanMessage m;NetUpperObservation o;uint64_t receipt;
    CHECK(setup(NET_WIRE_NODE_VSS,route(NET_WIRE_CAN,0x0c0u))==0);
    m=warning_of(0xA1u,1u,100u);CHECK(can_message(&m)==NET_OK);
    receipt=owner.delivery.receipt_id;CHECK(!owner.delivery.repeat);
    port.now+=50u;m=warning_of(0xA1u,2u,150u);CHECK(can_message(&m)==NET_OK);
    CHECK(owner.accepted==2u&&owner.delivery.repeat&&owner.delivery.receipt_id==receipt);
    CHECK(owner.delivery.source_sequence==2u&&owner.can.data.vss_warning_state.occupant_hazard_original_age_ms==150u);
    o=observed();CHECK(o.repeat_delivered==1u&&o.rx_delivered==1u&&o.duplicates==0u);
    /* The same occurrence again on VSS_EVENT (no opt-in): no second call. */
    m=event_of(0xA1u,1u,7u);CHECK(can_message(&m)==NET_OK);
    port.now+=10u;m=event_of(0xA1u,2u,7u);m.data.vss_event.original_age_ms=20u;CHECK(can_message(&m)==NET_DUPLICATE);
    CHECK(owner.accepted==3u);
    /* Without the opt-in the warning repetition is a duplicate as before. */
    CHECK(setup(NET_WIRE_NODE_VSS,NET_UPPER_ROUTE_COUNT)==0);
    m=warning_of(0xA1u,1u,100u);CHECK(can_message(&m)==NET_OK);
    port.now+=50u;m=warning_of(0xA1u,2u,150u);CHECK(can_message(&m)==NET_DUPLICATE);
    CHECK(owner.calls==1u);
    o=observed();CHECK(o.repeat_delivered==0u&&o.duplicates==1u);
    return 0;
}

/* CAN allowance 5 ms: a repetition 5 ms short is raised, 6 ms short is held.
 * A raised AGE is the new record, so the next on-time repetition fits it. */
static int repeat_can_slack(void)
{
    NetUpperCanMessage m;NetUpperObservation o;
    CHECK(setup(NET_WIRE_NODE_VSS,route(NET_WIRE_CAN,0x0c0u))==0);
    m=warning_of(0xA1u,1u,100u);CHECK(can_message(&m)==NET_OK);
    port.now+=55u;m=warning_of(0xA1u,2u,150u);CHECK(can_message(&m)==NET_OK);
    CHECK(owner.delivery.repeat&&owner.can.data.vss_warning_state.rear_activation_age_ms==155u);
    CHECK(owner.can.data.vss_warning_state.antipinch_original_age_ms==155u);
    o=observed();CHECK(o.repeat_age_raised==1u);
    port.now+=56u;m=warning_of(0xA1u,3u,205u);CHECK(can_message(&m)==NET_HOLD);
    CHECK(owner.accepted==2u);
    port.now+=1u;m=warning_of(0xA1u,4u,212u);CHECK(can_message(&m)==NET_OK);
    CHECK(owner.accepted==3u&&owner.can.data.vss_warning_state.occupant_hazard_original_age_ms==212u);
    o=observed();CHECK(o.repeat_delivered==2u&&o.repeat_age_raised==1u&&o.rx_held==1u);
    return 0;
}

/* UART allowance 50 ms on a Pi Vision repetition (new LINK_SEQUENCE, same Pi
 * BOOT and VISION_SEQUENCE): 50 ms short is raised, 51 ms short is held. */
static int repeat_uart_slack(void)
{
    NetUpperUartMessage m;NetUpperObservation o;uint64_t receipt;
    CHECK(setup(NET_WIRE_NODE_CIS,route(NET_WIRE_UART_C,0x60u))==0);
    m=vision_of(0x50490001u,1u,500u,20u);CHECK(uart_message(&m)==NET_OK);
    receipt=owner.delivery.receipt_id;
    port.now+=100u;m=vision_of(0x50490001u,2u,500u,70u);CHECK(uart_message(&m)==NET_OK);
    CHECK(owner.delivery.repeat&&owner.delivery.receipt_id==receipt&&owner.delivery.source_sequence==2u);
    CHECK(owner.uart.message.data.p_vision_result.vision_age_ms==120u);
    port.now+=100u;m=vision_of(0x50490001u,3u,500u,169u);CHECK(uart_message(&m)==NET_HOLD);
    CHECK(owner.accepted==2u);
    o=observed();CHECK(o.repeat_delivered==1u&&o.repeat_age_raised==1u&&o.rx_held==1u&&o.rx_delivered==1u);
    /* A new judgement is a new observation again. */
    m=vision_of(0x50490001u,4u,501u,5u);CHECK(uart_message(&m)==NET_OK);
    CHECK(!owner.delivery.repeat&&owner.delivery.receipt_id!=receipt);
    return 0;
}

/* UART-C: a new Pi run is judged before the LINK_SEQUENCE window and moves the
 * receive direction; an ended Pi run is refused on every type of the link. */
static int uart_c_generation(void)
{
    NetUpperUartMessage m;NetUpperObservation o;
    CHECK(setup(NET_WIRE_NODE_CIS,NET_UPPER_ROUTE_COUNT)==0);
    m=vision_of(0xA1u,40000u,10u,5u);CHECK(uart_message(&m)==NET_OK);
    port.now+=10u;m=vision_of(0xB2u,0u,1u,5u);CHECK(uart_message(&m)==NET_OK);
    CHECK(owner.uart.message.data.p_vision_result.pi_boot_id==0xB2u&&owner.delivery.source_sequence==0u);
    port.now+=10u;m=vision_of(0xA1u,40001u,11u,5u);CHECK(uart_message(&m)==NET_STALE);
    port.now+=10u;m=vision_of(0xB2u,1u,2u,5u);CHECK(uart_message(&m)==NET_OK);
    CHECK(owner.accepted==3u);
    o=observed();CHECK(o.generation_changes==1u&&o.old_generation==1u);
    return 0;
}

/* CAN TX_SEQUENCE starts at 1 in every sender run (docs/network/interface/
 * 03_COMMUNICATION/10_CAN_FD.md, TX_SEQUENCE: first 1). A frame carrying 0 is
 * a structure error (VSS hand-over C5: structure checks are Network's first,
 * a structure error frame causes no generation change): refused as invalid
 * with no Owner call, as the very first frame of a key, as the first frame of
 * a new run and inside the accepted run; the key never moves on it. */
static int tx_zero_refused(void)
{
    NetUpperCanMessage m;NetUpperObservation o;
    CHECK(setup(NET_WIRE_NODE_VSS,route(NET_WIRE_CAN,0x0c0u))==0);
    /* The very first frame of the key. */
    m=event_of(0xA1u,0u,1u);CHECK(can_message(&m)==NET_REJECTED);
    m=warning_of(0xA1u,0u,100u);CHECK(can_message(&m)==NET_REJECTED);
    CHECK(owner.calls==0u);
    o=observed();CHECK(o.rx_invalid==2u&&o.rx_delivered==0u&&o.generation_changes==0u);
    /* Run A then starts from TX 1. */
    m=event_of(0xA1u,1u,1u);CHECK(can_message(&m)==NET_OK);
    CHECK(owner.accepted==1u&&owner.delivery.source_boot==0xA1u&&owner.delivery.source_sequence==1u);
    /* The first frame of a new run B with TX 0: refused, the key stays on A. */
    m=event_of(0xB2u,0u,1u);CHECK(can_message(&m)==NET_REJECTED);
    CHECK(owner.calls==1u);
    o=observed();CHECK(o.rx_invalid==3u&&o.generation_changes==0u&&o.old_generation==0u);
    m=event_of(0xA1u,2u,2u);CHECK(can_message(&m)==NET_OK);
    CHECK(owner.accepted==2u&&owner.delivery.source_boot==0xA1u&&owner.delivery.source_sequence==2u);
    /* B from TX 1 is the new run, A has ended. */
    m=event_of(0xB2u,1u,1u);CHECK(can_message(&m)==NET_OK);
    CHECK(owner.accepted==3u&&owner.delivery.source_boot==0xB2u);
    /* TX 0 inside the accepted run is refused the same way. */
    m=event_of(0xB2u,0u,2u);CHECK(can_message(&m)==NET_REJECTED);
    m=event_of(0xA1u,3u,3u);CHECK(can_message(&m)==NET_STALE);
    CHECK(owner.calls==3u);
    o=observed();CHECK(o.rx_invalid==4u&&o.generation_changes==1u&&o.old_generation==1u);
    /* On the repeat route the same: a warning repetition with TX 0 is refused. */
    m=warning_of(0xB2u,1u,100u);CHECK(can_message(&m)==NET_OK);
    port.now+=50u;m=warning_of(0xB2u,0u,150u);CHECK(can_message(&m)==NET_REJECTED);
    CHECK(owner.calls==4u);
    o=observed();CHECK(o.rx_invalid==5u&&o.repeat_delivered==0u);
    return 0;
}

int main(int argc,char **argv)
{
    static const struct { const char *name; int (*run)(void); } cases[]={
        {"ended_depth",ended_depth},{"owner_refused_no_switch",owner_refused_no_switch},
        {"repeat_scope",repeat_scope},{"repeat_can_slack",repeat_can_slack},
        {"repeat_uart_slack",repeat_uart_slack},{"uart_c_generation",uart_c_generation},
        {"tx_zero_refused",tx_zero_refused}
    };
    size_t i;
    if(argc!=2){printf("usage: %s <case>\n",argv[0]);return 2;}
    for(i=0u;i<sizeof(cases)/sizeof(cases[0]);++i)
        if(strcmp(argv[1],cases[i].name)==0){case_name=cases[i].name;return cases[i].run();}
    printf("unknown case %s\n",argv[1]);return 2;
}
