#include "network/net_ecu_adapter.h"
#include <string.h>
NetStatus net_ecu_adapter_init(NetEcuAdapter *a,NetUpper *s,uint32_t node,const NetEcuCallbacks *ops,void *context)
{
    if(a==NULL || s==NULL || ops==NULL || node==0u || (node&(node-1u))!=0u || node>NET_WIRE_NODE_PI ||
       net_wire_ranges_overlap(a,sizeof(*a),s,sizeof(*s)) || net_wire_ranges_overlap(a,sizeof(*a),ops,sizeof(*ops)))return NET_INVALID_ARGUMENT;
    a->service=s;a->callbacks=ops;a->context=context;a->node=node;a->initialized=true;return NET_OK;
}
static NetStatus receive_can(void *context,const NetUpperCanMessage *m,const NetUpperDelivery *d)
{
    NetEcuAdapter *a=context;const NetWireRegistryEntry *r;
    if(a==NULL || !a->initialized || m==NULL || d==NULL)return NET_INVALID_ARGUMENT;
    r=net_wire_registry_find(NET_WIRE_CAN,m->id);
    if(d->observation_only || r==NULL || (r->receivers&a->node)==0u)return NET_HOLD;
    switch(m->id) {
    case 0x080u:return a->callbacks->domain_permission==NULL?NET_HOLD:a->callbacks->domain_permission(a->context,&m->data.domain_permission,d);
    case 0x100u:return a->callbacks->bcm_door_command==NULL?NET_HOLD:a->callbacks->bcm_door_command(a->context,&m->data.bcm_door_command,d);
    case 0x110u:return a->callbacks->bcm_climate_command==NULL?NET_HOLD:a->callbacks->bcm_climate_command(a->context,&m->data.bcm_climate_command,d);
    case 0x120u:return a->callbacks->bcm_light_command==NULL?NET_HOLD:a->callbacks->bcm_light_command(a->context,&m->data.bcm_light_command,d);
    case 0x090u:return a->callbacks->window_command==NULL?NET_HOLD:a->callbacks->window_command(a->context,&m->data.window_command,d);
    case 0x200u:return a->callbacks->bcm_door_state==NULL?NET_HOLD:a->callbacks->bcm_door_state(a->context,&m->data.bcm_door_state,d);
    case 0x210u:return a->callbacks->bcm_climate_state==NULL?NET_HOLD:a->callbacks->bcm_climate_state(a->context,&m->data.bcm_climate_state,d);
    case 0x220u:return a->callbacks->bcm_light_state==NULL?NET_HOLD:a->callbacks->bcm_light_state(a->context,&m->data.bcm_light_state,d);
    case 0x140u:return a->callbacks->bcm_result==NULL?NET_HOLD:a->callbacks->bcm_result(a->context,&m->data.bcm_result,d);
    case 0x141u:return a->callbacks->bcm_event==NULL?NET_HOLD:a->callbacks->bcm_event(a->context,&m->data.bcm_event,d);
    case 0x230u:return a->callbacks->bcm_status==NULL?NET_HOLD:a->callbacks->bcm_status(a->context,&m->data.bcm_status,d);
    case 0x300u:return a->callbacks->cis_environment==NULL?NET_HOLD:a->callbacks->cis_environment(a->context,&m->data.cis_environment,d);
    case 0x310u:return a->callbacks->cis_occupant==NULL?NET_HOLD:a->callbacks->cis_occupant(a->context,&m->data.cis_occupant,d);
    case 0x0a0u:return a->callbacks->cis_rear==NULL?NET_HOLD:a->callbacks->cis_rear(a->context,&m->data.cis_rear,d);
    case 0x320u:return a->callbacks->cis_status==NULL?NET_HOLD:a->callbacks->cis_status(a->context,&m->data.cis_status,d);
    case 0x0b0u:return a->callbacks->window_state==NULL?NET_HOLD:a->callbacks->window_state(a->context,&m->data.window_state,d);
    case 0x091u:return a->callbacks->window_event==NULL?NET_HOLD:a->callbacks->window_event(a->context,&m->data.window_event,d);
    case 0x092u:return a->callbacks->window_result==NULL?NET_HOLD:a->callbacks->window_result(a->context,&m->data.window_result,d);
    case 0x330u:return a->callbacks->window_fault==NULL?NET_HOLD:a->callbacks->window_fault(a->context,&m->data.window_fault,d);
    case 0x160u:return a->callbacks->vss_event==NULL?NET_HOLD:a->callbacks->vss_event(a->context,&m->data.vss_event,d);
    case 0x0c0u:return a->callbacks->vss_warning_state==NULL?NET_HOLD:a->callbacks->vss_warning_state(a->context,&m->data.vss_warning_state,d);
    case 0x340u:return a->callbacks->vss_status==NULL?NET_HOLD:a->callbacks->vss_status(a->context,&m->data.vss_status,d);
    default:return NET_UNSUPPORTED;
    }
}
static NetStatus receive_uart(void *context,const NetUpperUartDelivery *u,const NetUpperDelivery *d)
{
    NetEcuAdapter *a=context;const NetUpperUartMessage *m;const NetWireRegistryEntry *r;
    if(a==NULL || !a->initialized || u==NULL || d==NULL)return NET_INVALID_ARGUMENT;
    m=&u->message;r=net_wire_registry_find(m->link,m->type);
    if(d->observation_only || r==NULL || (r->receivers&a->node)==0u)return NET_HOLD;
    switch(m->type) {
    case 0x01u:return a->callbacks->m_context==NULL?NET_HOLD:a->callbacks->m_context(a->context,&m->data.m_context,d);
    case 0x10u:return a->callbacks->m_request==NULL?NET_HOLD:a->callbacks->m_request(a->context,&m->data.m_request,d);
    case 0x11u:return a->callbacks->m_query==NULL?NET_HOLD:a->callbacks->m_query(a->context,&m->data.m_query,d);
    case 0x12u:return a->callbacks->m_warning_ack==NULL?NET_HOLD:a->callbacks->m_warning_ack(a->context,&m->data.m_warning_ack,d);
    case 0x02u:return a->callbacks->m_domain_alive==NULL?NET_HOLD:a->callbacks->m_domain_alive(a->context,&m->data.m_domain_alive,d);
    case 0x20u:return a->callbacks->m_result==NULL?NET_HOLD:a->callbacks->m_result(a->context,&m->data.m_result,d);
    case 0x21u:return a->callbacks->m_warning==NULL?NET_HOLD:a->callbacks->m_warning(a->context,&m->data.m_warning,d);
    case 0x22u:return a->callbacks->m_availability==NULL?NET_HOLD:a->callbacks->m_availability(a->context,&m->data.m_availability,d);
    case 0x23u:return a->callbacks->m_digital_status==NULL?NET_HOLD:a->callbacks->m_digital_status(a->context,&m->data.m_digital_status,d);
    case 0x24u:return a->callbacks->m_digital_result==NULL?NET_HOLD:a->callbacks->m_digital_result(a->context,&m->data.m_digital_result,d);
    case 0x25u:return a->callbacks->m_user_settings==NULL?NET_HOLD:a->callbacks->m_user_settings(a->context,&m->data.m_user_settings,d);
    case 0x40u:return a->callbacks->m_query_end==NULL?NET_HOLD:a->callbacks->m_query_end(a->context,&m->data.m_query_end,d);
    case 0x30u:return a->callbacks->m_bcm_door_state==NULL?NET_HOLD:a->callbacks->m_bcm_door_state(a->context,&m->data.m_bcm_door_state,d);
    case 0x31u:return a->callbacks->m_bcm_climate_state==NULL?NET_HOLD:a->callbacks->m_bcm_climate_state(a->context,&m->data.m_bcm_climate_state,d);
    case 0x32u:return a->callbacks->m_bcm_light_state==NULL?NET_HOLD:a->callbacks->m_bcm_light_state(a->context,&m->data.m_bcm_light_state,d);
    case 0x33u:return a->callbacks->m_bcm_status==NULL?NET_HOLD:a->callbacks->m_bcm_status(a->context,&m->data.m_bcm_status,d);
    case 0x34u:return a->callbacks->m_cis_environment==NULL?NET_HOLD:a->callbacks->m_cis_environment(a->context,&m->data.m_cis_environment,d);
    case 0x35u:return a->callbacks->m_cis_occupant==NULL?NET_HOLD:a->callbacks->m_cis_occupant(a->context,&m->data.m_cis_occupant,d);
    case 0x36u:return a->callbacks->m_cis_rear==NULL?NET_HOLD:a->callbacks->m_cis_rear(a->context,&m->data.m_cis_rear,d);
    case 0x37u:return a->callbacks->m_cis_status==NULL?NET_HOLD:a->callbacks->m_cis_status(a->context,&m->data.m_cis_status,d);
    case 0x3au:return a->callbacks->m_vss_status==NULL?NET_HOLD:a->callbacks->m_vss_status(a->context,&m->data.m_vss_status,d);
    case 0x38u:return a->callbacks->m_window_state==NULL?NET_HOLD:a->callbacks->m_window_state(a->context,&m->data.m_window_state,d);
    case 0x39u:return a->callbacks->m_window_fault==NULL?NET_HOLD:a->callbacks->m_window_fault(a->context,&m->data.m_window_fault,d);
    case 0x60u:return a->callbacks->p_vision_result==NULL?NET_HOLD:a->callbacks->p_vision_result(a->context,&m->data.p_vision_result,d);
    case 0x61u:return a->callbacks->p_vision_status==NULL?NET_HOLD:a->callbacks->p_vision_status(a->context,&m->data.p_vision_status,d);
    case 0x62u:return a->callbacks->p_permission==NULL?NET_HOLD:a->callbacks->p_permission(a->context,&m->data.p_permission,d);
    default:return NET_UNSUPPORTED;
    }
}
NetUpperOwner net_ecu_adapter_owner(NetEcuAdapter *a)
{
    NetUpperOwner o={0};if(a!=NULL && a->initialized){o.context=a;o.can=receive_can;o.uart=receive_uart;}return o;
}
static NetStatus valid(NetEcuAdapter *a,const void *m,size_t bytes,NetTxKey *out)
{
    if(a==NULL || !a->initialized || m==NULL || out==NULL ||
       net_wire_ranges_overlap(m,bytes,out,sizeof(*out)) || net_wire_ranges_overlap(a,sizeof(*a),out,sizeof(*out)))return NET_INVALID_ARGUMENT;
    if(!a->service->initialized || a->service->config.node!=a->node)return NET_NOT_CONFIGURED;
    return NET_OK;
}
NetStatus net_ecu_publish_domain_permission(NetEcuAdapter *a,const NetWireDomainPermissionRaw *raw,const NetSourceStamp *source,const NetAgePolicy *policy,NetTxKey *out)
{
    NetUpperCanMessage m;NetStatus st=valid(a,raw,sizeof(*raw),out);
    if(st!=NET_OK)return st;
    memset(&m,0,sizeof(m));
    m.id=0x080u;
    m.data.domain_permission=*raw;
    return net_upper_submit_can(a->service,&m,source,policy,out);
}
NetStatus net_ecu_publish_bcm_door_command(NetEcuAdapter *a,const NetWireBcmDoorCommandRaw *raw,const NetSourceStamp *source,const NetAgePolicy *policy,NetTxKey *out)
{
    NetUpperCanMessage m;NetStatus st=valid(a,raw,sizeof(*raw),out);
    if(st!=NET_OK)return st;
    memset(&m,0,sizeof(m));
    m.id=0x100u;
    m.data.bcm_door_command=*raw;
    return net_upper_submit_can(a->service,&m,source,policy,out);
}
NetStatus net_ecu_publish_bcm_climate_command(NetEcuAdapter *a,const NetWireBcmClimateCommandRaw *raw,const NetSourceStamp *source,const NetAgePolicy *policy,NetTxKey *out)
{
    NetUpperCanMessage m;NetStatus st=valid(a,raw,sizeof(*raw),out);
    if(st!=NET_OK)return st;
    memset(&m,0,sizeof(m));
    m.id=0x110u;
    m.data.bcm_climate_command=*raw;
    return net_upper_submit_can(a->service,&m,source,policy,out);
}
NetStatus net_ecu_publish_bcm_light_command(NetEcuAdapter *a,const NetWireBcmLightCommandRaw *raw,const NetSourceStamp *source,const NetAgePolicy *policy,NetTxKey *out)
{
    NetUpperCanMessage m;NetStatus st=valid(a,raw,sizeof(*raw),out);
    if(st!=NET_OK)return st;
    memset(&m,0,sizeof(m));
    m.id=0x120u;
    m.data.bcm_light_command=*raw;
    return net_upper_submit_can(a->service,&m,source,policy,out);
}
NetStatus net_ecu_publish_window_command(NetEcuAdapter *a,const NetWireWindowCommandRaw *raw,const NetSourceStamp *source,const NetAgePolicy *policy,NetTxKey *out)
{
    NetUpperCanMessage m;NetStatus st=valid(a,raw,sizeof(*raw),out);
    if(st!=NET_OK)return st;
    memset(&m,0,sizeof(m));
    m.id=0x090u;
    m.data.window_command=*raw;
    return net_upper_submit_can(a->service,&m,source,policy,out);
}
NetStatus net_ecu_publish_bcm_door_state(NetEcuAdapter *a,const NetWireBcmDoorStateRaw *raw,const NetSourceStamp *source,const NetAgePolicy *policy,NetTxKey *out)
{
    NetUpperCanMessage m;NetStatus st=valid(a,raw,sizeof(*raw),out);
    if(st!=NET_OK)return st;
    memset(&m,0,sizeof(m));
    m.id=0x200u;
    m.data.bcm_door_state=*raw;
    return net_upper_submit_can(a->service,&m,source,policy,out);
}
NetStatus net_ecu_publish_bcm_climate_state(NetEcuAdapter *a,const NetWireBcmClimateStateRaw *raw,const NetSourceStamp *source,const NetAgePolicy *policy,NetTxKey *out)
{
    NetUpperCanMessage m;NetStatus st=valid(a,raw,sizeof(*raw),out);
    if(st!=NET_OK)return st;
    memset(&m,0,sizeof(m));
    m.id=0x210u;
    m.data.bcm_climate_state=*raw;
    return net_upper_submit_can(a->service,&m,source,policy,out);
}
NetStatus net_ecu_publish_bcm_light_state(NetEcuAdapter *a,const NetWireBcmLightStateRaw *raw,const NetSourceStamp *source,const NetAgePolicy *policy,NetTxKey *out)
{
    NetUpperCanMessage m;NetStatus st=valid(a,raw,sizeof(*raw),out);
    if(st!=NET_OK)return st;
    memset(&m,0,sizeof(m));
    m.id=0x220u;
    m.data.bcm_light_state=*raw;
    return net_upper_submit_can(a->service,&m,source,policy,out);
}
NetStatus net_ecu_publish_bcm_result(NetEcuAdapter *a,const NetWireBcmResultRaw *raw,const NetSourceStamp *source,const NetAgePolicy *policy,NetTxKey *out)
{
    NetUpperCanMessage m;NetStatus st=valid(a,raw,sizeof(*raw),out);
    if(st!=NET_OK)return st;
    memset(&m,0,sizeof(m));
    m.id=0x140u;
    m.data.bcm_result=*raw;
    return net_upper_submit_can(a->service,&m,source,policy,out);
}
NetStatus net_ecu_publish_bcm_event(NetEcuAdapter *a,const NetWireBcmEventRaw *raw,const NetSourceStamp *source,const NetAgePolicy *policy,NetTxKey *out)
{
    NetUpperCanMessage m;NetStatus st=valid(a,raw,sizeof(*raw),out);
    if(st!=NET_OK)return st;
    memset(&m,0,sizeof(m));
    m.id=0x141u;
    m.data.bcm_event=*raw;
    return net_upper_submit_can(a->service,&m,source,policy,out);
}
NetStatus net_ecu_publish_bcm_status(NetEcuAdapter *a,const NetWireBcmStatusRaw *raw,const NetSourceStamp *source,const NetAgePolicy *policy,NetTxKey *out)
{
    NetUpperCanMessage m;NetStatus st=valid(a,raw,sizeof(*raw),out);
    if(st!=NET_OK)return st;
    memset(&m,0,sizeof(m));
    m.id=0x230u;
    m.data.bcm_status=*raw;
    return net_upper_submit_can(a->service,&m,source,policy,out);
}
NetStatus net_ecu_publish_cis_environment(NetEcuAdapter *a,const NetWireCisEnvironmentRaw *raw,const NetSourceStamp *source,const NetAgePolicy *policy,NetTxKey *out)
{
    NetUpperCanMessage m;NetStatus st=valid(a,raw,sizeof(*raw),out);
    if(st!=NET_OK)return st;
    memset(&m,0,sizeof(m));
    m.id=0x300u;
    m.data.cis_environment=*raw;
    return net_upper_submit_can(a->service,&m,source,policy,out);
}
NetStatus net_ecu_publish_cis_occupant(NetEcuAdapter *a,const NetWireCisOccupantRaw *raw,const NetSourceStamp *source,const NetAgePolicy *policy,NetTxKey *out)
{
    NetUpperCanMessage m;NetStatus st=valid(a,raw,sizeof(*raw),out);
    if(st!=NET_OK)return st;
    memset(&m,0,sizeof(m));
    m.id=0x310u;
    m.data.cis_occupant=*raw;
    return net_upper_submit_can(a->service,&m,source,policy,out);
}
NetStatus net_ecu_publish_cis_rear(NetEcuAdapter *a,const NetWireCisRearRaw *raw,const NetSourceStamp *source,const NetAgePolicy *policy,NetTxKey *out)
{
    NetUpperCanMessage m;NetStatus st=valid(a,raw,sizeof(*raw),out);
    if(st!=NET_OK)return st;
    memset(&m,0,sizeof(m));
    m.id=0x0a0u;
    m.data.cis_rear=*raw;
    return net_upper_submit_can(a->service,&m,source,policy,out);
}
NetStatus net_ecu_publish_cis_status(NetEcuAdapter *a,const NetWireCisStatusRaw *raw,const NetSourceStamp *source,const NetAgePolicy *policy,NetTxKey *out)
{
    NetUpperCanMessage m;NetStatus st=valid(a,raw,sizeof(*raw),out);
    if(st!=NET_OK)return st;
    memset(&m,0,sizeof(m));
    m.id=0x320u;
    m.data.cis_status=*raw;
    return net_upper_submit_can(a->service,&m,source,policy,out);
}
NetStatus net_ecu_publish_window_state(NetEcuAdapter *a,const NetWireWindowStateRaw *raw,const NetSourceStamp *source,const NetAgePolicy *policy,NetTxKey *out)
{
    NetUpperCanMessage m;NetStatus st=valid(a,raw,sizeof(*raw),out);
    if(st!=NET_OK)return st;
    memset(&m,0,sizeof(m));
    m.id=0x0b0u;
    m.data.window_state=*raw;
    return net_upper_submit_can(a->service,&m,source,policy,out);
}
NetStatus net_ecu_publish_window_event(NetEcuAdapter *a,const NetWireWindowEventRaw *raw,const NetSourceStamp *source,const NetAgePolicy *policy,NetTxKey *out)
{
    NetUpperCanMessage m;NetStatus st=valid(a,raw,sizeof(*raw),out);
    if(st!=NET_OK)return st;
    memset(&m,0,sizeof(m));
    m.id=0x091u;
    m.data.window_event=*raw;
    return net_upper_submit_can(a->service,&m,source,policy,out);
}
NetStatus net_ecu_publish_window_result(NetEcuAdapter *a,const NetWireWindowResultRaw *raw,const NetSourceStamp *source,const NetAgePolicy *policy,NetTxKey *out)
{
    NetUpperCanMessage m;NetStatus st=valid(a,raw,sizeof(*raw),out);
    if(st!=NET_OK)return st;
    memset(&m,0,sizeof(m));
    m.id=0x092u;
    m.data.window_result=*raw;
    return net_upper_submit_can(a->service,&m,source,policy,out);
}
NetStatus net_ecu_publish_window_fault(NetEcuAdapter *a,const NetWireWindowFaultRaw *raw,const NetSourceStamp *source,const NetAgePolicy *policy,NetTxKey *out)
{
    NetUpperCanMessage m;NetStatus st=valid(a,raw,sizeof(*raw),out);
    if(st!=NET_OK)return st;
    memset(&m,0,sizeof(m));
    m.id=0x330u;
    m.data.window_fault=*raw;
    return net_upper_submit_can(a->service,&m,source,policy,out);
}
NetStatus net_ecu_publish_vss_event(NetEcuAdapter *a,const NetWireVssEventRaw *raw,const NetSourceStamp *source,const NetAgePolicy *policy,NetTxKey *out)
{
    NetUpperCanMessage m;NetStatus st=valid(a,raw,sizeof(*raw),out);
    if(st!=NET_OK)return st;
    memset(&m,0,sizeof(m));
    m.id=0x160u;
    m.data.vss_event=*raw;
    return net_upper_submit_can(a->service,&m,source,policy,out);
}
NetStatus net_ecu_publish_vss_warning_state(NetEcuAdapter *a,const NetWireVssWarningStateRaw *raw,const NetSourceStamp *source,const NetAgePolicy *policy,NetTxKey *out)
{
    NetUpperCanMessage m;NetStatus st=valid(a,raw,sizeof(*raw),out);
    if(st!=NET_OK)return st;
    memset(&m,0,sizeof(m));
    m.id=0x0c0u;
    m.data.vss_warning_state=*raw;
    return net_upper_submit_can(a->service,&m,source,policy,out);
}
NetStatus net_ecu_publish_vss_status(NetEcuAdapter *a,const NetWireVssStatusRaw *raw,const NetSourceStamp *source,const NetAgePolicy *policy,NetTxKey *out)
{
    NetUpperCanMessage m;NetStatus st=valid(a,raw,sizeof(*raw),out);
    if(st!=NET_OK)return st;
    memset(&m,0,sizeof(m));
    m.id=0x340u;
    m.data.vss_status=*raw;
    return net_upper_submit_can(a->service,&m,source,policy,out);
}
NetStatus net_ecu_publish_m_context(NetEcuAdapter *a,const NetWireMContext *raw,const NetSourceStamp *source,const NetAgePolicy *policy,NetTxKey *out)
{
    NetUpperUartMessage m;NetStatus st=valid(a,raw,sizeof(*raw),out);
    if(st!=NET_OK)return st;
    memset(&m,0,sizeof(m));
    m.link=NET_WIRE_UART_M;m.type=0x01u;
    m.data.m_context=*raw;
    return net_upper_submit_uart_auto(a->service,&m,source,policy,out);
}
NetStatus net_ecu_publish_m_request(NetEcuAdapter *a,const NetWireMRequest *raw,const NetSourceStamp *source,const NetAgePolicy *policy,NetTxKey *out)
{
    NetUpperUartMessage m;NetStatus st=valid(a,raw,sizeof(*raw),out);
    if(st!=NET_OK)return st;
    memset(&m,0,sizeof(m));
    m.link=NET_WIRE_UART_M;m.type=0x10u;
    m.data.m_request=*raw;
    return net_upper_submit_uart_auto(a->service,&m,source,policy,out);
}
NetStatus net_ecu_publish_m_query(NetEcuAdapter *a,const NetWireMQueryRaw *raw,const NetSourceStamp *source,const NetAgePolicy *policy,NetTxKey *out)
{
    NetUpperUartMessage m;NetStatus st=valid(a,raw,sizeof(*raw),out);
    if(st!=NET_OK)return st;
    memset(&m,0,sizeof(m));
    m.link=NET_WIRE_UART_M;m.type=0x11u;
    m.data.m_query=*raw;
    return net_upper_submit_uart_auto(a->service,&m,source,policy,out);
}
NetStatus net_ecu_publish_m_warning_ack(NetEcuAdapter *a,const NetWireMWarningAckRaw *raw,const NetSourceStamp *source,const NetAgePolicy *policy,NetTxKey *out)
{
    NetUpperUartMessage m;NetStatus st=valid(a,raw,sizeof(*raw),out);
    if(st!=NET_OK)return st;
    memset(&m,0,sizeof(m));
    m.link=NET_WIRE_UART_M;m.type=0x12u;
    m.data.m_warning_ack=*raw;
    return net_upper_submit_uart_auto(a->service,&m,source,policy,out);
}
NetStatus net_ecu_publish_m_domain_alive(NetEcuAdapter *a,const NetWireMDomainAliveRaw *raw,const NetSourceStamp *source,const NetAgePolicy *policy,NetTxKey *out)
{
    NetUpperUartMessage m;NetStatus st=valid(a,raw,sizeof(*raw),out);
    if(st!=NET_OK)return st;
    memset(&m,0,sizeof(m));
    m.link=NET_WIRE_UART_M;m.type=0x02u;
    m.data.m_domain_alive=*raw;
    return net_upper_submit_uart_auto(a->service,&m,source,policy,out);
}
NetStatus net_ecu_publish_m_result(NetEcuAdapter *a,const NetWireMResult *raw,const NetSourceStamp *source,const NetAgePolicy *policy,NetTxKey *out)
{
    NetUpperUartMessage m;NetStatus st=valid(a,raw,sizeof(*raw),out);
    if(st!=NET_OK)return st;
    memset(&m,0,sizeof(m));
    m.link=NET_WIRE_UART_M;m.type=0x20u;
    m.data.m_result=*raw;
    return net_upper_submit_uart_auto(a->service,&m,source,policy,out);
}
NetStatus net_ecu_publish_m_warning(NetEcuAdapter *a,const NetWireMWarningRaw *raw,const NetSourceStamp *source,const NetAgePolicy *policy,NetTxKey *out)
{
    NetUpperUartMessage m;NetStatus st=valid(a,raw,sizeof(*raw),out);
    if(st!=NET_OK)return st;
    memset(&m,0,sizeof(m));
    m.link=NET_WIRE_UART_M;m.type=0x21u;
    m.data.m_warning=*raw;
    return net_upper_submit_uart_auto(a->service,&m,source,policy,out);
}
NetStatus net_ecu_publish_m_availability(NetEcuAdapter *a,const NetWireMAvailabilityRaw *raw,const NetSourceStamp *source,const NetAgePolicy *policy,NetTxKey *out)
{
    NetUpperUartMessage m;NetStatus st=valid(a,raw,sizeof(*raw),out);
    if(st!=NET_OK)return st;
    memset(&m,0,sizeof(m));
    m.link=NET_WIRE_UART_M;m.type=0x22u;
    m.data.m_availability=*raw;
    return net_upper_submit_uart_auto(a->service,&m,source,policy,out);
}
NetStatus net_ecu_publish_m_digital_status(NetEcuAdapter *a,const NetWireMDigitalStatusRaw *raw,const NetSourceStamp *source,const NetAgePolicy *policy,NetTxKey *out)
{
    NetUpperUartMessage m;NetStatus st=valid(a,raw,sizeof(*raw),out);
    if(st!=NET_OK)return st;
    memset(&m,0,sizeof(m));
    m.link=NET_WIRE_UART_M;m.type=0x23u;
    m.data.m_digital_status=*raw;
    return net_upper_submit_uart_auto(a->service,&m,source,policy,out);
}
NetStatus net_ecu_publish_m_digital_result(NetEcuAdapter *a,const NetWireMDigitalResultRaw *raw,const NetSourceStamp *source,const NetAgePolicy *policy,NetTxKey *out)
{
    NetUpperUartMessage m;NetStatus st=valid(a,raw,sizeof(*raw),out);
    if(st!=NET_OK)return st;
    memset(&m,0,sizeof(m));
    m.link=NET_WIRE_UART_M;m.type=0x24u;
    m.data.m_digital_result=*raw;
    return net_upper_submit_uart_auto(a->service,&m,source,policy,out);
}
NetStatus net_ecu_publish_m_user_settings(NetEcuAdapter *a,const NetWireMUserSettingsRaw *raw,const NetSourceStamp *source,const NetAgePolicy *policy,NetTxKey *out)
{
    NetUpperUartMessage m;NetStatus st=valid(a,raw,sizeof(*raw),out);
    if(st!=NET_OK)return st;
    memset(&m,0,sizeof(m));
    m.link=NET_WIRE_UART_M;m.type=0x25u;
    m.data.m_user_settings=*raw;
    return net_upper_submit_uart_auto(a->service,&m,source,policy,out);
}
NetStatus net_ecu_publish_m_query_end(NetEcuAdapter *a,const NetWireMQueryEndRaw *raw,const NetSourceStamp *source,const NetAgePolicy *policy,NetTxKey *out)
{
    NetUpperUartMessage m;NetStatus st=valid(a,raw,sizeof(*raw),out);
    if(st!=NET_OK)return st;
    memset(&m,0,sizeof(m));
    m.link=NET_WIRE_UART_M;m.type=0x40u;
    m.data.m_query_end=*raw;
    return net_upper_submit_uart_auto(a->service,&m,source,policy,out);
}
NetStatus net_ecu_publish_m_bcm_door_state(NetEcuAdapter *a,const NetWireMBcmDoorStateRaw *raw,const NetSourceStamp *source,const NetAgePolicy *policy,NetTxKey *out)
{
    NetUpperUartMessage m;NetStatus st=valid(a,raw,sizeof(*raw),out);
    if(st!=NET_OK)return st;
    memset(&m,0,sizeof(m));
    m.link=NET_WIRE_UART_M;m.type=0x30u;
    m.data.m_bcm_door_state=*raw;
    return net_upper_submit_uart_auto(a->service,&m,source,policy,out);
}
NetStatus net_ecu_publish_m_bcm_climate_state(NetEcuAdapter *a,const NetWireMBcmClimateStateRaw *raw,const NetSourceStamp *source,const NetAgePolicy *policy,NetTxKey *out)
{
    NetUpperUartMessage m;NetStatus st=valid(a,raw,sizeof(*raw),out);
    if(st!=NET_OK)return st;
    memset(&m,0,sizeof(m));
    m.link=NET_WIRE_UART_M;m.type=0x31u;
    m.data.m_bcm_climate_state=*raw;
    return net_upper_submit_uart_auto(a->service,&m,source,policy,out);
}
NetStatus net_ecu_publish_m_bcm_light_state(NetEcuAdapter *a,const NetWireMBcmLightStateRaw *raw,const NetSourceStamp *source,const NetAgePolicy *policy,NetTxKey *out)
{
    NetUpperUartMessage m;NetStatus st=valid(a,raw,sizeof(*raw),out);
    if(st!=NET_OK)return st;
    memset(&m,0,sizeof(m));
    m.link=NET_WIRE_UART_M;m.type=0x32u;
    m.data.m_bcm_light_state=*raw;
    return net_upper_submit_uart_auto(a->service,&m,source,policy,out);
}
NetStatus net_ecu_publish_m_bcm_status(NetEcuAdapter *a,const NetWireMBcmStatusRaw *raw,const NetSourceStamp *source,const NetAgePolicy *policy,NetTxKey *out)
{
    NetUpperUartMessage m;NetStatus st=valid(a,raw,sizeof(*raw),out);
    if(st!=NET_OK)return st;
    memset(&m,0,sizeof(m));
    m.link=NET_WIRE_UART_M;m.type=0x33u;
    m.data.m_bcm_status=*raw;
    return net_upper_submit_uart_auto(a->service,&m,source,policy,out);
}
NetStatus net_ecu_publish_m_cis_environment(NetEcuAdapter *a,const NetWireMCisEnvironmentRaw *raw,const NetSourceStamp *source,const NetAgePolicy *policy,NetTxKey *out)
{
    NetUpperUartMessage m;NetStatus st=valid(a,raw,sizeof(*raw),out);
    if(st!=NET_OK)return st;
    memset(&m,0,sizeof(m));
    m.link=NET_WIRE_UART_M;m.type=0x34u;
    m.data.m_cis_environment=*raw;
    return net_upper_submit_uart_auto(a->service,&m,source,policy,out);
}
NetStatus net_ecu_publish_m_cis_occupant(NetEcuAdapter *a,const NetWireMCisOccupantRaw *raw,const NetSourceStamp *source,const NetAgePolicy *policy,NetTxKey *out)
{
    NetUpperUartMessage m;NetStatus st=valid(a,raw,sizeof(*raw),out);
    if(st!=NET_OK)return st;
    memset(&m,0,sizeof(m));
    m.link=NET_WIRE_UART_M;m.type=0x35u;
    m.data.m_cis_occupant=*raw;
    return net_upper_submit_uart_auto(a->service,&m,source,policy,out);
}
NetStatus net_ecu_publish_m_cis_rear(NetEcuAdapter *a,const NetWireMCisRearRaw *raw,const NetSourceStamp *source,const NetAgePolicy *policy,NetTxKey *out)
{
    NetUpperUartMessage m;NetStatus st=valid(a,raw,sizeof(*raw),out);
    if(st!=NET_OK)return st;
    memset(&m,0,sizeof(m));
    m.link=NET_WIRE_UART_M;m.type=0x36u;
    m.data.m_cis_rear=*raw;
    return net_upper_submit_uart_auto(a->service,&m,source,policy,out);
}
NetStatus net_ecu_publish_m_cis_status(NetEcuAdapter *a,const NetWireMCisStatusRaw *raw,const NetSourceStamp *source,const NetAgePolicy *policy,NetTxKey *out)
{
    NetUpperUartMessage m;NetStatus st=valid(a,raw,sizeof(*raw),out);
    if(st!=NET_OK)return st;
    memset(&m,0,sizeof(m));
    m.link=NET_WIRE_UART_M;m.type=0x37u;
    m.data.m_cis_status=*raw;
    return net_upper_submit_uart_auto(a->service,&m,source,policy,out);
}
NetStatus net_ecu_publish_m_vss_status(NetEcuAdapter *a,const NetWireMVssStatusRaw *raw,const NetSourceStamp *source,const NetAgePolicy *policy,NetTxKey *out)
{
    NetUpperUartMessage m;NetStatus st=valid(a,raw,sizeof(*raw),out);
    if(st!=NET_OK)return st;
    memset(&m,0,sizeof(m));
    m.link=NET_WIRE_UART_M;m.type=0x3au;
    m.data.m_vss_status=*raw;
    return net_upper_submit_uart_auto(a->service,&m,source,policy,out);
}
NetStatus net_ecu_publish_m_window_state(NetEcuAdapter *a,const NetWireMWindowStateRaw *raw,const NetSourceStamp *source,const NetAgePolicy *policy,NetTxKey *out)
{
    NetUpperUartMessage m;NetStatus st=valid(a,raw,sizeof(*raw),out);
    if(st!=NET_OK)return st;
    memset(&m,0,sizeof(m));
    m.link=NET_WIRE_UART_M;m.type=0x38u;
    m.data.m_window_state=*raw;
    return net_upper_submit_uart_auto(a->service,&m,source,policy,out);
}
NetStatus net_ecu_publish_m_window_fault(NetEcuAdapter *a,const NetWireMWindowFaultRaw *raw,const NetSourceStamp *source,const NetAgePolicy *policy,NetTxKey *out)
{
    NetUpperUartMessage m;NetStatus st=valid(a,raw,sizeof(*raw),out);
    if(st!=NET_OK)return st;
    memset(&m,0,sizeof(m));
    m.link=NET_WIRE_UART_M;m.type=0x39u;
    m.data.m_window_fault=*raw;
    return net_upper_submit_uart_auto(a->service,&m,source,policy,out);
}
NetStatus net_ecu_publish_p_vision_result(NetEcuAdapter *a,const NetWirePVisionResult *raw,const NetSourceStamp *source,const NetAgePolicy *policy,NetTxKey *out)
{
    NetUpperUartMessage m;NetStatus st=valid(a,raw,sizeof(*raw),out);
    if(st!=NET_OK)return st;
    memset(&m,0,sizeof(m));
    m.link=NET_WIRE_UART_C;m.type=0x60u;
    m.data.p_vision_result=*raw;
    return net_upper_submit_uart_auto(a->service,&m,source,policy,out);
}
NetStatus net_ecu_publish_p_vision_status(NetEcuAdapter *a,const NetWirePVisionStatusRaw *raw,const NetSourceStamp *source,const NetAgePolicy *policy,NetTxKey *out)
{
    NetUpperUartMessage m;NetStatus st=valid(a,raw,sizeof(*raw),out);
    if(st!=NET_OK)return st;
    memset(&m,0,sizeof(m));
    m.link=NET_WIRE_UART_C;m.type=0x61u;
    m.data.p_vision_status=*raw;
    return net_upper_submit_uart_auto(a->service,&m,source,policy,out);
}
NetStatus net_ecu_publish_p_permission(NetEcuAdapter *a,const NetWirePPermission *raw,const NetSourceStamp *source,const NetAgePolicy *policy,NetTxKey *out)
{
    NetUpperUartMessage m;NetStatus st=valid(a,raw,sizeof(*raw),out);
    if(st!=NET_OK)return st;
    memset(&m,0,sizeof(m));
    m.link=NET_WIRE_UART_C;m.type=0x62u;
    m.data.p_permission=*raw;
    return net_upper_submit_uart_auto(a->service,&m,source,policy,out);
}
