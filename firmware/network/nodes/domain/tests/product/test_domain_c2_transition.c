#include "network/net_domain_c2.h"
#include <stdio.h>
#include <string.h>
/* DOMAIN C2 adapter: the CIS environment (0x300, CIS BOOT_ID) and Occupant
 * (0x310, PI_BOOT_ID) BOOT transitions Network owns. Frames run through the
 * product service, codec and adapter; the C2 Owner functions and the
 * transition binding are fakes that record each call (a case calls the Owner
 * callback directly only for a frame the service itself would refuse).
 *
 * Expected values come from the contracts:
 *   - docs/network/interface/02_NODES/DOMAIN/25_PRIVATE_ROUTING.md, CIS
 *     environment BOOT change: a new BOOT is held, the private transition is
 *     called once, only its confirmed completion lets the new BOOT through;
 *     a missing, failed or unclear transition restricts that route only;
 *   - 07_CROSS_NODE_INTERFACE_RULES.md, environment BOOT change, and
 *     04_COMMON/10_DATA_AND_ID_RULES.md, CIS environment sequential transition:
 *     epoch 1 first, exactly +1 after a confirmed completion, no reset, no wrap,
 *     the type maximum blocks; no automatic retry or recovery after a failure;
 *     the completion is checked against the current request;
 *   - firmware/DOMAIN/docs/CIS_OCCUPANT_BINDING_CONTRACT_DRAFT.md (Network
 *     responsibilities): the Occupant generation is the Pi producer generation,
 *     not the CIS CAN BOOT_ID; ingress stays closed before the first admission
 *     and after a failed transition; a delayed frame of a reused generation is
 *     refused. */
#define CHECK(x) do{if(!(x)){fprintf(stderr,"line %d: %s\n",__LINE__,#x);return 1;}}while(0)
enum { C_ENV, C_OCC, C_REAR, C_COUNT };
enum { ENV = NET_DOMAIN_C2_ENVIRONMENT_BARRIER, OCC = NET_DOMAIN_C2_OCCUPANT_BARRIER };
static NetUpper service;
static NetDomainC2 adapter;
static uint64_t now=1000u;
static unsigned calls[C_COUNT];
static CisOccupantState_t last_occupant;
static uint32_t pi_boot=6401u;
static bool initialized(void){return true;}
static DomainIf_Status_t set_port(const DomainIf_TxPort_t *p){(void)p;return DOMAIN_IF_OK;}
static DomainIf_Status_t f_env(const CisCabinEnvironmentState_t *v){(void)v;++calls[C_ENV];return DOMAIN_IF_OK;}
static DomainIf_Status_t f_occ(const CisOccupantState_t *v){last_occupant=*v;++calls[C_OCC];return DOMAIN_IF_OK;}
static DomainIf_Status_t f_rear(const CisRearState_t *v){(void)v;++calls[C_REAR];return DOMAIN_IF_OK;}
static NetStatus clock_read(void *c,uint64_t *out){(void)c;*out=now;return NET_OK;}
static NetStatus submit(void *c,const NetUpperPacket *p,NetTxKey k){(void)c;(void)p;(void)k;return NET_OK;}
static NetStatus poll(void *c,NetUpperEvent *out){(void)c;(void)out;return NET_NO_WORK;}
static const NetUpperPortOps ops={clock_read,submit,poll};

/* Fake DOMAIN private transition per barrier. Matching mode answers like the
 * DOMAIN core: the receipt of this request with the next barrier revision. */
typedef struct {
    NetDomainC2TransitionStatus result;
    bool own_receipt;                     /* answer `receipt` instead */
    NetDomainC2TransitionReceipt receipt;
    unsigned calls, in_service_callback, outside_adapter_step;
    uint32_t id, revision;
    uint64_t epoch, max_epoch_seen;
} FakeTransition;
static FakeTransition fake[2];
static NetDomainC2TransitionStatus fake_apply(FakeTransition *f,uint32_t id,uint64_t epoch,NetDomainC2TransitionReceipt *out)
{
    ++f->calls;f->id=id;f->epoch=epoch;if(epoch>f->max_epoch_seen)f->max_epoch_seen=epoch;
    if(service.in_call)++f->in_service_callback;
    if(!adapter.in_call)++f->outside_adapter_step;
    if(f->result==NET_DOMAIN_C2_TRANSITION_APPLIED||f->result==NET_DOMAIN_C2_TRANSITION_DUPLICATE_APPLIED){
        if(f->own_receipt)*out=f->receipt;
        else{++f->revision;out->id=id;out->epoch=epoch;out->revision=f->revision;}
    }
    return f->result;
}
static NetDomainC2TransitionStatus apply_env(void *c,uint32_t id,uint64_t epoch,NetDomainC2TransitionReceipt *out){(void)c;return fake_apply(&fake[ENV],id,epoch,out);}
static NetDomainC2TransitionStatus apply_occ(void *c,uint32_t id,uint64_t epoch,NetDomainC2TransitionReceipt *out){(void)c;return fake_apply(&fake[OCC],id,epoch,out);}

static int start(void)
{
    NetDomainC2Api api;NetDomainC2Config cfg;NetUpperConfig config;size_t i;
    memset(&api,0,sizeof(api));memset(&cfg,0,sizeof(cfg));memset(&config,0,sizeof(config));
    memset(calls,0,sizeof(calls));memset(fake,0,sizeof(fake));
    for(i=0u;i<2u;++i)fake[i].result=NET_DOMAIN_C2_TRANSITION_APPLIED;
    pi_boot=6401u;
    api.IsInitialized=initialized;api.SetTxPort=set_port;
    api.RxCisCabinEnvironment=f_env;api.RxCisOccupant=f_occ;api.RxCisRear=f_rear;
    cfg.domain_boot=301u;cfg.bcm_boot=201u;cfg.first_can_sequence=1u;
    cfg.transmit_policy.bound=true;cfg.transmit_policy.maximum_age_ms=66535u;cfg.transmit_policy.expires_at_equality=true;
    cfg.value_quality=net_domain_c2_value_quality_provisional;
    cfg.cis_transition_barriers=true;
    cfg.transition.environment=apply_env;cfg.transition.occupant=apply_occ;
    CHECK(net_domain_c2_init(&adapter,&service,NULL,&api,&cfg)==NET_OK);
    config.node=NET_WIRE_NODE_DOMAIN;config.local_epoch=3u;config.profile=NET_WIRE_CANDIDATE_ONLY;config.port.ops=&ops;
    config.owner=net_domain_c2_owner(&adapter);
    for(i=0u;i<3u;++i)config.port_epoch[i]=4u;
    for(i=0u;i<NET_UPPER_ROUTE_COUNT;++i){config.rx_policy[i].bound=true;config.rx_policy[i].maximum_age_ms=10000u;}
    CHECK(net_upper_init(&service,&config)==NET_OK);CHECK(net_domain_c2_register(&adapter)==NET_OK);
    return 0;
}
/* A new run of the fixture (each latched result needs a barrier of its own). */
static int restart(void)
{
    CHECK(net_domain_c2_unregister(&adapter)==NET_OK);
    memset(&adapter,0,sizeof(adapter));memset(&service,0,sizeof(service));
    return start();
}
static NetUpperCanMessage env_frame(uint32_t boot,uint32_t tx)
{
    NetUpperCanMessage m;memset(&m,0,sizeof(m));m.id=0x300u;
    m.data.cis_environment.boot_id=boot;m.data.cis_environment.tx_sequence=tx;
    m.data.cis_environment.temperature_validity=1u;m.data.cis_environment.humidity_validity=1u;
    m.data.cis_environment.illuminance_validity=1u;m.data.cis_environment.temperature_sequence=tx;
    m.data.cis_environment.humidity_sequence=tx;m.data.cis_environment.illuminance_sequence=tx;
    m.data.cis_environment.temperature_centi_c=2150;m.data.cis_environment.humidity_centi_percent=4500u;
    return m;
}
static NetUpperCanMessage occ_frame(uint32_t boot,uint32_t tx,uint32_t vision)
{
    NetUpperCanMessage m;memset(&m,0,sizeof(m));m.id=0x310u;
    m.data.cis_occupant.boot_id=boot;m.data.cis_occupant.tx_sequence=tx;m.data.cis_occupant.pi_boot_id=pi_boot;
    m.data.cis_occupant.vision_sequence=vision;m.data.cis_occupant.occupant_presence=1u;m.data.cis_occupant.occupant_count=1u;
    m.data.cis_occupant.presence_validity=1u;m.data.cis_occupant.count_validity=1u;m.data.cis_occupant.vision_function_status=2u;
    return m;
}
static NetUpperCanMessage rear_frame(uint32_t boot,uint32_t tx)
{
    NetUpperCanMessage m;memset(&m,0,sizeof(m));m.id=0x0a0u;
    m.data.cis_rear.boot_id=boot;m.data.cis_rear.tx_sequence=tx;m.data.cis_rear.validity=1u;
    m.data.cis_rear.rear_distance_deci_cm=123u;m.data.cis_rear.update_sequence=tx;
    return m;
}
static int feed(const NetUpperCanMessage *m)
{
    NetUpperEvent e;memset(&e,0,sizeof(e));
    e.kind=NET_UPPER_CAN_RX;e.link=NET_WIRE_CAN;e.capture.port_epoch=4u;e.capture.timestamp_known=true;e.capture.captured_at_ms=now;
    CHECK(net_upper_can_encode(NET_WIRE_CANDIDATE_ONLY,m,&e.can)==NET_WIRE_OK);
    CHECK(net_upper_capture(&service,&e)==NET_OK);(void)net_upper_step(&service,1u);
    return 0;
}
/* The adapter callback alone, for a frame the service would refuse first. */
static NetStatus direct(const NetUpperCanMessage *m,uint32_t boot,uint32_t tx)
{
    NetUpperOwner owner=net_domain_c2_owner(&adapter);NetUpperDelivery d;
    memset(&d,0,sizeof(d));d.source_boot=boot;d.source_sequence=tx;d.capture.port_epoch=4u;
    d.capture.captured_at_ms=now;d.capture.timestamp_known=true;d.delivered_at_ms=now;d.receipt_id=1u;d.freshness_known=true;
    return owner.can(owner.context,m,&d);
}
static const NetDomainC2IngressRecord *record(uint16_t id)
{
    size_t i;
    for(i=0u;i<19u;++i)if(adapter.ingress[i].used&&adapter.ingress[i].link==NET_WIRE_CAN&&adapter.ingress[i].id==id)return &adapter.ingress[i];
    return NULL;
}
static bool left(const NetDomainC2Barrier *b,uint32_t id)
{
    size_t i;
    for(i=0u;i<NET_DOMAIN_C2_BARRIER_LEFT_IDS;++i)if(b->left[i]==id)return true;
    return false;
}
static bool closed_route_untouched(const NetDomainC2Barrier *b)
{
    return b->state==NET_DOMAIN_C2_BARRIER_CLOSED&&!b->open&&b->next_epoch==1u&&b->calls==0u&&b->held==0u;
}
/* Opens the environment barrier for boot (its first frame is held, the
 * adapter step confirms the transition). */
static int open_env(uint32_t boot,uint32_t tx)
{
    NetUpperCanMessage m=env_frame(boot,tx);unsigned before=fake[ENV].calls;
    CHECK(feed(&m)==0);CHECK(adapter.barrier[ENV].state==NET_DOMAIN_C2_BARRIER_PENDING&&adapter.barrier[ENV].id==boot);
    CHECK(net_domain_c2_step(&adapter)==NET_OK);CHECK(fake[ENV].calls==before+1u&&fake[ENV].id==boot);
    CHECK(adapter.barrier[ENV].state==NET_DOMAIN_C2_BARRIER_OPEN&&adapter.barrier[ENV].open&&adapter.barrier[ENV].id==boot);
    return 0;
}
static int open_occ(uint32_t boot,uint32_t tx,uint32_t vision)
{
    NetUpperCanMessage m=occ_frame(boot,tx,vision);unsigned before=fake[OCC].calls;
    CHECK(feed(&m)==0);CHECK(adapter.barrier[OCC].state==NET_DOMAIN_C2_BARRIER_PENDING&&adapter.barrier[OCC].id==pi_boot);
    CHECK(net_domain_c2_step(&adapter)==NET_OK);CHECK(fake[OCC].calls==before+1u&&fake[OCC].id==pi_boot);
    CHECK(adapter.barrier[OCC].state==NET_DOMAIN_C2_BARRIER_OPEN&&adapter.barrier[OCC].open&&adapter.barrier[OCC].id==pi_boot);
    return 0;
}

/* The first CIS BOOT: held before C2 and the journal, epoch 1 requested once
 * from the adapter step, the confirmed completion opens the route. */
static int barrier_first_epoch(void)
{
    const NetDomainC2Barrier *b=&adapter.barrier[ENV];NetUpperCanMessage m;const NetDomainC2IngressRecord *r;
    CHECK(b->state==NET_DOMAIN_C2_BARRIER_CLOSED&&!b->open&&b->next_epoch==1u&&b->last_result==NET_DOMAIN_C2_TRANSITION_NONE);
    m=env_frame(401u,1u);CHECK(feed(&m)==0);
    CHECK(calls[C_ENV]==0u&&record(0x300u)==NULL&&b->held==1u&&fake[ENV].calls==0u);
    CHECK(b->state==NET_DOMAIN_C2_BARRIER_PENDING&&b->id==401u&&!b->open);
    m=env_frame(401u,2u);CHECK(feed(&m)==0);
    CHECK(calls[C_ENV]==0u&&b->held==2u&&fake[ENV].calls==0u&&b->state==NET_DOMAIN_C2_BARRIER_PENDING);
    CHECK(net_domain_c2_step(&adapter)==NET_OK);
    CHECK(fake[ENV].calls==1u&&fake[ENV].id==401u&&fake[ENV].epoch==1u);
    CHECK(b->state==NET_DOMAIN_C2_BARRIER_OPEN&&b->open&&b->id==401u&&b->next_epoch==2u&&b->revision==1u);
    CHECK(b->last_result==NET_DOMAIN_C2_TRANSITION_APPLIED&&b->reason==NET_DOMAIN_C2_BARRIER_REASON_NONE&&b->applied==1u&&b->calls==1u);
    m=env_frame(401u,3u);CHECK(feed(&m)==0);CHECK(calls[C_ENV]==1u&&b->held==2u);
    r=record(0x300u);CHECK(r!=NULL&&r->boot==401u&&r->sequence==3u&&!r->blocked);
    m=env_frame(401u,4u);CHECK(feed(&m)==0);CHECK(calls[C_ENV]==2u);
    CHECK(net_domain_c2_step(&adapter)==NET_NO_WORK&&fake[ENV].calls==1u);
    /* The Occupant barrier and another route are not involved. */
    CHECK(closed_route_untouched(&adapter.barrier[OCC])&&fake[OCC].calls==0u);
    m=rear_frame(401u,1u);CHECK(feed(&m)==0);CHECK(calls[C_REAR]==1u);
    return 0;
}
/* CIS restarts: the new BOOT is held and gets epoch 2; the journal takes the
 * new generation; a late frame of the BOOT it left stays held. */
static int barrier_reboot_epoch2(void)
{
    const NetDomainC2Barrier *b=&adapter.barrier[ENV];NetUpperCanMessage m;const NetDomainC2IngressRecord *r;
    CHECK(open_env(401u,1u)==0);
    m=env_frame(401u,2u);CHECK(feed(&m)==0);CHECK(calls[C_ENV]==1u);
    m=env_frame(402u,1u);CHECK(feed(&m)==0);
    CHECK(calls[C_ENV]==1u&&b->state==NET_DOMAIN_C2_BARRIER_PENDING&&b->id==402u&&!b->open&&left(b,401u));
    m=env_frame(401u,3u);CHECK(direct(&m,401u,3u)==NET_HOLD);CHECK(b->state==NET_DOMAIN_C2_BARRIER_PENDING&&b->id==402u);
    CHECK(net_domain_c2_step(&adapter)==NET_OK);
    CHECK(fake[ENV].calls==2u&&fake[ENV].id==402u&&fake[ENV].epoch==2u);
    CHECK(b->state==NET_DOMAIN_C2_BARRIER_OPEN&&b->id==402u&&b->next_epoch==3u&&b->revision==2u&&b->applied==2u);
    r=record(0x300u);CHECK(r!=NULL&&r->boot==402u&&r->sequence==UINT64_MAX);
    m=env_frame(402u,2u);CHECK(feed(&m)==0);CHECK(calls[C_ENV]==2u&&r->boot==402u&&r->sequence==2u);
    /* The ended BOOT: the service refuses it, the barrier would hold it. */
    m=env_frame(401u,9u);CHECK(feed(&m)==0);CHECK(calls[C_ENV]==2u);
    CHECK(direct(&m,401u,9u)==NET_HOLD&&calls[C_ENV]==2u&&b->state==NET_DOMAIN_C2_BARRIER_OPEN&&b->id==402u);
    CHECK(net_domain_c2_step(&adapter)==NET_NO_WORK&&fake[ENV].calls==2u);
    return 0;
}
/* DOMAIN not initialized yet: the request stays pending with its epoch and is
 * made again at the next step; nothing passes meanwhile. */
static int barrier_not_initialized_retry(void)
{
    const NetDomainC2Barrier *b=&adapter.barrier[ENV];NetUpperCanMessage m;
    fake[ENV].result=NET_DOMAIN_C2_TRANSITION_NOT_INITIALIZED;
    m=env_frame(401u,1u);CHECK(feed(&m)==0);
    CHECK(net_domain_c2_step(&adapter)==NET_HOLD);
    CHECK(fake[ENV].calls==1u&&fake[ENV].epoch==1u&&b->state==NET_DOMAIN_C2_BARRIER_PENDING&&b->id==401u&&!b->open);
    CHECK(b->next_epoch==1u&&b->last_result==NET_DOMAIN_C2_TRANSITION_NOT_INITIALIZED&&b->applied==0u);
    m=env_frame(401u,2u);CHECK(feed(&m)==0);CHECK(calls[C_ENV]==0u);
    CHECK(net_domain_c2_step(&adapter)==NET_HOLD);
    CHECK(fake[ENV].calls==2u&&fake[ENV].id==401u&&fake[ENV].epoch==1u&&b->next_epoch==1u);
    fake[ENV].result=NET_DOMAIN_C2_TRANSITION_APPLIED;
    CHECK(net_domain_c2_step(&adapter)==NET_OK);
    CHECK(fake[ENV].calls==3u&&fake[ENV].epoch==1u&&b->state==NET_DOMAIN_C2_BARRIER_OPEN&&b->next_epoch==2u);
    m=env_frame(401u,3u);CHECK(feed(&m)==0);CHECK(calls[C_ENV]==1u);
    return 0;
}
/* The results the DOMAIN private transitions return besides a completion and
 * NOT_INITIALIZED (EPOCH_ORDER_UNAVAILABLE is never returned by the sequential
 * contract; it falls in the same latched branch). */
static const NetDomainC2TransitionStatus latched[6]={
    NET_DOMAIN_C2_TRANSITION_STALE,NET_DOMAIN_C2_TRANSITION_ORDER_ERROR,NET_DOMAIN_C2_TRANSITION_ID_CONFLICT,
    NET_DOMAIN_C2_TRANSITION_INVALIDATION_FAILED_BLOCKED,NET_DOMAIN_C2_TRANSITION_BUSY,
    NET_DOMAIN_C2_TRANSITION_INVALID_ARGUMENT};
/* Every other result closes the route for the rest of the run: no retry, no
 * epoch spent, no later generation reopens it; other routes keep going. */
static int barrier_latched_results(void)
{
    unsigned i;
    for(i=0u;i<6u;++i){
        const NetDomainC2Barrier *b=&adapter.barrier[ENV];NetUpperCanMessage m;
        if(i>0u)CHECK(restart()==0);
        fake[ENV].result=latched[i];
        m=env_frame(401u,1u);CHECK(feed(&m)==0);
        CHECK(net_domain_c2_step(&adapter)==NET_HOLD);
        CHECK(fake[ENV].calls==1u&&b->state==NET_DOMAIN_C2_BARRIER_BLOCKED&&!b->open);
        CHECK(b->reason==NET_DOMAIN_C2_BARRIER_REASON_RESULT&&b->last_result==latched[i]&&b->next_epoch==1u&&b->applied==0u);
        m=env_frame(401u,2u);CHECK(feed(&m)==0);
        m=env_frame(402u,1u);CHECK(feed(&m)==0);
        CHECK(b->state==NET_DOMAIN_C2_BARRIER_BLOCKED&&calls[C_ENV]==0u&&b->held==3u&&record(0x300u)==NULL);
        CHECK(net_domain_c2_step(&adapter)==NET_NO_WORK&&net_domain_c2_step(&adapter)==NET_NO_WORK&&fake[ENV].calls==1u);
        m=rear_frame(401u,1u);CHECK(feed(&m)==0);CHECK(calls[C_REAR]==1u);
        CHECK(open_occ(401u,1u,1u)==0);m=occ_frame(401u,2u,2u);CHECK(feed(&m)==0);CHECK(calls[C_OCC]==1u);
        printf("LATCHED environment result=%d blocked, calls=%u PASS\n",(int)latched[i],fake[ENV].calls);
    }
    /* A failed transition away from an open generation closes that one too. */
    {const NetDomainC2Barrier *b;NetUpperCanMessage m;
        CHECK(restart()==0);b=&adapter.barrier[ENV];
        CHECK(open_env(401u,1u)==0);m=env_frame(401u,2u);CHECK(feed(&m)==0);CHECK(calls[C_ENV]==1u);
        fake[ENV].result=NET_DOMAIN_C2_TRANSITION_ORDER_ERROR;
        m=env_frame(402u,1u);CHECK(feed(&m)==0);CHECK(net_domain_c2_step(&adapter)==NET_HOLD);
        CHECK(fake[ENV].calls==2u&&fake[ENV].epoch==2u&&b->state==NET_DOMAIN_C2_BARRIER_BLOCKED&&b->next_epoch==2u);
        m=env_frame(402u,2u);CHECK(feed(&m)==0);
        m=env_frame(401u,3u);CHECK(direct(&m,401u,3u)==NET_HOLD);
        CHECK(calls[C_ENV]==1u&&net_domain_c2_step(&adapter)==NET_NO_WORK&&fake[ENV].calls==2u);}
    /* UNAVAILABLE disables the environment barrier as it does the Occupant one. */
    {const NetDomainC2Barrier *b;NetUpperCanMessage m;
        CHECK(restart()==0);b=&adapter.barrier[ENV];
        fake[ENV].result=NET_DOMAIN_C2_TRANSITION_UNAVAILABLE;
        m=env_frame(401u,1u);CHECK(feed(&m)==0);CHECK(net_domain_c2_step(&adapter)==NET_HOLD);
        CHECK(b->state==NET_DOMAIN_C2_BARRIER_DISABLED&&b->next_epoch==1u&&fake[ENV].calls==1u);
        m=env_frame(402u,1u);CHECK(feed(&m)==0);
        CHECK(net_domain_c2_step(&adapter)==NET_NO_WORK&&fake[ENV].calls==1u&&calls[C_ENV]==0u&&b->state==NET_DOMAIN_C2_BARRIER_DISABLED);}
    return 0;
}
/* A completion counts only with the receipt of this request: same generation,
 * same epoch, a DOMAIN barrier revision past the last confirmed one. A
 * DUPLICATE_APPLIED with that receipt is the completion of this request. */
static int barrier_receipt_mismatch(void)
{
    static const struct { NetDomainC2TransitionStatus result; uint32_t id; uint64_t epoch; uint32_t revision; } bad[4]={
        {NET_DOMAIN_C2_TRANSITION_APPLIED,999u,1u,1u},{NET_DOMAIN_C2_TRANSITION_APPLIED,401u,7u,1u},
        {NET_DOMAIN_C2_TRANSITION_APPLIED,401u,1u,0u},{NET_DOMAIN_C2_TRANSITION_DUPLICATE_APPLIED,401u,2u,1u}};
    unsigned i;
    for(i=0u;i<4u;++i){
        const NetDomainC2Barrier *b=&adapter.barrier[ENV];NetUpperCanMessage m;
        if(i>0u)CHECK(restart()==0);
        fake[ENV].result=bad[i].result;fake[ENV].own_receipt=true;
        fake[ENV].receipt.id=bad[i].id;fake[ENV].receipt.epoch=bad[i].epoch;fake[ENV].receipt.revision=bad[i].revision;
        m=env_frame(401u,1u);CHECK(feed(&m)==0);CHECK(net_domain_c2_step(&adapter)==NET_HOLD);
        CHECK(b->state==NET_DOMAIN_C2_BARRIER_BLOCKED&&!b->open&&b->reason==NET_DOMAIN_C2_BARRIER_REASON_RECEIPT_MISMATCH);
        CHECK(b->next_epoch==1u&&b->revision==0u&&b->applied==0u&&b->last_receipt.id==bad[i].id);
        m=env_frame(401u,2u);CHECK(feed(&m)==0);CHECK(calls[C_ENV]==0u);
        CHECK(net_domain_c2_step(&adapter)==NET_NO_WORK&&fake[ENV].calls==1u);
    }
    /* The DOMAIN barrier revision must advance past the last confirmed one. */
    {const NetDomainC2Barrier *b;NetUpperCanMessage m;
        CHECK(restart()==0);b=&adapter.barrier[ENV];
        CHECK(open_env(401u,1u)==0);CHECK(b->revision==1u);
        fake[ENV].own_receipt=true;fake[ENV].receipt.id=402u;fake[ENV].receipt.epoch=2u;fake[ENV].receipt.revision=1u;
        m=env_frame(402u,1u);CHECK(feed(&m)==0);CHECK(net_domain_c2_step(&adapter)==NET_HOLD);
        CHECK(b->state==NET_DOMAIN_C2_BARRIER_BLOCKED&&b->reason==NET_DOMAIN_C2_BARRIER_REASON_RECEIPT_MISMATCH&&b->next_epoch==2u);
        m=env_frame(402u,2u);CHECK(feed(&m)==0);CHECK(calls[C_ENV]==0u);}
    /* DUPLICATE_APPLIED with the receipt of this request completes it. */
    {const NetDomainC2Barrier *b;NetUpperCanMessage m;
        CHECK(restart()==0);b=&adapter.barrier[ENV];
        fake[ENV].result=NET_DOMAIN_C2_TRANSITION_DUPLICATE_APPLIED;
        m=env_frame(401u,1u);CHECK(feed(&m)==0);CHECK(net_domain_c2_step(&adapter)==NET_OK);
        CHECK(b->state==NET_DOMAIN_C2_BARRIER_OPEN&&b->id==401u&&b->next_epoch==2u&&b->revision==1u);
        m=env_frame(401u,2u);CHECK(feed(&m)==0);CHECK(calls[C_ENV]==1u);}
    return 0;
}
/* Generations the barrier left (open, or pending and replaced before its
 * transition) stay held: a late frame starts no transition back. */
static int barrier_late_old_id(void)
{
    const NetDomainC2Barrier *b=&adapter.barrier[ENV];NetUpperCanMessage m;
    m=env_frame(400u,1u);CHECK(feed(&m)==0);CHECK(b->state==NET_DOMAIN_C2_BARRIER_PENDING&&b->id==400u);
    m=env_frame(401u,1u);CHECK(feed(&m)==0);CHECK(b->state==NET_DOMAIN_C2_BARRIER_PENDING&&b->id==401u&&left(b,400u));
    CHECK(net_domain_c2_step(&adapter)==NET_OK&&fake[ENV].calls==1u&&fake[ENV].id==401u&&fake[ENV].epoch==1u);
    m=env_frame(400u,2u);CHECK(direct(&m,400u,2u)==NET_HOLD);
    CHECK(b->state==NET_DOMAIN_C2_BARRIER_OPEN&&b->id==401u);
    CHECK(open_env(402u,1u)==0);CHECK(left(b,400u)&&left(b,401u)&&b->next_epoch==3u);
    m=env_frame(401u,5u);CHECK(direct(&m,401u,5u)==NET_HOLD);
    m=env_frame(400u,6u);CHECK(direct(&m,400u,6u)==NET_HOLD);
    CHECK(b->state==NET_DOMAIN_C2_BARRIER_OPEN&&b->id==402u&&b->open);
    CHECK(net_domain_c2_step(&adapter)==NET_NO_WORK&&fake[ENV].calls==2u);
    CHECK(open_env(403u,1u)==0);
    m=env_frame(402u,7u);CHECK(direct(&m,402u,7u)==NET_HOLD);
    CHECK(b->state==NET_DOMAIN_C2_BARRIER_OPEN&&b->id==403u&&net_domain_c2_step(&adapter)==NET_NO_WORK&&fake[ENV].calls==3u);
    m=env_frame(403u,2u);CHECK(feed(&m)==0);CHECK(calls[C_ENV]==1u);
    return 0;
}
/* The transition is never called inside a service callback: frames only make
 * the barrier pending; the adapter step calls it, and the step itself refuses
 * to run while the service is in a callback. */
static int barrier_never_in_callback(void)
{
    const NetDomainC2Barrier *b=&adapter.barrier[ENV];NetUpperCanMessage m;
    m=env_frame(401u,1u);CHECK(feed(&m)==0);
    m=occ_frame(401u,1u,1u);CHECK(feed(&m)==0);
    CHECK(fake[ENV].calls==0u&&fake[OCC].calls==0u);
    CHECK(b->state==NET_DOMAIN_C2_BARRIER_PENDING&&adapter.barrier[OCC].state==NET_DOMAIN_C2_BARRIER_PENDING);
    service.in_call=true;
    CHECK(net_domain_c2_step(&adapter)==NET_BUSY&&fake[ENV].calls==0u&&fake[OCC].calls==0u);
    service.in_call=false;
    CHECK(net_domain_c2_step(&adapter)==NET_OK);
    CHECK(fake[ENV].calls==1u&&fake[OCC].calls==1u);
    CHECK(fake[ENV].in_service_callback==0u&&fake[OCC].in_service_callback==0u);
    CHECK(fake[ENV].outside_adapter_step==0u&&fake[OCC].outside_adapter_step==0u&&!adapter.in_call);
    m=env_frame(402u,1u);CHECK(feed(&m)==0);CHECK(fake[ENV].calls==1u);
    CHECK(net_domain_c2_step(&adapter)==NET_OK&&fake[ENV].calls==2u&&fake[ENV].in_service_callback==0u);
    return 0;
}
/* The epoch never reaches UINT64_MAX: the run that has used every lower
 * epoch blocks the next transition instead of wrapping. The fixture stands in
 * for that run by setting the next epoch. */
static int barrier_epoch_max(void)
{
    NetDomainC2Barrier *b=&adapter.barrier[ENV];NetUpperCanMessage m;
    b->next_epoch=UINT64_MAX-1u;
    CHECK(open_env(401u,1u)==0);
    CHECK(fake[ENV].epoch==UINT64_MAX-1u&&b->next_epoch==UINT64_MAX);
    m=env_frame(401u,2u);CHECK(feed(&m)==0);CHECK(calls[C_ENV]==1u);
    m=env_frame(402u,1u);CHECK(feed(&m)==0);CHECK(b->state==NET_DOMAIN_C2_BARRIER_PENDING&&b->id==402u);
    CHECK(net_domain_c2_step(&adapter)==NET_HOLD);
    CHECK(fake[ENV].calls==1u&&fake[ENV].max_epoch_seen==UINT64_MAX-1u);
    CHECK(b->state==NET_DOMAIN_C2_BARRIER_BLOCKED&&!b->open&&b->reason==NET_DOMAIN_C2_BARRIER_REASON_EPOCH_EXHAUSTED);
    CHECK(b->next_epoch==UINT64_MAX);
    m=env_frame(402u,2u);CHECK(feed(&m)==0);CHECK(calls[C_ENV]==1u);
    CHECK(net_domain_c2_step(&adapter)==NET_NO_WORK&&fake[ENV].calls==1u&&b->next_epoch==UINT64_MAX);
    m=rear_frame(402u,1u);CHECK(feed(&m)==0);CHECK(calls[C_REAR]==1u);
    return 0;
}
/* Occupant: keyed by PI_BOOT_ID, epochs of its own from 1. */
static int occupant_first_epoch(void)
{
    const NetDomainC2Barrier *b=&adapter.barrier[OCC];NetUpperCanMessage m;
    m=occ_frame(401u,1u,1u);CHECK(feed(&m)==0);
    CHECK(calls[C_OCC]==0u&&record(0x310u)==NULL&&b->held==1u&&b->state==NET_DOMAIN_C2_BARRIER_PENDING&&b->id==6401u);
    CHECK(fake[OCC].calls==0u);
    CHECK(net_domain_c2_step(&adapter)==NET_OK);
    CHECK(fake[OCC].calls==1u&&fake[OCC].id==6401u&&fake[OCC].epoch==1u);
    CHECK(b->state==NET_DOMAIN_C2_BARRIER_OPEN&&b->open&&b->next_epoch==2u&&b->revision==1u);
    m=occ_frame(401u,2u,2u);CHECK(feed(&m)==0);
    CHECK(calls[C_OCC]==1u&&last_occupant.sourceGeneration==6401u&&last_occupant.presenceQuality.meta.updateSequence==2u);
    /* The environment barrier of the same CIS BOOT is a separate one. */
    CHECK(closed_route_untouched(&adapter.barrier[ENV])&&fake[ENV].calls==0u);
    return 0;
}
/* Pi restarts (CIS keeps its BOOT): the new PI_BOOT_ID gets epoch 2; a late
 * frame of the old one, which the service still passes (same CIS run), stays
 * held. */
static int occupant_pi_reboot_epoch2(void)
{
    const NetDomainC2Barrier *b=&adapter.barrier[OCC];NetUpperCanMessage m;
    CHECK(open_occ(401u,1u,1u)==0);
    m=occ_frame(401u,2u,2u);CHECK(feed(&m)==0);CHECK(calls[C_OCC]==1u);
    pi_boot=6402u;m=occ_frame(401u,3u,1u);CHECK(feed(&m)==0);
    CHECK(calls[C_OCC]==1u&&b->state==NET_DOMAIN_C2_BARRIER_PENDING&&b->id==6402u&&left(b,6401u));
    CHECK(net_domain_c2_step(&adapter)==NET_OK);
    CHECK(fake[OCC].calls==2u&&fake[OCC].id==6402u&&fake[OCC].epoch==2u&&b->next_epoch==3u&&b->state==NET_DOMAIN_C2_BARRIER_OPEN);
    m=occ_frame(401u,4u,2u);CHECK(feed(&m)==0);CHECK(calls[C_OCC]==2u&&last_occupant.sourceGeneration==6402u);
    pi_boot=6401u;m=occ_frame(401u,5u,3u);CHECK(feed(&m)==0);
    CHECK(calls[C_OCC]==2u&&b->state==NET_DOMAIN_C2_BARRIER_OPEN&&b->id==6402u);
    CHECK(net_domain_c2_step(&adapter)==NET_NO_WORK&&fake[OCC].calls==2u);
    pi_boot=6402u;m=occ_frame(401u,6u,3u);CHECK(feed(&m)==0);CHECK(calls[C_OCC]==3u);
    return 0;
}
/* PI_BOOT_ID 0 never passes and never starts a transition. The CAN codec
 * already refuses it (no frame reaches the adapter); the adapter rule is shown
 * on its own callback. */
static int occupant_gen_zero_hold(void)
{
    const NetDomainC2Barrier *b=&adapter.barrier[OCC];NetUpperCanMessage m;NetUpperEvent e;
    pi_boot=0u;m=occ_frame(401u,1u,1u);
    CHECK(net_upper_can_encode(NET_WIRE_CANDIDATE_ONLY,&m,&e.can)!=NET_WIRE_OK);
    CHECK(direct(&m,401u,1u)==NET_HOLD);
    CHECK(calls[C_OCC]==0u&&b->held==1u&&b->state==NET_DOMAIN_C2_BARRIER_CLOSED);
    CHECK(net_domain_c2_step(&adapter)==NET_NO_WORK&&fake[OCC].calls==0u);
    pi_boot=6401u;CHECK(open_occ(401u,2u,1u)==0);
    pi_boot=0u;m=occ_frame(401u,3u,2u);CHECK(direct(&m,401u,3u)==NET_HOLD);
    CHECK(calls[C_OCC]==0u&&b->state==NET_DOMAIN_C2_BARRIER_OPEN&&b->id==6401u&&b->held==3u);
    CHECK(net_domain_c2_step(&adapter)==NET_NO_WORK&&fake[OCC].calls==1u);
    pi_boot=6401u;m=occ_frame(401u,4u,3u);CHECK(feed(&m)==0);CHECK(calls[C_OCC]==1u);
    return 0;
}
/* CIS restarts, the Pi does not: the Occupant generation is the same, so no
 * Occupant transition; the journal takes the new CIS run. The environment
 * route of that CIS BOOT has its own transition. */
static int occupant_cis_reboot_only(void)
{
    const NetDomainC2Barrier *b=&adapter.barrier[OCC];NetUpperCanMessage m;const NetDomainC2IngressRecord *r;
    CHECK(open_occ(401u,1u,1u)==0);
    m=occ_frame(401u,2u,2u);CHECK(feed(&m)==0);CHECK(calls[C_OCC]==1u);
    r=record(0x310u);CHECK(r!=NULL&&r->boot==401u&&r->sequence==2u);
    m=occ_frame(402u,1u,3u);CHECK(feed(&m)==0);
    CHECK(calls[C_OCC]==2u&&r->boot==402u&&r->sequence==1u&&!r->blocked);
    CHECK(b->state==NET_DOMAIN_C2_BARRIER_OPEN&&b->id==6401u&&b->next_epoch==2u&&b->held==1u);
    CHECK(net_domain_c2_step(&adapter)==NET_NO_WORK&&fake[OCC].calls==1u);
    m=env_frame(402u,1u);CHECK(feed(&m)==0);CHECK(net_domain_c2_step(&adapter)==NET_OK);
    CHECK(fake[ENV].calls==1u&&fake[ENV].id==402u&&fake[ENV].epoch==1u&&fake[OCC].calls==1u);
    m=occ_frame(402u,2u,4u);CHECK(feed(&m)==0);CHECK(calls[C_OCC]==3u);
    return 0;
}
static int occupant_latched_results(void)
{
    unsigned i;
    for(i=0u;i<6u;++i){
        const NetDomainC2Barrier *b=&adapter.barrier[OCC];NetUpperCanMessage m;
        if(i>0u)CHECK(restart()==0);
        fake[OCC].result=latched[i];
        m=occ_frame(401u,1u,1u);CHECK(feed(&m)==0);
        CHECK(net_domain_c2_step(&adapter)==NET_HOLD);
        CHECK(fake[OCC].calls==1u&&b->state==NET_DOMAIN_C2_BARRIER_BLOCKED&&!b->open);
        CHECK(b->reason==NET_DOMAIN_C2_BARRIER_REASON_RESULT&&b->last_result==latched[i]&&b->next_epoch==1u);
        m=occ_frame(401u,2u,2u);CHECK(feed(&m)==0);
        pi_boot=6402u;m=occ_frame(401u,3u,1u);CHECK(feed(&m)==0);pi_boot=6401u;
        CHECK(b->state==NET_DOMAIN_C2_BARRIER_BLOCKED&&calls[C_OCC]==0u&&record(0x310u)==NULL);
        CHECK(net_domain_c2_step(&adapter)==NET_NO_WORK&&net_domain_c2_step(&adapter)==NET_NO_WORK&&fake[OCC].calls==1u);
        m=rear_frame(401u,1u);CHECK(feed(&m)==0);CHECK(calls[C_REAR]==1u);
        CHECK(open_env(401u,1u)==0);m=env_frame(401u,2u);CHECK(feed(&m)==0);CHECK(calls[C_ENV]==1u);
        printf("LATCHED Occupant result=%d blocked, calls=%u PASS\n",(int)latched[i],fake[OCC].calls);
    }
    return 0;
}
/* Occupant disabled on the DOMAIN side (the binding reaches no private
 * context): one call, no epoch spent, never called again, 0x310 held, every
 * other route normal. */
static int occupant_disabled(void)
{
    const NetDomainC2Barrier *b=&adapter.barrier[OCC];NetUpperCanMessage m;unsigned i;
    fake[OCC].result=NET_DOMAIN_C2_TRANSITION_UNAVAILABLE;
    m=occ_frame(401u,1u,1u);CHECK(feed(&m)==0);
    CHECK(net_domain_c2_step(&adapter)==NET_HOLD);
    CHECK(fake[OCC].calls==1u&&b->state==NET_DOMAIN_C2_BARRIER_DISABLED&&!b->open);
    CHECK(b->reason==NET_DOMAIN_C2_BARRIER_REASON_RESULT&&b->last_result==NET_DOMAIN_C2_TRANSITION_UNAVAILABLE);
    CHECK(b->next_epoch==1u&&b->applied==0u);
    m=occ_frame(401u,2u,2u);CHECK(feed(&m)==0);
    pi_boot=6402u;m=occ_frame(401u,3u,1u);CHECK(feed(&m)==0);
    CHECK(b->state==NET_DOMAIN_C2_BARRIER_DISABLED&&calls[C_OCC]==0u&&b->held==3u);
    for(i=0u;i<3u;++i)CHECK(net_domain_c2_step(&adapter)==NET_NO_WORK);
    CHECK(fake[OCC].calls==1u&&b->next_epoch==1u);
    m=rear_frame(401u,1u);CHECK(feed(&m)==0);CHECK(calls[C_REAR]==1u);
    CHECK(open_env(401u,1u)==0);m=env_frame(401u,2u);CHECK(feed(&m)==0);CHECK(calls[C_ENV]==1u);
    return 0;
}
int main(int argc,char **argv)
{
    static const struct { const char *name; int (*run)(void); } cases[]={
        {"barrier_first_epoch",barrier_first_epoch},{"barrier_reboot_epoch2",barrier_reboot_epoch2},
        {"barrier_not_initialized_retry",barrier_not_initialized_retry},{"barrier_latched_results",barrier_latched_results},
        {"barrier_receipt_mismatch",barrier_receipt_mismatch},{"barrier_late_old_id",barrier_late_old_id},
        {"barrier_never_in_callback",barrier_never_in_callback},{"barrier_epoch_max",barrier_epoch_max},
        {"occupant_first_epoch",occupant_first_epoch},{"occupant_pi_reboot_epoch2",occupant_pi_reboot_epoch2},
        {"occupant_gen_zero_hold",occupant_gen_zero_hold},{"occupant_cis_reboot_only",occupant_cis_reboot_only},
        {"occupant_latched_results",occupant_latched_results},{"occupant_disabled",occupant_disabled}};
    size_t i;
    if(argc!=2||start()!=0)return 2;
    for(i=0u;i<sizeof(cases)/sizeof(cases[0]);++i)if(strcmp(argv[1],cases[i].name)==0){
        int r=cases[i].run();
        if(r==0)printf("C2 %s PASS\n",argv[1]);
        return r;
    }
    return 2;
}
