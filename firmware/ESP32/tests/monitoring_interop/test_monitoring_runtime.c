/* Existing P6/Connection/GATT fixture, with its Transport stubs renamed only
 * here. GatewayAdapter links real Transport, Queue, AGE correction and Codec. */
#define main ExistingGatewayFixtureMain
#define uart_m_transport_send fixture_unused_send
#define uart_m_transport_send_context fixture_unused_send_context
#define uart_m_transport_invalidate_context fixture_unused_invalidate_context
#include "../gateway_adapter/test_gateway_adapter.c"
#undef main
#undef uart_m_transport_send
#undef uart_m_transport_send_context
#undef uart_m_transport_invalidate_context
#include "uart_m_transport_engine.h"
extern int monitoring_peer_exchange(const uint8_t *, size_t, uint32_t,
    bool (*receive)(const uint8_t *, size_t));
extern int monitoring_peer_expire(uint32_t);
static gateway_adapter_t *runtime;
static uart_m_transport_engine_t receiver;
static unsigned environment_received, rear_received;
static bool typed_receive(const uart_m_transport_rx_message_t *message, void *unused)
{
    (void)unused;
    if (message->message.type == UART_M_TYPE_CIS_ENVIRONMENT) {
        const uart_m_cis_environment_t *p = &message->message.body.cis_environment;
        CHECK(p->temperature.value == 2345 && p->temperature.sequence == 1U);
        CHECK(p->humidity.value == 4567U && p->illuminance.value == 123U);
        CHECK(p->temperature.validity == UART_M_VALIDITY_VALID && p->temperature.age_ms == 0U);
        CHECK(p->source.boot_id == 201U && p->source.tx_sequence == 2U);
        environment_received++;
    } else if (message->message.type == UART_M_TYPE_CIS_REAR) {
        const uart_m_cis_rear_t *p = &message->message.body.cis_rear;
        CHECK(p->rear_distance == 1234U && p->update_sequence == 1U);
        CHECK(p->validity == UART_M_VALIDITY_VALID && p->source_age_ms == 0U);
        CHECK(p->source.boot_id == 201U && p->source.tx_sequence == 1U);
        rear_received++;
    }
    else return false;
    gateway_adapter_on_uart_message(message, runtime);
    return gateway_adapter_host_process_one(runtime, 210U);
}
static bool wire_receive(const uint8_t *bytes, size_t length)
{
    uart_m_transport_feed_result_t result;
    unsigned before = s_vehicle_notify_calls;
    CHECK(uart_m_transport_engine_feed(&receiver, bytes, length / 2U,
        INT64_C(210000), &result) == UART_M_TRANSPORT_OK);
    CHECK(uart_m_transport_engine_feed(&receiver, bytes + length / 2U,
        length - length / 2U, INT64_C(210000), &result) == UART_M_TRANSPORT_OK);
    CHECK(s_vehicle_notify_calls == before + 1U);
    CHECK(s_vehicle_mode == BLE_GATT_SEND_NOTIFY);
    return true;
}
static bool run(void)
{
    uart_m_transport_t *transport = NULL;
    uart_m_transport_config_t config = {0};
    gateway_monitor_context_tx_policy_t policy = {0};
    gateway_core_context_t context;
    ble_gatt_server_event_t event;
    uint32_t generation;
    uint8_t frame[UART_M_MAX_FRAME_SIZE];
    size_t length;
    uart_m_frame_t parsed;
    uart_m_message_t queued;
    uart_m_transport_stats_t stats;
    runtime = start_adapter_with_registration(8U, 0x70U);
    CHECK(runtime != NULL);
    CHECK(establish_monitor_context(runtime, true, &generation, &context));
    gateway_adapter_host_enable_monitor_observation(runtime);
    config.uart_port = 2; config.tx_gpio = 27; config.rx_gpio = 26;
    config.rts_gpio = config.cts_gpio = -1; config.baud_rate = 115200U;
    config.data_bits = 8U; config.stop_bits = UART_M_TRANSPORT_STOP_BITS_1;
    config.driver_rx_buffer_size = config.driver_tx_buffer_size = 512U;
    config.driver_event_queue_depth = 8U; config.tx_queue_depth = 2U;
    config.rx_delivery_queue_depth = 3U; config.rx_chunk_size = 64U;
    config.uart_event_wait_ms = 10U; config.frame_assembly_timeout_ms = 50U;
    config.tx_complete_timeout_ms = config.shutdown_timeout_ms = 100U;
    config.rx_task_stack_size = config.tx_task_stack_size = config.dispatch_task_stack_size = 2048U;
    config.rx_task_priority = config.tx_task_priority = 5U; config.dispatch_task_priority = 4U;
    config.first_tx_sequence = 1U;
    CHECK(uart_m_transport_init(&transport, &config, gateway_adapter_on_uart_message, runtime) == UART_M_TRANSPORT_OK);
    policy.approved = true; policy.refresh_period_ms = 200U; policy.queued_context_ttl_ms = 600U;
    policy.sources.registration_connection_ttl_ms = 600U; policy.sources.app_ttl_ms = 1000U;
    CHECK(gateway_adapter_bind_monitor_context_tx(runtime, transport, &policy) == ESP_OK);
    CHECK(gateway_adapter_host_process_one(runtime, 7U));
    event = ble_event(BLE_GATT_EVENT_APP_STATE, 70U, generation);
    event.data.app_state.app_instance_id = 1U; event.data.app_state.active = true;
    mock_idf_set_time_ms(8U);
    CHECK(process_ble_event(runtime, &event, 8U));
    event = peer_observation(generation, 200U);
    mock_idf_set_time_ms(200U);
    CHECK(process_ble_event(runtime, &event, 200U));
    CHECK(mock_idf_uart_write_count() == 0U);
    mock_idf_set_time_ms(210U);
    CHECK(mock_idf_run_task(3U)); /* Gateway=1, UART RX=2, UART TX=3. */
    CHECK(mock_idf_uart_write_count() == 1U);
    length = mock_idf_last_uart_write(frame, sizeof(frame));
    CHECK(uart_m_frame_decode(frame, length, &parsed) == UART_M_FRAME_STATUS_OK);
    CHECK(uart_m_codec_decode(parsed.message_type, parsed.payload, parsed.payload_length, &queued) == UART_M_CODEC_OK);
    CHECK(queued.type == UART_M_TYPE_CONTEXT);
    CHECK(queued.body.context.registration_connection_age_ms == 10U);
    CHECK(queued.body.context.app_age_ms == 202U);
    CHECK(queued.body.context.context.device_context_id == read_u32_le(&s_status[4]));
    CHECK((uint32_t)queued.body.context.context.session_id == read_u32_le(&s_status[8]));
    CHECK((uint32_t)(queued.body.context.context.session_id >> 32U) == read_u32_le(&s_status[12]));
    CHECK(queued.body.context.proximity == UART_M_TRI_UNKNOWN && queued.body.context.proximity_age_ms == UINT16_MAX);
    CHECK(s_status[3] == 0U && P6_PRODUCTION_BINDING_ENABLED == 0 && GATEWAY_CORE_PRODUCTION_ROUTING_ENABLED == 0);
    CHECK(uart_m_transport_engine_init(&receiver, 1U, typed_receive, NULL) == UART_M_TRANSPORT_OK);
    CHECK(monitoring_peer_exchange(frame, length, 210U, wire_receive) == 0);
    CHECK(environment_received == 1U && rear_received == 1U && s_vehicle_notify_calls == 2U);
    CHECK(monitoring_peer_expire(800U) == 0);
    CHECK(s_vehicle_notify_calls == 2U);
    uart_m_transport_get_stats(transport, &stats);
    CHECK(stats.tx_context_driver_submit_calls == 1U && stats.tx_frames_completed == 1U);
    CHECK(gateway_adapter_stop(&runtime) == ESP_OK);
    CHECK(uart_m_transport_deinit(&transport) == UART_M_TRANSPORT_OK);
    return true;
}
int main(void)
{
    if (!run()) return 1;
    puts("PASS: single-process P6 fixture -> real UART Queue/AGE/Codec -> Domain Provider/CIS Environment+Rear Publisher -> Network -> ESP32 Parser/Gateway -> BLE Notify mock; no vehicle TX");
    return 0;
}
