#include "network/net_hal_adapter.h"
#include "network/net_upper_control.h"
#include <stdio.h>
#include <string.h>
#define CHECK(x) do { if(!(x)) { printf("{\"case\":\"%s\",\"line\":%d,\"assertion\":\"%s\",\"result\":\"FAIL\"}\n",name,__LINE__,#x); return 1; } } while(0)
static const char *name;
typedef struct {
    NetHalAdapter *adapter;
    uint64_t now;
    NetStatus now_status, submit_status, capture_status, reentry_status;
    unsigned depth, enters, leaves, bad, synchronous, reenter;
    size_t count;
    const NetHalTransfer *borrowed[32];
} FakeHal;
typedef struct {
    unsigned rear_calls, uart_calls;
    NetUpperRear rear;
    NetUpperUartDelivery uart;
    NetUpperDelivery delivery;
} FakeOwner;
static NetHalAdapter a,b;
static FakeHal fa,fb;
static NetUpper sender,receiver;
static FakeOwner owner;
static uintptr_t enter(void *context)
{
    FakeHal *f=context;
    if(f->depth!=0u) ++f->bad;
    ++f->enters; ++f->depth; return 0x1234u;
}
static void leave(void *context,uintptr_t previous)
{
    FakeHal *f=context;
    if(previous!=0x1234u || f->depth!=1u) ++f->bad;
    ++f->leaves; --f->depth;
}
static NetStatus now(void *context,uint64_t *out)
{
    FakeHal *f=context;
    if(f->depth!=0u) ++f->bad;
    if(f->now_status!=NET_OK) return f->now_status;
    *out=f->now; return NET_OK;
}
static NetUpperCapture capture(uint64_t epoch,uint64_t at)
{
    NetUpperCapture c={0}; c.port_epoch=epoch; c.captured_at_ms=at; c.timestamp_known=true; return c;
}
static NetStatus submit(void *context,const NetHalTransfer *transfer)
{
    FakeHal *f=context;
    if(f->depth!=0u || (uintptr_t)&transfer->packet.can.bytes[0]%4u!=0u) ++f->bad;
    if(f->count<32u) f->borrowed[f->count++]=transfer;
    else ++f->bad;
    if(f->reenter) {
        NetUpperEvent e; NetUpperPort p=net_hal_adapter_port(f->adapter);
        f->reentry_status=p.ops->poll(p.context,&e);
    }
    if(f->synchronous) {
        NetTxEvent e={0}; NetUpperCapture c=capture(transfer->port_epoch,f->now);
        e.key=transfer->key; e.admission_observed=true; e.accepted=true;
        if(f->synchronous==2u) { e.release_observed=true; e.outcome=NET_TX_SUCCEEDED; }
        f->capture_status=net_hal_capture_tx(f->adapter,transfer->packet.link,&e,&c);
    }
    return f->submit_status;
}
static const NetHalBackendOps ops={now,enter,leave,submit};
static NetHalConfig config(FakeHal *f)
{
    NetHalConfig c={0}; size_t i;
    c.binding_selected=true; c.enabled_links=7u; c.ops=&ops; c.context=f;
    for(i=0u;i<3u;++i) c.port_epoch[i]=1u;
    return c;
}
static int init(NetHalAdapter *adapter,FakeHal *f,NetHalPlatform platform)
{
    NetHalConfig c;
    memset(adapter,0,sizeof(*adapter)); memset(f,0,sizeof(*f)); f->adapter=adapter; f->now=100u;
    c=config(f);
    CHECK((platform==NET_HAL_S32K144?net_s32k144_upper_port_init(adapter,&c):net_s32k344_upper_port_init(adapter,&c))==NET_OK);
    return 0;
}
static NetWireCisRearRaw rear(void)
{
    NetWireCisRearRaw r={0}; r.boot_id=41u; r.tx_sequence=2u; r.update_sequence=77u;
    r.rear_distance_deci_cm=123u; r.validity=1u; return r;
}
static NetUpperPacket packet(void)
{
    NetUpperPacket p={0}; NetWireCisRearRaw r=rear();
    p.link=NET_WIRE_CAN; p.direction=NET_WIRE_CAN_DIRECTION;
    if(net_wire_can_encode_rear(NET_WIRE_CANDIDATE_ONLY,&r,&p.can)!=NET_WIRE_OK) puts("unexpected codec failure");
    return p;
}
static NetTxKey key(unsigned token) { NetTxKey k={9u,token}; return k; }
static NetStatus fake_rear(void *context,const NetUpperRear *r,const NetUpperDelivery *d)
{ FakeOwner *f=context; ++f->rear_calls; f->rear=*r; f->delivery=*d; return NET_OK; }
static NetStatus fake_uart(void *context,const NetUpperUartDelivery *u,const NetUpperDelivery *d)
{ FakeOwner *f=context; ++f->uart_calls; f->uart=*u; f->delivery=*d; return NET_OK; }
static int service(NetUpper *s,NetHalAdapter *adapter,uint32_t node)
{
    NetUpperConfig c={0}; size_t i;
    c.node=node; c.local_epoch=9u; c.profile=NET_WIRE_CANDIDATE_ONLY; c.port=net_hal_adapter_port(adapter);
    c.retain_failure_notices=true;
    c.owner.context=&owner; c.owner.rear=fake_rear; c.owner.uart=fake_uart;
    for(i=0u;i<3u;++i) c.port_epoch[i]=1u;
    for(i=0u;i<NET_UPPER_ROUTE_COUNT;++i) { c.rx_policy[i].bound=true; c.rx_policy[i].maximum_age_ms=1000u; c.rx_policy[i].expires_at_equality=true; }
    for(i=0u;i<2u;++i) { c.uart_policy[i].candidate_time_policy_selected=true; c.uart_policy[i].assembly_limit_ms=1000u; c.uart_policy[i].expires_at_equality=true; }
    CHECK(net_upper_init(s,&c)==NET_OK); return 0;
}
static int complete(NetHalAdapter *adapter,NetTxKey k,NetWireLink link)
{
    NetTxEvent e={0}; NetUpperCapture c=capture(1u,100u);
    e.key=k; e.admission_observed=true; e.accepted=true; e.outcome=NET_TX_SUCCEEDED; e.release_observed=true;
    CHECK(net_hal_capture_tx(adapter,link,&e,&c)==NET_OK); return 0;
}
static int rear_product(NetHalPlatform platform)
{
    NetWireCisRearRaw r=rear(); NetSourceStamp source={0}; NetAgePolicy policy={true,500u,true};
    NetTxKey k; NetUpperTxSnapshot state; NetUpperCapture c=capture(1u,103u);
    CHECK(init(&a,&fa,platform)==0); CHECK(init(&b,&fb,platform==NET_HAL_S32K144?NET_HAL_S32K344:NET_HAL_S32K144)==0);
    memset(&owner,0,sizeof(owner)); CHECK(service(&sender,&a,NET_WIRE_NODE_CIS)==0); CHECK(service(&receiver,&b,NET_WIRE_NODE_DOMAIN)==0);
    source.observation_id=77u; source.observed_at_ms=100u;
    CHECK(CisNet_PublishRear(&sender,&r,&source,&policy,&k)==NET_OK);
    fa.now=102u; CHECK(net_upper_step(&sender,32u)==NET_OK); CHECK(fa.count==1u);
    CHECK(fa.borrowed[0]->platform==platform && fa.borrowed[0]->packet.can.length==20u);
    CHECK(net_upper_tx_snapshot(&sender,k,&state)==NET_OK && !state.admission_known && state.outcome==NET_TX_UNKNOWN);
    CHECK(net_hal_capture_can(&b,&fa.borrowed[0]->packet.can,&c)==NET_OK);
    fb.now=104u; CHECK(net_upper_step(&receiver,32u)==NET_OK);
    CHECK(owner.rear_calls==1u && owner.rear.rear_distance_mm==123u && owner.rear.source_age_ms==3u);
    CHECK(owner.delivery.capture.captured_at_ms==103u && owner.delivery.local_elapsed_ms==1u);
    CHECK(complete(&a,k,NET_WIRE_CAN)==0); CHECK(net_upper_step(&sender,32u)==NET_OK);
    CHECK(net_upper_tx_snapshot(&sender,k,&state)==NET_OK && state.outcome==NET_TX_SUCCEEDED && state.reference_released);
    CHECK(net_upper_retire(&sender,k)==NET_OK && fa.bad==0u && fb.bad==0u);
    printf("{\"path\":\"CisNet_PublishRear->Queue->CANCodec->S32K%uAdapter->FakeHAL->S32K%uAdapter->RXCodec->TypedAdapter->FakeDomainIf\",\"mm\":123,\"age_ms\":3,\"hardware\":\"NOT_RUN\"}\n",(unsigned)platform,(unsigned)b.platform);
    return 0;
}
static int uart_product(void)
{
    NetUpperUartMessage m={0}; NetSourceStamp source={0}; NetAgePolicy policy={true,1000u,true};
    NetTxKey k; size_t i; NetWireUartFrame expected; const NetWireUartFrame *wire;
    CHECK(init(&a,&fa,NET_HAL_S32K344)==0 && init(&b,&fb,NET_HAL_S32K144)==0);
    memset(&owner,0,sizeof(owner)); CHECK(service(&sender,&a,NET_WIRE_NODE_PI)==0); CHECK(service(&receiver,&b,NET_WIRE_NODE_CIS)==0);
    m.link=NET_WIRE_UART_C; m.type=0x61u; m.sequence=8u;
    m.data.p_vision_status.pi_boot_id=51u; m.data.p_vision_status.status_sequence=7u;
    m.data.p_vision_status.vision_status=1u;
    source.observation_id=7u; source.observed_at_ms=100u;
    CHECK(net_upper_uart_encode(NET_WIRE_CANDIDATE_ONLY,&m,&expected)==NET_WIRE_OK);
    CHECK(net_upper_submit_uart(&sender,&m,&source,&policy,&k)==NET_OK);
    CHECK(net_upper_step(&sender,32u)==NET_OK && fa.count==1u);
    wire=&fa.borrowed[0]->packet.uart;
    CHECK(wire->length==expected.length && memcmp(wire->bytes,expected.bytes,wire->length)==0);
    for(i=0u;i<wire->length;++i) {
        NetUpperCapture c=capture(1u,101u+i); fb.now=101u+i;
        CHECK(net_hal_capture_uart(&b,NET_WIRE_UART_C,wire->bytes[i],&c)==NET_OK);
        { NetStatus status=net_upper_step(&receiver,32u); CHECK(status==(i+1u==wire->length?NET_OK:NET_NO_WORK)); }
    }
    CHECK(owner.uart_calls==1u && owner.uart.message.type==0x61u && owner.uart.message.sequence==8u);
    for(i=0u;i<wire->length;++i) CHECK(owner.uart.byte_at_ms[i]==101u+i);
    CHECK(owner.uart.message.data.p_vision_status.pi_boot_id==51u);
    CHECK(complete(&a,k,NET_WIRE_UART_C)==0 && net_upper_step(&sender,32u)==NET_OK);
    CHECK(net_upper_retire(&sender,k)==NET_OK);
    puts("{\"path\":\"ProductUARTSubmit->Codec->S32K344Adapter->FakeHALbytes->S32K144Adapter->ProductStream->TypedOwner\",\"frame\":\"P_VISION_STATUS\",\"hardware\":\"NOT_RUN\"}");
    return 0;
}
static int binding(void)
{
    NetHalConfig c; NetHalAdapter before;
    CHECK(init(&a,&fa,NET_HAL_S32K144)==0); c=config(&fa);
    memset(&b,0x6a,sizeof(b)); before=b; c.binding_selected=false;
    CHECK(net_hal_adapter_init(&b,&c,NET_HAL_S32K344)==NET_NOT_CONFIGURED && memcmp(&b,&before,sizeof(b))==0);
    c.binding_selected=true; c.ops=NULL;
    CHECK(net_hal_adapter_init(&b,&c,NET_HAL_S32K344)==NET_INVALID_ARGUMENT && memcmp(&b,&before,sizeof(b))==0);
    c=config(&fa); c.port_epoch[0]=0u; CHECK(net_hal_adapter_init(&b,&c,NET_HAL_S32K144)==NET_INVALID_ARGUMENT);
    c=config(&fa); c.enabled_links=8u; CHECK(net_hal_adapter_init(&b,&c,NET_HAL_S32K144)==NET_INVALID_ARGUMENT);
    c=config(&fa); memcpy(&b,&ops,sizeof(ops)); before=b; c.ops=(const NetHalBackendOps *)(const void *)&b;
    CHECK(net_hal_adapter_init(&b,&c,NET_HAL_S32K344)==NET_INVALID_ARGUMENT && memcmp(&b,&before,sizeof(b))==0);
    return 0;
}
static int can_metadata(void)
{
    NetUpperPacket p=packet(); NetUpperEvent e; NetUpperCapture c=capture(1u,77u); NetUpperPort port;
    CHECK(init(&a,&fa,NET_HAL_S32K144)==0); port=net_hal_adapter_port(&a);
    p.can.standard_id=0x123u; p.can.length=64u; p.can.dlc=15u;
    { size_t i; for(i=0u;i<64u;++i) p.can.bytes[i]=(uint8_t)(i^0xa5u); }
    c.errors=8u; c.overflow=3u;
    CHECK(net_hal_capture_can(&a,&p.can,&c)==NET_OK); memset(p.can.bytes,0,64u);
    CHECK(port.ops->poll(port.context,&e)==NET_OK);
    CHECK(e.can.standard_id==0x123u && e.can.dlc==15u && e.can.fdf && e.can.brs && e.can.length==64u);
    { size_t i; for(i=0u;i<64u;++i) CHECK(e.can.bytes[i]==(uint8_t)(i^0xa5u)); }
    CHECK(e.capture.captured_at_ms==77u && e.capture.errors==8u && e.capture.overflow==3u);
    p.can.length=63u; CHECK(net_hal_capture_can(&a,&p.can,&c)==NET_REJECTED);
    p.can.length=64u; p.can.dlc=16u; CHECK(net_hal_capture_can(&a,&p.can,&c)==NET_REJECTED);
    CHECK(port.ops->poll(port.context,&e)==NET_NO_WORK);
    return 0;
}
static int uart_bytes(void)
{
    NetUpperPort p; NetUpperEvent e; unsigned i;
    CHECK(init(&a,&fa,NET_HAL_S32K344)==0); p=net_hal_adapter_port(&a);
    for(i=0u;i<16u;++i) { NetUpperCapture c=capture(1u,100u+i); CHECK(net_hal_capture_uart(&a,NET_WIRE_UART_M,(uint8_t)i,&c)==NET_OK); }
    for(i=0u;i<16u;++i) { CHECK(p.ops->poll(p.context,&e)==NET_OK); CHECK(e.byte==i && e.capture.captured_at_ms==100u+i && e.link==NET_WIRE_UART_M); }
    return 0;
}
static int borrow_copy(void)
{
    NetUpperPacket p=packet(),expected=p; NetUpperPort port;
    CHECK(init(&a,&fa,NET_HAL_S32K144)==0); port=net_hal_adapter_port(&a);
    CHECK(port.ops->submit(port.context,&p,key(1u))==NET_OK); memset(&p,0xff,sizeof(p));
    CHECK(fa.borrowed[0]->packet.can.length==expected.can.length && memcmp(fa.borrowed[0]->packet.can.bytes,expected.can.bytes,expected.can.length)==0);
    CHECK(fa.bad==0u && fa.depth==0u && fa.enters==fa.leaves); return 0;
}
static int tx_order(void)
{
    static const unsigned permutations[6][3]={{0,1,2},{0,2,1},{1,0,2},{1,2,0},{2,0,1},{2,1,0}};
    unsigned n,j; NetUpperPacket p=packet();
    for(n=0u;n<6u;++n) {
        NetUpperPort port; NetUpperEvent out; bool admitted=false,released=false,final=false;
        CHECK(init(&a,&fa,NET_HAL_S32K144)==0); port=net_hal_adapter_port(&a);
        CHECK(port.ops->submit(port.context,&p,key(1u))==NET_OK);
        for(j=0u;j<3u;++j) {
            NetTxEvent e={0}; NetUpperCapture c=capture(1u,100u+j); e.key=key(1u);
            if(permutations[n][j]==0u) { e.admission_observed=true; e.accepted=true; }
            if(permutations[n][j]==1u) e.outcome=NET_TX_SUCCEEDED;
            if(permutations[n][j]==2u) e.release_observed=true;
            CHECK(net_hal_capture_tx(&a,NET_WIRE_CAN,&e,&c)==NET_OK);
            CHECK(port.ops->poll(port.context,&out)==NET_OK && out.kind==NET_UPPER_TX_EVENT);
            admitted|=out.tx.admission_observed; released|=out.tx.release_observed; final|=out.tx.outcome==NET_TX_SUCCEEDED;
            CHECK(j==2u || a.tx[0].active); CHECK(out.capture.captured_at_ms==100u+j);
        }
        CHECK(admitted && released && final && !a.tx[0].active);
        CHECK(port.ops->poll(port.context,&out)==NET_NO_WORK);
    }
    return 0;
}
static int submit_reject(void)
{
    NetUpperPacket p=packet(); NetUpperPort port; NetUpperEvent out;
    CHECK(init(&a,&fa,NET_HAL_S32K144)==0); port=net_hal_adapter_port(&a); fa.submit_status=NET_BUSY;
    CHECK(port.ops->submit(port.context,&p,key(1u))==NET_BUSY && !a.tx[0].active);
    CHECK(port.ops->poll(port.context,&out)==NET_NO_WORK);
    fa.submit_status=NET_OK; CHECK(port.ops->submit(port.context,&p,key(2u))==NET_OK);
    return 0;
}
static int stale_key(void)
{
    NetUpperPacket p=packet(); NetUpperPort port; NetTxEvent e={0}; NetUpperCapture c=capture(1u,100u); NetUpperEvent out;
    CHECK(init(&a,&fa,NET_HAL_S32K344)==0); port=net_hal_adapter_port(&a);
    CHECK(port.ops->submit(port.context,&p,key(1u))==NET_OK); e.key=key(2u); e.release_observed=true;
    CHECK(net_hal_capture_tx(&a,NET_WIRE_CAN,&e,&c)==NET_STALE); e.key=key(1u);
    CHECK(net_hal_capture_tx(&a,NET_WIRE_UART_M,&e,&c)==NET_STALE); c.port_epoch=2u;
    CHECK(net_hal_capture_tx(&a,NET_WIRE_CAN,&e,&c)==NET_STALE);
    CHECK(complete(&a,key(1u),NET_WIRE_CAN)==0);
    CHECK(port.ops->poll(port.context,&out)==NET_OK && out.tx.admission_observed);
    CHECK(port.ops->poll(port.context,&out)==NET_OK && out.tx.outcome==NET_TX_SUCCEEDED);
    CHECK(port.ops->poll(port.context,&out)==NET_OK && out.tx.release_observed);
    CHECK(port.ops->submit(port.context,&p,key(2u))==NET_OK); c.port_epoch=1u;
    CHECK(net_hal_capture_tx(&a,NET_WIRE_CAN,&e,&c)==NET_STALE && !a.tx[0].released);
    return 0;
}
static int tx_conflict(void)
{
    NetUpperPacket p=packet(); NetUpperPort port; NetTxEvent e={0}; NetUpperCapture c=capture(1u,100u); NetUpperEvent out;
    CHECK(init(&a,&fa,NET_HAL_S32K344)==0); port=net_hal_adapter_port(&a);
    CHECK(port.ops->submit(port.context,&p,key(1u))==NET_OK);
    e.key=key(1u); e.admission_observed=true; e.accepted=true;
    CHECK(net_hal_capture_tx(&a,NET_WIRE_CAN,&e,&c)==NET_OK);
    CHECK(net_hal_capture_tx(&a,NET_WIRE_CAN,&e,&c)==NET_DUPLICATE);
    e.accepted=false; CHECK(net_hal_capture_tx(&a,NET_WIRE_CAN,&e,&c)==NET_HOLD);
    CHECK(port.ops->poll(port.context,&out)==NET_OK && out.tx.accepted);
    CHECK(port.ops->poll(port.context,&out)==NET_OK && !out.tx.accepted);
    e.admission_observed=false; e.release_observed=true; CHECK(net_hal_capture_tx(&a,NET_WIRE_CAN,&e,&c)==NET_OK);
    CHECK(port.ops->poll(port.context,&out)==NET_OK && a.tx[0].held && a.tx[0].active);
    return 0;
}
static int rx_pressure(void)
{
    unsigned i; NetUpperPort port; NetUpperCapture c=capture(1u,100u); NetUpperEvent out;
    CHECK(init(&a,&fa,NET_HAL_S32K144)==0); port=net_hal_adapter_port(&a);
    for(i=0u;i<16u;++i) CHECK(net_hal_capture_uart(&a,NET_WIRE_UART_C,(uint8_t)i,&c)==NET_OK);
    CHECK(net_hal_capture_uart(&a,NET_WIRE_UART_C,99u,&c)==NET_NO_CAPACITY);
    for(i=0u;i<16u;++i) { CHECK(port.ops->poll(port.context,&out)==NET_OK); CHECK(out.kind==NET_UPPER_UART_BYTE && out.byte==i); }
    CHECK(port.ops->poll(port.context,&out)==NET_OK && out.kind==NET_UPPER_PORT_ERROR && out.capture.overflow==1u);
    CHECK(a.observation.rx_dropped==1u && a.faulted[NET_WIRE_UART_C]); return 0;
}
static int tx_pressure(void)
{
    unsigned i; NetUpperPacket p=packet(); NetUpperPort port;
    CHECK(init(&a,&fa,NET_HAL_S32K144)==0); port=net_hal_adapter_port(&a);
    for(i=0u;i<NET_HAL_TX_CAPACITY;++i) CHECK(port.ops->submit(port.context,&p,key(i+1u))==NET_OK);
    CHECK(port.ops->submit(port.context,&p,key(99u))==NET_NO_CAPACITY && fa.count==NET_HAL_TX_CAPACITY);
    for(i=0u;i<NET_HAL_TX_CAPACITY;++i) CHECK(fa.borrowed[i]->key.token==i+1u && fa.borrowed[i]->packet.can.bytes[14]==123u);
    return 0;
}
static int reset_borrow(void)
{
    NetUpperPacket p=packet(); NetUpperPort port; NetUpperCapture c=capture(2u,101u); NetUpperEvent out;
    CHECK(init(&a,&fa,NET_HAL_S32K344)==0); port=net_hal_adapter_port(&a);
    CHECK(port.ops->submit(port.context,&p,key(1u))==NET_OK);
    CHECK(net_hal_capture_link(&a,NET_UPPER_LINK_RESET,NET_WIRE_CAN,&c)==NET_OK);
    CHECK(a.tx[0].active && a.tx[0].held && !a.tx[0].released);
    CHECK(port.ops->submit(port.context,&p,key(2u))==NET_HOLD);
    CHECK(port.ops->poll(port.context,&out)==NET_OK && out.kind==NET_UPPER_LINK_RESET && out.capture.port_epoch==2u);
    CHECK(port.ops->submit(port.context,&p,key(2u))==NET_OK && fa.borrowed[1]->port_epoch==2u);
    CHECK(net_hal_capture_link(&a,NET_UPPER_LINK_RESET,NET_WIRE_CAN,&c)==NET_STALE);
    CHECK(fa.borrowed[0]->packet.can.bytes[14]==123u && fa.borrowed[0]->port_epoch==1u); return 0;
}
static int clock_test(void)
{
    NetUpperPort port; uint64_t out=999u;
    CHECK(init(&a,&fa,NET_HAL_S32K144)==0); port=net_hal_adapter_port(&a);
    fa.now_status=NET_HOLD; CHECK(port.ops->now_ms(port.context,&out)==NET_HOLD && out==999u);
    fa.now_status=NET_OK; CHECK(port.ops->now_ms(port.context,&out)==NET_OK && out==100u);
    fa.now=99u; CHECK(port.ops->now_ms(port.context,&out)==NET_HOLD && out==100u);
    fa.now=101u; CHECK(port.ops->now_ms(port.context,&out)==NET_OK && out==101u);
    CHECK(fa.bad==0u && fa.enters==fa.leaves); return 0;
}
static int callback_order(void)
{
    NetUpperPacket p=packet(); NetUpperPort port; NetUpperEvent out;
    CHECK(init(&a,&fa,NET_HAL_S32K344)==0); port=net_hal_adapter_port(&a); fa.synchronous=2u; fa.reenter=1u;
    CHECK(port.ops->submit(port.context,&p,key(1u))==NET_OK && fa.capture_status==NET_OK && fa.reentry_status==NET_BUSY);
    CHECK(port.ops->poll(port.context,&out)==NET_OK && out.tx.admission_observed && out.tx.outcome==NET_TX_UNKNOWN && !out.tx.release_observed);
    CHECK(port.ops->poll(port.context,&out)==NET_OK && out.tx.outcome==NET_TX_SUCCEEDED && !out.tx.release_observed);
    CHECK(port.ops->poll(port.context,&out)==NET_OK && out.tx.release_observed && !out.tx.admission_observed);
    CHECK(!a.tx[0].active && fa.bad==0u && fa.enters==fa.leaves); return 0;
}
static int backend_violation(void)
{
    NetUpperPacket p=packet(); NetUpperPort port; NetUpperEvent out;
    NetWireCisRearRaw r=rear(); NetSourceStamp source={0}; NetAgePolicy policy={true,500u,true};
    NetTxKey upper_key; NetUpperTxSnapshot state;
    CHECK(init(&a,&fa,NET_HAL_S32K144)==0); port=net_hal_adapter_port(&a); fa.synchronous=1u; fa.submit_status=NET_REJECTED;
    CHECK(port.ops->submit(port.context,&p,key(1u))==NET_OK && a.tx[0].active && a.tx[0].held);
    CHECK(a.observation.backend_violations==1u && a.faulted[NET_WIRE_CAN]);
    CHECK(port.ops->poll(port.context,&out)==NET_OK && out.kind==NET_UPPER_TX_EVENT && out.tx.admission_observed && out.tx.outcome==NET_TX_UNKNOWN && !out.tx.release_observed);
    CHECK(port.ops->poll(port.context,&out)==NET_OK && out.kind==NET_UPPER_PORT_ERROR);
    CHECK(fa.borrowed[0]->packet.can.bytes[14]==123u);
    /* Separate fresh bridge: actual final+release may precede a contradictory
     * backend return. The upper copy and backend-owned copy have distinct lives.
     * Preserve facts, retire only the released upper copy, pin the HAL copy. */
    CHECK(init(&b,&fb,NET_HAL_S32K344)==0 && service(&sender,&b,NET_WIRE_NODE_CIS)==0);
    fb.synchronous=2u; fb.submit_status=NET_REJECTED;
    source.observation_id=77u; source.observed_at_ms=100u;
    CHECK(CisNet_PublishRear(&sender,&r,&source,&policy,&upper_key)==NET_OK);
    r.rear_distance_deci_cm=321u;
    CHECK(net_upper_step(&sender,32u)==NET_OK && fb.count==1u && fb.capture_status==NET_OK);
    CHECK(net_upper_step(&sender,32u)==NET_HOLD);
    CHECK(net_upper_tx_snapshot(&sender,upper_key,&state)==NET_OK && !state.conflict && state.accepted && state.outcome==NET_TX_SUCCEEDED && state.reference_released);
    CHECK(net_upper_retire(&sender,upper_key)==NET_OK && sender.link_failed[NET_WIRE_CAN]);
    CHECK(b.tx[0].active && b.tx[0].held && b.tx[0].released && b.tx[0].outcome==NET_TX_SUCCEEDED && b.faulted[NET_WIRE_CAN]);
    CHECK(b.observation.backend_violations==1u && fb.borrowed[0]->packet.can.bytes[14]==123u);
    CHECK(fb.borrowed[0]->key.token==upper_key.token && fb.bad==0u);
    puts("{\"backend_violation\":\"all synchronous TX facts then non-OK return\",\"upper_released_copy\":\"retired\",\"HAL_independent_copy\":\"pinned unchanged\",\"binding_contract\":\"HOLD\"}");
    return 0;
}
static int tx_rx_pressure(void)
{
    NetUpperPacket p=packet(); NetUpperPort port; NetUpperEvent out; NetUpperCapture c=capture(1u,100u); unsigned i;
    CHECK(init(&a,&fa,NET_HAL_S32K344)==0); port=net_hal_adapter_port(&a);
    CHECK(port.ops->submit(port.context,&p,key(1u))==NET_OK);
    for(i=0u;i<16u;++i) CHECK(net_hal_capture_uart(&a,NET_WIRE_UART_M,(uint8_t)i,&c)==NET_OK);
    CHECK(complete(&a,key(1u),NET_WIRE_CAN)==0);
    for(i=0u;i<16u;++i) { CHECK(port.ops->poll(port.context,&out)==NET_OK); CHECK(out.kind==NET_UPPER_UART_BYTE && out.byte==i); }
    CHECK(port.ops->poll(port.context,&out)==NET_OK && out.kind==NET_UPPER_TX_EVENT && out.tx.admission_observed);
    CHECK(port.ops->poll(port.context,&out)==NET_OK && out.tx.outcome==NET_TX_SUCCEEDED);
    CHECK(port.ops->poll(port.context,&out)==NET_OK && out.tx.release_observed && !a.tx[0].active);
    return 0;
}
static int loss_error(void)
{
    NetUpperPort port; NetUpperEvent out; NetUpperCapture c=capture(1u,100u); NetUpperPacket p=packet();
    CHECK(init(&a,&fa,NET_HAL_S32K344)==0); port=net_hal_adapter_port(&a); c.errors=0x80u;
    CHECK(net_hal_capture_link(&a,NET_UPPER_PORT_ERROR,NET_WIRE_CAN,&c)==NET_OK);
    CHECK(port.ops->poll(port.context,&out)==NET_OK && out.capture.errors==0x80u && out.capture.captured_at_ms==100u);
    CHECK(port.ops->submit(port.context,&p,key(1u))==NET_HOLD && fa.count==0u);
    c.errors=0u; CHECK(net_hal_capture_link(&a,NET_UPPER_LINK_LOSS,NET_WIRE_UART_C,&c)==NET_OK);
    CHECK(port.ops->poll(port.context,&out)==NET_OK && out.kind==NET_UPPER_LINK_LOSS);
    return 0;
}
static int footprint(void)
{
    CHECK(NET_HAL_TX_CAPACITY==4u && NET_HAL_RX_CAPACITY==16u);
    CHECK(_Alignof(NetHalTransfer)>=4u);
    printf("{\"host_sizeof_adapter\":%lu,\"host_sizeof_attempt\":%lu,\"host_sizeof_transfer\":%lu,\"target_fit\":\"HOLD\"}\n",(unsigned long)sizeof(NetHalAdapter),(unsigned long)sizeof(NetHalAttempt),(unsigned long)sizeof(NetHalTransfer));
    return 0;
}
static int reset_causal(bool settled_first)
{
    NetWireCisRearRaw r=rear(); NetSourceStamp source={0}; NetAgePolicy policy={true,500u,true};
    NetTxKey k; NetUpperTxSnapshot state; NetUpperCapture reset=capture(2u,100u);
    CHECK(init(&a,&fa,NET_HAL_S32K144)==0 && service(&sender,&a,NET_WIRE_NODE_CIS)==0);
    source.observation_id=77u; source.observed_at_ms=100u;
    CHECK(CisNet_PublishRear(&sender,&r,&source,&policy,&k)==NET_OK);
    CHECK(net_upper_step(&sender,32u)==NET_OK && fa.count==1u);
    if(settled_first) CHECK(complete(&a,k,NET_WIRE_CAN)==0);
    CHECK(net_hal_capture_link(&a,NET_UPPER_LINK_RESET,NET_WIRE_CAN,&reset)==NET_OK);
    if(!settled_first) CHECK(complete(&a,k,NET_WIRE_CAN)==0);
    CHECK(net_upper_step(&sender,32u)==(settled_first?NET_OK:NET_HOLD));
    CHECK(net_upper_tx_snapshot(&sender,k,&state)==NET_OK);
    if(settled_first) {
        CHECK(!state.conflict && state.reference_released && state.outcome==NET_TX_SUCCEEDED);
        CHECK(!a.tx[0].active && !a.tx[0].held && net_upper_retire(&sender,k)==NET_OK);
    } else {
        CHECK(state.conflict && a.tx[0].active && a.tx[0].held);
        CHECK(net_upper_retire(&sender,k)==NET_HOLD);
    }
    printf("{\"counterexample\":\"%s\",\"upper_conflict\":%u,\"adapter_held\":%u,\"result\":\"PASS\"}\n",settled_first?"settled_then_reset":"reset_then_facts",state.conflict,a.tx[0].held);
    return 0;
}
static int error_reset_error(void)
{
    NetUpperCapture c=capture(1u,100u); NetUpperPort port; NetUpperPacket p=packet();
    CHECK(init(&a,&fa,NET_HAL_S32K344)==0 && service(&sender,&a,NET_WIRE_NODE_CIS)==0);
    c.errors=1u; CHECK(net_hal_capture_link(&a,NET_UPPER_PORT_ERROR,NET_WIRE_CAN,&c)==NET_OK);
    c.errors=0u; c.port_epoch=2u; CHECK(net_hal_capture_link(&a,NET_UPPER_LINK_RESET,NET_WIRE_CAN,&c)==NET_OK);
    c.errors=2u; CHECK(net_hal_capture_link(&a,NET_UPPER_PORT_ERROR,NET_WIRE_CAN,&c)==NET_OK);
    CHECK(net_upper_step(&sender,32u)==NET_HOLD);
    CHECK(a.faulted[NET_WIRE_CAN] && sender.link_failed[NET_WIRE_CAN] && sender.config.port_epoch[NET_WIRE_CAN]==2u);
    port=net_hal_adapter_port(&a); CHECK(port.ops->submit(port.context,&p,key(1u))==NET_HOLD);
    return 0;
}
static int reset_pressure(void)
{
    NetUpperCapture c=capture(1u,100u); NetUpperPort port; NetUpperEvent e; unsigned i;
    CHECK(init(&a,&fa,NET_HAL_S32K144)==0); port=net_hal_adapter_port(&a);
    for(i=0u;i<15u;++i) CHECK(net_hal_capture_uart(&a,NET_WIRE_UART_C,(uint8_t)i,&c)==NET_OK);
    c.errors=1u; CHECK(net_hal_capture_link(&a,NET_UPPER_PORT_ERROR,NET_WIRE_CAN,&c)==NET_OK);
    c.errors=0u; c.port_epoch=2u;
    CHECK(net_hal_capture_link(&a,NET_UPPER_LINK_RESET,NET_WIRE_CAN,&c)==NET_NO_CAPACITY);
    CHECK(a.config.port_epoch[NET_WIRE_CAN]==1u && a.faulted[NET_WIRE_CAN] && a.error_pending[NET_WIRE_CAN]);
    for(i=0u;i<15u;++i) { CHECK(port.ops->poll(port.context,&e)==NET_OK); CHECK(e.kind==NET_UPPER_UART_BYTE && e.byte==i); }
    CHECK(port.ops->poll(port.context,&e)==NET_OK && e.kind==NET_UPPER_PORT_ERROR && e.capture.port_epoch==1u);
    CHECK(port.ops->poll(port.context,&e)==NET_NO_WORK);
    CHECK(net_hal_capture_link(&a,NET_UPPER_LINK_RESET,NET_WIRE_CAN,&c)==NET_OK);
    CHECK(port.ops->poll(port.context,&e)==NET_OK && e.kind==NET_UPPER_LINK_RESET && !a.faulted[NET_WIRE_CAN]);
    return 0;
}
static int order_exhaustion(void)
{
    NetUpperPacket p=packet(); NetUpperPort port; NetUpperCapture c=capture(1u,100u); NetTxEvent tx={0};
    CHECK(init(&a,&fa,NET_HAL_S32K344)==0); port=net_hal_adapter_port(&a);
    CHECK(port.ops->submit(port.context,&p,key(1u))==NET_OK);
    a.next_order=UINT64_MAX-3u;
    CHECK(net_hal_capture_uart(&a,NET_WIRE_UART_C,0xa5u,&c)==NET_HOLD && a.rx_count==0u);
    tx.key=key(1u); tx.release_observed=true;
    CHECK(net_hal_capture_tx(&a,NET_WIRE_CAN,&tx,&c)==NET_HOLD && a.tx[0].held && !a.tx[0].released);
    CHECK(port.ops->submit(port.context,&p,key(2u))==NET_HOLD && a.next_order==UINT64_MAX-3u);
    return 0;
}
static int tx_metadata_fault(void)
{
    NetUpperPacket p=packet(); NetUpperPort port; NetUpperEvent out;
    NetTxEvent tx={0}; NetUpperCapture c=capture(1u,100u);
    CHECK(init(&a,&fa,NET_HAL_S32K144)==0); port=net_hal_adapter_port(&a);
    CHECK(port.ops->submit(port.context,&p,key(1u))==NET_OK);
    tx.key=key(1u); tx.admission_observed=true; tx.accepted=true; c.errors=4u;
    CHECK(net_hal_capture_tx(&a,NET_WIRE_CAN,&tx,&c)==NET_HOLD && a.tx[0].held);
    CHECK(port.ops->poll(port.context,&out)==NET_OK && out.kind==NET_UPPER_PORT_ERROR && out.capture.errors==4u);
    CHECK(port.ops->poll(port.context,&out)==NET_OK && out.kind==NET_UPPER_TX_EVENT && out.tx.outcome==NET_TX_UNKNOWN && !out.tx.release_observed);
    CHECK(a.tx[0].active && !a.tx[0].released && a.faulted[NET_WIRE_CAN]); return 0;
}
static int tx_capture_error(void)
{
    NetWireCisRearRaw r=rear(); NetSourceStamp source={0}; NetAgePolicy policy={true,500u,true};
    NetTxKey first,second; NetUpperTxSnapshot old,current; NetUpperFailureNotice notice;
    NetTxEvent event={0}; NetUpperCapture c=capture(1u,100u);
    CHECK(init(&a,&fa,NET_HAL_S32K144)==0 && service(&sender,&a,NET_WIRE_NODE_CIS)==0);
    source.observation_id=77u; source.observed_at_ms=100u;
    CHECK(CisNet_PublishRear(&sender,&r,&source,&policy,&first)==NET_OK);
    CHECK(net_upper_step(&sender,32u)==NET_OK && complete(&a,first,NET_WIRE_CAN)==0);
    CHECK(net_upper_step(&sender,32u)==NET_OK);
    ++r.tx_sequence;
    CHECK(CisNet_PublishRear(&sender,&r,&source,&policy,&second)==NET_OK);
    CHECK(net_upper_step(&sender,32u)==NET_OK);
    event.key=second; event.admission_observed=true; event.accepted=true;
    event.outcome=NET_TX_SUCCEEDED; event.release_observed=true; c.errors=4u; c.overflow=2u;
    CHECK(net_hal_capture_tx(&a,NET_WIRE_CAN,&event,&c)==NET_HOLD);
    CHECK(net_upper_step(&sender,32u)==NET_HOLD);
    CHECK(net_upper_tx_snapshot(&sender,first,&old)==NET_OK && !old.conflict && old.outcome==NET_TX_SUCCEEDED && old.reference_released);
    CHECK(net_upper_retire(&sender,first)==NET_OK);
    CHECK(net_upper_tx_snapshot(&sender,second,&current)==NET_OK && current.conflict && current.outcome==NET_TX_UNKNOWN && !current.reference_released);
    CHECK(net_upper_failure_poll(&sender,&notice)==NET_OK && notice.detail.state.key.token==second.token && notice.version!=0u);
    CHECK(net_upper_failure_ack(&sender,second,notice.version)==NET_OK && net_upper_retire(&sender,second)==NET_HOLD);
    CHECK(a.tx[0].active && a.tx[0].held && a.tx[0].outcome==NET_TX_SUCCEEDED && a.tx[0].released);
    CHECK(a.tx[0].fact_capture[1].errors==4u && a.tx[0].fact_capture[2].overflow==2u);
    puts("{\"counterexample\":\"TX capture error after settled older attempt\",\"old_attempt\":\"preserved\",\"uncertain_attempt\":\"HOLD\",\"physical_facts\":\"retained without synthesizing new result\"}");
    return 0;
}
int main(int argc,char **argv)
{
    int result=1;
    if(argc!=2) return 2;
    name=argv[1];
    if(strcmp(name,"binding")==0) result=binding();
    else if(strcmp(name,"s32k144_rear")==0) result=rear_product(NET_HAL_S32K144);
    else if(strcmp(name,"s32k344_rear")==0) result=rear_product(NET_HAL_S32K344);
    else if(strcmp(name,"uart_product")==0) result=uart_product();
    else if(strcmp(name,"uart_bytes")==0) result=uart_bytes();
    else if(strcmp(name,"can_metadata")==0) result=can_metadata();
    else if(strcmp(name,"borrow_copy")==0) result=borrow_copy();
    else if(strcmp(name,"tx_order")==0) result=tx_order();
    else if(strcmp(name,"submit_reject")==0) result=submit_reject();
    else if(strcmp(name,"stale_key")==0) result=stale_key();
    else if(strcmp(name,"tx_conflict")==0) result=tx_conflict();
    else if(strcmp(name,"rx_pressure")==0) result=rx_pressure();
    else if(strcmp(name,"tx_pressure")==0) result=tx_pressure();
    else if(strcmp(name,"reset_borrow")==0) result=reset_borrow();
    else if(strcmp(name,"clock")==0) result=clock_test();
    else if(strcmp(name,"callback_order")==0) result=callback_order();
    else if(strcmp(name,"backend_violation")==0) result=backend_violation();
    else if(strcmp(name,"tx_rx_pressure")==0) result=tx_rx_pressure();
    else if(strcmp(name,"loss_error")==0) result=loss_error();
    else if(strcmp(name,"footprint")==0) result=footprint();
    else if(strcmp(name,"reset_then_facts")==0) result=reset_causal(false);
    else if(strcmp(name,"settled_then_reset")==0) result=reset_causal(true);
    else if(strcmp(name,"error_reset_error")==0) result=error_reset_error();
    else if(strcmp(name,"reset_pressure")==0) result=reset_pressure();
    else if(strcmp(name,"order_exhaustion")==0) result=order_exhaustion();
    else if(strcmp(name,"tx_metadata_fault")==0) result=tx_metadata_fault();
    else if(strcmp(name,"tx_capture_error")==0) result=tx_capture_error();
    else return 2;
    if(result==0) printf("{\"case\":\"%s\",\"result\":\"PASS\",\"scope\":\"HOST_ONLY\"}\n",name);
    return result;
}
