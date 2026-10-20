#include "network/net_upper_control.h"
#include <stdio.h>
#include <string.h>

/* Only physical Port/clock and external Owner are faked. Control, receipt,
 * relay, codec, queue, outcomes, notices and lifecycle are product code. */
#define CHECK(x) do { if(!(x)){fprintf(stderr,"%s:%d: %s\n",name,__LINE__,#x);return 1;} } while(0)
#define PROFILE NET_WIRE_CANDIDATE_ONLY
static const char *name;
typedef struct {
    uint64_t now;
    NetStatus submit_status,owner_status;
    const NetUpperPacket *borrowed[NET_UPPER_TX_CAPACITY];
    NetTxKey keys[NET_UPPER_TX_CAPACITY];
    size_t submissions,can_calls,uart_calls;
    NetUpperCanMessage can;
    NetUpperUartDelivery uart;
    NetUpper *reenter;
    NetStatus nested_quiesce,nested_detail,nested_diagnostics,nested_ack;
} Fake;
static NetUpper service;
static Fake fake;
static NetStatus clock_now(void *context,uint64_t *out)
{ *out=((Fake*)context)->now;return NET_OK; }
static NetStatus submit(void *context,const NetUpperPacket *packet,NetTxKey key)
{
    Fake *f=context;size_t i;
    if(f->submit_status!=NET_OK)return f->submit_status;
    for(i=0u;i<NET_UPPER_TX_CAPACITY;++i)if(f->borrowed[i]==NULL)break;
    if(i==NET_UPPER_TX_CAPACITY)return NET_NO_CAPACITY;
    f->borrowed[i]=packet;f->keys[i]=key;++f->submissions;
    if(f->reenter!=NULL) {
        NetUpperTxDetail detail;NetUpperDiagnostics diagnostics;
        f->nested_quiesce=net_upper_quiesce_link(f->reenter,packet->link);
        f->nested_detail=net_upper_tx_detail(f->reenter,key,&detail);
        f->nested_diagnostics=net_upper_diagnostics(f->reenter,&diagnostics);
        f->nested_ack=net_upper_failure_ack(f->reenter,key,1u);
    }
    return NET_OK;
}
static NetStatus poll(void *context,NetUpperEvent *out)
{ (void)context;(void)out;return NET_NO_WORK; }
static NetStatus owner_can(void *context,const NetUpperCanMessage *message,const NetUpperDelivery *delivery)
{ Fake *f=context;(void)delivery;f->can=*message;++f->can_calls;return f->owner_status; }
static NetStatus owner_uart(void *context,const NetUpperUartDelivery *message,const NetUpperDelivery *delivery)
{ Fake *f=context;(void)delivery;f->uart=*message;++f->uart_calls;return f->owner_status; }
static const NetUpperPortOps ops={clock_now,submit,poll};
static int setup(uint32_t node,bool notices)
{
    NetUpperConfig config={0};size_t i;
    memset(&service,0,sizeof(service));memset(&fake,0,sizeof(fake));fake.now=100u;
    config.node=node;config.local_epoch=9u;config.profile=PROFILE;
    config.port.ops=&ops;config.port.context=&fake;
    config.owner.context=&fake;config.owner.can=owner_can;config.owner.uart=owner_uart;
    config.retain_failure_notices=notices;
    for(i=0u;i<3u;++i)config.port_epoch[i]=1u;
    for(i=0u;i<NET_UPPER_ROUTE_COUNT;++i) {
        config.rx_policy[i].bound=true;config.rx_policy[i].maximum_age_ms=500u;
        config.rx_policy[i].expires_at_equality=true;
    }
    for(i=0u;i<2u;++i) {
        config.uart_policy[i].candidate_time_policy_selected=true;
        config.uart_policy[i].assembly_limit_ms=1000u;config.uart_policy[i].expires_at_equality=true;
        config.uart_sequence_bound[i]=true;config.uart_initial_sequence[i]=100u;
    }
    CHECK(net_upper_init(&service,&config)==NET_OK);return 0;
}
static NetStatus apply(const NetUpperEvent *event)
{
    NetStatus s=net_upper_capture(&service,event);
    return s==NET_OK?net_upper_step(&service,NET_UPPER_RX_CAPACITY):s;
}
static NetStatus reset_link(NetWireLink link,uint64_t epoch)
{
    NetUpperEvent e={0};e.kind=NET_UPPER_LINK_RESET;e.link=link;e.capture.port_epoch=epoch;
    return apply(&e);
}
static NetStatus emit(NetTxKey key,NetWireLink link,unsigned part,NetTxOutcome outcome,bool accepted)
{
    NetUpperEvent e={0};size_t i;e.kind=NET_UPPER_TX_EVENT;e.link=link;e.tx.key=key;
    if(part==0u || part==3u){e.tx.admission_observed=true;e.tx.accepted=accepted;}
    if(part==1u || part==3u)e.tx.outcome=outcome;
    if(part==2u || part==3u) {
        e.tx.release_observed=true;
        for(i=0u;i<NET_UPPER_TX_CAPACITY;++i)
            if(fake.keys[i].epoch==key.epoch && fake.keys[i].token==key.token)fake.borrowed[i]=NULL;
    }
    return apply(&e);
}
static NetAgePolicy age_policy(void)
{ NetAgePolicy p={true,500u,true};return p; }
static NetStatus publish_rear(uint32_t sequence,NetTxKey *out)
{
    NetWireCisRearRaw rear={0};NetSourceStamp source={0};NetAgePolicy policy=age_policy();
    rear.boot_id=41u;rear.tx_sequence=sequence;rear.update_sequence=77u;
    rear.rear_distance_deci_cm=123u;rear.validity=1u;
    source.observation_id=77u;source.observed_at_ms=fake.now;
    return net_upper_cis_publish_rear(&service,&rear,&source,&policy,out);
}
static NetUpperUartMessage vision(uint16_t sequence)
{
    NetUpperUartMessage m={0};m.link=NET_WIRE_UART_C;m.type=0x60u;m.sequence=sequence;
    m.data.p_vision_result.pi_boot_id=31u;m.data.p_vision_result.vision_sequence=7u;
    m.data.p_vision_result.vision_age_ms=5u;m.data.p_vision_result.occupant_presence=1u;
    m.data.p_vision_result.presence_validity=1u;m.data.p_vision_result.occupant_count=2u;
    m.data.p_vision_result.count_validity=1u;m.data.p_vision_result.vision_status=1u;
    return m;
}
static NetUpperUartMessage context(uint16_t sequence)
{
    NetUpperUartMessage m={0};m.link=NET_WIRE_UART_M;m.type=0x01u;m.sequence=sequence;
    m.data.m_context.device_context_id=7u;m.data.m_context.session_id=11u;return m;
}
static NetStatus feed(const NetUpperUartMessage *message,NetWireUartFrame *wire)
{
    NetWireUartFrame frame;NetUpperEvent event={0};NetStatus status=NET_NO_WORK;size_t i;
    if(net_upper_uart_encode(PROFILE,message,&frame)!=NET_WIRE_OK)return NET_REJECTED;
    event.kind=NET_UPPER_UART_BYTE;event.link=message->link;
    event.capture.timestamp_known=true;event.capture.captured_at_ms=fake.now;
    event.capture.port_epoch=service.config.port_epoch[message->link];
    for(i=0u;i<frame.length;++i){event.byte=frame.bytes[i];status=apply(&event);}
    if(wire!=NULL)*wire=frame;
    return status;
}
static NetStatus permission(uint8_t value,uint8_t quality,uint32_t sequence,uint16_t age)
{
    NetWireDomainPermissionRaw p={0};NetUpperEvent e={0};
    p.boot_id=51u;p.tx_sequence=sequence;p.update_sequence=sequence;
    p.vehicle_power_permission=value;p.power_permission_quality=quality;p.source_age_ms=age;
    e.kind=NET_UPPER_CAN_RX;e.link=NET_WIRE_CAN;e.capture.port_epoch=service.config.port_epoch[NET_WIRE_CAN];
    e.capture.timestamp_known=true;e.capture.captured_at_ms=fake.now;
    if(net_wire_can_encode_permission(PROFILE,&p,&e.can)!=NET_WIRE_OK)return NET_REJECTED;
    return apply(&e);
}

static int uart_receipt(void)
{
    NetUpperUartMessage m=vision(3u);NetWireUartFrame frame;NetUpperUartReceipt receipt,copy;
    CHECK(setup(NET_WIRE_NODE_CIS,false)==0);
    CHECK(net_upper_received_uart(&service,NET_WIRE_UART_C,0x60u,&receipt)==NET_NO_WORK);
    CHECK(feed(&m,&frame)==NET_OK && fake.uart_calls==1u);
    CHECK(net_upper_received_uart(&service,NET_WIRE_UART_C,0x60u,&receipt)==NET_OK);
    CHECK(receipt.message.data.p_vision_result.occupant_count==2u && receipt.message.sequence==3u);
    CHECK(receipt.frame.length==frame.length && memcmp(receipt.frame.bytes,frame.bytes,frame.length)==0);
    CHECK(receipt.source.observed_at_ms==100u && receipt.source.original_age_ms==5u &&
          receipt.source.observation_id==receipt.delivery.receipt_id && receipt.has_wire_age);
    copy=receipt;receipt.message.data.p_vision_result.occupant_count=5u;
    CHECK(net_upper_received_uart(&service,NET_WIRE_UART_C,0x60u,&receipt)==NET_OK);
    CHECK(receipt.message.data.p_vision_result.occupant_count==2u && receipt.delivery.receipt_id==copy.delivery.receipt_id);
    fake.owner_status=NET_REJECTED;m.sequence=4u;m.data.p_vision_result.vision_sequence=8u;
    CHECK(feed(&m,NULL)==NET_REJECTED);
    CHECK(net_upper_received_uart(&service,NET_WIRE_UART_C,0x60u,&receipt)==NET_OK);
    CHECK(receipt.message.sequence==3u && receipt.delivery.receipt_id==copy.delivery.receipt_id);return 0;
}
static int uart_receipt_no_age(void)
{
    NetUpperUartMessage m=context(1u);NetUpperUartReceipt r,copy;NetUpperEvent loss={0};
    CHECK(setup(NET_WIRE_NODE_DOMAIN,false)==0 && feed(&m,NULL)==NET_OK);
    memset(&m,0,sizeof(m));m.link=NET_WIRE_UART_M;m.type=0x10u;m.sequence=2u;
    m.data.m_request.device_context_id=7u;m.data.m_request.session_id=11u;
    m.data.m_request.request_id=91u;m.data.m_request.kind=1u;
    CHECK(feed(&m,NULL)==NET_OK);
    CHECK(net_upper_received_uart(&service,NET_WIRE_UART_M,0x10u,&r)==NET_OK);
    CHECK(!r.has_wire_age && !r.delivery.freshness_known && r.source.original_age_ms==0u);
    CHECK(r.message.data.m_request.request_id==91u && r.message.data.m_request.session_id==11u);
    copy=r;loss.kind=NET_UPPER_LINK_LOSS;loss.link=NET_WIRE_UART_M;loss.capture.port_epoch=1u;
    CHECK(apply(&loss)==NET_HOLD && fake.uart_calls==2u);
    CHECK(net_upper_received_uart(&service,NET_WIRE_UART_M,0x10u,&r)==NET_HOLD);
    CHECK(memcmp(&r,&copy,sizeof(r))==0);
    CHECK(net_upper_received_uart(&service,NET_WIRE_UART_M,0x01u,&r)==NET_HOLD);
    CHECK(memcmp(&r,&copy,sizeof(r))==0 && fake.uart_calls==2u);
    m=context(3u);CHECK(feed(&m,NULL)==NET_OK && fake.uart_calls==3u);
    CHECK(net_upper_received_uart(&service,NET_WIRE_UART_M,0x10u,&r)==NET_NO_WORK);
    CHECK(memcmp(&r,&copy,sizeof(r))==0);return 0;
}
static int receipt_epoch(void)
{
    NetUpperUartMessage m=vision(1u);NetUpperUartReceipt r,old;
    NetUpperCanMessage can;NetUpperDelivery delivery;NetSourceStamp source;
    CHECK(setup(NET_WIRE_NODE_CIS,false)==0 && feed(&m,NULL)==NET_OK);
    CHECK(net_upper_received_uart(&service,NET_WIRE_UART_C,0x60u,&r)==NET_OK);old=r;
    CHECK(reset_link(NET_WIRE_UART_C,2u)==NET_OK);
    CHECK(net_upper_received_uart(&service,NET_WIRE_UART_C,0x60u,&r)==NET_HOLD);
    CHECK(r.delivery.receipt_id==old.delivery.receipt_id && r.delivery.capture.port_epoch==1u);
    m.sequence=2u;CHECK(feed(&m,NULL)==NET_OK);
    CHECK(net_upper_received_uart(&service,NET_WIRE_UART_C,0x60u,&r)==NET_OK && r.delivery.capture.port_epoch==2u);
    CHECK(permission(0u,0u,1u,0u)==NET_OK);
    CHECK(net_upper_received_can(&service,0x080u,&can,&delivery,&source)==NET_OK);
    CHECK(reset_link(NET_WIRE_CAN,2u)==NET_OK);
    CHECK(net_upper_received_can(&service,0x080u,&can,&delivery,&source)==NET_HOLD);
    CHECK(delivery.capture.port_epoch==1u);return 0;
}
static int tx_detail(void)
{
    NetWireCisRearRaw rear={0};NetSourceStamp source={77u,100u,5u};NetAgePolicy policy=age_policy();
    NetTxKey key;NetUpperTxDetail detail,copy;NetWireCisRearRaw wire;
    CHECK(setup(NET_WIRE_NODE_CIS,false)==0);
    rear.boot_id=41u;rear.tx_sequence=2u;rear.update_sequence=77u;
    rear.rear_distance_deci_cm=123u;rear.validity=1u;rear.source_age_ms=5u;
    CHECK(net_upper_cis_publish_rear(&service,&rear,&source,&policy,&key)==NET_OK);
    rear.rear_distance_deci_cm=321u;source.original_age_ms=99u;policy.maximum_age_ms=1u;
    CHECK(net_upper_tx_detail(&service,key,&detail)==NET_OK && detail.state.queued);
    CHECK(detail.can.data.cis_rear.rear_distance_deci_cm==123u && detail.source.original_age_ms==5u);
    fake.now=105u;CHECK(net_upper_step(&service,32u)==NET_OK);
    CHECK(net_upper_tx_detail(&service,key,&detail)==NET_OK && detail.state.submitted);
    CHECK(net_wire_can_decode_rear(PROFILE,&detail.packet.can,&wire)==NET_WIRE_OK && wire.source_age_ms==10u);
    CHECK(detail.can.data.cis_rear.source_age_ms==5u && detail.source.observed_at_ms==100u);
    CHECK(emit(key,NET_WIRE_CAN,3u,NET_TX_FAILED,true)==NET_OK);
    CHECK(net_upper_tx_detail(&service,key,&detail)==NET_OK && detail.state.outcome==NET_TX_FAILED);
    copy=detail;CHECK(net_upper_retire(&service,key)==NET_OK);
    CHECK(net_upper_tx_detail(&service,key,&detail)==NET_STALE && detail.can.data.cis_rear.rear_distance_deci_cm==123u);
    CHECK(copy.state.key.token==key.token && copy.source.observation_id==77u);return 0;
}
static int failure_notice(void)
{
    NetTxKey key;NetUpperFailureNotice notice,again;NetUpperTxSnapshot snapshot;
    CHECK(setup(NET_WIRE_NODE_CIS,true)==0 && publish_rear(1u,&key)==NET_OK);
    CHECK(net_upper_step(&service,32u)==NET_OK);
    CHECK(emit(key,NET_WIRE_CAN,0u,NET_TX_UNKNOWN,true)==NET_OK);
    CHECK(emit(key,NET_WIRE_CAN,2u,NET_TX_UNKNOWN,true)==NET_OK);
    CHECK(net_upper_failure_poll(&service,&notice)==NET_NO_WORK);
    CHECK(emit(key,NET_WIRE_CAN,1u,NET_TX_FAILED,true)==NET_OK);
    CHECK(net_upper_failure_poll(&service,&notice)==NET_OK && notice.version==1u);
    CHECK(notice.detail.state.outcome==NET_TX_FAILED && notice.detail.state.reference_released);
    CHECK(notice.detail.can.data.cis_rear.update_sequence==77u && notice.detail.source.observation_id==77u);
    CHECK(net_upper_failure_poll(&service,&again)==NET_OK && again.version==notice.version);
    CHECK(net_upper_retire(&service,key)==NET_HOLD);
    CHECK(emit(key,NET_WIRE_CAN,1u,NET_TX_FAILED,true)==NET_DUPLICATE);
    CHECK(net_upper_failure_poll(&service,&again)==NET_OK && again.version==1u);
    CHECK(net_upper_failure_ack(&service,key,1u)==NET_OK);
    CHECK(net_upper_failure_ack(&service,key,1u)==NET_DUPLICATE);
    CHECK(net_upper_failure_poll(&service,&again)==NET_NO_WORK);
    CHECK(net_upper_tx_snapshot(&service,key,&snapshot)==NET_OK && snapshot.outcome==NET_TX_FAILED);
    CHECK(net_upper_retire(&service,key)==NET_OK && net_upper_failure_ack(&service,key,1u)==NET_STALE);return 0;
}
static int failure_notice_versions(void)
{
    NetTxKey key;NetUpperFailureNotice notice;NetUpperLinkSnapshot link;
    CHECK(setup(NET_WIRE_NODE_CIS,true)==0 && publish_rear(1u,&key)==NET_OK);
    CHECK(net_upper_step(&service,32u)==NET_OK);
    CHECK(emit(key,NET_WIRE_CAN,0u,NET_TX_UNKNOWN,true)==NET_OK);
    CHECK(emit(key,NET_WIRE_CAN,1u,NET_TX_FAILED,true)==NET_OK);
    CHECK(net_upper_failure_poll(&service,&notice)==NET_OK && notice.version==1u);
    CHECK(emit(key,NET_WIRE_CAN,1u,NET_TX_SUCCEEDED,true)==NET_HOLD);
    CHECK(net_upper_failure_poll(&service,&notice)==NET_OK && notice.version==2u && notice.detail.state.conflict);
    CHECK(net_upper_failure_ack(&service,key,1u)==NET_STALE);
    CHECK(net_upper_failure_ack(&service,key,2u)==NET_OK);
    CHECK(net_upper_quiesce_link(&service,NET_WIRE_CAN)==NET_OK);
    CHECK(net_upper_link_snapshot(&service,NET_WIRE_CAN,&link)==NET_OK && link.conflicts==1u && !link.can_resume);
    CHECK(net_upper_retire(&service,key)==NET_HOLD && net_upper_resume_link(&service,NET_WIRE_CAN,2u)==NET_HOLD);return 0;
}
static int failure_notice_capacity(void)
{
    NetTxKey keys[NET_UPPER_TX_CAPACITY],extra={88u,99u};NetUpperFailureNotice n;size_t i;
    CHECK(setup(NET_WIRE_NODE_CIS,true)==0);
    for(i=0u;i<NET_UPPER_TX_CAPACITY;++i)CHECK(publish_rear((uint32_t)i+1u,&keys[i])==NET_OK);
    CHECK(net_upper_quiesce_link(&service,NET_WIRE_CAN)==NET_OK);
    for(i=0u;i<NET_UPPER_TX_CAPACITY;++i)CHECK(net_upper_retire(&service,keys[i])==NET_HOLD);
    CHECK(net_upper_resume_link(&service,NET_WIRE_CAN,2u)==NET_HOLD);
    for(i=0u;i<NET_UPPER_TX_CAPACITY;++i) {
        CHECK(net_upper_failure_poll(&service,&n)==NET_OK && n.detail.state.key.token==keys[i].token);
        CHECK(!n.detail.state.submitted && !n.detail.state.accepted && n.detail.state.outcome==NET_TX_UNKNOWN);
        CHECK(net_upper_failure_ack(&service,keys[i],n.version)==NET_OK);
        CHECK(net_upper_retire(&service,keys[i])==NET_OK);
    }
    CHECK(net_upper_failure_poll(&service,&n)==NET_NO_WORK && fake.submissions==0u);
    CHECK(net_upper_resume_link(&service,NET_WIRE_CAN,2u)==NET_OK);
    CHECK(publish_rear(99u,&extra)==NET_OK && extra.token>keys[15].token);return 0;
}
static int failure_notice_default_off(void)
{
    NetTxKey key;NetUpperFailureNotice n;NetUpperTxDetail d;
    CHECK(setup(NET_WIRE_NODE_CIS,false)==0 && publish_rear(1u,&key)==NET_OK);
    fake.submit_status=NET_REJECTED;CHECK(net_upper_step(&service,32u)==NET_REJECTED);
    CHECK(net_upper_tx_detail(&service,key,&d)==NET_OK && !d.failure_notice_enabled && d.failure_version==0u);
    CHECK(d.state.local_status==NET_REJECTED && d.state.outcome==NET_TX_UNKNOWN && !d.state.submitted);
    CHECK(net_upper_failure_poll(&service,&n)==NET_NO_WORK && net_upper_retire(&service,key)==NET_OK);return 0;
}
static int quiesce_empty(void)
{
    NetUpperLinkSnapshot link;NetTxKey key={88u,99u};
    CHECK(setup(NET_WIRE_NODE_CIS,false)==0);
    CHECK(net_upper_resume_link(&service,NET_WIRE_CAN,2u)==NET_HOLD);
    CHECK(net_upper_quiesce_link(&service,NET_WIRE_CAN)==NET_OK);
    CHECK(net_upper_quiesce_link(&service,NET_WIRE_CAN)==NET_OK);
    CHECK(net_upper_link_snapshot(&service,NET_WIRE_CAN,&link)==NET_OK && link.can_resume);
    CHECK(publish_rear(1u,&key)==NET_HOLD && key.token==99u);
    CHECK(net_upper_resume_link(&service,NET_WIRE_CAN,1u)==NET_STALE);
    CHECK(net_upper_resume_link(&service,NET_WIRE_CAN,2u)==NET_OK);
    CHECK(publish_rear(1u,&key)==NET_OK && key.token==1u);return 0;
}
static int quiesce_queued(void)
{
    NetTxKey key;NetUpperTxDetail detail;NetUpperLinkSnapshot link;
    CHECK(setup(NET_WIRE_NODE_CIS,false)==0 && publish_rear(1u,&key)==NET_OK);
    CHECK(net_upper_quiesce_link(&service,NET_WIRE_CAN)==NET_OK);
    CHECK(net_upper_tx_detail(&service,key,&detail)==NET_OK);
    CHECK(!detail.state.submitted && detail.state.admission_known && !detail.state.accepted && detail.state.reference_released);
    CHECK(detail.state.outcome==NET_TX_UNKNOWN && detail.state.local_status==NET_HOLD);
    CHECK(net_upper_step(&service,32u)==NET_NO_WORK && fake.submissions==0u);
    CHECK(net_upper_link_snapshot(&service,NET_WIRE_CAN,&link)==NET_OK && link.can_resume);
    CHECK(net_upper_retire(&service,key)==NET_OK && net_upper_resume_link(&service,NET_WIRE_CAN,2u)==NET_OK);return 0;
}
static int quiesce_six_orders(void)
{
    static const unsigned orders[6][3]={{0,1,2},{0,2,1},{1,0,2},{1,2,0},{2,0,1},{2,1,0}};
    size_t n,j;NetTxKey key;NetUpperLinkSnapshot state;NetUpperPacket immutable;
    for(n=0u;n<6u;++n) {
        CHECK(setup(NET_WIRE_NODE_CIS,false)==0 && publish_rear(1u,&key)==NET_OK);
        CHECK(net_upper_step(&service,32u)==NET_OK);immutable=*fake.borrowed[0];
        CHECK(net_upper_quiesce_link(&service,NET_WIRE_CAN)==NET_OK);
        CHECK(net_upper_resume_link(&service,NET_WIRE_CAN,2u)==NET_HOLD);
        for(j=0u;j<3u;++j) {
            CHECK(emit(key,NET_WIRE_CAN,orders[n][j],NET_TX_SUCCEEDED,true)==NET_OK);
            CHECK(net_upper_link_snapshot(&service,NET_WIRE_CAN,&state)==NET_OK);
            CHECK(state.can_resume==(j==2u));
            if(fake.borrowed[0]!=NULL)CHECK(memcmp(fake.borrowed[0]->can.bytes,immutable.can.bytes,immutable.can.length)==0);
        }
        CHECK(net_upper_resume_link(&service,NET_WIRE_CAN,2u)==NET_OK);
        CHECK(net_upper_retire(&service,key)==NET_OK);
    }
    return 0;
}
static int quiesce_conflict(void)
{
    NetTxKey key;NetUpperLinkSnapshot state;NetUpperFailureNotice notice;NetUpperTxDetail detail;uint64_t version;
    CHECK(setup(NET_WIRE_NODE_CIS,true)==0 && publish_rear(1u,&key)==NET_OK);
    CHECK(net_upper_step(&service,32u)==NET_OK && reset_link(NET_WIRE_CAN,2u)==NET_OK);
    CHECK(net_upper_quiesce_link(&service,NET_WIRE_CAN)==NET_OK);
    CHECK(emit(key,NET_WIRE_CAN,3u,NET_TX_SUCCEEDED,true)==NET_HOLD);
    CHECK(net_upper_failure_poll(&service,&notice)==NET_OK && notice.detail.state.conflict);
    CHECK(net_upper_failure_ack(&service,key,notice.version)==NET_OK);
    CHECK(net_upper_link_snapshot(&service,NET_WIRE_CAN,&state)==NET_OK && !state.can_resume && state.conflicts==1u);
    CHECK(net_upper_resume_link(&service,NET_WIRE_CAN,3u)==NET_HOLD && net_upper_retire(&service,key)==NET_HOLD);

    /* Already settled records carry no outstanding reference or unknown TX
     * fact. A reset must not invent a new contradiction for those records. */
    CHECK(setup(NET_WIRE_NODE_CIS,true)==0 && publish_rear(1u,&key)==NET_OK);
    CHECK(net_upper_step(&service,32u)==NET_OK && emit(key,NET_WIRE_CAN,3u,NET_TX_SUCCEEDED,true)==NET_OK);
    CHECK(reset_link(NET_WIRE_CAN,2u)==NET_OK);
    CHECK(net_upper_tx_detail(&service,key,&detail)==NET_OK && !detail.state.conflict && detail.state.reference_released);
    CHECK(detail.state.outcome==NET_TX_SUCCEEDED && detail.state.admission_known && detail.state.accepted);
    CHECK(net_upper_failure_poll(&service,&notice)==NET_NO_WORK);
    CHECK(net_upper_quiesce_link(&service,NET_WIRE_CAN)==NET_OK && net_upper_resume_link(&service,NET_WIRE_CAN,3u)==NET_OK);
    CHECK(net_upper_retire(&service,key)==NET_OK);

    CHECK(setup(NET_WIRE_NODE_CIS,true)==0 && publish_rear(1u,&key)==NET_OK);
    CHECK(net_upper_step(&service,32u)==NET_OK && emit(key,NET_WIRE_CAN,3u,NET_TX_FAILED,true)==NET_OK);
    CHECK(net_upper_failure_poll(&service,&notice)==NET_OK);version=notice.version;
    CHECK(reset_link(NET_WIRE_CAN,2u)==NET_OK);
    CHECK(net_upper_failure_poll(&service,&notice)==NET_OK && notice.version==version && !notice.detail.state.conflict);
    CHECK(notice.detail.state.reference_released && notice.detail.state.outcome==NET_TX_FAILED);
    CHECK(net_upper_quiesce_link(&service,NET_WIRE_CAN)==NET_OK);
    CHECK(net_upper_resume_link(&service,NET_WIRE_CAN,3u)==NET_HOLD && net_upper_retire(&service,key)==NET_HOLD);
    CHECK(net_upper_failure_ack(&service,key,version)==NET_OK);
    CHECK(net_upper_resume_link(&service,NET_WIRE_CAN,3u)==NET_OK && net_upper_retire(&service,key)==NET_OK);

    CHECK(setup(NET_WIRE_NODE_CIS,true)==0 && publish_rear(1u,&key)==NET_OK);
    CHECK(net_upper_step(&service,32u)==NET_OK && emit(key,NET_WIRE_CAN,0u,NET_TX_UNKNOWN,false)==NET_OK);
    CHECK(emit(key,NET_WIRE_CAN,2u,NET_TX_UNKNOWN,false)==NET_OK);
    CHECK(net_upper_failure_poll(&service,&notice)==NET_OK);version=notice.version;
    CHECK(reset_link(NET_WIRE_CAN,2u)==NET_OK);
    CHECK(net_upper_failure_poll(&service,&notice)==NET_OK && notice.version==version && !notice.detail.state.conflict);
    CHECK(notice.detail.state.admission_known && !notice.detail.state.accepted && notice.detail.state.reference_released);
    CHECK(notice.detail.state.outcome==NET_TX_UNKNOWN);
    CHECK(net_upper_failure_ack(&service,key,version)==NET_OK && net_upper_retire(&service,key)==NET_OK);

    /* Release alone never completes an accepted transmission. */
    CHECK(setup(NET_WIRE_NODE_CIS,true)==0 && publish_rear(1u,&key)==NET_OK);
    CHECK(net_upper_step(&service,32u)==NET_OK && emit(key,NET_WIRE_CAN,0u,NET_TX_UNKNOWN,true)==NET_OK);
    CHECK(emit(key,NET_WIRE_CAN,2u,NET_TX_UNKNOWN,true)==NET_OK);
    CHECK(reset_link(NET_WIRE_CAN,2u)==NET_OK);
    CHECK(net_upper_failure_poll(&service,&notice)==NET_OK && notice.detail.state.conflict);
    CHECK(notice.detail.state.reference_released && notice.detail.state.outcome==NET_TX_UNKNOWN);
    CHECK(net_upper_failure_ack(&service,key,notice.version)==NET_OK);
    CHECK(net_upper_quiesce_link(&service,NET_WIRE_CAN)==NET_OK);
    CHECK(net_upper_resume_link(&service,NET_WIRE_CAN,3u)==NET_HOLD && net_upper_retire(&service,key)==NET_HOLD);return 0;
}
static int quiesce_uart(void)
{
    NetUpperUartMessage m=context(1u);NetWireUartFrame frame;NetUpperEvent e={0};
    NetUpperLinkSnapshot state;NetUpperDiagnostics d;size_t i;
    CHECK(setup(NET_WIRE_NODE_DOMAIN,false)==0 && feed(&m,NULL)==NET_OK);
    CHECK(net_upper_diagnostics(&service,&d)==NET_OK && d.context_current);
    m.sequence=2u;CHECK(net_upper_uart_encode(PROFILE,&m,&frame)==NET_WIRE_OK);
    e.kind=NET_UPPER_UART_BYTE;e.link=NET_WIRE_UART_M;e.capture.port_epoch=1u;e.capture.timestamp_known=true;e.capture.captured_at_ms=100u;
    for(i=0u;i<5u;++i){e.byte=frame.bytes[i];CHECK(apply(&e)==NET_NO_WORK);}
    CHECK(net_upper_link_snapshot(&service,NET_WIRE_UART_M,&state)==NET_OK && state.partial_uart_bytes>0u);
    CHECK(net_upper_quiesce_link(&service,NET_WIRE_UART_M)==NET_OK);
    CHECK(net_upper_link_snapshot(&service,NET_WIRE_UART_M,&state)==NET_OK && state.can_resume && state.partial_uart_bytes==0u);
    CHECK(net_upper_diagnostics(&service,&d)==NET_OK && !d.context_current);
    e.byte=frame.bytes[5];CHECK(apply(&e)==NET_HOLD && fake.uart_calls==1u);
    CHECK(net_upper_resume_link(&service,NET_WIRE_UART_M,2u)==NET_OK);
    CHECK(net_upper_diagnostics(&service,&d)==NET_OK && !d.context_current && d.next_uart_sequence[0]==100u);
    CHECK(apply(&e)==NET_STALE); /* Explicit old capture epoch remains rejected. */
    CHECK(feed(&m,NULL)==NET_OK && fake.uart_calls==2u);
    CHECK(net_upper_diagnostics(&service,&d)==NET_OK && d.context_current && d.context_port_epoch==2u);return 0;
}
static int permission_relay(void)
{
    NetTxKey key;NetAgePolicy p=age_policy();NetUpperTxDetail d;NetWirePPermission decoded;uint16_t seq;
    CHECK(setup(NET_WIRE_NODE_CIS,false)==0 && permission(0u,0u,7u,37u)==NET_OK);
    CHECK(net_upper_cis_publish_pi_permission(&service,81u,1500u,&p,&key)==NET_OK);
    fake.now=103u;CHECK(net_upper_step(&service,32u)==NET_OK);
    CHECK(net_upper_tx_detail(&service,key,&d)==NET_OK && d.source.original_age_ms==37u);
    CHECK(net_wire_p_permission_decode(PROFILE,&fake.borrowed[0]->uart,&decoded,&seq)==NET_WIRE_OK);
    CHECK(decoded.vehicle_power_permission==0u && decoded.cis_boot_id==81u && decoded.update_sequence==7u);
    CHECK(decoded.source_age_ms==40u && decoded.use_limit_ms==1500u && seq==100u);
    CHECK(d.uart.data.p_permission.source_age_ms==37u && d.source.observed_at_ms==100u);
    printf("{\"case\":\"permission_relay\",\"accepted_permission\":0,\"wire_permission\":0,\"original_age\":37,\"wire_age\":40,\"use_limit_explicit\":1500,\"sequence\":100}\n");return 0;
}
static int permission_hold(void)
{
    NetTxKey key={88u,99u};NetAgePolicy p=age_policy();
    CHECK(setup(NET_WIRE_NODE_CIS,false)==0);
    CHECK(net_upper_cis_publish_pi_permission(&service,81u,1500u,&p,&key)==NET_NO_WORK);
    CHECK(permission(0u,1u,1u,0u)==NET_OK);
    CHECK(net_upper_cis_publish_pi_permission(&service,81u,1500u,&p,&key)==NET_HOLD && key.token==99u);
    CHECK(permission(0u,0u,2u,0u)==NET_OK);fake.now=599u;
    CHECK(net_upper_cis_publish_pi_permission(&service,81u,1500u,&p,&key)==NET_OK);
    fake.now=600u;CHECK(net_upper_step(&service,32u)==NET_EXPIRED && fake.submissions==0u);
    CHECK(net_upper_retire(&service,key)==NET_OK);
    CHECK(net_upper_cis_publish_pi_permission(&service,81u,1500u,&p,&key)==NET_EXPIRED);
    CHECK(setup(NET_WIRE_NODE_DOMAIN,false)==0);
    CHECK(net_upper_cis_publish_pi_permission(&service,81u,1500u,&p,&key)==NET_REJECTED);return 0;
}
static int permission_epoch(void)
{
    NetTxKey key;NetAgePolicy p=age_policy();NetUpperTxDetail d;
    CHECK(setup(NET_WIRE_NODE_CIS,false)==0 && permission(0u,0u,1u,0u)==NET_OK);
    CHECK(net_upper_cis_publish_pi_permission(&service,81u,1500u,&p,&key)==NET_OK);
    CHECK(reset_link(NET_WIRE_CAN,2u)==NET_HOLD); /* reset drains before queued relay dispatch */
    CHECK(net_upper_tx_detail(&service,key,&d)==NET_OK);
    CHECK(!d.state.submitted && !d.state.accepted && d.state.local_status==NET_HOLD && fake.submissions==0u);
    CHECK(net_upper_cis_publish_pi_permission(&service,81u,1500u,&p,&key)==NET_HOLD);
    CHECK(permission(0u,0u,2u,0u)==NET_OK);
    CHECK(net_upper_cis_publish_pi_permission(&service,81u,1500u,&p,&key)==NET_OK);
    CHECK(net_upper_step(&service,32u)==NET_OK && fake.submissions==1u);return 0;
}
static int diagnostics(void)
{
    NetUpperDiagnostics a,b;NetTxKey key;NetUpperEvent event={0};
    CHECK(setup(NET_WIRE_NODE_CIS,true)==0 && publish_rear(1u,&key)==NET_OK);
    event.kind=NET_UPPER_PORT_ERROR;event.link=NET_WIRE_UART_C;
    CHECK(net_upper_capture(&service,&event)==NET_OK);
    CHECK(net_upper_diagnostics(&service,&a)==NET_OK && a.tx_used==1u && a.rx_used==1u);
    CHECK(a.links[NET_WIRE_CAN].queued_tx==1u && a.links[NET_WIRE_UART_C].pending_rx==1u);
    CHECK(net_upper_diagnostics(&service,&b)==NET_OK && b.observation.tx_queued==a.observation.tx_queued);
    CHECK(net_upper_quiesce_link(&service,NET_WIRE_CAN)==NET_OK);
    CHECK(net_upper_diagnostics(&service,&b)==NET_OK && b.links[NET_WIRE_CAN].pending_failure_notices==1u);
    CHECK(a.links[NET_WIRE_CAN].queued_tx==1u && !a.links[NET_WIRE_CAN].quiescing);
    CHECK(!b.context_current && b.context.device_context_id==0u);
    CHECK(b.sequence_bound[1] && b.next_uart_sequence[1]==100u && !b.sequence_exhausted[1]);return 0;
}
static int control_arguments(void)
{
    NetTxKey key;NetUpperTxDetail d,old;NetUpperDiagnostics diag;NetUpperUartReceipt receipt;
    CHECK(setup(NET_WIRE_NODE_CIS,false)==0);
    memset(&d,0x5a,sizeof(d));old=d;
    CHECK(net_upper_tx_detail(&service,(NetTxKey){1u,99u},&d)==NET_STALE);
    CHECK(memcmp(&d,&old,sizeof(d))==0);
    CHECK(net_upper_diagnostics(&service,(NetUpperDiagnostics*)&service)==NET_INVALID_ARGUMENT);
    CHECK(net_upper_received_uart(&service,NET_WIRE_CAN,0x60u,&receipt)==NET_INVALID_ARGUMENT);
    CHECK(net_upper_quiesce_link(&service,(NetWireLink)99)==NET_INVALID_ARGUMENT);
    CHECK(net_upper_resume_link(&service,NET_WIRE_CAN,0u)==NET_INVALID_ARGUMENT);
    CHECK(net_upper_failure_ack(&service,(NetTxKey){1u,1u},0u)==NET_INVALID_ARGUMENT);
    CHECK(publish_rear(1u,&key)==NET_OK);fake.reenter=&service;
    CHECK(net_upper_step(&service,32u)==NET_OK);
    CHECK(fake.nested_quiesce==NET_BUSY && fake.nested_detail==NET_BUSY &&
          fake.nested_diagnostics==NET_BUSY && fake.nested_ack==NET_BUSY);
    CHECK(net_upper_diagnostics(&service,&diag)==NET_OK && !diag.links[NET_WIRE_CAN].quiescing);return 0;
}
static int quiesce_independent(void)
{
    NetUpperUartMessage m=vision(1u);NetTxKey key;NetUpperEvent rx={0};NetUpperLinkSnapshot state;
    CHECK(setup(NET_WIRE_NODE_CIS,false)==0);
    CHECK(net_upper_quiesce_link(&service,NET_WIRE_CAN)==NET_OK);
    CHECK(feed(&m,NULL)==NET_OK && fake.uart_calls==1u);
    CHECK(publish_rear(1u,&key)==NET_HOLD);
    rx.kind=NET_UPPER_CAN_RX;rx.link=NET_WIRE_CAN;
    rx.capture.port_epoch=1u;rx.capture.timestamp_known=true;rx.capture.captured_at_ms=100u;
    CHECK(net_upper_capture(&service,&rx)==NET_OK);
    CHECK(net_upper_link_snapshot(&service,NET_WIRE_CAN,&state)==NET_OK && state.pending_rx==1u && !state.can_resume);
    CHECK(net_upper_resume_link(&service,NET_WIRE_CAN,2u)==NET_HOLD);
    CHECK(net_upper_step(&service,32u)==NET_HOLD && fake.can_calls==0u);
    CHECK(net_upper_resume_link(&service,NET_WIRE_CAN,2u)==NET_OK);
    CHECK(fake.uart.message.data.p_vision_result.vision_sequence==7u);return 0;
}
static int failure_notice_exhaustion(void)
{
    NetTxKey key;NetUpperFailureNotice notice;NetUpperLinkSnapshot state;
    CHECK(setup(NET_WIRE_NODE_CIS,true)==0 && publish_rear(1u,&key)==NET_OK);
    /* Fault injection at a counter boundary, not a replacement outcome engine. */
    service.tx[0].failure_version=UINT64_MAX-1u;service.tx[0].failure_ack_version=UINT64_MAX-1u;
    CHECK(net_upper_cancel(&service,key)==NET_OK);
    CHECK(net_upper_failure_poll(&service,&notice)==NET_OK && notice.version==UINT64_MAX);
    CHECK(net_upper_failure_ack(&service,key,UINT64_MAX)==NET_HOLD);
    CHECK(net_upper_failure_ack(&service,key,UINT64_MAX-1u)==NET_STALE);
    CHECK(net_upper_retire(&service,key)==NET_HOLD);
    CHECK(net_upper_quiesce_link(&service,NET_WIRE_CAN)==NET_OK);
    CHECK(net_upper_link_snapshot(&service,NET_WIRE_CAN,&state)==NET_OK && !state.can_resume);return 0;
}
int main(int argc,char **argv)
{
    int result=1;if(argc!=2)return 2;name=argv[1];
#define RUN(c) if(strcmp(name,#c)==0)result=c(); else
    RUN(uart_receipt) RUN(uart_receipt_no_age) RUN(receipt_epoch) RUN(tx_detail)
    RUN(failure_notice) RUN(failure_notice_versions) RUN(failure_notice_capacity) RUN(failure_notice_default_off)
    RUN(quiesce_empty) RUN(quiesce_queued) RUN(quiesce_six_orders) RUN(quiesce_conflict) RUN(quiesce_uart)
    RUN(permission_relay) RUN(permission_hold) RUN(permission_epoch) RUN(diagnostics) RUN(control_arguments)
    RUN(quiesce_independent) RUN(failure_notice_exhaustion) {return 2;}
#undef RUN
    if(result==0)printf("{\"case\":\"%s\",\"product_control\":true,\"fake_scope\":\"Port_clock_external_Owner\",\"result\":\"PASS\"}\n",name);
    return result;
}
