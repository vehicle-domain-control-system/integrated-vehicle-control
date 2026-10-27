#include "network/net_domain_c2.h"
#include "net_domain_private.h"
#include <stdio.h>
#include <string.h>
/* DOMAIN C2 adapter, DOMAIN_PERMISSION (0x080) transmit: the Network
 * synthesized CAN UPDATE_SEQUENCE and the UNKNOWN power projection.
 * Expected values come from the contract, not from the engine:
 *   - docs/network/interface/04_COMMON/10_DATA_AND_ID_RULES.md, number spaces
 *     of the original success evaluation, the synthesis and the transmission:
 *     the CAN UPDATE_SEQUENCE is Network's own number, renewed for a new
 *     original evaluation or an actual change of meaning or quality
 *     restriction; DOMAIN keeps its success sequence on a failed evaluation;
 *     a withdrawal is not deferred to the next success or the 500 ms life;
 *   - 07_CROSS_NODE_INTERFACE_RULES.md, permission adapter: a repetition never
 *     renews an old ALLOW;
 *   - 03_COMMUNICATION/10_CAN_FD.md DOMAIN_PERMISSION: a confirmed ALLOW /
 *     DENY is 1 / 0 with OK, an UNKNOWN power is 255 with a non-OK quality;
 *     an unknown Operation meaning is UNKNOWN with a non-normal quality.
 * The DOMAIN side runs the product service, codec and adapter on a fake Port;
 * every frame it hands to the Port is fed to a receiving CIS service (product
 * code, CIS receive policy of 0x080: 500 ms bound, repetitions delivered), so
 * the cases also show what a receiver does with the bytes. The C2 Owner is
 * faked (IsInitialized, SetTxPort) and the test bundle copies the C2 power
 * value as 0 / 1 (the adapter projects UNKNOWN), unless a case binds the
 * provisional bundle. One case per process. */
#define CHECK(x) do{if(!(x)){fprintf(stderr,"%s:%d: %s\n",case_name,__LINE__,#x);return 1;}}while(0)
#define MAX_FRAMES 32u
#define DOMAIN_BOOT 301u
static const char *case_name;
static NetUpper dom, cis;
static NetDomainC2 adapter;
static DomainIf_TxPort_t ports;
static uint64_t now=1000u;
/* DOMAIN Port: every frame handed to it */
static NetUpperPacket sent[MAX_FRAMES];
static NetTxKey sent_key[MAX_FRAMES];
static size_t sent_count, settled;
/* CIS receiver */
static unsigned delivered;
static NetUpperCanMessage got;
static NetUpperDelivery got_meta;
/* Operation fields the test bundle reports (not part of the C2 power input) */
static uint8_t op_state=255u, op_allowed=0u, op_quality=3u;
static uint8_t bundle_power_override=0u; static bool override_power;

static bool initialized(void){return true;}
static DomainIf_Status_t set_port(const DomainIf_TxPort_t *p){if(p)ports=*p;else memset(&ports,0,sizeof(ports));return DOMAIN_IF_OK;}
static NetStatus clock_read(void *c,uint64_t *out){(void)c;*out=now;return NET_OK;}
static NetStatus dom_submit(void *c,const NetUpperPacket *p,NetTxKey k){(void)c;if(sent_count>=MAX_FRAMES)return NET_NO_CAPACITY;sent[sent_count]=*p;sent_key[sent_count]=k;++sent_count;return NET_OK;}
static NetStatus cis_submit(void *c,const NetUpperPacket *p,NetTxKey k){(void)c;(void)p;(void)k;return NET_OK;}
static NetStatus poll(void *c,NetUpperEvent *out){(void)c;(void)out;return NET_NO_WORK;}
static const NetUpperPortOps dom_ops={clock_read,dom_submit,poll};
static const NetUpperPortOps cis_ops={clock_read,cis_submit,poll};
static NetStatus cis_can(void *c,const NetUpperCanMessage *m,const NetUpperDelivery *d){(void)c;if(m->id==0x080u){got=*m;got_meta=*d;++delivered;}return NET_OK;}
/* The C2 power value as 0 / 1 (or a forced value), the C2 quality, the test's
 * Operation fields, meta.ageMs; UPDATE_SEQUENCE is junk: the adapter owns it. */
static NetStatus raw_bundle(void *c,const DomainIf_CisPowerPermission_t *in,NetWireDomainPermissionRaw *out){
    (void)c;memset(out,0,sizeof(*out));
    out->vehicle_power_permission=override_power?bundle_power_override:(in->vehiclePowerAllowed?1u:0u);
    switch(in->quality){case DATA_QUALITY_OK:out->power_permission_quality=0u;break;case DATA_QUALITY_STALE:out->power_permission_quality=1u;break;
    case DATA_QUALITY_INVALID:out->power_permission_quality=2u;break;case DATA_QUALITY_NO_DATA:out->power_permission_quality=3u;break;default:return NET_HOLD;}
    out->power_operation_state=op_state;out->operation_allowed=op_allowed;out->operation_quality=op_quality;
    out->update_sequence=0xDEAD0000u+in->meta.updateSequence;out->source_age_ms=(uint16_t)in->meta.ageMs;return NET_OK;}
static NetStatus window_bundle(void *c,const DomainIf_WindowOperationPermission_t *in,NetWireDomainPermissionRaw *out){
    (void)c;memset(out,0,sizeof(*out));out->vehicle_power_permission=255u;out->power_permission_quality=3u;out->power_operation_state=1u;
    out->operation_allowed=in->operationAllowed?1u:0u;out->operation_quality=0u;out->update_sequence=in->meta.updateSequence;out->source_age_ms=(uint16_t)in->meta.ageMs;return NET_OK;}
static NetStatus alive_bundle(void *c,const DomainIf_WindowDomainAlive_t *in,NetWireDomainPermissionRaw *out){
    (void)c;memset(out,0,sizeof(*out));out->vehicle_power_permission=255u;out->power_permission_quality=3u;out->power_operation_state=1u;
    out->operation_allowed=1u;out->operation_quality=0u;out->update_sequence=in->meta.updateSequence;out->source_age_ms=(uint16_t)in->meta.ageMs;return NET_OK;}

static size_t route(NetWireLink link,uint16_t id){size_t i;for(i=0u;i<NET_UPPER_ROUTE_COUNT;++i){const NetWireRegistryEntry *e=net_wire_registry_at(i);if(e!=NULL&&e->link==link&&e->id_type==id)return i;}return NET_UPPER_ROUTE_COUNT;}
static int start(void)
{
    NetDomainC2Api api;NetDomainC2Config cfg;NetUpperConfig c;size_t i,pidx=route(NET_WIRE_CAN,0x080u);
    CHECK(pidx<NET_UPPER_ROUTE_COUNT);
    memset(&api,0,sizeof(api));memset(&cfg,0,sizeof(cfg));
    api.IsInitialized=initialized;api.SetTxPort=set_port;
    cfg.domain_boot=DOMAIN_BOOT;cfg.bcm_boot=201u;cfg.first_can_sequence=1u;cfg.command_accept_limit_ms=100u;cfg.event_use_limit_ms=100u;
    cfg.transmit_policy.bound=true;cfg.transmit_policy.maximum_age_ms=1000u;cfg.transmit_policy.expires_at_equality=true;
    cfg.permission_bundle=raw_bundle;
    CHECK(net_domain_c2_init(&adapter,&dom,NULL,&api,&cfg)==NET_OK);
    memset(&c,0,sizeof(c));c.node=NET_WIRE_NODE_DOMAIN;c.local_epoch=3u;c.profile=NET_WIRE_CANDIDATE_ONLY;c.port.ops=&dom_ops;
    c.owner=net_domain_c2_owner(&adapter);
    for(i=0u;i<3u;++i)c.port_epoch[i]=4u;
    for(i=0u;i<NET_UPPER_ROUTE_COUNT;++i){c.rx_policy[i].bound=true;c.rx_policy[i].maximum_age_ms=10000u;}
    for(i=0u;i<2u;++i){c.uart_policy[i].candidate_time_policy_selected=true;c.uart_policy[i].assembly_limit_ms=100u;c.uart_policy[i].expires_at_equality=true;c.uart_sequence_bound[i]=true;c.uart_initial_sequence[i]=1u;}
    CHECK(net_upper_init(&dom,&c)==NET_OK);CHECK(net_domain_c2_register(&adapter)==NET_OK);
    /* Receiving CIS: 0x080 bound to the 500 ms original life, repetitions delivered (CIS node profile). */
    memset(&c,0,sizeof(c));c.node=NET_WIRE_NODE_CIS;c.local_epoch=5u;c.profile=NET_WIRE_CANDIDATE_ONLY;c.port.ops=&cis_ops;
    c.owner.can=cis_can;
    for(i=0u;i<3u;++i)c.port_epoch[i]=6u;
    for(i=0u;i<NET_UPPER_ROUTE_COUNT;++i){c.rx_policy[i].bound=true;c.rx_policy[i].maximum_age_ms=2000u;c.rx_policy[i].expires_at_equality=true;}
    c.rx_policy[pidx].maximum_age_ms=500u;c.rx_repeat_delivery[pidx]=true;
    for(i=0u;i<2u;++i){c.uart_policy[i].candidate_time_policy_selected=true;c.uart_policy[i].assembly_limit_ms=50u;c.uart_policy[i].expires_at_equality=true;c.uart_sequence_bound[i]=true;c.uart_initial_sequence[i]=1u;}
    CHECK(net_upper_init(&cis,&c)==NET_OK);
    return 0;
}
static DomainIf_CisPowerPermission_t perm(bool allowed,DataQuality_t q,uint32_t seq,uint32_t age)
{
    DomainIf_CisPowerPermission_t p;memset(&p,0,sizeof(p));
    p.vehiclePowerAllowed=allowed;p.quality=q;p.meta.quality=q;p.meta.updateSequence=seq;p.meta.ageMs=age;return p;
}
/* Settle every frame the DOMAIN Port took (sent, then released) and let the
 * adapter retire it; feed it to the CIS receiver. */
static int settle_and_deliver(void)
{
    for(;settled<sent_count;++settled){
        NetUpperEvent e;memset(&e,0,sizeof(e));
        e.kind=NET_UPPER_CAN_RX;e.link=NET_WIRE_CAN;e.capture.port_epoch=6u;e.capture.timestamp_known=true;e.capture.captured_at_ms=now;
        e.can=sent[settled].can;CHECK(net_upper_capture(&cis,&e)==NET_OK);(void)net_upper_step(&cis,NET_UPPER_RX_CAPACITY);
        memset(&e,0,sizeof(e));e.kind=NET_UPPER_TX_EVENT;e.link=NET_WIRE_CAN;e.capture.port_epoch=4u;e.capture.timestamp_known=true;e.capture.captured_at_ms=now;
        e.tx.key=sent_key[settled];e.tx.admission_observed=true;e.tx.accepted=true;e.tx.outcome=NET_TX_SUCCEEDED;e.tx.release_observed=true;
        CHECK(net_upper_capture(&dom,&e)==NET_OK);(void)net_upper_step(&dom,NET_UPPER_RX_CAPACITY);
    }
    (void)net_domain_c2_step(&adapter);
    return 0;
}
/* Submit through the C2 TxPort, dispatch at once, settle and deliver. */
static DomainIf_Status_t submit(const DomainIf_CisPowerPermission_t *p)
{
    DomainIf_Status_t s=ports.txCisPowerPermission(p);
    (void)net_upper_step(&dom,0u);
    if(settle_and_deliver()!=0)return (DomainIf_Status_t)0x7F;
    return s;
}
static bool frame(size_t k,NetWireDomainPermissionRaw *out)
{
    NetUpperCanMessage m;
    if(k>=sent_count||net_upper_can_decode(NET_WIRE_CANDIDATE_ONLY,&sent[k].can,&m)!=NET_WIRE_OK||m.id!=0x080u)return false;
    *out=m.data.domain_permission;return true;
}
static uint32_t synth(size_t k){NetWireDomainPermissionRaw r;return frame(k,&r)?r.update_sequence:0u;}
static const NetWireDomainPermissionRaw *rx(void){return &got.data.domain_permission;}

/* First frame: CAN UPDATE_SEQUENCE 1, not the DOMAIN success number 17; the
 * bundle's number is ignored. BOOT, TX_SEQUENCE, value, quality and AGE as
 * given; the receiver takes it. */
static int perm_first(void)
{
    DomainIf_CisPowerPermission_t p=perm(true,DATA_QUALITY_OK,17u,10u);NetWireDomainPermissionRaw r;
    CHECK(submit(&p)==DOMAIN_IF_OK);
    CHECK(sent_count==1u&&frame(0u,&r));
    CHECK(r.update_sequence==1u&&r.boot_id==DOMAIN_BOOT&&r.tx_sequence==1u);
    CHECK(r.vehicle_power_permission==1u&&r.power_permission_quality==0u&&r.source_age_ms==10u);
    CHECK(r.power_operation_state==255u&&r.operation_allowed==0u&&r.operation_quality==3u);
    CHECK(delivered==1u&&rx()->update_sequence==1u&&rx()->vehicle_power_permission==1u&&!got_meta.repeat);
    CHECK(adapter.permission.sent&&adapter.permission.sequence==1u&&adapter.permission.domain_sequence==17u);
    CHECK(adapter.permission.advanced==1u&&adapter.permission.kept==0u);
    return 0;
}
/* ALLOW (success 17) then the failed evaluation (DOMAIN keeps 17, false +
 * NO_DATA): a new CAN number, so the receiver delivers 255 + NO_DATA at once
 * (under number 1 it would hold the changed value of one observation). */
static int perm_withdraw_immediate(void)
{
    DomainIf_CisPowerPermission_t allow=perm(true,DATA_QUALITY_OK,17u,10u),off=perm(false,DATA_QUALITY_NO_DATA,17u,60u);
    NetWireDomainPermissionRaw r;
    CHECK(submit(&allow)==DOMAIN_IF_OK&&delivered==1u&&rx()->vehicle_power_permission==1u);
    now+=50u;
    CHECK(submit(&off)==DOMAIN_IF_OK);
    CHECK(sent_count==2u&&frame(1u,&r)&&r.update_sequence==2u&&r.tx_sequence==2u);
    CHECK(r.vehicle_power_permission==255u&&r.power_permission_quality==3u&&r.source_age_ms==60u);
    CHECK(delivered==2u&&rx()->vehicle_power_permission==255u&&rx()->power_permission_quality==3u);
    CHECK(rx()->update_sequence==2u&&!got_meta.repeat&&got_meta.age_ms==60u);
    CHECK(adapter.permission.advanced==2u&&adapter.permission.domain_sequence==17u);
    return 0;
}
/* The same tuple again 50 ms later (AGE +50): the same number, the next
 * TX_SEQUENCE; the receiver takes it as a repetition with the grown AGE. */
static int perm_repeat_kept(void)
{
    DomainIf_CisPowerPermission_t a=perm(true,DATA_QUALITY_OK,17u,10u),b=perm(true,DATA_QUALITY_OK,17u,60u);
    NetWireDomainPermissionRaw r;
    CHECK(submit(&a)==DOMAIN_IF_OK&&delivered==1u);
    now+=50u;
    CHECK(submit(&b)==DOMAIN_IF_OK);
    CHECK(sent_count==2u&&frame(1u,&r)&&r.update_sequence==1u&&r.tx_sequence==2u&&r.source_age_ms==60u);
    CHECK(delivered==2u&&got_meta.repeat&&rx()->update_sequence==1u&&rx()->source_age_ms==60u&&got_meta.age_ms==60u);
    now+=50u;b.meta.ageMs=110u;
    CHECK(submit(&b)==DOMAIN_IF_OK&&synth(2u)==1u&&delivered==3u&&got_meta.repeat);
    CHECK(adapter.permission.advanced==1u&&adapter.permission.kept==2u);
    return 0;
}
/* A value change under the same DOMAIN sequence (a restriction): + 1. */
static int perm_value_change_same_seq(void)
{
    DomainIf_CisPowerPermission_t p=perm(true,DATA_QUALITY_OK,17u,10u);NetWireDomainPermissionRaw r;
    CHECK(submit(&p)==DOMAIN_IF_OK&&synth(0u)==1u);
    p=perm(false,DATA_QUALITY_OK,17u,20u);                 /* power 1 -> 0 */
    CHECK(submit(&p)==DOMAIN_IF_OK&&frame(1u,&r)&&r.update_sequence==2u&&r.vehicle_power_permission==0u);
    CHECK(delivered==2u&&rx()->vehicle_power_permission==0u&&rx()->update_sequence==2u);
    op_state=0u;                                          /* POWER_OPERATION_STATE 255 -> 0 */
    CHECK(submit(&p)==DOMAIN_IF_OK&&frame(2u,&r)&&r.update_sequence==3u&&r.power_operation_state==0u);
    CHECK(delivered==3u&&rx()->power_operation_state==0u);
    CHECK(adapter.permission.advanced==3u&&adapter.permission.kept==0u);
    return 0;
}
/* A quality-only restriction under the same DOMAIN sequence: + 1 each time
 * (Operation OK -> STALE with the same value; power UNKNOWN NO_DATA -> STALE). */
static int perm_quality_restrict_same_seq(void)
{
    DomainIf_CisPowerPermission_t p=perm(false,DATA_QUALITY_OK,5u,10u);NetWireDomainPermissionRaw r;
    op_state=0u;op_allowed=0u;op_quality=0u;
    CHECK(submit(&p)==DOMAIN_IF_OK&&synth(0u)==1u);
    op_quality=1u;p.meta.ageMs=20u;                         /* Operation quality OK -> STALE, values the same */
    CHECK(submit(&p)==DOMAIN_IF_OK&&frame(1u,&r)&&r.update_sequence==2u&&r.operation_quality==1u&&r.operation_allowed==0u);
    CHECK(delivered==2u&&rx()->operation_quality==1u);
    p=perm(false,DATA_QUALITY_NO_DATA,5u,30u);              /* power DENY -> UNKNOWN */
    CHECK(submit(&p)==DOMAIN_IF_OK&&frame(2u,&r)&&r.update_sequence==3u&&r.vehicle_power_permission==255u);
    p=perm(false,DATA_QUALITY_STALE,5u,40u);                /* UNKNOWN NO_DATA -> STALE, value 255 both */
    CHECK(submit(&p)==DOMAIN_IF_OK&&frame(3u,&r)&&r.update_sequence==4u&&r.vehicle_power_permission==255u&&r.power_permission_quality==1u);
    CHECK(delivered==4u&&rx()->power_permission_quality==1u&&rx()->update_sequence==4u);
    CHECK(adapter.permission.advanced==4u&&adapter.permission.kept==0u);
    return 0;
}
/* A newer DOMAIN success sequence: + 1, also across the u32 wrap of the
 * DOMAIN number (serial order). */
static int perm_new_success(void)
{
    DomainIf_CisPowerPermission_t p=perm(true,DATA_QUALITY_OK,UINT32_MAX-1u,10u);
    CHECK(submit(&p)==DOMAIN_IF_OK&&synth(0u)==1u);
    p=perm(false,DATA_QUALITY_NO_DATA,UINT32_MAX-1u,20u);
    CHECK(submit(&p)==DOMAIN_IF_OK&&synth(1u)==2u&&rx()->vehicle_power_permission==255u);
    p=perm(true,DATA_QUALITY_OK,UINT32_MAX,0u);              /* new success evaluation */
    CHECK(submit(&p)==DOMAIN_IF_OK&&synth(2u)==3u&&delivered==3u&&rx()->vehicle_power_permission==1u);
    p=perm(true,DATA_QUALITY_OK,0u,0u);                      /* DOMAIN number wrapped: still newer */
    CHECK(submit(&p)==DOMAIN_IF_OK&&synth(3u)==4u&&adapter.permission.domain_sequence==0u);
    p=perm(true,DATA_QUALITY_OK,7u,0u);                      /* several evaluations later, same values */
    CHECK(submit(&p)==DOMAIN_IF_OK&&synth(4u)==5u);
    CHECK(delivered==5u&&rx()->update_sequence==5u&&rx()->vehicle_power_permission==1u);
    CHECK(adapter.permission.advanced==5u&&adapter.permission.held_older==0u);
    return 0;
}
/* After a withdrawal under DOMAIN sequence 17, ALLOW (or a confirmed DENY)
 * under 17 again would lift the restriction: HOLD, no frame. ALLOW comes back
 * only with 18. */
static int perm_relax_guard(void)
{
    DomainIf_CisPowerPermission_t p=perm(true,DATA_QUALITY_OK,17u,10u);
    CHECK(submit(&p)==DOMAIN_IF_OK);
    p=perm(false,DATA_QUALITY_NO_DATA,17u,20u);
    CHECK(submit(&p)==DOMAIN_IF_OK&&synth(1u)==2u&&delivered==2u);
    p=perm(true,DATA_QUALITY_OK,17u,30u);
    CHECK(submit(&p)==DOMAIN_IF_NOT_AVAILABLE&&sent_count==2u&&delivered==2u);
    p=perm(false,DATA_QUALITY_OK,17u,30u);                  /* NO_DATA -> OK: also a lifted restriction */
    CHECK(submit(&p)==DOMAIN_IF_NOT_AVAILABLE&&sent_count==2u);
    CHECK(adapter.permission.held_relax==2u&&adapter.permission.sequence==2u&&adapter.permission.power==255u);
    CHECK(rx()->vehicle_power_permission==255u);
    p=perm(true,DATA_QUALITY_OK,18u,0u);
    CHECK(submit(&p)==DOMAIN_IF_OK&&sent_count==3u&&synth(2u)==3u);
    CHECK(delivered==3u&&rx()->vehicle_power_permission==1u&&rx()->update_sequence==3u);
    /* Operation 0 -> 1 under one DOMAIN sequence is a lifted restriction as well. */
    op_allowed=1u;op_quality=0u;op_state=1u;
    CHECK(submit(&p)==DOMAIN_IF_NOT_AVAILABLE&&sent_count==3u&&adapter.permission.held_relax==3u);
    return 0;
}
/* An older DOMAIN sequence (behind, or exactly opposite in the u32 circle) is
 * HOLD and changes nothing. */
static int perm_older_domain_seq(void)
{
    DomainIf_CisPowerPermission_t p=perm(true,DATA_QUALITY_OK,18u,10u);
    CHECK(submit(&p)==DOMAIN_IF_OK&&synth(0u)==1u);
    p=perm(false,DATA_QUALITY_NO_DATA,17u,20u);
    CHECK(submit(&p)==DOMAIN_IF_NOT_AVAILABLE&&sent_count==1u);
    p=perm(true,DATA_QUALITY_OK,18u+0x80000000u,20u);
    CHECK(submit(&p)==DOMAIN_IF_NOT_AVAILABLE&&sent_count==1u);
    CHECK(adapter.permission.held_older==2u&&adapter.permission.domain_sequence==18u&&adapter.permission.sequence==1u);
    p=perm(true,DATA_QUALITY_OK,18u,30u);                   /* the current evaluation still repeats */
    CHECK(submit(&p)==DOMAIN_IF_OK&&synth(1u)==1u&&delivered==2u);
    return 0;
}
/* The same tuple with a smaller AGE: HOLD; the same AGE is a repetition. */
static int perm_age_backwards(void)
{
    DomainIf_CisPowerPermission_t p=perm(true,DATA_QUALITY_OK,17u,100u);
    CHECK(submit(&p)==DOMAIN_IF_OK&&synth(0u)==1u);
    p.meta.ageMs=90u;
    CHECK(submit(&p)==DOMAIN_IF_NOT_AVAILABLE&&sent_count==1u&&adapter.permission.held_age==1u);
    CHECK(adapter.permission.age_ms==100u);
    p.meta.ageMs=100u;
    CHECK(submit(&p)==DOMAIN_IF_OK&&synth(1u)==1u&&adapter.permission.kept==1u);
    return 0;
}
/* Next number: 1 after none, +1, and 1 after 0xFFFFFFFF (0 is never used). */
static int perm_wrap(void)
{
    DomainIf_CisPowerPermission_t p=perm(true,DATA_QUALITY_OK,17u,10u);
    CHECK(nd_permission_next(0u)==1u&&nd_permission_next(1u)==2u);
    CHECK(nd_permission_next(UINT32_MAX-1u)==UINT32_MAX&&nd_permission_next(UINT32_MAX)==1u);
    /* The adapter uses it: the last number sent was 0xFFFFFFFF (state set by
     * the fixture; no configuration sets a start value). */
    adapter.permission.sent=true;adapter.permission.sequence=UINT32_MAX;adapter.permission.domain_sequence=16u;
    adapter.permission.power=1u;adapter.permission.power_quality=0u;adapter.permission.operation_state=255u;
    adapter.permission.operation_quality=3u;
    CHECK(submit(&p)==DOMAIN_IF_OK&&synth(0u)==1u);
    p=perm(false,DATA_QUALITY_NO_DATA,17u,20u);
    CHECK(submit(&p)==DOMAIN_IF_OK&&synth(1u)==2u);
    return 0;
}
/* WINDOW is held: with the transport enabled and both WINDOW bundles bound,
 * neither WINDOW input sends a 0x080. The CIS power permission still does. */
static int perm_window_hold(void)
{
    DomainIf_WindowOperationPermission_t wp;DomainIf_WindowDomainAlive_t wa;DomainIf_CisPowerPermission_t p=perm(true,DATA_QUALITY_OK,17u,10u);
    memset(&wp,0,sizeof(wp));memset(&wa,0,sizeof(wa));
    adapter.config.window_transport_enabled=true;adapter.config.window_channel_bound=true;
    adapter.config.window_permission_bundle=window_bundle;adapter.config.window_alive_bundle=alive_bundle;
    wp.operationAllowed=true;wp.quality=DATA_QUALITY_OK;wp.meta.updateSequence=3u;wa.domainLinkAvailable=true;wa.meta.updateSequence=4u;
    CHECK(ports.txWindowOperationPermission(&wp)==DOMAIN_IF_NOT_AVAILABLE);
    CHECK(ports.txWindowDomainAlive(&wa)==DOMAIN_IF_NOT_AVAILABLE);
    (void)net_upper_step(&dom,0u);
    CHECK(sent_count==0u&&!adapter.permission.sent);
    CHECK(submit(&p)==DOMAIN_IF_OK&&sent_count==1u&&synth(0u)==1u);
    return 0;
}
/* A submission the service refuses (the original life of 500 ms is over)
 * uses no number: the next accepted frame carries the next number. */
static int perm_refused_not_consumed(void)
{
    DomainIf_CisPowerPermission_t p=perm(true,DATA_QUALITY_OK,17u,10u);
    CHECK(submit(&p)==DOMAIN_IF_OK&&synth(0u)==1u);
    p=perm(false,DATA_QUALITY_NO_DATA,17u,600u);
    CHECK(submit(&p)==DOMAIN_IF_REJECTED&&sent_count==1u);
    CHECK(adapter.permission.sequence==1u&&adapter.permission.power==1u&&adapter.permission.advanced==1u);
    p=perm(false,DATA_QUALITY_NO_DATA,17u,60u);
    CHECK(submit(&p)==DOMAIN_IF_OK&&sent_count==2u&&synth(1u)==2u);
    CHECK(delivered==2u&&rx()->update_sequence==2u&&rx()->vehicle_power_permission==255u);
    return 0;
}
/* UNKNOWN power on CAN: false + non-OK quality is 255 with that quality
 * (the bundle's 0 is replaced); true + non-OK is HOLD; false + OK is 0. A
 * bundle that says 1 or 255 against the C2 input is HOLD. */
static int perm_unknown_255(void)
{
    DomainIf_CisPowerPermission_t p;NetWireDomainPermissionRaw r;
    p=perm(false,DATA_QUALITY_NO_DATA,1u,0u);
    CHECK(submit(&p)==DOMAIN_IF_OK&&frame(0u,&r)&&r.vehicle_power_permission==255u&&r.power_permission_quality==3u);
    CHECK(delivered==1u&&rx()->vehicle_power_permission==255u&&rx()->power_permission_quality==3u);
    p=perm(false,DATA_QUALITY_STALE,2u,0u);
    CHECK(submit(&p)==DOMAIN_IF_OK&&frame(1u,&r)&&r.vehicle_power_permission==255u&&r.power_permission_quality==1u);
    p=perm(true,DATA_QUALITY_INVALID,3u,0u);
    CHECK(submit(&p)==DOMAIN_IF_NOT_AVAILABLE&&sent_count==2u);
    p=perm(false,DATA_QUALITY_OK,4u,0u);
    CHECK(submit(&p)==DOMAIN_IF_OK&&frame(2u,&r)&&r.vehicle_power_permission==0u&&r.power_permission_quality==0u);
    CHECK(delivered==3u&&rx()->vehicle_power_permission==0u);
    /* The bundle against the C2 input */
    override_power=true;bundle_power_override=1u;p=perm(false,DATA_QUALITY_NO_DATA,5u,0u);
    CHECK(submit(&p)==DOMAIN_IF_NOT_AVAILABLE&&sent_count==3u);
    bundle_power_override=255u;p=perm(false,DATA_QUALITY_OK,6u,0u);
    CHECK(submit(&p)==DOMAIN_IF_NOT_AVAILABLE&&sent_count==3u);
    p=perm(false,DATA_QUALITY_INVALID,7u,0u);                /* a bundle that already says 255 */
    CHECK(submit(&p)==DOMAIN_IF_OK&&frame(3u,&r)&&r.vehicle_power_permission==255u&&r.power_permission_quality==2u);
    return 0;
}
/* The provisional bundle bound as the integration would: ALLOW + OK ->
 * 1 / OK / 255 / 0 / NO_DATA, DENY + OK -> 0 / OK, UNKNOWN -> 255 / non-OK,
 * true + non-OK -> HOLD and no frame. */
static int perm_bundle_provisional(void)
{
    DomainIf_CisPowerPermission_t p;NetWireDomainPermissionRaw r,keep;
    adapter.config.permission_bundle=net_domain_c2_permission_bundle_provisional;
    p=perm(true,DATA_QUALITY_OK,1u,10u);
    CHECK(submit(&p)==DOMAIN_IF_OK&&frame(0u,&r));
    CHECK(r.vehicle_power_permission==1u&&r.power_permission_quality==0u&&r.power_operation_state==255u&&r.operation_allowed==0u&&r.operation_quality==3u);
    CHECK(r.update_sequence==1u&&r.source_age_ms==10u&&delivered==1u);
    p=perm(false,DATA_QUALITY_OK,2u,0u);
    CHECK(submit(&p)==DOMAIN_IF_OK&&frame(1u,&r)&&r.vehicle_power_permission==0u&&r.power_permission_quality==0u&&r.update_sequence==2u);
    p=perm(false,DATA_QUALITY_NO_DATA,2u,30u);
    CHECK(submit(&p)==DOMAIN_IF_OK&&frame(2u,&r)&&r.vehicle_power_permission==255u&&r.power_permission_quality==3u&&r.update_sequence==3u);
    CHECK(delivered==3u&&rx()->vehicle_power_permission==255u);
    p=perm(true,DATA_QUALITY_NO_DATA,3u,0u);
    CHECK(submit(&p)==DOMAIN_IF_NOT_AVAILABLE&&sent_count==3u);
    /* The function alone */
    memset(&keep,0x5a,sizeof(keep));r=keep;
    p=perm(true,DATA_QUALITY_STALE,4u,0u);
    CHECK(net_domain_c2_permission_bundle_provisional(NULL,&p,&r)==NET_HOLD&&memcmp(&r,&keep,sizeof(r))==0);
    p=perm(true,DATA_QUALITY_OK,4u,0u);p.meta.quality=DATA_QUALITY_STALE; /* quality and meta.quality disagree */
    CHECK(net_domain_c2_permission_bundle_provisional(NULL,&p,&r)==NET_HOLD&&memcmp(&r,&keep,sizeof(r))==0);
    p=perm(false,DATA_QUALITY_OK,4u,70000u);
    CHECK(net_domain_c2_permission_bundle_provisional(NULL,&p,&r)==NET_HOLD&&memcmp(&r,&keep,sizeof(r))==0);
    CHECK(net_domain_c2_permission_bundle_provisional(NULL,NULL,&r)==NET_INVALID_ARGUMENT);
    CHECK(net_domain_c2_permission_bundle_provisional(NULL,&p,NULL)==NET_INVALID_ARGUMENT);
    p=perm(false,DATA_QUALITY_INVALID,9u,65535u);
    CHECK(net_domain_c2_permission_bundle_provisional(NULL,&p,&r)==NET_OK);
    CHECK(r.vehicle_power_permission==255u&&r.power_permission_quality==2u&&r.power_operation_state==255u&&r.operation_allowed==0u);
    CHECK(r.operation_quality==3u&&r.source_age_ms==65535u&&r.update_sequence==9u);
    return 0;
}
int main(int argc,char **argv)
{
    static const struct { const char *name; int (*run)(void); } cases[]={
        {"perm_first",perm_first},{"perm_withdraw_immediate",perm_withdraw_immediate},{"perm_repeat_kept",perm_repeat_kept},
        {"perm_value_change_same_seq",perm_value_change_same_seq},{"perm_quality_restrict_same_seq",perm_quality_restrict_same_seq},
        {"perm_new_success",perm_new_success},{"perm_relax_guard",perm_relax_guard},{"perm_older_domain_seq",perm_older_domain_seq},
        {"perm_age_backwards",perm_age_backwards},{"perm_wrap",perm_wrap},{"perm_window_hold",perm_window_hold},
        {"perm_refused_not_consumed",perm_refused_not_consumed},{"perm_unknown_255",perm_unknown_255},
        {"perm_bundle_provisional",perm_bundle_provisional}};
    size_t i;
    if(argc!=2)return 2;
    for(i=0u;i<sizeof(cases)/sizeof(cases[0]);++i)if(strcmp(argv[1],cases[i].name)==0){
        case_name=cases[i].name;if(start()!=0)return 2;
        if(cases[i].run()!=0)return 1;
        printf("C2 %s PASS\n",argv[1]);return 0;}
    fprintf(stderr,"unknown case %s\n",argv[1]);return 2;
}
