#ifndef P6_SESSION_MANAGER_H
#define P6_SESSION_MANAGER_H

#include <stdbool.h>
#include <stdint.h>

#include "p6_registration_store.h"

#ifdef __cplusplus
extern "C" {
#endif

#define P6_SESSION_MAX_ID_ATTEMPTS 16U
#define P6_RETIRED_SESSION_CAPACITY 16U
#define P6_PRODUCTION_BINDING_ENABLED 0

typedef enum
{
    P6_SESSION_STATE_UNREGISTERED = 0,
    P6_SESSION_STATE_REGISTRATION_PENDING,
    P6_SESSION_STATE_REGISTERED,
    P6_SESSION_STATE_SESSION_PENDING,
    P6_SESSION_STATE_AUTHORIZED,
    P6_SESSION_STATE_SESSION_EXPIRED,
    P6_SESSION_STATE_REVOKED,
    P6_SESSION_STATE_RESET_REQUESTED,
    P6_SESSION_STATE_RESET_REQUIRED,
    P6_SESSION_STATE_RESET_REJECTED
} p6_session_state_t;

typedef enum
{
    P6_RESET_STATE_NONE = 0,
    P6_RESET_STATE_REQUESTED,
    P6_RESET_STATE_ACCEPTED,
    P6_RESET_STATE_REQUIRED,
    P6_RESET_STATE_COMPLETE,
    P6_RESET_STATE_REJECTED
} p6_reset_state_t;

typedef enum
{
    P6_SESSION_OK = 0,
    P6_SESSION_INVALID_ARGUMENT,
    P6_SESSION_NOT_INITIALIZED,
    P6_SESSION_INVALID_STATE,
    P6_SESSION_NOT_REGISTERED,
    P6_SESSION_ALREADY_REGISTERED,
    P6_SESSION_PEER_MISMATCH,
    P6_SESSION_NOT_SECURE,
    P6_SESSION_GENERATION_MISMATCH,
    P6_SESSION_STALE_EVENT,
    P6_SESSION_STORE_ERROR,
    P6_SESSION_STORE_NOT_COMMITTED,
    P6_SESSION_STORE_COMMIT_UNKNOWN,
    P6_SESSION_STORE_CORRUPT,
    P6_SESSION_ENTROPY_ERROR,
    P6_SESSION_ID_COLLISION,
    P6_SESSION_EXPIRED,
    P6_SESSION_ROTATION_REQUIRED,
    P6_SESSION_CLOCK_ERROR,
    P6_SESSION_HISTORY_FULL,
    P6_SESSION_RESET_REQUIRED,
    P6_SESSION_NOT_AUTHORIZED,
    P6_SESSION_BOND_RECONCILIATION_REQUIRED,
    P6_SESSION_BOND_NOT_FOUND
} p6_session_status_t;

typedef bool (*p6_entropy_u32_fn)(void *context, uint32_t *out_value);
typedef bool (*p6_entropy_u64_fn)(void *context, uint64_t *out_value);

typedef struct
{
    p6_entropy_u32_fn next_u32;
    p6_entropy_u64_fn next_u64;
    void *context;
} p6_entropy_source_t;

typedef uint64_t (*p6_monotonic_now_ms_fn)(void *context);

typedef struct
{
    p6_monotonic_now_ms_fn now_ms;
    void *context;
} p6_monotonic_clock_t;

typedef struct
{
    uint64_t session_timeout_ms;
    uint8_t id_generation_attempts;
} p6_session_config_t;

typedef struct
{
    p6_session_state_t state;
    p6_reset_state_t reset_state;
    bool registered;
    bool connected;
    bool bonded;
    bool encrypted;
    bool authorization_verified;
    bool session_valid;
    bool app_instance_known;
    bool app_active;
    bool domain_boot_known;
    bool storage_fault_latched;
    bool registration_bond_reconciled;
    bool registration_bond_present;
    uint32_t device_context_id;
    uint32_t registration_epoch;
    uint64_t session_id;
    uint32_t connection_generation;
    uint32_t app_instance_id;
    uint32_t domain_boot_id;
    uint64_t session_issued_at_ms;
} p6_authorization_snapshot_t;

/*
 * Public fixed storage is intentional: P6-B1 performs no heap allocation.
 * Exactly one owner may mutate a manager. Producers must enqueue a copied
 * event to that owner; a failed/full external queue must not call the manager.
 */
typedef struct p6_session_manager
{
    bool initialized;
    p6_session_config_t config;
    p6_registration_store_t store;
    p6_entropy_source_t entropy;
    p6_monotonic_clock_t clock;
    p6_session_state_t state;
    p6_reset_state_t reset_state;
    bool registered;
    bool connected;
    bool bonded;
    bool encrypted;
    bool authorization_verified;
    bool session_valid;
    bool app_instance_known;
    bool app_active;
    bool domain_boot_known;
    bool storage_fault_latched;
    bool reset_marker_loaded;
    bool registration_bond_reconciled;
    bool registration_bond_present;
    p6_bond_identity_t registered_identity;
    p6_bond_identity_t current_identity;
    uint32_t device_context_id;
    uint32_t registration_epoch;
    uint32_t retired_registration_epoch;
    uint32_t retired_device_context_id;
    uint64_t session_id;
    uint32_t connection_generation;
    uint32_t app_instance_id;
    uint32_t domain_boot_id;
    uint64_t session_issued_at_ms;
    uint64_t last_clock_ms;
    uint32_t highest_connection_generation;
    uint64_t retired_session_ids[P6_RETIRED_SESSION_CAPACITY];
    uint8_t retired_session_count;
    bool retired_session_cache_full;
} p6_session_manager_t;

p6_session_status_t p6_session_manager_init(
    p6_session_manager_t *manager,
    const p6_session_config_t *config,
    const p6_registration_store_t *store,
    const p6_entropy_source_t *entropy,
    const p6_monotonic_clock_t *clock);

p6_session_status_t p6_session_manager_begin_registration(
    p6_session_manager_t *manager);
/*
 * Called by the single owner after NimBLE host sync.  lookup_succeeded=false
 * and an absent exact registered identity both fail closed without deleting
 * either the Registration record or a NimBLE bond.
 */
p6_session_status_t p6_session_manager_reconcile_registered_bond(
    p6_session_manager_t *manager,
    bool lookup_succeeded,
    bool exact_identity_present);
/*
 * The BLE lifecycle owner supplies a nonzero generation that is strictly
 * newer (serial-number order) than every generation seen in this boot.
 * Reboot-to-reboot continuity requires a P6-B2 boot epoch contract.
 */
p6_session_status_t p6_session_manager_connect(
    p6_session_manager_t *manager,
    const p6_bond_identity_t *identity,
    uint32_t connection_generation);
p6_session_status_t p6_session_manager_observe_security(
    p6_session_manager_t *manager,
    uint32_t connection_generation,
    bool bonded,
    bool encrypted);
p6_session_status_t p6_session_manager_register_confirmed_bond(
    p6_session_manager_t *manager,
    uint32_t connection_generation);
p6_session_status_t p6_session_manager_start_session(
    p6_session_manager_t *manager,
    uint32_t connection_generation);
p6_session_status_t p6_session_manager_disconnect(
    p6_session_manager_t *manager,
    uint32_t connection_generation);
p6_session_status_t p6_session_manager_observe_app_state(
    p6_session_manager_t *manager,
    uint32_t connection_generation,
    uint32_t app_instance_id,
    bool app_active);
p6_session_status_t p6_session_manager_observe_domain_boot(
    p6_session_manager_t *manager,
    uint32_t connection_generation,
    uint32_t domain_boot_id);
p6_session_status_t p6_session_manager_domain_link_down(
    p6_session_manager_t *manager,
    uint32_t connection_generation);
/* The single owner must call tick before publishing/using a snapshot. */
p6_session_status_t p6_session_manager_tick(p6_session_manager_t *manager);
/*
 * Legacy pure-store convenience used by P6-B1 tests. It does not delete a
 * NimBLE bond. Production reset owners must use the two-phase APIs below via
 * p6_bond_reset_coordinator so registration erase cannot precede bond delete.
 */
p6_session_status_t p6_session_manager_reset_registration(
    p6_session_manager_t *manager);
/*
 * Two-phase production reset contract.  request_reset durably commits the
 * RESET_PENDING marker and invalidates RAM authorization, but deliberately
 * does not erase the registration record.  The owner must delete the exact
 * NimBLE bond, then call complete_reset.  Neither API is bound to BLE input.
 */
p6_session_status_t p6_session_manager_request_reset(
    p6_session_manager_t *manager);
p6_session_status_t p6_session_manager_complete_reset(
    p6_session_manager_t *manager);

p6_session_status_t p6_session_manager_get_snapshot(
    const p6_session_manager_t *manager,
    p6_authorization_snapshot_t *out_snapshot);
/* The single owner must call tick before treating this result as current. */
bool p6_session_manager_context_is_current(
    const p6_session_manager_t *manager,
    uint32_t device_context_id,
    uint64_t session_id,
    uint32_t connection_generation);

#ifdef __cplusplus
}
#endif

#endif
