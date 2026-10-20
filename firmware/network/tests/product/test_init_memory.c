#include "network/net_upper.h"
#include <stdio.h>
#include <string.h>

/* Independent initialization regressions. Physical Port and external Owner
 * only are fake. Byte comparison checks no-write failure atomicity, not wire
 * equality. Large snapshots are test-owned static objects, never product scratch. */
#define CHECK(x) do { if(!(x)) { printf("FAIL %s:%d %s\n",name,__LINE__,#x); return 1; } } while(0)
static const char *name;
typedef struct { uint64_t now; unsigned clocks,submits,polls; const NetUpperPacket *borrowed; NetTxKey key; bool event_ready; NetUpperEvent event; } PhysicalPort;
typedef struct { unsigned calls; NetUpperRear rear; NetUpperDelivery delivery; } ExternalOwner;
static NetUpper a,b;
static unsigned char original[sizeof(NetUpper)],other_original[sizeof(NetUpper)];
static PhysicalPort pa,pb;
static ExternalOwner oa,ob;
static union { NetUpper output; NetUpperPortOps operations; } alias_storage;
static NetStatus clock_now(void *p,uint64_t *out)
{ PhysicalPort *f=p;++f->clocks;*out=f->now;return NET_OK; }
static NetStatus submit(void *p,const NetUpperPacket *packet,NetTxKey key)
{ PhysicalPort *f=p;++f->submits;if(f->borrowed!=NULL)return NET_BUSY;f->borrowed=packet;f->key=key;return NET_OK; }
static NetStatus poll(void *p,NetUpperEvent *out)
{ PhysicalPort *f=p;++f->polls;if(!f->event_ready)return NET_NO_WORK;*out=f->event;f->event_ready=false;return NET_OK; }
static const NetUpperPortOps ops={clock_now,submit,poll};
static NetStatus rear(void *p,const NetUpperRear *r,const NetUpperDelivery *d)
{ ExternalOwner *o=p;++o->calls;o->rear=*r;o->delivery=*d;return NET_OK; }
static NetUpperConfig config(uint32_t node,PhysicalPort *p,ExternalOwner *o)
{
    NetUpperConfig c={0};size_t i;c.node=node;c.local_epoch=9u;c.profile=NET_WIRE_CANDIDATE_ONLY;
    c.port.ops=&ops;c.port.context=p;c.owner.context=o;c.owner.rear=rear;
    for(i=0u;i<3u;++i)c.port_epoch[i]=1u;
    for(i=0u;i<NET_UPPER_ROUTE_COUNT;++i){c.rx_policy[i].bound=true;c.rx_policy[i].maximum_age_ms=5000u;c.rx_policy[i].expires_at_equality=true;}
    for(i=0u;i<2u;++i){c.uart_policy[i].candidate_time_policy_selected=true;c.uart_policy[i].assembly_limit_ms=1000u;c.uart_policy[i].expires_at_equality=true;}
    return c;
}
static void poison(void)
{
    memset(&a,0xa5,sizeof(a));memset(&b,0x6d,sizeof(b));
    memset(&pa,0,sizeof(pa));memset(&pb,0,sizeof(pb));memset(&oa,0,sizeof(oa));memset(&ob,0,sizeof(ob));pa.now=100u;pb.now=100u;
}
static int rejected(NetUpper *out,const NetUpperConfig *c,NetStatus expected)
{
    NetStatus actual;memcpy(original,out,sizeof(*out));actual=net_upper_init(out,c);
    printf("TRACE init result=%d expected=%d bytes_unchanged=%u\n",(int)actual,(int)expected,memcmp(original,out,sizeof(*out))==0);
    CHECK(actual==expected);CHECK(memcmp(original,out,sizeof(*out))==0);
    CHECK(pa.clocks==0u&&pa.submits==0u&&pa.polls==0u&&oa.calls==0u);return 0;
}
static int invalid_arguments(void)
{
    NetUpperConfig good=config(NET_WIRE_NODE_CIS,&pa,&oa),c;NetUpperPortOps invalid;size_t i;
    CHECK(net_upper_init(NULL,&good)==NET_INVALID_ARGUMENT);CHECK(rejected(&a,NULL,NET_INVALID_ARGUMENT)==0);
    c=good;c.local_epoch=0u;CHECK(rejected(&a,&c,NET_INVALID_ARGUMENT)==0);
    c=good;c.node=0u;CHECK(rejected(&a,&c,NET_INVALID_ARGUMENT)==0);
    c=good;c.node=NET_WIRE_NODE_CIS|NET_WIRE_NODE_DOMAIN;CHECK(rejected(&a,&c,NET_INVALID_ARGUMENT)==0);
    c=good;c.node=NET_WIRE_NODE_PI<<1u;CHECK(rejected(&a,&c,NET_INVALID_ARGUMENT)==0);
    c=good;c.profile=NET_WIRE_NOT_BOUND;CHECK(rejected(&a,&c,NET_HOLD)==0);
    for(i=0u;i<3u;++i){c=good;c.port_epoch[i]=0u;CHECK(rejected(&a,&c,NET_INVALID_ARGUMENT)==0);}
    c=good;c.port.ops=NULL;CHECK(rejected(&a,&c,NET_INVALID_ARGUMENT)==0);
    invalid=ops;invalid.now_ms=NULL;c=good;c.port.ops=&invalid;CHECK(rejected(&a,&c,NET_INVALID_ARGUMENT)==0);
    invalid=ops;invalid.submit=NULL;CHECK(rejected(&a,&c,NET_INVALID_ARGUMENT)==0);
    invalid=ops;invalid.poll=NULL;CHECK(rejected(&a,&c,NET_INVALID_ARGUMENT)==0);return 0;
}
static int uart_policy(bool channel_c)
{
    NetUpperConfig c=config(channel_c?NET_WIRE_NODE_CIS:NET_WIRE_NODE_DOMAIN,&pa,&oa);
    c.uart_policy[channel_c?1u:0u].assembly_limit_ms=0u;
    CHECK(rejected(&a,&c,NET_INVALID_ARGUMENT)==0);
    c.uart_policy[channel_c?1u:0u].assembly_limit_ms=1u;
    CHECK(net_upper_init(&a,&c)==NET_OK);CHECK(a.initialized&&a.next_token==1u&&a.next_receipt==1u);
    CHECK(a.uart_enabled[channel_c?1u:0u]);return 0;
}
static int config_overlap(void)
{ NetUpperConfig c=config(NET_WIRE_NODE_CIS,&pa,&oa);a.config=c;return rejected(&a,&a.config,NET_INVALID_ARGUMENT); }
static int ops_overlap(void)
{
    NetUpperConfig c=config(NET_WIRE_NODE_CIS,&pa,&oa);
    memset(&alias_storage,0x5b,sizeof(alias_storage));alias_storage.operations=ops;c.port.ops=&alias_storage.operations;
    return rejected(&alias_storage.output,&c,NET_INVALID_ARGUMENT);
}
static int context_overlap(void)
{
    static struct { NetUpper service; unsigned char owner_data[32]; } container;
    NetUpperConfig c=config(NET_WIRE_NODE_CIS,&pa,&oa);
    unsigned char expected[32];
    memset(&container,0xa5,sizeof(container));memset(expected,0xa5,sizeof(expected));
    c.port.context=&container;c.owner.context=&container;
    CHECK(net_upper_init(&container.service,&c)==NET_OK);
    CHECK(container.service.config.port.context==&container);
    CHECK(container.service.config.owner.context==&container);
    CHECK(memcmp(container.owner_data,expected,sizeof(expected))==0);
    return 0;
}

static int poisoned_success(void)
{
    NetUpperConfig c=config(NET_WIRE_NODE_CIS,&pa,&oa);size_t i;
    CHECK(net_upper_init(&a,&c)==NET_OK);CHECK(a.initialized&&!a.in_call&&!a.clock_seen);
    CHECK(a.next_token==1u&&a.next_receipt==1u&&a.rx_count==0u&&!a.context_seen);
    CHECK(!a.uart_enabled[0]&&a.uart_enabled[1]);
    for(i=0u;i<NET_UPPER_TX_CAPACITY;++i)CHECK(!a.tx[i].state.used);
    CHECK(pa.clocks==0u&&pa.submits==0u&&pa.polls==0u&&oa.calls==0u);return 0;
}
static NetWireCisRearRaw sample(void)
{
    NetWireCisRearRaw r={0};r.boot_id=41u;r.tx_sequence=2u;r.update_sequence=77u;
    r.source_age_ms=10u;r.rear_distance_deci_cm=123u;r.validity=1u;return r;
}
static int begin_borrow(NetUpper *s,PhysicalPort *p,NetTxKey *out)
{
    NetWireCisRearRaw r=sample();NetSourceStamp source={77u,100u,10u};NetAgePolicy policy={true,5000u,true};
    CHECK(net_upper_cis_publish_rear(s,&r,&source,&policy,out)==NET_OK);
    CHECK(net_upper_step(s,1u)==NET_OK&&p->borrowed!=NULL);return 0;
}
static int finish_borrow(NetUpper *s,PhysicalPort *p,NetTxKey key)
{
    memset(&p->event,0,sizeof(p->event));p->event.kind=NET_UPPER_TX_EVENT;p->event.link=NET_WIRE_CAN;
    p->event.capture.port_epoch=1u;p->event.capture.timestamp_known=true;p->event.capture.captured_at_ms=p->now;
    p->event.tx.key=key;p->event.tx.admission_observed=true;p->event.tx.accepted=true;
    p->event.tx.outcome=NET_TX_SUCCEEDED;p->event.tx.release_observed=true;
    p->borrowed=NULL;p->event_ready=true;CHECK(net_upper_step(s,1u)==NET_OK);CHECK(net_upper_retire(s,key)==NET_OK);return 0;
}
static int active_borrow(void)
{
    NetUpperConfig c=config(NET_WIRE_NODE_CIS,&pa,&oa);NetTxKey key;const NetUpperPacket *held;NetWireCanEnvelope old;
    CHECK(net_upper_init(&a,&c)==NET_OK);CHECK(begin_borrow(&a,&pa,&key)==0);held=pa.borrowed;old=held->can;
    memcpy(original,&a,sizeof(a));c.local_epoch=10u;
    CHECK(net_upper_init(&a,&c)==NET_BUSY);CHECK(memcmp(original,&a,sizeof(a))==0);
    CHECK(pa.borrowed==held&&held->can.length==old.length&&memcmp(held->can.bytes,old.bytes,old.length)==0);
    CHECK(finish_borrow(&a,&pa,key)==0);return 0;
}
static int live_reinit(void)
{
    NetUpperConfig c=config(NET_WIRE_NODE_CIS,&pa,&oa);
    CHECK(net_upper_init(&a,&c)==NET_OK);memcpy(original,&a,sizeof(a));
    CHECK(net_upper_init(&a,&c)==NET_BUSY);CHECK(memcmp(original,&a,sizeof(a))==0);return 0;
}
static int two_instances(void)
{
    NetUpperConfig ca=config(NET_WIRE_NODE_CIS,&pa,&oa),cb=config(NET_WIRE_NODE_DOMAIN,&pb,&ob);
    NetTxKey key;NetUpperEvent e={0};const NetUpperPacket *held;
    CHECK(net_upper_init(&a,&ca)==NET_OK);CHECK(begin_borrow(&a,&pa,&key)==0);held=pa.borrowed;memcpy(original,&a,sizeof(a));
    CHECK(net_upper_init(&b,&cb)==NET_OK);CHECK(memcmp(original,&a,sizeof(a))==0&&pa.borrowed==held);
    memcpy(other_original,&b,sizeof(b));cb.uart_policy[0].assembly_limit_ms=0u;
    CHECK(net_upper_init(&b,&cb)!=NET_OK);CHECK(memcmp(other_original,&b,sizeof(b))==0&&memcmp(original,&a,sizeof(a))==0);
    e.kind=NET_UPPER_CAN_RX;e.link=NET_WIRE_CAN;e.can=held->can;e.capture.port_epoch=1u;e.capture.captured_at_ms=100u;e.capture.timestamp_known=true;
    CHECK(net_upper_capture(&b,&e)==NET_OK&&net_upper_step(&b,1u)==NET_OK);
    CHECK(ob.calls==1u&&ob.rear.rear_distance_mm==123u&&ob.rear.source_age_ms==10u&&oa.calls==0u);
    CHECK(memcmp(original,&a,sizeof(a))==0);CHECK(finish_borrow(&a,&pa,key)==0);return 0;
}
static int inactive_uart_policy(void)
{
    NetUpperConfig c=config(NET_WIRE_NODE_BCM,&pa,&oa);c.uart_policy[0].assembly_limit_ms=0u;c.uart_policy[1].assembly_limit_ms=0u;
    CHECK(net_upper_init(&a,&c)==NET_OK);CHECK(!a.uart_enabled[0]&&!a.uart_enabled[1]);return 0;
}
int main(int argc,char **argv)
{
    int status;if(argc!=2)return 2;name=argv[1];poison();
    if(strcmp(name,"invalid_arguments")==0)status=invalid_arguments();
    else if(strcmp(name,"uart_m_policy")==0)status=uart_policy(false);
    else if(strcmp(name,"uart_c_policy")==0)status=uart_policy(true);
    else if(strcmp(name,"config_overlap")==0)status=config_overlap();
    else if(strcmp(name,"ops_overlap")==0)status=ops_overlap();
    else if(strcmp(name,"context_overlap")==0)status=context_overlap();
    else if(strcmp(name,"poisoned_success")==0)status=poisoned_success();
    else if(strcmp(name,"active_borrow")==0)status=active_borrow();
    else if(strcmp(name,"live_reinit")==0)status=live_reinit();
    else if(strcmp(name,"two_instances")==0)status=two_instances();
    else if(strcmp(name,"inactive_uart_policy")==0)status=inactive_uart_policy();
    else return 2;
    if(status==0)printf("PASS memory_init.%s actual_product=1\n",name);
    return status;
}
