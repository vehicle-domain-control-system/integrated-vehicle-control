#include "network/net_domain_c2.h"
#include <stdio.h>
#include <string.h>
/* DOMAIN C2 adapter receive side: the ingress journal after a sender restart,
 * UART-M LINK_SEQUENCE wrap and a new mobile context, the CIS transition
 * barriers of 0x300 / 0x310, and the HOLD of every missing semantic callback.
 * Only the Port and the C2 Owner functions are faked; frames run through the
 * product service, codec and adapter unless a case calls the Owner callback
 * directly to see one adapter rule alone. */
#define CHECK(x) do{if(!(x)){fprintf(stderr,"line %d: %s\n",__LINE__,#x);return 1;}}while(0)
enum { C_DOOR, C_CLIMATE, C_LIGHT, C_BCM, C_ENV, C_OCC, C_REAR, C_CIS, C_VSS, C_RESULT, C_EVENT,
       C_WFAULT, C_CONN, C_PROX, C_ACK, C_FAIL, C_COUNT };
static NetUpper service;
static NetDomainC2 adapter;
static DomainIf_TxPort_t ports;
static uint64_t now=1000u;
static unsigned calls[C_COUNT];
static DomainIf_Status_t ret[C_COUNT];
static unsigned submissions;
static CisRearState_t last_rear;
static uint32_t pi_boot=6401u;
static bool initialized(void){return true;}
static DomainIf_Status_t set_port(const DomainIf_TxPort_t *p){if(p)ports=*p;else memset(&ports,0,sizeof(ports));return DOMAIN_IF_OK;}
#define FAKE(name,type,slot) static DomainIf_Status_t f_##name(const type *v){(void)v;++calls[slot];return ret[slot];}
FAKE(door,DoorState_t,C_DOOR)
FAKE(climate,ClimateState_t,C_CLIMATE)
FAKE(light,InteriorLightState_t,C_LIGHT)
FAKE(bcm,DomainIf_BcmStatusReport_t,C_BCM)
FAKE(env,CisCabinEnvironmentState_t,C_ENV)
FAKE(occ,CisOccupantState_t,C_OCC)
FAKE(cis,DomainIf_CisStatusReport_t,C_CIS)
FAKE(vss,DomainIf_VssStateReport_t,C_VSS)
FAKE(result,DomainIf_BcmCommandResult_t,C_RESULT)
FAKE(event,DomainIf_BcmProtectionEvent_t,C_EVENT)
FAKE(wfault,FaultInfo_t,C_WFAULT)
FAKE(ack,DomainIf_WarningAck_t,C_ACK)
FAKE(failure,NetworkLocalFailure_t,C_FAIL)
#undef FAKE
static DomainIf_Status_t f_rear(const CisRearState_t *v){last_rear=*v;++calls[C_REAR];return ret[C_REAR];}
static DomainIf_Status_t f_conn(DeviceContextId_t d,const DigitalKeyConnectionState_t *v){(void)d;(void)v;++calls[C_CONN];return ret[C_CONN];}
static DomainIf_Status_t f_prox(DeviceContextId_t d,const ProximityInput_t *v){(void)d;(void)v;++calls[C_PROX];return ret[C_PROX];}
static DomainIf_Status_t f_get_rear(uint32_t at,CisRearState_t *out){(void)at;*out=last_rear;return DOMAIN_IF_OK;}
static DomainIf_Status_t f_passive(DeviceContextId_t d,SessionId_t s){(void)d;(void)s;return DOMAIN_IF_OK;}
static NetStatus status_ok(void *c,uint16_t id,const NetUpperCanMessage *m,DataQuality_t *q){(void)c;(void)id;(void)m;*q=DATA_QUALITY_OK;return NET_OK;}
static NetStatus reason_ok(void *c,const NetUpperTxSnapshot *s,NetworkLocalFailureReason_t *out){(void)c;if(s->local_status!=NET_REJECTED)return NET_HOLD;*out=NETWORK_LOCAL_FAILURE_CONTEXT_INVALIDATED_BEFORE_SEND;return NET_OK;}
static bool confirm_true(void *c,uint16_t id,uint64_t receipt,uint32_t boot,uint32_t seq){(void)c;(void)id;(void)receipt;(void)boot;(void)seq;return true;}
static NetStatus clock_read(void *c,uint64_t *out){(void)c;*out=now;return NET_OK;}
static NetStatus submit(void *c,const NetUpperPacket *p,NetTxKey k){(void)c;(void)p;(void)k;++submissions;return NET_OK;}
static NetStatus poll(void *c,NetUpperEvent *out){(void)c;(void)out;return NET_NO_WORK;}
static const NetUpperPortOps ops={clock_read,submit,poll};
static int start(void)
{
    NetDomainC2Api api;NetDomainC2Config cfg;NetUpperConfig config;size_t i;
    memset(&api,0,sizeof(api));memset(&cfg,0,sizeof(cfg));memset(&config,0,sizeof(config));
    for(i=0u;i<C_COUNT;++i)ret[i]=DOMAIN_IF_OK;
    api.IsInitialized=initialized;api.SetTxPort=set_port;
    api.RxBcmDoorState=f_door;api.RxBcmClimateState=f_climate;api.RxBcmInteriorLightState=f_light;api.RxBcmStatus=f_bcm;
    api.RxCisCabinEnvironment=f_env;api.RxCisOccupant=f_occ;api.RxCisRear=f_rear;api.RxCisStatus=f_cis;api.RxVssState=f_vss;
    api.RxBcmCommandResult=f_result;api.RxBcmProtectionEvent=f_event;api.RxWindowFault=f_wfault;
    api.RxDigitalKeyConnection=f_conn;api.RxProximity=f_prox;api.RxWarningAck=f_ack;api.RxNetworkLocalFailure=f_failure;
    api.QueryGetCisRear=f_get_rear;api.ValidatePassiveQueryContext=f_passive;
    cfg.domain_boot=301u;cfg.bcm_boot=201u;cfg.first_can_sequence=1u;cfg.command_accept_limit_ms=100u;cfg.event_use_limit_ms=100u;
    cfg.transmit_policy.bound=true;cfg.transmit_policy.maximum_age_ms=66535u;cfg.transmit_policy.expires_at_equality=true;
    cfg.value_quality=net_domain_c2_value_quality_provisional;cfg.status_quality=status_ok;cfg.local_failure_reason=reason_ok;
    cfg.confirm_snapshot=confirm_true;cfg.failure_sink_ready=true;
    CHECK(net_domain_c2_init(&adapter,&service,NULL,&api,&cfg)==NET_OK);
    config.node=NET_WIRE_NODE_DOMAIN;config.local_epoch=3u;config.profile=NET_WIRE_CANDIDATE_ONLY;config.port.ops=&ops;
    config.owner=net_domain_c2_owner(&adapter);
    for(i=0u;i<3u;++i)config.port_epoch[i]=4u;
    for(i=0u;i<NET_UPPER_ROUTE_COUNT;++i){config.rx_policy[i].bound=true;config.rx_policy[i].maximum_age_ms=10000u;}
    for(i=0u;i<2u;++i){config.uart_policy[i].candidate_time_policy_selected=true;config.uart_policy[i].assembly_limit_ms=100u;
        config.uart_policy[i].expires_at_equality=true;config.uart_sequence_bound[i]=true;config.uart_initial_sequence[i]=1u;}
    CHECK(net_upper_init(&service,&config)==NET_OK);CHECK(net_domain_c2_register(&adapter)==NET_OK);
    return 0;
}
/* One valid frame per receive route; obs is the original observation identity
 * (UPDATE_SEQUENCE, occurrence, command) so that a new obs is a new observation. */
static NetUpperCanMessage route_message(uint16_t id,uint32_t boot,uint32_t tx,uint32_t obs)
{
    NetUpperCanMessage m;memset(&m,0,sizeof(m));m.id=id;
#define BASE(field) m.data.field.boot_id=boot;m.data.field.tx_sequence=tx
    switch(id){
    case 0x200u:BASE(bcm_door_state);m.data.bcm_door_state.update_sequence=obs;break;
    case 0x210u:BASE(bcm_climate_state);m.data.bcm_climate_state.update_sequence=obs;break;
    case 0x220u:BASE(bcm_light_state);m.data.bcm_light_state.update_sequence=obs;break;
    case 0x230u:BASE(bcm_status);m.data.bcm_status.update_sequence=obs;break;
    case 0x300u:BASE(cis_environment);m.data.cis_environment.temperature_validity=1u;m.data.cis_environment.humidity_validity=1u;
        m.data.cis_environment.illuminance_validity=1u;m.data.cis_environment.temperature_sequence=obs;
        m.data.cis_environment.humidity_sequence=obs;m.data.cis_environment.illuminance_sequence=obs;break;
    case 0x310u:BASE(cis_occupant);m.data.cis_occupant.pi_boot_id=pi_boot;m.data.cis_occupant.vision_sequence=obs;
        m.data.cis_occupant.presence_validity=1u;m.data.cis_occupant.count_validity=1u;break;
    case 0x0a0u:BASE(cis_rear);m.data.cis_rear.validity=1u;m.data.cis_rear.rear_distance_deci_cm=123u;m.data.cis_rear.update_sequence=obs;break;
    case 0x320u:BASE(cis_status);m.data.cis_status.update_sequence=obs;break;
    case 0x340u:BASE(vss_status);m.data.vss_status.update_sequence=obs;m.data.vss_status.last_fault_quality=3u;break;
    case 0x140u:BASE(bcm_result);m.data.bcm_result.command_domain_boot=301u;m.data.bcm_result.command_id=obs;
        m.data.bcm_result.command_kind=1u;m.data.bcm_result.result=2u;m.data.bcm_result.result_reason=9u;break;
    case 0x141u:BASE(bcm_event);m.data.bcm_event.event_kind=1u;m.data.bcm_event.occurrence_id=obs;break;
    case 0x330u:BASE(window_fault);m.data.window_fault.update_sequence=obs;break;
    default:break;
    }
#undef BASE
    return m;
}
static NetUpperDelivery can_meta(uint32_t boot,uint32_t tx)
{
    NetUpperDelivery d;memset(&d,0,sizeof(d));d.source_boot=boot;d.source_sequence=tx;d.capture.port_epoch=4u;
    d.capture.captured_at_ms=now;d.capture.timestamp_known=true;d.delivered_at_ms=now;d.receipt_id=1u;d.freshness_known=true;
    return d;
}
static int feed_can(const NetUpperCanMessage *m,uint64_t epoch)
{
    NetUpperEvent e;memset(&e,0,sizeof(e));
    e.kind=NET_UPPER_CAN_RX;e.link=NET_WIRE_CAN;e.capture.port_epoch=epoch;e.capture.timestamp_known=true;e.capture.captured_at_ms=now;
    CHECK(net_upper_can_encode(NET_WIRE_CANDIDATE_ONLY,m,&e.can)==NET_WIRE_OK);
    CHECK(net_upper_capture(&service,&e)==NET_OK);(void)net_upper_step(&service,1u);
    return 0;
}
static NetUpperUartMessage context_message(uint16_t seq,uint64_t session)
{
    NetUpperUartMessage m;memset(&m,0,sizeof(m));m.link=NET_WIRE_UART_M;m.type=0x01u;m.sequence=seq;
    m.data.m_context.device_context_id=7u;m.data.m_context.session_id=session;m.data.m_context.device_registration_state=1u;
    m.data.m_context.bt_connection_state=1u;m.data.m_context.app_active_state=1u;m.data.m_context.proximity_validity=1u;
    return m;
}
static NetUpperUartMessage ack_message(uint16_t seq,uint64_t session)
{
    NetUpperUartMessage m;memset(&m,0,sizeof(m));m.link=NET_WIRE_UART_M;m.type=0x12u;m.sequence=seq;
    m.data.m_warning_ack.device_context_id=7u;m.data.m_warning_ack.session_id=session;m.data.m_warning_ack.warning_type=1u;
    m.data.m_warning_ack.occurrence_id=3u;m.data.m_warning_ack.warning_domain_boot=301u;
    return m;
}
static int feed_uart(const NetUpperUartMessage *m)
{
    NetWireUartFrame frame;NetUpperEvent e;size_t i;
    CHECK(net_upper_uart_encode(NET_WIRE_CANDIDATE_ONLY,m,&frame)==NET_WIRE_OK);
    memset(&e,0,sizeof(e));e.kind=NET_UPPER_UART_BYTE;e.link=NET_WIRE_UART_M;e.capture.port_epoch=4u;
    e.capture.timestamp_known=true;e.capture.captured_at_ms=now;
    for(i=0u;i<frame.length;++i){e.byte=frame.bytes[i];CHECK(net_upper_capture(&service,&e)==NET_OK);(void)net_upper_step(&service,1u);}
    return 0;
}
static NetUpperUartDelivery uart_direct(const NetUpperUartMessage *m)
{
    NetUpperUartDelivery u;memset(&u,0,sizeof(u));u.port_epoch=4u;u.message=*m;return u;
}
static const NetDomainC2IngressRecord *record(NetWireLink link,uint16_t id)
{
    size_t i;
    for(i=0u;i<19u;++i)if(adapter.ingress[i].used&&adapter.ingress[i].link==link&&adapter.ingress[i].id==id)return &adapter.ingress[i];
    return NULL;
}
static size_t queued(void)
{
    size_t i,n=0u;
    for(i=0u;i<NET_UPPER_TX_CAPACITY;++i)if(service.tx[i].state.used&&service.tx[i].state.queued)++n;
    return n;
}
static size_t tx_records(void)
{
    size_t i,n=0u;
    for(i=0u;i<NET_UPPER_TX_CAPACITY;++i)if(adapter.tx[i].used)++n;
    return n;
}
static const struct { uint16_t id; unsigned slot; } plain[9]={
    {0x200u,C_DOOR},{0x210u,C_CLIMATE},{0x220u,C_LIGHT},{0x230u,C_BCM},{0x0a0u,C_REAR},
    {0x320u,C_CIS},{0x340u,C_VSS},{0x140u,C_RESULT},{0x141u,C_EVENT}};

/* A sender that restarts with a new BOOT counts TX_SEQUENCE from 1 again
 * (docs/network/interface/03_COMMUNICATION/10_CAN_FD.md, TX_SEQUENCE). The
 * service takes the new generation; the adapter passes it to C2 on the same
 * port epoch and rebases its journal. A late frame of the ended run is the
 * service's to refuse. */
static int ingress_sender_reboot(void)
{
    unsigned i;
    for(i=0u;i<9u;++i){
        uint16_t id=plain[i].id;unsigned s=plain[i].slot;NetUpperCanMessage m;const NetDomainC2IngressRecord *r;
        m=route_message(id,401u,1u,1u);CHECK(feed_can(&m,4u)==0);CHECK(calls[s]==1u);
        r=record(NET_WIRE_CAN,id);CHECK(r!=NULL&&r->boot==401u&&r->sequence==1u);
        m=route_message(id,402u,1u,1u);CHECK(feed_can(&m,4u)==0);CHECK(calls[s]==2u);
        CHECK(r->boot==402u&&r->sequence==1u&&r->epoch==4u&&!r->blocked);
        m=route_message(id,402u,2u,2u);CHECK(feed_can(&m,4u)==0);CHECK(calls[s]==3u);
        CHECK(r->boot==402u&&r->sequence==2u);
        m=route_message(id,401u,2u,3u);CHECK(feed_can(&m,4u)==0);CHECK(calls[s]==3u);
        CHECK(r->boot==402u&&r->sequence==2u);
        printf("REBOOT id=0x%03x c2_calls=%u boot=%u PASS\n",(unsigned)id,calls[s],(unsigned)r->boot);
    }
    CHECK(service.config.port_epoch[NET_WIRE_CAN]==4u);
    return 0;
}
/* A C2 failure blocks the record for good: a new sender BOOT does not clear
 * it. Only the integration acknowledgement on a newer port epoch does. */
static int ingress_blocked_survives_reboot(void)
{
    NetUpperCanMessage m;const NetDomainC2IngressRecord *r;NetUpperEvent e;
    ret[C_REAR]=DOMAIN_IF_QUEUE_FULL;
    m=route_message(0x0a0u,401u,1u,1u);CHECK(feed_can(&m,4u)==0);CHECK(calls[C_REAR]==1u);
    r=record(NET_WIRE_CAN,0x0a0u);CHECK(r!=NULL&&r->blocked);
    ret[C_REAR]=DOMAIN_IF_OK;
    m=route_message(0x0a0u,402u,1u,1u);CHECK(feed_can(&m,4u)==0);CHECK(calls[C_REAR]==1u);
    m=route_message(0x0a0u,402u,2u,2u);CHECK(feed_can(&m,4u)==0);CHECK(calls[C_REAR]==1u);
    CHECK(r->blocked&&r->boot==401u&&r->sequence==1u);
    CHECK(net_domain_c2_accept_ingress_epoch(&adapter,NET_WIRE_CAN,0x0a0u,4u,402u,true)==NET_STALE);
    CHECK(r->blocked);
    memset(&e,0,sizeof(e));e.kind=NET_UPPER_LINK_RESET;e.link=NET_WIRE_CAN;e.capture.port_epoch=5u;
    e.capture.timestamp_known=true;e.capture.captured_at_ms=now;
    CHECK(net_upper_capture(&service,&e)==NET_OK);(void)net_upper_step(&service,1u);
    CHECK(service.config.port_epoch[NET_WIRE_CAN]==5u);
    CHECK(net_domain_c2_accept_ingress_epoch(&adapter,NET_WIRE_CAN,0x0a0u,5u,402u,false)==NET_HOLD&&r->blocked);
    CHECK(net_domain_c2_accept_ingress_epoch(&adapter,NET_WIRE_CAN,0x0a0u,5u,402u,true)==NET_OK&&!r->blocked);
    m=route_message(0x0a0u,402u,3u,3u);CHECK(feed_can(&m,5u)==0);CHECK(calls[C_REAR]==2u);
    CHECK(r->boot==402u&&r->sequence==3u&&r->epoch==5u);
    return 0;
}
/* UART-M LINK_SEQUENCE modulo 65536
 * (docs/network/interface/03_COMMUNICATION/20_UART_M.md shared framing):
 * 65534, 65535, 0, 1 are in order; the same number is a duplicate; a number
 * behind or exactly opposite is stale; up to 2^15-1 ahead is newer. */
static int ingress_uart_wrap(void)
{
    static const uint16_t order[4]={65534u,65535u,0u,1u};
    NetUpperOwner owner=net_domain_c2_owner(&adapter);NetUpperDelivery d=can_meta(0u,0u);NetUpperUartMessage m;
    NetUpperUartDelivery u;unsigned i;const NetDomainC2IngressRecord *r;
    for(i=0u;i<4u;++i){m=ack_message(order[i],11u);u=uart_direct(&m);CHECK(owner.uart(owner.context,&u,&d)==NET_OK);CHECK(calls[C_ACK]==i+1u);}
    r=record(NET_WIRE_UART_M,0x12u);CHECK(r!=NULL&&r->sequence==1u);
    m=ack_message(1u,11u);u=uart_direct(&m);CHECK(owner.uart(owner.context,&u,&d)==NET_DUPLICATE);
    m=ack_message(0u,11u);u=uart_direct(&m);CHECK(owner.uart(owner.context,&u,&d)==NET_STALE);
    m=ack_message(65535u,11u);u=uart_direct(&m);CHECK(owner.uart(owner.context,&u,&d)==NET_STALE);
    m=ack_message((uint16_t)(1u+0x8000u),11u);u=uart_direct(&m);CHECK(owner.uart(owner.context,&u,&d)==NET_STALE);
    CHECK(calls[C_ACK]==4u&&r->sequence==1u);
    m=ack_message((uint16_t)(1u+0x7fffu),11u);u=uart_direct(&m);CHECK(owner.uart(owner.context,&u,&d)==NET_OK);
    CHECK(calls[C_ACK]==5u&&r->sequence==0x8000u);
    return 0;
}
/* A new mobile context (another session) from a restarted ESP32 starts its
 * LINK_SEQUENCE again. The service takes such an M_CONTEXT without its window
 * and opens every UART-M window; the adapter follows, so neither the new
 * M_CONTEXT nor the next frame of another UART-M route is refused. The same
 * context again keeps the window. */
static int ingress_context_restart(void)
{
    NetUpperOwner owner=net_domain_c2_owner(&adapter);NetUpperDelivery d=can_meta(0u,0u);
    NetUpperUartMessage m;NetUpperUartDelivery u;const NetDomainC2IngressRecord *r;
    m=context_message(500u,11u);CHECK(feed_uart(&m)==0);
    CHECK(calls[C_CONN]==1u&&calls[C_PROX]==1u&&service.context_seen&&service.context.session_id==11u);
    m=ack_message(501u,11u);CHECK(feed_uart(&m)==0);CHECK(calls[C_ACK]==1u);
    m=context_message(1u,12u);CHECK(feed_uart(&m)==0);
    CHECK(calls[C_CONN]==2u&&calls[C_PROX]==2u&&service.context.session_id==12u);
    r=record(NET_WIRE_UART_M,0x01u);CHECK(r!=NULL&&!r->blocked&&r->sequence==1u);
    m=ack_message(2u,12u);CHECK(feed_uart(&m)==0);CHECK(calls[C_ACK]==2u);
    r=record(NET_WIRE_UART_M,0x12u);CHECK(r!=NULL&&!r->blocked&&r->sequence==2u);
    m=context_message(0u,12u);u=uart_direct(&m);CHECK(owner.uart(owner.context,&u,&d)==NET_STALE);
    CHECK(calls[C_CONN]==2u);
    return 0;
}
/* 0x300 is passed to C2 only through the environment barrier opened for the
 * frame's CIS BOOT_ID. No transition is bound, so nothing opens it: the frame
 * is held before the journal and the C2 call, even with value quality and the
 * Rx function bound. The fixture then sets the barrier state by hand to show
 * that an open barrier passes only its own generation. */
static int env_hold(void)
{
    NetUpperOwner owner=net_domain_c2_owner(&adapter);NetUpperCanMessage m;NetUpperDelivery d;
    const NetDomainC2Barrier *b=&adapter.barrier[NET_DOMAIN_C2_ENVIRONMENT_BARRIER];uint32_t boundary=adapter.boundary_calls;
    adapter.config.cis_transition_barriers=true;
    CHECK(adapter.config.value_quality!=NULL&&adapter.api.RxCisCabinEnvironment!=NULL&&!b->open);
    m=route_message(0x300u,401u,1u,1u);d=can_meta(401u,1u);
    CHECK(owner.can(owner.context,&m,&d)==NET_HOLD);
    CHECK(calls[C_ENV]==0u&&record(NET_WIRE_CAN,0x300u)==NULL&&b->held==1u&&adapter.boundary_calls==boundary);
    m=route_message(0x300u,401u,2u,2u);CHECK(feed_can(&m,4u)==0);
    CHECK(calls[C_ENV]==0u&&record(NET_WIRE_CAN,0x300u)==NULL&&b->held==2u);
    m=route_message(0x0a0u,401u,1u,1u);CHECK(feed_can(&m,4u)==0);CHECK(calls[C_REAR]==1u);
    CHECK(adapter.barrier[NET_DOMAIN_C2_OCCUPANT_BARRIER].held==0u);
    adapter.barrier[NET_DOMAIN_C2_ENVIRONMENT_BARRIER].open=true;adapter.barrier[NET_DOMAIN_C2_ENVIRONMENT_BARRIER].id=402u;
    m=route_message(0x300u,401u,3u,3u);d=can_meta(401u,3u);CHECK(owner.can(owner.context,&m,&d)==NET_HOLD);
    CHECK(calls[C_ENV]==0u&&b->held==3u);
    m=route_message(0x300u,402u,1u,4u);d=can_meta(402u,1u);CHECK(owner.can(owner.context,&m,&d)==NET_OK);
    CHECK(calls[C_ENV]==1u&&record(NET_WIRE_CAN,0x300u)!=NULL&&b->held==3u);
    return 0;
}
/* 0x310 is passed only through the Occupant barrier opened for its
 * PI_BOOT_ID; PI_BOOT_ID 0 never passes. */
static int occupant_hold(void)
{
    NetUpperOwner owner=net_domain_c2_owner(&adapter);NetUpperCanMessage m;NetUpperDelivery d;
    const NetDomainC2Barrier *b=&adapter.barrier[NET_DOMAIN_C2_OCCUPANT_BARRIER];uint32_t boundary=adapter.boundary_calls;
    adapter.config.cis_transition_barriers=true;
    CHECK(adapter.config.value_quality!=NULL&&adapter.api.RxCisOccupant!=NULL&&!b->open);
    m=route_message(0x310u,401u,1u,1u);d=can_meta(401u,1u);
    CHECK(owner.can(owner.context,&m,&d)==NET_HOLD);
    CHECK(calls[C_OCC]==0u&&record(NET_WIRE_CAN,0x310u)==NULL&&b->held==1u&&adapter.boundary_calls==boundary);
    m=route_message(0x310u,401u,2u,2u);CHECK(feed_can(&m,4u)==0);
    CHECK(calls[C_OCC]==0u&&record(NET_WIRE_CAN,0x310u)==NULL&&b->held==2u);
    pi_boot=0u;m=route_message(0x310u,401u,3u,3u);d=can_meta(401u,3u);
    CHECK(owner.can(owner.context,&m,&d)==NET_HOLD&&calls[C_OCC]==0u&&b->held==3u);
    m=route_message(0x0a0u,401u,1u,1u);CHECK(feed_can(&m,4u)==0);CHECK(calls[C_REAR]==1u);
    CHECK(adapter.barrier[NET_DOMAIN_C2_ENVIRONMENT_BARRIER].held==0u);
    adapter.barrier[NET_DOMAIN_C2_OCCUPANT_BARRIER].open=true;adapter.barrier[NET_DOMAIN_C2_OCCUPANT_BARRIER].id=6401u;
    pi_boot=6401u;m=route_message(0x310u,401u,4u,4u);d=can_meta(401u,4u);CHECK(owner.can(owner.context,&m,&d)==NET_OK);
    CHECK(calls[C_OCC]==1u&&record(NET_WIRE_CAN,0x310u)!=NULL&&b->held==3u);
    pi_boot=0u;m=route_message(0x310u,401u,5u,5u);d=can_meta(401u,5u);CHECK(owner.can(owner.context,&m,&d)==NET_HOLD);
    adapter.barrier[NET_DOMAIN_C2_OCCUPANT_BARRIER].id=0u;
    m=route_message(0x310u,401u,6u,6u);d=can_meta(401u,6u);CHECK(owner.can(owner.context,&m,&d)==NET_HOLD);
    CHECK(calls[C_OCC]==1u&&b->held==5u);
    return 0;
}
static unsigned c2_total(void)
{
    unsigned i,n=0u;
    for(i=0u;i<C_COUNT;++i)n+=calls[i];
    return n;
}
/* Each missing semantic callback restricts only its own mapping: no C2 call,
 * no TX, no ingress record (the failure notice stays recorded and held); the
 * same route passes once the callback is bound. */
static int semantic_holds(void)
{
    static const struct { uint16_t id; unsigned slot; } value_routes[3]={{0x300u,C_ENV},{0x310u,C_OCC},{0x0a0u,C_REAR}};
    static const struct { uint16_t id; unsigned slot; } status_routes[4]={{0x230u,C_BCM},{0x320u,C_CIS},{0x340u,C_VSS},{0x330u,C_WFAULT}};
    NetUpperOwner owner=net_domain_c2_owner(&adapter);NetUpperQueryOwner qo=net_domain_c2_query_owner(&adapter);
    NetUpperCanMessage m;NetUpperDelivery d;unsigned i,before;uint32_t seq=1u;
    adapter.config.window_transport_enabled=true;
    /* value_quality: 0x300, 0x310, 0x0A0 */
    adapter.config.value_quality=NULL;before=c2_total();
    for(i=0u;i<3u;++i){m=route_message(value_routes[i].id,401u,seq,seq);d=can_meta(401u,seq);++seq;
        CHECK(owner.can(owner.context,&m,&d)==NET_HOLD);CHECK(record(NET_WIRE_CAN,value_routes[i].id)==NULL);}
    CHECK(c2_total()==before);
    adapter.config.value_quality=net_domain_c2_value_quality_provisional;
    for(i=0u;i<3u;++i){m=route_message(value_routes[i].id,401u,seq,seq);d=can_meta(401u,seq);++seq;
        CHECK(owner.can(owner.context,&m,&d)==NET_OK);CHECK(calls[value_routes[i].slot]==1u&&record(NET_WIRE_CAN,value_routes[i].id)!=NULL);}
    puts("HOLD value_quality: 0x300 0x310 0x0A0 PASS");
    /* status_quality: 0x230, 0x320, 0x340, 0x330 */
    adapter.config.status_quality=NULL;before=c2_total();
    for(i=0u;i<4u;++i){m=route_message(status_routes[i].id,401u,seq,seq);d=can_meta(401u,seq);++seq;
        CHECK(owner.can(owner.context,&m,&d)==NET_HOLD);CHECK(record(NET_WIRE_CAN,status_routes[i].id)==NULL);}
    CHECK(c2_total()==before);
    adapter.config.status_quality=status_ok;
    for(i=0u;i<4u;++i){m=route_message(status_routes[i].id,401u,seq,seq);d=can_meta(401u,seq);++seq;
        CHECK(owner.can(owner.context,&m,&d)==NET_OK);CHECK(calls[status_routes[i].slot]==1u&&record(NET_WIRE_CAN,status_routes[i].id)!=NULL);}
    puts("HOLD status_quality: 0x230 0x320 0x340 0x330 PASS");
    /* permission_bundle, window_permission_bundle, window_alive_bundle */
    {DomainIf_CisPowerPermission_t p;DomainIf_WindowOperationPermission_t wp;DomainIf_WindowDomainAlive_t wa;
        memset(&p,0,sizeof(p));memset(&wp,0,sizeof(wp));memset(&wa,0,sizeof(wa));
        p.vehiclePowerAllowed=true;p.quality=DATA_QUALITY_OK;p.meta.updateSequence=1u;
        wp.operationAllowed=true;wp.quality=DATA_QUALITY_OK;wp.meta.updateSequence=1u;wa.domainLinkAvailable=true;wa.meta.updateSequence=1u;
        CHECK(adapter.config.permission_bundle==NULL&&adapter.config.window_permission_bundle==NULL&&adapter.config.window_alive_bundle==NULL);
        CHECK(ports.txCisPowerPermission(&p)==DOMAIN_IF_NOT_AVAILABLE);
        CHECK(ports.txWindowOperationPermission(&wp)==DOMAIN_IF_NOT_AVAILABLE);
        CHECK(ports.txWindowDomainAlive(&wa)==DOMAIN_IF_NOT_AVAILABLE);
        CHECK(queued()==0u&&tx_records()==0u);}
    puts("HOLD permission_bundle window_permission_bundle window_alive_bundle: NOT_AVAILABLE, no TX PASS");
    /* window_event_projection: 0x091 inspection view */
    {NetUpperCanMessage w;NetUpperDelivery od=can_meta(501u,1u);NetDomainC2WindowView view,same;
        memset(&w,0,sizeof(w));w.id=0x091u;w.data.window_event.boot_id=501u;w.data.window_event.tx_sequence=1u;
        w.data.window_event.event_kind=3u;w.data.window_event.occurrence_id=9u;od.observation_only=true;
        memset(&view,0x5a,sizeof(view));same=view;before=c2_total();
        CHECK(adapter.config.window_event_projection==NULL);
        CHECK(net_domain_c2_window_view(&adapter,&w,&od,&view)==NET_HOLD&&memcmp(&view,&same,sizeof(view))==0);
        CHECK(c2_total()==before);}
    puts("HOLD window_event_projection: 0x091 view PASS");
    /* select_query_plan: M_QUERY plan */
    {NetWireMQueryRaw rq;NetUpperQueryPlan plan,same;
        memset(&rq,0,sizeof(rq));rq.device_context_id=7u;rq.session_id=11u;rq.query_id=5u;
        memset(&plan,0x5a,sizeof(plan));same=plan;before=adapter.boundary_calls;
        CHECK(adapter.config.select_query_plan==NULL);
        CHECK(qo.select_plan(qo.context,&rq,&plan)==NET_HOLD&&memcmp(&plan,&same,sizeof(plan))==0);
        CHECK(adapter.boundary_calls==before);}
    puts("HOLD select_query_plan: M_QUERY plan PASS");
    /* confirm_snapshot: state projection and Getter */
    {NetWireMQueryRaw rq;NetUpperQueryOutput out,same;size_t n=77u;
        m=route_message(0x0a0u,401u,seq,seq);++seq;CHECK(feed_can(&m,4u)==0);CHECK(calls[C_REAR]==2u);
        memset(&rq,0,sizeof(rq));rq.device_context_id=7u;rq.session_id=11u;rq.query_id=6u;
        adapter.config.confirm_snapshot=NULL;
        CHECK(ports.txMobileCisRear(&last_rear)==DOMAIN_IF_NOT_AVAILABLE);
        memset(&out,0x5a,sizeof(out));same=out;
        CHECK(qo.getters[NET_QUERY_CIS_REAR](qo.context,&rq,0u,now,&out,1u,&n)==NET_HOLD&&n==77u&&memcmp(&out,&same,sizeof(out))==0);
        CHECK(queued()==0u);
        adapter.config.confirm_snapshot=confirm_true;
        CHECK(qo.getters[NET_QUERY_CIS_REAR](qo.context,&rq,0u,now,&out,1u,&n)==NET_OK&&n==1u);
        CHECK(out.message.data.m_cis_rear.rear_distance_deci_cm==123u);}
    puts("HOLD confirm_snapshot: projection and Getter PASS");
    (void)net_upper_step(&service,0u);CHECK(submissions==0u);
    /* local_failure_reason: the local failure notice stays recorded and held */
    {DomainIf_BcmDoorCommand_t c;NetTxKey k;
        memset(&c,0,sizeof(c));c.context.commandId=77u;c.context.origin=COMMAND_ORIGIN_MOBILE_REQUEST;c.target=DOOR_TARGET_UNLOCK;
        adapter.config.local_failure_reason=NULL;
        CHECK(ports.txBcmDoorCommand(&c)==DOMAIN_IF_OK);k=adapter.last_tx_key;CHECK(net_upper_cancel(&service,k)==NET_OK);
        CHECK(net_domain_c2_step(&adapter)==NET_HOLD&&calls[C_FAIL]==0u&&tx_records()==1u);
        CHECK(net_domain_c2_step(&adapter)==NET_HOLD&&calls[C_FAIL]==0u&&tx_records()==1u);
        adapter.config.local_failure_reason=reason_ok;
        CHECK(net_domain_c2_step(&adapter)==NET_OK&&calls[C_FAIL]==1u&&tx_records()==0u);}
    puts("HOLD local_failure_reason: notice kept, delivered once bound PASS");
    (void)net_upper_step(&service,0u);CHECK(submissions==0u);
    /* Another route is unaffected throughout. */
    m=route_message(0x200u,401u,seq,seq);d=can_meta(401u,seq);CHECK(owner.can(owner.context,&m,&d)==NET_OK&&calls[C_DOOR]==1u);
    return 0;
}
/* BCM_COMMAND_RESULT (0x140) and BCM_PROTECTION_EVENT (0x141) report one
 * command or one event per frame. DOMAIN returns DOMAIN_IF_REJECTED for the
 * result of a command it has no record of (DOMAIN ResultManager NOT_FOUND):
 * that frame alone is dropped and counted (rejected_frames), the same frame is
 * not offered to C2 again, and the next frame of the route reaches C2. Any
 * other non-OK status on these routes, and REJECTED on a state route, still
 * blocks the route for the rest of the run. */
static int ingress_bcm_rejected_frame(void)
{
    static const struct { uint16_t id; unsigned slot; } lanes[2]={{0x140u,C_RESULT},{0x141u,C_EVENT}};
    NetUpperCanMessage m;const NetDomainC2IngressRecord *r;unsigned i;
    for(i=0u;i<2u;++i){unsigned s=lanes[i].slot;uint16_t id=lanes[i].id;
        ret[s]=DOMAIN_IF_REJECTED;
        m=route_message(id,201u,1u,901u);CHECK(feed_can(&m,4u)==0);CHECK(calls[s]==1u);
        r=record(NET_WIRE_CAN,id);CHECK(r!=NULL&&!r->blocked&&r->sequence==1u);
        CHECK(adapter.rejected_frames==i*2u+1u&&adapter.last_c2_status==DOMAIN_IF_REJECTED);
        CHECK(feed_can(&m,4u)==0);CHECK(calls[s]==1u&&adapter.rejected_frames==i*2u+1u); /* same frame: no second call */
        m=route_message(id,201u,2u,902u);CHECK(feed_can(&m,4u)==0);
        CHECK(calls[s]==2u&&!r->blocked&&r->sequence==2u&&adapter.rejected_frames==i*2u+2u);
        ret[s]=DOMAIN_IF_OK;
        m=route_message(id,201u,3u,903u);CHECK(feed_can(&m,4u)==0);
        CHECK(calls[s]==3u&&!r->blocked&&r->sequence==3u&&adapter.rejected_frames==i*2u+2u);
        ret[s]=DOMAIN_IF_QUEUE_FULL; /* another failure still blocks the route */
        m=route_message(id,201u,4u,904u);CHECK(feed_can(&m,4u)==0);CHECK(calls[s]==4u&&r->blocked);
        ret[s]=DOMAIN_IF_OK;
        m=route_message(id,201u,5u,905u);CHECK(feed_can(&m,4u)==0);
        CHECK(calls[s]==4u&&r->blocked&&adapter.rejected_frames==i*2u+2u);
    }
    ret[C_BCM]=DOMAIN_IF_REJECTED; /* REJECTED on a state route blocks it as before */
    m=route_message(0x230u,201u,1u,1u);CHECK(feed_can(&m,4u)==0);CHECK(calls[C_BCM]==1u);
    r=record(NET_WIRE_CAN,0x230u);CHECK(r!=NULL&&r->blocked&&adapter.rejected_frames==4u);
    ret[C_BCM]=DOMAIN_IF_OK;
    m=route_message(0x230u,201u,2u,2u);CHECK(feed_can(&m,4u)==0);CHECK(calls[C_BCM]==1u);
    return 0;
}
/* Provisional status report quality (selected by the DOMAIN board hook): OK
 * for BCM_STATUS, CIS_STATUS and VSS_STATUS, whose wire has no quality field;
 * HOLD for any other id; INVALID_ARGUMENT for a NULL or an id that is not the
 * message's. Bound in the adapter, the three status routes reach C2 with
 * meta.quality OK and their AGE, and WINDOW_FAULT stays held. */
static int status_quality_provisional(void)
{
    static const uint16_t ok_ids[3]={0x230u,0x320u,0x340u};
    static const uint16_t hold_ids[5]={0x330u,0x200u,0x0a0u,0x140u,0x300u};
    NetUpperOwner owner=net_domain_c2_owner(&adapter);NetUpperCanMessage m;NetUpperDelivery d;DataQuality_t q;unsigned i;
    for(i=0u;i<3u;++i){m=route_message(ok_ids[i],401u,1u,1u);q=DATA_QUALITY_INVALID;
        CHECK(net_domain_c2_status_quality_provisional(NULL,ok_ids[i],&m,&q)==NET_OK&&q==DATA_QUALITY_OK);}
    for(i=0u;i<5u;++i){m=route_message(hold_ids[i],401u,1u,1u);q=DATA_QUALITY_INVALID;
        CHECK(net_domain_c2_status_quality_provisional(NULL,hold_ids[i],&m,&q)==NET_HOLD&&q==DATA_QUALITY_INVALID);}
    m=route_message(0x230u,401u,1u,1u);
    CHECK(net_domain_c2_status_quality_provisional(NULL,0x230u,NULL,&q)==NET_INVALID_ARGUMENT);
    CHECK(net_domain_c2_status_quality_provisional(NULL,0x230u,&m,NULL)==NET_INVALID_ARGUMENT);
    CHECK(net_domain_c2_status_quality_provisional(NULL,0x320u,&m,&q)==NET_INVALID_ARGUMENT);
    adapter.config.status_quality=net_domain_c2_status_quality_provisional;
    for(i=0u;i<3u;++i){m=route_message(ok_ids[i],401u,1u,1u);CHECK(feed_can(&m,4u)==0);}
    CHECK(calls[C_BCM]==1u&&calls[C_CIS]==1u&&calls[C_VSS]==1u&&adapter.last_c2_status==DOMAIN_IF_OK);
    adapter.config.window_transport_enabled=true;
    m=route_message(0x330u,401u,1u,1u);d=can_meta(401u,1u);
    CHECK(owner.can(owner.context,&m,&d)==NET_HOLD&&calls[C_WFAULT]==0u&&record(NET_WIRE_CAN,0x330u)==NULL);
    return 0;
}
int main(int argc,char **argv)
{
    int r;
    if(argc!=2||start()!=0)return 2;
    if(strcmp(argv[1],"ingress_sender_reboot")==0)r=ingress_sender_reboot();
    else if(strcmp(argv[1],"ingress_blocked_survives_reboot")==0)r=ingress_blocked_survives_reboot();
    else if(strcmp(argv[1],"ingress_uart_wrap")==0)r=ingress_uart_wrap();
    else if(strcmp(argv[1],"ingress_context_restart")==0)r=ingress_context_restart();
    else if(strcmp(argv[1],"env_hold")==0)r=env_hold();
    else if(strcmp(argv[1],"occupant_hold")==0)r=occupant_hold();
    else if(strcmp(argv[1],"semantic_holds")==0)r=semantic_holds();
    else if(strcmp(argv[1],"ingress_bcm_rejected_frame")==0)r=ingress_bcm_rejected_frame();
    else if(strcmp(argv[1],"status_quality_provisional")==0)r=status_quality_provisional();
    else return 2;
    if(r==0)printf("C2 %s PASS\n",argv[1]);
    return r;
}
