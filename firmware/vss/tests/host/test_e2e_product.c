/* Product path end to end: the test acts as the Network module, calling
 * VssApp_OnDomainEvent / VssApp_OnDomainWarningState with DOMAIN messages,
 * and runs the super-loop steps around the flow: audio stream service,
 * message delivery, flow pass, codec mute after a transmit fault. Below it
 * run the real adapter, INPUT, STORE, selection, playback, audio stream,
 * transmit driver and health, on the HAL stand-in that raises the output
 * boundaries every 1024 frames at 48 kHz. The Network stand-in keeps the
 * status reports.
 *
 * One run, in order: boot and first reports; 200 one-shot events played
 * back to back; warning preemption, release and re-arbitration; a DOMAIN
 * restart with a late duplicate of the old run; a refused status report;
 * the output stalling. Then a second run whose boot sequence reported an
 * initialization fault. */

#include "test_util.h"
#include "hal_mock.h"

#include "../../src/app/vss_flow.h"
#include "../../src/service/input/vss_input_net.h"
#include "../../src/service/health/vss_health.h"
#include "../../src/service/audio_stream/audio_stream.h"
#include "../../src/service/asset/audio_asset.h"
#include "../../src/driver/audio_tx/audio_tx.h"
#include "../../src/driver/audio_control/audio_control.h"
#include "../../src/net_if/net_stub.h"
#include "../../src/infra/vss_time.h"

#include <string.h>

/* ---- time line --------------------------------------------------------- */

static uint32_t s_boundary;
static bool s_boundariesStopped;
static uint32_t s_stallTick;

static uint32_t BoundaryTick(uint32_t k)
{
    return (uint32_t)(((uint64_t)k * 256u) / 15u);
}

static uint64_t NowMs(void)
{
    return VssTime_NowMs();
}

static VssTimeEvidence Now(void)
{
    VssTimeEvidence t;
    uint64_t ms = VssTime_NowMs();

    t.domain = VSS_PRODUCT;
    t.epoch = 1u;
    t.earliest = ms;
    t.latest = ms;
    t.continuity = VSS_CONTINUITY_COMPARABLE;
    return t;
}

/* ---- DOMAIN stand-in --------------------------------------------------- */

typedef struct
{
    uint32_t boot;
    uint32_t eventTx;
    uint32_t warningTx;
    uint32_t laneSequence;
    NetFlag antipinch;
    NetFlag occupant;
    NetRearWarningState rear;
    uint64_t nextWarningMs;
} Domain;

static Domain s_domain;
static bool s_inputThisStep;

static NetCanRxMeta Meta(uint32_t boot, uint32_t tx)
{
    NetCanRxMeta m;

    m.source.boot_id = boot;
    m.source.tx_sequence = tx;
    m.received_at_ms = VssTime_NowMs32();
    return m;
}

static NetVssEvent EventMessage(NetVssEventKind kind, uint32_t occurrence)
{
    NetVssEvent e;

    memset(&e, 0, sizeof e);
    e.event_kind = kind;
    e.occurrence_id = occurrence;
    e.original_age_ms = 0u;
    e.use_limit_ms = 500u;
    e.value_quality = NET_QUALITY_OK;
    e.input_availability = NET_AVAILABILITY_AVAILABLE;
    return e;
}

/* Sends one event of the current DOMAIN run. Returns its TX number. */
static uint32_t SendEvent(NetVssEventKind kind, uint32_t occurrence)
{
    NetVssEvent e = EventMessage(kind, occurrence);
    NetCanRxMeta m = Meta(s_domain.boot, ++s_domain.eventTx);

    VssApp_OnDomainEvent(&e, &m);
    s_inputThisStep = true;
    return s_domain.eventTx;
}

static void SendWarning(void)
{
    NetVssWarningState w;
    NetCanRxMeta m;
    uint32_t seq = ++s_domain.laneSequence;

    memset(&w, 0, sizeof w);
    w.antipinch_source_boot = 21u;
    w.antipinch_source_sequence = seq;
    w.antipinch_state = s_domain.antipinch;
    w.occupant_hazard_source_boot = 22u;
    w.occupant_hazard_source_sequence = seq;
    w.occupant_hazard_state = s_domain.occupant;
    w.rear_obstacle_source_boot = 23u;
    w.rear_obstacle_source_sequence = seq;
    w.rear_obstacle_state = s_domain.rear;
    w.rear_detection_activation = NET_FLAG_YES;
    w.activation_meta_format = 1u;
    w.rear_activation_sequence = seq;
    m = Meta(s_domain.boot, ++s_domain.warningTx);
    VssApp_OnDomainWarningState(&w, &m);
    s_inputThisStep = true;
}

/* ---- Network stand-in log ---------------------------------------------- */

#define LOG_CAPACITY 8192u

typedef struct
{
    NetVssStatus status;
    uint64_t atMs;
} LoggedStatus;

static LoggedStatus s_log[LOG_CAPACITY];
static size_t s_logCount;
static uint32_t s_seenPublishCount;
static bool s_logOverflow;

static void CollectStatus(void)
{
    uint32_t count = NetStub_PublishCount();

    if (count == s_seenPublishCount)
    {
        return;
    }
    CHECK_EQ_U(count, s_seenPublishCount + 1u);   /* at most one report per pass */
    s_seenPublishCount = count;
    if (s_logCount == LOG_CAPACITY)
    {
        s_logOverflow = true;
        return;
    }
    CHECK(NetStub_ReadLastStatus(&s_log[s_logCount].status));
    s_log[s_logCount].atMs = NowMs();
    s_logCount++;
}

static const NetVssStatus *LastStatus(void)
{
    return (s_logCount != 0u) ? &s_log[s_logCount - 1u].status : NULL;
}

/* ---- super-loop stand-in ------------------------------------------------ */

static VssPlaybackObservation s_view;
static VssFlowProgress s_progress;

/* Playback session log. */
#define SESSION_LOG_CAPACITY 512u

typedef struct
{
    uint64_t session;
    VssCandidateMode mode;
    VssInputSignal signal;
    VssInputValue value;
    uint32_t generation;
    uint32_t occurrence;
    PlaybackEnd end;
    bool ended;
} SessionRecord;

static SessionRecord s_sessions[SESSION_LOG_CAPACITY];
static size_t s_sessionCount;
static uint64_t s_loggedLastEnd;

static SessionRecord *FindSession(uint64_t session)
{
    size_t i;

    for (i = s_sessionCount; i > 0u; --i)
    {
        if (s_sessions[i - 1u].session == session)
        {
            return &s_sessions[i - 1u];
        }
    }
    return NULL;
}

static void LogSessions(void)
{
    CHECK(Playback_Read(&s_view));
    if (s_view.present && (FindSession(s_view.session) == NULL) && (s_sessionCount < SESSION_LOG_CAPACITY))
    {
        SessionRecord *r = &s_sessions[s_sessionCount++];

        memset(r, 0, sizeof *r);
        r->session = s_view.session;
        r->mode = s_view.candidate.mode;
        r->signal = s_view.candidate.signal;
        r->value = s_view.candidate.value;
        r->generation = s_view.candidate.key.generation;
        r->occurrence = s_view.candidate.key.occurrenceId;
    }
    if (s_view.lastSession != s_loggedLastEnd)
    {
        SessionRecord *r = FindSession(s_view.lastSession);

        s_loggedLastEnd = s_view.lastSession;
        CHECK(r != NULL);   /* every session was seen while it ran */
        if (r != NULL)
        {
            r->end = s_view.lastEnd;
            r->ended = true;
        }
    }
}

/* One super-loop pass at the current clock. */
static void LoopPass(void)
{
    VssTimeEvidence now;

    s_inputThisStep = false;
    AudioStream_Service();
    if (NowMs() >= s_domain.nextWarningMs)
    {
        SendWarning();
        s_domain.nextWarningMs += 50u;
    }
    now = Now();
    s_progress = Flow_Process(s_inputThisStep ? VSS_OPPORTUNITY_INPUT : VSS_OPPORTUNITY_TIME_CHECK, &now);
    CollectStatus();
    if (AudioTx_FaultMask() != 0u)
    {
        (void)AudioControl_MuteLatched();
    }
    LogSessions();
}

/* One output boundary followed by a pass, then a pass 10 ms later. Once the
 * boundaries are stopped only the clock moves. */
static void Step(void)
{
    s_boundary++;
    if (!s_boundariesStopped)
    {
        HalMock_SetTicks(BoundaryTick(s_boundary));
        HalMock_Boundary();
    }
    else
    {
        s_stallTick += 17u;
        HalMock_SetTicks(s_stallTick);
    }
    LoopPass();
    HalMock_SetTicks((s_boundariesStopped ? s_stallTick : BoundaryTick(s_boundary)) + 8u);
    LoopPass();
}

static void RunMs(uint64_t ms)
{
    uint64_t end = NowMs() + ms;

    while (NowMs() < end)
    {
        Step();
    }
}

/* Steps until no session runs, at most maxMs. */
static bool RunUntilIdle(uint64_t maxMs)
{
    uint64_t end = NowMs() + maxMs;

    while (NowMs() < end)
    {
        CHECK(Playback_Read(&s_view));
        if (!s_view.present)
        {
            return true;
        }
        Step();
    }
    return false;
}

/* The playing session, or false when idle or not yet started. */
static bool Playing(VssInputSignal *signal, VssInputValue *value)
{
    CHECK(Playback_Read(&s_view));
    if (!s_view.present || (s_view.lastStartedSession != s_view.session) || s_view.stopRequested)
    {
        return false;
    }
    *signal = s_view.candidate.signal;
    *value = s_view.candidate.value;
    return true;
}

static bool RunUntilPlaying(VssInputSignal signal, VssInputValue value, uint64_t maxMs)
{
    uint64_t end = NowMs() + maxMs;
    VssInputSignal s;
    VssInputValue v;

    while (NowMs() < end)
    {
        if (Playing(&s, &v) && (s == signal) && (v == value))
        {
            return true;
        }
        Step();
    }
    return false;
}

/* ---- boot ---------------------------------------------------------------- */

static void Boot(void)
{
    VssTimeEvidence now;

    HalMock_Reset();
    AudioTx_ResetForTest();
    AudioControl_ResetForTest();
    AudioAsset_Init();
    VssTime_ResetForTest();
    Input_TestReset();
    Store_TestReset();
    StorePlayback_TestReset();
    VssInputNet_ResetForTest();
    NetStub_ResetForTest();
    AudioStream_Init();
    HalMock_SetTicks(0u);
    CHECK(VssTime_Init(HalTimer_GetTicks));
    CHECK(AudioTx_Init());
    CHECK(AudioTx_Start());
    Net_Init();
    Health_Init();
    Flow_Init(VSS_PRODUCT);
    memset(&s_domain, 0, sizeof s_domain);
    s_domain.boot = 1u;
    s_domain.nextWarningMs = 100u;   /* the DOMAIN starts sending a little later */
    s_boundary = 0u;
    s_boundariesStopped = false;
    s_logCount = 0u;
    s_seenPublishCount = 0u;
    s_sessionCount = 0u;
    s_loggedLastEnd = 0u;

    /* A pass before the boot sequence finished reports STARTUP. */
    now = Now();
    (void)Flow_Process(VSS_OPPORTUNITY_TIME_CHECK, &now);
    CollectStatus();
    Health_BootComplete();
}

static void test_boot_reports(void)
{
    const NetVssStatus *s;

    Boot();
    CHECK_EQ_U(s_logCount, 1u);
    s = LastStatus();
    CHECK(s != NULL);
    if (s == NULL)
    {
        return;
    }
    CHECK_EQ_U(s->vss_state, NET_VSS_STATE_STARTUP);
    CHECK_EQ_U(s->vss_availability, NET_VSS_AVAILABILITY_UNAVAILABLE);
    CHECK_EQ_U(s->vss_accepting_events, NET_FLAG_NO);
    CHECK_EQ_U(s->update_sequence, 1u);
    CHECK_EQ_U(s->fault_mask, 0u);
    CHECK_EQ_U(s->vss_fault_active, NET_FLAG_NO);
    CHECK_EQ_U(s->vss_last_fault, NET_VSS_FAULT_CODE_NONE);
    CHECK_EQ_U(s->last_fault_quality, NET_QUALITY_NO_DATA);

    /* READY at once, accepting once the first DOMAIN message opened INPUT. */
    Step();
    s = LastStatus();
    CHECK_EQ_U(s->vss_state, NET_VSS_STATE_READY);
    CHECK_EQ_U(s->vss_availability, NET_VSS_AVAILABILITY_FULL);
    CHECK_EQ_U(s->vss_accepting_events, NET_FLAG_NO);
    CHECK_EQ_U(s->update_sequence, 2u);
    RunMs(150u);
    s = LastStatus();
    CHECK_EQ_U(s->vss_accepting_events, NET_FLAG_YES);
    CHECK_EQ_U(s->update_sequence, 3u);
}

/* ---- 200 one-shot events ------------------------------------------------- */

#define EVENT_COUNT 200u

static VssInputSignal SignalOf(NetVssEventKind kind)
{
    return (VssInputSignal)((unsigned)VSS_SIGNAL_WELCOME + (unsigned)kind - 1u);
}

static void test_two_hundred_events(void)
{
    uint32_t i;
    uint32_t completed = 0u;
    size_t firstSession = s_sessionCount;
    VssFlowObservation flow;
    VssInputNetObservation in;
    bool sawPlaying = false;
    bool sawReady = false;
    size_t firstLog = s_logCount;
    size_t k;

    for (i = 1u; i <= EVENT_COUNT; ++i)
    {
        NetVssEventKind kind = (NetVssEventKind)(1u + ((i - 1u) % 5u));
        SessionRecord *r;

        (void)SendEvent(kind, i);
        CHECK(RunUntilPlaying(SignalOf(kind), VSS_VALUE_EVENT, 200u));
        CHECK(RunUntilIdle(3000u));
        r = &s_sessions[s_sessionCount - 1u];
        CHECK_EQ_U(r->occurrence, i);
        CHECK_EQ_U(r->generation, 1u);
        CHECK_EQ_U(r->signal, SignalOf(kind));
        CHECK(r->ended);
        if (r->ended && (r->end == PLAYBACK_END_COMPLETED) && (r->occurrence == i))
        {
            ++completed;
        }
    }
    CHECK_EQ_U(completed, EVENT_COUNT);
    CHECK_EQ_U(s_sessionCount - firstSession, EVENT_COUNT);   /* one session each */

    CHECK(Flow_Read(&flow));
    CHECK(flow.recordsReclaimed >= EVENT_COUNT - 10u);       /* the ledger keeps turning over */
    CHECK(Store_Count() < 10u);
    CHECK_EQ_U(s_view.storeRefusals, 0u);
    CHECK_EQ_U(s_view.startRefusals, 0u);
    CHECK_EQ_U(s_view.outputFaults, 0u);
    CHECK_EQ_U(s_view.strayFacts, 0u);
    CHECK(VssInputNet_Read(VSS_PRODUCT, &in));
    CHECK_EQ_U(in.eventsForwarded, EVENT_COUNT);
    CHECK_EQ_U(in.eventsDiscarded, 0u);

    for (k = firstLog; k < s_logCount; ++k)
    {
        const NetVssStatus *s = &s_log[k].status;

        CHECK_EQ_U(s->fault_mask, 0u);
        CHECK_EQ_U(s->vss_accepting_events, NET_FLAG_YES);
        sawPlaying = sawPlaying || (s->vss_state == NET_VSS_STATE_PLAYING);
        sawReady = sawReady || (s->vss_state == NET_VSS_STATE_READY);
    }
    CHECK(sawPlaying && sawReady);
}

/* ---- warnings ------------------------------------------------------------ */

static void test_warning_preemption_and_release(void)
{
    VssInputSignal s;
    VssInputValue v;
    uint64_t emergencySession;
    uint64_t i;

    s_domain.rear = NET_REAR_WARNING_CAUTION;
    CHECK(RunUntilPlaying(VSS_SIGNAL_REAR_RISK, VSS_VALUE_CAUTION, 300u));
    CHECK_EQ_U(s_view.candidate.mode, VSS_MODE_STATEFUL);
    RunMs(2000u);   /* the warning loops while it holds */
    CHECK(Playing(&s, &v) && (s == VSS_SIGNAL_REAR_RISK) && (v == VSS_VALUE_CAUTION));
    CHECK_EQ_U(LastStatus()->vss_state, NET_VSS_STATE_PLAYING);

    /* Caution -> emergency preempts. */
    s_domain.rear = NET_REAR_WARNING_EMERGENCY;
    CHECK(RunUntilPlaying(VSS_SIGNAL_REAR_RISK, VSS_VALUE_EMERGENCY, 300u));
    emergencySession = s_view.session;
    CHECK(FindSession(emergencySession - 1u) != NULL);
    CHECK_EQ_U(FindSession(emergencySession - 1u)->end, PLAYBACK_END_STOPPED);

    /* An occupant hazard ranks below a rear emergency: no change. */
    s_domain.occupant = NET_FLAG_YES;
    for (i = 0u; i < 50u; ++i)
    {
        Step();
        CHECK(Playing(&s, &v));
        CHECK_EQ_U(s_view.session, emergencySession);
    }

    /* The rear clears: re-arbitration to the occupant hazard. */
    s_domain.rear = NET_REAR_WARNING_CLEAR;
    CHECK(RunUntilPlaying(VSS_SIGNAL_OCCUPANT_HAZARD, VSS_VALUE_ACTIVE, 300u));
    CHECK_EQ_U(FindSession(emergencySession)->end, PLAYBACK_END_STOPPED);

    /* Everything clears: silence, and it stays silent. */
    s_domain.occupant = NET_FLAG_NO;
    CHECK(RunUntilIdle(300u));
    RunMs(500u);
    CHECK(Playback_Read(&s_view));
    CHECK(!s_view.present);
    CHECK_EQ_U(LastStatus()->vss_state, NET_VSS_STATE_READY);
    CHECK_EQ_U(LastStatus()->fault_mask, 0u);
}

/* ---- DOMAIN restart ------------------------------------------------------ */

static void test_domain_restart(void)
{
    NetVssEvent old;
    NetCanRxMeta oldMeta;
    uint32_t oldTx;
    size_t sessions;
    VssInputNetObservation in;
    uint64_t i;

    /* One more event of run 1, kept to be delivered again later. */
    oldTx = SendEvent(NET_VSS_EVENT_LOCK_COMPLETE, 500u);
    old = EventMessage(NET_VSS_EVENT_LOCK_COMPLETE, 500u);
    oldMeta = Meta(1u, oldTx);
    CHECK(RunUntilPlaying(VSS_SIGNAL_LOCK_COMPLETE, VSS_VALUE_EVENT, 200u));
    CHECK(RunUntilIdle(3000u));
    CHECK_EQ_U(s_sessions[s_sessionCount - 1u].occurrence, 500u);

    /* The DOMAIN restarts: run 2 starts its TX numbers and occurrences anew. */
    s_domain.boot = 2u;
    s_domain.eventTx = 0u;
    s_domain.warningTx = 0u;
    RunMs(100u);
    CHECK(VssInputNet_Read(VSS_PRODUCT, &in));
    CHECK_EQ_U(in.acceptedBoot, 2u);
    CHECK_EQ_U(in.generationChanges, 1u);
    CHECK_EQ_U(LastStatus()->vss_accepting_events, NET_FLAG_YES);

    (void)SendEvent(NET_VSS_EVENT_WELCOME, 1u);
    CHECK(RunUntilPlaying(VSS_SIGNAL_WELCOME, VSS_VALUE_EVENT, 200u));
    CHECK_EQ_U(s_view.candidate.key.generation, 2u);
    CHECK_EQ_U(s_view.candidate.key.occurrenceId, 1u);
    CHECK(RunUntilIdle(3000u));
    CHECK_EQ_U(s_sessions[s_sessionCount - 1u].end, PLAYBACK_END_COMPLETED);

    /* A late duplicate of run 1 arrives: refused, nothing plays. */
    sessions = s_sessionCount;
    oldMeta.received_at_ms = VssTime_NowMs32();
    VssApp_OnDomainEvent(&old, &oldMeta);
    CHECK(VssInputNet_Read(VSS_PRODUCT, &in));
    CHECK_EQ_U(in.lastEvent.validation, VSS_VALIDATION_REJECTED);
    CHECK_EQ_U(in.acceptedBoot, 2u);
    for (i = 0u; i < 30u; ++i)
    {
        Step();
    }
    CHECK_EQ_U(s_sessionCount, sessions);

    /* Run 2 keeps playing. */
    (void)SendEvent(NET_VSS_EVENT_GOODBYE, 2u);
    CHECK(RunUntilPlaying(VSS_SIGNAL_GOODBYE, VSS_VALUE_EVENT, 200u));
    CHECK(RunUntilIdle(3000u));
    CHECK_EQ_U(s_sessions[s_sessionCount - 1u].end, PLAYBACK_END_COMPLETED);
    CHECK_EQ_U(s_sessions[s_sessionCount - 1u].generation, 2u);
}

/* ---- a refused report ---------------------------------------------------- */

static void test_refused_report(void)
{
    uint32_t attempts = NetStub_AttemptCount();
    uint32_t lastSequence = LastStatus()->update_sequence;
    uint64_t lastAt = s_log[s_logCount - 1u].atMs;
    uint64_t refusedAt = 0u;
    size_t logged = s_logCount;
    VssFlowObservation flow;

    NetStub_RefuseForTest(NET_SUBMIT_NOT_READY, 1u);
    while (NetStub_AttemptCount() == attempts)
    {
        Step();
    }
    refusedAt = NowMs();
    CHECK_EQ_U(s_logCount, logged);
    CHECK(Flow_Read(&flow));
    CHECK_EQ_U(flow.statusRefused, 1u);
    CHECK_EQ_U(flow.lastRefusal, NET_SUBMIT_NOT_READY);
    CHECK(refusedAt - lastAt <= (uint64_t)VSS_CFG_STATUS_PERIOD_MS + 25u);

    /* Not retried: the next status period submits the next evaluation. */
    while (s_logCount == logged)
    {
        Step();
    }
    CHECK_EQ_U(NetStub_AttemptCount(), attempts + 2u);
    CHECK_EQ_U(LastStatus()->update_sequence, lastSequence + 2u);
    CHECK(s_log[s_logCount - 1u].atMs - refusedAt >= (uint64_t)VSS_CFG_STATUS_PERIOD_MS - 25u);
    CHECK(s_log[s_logCount - 1u].atMs - refusedAt <= (uint64_t)VSS_CFG_STATUS_PERIOD_MS + 25u);
}

/* ---- output stall -------------------------------------------------------- */

static void test_output_stall(void)
{
    const NetVssStatus *s;
    VssHealthObservation h;
    uint64_t faultAt;
    uint16_t age;

    CHECK(Health_Read(&h));
    CHECK_EQ_U(h.occurrences, 0u);
    CHECK_EQ_U(LastStatus()->vss_last_fault, NET_VSS_FAULT_CODE_NONE);

    (void)SendEvent(NET_VSS_EVENT_UNLOCK_COMPLETE, 3u);
    CHECK(RunUntilPlaying(VSS_SIGNAL_UNLOCK_COMPLETE, VSS_VALUE_EVENT, 200u));
    s_boundariesStopped = true;
    s_stallTick = BoundaryTick(s_boundary);
    while ((AudioTx_FaultMask() & AUDIO_TX_FAULT_STALL) == 0u)
    {
        Step();
    }
    RunMs(30u);
    s = LastStatus();
    CHECK_EQ_U(s->vss_state, NET_VSS_STATE_FAULT);
    CHECK_EQ_U(s->vss_availability, NET_VSS_AVAILABILITY_UNAVAILABLE);
    CHECK_EQ_U(s->vss_fault_active, NET_FLAG_YES);
    CHECK_EQ_U(s->fault_mask, NET_VSS_FAULT_AUDIO_OUTPUT_FAILURE);
    CHECK_EQ_U(s->vss_last_fault, NET_VSS_FAULT_CODE_AUDIO_OUTPUT_FAILURE);
    CHECK_EQ_U(s->last_fault_occurrence, 1u);
    CHECK_EQ_U(s->last_fault_quality, NET_QUALITY_OK);
    CHECK(Health_Read(&h));
    CHECK(h.muteRequested);
    CHECK(!h.muteAccepted);            /* the codec was never configured here */
    faultAt = h.current[VSS_FAULT_OUTPUT].firstAtMs;
    CHECK_EQ_U(s->last_fault_age_ms, s_log[s_logCount - 1u].atMs - faultAt);
    CHECK(Playback_Read(&s_view));
    CHECK(!s_view.present);
    CHECK_EQ_U(s_sessions[s_sessionCount - 1u].end, PLAYBACK_END_FAULT);

    /* The fault holds; the same instance ages in every later report. */
    age = s->last_fault_age_ms;
    RunMs(600u);
    s = LastStatus();
    CHECK_EQ_U(s->vss_state, NET_VSS_STATE_FAULT);
    CHECK_EQ_U(s->fault_mask, NET_VSS_FAULT_AUDIO_OUTPUT_FAILURE);
    CHECK_EQ_U(s->last_fault_occurrence, 1u);
    CHECK(s->last_fault_age_ms > age);
    CHECK(s->source_age_ms < 25u);

    /* No new session is started after the output failed. */
    (void)SendEvent(NET_VSS_EVENT_WELCOME, 4u);
    RunMs(300u);
    CHECK(Playback_Read(&s_view));
    CHECK(!s_view.present);
}

/* Over the whole run: every report took the next number (one refusal
 * excepted), no report was older than the period at submission, and reports
 * came at least every period. */
static void test_report_stream(void)
{
    size_t k;
    uint32_t gaps = 0u;

    CHECK(!s_logOverflow);
    /* The run covers some 190 s. */
    CHECK(s_log[s_logCount - 1u].atMs - s_log[0].atMs > 150000u);
    CHECK(s_logCount >= (s_log[s_logCount - 1u].atMs - s_log[0].atMs) / VSS_CFG_STATUS_PERIOD_MS);
    for (k = 1u; k < s_logCount; ++k)
    {
        const NetVssStatus *a = &s_log[k - 1u].status;
        const NetVssStatus *b = &s_log[k].status;

        if (b->update_sequence != a->update_sequence + 1u)
        {
            CHECK_EQ_U(b->update_sequence, a->update_sequence + 2u);
            ++gaps;
        }
        CHECK(s_log[k].atMs - s_log[k - 1u].atMs <= 2u * (uint64_t)VSS_CFG_STATUS_PERIOD_MS + 25u);
        CHECK_EQ_U(b->source_age_ms, 0u);   /* submitted in the pass that evaluated it */
        CHECK((b->fault_mask == 0u) == (b->vss_fault_active == NET_FLAG_NO));
    }
    CHECK_EQ_U(gaps, 1u);
}

/* A new run whose boot sequence reported an initialization fault: FAULT
 * from the first report on, and no session is started. */
static void test_init_fault_blocks_starts(void)
{
    VssTimeEvidence now;
    const NetVssStatus *s;

    Boot();
    now = Now();
    Health_Report(VSS_FAULT_INIT, &now);
    RunMs(200u);
    s = LastStatus();
    CHECK_EQ_U(s->vss_state, NET_VSS_STATE_FAULT);
    CHECK_EQ_U(s->fault_mask, NET_VSS_FAULT_INITIALIZATION_FAILURE);
    CHECK_EQ_U(s->vss_last_fault, NET_VSS_FAULT_CODE_INITIALIZATION_FAILURE);
    CHECK_EQ_U(s->vss_accepting_events, NET_FLAG_YES);
    (void)SendEvent(NET_VSS_EVENT_WELCOME, 1u);
    RunMs(300u);
    CHECK_EQ_U(s_sessionCount, 0u);
    CHECK(Playback_Read(&s_view));
    CHECK_EQ_U(s_view.lastSession, 0u);
}

int main(void)
{
    test_boot_reports();
    test_two_hundred_events();
    test_warning_preemption_and_release();
    test_domain_restart();
    test_refused_report();
    test_output_stall();
    test_report_stream();
    test_init_fault_blocks_starts();
    TEST_FINISH();
}
