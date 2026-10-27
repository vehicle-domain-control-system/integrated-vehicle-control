#include "network/net_domain_c2.h"
/* Test application alone may use the DOMAIN runtime and its private
 * transitions (domain_transition_binding.h); the product adapter never
 * includes these implementation headers. */
#include "domain_runtime.h"
#include "domain_transition_binding.h"
#include <stdio.h>
#include <string.h>
/* CIS environment and Occupant BOOT transitions against the actual DOMAIN
 * runtime (pinned DOMAIN sources): CAN bytes -> product decode -> C2 adapter
 * barrier -> adapter step -> test binding -> DOMAIN private transition ->
 * DomainIf_RxCisCabinEnvironment / DomainIf_RxCisOccupant -> DOMAIN managers ->
 * DomainIf_QueryGetCisEnvironment / DomainIf_QueryGetCisOccupant.
 * Contract: docs/network/interface/02_NODES/DOMAIN/25_PRIVATE_ROUTING.md (CIS
 * environment BOOT change: hold the new BOOT, call the private transition
 * once, deliver after its confirmed completion) and
 * firmware/DOMAIN/docs/CIS_OCCUPANT_BINDING_CONTRACT_DRAFT.md (Network keeps
 * Occupant ingress closed until the private transition for that Pi generation
 * succeeded). */
#define CHECK(x) do{if(!(x)){fprintf(stderr,"line %d: %s\n",__LINE__,#x);return 1;}}while(0)
static NetUpper network;
static NetDomainC2 adapter;
static DomainRuntime_t runtime;
static uint64_t now=1000u;
static NetStatus clock_read(void *c,uint64_t *out){(void)c;*out=now;return NET_OK;}
static bool owner_clock(void *c,uint32_t *out){(void)c;*out=(uint32_t)now;return true;}
static NetStatus submit(void *c,const NetUpperPacket *p,NetTxKey k){(void)c;(void)p;(void)k;return NET_OK;}
static NetStatus poll(void *c,NetUpperEvent *out){(void)c;(void)out;return NET_NO_WORK;}
static const NetUpperPortOps ops={clock_read,submit,poll};

/* DOMAIN first (runtime: managers, C2 core, bindings), then Network. */
static int start(bool occupant_enabled)
{
    DomainRuntime_Config_t rc;NetDomainC2Api api;NetDomainC2Config binding;NetUpperConfig cfg;size_t i;
    memset(&rc,0,sizeof(rc));
    rc.timeProvider=owner_clock;rc.firstCommandId=1u;
    rc.resultPolicy.firstResponseTimeoutEnabled=true;rc.resultPolicy.firstResponseTimeoutMs=100u;
    rc.resultPolicy.finalResultTimeoutEnabled=true;rc.resultPolicy.finalResultTimeoutMs=1000u;
    rc.resultPolicy.doorEvidenceTtlEnabled=true;rc.resultPolicy.doorEvidenceTtlMs=500u;
    rc.permissionPolicy.reevaluationPeriodMs=200u;rc.permissionPolicy.sourceValidityMs=500u;
    rc.cisOccupantEnabled=occupant_enabled;rc.cisOccupantPolicy.presenceTtlMs=500u;rc.cisOccupantPolicy.countTtlMs=500u;
    rc.stateQueryPolicy.bcmDoorTtlMs=500u;rc.stateQueryPolicy.bcmStatusTtlMs=500u;
    rc.stateQueryPolicy.cisRearTtlMs=500u;rc.stateQueryPolicy.cisStatusTtlMs=500u;
    CHECK(DomainRuntime_Init(&runtime,&rc)==DOMAIN_RUNTIME_OK&&DomainRuntime_IsReady(&runtime));
    net_domain_c2_pinned_owner_api(&api);
    CHECK(api.RxCisOccupant==DomainIf_RxCisOccupant&&api.QueryGetCisOccupant==DomainIf_QueryGetCisOccupant);
    memset(&binding,0,sizeof(binding));
    binding.domain_boot=301u;binding.first_can_sequence=1u;binding.value_quality=net_domain_c2_value_quality_provisional;
    binding.cis_transition_barriers=true;binding.transition=domain_transition_binding(&runtime);
    CHECK(net_domain_c2_init(&adapter,&network,NULL,&api,&binding)==NET_OK);
    memset(&cfg,0,sizeof(cfg));
    cfg.node=NET_WIRE_NODE_DOMAIN;cfg.local_epoch=1u;cfg.profile=NET_WIRE_CANDIDATE_ONLY;cfg.port.ops=&ops;
    cfg.owner=net_domain_c2_owner(&adapter);
    for(i=0u;i<3u;++i)cfg.port_epoch[i]=1u;
    for(i=0u;i<NET_UPPER_ROUTE_COUNT;++i){cfg.rx_policy[i].bound=true;cfg.rx_policy[i].maximum_age_ms=2000u;}
    CHECK(net_upper_init(&network,&cfg)==NET_OK);CHECK(net_domain_c2_register(&adapter)==NET_OK);
    return 0;
}
static int feed(const NetUpperCanMessage *m)
{
    NetUpperEvent e;memset(&e,0,sizeof(e));
    e.kind=NET_UPPER_CAN_RX;e.link=NET_WIRE_CAN;e.capture.port_epoch=1u;e.capture.timestamp_known=true;e.capture.captured_at_ms=now;
    CHECK(net_upper_can_encode(NET_WIRE_CANDIDATE_ONLY,m,&e.can)==NET_WIRE_OK);
    CHECK(net_upper_capture(&network,&e)==NET_OK);(void)net_upper_step(&network,1u);
    return 0;
}
static NetUpperCanMessage env_frame(uint32_t boot,uint32_t tx,int16_t temperature,uint16_t humidity,uint32_t lux,uint32_t seq)
{
    NetUpperCanMessage m;memset(&m,0,sizeof(m));m.id=0x300u;
    m.data.cis_environment.boot_id=boot;m.data.cis_environment.tx_sequence=tx;
    m.data.cis_environment.temperature_centi_c=temperature;m.data.cis_environment.humidity_centi_percent=humidity;
    m.data.cis_environment.illuminance_lux=lux;
    m.data.cis_environment.temperature_validity=1u;m.data.cis_environment.humidity_validity=1u;m.data.cis_environment.illuminance_validity=1u;
    m.data.cis_environment.temperature_sequence=seq;m.data.cis_environment.humidity_sequence=seq+1u;m.data.cis_environment.illuminance_sequence=seq+2u;
    m.data.cis_environment.temperature_age_ms=30u;m.data.cis_environment.humidity_age_ms=40u;m.data.cis_environment.illuminance_age_ms=50u;
    return m;
}
static NetUpperCanMessage occ_frame(uint32_t boot,uint32_t tx,uint32_t pi,uint32_t vision,uint8_t count)
{
    NetUpperCanMessage m;memset(&m,0,sizeof(m));m.id=0x310u;
    m.data.cis_occupant.boot_id=boot;m.data.cis_occupant.tx_sequence=tx;m.data.cis_occupant.pi_boot_id=pi;
    m.data.cis_occupant.vision_sequence=vision;m.data.cis_occupant.vision_age_ms=20u;
    m.data.cis_occupant.occupant_presence=1u;m.data.cis_occupant.occupant_count=count;
    m.data.cis_occupant.presence_validity=1u;m.data.cis_occupant.count_validity=1u;m.data.cis_occupant.vision_function_status=2u;
    return m;
}
static bool env_is(const CisCabinEnvironmentState_t *s,int16_t temperature,uint16_t humidity,uint32_t lux,uint32_t seq)
{
    return s->cabinTemperature==temperature&&s->cabinHumidity==humidity&&s->cabinIlluminance==lux&&
        s->temperatureQuality.meta.quality==DATA_QUALITY_OK&&s->temperatureQuality.meta.updateSequence==seq&&s->temperatureQuality.meta.ageMs==30u&&
        s->humidityQuality.meta.quality==DATA_QUALITY_OK&&s->humidityQuality.meta.updateSequence==seq+1u&&s->humidityQuality.meta.ageMs==40u&&
        s->illuminanceQuality.meta.quality==DATA_QUALITY_OK&&s->illuminanceQuality.meta.updateSequence==seq+2u&&s->illuminanceQuality.meta.ageMs==50u;
}

/* The environment: DOMAIN refuses values before its transition; Network holds
 * the first CIS BOOT, the adapter step applies epoch 1, the next frame is
 * stored and read back. A CIS restart: epoch 2 invalidates the old values in
 * DOMAIN before the new BOOT's frame is stored. */
static int env_transition(void)
{
    NetDomainC2TransitionBinding tb=domain_transition_binding(&runtime);NetDomainC2TransitionReceipt r;
    CisCabinEnvironmentState_t q;DomainCisEnvironmentPriv_Snapshot_t snap;NetUpperCanMessage m;
    const NetDomainC2Barrier *b=&adapter.barrier[NET_DOMAIN_C2_ENVIRONMENT_BARRIER];
    /* Runtime not ready yet: no private context, no DOMAIN call. */
    memset(&r,0,sizeof(r));CHECK(tb.environment(tb.context,401u,1u,&r)==NET_DOMAIN_C2_TRANSITION_UNAVAILABLE&&r.revision==0u);
    CHECK(start(true)==0);
    {CisCabinEnvironmentState_t direct;memset(&direct,0,sizeof(direct));
        direct.temperatureQuality.validity=VALUE_VALIDITY_VALID;direct.temperatureQuality.meta.quality=DATA_QUALITY_OK;
        direct.humidityQuality=direct.temperatureQuality;direct.illuminanceQuality=direct.temperatureQuality;
        CHECK(DomainIf_RxCisCabinEnvironment(&direct)==DOMAIN_IF_REJECTED);}
    m=env_frame(401u,1u,2150,4500u,321u,11u);CHECK(feed(&m)==0);
    CHECK(b->state==NET_DOMAIN_C2_BARRIER_PENDING&&b->id==401u&&b->held==1u&&adapter.boundary_calls==1u);
    CHECK(DomainIf_QueryGetCisEnvironment((uint32_t)now,&q)==DOMAIN_IF_OK&&q.temperatureQuality.meta.quality==DATA_QUALITY_NO_DATA);
    CHECK(net_domain_c2_step(&adapter)==NET_OK);
    CHECK(b->state==NET_DOMAIN_C2_BARRIER_OPEN&&b->id==401u&&b->next_epoch==2u&&b->revision==1u);
    CHECK(DomainCisEnvironmentPriv_GetSnapshot(DomainRuntime_GetCisEnvironmentPrivate(&runtime),&snap)==DOMAIN_CIS_ENVIRONMENT_PRIV_OK);
    CHECK(snap.hasAppliedTransition&&snap.currentTransition.admittedCisBootId==401u&&snap.currentTransition.transitionEpoch==1u);
    CHECK(snap.domainBarrierRevision==1u&&snap.newDataAdmissionAllowed);
    m=env_frame(401u,2u,2150,4500u,321u,11u);CHECK(feed(&m)==0);
    CHECK(adapter.last_c2_status==DOMAIN_IF_OK&&adapter.boundary_calls==2u);
    CHECK(DomainIf_QueryGetCisEnvironment((uint32_t)now,&q)==DOMAIN_IF_OK&&env_is(&q,2150,4500u,321u,11u));
    /* CIS restarts with BOOT 402. */
    now+=100u;
    m=env_frame(402u,1u,1980,4100u,77u,1u);CHECK(feed(&m)==0);
    CHECK(b->state==NET_DOMAIN_C2_BARRIER_PENDING&&b->id==402u&&adapter.boundary_calls==2u);
    CHECK(net_domain_c2_step(&adapter)==NET_OK);
    CHECK(b->state==NET_DOMAIN_C2_BARRIER_OPEN&&b->id==402u&&b->next_epoch==3u&&b->revision==2u&&b->calls==2u);
    CHECK(DomainIf_QueryGetCisEnvironment((uint32_t)now,&q)==DOMAIN_IF_OK);
    CHECK(q.temperatureQuality.meta.quality==DATA_QUALITY_NO_DATA&&q.humidityQuality.meta.quality==DATA_QUALITY_NO_DATA);
    CHECK(q.illuminanceQuality.meta.quality==DATA_QUALITY_NO_DATA&&q.cabinTemperature==0);
    m=env_frame(402u,2u,1980,4100u,77u,1u);CHECK(feed(&m)==0);
    CHECK(adapter.last_c2_status==DOMAIN_IF_OK&&adapter.boundary_calls==3u);
    CHECK(DomainIf_QueryGetCisEnvironment((uint32_t)now,&q)==DOMAIN_IF_OK&&env_is(&q,1980,4100u,77u,1u));
    CHECK(DomainCisEnvironmentPriv_GetSnapshot(DomainRuntime_GetCisEnvironmentPrivate(&runtime),&snap)==DOMAIN_CIS_ENVIRONMENT_PRIV_OK);
    CHECK(snap.currentTransition.admittedCisBootId==402u&&snap.currentTransition.transitionEpoch==2u&&snap.domainBarrierRevision==2u);
    /* A late frame of the ended BOOT changes nothing. */
    m=env_frame(401u,3u,3000,9000u,999u,12u);CHECK(feed(&m)==0);
    CHECK(adapter.boundary_calls==3u&&DomainIf_QueryGetCisEnvironment((uint32_t)now,&q)==DOMAIN_IF_OK&&env_is(&q,1980,4100u,77u,1u));
    printf("ACTUAL_DOMAIN environment: CIS BOOT 401 epoch 1, 402 epoch 2, revision %u, temperature %d PASS\n",
        (unsigned)snap.domainBarrierRevision,(int)q.cabinTemperature);
    return 0;
}
/* The Occupant (enabled with both TTLs): the Pi generation 6401 gets epoch 1,
 * its next frame is stored and read back; a Pi restart (6402, the CIS keeps
 * its BOOT) gets epoch 2, the old generation is invalidated and its late frame
 * stays held. */
static int occupant_transition(void)
{
    NetDomainC2TransitionBinding tb=domain_transition_binding(&runtime);NetDomainC2TransitionReceipt r;
    CisOccupantState_t q;DomainCisOccupantPriv_Snapshot_t snap;NetUpperCanMessage m;
    const NetDomainC2Barrier *b=&adapter.barrier[NET_DOMAIN_C2_OCCUPANT_BARRIER];
    memset(&r,0,sizeof(r));CHECK(tb.occupant(tb.context,6401u,1u,&r)==NET_DOMAIN_C2_TRANSITION_UNAVAILABLE&&r.revision==0u);
    CHECK(start(true)==0);
    {CisOccupantState_t direct;memset(&direct,0,sizeof(direct));
        direct.presenceQuality.validity=VALUE_VALIDITY_VALID;direct.presenceQuality.meta.quality=DATA_QUALITY_OK;
        direct.countQuality=direct.presenceQuality;direct.sourceGeneration=6401u;direct.visionFunctionStatus=CIS_FUNCTION_STATUS_ACTIVE;
        CHECK(DomainIf_RxCisOccupant(&direct)==DOMAIN_IF_REJECTED);}
    m=occ_frame(401u,1u,6401u,21u,2u);CHECK(feed(&m)==0);
    CHECK(b->state==NET_DOMAIN_C2_BARRIER_PENDING&&b->id==6401u&&b->held==1u&&adapter.boundary_calls==1u);
    CHECK(net_domain_c2_step(&adapter)==NET_OK);
    CHECK(b->state==NET_DOMAIN_C2_BARRIER_OPEN&&b->id==6401u&&b->next_epoch==2u&&b->revision==1u);
    CHECK(DomainCisOccupantPriv_GetSnapshot(DomainRuntime_GetCisOccupantPrivate(&runtime),&snap)==DOMAIN_CIS_OCCUPANT_PRIV_OK);
    CHECK(snap.hasAppliedTransition&&snap.currentTransition.admittedSourceGeneration==6401u&&snap.currentTransition.transitionEpoch==1u);
    m=occ_frame(401u,2u,6401u,22u,2u);CHECK(feed(&m)==0);
    CHECK(adapter.last_c2_status==DOMAIN_IF_OK&&adapter.boundary_calls==2u);
    CHECK(DomainIf_QueryGetCisOccupant((uint32_t)now,&q)==DOMAIN_IF_OK);
    CHECK(q.occupantPresent&&q.occupantCount==2u&&q.sourceGeneration==6401u&&q.visionFunctionStatus==CIS_FUNCTION_STATUS_ACTIVE);
    CHECK(q.presenceQuality.meta.quality==DATA_QUALITY_OK&&q.presenceQuality.meta.updateSequence==22u&&q.presenceQuality.meta.ageMs==20u);
    CHECK(q.countQuality.meta.quality==DATA_QUALITY_OK&&q.countQuality.meta.updateSequence==22u);
    /* The Pi restarts with generation 6402; the CIS keeps BOOT 401. */
    now+=100u;
    m=occ_frame(401u,3u,6402u,1u,1u);CHECK(feed(&m)==0);
    CHECK(b->state==NET_DOMAIN_C2_BARRIER_PENDING&&b->id==6402u&&adapter.boundary_calls==2u);
    CHECK(net_domain_c2_step(&adapter)==NET_OK);
    CHECK(b->state==NET_DOMAIN_C2_BARRIER_OPEN&&b->id==6402u&&b->next_epoch==3u&&b->revision==2u);
    CHECK(DomainIf_QueryGetCisOccupant((uint32_t)now,&q)==DOMAIN_IF_OK);
    CHECK(q.presenceQuality.meta.quality!=DATA_QUALITY_OK&&q.countQuality.meta.quality!=DATA_QUALITY_OK);
    m=occ_frame(401u,4u,6402u,2u,1u);CHECK(feed(&m)==0);
    CHECK(adapter.last_c2_status==DOMAIN_IF_OK&&adapter.boundary_calls==3u);
    CHECK(DomainIf_QueryGetCisOccupant((uint32_t)now,&q)==DOMAIN_IF_OK);
    CHECK(q.occupantCount==1u&&q.sourceGeneration==6402u&&q.presenceQuality.meta.quality==DATA_QUALITY_OK&&q.presenceQuality.meta.updateSequence==2u);
    CHECK(DomainCisOccupantPriv_GetSnapshot(DomainRuntime_GetCisOccupantPrivate(&runtime),&snap)==DOMAIN_CIS_OCCUPANT_PRIV_OK);
    CHECK(snap.currentTransition.admittedSourceGeneration==6402u&&snap.currentTransition.transitionEpoch==2u&&snap.domainBarrierRevision==2u);
    /* A late frame of the old Pi generation (same CIS run) stays held. */
    m=occ_frame(401u,5u,6401u,23u,3u);CHECK(feed(&m)==0);
    CHECK(adapter.boundary_calls==3u&&b->state==NET_DOMAIN_C2_BARRIER_OPEN&&b->id==6402u&&b->calls==2u);
    CHECK(DomainIf_QueryGetCisOccupant((uint32_t)now,&q)==DOMAIN_IF_OK&&q.sourceGeneration==6402u&&q.occupantCount==1u);
    printf("ACTUAL_DOMAIN Occupant: Pi 6401 epoch 1, 6402 epoch 2, revision %u, count %u PASS\n",
        (unsigned)snap.domainBarrierRevision,(unsigned)q.occupantCount);
    return 0;
}
int main(int argc,char **argv)
{
    int r;
    if(argc!=2)return 2;
    if(strcmp(argv[1],"env_transition")==0)r=env_transition();
    else if(strcmp(argv[1],"occupant_transition")==0)r=occupant_transition();
    else return 2;
    if(r==0)printf("C2 actual %s PASS\n",argv[1]);
    return r;
}
