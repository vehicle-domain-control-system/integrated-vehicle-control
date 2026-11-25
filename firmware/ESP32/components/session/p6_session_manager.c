#include "p6_session_manager.h"

#include <string.h>

static bool bytes_are_all(const uint8_t *bytes, uint8_t value)
{
    size_t i;

    for (i = 0U; i < P6_BOND_IDENTITY_BYTES; ++i) {
        if (bytes[i] != value) {
            return false;
        }
    }
    return true;
}

bool p6_bond_identity_is_valid(const p6_bond_identity_t *identity)
{
    if ((identity == NULL) || (identity->address_type > 3U)) {
        return false;
    }
    return !bytes_are_all(identity->bytes, 0x00U) &&
           !bytes_are_all(identity->bytes, 0xFFU);
}

bool p6_bond_identity_equal(const p6_bond_identity_t *left,
                            const p6_bond_identity_t *right)
{
    return (left != NULL) && (right != NULL) &&
           (left->address_type == right->address_type) &&
           (memcmp(left->bytes, right->bytes, P6_BOND_IDENTITY_BYTES) == 0);
}

bool p6_registration_record_is_valid(const p6_registration_record_t *record)
{
    if ((record == NULL) ||
        (record->schema_version != P6_REGISTRATION_SCHEMA_VERSION)) {
        return false;
    }
    if (record->state == P6_REGISTRATION_RECORD_RESET_PENDING) {
        return true;
    }
    return (record->state == P6_REGISTRATION_RECORD_ACTIVE) &&
           (record->device_context_id != 0U) &&
           (record->registration_epoch != 0U) &&
           p6_bond_identity_is_valid(&record->bond_identity);
}

static bool dependencies_are_valid(const p6_session_config_t *config,
                                   const p6_registration_store_t *store,
                                   const p6_entropy_source_t *entropy,
                                   const p6_monotonic_clock_t *clock)
{
    return (config != NULL) && (store != NULL) && (entropy != NULL) &&
           (clock != NULL) && (config->session_timeout_ms > 0U) &&
           (config->id_generation_attempts > 0U) &&
           (config->id_generation_attempts <= P6_SESSION_MAX_ID_ATTEMPTS) &&
           (store->load != NULL) && (store->save != NULL) &&
           (store->erase != NULL) && (entropy->next_u32 != NULL) &&
           (entropy->next_u64 != NULL) && (clock->now_ms != NULL);
}

static bool retired_session_contains(const p6_session_manager_t *manager,
                                     uint64_t session_id)
{
    uint8_t i;

    for (i = 0U; i < manager->retired_session_count; ++i) {
        if (manager->retired_session_ids[i] == session_id) {
            return true;
        }
    }
    return false;
}

static void retire_active_session(p6_session_manager_t *manager)
{
    if ((manager->session_id == 0U) ||
        retired_session_contains(manager, manager->session_id)) {
        return;
    }
    if (manager->retired_session_count < P6_RETIRED_SESSION_CAPACITY) {
        manager->retired_session_ids[manager->retired_session_count++] =
            manager->session_id;
        if (manager->retired_session_count == P6_RETIRED_SESSION_CAPACITY) {
            manager->retired_session_cache_full = true;
        }
    } else {
        manager->retired_session_cache_full = true;
    }
}

static void invalidate_session(p6_session_manager_t *manager,
                               p6_session_state_t next_state)
{
    if (manager->session_id != 0U) {
        retire_active_session(manager);
    }
    manager->authorization_verified = false;
    manager->session_valid = false;
    manager->session_id = 0U;
    manager->session_issued_at_ms = 0U;
    manager->state = next_state;
}

static void clear_link(p6_session_manager_t *manager,
                       p6_session_state_t next_state)
{
    invalidate_session(manager, next_state);
    manager->connected = false;
    manager->bonded = false;
    manager->encrypted = false;
    manager->connection_generation = 0U;
    memset(&manager->current_identity, 0, sizeof(manager->current_identity));
    manager->app_instance_known = false;
    manager->app_instance_id = 0U;
    manager->app_active = false;
}

static p6_session_status_t read_monotonic_time(p6_session_manager_t *manager,
                                               uint64_t *out_now_ms)
{
    uint64_t now_ms;

    now_ms = manager->clock.now_ms(manager->clock.context);
    if (now_ms < manager->last_clock_ms) {
        invalidate_session(manager, P6_SESSION_STATE_SESSION_EXPIRED);
        return P6_SESSION_CLOCK_ERROR;
    }
    manager->last_clock_ms = now_ms;
    *out_now_ms = now_ms;
    return P6_SESSION_OK;
}

static p6_session_status_t generate_device_context_id(
    p6_session_manager_t *manager,
    uint32_t *out_device_context_id)
{
    uint8_t attempt;

    for (attempt = 0U; attempt < manager->config.id_generation_attempts; ++attempt) {
        uint32_t candidate = 0U;

        if (!manager->entropy.next_u32(manager->entropy.context, &candidate)) {
            return P6_SESSION_ENTROPY_ERROR;
        }
        if ((candidate != 0U) &&
            (candidate != manager->device_context_id) &&
            (candidate != manager->retired_device_context_id)) {
            *out_device_context_id = candidate;
            return P6_SESSION_OK;
        }
    }
    return P6_SESSION_ID_COLLISION;
}

static p6_session_status_t generate_registration_epoch(
    p6_session_manager_t *manager,
    uint32_t *out_registration_epoch)
{
    uint8_t attempt;

    for (attempt = 0U; attempt < manager->config.id_generation_attempts; ++attempt) {
        uint32_t candidate = 0U;

        if (!manager->entropy.next_u32(manager->entropy.context, &candidate)) {
            return P6_SESSION_ENTROPY_ERROR;
        }
        if ((candidate != 0U) &&
            (candidate != manager->registration_epoch) &&
            (candidate != manager->retired_registration_epoch)) {
            *out_registration_epoch = candidate;
            return P6_SESSION_OK;
        }
    }
    return P6_SESSION_ID_COLLISION;
}

static p6_session_status_t generate_session_id(p6_session_manager_t *manager,
                                               uint64_t *out_session_id)
{
    uint8_t attempt;

    if (manager->retired_session_cache_full) {
        return P6_SESSION_HISTORY_FULL;
    }
    for (attempt = 0U; attempt < manager->config.id_generation_attempts; ++attempt) {
        uint64_t candidate = 0U;

        if (!manager->entropy.next_u64(manager->entropy.context, &candidate)) {
            return P6_SESSION_ENTROPY_ERROR;
        }
        if ((candidate != 0U) && (candidate != manager->session_id) &&
            !retired_session_contains(manager, candidate)) {
            *out_session_id = candidate;
            return P6_SESSION_OK;
        }
    }
    return P6_SESSION_ID_COLLISION;
}

static bool generation_is_current(const p6_session_manager_t *manager,
                                  uint32_t connection_generation)
{
    return manager->connected && (connection_generation != 0U) &&
           (connection_generation == manager->connection_generation);
}

static bool generation_is_newer(uint32_t candidate, uint32_t previous)
{
    uint32_t delta;

    if (candidate == 0U) {
        return false;
    }
    if (previous == 0U) {
        return true;
    }
    delta = candidate - previous;
    return (delta != 0U) && (delta < UINT32_C(0x80000000));
}

static p6_session_status_t mutation_failure_status(
    p6_store_commit_result_t result)
{
    if (result == P6_STORE_NOT_COMMITTED) {
        return P6_SESSION_STORE_NOT_COMMITTED;
    }
    if (result == P6_STORE_COMMIT_UNKNOWN) {
        return P6_SESSION_STORE_COMMIT_UNKNOWN;
    }
    return P6_SESSION_OK;
}

static void clear_registration(p6_session_manager_t *manager)
{
    clear_link(manager, P6_SESSION_STATE_UNREGISTERED);
    manager->registered = false;
    manager->device_context_id = 0U;
    manager->registration_epoch = 0U;
    manager->registration_bond_reconciled = false;
    manager->registration_bond_present = false;
    manager->domain_boot_known = false;
    manager->domain_boot_id = 0U;
    manager->reset_marker_loaded = false;
    memset(&manager->registered_identity, 0, sizeof(manager->registered_identity));
}

p6_session_status_t p6_session_manager_init(
    p6_session_manager_t *manager,
    const p6_session_config_t *config,
    const p6_registration_store_t *store,
    const p6_entropy_source_t *entropy,
    const p6_monotonic_clock_t *clock)
{
    p6_registration_record_t record;
    p6_store_read_result_t store_result;

    if ((manager == NULL) || !dependencies_are_valid(config, store, entropy, clock)) {
        return P6_SESSION_INVALID_ARGUMENT;
    }

    memset(manager, 0, sizeof(*manager));
    manager->config = *config;
    manager->store = *store;
    manager->entropy = *entropy;
    manager->clock = *clock;
    manager->state = P6_SESSION_STATE_UNREGISTERED;
    manager->last_clock_ms = manager->clock.now_ms(manager->clock.context);
    manager->initialized = true;

    memset(&record, 0, sizeof(record));
    store_result = manager->store.load(manager->store.context, &record);
    if (store_result == P6_STORE_READ_EMPTY) {
        return P6_SESSION_OK;
    }
    if (store_result == P6_STORE_READ_CORRUPT) {
        manager->state = P6_SESSION_STATE_REVOKED;
        manager->storage_fault_latched = true;
        return P6_SESSION_STORE_CORRUPT;
    }
    if (store_result != P6_STORE_READ_OK) {
        manager->state = P6_SESSION_STATE_REVOKED;
        manager->storage_fault_latched = true;
        return P6_SESSION_STORE_ERROR;
    }
    if (!p6_registration_record_is_valid(&record)) {
        manager->state = P6_SESSION_STATE_REVOKED;
        manager->storage_fault_latched = true;
        return P6_SESSION_STORE_CORRUPT;
    }

    manager->registered_identity = record.bond_identity;
    manager->device_context_id = record.device_context_id;
    manager->registration_epoch = record.registration_epoch;
    manager->registration_bond_reconciled = false;
    manager->registration_bond_present = false;
    if (record.state == P6_REGISTRATION_RECORD_RESET_PENDING) {
        manager->retired_device_context_id = record.device_context_id;
        manager->state = P6_SESSION_STATE_RESET_REQUIRED;
        manager->reset_state = P6_RESET_STATE_REQUIRED;
        manager->storage_fault_latched = true;
        manager->reset_marker_loaded = true;
        return P6_SESSION_RESET_REQUIRED;
    }

    manager->registered = true;
    manager->state = P6_SESSION_STATE_REGISTERED;
    return P6_SESSION_OK;
}

p6_session_status_t p6_session_manager_begin_registration(
    p6_session_manager_t *manager)
{
    if ((manager == NULL) || !manager->initialized) {
        return P6_SESSION_NOT_INITIALIZED;
    }
    if (manager->registered) {
        return P6_SESSION_ALREADY_REGISTERED;
    }
    if ((manager->state != P6_SESSION_STATE_UNREGISTERED) ||
        manager->storage_fault_latched) {
        return P6_SESSION_INVALID_STATE;
    }
    manager->state = P6_SESSION_STATE_REGISTRATION_PENDING;
    manager->reset_state = P6_RESET_STATE_NONE;
    return P6_SESSION_OK;
}

p6_session_status_t p6_session_manager_reconcile_registered_bond(
    p6_session_manager_t *manager,
    bool lookup_succeeded,
    bool exact_identity_present)
{
    if ((manager == NULL) || !manager->initialized) {
        return P6_SESSION_NOT_INITIALIZED;
    }
    if (!manager->registered ||
        !p6_bond_identity_is_valid(&manager->registered_identity) ||
        (manager->device_context_id == 0U) ||
        (manager->registration_epoch == 0U) ||
        manager->reset_marker_loaded || manager->storage_fault_latched) {
        return P6_SESSION_INVALID_STATE;
    }

    if (manager->connected || manager->session_valid ||
        manager->authorization_verified) {
        clear_link(manager, P6_SESSION_STATE_REVOKED);
    }
    manager->registration_bond_reconciled = lookup_succeeded;
    manager->registration_bond_present =
        lookup_succeeded && exact_identity_present;
    if (!lookup_succeeded) {
        manager->state = P6_SESSION_STATE_REVOKED;
        return P6_SESSION_BOND_RECONCILIATION_REQUIRED;
    }
    if (!exact_identity_present) {
        manager->state = P6_SESSION_STATE_REVOKED;
        return P6_SESSION_BOND_NOT_FOUND;
    }
    manager->state = P6_SESSION_STATE_REGISTERED;
    return P6_SESSION_OK;
}

p6_session_status_t p6_session_manager_connect(
    p6_session_manager_t *manager,
    const p6_bond_identity_t *identity,
    uint32_t connection_generation)
{
    if ((manager == NULL) || !manager->initialized) {
        return P6_SESSION_NOT_INITIALIZED;
    }
    if (!p6_bond_identity_is_valid(identity) ||
        (connection_generation == 0U)) {
        return P6_SESSION_INVALID_ARGUMENT;
    }
    if (manager->storage_fault_latched) {
        return P6_SESSION_INVALID_STATE;
    }
    if (manager->connected) {
        return P6_SESSION_INVALID_STATE;
    }
    if (!generation_is_newer(connection_generation,
                             manager->highest_connection_generation)) {
        return P6_SESSION_STALE_EVENT;
    }
    if (manager->registered) {
        if (!manager->registration_bond_reconciled ||
            !manager->registration_bond_present) {
            return P6_SESSION_BOND_RECONCILIATION_REQUIRED;
        }
        if (!p6_bond_identity_equal(identity, &manager->registered_identity)) {
            return P6_SESSION_PEER_MISMATCH;
        }
    } else if (manager->state != P6_SESSION_STATE_REGISTRATION_PENDING) {
        return P6_SESSION_INVALID_STATE;
    }

    manager->connected = true;
    manager->bonded = false;
    manager->encrypted = false;
    manager->current_identity = *identity;
    manager->connection_generation = connection_generation;
    manager->highest_connection_generation = connection_generation;
    if (manager->registered) {
        manager->state = P6_SESSION_STATE_SESSION_PENDING;
    }
    return P6_SESSION_OK;
}

p6_session_status_t p6_session_manager_observe_security(
    p6_session_manager_t *manager,
    uint32_t connection_generation,
    bool bonded,
    bool encrypted)
{
    if ((manager == NULL) || !manager->initialized) {
        return P6_SESSION_NOT_INITIALIZED;
    }
    if (!generation_is_current(manager, connection_generation)) {
        return P6_SESSION_STALE_EVENT;
    }

    manager->bonded = bonded;
    manager->encrypted = encrypted;
    if (!bonded || !encrypted) {
        if (manager->session_valid || manager->authorization_verified) {
            invalidate_session(manager, P6_SESSION_STATE_SESSION_EXPIRED);
        }
        return P6_SESSION_NOT_SECURE;
    }
    return P6_SESSION_OK;
}

p6_session_status_t p6_session_manager_register_confirmed_bond(
    p6_session_manager_t *manager,
    uint32_t connection_generation)
{
    p6_registration_record_t record;
    p6_session_status_t status;
    p6_store_commit_result_t save_result;
    uint32_t device_context_id;
    uint32_t registration_epoch;

    if ((manager == NULL) || !manager->initialized) {
        return P6_SESSION_NOT_INITIALIZED;
    }
    if (manager->registered) {
        return P6_SESSION_ALREADY_REGISTERED;
    }
    if (manager->state != P6_SESSION_STATE_REGISTRATION_PENDING) {
        return P6_SESSION_INVALID_STATE;
    }
    if (!generation_is_current(manager, connection_generation)) {
        return P6_SESSION_STALE_EVENT;
    }
    if (!manager->bonded || !manager->encrypted) {
        return P6_SESSION_NOT_SECURE;
    }

    status = generate_device_context_id(manager, &device_context_id);
    if (status != P6_SESSION_OK) {
        return status;
    }
    status = generate_registration_epoch(manager, &registration_epoch);
    if (status != P6_SESSION_OK) {
        return status;
    }

    memset(&record, 0, sizeof(record));
    record.schema_version = P6_REGISTRATION_SCHEMA_VERSION;
    record.state = P6_REGISTRATION_RECORD_ACTIVE;
    record.bond_identity = manager->current_identity;
    record.device_context_id = device_context_id;
    record.registration_epoch = registration_epoch;
    save_result = manager->store.save(manager->store.context, &record);
    if (save_result != P6_STORE_COMMITTED) {
        manager->storage_fault_latched = true;
        return mutation_failure_status(save_result);
    }

    manager->registered = true;
    manager->registered_identity = manager->current_identity;
    manager->device_context_id = device_context_id;
    manager->registration_epoch = registration_epoch;
    manager->registration_bond_reconciled = true;
    manager->registration_bond_present = true;
    manager->storage_fault_latched = false;
    manager->state = P6_SESSION_STATE_REGISTERED;
    return P6_SESSION_OK;
}

p6_session_status_t p6_session_manager_start_session(
    p6_session_manager_t *manager,
    uint32_t connection_generation)
{
    p6_session_status_t status;
    uint64_t now_ms;
    uint64_t new_session_id;

    if ((manager == NULL) || !manager->initialized) {
        return P6_SESSION_NOT_INITIALIZED;
    }
    if (!manager->registered || manager->storage_fault_latched) {
        return P6_SESSION_NOT_REGISTERED;
    }
    if (!generation_is_current(manager, connection_generation)) {
        return P6_SESSION_STALE_EVENT;
    }
    if (!p6_bond_identity_equal(&manager->current_identity,
                                &manager->registered_identity)) {
        return P6_SESSION_PEER_MISMATCH;
    }
    if (!manager->bonded || !manager->encrypted) {
        return P6_SESSION_NOT_SECURE;
    }
    if (manager->state == P6_SESSION_STATE_AUTHORIZED) {
        return P6_SESSION_INVALID_STATE;
    }
    manager->state = P6_SESSION_STATE_SESSION_PENDING;

    status = read_monotonic_time(manager, &now_ms);
    if (status != P6_SESSION_OK) {
        return status;
    }
    status = generate_session_id(manager, &new_session_id);
    if (status != P6_SESSION_OK) {
        manager->authorization_verified = false;
        manager->session_valid = false;
        return status;
    }

    manager->session_id = new_session_id;
    manager->session_issued_at_ms = now_ms;
    manager->session_valid = true;
    manager->authorization_verified = true;
    manager->state = P6_SESSION_STATE_AUTHORIZED;
    return P6_SESSION_OK;
}

p6_session_status_t p6_session_manager_disconnect(
    p6_session_manager_t *manager,
    uint32_t connection_generation)
{
    if ((manager == NULL) || !manager->initialized) {
        return P6_SESSION_NOT_INITIALIZED;
    }
    if (!generation_is_current(manager, connection_generation)) {
        return P6_SESSION_STALE_EVENT;
    }
    clear_link(manager, P6_SESSION_STATE_SESSION_EXPIRED);
    return P6_SESSION_OK;
}

p6_session_status_t p6_session_manager_observe_app_state(
    p6_session_manager_t *manager,
    uint32_t connection_generation,
    uint32_t app_instance_id,
    bool app_active)
{
    if ((manager == NULL) || !manager->initialized) {
        return P6_SESSION_NOT_INITIALIZED;
    }
    if (app_instance_id == 0U) {
        return P6_SESSION_INVALID_ARGUMENT;
    }
    if (!generation_is_current(manager, connection_generation)) {
        return P6_SESSION_STALE_EVENT;
    }
    if (!manager->session_valid) {
        return P6_SESSION_EXPIRED;
    }
    if (!manager->app_instance_known) {
        manager->app_instance_known = true;
        manager->app_instance_id = app_instance_id;
        manager->app_active = app_active;
        return P6_SESSION_OK;
    }
    if (manager->app_instance_id != app_instance_id) {
        manager->app_instance_id = app_instance_id;
        manager->app_active = app_active;
        invalidate_session(manager, P6_SESSION_STATE_SESSION_EXPIRED);
        return P6_SESSION_ROTATION_REQUIRED;
    }
    manager->app_active = app_active;
    return P6_SESSION_OK;
}

p6_session_status_t p6_session_manager_observe_domain_boot(
    p6_session_manager_t *manager,
    uint32_t connection_generation,
    uint32_t domain_boot_id)
{
    if ((manager == NULL) || !manager->initialized) {
        return P6_SESSION_NOT_INITIALIZED;
    }
    if (domain_boot_id == 0U) {
        return P6_SESSION_INVALID_ARGUMENT;
    }
    if (!generation_is_current(manager, connection_generation)) {
        return P6_SESSION_STALE_EVENT;
    }
    if (!manager->session_valid) {
        return P6_SESSION_EXPIRED;
    }
    if (!manager->domain_boot_known) {
        manager->domain_boot_known = true;
        manager->domain_boot_id = domain_boot_id;
        return P6_SESSION_OK;
    }
    if (manager->domain_boot_id != domain_boot_id) {
        manager->domain_boot_id = domain_boot_id;
        invalidate_session(manager, P6_SESSION_STATE_SESSION_EXPIRED);
        return P6_SESSION_ROTATION_REQUIRED;
    }
    return P6_SESSION_OK;
}

p6_session_status_t p6_session_manager_domain_link_down(
    p6_session_manager_t *manager,
    uint32_t connection_generation)
{
    if ((manager == NULL) || !manager->initialized) {
        return P6_SESSION_NOT_INITIALIZED;
    }
    if (!generation_is_current(manager, connection_generation)) {
        return P6_SESSION_STALE_EVENT;
    }
    manager->domain_boot_known = false;
    manager->domain_boot_id = 0U;
    invalidate_session(manager, P6_SESSION_STATE_SESSION_EXPIRED);
    return P6_SESSION_OK;
}

p6_session_status_t p6_session_manager_tick(p6_session_manager_t *manager)
{
    p6_session_status_t status;
    uint64_t now_ms;

    if ((manager == NULL) || !manager->initialized) {
        return P6_SESSION_NOT_INITIALIZED;
    }
    status = read_monotonic_time(manager, &now_ms);
    if (status != P6_SESSION_OK) {
        return status;
    }
    if (!manager->session_valid) {
        return P6_SESSION_OK;
    }
    if ((now_ms - manager->session_issued_at_ms) >=
        manager->config.session_timeout_ms) {
        invalidate_session(manager, P6_SESSION_STATE_SESSION_EXPIRED);
        return P6_SESSION_EXPIRED;
    }
    return P6_SESSION_OK;
}

p6_session_status_t p6_session_manager_request_reset(
    p6_session_manager_t *manager)
{
    p6_registration_record_t marker;
    p6_store_commit_result_t mutation_result;
    p6_session_status_t failure_status;

    if ((manager == NULL) || !manager->initialized) {
        return P6_SESSION_NOT_INITIALIZED;
    }
    if (manager->reset_marker_loaded ||
        manager->reset_state == P6_RESET_STATE_REQUIRED) {
        return P6_SESSION_INVALID_STATE;
    }
    if (!manager->registered) {
        if (!manager->storage_fault_latched) {
            clear_link(manager, P6_SESSION_STATE_UNREGISTERED);
            manager->reset_state = P6_RESET_STATE_REJECTED;
        }
        return P6_SESSION_INVALID_STATE;
    }
    if (manager->storage_fault_latched ||
        !p6_bond_identity_is_valid(&manager->registered_identity) ||
        (manager->device_context_id == 0U) ||
        (manager->registration_epoch == 0U)) {
        clear_link(manager, P6_SESSION_STATE_REVOKED);
        manager->reset_state = P6_RESET_STATE_REJECTED;
        return P6_SESSION_INVALID_STATE;
    }

    if (manager->registered || (manager->device_context_id != 0U)) {
        manager->retired_device_context_id = manager->device_context_id;
        manager->retired_registration_epoch = manager->registration_epoch;
    }
    clear_link(manager, P6_SESSION_STATE_RESET_REQUESTED);
    manager->reset_state = P6_RESET_STATE_REQUESTED;
    manager->registered = false;

    if (!manager->reset_marker_loaded) {
        memset(&marker, 0, sizeof(marker));
        marker.schema_version = P6_REGISTRATION_SCHEMA_VERSION;
        marker.state = P6_REGISTRATION_RECORD_RESET_PENDING;
        if (p6_bond_identity_is_valid(&manager->registered_identity) &&
            (manager->device_context_id != 0U)) {
            marker.bond_identity = manager->registered_identity;
            marker.device_context_id = manager->device_context_id;
            marker.registration_epoch = manager->registration_epoch;
        }
        mutation_result = manager->store.save(manager->store.context, &marker);
        if (mutation_result != P6_STORE_COMMITTED) {
            manager->state = P6_SESSION_STATE_RESET_REJECTED;
            manager->reset_state = P6_RESET_STATE_REJECTED;
            manager->storage_fault_latched = true;
            failure_status = mutation_failure_status(mutation_result);
            return failure_status;
        }
        manager->reset_marker_loaded = true;
    }
    manager->reset_state = P6_RESET_STATE_ACCEPTED;

    manager->state = P6_SESSION_STATE_RESET_REQUIRED;
    manager->reset_state = P6_RESET_STATE_REQUIRED;
    manager->storage_fault_latched = true;
    return P6_SESSION_RESET_REQUIRED;
}

p6_session_status_t p6_session_manager_complete_reset(
    p6_session_manager_t *manager)
{
    p6_store_commit_result_t mutation_result;

    if ((manager == NULL) || !manager->initialized) {
        return P6_SESSION_NOT_INITIALIZED;
    }
    if (!manager->reset_marker_loaded ||
        manager->reset_state != P6_RESET_STATE_REQUIRED) {
        return P6_SESSION_INVALID_STATE;
    }

    mutation_result = manager->store.erase(manager->store.context);
    if (mutation_result != P6_STORE_COMMITTED) {
        manager->state = P6_SESSION_STATE_RESET_REQUIRED;
        manager->reset_state = P6_RESET_STATE_REQUIRED;
        manager->storage_fault_latched = true;
        return P6_SESSION_RESET_REQUIRED;
    }

    clear_registration(manager);
    manager->storage_fault_latched = false;
    manager->state = P6_SESSION_STATE_UNREGISTERED;
    manager->reset_state = P6_RESET_STATE_COMPLETE;
    return P6_SESSION_OK;
}

p6_session_status_t p6_session_manager_reset_registration(
    p6_session_manager_t *manager)
{
    p6_session_status_t status;

    if (manager != NULL && manager->initialized &&
        manager->reset_marker_loaded &&
        manager->reset_state == P6_RESET_STATE_REQUIRED) {
        return p6_session_manager_complete_reset(manager);
    }
    status = p6_session_manager_request_reset(manager);

    if (status != P6_SESSION_RESET_REQUIRED) {
        return status;
    }
    return p6_session_manager_complete_reset(manager);
}

p6_session_status_t p6_session_manager_get_snapshot(
    const p6_session_manager_t *manager,
    p6_authorization_snapshot_t *out_snapshot)
{
    if ((manager == NULL) || (out_snapshot == NULL)) {
        return P6_SESSION_INVALID_ARGUMENT;
    }
    if (!manager->initialized) {
        return P6_SESSION_NOT_INITIALIZED;
    }

    memset(out_snapshot, 0, sizeof(*out_snapshot));
    out_snapshot->state = manager->state;
    out_snapshot->reset_state = manager->reset_state;
    out_snapshot->registered = manager->registered;
    out_snapshot->connected = manager->connected;
    out_snapshot->bonded = manager->bonded;
    out_snapshot->encrypted = manager->encrypted;
    out_snapshot->authorization_verified = manager->authorization_verified;
    out_snapshot->session_valid = manager->session_valid;
    out_snapshot->app_instance_known = manager->app_instance_known;
    out_snapshot->app_active = manager->app_active;
    out_snapshot->domain_boot_known = manager->domain_boot_known;
    out_snapshot->storage_fault_latched = manager->storage_fault_latched;
    out_snapshot->registration_bond_reconciled =
        manager->registration_bond_reconciled;
    out_snapshot->registration_bond_present =
        manager->registration_bond_present;
    out_snapshot->device_context_id = manager->device_context_id;
    out_snapshot->registration_epoch = manager->registration_epoch;
    out_snapshot->session_id = manager->session_id;
    out_snapshot->connection_generation = manager->connection_generation;
    out_snapshot->app_instance_id = manager->app_instance_id;
    out_snapshot->domain_boot_id = manager->domain_boot_id;
    out_snapshot->session_issued_at_ms = manager->session_issued_at_ms;
    return P6_SESSION_OK;
}

bool p6_session_manager_context_is_current(
    const p6_session_manager_t *manager,
    uint32_t device_context_id,
    uint64_t session_id,
    uint32_t connection_generation)
{
    return (manager != NULL) && manager->initialized && manager->registered &&
           manager->registration_bond_reconciled &&
           manager->registration_bond_present &&
           manager->connected && manager->bonded && manager->encrypted &&
           manager->authorization_verified && manager->session_valid &&
           !manager->storage_fault_latched && (device_context_id != 0U) &&
           (session_id != 0U) &&
           (device_context_id == manager->device_context_id) &&
           (session_id == manager->session_id) &&
           (connection_generation == manager->connection_generation);
}
