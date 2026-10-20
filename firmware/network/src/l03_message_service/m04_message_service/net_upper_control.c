#include "network/net_upper_control.h"
#include "net_upper_private.h"
#include <string.h>

static bool valid_link(NetWireLink link)
{ return link>=NET_WIRE_CAN && link<=NET_WIRE_UART_C; }

NetStatus net_upper_link_snapshot(const NetUpper *s, NetWireLink link,
    NetUpperLinkSnapshot *out)
{
    size_t i;NetUpperLinkSnapshot value;
    if(s==NULL || !s->initialized || out==NULL || !valid_link(link) ||
       net_wire_ranges_overlap(s,sizeof(*s),out,sizeof(*out)))return NET_INVALID_ARGUMENT;
    if(s->in_call)return NET_BUSY;
    if(!net_upper_profile_link(link))return NET_UNSUPPORTED;
    memset(&value,0,sizeof(value));value.quiescing=s->quiescing[link];
    value.failed=s->link_failed[link];value.port_epoch=s->config.port_epoch[link];
    for(i=0u;i<NET_UPPER_TX_CAPACITY;++i) {
        const NetUpperTx *r=&s->tx[i];const NetUpperTxSnapshot *t=&r->state;
        if(!t->used || r->link!=link)continue;
        ++value.tx_used;if(t->queued)++value.queued_tx;
        if(t->submitted && !t->reference_released)++value.borrowed_tx;
        if(t->conflict)++value.conflicts;
        if(t->queued || t->conflict || !t->admission_known || !t->reference_released ||
           (t->accepted && t->outcome==NET_TX_UNKNOWN))++value.unresolved_tx;
        if(r->failure_notice_enabled && r->failure_version!=r->failure_ack_version)
            ++value.pending_failure_notices;
    }
    for(i=0u;i<s->rx_count;++i)
        if(s->rx[(s->rx_head+i)%NET_UPPER_RX_CAPACITY].link==link)++value.pending_rx;
    if(link!=NET_WIRE_CAN && s->uart_enabled[(size_t)link-1u])
        value.partial_uart_bytes=net_upper_receiver(s,(size_t)link-1u)->history_count;
    value.can_resume=value.quiescing && value.queued_tx==0u && value.unresolved_tx==0u &&
        value.pending_failure_notices==0u && value.pending_rx==0u && value.partial_uart_bytes==0u;
    *out=value;return NET_OK;
}

NetStatus net_upper_quiesce_link(NetUpper *s, NetWireLink link)
{
    size_t i;
    if(s==NULL || !s->initialized || !valid_link(link))return NET_INVALID_ARGUMENT;
    if(s->in_call)return NET_BUSY;
    if(!net_upper_profile_link(link))return NET_UNSUPPORTED;
    if(s->quiescing[link])return NET_OK;
    if(link!=NET_WIRE_CAN && s->uart_enabled[(size_t)link-1u]) {
        NetWireStatus status;
        status=net_upper_uart_receiver_signal(net_upper_receiver_mut(s,(size_t)link-1u),s->config.port_epoch[link],false,false);
        if(status!=NET_WIRE_OK)return net_upper_wire_status(status);
    }
    s->in_call=true;s->quiescing[link]=true;
    for(i=0u;i<NET_UPPER_TX_CAPACITY;++i) {
        NetUpperTx *r=&s->tx[i];
        if(r->state.used && r->link==link && r->state.queued)net_upper_reject_queued(r,NET_HOLD);
    }
    if(link==NET_WIRE_UART_M)s->context_seen=false;
    s->in_call=false;return NET_OK;
}

NetStatus net_upper_resume_link(NetUpper *s, NetWireLink link, uint64_t epoch)
{
    NetUpperLinkSnapshot state;NetStatus status;
    if(s==NULL || !s->initialized || !valid_link(link) || epoch==0u)return NET_INVALID_ARGUMENT;
    if(s->in_call)return NET_BUSY;
    if(!net_upper_profile_link(link))return NET_UNSUPPORTED;
    status=net_upper_link_snapshot(s,link,&state);if(status!=NET_OK)return status;
    if(!state.can_resume)return NET_HOLD;
    if(epoch<=state.port_epoch)return NET_STALE;
    if(link!=NET_WIRE_CAN && s->uart_enabled[(size_t)link-1u]) {
        NetWireStatus wire;
        wire=net_upper_uart_receiver_signal(net_upper_receiver_mut(s,(size_t)link-1u),epoch,false,true);
        if(wire!=NET_WIRE_OK)return net_upper_wire_status(wire);
    }
    s->in_call=true;
    s->config.port_epoch[link]=epoch;s->quiescing[link]=false;s->link_failed[link]=false;
    if(link==NET_WIRE_UART_M)s->context_seen=false;
    s->in_call=false;return NET_OK;
}
