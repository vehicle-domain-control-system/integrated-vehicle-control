#include "network/net_domain_c2.h"
#include <stdio.h>
#include <string.h>
#define CHECK(x) do{if(!(x)){fprintf(stderr,"line %d: %s\n",__LINE__,#x);return 1;}}while(0)
static NetUpper service;
static NetDomainC2 adapter;
static NetUpperQuery query;
static DomainIf_TxPort_t ports;
static uint64_t now=1000u;
static CisRearState_t rear;
static DomainIf_MobileRequest_t request;
static DomainIf_BcmCommandResult_t result;
static unsigned calls,failures;
static DomainIf_Status_t rx_return=DOMAIN_IF_OK,fail_return=DOMAIN_IF_OK;
static bool confirms=true;
static NetUpperPacket packet;
static NetTxKey key;
static bool initialized(void){return true;}
static DomainIf_Status_t set_port(const DomainIf_TxPort_t *p){if(p)ports=*p;else memset(&ports,0,sizeof(ports));return DOMAIN_IF_OK;}
static DomainIf_Status_t rx_rear(const CisRearState_t *p){rear=*p;++calls;return rx_return;}
static DomainIf_Status_t rx_request(const DomainIf_MobileRequest_t *p){request=*p;++calls;return DOMAIN_IF_OK;}
static DomainIf_Status_t rx_result(const DomainIf_BcmCommandResult_t *p){result=*p;++calls;return DOMAIN_IF_OK;}
static DomainIf_Status_t rx_failure(const NetworkLocalFailure_t *p){++failures;if(!p->hasCommandId||p->commandId!=77u||p->identityGeneration!=301u)return DOMAIN_IF_INVALID_ARGUMENT;return fail_return;}
static DomainIf_Status_t get_rear(uint32_t at,CisRearState_t *out){*out=rear;out->distanceQuality.meta.ageMs+=at-1000u;return DOMAIN_IF_OK;}
static DomainIf_Status_t passive(DeviceContextId_t device,SessionId_t session){return device==7u&&session==11u?DOMAIN_IF_OK:DOMAIN_IF_REJECTED;}
static NetStatus local_reason(void *context,const NetUpperTxSnapshot *s,NetworkLocalFailureReason_t *out){(void)context;if(s->local_status!=NET_REJECTED)return NET_HOLD;*out=NETWORK_LOCAL_FAILURE_CONTEXT_INVALIDATED_BEFORE_SEND;return NET_OK;}
static bool confirm(void *c,uint16_t id,uint64_t receipt,uint32_t boot,uint32_t seq){(void)c;return confirms&&id==0xa0u&&receipt!=0u&&boot==401u&&seq==1u;}
static NetStatus clock_read(void *c,uint64_t *out){(void)c;*out=now;return NET_OK;}
static NetStatus submit(void *c,const NetUpperPacket *p,NetTxKey k){(void)c;packet=*p;key=k;return NET_OK;}
static NetStatus poll(void *c,NetUpperEvent *out){(void)c;(void)out;return NET_NO_WORK;}
static const NetUpperPortOps ops={clock_read,submit,poll};
static int start(void){NetDomainC2Api api={0};NetDomainC2Config cfg={0};NetUpperConfig config={0};size_t i;
api.IsInitialized=initialized;api.SetTxPort=set_port;api.RxCisRear=rx_rear;api.RxMobileRequest=rx_request;api.RxBcmCommandResult=rx_result;api.RxNetworkLocalFailure=rx_failure;api.QueryGetCisRear=get_rear;api.ValidatePassiveQueryContext=passive;
cfg.domain_boot=301u;cfg.bcm_boot=201u;cfg.first_can_sequence=1u;cfg.command_accept_limit_ms=100u;cfg.event_use_limit_ms=100u;cfg.transmit_policy.bound=true;cfg.transmit_policy.maximum_age_ms=100u;cfg.transmit_policy.expires_at_equality=true;cfg.value_quality=net_domain_c2_value_quality_provisional;cfg.failure_sink_ready=true;cfg.confirm_snapshot=confirm;cfg.local_failure_reason=local_reason;
CHECK(net_domain_c2_init(&adapter,&service,&query,&api,&cfg)==NET_OK);
config.node=NET_WIRE_NODE_DOMAIN;config.local_epoch=3u;config.profile=NET_WIRE_CANDIDATE_ONLY;config.port.ops=&ops;config.owner=net_domain_c2_owner(&adapter);
for(i=0;i<3u;++i)config.port_epoch[i]=4u;
for(i=0;i<NET_UPPER_ROUTE_COUNT;++i){config.rx_policy[i].bound=true;config.rx_policy[i].maximum_age_ms=10000u;}
for(i=0;i<2u;++i){config.uart_policy[i].candidate_time_policy_selected=true;config.uart_policy[i].assembly_limit_ms=100u;config.uart_policy[i].expires_at_equality=true;config.uart_sequence_bound[i]=true;config.uart_initial_sequence[i]=1u;}
CHECK(net_upper_init(&service,&config)==NET_OK);CHECK(net_domain_c2_register(&adapter)==NET_OK);return 0;}
static NetUpperDelivery metadata(void){NetUpperDelivery d;memset(&d,0,sizeof(d));d.source_boot=401u;d.source_sequence=1u;d.capture.port_epoch=4u;d.capture.captured_at_ms=1000u;d.capture.timestamp_known=true;d.receipt_id=1u;d.freshness_known=true;return d;}
static NetUpperCanMessage rear_message(void){NetUpperCanMessage m;memset(&m,0,sizeof(m));m.id=0xa0u;m.data.cis_rear.boot_id=401u;m.data.cis_rear.tx_sequence=1u;m.data.cis_rear.update_sequence=17u;m.data.cis_rear.rear_distance_deci_cm=123u;m.data.cis_rear.validity=1u;m.data.cis_rear.source_age_ms=23u;return m;}
static DomainIf_BcmDoorCommand_t door_command(void){DomainIf_BcmDoorCommand_t c;memset(&c,0,sizeof(c));c.context.commandId=77u;c.context.origin=COMMAND_ORIGIN_MOBILE_REQUEST;c.target=DOOR_TARGET_UNLOCK;return c;}
static int units(void){uint16_t out=999u;NetUpperCanMessage m=rear_message();NetUpperDelivery d=metadata();NetUpperOwner owner=net_domain_c2_owner(&adapter);d.local_elapsed_ms=9u;
CHECK(owner.can(owner.context,&m,&d)==NET_OK);CHECK(rear.rearDistanceMm==123u&&rear.distanceQuality.meta.ageMs==32u&&rear.sourceGeneration==401u);
CHECK(net_domain_c2_mm_to_cm(123u,NET_DOMAIN_CM_EXACT_ONLY,&out)==NET_HOLD&&out==999u);
CHECK(net_domain_c2_mm_to_cm(123u,NET_DOMAIN_CM_FLOOR,&out)==NET_OK&&out==12u);
CHECK(net_domain_c2_mm_to_cm(125u,NET_DOMAIN_CM_NEAREST,&out)==NET_OK&&out==13u);
CHECK(net_domain_c2_mm_to_cm(UINT16_MAX,NET_DOMAIN_CM_NEAREST,&out)==NET_INVALID_ARGUMENT&&out==13u);return 0;}
static int observer_failure(void){NetUpperCanMessage m=rear_message();NetUpperDelivery d=metadata();NetUpperOwner owner=net_domain_c2_owner(&adapter);rx_return=DOMAIN_IF_QUEUE_FULL;
CHECK(owner.can(owner.context,&m,&d)==NET_NO_CAPACITY);CHECK(calls==1u);d.source_sequence=2u;m.data.cis_rear.tx_sequence=2u;
CHECK(owner.can(owner.context,&m,&d)==NET_HOLD);CHECK(calls==1u);return 0;}
static int request_tags(void){NetUpperUartDelivery u;NetUpperDelivery d=metadata();NetUpperOwner owner=net_domain_c2_owner(&adapter);unsigned i;static const uint8_t kind[]={1,2,2,2,3,3,3,4};static const uint8_t op[]={1,1,2,3,1,2,3,1};
for(i=0;i<8u;++i){memset(&u,0,sizeof(u));u.port_epoch=4u;u.message.link=NET_WIRE_UART_M;u.message.type=0x10u;u.message.sequence=(uint16_t)(i+1u);u.message.data.m_request.device_context_id=7u;u.message.data.m_request.session_id=11u;u.message.data.m_request.request_id=i+1u;u.message.data.m_request.kind=kind[i];u.message.data.m_request.operation=op[i];
if(i==1u){u.message.data.m_request.args[0]=0x9cu;u.message.data.m_request.args[1]=0xffu;}else if(i==6u){u.message.data.m_request.args[0]=1u;u.message.data.m_request.args[1]=128u;u.message.data.m_request.args[2]=255u;}else if(i!=0u)u.message.data.m_request.args[0]=1u;
CHECK(owner.uart(owner.context,&u,&d)==NET_OK);CHECK(request.type==(DomainIf_MobileRequestType_t)i);CHECK(request.meta.quality==DATA_QUALITY_NO_DATA&&request.meta.ageMs==UINT32_MAX);if(i==1u)CHECK(request.payload.targetTemperature==-100);if(i==6u)CHECK(request.payload.color.green==128u&&request.payload.color.blue==255u);}
return 0;}
static int command_lifetime(void){DomainIf_BcmDoorCommand_t command=door_command();NetWireBcmDoorCommandRaw decoded;NetUpperTxSnapshot snapshot;
CHECK(ports.txBcmDoorCommand(&command)==DOMAIN_IF_OK);command.target=DOOR_TARGET_LOCK;CHECK(net_upper_step(&service,0u)==NET_OK);
CHECK(net_wire_can_decode_bcm_door_command(NET_WIRE_CANDIDATE_ONLY,&packet.can,&decoded)==NET_WIRE_OK);CHECK(decoded.command_id==77u&&decoded.door_lock_target==1u&&decoded.target_boot_id==201u);
CHECK(net_upper_tx_snapshot(&service,key,&snapshot)==NET_OK&&snapshot.submitted&&!snapshot.admission_known&&!snapshot.reference_released);CHECK(net_domain_c2_unregister(&adapter)==NET_BUSY);return 0;}
static int failure_case(bool cancel,bool reject_sink){DomainIf_BcmDoorCommand_t command=door_command();NetUpperTxSnapshot snapshot;CHECK(ports.txBcmDoorCommand(&command)==DOMAIN_IF_OK);key=adapter.last_tx_key;
if(cancel)CHECK(net_upper_cancel(&service,key)==NET_OK);else{now=1100u;(void)net_upper_step(&service,0u);}
if(reject_sink){fail_return=DOMAIN_IF_QUEUE_FULL;}(void)net_domain_c2_step(&adapter);CHECK(failures==1u);(void)net_domain_c2_step(&adapter);CHECK(failures==1u);
if(reject_sink)CHECK(net_upper_tx_snapshot(&service,key,&snapshot)==NET_OK);else CHECK(net_upper_tx_snapshot(&service,key,&snapshot)==NET_STALE);return 0;}
static int result_mapping(void){NetUpperCanMessage m;NetUpperDelivery d=metadata();NetUpperOwner owner=net_domain_c2_owner(&adapter);memset(&m,0,sizeof(m));m.id=0x140u;m.data.bcm_result.command_domain_boot=301u;m.data.bcm_result.command_id=77u;m.data.bcm_result.command_kind=1u;m.data.bcm_result.result=2u;m.data.bcm_result.result_reason=9u;
CHECK(owner.can(owner.context,&m,&d)==NET_OK);CHECK(result.reason==RESULT_REASON_ALREADY_AT_TARGET&&result.confirmation==RESULT_CONFIRMATION_UNCONFIRMED&&!result.hasDoorEvidence);return 0;}
static int query_binding(void){NetUpperCanMessage m=rear_message();NetUpperEvent event;NetUpperQueryOwner owner=net_domain_c2_query_owner(&adapter);NetWireMQueryRaw request_q={0};NetUpperQueryOutput output,before;size_t count=77u;
memset(&event,0,sizeof(event));event.kind=NET_UPPER_CAN_RX;event.link=NET_WIRE_CAN;event.capture=metadata().capture;CHECK(net_upper_can_encode(NET_WIRE_CANDIDATE_ONLY,&m,&event.can)==NET_WIRE_OK);CHECK(net_upper_capture(&service,&event)==NET_OK);CHECK(net_upper_step(&service,1u)==NET_OK);
service.context_seen=true;service.context_port_epoch=4u;service.context.device_context_id=7u;service.context.session_id=11u;
request_q.device_context_id=7u;request_q.session_id=11u;request_q.query_id=3u;memset(&output,0x5a,sizeof(output));
CHECK(owner.getters[NET_QUERY_CIS_REAR](owner.context,&request_q,0u,1000u,&output,1u,&count)==NET_OK&&count==1u);CHECK(output.message.data.m_cis_rear.rear_distance_deci_cm==123u&&output.message.data.m_cis_rear.original_can_boot_id==401u);
before=output;count=77u;confirms=false;CHECK(owner.getters[NET_QUERY_CIS_REAR](owner.context,&request_q,0u,1000u,&output,1u,&count)==NET_HOLD&&count==77u&&memcmp(&output,&before,sizeof(output))==0);
confirms=true;rear.rearDistanceMm=999u;CHECK(owner.getters[NET_QUERY_CIS_REAR](owner.context,&request_q,0u,1000u,&output,1u,&count)==NET_HOLD);return 0;}
static int missing_getters(void){NetUpperQueryOwner owner=net_domain_c2_query_owner(&adapter);NetWireMQueryRaw r={0};NetUpperQueryOutput out,before;size_t n,i;memset(&out,0xa5,sizeof(out));before=out;
for(i=0u;i<NET_UPPER_QUERY_GETTERS;++i){CHECK(owner.getters[i]!=NULL);if(i==NET_QUERY_CIS_REAR)continue;n=999u;CHECK(owner.getters[i](owner.context,&r,1u,1000u,&out,1u,&n)==NET_HOLD);CHECK(n==999u&&memcmp(&out,&before,sizeof(out))==0);}return 0;}
static int route_slots(void){
CHECK(ports.txBcmDoorCommand!=NULL);CHECK(ports.txBcmDoorCommand(NULL)==DOMAIN_IF_INVALID_ARGUMENT);
CHECK(ports.txBcmClimateCommand!=NULL);CHECK(ports.txBcmClimateCommand(NULL)==DOMAIN_IF_INVALID_ARGUMENT);
CHECK(ports.txBcmInteriorLightCommand!=NULL);CHECK(ports.txBcmInteriorLightCommand(NULL)==DOMAIN_IF_INVALID_ARGUMENT);
CHECK(ports.txCisPowerPermission!=NULL);CHECK(ports.txCisPowerPermission(NULL)==DOMAIN_IF_INVALID_ARGUMENT);
CHECK(ports.txWindowCommand!=NULL);CHECK(ports.txWindowCommand(NULL)==DOMAIN_IF_INVALID_ARGUMENT);
CHECK(ports.txWindowOperationPermission!=NULL);CHECK(ports.txWindowOperationPermission(NULL)==DOMAIN_IF_INVALID_ARGUMENT);
CHECK(ports.txWindowDomainAlive!=NULL);CHECK(ports.txWindowDomainAlive(NULL)==DOMAIN_IF_INVALID_ARGUMENT);
CHECK(ports.txVssOneShotEvent!=NULL);CHECK(ports.txVssOneShotEvent(NULL)==DOMAIN_IF_INVALID_ARGUMENT);
CHECK(ports.txVssWarningState!=NULL);CHECK(ports.txVssWarningState(NULL)==DOMAIN_IF_INVALID_ARGUMENT);
CHECK(ports.txMobileRequestResult!=NULL);CHECK(ports.txMobileRequestResult(NULL)==DOMAIN_IF_INVALID_ARGUMENT);
CHECK(ports.txMobileBcmDoorState!=NULL);CHECK(ports.txMobileBcmDoorState(NULL)==DOMAIN_IF_INVALID_ARGUMENT);
CHECK(ports.txMobileBcmClimateState!=NULL);CHECK(ports.txMobileBcmClimateState(NULL)==DOMAIN_IF_INVALID_ARGUMENT);
CHECK(ports.txMobileBcmInteriorLightState!=NULL);CHECK(ports.txMobileBcmInteriorLightState(NULL)==DOMAIN_IF_INVALID_ARGUMENT);
CHECK(ports.txMobileBcmStatus!=NULL);CHECK(ports.txMobileBcmStatus(NULL)==DOMAIN_IF_INVALID_ARGUMENT);
CHECK(ports.txMobileCisEnvironment!=NULL);CHECK(ports.txMobileCisEnvironment(NULL)==DOMAIN_IF_INVALID_ARGUMENT);
CHECK(ports.txMobileCisOccupant!=NULL);CHECK(ports.txMobileCisOccupant(NULL)==DOMAIN_IF_INVALID_ARGUMENT);
CHECK(ports.txMobileCisRear!=NULL);CHECK(ports.txMobileCisRear(NULL)==DOMAIN_IF_INVALID_ARGUMENT);
CHECK(ports.txMobileCisStatus!=NULL);CHECK(ports.txMobileCisStatus(NULL)==DOMAIN_IF_INVALID_ARGUMENT);
CHECK(ports.txMobileVssStatus!=NULL);CHECK(ports.txMobileVssStatus(NULL)==DOMAIN_IF_INVALID_ARGUMENT);
CHECK(ports.txMobileWindowState!=NULL);CHECK(ports.txMobileWindowState(NULL)==DOMAIN_IF_INVALID_ARGUMENT);
CHECK(ports.txMobileWindowFault!=NULL);CHECK(ports.txMobileWindowFault(NULL)==DOMAIN_IF_INVALID_ARGUMENT);
CHECK(ports.txMobileWarning!=NULL);CHECK(ports.txMobileWarning(NULL)==DOMAIN_IF_INVALID_ARGUMENT);
CHECK(ports.txMobileFunctionAvailability!=NULL);CHECK(ports.txMobileFunctionAvailability(NULL)==DOMAIN_IF_INVALID_ARGUMENT);
CHECK(ports.txMobileDigitalKeyState!=NULL);CHECK(ports.txMobileDigitalKeyState(NULL)==DOMAIN_IF_INVALID_ARGUMENT);
CHECK(ports.txMobileDigitalKeyResult!=NULL);CHECK(ports.txMobileDigitalKeyResult(NULL)==DOMAIN_IF_INVALID_ARGUMENT);
CHECK(ports.txMobileUserSettings!=NULL);CHECK(ports.txMobileUserSettings(NULL)==DOMAIN_IF_INVALID_ARGUMENT);
return 0;}
static int window_and_warning(void){
    NetUpperCanMessage raw;NetUpperDelivery delivery=metadata();NetDomainC2WindowView view,before;
    DomainIf_VssWarningState_t warning;NetWireVssWarningStateRaw decoded;
    memset(&raw,0,sizeof(raw));raw.id=0x092u;raw.data.window_result.boot_id=501u;raw.data.window_result.tx_sequence=1u;raw.data.window_result.request_domain_boot=301u;raw.data.window_result.request_sequence=77u;
    memset(&view,0x5a,sizeof(view));before=view;
    CHECK(net_domain_c2_window_view(&adapter,&raw,&delivery,&view)==NET_HOLD&&memcmp(&before,&view,sizeof(view))==0);
    adapter.config.window_transport_enabled=true;delivery.observation_only=true;
    CHECK(net_domain_c2_window_view(&adapter,&raw,&delivery,&view)==NET_OK);
    CHECK(view.delivery.observation_only&&view.value.result.commandId==77u&&view.value.result.meta.quality==DATA_QUALITY_NO_DATA&&view.value.result.meta.ageMs==UINT32_MAX);
    CHECK(calls==0u);
    memset(&warning,0,sizeof(warning));warning.antiPinchEvidence.availability=FUNCTION_AVAILABILITY_UNSUPPORTED;
    warning.occupantHazardState=OCCUPANT_HAZARD_UNCONFIRMED;warning.occupantEvidence.quality=DATA_QUALITY_OK;warning.occupantEvidence.source.generation=501u;warning.occupantEvidence.source.sequence=19u;
    warning.rearEvidence.quality=DATA_QUALITY_NO_DATA;warning.rearEvidence.reason=QUALITY_REASON_NO_DATA;warning.rearEvidence.availability=FUNCTION_AVAILABILITY_UNAVAILABLE;warning.rearEvidence.source.ageMs=UINT16_MAX;
    warning.rearActivationMeta.updateSequence=1u;adapter.config.transmit_policy.maximum_age_ms=66535u;
    CHECK(ports.txVssWarningState(&warning)==DOMAIN_IF_OK);CHECK(net_upper_step(&service,0u)==NET_OK);
    CHECK(net_wire_can_decode_vss_warning_state(NET_WIRE_CANDIDATE_ONLY,&packet.can,&decoded)==NET_WIRE_OK);
    CHECK(decoded.antipinch_quality==3u&&decoded.antipinch_original_age_ms==UINT16_MAX);
    CHECK(decoded.occupant_hazard_state==0u&&decoded.occupant_hazard_quality==2u&&decoded.occupant_hazard_reason==9u&&decoded.occupant_hazard_availability==2u&&decoded.occupant_hazard_source_sequence==19u);
    return 0;
}
int main(int argc,char **argv){int result_code;if(argc!=2||start()!=0)return 2;
if(strcmp(argv[1],"units")==0)result_code=units();else if(strcmp(argv[1],"observer_failure")==0)result_code=observer_failure();else if(strcmp(argv[1],"request_tags")==0)result_code=request_tags();else if(strcmp(argv[1],"command_lifetime")==0)result_code=command_lifetime();else if(strcmp(argv[1],"expiry")==0)result_code=failure_case(false,false);else if(strcmp(argv[1],"context_cancel")==0)result_code=failure_case(true,false);else if(strcmp(argv[1],"failed_sink")==0)result_code=failure_case(true,true);else if(strcmp(argv[1],"result_mapping")==0)result_code=result_mapping();else if(strcmp(argv[1],"query_binding")==0)result_code=query_binding();else if(strcmp(argv[1],"missing_getters")==0)result_code=missing_getters();else if(strcmp(argv[1],"route_slots")==0)result_code=route_slots();else if(strcmp(argv[1],"window_and_warning")==0)result_code=window_and_warning();else return 2;
if(result_code==0){printf("C2 %s PASS\n",argv[1]);}return result_code;}
