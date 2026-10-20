#ifndef NETWORK_NET_NODE_H
#define NETWORK_NET_NODE_H

/* Network node composition root (not a layer). Wires, for the node profile
 * selected at compile time (NETWORK_UPPER_NODE_PROFILE):
 *   App Port facade (L02/M03) -> typed ECU adapter (L02/M03) -> NetUpper
 *   (L01/L03/L04/L05) -> NetHalAdapter (L05/M08) -> RTD backend (platform).
 * Currently wired profile: VSS (6). Other profiles fail at compile time.
 *
 * Public entry: Net_Init() / Net_MainFunction(nowMs) (network/app/net_if_entry.h,
 * src/node/net_entry.c) wrap NetNode_Init() / NetNode_Step(). NetNode_* stay
 * for internal and bench use (main context only, never from an ISR):
 *     Clock/Port/IntCtrl/OsIf init ... ;
 *     NetNode_Init(&cfg);                 // starts FlexCAN
 *     for (;;) { NetNode_Step(); App_Service(); }
 * VssApp_On* callbacks run inside NetNode_Step(). With the application clock
 * (net_board_cfg.h time base A) Network time only advances through
 * Net_MainFunction(nowMs). */
#include "network/net_upper.h"
#include "network/net_upper_monitor.h"
#include "network/net_hal_adapter.h"
#include "network/net_hal_rtd.h"
#include "network/net_app_port_vss.h"
#include "network/net_upper_publish.h"

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

/* Outcome of the first Net_Init() (NetNodeDiag.entry_init_result). */
typedef enum {
    NET_ENTRY_INIT_NONE = 0,          /* Net_Init() not called yet */
    NET_ENTRY_INIT_OK,                /* NetNode_Init() OK: node READY */
    NET_ENTRY_INIT_NO_CONFIG_HOOK,    /* built without NET_BOARD_NODE_CONFIG_HOOK */
    NET_ENTRY_INIT_CONFIG_REFUSED,    /* the hook returned false */
    NET_ENTRY_INIT_BOOT_ID_ZERO,      /* the hook gave BOOT_ID 0 (reserved) */
    NET_ENTRY_INIT_NODE_FAILED        /* NetNode_Init() failed: stage / stage_status */
} NetEntryInitResult;

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
    NetUpperPublishView status_tx; /* M04 slot of VSS_STATUS: BOOT/TX, waits, repetitions */
    bool permission_valid;
    NetPermissionView permission; /* DOMAIN_PERMISSION supervision view */
    /* Public entry (net_entry.c). ISR ring counters are in rtd. */
    uint8_t entry_init_result;    /* NetEntryInitResult */
    uint32_t entry_init_calls;    /* every Net_Init() call; only the first one acts */
    uint32_t entry_main_calls;    /* every Net_MainFunction() call (also before init) */
    uint32_t entry_now_backward;  /* nowMs behind the previous one: Network time held */
    uint64_t entry_now_ms;        /* last nowMs, 32-bit wrap extended to 64 bits */
    uint32_t main_max_us;         /* longest Net_MainFunction() (NET_BOARD_US_HOOK), else 0 */
} NetNodeDiag;

/* Once per power cycle. After a call that got past argument checks, later
 * calls return NET_BUSY and leave the node (running or failed) untouched. */
NetStatus NetNode_Init(const NetNodeConfig *config);
/* One main pass: ISR ring -> adapter (bounded), RX dispatch -> VssApp_*,
 * permission supervision, TX supervision, M04 publish service. */
void NetNode_Step(void);
NetStatus NetNode_GetDiag(NetNodeDiag *out);

/* Composition-internal (net_entry.c): fills the entry_* and main_max_us fields. */
void net_entry_diag(NetNodeDiag *out);

#endif
