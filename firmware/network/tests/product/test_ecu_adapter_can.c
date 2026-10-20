#define main inherited_can_fixture_main
#include "test_upper_can_routes.c"
#undef main
#include "network/net_ecu_adapter.h"
/* Existing independent scalar/literal fixtures are reused without edits.
 * These tests add the real ECU typed adapter to the product queue/wire/RX path. */
typedef struct { size_t expected,normal,held; bool reenter; NetStatus nested; NetUpperCanMessage value; NetUpperDelivery delivery; } AdapterProbe;
static AdapterProbe ap;
static NetEcuAdapter txa,rxa;
static NetEcuCallbacks callbacks;
static NetStatus record_can(NetEcuAdapter *a,const NetUpperCanMessage *m,const NetUpperDelivery *d)
{
 AdapterProbe *p=&ap;
 if(d->observation_only || m->id!=fixtures[p->expected].id)return NET_REJECTED;
 ++p->normal;p->value=*m;p->delivery=*d;
 if(p->reenter)p->nested=net_upper_step(a->service,1u);
 return NET_OK;
}
static NetStatus on_domain_permission(void *context,const NetWireDomainPermissionRaw *v,const NetUpperDelivery *d)
{NetUpperCanMessage m={0};m.id=0x080u;m.data.domain_permission=*v;return record_can(context,&m,d);}
static NetStatus on_bcm_door_command(void *context,const NetWireBcmDoorCommandRaw *v,const NetUpperDelivery *d)
{NetUpperCanMessage m={0};m.id=0x100u;m.data.bcm_door_command=*v;return record_can(context,&m,d);}
static NetStatus on_bcm_climate_command(void *context,const NetWireBcmClimateCommandRaw *v,const NetUpperDelivery *d)
{NetUpperCanMessage m={0};m.id=0x110u;m.data.bcm_climate_command=*v;return record_can(context,&m,d);}
static NetStatus on_bcm_light_command(void *context,const NetWireBcmLightCommandRaw *v,const NetUpperDelivery *d)
{NetUpperCanMessage m={0};m.id=0x120u;m.data.bcm_light_command=*v;return record_can(context,&m,d);}
static NetStatus on_window_command(void *context,const NetWireWindowCommandRaw *v,const NetUpperDelivery *d)
{NetUpperCanMessage m={0};m.id=0x090u;m.data.window_command=*v;return record_can(context,&m,d);}
static NetStatus on_bcm_door_state(void *context,const NetWireBcmDoorStateRaw *v,const NetUpperDelivery *d)
{NetUpperCanMessage m={0};m.id=0x200u;m.data.bcm_door_state=*v;return record_can(context,&m,d);}
static NetStatus on_bcm_climate_state(void *context,const NetWireBcmClimateStateRaw *v,const NetUpperDelivery *d)
{NetUpperCanMessage m={0};m.id=0x210u;m.data.bcm_climate_state=*v;return record_can(context,&m,d);}
static NetStatus on_bcm_light_state(void *context,const NetWireBcmLightStateRaw *v,const NetUpperDelivery *d)
{NetUpperCanMessage m={0};m.id=0x220u;m.data.bcm_light_state=*v;return record_can(context,&m,d);}
static NetStatus on_bcm_result(void *context,const NetWireBcmResultRaw *v,const NetUpperDelivery *d)
{NetUpperCanMessage m={0};m.id=0x140u;m.data.bcm_result=*v;return record_can(context,&m,d);}
static NetStatus on_bcm_event(void *context,const NetWireBcmEventRaw *v,const NetUpperDelivery *d)
{NetUpperCanMessage m={0};m.id=0x141u;m.data.bcm_event=*v;return record_can(context,&m,d);}
static NetStatus on_bcm_status(void *context,const NetWireBcmStatusRaw *v,const NetUpperDelivery *d)
{NetUpperCanMessage m={0};m.id=0x230u;m.data.bcm_status=*v;return record_can(context,&m,d);}
static NetStatus on_cis_environment(void *context,const NetWireCisEnvironmentRaw *v,const NetUpperDelivery *d)
{NetUpperCanMessage m={0};m.id=0x300u;m.data.cis_environment=*v;return record_can(context,&m,d);}
static NetStatus on_cis_occupant(void *context,const NetWireCisOccupantRaw *v,const NetUpperDelivery *d)
{NetUpperCanMessage m={0};m.id=0x310u;m.data.cis_occupant=*v;return record_can(context,&m,d);}
static NetStatus on_cis_rear(void *context,const NetWireCisRearRaw *v,const NetUpperDelivery *d)
{NetUpperCanMessage m={0};m.id=0x0a0u;m.data.cis_rear=*v;return record_can(context,&m,d);}
static NetStatus on_cis_status(void *context,const NetWireCisStatusRaw *v,const NetUpperDelivery *d)
{NetUpperCanMessage m={0};m.id=0x320u;m.data.cis_status=*v;return record_can(context,&m,d);}
static NetStatus on_window_state(void *context,const NetWireWindowStateRaw *v,const NetUpperDelivery *d)
{NetUpperCanMessage m={0};m.id=0x0b0u;m.data.window_state=*v;return record_can(context,&m,d);}
static NetStatus on_window_event(void *context,const NetWireWindowEventRaw *v,const NetUpperDelivery *d)
{NetUpperCanMessage m={0};m.id=0x091u;m.data.window_event=*v;return record_can(context,&m,d);}
static NetStatus on_window_result(void *context,const NetWireWindowResultRaw *v,const NetUpperDelivery *d)
{NetUpperCanMessage m={0};m.id=0x092u;m.data.window_result=*v;return record_can(context,&m,d);}
static NetStatus on_window_fault(void *context,const NetWireWindowFaultRaw *v,const NetUpperDelivery *d)
{NetUpperCanMessage m={0};m.id=0x330u;m.data.window_fault=*v;return record_can(context,&m,d);}
static NetStatus on_vss_event(void *context,const NetWireVssEventRaw *v,const NetUpperDelivery *d)
{NetUpperCanMessage m={0};m.id=0x160u;m.data.vss_event=*v;return record_can(context,&m,d);}
static NetStatus on_vss_warning_state(void *context,const NetWireVssWarningStateRaw *v,const NetUpperDelivery *d)
{NetUpperCanMessage m={0};m.id=0x0c0u;m.data.vss_warning_state=*v;return record_can(context,&m,d);}
static NetStatus on_vss_status(void *context,const NetWireVssStatusRaw *v,const NetUpperDelivery *d)
{NetUpperCanMessage m={0};m.id=0x340u;m.data.vss_status=*v;return record_can(context,&m,d);}
static void bind_callbacks(void)
{
 memset(&callbacks,0,sizeof(callbacks));
 callbacks.domain_permission=on_domain_permission;
 callbacks.bcm_door_command=on_bcm_door_command;
 callbacks.bcm_climate_command=on_bcm_climate_command;
 callbacks.bcm_light_command=on_bcm_light_command;
 callbacks.window_command=on_window_command;
 callbacks.bcm_door_state=on_bcm_door_state;
 callbacks.bcm_climate_state=on_bcm_climate_state;
 callbacks.bcm_light_state=on_bcm_light_state;
 callbacks.bcm_result=on_bcm_result;
 callbacks.bcm_event=on_bcm_event;
 callbacks.bcm_status=on_bcm_status;
 callbacks.cis_environment=on_cis_environment;
 callbacks.cis_occupant=on_cis_occupant;
 callbacks.cis_rear=on_cis_rear;
 callbacks.cis_status=on_cis_status;
 callbacks.window_state=on_window_state;
 callbacks.window_event=on_window_event;
 callbacks.window_result=on_window_result;
 callbacks.window_fault=on_window_fault;
 callbacks.vss_event=on_vss_event;
 callbacks.vss_warning_state=on_vss_warning_state;
 callbacks.vss_status=on_vss_status;
}
static NetStatus publish(NetEcuAdapter *a,const NetUpperCanMessage *m,const NetSourceStamp *s,const NetAgePolicy *p,NetTxKey *k)
{
 switch(m->id){
 case 0x080u:return net_ecu_publish_domain_permission(a,&m->data.domain_permission,s,p,k);
 case 0x100u:return net_ecu_publish_bcm_door_command(a,&m->data.bcm_door_command,s,p,k);
 case 0x110u:return net_ecu_publish_bcm_climate_command(a,&m->data.bcm_climate_command,s,p,k);
 case 0x120u:return net_ecu_publish_bcm_light_command(a,&m->data.bcm_light_command,s,p,k);
 case 0x090u:return net_ecu_publish_window_command(a,&m->data.window_command,s,p,k);
 case 0x200u:return net_ecu_publish_bcm_door_state(a,&m->data.bcm_door_state,s,p,k);
 case 0x210u:return net_ecu_publish_bcm_climate_state(a,&m->data.bcm_climate_state,s,p,k);
 case 0x220u:return net_ecu_publish_bcm_light_state(a,&m->data.bcm_light_state,s,p,k);
 case 0x140u:return net_ecu_publish_bcm_result(a,&m->data.bcm_result,s,p,k);
 case 0x141u:return net_ecu_publish_bcm_event(a,&m->data.bcm_event,s,p,k);
 case 0x230u:return net_ecu_publish_bcm_status(a,&m->data.bcm_status,s,p,k);
 case 0x300u:return net_ecu_publish_cis_environment(a,&m->data.cis_environment,s,p,k);
 case 0x310u:return net_ecu_publish_cis_occupant(a,&m->data.cis_occupant,s,p,k);
 case 0x0a0u:return net_ecu_publish_cis_rear(a,&m->data.cis_rear,s,p,k);
 case 0x320u:return net_ecu_publish_cis_status(a,&m->data.cis_status,s,p,k);
 case 0x0b0u:return net_ecu_publish_window_state(a,&m->data.window_state,s,p,k);
 case 0x091u:return net_ecu_publish_window_event(a,&m->data.window_event,s,p,k);
 case 0x092u:return net_ecu_publish_window_result(a,&m->data.window_result,s,p,k);
 case 0x330u:return net_ecu_publish_window_fault(a,&m->data.window_fault,s,p,k);
 case 0x160u:return net_ecu_publish_vss_event(a,&m->data.vss_event,s,p,k);
 case 0x0c0u:return net_ecu_publish_vss_warning_state(a,&m->data.vss_warning_state,s,p,k);
 case 0x340u:return net_ecu_publish_vss_status(a,&m->data.vss_status,s,p,k);
 default:return NET_UNSUPPORTED;}
}
static NetStatus on_held(void *context,const NetUpperCanMessage *m,const NetUpperDelivery *d)
{
 AdapterProbe *p=&ap;(void)context;
 if(!d->observation_only || m->id!=fixtures[p->expected].id)return NET_REJECTED;
 ++p->held;p->value=*m;p->delivery=*d;return NET_OK;
}
static int init_receiver(uint32_t node,bool inspect,bool missing)
{
 NetUpperConfig cfg={0};size_t j;
 memset(&rx,0,sizeof(rx));memset(&rx_port,0,sizeof(rx_port));rx_port.now=105u;
 CHECK(net_ecu_adapter_init(&rxa,&rx,node,&callbacks,&ap)==NET_OK);
 /* callbacks context is the typed adapter, whose owned context is ap. */
 rxa.context=&rxa;
 cfg.node=node;cfg.local_epoch=9u;cfg.profile=NET_WIRE_CANDIDATE_ONLY;
 cfg.port.ops=&port_ops;cfg.port.context=&rx_port;cfg.owner=net_ecu_adapter_owner(&rxa);
 cfg.owner.held_can=on_held;cfg.inspection_transport=inspect;
 for(j=0u;j<3u;++j)cfg.port_epoch[j]=1u;
 for(j=0u;j<2u;++j){cfg.uart_policy[j].candidate_time_policy_selected=true;cfg.uart_policy[j].assembly_limit_ms=1000u;cfg.uart_policy[j].expires_at_equality=true;}
 for(j=0u;j<NET_UPPER_ROUTE_COUNT;++j){cfg.rx_policy[j].bound=true;cfg.rx_policy[j].maximum_age_ms=70000u;cfg.rx_policy[j].expires_at_equality=true;cfg.rx_per_field_age[j]=true;}
 (void)missing;
 CHECK(net_upper_init(&rx,&cfg)==NET_OK);return 0;
}
static int adapter_route(size_t index,uint32_t receiver)
{
 const Fixture *f=&fixtures[index];NetUpperCanMessage m,expected,copy;NetSourceStamp stamp={7u,100u,0u},received_source;
 NetAgePolicy policy={true,70000u,true};NetTxKey key={99u,99u};NetUpperEvent e;NetUpperDelivery d;
 bool held=f->hold || receiver==NET_WIRE_NODE_WINDOW;size_t part;
 case_name=f->name;memset(&ap,0,sizeof(ap));ap.expected=index;ap.reenter=true;bind_callbacks();
 CHECK(initialize(&tx,&tx_port,f->sender,f->id==0x310u?&bridge_owner:NULL)==NET_OK);
 /* Fresh test sender config is copied before any submission. */
 if(f->hold){NetUpperConfig cfg=tx.config;cfg.inspection_transport=true;memset(&tx,0,sizeof(tx));CHECK(net_upper_init(&tx,&cfg)==NET_OK);}
 CHECK(net_ecu_adapter_init(&txa,&tx,f->sender,&callbacks,&ap)==NET_OK);
 CHECK(init_receiver(receiver,true,false)==0);
 fixture_message(index,0u,&m);stamp.original_age_ms=f->age;
 if(f->id==0x310u){CHECK(bootstrap_vision(&m)==0);stamp.observation_id=1u;}
 if(f->hold){CHECK(publish(&txa,&m,&stamp,&policy,&key)==NET_HOLD);CHECK(key.epoch==99u && key.token==99u);CHECK(net_upper_submit_inspection_can(&tx,&m,&stamp,&policy,&key)==NET_OK);}
 else CHECK(publish(&txa,&m,&stamp,&policy,&key)==NET_OK);
 memset(&m,0xa5,sizeof(m));tx_port.now=103u;CHECK(net_upper_step(&tx,32u)==NET_OK);
 CHECK(tx_port.borrowed!=NULL && tx_port.submitted==1u);
 CHECK(tx_port.borrowed->can.length==f->length && tx_port.borrowed->can.dlc==f->dlc && tx_port.borrowed->can.fdf && tx_port.borrowed->can.brs);
 CHECK(memcmp(tx_port.borrowed->can.bytes,f->dispatched,f->length)==0);
 e=frame_event(&tx_port.borrowed->can);CHECK(net_upper_capture(&rx,&e)==NET_OK);CHECK(net_upper_step(&rx,32u)==(held?NET_HOLD:NET_OK));
 fixture_message(index,3u,&expected);CHECK(equal_typed(index,&ap.value,&expected));
 CHECK(ap.normal==(held?0u:1u) && ap.held==(held?1u:0u));CHECK(ap.delivery.observation_only==held);
 if(!held)CHECK(ap.nested==NET_BUSY);
 CHECK(net_upper_received_can(&rx,f->id,&copy,&d,&received_source)==(held?NET_HOLD:NET_OK));
 for(part=0u;part<3u;++part){memset(&e,0,sizeof(e));e.kind=NET_UPPER_TX_EVENT;e.link=NET_WIRE_CAN;e.tx.key=key;
  if(part==0u){e.tx.admission_observed=true;e.tx.accepted=true;}if(part==1u)e.tx.outcome=NET_TX_SUCCEEDED;
  if(part==2u){tx_port.borrowed=NULL;e.tx.release_observed=true;}CHECK(net_upper_capture(&tx,&e)==NET_OK);CHECK(net_upper_step(&tx,32u)==NET_OK);}
 CHECK(net_upper_retire(&tx,key)==NET_OK);
 printf("ADAPTER_CAN wire=%s receiver=%lu exact_literal=true typed=%lu inspection=%lu execution_authorized=false PASS\n",f->name,(unsigned long)receiver,(unsigned long)ap.normal,(unsigned long)ap.held);
 return 0;
}
static int can25(void)
{
 size_t i;static const uint32_t extras[]={NET_WIRE_NODE_CIS,NET_WIRE_NODE_WINDOW,NET_WIRE_NODE_VSS};
 for(i=0u;i<22u;++i)CHECK(adapter_route(i,fixtures[i].receiver)==0);
 for(i=0u;i<3u;++i)CHECK(adapter_route(0u,extras[i])==0);
 return 0;
}
static int missing_alias(void)
{
 NetUpperCanMessage m;NetSourceStamp stamp={1u,100u,0u};NetAgePolicy p={true,500u,true};NetTxKey k={99u,99u};NetUpperEvent e;NetUpperDelivery d;
 case_name="missing_alias";memset(&ap,0,sizeof(ap));ap.expected=13u;bind_callbacks();callbacks.cis_rear=NULL;
 CHECK(init_receiver(NET_WIRE_NODE_DOMAIN,false,true)==0);fixture_message(13u,0u,&m);
 memset(&e,0,sizeof(e));e.kind=NET_UPPER_CAN_RX;e.link=NET_WIRE_CAN;e.capture.port_epoch=1u;e.capture.timestamp_known=true;e.capture.captured_at_ms=103u;
 CHECK(net_upper_can_encode(NET_WIRE_CANDIDATE_ONLY,&m,&e.can)==NET_WIRE_OK);CHECK(net_upper_capture(&rx,&e)==NET_OK);
 CHECK(net_upper_step(&rx,32u)==NET_HOLD);CHECK(ap.normal==0u);CHECK(net_upper_received_can(&rx,m.id,&m,&d,&stamp)==NET_NO_WORK);
 CHECK(initialize(&tx,&tx_port,NET_WIRE_NODE_CIS,NULL)==NET_OK);CHECK(net_ecu_adapter_init(&txa,&tx,NET_WIRE_NODE_CIS,&callbacks,&ap)==NET_OK);
 CHECK(net_ecu_publish_cis_rear(&txa,&m.data.cis_rear,&stamp,&p,(NetTxKey *)&m.data.cis_rear)==NET_INVALID_ARGUMENT);
 CHECK(net_ecu_publish_cis_rear(&txa,NULL,&stamp,&p,&k)==NET_INVALID_ARGUMENT);CHECK(k.epoch==99u && k.token==99u && tx.next_token==1u);return 0;
}
int main(int argc,char **argv)
{case_name=argc==2?argv[1]:"missing";if(argc!=2)return 2;if(strcmp(argv[1],"can25")==0)return can25();if(strcmp(argv[1],"missing_alias")==0)return missing_alias();return 2;}
