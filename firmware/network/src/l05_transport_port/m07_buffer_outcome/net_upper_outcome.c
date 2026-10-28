#include "net_upper_private.h"
#include <string.h>
NetUpperTx *net_upper_find_tx(NetUpper *s, NetTxKey key)
{
    size_t i;
    for(i=0u;i<NET_UPPER_TX_CAPACITY;++i)
        if(s->tx[i].state.used && s->tx[i].state.key.epoch==key.epoch &&
           s->tx[i].state.key.token==key.token) return &s->tx[i];
    return NULL;
}
void net_upper_reject_queued(NetUpperTx *r, NetStatus reason)
{
    r->state.queued=false; r->state.admission_known=true;
    r->state.accepted=false; r->state.reference_released=true;
    r->state.local_status=reason; /* no driver final result was observed */
    net_upper_failure_changed(r);
}
NetStatus net_upper_observe_tx(NetUpper *s, const NetTxEvent *e)
{
    NetUpperTx *r=net_upper_find_tx(s,e->key); bool changed=false;
    if(r==NULL || !r->state.submitted) { net_upper_observe_increment(&s->observation.tx_stale); return NET_STALE; }
    if((uint32_t)e->outcome>(uint32_t)NET_TX_FAILED ||
       (!e->admission_observed && !e->release_observed && e->outcome==NET_TX_UNKNOWN)) return NET_INVALID_ARGUMENT;
    if(r->state.conflict) return NET_HOLD;
    if((e->admission_observed && r->state.admission_known && e->accepted!=r->state.accepted) ||
       (e->outcome!=NET_TX_UNKNOWN && r->state.outcome!=NET_TX_UNKNOWN && e->outcome!=r->state.outcome) ||
       (e->admission_observed && !e->accepted && (r->state.outcome!=NET_TX_UNKNOWN || e->outcome!=NET_TX_UNKNOWN)) ||
       (r->state.admission_known && !r->state.accepted && e->outcome!=NET_TX_UNKNOWN)) {
        r->state.conflict=true;net_upper_failure_changed(r);
        net_upper_observe_increment(&s->observation.tx_conflicts); return NET_HOLD;
    }
    if(e->admission_observed && !r->state.admission_known) {
        r->state.admission_known=true; r->state.accepted=e->accepted; changed=true;
        if(e->accepted) net_upper_observe_increment(&s->observation.tx_admitted);
    }
    if(e->outcome!=NET_TX_UNKNOWN && r->state.outcome==NET_TX_UNKNOWN) {
        r->state.outcome=e->outcome; changed=true;
        net_upper_observe_increment(e->outcome==NET_TX_SUCCEEDED ? &s->observation.tx_succeeded : &s->observation.tx_failed);
    }
    if(e->release_observed && !r->state.reference_released) {
        r->state.reference_released=true; changed=true; net_upper_observe_increment(&s->observation.tx_released);
    }
    if(!changed) { net_upper_observe_increment(&s->observation.tx_duplicates); return NET_DUPLICATE; }
    if((r->state.admission_known && !r->state.accepted) || r->state.outcome==NET_TX_FAILED)
        net_upper_failure_changed(r);
    return NET_OK;
}
NetStatus net_upper_tx_snapshot(const NetUpper *s, NetTxKey key, NetUpperTxSnapshot *out)
{
    size_t i;
    if(s==NULL || !s->initialized || out==NULL || net_wire_ranges_overlap(s,sizeof(*s),out,sizeof(*out))) return NET_INVALID_ARGUMENT;
    for(i=0u;i<NET_UPPER_TX_CAPACITY;++i) {
        const NetUpperTxSnapshot *r=&s->tx[i].state;
        if(r->used && r->key.epoch==key.epoch && r->key.token==key.token) { *out=*r; return NET_OK; }
    }
    return NET_STALE;
}
NetStatus net_upper_retire(NetUpper *s, NetTxKey key)
{
    NetUpperTx *r;
    if(s==NULL || !s->initialized) return NET_INVALID_ARGUMENT;
    if(s->in_call) return NET_BUSY;
    r=net_upper_find_tx(s,key); if(r==NULL) return NET_STALE;
    if(r->state.queued || r->state.conflict || !r->state.admission_known || !r->state.reference_released ||
       (r->state.accepted && r->state.outcome==NET_TX_UNKNOWN)) return NET_HOLD;
    if(r->failure_notice_enabled && r->failure_version!=r->failure_ack_version)return NET_HOLD;
    memset(r,0,sizeof(*r)); return NET_OK;
}
NetStatus net_upper_cancel(NetUpper *s, NetTxKey key)
{
    NetUpperTx *r;
    if(s==NULL || !s->initialized) return NET_INVALID_ARGUMENT;
    if(s->in_call) return NET_BUSY;
    r=net_upper_find_tx(s,key); if(r==NULL) return NET_STALE;
    if(!r->state.queued || r->state.submitted) return NET_HOLD;
    net_upper_reject_queued(r,NET_REJECTED); return NET_OK;
}
