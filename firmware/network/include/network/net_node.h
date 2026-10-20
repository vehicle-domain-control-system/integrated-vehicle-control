#ifndef NETWORK_NET_NODE_H
#define NETWORK_NET_NODE_H

/* Network node composition root (not a layer). Wires, for the node profile
 * selected at compile time (NETWORK_UPPER_NODE_PROFILE):
 *   App Port facade (L02/M03) -> typed ECU adapter (L02/M03) -> NetUpper
 *   (L01/L03/L04/L05) -> NetHalAdapter (L05/M08) -> RTD backend (platform).
 * Currently wired profile: VSS (6). Other profiles fail at compile time.
 *
 * Superloop use (main context only, never from an ISR):
 *     Clock/Port/IntCtrl/OsIf init ... ;
 *     NetNode_Init(&cfg);                 // starts FlexCAN
 *     for (;;) { NetNode_Step(); App_Service(); }
 * VssApp_On* callbacks run inside NetNode_Step(). */
#include "network/net_upper.h"
#include "network/net_upper_monitor.h"
#include "network/net_hal_adapter.h"
#include "network/net_hal_rtd.h"
#include "network/net_app_port_vss.h"

typedef struct {
    /* Network-owned CAN BOOT_ID for this power cycle (0 reserved). Must change
     * on every reboot, otherwise receivers treat the restarted TX_SEQUENCE as
     * old/duplicate. Source (NVM counter, noinit RAM counter...) is the
     * integrator's choice; see VSS_NODE_INTEGRATION.md. */
    uint32_t boot_id;
    /* Local TX key epoch (nonzero). Any value; 1 is fine. */
    uint64_t local_epoch;
    /* CAN port epoch (nonzero). Any value; 1 is fine. */
    uint64_t can_port_epoch;
} NetNodeConfig;

typedef enum {
    NET_NODE_STAGE_NONE = 0, NET_NODE_STAGE_HAL_CONFIG, NET_NODE_STAGE_HAL_ADAPTER,
    NET_NODE_STAGE_ECU_ADAPTER, NET_NODE_STAGE_UPPER, NET_NODE_STAGE_APP_PORT,
    NET_NODE_STAGE_PERMISSION, NET_NODE_STAGE_HAL_START, NET_NODE_STAGE_READY
} NetNodeStage;

typedef struct {
    NetNodeStage stage;           /* last stage reached; READY when running */
    NetStatus stage_status;       /* status of the failing stage */
    uint32_t steps;
    NetUpperObservation upper;    /* M09 raw counters */
    NetHalObservation hal;        /* M08 adapter counters */
    NetHalRtdDiag rtd;            /* RTD backend counters / init failure */
    NetAppPortVssDiag app;        /* App Port counters, last permission */
    bool permission_valid;
    NetPermissionView permission; /* DOMAIN_PERMISSION supervision view */
} NetNodeDiag;

/* Once per power cycle. After a call that got past argument checks, later
 * calls return NET_BUSY and leave the node (running or failed) untouched. */
NetStatus NetNode_Init(const NetNodeConfig *config);
void NetNode_Step(void);
NetStatus NetNode_GetDiag(NetNodeDiag *out);

#endif
