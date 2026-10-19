/* Playback sessions on the real audio stream, transmit driver, STORE and
 * selection, with the HAL stand-in raising the output boundaries every
 * 1024 frames at 48 kHz (21.33 ms) on the 800 Hz tick. Each pass does what
 * the flow will do: stream service, deadlines, playback advance, candidate
 * collection, selection and the transition request. */

#include "test_util.h"
#include "hal_mock.h"

#include "../../src/service/playback/vss_playback.h"
#include "../../src/service/audio_stream/audio_stream.h"
#include "../../src/service/asset/audio_asset.h"
#include "../../src/service/asset/vss_cue_table.h"
#include "../../src/driver/audio_tx/audio_tx.h"
#include "../../src/infra/vss_time.h"

#include <string.h>

/* ---- time line --------------------------------------------------------- */

static uint32_t s_boundary;

/* Tick of boundary k: k halves of 1024/48000 s at 800 ticks/s. */
static uint32_t BoundaryTick(uint32_t k)
{
    return (uint32_t)(((uint64_t)k * 256u) / 15u);
}

static uint64_t BoundaryMs(uint32_t k)
{
    return VssTime_TicksToMs(BoundaryTick(k));
}

static VssTimeEvidence Now(void)
{
    VssTimeEvidence t;
    uint64_t ms = VssTime_NowMs();

    t.domain = VSS_TEST;
    t.epoch = 1u;
    t.earliest = ms;
    t.latest = ms;
    t.continuity = VSS_CONTINUITY_COMPARABLE;
    return t;
}

/* ---- flow stand-in ----------------------------------------------------- */

static VssPlanDescriptor s_catalog[AUDIO_ASSET_CUE_ROWS];
static VssCandidateObservation s_candidates[VSS_CANDIDATE_CAPACITY];
static size_t s_count;
static VssReadBasis s_basis;
static VssSelectionDecision s_decision;
static VssRequestDisposition s_disposition;
static VssPlaybackObservation s_view;
static bool s_startsAllowed = true;

static void BuildCatalog(void)
{
    size_t i;

    memset(s_catalog, 0, sizeof s_catalog);
    for (i = 0u; i < AUDIO_ASSET_CUE_ROWS; ++i)
    {
        const AudioAsset_CueRow *row = AudioAsset_CueAt(i);
        VssPlanDescriptor *p = &s_catalog[i];

        CHECK(row != NULL);
        if (row == NULL)
        {
            continue;
        }
        p->key = (uint64_t)i + 1u;
        p->signal = (VssInputSignal)row->signal;
        p->value = (VssInputValue)row->value;
        p->image = 1u;
        p->defined = true;
        p->available = true;
        p->cueCount = 1u;
        p->cues[0].asset = row->asset;
        p->repeatCount = 1u;
        CHECK(VssPolicy_ValidPlan(p));
    }
}

static void View(void)
{
    CHECK(Playback_Read(&s_view));
}

static void Collect(const VssTimeEvidence *now)
{
    View();
    CHECK(Store_ReadCandidates(now, VSS_TEST, s_candidates, VSS_CANDIDATE_CAPACITY, &s_count));
    memset(&s_basis, 0, sizeof s_basis);
    s_basis.origin = VSS_TEST;
    s_basis.time = *now;
    s_basis.validity = VSS_READ_VALID;
    s_basis.storeRevision = Store_ReadRevision(VSS_TEST);
    s_basis.pbRevision = s_view.revision;
    s_basis.startsAllowed = s_startsAllowed;
}

/* One flow pass at the current time. */
static void Pass(void)
{
    VssTimeEvidence now = Now();
    VssStorePlaybackSnapshot snapshot;
    VssPolicyCurrent current;

    AudioStream_Service();
    Playback_ReadStoreSnapshot(VSS_TEST, &snapshot);
    (void)Store_AdvanceDeadlines(&now, &snapshot);
    Collect(&now);
    Playback_Advance(s_candidates, s_count, &now);
    Collect(&now);
    Playback_ReadPolicyCurrent(&current);
    s_decision = Select_Choose(s_candidates, s_count, &current, s_catalog,
                               AUDIO_ASSET_CUE_ROWS, &s_basis);
    s_disposition = Playback_RequestTransition(&s_decision, s_candidates, s_count, &s_basis, &now);
    View();
}

/* Raises the next boundary and runs one pass. */
static void Boundary(void)
{
    s_boundary++;
    HalMock_SetTicks(BoundaryTick(s_boundary));
    HalMock_Boundary();
    Pass();
}

/* Boundaries until no session exists; false when the limit was reached. */
static bool RunUntilIdle(uint32_t maxBoundaries)
{
    uint32_t i;

    for (i = 0u; i < maxBoundaries; ++i)
    {
        View();
        if (!s_view.present)
        {
            return true;
        }
        Boundary();
    }
    View();
    return !s_view.present;
}

static void Setup(void)
{
    HalMock_Reset();
    AudioTx_ResetForTest();
    AudioAsset_OverrideForTest(0u, NULL, 0u);
    AudioAsset_Init();
    VssTime_ResetForTest();
    CHECK(VssTime_Init(HalTimer_GetTicks));
    Store_TestReset();
    StorePlayback_TestReset();
    AudioStream_Init();
    Playback_ResetForTest();
    CHECK(AudioTx_Init());
    CHECK(AudioTx_Start());
    BuildCatalog();
    s_boundary = 0u;
    s_startsAllowed = true;
    HalMock_SetTicks(0u);
}

/* ---- inputs ------------------------------------------------------------ */

static VssOccurrenceKey AddEvent(uint32_t id, VssInputSignal signal)
{
    VssOneShotEvent e;
    uint64_t now = VssTime_NowMs();

    memset(&e, 0, sizeof e);
    e.signal = signal;
    e.meta.key.origin = VSS_TEST;
    e.meta.key.source = VSS_SOURCE_DOMAIN;
    e.meta.key.generation = 1u;
    e.meta.key.occurrenceId = id;
    e.meta.deliveryOrder = id;
    e.meta.original = Now();
    e.meta.receivedUseLimit = 500u;
    e.meta.useLimit = 500u;
    e.meta.validUntil = now + 500u;
    e.meta.availability = VSS_AVAILABLE;
    CHECK_EQ_U(Store_ApplyInput(&e, NULL, NULL), VSS_STORE_ACCEPTED);
    return e.meta.key;
}

static uint32_t s_occupantSequence;

/* One occupant-hazard evaluation (TEST origin, DOMAIN boot 1), valid for
 * the warning's 1000 ms use limit. */
static void Occupant(bool active)
{
    VssStateUpdate s;
    uint64_t now = VssTime_NowMs();

    memset(&s, 0, sizeof s);
    s.signal = VSS_SIGNAL_OCCUPANT_HAZARD;
    s.value = active ? VSS_VALUE_ACTIVE : VSS_VALUE_CLEAR;
    s.meta.origin = VSS_TEST;
    s.meta.domainBoot = 1u;
    s.meta.deliveryOrder = ++s_occupantSequence;
    s.meta.evidenceSource = VSS_EVIDENCE_PI_VIA_CIS;
    s.meta.evidenceBoot = 1u;
    s.meta.evidenceSequence = s_occupantSequence;
    s.meta.original = Now();
    s.meta.useLimit = 1000u;
    s.meta.validUntil = now + 1000u;
    s.meta.reason = VSS_REASON_NONE;
    s.meta.availability = VSS_AVAILABLE;
    s.evaluatedAt = Now();
    CHECK_EQ_U(Store_ApplyInput(NULL, &s, NULL), VSS_STORE_UPDATED);
}

static bool Record(const VssOccurrenceKey *key, VssOccurrenceObservation *r)
{
    return Store_ReadOccurrence(key, r);
}

static const VssCandidateObservation *Candidate(const VssOccurrenceKey *key)
{
    size_t i;

    for (i = 0u; i < s_count; ++i)
    {
        if ((s_candidates[i].mode == VSS_MODE_EVENT) &&
            (s_candidates[i].key.occurrenceId == key->occurrenceId) &&
            (s_candidates[i].key.generation == key->generation))
        {
            return &s_candidates[i];
        }
    }
    return NULL;
}

/* MPEG-1 Layer III frames at 44.1 kHz: well-formed, but the output refuses
 * the sample rate, so the stream reports a decode fault. */
#define BAD_FRAME_BYTES 417u
#define BAD_FRAMES      12u
static uint8_t s_badAsset[BAD_FRAME_BYTES * BAD_FRAMES];

static uint32_t BuildBadFrames(void)
{
    uint32_t i;

    memset(s_badAsset, 0, sizeof s_badAsset);
    for (i = 0u; i < BAD_FRAMES; ++i)
    {
        s_badAsset[i * BAD_FRAME_BYTES + 0u] = 0xFFu;
        s_badAsset[i * BAD_FRAME_BYTES + 1u] = 0xFBu;
        s_badAsset[i * BAD_FRAME_BYTES + 2u] = 0x90u;
        s_badAsset[i * BAD_FRAME_BYTES + 3u] = 0xC0u;
    }
    return BAD_FRAME_BYTES * BAD_FRAMES;
}

/* ---- cases -------------------------------------------------------------- */

/* Adoption binds the record and starts the stream; STARTED becomes the
 * first-start fact at the boundary time; COMPLETED closes the record. The
 * boundary time and the system clock are the same axis. */
static void test_event_session_plays_to_completion(void)
{
    VssOccurrenceKey k;
    VssOccurrenceObservation r;
    const VssCandidateObservation *c;
    uint64_t revisionBefore;

    Setup();
    k = AddEvent(1u, VSS_SIGNAL_WELCOME);
    revisionBefore = Store_ReadRevision(VSS_TEST);
    Pass();
    CHECK_EQ_U(s_decision.action, VSS_SELECTION_CHOOSE);
    CHECK_EQ_U(s_disposition, VSS_REQUEST_ADOPTED);
    CHECK(s_view.present);
    CHECK_EQ_U(s_view.phase, PLAYBACK_STARTING);
    CHECK_EQ_U(s_view.session, 1u);
    CHECK_EQ_U(s_view.token, 1u);
    CHECK(!s_view.started);
    CHECK(!AudioStream_IsIdle());
    CHECK(Store_ReadRevision(VSS_TEST) > revisionBefore);   /* binding + attempted */
    CHECK(Record(&k, &r));
    CHECK_EQ_U(r.state, VSS_OCCURRENCE_PENDING);
    Collect(&s_basis.time);
    c = Candidate(&k);
    CHECK(c != NULL);
    if (c != NULL)
    {
        CHECK(!c->newSessionAllowed);   /* bound to this session */
    }

    /* A second pass at the same time keeps the session. */
    Pass();
    CHECK_EQ_U(s_decision.action, VSS_SELECTION_KEEP);
    CHECK_EQ_U(s_disposition, VSS_REQUEST_KEPT);
    CHECK_EQ_U(s_view.session, 1u);

    /* Boundary 1 takes the first PCM; it starts to play at boundary 2. */
    Boundary();
    CHECK_EQ_U(s_view.phase, PLAYBACK_STARTING);
    Boundary();
    CHECK_EQ_U(s_view.phase, PLAYBACK_ACTIVE);
    CHECK(s_view.started);
    CHECK(Record(&k, &r));
    CHECK_EQ_U(r.state, VSS_OCCURRENCE_STARTED);
    CHECK(r.firstStartFact.present);
    CHECK_EQ_U(r.firstStartFact.kind, VSS_FACT_FIRST_ACTUAL_START);
    CHECK_EQ_U(r.firstStartFact.session, 1u);
    CHECK_EQ_U(r.firstStartFact.occurredAt.earliest, BoundaryMs(2u));
    CHECK_EQ_U(r.firstStartFact.occurredAt.earliest, VssTime_NowMs());
    CHECK_EQ_U(r.firstStartFact.occurredAt.latest, BoundaryMs(2u) + PLAYBACK_FACT_SPAN_MS);
    CHECK_EQ_U(r.firstStartFact.occurredAt.domain, VSS_TEST);
    CHECK(r.firstStartFact.basisRevision != 0u);

    /* WELCOME is 47 halves: it completes at boundary 2 + 47. */
    CHECK(RunUntilIdle(80u));
    CHECK_EQ_U(s_boundary, 49u);
    CHECK_EQ_U(s_view.lastEnd, PLAYBACK_END_COMPLETED);
    CHECK_EQ_U(s_view.lastSession, 1u);
    CHECK_EQ_U(s_view.lastEndAtMs, BoundaryMs(49u));
    CHECK_EQ_U(s_view.lastEndAtMs, VssTime_NowMs());
    CHECK(Record(&k, &r));
    CHECK_EQ_U(r.state, VSS_OCCURRENCE_COMPLETED);
    CHECK(r.finalFact.present);
    CHECK_EQ_U(r.finalFact.kind, VSS_FACT_WHOLE_COMPLETED);
    CHECK_EQ_U(r.finalFact.occurredAt.earliest, BoundaryMs(49u));
    CHECK(!r.replayAllowed);
    CHECK(AudioStream_IsIdle());
    /* The binding is retired: the record is no candidate any more. */
    Pass();
    CHECK_EQ_U(s_decision.action, VSS_SELECTION_WAIT);
    CHECK(!s_view.present);
    c = Candidate(&k);
    CHECK(c != NULL);
    if (c != NULL)
    {
        CHECK_EQ_U(c->validity, VSS_CANDIDATE_NOT_APPLICABLE);
    }
    CHECK_EQ_U(s_view.storeRefusals, 0u);
    CHECK_EQ_U(s_view.startRefusals, 0u);
    CHECK_EQ_U(s_view.outputFaults, 0u);
    CHECK_EQ_U(s_view.assetFaults, 0u);
    CHECK_EQ_U(s_view.strayFacts, 0u);
}

/* The system clock and the boundary times agree at every boundary. */
static void test_boundary_time_is_the_system_clock(void)
{
    uint32_t k;

    Setup();
    for (k = 1u; k <= 40u; ++k)
    {
        Boundary();
        CHECK_EQ_U(VssTime_NowMs(), BoundaryMs(k));
        CHECK_EQ_U(VssTime_NowMs(), VssTime_TicksToMs(HalTimer_GetTicks()));
    }
}

/* An event of the same rank waits: the running session is kept. */
static void test_same_rank_event_is_kept(void)
{
    VssOccurrenceKey second;
    VssOccurrenceObservation r;

    Setup();
    (void)AddEvent(1u, VSS_SIGNAL_WELCOME);
    Pass();
    Boundary();
    Boundary();
    CHECK_EQ_U(s_view.phase, PLAYBACK_ACTIVE);
    second = AddEvent(2u, VSS_SIGNAL_WELCOME);
    Boundary();
    CHECK_EQ_U(s_decision.action, VSS_SELECTION_KEEP);
    CHECK_EQ_U(s_disposition, VSS_REQUEST_KEPT);
    CHECK_EQ_U(s_view.session, 1u);
    CHECK_EQ_U(s_view.phase, PLAYBACK_ACTIVE);
    CHECK(Record(&second, &r));
    CHECK_EQ_U(r.state, VSS_OCCURRENCE_PENDING);
}

/* The output begins after the use limit: STORE refuses the first start, the
 * session is stopped and the record closed as uncertain. */
static void test_first_start_refused_after_use_limit(void)
{
    VssOccurrenceKey k;
    VssOccurrenceObservation r;
    const VssCandidateObservation *c;
    uint32_t i;

    Setup();
    k = AddEvent(1u, VSS_SIGNAL_WELCOME);
    s_startsAllowed = false;
    for (i = 0u; i < 23u; ++i)
    {
        Boundary();
        CHECK(!s_view.present);
        CHECK_EQ_U(s_decision.action, VSS_SELECTION_WAIT);
    }
    CHECK_EQ_U(VssTime_NowMs(), 490u);
    s_startsAllowed = true;
    Pass();
    CHECK_EQ_U(s_disposition, VSS_REQUEST_ADOPTED);
    Boundary();                         /* 512 ms: first PCM written */
    CHECK_EQ_U(s_view.phase, PLAYBACK_STARTING);
    Boundary();                         /* 532 ms: STARTED, beyond the 500 ms limit */
    CHECK_EQ_U(VssTime_NowMs(), BoundaryMs(25u));
    CHECK(VssTime_NowMs() >= 500u);
    CHECK(s_view.present);
    CHECK_EQ_U(s_view.phase, PLAYBACK_STOPPING);
    CHECK(s_view.startUncertain);
    CHECK(s_view.stopRequested);
    CHECK(!s_view.started);
    CHECK(Record(&k, &r));
    CHECK_EQ_U(r.state, VSS_OCCURRENCE_UNCERTAIN_FINAL);
    CHECK(!r.firstStartFact.present);
    CHECK(r.finalFact.present);
    CHECK_EQ_U(r.finalFact.kind, VSS_FACT_START_OUTCOME_UNCERTAIN);
    CHECK_EQ_U(r.finalFact.occurredAt.earliest, BoundaryMs(25u));
    CHECK(!r.replayAllowed);
    /* The stop takes effect within two boundaries. */
    CHECK(RunUntilIdle(3u));
    CHECK_EQ_U(s_view.lastEnd, PLAYBACK_END_START_UNCERTAIN);
    CHECK_EQ_U(s_view.storeRefusals, 0u);
    CHECK(Record(&k, &r));
    CHECK_EQ_U(r.state, VSS_OCCURRENCE_UNCERTAIN_FINAL);
    Pass();
    CHECK(!s_view.present);
    c = Candidate(&k);
    CHECK(c != NULL);
    if (c != NULL)
    {
        CHECK_EQ_U(c->validity, VSS_CANDIDATE_NOT_APPLICABLE);
        CHECK(!c->newSessionAllowed);
    }
}

/* A stop before any output: no start is recorded, the binding is retired as
 * no-start and the record can be selected again within its use limit. */
static void test_stop_before_output_allows_reselection(void)
{
    VssOccurrenceKey k;
    VssOccurrenceObservation r;
    const VssCandidateObservation *c;

    Setup();
    k = AddEvent(1u, VSS_SIGNAL_LOCK_COMPLETE);
    Pass();
    CHECK_EQ_U(s_disposition, VSS_REQUEST_ADOPTED);
    CHECK_EQ_U(s_view.phase, PLAYBACK_STARTING);

    s_startsAllowed = false;
    Pass();
    CHECK_EQ_U(s_view.phase, PLAYBACK_STOPPING);
    CHECK(s_view.stopRequested);
    CHECK(AudioStream_IsIdle());        /* nothing was written: STOPPED at once */
    Pass();
    CHECK(!s_view.present);
    CHECK_EQ_U(s_view.lastEnd, PLAYBACK_END_NO_START);
    CHECK_EQ_U(s_view.lastSession, 1u);
    CHECK(Record(&k, &r));
    CHECK_EQ_U(r.state, VSS_OCCURRENCE_PENDING);
    CHECK(!r.firstStartFact.present);
    CHECK(!r.finalFact.present);
    CHECK(r.replayAllowed);
    c = Candidate(&k);
    CHECK(c != NULL);
    if (c != NULL)
    {
        CHECK_EQ_U(c->validity, VSS_CANDIDATE_APPLICABLE);
        CHECK(c->newSessionAllowed);
    }
    CHECK_EQ_U(s_decision.action, VSS_SELECTION_WAIT);   /* starts not allowed */

    s_startsAllowed = true;
    Pass();
    CHECK_EQ_U(s_disposition, VSS_REQUEST_ADOPTED);
    CHECK_EQ_U(s_view.session, 2u);
    CHECK(RunUntilIdle(40u));
    CHECK_EQ_U(s_view.lastEnd, PLAYBACK_END_COMPLETED);
    CHECK(Record(&k, &r));
    CHECK_EQ_U(r.state, VSS_OCCURRENCE_COMPLETED);
    CHECK_EQ_U(r.firstStartFact.session, 2u);
    CHECK_EQ_U(r.finalFact.session, 2u);
    CHECK_EQ_U(s_view.storeRefusals, 0u);
}

/* A higher-rank warning replaces the running event: the event is stopped
 * after its start (WHOLE_INTERRUPTED), the warning loops until it is no
 * longer an applicable candidate, then stops. */
static void test_replace_interrupts_event_and_stateful_runs_until_cleared(void)
{
    VssOccurrenceKey k;
    VssOccurrenceObservation r;
    uint32_t i;
    uint64_t stoppedAt;

    Setup();
    s_occupantSequence = 0u;
    k = AddEvent(1u, VSS_SIGNAL_WELCOME);
    Pass();
    Boundary();
    Boundary();
    CHECK_EQ_U(s_view.phase, PLAYBACK_ACTIVE);

    Occupant(true);
    Boundary();
    CHECK_EQ_U(s_decision.action, VSS_SELECTION_REPLACE);
    CHECK_EQ_U(s_disposition, VSS_REQUEST_WAIT);
    CHECK_EQ_U(s_view.session, 1u);
    CHECK_EQ_U(s_view.phase, PLAYBACK_STOPPING);
    CHECK(s_view.stopRequested);
    for (i = 0u; i < 3u; ++i)
    {
        Occupant(true);
        Boundary();
        if (s_view.session != 1u)
        {
            break;
        }
    }
    CHECK(i < 3u);
    CHECK_EQ_U(s_view.lastSession, 1u);
    CHECK_EQ_U(s_view.lastEnd, PLAYBACK_END_INTERRUPTED);
    stoppedAt = s_view.lastEndAtMs;
    CHECK(Record(&k, &r));
    CHECK_EQ_U(r.state, VSS_OCCURRENCE_INTERRUPTED);
    CHECK_EQ_U(r.finalFact.kind, VSS_FACT_WHOLE_INTERRUPTED);
    CHECK_EQ_U(r.finalFact.occurredAt.earliest, stoppedAt);
    CHECK(r.finalFact.occurredAt.earliest >= r.firstStartFact.occurredAt.latest);
    CHECK(!r.replayAllowed);

    /* The warning session was adopted in the pass that ended the event. */
    CHECK(s_view.present);
    CHECK_EQ_U(s_view.session, 2u);
    CHECK_EQ_U(s_view.candidate.mode, VSS_MODE_STATEFUL);
    CHECK_EQ_U(s_view.candidate.signal, VSS_SIGNAL_OCCUPANT_HAZARD);
    CHECK_EQ_U(s_view.plan.cues[0].asset, AUDIO_ASSET_ID_OCCUPANT);
    /* It loops well past the cue's 101 halves while the warning is refreshed. */
    for (i = 0u; i < 150u; ++i)
    {
        Occupant(true);
        Boundary();
        CHECK_EQ_U(s_view.session, 2u);
        CHECK_EQ_U(s_disposition, VSS_REQUEST_KEPT);
    }
    CHECK_EQ_U(s_view.phase, PLAYBACK_ACTIVE);
    CHECK(s_view.started);

    /* Cleared: the candidate is gone, the session is stopped. */
    Occupant(false);
    Boundary();
    CHECK_EQ_U(s_view.phase, PLAYBACK_STOPPING);
    CHECK(RunUntilIdle(3u));
    CHECK_EQ_U(s_view.lastSession, 2u);
    CHECK_EQ_U(s_view.lastEnd, PLAYBACK_END_STOPPED);
    CHECK(AudioStream_IsIdle());
    CHECK_EQ_U(s_view.storeRefusals, 0u);
    CHECK_EQ_U(s_view.strayFacts, 0u);
    Boundary();
    CHECK(!s_view.present);
}

/* The warning's evaluation stops arriving: after its use limit it is no
 * longer applicable and the session stops. */
static void test_stateful_stops_when_evaluations_stop(void)
{
    uint32_t i;

    Setup();
    s_occupantSequence = 0u;
    Occupant(true);
    Pass();
    CHECK_EQ_U(s_disposition, VSS_REQUEST_ADOPTED);
    for (i = 0u; i < 4u; ++i)
    {
        Occupant(true);
        Boundary();
    }
    CHECK_EQ_U(s_view.phase, PLAYBACK_ACTIVE);
    /* No refresh: the 1000 ms use limit is about 47 boundaries. */
    for (i = 0u; i < 60u; ++i)
    {
        Boundary();
        if (!s_view.present)
        {
            break;
        }
    }
    CHECK(!s_view.present);
    CHECK_EQ_U(s_view.lastEnd, PLAYBACK_END_STOPPED);
    CHECK(i >= 40u);
}

/* A transmit fault ends the session as a fault, the record as interrupted,
 * and no later session is started. */
static void test_transmit_fault_latches_output(void)
{
    VssOccurrenceKey k;
    VssOccurrenceObservation r;
    uint64_t revision;

    Setup();
    k = AddEvent(1u, VSS_SIGNAL_WELCOME);
    Pass();
    Boundary();
    Boundary();
    Boundary();
    CHECK_EQ_U(s_view.phase, PLAYBACK_ACTIVE);
    HalMock_SaiError();
    Boundary();
    CHECK(!s_view.present);
    CHECK_EQ_U(s_view.lastEnd, PLAYBACK_END_FAULT);
    CHECK_EQ_U(s_view.outputFaults, 1u);
    CHECK_EQ_U(s_view.assetFaults, 0u);
    CHECK(s_view.outputFailed);
    CHECK((s_view.lastFaultMask & AUDIO_TX_FAULT_SAI) != 0u);
    CHECK_EQ_U(s_view.lastFaultAtMs, s_view.lastEndAtMs);
    CHECK(Record(&k, &r));
    CHECK_EQ_U(r.state, VSS_OCCURRENCE_INTERRUPTED);
    CHECK_EQ_U(r.finalFact.kind, VSS_FACT_WHOLE_INTERRUPTED);
    CHECK(AudioTx_FaultMask() != 0u);

    /* A new event is not started and STORE is not touched. */
    (void)AddEvent(2u, VSS_SIGNAL_LOCK_ERROR);
    revision = Store_ReadRevision(VSS_TEST);
    Pass();
    CHECK_EQ_U(s_decision.action, VSS_SELECTION_CHOOSE);
    CHECK_EQ_U(s_disposition, VSS_REQUEST_WAIT);
    CHECK(!s_view.present);
    CHECK_EQ_U(Store_ReadRevision(VSS_TEST), revision);
    CHECK_EQ_U(s_view.storeRefusals, 0u);
    CHECK_EQ_U(s_view.startRefusals, 0u);
}

/* A decode failure before any output is an asset fault. The record keeps
 * its start opportunity, so the pass that sees the fault adopts it again;
 * once the asset is readable it plays. */
static void test_decode_fault_counts_as_asset_fault(void)
{
    VssOccurrenceKey k;
    VssOccurrenceObservation r;
    uint32_t size = BuildBadFrames();
    uint32_t i;

    Setup();
    AudioAsset_OverrideForTest(AUDIO_ASSET_ID_WELCOME, s_badAsset, size);
    k = AddEvent(1u, VSS_SIGNAL_WELCOME);
    Pass();
    CHECK_EQ_U(s_disposition, VSS_REQUEST_ADOPTED);
    for (i = 0u; i < 4u; ++i)
    {
        Boundary();
        if (s_view.lastSession == 1u)
        {
            break;
        }
    }
    CHECK(i < 4u);
    CHECK_EQ_U(s_view.lastEnd, PLAYBACK_END_FAULT);
    CHECK_EQ_U(s_view.assetFaults, 1u);
    CHECK_EQ_U(s_view.outputFaults, 0u);
    CHECK(!s_view.outputFailed);
    CHECK_EQ_U(s_view.lastFaultMask, AUDIO_STREAM_FAULT_DECODE);
    CHECK_EQ_U(AudioTx_FaultMask(), 0u);
    /* Retried at once with the same asset. */
    CHECK(s_view.present);
    CHECK_EQ_U(s_view.session, 2u);
    CHECK_EQ_U(s_disposition, VSS_REQUEST_ADOPTED);

    CHECK(Record(&k, &r));
    CHECK_EQ_U(r.state, VSS_OCCURRENCE_PENDING);
    CHECK(!r.firstStartFact.present);
    CHECK(r.replayAllowed);

    /* The decoder reads the asset as it renders: once readable, the retried
     * session plays. */
    AudioAsset_OverrideForTest(0u, NULL, 0u);
    CHECK(RunUntilIdle(60u));
    CHECK_EQ_U(s_view.lastSession, 2u);
    CHECK_EQ_U(s_view.lastEnd, PLAYBACK_END_COMPLETED);
    CHECK_EQ_U(s_view.assetFaults, 1u);
    CHECK(Record(&k, &r));
    CHECK_EQ_U(r.state, VSS_OCCURRENCE_COMPLETED);
    CHECK_EQ_U(r.firstStartFact.session, 2u);
    CHECK_EQ_U(s_view.storeRefusals, 0u);
}

/* The stream refuses to prepare the asset: a start refusal, no session, and
 * the record is released for another attempt. */
static void test_prepare_refusal_releases_the_record(void)
{
    VssOccurrenceKey k;
    VssOccurrenceObservation r;
    const VssCandidateObservation *c;

    Setup();
    AudioAsset_OverrideForTest(AUDIO_ASSET_ID_LOCK, s_badAsset, 0u);
    k = AddEvent(1u, VSS_SIGNAL_LOCK_COMPLETE);
    Pass();
    CHECK_EQ_U(s_decision.action, VSS_SELECTION_CHOOSE);
    CHECK_EQ_U(s_disposition, VSS_REQUEST_REJECTED);
    CHECK(!s_view.present);
    CHECK_EQ_U(s_view.lastEnd, PLAYBACK_END_NOT_STARTED);
    CHECK_EQ_U(s_view.lastSession, 1u);
    CHECK_EQ_U(s_view.startRefusals, 1u);
    CHECK_EQ_U(s_view.storeRefusals, 0u);
    CHECK(AudioStream_IsIdle());
    CHECK(Record(&k, &r));
    CHECK_EQ_U(r.state, VSS_OCCURRENCE_PENDING);
    CHECK(r.replayAllowed);
    Pass();
    c = Candidate(&k);
    CHECK(c != NULL && c->newSessionAllowed);
    /* Every pass retries while the asset stays unreadable. */
    CHECK_EQ_U(s_disposition, VSS_REQUEST_REJECTED);
    CHECK_EQ_U(s_view.startRefusals, 2u);

    AudioAsset_OverrideForTest(0u, NULL, 0u);
    Pass();
    CHECK_EQ_U(s_disposition, VSS_REQUEST_ADOPTED);
    CHECK_EQ_U(s_view.session, 3u);
    CHECK(RunUntilIdle(40u));
    CHECK_EQ_U(s_view.lastEnd, PLAYBACK_END_COMPLETED);
    CHECK(Record(&k, &r));
    CHECK_EQ_U(r.state, VSS_OCCURRENCE_COMPLETED);
    CHECK_EQ_U(r.firstStartFact.session, 3u);
}

/* Without reclaim the 33rd event of a run finds no binding: a STORE
 * refusal, no session. After a reclaim it plays. */
static void test_binding_exhaustion_is_reported_and_reclaim_clears_it(void)
{
    uint32_t i;
    VssOccurrenceKey k;
    VssOccurrenceObservation r;
    VssTimeEvidence now;

    Setup();
    for (i = 1u; i <= VSS_PLAYBACK_BINDINGS_CAPACITY; ++i)
    {
        k = AddEvent(i, VSS_SIGNAL_LOCK_COMPLETE);
        Pass();
        CHECK_EQ_U(s_disposition, VSS_REQUEST_ADOPTED);
        CHECK(RunUntilIdle(40u));
        CHECK_EQ_U(s_view.lastEnd, PLAYBACK_END_COMPLETED);
    }
    CHECK_EQ_U(s_view.storeRefusals, 0u);
    k = AddEvent(33u, VSS_SIGNAL_LOCK_COMPLETE);
    Pass();
    CHECK_EQ_U(s_decision.action, VSS_SELECTION_CHOOSE);
    CHECK_EQ_U(s_disposition, VSS_REQUEST_REJECTED);
    CHECK(!s_view.present);
    CHECK_EQ_U(s_view.lastEnd, PLAYBACK_END_NOT_STARTED);
    CHECK_EQ_U(s_view.storeRefusals, 1u);
    CHECK_EQ_U(s_view.startRefusals, 0u);
    CHECK(AudioStream_IsIdle());
    CHECK(Record(&k, &r));
    CHECK_EQ_U(r.state, VSS_OCCURRENCE_PENDING);

    now = Now();
    CHECK(Store_Reclaim(&now) >= 28u);
    Pass();
    CHECK_EQ_U(s_disposition, VSS_REQUEST_ADOPTED);
    CHECK(RunUntilIdle(40u));
    CHECK_EQ_U(s_view.lastEnd, PLAYBACK_END_COMPLETED);
    CHECK(Record(&k, &r));
    CHECK_EQ_U(r.state, VSS_OCCURRENCE_COMPLETED);
    CHECK_EQ_U(s_view.storeRefusals, 1u);
}

/* Argument and basis guards. */
static void test_request_guards(void)
{
    VssTimeEvidence now;
    VssSelectionDecision d;
    VssReadBasis b;

    Setup();
    (void)AddEvent(1u, VSS_SIGNAL_WELCOME);
    now = Now();
    Collect(&now);
    CHECK_EQ_U(Playback_RequestTransition(NULL, s_candidates, s_count, &s_basis, &now), VSS_REQUEST_REJECTED);
    memset(&d, 0, sizeof d);
    d.action = VSS_SELECTION_CHOOSE;
    CHECK_EQ_U(Playback_RequestTransition(&d, s_candidates, s_count, NULL, &now), VSS_REQUEST_REJECTED);
    CHECK_EQ_U(Playback_RequestTransition(&d, s_candidates, s_count, &s_basis, NULL), VSS_REQUEST_REJECTED);
    /* A stale basis waits. */
    b = s_basis;
    b.pbRevision = s_basis.pbRevision + 1u;
    d.basis = b;
    CHECK_EQ_U(Playback_RequestTransition(&d, s_candidates, s_count, &b, &now), VSS_REQUEST_WAIT);
    b = s_basis;
    b.storeRevision = s_basis.storeRevision + 1u;
    d.basis = b;
    CHECK_EQ_U(Playback_RequestTransition(&d, s_candidates, s_count, &b, &now), VSS_REQUEST_WAIT);
    /* A plan with more than one cue is not played. */
    Pass();
    CHECK_EQ_U(s_disposition, VSS_REQUEST_ADOPTED);
    CHECK(RunUntilIdle(60u));
    Setup();
    (void)AddEvent(1u, VSS_SIGNAL_WELCOME);
    s_catalog[0].cueCount = 2u;
    s_catalog[0].cues[1].asset = AUDIO_ASSET_ID_GOODBYE;
    Pass();
    CHECK_EQ_U(s_decision.action, VSS_SELECTION_CHOOSE);
    CHECK_EQ_U(s_disposition, VSS_REQUEST_WAIT);
    CHECK(!s_view.present);
    CHECK_EQ_U(Store_ReadRevision(VSS_TEST), s_basis.storeRevision);
}

int main(void)
{
    test_event_session_plays_to_completion();
    test_boundary_time_is_the_system_clock();
    test_same_rank_event_is_kept();
    test_first_start_refused_after_use_limit();
    test_stop_before_output_allows_reselection();
    test_replace_interrupts_event_and_stateful_runs_until_cleared();
    test_stateful_stops_when_evaluations_stop();
    test_transmit_fault_latches_output();
    test_decode_fault_counts_as_asset_fault();
    test_prepare_refusal_releases_the_record();
    test_binding_exhaustion_is_reported_and_reclaim_clears_it();
    test_request_guards();
    TEST_FINISH();
}
