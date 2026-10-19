/* The built-in SW2 input sequence end to end, through the image's own boot
 * sequence and super-loop (VssMain_Boot, VssMain_LoopOnce) on the HAL
 * stand-in: the stand-in answers the clock synthesizer and codec bring-up,
 * raises the output boundaries every 1024 frames at 48 kHz and plays the
 * switch. Every step of the 18-step table is pressed in turn and the cue
 * that plays is checked against the table.
 *
 * Separate runs: an interface error and a late refill each get the codec
 * mute bytes on the bus within the next pass; a refused mute is retried
 * every 50 ms, not every pass; a timer that does not start stops the boot;
 * a clock synthesizer that does not lock leaves the output unstarted and
 * reports an initialization fault. */

#include "test_util.h"
#include "hal_mock.h"

#include "../../src/app/vss_main.h"
#include "../../src/app/vss_test_input.h"
#include "../../src/service/input/vss_input_net.h"
#include "../../src/service/health/vss_health.h"
#include "../../src/service/playback/vss_playback.h"
#include "../../src/service/policy/vss_policy_store.h"
#include "../../src/driver/audio_tx/audio_tx.h"
#include "../../src/driver/audio_control/audio_control.h"
#include "../../src/net_if/net_stub.h"
#include "../../src/infra/vss_time.h"

#include <string.h>

#define CODEC 0x0Au

/* ---- time line --------------------------------------------------------- */

static uint32_t s_startTick;     /* tick of AudioTx_Start */
static uint32_t s_boundary;
static bool s_atBoundary;

static uint32_t BoundaryTick(uint32_t k)
{
    return s_startTick + (uint32_t)(((uint64_t)k * 256u) / 15u);
}

static void BoundaryPass(void)
{
    s_boundary++;
    HalMock_SetTicks(BoundaryTick(s_boundary));
    HalMock_Boundary();
    VssMain_LoopOnce();
    s_atBoundary = true;
}

/* A pass 10 ms after the last boundary. */
static void LaterPass(void)
{
    HalMock_SetTicks(BoundaryTick(s_boundary) + 8u);
    VssMain_LoopOnce();
    s_atBoundary = false;
}

static void NextPass(void)
{
    if (s_atBoundary)
    {
        LaterPass();
    }
    else
    {
        BoundaryPass();
    }
}

static void RunMs(uint64_t ms)
{
    uint64_t end = VssTime_NowMs() + ms;

    while (VssTime_NowMs() < end)
    {
        NextPass();
    }
}

/* ---- observations ------------------------------------------------------ */

static VssPlaybackObservation s_view;
static VssTestInputObservation s_in;
static VssMainObservation s_main;

static void View(void)
{
    CHECK(Playback_Read(&s_view));
    CHECK(VssTestInput_Read(&s_in));
    CHECK(VssMain_Read(&s_main));
}

static bool Playing(VssInputSignal signal, VssInputValue value)
{
    View();
    return s_view.present && (s_view.lastStartedSession == s_view.session) && !s_view.stopRequested &&
           (s_view.candidate.signal == signal) && (s_view.candidate.value == value);
}

static bool RunUntilPlaying(VssInputSignal signal, VssInputValue value, uint64_t maxMs)
{
    uint64_t end = VssTime_NowMs() + maxMs;

    while (VssTime_NowMs() < end)
    {
        if (Playing(signal, value))
        {
            return true;
        }
        NextPass();
    }
    return Playing(signal, value);
}

static bool RunUntilIdle(uint64_t maxMs)
{
    uint64_t end = VssTime_NowMs() + maxMs;

    while (VssTime_NowMs() < end)
    {
        View();
        if (!s_view.present)
        {
            return true;
        }
        NextPass();
    }
    View();
    return !s_view.present;
}

/* One debounced press and release of SW2. */
static void Press(void)
{
    uint32_t presses;
    uint64_t end;

    View();
    presses = s_in.presses;
    HalMock_SetSw2(true);
    end = VssTime_NowMs() + 100u;
    while ((VssTime_NowMs() < end) && (s_in.presses == presses))
    {
        NextPass();
        View();
    }
    CHECK_EQ_U(s_in.presses, presses + 1u);
    HalMock_SetSw2(false);
    RunMs(40u);
}

/* ---- boot -------------------------------------------------------------- */

static void Queue1(uint8_t a)
{
    HalMock_QueueRead(&a, 1u);
}

static void Queue2(uint16_t v)
{
    uint8_t b[2];

    b[0] = (uint8_t)(v >> 8);
    b[1] = (uint8_t)v;
    HalMock_QueueRead(b, 2u);
}

/* Clock synthesizer and codec answers of a good bring-up, then the two
 * registers the unmute reads. */
static void QueueBringUp(bool clockLocks)
{
    Queue1(0x6Cu);                        /* DEVICE_ID */
    Queue1(0x80u);                        /* DEVICE_CTRL first read */
    Queue1(clockLocks ? 0x00u : 0x80u);   /* DEVICE_CTRL: unlock bit */
    if (!clockLocks)
    {
        return;
    }
    Queue2(0xA011u);                      /* CHIP_ID */
    Queue2(0x0040u);
    Queue2(0x7060u);
    Queue2(0x0200u);
    Queue2(0x0145u);
    Queue2(0x020Cu);                      /* unmute: ADCDAC_CTRL */
    Queue2(0x0135u);                      /* unmute: ANA_CTRL */
}

/* Power-on state of every module. */
static void PowerOn(void)
{
    HalMock_Reset();
    AudioTx_ResetForTest();
    AudioControl_ResetForTest();
    VssTime_ResetForTest();
    Input_TestReset();
    Store_TestReset();
    StorePlayback_TestReset();
    VssInputNet_ResetForTest();
    NetStub_ResetForTest();
    s_boundary = 0u;
    s_atBoundary = false;
}

/* Index of the first codec write with these four bytes at or after from. */
static uint32_t FindCodecWrite(uint32_t from, uint16_t reg, uint16_t value)
{
    const HalMockLog *log = HalMock_Log();
    uint32_t i;

    for (i = from; i < log->count; ++i)
    {
        const HalMockOp *op = &log->ops[i];

        if ((op->kind == HAL_MOCK_I2C_WRITE) && (op->addr == CODEC) && (op->len == 4u) &&
            (op->bytes[0] == (uint8_t)(reg >> 8)) && (op->bytes[1] == (uint8_t)reg) &&
            (op->bytes[2] == (uint8_t)(value >> 8)) && (op->bytes[3] == (uint8_t)value))
        {
            return i;
        }
    }
    return UINT32_MAX;
}

static bool MuteBytesLogged(void)
{
    uint32_t hp = FindCodecWrite(0u, 0x0024u, 0x0135u);   /* headphone mute */

    return (hp != UINT32_MAX) && (FindCodecWrite(hp + 1u, 0x0022u, 0x7F7Fu) != UINT32_MAX);
}

static bool BootGood(void)
{
    const HalMockLog *log;
    uint32_t unmute;
    bool ok;

    PowerOn();
    QueueBringUp(true);
    ok = VssMain_Boot();
    log = HalMock_Log();
    CHECK(!log->overflow);
    /* The unmute is the last transfer: nothing blocks after the start. */
    unmute = FindCodecWrite(0u, 0x0024u, 0x0125u);
    CHECK(unmute != UINT32_MAX);
    CHECK_EQ_U(unmute + 1u, log->count);
    CHECK(FindCodecWrite(0u, 0x0022u, 0x4848u) != UINT32_MAX);   /* 70 % level */
    CHECK_EQ_U(HalMock_AudioSubmits(), 1u);
    s_startTick = HalTimer_GetTicks();
    HalMock_ClearLog();
    VssMain_LoopOnce();
    return ok;
}

/* ---- the 18 steps ------------------------------------------------------- */

static void test_eighteen_steps(void)
{
    VssInputNetObservation adapter;
    NetVssStatus status;
    uint32_t session;
    uint32_t frames;

    CHECK(BootGood());
    View();
    CHECK_EQ_U(s_main.stage, VSS_MAIN_STAGE_RUNNING);
    CHECK(s_main.boardOk && s_main.timerOk && s_main.audioControlOk && s_main.unmuteOk &&
          s_main.outputStarted);
    /* VSS start is clock 0: the clock counts from the first timer tick. */
    CHECK_EQ_U(VssTime_NowMs(), VssTime_TicksToMs(HalTimer_GetTicks()));

    /* 0: boot, every warning CLEAR - nothing plays. */
    RunMs(500u);
    View();
    CHECK(!s_view.present);
    CHECK_EQ_U(s_view.session, 0u);
    CHECK(s_in.warningFrames >= 9u);
    CHECK(!s_in.fault);

    /* 1: WELCOME once. */
    Press();
    CHECK_EQ_U(s_in.step, 1u);
    CHECK(RunUntilPlaying(VSS_SIGNAL_WELCOME, VSS_VALUE_EVENT, 300u));
    CHECK(RunUntilIdle(8000u));
    CHECK_EQ_U(s_view.lastEnd, PLAYBACK_END_COMPLETED);
    CHECK_EQ_U(s_in.accepted, 1u);
    session = s_view.lastSession;

    /* 2: the retransmission of step 1 arrives after the use limit: refused,
     * nothing plays. */
    Press();
    CHECK_EQ_U(s_in.step, 2u);
    RunMs(600u);
    View();
    CHECK(!s_view.present);
    CHECK_EQ_U(s_view.lastSession, session);
    CHECK_EQ_U(s_in.rejected, 1u);
    CHECK_EQ_U(s_in.duplicates, 0u);
    CHECK(VssInputNet_Read(VSS_TEST, &adapter));
    CHECK_EQ_U(adapter.lastEvent.validation, VSS_VALIDATION_REJECTED);

    /* 3..6: one-shot cues. */
    Press();
    CHECK(RunUntilPlaying(VSS_SIGNAL_LOCK_COMPLETE, VSS_VALUE_EVENT, 300u));
    CHECK(RunUntilIdle(8000u));
    Press();
    CHECK(RunUntilPlaying(VSS_SIGNAL_UNLOCK_COMPLETE, VSS_VALUE_EVENT, 300u));
    CHECK(RunUntilIdle(8000u));
    Press();
    CHECK(RunUntilPlaying(VSS_SIGNAL_LOCK_ERROR, VSS_VALUE_EVENT, 300u));
    CHECK(RunUntilIdle(8000u));
    Press();
    CHECK(RunUntilPlaying(VSS_SIGNAL_GOODBYE, VSS_VALUE_EVENT, 300u));
    CHECK(RunUntilIdle(8000u));
    CHECK_EQ_U(s_view.lastEnd, PLAYBACK_END_COMPLETED);
    CHECK_EQ_U(s_in.step, 6u);
    CHECK_EQ_U(s_in.accepted, 5u);

    /* 7: rear CAUTION, repeated. */
    Press();
    CHECK(RunUntilPlaying(VSS_SIGNAL_REAR_RISK, VSS_VALUE_CAUTION, 300u));
    session = s_view.session;
    RunMs(3000u);
    CHECK(Playing(VSS_SIGNAL_REAR_RISK, VSS_VALUE_CAUTION));
    CHECK_EQ_U(s_view.session, session);

    /* 8: rear EMERGENCY preempts. */
    Press();
    CHECK(RunUntilPlaying(VSS_SIGNAL_REAR_RISK, VSS_VALUE_EMERGENCY, 300u));

    /* 9: anti-pinch preempts (highest). */
    Press();
    CHECK(RunUntilPlaying(VSS_SIGNAL_ANTI_PINCH, VSS_VALUE_ACTIVE, 300u));

    /* 10: anti-pinch CLEAR - rear EMERGENCY again. */
    Press();
    CHECK(RunUntilPlaying(VSS_SIGNAL_REAR_RISK, VSS_VALUE_EMERGENCY, 300u));
    session = s_view.session;

    /* 11: occupant ACTIVE - no change, it ranks below rear emergency. */
    Press();
    RunMs(1000u);
    CHECK(Playing(VSS_SIGNAL_REAR_RISK, VSS_VALUE_EMERGENCY));
    CHECK_EQ_U(s_view.session, session);

    /* 12: rear CLEAR - occupant. */
    Press();
    CHECK(RunUntilPlaying(VSS_SIGNAL_OCCUPANT_HAZARD, VSS_VALUE_ACTIVE, 300u));

    /* 13: occupant CLEAR - stops. */
    Press();
    CHECK(RunUntilIdle(300u));
    session = s_view.lastSession;
    RunMs(500u);
    View();
    CHECK(!s_view.present);

    /* 14: rear quality INVALID - no change (INVALID is not CLEAR, and the
     * last valid value was CLEAR). */
    Press();
    RunMs(1000u);
    View();
    CHECK(!s_view.present);
    CHECK_EQ_U(s_view.lastSession, session);

    /* 15: quality OK, EMERGENCY. */
    Press();
    CHECK(RunUntilPlaying(VSS_SIGNAL_REAR_RISK, VSS_VALUE_EMERGENCY, 300u));

    /* 16: rear CLEAR - stops. */
    Press();
    CHECK(RunUntilIdle(300u));

    /* 17: WELCOME. */
    Press();
    CHECK(RunUntilPlaying(VSS_SIGNAL_WELCOME, VSS_VALUE_EVENT, 300u));
    CHECK(RunUntilIdle(8000u));
    CHECK_EQ_U(s_in.accepted, 6u);
    CHECK_EQ_U(s_in.rejected, 1u);

    /* 18: end of input - no more frames, later presses ignored, silence. */
    Press();
    CHECK_EQ_U(s_in.step, 18u);
    frames = s_in.warningFrames;
    session = s_view.lastSession;
    Press();
    RunMs(1000u);
    View();
    CHECK_EQ_U(s_in.step, 18u);
    CHECK_EQ_U(s_in.presses, 19u);
    CHECK_EQ_U(s_in.warningFrames, frames);
    CHECK(!s_view.present);
    CHECK_EQ_U(s_view.lastSession, session);
    CHECK(!s_in.fault);

    /* The whole run: no transfer or wait in any pass, no output fault, and
     * the service reports a full READY. */
    CHECK_EQ_U(HalMock_Log()->count, 0u);
    CHECK_EQ_U(AudioTx_FaultMask(), 0u);
    CHECK(!s_main.outputFailed);
    CHECK_EQ_U(s_main.muteCalls, 0u);
    CHECK(s_main.flowPasses > 0u);
    CHECK(s_main.inputPasses > 0u);
    CHECK(s_main.loopMaxTicks <= 1u);
    CHECK(NetStub_ReadLastStatus(&status));
    CHECK_EQ_U(status.vss_state, NET_VSS_STATE_READY);
    CHECK_EQ_U(status.vss_availability, NET_VSS_AVAILABILITY_FULL);
    CHECK_EQ_U(status.fault_mask, 0u);
}

/* The flow runs on input, on a follow-up and VSS_CFG_FLOW_PERIOD_MS after
 * its last pass, not on every pass: passes one tick (1.25 ms) apart for
 * 40 ticks skip some flow passes and never leave 10 ms without one. */
static void test_flow_pass_rule(void)
{
    uint32_t passes;
    uint32_t lastFlowTick;
    uint32_t maxGap = 0u;
    uint32_t skipped = 0u;
    uint32_t t;

    CHECK(BootGood());
    View();
    passes = s_main.flowPasses;
    lastFlowTick = s_startTick;
    for (t = 1u; t <= 40u; ++t)
    {
        HalMock_SetTicks(s_startTick + t);
        VssMain_LoopOnce();
        View();
        if (s_main.flowPasses != passes)
        {
            passes = s_main.flowPasses;
            if ((t + s_startTick - lastFlowTick) > maxGap)
            {
                maxGap = t + s_startTick - lastFlowTick;
            }
            lastFlowTick = s_startTick + t;
        }
        else
        {
            skipped++;
        }
    }
    CHECK(skipped > 0u);
    CHECK(maxGap <= 8u);    /* 10 ms */
    CHECK(maxGap >= 7u);
}

/* ---- output failures ----------------------------------------------------- */

static void test_interface_error_mutes_in_the_next_pass(void)
{
    CHECK(BootGood());
    RunMs(200u);
    HalMock_ClearLog();
    Queue2(0x0125u);                      /* ANA_CTRL read by the mute */
    HalMock_SaiError();
    VssMain_LoopOnce();
    CHECK(MuteBytesLogged());
    View();
    CHECK(s_main.outputFailed);
    CHECK(s_main.muteAccepted);
    CHECK_EQ_U(s_main.muteCalls, 1u);
    CHECK((AudioTx_FaultMask() & AUDIO_TX_FAULT_SAI) != 0u);
    HalMock_ClearLog();
    RunMs(200u);
    View();
    CHECK_EQ_U(s_main.muteCalls, 1u);
    CHECK_EQ_U(HalMock_Log()->count, 0u);
}

static void test_late_refill_mutes_in_the_next_pass(void)
{
    CHECK(BootGood());
    RunMs(200u);
    if (!s_atBoundary)
    {
        BoundaryPass();
    }
    HalMock_ClearLog();
    Queue2(0x0125u);
    /* Two boundaries without a pass between them: the half freed by the
     * first is still empty at the second. */
    s_boundary++;
    HalMock_SetTicks(BoundaryTick(s_boundary));
    HalMock_Boundary();
    s_boundary++;
    HalMock_SetTicks(BoundaryTick(s_boundary));
    HalMock_Boundary();
    CHECK((AudioTx_FaultMask() & AUDIO_TX_FAULT_LATE) != 0u);
    VssMain_LoopOnce();
    CHECK(MuteBytesLogged());
    View();
    CHECK(s_main.muteAccepted);
    CHECK_EQ_U(s_main.muteCalls, 1u);
}

/* A refused mute is tried again every 50 ms (40 ticks), not every pass,
 * until the codec takes it. */
static void test_refused_mute_is_retried(void)
{
    uint32_t failedTick;
    uint32_t t;

    CHECK(BootGood());
    RunMs(200u);
    HalMock_ClearLog();
    HalMock_FailI2cAt(0u);
    HalMock_SaiError();
    VssMain_LoopOnce();
    failedTick = HalTimer_GetTicks();
    View();
    CHECK(s_main.outputFailed);
    CHECK(!s_main.muteAccepted);
    CHECK_EQ_U(s_main.muteCalls, 1u);
    for (t = 4u; t < 40u; t += 4u)
    {
        HalMock_SetTicks(failedTick + t);
        VssMain_LoopOnce();
        View();
        CHECK_EQ_U(s_main.muteCalls, 1u);
    }
    /* The log is not cleared: that would re-arm the failing transfer. */
    Queue2(0x0125u);
    HalMock_SetTicks(failedTick + 40u);
    VssMain_LoopOnce();
    View();
    CHECK_EQ_U(s_main.muteCalls, 2u);
    CHECK(s_main.muteAccepted);
    CHECK(MuteBytesLogged());
    HalMock_SetTicks(failedTick + 200u);
    VssMain_LoopOnce();
    View();
    CHECK_EQ_U(s_main.muteCalls, 2u);
}

/* ---- boot failures --------------------------------------------------------- */

static void test_timer_failure_stops_the_boot(void)
{
    PowerOn();
    HalMock_SetTimerInitResult(false);
    CHECK(!VssMain_Boot());
    View();
    CHECK_EQ_U(s_main.stage, VSS_MAIN_STAGE_TIMER_FAILED);
    CHECK_EQ_U(HalMock_Log()->count, 0u);
    CHECK_EQ_U(HalMock_AudioInitCalls(), 0u);
    CHECK(!VssTime_IsRegistered());
}

static void test_clock_not_locked_leaves_the_output_off(void)
{
    NetVssStatus status;
    uint32_t i;

    PowerOn();
    QueueBringUp(false);
    CHECK(VssMain_Boot());
    View();
    CHECK(!s_main.audioControlOk);
    CHECK(!s_main.unmuteOk);
    CHECK(!s_main.outputStarted);
    CHECK_EQ_U(HalMock_AudioInitCalls(), 0u);
    CHECK_EQ_U(HalMock_AudioStartCalls(), 0u);
    s_startTick = HalTimer_GetTicks();
    for (i = 1u; i <= 40u; ++i)
    {
        HalMock_SetTicks(s_startTick + 8u * i);
        VssMain_LoopOnce();
    }
    CHECK_EQ_U(Health_CurrentMask(), 1u << (unsigned)VSS_FAULT_INIT);
    CHECK(NetStub_ReadLastStatus(&status));
    CHECK_EQ_U(status.vss_state, NET_VSS_STATE_FAULT);
    CHECK_EQ_U(status.fault_mask, NET_VSS_FAULT_INITIALIZATION_FAILURE);
    CHECK_EQ_U(status.vss_accepting_events, NET_FLAG_NO);
}

int main(void)
{
    test_eighteen_steps();
    test_flow_pass_rule();
    test_interface_error_mutes_in_the_next_pass();
    test_late_refill_mutes_in_the_next_pass();
    test_refused_mute_is_retried();
    test_timer_failure_stops_the_boot();
    test_clock_not_locked_leaves_the_output_off();
    TEST_FINISH();
}
