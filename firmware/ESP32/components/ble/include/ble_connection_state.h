#ifndef BLE_CONNECTION_STATE_H
#define BLE_CONNECTION_STATE_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define BLE_CONNECTION_HANDLE_NONE UINT16_MAX

typedef struct
{
    bool initialized;
    bool available;
    bool connected;
    uint16_t connection_handle;
    uint32_t connection_generation;
    uint32_t connection_epoch;
} ble_connection_state_t;

typedef struct
{
    bool available;
    bool connected;
    uint16_t connection_handle;
    uint32_t connection_generation;
    uint32_t connection_epoch;
} ble_connection_snapshot_t;

/* Caller supplies synchronization when this object crosses task boundaries. */
void ble_connection_state_init(ble_connection_state_t *state);
void ble_connection_state_set_available(ble_connection_state_t *state,
                                        bool available);
bool ble_connection_state_connect(ble_connection_state_t *state,
                                  uint16_t connection_handle);
bool ble_connection_state_disconnect(ble_connection_state_t *state,
                                     uint16_t connection_handle);
void ble_connection_state_snapshot(const ble_connection_state_t *state,
                                   ble_connection_snapshot_t *out_snapshot);

#ifdef __cplusplus
}
#endif

#endif
