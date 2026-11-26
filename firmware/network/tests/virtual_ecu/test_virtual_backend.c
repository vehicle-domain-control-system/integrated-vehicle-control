#include "network/net_virtual.h"
#include <stdio.h>
#include <string.h>
#define CHECK(x) do { if(!(x)) { fprintf(stderr,"%s:%d: %s\n",__FILE__,__LINE__,#x); return 1; } } while(0)
static NetUpper upper;
static NetHalAdapter adapter;
static NetVirtualBackend backend;
static NetUpperPacket delivered;
static unsigned wires;
static NetStatus wire(void *ctx,const NetUpperPacket *p,uint64_t at)
{ (void)ctx; (void)at; delivered=*p; ++wires; return NET_OK; }
static int setup(void)
{
    NetUpperConfig c={0}; NetHalConfig hc; size_t i;
    CHECK(net_virtual_init(&backend,&adapter,7u,3u,1000u,wire,NULL,NULL)==NET_OK);
    hc=net_virtual_hal_config(&backend); CHECK(net_hal_adapter_init(&adapter,&hc,NET_HAL_S32K144)==NET_OK);
    c.node=NET_WIRE_NODE_CIS; c.local_epoch=2u; c.profile=NET_WIRE_CANDIDATE_ONLY; c.port=net_hal_adapter_port(&adapter);
    for(i=0u;i<3u;++i) c.port_epoch[i]=3u;
    for(i=0u;i<NET_UPPER_ROUTE_COUNT;++i) { c.rx_policy[i].bound=true; c.rx_policy[i].maximum_age_ms=1000u; }
    for(i=0u;i<2u;++i) { c.uart_policy[i].candidate_time_policy_selected=true; c.uart_policy[i].assembly_limit_ms=100u; }
    CHECK(net_upper_init(&upper,&c)==NET_OK); return 0;
}
static int publish(NetTxKey *key)
{
    NetWireCisRearRaw rear={0}; NetSourceStamp source={1u,1000u,0u}; NetAgePolicy age={true,1000u,true};
    rear.boot_id=4u; rear.tx_sequence=1u; rear.update_sequence=1u; rear.rear_distance_deci_cm=321u; rear.validity=1u;
    CHECK(net_upper_cis_publish_rear(&upper,&rear,&source,&age,key)==NET_OK);
    CHECK(net_upper_step(&upper,32u)==NET_OK); return 0;
}
static int phases(void)
{
    NetTxKey key; NetUpperTxSnapshot s; NetWireCisRearRaw decoded;
    NetUpperEvent captured; NetUpperPort port;
    CHECK(setup()==0 && publish(&key)==0);
    CHECK(net_upper_tx_snapshot(&upper,key,&s)==NET_OK && s.submitted && !s.admission_known && s.outcome==NET_TX_UNKNOWN && !s.reference_released);
    CHECK(net_virtual_tick(&backend,1001u)==NET_OK && wires==0u); (void)net_upper_step(&upper,32u);
    CHECK(net_upper_tx_snapshot(&upper,key,&s)==NET_OK && s.admission_known && s.accepted && s.outcome==NET_TX_UNKNOWN);
    CHECK(net_virtual_tick(&backend,1002u)==NET_OK && wires==1u); (void)net_upper_step(&upper,32u);
    CHECK(net_wire_can_decode_rear(NET_WIRE_CANDIDATE_ONLY,&delivered.can,&decoded)==NET_WIRE_OK && decoded.rear_distance_deci_cm==321u);
    CHECK(net_upper_tx_snapshot(&upper,key,&s)==NET_OK && s.outcome==NET_TX_UNKNOWN && !s.reference_released);
    CHECK(net_virtual_tick(&backend,1003u)==NET_OK); (void)net_upper_step(&upper,32u);
    CHECK(net_upper_tx_snapshot(&upper,key,&s)==NET_OK && s.outcome==NET_TX_SUCCEEDED && !s.reference_released);
    CHECK(net_upper_retire(&upper,key)==NET_HOLD);
    CHECK(net_virtual_tick(&backend,1004u)==NET_OK); (void)net_upper_step(&upper,32u);
    CHECK(net_upper_tx_snapshot(&upper,key,&s)==NET_OK && s.reference_released);
    CHECK(backend.pending[0].borrow==NULL && backend.releases==1u && net_upper_retire(&upper,key)==NET_OK);
    delivered.can.esi=true;
    CHECK(net_wire_can_decode_rear(NET_WIRE_CANDIDATE_ONLY,&delivered.can,&decoded)==NET_WIRE_OK);
    CHECK(net_virtual_can(&backend,&delivered.can,3u,1004u)==NET_OK);
    port=net_hal_adapter_port(&adapter);
    CHECK(port.ops->poll(port.context,&captured)==NET_OK && captured.kind==NET_UPPER_CAN_RX && captured.can.esi);
    CHECK(memcmp(captured.can.bytes,delivered.can.bytes,delivered.can.length)==0);
    puts("{\"test\":\"virtual_four_phase_borrow\",\"status\":\"PASS\"}"); return 0;
}
static int saturation(void)
{
    uint8_t bytes[NET_HAL_RX_CAPACITY+1u]={0}; uint64_t times[NET_HAL_RX_CAPACITY+1u];
    size_t consumed=99u,i; NetHalObservation o; NetVirtualTiming bad={2u,1u,3u,4u},before;
    CHECK(setup()==0); before=backend.timing;
    CHECK(net_virtual_set_timing(&backend,&bad)==NET_INVALID_ARGUMENT && memcmp(&backend.timing,&before,sizeof(before))==0);
    for(i=0u;i<sizeof(times)/sizeof(times[0]);++i) times[i]=1000u;
    CHECK(net_virtual_uart(&backend,NET_WIRE_UART_C,bytes,times,sizeof(bytes),3u,&consumed)==NET_NO_CAPACITY && consumed==NET_HAL_RX_CAPACITY);
    CHECK(net_hal_adapter_observation(&adapter,&o)==NET_OK && o.rx_dropped==1u);
    CHECK(adapter.rx_count==NET_HAL_RX_CAPACITY && adapter.error_pending[NET_WIRE_UART_C]);
    puts("{\"test\":\"virtual_capture_overflow\",\"consumed\":16,\"lost\":1,\"status\":\"PASS\"}"); return 0;
}
static int reset(void)
{
    NetTxKey key; NetUpperTxSnapshot s; NetTxEvent late={0}; NetVirtualTiming t={1u,5u,8u,10u};
    CHECK(setup()==0 && net_virtual_set_timing(&backend,&t)==NET_OK && publish(&key)==0);
    CHECK(net_virtual_tick(&backend,1001u)==NET_OK); (void)net_upper_step(&upper,32u);
    CHECK(net_virtual_link(&backend,NET_UPPER_LINK_RESET,NET_WIRE_CAN,4u,0u)==NET_OK); (void)net_upper_step(&upper,32u);
    CHECK(backend.pending[0].borrow!=NULL && net_upper_retire(&upper,key)==NET_HOLD);
    CHECK(net_virtual_tick(&backend,1005u)==NET_OK && wires==0u);
    CHECK(net_virtual_tick(&backend,1008u)==NET_OK); (void)net_upper_step(&upper,32u);
    CHECK(net_virtual_tick(&backend,1010u)==NET_OK); (void)net_upper_step(&upper,32u);
    CHECK(net_upper_tx_snapshot(&upper,key,&s)==NET_OK && s.conflict && s.outcome!=NET_TX_SUCCEEDED);
    late.key=key; late.outcome=NET_TX_SUCCEEDED;
    CHECK(net_virtual_tx_fact(&backend,NET_WIRE_CAN,&late,4u)==NET_STALE);
    CHECK(net_upper_retire(&upper,key)==NET_HOLD);
    puts("{\"test\":\"virtual_reset_late_completion\",\"pinned\":true,\"status\":\"PASS\"}"); return 0;
}
static int clock_regression(void)
{
    uint64_t out=77u; NetUpperPort p;
    CHECK(setup()==0); p=net_hal_adapter_port(&adapter);
    CHECK(p.ops->now_ms(p.context,&out)==NET_OK && out==1000u);
    CHECK(net_virtual_clock_fault(&backend,999u)==NET_OK);
    out=77u; CHECK(p.ops->now_ms(p.context,&out)==NET_HOLD && out==77u);
    CHECK(net_upper_step(&upper,32u)==NET_HOLD);
    puts("{\"test\":\"virtual_clock_regression\",\"status\":\"PASS\"}"); return 0;
}
static int early_release(void)
{
    NetTxKey key; NetTxEvent release={0}; NetUpperTxSnapshot s;
    CHECK(setup()==0 && publish(&key)==0);
    release.key=key; release.release_observed=true;
    CHECK(net_virtual_tx_fact(&backend,NET_WIRE_CAN,&release,3u)==NET_OK);
    CHECK(backend.pending[0].borrow==NULL); (void)net_upper_step(&upper,32u);
    CHECK(net_virtual_tick(&backend,1001u)==NET_OK); (void)net_upper_step(&upper,32u);
    CHECK(net_virtual_tick(&backend,1002u)==NET_OK && wires==0u);
    CHECK(net_virtual_tick(&backend,1003u)==NET_OK); (void)net_upper_step(&upper,32u);
    CHECK(net_upper_tx_snapshot(&upper,key,&s)==NET_OK && s.reference_released && s.outcome==NET_TX_FAILED);
    CHECK(net_upper_retire(&upper,key)==NET_OK);
    CHECK(net_virtual_tick(&backend,1004u)==NET_OK && wires==0u);
    puts("{\"test\":\"virtual_early_release_no_dereference\",\"status\":\"PASS\"}"); return 0;
}
static int deadline(void)
{
    NetTxKey key; NetVirtualTiming timing={1u,1000u,1001u,1002u}; NetUpperPacket packet; NetUpperTxSnapshot s;
    CHECK(setup()==0 && net_virtual_set_timing(&backend,&timing)==NET_OK && publish(&key)==0);
    packet=backend.pending[0].borrow->packet;
    CHECK(packet.deadline_bound && packet.age_encoded_at_ms==1000u && packet.transmit_deadline_ms==2000u);
    packet.deadline_bound=false;
    CHECK(net_upper_packet_eligible(&packet,1000u)==NET_HOLD);
    packet.deadline_bound=true;
    CHECK(net_upper_packet_eligible(&packet,1999u)==NET_OK);
    CHECK(net_upper_packet_eligible(&packet,2000u)==NET_EXPIRED);
    CHECK(net_upper_packet_eligible(&packet,999u)==NET_HOLD);
    packet.expires_at_equality=false;
    CHECK(net_upper_packet_eligible(&packet,2000u)==NET_OK && net_upper_packet_eligible(&packet,2001u)==NET_EXPIRED);
    CHECK(net_virtual_tick(&backend,1001u)==NET_OK); (void)net_upper_step(&upper,32u);
    CHECK(net_virtual_tick(&backend,2000u)==NET_OK && wires==0u);
    CHECK(net_virtual_tick(&backend,2001u)==NET_OK); (void)net_upper_step(&upper,32u);
    CHECK(net_virtual_tick(&backend,2002u)==NET_OK); (void)net_upper_step(&upper,32u);
    CHECK(net_upper_tx_snapshot(&upper,key,&s)==NET_OK && s.outcome==NET_TX_FAILED && s.reference_released);
    CHECK(net_upper_retire(&upper,key)==NET_OK);
    /* A mathematical deadline beyond uint64 time must not reject a live
     * local control/retention policy. At every representable instant it is
     * eligible, including MAX; an exactly representable strict MAX is not. */
    {
        NetWireCisRearRaw rear={0}; NetSourceStamp source={7u,1000u,0u};
        NetAgePolicy retention={true,UINT64_MAX,true};
        memset(&upper,0,sizeof(upper)); CHECK(setup()==0);
        rear.boot_id=4u; rear.tx_sequence=1u; rear.update_sequence=1u;
        rear.rear_distance_deci_cm=321u; rear.validity=1u;
        CHECK(net_upper_cis_publish_rear(&upper,&rear,&source,&retention,&key)==NET_OK);
        CHECK(net_upper_step(&upper,32u)==NET_OK);
        CHECK(backend.pending[0].borrow!=NULL);
        packet=backend.pending[0].borrow->packet;
        CHECK(packet.deadline_bound && packet.age_encoded_at_ms==1000u);
        CHECK(packet.transmit_deadline_ms==UINT64_MAX && !packet.expires_at_equality);
        CHECK(net_upper_packet_eligible(&packet,UINT64_MAX)==NET_OK);
        CHECK(net_upper_packet_eligible(&packet,999u)==NET_HOLD);
        CHECK(net_virtual_tick(&backend,1001u)==NET_OK); (void)net_upper_step(&upper,32u);
        CHECK(net_virtual_tick(&backend,1002u)==NET_OK && wires==1u); (void)net_upper_step(&upper,32u);
        CHECK(net_virtual_tick(&backend,1003u)==NET_OK); (void)net_upper_step(&upper,32u);
        CHECK(net_virtual_tick(&backend,1004u)==NET_OK); (void)net_upper_step(&upper,32u);
        CHECK(net_upper_tx_snapshot(&upper,key,&s)==NET_OK && s.outcome==NET_TX_SUCCEEDED && s.reference_released);
        CHECK(net_upper_retire(&upper,key)==NET_OK);
        memset(&upper,0,sizeof(upper)); CHECK(setup()==0); retention.maximum_age_ms=UINT64_MAX-1000u;
        CHECK(net_upper_cis_publish_rear(&upper,&rear,&source,&retention,&key)==NET_OK);
        CHECK(net_upper_step(&upper,32u)==NET_OK && backend.pending[0].borrow!=NULL);
        packet=backend.pending[0].borrow->packet;
        CHECK(packet.transmit_deadline_ms==UINT64_MAX && packet.expires_at_equality);
        CHECK(net_upper_packet_eligible(&packet,UINT64_MAX-1u)==NET_OK);
        CHECK(net_upper_packet_eligible(&packet,UINT64_MAX)==NET_EXPIRED);
    }
    puts("{\"test\":\"virtual_deadline_before_wire\",\"boundary_ms\":2000,\"expired_not_transmitted\":true,\"status\":\"PASS\"}"); return 0;
}
int main(int argc,char **argv)
{
    if(argc!=2) return 2;
    if(strcmp(argv[1],"phases")==0) return phases();
    if(strcmp(argv[1],"saturation")==0) return saturation();
    if(strcmp(argv[1],"reset")==0) return reset();
    if(strcmp(argv[1],"clock")==0) return clock_regression();
    if(strcmp(argv[1],"early_release")==0) return early_release();
    if(strcmp(argv[1],"deadline")==0) return deadline();
    return 2;
}
