#include "vss_health.h"

#include <string.h>

#include "../asset/audio_asset.h"
#include "../asset/vss_cue_table.h"
#include "../playback/vss_playback.h"
#include "../../driver/audio_control/audio_control.h"
#include "../../driver/audio_tx/audio_tx.h"

#define KIND_BIT(kind) ((uint8_t)(1u << (unsigned)(kind)))
#define SERVICE_STOPPING_BITS (KIND_BIT(VSS_FAULT_INIT) | KIND_BIT(VSS_FAULT_OUTPUT))

typedef struct
{
    bool pending;
    bool timed;
    uint64_t atMs;
} HealthReport;

static VssHealthObservation s_obs;
static HealthReport s_reports[VSS_FAULT_KINDS];
static bool s_latched[VSS_FAULT_KINDS];   /* stays current until reset */

/* START and STATE refusals, with the session number at the refusal. */
static bool s_startActive;
static uint64_t s_startSession;
static bool s_stateActive;
static uint64_t s_stateSession;

/* Playback counters at the previous evaluation. */
static uint32_t s_prevStartRefusals;
static uint32_t s_prevStoreRefusals;
static uint32_t s_prevOutputFaults;
static uint32_t s_prevAssetFaults;
static uint64_t s_lastSeenSession;

static bool s_assetsChecked;
static bool s_assetMissing;

/* Cue rows (bit i = row i of the cue table) of the supported service set,
 * and those whose asset had data at the first evaluation. */
static uint32_t s_supportedRows;
static uint32_t s_presentRows;

static bool s_evaluated;             /* Health_Evaluate ran at least once */
static uint64_t s_evaluatedMs;

static bool s_reportPresent;
static bool s_evaluatedSinceReport;  /* an evaluation ran after the last report */
static VssStatusSnapshot s_report;

/* Scratch: too large for the stack. */
static VssPlaybackObservation s_pb;
static VssStatusSnapshot s_next;

static void count_up(uint32_t *counter)
{
    if (*counter != UINT32_MAX)
    {
        ++*counter;
    }
}

static bool valid_time(const VssTimeEvidence *t)
{
    return (t != NULL) && ((t->domain == VSS_PRODUCT) || (t->domain == VSS_TEST)) &&
           (t->epoch != 0u) && (t->continuity == VSS_CONTINUITY_COMPARABLE) &&
           (t->earliest <= t->latest);
}

static bool valid_kind(VssFaultKind kind)
{
    return (unsigned)kind < (unsigned)VSS_FAULT_KINDS;
}

/* The kind becomes current at atMs unless it already is: a repeat of a
 * current fault is the same instance. */
static void fault_begin(VssFaultKind kind, uint64_t atMs)
{
    VssFaultRecord *r = &s_obs.current[(size_t)kind];

    if (r->present)
    {
        return;
    }
    count_up(&s_obs.occurrences);
    memset(r, 0, sizeof *r);
    r->present = true;
    r->kind = kind;
    r->occurrence = s_obs.occurrences;
    r->firstAtMs = atMs;
}

static void fault_end(VssFaultKind kind, uint64_t atMs)
{
    VssFaultRecord *r = &s_obs.current[(size_t)kind];

    if (!r->present)
    {
        return;
    }
    s_obs.recent[(size_t)kind] = *r;
    s_obs.recent[(size_t)kind].clearedAtMs = atMs;
    memset(r, 0, sizeof *r);
}

/* Every cue of the table has data. */
static bool assets_present(void)
{
    AudioAsset_IdType id;

    for (id = 0u; id < AUDIO_ASSET_COUNT; ++id)
    {
        uint32_t size = 0u;

        if ((AudioAsset_GetSize(id, &size) != AUDIO_ASSET_STATUS_OK) || (size == 0u))
        {
            return false;
        }
    }
    return true;
}

/* The supported set is every cue row except the window anti-pinch warning,
 * which the product input path does not carry. */
static void check_cue_rows(void)
{
    size_t i;

    s_supportedRows = 0u;
    s_presentRows = 0u;
    for (i = 0u; i < AUDIO_ASSET_CUE_ROWS; ++i)
    {
        const AudioAsset_CueRow *row = AudioAsset_CueAt(i);
        uint32_t size = 0u;

        if ((row == NULL) || (row->signal == AUDIO_ASSET_CUE_SIGNAL_ANTI_PINCH))
        {
            continue;
        }
        s_supportedRows |= 1UL << i;
        if ((AudioAsset_GetSize(row->asset, &size) == AUDIO_ASSET_STATUS_OK) && (size != 0u))
        {
            s_presentRows |= 1UL << i;
        }
    }
}

/* Supported rows that can play: data present at boot, and the asset has not
 * failed to decode since. */
static uint32_t ready_rows(uint32_t failedAssets)
{
    uint32_t ready = s_presentRows & s_supportedRows;
    size_t i;

    for (i = 0u; i < AUDIO_ASSET_CUE_ROWS; ++i)
    {
        const AudioAsset_CueRow *row = AudioAsset_CueAt(i);

        if ((row != NULL) && (row->asset < 32u) && ((failedAssets & (1UL << row->asset)) != 0u))
        {
            ready &= ~(1UL << i);
        }
    }
    return ready;
}

static uint64_t latest_session(const VssPlaybackObservation *pb)
{
    uint64_t s = pb->present ? pb->session : 0u;

    return (pb->lastSession > s) ? pb->lastSession : s;
}

static void take_reports(uint64_t nowMs)
{
    size_t k;

    for (k = 0u; k < (size_t)VSS_FAULT_KINDS; ++k)
    {
        HealthReport *r = &s_reports[k];
        uint64_t at;

        if (!r->pending)
        {
            continue;
        }
        at = (r->timed && (r->atMs <= nowMs)) ? r->atMs : nowMs;
        r->pending = false;
        if (k == (size_t)VSS_FAULT_START)
        {
            s_startActive = true;
            s_startSession = s_lastSeenSession;
        }
        else if (k == (size_t)VSS_FAULT_STATE)
        {
            s_stateActive = true;
            s_stateSession = s_lastSeenSession;
        }
        else
        {
            s_latched[k] = true;
        }
        fault_begin((VssFaultKind)k, at);
    }
}

/* Playback refusals and stream faults since the previous evaluation, and
 * the clearing of refusals by later sessions. True when a session is
 * playing: a stateful session the stream reported STARTED for, or an event
 * session whose start STORE accepted, that has not ended. */
static bool poll_playback(uint64_t nowMs, bool *outputFailed)
{
    const VssPlaybackObservation *pb = &s_pb;
    uint64_t faultAt;
    uint64_t session;
    bool stateRefused = false;

    *outputFailed = false;
    if (!Playback_Read(&s_pb))
    {
        return false;
    }
    session = latest_session(pb);
    faultAt = (pb->lastFaultAtMs <= nowMs) ? pb->lastFaultAtMs : nowMs;
    if (pb->startRefusals != s_prevStartRefusals)
    {
        s_startActive = true;
        s_startSession = session;
        fault_begin(VSS_FAULT_START, nowMs);
    }
    if (pb->storeRefusals != s_prevStoreRefusals)
    {
        stateRefused = true;
        fault_begin(VSS_FAULT_STATE, nowMs);
    }
    if (pb->assetFaults != s_prevAssetFaults)
    {
        s_latched[VSS_FAULT_ASSET] = true;
        fault_begin(VSS_FAULT_ASSET, faultAt);
    }
    if (pb->outputFaults != s_prevOutputFaults)
    {
        if (pb->outputFailed)
        {
            fault_begin(VSS_FAULT_OUTPUT, faultAt);
        }
        else
        {
            /* A stream fault that is not an output failure: the facts were
             * not collected in time. */
            stateRefused = true;
            fault_begin(VSS_FAULT_STATE, faultAt);
        }
    }
    if (stateRefused)
    {
        s_stateActive = true;
        s_stateSession = session;
    }
    s_prevStartRefusals = pb->startRefusals;
    s_prevStoreRefusals = pb->storeRefusals;
    s_prevOutputFaults = pb->outputFaults;
    s_prevAssetFaults = pb->assetFaults;
    s_lastSeenSession = session;

    if (s_startActive && (pb->lastStartedSession > s_startSession))
    {
        s_startActive = false;
    }
    if (s_stateActive && !stateRefused && (pb->lastSession > s_stateSession) &&
        ((pb->lastEnd == PLAYBACK_END_COMPLETED) || (pb->lastEnd == PLAYBACK_END_INTERRUPTED)))
    {
        s_stateActive = false;
    }
    *outputFailed = pb->outputFailed;
    return pb->present && (pb->session != 0u) && pb->started;
}

void Health_Init(void)
{
    memset(&s_obs, 0, sizeof s_obs);
    memset(s_reports, 0, sizeof s_reports);
    memset(s_latched, 0, sizeof s_latched);
    s_startActive = false;
    s_startSession = 0u;
    s_stateActive = false;
    s_stateSession = 0u;
    s_prevStartRefusals = 0u;
    s_prevStoreRefusals = 0u;
    s_prevOutputFaults = 0u;
    s_prevAssetFaults = 0u;
    s_lastSeenSession = 0u;
    s_assetsChecked = false;
    s_assetMissing = false;
    s_supportedRows = 0u;
    s_presentRows = 0u;
    s_evaluated = false;
    s_evaluatedMs = 0u;
    s_reportPresent = false;
    s_evaluatedSinceReport = false;
    memset(&s_report, 0, sizeof s_report);
    s_obs.state = (uint8_t)VSS_SERVICE_STARTUP;
    s_obs.availability = (uint8_t)VSS_SERVICE_UNAVAILABLE;
}

void Health_BootComplete(void)
{
    s_obs.bootComplete = true;
}

void Health_Report(VssFaultKind kind, const VssTimeEvidence *at)
{
    HealthReport *r;

    if (!valid_kind(kind))
    {
        return;
    }
    r = &s_reports[(size_t)kind];
    if (r->pending && r->timed)
    {
        /* Keep the earliest time of the reports still waiting. */
        if (valid_time(at) && (at->earliest < r->atMs))
        {
            r->atMs = at->earliest;
        }
        return;
    }
    r->pending = true;
    r->timed = valid_time(at);
    r->atMs = r->timed ? at->earliest : 0u;
}

void Health_Evaluate(const VssTimeEvidence *now)
{
    AudioControlStatus control;
    uint64_t nowMs;
    uint32_t txMask;
    bool playing;
    bool playbackOutput;
    bool ledgerFull;
    uint32_t readyRows;
    bool condition[VSS_FAULT_KINDS];
    uint8_t mask = 0u;
    size_t k;

    if (!valid_time(now) || (s_evaluated && (now->earliest < s_evaluatedMs)))
    {
        count_up(&s_obs.invalidTimes);
        return;
    }
    nowMs = now->earliest;
    if (!s_assetsChecked)
    {
        s_assetsChecked = true;
        s_assetMissing = !assets_present();
        check_cue_rows();
    }
    take_reports(nowMs);
    playing = poll_playback(nowMs, &playbackOutput);
    readyRows = ready_rows(s_pb.failedAssets);

    txMask = AudioTx_FaultMask();
    memset(&control, 0, sizeof control);
    AudioControl_Read(&control);
    ledgerFull = Store_Count() >= VSS_LEDGER_CAPACITY;

    condition[VSS_FAULT_ASSET] = s_latched[VSS_FAULT_ASSET] || s_assetMissing;
    condition[VSS_FAULT_START] = s_startActive;
    condition[VSS_FAULT_OUTPUT] = s_latched[VSS_FAULT_OUTPUT] || (txMask != 0u) || playbackOutput ||
                                  (control.muteLatched && !control.muteAccepted);
    condition[VSS_FAULT_STATE] = s_stateActive || ledgerFull;
    condition[VSS_FAULT_INIT] = s_latched[VSS_FAULT_INIT];
    for (k = 0u; k < (size_t)VSS_FAULT_KINDS; ++k)
    {
        if (condition[k])
        {
            fault_begin((VssFaultKind)k, nowMs);
            mask |= KIND_BIT(k);
        }
        else
        {
            fault_end((VssFaultKind)k, nowMs);
        }
    }

    s_obs.currentMask = mask;
    s_obs.supportedCueRows = s_supportedRows;
    s_obs.readyCueRows = readyRows;
    /* No supported cue can play: a confirmed fault leaves no new start. */
    if (((mask & SERVICE_STOPPING_BITS) != 0u) || (readyRows == 0u))
    {
        s_obs.state = (uint8_t)VSS_SERVICE_FAULT;
        s_obs.availability = (uint8_t)VSS_SERVICE_UNAVAILABLE;
    }
    else if (!s_obs.bootComplete)
    {
        s_obs.state = (uint8_t)VSS_SERVICE_STARTUP;
        s_obs.availability = (uint8_t)VSS_SERVICE_UNAVAILABLE;
    }
    else
    {
        /* FULL needs every supported cue ready, not only an empty mask. */
        s_obs.state = (uint8_t)(playing ? VSS_SERVICE_PLAYING : VSS_SERVICE_READY);
        s_obs.availability = (uint8_t)(((mask == 0u) && (readyRows == s_supportedRows))
                                       ? VSS_SERVICE_FULL : VSS_SERVICE_DEGRADED);
    }
    /* Not tied to the INPUT admission: the DOMAIN may wait for this flag
     * before it sends, and INPUT opens only with the first DOMAIN message. */
    s_obs.accepting = s_obs.bootComplete && (s_obs.state != (uint8_t)VSS_SERVICE_FAULT) && !ledgerFull;
    s_obs.audioTxFaultMask = txMask;
    s_obs.abortState = (uint8_t)AudioTx_AbortState();
    s_obs.muteRequested = control.muteLatched;
    s_obs.muteAccepted = control.muteAccepted;
    count_up(&s_obs.evaluations);
    s_evaluated = true;
    s_evaluatedSinceReport = true;
    s_evaluatedMs = nowMs;
}

/* The latest-starting instance among the current and recent records; at
 * the same start time the larger occurrence number. */
static const VssFaultRecord *last_fault(void)
{
    const VssFaultRecord *best = NULL;
    size_t k;

    for (k = 0u; k < 2u * (size_t)VSS_FAULT_KINDS; ++k)
    {
        const VssFaultRecord *r = (k < (size_t)VSS_FAULT_KINDS)
            ? &s_obs.current[k] : &s_obs.recent[k - (size_t)VSS_FAULT_KINDS];

        if (!r->present)
        {
            continue;
        }
        if ((best == NULL) || (r->firstAtMs > best->firstAtMs) ||
            ((r->firstAtMs == best->firstAtMs) && (r->occurrence > best->occurrence)))
        {
            best = r;
        }
    }
    return best;
}

static bool same_content(const VssStatusSnapshot *a, const VssStatusSnapshot *b)
{
    return (a->state == b->state) && (a->availability == b->availability) &&
           (a->accepting == b->accepting) && (a->currentMask == b->currentMask) &&
           (a->lastFaultPresent == b->lastFaultPresent) &&
           (!a->lastFaultPresent ||
            ((a->lastFaultKind == b->lastFaultKind) && (a->lastFaultOccurrence == b->lastFaultOccurrence)));
}

bool Health_BuildStatus(const VssTimeEvidence *now, VssStatusSnapshot *out)
{
    const VssFaultRecord *last;

    if (out == NULL)
    {
        return false;
    }
    if (!valid_time(now) || (now->earliest < s_evaluatedMs))
    {
        count_up(&s_obs.invalidTimes);
        return false;
    }
    if (!s_evaluatedSinceReport)
    {
        return false;   /* no evaluation since the last report */
    }
    memset(&s_next, 0, sizeof s_next);
    s_next.state = s_obs.state;
    s_next.availability = s_obs.availability;
    s_next.accepting = s_obs.accepting;
    s_next.currentMask = s_obs.currentMask;
    last = last_fault();
    if (last != NULL)
    {
        s_next.lastFaultPresent = true;
        s_next.lastFaultKind = last->kind;
        s_next.lastFaultOccurrence = last->occurrence;
        s_next.lastFaultAtMs = last->firstAtMs;
    }
    s_next.evaluatedAtMs = s_evaluatedMs;
    if (s_reportPresent && same_content(&s_next, &s_report) &&
        ((s_evaluatedMs - s_report.evaluatedAtMs) < (uint64_t)VSS_CFG_STATUS_PERIOD_MS))
    {
        return false;
    }
    /* 0 is not a report number: after UINT32_MAX the numbers go on at 1. */
    if (!s_reportPresent || (s_report.reportSequence == UINT32_MAX))
    {
        s_next.reportSequence = 1u;
    }
    else
    {
        s_next.reportSequence = s_report.reportSequence + 1u;
    }
    s_report = s_next;
    s_reportPresent = true;
    s_evaluatedSinceReport = false;
    *out = s_report;
    return true;
}

bool Health_ReadStatus(VssStatusSnapshot *out)
{
    if ((out == NULL) || !s_reportPresent)
    {
        return false;
    }
    *out = s_report;
    return true;
}

uint8_t Health_CurrentMask(void)
{
    return s_obs.currentMask;
}

bool Health_Read(VssHealthObservation *out)
{
    if (out == NULL)
    {
        return false;
    }
    *out = s_obs;
    return true;
}

#if VSS_CFG_HOST_TEST
void Health_SeedCountersForTest(uint32_t evaluations, uint32_t lastReportSequence)
{
    s_obs.evaluations = evaluations;
    if (s_reportPresent)
    {
        s_report.reportSequence = lastReportSequence;
    }
}
#endif
