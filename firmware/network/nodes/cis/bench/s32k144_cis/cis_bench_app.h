#ifndef NET_BENCH_CIS_BENCH_APP_H
#define NET_BENCH_CIS_BENCH_APP_H

/* S32K144 bench stand-in for the CIS application (not Network product code).
 * Built only when the board build selects the CIS node profile
 * (NETWORK_UPPER_NODE_PROFILE=1).
 *
 * - CisApp_OnDomainPermission / OnPiVisionResult / OnPiVisionStatus count what
 *   Network delivers and keep the last record with its meta (g_cis_bench).
 * - Every P_VISION_RESULT delivered is relayed at once as CIS_OCCUPANT:
 *   CisNet_PublishOccupant inside the callback (sent in the same pass) with
 *   the Pi BOOT, VISION_SEQUENCE, AGE, values, validity and reasons as
 *   received, and the CIS function state ACTIVE.
 * - CisBench_Service() submits CIS_REAR (50 ms), CIS_ENVIRONMENT (200 ms) and
 *   CIS_STATUS (250 ms), right after start and then once per period. Every
 *   submission is a new evaluation: sequence + 1, AGE 0, every value VALID /
 *   reason NONE. A refused one is counted, not offered again.
 * - g_net_diag is NetNode_GetDiag(), refreshed every 100 ms. The DOMAIN_PERMISSION
 *   supervision reason (NetNodeDiag.permission.reason) is sampled every 10 ms
 *   and each change is counted with its time, so a short state (RX_TIMEOUT
 *   lasts from 150 ms of silence until the original AGE reaches 500 ms, then
 *   SOURCE_EXPIRED) is still seen when the board is suspended later. */
#include <stdint.h>
#include "network/net_node.h"

#if NETWORK_UPPER_NODE_PROFILE == NET_UPPER_PROFILE_CIS
#include "network/app/net_if_cis.h"

#define CIS_BENCH_REAR_PERIOD_MS 50u
#define CIS_BENCH_ENVIRONMENT_PERIOD_MS 200u
#define CIS_BENCH_STATUS_PERIOD_MS 250u
#define CIS_BENCH_DIAG_PERIOD_MS 100u
#define CIS_BENCH_WATCH_PERIOD_MS 10u
/* NetPermissionReason values (net_upper_monitor.h): NO_DATA 0 .. OWNER_NO_DATA 9. */
#define CIS_BENCH_REASON_COUNT 10u

/* Index of CisBenchDiag.publish[]. The first three are periodic (service),
 * OCCUPANT is the relay made in the P_VISION_RESULT callback. */
enum CisBenchPublish
{
    CIS_BENCH_REAR = 0,
    CIS_BENCH_ENVIRONMENT = 1,
    CIS_BENCH_STATUS = 2,
    CIS_BENCH_OCCUPANT = 3,
    CIS_BENCH_PUBLISH_COUNT = 4
};
#define CIS_BENCH_PERIODIC_COUNT 3u

typedef struct {
    uint32_t submitted;             /* records offered */
    uint32_t result[4];             /* by NetSubmitResult: OK, INVALID_ARGUMENT, NOT_READY, NO_CAPACITY */
    uint32_t last_sequence;         /* REAR / STATUS: UPDATE_SEQUENCE, ENVIRONMENT: the three
                                       measurement numbers, OCCUPANT: VISION_SEQUENCE */
    uint8_t last_result;            /* NetSubmitResult of the last submission */
} CisBenchPublishDiag;

typedef struct {
    /* Received from Network (inside Net_MainFunction). */
    uint32_t permission_count;      /* DOMAIN_PERMISSION (CAN 0x080), repetitions included */
    NetDomainPermission last_permission;
    NetCanRxMeta last_permission_meta;      /* DOMAIN BOOT / TX_SEQUENCE, local RX ms */
    uint32_t vision_result_count;   /* P_VISION_RESULT (UART-C 0x60), repetitions included */
    NetVisionResult last_vision_result;
    NetUartRxMeta last_vision_result_meta;  /* LINK_SEQUENCE, local RX ms */
    uint32_t vision_status_count;   /* P_VISION_STATUS (UART-C 0x61) */
    NetVisionStatus last_vision_status;
    NetUartRxMeta last_vision_status_meta;
    /* Submissions: REAR, ENVIRONMENT, STATUS, OCCUPANT (enum CisBenchPublish). */
    CisBenchPublishDiag publish[CIS_BENCH_PUBLISH_COUNT];
    uint32_t refused;               /* submissions of any kind not answered OK */
    uint32_t diag_refreshes;
    /* DOMAIN_PERMISSION supervision, sampled every 10 ms. */
    uint8_t permission_reason;      /* NetPermissionReason now: 1 CURRENT, 4 SOURCE_EXPIRED, 5 RX_TIMEOUT ... */
    uint32_t permission_reason_changes;
    uint32_t permission_reason_entries[CIS_BENCH_REASON_COUNT];  /* times each reason was entered */
    uint32_t permission_reason_entered_ms[CIS_BENCH_REASON_COUNT]; /* Board ms of the last entry */
} CisBenchDiag;

extern volatile CisBenchDiag g_cis_bench;
extern volatile NetNodeDiag g_net_diag;

/* Main context, once, right after Net_Init(): records the init result. */
void CisBench_Init(void);
/* Main context, once per superloop pass, after Net_MainFunction(). */
void CisBench_Service(void);
#endif

#endif
