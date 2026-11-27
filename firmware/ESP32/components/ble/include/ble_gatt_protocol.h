#ifndef BLE_GATT_PROTOCOL_H
#define BLE_GATT_PROTOCOL_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define BLE_GATT_PROTOCOL_VERSION 1U
#define BLE_GATT_FRAGMENT_HEADER_SIZE 8U
#define BLE_GATT_APP_STATE_LENGTH 5U
#define BLE_GATT_GATEWAY_STATUS_LENGTH 21U
#define BLE_GATT_MAX_UPLINK_PAYLOAD 20U
#define BLE_GATT_MAX_DOWNLINK_PAYLOAD 62U
#define BLE_GATT_MAX_FRAME_SIZE (BLE_GATT_FRAGMENT_HEADER_SIZE + BLE_GATT_MAX_DOWNLINK_PAYLOAD)
#define BLE_GATT_REASSEMBLY_TIMEOUT_MS 1000U

#define BLE_GATT_ATT_ERR_VERSION_UNSUPPORTED 0x80U
#define BLE_GATT_ATT_ERR_MESSAGE_TYPE_UNSUPPORTED 0x81U
#define BLE_GATT_ATT_ERR_FRAGMENT_INVALID 0x82U
#define BLE_GATT_ATT_ERR_PAYLOAD_LENGTH_INVALID 0x83U
#define BLE_GATT_ATT_ERR_VALUE_INVALID 0x88U
#define BLE_GATT_ATT_ERR_SUBSCRIPTION_REQUIRED 0x89U

typedef struct {
    bool active;
    uint32_t app_instance_id;
} ble_gatt_app_state_t;

typedef struct {
    uint8_t message_type;
    uint16_t transfer_id;
    uint16_t total_payload_length;
    uint8_t fragment_index;
    uint8_t fragment_count;
    const uint8_t *data;
    size_t data_length;
} ble_gatt_fragment_view_t;

typedef struct {
    bool active;
    uint8_t message_type;
    uint16_t transfer_id;
    uint16_t total_payload_length;
    uint8_t fragment_count;
    uint8_t next_fragment_index;
    size_t received_length;
    uint64_t started_at_ms;
    uint8_t payload[BLE_GATT_MAX_UPLINK_PAYLOAD];
} ble_gatt_reassembler_t;

typedef struct {
    uint8_t message_type;
    size_t payload_length;
    uint8_t payload[BLE_GATT_MAX_UPLINK_PAYLOAD];
} ble_gatt_message_t;

void ble_gatt_reassembler_reset(ble_gatt_reassembler_t *reassembler);

uint8_t ble_gatt_protocol_decode_app_state(const uint8_t *data,
                                           size_t length,
                                           ble_gatt_app_state_t *out_state);

uint8_t ble_gatt_protocol_parse_fragment(const uint8_t *data,
                                         size_t length,
                                         ble_gatt_fragment_view_t *out_fragment);

uint8_t ble_gatt_protocol_accept_app_command(ble_gatt_reassembler_t *reassembler,
                                             const uint8_t *data,
                                             size_t length,
                                             uint64_t now_ms,
                                             bool *out_complete,
                                             ble_gatt_message_t *out_message);

size_t ble_gatt_protocol_payload_length(uint8_t message_type, bool app_to_gateway);

uint8_t ble_gatt_protocol_validate_uplink_payload(uint8_t message_type,
                                                  const uint8_t *payload,
                                                  size_t payload_length);

bool ble_gatt_protocol_next_transfer_id(uint16_t *counter, uint16_t *out_transfer_id);

bool ble_gatt_protocol_encode_fragment(uint8_t message_type,
                                       uint16_t transfer_id,
                                       uint16_t total_payload_length,
                                       uint8_t fragment_index,
                                       uint8_t fragment_count,
                                       const uint8_t *fragment_data,
                                       size_t fragment_data_length,
                                       uint8_t *out,
                                       size_t out_capacity,
                                       size_t *out_length);

#ifdef __cplusplus
}
#endif

#endif
