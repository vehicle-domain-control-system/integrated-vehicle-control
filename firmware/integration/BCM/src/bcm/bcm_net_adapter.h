#ifndef BCM_NET_ADAPTER_H
#define BCM_NET_ADAPTER_H

/* Network entry for the BCM node (network/app/net_if_entry.h), main context only.
 * The BCM application already implements BcmApp_OnDomain* and calls BcmNet_*
 * (bcm/bcm_app.c), so this only wraps the entry points and keeps g_net_diag. */
#include <stdbool.h>
#include <stdint.h>
#include "network/net_node.h"

#define BCM_NET_ADAPTER_DIAG_PERIOD_MS  100u

/* NetNode_GetDiag() snapshot for S32DS Expressions, refreshed every 100 ms. */
extern volatile NetNodeDiag g_net_diag;

/* Net_Init() once (after Board_TimeInit), then the first g_net_diag. */
void BcmNetAdapter_Init(void);

/* Net_MainFunction(): BcmApp_OnDomain* run inside it. */
void BcmNetAdapter_Poll(uint32_t nowMs);

/* Net_TxFlush(): sends what BcmApp_Step() submitted in this pass. */
void BcmNetAdapter_Flush(uint32_t nowMs);

/* Every BCM_NET_ADAPTER_DIAG_PERIOD_MS: refreshes g_net_diag, returns true and the
 * time of the NetNode_GetDiag() call in us (Board_Us32). */
bool BcmNetAdapter_RefreshDiag(uint32_t nowMs, uint32_t *diagUs);

#endif
