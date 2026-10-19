#ifndef CIS_OCCUPANT_MANAGER_H
#define CIS_OCCUPANT_MANAGER_H

#include <stdbool.h>
#include <stdint.h>

#include "Vehicle_Types_v0.2.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Domain-owned typed cache for one Network-admitted CIS occupant aggregate.
 *
 * The caller serializes every API call in one non-ISR owner context. Network
 * remains responsible for admitting a producer generation and for preventing
 * delayed callbacks from an older BOOT after a newer generation is admitted.
 * This manager compares sequence numbers only within an equal generation and
 * never orders generations by their numeric value.
 */

typedef enum
{
    CIS_OCCUPANT_MANAGER_OK = 0,
    CIS_OCCUPANT_MANAGER_INVALID_ARGUMENT,
    CIS_OCCUPANT_MANAGER_NOT_INITIALIZED,
    CIS_OCCUPANT_MANAGER_DUPLICATE,
    CIS_OCCUPANT_MANAGER_OUT_OF_ORDER,
    CIS_OCCUPANT_MANAGER_CONFLICT,
    CIS_OCCUPANT_MANAGER_NOT_ADMITTED
} CisOccupantManager_Status_t;

typedef struct
{
    uint32_t presenceTtlMs;
    uint32_t countTtlMs;
} CisOccupantManager_Policy_t;

typedef struct
{
    CisOccupantState_t state;
    CisOccupantManager_Policy_t policy;
    uint32_t presenceReceivedAtMs;
    uint32_t countReceivedAtMs;
    SourceGeneration_t admittedSourceGeneration;
    bool hasObservation;
    bool sourceGenerationChanged;
    bool hasAdmittedSourceGeneration;
    bool invalidated;
    bool initialized;
} CisOccupantManager_t;

typedef struct
{
    bool hasObservation;
    uint32_t receivedAtMs;
    uint32_t localElapsedMs;
    uint32_t effectiveAgeMs;
} CisOccupantManager_ValueTiming_t;

typedef struct
{
    CisOccupantState_t original;
    CisOccupantState_t effective;
    CisOccupantManager_ValueTiming_t presenceTiming;
    CisOccupantManager_ValueTiming_t countTiming;
    bool hasObservation;
    bool sourceGenerationChanged;
    bool invalidated;
} CisOccupantManager_Snapshot_t;

CisOccupantManager_Status_t CisOccupantManager_Init(
    CisOccupantManager_t *manager,
    const CisOccupantManager_Policy_t *policy);

/*
 * Synchronously invalidates the effective snapshot and admits exactly one
 * Network-validated producer generation. The last accepted observation is
 * retained only in snapshot.original for diagnostics. This is an
 * Application-private BOOT barrier primitive, not a public DomainIf_* API.
 */
CisOccupantManager_Status_t CisOccupantManager_Invalidate(
    CisOccupantManager_t *manager,
    SourceGeneration_t admittedSourceGeneration);

/*
 * Atomically stores one coherent Presence/Count observation. Both independent
 * ValueQuality records must identify the same producer updateSequence and the
 * producer generation must already have been admitted by Invalidate().
 */
CisOccupantManager_Status_t CisOccupantManager_Store(
    CisOccupantManager_t *manager,
    const CisOccupantState_t *state,
    uint32_t receivedAtMs);

/* Read-only copy. Failure leaves outSnapshot unchanged. */
CisOccupantManager_Status_t CisOccupantManager_GetSnapshot(
    const CisOccupantManager_t *manager,
    uint32_t nowMs,
    CisOccupantManager_Snapshot_t *outSnapshot);

#ifdef __cplusplus
}
#endif

#endif /* CIS_OCCUPANT_MANAGER_H */
