#include "bcm_net_adapter.h"
#include "net_board_time144.h"
#include "network/app/net_if_entry.h"

volatile NetNodeDiag g_net_diag;

static NetNodeDiag s_diag;
static uint32_t    s_nextDiagMs;

static uint32_t TakeDiag(void)
{
    uint32_t startUs = Board_Us32();
    uint32_t tookUs;

    (void)NetNode_GetDiag(&s_diag);
    tookUs = Board_Us32() - startUs;
    g_net_diag = s_diag;
    return tookUs;
}

void BcmNetAdapter_Init(void)
{
    Net_Init();
    (void)TakeDiag();
    s_nextDiagMs = Board_NowMs32() + BCM_NET_ADAPTER_DIAG_PERIOD_MS;
}

void BcmNetAdapter_Poll(uint32_t nowMs)
{
    Net_MainFunction(nowMs);
}

void BcmNetAdapter_Flush(uint32_t nowMs)
{
    Net_TxFlush(nowMs);
}

bool BcmNetAdapter_RefreshDiag(uint32_t nowMs, uint32_t *diagUs)
{
    if ((int32_t)(nowMs - s_nextDiagMs) < 0)
    {
        return false;
    }
    s_nextDiagMs = nowMs + BCM_NET_ADAPTER_DIAG_PERIOD_MS;
    *diagUs = TakeDiag();
    return true;
}
