#include "network/net_upper_control.h"
#include <string.h>

NetStatus net_upper_diagnostics(const NetUpper *s, NetUpperDiagnostics *out)
{
    size_t i;NetUpperDiagnostics value;NetStatus status;
    if(s==NULL || !s->initialized || out==NULL ||
       net_wire_ranges_overlap(s,sizeof(*s),out,sizeof(*out)))return NET_INVALID_ARGUMENT;
    if(s->in_call)return NET_BUSY;
    memset(&value,0,sizeof(value));value.observation=s->observation;value.rx_used=s->rx_count;
    for(i=0u;i<3u;++i) {
        status=net_upper_link_snapshot(s,(NetWireLink)i,&value.links[i]);
        if(status!=NET_OK)return status;
        value.tx_used+=value.links[i].tx_used;
    }
    value.context_current=s->context_seen && !s->quiescing[NET_WIRE_UART_M] &&
        !s->link_failed[NET_WIRE_UART_M] &&
        s->context_port_epoch==s->config.port_epoch[NET_WIRE_UART_M];
    if(value.context_current){value.context=s->context;value.context_port_epoch=s->context_port_epoch;}
    for(i=0u;i<2u;++i) {
        value.sequence_bound[i]=s->config.uart_sequence_bound[i];
        value.next_uart_sequence[i]=s->next_uart_sequence[i];
        value.sequence_exhausted[i]=value.sequence_bound[i] && s->next_uart_sequence[i]>UINT16_MAX;
    }
    *out=value;return NET_OK;
}
