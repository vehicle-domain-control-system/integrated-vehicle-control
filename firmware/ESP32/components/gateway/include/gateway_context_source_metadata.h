#ifndef GATEWAY_CONTEXT_SOURCE_METADATA_H
#define GATEWAY_CONTEXT_SOURCE_METADATA_H

#include <stdbool.h>
#include <stdint.h>

#include "gateway_context_dto.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Local observation policy only. A zero TTL keeps that source unavailable.
 * The production TTL/reset contract is open, so the default Firmware never
 * turns these observations into fresh wire metadata without explicit policy.
 */
typedef struct
{
    uint32_t registration_connection_ttl_ms;
    uint32_t app_ttl_ms;
    uint32_t registration_connection_sequence_seed;
    uint32_t app_sequence_seed;
} gateway_context_source_policy_t;

typedef struct
{
    bool valid;
    bool timestamp_known;
    uint64_t observed_at_ms;
    uint32_t sequence;
    uint32_t device_context_id;
    uint64_t session_id;
    uint32_t connection_generation;
    uint32_t app_instance_id;
    bool app_active;
} gateway_context_source_observation_t;

typedef struct
{
    gateway_context_source_policy_t policy;
    gateway_context_source_observation_t registration_connection;
    gateway_context_source_observation_t app;
    uint32_t registration_connection_sequence;
    uint32_t app_sequence;
} gateway_context_source_metadata_t;

void gateway_context_source_metadata_init(
    gateway_context_source_metadata_t *metadata,
    const gateway_context_source_policy_t *policy);

bool gateway_context_source_observe_registration_connection(
    gateway_context_source_metadata_t *metadata,
    const p6_authorization_snapshot_t *snapshot,
    bool timestamp_known,
    uint64_t observed_at_ms);

bool gateway_context_source_observe_app(
    gateway_context_source_metadata_t *metadata,
    const p6_authorization_snapshot_t *snapshot,
    bool timestamp_known,
    uint64_t observed_at_ms);

/* Invalidates observations, but preserves monotonically advancing counters. */
void gateway_context_source_invalidate(
    gateway_context_source_metadata_t *metadata);

/* Drops observations that no longer describe the current P6 snapshot. */
void gateway_context_source_reconcile(
    gateway_context_source_metadata_t *metadata,
    const p6_authorization_snapshot_t *snapshot);

/*
 * Builds DTO bases without changing producer state. At or beyond the explicit
 * TTL the basis is STALE. Unknown/rollback time and disabled policy are
 * represented as unavailable, which the DTO maps to NO_DATA.
 */
void gateway_context_source_build(
    const gateway_context_source_metadata_t *metadata,
    const p6_authorization_snapshot_t *snapshot,
    bool now_known,
    uint64_t now_ms,
    gateway_context_sources_t *out_sources);

#ifdef __cplusplus
}
#endif

#endif
