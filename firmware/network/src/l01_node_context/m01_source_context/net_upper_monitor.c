#include "network/net_upper_monitor.h"
#include <string.h>

static bool expired(uint64_t age,const NetAgePolicy *p)
{ return age>p->maximum_age_ms || (age==p->maximum_age_ms && p->expires_at_equality); }
static bool same_view(const NetPermissionView *a,const NetPermissionView *b)
{
    return a->has_original==b->has_original && a->reason==b->reason &&
        a->power_usable==b->power_usable && a->operation_usable==b->operation_usable &&
        a->alive_usable==b->alive_usable && a->delivery.receipt_id==b->delivery.receipt_id;
}
static void cancel_permissions(NetUpper *s)
{
    size_t i;
    for(i=0u;i<NET_UPPER_TX_CAPACITY;++i) {
        const NetUpperTx *r=&s->tx[i];
        if(r->state.used && r->state.queued &&
           ((r->link==NET_WIRE_CAN && r->can.id==0x080u) ||
            (r->link==NET_WIRE_UART_C && r->uart.type==0x62u)))
            (void)net_upper_cancel(s,r->state.key);
    }
}
NetStatus net_permission_init(NetPermissionGuard *g,NetUpper *s,const NetPermissionPolicy *p)
{
    if(g==NULL || s==NULL || !s->initialized || p==NULL || !p->receive_timeout.bound ||
       p->receive_timeout.maximum_age_ms==0u ||
       net_wire_ranges_overlap(g,sizeof(*g),s,sizeof(*s)) ||
       net_wire_ranges_overlap(g,sizeof(*g),p,sizeof(*p)))return NET_INVALID_ARGUMENT;
    if(s->in_call)return NET_BUSY;
    if(p->relay_to_pi && (s->config.node!=NET_WIRE_NODE_CIS ||
       NETWORK_UPPER_NODE_PROFILE==NET_UPPER_PROFILE_CIS_CAN_ONLY ||
       p->cis_boot==0u || p->pi_use_limit_ms==0u || !p->relay_age.bound ||
       !s->config.uart_sequence_bound[1]))return NET_HOLD;
    memset(g,0,sizeof(*g));g->service=s;g->policy=*p;g->initialized=true;
    return NET_OK;
}
static void publish_view(NetPermissionGuard *g,const NetPermissionView *v)
{
    bool changed=!g->view_seen || !same_view(&g->view,v);
    g->view=*v;g->view_seen=true;
    if(changed && g->policy.changed!=NULL)g->policy.changed(g->policy.context,&g->view);
}
NetStatus net_permission_invalidate(NetPermissionGuard *g,NetPermissionReason reason)
{
    NetPermissionView v;
    if(g==NULL || !g->initialized ||
       (reason!=NET_PERMISSION_OWNER_FAILED && reason!=NET_PERMISSION_OWNER_NO_DATA))return NET_INVALID_ARGUMENT;
    if(g->in_call || g->service->in_call)return NET_BUSY;
    g->in_call=true;g->invalidated=true;g->invalid_reason=reason;
    g->invalid_receipt=g->service->next_receipt-1u;
    cancel_permissions(g->service);
    v=g->view;v.reason=reason;v.power_usable=false;v.operation_usable=false;v.alive_usable=false;
    publish_view(g,&v);g->in_call=false;return NET_OK;
}
static NetStatus relay(NetPermissionGuard *g,const NetPermissionView *v)
{
    NetStatus st;
    if(!g->policy.relay_to_pi)return NET_OK;
    if(g->relay_active) {
        NetUpperTxSnapshot tx;
        st=net_upper_tx_snapshot(g->service,g->relay_key,&tx);
        if(st!=NET_OK)return st;
        if(tx.queued && (!v->has_original || v->delivery.receipt_id!=g->relayed_receipt ||
            (v->reason!=NET_PERMISSION_CURRENT && v->reason!=NET_PERMISSION_RESTRICTED)))
            (void)net_upper_cancel(g->service,g->relay_key);
        st=net_upper_retire(g->service,g->relay_key);
        if(st==NET_OK)g->relay_active=false;
        else return st; /* Immutable borrow/failure-consumer obligation preserved. */
    }
    if(!v->has_original || (v->reason!=NET_PERMISSION_CURRENT && v->reason!=NET_PERMISSION_RESTRICTED) ||
       v->delivery.receipt_id==g->relayed_receipt)return NET_NO_WORK;
    st=CisNet_PublishPiPermission(g->service,g->policy.cis_boot,g->policy.pi_use_limit_ms,
        &g->policy.relay_age,&g->relay_key);
    if(st==NET_OK){g->relay_active=true;g->relayed_receipt=v->delivery.receipt_id;}
    return st;
}
NetStatus net_permission_step(NetPermissionGuard *g)
{
    NetPermissionView v;NetUpperCanMessage m;NetUpperDelivery d;NetSourceStamp source;
    NetStatus st;uint64_t now=0u,age=0u;
    if(g==NULL || !g->initialized)return NET_INVALID_ARGUMENT;
    if(g->in_call || g->service->in_call)return NET_BUSY;
    g->in_call=true;memset(&v,0,sizeof(v));v.reason=NET_PERMISSION_NO_DATA;
    st=net_upper_now(g->service,&now);
    if(st!=NET_OK) {
        v=g->view;v.reason=NET_PERMISSION_CLOCK_HOLD;
        v.power_usable=false;v.operation_usable=false;v.alive_usable=false;
        cancel_permissions(g->service);publish_view(g,&v);g->in_call=false;return st;
    }
    v.evaluated_at_ms=now;
    st=net_upper_received_can(g->service,0x080u,&m,&d,&source);
    if(st==NET_OK) {
        v.has_original=true;v.original=m.data.domain_permission;v.delivery=d;
        if(g->invalidated && d.receipt_id>g->invalid_receipt)g->invalidated=false;
        if(g->invalidated)v.reason=g->invalid_reason;
        else if(now<d.capture.captured_at_ms || net_source_age(&source,now,&age)!=NET_OK)
            v.reason=NET_PERMISSION_CLOCK_HOLD;
        else {
            v.original_age_ms=age;v.receive_silence_ms=now-d.capture.captured_at_ms;
            if(age>=500u)v.reason=NET_PERMISSION_SOURCE_EXPIRED;
            else if(expired(v.receive_silence_ms,&g->policy.receive_timeout))v.reason=NET_PERMISSION_RX_TIMEOUT;
            else if(v.original.power_permission_quality!=0u)v.reason=NET_PERMISSION_BAD_QUALITY;
            else {
                v.power_usable=v.original.vehicle_power_permission==1u;
                v.operation_usable=v.original.operation_allowed==1u && v.original.operation_quality==0u;
                /* This Wire has no independently proven Alive field/AGE.
                 * Power-operation state is not substituted for that fact. */
                v.alive_usable=false;
                v.reason=v.power_usable?NET_PERMISSION_CURRENT:NET_PERMISSION_RESTRICTED;
            }
        }
    } else if(st==NET_HOLD || st==NET_STALE) {
        v=g->view;v.reason=NET_PERMISSION_LINK_UNAVAILABLE;v.evaluated_at_ms=now;
        v.power_usable=false;v.operation_usable=false;v.alive_usable=false;
    } else if(g->invalidated)v.reason=g->invalid_reason;
    if(v.reason!=NET_PERMISSION_CURRENT && v.reason!=NET_PERMISSION_RESTRICTED)cancel_permissions(g->service);
    publish_view(g,&v);st=relay(g,&v);g->in_call=false;
    return st==NET_NO_WORK?NET_OK:st;
}
NetStatus net_permission_snapshot(const NetPermissionGuard *g,NetPermissionView *out)
{
    if(g==NULL || !g->initialized || out==NULL || net_wire_ranges_overlap(g,sizeof(*g),out,sizeof(*out)) ||
       net_wire_ranges_overlap(g->service,sizeof(*g->service),out,sizeof(*out)))return NET_INVALID_ARGUMENT;
    if(g->in_call)return NET_BUSY;
    if(!g->view_seen)return NET_NO_WORK;
    *out=g->view;return NET_OK;
}
NetStatus net_route_watch_init(NetRouteWatch *w,NetWireLink link,uint16_t id,const NetAgePolicy *p)
{
    const NetWireRegistryEntry *r=net_wire_registry_find(link,id);
    if(w==NULL || p==NULL || !p->bound || p->maximum_age_ms==0u || r==NULL ||
       net_wire_ranges_overlap(w,sizeof(*w),p,sizeof(*p)))return NET_INVALID_ARGUMENT;
    /* Explicit periodic allowlist: never infer timeout of commands/results/events. */
    if((link==NET_WIRE_CAN && !(id==0x080u || id==0x0a0u || id==0x200u || id==0x210u ||
        id==0x220u || id==0x230u || id==0x300u || id==0x310u || id==0x320u || id==0x340u)) ||
       (link==NET_WIRE_UART_M && id!=0x02u) ||
       (link==NET_WIRE_UART_C && id!=0x60u && id!=0x61u && id!=0x62u))return NET_HOLD;
    memset(w,0,sizeof(*w));w->initialized=true;w->link=link;w->id_type=id;w->silence=*p;return NET_OK;
}
NetStatus net_route_watch_observe(NetRouteWatch *w,const NetUpperDelivery *d)
{
    if(w==NULL || !w->initialized || d==NULL ||
       net_wire_ranges_overlap(w,sizeof(*w),d,sizeof(*d)))return NET_INVALID_ARGUMENT;
    if(d->observation_only || !d->capture.timestamp_known || d->capture.errors!=0u || d->capture.overflow!=0u ||
       d->capture.port_epoch==0u || d->receipt_id==0u)return NET_HOLD;
    if(w->seen) {
        if(d->capture.port_epoch<w->port_epoch || d->receipt_id<w->receipt_id)return NET_STALE;
        if(d->receipt_id==w->receipt_id && d->source_sequence==w->transport_sequence)return NET_DUPLICATE;
        if(d->capture.port_epoch==w->port_epoch && d->source_boot==w->source_boot &&
           d->source_sequence<w->transport_sequence)return NET_STALE;
        if(d->capture.port_epoch==w->port_epoch && d->capture.captured_at_ms<w->captured_at_ms)return NET_HOLD;
    }
    w->seen=true;w->port_epoch=d->capture.port_epoch;w->captured_at_ms=d->capture.captured_at_ms;
    w->receipt_id=d->receipt_id;w->source_boot=d->source_boot;w->transport_sequence=d->source_sequence;return NET_OK;
}
NetStatus net_route_watch_refresh(NetRouteWatch *w,const NetUpper *s)
{
    NetStatus st;
    if(w==NULL || !w->initialized || s==NULL || !s->initialized ||
       net_wire_ranges_overlap(w,sizeof(*w),s,sizeof(*s)))return NET_INVALID_ARGUMENT;
    if(s->in_call)return NET_BUSY;
    if(w->link==NET_WIRE_CAN) {
        NetUpperCanMessage m;NetUpperDelivery d;NetSourceStamp source;
        st=net_upper_received_can(s,w->id_type,&m,&d,&source);
        return st==NET_OK?net_route_watch_observe(w,&d):st;
    } else {
        NetUpperUartReceipt r;
        st=net_upper_received_uart(s,w->link,(uint8_t)w->id_type,&r);
        return st==NET_OK?net_route_watch_observe(w,&r.delivery):st;
    }
}
NetStatus net_route_watch_read(const NetRouteWatch *w,uint64_t now,uint64_t epoch,bool available,NetRouteHealth *out)
{
    NetRouteHealth v;
    if(w==NULL || !w->initialized || epoch==0u || out==NULL ||
       net_wire_ranges_overlap(w,sizeof(*w),out,sizeof(*out)))return NET_INVALID_ARGUMENT;
    memset(&v,0,sizeof(v));v.has_valid_rx=w->seen;v.port_epoch=w->port_epoch;v.receipt_id=w->receipt_id;
    if(!w->seen)v.status=NET_NO_WORK;
    else if(!available || epoch!=w->port_epoch)v.status=NET_HOLD;
    else if(now<w->captured_at_ms)return NET_HOLD; /* OUT unchanged on time ambiguity. */
    else {v.age_since_capture_ms=now-w->captured_at_ms;v.status=expired(v.age_since_capture_ms,&w->silence)?NET_EXPIRED:NET_OK;v.usable=v.status==NET_OK;}
    *out=v;return NET_OK; /* Snapshot success is not peer liveness. */
}
