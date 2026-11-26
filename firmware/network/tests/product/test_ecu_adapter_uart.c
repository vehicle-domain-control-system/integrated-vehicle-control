#define main inherited_uart_fixture_main
#include "test_upper_uart.c"
#undef main
#include "network/net_ecu_adapter.h"
/* Independent frozen vectors and setup helpers are reused without edits.
 * New path uses real per-type adapter TX, actual Port bytes, stream RX and typed callback. */
static NetUpper as,ar;
static ProductPeer asp,arp;
static NetEcuAdapter asa,ara;
static NetEcuCallbacks ops;
static NetUpperUartMessage received_typed;
static size_t normal_count,held_count;
static NetStatus reentry_status;
static NetStatus record_uart(NetEcuAdapter *a,NetUpperUartMessage *m,const NetUpperDelivery *d)
{
 ProductPeer *peer=a==&ara?&arp:&asp;
 if(d->observation_only)return NET_HOLD;
 m->sequence=(uint16_t)d->source_sequence;received_typed=*m;
 ++normal_count;++peer->uart_count;peer->received.message=*m;peer->metadata=*d;
 reentry_status=net_upper_step(a->service,1u);return NET_OK;
}
static NetStatus on_m_context(void *context,const NetWireMContext *v,const NetUpperDelivery *d)
{NetUpperUartMessage m={0};m.link=NET_WIRE_UART_M;m.type=0x01u;m.data.m_context=*v;return record_uart(context,&m,d);}
static NetStatus on_m_request(void *context,const NetWireMRequest *v,const NetUpperDelivery *d)
{NetUpperUartMessage m={0};m.link=NET_WIRE_UART_M;m.type=0x10u;m.data.m_request=*v;return record_uart(context,&m,d);}
static NetStatus on_m_query(void *context,const NetWireMQueryRaw *v,const NetUpperDelivery *d)
{NetUpperUartMessage m={0};m.link=NET_WIRE_UART_M;m.type=0x11u;m.data.m_query=*v;return record_uart(context,&m,d);}
static NetStatus on_m_warning_ack(void *context,const NetWireMWarningAckRaw *v,const NetUpperDelivery *d)
{NetUpperUartMessage m={0};m.link=NET_WIRE_UART_M;m.type=0x12u;m.data.m_warning_ack=*v;return record_uart(context,&m,d);}
static NetStatus on_m_domain_alive(void *context,const NetWireMDomainAliveRaw *v,const NetUpperDelivery *d)
{NetUpperUartMessage m={0};m.link=NET_WIRE_UART_M;m.type=0x02u;m.data.m_domain_alive=*v;return record_uart(context,&m,d);}
static NetStatus on_m_result(void *context,const NetWireMResult *v,const NetUpperDelivery *d)
{NetUpperUartMessage m={0};m.link=NET_WIRE_UART_M;m.type=0x20u;m.data.m_result=*v;return record_uart(context,&m,d);}
static NetStatus on_m_warning(void *context,const NetWireMWarningRaw *v,const NetUpperDelivery *d)
{NetUpperUartMessage m={0};m.link=NET_WIRE_UART_M;m.type=0x21u;m.data.m_warning=*v;return record_uart(context,&m,d);}
static NetStatus on_m_availability(void *context,const NetWireMAvailabilityRaw *v,const NetUpperDelivery *d)
{NetUpperUartMessage m={0};m.link=NET_WIRE_UART_M;m.type=0x22u;m.data.m_availability=*v;return record_uart(context,&m,d);}
static NetStatus on_m_digital_status(void *context,const NetWireMDigitalStatusRaw *v,const NetUpperDelivery *d)
{NetUpperUartMessage m={0};m.link=NET_WIRE_UART_M;m.type=0x23u;m.data.m_digital_status=*v;return record_uart(context,&m,d);}
static NetStatus on_m_digital_result(void *context,const NetWireMDigitalResultRaw *v,const NetUpperDelivery *d)
{NetUpperUartMessage m={0};m.link=NET_WIRE_UART_M;m.type=0x24u;m.data.m_digital_result=*v;return record_uart(context,&m,d);}
static NetStatus on_m_user_settings(void *context,const NetWireMUserSettingsRaw *v,const NetUpperDelivery *d)
{NetUpperUartMessage m={0};m.link=NET_WIRE_UART_M;m.type=0x25u;m.data.m_user_settings=*v;return record_uart(context,&m,d);}
static NetStatus on_m_query_end(void *context,const NetWireMQueryEndRaw *v,const NetUpperDelivery *d)
{NetUpperUartMessage m={0};m.link=NET_WIRE_UART_M;m.type=0x40u;m.data.m_query_end=*v;return record_uart(context,&m,d);}
static NetStatus on_m_bcm_door_state(void *context,const NetWireMBcmDoorStateRaw *v,const NetUpperDelivery *d)
{NetUpperUartMessage m={0};m.link=NET_WIRE_UART_M;m.type=0x30u;m.data.m_bcm_door_state=*v;return record_uart(context,&m,d);}
static NetStatus on_m_bcm_climate_state(void *context,const NetWireMBcmClimateStateRaw *v,const NetUpperDelivery *d)
{NetUpperUartMessage m={0};m.link=NET_WIRE_UART_M;m.type=0x31u;m.data.m_bcm_climate_state=*v;return record_uart(context,&m,d);}
static NetStatus on_m_bcm_light_state(void *context,const NetWireMBcmLightStateRaw *v,const NetUpperDelivery *d)
{NetUpperUartMessage m={0};m.link=NET_WIRE_UART_M;m.type=0x32u;m.data.m_bcm_light_state=*v;return record_uart(context,&m,d);}
static NetStatus on_m_bcm_status(void *context,const NetWireMBcmStatusRaw *v,const NetUpperDelivery *d)
{NetUpperUartMessage m={0};m.link=NET_WIRE_UART_M;m.type=0x33u;m.data.m_bcm_status=*v;return record_uart(context,&m,d);}
static NetStatus on_m_cis_environment(void *context,const NetWireMCisEnvironmentRaw *v,const NetUpperDelivery *d)
{NetUpperUartMessage m={0};m.link=NET_WIRE_UART_M;m.type=0x34u;m.data.m_cis_environment=*v;return record_uart(context,&m,d);}
static NetStatus on_m_cis_occupant(void *context,const NetWireMCisOccupantRaw *v,const NetUpperDelivery *d)
{NetUpperUartMessage m={0};m.link=NET_WIRE_UART_M;m.type=0x35u;m.data.m_cis_occupant=*v;return record_uart(context,&m,d);}
static NetStatus on_m_cis_rear(void *context,const NetWireMCisRearRaw *v,const NetUpperDelivery *d)
{NetUpperUartMessage m={0};m.link=NET_WIRE_UART_M;m.type=0x36u;m.data.m_cis_rear=*v;return record_uart(context,&m,d);}
static NetStatus on_m_cis_status(void *context,const NetWireMCisStatusRaw *v,const NetUpperDelivery *d)
{NetUpperUartMessage m={0};m.link=NET_WIRE_UART_M;m.type=0x37u;m.data.m_cis_status=*v;return record_uart(context,&m,d);}
static NetStatus on_m_vss_status(void *context,const NetWireMVssStatusRaw *v,const NetUpperDelivery *d)
{NetUpperUartMessage m={0};m.link=NET_WIRE_UART_M;m.type=0x3au;m.data.m_vss_status=*v;return record_uart(context,&m,d);}
static NetStatus on_m_window_state(void *context,const NetWireMWindowStateRaw *v,const NetUpperDelivery *d)
{NetUpperUartMessage m={0};m.link=NET_WIRE_UART_M;m.type=0x38u;m.data.m_window_state=*v;return record_uart(context,&m,d);}
static NetStatus on_m_window_fault(void *context,const NetWireMWindowFaultRaw *v,const NetUpperDelivery *d)
{NetUpperUartMessage m={0};m.link=NET_WIRE_UART_M;m.type=0x39u;m.data.m_window_fault=*v;return record_uart(context,&m,d);}
static NetStatus on_p_vision_result(void *context,const NetWirePVisionResult *v,const NetUpperDelivery *d)
{NetUpperUartMessage m={0};m.link=NET_WIRE_UART_C;m.type=0x60u;m.data.p_vision_result=*v;return record_uart(context,&m,d);}
static NetStatus on_p_vision_status(void *context,const NetWirePVisionStatusRaw *v,const NetUpperDelivery *d)
{NetUpperUartMessage m={0};m.link=NET_WIRE_UART_C;m.type=0x61u;m.data.p_vision_status=*v;return record_uart(context,&m,d);}
static NetStatus on_p_permission(void *context,const NetWirePPermission *v,const NetUpperDelivery *d)
{NetUpperUartMessage m={0};m.link=NET_WIRE_UART_C;m.type=0x62u;m.data.p_permission=*v;return record_uart(context,&m,d);}
static void bind(void)
{memset(&ops,0,sizeof(ops));
 ops.m_context=on_m_context;
 ops.m_request=on_m_request;
 ops.m_query=on_m_query;
 ops.m_warning_ack=on_m_warning_ack;
 ops.m_domain_alive=on_m_domain_alive;
 ops.m_result=on_m_result;
 ops.m_warning=on_m_warning;
 ops.m_availability=on_m_availability;
 ops.m_digital_status=on_m_digital_status;
 ops.m_digital_result=on_m_digital_result;
 ops.m_user_settings=on_m_user_settings;
 ops.m_query_end=on_m_query_end;
 ops.m_bcm_door_state=on_m_bcm_door_state;
 ops.m_bcm_climate_state=on_m_bcm_climate_state;
 ops.m_bcm_light_state=on_m_bcm_light_state;
 ops.m_bcm_status=on_m_bcm_status;
 ops.m_cis_environment=on_m_cis_environment;
 ops.m_cis_occupant=on_m_cis_occupant;
 ops.m_cis_rear=on_m_cis_rear;
 ops.m_cis_status=on_m_cis_status;
 ops.m_vss_status=on_m_vss_status;
 ops.m_window_state=on_m_window_state;
 ops.m_window_fault=on_m_window_fault;
 ops.p_vision_result=on_p_vision_result;
 ops.p_vision_status=on_p_vision_status;
 ops.p_permission=on_p_permission;
}
static NetStatus publish_uart(NetEcuAdapter *a,const NetUpperUartMessage *m,const NetSourceStamp *s,const NetAgePolicy *p,NetTxKey *k)
{switch(m->type){
 case 0x01u:return net_ecu_publish_m_context(a,&m->data.m_context,s,p,k);
 case 0x10u:return net_ecu_publish_m_request(a,&m->data.m_request,s,p,k);
 case 0x11u:return net_ecu_publish_m_query(a,&m->data.m_query,s,p,k);
 case 0x12u:return net_ecu_publish_m_warning_ack(a,&m->data.m_warning_ack,s,p,k);
 case 0x02u:return net_ecu_publish_m_domain_alive(a,&m->data.m_domain_alive,s,p,k);
 case 0x20u:return net_ecu_publish_m_result(a,&m->data.m_result,s,p,k);
 case 0x21u:return net_ecu_publish_m_warning(a,&m->data.m_warning,s,p,k);
 case 0x22u:return net_ecu_publish_m_availability(a,&m->data.m_availability,s,p,k);
 case 0x23u:return net_ecu_publish_m_digital_status(a,&m->data.m_digital_status,s,p,k);
 case 0x24u:return net_ecu_publish_m_digital_result(a,&m->data.m_digital_result,s,p,k);
 case 0x25u:return net_ecu_publish_m_user_settings(a,&m->data.m_user_settings,s,p,k);
 case 0x40u:return net_ecu_publish_m_query_end(a,&m->data.m_query_end,s,p,k);
 case 0x30u:return net_ecu_publish_m_bcm_door_state(a,&m->data.m_bcm_door_state,s,p,k);
 case 0x31u:return net_ecu_publish_m_bcm_climate_state(a,&m->data.m_bcm_climate_state,s,p,k);
 case 0x32u:return net_ecu_publish_m_bcm_light_state(a,&m->data.m_bcm_light_state,s,p,k);
 case 0x33u:return net_ecu_publish_m_bcm_status(a,&m->data.m_bcm_status,s,p,k);
 case 0x34u:return net_ecu_publish_m_cis_environment(a,&m->data.m_cis_environment,s,p,k);
 case 0x35u:return net_ecu_publish_m_cis_occupant(a,&m->data.m_cis_occupant,s,p,k);
 case 0x36u:return net_ecu_publish_m_cis_rear(a,&m->data.m_cis_rear,s,p,k);
 case 0x37u:return net_ecu_publish_m_cis_status(a,&m->data.m_cis_status,s,p,k);
 case 0x3au:return net_ecu_publish_m_vss_status(a,&m->data.m_vss_status,s,p,k);
 case 0x38u:return net_ecu_publish_m_window_state(a,&m->data.m_window_state,s,p,k);
 case 0x39u:return net_ecu_publish_m_window_fault(a,&m->data.m_window_fault,s,p,k);
 case 0x60u:return net_ecu_publish_p_vision_result(a,&m->data.p_vision_result,s,p,k);
 case 0x61u:return net_ecu_publish_p_vision_status(a,&m->data.p_vision_status,s,p,k);
 case 0x62u:return net_ecu_publish_p_permission(a,&m->data.p_permission,s,p,k);
 default:return NET_UNSUPPORTED;}}
static NetStatus held_uart(void *context,const NetUpperUartDelivery *m,const NetUpperDelivery *d)
{(void)context;if(!d->observation_only)return NET_REJECTED;++held_count;received_typed=m->message;arp.received=*m;arp.metadata=*d;return NET_OK;}
static NetStatus held_can(void *context,const NetUpperCanMessage *m,const NetUpperDelivery *d)
{(void)context;(void)m;if(!d->observation_only)return NET_REJECTED;asp.metadata=*d;++asp.can_count;return NET_OK;}
static int init_endpoint(NetUpper *s,ProductPeer *p,NetEcuAdapter *a,uint32_t node,bool typed)
{
 NetUpperConfig cfg={0};size_t j;memset(s,0,sizeof(*s));memset(p,0,sizeof(*p));
 CHECK(net_ecu_adapter_init(a,s,node,&ops,a)==NET_OK);
 cfg.node=node;cfg.local_epoch=7u;cfg.profile=PROFILE;cfg.port.ops=&peer_ops;cfg.port.context=p;cfg.inspection_transport=true;
 if(typed){cfg.owner=net_ecu_adapter_owner(a);cfg.owner.held_uart=held_uart;}
 else{cfg.owner.context=p;cfg.owner.uart=peer_uart;cfg.owner.can=peer_can;cfg.owner.rear=peer_rear;cfg.owner.held_can=held_can;}
 for(j=0u;j<3u;++j)cfg.port_epoch[j]=3u;
 for(j=0u;j<2u;++j){cfg.uart_policy[j].candidate_time_policy_selected=true;cfg.uart_policy[j].assembly_limit_ms=1000u;cfg.uart_sequence_bound[j]=true;cfg.uart_initial_sequence[j]=100u;}
 for(j=0u;j<NET_UPPER_ROUTE_COUNT;++j){cfg.rx_policy[j].bound=true;cfg.rx_policy[j].maximum_age_ms=10000u;cfg.rx_policy[j].expires_at_equality=true;}
 CHECK(net_upper_init(s,&cfg)==NET_OK);return 0;
}
static int finish_tx(NetUpper *s,ProductPeer *p,NetTxKey k,NetWireLink link)
{
 NetUpperEvent e={0};size_t phase;
 for(phase=0u;phase<3u;++phase){memset(&e,0,sizeof(e));e.kind=NET_UPPER_TX_EVENT;e.link=link;e.tx.key=k;
  if(phase==0u){e.tx.admission_observed=true;e.tx.accepted=true;}
  if(phase==1u)e.tx.outcome=NET_TX_SUCCEEDED;
  if(phase==2u){p->borrowed[0]=NULL;e.tx.release_observed=true;}
  CHECK(net_upper_capture(s,&e)==NET_OK);CHECK(net_upper_step(s,32u)==NET_OK);}
 CHECK(net_upper_retire(s,k)==NET_OK);return 0;
}
static int esp_context(NetUpper *s,ProductPeer *p,NetEcuAdapter *a,uint32_t device,uint64_t session)
{
 NetUpperUartMessage m;NetWireUartFrame f=frame_at(0u);NetSourceStamp stamp={1u,100u,0u};NetAgePolicy age={true,10000u,true};NetTxKey k;
 CHECK(net_upper_uart_decode(PROFILE,NET_WIRE_UART_M,NET_WIRE_UPLINK,&f,&m)==NET_WIRE_OK);clear_test_ages(&m);
 m.data.m_context.device_context_id=device;m.data.m_context.session_id=session;p->now=100u;
 CHECK(net_ecu_publish_m_context(a,&m.data.m_context,&stamp,&age,&k)==NET_OK);CHECK(net_upper_step(s,32u)==NET_OK);
 CHECK(finish_tx(s,p,k,NET_WIRE_UART_M)==0);p->sent_count=0u;return 0;
}
static int uart26(void)
{
 size_t i,normal=0u,held=0u;
 bind();for(i=0u;i<sizeof(vectors)/sizeof(vectors[0]);++i){
  NetUpperUartMessage m,owned;NetWireUartFrame f=frame_at(i),expected,callback_frame;
  const NetWireRegistryEntry *entry=net_wire_registry_find(vectors[i].link,vectors[i].type);
  NetSourceStamp stamp={77u,1000u,0u};NetAgePolicy policy={true,10000u,true};NetTxKey k={99u,99u};uint32_t device;uint64_t session;size_t b;
  bool inspect=vectors[i].type==0x38u || vectors[i].type==0x39u;
  CHECK(entry!=NULL);CHECK(init_endpoint(&as,&asp,&asa,entry->sender,false)==0);CHECK(init_endpoint(&ar,&arp,&ara,entry->receivers,true)==0);
  CHECK(net_upper_uart_decode(PROFILE,vectors[i].link,vectors[i].direction,&f,&m)==NET_WIRE_OK);clear_test_ages(&m);
  CHECK(net_upper_uart_encode(PROFILE,&m,&f)==NET_WIRE_OK);device=(uint32_t)read_le_test(f.bytes+8u,4u);session=read_le_test(f.bytes+12u,8u);
  if(m.link==NET_WIRE_UART_M && m.type!=1u && m.type!=2u){
   if(entry->sender==NET_WIRE_NODE_DOMAIN)CHECK(inject_context(&as,&asp,device,session)==0);
   if(entry->receivers==NET_WIRE_NODE_DOMAIN)CHECK(inject_context(&ar,&arp,device,session)==0);
   if(entry->sender==NET_WIRE_NODE_ESP32)CHECK(esp_context(&as,&asp,&asa,device,session)==0);
   if(entry->receivers==NET_WIRE_NODE_ESP32)CHECK(esp_context(&ar,&arp,&ara,device,session)==0);
  }
  if(projection_can_id(m.type)!=0u){
   NetUpperCanMessage can,accepted;CHECK(from_projection(&m,&can)==0);
   if(inspect){NetUpperEvent e={0};e.kind=NET_UPPER_CAN_RX;e.link=NET_WIRE_CAN;e.capture.port_epoch=3u;e.capture.timestamp_known=true;e.capture.captured_at_ms=1000u;
    CHECK(net_upper_can_encode(PROFILE,&can,&e.can)==NET_WIRE_OK);asp.now=1000u;CHECK(net_upper_capture(&as,&e)==NET_OK);CHECK(net_upper_step(&as,32u)==NET_HOLD);
    CHECK(asp.can_count==1u && asp.metadata.observation_only);stamp.observation_id=asp.metadata.receipt_id;stamp.observed_at_ms=1000u;stamp.original_age_ms=net_upper_can_max_age(&can);accepted=can;
   }else CHECK(inject_can(&as,&asp,&can,&accepted,&stamp)==0);
   CHECK(net_upper_uart_project_can(PROFILE,&accepted,device,session,(uint32_t)read_le_test(f.bytes+20u,4u),(uint32_t)read_le_test(f.bytes+24u,4u),100u,&m)==NET_WIRE_OK);
  }
  if(m.type==0x62u){NetUpperCanMessage can={0},accepted;can.id=0x80u;can.data.domain_permission.boot_id=10u;can.data.domain_permission.tx_sequence=20u;can.data.domain_permission.update_sequence=m.data.p_permission.update_sequence;
   CHECK(inject_can(&as,&asp,&can,&accepted,&stamp)==0);m.data.p_permission.vehicle_power_permission=0u;m.data.p_permission.value_quality=0u;m.data.p_permission.quality_reason=0u;}
  asp.now=1000u;m.sequence=(uint16_t)as.next_uart_sequence[(size_t)m.link-1u];owned=m;
  CHECK(net_upper_uart_encode(PROFILE,&owned,&expected)==NET_WIRE_OK);normal_count=0u;held_count=0u;
  if(inspect){CHECK(publish_uart(&asa,&m,&stamp,&policy,&k)==NET_HOLD);CHECK(k.token==99u);CHECK(net_upper_submit_inspection_uart(&as,&m,&stamp,&policy,&k)==NET_OK);}
  else CHECK(publish_uart(&asa,&m,&stamp,&policy,&k)==NET_OK);
  memset(&m,0xa5,sizeof(m));CHECK(net_upper_step(&as,32u)==NET_OK);CHECK(asp.sent_count==1u && asp.borrowed[0]!=NULL);
  CHECK(asp.sent[0].uart.length==expected.length && memcmp(asp.sent[0].uart.bytes,expected.bytes,expected.length)==0);
  for(b=0u;b<asp.sent[0].uart.length;++b){NetUpperEvent e={0};e.kind=NET_UPPER_UART_BYTE;e.link=owned.link;e.byte=asp.sent[0].uart.bytes[b];e.capture.port_epoch=3u;e.capture.timestamp_known=true;e.capture.captured_at_ms=2000u+b;
   arp.now=e.capture.captured_at_ms;CHECK(net_upper_capture(&ar,&e)==NET_OK);CHECK(net_upper_step(&ar,32u)==(b+1u==expected.length?(inspect?NET_HOLD:NET_OK):NET_NO_WORK));}
  CHECK(normal_count==(inspect?0u:1u) && held_count==(inspect?1u:0u));CHECK(arp.metadata.observation_only==inspect);
  if(!inspect)CHECK(reentry_status==NET_BUSY);
  CHECK(net_upper_uart_encode(PROFILE,&received_typed,&callback_frame)==NET_WIRE_OK);
  CHECK(callback_frame.length==expected.length && memcmp(callback_frame.bytes,expected.bytes,expected.length)==0);
  CHECK(finish_tx(&as,&asp,k,owned.link)==0);if(inspect)++held;else ++normal;
  printf("ADAPTER_UART wire=%s actual_port_bytes=%lu typed=%lu inspection=%lu execution_authorized=false PASS\n",vectors[i].name,(unsigned long)expected.length,(unsigned long)normal_count,(unsigned long)held_count);
 }
 CHECK(normal==24u && held==2u);return 0;
}
static int uart_missing(void)
{
 NetWireUartFrame f=frame_at(23u);NetUpperUartMessage m;NetUpperOwner owner;NetUpperUartDelivery u={0};NetUpperDelivery d={0};
 bind();memset(&ops,0,sizeof(ops));CHECK(init_endpoint(&ar,&arp,&ara,NET_WIRE_NODE_CIS,true)==0);
 CHECK(net_upper_uart_decode(PROFILE,vectors[23].link,vectors[23].direction,&f,&m)==NET_WIRE_OK);
 u.message=m;owner=net_ecu_adapter_owner(&ara);normal_count=0u;CHECK(owner.uart(owner.context,&u,&d)==NET_HOLD);CHECK(normal_count==0u);return 0;
}
int main(int argc,char **argv)
{if(argc!=2)return 2;if(strcmp(argv[1],"uart26")==0)return uart26();if(strcmp(argv[1],"uart_missing")==0)return uart_missing();return 2;}
