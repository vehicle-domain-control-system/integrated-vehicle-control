#include "network/net_upper_control.h"
#include "net_upper_private.h"
#include <string.h>

void net_upper_failure_changed(NetUpperTx *record)
{
    if(record!=NULL && record->failure_notice_enabled && record->failure_version<UINT64_MAX)
        ++record->failure_version;
}

NetStatus net_upper_tx_detail(const NetUpper *s, NetTxKey key, NetUpperTxDetail *out)
{
    size_t i;NetUpperTxDetail value;
    if(s==NULL || !s->initialized || out==NULL ||
       net_wire_ranges_overlap(s,sizeof(*s),out,sizeof(*out)))return NET_INVALID_ARGUMENT;
    if(s->in_call)return NET_BUSY;
    for(i=0u;i<NET_UPPER_TX_CAPACITY;++i) {
        const NetUpperTx *r=&s->tx[i];
        if(!r->state.used || r->state.key.epoch!=key.epoch || r->state.key.token!=key.token)continue;
        memset(&value,0,sizeof(value));value.state=r->state;value.link=r->link;
        value.can=r->can;value.uart=r->uart;value.source=r->source;
        value.policy=r->policy;value.packet=r->packet;
        value.failure_notice_enabled=r->failure_notice_enabled;
        value.failure_version=r->failure_version;value.failure_ack_version=r->failure_ack_version;
        *out=value;return NET_OK;
    }
    return NET_STALE;
}

NetStatus net_upper_failure_poll(const NetUpper *s, NetUpperFailureNotice *out)
{
    size_t i;const NetUpperTx *selected=NULL;NetUpperFailureNotice value;NetStatus status;
    if(s==NULL || !s->initialized || out==NULL ||
       net_wire_ranges_overlap(s,sizeof(*s),out,sizeof(*out)))return NET_INVALID_ARGUMENT;
    if(s->in_call)return NET_BUSY;
    for(i=0u;i<NET_UPPER_TX_CAPACITY;++i) {
        const NetUpperTx *r=&s->tx[i];
        if(r->state.used && r->failure_notice_enabled &&
           r->failure_version!=r->failure_ack_version &&
           (selected==NULL || r->state.key.token<selected->state.key.token))selected=r;
    }
    if(selected==NULL)return NET_NO_WORK;
    memset(&value,0,sizeof(value));
    status=net_upper_tx_detail(s,selected->state.key,&value.detail);
    if(status!=NET_OK)return status;
    value.version=selected->failure_version;*out=value;return NET_OK;
}

NetStatus net_upper_failure_ack(NetUpper *s, NetTxKey key, uint64_t version)
{
    NetUpperTx *r;
    if(s==NULL || !s->initialized || version==0u)return NET_INVALID_ARGUMENT;
    if(s->in_call)return NET_BUSY;
    r=net_upper_find_tx(s,key);if(r==NULL)return NET_STALE;
    if(!r->failure_notice_enabled || r->failure_version!=version)return NET_STALE;
    /* Saturation cannot identify a later change; never acknowledge an
     * ambiguous version or permit reclamation on that basis. */
    if(version==UINT64_MAX)return NET_HOLD;
    if(r->failure_ack_version==version)return NET_DUPLICATE;
    r->failure_ack_version=version;return NET_OK;
}
