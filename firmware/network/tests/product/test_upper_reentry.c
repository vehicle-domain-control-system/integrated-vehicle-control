#include "network/net_upper.h"
#include <stdio.h>
#include <string.h>

/* Re-entry from inside a UART Owner callback, through the real product
 * service, receiver, codec and admission; only the Port and the Owner are
 * faked. The step a callback is called from is serialized: a nested step of
 * the same service is NET_BUSY. The frame lent to the callback stays exactly
 * as delivered until it returns and is the frame the step records
 * (docs/network/sw_detail/flows/UART_C_PI_TO_CIS.md: typed and meta are one
 * stable copy, lent until the callback returns and not replaced by re-entry).
 * A step of another service from inside the callback hands its own frame over
 * and leaves the lent one alone. */
#define PROFILE NET_WIRE_CANDIDATE_ONLY
#define CHECK(x) do { if (!(x)) { printf("FAIL %s:%d %s\n",case_name,__LINE__,#x); return 1; } } while (0)
static const char *case_name;
typedef struct { uint64_t now; } FakePort;
typedef struct {
    unsigned calls;
    NetStatus nested_same, nested_second;
    int lent_changed;
    NetUpperUartDelivery lent;
    NetUpperDelivery delivery;
} FakeOwner;
static NetUpper service, second;
static FakePort port;
static FakeOwner owner, second_owner;

static NetStatus now_ms(void *p,uint64_t *out) { *out=((FakePort *)p)->now; return NET_OK; }
static NetStatus submit(void *p,const NetUpperPacket *packet,NetTxKey key) { (void)p;(void)packet;(void)key;return NET_BUSY; }
static NetStatus poll(void *p,NetUpperEvent *out) { (void)p;(void)out;return NET_NO_WORK; }
static const NetUpperPortOps ops={now_ms,submit,poll};
/* Copies the lent frame byte for byte, re-enters both services, then checks
 * that the lent frame did not change while it was lent. */
static NetStatus reenter_owner(void *p,const NetUpperUartDelivery *m,const NetUpperDelivery *d)
{
    FakeOwner *o=p;++o->calls;memcpy(&o->lent,m,sizeof(*m));o->delivery=*d;
    o->nested_same=net_upper_step(&service,1u);
    o->nested_second=net_upper_step(&second,1u);
    o->lent_changed=memcmp(&o->lent,m,sizeof(*m))!=0;
    return NET_OK;
}
static NetStatus copy_owner(void *p,const NetUpperUartDelivery *m,const NetUpperDelivery *d)
{ FakeOwner *o=p;++o->calls;memcpy(&o->lent,m,sizeof(*m));o->delivery=*d;return NET_OK; }
static int setup(NetUpper *s,FakeOwner *o,NetStatus (*uart)(void *,const NetUpperUartDelivery *,const NetUpperDelivery *))
{
    NetUpperConfig c={0};size_t i;
    memset(s,0,sizeof(*s));memset(o,0,sizeof(*o));
    c.node=NET_WIRE_NODE_CIS;c.local_epoch=9u;c.profile=PROFILE;
    c.port.ops=&ops;c.port.context=&port;c.owner.context=o;c.owner.uart=uart;
    for(i=0u;i<3u;++i)c.port_epoch[i]=1u;
    for(i=0u;i<NET_UPPER_ROUTE_COUNT;++i){c.rx_policy[i].bound=true;c.rx_policy[i].maximum_age_ms=5000u;c.rx_policy[i].expires_at_equality=true;}
    for(i=0u;i<2u;++i){c.uart_policy[i].candidate_time_policy_selected=true;c.uart_policy[i].assembly_limit_ms=1000u;c.uart_policy[i].expires_at_equality=true;}
    CHECK(net_upper_init(s,&c)==NET_OK);return 0;
}
static size_t route(NetWireLink link,uint16_t id)
{
    size_t i;for(i=0u;i<net_wire_registry_count();++i){const NetWireRegistryEntry *r=net_wire_registry_at(i);if(r->link==link&&r->id_type==id)return i;}
    return NET_UPPER_ROUTE_COUNT;
}
static int equal_uart(const NetWireUartFrame *a,const NetWireUartFrame *b)
{ return a->length==b->length&&memcmp(a->bytes,b->bytes,a->length)==0; }
static NetUpperUartMessage vision(uint32_t sequence,uint8_t count)
{
    NetUpperUartMessage m={0};NetWirePVisionResult *v=&m.data.p_vision_result;
    m.link=NET_WIRE_UART_C;m.type=0x60u;m.sequence=7u;
    v->pi_boot_id=41u;v->vision_sequence=sequence;v->vision_age_ms=10u;v->occupant_presence=1u;v->presence_validity=1u;
    v->occupant_count=count;v->count_validity=1u;v->vision_status=2u;return m;
}
static NetUpperEvent byte_event(uint8_t byte)
{
    NetUpperEvent e={0};e.kind=NET_UPPER_UART_BYTE;e.link=NET_WIRE_UART_C;e.byte=byte;
    e.capture.port_epoch=1u;e.capture.timestamp_known=true;e.capture.captured_at_ms=port.now;return e;
}
static int uart_owner(void)
{
    NetUpperUartMessage a=vision(77u,3u),b=vision(78u,2u);NetWireUartFrame fa,fb;NetUpperEvent e;
    size_t j,i=route(NET_WIRE_UART_C,0x60u);NetStatus r=NET_NO_WORK;
    port.now=100u;
    CHECK(setup(&service,&owner,reenter_owner)==0);CHECK(setup(&second,&second_owner,copy_owner)==0);
    CHECK(net_upper_uart_encode(PROFILE,&a,&fa)==NET_WIRE_OK&&net_upper_uart_encode(PROFILE,&b,&fb)==NET_WIRE_OK);
    /* The second service holds all of its frame but the last byte in its
     * receiver and that last byte in its queue: its next step completes it. */
    for(j=0u;j<fb.length;++j){e=byte_event(fb.bytes[j]);CHECK(net_upper_capture(&second,&e)==NET_OK);
        if(j+1u<fb.length)CHECK(net_upper_step(&second,1u)==NET_NO_WORK);}
    CHECK(second_owner.calls==0u);
    for(j=0u;j<fa.length;++j){e=byte_event(fa.bytes[j]);CHECK(net_upper_capture(&service,&e)==NET_OK);r=net_upper_step(&service,1u);
        if(j+1u<fa.length)CHECK(r==NET_NO_WORK);}
    CHECK(r==NET_OK);
    CHECK(owner.calls==1u&&owner.nested_same==NET_BUSY&&owner.nested_second==NET_OK);
    CHECK(owner.lent_changed==0);
    CHECK(owner.lent.message.type==0x60u&&owner.lent.message.data.p_vision_result.vision_sequence==77u);
    CHECK(equal_uart(&owner.lent.frame,&fa)&&owner.lent.port_epoch==1u);
    CHECK(second_owner.calls==1u&&second_owner.lent.message.data.p_vision_result.vision_sequence==78u);
    CHECK(equal_uart(&second_owner.lent.frame,&fb));
    CHECK(i<NET_UPPER_ROUTE_COUNT&&service.admission[i].seen&&equal_uart(&service.admission[i].raw.uart,&fa));
    CHECK(service.admission[i].uart.data.p_vision_result.vision_sequence==77u&&service.admission[i].uart.data.p_vision_result.occupant_count==3u);
    CHECK(second.admission[i].seen&&equal_uart(&second.admission[i].raw.uart,&fb));
    CHECK(service.observation.rx_delivered==1u&&second.observation.rx_delivered==1u&&!service.in_call&&!second.in_call);
    return 0;
}
int main(int argc,char **argv)
{
    int r=1;if(argc!=2)return 2;case_name=argv[1];
    if(strcmp(case_name,"uart_owner")==0)r=uart_owner();
    if(r==0)printf("PASS %s: nested step NET_BUSY; lent UART frame unchanged and recorded; another service keeps its own frame\n",case_name);
    return r;
}
