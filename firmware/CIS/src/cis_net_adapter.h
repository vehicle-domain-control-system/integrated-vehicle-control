/*
 * Connection between the CIS and the Network stack API
 * (docs/network/interface/02_NODES/CIS/20_API.md). Only built with CIS_USE_NETWORK_API.
 *
 * Sending: cis_net_adapter_run() hands the current values to CisNet_Publish*() at the CAN
 * report periods. The Network stack repeats the last value and adds the transport delay;
 * values keep their own sequence and age, so a repeat is never a new observation.
 * Receiving: CisApp_OnDomainPermission() and CisApp_OnPiVision*() are called by the Network
 * stack in the owner context (never from an interrupt) and only copy the data.
 */
#ifndef CIS_NET_ADAPTER_H
#define CIS_NET_ADAPTER_H

#include <stdint.h>

#define NETA_REAR_PERIOD_MS         50u
#define NETA_OCCUPANT_PERIOD_MS     100u
#define NETA_ENVIRONMENT_PERIOD_MS  200u
#define NETA_STATUS_PERIOD_MS       250u

void cis_net_adapter_init(void);

/* Call after cis_app_run(). A message the stack did not accept is offered again next time. */
void cis_net_adapter_run(uint32_t now_ms);

#endif /* CIS_NET_ADAPTER_H */
