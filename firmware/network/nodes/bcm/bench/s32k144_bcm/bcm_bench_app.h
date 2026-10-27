#ifndef NET_BENCH_BCM_BENCH_APP_H
#define NET_BENCH_BCM_BENCH_APP_H

/* S32K144 bench stand-in for the BCM application (not Network product code).
 * Built only when the board build selects the BCM node profile
 * (NETWORK_UPPER_NODE_PROFILE=3).
 *
 * - BcmApp_OnDomainDoorCommand / ClimateCommand / LightCommand only count
 *   what Network delivers (g_bcm_bench).
 * - BcmBench_Service() submits the four BCM states at their document
 *   periods (DOOR 100 ms, CLIMATE 200 ms, LIGHT 200 ms, STATUS 250 ms), right
 *   after start and then once per period. Every submission is a new
 *   evaluation: UPDATE_SEQUENCE + 1, SOURCE_AGE 0, every quality OK. A refused
 *   one is counted, not offered again.
 * - g_net_diag is NetNode_GetDiag(), refreshed every 100 ms. */
#include <stdint.h>
#include "network/net_node.h"

#if NETWORK_UPPER_NODE_PROFILE == NET_UPPER_PROFILE_BCM
#include "network/app/net_if_bcm.h"

#define BCM_BENCH_DOOR_PERIOD_MS 100u
#define BCM_BENCH_CLIMATE_PERIOD_MS 200u
#define BCM_BENCH_LIGHT_PERIOD_MS 200u
#define BCM_BENCH_STATUS_PERIOD_MS 250u
#define BCM_BENCH_DIAG_PERIOD_MS 100u

/* Index of BcmBenchDiag.state[]. */
enum BcmBenchState
{
    BCM_BENCH_DOOR = 0,
    BCM_BENCH_CLIMATE = 1,
    BCM_BENCH_LIGHT = 2,
    BCM_BENCH_STATUS = 3,
    BCM_BENCH_STATE_COUNT = 4
};

typedef struct {
    uint32_t submitted;             /* evaluations offered (UPDATE_SEQUENCE values used) */
    uint32_t result[4];             /* by NetSubmitResult: OK, INVALID_ARGUMENT, NOT_READY, NO_CAPACITY */
    uint32_t last_update_sequence;  /* last submitted evaluation */
    uint8_t last_result;            /* NetSubmitResult of the last submission */
} BcmBenchStateDiag;

typedef struct {
    /* Received from Network (inside Net_MainFunction). */
    uint32_t door_command_count;
    uint32_t climate_command_count;
    uint32_t light_command_count;
    /* State submissions: DOOR, CLIMATE, LIGHT, STATUS (enum BcmBenchState). */
    BcmBenchStateDiag state[BCM_BENCH_STATE_COUNT];
    uint32_t refused;               /* submissions of any state not answered OK */
    uint32_t diag_refreshes;
} BcmBenchDiag;

extern volatile BcmBenchDiag g_bcm_bench;
extern volatile NetNodeDiag g_net_diag;

/* Main context, once, right after Net_Init(): records the init result. */
void BcmBench_Init(void);
/* Main context, once per superloop pass, after Net_MainFunction(). */
void BcmBench_Service(void);
#endif

#endif
