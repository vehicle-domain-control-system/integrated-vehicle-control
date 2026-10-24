#include "net_upper_private.h"
#include "network/net_upper_control.h"
#include "../../l04_frame_codec/net_wire_internal.h"
#include <string.h>
#define INC(x) net_upper_observe_increment(&(x))

/* Queue high-water marks (NetUpperObservation.tx_max_depth, rx_max_depth). */
static void observe_rx_depth(NetUpper *s)
{ if(s->rx_count>s->observation.rx_max_depth)s->observation.rx_max_depth=(uint32_t)s->rx_count; }
static void observe_tx_depth(NetUpper *s)
{
    size_t i,used=0u;
    for(i=0u;i<NET_UPPER_TX_CAPACITY;++i)if(s->tx[i].state.used)++used;
    if(used>s->observation.tx_max_depth)s->observation.tx_max_depth=(uint32_t)used;
}

static size_t route_index(NetWireLink link, uint16_t id)
{
    size_t i;
    for(i=0u;i<net_wire_registry_count();++i) {
        const NetWireRegistryEntry *r=net_wire_registry_at(i);
        if(r->link==link && r->id_type==id) return i;
    }
    return NET_UPPER_ROUTE_COUNT;
}
static bool disabled(const NetWireRegistryEntry *r, uint32_t node)
{
    return node==NET_WIRE_NODE_WINDOW || r->sender==NET_WIRE_NODE_WINDOW ||
        (r->link==NET_WIRE_CAN && r->receivers==NET_WIRE_NODE_WINDOW) ||
        (r->link==NET_WIRE_UART_M && (r->id_type==0x38u || r->id_type==0x39u));
}
static NetStatus age_check(const NetSourceStamp *stamp, const NetAgePolicy *p,
    uint64_t now, uint64_t *age)
{
    NetStatus s;
    if(!p->bound) return NET_HOLD;
    s=net_source_age(stamp,now,age); if(s!=NET_OK) return s;
    if(*age>p->maximum_age_ms || (*age==p->maximum_age_ms && p->expires_at_equality)) return NET_EXPIRED;
    return NET_OK;
}
static uint32_t u32(const uint8_t *b)
{ return (uint32_t)b[0]|((uint32_t)b[1]<<8u)|((uint32_t)b[2]<<16u)|((uint32_t)b[3]<<24u); }
static uint64_t u64(const uint8_t *b)
{ return (uint64_t)u32(b)|((uint64_t)u32(b+4u)<<32u); }
static bool context_equal(const NetWireMContext *a, const NetWireMContext *b)
{ return a->device_context_id==b->device_context_id && a->session_id==b->session_id; }
static NetStatus context_check(const NetUpper *s, const NetUpperUartMessage *m,
    const NetWireUartFrame *frame)
{
    if(m->link!=NET_WIRE_UART_M || m->type==0x01u || m->type==0x02u) return NET_OK;
    if(!s->context_seen || s->context_port_epoch!=s->config.port_epoch[NET_WIRE_UART_M]) return NET_HOLD;
    if(frame->length<20u || u32(frame->bytes+8u)!=s->context.device_context_id ||
       u64(frame->bytes+12u)!=s->context.session_id) return NET_STALE;
    return NET_OK;
}
static void accept_context(NetUpper *s, const NetWireMContext *context)
{
    size_t i;
    if(!s->context_seen || !context_equal(&s->context,context)) {
        for(i=0u;i<NET_UPPER_ROUTE_COUNT;++i)
            if(net_wire_registry_at(i)->link==NET_WIRE_UART_M) {
                NetUpperAdmission *a=net_upper_admission_at_mut(s,i);
                if(a!=NULL)a->seen=false;
            }
    }
    s->context=*context; s->context_seen=true;
    s->context_port_epoch=s->config.port_epoch[NET_WIRE_UART_M];
}
static uint16_t projection_id(uint8_t type)
{
    switch(type) {
    case 0x30u:return 0x200u; case 0x31u:return 0x210u; case 0x32u:return 0x220u;
    case 0x33u:return 0x230u; case 0x34u:return 0x300u; case 0x35u:return 0x310u;
    case 0x36u:return 0x0a0u; case 0x37u:return 0x320u; case 0x3au:return 0x340u;
    case 0x38u:return 0x0b0u; case 0x39u:return 0x330u; default:return 0u;
    }
}
static bool receipt_matches(const NetUpper *s,const NetUpperAdmission *a, const NetSourceStamp *source)
{
    return a!=NULL && a->seen && a->delivery.capture.timestamp_known &&
        a->delivery.capture.port_epoch==s->config.port_epoch[NET_WIRE_CAN] &&
        !s->link_failed[NET_WIRE_CAN] && !s->quiescing[NET_WIRE_CAN] &&
        source->observation_id==a->delivery.receipt_id &&
        source->observed_at_ms==a->delivery.capture.captured_at_ms &&
        source->original_age_ms==net_upper_can_max_age(&a->can);
}
/* P_PERMISSION QUALITY_REASON of a relayed DOMAIN power quality: a fixed
 * table, the same value for the same quality (the DOMAIN_PERMISSION wire has
 * no reason field, so nothing else is derived). VALUE_QUALITY codes
 * (04_COMMON/10_DATA_AND_ID_RULES.md NetValueQuality) to NetQualityReason
 * codes (same document, quality reason): OK -> NONE, STALE -> STALE,
 * INVALID -> DATA_INVALID, NO_DATA -> NO_DATA. Outside the codes: 0xFF, which
 * no relay record can carry. */
uint8_t net_upper_pi_permission_reason(uint8_t power_quality)
{
    switch(power_quality) {
    case 0u:return 0u;
    case 1u:return 5u;
    case 2u:return 9u;
    case 3u:return 6u;
    default:return 0xFFu;
    }
}
static NetStatus uart_origin(const NetUpper *s, const NetUpperUartMessage *m,
    const NetWireUartFrame *frame, const NetSourceStamp *source)
{
    uint16_t id=m->link==NET_WIRE_UART_M ? projection_id(m->type) : 0u;
    if(id!=0u) {
        size_t i=route_index(NET_WIRE_CAN,id);
        const NetUpperAdmission *a=net_upper_admission_at(s,i);
        const NetWireRegistryEntry *r=net_wire_registry_at(i);
        NetWireCanEnvelope original;
        if(!receipt_matches(s,a,source)) return NET_HOLD;
        if(net_upper_can_encode(s->config.profile,&a->can,&original)!=NET_WIRE_OK) return NET_HOLD;
        if(frame->length!=(size_t)r->payload_length+30u ||
           memcmp(frame->bytes+28u,original.bytes,r->payload_length)!=0) return NET_HOLD;
    }
    if(m->link==NET_WIRE_UART_C && m->type==0x62u) {
        /* The relay record against the original receipt it relays: the stamp
         * is that DOMAIN_PERMISSION receipt (id, capture time, AGE basis) and
         * the power value, power quality and AGE are the received ones. The
         * quality is relayed as received, restricted ones included
         * (30_UART_C.md P_PERMISSION: an actual quality restriction is
         * submitted at once; 02_NODES/CIS/20_API.md internal relay: a
         * normal-format DENY/STALE/INVALID is kept and relayed as an explicit
         * state), with the fixed reason of net_upper_pi_permission_reason().
         * UPDATE_SEQUENCE is the CIS relay revision (04_COMMON/
         * 10_DATA_AND_ID_RULES.md number spaces: not a lossless copy of the
         * DOMAIN number), so it is not compared with the DOMAIN one. */
        const NetUpperAdmission *a=net_upper_admission_at(s,route_index(NET_WIRE_CAN,0x080u));
        const NetWireDomainPermissionRaw *p;
        if(a==NULL)return NET_HOLD;
        p=&a->can.data.domain_permission;
        const NetWirePPermission *relay=&m->data.p_permission;
        if(!receipt_matches(s,a,source) || relay->vehicle_power_permission!=p->vehicle_power_permission ||
           relay->value_quality!=p->power_permission_quality || relay->source_age_ms!=p->source_age_ms ||
           relay->quality_reason!=net_upper_pi_permission_reason(p->power_permission_quality)) return NET_HOLD;
    }
    return NET_OK;
}
/* One field of a CIS_OCCUPANT relay against the Pi judgement it relays: the
 * same validity and reason, or a restriction (INVALID, any reason). A VALID
 * field needs a VALID Pi field: no promotion (02_NODES/CIS/10_DATA.md, Pi
 * original observation and CIS evaluation: the original Pi quality is kept and
 * later actual faults only restrict it; 02_NODES/CIS/20_API.md
 * CisNet_PublishOccupant: a higher TX_SEQUENCE never allows INVALID -> VALID). */
static bool occupant_field_restricted(uint8_t pi_validity,uint8_t pi_reason,uint8_t validity,uint8_t reason)
{
    return validity==0u || (validity==pi_validity && reason==pi_reason);
}
/* CIS_OCCUPANT relays the P_VISION_RESULT accepted last
 * (02_NODES/CIS/20_API.md CisNet_PublishOccupant): the stamp is that receipt
 * and the Pi identity (BOOT, VISION_SEQUENCE), AGE basis and values
 * (presence, count) are the Pi's. Each field's validity / reason is the
 * Pi's or a restriction of it, and vision_function_status is the CIS's own
 * function state (02_NODES/CIS/10_DATA.md: no raw cast of the Pi
 * VISION_STATUS), so it is not compared. */
static NetStatus vision_origin(const NetUpper *s,const NetUpperCanMessage *m,
    const NetSourceStamp *source)
{
    const NetUpperAdmission *a=net_upper_admission_at(s,route_index(NET_WIRE_UART_C,0x60u));
    const NetWirePVisionResult *v;
    if(a==NULL)return NET_HOLD;
    v=&a->uart.data.p_vision_result;
    const NetWireCisOccupantRaw *c=&m->data.cis_occupant;
    if(!a->seen || !a->delivery.capture.timestamp_known ||
       s->link_failed[NET_WIRE_UART_C] || s->quiescing[NET_WIRE_UART_C] ||
       a->delivery.capture.port_epoch!=s->config.port_epoch[NET_WIRE_UART_C] ||
       source->observation_id!=a->delivery.receipt_id ||
       source->observed_at_ms!=a->delivery.capture.captured_at_ms ||
       source->original_age_ms!=v->vision_age_ms || c->pi_boot_id!=v->pi_boot_id ||
       c->vision_sequence!=v->vision_sequence || c->vision_age_ms!=v->vision_age_ms ||
       c->occupant_presence!=v->occupant_presence || c->occupant_count!=v->occupant_count ||
       !occupant_field_restricted(v->presence_validity,v->presence_reason,c->presence_validity,c->presence_reason) ||
       !occupant_field_restricted(v->count_validity,v->count_reason,c->count_validity,c->count_reason))return NET_HOLD;
    return NET_OK;
}
static void config_commit(NetUpper *s,const NetUpperConfig *c)
{
#if NETWORK_UPPER_NODE_PROFILE == 0
    s->config=*c;
#else
    size_t i;
    s->config.node=c->node;s->config.local_epoch=c->local_epoch;
    s->config.profile=c->profile;s->config.port=c->port;s->config.owner=c->owner;
    s->config.retain_failure_notices=c->retain_failure_notices;
    s->config.inspection_transport=c->inspection_transport;
    for(i=0u;i<3u;++i)s->config.port_epoch[i]=c->port_epoch[i];
    for(i=0u;i<2u;++i) {
        s->config.uart_policy[i]=c->uart_policy[i];
        s->config.uart_sequence_bound[i]=c->uart_sequence_bound[i];
        s->config.uart_initial_sequence[i]=c->uart_initial_sequence[i];
    }
    for(i=0u;i<NET_UPPER_ROUTE_COUNT;++i) {
        size_t j=net_upper_admission_index(i);
        if(j<NET_UPPER_ADMISSION_CAPACITY) {
            s->config.rx_policy[j]=c->rx_policy[i];
            s->config.rx_per_field_age[j]=c->rx_per_field_age[i];
            s->config.rx_repeat_delivery[j]=c->rx_repeat_delivery[i];
        }
    }
#endif
}
NetStatus net_upper_init(NetUpper *s, const NetUpperConfig *c)
{
    static const unsigned char tag[8] = {0x4eu,0x55u,0x50u,0x52u,0x31u,0x69u,0xa7u,0x5cu};
    size_t i;
    if(s==NULL || c==NULL || net_wire_ranges_overlap(s,sizeof(*s),c,sizeof(*c)) ||
       c->local_epoch==0u || c->node==0u || (c->node&(c->node-1u))!=0u || c->node>NET_WIRE_NODE_PI ||
       c->port.ops==NULL || net_wire_ranges_overlap(s,sizeof(*s),c->port.ops,sizeof(*c->port.ops)) ||
       c->port.ops->now_ms==NULL || c->port.ops->submit==NULL || c->port.ops->poll==NULL)
        return NET_INVALID_ARGUMENT;
    /* Inspect object representation only, never an indeterminate _Bool. A live
     * service cannot be reinitialized, even after its last borrow is released. */
    if(memcmp(s->init_tag,tag,sizeof(tag))==0)return NET_BUSY;
    if(c->profile!=NET_WIRE_CANDIDATE_ONLY) return NET_HOLD;
    if(!net_upper_profile_matches(c->node))return NET_NOT_CONFIGURED;
    for(i=0u;i<3u;++i) if(c->port_epoch[i]==0u) return NET_INVALID_ARGUMENT;
    /* All fallible configuration checks precede the first store. The receiver
     * initializers below use these same pure checks, disjoint policy storage,
     * known link/direction and already validated nonzero epochs. */
    for(i=0u;i<2u;++i) {
        NetWireLink link=i==0u?NET_WIRE_UART_M:NET_WIRE_UART_C;
        bool receiver=i==0u ? (c->node==NET_WIRE_NODE_DOMAIN || c->node==NET_WIRE_NODE_ESP32) :
            (c->node==NET_WIRE_NODE_CIS || c->node==NET_WIRE_NODE_PI);
        NetWireDirection direction=(c->node==NET_WIRE_NODE_DOMAIN || c->node==NET_WIRE_NODE_CIS) ? NET_WIRE_UPLINK : NET_WIRE_DOWNLINK;
        if(receiver && net_upper_profile_link(link) && c->uart_policy[i].candidate_time_policy_selected) {
            NetWireStatus status=nw_uart_stream_config_check(c->profile,link,direction,&c->uart_policy[i]);
            if(status!=NET_WIRE_OK)return net_upper_wire_status(status);
        }
    }
    memset(s,0,sizeof(*s)); config_commit(s,c); s->next_token=1u; s->next_receipt=1u;
    for(i=0u;i<2u;++i) {
        s->next_uart_sequence[i]=c->uart_initial_sequence[i];
        NetWireLink link=i==0u?NET_WIRE_UART_M:NET_WIRE_UART_C;
        bool receiver=i==0u ? (c->node==NET_WIRE_NODE_DOMAIN || c->node==NET_WIRE_NODE_ESP32) :
            (c->node==NET_WIRE_NODE_CIS || c->node==NET_WIRE_NODE_PI);
        NetWireDirection direction=(c->node==NET_WIRE_NODE_DOMAIN || c->node==NET_WIRE_NODE_CIS) ? NET_WIRE_UPLINK : NET_WIRE_DOWNLINK;
        if(receiver && net_upper_profile_link(link) && c->uart_policy[i].candidate_time_policy_selected) {
            (void)net_upper_uart_receiver_init(net_upper_receiver_mut(s,i),c->profile,link,direction,&c->uart_policy[i],c->port_epoch[link]);
            s->uart_enabled[i]=true;
        }
    }
    s->initialized=true; memcpy(s->init_tag,tag,sizeof(tag)); return NET_OK;
}
static bool output_alias(const NetUpper *s, const void *m,size_t size,
    const NetSourceStamp *source,const NetAgePolicy *policy,NetTxKey *out)
{
    return net_wire_ranges_overlap(s,sizeof(*s),m,size) ||
        net_wire_ranges_overlap(s,sizeof(*s),source,sizeof(*source)) ||
        net_wire_ranges_overlap(s,sizeof(*s),policy,sizeof(*policy)) ||
        net_wire_ranges_overlap(s,sizeof(*s),out,sizeof(*out)) ||
        net_wire_ranges_overlap(m,size,out,sizeof(*out)) ||
        net_wire_ranges_overlap(source,sizeof(*source),out,sizeof(*out)) ||
        net_wire_ranges_overlap(policy,sizeof(*policy),out,sizeof(*out));
}
static NetStatus queue_message(NetUpper *s, const NetUpperCanMessage *can,
    const NetUpperUartMessage *uart, const NetSourceStamp *source,
    const NetAgePolicy *policy, NetTxKey *out, bool inspect)
{
    size_t i; NetUpperTx local; NetUpperTx *slot=NULL; uint64_t now,age;
    NetWireLink link=can!=NULL?NET_WIRE_CAN:uart->link;
    uint16_t id=can!=NULL?can->id:uart->type;
    const NetWireRegistryEntry *r=net_wire_registry_find(link,id);
    NetStatus status;
    if(r==NULL || !net_upper_profile_link(link)) return NET_UNSUPPORTED;
    if(inspect && (!s->config.inspection_transport || !disabled(r,s->config.node))) return NET_HOLD;
    if(!inspect && disabled(r,s->config.node)) return NET_HOLD;
    if(r->sender!=s->config.node) return NET_REJECTED;
    if(s->link_failed[link] || s->quiescing[link]) return NET_HOLD;
    memset(&local,0,sizeof(local));local.link=link;
    local.packet.link=link;local.packet.direction=r->direction;
    status=net_upper_wire_status(can!=NULL ? net_upper_can_encode(s->config.profile,can,&local.packet.can) :
        net_upper_uart_encode(s->config.profile,uart,&local.packet.uart));
    if(status!=NET_OK) return status;
    if(source->original_age_ms!=(can!=NULL ? net_upper_can_max_age(can) : net_upper_uart_max_age(uart))) return NET_HOLD;
    if(can!=NULL && can->id==0x310u){status=vision_origin(s,can,source);if(status!=NET_OK)return status;}
    if(uart!=NULL) {
        status=context_check(s,uart,&local.packet.uart);if(status!=NET_OK)return status;
        status=uart_origin(s,uart,&local.packet.uart,source);if(status!=NET_OK)return status;
    }
    for(i=0u;i<NET_UPPER_TX_CAPACITY;++i) if(!s->tx[i].state.used) { slot=&s->tx[i]; break; }
    if(slot==NULL) return NET_NO_CAPACITY;
    if(s->next_token==0u) return NET_HOLD;
    status=net_upper_read_clock(s,&now);if(status!=NET_OK)return status;
    status=age_check(source,policy,now,&age);if(status!=NET_OK)return status;
    if((can!=NULL && can->id==0x080u) || (uart!=NULL && link==NET_WIRE_UART_C && id==0x62u))
        if(age>=500u) return NET_EXPIRED; /* original Power lifetime, never relay1500 */
    local.state.used=true;local.state.queued=true;
    local.state.key.epoch=s->config.local_epoch;local.state.key.token=s->next_token;
    local.state.local_status=NET_OK;local.source=*source;local.policy=*policy;
    if(can!=NULL) local.can=*can; else local.uart=*uart;
    local.projection_bound=(uart!=NULL &&
        ((link==NET_WIRE_UART_M && projection_id(uart->type)!=0u) ||
         (link==NET_WIRE_UART_C && uart->type==0x62u))) || (can!=NULL && can->id==0x310u);
    if(local.projection_bound) {
        local.source_link=can!=NULL?NET_WIRE_UART_C:NET_WIRE_CAN;
        local.source_port_epoch=s->config.port_epoch[local.source_link];
    }
    local.failure_notice_enabled=s->config.retain_failure_notices;
    *slot=local; ++s->next_token; INC(s->observation.tx_queued); observe_tx_depth(s);
    if(uart!=NULL && uart->link==NET_WIRE_UART_M && uart->type==0x01u) accept_context(s,&uart->data.m_context);
    /* Accepted restrictive Owner updates supersede still-queued older allows.
     * An in-flight immutable borrow cannot be revoked or rewritten. */
    if(can!=NULL && can->id==0x080u && can->data.domain_permission.vehicle_power_permission!=1u) {
        for(i=0u;i<NET_UPPER_TX_CAPACITY;++i) {
            NetUpperTx *old=&s->tx[i];
            if(old!=slot && old->state.queued && old->link==NET_WIRE_CAN && old->can.id==0x080u &&
               old->can.data.domain_permission.vehicle_power_permission==1u) net_upper_reject_queued(old,NET_STALE);
        }
    }
    *out=local.state.key;return NET_OK;
}
NetStatus net_upper_submit_can(NetUpper *s,const NetUpperCanMessage *m,
    const NetSourceStamp *source,const NetAgePolicy *p,NetTxKey *out)
{
    NetStatus status;
    if(s==NULL || !s->initialized || m==NULL || source==NULL || p==NULL || out==NULL ||
       output_alias(s,m,sizeof(*m),source,p,out))return NET_INVALID_ARGUMENT;
    if(s->in_call)return NET_BUSY;
    s->in_call=true;status=queue_message(s,m,NULL,source,p,out,false);s->in_call=false;return status;
}
NetStatus net_upper_submit_uart(NetUpper *s,const NetUpperUartMessage *m,
    const NetSourceStamp *source,const NetAgePolicy *p,NetTxKey *out)
{
    NetStatus status;
    if(s==NULL || !s->initialized || m==NULL || source==NULL || p==NULL || out==NULL ||
       output_alias(s,m,sizeof(*m),source,p,out))return NET_INVALID_ARGUMENT;
    if(s->in_call)return NET_BUSY;
    if(m->link<NET_WIRE_UART_M || m->link>NET_WIRE_UART_C)return NET_INVALID_ARGUMENT;
    if(s->config.uart_sequence_bound[(size_t)m->link-1u])return NET_HOLD;
    s->in_call=true;status=queue_message(s,NULL,m,source,p,out,false);s->in_call=false;return status;
}
/* UART LINK_SEQUENCE is "u16 LE, per direction frame order modulo 65536"
 * (docs/network/interface/03_COMMUNICATION/20_UART_M.md shared framing, also
 * the UART-C frame): after 65535 the next frame carries 0. CAN TX_SEQUENCE
 * keeps its own rule (no wrap). */
static uint32_t next_link_sequence(uint32_t used)
{
    return used>=UINT16_MAX ? 0u : used+1u;
}
NetStatus net_upper_submit_uart_auto(NetUpper *s,const NetUpperUartMessage *m,
    const NetSourceStamp *source,const NetAgePolicy *p,NetTxKey *out)
{
    NetUpperUartMessage local;NetStatus status;size_t i;
    if(s==NULL || !s->initialized || m==NULL || source==NULL || p==NULL || out==NULL ||
       m->link<NET_WIRE_UART_M || m->link>NET_WIRE_UART_C || output_alias(s,m,sizeof(*m),source,p,out))return NET_INVALID_ARGUMENT;
    if(s->in_call)return NET_BUSY;
    i=(size_t)m->link-1u;
    if(!s->config.uart_sequence_bound[i] || s->next_uart_sequence[i]>UINT16_MAX)return NET_HOLD;
    local=*m;local.sequence=(uint16_t)s->next_uart_sequence[i];
    s->in_call=true;status=queue_message(s,NULL,&local,source,p,out,false);s->in_call=false;
    if(status==NET_OK)s->next_uart_sequence[i]=next_link_sequence(s->next_uart_sequence[i]);
    return status;
}
NetStatus net_upper_submit_inspection_can(NetUpper *s,const NetUpperCanMessage *m,
    const NetSourceStamp *source,const NetAgePolicy *p,NetTxKey *out)
{
    NetStatus st;
    if(s==NULL || !s->initialized || m==NULL || source==NULL || p==NULL || out==NULL ||
       output_alias(s,m,sizeof(*m),source,p,out))return NET_INVALID_ARGUMENT;
    if(s->in_call)return NET_BUSY;
    s->in_call=true;st=queue_message(s,m,NULL,source,p,out,true);s->in_call=false;return st;
}
NetStatus net_upper_submit_inspection_uart(NetUpper *s,const NetUpperUartMessage *m,
    const NetSourceStamp *source,const NetAgePolicy *p,NetTxKey *out)
{
    NetUpperUartMessage local;NetStatus st;size_t i;
    if(s==NULL || !s->initialized || m==NULL || source==NULL || p==NULL || out==NULL ||
       m->link!=NET_WIRE_UART_M || output_alias(s,m,sizeof(*m),source,p,out))return NET_INVALID_ARGUMENT;
    if(s->in_call)return NET_BUSY;
    i=(size_t)m->link-1u;
    if(!s->config.uart_sequence_bound[i] || s->next_uart_sequence[i]>UINT16_MAX)return NET_HOLD;
    local=*m;local.sequence=(uint16_t)s->next_uart_sequence[i];
    s->in_call=true;st=queue_message(s,NULL,&local,source,p,out,true);s->in_call=false;
    if(st==NET_OK)s->next_uart_sequence[i]=next_link_sequence(s->next_uart_sequence[i]);
    return st;
}
NetStatus net_upper_cis_publish_rear(NetUpper *s,const NetWireCisRearRaw *rear,
    const NetSourceStamp *source,const NetAgePolicy *policy,NetTxKey *out)
{
    NetUpperCanMessage m;
    if(rear==NULL || out==NULL || net_wire_ranges_overlap(rear,sizeof(*rear),out,sizeof(*out)))return NET_INVALID_ARGUMENT;
    memset(&m,0,sizeof(m));m.id=0x0a0u;m.data.cis_rear=*rear;
    return net_upper_submit_can(s,&m,source,policy,out);
}
NetStatus net_upper_capture(NetUpper *s,const NetUpperEvent *e)
{
    size_t tail;
    if(s==NULL || !s->initialized || e==NULL || e->kind<NET_UPPER_CAN_RX || e->kind>NET_UPPER_PORT_ERROR ||
       e->link<NET_WIRE_CAN || e->link>NET_WIRE_UART_C || net_wire_ranges_overlap(s,sizeof(*s),e,sizeof(*e)))return NET_INVALID_ARGUMENT;
    if(s->in_call)return NET_BUSY;
    if(!net_upper_profile_link(e->link))return NET_UNSUPPORTED;
    if(s->rx_count==NET_UPPER_RX_CAPACITY){INC(s->observation.rx_dropped);return NET_NO_CAPACITY;}
    tail=(s->rx_head+s->rx_count)%NET_UPPER_RX_CAPACITY;s->rx[tail]=*e;++s->rx_count;INC(s->observation.rx_captured);
    observe_rx_depth(s);return NET_OK;
}
/* Sender runs (BOOT_ID). docs/network/interface/04_COMMON/10_DATA_AND_ID_RULES.md
 * (generation): an accepted change of the sending run invalidates that
 * sender's earlier context, and an ended run is not accepted again (A -> B ->
 * late A). Per receive key the runs it left are kept, NET_UPPER_ENDED_BOOTS
 * deep; the oldest one goes first. A BOOT that never ran on the key and is not
 * ended starts a new generation: it needs no TX_SEQUENCE relation to the old
 * run (a restarted sender counts from 1 again). A sender that restarts with
 * the same BOOT_ID is not recognized (VSS contract C4 (VSS_NODE_INTEGRATION.md):
 * BOOT_ID differs on every start). Structure errors are refused before this
 * point (decode, CAN TX_SEQUENCE 0), so they never move a key. */
static bool boot_ended(const NetUpperEndedBoots *e,uint32_t boot)
{
    size_t i;
    for(i=0u;i<e->count && i<NET_UPPER_ENDED_BOOTS;++i)if(e->boot[i]==boot)return true;
    return false;
}
static void end_boot(NetUpperEndedBoots *e,uint32_t boot)
{
    size_t i;
    if(boot_ended(e,boot))return;
    if(e->count<NET_UPPER_ENDED_BOOTS){e->boot[e->count]=boot;++e->count;return;}
    for(i=1u;i<NET_UPPER_ENDED_BOOTS;++i)e->boot[i-1u]=e->boot[i];
    e->boot[NET_UPPER_ENDED_BOOTS-1u]=boot;
}
/* CAN transport order per receive key (sender and CAN ID, registry route):
 * same accepted BOOT, TX_SEQUENCE strictly growing. Another BOOT that is not
 * ended starts a new generation (new_generation); the caller commits it only
 * together with an accepted frame. */
static NetStatus admission(const NetUpperAdmission *a,uint32_t boot,uint32_t sequence,bool *new_generation)
{
    *new_generation=false;
    if(!a->seen)return NET_OK;
    if(a->boot!=boot){*new_generation=true;return NET_OK;}
    if(a->sequence==sequence)return NET_DUPLICATE;
    if(sequence<a->sequence)return NET_STALE; /* wrap/epoch ordering is not inferred */
    return NET_OK;
}
static bool repeat_route(const NetUpper *s,size_t idx)
{
    size_t j=net_upper_admission_index(idx);
    return j<NET_UPPER_ADMISSION_CAPACITY && s->config.rx_repeat_delivery[j];
}
/* UART LINK_SEQUENCE runs modulo 65536 (next_link_sequence). Within the same
 * accepted boot a number is newer when it lies 1..2^15-1 ahead of the last
 * accepted one in that circle, so 65535 -> 0 is the next frame and a number
 * behind (or exactly opposite) is stale. */
static NetStatus uart_admission(const NetUpperAdmission *a,uint32_t boot,uint16_t sequence)
{
    uint16_t ahead;
    if(!a->seen)return NET_OK;
    if(a->boot!=boot)return NET_HOLD;
    ahead=(uint16_t)(sequence-(uint16_t)a->sequence);
    if(ahead==0u)return NET_DUPLICATE;
    if(ahead>=0x8000u)return NET_STALE;
    return NET_OK;
}
static bool can_raw_equal(const NetWireCanEnvelope *a,const NetWireCanEnvelope *b)
{
    return a->standard_id==b->standard_id && a->extended==b->extended &&
        a->fdf==b->fdf && a->brs==b->brs && a->rtr==b->rtr &&
        a->driver_error==b->driver_error && a->esi==b->esi && a->dlc==b->dlc &&
        a->length==b->length && a->length<=sizeof(a->bytes) &&
        memcmp(a->bytes,b->bytes,a->length)==0;
}
static NetStatus same_sequence(NetUpper *s,const NetUpperAdmission *a,
    uint64_t port_epoch,NetWireDirection direction,bool raw_equal)
{
    if(a->delivery.capture.port_epoch!=port_epoch)return NET_HOLD;
    if(a->raw_direction==direction && raw_equal)return NET_DUPLICATE;
    INC(s->observation.rx_conflicts);
    return NET_HOLD; /* No Owner call, receipt advance, AGE refresh or ledger commit. */
}
static NetStatus delivery_time(NetUpper *s,size_t idx,const NetUpperCapture *capture,
    uint64_t raw_age,uint64_t now,NetUpperDelivery *out)
{
    NetSourceStamp stamp;NetStatus status;uint64_t age;bool per_field;
    const NetWireRegistryEntry *route=net_wire_registry_at(idx);
    if(!capture->timestamp_known || capture->errors!=0u || capture->overflow!=0u ||
       capture->port_epoch!=s->config.port_epoch[net_wire_registry_at(idx)->link])return NET_HOLD;
    if(s->next_receipt==0u)return NET_HOLD;
    per_field=s->config.rx_per_field_age[net_upper_admission_index(idx)] &&
        !((route->link==NET_WIRE_CAN && (route->id_type==0x0a0u || route->id_type==0x080u)) ||
          (route->link==NET_WIRE_UART_C && route->id_type==0x62u));
    stamp.observation_id=s->next_receipt;stamp.observed_at_ms=capture->captured_at_ms;stamp.original_age_ms=per_field?0u:raw_age;
    status=age_check(&stamp,&s->config.rx_policy[net_upper_admission_index(idx)],now,&age);if(status!=NET_OK)return status;
    if(per_field){stamp.original_age_ms=raw_age;status=net_source_age(&stamp,now,&age);if(status!=NET_OK)return status;}
    memset(out,0,sizeof(*out));out->capture=*capture;out->delivered_at_ms=now;
    out->local_elapsed_ms=now-capture->captured_at_ms;out->age_ms=age;
    out->receipt_id=s->next_receipt;out->freshness_known=!per_field && raw_age<UINT16_MAX;
    out->per_field_age_required=true;
    return NET_OK;
}
/* Receive delivery (docs/network/interface/04_COMMON/10_DATA_AND_ID_RULES.md,
 * repeat and current quality; 02_NODES/CIS/20_API.md receive delivery):
 *   - an exact duplicate (same accepted BOOT, same TX_SEQUENCE / LINK_SEQUENCE,
 *     same raw) refreshes nothing (same_sequence);
 *   - a newer transport sequence carrying the same original observation is a
 *     repetition. On a route with rx_repeat_delivery it is passed to the Owner
 *     with the receipt of that observation (NetUpperDelivery.repeat) and counted
 *     as repeat_delivered; elsewhere it refreshes the receipt record and AGE
 *     without an Owner call (C2 is not republished, 04_COMMON/
 *     20_API_USAGE_AND_FAILURE.md receive delivery) and ends as a duplicate;
 *   - anything else is a new observation (new receipt).
 * *repeat_delivered is set when a repetition reached the Owner. */
static NetStatus receive_can(NetUpper *s,const NetUpperEvent *event,uint64_t now,bool *repeat_delivered)
{
    size_t idx=route_index(NET_WIRE_CAN,(uint16_t)event->can.standard_id);
    const NetWireRegistryEntry *r;NetUpperCanMessage m;NetUpperDelivery d;NetStatus status;NetUpperAdmission *record;
    uint32_t boot,sequence;bool same_observation=false,repeat=false,raised=false,new_generation=false;
    *repeat_delivered=false;
    if(event->link!=NET_WIRE_CAN || idx>=NET_UPPER_ROUTE_COUNT)return NET_REJECTED;
    r=net_wire_registry_at(idx);
    if(disabled(r,s->config.node) && !s->config.inspection_transport)return NET_HOLD;
    if((r->receivers&s->config.node)==0u)return NET_REJECTED;
    record=net_upper_admission_at_mut(s,idx);if(record==NULL)return NET_REJECTED;
    status=net_upper_wire_status(net_upper_can_decode(s->config.profile,&event->can,&m));if(status!=NET_OK)return status;
    status=net_upper_wire_status(net_upper_can_identity(&m,&boot,&sequence));if(status!=NET_OK)return status;
    /* TX_SEQUENCE starts at 1 in every sender run and grows per frame
     * (docs/network/interface/03_COMMUNICATION/10_CAN_FD.md, TX_SEQUENCE:
     * first 1), so 0 is a reserved value and never sent. A frame carrying 0
     * is a structure error, refused before the run, order and receipt rules:
     * it never starts a generation and never reaches the Owner (VSS contract
     * C5 (VSS_NODE_INTEGRATION.md): structure checks are Network's first; a
     * structure error moves no generation). */
    if(sequence==0u)return NET_REJECTED;
    if(!event->capture.timestamp_known || event->capture.errors!=0u || event->capture.overflow!=0u ||
       event->capture.captured_at_ms>now)return NET_HOLD;
    if(record->seen && record->boot!=boot && boot_ended(&record->ended,boot)) {
        /* Late frame of an ended run: no Owner call, no receipt, no AGE refresh. */
        INC(s->observation.old_generation);return NET_STALE;
    }
    status=admission(record,boot,sequence,&new_generation);
    if(status==NET_DUPLICATE)return same_sequence(s,record,event->capture.port_epoch,
        r->direction,can_raw_equal(&record->raw.can,&event->can));
    if(status!=NET_OK)return status;
    status=delivery_time(s,idx,&event->capture,net_upper_can_max_age(&m),now,&d);if(status!=NET_OK)return status;
    /* A new generation is compared with nothing of the run it replaces. */
    if(record->seen && !new_generation) {
        const NetUpperAdmission *a=record;uint64_t elapsed;
        if(event->capture.captured_at_ms<a->delivery.capture.captured_at_ms)return NET_HOLD;
        elapsed=event->capture.captured_at_ms-a->delivery.capture.captured_at_ms;
        same_observation=net_upper_can_same_observation(&a->can,&m);
        repeat=same_observation && repeat_route(s,idx) && !disabled(r,s->config.node);
        status=net_upper_can_original_age(&a->can,&m,elapsed);
        if(status==NET_HOLD && repeat) {
            /* Late repetition: AGE raised to the bound within the slack, then
             * every other rule of the same observation still applies. */
            status=net_upper_can_repeat_age(&a->can,&m,elapsed,NET_UPPER_CAN_REPEAT_SLACK_MS,&raised);
            if(status==NET_OK)status=net_upper_can_original_age(&a->can,&m,elapsed);
            if(status==NET_OK)status=delivery_time(s,idx,&event->capture,net_upper_can_max_age(&m),now,&d);
        }
        if(status!=NET_OK)return status;
    }
    if((m.id==0x0a0u || m.id==0x080u) && net_upper_can_max_age(&m)==UINT16_MAX)return NET_HOLD;
    if(m.id==0x080u && d.age_ms>=500u)return NET_EXPIRED;
    d.source_boot=boot;d.source_sequence=sequence;
    d.observation_only=disabled(r,s->config.node);
    if(same_observation)d.receipt_id=record->delivery.receipt_id;
    d.repeat=repeat;
    status=same_observation && !repeat?NET_OK:d.observation_only ? (s->config.owner.held_can==NULL?NET_HOLD:
        s->config.owner.held_can(s->config.owner.context,&m,&d)) : net_upper_deliver_can(&s->config.owner,&m,&d);
    if(status!=NET_OK)INC(s->observation.consumer_rejected);
    if(status==NET_OK) {
        NetUpperAdmission *a=record;
        if(new_generation){end_boot(&a->ended,a->boot);INC(s->observation.generation_changes);}
        a->seen=true;a->boot=boot;a->sequence=sequence;a->can=m;a->delivery=d;
        a->raw.can=event->can;a->raw_direction=r->direction;if(!same_observation)++s->next_receipt;
        if(raised)INC(s->observation.repeat_age_raised);
    }
    if(status==NET_OK && repeat){*repeat_delivered=true;return NET_OK;}
    if(status==NET_OK && same_observation)return NET_DUPLICATE;
    return status==NET_OK && d.observation_only?NET_HOLD:status;
}
/* AGE the UART delivery bound is checked against (the record's own AGE). */
static uint64_t uart_rx_age(const NetUpperUartMessage *m)
{
    if(m->link==NET_WIRE_UART_C) {
        if(m->type==0x60u)return m->data.p_vision_result.vision_age_ms;
        if(m->type==0x61u)return m->data.p_vision_status.status_age_ms;
        return m->data.p_permission.source_age_ms;
    }
    return net_upper_uart_max_age(m);
}
/* u is the step's handoff buffer: a raised repetition AGE is written into
 * u->message before the Owner sees it (the raw frame stays as received). */
static NetStatus receive_uart(NetUpper *s,NetUpperUartDelivery *u,uint64_t now,bool *repeat_delivered)
{
    NetUpperUartMessage *m=&u->message;size_t idx=route_index(m->link,m->type);
    const NetWireRegistryEntry *r;NetUpperCapture capture;NetUpperDelivery d;NetStatus status;NetUpperAdmission *record;
    uint32_t boot=0u;bool new_context=false,same_observation=false,repeat=false,raised=false;
    bool new_generation=false,rebase=false;
    *repeat_delivered=false;
    if(idx>=NET_UPPER_ROUTE_COUNT)return NET_UNSUPPORTED;
    if(s->link_failed[m->link] || s->quiescing[m->link])return NET_HOLD;
    r=net_wire_registry_at(idx);
    if(disabled(r,s->config.node) && !s->config.inspection_transport)return NET_HOLD;
    if((r->receivers&s->config.node)==0u)return NET_REJECTED;
    record=net_upper_admission_at_mut(s,idx);if(record==NULL)return NET_REJECTED;
    status=context_check(s,m,&u->frame);if(status!=NET_OK)return status;
    if(m->link==NET_WIRE_UART_C) {
        if(m->type==0x60u)boot=m->data.p_vision_result.pi_boot_id;
        else if(m->type==0x61u)boot=m->data.p_vision_status.pi_boot_id;
        else boot=m->data.p_permission.cis_boot_id;
    } else if(m->type==0x01u) new_context=!s->context_seen || !context_equal(&s->context,&m->data.m_context);
    else if(r->direction==NET_WIRE_DOWNLINK)boot=u32(u->frame.bytes+(m->type==0x02u?8u:20u));
    if(!new_context){
        const NetUpperAdmission *a=record;
        if(m->link==NET_WIRE_UART_C) {
            /* UART-C sender run (Pi BOOT for the Pi -> CIS direction), judged
             * before the LINK_SEQUENCE window: a restarted sender counts its
             * LINK_SEQUENCE from the start again. The frame passed CRC, length
             * and format (M06) and its BOOT is nonzero (codec). */
            const NetUpperLinkGeneration *g=&s->uart_c_generation;
            if(g->seen && boot!=g->boot) {
                if(boot_ended(&g->ended,boot)){INC(s->observation.old_generation);return NET_STALE;}
                new_generation=true;
            }
            /* This record last took a frame of an earlier run: its LINK_SEQUENCE
             * and original observation are not comparable; this frame is its
             * new baseline. */
            rebase=new_generation || (a->seen && a->boot!=boot);
        }
        if(!rebase) {
            status=uart_admission(a,boot,m->sequence);
            if(status==NET_DUPLICATE)return same_sequence(s,a,u->port_epoch,r->direction,
                a->raw.uart.length==u->frame.length && u->frame.length<=sizeof(u->frame.bytes) &&
                memcmp(a->raw.uart.bytes,u->frame.bytes,u->frame.length)==0);
            if(status!=NET_OK)return status;
        }
    }
    memset(&capture,0,sizeof(capture));capture.port_epoch=u->port_epoch;
    capture.captured_at_ms=u->frame.captured_at_ms;capture.timestamp_known=true;
    status=delivery_time(s,idx,&capture,uart_rx_age(m),now,&d);if(status!=NET_OK)return status;
    if(!new_context && !rebase && record->seen) {
        const NetUpperAdmission *a=record;uint64_t elapsed;
        if(capture.captured_at_ms<a->delivery.capture.captured_at_ms)return NET_HOLD;
        elapsed=capture.captured_at_ms-a->delivery.capture.captured_at_ms;
        repeat=repeat_route(s,idx) && !disabled(r,s->config.node) && net_upper_uart_same_source(&a->uart,m);
        same_observation=repeat || net_upper_uart_same_observation(&a->uart,m);
        status=net_upper_uart_original_age(&a->uart,m,elapsed);
        if(status==NET_HOLD && repeat) {
            status=net_upper_uart_repeat_age(&a->uart,m,elapsed,NET_UPPER_UART_REPEAT_SLACK_MS,&raised);
            if(status==NET_OK)status=net_upper_uart_original_age(&a->uart,m,elapsed);
            if(status==NET_OK)status=delivery_time(s,idx,&capture,uart_rx_age(m),now,&d);
        }
        if(status!=NET_OK)return status;
    }
    if(m->link==NET_WIRE_UART_M && (m->type==0x10u || m->type==0x11u || m->type==0x12u || m->type==0x40u))d.freshness_known=false;
    if(m->link==NET_WIRE_UART_C && m->type==0x62u && d.age_ms>=500u)return NET_EXPIRED;
    d.source_boot=boot;d.source_sequence=m->sequence;
    d.observation_only=disabled(r,s->config.node);
    if(same_observation)d.receipt_id=record->delivery.receipt_id;
    d.repeat=repeat;
    status=same_observation && !repeat?NET_OK:d.observation_only ? (s->config.owner.held_uart==NULL?NET_HOLD:
        s->config.owner.held_uart(s->config.owner.context,u,&d)) :
        (s->config.owner.uart==NULL?NET_HOLD:s->config.owner.uart(s->config.owner.context,u,&d));
    if(status!=NET_OK)INC(s->observation.consumer_rejected);
    if(status==NET_OK) {
        if(m->link==NET_WIRE_UART_M && m->type==0x01u)accept_context(s,&m->data.m_context);
        if(m->link==NET_WIRE_UART_C) {
            NetUpperLinkGeneration *g=&s->uart_c_generation;
            if(new_generation){end_boot(&g->ended,g->boot);INC(s->observation.generation_changes);}
            g->seen=true;g->boot=boot;
        }
        record->seen=true;record->boot=boot;record->sequence=m->sequence;
        record->uart=*m;record->delivery=d;
        record->raw.uart=u->frame;record->raw_direction=r->direction;if(!same_observation)++s->next_receipt;
        if(raised)INC(s->observation.repeat_age_raised);
    }
    if(status==NET_OK && repeat){*repeat_delivered=true;return NET_OK;}
    if(status==NET_OK && same_observation)return NET_DUPLICATE;
    return status==NET_OK && d.observation_only?NET_HOLD:status;
}
static void observe_receive(NetUpper *s,NetStatus status,bool repeat_delivered)
{
    if(repeat_delivered)INC(s->observation.repeat_delivered); /* not a new observation */
    else if(status==NET_OK)INC(s->observation.rx_delivered);
    else if(status==NET_DUPLICATE)INC(s->observation.duplicates);
    else if(status==NET_STALE)INC(s->observation.stale);
    else if(status==NET_HOLD || status==NET_EXPIRED)INC(s->observation.rx_held);
    else if(status==NET_REJECTED || status==NET_INVALID_ARGUMENT || status==NET_UNSUPPORTED)INC(s->observation.rx_invalid);
}
static void pin_unresolved(NetUpper *s,NetWireLink link)
{
    size_t i;
    for(i=0u;i<NET_UPPER_TX_CAPACITY;++i) {
        NetUpperTx *r=&s->tx[i];
        if(r->state.used && r->link==link && r->state.submitted && !r->state.conflict &&
           (!r->state.admission_known || !r->state.reference_released ||
            (r->state.accepted && r->state.outcome==NET_TX_UNKNOWN))) {
            /* A link fault/reset cannot establish the fate of a live borrow.
             * Preserve all observed facts; pin uncertainty without fake final/release. */
            r->state.conflict=true;net_upper_failure_changed(r);
        }
    }
}
static NetStatus reset_link(NetUpper *s,const NetUpperEvent *e)
{
    size_t i;NetWireLink link=e->link;
    if(e->capture.port_epoch<=s->config.port_epoch[link])return NET_STALE;
    for(i=0u;i<NET_UPPER_TX_CAPACITY;++i) {
        NetUpperTx *r=&s->tx[i];
        if(r->state.used && r->link==link) {
            if(r->state.queued)net_upper_reject_queued(r,NET_HOLD);
        }
    }
    pin_unresolved(s,link);
    s->config.port_epoch[link]=e->capture.port_epoch;s->link_failed[link]=false;
    if(link!=NET_WIRE_CAN && s->uart_enabled[(size_t)link-1u])
        (void)net_upper_uart_receiver_signal(net_upper_receiver_mut(s,(size_t)link-1u),e->capture.port_epoch,false,true);
    if(link==NET_WIRE_UART_M)s->context_seen=false;
    INC(s->observation.resets);return NET_OK;
}
static NetStatus process_event(NetUpper *s,const NetUpperEvent *e,uint64_t now,bool *pop,
    NetUpperUartDelivery *delivery)
{
    NetStatus status;*pop=true;
    if(e->kind==NET_UPPER_TX_EVENT) {
        NetUpperTx *r=net_upper_find_tx(s,e->tx.key);
        if(r!=NULL && r->link!=e->link)return NET_REJECTED;
        return net_upper_observe_tx(s,&e->tx);
    }
    if(e->kind==NET_UPPER_LINK_RESET)return reset_link(s,e);
    if(e->kind==NET_UPPER_PORT_ERROR){
        s->link_failed[e->link]=true;pin_unresolved(s,e->link);
        INC(s->observation.errors);return NET_HOLD;
    }
    if(s->quiescing[e->link] || s->link_failed[e->link])return NET_HOLD;
    if(e->capture.port_epoch!=s->config.port_epoch[e->link])return NET_STALE;
    if(e->kind==NET_UPPER_LINK_LOSS) {
        if(e->link==NET_WIRE_UART_M)s->context_seen=false;
        if(e->link!=NET_WIRE_CAN && s->uart_enabled[(size_t)e->link-1u])
            (void)net_upper_uart_receiver_signal(net_upper_receiver_mut(s,(size_t)e->link-1u),e->capture.port_epoch,true,false);
        INC(s->observation.rx_dropped);return NET_HOLD;
    }
    if(e->kind==NET_UPPER_CAN_RX){bool repeat;status=receive_can(s,e,now,&repeat);observe_receive(s,status,repeat);return status;}
    if(e->kind==NET_UPPER_UART_BYTE && e->link!=NET_WIRE_CAN) {
        NetWireStatus ws;size_t i=(size_t)e->link-1u;bool consumed=false,repeat=false;
        if(!s->uart_enabled[i])return NET_HOLD;
        if(e->capture.errors!=0u || e->capture.overflow!=0u) {
            (void)net_upper_uart_receiver_signal(net_upper_receiver_mut(s,i),e->capture.port_epoch,true,false);INC(s->observation.rx_dropped);return NET_HOLD;
        }
        ws=net_upper_uart_receiver_feed(net_upper_receiver_mut(s,i),e->byte,e->capture.captured_at_ms,now,
            e->capture.timestamp_known,e->capture.port_epoch,&consumed,delivery);
        *pop=consumed;
        if(ws==NET_WIRE_TIME_HOLD || ws==NET_WIRE_INVALID_ARGUMENT) {
            *pop=true;(void)net_upper_uart_receiver_signal(net_upper_receiver_mut(s,i),e->capture.port_epoch,true,false);
        }
        status=ws==NET_WIRE_OK?receive_uart(s,delivery,now,&repeat):net_upper_wire_status(ws);
        observe_receive(s,status,repeat);return status;
    }
    return NET_INVALID_ARGUMENT;
}
static NetStatus dispatch(NetUpper *s,NetUpperTx *r,uint64_t now)
{
    uint64_t age,elapsed,limit,remaining;bool equal;
    NetStatus status=age_check(&r->source,&r->policy,now,&age);
    if(status!=NET_OK)goto reject;
    if(s->link_failed[r->link] || s->quiescing[r->link]){status=NET_HOLD;goto reject;}
    if(r->projection_bound &&
       (r->source_port_epoch!=s->config.port_epoch[r->source_link] ||
        s->link_failed[r->source_link] || s->quiescing[r->source_link])){status=NET_HOLD;goto reject;}
    elapsed=now-r->source.observed_at_ms;
    if(r->link==NET_WIRE_CAN) {
        NetUpperCanMessage m=r->can;
        if(m.id==0x080u && age>=500u){status=NET_EXPIRED;goto reject;}
        status=net_upper_wire_status(net_upper_can_add_age(&m,elapsed));if(status!=NET_OK)goto reject;
        status=net_upper_wire_status(net_upper_can_encode(s->config.profile,&m,&r->packet.can));
    } else {
        NetUpperUartMessage m=r->uart;
        if(m.link==NET_WIRE_UART_C && m.type==0x62u && age>=500u){status=NET_EXPIRED;goto reject;}
        if(m.link==NET_WIRE_UART_C && m.type==0x62u) {
            const NetUpperAdmission *a=net_upper_admission_at(s,route_index(NET_WIRE_CAN,0x080u));
            if(!receipt_matches(s,a,&r->source)){status=NET_STALE;goto reject;}
        }
        status=net_upper_wire_status(net_upper_uart_add_age(&m,elapsed));if(status!=NET_OK)goto reject;
        status=net_upper_wire_status(net_upper_uart_encode(s->config.profile,&m,&r->packet.uart));if(status!=NET_OK)goto reject;
        status=context_check(s,&m,&r->packet.uart);
    }
    if(status!=NET_OK)goto reject;
    limit=r->policy.maximum_age_ms;equal=r->policy.expires_at_equality;
    if((r->link==NET_WIRE_CAN && r->can.id==0x080u) ||
       (r->link==NET_WIRE_UART_C && r->uart.type==0x62u)) {
        if(limit>=500u){limit=500u;equal=true;}
    }
    if(age>limit || (equal && age==limit)){status=NET_EXPIRED;goto reject;}
    remaining=limit-age;
    /* A mathematical expiry beyond this monotonic epoch is unreachable.
     * Clamp inclusively; do not reject no-TTL control records such as QueryEnd.
     * A representable exact UINT64_MAX boundary retains its original rule. */
    if(remaining>UINT64_MAX-now){r->packet.transmit_deadline_ms=UINT64_MAX;equal=false;}
    else r->packet.transmit_deadline_ms=now+remaining;
    r->packet.age_encoded_at_ms=now;
    r->packet.deadline_bound=true;r->packet.expires_at_equality=equal;
    r->state.queued=false;r->state.submitted=true;
    status=s->config.port.ops->submit(s->config.port.context,&r->packet,r->state.key);
    if(status!=NET_OK){r->state.submitted=false;goto reject;}
    INC(s->observation.tx_submitted);return NET_OK;
reject:
    net_upper_reject_queued(r,status);return status;
}
NetStatus net_upper_step(NetUpper *s,size_t budget)
{
    size_t i;uint64_t now;NetStatus result=NET_NO_WORK,status;bool drained=false;
    /* One automatic handoff buffer shared by sequential RX/poll paths. The
     * serialized service never nests a second delivery-sized stack object. */
    NetUpperUartDelivery delivery;
    if(s==NULL || !s->initialized || budget>NET_UPPER_RX_CAPACITY)return NET_INVALID_ARGUMENT;
    if(s->in_call)return NET_BUSY;
    s->in_call=true;status=net_upper_read_clock(s,&now);if(status!=NET_OK){result=status;goto done;}
    for(i=0u;i<NET_UPPER_POLL_BUDGET && s->rx_count<NET_UPPER_RX_CAPACITY;++i) {
        NetUpperEvent e;size_t tail;status=s->config.port.ops->poll(s->config.port.context,&e);
        if(status==NET_NO_WORK){drained=true;break;}
        if(status!=NET_OK){INC(s->observation.errors);result=status;break;}
        if(e.kind<NET_UPPER_CAN_RX || e.kind>NET_UPPER_PORT_ERROR || e.link<NET_WIRE_CAN || e.link>NET_WIRE_UART_C || !net_upper_profile_link(e.link)) {
            INC(s->observation.rx_invalid);result=NET_INVALID_ARGUMENT;continue;
        }
        tail=(s->rx_head+s->rx_count)%NET_UPPER_RX_CAPACITY;s->rx[tail]=e;++s->rx_count;INC(s->observation.rx_captured);
        observe_rx_depth(s);
    }
    for(i=0u;i<budget && s->rx_count!=0u;++i) {
        bool pop;status=process_event(s,&s->rx[s->rx_head],now,&pop,&delivery);
        if(pop){s->rx_head=(s->rx_head+1u)%NET_UPPER_RX_CAPACITY;--s->rx_count;}
        if(result==NET_NO_WORK || result==NET_OK)result=status;
    }
    if(drained && s->rx_count==0u) {
        for(i=0u;i<2u;++i)if(s->uart_enabled[i] && !s->quiescing[i+1u] && !s->link_failed[i+1u]) {
            NetWireStatus ws=net_upper_uart_receiver_poll(net_upper_receiver_mut(s,i),now,s->config.port_epoch[i+1u],&delivery);
            if(ws==NET_WIRE_OK){bool repeat;status=receive_uart(s,&delivery,now,&repeat);observe_receive(s,status,repeat);if(result==NET_NO_WORK)result=status;}
        }
    }
    /* Token order is enqueue order, independent of reclaimed slot index. */
    for(i=0u;i<NET_UPPER_TX_CAPACITY;++i) {
        size_t j;NetUpperTx *next=NULL;
        for(j=0u;j<NET_UPPER_TX_CAPACITY;++j)
            if(s->tx[j].state.queued && (next==NULL || s->tx[j].state.key.token<next->state.key.token))next=&s->tx[j];
        if(next==NULL)break;
        status=dispatch(s,next,now);if(result==NET_NO_WORK || result==NET_OK)result=status;
    }
done:
    s->in_call=false;return result;
}
NetStatus net_upper_received_can(const NetUpper *s,uint16_t id,NetUpperCanMessage *out,
    NetUpperDelivery *delivery,NetSourceStamp *source)
{
    size_t idx=route_index(NET_WIRE_CAN,id);const NetUpperAdmission *a;
    if(s==NULL || !s->initialized || out==NULL || delivery==NULL || source==NULL || idx>=NET_UPPER_ROUTE_COUNT ||
       net_wire_ranges_overlap(s,sizeof(*s),out,sizeof(*out)) || net_wire_ranges_overlap(s,sizeof(*s),delivery,sizeof(*delivery)) ||
       net_wire_ranges_overlap(s,sizeof(*s),source,sizeof(*source)) || net_wire_ranges_overlap(out,sizeof(*out),delivery,sizeof(*delivery)) ||
       net_wire_ranges_overlap(out,sizeof(*out),source,sizeof(*source)) || net_wire_ranges_overlap(delivery,sizeof(*delivery),source,sizeof(*source)))return NET_INVALID_ARGUMENT;
    a=net_upper_admission_at(s,idx);if(a==NULL || !a->seen)return NET_NO_WORK;
    if(a->delivery.observation_only || a->delivery.capture.port_epoch!=s->config.port_epoch[NET_WIRE_CAN] ||
       s->link_failed[NET_WIRE_CAN] || s->quiescing[NET_WIRE_CAN])return NET_HOLD;
    *out=a->can;*delivery=a->delivery;source->observation_id=a->delivery.receipt_id;
    source->observed_at_ms=a->delivery.capture.captured_at_ms;source->original_age_ms=net_upper_can_max_age(&a->can);return NET_OK;
}
