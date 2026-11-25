#include "gateway_context_source_metadata.h"

#include <limits.h>
#include <string.h>

static bool snapshot_is_authorized(
    const p6_authorization_snapshot_t *snapshot)
{
    return snapshot != NULL &&
           snapshot->state == P6_SESSION_STATE_AUTHORIZED &&
           snapshot->reset_state == P6_RESET_STATE_NONE &&
           snapshot->registered && snapshot->registration_bond_reconciled &&
           snapshot->registration_bond_present && snapshot->connected &&
           snapshot->bonded && snapshot->encrypted &&
           snapshot->authorization_verified && snapshot->session_valid &&
           !snapshot->storage_fault_latched &&
           snapshot->device_context_id != 0U && snapshot->session_id != 0U &&
           snapshot->connection_generation != 0U;
}

static uint32_t next_sequence(uint32_t sequence)
{
    return sequence == UINT32_MAX ? 1U : sequence + 1U;
}

static void capture_identity(
    gateway_context_source_observation_t *observation,
    const p6_authorization_snapshot_t *snapshot,
    bool timestamp_known,
    uint64_t observed_at_ms,
    uint32_t sequence)
{
    memset(observation, 0, sizeof(*observation));
    observation->valid = true;
    observation->timestamp_known = timestamp_known;
    observation->observed_at_ms = observed_at_ms;
    observation->sequence = sequence;
    observation->device_context_id = snapshot->device_context_id;
    observation->session_id = snapshot->session_id;
    observation->connection_generation = snapshot->connection_generation;
}

static bool identity_matches(
    const gateway_context_source_observation_t *observation,
    const p6_authorization_snapshot_t *snapshot)
{
    return observation->valid && snapshot_is_authorized(snapshot) &&
           observation->device_context_id == snapshot->device_context_id &&
           observation->session_id == snapshot->session_id &&
           observation->connection_generation ==
               snapshot->connection_generation;
}

static void build_basis(
    const gateway_context_source_observation_t *observation,
    const p6_authorization_snapshot_t *snapshot,
    uint32_t ttl_ms,
    bool now_known,
    uint64_t now_ms,
    gateway_context_source_basis_t *out_basis)
{
    uint64_t elapsed_ms;

    memset(out_basis, 0, sizeof(*out_basis));
    if (ttl_ms == 0U || !now_known || !observation->timestamp_known ||
        !identity_matches(observation, snapshot) ||
        now_ms < observation->observed_at_ms) {
        return;
    }

    elapsed_ms = now_ms - observation->observed_at_ms;
    out_basis->available = true;
    out_basis->update_sequence = observation->sequence;
    out_basis->age_ms = elapsed_ms > UINT16_MAX
                            ? UINT16_MAX
                            : (uint16_t)elapsed_ms;
    out_basis->quality =
        elapsed_ms >= ttl_ms || elapsed_ms > UINT16_MAX
            ? UART_M_QUALITY_STALE
            : UART_M_QUALITY_OK;
}

void gateway_context_source_metadata_init(
    gateway_context_source_metadata_t *metadata,
    const gateway_context_source_policy_t *policy)
{
    if (metadata == NULL) {
        return;
    }
    memset(metadata, 0, sizeof(*metadata));
    if (policy != NULL) {
        metadata->policy = *policy;
        metadata->registration_connection_sequence =
            policy->registration_connection_sequence_seed;
        metadata->app_sequence = policy->app_sequence_seed;
    }
}

bool gateway_context_source_observe_registration_connection(
    gateway_context_source_metadata_t *metadata,
    const p6_authorization_snapshot_t *snapshot,
    bool timestamp_known,
    uint64_t observed_at_ms)
{
    if (metadata == NULL || !timestamp_known ||
        !snapshot_is_authorized(snapshot)) {
        return false;
    }
    metadata->registration_connection_sequence =
        next_sequence(metadata->registration_connection_sequence);
    capture_identity(&metadata->registration_connection, snapshot,
                     timestamp_known, observed_at_ms,
                     metadata->registration_connection_sequence);
    return true;
}

bool gateway_context_source_observe_app(
    gateway_context_source_metadata_t *metadata,
    const p6_authorization_snapshot_t *snapshot,
    bool timestamp_known,
    uint64_t observed_at_ms)
{
    if (metadata == NULL || !timestamp_known ||
        !snapshot_is_authorized(snapshot) ||
        !snapshot->app_instance_known || snapshot->app_instance_id == 0U) {
        return false;
    }
    metadata->app_sequence = next_sequence(metadata->app_sequence);
    capture_identity(&metadata->app, snapshot, timestamp_known, observed_at_ms,
                     metadata->app_sequence);
    metadata->app.app_instance_id = snapshot->app_instance_id;
    metadata->app.app_active = snapshot->app_active;
    return true;
}

void gateway_context_source_invalidate(
    gateway_context_source_metadata_t *metadata)
{
    if (metadata == NULL) {
        return;
    }
    memset(&metadata->registration_connection, 0,
           sizeof(metadata->registration_connection));
    memset(&metadata->app, 0, sizeof(metadata->app));
}

void gateway_context_source_reconcile(
    gateway_context_source_metadata_t *metadata,
    const p6_authorization_snapshot_t *snapshot)
{
    if (metadata == NULL) {
        return;
    }
    if (!identity_matches(&metadata->registration_connection, snapshot)) {
        memset(&metadata->registration_connection, 0,
               sizeof(metadata->registration_connection));
    }
    if (!identity_matches(&metadata->app, snapshot) ||
        !snapshot->app_instance_known ||
        metadata->app.app_instance_id != snapshot->app_instance_id ||
        metadata->app.app_active != snapshot->app_active) {
        memset(&metadata->app, 0, sizeof(metadata->app));
    }
}

void gateway_context_source_build(
    const gateway_context_source_metadata_t *metadata,
    const p6_authorization_snapshot_t *snapshot,
    bool now_known,
    uint64_t now_ms,
    gateway_context_sources_t *out_sources)
{
    if (out_sources == NULL) {
        return;
    }
    memset(out_sources, 0, sizeof(*out_sources));
    if (metadata == NULL || snapshot == NULL) {
        return;
    }

    build_basis(&metadata->registration_connection, snapshot,
                metadata->policy.registration_connection_ttl_ms,
                now_known, now_ms, &out_sources->registration_connection);
    build_basis(&metadata->app, snapshot, metadata->policy.app_ttl_ms,
                now_known, now_ms, &out_sources->app);
    /* No Proximity producer is bound. The DTO keeps UNKNOWN/INVALID/NO_DATA. */
}
