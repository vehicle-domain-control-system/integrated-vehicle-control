#include "net_domain_private.h"
NetStatus nd_map_can(NetDomainC2 *a,const NetUpperCanMessage *m,uint64_t elapsed,NdCanValue *out) {
if(a==NULL||m==NULL||out==NULL)return NET_INVALID_ARGUMENT;
switch(m->id){
case 0x200u: { const NetWireBcmDoorStateRaw *r=&m->data.bcm_door_state;
 DoorState_t v;
 memset(&v,0,sizeof(v));
ND_FROM(DoorLockState_t,r->door_lock_state,v.lockState);
ND_FROM(DoorOpenState_t,r->door_open_state,v.openState);
ND_FROM(DoorCompositeState_t,r->door_composite_state,v.compositeState);
ND_FROM(DataQuality_t,r->lock_quality,v.lockQuality);
ND_FROM(DataQuality_t,r->open_quality,v.openQuality);
ND_FROM(DataQuality_t,r->composite_quality,v.compositeQuality);
v.sourceGeneration=r->boot_id;
v.meta.updateSequence=r->update_sequence;
v.meta.quality=v.lockQuality;
if(v.openQuality>v.meta.quality)v.meta.quality=v.openQuality;
if(v.compositeQuality>v.meta.quality)v.meta.quality=v.compositeQuality;
ND_TRY(nd_age(r->source_age_ms,elapsed,&v.meta.ageMs));
out->door=v;
return NET_OK;
}
case 0x210u: { const NetWireBcmClimateStateRaw *r=&m->data.bcm_climate_state;
 ClimateState_t v;
 memset(&v,0,sizeof(v));
ND_FROM(FanLevel_t,r->fan_command_level,v.commandedFanLevel);
ND_FROM(FanLevel_t,r->fan_measured_level,v.measuredFanLevel);
ND_FROM(ThermalDirection_t,r->thermal_direction_state,v.thermalDirection);
ND_FROM(HeatRemovalState_t,r->heat_removal_state,v.heatRemovalState);
v.thermalOutputLevelPercent=r->thermal_output_percent;
ND_FROM(DataQuality_t,r->fan_command_quality,v.commandedFanQuality);
ND_FROM(DataQuality_t,r->fan_measured_quality,v.measuredFanQuality);
ND_FROM(DataQuality_t,r->thermal_direction_quality,v.thermalDirectionQuality);
ND_FROM(DataQuality_t,r->thermal_output_quality,v.thermalOutputQuality);
ND_FROM(DataQuality_t,r->heat_removal_quality,v.heatRemovalQuality);
v.meta.quality=v.commandedFanQuality;
if(v.measuredFanQuality>v.meta.quality)v.meta.quality=v.measuredFanQuality;
if(v.thermalDirectionQuality>v.meta.quality)v.meta.quality=v.thermalDirectionQuality;
if(v.thermalOutputQuality>v.meta.quality)v.meta.quality=v.thermalOutputQuality;
if(v.heatRemovalQuality>v.meta.quality)v.meta.quality=v.heatRemovalQuality;
v.meta.updateSequence=r->update_sequence;
ND_TRY(nd_age(r->source_age_ms,elapsed,&v.meta.ageMs));
out->climate=v;
return NET_OK;
}
case 0x220u: { const NetWireBcmLightStateRaw *r=&m->data.bcm_light_state;
 InteriorLightState_t v;
 memset(&v,0,sizeof(v));
ND_FROM(InteriorLightType_t,r->interior_light_type_state,v.type);
v.levelPercent=r->light_level_percent;
v.color.red=r->red;
v.color.green=r->green;
v.color.blue=r->blue;
ND_FROM(InteriorLightApplyState_t,r->interior_light_apply_result,v.applyState);
if(r->light_physical_feedback_capability>1u)return NET_HOLD;
v.physicalFeedbackSupported=r->light_physical_feedback_capability==1u;
ND_FROM(DataQuality_t,r->light_type_quality,v.typeQuality);
ND_FROM(DataQuality_t,r->light_level_quality,v.levelQuality);
ND_FROM(DataQuality_t,r->light_color_quality,v.colorQuality);
ND_FROM(DataQuality_t,r->light_apply_quality,v.applyQuality);
v.meta.quality=v.typeQuality;
if(v.levelQuality>v.meta.quality)v.meta.quality=v.levelQuality;
if(v.colorQuality>v.meta.quality)v.meta.quality=v.colorQuality;
if(v.applyQuality>v.meta.quality)v.meta.quality=v.applyQuality;
v.meta.updateSequence=r->update_sequence;
ND_TRY(nd_age(r->source_age_ms,elapsed,&v.meta.ageMs));
out->light=v;
return NET_OK;
}
case 0x230u: { const NetWireBcmStatusRaw *r=&m->data.bcm_status;
 DomainIf_BcmStatusReport_t v;
 memset(&v,0,sizeof(v));
ND_FROM(CommonEcuState_t,r->bcm_ecu_state,v.ecuState);
v.faultMask=r->fault_mask;
v.activeFault.faultCode=r->active_fault_code;
if(r->active_fault_code!=0u){ND_FROM(FaultCategory_t,r->active_fault_category,v.activeFault.category);
v.activeFault.state=r->recovering==1u?FAULT_STATE_RECOVERING:FAULT_STATE_ACTIVE;
}
if(a->config.status_quality==NULL)return NET_HOLD;
ND_TRY(a->config.status_quality(a->config.context,m->id,m,&v.meta.quality));
v.activeFault.quality=v.meta.quality;
v.affectedFunctionMask=r->affected_function;
v.recovering=r->recovering==1u;
v.sourceGeneration=r->boot_id;
v.meta.updateSequence=r->update_sequence;
ND_TRY(nd_age(r->source_age_ms,elapsed,&v.meta.ageMs));
out->bcm_status=v;
return NET_OK;
}
case 0x300u: { const NetWireCisEnvironmentRaw *r=&m->data.cis_environment;
 CisCabinEnvironmentState_t v;
 memset(&v,0,sizeof(v));
v.cabinTemperature=r->temperature_centi_c;
v.cabinHumidity=r->humidity_centi_percent;
v.cabinIlluminance=r->illuminance_lux;
ND_TRY(nd_value(a,r->temperature_validity,r->temperature_reason,r->temperature_sequence,r->temperature_age_ms,elapsed,&v.temperatureQuality));
ND_TRY(nd_value(a,r->humidity_validity,r->humidity_reason,r->humidity_sequence,r->humidity_age_ms,elapsed,&v.humidityQuality));
ND_TRY(nd_value(a,r->illuminance_validity,r->illuminance_reason,r->illuminance_sequence,r->illuminance_age_ms,elapsed,&v.illuminanceQuality));
out->environment=v;
return NET_OK;
}
case 0x310u: { const NetWireCisOccupantRaw *r=&m->data.cis_occupant;
 CisOccupantState_t v;
 memset(&v,0,sizeof(v));
v.occupantPresent=r->occupant_presence==1u;
v.occupantCount=r->occupant_count;
ND_TRY(nd_value(a,r->presence_validity,r->presence_reason,r->vision_sequence,r->vision_age_ms,elapsed,&v.presenceQuality));
ND_TRY(nd_value(a,r->count_validity,r->count_reason,r->vision_sequence,r->vision_age_ms,elapsed,&v.countQuality));
v.sourceGeneration=r->pi_boot_id;
ND_FROM(CisFunctionStatus_t,r->vision_function_status,v.visionFunctionStatus);
out->occupant=v;
return NET_OK;
}
case 0x0a0u: { const NetWireCisRearRaw *r=&m->data.cis_rear;
 CisRearState_t v;
 memset(&v,0,sizeof(v));
v.rearDistanceMm=r->rear_distance_deci_cm;
ND_FROM(RearMeasurementState_t,r->proximity_status,v.measurementState);
ND_TRY(nd_value(a,r->validity,r->quality_reason,r->update_sequence,r->source_age_ms,elapsed,&v.distanceQuality));
v.sourceGeneration=r->boot_id;
out->rear=v;
return NET_OK;
}
case 0x320u: { const NetWireCisStatusRaw *r=&m->data.cis_status;
 DomainIf_CisStatusReport_t v;
 memset(&v,0,sizeof(v));
ND_FROM(CisState_t,r->cis_state,v.ecuState);
ND_FROM(CisFunctionStatus_t,r->vision_status,v.occupantFunctionStatus);
ND_FROM(CisFunctionStatus_t,r->temperature_status,v.temperatureFunctionStatus);
ND_FROM(CisFunctionStatus_t,r->humidity_status,v.humidityFunctionStatus);
ND_FROM(CisFunctionStatus_t,r->illuminance_status,v.illuminanceFunctionStatus);
ND_FROM(CisFunctionStatus_t,r->rear_status,v.rearFunctionStatus);
switch(r->pi_uart_status){case 0:v.piUartStatus=CIS_INTERFACE_AVAILABLE;
break;
case 1:v.piUartStatus=CIS_INTERFACE_UNAVAILABLE;
break;
case 2:v.piUartStatus=CIS_INTERFACE_RECOVERING;
break;
default:return NET_HOLD;
}
switch(r->can_interface_status){case 0:v.canInterfaceStatus=CIS_INTERFACE_AVAILABLE;
break;
case 1:v.canInterfaceStatus=CIS_INTERFACE_UNAVAILABLE;
break;
case 2:v.canInterfaceStatus=CIS_INTERFACE_RECOVERING;
break;
default:return NET_HOLD;
}
v.faultMask=r->fault_mask;
v.activeFaultCode=r->active_fault;
v.recovering=r->recovering==1u;
v.lastFaultCode=r->last_fault;
v.lastFaultOccurrenceId=r->last_fault_occurrence;
v.affectedFunctionMask=r->affected_function;
v.sourceGeneration=r->boot_id;
v.meta.updateSequence=r->update_sequence;
if(a->config.status_quality==NULL)return NET_HOLD;
ND_TRY(a->config.status_quality(a->config.context,m->id,m,&v.meta.quality));
ND_TRY(nd_age(r->source_age_ms,elapsed,&v.meta.ageMs));
out->cis_status=v;
return NET_OK;
}
case 0x340u: { const NetWireVssStatusRaw *r=&m->data.vss_status;
 DomainIf_VssStateReport_t v;
 memset(&v,0,sizeof(v));
ND_FROM(VssState_t,r->vss_state,v.service.state);
ND_FROM(VssAvailability_t,r->vss_availability,v.service.availability);
v.service.acceptingEvents=r->vss_accepting_events==1u;
v.service.faultActive=r->vss_fault_active==1u;
v.faultMask=r->fault_mask;
v.lastFaultCode=r->vss_last_fault;
v.lastFaultOccurrenceId=r->last_fault_occurrence;
ND_TRY(nd_age(r->last_fault_age_ms,elapsed,&v.lastFaultAgeMs));
ND_FROM(DataQuality_t,r->last_fault_quality,v.faultQuality);
v.sourceGeneration=r->boot_id;
v.service.meta.updateSequence=r->update_sequence;
if(a->config.status_quality==NULL)return NET_HOLD;
ND_TRY(a->config.status_quality(a->config.context,m->id,m,&v.service.meta.quality));
ND_TRY(nd_age(r->source_age_ms,elapsed,&v.service.meta.ageMs));
out->vss=v;
return NET_OK;
}
case 0x140u: { const NetWireBcmResultRaw *r=&m->data.bcm_result;
 DomainIf_BcmCommandResult_t v;
 memset(&v,0,sizeof(v));
if(r->command_domain_boot!=a->config.domain_boot)return NET_HOLD;
v.commandId=r->command_id;
ND_FROM(DomainIf_BcmCommandKind_t,r->command_kind,v.kind);
ND_FROM(RequestResult_t,r->result,v.result);
if(r->result_reason>255u)return NET_HOLD;
ND_FROM(ResultReason_t,(uint8_t)r->result_reason,v.reason);
v.confirmation=RESULT_CONFIRMATION_UNCONFIRMED;
v.hasDoorEvidence=r->door_event!=0u;
if(v.hasDoorEvidence){
switch(r->door_event){case 1:v.doorEvidence.type=BCM_DOOR_EVIDENCE_LOCK_COMPLETED;
break;
case 2:v.doorEvidence.type=BCM_DOOR_EVIDENCE_UNLOCK_COMPLETED;
break;
case 3:v.doorEvidence.type=BCM_DOOR_EVIDENCE_ALREADY_AT_TARGET;
break;
case 4:v.doorEvidence.type=BCM_DOOR_EVIDENCE_REQUEST_REJECTED;
break;
case 5:v.doorEvidence.type=BCM_DOOR_EVIDENCE_REQUEST_FAILED;
break;
default:return NET_HOLD;
}
v.doorEvidence.occurrenceId=r->occurrence_id;
ND_FROM(DoorLockTarget_t,r->door_target,v.doorEvidence.target);
ND_FROM(DataQuality_t,r->event_quality,v.doorEvidence.quality);
ND_TRY(nd_age(r->event_age_ms,elapsed,&v.doorEvidence.ageMs));
}
out->bcm_result=v;
return NET_OK;
}
case 0x141u: { const NetWireBcmEventRaw *r=&m->data.bcm_event;
 DomainIf_BcmProtectionEvent_t v;
 memset(&v,0,sizeof(v));
switch(r->event_kind){case 1:v.type=BCM_PROTECTION_OVERHEAT_DETECTED;
break;
case 2:v.type=BCM_PROTECTION_FAN_MISMATCH_DETECTED;
break;
case 3:v.type=BCM_PROTECTION_RECOVERY_CONFIRMED;
break;
default:return NET_HOLD;
}
v.occurrenceId=r->occurrence_id;
v.sourceGeneration=r->boot_id;
ND_FROM(DataQuality_t,r->event_quality,v.quality);
ND_TRY(nd_age(r->event_age_ms,elapsed,&v.ageMs));
v.hasRelatedCommand=r->related_command_id!=0u;
if(v.hasRelatedCommand&&r->related_domain_boot!=a->config.domain_boot)return NET_HOLD;
v.relatedCommandId=r->related_command_id;
v.affectedFunctionMask=r->affected_function;
out->bcm_event=v;
return NET_OK;
}
case 0x0b0u: { const NetWireWindowStateRaw *r=&m->data.window_state;
 WindowState_t v;
 memset(&v,0,sizeof(v));
if(!a->config.window_transport_enabled)return NET_HOLD;
/* Current wire has no channel; zero is only a single-channel provisional transport projection. */
if(!a->config.window_channel_bound)return NET_HOLD;
v.channel=a->config.window_channel;
ND_FROM(WindowMotionState_t,r->motion_state,v.motionState);
ND_FROM(CommonEcuState_t,r->window_ecu_state,v.ecuState);
v.positionPercent=r->window_position;
if(r->fully_open_state>1u||r->fully_closed_state>1u)return NET_HOLD;
v.fullyOpen=r->fully_open_state==1u;
v.fullyClosed=r->fully_closed_state==1u;
v.antiPinchState=r->antipinch_protection_status==0u?ANTIPINCH_STATE_CLEAR:ANTIPINCH_STATE_ACTIVE;
ND_FROM(DataQuality_t,r->window_value_quality,v.meta.quality);
v.meta.updateSequence=r->update_sequence;
ND_TRY(nd_age(r->source_age_ms,elapsed,&v.meta.ageMs));
v.sourceGeneration=r->boot_id;
out->window=v;
return NET_OK;
}
case 0x330u: { const NetWireWindowFaultRaw *r=&m->data.window_fault;
 DomainIf_WindowFaultState_t v;
 memset(&v,0,sizeof(v));
if(!a->config.window_transport_enabled)return NET_HOLD;
v.fault.faultCode=r->fault_code;
if(r->fault_code!=0u){ND_FROM(FaultCategory_t,r->fault_category,v.fault.category);
ND_FROM(FaultState_t,r->fault_status,v.fault.state);
}
if(a->config.status_quality==NULL)return NET_HOLD;
ND_TRY(a->config.status_quality(a->config.context,m->id,m,&v.meta.quality));
v.fault.quality=v.meta.quality;
v.occurrenceId=r->occurrence_id;
v.sourceGeneration=r->boot_id;
v.meta.updateSequence=r->update_sequence;
ND_TRY(nd_age(r->source_age_ms,elapsed,&v.meta.ageMs));
out->window_fault=v;
return NET_OK;
}
default:return NET_UNSUPPORTED;
}
}
NetStatus nd_rx_can(void *context,const NetUpperCanMessage *m,const NetUpperDelivery *d){
NetDomainC2 *a=context;
NdCanValue value;
NetStatus status;
if(a==NULL||m==NULL||d==NULL)return NET_INVALID_ARGUMENT;
if(!nd_ready(a))return NET_NOT_CONFIGURED;
if(d->observation_only)return NET_HOLD;
status=nd_map_can(a,m,d->local_elapsed_ms,&value);
if(status!=NET_OK)return status;
switch(m->id){
case 0x200u: if(a->api.RxBcmDoorState==NULL)return NET_HOLD;
ND_TRY(nd_ingress_once(a,NET_WIRE_CAN,m->id,d->source_boot,d->capture.port_epoch,d->source_sequence));
return nd_rx_call(a,a->api.RxBcmDoorState(&value.door));
case 0x210u: if(a->api.RxBcmClimateState==NULL)return NET_HOLD;
ND_TRY(nd_ingress_once(a,NET_WIRE_CAN,m->id,d->source_boot,d->capture.port_epoch,d->source_sequence));
return nd_rx_call(a,a->api.RxBcmClimateState(&value.climate));
case 0x220u: if(a->api.RxBcmInteriorLightState==NULL)return NET_HOLD;
ND_TRY(nd_ingress_once(a,NET_WIRE_CAN,m->id,d->source_boot,d->capture.port_epoch,d->source_sequence));
return nd_rx_call(a,a->api.RxBcmInteriorLightState(&value.light));
case 0x230u: if(a->api.RxBcmStatus==NULL)return NET_HOLD;
ND_TRY(nd_ingress_once(a,NET_WIRE_CAN,m->id,d->source_boot,d->capture.port_epoch,d->source_sequence));
return nd_rx_call(a,a->api.RxBcmStatus(&value.bcm_status));
/* Transition routes: the barrier is passed before the ingress journal and the
 * C2 call (environment by the CIS BOOT_ID, Occupant by PI_BOOT_ID). */
case 0x300u: ND_TRY(nd_barrier(a,NET_DOMAIN_C2_ENVIRONMENT_BARRIER,d->source_boot));
if(a->api.RxCisCabinEnvironment==NULL)return NET_HOLD;
ND_TRY(nd_ingress_once(a,NET_WIRE_CAN,m->id,d->source_boot,d->capture.port_epoch,d->source_sequence));
return nd_rx_call(a,a->api.RxCisCabinEnvironment(&value.environment));
case 0x310u: ND_TRY(nd_barrier(a,NET_DOMAIN_C2_OCCUPANT_BARRIER,m->data.cis_occupant.pi_boot_id));
if(a->api.RxCisOccupant==NULL)return NET_HOLD;
ND_TRY(nd_ingress_once(a,NET_WIRE_CAN,m->id,d->source_boot,d->capture.port_epoch,d->source_sequence));
return nd_rx_call(a,a->api.RxCisOccupant(&value.occupant));
case 0x0a0u: if(a->api.RxCisRear==NULL)return NET_HOLD;
ND_TRY(nd_ingress_once(a,NET_WIRE_CAN,m->id,d->source_boot,d->capture.port_epoch,d->source_sequence));
return nd_rx_call(a,a->api.RxCisRear(&value.rear));
case 0x320u: if(a->api.RxCisStatus==NULL)return NET_HOLD;
ND_TRY(nd_ingress_once(a,NET_WIRE_CAN,m->id,d->source_boot,d->capture.port_epoch,d->source_sequence));
return nd_rx_call(a,a->api.RxCisStatus(&value.cis_status));
case 0x340u: if(a->api.RxVssState==NULL)return NET_HOLD;
ND_TRY(nd_ingress_once(a,NET_WIRE_CAN,m->id,d->source_boot,d->capture.port_epoch,d->source_sequence));
return nd_rx_call(a,a->api.RxVssState(&value.vss));
case 0x140u: if(a->api.RxBcmCommandResult==NULL)return NET_HOLD;
ND_TRY(nd_ingress_once(a,NET_WIRE_CAN,m->id,d->source_boot,d->capture.port_epoch,d->source_sequence));
return nd_rx_call(a,a->api.RxBcmCommandResult(&value.bcm_result));
case 0x141u: if(a->api.RxBcmProtectionEvent==NULL)return NET_HOLD;
ND_TRY(nd_ingress_once(a,NET_WIRE_CAN,m->id,d->source_boot,d->capture.port_epoch,d->source_sequence));
return nd_rx_call(a,a->api.RxBcmProtectionEvent(&value.bcm_event));
case 0x0b0u: if(a->api.RxWindowState==NULL)return NET_HOLD;
ND_TRY(nd_ingress_once(a,NET_WIRE_CAN,m->id,d->source_boot,d->capture.port_epoch,d->source_sequence));
return nd_rx_call(a,a->api.RxWindowState(&value.window));
case 0x330u: if(a->api.RxWindowFault==NULL)return NET_HOLD;
ND_TRY(nd_ingress_once(a,NET_WIRE_CAN,m->id,d->source_boot,d->capture.port_epoch,d->source_sequence));
return nd_rx_call(a,a->api.RxWindowFault(&value.window_fault.fault));
default:return NET_UNSUPPORTED;
}
}
static bool nd_same(uint16_t id,const void *object,const NdCanValue *mapped){
switch(id){
case 0x200u:{ const DoorState_t *v=object;
const DoorState_t *e=&mapped->door;
return v->compositeQuality==e->compositeQuality&&v->compositeState==e->compositeState&&v->lockQuality==e->lockQuality&&v->lockState==e->lockState&&v->meta.ageMs==e->meta.ageMs&&v->meta.quality==e->meta.quality&&v->meta.updateSequence==e->meta.updateSequence&&v->openQuality==e->openQuality&&v->openState==e->openState&&v->sourceGeneration==e->sourceGeneration;
}
case 0x210u:{ const ClimateState_t *v=object;
const ClimateState_t *e=&mapped->climate;
return v->commandedFanLevel==e->commandedFanLevel&&v->commandedFanQuality==e->commandedFanQuality&&v->heatRemovalQuality==e->heatRemovalQuality&&v->heatRemovalState==e->heatRemovalState&&v->measuredFanLevel==e->measuredFanLevel&&v->measuredFanQuality==e->measuredFanQuality&&v->meta.ageMs==e->meta.ageMs&&v->meta.quality==e->meta.quality&&v->meta.updateSequence==e->meta.updateSequence&&v->thermalDirection==e->thermalDirection&&v->thermalDirectionQuality==e->thermalDirectionQuality&&v->thermalOutputLevelPercent==e->thermalOutputLevelPercent&&v->thermalOutputQuality==e->thermalOutputQuality;
}
case 0x220u:{ const InteriorLightState_t *v=object;
const InteriorLightState_t *e=&mapped->light;
return v->applyQuality==e->applyQuality&&v->applyState==e->applyState&&v->color.blue==e->color.blue&&v->color.green==e->color.green&&v->color.red==e->color.red&&v->colorQuality==e->colorQuality&&v->levelPercent==e->levelPercent&&v->levelQuality==e->levelQuality&&v->meta.ageMs==e->meta.ageMs&&v->meta.quality==e->meta.quality&&v->meta.updateSequence==e->meta.updateSequence&&v->physicalFeedbackSupported==e->physicalFeedbackSupported&&v->type==e->type&&v->typeQuality==e->typeQuality;
}
case 0x230u:{ const DomainIf_BcmStatusReport_t *v=object;
const DomainIf_BcmStatusReport_t *e=&mapped->bcm_status;
return v->activeFault.category==e->activeFault.category&&v->activeFault.faultCode==e->activeFault.faultCode&&v->activeFault.quality==e->activeFault.quality&&v->activeFault.state==e->activeFault.state&&v->affectedFunctionMask==e->affectedFunctionMask&&v->ecuState==e->ecuState&&v->faultMask==e->faultMask&&v->meta.ageMs==e->meta.ageMs&&v->meta.quality==e->meta.quality&&v->meta.updateSequence==e->meta.updateSequence&&v->recovering==e->recovering&&v->sourceGeneration==e->sourceGeneration;
}
case 0x300u:{ const CisCabinEnvironmentState_t *v=object;
const CisCabinEnvironmentState_t *e=&mapped->environment;
return v->cabinHumidity==e->cabinHumidity&&v->cabinIlluminance==e->cabinIlluminance&&v->cabinTemperature==e->cabinTemperature&&v->humidityQuality.meta.ageMs==e->humidityQuality.meta.ageMs&&v->humidityQuality.meta.quality==e->humidityQuality.meta.quality&&v->humidityQuality.meta.updateSequence==e->humidityQuality.meta.updateSequence&&v->humidityQuality.reason==e->humidityQuality.reason&&v->humidityQuality.validity==e->humidityQuality.validity&&v->illuminanceQuality.meta.ageMs==e->illuminanceQuality.meta.ageMs&&v->illuminanceQuality.meta.quality==e->illuminanceQuality.meta.quality&&v->illuminanceQuality.meta.updateSequence==e->illuminanceQuality.meta.updateSequence&&v->illuminanceQuality.reason==e->illuminanceQuality.reason&&v->illuminanceQuality.validity==e->illuminanceQuality.validity&&v->temperatureQuality.meta.ageMs==e->temperatureQuality.meta.ageMs&&v->temperatureQuality.meta.quality==e->temperatureQuality.meta.quality&&v->temperatureQuality.meta.updateSequence==e->temperatureQuality.meta.updateSequence&&v->temperatureQuality.reason==e->temperatureQuality.reason&&v->temperatureQuality.validity==e->temperatureQuality.validity;
}
case 0x310u:{ const CisOccupantState_t *v=object;
const CisOccupantState_t *e=&mapped->occupant;
return v->countQuality.meta.ageMs==e->countQuality.meta.ageMs&&v->countQuality.meta.quality==e->countQuality.meta.quality&&v->countQuality.meta.updateSequence==e->countQuality.meta.updateSequence&&v->countQuality.reason==e->countQuality.reason&&v->countQuality.validity==e->countQuality.validity&&v->occupantCount==e->occupantCount&&v->occupantPresent==e->occupantPresent&&v->presenceQuality.meta.ageMs==e->presenceQuality.meta.ageMs&&v->presenceQuality.meta.quality==e->presenceQuality.meta.quality&&v->presenceQuality.meta.updateSequence==e->presenceQuality.meta.updateSequence&&v->presenceQuality.reason==e->presenceQuality.reason&&v->presenceQuality.validity==e->presenceQuality.validity&&v->sourceGeneration==e->sourceGeneration&&v->visionFunctionStatus==e->visionFunctionStatus;
}
case 0x0a0u:{ const CisRearState_t *v=object;
const CisRearState_t *e=&mapped->rear;
return v->distanceQuality.meta.ageMs==e->distanceQuality.meta.ageMs&&v->distanceQuality.meta.quality==e->distanceQuality.meta.quality&&v->distanceQuality.meta.updateSequence==e->distanceQuality.meta.updateSequence&&v->distanceQuality.reason==e->distanceQuality.reason&&v->distanceQuality.validity==e->distanceQuality.validity&&v->measurementState==e->measurementState&&v->rearDistanceMm==e->rearDistanceMm&&v->sourceGeneration==e->sourceGeneration;
}
case 0x320u:{ const DomainIf_CisStatusReport_t *v=object;
const DomainIf_CisStatusReport_t *e=&mapped->cis_status;
return v->activeFaultCode==e->activeFaultCode&&v->affectedFunctionMask==e->affectedFunctionMask&&v->canInterfaceStatus==e->canInterfaceStatus&&v->ecuState==e->ecuState&&v->faultMask==e->faultMask&&v->humidityFunctionStatus==e->humidityFunctionStatus&&v->illuminanceFunctionStatus==e->illuminanceFunctionStatus&&v->lastFaultCode==e->lastFaultCode&&v->lastFaultOccurrenceId==e->lastFaultOccurrenceId&&v->meta.ageMs==e->meta.ageMs&&v->meta.quality==e->meta.quality&&v->meta.updateSequence==e->meta.updateSequence&&v->occupantFunctionStatus==e->occupantFunctionStatus&&v->piUartStatus==e->piUartStatus&&v->rearFunctionStatus==e->rearFunctionStatus&&v->recovering==e->recovering&&v->sourceGeneration==e->sourceGeneration&&v->temperatureFunctionStatus==e->temperatureFunctionStatus;
}
case 0x340u:{ const DomainIf_VssStateReport_t *v=object;
const DomainIf_VssStateReport_t *e=&mapped->vss;
return v->faultMask==e->faultMask&&v->faultQuality==e->faultQuality&&v->lastFaultAgeMs==e->lastFaultAgeMs&&v->lastFaultCode==e->lastFaultCode&&v->lastFaultOccurrenceId==e->lastFaultOccurrenceId&&v->service.acceptingEvents==e->service.acceptingEvents&&v->service.availability==e->service.availability&&v->service.faultActive==e->service.faultActive&&v->service.meta.ageMs==e->service.meta.ageMs&&v->service.meta.quality==e->service.meta.quality&&v->service.meta.updateSequence==e->service.meta.updateSequence&&v->service.state==e->service.state&&v->sourceGeneration==e->sourceGeneration;
}
case 0x0b0u:{ const WindowState_t *v=object;
const WindowState_t *e=&mapped->window;
return v->antiPinchState==e->antiPinchState&&v->channel==e->channel&&v->ecuState==e->ecuState&&v->fullyClosed==e->fullyClosed&&v->fullyOpen==e->fullyOpen&&v->meta.ageMs==e->meta.ageMs&&v->meta.quality==e->meta.quality&&v->meta.updateSequence==e->meta.updateSequence&&v->motionState==e->motionState&&v->positionPercent==e->positionPercent&&v->sourceGeneration==e->sourceGeneration;
}
case 0x330u:{ const DomainIf_WindowFaultState_t *v=object;
const DomainIf_WindowFaultState_t *e=&mapped->window_fault;
return v->fault.category==e->fault.category&&v->fault.faultCode==e->fault.faultCode&&v->fault.quality==e->fault.quality&&v->fault.state==e->fault.state&&v->meta.ageMs==e->meta.ageMs&&v->meta.quality==e->meta.quality&&v->meta.updateSequence==e->meta.updateSequence&&v->occurrenceId==e->occurrenceId&&v->sourceGeneration==e->sourceGeneration;
}
default:return false;
}
}
NetStatus nd_projection(NetDomainC2 *a,uint16_t id,const void *object,uint64_t now,const NetWireMQueryRaw *request,NetUpperQueryOutput *out){
NetUpperCanMessage raw;
NetUpperDelivery delivery;
NetSourceStamp source;
NdCanValue mapped;
NetUpperQueryOutput value;
uint32_t boot,seq;
uint64_t elapsed;
uint32_t device;
uint64_t session;
if(a==NULL||object==NULL||out==NULL)return NET_INVALID_ARGUMENT;
ND_TRY(net_upper_received_can(a->service,id,&raw,&delivery,&source));
if(now<source.observed_at_ms||!delivery.capture.timestamp_known)return NET_HOLD;
elapsed=now-source.observed_at_ms;
ND_TRY(nd_map_can(a,&raw,elapsed,&mapped));
if(!nd_same(id,object,&mapped))return NET_HOLD;
if(net_upper_can_identity(&raw,&boot,&seq)!=NET_WIRE_OK)return NET_HOLD;
if(a->config.confirm_snapshot==NULL||!a->config.confirm_snapshot(a->config.context,id,delivery.receipt_id,boot,seq))return NET_HOLD;
if(request!=NULL){device=request->device_context_id;
session=request->session_id;
}
else{ND_TRY(nd_context(a,&device,&session));
}
memset(&value,0,sizeof(value));
ND_TRY(net_upper_wire_status(net_upper_uart_project_can(a->service->config.profile,&raw,device,session,a->config.domain_boot,request==NULL?0u:request->query_id,0u,&value.message)));
value.source=source;
value.age_policy=a->config.transmit_policy;
*out=value;
return NET_OK;
}
NetStatus nd_publish_projection(NetDomainC2 *a,uint16_t id,const void *object){
uint64_t now;
NetUpperQueryOutput value;
NetTxKey key;
ND_TRY(net_upper_now(a->service,&now));
ND_TRY(nd_projection(a,id,object,now,NULL,&value));
ND_TRY(net_upper_submit_uart_auto(a->service,&value.message,&value.source,&value.age_policy,&key));
a->last_tx_key=key;
return NET_OK;
}

NetStatus net_domain_c2_window_view(NetDomainC2 *a,const NetUpperCanMessage *m,const NetUpperDelivery *d,NetDomainC2WindowView *out)
{
    NetDomainC2WindowView view;NdCanValue mapped;NetWireCanEnvelope frame;
    if(a==NULL||m==NULL||d==NULL||out==NULL)return NET_INVALID_ARGUMENT;
    if(net_wire_ranges_overlap(out,sizeof(*out),m,sizeof(*m))||net_wire_ranges_overlap(out,sizeof(*out),d,sizeof(*d))||net_wire_ranges_overlap(out,sizeof(*out),a,sizeof(*a)))return NET_INVALID_ARGUMENT;
    if(!d->observation_only||!a->config.window_transport_enabled)return NET_HOLD;
    ND_TRY(net_upper_wire_status(net_upper_can_encode(a->service->config.profile,m,&frame)));
    memset(&view,0,sizeof(view));view.id=m->id;view.original=*m;view.delivery=*d;
    switch(m->id){
    case 0x0b0u:ND_TRY(nd_map_can(a,m,d->local_elapsed_ms,&mapped));view.value.state=mapped.window;break;
    case 0x330u:ND_TRY(nd_map_can(a,m,d->local_elapsed_ms,&mapped));view.value.fault=mapped.window_fault;break;
    case 0x092u:{const NetWireWindowResultRaw *r=&m->data.window_result;
        if(r->request_domain_boot!=a->config.domain_boot||r->result_reason>255u)return NET_HOLD;
        view.value.result.commandId=r->request_sequence;view.value.result.channel=r->target_channel;
        ND_FROM(RequestResult_t,r->result,view.value.result.result);ND_FROM(ResultReason_t,(uint8_t)r->result_reason,view.value.result.reason);
        /* This wire has no result age or observation sequence. */
        view.value.result.meta.quality=DATA_QUALITY_NO_DATA;view.value.result.meta.ageMs=UINT32_MAX;view.value.result.meta.updateSequence=0u;break;}
    case 0x091u:{const NetWireWindowEventRaw *r=&m->data.window_event;
        if(r->event_kind!=3u||a->config.window_event_projection==NULL)return NET_HOLD;
        ND_TRY(a->config.window_event_projection(a->config.context,m,&view.value.antipinch));
        if(view.value.antipinch.occurrenceId!=r->occurrence_id||view.value.antipinch.sourceGeneration!=r->boot_id)return NET_HOLD;
        ND_FROM(DataQuality_t,r->event_quality,view.value.antipinch.meta.quality);
        ND_TRY(nd_age(r->event_age_ms,d->local_elapsed_ms,&view.value.antipinch.meta.ageMs));
        break;}
    default:return NET_UNSUPPORTED;
    }
    *out=view;return NET_OK;
}
