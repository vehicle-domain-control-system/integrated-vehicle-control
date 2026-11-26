#include "network/net_domain_c2.h"
/* Test application alone may configure Owner managers. Product adapter never
 * includes these implementation headers. */
#include "domain_interface_state_binding.h"
#include "domain_interface_query_binding.h"
#include <stdio.h>
#include <string.h>
#define CHECK(x) do{if(!(x)){fprintf(stderr,"line %d: %s\n",__LINE__,#x);return 1;}}while(0)
static NetUpper network;
static NetDomainC2 adapter;
static VehicleStateManager_t manager;
static VehicleStateManager_Slot_t slots[4];
static DoorState_t door;
static DomainIf_BcmStatusReport_t bcm;
static CisRearState_t rear;
static DomainIf_CisStatusReport_t cis;
static uint64_t now=1000u;
static NetStatus clock_read(void *c,uint64_t *out){(void)c;*out=now;return NET_OK;}
static bool owner_clock(void *c,uint32_t *out){(void)c;*out=(uint32_t)now;return true;}
static NetStatus submit(void *c,const NetUpperPacket *p,NetTxKey k){(void)c;(void)p;(void)k;return NET_HOLD;}
static NetStatus poll(void *c,NetUpperEvent *out){(void)c;(void)out;return NET_NO_WORK;}
static const NetUpperPortOps ops={clock_read,submit,poll};
int main(void)
{
    VehicleStateManager_SlotConfig_t storage[4];DomainIfStateBinding_SlotMap_t map={0u,1u,2u,3u};
    DomainIfQueryBinding_StatePolicy_t policy;NetDomainC2Api api;NetDomainC2Config binding={0};NetUpperConfig cfg={0};
    NetUpperCanMessage message={0};NetUpperEvent event={0};CisRearState_t readback;size_t i;
    memset(storage,0,sizeof(storage));storage[0].valueStorage=&door;storage[0].valueCapacity=sizeof(door);
    storage[1].valueStorage=&bcm;storage[1].valueCapacity=sizeof(bcm);storage[2].valueStorage=&rear;storage[2].valueCapacity=sizeof(rear);storage[3].valueStorage=&cis;storage[3].valueCapacity=sizeof(cis);
    CHECK(VehicleStateManager_Init(&manager,slots,storage,4u)==VEHICLE_STATE_MANAGER_OK);
    CHECK(DomainIf_Init()==DOMAIN_IF_OK);
    CHECK(DomainIfStateBinding_Configure(&manager,&map,owner_clock,NULL)==DOMAIN_IF_OK);
    memset(&policy,0,sizeof(policy));policy.bcmDoorTtlMs=1000u;policy.bcmStatusTtlMs=1000u;policy.cisRearTtlMs=1000u;policy.cisStatusTtlMs=1000u;
    CHECK(DomainIfQueryBinding_ConfigureState(&manager,&map,&policy)==DOMAIN_IF_OK);
    net_domain_c2_pinned_owner_api(&api);CHECK(api.RxCisRear==DomainIf_RxCisRear&&api.QueryGetCisRear==DomainIf_QueryGetCisRear);CHECK(api.RxMobileRequest==NULL);
    binding.domain_boot=301u;binding.first_can_sequence=1u;binding.value_quality=net_domain_c2_value_quality_provisional;
    CHECK(net_domain_c2_init(&adapter,&network,NULL,&api,&binding)==NET_OK);
    cfg.node=NET_WIRE_NODE_DOMAIN;cfg.local_epoch=1u;cfg.profile=NET_WIRE_CANDIDATE_ONLY;cfg.port.ops=&ops;cfg.owner=net_domain_c2_owner(&adapter);
    for(i=0u;i<3u;++i)cfg.port_epoch[i]=1u;
    for(i=0u;i<NET_UPPER_ROUTE_COUNT;++i){cfg.rx_policy[i].bound=true;cfg.rx_policy[i].maximum_age_ms=1000u;}
    for(i=0u;i<2u;++i){cfg.uart_policy[i].candidate_time_policy_selected=true;cfg.uart_policy[i].assembly_limit_ms=100u;}
    CHECK(net_upper_init(&network,&cfg)==NET_OK);CHECK(net_domain_c2_register(&adapter)==NET_OK);
    message.id=0xa0u;message.data.cis_rear.boot_id=401u;message.data.cis_rear.tx_sequence=9u;message.data.cis_rear.update_sequence=15u;message.data.cis_rear.rear_distance_deci_cm=1234u;message.data.cis_rear.source_age_ms=17u;message.data.cis_rear.validity=1u;
    CHECK(net_upper_can_encode(cfg.profile,&message,&event.can)==NET_WIRE_OK);event.kind=NET_UPPER_CAN_RX;event.link=NET_WIRE_CAN;event.capture.port_epoch=1u;event.capture.timestamp_known=true;event.capture.captured_at_ms=990u;
    CHECK(net_upper_capture(&network,&event)==NET_OK);CHECK(net_upper_step(&network,1u)==NET_OK);
    now=1010u;CHECK(DomainIf_QueryGetCisRear((uint32_t)now,&readback)==DOMAIN_IF_OK);
    CHECK(readback.rearDistanceMm==1234u&&readback.sourceGeneration==401u&&readback.distanceQuality.meta.updateSequence==15u&&readback.distanceQuality.meta.ageMs==37u);
    printf("ACTUAL_PINNED_OWNER CAN bytes -> product decode -> C2 adapter -> DomainIf_RxCisRear -> VehicleStateManager -> DomainIf_QueryGetCisRear: mm=%u age=%u generation=%u PASS\n",readback.rearDistanceMm,readback.distanceQuality.meta.ageMs,readback.sourceGeneration);
    return 0;
}
