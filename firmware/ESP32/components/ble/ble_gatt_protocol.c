#include "ble_gatt_protocol.h"

#include <string.h>

#define BLE_MSG_REQUEST 0x10U
#define BLE_MSG_QUERY 0x11U
#define BLE_MSG_WARNING_ACK 0x12U

static uint16_t read_u16_le(const uint8_t *data)
{
    return (uint16_t)data[0] | ((uint16_t)data[1] << 8U);
}

static uint32_t read_u32_le(const uint8_t *data)
{
    return (uint32_t)data[0] | ((uint32_t)data[1] << 8U) |
           ((uint32_t)data[2] << 16U) | ((uint32_t)data[3] << 24U);
}

static void write_u16_le(uint8_t *data, uint16_t value)
{
    data[0] = (uint8_t)value;
    data[1] = (uint8_t)(value >> 8U);
}

static bool all_zero(const uint8_t *data, size_t length)
{
    size_t index;

    for (index = 0U; index < length; ++index) {
        if (data[index] != 0U) {
            return false;
        }
    }
    return true;
}

void ble_gatt_reassembler_reset(ble_gatt_reassembler_t *reassembler)
{
    if (reassembler != NULL) {
        memset(reassembler, 0, sizeof(*reassembler));
    }
}

uint8_t ble_gatt_protocol_decode_app_state(const uint8_t *data,
                                           size_t length,
                                           ble_gatt_app_state_t *out_state)
{
    uint32_t app_instance_id;

    if (data == NULL || out_state == NULL) {
        return BLE_GATT_ATT_ERR_VALUE_INVALID;
    }
    if (length != BLE_GATT_APP_STATE_LENGTH) {
        return BLE_GATT_ATT_ERR_PAYLOAD_LENGTH_INVALID;
    }
    if (data[0] > 1U) {
        return BLE_GATT_ATT_ERR_VALUE_INVALID;
    }

    app_instance_id = read_u32_le(&data[1]);
    if (app_instance_id == 0U) {
        return BLE_GATT_ATT_ERR_VALUE_INVALID;
    }

    out_state->active = data[0] == 1U;
    out_state->app_instance_id = app_instance_id;
    return 0U;
}

size_t ble_gatt_protocol_payload_length(uint8_t message_type, bool app_to_gateway)
{
    if (app_to_gateway) {
        switch (message_type) {
        case BLE_MSG_REQUEST:
            return 12U;
        case BLE_MSG_QUERY:
            return 20U;
        case BLE_MSG_WARNING_ACK:
            return 9U;
        default:
            return 0U;
        }
    }

    switch (message_type) {
    case 0x20U: return 44U;
    case 0x21U: return 40U;
    case 0x22U: return 62U;
    case 0x23U: return 30U;
    case 0x24U: return 35U;
    case 0x25U: return 36U;
    case 0x30U: return 40U;
    case 0x31U: return 44U;
    case 0x32U: return 45U;
    case 0x33U: return 42U;
    case 0x34U: return 60U;
    case 0x35U: return 45U;
    case 0x36U: return 39U;
    case 0x37U: return 54U;
    case 0x38U: return 42U;
    case 0x39U: return 42U;
    case 0x3AU: return 48U;
    case 0x40U: return 24U;
    default: return 0U;
    }
}

uint8_t ble_gatt_protocol_parse_fragment(const uint8_t *data,
                                         size_t length,
                                         ble_gatt_fragment_view_t *out_fragment)
{
    size_t expected_length;
    uint16_t total_length;
    uint8_t fragment_count;
    size_t fragment_data_length;

    if (data == NULL || out_fragment == NULL) {
        return BLE_GATT_ATT_ERR_FRAGMENT_INVALID;
    }
    if (length < BLE_GATT_FRAGMENT_HEADER_SIZE || length > BLE_GATT_MAX_FRAME_SIZE) {
        return BLE_GATT_ATT_ERR_FRAGMENT_INVALID;
    }
    if (data[0] != BLE_GATT_PROTOCOL_VERSION) {
        return BLE_GATT_ATT_ERR_VERSION_UNSUPPORTED;
    }

    expected_length = ble_gatt_protocol_payload_length(data[1], true);
    if (expected_length == 0U) {
        return BLE_GATT_ATT_ERR_MESSAGE_TYPE_UNSUPPORTED;
    }

    total_length = read_u16_le(&data[4]);
    if ((size_t)total_length != expected_length) {
        return BLE_GATT_ATT_ERR_PAYLOAD_LENGTH_INVALID;
    }

    fragment_count = data[7];
    fragment_data_length = length - BLE_GATT_FRAGMENT_HEADER_SIZE;
    if (read_u16_le(&data[2]) == 0U || fragment_count == 0U ||
        data[6] >= fragment_count || fragment_data_length == 0U ||
        fragment_data_length > total_length || fragment_count > total_length) {
        return BLE_GATT_ATT_ERR_FRAGMENT_INVALID;
    }
    if (fragment_count == 1U && fragment_data_length != total_length) {
        return BLE_GATT_ATT_ERR_FRAGMENT_INVALID;
    }

    out_fragment->message_type = data[1];
    out_fragment->transfer_id = read_u16_le(&data[2]);
    out_fragment->total_payload_length = total_length;
    out_fragment->fragment_index = data[6];
    out_fragment->fragment_count = fragment_count;
    out_fragment->data = &data[BLE_GATT_FRAGMENT_HEADER_SIZE];
    out_fragment->data_length = fragment_data_length;
    return 0U;
}

static uint8_t validate_request(const uint8_t *payload)
{
    uint8_t kind = payload[4];
    uint8_t operation = payload[5];
    const uint8_t *args = &payload[6];

    if (read_u32_le(payload) == 0U) {
        return BLE_GATT_ATT_ERR_VALUE_INVALID;
    }

    switch (kind) {
    case 1U:
        return (operation <= 1U && all_zero(args, 6U)) ? 0U : BLE_GATT_ATT_ERR_VALUE_INVALID;
    case 2U:
        if (operation == 1U) {
            return (read_u16_le(args) != 0x8000U && all_zero(&args[2], 4U))
                       ? 0U : BLE_GATT_ATT_ERR_VALUE_INVALID;
        }
        if (operation == 2U) {
            return (args[0] <= 1U && all_zero(&args[1], 5U)) ? 0U : BLE_GATT_ATT_ERR_VALUE_INVALID;
        }
        if (operation == 3U) {
            return (args[0] <= 3U && all_zero(&args[1], 5U)) ? 0U : BLE_GATT_ATT_ERR_VALUE_INVALID;
        }
        return BLE_GATT_ATT_ERR_VALUE_INVALID;
    case 3U:
        if (operation == 1U) {
            return (args[0] <= 1U && all_zero(&args[1], 5U)) ? 0U : BLE_GATT_ATT_ERR_VALUE_INVALID;
        }
        if (operation == 2U) {
            return (args[0] <= 100U && all_zero(&args[1], 5U)) ? 0U : BLE_GATT_ATT_ERR_VALUE_INVALID;
        }
        if (operation == 3U) {
            return all_zero(&args[3], 3U) ? 0U : BLE_GATT_ATT_ERR_VALUE_INVALID;
        }
        return BLE_GATT_ATT_ERR_VALUE_INVALID;
    case 4U:
        return (operation == 1U && args[0] <= 1U && all_zero(&args[1], 5U))
                   ? 0U : BLE_GATT_ATT_ERR_VALUE_INVALID;
    default:
        return BLE_GATT_ATT_ERR_VALUE_INVALID;
    }
}

uint8_t ble_gatt_protocol_validate_uplink_payload(uint8_t message_type,
                                                  const uint8_t *payload,
                                                  size_t payload_length)
{
    size_t expected_length = ble_gatt_protocol_payload_length(message_type, true);

    if (expected_length == 0U) {
        return BLE_GATT_ATT_ERR_MESSAGE_TYPE_UNSUPPORTED;
    }
    if (payload == NULL || payload_length != expected_length) {
        return BLE_GATT_ATT_ERR_PAYLOAD_LENGTH_INVALID;
    }

    switch (message_type) {
    case BLE_MSG_REQUEST:
        return validate_request(payload);
    case BLE_MSG_QUERY:
        if (read_u32_le(payload) == 0U || payload[4] > 3U || payload[5] != 0U ||
            payload[18] != 0U || payload[19] != 0U) {
            return BLE_GATT_ATT_ERR_VALUE_INVALID;
        }
        return 0U;
    case BLE_MSG_WARNING_ACK:
        if (payload[0] < 1U || payload[0] > 8U || read_u32_le(&payload[1]) == 0U ||
            read_u32_le(&payload[5]) == 0U) {
            return BLE_GATT_ATT_ERR_VALUE_INVALID;
        }
        return 0U;
    default:
        return BLE_GATT_ATT_ERR_MESSAGE_TYPE_UNSUPPORTED;
    }
}

uint8_t ble_gatt_protocol_accept_app_command(ble_gatt_reassembler_t *reassembler,
                                             const uint8_t *data,
                                             size_t length,
                                             uint64_t now_ms,
                                             bool *out_complete,
                                             ble_gatt_message_t *out_message)
{
    ble_gatt_fragment_view_t fragment;
    uint8_t error;

    if (reassembler == NULL || out_complete == NULL || out_message == NULL) {
        return BLE_GATT_ATT_ERR_FRAGMENT_INVALID;
    }
    *out_complete = false;

    error = ble_gatt_protocol_parse_fragment(data, length, &fragment);
    if (error != 0U) {
        ble_gatt_reassembler_reset(reassembler);
        return error;
    }

    if (reassembler->active && now_ms - reassembler->started_at_ms > BLE_GATT_REASSEMBLY_TIMEOUT_MS) {
        ble_gatt_reassembler_reset(reassembler);
    }

    if (fragment.fragment_index == 0U) {
        ble_gatt_reassembler_reset(reassembler);
        reassembler->active = true;
        reassembler->message_type = fragment.message_type;
        reassembler->transfer_id = fragment.transfer_id;
        reassembler->total_payload_length = fragment.total_payload_length;
        reassembler->fragment_count = fragment.fragment_count;
        reassembler->started_at_ms = now_ms;
    } else if (!reassembler->active || reassembler->message_type != fragment.message_type ||
               reassembler->transfer_id != fragment.transfer_id ||
               reassembler->total_payload_length != fragment.total_payload_length ||
               reassembler->fragment_count != fragment.fragment_count) {
        ble_gatt_reassembler_reset(reassembler);
        return BLE_GATT_ATT_ERR_FRAGMENT_INVALID;
    }

    if (fragment.fragment_index != reassembler->next_fragment_index ||
        reassembler->received_length + fragment.data_length > reassembler->total_payload_length) {
        ble_gatt_reassembler_reset(reassembler);
        return BLE_GATT_ATT_ERR_FRAGMENT_INVALID;
    }

    memcpy(&reassembler->payload[reassembler->received_length], fragment.data, fragment.data_length);
    reassembler->received_length += fragment.data_length;
    reassembler->next_fragment_index++;

    if (reassembler->next_fragment_index < reassembler->fragment_count) {
        return 0U;
    }
    if (reassembler->received_length != reassembler->total_payload_length) {
        ble_gatt_reassembler_reset(reassembler);
        return BLE_GATT_ATT_ERR_FRAGMENT_INVALID;
    }

    error = ble_gatt_protocol_validate_uplink_payload(reassembler->message_type,
                                                      reassembler->payload,
                                                      reassembler->received_length);
    if (error == 0U) {
        out_message->message_type = reassembler->message_type;
        out_message->payload_length = reassembler->received_length;
        memcpy(out_message->payload, reassembler->payload, reassembler->received_length);
        *out_complete = true;
    }
    ble_gatt_reassembler_reset(reassembler);
    return error;
}

bool ble_gatt_protocol_next_transfer_id(uint16_t *counter, uint16_t *out_transfer_id)
{
    if (counter == NULL || out_transfer_id == NULL) {
        return false;
    }
    if (*counter == 0U) {
        *counter = 1U;
    }
    *out_transfer_id = *counter;
    *counter = (*counter == UINT16_MAX) ? 1U : (uint16_t)(*counter + 1U);
    return true;
}

bool ble_gatt_protocol_encode_fragment(uint8_t message_type,
                                       uint16_t transfer_id,
                                       uint16_t total_payload_length,
                                       uint8_t fragment_index,
                                       uint8_t fragment_count,
                                       const uint8_t *fragment_data,
                                       size_t fragment_data_length,
                                       uint8_t *out,
                                       size_t out_capacity,
                                       size_t *out_length)
{
    size_t expected_length = ble_gatt_protocol_payload_length(message_type, false);

    if (expected_length == 0U || total_payload_length != expected_length || transfer_id == 0U ||
        fragment_count == 0U || fragment_index >= fragment_count || fragment_data == NULL ||
        fragment_data_length == 0U || fragment_data_length > total_payload_length ||
        out == NULL || out_length == NULL ||
        out_capacity < BLE_GATT_FRAGMENT_HEADER_SIZE + fragment_data_length) {
        return false;
    }

    out[0] = BLE_GATT_PROTOCOL_VERSION;
    out[1] = message_type;
    write_u16_le(&out[2], transfer_id);
    write_u16_le(&out[4], total_payload_length);
    out[6] = fragment_index;
    out[7] = fragment_count;
    memcpy(&out[BLE_GATT_FRAGMENT_HEADER_SIZE], fragment_data, fragment_data_length);
    *out_length = BLE_GATT_FRAGMENT_HEADER_SIZE + fragment_data_length;
    return true;
}
