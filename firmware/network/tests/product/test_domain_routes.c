/* Reuse the deterministic Host Port/external Owner harness. Every transmitted
 * value still traverses the actual product service, codec and TX state machine. */
#define main domain_component_cases_main
#include "test_domain_c2.c"
#undef main
typedef struct {
    DoorState_t door;ClimateState_t climate;InteriorLightState_t light;
    DomainIf_BcmStatusReport_t bcm;CisCabinEnvironmentState_t environment;
    CisOccupantState_t occupant;CisRearState_t rear;DomainIf_CisStatusReport_t cis;
    DomainIf_VssStateReport_t vss;
} RouteOwners;
static RouteOwners owners;
static NetUpperOwner original_owner;
static NetUpperDelivery active_receipt,committed[9];
static unsigned active_index;
static const uint16_t ids[9]={0x200u,0x210u,0x220u,0x230u,0x300u,0x310u,0x0a0u,0x320u,0x340u};
#define STORE(name,type,member) static DomainIf_Status_t rt_##name(const type *v){owners.member=*v;committed[active_index]=active_receipt;return DOMAIN_IF_OK;}
STORE(door,DoorState_t,door)
STORE(climate,ClimateState_t,climate)
STORE(light,InteriorLightState_t,light)
STORE(bcm,DomainIf_BcmStatusReport_t,bcm)
STORE(environment,CisCabinEnvironmentState_t,environment)
STORE(occupant,CisOccupantState_t,occupant)
STORE(rear,CisRearState_t,rear)
STORE(cis,DomainIf_CisStatusReport_t,cis)
STORE(vss,DomainIf_VssStateReport_t,vss)
#undef STORE
static NetStatus rt_receive(void *context,const NetUpperCanMessage *m,const NetUpperDelivery *d){unsigned i;for(i=0u;i<9u;++i)if(ids[i]==m->id){active_index=i;active_receipt=*d;break;}return original_owner.can(context,m,d);}
static bool rt_confirm(void *context,uint16_t id,uint64_t receipt,uint32_t boot,uint32_t seq){unsigned i;(void)context;for(i=0u;i<9u;++i)if(ids[i]==id)return committed[i].receipt_id==receipt&&committed[i].source_boot==boot&&committed[i].source_sequence==seq;return false;}
static NetStatus rt_quality(void *c,uint16_t id,const NetUpperCanMessage *m,DataQuality_t *q){(void)c;(void)id;(void)m;*q=DATA_QUALITY_OK;return NET_OK;}
static NetStatus rt_permission(void *c,const DomainIf_CisPowerPermission_t *p,NetWireDomainPermissionRaw *r){(void)c;memset(r,0,sizeof(*r));r->vehicle_power_permission=p->vehiclePowerAllowed?1u:0u;r->update_sequence=p->meta.updateSequence;r->source_age_ms=(uint16_t)p->meta.ageMs;return NET_OK;}
static DomainIf_Status_t rt_connection(DeviceContextId_t d,const DigitalKeyConnectionState_t *v){(void)d;(void)v;return DOMAIN_IF_OK;}
static DomainIf_Status_t rt_proximity(DeviceContextId_t d,const ProximityInput_t *v){(void)d;(void)v;return DOMAIN_IF_OK;}
static int inject_context(void){NetUpperUartMessage m={0};NetWireUartFrame frame;size_t i;NetUpperEvent e={0};m.link=NET_WIRE_UART_M;m.type=1u;m.sequence=1u;m.data.m_context.device_context_id=7u;m.data.m_context.session_id=11u;m.data.m_context.device_registration_state=1u;m.data.m_context.bt_connection_state=1u;m.data.m_context.app_active_state=1u;m.data.m_context.proximity_validity=1u;
CHECK(net_upper_uart_encode(NET_WIRE_CANDIDATE_ONLY,&m,&frame)==NET_WIRE_OK);e.kind=NET_UPPER_UART_BYTE;e.link=NET_WIRE_UART_M;e.capture=metadata().capture;
for(i=0u;i<frame.length;++i){e.byte=frame.bytes[i];CHECK(net_upper_capture(&service,&e)==NET_OK);(void)net_upper_step(&service,1u);}CHECK(service.context_seen);return 0;}
static int inject(unsigned index){NetUpperCanMessage m={0};NetUpperEvent e={0};m.id=ids[index];
#define BASE(field) m.data.field.boot_id=401u;m.data.field.tx_sequence=1u
switch(m.id){case 0x200u:BASE(bcm_door_state);m.data.bcm_door_state.update_sequence=1u;break;
case 0x210u:BASE(bcm_climate_state);m.data.bcm_climate_state.update_sequence=1u;break;
case 0x220u:BASE(bcm_light_state);m.data.bcm_light_state.update_sequence=1u;m.data.bcm_light_state.red=17u;m.data.bcm_light_state.blue=255u;break;
case 0x230u:BASE(bcm_status);m.data.bcm_status.update_sequence=1u;break;
case 0x300u:BASE(cis_environment);m.data.cis_environment.temperature_validity=1u;m.data.cis_environment.humidity_validity=1u;m.data.cis_environment.illuminance_validity=1u;m.data.cis_environment.temperature_sequence=1u;m.data.cis_environment.humidity_sequence=1u;m.data.cis_environment.illuminance_sequence=1u;m.data.cis_environment.illuminance_lux=123456u;break;
case 0x310u:BASE(cis_occupant);m.data.cis_occupant.pi_boot_id=6401u;m.data.cis_occupant.vision_sequence=1u;m.data.cis_occupant.presence_validity=1u;m.data.cis_occupant.count_validity=1u;break;
case 0x0a0u:BASE(cis_rear);m.data.cis_rear.validity=1u;m.data.cis_rear.rear_distance_deci_cm=123u;m.data.cis_rear.update_sequence=1u;break;
case 0x320u:BASE(cis_status);m.data.cis_status.update_sequence=1u;break;
case 0x340u:BASE(vss_status);m.data.vss_status.update_sequence=1u;m.data.vss_status.last_fault_quality=3u;break;default:CHECK(false);}
#undef BASE
printf("RX_FIXTURE id=%u\n",m.id);CHECK(net_upper_can_encode(NET_WIRE_CANDIDATE_ONLY,&m,&e.can)==NET_WIRE_OK);e.kind=NET_UPPER_CAN_RX;e.link=NET_WIRE_CAN;e.capture=metadata().capture;CHECK(net_upper_capture(&service,&e)==NET_OK);CHECK(net_upper_step(&service,1u)==NET_OK);CHECK(committed[index].receipt_id!=0u);return 0;}
static int finish(uint16_t expected,NetWireLink link){NetUpperEvent e={0};NetUpperTxSnapshot snap;NetUpperUartMessage uart;NetUpperCanMessage can;
CHECK(net_upper_step(&service,0u)==NET_OK);CHECK(packet.link==link);CHECK(net_upper_tx_snapshot(&service,key,&snap)==NET_OK&&!snap.admission_known&&!snap.reference_released);
if(link==NET_WIRE_CAN){CHECK(net_upper_can_decode(NET_WIRE_CANDIDATE_ONLY,&packet.can,&can)==NET_WIRE_OK);CHECK(can.id==expected);}else{CHECK(net_upper_uart_decode(NET_WIRE_CANDIDATE_ONLY,link,NET_WIRE_DOWNLINK,&packet.uart,&uart)==NET_WIRE_OK);CHECK(uart.type==expected);}
e.kind=NET_UPPER_TX_EVENT;e.link=link;e.capture=metadata().capture;e.tx.key=key;e.tx.admission_observed=true;e.tx.accepted=true;CHECK(net_upper_capture(&service,&e)==NET_OK);(void)net_upper_step(&service,1u);
CHECK(net_upper_tx_snapshot(&service,key,&snap)==NET_OK&&snap.accepted&&snap.outcome==NET_TX_UNKNOWN&&!snap.reference_released);
e.tx.admission_observed=false;e.tx.outcome=NET_TX_SUCCEEDED;CHECK(net_upper_capture(&service,&e)==NET_OK);(void)net_upper_step(&service,1u);
CHECK(net_upper_tx_snapshot(&service,key,&snap)==NET_OK&&snap.outcome==NET_TX_SUCCEEDED&&!snap.reference_released);
e.tx.outcome=NET_TX_UNKNOWN;e.tx.release_observed=true;CHECK(net_upper_capture(&service,&e)==NET_OK);(void)net_upper_step(&service,1u);
(void)net_domain_c2_step(&adapter);if(net_upper_tx_snapshot(&service,key,&snap)==NET_OK)CHECK(net_upper_retire(&service,key)==NET_OK);
printf("VALID_ROUTE link=%u schema=%u admission/final/release PASS\n",(unsigned)link,expected);return 0;}
static DomainIf_MobileRequestResult_t mobile_result;
static DomainIf_UserSettingsState_t settings;
static DomainIf_FunctionAvailabilitySet_t availability;
static DigitalKeyState_t digital;
static DomainIf_DigitalKeyResult_t digital_result;
static DomainIf_WarningInfo_t mobile_warning;
#define GET(name,type,expression) static DomainIf_Status_t rt_get_##name(uint32_t at,type *out){(void)at;*out=(expression);return DOMAIN_IF_OK;}
GET(door,DoorState_t,owners.door)
GET(climate,ClimateState_t,owners.climate)
GET(light,InteriorLightState_t,owners.light)
GET(bcm,DomainIf_BcmStatusReport_t,owners.bcm)
GET(environment,CisCabinEnvironmentState_t,owners.environment)
GET(occupant,CisOccupantState_t,owners.occupant)
GET(rear,CisRearState_t,owners.rear)
GET(cis,DomainIf_CisStatusReport_t,owners.cis)
GET(vss,DomainIf_VssStateReport_t,owners.vss)
GET(settings,DomainIf_UserSettingsState_t,settings)
GET(availability,DomainIf_FunctionAvailabilitySet_t,availability)
GET(digital,DigitalKeyState_t,digital)
#undef GET
static DomainIf_Status_t rt_get_warning(DomainWarningType_t t,uint32_t at,DomainIf_WarningInfo_t *out){(void)t;(void)at;*out=mobile_warning;return DOMAIN_IF_OK;}
static DomainIf_Status_t rt_get_result(DeviceContextId_t d,const RequestContext_t *r,uint32_t at,DomainIf_MobileRequestResult_t *out,bool *has){(void)d;(void)r;(void)at;*out=mobile_result;*has=true;return DOMAIN_IF_OK;}
static DomainIf_Status_t rt_get_recent(DeviceContextId_t d,uint32_t at,DomainIf_MobileRequestResult_t *out,size_t cap,size_t *count){(void)d;(void)at;if(cap<1u)return DOMAIN_IF_QUEUE_FULL;out[0]=mobile_result;*count=1u;return DOMAIN_IF_OK;}
static DomainIf_Status_t rt_get_digital_result(DeviceContextId_t d,uint32_t at,DomainIf_DigitalKeyResult_t *out,bool *has){(void)d;(void)at;*out=digital_result;*has=true;return DOMAIN_IF_OK;}
static NetStatus nested_health_status;
static unsigned health_calls;
static DomainIf_Status_t rt_health(NetworkLinkId_t link,uint32_t at,NetworkHealthSnapshot_t *out)
{
    NetworkHealthSnapshot_t nested;
    ++health_calls;
    nested_health_status=net_domain_c2_health(&adapter,link,at,&nested);
    memset(out,0,sizeof(*out));out->link=link;out->transportState=NETWORK_TRANSPORT_AVAILABLE;
    return DOMAIN_IF_OK;
}
static NetStatus rt_driver_rejection(void *context,const NetUpperTxSnapshot *s,NetworkLocalFailureReason_t *reason)
{
    (void)context;
    if(!s->submitted||!s->admission_known||s->accepted||!s->reference_released)return NET_HOLD;
    *reason=NETWORK_LOCAL_FAILURE_DRIVER_SUBMISSION_FINAL;return NET_OK;
}
static int lifecycle_guards(void)
{
    NetDomainC2 fresh={0};NetDomainC2Api api=adapter.api;NetDomainC2Config cfg=adapter.config;
    NetworkHealthSnapshot_t out,before;NetUpperEvent e={0};NetUpperTxSnapshot snap;
    DomainIf_BcmDoorCommand_t command=door_command();unsigned failure_before=failures;
    CHECK(net_domain_c2_init(&adapter,&service,&query,&api,&cfg)==NET_BUSY);
    CHECK(net_domain_c2_init(&fresh,&service,(NetUpperQuery *)&service,&api,&cfg)==NET_INVALID_ARGUMENT);
    adapter.api.GetNetworkHealthSnapshot=rt_health;memset(&out,0x5a,sizeof(out));before=out;
    adapter.in_call=true;CHECK(net_domain_c2_health(&adapter,NETWORK_LINK_CAN,1000u,&out)==NET_BUSY);adapter.in_call=false;
    service.in_call=true;CHECK(net_domain_c2_health(&adapter,NETWORK_LINK_CAN,1000u,&out)==NET_BUSY);service.in_call=false;
    CHECK(health_calls==0u&&memcmp(&before,&out,sizeof(out))==0);
    CHECK(net_domain_c2_health(&adapter,NETWORK_LINK_CAN,1000u,(NetworkHealthSnapshot_t *)&adapter)==NET_INVALID_ARGUMENT);
    CHECK(net_domain_c2_health(&adapter,NETWORK_LINK_CAN,1000u,(NetworkHealthSnapshot_t *)&service)==NET_INVALID_ARGUMENT);
    CHECK(net_domain_c2_health(&adapter,NETWORK_LINK_CAN,1000u,&out)==NET_OK);
    CHECK(health_calls==1u&&nested_health_status==NET_BUSY&&out.transportState==NETWORK_TRANSPORT_AVAILABLE);
    adapter.config.local_failure_reason=rt_driver_rejection;
    CHECK(ports.txBcmDoorCommand(&command)==DOMAIN_IF_OK);CHECK(net_upper_step(&service,0u)==NET_OK);
    e.kind=NET_UPPER_TX_EVENT;e.link=NET_WIRE_CAN;e.capture=metadata().capture;e.tx.key=key;e.tx.admission_observed=true;e.tx.accepted=false;
    CHECK(net_upper_capture(&service,&e)==NET_OK);(void)net_upper_step(&service,1u);
    CHECK(net_upper_tx_snapshot(&service,key,&snap)==NET_OK&&snap.submitted&&!snap.accepted&&!snap.reference_released);
    CHECK(net_domain_c2_step(&adapter)==NET_HOLD&&failures==failure_before);
    e.tx.admission_observed=false;e.tx.release_observed=true;
    CHECK(net_upper_capture(&service,&e)==NET_OK);(void)net_upper_step(&service,1u);
    CHECK(net_domain_c2_step(&adapter)==NET_OK&&failures==failure_before+1u);
    CHECK(net_upper_tx_snapshot(&service,key,&snap)==NET_STALE);
    (void)net_domain_c2_step(&adapter);CHECK(failures==failure_before+1u);
    puts("LIFECYCLE: live init, alias, nested health, Driver rejection then release PASS");return 0;
}
int main(void)
{
    unsigned i;DomainIf_BcmDoorCommand_t dc=door_command();DomainIf_BcmClimateCommand_t cc={0};DomainIf_BcmInteriorLightCommand_t lc={0};DomainIf_CisPowerPermission_t power={0};DomainIf_VssOneShotEvent_t event={0};DomainIf_VssWarningState_t warning={0};
    DomainIf_WindowCommand_t wc={0};DomainIf_WindowOperationPermission_t wp={0};DomainIf_WindowDomainAlive_t wa={0};WindowState_t ws={0};DomainIf_WindowFaultState_t wf={0};
    NetUpperQueryOwner getter;NetWireMQueryRaw query_request={0};NetUpperQueryOutput output;size_t count;
    CHECK(start()==0);adapter.query=NULL;adapter.config.confirm_snapshot=rt_confirm;adapter.config.status_quality=rt_quality;adapter.config.permission_bundle=rt_permission;adapter.config.transmit_policy.maximum_age_ms=66535u;
    adapter.api.RxBcmDoorState=rt_door;adapter.api.RxBcmClimateState=rt_climate;adapter.api.RxBcmInteriorLightState=rt_light;adapter.api.RxBcmStatus=rt_bcm;adapter.api.RxCisCabinEnvironment=rt_environment;adapter.api.RxCisOccupant=rt_occupant;adapter.api.RxCisRear=rt_rear;adapter.api.RxCisStatus=rt_cis;adapter.api.RxVssState=rt_vss;adapter.api.RxDigitalKeyConnection=rt_connection;adapter.api.RxProximity=rt_proximity;
    original_owner=net_domain_c2_owner(&adapter);service.config.owner=original_owner;service.config.owner.can=rt_receive;
    CHECK(inject_context()==0);for(i=0u;i<9u;++i)CHECK(inject(i)==0);
    CHECK(owners.rear.rearDistanceMm==123u&&owners.environment.cabinIlluminance==123456u&&owners.light.color.blue==255u);
#define SEND(call,id,link) do{CHECK((call)==DOMAIN_IF_OK);CHECK(finish(id,link)==0);}while(0)
    SEND(ports.txBcmDoorCommand(&dc),0x100u,NET_WIRE_CAN);
    cc.context=dc.context;cc.fanTarget=FAN_LEVEL_HIGH;cc.thermalDirection=THERMAL_DIRECTION_COOL;cc.thermalTargetLevelPercent=37u;SEND(ports.txBcmClimateCommand(&cc),0x110u,NET_WIRE_CAN);
    lc.context=dc.context;lc.type=INTERIOR_LIGHT_TYPE_WARNING;lc.levelPercent=73u;lc.color.red=128u;lc.color.blue=255u;SEND(ports.txBcmInteriorLightCommand(&lc),0x120u,NET_WIRE_CAN);
    power.meta.updateSequence=1u;SEND(ports.txCisPowerPermission(&power),0x080u,NET_WIRE_CAN);
    event.event=VSS_EVENT_DOOR_LOCK_ERROR;event.occurrenceId=88u;SEND(ports.txVssOneShotEvent(&event),0x160u,NET_WIRE_CAN);
    warning.antiPinchEvidence.availability=FUNCTION_AVAILABILITY_UNSUPPORTED;warning.occupantEvidence.quality=DATA_QUALITY_NO_DATA;warning.occupantEvidence.reason=QUALITY_REASON_NO_DATA;warning.occupantEvidence.availability=FUNCTION_AVAILABILITY_UNAVAILABLE;warning.occupantEvidence.source.ageMs=UINT16_MAX;warning.rearEvidence.source.generation=401u;warning.rearEvidence.source.sequence=1u;warning.rearActivationMeta.updateSequence=1u;SEND(ports.txVssWarningState(&warning),0x0c0u,NET_WIRE_CAN);
    mobile_result.deviceContextId=7u;mobile_result.resultInfo.context.sessionId=11u;mobile_result.resultInfo.context.requestId=101u;mobile_result.resultInfo.result=REQUEST_RESULT_REJECTED;mobile_result.hasVehicleResult=true;mobile_result.target=MOBILE_RESULT_TARGET_DOMAIN_SETTING;mobile_result.origin=COMMAND_ORIGIN_MOBILE_REQUEST;SEND(ports.txMobileRequestResult(&mobile_result),0x20u,NET_WIRE_UART_M);
    SEND(ports.txMobileBcmDoorState(&owners.door),0x30u,NET_WIRE_UART_M);SEND(ports.txMobileBcmClimateState(&owners.climate),0x31u,NET_WIRE_UART_M);SEND(ports.txMobileBcmInteriorLightState(&owners.light),0x32u,NET_WIRE_UART_M);SEND(ports.txMobileBcmStatus(&owners.bcm),0x33u,NET_WIRE_UART_M);SEND(ports.txMobileCisEnvironment(&owners.environment),0x34u,NET_WIRE_UART_M);SEND(ports.txMobileCisOccupant(&owners.occupant),0x35u,NET_WIRE_UART_M);SEND(ports.txMobileCisRear(&owners.rear),0x36u,NET_WIRE_UART_M);SEND(ports.txMobileCisStatus(&owners.cis),0x37u,NET_WIRE_UART_M);SEND(ports.txMobileVssStatus(&owners.vss),0x3au,NET_WIRE_UART_M);
    mobile_warning.type=DOMAIN_WARNING_REAR_OBSTACLE;mobile_warning.occurrenceId=3u;mobile_warning.source.generation=401u;mobile_warning.source.sequence=1u;SEND(ports.txMobileWarning(&mobile_warning),0x21u,NET_WIRE_UART_M);
    for(i=0u;i<DOMAIN_FUNCTION_COUNT;++i){availability.items[i].functionId=(DomainFunctionId_t)i;}availability.meta.updateSequence=1u;SEND(ports.txMobileFunctionAvailability(&availability),0x22u,NET_WIRE_UART_M);
    digital.meta.updateSequence=1u;SEND(ports.txMobileDigitalKeyState(&digital),0x23u,NET_WIRE_UART_M);
    digital_result.hasVehicleResult=true;digital_result.result=REQUEST_RESULT_REJECTED;digital_result.origin=UNLOCK_ORIGIN_PROXIMITY_AUTO;digital_result.occurrenceId=9u;SEND(ports.txMobileDigitalKeyResult(&digital_result),0x24u,NET_WIRE_UART_M);
    settings.hasTargetTemperature=true;settings.targetTemperatureCentiC=2345;settings.hasClimateAutoSetting=true;settings.hasUserFanLevel=true;settings.userFanLevel=FAN_LEVEL_MEDIUM;settings.hasInteriorLightEnableSetting=true;settings.hasNormalLightLevel=true;settings.hasNormalLightColor=true;settings.normalLightLevelPercent=49u;settings.normalLightColor.blue=255u;settings.meta.updateSequence=1u;SEND(ports.txMobileUserSettings(&settings),0x25u,NET_WIRE_UART_M);
    CHECK(ports.txWindowCommand(&wc)==DOMAIN_IF_NOT_AVAILABLE);CHECK(ports.txWindowOperationPermission(&wp)==DOMAIN_IF_NOT_AVAILABLE);CHECK(ports.txWindowDomainAlive(&wa)==DOMAIN_IF_NOT_AVAILABLE);CHECK(ports.txMobileWindowState(&ws)==DOMAIN_IF_NOT_AVAILABLE);CHECK(ports.txMobileWindowFault(&wf)==DOMAIN_IF_NOT_AVAILABLE);
    puts("TXPORT26: 21 valid encoded routes; 5 WINDOW default HOLD PASS");
    adapter.api.QueryGetBcmDoorState=rt_get_door;adapter.api.QueryGetBcmClimateState=rt_get_climate;adapter.api.QueryGetBcmInteriorLightState=rt_get_light;adapter.api.QueryGetBcmStatus=rt_get_bcm;adapter.api.QueryGetCisEnvironment=rt_get_environment;adapter.api.QueryGetCisOccupant=rt_get_occupant;adapter.api.QueryGetCisRear=rt_get_rear;adapter.api.QueryGetCisStatus=rt_get_cis;adapter.api.QueryGetVssStatus=rt_get_vss;adapter.api.QueryGetUserSettings=rt_get_settings;adapter.api.QueryGetFunctionAvailability=rt_get_availability;adapter.api.QueryGetDigitalKeyState=rt_get_digital;adapter.api.QueryGetCurrentWarning=rt_get_warning;adapter.api.QueryGetRequestResult=rt_get_result;adapter.api.QueryGetRecentRequestResults=rt_get_recent;adapter.api.QueryGetLastDigitalKeyResult=rt_get_digital_result;
    getter=net_domain_c2_query_owner(&adapter);query_request.device_context_id=7u;query_request.session_id=11u;query_request.query_id=10u;
    for(i=0u;i<NET_UPPER_QUERY_GETTERS;++i){NetWireUartFrame encoded;count=777u;if(i==NET_QUERY_WINDOW_STATE||i==NET_QUERY_WINDOW_FAULT){CHECK(getter.getters[i](getter.context,&query_request,1u,now,&output,1u,&count)==NET_HOLD&&count==777u);continue;}
        CHECK(getter.getters[i](getter.context,&query_request,1u,now,&output,1u,&count)==NET_OK&&count==1u);CHECK(net_upper_uart_encode(NET_WIRE_CANDIDATE_ONLY,&output.message,&encoded)==NET_WIRE_OK);CHECK(output.message.type==net_upper_query_schema((NetUpperQueryGetter)i));printf("VALID_GETTER %u schema=%u PASS\n",i,output.message.type);}
    puts("GETTER18: 16 typed successful codecs; 2 WINDOW HOLD PASS");CHECK(lifecycle_guards()==0);return 0;
}
