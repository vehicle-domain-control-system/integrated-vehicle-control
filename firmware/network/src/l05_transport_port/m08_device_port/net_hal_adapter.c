#include "network/net_hal_adapter.h"
#include <string.h>

static void inc(uint32_t *v) { if(*v!=UINT32_MAX) ++*v; }
static bool link_valid(NetWireLink link) { return link>=NET_WIRE_CAN && link<=NET_WIRE_UART_C; }
static bool ready(const NetHalAdapter *a) { return a!=NULL && a->initialized; }
static bool overlap(const NetHalAdapter *a, const void *p, size_t n)
{ return p==NULL || net_wire_ranges_overlap(a,sizeof(*a),p,n); }
static uintptr_t lock(NetHalAdapter *a) { return a->config.ops->enter(a->config.context); }
static void unlock(NetHalAdapter *a, uintptr_t k) { a->config.ops->leave(a->config.context,k); }
static bool key_equal(NetTxKey a,NetTxKey b) { return a.epoch==b.epoch && a.token==b.token; }
static bool enabled(const NetHalAdapter *a,NetWireLink link)
{ return (a->config.enabled_links & NET_HAL_LINK_BIT(link))!=0u; }
static NetHalAttempt *find(NetHalAdapter *a,NetTxKey key)
{
    size_t i;
    for(i=0u;i<NET_HAL_TX_CAPACITY;++i)
        if(a->tx[i].active && key_equal(a->tx[i].transfer.key,key)) return &a->tx[i];
    return NULL;
}
static bool can_bounded(const NetWireCanEnvelope *f)
{
    static const uint8_t length[16]={0,1,2,3,4,5,6,7,8,12,16,20,24,32,48,64};
    return f->dlc<16u && f->length<=64u && f->length==length[f->dlc] &&
        f->standard_id<=(f->extended?0x1fffffffu:0x7ffu) &&
        (f->fdf || (f->length<=8u && !f->brs));
}
static bool packet_valid(const NetUpperPacket *p)
{
    NetWireUartPayload payload;
    if(!link_valid(p->link)) return false;
    if(p->link==NET_WIRE_CAN)
        return p->direction==NET_WIRE_CAN_DIRECTION && can_bounded(&p->can) &&
            p->can.fdf && p->can.brs && !p->can.extended && !p->can.rtr && !p->can.driver_error;
    return net_wire_uart_decode(NET_WIRE_CANDIDATE_ONLY,p->link,p->direction,&p->uart,&payload)==NET_WIRE_OK;
}
static void fault(NetHalAdapter *a,NetWireLink link,const NetUpperCapture *capture,bool lost)
{
    NetUpperCapture c=*capture;
    if(lost && c.overflow!=UINT32_MAX) ++c.overflow;
    if(a->error_pending[link]) {
        NetUpperCapture prior=a->error_capture[link];
        prior.errors |= c.errors;
        if(prior.overflow>UINT32_MAX-c.overflow) prior.overflow=UINT32_MAX;
        else prior.overflow+=c.overflow;
        c=prior; /* Preserve the first actual capture and causal position. */
    }
    a->last_fault_order[link]=a->next_order++;
    if(!a->error_pending[link]) a->error_order[link]=a->last_fault_order[link];
    a->error_capture[link]=c; a->error_pending[link]=true; a->faulted[link]=true;
    inc(&a->observation.errors);
}
static void queue_at(NetHalAdapter *a,const NetUpperEvent *e,uint64_t order)
{
    size_t n=a->rx_count;
    while(n>0u && a->rx_order[(a->rx_head+n-1u)%NET_HAL_RX_CAPACITY]>order) {
        a->rx[(a->rx_head+n)%NET_HAL_RX_CAPACITY]=a->rx[(a->rx_head+n-1u)%NET_HAL_RX_CAPACITY];
        a->rx_order[(a->rx_head+n)%NET_HAL_RX_CAPACITY]=a->rx_order[(a->rx_head+n-1u)%NET_HAL_RX_CAPACITY];
        --n;
    }
    a->rx[(a->rx_head+n)%NET_HAL_RX_CAPACITY]=*e;
    a->rx_order[(a->rx_head+n)%NET_HAL_RX_CAPACITY]=order; ++a->rx_count;
    if(a->rx_count>a->observation.rx_max_depth) a->observation.rx_max_depth=(uint32_t)a->rx_count;
}
static NetStatus queue(NetHalAdapter *a,const NetUpperEvent *e)
{
    if(a->rx_count==NET_HAL_RX_CAPACITY) {
        inc(&a->observation.rx_dropped); fault(a,e->link,&e->capture,true); return NET_NO_CAPACITY;
    }
    queue_at(a,e,a->next_order++);
    inc(&a->observation.rx_captured); return NET_OK;
}
NetStatus net_hal_adapter_init(NetHalAdapter *a,const NetHalConfig *c,NetHalPlatform platform)
{
    size_t i;
    if(a==NULL || c==NULL || net_wire_ranges_overlap(a,sizeof(*a),c,sizeof(*c)) ||
       (platform!=NET_HAL_S32K144 && platform!=NET_HAL_S32K344) ||
       c->enabled_links==0u || (c->enabled_links & ~7u)!=0u || c->ops==NULL ||
       net_wire_ranges_overlap(a,sizeof(*a),c->ops,sizeof(*c->ops)) ||
       c->ops->now_ms==NULL || c->ops->enter==NULL || c->ops->leave==NULL || c->ops->submit==NULL)
        return NET_INVALID_ARGUMENT;
    if(!c->binding_selected) return NET_NOT_CONFIGURED;
    for(i=0u;i<3u;++i)
        if((c->enabled_links&(1u<<i))!=0u && c->port_epoch[i]==0u) return NET_INVALID_ARGUMENT;
    memset(a,0,sizeof(*a)); a->config=*c; a->platform=platform; a->initialized=true; a->next_order=1u;
    return NET_OK;
}
static NetStatus port_now(void *context,uint64_t *out)
{
    NetHalAdapter *a=context; uintptr_t k; uint64_t value; NetStatus status;
    if(!ready(a) || overlap(a,out,sizeof(*out))) return NET_INVALID_ARGUMENT;
    k=lock(a); if(a->api_busy) { unlock(a,k); return NET_BUSY; }
    a->api_busy=true; unlock(a,k);
    status=a->config.ops->now_ms(a->config.context,&value);
    k=lock(a);
    if(status==NET_OK && a->clock_seen && value<a->last_now_ms) {
        inc(&a->observation.clock_holds); status=NET_HOLD;
    }
    if(status==NET_OK) { a->clock_seen=true; a->last_now_ms=value; *out=value; }
    a->api_busy=false; unlock(a,k); return status;
}
static NetStatus port_submit(void *context,const NetUpperPacket *packet,NetTxKey key)
{
    NetHalAdapter *a=context; NetHalAttempt *r=NULL; uintptr_t k; size_t i; NetStatus status;
    if(!ready(a) || overlap(a,packet,sizeof(*packet)) || key.epoch==0u || key.token==0u)
        return NET_INVALID_ARGUMENT;
    if(!packet_valid(packet)) return NET_REJECTED;
    k=lock(a);
    if(a->api_busy) { unlock(a,k); return NET_BUSY; }
    if(!enabled(a,packet->link)) { unlock(a,k); return NET_NOT_CONFIGURED; }
    if(a->faulted[packet->link] || a->next_order>=UINT64_MAX-4u) { unlock(a,k); return NET_HOLD; }
    if(find(a,key)!=NULL) { unlock(a,k); return NET_DUPLICATE; }
    for(i=0u;i<NET_HAL_TX_CAPACITY;++i) if(!a->tx[i].active) { r=&a->tx[i]; break; }
    if(r==NULL) { unlock(a,k); return NET_NO_CAPACITY; }
    memset(r,0,sizeof(*r)); r->active=true;
    r->transfer.platform=a->platform; r->transfer.key=key;
    r->transfer.port_epoch=a->config.port_epoch[packet->link]; r->transfer.packet=*packet;
    r->capture.port_epoch=r->transfer.port_epoch;
    a->api_busy=true; unlock(a,k);
    status=a->config.ops->submit(a->config.context,&r->transfer);
    k=lock(a);
    if(status!=NET_OK) {
        if(r->admission_known || r->released || r->outcome!=NET_TX_UNKNOWN || r->held) {
            /* Callback proves the lower non-OK promise was broken. Our copy is
             * retained, and OK reports its borrow, never a successful TX. */
            r->held=true; fault(a,packet->link,&r->capture,false);
            inc(&a->observation.backend_violations); status=NET_OK;
        } else { memset(r,0,sizeof(*r)); inc(&a->observation.tx_rejected); }
    }
    if(status==NET_OK) inc(&a->observation.tx_submitted);
    a->api_busy=false; unlock(a,k); return status;
}
static bool terminal(const NetHalAttempt *r)
{ return r->admission_known && r->released && (!r->accepted || r->outcome!=NET_TX_UNKNOWN); }
static bool pending(const NetHalAttempt *r)
{
    return (r->admission_known && !r->sent_admission) || (r->released && !r->sent_release) ||
        (r->outcome!=NET_TX_UNKNOWN && r->outcome!=r->sent_outcome);
}
static uint64_t first_tx(const NetHalAttempt *r,unsigned *part)
{
    uint64_t order=UINT64_MAX;
    if(!r->active) return order;
    if(r->admission_known && !r->sent_admission) { order=r->fact_order[0]; *part=0u; }
    if(r->outcome!=r->sent_outcome && r->fact_order[1]<order) { order=r->fact_order[1]; *part=1u; }
    if(r->released && !r->sent_release && r->fact_order[2]<order) { order=r->fact_order[2]; *part=2u; }
    if(r->conflict_pending && r->conflict_order<order) { order=r->conflict_order; *part=3u; }
    return order;
}
static void emit_tx(NetHalAttempt *r,unsigned part,NetUpperEvent *e)
{
    memset(e,0,sizeof(*e)); e->kind=NET_UPPER_TX_EVENT;
    e->link=r->transfer.packet.link; e->tx.key=r->transfer.key;
    if(part==3u) { e->capture=r->conflict_capture; e->tx=r->conflict; r->conflict_pending=false; }
    else {
        e->capture=r->fact_capture[part];
        if(part==0u) { e->tx.admission_observed=true; e->tx.accepted=r->accepted; r->sent_admission=true; }
        if(part==1u) { e->tx.outcome=r->outcome; r->sent_outcome=r->outcome; }
        if(part==2u) { e->tx.release_observed=true; r->sent_release=true; }
    }
    if(terminal(r) && !pending(r) && !r->held) memset(r,0,sizeof(*r));
}
static NetStatus port_poll(void *context,NetUpperEvent *out)
{
    NetHalAdapter *a=context; NetUpperEvent e; uintptr_t k; size_t i,index=0u;
    uint64_t order=UINT64_MAX; unsigned kind=0u,part=0u;
    if(!ready(a) || overlap(a,out,sizeof(*out))) return NET_INVALID_ARGUMENT;
    k=lock(a); if(a->api_busy) { unlock(a,k); return NET_BUSY; }
    if(a->rx_count!=0u) { order=a->rx_order[a->rx_head]; kind=1u; }
    for(i=0u;i<3u;++i) if(a->error_pending[i] && a->error_order[i]<order) {
        order=a->error_order[i]; kind=2u; index=i;
    }
    for(i=0u;i<NET_HAL_TX_CAPACITY;++i) {
        unsigned candidate=0u; uint64_t tx_order=first_tx(&a->tx[i],&candidate);
        if(tx_order<order) { order=tx_order; kind=3u; index=i; part=candidate; }
    }
    if(kind==0u) { unlock(a,k); return NET_NO_WORK; }
    if(kind==2u) {
        i=index;
        memset(&e,0,sizeof(e)); e.kind=NET_UPPER_PORT_ERROR; e.link=(NetWireLink)i;
        e.capture=a->error_capture[i]; a->error_pending[i]=false; a->faulted[i]=true;
        *out=e; unlock(a,k); return NET_OK;
    }
    if(kind==3u) { emit_tx(&a->tx[index],part,&e); *out=e; unlock(a,k); return NET_OK; }
    e=a->rx[a->rx_head]; a->rx_head=(a->rx_head+1u)%NET_HAL_RX_CAPACITY; --a->rx_count;
    if(e.kind==NET_UPPER_LINK_RESET && e.capture.port_epoch==a->config.port_epoch[e.link] && a->last_fault_order[e.link]<order)
        a->faulted[e.link]=false;
    *out=e; unlock(a,k); return NET_OK;
}
NetUpperPort net_hal_adapter_port(NetHalAdapter *a)
{
    static const NetUpperPortOps ops={port_now,port_submit,port_poll};
    NetUpperPort p={&ops,a}; return p;
}
static NetStatus capture_check(const NetHalAdapter *a,NetWireLink link,const NetUpperCapture *c)
{
    if(a->next_order>=UINT64_MAX-4u) return NET_HOLD;
    if(!enabled(a,link)) return NET_NOT_CONFIGURED;
    if(c->port_epoch!=a->config.port_epoch[link]) return NET_STALE;
    return NET_OK;
}
NetStatus net_hal_capture_can(NetHalAdapter *a,const NetWireCanEnvelope *frame,const NetUpperCapture *capture)
{
    NetUpperEvent e; NetStatus status; uintptr_t k;
    if(!ready(a) || overlap(a,frame,sizeof(*frame)) || overlap(a,capture,sizeof(*capture))) return NET_INVALID_ARGUMENT;
    if(!can_bounded(frame)) return NET_REJECTED;
    memset(&e,0,sizeof(e)); e.kind=NET_UPPER_CAN_RX; e.link=NET_WIRE_CAN; e.can=*frame; e.capture=*capture;
    k=lock(a); status=capture_check(a,e.link,capture);
    if(status==NET_OK) status=queue(a,&e);
    unlock(a,k); return status;
}
NetStatus net_hal_capture_uart(NetHalAdapter *a,NetWireLink link,uint8_t byte,const NetUpperCapture *capture)
{
    NetUpperEvent e; NetStatus status; uintptr_t k;
    if(!ready(a) || !link_valid(link) || link==NET_WIRE_CAN || overlap(a,capture,sizeof(*capture))) return NET_INVALID_ARGUMENT;
    memset(&e,0,sizeof(e)); e.kind=NET_UPPER_UART_BYTE; e.link=link; e.byte=byte; e.capture=*capture;
    k=lock(a); status=capture_check(a,link,capture);
    if(status==NET_OK) status=queue(a,&e);
    unlock(a,k); return status;
}
NetStatus net_hal_capture_tx(NetHalAdapter *a,NetWireLink link,const NetTxEvent *event,const NetUpperCapture *capture)
{
    NetHalAttempt *r; uintptr_t k; bool changed=false;
    if(!ready(a) || !link_valid(link) || overlap(a,event,sizeof(*event)) || overlap(a,capture,sizeof(*capture)) ||
       event->outcome<NET_TX_UNKNOWN || event->outcome>NET_TX_FAILED ||
       (!event->admission_observed && !event->release_observed && event->outcome==NET_TX_UNKNOWN)) return NET_INVALID_ARGUMENT;
    k=lock(a); r=find(a,event->key);
    if(r==NULL || r->transfer.packet.link!=link || r->transfer.port_epoch!=capture->port_epoch) {
        inc(&a->observation.tx_stale); unlock(a,k); return NET_STALE;
    }
    if(a->next_order>=UINT64_MAX-4u) {
        r->held=true; a->faulted[link]=true; inc(&a->observation.errors); unlock(a,k); return NET_HOLD;
    }
    if(capture->errors!=0u || capture->overflow!=0u) { r->held=true; fault(a,link,capture,false); }
    if((event->admission_observed && r->admission_known && event->accepted!=r->accepted) ||
       (event->outcome!=NET_TX_UNKNOWN && r->outcome!=NET_TX_UNKNOWN && event->outcome!=r->outcome) ||
       (event->admission_observed && !event->accepted && (r->outcome!=NET_TX_UNKNOWN || event->outcome!=NET_TX_UNKNOWN)) ||
       (r->admission_known && !r->accepted && event->outcome!=NET_TX_UNKNOWN)) {
        if(!r->conflict_pending) {
            r->conflict=*event; r->conflict_capture=*capture;
            r->conflict_pending=true; r->conflict_order=a->next_order++;
        }
        r->held=true; inc(&a->observation.tx_conflicts); unlock(a,k); return NET_HOLD;
    }
    if(event->admission_observed && !r->admission_known) {
        r->admission_known=true; r->accepted=event->accepted; changed=true;
        r->fact_capture[0]=*capture; r->fact_order[0]=a->next_order++;
    }
    if(event->outcome!=NET_TX_UNKNOWN && r->outcome==NET_TX_UNKNOWN) {
        r->outcome=event->outcome; changed=true;
        r->fact_capture[1]=*capture; r->fact_order[1]=a->next_order++;
    }
    if(event->release_observed && !r->released) {
        r->released=true; changed=true;
        r->fact_capture[2]=*capture; r->fact_order[2]=a->next_order++;
    }
    if(changed) { r->capture=*capture; inc(&a->observation.tx_facts); }
    else inc(&a->observation.tx_duplicates);
    unlock(a,k);
    if(capture->errors!=0u || capture->overflow!=0u) return NET_HOLD;
    return changed?NET_OK:NET_DUPLICATE;
}
NetStatus net_hal_capture_link(NetHalAdapter *a,NetUpperEventKind kind,NetWireLink link,const NetUpperCapture *capture)
{
    NetUpperEvent e; NetStatus status; uintptr_t k; size_t i;
    if(!ready(a) || !link_valid(link) || overlap(a,capture,sizeof(*capture)) ||
       (kind!=NET_UPPER_LINK_LOSS && kind!=NET_UPPER_LINK_RESET && kind!=NET_UPPER_PORT_ERROR)) return NET_INVALID_ARGUMENT;
    k=lock(a);
    if(!enabled(a,link)) { unlock(a,k); return NET_NOT_CONFIGURED; }
    if(a->next_order>=UINT64_MAX-4u) { a->faulted[link]=true; inc(&a->observation.errors); unlock(a,k); return NET_HOLD; }
    if(kind==NET_UPPER_LINK_RESET) {
        if(capture->port_epoch<=a->config.port_epoch[link]) { unlock(a,k); return NET_STALE; }
        if(a->error_pending[link]) {
            if(a->rx_count>NET_HAL_RX_CAPACITY-2u) {
                inc(&a->observation.rx_dropped); a->faulted[link]=true; unlock(a,k); return NET_NO_CAPACITY;
            }
            /* Preserve old-epoch faults on their original side of the reset.
             * A later fault receives a separate latch/order after this barrier. */
            memset(&e,0,sizeof(e)); e.kind=NET_UPPER_PORT_ERROR; e.link=link;
            e.capture=a->error_capture[link]; queue_at(a,&e,a->error_order[link]);
            a->error_pending[link]=false;
        }
    } else if(capture->port_epoch!=a->config.port_epoch[link]) { unlock(a,k); return NET_STALE; }
    if(kind==NET_UPPER_PORT_ERROR) { fault(a,link,capture,false); unlock(a,k); return NET_OK; }
    memset(&e,0,sizeof(e)); e.kind=kind; e.link=link; e.capture=*capture;
    status=queue(a,&e);
    if(status==NET_OK && kind==NET_UPPER_LINK_RESET) {
        a->config.port_epoch[link]=capture->port_epoch; a->faulted[link]=true;
        for(i=0u;i<NET_HAL_TX_CAPACITY;++i)
            if(a->tx[i].active && a->tx[i].transfer.packet.link==link && !terminal(&a->tx[i])) a->tx[i].held=true;
        inc(&a->observation.resets);
    }
    if(status==NET_OK && kind==NET_UPPER_LINK_LOSS) { a->faulted[link]=true; a->last_fault_order[link]=a->next_order-1u; }
    unlock(a,k); return status;
}
size_t net_hal_adapter_rx_room(NetHalAdapter *a)
{
    uintptr_t k; size_t room;
    if(!ready(a)) return 0u;
    k=lock(a); room=NET_HAL_RX_CAPACITY-a->rx_count; unlock(a,k); return room;
}
NetStatus net_hal_adapter_observation(NetHalAdapter *a,NetHalObservation *out)
{
    uintptr_t k;
    if(!ready(a) || overlap(a,out,sizeof(*out))) return NET_INVALID_ARGUMENT;
    k=lock(a); *out=a->observation; unlock(a,k); return NET_OK;
}
