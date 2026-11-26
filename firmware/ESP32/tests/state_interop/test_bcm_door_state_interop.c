#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "gateway_mapper.h"
#include "network/net_wire_uart_m_projection.h"
#include "uart_m_codec.h"
#include "uart_m_frame.h"

static int failures;

#define CHECK(condition)                                                        \
    do {                                                                        \
        if (!(condition)) {                                                     \
            (void)fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__,     \
                          #condition);                                          \
            failures++;                                                        \
        }                                                                       \
    } while (0)

static NetWireMBcmDoorStateRaw make_door_state(uint8_t quality)
{
    NetWireMBcmDoorStateRaw door = {0};

    door.device_context_id = UINT32_C(0x10203040);
    door.session_id = UINT64_C(0x0102030405060708);
    door.domain_boot_id = UINT32_C(0x11223344);
    door.query_id = 0U;
    door.original_can_boot_id = UINT32_C(0xa1b2c3d4);
    door.original_can_tx_sequence = UINT32_C(0x55667788);
    door.door_lock_state = 1U;
    door.door_open_state = 0U;
    door.door_composite_state = 0U;
    door.lock_quality = quality;
    door.open_quality = quality;
    door.composite_quality = quality;
    door.update_sequence = UINT32_C(0x89abcdef);
    door.source_age_ms = (quality == (uint8_t)UART_M_QUALITY_OK) ? 25U : UINT16_MAX;
    return door;
}

static void verify_network_to_esp32(uint8_t quality)
{
    const uint16_t link_sequence = UINT16_C(0x5aa5);
    const NetWireMBcmDoorStateRaw source = make_door_state(quality);
    NetWireUartFrame network_frame;
    uart_m_frame_t esp_frame;
    uart_m_message_t message;
    gateway_ble_message_t ble;
    const uart_m_bcm_door_state_t *door;

    memset(&network_frame, 0, sizeof(network_frame));
    CHECK(net_wire_m_bcm_door_state_encode(NET_WIRE_CANDIDATE_ONLY,
                                            link_sequence, &source,
                                            &network_frame) == NET_WIRE_OK);
    CHECK(network_frame.length == 50U);
    CHECK(uart_m_frame_decode(network_frame.bytes, network_frame.length,
                              &esp_frame) == UART_M_FRAME_STATUS_OK);
    CHECK(esp_frame.message_type == (uint8_t)UART_M_TYPE_BCM_DOOR_STATE);
    CHECK(esp_frame.link_sequence == link_sequence);
    CHECK(esp_frame.payload_length == 40U);
    CHECK(uart_m_codec_decode(esp_frame.message_type, esp_frame.payload,
                              esp_frame.payload_length, &message) ==
          UART_M_CODEC_OK);

    door = &message.body.bcm_door_state;
    CHECK(door->context.recipient.device_context_id == source.device_context_id);
    CHECK(door->context.recipient.session_id == source.session_id);
    CHECK(door->context.domain_boot_id == source.domain_boot_id);
    CHECK(door->context.query_id == source.query_id);
    CHECK(door->source.boot_id == source.original_can_boot_id);
    CHECK(door->source.tx_sequence == source.original_can_tx_sequence);
    CHECK((uint8_t)door->lock_state == source.door_lock_state);
    CHECK((uint8_t)door->open_state == source.door_open_state);
    CHECK((uint8_t)door->composite_state == source.door_composite_state);
    CHECK((uint8_t)door->lock_quality == quality);
    CHECK((uint8_t)door->open_quality == quality);
    CHECK((uint8_t)door->composite_quality == quality);
    CHECK(door->update_sequence == source.update_sequence);
    CHECK(door->source_age_ms == source.source_age_ms);

    CHECK(gateway_mapper_uart_downlink_to_ble(&message, &ble) ==
          GATEWAY_MAPPER_OK);
    CHECK(ble.message_type == (uint8_t)UART_M_TYPE_BCM_DOOR_STATE);
    CHECK(ble.payload_length == 40U);
    /* Android MessageCodec consumes state at 28..30, quality at 31..33,
     * update sequence at 34..37 and source age at 38..39. */
    CHECK(ble.payload[28] == source.door_lock_state);
    CHECK(ble.payload[29] == source.door_open_state);
    CHECK(ble.payload[30] == source.door_composite_state);
    CHECK(ble.payload[31] == quality);
    CHECK(ble.payload[32] == quality);
    CHECK(ble.payload[33] == quality);
    CHECK(ble.payload[34] == UINT8_C(0xef));
    CHECK(ble.payload[35] == UINT8_C(0xcd));
    CHECK(ble.payload[36] == UINT8_C(0xab));
    CHECK(ble.payload[37] == UINT8_C(0x89));
    CHECK(ble.payload[38] == (uint8_t)(source.source_age_ms & UINT16_C(0xff)));
    CHECK(ble.payload[39] == (uint8_t)(source.source_age_ms >> 8U));
}

int main(void)
{
    verify_network_to_esp32((uint8_t)UART_M_QUALITY_OK);
    verify_network_to_esp32((uint8_t)UART_M_QUALITY_STALE);
    verify_network_to_esp32((uint8_t)UART_M_QUALITY_INVALID);
    verify_network_to_esp32((uint8_t)UART_M_QUALITY_NO_DATA);

    if (failures != 0) {
        (void)fprintf(stderr, "%d vehicle-state interop assertion(s) failed\n",
                      failures);
        return 1;
    }
    (void)puts("vehicle-state Network encoder -> ESP32 mapper interop PASS");
    return 0;
}
