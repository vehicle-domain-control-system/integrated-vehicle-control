#ifndef P6_BLE_LIFECYCLE_H
#define P6_BLE_LIFECYCLE_H

#include <stdbool.h>
#include <stdint.h>

#include "p6_session_manager.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum
{
    P6_BLE_LIFECYCLE_OK = 0,
    P6_BLE_LIFECYCLE_INVALID_ARGUMENT,
    P6_BLE_LIFECYCLE_STALE_EVENT,
    P6_BLE_LIFECYCLE_IDENTITY_UNRESOLVED,
    P6_BLE_LIFECYCLE_IDENTITY_LOOKUP_ERROR,
    P6_BLE_LIFECYCLE_BOND_NOT_FOUND,
    P6_BLE_LIFECYCLE_PEER_MISMATCH,
    P6_BLE_LIFECYCLE_NOT_SECURE,
    P6_BLE_LIFECYCLE_REGISTRATION_UNAVAILABLE,
    P6_BLE_LIFECYCLE_SESSION_NOT_AUTHORIZED,
    P6_BLE_LIFECYCLE_MANAGER_REJECTED
} p6_ble_lifecycle_status_t;

typedef struct
{
    bool identity_resolved;
    bool ota_address_is_rpa;
    bool bond_store_present;
    bool lookup_error;
    p6_bond_identity_t identity;
} p6_ble_peer_observation_t;

typedef struct
{
    uint32_t connections;
    uint32_t security_events;
    uint32_t disconnects;
    uint32_t stale_events;
    uint32_t identity_rejections;
    uint32_t session_start_attempts;
    uint32_t session_authorized;
    bool connected;
    bool manager_connected;
    bool generation_blocked;
    bool mitm_authenticated;
    bool authorization_verified;
    uint32_t connection_generation;
    uint32_t connection_epoch;
} p6_ble_lifecycle_snapshot_t;

/*
 * This object is called by one owner only.  It never registers a peer and
 * never makes authorization a Production routing fact.  Peer observations
 * are copied values; address bytes must not be logged.
 */
typedef struct
{
    bool initialized;
    bool connected;
    bool manager_connected;
    bool generation_blocked;
    bool mitm_authenticated;
    uint32_t connection_generation;
    uint32_t connection_epoch;
    uint32_t highest_connection_generation;
    p6_bond_identity_t bound_identity;
    p6_session_manager_t *manager;
    uint32_t connections;
    uint32_t security_events;
    uint32_t disconnects;
    uint32_t stale_events;
    uint32_t identity_rejections;
    uint32_t session_start_attempts;
    uint32_t session_authorized;
} p6_ble_lifecycle_t;

p6_ble_lifecycle_status_t p6_ble_lifecycle_init(
    p6_ble_lifecycle_t *lifecycle,
    p6_session_manager_t *manager);
p6_ble_lifecycle_status_t p6_ble_lifecycle_on_connect(
    p6_ble_lifecycle_t *lifecycle,
    uint32_t connection_generation,
    uint32_t connection_epoch,
    const p6_ble_peer_observation_t *peer);
p6_ble_lifecycle_status_t p6_ble_lifecycle_on_security(
    p6_ble_lifecycle_t *lifecycle,
    uint32_t connection_generation,
    const p6_ble_peer_observation_t *peer,
    bool encrypted,
    bool bonded,
    bool mitm_authenticated);
p6_ble_lifecycle_status_t p6_ble_lifecycle_on_disconnect(
    p6_ble_lifecycle_t *lifecycle,
    uint32_t connection_generation);
p6_ble_lifecycle_status_t p6_ble_lifecycle_on_app_state(
    p6_ble_lifecycle_t *lifecycle,
    uint32_t connection_generation,
    uint32_t app_instance_id,
    bool app_active);
void p6_ble_lifecycle_get_snapshot(
    const p6_ble_lifecycle_t *lifecycle,
    p6_ble_lifecycle_snapshot_t *out_snapshot);
bool p6_ble_lifecycle_authorization_is_current(
    const p6_ble_lifecycle_t *lifecycle,
    bool connection_available,
    bool connected,
    uint32_t connection_generation,
    uint32_t connection_epoch);

#ifdef __cplusplus
}
#endif

#endif
