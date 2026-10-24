#ifndef NET_DOMAIN_PRIVATE_H
#define NET_DOMAIN_PRIVATE_H
#include "network/net_domain_c2.h"
#include "net_domain_enum_internal.h"
#include <string.h>
#define ND_TRY(x) do { NetStatus nd_s=(x); if(nd_s!=NET_OK)return nd_s; } while(0)
#define ND_FROM(t,x,y) do { if(!nd_from_##t((x),&(y)))return NET_HOLD; } while(0)
#define ND_TO(t,x,y) do { if(!nd_to_##t((x),&(y)))return NET_HOLD; } while(0)
typedef union {
 DoorState_t door; ClimateState_t climate; InteriorLightState_t light;
 DomainIf_BcmStatusReport_t bcm_status; DomainIf_BcmCommandResult_t bcm_result;
 DomainIf_BcmProtectionEvent_t bcm_event; CisCabinEnvironmentState_t environment;
 CisOccupantState_t occupant; CisRearState_t rear; DomainIf_CisStatusReport_t cis_status;
 DomainIf_VssStateReport_t vss; WindowState_t window; DomainIf_WindowFaultState_t window_fault;
 DomainIf_WindowCommandResult_t window_result; DomainIf_AntiPinchReport_t antipinch;
} NdCanValue;
NetStatus nd_age(uint16_t, uint64_t, uint32_t *);
NetStatus nd_value(NetDomainC2 *,uint8_t,uint8_t,uint32_t,uint16_t,uint64_t,ValueQuality_t *);
NetStatus nd_map_can(NetDomainC2 *,const NetUpperCanMessage *,uint64_t,NdCanValue *);
NetStatus nd_projection(NetDomainC2 *,uint16_t,const void *,uint64_t,const NetWireMQueryRaw *,NetUpperQueryOutput *);
NetStatus nd_publish_projection(NetDomainC2 *,uint16_t,const void *);
NetStatus nd_send_can(NetDomainC2 *,NetUpperCanMessage *,const NetworkLocalFailure_t *);
NetStatus nd_send_uart(NetDomainC2 *,NetUpperUartMessage *,const NetworkLocalFailure_t *);
NetStatus nd_core_message(NetDomainC2 *,uint8_t,const void *,NetUpperUartMessage *);
NetStatus nd_context(NetDomainC2 *,uint32_t *,uint64_t *);
NetDomainC2 *nd_registered(void);
NetStatus nd_call(NetDomainC2 *,DomainIf_Status_t);
bool nd_ready(const NetDomainC2 *);
NetStatus nd_rx_can(void *,const NetUpperCanMessage *,const NetUpperDelivery *);
NetStatus nd_rx_uart(void *,const NetUpperUartDelivery *,const NetUpperDelivery *);
NetStatus nd_ingress_once(NetDomainC2 *,NetWireLink,uint16_t,uint32_t,uint64_t,uint64_t);
NetStatus nd_ingress_new_context(NetDomainC2 *,uint64_t,uint64_t);
void nd_ingress_context_taken(NetDomainC2 *);
NetStatus nd_barrier(NetDomainC2 *,NetDomainC2BarrierKind,uint32_t);
NetStatus nd_rx_call(NetDomainC2 *,DomainIf_Status_t);
/* Synthesized CAN UPDATE_SEQUENCE of DOMAIN_PERMISSION after last (0 = none
 * sent yet): 1, 2, ... 0xFFFFFFFF, then 1 again; 0 is never used. */
uint32_t nd_permission_next(uint32_t last);
#endif
