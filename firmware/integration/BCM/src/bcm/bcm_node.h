#ifndef BCM_NODE_H
#define BCM_NODE_H

/* BCM node entry for the Network S32K144 BCM bench project (NET_NODE_BUILD). It takes the
 * place of the bench stand-in (bcm_bench_app.c) in the project's main():
 *
 *   Board_TimeInit(); Net_Init(); BcmBench_Init();           ->  Board_TimeInit(); BcmNode_Init();
 *   Net_MainFunction(Board_NowMs32()); BcmBench_Service();   ->  BcmNode_Service();
 *
 * BcmNode_Init(): Ftm_Pwm_Ip_Init (interior light), BCM port, Net_Init(), BcmApp_Init().
 * BcmNode_Service(): only in a pass where the ms changed (bcm_runtime_rtd.c):
 * Net_MainFunction -> BcmApp_Step -> Net_TxFlush. No watchdog on the bench. */
#include <stdint.h>
#include "bcm/bcm_app.h"

/* S32DS Expressions: g_bcm_node (and g_net_diag, bcm_net_adapter.h). */
typedef struct
{
    uint32_t       passes;          /* service passes that ran */
    uint32_t       loop_us_last;    /* Net_MainFunction + BcmApp_Step + Net_TxFlush, Board_Us32 */
    uint32_t       loop_us_max;
    uint32_t       diag_us_last;    /* one NetNode_GetDiag() call (100 ms refresh) */
    uint32_t       diag_us_max;
    uint32_t       diag_refreshes;
    uint32_t       boot_id;         /* Net_GetBootId() */
    BcmAppCounters app;             /* BcmApp_GetCounters(), refreshed with g_net_diag */
} BcmNodeDiag;

extern volatile BcmNodeDiag g_bcm_node;

/* Main context, once, after Clock/Port/IntCtrl and Board_TimeInit(). */
void BcmNode_Init(void);

/* Main context, every superloop pass. */
void BcmNode_Service(void);

#endif
