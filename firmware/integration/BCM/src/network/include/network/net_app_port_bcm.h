#ifndef NETWORK_NET_APP_PORT_BCM_H
#define NETWORK_NET_APP_PORT_BCM_H

/* L02/M03 BCM Application Port facade (Network-internal binding).
 * Implements docs/network/interface/02_NODES/BCM/20_API.md on top of the
 * typed ECU adapter (RX: DOMAIN_PERMISSION, BCM_DOOR_COMMAND,
 * BCM_CLIMATE_COMMAND, BCM_LIGHT_COMMAND on CAN) and the M04 publish slots
 * (TX: BCM_DOOR_STATE, BCM_CLIMATE_STATE, BCM_LIGHT_STATE, BCM_STATUS as
 * current values; BCM_RESULT, BCM_EVENT as discrete records through a bounded
 * store in front of their slot, net_app_port.h). The application only sees
 * network/app/net_if_bcm.h.
 *
 * Commands: delivered once per original command (DOMAIN boot, target boot,
 * command id; a retransmission of the same command is not delivered again)
 * and only when TARGET_BOOT_ID is this node's BOOT_ID (20_API.md: Network
 * checks the DOMAIN / BCM generation before delivering a target). A command
 * for another BCM boot is refused below the application (counted). The
 * acceptance limit (ACCEPT_LIMIT, 200 ms vs 1 s still open) is not applied by
 * the Network: command_age_ms and accept_limit_ms reach the application.
 *
 * DOMAIN_PERMISSION: BcmNetPriv_OnDomainPermission() keeps the supervision
 * record; receive timeout and original age are judged by the node's
 * permission supervision. No public BcmApp permission callback exists. */
#include "network/net_ecu_adapter.h"
#include "network/net_app_port.h"
#include "network/net_upper_publish.h"
#include "network/app/net_if_bcm.h"

typedef struct {
    /* M04 publish slots of this node, initialized by the composition root
     * with the BCM entries (CAN 0x140, 0x141, 0x200, 0x210, 0x220, 0x230). */
    NetUpperPublisher *publisher;
    uint32_t bcm_boot_id;      /* this node's Network BOOT_ID (command TARGET_BOOT_ID) */
} NetAppPortBcmConfig;

/* Index of the six BcmNet_* functions in the per-message counters. */
typedef enum {
    NET_APP_PORT_BCM_RESULT = 0,
    NET_APP_PORT_BCM_EVENT,
    NET_APP_PORT_BCM_DOOR_STATE,
    NET_APP_PORT_BCM_CLIMATE_STATE,
    NET_APP_PORT_BCM_LIGHT_STATE,
    NET_APP_PORT_BCM_STATUS,
    NET_APP_PORT_BCM_PUBLISH_COUNT
} NetAppPortBcmPublish;

typedef struct {
    uint32_t not_accepted;     /* calls that did not return OK */
    uint32_t deferred;         /* OK inside an RX callback: offered after that step */
    NetSubmitResult last_result;
    NetStatus last_engine;
} NetAppPortBcmPublishDiag;

typedef struct {
    bool bound;
    uint32_t door_delivered, climate_delivered, light_delivered;
    uint32_t command_wrong_target; /* commands refused: TARGET_BOOT_ID is not this boot */
    uint32_t permission_received;
    bool permission_seen;
    NetDomainPermission last_permission;
    NetCanRxMeta last_permission_meta;
    NetAppPortBcmPublishDiag publish[NET_APP_PORT_BCM_PUBLISH_COUNT];
    /* Discrete stores of BCM_RESULT and BCM_EVENT (depth, refusals). */
    NetAppPortDiscreteQueue result_store, event_store;
} NetAppPortBcmDiag;

/* RX typed callback table to pass to net_ecu_adapter_init() for the BCM node:
 * DOMAIN_PERMISSION and the three commands. Others stay NULL. */
const NetEcuCallbacks *net_app_port_bcm_callbacks(void);

/* Bind after net_ecu_adapter_init() and net_upper_publish_init(); RX before
 * binding is rejected (HOLD). The publisher must serve the same service and
 * hold every BCM entry. */
NetStatus net_app_port_bcm_bind(NetUpper *service, NetEcuAdapter *ecu,
                                const NetAppPortBcmConfig *config);

/* Once per pass, outside the service step, before net_upper_publish_service():
 * hands waiting BCM_RESULT / BCM_EVENT records to their idle M04 slot. */
void net_app_port_bcm_service(void);

NetStatus net_app_port_bcm_diag(NetAppPortBcmDiag *out);

#endif
