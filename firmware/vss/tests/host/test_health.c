/* Service health on the real playback, audio stream, transmit driver, codec
 * control and STORE, with the HAL stand-in raising the output boundaries
 * every 1024 frames at 48 kHz on the 800 Hz tick: every row of the state
 * table, when a report number is created, which fault is reported as the
 * last one, and that a report read again for resubmission keeps its
 * number. Each pass is a flow pass without the reclaim sweep, followed by
 * a health evaluation. */

#include "test_util.h"
#include "hal_mock.h"

#include "../../src/service/health/vss_health.h"
#include "../../src/service/input/vss_input_net.h"
#include "../../src/service/playback/vss_playback.h"
#include "../../src/service/audio_stream/audio_stream.h"
#include "../../src/service/asset/audio_asset.h"
#include "../../src/service/asset/vss_cue_table.h"
#include "../../src/driver/audio_tx/audio_tx.h"
#include "../../src/driver/audio_control/audio_control.h"
#include "../../src/infra/vss_time.h"

#include <string.h>

#define BIT(kind) ((uint8_t)(1u << (unsigned)(kind)))

/* ---- time line --------------------------------------------------------- */

static uint32_t s_boundary;

static uint32_t BoundaryTick(uint32_t k)
{
    return (uint32_t)(((uint64_t)k * 256u) / 15u);
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

static VssTimeEvidence At(uint64_t ms)
{
    VssTimeEvidence t = Now();

    t.earliest = ms;
    t.latest = ms;
    return t;
}

/* Moves the clock forward without output boundaries (only for cases that
 * run no further pass afterwards). ms must be a multiple of 5. */
static void JumpToMs(uint64_t ms)
{
    HalMock_SetTicks((uint32_t)((ms * 4u) / 5u));
    (void)VssTime_NowMs();
}

/* ---- flow stand-in ----------------------------------------------------- */

static VssPlanDescriptor s_catalog[AUDIO_ASSET_CUE_ROWS];
static VssCandidateObservation s_candidates[VSS_CANDIDATE_CAPACITY];
static size_t s_count;
static VssReadBasis s_basis;
static VssSelectionDecision s_decision;
static VssRequestDisposition s_disposition;
static VssPlaybackObservation s_view;
static VssHealthObservation s_health;
static VssStatusSnapshot s_status;

static void BuildCatalog(void)
{
    size_t i;

    memset(s_catalog, 0, sizeof s_catalog);
    for (i = 0u; i < AUDIO_ASSET_CUE_ROWS; ++i)
    {
        const AudioAsset_CueRow *row = AudioAsset_CueAt(i);
        VssPlanDescriptor *p = &s_catalog[i];

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
    }
}

static void View(void)
{
    CHECK(Playback_Read(&s_view));
    CHECK(Health_Read(&s_health));
}

static void Evaluate(void)
{
    VssTimeEvidence now = Now();

    Health_Evaluate(&now);
    View();
}

/* One flow pass at the current time, then a health evaluation. */
static void Pass(void)
{
    VssTimeEvidence now = Now();
    VssStorePlaybackSnapshot snapshot;
    VssPolicyCurrent current;

    AudioStream_Service();
    Playback_ReadStoreSnapshot(VSS_TEST, &snapshot);
    (void)Store_AdvanceDeadlines(&now, &snapshot);
    Playback_Advance(NULL, 0u, &now);
    CHECK(Store_ReadCandidates(&now, VSS_TEST, s_candidates, VSS_CANDIDATE_CAPACITY, &s_count));
    CHECK(Playback_Read(&s_view));
    memset(&s_basis, 0, sizeof s_basis);
    s_basis.origin = VSS_TEST;
    s_basis.time = now;
    s_basis.validity = VSS_READ_VALID;
    s_basis.storeRevision = Store_ReadRevision(VSS_TEST);
    s_basis.pbRevision = s_view.revision;
    s_basis.startsAllowed = true;
    Playback_ReadPolicyCurrent(&current);
    s_decision = Select_Choose(s_candidates, s_count, &current, s_catalog,
                               AUDIO_ASSET_CUE_ROWS, &s_basis);
    s_disposition = Playback_RequestTransition(&s_decision, s_candidates, s_count, &s_basis, &now);
    Health_Evaluate(&now);
    View();
}

static void Boundary(void)
{
    s_boundary++;
    HalMock_SetTicks(BoundaryTick(s_boundary));
    HalMock_Boundary();
    Pass();
}

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

/* Boundaries until the session reported STARTED. */
static bool RunUntilStarted(uint32_t maxBoundaries)
{
    uint32_t i;

    for (i = 0u; i < maxBoundaries; ++i)
    {
        View();
        if (s_view.present && (s_view.lastStartedSession == s_view.session))
        {
            return true;
        }
        Boundary();
    }
    return false;
}

/* Power-on state of every module; bootComplete as the boot sequence would
 * leave it. No evaluation has run yet. */
static VssOrigin s_healthOrigin = VSS_TEST;

static void Setup(bool bootComplete)
{
    HalMock_Reset();
    AudioTx_ResetForTest();
    AudioControl_ResetForTest();
    AudioAsset_OverrideForTest(0u, NULL, 0u);
    AudioAsset_HideForTest(0u);
    AudioAsset_Init();
    VssTime_ResetForTest();
    CHECK(VssTime_Init(HalTimer_GetTicks));
    Input_TestReset();
    Store_TestReset();
    StorePlayback_TestReset();
    AudioStream_Init();
    Playback_ResetForTest();
    Health_Init(s_healthOrigin);
    CHECK(AudioTx_Init());
    CHECK(AudioTx_Start());
    BuildCatalog();
    s_boundary = 0u;
    HalMock_SetTicks(0u);
    if (bootComplete)
    {
        Health_BootComplete();
    }
}

static void OpenInput(void)
{
    VssTimeEvidence now = Now();

    CHECK(Input_OpenAfterSynchronization(VSS_TEST, 1u, &now));
}

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

/* An event that occurred ageMs before now. */
static VssOccurrenceKey AddAgedEvent(uint32_t id, VssInputSignal signal, uint64_t ageMs)
{
    VssOneShotEvent e;
    uint64_t original = VssTime_NowMs() - ageMs;

    memset(&e, 0, sizeof e);
    e.signal = signal;
    e.meta.key.origin = VSS_TEST;
    e.meta.key.source = VSS_SOURCE_DOMAIN;
    e.meta.key.generation = 1u;
    e.meta.key.occurrenceId = id;
    e.meta.deliveryOrder = id;
    e.meta.original = At(original);
    e.meta.receivedUseLimit = 500u;
    e.meta.useLimit = 500u;
    e.meta.validUntil = original + 500u;
    e.meta.availability = VSS_AVAILABLE;
    CHECK_EQ_U(Store_ApplyInput(&e, NULL, NULL), VSS_STORE_ACCEPTED);
    return e.meta.key;
}

static bool Build(void)
{
    VssTimeEvidence now = Now();

    return Health_BuildStatus(&now, &s_status);
}

static void ExpectState(VssServiceState state, VssServiceAvailability availability, uint8_t mask,
                        int line)
{
    if ((s_health.state != (uint8_t)state) || (s_health.availability != (uint8_t)availability) ||
        (s_health.currentMask != mask))
    {
        printf("  (state check at line %d)\n", line);
    }
    CHECK_EQ_U(s_health.state, state);
    CHECK_EQ_U(s_health.availability, availability);
    CHECK_EQ_U(s_health.currentMask, mask);
}
#define EXPECT_STATE(s, a, m) ExpectState((s), (a), (m), __LINE__)

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

/* ---- state table -------------------------------------------------------- */

/* Boot not complete: STARTUP / UNAVAILABLE, nothing current, not
 * accepting even with INPUT open. */
static void test_startup_until_boot_complete(void)
{
    Setup(false);
    Evaluate();
    EXPECT_STATE(VSS_SERVICE_STARTUP, VSS_SERVICE_UNAVAILABLE, 0u);
    CHECK(!s_health.accepting);
    CHECK(Build());
    CHECK_EQ_U(s_status.reportSequence, 1u);
    CHECK_EQ_U(s_status.state, VSS_SERVICE_STARTUP);
    CHECK(!s_status.accepting);
    CHECK(!s_status.lastFaultPresent);

    OpenInput();
    Evaluate();
    CHECK(!s_health.accepting);
    EXPECT_STATE(VSS_SERVICE_STARTUP, VSS_SERVICE_UNAVAILABLE, 0u);

    Health_BootComplete();
    Evaluate();
    EXPECT_STATE(VSS_SERVICE_READY, VSS_SERVICE_FULL, 0u);
    CHECK(s_health.bootComplete);
    CHECK(s_health.accepting);
}

/* accepting = boot complete, not FAULT, a free ledger slot. It does not wait
 * for the first DOMAIN message: the DOMAIN may hold its events until VSS
 * reports that it accepts them. */
static void test_accepting_rule(void)
{
    VssTimeEvidence now;

    /* Boot complete, no DOMAIN message yet: INPUT is still closed. */
    Setup(true);
    Evaluate();
    EXPECT_STATE(VSS_SERVICE_READY, VSS_SERVICE_FULL, 0u);
    CHECK(s_health.accepting);
    CHECK(Build());
    CHECK(s_status.accepting);
    now = Now();
    {
        NetVssStatus net;

        CHECK(VssInputNet_ToNetStatus(&s_status, &now, &net));
        CHECK_EQ_U(net.vss_accepting_events, NET_FLAG_YES);
    }

    /* DEGRADED still accepts. */
    now = Now();
    Health_Report(VSS_FAULT_START, &now);
    Evaluate();
    EXPECT_STATE(VSS_SERVICE_READY, VSS_SERVICE_DEGRADED, BIT(VSS_FAULT_START));
    CHECK(s_health.accepting);

    /* FAULT does not. */
    Health_Report(VSS_FAULT_INIT, &now);
    Evaluate();
    CHECK_EQ_U(s_health.state, VSS_SERVICE_FAULT);
    CHECK(!s_health.accepting);

    /* An output fault while booting is FAULT as well. */
    Setup(false);
    CHECK(!AudioControl_MuteLatched());
    Evaluate();
    CHECK_EQ_U(s_health.state, VSS_SERVICE_FAULT);
    CHECK(!s_health.accepting);
    Health_BootComplete();
    Evaluate();
    CHECK(!s_health.accepting);
}

/* A session is PLAYING from the boundary the stream reported STARTED until
 * it ends; while only prepared it is still READY. */
static void test_ready_playing_ready(void)
{
    Setup(true);
    OpenInput();
    Evaluate();
    EXPECT_STATE(VSS_SERVICE_READY, VSS_SERVICE_FULL, 0u);
    (void)AddEvent(1u, VSS_SIGNAL_WELCOME);
    Pass();
    CHECK_EQ_U(s_disposition, VSS_REQUEST_ADOPTED);
    EXPECT_STATE(VSS_SERVICE_READY, VSS_SERVICE_FULL, 0u);
    Boundary();
    EXPECT_STATE(VSS_SERVICE_READY, VSS_SERVICE_FULL, 0u);
    Boundary();
    CHECK(s_view.started);
    EXPECT_STATE(VSS_SERVICE_PLAYING, VSS_SERVICE_FULL, 0u);
    CHECK(RunUntilIdle(80u));
    CHECK_EQ_U(s_view.lastEnd, PLAYBACK_END_COMPLETED);
    EXPECT_STATE(VSS_SERVICE_READY, VSS_SERVICE_FULL, 0u);
    CHECK(s_health.accepting);
    CHECK_EQ_U(s_health.occurrences, 0u);
}

/* PLAYING needs a start that STORE accepted: output that began after the
 * event's use limit is stopped as uncertain, and the service stays READY
 * while it fades out. */
static void test_uncertain_start_is_not_playing(void)
{
    uint32_t i;

    Setup(true);
    while (VssTime_NowMs() < 1000u)
    {
        Boundary();
    }
    (void)AddAgedEvent(1u, VSS_SIGNAL_WELCOME, 490u);
    Pass();
    CHECK_EQ_U(s_disposition, VSS_REQUEST_ADOPTED);
    CHECK(RunUntilStarted(4u));
    CHECK(s_view.startUncertain);
    CHECK(!s_view.started);
    EXPECT_STATE(VSS_SERVICE_READY, VSS_SERVICE_FULL, 0u);
    for (i = 0u; (i < 4u) && s_view.present; ++i)
    {
        Boundary();
        EXPECT_STATE(VSS_SERVICE_READY, VSS_SERVICE_FULL, 0u);
    }
    CHECK(!s_view.present);
    CHECK_EQ_U(s_view.lastEnd, PLAYBACK_END_START_UNCERTAIN);
}

/* FULL needs every supported cue ready, not only an empty fault mask. With
 * no supported cue left nothing can start: FAULT / UNAVAILABLE. The window
 * anti-pinch cue is in the supported set of the test origin, which plays it,
 * and not in that of the product origin, which does not carry it. A cue
 * that failed to decode is no longer ready. */
static void test_availability_follows_the_supported_cues(void)
{
    const uint32_t all = (1u << AUDIO_ASSET_COUNT) - 1u;
    uint32_t size = BuildBadFrames();
    uint32_t i;

    /* The test origin plays the anti-pinch cue: it is in the supported set. */
    Setup(true);
    Evaluate();
    CHECK_EQ_U(s_health.supportedCueRows, (1u << AUDIO_ASSET_CUE_ROWS) - 1u);
    CHECK_EQ_U(s_health.readyCueRows, (1u << AUDIO_ASSET_CUE_ROWS) - 1u);
    EXPECT_STATE(VSS_SERVICE_READY, VSS_SERVICE_FULL, 0u);
    Setup(true);
    AudioAsset_HideForTest(all & ~(1u << AUDIO_ASSET_ID_ANTI_PINCH));
    Evaluate();
    EXPECT_STATE(VSS_SERVICE_READY, VSS_SERVICE_DEGRADED, BIT(VSS_FAULT_ASSET));
    CHECK(s_health.accepting);

    /* The product origin: anti-pinch is not supported, so it alone is no
     * service. Every other cue ready is FULL. */
    s_healthOrigin = VSS_PRODUCT;
    Setup(true);
    Evaluate();
    CHECK_EQ_U(s_health.supportedCueRows,
               ((1u << AUDIO_ASSET_CUE_ROWS) - 1u) & ~(1u << AUDIO_ASSET_ID_ANTI_PINCH));
    EXPECT_STATE(VSS_SERVICE_READY, VSS_SERVICE_FULL, 0u);
    Setup(true);
    AudioAsset_HideForTest(all & ~(1u << AUDIO_ASSET_ID_ANTI_PINCH));
    Evaluate();
    EXPECT_STATE(VSS_SERVICE_FAULT, VSS_SERVICE_UNAVAILABLE, BIT(VSS_FAULT_ASSET));
    CHECK(!s_health.accepting);
    s_healthOrigin = VSS_TEST;

    Setup(true);
    AudioAsset_HideForTest(all & ~(1u << AUDIO_ASSET_ID_WELCOME));
    Evaluate();
    EXPECT_STATE(VSS_SERVICE_READY, VSS_SERVICE_DEGRADED, BIT(VSS_FAULT_ASSET));
    CHECK(s_health.accepting);

    /* The one ready cue fails to decode: none is left. */
    Setup(true);
    AudioAsset_HideForTest(all & ~(1u << AUDIO_ASSET_ID_WELCOME));
    AudioAsset_OverrideForTest(AUDIO_ASSET_ID_WELCOME, s_badAsset, size);
    Evaluate();
    EXPECT_STATE(VSS_SERVICE_READY, VSS_SERVICE_DEGRADED, BIT(VSS_FAULT_ASSET));
    (void)AddEvent(1u, VSS_SIGNAL_WELCOME);
    Pass();
    CHECK_EQ_U(s_disposition, VSS_REQUEST_ADOPTED);
    for (i = 0u; (i < 10u) && (s_view.assetFaults == 0u); ++i)
    {
        Boundary();
    }
    CHECK_EQ_U(s_view.assetFaults, 1u);
    EXPECT_STATE(VSS_SERVICE_FAULT, VSS_SERVICE_UNAVAILABLE, BIT(VSS_FAULT_ASSET));
    AudioAsset_OverrideForTest(0u, NULL, 0u);
    AudioAsset_HideForTest(0u);
}

/* A cue without data at the first evaluation is an asset fault until reset:
 * READY and PLAYING are DEGRADED. */
static void test_missing_cue_degrades(void)
{
    Setup(true);
    AudioAsset_OverrideForTest(AUDIO_ASSET_ID_OCCUPANT, s_badAsset, 0u);
    Evaluate();
    EXPECT_STATE(VSS_SERVICE_READY, VSS_SERVICE_DEGRADED, BIT(VSS_FAULT_ASSET));
    CHECK(s_health.current[VSS_FAULT_ASSET].present);
    CHECK_EQ_U(s_health.current[VSS_FAULT_ASSET].occurrence, 1u);
    CHECK_EQ_U(s_health.current[VSS_FAULT_ASSET].firstAtMs, 0u);

    AudioAsset_OverrideForTest(0u, NULL, 0u);   /* checked once, at boot */
    (void)AddEvent(1u, VSS_SIGNAL_WELCOME);
    Pass();
    CHECK(RunUntilStarted(4u));
    EXPECT_STATE(VSS_SERVICE_PLAYING, VSS_SERVICE_DEGRADED, BIT(VSS_FAULT_ASSET));
    CHECK(RunUntilIdle(80u));
    EXPECT_STATE(VSS_SERVICE_READY, VSS_SERVICE_DEGRADED, BIT(VSS_FAULT_ASSET));
    CHECK_EQ_U(s_health.occurrences, 1u);
}

/* A decode failure seen by playback is an asset fault at the stream's fault
 * time, and stays current. */
static void test_decode_failure_is_an_asset_fault(void)
{
    uint32_t size = BuildBadFrames();
    uint32_t i;

    Setup(true);
    Evaluate();
    AudioAsset_OverrideForTest(AUDIO_ASSET_ID_LOCK_ERROR, s_badAsset, size);
    (void)AddEvent(1u, VSS_SIGNAL_LOCK_ERROR);
    Pass();
    CHECK_EQ_U(s_disposition, VSS_REQUEST_ADOPTED);
    for (i = 0u; (i < 10u) && (s_view.assetFaults == 0u); ++i)
    {
        Boundary();
    }
    CHECK_EQ_U(s_view.assetFaults, 1u);
    EXPECT_STATE(VSS_SERVICE_READY, VSS_SERVICE_DEGRADED, BIT(VSS_FAULT_ASSET));
    CHECK_EQ_U(s_health.current[VSS_FAULT_ASSET].firstAtMs, s_view.lastFaultAtMs);
    AudioAsset_OverrideForTest(0u, NULL, 0u);
    Boundary();
    Boundary();
    EXPECT_STATE(VSS_SERVICE_READY, VSS_SERVICE_DEGRADED, BIT(VSS_FAULT_ASSET));
    CHECK_EQ_U(s_health.occurrences, 1u);
}

/* A refused Prepare is a START fault; repeated refusals are the same
 * instance; a later session that starts clears it into the recent record. */
static void test_start_refusal_clears_after_a_start(void)
{
    uint32_t occurrence;

    Setup(true);
    Evaluate();
    AudioAsset_OverrideForTest(AUDIO_ASSET_ID_LOCK, s_badAsset, 0u);
    (void)AddEvent(1u, VSS_SIGNAL_LOCK_COMPLETE);
    Pass();
    CHECK_EQ_U(s_disposition, VSS_REQUEST_REJECTED);
    CHECK_EQ_U(s_view.startRefusals, 1u);
    EXPECT_STATE(VSS_SERVICE_READY, VSS_SERVICE_DEGRADED, BIT(VSS_FAULT_START));
    occurrence = s_health.current[VSS_FAULT_START].occurrence;
    CHECK_EQ_U(occurrence, 1u);
    Pass();
    CHECK_EQ_U(s_view.startRefusals, 2u);
    CHECK_EQ_U(s_health.current[VSS_FAULT_START].occurrence, occurrence);

    AudioAsset_OverrideForTest(0u, NULL, 0u);
    Pass();
    CHECK_EQ_U(s_disposition, VSS_REQUEST_ADOPTED);
    EXPECT_STATE(VSS_SERVICE_READY, VSS_SERVICE_DEGRADED, BIT(VSS_FAULT_START));  /* not started yet */
    CHECK(RunUntilStarted(4u));
    EXPECT_STATE(VSS_SERVICE_PLAYING, VSS_SERVICE_FULL, 0u);
    CHECK(!s_health.current[VSS_FAULT_START].present);
    CHECK(s_health.recent[VSS_FAULT_START].present);
    CHECK_EQ_U(s_health.recent[VSS_FAULT_START].occurrence, occurrence);
    CHECK_EQ_U(s_health.recent[VSS_FAULT_START].clearedAtMs, VssTime_NowMs());
}

/* A transmit fault during a session: OUTPUT, FAULT / UNAVAILABLE, until
 * reset. The transfer stop result is shown for diagnosis only. */
static void test_transmit_fault_is_an_output_fault(void)
{
    uint32_t i;

    Setup(true);
    Evaluate();
    (void)AddEvent(1u, VSS_SIGNAL_WELCOME);
    Pass();
    CHECK(RunUntilStarted(4u));
    /* Seen 10 ms after the last boundary: the fault keeps the stream's
     * boundary time, not the evaluation time. */
    HalMock_SaiError();
    HalMock_SetTicks(BoundaryTick(s_boundary) + 8u);
    Pass();
    CHECK(s_view.outputFailed);
    EXPECT_STATE(VSS_SERVICE_FAULT, VSS_SERVICE_UNAVAILABLE, BIT(VSS_FAULT_OUTPUT));
    CHECK((s_health.audioTxFaultMask & AUDIO_TX_FAULT_SAI) != 0u);
    CHECK_EQ_U(s_health.abortState, AUDIO_TX_ABORT_STOPPED);
    CHECK_EQ_U(s_health.current[VSS_FAULT_OUTPUT].firstAtMs, s_view.lastFaultAtMs);
    CHECK(s_health.current[VSS_FAULT_OUTPUT].firstAtMs < VssTime_NowMs());
    for (i = 0u; i < 5u; ++i)
    {
        Pass();
    }
    EXPECT_STATE(VSS_SERVICE_FAULT, VSS_SERVICE_UNAVAILABLE, BIT(VSS_FAULT_OUTPUT));
    CHECK_EQ_U(s_health.occurrences, 1u);
}

/* Boundaries that stop arriving while idle: the driver latches STALL, which
 * HEALTH reads from the driver's mask (no session, no playback fact). An
 * unconfirmed transfer stop is still only a diagnostic value. */
static void test_stall_while_idle_is_an_output_fault(void)
{
    Setup(true);
    HalMock_SetAbortResult(HAL_AUDIO_TX_ABORT_UNCONFIRMED);
    Evaluate();
    Boundary();
    EXPECT_STATE(VSS_SERVICE_READY, VSS_SERVICE_FULL, 0u);
    HalMock_SetTicks(BoundaryTick(s_boundary) + AUDIO_TX_STALL_TICKS + 1u);
    Pass();
    CHECK((AudioTx_FaultMask() & AUDIO_TX_FAULT_STALL) != 0u);
    CHECK_EQ_U(s_view.outputFaults, 0u);
    EXPECT_STATE(VSS_SERVICE_FAULT, VSS_SERVICE_UNAVAILABLE, BIT(VSS_FAULT_OUTPUT));
    CHECK_EQ_U(s_health.abortState, AUDIO_TX_ABORT_UNCONFIRMED);
}

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

/* Clock locks, codec answers with its part number. */
static void BringUpCodec(void)
{
    Queue1(0x6Cu);
    Queue1(0x80u);
    Queue1(0x00u);
    Queue2(0xA011u);
    Queue2(0x0040u);
    Queue2(0x7060u);
    Queue2(0x0200u);
    Queue2(0x0145u);
    CHECK(AudioControl_Init());
    HalMock_ClearLog();
}

/* A mute that was requested and not accepted is an output fault; once the
 * codec takes it the fault clears into the recent record. Never configured,
 * the mute is not accepted either. */
static void test_refused_mute_is_an_output_fault(void)
{
    Setup(true);
    Evaluate();
    BringUpCodec();
    HalMock_FailI2cAt(0u);
    CHECK(!AudioControl_MuteLatched());
    Evaluate();
    EXPECT_STATE(VSS_SERVICE_FAULT, VSS_SERVICE_UNAVAILABLE, BIT(VSS_FAULT_OUTPUT));
    CHECK(s_health.muteRequested);
    CHECK(!s_health.muteAccepted);
    CHECK_EQ_U(s_health.audioTxFaultMask, 0u);

    Queue2(0x0125u);
    CHECK(AudioControl_MuteLatched());
    Evaluate();
    EXPECT_STATE(VSS_SERVICE_READY, VSS_SERVICE_FULL, 0u);
    CHECK(s_health.muteAccepted);
    CHECK(s_health.recent[VSS_FAULT_OUTPUT].present);

    Setup(true);
    CHECK(!AudioControl_MuteLatched());
    Evaluate();
    EXPECT_STATE(VSS_SERVICE_FAULT, VSS_SERVICE_UNAVAILABLE, BIT(VSS_FAULT_OUTPUT));
}

/* INIT is reported by the boot sequence and stays current; FAULT takes
 * precedence over STARTUP. A report without a usable time takes the
 * evaluation time. */
static void test_init_report_is_a_fault_until_reset(void)
{
    VssTimeEvidence bad = Now();

    Setup(false);
    JumpToMs(40u);
    bad.continuity = VSS_CONTINUITY_UNKNOWN;
    Health_Report(VSS_FAULT_INIT, &bad);
    JumpToMs(60u);
    Evaluate();
    EXPECT_STATE(VSS_SERVICE_FAULT, VSS_SERVICE_UNAVAILABLE, BIT(VSS_FAULT_INIT));
    CHECK_EQ_U(s_health.current[VSS_FAULT_INIT].firstAtMs, 60u);
    Health_BootComplete();
    JumpToMs(100u);
    Evaluate();
    EXPECT_STATE(VSS_SERVICE_FAULT, VSS_SERVICE_UNAVAILABLE, BIT(VSS_FAULT_INIT));

    /* A report with a time keeps it. */
    Setup(true);
    JumpToMs(200u);
    {
        VssTimeEvidence at = At(150u);
        Health_Report(VSS_FAULT_INIT, &at);
    }
    Evaluate();
    CHECK_EQ_U(s_health.current[VSS_FAULT_INIT].firstAtMs, 150u);
    /* Out-of-range kinds are ignored. */
    Health_Report(VSS_FAULT_KINDS, NULL);
    Evaluate();
    CHECK_EQ_U(s_health.currentMask, BIT(VSS_FAULT_INIT));
    CHECK_EQ_U(s_health.occurrences, 1u);
}

/* A full ledger is a STATE fault and closes acceptance; once reclaim frees
 * slots both recover. */
static void test_full_ledger_is_a_state_fault(void)
{
    uint32_t i;
    VssTimeEvidence now;

    Setup(true);
    Evaluate();
    CHECK(s_health.accepting);
    for (i = 1u; i <= VSS_LEDGER_CAPACITY; ++i)
    {
        (void)AddEvent(i, VSS_SIGNAL_WELCOME);
    }
    CHECK_EQ_U(Store_Count(), VSS_LEDGER_CAPACITY);
    Evaluate();
    EXPECT_STATE(VSS_SERVICE_READY, VSS_SERVICE_DEGRADED, BIT(VSS_FAULT_STATE));
    CHECK(!s_health.accepting);

    JumpToMs(VSS_CFG_STORE_RECLAIM_MS + 100u);
    now = Now();
    CHECK_EQ_U(Store_Reclaim(&now), VSS_LEDGER_CAPACITY);
    Evaluate();
    EXPECT_STATE(VSS_SERVICE_READY, VSS_SERVICE_FULL, 0u);
    CHECK(s_health.accepting);
    CHECK(s_health.recent[VSS_FAULT_STATE].present);
}

/* Without reclaim the 33rd event finds no binding: a STATE fault that
 * repeats as the same instance; after a reclaim the event completes and
 * the fault clears. */
static void test_store_refusal_clears_after_a_clean_session(void)
{
    uint32_t i;
    uint32_t occurrence;
    VssTimeEvidence now;

    Setup(true);
    Evaluate();
    for (i = 1u; i <= VSS_PLAYBACK_BINDINGS_CAPACITY; ++i)
    {
        (void)AddEvent(i, VSS_SIGNAL_LOCK_COMPLETE);
        Pass();
        CHECK(RunUntilIdle(40u));
    }
    EXPECT_STATE(VSS_SERVICE_READY, VSS_SERVICE_FULL, 0u);
    (void)AddEvent(33u, VSS_SIGNAL_LOCK_COMPLETE);
    Pass();
    CHECK_EQ_U(s_view.storeRefusals, 1u);
    EXPECT_STATE(VSS_SERVICE_READY, VSS_SERVICE_DEGRADED, BIT(VSS_FAULT_STATE));
    occurrence = s_health.current[VSS_FAULT_STATE].occurrence;
    Pass();
    CHECK_EQ_U(s_view.storeRefusals, 2u);
    CHECK_EQ_U(s_health.current[VSS_FAULT_STATE].occurrence, occurrence);

    now = Now();
    CHECK(Store_Reclaim(&now) >= 28u);
    /* A later session that ends without playing does not clear it. */
    AudioAsset_OverrideForTest(AUDIO_ASSET_ID_LOCK, s_badAsset, 0u);
    Pass();
    CHECK_EQ_U(s_disposition, VSS_REQUEST_REJECTED);
    CHECK_EQ_U(s_view.lastEnd, PLAYBACK_END_NOT_STARTED);
    CHECK_EQ_U(s_view.storeRefusals, 2u);
    EXPECT_STATE(VSS_SERVICE_READY, VSS_SERVICE_DEGRADED,
                 BIT(VSS_FAULT_STATE) | BIT(VSS_FAULT_START));
    AudioAsset_OverrideForTest(0u, NULL, 0u);
    Pass();
    CHECK_EQ_U(s_disposition, VSS_REQUEST_ADOPTED);
    CHECK(RunUntilStarted(4u));
    EXPECT_STATE(VSS_SERVICE_PLAYING, VSS_SERVICE_DEGRADED, BIT(VSS_FAULT_STATE));
    CHECK(RunUntilIdle(40u));
    CHECK_EQ_U(s_view.lastEnd, PLAYBACK_END_COMPLETED);
    EXPECT_STATE(VSS_SERVICE_READY, VSS_SERVICE_FULL, 0u);
    CHECK_EQ_U(s_health.recent[VSS_FAULT_STATE].occurrence, occurrence);
}

/* ---- report numbers ----------------------------------------------------- */

static void test_report_numbers(void)
{
    VssStatusSnapshot read;
    VssTimeEvidence now;

    Setup(false);
    CHECK(!Build());                     /* nothing evaluated yet */
    CHECK(!Health_ReadStatus(&read));
    Evaluate();
    CHECK(Build());
    CHECK_EQ_U(s_status.reportSequence, 1u);
    CHECK_EQ_U(s_status.evaluatedAtMs, 0u);
    CHECK(!Build());                     /* same evaluation */
    CHECK(Health_ReadStatus(&read));
    CHECK_EQ_U(read.reportSequence, 1u);

    /* Same content within the period: no number. */
    JumpToMs(245u);
    Evaluate();
    CHECK(!Build());
    /* The period passed since the last report: a new number. */
    JumpToMs(250u);
    Evaluate();
    CHECK(Build());
    CHECK_EQ_U(s_status.reportSequence, 2u);
    CHECK_EQ_U(s_status.evaluatedAtMs, 250u);

    /* A content change is reported at once. */
    JumpToMs(260u);
    Health_BootComplete();
    Evaluate();
    CHECK(Build());
    CHECK_EQ_U(s_status.reportSequence, 3u);
    CHECK_EQ_U(s_status.state, VSS_SERVICE_READY);
    CHECK(s_status.accepting);

    /* Time alone without an evaluation never makes a number. */
    JumpToMs(900u);
    CHECK(!Build());
    CHECK(Health_ReadStatus(&read));
    CHECK_EQ_U(read.reportSequence, 3u);
    CHECK_EQ_U(read.evaluatedAtMs, 260u);

    /* A fault is a content change. */
    {
        VssTimeEvidence at = At(900u);
        Health_Report(VSS_FAULT_STATE, &at);
    }
    Evaluate();
    CHECK(Build());
    CHECK_EQ_U(s_status.reportSequence, 4u);
    CHECK_EQ_U(s_status.currentMask, BIT(VSS_FAULT_STATE));
    CHECK(s_status.lastFaultPresent);

    /* A time before the last evaluation is refused and changes nothing. */
    now = At(800u);
    Health_Evaluate(&now);
    CHECK(!Health_BuildStatus(&now, &s_status));
    View();
    CHECK_EQ_U(s_health.invalidTimes, 2u);
    CHECK(Health_ReadStatus(&read));
    CHECK_EQ_U(read.reportSequence, 4u);
    CHECK(!Health_BuildStatus(NULL, &s_status) && !Health_BuildStatus(&now, NULL));
}

/* Neither the evaluation counter nor the report number stops the reports:
 * the counter saturates as a diagnostic only, and the number after
 * UINT32_MAX is 1 (0 is not a report number). */
static void test_counters_never_stop_reports(void)
{
    VssTimeEvidence now;
    NetVssStatus net;

    Setup(true);
    Evaluate();
    CHECK(Build());
    CHECK_EQ_U(s_status.reportSequence, 1u);

    /* Evaluation counter at its limit. */
    Health_SeedCountersForTest(UINT32_MAX - 1u, 10u);
    JumpToMs(300u);
    Evaluate();
    CHECK(Build());
    CHECK_EQ_U(s_status.reportSequence, 11u);
    JumpToMs(600u);
    Evaluate();
    CHECK_EQ_U(s_health.evaluations, UINT32_MAX);
    CHECK(Build());
    CHECK_EQ_U(s_status.reportSequence, 12u);
    JumpToMs(900u);
    Evaluate();
    CHECK(Build());
    CHECK_EQ_U(s_status.reportSequence, 13u);

    /* Report number wraps past UINT32_MAX to 1. */
    Health_SeedCountersForTest(5u, UINT32_MAX - 1u);
    JumpToMs(1200u);
    Evaluate();
    CHECK(Build());
    CHECK_EQ_U(s_status.reportSequence, UINT32_MAX);
    JumpToMs(1500u);
    Evaluate();
    CHECK(Build());
    CHECK_EQ_U(s_status.reportSequence, 1u);
    now = Now();
    memset(&net, 0, sizeof net);
    CHECK(VssInputNet_ToNetStatus(&s_status, &now, &net));
    CHECK_EQ_U(net.update_sequence, 1u);
    JumpToMs(1800u);
    Evaluate();
    CHECK(Build());
    CHECK_EQ_U(s_status.reportSequence, 2u);
}

/* ---- last fault ---------------------------------------------------------- */

static void ExpectLast(VssFaultKind kind, uint32_t occurrence, uint64_t atMs, int line)
{
    if (!s_status.lastFaultPresent || (s_status.lastFaultKind != kind) ||
        (s_status.lastFaultOccurrence != occurrence) || (s_status.lastFaultAtMs != atMs))
    {
        printf("  (last fault check at line %d)\n", line);
    }
    CHECK(s_status.lastFaultPresent);
    CHECK_EQ_U(s_status.lastFaultKind, kind);
    CHECK_EQ_U(s_status.lastFaultOccurrence, occurrence);
    CHECK_EQ_U(s_status.lastFaultAtMs, atMs);
}
#define EXPECT_LAST(k, o, t) ExpectLast((k), (o), (t), __LINE__)

static void ReportAt(VssFaultKind kind, uint64_t ms)
{
    VssTimeEvidence at = At(ms);

    Health_Report(kind, &at);
}

/* The last fault is the instance that began latest among the current and
 * the recent ones; at the same time the larger occurrence. It survives
 * clearing, a repeat keeps its number, and a late report of an older fault
 * does not take its place. */
static void test_last_fault_selection(void)
{
    NetVssStatus net;
    VssTimeEvidence now;
    uint64_t base;

    Setup(true);
    Evaluate();
    CHECK(Build());
    CHECK(!s_status.lastFaultPresent);
    now = Now();
    CHECK(VssInputNet_ToNetStatus(&s_status, &now, &net));
    CHECK_EQ_U(net.vss_last_fault, NET_VSS_FAULT_CODE_NONE);
    CHECK_EQ_U(net.last_fault_occurrence, 0u);
    CHECK_EQ_U(net.last_fault_age_ms, 0u);
    CHECK_EQ_U(net.last_fault_quality, NET_QUALITY_NO_DATA);

    /* START at boundary time, cleared by a session that starts. */
    Boundary();
    Boundary();
    ReportAt(VSS_FAULT_START, VssTime_NowMs());
    Pass();
    CHECK(Build());
    EXPECT_LAST(VSS_FAULT_START, 1u, VssTime_NowMs());
    {
        uint64_t startAt = VssTime_NowMs();

        (void)AddEvent(1u, VSS_SIGNAL_WELCOME);
        Pass();
        CHECK(RunUntilStarted(4u));
        CHECK_EQ_U(s_health.currentMask, 0u);
        CHECK(Build());
        CHECK_EQ_U(s_status.currentMask, 0u);
        EXPECT_LAST(VSS_FAULT_START, 1u, startAt);
        now = Now();
        CHECK(VssInputNet_ToNetStatus(&s_status, &now, &net));
        CHECK_EQ_U(net.vss_fault_active, NET_FLAG_NO);
        CHECK_EQ_U(net.vss_last_fault, NET_VSS_FAULT_CODE_PLAYBACK_START_FAILURE);
        CHECK_EQ_U(net.last_fault_occurrence, 1u);
        CHECK_EQ_U(net.last_fault_age_ms, VssTime_NowMs() - startAt);
        CHECK_EQ_U(net.last_fault_quality, NET_QUALITY_OK);
    }
    CHECK(RunUntilIdle(80u));
    base = ((VssTime_NowMs() / 5u) + 1u) * 5u;

    /* A second START is a new instance; a repeat while current keeps it. */
    ReportAt(VSS_FAULT_START, base + 10u);
    JumpToMs(base + 100u);
    Evaluate();
    CHECK(Build());
    EXPECT_LAST(VSS_FAULT_START, 2u, base + 10u);
    ReportAt(VSS_FAULT_START, base + 90u);
    JumpToMs(base + 110u);
    Evaluate();
    CHECK_EQ_U(s_health.current[VSS_FAULT_START].occurrence, 2u);
    CHECK_EQ_U(s_health.current[VSS_FAULT_START].firstAtMs, base + 10u);

    /* A late report of an older fault: a larger number, an earlier start. */
    ReportAt(VSS_FAULT_STATE, base + 5u);
    JumpToMs(base + 120u);
    Evaluate();
    CHECK(Build());
    CHECK_EQ_U(s_status.currentMask, BIT(VSS_FAULT_START) | BIT(VSS_FAULT_STATE));
    CHECK_EQ_U(s_health.current[VSS_FAULT_STATE].occurrence, 3u);
    EXPECT_LAST(VSS_FAULT_START, 2u, base + 10u);

    /* Two faults beginning at the same time: the larger number. */
    ReportAt(VSS_FAULT_ASSET, base + 200u);
    ReportAt(VSS_FAULT_INIT, base + 200u);
    JumpToMs(base + 200u);
    Evaluate();
    CHECK(Build());
    CHECK_EQ_U(s_health.current[VSS_FAULT_ASSET].occurrence, 4u);
    EXPECT_LAST(VSS_FAULT_INIT, 5u, base + 200u);
    CHECK_EQ_U(s_status.state, VSS_SERVICE_FAULT);
}

/* ---- resubmission ------------------------------------------------------- */

/* A report read again for a resubmission keeps its number and evaluation
 * time; only its ages grow. The next evaluation takes the next number. */
static void test_resubmission_keeps_the_number(void)
{
    VssStatusSnapshot again;
    NetVssStatus first;
    NetVssStatus second;
    VssTimeEvidence now;

    Setup(true);
    JumpToMs(100u);
    ReportAt(VSS_FAULT_STATE, 100u);
    Evaluate();
    CHECK(Build());
    CHECK_EQ_U(s_status.reportSequence, 1u);
    now = At(110u);
    CHECK(VssInputNet_ToNetStatus(&s_status, &now, &first));

    JumpToMs(200u);
    CHECK(!Build());
    CHECK(Health_ReadStatus(&again));
    CHECK_EQ_U(again.reportSequence, 1u);
    CHECK_EQ_U(again.evaluatedAtMs, 100u);
    now = At(200u);
    CHECK(VssInputNet_ToNetStatus(&again, &now, &second));
    CHECK_EQ_U(second.update_sequence, first.update_sequence);
    CHECK_EQ_U(first.source_age_ms, 10u);
    CHECK_EQ_U(second.source_age_ms, 100u);
    CHECK_EQ_U(second.last_fault_age_ms, 100u);
    CHECK_EQ_U(second.last_fault_occurrence, first.last_fault_occurrence);

    JumpToMs(350u);
    Evaluate();
    CHECK(Build());
    CHECK_EQ_U(s_status.reportSequence, 2u);
}

int main(void)
{
    test_startup_until_boot_complete();
    test_accepting_rule();
    test_ready_playing_ready();
    test_uncertain_start_is_not_playing();
    test_availability_follows_the_supported_cues();
    test_missing_cue_degrades();
    test_decode_failure_is_an_asset_fault();
    test_start_refusal_clears_after_a_start();
    test_transmit_fault_is_an_output_fault();
    test_stall_while_idle_is_an_output_fault();
    test_refused_mute_is_an_output_fault();
    test_init_report_is_a_fault_until_reset();
    test_full_ledger_is_a_state_fault();
    test_store_refusal_clears_after_a_clean_session();
    test_report_numbers();
    test_counters_never_stop_reports();
    test_last_fault_selection();
    test_resubmission_keeps_the_number();
    TEST_FINISH();
}
