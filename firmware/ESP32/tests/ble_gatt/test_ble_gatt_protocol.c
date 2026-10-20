#include "ble_gatt_protocol.h"

#include <stdio.h>
#include <string.h>

#define CHECK(condition) do { \
    if (!(condition)) { \
        printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #condition); \
        return false; \
    } \
} while (0)

static void write_u32_le(uint8_t *data, uint32_t value)
{
    data[0] = (uint8_t)value;
    data[1] = (uint8_t)(value >> 8U);
    data[2] = (uint8_t)(value >> 16U);
    data[3] = (uint8_t)(value >> 24U);
}

static size_t make_fragment(uint8_t type,
                            uint16_t transfer_id,
                            uint16_t total,
                            uint8_t index,
                            uint8_t count,
                            const uint8_t *payload,
                            size_t payload_length,
                            uint8_t *out)
{
    out[0] = 1U;
    out[1] = type;
    out[2] = (uint8_t)transfer_id;
    out[3] = (uint8_t)(transfer_id >> 8U);
    out[4] = (uint8_t)total;
    out[5] = (uint8_t)(total >> 8U);
    out[6] = index;
    out[7] = count;
    memcpy(&out[8], payload, payload_length);
    return 8U + payload_length;
}

static bool test_app_state(void)
{
    const uint8_t active[] = {1U, 0x78U, 0x56U, 0x34U, 0x12U};
    uint8_t invalid[5];
    ble_gatt_app_state_t state;

    CHECK(ble_gatt_protocol_decode_app_state(active, sizeof(active), &state) == 0U);
    CHECK(state.active);
    CHECK(state.app_instance_id == 0x12345678U);
    CHECK(ble_gatt_protocol_decode_app_state(active, 4U, &state) == BLE_GATT_ATT_ERR_PAYLOAD_LENGTH_INVALID);
    memcpy(invalid, active, sizeof(invalid));
    invalid[0] = 2U;
    CHECK(ble_gatt_protocol_decode_app_state(invalid, sizeof(invalid), &state) == BLE_GATT_ATT_ERR_VALUE_INVALID);
    memset(invalid, 0, sizeof(invalid));
    CHECK(ble_gatt_protocol_decode_app_state(invalid, sizeof(invalid), &state) == BLE_GATT_ATT_ERR_VALUE_INVALID);
    return true;
}

static bool test_single_fragment_and_golden_bytes(void)
{
    uint8_t payload[12] = {0};
    uint8_t frame[32];
    const uint8_t expected_header[] = {1U, 0x10U, 0x34U, 0x12U, 12U, 0U, 0U, 1U};
    ble_gatt_reassembler_t reassembler = {0};
    ble_gatt_message_t message;
    bool complete = false;
    size_t length;

    write_u32_le(payload, 7U);
    payload[4] = 1U;
    payload[5] = 0U;
    length = make_fragment(0x10U, 0x1234U, sizeof(payload), 0U, 1U,
                           payload, sizeof(payload), frame);
    CHECK(memcmp(frame, expected_header, sizeof(expected_header)) == 0);
    CHECK(ble_gatt_protocol_accept_app_command(&reassembler, frame, length, 10U,
                                                &complete, &message) == 0U);
    CHECK(complete);
    CHECK(message.message_type == 0x10U);
    CHECK(message.payload_length == sizeof(payload));
    CHECK(memcmp(message.payload, payload, sizeof(payload)) == 0);
    return true;
}

static bool test_multifragment_query(void)
{
    uint8_t payload[20] = {0};
    uint8_t first[32];
    uint8_t second[32];
    ble_gatt_reassembler_t reassembler = {0};
    ble_gatt_message_t message;
    bool complete = false;

    write_u32_le(payload, 0x11223344U);
    payload[4] = 3U;
    CHECK(ble_gatt_protocol_accept_app_command(
              &reassembler, first,
              make_fragment(0x11U, 9U, sizeof(payload), 0U, 2U, payload, 12U, first),
              100U, &complete, &message) == 0U);
    CHECK(!complete);
    CHECK(ble_gatt_protocol_accept_app_command(
              &reassembler, second,
              make_fragment(0x11U, 9U, sizeof(payload), 1U, 2U, &payload[12], 8U, second),
              150U, &complete, &message) == 0U);
    CHECK(complete);
    CHECK(message.payload_length == sizeof(payload));
    CHECK(memcmp(message.payload, payload, sizeof(payload)) == 0);
    return true;
}

static bool test_fragment_errors_and_recovery(void)
{
    uint8_t payload[12] = {0};
    uint8_t frame[32];
    ble_gatt_reassembler_t reassembler = {0};
    ble_gatt_message_t message;
    bool complete = false;
    size_t length;

    write_u32_le(payload, 1U);
    payload[4] = 1U;
    length = make_fragment(0x10U, 1U, sizeof(payload), 0U, 1U, payload, sizeof(payload), frame);

    frame[0] = 2U;
    CHECK(ble_gatt_protocol_accept_app_command(&reassembler, frame, length, 0U, &complete, &message) ==
          BLE_GATT_ATT_ERR_VERSION_UNSUPPORTED);
    frame[0] = 1U;
    frame[1] = 0x20U;
    CHECK(ble_gatt_protocol_accept_app_command(&reassembler, frame, length, 0U, &complete, &message) ==
          BLE_GATT_ATT_ERR_MESSAGE_TYPE_UNSUPPORTED);
    frame[1] = 0x10U;
    frame[4] = 11U;
    CHECK(ble_gatt_protocol_accept_app_command(&reassembler, frame, length, 0U, &complete, &message) ==
          BLE_GATT_ATT_ERR_PAYLOAD_LENGTH_INVALID);

    length = make_fragment(0x10U, 2U, sizeof(payload), 0U, 2U, payload, 6U, frame);
    CHECK(ble_gatt_protocol_accept_app_command(&reassembler, frame, length, 10U, &complete, &message) == 0U);
    length = make_fragment(0x10U, 2U, sizeof(payload), 1U, 2U, &payload[6], 6U, frame);
    CHECK(ble_gatt_protocol_accept_app_command(&reassembler, frame, length, 1011U, &complete, &message) ==
          BLE_GATT_ATT_ERR_FRAGMENT_INVALID);

    length = make_fragment(0x10U, 3U, sizeof(payload), 0U, 1U, payload, sizeof(payload), frame);
    CHECK(ble_gatt_protocol_accept_app_command(&reassembler, frame, length, 1012U, &complete, &message) == 0U);
    CHECK(complete);
    return true;
}

static bool test_semantic_validation(void)
{
    uint8_t request[12] = {0};
    uint8_t query[20] = {0};
    uint8_t warning[9] = {0};

    write_u32_le(request, 1U);
    request[4] = 4U;
    request[5] = 1U;
    request[6] = 1U;
    CHECK(ble_gatt_protocol_validate_uplink_payload(0x10U, request, sizeof(request)) == 0U);
    request[7] = 1U;
    CHECK(ble_gatt_protocol_validate_uplink_payload(0x10U, request, sizeof(request)) ==
          BLE_GATT_ATT_ERR_VALUE_INVALID);

    write_u32_le(query, 2U);
    query[4] = 1U;
    CHECK(ble_gatt_protocol_validate_uplink_payload(0x11U, query, sizeof(query)) == 0U);
    query[5] = 1U;
    CHECK(ble_gatt_protocol_validate_uplink_payload(0x11U, query, sizeof(query)) ==
          BLE_GATT_ATT_ERR_VALUE_INVALID);

    warning[0] = 8U;
    write_u32_le(&warning[1], 3U);
    write_u32_le(&warning[5], 4U);
    CHECK(ble_gatt_protocol_validate_uplink_payload(0x12U, warning, sizeof(warning)) == 0U);
    warning[0] = 9U;
    CHECK(ble_gatt_protocol_validate_uplink_payload(0x12U, warning, sizeof(warning)) ==
          BLE_GATT_ATT_ERR_VALUE_INVALID);
    return true;
}

static bool test_downlink_lengths_encoding_and_sequence(void)
{
    const uint8_t types[] = {0x20U, 0x21U, 0x22U, 0x23U, 0x24U, 0x25U,
                             0x30U, 0x31U, 0x32U, 0x33U, 0x34U, 0x35U,
                             0x36U, 0x37U, 0x38U, 0x39U, 0x3AU, 0x40U};
    const size_t lengths[] = {44U, 40U, 62U, 30U, 35U, 36U, 40U, 44U, 45U,
                              42U, 60U, 45U, 39U, 54U, 42U, 42U, 48U, 24U};
    uint8_t payload[62] = {0};
    uint8_t frame[70];
    size_t frame_length = 0U;
    uint16_t counter = UINT16_MAX;
    uint16_t transfer_id;
    size_t index;

    for (index = 0U; index < sizeof(types); ++index) {
        CHECK(ble_gatt_protocol_payload_length(types[index], false) == lengths[index]);
    }
    CHECK(ble_gatt_protocol_encode_fragment(0x20U, 1U, 44U, 0U, 1U,
                                             payload, 44U, frame, sizeof(frame), &frame_length));
    CHECK(frame_length == 52U);
    CHECK(frame[0] == 1U && frame[1] == 0x20U && frame[2] == 1U && frame[3] == 0U);
    CHECK(frame[4] == 44U && frame[5] == 0U && frame[6] == 0U && frame[7] == 1U);
    CHECK(ble_gatt_protocol_next_transfer_id(&counter, &transfer_id));
    CHECK(transfer_id == UINT16_MAX && counter == 1U);
    CHECK(ble_gatt_protocol_next_transfer_id(&counter, &transfer_id));
    CHECK(transfer_id == 1U && counter == 2U);
    return true;
}

typedef bool (*test_function_t)(void);

int main(void)
{
    const struct {
        const char *name;
        test_function_t function;
    } tests[] = {
        {"app_state", test_app_state},
        {"single_fragment_golden", test_single_fragment_and_golden_bytes},
        {"multifragment_query", test_multifragment_query},
        {"fragment_errors_recovery", test_fragment_errors_and_recovery},
        {"semantic_validation", test_semantic_validation},
        {"downlink_lengths_encoding_sequence", test_downlink_lengths_encoding_and_sequence},
    };
    size_t index;

    for (index = 0U; index < sizeof(tests) / sizeof(tests[0]); ++index) {
        if (!tests[index].function()) {
            return 1;
        }
        printf("[PASS] %s\n", tests[index].name);
    }
    printf("All %zu BLE GATT protocol test groups passed.\n", sizeof(tests) / sizeof(tests[0]));
    return 0;
}
