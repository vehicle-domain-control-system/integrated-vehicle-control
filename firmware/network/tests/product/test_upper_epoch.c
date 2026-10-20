/* Reuse only the physical Port/external Owner harness. The included test entry
 * remains compiled but is not called; every tested Network action is product code. */
#define main identity_harness_entry
#include "test_upper_rx_identity.c"
#undef main

static const NetAgePolicy epoch_policy={true,5000u,true};
static NetStatus signal_link(NetUpperEventKind kind,NetWireLink link,uint64_t epoch)
{
    NetUpperEvent e={0};NetStatus s;e.kind=kind;e.link=link;e.capture=capture();e.capture.port_epoch=epoch;
    s=net_upper_capture(&service,&e);return s==NET_OK?net_upper_step(&service,1u):s;
}
static int get_rear(NetUpperCanMessage *m,NetUpperDelivery *d,NetSourceStamp *source)
{ CHECK(net_upper_received_can(&service,0x0a0u,m,d,source)==NET_OK);return 0; }
static int prepare_rear(NetUpperUartMessage *projection,NetSourceStamp *source)
{
    NetUpperCanMessage m=rear_sample();NetUpperDelivery d;NetUpperUartMessage context=context_sample();
    CHECK(setup(NET_WIRE_NODE_DOMAIN)==0);CHECK(uart_message(&context)==NET_OK);CHECK(can_message(&m)==NET_OK);
    CHECK(get_rear(&m,&d,source)==0);CHECK(net_upper_uart_project_can(PROFILE,&m,12u,34u,51u,1u,17u,projection)==NET_WIRE_OK);
    return 0;
}
static int assert_local_hold(NetTxKey key)
{
    NetUpperTxSnapshot tx;CHECK(net_upper_tx_snapshot(&service,key,&tx)==NET_OK);
    /* Rejected before borrowing: local admission failure is not an observed
     * driver final-TX failure. UNKNOWN must remain UNKNOWN. */
    CHECK(!tx.queued&&!tx.submitted&&tx.admission_known&&!tx.accepted&&
        tx.outcome==NET_TX_UNKNOWN&&tx.reference_released&&tx.local_status==NET_HOLD);
    CHECK(port.borrowed==NULL&&service.observation.tx_submitted==0u);
    CHECK(net_upper_retire(&service,key)==NET_OK);return 0;
}
static int complete_borrow(NetWireLink link,NetTxKey key)
{
    NetUpperEvent e={0};CHECK(port.borrowed!=NULL);port.borrowed=NULL;
    e.kind=NET_UPPER_TX_EVENT;e.link=link;e.tx.key=key;e.tx.admission_observed=true;e.tx.accepted=true;
    e.tx.release_observed=true;e.tx.outcome=NET_TX_SUCCEEDED;
    CHECK(net_upper_capture(&service,&e)==NET_OK);CHECK(net_upper_step(&service,1u)==NET_OK);
    CHECK(net_upper_retire(&service,key)==NET_OK);return 0;
}
static int old_receipt_read(void)
{
    NetUpperCanMessage input=rear_sample(),out,expected;NetWireCanEnvelope a,b;NetUpperDelivery d,saved;
    NetSourceStamp source={99u,77u,55u};uint64_t receipt;unsigned calls;
    CHECK(setup(NET_WIRE_NODE_DOMAIN)==0);CHECK(can_message(&input)==NET_OK);receipt=service.next_receipt;calls=owner.calls;
    CHECK(signal_link(NET_UPPER_LINK_RESET,NET_WIRE_CAN,2u)==NET_OK);
    out=permission_sample();expected=out;memset(&d,0,sizeof(d));d.receipt_id=88u;d.age_ms=123u;saved=d;
    CHECK(expected_status(net_upper_received_can(&service,input.id,&out,&d,&source),NET_HOLD));
    CHECK(net_upper_can_encode(PROFILE,&out,&a)==NET_WIRE_OK&&net_upper_can_encode(PROFILE,&expected,&b)==NET_WIRE_OK&&equal_can(&a,&b));
    CHECK(equal_delivery(&d,&saved)&&source.observation_id==99u&&source.observed_at_ms==77u&&source.original_age_ms==55u);
    CHECK(service.next_receipt==receipt&&owner.calls==calls);return 0;
}
static int old_receipt_enqueue(void)
{
    NetUpperUartMessage m;NetSourceStamp source;NetTxKey key={99u,88u};uint64_t token;
    CHECK(prepare_rear(&m,&source)==0);token=service.next_token;
    CHECK(signal_link(NET_UPPER_LINK_RESET,NET_WIRE_CAN,2u)==NET_OK);
    CHECK(expected_status(net_upper_submit_uart(&service,&m,&source,&epoch_policy,&key),NET_HOLD));
    CHECK(key.epoch==99u&&key.token==88u&&service.next_token==token&&service.observation.tx_queued==0u&&port.borrowed==NULL);
    return 0;
}
static int queued_rear_reset(void)
{
    NetUpperUartMessage m;NetSourceStamp source;NetTxKey key;uint64_t receipt;
    CHECK(prepare_rear(&m,&source)==0);receipt=service.next_receipt;
    CHECK(net_upper_submit_uart(&service,&m,&source,&epoch_policy,&key)==NET_OK);CHECK(port.borrowed==NULL);
    CHECK(expected_status(signal_link(NET_UPPER_LINK_RESET,NET_WIRE_CAN,2u),NET_HOLD));
    CHECK(assert_local_hold(key)==0);CHECK(service.next_receipt==receipt&&service.config.port_epoch[NET_WIRE_UART_M]==1u);return 0;
}
static int queued_permission_reset(void)
{
    NetUpperCanMessage m=permission_sample();NetUpperUartMessage p={0};NetUpperDelivery d;NetSourceStamp source;NetTxKey key;
    CHECK(setup(NET_WIRE_NODE_CIS)==0);CHECK(can_message(&m)==NET_OK);
    CHECK(net_upper_received_can(&service,m.id,&m,&d,&source)==NET_OK);
    p.link=NET_WIRE_UART_C;p.type=0x62u;p.sequence=5u;p.data.p_permission.cis_boot_id=61u;
    p.data.p_permission.update_sequence=m.data.domain_permission.update_sequence;
    p.data.p_permission.vehicle_power_permission=m.data.domain_permission.vehicle_power_permission;
    p.data.p_permission.value_quality=m.data.domain_permission.power_permission_quality;
    p.data.p_permission.source_age_ms=m.data.domain_permission.source_age_ms;p.data.p_permission.use_limit_ms=1500u;
    CHECK(net_upper_submit_uart(&service,&p,&source,&epoch_policy,&key)==NET_OK);
    CHECK(expected_status(signal_link(NET_UPPER_LINK_RESET,NET_WIRE_CAN,2u),NET_HOLD));
    CHECK(assert_local_hold(key)==0);CHECK(service.config.port_epoch[NET_WIRE_UART_C]==1u);return 0;
}
static int queued_vision_reset(void)
{
    NetUpperUartMessage v=uart_sample(0u);NetTxKey key;uint64_t receipt;
    CHECK(setup(NET_WIRE_NODE_CIS)==0);CHECK(uart_message(&v)==NET_OK);receipt=service.next_receipt;
    CHECK(CisNet_PublishOccupant(&service,61u,3u,&epoch_policy,&key)==NET_OK);
    CHECK(expected_status(signal_link(NET_UPPER_LINK_RESET,NET_WIRE_UART_C,2u),NET_HOLD));
    CHECK(assert_local_hold(key)==0);CHECK(service.next_receipt==receipt&&service.config.port_epoch[NET_WIRE_CAN]==1u);
    key.epoch=99u;key.token=88u;CHECK(CisNet_PublishOccupant(&service,61u,4u,&epoch_policy,&key)==NET_HOLD);
    CHECK(key.epoch==99u&&key.token==88u);return 0;
}
static int newer_receipt_immutable(void)
{
    NetUpperUartMessage m,decoded;NetUpperCanMessage rear=rear_sample();NetSourceStamp source;NetTxKey key;
    CHECK(prepare_rear(&m,&source)==0);CHECK(net_upper_submit_uart(&service,&m,&source,&epoch_policy,&key)==NET_OK);
    ++rear.data.cis_rear.tx_sequence;++rear.data.cis_rear.update_sequence;rear.data.cis_rear.rear_distance_deci_cm=456u;
    CHECK(can_message(&rear)==NET_OK);CHECK(owner.rear.rear_distance_mm==456u&&port.borrowed!=NULL);
    CHECK(net_upper_uart_decode(PROFILE,NET_WIRE_UART_M,NET_WIRE_DOWNLINK,&port.borrowed->uart,&decoded)==NET_WIRE_OK);
    CHECK(decoded.type==0x36u&&decoded.data.m_cis_rear.rear_distance_deci_cm==123u);
    CHECK(decoded.data.m_cis_rear.original_can_tx_sequence==2u&&decoded.data.m_cis_rear.update_sequence==77u);
    CHECK(decoded.data.m_cis_rear.source_age_ms==10u);CHECK(complete_borrow(NET_WIRE_UART_M,key)==0);return 0;
}
static int source_error(void)
{
    NetUpperUartMessage m;NetUpperCanMessage out=rear_sample();NetUpperDelivery d={0};NetSourceStamp source,ignored={0};NetTxKey key;
    CHECK(prepare_rear(&m,&source)==0);CHECK(net_upper_submit_uart(&service,&m,&source,&epoch_policy,&key)==NET_OK);
    CHECK(expected_status(signal_link(NET_UPPER_PORT_ERROR,NET_WIRE_CAN,1u),NET_HOLD));
    CHECK(assert_local_hold(key)==0);CHECK(net_upper_received_can(&service,out.id,&out,&d,&ignored)==NET_HOLD);
    { unsigned calls=owner.calls;uint64_t receipt=service.next_receipt;
      ++out.data.cis_rear.tx_sequence;++out.data.cis_rear.update_sequence;
      CHECK(can_message(&out)==NET_HOLD);CHECK(owner.calls==calls&&service.next_receipt==receipt);
    }
    key.epoch=99u;key.token=88u;CHECK(net_upper_submit_uart(&service,&m,&source,&epoch_policy,&key)==NET_HOLD);
    CHECK(key.epoch==99u&&key.token==88u&&service.config.port_epoch[NET_WIRE_CAN]==1u);return 0;
}
static int epoch_recovery(void)
{
    NetUpperCanMessage m=rear_sample(),out;NetWireCanEnvelope f;NetUpperCapture c;NetUpperDelivery d;NetSourceStamp source;
    NetUpperUartMessage v=uart_sample(0u);NetWireUartFrame frame;NetUpperEvent e={0};size_t n;unsigned accepted;
    CHECK(setup(NET_WIRE_NODE_DOMAIN)==0);CHECK(can_message(&m)==NET_OK);CHECK(signal_link(NET_UPPER_LINK_RESET,NET_WIRE_CAN,2u)==NET_OK);
    CHECK(can_message(&m)==NET_STALE);CHECK(net_upper_can_encode(PROFILE,&m,&f)==NET_WIRE_OK);c=capture();c.port_epoch=2u;
    CHECK(can_frame(&f,c)==NET_HOLD);++m.data.cis_rear.tx_sequence;++m.data.cis_rear.update_sequence;
    CHECK(net_upper_can_encode(PROFILE,&m,&f)==NET_WIRE_OK);CHECK(can_frame(&f,c)==NET_OK);CHECK(get_rear(&out,&d,&source)==0);
    CHECK(d.capture.port_epoch==2u&&out.data.cis_rear.tx_sequence==3u&&source.original_age_ms==10u);
    CHECK(setup(NET_WIRE_NODE_CIS)==0);CHECK(uart_message(&v)==NET_OK);accepted=owner.accepted;
    ++v.sequence;++v.data.p_vision_result.vision_sequence;v.data.p_vision_result.occupant_count=2u;
    CHECK(net_upper_uart_encode(PROFILE,&v,&frame)==NET_WIRE_OK);e.kind=NET_UPPER_UART_BYTE;e.link=NET_WIRE_UART_C;e.capture=capture();
    for(n=0u;n<8u;++n){e.byte=frame.bytes[n];CHECK(net_upper_capture(&service,&e)==NET_OK);CHECK(net_upper_step(&service,1u)==NET_NO_WORK);}
    CHECK(signal_link(NET_UPPER_LINK_LOSS,NET_WIRE_UART_C,1u)==NET_HOLD);CHECK(owner.accepted==accepted&&service.config.port_epoch[NET_WIRE_UART_C]==1u);
    CHECK(uart_frame(NET_WIRE_UART_C,&frame)==NET_OK);CHECK(owner.accepted==accepted+1u&&owner.uart.message.data.p_vision_result.vision_sequence==78u);
    CHECK(owner.delivery.capture.port_epoch==1u&&owner.delivery.age_ms==10u);return 0;
}
static int buffered_error(void)
{
    NetUpperUartMessage alive={0};NetWireUartFrame first,second,outer;uint8_t payload[62]={0};
    NetUpperEvent e={0};size_t n;NetStatus status;
    CHECK(setup(NET_WIRE_NODE_ESP32)==0);alive.link=NET_WIRE_UART_M;alive.type=0x02u;alive.sequence=7u;
    alive.data.m_domain_alive.domain_boot_id=51u;alive.data.m_domain_alive.update_sequence=1u;
    alive.data.m_domain_alive.source_age_ms=10u;alive.data.m_domain_alive.domain_state=2u;alive.data.m_domain_alive.value_quality=1u;
    CHECK(net_upper_uart_encode(PROFILE,&alive,&first)==NET_WIRE_OK);++alive.sequence;++alive.data.m_domain_alive.update_sequence;
    CHECK(net_upper_uart_encode(PROFILE,&alive,&second)==NET_WIRE_OK);CHECK(first.length+second.length<=sizeof(payload));
    memcpy(payload,first.bytes,first.length);memcpy(payload+first.length,second.bytes,second.length);
    CHECK(net_wire_uart_encode(PROFILE,NET_WIRE_UART_M,NET_WIRE_DOWNLINK,0x22u,1u,payload,sizeof(payload),&outer)==NET_WIRE_OK);
    outer.bytes[outer.length-1u]^=1u;e.kind=NET_UPPER_UART_BYTE;e.link=NET_WIRE_UART_M;e.capture=capture();
    for(n=0u;n+1u<outer.length;++n){e.byte=outer.bytes[n];CHECK(net_upper_capture(&service,&e)==NET_OK);CHECK(net_upper_step(&service,1u)==NET_NO_WORK);}
    CHECK(owner.accepted==0u);e.byte=outer.bytes[outer.length-1u];CHECK(net_upper_capture(&service,&e)==NET_OK);
    e.kind=NET_UPPER_PORT_ERROR;CHECK(net_upper_capture(&service,&e)==NET_OK);status=net_upper_step(&service,2u);
    printf("TRACE %s failed_link=%u status=%d accepted_before_error_only=%u last_sequence=%u\n",case_name,
        service.link_failed[NET_WIRE_UART_M],(int)status,owner.accepted,owner.uart.message.sequence);
    CHECK(status==NET_HOLD&&service.link_failed[NET_WIRE_UART_M]&&owner.accepted==1u&&owner.uart.message.sequence==7u);
    CHECK(net_upper_step(&service,1u)==NET_NO_WORK&&owner.accepted==1u);
    e.kind=NET_UPPER_UART_BYTE;e.byte=0u;CHECK(net_upper_capture(&service,&e)==NET_OK);
    CHECK(net_upper_step(&service,1u)==NET_HOLD&&owner.accepted==1u);
    CHECK(signal_link(NET_UPPER_LINK_RESET,NET_WIRE_UART_M,2u)==NET_OK&&owner.accepted==1u);
    ++alive.sequence;++alive.data.m_domain_alive.update_sequence;
    CHECK(net_upper_uart_encode(PROFILE,&alive,&second)==NET_WIRE_OK);e.capture.port_epoch=2u;
    for(n=0u;n<second.length;++n){e.byte=second.bytes[n];CHECK(net_upper_capture(&service,&e)==NET_OK);status=net_upper_step(&service,1u);CHECK(status==(n+1u==second.length?NET_OK:NET_NO_WORK));}
    CHECK(owner.accepted==2u&&owner.uart.message.sequence==9u&&owner.delivery.capture.port_epoch==2u);
    CHECK(owner.uart.message.data.m_domain_alive.update_sequence==3u&&owner.delivery.age_ms==10u);return 0;
}
int main(int argc,char **argv)
{
    int r=1;if(argc!=2)return 2;case_name=argv[1];
    if(strcmp(case_name,"old_receipt_read")==0)r=old_receipt_read();
    else if(strcmp(case_name,"old_receipt_enqueue")==0)r=old_receipt_enqueue();
    else if(strcmp(case_name,"queued_rear_reset")==0)r=queued_rear_reset();
    else if(strcmp(case_name,"queued_permission_reset")==0)r=queued_permission_reset();
    else if(strcmp(case_name,"queued_vision_reset")==0)r=queued_vision_reset();
    else if(strcmp(case_name,"newer_receipt_immutable")==0)r=newer_receipt_immutable();
    else if(strcmp(case_name,"source_error")==0)r=source_error();
    else if(strcmp(case_name,"epoch_recovery")==0)r=epoch_recovery();
    else if(strcmp(case_name,"buffered_error")==0)r=buffered_error();
    if(r==0)printf("PASS %s: actual accepted receipt and queued source epoch; no fabricated release or fresh observation\n",case_name);
    return r;
}
