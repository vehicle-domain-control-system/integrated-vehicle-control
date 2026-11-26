#include "p6_ble_lifecycle.h"

#include <string.h>

static bool generation_is_newer(uint32_t candidate, uint32_t previous)
{
    return candidate != 0U &&
           (previous == 0U || (int32_t)(candidate - previous) > 0);
}

static bool current_generation(const p6_ble_lifecycle_t *lifecycle,
                               uint32_t connection_generation)
{
    return lifecycle->connected && connection_generation != 0U &&
           connection_generation == lifecycle->connection_generation;
}

static p6_ble_lifecycle_status_t validate_peer(
    const p6_ble_peer_observation_t *peer)
{
    if (peer == NULL) {
        return P6_BLE_LIFECYCLE_INVALID_ARGUMENT;
    }
    if (peer->lookup_error) {
        return P6_BLE_LIFECYCLE_IDENTITY_LOOKUP_ERROR;
    }
    if (!peer->identity_resolved ||
        !p6_bond_identity_is_valid(&peer->identity)) {
        return P6_BLE_LIFECYCLE_IDENTITY_UNRESOLVED;
    }
    if (!peer->bond_store_present) {
        return P6_BLE_LIFECYCLE_BOND_NOT_FOUND;
    }
    return P6_BLE_LIFECYCLE_OK;
}

static p6_ble_lifecycle_status_t map_connect_status(
    p6_session_status_t status)
{
    if (status == P6_SESSION_OK) {
        return P6_BLE_LIFECYCLE_OK;
    }
    if (status == P6_SESSION_PEER_MISMATCH) {
        return P6_BLE_LIFECYCLE_PEER_MISMATCH;
    }
    if (status == P6_SESSION_STALE_EVENT ||
        status == P6_SESSION_GENERATION_MISMATCH) {
        return P6_BLE_LIFECYCLE_STALE_EVENT;
    }
    if (status == P6_SESSION_INVALID_STATE ||
        status == P6_SESSION_NOT_REGISTERED ||
        status == P6_SESSION_BOND_RECONCILIATION_REQUIRED) {
        return P6_BLE_LIFECYCLE_REGISTRATION_UNAVAILABLE;
    }
    if (status == P6_SESSION_BOND_NOT_FOUND) {
        return P6_BLE_LIFECYCLE_BOND_NOT_FOUND;
    }
    return P6_BLE_LIFECYCLE_MANAGER_REJECTED;
}

static p6_ble_lifecycle_status_t attach_manager(
    p6_ble_lifecycle_t *lifecycle,
    const p6_ble_peer_observation_t *peer)
{
    p6_ble_lifecycle_status_t validation = validate_peer(peer);
    p6_session_status_t manager_status;

    if (validation != P6_BLE_LIFECYCLE_OK) {
        lifecycle->identity_rejections++;
        return validation;
    }
    manager_status = p6_session_manager_connect(
        lifecycle->manager, &peer->identity,
        lifecycle->connection_generation);
    validation = map_connect_status(manager_status);
    if (validation == P6_BLE_LIFECYCLE_OK) {
        lifecycle->manager_connected = true;
        lifecycle->bound_identity = peer->identity;
    }
    return validation;
}

static void block_current_generation(p6_ble_lifecycle_t *lifecycle)
{
    if (lifecycle->manager_connected) {
        (void)p6_session_manager_disconnect(
            lifecycle->manager, lifecycle->connection_generation);
    }
    lifecycle->manager_connected = false;
    lifecycle->generation_blocked = true;
    lifecycle->mitm_authenticated = false;
    memset(&lifecycle->bound_identity, 0,
           sizeof(lifecycle->bound_identity));
}

p6_ble_lifecycle_status_t p6_ble_lifecycle_init(
    p6_ble_lifecycle_t *lifecycle,
    p6_session_manager_t *manager)
{
    if (lifecycle == NULL || manager == NULL || !manager->initialized) {
        return P6_BLE_LIFECYCLE_INVALID_ARGUMENT;
    }
    memset(lifecycle, 0, sizeof(*lifecycle));
    lifecycle->manager = manager;
    lifecycle->initialized = true;
    return P6_BLE_LIFECYCLE_OK;
}

p6_ble_lifecycle_status_t p6_ble_lifecycle_on_connect(
    p6_ble_lifecycle_t *lifecycle,
    uint32_t connection_generation,
    uint32_t connection_epoch,
    const p6_ble_peer_observation_t *peer)
{
    p6_ble_lifecycle_status_t status;

    if (lifecycle == NULL || !lifecycle->initialized || peer == NULL ||
        connection_epoch == 0U) {
        return P6_BLE_LIFECYCLE_INVALID_ARGUMENT;
    }
    if (lifecycle->connected ||
        !generation_is_newer(connection_generation,
                             lifecycle->highest_connection_generation)) {
        lifecycle->stale_events++;
        return P6_BLE_LIFECYCLE_STALE_EVENT;
    }

    lifecycle->connected = true;
    lifecycle->connection_generation = connection_generation;
    lifecycle->connection_epoch = connection_epoch;
    lifecycle->highest_connection_generation = connection_generation;
    lifecycle->generation_blocked = false;
    lifecycle->mitm_authenticated = false;
    lifecycle->connections++;
    status = attach_manager(lifecycle, peer);
    if (status == P6_BLE_LIFECYCLE_IDENTITY_LOOKUP_ERROR ||
        status == P6_BLE_LIFECYCLE_BOND_NOT_FOUND ||
        status == P6_BLE_LIFECYCLE_PEER_MISMATCH) {
        block_current_generation(lifecycle);
    }
    return status;
}

p6_ble_lifecycle_status_t p6_ble_lifecycle_on_security(
    p6_ble_lifecycle_t *lifecycle,
    uint32_t connection_generation,
    const p6_ble_peer_observation_t *peer,
    bool encrypted,
    bool bonded,
    bool mitm_authenticated)
{
    p6_ble_lifecycle_status_t status;
    p6_session_status_t manager_status;

    if (lifecycle == NULL || !lifecycle->initialized || peer == NULL) {
        return P6_BLE_LIFECYCLE_INVALID_ARGUMENT;
    }
    if (!current_generation(lifecycle, connection_generation) ||
        lifecycle->generation_blocked) {
        lifecycle->stale_events++;
        return P6_BLE_LIFECYCLE_STALE_EVENT;
    }
    lifecycle->security_events++;
    lifecycle->mitm_authenticated = mitm_authenticated;

    status = validate_peer(peer);
    if (status != P6_BLE_LIFECYCLE_OK) {
        lifecycle->identity_rejections++;
        block_current_generation(lifecycle);
        return status;
    }
    if (!lifecycle->manager_connected) {
        status = attach_manager(lifecycle, peer);
        if (status != P6_BLE_LIFECYCLE_OK) {
            if (status == P6_BLE_LIFECYCLE_PEER_MISMATCH) {
                block_current_generation(lifecycle);
            }
            return status;
        }
    } else if (!p6_bond_identity_equal(&lifecycle->bound_identity,
                                       &peer->identity)) {
        lifecycle->identity_rejections++;
        block_current_generation(lifecycle);
        return P6_BLE_LIFECYCLE_PEER_MISMATCH;
    }

    manager_status = p6_session_manager_observe_security(
        lifecycle->manager, connection_generation,
        bonded && peer->bond_store_present, encrypted);
    if (manager_status != P6_SESSION_OK) {
        return manager_status == P6_SESSION_NOT_SECURE
                   ? P6_BLE_LIFECYCLE_NOT_SECURE
                   : P6_BLE_LIFECYCLE_MANAGER_REJECTED;
    }

    {
        p6_authorization_snapshot_t snapshot;

        if (p6_session_manager_get_snapshot(lifecycle->manager, &snapshot) !=
            P6_SESSION_OK) {
            return P6_BLE_LIFECYCLE_MANAGER_REJECTED;
        }
        if (snapshot.authorization_verified) {
            return snapshot.connection_generation == connection_generation
                       ? P6_BLE_LIFECYCLE_OK
                       : P6_BLE_LIFECYCLE_MANAGER_REJECTED;
        }
    }

    lifecycle->session_start_attempts++;
    manager_status = p6_session_manager_start_session(
        lifecycle->manager, connection_generation);
    if (manager_status == P6_SESSION_OK) {
        lifecycle->session_authorized++;
        return P6_BLE_LIFECYCLE_OK;
    }
    if (manager_status == P6_SESSION_NOT_REGISTERED) {
        return P6_BLE_LIFECYCLE_REGISTRATION_UNAVAILABLE;
    }
    return P6_BLE_LIFECYCLE_SESSION_NOT_AUTHORIZED;
}

p6_ble_lifecycle_status_t p6_ble_lifecycle_on_disconnect(
    p6_ble_lifecycle_t *lifecycle,
    uint32_t connection_generation)
{
    p6_session_status_t manager_status = P6_SESSION_OK;

    if (lifecycle == NULL || !lifecycle->initialized) {
        return P6_BLE_LIFECYCLE_INVALID_ARGUMENT;
    }
    if (!current_generation(lifecycle, connection_generation)) {
        lifecycle->stale_events++;
        return P6_BLE_LIFECYCLE_STALE_EVENT;
    }
    if (lifecycle->manager_connected) {
        manager_status = p6_session_manager_disconnect(
            lifecycle->manager, connection_generation);
    }
    lifecycle->connected = false;
    lifecycle->manager_connected = false;
    lifecycle->generation_blocked = false;
    lifecycle->mitm_authenticated = false;
    lifecycle->connection_generation = 0U;
    lifecycle->connection_epoch = 0U;
    memset(&lifecycle->bound_identity, 0,
           sizeof(lifecycle->bound_identity));
    lifecycle->disconnects++;
    return manager_status == P6_SESSION_OK
               ? P6_BLE_LIFECYCLE_OK
               : P6_BLE_LIFECYCLE_MANAGER_REJECTED;
}

p6_ble_lifecycle_status_t p6_ble_lifecycle_on_app_state(
    p6_ble_lifecycle_t *lifecycle,
    uint32_t connection_generation,
    uint32_t app_instance_id,
    bool app_active)
{
    p6_session_status_t manager_status;

    if (lifecycle == NULL || !lifecycle->initialized) {
        return P6_BLE_LIFECYCLE_INVALID_ARGUMENT;
    }
    if (!current_generation(lifecycle, connection_generation) ||
        lifecycle->generation_blocked) {
        lifecycle->stale_events++;
        return P6_BLE_LIFECYCLE_STALE_EVENT;
    }
    if (!lifecycle->manager_connected) {
        return P6_BLE_LIFECYCLE_REGISTRATION_UNAVAILABLE;
    }
    manager_status = p6_session_manager_observe_app_state(
        lifecycle->manager, connection_generation, app_instance_id,
        app_active);
    if (manager_status == P6_SESSION_OK) {
        return P6_BLE_LIFECYCLE_OK;
    }
    if (manager_status == P6_SESSION_EXPIRED ||
        manager_status == P6_SESSION_NOT_AUTHORIZED) {
        return P6_BLE_LIFECYCLE_SESSION_NOT_AUTHORIZED;
    }
    return P6_BLE_LIFECYCLE_MANAGER_REJECTED;
}

void p6_ble_lifecycle_get_snapshot(
    const p6_ble_lifecycle_t *lifecycle,
    p6_ble_lifecycle_snapshot_t *out_snapshot)
{
    p6_authorization_snapshot_t manager_snapshot;

    if (lifecycle == NULL || out_snapshot == NULL) {
        return;
    }
    memset(out_snapshot, 0, sizeof(*out_snapshot));
    out_snapshot->connections = lifecycle->connections;
    out_snapshot->security_events = lifecycle->security_events;
    out_snapshot->disconnects = lifecycle->disconnects;
    out_snapshot->stale_events = lifecycle->stale_events;
    out_snapshot->identity_rejections = lifecycle->identity_rejections;
    out_snapshot->session_start_attempts = lifecycle->session_start_attempts;
    out_snapshot->session_authorized = lifecycle->session_authorized;
    out_snapshot->connected = lifecycle->connected;
    out_snapshot->manager_connected = lifecycle->manager_connected;
    out_snapshot->generation_blocked = lifecycle->generation_blocked;
    out_snapshot->mitm_authenticated = lifecycle->mitm_authenticated;
    out_snapshot->connection_generation = lifecycle->connection_generation;
    out_snapshot->connection_epoch = lifecycle->connection_epoch;
    if (p6_session_manager_get_snapshot(lifecycle->manager,
                                        &manager_snapshot) == P6_SESSION_OK) {
        out_snapshot->authorization_verified =
            manager_snapshot.authorization_verified;
    }
}

bool p6_ble_lifecycle_authorization_is_current(
    const p6_ble_lifecycle_t *lifecycle,
    bool connection_available,
    bool connected,
    uint32_t connection_generation,
    uint32_t connection_epoch)
{
    p6_authorization_snapshot_t snapshot;

    if (lifecycle == NULL || !lifecycle->initialized ||
        !connection_available || !connected || !lifecycle->connected ||
        lifecycle->generation_blocked || connection_generation == 0U ||
        connection_epoch == 0U ||
        lifecycle->connection_generation != connection_generation ||
        lifecycle->connection_epoch != connection_epoch) {
        return false;
    }
    return p6_session_manager_get_snapshot(lifecycle->manager, &snapshot) ==
               P6_SESSION_OK &&
           snapshot.authorization_verified && snapshot.session_valid &&
           snapshot.connection_generation == connection_generation;
}
