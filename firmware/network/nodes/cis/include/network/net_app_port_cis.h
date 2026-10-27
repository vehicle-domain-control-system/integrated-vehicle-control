#ifndef NETWORK_NET_APP_PORT_CIS_H
#define NETWORK_NET_APP_PORT_CIS_H

/* L02/M03 CIS Application Port facade (Network-internal binding).
 * Implements docs/network/interface/02_NODES/CIS/20_API.md on top of the
 * typed ECU adapter (RX: DOMAIN_PERMISSION on CAN, P_VISION_RESULT /
 * P_VISION_STATUS on UART-C) and the M04 publish slots (TX: CIS_REAR,
 * CIS_ENVIRONMENT, CIS_OCCUPANT, CIS_STATUS on CAN, P_PERMISSION on UART-C).
 * The application only sees network/app/net_if_cis.h.
 *
 * CisNet_PublishOccupant: a CIS_OCCUPANT relays the P_VISION_RESULT accepted
 * last (02_NODES/CIS/20_API.md, 10_DATA.md Pi original observation and CIS
 * evaluation): the same Pi BOOT, VISION_SEQUENCE, presence and count, the AGE
 * counted from that receipt, and each field's validity / reason either the
 * Pi's or a CIS restriction of it (INVALID with the CIS reason); never a
 * promotion. vision_function_status is the CIS's own function state.
 * Returns: NOT_READY before any Pi judgement is accepted; INVALID_ARGUMENT for
 * a record that can never be that relay (another identity, another value, a
 * promoted validity, an AGE below the Pi AGE of that judgement's first
 * receipt), so a resubmission cannot succeed; nothing is sent then.
 * vision_age_ms may be the AGE of the first delivery of the judgement, with
 * or without the application's own hold time; the AGE sent is the latest
 * receipt's Pi AGE (repetitions included) plus the time since it, counted by
 * Network once.
 *
 * P_PERMISSION: CisNetPriv_RelayPiPermission() takes the DOMAIN_PERMISSION the
 * dispatcher just delivered to CisApp_OnDomainPermission() and, for a relay
 * event (new original evaluation, accepted change of the DOMAIN run, actual
 * change of the power quality), submits the relay record to the M04 slot:
 * the DOMAIN power value, quality and AGE as received, the fixed reason of
 * that quality (net_upper_pi_permission_reason), CIS_BOOT_ID and USE_LIMIT;
 * M04 gives the relay revision (UPDATE_SEQUENCE). Network does not repeat the
 * record (period 0). A repetition of the same DOMAIN evaluation with a new
 * TX_SEQUENCE reaches CisApp_OnDomainPermission() but is not relayed again;
 * the first frame of a new DOMAIN run is (30_UART_C.md P_PERMISSION). */
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
    uint32_t permission_repeats; /* of permission_delivered: repetitions of one evaluation, not relayed */
    bool permission_seen;
    NetDomainPermission last_permission;
    NetCanRxMeta last_permission_meta;
    uint32_t relay_submitted;  /* relay records handed to the P_PERMISSION slot */
    uint32_t relay_refused;    /* relay calls that did not return OK */
    uint32_t relay_unchanged;  /* relay calls with no relay event (the last record stands) */
    NetSubmitResult last_relay_result;
    NetStatus last_relay_engine;
    NetAppPortCisPublishDiag publish[NET_APP_PORT_CIS_PUBLISH_COUNT];
    uint32_t occupant_not_current; /* occupant records refused: no Pi judgement yet, or not its relay */
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
