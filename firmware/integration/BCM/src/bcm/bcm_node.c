#include "bcm_node.h"
#include "bcm_net_adapter.h"
#include "bcm_port_bench144.h"
#include "net_board_time144.h"
#include "network/app/net_if_entry.h"

#include "Ftm_Pwm_Ip.h"

#define PWM_FTM_INSTANCE  0U

volatile BcmNodeDiag g_bcm_node;

static uint32_t s_lastMs;

static void Observe(uint32_t nowMs)
{
    uint32_t diagUs;
    BcmAppCounters counters;

    if (!BcmNetAdapter_RefreshDiag(nowMs, &diagUs))
    {
        return;
    }
    g_bcm_node.diag_us_last = diagUs;
    if (diagUs > g_bcm_node.diag_us_max)
    {
        g_bcm_node.diag_us_max = diagUs;
    }
    g_bcm_node.diag_refreshes++;
    g_bcm_node.boot_id = Net_GetBootId();
    BcmApp_GetCounters(&counters);
    g_bcm_node.app = counters;
}

/* Order of the BCM main.c: Ftm_Pwm_Ip_Init -> port -> Net_Init -> BcmApp_Init. */
void BcmNode_Init(void)
{
    uint32_t nowMs;

    Ftm_Pwm_Ip_Init(PWM_FTM_INSTANCE, &Ftm_Pwm_Ip_UserCfg0);
    BcmPortBench_Init();
    BcmNetAdapter_Init();
    nowMs = Board_NowMs32();
    BcmApp_Init(nowMs);
    s_lastMs = nowMs;
    g_bcm_node.boot_id = Net_GetBootId();
}

void BcmNode_Service(void)
{
    uint32_t nowMs = Board_NowMs32();
    uint32_t startUs;
    uint32_t loopUs;

    if (nowMs == s_lastMs)
    {
        return;
    }
    s_lastMs = nowMs;

    startUs = Board_Us32();
    BcmNetAdapter_Poll(nowMs);
    BcmApp_Step(nowMs);
    BcmNetAdapter_Flush(nowMs);
    loopUs = Board_Us32() - startUs;

    g_bcm_node.passes++;
    g_bcm_node.loop_us_last = loopUs;
    if (loopUs > g_bcm_node.loop_us_max)
    {
        g_bcm_node.loop_us_max = loopUs;
    }
    Observe(nowMs);
}
