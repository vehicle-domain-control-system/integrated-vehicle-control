#include "net_domain_private.h"
static NetDomainC2 *bound;
static const unsigned char init_tag[8]={0x4eu,0x43u,0x32u,0x41u,0x44u,0x50u,0x54u,0x31u};
NetStatus nd_ingress_once(NetDomainC2 *a,NetWireLink link,uint16_t id,uint32_t boot,uint64_t epoch,uint64_t sequence)
{
    size_t i;
NetDomainC2IngressRecord *slot=NULL;
    for(i=0u;i<19u;++i){NetDomainC2IngressRecord *r=&a->ingress[i];
        if(r->used&&r->link==link&&r->id==id){
            if(r->blocked)return NET_HOLD;
if(r->epoch>epoch||r->boot!=boot)return NET_HOLD;
            if(r->sequence!=UINT64_MAX&&sequence==r->sequence)return NET_DUPLICATE;
            if(r->sequence!=UINT64_MAX&&sequence<r->sequence)return NET_STALE;
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
memset(a,0,sizeof(*a));
a->service=s;
a->query=q;
a->api=*api;
a->config=*config;
a->next_can_sequence=config->first_can_sequence;
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
static NetStatus remember(NetDomainC2 *a,NetTxKey key,uint64_t now,const NetworkLocalFailure_t *id){size_t i;
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
static NetStatus room(NetDomainC2 *a,const NetworkLocalFailure_t *id){size_t i;
if(!nd_ready(a))return NET_NOT_CONFIGURED;
if(id!=NULL&&(!a->config.failure_sink_ready||a->api.RxNetworkLocalFailure==NULL))return NET_HOLD;
for(i=0u;i<NET_UPPER_TX_CAPACITY;++i)if(!a->tx[i].used)return NET_OK;
return NET_NO_CAPACITY;
}
NetStatus nd_send_can(NetDomainC2 *a,NetUpperCanMessage *m,const NetworkLocalFailure_t *id){NetSourceStamp source;
NetTxKey key;
uint64_t now;
ND_TRY(room(a,id));
ND_TRY(net_upper_now(a->service,&now));
if(a->next_can_sequence==0u||a->next_can_sequence==UINT32_MAX)return NET_HOLD;
source.observation_id=a->next_can_sequence;
source.observed_at_ms=now;
source.original_age_ms=net_upper_can_max_age(m);
ND_TRY(net_upper_submit_can(a->service,m,&source,&a->config.transmit_policy,&key));
++a->next_can_sequence;
return remember(a,key,now,id);
}
NetStatus nd_send_uart(NetDomainC2 *a,NetUpperUartMessage *m,const NetworkLocalFailure_t *id){NetSourceStamp source;
NetTxKey key;
uint64_t now;
ND_TRY(room(a,id));
ND_TRY(net_upper_now(a->service,&now));
source.observation_id=a->service->next_token;
source.observed_at_ms=now;
source.original_age_ms=net_upper_uart_max_age(m);
ND_TRY(net_upper_submit_uart_auto(a->service,m,&source,&a->config.transmit_policy,&key));
return remember(a,key,now,id);
}
NetStatus net_domain_c2_step(NetDomainC2 *a)
{
    size_t i;uint64_t now;NetStatus result=NET_NO_WORK;
    if(!nd_ready(a))return NET_NOT_CONFIGURED;
    if(a->in_call||a->service->in_call)return NET_BUSY;
    ND_TRY(net_upper_now(a->service,&now));a->in_call=true;
    for(i=0u;i<NET_UPPER_TX_CAPACITY;++i){
        NetDomainC2TxRecord *r=&a->tx[i];NetUpperTxSnapshot state;NetStatus st;bool rejected,terminal;
        if(!r->used)continue;
        st=net_upper_tx_snapshot(a->service,r->key,&state);
        if(st!=NET_OK){result=st;continue;}
        rejected=state.admission_known&&!state.accepted;
        terminal=rejected||state.outcome!=NET_TX_UNKNOWN;
        if(!r->failure_delivered&&(rejected||state.outcome==NET_TX_FAILED)){
            if(state.conflict||(state.submitted&&(!rejected||!state.reference_released))){result=NET_HOLD;continue;}
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
    a->in_call=false;return result;
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
    if(value.link!=link||value.transportState<NETWORK_TRANSPORT_UNKNOWN||value.transportState>NETWORK_TRANSPORT_RECOVERING)return NET_REJECTED;
    *out=value;return nd_call(a,status);
}
NetStatus nd_rx_call(NetDomainC2 *a,DomainIf_Status_t status){
if(a->current_ingress!=NULL){if(status!=DOMAIN_IF_OK)a->current_ingress->blocked=true;a->current_ingress=NULL;}
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
