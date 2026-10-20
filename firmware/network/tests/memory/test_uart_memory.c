#include "network/net_upper_query.h"
#include "network/net_upper_control.h"
#include <stdio.h>
#include <string.h>
#define CHECK(x) do { if(!(x)) { printf("{\"case\":\"%s\",\"line\":%d,\"assertion\":\"%s\",\"result\":\"FAIL\"}\n",name,__LINE__,#x); return 1; } } while(0)
static const char *name;
static NetWireUartStream stream,stream_before;
static NetUpperUartReceiver receiver,receiver_before,other;
static NetUpperUartDelivery delivery,out_before,context_delivery,query_delivery;
static NetUpperQuery query,query_before;
static NetUpper service,service_before;
static NetWireStreamPolicy policy(void)
{ NetWireStreamPolicy p={true,1000u,true}; return p; }
static NetWireUartFrame frame(void)
{
    NetWirePVisionStatusRaw raw={0}; NetWireUartFrame f={0};
    raw.pi_boot_id=0xa55a0029u; raw.status_sequence=7u; raw.vision_status=1u;
    (void)net_wire_p_vision_status_encode(NET_WIRE_CANDIDATE_ONLY,4u,&raw,&f);
    return f;
}
static int init_stream(void)
{
    NetWireStreamPolicy p=policy();
    CHECK(net_wire_uart_stream_init(&stream,NET_WIRE_CANDIDATE_ONLY,NET_WIRE_UART_C,NET_WIRE_UPLINK,&p)==NET_WIRE_OK); return 0;
}
static int init_receiver(NetUpperUartReceiver *r)
{
    NetWireStreamPolicy p=policy();
    CHECK(net_upper_uart_receiver_init(r,NET_WIRE_CANDIDATE_ONLY,NET_WIRE_UART_C,NET_WIRE_UPLINK,&p,1u)==NET_WIRE_OK); return 0;
}
static int stream_init_failure(void)
{
    NetWireStreamPolicy p=policy();
    memset(&stream,0xa6,sizeof(stream)); stream_before=stream;
    CHECK(net_wire_uart_stream_init(&stream,NET_WIRE_NOT_BOUND,NET_WIRE_UART_C,NET_WIRE_UPLINK,&p)==NET_WIRE_NOT_BOUND_STATUS);
    CHECK(memcmp(&stream,&stream_before,sizeof(stream))==0);
    p.assembly_limit_ms=0u;
    CHECK(net_wire_uart_stream_init(&stream,NET_WIRE_CANDIDATE_ONLY,NET_WIRE_UART_C,NET_WIRE_UPLINK,&p)==NET_WIRE_INVALID_ARGUMENT);
    CHECK(memcmp(&stream,&stream_before,sizeof(stream))==0);
    CHECK(net_wire_uart_stream_init(&stream,NET_WIRE_CANDIDATE_ONLY,NET_WIRE_UART_C,NET_WIRE_UPLINK,&stream.policy)==NET_WIRE_INVALID_ARGUMENT);
    CHECK(memcmp(&stream,&stream_before,sizeof(stream))==0); return 0;
}
static int stream_input_atomic(void)
{
    NetWireUartFrame f=frame(),out,before; uint64_t times[NET_WIRE_UART_FRAME_MAX]; size_t i,used=777u;
    NetWireStreamInput input={0}; CHECK(init_stream()==0);
    for(i=0u;i<f.length;++i) times[i]=100u+i;
    times[f.length-1u]=99u; input.bytes=f.bytes; input.captured_at_ms=times; input.length=f.length;
    input.timestamps_unambiguous=true; input.timer_check_permitted=true; input.now_ms=500u;
    memset(&out,0x5a,sizeof(out)); before=out; stream_before=stream;
    CHECK(net_wire_uart_stream_next(&stream,&input,&used,&out)==NET_WIRE_TIME_HOLD);
    CHECK(used==777u && memcmp(&out,&before,sizeof(out))==0 && memcmp(&stream,&stream_before,sizeof(stream))==0);
    times[f.length-1u]=100u+f.length-1u; input.now_ms=99u;
    CHECK(net_wire_uart_stream_next(&stream,&input,&used,&out)==NET_WIRE_TIME_HOLD);
    CHECK(used==777u && memcmp(&out,&before,sizeof(out))==0 && memcmp(&stream,&stream_before,sizeof(stream))==0); return 0;
}
static int stream_retained_atomic(void)
{
    NetWireUartFrame out,before; NetWireStreamInput input={0}; size_t used=888u;
    CHECK(init_stream()==0); stream.count=4u; stream.bytes[0]=0u; stream.bytes[1]=0xa5u; stream.bytes[2]=0x5au; stream.bytes[3]=1u;
    stream.captured_at_ms[0]=10u; stream.captured_at_ms[1]=20u; stream.captured_at_ms[2]=5u; stream.captured_at_ms[3]=30u;
    stream.has_last_capture=true; stream.last_capture_ms=30u; stream_before=stream;
    memset(&out,0x5a,sizeof(out)); before=out; input.timestamps_unambiguous=true; input.timer_check_permitted=true; input.now_ms=100u;
    CHECK(net_wire_uart_stream_next(&stream,&input,&used,&out)==NET_WIRE_TIME_HOLD);
    CHECK(used==888u && memcmp(&stream,&stream_before,sizeof(stream))==0 && memcmp(&out,&before,sizeof(out))==0); return 0;
}
static int stream_split(void)
{
    NetWireUartFrame f=frame(),out; NetWireStreamInput input={0}; uint64_t times[NET_WIRE_UART_FRAME_MAX]; size_t i,used;
    CHECK(init_stream()==0); for(i=0u;i<f.length;++i) times[i]=100u+i;
    input.bytes=f.bytes; input.captured_at_ms=times; input.length=1u; input.timestamps_unambiguous=true;
    CHECK(net_wire_uart_stream_next(&stream,&input,&used,&out)==NET_WIRE_NEED_MORE && used==1u);
    input.bytes=f.bytes+1u; input.captured_at_ms=times+1u; input.length=f.length-1u;
    CHECK(net_wire_uart_stream_next(&stream,&input,&used,&out)==NET_WIRE_OK && used==f.length-1u);
    CHECK(out.length==f.length && memcmp(out.bytes,f.bytes,f.length)==0 && out.captured_at_ms==times[f.length-1u]); return 0;
}
static int stream_crc_suffix(void)
{
    NetWireUartFrame f=frame(),out; NetWireStreamInput input={0}; uint8_t bytes[2u*NET_WIRE_UART_FRAME_MAX+2u];
    uint64_t times[2u*NET_WIRE_UART_FRAME_MAX+2u]; size_t i,used,n=2u+2u*f.length;
    CHECK(init_stream()==0); bytes[0]=0xa5u; bytes[1]=0u;
    memcpy(bytes+2u,f.bytes,f.length); bytes[2u+f.length-1u]^=1u;
    memcpy(bytes+2u+f.length,f.bytes,f.length); for(i=0u;i<n;++i) times[i]=100u+i;
    input.bytes=bytes; input.captured_at_ms=times; input.length=n; input.timestamps_unambiguous=true;
    CHECK(net_wire_uart_stream_next(&stream,&input,&used,&out)==NET_WIRE_OK && used==n);
    CHECK(stream.bad_crc==1u && stream.discarded_bytes>=2u+f.length && out.captured_at_ms==times[n-1u]);
    CHECK(memcmp(out.bytes,f.bytes,f.length)==0); return 0;
}
static int receiver_init_failure(void)
{
    NetWireStreamPolicy p=policy(); memset(&receiver,0xa6,sizeof(receiver)); receiver_before=receiver;
    p.candidate_time_policy_selected=false;
    CHECK(net_upper_uart_receiver_init(&receiver,NET_WIRE_CANDIDATE_ONLY,NET_WIRE_UART_C,NET_WIRE_UPLINK,&p,1u)==NET_WIRE_CONTRACT_HOLD);
    CHECK(memcmp(&receiver,&receiver_before,sizeof(receiver))==0);
    p=policy(); CHECK(net_upper_uart_receiver_init(&receiver,NET_WIRE_CANDIDATE_ONLY,NET_WIRE_UART_C,NET_WIRE_UPLINK,&p,0u)==NET_WIRE_INVALID_ARGUMENT);
    CHECK(memcmp(&receiver,&receiver_before,sizeof(receiver))==0);
    CHECK(net_upper_uart_receiver_init(&receiver,NET_WIRE_CANDIDATE_ONLY,NET_WIRE_UART_C,NET_WIRE_UPLINK,&receiver.stream.policy,1u)==NET_WIRE_INVALID_ARGUMENT);
    CHECK(memcmp(&receiver,&receiver_before,sizeof(receiver))==0); return 0;
}
static int receiver_history_atomic(void)
{
    bool consumed=false; CHECK(init_receiver(&receiver)==0);
    CHECK(net_upper_uart_receiver_feed(&receiver,0xa5u,100u,100u,true,1u,&consumed,&delivery)==NET_WIRE_NEED_MORE && consumed);
    receiver.history_bytes[0]=0u; receiver_before=receiver; consumed=false;
    memset(&delivery,0xa6,sizeof(delivery)); out_before=delivery;
    CHECK(net_upper_uart_receiver_feed(&receiver,0x5au,101u,101u,true,1u,&consumed,&delivery)==NET_WIRE_TIME_HOLD);
    CHECK(!consumed && memcmp(&receiver,&receiver_before,sizeof(receiver))==0 && memcmp(&delivery,&out_before,sizeof(delivery))==0); return 0;
}
static int receiver_interleave(void)
{
    NetWireUartFrame f=frame(); bool consumed; size_t i;
    CHECK(init_receiver(&receiver)==0 && init_receiver(&other)==0);
    for(i=0u;i<f.length;++i) {
        NetWireStatus expected=i+1u==f.length?NET_WIRE_OK:NET_WIRE_NEED_MORE;
        CHECK(net_upper_uart_receiver_feed(&receiver,f.bytes[i],100u+i,100u+i,true,1u,&consumed,&delivery)==expected && consumed);
        CHECK(net_upper_uart_receiver_feed(&other,f.bytes[i],500u+i,500u+i,true,1u,&consumed,&out_before)==expected && consumed);
    }
    for(i=0u;i<f.length;++i) CHECK(delivery.byte_at_ms[i]==100u+i && out_before.byte_at_ms[i]==500u+i);
    CHECK(receiver.delivered_frames==1u && other.delivered_frames==1u); return 0;
}
static int receiver_typed_reject(void)
{
    NetWireUartFrame f=frame(); NetWireUartPayload payload; NetUpperUartMessage typed;
    uint16_t crc; size_t i; bool consumed=false;
    CHECK(init_receiver(&receiver)==0); f.bytes[16]=0xffu;
    CHECK(net_wire_crc16(f.bytes+2u,f.length-4u,&crc)==NET_WIRE_OK);
    f.bytes[f.length-2u]=(uint8_t)crc; f.bytes[f.length-1u]=(uint8_t)(crc>>8u);
    CHECK(net_wire_uart_decode(NET_WIRE_CANDIDATE_ONLY,NET_WIRE_UART_C,NET_WIRE_UPLINK,&f,&payload)==NET_WIRE_OK);
    CHECK(net_upper_uart_decode(NET_WIRE_CANDIDATE_ONLY,NET_WIRE_UART_C,NET_WIRE_UPLINK,&f,&typed)==NET_WIRE_MALFORMED);
    memset(&delivery,0xa6,sizeof(delivery)); out_before=delivery;
    for(i=0u;i<f.length;++i) CHECK(net_upper_uart_receiver_feed(&receiver,f.bytes[i],100u+i,100u+i,true,1u,&consumed,&delivery)==(i+1u==f.length?NET_WIRE_MALFORMED:NET_WIRE_NEED_MORE));
    CHECK(consumed && receiver.typed_rejected==1u && receiver.stream.count==0u && memcmp(&delivery,&out_before,sizeof(delivery))==0); return 0;
}
static int receiver_signal_failure(void)
{
    bool consumed; CHECK(init_receiver(&receiver)==0);
    CHECK(net_upper_uart_receiver_feed(&receiver,0xa5u,100u,100u,true,1u,&consumed,&delivery)==NET_WIRE_NEED_MORE);
    receiver.stream.policy.candidate_time_policy_selected=false; receiver_before=receiver;
    CHECK(net_upper_uart_receiver_signal(&receiver,2u,false,true)==NET_WIRE_CONTRACT_HOLD && memcmp(&receiver,&receiver_before,sizeof(receiver))==0);
    CHECK(net_upper_uart_receiver_signal(&receiver,0u,true,true)==NET_WIRE_INVALID_ARGUMENT && memcmp(&receiver,&receiver_before,sizeof(receiver))==0);
    CHECK(net_upper_uart_receiver_signal(&receiver,2u,true,false)==NET_WIRE_CONTRACT_HOLD && memcmp(&receiver,&receiver_before,sizeof(receiver))==0); return 0;
}
static int receiver_signal_epoch(void)
{
    bool consumed; CHECK(init_receiver(&receiver)==0);
    CHECK(net_upper_uart_receiver_feed(&receiver,0xa5u,100u,100u,true,1u,&consumed,&delivery)==NET_WIRE_NEED_MORE);
    receiver.stream.bad_crc=8u; receiver.stream.discarded_bytes=9u;
    CHECK(net_upper_uart_receiver_signal(&receiver,1u,true,false)==NET_WIRE_OK);
    CHECK(receiver.history_count==0u && receiver.stream.count==0u && receiver.stream.has_last_capture && receiver.stream.last_capture_ms==100u);
    CHECK(net_upper_uart_receiver_signal(&receiver,2u,true,true)==NET_WIRE_OK);
    CHECK(receiver.port_epoch==2u && !receiver.stream.has_last_capture && !receiver.stream.has_last_check && receiver.stream.byte_losses==2u && receiver.stream.bad_crc==8u && receiver.stream.discarded_bytes==9u); return 0;
}
static int make_delivery(NetUpperUartMessage *m,uint64_t first,NetUpperUartDelivery *d)
{
    size_t i; memset(d,0,sizeof(*d)); d->message=*m; d->port_epoch=1u;
    CHECK(net_upper_uart_encode(NET_WIRE_CANDIDATE_ONLY,m,&d->frame)==NET_WIRE_OK);
    for(i=0u;i<d->frame.length;++i) d->byte_at_ms[i]=first+i;
    d->frame.captured_at_ms=first+d->frame.length-1u; return 0;
}
static int init_query(void)
{
    NetUpperUartMessage m={0}; NetUpperQueryOwner o={0};
    memset(&query,0,sizeof(query)); CHECK(net_upper_query_init(&query,&service,9u,&o)==NET_OK);
    m.link=NET_WIRE_UART_M; m.type=1u; m.sequence=1u; m.data.m_context.device_context_id=17u;
    m.data.m_context.session_id=0xbbu; m.data.m_context.proximity_validity=1u;
    CHECK(make_delivery(&m,100u,&context_delivery)==0 && net_upper_query_on_context(&query,&context_delivery)==NET_OK);
    memset(&m,0,sizeof(m)); m.link=NET_WIRE_UART_M; m.type=0x11u; m.sequence=2u;
    m.data.m_query.device_context_id=17u; m.data.m_query.session_id=0xbbu; m.data.m_query.query_id=19u;
    CHECK(make_delivery(&m,200u,&query_delivery)==0); return 0;
}
static int query_begin_failure(void)
{
    CHECK(init_query()==0); query.run.final_success_count=17u; query.run.response_count=2u; query_before=query;
    query_delivery.port_epoch=2u;
    CHECK(net_upper_query_begin(&query,&query_delivery)==NET_HOLD && memcmp(&query,&query_before,sizeof(query))==0);
    query_delivery.port_epoch=1u; query_delivery.frame.bytes[query_delivery.frame.length-1u]^=1u;
    CHECK(net_upper_query_begin(&query,&query_delivery)==NET_INVALID_ARGUMENT && memcmp(&query,&query_before,sizeof(query))==0);
    query_delivery.frame.bytes[query_delivery.frame.length-1u]^=1u;
    query.run.active=true; query_before=query;
    CHECK(net_upper_query_begin(&query,&query_delivery)==NET_BUSY && memcmp(&query,&query_before,sizeof(query))==0); return 0;
}
static int query_begin_alias(void)
{
    NetUpperUartDelivery *alias;
    CHECK(init_query()==0); alias=(NetUpperUartDelivery *)(void *)&query.run;
    memcpy(alias,&query_delivery,sizeof(query_delivery)); query_before=query;
    CHECK(net_upper_query_begin(&query,alias)==NET_INVALID_ARGUMENT && memcmp(&query,&query_before,sizeof(query))==0); return 0;
}
static int query_begin_replace(void)
{
    CHECK(init_query()==0); query.run.final_success_count=17u; query.run.failed_getter_count=4u;
    CHECK(net_upper_query_begin(&query,&query_delivery)==NET_OK);
    CHECK(query.run.active && query.run.final_success_count==0u && query.run.failed_getter_count==0u && query.run.request.query_id==19u);
    CHECK(query.run.received_length==query_delivery.frame.length && memcmp(query.run.byte_at_ms,query_delivery.byte_at_ms,query_delivery.frame.length*sizeof(uint64_t))==0);
    query.run.finished=true; query_before=query;
    CHECK(net_upper_query_begin(&query,&query_delivery)==NET_DUPLICATE && memcmp(&query,&query_before,sizeof(query))==0); return 0;
}
static NetStatus now(void *unused,uint64_t *out) { (void)unused; *out=100u; return NET_OK; }
static NetStatus submit(void *unused,const NetUpperPacket *p,NetTxKey k) { (void)unused; (void)p; (void)k; return NET_NOT_CONFIGURED; }
static NetStatus poll(void *unused,NetUpperEvent *out) { (void)unused; (void)out; return NET_NO_WORK; }
static int init_service(void)
{
    static const NetUpperPortOps ops={now,submit,poll}; NetUpperConfig c={0}; size_t i;
    memset(&service,0,sizeof(service)); c.node=NET_WIRE_NODE_DOMAIN; c.local_epoch=1u; c.profile=NET_WIRE_CANDIDATE_ONLY; c.port.ops=&ops;
    for(i=0u;i<3u;++i) c.port_epoch[i]=1u;
    c.uart_policy[0]=policy();
    CHECK(net_upper_init(&service,&c)==NET_OK); return 0;
}
static int control_quiesce_atomic(void)
{
    CHECK(init_service()==0); service.uart_rx[0].stream.configured=false; service_before=service;
    CHECK(net_upper_quiesce_link(&service,NET_WIRE_UART_M)==NET_HOLD && memcmp(&service,&service_before,sizeof(service))==0); return 0;
}
static int control_resume_atomic(void)
{
    CHECK(init_service()==0); CHECK(net_upper_quiesce_link(&service,NET_WIRE_UART_M)==NET_OK);
    service.uart_rx[0].stream.policy.assembly_limit_ms=0u; service_before=service;
    CHECK(net_upper_resume_link(&service,NET_WIRE_UART_M,2u)==NET_INVALID_ARGUMENT && memcmp(&service,&service_before,sizeof(service))==0); return 0;
}
static int stream_capacity(void)
{
    NetWireStreamInput input={0}; NetWireUartFrame out,before; size_t used=7u;
    CHECK(init_stream()==0); stream.count=NET_WIRE_UART_FRAME_MAX+1u; stream_before=stream;
    memset(&out,0xa6,sizeof(out)); before=out; input.timestamps_unambiguous=true;
    CHECK(net_wire_uart_stream_next(&stream,&input,&used,&out)==NET_WIRE_INVALID_ARGUMENT);
    CHECK(used==7u && memcmp(&stream,&stream_before,sizeof(stream))==0 && memcmp(&out,&before,sizeof(out))==0); return 0;
}
static int receiver_output_alias(void)
{
    bool consumed=false; NetUpperUartDelivery *alias;
    CHECK(init_receiver(&receiver)==0); alias=(NetUpperUartDelivery *)(void *)&receiver;
    receiver_before=receiver;
    CHECK(net_upper_uart_receiver_feed(&receiver,0xa5u,100u,100u,true,1u,&consumed,alias)==NET_WIRE_INVALID_ARGUMENT);
    CHECK(!consumed && memcmp(&receiver,&receiver_before,sizeof(receiver))==0); return 0;
}
int main(int argc,char **argv)
{
    int result=1;
    if(argc!=2) return 2;
    name=argv[1];
    if(strcmp(name,"stream_init_failure")==0) result=stream_init_failure();
    else if(strcmp(name,"stream_input_atomic")==0) result=stream_input_atomic();
    else if(strcmp(name,"stream_retained_atomic")==0) result=stream_retained_atomic();
    else if(strcmp(name,"stream_split")==0) result=stream_split();
    else if(strcmp(name,"stream_crc_suffix")==0) result=stream_crc_suffix();
    else if(strcmp(name,"receiver_init_failure")==0) result=receiver_init_failure();
    else if(strcmp(name,"receiver_history_atomic")==0) result=receiver_history_atomic();
    else if(strcmp(name,"receiver_interleave")==0) result=receiver_interleave();
    else if(strcmp(name,"receiver_typed_reject")==0) result=receiver_typed_reject();
    else if(strcmp(name,"receiver_signal_failure")==0) result=receiver_signal_failure();
    else if(strcmp(name,"receiver_signal_epoch")==0) result=receiver_signal_epoch();
    else if(strcmp(name,"query_begin_failure")==0) result=query_begin_failure();
    else if(strcmp(name,"query_begin_alias")==0) result=query_begin_alias();
    else if(strcmp(name,"query_begin_replace")==0) result=query_begin_replace();
    else if(strcmp(name,"control_quiesce_atomic")==0) result=control_quiesce_atomic();
    else if(strcmp(name,"control_resume_atomic")==0) result=control_resume_atomic();
    else if(strcmp(name,"stream_capacity")==0) result=stream_capacity();
    else if(strcmp(name,"receiver_output_alias")==0) result=receiver_output_alias();
    else return 2;
    if(result==0) printf("{\"case\":\"%s\",\"result\":\"PASS\"}\n",name);
    return result;
}
