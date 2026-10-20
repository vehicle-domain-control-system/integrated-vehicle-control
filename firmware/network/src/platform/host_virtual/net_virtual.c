#include "network/net_virtual.h"
#include <string.h>

static bool ready(const NetVirtualBackend *b) { return b!=NULL && b->initialized; }
static bool valid_link(NetWireLink link) { return link>=NET_WIRE_CAN && link<=NET_WIRE_UART_C; }
static void increment(uint32_t *n) { if(*n!=UINT32_MAX) ++*n; }
static NetUpperCapture capture(const NetVirtualBackend *b, uint64_t epoch, uint64_t at)
{
    NetUpperCapture c={0}; (void)b;
    c.port_epoch=epoch; c.captured_at_ms=at; c.timestamp_known=true; return c;
}
static NetStatus now(void *context,uint64_t *out)
{
    NetVirtualBackend *b=context;
    if(!ready(b) || out==NULL) return NET_INVALID_ARGUMENT;
    *out=b->now_ms; return NET_OK;
}
static uintptr_t enter(void *context)
{
    NetVirtualBackend *b=context; uintptr_t prior=b->critical_depth;
    ++b->critical_depth; return prior;
}
static void leave(void *context,uintptr_t prior)
{
    NetVirtualBackend *b=context;
    if(b->critical_depth==(unsigned)prior+1u) --b->critical_depth;
}
static NetStatus submit(void *context,const NetHalTransfer *transfer)
{
    NetVirtualBackend *b=context; size_t i;
    if(!ready(b) || transfer==NULL || !valid_link(transfer->packet.link)) return NET_INVALID_ARGUMENT;
    if(b->critical_depth!=0u || b->ticking) return NET_BUSY;
    if(!b->connected[transfer->packet.link] || transfer->port_epoch!=b->epoch[transfer->packet.link]) {
        increment(&b->driver_rejections); return NET_HOLD;
    }
    for(i=0u;i<NET_HAL_TX_CAPACITY;++i) if(!b->pending[i].active) {
        NetVirtualPending *p=&b->pending[i]; memset(p,0,sizeof(*p));
        p->active=true; p->borrow=transfer; p->key=transfer->key;
        p->link=transfer->packet.link; p->epoch=transfer->port_epoch;
        p->submitted_at=b->now_ms; increment(&b->submissions); return NET_OK;
    }
    increment(&b->driver_rejections); return NET_NO_CAPACITY;
}
NetStatus net_virtual_init(NetVirtualBackend *b,NetHalAdapter *adapter,uint32_t links,
    uint64_t epoch,uint64_t at,NetVirtualWire wire,NetVirtualFact fact,void *context)
{
    size_t i;
    if(b==NULL || adapter==NULL || wire==NULL || links==0u || (links&~7u)!=0u || epoch==0u ||
       net_wire_ranges_overlap(b,sizeof(*b),adapter,sizeof(*adapter))) return NET_INVALID_ARGUMENT;
    memset(b,0,sizeof(*b)); b->adapter=adapter; b->enabled_links=links; b->now_ms=at;
    b->wire=wire; b->fact=fact; b->context=context;
    b->timing.admission_delay=1u; b->timing.wire_delay=2u;
    b->timing.final_delay=3u; b->timing.release_delay=4u;
    for(i=0u;i<3u;++i) { b->epoch[i]=epoch; b->connected[i]=(links&(1u<<i))!=0u; }
    b->initialized=true; return NET_OK;
}
NetHalConfig net_virtual_hal_config(NetVirtualBackend *b)
{
    static const NetHalBackendOps ops={now,enter,leave,submit};
    NetHalConfig c={0}; size_t i;
    if(!ready(b)) return c;
    c.binding_selected=true; c.enabled_links=b->enabled_links; c.ops=&ops; c.context=b;
    for(i=0u;i<3u;++i) c.port_epoch[i]=b->epoch[i];
    return c;
}
NetStatus net_virtual_set_timing(NetVirtualBackend *b,const NetVirtualTiming *t)
{
    size_t i;
    if(!ready(b) || t==NULL || t->admission_delay>t->wire_delay || t->wire_delay>t->final_delay ||
       t->final_delay>t->release_delay) return NET_INVALID_ARGUMENT;
    for(i=0u;i<NET_HAL_TX_CAPACITY;++i) if(b->pending[i].active) return NET_BUSY;
    b->timing=*t; return NET_OK;
}
NetStatus net_virtual_tx_fact(NetVirtualBackend *b,NetWireLink link,const NetTxEvent *e,uint64_t epoch)
{
    NetUpperCapture c; size_t i;
    if(!ready(b) || !valid_link(link) || e==NULL || e->outcome<NET_TX_UNKNOWN || e->outcome>NET_TX_FAILED ||
       (!e->admission_observed && !e->release_observed && e->outcome==NET_TX_UNKNOWN)) return NET_INVALID_ARGUMENT;
    if(e->release_observed) for(i=0u;i<NET_HAL_TX_CAPACITY;++i) {
        NetVirtualPending *p=&b->pending[i];
        if(p->active && p->link==link && p->epoch==epoch && p->key.epoch==e->key.epoch && p->key.token==e->key.token)
            p->borrow=NULL;
    }
    c=capture(b,epoch,b->now_ms); return net_hal_capture_tx(b->adapter,link,e,&c);
}
NetStatus net_virtual_tick(NetVirtualBackend *b,uint64_t at)
{
    size_t i; bool progressed=false;
    if(!ready(b)) return NET_INVALID_ARGUMENT;
    if(b->ticking || b->critical_depth!=0u) return NET_BUSY;
    if(at<b->now_ms) return NET_HOLD;
    b->now_ms=at; b->ticking=true;
    for(i=0u;i<NET_HAL_TX_CAPACITY;++i) {
        NetVirtualPending *p=&b->pending[i]; NetTxEvent event={0};
        uint64_t delay; NetStatus status=NET_NO_WORK;
        if(!p->active) continue;
        delay=p->phase==0u?b->timing.admission_delay:(p->phase==1u?b->timing.wire_delay:
            (p->phase==2u?b->timing.final_delay:b->timing.release_delay));
        if(at<p->submitted_at || at-p->submitted_at<delay) continue;
        event.key=p->key;
        if(p->phase==0u) { event.admission_observed=true; event.accepted=true;
            status=net_virtual_tx_fact(b,p->link,&event,p->epoch);
        } else if(p->phase==1u) {
            if(p->borrow!=NULL && b->connected[p->link] && p->epoch==b->epoch[p->link]) {
                status=net_upper_packet_eligible(&p->borrow->packet,at);
                if(status==NET_OK) status=b->wire(b->context,&p->borrow->packet,at);
                p->wire_sent=status==NET_OK;
                if(p->wire_sent) increment(&b->wire_frames);
            } else status=NET_HOLD;
        } else if(p->phase==2u) {
            event.outcome=p->wire_sent?NET_TX_SUCCEEDED:NET_TX_FAILED;
            status=net_virtual_tx_fact(b,p->link,&event,p->epoch);
        } else {
            /* No transfer dereference after release: adapter may retire it as
             * soon as the event is polled. Keep only value identity in logs. */
            p->borrow=NULL; event.release_observed=true;
            status=net_virtual_tx_fact(b,p->link,&event,p->epoch);
            increment(&b->releases);
        }
        if(b->fact!=NULL) b->fact(b->context,p->key,p->link,p->phase,status,at);
        ++p->phase; if(p->phase==4u) p->active=false;
        progressed=true;
    }
    b->ticking=false; return progressed?NET_OK:NET_NO_WORK;
}
NetStatus net_virtual_clock_fault(NetVirtualBackend *b,uint64_t at)
{ if(!ready(b) || b->ticking) return NET_INVALID_ARGUMENT; b->now_ms=at; return NET_OK; }
NetStatus net_virtual_link(NetVirtualBackend *b,NetUpperEventKind kind,NetWireLink link,uint64_t epoch,uint32_t errors)
{
    NetUpperCapture c; NetStatus status;
    if(!ready(b) || !valid_link(link)) return NET_INVALID_ARGUMENT;
    c=capture(b,epoch,b->now_ms); c.errors=errors;
    status=net_hal_capture_link(b->adapter,kind,link,&c);
    if(status==NET_OK) {
        if(kind==NET_UPPER_LINK_RESET) { b->epoch[link]=epoch; b->connected[link]=true; }
        else b->connected[link]=false;
    }
    return status;
}
NetStatus net_virtual_can(NetVirtualBackend *b,const NetWireCanEnvelope *f,uint64_t epoch,uint64_t at)
{
    NetUpperCapture c;
    if(!ready(b) || f==NULL) return NET_INVALID_ARGUMENT;
    if(!b->connected[NET_WIRE_CAN]) return NET_HOLD;
    c=capture(b,epoch,at); return net_hal_capture_can(b->adapter,f,&c);
}
NetStatus net_virtual_uart(NetVirtualBackend *b,NetWireLink link,const uint8_t *bytes,
    const uint64_t *times,size_t count,uint64_t epoch,size_t *consumed)
{
    size_t i; NetUpperCapture c; NetStatus status;
    if(!ready(b) || !valid_link(link) || link==NET_WIRE_CAN || bytes==NULL || times==NULL || consumed==NULL ||
       count>NET_WIRE_STREAM_INPUT_MAX || net_wire_ranges_overlap(consumed,sizeof(*consumed),bytes,count) ||
       net_wire_ranges_overlap(consumed,sizeof(*consumed),times,count*sizeof(*times))) return NET_INVALID_ARGUMENT;
    if(!b->connected[link]) { *consumed=0u; return NET_HOLD; }
    for(i=0u;i<count;++i) {
        c=capture(b,epoch,times[i]); status=net_hal_capture_uart(b->adapter,link,bytes[i],&c);
        if(status!=NET_OK) { *consumed=i; return status; }
    }
    *consumed=count; return NET_OK;
}
