/* VSS half of the integration harness (see vss_integ_bus.h): the real VSS
 * image (VssMain_Boot, VssMain_LoopOnce, built with VSS_CFG_NET_STUB=0 and
 * VSS_CFG_TEST_INPUT=0) on its own HAL stand-in, linked with this Network node
 * (VSS profile, mock RTD, time base A with HalTimer_GetTicks /
 * VssTime_TickToMs32). VSS headers only: never include a Network header here.
 *
 * Time line: one super-loop pass per board tick (1.25 ms). Before the pass the
 * tick moves, the output boundary interrupts come every 1024 frames at 48 kHz
 * (without them VSS would report an output fault) and the DOMAIN stand-in puts
 * its frames on the bus from the CAN "ISR". After the pass the bus takes the
 * frames the node sent.
 *
 * Spies (-Wl,--wrap): VssApp_OnDomainEvent / VssApp_OnDomainWarningState
 * (Network -> VSS) and VssNet_PublishStatus (VSS -> Network) record each call
 * and then run the real function.
 *
 * Expected results come from "VSS 연동 확인 시험" (VSS_NODE_INTEGRATION.md),
 * the VSS contracts C1..C12 (VSS_NODE_INTEGRATION.md) and docs/network. One
 * case per process. */
#include "hal_mock.h"
#include "app/vss_main.h"
#include "app/vss_flow.h"
#include "service/input/vss_input_net.h"
#include "service/health/vss_health.h"
#include "service/playback/vss_playback.h"
#include "service/policy/vss_policy_store.h"
#include "driver/audio_tx/audio_tx.h"
#include "driver/audio_control/audio_control.h"
#include "infra/vss_time.h"
#include "net_if/vss_net_api.h"
#include "vss_integ_bus.h"
#include "vss_integ_layout.h"

#include <stddef.h>
#include <stdio.h>
#include <string.h>

static const char *s_case = "";

#define CHECK(x) do { if (!(x)) { fprintf(stderr, "%s:%d: %s\n", s_case, __LINE__, #x); return 1; } } while (0)

/* DOMAIN boots (CAN BOOT_ID of the DOMAIN node, run A and run B) and the
 * producers named in the warning lanes. */
#define DOMAIN_BOOT_A 0x444F4D41u
#define DOMAIN_BOOT_B 0x444F4D42u
#define WINDOW_BOOT   0x57494E31u
#define CIS_BOOT      0x43495331u

/* Codec sample rate: a half-buffer of AUDIO_TX_HALF_FRAMES frames ends every
 * 1024 / 48000 s = 256/15 ticks. */
#define SAMPLE_HZ 48000u
/* DOMAIN sends VSS_WARNING_STATE every 50 ms (contract C11). */
#define WARNING_PERIOD_TICKS 40u
/* VSS receive watch of the warning stream and the occupant hazard Hold
 * ("VSS 수신 처리" (VSS_NODE_INTEGRATION.md): TTL / Hold occupant
 * 1000 / 500 ms). */
#define RX_WATCH_MS 150u
#define OCCUPANT_HOLD_MS 500u

/* ------------------------------------------------------------ time line ---- */
static uint32_t s_tick;        /* board tick (HalMock_SetTicks) */
static uint32_t s_startTick;   /* tick of AudioTx_Start, end of the boot */
static uint32_t s_boundary;    /* output boundaries raised since the start */
static uint32_t s_loop;        /* index of the current / last super-loop pass */
static bool s_inLoop;

static uint32_t BoundaryTick(uint32_t k)
{
    return s_startTick + (uint32_t)(((uint64_t)k * AUDIO_TX_HALF_FRAMES * VSS_TIME_TICK_HZ) / SAMPLE_HZ);
}

static uint64_t NowMs(void)
{
    return VssTime_NowMs();
}

/* ------------------------------------------------------------- spies ------ */
typedef struct {
    uint32_t boot, tx, rxMs, nowMs, occurrence, loop;
    uint8_t kind;
    uint16_t age;
    VssInputApplication result;    /* adapter outcome after the real callback */
} SpyEvent;

typedef struct {
    uint32_t boot, tx, rxMs, loop, occupantSequence;
    VssWarningBundleResult result;
} SpyWarning;

typedef struct {
    uint32_t update, loop, tick;
    uint8_t state;
    NetSubmitResult result;
} SpyPublish;

#define SPY_MAX 1024u
static SpyEvent s_ev[SPY_MAX];
static uint32_t s_evCount;
static SpyWarning s_wr[SPY_MAX];
static uint32_t s_wrCount;
static SpyPublish s_pub[SPY_MAX];
static uint32_t s_pubCount;
static uint32_t s_cbOutsideLoop, s_cbInIsr, s_cbThisLoop, s_cbMaxPerLoop;
static VssInputNetObservation s_spyIn;

void __real_VssApp_OnDomainEvent(const NetVssEvent *message, const NetCanRxMeta *meta);
void __real_VssApp_OnDomainWarningState(const NetVssWarningState *message, const NetCanRxMeta *meta);
NetSubmitResult __real_VssNet_PublishStatus(const NetVssStatus *message);
void __wrap_VssApp_OnDomainEvent(const NetVssEvent *message, const NetCanRxMeta *meta);
void __wrap_VssApp_OnDomainWarningState(const NetVssWarningState *message, const NetCanRxMeta *meta);
NetSubmitResult __wrap_VssNet_PublishStatus(const NetVssStatus *message);

static void callback_context(void)
{
    if (!s_inLoop) ++s_cbOutsideLoop;
    if (IntegBus_InIsr()) ++s_cbInIsr;
    ++s_cbThisLoop;
}

void __wrap_VssApp_OnDomainEvent(const NetVssEvent *message, const NetCanRxMeta *meta)
{
    SpyEvent *e = (s_evCount < SPY_MAX) ? &s_ev[s_evCount] : NULL;

    callback_context();
    __real_VssApp_OnDomainEvent(message, meta);
    if ((e != NULL) && (message != NULL) && (meta != NULL))
    {
        memset(e, 0, sizeof *e);
        e->boot = meta->source.boot_id;
        e->tx = meta->source.tx_sequence;
        e->rxMs = meta->received_at_ms;
        e->nowMs = VssTime_NowMs32();
        e->occurrence = message->occurrence_id;
        e->kind = message->event_kind;
        e->age = message->original_age_ms;
        e->loop = s_loop;
        (void)VssInputNet_Read(VSS_PRODUCT, &s_spyIn);
        e->result = s_spyIn.lastEvent;
    }
    ++s_evCount;
}

void __wrap_VssApp_OnDomainWarningState(const NetVssWarningState *message, const NetCanRxMeta *meta)
{
    SpyWarning *w = (s_wrCount < SPY_MAX) ? &s_wr[s_wrCount] : NULL;

    callback_context();
    __real_VssApp_OnDomainWarningState(message, meta);
    if ((w != NULL) && (message != NULL) && (meta != NULL))
    {
        memset(w, 0, sizeof *w);
        w->boot = meta->source.boot_id;
        w->tx = meta->source.tx_sequence;
        w->rxMs = meta->received_at_ms;
        w->loop = s_loop;
        w->occupantSequence = message->occupant_hazard_source_sequence;
        (void)VssInputNet_Read(VSS_PRODUCT, &s_spyIn);
        w->result = s_spyIn.lastWarning;
    }
    ++s_wrCount;
}

NetSubmitResult __wrap_VssNet_PublishStatus(const NetVssStatus *message)
{
    NetSubmitResult r = __real_VssNet_PublishStatus(message);

    if ((s_pubCount < SPY_MAX) && (message != NULL))
    {
        SpyPublish *p = &s_pub[s_pubCount];

        p->update = message->update_sequence;
        p->state = message->vss_state;
        p->loop = s_loop;
        p->tick = s_tick;
        p->result = r;
    }
    ++s_pubCount;
    return r;
}

/* ------------------------------------------------------- bus frame log ---- */
typedef struct {
    IntegFrame f;
    uint32_t loop, tick;
} LoggedFrame;

static LoggedFrame s_frames[SPY_MAX];
static uint32_t s_frameCount;

/* --------------------------------------------- node configuration hook ---- */
static uint32_t s_configCalls;
static bool s_configClockRegistered;
static uint32_t s_configAudioStarts;
static uint8_t s_configStage;

void IntegVss_OnNodeConfig(void)
{
    VssMainObservation m;

    ++s_configCalls;
    s_configClockRegistered = VssTime_IsRegistered();
    s_configAudioStarts = HalMock_AudioStartCalls();
    s_configStage = VssMain_Read(&m) ? m.stage : 0xFFu;
}

/* -------------------------------------------------------- DOMAIN stand-in -- */
typedef struct {
    uint32_t boot;
    uint32_t eventTx, warningTx;   /* TX_SEQUENCE per CAN ID, from 1 in each boot */
    uint32_t laneSequence;         /* every frame is a new evaluation */
    bool warnings;                 /* send VSS_WARNING_STATE every 50 ms */
    uint32_t nextWarningTick;
    uint32_t lastWarningTick;
    uint8_t occupant;              /* NetFlag */
    uint8_t rear;                  /* NetRearWarningState */
    /* Occupant lane without a valid evaluation (VSS contract C12): the last valid
     * source sequence is kept and the AGE grows from its time. */
    bool occupantLost;
    uint32_t occupantKeptSequence, occupantValidTick;
    /* Every lane repeats the last evaluation (same source sequences) with its
     * AGE grown from evaluationTick, computed at the frame's nominal 50 ms
     * time; the frame itself reaches the bus 0..2 ticks later (arbitration). */
    bool repeatEvaluation;
    uint32_t evaluationTick, nominalTick, repeats;
    IntegEvent lastEvent;
} Domain;

/* Arrival delay of each repeated frame, in board ticks (0, 1.25, 2.5 ms). */
static const uint8_t s_jitter[] = { 0u, 2u, 1u, 0u, 2u, 2u, 0u, 1u, 2u, 0u, 1u, 1u, 2u };

static Domain s_dom;

static IntegLane Lane(uint32_t boot, uint32_t sequence, uint8_t state)
{
    IntegLane l;

    memset(&l, 0, sizeof l);
    l.source_boot = boot;
    l.source_sequence = sequence;
    l.original_age_ms = 0u;
    l.state = state;
    l.quality = NET_QUALITY_OK;
    l.reason = NET_QUALITY_REASON_NONE;
    l.availability = NET_AVAILABILITY_AVAILABLE;
    return l;
}

/* The next VSS_WARNING_STATE of the current DOMAIN run (new evaluation). */
static IntegWarning NextWarning(void)
{
    IntegWarning w;
    uint32_t seq = s_dom.repeatEvaluation ? s_dom.laneSequence : ++s_dom.laneSequence;

    memset(&w, 0, sizeof w);
    w.boot_id = s_dom.boot;
    w.tx_sequence = ++s_dom.warningTx;
    w.antipinch = Lane(WINDOW_BOOT, seq, NET_FLAG_NO);
    w.occupant = Lane(CIS_BOOT, seq, s_dom.occupant);
    if (s_dom.occupantLost)
    {
        w.occupant.source_sequence = s_dom.occupantKeptSequence;
        w.occupant.original_age_ms =
            (uint16_t)(VssTime_TicksToMs(s_tick) - VssTime_TicksToMs(s_dom.occupantValidTick));
        w.occupant.quality = NET_QUALITY_STALE;
        w.occupant.reason = NET_QUALITY_REASON_STALE;
    }
    else
    {
        s_dom.occupantKeptSequence = seq;
        s_dom.occupantValidTick = s_tick;
    }
    w.rear = Lane(CIS_BOOT, seq, s_dom.rear);
    w.rear_detection_activation = NET_FLAG_YES;
    w.activation_meta_format = 1u;
    w.rear_activation_quality = NET_QUALITY_OK;
    w.rear_activation_reason = NET_QUALITY_REASON_NONE;
    w.rear_activation_sequence = seq;
    w.rear_activation_age_ms = 0u;
    if (s_dom.repeatEvaluation)
    {
        uint16_t age = (uint16_t)(VssTime_TicksToMs(s_dom.nominalTick) - VssTime_TicksToMs(s_dom.evaluationTick));

        w.antipinch.original_age_ms = age;
        w.occupant.original_age_ms = age;
        w.rear.original_age_ms = age;
        w.rear_activation_age_ms = age;
    }
    return w;
}

static bool SendWarning(void)
{
    IntegWarning w = NextWarning();

    s_dom.lastWarningTick = s_tick;
    return IntegBus_SendWarning(&w);
}

static IntegEvent EventOf(uint8_t kind, uint32_t occurrence, uint16_t ageMs)
{
    IntegEvent e;

    memset(&e, 0, sizeof e);
    e.boot_id = s_dom.boot;
    e.event_kind = kind;
    e.occurrence_id = occurrence;
    e.original_age_ms = ageMs;
    e.use_limit_ms = 500u;
    e.value_quality = NET_QUALITY_OK;
    e.input_availability = NET_AVAILABILITY_AVAILABLE;
    e.related_command_id = 0u;
    return e;
}

/* A new VSS_EVENT of the current DOMAIN run, from the CAN ISR at this tick. */
static bool SendEvent(uint8_t kind, uint32_t occurrence, uint16_t ageMs)
{
    IntegEvent e = EventOf(kind, occurrence, ageMs);

    e.tx_sequence = ++s_dom.eventTx;
    s_dom.lastEvent = e;
    return IntegBus_SendEvent(&e);
}

static bool s_domainSendFailed;

static void DomainService(void)
{
    uint32_t due = s_dom.nextWarningTick;

    if (s_dom.repeatEvaluation)
    {
        due += s_jitter[s_dom.repeats % (sizeof s_jitter / sizeof s_jitter[0])];
    }
    if (s_dom.warnings && (s_tick >= due))
    {
        s_dom.nominalTick = s_dom.nextWarningTick;
        if (!SendWarning())
        {
            s_domainSendFailed = true;
        }
        s_dom.nextWarningTick += WARNING_PERIOD_TICKS;
        if (s_dom.repeatEvaluation)
        {
            ++s_dom.repeats;
        }
    }
}

/* ---------------------------------------------------------- super-loop ---- */
/* The tick moves: boundary interrupts due by now, then the DOMAIN frames. */
static void AdvanceTick(void)
{
    ++s_tick;
    HalMock_SetTicks(s_tick);
    while (s_tick >= BoundaryTick(s_boundary + 1u))
    {
        ++s_boundary;
        HalMock_Boundary();
    }
    DomainService();
}

/* One VssMain_LoopOnce pass; afterwards the bus takes what the node sent. */
static void Loop(void)
{
    IntegFrame f;

    ++s_loop;
    s_cbThisLoop = 0u;
    s_inLoop = true;
    VssMain_LoopOnce();
    s_inLoop = false;
    if (s_cbThisLoop > s_cbMaxPerLoop)
    {
        s_cbMaxPerLoop = s_cbThisLoop;
    }
    while (IntegBus_TakeFrame(&f))
    {
        if (s_frameCount < SPY_MAX)
        {
            s_frames[s_frameCount].f = f;
            s_frames[s_frameCount].loop = s_loop;
            s_frames[s_frameCount].tick = s_tick;
        }
        ++s_frameCount;
    }
}

static void Step(void)
{
    AdvanceTick();
    Loop();
}

static void RunMs(uint32_t ms)
{
    uint64_t end = NowMs() + ms;

    while (NowMs() < end)
    {
        Step();
    }
}

/* ---------------------------------------------------------- observations -- */
static VssInputNetObservation s_in;
static VssPlaybackObservation s_view;
static VssFlowObservation s_flow;
static VssMainObservation s_main;

static void View(void)
{
    (void)VssInputNet_Read(VSS_PRODUCT, &s_in);
    (void)Playback_Read(&s_view);
    (void)Flow_Read(&s_flow);
    (void)VssMain_Read(&s_main);
}

static IntegNetDiag Net(void)
{
    IntegNetDiag d;

    memset(&d, 0, sizeof d);
    (void)IntegBus_ReadDiag(&d);
    return d;
}

static bool Playing(VssInputSignal signal, VssInputValue value)
{
    View();
    return s_view.present && (s_view.lastStartedSession == s_view.session) && !s_view.stopRequested &&
           (s_view.candidate.signal == signal) && (s_view.candidate.value == value);
}

static bool RunUntilPlaying(VssInputSignal signal, VssInputValue value, uint32_t maxMs)
{
    uint64_t end = NowMs() + maxMs;

    while (NowMs() < end)
    {
        if (Playing(signal, value))
        {
            return true;
        }
        Step();
    }
    return Playing(signal, value);
}

static bool RunUntilIdle(uint32_t maxMs)
{
    uint64_t end = NowMs() + maxMs;

    while (NowMs() < end)
    {
        View();
        if (!s_view.present)
        {
            return true;
        }
        Step();
    }
    View();
    return !s_view.present;
}

/* ---------------------------------------------------------------- boot ---- */
/* Register answers the VSS audio-control drivers read at boot
 * (src/driver/audio_control): CS2100 DEVICE_ID (not checked), DEVICE_CTRL
 * twice (UNLOCK bit 0x80 clear: PLL locked), SGTL5000 CHIP_ID (part 0xA0 in
 * the high byte). The read-modify-write reads after that may return 0, which
 * the stand-in serves once its queue is empty. */
static void QueueBringUp(void)
{
    static const uint8_t answers[] = { 0x00u, 0x00u, 0x00u, 0xA0u, 0x11u };

    HalMock_QueueRead(answers, (uint32_t)sizeof answers);
}

/* Power-on state of both sides, then the image's own boot sequence and its
 * first pass. */
static bool Boot(void)
{
    HalMock_Reset();
    AudioTx_ResetForTest();
    AudioControl_ResetForTest();
    VssTime_ResetForTest();
    Input_TestReset();
    Store_TestReset();
    StorePlayback_TestReset();
    VssInputNet_ResetForTest();
    IntegBus_Reset();
    memset(&s_dom, 0, sizeof s_dom);
    s_dom.boot = DOMAIN_BOOT_A;
    s_dom.occupant = NET_FLAG_NO;
    s_dom.rear = NET_REAR_WARNING_CLEAR;
    QueueBringUp();
    if (!VssMain_Boot())
    {
        return false;
    }
    s_tick = HalTimer_GetTicks();
    s_startTick = s_tick;
    s_boundary = 0u;
    s_dom.nextWarningTick = s_tick + WARNING_PERIOD_TICKS;
    Loop();
    return true;
}

/* Rules every case keeps: callbacks only inside a super-loop pass, never in
 * the CAN interrupt (C1); every ISR tick placed on the VSS clock (C2); the
 * output ran without fault (the loop stayed short enough, C7 / C8). */
static int Invariants(void)
{
    IntegNetDiag d = Net();

    CHECK(s_cbOutsideLoop == 0u);
    CHECK(s_cbInIsr == 0u);
    CHECK(VssTime_UnplacedTicks() == 0u);
    CHECK(d.tick_future_clamped == 0u);
    CHECK(AudioTx_FaultMask() == 0u);
    CHECK(!s_domainSendFailed);
    CHECK(s_evCount <= SPY_MAX && s_wrCount <= SPY_MAX && s_pubCount <= SPY_MAX && s_frameCount <= SPY_MAX);
    return 0;
}

static bool StatusFrameOk(const IntegFrame *f)
{
    return f->decoded && (f->id == 0x340u) && f->fd && f->brs && (f->length == 32u) &&
           (f->boot_id == INTEG_NODE_BOOT);
}

/* ---------------------------------------------------------------- cases --- */

/* VI1 boot order: VssMain_Boot calls Net_Init once, after VssTime_Init and
 * before the output starts; the node is READY on the application clock and
 * Net_MainFunction runs once per pass. The first reports reach the bus with
 * the hook BOOT_ID and TX from 1. */
static int vi1_boot_order(void)
{
    IntegNetDiag d;
    uint32_t k;

    CHECK(Boot());
    View();
    CHECK(s_main.stage == VSS_MAIN_STAGE_RUNNING);
    CHECK(s_main.boardOk && s_main.timerOk && s_main.audioControlOk && s_main.unmuteOk && s_main.outputStarted);
    CHECK(s_configCalls == 1u);
    CHECK(s_configClockRegistered);
    CHECK(s_configAudioStarts == 0u);
    CHECK(s_configStage == VSS_MAIN_STAGE_SERVICES);
    d = Net();
    CHECK(d.ready && d.init_calls == 1u && d.app_tick_time);
    CHECK(d.node_boot_id == INTEG_NODE_BOOT);
    CHECK(d.main_calls == s_loop);

    RunMs(1000u);
    d = Net();
    CHECK(d.init_calls == 1u && d.main_calls == s_loop);
    CHECK(s_configCalls == 1u);
    CHECK(s_frameCount >= 4u);
    for (k = 0u; k < s_frameCount; ++k)
    {
        CHECK(StatusFrameOk(&s_frames[k].f));
        CHECK(s_frames[k].f.tx_sequence == k + 1u);
        CHECK(s_frames[k].f.update_sequence == k + 1u);
    }
    CHECK(s_frames[0].f.vss_state == NET_VSS_STATE_READY);
    CHECK(s_frames[0].f.vss_availability == NET_VSS_AVAILABILITY_FULL);
    CHECK(s_frames[0].f.vss_accepting_events == NET_FLAG_YES);
    CHECK(s_frames[0].f.fault_mask == 0u && s_frames[0].f.vss_fault_active == NET_FLAG_NO);
    CHECK(Health_CurrentMask() == 0u);
    return Invariants();
}

/* VI2 clock axis (C2): the ISR stamps tick T, the pass comes 5 ms later;
 * VSS gets received_at_ms = VssTime_TickToMs32(T), not the pass time, and the
 * Event is taken and plays. The age stays the age at the receive time. */
static int vi2_clock_axis(void)
{
    uint32_t t;
    const SpyEvent *e;

    CHECK(Boot());
    RunMs(500u);
    AdvanceTick();
    t = s_tick;
    CHECK(SendEvent(NET_VSS_EVENT_WELCOME, 1u, 0u));
    CHECK(s_evCount == 0u);
    AdvanceTick();
    AdvanceTick();
    AdvanceTick();
    AdvanceTick();
    Loop();
    CHECK(s_evCount == 1u);
    e = &s_ev[0];
    CHECK(e->rxMs == VssTime_TickToMs32(t));
    CHECK(e->nowMs == VssTime_NowMs32());
    CHECK(e->nowMs - e->rxMs == 5u);
    CHECK(e->age == 0u);
    CHECK(e->boot == DOMAIN_BOOT_A && e->tx == 1u && e->occurrence == 1u);
    CHECK(e->kind == NET_VSS_EVENT_WELCOME);
    CHECK(e->result.validation == VSS_VALIDATION_APPLICABLE);
    CHECK(e->result.storeAdmission == VSS_STORE_ACCEPTED);
    View();
    CHECK(s_in.eventsForwarded == 1u && s_in.eventsDiscarded == 0u);
    CHECK(s_in.generationOpen && s_in.acceptedBoot == DOMAIN_BOOT_A);
    CHECK(RunUntilPlaying(VSS_SIGNAL_WELCOME, VSS_VALUE_EVENT, 300u));
    return Invariants();
}

/* VI3 TX per message (C3): Event TX 1..5, then Warning TX 1, then Event TX 6
 * and Warning TX 2. Every frame is delivered and taken: the two CAN IDs keep
 * their own order. */
static int vi3_tx_per_message(void)
{
    IntegNetDiag d;
    uint32_t i, lane;

    CHECK(Boot());
    RunMs(300u);
    for (i = 1u; i <= 5u; ++i)
    {
        CHECK(SendEvent((uint8_t)i, i, 0u));
        RunMs(20u);
    }
    CHECK(s_evCount == 5u && s_ev[4].tx == 5u);
    CHECK(SendWarning());
    RunMs(20u);
    CHECK(s_wrCount == 1u && s_wr[0].tx == 1u);
    CHECK(SendEvent(NET_VSS_EVENT_WELCOME, 6u, 0u));
    RunMs(20u);
    CHECK(SendWarning());
    RunMs(20u);
    CHECK(s_evCount == 6u && s_ev[5].tx == 6u);
    CHECK(s_wrCount == 2u && s_wr[1].tx == 2u);
    for (i = 0u; i < s_evCount; ++i)
    {
        CHECK(s_ev[i].result.validation == VSS_VALIDATION_APPLICABLE);
        CHECK(s_ev[i].result.storeAdmission == VSS_STORE_ACCEPTED);
    }
    for (i = 0u; i < s_wrCount; ++i)
    {
        CHECK(s_wr[i].result.frameValidation != VSS_VALIDATION_REJECTED);
        for (lane = 0u; lane < VSS_WARNING_LANES; ++lane)
        {
            CHECK(s_wr[i].result.lanes[lane].validation != VSS_VALIDATION_REJECTED);
        }
    }
    View();
    CHECK(s_in.eventsForwarded == 6u && s_in.eventsDiscarded == 0u);
    CHECK(s_in.warningFramesForwarded == 2u && s_in.warningFramesUnverified == 0u);
    CHECK(s_in.txRestartsSameBoot == 0u && s_in.generationChanges == 0u);
    d = Net();
    CHECK(d.event_delivered == 6u && d.warning_delivered == 2u && d.stale == 0u);
    return Invariants();
}

/* VI4 DOMAIN restart (C4, C6): run A, then run B with a new BOOT_ID and TX
 * from 1 again: VSS takes B over as a new generation (generationChanges +1).
 * A late frame of run A is refused and does not close B; B keeps working.
 * Network side (04_COMMON/10_DATA_AND_ID_RULES.md generation): each CAN ID
 * moves to B on its first valid B frame and passes it on; the late A frame
 * is an old generation and reaches no VSS callback. */
static int vi4_domain_reboot(void)
{
    IntegEvent late;
    uint32_t lateTx, events, old;
    uint64_t lastSession;

    CHECK(Boot());
    s_dom.warnings = true;
    RunMs(300u);
    CHECK(SendEvent(NET_VSS_EVENT_WELCOME, 1u, 0u));
    CHECK(RunUntilPlaying(VSS_SIGNAL_WELCOME, VSS_VALUE_EVENT, 300u));
    CHECK(RunUntilIdle(8000u));
    View();
    CHECK(s_in.generationOpen && s_in.acceptedBoot == DOMAIN_BOOT_A && s_in.generationChanges == 0u);
    lateTx = s_dom.eventTx + 1u;

    /* DOMAIN restarts: new BOOT_ID, TX numbers and occurrences from 1. */
    s_dom.boot = DOMAIN_BOOT_B;
    s_dom.eventTx = 0u;
    s_dom.warningTx = 0u;
    RunMs(100u);
    View();
    CHECK(s_in.acceptedBoot == DOMAIN_BOOT_B);
    CHECK(s_in.generationChanges == 1u);
    CHECK(SendEvent(NET_VSS_EVENT_GOODBYE, 1u, 0u));
    CHECK(RunUntilPlaying(VSS_SIGNAL_GOODBYE, VSS_VALUE_EVENT, 300u));
    CHECK(RunUntilIdle(8000u));

    /* A late frame of run A (next TX of A, a new occurrence of A). */
    View();
    lastSession = s_view.lastSession;
    events = s_evCount;
    late = EventOf(NET_VSS_EVENT_LOCK_COMPLETE, 2u, 0u);
    late.boot_id = DOMAIN_BOOT_A;
    late.tx_sequence = lateTx;
    old = Net().old_generation;
    CHECK(IntegBus_SendEvent(&late));
    RunMs(300u);
    View();
    CHECK(s_in.acceptedBoot == DOMAIN_BOOT_B && s_in.generationChanges == 1u);
    CHECK(!s_view.present && s_view.lastSession == lastSession);
    /* Refused by the Network already: no VSS callback for it. */
    CHECK(s_evCount == events && Net().old_generation == old + 1u);

    /* Run B goes on. */
    CHECK(SendEvent(NET_VSS_EVENT_UNLOCK_COMPLETE, 2u, 0u));
    CHECK(RunUntilPlaying(VSS_SIGNAL_UNLOCK_COMPLETE, VSS_VALUE_EVENT, 300u));
    View();
    CHECK(s_in.acceptedBoot == DOMAIN_BOOT_B && s_in.generationChanges == 1u);
    return Invariants();
}

/* VI5 retransmission: the same occurrence again with the next TX and its age
 * grown by the time since (inside and after the use limit) does not sound
 * again; the exact same frame again (same TX, same bytes) is not delivered. */
static int vi5_same_occurrence(void)
{
    IntegEvent re;
    IntegNetDiag d0, d;
    uint32_t t0, events;
    uint64_t session;

    CHECK(Boot());
    s_dom.warnings = true;
    RunMs(300u);
    AdvanceTick();
    t0 = s_tick;
    CHECK(SendEvent(NET_VSS_EVENT_WELCOME, 7u, 0u));
    re = s_dom.lastEvent;
    Loop();
    CHECK(RunUntilPlaying(VSS_SIGNAL_WELCOME, VSS_VALUE_EVENT, 300u));
    session = s_view.session;
    RunMs(100u);

    /* Inside the use limit, while it plays. */
    re.tx_sequence = ++s_dom.eventTx;
    re.original_age_ms = (uint16_t)(VssTime_NowMs32() - VssTime_TickToMs32(t0));
    CHECK(re.original_age_ms >= 100u && re.original_age_ms < re.use_limit_ms);
    CHECK(IntegBus_SendEvent(&re));
    CHECK(RunUntilIdle(8000u));
    CHECK(s_view.lastSession == session && s_view.lastStartedSession == session);
    CHECK(s_view.lastEnd == PLAYBACK_END_COMPLETED);

    /* After the use limit. */
    re.tx_sequence = ++s_dom.eventTx;
    re.original_age_ms = (uint16_t)(VssTime_NowMs32() - VssTime_TickToMs32(t0));
    CHECK(re.original_age_ms > re.use_limit_ms);
    CHECK(IntegBus_SendEvent(&re));
    RunMs(1000u);
    View();
    CHECK(!s_view.present && s_view.lastStartedSession == session);
    CHECK(s_in.eventsDiscarded == 0u);

    /* The exact same frame twice: the second one is a duplicate on the bus. */
    CHECK(SendEvent(NET_VSS_EVENT_LOCK_COMPLETE, 8u, 0u));
    re = s_dom.lastEvent;
    RunMs(10u);
    events = s_evCount;
    CHECK(events >= 2u && s_ev[events - 1u].occurrence == 8u);
    d0 = Net();
    CHECK(IntegBus_SendEvent(&re));
    RunMs(10u);
    d = Net();
    CHECK(s_evCount == events);
    CHECK(d.duplicates == d0.duplicates + 1u);
    CHECK(d.rx_delivered == d0.rx_delivered);
    CHECK(RunUntilPlaying(VSS_SIGNAL_LOCK_COMPLETE, VSS_VALUE_EVENT, 300u));
    session = s_view.session;
    CHECK(RunUntilIdle(8000u));
    RunMs(500u);
    View();
    CHECK(!s_view.present && s_view.lastStartedSession == session);
    return Invariants();
}

/* Sends every structure defect once, on top of the given valid frames. */
static int SendDefects(const IntegEvent *ev, const IntegWarning *w)
{
    CHECK(IntegBus_SendEventDefect(ev, INTEG_DEFECT_LONGER));
    CHECK(IntegBus_SendEventDefect(ev, INTEG_DEFECT_SHORTER));
    CHECK(IntegBus_SendEventDefect(ev, INTEG_DEFECT_CLASSIC));
    CHECK(IntegBus_SendEventDefect(ev, INTEG_DEFECT_EVENT_KIND_0));
    CHECK(IntegBus_SendEventDefect(ev, INTEG_DEFECT_EVENT_KIND_6));
    CHECK(IntegBus_SendEventDefect(ev, INTEG_DEFECT_BOOT_ZERO));
    RunMs(5u);
    CHECK(IntegBus_SendWarningDefect(w, INTEG_DEFECT_SHORTER));
    CHECK(IntegBus_SendWarningDefect(w, INTEG_DEFECT_CLASSIC));
    CHECK(IntegBus_SendWarningDefect(w, INTEG_DEFECT_META_FORMAT_2));
    CHECK(IntegBus_SendWarningDefect(w, INTEG_DEFECT_REAR_STATE_3));
    CHECK(IntegBus_SendWarningDefect(w, INTEG_DEFECT_BOOT_ZERO));
    RunMs(20u);
    return 0;
}

#define DEFECT_FRAMES 11u

/* VI6 structure first (C5): DLC errors, Classic CAN, enum out of range,
 * activation_meta_format 2, BOOT_ID 0 - before INPUT is open and after it.
 * No VSS callback, no generation opened or changed; the Network counts each
 * frame as refused. The valid frames are still new afterwards. */
static int vi6_structure_rejected(void)
{
    IntegEvent ev;
    IntegWarning w;
    IntegNetDiag d0, d;

    CHECK(Boot());
    RunMs(300u);
    ev = EventOf(NET_VSS_EVENT_WELCOME, 1u, 0u);
    ev.tx_sequence = 1u;
    w = NextWarning();

    d0 = Net();
    CHECK(SendDefects(&ev, &w) == 0);
    d = Net();
    CHECK(s_evCount == 0u && s_wrCount == 0u);
    CHECK(d.rx_frames == d0.rx_frames + DEFECT_FRAMES);
    CHECK(d.rx_invalid == d0.rx_invalid + DEFECT_FRAMES);
    CHECK(d.rx_delivered == d0.rx_delivered);
    View();
    CHECK(!s_in.generationOpen && s_in.generationChanges == 0u);
    CHECK(s_in.eventsForwarded == 0u && s_in.warningFramesForwarded == 0u);

    /* Nothing was admitted: the valid TX 1 frames open INPUT for this boot. */
    CHECK(IntegBus_SendEvent(&ev));
    CHECK(IntegBus_SendWarning(&w));
    RunMs(20u);
    CHECK(s_evCount == 1u && s_wrCount == 1u);
    View();
    CHECK(s_in.generationOpen && s_in.acceptedBoot == DOMAIN_BOOT_A && s_in.generationChanges == 0u);

    /* With INPUT open, the same defects on the next TX change nothing. */
    ev.tx_sequence = 2u;
    ev.occurrence_id = 2u;
    w = NextWarning();
    d0 = Net();
    CHECK(SendDefects(&ev, &w) == 0);
    d = Net();
    CHECK(s_evCount == 1u && s_wrCount == 1u);
    CHECK(d.rx_frames == d0.rx_frames + DEFECT_FRAMES);
    CHECK(d.rx_invalid == d0.rx_invalid + DEFECT_FRAMES);
    CHECK(d.rx_delivered == d0.rx_delivered);
    View();
    CHECK(s_in.acceptedBoot == DOMAIN_BOOT_A && s_in.generationChanges == 0u);
    CHECK(s_in.eventsForwarded == 1u && s_in.warningFramesForwarded == 1u);
    CHECK(s_in.eventsDiscarded == 0u && s_in.warningFramesUnverified == 0u);
    return Invariants();
}

/* VI7 warning stream lost ("VSS 연동 확인 시험" (VSS_NODE_INTEGRATION.md),
 * VSS contract C11): the occupant hazard plays while VSS_WARNING_STATE comes
 * every 50 ms. The frames stop: 150 ms after the last receive time VSS takes
 * the input as lost, the Hold of 500 ms runs from there, then the warning
 * stops. */
static int vi7_warning_rx_loss(void)
{
    VssWarningObservation obs;
    uint64_t session, lastRx, lossAt;

    CHECK(Boot());
    s_dom.warnings = true;
    RunMs(300u);
    s_dom.occupant = NET_FLAG_YES;
    CHECK(RunUntilPlaying(VSS_SIGNAL_OCCUPANT_HAZARD, VSS_VALUE_ACTIVE, 300u));
    session = s_view.session;
    RunMs(500u);
    CHECK(Playing(VSS_SIGNAL_OCCUPANT_HAZARD, VSS_VALUE_ACTIVE) && s_view.session == session);
    CHECK(s_wrCount >= 15u);

    /* The DOMAIN frames stop after the next one. */
    do
    {
        Step();
    } while (s_dom.lastWarningTick != s_tick);
    s_dom.warnings = false;
    lastRx = VssTime_TicksToMs(s_dom.lastWarningTick);
    CHECK(s_wr[s_wrCount - 1u].rxMs == (uint32_t)lastRx);
    lossAt = lastRx + RX_WATCH_MS;
    while (NowMs() < lossAt + OCCUPANT_HOLD_MS - 10u)
    {
        Step();
        View();
        CHECK(s_view.present && s_view.session == session && !s_view.stopRequested);
        CHECK(Store_ReadWarning(VSS_PRODUCT, VSS_SIGNAL_OCCUPANT_HAZARD, &obs));
        /* Seen by the first flow pass at or after the watch time. */
        if (NowMs() < lossAt)
        {
            CHECK(!obs.firstLossPresent);
        }
        else if (NowMs() >= lossAt + VSS_CFG_FLOW_PERIOD_MS)
        {
            CHECK(obs.firstLossPresent);
        }
        if (obs.firstLossPresent)
        {
            CHECK(obs.firstLossAt.earliest == lossAt);
        }
    }
    CHECK(RunUntilIdle(100u));
    CHECK(s_view.lastSession == session && s_view.lastEnd == PLAYBACK_END_STOPPED);
    CHECK(s_view.lastEndAtMs >= lossAt + OCCUPANT_HOLD_MS);
    RunMs(500u);
    View();
    CHECK(!s_view.present && s_view.lastSession == session);
    return Invariants();
}

/* Index of the first logged frame at or after this pass. */
static uint32_t FrameAtLoop(uint32_t loop)
{
    uint32_t k;

    for (k = 0u; k < s_frameCount && k < SPY_MAX; ++k)
    {
        if (s_frames[k].loop >= loop)
        {
            return k;
        }
    }
    return s_frameCount;
}

/* The publication of this report number, or NULL. */
static const SpyPublish *PublishOf(uint32_t update)
{
    uint32_t i;

    for (i = s_pubCount; i > 0u; --i)
    {
        if ((i - 1u < SPY_MAX) && (s_pub[i - 1u].update == update) && (s_pub[i - 1u].result == NET_SUBMIT_OK))
        {
            return &s_pub[i - 1u];
        }
    }
    return NULL;
}

/* VI8 status reports (0x340 not repeated by the Network): 10 s idle gives
 * 40 +- 2 frames, each a VSS evaluation (UPDATE_SEQUENCE +1, TX +1, BOOT from
 * the hook), sent in the pass after VSS submitted it. A state change (Event
 * playback starts) adds one frame between two periodic ones. */
static int vi8_status_period(void)
{
    uint32_t loop0, first, n, k, before;
    const LoggedFrame *p, *prev;
    const SpyPublish *pub;
    uint64_t end;

    CHECK(Boot());
    s_dom.warnings = true;
    RunMs(1000u);
    loop0 = s_loop + 1u;
    RunMs(10000u);
    first = FrameAtLoop(loop0);
    CHECK(first > 0u);
    n = s_frameCount - first;
    printf("vi8: %u VSS_STATUS frames in 10 s\n", (unsigned)n);
    CHECK(n >= 38u && n <= 42u);
    for (k = first; k < s_frameCount; ++k)
    {
        const IntegFrame *f = &s_frames[k].f;

        CHECK(StatusFrameOk(f));
        CHECK(f->vss_state == NET_VSS_STATE_READY);
        CHECK(f->tx_sequence == s_frames[k - 1u].f.tx_sequence + 1u);
        CHECK(f->update_sequence == s_frames[k - 1u].f.update_sequence + 1u);
        pub = PublishOf(f->update_sequence);
        CHECK(pub != NULL);
        CHECK(s_frames[k].loop == pub->loop + 1u);
    }

    /* A state change 50 ms after a periodic report. */
    before = s_frameCount;
    while (s_frameCount == before)
    {
        Step();
    }
    RunMs(50u);
    before = s_frameCount;
    CHECK(SendEvent(NET_VSS_EVENT_WELCOME, 1u, 0u));
    end = NowMs() + 300u;
    while ((NowMs() < end) && ((s_frameCount == before) ||
           (s_frames[s_frameCount - 1u].f.vss_state != NET_VSS_STATE_PLAYING)))
    {
        Step();
    }
    CHECK(s_frameCount == before + 1u);
    p = &s_frames[s_frameCount - 1u];
    prev = &s_frames[s_frameCount - 2u];
    CHECK(StatusFrameOk(&p->f) && p->f.vss_state == NET_VSS_STATE_PLAYING);
    CHECK(p->f.update_sequence == prev->f.update_sequence + 1u);
    CHECK(p->f.tx_sequence == prev->f.tx_sequence + 1u);
    CHECK((p->tick - prev->tick) < 200u);   /* < 250 ms: between two periodic reports */
    pub = PublishOf(p->f.update_sequence);
    CHECK(pub != NULL && pub->state == NET_VSS_STATE_PLAYING);
    CHECK(p->loop == pub->loop + 1u);
    return Invariants();
}

/* VI9 link locked (C10): while the CAN link is closed after a bus-off
 * (1500 ms hold in this build) VssNet_PublishStatus answers NOT_READY; VSS
 * keeps the report and offers it again with the same number in every flow
 * pass (a newer evaluation may take its place), drops nothing, and nothing
 * more goes on the bus. When the link opens again the report that waits goes
 * out with its number and the next TX_SEQUENCE; nothing old is sent again. */
static int vi9_link_locked_resubmit(void)
{
    IntegNetDiag d;
    uint32_t frames, firstPub, seq, i, refused, accepted;
    uint64_t end;

    CHECK(Boot());
    s_dom.warnings = true;
    RunMs(1000u);
    CHECK(s_frameCount >= 4u);
    IntegBus_RaiseBusOff();
    Step();
    d = Net();
    CHECK(d.busoff_events == 1u);
    frames = s_frameCount;

    firstPub = s_pubCount;
    end = NowMs() + 300u;
    while ((NowMs() < end) && (s_pubCount == firstPub))
    {
        Step();
    }
    CHECK(s_pubCount > firstPub);
    CHECK(s_pub[firstPub].result == NET_SUBMIT_NOT_READY);
    seq = s_pub[firstPub].update;
    View();
    CHECK(s_flow.statusWaiting && s_flow.waitingSequence == seq);
    CHECK(s_flow.lastRefusal == NET_SUBMIT_NOT_READY);

    /* Offered again with the same number in the next flow passes. */
    RunMs(100u);
    CHECK(s_pubCount - firstPub >= 9u);
    for (i = firstPub; i < s_pubCount; ++i)
    {
        CHECK(s_pub[i].result == NET_SUBMIT_NOT_READY && s_pub[i].update == seq);
    }
    View();
    CHECK(s_flow.statusResubmitted >= 8u && s_flow.statusDropped == 0u);

    /* Later evaluations take its place; nothing is dropped. */
    RunMs(600u);
    refused = 0u;
    for (i = firstPub; i < s_pubCount; ++i)
    {
        CHECK(s_pub[i].result == NET_SUBMIT_NOT_READY);
        CHECK(i == firstPub || s_pub[i].update == s_pub[i - 1u].update ||
              s_pub[i].update == s_pub[i - 1u].update + 1u);
        ++refused;
    }
    View();
    CHECK(s_flow.statusDropped == 0u && s_flow.statusWaiting);
    CHECK(s_flow.statusReplaced >= 2u);
    CHECK(s_flow.lastRefusal == NET_SUBMIT_NOT_READY);
    CHECK(s_frameCount == frames);
    d = Net();
    CHECK(d.status_not_accepted >= refused);
    CHECK(d.last_status_result == (uint8_t)NET_SUBMIT_NOT_READY);
    CHECK(d.busoff_recoveries == 0u);
    CHECK(!s_main.outputFailed);

    /* The hold ends: the waiting report is accepted and goes out. */
    end = NowMs() + 1500u;
    while ((NowMs() < end) && (s_frameCount == frames))
    {
        Step();
    }
    CHECK(s_frameCount > frames && frames >= 1u);
    d = Net();
    CHECK(d.busoff_recoveries == 1u && d.busoff_events == 1u);
    accepted = s_pubCount;
    for (i = firstPub; i < s_pubCount; ++i)
    {
        if (s_pub[i].result == NET_SUBMIT_OK)
        {
            accepted = i;
            break;
        }
    }
    CHECK(accepted < s_pubCount);
    CHECK(s_frames[frames].f.decoded);
    CHECK(s_frames[frames].f.update_sequence == s_pub[accepted].update);
    CHECK(s_frames[frames].f.tx_sequence == s_frames[frames - 1u].f.tx_sequence + 1u);
    View();
    CHECK(!s_flow.statusWaiting && s_flow.statusDropped == 0u);
    return Invariants();
}

/* VI10 receive flood (C8): a burst between two passes (30 frames, or one less
 * than the ISR ring holds when the ring is smaller) that is larger than the
 * drain budget, and a sustained flood above the drain budget; no pass
 * delivers more frames than the budget, the burst arrives complete and in
 * order, the link stays up. The host run time is a reference value only
 * (measured on the board). */
static int vi10_rx_flood_budget(void)
{
    IntegNetDiag d;
    uint32_t i, j, first, frames, burst;

    CHECK(Boot());
    s_dom.warnings = true;
    RunMs(300u);
    s_dom.warnings = false;
    RunMs(20u);
    d = Net();
    CHECK(d.drain_budget >= 1u && d.ring_capacity >= 2u);
    burst = (d.ring_capacity - 1u < 30u) ? d.ring_capacity - 1u : 30u;
    CHECK(burst > d.drain_budget); /* the burst needs more than one pass */

    first = s_wrCount;
    s_cbMaxPerLoop = 0u;
    for (i = 0u; i < burst; ++i)
    {
        CHECK(SendWarning());
    }
    Loop();
    CHECK(s_wrCount - first == d.drain_budget || s_wrCount - first == d.drain_budget - 1u);
    RunMs(10u);
    CHECK(s_wrCount - first == burst);
    for (i = first + 1u; i < s_wrCount; ++i)
    {
        CHECK(s_wr[i].tx == s_wr[i - 1u].tx + 1u);
        CHECK(s_wr[i].result.frameValidation != VSS_VALIDATION_REJECTED);
    }
    d = Net();
    CHECK(d.ring_overflow == 0u);
    CHECK(s_cbMaxPerLoop <= d.drain_budget && d.main_drained_max <= d.drain_budget);

    /* Sustained: 24 frames per tick for 40 ticks. */
    frames = s_frameCount;
    for (j = 0u; j < 40u; ++j)
    {
        AdvanceTick();
        for (i = 0u; i < 24u; ++i)
        {
            CHECK(SendWarning());
        }
        Loop();
    }
    RunMs(20u);
    d = Net();
    CHECK(s_cbMaxPerLoop <= d.drain_budget && d.main_drained_max <= d.drain_budget);
    CHECK(d.ring_overflow > 0u);
    /* Still working afterwards: the link is not locked, new frames are
     * delivered and taken, and the status reports go on. The first frame
     * stored after a ring loss is complete and is delivered as received
     * (CAN_PORT_RECOVERY.md, RX overflow: the loss is recorded, nothing is
     * synthesized), and so is the next one. */
    first = s_wrCount;
    CHECK(SendWarning());
    RunMs(20u);
    CHECK(SendWarning());
    RunMs(300u);
    CHECK(s_wrCount == first + 2u);
    CHECK(d.rx_loss_events > 0u);
    CHECK(s_wr[s_wrCount - 1u].tx == s_dom.warningTx);
    CHECK(s_wr[s_wrCount - 1u].result.frameValidation != VSS_VALIDATION_REJECTED);
    CHECK(s_frameCount > frames);
    View();
    CHECK(s_in.warningFramesUnverified == 0u);
    printf("vi10: drain budget %u, most frames delivered in one pass %u, ring overflow %u, "
           "Net_MainFunction max %u us (host reference)\n", (unsigned)d.drain_budget,
           (unsigned)s_cbMaxPerLoop, (unsigned)d.ring_overflow, (unsigned)d.main_max_us);
    return Invariants();
}

/* VI11 C12 with a new evaluation on another lane: the occupant hazard plays;
 * then the occupant lane loses its quality (the last valid source sequence
 * kept, STALE, AGE growing, VSS contract C12) while the rear lane brings new
 * evaluations and turns EMERGENCY. 04_COMMON/10_DATA_AND_ID_RULES.md (repeat
 * and current quality): such a frame is accepted and delivered; VSS/10_DATA.md:
 * the lanes are independent. Every frame reaches VSS, the warning stream has
 * no gap, and the rear evaluation is not held behind the lost lane. */
static int vi11_warning_quality_drop(void)
{
    VssWarningObservation obs;
    IntegNetDiag d0, d;
    uint32_t first, sent, kept, i;

    CHECK(Boot());
    s_dom.warnings = true;
    RunMs(300u);
    s_dom.occupant = NET_FLAG_YES;
    CHECK(RunUntilPlaying(VSS_SIGNAL_OCCUPANT_HAZARD, VSS_VALUE_ACTIVE, 300u));
    RunMs(100u);
    d0 = Net();
    first = s_wrCount;
    sent = s_dom.warningTx;
    kept = s_dom.occupantKeptSequence;
    s_dom.occupantLost = true;
    s_dom.rear = NET_REAR_WARNING_EMERGENCY;
    CHECK(RunUntilPlaying(VSS_SIGNAL_REAR_RISK, VSS_VALUE_EMERGENCY, 600u));
    RunMs(300u);
    d = Net();
    CHECK(s_dom.warningTx - sent >= 6u);
    /* Every frame of the lost lane delivered, none held. */
    CHECK(s_wrCount - first == s_dom.warningTx - sent);
    CHECK(d.rx_held == d0.rx_held && d.duplicates == d0.duplicates);
    CHECK(d.warning_delivered == d0.warning_delivered + (s_dom.warningTx - sent));
    for (i = first; i < s_wrCount; ++i)
    {
        CHECK(s_wr[i].occupantSequence == kept);
        CHECK(s_wr[i].tx == s_wr[i - 1u].tx + 1u);
        CHECK(s_wr[i].result.frameValidation != VSS_VALIDATION_REJECTED);
    }
    /* The rear lane's stream never looked lost to VSS. */
    CHECK(Store_ReadWarning(VSS_PRODUCT, VSS_SIGNAL_REAR_RISK, &obs));
    CHECK(!obs.firstLossPresent);
    CHECK(Playing(VSS_SIGNAL_REAR_RISK, VSS_VALUE_EMERGENCY));
    return Invariants();
}

/* VI12 one evaluation repeated (C11): the occupant hazard plays; then DOMAIN
 * stops evaluating anew and repeats the last evaluation every 50 ms for one
 * second (same source sequences, new TX_SEQUENCE, AGE grown from the
 * evaluation), each frame reaching the bus 0..2.5 ms after its nominal time.
 * 04_COMMON/10_DATA_AND_ID_RULES.md (repeat and current quality) and the
 * receive delivery rule: every repetition reaches VssApp_OnDomainWarningState,
 * so the VSS receive watch (150 ms) never fires and the warning keeps playing
 * (without them VSS would stop 150 + 500 ms after the last new evaluation).
 * The Network counts them as repetitions, not duplicates; a late frame's AGE
 * is raised to the accumulated bound within the CAN allowance, never held. */
static int vi12_warning_repeat_keeps_vss(void)
{
    VssWarningObservation obs;
    IntegNetDiag d0, d;
    uint64_t session, evaluationMs;
    uint32_t first, sent, kept, i;

    CHECK(Boot());
    s_dom.warnings = true;
    RunMs(300u);
    s_dom.occupant = NET_FLAG_YES;
    CHECK(RunUntilPlaying(VSS_SIGNAL_OCCUPANT_HAZARD, VSS_VALUE_ACTIVE, 300u));
    session = s_view.session;
    /* The next new evaluation is the last one. */
    do
    {
        Step();
    } while (s_dom.lastWarningTick != s_tick);
    s_dom.repeatEvaluation = true;
    s_dom.evaluationTick = s_dom.lastWarningTick;
    kept = s_dom.laneSequence;
    evaluationMs = VssTime_TicksToMs(s_dom.evaluationTick);
    d0 = Net();
    first = s_wrCount;
    sent = s_dom.warningTx;
    while (NowMs() < evaluationMs + 1000u)
    {
        Step();
        CHECK(Playing(VSS_SIGNAL_OCCUPANT_HAZARD, VSS_VALUE_ACTIVE));
        CHECK(s_view.session == session && !s_view.stopRequested);
        CHECK(Store_ReadWarning(VSS_PRODUCT, VSS_SIGNAL_OCCUPANT_HAZARD, &obs));
        /* The occupant TTL (1000 ms from the evaluation) is not reached yet. */
        if (NowMs() < evaluationMs + 900u)
        {
            CHECK(!obs.firstLossPresent);
        }
    }
    d = Net();
    CHECK(s_dom.warningTx - sent >= 19u);
    /* Every repetition delivered, as a repetition. */
    CHECK(s_wrCount - first == s_dom.warningTx - sent);
    CHECK(d.repeat_delivered - d0.repeat_delivered == s_dom.warningTx - sent);
    CHECK(d.warning_delivered - d0.warning_delivered == s_dom.warningTx - sent);
    CHECK(d.duplicates == d0.duplicates && d.rx_held == d0.rx_held && d.rx_delivered == d0.rx_delivered);
    CHECK(d.repeat_age_raised > d0.repeat_age_raised);
    for (i = first; i < s_wrCount; ++i)
    {
        CHECK(s_wr[i].occupantSequence == kept);
        CHECK(s_wr[i].tx == s_wr[i - 1u].tx + 1u);
        CHECK(s_wr[i].result.frameValidation != VSS_VALIDATION_REJECTED);
    }
    CHECK(Playing(VSS_SIGNAL_OCCUPANT_HAZARD, VSS_VALUE_ACTIVE) && s_view.session == session);
    return Invariants();
}

/* vi0: the two header model. VSS compiles with vss_net_api.h, the Network
 * with network/app/net_if_*.h; the same list (vss_integ_layout.h) expanded on
 * both sides must give the same sizes, field offsets / sizes and codes. */
#define LAYOUT_PUT(x) do { if (n < cap) out[n] = (uint32_t)(x); n++; } while (0);
#define LAYOUT_SIZE(T) LAYOUT_PUT(sizeof(T))
#define LAYOUT_FIELD(T, f) LAYOUT_PUT(offsetof(T, f)) LAYOUT_PUT(sizeof(((T *)0)->f))
#define LAYOUT_VALUE(v) LAYOUT_PUT(v)
#define NAME_SIZE(T) "sizeof(" #T ")",
#define NAME_FIELD(T, f) "offsetof(" #T ", " #f ")", "sizeof(" #T "." #f ")",
#define NAME_VALUE(v) #v,

static size_t VssApiLayout(uint32_t *out, size_t cap)
{
    size_t n = 0u;
    VSS_INTEG_API_LAYOUT(LAYOUT_SIZE, LAYOUT_FIELD, LAYOUT_VALUE)
    return n;
}

static int vi0_api_layout(void)
{
    static const char *const names[] = { VSS_INTEG_API_LAYOUT(NAME_SIZE, NAME_FIELD, NAME_VALUE) };
    static uint32_t vss[VSS_INTEG_LAYOUT_MAX], net[VSS_INTEG_LAYOUT_MAX];
    const size_t count = sizeof names / sizeof names[0];
    size_t nv = VssApiLayout(vss, VSS_INTEG_LAYOUT_MAX);
    size_t nn = IntegBus_ApiLayout(net, VSS_INTEG_LAYOUT_MAX);
    size_t i;

    CHECK(count <= VSS_INTEG_LAYOUT_MAX);
    CHECK(nv == count);
    CHECK(nn == count);
    for (i = 0u; i < count; ++i)
    {
        if (vss[i] != net[i])
        {
            fprintf(stderr, "%s: first layout difference %s: VSS %lu, Network %lu\n", s_case, names[i],
                    (unsigned long)vss[i], (unsigned long)net[i]);
            return 1;
        }
    }
    printf("%s: %lu entries identical\n", s_case, (unsigned long)count);
    return 0;
}

int main(int argc, char **argv)
{
    static const struct {
        const char *name;
        int (*run)(void);
    } cases[] = {
        { "vi0_api_layout", vi0_api_layout },
        { "vi1_boot_order", vi1_boot_order },
        { "vi2_clock_axis", vi2_clock_axis },
        { "vi3_tx_per_message", vi3_tx_per_message },
        { "vi4_domain_reboot", vi4_domain_reboot },
        { "vi5_same_occurrence", vi5_same_occurrence },
        { "vi6_structure_rejected", vi6_structure_rejected },
        { "vi7_warning_rx_loss", vi7_warning_rx_loss },
        { "vi8_status_period", vi8_status_period },
        { "vi9_link_locked_resubmit", vi9_link_locked_resubmit },
        { "vi10_rx_flood_budget", vi10_rx_flood_budget },
        { "vi11_warning_quality_drop", vi11_warning_quality_drop },
        { "vi12_warning_repeat_keeps_vss", vi12_warning_repeat_keeps_vss }
    };
    size_t i;

    if (argc != 2)
    {
        fprintf(stderr, "usage: %s <case>\n", argv[0]);
        return 2;
    }
    for (i = 0u; i < sizeof cases / sizeof cases[0]; ++i)
    {
        if (strcmp(argv[1], cases[i].name) == 0)
        {
            s_case = cases[i].name;
            return cases[i].run();
        }
    }
    fprintf(stderr, "unknown case %s\n", argv[1]);
    return 2;
}
