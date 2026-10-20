#include "network/net_upper_control.h"
#include "net_upper_private.h"
#include <string.h>

NetStatus net_upper_received_uart(const NetUpper *s, NetWireLink link,
    uint8_t type, NetUpperUartReceipt *out)
{
    size_t i; const NetUpperAdmission *a=NULL; NetUpperUartReceipt value;
    if(s==NULL || !s->initialized || out==NULL ||
       link<NET_WIRE_UART_M || link>NET_WIRE_UART_C ||
       net_wire_ranges_overlap(s,sizeof(*s),out,sizeof(*out)))return NET_INVALID_ARGUMENT;
    if(s->in_call)return NET_BUSY;
    for(i=0u;i<NET_UPPER_ROUTE_COUNT;++i) {
        const NetWireRegistryEntry *r=net_wire_registry_at(i);
        if(r->link==link && r->id_type==type){a=net_upper_admission_at(s,i);break;}
    }
    if(a==NULL)return i<NET_UPPER_ROUTE_COUNT?NET_NO_WORK:NET_UNSUPPORTED;
    if(!a->seen)return NET_NO_WORK;
    if(a->delivery.observation_only || s->quiescing[link] || s->link_failed[link] ||
       !a->delivery.capture.timestamp_known ||
       a->delivery.capture.port_epoch!=s->config.port_epoch[link])return NET_HOLD;
    /* This accessor exports currently usable receipt authority, not history.
     * Domain Alive alone is independent of a recipient context. */
    if(link==NET_WIRE_UART_M && type!=0x02u &&
       (!s->context_seen || s->context_port_epoch!=s->config.port_epoch[link]))return NET_HOLD;
    memset(&value,0,sizeof(value));value.message=a->uart;value.frame=a->raw.uart;
    value.delivery=a->delivery;
    value.source.observation_id=a->delivery.receipt_id;
    value.source.observed_at_ms=a->delivery.capture.captured_at_ms;
    value.source.original_age_ms=net_upper_uart_max_age(&a->uart);
    value.has_wire_age=!(link==NET_WIRE_UART_M &&
        (type==0x10u || type==0x11u || type==0x12u || type==0x40u));
    *out=value;return NET_OK;
}

NetStatus CisNet_PublishPiPermission(NetUpper *s, uint32_t cis_boot,
    uint16_t use_limit, const NetAgePolicy *policy, NetTxKey *out)
{
    NetUpperCanMessage original;NetUpperDelivery delivery;NetSourceStamp source;
    NetUpperUartMessage message;const NetWireDomainPermissionRaw *p;NetStatus status;
    if(s==NULL || !s->initialized || policy==NULL || out==NULL ||
       net_wire_ranges_overlap(s,sizeof(*s),policy,sizeof(*policy)) ||
       net_wire_ranges_overlap(s,sizeof(*s),out,sizeof(*out)) ||
       net_wire_ranges_overlap(policy,sizeof(*policy),out,sizeof(*out)))return NET_INVALID_ARGUMENT;
    if(s->in_call)return NET_BUSY;
    if(s->config.node!=NET_WIRE_NODE_CIS)return NET_REJECTED;
    status=net_upper_received_can(s,0x080u,&original,&delivery,&source);
    if(status!=NET_OK)return status;
    if(s->quiescing[NET_WIRE_CAN] || s->link_failed[NET_WIRE_CAN] ||
       delivery.capture.port_epoch!=s->config.port_epoch[NET_WIRE_CAN])return NET_HOLD;
    p=&original.data.domain_permission;
    if(p->power_permission_quality!=0u)return NET_HOLD;
    memset(&message,0,sizeof(message));message.link=NET_WIRE_UART_C;message.type=0x62u;
    message.data.p_permission.cis_boot_id=cis_boot;
    message.data.p_permission.update_sequence=p->update_sequence;
    message.data.p_permission.vehicle_power_permission=p->vehicle_power_permission;
    message.data.p_permission.value_quality=p->power_permission_quality;
    message.data.p_permission.quality_reason=0u;
    message.data.p_permission.use_limit_ms=use_limit;
    message.data.p_permission.source_age_ms=p->source_age_ms;
    return net_upper_submit_uart_auto(s,&message,&source,policy,out);
}
