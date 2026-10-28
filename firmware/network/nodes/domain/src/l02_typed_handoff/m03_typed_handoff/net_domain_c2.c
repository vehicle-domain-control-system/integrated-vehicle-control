#include "net_domain_private.h"
static NetDomainC2 *bound;
static const unsigned char init_tag[8]={0x4eu,0x43u,0x32u,0x41u,0x44u,0x50u,0x54u,0x31u};
/* Ingress journal, one record per (link, ID), checked after the service has
 * admitted the frame and before the C2 call. The service owns the transport
 * order; the journal keeps one C2 failure from replaying Core and follows the
 * service rules so that it never refuses what the service admitted:
 *   - a blocked record stays blocked under any BOOT or context; only
 *     net_domain_c2_accept_ingress_epoch releases it;
 *   - an older port epoch is HOLD;
 *   - CAN: another sender BOOT is the new generation the service accepted
 *     (docs/network/interface/04_COMMON/10_DATA_AND_ID_RULES.md, communication
 *     start and source generation: an accepted start change invalidates the
 *     prior context of that source). The record takes that BOOT and
 *     TX_SEQUENCE as its new baseline; no port epoch change is needed. With
 *     cis_transition_barriers, 0x300 and 0x310 reach this point only through
 *     their transition barrier;
 *   - UART-M: LINK_SEQUENCE runs modulo 65536
 *     (docs/network/interface/03_COMMUNICATION/20_UART_M.md shared framing). As
 *     in the service window, a number 1..2^15-1 ahead is newer, the same
 *     number is a duplicate, anything else is stale;
 *   - fresh: a new M_CONTEXT, which the service takes without its window. */
static NetStatus ingress(NetDomainC2 *a,NetWireLink link,uint16_t id,uint32_t boot,uint64_t epoch,uint64_t sequence,bool fresh)
{
    size_t i;
NetDomainC2IngressRecord *slot=NULL;
    for(i=0u;i<19u;++i){NetDomainC2IngressRecord *r=&a->ingress[i];
        if(r->used&&r->link==link&&r->id==id){
            if(r->blocked)return NET_HOLD;
            if(r->epoch>epoch)return NET_HOLD;
            if(r->boot!=boot){if(link!=NET_WIRE_CAN)return NET_HOLD;fresh=true;}
            if(!fresh&&r->sequence!=UINT64_MAX){
                if(sequence==r->sequence)return NET_DUPLICATE;
                if(link==NET_WIRE_CAN){if(sequence<r->sequence)return NET_STALE;}
                else if((uint16_t)((uint16_t)sequence-(uint16_t)r->sequence)>=0x8000u)return NET_STALE;
            }
            slot=r;
break;
        }
        if(!r->used&&slot==NULL)slot=r;
    }
    if(slot==NULL)return NET_NO_CAPACITY;
    slot->used=true;
slot->link=link;
slot->id=id;
slot->boot=boot;
slot->epoch=epoch;
slot->sequence=sequence;
a->current_ingress=slot;
    return NET_OK;
}
NetStatus nd_ingress_once(NetDomainC2 *a,NetWireLink link,uint16_t id,uint32_t boot,uint64_t epoch,uint64_t sequence){
return ingress(a,link,id,boot,epoch,sequence,false);
}
NetStatus nd_ingress_new_context(NetDomainC2 *a,uint64_t epoch,uint64_t sequence){
return ingress(a,NET_WIRE_UART_M,0x01u,0u,epoch,sequence,true);
}
/* The service ends the window of every UART-M route when it takes a new
 * M_CONTEXT (its context acceptance), so the next frame of each route is a
 * new baseline there; the journal follows once the Owner took that context.
 * Blocked records stay blocked. */
void nd_ingress_context_taken(NetDomainC2 *a){size_t i;
for(i=0u;i<19u;++i){NetDomainC2IngressRecord *r=&a->ingress[i];
if(r->used&&r->link==NET_WIRE_UART_M&&r->id!=0x01u&&!r->blocked)r->sequence=UINT64_MAX;
}
}
/* CIS transition barriers of 0x300 (environment, CIS BOOT_ID) and 0x310
 * (Occupant, PI_BOOT_ID), cis_transition_barriers
 * (docs/network/interface/02_NODES/DOMAIN/25_PRIVATE_ROUTING.md, CIS
 * environment BOOT change; 07_CROSS_NODE_INTERFACE_RULES.md, environment BOOT
 * change; 04_COMMON/10_DATA_AND_ID_RULES.md, environment values and their
 * invalidation): a new generation is held before the ingress journal and the
 * C2 call, the DOMAIN private transition is called once from the adapter step,
 * and only its confirmed completion lets that generation's frames through. A
 * failed or unclear transition holds that route for the rest of the run (no
 * retry, no recovery handshake); every other route is unaffected. */
static NetDomainC2TransitionApply barrier_apply(const NetDomainC2 *a,NetDomainC2BarrierKind kind){
return kind==NET_DOMAIN_C2_ENVIRONMENT_BARRIER?a->config.transition.environment:a->config.transition.occupant;
}
static bool barrier_left(const NetDomainC2Barrier *b,uint32_t id){size_t i;
for(i=0u;i<NET_DOMAIN_C2_BARRIER_LEFT_IDS;++i)if(b->left[i]==id)return true;
return false;
}
/* Remembers the last four generations the barrier left (open or pending). */
static void barrier_leave(NetDomainC2Barrier *b,uint32_t id){size_t i;
if(id==0u||barrier_left(b,id))return;
for(i=1u;i<NET_DOMAIN_C2_BARRIER_LEFT_IDS;++i)b->left[i-1u]=b->left[i];
b->left[NET_DOMAIN_C2_BARRIER_LEFT_IDS-1u]=id;
}
/* RX callback side, in the service callback: never calls DOMAIN.
 *   - open for this nonzero generation: pass;
 *   - otherwise hold (the frame is dropped and counted). A nonzero generation
 *     that is not the pending one and not one the barrier left makes it
 *     PENDING(id); the generation it was open or pending for is left. PI_BOOT_ID
 *     0 never passes and never starts a transition. A late frame of a left
 *     generation stays held: no transition goes back.
 *   - BLOCKED / DISABLED: hold only. Without a bound transition nothing opens
 *     the barrier and the route stays held. */
NetStatus nd_barrier(NetDomainC2 *a,NetDomainC2BarrierKind kind,uint32_t id){NetDomainC2Barrier *b;
if(!a->config.cis_transition_barriers)return NET_OK;
b=&a->barrier[kind];
if(b->open&&id!=0u&&b->id==id)return NET_OK;
net_upper_observe_increment(&b->held);
if(barrier_apply(a,kind)==NULL||id==0u||barrier_left(b,id))return NET_HOLD;
if(b->state==NET_DOMAIN_C2_BARRIER_BLOCKED||b->state==NET_DOMAIN_C2_BARRIER_DISABLED)return NET_HOLD;
if(b->state==NET_DOMAIN_C2_BARRIER_PENDING&&b->id==id)return NET_HOLD;
if(b->state!=NET_DOMAIN_C2_BARRIER_CLOSED)barrier_leave(b,b->id);
b->open=false;
b->state=NET_DOMAIN_C2_BARRIER_PENDING;
b->id=id;
return NET_HOLD;
}
static void barrier_latch(NetDomainC2Barrier *b,NetDomainC2BarrierState state,NetDomainC2BarrierReason reason){
b->open=false;
b->state=state;
b->reason=reason;
}
/* The route's journal record takes the new generation as its baseline: the
 * environment record its CIS BOOT_ID, the Occupant record (keyed by the CIS
 * BOOT_ID of its CAN frames) a fresh order baseline. A blocked record stays
 * blocked (net_domain_c2_accept_ingress_epoch only). */
static void barrier_rebase(NetDomainC2 *a,NetDomainC2BarrierKind kind,uint32_t id){size_t i;
uint16_t can_id=kind==NET_DOMAIN_C2_ENVIRONMENT_BARRIER?0x300u:0x310u;
for(i=0u;i<19u;++i){NetDomainC2IngressRecord *r=&a->ingress[i];
if(r->used&&r->link==NET_WIRE_CAN&&r->id==can_id&&!r->blocked){if(kind==NET_DOMAIN_C2_ENVIRONMENT_BARRIER)r->boot=id;
r->sequence=UINT64_MAX;
}
}
}
/* Adapter step side, outside every service callback: one call per pending
 * barrier. A completion counts only with the receipt of this request (same
 * generation and epoch, a DOMAIN barrier revision past the last confirmed
 * one); then the barrier opens and the epoch goes +1. NOT_INITIALIZED keeps the
 * request and its epoch for the next step. UNAVAILABLE disables the barrier.
 * Any other result, a receipt of another request and an epoch that would reach
 * UINT64_MAX (never issued, never wrapped) block it. NET_NO_WORK: no call. */
static NetStatus barrier_step(NetDomainC2 *a,NetDomainC2BarrierKind kind){NetDomainC2Barrier *b=&a->barrier[kind];
NetDomainC2TransitionApply apply=barrier_apply(a,kind);
NetDomainC2TransitionReceipt receipt;
NetDomainC2TransitionStatus st;
if(!a->config.cis_transition_barriers||apply==NULL||b->state!=NET_DOMAIN_C2_BARRIER_PENDING)return NET_NO_WORK;
if(b->next_epoch==0u||b->next_epoch==UINT64_MAX){barrier_latch(b,NET_DOMAIN_C2_BARRIER_BLOCKED,NET_DOMAIN_C2_BARRIER_REASON_EPOCH_EXHAUSTED);
return NET_HOLD;
}
memset(&receipt,0,sizeof(receipt));
st=apply(a->config.transition.context,b->id,b->next_epoch,&receipt);
net_upper_observe_increment(&b->calls);
b->last_result=st;
b->last_receipt=receipt;
switch(st){
case NET_DOMAIN_C2_TRANSITION_APPLIED:case NET_DOMAIN_C2_TRANSITION_DUPLICATE_APPLIED:
if(receipt.id!=b->id||receipt.epoch!=b->next_epoch||receipt.revision<=b->revision){
barrier_latch(b,NET_DOMAIN_C2_BARRIER_BLOCKED,NET_DOMAIN_C2_BARRIER_REASON_RECEIPT_MISMATCH);
return NET_HOLD;
}
b->revision=receipt.revision;
++b->next_epoch;
net_upper_observe_increment(&b->applied);
barrier_latch(b,NET_DOMAIN_C2_BARRIER_OPEN,NET_DOMAIN_C2_BARRIER_REASON_NONE);
b->open=true;
barrier_rebase(a,kind,b->id);
return NET_OK;
case NET_DOMAIN_C2_TRANSITION_NOT_INITIALIZED:return NET_HOLD;
case NET_DOMAIN_C2_TRANSITION_UNAVAILABLE:barrier_latch(b,NET_DOMAIN_C2_BARRIER_DISABLED,NET_DOMAIN_C2_BARRIER_REASON_RESULT);
return NET_HOLD;
default:barrier_latch(b,NET_DOMAIN_C2_BARRIER_BLOCKED,NET_DOMAIN_C2_BARRIER_REASON_RESULT);
return NET_HOLD;
}
}
NetDomainC2 *nd_registered(void){return bound;
}
NetStatus net_domain_c2_from_status(DomainIf_Status_t s){switch(s){
case DOMAIN_IF_OK:return NET_OK;
case DOMAIN_IF_INVALID_ARGUMENT:return NET_INVALID_ARGUMENT;
case DOMAIN_IF_NOT_INITIALIZED:return NET_NOT_CONFIGURED;
case DOMAIN_IF_QUEUE_FULL:return NET_NO_CAPACITY;
case DOMAIN_IF_REJECTED:return NET_REJECTED;
case DOMAIN_IF_UNSUPPORTED:return NET_UNSUPPORTED;
case DOMAIN_IF_NOT_AVAILABLE:default:return NET_HOLD;
}
}
DomainIf_Status_t net_domain_c2_to_status(NetStatus s){switch(s){
case NET_OK:return DOMAIN_IF_OK;
case NET_INVALID_ARGUMENT:return DOMAIN_IF_INVALID_ARGUMENT;
case NET_NOT_CONFIGURED:return DOMAIN_IF_NOT_INITIALIZED;
case NET_NO_CAPACITY:return DOMAIN_IF_QUEUE_FULL;
case NET_UNSUPPORTED:return DOMAIN_IF_UNSUPPORTED;
case NET_REJECTED:case NET_EXPIRED:case NET_STALE:return DOMAIN_IF_REJECTED;
default:return DOMAIN_IF_NOT_AVAILABLE;
}
}
bool nd_ready(const NetDomainC2 *a){return a!=NULL&&a->initialized&&a->api.IsInitialized!=NULL&&a->api.IsInitialized();
}
NetStatus nd_call(NetDomainC2 *a,DomainIf_Status_t status){a->last_c2_status=status;
net_upper_observe_increment(&a->boundary_calls);
a->last_status=net_domain_c2_from_status(status);
return a->last_status;
}
NetStatus nd_age(uint16_t raw,uint64_t elapsed,uint32_t *out){
if(out==NULL)return NET_INVALID_ARGUMENT;
if(raw==UINT16_MAX){*out=UINT32_MAX;
return NET_OK;
}
if(elapsed>UINT32_MAX-raw)return NET_HOLD;
*out=(uint32_t)elapsed+raw;
return NET_OK;
}
NetStatus net_domain_c2_value_quality_provisional(void *context,uint8_t validity,uint8_t reason,DataQuality_t *out){
DataQuality_t q;
(void)context;
if(out==NULL||validity>1u||reason>9u)return NET_INVALID_ARGUMENT;
if(reason==5u)q=DATA_QUALITY_STALE;
else if(reason==6u)q=DATA_QUALITY_NO_DATA;
else if(validity==1u&&reason==0u)q=DATA_QUALITY_OK;
else q=DATA_QUALITY_INVALID;
*out=q;
return NET_OK;
}
/* PROVISIONAL status report quality. BCM_STATUS (0x230), CIS_STATUS (0x320)
 * and VSS_STATUS (0x340) carry no quality field on the wire
 * (docs/network/interface/03_COMMUNICATION/10_CAN_FD.md). The adapter calls
 * this only for a frame the service admitted and the mapping decoded, so the
 * report itself is OK; its freshness is judged by DOMAIN on meta.ageMs (the
 * wire SOURCE_AGE plus the local wait) and its TTL. Any other id (WINDOW_FAULT
 * 0x330 included) is HOLD. Not selected by default: the integration (board
 * hook) or a test binds it. */
NetStatus net_domain_c2_status_quality_provisional(void *context,uint16_t id,const NetUpperCanMessage *m,DataQuality_t *out){
(void)context;
if(m==NULL||out==NULL||m->id!=id)return NET_INVALID_ARGUMENT;
if(id!=0x230u&&id!=0x320u&&id!=0x340u)return NET_HOLD;
*out=DATA_QUALITY_OK;
return NET_OK;
}
/* PROVISIONAL DOMAIN_PERMISSION bundle
 * (docs/network/interface/03_COMMUNICATION/10_CAN_FD.md DOMAIN_PERMISSION):
 *   - Power: only a confirmed ALLOW / DENY (OK quality) is 1 / 0; UNKNOWN (a
 *     non-OK quality) is 255 with that quality. A true with a non-OK quality is
 *     HOLD, and so is a true whose meta.quality is not OK
 *     (04_COMMON/10_DATA_AND_ID_RULES.md, DOMAIN_PERMISSION synthesis: a
 *     quality and meta.quality in conflict make the input unconfirmed, no OK
 *     is chosen, and no permission is newly made from it).
 *   - Operation: its source is not decided; an unknown meaning is UNKNOWN with
 *     a non-normal quality and no permission is made up: POWER_OPERATION_STATE
 *     255, OPERATION_ALLOWED 0, OPERATION_QUALITY NO_DATA.
 *   - SOURCE_AGE is meta.ageMs (above 65535 is HOLD). UPDATE_SEQUENCE carries
 *     meta.updateSequence here; the adapter replaces it with the synthesized
 *     number. */
NetStatus net_domain_c2_permission_bundle_provisional(void *context,const DomainIf_CisPowerPermission_t *in,NetWireDomainPermissionRaw *out){
NetWireDomainPermissionRaw r;
uint8_t quality;
(void)context;
if(in==NULL||out==NULL)return NET_INVALID_ARGUMENT;
ND_TO(DataQuality_t,in->quality,quality);
if(in->vehiclePowerAllowed&&(in->quality!=DATA_QUALITY_OK||in->meta.quality!=DATA_QUALITY_OK))return NET_HOLD;
if(in->meta.ageMs>UINT16_MAX)return NET_HOLD;
memset(&r,0,sizeof(r));
r.vehicle_power_permission=in->quality!=DATA_QUALITY_OK?255u:(in->vehiclePowerAllowed?1u:0u);
r.power_permission_quality=quality;
r.power_operation_state=255u;
r.operation_allowed=0u;
r.operation_quality=3u;
r.update_sequence=in->meta.updateSequence;
r.source_age_ms=(uint16_t)in->meta.ageMs;
*out=r;
return NET_OK;
}
NetStatus nd_value(NetDomainC2 *a,uint8_t valid,uint8_t reason,uint32_t sequence,uint16_t age,uint64_t elapsed,ValueQuality_t *out){
ValueQuality_t q;
if(a->config.value_quality==NULL)return NET_HOLD;
memset(&q,0,sizeof(q));
ND_FROM(ValueValidity_t,valid,q.validity);
ND_FROM(QualityReason_t,reason,q.reason);
ND_TRY(a->config.value_quality(a->config.context,valid,reason,&q.meta.quality));
q.meta.updateSequence=sequence;
ND_TRY(nd_age(age,elapsed,&q.meta.ageMs));
*out=q;
return NET_OK;
}
NetStatus net_domain_c2_mm_to_cm(uint16_t mm,NetDomainCmPolicy policy,uint16_t *out){uint16_t value;
if(out==NULL||mm==UINT16_MAX)return NET_INVALID_ARGUMENT;
switch(policy){case NET_DOMAIN_CM_EXACT_ONLY:if(mm%10u!=0u)return NET_HOLD;
value=mm/10u;
break;
case NET_DOMAIN_CM_FLOOR:value=mm/10u;
break;
case NET_DOMAIN_CM_NEAREST:value=(uint16_t)(((uint32_t)mm+5u)/10u);
break;
default:return NET_INVALID_ARGUMENT;
}
*out=value;
return NET_OK;
}
NetStatus net_domain_c2_init(NetDomainC2 *a,NetUpper *s,NetUpperQuery *q,const NetDomainC2Api *api,const NetDomainC2Config *config){
if(a==NULL||s==NULL||api==NULL||config==NULL||api->IsInitialized==NULL||api->SetTxPort==NULL||config->domain_boot==0u||config->first_can_sequence==0u)return NET_INVALID_ARGUMENT;
if(net_wire_ranges_overlap(a,sizeof(*a),s,sizeof(*s))||net_wire_ranges_overlap(a,sizeof(*a),api,sizeof(*api))||net_wire_ranges_overlap(a,sizeof(*a),config,sizeof(*config)))return NET_INVALID_ARGUMENT;
if(bound==a||memcmp(a->init_tag,init_tag,sizeof(init_tag))==0)return NET_BUSY;
if(q!=NULL&&(net_wire_ranges_overlap(a,sizeof(*a),q,sizeof(*q))||net_wire_ranges_overlap(s,sizeof(*s),q,sizeof(*q))))return NET_INVALID_ARGUMENT;
/* A bound publisher serves this service and is not part of the adapter. */
if(config->publisher!=NULL&&(net_wire_ranges_overlap(a,sizeof(*a),config->publisher,sizeof(*config->publisher))||config->publisher->service!=s))return NET_INVALID_ARGUMENT;
memset(a,0,sizeof(*a));
a->service=s;
a->query=q;
a->api=*api;
a->config=*config;
{size_t i;
for(i=0u;i<NET_DOMAIN_C2_CAN_MESSAGES;++i)a->can_sequence[i]=config->first_can_sequence;
/* Transition epochs start at 1 in every run, one count per barrier. */
for(i=0u;i<NET_DOMAIN_C2_BARRIERS;++i)a->barrier[i].next_epoch=1u;
}
a->initialized=true;
memcpy(a->init_tag,init_tag,sizeof(init_tag));
return NET_OK;
}
NetUpperOwner net_domain_c2_owner(NetDomainC2 *a){NetUpperOwner owner;
memset(&owner,0,sizeof(owner));
owner.context=a;
owner.rear=NULL;
owner.can=nd_rx_can;
owner.uart=nd_rx_uart;
return owner;
}
NetStatus net_domain_c2_register(NetDomainC2 *a){NetStatus status;
if(!nd_ready(a)||!a->service->initialized)return NET_NOT_CONFIGURED;
if(bound!=NULL&&bound!=a)return NET_BUSY;
if(a->registered)return NET_DUPLICATE;
bound=a;
status=nd_call(a,a->api.SetTxPort(net_domain_c2_tx_port()));
if(status==NET_OK)a->registered=true;
else bound=NULL;
return status;
}
NetStatus net_domain_c2_unregister(NetDomainC2 *a){size_t i;
NetStatus status;
if(a==NULL||bound!=a||!a->registered)return NET_NOT_CONFIGURED;
if(a->in_call||a->service->in_call)return NET_BUSY;
for(i=0u;i<NET_UPPER_TX_CAPACITY;++i)if(a->tx[i].used)return NET_BUSY;
status=nd_call(a,a->api.SetTxPort(NULL));
if(status==NET_OK){bound=NULL;
a->registered=false;
}
return status;
}
NetStatus net_domain_c2_set_observer(NetDomainC2 *a,const DomainIf_RxObserver_t *o){
if(!nd_ready(a))return NET_NOT_CONFIGURED;
if(a->api.SetRxObserver==NULL)return NET_HOLD;
if(a->in_call||a->service->in_call)return NET_BUSY;
return nd_call(a,a->api.SetRxObserver(o));
}
NetStatus nd_context(NetDomainC2 *a,uint32_t *device,uint64_t *session){
if(a==NULL||device==NULL||session==NULL)return NET_INVALID_ARGUMENT;
if(!a->service->context_seen||a->service->context_port_epoch!=a->service->config.port_epoch[NET_WIRE_UART_M]||a->service->link_failed[NET_WIRE_UART_M])return NET_HOLD;
*device=a->service->context.device_context_id;
*session=a->service->context.session_id;
return NET_OK;
}
NetStatus nd_remember(NetDomainC2 *a,NetTxKey key,uint64_t now,const NetworkLocalFailure_t *id){size_t i;
for(i=0u;i<NET_UPPER_TX_CAPACITY;++i)if(!a->tx[i].used){NetDomainC2TxRecord *r=&a->tx[i];
memset(r,0,sizeof(*r));
r->used=true;
r->key=key;
r->admitted_at=now;
if(id!=NULL)r->identity=*id;
else r->failure_delivered=true;
a->last_tx_key=key;
return NET_OK;
}
return NET_NO_CAPACITY;
}
NetStatus nd_room(NetDomainC2 *a,const NetworkLocalFailure_t *id){size_t i;
if(!nd_ready(a))return NET_NOT_CONFIGURED;
if(id!=NULL&&(!a->config.failure_sink_ready||a->api.RxNetworkLocalFailure==NULL))return NET_HOLD;
for(i=0u;i<NET_UPPER_TX_CAPACITY;++i)if(!a->tx[i].used)return NET_OK;
return NET_NO_CAPACITY;
}
/* TX_SEQUENCE is kept per message, first 1, +1 for every frame
 * (docs/network/interface/03_COMMUNICATION/10_CAN_FD.md common header). */
uint32_t *nd_can_sequence(NetDomainC2 *a,uint16_t id){size_t i;
switch(id){case 0x100u:i=0u;
break;
case 0x110u:i=1u;
break;
case 0x120u:i=2u;
break;
case 0x080u:i=3u;
break;
case 0x0c0u:i=4u;
break;
case 0x160u:i=5u;
break;
case 0x090u:i=6u;
break;
default:return NULL;
}
return &a->can_sequence[i];
}
/* BCM_STATUS first, then any other BCM frame (bcm_target_from_rx). */
void nd_bcm_boot(NetDomainC2 *a,uint16_t id,uint32_t boot){const NetWireRegistryEntry *r=net_wire_registry_find(NET_WIRE_CAN,id);
if(r==NULL||r->sender!=NET_WIRE_NODE_BCM||boot==0u)return;
if(id==0x230u)a->bcm_status_boot=boot;
else a->bcm_frame_boot=boot;
}
NetStatus nd_send_can(NetDomainC2 *a,NetUpperCanMessage *m,const NetworkLocalFailure_t *id){NetSourceStamp source;
NetTxKey key;
uint64_t now;
uint32_t *sequence;
ND_TRY(nd_room(a,id));
ND_TRY(net_upper_now(a->service,&now));
sequence=nd_can_sequence(a,m->id);
if(sequence==NULL)return NET_UNSUPPORTED;
if(*sequence==0u||*sequence==UINT32_MAX)return NET_HOLD;
ND_TRY(net_upper_wire_status(net_upper_can_set_identity(m,a->config.domain_boot,*sequence)));
source.observation_id=*sequence;
source.observed_at_ms=now;
source.original_age_ms=net_upper_can_max_age(m);
ND_TRY(net_upper_submit_can(a->service,m,&source,&a->config.transmit_policy,&key));
++*sequence;
return nd_remember(a,key,now,id);
}
/* The M04 slot takes the latest content and assigns BOOT_ID / TX_SEQUENCE per
 * frame; a newer submission replaces one no device has taken
 * (net_upper_publish.h). Accepted = copied into the slot. */
NetStatus nd_send_state(NetDomainC2 *a,NetUpperCanMessage *m){
if(a->config.publisher==NULL)return nd_send_can(a,m,NULL);
return net_upper_publish_can(a->config.publisher,m,NULL,NULL);
}
NetStatus nd_send_uart(NetDomainC2 *a,NetUpperUartMessage *m,const NetworkLocalFailure_t *id){NetSourceStamp source;
NetTxKey key;
uint64_t now;
ND_TRY(nd_room(a,id));
ND_TRY(net_upper_now(a->service,&now));
source.observation_id=a->service->next_token;
source.observed_at_ms=now;
source.original_age_ms=net_upper_uart_max_age(m);
ND_TRY(net_upper_submit_uart_auto(a->service,m,&source,&a->config.transmit_policy,&key));
return nd_remember(a,key,now,id);
}
NetStatus net_domain_c2_step(NetDomainC2 *a)
{
    size_t i;uint64_t now;NetStatus result=NET_NO_WORK,transition=NET_NO_WORK;
    if(!nd_ready(a))return NET_NOT_CONFIGURED;
    if(a->in_call||a->service->in_call)return NET_BUSY;
    ND_TRY(net_upper_now(a->service,&now));a->in_call=true;
    /* CIS transitions: here only, never inside a service callback. */
    for(i=0u;i<NET_DOMAIN_C2_BARRIERS;++i){NetStatus st=barrier_step(a,(NetDomainC2BarrierKind)i);
        if(st!=NET_NO_WORK&&transition!=NET_HOLD)transition=st;}
    for(i=0u;i<NET_UPPER_TX_CAPACITY;++i){
        NetDomainC2TxRecord *r=&a->tx[i];NetUpperTxSnapshot state;NetStatus st;bool rejected,terminal;
        if(!r->used)continue;
        if(r->held){result=NET_HOLD;continue;}
        st=net_upper_tx_snapshot(a->service,r->key,&state);
        if(st!=NET_OK){result=st;continue;}
        rejected=state.admission_known&&!state.accepted;
        terminal=rejected||state.outcome!=NET_TX_UNKNOWN;
        if(!r->failure_delivered&&(rejected||state.outcome==NET_TX_FAILED)){
            if(state.conflict||(state.submitted&&!state.reference_released)){result=NET_HOLD;continue;}
            if(state.submitted&&!rejected){
                /* The device took the frame and it failed afterwards: an
                 * in-flight failure, no local failure reason applies. The record
                 * keeps its identity and stays held; only its M04 slot is
                 * retired (NetDomainC2TxRecord.held). */
                if(now>=r->admitted_at&&now-r->admitted_at<=UINT32_MAX)
                    r->identity.elapsedSinceAdmissionMs=(uint32_t)(now-r->admitted_at);
                if(a->service->config.retain_failure_notices){NetUpperTxDetail detail;
                    st=net_upper_tx_detail(a->service,r->key,&detail);
                    if(st==NET_OK&&detail.failure_version!=detail.failure_ack_version)
                        st=net_upper_failure_ack(a->service,r->key,detail.failure_version);
                    if(st!=NET_OK){result=st;continue;}
                }
                st=net_upper_retire(a->service,r->key);
                if(st!=NET_OK){result=st;continue;}
                r->held=true;
                if(a->tx_held<UINT32_MAX)++a->tx_held;
                result=NET_HOLD;continue;
            }
            if(now<r->admitted_at||now-r->admitted_at>UINT32_MAX){result=NET_HOLD;continue;}
            if(state.local_status==NET_EXPIRED)r->identity.reason=NETWORK_LOCAL_FAILURE_EXPIRED_BEFORE_SEND;
            else if(a->config.local_failure_reason!=NULL){
                st=a->config.local_failure_reason(a->config.context,&state,&r->identity.reason);
                if(st!=NET_OK){result=st;continue;}
                if(r->identity.reason!=NETWORK_LOCAL_FAILURE_DRIVER_SUBMISSION_FINAL&&r->identity.reason!=NETWORK_LOCAL_FAILURE_CONTEXT_INVALIDATED_BEFORE_SEND){result=NET_HOLD;continue;}
            }else{result=NET_HOLD;continue;}
            r->identity.elapsedSinceAdmissionMs=(uint32_t)(now-r->admitted_at);
            r->failure_delivered=true;
            r->failure_return=a->api.RxNetworkLocalFailure(&r->identity);
            result=nd_call(a,r->failure_return);
        }
        /* A failed/ambiguous sink is retained once, without replaying Core. */
        if(r->failure_delivered&&r->failure_return!=DOMAIN_IF_OK){result=NET_HOLD;continue;}
        if(terminal&&(!state.submitted||state.reference_released)&&!state.conflict){
            if(a->service->config.retain_failure_notices){NetUpperTxDetail detail;
                st=net_upper_tx_detail(a->service,r->key,&detail);
                if(st==NET_OK&&detail.failure_version!=detail.failure_ack_version)
                    st=net_upper_failure_ack(a->service,r->key,detail.failure_version);
                if(st!=NET_OK){result=st;continue;}
            }
            st=net_upper_retire(a->service,r->key);
            if(st==NET_OK){r->used=false;result=NET_OK;}else result=st;
        }
    }
    a->in_call=false;return result==NET_NO_WORK?transition:result;
}
NetStatus net_domain_c2_health(NetDomainC2 *a,NetworkLinkId_t link,uint32_t now,NetworkHealthSnapshot_t *out)
{
    NetworkHealthSnapshot_t value;DomainIf_Status_t status;
    if(a==NULL||!a->initialized||out==NULL)return NET_INVALID_ARGUMENT;
    if(net_wire_ranges_overlap(a,sizeof(*a),out,sizeof(*out))||net_wire_ranges_overlap(a->service,sizeof(*a->service),out,sizeof(*out)))return NET_INVALID_ARGUMENT;
    if(a->in_call||a->service->in_call)return NET_BUSY;
    if(!nd_ready(a))return NET_NOT_CONFIGURED;
    if(link==NETWORK_LINK_UART_C)return NET_UNSUPPORTED;
    if(link!=NETWORK_LINK_CAN&&link!=NETWORK_LINK_UART_M)return NET_INVALID_ARGUMENT;
    if(a->api.GetNetworkHealthSnapshot==NULL)return NET_HOLD;
    a->in_call=true;status=a->api.GetNetworkHealthSnapshot(link,now,&value);a->in_call=false;
    if(status!=DOMAIN_IF_OK)return nd_call(a,status);
    if(value.link!=link||(uint32_t)value.transportState>(uint32_t)NETWORK_TRANSPORT_RECOVERING)return NET_REJECTED;
    *out=value;return nd_call(a,status);
}
/* A non-OK Owner return blocks the route (the ingress record) for the rest of
 * the run, except DOMAIN_IF_REJECTED on BCM_COMMAND_RESULT (0x140) and
 * BCM_PROTECTION_EVENT (0x141): each of those frames reports one command or
 * one event, not a state stream, and DOMAIN rejects a result whose command it
 * has no record of (ResultManager NOT_FOUND, for example the late result of a
 * command sent before a DOMAIN reset). That frame alone is dropped and counted
 * in rejected_frames; the record keeps its sequence, so the same frame is not
 * offered again, and the next result or event of the route is delivered. */
NetStatus nd_rx_call(NetDomainC2 *a,DomainIf_Status_t status){
NetDomainC2IngressRecord *r=a->current_ingress;
if(r!=NULL){a->current_ingress=NULL;
if(status==DOMAIN_IF_REJECTED&&r->link==NET_WIRE_CAN&&(r->id==0x140u||r->id==0x141u))net_upper_observe_increment(&a->rejected_frames);
else if(status!=DOMAIN_IF_OK)r->blocked=true;}
return nd_call(a,status);}

NetStatus net_domain_c2_accept_ingress_epoch(NetDomainC2 *a,NetWireLink link,uint16_t id,uint64_t epoch,uint32_t boot,bool confirmed)
{
    size_t i;
    if(a==NULL||!a->initialized||epoch==0u||link>NET_WIRE_UART_C)return NET_INVALID_ARGUMENT;
    if(a->in_call||a->service->in_call)return NET_BUSY;
    if(!confirmed||epoch!=a->service->config.port_epoch[link])return NET_HOLD;
    for(i=0u;i<19u;++i){NetDomainC2IngressRecord *r=&a->ingress[i];
        if(r->used&&r->link==link&&r->id==id){
            if(epoch<=r->epoch)return NET_STALE;
            if(link==NET_WIRE_CAN&&boot==0u)return NET_HOLD;
            /* Owner explicitly fenced old writes and resolved prior commit.
             * No old DTO is replayed and no stored source is relabeled. */
            memset(r,0,sizeof(*r));r->used=true;r->link=link;r->id=id;r->epoch=epoch;r->boot=boot;r->sequence=UINT64_MAX;return NET_OK;
        }
    }
    return NET_NO_WORK;
}
