#ifndef NETWORK_NET_APP_PORT_VSS_H
#define NETWORK_NET_APP_PORT_VSS_H

/* L02/M03 VSS Application Port facade (Network-internal binding).
 * Implements docs/network/interface/02_NODES/VSS/20_API.md on top of the
 * typed ECU adapter. The application only sees network/app/net_if_vss.h. */
#include "network/net_ecu_adapter.h"
#include "network/net_app_port.h"
#include "network/app/net_if_vss.h"

typedef struct {
    /* Network-owned CAN BOOT_ID for this power cycle. 0 is reserved and
     * rejected. Must differ from the previous boot (see VSS_NODE_INTEGRATION.md). */
    uint32_t boot_id;
    /* Freshness bound for VSS_STATUS: current-evaluation source_age_ms plus
     * local queue wait. History (last_fault_age_ms) is never a freshness bound. */
    NetAgePolicy status_tx_age;
} NetAppPortVssConfig;

typedef struct {
    bool bound;
    uint32_t boot_id, next_status_sequence;
    uint32_t status_submitted, status_replaced, status_not_accepted, status_deferred;
    uint32_t event_delivered, warning_delivered, permission_received;
    bool permission_seen;
    NetDomainPermission last_permission;
    NetCanRxMeta last_permission_meta;
    NetSubmitResult last_status_result;
    NetStatus last_status_engine;
} NetAppPortVssDiag;

/* RX typed callback table to pass to net_ecu_adapter_init() for the VSS node:
 * DOMAIN_PERMISSION, VSS_EVENT, VSS_WARNING_STATE. Others stay NULL (fail closed). */
const NetEcuCallbacks *net_app_port_vss_callbacks(void);

/* Bind after net_ecu_adapter_init(); RX before binding is rejected (HOLD). */
NetStatus net_app_port_vss_bind(NetUpper *service, NetEcuAdapter *ecu,
                                const NetAppPortVssConfig *config);

/* Superloop housekeeping after net_upper_step(): retire finished TX slots. */
void net_app_port_vss_service(void);

NetStatus net_app_port_vss_diag(NetAppPortVssDiag *out);

/* Network-private DOMAIN_PERMISSION supervision entry (20_API.md). Not an
 * App callback and never converted to a VSS recovery permission. */
void VssNetPriv_OnDomainPermission(const NetDomainPermission *message, const NetCanRxMeta *meta);

#endif
