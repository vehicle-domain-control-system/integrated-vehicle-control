#ifndef VSS_HEALTH_H
#define VSS_HEALTH_H

#include <stdbool.h>
#include <stdint.h>

#include "../input/vss_input_store.h"

/* Fault kinds. The value is the bit index in VssStatusSnapshot.currentMask
 * and in the reported fault mask; the reported fault code is value + 1. */
typedef enum
{
    VSS_FAULT_ASSET = 0,
    VSS_FAULT_START,
    VSS_FAULT_OUTPUT,
    VSS_FAULT_STATE,
    VSS_FAULT_INIT,
    VSS_FAULT_KINDS
} VssFaultKind;

/* Service state and availability codes, in the order of the reported
 * status codes. */
typedef enum
{
    VSS_SERVICE_STARTUP = 0,
    VSS_SERVICE_READY,
    VSS_SERVICE_PLAYING,
    VSS_SERVICE_FAULT
} VssServiceState;

typedef enum
{
    VSS_SERVICE_FULL = 0,
    VSS_SERVICE_DEGRADED,
    VSS_SERVICE_UNAVAILABLE
} VssServiceAvailability;

/* One complete status evaluation. Times are on the VssTime millisecond
 * clock. */
typedef struct
{
    uint8_t state;            /* VssServiceState */
    uint8_t availability;     /* VssServiceAvailability */
    bool accepting;
    uint8_t currentMask;      /* bit n = VssFaultKind n currently present */
    bool lastFaultPresent;
    VssFaultKind lastFaultKind;
    uint32_t lastFaultOccurrence;
    uint64_t lastFaultAtMs;
    uint32_t reportSequence;  /* +1 per new evaluation, from 1 */
    uint64_t evaluatedAtMs;
} VssStatusSnapshot;

/* Service health: the current fault set, the recent (cleared) faults and
 * the service state reported to the Network.
 *
 * A fault instance begins when its kind becomes current. It gets the next
 * occurrence number of this run (1, 2, ...) and keeps the time it began; it
 * stays the same instance, with the same number, for as long as the kind
 * stays current. When the kind clears, the instance becomes the recent one
 * of its kind. Sources, read on every Health_Evaluate:
 *
 *   ASSET   a cue missing from the asset table at the first evaluation, a
 *           decode failure seen by playback; stays current until reset (the
 *           cues are fixed data)
 *   START   the stream refused to prepare or start a session; clears once a
 *           later session has started
 *   OUTPUT  the transmit driver's latched fault mask (late refill, overlap,
 *           interface error, stalled boundaries), a playback output
 *           failure, a codec mute that was requested but not accepted;
 *           stays current while any of them holds (all are latched)
 *   STATE   STORE refused a binding, fact or retirement, the stream's fact
 *           queue overflowed, or the ledger is full; a refusal clears once a
 *           later one-shot session has ended completed or interrupted with no
 *           new refusal, a full ledger clears when a slot is free again
 *   INIT    reported by the boot sequence; stays current until reset
 *
 * Playback is polled through its observation counters; a change since the
 * previous evaluation is a new refusal or fault. Health_Report adds a fault
 * that only the caller can see (boot results): INIT, ASSET and OUTPUT
 * reports stay current until reset, START and STATE reports clear like the
 * polled ones.
 *
 * State: INIT or OUTPUT current -> FAULT / UNAVAILABLE; boot not complete
 * -> STARTUP / UNAVAILABLE; a session the stream reported STARTED for and
 * that has not ended -> PLAYING; otherwise READY. PLAYING and READY are
 * DEGRADED while any fault is current, FULL otherwise. accepting = boot
 * complete, state not FAULT and a free ledger slot; it does not depend on
 * whether a DOMAIN message has opened INPUT yet.
 *
 * All calls run in the super-loop owner context. Times come from the
 * caller (the VssTime clock); HEALTH reads no clock itself. */

/* Clears every fault, report and counter; boot not complete. Called once at
 * boot before the first Health_Report. */
void Health_Init(void);

/* The boot sequence finished; the service may leave STARTUP. */
void Health_BootComplete(void);

/* Notification of a fault the caller detected at time at. The fault becomes
 * current at the next Health_Evaluate, with time at (now when at is not
 * usable or lies after that evaluation). */
void Health_Report(VssFaultKind kind, const VssTimeEvidence *at);

/* Collects the observations and updates the current fault set and the
 * service state at now. now must not move backwards. */
void Health_Evaluate(const VssTimeEvidence *now);

/* A new evaluation is made from the latest Health_Evaluate when there is
 * none yet, when state, availability, accepting, the current mask or the
 * last fault differ from the previous one, or when that evaluation lies
 * VSS_CFG_STATUS_PERIOD_MS or more after the previous one. Then true and
 * *out with the next report number. False otherwise (out untouched): reading,
 * waiting or resubmitting never creates a number. */
bool Health_BuildStatus(const VssTimeEvidence *now, VssStatusSnapshot *out);

/* The last evaluation made by Health_BuildStatus; false before the first. */
bool Health_ReadStatus(VssStatusSnapshot *out);

/* Current fault mask of the latest Health_Evaluate. */
uint8_t Health_CurrentMask(void);

typedef struct
{
    bool present;
    VssFaultKind kind;
    uint32_t occurrence;          /* 1.. within this run */
    uint64_t firstAtMs;           /* when the instance began */
    uint64_t clearedAtMs;         /* recent records only */
} VssFaultRecord;

/* Diagnostic view. Counters saturate. */
typedef struct
{
    bool bootComplete;
    uint32_t evaluations;
    uint32_t invalidTimes;        /* Evaluate/Build calls refused for their time */
    uint8_t currentMask;
    uint8_t state;
    uint8_t availability;
    bool accepting;
    VssFaultRecord current[VSS_FAULT_KINDS];
    VssFaultRecord recent[VSS_FAULT_KINDS];
    uint32_t occurrences;         /* instances begun in this run */
    /* Output path, as read at the latest evaluation. abortState is the
     * transmit driver's AudioTxAbortState: the result of its one transfer
     * stop after a fault, shown for diagnosis only. */
    uint32_t audioTxFaultMask;
    uint8_t abortState;
    bool muteRequested;
    bool muteAccepted;
} VssHealthObservation;

bool Health_Read(VssHealthObservation *out);

#endif /* VSS_HEALTH_H */
