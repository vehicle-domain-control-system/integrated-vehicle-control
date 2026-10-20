#if defined(__MINGW32__)
#define __USE_MINGW_ANSI_STDIO 1
#endif
#include "network/net_virtual.h"
#include "network/net_upper_monitor.h"
#include "network/net_ecu_adapter.h"
#include "virtual_ecu.h"
#if NETWORK_UPPER_NODE_PROFILE == 5
#include "virtual_domain_owner.h"
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

/* Test application and external feature owners only. Every transport, codec,
 * queue, duplicate/age policy and adapter executes in the product library. */
static NetUpper service;
static NetHalAdapter hal;
static NetVirtualBackend backend;
static NetPermissionGuard permission;
static NetEcuAdapter ecu_adapter;
static bool permission_enabled;
static uint32_t node, next_sequence=1u;
static const char *node_name;
static NetTxKey tracked[NET_UPPER_TX_CAPACITY];
static bool used[NET_UPPER_TX_CAPACITY], relay_vision, bcm_pending;
static NetWireBcmDoorCommandRaw bcm_command;
static uint32_t owner_can_count,owner_uart_count;
static NetStatus held_can(void *ctx,const NetUpperCanMessage *m,const NetUpperDelivery *d)
{
    (void)ctx;
    printf("{\"event\":\"held_can\",\"node\":\"%s\",\"id\":%u,\"observation_only\":%s,\"execution\":\"HOLD\"}\n",node_name,(unsigned)m->id,d->observation_only?"true":"false");
    return d->observation_only?NET_OK:NET_REJECTED;
}
static NetStatus held_uart(void *ctx,const NetUpperUartDelivery *m,const NetUpperDelivery *d)
{
    (void)ctx;
    printf("{\"event\":\"held_uart\",\"node\":\"%s\",\"type\":%u,\"observation_only\":%s,\"execution\":\"HOLD\"}\n",node_name,(unsigned)m->message.type,d->observation_only?"true":"false");
    return d->observation_only?NET_OK:NET_REJECTED;
}
static void permission_changed(void *context,const NetPermissionView *view)
{
    (void)context;
    printf("{\"event\":\"permission_view\",\"node\":\"%s\",\"has_original\":%s,\"raw_power\":%u,\"power_usable\":%s,\"operation_usable\":%s,\"reason\":%u,\"age\":%llu}\n",
        node_name,view->has_original?"true":"false",(unsigned)view->original.vehicle_power_permission,
        view->power_usable?"true":"false",view->operation_usable?"true":"false",(unsigned)view->reason,(unsigned long long)view->original_age_ms);
}

NetUpper *virtual_service(void) { return &service; }
uint64_t virtual_now(void) { return backend.now_ms; }
static const char *status_name(NetStatus s)
{
    static const char *names[]={"OK","INVALID_ARGUMENT","NO_CAPACITY","NOT_CONFIGURED","UNSUPPORTED","HOLD","NO_WORK","EXPIRED","REJECTED","DUPLICATE","STALE","BUSY"};
    return (unsigned)s<sizeof(names)/sizeof(names[0])?names[s]:"UNKNOWN";
}
static void bytes_json(const uint8_t *bytes,size_t n)
{ size_t i; putchar('"'); for(i=0u;i<n;++i) printf("%02x",(unsigned)bytes[i]); putchar('"'); }
static NetStatus wire_output(void *ctx,const NetUpperPacket *p,uint64_t at)
{
    (void)ctx;
    printf("{\"event\":\"wire\",\"node\":\"%s\",\"at\":%llu,\"link\":%u,\"direction\":%u,",node_name,(unsigned long long)at,(unsigned)p->link,(unsigned)p->direction);
    if(p->link==NET_WIRE_CAN) {
        unsigned flags=(p->can.fdf?1u:0u)|(p->can.brs?2u:0u)|(p->can.extended?4u:0u)|(p->can.rtr?8u:0u)|(p->can.driver_error?16u:0u)|(p->can.esi?32u:0u);
        printf("\"id\":%u,\"dlc\":%u,\"flags\":%u,\"hex\":",(unsigned)p->can.standard_id,(unsigned)p->can.dlc,flags); bytes_json(p->can.bytes,p->can.length);
    } else { printf("\"type\":%u,\"hex\":",(unsigned)p->uart.bytes[3]); bytes_json(p->uart.bytes,p->uart.length); }
    puts("}"); return NET_OK;
}
static void fact_output(void *ctx,NetTxKey key,NetWireLink link,unsigned phase,NetStatus s,uint64_t at)
{
    (void)ctx;
    printf("{\"event\":\"tx_fact\",\"node\":\"%s\",\"phase\":%u,\"at\":%llu,\"link\":%u,\"epoch\":%llu,\"token\":%llu,\"status\":\"%s\"}\n",
        node_name,phase,(unsigned long long)at,(unsigned)link,(unsigned long long)key.epoch,(unsigned long long)key.token,status_name(s));
}
static void remember(NetTxKey key)
{ size_t i; for(i=0u;i<NET_UPPER_TX_CAPACITY;++i) if(!used[i]) { used[i]=true; tracked[i]=key; return; } }
NetStatus virtual_send_can(const NetUpperCanMessage *m)
{
    NetSourceStamp source; NetAgePolicy policy={true,10000u,true}; NetTxKey key; NetStatus s;
    source.observation_id=backend.now_ms+1u; source.observed_at_ms=backend.now_ms; source.original_age_ms=net_upper_can_max_age(m);
    if(m->id==0x0c0u) policy.maximum_age_ms=66535u;
    s=m->id==0x140u?net_ecu_publish_bcm_result(&ecu_adapter,&m->data.bcm_result,&source,&policy,&key):
        net_upper_submit_can(&service,m,&source,&policy,&key);
    if(s==NET_OK) remember(key);
    printf("{\"event\":\"publish_can\",\"node\":\"%s\",\"id\":%u,\"status\":\"%s\"}\n",node_name,(unsigned)m->id,status_name(s)); return s;
}
NetStatus virtual_send_uart(const NetUpperUartMessage *m)
{
    NetSourceStamp source; NetAgePolicy policy={true,10000u,true}; NetTxKey key; NetStatus s;
    source.observation_id=backend.now_ms+1u; source.observed_at_ms=backend.now_ms; source.original_age_ms=net_upper_uart_max_age(m);
    switch(m->type) {
    case 0x01u:s=net_ecu_publish_m_context(&ecu_adapter,&m->data.m_context,&source,&policy,&key);break;
    case 0x10u:s=net_ecu_publish_m_request(&ecu_adapter,&m->data.m_request,&source,&policy,&key);break;
    case 0x11u:s=net_ecu_publish_m_query(&ecu_adapter,&m->data.m_query,&source,&policy,&key);break;
    case 0x60u:s=net_ecu_publish_p_vision_result(&ecu_adapter,&m->data.p_vision_result,&source,&policy,&key);break;
    case 0x61u:s=net_ecu_publish_p_vision_status(&ecu_adapter,&m->data.p_vision_status,&source,&policy,&key);break;
    default:s=net_upper_submit_uart_auto(&service,m,&source,&policy,&key);break;
    }
    if(s==NET_OK) remember(key);
    printf("{\"event\":\"publish_uart\",\"node\":\"%s\",\"type\":%u,\"status\":\"%s\"}\n",node_name,(unsigned)m->type,status_name(s)); return s;
}
static NetStatus owner_can(void *ctx,const NetUpperCanMessage *m,const NetUpperDelivery *d)
{
    (void)ctx; ++owner_can_count;
    printf("{\"event\":\"owner_can\",\"node\":\"%s\",\"id\":%u,\"age\":%llu,\"receipt\":%llu",node_name,(unsigned)m->id,(unsigned long long)d->age_ms,(unsigned long long)d->receipt_id);
    if(m->id==0x100u) {
        printf(",\"command\":%u,\"target\":%u",(unsigned)m->data.bcm_door_command.command_id,(unsigned)m->data.bcm_door_command.door_lock_target);
        if(node==NET_WIRE_NODE_BCM) { bcm_command=m->data.bcm_door_command; bcm_pending=true; }
    } else if(m->id==0x0c0u) printf(",\"rear_state\":%u,\"rear_source\":%u",(unsigned)m->data.vss_warning_state.rear_obstacle_state,(unsigned)m->data.vss_warning_state.rear_obstacle_source_sequence);
    else if(m->id==0x080u) printf(",\"permission\":%u,\"operation\":%u",(unsigned)m->data.domain_permission.vehicle_power_permission,(unsigned)m->data.domain_permission.operation_allowed);
    puts("}"); return NET_OK;
}
static NetStatus owner_uart(void *ctx,const NetUpperUartDelivery *delivery,const NetUpperDelivery *d)
{
    const NetUpperUartMessage *m=&delivery->message; (void)ctx; ++owner_uart_count;
    printf("{\"event\":\"owner_uart\",\"node\":\"%s\",\"type\":%u,\"age\":%llu",node_name,(unsigned)m->type,(unsigned long long)d->age_ms);
    if(m->type==0x60u) { printf(",\"vision\":%u,\"count\":%u",(unsigned)m->data.p_vision_result.vision_sequence,(unsigned)m->data.p_vision_result.occupant_count); relay_vision=node==NET_WIRE_NODE_CIS; }
    if(m->type==0x61u) printf(",\"vision_status\":%u",(unsigned)m->data.p_vision_status.vision_status);
    if(m->type==0x62u) printf(",\"permission\":%u,\"limit\":%u",(unsigned)m->data.p_permission.vehicle_power_permission,(unsigned)m->data.p_permission.use_limit_ms);
    if(m->type==0x20u) printf(",\"request\":%u,\"command\":%u,\"result\":%u,\"confirmation\":%u",(unsigned)m->data.m_result.request_id,(unsigned)m->data.m_result.command_id,(unsigned)m->data.m_result.result,(unsigned)m->data.m_result.result_confirmation);
    if(m->type==0x40u) printf(",\"query\":%u,\"count\":%u,\"query_status\":%u",(unsigned)m->data.m_query_end.query_id,(unsigned)m->data.m_query_end.item_count,(unsigned)m->data.m_query_end.query_status);
    puts("}"); return NET_OK;
}
/* Only the actual product M03 adapter selects these typed callbacks. The
 * small test wrappers format observations; they do not decode or route wire. */
#define CAN_OBSERVER(member,ctype,identifier) \
static NetStatus typed_##member(void *ctx,const ctype *raw,const NetUpperDelivery *d) \
{ NetUpperCanMessage m={0}; m.id=identifier; m.data.member=*raw; return owner_can(ctx,&m,d); }
CAN_OBSERVER(domain_permission,NetWireDomainPermissionRaw,0x080u)
CAN_OBSERVER(bcm_door_command,NetWireBcmDoorCommandRaw,0x100u)
CAN_OBSERVER(vss_warning_state,NetWireVssWarningStateRaw,0x0c0u)
CAN_OBSERVER(vss_event,NetWireVssEventRaw,0x160u)
#undef CAN_OBSERVER
#define UART_OBSERVER(member,ctype,identifier,which_link) \
static NetStatus typed_##member(void *ctx,const ctype *raw,const NetUpperDelivery *d) \
{ NetUpperUartDelivery m={0}; m.message.link=which_link; m.message.type=identifier; m.message.data.member=*raw; return owner_uart(ctx,&m,d); }
UART_OBSERVER(p_vision_result,NetWirePVisionResult,0x60u,NET_WIRE_UART_C)
UART_OBSERVER(p_vision_status,NetWirePVisionStatusRaw,0x61u,NET_WIRE_UART_C)
UART_OBSERVER(p_permission,NetWirePPermission,0x62u,NET_WIRE_UART_C)
UART_OBSERVER(m_result,NetWireMResult,0x20u,NET_WIRE_UART_M)
UART_OBSERVER(m_query_end,NetWireMQueryEndRaw,0x40u,NET_WIRE_UART_M)
UART_OBSERVER(m_cis_rear,NetWireMCisRearRaw,0x36u,NET_WIRE_UART_M)
#undef UART_OBSERVER
static const NetEcuCallbacks typed_callbacks={
    .domain_permission=typed_domain_permission,.bcm_door_command=typed_bcm_door_command,
    .vss_warning_state=typed_vss_warning_state,.vss_event=typed_vss_event,
    .p_vision_result=typed_p_vision_result,.p_vision_status=typed_p_vision_status,.p_permission=typed_p_permission,
    .m_result=typed_m_result,.m_query_end=typed_m_query_end,.m_cis_rear=typed_m_cis_rear
};
static void external_owner_step(void)
{
    size_t i;
    if(permission_enabled) (void)net_permission_step(&permission);
    if(bcm_pending) {
        NetUpperCanMessage m={0}; NetWireBcmResultRaw *r=&m.data.bcm_result;
        m.id=0x140u; r->boot_id=201u; r->tx_sequence=next_sequence++;
        r->command_domain_boot=bcm_command.boot_id; r->command_id=bcm_command.command_id;
        r->command_kind=1u; r->result=5u; r->result_reason=9u;
        r->door_target=bcm_command.door_lock_target;
        /* Explicit Host Owner rejection; no actuator or fabricated DONE. */
        if(virtual_send_can(&m)==NET_OK) bcm_pending=false;
    }
    if(relay_vision) {
        NetTxKey key; NetAgePolicy policy={true,10000u,true}; NetStatus s;
        s=CisNet_PublishOccupant(&service,401u,next_sequence++,&policy,&key);
        printf("{\"event\":\"vision_relay\",\"node\":\"%s\",\"status\":\"%s\"}\n",node_name,status_name(s));
        if(s==NET_OK) { remember(key); relay_vision=false; }
    }
#if NETWORK_UPPER_NODE_PROFILE == 5
    virtual_domain_step(backend.now_ms);
#endif
    for(i=0u;i<NET_UPPER_TX_CAPACITY;++i) if(used[i]) {
        NetUpperTxSnapshot snapshot;
        if(net_upper_tx_snapshot(&service,tracked[i],&snapshot)==NET_OK && snapshot.reference_released &&
           snapshot.admission_known && (!snapshot.accepted || snapshot.outcome!=NET_TX_UNKNOWN)) {
            NetStatus s=net_upper_retire(&service,tracked[i]);
            printf("{\"event\":\"retire\",\"node\":\"%s\",\"token\":%llu,\"outcome\":%u,\"status\":\"%s\"}\n",node_name,(unsigned long long)tracked[i].token,(unsigned)snapshot.outcome,status_name(s));
            if(s==NET_OK) used[i]=false;
        }
    }
}
static NetStatus step(void)
{
    NetStatus first=net_upper_step(&service,NET_UPPER_RX_CAPACITY);
    external_owner_step(); (void)net_upper_step(&service,NET_UPPER_RX_CAPACITY); return first;
}
static size_t from_hex(const char *hex,uint8_t *out,size_t capacity)
{
    size_t n=strlen(hex),i;
    if(n%2u!=0u || n/2u>capacity) return SIZE_MAX;
    for(i=0u;i<n/2u;++i) {
        unsigned j,value=0u;
        for(j=0u;j<2u;++j) {
            char c=hex[2u*i+j]; unsigned digit;
            if(c>='0' && c<='9') digit=(unsigned)(c-'0');
            else if(c>='a' && c<='f') digit=(unsigned)(c-'a')+10u;
            else if(c>='A' && c<='F') digit=(unsigned)(c-'A')+10u;
            else return SIZE_MAX;
            value=value*16u+digit;
        }
        out[i]=(uint8_t)value;
    }
    return n/2u;
}
static bool number(const char *text,uint64_t *out)
{
    char *end; unsigned long long value;
    if(text==NULL || *text<'0' || *text>'9') return false;
    errno=0; value=strtoull(text,&end,10);
    if(errno==ERANGE || *end!=0) return false;
    *out=(uint64_t)value; return true;
}
static NetStatus publish_command(const char *line)
{
    unsigned a=0u,b=0u,c=0u,d=0u; NetUpperUartMessage m={0};
    if(strcmp(line,"window_state")==0) {
        NetUpperCanMessage state={0}; NetSourceStamp source={1u,0u,0u}; NetAgePolicy policy={true,10000u,true}; NetTxKey key; NetStatus s;
        state.id=0x0b0u; state.data.window_state.boot_id=501u; state.data.window_state.tx_sequence=next_sequence++;
        state.data.window_state.update_sequence=next_sequence++; state.data.window_state.window_position=25u;
        source.observed_at_ms=backend.now_ms;
        s=net_upper_submit_inspection_can(&service,&state,&source,&policy,&key);
        if(s==NET_OK) remember(key);
        return s;
    }
    if(sscanf(line,"rear %u %u %u %u",&a,&b,&c,&d)==4) {
        NetWireCisRearRaw rear={0}; NetSourceStamp source={1u,0u,0u}; NetAgePolicy policy={true,10000u,true}; NetTxKey key; NetStatus s;
        if(a>UINT16_MAX || c>UINT8_MAX || d>UINT16_MAX) return NET_INVALID_ARGUMENT;
        rear.boot_id=401u; rear.tx_sequence=b; rear.update_sequence=b; rear.rear_distance_deci_cm=(uint16_t)a;
        rear.validity=(uint8_t)c; rear.source_age_ms=(uint16_t)d; source.observed_at_ms=backend.now_ms; source.original_age_ms=d;
        s=CisNet_PublishRear(&service,&rear,&source,&policy,&key); if(s==NET_OK) remember(key); return s;
    }
    if(sscanf(line,"context %u",&a)==1) {
        NetWireMContext *r=&m.data.m_context; m.link=NET_WIRE_UART_M; m.type=0x01u;
        r->device_context_id=7u; r->session_id=a; r->device_registration_state=1u; r->bt_connection_state=1u;
        r->app_active_state=1u; r->app_update_sequence=next_sequence++; r->proximity_state=1u; r->proximity_validity=1u;
        r->proximity_update_sequence=next_sequence++; r->registration_connection_update_sequence=next_sequence++;
        return virtual_send_uart(&m);
    }
    if(sscanf(line,"request %u %u",&a,&b)==2) {
        m.link=NET_WIRE_UART_M; m.type=0x10u; m.data.m_request.device_context_id=7u; m.data.m_request.session_id=11u;
        m.data.m_request.request_id=a; m.data.m_request.kind=1u; m.data.m_request.operation=(uint8_t)b;
        return virtual_send_uart(&m);
    }
    if(sscanf(line,"query %u %u",&a,&b)==2) {
        m.link=NET_WIRE_UART_M; m.type=0x11u; m.data.m_query.device_context_id=7u; m.data.m_query.session_id=11u;
        m.data.m_query.query_id=a; m.data.m_query.query_scope=(uint8_t)b; return virtual_send_uart(&m);
    }
    if(sscanf(line,"vision %u %u",&a,&b)==2) {
        m.link=NET_WIRE_UART_C; m.type=0x60u; m.data.p_vision_result.pi_boot_id=6401u; m.data.p_vision_result.vision_sequence=a;
        m.data.p_vision_result.occupant_presence=b?1u:0u; m.data.p_vision_result.presence_validity=1u;
        m.data.p_vision_result.occupant_count=(uint8_t)b; m.data.p_vision_result.count_validity=1u; m.data.p_vision_result.vision_status=2u;
        return virtual_send_uart(&m);
    }
    if(sscanf(line,"visionstatus %u",&a)==1) {
        m.link=NET_WIRE_UART_C; m.type=0x61u; m.data.p_vision_status.pi_boot_id=6401u;
        m.data.p_vision_status.status_sequence=a; m.data.p_vision_status.vision_status=1u; return virtual_send_uart(&m);
    }
#if NETWORK_UPPER_NODE_PROFILE == 5
    return virtual_domain_command(line);
#else
    return NET_UNSUPPORTED;
#endif
}
static NetStatus command(const char *line)
{
    unsigned a,b,c,d,e; uint64_t at,epoch,key_epoch,token;
    char at_text[32],epoch_text[32],key_text[32],token_text[32],hex[NET_WIRE_STREAM_INPUT_MAX*2u+1u];
    if(sscanf(line,"tick %31s",at_text)==1) { NetStatus s; if(!number(at_text,&at)) return NET_INVALID_ARGUMENT; s=net_virtual_tick(&backend,at); if(s==NET_HOLD) return s; return step(); }
    if(strcmp(line,"step")==0) return step();
    if(strcmp(line,"saturate_tx")==0) {
        NetWireCisRearRaw rear={0}; NetSourceStamp source={1u,0u,0u}; NetAgePolicy policy={true,10000u,true};
        NetTxKey keys[NET_UPPER_TX_CAPACITY],extra={77u,88u}; size_t i; NetStatus s;
        rear.boot_id=401u; rear.validity=1u; rear.rear_distance_deci_cm=123u; source.observed_at_ms=backend.now_ms;
        for(i=0u;i<NET_UPPER_TX_CAPACITY;++i) {
            rear.tx_sequence=100u+(uint32_t)i; rear.update_sequence=rear.tx_sequence;
            s=CisNet_PublishRear(&service,&rear,&source,&policy,&keys[i]); if(s!=NET_OK) return s;
        }
        s=CisNet_PublishRear(&service,&rear,&source,&policy,&extra);
        printf("{\"event\":\"tx_saturation\",\"node\":\"%s\",\"queued\":%u,\"extra_status\":\"%s\",\"output_unchanged\":%s}\n",node_name,(unsigned)NET_UPPER_TX_CAPACITY,status_name(s),(extra.epoch==77u&&extra.token==88u)?"true":"false");
        for(i=0u;i<NET_UPPER_TX_CAPACITY;++i) {
            if(net_upper_cancel(&service,keys[i])!=NET_OK || net_upper_retire(&service,keys[i])!=NET_OK) return NET_HOLD;
        }
        return s;
    }
    if(strcmp(line,"owner_no_data")==0 || strcmp(line,"owner_failed")==0) {
        NetStatus s;
        if(!permission_enabled) return NET_UNSUPPORTED;
        s=net_permission_invalidate(&permission,strcmp(line,"owner_no_data")==0?NET_PERMISSION_OWNER_NO_DATA:NET_PERMISSION_OWNER_FAILED);
        (void)net_permission_step(&permission); return s;
    }
    if(sscanf(line,"clock %31s",at_text)==1) { if(!number(at_text,&at)) return NET_INVALID_ARGUMENT; (void)net_virtual_clock_fault(&backend,at); return step(); }
    if(sscanf(line,"timing %u %u %u %u",&a,&b,&c,&d)==4) { NetVirtualTiming t={a,b,c,d}; return net_virtual_set_timing(&backend,&t); }
    if(sscanf(line,"link %u %u %31s %u",&a,&b,epoch_text,&c)==4) { NetStatus s; if(!number(epoch_text,&epoch)) return NET_INVALID_ARGUMENT; s=net_virtual_link(&backend,(NetUpperEventKind)a,(NetWireLink)b,epoch,c); (void)step(); return s; }
    if(sscanf(line,"canrx %u %u %u %31s %31s %1024s",&a,&b,&c,epoch_text,at_text,hex)==6) {
        NetWireCanEnvelope f={0}; NetStatus s; f.length=from_hex(hex,f.bytes,sizeof(f.bytes)); if(f.length==SIZE_MAX) return NET_INVALID_ARGUMENT;
        if(!number(epoch_text,&epoch) || !number(at_text,&at)) return NET_INVALID_ARGUMENT;
        f.standard_id=a; f.dlc=(uint8_t)b; f.fdf=(c&1u)!=0u; f.brs=(c&2u)!=0u; f.extended=(c&4u)!=0u; f.rtr=(c&8u)!=0u; f.driver_error=(c&16u)!=0u; f.esi=(c&32u)!=0u;
        s=net_virtual_can(&backend,&f,(uint64_t)epoch,(uint64_t)at); if(s!=NET_OK) return s; return step();
    }
    if(sscanf(line,"uartrx %u %31s %31s %1024s",&a,epoch_text,at_text,hex)==4) {
        uint8_t bytes[NET_WIRE_STREAM_INPUT_MAX]; uint64_t times[NET_WIRE_STREAM_INPUT_MAX]; size_t n,i,consumed=0u; NetStatus s;
        n=from_hex(hex,bytes,sizeof(bytes)); if(n==SIZE_MAX) return NET_INVALID_ARGUMENT;
        if(!number(epoch_text,&epoch) || !number(at_text,&at)) return NET_INVALID_ARGUMENT;
        for(i=0u;i<n;++i) times[i]=(uint64_t)at;
        s=net_virtual_uart(&backend,(NetWireLink)a,bytes,times,n,(uint64_t)epoch,&consumed);
        printf("{\"event\":\"uart_capture\",\"node\":\"%s\",\"consumed\":%u,\"requested\":%u,\"status\":\"%s\"}\n",node_name,(unsigned)consumed,(unsigned)n,status_name(s));
        if(s!=NET_OK) { (void)step(); return s; } return step();
    }
    if(sscanf(line,"txfact %u %31s %31s %31s %u %u %u %u",&a,epoch_text,key_text,token_text,&b,&c,&d,&e)==8) {
        NetTxEvent fact={0}; NetStatus s;
        if(!number(epoch_text,&epoch) || !number(key_text,&key_epoch) || !number(token_text,&token)) return NET_INVALID_ARGUMENT;
        fact.key.epoch=(uint64_t)key_epoch; fact.key.token=(uint64_t)token;
        fact.admission_observed=b!=0u; fact.accepted=c!=0u; fact.release_observed=d!=0u; fact.outcome=(NetTxOutcome)e;
        s=net_virtual_tx_fact(&backend,(NetWireLink)a,&fact,(uint64_t)epoch); (void)step(); return s;
    }
    if(strcmp(line,"observe")==0) {
        NetUpperObservation o; NetHalObservation h;
        (void)net_upper_observation(&service,&o); (void)net_hal_adapter_observation(&hal,&h);
        printf("{\"event\":\"observation\",\"node\":\"%s\",\"now\":%llu,\"rx_delivered\":%u,\"rx_invalid\":%u,\"rx_held\":%u,\"duplicates\":%u,\"stale\":%u,\"rx_conflicts\":%u,\"tx_submitted\":%u,\"tx_admitted\":%u,\"tx_succeeded\":%u,\"tx_released\":%u,\"hal_rx_dropped\":%u,\"hal_tx_stale\":%u,\"hal_tx_duplicates\":%u,\"hal_clock_holds\":%u,\"owner_can\":%u,\"owner_uart\":%u}\n",
            node_name,(unsigned long long)backend.now_ms,o.rx_delivered,o.rx_invalid,o.rx_held,o.duplicates,o.stale,o.rx_conflicts,o.tx_submitted,o.tx_admitted,o.tx_succeeded,o.tx_released,h.rx_dropped,h.tx_stale,h.tx_duplicates,h.clock_holds,owner_can_count,owner_uart_count); return NET_OK;
    }
    return publish_command(line);
}
static int initialize(const char *name)
{
    NetUpperConfig c={0}; NetHalConfig h; uint32_t links; size_t i;
#if NETWORK_UPPER_NODE_PROFILE == 1
    node=NET_WIRE_NODE_CIS; node_name="CIS";
#elif NETWORK_UPPER_NODE_PROFILE == 3
    node=NET_WIRE_NODE_BCM; node_name="BCM";
#elif NETWORK_UPPER_NODE_PROFILE == 4
    node=NET_WIRE_NODE_WINDOW; node_name="WINDOW";
#elif NETWORK_UPPER_NODE_PROFILE == 5
    node=NET_WIRE_NODE_DOMAIN; node_name="DOMAIN";
#elif NETWORK_UPPER_NODE_PROFILE == 6
    node=NET_WIRE_NODE_VSS; node_name="VSS";
#else
    if(name!=NULL && strcmp(name,"PI")==0) { node=NET_WIRE_NODE_PI; node_name="PI"; }
    else if(name!=NULL && strcmp(name,"ESP32")==0) { node=NET_WIRE_NODE_ESP32; node_name="ESP32"; }
    else return 2;
#endif
    (void)name; links=NET_HAL_LINK_BIT(NET_WIRE_CAN);
    if(node==NET_WIRE_NODE_CIS || node==NET_WIRE_NODE_PI) links|=NET_HAL_LINK_BIT(NET_WIRE_UART_C);
    if(node==NET_WIRE_NODE_DOMAIN || node==NET_WIRE_NODE_ESP32) links|=NET_HAL_LINK_BIT(NET_WIRE_UART_M);
    if(net_virtual_init(&backend,&hal,links,3u,1000u,wire_output,fact_output,NULL)!=NET_OK) return 3;
    h=net_virtual_hal_config(&backend);
    if(net_hal_adapter_init(&hal,&h,(node==NET_WIRE_NODE_DOMAIN || node==NET_WIRE_NODE_VSS)?NET_HAL_S32K344:NET_HAL_S32K144)!=NET_OK) return 4;
    c.node=node; c.local_epoch=10u+node; c.profile=NET_WIRE_CANDIDATE_ONLY; c.port=net_hal_adapter_port(&hal);
    if(net_ecu_adapter_init(&ecu_adapter,&service,node,&typed_callbacks,NULL)!=NET_OK) return 10;
    c.owner=net_ecu_adapter_owner(&ecu_adapter);
    c.inspection_transport=true;
    for(i=0u;i<3u;++i) c.port_epoch[i]=3u;
    for(i=0u;i<NET_UPPER_ROUTE_COUNT;++i) {
        const NetWireRegistryEntry *r=net_wire_registry_at(i);
        c.rx_policy[i].bound=true; c.rx_policy[i].maximum_age_ms=10000u; c.rx_policy[i].expires_at_equality=true;
        if(r->link==NET_WIRE_CAN && r->id_type==0x080u) c.rx_policy[i].maximum_age_ms=500u;
        if(r->link==NET_WIRE_CAN && r->id_type==0x0c0u) c.rx_per_field_age[i]=true;
    }
    for(i=0u;i<2u;++i) {
        c.uart_policy[i].candidate_time_policy_selected=true; c.uart_policy[i].assembly_limit_ms=100u;
        c.uart_policy[i].expires_at_equality=true; c.uart_sequence_bound[i]=true; c.uart_initial_sequence[i]=1u;
    }
#if NETWORK_UPPER_NODE_PROFILE == 5
    if(virtual_domain_init(&service,&c.owner)!=NET_OK) return 5;
#endif
    c.owner.held_can=held_can; c.owner.held_uart=held_uart;
    if(net_upper_init(&service,&c)!=NET_OK) return 6;
    if(node==NET_WIRE_NODE_CIS || node==NET_WIRE_NODE_BCM || node==NET_WIRE_NODE_VSS || node==NET_WIRE_NODE_WINDOW) {
        NetPermissionPolicy policy={0};
        policy.receive_timeout.bound=true; policy.receive_timeout.maximum_age_ms=150u; policy.receive_timeout.expires_at_equality=true;
        policy.changed=permission_changed; policy.relay_to_pi=node==NET_WIRE_NODE_CIS;
        policy.cis_boot=401u; policy.pi_use_limit_ms=500u;
        policy.relay_age.bound=true; policy.relay_age.maximum_age_ms=500u; policy.relay_age.expires_at_equality=true;
        if(net_permission_init(&permission,&service,&policy)!=NET_OK) return 9;
        permission_enabled=true;
    }
#if NETWORK_UPPER_NODE_PROFILE == 5
    if(virtual_domain_start()!=NET_OK) return 7;
#endif
    printf("{\"event\":\"ready\",\"node\":\"%s\",\"profile\":%u,\"sizeof_upper\":%u,\"admissions\":%u,\"parsers\":%u}\n",node_name,(unsigned)NETWORK_UPPER_NODE_PROFILE,(unsigned)sizeof(service),(unsigned)NET_UPPER_ADMISSION_CAPACITY,(unsigned)NET_UPPER_UART_RECEIVER_CAPACITY);
    return 0;
}
int main(int argc,char **argv)
{
    char line[1400]; int rc=initialize(argc>1?argv[1]:NULL); if(rc!=0) return rc;
    fflush(stdout);
    while(fgets(line,sizeof(line),stdin)!=NULL) {
        size_t n=strlen(line); NetStatus status;
        while(n>0u && (line[n-1u]=='\n' || line[n-1u]=='\r')) line[--n]=0;
        if(strcmp(line,"quit")==0) return 0;
        status=command(line);
        printf("{\"event\":\"done\",\"node\":\"%s\",\"status\":\"%s\"}\n",node_name,status_name(status)); fflush(stdout);
    }
    return ferror(stdin)?8:0;
}
