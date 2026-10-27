#ifndef NETWORK_NET_APP_PORT_VSS_H
#define NETWORK_NET_APP_PORT_VSS_H

/* L02/M03 VSS Application Port facade (Network-internal binding).
 * Implements docs/network/interface/02_NODES/VSS/20_API.md on top of the
 * typed ECU adapter (RX) and the M04 publish slots (TX). The application only
 * sees network/app/net_if_vss.h. */
#include "network/net_ecu_adapter.h"
#include "network/net_app_port.h"
#include "network/net_upper_publish.h"
#include "network/app/net_if_vss.h"

typedef struct {
    /* M04 publish slots of this node, initialized by the composition root
     * with a VSS_STATUS (CAN 0x340) entry. M04 owns the BOOT_ID, the
     * TX_SEQUENCE, the pending replacement and the waiting AGE; this facade
     * only hands over the latest NetVssStatus copy. */
    NetUpperPublisher *publisher;
} NetAppPortVssConfig;

typedef struct {
    bool bound;
    uint32_t boot_id, next_status_sequence;              /* from the M04 slot */
    uint32_t status_submitted, status_replaced;          /* from the M04 slot: offered / replaced */
    uint32_t status_not_accepted, status_deferred;       /* facade: refused / called inside a step */
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

/* Bind after net_ecu_adapter_init() and net_upper_publish_init(); RX before
 * binding is rejected (HOLD). The publisher must serve the same service and
 * have the VSS_STATUS entry. */
NetStatus net_app_port_vss_bind(NetUpper *service, NetEcuAdapter *ecu,
                                const NetAppPortVssConfig *config);

NetStatus net_app_port_vss_diag(NetAppPortVssDiag *out);

/* Network-private DOMAIN_PERMISSION supervision entry (20_API.md). Not an
 * App callback and never converted to a VSS recovery permission. */
void VssNetPriv_OnDomainPermission(const NetDomainPermission *message, const NetCanRxMeta *meta);

#endif
