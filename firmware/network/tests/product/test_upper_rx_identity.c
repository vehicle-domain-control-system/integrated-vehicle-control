#include "network/net_upper.h"
#include <stdio.h>
#include <string.h>

/* Independent P1 regression: only the physical Port and external Owner are
 * faked. Every RX byte/frame passes through the real product service, codec,
 * admission and adapter. No C-structure representation is compared. */
#define PROFILE NET_WIRE_CANDIDATE_ONLY
#define CHECK(x) do { if (!(x)) { printf("FAIL %s:%d %s\n",case_name,__LINE__,#x); return 1; } } while (0)
static const char *case_name;
typedef struct { uint64_t now; const NetUpperPacket *borrowed; NetTxKey key; } FakePort;
typedef struct {
    unsigned calls, accepted;
    NetStatus status;
    NetUpperRear rear;
    NetUpperCanMessage can;
    NetUpperUartDelivery uart;
    NetUpperDelivery delivery;
} FakeOwner;
typedef struct {
    NetUpperAdmission admission;
    NetUpperDelivery delivery;
    NetWireCanEnvelope can;
    NetWireUartFrame uart, context;
    uint64_t next_receipt;
    unsigned calls, accepted;
} Snapshot;
static NetUpper service;
static FakePort port;
static FakeOwner owner;
static int expected_status(NetStatus actual,NetStatus expected)
{
    printf("TRACE %s actual=%d expected=%d owner_calls=%u accepted=%u duplicate_count=%u next_receipt=%llu\n",
        case_name,(int)actual,(int)expected,owner.calls,owner.accepted,service.observation.duplicates,
        (unsigned long long)service.next_receipt);
    return actual==expected;
}

static NetStatus now_ms(void *p,uint64_t *out) { *out=((FakePort *)p)->now; return NET_OK; }
static NetStatus submit(void *p,const NetUpperPacket *packet,NetTxKey key)
{ FakePort *f=p; if(f->borrowed!=NULL)return NET_BUSY; f->borrowed=packet;f->key=key;return NET_OK; }
static NetStatus poll(void *p,NetUpperEvent *out) { (void)p;(void)out;return NET_NO_WORK; }
static const NetUpperPortOps ops={now_ms,submit,poll};
static NetStatus rear_owner(void *p,const NetUpperRear *r,const NetUpperDelivery *d)
{ FakeOwner *o=p;++o->calls;if(o->status==NET_OK){++o->accepted;o->rear=*r;o->delivery=*d;}return o->status; }
static NetStatus can_owner(void *p,const NetUpperCanMessage *m,const NetUpperDelivery *d)
{ FakeOwner *o=p;++o->calls;if(o->status==NET_OK){++o->accepted;o->can=*m;o->delivery=*d;}return o->status; }
static NetStatus uart_owner(void *p,const NetUpperUartDelivery *m,const NetUpperDelivery *d)
{ FakeOwner *o=p;++o->calls;if(o->status==NET_OK){++o->accepted;o->uart=*m;o->delivery=*d;}return o->status; }
static int setup(uint32_t node)
{
    NetUpperConfig c={0};size_t i;
    memset(&service,0,sizeof(service));memset(&port,0,sizeof(port));memset(&owner,0,sizeof(owner));
    port.now=100u;owner.status=NET_OK;c.node=node;c.local_epoch=9u;c.profile=PROFILE;
    c.port.ops=&ops;c.port.context=&port;c.owner.context=&owner;
    c.owner.rear=rear_owner;c.owner.can=can_owner;c.owner.uart=uart_owner;
    for(i=0u;i<3u;++i)c.port_epoch[i]=1u;
    for(i=0u;i<NET_UPPER_ROUTE_COUNT;++i){c.rx_policy[i].bound=true;c.rx_policy[i].maximum_age_ms=5000u;c.rx_policy[i].expires_at_equality=true;}
    for(i=0u;i<2u;++i){c.uart_policy[i].candidate_time_policy_selected=true;c.uart_policy[i].assembly_limit_ms=1000u;c.uart_policy[i].expires_at_equality=true;}
    CHECK(net_upper_init(&service,&c)==NET_OK);return 0;
}
static size_t route(NetWireLink link,uint16_t id)
{
    size_t i;for(i=0u;i<net_wire_registry_count();++i){const NetWireRegistryEntry *r=net_wire_registry_at(i);if(r->link==link&&r->id_type==id)return i;}
    return NET_UPPER_ROUTE_COUNT;
}
static int equal_delivery(const NetUpperDelivery *a,const NetUpperDelivery *b)
{
    return a->capture.port_epoch==b->capture.port_epoch&&a->capture.captured_at_ms==b->capture.captured_at_ms&&
        a->capture.timestamp_known==b->capture.timestamp_known&&a->capture.errors==b->capture.errors&&
        a->capture.overflow==b->capture.overflow&&a->delivered_at_ms==b->delivered_at_ms&&
        a->local_elapsed_ms==b->local_elapsed_ms&&a->age_ms==b->age_ms&&a->receipt_id==b->receipt_id&&
        a->source_boot==b->source_boot&&a->source_sequence==b->source_sequence&&
        a->freshness_known==b->freshness_known&&a->per_field_age_required==b->per_field_age_required;
}
static int equal_can(const NetWireCanEnvelope *a,const NetWireCanEnvelope *b)
{
    return a->standard_id==b->standard_id&&a->extended==b->extended&&a->fdf==b->fdf&&
        a->brs==b->brs&&a->rtr==b->rtr&&a->driver_error==b->driver_error&&a->esi==b->esi&&a->dlc==b->dlc&&
        a->length==b->length&&memcmp(a->bytes,b->bytes,a->length)==0;
}
static int equal_uart(const NetWireUartFrame *a,const NetWireUartFrame *b)
{ return a->length==b->length&&memcmp(a->bytes,b->bytes,a->length)==0; }
static int snapshot(size_t i,Snapshot *s)
{
    CHECK(i<NET_UPPER_ROUTE_COUNT);memset(s,0,sizeof(*s));s->admission=service.admission[i];
    s->next_receipt=service.next_receipt;s->calls=owner.calls;s->accepted=owner.accepted;s->delivery=owner.delivery;
    if(net_wire_registry_at(i)->link==NET_WIRE_CAN)CHECK(net_upper_can_encode(PROFILE,&s->admission.can,&s->can)==NET_WIRE_OK);
    else CHECK(net_upper_uart_encode(PROFILE,&s->admission.uart,&s->uart)==NET_WIRE_OK);
    if(service.context_seen)CHECK(net_wire_m_context_encode(PROFILE,0u,&service.context,&s->context)==NET_WIRE_OK);
    return 0;
}
static int unchanged(size_t i,const Snapshot *s)
{
    NetWireCanEnvelope c;NetWireUartFrame u;
    const NetUpperAdmission *a=&service.admission[i];
    CHECK(a->seen==s->admission.seen&&a->boot==s->admission.boot&&a->sequence==s->admission.sequence);
    CHECK(equal_delivery(&a->delivery,&s->admission.delivery));
    CHECK(service.next_receipt==s->next_receipt&&owner.calls==s->calls&&owner.accepted==s->accepted);
    CHECK(equal_delivery(&owner.delivery,&s->delivery));
    if(net_wire_registry_at(i)->link==NET_WIRE_CAN){CHECK(net_upper_can_encode(PROFILE,&a->can,&c)==NET_WIRE_OK);CHECK(equal_can(&c,&s->can));}
    else {CHECK(net_upper_uart_encode(PROFILE,&a->uart,&u)==NET_WIRE_OK);CHECK(equal_uart(&u,&s->uart));}
#ifndef P1_BASELINE_REPLAY
    CHECK(a->raw_direction==s->admission.raw_direction);
    if(net_wire_registry_at(i)->link==NET_WIRE_CAN)CHECK(equal_can(&a->raw.can,&s->admission.raw.can));
    else {CHECK(equal_uart(&a->raw.uart,&s->admission.raw.uart));CHECK(a->raw.uart.captured_at_ms==s->admission.raw.uart.captured_at_ms);}
#endif
    if(s->context.length!=0u){CHECK(service.context_seen);CHECK(net_wire_m_context_encode(PROFILE,0u,&service.context,&u)==NET_WIRE_OK);CHECK(equal_uart(&u,&s->context));}
    return 0;
}
static int observation(uint32_t duplicates,uint32_t conflicts)
{
    NetUpperObservation o;CHECK(net_upper_observation(&service,&o)==NET_OK);CHECK(o.duplicates==duplicates);
#ifndef P1_BASELINE_REPLAY
    CHECK(o.rx_conflicts==conflicts);
#else
    /* Historical header has no rx_conflicts member. The exact same required
     * status/Owner/ledger expectations still fail against that historical core. */
    (void)conflicts;
#endif
    return 0;
}
static NetStatus can_frame(const NetWireCanEnvelope *f,NetUpperCapture c)
{
    NetUpperEvent e={0};NetStatus r;e.kind=NET_UPPER_CAN_RX;e.link=NET_WIRE_CAN;e.can=*f;e.capture=c;
    r=net_upper_capture(&service,&e);return r==NET_OK?net_upper_step(&service,1u):r;
}
static NetUpperCapture capture(void)
{ NetUpperCapture c={0};c.port_epoch=1u;c.timestamp_known=true;c.captured_at_ms=port.now;return c; }
static NetStatus can_message(const NetUpperCanMessage *m)
{ NetWireCanEnvelope f;if(net_upper_can_encode(PROFILE,m,&f)!=NET_WIRE_OK)return NET_INVALID_ARGUMENT;return can_frame(&f,capture()); }
static NetStatus uart_frame(NetWireLink link,const NetWireUartFrame *f)
{
    size_t i;NetStatus r=NET_NO_WORK;NetUpperEvent e={0};e.kind=NET_UPPER_UART_BYTE;e.link=link;e.capture=capture();
    for(i=0u;i<f->length;++i){e.byte=f->bytes[i];r=net_upper_capture(&service,&e);if(r!=NET_OK)return r;r=net_upper_step(&service,1u);if(i+1u<f->length&&r!=NET_NO_WORK)return r;}
    return r;
}
static NetStatus uart_message(const NetUpperUartMessage *m)
{ NetWireUartFrame f;if(net_upper_uart_encode(PROFILE,m,&f)!=NET_WIRE_OK)return NET_INVALID_ARGUMENT;return uart_frame(m->link,&f); }
static NetUpperCanMessage rear_sample(void)
{
    NetUpperCanMessage m={0};m.id=0x0a0u;m.data.cis_rear.boot_id=41u;m.data.cis_rear.tx_sequence=2u;
    m.data.cis_rear.update_sequence=77u;m.data.cis_rear.source_age_ms=10u;m.data.cis_rear.rear_distance_deci_cm=123u;
    m.data.cis_rear.validity=1u;return m;
}
static NetUpperCanMessage permission_sample(void)
{
    NetUpperCanMessage m={0};m.id=0x080u;m.data.domain_permission.boot_id=51u;m.data.domain_permission.tx_sequence=7u;
    m.data.domain_permission.update_sequence=3u;m.data.domain_permission.source_age_ms=10u;m.data.domain_permission.vehicle_power_permission=1u;return m;
}
static NetUpperUartMessage context_sample(void)
{
    NetUpperUartMessage m={0};NetWireMContext *c=&m.data.m_context;m.link=NET_WIRE_UART_M;m.type=0x01u;m.sequence=4u;
    c->device_context_id=12u;c->session_id=34u;c->device_registration_state=1u;c->bt_connection_state=1u;
    c->app_active_state=1u;c->app_update_sequence=3u;c->app_age_ms=10u;c->proximity_state=1u;c->proximity_validity=1u;
    c->proximity_update_sequence=4u;c->proximity_age_ms=10u;c->registration_connection_update_sequence=5u;c->registration_connection_age_ms=10u;
    return m;
}
static NetUpperUartMessage uart_sample(unsigned kind)
{
    NetUpperUartMessage m={0};m.sequence=7u;
    if(kind==0u){NetWirePVisionResult *v=&m.data.p_vision_result;m.link=NET_WIRE_UART_C;m.type=0x60u;
        v->pi_boot_id=41u;v->vision_sequence=77u;v->vision_age_ms=10u;v->occupant_presence=1u;v->presence_validity=1u;v->occupant_count=3u;v->count_validity=1u;v->vision_status=2u;
    }else if(kind==1u){m.link=NET_WIRE_UART_M;m.type=0x10u;m.data.m_request.device_context_id=12u;m.data.m_request.session_id=34u;m.data.m_request.request_id=50u;m.data.m_request.kind=1u;m.data.m_request.operation=1u;
    }else if(kind==2u){m=context_sample();
    }else {NetWireMResult *r=&m.data.m_result;m.link=NET_WIRE_UART_M;m.type=0x20u;r->device_context_id=12u;r->session_id=34u;r->domain_boot_id=51u;r->query_id=1u;r->request_session=34u;r->request_id=50u;r->result=2u;r->result_reason=9u;r->result_confirmation=1u;r->command_target=1u;r->command_id=701u;r->origin=1u;r->result_age_ms=10u;}
    return m;
}
static int bootstrap(unsigned kind)
{
    NetUpperUartMessage c=context_sample();NetSourceStamp source={1u,100u,10u};NetAgePolicy p={true,5000u,true};NetTxKey key;NetUpperEvent e={0};NetUpperTxSnapshot tx;
    if(kind==1u){CHECK(uart_message(&c)==NET_OK);}
    if(kind==3u){
        CHECK(net_upper_submit_uart(&service,&c,&source,&p,&key)==NET_OK);CHECK(net_upper_step(&service,1u)==NET_OK);
        CHECK(port.borrowed!=NULL&&port.borrowed->link==NET_WIRE_UART_M);
        e.kind=NET_UPPER_TX_EVENT;e.link=NET_WIRE_UART_M;e.tx.key=key;e.tx.admission_observed=true;e.tx.accepted=true;e.tx.release_observed=true;e.tx.outcome=NET_TX_SUCCEEDED;
        port.borrowed=NULL;CHECK(net_upper_capture(&service,&e)==NET_OK);CHECK(net_upper_step(&service,1u)==NET_OK);
        CHECK(net_upper_tx_snapshot(&service,key,&tx)==NET_OK&&tx.reference_released&&tx.outcome==NET_TX_SUCCEEDED);
        CHECK(net_upper_retire(&service,key)==NET_OK);
    }
    return 0;
}
static int can_case(int permission)
{
    NetUpperCanMessage m=permission?permission_sample():rear_sample(),changed=m;Snapshot s;size_t i=route(NET_WIRE_CAN,m.id);
    CHECK(setup(permission?NET_WIRE_NODE_CIS:NET_WIRE_NODE_DOMAIN)==0);CHECK(can_message(&m)==NET_OK);CHECK(snapshot(i,&s)==0);
    port.now=101u;CHECK(can_message(&m)==NET_DUPLICATE);CHECK(unchanged(i,&s)==0);CHECK(observation(1u,0u)==0);
    if(permission)changed.data.domain_permission.vehicle_power_permission=0u;else changed.data.cis_rear.rear_distance_deci_cm=321u;
    CHECK(expected_status(can_message(&changed),NET_HOLD));CHECK(unchanged(i,&s)==0);CHECK(observation(1u,1u)==0);
    changed=m;if(permission)++changed.data.domain_permission.source_age_ms;else ++changed.data.cis_rear.source_age_ms;
    CHECK(expected_status(can_message(&changed),NET_HOLD));CHECK(unchanged(i,&s)==0);CHECK(observation(1u,2u)==0);
    CHECK(can_message(&m)==NET_DUPLICATE);CHECK(unchanged(i,&s)==0);CHECK(observation(2u,2u)==0);
    if(permission){++changed.data.domain_permission.tx_sequence;++changed.data.domain_permission.update_sequence;changed.data.domain_permission.vehicle_power_permission=0u;}
    else {++changed.data.cis_rear.tx_sequence;++changed.data.cis_rear.update_sequence;changed.data.cis_rear.rear_distance_deci_cm=321u;}
    CHECK(can_message(&changed)==NET_OK);CHECK(owner.accepted==2u&&service.next_receipt==s.next_receipt+1u);
    if(permission)CHECK(owner.can.data.domain_permission.vehicle_power_permission==0u);else CHECK(owner.rear.rear_distance_mm==321u);
    CHECK(observation(2u,2u)==0);return 0;
}
static int uart_case(unsigned kind)
{
    NetUpperUartMessage m=uart_sample(kind),changed=m;Snapshot s;size_t i=route(m.link,m.type);uint32_t conflicts=1u;
    CHECK(setup(kind==0u?NET_WIRE_NODE_CIS:(kind==3u?NET_WIRE_NODE_ESP32:NET_WIRE_NODE_DOMAIN))==0);
    CHECK(bootstrap(kind)==0);CHECK(uart_message(&m)==NET_OK);CHECK(snapshot(i,&s)==0);
    port.now=101u;CHECK(uart_message(&m)==NET_DUPLICATE);CHECK(unchanged(i,&s)==0);CHECK(observation(1u,0u)==0);
    if(kind==0u)changed.data.p_vision_result.occupant_count=2u;
    else if(kind==1u)++changed.data.m_request.request_id;
    else if(kind==2u)changed.data.m_context.app_active_state=0u;
    else changed.data.m_result.result=3u;
    CHECK(expected_status(uart_message(&changed),NET_HOLD));CHECK(unchanged(i,&s)==0);CHECK(observation(1u,1u)==0);
    changed=m;
    if(kind!=1u){
        if(kind==0u)++changed.data.p_vision_result.vision_age_ms;
        else if(kind==2u)++changed.data.m_context.proximity_age_ms;
        else ++changed.data.m_result.result_age_ms;
        CHECK(expected_status(uart_message(&changed),NET_HOLD));CHECK(unchanged(i,&s)==0);conflicts=2u;CHECK(observation(1u,conflicts)==0);
    }
    CHECK(uart_message(&m)==NET_DUPLICATE);CHECK(unchanged(i,&s)==0);CHECK(observation(2u,conflicts)==0);
    changed=m;++changed.sequence;
    if(kind==0u){++changed.data.p_vision_result.vision_sequence;changed.data.p_vision_result.occupant_count=2u;}
    else if(kind==1u)++changed.data.m_request.request_id;
    else if(kind==2u){++changed.data.m_context.app_update_sequence;++changed.data.m_context.proximity_update_sequence;++changed.data.m_context.registration_connection_update_sequence;}
    else ++changed.data.m_result.request_id;
    CHECK(uart_message(&changed)==NET_OK);CHECK(owner.accepted==s.accepted+1u&&service.next_receipt==s.next_receipt+1u);
    CHECK(observation(2u,conflicts)==0);
    if(kind==2u){ /* Genuine new recipient context keeps the prior explicit policy. */
        changed=m;++changed.data.m_context.session_id;changed.sequence=1u;
        CHECK(uart_message(&changed)==NET_OK);CHECK(service.context.session_id==35u);
    }
    return 0;
}
static int rejected_owner(void)
{
    unsigned k;Snapshot s;NetUpperCanMessage c=rear_sample();NetUpperUartMessage u;
    size_t i=route(NET_WIRE_CAN,c.id);CHECK(setup(NET_WIRE_NODE_DOMAIN)==0);owner.status=NET_BUSY;
    CHECK(can_message(&c)==NET_BUSY);CHECK(!service.admission[i].seen&&service.next_receipt==1u&&owner.accepted==0u);
    c.data.cis_rear.rear_distance_deci_cm=321u;owner.status=NET_OK;CHECK(can_message(&c)==NET_OK);CHECK(snapshot(i,&s)==0);
    CHECK(can_message(&c)==NET_DUPLICATE);CHECK(unchanged(i,&s)==0);CHECK(observation(1u,0u)==0);
    for(k=0u;k<4u;++k){
        CHECK(setup(k==0u?NET_WIRE_NODE_CIS:(k==3u?NET_WIRE_NODE_ESP32:NET_WIRE_NODE_DOMAIN))==0);CHECK(bootstrap(k)==0);
        u=uart_sample(k);i=route(u.link,u.type);owner.status=NET_REJECTED;
        CHECK(uart_message(&u)==NET_REJECTED);CHECK(!service.admission[i].seen);
        if(k==0u)u.data.p_vision_result.occupant_count=2u;
        else if(k==1u)++u.data.m_request.request_id;
        else if(k==2u)u.data.m_context.app_active_state=0u;
        else u.data.m_result.result=3u;
        owner.status=NET_OK;CHECK(uart_message(&u)==NET_OK);CHECK(snapshot(i,&s)==0);
        CHECK(uart_message(&u)==NET_DUPLICATE);CHECK(unchanged(i,&s)==0);CHECK(observation(1u,0u)==0);
        CHECK(service.observation.consumer_rejected==1u);
    }
    return 0;
}
static int envelope_boundaries(void)
{
    NetUpperCanMessage m=rear_sample();NetWireCanEnvelope f,other;NetUpperCapture c;Snapshot s;size_t j,i=route(NET_WIRE_CAN,m.id);
    CHECK(setup(NET_WIRE_NODE_DOMAIN)==0);CHECK(net_upper_can_encode(PROFILE,&m,&f)==NET_WIRE_OK);
    CHECK(can_frame(&f,capture())==NET_OK);CHECK(snapshot(i,&s)==0);
    /* Different native padding and unused tail bytes must never create a conflict. */
    memset(&other,0xa5,sizeof(other));other.standard_id=f.standard_id;other.extended=f.extended;other.fdf=f.fdf;
    other.brs=f.brs;other.rtr=f.rtr;other.driver_error=f.driver_error;other.esi=f.esi;other.dlc=f.dlc;other.length=f.length;memcpy(other.bytes,f.bytes,f.length);
    port.now=101u;CHECK(can_frame(&other,capture())==NET_DUPLICATE);CHECK(unchanged(i,&s)==0);CHECK(observation(1u,0u)==0);
    for(j=0u;j<5u;++j){c=capture();if(j==0u)c.timestamp_known=false;if(j==1u)c.errors=1u;if(j==2u)c.overflow=1u;if(j==3u)c.port_epoch=2u;if(j==4u)c.captured_at_ms=102u;
        CHECK(can_frame(&f,c)==(j==3u?NET_STALE:NET_HOLD));CHECK(unchanged(i,&s)==0);CHECK(observation(1u,0u)==0);}
    other=f;other.fdf=false;CHECK(can_frame(&other,capture())==NET_REJECTED);CHECK(unchanged(i,&s)==0);
    other=f;other.brs=false;CHECK(can_frame(&other,capture())==NET_REJECTED);CHECK(unchanged(i,&s)==0);
    other=f;++other.dlc;CHECK(can_frame(&other,capture())==NET_REJECTED);CHECK(unchanged(i,&s)==0);
    CHECK(observation(1u,0u)==0);CHECK(can_frame(&f,capture())==NET_DUPLICATE);CHECK(unchanged(i,&s)==0);
#ifndef P1_BASELINE_REPLAY
    /* ESI is defined transport metadata, unlike padding and unused bytes. */
    other=f;other.esi=!f.esi;
    CHECK(can_frame(&other,capture())==NET_HOLD);CHECK(unchanged(i,&s)==0);
    /* A diagnostic counter at saturation must not wrap or change admission. */
    service.observation.rx_conflicts=UINT32_MAX;
    ++m.data.cis_rear.rear_distance_deci_cm;
    CHECK(expected_status(can_message(&m),NET_HOLD));CHECK(unchanged(i,&s)==0);CHECK(observation(2u,UINT32_MAX)==0);
#endif
    { NetUpperUartMessage u=uart_sample(0u);NetWireUartFrame raw,bad;uint32_t bad_before;
      CHECK(setup(NET_WIRE_NODE_CIS)==0);i=route(u.link,u.type);CHECK(net_upper_uart_encode(PROFILE,&u,&raw)==NET_WIRE_OK);
      CHECK(uart_frame(u.link,&raw)==NET_OK);CHECK(snapshot(i,&s)==0);bad=raw;bad.bytes[bad.length-1u]^=1u;
      bad_before=service.uart_rx[1].stream.bad_crc;CHECK(uart_frame(u.link,&bad)==NET_NO_WORK);
      CHECK(service.uart_rx[1].stream.bad_crc==bad_before+1u);CHECK(unchanged(i,&s)==0);CHECK(observation(0u,0u)==0);
      for(j=raw.length;j<sizeof(raw.bytes);++j)raw.bytes[j]=0xa5u;
      raw.captured_at_ms=999u;
      port.now=101u;CHECK(uart_frame(u.link,&raw)==NET_DUPLICATE);CHECK(unchanged(i,&s)==0);CHECK(observation(1u,0u)==0);
    }
    return 0;
}
int main(int argc,char **argv)
{
    int r=1;if(argc!=2)return 2;case_name=argv[1];
    if(strcmp(case_name,"can_rear")==0)r=can_case(0);
    else if(strcmp(case_name,"can_permission")==0)r=can_case(1);
    else if(strcmp(case_name,"uart_c_vision")==0)r=uart_case(0u);
    else if(strcmp(case_name,"uart_m_request")==0)r=uart_case(1u);
    else if(strcmp(case_name,"uart_m_context")==0)r=uart_case(2u);
    else if(strcmp(case_name,"uart_m_result")==0)r=uart_case(3u);
    else if(strcmp(case_name,"rejected_owner")==0)r=rejected_owner();
    else if(strcmp(case_name,"envelope_boundaries")==0)r=envelope_boundaries();
    if(r==0)printf("PASS %s: actual product RX; exact duplicate distinct from raw conflict; Owner/accepted ledger preserved\n",case_name);
    return r;
}
