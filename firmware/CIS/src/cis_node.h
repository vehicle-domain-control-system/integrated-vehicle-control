/*
 * CIS node entry for the S32DS project of the Network team (NET_NODE_BUILD).
 * It replaces the bench application (cis_bench_app.c) in the project's main loop:
 *
 *   Board_TimeInit();   Net_Init(); CisBench_Init();      ->   CisNode_Init();
 *   Net_MainFunction(Board_NowMs32()); CisBench_Service(); ->   CisNode_Service();
 *
 * Board_TimeInit() (SysTick) stays the first call of the project's main(), after the RTD clock
 * initialisation. CisNode_Init() starts the sensors and then Net_Init().
 */
#ifndef CIS_NODE_H
#define CIS_NODE_H

#include <stdbool.h>

/* Returns false if the sensor time base (FTM0 / SOSC) did not start; the node still runs. */
bool CisNode_Init(void);

/* One pass: Net_MainFunction() (receive callbacks), the CIS functions, then the CisNet_Publish*() calls. */
void CisNode_Service(void);

#endif /* CIS_NODE_H */
