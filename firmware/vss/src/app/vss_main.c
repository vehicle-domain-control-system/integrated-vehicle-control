#include "vss_main.h"

#include <string.h>

#include "vss_flow.h"
#include "vss_test_input.h"
#include "../service/asset/audio_asset.h"
#include "../service/audio_stream/audio_stream.h"
#include "../service/health/vss_health.h"
#include "../service/input/vss_input_net.h"
#include "../driver/audio_control/audio_control.h"
#include "../driver/audio_tx/audio_tx.h"
#include "../driver/input/sw2_button.h"
#include "../hal/include/hal_board.h"
#include "../hal/include/hal_timer.h"
#include "../net_if/vss_net_api.h"
#include "../infra/vss_config.h"
#include "../infra/vss_time.h"

/* Bring-up value: the codec reference clock settles after the tick starts. */
#define VSS_MAIN_REF_SETTLE_TICKS 80u

#define VSS_MAIN_ORIGIN ((VSS_CFG_TEST_INPUT != 0) ? VSS_TEST : VSS_PRODUCT)

/* Debugger view. */
static VssMainObservation g_vssMain;

static bool s_followUp;
static bool s_flowRan;
static uint64_t s_lastFlowMs;
static uint64_t s_lastMuteMs;

/* Scratch: too large for the stack. */
static VssInputNetObservation s_adapter;

static void count_up(uint32_t *counter)
{
    if (*counter != UINT32_MAX)
    {
        ++*counter;
    }
}

static VssTimeEvidence now_evidence(void)
{
    VssTimeEvidence t;
    uint64_t ms = VssTime_NowMs();

    t.domain = VSS_MAIN_ORIGIN;
    t.epoch = VSS_TIME_EPOCH;
    t.earliest = ms;
    t.latest = ms;
    t.continuity = VSS_CONTINUITY_COMPARABLE;
    return t;
}

static void report(VssFaultKind kind)
{
    VssTimeEvidence at = now_evidence();

    Health_Report(kind, &at);
}

/* Messages the adapter passed to INPUT for the active origin. */
static uint32_t forwarded(void)
{
    memset(&s_adapter, 0, sizeof s_adapter);
    (void)VssInputNet_Read(VSS_MAIN_ORIGIN, &s_adapter);
    return s_adapter.eventsForwarded + s_adapter.warningFramesForwarded;
}

bool VssMain_Boot(void)
{
    memset(&g_vssMain, 0, sizeof g_vssMain);
    s_followUp = false;
    s_flowRan = false;
    s_lastFlowMs = 0u;
    s_lastMuteMs = 0u;

    g_vssMain.boardOk = HalBoard_Init();
    g_vssMain.timerOk = HalTimer_Init();
    if (!g_vssMain.timerOk)
    {
        g_vssMain.stage = (uint8_t)VSS_MAIN_STAGE_TIMER_FAILED;
        return false;
    }
    HalTimer_WaitTicks(VSS_MAIN_REF_SETTLE_TICKS);
    (void)VssTime_Init(HalTimer_GetTicks);
    Health_Init(VSS_MAIN_ORIGIN);
    if (!g_vssMain.boardOk)
    {
        report(VSS_FAULT_INIT);
    }

    g_vssMain.stage = (uint8_t)VSS_MAIN_STAGE_AUDIO_CONTROL;
    g_vssMain.audioControlOk = AudioControl_Init();
    if (!g_vssMain.audioControlOk)
    {
        report(VSS_FAULT_INIT);
    }
    else
    {
        g_vssMain.unmuteOk = AudioControl_Unmute((uint8_t)VSS_CFG_VOLUME_PERCENT);
        if (!g_vssMain.unmuteOk)
        {
            report(VSS_FAULT_OUTPUT);
        }
    }

    g_vssMain.stage = (uint8_t)VSS_MAIN_STAGE_SERVICES;
#if VSS_CFG_TEST_INPUT
    Sw2Button_Init(HalTimer_GetTicks());
#endif
    AudioAsset_Init();
    AudioStream_Init();
    VssTestInput_Init();
    Flow_Init(VSS_MAIN_ORIGIN);
    Net_Init();

    /* No blocking call after this point: the output deadlines start. */
    g_vssMain.stage = (uint8_t)VSS_MAIN_STAGE_OUTPUT;
    if (g_vssMain.audioControlOk)
    {
        g_vssMain.outputStarted = AudioTx_Init() && AudioTx_Start();
        if (!g_vssMain.outputStarted)
        {
            report(VSS_FAULT_INIT);
        }
    }
    Health_BootComplete();
    g_vssMain.stage = (uint8_t)VSS_MAIN_STAGE_RUNNING;
    return true;
}

/* After an output failure the codec is muted; a refused mute is tried again
 * every VSS_MAIN_MUTE_RETRY_MS until the codec takes it. */
static void mute_after_output_failure(void)
{
    uint64_t nowMs;

    if (g_vssMain.muteAccepted || !AudioStream_OutputFailed())
    {
        return;
    }
    nowMs = VssTime_NowMs();
    if (g_vssMain.outputFailed && ((nowMs - s_lastMuteMs) < (uint64_t)VSS_MAIN_MUTE_RETRY_MS))
    {
        return;
    }
    g_vssMain.outputFailed = true;
    s_lastMuteMs = nowMs;
    count_up(&g_vssMain.muteCalls);
    g_vssMain.muteAccepted = AudioControl_MuteLatched();
}

void VssMain_LoopOnce(void)
{
    uint32_t t0 = HalTimer_GetTicks();
    uint32_t before;
    bool input;
    uint32_t took;

    AudioStream_Service();

    before = forwarded();
    Net_MainFunction(VssTime_NowMs32());
#if VSS_CFG_TEST_INPUT
    VssTestInput_Service(HalTimer_GetTicks());
#endif
    input = forwarded() != before;

    if (input || s_followUp || !s_flowRan ||
        ((VssTime_NowMs() - s_lastFlowMs) >= (uint64_t)VSS_CFG_FLOW_PERIOD_MS))
    {
        if (input)
        {
            count_up(&g_vssMain.inputPasses);
        }
        else if (s_followUp)
        {
            count_up(&g_vssMain.followUpPasses);
        }
        s_followUp = Flow_Process(input ? VSS_OPPORTUNITY_INPUT : VSS_OPPORTUNITY_TIME_CHECK) !=
                     VSS_FLOW_SETTLED;
        s_flowRan = true;
        s_lastFlowMs = VssTime_NowMs();
        count_up(&g_vssMain.flowPasses);
    }

    mute_after_output_failure();

    count_up(&g_vssMain.loops);
    took = HalTimer_GetTicks() - t0;
    if (took > g_vssMain.loopMaxTicks)
    {
        g_vssMain.loopMaxTicks = took;
    }
}

bool VssMain_Read(VssMainObservation *out)
{
    if (out == NULL)
    {
        return false;
    }
    *out = g_vssMain;
    return true;
}

#if !VSS_CFG_HOST_TEST
int main(void)
{
    if (VssMain_Boot())
    {
        for (;;)
        {
            VssMain_LoopOnce();
        }
    }
    for (;;)
    {
        /* No tick: nothing that needs time can run. */
    }
}
#endif
