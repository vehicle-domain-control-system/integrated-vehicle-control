#include "gateway_adapter.h"
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "mock_idf.h"
#include "freertos/queue.h"
#include "uart_m_frame.h"
#include "uart_m_transport_engine.h"

#define CHECK(c) do { if (!(c)) { printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #c); return false; } } while (0)

static uint8_t s_status[BLE_GATT_GATEWAY_STATUS_LENGTH];
static esp_err_t s_notify_result;
static esp_err_t s_vehicle_notify_result;
static unsigned s_gateway_notify_calls;
static unsigned s_vehicle_notify_calls;
static uint8_t s_vehicle_type;
static uint8_t s_vehicle_payload[BLE_GATT_MAX_DOWNLINK_PAYLOAD];
static size_t s_vehicle_payload_length;
static ble_gatt_send_mode_t s_vehicle_mode;
static ble_connection_state_t s_ble_connection_state;
static bool s_snapshot_error;
static bool s_link_secure;
static p6_registration_record_t s_registration_record;
static p6_store_read_result_t s_registration_load_result;
static p6_store_commit_result_t s_registration_save_result;
static unsigned s_registration_save_calls;
static unsigned s_context_send_calls;
static unsigned s_non_context_send_calls;
static uart_m_transport_status_t s_context_send_result;
static uart_m_message_t s_context_sent;
static int s_context_transport_token;
static unsigned s_context_cancel_calls;

uart_m_transport_status_t uart_m_transport_invalidate_context(
    uart_m_transport_t *transport, bool *submission_in_progress)
{
    if (transport == NULL || submission_in_progress == NULL) return UART_M_TRANSPORT_INVALID_ARGUMENT;
    s_context_cancel_calls++;
    *submission_in_progress = false;
    return UART_M_TRANSPORT_OK;
}

uart_m_transport_status_t uart_m_transport_send_context(
    uart_m_transport_t *transport, const uart_m_message_t *message,
    const uart_m_context_tx_limits_t *limits)
{
    if (limits == NULL || limits->context_ttl_ms == 0U) return UART_M_TRANSPORT_INVALID_ARGUMENT;
    return uart_m_transport_send(transport, message, 0U);
}

/* Transport queue boundary mock. Real Transport deep-copy/driver/sequence
 * behavior is covered in tests/uart_transport, not simulated as board PASS. */
uart_m_transport_status_t uart_m_transport_send(uart_m_transport_t *transport,
    const uart_m_message_t *message, uint32_t queue_wait_ms)
{
    if (transport != (uart_m_transport_t *)&s_context_transport_token ||
        message == NULL || queue_wait_ms != 0U) {
        return UART_M_TRANSPORT_INVALID_ARGUMENT;
    }
    if (message->type != UART_M_TYPE_CONTEXT) {
        s_non_context_send_calls++;
        return UART_M_TRANSPORT_INVALID_ARGUMENT;
    }
    s_context_send_calls++;
    s_context_sent = *message;
    return s_context_send_result;
}

static p6_store_read_result_t registration_load(
    void *context, p6_registration_record_t *out_record)
{
    (void)context;
    if (s_registration_load_result == P6_STORE_READ_OK) {
        *out_record = s_registration_record;
    }
    return s_registration_load_result;
}

static p6_store_commit_result_t registration_save(
    void *context, const p6_registration_record_t *record)
{
    (void)context;
    s_registration_save_calls++;
    if (s_registration_save_result == P6_STORE_COMMITTED) {
        s_registration_record = *record;
        s_registration_load_result = P6_STORE_READ_OK;
    }
    return s_registration_save_result;
}

static p6_store_commit_result_t registration_erase(void *context)
{
    (void)context;
    return P6_STORE_NOT_COMMITTED;
}

static bool test_entropy_u32(void *context, uint32_t *out_value)
{
    (void)context;
    *out_value = UINT32_C(0xA5A50001);
    return true;
}

static bool test_entropy_u64(void *context, uint64_t *out_value)
{
    (void)context;
    *out_value = UINT64_C(0xA5A5000000000001);
    return true;
}

const char *esp_err_to_name(esp_err_t error) { (void)error; return "mock"; }
esp_err_t ble_gatt_server_set_gateway_status(const uint8_t status[BLE_GATT_GATEWAY_STATUS_LENGTH])
{ memcpy(s_status, status, sizeof(s_status)); return ESP_OK; }
esp_err_t ble_gatt_server_send_gateway_status(ble_gatt_send_mode_t mode)
{
    if (mode != BLE_GATT_SEND_NOTIFY) return ESP_FAIL;
    s_gateway_notify_calls++;
    return s_notify_result;
}
esp_err_t ble_gatt_server_send_vehicle_data(uint8_t type, const uint8_t *payload,
                                             size_t length, ble_gatt_send_mode_t mode)
{
    if (payload == NULL || length > sizeof(s_vehicle_payload)) return ESP_ERR_INVALID_ARG;
    s_vehicle_notify_calls++;
    s_vehicle_type = type;
    s_vehicle_payload_length = length;
    s_vehicle_mode = mode;
    memcpy(s_vehicle_payload, payload, length);
    return s_vehicle_notify_result;
}
esp_err_t ble_gatt_server_get_connection_snapshot(
    ble_connection_snapshot_t *out_snapshot)
{
    if (out_snapshot == NULL) return ESP_ERR_INVALID_ARG;
    if (s_snapshot_error) return ESP_FAIL;
    ble_connection_state_snapshot(&s_ble_connection_state, out_snapshot);
    return ESP_OK;
}
bool ble_gatt_server_is_secure(void) { return s_link_secure; }

static void reset_ble_snapshot(void)
{
    ble_connection_state_init(&s_ble_connection_state);
    ble_connection_state_set_available(&s_ble_connection_state, true);
    s_snapshot_error = false;
    s_link_secure = false;
}

static gateway_adapter_t *start_adapter(size_t depth)
{
    gateway_adapter_t *adapter = NULL;
    gateway_adapter_config_t config = gateway_adapter_safe_config();
    config.queue_depth = depth;
    memset(s_status, 0, sizeof(s_status));
    s_notify_result = ESP_OK;
    s_vehicle_notify_result = ESP_OK;
    s_gateway_notify_calls = 0U;
    s_vehicle_notify_calls = 0U;
    s_vehicle_type = 0U;
    memset(s_vehicle_payload, 0, sizeof(s_vehicle_payload));
    s_vehicle_payload_length = 0U;
    s_vehicle_mode = BLE_GATT_SEND_NOTIFY;
    s_registration_load_result = P6_STORE_READ_EMPTY;
    s_registration_save_result = P6_STORE_NOT_COMMITTED;
    s_registration_save_calls = 0U;
    return gateway_adapter_start(&adapter, &config) == ESP_OK ? adapter : NULL;
}

#if CONFIG_P6_DEV_REGISTRATION_APPROVAL
static gateway_adapter_t *start_registration_adapter(
    size_t depth, p6_store_commit_result_t save_result)
{
    static const p6_registration_store_t store = {
        .load = registration_load,
        .save = registration_save,
        .erase = registration_erase,
        .context = NULL,
    };
    static const p6_entropy_source_t entropy = {
        .next_u32 = test_entropy_u32,
        .next_u64 = test_entropy_u64,
        .context = NULL,
    };
    gateway_adapter_t *adapter = NULL;
    gateway_adapter_config_t config = gateway_adapter_safe_config();

    memset(&s_registration_record, 0, sizeof(s_registration_record));
    s_registration_load_result = P6_STORE_READ_EMPTY;
    s_registration_save_result = save_result;
    s_registration_save_calls = 0U;
    config.queue_depth = depth;
    config.registration_store = &store;
    config.entropy_source = &entropy;
    return gateway_adapter_start(&adapter, &config) == ESP_OK ? adapter : NULL;
}
#endif

static gateway_adapter_t *start_adapter_with_registration(size_t depth,
                                                          uint8_t seed)
{
    static const p6_registration_store_t store = {
        .load = registration_load,
        .save = registration_save,
        .erase = registration_erase,
        .context = NULL,
    };
    static const p6_entropy_source_t entropy = {
        .next_u32 = test_entropy_u32,
        .next_u64 = test_entropy_u64,
        .context = NULL,
    };
    gateway_adapter_t *adapter = NULL;
    gateway_adapter_config_t config = gateway_adapter_safe_config();
    size_t index;

    memset(&s_registration_record, 0, sizeof(s_registration_record));
    s_registration_load_result = P6_STORE_READ_OK;
    s_registration_save_result = P6_STORE_NOT_COMMITTED;
    s_registration_save_calls = 0U;
    s_registration_record.schema_version = P6_REGISTRATION_SCHEMA_VERSION;
    s_registration_record.state = P6_REGISTRATION_RECORD_ACTIVE;
    s_registration_record.bond_identity.address_type = 1U;
    for (index = 0U; index < P6_BOND_IDENTITY_BYTES; ++index) {
        s_registration_record.bond_identity.bytes[index] =
            (uint8_t)(seed + index);
    }
    s_registration_record.device_context_id = UINT32_C(0x11223344);
    s_registration_record.registration_epoch = UINT32_C(0x55667788);
    config.queue_depth = depth;
    config.registration_store = &store;
    config.entropy_source = &entropy;
    return gateway_adapter_start(&adapter, &config) == ESP_OK ? adapter : NULL;
}

static ble_gatt_server_event_t ble_event(ble_gatt_event_type_t type,
                                         uint16_t handle, uint32_t generation)
{
    ble_gatt_server_event_t event;
    memset(&event, 0, sizeof(event));
    event.type = type;
    event.connection_handle = handle;
    event.connection_generation = generation;
    return event;
}

static void add_bonded_identity(ble_gatt_server_event_t *event, uint8_t seed,
                                bool ota_rpa)
{
    size_t index;

    event->peer_identity.identity_resolved = true;
    event->peer_identity.ota_address_is_rpa = ota_rpa;
    event->peer_identity.bond_store_present = true;
    event->peer_identity.address_type = 1U;
    for (index = 0U; index < sizeof(event->peer_identity.address); ++index) {
        event->peer_identity.address[index] = (uint8_t)(seed + index);
    }
}

static bool reconcile_registered_bond(gateway_adapter_t *adapter,
                                      uint8_t seed, uint64_t now_ms)
{
    ble_gatt_server_event_t event =
        ble_event(BLE_GATT_EVENT_BOND_STORE_READY, UINT16_MAX, 0U);
    size_t index;

    event.data.bond_store.count = 1U;
    event.data.bond_store.peers[0].identity_resolved = true;
    event.data.bond_store.peers[0].bond_store_present = true;
    event.data.bond_store.peers[0].address_type = 1U;
    for (index = 0U; index < P6_BOND_IDENTITY_BYTES; ++index) {
        event.data.bond_store.peers[0].address[index] =
            (uint8_t)(seed + index);
    }
    gateway_adapter_on_ble_event(&event, adapter);
    return gateway_adapter_host_process_one(adapter, now_ms);
}

static bool process_ble_event(gateway_adapter_t *adapter,
                              ble_gatt_server_event_t *event,
                              uint64_t now_ms)
{
    ble_connection_snapshot_t snapshot;

    if (event->type == BLE_GATT_EVENT_CONNECTED) {
        CHECK(ble_connection_state_connect(&s_ble_connection_state,
                                            event->connection_handle));
        ble_connection_state_snapshot(&s_ble_connection_state, &snapshot);
        event->connection_generation = snapshot.connection_generation;
        s_link_secure = false;
    } else if (event->type == BLE_GATT_EVENT_DISCONNECTED) {
        ble_connection_state_snapshot(&s_ble_connection_state, &snapshot);
        event->connection_generation = snapshot.connection_generation;
        CHECK(ble_connection_state_disconnect(&s_ble_connection_state,
                                               event->connection_handle));
        s_link_secure = false;
    } else if (event->type == BLE_GATT_EVENT_SECURITY_CHANGED) {
        s_link_secure = event->data.security.encrypted &&
                        event->data.security.bonded;
    }
    gateway_adapter_on_ble_event(event, adapter);
    return gateway_adapter_host_process_one(adapter, now_ms);
}

static void make_alive(uart_m_transport_rx_message_t *rx, uint32_t boot_id)
{
    memset(rx, 0, sizeof(*rx));
    rx->message.type = UART_M_TYPE_DOMAIN_ALIVE;
    rx->message.body.domain_alive.domain_boot_id = boot_id;
    rx->message.body.domain_alive.update_sequence = 10U;
    rx->message.body.domain_alive.source_age_ms = 1U;
    rx->message.body.domain_alive.domain_state = UART_M_CONTROLLER_READY;
    rx->message.body.domain_alive.quality = UART_M_QUALITY_OK;
    rx->received_at_us = INT64_C(5000000000000);
}

static void make_result(uart_m_transport_rx_message_t *rx,
                        uint32_t device_context_id,
                        uint64_t receive_session,
                        uint32_t domain_boot_id,
                        uint64_t request_session,
                        uint32_t request_id)
{
    uart_m_result_t *result;

    memset(rx, 0, sizeof(*rx));
    rx->message.type = UART_M_TYPE_RESULT;
    result = &rx->message.body.result;
    result->context.recipient.device_context_id = device_context_id;
    result->context.recipient.session_id = receive_session;
    result->context.domain_boot_id = domain_boot_id;
    result->context.query_id = 0U;
    result->request_session = request_session;
    result->request_id = request_id;
    result->result = UART_M_RESULT_ACCEPTED;
    result->result_reason = 0U;
    result->confirmation = UART_M_UNCONFIRMED;
    result->command_target = UART_M_COMMAND_TARGET_DOMAIN_SETTING;
    result->command_id = request_id;
    result->origin = UART_M_ORIGIN_MOBILE_MANUAL;
    result->result_age_ms = 5U;
}

static void make_query_end(uart_m_transport_rx_message_t *rx,
                           uint32_t device_context_id,
                           uint64_t session_id,
                           uint32_t domain_boot_id,
                           uint32_t query_id,
                           uart_m_query_scope_t scope,
                           uart_m_query_status_t status,
                           uint16_t item_count)
{
    uart_m_query_end_t *end;

    memset(rx, 0, sizeof(*rx));
    rx->message.type = UART_M_TYPE_QUERY_END;
    end = &rx->message.body.query_end;
    end->context.recipient.device_context_id = device_context_id;
    end->context.recipient.session_id = session_id;
    end->context.domain_boot_id = domain_boot_id;
    end->context.query_id = query_id;
    end->scope = scope;
    end->status = status;
    end->item_count = item_count;
}

static void make_door_state(uart_m_transport_rx_message_t *rx,
                            uint32_t device_context_id,
                            uint64_t session_id,
                            uint32_t domain_boot_id,
                            uart_m_value_quality_t quality)
{
    uart_m_bcm_door_state_t *door;

    memset(rx, 0, sizeof(*rx));
    rx->message.type = UART_M_TYPE_BCM_DOOR_STATE;
    door = &rx->message.body.bcm_door_state;
    door->context.recipient.device_context_id = device_context_id;
    door->context.recipient.session_id = session_id;
    door->context.domain_boot_id = domain_boot_id;
    door->context.query_id = 0U;
    door->source.boot_id = UINT32_C(0x10203040);
    door->source.tx_sequence = UINT32_C(0x11223344);
    door->lock_state = UART_M_DOOR_LOCKED;
    door->open_state = UART_M_DOOR_CLOSED;
    door->composite_state = UART_M_DOOR_NORMAL;
    door->lock_quality = quality;
    door->open_quality = quality;
    door->composite_quality = quality;
    door->update_sequence = UINT32_C(0x55667788);
    door->source_age_ms = (quality == UART_M_QUALITY_OK) ? 25U : UINT16_MAX;
}

static bool establish_vehicle_data_link(gateway_adapter_t *adapter,
                                        uint16_t handle,
                                        bool subscribe,
                                        uint16_t mtu,
                                        uint64_t now_ms,
                                        uint32_t *out_generation)
{
    ble_gatt_server_event_t connected =
        ble_event(BLE_GATT_EVENT_CONNECTED, handle, 0U);
    ble_gatt_server_event_t security =
        ble_event(BLE_GATT_EVENT_SECURITY_CHANGED, handle, 0U);
    ble_gatt_server_event_t subscription =
        ble_event(BLE_GATT_EVENT_SUBSCRIPTION_CHANGED, handle, 0U);
    ble_gatt_server_event_t mtu_event =
        ble_event(BLE_GATT_EVENT_MTU_CHANGED, handle, 0U);
    ble_connection_snapshot_t snapshot;

    add_bonded_identity(&connected, 0x70U, true);
    if (!process_ble_event(adapter, &connected, now_ms)) {
        return false;
    }
    ble_connection_state_snapshot(&s_ble_connection_state, &snapshot);
    security.connection_generation = snapshot.connection_generation;
    add_bonded_identity(&security, 0x70U, true);
    security.data.security.encrypted = true;
    security.data.security.bonded = true;
    security.data.security.authenticated = false;
    if (!process_ble_event(adapter, &security, now_ms + 1U)) {
        return false;
    }
    if (subscribe) {
        subscription.connection_generation = snapshot.connection_generation;
        subscription.data.subscription.vehicle_data_notify = true;
        if (!process_ble_event(adapter, &subscription, now_ms + 2U)) {
            return false;
        }
    }
    mtu_event.connection_generation = snapshot.connection_generation;
    mtu_event.data.mtu.mtu = mtu;
    if (!process_ble_event(adapter, &mtu_event, now_ms + 3U)) {
        return false;
    }
    *out_generation = snapshot.connection_generation;
    return true;
}

static bool install_result_context(gateway_adapter_t *adapter,
                                   uint32_t generation,
                                   uint32_t device_context_id,
                                   uint64_t session_id,
                                   uint32_t domain_boot_id,
                                   uint64_t now_ms)
{
    gateway_core_context_t context;
    uart_m_transport_rx_message_t alive;

    memset(&context, 0, sizeof(context));
    context.identity.device_context_id = device_context_id;
    context.identity.session_id = session_id;
    context.identity.verified = true;
    context.connection_generation = generation;
    context.connected = true;
    context.encrypted = true;
    context.authenticated = false;
    context.registered = true;
    context.session_ready = true;
    context.authorization_verified = true;
    context.observed_at_ms = now_ms;
    if (!gateway_adapter_host_set_core_context(adapter, &context, now_ms)) {
        return false;
    }
    make_alive(&alive, domain_boot_id);
    gateway_adapter_on_uart_message(&alive, adapter);
    return gateway_adapter_host_process_one(adapter, now_ms + 1U);
}

typedef struct {
    gateway_adapter_t *adapter;
    size_t delivered;
} uart_gateway_bridge_t;

static bool deliver_uart_to_gateway(
    const uart_m_transport_rx_message_t *message,
    void *context)
{
    uart_gateway_bridge_t *bridge = (uart_gateway_bridge_t *)context;

    gateway_adapter_on_uart_message(message, bridge->adapter);
    bridge->delivered++;
    return true;
}

static bool encode_rx_frame(const uart_m_message_t *message,
                            uint16_t link_sequence,
                            uint8_t *frame,
                            size_t *frame_length)
{
    uint8_t payload[UART_M_MAX_PAYLOAD_SIZE];
    size_t payload_length = 0U;

    return uart_m_codec_encode(message, payload, sizeof(payload),
                               &payload_length) == UART_M_CODEC_OK &&
           uart_m_frame_encode((uint8_t)message->type, link_sequence,
                               payload, payload_length, frame,
                               UART_M_MAX_FRAME_SIZE, frame_length) ==
               UART_M_FRAME_STATUS_OK;
}

static uint32_t read_u32_le(const uint8_t *bytes)
{
    return (uint32_t)bytes[0] |
           ((uint32_t)bytes[1] << 8U) |
           ((uint32_t)bytes[2] << 16U) |
           ((uint32_t)bytes[3] << 24U);
}

static bool test_uart_frame_stream_to_gateway_task(void)
{
    gateway_adapter_t *adapter = start_adapter(8U);
    uart_gateway_bridge_t bridge = {.adapter = adapter};
    uart_m_transport_engine_t engine;
    uart_m_transport_rx_message_t alive;
    uart_m_transport_feed_result_t result;
    gateway_adapter_stats_t stats;
    uint8_t frame[UART_M_MAX_FRAME_SIZE];
    uint8_t damaged[UART_M_MAX_FRAME_SIZE];
    uint8_t joined[UART_M_MAX_FRAME_SIZE * 2U];
    size_t frame_length;
    size_t index;

    CHECK(adapter != NULL);
    CHECK(uart_m_transport_engine_init(&engine, 0U,
                                       deliver_uart_to_gateway, &bridge) ==
          UART_M_TRANSPORT_OK);
    make_alive(&alive, UINT32_C(0x11223344));
    alive.received_at_us = 1000;
    CHECK(encode_rx_frame(&alive.message, 7U, frame, &frame_length));
    for (index = 0U; index < frame_length; ++index) {
        CHECK(uart_m_transport_engine_feed(&engine, &frame[index], 1U, 1000,
                                            &result) == UART_M_TRANSPORT_OK);
    }
    CHECK(bridge.delivered == 1U);
    CHECK(gateway_adapter_host_process_one(adapter, 1U));
    gateway_adapter_get_stats(adapter, &stats);
    CHECK(stats.uart_events_enqueued == 1U);
    CHECK(stats.uart_messages_processed == 1U);
    CHECK(stats.uart_messages_rejected == 0U);
    CHECK(stats.uart_domain_alive_accepted == 1U);
    CHECK(s_status[2] == 1U);
    CHECK(read_u32_le(&s_status[16]) == UINT32_C(0x11223344));

    memcpy(damaged, frame, frame_length);
    damaged[frame_length - 1U] ^= UINT8_C(0x80);
    memcpy(joined, damaged, frame_length);
    memcpy(&joined[frame_length], frame, frame_length);
    CHECK(uart_m_transport_engine_feed(&engine, joined, frame_length * 2U,
                                        2000, &result) == UART_M_TRANSPORT_OK);
    CHECK(result.crc_errors == 1U);
    CHECK(result.messages_accepted == 1U);
    CHECK(bridge.delivered == 2U);
    CHECK(gateway_adapter_host_process_one(adapter, 2U));

    make_alive(&alive, UINT32_C(0x55667788));
    alive.message.body.domain_alive.source_age_ms = 751U;
    alive.received_at_us = 3000;
    gateway_adapter_on_uart_message(&alive, adapter);
    CHECK(gateway_adapter_host_process_one(adapter, 3U));
    gateway_adapter_get_stats(adapter, &stats);
    CHECK(stats.uart_messages_processed == 3U);
    CHECK(stats.uart_messages_rejected == 1U);
    CHECK(stats.uart_domain_alive_accepted == 2U);
    CHECK(stats.domain_boot_changes == 0U);
    CHECK(s_status[2] == 0U);
    CHECK(read_u32_le(&s_status[16]) == UINT32_C(0x11223344));
    CHECK(stats.production_uart_tx_count == 0U);
    CHECK(stats.production_m_context_tx_count == 0U);
    CHECK(stats.digital_key_auto_unlock_count == 0U);
    CHECK(gateway_adapter_is_safe_mode(adapter));
    CHECK(gateway_adapter_stop(&adapter) == ESP_OK);
    return true;
}

static bool test_domain_phase5_m19a_golden_stream_to_diagnostics(void)
{
    static const uint8_t golden[] = {
        0xA5, 0x5A, 0x02, 0x02, 0x0C, 0x00, 0x01, 0x00,
        0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0xFF, 0xFF, 0x00, 0x03, 0xCC, 0xC9,
    };
    gateway_adapter_t *adapter = start_adapter(4U);
    uart_gateway_bridge_t bridge = {.adapter = adapter};
    uart_m_transport_engine_t engine;
    uart_m_transport_feed_result_t result;
    gateway_adapter_stats_t stats;
    gateway_adapter_uart_rx_diagnostics_t diagnostics;
    uint8_t damaged[sizeof(golden)];
    size_t index;

    CHECK(adapter != NULL);
    CHECK(uart_m_transport_engine_init(&engine, 0U,
                                       deliver_uart_to_gateway, &bridge) ==
          UART_M_TRANSPORT_OK);

    CHECK(uart_m_transport_engine_feed(&engine, golden, sizeof(golden),
                                        1000, &result) == UART_M_TRANSPORT_OK);
    CHECK(result.frames_valid == 1U);
    CHECK(result.version_errors == 0U);
    CHECK(result.length_errors == 0U);
    CHECK(result.crc_errors == 0U);
    CHECK(result.codec_errors == 0U);
    CHECK(result.direction_errors == 0U);
    CHECK(result.messages_accepted == 1U);
    CHECK(bridge.delivered == 1U);
    CHECK(gateway_adapter_host_process_one(adapter, 1U));

    gateway_adapter_get_stats(adapter, &stats);
    gateway_adapter_get_uart_rx_diagnostics(adapter, &diagnostics);
    CHECK(stats.uart_events_enqueued == 1U);
    CHECK(stats.uart_messages_processed == 1U);
    CHECK(stats.uart_messages_rejected == 1U);
    CHECK(diagnostics.uart_domain_alive_decoded == 1U);
    CHECK(diagnostics.uart_domain_alive_accepted == 0U);
    CHECK(diagnostics.uart_last_link_sequence == 1U);
    CHECK(diagnostics.uart_last_domain_boot_id == 1U);
    CHECK(diagnostics.uart_last_update_sequence == 0U);
    CHECK(diagnostics.uart_last_source_age_ms == UINT16_MAX);
    CHECK(diagnostics.uart_last_domain_state == UART_M_CONTROLLER_INIT);
    CHECK(diagnostics.uart_last_quality == UART_M_QUALITY_NO_DATA);
    CHECK(!diagnostics.uart_domain_link_up);
    CHECK(s_status[2] == 0U);
    CHECK(s_status[3] == 0U);

    for (index = 0U; index < sizeof(golden); ++index) {
        CHECK(uart_m_transport_engine_feed(&engine, &golden[index], 1U,
                                            2000 + (int64_t)index,
                                            &result) == UART_M_TRANSPORT_OK);
    }
    CHECK(result.frames_valid == 1U);
    CHECK(result.version_errors == 0U);
    CHECK(result.length_errors == 0U);
    CHECK(result.crc_errors == 0U);
    CHECK(result.codec_errors == 0U);
    CHECK(result.direction_errors == 0U);
    CHECK(result.messages_accepted == 1U);
    CHECK(bridge.delivered == 2U);
    CHECK(gateway_adapter_host_process_one(adapter, 2U));
    gateway_adapter_get_uart_rx_diagnostics(adapter, &diagnostics);
    CHECK(diagnostics.uart_domain_alive_decoded == 2U);
    CHECK(diagnostics.uart_domain_alive_accepted == 0U);
    CHECK(!diagnostics.uart_domain_link_up);
    CHECK(s_status[2] == 0U);
    CHECK(s_status[3] == 0U);

    memcpy(damaged, golden, sizeof(damaged));
    damaged[sizeof(damaged) - 1U] ^= UINT8_C(0x01);
    CHECK(uart_m_transport_engine_feed(&engine, damaged, sizeof(damaged),
                                        3000, &result) == UART_M_TRANSPORT_OK);
    CHECK(result.frames_valid == 0U);
    CHECK(result.crc_errors == 1U);
    CHECK(result.messages_accepted == 0U);
    CHECK(bridge.delivered == 2U);
    gateway_adapter_get_uart_rx_diagnostics(adapter, &diagnostics);
    CHECK(diagnostics.uart_domain_alive_decoded == 2U);
    CHECK(diagnostics.uart_domain_alive_accepted == 0U);
    CHECK(!diagnostics.uart_domain_link_up);
    CHECK(s_status[3] == 0U);
    gateway_adapter_get_stats(adapter, &stats);
    CHECK(stats.production_uart_tx_count == 0U);
    CHECK(stats.production_m_context_tx_count == 0U);
    CHECK(stats.digital_key_auto_unlock_count == 0U);
    CHECK(gateway_adapter_stop(&adapter) == ESP_OK);
    return true;
}

static bool test_uart_gateway_queue_full_and_invalid_type(void)
{
    gateway_adapter_t *adapter = start_adapter(1U);
    uart_m_transport_rx_message_t first;
    uart_m_transport_rx_message_t second;
    gateway_adapter_stats_t stats;

    CHECK(adapter != NULL);
    make_alive(&first, 1U);
    make_alive(&second, 2U);
    gateway_adapter_on_uart_message(&first, adapter);
    gateway_adapter_on_uart_message(&second, adapter);
    CHECK(gateway_adapter_host_process_one(adapter,
                                           UINT64_C(5000000000)));
    gateway_adapter_get_stats(adapter, &stats);
    CHECK(stats.uart_events_enqueued == 1U);
    CHECK(stats.uart_queue_full == 1U);
    CHECK(stats.queue_full == 1U);
    CHECK(stats.uart_domain_alive_accepted == 1U);

    memset(&first, 0, sizeof(first));
    first.message.type = (uart_m_message_type_t)0x7FU;
    first.received_at_us = INT64_C(5000000001000);
    gateway_adapter_on_uart_message(&first, adapter);
    CHECK(gateway_adapter_host_process_one(adapter,
                                           UINT64_C(5000000001)));
    gateway_adapter_get_stats(adapter, &stats);
    CHECK(stats.uart_messages_processed == 2U);
    CHECK(stats.uart_messages_rejected == 1U);
    CHECK(stats.production_uart_tx_count == 0U);
    CHECK(gateway_adapter_stop(&adapter) == ESP_OK);
    return true;
}

static bool test_domain_link_timeout_and_recovery(void)
{
    gateway_adapter_t *adapter = start_adapter(4U);
    uart_m_transport_rx_message_t alive;
    gateway_adapter_stats_t stats;
    const uint64_t first_ms = UINT64_C(5000000000);

    CHECK(adapter != NULL);
    make_alive(&alive, UINT32_C(0x11223344));
    alive.received_at_us = (int64_t)(first_ms * UINT64_C(1000));
    gateway_adapter_on_uart_message(&alive, adapter);
    CHECK(gateway_adapter_host_process_one(adapter, first_ms));
    CHECK(s_status[2] == 1U);

    CHECK(gateway_adapter_host_tick(adapter, first_ms + UINT64_C(751)));
    CHECK(s_status[2] == 0U);

    alive.received_at_us = (int64_t)((first_ms + UINT64_C(752)) *
                                     UINT64_C(1000));
    gateway_adapter_on_uart_message(&alive, adapter);
    CHECK(gateway_adapter_host_process_one(adapter, first_ms + UINT64_C(752)));
    CHECK(s_status[2] == 1U);
    gateway_adapter_get_stats(adapter, &stats);
    CHECK(stats.uart_domain_alive_accepted == 2U);
    CHECK(stats.production_uart_tx_count == 0U);
    CHECK(stats.production_m_context_tx_count == 0U);
    CHECK(stats.digital_key_auto_unlock_count == 0U);
    CHECK(gateway_adapter_stop(&adapter) == ESP_OK);
    return true;
}

static bool test_domain_boot_change_invalidates_p6_and_pending_result(void)
{
    const uint64_t now_ms = UINT64_C(5000000000);
    gateway_adapter_t *adapter = start_adapter_with_registration(16U, 0x70U);
    uart_m_transport_rx_message_t result;
    uart_m_transport_rx_message_t changed_alive;
    uart_m_transport_engine_t engine;
    uart_m_transport_feed_result_t feed_result;
    uart_gateway_bridge_t bridge = {.adapter = adapter};
    gateway_adapter_stats_t stats;
    uart_m_context_t context;
    uint8_t frame[UART_M_MAX_FRAME_SIZE];
    size_t frame_length;
    uint32_t generation;

    CHECK(adapter != NULL);
    CHECK(reconcile_registered_bond(adapter, 0x70U, now_ms));
    CHECK(establish_vehicle_data_link(adapter, 61U, false, 23U,
                                      now_ms + 1U, &generation));
    CHECK(install_result_context(adapter, generation, UINT32_C(0x10203040),
                                 UINT64_C(0x1122334455667788),
                                 UINT32_C(0x01020304), now_ms + 5U));
    CHECK(gateway_adapter_host_get_context_candidate(adapter, &context));

    make_result(&result, UINT32_C(0x10203040),
                UINT64_C(0x1122334455667788), UINT32_C(0x01020304),
                UINT64_C(0x8877665544332211), 1U);
    result.received_at_us =
        (int64_t)((now_ms + UINT64_C(7)) * UINT64_C(1000));
    gateway_adapter_on_uart_message(&result, adapter);
    CHECK(gateway_adapter_host_process_one(adapter, now_ms + 7U));
    gateway_adapter_get_stats(adapter, &stats);
    CHECK(stats.p6_authorization_verified);
    CHECK(stats.vehicle_data_pending_count == 1U);

    make_alive(&changed_alive, UINT32_C(0x05060708));
    changed_alive.received_at_us =
        (int64_t)((now_ms + UINT64_C(8)) * UINT64_C(1000));
    CHECK(encode_rx_frame(&changed_alive.message, 2U, frame, &frame_length));
    frame[frame_length - 1U] ^= UINT8_C(0x01);
    CHECK(uart_m_transport_engine_init(&engine, 0U,
                                       deliver_uart_to_gateway, &bridge) ==
          UART_M_TRANSPORT_OK);
    CHECK(uart_m_transport_engine_feed(&engine, frame, frame_length,
                                        changed_alive.received_at_us,
                                        &feed_result) == UART_M_TRANSPORT_OK);
    CHECK(feed_result.crc_errors == 1U);
    CHECK(feed_result.messages_accepted == 0U);
    CHECK(bridge.delivered == 0U);
    gateway_adapter_get_stats(adapter, &stats);
    CHECK(stats.p6_authorization_verified);
    CHECK(stats.vehicle_data_pending_count == 1U);
    CHECK(gateway_adapter_host_get_context_candidate(adapter, &context));

    gateway_adapter_on_uart_message(&changed_alive, adapter);
    CHECK(gateway_adapter_host_process_one(adapter, now_ms + 8U));
    gateway_adapter_get_stats(adapter, &stats);
    CHECK(!stats.p6_authorization_verified);
    CHECK(stats.domain_boot_changes == 1U);
    CHECK(stats.vehicle_data_pending_count == 0U);
    CHECK(stats.vehicle_data_candidates_invalidated == 1U);
    CHECK(!gateway_adapter_host_get_context_candidate(adapter, &context));

    gateway_adapter_on_uart_message(&changed_alive, adapter);
    CHECK(gateway_adapter_host_process_one(adapter, now_ms + 9U));
    gateway_adapter_get_stats(adapter, &stats);
    CHECK(!stats.p6_authorization_verified);
    CHECK(stats.production_uart_tx_count == 0U);
    CHECK(stats.production_m_context_tx_count == 0U);
    CHECK(stats.digital_key_auto_unlock_count == 0U);
    CHECK(gateway_adapter_stop(&adapter) == ESP_OK);
    return true;
}

static bool test_domain_link_down_invalidates_p6_without_recovery(void)
{
    const uint64_t now_ms = UINT64_C(5000000000);
    gateway_adapter_t *adapter = start_adapter_with_registration(16U, 0x70U);
    uart_m_transport_rx_message_t result;
    uart_m_transport_rx_message_t alive;
    gateway_adapter_stats_t stats;
    gateway_adapter_uart_rx_diagnostics_t diagnostics;
    uart_m_context_t context;
    uint32_t generation;

    CHECK(adapter != NULL);
    CHECK(reconcile_registered_bond(adapter, 0x70U, now_ms));
    CHECK(establish_vehicle_data_link(adapter, 62U, false, 23U,
                                      now_ms + 1U, &generation));
    CHECK(install_result_context(adapter, generation, UINT32_C(0x20304050),
                                 UINT64_C(0x2233445566778899),
                                 UINT32_C(0x11121314), now_ms + 5U));
    CHECK(gateway_adapter_host_get_context_candidate(adapter, &context));

    make_result(&result, UINT32_C(0x20304050),
                UINT64_C(0x2233445566778899), UINT32_C(0x11121314),
                UINT64_C(0x9988776655443322), 2U);
    result.received_at_us =
        (int64_t)((now_ms + UINT64_C(7)) * UINT64_C(1000));
    gateway_adapter_on_uart_message(&result, adapter);
    CHECK(gateway_adapter_host_process_one(adapter, now_ms + 7U));
    gateway_adapter_get_stats(adapter, &stats);
    CHECK(stats.p6_authorization_verified);
    CHECK(stats.vehicle_data_pending_count == 1U);

    CHECK(gateway_adapter_host_tick(adapter, now_ms + UINT64_C(757)));
    gateway_adapter_get_stats(adapter, &stats);
    gateway_adapter_get_uart_rx_diagnostics(adapter, &diagnostics);
    CHECK(!stats.p6_authorization_verified);
    CHECK(!diagnostics.uart_domain_link_up);
    CHECK(stats.vehicle_data_pending_count == 0U);
    CHECK(stats.vehicle_data_candidates_invalidated == 1U);
    CHECK(!gateway_adapter_host_get_context_candidate(adapter, &context));

    make_alive(&alive, UINT32_C(0x11121314));
    alive.received_at_us =
        (int64_t)((now_ms + UINT64_C(758)) * UINT64_C(1000));
    gateway_adapter_on_uart_message(&alive, adapter);
    CHECK(gateway_adapter_host_process_one(adapter, now_ms + UINT64_C(758)));
    gateway_adapter_get_stats(adapter, &stats);
    gateway_adapter_get_uart_rx_diagnostics(adapter, &diagnostics);
    CHECK(diagnostics.uart_domain_link_up);
    CHECK(!stats.p6_authorization_verified);
    CHECK(!gateway_adapter_host_get_context_candidate(adapter, &context));
    CHECK(stats.production_uart_tx_count == 0U);
    CHECK(stats.production_m_context_tx_count == 0U);
    CHECK(stats.digital_key_auto_unlock_count == 0U);
    CHECK(gateway_adapter_stop(&adapter) == ESP_OK);
    return true;
}

static bool test_alive_diagnostics_keep_decode_and_supervisor_separate(void)
{
    gateway_adapter_t *adapter = start_adapter(4U);
    uart_m_transport_rx_message_t alive;
    gateway_adapter_stats_t stats;
    gateway_adapter_uart_rx_diagnostics_t diagnostics;

    CHECK(adapter != NULL);
    make_alive(&alive, UINT32_C(0x00000001));
    alive.link_sequence = UINT16_C(0xFFFE);
    alive.message.body.domain_alive.update_sequence = UINT32_C(7);
    alive.message.body.domain_alive.source_age_ms = UINT16_MAX;
    alive.message.body.domain_alive.domain_state = UART_M_CONTROLLER_INIT;
    alive.message.body.domain_alive.quality = UART_M_QUALITY_NO_DATA;
    gateway_adapter_on_uart_message(&alive, adapter);
    CHECK(gateway_adapter_host_process_one(adapter, 1U));
    gateway_adapter_get_stats(adapter, &stats);
    gateway_adapter_get_uart_rx_diagnostics(adapter, &diagnostics);
    CHECK(diagnostics.uart_domain_alive_decoded == 1U);
    CHECK(diagnostics.uart_domain_alive_accepted == 0U);
    CHECK(stats.uart_messages_rejected == 1U);
    CHECK(diagnostics.uart_last_link_sequence == UINT16_C(0xFFFE));
    CHECK(diagnostics.uart_last_domain_boot_id == UINT32_C(0x00000001));
    CHECK(diagnostics.uart_last_update_sequence == UINT32_C(7));
    CHECK(diagnostics.uart_last_source_age_ms == UINT16_MAX);
    CHECK(diagnostics.uart_last_domain_state == UART_M_CONTROLLER_INIT);
    CHECK(diagnostics.uart_last_quality == UART_M_QUALITY_NO_DATA);
    CHECK(!diagnostics.uart_domain_link_up);
    CHECK(s_status[2] == 0U);
    CHECK(s_status[3] == 0U);

    make_alive(&alive, UINT32_C(0x00000002));
    alive.link_sequence = UINT16_C(0xFFFF);
    alive.message.body.domain_alive.update_sequence = UINT32_C(8);
    gateway_adapter_on_uart_message(&alive, adapter);
    CHECK(gateway_adapter_host_process_one(adapter, 2U));
    gateway_adapter_get_stats(adapter, &stats);
    gateway_adapter_get_uart_rx_diagnostics(adapter, &diagnostics);
    CHECK(diagnostics.uart_domain_alive_decoded == 2U);
    CHECK(diagnostics.uart_domain_alive_accepted == 1U);
    CHECK(diagnostics.uart_domain_boot_observed_changes == 1U);
    CHECK(diagnostics.uart_last_link_sequence == UINT16_C(0xFFFF));
    CHECK(diagnostics.uart_last_update_sequence == UINT32_C(8));
    CHECK(diagnostics.uart_domain_link_up);
    CHECK(stats.production_uart_tx_count == 0U);
    CHECK(stats.production_m_context_tx_count == 0U);
    CHECK(stats.digital_key_auto_unlock_count == 0U);
    CHECK(gateway_adapter_stop(&adapter) == ESP_OK);
    return true;
}

typedef struct
{
    QueueHandle_t queue;
} mock_ble_server_stage_t;

static bool server_stage_init(mock_ble_server_stage_t *stage, size_t depth)
{
    stage->queue = xQueueCreate((UBaseType_t)depth,
                                (UBaseType_t)sizeof(ble_gatt_server_event_t));
    return stage->queue != NULL;
}

static void server_stage_deinit(mock_ble_server_stage_t *stage)
{
    if (stage->queue != NULL) {
        vQueueDelete(stage->queue);
        stage->queue = NULL;
    }
}

static bool server_stage_connect(mock_ble_server_stage_t *stage,
                                 uint16_t handle, uint8_t identity_seed)
{
    ble_gatt_server_event_t event =
        ble_event(BLE_GATT_EVENT_CONNECTED, handle, 0U);
    ble_connection_snapshot_t snapshot;

    if (!ble_connection_state_connect(&s_ble_connection_state, handle)) {
        return false;
    }
    s_link_secure = false;
    ble_connection_state_snapshot(&s_ble_connection_state, &snapshot);
    event.connection_generation = snapshot.connection_generation;
    add_bonded_identity(&event, identity_seed, true);
    return xQueueSend(stage->queue, &event, 0U) == pdTRUE;
}

static bool server_stage_disconnect(mock_ble_server_stage_t *stage,
                                    uint16_t handle)
{
    ble_gatt_server_event_t event =
        ble_event(BLE_GATT_EVENT_DISCONNECTED, handle, 0U);
    ble_connection_snapshot_t snapshot;

    ble_connection_state_snapshot(&s_ble_connection_state, &snapshot);
    event.connection_generation = snapshot.connection_generation;
    if (!ble_connection_state_disconnect(&s_ble_connection_state, handle)) {
        return false;
    }
    s_link_secure = false;
    return xQueueSend(stage->queue, &event, 0U) == pdTRUE;
}

static bool server_stage_enqueue_noncritical(mock_ble_server_stage_t *stage,
                                             uint16_t handle,
                                             uint32_t generation)
{
    ble_gatt_server_event_t event =
        ble_event(BLE_GATT_EVENT_MTU_CHANGED, handle, generation);

    event.data.mtu.mtu = 185U;
    return xQueueSend(stage->queue, &event, 0U) == pdTRUE;
}

static bool server_stage_deliver_one(mock_ble_server_stage_t *stage,
                                     gateway_adapter_t *adapter)
{
    ble_gatt_server_event_t event;

    if (xQueueReceive(stage->queue, &event, 0U) != pdTRUE) {
        return false;
    }
    gateway_adapter_on_ble_event(&event, adapter);
    return true;
}

static bool test_callbacks_deep_copy_and_queue_full(void)
{
    gateway_adapter_t *adapter = start_adapter(1U);
    ble_gatt_server_event_t first = ble_event(BLE_GATT_EVENT_CONNECTED, 7U, 1U);
    ble_gatt_server_event_t second = ble_event(BLE_GATT_EVENT_MTU_CHANGED, 7U, 1U);
    ble_connection_snapshot_t snapshot;
    gateway_adapter_stats_t stats;
    CHECK(adapter != NULL);
    CHECK(ble_connection_state_connect(&s_ble_connection_state, 7U));
    ble_connection_state_snapshot(&s_ble_connection_state, &snapshot);
    first.connection_generation = snapshot.connection_generation;
    second.connection_generation = snapshot.connection_generation;
    gateway_adapter_on_ble_event(&first, adapter);
    first.type = (ble_gatt_event_type_t)99;
    gateway_adapter_on_ble_event(&second, adapter);
    CHECK(gateway_adapter_host_process_one(adapter, UINT64_C(5000000000)));
    gateway_adapter_get_stats(adapter, &stats);
    CHECK(stats.ble_events_enqueued == 1U);
    CHECK(stats.queue_full == 1U);
    CHECK(stats.invalid_callbacks == 0U);
    CHECK(stats.events_processed == 1U);
    CHECK(gateway_adapter_stop(&adapter) == ESP_OK);
    return true;
}

static bool test_boot_bond_snapshot_deep_copy_and_exact_match(void)
{
    gateway_adapter_t *adapter = start_adapter_with_registration(4U, 0x30U);
    ble_gatt_server_event_t event =
        ble_event(BLE_GATT_EVENT_BOND_STORE_READY, UINT16_MAX, 0U);
    gateway_adapter_stats_t stats;
    size_t index;

    CHECK(adapter != NULL);
    event.data.bond_store.count = 1U;
    event.data.bond_store.peers[0].identity_resolved = true;
    event.data.bond_store.peers[0].bond_store_present = true;
    event.data.bond_store.peers[0].address_type = 1U;
    for (index = 0U; index < P6_BOND_IDENTITY_BYTES; ++index) {
        event.data.bond_store.peers[0].address[index] =
            (uint8_t)(0x30U + index);
    }
    gateway_adapter_on_ble_event(&event, adapter);
    memset(&event, 0, sizeof(event));
    event.type = (ble_gatt_event_type_t)99;
    CHECK(gateway_adapter_host_process_one(adapter, 1U));
    gateway_adapter_get_stats(adapter, &stats);
    CHECK(stats.p6_bond_reconciliation_events == 1U);
    CHECK(stats.p6_bond_reconciliation_failures == 0U);
    CHECK(stats.p6_registration_bond_reconciled);
    CHECK(!stats.p6_authorization_verified);
    CHECK(stats.production_uart_tx_count == 0U);
    CHECK(gateway_adapter_stop(&adapter) == ESP_OK);

    adapter = start_adapter_with_registration(4U, 0x40U);
    CHECK(adapter != NULL);
    event = ble_event(BLE_GATT_EVENT_BOND_STORE_READY, UINT16_MAX, 0U);
    event.data.bond_store.count = 0U;
    gateway_adapter_on_ble_event(&event, adapter);
    CHECK(gateway_adapter_host_process_one(adapter, 2U));
    gateway_adapter_get_stats(adapter, &stats);
    CHECK(stats.p6_bond_reconciliation_failures == 1U);
    CHECK(!stats.p6_registration_bond_reconciled);
    CHECK(!stats.p6_authorization_verified);
    CHECK(gateway_adapter_stop(&adapter) == ESP_OK);
    return true;
}

static bool test_gateway_task_builds_authorized_context_candidate(void)
{
    gateway_adapter_t *adapter = start_adapter_with_registration(8U, 0x51U);
    ble_gatt_server_event_t bond_store =
        ble_event(BLE_GATT_EVENT_BOND_STORE_READY, UINT16_MAX, 0U);
    ble_gatt_server_event_t connected =
        ble_event(BLE_GATT_EVENT_CONNECTED, 51U, 0U);
    ble_gatt_server_event_t security =
        ble_event(BLE_GATT_EVENT_SECURITY_CHANGED, 51U, 0U);
    ble_gatt_server_event_t app_state =
        ble_event(BLE_GATT_EVENT_APP_STATE, 51U, 0U);
    ble_connection_snapshot_t snapshot;
    gateway_adapter_stats_t stats;
    uart_m_context_t context;
    size_t index;

    CHECK(adapter != NULL);
    CHECK(gateway_adapter_host_configure_context_source_policy(
        adapter, 100U, 50U));
    CHECK(!gateway_adapter_host_get_context_candidate(adapter, &context));
    CHECK(!gateway_adapter_host_get_context_tx_candidate(adapter, &context));

    bond_store.data.bond_store.count = 1U;
    bond_store.data.bond_store.peers[0].identity_resolved = true;
    bond_store.data.bond_store.peers[0].bond_store_present = true;
    bond_store.data.bond_store.peers[0].address_type = 1U;
    for (index = 0U; index < P6_BOND_IDENTITY_BYTES; ++index) {
        bond_store.data.bond_store.peers[0].address[index] =
            (uint8_t)(0x51U + index);
    }
    mock_idf_set_time_ms(1U);
    gateway_adapter_on_ble_event(&bond_store, adapter);
    CHECK(gateway_adapter_host_process_one(adapter, 1U));
    CHECK(!gateway_adapter_host_get_context_candidate(adapter, &context));

    add_bonded_identity(&connected, 0x51U, true);
    mock_idf_set_time_ms(2U);
    CHECK(process_ble_event(adapter, &connected, 2U));
    CHECK(!gateway_adapter_host_get_context_candidate(adapter, &context));
    ble_connection_state_snapshot(&s_ble_connection_state, &snapshot);
    security.connection_generation = snapshot.connection_generation;
    add_bonded_identity(&security, 0x51U, true);
    security.data.security.encrypted = true;
    security.data.security.bonded = true;
    mock_idf_set_time_ms(3U);
    CHECK(process_ble_event(adapter, &security, 3U));

    CHECK(gateway_adapter_host_get_context_candidate(adapter, &context));
    CHECK(!gateway_adapter_host_get_context_tx_candidate(adapter, &context));
    CHECK(context.context.device_context_id == UINT32_C(0x11223344));
    CHECK(context.context.session_id != 0U);
    CHECK(context.registration == UART_M_TRI_YES);
    CHECK(context.connection == UART_M_TRI_YES);
    CHECK(context.app_active == UART_M_TRI_UNKNOWN);
    CHECK(context.registration_connection_quality == UART_M_QUALITY_OK);
    CHECK(context.registration_connection_update_sequence == 1U);
    CHECK(context.registration_connection_age_ms == 0U);
    CHECK(context.app_quality == UART_M_QUALITY_NO_DATA);
    CHECK(context.app_update_sequence == 0U);
    CHECK(context.app_age_ms == UINT16_MAX);
    CHECK(context.proximity == UART_M_TRI_UNKNOWN);
    CHECK(context.proximity_validity == UART_M_VALIDITY_INVALID);
    CHECK(context.proximity_reason == UART_M_QUALITY_REASON_NO_DATA);
    CHECK(context.proximity_update_sequence == 0U);
    CHECK(context.proximity_age_ms == UINT16_MAX);

    app_state.connection_generation = snapshot.connection_generation;
    app_state.data.app_state.app_instance_id = UINT32_C(0x10203040);
    app_state.data.app_state.active = true;
    mock_idf_set_time_ms(4U);
    CHECK(process_ble_event(adapter, &app_state, 4U));
    CHECK(gateway_adapter_host_get_context_candidate(adapter, &context));
    CHECK(gateway_adapter_host_get_context_tx_candidate(adapter, &context));
    CHECK(context.app_active == UART_M_TRI_YES);
    CHECK(context.app_quality == UART_M_QUALITY_OK);
    CHECK(context.app_update_sequence == 1U);
    CHECK(context.app_age_ms == 0U);

    CHECK(gateway_adapter_host_tick(adapter, 14U));
    CHECK(gateway_adapter_host_get_context_candidate(adapter, &context));
    CHECK(gateway_adapter_host_get_context_tx_candidate(adapter, &context));
    CHECK(context.registration_connection_age_ms == 11U);
    CHECK(context.app_age_ms == 10U);

    CHECK(gateway_adapter_host_tick(adapter, 54U));
    CHECK(gateway_adapter_host_get_context_tx_candidate(adapter, &context));
    CHECK(context.app_quality == UART_M_QUALITY_STALE);
    CHECK(context.app_update_sequence == 1U);
    CHECK(context.app_age_ms == 50U);

    app_state.data.app_state.active = false;
    mock_idf_set_time_ms(55U);
    CHECK(process_ble_event(adapter, &app_state, 55U));
    CHECK(gateway_adapter_host_get_context_candidate(adapter, &context));
    CHECK(context.app_active == UART_M_TRI_NO);
    CHECK(context.app_quality == UART_M_QUALITY_OK);
    CHECK(context.app_update_sequence == 2U);
    CHECK(context.app_age_ms == 0U);

    CHECK(gateway_adapter_host_configure_context_source_policy(
        adapter, 0U, 0U));
    CHECK(gateway_adapter_host_tick(adapter, 56U));
    CHECK(gateway_adapter_host_get_context_candidate(adapter, &context));
    CHECK(!gateway_adapter_host_get_context_tx_candidate(adapter, &context));
    CHECK(context.registration_connection_quality == UART_M_QUALITY_NO_DATA);
    CHECK(context.app_quality == UART_M_QUALITY_NO_DATA);

    CHECK(ble_connection_state_disconnect(&s_ble_connection_state, 51U));
    CHECK(!gateway_adapter_host_process_one(adapter, 57U));
    CHECK(!gateway_adapter_host_get_context_candidate(adapter, &context));
    CHECK(!gateway_adapter_host_get_context_tx_candidate(adapter, &context));
    gateway_adapter_get_stats(adapter, &stats);
    CHECK(!stats.context_dto_ready);
    CHECK(!stats.p6_authorization_verified);
    CHECK(stats.production_uart_tx_count == 0U);
    CHECK(stats.production_m_context_tx_count == 0U);
    CHECK(stats.digital_key_auto_unlock_count == 0U);
    CHECK(gateway_adapter_stop(&adapter) == ESP_OK);
    return true;
}

static bool test_request_admission_boundary_revalidates_before_uart(void)
{
    gateway_adapter_t *adapter = start_adapter_with_registration(8U, 0x52U);
    ble_gatt_server_event_t bond_store =
        ble_event(BLE_GATT_EVENT_BOND_STORE_READY, UINT16_MAX, 0U);
    ble_gatt_server_event_t connected =
        ble_event(BLE_GATT_EVENT_CONNECTED, 52U, 0U);
    ble_gatt_server_event_t security =
        ble_event(BLE_GATT_EVENT_SECURITY_CHANGED, 52U, 0U);
    ble_gatt_server_event_t command =
        ble_event(BLE_GATT_EVENT_APP_COMMAND, 52U, 0U);
    ble_connection_snapshot_t connection;
    gateway_core_context_t core_context;
    gateway_adapter_stats_t stats;
    uart_m_context_t context;
    uart_m_transport_rx_message_t alive;
    uart_m_request_t request;
    size_t index;

    CHECK(adapter != NULL);
    bond_store.data.bond_store.count = 1U;
    bond_store.data.bond_store.peers[0].identity_resolved = true;
    bond_store.data.bond_store.peers[0].bond_store_present = true;
    bond_store.data.bond_store.peers[0].address_type = 1U;
    for (index = 0U; index < P6_BOND_IDENTITY_BYTES; ++index) {
        bond_store.data.bond_store.peers[0].address[index] =
            (uint8_t)(0x52U + index);
    }
    gateway_adapter_on_ble_event(&bond_store, adapter);
    CHECK(gateway_adapter_host_process_one(adapter, 1U));

    add_bonded_identity(&connected, 0x52U, true);
    CHECK(process_ble_event(adapter, &connected, 2U));
    ble_connection_state_snapshot(&s_ble_connection_state, &connection);
    security.connection_generation = connection.connection_generation;
    add_bonded_identity(&security, 0x52U, true);
    security.data.security.encrypted = true;
    security.data.security.bonded = true;
    CHECK(process_ble_event(adapter, &security, 3U));
    CHECK(gateway_adapter_host_get_context_candidate(adapter, &context));

    memset(&core_context, 0, sizeof(core_context));
    core_context.identity.device_context_id = context.context.device_context_id;
    core_context.identity.session_id = context.context.session_id;
    core_context.identity.verified = true;
    core_context.connection_generation = connection.connection_generation;
    core_context.connected = true;
    core_context.encrypted = true;
    core_context.registered = true;
    core_context.session_ready = true;
    core_context.authorization_verified = true;
    core_context.observed_at_ms = 4U;
    CHECK(gateway_adapter_host_set_core_context(adapter, &core_context, 4U));
    make_alive(&alive, UINT32_C(0x12345678));
    alive.received_at_us = INT64_C(5000);
    gateway_adapter_on_uart_message(&alive, adapter);
    CHECK(gateway_adapter_host_process_one(adapter, 5U));
    CHECK(gateway_adapter_host_configure_request_admission(adapter, 100U));

    command.connection_generation = connection.connection_generation;
    command.data.app_command.message_type = (uint8_t)UART_M_TYPE_REQUEST;
    command.data.app_command.payload_length = 12U;
    command.data.app_command.payload[0] = 0x44U;
    command.data.app_command.payload[1] = 0x33U;
    command.data.app_command.payload[2] = 0x22U;
    command.data.app_command.payload[3] = 0x11U;
    command.data.app_command.payload[4] = 1U;
    command.data.app_command.payload[5] = 1U;
    mock_idf_set_time_ms(10U);
    CHECK(process_ble_event(adapter, &command, 10U));
    CHECK(gateway_adapter_host_prepare_request_candidate(adapter, 109U,
                                                         &request) ==
          GATEWAY_REQUEST_ADMISSION_OK);
    CHECK(request.request_id == UINT32_C(0x11223344));
    gateway_adapter_get_stats(adapter, &stats);
    CHECK(stats.production_uart_tx_count == 0U);

    command.data.app_command.payload[0] = 0x45U;
    mock_idf_set_time_ms(20U);
    CHECK(process_ble_event(adapter, &command, 20U));
    CHECK(gateway_adapter_host_prepare_request_candidate(adapter, 120U,
                                                         &request) ==
          GATEWAY_REQUEST_ADMISSION_TTL_EXPIRED);
    CHECK(stats.production_uart_tx_count == 0U);

    command.data.app_command.payload[0] = 0x46U;
    mock_idf_set_time_ms(30U);
    CHECK(process_ble_event(adapter, &command, 30U));
    CHECK(process_ble_event(adapter,
                            &(ble_gatt_server_event_t){
                                .type = BLE_GATT_EVENT_DISCONNECTED,
                                .connection_handle = 52U,
                            },
                            31U));
    CHECK(gateway_adapter_host_prepare_request_candidate(adapter, 32U,
                                                         &request) ==
          GATEWAY_REQUEST_ADMISSION_NO_CANDIDATE);
    gateway_adapter_get_stats(adapter, &stats);
    CHECK(stats.production_uart_tx_count == 0U);
    CHECK(gateway_adapter_stop(&adapter) == ESP_OK);
    return true;
}

static bool test_read_only_query_candidate_and_notify_runtime(void)
{
    gateway_adapter_t *adapter = start_adapter_with_registration(8U, 0x56U);
    ble_gatt_server_event_t bond_store =
        ble_event(BLE_GATT_EVENT_BOND_STORE_READY, UINT16_MAX, 0U);
    ble_gatt_server_event_t connected =
        ble_event(BLE_GATT_EVENT_CONNECTED, 56U, 0U);
    ble_gatt_server_event_t security =
        ble_event(BLE_GATT_EVENT_SECURITY_CHANGED, 56U, 0U);
    ble_gatt_server_event_t command =
        ble_event(BLE_GATT_EVENT_APP_COMMAND, 56U, 0U);
    ble_gatt_server_event_t subscription =
        ble_event(BLE_GATT_EVENT_SUBSCRIPTION_CHANGED, 56U, 0U);
    ble_gatt_server_event_t mtu =
        ble_event(BLE_GATT_EVENT_MTU_CHANGED, 56U, 0U);
    ble_gatt_server_event_t disconnected =
        ble_event(BLE_GATT_EVENT_DISCONNECTED, 56U, 0U);
    ble_connection_snapshot_t connection;
    gateway_core_context_t core_context;
    gateway_adapter_stats_t stats;
    uart_m_context_t context;
    uart_m_query_t query;
    uart_m_transport_rx_message_t alive;
    uart_m_transport_rx_message_t result;
    uart_m_transport_rx_message_t end;
    const uint32_t domain_boot_id = UINT32_C(0x55667788);
    const uint64_t request_session = UINT64_C(0x8877665544332211);
    uint32_t query_id = UINT32_C(0x10203040);
    size_t index;

    CHECK(adapter != NULL);
    bond_store.data.bond_store.count = 1U;
    bond_store.data.bond_store.peers[0].identity_resolved = true;
    bond_store.data.bond_store.peers[0].bond_store_present = true;
    bond_store.data.bond_store.peers[0].address_type = 1U;
    for (index = 0U; index < P6_BOND_IDENTITY_BYTES; ++index) {
        bond_store.data.bond_store.peers[0].address[index] =
            (uint8_t)(0x56U + index);
    }
    gateway_adapter_on_ble_event(&bond_store, adapter);
    CHECK(gateway_adapter_host_process_one(adapter, 1U));

    add_bonded_identity(&connected, 0x56U, true);
    CHECK(process_ble_event(adapter, &connected, 2U));
    ble_connection_state_snapshot(&s_ble_connection_state, &connection);
    security.connection_generation = connection.connection_generation;
    add_bonded_identity(&security, 0x56U, true);
    security.data.security.encrypted = true;
    security.data.security.bonded = true;
    CHECK(process_ble_event(adapter, &security, 3U));
    CHECK(gateway_adapter_host_get_context_candidate(adapter, &context));

    memset(&core_context, 0, sizeof(core_context));
    core_context.identity.device_context_id = context.context.device_context_id;
    core_context.identity.session_id = context.context.session_id;
    core_context.identity.verified = true;
    core_context.connection_generation = connection.connection_generation;
    core_context.connected = true;
    core_context.encrypted = true;
    core_context.registered = true;
    core_context.session_ready = false;
    core_context.authorization_verified = true;
    core_context.observed_at_ms = 4U;
    CHECK(gateway_adapter_host_set_core_context(adapter, &core_context, 4U));
    make_alive(&alive, domain_boot_id);
    alive.received_at_us = INT64_C(5000);
    gateway_adapter_on_uart_message(&alive, adapter);
    CHECK(gateway_adapter_host_process_one(adapter, 5U));

    command.connection_generation = connection.connection_generation;
    command.data.app_command.message_type = (uint8_t)UART_M_TYPE_QUERY;
    command.data.app_command.payload_length = 20U;
    command.data.app_command.payload[0] = (uint8_t)query_id;
    command.data.app_command.payload[1] = (uint8_t)(query_id >> 8U);
    command.data.app_command.payload[2] = (uint8_t)(query_id >> 16U);
    command.data.app_command.payload[3] = (uint8_t)(query_id >> 24U);
    command.data.app_command.payload[4] = (uint8_t)UART_M_QUERY_RESULT;
    command.data.app_command.payload[5] = 0U;
    for (index = 0U; index < 8U; ++index) {
        command.data.app_command.payload[6U + index] =
            (uint8_t)(request_session >> (index * 8U));
    }
    command.data.app_command.payload[14] = 0x44U;
    command.data.app_command.payload[15] = 0x33U;
    command.data.app_command.payload[16] = 0x22U;
    command.data.app_command.payload[17] = 0x11U;
    command.data.app_command.payload[18] = 0U;
    command.data.app_command.payload[19] = 0U;
    CHECK(process_ble_event(adapter, &command, 10U));
    CHECK(gateway_adapter_host_take_query_candidate(adapter, 11U, &query));
    CHECK(query.query_id == query_id);
    CHECK(query.scope == UART_M_QUERY_RESULT);
    CHECK(query.request_session == request_session);
    CHECK(query.context.device_context_id == context.context.device_context_id);
    CHECK(query.context.session_id == context.context.session_id);

    /* Same active Query is rejected and never creates another candidate. */
    CHECK(process_ble_event(adapter, &command, 12U));
    CHECK(!gateway_adapter_host_take_query_candidate(adapter, 13U, &query));

    subscription.connection_generation = connection.connection_generation;
    subscription.data.subscription.vehicle_data_notify = true;
    CHECK(process_ble_event(adapter, &subscription, 14U));
    mtu.connection_generation = connection.connection_generation;
    mtu.data.mtu.mtu = 185U;
    CHECK(process_ble_event(adapter, &mtu, 15U));

    make_result(&result, context.context.device_context_id,
                context.context.session_id, domain_boot_id,
                request_session, UINT32_C(0x11223344));
    result.message.body.result.context.query_id = query_id;
    result.received_at_us = INT64_C(16000);
    gateway_adapter_on_uart_message(&result, adapter);
    CHECK(gateway_adapter_host_process_one(adapter, 16U));
    CHECK(s_vehicle_notify_calls == 1U);
    CHECK(s_vehicle_type == (uint8_t)UART_M_TYPE_RESULT);

    make_query_end(&end, context.context.device_context_id,
                   context.context.session_id, domain_boot_id, query_id,
                   UART_M_QUERY_RESULT, UART_M_QUERY_DONE, 1U);
    end.received_at_us = INT64_C(17000);
    gateway_adapter_on_uart_message(&end, adapter);
    CHECK(gateway_adapter_host_process_one(adapter, 17U));
    CHECK(s_vehicle_notify_calls == 2U);
    CHECK(s_vehicle_type == (uint8_t)UART_M_TYPE_QUERY_END);

    /* PARTIAL is delivered but remains a non-success HOLD outcome. */
    query_id++;
    command.data.app_command.payload[0] = (uint8_t)query_id;
    CHECK(process_ble_event(adapter, &command, 18U));
    CHECK(gateway_adapter_host_take_query_candidate(adapter, 19U, &query));
    make_query_end(&end, context.context.device_context_id,
                   context.context.session_id, domain_boot_id, query_id,
                   UART_M_QUERY_RESULT, UART_M_QUERY_PARTIAL_UNCONFIRMED, 0U);
    end.received_at_us = INT64_C(20000);
    gateway_adapter_on_uart_message(&end, adapter);
    CHECK(gateway_adapter_host_process_one(adapter, 20U));
    CHECK(s_vehicle_notify_calls == 3U);

    /* Count mismatch is rejected and not exposed as a BLE success. */
    query_id++;
    command.data.app_command.payload[0] = (uint8_t)query_id;
    CHECK(process_ble_event(adapter, &command, 21U));
    CHECK(gateway_adapter_host_take_query_candidate(adapter, 22U, &query));
    make_query_end(&end, context.context.device_context_id,
                   context.context.session_id, domain_boot_id, query_id,
                   UART_M_QUERY_RESULT, UART_M_QUERY_DONE, 1U);
    end.received_at_us = INT64_C(23000);
    gateway_adapter_on_uart_message(&end, adapter);
    CHECK(gateway_adapter_host_process_one(adapter, 23U));
    CHECK(s_vehicle_notify_calls == 3U);

    /* A pending candidate is discarded on disconnect and never replayed. */
    query_id++;
    command.data.app_command.payload[0] = (uint8_t)query_id;
    CHECK(process_ble_event(adapter, &command, 24U));
    disconnected.connection_generation = connection.connection_generation;
    CHECK(process_ble_event(adapter, &disconnected, 25U));
    CHECK(!gateway_adapter_host_take_query_candidate(adapter, 26U, &query));
    gateway_adapter_get_stats(adapter, &stats);
    CHECK(!stats.query_candidate_pending);
    CHECK(stats.query_candidates_queued == 4U);
    CHECK(stats.query_candidates_invalidated >= 1U);
    CHECK(stats.production_uart_tx_count == 0U);
    CHECK(stats.production_m_context_tx_count == 0U);
    CHECK(stats.digital_key_auto_unlock_count == 0U);
    CHECK(gateway_adapter_stop(&adapter) == ESP_OK);
    return true;
}

static bool test_notify_gate_subscription_mtu_then_security(void)
{
    gateway_adapter_t *adapter = start_adapter(8U);
    ble_gatt_server_event_t connected = ble_event(BLE_GATT_EVENT_CONNECTED, 7U, 1U);
    ble_gatt_server_event_t subscription = ble_event(BLE_GATT_EVENT_SUBSCRIPTION_CHANGED, 7U, 1U);
    ble_gatt_server_event_t mtu = ble_event(BLE_GATT_EVENT_MTU_CHANGED, 7U, 1U);
    ble_gatt_server_event_t security = ble_event(BLE_GATT_EVENT_SECURITY_CHANGED, 7U, 1U);
    gateway_adapter_stats_t stats;
    CHECK(adapter != NULL);
    gateway_adapter_get_stats(adapter, &stats);
    CHECK(stats.gateway_status_notify_pending);
    CHECK(s_gateway_notify_calls == 0U);

    CHECK(process_ble_event(adapter, &connected, 1U));
    CHECK(s_gateway_notify_calls == 0U);
    subscription.data.subscription.gateway_status_notify = true;
    subscription.data.subscription.vehicle_data_notify = true;
    CHECK(process_ble_event(adapter, &subscription, 2U));
    CHECK(s_gateway_notify_calls == 0U);
    mtu.data.mtu.mtu = 185U;
    CHECK(process_ble_event(adapter, &mtu, 3U));
    CHECK(s_gateway_notify_calls == 0U);
    security.data.security.encrypted = true;
    security.data.security.authenticated = false;
    security.data.security.bonded = true;
    CHECK(process_ble_event(adapter, &security, 4U));
    CHECK(s_gateway_notify_calls == 1U);

    CHECK(process_ble_event(adapter, &security, 5U));
    CHECK(process_ble_event(adapter, &subscription, 6U));
    CHECK(process_ble_event(adapter, &mtu, 7U));
    CHECK(s_gateway_notify_calls == 1U);
    gateway_adapter_get_stats(adapter, &stats);
    CHECK(!stats.gateway_status_notify_pending);
    CHECK(stats.gateway_status_notify_api_accepted == 1U);
    CHECK(stats.gateway_status_notify_failures == 0U);
    CHECK(s_vehicle_notify_calls == 0U);
    CHECK(stats.production_uart_tx_count == 0U);
    CHECK(gateway_adapter_stop(&adapter) == ESP_OK);
    return true;
}

static bool test_notify_gate_security_then_subscription(void)
{
    gateway_adapter_t *adapter = start_adapter(6U);
    ble_gatt_server_event_t connected = ble_event(BLE_GATT_EVENT_CONNECTED, 8U, 1U);
    ble_gatt_server_event_t mtu = ble_event(BLE_GATT_EVENT_MTU_CHANGED, 8U, 1U);
    ble_gatt_server_event_t security = ble_event(BLE_GATT_EVENT_SECURITY_CHANGED, 8U, 1U);
    ble_gatt_server_event_t subscription = ble_event(BLE_GATT_EVENT_SUBSCRIPTION_CHANGED, 8U, 1U);
    CHECK(adapter != NULL);
    mtu.data.mtu.mtu = 185U;
    security.data.security.encrypted = true;
    security.data.security.bonded = true;
    subscription.data.subscription.gateway_status_notify = true;
    CHECK(process_ble_event(adapter, &connected, 1U));
    CHECK(process_ble_event(adapter, &mtu, 2U));
    CHECK(process_ble_event(adapter, &security, 3U));
    CHECK(s_gateway_notify_calls == 0U);
    CHECK(process_ble_event(adapter, &subscription, 4U));
    CHECK(s_gateway_notify_calls == 1U);
    CHECK(gateway_adapter_stop(&adapter) == ESP_OK);
    return true;
}

static bool test_notify_gate_security_subscription_then_mtu(void)
{
    gateway_adapter_t *adapter = start_adapter(6U);
    ble_gatt_server_event_t connected = ble_event(BLE_GATT_EVENT_CONNECTED, 9U, 1U);
    ble_gatt_server_event_t security = ble_event(BLE_GATT_EVENT_SECURITY_CHANGED, 9U, 1U);
    ble_gatt_server_event_t subscription = ble_event(BLE_GATT_EVENT_SUBSCRIPTION_CHANGED, 9U, 1U);
    ble_gatt_server_event_t mtu = ble_event(BLE_GATT_EVENT_MTU_CHANGED, 9U, 1U);
    CHECK(adapter != NULL);
    security.data.security.encrypted = true;
    security.data.security.bonded = true;
    subscription.data.subscription.gateway_status_notify = true;
    mtu.data.mtu.mtu = 185U;
    CHECK(process_ble_event(adapter, &connected, 1U));
    CHECK(process_ble_event(adapter, &security, 2U));
    CHECK(process_ble_event(adapter, &subscription, 3U));
    CHECK(s_gateway_notify_calls == 0U);
    CHECK(process_ble_event(adapter, &mtu, 4U));
    CHECK(s_gateway_notify_calls == 1U);
    CHECK(gateway_adapter_stop(&adapter) == ESP_OK);
    return true;
}

static bool test_disconnect_stale_events_and_app_state_copy(void)
{
    gateway_adapter_t *adapter = start_adapter(8U);
    ble_gatt_server_event_t connected = ble_event(BLE_GATT_EVENT_CONNECTED, 10U, 1U);
    ble_gatt_server_event_t app_state = ble_event(BLE_GATT_EVENT_APP_STATE, 10U, 1U);
    ble_gatt_server_event_t disconnected = ble_event(BLE_GATT_EVENT_DISCONNECTED, 10U, 1U);
    ble_gatt_server_event_t reconnect = ble_event(BLE_GATT_EVENT_CONNECTED, 10U, 2U);
    ble_gatt_server_event_t stale_mtu = ble_event(BLE_GATT_EVENT_MTU_CHANGED, 10U, 1U);
    ble_gatt_server_event_t stale_subscription = ble_event(BLE_GATT_EVENT_SUBSCRIPTION_CHANGED, 10U, 1U);
    uart_m_transport_rx_message_t alive;
    gateway_adapter_stats_t stats;
    CHECK(adapter != NULL);
    CHECK(process_ble_event(adapter, &connected, 1U));
    app_state.data.app_state.active = true;
    app_state.data.app_state.app_instance_id = 1234U;
    gateway_adapter_on_ble_event(&app_state, adapter);
    app_state.data.app_state.active = false;
    CHECK(gateway_adapter_host_process_one(adapter, 2U));
    gateway_adapter_get_stats(adapter, &stats);
    CHECK(stats.app_state_events_processed == 1U);
    CHECK(stats.app_state_seen);
    CHECK(stats.app_state_active);

    make_alive(&alive, 10U);
    gateway_adapter_on_uart_message(&alive, adapter);
    CHECK(gateway_adapter_host_process_one(adapter, 3U));
    make_alive(&alive, 11U);
    gateway_adapter_on_uart_message(&alive, adapter);
    CHECK(gateway_adapter_host_process_one(adapter, 4U));

    CHECK(process_ble_event(adapter, &disconnected, 5U));
    make_alive(&alive, 12U);
    gateway_adapter_on_uart_message(&alive, adapter);
    CHECK(gateway_adapter_host_process_one(adapter, 6U));
    CHECK(s_gateway_notify_calls == 0U);
    CHECK(process_ble_event(adapter, &reconnect, 7U));
    stale_mtu.data.mtu.mtu = 185U;
    stale_subscription.data.subscription.gateway_status_notify = true;
    CHECK(process_ble_event(adapter, &stale_mtu, 8U));
    CHECK(process_ble_event(adapter, &stale_subscription, 9U));
    gateway_adapter_get_stats(adapter, &stats);
    CHECK(stats.stale_ble_events == 2U);
    CHECK(stats.disconnects_processed == 1U);
    CHECK(stats.domain_boot_changes == 2U);
    CHECK(s_gateway_notify_calls == 0U);
    CHECK(s_vehicle_notify_calls == 0U);
    CHECK(stats.production_uart_tx_count == 0U);
    CHECK(gateway_adapter_stop(&adapter) == ESP_OK);
    return true;
}

static bool test_notify_failure_retains_pending_and_command_stays_blocked(void)
{
    gateway_adapter_t *adapter = start_adapter(8U);
    ble_gatt_server_event_t connected = ble_event(BLE_GATT_EVENT_CONNECTED, 11U, 1U);
    ble_gatt_server_event_t mtu = ble_event(BLE_GATT_EVENT_MTU_CHANGED, 11U, 1U);
    ble_gatt_server_event_t security = ble_event(BLE_GATT_EVENT_SECURITY_CHANGED, 11U, 1U);
    ble_gatt_server_event_t subscription = ble_event(BLE_GATT_EVENT_SUBSCRIPTION_CHANGED, 11U, 1U);
    ble_gatt_server_event_t command = ble_event(BLE_GATT_EVENT_APP_COMMAND, 11U, 1U);
    gateway_adapter_stats_t stats;
    CHECK(adapter != NULL);
    mtu.data.mtu.mtu = 185U;
    security.data.security.encrypted = true;
    security.data.security.bonded = true;
    subscription.data.subscription.gateway_status_notify = true;
    command.data.app_command.message_type = 0x10U;
    command.data.app_command.payload_length = 12U;
    command.data.app_command.payload[0] = 1U;
    CHECK(process_ble_event(adapter, &connected, 1U));
    CHECK(process_ble_event(adapter, &mtu, 2U));
    CHECK(process_ble_event(adapter, &security, 3U));
    s_notify_result = ESP_FAIL;
    CHECK(process_ble_event(adapter, &subscription, 4U));
    gateway_adapter_get_stats(adapter, &stats);
    CHECK(stats.gateway_status_notify_failures == 1U);
    CHECK(stats.gateway_status_notify_pending);
    s_notify_result = ESP_OK;
    CHECK(process_ble_event(adapter, &security, 5U));
    CHECK(s_gateway_notify_calls == 2U);
    CHECK(process_ble_event(adapter, &command, 6U));
    gateway_adapter_get_stats(adapter, &stats);
    CHECK(stats.gateway_status_notify_api_accepted == 1U);
    CHECK(!stats.gateway_status_notify_pending);
    CHECK(stats.app_commands_blocked == 1U);
    CHECK(stats.core_rejections == 1U);
    CHECK(stats.production_uart_tx_count == 0U);
    CHECK(s_vehicle_notify_calls == 0U);
    CHECK(gateway_adapter_is_safe_mode(adapter));
    CHECK(gateway_adapter_stop(&adapter) == ESP_OK);
    return true;
}

static bool test_p6_observation_has_no_production_authority(void)
{
    gateway_adapter_t *adapter = start_adapter(8U);
    ble_gatt_server_event_t connected =
        ble_event(BLE_GATT_EVENT_CONNECTED, 12U, 1U);
    ble_gatt_server_event_t security =
        ble_event(BLE_GATT_EVENT_SECURITY_CHANGED, 12U, 1U);
    ble_gatt_server_event_t app_state =
        ble_event(BLE_GATT_EVENT_APP_STATE, 12U, 1U);
    ble_gatt_server_event_t disconnected =
        ble_event(BLE_GATT_EVENT_DISCONNECTED, 12U, 1U);
    gateway_adapter_stats_t stats;

    CHECK(adapter != NULL);
    add_bonded_identity(&connected, 0x20U, true);
    add_bonded_identity(&security, 0x20U, true);
    security.data.security.encrypted = true;
    security.data.security.bonded = true;
    app_state.data.app_state.active = true;
    app_state.data.app_state.app_instance_id = 1U;
    CHECK(process_ble_event(adapter, &connected, 1U));
    CHECK(process_ble_event(adapter, &security, 2U));
    CHECK(process_ble_event(adapter, &app_state, 3U));
    gateway_adapter_get_stats(adapter, &stats);
    CHECK(!stats.p6_registration_available);
    CHECK(!stats.p6_authorization_verified);
    CHECK(stats.p6_identity_rejections == 0U);
    CHECK(stats.p6_session_start_attempts == 0U);
    CHECK(stats.production_uart_tx_count == 0U);
    CHECK(stats.production_m_context_tx_count == 0U);
    CHECK(stats.digital_key_auto_unlock_count == 0U);
    CHECK(s_vehicle_notify_calls == 0U);
    CHECK(process_ble_event(adapter, &disconnected, 4U));
    CHECK(gateway_adapter_stop(&adapter) == ESP_OK);
    return true;
}

static bool test_server_queue_full_disconnect_invalidates(void)
{
    mock_ble_server_stage_t stage = {0};
    gateway_adapter_t *adapter;
    ble_connection_snapshot_t snapshot;
    gateway_adapter_stats_t stats;

    CHECK(server_stage_init(&stage, 1U));
    adapter = start_adapter(4U);
    CHECK(adapter != NULL);
    CHECK(server_stage_connect(&stage, 20U, 0x20U));
    CHECK(server_stage_deliver_one(&stage, adapter));
    CHECK(gateway_adapter_host_process_one(adapter, 1U));
    ble_connection_state_snapshot(&s_ble_connection_state, &snapshot);
    CHECK(server_stage_enqueue_noncritical(&stage, 20U,
                                           snapshot.connection_generation));
    CHECK(!server_stage_disconnect(&stage, 20U));
    CHECK(!gateway_adapter_host_process_one(adapter, 2U));
    gateway_adapter_get_stats(adapter, &stats);
    CHECK(stats.ble_epoch_invalidations == 1U);
    CHECK(stats.disconnects_processed == 1U);
    CHECK(!stats.p6_authorization_verified);
    CHECK(!gateway_adapter_p6_authorization_is_current(adapter));
    CHECK(server_stage_deliver_one(&stage, adapter));
    CHECK(gateway_adapter_host_process_one(adapter, 3U));
    gateway_adapter_get_stats(adapter, &stats);
    CHECK(stats.stale_ble_events >= 1U);
    CHECK(stats.production_uart_tx_count == 0U);
    CHECK(stats.production_m_context_tx_count == 0U);
    CHECK(stats.digital_key_auto_unlock_count == 0U);
    CHECK(gateway_adapter_stop(&adapter) == ESP_OK);
    server_stage_deinit(&stage);
    return true;
}

static bool test_gateway_queue_full_disconnect_invalidates(void)
{
    gateway_adapter_t *adapter = start_adapter(1U);
    ble_gatt_server_event_t connected =
        ble_event(BLE_GATT_EVENT_CONNECTED, 21U, 0U);
    ble_gatt_server_event_t filler =
        ble_event(BLE_GATT_EVENT_MTU_CHANGED, 21U, 1U);
    ble_gatt_server_event_t disconnected =
        ble_event(BLE_GATT_EVENT_DISCONNECTED, 21U, 1U);
    ble_connection_snapshot_t snapshot;
    gateway_adapter_stats_t stats;

    CHECK(adapter != NULL);
    add_bonded_identity(&connected, 0x21U, true);
    CHECK(process_ble_event(adapter, &connected, 1U));
    ble_connection_state_snapshot(&s_ble_connection_state, &snapshot);
    filler.connection_generation = snapshot.connection_generation;
    disconnected.connection_generation = snapshot.connection_generation;
    filler.data.mtu.mtu = 185U;
    gateway_adapter_on_ble_event(&filler, adapter);
    CHECK(ble_connection_state_disconnect(&s_ble_connection_state, 21U));
    gateway_adapter_on_ble_event(&disconnected, adapter);
    CHECK(gateway_adapter_host_process_one(adapter, 2U));
    gateway_adapter_get_stats(adapter, &stats);
    CHECK(stats.queue_full == 1U);
    CHECK(stats.ble_epoch_invalidations == 1U);
    CHECK(!stats.p6_authorization_verified);
    CHECK(!gateway_adapter_p6_authorization_is_current(adapter));
    CHECK(gateway_adapter_stop(&adapter) == ESP_OK);
    return true;
}

static bool test_both_queues_full_then_normal_reconnect_recovers_notify(void)
{
    mock_ble_server_stage_t stage = {0};
    gateway_adapter_t *adapter;
    ble_gatt_server_event_t adapter_filler;
    ble_gatt_server_event_t delayed_security;
    ble_gatt_server_event_t delayed_app_state;
    ble_gatt_server_event_t subscription;
    ble_gatt_server_event_t mtu;
    ble_gatt_server_event_t security;
    ble_connection_snapshot_t snapshot;
    gateway_adapter_stats_t stats;
    uint32_t generation_one;

    CHECK(server_stage_init(&stage, 1U));
    adapter = start_adapter(1U);
    CHECK(adapter != NULL);
    CHECK(server_stage_connect(&stage, 22U, 0x22U));
    CHECK(server_stage_deliver_one(&stage, adapter));
    CHECK(gateway_adapter_host_process_one(adapter, 1U));
    ble_connection_state_snapshot(&s_ble_connection_state, &snapshot);
    generation_one = snapshot.connection_generation;
    adapter_filler = ble_event(BLE_GATT_EVENT_MTU_CHANGED, 22U,
                               generation_one);
    adapter_filler.data.mtu.mtu = 185U;
    gateway_adapter_on_ble_event(&adapter_filler, adapter);
    CHECK(server_stage_enqueue_noncritical(&stage, 22U, generation_one));
    CHECK(!server_stage_disconnect(&stage, 22U));
    CHECK(gateway_adapter_host_process_one(adapter, 2U));
    CHECK(server_stage_deliver_one(&stage, adapter));
    CHECK(gateway_adapter_host_process_one(adapter, 3U));

    CHECK(ble_connection_state_connect(&s_ble_connection_state, 23U));
    ble_connection_state_snapshot(&s_ble_connection_state, &snapshot);
    CHECK(ble_connection_state_disconnect(&s_ble_connection_state, 23U));
    CHECK(ble_connection_state_connect(&s_ble_connection_state, 24U));
    ble_connection_state_snapshot(&s_ble_connection_state, &snapshot);
    {
        ble_gatt_server_event_t reconnect =
            ble_event(BLE_GATT_EVENT_CONNECTED, 24U,
                      snapshot.connection_generation);
        add_bonded_identity(&reconnect, 0x24U, true);
        gateway_adapter_on_ble_event(&reconnect, adapter);
        CHECK(gateway_adapter_host_process_one(adapter, 4U));
    }

    delayed_security = ble_event(BLE_GATT_EVENT_SECURITY_CHANGED, 22U,
                                 generation_one);
    add_bonded_identity(&delayed_security, 0x22U, true);
    delayed_security.peer_identity.lookup_error = true;
    delayed_security.data.security.encrypted = true;
    delayed_security.data.security.bonded = true;
    delayed_app_state = ble_event(BLE_GATT_EVENT_APP_STATE, 22U,
                                  generation_one);
    delayed_app_state.data.app_state.active = true;
    delayed_app_state.data.app_state.app_instance_id = 9U;
    gateway_adapter_on_ble_event(&delayed_security, adapter);
    CHECK(gateway_adapter_host_process_one(adapter, 5U));
    gateway_adapter_on_ble_event(&delayed_app_state, adapter);
    CHECK(gateway_adapter_host_process_one(adapter, 6U));

    subscription = ble_event(BLE_GATT_EVENT_SUBSCRIPTION_CHANGED, 24U,
                             snapshot.connection_generation);
    subscription.data.subscription.gateway_status_notify = true;
    mtu = ble_event(BLE_GATT_EVENT_MTU_CHANGED, 24U,
                    snapshot.connection_generation);
    mtu.data.mtu.mtu = 185U;
    security = ble_event(BLE_GATT_EVENT_SECURITY_CHANGED, 24U,
                         snapshot.connection_generation);
    add_bonded_identity(&security, 0x24U, true);
    security.data.security.encrypted = true;
    security.data.security.bonded = true;
    gateway_adapter_on_ble_event(&subscription, adapter);
    CHECK(gateway_adapter_host_process_one(adapter, 7U));
    gateway_adapter_on_ble_event(&mtu, adapter);
    CHECK(gateway_adapter_host_process_one(adapter, 8U));
    gateway_adapter_on_ble_event(&security, adapter);
    CHECK(gateway_adapter_host_process_one(adapter, 9U));
    CHECK(s_gateway_notify_calls == 1U);
    gateway_adapter_get_stats(adapter, &stats);
    CHECK(stats.ble_epoch_invalidations == 1U);
    CHECK(stats.stale_ble_events >= 3U);
    CHECK(!stats.p6_authorization_verified);
    CHECK(stats.production_uart_tx_count == 0U);
    CHECK(gateway_adapter_stop(&adapter) == ESP_OK);
    server_stage_deinit(&stage);
    return true;
}

static bool test_epoch_wrap_and_snapshot_failure_fail_closed(void)
{
    gateway_adapter_t *adapter = start_adapter(4U);
    ble_gatt_server_event_t connected =
        ble_event(BLE_GATT_EVENT_CONNECTED, 25U, 0U);
    ble_connection_snapshot_t snapshot;
    gateway_adapter_stats_t stats;

    CHECK(adapter != NULL);
    s_ble_connection_state.connection_epoch = UINT32_MAX;
    s_ble_connection_state.connection_generation = UINT32_MAX;
    add_bonded_identity(&connected, 0x25U, true);
    CHECK(process_ble_event(adapter, &connected, 1U));
    ble_connection_state_snapshot(&s_ble_connection_state, &snapshot);
    CHECK(snapshot.connection_epoch == 1U);
    CHECK(snapshot.connection_generation == 1U);
    s_snapshot_error = true;
    CHECK(!gateway_adapter_host_process_one(adapter, 2U));
    gateway_adapter_get_stats(adapter, &stats);
    CHECK(stats.ble_snapshot_fail_closed == 1U);
    CHECK(stats.ble_epoch_invalidations == 1U);
    CHECK(!stats.p6_authorization_verified);
    CHECK(!gateway_adapter_p6_authorization_is_current(adapter));
    s_snapshot_error = false;
    ble_connection_state_snapshot(&s_ble_connection_state, &snapshot);
    {
        uint32_t previous_epoch = snapshot.connection_epoch;
        ble_connection_state_set_available(&s_ble_connection_state, false);
        ble_connection_state_set_available(&s_ble_connection_state, true);
        ble_connection_state_snapshot(&s_ble_connection_state, &snapshot);
        CHECK(snapshot.available);
        CHECK(!snapshot.connected);
        CHECK(snapshot.connection_epoch != previous_epoch);
    }
    CHECK(gateway_adapter_stop(&adapter) == ESP_OK);
    return true;
}

static bool test_bcm_door_state_stream_to_android_vehicle_data(void)
{
    const uint32_t device_context_id = UINT32_C(0x10203040);
    const uint64_t session_id = UINT64_C(0x1122334455667788);
    const uint32_t domain_boot_id = UINT32_C(0x55667788);
    gateway_adapter_t *adapter = start_adapter(8U);
    uart_gateway_bridge_t bridge = {.adapter = adapter};
    uart_m_transport_engine_t engine;
    uart_m_transport_rx_message_t door;
    uart_m_transport_feed_result_t feed_result;
    uart_m_message_t decoded;
    gateway_adapter_stats_t stats;
    uint8_t frame[UART_M_MAX_FRAME_SIZE];
    size_t frame_length = 0U;
    uint32_t generation;

    CHECK(adapter != NULL);
    CHECK(establish_vehicle_data_link(adapter, 49U, true, 185U, 1U,
                                      &generation));
    CHECK(install_result_context(adapter, generation, device_context_id,
                                 session_id, domain_boot_id, 5U));
    CHECK(uart_m_transport_engine_init(&engine, 0U,
                                       deliver_uart_to_gateway, &bridge) ==
          UART_M_TRANSPORT_OK);

    make_door_state(&door, device_context_id, session_id, domain_boot_id,
                    UART_M_QUALITY_OK);
    door.received_at_us = INT64_C(7000);
    CHECK(encode_rx_frame(&door.message, 1U, frame, &frame_length));
    CHECK(frame_length == 50U);
    CHECK(uart_m_transport_engine_feed(&engine, frame, frame_length,
                                        door.received_at_us, &feed_result) ==
          UART_M_TRANSPORT_OK);
    CHECK(feed_result.messages_accepted == 1U);
    CHECK(bridge.delivered == 1U);
    CHECK(gateway_adapter_host_process_one(adapter, 7U));
    CHECK(s_vehicle_notify_calls == 1U);
    CHECK(s_vehicle_type == (uint8_t)UART_M_TYPE_BCM_DOOR_STATE);
    CHECK(s_vehicle_payload_length == 40U);
    /* Android MessageCodec reads state at 28..30, quality at 31..33 and age
     * at 38. Keep this independent check beside the ESP32 typed decode. */
    CHECK(s_vehicle_payload[28] == (uint8_t)UART_M_DOOR_LOCKED);
    CHECK(s_vehicle_payload[29] == (uint8_t)UART_M_DOOR_CLOSED);
    CHECK(s_vehicle_payload[30] == (uint8_t)UART_M_DOOR_NORMAL);
    CHECK(s_vehicle_payload[31] == (uint8_t)UART_M_QUALITY_OK);
    CHECK(s_vehicle_payload[32] == (uint8_t)UART_M_QUALITY_OK);
    CHECK(s_vehicle_payload[33] == (uint8_t)UART_M_QUALITY_OK);
    CHECK(s_vehicle_payload[38] == 25U && s_vehicle_payload[39] == 0U);
    CHECK(uart_m_codec_decode(s_vehicle_type, s_vehicle_payload,
                              s_vehicle_payload_length, &decoded) ==
          UART_M_CODEC_OK);
    CHECK(decoded.body.bcm_door_state.source.boot_id ==
          UINT32_C(0x10203040));
    CHECK(decoded.body.bcm_door_state.source.tx_sequence ==
          UINT32_C(0x11223344));
    CHECK(decoded.body.bcm_door_state.update_sequence ==
          UINT32_C(0x55667788));

    make_door_state(&door, device_context_id, session_id, domain_boot_id,
                    UART_M_QUALITY_NO_DATA);
    door.received_at_us = INT64_C(8000);
    CHECK(encode_rx_frame(&door.message, 2U, frame, &frame_length));
    CHECK(uart_m_transport_engine_feed(&engine, frame, frame_length,
                                        door.received_at_us, &feed_result) ==
          UART_M_TRANSPORT_OK);
    CHECK(gateway_adapter_host_process_one(adapter, 8U));
    CHECK(s_vehicle_notify_calls == 2U);
    CHECK(s_vehicle_payload[31] == (uint8_t)UART_M_QUALITY_NO_DATA);
    CHECK(s_vehicle_payload[32] == (uint8_t)UART_M_QUALITY_NO_DATA);
    CHECK(s_vehicle_payload[33] == (uint8_t)UART_M_QUALITY_NO_DATA);
    CHECK(s_vehicle_payload[38] == UINT8_MAX &&
          s_vehicle_payload[39] == UINT8_MAX);

    gateway_adapter_get_stats(adapter, &stats);
    CHECK(stats.vehicle_data_candidates_queued == 2U);
    CHECK(stats.vehicle_data_notify_api_accepted == 2U);
    CHECK(stats.vehicle_data_notify_failures == 0U);
    CHECK(stats.production_uart_tx_count == 0U);
    CHECK(stats.production_m_context_tx_count == 0U);
    CHECK(stats.digital_key_auto_unlock_count == 0U);
    CHECK(gateway_adapter_stop(&adapter) == ESP_OK);
    return true;
}

static bool test_result_vehicle_data_candidate_preserves_sessions(void)
{
    const uint32_t device_context_id = UINT32_C(0x10203040);
    const uint64_t receive_session = UINT64_C(0x1122334455667788);
    const uint64_t request_session = UINT64_C(0x8877665544332211);
    const uint32_t domain_boot_id = UINT32_C(0x55667788);
    gateway_adapter_t *adapter = start_adapter(8U);
    uart_m_transport_rx_message_t result;
    uart_m_message_t decoded;
    gateway_adapter_stats_t stats;
    uint32_t generation;

    CHECK(adapter != NULL);
    CHECK(establish_vehicle_data_link(adapter, 50U, true, 185U, 1U,
                                      &generation));
    CHECK(install_result_context(adapter, generation, device_context_id,
                                 receive_session, domain_boot_id, 5U));
    make_result(&result, device_context_id, receive_session, domain_boot_id,
                request_session, UINT32_C(0xA1B2C3D4));
    gateway_adapter_on_uart_message(&result, adapter);
    CHECK(gateway_adapter_host_process_one(adapter, 7U));

    CHECK(s_vehicle_notify_calls == 1U);
    CHECK(s_vehicle_type == (uint8_t)UART_M_TYPE_RESULT);
    CHECK(s_vehicle_payload_length == 44U);
    CHECK(s_vehicle_mode == BLE_GATT_SEND_NOTIFY);
    CHECK(uart_m_codec_decode(s_vehicle_type, s_vehicle_payload,
                              s_vehicle_payload_length, &decoded) ==
          UART_M_CODEC_OK);
    CHECK(decoded.body.result.context.recipient.session_id == receive_session);
    CHECK(decoded.body.result.request_session == request_session);
    CHECK(decoded.body.result.request_id == UINT32_C(0xA1B2C3D4));
    gateway_adapter_get_stats(adapter, &stats);
    CHECK(stats.vehicle_data_candidates_queued == 1U);
    CHECK(stats.vehicle_data_notify_api_accepted == 1U);
    CHECK(stats.vehicle_data_notify_failures == 0U);
    CHECK(stats.vehicle_data_pending_count == 0U);
    CHECK(stats.production_uart_tx_count == 0U);
    CHECK(stats.production_m_context_tx_count == 0U);
    CHECK(stats.digital_key_auto_unlock_count == 0U);
    CHECK(gateway_adapter_stop(&adapter) == ESP_OK);
    return true;
}

static bool test_result_notify_waits_for_subscription_and_mtu(void)
{
    gateway_adapter_t *adapter = start_adapter(8U);
    uart_m_transport_rx_message_t result;
    ble_gatt_server_event_t subscription;
    ble_gatt_server_event_t mtu;
    gateway_adapter_stats_t stats;
    uint32_t generation;

    CHECK(adapter != NULL);
    CHECK(establish_vehicle_data_link(adapter, 51U, false, 23U, 1U,
                                      &generation));
    CHECK(install_result_context(adapter, generation, UINT32_C(0x01020304),
                                 UINT64_C(0x0102030405060708),
                                 UINT32_C(0x11223344), 5U));
    make_result(&result, UINT32_C(0x01020304),
                UINT64_C(0x0102030405060708), UINT32_C(0x11223344),
                UINT64_C(0x1111111111111111), 9U);
    gateway_adapter_on_uart_message(&result, adapter);
    CHECK(gateway_adapter_host_process_one(adapter, 7U));
    CHECK(s_vehicle_notify_calls == 0U);

    subscription = ble_event(BLE_GATT_EVENT_SUBSCRIPTION_CHANGED, 51U,
                             generation);
    subscription.data.subscription.vehicle_data_notify = true;
    CHECK(process_ble_event(adapter, &subscription, 8U));
    CHECK(s_vehicle_notify_calls == 0U);
    mtu = ble_event(BLE_GATT_EVENT_MTU_CHANGED, 51U, generation);
    mtu.data.mtu.mtu = 185U;
    CHECK(process_ble_event(adapter, &mtu, 9U));
    CHECK(s_vehicle_notify_calls == 1U);
    gateway_adapter_get_stats(adapter, &stats);
    CHECK(stats.vehicle_data_notify_deferred >= 2U);
    CHECK(stats.vehicle_data_notify_api_accepted == 1U);
    CHECK(stats.vehicle_data_pending_count == 0U);
    CHECK(gateway_adapter_stop(&adapter) == ESP_OK);
    return true;
}

static bool test_result_pending_is_not_replayed_after_disconnect(void)
{
    gateway_adapter_t *adapter = start_adapter(8U);
    uart_m_transport_rx_message_t result;
    ble_gatt_server_event_t disconnected;
    gateway_adapter_stats_t stats;
    uint32_t generation;
    uint32_t new_generation;

    CHECK(adapter != NULL);
    CHECK(establish_vehicle_data_link(adapter, 52U, false, 23U, 1U,
                                      &generation));
    CHECK(install_result_context(adapter, generation, UINT32_C(0x11112222),
                                 UINT64_C(0x3333444455556666),
                                 UINT32_C(0x77778888), 5U));
    make_result(&result, UINT32_C(0x11112222),
                UINT64_C(0x3333444455556666), UINT32_C(0x77778888),
                UINT64_C(0x9999AAAABBBBCCCC), 10U);
    gateway_adapter_on_uart_message(&result, adapter);
    CHECK(gateway_adapter_host_process_one(adapter, 7U));
    disconnected = ble_event(BLE_GATT_EVENT_DISCONNECTED, 52U, generation);
    CHECK(process_ble_event(adapter, &disconnected, 8U));
    CHECK(establish_vehicle_data_link(adapter, 53U, true, 185U, 9U,
                                      &new_generation));
    CHECK(new_generation != generation);
    CHECK(s_vehicle_notify_calls == 0U);
    gateway_adapter_get_stats(adapter, &stats);
    CHECK(stats.vehicle_data_candidates_invalidated == 1U);
    CHECK(stats.vehicle_data_pending_count == 0U);
    CHECK(gateway_adapter_stop(&adapter) == ESP_OK);
    return true;
}

static bool test_result_notify_failure_is_not_vehicle_success_or_replayed(void)
{
    gateway_adapter_t *adapter = start_adapter(8U);
    uart_m_transport_rx_message_t result;
    ble_gatt_server_event_t mtu;
    gateway_adapter_stats_t stats;
    uint32_t generation;

    CHECK(adapter != NULL);
    CHECK(establish_vehicle_data_link(adapter, 54U, true, 185U, 1U,
                                      &generation));
    CHECK(install_result_context(adapter, generation, UINT32_C(0xABCDEF01),
                                 UINT64_C(0x0101010102020202),
                                 UINT32_C(0x03030303), 5U));
    s_vehicle_notify_result = ESP_FAIL;
    make_result(&result, UINT32_C(0xABCDEF01),
                UINT64_C(0x0101010102020202), UINT32_C(0x03030303),
                UINT64_C(0xDEADBEEF00000001), 11U);
    gateway_adapter_on_uart_message(&result, adapter);
    CHECK(gateway_adapter_host_process_one(adapter, 7U));
    CHECK(s_vehicle_notify_calls == 1U);
    mtu = ble_event(BLE_GATT_EVENT_MTU_CHANGED, 54U, generation);
    mtu.data.mtu.mtu = 185U;
    CHECK(process_ble_event(adapter, &mtu, 8U));
    CHECK(s_vehicle_notify_calls == 1U);
    gateway_adapter_get_stats(adapter, &stats);
    CHECK(stats.vehicle_data_notify_api_accepted == 0U);
    CHECK(stats.vehicle_data_notify_failures == 1U);
    CHECK(stats.vehicle_data_pending_count == 0U);
    CHECK(stats.production_uart_tx_count == 0U);
    CHECK(stats.production_m_context_tx_count == 0U);
    CHECK(stats.digital_key_auto_unlock_count == 0U);
    CHECK(gateway_adapter_stop(&adapter) == ESP_OK);
    return true;
}

static bool test_result_candidate_queue_full_is_fail_closed(void)
{
    gateway_adapter_t *adapter = start_adapter(16U);
    uart_m_transport_rx_message_t result;
    gateway_adapter_stats_t stats;
    uint32_t generation;
    uint32_t index;

    CHECK(adapter != NULL);
    CHECK(establish_vehicle_data_link(adapter, 55U, false, 23U, 1U,
                                      &generation));
    CHECK(install_result_context(adapter, generation, UINT32_C(0x44445555),
                                 UINT64_C(0x6666777788889999),
                                 UINT32_C(0xAAAABBBB), 5U));
    for (index = 0U; index < 9U; ++index) {
        make_result(&result, UINT32_C(0x44445555),
                    UINT64_C(0x6666777788889999), UINT32_C(0xAAAABBBB),
                    UINT64_C(0x1111222233334444), index + 1U);
        gateway_adapter_on_uart_message(&result, adapter);
        CHECK(gateway_adapter_host_process_one(adapter, 7U + index));
    }
    gateway_adapter_get_stats(adapter, &stats);
    CHECK(stats.vehicle_data_candidates_queued == 8U);
    CHECK(stats.vehicle_data_candidate_queue_full == 1U);
    CHECK(stats.vehicle_data_pending_count == 8U);
    CHECK(stats.core_rejections == 1U);
    CHECK(stats.uart_downlink_held == 1U);
    CHECK(s_vehicle_notify_calls == 0U);
    CHECK(stats.production_uart_tx_count == 0U);
    CHECK(gateway_adapter_stop(&adapter) == ESP_OK);
    return true;
}

#if CONFIG_P6_DEV_REGISTRATION_APPROVAL
static bool establish_secure_peer(gateway_adapter_t *adapter,
                                  uint16_t handle, uint8_t identity_seed,
                                  uint64_t now_ms)
{
    ble_gatt_server_event_t connected =
        ble_event(BLE_GATT_EVENT_CONNECTED, handle, 0U);
    ble_gatt_server_event_t security =
        ble_event(BLE_GATT_EVENT_SECURITY_CHANGED, handle, 0U);
    ble_connection_snapshot_t snapshot;

    add_bonded_identity(&connected, identity_seed, true);
    if (!process_ble_event(adapter, &connected, now_ms)) {
        return false;
    }
    ble_connection_state_snapshot(&s_ble_connection_state, &snapshot);
    security.connection_generation = snapshot.connection_generation;
    add_bonded_identity(&security, identity_seed, true);
    security.data.security.encrypted = true;
    security.data.security.bonded = true;
    return process_ble_event(adapter, &security, now_ms + 1U);
}

static bool test_registration_requires_separate_request_and_approval(void)
{
    gateway_adapter_t *adapter = start_registration_adapter(
        8U, P6_STORE_COMMITTED);
    gateway_adapter_stats_t stats;

    CHECK(adapter != NULL);
    CHECK(establish_secure_peer(adapter, 30U, 0x30U, 1U));
    gateway_adapter_get_stats(adapter, &stats);
    CHECK(!stats.p6_registration_available);
    CHECK(!stats.registration_request_pending);
    CHECK(s_registration_save_calls == 0U);

    CHECK(gateway_adapter_development_approve_registration(adapter) == ESP_OK);
    CHECK(gateway_adapter_host_process_one(adapter, 3U));
    gateway_adapter_get_stats(adapter, &stats);
    CHECK(stats.registration_approval_rejections == 1U);
    CHECK(stats.registration_commits == 0U);
    CHECK(!stats.p6_registration_available);
    CHECK(s_registration_save_calls == 0U);
    CHECK(gateway_adapter_stop(&adapter) == ESP_OK);
    return true;
}

static bool test_registration_valid_approval_commits_once(void)
{
    gateway_adapter_t *adapter = start_registration_adapter(
        8U, P6_STORE_COMMITTED);
    gateway_adapter_stats_t stats;

    CHECK(adapter != NULL);
    CHECK(establish_secure_peer(adapter, 31U, 0x31U, 1U));
    CHECK(gateway_adapter_development_request_registration(adapter) == ESP_OK);
    CHECK(gateway_adapter_host_process_one(adapter, 3U));
    gateway_adapter_get_stats(adapter, &stats);
    CHECK(stats.registration_request_pending);
    CHECK(!stats.p6_registration_available);

    CHECK(gateway_adapter_development_approve_registration(adapter) == ESP_OK);
    CHECK(gateway_adapter_host_process_one(adapter, 4U));
    gateway_adapter_get_stats(adapter, &stats);
    CHECK(!stats.registration_request_pending);
    CHECK(stats.registration_commits == 1U);
    CHECK(stats.p6_registration_available);
    CHECK(!stats.p6_authorization_verified);
    CHECK(s_registration_save_calls == 1U);
    CHECK(s_registration_record.state == P6_REGISTRATION_RECORD_ACTIVE);
    CHECK(stats.production_uart_tx_count == 0U);
    CHECK(stats.production_m_context_tx_count == 0U);
    CHECK(stats.digital_key_auto_unlock_count == 0U);

    CHECK(gateway_adapter_development_approve_registration(adapter) == ESP_OK);
    CHECK(gateway_adapter_host_process_one(adapter, 5U));
    gateway_adapter_get_stats(adapter, &stats);
    CHECK(stats.registration_commits == 1U);
    CHECK(stats.registration_approval_rejections == 1U);
    CHECK(s_registration_save_calls == 1U);
    CHECK(gateway_adapter_stop(&adapter) == ESP_OK);
    return true;
}

static bool test_registration_request_ttl_and_clock_fail_closed(void)
{
    gateway_adapter_t *adapter = start_registration_adapter(
        8U, P6_STORE_COMMITTED);
    gateway_adapter_stats_t stats;

    CHECK(adapter != NULL);
    CHECK(establish_secure_peer(adapter, 41U, 0x41U, 1U));
    mock_idf_set_time_ms(UINT64_C(1000));
    CHECK(gateway_adapter_development_request_registration(adapter) == ESP_OK);
    CHECK(gateway_adapter_host_process_one(adapter, UINT64_C(1000)));
    mock_idf_set_time_ms(UINT64_C(30999));
    CHECK(gateway_adapter_development_approve_registration(adapter) == ESP_OK);
    CHECK(gateway_adapter_host_process_one(adapter, UINT64_C(30999)));
    gateway_adapter_get_stats(adapter, &stats);
    CHECK(stats.registration_commits == 1U);
    CHECK(gateway_adapter_stop(&adapter) == ESP_OK);

    reset_ble_snapshot();
    adapter = start_registration_adapter(8U, P6_STORE_COMMITTED);
    CHECK(adapter != NULL);
    CHECK(establish_secure_peer(adapter, 42U, 0x42U, 1U));
    mock_idf_set_time_ms(UINT64_C(1000));
    CHECK(gateway_adapter_development_request_registration(adapter) == ESP_OK);
    CHECK(gateway_adapter_host_process_one(adapter, UINT64_C(1000)));
    mock_idf_set_time_ms(UINT64_C(31000));
    CHECK(gateway_adapter_development_approve_registration(adapter) == ESP_OK);
    CHECK(gateway_adapter_host_process_one(adapter, UINT64_C(31000)));
    gateway_adapter_get_stats(adapter, &stats);
    CHECK(!stats.registration_request_pending);
    CHECK(stats.registration_commits == 0U);
    CHECK(s_registration_save_calls == 0U);
    CHECK(gateway_adapter_stop(&adapter) == ESP_OK);

    reset_ble_snapshot();
    adapter = start_registration_adapter(8U, P6_STORE_COMMITTED);
    CHECK(adapter != NULL);
    CHECK(establish_secure_peer(adapter, 43U, 0x43U, 1U));
    mock_idf_set_time_ms(UINT64_C(2000));
    CHECK(gateway_adapter_development_request_registration(adapter) == ESP_OK);
    CHECK(gateway_adapter_host_process_one(adapter, UINT64_C(2000)));
    mock_idf_set_time_ms(UINT64_C(1999));
    CHECK(gateway_adapter_development_approve_registration(adapter) == ESP_OK);
    CHECK(gateway_adapter_host_process_one(adapter, UINT64_C(1999)));
    gateway_adapter_get_stats(adapter, &stats);
    CHECK(!stats.registration_request_pending);
    CHECK(stats.registration_commits == 0U);
    CHECK(s_registration_save_calls == 0U);
    CHECK(gateway_adapter_stop(&adapter) == ESP_OK);

    reset_ble_snapshot();
    adapter = start_registration_adapter(8U, P6_STORE_COMMITTED);
    CHECK(adapter != NULL);
    CHECK(establish_secure_peer(adapter, 44U, 0x44U, 1U));
    mock_idf_set_time_ms(UINT64_C(3000));
    CHECK(gateway_adapter_development_request_registration(adapter) == ESP_OK);
    CHECK(gateway_adapter_host_process_one(adapter, UINT64_C(3000)));
    CHECK(!gateway_adapter_host_process_one(adapter, UINT64_MAX));
    gateway_adapter_get_stats(adapter, &stats);
    CHECK(!stats.registration_request_pending);
    CHECK(stats.registration_commits == 0U);
    CHECK(s_registration_save_calls == 0U);
    CHECK(gateway_adapter_stop(&adapter) == ESP_OK);
    return true;
}

static bool test_registration_stale_epoch_and_peer_mismatch_rejected(void)
{
    gateway_adapter_t *adapter = start_registration_adapter(
        8U, P6_STORE_COMMITTED);
    gateway_adapter_stats_t stats;

    CHECK(adapter != NULL);
    CHECK(establish_secure_peer(adapter, 32U, 0x32U, 1U));
    CHECK(gateway_adapter_development_request_registration(adapter) == ESP_OK);
    s_ble_connection_state.connection_epoch++;
    CHECK(gateway_adapter_host_process_one(adapter, 3U));
    gateway_adapter_get_stats(adapter, &stats);
    CHECK(!stats.registration_request_pending);
    CHECK(stats.registration_request_rejections == 1U);
    CHECK(s_registration_save_calls == 0U);
    CHECK(gateway_adapter_stop(&adapter) == ESP_OK);

    reset_ble_snapshot();
    adapter = start_registration_adapter(8U, P6_STORE_COMMITTED);
    CHECK(adapter != NULL);
    CHECK(establish_secure_peer(adapter, 38U, 0x38U, 5U));
    CHECK(gateway_adapter_development_request_registration(adapter) == ESP_OK);
    CHECK(gateway_adapter_host_process_one(adapter, 7U));
    CHECK(gateway_adapter_development_approve_registration(adapter) == ESP_OK);
    CHECK(ble_connection_state_disconnect(&s_ble_connection_state, 38U));
    CHECK(ble_connection_state_connect(&s_ble_connection_state, 39U));
    CHECK(gateway_adapter_host_process_one(adapter, 8U));
    gateway_adapter_get_stats(adapter, &stats);
    CHECK(!stats.registration_request_pending);
    CHECK(stats.registration_approval_rejections == 1U);
    CHECK(s_registration_save_calls == 0U);
    CHECK(gateway_adapter_stop(&adapter) == ESP_OK);

    reset_ble_snapshot();
    adapter = start_registration_adapter(8U, P6_STORE_COMMITTED);
    CHECK(adapter != NULL);
    CHECK(establish_secure_peer(adapter, 33U, 0x33U, 10U));
    CHECK(gateway_adapter_development_request_registration(adapter) == ESP_OK);
    CHECK(gateway_adapter_host_process_one(adapter, 12U));
    {
        ble_gatt_server_event_t changed =
            ble_event(BLE_GATT_EVENT_SECURITY_CHANGED, 33U, 1U);
        add_bonded_identity(&changed, 0x44U, true);
        changed.data.security.encrypted = true;
        changed.data.security.bonded = true;
        CHECK(process_ble_event(adapter, &changed, 13U));
    }
    CHECK(gateway_adapter_development_approve_registration(adapter) == ESP_OK);
    CHECK(gateway_adapter_host_process_one(adapter, 14U));
    gateway_adapter_get_stats(adapter, &stats);
    CHECK(!stats.p6_registration_available);
    CHECK(!stats.registration_request_pending);
    CHECK(stats.registration_approval_rejections >= 1U);
    CHECK(s_registration_save_calls == 0U);
    CHECK(gateway_adapter_stop(&adapter) == ESP_OK);
    return true;
}

static bool test_registration_disconnect_commit_failure_and_existing_rejected(void)
{
    gateway_adapter_t *adapter = start_registration_adapter(
        8U, P6_STORE_COMMIT_UNKNOWN);
    gateway_adapter_stats_t stats;

    CHECK(adapter != NULL);
    CHECK(establish_secure_peer(adapter, 34U, 0x34U, 1U));
    CHECK(gateway_adapter_development_request_registration(adapter) == ESP_OK);
    CHECK(gateway_adapter_host_process_one(adapter, 3U));
    CHECK(gateway_adapter_development_approve_registration(adapter) == ESP_OK);
    CHECK(gateway_adapter_host_process_one(adapter, 4U));
    gateway_adapter_get_stats(adapter, &stats);
    CHECK(!stats.p6_registration_available);
    CHECK(!stats.registration_request_pending);
    CHECK(stats.registration_commits == 0U);
    CHECK(s_registration_save_calls == 1U);
    CHECK(gateway_adapter_stop(&adapter) == ESP_OK);

    reset_ble_snapshot();
    adapter = start_registration_adapter(8U, P6_STORE_COMMITTED);
    CHECK(adapter != NULL);
    CHECK(establish_secure_peer(adapter, 35U, 0x35U, 10U));
    CHECK(gateway_adapter_development_request_registration(adapter) == ESP_OK);
    CHECK(gateway_adapter_host_process_one(adapter, 12U));
    {
        ble_gatt_server_event_t disconnected =
            ble_event(BLE_GATT_EVENT_DISCONNECTED, 35U, 1U);
        CHECK(process_ble_event(adapter, &disconnected, 13U));
    }
    CHECK(gateway_adapter_development_approve_registration(adapter) ==
          ESP_ERR_INVALID_STATE);
    gateway_adapter_get_stats(adapter, &stats);
    CHECK(!stats.registration_request_pending);
    CHECK(s_registration_save_calls == 0U);
    CHECK(gateway_adapter_stop(&adapter) == ESP_OK);

    reset_ble_snapshot();
    adapter = start_adapter_with_registration(8U, 0x36U);
    CHECK(adapter != NULL);
    CHECK(establish_secure_peer(adapter, 36U, 0x36U, 20U));
    CHECK(gateway_adapter_development_request_registration(adapter) == ESP_OK);
    CHECK(gateway_adapter_host_process_one(adapter, 22U));
    gateway_adapter_get_stats(adapter, &stats);
    CHECK(stats.registration_request_rejections == 1U);
    CHECK(!stats.registration_request_pending);
    CHECK(s_registration_save_calls == 0U);
    CHECK(gateway_adapter_stop(&adapter) == ESP_OK);
    return true;
}

static bool test_registration_queue_full_invalidates_pending(void)
{
    gateway_adapter_t *adapter = start_registration_adapter(
        1U, P6_STORE_COMMITTED);
    ble_gatt_server_event_t filler =
        ble_event(BLE_GATT_EVENT_MTU_CHANGED, 37U, 1U);
    gateway_adapter_stats_t stats;

    CHECK(adapter != NULL);
    CHECK(establish_secure_peer(adapter, 37U, 0x37U, 1U));
    CHECK(gateway_adapter_development_request_registration(adapter) == ESP_OK);
    CHECK(gateway_adapter_host_process_one(adapter, 3U));
    filler.data.mtu.mtu = 185U;
    gateway_adapter_on_ble_event(&filler, adapter);
    CHECK(gateway_adapter_development_approve_registration(adapter) ==
          ESP_ERR_TIMEOUT);
    CHECK(gateway_adapter_host_process_one(adapter, 4U));
    gateway_adapter_get_stats(adapter, &stats);
    CHECK(stats.registration_input_queue_failures == 1U);
    CHECK(!stats.registration_request_pending);
    CHECK(!stats.p6_registration_available);
    CHECK(s_registration_save_calls == 0U);
    CHECK(gateway_adapter_stop(&adapter) == ESP_OK);
    return true;
}
#endif

static bool establish_monitor_context(gateway_adapter_t *adapter,
                                      bool subscribed, uint32_t *generation,
                                      gateway_core_context_t *context)
{
    uart_m_transport_rx_message_t alive;
    ble_gatt_server_event_t subscription;

    reset_ble_snapshot();
    s_vehicle_notify_calls = 0U;
    s_vehicle_notify_result = ESP_OK;
    s_notify_result = ESP_OK;
    gateway_adapter_host_enable_monitor_binding(adapter);
    CHECK(reconcile_registered_bond(adapter, 0x70U, 1U));
    CHECK(!gateway_adapter_host_get_monitor_context(adapter, context));
    CHECK(establish_vehicle_data_link(adapter, 70U, subscribed, 185U,
                                      2U, generation));
    if (subscribed) {
        subscription = ble_event(BLE_GATT_EVENT_SUBSCRIPTION_CHANGED,
                                  70U, *generation);
        subscription.data.subscription.gateway_status_notify = true;
        subscription.data.subscription.vehicle_data_notify = true;
        CHECK(process_ble_event(adapter, &subscription, 5U));
    }
    CHECK(!gateway_adapter_host_get_monitor_context(adapter, context));
    make_alive(&alive, UINT32_C(0x12345678));
    alive.received_at_us = INT64_C(6000);
    gateway_adapter_on_uart_message(&alive, adapter);
    CHECK(gateway_adapter_host_process_one(adapter, 6U));
    CHECK(gateway_adapter_host_get_monitor_context(adapter, context));
    CHECK(context->identity.verified && context->authorization_verified);
    CHECK(context->registered && context->connected && context->encrypted);
    CHECK(!context->authenticated && !context->session_ready);
    CHECK(context->connection_generation == *generation);
    CHECK(context->identity.device_context_id == UINT32_C(0x11223344));
    CHECK(context->identity.session_id != 0U);
    CHECK(s_status[1] == 1U && s_status[3] == 0U);
    CHECK(read_u32_le(&s_status[4]) == context->identity.device_context_id);
    CHECK(read_u32_le(&s_status[8]) == (uint32_t)context->identity.session_id);
    CHECK(read_u32_le(&s_status[12]) ==
          (uint32_t)(context->identity.session_id >> 32U));
    CHECK(read_u32_le(&s_status[16]) == UINT32_C(0x12345678));
    return true;
}

static bool test_monitor_snapshot_runtime_notify_and_command_gate(void)
{
    gateway_adapter_t *adapter = start_adapter_with_registration(8U, 0x70U);
    gateway_core_context_t context;
    gateway_adapter_stats_t stats;
    uart_m_transport_rx_message_t door;
    ble_gatt_server_event_t command;
    uint32_t generation;

    CHECK(adapter != NULL);
    CHECK(establish_monitor_context(adapter, true, &generation, &context));
    make_door_state(&door, context.identity.device_context_id,
                    context.identity.session_id, UINT32_C(0x12345678),
                    UART_M_QUALITY_STALE);
    door.received_at_us = INT64_C(7000);
    gateway_adapter_on_uart_message(&door, adapter);
    CHECK(gateway_adapter_host_process_one(adapter, 7U));
    CHECK(s_vehicle_notify_calls == 1U);
    CHECK(s_vehicle_payload[31] == (uint8_t)UART_M_QUALITY_STALE);
    CHECK(s_vehicle_payload[38] == UINT8_MAX);
    /* An older recipient Session is rejected, rather than rewritten. */
    door.message.body.bcm_door_state.context.recipient.session_id ^= UINT64_C(1);
    door.received_at_us = INT64_C(8000);
    gateway_adapter_on_uart_message(&door, adapter);
    CHECK(gateway_adapter_host_process_one(adapter, 8U));
    CHECK(s_vehicle_notify_calls == 1U);
    command = ble_event(BLE_GATT_EVENT_APP_COMMAND, 70U, generation);
    command.data.app_command.message_type = (uint8_t)UART_M_TYPE_REQUEST;
    command.data.app_command.payload_length = 12U;
    command.data.app_command.payload[0] = 1U;
    command.data.app_command.payload[4] = 1U;
    CHECK(process_ble_event(adapter, &command, 9U));
    gateway_adapter_get_stats(adapter, &stats);
    CHECK(stats.app_commands_blocked == 1U);
    CHECK(stats.production_uart_tx_count == 0U);
    CHECK(stats.production_m_context_tx_count == 0U);
    CHECK(stats.digital_key_auto_unlock_count == 0U);
    CHECK(mock_idf_uart_write_count() == 0U);
    CHECK(gateway_adapter_stop(&adapter) == ESP_OK);
    return true;
}

static bool test_monitor_snapshot_fail_closed_and_boot_link_invalidation(void)
{
    gateway_adapter_t *adapter = start_adapter(8U);
    gateway_core_context_t context;
    gateway_adapter_stats_t stats;
    uart_m_transport_rx_message_t alive;
    uart_m_transport_rx_message_t door;
    uint32_t generation;

    CHECK(adapter != NULL);
    gateway_adapter_host_enable_monitor_binding(adapter);
    CHECK(!gateway_adapter_host_get_monitor_context(adapter, &context));
    CHECK(establish_vehicle_data_link(adapter, 70U, true, 185U, 1U,
                                      &generation));
    CHECK(!gateway_adapter_host_get_monitor_context(adapter, &context));
    CHECK(s_vehicle_notify_calls == 0U);
    CHECK(read_u32_le(&s_status[4]) == 0U);
    CHECK(read_u32_le(&s_status[8]) == 0U);
    CHECK(read_u32_le(&s_status[12]) == 0U);
    CHECK(gateway_adapter_stop(&adapter) == ESP_OK);

    adapter = start_adapter_with_registration(8U, 0x70U);
    CHECK(adapter != NULL);
    CHECK(establish_monitor_context(adapter, false, &generation, &context));
    make_door_state(&door, context.identity.device_context_id,
                    context.identity.session_id, UINT32_C(0x12345678),
                    UART_M_QUALITY_OK);
    door.received_at_us = INT64_C(7000);
    gateway_adapter_on_uart_message(&door, adapter);
    CHECK(gateway_adapter_host_process_one(adapter, 7U));
    gateway_adapter_get_stats(adapter, &stats);
    CHECK(stats.vehicle_data_pending_count == 1U);
    make_alive(&alive, UINT32_C(0x87654321));
    alive.received_at_us = INT64_C(8000);
    gateway_adapter_on_uart_message(&alive, adapter);
    CHECK(gateway_adapter_host_process_one(adapter, 8U));
    CHECK(!gateway_adapter_host_get_monitor_context(adapter, &context));
    gateway_adapter_get_stats(adapter, &stats);
    CHECK(stats.vehicle_data_pending_count == 0U);
    CHECK(!stats.p6_authorization_verified);
    CHECK(read_u32_le(&s_status[4]) == 0U);
    CHECK(read_u32_le(&s_status[8]) == 0U);
    CHECK(read_u32_le(&s_status[12]) == 0U);
    CHECK(gateway_adapter_stop(&adapter) == ESP_OK);

    adapter = start_adapter_with_registration(8U, 0x70U);
    CHECK(adapter != NULL);
    CHECK(establish_monitor_context(adapter, false, &generation, &context));
    CHECK(gateway_adapter_host_tick(adapter, 757U));
    CHECK(!gateway_adapter_host_get_monitor_context(adapter, &context));
    make_alive(&alive, UINT32_C(0x12345678));
    alive.received_at_us = INT64_C(758000);
    gateway_adapter_on_uart_message(&alive, adapter);
    CHECK(gateway_adapter_host_process_one(adapter, 758U));
    CHECK(!gateway_adapter_host_get_monitor_context(adapter, &context));
    CHECK(s_vehicle_notify_calls == 0U);
    CHECK(gateway_adapter_stop(&adapter) == ESP_OK);
    return true;
}

static bool test_monitor_snapshot_session_and_generation_invalidation(void)
{
    gateway_adapter_t *adapter = start_adapter_with_registration(8U, 0x70U);
    gateway_core_context_t context;
    ble_gatt_server_event_t app;
    ble_connection_snapshot_t connection;
    uint32_t generation;

    CHECK(adapter != NULL);
    CHECK(establish_monitor_context(adapter, false, &generation, &context));
    app = ble_event(BLE_GATT_EVENT_APP_STATE, 70U, generation);
    app.data.app_state.app_instance_id = 1U;
    app.data.app_state.active = true;
    CHECK(process_ble_event(adapter, &app, 7U));
    CHECK(gateway_adapter_host_get_monitor_context(adapter, &context));
    app.data.app_state.app_instance_id = 2U;
    CHECK(process_ble_event(adapter, &app, 8U));
    CHECK(!gateway_adapter_host_get_monitor_context(adapter, &context));
    CHECK(read_u32_le(&s_status[4]) == 0U);
    CHECK(read_u32_le(&s_status[8]) == 0U);
    CHECK(read_u32_le(&s_status[12]) == 0U);
    CHECK(process_ble_event(adapter, &app, 9U));
    CHECK(!gateway_adapter_host_get_monitor_context(adapter, &context));
    CHECK(gateway_adapter_stop(&adapter) == ESP_OK);

    adapter = start_adapter_with_registration(8U, 0x70U);
    CHECK(adapter != NULL);
    CHECK(establish_monitor_context(adapter, false, &generation, &context));
    CHECK(ble_connection_state_disconnect(&s_ble_connection_state, 70U));
    CHECK(ble_connection_state_connect(&s_ble_connection_state, 70U));
    ble_connection_state_snapshot(&s_ble_connection_state, &connection);
    CHECK(connection.connection_generation != generation);
    CHECK(!gateway_adapter_host_process_one(adapter, 7U));
    CHECK(!gateway_adapter_host_get_monitor_context(adapter, &context));
    CHECK(gateway_adapter_host_tick(adapter, 7U));
    CHECK(!gateway_adapter_host_get_monitor_context(adapter, &context));
    CHECK(s_vehicle_notify_calls == 0U);
    CHECK(gateway_adapter_stop(&adapter) == ESP_OK);

    adapter = start_adapter_with_registration(8U, 0x70U);
    CHECK(adapter != NULL);
    CHECK(establish_monitor_context(adapter, false, &generation, &context));
    app = ble_event(BLE_GATT_EVENT_SECURITY_CHANGED, 70U, generation);
    add_bonded_identity(&app, 0x70U, true);
    app.data.security.bonded = true;
    app.data.security.encrypted = false;
    CHECK(process_ble_event(adapter, &app, 7U));
    CHECK(!gateway_adapter_host_get_monitor_context(adapter, &context));
    CHECK(s_vehicle_notify_calls == 0U);
    CHECK(gateway_adapter_stop(&adapter) == ESP_OK);
    return true;
}

static bool test_monitor_status_precedes_vehicle_notify(void)
{
    gateway_adapter_t *adapter = start_adapter_with_registration(8U, 0x70U);
    gateway_core_context_t context;
    gateway_adapter_stats_t stats;
    uart_m_transport_rx_message_t door;
    ble_gatt_server_event_t subscription;
    ble_gatt_server_event_t mtu;
    uint32_t generation;

    CHECK(adapter != NULL);
    CHECK(establish_monitor_context(adapter, false, &generation, &context));
    make_door_state(&door, context.identity.device_context_id,
                    context.identity.session_id, UINT32_C(0x12345678),
                    UART_M_QUALITY_OK);
    door.received_at_us = INT64_C(7000);
    gateway_adapter_on_uart_message(&door, adapter);
    CHECK(gateway_adapter_host_process_one(adapter, 7U));
    CHECK(s_vehicle_notify_calls == 0U);

    subscription = ble_event(BLE_GATT_EVENT_SUBSCRIPTION_CHANGED, 70U, generation);
    subscription.data.subscription.gateway_status_notify = true;
    subscription.data.subscription.vehicle_data_notify = true;
    s_notify_result = ESP_FAIL;
    CHECK(process_ble_event(adapter, &subscription, 8U));
    CHECK(s_vehicle_notify_calls == 0U);
    gateway_adapter_get_stats(adapter, &stats);
    CHECK(stats.vehicle_data_pending_count == 1U);

    s_notify_result = ESP_OK;
    mtu = ble_event(BLE_GATT_EVENT_MTU_CHANGED, 70U, generation);
    mtu.data.mtu.mtu = 185U;
    CHECK(process_ble_event(adapter, &mtu, 9U));
    CHECK(s_vehicle_notify_calls == 1U);
    gateway_adapter_get_stats(adapter, &stats);
    CHECK(stats.vehicle_data_pending_count == 0U);
    CHECK(mock_idf_uart_write_count() == 0U);
    CHECK(gateway_adapter_stop(&adapter) == ESP_OK);
    return true;
}

static bool bind_context_test_policy(gateway_adapter_t *adapter,
                                      bool approved, uint64_t now_ms)
{
    gateway_monitor_context_tx_policy_t policy = {0};
    policy.approved = approved;
    policy.refresh_period_ms = 10U;
    policy.queued_context_ttl_ms = 100U;
    policy.sources.registration_connection_ttl_ms = 100U;
    policy.sources.app_ttl_ms = 100U;
    CHECK(gateway_adapter_bind_monitor_context_tx(adapter,
        (uart_m_transport_t *)&s_context_transport_token, &policy) == ESP_OK);
    memset(&policy, 0, sizeof(policy)); /* Queued policy must be a deep copy. */
    CHECK(gateway_adapter_host_process_one(adapter, now_ms));
    return true;
}

static bool test_monitor_context_tx_policy_freshness_backpressure(void)
{
    gateway_adapter_t *adapter = start_adapter_with_registration(8U, 0x70U);
    gateway_core_context_t context;
    gateway_adapter_stats_t stats;
    ble_gatt_server_event_t app;
    uart_m_transport_rx_message_t alive;
    uint32_t generation;

    CHECK(adapter != NULL);
    mock_idf_set_time_ms(0U);
    CHECK(bind_context_test_policy(adapter, true, 0U));
    CHECK(establish_monitor_context(adapter, true, &generation, &context));
    CHECK(s_context_send_calls == 0U); /* No real App source yet. */
    app = ble_event(BLE_GATT_EVENT_APP_STATE, 70U, generation);
    app.data.app_state.app_instance_id = 1U;
    app.data.app_state.active = true;
    s_context_send_result = UART_M_TRANSPORT_NO_CAPACITY;
    mock_idf_set_time_ms(7U);
    CHECK(process_ble_event(adapter, &app, 7U));
    CHECK(s_context_send_calls == 1U);
    gateway_adapter_get_stats(adapter, &stats);
    CHECK(stats.monitor_context_tx_queued == 0U);
    CHECK(stats.monitor_context_tx_backpressure == 1U);
    CHECK(gateway_adapter_host_tick(adapter, 8U));
    CHECK(s_context_send_calls == 1U); /* No busy-loop retry. */
    s_context_send_result = UART_M_TRANSPORT_OK;
    CHECK(gateway_adapter_host_tick(adapter, 17U));
    CHECK(s_context_send_calls == 2U);
    CHECK(s_context_sent.type == UART_M_TYPE_CONTEXT);
    CHECK(s_context_sent.body.context.context.device_context_id == read_u32_le(&s_status[4]));
    CHECK((uint32_t)s_context_sent.body.context.context.session_id == read_u32_le(&s_status[8]));
    CHECK((uint32_t)(s_context_sent.body.context.context.session_id >> 32U) == read_u32_le(&s_status[12]));
    CHECK(s_context_sent.body.context.app_update_sequence == 1U);
    CHECK(s_context_sent.body.context.app_age_ms == 10U);
    /* The existing link fixture captures Security at mock clock zero. */
    CHECK(s_context_sent.body.context.registration_connection_age_ms == 17U);
    CHECK(s_context_sent.body.context.proximity_validity == UART_M_VALIDITY_INVALID);
    CHECK(s_context_sent.body.context.proximity_reason == UART_M_QUALITY_REASON_NO_DATA);
    CHECK(gateway_adapter_host_tick(adapter, 107U));
    CHECK(s_context_send_calls == 2U); /* Expired sources cannot be sent. */
    make_alive(&alive, UINT32_C(0x87654321));
    alive.received_at_us = INT64_C(108000);
    gateway_adapter_on_uart_message(&alive, adapter);
    CHECK(gateway_adapter_host_process_one(adapter, 108U));
    CHECK(!gateway_adapter_host_get_monitor_context(adapter, &context));
    CHECK(s_context_send_calls == 2U);
    CHECK(s_non_context_send_calls == 0U);
    CHECK(s_context_cancel_calls > 0U);
    CHECK(mock_idf_uart_write_count() == 0U);
    CHECK(gateway_adapter_stop(&adapter) == ESP_OK);
    return true;
}

static bool test_monitor_context_tx_closed_and_session_invalidation(void)
{
    gateway_adapter_t *adapter;
    gateway_core_context_t context;
    ble_gatt_server_event_t app;
    uint32_t generation;
    unsigned scenario;

    for (scenario = 0U; scenario < 4U; ++scenario) {
        s_context_cancel_calls = 0U;
        mock_idf_set_time_ms(0U);
        s_context_send_calls = 0U;
        adapter = start_adapter_with_registration(8U, 0x70U);
        CHECK(adapter != NULL);
        CHECK(bind_context_test_policy(adapter, scenario != 0U, 0U));
        CHECK(establish_monitor_context(adapter, true, &generation, &context));
        app = ble_event(BLE_GATT_EVENT_APP_STATE, 70U, generation);
        app.data.app_state.app_instance_id = 1U;
        app.data.app_state.active = true;
        CHECK(process_ble_event(adapter, &app, 7U));
        CHECK(s_context_send_calls == (scenario == 0U ? 0U : 1U));
        if (scenario == 1U) {
            app.data.app_state.app_instance_id = 2U;
            CHECK(process_ble_event(adapter, &app, 8U));
            CHECK(gateway_adapter_host_tick(adapter, 18U));
            CHECK(!gateway_adapter_host_get_monitor_context(adapter, &context));
            CHECK(s_context_send_calls == 1U);
        } else if (scenario == 2U) {
            uart_m_transport_rx_message_t alive;
            CHECK(gateway_adapter_host_tick(adapter, 757U));
            CHECK(!gateway_adapter_host_get_monitor_context(adapter, &context));
            make_alive(&alive, UINT32_C(0x12345678));
            alive.received_at_us = INT64_C(758000);
            gateway_adapter_on_uart_message(&alive, adapter);
            CHECK(gateway_adapter_host_process_one(adapter, 758U));
            CHECK(s_context_send_calls == 1U);
        } else if (scenario == 3U) {
            CHECK(ble_connection_state_disconnect(&s_ble_connection_state, 70U));
            CHECK(ble_connection_state_connect(&s_ble_connection_state, 70U));
            CHECK(!gateway_adapter_host_process_one(adapter, 18U));
            CHECK(gateway_adapter_host_tick(adapter, 18U));
            CHECK(!gateway_adapter_host_get_monitor_context(adapter, &context));
            CHECK(s_context_send_calls == 1U);
        }
        CHECK(s_non_context_send_calls == 0U);
        if (scenario != 0U) CHECK(s_context_cancel_calls > 0U);
        CHECK(gateway_adapter_stop(&adapter) == ESP_OK);
    }
    return true;
}

static bool test_monitor_context_bind_loss_is_fail_closed(void)
{
    gateway_adapter_t *adapter = start_adapter_with_registration(1U, 0x70U);
    gateway_monitor_context_tx_policy_t policy = {0};
    gateway_core_context_t context;
    ble_gatt_server_event_t app;
    uint32_t generation;

    CHECK(adapter != NULL);
    policy.approved = true;
    CHECK(gateway_adapter_bind_monitor_context_tx(adapter,
        (uart_m_transport_t *)&s_context_transport_token, &policy) == ESP_ERR_INVALID_ARG);
    policy.refresh_period_ms = 10U;
    policy.queued_context_ttl_ms = 100U;
    policy.sources.registration_connection_ttl_ms = 100U;
    policy.sources.app_ttl_ms = 100U;
    CHECK(gateway_adapter_bind_monitor_context_tx(adapter,
        (uart_m_transport_t *)&s_context_transport_token, &policy) == ESP_OK);
    /* A later close request is lost; the earlier queued approval cannot
     * clear the fault or resurrect TX when the queue drains. */
    CHECK(gateway_adapter_bind_monitor_context_tx(adapter,
        (uart_m_transport_t *)&s_context_transport_token, NULL) == ESP_ERR_NO_MEM);
    CHECK(gateway_adapter_host_process_one(adapter, 0U));
    CHECK(establish_monitor_context(adapter, true, &generation, &context));
    app = ble_event(BLE_GATT_EVENT_APP_STATE, 70U, generation);
    app.data.app_state.app_instance_id = 1U;
    app.data.app_state.active = true;
    CHECK(process_ble_event(adapter, &app, 7U));
    CHECK(gateway_adapter_host_tick(adapter, 17U));
    CHECK(s_context_send_calls == 0U);
    CHECK(gateway_adapter_stop(&adapter) == ESP_OK);
    return true;
}

static ble_gatt_server_event_t peer_observation(uint32_t generation,
                                                uint64_t sampled_at_ms)
{
    ble_connection_snapshot_t snapshot;
    ble_gatt_server_event_t event = ble_event(BLE_GATT_EVENT_PEER_OBSERVED,
                                              70U, generation);
    ble_connection_state_snapshot(&s_ble_connection_state, &snapshot);
    event.observation_epoch = snapshot.connection_epoch;
    event.observed_at_ms = sampled_at_ms;
    add_bonded_identity(&event, 0x70U, true);
    event.data.security.encrypted = true;
    event.data.security.bonded = true;
    return event;
}

static bool test_monitor_live_observation_source_ages(void)
{
    gateway_adapter_t *adapter = start_adapter_with_registration(8U, 0x70U);
    gateway_core_context_t core;
    uart_m_context_t context;
    uart_m_transport_rx_message_t alive;
    ble_gatt_server_event_t event;
    uint32_t generation;

    CHECK(adapter != NULL);
    CHECK(establish_monitor_context(adapter, true, &generation, &core));
    gateway_adapter_host_enable_monitor_observation(adapter);
    CHECK(gateway_adapter_bind_monitor_context_tx(adapter,
        (uart_m_transport_t *)&s_context_transport_token, NULL) == ESP_OK);
    CHECK(gateway_adapter_host_process_one(adapter, 7U));
    event = ble_event(BLE_GATT_EVENT_APP_STATE, 70U, generation);
    event.data.app_state.app_instance_id = 1U;
    event.data.app_state.active = true;
    mock_idf_set_time_ms(8U);
    CHECK(process_ble_event(adapter, &event, 8U));
    CHECK(gateway_adapter_host_tick(adapter, 199U));
    CHECK(gateway_adapter_host_get_context_candidate(adapter, &context));
    CHECK(context.registration_connection_quality == UART_M_QUALITY_NO_DATA);
    CHECK(context.app_age_ms == 191U);

    event = peer_observation(generation, 200U);
    CHECK(process_ble_event(adapter, &event, 220U));
    CHECK(gateway_adapter_host_get_context_candidate(adapter, &context));
    CHECK(context.registration_connection_quality == UART_M_QUALITY_OK);
    CHECK(context.registration_connection_update_sequence == 1U);
    CHECK(context.registration_connection_age_ms == 20U); /* Host sample, not dispatch time. */
    CHECK(context.app_age_ms == 212U && context.app_update_sequence == 1U);
    CHECK(gateway_adapter_host_tick(adapter, 400U));
    CHECK(gateway_adapter_host_get_context_candidate(adapter, &context));
    CHECK(context.registration_connection_update_sequence == 1U);
    CHECK(context.registration_connection_age_ms == 200U); /* Timer cannot refresh it. */
    event = peer_observation(generation, 400U);
    CHECK(process_ble_event(adapter, &event, 410U));
    CHECK(gateway_adapter_host_get_context_candidate(adapter, &context));
    CHECK(context.registration_connection_update_sequence == 2U);
    CHECK(context.registration_connection_age_ms == 10U);
    CHECK(context.app_update_sequence == 1U && context.app_age_ms == 402U);
    make_alive(&alive, UINT32_C(0x12345678));
    alive.received_at_us = INT64_C(700000);
    gateway_adapter_on_uart_message(&alive, adapter);
    CHECK(gateway_adapter_host_process_one(adapter, 700U));
    CHECK(gateway_adapter_host_tick(adapter, 1000U));
    CHECK(gateway_adapter_host_get_context_candidate(adapter, &context));
    CHECK(context.registration_connection_quality == UART_M_QUALITY_STALE);
    CHECK(context.registration_connection_age_ms == 600U);
    CHECK(gateway_adapter_host_tick(adapter, 1008U));
    CHECK(gateway_adapter_host_get_context_candidate(adapter, &context));
    CHECK(context.app_quality == UART_M_QUALITY_STALE && context.app_age_ms == 1000U);
    CHECK(context.proximity_validity == UART_M_VALIDITY_INVALID);
    CHECK(context.proximity_reason == UART_M_QUALITY_REASON_NO_DATA);
    CHECK(s_status[3] == 0U);
    CHECK(s_context_send_calls == 0U && s_non_context_send_calls == 0U);
    CHECK(gateway_adapter_stop(&adapter) == ESP_OK);
    return true;
}

static bool test_monitor_live_observation_rejects_stale_or_untrusted(void)
{
    gateway_adapter_t *adapter = start_adapter_with_registration(8U, 0x70U);
    gateway_core_context_t core;
    gateway_adapter_stats_t stats;
    ble_gatt_server_event_t event;
    uint32_t generation;

    CHECK(adapter != NULL);
    CHECK(establish_monitor_context(adapter, true, &generation, &core));
    gateway_adapter_host_enable_monitor_observation(adapter);
    event = peer_observation(generation, 200U);
    CHECK(process_ble_event(adapter, &event, 200U));
    CHECK(process_ble_event(adapter, &event, 201U)); /* Duplicate sample. */
    event.observation_epoch++;
    event.observed_at_ms = 400U;
    CHECK(process_ble_event(adapter, &event, 400U));
    CHECK(gateway_adapter_host_get_monitor_context(adapter, &core));
    gateway_adapter_get_stats(adapter, &stats);
    CHECK(stats.monitor_peer_observations_accepted == 1U);
    CHECK(stats.monitor_peer_observations_rejected == 2U);
    event = peer_observation(generation, 401U);
    add_bonded_identity(&event, 0x71U, true); /* Existing Bond is not the registered peer. */
    CHECK(process_ble_event(adapter, &event, 401U));
    CHECK(!gateway_adapter_host_get_monitor_context(adapter, &core));
    event = peer_observation(generation, 402U);
    CHECK(process_ble_event(adapter, &event, 402U));
    CHECK(!gateway_adapter_host_get_monitor_context(adapter, &core));
    CHECK(s_context_send_calls == 0U);
    CHECK(gateway_adapter_stop(&adapter) == ESP_OK);
    reset_ble_snapshot();
    adapter = start_adapter(8U);
    CHECK(adapter != NULL);
    CHECK(establish_vehicle_data_link(adapter, 70U, true, 185U, 2U, &generation));
    gateway_adapter_host_enable_monitor_binding(adapter);
    gateway_adapter_host_enable_monitor_observation(adapter);
    event = peer_observation(generation, 200U);
    CHECK(process_ble_event(adapter, &event, 200U));
    gateway_adapter_get_stats(adapter, &stats);
    CHECK(stats.monitor_peer_observations_accepted == 0U);
    CHECK(!stats.p6_authorization_verified);
    CHECK(!gateway_adapter_host_get_monitor_context(adapter, &core));
    CHECK(gateway_adapter_stop(&adapter) == ESP_OK);
    return true;
}

static bool test_monitor_observation_during_security_pending(void)
{
    gateway_adapter_t *adapter = start_adapter_with_registration(8U, 0x70U);
    gateway_core_context_t context;
    ble_connection_snapshot_t connection;
    ble_gatt_server_event_t event;
    uart_m_transport_rx_message_t alive;

    CHECK(adapter != NULL);
    reset_ble_snapshot();
    gateway_adapter_host_enable_monitor_binding(adapter);
    gateway_adapter_host_enable_monitor_observation(adapter);
    CHECK(reconcile_registered_bond(adapter, 0x70U, 1U));
    event = ble_event(BLE_GATT_EVENT_CONNECTED, 70U, 0U);
    add_bonded_identity(&event, 0x70U, true);
    CHECK(process_ble_event(adapter, &event, 2U));
    ble_connection_state_snapshot(&s_ble_connection_state, &connection);
    event = peer_observation(connection.connection_generation, 200U);
    event.data.security.encrypted = false;
    CHECK(process_ble_event(adapter, &event, 200U));
    CHECK(!gateway_adapter_host_get_monitor_context(adapter, &context));
    event = ble_event(BLE_GATT_EVENT_SECURITY_CHANGED, 70U,
                     connection.connection_generation);
    add_bonded_identity(&event, 0x70U, true);
    event.data.security.encrypted = true;
    event.data.security.bonded = true;
    CHECK(process_ble_event(adapter, &event, 201U));
    make_alive(&alive, UINT32_C(0x12345678));
    alive.received_at_us = INT64_C(202000);
    gateway_adapter_on_uart_message(&alive, adapter);
    CHECK(gateway_adapter_host_process_one(adapter, 202U));
    CHECK(gateway_adapter_host_get_monitor_context(adapter, &context));
    CHECK(!context.session_ready);
    CHECK(s_context_send_calls == 0U);
    CHECK(gateway_adapter_stop(&adapter) == ESP_OK);
    return true;
}

static bool test_monitor_operating_policy_context_runtime(void)
{
    gateway_adapter_t *adapter = start_adapter_with_registration(8U, 0x70U);
    gateway_core_context_t core;
    gateway_monitor_context_tx_policy_t policy = {0};
    ble_gatt_server_event_t event;
    uart_m_transport_rx_message_t alive;
    uint32_t generation;

    CHECK(adapter != NULL);
    CHECK(establish_monitor_context(adapter, true, &generation, &core));
    gateway_adapter_host_enable_monitor_observation(adapter);
    policy.approved = true;
    policy.refresh_period_ms = GATEWAY_MONITOR_CONTEXT_PERIOD_MS;
    policy.queued_context_ttl_ms = GATEWAY_MONITOR_REGISTRATION_CONNECTION_TTL_MS;
    policy.sources.registration_connection_ttl_ms = GATEWAY_MONITOR_REGISTRATION_CONNECTION_TTL_MS;
    policy.sources.app_ttl_ms = GATEWAY_MONITOR_APP_STATE_TTL_MS;
    CHECK(gateway_adapter_bind_monitor_context_tx(adapter,
        (uart_m_transport_t *)&s_context_transport_token, &policy) == ESP_OK);
    CHECK(gateway_adapter_host_process_one(adapter, 7U));
    event = ble_event(BLE_GATT_EVENT_APP_STATE, 70U, generation);
    event.data.app_state.app_instance_id = 1U;
    event.data.app_state.active = true;
    mock_idf_set_time_ms(8U);
    CHECK(process_ble_event(adapter, &event, 8U));
    CHECK(s_context_send_calls == 0U);
    event = peer_observation(generation, 200U);
    CHECK(process_ble_event(adapter, &event, 200U));
    CHECK(s_context_send_calls == 1U);
    CHECK(gateway_adapter_host_tick(adapter, 399U));
    CHECK(s_context_send_calls == 1U);
    event = peer_observation(generation, 400U);
    CHECK(process_ble_event(adapter, &event, 400U));
    CHECK(s_context_send_calls == 2U);
    CHECK(s_context_sent.body.context.registration_connection_update_sequence == 2U);
    CHECK(s_context_sent.body.context.app_update_sequence == 1U);
    CHECK(s_context_sent.body.context.app_age_ms == 392U);
    CHECK(gateway_adapter_host_tick(adapter, 600U));
    CHECK(s_context_send_calls == 3U);
    CHECK(s_context_sent.body.context.registration_connection_update_sequence == 2U);
    CHECK(s_context_sent.body.context.registration_connection_age_ms == 200U);
    CHECK(s_context_sent.body.context.app_age_ms == 592U);
    make_alive(&alive, UINT32_C(0x12345678));
    alive.received_at_us = INT64_C(700000);
    gateway_adapter_on_uart_message(&alive, adapter);
    CHECK(gateway_adapter_host_process_one(adapter, 700U));
    CHECK(gateway_adapter_host_tick(adapter, 800U));
    CHECK(s_context_send_calls == 4U);
    CHECK(gateway_adapter_host_tick(adapter, 1000U));
    CHECK(s_context_send_calls == 4U); /* Repeated TX cannot renew missing observation. */
    CHECK(s_context_sent.body.context.context.device_context_id == read_u32_le(&s_status[4]));
    CHECK((uint32_t)s_context_sent.body.context.context.session_id == read_u32_le(&s_status[8]));
    CHECK(s_status[3] == 0U);
    CHECK(s_non_context_send_calls == 0U && mock_idf_uart_write_count() == 0U);
    CHECK(gateway_adapter_stop(&adapter) == ESP_OK);
    return true;
}

int main(int argc, char **argv)
{
    unsigned passed = 0U;
    const struct { const char *name; bool (*run)(void); } tests[] = {
        {"monitor_operating_policy", test_monitor_operating_policy_context_runtime},
        {"monitor_live_observation_ages", test_monitor_live_observation_source_ages},
        {"monitor_live_observation_rejection", test_monitor_live_observation_rejects_stale_or_untrusted},
        {"monitor_observation_security_pending", test_monitor_observation_during_security_pending},
        {"monitor_context_bind_loss", test_monitor_context_bind_loss_is_fail_closed},
        {"monitor_context_tx_policy_backpressure", test_monitor_context_tx_policy_freshness_backpressure},
        {"monitor_context_tx_session_invalidation", test_monitor_context_tx_closed_and_session_invalidation},
        {"monitor_status_before_vehicle", test_monitor_status_precedes_vehicle_notify},
        {"monitor_snapshot_notify_gate", test_monitor_snapshot_runtime_notify_and_command_gate},
        {"monitor_snapshot_boot_link", test_monitor_snapshot_fail_closed_and_boot_link_invalidation},
        {"monitor_snapshot_session_generation", test_monitor_snapshot_session_and_generation_invalidation},
        {"callbacks_deep_copy_and_queue_full", test_callbacks_deep_copy_and_queue_full},
        {"uart_frame_stream_gateway_task", test_uart_frame_stream_to_gateway_task},
        {"domain_phase5_m19a_golden_stream", test_domain_phase5_m19a_golden_stream_to_diagnostics},
        {"uart_gateway_queue_full_invalid_type", test_uart_gateway_queue_full_and_invalid_type},
        {"domain_link_timeout_recovery", test_domain_link_timeout_and_recovery},
        {"domain_boot_invalidates_p6", test_domain_boot_change_invalidates_p6_and_pending_result},
        {"domain_link_down_invalidates_p6", test_domain_link_down_invalidates_p6_without_recovery},
        {"alive_diagnostics_decode_supervisor", test_alive_diagnostics_keep_decode_and_supervisor_separate},
        {"boot_bond_snapshot", test_boot_bond_snapshot_deep_copy_and_exact_match},
        {"gateway_task_context_candidate", test_gateway_task_builds_authorized_context_candidate},
        {"request_admission_boundary", test_request_admission_boundary_revalidates_before_uart},
        {"read_only_query_runtime", test_read_only_query_candidate_and_notify_runtime},
        {"notify_gate_subscription_mtu_then_security", test_notify_gate_subscription_mtu_then_security},
        {"notify_gate_security_then_subscription", test_notify_gate_security_then_subscription},
        {"notify_gate_security_subscription_then_mtu", test_notify_gate_security_subscription_then_mtu},
        {"disconnect_stale_events_and_app_state_copy", test_disconnect_stale_events_and_app_state_copy},
        {"notify_failure_retains_pending_and_command_stays_blocked", test_notify_failure_retains_pending_and_command_stays_blocked},
        {"p6_observation_no_production_authority", test_p6_observation_has_no_production_authority},
        {"server_queue_full_disconnect", test_server_queue_full_disconnect_invalidates},
        {"gateway_queue_full_disconnect", test_gateway_queue_full_disconnect_invalidates},
        {"both_queues_full_reconnect", test_both_queues_full_then_normal_reconnect_recovers_notify},
        {"epoch_wrap_snapshot_failure", test_epoch_wrap_and_snapshot_failure_fail_closed},
        {"bcm_door_state_monitoring", test_bcm_door_state_stream_to_android_vehicle_data},
        {"result_vehicle_data_sessions", test_result_vehicle_data_candidate_preserves_sessions},
        {"result_notify_subscription_mtu", test_result_notify_waits_for_subscription_and_mtu},
        {"result_disconnect_no_replay", test_result_pending_is_not_replayed_after_disconnect},
        {"result_notify_failure_no_replay", test_result_notify_failure_is_not_vehicle_success_or_replayed},
        {"result_candidate_queue_full", test_result_candidate_queue_full_is_fail_closed},
#if CONFIG_P6_DEV_REGISTRATION_APPROVAL
        {"registration_requires_request_approval", test_registration_requires_separate_request_and_approval},
        {"registration_valid_commit_once", test_registration_valid_approval_commits_once},
        {"registration_ttl_clock", test_registration_request_ttl_and_clock_fail_closed},
        {"registration_stale_epoch_peer", test_registration_stale_epoch_and_peer_mismatch_rejected},
        {"registration_disconnect_commit_existing", test_registration_disconnect_commit_failure_and_existing_rejected},
        {"registration_queue_full", test_registration_queue_full_invalidates_pending},
#endif
    };
    size_t i;
    for (i = 0U; i < sizeof(tests) / sizeof(tests[0]); ++i) {
        if (argc > 1 && strcmp(argv[1], "--context-queue") == 0 &&
            strncmp(tests[i].name, "monitor_", 8U) != 0 &&
            strncmp(tests[i].name, "domain_boot_", 12U) != 0 &&
            strncmp(tests[i].name, "domain_link_down_", 17U) != 0) continue;
        mock_idf_reset();
        s_context_send_calls = 0U;
        s_context_cancel_calls = 0U;
        s_non_context_send_calls = 0U;
        s_context_send_result = UART_M_TRANSPORT_OK;
        reset_ble_snapshot();
        if (!tests[i].run()) { printf("FAILED: %s\n", tests[i].name); return 1; }
        passed++;
    }
    printf("PASS: %u gateway adapter tests\n", passed);
    return 0;
}
