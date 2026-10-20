#include "virtual_domain_owner.h"
#include "virtual_ecu.h"
#include "network/net_domain_c2.h"
#include <stdio.h>
#include <string.h>
/* External DOMAIN feature Owner for Host evidence only. It owns decision,
 * request/result state and pending actions. It does not encode or route bytes. */
static NetDomainC2 adapter;
static NetUpperQuery query;
static DomainIf_TxPort_t tx;
static DigitalKeyConnectionState_t connection;
static CisRearState_t rear;
static uint64_t rear_at,result_at,command_at;
static NetUpperOwner network_owner;
static NetUpperDelivery active_delivery,rear_commit;
static NetStatus owner_can_bridge(void *context,const NetUpperCanMessage *m,const NetUpperDelivery *d){active_delivery=*d;return network_owner.can(context,m,d);}
static DomainIf_MobileRequest_t request;
static DomainIf_MobileRequestResult_t result;
static DomainIf_VssWarningState_t warning;
static bool warning_pending,command_pending,result_pending,has_result;
static unsigned query_mode;
static bool initialized(void){return true;}
static DomainIf_Status_t set_tx(const DomainIf_TxPort_t *port){if(port==NULL)memset(&tx,0,sizeof(tx));else tx=*port;return DOMAIN_IF_OK;}
static DomainIf_Status_t receive_connection(DeviceContextId_t device,const DigitalKeyConnectionState_t *v){connection=*v;printf("{\"event\":\"c2_context\",\"device\":%u,\"session\":%llu,\"app_age\":%u}\n",device,(unsigned long long)v->sessionId,v->appMeta.ageMs);return DOMAIN_IF_OK;}
static DomainIf_Status_t receive_proximity(DeviceContextId_t device,const ProximityInput_t *v){printf("{\"event\":\"c2_proximity\",\"device\":%u,\"state\":%u,\"age\":%u}\n",device,(unsigned)v->state,v->meta.ageMs);return DOMAIN_IF_OK;}
static DomainIf_Status_t receive_rear(const CisRearState_t *v){rear=*v;rear_commit=active_delivery;rear_at=virtual_now();memset(&warning,0,sizeof(warning));
warning.antiPinchEvidence.availability=FUNCTION_AVAILABILITY_UNSUPPORTED;
warning.occupantEvidence.quality=DATA_QUALITY_NO_DATA;warning.occupantEvidence.reason=QUALITY_REASON_NO_DATA;warning.occupantEvidence.availability=FUNCTION_AVAILABILITY_UNAVAILABLE;warning.occupantEvidence.source.ageMs=UINT16_MAX;
warning.rearObstacleState=(v->distanceQuality.meta.quality==DATA_QUALITY_OK&&v->rearDistanceMm<500u)?REAR_OBSTACLE_STATE_CAUTION:REAR_OBSTACLE_STATE_CLEAR;
warning.rearEvidence.quality=v->distanceQuality.meta.quality;warning.rearEvidence.reason=v->distanceQuality.reason;warning.rearEvidence.availability=FUNCTION_AVAILABILITY_AVAILABLE;warning.rearEvidence.source.generation=v->sourceGeneration;warning.rearEvidence.source.sequence=v->distanceQuality.meta.updateSequence;warning.rearEvidence.source.ageMs=v->distanceQuality.meta.ageMs;
warning.rearDetectionActivation=REAR_DETECTION_ACTIVE;warning.rearActivationMeta.quality=DATA_QUALITY_OK;warning.rearActivationMeta.updateSequence=v->distanceQuality.meta.updateSequence;warning_pending=true;
printf("{\"event\":\"c2_rear\",\"mm\":%u,\"quality\":%u,\"age\":%u,\"source\":%u}\n",v->rearDistanceMm,(unsigned)v->distanceQuality.meta.quality,v->distanceQuality.meta.ageMs,v->sourceGeneration);return DOMAIN_IF_OK;}
static DomainIf_Status_t receive_occupant(const CisOccupantState_t *v){printf("{\"event\":\"c2_occupant\",\"count\":%u,\"source\":%u,\"sequence\":%u,\"age\":%u}\n",v->occupantCount,v->sourceGeneration,v->presenceQuality.meta.updateSequence,v->presenceQuality.meta.ageMs);return DOMAIN_IF_OK;}
static DomainIf_Status_t receive_request(const DomainIf_MobileRequest_t *v){if(command_pending)return DOMAIN_IF_QUEUE_FULL;request=*v;command_at=virtual_now();command_pending=true;printf("{\"event\":\"c2_request\",\"request\":%u,\"session\":%llu,\"quality\":%u,\"source_age_unknown\":%s}\n",v->context.requestId,(unsigned long long)v->context.sessionId,(unsigned)v->meta.quality,v->meta.ageMs==UINT32_MAX?"true":"false");return DOMAIN_IF_OK;}
static DomainIf_Status_t receive_result(const DomainIf_BcmCommandResult_t *v){if(v->commandId!=request.context.requestId+1000u)return DOMAIN_IF_REJECTED;
memset(&result,0,sizeof(result));result.deviceContextId=request.deviceContextId;result.resultInfo.context=request.context;result.resultInfo.result=v->result;result.resultInfo.reason=v->reason;result.resultInfo.confirmation=v->confirmation;result.hasVehicleResult=true;result.target=MOBILE_RESULT_TARGET_BCM;result.commandId=v->commandId;result.origin=COMMAND_ORIGIN_MOBILE_REQUEST;result_at=virtual_now();result_pending=true;has_result=true;
printf("{\"event\":\"c2_result\",\"command\":%u,\"result\":%u,\"confirmation\":%u}\n",v->commandId,(unsigned)v->result,(unsigned)v->confirmation);return DOMAIN_IF_OK;}
static DomainIf_Status_t failure(const NetworkLocalFailure_t *v){printf("{\"event\":\"c2_local_failure\",\"kind\":%u,\"reason\":%u,\"generation\":%u,\"command\":%u}\n",(unsigned)v->itemKind,(unsigned)v->reason,v->identityGeneration,v->commandId);return DOMAIN_IF_OK;}
static DomainIf_Status_t passive(DeviceContextId_t device,SessionId_t session){return device==7u&&session==connection.sessionId?DOMAIN_IF_OK:DOMAIN_IF_REJECTED;}
static DomainIf_Status_t get_rear(uint32_t now,CisRearState_t *out){if(query_mode==2u)return DOMAIN_IF_NOT_AVAILABLE;*out=rear;out->distanceQuality.meta.ageMs+=(uint32_t)((uint64_t)now-rear_at);return DOMAIN_IF_OK;}
static DomainIf_Status_t get_recent(DeviceContextId_t device,uint32_t now,DomainIf_MobileRequestResult_t *out,size_t capacity,size_t *count){if((uint64_t)now<result_at||(uint64_t)now-result_at>UINT32_MAX)return DOMAIN_IF_NOT_AVAILABLE;if(query_mode==2u)return DOMAIN_IF_NOT_AVAILABLE;if(!has_result||query_mode==1u){*count=0u;return DOMAIN_IF_OK;}if(device!=result.deviceContextId||capacity<1u)return DOMAIN_IF_INVALID_ARGUMENT;out[0]=result;out[0].resultAgeMs=(uint32_t)((uint64_t)now-result_at);*count=1u;return DOMAIN_IF_OK;}
static NetStatus plan(void *context,const NetWireMQueryRaw *q,NetUpperQueryPlan *out){(void)context;memset(out,0,sizeof(*out));if(query_mode==1u)return NET_OK;out->count=q->query_scope==3u?2u:1u;out->selected[0].getter=(q->query_scope==1u||q->query_scope==3u)?NET_QUERY_RECENT_RESULTS:NET_QUERY_CIS_REAR;out->selected[0].capacity=(q->query_scope==1u||q->query_scope==3u)?5u:1u;if(out->count==2u){out->selected[1].getter=NET_QUERY_CIS_REAR;out->selected[1].capacity=1u;}return NET_OK;}
static bool confirm(void *context,uint16_t id,uint64_t receipt,uint32_t boot,uint32_t sequence){(void)context;return id==0x0a0u&&rear.sourceGeneration==boot&&rear_commit.source_boot==boot&&rear_commit.source_sequence==sequence&&rear_commit.receipt_id==receipt;}
static NetStatus status_quality(void *context,uint16_t id,const NetUpperCanMessage *m,DataQuality_t *out){(void)context;(void)id;(void)m;*out=DATA_QUALITY_NO_DATA;return NET_OK;}
static NetStatus permission_bundle(void *context,const DomainIf_CisPowerPermission_t *p,NetWireDomainPermissionRaw *out){(void)context;memset(out,0,sizeof(*out));out->vehicle_power_permission=p->vehiclePowerAllowed?1u:0u;
switch(p->quality){case DATA_QUALITY_OK:out->power_permission_quality=0u;break;case DATA_QUALITY_STALE:out->power_permission_quality=1u;break;case DATA_QUALITY_INVALID:out->power_permission_quality=2u;break;case DATA_QUALITY_NO_DATA:out->power_permission_quality=3u;break;default:return NET_HOLD;}
out->power_operation_state=0u;out->operation_allowed=0u;out->operation_quality=3u;out->update_sequence=p->meta.updateSequence;out->source_age_ms=(uint16_t)p->meta.ageMs;return NET_OK;}
NetStatus virtual_domain_init(NetUpper *service,NetUpperOwner *out){NetDomainC2Api api={0};NetDomainC2Config config={0};NetStatus s;
api.IsInitialized=initialized;api.SetTxPort=set_tx;api.RxDigitalKeyConnection=receive_connection;api.RxProximity=receive_proximity;api.RxCisRear=receive_rear;api.RxCisOccupant=receive_occupant;api.RxMobileRequest=receive_request;api.RxBcmCommandResult=receive_result;api.RxNetworkLocalFailure=failure;api.ValidatePassiveQueryContext=passive;api.QueryGetCisRear=get_rear;api.QueryGetRecentRequestResults=get_recent;
config.domain_boot=301u;config.bcm_boot=201u;config.first_can_sequence=1u;config.command_accept_limit_ms=500u;config.event_use_limit_ms=500u;config.transmit_policy.bound=true;config.transmit_policy.maximum_age_ms=66535u;config.transmit_policy.expires_at_equality=true;config.value_quality=net_domain_c2_value_quality_provisional;config.status_quality=status_quality;config.permission_bundle=permission_bundle;config.select_query_plan=plan;config.confirm_snapshot=confirm;config.failure_sink_ready=true;
s=net_domain_c2_init(&adapter,service,&query,&api,&config);if(s==NET_OK){network_owner=net_domain_c2_owner(&adapter);*out=network_owner;out->can=owner_can_bridge;}return s;}
NetStatus virtual_domain_start(void){NetUpperQueryOwner owner=net_domain_c2_query_owner(&adapter);NetStatus s=net_upper_query_init(&query,virtual_service(),301u,&owner);return s==NET_OK?net_domain_c2_register(&adapter):s;}
void virtual_domain_step(uint64_t now){DomainIf_Status_t status;(void)now;
if(warning_pending){status=tx.txVssWarningState(&warning);printf("{\"event\":\"c2_tx_warning\",\"status\":%u}\n",(unsigned)status);if(status==DOMAIN_IF_OK)warning_pending=false;}
if(command_pending){DomainIf_BcmDoorCommand_t c;memset(&c,0,sizeof(c));c.context.commandId=request.context.requestId+1000u;c.context.commandAgeMs=now>=command_at&&now-command_at<=UINT32_MAX?(uint32_t)(now-command_at):UINT32_MAX;c.context.decisionSequence=request.context.requestId;c.context.hasRequestContext=true;c.context.requestContext=request.context;c.context.requestDeviceContextId=request.deviceContextId;c.context.origin=COMMAND_ORIGIN_MOBILE_REQUEST;c.context.meta.quality=DATA_QUALITY_OK;c.target=request.payload.doorTarget;
status=tx.txBcmDoorCommand(&c);printf("{\"event\":\"c2_tx_command\",\"command\":%u,\"status\":%u}\n",c.context.commandId,(unsigned)status);if(status==DOMAIN_IF_OK)command_pending=false;}
if(result_pending){DomainIf_MobileRequestResult_t current=result;current.resultAgeMs=now>=result_at&&now-result_at<=UINT32_MAX?(uint32_t)(now-result_at):UINT32_MAX;status=tx.txMobileRequestResult(&current);printf("{\"event\":\"c2_tx_result\",\"status\":%u}\n",(unsigned)status);if(status==DOMAIN_IF_OK)result_pending=false;}
(void)net_upper_query_step(&query);(void)net_domain_c2_step(&adapter);}
NetStatus virtual_domain_command(const char *line){unsigned value,sequence;if(sscanf(line,"permission %u %u",&value,&sequence)==2){DomainIf_CisPowerPermission_t p;memset(&p,0,sizeof(p));if(value>1u)return NET_INVALID_ARGUMENT;p.vehiclePowerAllowed=value==1u;p.quality=DATA_QUALITY_OK;p.meta.quality=DATA_QUALITY_OK;p.meta.updateSequence=sequence;return net_domain_c2_from_status(tx.txCisPowerPermission(&p));}
if(strcmp(line,"query_mode normal")==0){query_mode=0u;return NET_OK;}if(strcmp(line,"query_mode empty")==0){query_mode=1u;return NET_OK;}if(strcmp(line,"query_mode error")==0){query_mode=2u;return NET_OK;}return NET_UNSUPPORTED;}
