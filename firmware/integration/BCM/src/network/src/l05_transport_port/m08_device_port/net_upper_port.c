#include "net_upper_private.h"
NetStatus net_upper_packet_eligible(const NetUpperPacket *p,uint64_t now)
{
    if(p==NULL)return NET_INVALID_ARGUMENT;
    if(!p->deadline_bound || p->transmit_deadline_ms<p->age_encoded_at_ms ||
       now<p->age_encoded_at_ms)return NET_HOLD;
    if(now>p->transmit_deadline_ms ||
       (p->expires_at_equality && now==p->transmit_deadline_ms))return NET_EXPIRED;
    return NET_OK;
}
NetStatus net_upper_read_clock(NetUpper *s, uint64_t *out)
{
    uint64_t now; NetStatus status=s->config.port.ops->now_ms(s->config.port.context,&now);
    if(status!=NET_OK) return status;
    if(s->clock_seen && now<s->last_now_ms) { net_upper_observe_increment(&s->observation.time_holds); return NET_HOLD; }
    s->clock_seen=true; s->last_now_ms=now; *out=now; return NET_OK;
}
NetStatus net_upper_now(NetUpper *s, uint64_t *out)
{
    NetStatus status;
    if(s==NULL || !s->initialized || out==NULL || net_wire_ranges_overlap(s,sizeof(*s),out,sizeof(*out))) return NET_INVALID_ARGUMENT;
    if(s->in_call) return NET_BUSY;
    s->in_call=true; status=net_upper_read_clock(s,out); s->in_call=false; return status;
}
NetStatus net_upper_wire_status(NetWireStatus status)
{
    switch(status) {
    case NET_WIRE_OK:return NET_OK;
    case NET_WIRE_NEED_MORE:return NET_NO_WORK;
    case NET_WIRE_NO_CAPACITY:return NET_NO_CAPACITY;
    case NET_WIRE_UNSUPPORTED:return NET_UNSUPPORTED;
    case NET_WIRE_MALFORMED:return NET_REJECTED;
    case NET_WIRE_INVALID_ARGUMENT:return NET_INVALID_ARGUMENT;
    default:return NET_HOLD;
    }
}
