#include "network/net_upper_monitor.h"
#include <stdio.h>
#include <string.h>
#define CHECK(x) do{if(!(x)){fprintf(stderr,"%s:%d %s\n",__func__,__LINE__,#x);return 1;}}while(0)
static NetUpper s,sender;
static NetPermissionGuard guard;
static uint64_t now=100u;
static unsigned calls,held_calls,changed_calls;
static NetStatus now_fn(void *ctx,uint64_t *out){(void)ctx;*out=now;return NET_OK;}
static NetStatus submit(void *ctx,const NetUpperPacket *p,NetTxKey k){(void)ctx;(void)p;(void)k;return NET_NO_CAPACITY;}
static NetStatus poll_fn(void *ctx,NetUpperEvent *out){(void)ctx;(void)out;return NET_NO_WORK;}
static NetStatus consume(void *ctx,const NetUpperCanMessage *m,const NetUpperDelivery *d)
{(void)ctx;(void)m;(void)d;++calls;return NET_OK;}
static NetStatus inspect(void *ctx,const NetUpperCanMessage *m,const NetUpperDelivery *d)
{(void)ctx;(void)m;if(!d->observation_only)return NET_REJECTED;++held_calls;return NET_OK;}
static void changed(void *ctx,const NetPermissionView *view)
{(void)ctx;(void)view;++changed_calls;}
static const NetUpperPortOps ops={now_fn,submit,poll_fn};
static int setup(NetUpper *service,uint32_t node,bool inspection)
{
    NetUpperConfig c={0};size_t i;
    memset(service,0,sizeof(*service));c.node=node;c.local_epoch=1u;c.profile=NET_WIRE_CANDIDATE_ONLY;
    c.port.ops=&ops;c.owner.can=consume;c.owner.held_can=inspect;c.inspection_transport=inspection;
    for(i=0u;i<3u;++i)c.port_epoch[i]=1u;
    for(i=0u;i<NET_UPPER_ROUTE_COUNT;++i){c.rx_policy[i].bound=true;c.rx_policy[i].maximum_age_ms=1000u;}
    c.uart_sequence_bound[1]=true;c.uart_initial_sequence[1]=1u;
    c.uart_policy[1].candidate_time_policy_selected=true;c.uart_policy[1].assembly_limit_ms=100u;
    CHECK(net_upper_init(service,&c)==NET_OK);return 0;
}
static NetStatus permission_rx(unsigned sequence,uint16_t age,uint8_t quality)
{
    NetUpperEvent e={0};NetWireDomainPermissionRaw p={0};
    p.boot_id=7u;p.tx_sequence=sequence;p.update_sequence=sequence;p.source_age_ms=age;
    p.vehicle_power_permission=1u;p.power_permission_quality=quality;p.operation_allowed=1u;
    e.kind=NET_UPPER_CAN_RX;e.link=NET_WIRE_CAN;e.capture.timestamp_known=true;
    e.capture.captured_at_ms=now;e.capture.port_epoch=1u;
    if(net_wire_can_encode_permission(NET_WIRE_CANDIDATE_ONLY,&p,&e.can)!=NET_WIRE_OK)return NET_INVALID_ARGUMENT;
    if(net_upper_capture(&s,&e)!=NET_OK)return NET_NO_CAPACITY;
    return net_upper_step(&s,32u);
}
static int permission_age(void)
{
    NetPermissionPolicy p={0};NetPermissionView v;NetUpperObservation o;
    CHECK(setup(&s,NET_WIRE_NODE_CIS,false)==0);p.receive_timeout.bound=true;p.receive_timeout.maximum_age_ms=150u;
    p.receive_timeout.expires_at_equality=true;p.changed=changed;
    CHECK(net_permission_init(&guard,&s,&p)==NET_OK);
    CHECK(net_permission_step(&guard)==NET_OK);CHECK(net_permission_snapshot(&guard,&v)==NET_OK);
    CHECK(!v.has_original && v.reason==NET_PERMISSION_NO_DATA);
    CHECK(permission_rx(1u,10u,0u)==NET_OK);CHECK(net_permission_step(&guard)==NET_OK);
    CHECK(net_permission_snapshot(&guard,&v)==NET_OK && v.power_usable && v.operation_usable && !v.alive_usable);
    now=249u;CHECK(net_permission_step(&guard)==NET_OK);CHECK(net_permission_snapshot(&guard,&v)==NET_OK && v.power_usable);
    now=250u;CHECK(net_permission_step(&guard)==NET_OK);CHECK(net_permission_snapshot(&guard,&v)==NET_OK);
    CHECK(!v.power_usable && v.reason==NET_PERMISSION_RX_TIMEOUT && v.original.vehicle_power_permission==1u && v.original.source_age_ms==10u);
    CHECK(permission_rx(2u,490u,0u)==NET_OK);CHECK(net_permission_step(&guard)==NET_OK);
    now=260u;CHECK(net_permission_step(&guard)==NET_OK);CHECK(net_permission_snapshot(&guard,&v)==NET_OK);
    CHECK(v.reason==NET_PERMISSION_SOURCE_EXPIRED && !v.power_usable && v.original_age_ms==500u);
    CHECK(permission_rx(3u,1u,1u)==NET_OK);CHECK(net_permission_step(&guard)==NET_OK);CHECK(net_permission_snapshot(&guard,&v)==NET_OK);
    CHECK(v.reason==NET_PERMISSION_BAD_QUALITY && !v.power_usable && v.original.power_permission_quality==1u);
    CHECK(net_upper_observation(&s,&o)==NET_OK && o.rx_delivered==3u && changed_calls>=5u);
    printf("TRACE permission original500ms and separate Rx150ms; rawALLOW/quality retained, usability withdrawn\n");return 0;
}
static int permission_owner(void)
{
    NetPermissionPolicy p={0};NetPermissionView v;NetUpperTxSnapshot tx;uint64_t original_receipt;
    CHECK(setup(&s,NET_WIRE_NODE_CIS,false)==0);p.receive_timeout.bound=true;p.receive_timeout.maximum_age_ms=150u;p.changed=changed;
    p.relay_to_pi=true;p.cis_boot=8u;p.pi_use_limit_ms=500u;p.relay_age.bound=true;p.relay_age.maximum_age_ms=500u;
    CHECK(net_permission_init(&guard,&s,&p)==NET_OK);CHECK(permission_rx(1u,10u,0u)==NET_OK);
    CHECK(net_permission_step(&guard)==NET_OK && guard.relay_active);
    CHECK(net_upper_tx_snapshot(&s,guard.relay_key,&tx)==NET_OK && tx.queued && !tx.submitted);
    original_receipt=guard.view.delivery.receipt_id;
    CHECK(net_permission_invalidate(&guard,NET_PERMISSION_OWNER_FAILED)==NET_OK);
    CHECK(net_permission_snapshot(&guard,&v)==NET_OK && !v.power_usable && v.reason==NET_PERMISSION_OWNER_FAILED);
    CHECK(v.original.vehicle_power_permission==1u && v.delivery.receipt_id==original_receipt);
    CHECK(net_upper_tx_snapshot(&s,guard.relay_key,&tx)==NET_OK && !tx.queued && !tx.submitted);
    CHECK(net_permission_step(&guard)==NET_OK && !guard.relay_active);
    CHECK(net_permission_snapshot(&guard,&v)==NET_OK && !v.power_usable);
    ++now;CHECK(permission_rx(2u,1u,0u)==NET_OK);CHECK(net_permission_step(&guard)==NET_OK);
    CHECK(net_permission_snapshot(&guard,&v)==NET_OK && v.power_usable && v.delivery.receipt_id>original_receipt);
    CHECK(net_upper_quiesce_link(&s,NET_WIRE_CAN)==NET_OK);
    CHECK(net_permission_step(&guard)==NET_OK);
    CHECK(net_permission_snapshot(&guard,&v)==NET_OK && !v.power_usable && v.reason==NET_PERMISSION_LINK_UNAVAILABLE);
    printf("TRACE OwnerFAILED immediately fences queuedrelay; no Driverborrow fabricated; newreceipt required\n");return 0;
}
static int route_watch(void)
{
    NetRouteWatch w,before;NetRouteHealth h,old;NetAgePolicy p={true,150u,true};NetUpperDelivery d={0};
    memset(&w,0xa5,sizeof(w));before=w;
    CHECK(net_route_watch_init(&w,NET_WIRE_CAN,0x140u,&p)==NET_HOLD);CHECK(memcmp(&w,&before,sizeof(w))==0);
    CHECK(net_route_watch_init(&w,NET_WIRE_CAN,0x0a0u,&p)==NET_OK);
    CHECK(net_route_watch_read(&w,100u,1u,true,&h)==NET_OK && h.status==NET_NO_WORK && !h.usable);
    d.capture.timestamp_known=true;d.capture.captured_at_ms=80u;d.capture.port_epoch=1u;d.receipt_id=1u;
    CHECK(net_route_watch_observe(&w,&d)==NET_OK);CHECK(net_route_watch_read(&w,229u,1u,true,&h)==NET_OK && h.usable);
    CHECK(net_route_watch_read(&w,230u,1u,true,&h)==NET_OK && !h.usable && h.status==NET_EXPIRED);
    CHECK(net_route_watch_observe(&w,&d)==NET_DUPLICATE);old=h;
    CHECK(net_route_watch_read(&w,79u,1u,true,&h)==NET_HOLD && memcmp(&h,&old,sizeof(h))==0);
    CHECK(net_route_watch_read(&w,230u,2u,true,&h)==NET_OK && !h.usable && h.status==NET_HOLD);
    printf("TRACE perroute silence exactboundary; duplicate cannot refresh; no Eventtimeout or falsepeerhealth\n");return 0;
}
static int held_route(void)
{
    NetUpperCanMessage m={0},out;NetUpperDelivery d;NetSourceStamp stamp={1u,100u,0u},source;NetAgePolicy p={true,1000u,false};
    NetUpperEvent e={0};NetTxKey key;NetUpperObservation o;
    CHECK(setup(&s,NET_WIRE_NODE_WINDOW,true)==0);CHECK(setup(&sender,NET_WIRE_NODE_DOMAIN,true)==0);
    m.id=0x090u;m.data.window_command.boot_id=7u;m.data.window_command.tx_sequence=1u;
    m.data.window_command.command_id=4u;m.data.window_command.target_boot_id=8u;
    m.data.window_command.accept_limit_ms=200u;m.data.window_command.window_command=1u;
    CHECK(net_upper_submit_can(&sender,&m,&stamp,&p,&key)==NET_HOLD);
    CHECK(net_upper_submit_inspection_can(&sender,&m,&stamp,&p,&key)==NET_OK);
    e.kind=NET_UPPER_CAN_RX;e.link=NET_WIRE_CAN;e.capture.timestamp_known=true;e.capture.captured_at_ms=now;e.capture.port_epoch=1u;
    CHECK(net_upper_can_encode(NET_WIRE_CANDIDATE_ONLY,&m,&e.can)==NET_WIRE_OK);
    CHECK(net_upper_capture(&s,&e)==NET_OK);CHECK(net_upper_step(&s,32u)==NET_HOLD);
    CHECK(held_calls==1u && calls==0u);CHECK(net_upper_observation(&s,&o)==NET_OK && o.rx_held==1u && o.rx_delivered==0u);
    CHECK(net_upper_received_can(&s,m.id,&out,&d,&source)==NET_HOLD);
    CHECK(net_upper_capture(&s,&e)==NET_OK);CHECK(net_upper_step(&s,32u)==NET_DUPLICATE && held_calls==1u);
    printf("TRACE heldWINDOW bytes decoded/routed to inspection only; ordinaryOwner=0 acceptedreceipt=HOLD\n");return 0;
}
int main(int argc,char **argv)
{
    if(argc!=2)return 2;
    if(strcmp(argv[1],"permission_age")==0)return permission_age();
    if(strcmp(argv[1],"permission_owner")==0)return permission_owner();
    if(strcmp(argv[1],"route_watch")==0)return route_watch();
    if(strcmp(argv[1],"held_route")==0)return held_route();
    return 2;
}
