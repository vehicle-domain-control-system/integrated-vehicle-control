#include "net_domain_private.h"
static NetStatus context_rx(NetDomainC2 *a,const NetWireMContext *r,uint64_t elapsed){DigitalKeyConnectionState_t c;
ProximityInput_t p;
ValueQuality_t q;
DomainIf_Status_t first,second;
if(a->api.RxDigitalKeyConnection==NULL||a->api.RxProximity==NULL)return NET_HOLD;
memset(&c,0,sizeof(c));
memset(&p,0,sizeof(p));
ND_FROM(DeviceRegistrationState_t,r->device_registration_state,c.registration);
ND_FROM(BtConnectionState_t,r->bt_connection_state,c.btConnection);
ND_FROM(AppActiveState_t,r->app_active_state,c.appActive);
c.sessionId=r->session_id;
ND_FROM(DataQuality_t,r->registration_connection_quality,c.meta.quality);
c.meta.updateSequence=r->registration_connection_update_sequence;
ND_TRY(nd_age(r->registration_connection_age_ms,elapsed,&c.meta.ageMs));
ND_FROM(DataQuality_t,r->app_quality,c.appMeta.quality);
c.appMeta.updateSequence=r->app_update_sequence;
ND_TRY(nd_age(r->app_age_ms,elapsed,&c.appMeta.ageMs));
ND_FROM(ProximityState_t,r->proximity_state,p.state);
ND_TRY(nd_value(a,r->proximity_validity,r->proximity_reason,r->proximity_update_sequence,r->proximity_age_ms,elapsed,&q));
p.validity=q.validity;
p.reason=q.reason;
p.meta=q.meta;
/* Both shapes validated before either Owner invocation. They are distinct
 * ingress operations; partial return is retained, never blindly replayed. */
first=a->api.RxDigitalKeyConnection(r->device_context_id,&c);
(void)nd_call(a,first);
if(first!=DOMAIN_IF_OK)return nd_rx_call(a,first);
second=a->api.RxProximity(r->device_context_id,&p);
return nd_rx_call(a,second);
}
static NetStatus request_rx(NetDomainC2 *a,const NetWireMRequest *r){DomainIf_MobileRequest_t v;
if(a->api.RxMobileRequest==NULL)return NET_HOLD;
memset(&v,0,sizeof(v));
v.deviceContextId=r->device_context_id;
v.context.sessionId=r->session_id;
v.context.requestId=r->request_id;
/* No source age/quality/sequence exists on M_REQUEST. Explicit unavailable
 * tuple, not fabricated age zero, transport sequence or local arrival age. */
v.meta.quality=DATA_QUALITY_NO_DATA;
v.meta.ageMs=UINT32_MAX;
v.meta.updateSequence=0u;
switch(r->kind){case 1u:v.type=MOBILE_REQUEST_DOOR_LOCK;
ND_FROM(DoorLockTarget_t,r->operation,v.payload.doorTarget);
break;
case 2u:switch(r->operation){case 1u:v.type=MOBILE_REQUEST_CLIMATE_TARGET_TEMPERATURE;
ND_TRY(net_upper_wire_status(net_wire_m_request_temperature(r,&v.payload.targetTemperature)));
break;
case 2u:v.type=MOBILE_REQUEST_CLIMATE_AUTO_ENABLE;
v.payload.enable=r->args[0]==1u;
break;
case 3u:v.type=MOBILE_REQUEST_FAN_LEVEL;
ND_FROM(FanLevel_t,r->args[0],v.payload.fanLevel);
break;
default:return NET_HOLD;
}
break;
case 3u:switch(r->operation){case 1u:v.type=MOBILE_REQUEST_INTERIOR_LIGHT_ENABLE;
v.payload.enable=r->args[0]==1u;
break;
case 2u:v.type=MOBILE_REQUEST_INTERIOR_LIGHT_LEVEL;
v.payload.levelPercent=r->args[0];
break;
case 3u:v.type=MOBILE_REQUEST_INTERIOR_LIGHT_COLOR;
v.payload.color.red=r->args[0];
v.payload.color.green=r->args[1];
v.payload.color.blue=r->args[2];
break;
default:return NET_HOLD;
}
break;
case 4u:v.type=MOBILE_REQUEST_DIGITAL_KEY_SETTING;
ND_FROM(DigitalKeySetting_t,r->args[0],v.payload.digitalKeySetting);
break;
default:return NET_HOLD;
}
return nd_rx_call(a,a->api.RxMobileRequest(&v));
}
/* The service's own test for an M_CONTEXT it takes without its LINK_SEQUENCE
 * window: none seen yet, or another device context or session. Its stored
 * context changes only after the Owner returned. */
static bool new_context(const NetDomainC2 *a,const NetWireMContext *r){
return !a->service->context_seen||a->service->context.device_context_id!=r->device_context_id||a->service->context.session_id!=r->session_id;
}
NetStatus nd_rx_uart(void *context,const NetUpperUartDelivery *d,const NetUpperDelivery *meta){NetDomainC2 *a=context;
const NetUpperUartMessage *m;
NetStatus status;
bool fresh;
if(a==NULL||d==NULL||meta==NULL)return NET_INVALID_ARGUMENT;
if(!nd_ready(a))return NET_NOT_CONFIGURED;
m=&d->message;
if(m->link!=NET_WIRE_UART_M)return NET_UNSUPPORTED;
fresh=m->type==0x01u&&new_context(a,&m->data.m_context);
if(fresh)ND_TRY(nd_ingress_new_context(a,d->port_epoch,m->sequence));
else ND_TRY(nd_ingress_once(a,NET_WIRE_UART_M,m->type,0u,d->port_epoch,m->sequence));
switch(m->type){case 0x01u:status=context_rx(a,&m->data.m_context,meta->local_elapsed_ms);
if(status!=NET_OK)return status;
status=a->query==NULL?NET_OK:net_upper_query_on_context(a->query,d);
if(status==NET_OK&&fresh)nd_ingress_context_taken(a);
return status;
case 0x10u:return request_rx(a,&m->data.m_request);
case 0x11u:return a->query==NULL?NET_HOLD:net_upper_query_begin(a->query,d);
case 0x12u:{DomainIf_WarningAck_t v;
const NetWireMWarningAckRaw *r=&m->data.m_warning_ack;
if(r->warning_domain_boot!=a->config.domain_boot||a->api.RxWarningAck==NULL)return NET_HOLD;
memset(&v,0,sizeof(v));
v.deviceContextId=r->device_context_id;
v.sessionId=r->session_id;
v.occurrenceId=r->occurrence_id;
ND_FROM(DomainWarningType_t,r->warning_type,v.type);
return nd_rx_call(a,a->api.RxWarningAck(&v));
}
default:return NET_UNSUPPORTED;
}
}
