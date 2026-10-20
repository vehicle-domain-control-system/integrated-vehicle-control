#include "net_domain_private.h"
static NetStatus passive(void *context,uint32_t device,uint64_t session){NetDomainC2 *a=context;
if(!nd_ready(a))return NET_NOT_CONFIGURED;
if(a->api.ValidatePassiveQueryContext==NULL)return NET_HOLD;
return nd_call(a,a->api.ValidatePassiveQueryContext(device,session));
}
static NetStatus plan(void *context,const NetWireMQueryRaw *request,NetUpperQueryPlan *out){NetDomainC2 *a=context;
if(!nd_ready(a)||out==NULL||request==NULL)return NET_INVALID_ARGUMENT;
if(a->config.select_query_plan==NULL)return NET_HOLD;
return a->config.select_query_plan(a->config.context,request,out);
}
static NetStatus output_core(NetDomainC2 *a,const NetWireMQueryRaw *q,uint8_t type,const void *value,uint64_t now,NetUpperQueryOutput *out){NetUpperQueryOutput output;
memset(&output,0,sizeof(output));
ND_TRY(nd_core_message(a,type,value,&output.message));
ND_TRY(net_upper_wire_status(net_upper_uart_set_query_context(&output.message,q->device_context_id,q->session_id,a->config.domain_boot,q->query_id)));
output.source.observation_id=q->query_id;
output.source.observed_at_ms=now;
output.source.original_age_ms=net_upper_uart_max_age(&output.message);
output.age_policy=a->config.transmit_policy;
*out=output;
return NET_OK;
}
#define QHEAD(name) static NetStatus name(void *context,const NetWireMQueryRaw *request,uint8_t selector,uint64_t now,NetUpperQueryOutput *outputs,size_t capacity,size_t *count)
#define QCHECK() NetDomainC2 *a=context;DomainIf_Status_t ds;(void)selector;if(a==NULL||request==NULL||outputs==NULL||count==NULL||capacity==0u)return NET_INVALID_ARGUMENT;if(!nd_ready(a))return NET_NOT_CONFIGURED;if(now>UINT32_MAX)return NET_HOLD
#define QCAN(name,ctype,fn,id) QHEAD(name){ctype value;NetUpperQueryOutput result;QCHECK();if(a->api.fn==NULL)return NET_HOLD;ds=a->api.fn((uint32_t)now,&value);if(ds!=DOMAIN_IF_OK)return nd_call(a,ds);ND_TRY(nd_projection(a,id,&value,now,request,&result));outputs[0]=result;*count=1u;return NET_OK;}
QCAN(q_door,DoorState_t,QueryGetBcmDoorState,0x200u)
QCAN(q_climate,ClimateState_t,QueryGetBcmClimateState,0x210u)
QCAN(q_light,InteriorLightState_t,QueryGetBcmInteriorLightState,0x220u)
QCAN(q_bcm,DomainIf_BcmStatusReport_t,QueryGetBcmStatus,0x230u)
QCAN(q_environment,CisCabinEnvironmentState_t,QueryGetCisEnvironment,0x300u)
QCAN(q_occupant,CisOccupantState_t,QueryGetCisOccupant,0x310u)
QCAN(q_rear,CisRearState_t,QueryGetCisRear,0x0a0u)
QCAN(q_cis,DomainIf_CisStatusReport_t,QueryGetCisStatus,0x320u)
QCAN(q_vss,DomainIf_VssStateReport_t,QueryGetVssStatus,0x340u)
QCAN(q_window,WindowState_t,QueryGetWindowState,0x0b0u)
QCAN(q_window_fault,DomainIf_WindowFaultState_t,QueryGetWindowFault,0x330u)
#define QCORE(name,ctype,fn,type) QHEAD(name){ctype value;NetUpperQueryOutput result;QCHECK();if(a->api.fn==NULL)return NET_HOLD;ds=a->api.fn((uint32_t)now,&value);if(ds!=DOMAIN_IF_OK)return nd_call(a,ds);ND_TRY(output_core(a,request,type,&value,now,&result));outputs[0]=result;*count=1u;return NET_OK;}
QCORE(q_settings,DomainIf_UserSettingsState_t,QueryGetUserSettings,0x25u)
QCORE(q_availability,DomainIf_FunctionAvailabilitySet_t,QueryGetFunctionAvailability,0x22u)
QCORE(q_digital,DigitalKeyState_t,QueryGetDigitalKeyState,0x23u)
QHEAD(q_warning){DomainIf_WarningInfo_t value;
NetUpperQueryOutput result;
DomainWarningType_t type;
QCHECK();
if(a->api.QueryGetCurrentWarning==NULL)return NET_HOLD;
ND_FROM(DomainWarningType_t,selector,type);
ds=a->api.QueryGetCurrentWarning(type,(uint32_t)now,&value);
if(ds!=DOMAIN_IF_OK)return nd_call(a,ds);
ND_TRY(output_core(a,request,0x21u,&value,now,&result));
outputs[0]=result;
*count=1u;
return NET_OK;
}
QHEAD(q_result){DomainIf_MobileRequestResult_t value;
NetUpperQueryOutput result;
bool has;
RequestContext_t key;
QCHECK();
if(a->api.QueryGetRequestResult==NULL)return NET_HOLD;
key.sessionId=request->request_session;
key.requestId=request->request_id;
ds=a->api.QueryGetRequestResult(request->device_context_id,&key,(uint32_t)now,&value,&has);
if(ds!=DOMAIN_IF_OK)return nd_call(a,ds);
if(!has){*count=0u;
return NET_OK;
}
ND_TRY(output_core(a,request,0x20u,&value,now,&result));
outputs[0]=result;
*count=1u;
return NET_OK;
}
QHEAD(q_recent){DomainIf_MobileRequestResult_t values[NET_UPPER_QUERY_RECENT_CAPACITY];
NetUpperQueryOutput results[NET_UPPER_QUERY_RECENT_CAPACITY];
size_t n,i;
QCHECK();
if(a->api.QueryGetRecentRequestResults==NULL)return NET_HOLD;
if(capacity>NET_UPPER_QUERY_RECENT_CAPACITY)return NET_INVALID_ARGUMENT;
ds=a->api.QueryGetRecentRequestResults(request->device_context_id,(uint32_t)now,values,capacity,&n);
if(ds!=DOMAIN_IF_OK)return nd_call(a,ds);
if(n>capacity)return NET_REJECTED;
for(i=0u;i<n;++i){ND_TRY(output_core(a,request,0x20u,&values[i],now,&results[i]));
}
for(i=0u;i<n;++i)outputs[i]=results[i];
*count=n;
return NET_OK;
}
QHEAD(q_digital_result){DomainIf_DigitalKeyResult_t value;
NetUpperQueryOutput result;
bool has;
QCHECK();
if(a->api.QueryGetLastDigitalKeyResult==NULL)return NET_HOLD;
ds=a->api.QueryGetLastDigitalKeyResult(request->device_context_id,(uint32_t)now,&value,&has);
if(ds!=DOMAIN_IF_OK)return nd_call(a,ds);
if(!has){*count=0u;
return NET_OK;
}
ND_TRY(output_core(a,request,0x24u,&value,now,&result));
outputs[0]=result;
*count=1u;
return NET_OK;
}
NetUpperQueryOwner net_domain_c2_query_owner(NetDomainC2 *a){NetUpperQueryOwner owner;
static const NetUpperQueryGet getters[NET_UPPER_QUERY_GETTERS]={q_door,q_climate,q_light,q_bcm,q_environment,q_occupant,q_rear,q_cis,q_vss,q_window,q_window_fault,q_settings,q_availability,q_digital,q_warning,q_result,q_recent,q_digital_result};
memset(&owner,0,sizeof(owner));
owner.context=a;
owner.validate_passive=passive;
owner.select_plan=plan;
memcpy(owner.getters,getters,sizeof(getters));
return owner;
}
