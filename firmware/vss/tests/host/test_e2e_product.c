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
 * back to back; warning preemption, release and re-arbitration; a warning
 * preempting a playing event; a warning losing its quality and then its
 * frames; a DOMAIN restart with a late duplicate of the old run; refused
 * status reports; the output stalling. Then separate runs: an
 * initialization fault at boot; an event preempted before its first PCM;
 * the output stalling while idle; the clock moving before an input.
 *
 * Every session is checked against the decoder: the half-buffers it
 * writes to the ring are those the decoder renders for its cue, up to the
 * first audible one. */

#include "test_util.h"
#include "hal_mock.h"

#include "../../src/app/vss_flow.h"
#include "../../src/service/input/vss_input_net.h"
#include "../../src/service/health/vss_health.h"
#include "../../src/service/audio_stream/audio_stream.h"
#include "../../src/service/audio_stream/decoder/Mp3PcmProvider.h"
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
    NetValueQuality occupantQuality;
    NetQualityReason occupantReason;
    NetRearWarningState rear;
    uint64_t nextWarningMs;
    uint64_t lastSentMs;            /* receive time of the last warning frame */
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
    w.occupant_hazard_quality = s_domain.occupantQuality;
    w.occupant_hazard_reason = s_domain.occupantReason;
    w.rear_obstacle_source_boot = 23u;
    w.rear_obstacle_source_sequence = seq;
    w.rear_obstacle_state = s_domain.rear;
    w.rear_detection_activation = NET_FLAG_YES;
    w.activation_meta_format = 1u;
    w.rear_activation_sequence = seq;
    m = Meta(s_domain.boot, ++s_domain.warningTx);
    VssApp_OnDomainWarningState(&w, &m);
    s_domain.lastSentMs = NowMs();
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

/* ---- decoder reference ---------------------------------------------------- */

/* The first half-buffers the decoder renders for each asset, as the stream
 * would (warnings loop, events play once). The cues begin with a few
 * half-buffers of silence, so a session's output is compared half by half,
 * in the order written, up to and including the first audible one. */
#define REFERENCE_HALVES 8u

static int16_t s_reference[AUDIO_ASSET_COUNT][REFERENCE_HALVES][AUDIO_TX_HALF_WORDS];
static uint32_t s_firstAudible[AUDIO_ASSET_COUNT];

typedef struct
{
    uint64_t session;
    AudioAsset_IdType asset;
    uint32_t next;                  /* index of the next half the session writes */
    bool open;                      /* comparing */
} RingCheck;

static RingCheck s_ring;
static uint32_t s_ringVerified;     /* sessions whose output matched up to the first sound */
static bool s_boundaryThisPass;

static bool AssetLoops(AudioAsset_IdType asset)
{
    return asset >= AUDIO_ASSET_ID_ANTI_PINCH;
}

/* The cue each input must sound, independent of the firmware's cue table. */
static AudioAsset_IdType ExpectedAsset(VssInputSignal signal, VssInputValue value)
{
    switch (signal)
    {
    case VSS_SIGNAL_WELCOME:          return AUDIO_ASSET_ID_WELCOME;
    case VSS_SIGNAL_GOODBYE:          return AUDIO_ASSET_ID_GOODBYE;
    case VSS_SIGNAL_LOCK_COMPLETE:    return AUDIO_ASSET_ID_LOCK;
    case VSS_SIGNAL_UNLOCK_COMPLETE:  return AUDIO_ASSET_ID_UNLOCK;
    case VSS_SIGNAL_LOCK_ERROR:       return AUDIO_ASSET_ID_LOCK_ERROR;
    case VSS_SIGNAL_ANTI_PINCH:       return AUDIO_ASSET_ID_ANTI_PINCH;
    case VSS_SIGNAL_OCCUPANT_HAZARD:  return AUDIO_ASSET_ID_OCCUPANT;
    case VSS_SIGNAL_REAR_RISK:
        return (value == VSS_VALUE_EMERGENCY) ? AUDIO_ASSET_ID_REAR_EMERGENCY : AUDIO_ASSET_ID_REAR_CAUTION;
    default:                          return AUDIO_ASSET_COUNT;
    }
}

static bool Audible(const int16_t *pcm)
{
    uint32_t i;

    for (i = 0u; i < AUDIO_TX_HALF_WORDS; ++i)
    {
        if (pcm[i] != 0)
        {
            return true;
        }
    }
    return false;
}

/* Before the stream initializes the decoder for the run. */
static void DecodeReferences(void)
{
    AudioAsset_IdType a;
    uint32_t h;

    for (a = 0u; a < AUDIO_ASSET_COUNT; ++a)
    {
        Mp3PcmProvider_Init();
        Mp3PcmProvider_SelectAsset(a, AssetLoops(a));
        Mp3PcmProvider_Reset();
        s_firstAudible[a] = REFERENCE_HALVES;
        for (h = 0u; h < REFERENCE_HALVES; ++h)
        {
            Mp3PcmProvider_RenderStereo16((uint16_t *)s_reference[a][h], AUDIO_TX_HALF_FRAMES);
            if ((s_firstAudible[a] == REFERENCE_HALVES) && Audible(s_reference[a][h]))
            {
                s_firstAudible[a] = h;
            }
        }
        CHECK(Mp3PcmProvider_IsValid());
        CHECK(s_firstAudible[a] < REFERENCE_HALVES);
    }
}

/* After every pass: from the boundary pass after its adoption, each half a
 * session writes (A at odd boundaries, B at even ones) must be the next
 * half the decoder renders for the cue of its signal and value, until the
 * first audible one. A session stopped before that is not judged. */
static void CheckRing(const VssPlaybackObservation *v)
{
    const uint16_t *ring = HalMock_RingWords() + HAL_AUDIO_PRIME_WORDS;
    const uint16_t *written;
    AudioAsset_IdType asset;
    bool same;

    if (!v->present)
    {
        s_ring.open = false;
        return;
    }
    if (v->session != s_ring.session)
    {
        /* Adopted in this pass: whatever was written here came before it. */
        s_ring.session = v->session;
        s_ring.asset = ExpectedAsset(v->candidate.signal, v->candidate.value);
        s_ring.next = 0u;
        s_ring.open = s_ring.asset < AUDIO_ASSET_COUNT;
        CHECK(s_ring.open);
        return;
    }
    if (!s_ring.open || !s_boundaryThisPass || s_boundariesStopped || v->stopRequested)
    {
        if (v->stopRequested)
        {
            s_ring.open = false;
        }
        return;
    }
    asset = s_ring.asset;
    written = ring + (((s_boundary & 1u) != 0u) ? 0u : AUDIO_TX_HALF_WORDS);
    same = memcmp(written, s_reference[asset][s_ring.next], AUDIO_TX_HALF_WORDS * sizeof(uint16_t)) == 0;
    if (!same)
    {
        printf("  session %llu (asset %u): half %u differs from the decoder\n",
               (unsigned long long)v->session, (unsigned)asset, (unsigned)s_ring.next);
    }
    CHECK(same);
    if (!same || (s_ring.next == s_firstAudible[asset]))
    {
        s_ringVerified += same ? 1u : 0u;
        s_ring.open = false;
        return;
    }
    s_ring.next++;
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
    CheckRing(&s_view);
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

/* One super-loop pass at the current clock: refill, input delivery, then
 * the flow pass, which reads the clock after the delivery. */
static void LoopPass(void)
{
    s_inputThisStep = false;
    AudioStream_Service();
    if (NowMs() >= s_domain.nextWarningMs)
    {
        SendWarning();
        s_domain.nextWarningMs += 50u;
    }
    s_progress = Flow_Process(s_inputThisStep ? VSS_OPPORTUNITY_INPUT : VSS_OPPORTUNITY_TIME_CHECK);
    CollectStatus();
    if (AudioTx_FaultMask() != 0u)
    {
        (void)AudioControl_MuteLatched();
    }
    LogSessions();
}

static bool s_atBoundary;

/* One output boundary followed by a pass. Once the boundaries are stopped
 * only the clock moves. */
static void BoundaryPass(void)
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
    s_boundaryThisPass = true;
    LoopPass();
    s_boundaryThisPass = false;
    s_atBoundary = true;
}

/* A pass 10 ms after the last boundary. */
static void LaterPass(void)
{
    HalMock_SetTicks((s_boundariesStopped ? s_stallTick : BoundaryTick(s_boundary)) + 8u);
    LoopPass();
    s_atBoundary = false;
}

/* The next of the two passes per boundary. */
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

static void Step(void)
{
    BoundaryPass();
    LaterPass();
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
    DecodeReferences();
    memset(&s_ring, 0, sizeof s_ring);
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
    Health_Init(VSS_PRODUCT);
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
    (void)Flow_Process(VSS_OPPORTUNITY_TIME_CHECK);
    CollectStatus();
    Health_BootComplete();
}

static void test_boot_reports(void)
{
    const NetVssStatus *s;
    VssInputNetObservation in;

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

    /* READY and accepting as soon as the boot completed, before any DOMAIN
     * message reached INPUT. */
    Step();
    s = LastStatus();
    CHECK(VssInputNet_Read(VSS_PRODUCT, &in));
    CHECK(!in.generationOpen);
    CHECK_EQ_U(in.warningFramesForwarded + in.eventsForwarded, 0u);
    CHECK_EQ_U(s->vss_state, NET_VSS_STATE_READY);
    CHECK_EQ_U(s->vss_availability, NET_VSS_AVAILABILITY_FULL);
    CHECK_EQ_U(s->vss_accepting_events, NET_FLAG_YES);
    CHECK_EQ_U(s->update_sequence, 2u);
    /* The first DOMAIN message changes nothing in the report. */
    RunMs(150u);
    CHECK(VssInputNet_Read(VSS_PRODUCT, &in));
    CHECK(in.generationOpen);
    s = LastStatus();
    CHECK_EQ_U(s->vss_accepting_events, NET_FLAG_YES);
    CHECK_EQ_U(s->update_sequence, 2u);
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
    uint32_t verified = s_ringVerified;
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
    CHECK_EQ_U(s_ringVerified - verified, EVENT_COUNT);        /* each one's PCM was its cue's */

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

/* A rear emergency arrives while an event plays: the event is interrupted
 * and, having started, is not offered again; the warning plays; once it
 * clears nothing else plays. */
static void test_warning_preempts_a_playing_event(void)
{
    VssOccurrenceObservation rec;
    VssOccurrenceKey key;
    uint64_t eventSession;
    size_t sessions;
    SessionRecord *r;

    (void)SendEvent(NET_VSS_EVENT_GOODBYE, 300u);
    CHECK(RunUntilPlaying(VSS_SIGNAL_GOODBYE, VSS_VALUE_EVENT, 200u));
    eventSession = s_view.session;
    key = s_view.candidate.key;
    Step();
    Step();
    CHECK_EQ_U(LastStatus()->vss_state, NET_VSS_STATE_PLAYING);

    s_domain.rear = NET_REAR_WARNING_EMERGENCY;
    CHECK(RunUntilPlaying(VSS_SIGNAL_REAR_RISK, VSS_VALUE_EMERGENCY, 300u));
    r = FindSession(eventSession);
    CHECK((r != NULL) && r->ended);
    if (r != NULL)
    {
        CHECK_EQ_U(r->end, PLAYBACK_END_INTERRUPTED);
    }
    CHECK(Store_ReadOccurrence(&key, &rec));
    CHECK_EQ_U(rec.state, VSS_OCCURRENCE_INTERRUPTED);
    CHECK(!rec.replayAllowed);

    s_domain.rear = NET_REAR_WARNING_CLEAR;
    CHECK(RunUntilIdle(300u));
    sessions = s_sessionCount;
    RunMs(600u);
    CHECK_EQ_U(s_sessionCount, sessions);
    CHECK_EQ_U(LastStatus()->vss_state, NET_VSS_STATE_READY);
    CHECK_EQ_U(LastStatus()->fault_mask, 0u);
}

/* Passes until the next warning frame was delivered; its receive time. */
static uint64_t PassUntilWarningFrame(void)
{
    uint32_t tx = s_domain.warningTx;

    while (s_domain.warningTx == tx)
    {
        NextPass();
    }
    return s_domain.lastSentMs;
}

/* Steps while the session keeps playing, until the clock reaches untilMs. */
static void ExpectPlayingUntil(uint64_t session, uint64_t untilMs)
{
    while (NowMs() < untilMs)
    {
        NextPass();
        CHECK(Playback_Read(&s_view));
        CHECK(s_view.present && (s_view.session == session) && !s_view.stopRequested);
        if (!s_view.present || (s_view.session != session))
        {
            return;
        }
    }
}

/* The occupant hazard plays. Its frames turn INVALID: the warning keeps
 * playing for the 500 ms Hold from the first such frame, then stops. Valid
 * again, it plays again. Then the frames stop: 150 ms after the last one
 * the input counts as lost, and the Hold runs from there. */
static void test_warning_quality_loss_and_hold(void)
{
    const uint64_t holdMs = 500u;
    const uint64_t noRxMs = 150u;
    uint64_t session;
    uint64_t lossAt;
    uint64_t lastRx;

    s_domain.occupant = NET_FLAG_YES;
    CHECK(RunUntilPlaying(VSS_SIGNAL_OCCUPANT_HAZARD, VSS_VALUE_ACTIVE, 300u));
    session = s_view.session;

    s_domain.occupantQuality = NET_QUALITY_INVALID;
    s_domain.occupantReason = NET_QUALITY_REASON_SENSOR_FAULT;
    lossAt = PassUntilWarningFrame();
    ExpectPlayingUntil(session, lossAt + holdMs - 10u);
    CHECK(RunUntilIdle(100u));
    CHECK_EQ_U(s_view.lastSession, session);
    CHECK_EQ_U(s_view.lastEnd, PLAYBACK_END_STOPPED);
    CHECK(s_view.lastEndAtMs >= lossAt + holdMs);

    s_domain.occupantQuality = NET_QUALITY_OK;
    s_domain.occupantReason = NET_QUALITY_REASON_NONE;
    CHECK(RunUntilPlaying(VSS_SIGNAL_OCCUPANT_HAZARD, VSS_VALUE_ACTIVE, 300u));
    session = s_view.session;

    lastRx = PassUntilWarningFrame();
    s_domain.nextWarningMs = UINT64_MAX;
    ExpectPlayingUntil(session, lastRx + noRxMs + holdMs - 10u);
    CHECK(RunUntilIdle(100u));
    CHECK_EQ_U(s_view.lastSession, session);
    CHECK_EQ_U(s_view.lastEnd, PLAYBACK_END_STOPPED);
    CHECK(s_view.lastEndAtMs >= lastRx + noRxMs + holdMs);

    /* The frames resume with the hazard gone: silence. */
    s_domain.occupant = NET_FLAG_NO;
    s_domain.nextWarningMs = NowMs();
    RunMs(500u);
    CHECK(Playback_Read(&s_view));
    CHECK(!s_view.present);
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

/* Passes until the Network stand-in saw another submission. */
static void PassUntilAttempt(void)
{
    uint32_t attempts = NetStub_AttemptCount();

    while (NetStub_AttemptCount() == attempts)
    {
        NextPass();
    }
}

static void test_refused_report(void)
{
    uint32_t lastSequence = LastStatus()->update_sequence;
    uint64_t lastAt = s_log[s_logCount - 1u].atMs;
    size_t logged = s_logCount;
    uint32_t attempts = NetStub_AttemptCount();
    uint32_t passes;
    uint64_t refusedAt;
    VssFlowObservation flow;

    /* Refused twice: the same report, with its number, is offered again in
     * every following pass until the Network takes it. Only its ages grow. */
    NetStub_RefuseForTest(NET_SUBMIT_NOT_READY, 2u);
    PassUntilAttempt();
    refusedAt = NowMs();
    CHECK(refusedAt - lastAt <= (uint64_t)VSS_CFG_STATUS_PERIOD_MS + 25u);
    CHECK_EQ_U(s_logCount, logged);
    NextPass();
    CHECK_EQ_U(NetStub_AttemptCount(), attempts + 2u);
    CHECK_EQ_U(s_logCount, logged);
    NextPass();
    CHECK_EQ_U(NetStub_AttemptCount(), attempts + 3u);
    CHECK_EQ_U(s_logCount, logged + 1u);
    CHECK_EQ_U(LastStatus()->update_sequence, lastSequence + 1u);
    CHECK(LastStatus()->source_age_ms > 0u);
    CHECK_EQ_U(LastStatus()->source_age_ms, NowMs() - refusedAt);
    CHECK(Flow_Read(&flow));
    CHECK_EQ_U(flow.statusRefused, 2u);
    CHECK_EQ_U(flow.lastRefusal, NET_SUBMIT_NOT_READY);

    /* Refused while the state changes: the newer evaluation takes the place
     * of the waiting one, which is never sent. */
    lastSequence = LastStatus()->update_sequence;
    logged = s_logCount;
    NetStub_RefuseForTest(NET_SUBMIT_NO_CAPACITY, UINT32_MAX);
    PassUntilAttempt();                       /* the periodic report, refused */
    (void)SendEvent(NET_VSS_EVENT_LOCK_ERROR, 3u);
    CHECK(RunUntilPlaying(VSS_SIGNAL_LOCK_ERROR, VSS_VALUE_EVENT, 200u));
    CHECK(Flow_Read(&flow));
    passes = flow.passes;
    attempts = NetStub_AttemptCount();
    Step();
    CHECK(Flow_Read(&flow));
    CHECK_EQ_U(NetStub_AttemptCount() - attempts, flow.passes - passes);   /* offered in every pass */
    CHECK_EQ_U(s_logCount, logged);
    NetStub_RefuseForTest(NET_SUBMIT_OK, 0u);
    NextPass();
    CHECK_EQ_U(s_logCount, logged + 1u);
    CHECK_EQ_U(LastStatus()->vss_state, NET_VSS_STATE_PLAYING);
    CHECK_EQ_U(LastStatus()->update_sequence, lastSequence + 2u);
    CHECK(RunUntilIdle(3000u));
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

    (void)SendEvent(NET_VSS_EVENT_UNLOCK_COMPLETE, 4u);
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
    CHECK_EQ_U(s->vss_accepting_events, NET_FLAG_NO);
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
    (void)SendEvent(NET_VSS_EVENT_WELCOME, 5u);
    RunMs(300u);
    CHECK(Playback_Read(&s_view));
    CHECK(!s_view.present);
}

/* Over the whole run: every report took the next number, except the one
 * report that a newer evaluation replaced while the Network refused it;
 * only the two reports that waited for a refusal to end were sent after
 * the pass that evaluated them; reports came at least every period. */
static void test_report_stream(void)
{
    size_t k;
    uint32_t gaps = 0u;
    uint32_t aged = 0u;

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
        if (b->source_age_ms != 0u)
        {
            ++aged;
        }
        CHECK((b->fault_mask == 0u) == (b->vss_fault_active == NET_FLAG_NO));
    }
    CHECK_EQ_U(gaps, 1u);
    CHECK_EQ_U(aged, 2u);
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
    CHECK_EQ_U(s->vss_accepting_events, NET_FLAG_NO);
    (void)SendEvent(NET_VSS_EVENT_WELCOME, 1u);
    RunMs(300u);
    CHECK_EQ_U(s_sessionCount, 0u);
    CHECK(Playback_Read(&s_view));
    CHECK_EQ_U(s_view.lastSession, 0u);
}

/* ---- separate runs --------------------------------------------------------- */

/* A rear emergency preempts an event before the event's first PCM reached
 * the output, a while after boot and after the last boundary. The event
 * ended without a start: STORE takes the no-start fact, no fault is raised,
 * and the event plays once the warning clears within its use limit. */
static void test_event_preempted_before_output_plays_later(void)
{
    VssHealthObservation h;
    VssOccurrenceObservation rec;
    VssOccurrenceKey key;
    uint64_t eventSession;
    SessionRecord *replay = NULL;
    uint32_t i;

    Boot();
    RunMs(3000u);
    (void)SendEvent(NET_VSS_EVENT_WELCOME, 77u);
    LoopPass();
    CHECK(Playback_Read(&s_view));
    CHECK(s_view.present);
    CHECK_EQ_U(s_view.phase, PLAYBACK_STARTING);
    CHECK_EQ_U(s_view.candidate.signal, VSS_SIGNAL_WELCOME);
    CHECK(NowMs() > VssTime_TicksToMs(BoundaryTick(s_boundary)));   /* after the last boundary */
    eventSession = s_view.session;
    key = s_view.candidate.key;

    /* Same tick, before the next boundary. */
    s_domain.rear = NET_REAR_WARNING_EMERGENCY;
    SendWarning();
    LoopPass();
    HalMock_SetTicks(BoundaryTick(s_boundary) + 9u);
    LoopPass();
    CHECK(Playback_Read(&s_view));
    CHECK(FindSession(eventSession) != NULL);
    if (FindSession(eventSession) != NULL)
    {
        CHECK_EQ_U(FindSession(eventSession)->end, PLAYBACK_END_NO_START);
    }
    CHECK_EQ_U(s_view.storeRefusals, 0u);
    CHECK(Health_Read(&h));
    CHECK_EQ_U(h.currentMask, 0u);
    CHECK(Store_ReadOccurrence(&key, &rec));
    CHECK_EQ_U(rec.state, VSS_OCCURRENCE_PENDING);
    CHECK(rec.replayAllowed);

    s_domain.rear = NET_REAR_WARNING_CLEAR;
    for (i = 0u; (i < 20u) && (replay == NULL); ++i)
    {
        size_t k;

        Step();
        for (k = 0u; k < s_sessionCount; ++k)
        {
            if ((s_sessions[k].session > eventSession) && (s_sessions[k].signal == VSS_SIGNAL_WELCOME))
            {
                replay = &s_sessions[k];
            }
        }
    }
    CHECK(replay != NULL);
    if (replay == NULL)
    {
        return;
    }
    CHECK_EQ_U(replay->occurrence, 77u);
    CHECK(RunUntilIdle(3000u));
    CHECK_EQ_U(replay->end, PLAYBACK_END_COMPLETED);
    CHECK(Store_ReadOccurrence(&key, &rec));
    CHECK_EQ_U(rec.state, VSS_OCCURRENCE_COMPLETED);
    CHECK_EQ_U(LastStatus()->fault_mask, 0u);
}

/* The output stalls while nothing plays, and an event arrives in the pass
 * that sees the stall: the event is not started, and the only fault is the
 * output fault. */
static void test_output_stall_while_idle(void)
{
    VssHealthObservation h;
    const NetVssStatus *s;

    Boot();
    RunMs(400u);
    s_boundariesStopped = true;
    s_stallTick = BoundaryTick(s_boundary);
    HalMock_SetTicks(s_stallTick + 50u);
    LoopPass();
    CHECK_EQ_U(AudioTx_FaultMask(), 0u);
    HalMock_SetTicks(s_stallTick + 60u);
    (void)SendEvent(NET_VSS_EVENT_LOCK_COMPLETE, 88u);
    LoopPass();
    CHECK((AudioTx_FaultMask() & AUDIO_TX_FAULT_STALL) != 0u);
    CHECK(Playback_Read(&s_view));
    CHECK(!s_view.present);
    CHECK_EQ_U(s_view.startRefusals, 0u);
    CHECK_EQ_U(s_view.lastSession, 0u);
    CHECK(Health_Read(&h));
    CHECK_EQ_U(h.currentMask, 1u << VSS_FAULT_OUTPUT);

    RunMs(300u);
    CHECK_EQ_U(s_sessionCount, 0u);
    s = LastStatus();
    CHECK_EQ_U(s->vss_state, NET_VSS_STATE_FAULT);
    CHECK_EQ_U(s->fault_mask, NET_VSS_FAULT_AUDIO_OUTPUT_FAILURE);
    CHECK_EQ_U(s->vss_last_fault, NET_VSS_FAULT_CODE_AUDIO_OUTPUT_FAILURE);
    CHECK_EQ_U(s->last_fault_occurrence, 1u);
}

/* The clock moves between the top of the loop and the delivery of a
 * warning frame (a decode runs in between). The flow pass that follows the
 * delivery reads its time after it and keeps the warning playing. Without
 * a registered clock a pass does nothing. */
static void test_flow_time_follows_the_input(void)
{
    VssFlowObservation flow;
    uint64_t session;
    uint32_t passes;

    Boot();
    RunMs(400u);
    s_domain.rear = NET_REAR_WARNING_CAUTION;
    CHECK(RunUntilPlaying(VSS_SIGNAL_REAR_RISK, VSS_VALUE_CAUTION, 300u));
    RunMs(200u);
    CHECK(Playback_Read(&s_view));
    session = s_view.session;

    AudioStream_Service();
    HalMock_SetTicks(BoundaryTick(s_boundary) + 10u);
    SendWarning();
    (void)Flow_Process(VSS_OPPORTUNITY_INPUT);
    CHECK(Playback_Read(&s_view));
    CHECK_EQ_U(s_view.session, session);
    CHECK(!s_view.stopRequested);
    RunMs(100u);
    CHECK(Playback_Read(&s_view));
    CHECK_EQ_U(s_view.session, session);

    CHECK(Flow_Read(&flow));
    passes = flow.passes;
    VssTime_ResetForTest();
    CHECK_EQ_U(Flow_Process(VSS_OPPORTUNITY_TIME_CHECK), VSS_FLOW_RECOLLECT_OBSERVATIONS);
    CHECK(Flow_Read(&flow));
    CHECK_EQ_U(flow.passes, passes);
    CHECK_EQ_U(flow.passesWithoutClock, 1u);
}

int main(void)
{
    test_boot_reports();
    test_two_hundred_events();
    test_warning_preemption_and_release();
    test_warning_preempts_a_playing_event();
    test_warning_quality_loss_and_hold();
    test_domain_restart();
    test_refused_report();
    test_output_stall();
    test_report_stream();
    test_init_fault_blocks_starts();
    test_event_preempted_before_output_plays_later();
    test_output_stall_while_idle();
    test_flow_time_follows_the_input();
    TEST_FINISH();
}
