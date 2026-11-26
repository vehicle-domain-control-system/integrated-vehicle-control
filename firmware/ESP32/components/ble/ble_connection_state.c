#include "ble_connection_state.h"

#include <string.h>

static uint32_t next_nonzero(uint32_t value)
{
    value++;
    return value == 0U ? 1U : value;
}

void ble_connection_state_init(ble_connection_state_t *state)
{
    if (state == NULL) {
        return;
    }
    memset(state, 0, sizeof(*state));
    state->initialized = true;
    state->connection_handle = BLE_CONNECTION_HANDLE_NONE;
}

void ble_connection_state_set_available(ble_connection_state_t *state,
                                        bool available)
{
    if (state == NULL || !state->initialized) {
        return;
    }
    if (state->available == available && !state->connected) {
        return;
    }
    state->connection_epoch = next_nonzero(state->connection_epoch);
    state->available = available;
    state->connected = false;
    state->connection_handle = BLE_CONNECTION_HANDLE_NONE;
}

bool ble_connection_state_connect(ble_connection_state_t *state,
                                  uint16_t connection_handle)
{
    if (state == NULL || !state->initialized || !state->available ||
        connection_handle == BLE_CONNECTION_HANDLE_NONE || state->connected) {
        return false;
    }
    state->connection_generation =
        next_nonzero(state->connection_generation);
    state->connection_epoch = next_nonzero(state->connection_epoch);
    state->connection_handle = connection_handle;
    state->connected = true;
    return true;
}

bool ble_connection_state_disconnect(ble_connection_state_t *state,
                                     uint16_t connection_handle)
{
    if (state == NULL || !state->initialized || !state->available ||
        !state->connected || state->connection_handle != connection_handle) {
        return false;
    }
    state->connection_epoch = next_nonzero(state->connection_epoch);
    state->connected = false;
    state->connection_handle = BLE_CONNECTION_HANDLE_NONE;
    return true;
}

void ble_connection_state_snapshot(const ble_connection_state_t *state,
                                   ble_connection_snapshot_t *out_snapshot)
{
    if (state == NULL || out_snapshot == NULL) {
        return;
    }
    memset(out_snapshot, 0, sizeof(*out_snapshot));
    out_snapshot->available = state->initialized && state->available;
    out_snapshot->connected = out_snapshot->available && state->connected;
    out_snapshot->connection_handle = state->connection_handle;
    out_snapshot->connection_generation = state->connection_generation;
    out_snapshot->connection_epoch = state->connection_epoch;
}
