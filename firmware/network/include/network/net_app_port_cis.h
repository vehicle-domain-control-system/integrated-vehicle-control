#ifndef NETWORK_NET_APP_PORT_CIS_H
#define NETWORK_NET_APP_PORT_CIS_H

/* L02/M03 CIS Application Port facade (Network-internal binding).
 * Implements docs/network/interface/02_NODES/CIS/20_API.md on top of the
 * typed ECU adapter (RX: DOMAIN_PERMISSION on CAN, P_VISION_RESULT /
 * P_VISION_STATUS on UART-C) and the M04 publish slots (TX: CIS_REAR,
 * CIS_ENVIRONMENT, CIS_OCCUPANT, CIS_STATUS on CAN, P_PERMISSION on UART-C).
 * The application only sees network/app/net_if_cis.h.
 *
 * CisNet_PublishOccupant: the service accepts a CIS_OCCUPANT only as the
 * relay of the P_VISION_RESULT it accepted last (same Pi BOOT, VISION_SEQUENCE,
 * values, validities, reasons and a vision function state equal to the Pi
 * VISION_STATUS code, AGE counted from that receipt). A record that is not
 * that relay, for example a CIS-restricted validity, returns NOT_READY and
 * nothing is sent. vision_age_ms may already include the application's own
 * hold time (it must not be below the Pi AGE); Network counts the time since
 * the receipt itself, once.
 *
 * P_PERMISSION: CisNetPriv_RelayPiPermission() takes the DOMAIN_PERMISSION the
 * dispatcher just delivered to CisApp_OnDomainPermission() and submits the
 * relay record to the M04 slot (repetition period from the node table). Only
 * OK power quality is relayed: the wire has no reason for the other
 * qualities, so they return NOT_READY and the slot keeps nothing new. */
#include "network/net_ecu_adapter.h"
#include "network/net_app_port.h"
#include "network/net_upper_publish.h"
#include "network/app/net_if_cis.h"

typedef struct {
    /* M04 publish slots of this node, initialized by the composition root
     * with the CIS entries (CAN 0x0A0, 0x300, 0x310, 0x320, UART-C 0x62). M04
     * owns BOOT_ID / TX_SEQUENCE / LINK_SEQUENCE, the pending replacement,
     * the waiting AGE and the repetition. */
    NetUpperPublisher *publisher;
    uint32_t cis_boot_id;      /* P_PERMISSION CIS_BOOT_ID: this node's Network BOOT_ID */
    uint16_t pi_use_limit_ms;  /* P_PERMISSION USE_LIMIT, nonzero (draft 1500 ms) */
} NetAppPortCisConfig;

/* Index of the four CisNet_Publish* functions in the per-message counters. */
typedef enum {
    NET_APP_PORT_CIS_REAR = 0,
    NET_APP_PORT_CIS_ENVIRONMENT,
    NET_APP_PORT_CIS_OCCUPANT,
    NET_APP_PORT_CIS_STATUS,
    NET_APP_PORT_CIS_PUBLISH_COUNT
} NetAppPortCisPublish;

typedef struct {
    uint32_t not_accepted;     /* calls that did not return OK */
    uint32_t deferred;         /* OK inside an RX callback: sent after that step */
    NetSubmitResult last_result;
    NetStatus last_engine;
} NetAppPortCisPublishDiag;

typedef struct {
    bool bound;
    uint32_t permission_delivered, vision_result_delivered, vision_status_delivered;
    bool permission_seen;
    NetDomainPermission last_permission;
    NetCanRxMeta last_permission_meta;
    uint32_t relay_submitted;  /* relay records handed to the P_PERMISSION slot */
    uint32_t relay_refused;    /* relay calls that did not return OK */
    NetSubmitResult last_relay_result;
    NetStatus last_relay_engine;
    NetAppPortCisPublishDiag publish[NET_APP_PORT_CIS_PUBLISH_COUNT];
    uint32_t occupant_not_current; /* occupant records refused as not the accepted Pi relay */
} NetAppPortCisDiag;

/* RX typed callback table to pass to net_ecu_adapter_init() for the CIS node:
 * DOMAIN_PERMISSION, P_VISION_RESULT, P_VISION_STATUS. Others stay NULL. */
const NetEcuCallbacks *net_app_port_cis_callbacks(void);

/* Bind after net_ecu_adapter_init() and net_upper_publish_init(); RX before
 * binding is rejected (HOLD). The publisher must serve the same service and
 * hold every CIS entry. */
NetStatus net_app_port_cis_bind(NetUpper *service, NetEcuAdapter *ecu,
                                const NetAppPortCisConfig *config);

NetStatus net_app_port_cis_diag(NetAppPortCisDiag *out);

#endif
