#include "net_domain_private.h"
static NetStatus age16(uint32_t age,uint16_t *out){if(age>UINT16_MAX)return NET_HOLD;
*out=(uint16_t)age;
return NET_OK;
}
static NetStatus origin(CommandOrigin_t origin,uint8_t *out){switch(origin){
case COMMAND_ORIGIN_MOBILE_REQUEST:*out=1u;
return NET_OK;
case COMMAND_ORIGIN_DIGITAL_KEY:*out=2u;
return NET_OK;
case COMMAND_ORIGIN_CLIMATE_POLICY:case COMMAND_ORIGIN_INTERIOR_LIGHT_POLICY:case COMMAND_ORIGIN_AUTO_VENTILATION:case COMMAND_ORIGIN_SYSTEM:*out=5u;
return NET_OK;
default:return NET_HOLD;
}
}
static NetStatus reason(ResultReason_t reason,uint16_t *out){uint8_t value;
ND_TO(ResultReason_t,reason,value);
*out=value;
return NET_OK;
}
static NetStatus command(NetDomainC2 *a,const DomainIf_CommandContext_t *c,NetworkLocalFailure_t *id,uint16_t *age){
if(c->commandId==0u||a->config.bcm_boot==0u||a->config.command_accept_limit_ms==0u)return NET_HOLD;
ND_TRY(age16(c->commandAgeMs,age));
memset(id,0,sizeof(*id));
id->link=NETWORK_LINK_CAN;
id->itemKind=NETWORK_LOCAL_ITEM_COMMAND;
id->identityGeneration=a->config.domain_boot;
id->hasCommandId=true;
id->commandId=c->commandId;
id->hasRequestContext=c->hasRequestContext;
if(c->hasRequestContext){id->requestContext=c->requestContext;
id->requestDeviceContextId=c->requestDeviceContextId;
}
return NET_OK;
}
static NetStatus bcm_door(NetDomainC2 *a,const DomainIf_BcmDoorCommand_t *in){NetUpperCanMessage m;
NetworkLocalFailure_t id;
NetWireBcmDoorCommandRaw *r=&m.data.bcm_door_command;
if(in==NULL)return NET_INVALID_ARGUMENT;
memset(&m,0,sizeof(m));
m.id=0x100u;
ND_TRY(command(a,&in->context,&id,&r->command_age_ms));
r->boot_id=a->config.domain_boot;
r->tx_sequence=a->next_can_sequence;
r->target_boot_id=a->config.bcm_boot;
r->command_id=in->context.commandId;
r->accept_limit_ms=a->config.command_accept_limit_ms;
ND_TO(DoorLockTarget_t,in->target,r->door_lock_target);
ND_TRY(origin(in->context.origin,&r->origin));
return nd_send_can(a,&m,&id);
}
static NetStatus bcm_climate(NetDomainC2 *a,const DomainIf_BcmClimateCommand_t *in){NetUpperCanMessage m;
NetworkLocalFailure_t id;
NetWireBcmClimateCommandRaw *r=&m.data.bcm_climate_command;
if(in==NULL)return NET_INVALID_ARGUMENT;
memset(&m,0,sizeof(m));
m.id=0x110u;
ND_TRY(command(a,&in->context,&id,&r->command_age_ms));
r->boot_id=a->config.domain_boot;
r->tx_sequence=a->next_can_sequence;
r->target_boot_id=a->config.bcm_boot;
r->command_id=in->context.commandId;
r->accept_limit_ms=a->config.command_accept_limit_ms;
ND_TO(FanLevel_t,in->fanTarget,r->fan_target_level);
ND_TO(ThermalDirection_t,in->thermalDirection,r->thermal_direction);
r->thermal_target_percent=in->thermalTargetLevelPercent;
return nd_send_can(a,&m,&id);
}
static NetStatus bcm_light(NetDomainC2 *a,const DomainIf_BcmInteriorLightCommand_t *in){NetUpperCanMessage m;
NetworkLocalFailure_t id;
NetWireBcmLightCommandRaw *r=&m.data.bcm_light_command;
if(in==NULL)return NET_INVALID_ARGUMENT;
memset(&m,0,sizeof(m));
m.id=0x120u;
ND_TRY(command(a,&in->context,&id,&r->command_age_ms));
r->boot_id=a->config.domain_boot;
r->tx_sequence=a->next_can_sequence;
r->target_boot_id=a->config.bcm_boot;
r->command_id=in->context.commandId;
r->accept_limit_ms=a->config.command_accept_limit_ms;
ND_TO(InteriorLightType_t,in->type,r->interior_light_type);
r->light_level_percent=in->levelPercent;
r->red=in->color.red;
r->green=in->color.green;
r->blue=in->color.blue;
return nd_send_can(a,&m,&id);
}
/* CAN UPDATE_SEQUENCE of DOMAIN_PERMISSION, synthesized by Network in a number
 * space apart from the DOMAIN success evaluation sequence
 * (docs/network/interface/04_COMMON/10_DATA_AND_ID_RULES.md, number spaces of
 * the original success evaluation, the synthesis and the transmission;
 * 03_COMMUNICATION/10_CAN_FD.md DOMAIN_PERMISSION: the "new evaluation number"
 * of B13~16 is the Network synthesized UPDATE_SEQUENCE). DOMAIN keeps its
 * success sequence when an evaluation fails and reports false with a non-OK
 * quality; under one number a receiver holds that withdrawal as a changed
 * value of the same observation, so the withdrawal would wait for the 500 ms
 * original life. Compared tuple: the DOMAIN sequence, the three values (power
 * after the UNKNOWN projection) and the two qualities.
 *   +1   : the first frame, a newer DOMAIN sequence (u32 serial order, 1..2^31-1
 *          ahead), or a value or quality change under the same DOMAIN sequence
 *          (a withdrawal or a quality drop is sent at once);
 *   kept : the same tuple, a repetition: same number, AGE only grows (a
 *          smaller AGE is HOLD);
 *   HOLD : an older DOMAIN sequence; under the same DOMAIN sequence a change
 *          that lifts a restriction (power not 1 -> 1, operation 0 -> 1, a
 *          quality non-OK -> OK). Permission comes back only with a newer
 *          DOMAIN sequence (07_CROSS_NODE_INTERFACE_RULES.md, permission
 *          adapter: an old ALLOW is never renewed by a repetition;
 *          10_DATA_AND_ID_RULES.md: the DOMAIN sequence grows only on a
 *          successful evaluation).
 * The state commits only when the service accepts the frame: a refused
 * submission uses no number. */
uint32_t nd_permission_next(uint32_t last){return last==UINT32_MAX?1u:last+1u;
}
static bool relaxes(const NetDomainC2PermissionSequence *p,const NetWireDomainPermissionRaw *r){
return (p->power!=1u&&r->vehicle_power_permission==1u)||(p->operation_allowed==0u&&r->operation_allowed==1u)||
(p->power_quality!=0u&&r->power_permission_quality==0u)||(p->operation_quality!=0u&&r->operation_quality==0u);
}
static NetStatus permission_sequence(NetDomainC2 *a,const DomainIf_CisPowerPermission_t *in,const NetWireDomainPermissionRaw *r,uint32_t *out,bool *advance){
NetDomainC2PermissionSequence *p=&a->permission;
uint32_t ahead;
bool values,qualities;
*advance=true;
if(!p->sent){*out=nd_permission_next(p->sequence);
return NET_OK;
}
ahead=in->meta.updateSequence-p->domain_sequence;
if(ahead>=0x80000000u){net_upper_observe_increment(&p->held_older);
return NET_HOLD;
}
if(ahead!=0u){*out=nd_permission_next(p->sequence);
return NET_OK;
}
values=p->power!=r->vehicle_power_permission||p->operation_state!=r->power_operation_state||p->operation_allowed!=r->operation_allowed;
qualities=p->power_quality!=r->power_permission_quality||p->operation_quality!=r->operation_quality;
if(!values&&!qualities){if(in->meta.ageMs<p->age_ms){net_upper_observe_increment(&p->held_age);
return NET_HOLD;
}
*advance=false;*out=p->sequence;
return NET_OK;
}
if(relaxes(p,r)){net_upper_observe_increment(&p->held_relax);return NET_HOLD;}
*out=nd_permission_next(p->sequence);
return NET_OK;
}
static void permission_commit(NetDomainC2 *a,const DomainIf_CisPowerPermission_t *in,const NetWireDomainPermissionRaw *r,bool advance){
NetDomainC2PermissionSequence *p=&a->permission;
p->sent=true;
p->sequence=r->update_sequence;
p->domain_sequence=in->meta.updateSequence;
p->age_ms=in->meta.ageMs;
p->power=r->vehicle_power_permission;
p->power_quality=r->power_permission_quality;
p->operation_state=r->power_operation_state;
p->operation_allowed=r->operation_allowed;
p->operation_quality=r->operation_quality;
net_upper_observe_increment(advance?&p->advanced:&p->kept);
}
static NetStatus permission(NetDomainC2 *a,const DomainIf_CisPowerPermission_t *in){NetUpperCanMessage m;
NetWireDomainPermissionRaw *r=&m.data.domain_permission;
uint8_t quality;
uint32_t sequence;
bool advance;
NetStatus status;
if(in==NULL)return NET_INVALID_ARGUMENT;
if(a->config.permission_bundle==NULL)return NET_HOLD;
memset(&m,0,sizeof(m));
m.id=0x080u;
ND_TRY(a->config.permission_bundle(a->config.context,in,r));
ND_TO(DataQuality_t,in->quality,quality);
/* Complete Power/Operation bundle comes from owner; never fill ALLOW or state.
 * Its UPDATE_SEQUENCE is not used: the adapter synthesizes it below. */
if(r->power_permission_quality!=quality||r->source_age_ms!=in->meta.ageMs)return NET_HOLD;
/* Power on CAN (10_CAN_FD.md DOMAIN_PERMISSION, semantic projection): a
 * confirmed ALLOW / DENY with OK quality is 1 / 0; a non-OK quality is the
 * UNKNOWN power 255 with that quality. DOMAIN makes no true with a non-OK
 * quality (that and a bundle value 1 there are HOLD). */
if(in->quality==DATA_QUALITY_OK){if(r->vehicle_power_permission!=(in->vehiclePowerAllowed?1u:0u))return NET_HOLD;
}
else{if(in->vehiclePowerAllowed||(r->vehicle_power_permission!=0u&&r->vehicle_power_permission!=255u))return NET_HOLD;
r->vehicle_power_permission=255u;
}
ND_TRY(permission_sequence(a,in,r,&sequence,&advance));
r->boot_id=a->config.domain_boot;
r->tx_sequence=a->next_can_sequence;
r->update_sequence=sequence;
status=nd_send_can(a,&m,NULL);
if(status==NET_OK)permission_commit(a,in,r,advance);
return status;
}
static NetStatus vss_event(NetDomainC2 *a,const DomainIf_VssOneShotEvent_t *in){NetUpperCanMessage m;
NetworkLocalFailure_t id;
NetWireVssEventRaw *r=&m.data.vss_event;
if(in==NULL)return NET_INVALID_ARGUMENT;
memset(&m,0,sizeof(m));
memset(&id,0,sizeof(id));
m.id=0x160u;
r->boot_id=a->config.domain_boot;
r->tx_sequence=a->next_can_sequence;
ND_TO(VssOneShotEvent_t,in->event,r->event_kind);
r->occurrence_id=in->occurrenceId;
ND_TRY(age16(in->meta.ageMs,&r->original_age_ms));
r->use_limit_ms=a->config.event_use_limit_ms;
ND_TO(DataQuality_t,in->meta.quality,r->value_quality);
ND_TO(FunctionAvailability_t,in->availability,r->input_availability);
r->related_command_id=in->hasRelatedCommand?in->relatedCommandId:0u;
id.link=NETWORK_LINK_CAN;
id.itemKind=NETWORK_LOCAL_ITEM_OCCURRENCE;
id.identityGeneration=a->config.domain_boot;
id.hasOccurrenceId=true;
id.occurrenceId=in->occurrenceId;
return nd_send_can(a,&m,&id);
}
static NetStatus warning_evidence(const DomainIf_WarningInputEvidence_t *e,uint32_t *boot,uint32_t *seq,uint16_t *age,uint8_t *quality,uint8_t *why,uint8_t *availability){
*boot=e->source.generation;
*seq=e->source.sequence;
ND_TRY(age16(e->source.ageMs,age));
ND_TO(DataQuality_t,e->quality,*quality);
ND_TO(QualityReason_t,e->reason,*why);
if(e->availability==FUNCTION_AVAILABILITY_UNSUPPORTED){*quality=2u;*why=9u;*availability=2u;}else{ND_TO(FunctionAvailability_t,e->availability,*availability);}
return NET_OK;
}
static NetStatus vss_warning(NetDomainC2 *a,const DomainIf_VssWarningState_t *in){NetUpperCanMessage m;
NetWireVssWarningStateRaw *r=&m.data.vss_warning_state;
if(in==NULL)return NET_INVALID_ARGUMENT;
memset(&m,0,sizeof(m));
m.id=0x0c0u;
r->boot_id=a->config.domain_boot;
r->tx_sequence=a->next_can_sequence;
/* Unsupported WINDOW is the prescribed non-normal A1 tuple, never CLEAR+OK. */
if(in->antiPinchEvidence.availability==FUNCTION_AVAILABILITY_UNSUPPORTED){r->antipinch_quality=3u;
r->antipinch_reason=6u;
r->antipinch_availability=2u;
r->antipinch_original_age_ms=UINT16_MAX;
}
else{ND_TO(AntiPinchState_t,in->windowAntiPinchState,r->antipinch_state);
ND_TRY(warning_evidence(&in->antiPinchEvidence,&r->antipinch_source_boot,&r->antipinch_source_sequence,&r->antipinch_original_age_ms,&r->antipinch_quality,&r->antipinch_reason,&r->antipinch_availability));
}
if(in->occupantHazardState==OCCUPANT_HAZARD_UNCONFIRMED)r->occupant_hazard_state=0u;else{ND_TO(OccupantHazardState_t,in->occupantHazardState,r->occupant_hazard_state);}
ND_TRY(warning_evidence(&in->occupantEvidence,&r->occupant_hazard_source_boot,&r->occupant_hazard_source_sequence,&r->occupant_hazard_original_age_ms,&r->occupant_hazard_quality,&r->occupant_hazard_reason,&r->occupant_hazard_availability));
if(in->occupantHazardState==OCCUPANT_HAZARD_UNCONFIRMED&&r->occupant_hazard_quality==0u){r->occupant_hazard_quality=2u;r->occupant_hazard_reason=9u;r->occupant_hazard_availability=2u;}
ND_TO(RearObstacleState_t,in->rearObstacleState,r->rear_obstacle_state);
ND_TRY(warning_evidence(&in->rearEvidence,&r->rear_obstacle_source_boot,&r->rear_obstacle_source_sequence,&r->rear_obstacle_original_age_ms,&r->rear_obstacle_quality,&r->rear_obstacle_reason,&r->rear_obstacle_availability));
ND_TO(RearDetectionActivation_t,in->rearDetectionActivation,r->rear_detection_activation);
r->activation_meta_format=1u;
ND_TO(DataQuality_t,in->rearActivationMeta.quality,r->rear_activation_quality);
ND_TO(QualityReason_t,in->rearActivationReason,r->rear_activation_reason);
r->rear_activation_sequence=in->rearActivationMeta.updateSequence;
ND_TRY(age16(in->rearActivationMeta.ageMs,&r->rear_activation_age_ms));
return nd_send_can(a,&m,NULL);
}
NetStatus nd_core_message(NetDomainC2 *a,uint8_t type,const void *object,NetUpperUartMessage *out){NetUpperUartMessage m;
uint32_t device;
uint64_t session;
if(object==NULL||out==NULL)return NET_INVALID_ARGUMENT;
ND_TRY(nd_context(a,&device,&session));
memset(&m,0,sizeof(m));
m.link=NET_WIRE_UART_M;
m.type=type;
switch(type){
case 0x20u:{const DomainIf_MobileRequestResult_t *v=object;
NetWireMResult *r=&m.data.m_result;
if(v->deviceContextId!=device)return NET_HOLD;
r->request_session=v->resultInfo.context.sessionId;
r->request_id=v->resultInfo.context.requestId;
if(v->hasVehicleResult){ND_TO(RequestResult_t,v->resultInfo.result,r->result);
}
else{if(v->resultInfo.confirmation!=RESULT_CONFIRMATION_UNCONFIRMED)return NET_HOLD;
r->result=255u;
}
ND_TRY(reason(v->resultInfo.reason,&r->result_reason));
ND_TO(ResultConfirmation_t,v->resultInfo.confirmation,r->result_confirmation);
switch(v->target){case MOBILE_RESULT_TARGET_DOMAIN_SETTING:r->command_target=0u;
break;
case MOBILE_RESULT_TARGET_BCM:r->command_target=1u;
break;
case MOBILE_RESULT_TARGET_WINDOW:r->command_target=2u;
break;
default:return NET_HOLD;
}
r->command_id=v->commandId;
ND_TRY(origin(v->origin,&r->origin));
ND_TRY(age16(v->resultAgeMs,&r->result_age_ms));
break;
}
case 0x21u:{const DomainIf_WarningInfo_t *v=object;
NetWireMWarningRaw *r=&m.data.m_warning;
if(v->simulated)return NET_HOLD;
ND_TO(DomainWarningType_t,v->type,r->warning_type);
ND_TO(DomainWarningSeverity_t,v->severity,r->severity);
ND_TO(DomainWarningState_t,v->state,r->warning_state);
r->occurrence_id=v->occurrenceId;
r->read_state=v->isRead?1u:0u;
ND_TO(DataQuality_t,v->quality,r->value_quality);
ND_TO(QualityReason_t,v->qualityReason,r->quality_reason);
r->source_boot=v->source.generation;
r->source_sequence=v->source.sequence;
ND_TRY(age16(v->source.ageMs,&r->source_age_ms));
break;
}
case 0x22u:{const DomainIf_FunctionAvailabilitySet_t *v=object;
NetWireMAvailabilityRaw *r=&m.data.m_availability;
size_t i;
bool used[9]={false};
for(i=0u;i<DOMAIN_FUNCTION_COUNT;++i){uint8_t wire;
NetWireMAvailabilityItemRaw *item;
ND_TO(DomainFunctionId_t,v->items[i].functionId,wire);
if(wire<1u||wire>9u||used[wire-1u])return NET_HOLD;
used[wire-1u]=true;
item=&r->items[wire-1u];
item->function_id=wire;
ND_TO(FunctionAvailability_t,v->items[i].availability,item->availability);
ND_TRY(reason(v->items[i].reason,&item->reason));
}
r->update_sequence=v->meta.updateSequence;
ND_TRY(age16(v->meta.ageMs,&r->source_age_ms));
break;
}
case 0x23u:{const DigitalKeyState_t *v=object;
NetWireMDigitalStatusRaw *r=&m.data.m_digital_status;
ND_TO(DigitalKeySetting_t,v->setting,r->digital_key_setting_state);
ND_TO(FunctionAvailability_t,v->availability,r->digital_key_availability);
ND_TRY(reason(v->reason,&r->reason));
r->update_sequence=v->meta.updateSequence;
ND_TRY(age16(v->meta.ageMs,&r->source_age_ms));
break;
}
case 0x24u:{const DomainIf_DigitalKeyResult_t *v=object;
NetWireMDigitalResultRaw *r=&m.data.m_digital_result;
if(v->hasVehicleResult){ND_TO(RequestResult_t,v->result,r->result);
}
else{if(v->confirmation!=RESULT_CONFIRMATION_UNCONFIRMED)return NET_HOLD;
r->result=255u;
}
ND_TRY(reason(v->reason,&r->result_reason));
ND_TO(ResultConfirmation_t,v->confirmation,r->result_confirmation);
switch(v->origin){case UNLOCK_ORIGIN_USER_REQUEST:r->origin=1u;
break;
case UNLOCK_ORIGIN_PROXIMITY_AUTO:r->origin=2u;
break;
default:return NET_HOLD;
}
/* A false actual transition must not silently become a confirmed auto unlock. */
if(v->hasVehicleResult&&v->result==REQUEST_RESULT_DONE&&v->reason!=RESULT_REASON_ALREADY_AT_TARGET&&!v->actualUnlockTransition)return NET_HOLD;
r->auto_occurrence_id=v->occurrenceId;
r->related_door_command_id=v->relatedDoorCommandId;
ND_TRY(age16(v->resultAgeMs,&r->result_age_ms));
break;
}
case 0x25u:{const DomainIf_UserSettingsState_t *v=object;
NetWireMUserSettingsRaw *r=&m.data.m_user_settings;
if(!v->hasNormalLightLevel||!v->hasNormalLightColor)return NET_HOLD;
r->target_temperature_centi_c=v->hasTargetTemperature?v->targetTemperatureCentiC:INT16_MIN;
r->climate_auto=v->hasClimateAutoSetting?(v->climateAutoEnabled?1u:0u):255u;
if(v->hasUserFanLevel){ND_TO(FanLevel_t,v->userFanLevel,r->user_fan_level);
}
else r->user_fan_level=255u;
r->interior_light_enabled=v->hasInteriorLightEnableSetting?(v->interiorLightEnabled?1u:0u):255u;
r->normal_light_level_percent=v->normalLightLevelPercent;
r->normal_light_rgb[0]=v->normalLightColor.red;
r->normal_light_rgb[1]=v->normalLightColor.green;
r->normal_light_rgb[2]=v->normalLightColor.blue;
ND_TO(DataQuality_t,v->meta.quality,r->value_quality);
r->update_sequence=v->meta.updateSequence;
ND_TRY(age16(v->meta.ageMs,&r->source_age_ms));
break;
}
default:return NET_UNSUPPORTED;
}
ND_TRY(net_upper_wire_status(net_upper_uart_set_query_context(&m,device,session,a->config.domain_boot,0u)));
*out=m;
return NET_OK;
}
static NetStatus publish_core(NetDomainC2 *a,uint8_t type,const void *value){NetUpperUartMessage m;
NetworkLocalFailure_t identity;
NetworkLocalFailure_t *id=NULL;
ND_TRY(nd_core_message(a,type,value,&m));
if(type==0x20u){const DomainIf_MobileRequestResult_t *v=value;
memset(&identity,0,sizeof(identity));
identity.link=NETWORK_LINK_UART_M;
identity.itemKind=NETWORK_LOCAL_ITEM_REQUEST_RESULT;
identity.identityGeneration=a->config.domain_boot;
identity.hasRequestContext=true;
identity.requestDeviceContextId=v->deviceContextId;
identity.requestContext=v->resultInfo.context;
id=&identity;
}
return nd_send_uart(a,&m,id);
}
static NetStatus window_command(NetDomainC2 *a,const DomainIf_WindowCommand_t *in){
NetUpperCanMessage m;
NetworkLocalFailure_t id;
NetWireWindowCommandRaw *r=&m.data.window_command;
if(!a->config.window_transport_enabled||!a->service->config.inspection_transport)return NET_HOLD;
if(a->next_can_sequence==0u||a->next_can_sequence==UINT32_MAX)return NET_HOLD;
if(a->config.window_boot==0u||in->context.commandId==0u||a->config.command_accept_limit_ms==0u)return NET_HOLD;
memset(&m,0,sizeof(m));
memset(&id,0,sizeof(id));
m.id=0x090u;
r->boot_id=a->config.domain_boot;
r->tx_sequence=a->next_can_sequence;
r->target_boot_id=a->config.window_boot;
r->command_id=in->context.commandId;
ND_TRY(age16(in->context.commandAgeMs,&r->command_age_ms));
r->accept_limit_ms=a->config.command_accept_limit_ms;
ND_TO(WindowCommand_t,in->command,r->window_command);
r->target_channel=in->channel;
r->target_position=in->targetPositionPercent;
switch(in->context.origin){case COMMAND_ORIGIN_AUTO_VENTILATION:r->source_context=2u;
break;
case COMMAND_ORIGIN_CLIMATE_POLICY:case COMMAND_ORIGIN_SYSTEM:r->source_context=1u;
break;
default:return NET_HOLD;
}
/* Inspection route stays disconnected from Owner actuator callbacks. */
{NetSourceStamp source;
NetTxKey key;
uint64_t now;
ND_TRY(net_upper_now(a->service,&now));
source.observation_id=a->next_can_sequence;
source.observed_at_ms=now;
source.original_age_ms=r->command_age_ms;
ND_TRY(net_upper_submit_inspection_can(a->service,&m,&source,&a->config.transmit_policy,&key));
a->last_tx_key=key;
++a->next_can_sequence;
return NET_OK;
}
}
/* WINDOW is held (implementation on hold, design kept). DOMAIN_PERMISSION has
 * one synthesizer, the CIS power permission above: the WINDOW operation and
 * DOMAIN alive inputs never send a 0x080 of their own, whatever the
 * window_transport_enabled setting and the bundles bound; a second source
 * would break the synthesized UPDATE_SEQUENCE of the one message. */
static NetStatus window_permission(NetDomainC2 *a,const DomainIf_WindowOperationPermission_t *in){
(void)a;
(void)in;
return NET_HOLD;
}
static NetStatus window_alive(NetDomainC2 *a,const DomainIf_WindowDomainAlive_t *in){
(void)a;
(void)in;
return NET_HOLD;
}
#define PORT(name,type,body) static DomainIf_Status_t name(const type *v){NetDomainC2 *a=nd_registered();NetStatus status;if(a==NULL||!nd_ready(a))return DOMAIN_IF_NOT_AVAILABLE;if(v==NULL)return DOMAIN_IF_INVALID_ARGUMENT;if(a->in_call||a->service->in_call)return DOMAIN_IF_NOT_AVAILABLE;status=(body);a->last_status=status;return net_domain_c2_to_status(status);}
PORT(tx_door,DomainIf_BcmDoorCommand_t,bcm_door(a,v))
PORT(tx_climate,DomainIf_BcmClimateCommand_t,bcm_climate(a,v))
PORT(tx_light,DomainIf_BcmInteriorLightCommand_t,bcm_light(a,v))
PORT(tx_power,DomainIf_CisPowerPermission_t,permission(a,v))
PORT(tx_window,DomainIf_WindowCommand_t,window_command(a,v))
PORT(tx_window_permission,DomainIf_WindowOperationPermission_t,window_permission(a,v))
PORT(tx_window_alive,DomainIf_WindowDomainAlive_t,window_alive(a,v))
PORT(tx_event,DomainIf_VssOneShotEvent_t,vss_event(a,v))
PORT(tx_warning_state,DomainIf_VssWarningState_t,vss_warning(a,v))
PORT(tx_result,DomainIf_MobileRequestResult_t,publish_core(a,0x20u,v))
PORT(tx_m_door,DoorState_t,nd_publish_projection(a,0x200u,v))
PORT(tx_m_climate,ClimateState_t,nd_publish_projection(a,0x210u,v))
PORT(tx_m_light,InteriorLightState_t,nd_publish_projection(a,0x220u,v))
PORT(tx_m_bcm,DomainIf_BcmStatusReport_t,nd_publish_projection(a,0x230u,v))
PORT(tx_m_environment,CisCabinEnvironmentState_t,nd_publish_projection(a,0x300u,v))
PORT(tx_m_occupant,CisOccupantState_t,nd_publish_projection(a,0x310u,v))
PORT(tx_m_rear,CisRearState_t,nd_publish_projection(a,0x0a0u,v))
PORT(tx_m_cis,DomainIf_CisStatusReport_t,nd_publish_projection(a,0x320u,v))
PORT(tx_m_vss,DomainIf_VssStateReport_t,nd_publish_projection(a,0x340u,v))
PORT(tx_m_window,WindowState_t,nd_publish_projection(a,0x0b0u,v))
PORT(tx_m_window_fault,DomainIf_WindowFaultState_t,nd_publish_projection(a,0x330u,v))
PORT(tx_m_warning,DomainIf_WarningInfo_t,publish_core(a,0x21u,v))
PORT(tx_m_availability,DomainIf_FunctionAvailabilitySet_t,publish_core(a,0x22u,v))
PORT(tx_m_digital,DigitalKeyState_t,publish_core(a,0x23u,v))
PORT(tx_m_digital_result,DomainIf_DigitalKeyResult_t,publish_core(a,0x24u,v))
PORT(tx_m_settings,DomainIf_UserSettingsState_t,publish_core(a,0x25u,v))
#undef PORT
static const DomainIf_TxPort_t ports={tx_door,tx_climate,tx_light,tx_power,tx_window,tx_window_permission,tx_window_alive,tx_event,tx_warning_state,tx_result,tx_m_door,tx_m_climate,tx_m_light,tx_m_bcm,tx_m_environment,tx_m_occupant,tx_m_rear,tx_m_cis,tx_m_vss,tx_m_window,tx_m_window_fault,tx_m_warning,tx_m_availability,tx_m_digital,tx_m_digital_result,tx_m_settings};
const DomainIf_TxPort_t *net_domain_c2_tx_port(void){return &ports;
}
