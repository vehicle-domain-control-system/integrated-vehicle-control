#include "network/net_upper.h"
#include "../../src/l03_message_service/m04_message_service/net_upper_private.h"
#include <stdio.h>
#include <string.h>

/* Counterexamples use actual product identity guards and actual RX admission.
 * Only the monotonic clock, empty physical Port and external Owner are faked. */
bool net_upper_can_same_observation(const NetUpperCanMessage *,const NetUpperCanMessage *);
bool net_upper_uart_same_observation(const NetUpperUartMessage *,const NetUpperUartMessage *);
static const char *case_name;
#define CHECK(x) do { if(!(x)) { printf("FAIL semantic.%s:%d %s\n",case_name,__LINE__,#x);return 1;} } while(0)
static NetUpperCanMessage ca,cb;
static NetUpperUartMessage ua,ub;
static NetUpper service;
static uint64_t now_ms;
static size_t owner_calls;
static NetStatus clock_read(void *context,uint64_t *out)
{ (void)context;*out=now_ms;return NET_OK; }
static NetStatus no_submit(void *context,const NetUpperPacket *packet,NetTxKey key)
{ (void)context;(void)packet;(void)key;return NET_NOT_CONFIGURED; }
static NetStatus no_poll(void *context,NetUpperEvent *out)
{ (void)context;(void)out;return NET_NO_WORK; }
static const NetUpperPortOps port_ops={clock_read,no_submit,no_poll};
static NetStatus receive_rear(void *context,const NetUpperRear *rear,const NetUpperDelivery *delivery)
{ (void)context;(void)rear;(void)delivery;++owner_calls;return NET_OK; }
static NetStatus receive_uart(void *context,const NetUpperUartDelivery *uart,const NetUpperDelivery *delivery)
{ (void)context;(void)uart;(void)delivery;++owner_calls;return NET_OK; }
static int setup(uint32_t node)
{
    NetUpperConfig c={0};size_t i;
    memset(&service,0,sizeof(service));owner_calls=0u;now_ms=100u;
    c.node=node;c.local_epoch=1u;c.profile=NET_WIRE_CANDIDATE_ONLY;c.port.ops=&port_ops;
    c.owner.rear=receive_rear;c.owner.uart=receive_uart;
    for(i=0u;i<3u;++i)c.port_epoch[i]=1u;
    for(i=0u;i<NET_UPPER_ROUTE_COUNT;++i){c.rx_policy[i].bound=true;c.rx_policy[i].maximum_age_ms=500u;c.rx_policy[i].expires_at_equality=true;}
    for(i=0u;i<2u;++i){c.uart_policy[i].candidate_time_policy_selected=true;
        c.uart_policy[i].assembly_limit_ms=100u;c.uart_policy[i].expires_at_equality=true;}
    CHECK(net_upper_init(&service,&c)==NET_OK);return 0;
}
static void rear(void)
{
    memset(&ca,0,sizeof(ca));ca.id=0xa0u;ca.data.cis_rear.boot_id=1u;
    ca.data.cis_rear.tx_sequence=1u;ca.data.cis_rear.update_sequence=7u;
    ca.data.cis_rear.rear_distance_deci_cm=123u;ca.data.cis_rear.validity=1u;
    ca.data.cis_rear.source_age_ms=10u;cb=ca;cb.data.cis_rear.tx_sequence=2u;cb.data.cis_rear.source_age_ms=15u;
}
static int inject_can(const NetUpperCanMessage *m,NetStatus expected)
{
    NetUpperEvent e={0};e.kind=NET_UPPER_CAN_RX;e.link=NET_WIRE_CAN;
    e.capture.port_epoch=1u;e.capture.timestamp_known=true;e.capture.captured_at_ms=now_ms;
    CHECK(net_upper_can_encode(NET_WIRE_CANDIDATE_ONLY,m,&e.can)==NET_WIRE_OK);
    CHECK(net_upper_capture(&service,&e)==NET_OK);CHECK(net_upper_step(&service,1u)==expected);return 0;
}
static int can_rear_collision(void)
{
    rear();CHECK(net_upper_can_original_age(&ca,&cb,5u)==NET_OK);
    cb.data.cis_rear.rear_distance_deci_cm=456u;
    CHECK(net_upper_can_original_age(&ca,&cb,5u)==NET_HOLD);
    cb.data.cis_rear.update_sequence=8u;CHECK(net_upper_can_original_age(&ca,&cb,5u)==NET_OK);return 0;
}
static int can_permission_collision(void)
{
    memset(&ca,0,sizeof(ca));ca.id=0x80u;ca.data.domain_permission.boot_id=1u;
    ca.data.domain_permission.update_sequence=17u;ca.data.domain_permission.vehicle_power_permission=1u;
    cb=ca;cb.data.domain_permission.tx_sequence=2u;cb.data.domain_permission.source_age_ms=5u;
    CHECK(net_upper_can_original_age(&ca,&cb,5u)==NET_OK);
    cb.data.domain_permission.vehicle_power_permission=0u;CHECK(net_upper_can_original_age(&ca,&cb,5u)==NET_HOLD);
    cb.data.domain_permission.update_sequence=18u;CHECK(net_upper_can_original_age(&ca,&cb,5u)==NET_OK);return 0;
}
static int can_independent_lanes(void)
{
    memset(&ca,0,sizeof(ca));ca.id=0x300u;ca.data.cis_environment.boot_id=1u;
    ca.data.cis_environment.temperature_sequence=1u;ca.data.cis_environment.humidity_sequence=1u;ca.data.cis_environment.illuminance_sequence=1u;
    cb=ca;CHECK(net_upper_can_add_age(&cb,5u)==NET_WIRE_OK);
    cb.data.cis_environment.humidity_sequence=2u;cb.data.cis_environment.humidity_centi_percent=125u;
    cb.data.cis_environment.humidity_age_ms=0u;CHECK(net_upper_can_original_age(&ca,&cb,5u)==NET_OK);
    CHECK(!net_upper_can_same_observation(&ca,&cb));
    cb.data.cis_environment.temperature_centi_c=100;CHECK(net_upper_can_original_age(&ca,&cb,5u)==NET_HOLD);return 0;
}
static int can_vss_lanes(void)
{
    memset(&ca,0,sizeof(ca));ca.id=0xc0u;ca.data.vss_warning_state.boot_id=1u;
    ca.data.vss_warning_state.antipinch_source_boot=2u;ca.data.vss_warning_state.antipinch_source_sequence=3u;
    ca.data.vss_warning_state.occupant_hazard_source_boot=2u;ca.data.vss_warning_state.occupant_hazard_source_sequence=3u;
    cb=ca;CHECK(net_upper_can_add_age(&cb,5u)==NET_WIRE_OK);
    cb.data.vss_warning_state.occupant_hazard_source_sequence=4u;cb.data.vss_warning_state.occupant_hazard_state=1u;
    cb.data.vss_warning_state.occupant_hazard_original_age_ms=0u;
    CHECK(net_upper_can_original_age(&ca,&cb,5u)==NET_OK);
    cb.data.vss_warning_state.antipinch_state=1u;CHECK(net_upper_can_original_age(&ca,&cb,5u)==NET_HOLD);return 0;
}
static int can_command_immutable(void)
{
    memset(&ca,0,sizeof(ca));ca.id=0x100u;ca.data.bcm_door_command.boot_id=1u;
    ca.data.bcm_door_command.target_boot_id=2u;ca.data.bcm_door_command.command_id=3u;
    ca.data.bcm_door_command.door_lock_target=1u;cb=ca;cb.data.bcm_door_command.command_age_ms=5u;
    CHECK(net_upper_can_original_age(&ca,&cb,5u)==NET_OK);
    cb.data.bcm_door_command.door_lock_target=2u;CHECK(net_upper_can_original_age(&ca,&cb,5u)==NET_HOLD);return 0;
}
static int can_result_progression(void)
{
    memset(&ca,0,sizeof(ca));ca.id=0x140u;ca.data.bcm_result.boot_id=1u;
    ca.data.bcm_result.occurrence_id=1u;ca.data.bcm_result.result=0u;ca.data.bcm_result.door_event=1u;
    cb=ca;cb.data.bcm_result.result=2u;cb.data.bcm_result.event_age_ms=5u;
    CHECK(net_upper_can_original_age(&ca,&cb,5u)==NET_OK);CHECK(!net_upper_can_same_observation(&ca,&cb));
    cb.data.bcm_result.door_event=2u;CHECK(net_upper_can_original_age(&ca,&cb,5u)==NET_HOLD);return 0;
}
static int uart_context_lanes(void)
{
    memset(&ua,0,sizeof(ua));ua.link=NET_WIRE_UART_M;ua.type=1u;
    ua.data.m_context.device_context_id=1u;ua.data.m_context.session_id=2u;
    ub=ua;CHECK(net_upper_uart_add_age(&ub,5u)==NET_WIRE_OK);
    ub.data.m_context.app_update_sequence=1u;ub.data.m_context.app_active_state=1u;ub.data.m_context.app_age_ms=0u;
    CHECK(net_upper_uart_original_age(&ua,&ub,5u)==NET_OK);CHECK(!net_upper_uart_same_observation(&ua,&ub));
    ub.data.m_context.proximity_state=1u;CHECK(net_upper_uart_original_age(&ua,&ub,5u)==NET_HOLD);return 0;
}
static int uart_permission_collision(void)
{
    memset(&ua,0,sizeof(ua));ua.link=NET_WIRE_UART_C;ua.type=0x62u;
    ua.data.p_permission.cis_boot_id=1u;ua.data.p_permission.update_sequence=2u;ua.data.p_permission.vehicle_power_permission=1u;
    ub=ua;ub.sequence=2u;ub.data.p_permission.source_age_ms=5u;
    CHECK(net_upper_uart_original_age(&ua,&ub,5u)==NET_OK);CHECK(net_upper_uart_same_observation(&ua,&ub));
    ub.data.p_permission.value_quality=1u;CHECK(net_upper_uart_original_age(&ua,&ub,5u)==NET_HOLD);return 0;
}
static int uart_projection_collision(void)
{
    memset(&ua,0,sizeof(ua));ua.link=NET_WIRE_UART_M;ua.type=0x36u;
    ua.data.m_cis_rear.original_can_boot_id=1u;ua.data.m_cis_rear.update_sequence=2u;
    ua.data.m_cis_rear.rear_distance_deci_cm=123u;ub=ua;
    ub.data.m_cis_rear.source_age_ms=5u;ub.data.m_cis_rear.query_id=2u;
    CHECK(net_upper_uart_original_age(&ua,&ub,5u)==NET_OK);CHECK(!net_upper_uart_same_observation(&ua,&ub));
    ub.data.m_cis_rear.rear_distance_deci_cm=456u;CHECK(net_upper_uart_original_age(&ua,&ub,5u)==NET_HOLD);return 0;
}
static int uart_vision_callback(void)
{
    memset(&ua,0,sizeof(ua));ua.link=NET_WIRE_UART_C;ua.type=0x60u;
    ua.data.p_vision_result.pi_boot_id=1u;ua.data.p_vision_result.vision_sequence=2u;
    ub=ua;ub.sequence=2u;ub.data.p_vision_result.vision_age_ms=5u;
    CHECK(net_upper_uart_original_age(&ua,&ub,5u)==NET_OK);
    CHECK(!net_upper_uart_same_observation(&ua,&ub)); /* VC-F04 still calls Owner. */
    ub.data.p_vision_result.occupant_count=2u;CHECK(net_upper_uart_original_age(&ua,&ub,5u)==NET_HOLD);return 0;
}
static int rx_collision_atomic(void)
{
    NetUpperCanMessage before,after;NetUpperDelivery db,da;NetSourceStamp sb,sa;
    CHECK(setup(NET_WIRE_NODE_DOMAIN)==0);rear();CHECK(inject_can(&ca,NET_OK)==0);CHECK(owner_calls==1u);
    CHECK(net_upper_received_can(&service,0xa0u,&before,&db,&sb)==NET_OK);
    now_ms=105u;cb.data.cis_rear.rear_distance_deci_cm=456u;CHECK(inject_can(&cb,NET_HOLD)==0);CHECK(owner_calls==1u);
    CHECK(net_upper_received_can(&service,0xa0u,&after,&da,&sa)==NET_OK);
    CHECK(after.data.cis_rear.rear_distance_deci_cm==123u);CHECK(da.receipt_id==db.receipt_id);
    CHECK(sa.observation_id==sb.observation_id);CHECK(da.capture.captured_at_ms==db.capture.captured_at_ms);return 0;
}
static int rx_duplicate_refresh(void)
{
    NetUpperCanMessage after;NetUpperDelivery d;NetSourceStamp stamp;
    CHECK(setup(NET_WIRE_NODE_DOMAIN)==0);rear();CHECK(inject_can(&ca,NET_OK)==0);CHECK(owner_calls==1u);
    now_ms=105u;CHECK(inject_can(&cb,NET_DUPLICATE)==0);CHECK(owner_calls==1u);
    CHECK(net_upper_received_can(&service,0xa0u,&after,&d,&stamp)==NET_OK);
    CHECK(after.data.cis_rear.tx_sequence==2u);CHECK(d.capture.captured_at_ms==105u);
    CHECK(after.data.cis_rear.source_age_ms==15u);CHECK(stamp.original_age_ms==15u);
    CHECK(inject_can(&cb,NET_DUPLICATE)==0);CHECK(owner_calls==1u);return 0;
}
int main(int argc,char **argv)
{
    int result=2;case_name=argc==2?argv[1]:"missing";
#define RUN(n) if(strcmp(case_name,#n)==0)result=n()
    RUN(can_rear_collision);else RUN(can_permission_collision);else RUN(can_independent_lanes);
    else RUN(can_vss_lanes);else RUN(can_command_immutable);else RUN(can_result_progression);
    else RUN(uart_context_lanes);else RUN(uart_permission_collision);else RUN(uart_projection_collision);
    else RUN(uart_vision_callback);else RUN(rx_collision_atomic);else RUN(rx_duplicate_refresh);
#undef RUN
    if(result==0)printf("PASS semantic.%s\n",case_name);
    return result;
}
