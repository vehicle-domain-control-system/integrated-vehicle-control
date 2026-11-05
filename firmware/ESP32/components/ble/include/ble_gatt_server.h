#ifndef BLE_GATT_SERVER_H
#define BLE_GATT_SERVER_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "esp_err.h"
#include "ble_gatt_protocol.h"

#ifdef __cplusplus
extern "C" {
#endif

#define BLE_GATT_DEVICE_NAME "VEHICLE-GW"
#define BLE_GATT_MIN_APP_MTU 24U

typedef enum {
    BLE_GATT_EVENT_CONNECTED = 0,
    BLE_GATT_EVENT_DISCONNECTED,
    BLE_GATT_EVENT_SECURITY_CHANGED,
    BLE_GATT_EVENT_SUBSCRIPTION_CHANGED,
    BLE_GATT_EVENT_MTU_CHANGED,
    BLE_GATT_EVENT_APP_COMMAND,
    BLE_GATT_EVENT_APP_STATE,
} ble_gatt_event_type_t;

typedef enum {
    BLE_GATT_SEND_NOTIFY = 0,
    BLE_GATT_SEND_INDICATE,
} ble_gatt_send_mode_t;

typedef struct {
    ble_gatt_event_type_t type;
    uint16_t connection_handle;
    uint32_t connection_generation;
    union {
        struct {
            uint16_t reason;
        } disconnected;
        struct {
            bool encrypted;
            bool authenticated;
            bool bonded;
        } security;
        struct {
            bool gateway_status_notify;
            bool vehicle_data_notify;
        } subscription;
        struct {
            uint16_t mtu;
        } mtu;
        struct {
            uint8_t message_type;
            size_t payload_length;
            uint8_t payload[BLE_GATT_MAX_UPLINK_PAYLOAD];
        } app_command;
        ble_gatt_app_state_t app_state;
    } data;
} ble_gatt_server_event_t;

/* event is a copied snapshot and remains valid only for the callback duration. */
typedef void (*ble_gatt_server_event_callback_t)(const ble_gatt_server_event_t *event,
                                                 void *context);

typedef struct {
    uint16_t event_queue_depth;
    uint32_t event_task_stack_size;
    uint32_t event_task_priority;
    bool bonding_enabled;
    bool mitm_required;
    bool secure_connections_enabled;
    uint8_t io_capability;
    ble_gatt_server_event_callback_t event_callback;
    void *event_context;
} ble_gatt_server_config_t;

typedef struct {
    uint32_t connections;
    uint32_t disconnections;
    uint32_t rejected_writes;
    uint32_t event_queue_overflows;
    uint32_t notifications_sent;
    uint32_t notification_failures;
} ble_gatt_server_stats_t;

/* Development-only baseline; TBD-SEC-01 must be resolved before production use. */
ble_gatt_server_config_t ble_gatt_server_development_config(void);

esp_err_t ble_gatt_server_start(const ble_gatt_server_config_t *config);
esp_err_t ble_gatt_server_stop(void);

esp_err_t ble_gatt_server_set_gateway_status(const uint8_t status[BLE_GATT_GATEWAY_STATUS_LENGTH]);
esp_err_t ble_gatt_server_send_gateway_status(ble_gatt_send_mode_t mode);
esp_err_t ble_gatt_server_send_vehicle_data(uint8_t message_type,
                                            const uint8_t *payload,
                                            size_t payload_length,
                                            ble_gatt_send_mode_t mode);

bool ble_gatt_server_is_connected(void);
bool ble_gatt_server_is_secure(void);
void ble_gatt_server_get_stats(ble_gatt_server_stats_t *out_stats);

#ifdef __cplusplus
}
#endif

#endif
