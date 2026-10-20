#ifndef NET_BENCH_VSS_BENCH_APP_H
#define NET_BENCH_VSS_BENCH_APP_H

/* S32K344 bench stand-in for the VSS application (not Network product code).
 * Built only when the board build selects the VSS node profile
 * (NETWORK_UPPER_NODE_PROFILE=6).
 *
 * - VssApp_OnDomainEvent / VssApp_OnDomainWarningState record what Network
 *   delivers in g_vss_bench (watch it in the debugger).
 * - VssBench_Service() submits VSS_STATUS the way the VSS application does:
 *   a new evaluation right after start and every 250 ms, the waiting one
 *   again on the next pass after NOT_READY / NO_CAPACITY (a new evaluation
 *   replaces it), INVALID_ARGUMENT dropped.
 * - g_net_diag is NetNode_GetDiag(), refreshed every 100 ms. */
#include <stdint.h>
#include "network/net_node.h"

#if NETWORK_UPPER_NODE_PROFILE == NET_UPPER_PROFILE_VSS
#include "network/app/net_if_vss.h"

#define VSS_BENCH_STATUS_PERIOD_MS 250u
#define VSS_BENCH_DIAG_PERIOD_MS 100u

typedef struct {
    /* Received from Network (inside Net_MainFunction). */
    uint32_t event_count;
    NetVssEvent last_event;
    NetCanRxMeta last_event_meta;
    uint32_t warning_count;
    NetVssWarningState last_warning;
    NetCanRxMeta last_warning_meta;
    /* VSS_STATUS submissions. */
    uint32_t status_built;          /* new evaluations (UPDATE_SEQUENCE values used) */
    uint32_t status_replaced;       /* a new evaluation replaced a waiting one */
    uint32_t status_resubmitted;    /* waiting evaluation offered again */
    uint32_t status_result[4];      /* by NetSubmitResult: OK, INVALID_ARGUMENT, NOT_READY, NO_CAPACITY */
    uint32_t status_dropped;        /* INVALID_ARGUMENT: not offered again */
    uint32_t status_waiting;        /* 1 while a refused evaluation waits */
    uint32_t last_update_sequence;  /* last submitted evaluation */
    uint8_t last_result;            /* NetSubmitResult of the last submission */
    uint32_t diag_refreshes;
} VssBenchDiag;

extern volatile VssBenchDiag g_vss_bench;
extern volatile NetNodeDiag g_net_diag;

/* Main context, once, right after Net_Init(): records the init result. */
void VssBench_Init(void);
/* Main context, once per superloop pass, after Net_MainFunction(). */
void VssBench_Service(void);
#endif

#endif
