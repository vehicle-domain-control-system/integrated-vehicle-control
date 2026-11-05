#include "gateway_core.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define CHECK(condition)                                                              \
    do {                                                                              \
        if (!(condition)) {                                                           \
            printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #condition);          \
            return false;                                                             \
        }                                                                             \
    } while (0)

typedef struct
{
    size_t uart_count;
    size_t ble_count;
    uart_m_message_t last_uart;
    gateway_ble_message_t last_ble;
    bool accept_uart;
    bool accept_ble;
} mock_ports_t;

typedef struct
{
    gateway_core_t core;
    mock_ports_t ports;
    uint64_t now_ms;
    uint32_t device_id;
    uint64_t session_id;
    uint32_t domain_boot_id;
    uint32_t generation;
} fixture_t;

static void write_u32_le(uint8_t *data, uint32_t value)
{
    data[0] = (uint8_t)value;
    data[1] = (uint8_t)(value >> 8U);
    data[2] = (uint8_t)(value >> 16U);
    data[3] = (uint8_t)(value >> 24U);
}

static void write_u64_le(uint8_t *data, uint64_t value)
{
    write_u32_le(data, (uint32_t)value);
    write_u32_le(&data[4], (uint32_t)(value >> 32U));
}

static bool observe_uart(const uart_m_message_t *message, void *user_context)
{
    mock_ports_t *ports = (mock_ports_t *)user_context;

    if (!ports->accept_uart) {
        return false;
    }
    ports->last_uart = *message;
    ports->uart_count++;
    return true;
}

static bool observe_ble(const gateway_ble_message_t *message, void *user_context)
{
    mock_ports_t *ports = (mock_ports_t *)user_context;

    if (!ports->accept_ble) {
        return false;
    }
    ports->last_ble = *message;
    ports->ble_count++;
    return true;
}

static gateway_core_context_t make_context(const fixture_t *fixture,
                                           uint64_t session_id,
                                           uint32_t generation)
{
    gateway_core_context_t context;

    memset(&context, 0, sizeof(context));
    context.identity.device_context_id = fixture->device_id;
    context.identity.session_id = session_id;
    context.identity.verified = true;
    context.connection_generation = generation;
    context.connected = true;
    context.encrypted = true;
    context.authenticated = false;
    context.registered = true;
    context.session_ready = true;
    context.authorization_verified = true;
    context.observed_at_ms = fixture->now_ms;
    return context;
}

static uart_m_message_t make_alive(uint32_t boot_id)
{
    uart_m_message_t message;

    memset(&message, 0, sizeof(message));
    message.type = UART_M_TYPE_DOMAIN_ALIVE;
    message.body.domain_alive.domain_boot_id = boot_id;
    message.body.domain_alive.update_sequence = 1U;
    message.body.domain_alive.source_age_ms = 0U;
    message.body.domain_alive.domain_state = UART_M_CONTROLLER_READY;
    message.body.domain_alive.quality = UART_M_QUALITY_OK;
    return message;
}

static bool fixture_init(fixture_t *fixture, size_t queue_depth)
{
    gateway_core_config_t config = gateway_core_isolated_mock_config();
    gateway_core_context_t context;
    uart_m_message_t alive;

    memset(fixture, 0, sizeof(*fixture));
    fixture->now_ms = 100U;
    fixture->device_id = UINT32_C(0x11223344);
    fixture->session_id = UINT64_C(0x0102030405060708);
    fixture->domain_boot_id = UINT32_C(0xAABBCCDD);
    fixture->generation = 7U;
    fixture->ports.accept_uart = true;
    fixture->ports.accept_ble = true;

    config.queue_depth = queue_depth;
    config.query_timeout_ms = 100U;
    config.context_stale_after_ms = 10000U;
    config.event_stale_after_ms = 500U;
    config.domain_alive_timeout_ms = 750U;
    config.mock_ports.observe_uart_candidate = observe_uart;
    config.mock_ports.observe_ble_candidate = observe_ble;
    config.mock_ports.user_context = &fixture->ports;
    CHECK(gateway_core_init(&fixture->core, &config) == GATEWAY_CORE_OK);

    context = make_context(fixture, fixture->session_id, fixture->generation);
    CHECK(gateway_core_enqueue_context(&fixture->core, &context, fixture->now_ms) ==
          GATEWAY_CORE_OK);
    CHECK(gateway_core_process_next(&fixture->core, fixture->now_ms) == GATEWAY_CORE_OK);

    alive = make_alive(fixture->domain_boot_id);
    CHECK(gateway_core_enqueue_uart_message(&fixture->core, &alive, fixture->now_ms) ==
          GATEWAY_CORE_OK);
    CHECK(gateway_core_process_next(&fixture->core, fixture->now_ms) == GATEWAY_CORE_OK);
    CHECK(gateway_core_is_domain_link_up(&fixture->core));
    return true;
}

static void make_request(uint8_t payload[12], uint32_t request_id)
{
    memset(payload, 0, 12U);
    write_u32_le(payload, request_id);
    payload[4] = 1U;
    payload[5] = 1U;
}

static void make_query(uint8_t payload[20], uint32_t query_id,
                       uart_m_query_scope_t scope)
{
    memset(payload, 0, 20U);
    write_u32_le(payload, query_id);
    payload[4] = (uint8_t)scope;
}

static uart_m_message_t make_digital_status(const fixture_t *fixture,
                                            uint32_t query_id)
{
    uart_m_message_t message;

    memset(&message, 0, sizeof(message));
    message.type = UART_M_TYPE_DIGITAL_STATUS;
    message.body.digital_status.context.recipient.device_context_id = fixture->device_id;
    message.body.digital_status.context.recipient.session_id = fixture->session_id;
    message.body.digital_status.context.domain_boot_id = fixture->domain_boot_id;
    message.body.digital_status.context.query_id = query_id;
    message.body.digital_status.setting = UART_M_FLAG_NO;
    message.body.digital_status.availability = UART_M_AVAILABILITY_AVAILABLE;
    message.body.digital_status.reason = 0U;
    message.body.digital_status.update_sequence = 3U;
    message.body.digital_status.source_age_ms = 10U;
    return message;
}

static uart_m_message_t make_user_settings(const fixture_t *fixture,
                                           uint32_t query_id)
{
    uart_m_message_t message;

    memset(&message, 0, sizeof(message));
    message.type = UART_M_TYPE_USER_SETTINGS;
    message.body.user_settings.context.recipient.device_context_id = fixture->device_id;
    message.body.user_settings.context.recipient.session_id = fixture->session_id;
    message.body.user_settings.context.domain_boot_id = fixture->domain_boot_id;
    message.body.user_settings.context.query_id = query_id;
    message.body.user_settings.climate_auto = UART_M_TRI_UNKNOWN;
    message.body.user_settings.fan_level = UART_M_FAN_UNKNOWN;
    message.body.user_settings.light_enabled = UART_M_TRI_UNKNOWN;
    message.body.user_settings.quality = UART_M_QUALITY_NO_DATA;
    message.body.user_settings.update_sequence = 4U;
    message.body.user_settings.source_age_ms = 20U;
    return message;
}

static uart_m_message_t make_result(const fixture_t *fixture,
                                    uint32_t query_id,
                                    uint32_t request_id)
{
    uart_m_message_t message;

    memset(&message, 0, sizeof(message));
    message.type = UART_M_TYPE_RESULT;
    message.body.result.context.recipient.device_context_id = fixture->device_id;
    message.body.result.context.recipient.session_id = fixture->session_id;
    message.body.result.context.domain_boot_id = fixture->domain_boot_id;
    message.body.result.context.query_id = query_id;
    message.body.result.request_session = fixture->session_id;
    message.body.result.request_id = request_id;
    message.body.result.result = UART_M_RESULT_ACCEPTED;
    message.body.result.result_reason = 0U;
    message.body.result.confirmation = UART_M_UNCONFIRMED;
    message.body.result.command_target = UART_M_COMMAND_TARGET_DOMAIN_SETTING;
    message.body.result.command_id = request_id;
    message.body.result.origin = UART_M_ORIGIN_MOBILE_MANUAL;
    message.body.result.result_age_ms = 0U;
    return message;
}

static uart_m_message_t make_query_end(const fixture_t *fixture,
                                       uint32_t query_id,
                                       uart_m_query_scope_t scope,
                                       uart_m_query_status_t status,
                                       uint16_t count)
{
    uart_m_message_t message;

    memset(&message, 0, sizeof(message));
    message.type = UART_M_TYPE_QUERY_END;
    message.body.query_end.context.recipient.device_context_id = fixture->device_id;
    message.body.query_end.context.recipient.session_id = fixture->session_id;
    message.body.query_end.context.domain_boot_id = fixture->domain_boot_id;
    message.body.query_end.context.query_id = query_id;
    message.body.query_end.scope = scope;
    message.body.query_end.status = status;
    message.body.query_end.item_count = count;
    return message;
}

static bool start_query(fixture_t *fixture, uint32_t query_id,
                        uart_m_query_scope_t scope)
{
    uint8_t payload[20];

    make_query(payload, query_id, scope);
    CHECK(gateway_core_enqueue_ble_command(&fixture->core, UART_M_TYPE_QUERY,
                                           payload, sizeof(payload),
                                           fixture->generation, fixture->now_ms) ==
          GATEWAY_CORE_OK);
    CHECK(gateway_core_process_next(&fixture->core, fixture->now_ms) == GATEWAY_CORE_OK);
    CHECK(fixture->ports.last_uart.type == UART_M_TYPE_QUERY);
    return true;
}

static bool test_mapper_uplink_golden_and_validation(void)
{
    const gateway_verified_context_t context = {
        .device_context_id = UINT32_C(0x04030201),
        .session_id = UINT64_C(0x0C0B0A0908070605),
        .verified = true,
    };
    const uint8_t request[12] = {
        0x0D, 0x0E, 0x0F, 0x10, 0x02, 0x01,
        0xD4, 0xFE, 0x00, 0x00, 0x00, 0x00,
    };
    const uint8_t expected[24] = {
        0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08,
        0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F, 0x10,
        0x02, 0x01, 0xD4, 0xFE, 0x00, 0x00, 0x00, 0x00,
    };
    uart_m_message_t message;
    uint8_t query[20];
    uint8_t warning_ack[9] = {0};
    uint8_t invalid_request[12];
    uint8_t encoded[128];
    size_t encoded_length = 0U;
    gateway_verified_context_t invalid_context = context;

    CHECK(gateway_mapper_ble_uplink_to_uart(UART_M_TYPE_REQUEST, request,
                                            sizeof(request), &context, &message) ==
          GATEWAY_MAPPER_OK);
    CHECK(message.type == UART_M_TYPE_REQUEST);
    CHECK(message.body.request.context.device_context_id == context.device_context_id);
    CHECK(message.body.request.context.session_id == context.session_id);
    CHECK(uart_m_codec_encode(&message, encoded, sizeof(encoded), &encoded_length) ==
          UART_M_CODEC_OK);
    CHECK(encoded_length == sizeof(expected));
    CHECK(memcmp(encoded, expected, sizeof(expected)) == 0);

    make_query(query, UINT32_C(0x44332211), UART_M_QUERY_RESULT);
    write_u64_le(&query[6], UINT64_C(0x8877665544332211));
    write_u32_le(&query[14], UINT32_C(0xCCBBAA99));
    CHECK(gateway_mapper_ble_uplink_to_uart(UART_M_TYPE_QUERY, query,
                                            sizeof(query), &context, &message) ==
          GATEWAY_MAPPER_OK);
    CHECK(message.type == UART_M_TYPE_QUERY);
    CHECK(message.body.query.query_id == UINT32_C(0x44332211));
    CHECK(message.body.query.scope == UART_M_QUERY_RESULT);
    CHECK(message.body.query.request_session == UINT64_C(0x8877665544332211));
    CHECK(message.body.query.request_id == UINT32_C(0xCCBBAA99));

    warning_ack[0] = 8U;
    write_u32_le(&warning_ack[1], UINT32_C(0x12345678));
    write_u32_le(&warning_ack[5], UINT32_C(0x87654321));
    CHECK(gateway_mapper_ble_uplink_to_uart(UART_M_TYPE_WARNING_ACK, warning_ack,
                                            sizeof(warning_ack), &context, &message) ==
          GATEWAY_MAPPER_OK);
    CHECK(message.type == UART_M_TYPE_WARNING_ACK);
    CHECK(message.body.warning_ack.warning_type ==
          UART_M_WARNING_OPEN_DOOR_AFTER_LEAVE);
    CHECK(message.body.warning_ack.occurrence_id == UINT32_C(0x12345678));
    CHECK(message.body.warning_ack.warning_domain_boot == UINT32_C(0x87654321));

    CHECK(gateway_mapper_ble_uplink_to_uart(0x99U, request, sizeof(request),
                                            &context, &message) ==
          GATEWAY_MAPPER_UNSUPPORTED_TYPE);
    CHECK(gateway_mapper_ble_uplink_to_uart(UART_M_TYPE_REQUEST, request,
                                            sizeof(request) - 1U, &context, &message) ==
          GATEWAY_MAPPER_INVALID_LENGTH);
    invalid_context.verified = false;
    CHECK(gateway_mapper_ble_uplink_to_uart(UART_M_TYPE_REQUEST, request,
                                            sizeof(request), &invalid_context, &message) ==
          GATEWAY_MAPPER_INVALID_CONTEXT);
    memcpy(invalid_request, request, sizeof(invalid_request));
    invalid_request[4] = 0xFFU;
    CHECK(gateway_mapper_ble_uplink_to_uart(UART_M_TYPE_REQUEST, invalid_request,
                                            sizeof(invalid_request), &context, &message) ==
          GATEWAY_MAPPER_INVALID_VALUE);
    return true;
}

static bool test_mapper_downlink_and_context_schema(void)
{
    fixture_t fixture;
    uart_m_message_t downlink;
    gateway_ble_message_t ble;
    uart_m_context_t context;
    uint8_t encoded[38];
    size_t encoded_length = 0U;

    memset(&fixture, 0, sizeof(fixture));
    fixture.device_id = UINT32_C(0x11223344);
    fixture.session_id = UINT64_C(0x0102030405060708);
    fixture.domain_boot_id = UINT32_C(0xAABBCCDD);
    downlink = make_digital_status(&fixture, 0U);
    CHECK(gateway_mapper_uart_downlink_to_ble(&downlink, &ble) == GATEWAY_MAPPER_OK);
    CHECK(ble.message_type == UART_M_TYPE_DIGITAL_STATUS);
    CHECK(ble.payload_length == 30U);
    CHECK(ble.payload[0] == 0x44U && ble.payload[3] == 0x11U);

    downlink.type = UART_M_TYPE_WINDOW_STATE;
    CHECK(gateway_mapper_uart_downlink_to_ble(&downlink, &ble) ==
          GATEWAY_MAPPER_SCHEMA_HOLD);
    downlink = make_alive(UINT32_C(0x12345678));
    CHECK(gateway_mapper_uart_downlink_to_ble(&downlink, &ble) ==
          GATEWAY_MAPPER_PRIVATE_MESSAGE);

    memset(&context, 0, sizeof(context));
    context.context.device_context_id = UINT32_C(0x04030201);
    context.context.session_id = UINT64_C(0x0C0B0A0908070605);
    context.registration = UART_M_TRI_UNKNOWN;
    context.connection = UART_M_TRI_YES;
    context.app_active = UART_M_TRI_NO;
    context.app_quality = UART_M_QUALITY_STALE;
    context.app_update_sequence = UINT32_C(0x14131211);
    context.app_age_ms = UINT16_C(0x1615);
    context.proximity = UART_M_TRI_UNKNOWN;
    context.proximity_validity = UART_M_VALIDITY_VALID;
    context.proximity_reason = UART_M_QUALITY_REASON_RECOVERING;
    context.proximity_update_sequence = UINT32_C(0x1C1B1A19);
    context.proximity_age_ms = UINT16_C(0x1E1D);
    context.registration_connection_quality = UART_M_QUALITY_INVALID;
    context.registration_connection_update_sequence = UINT32_C(0x23222120);
    context.registration_connection_age_ms = UINT16_C(0x2524);
    CHECK(gateway_mapper_encode_context_schema(&context, encoded, sizeof(encoded),
                                               &encoded_length) == GATEWAY_MAPPER_OK);
    CHECK(encoded_length == 38U);
    CHECK(encoded[0] == 0x01U && encoded[12] == 0xFFU && encoded[37] == 0x25U);
    CHECK(!gateway_core_production_routing_enabled());
    return true;
}

static bool test_deep_copy_duplicate_request_and_mock_only(void)
{
    fixture_t fixture;
    uint8_t payload[12];

    CHECK(fixture_init(&fixture, 8U));
    make_request(payload, 10U);
    CHECK(gateway_core_enqueue_ble_command(&fixture.core, UART_M_TYPE_REQUEST,
                                           payload, sizeof(payload), fixture.generation,
                                           fixture.now_ms) == GATEWAY_CORE_OK);
    memset(payload, 0xFF, sizeof(payload));
    CHECK(gateway_core_process_next(&fixture.core, fixture.now_ms) == GATEWAY_CORE_OK);
    CHECK(fixture.ports.uart_count == 1U);
    CHECK(fixture.ports.last_uart.body.request.request_id == 10U);
    CHECK(fixture.ports.last_uart.body.request.argument.door ==
          UART_M_DOOR_OPERATION_UNLOCK);

    make_request(payload, 10U);
    CHECK(gateway_core_enqueue_ble_command(&fixture.core, UART_M_TYPE_REQUEST,
                                           payload, sizeof(payload), fixture.generation,
                                           fixture.now_ms) == GATEWAY_CORE_OK);
    CHECK(gateway_core_process_next(&fixture.core, fixture.now_ms) ==
          GATEWAY_CORE_DUPLICATE_REQUEST);
    CHECK(fixture.ports.uart_count == 1U);
    return true;
}

static bool test_explicit_authorization_gate(void)
{
    fixture_t fixture;
    gateway_core_context_t context;
    uint8_t request[12];

    CHECK(fixture_init(&fixture, 8U));
    CHECK(!fixture.core.context.authenticated);
    CHECK(fixture.core.context.authorization_verified);

    make_request(request, 90U);
    CHECK(gateway_core_enqueue_ble_command(&fixture.core, UART_M_TYPE_REQUEST,
                                           request, sizeof(request), fixture.generation,
                                           fixture.now_ms) == GATEWAY_CORE_OK);
    CHECK(gateway_core_process_next(&fixture.core, fixture.now_ms) == GATEWAY_CORE_OK);
    CHECK(fixture.ports.uart_count == 1U);

    context = make_context(&fixture, fixture.session_id, fixture.generation);
    context.registered = false;
    CHECK(gateway_core_enqueue_context(&fixture.core, &context, fixture.now_ms) ==
          GATEWAY_CORE_OK);
    CHECK(gateway_core_process_next(&fixture.core, fixture.now_ms) == GATEWAY_CORE_OK);

    make_request(request, 91U);
    CHECK(gateway_core_enqueue_ble_command(&fixture.core, UART_M_TYPE_REQUEST,
                                           request, sizeof(request), fixture.generation,
                                           fixture.now_ms) == GATEWAY_CORE_OK);
    CHECK(gateway_core_process_next(&fixture.core, fixture.now_ms) ==
          GATEWAY_CORE_NOT_READY);
    CHECK(fixture.ports.uart_count == 1U);

    context = make_context(&fixture, fixture.session_id, fixture.generation);
    context.authenticated = true;
    context.authorization_verified = false;
    CHECK(gateway_core_enqueue_context(&fixture.core, &context, fixture.now_ms) ==
          GATEWAY_CORE_OK);
    CHECK(gateway_core_process_next(&fixture.core, fixture.now_ms) == GATEWAY_CORE_OK);

    make_request(request, 92U);
    CHECK(gateway_core_enqueue_ble_command(&fixture.core, UART_M_TYPE_REQUEST,
                                           request, sizeof(request), fixture.generation,
                                           fixture.now_ms) == GATEWAY_CORE_OK);
    CHECK(gateway_core_process_next(&fixture.core, fixture.now_ms) ==
          GATEWAY_CORE_NOT_READY);
    CHECK(fixture.ports.uart_count == 1U);
    return true;
}

static bool test_request_cache_full_and_session_rotation(void)
{
    fixture_t fixture;
    gateway_core_context_t context;
    gateway_core_stats_t stats;
    uint8_t request[12];
    uint32_t request_id;

    CHECK(fixture_init(&fixture, 8U));
    for (request_id = 1U; request_id <= GATEWAY_CORE_REQUEST_CACHE_DEPTH;
         ++request_id) {
        make_request(request, request_id);
        CHECK(gateway_core_enqueue_ble_command(&fixture.core, UART_M_TYPE_REQUEST,
                                               request, sizeof(request),
                                               fixture.generation,
                                               fixture.now_ms) == GATEWAY_CORE_OK);
        CHECK(gateway_core_process_next(&fixture.core, fixture.now_ms) ==
              GATEWAY_CORE_OK);
    }
    CHECK(fixture.ports.uart_count == GATEWAY_CORE_REQUEST_CACHE_DEPTH);

    make_request(request, 1U);
    CHECK(gateway_core_enqueue_ble_command(&fixture.core, UART_M_TYPE_REQUEST,
                                           request, sizeof(request), fixture.generation,
                                           fixture.now_ms) == GATEWAY_CORE_OK);
    CHECK(gateway_core_process_next(&fixture.core, fixture.now_ms) ==
          GATEWAY_CORE_DUPLICATE_REQUEST);

    make_request(request, GATEWAY_CORE_REQUEST_CACHE_DEPTH + 1U);
    CHECK(gateway_core_enqueue_ble_command(&fixture.core, UART_M_TYPE_REQUEST,
                                           request, sizeof(request), fixture.generation,
                                           fixture.now_ms) == GATEWAY_CORE_OK);
    CHECK(gateway_core_process_next(&fixture.core, fixture.now_ms) ==
          GATEWAY_CORE_REQUEST_CACHE_FULL);
    CHECK(fixture.ports.uart_count == GATEWAY_CORE_REQUEST_CACHE_DEPTH);
    gateway_core_get_stats(&fixture.core, &stats);
    CHECK(stats.duplicate_requests == 1U);
    CHECK(stats.request_cache_full == 1U);

    context = make_context(&fixture, fixture.session_id + 1U, fixture.generation);
    CHECK(gateway_core_enqueue_context(&fixture.core, &context, fixture.now_ms) ==
          GATEWAY_CORE_OK);
    CHECK(gateway_core_process_next(&fixture.core, fixture.now_ms) == GATEWAY_CORE_OK);
    make_request(request, 1U);
    CHECK(gateway_core_enqueue_ble_command(&fixture.core, UART_M_TYPE_REQUEST,
                                           request, sizeof(request), fixture.generation,
                                           fixture.now_ms) == GATEWAY_CORE_OK);
    CHECK(gateway_core_process_next(&fixture.core, fixture.now_ms) == GATEWAY_CORE_OK);
    CHECK(fixture.ports.uart_count == GATEWAY_CORE_REQUEST_CACHE_DEPTH + 1U);
    return true;
}

static bool test_invalid_type_length_and_queue_full(void)
{
    fixture_t fixture;
    uint8_t payload[12];

    CHECK(fixture_init(&fixture, 2U));
    make_request(payload, 1U);
    CHECK(gateway_core_enqueue_ble_command(&fixture.core, 0x99U, payload,
                                           sizeof(payload), fixture.generation,
                                           fixture.now_ms) == GATEWAY_CORE_OK);
    CHECK(gateway_core_process_next(&fixture.core, fixture.now_ms) ==
          GATEWAY_CORE_INVALID_MESSAGE);
    CHECK(gateway_core_enqueue_ble_command(&fixture.core, UART_M_TYPE_REQUEST,
                                           payload, sizeof(payload) - 1U,
                                           fixture.generation, fixture.now_ms) ==
          GATEWAY_CORE_OK);
    CHECK(gateway_core_process_next(&fixture.core, fixture.now_ms) ==
          GATEWAY_CORE_INVALID_MESSAGE);

    CHECK(gateway_core_enqueue_tick(&fixture.core, fixture.now_ms) == GATEWAY_CORE_OK);
    CHECK(gateway_core_enqueue_tick(&fixture.core, fixture.now_ms) == GATEWAY_CORE_OK);
    CHECK(gateway_core_enqueue_tick(&fixture.core, fixture.now_ms) ==
          GATEWAY_CORE_QUEUE_FULL);
    return true;
}

static bool test_context_mismatch_and_stale(void)
{
    fixture_t fixture;
    uart_m_message_t message;

    CHECK(fixture_init(&fixture, 8U));
    message = make_digital_status(&fixture, 0U);
    message.body.digital_status.context.recipient.device_context_id++;
    CHECK(gateway_core_enqueue_uart_message(&fixture.core, &message, fixture.now_ms) ==
          GATEWAY_CORE_OK);
    CHECK(gateway_core_process_next(&fixture.core, fixture.now_ms) ==
          GATEWAY_CORE_CONTEXT_MISMATCH);

    message = make_digital_status(&fixture, 0U);
    message.body.digital_status.context.recipient.session_id++;
    CHECK(gateway_core_enqueue_uart_message(&fixture.core, &message, fixture.now_ms) ==
          GATEWAY_CORE_OK);
    CHECK(gateway_core_process_next(&fixture.core, fixture.now_ms) ==
          GATEWAY_CORE_CONTEXT_MISMATCH);

    CHECK(gateway_core_enqueue_tick(&fixture.core, fixture.now_ms) == GATEWAY_CORE_OK);
    CHECK(gateway_core_process_next(&fixture.core, fixture.now_ms + 501U) ==
          GATEWAY_CORE_STALE);
    return true;
}

static bool test_query_duplicate_partial_and_count_mismatch(void)
{
    fixture_t fixture;
    uart_m_message_t response;
    uart_m_message_t end;
    gateway_query_state_t query;
    gateway_query_outcome_t outcome;

    CHECK(fixture_init(&fixture, 8U));
    CHECK(start_query(&fixture, 21U, UART_M_QUERY_ALL));
    response = make_digital_status(&fixture, 21U);
    CHECK(gateway_core_enqueue_uart_message(&fixture.core, &response, fixture.now_ms) ==
          GATEWAY_CORE_OK);
    CHECK(gateway_core_process_next(&fixture.core, fixture.now_ms) == GATEWAY_CORE_OK);
    CHECK(fixture.ports.ble_count == 1U);

    CHECK(gateway_core_enqueue_uart_message(&fixture.core, &response, fixture.now_ms) ==
          GATEWAY_CORE_OK);
    CHECK(gateway_core_process_next(&fixture.core, fixture.now_ms) ==
          GATEWAY_CORE_DUPLICATE_RESPONSE);
    CHECK(fixture.ports.ble_count == 1U);
    gateway_core_get_query_state(&fixture.core, &query, &outcome);
    CHECK(query.unique_response_count == 1U);

    end = make_query_end(&fixture, 21U, UART_M_QUERY_ALL,
                         UART_M_QUERY_PARTIAL_UNCONFIRMED, 1U);
    CHECK(gateway_core_enqueue_uart_message(&fixture.core, &end, fixture.now_ms) ==
          GATEWAY_CORE_OK);
    CHECK(gateway_core_process_next(&fixture.core, fixture.now_ms) ==
          GATEWAY_CORE_ROUTING_HOLD);
    gateway_core_get_query_state(&fixture.core, &query, &outcome);
    CHECK(!query.active);
    CHECK(outcome == GATEWAY_QUERY_OUTCOME_PARTIAL);
    CHECK(fixture.ports.ble_count == 1U);

    CHECK(start_query(&fixture, 22U, UART_M_QUERY_ALL));
    response = make_user_settings(&fixture, 22U);
    CHECK(gateway_core_enqueue_uart_message(&fixture.core, &response, fixture.now_ms) ==
          GATEWAY_CORE_OK);
    CHECK(gateway_core_process_next(&fixture.core, fixture.now_ms) == GATEWAY_CORE_OK);
    end = make_query_end(&fixture, 22U, UART_M_QUERY_ALL, UART_M_QUERY_DONE, 2U);
    CHECK(gateway_core_enqueue_uart_message(&fixture.core, &end, fixture.now_ms) ==
          GATEWAY_CORE_OK);
    CHECK(gateway_core_process_next(&fixture.core, fixture.now_ms) ==
          GATEWAY_CORE_QUERY_COUNT_MISMATCH);
    gateway_core_get_query_state(&fixture.core, &query, &outcome);
    CHECK(outcome == GATEWAY_QUERY_OUTCOME_COUNT_MISMATCH);
    return true;
}

static bool test_query_scope_mismatch(void)
{
    fixture_t fixture;
    uart_m_message_t response;
    gateway_query_state_t query;
    gateway_query_outcome_t outcome;

    CHECK(fixture_init(&fixture, 8U));
    CHECK(start_query(&fixture, 25U, UART_M_QUERY_RESULT));
    response = make_digital_status(&fixture, 25U);
    CHECK(gateway_core_enqueue_uart_message(&fixture.core, &response, fixture.now_ms) ==
          GATEWAY_CORE_OK);
    CHECK(gateway_core_process_next(&fixture.core, fixture.now_ms) ==
          GATEWAY_CORE_INVALID_MESSAGE);
    gateway_core_get_query_state(&fixture.core, &query, &outcome);
    CHECK(query.active);
    CHECK(query.unique_response_count == 0U);
    CHECK(fixture.ports.ble_count == 0U);
    return true;
}

static bool test_query_capacity_overflow_is_not_duplicate(void)
{
    fixture_t fixture;
    uart_m_message_t response;
    uart_m_message_t end;
    gateway_query_state_t query;
    gateway_query_outcome_t outcome;
    gateway_core_stats_t stats;
    uint32_t index;

    CHECK(fixture_init(&fixture, 8U));
    CHECK(start_query(&fixture, 26U, UART_M_QUERY_RESULT));
    for (index = 0U; index < GATEWAY_CORE_MAX_QUERY_RESPONSES; ++index) {
        response = make_result(&fixture, 26U, index + 1U);
        CHECK(gateway_core_enqueue_uart_message(&fixture.core, &response,
                                                fixture.now_ms) == GATEWAY_CORE_OK);
        CHECK(gateway_core_process_next(&fixture.core, fixture.now_ms) ==
              GATEWAY_CORE_OK);
    }

    response = make_result(&fixture, 26U,
                           GATEWAY_CORE_MAX_QUERY_RESPONSES + 1U);
    CHECK(gateway_core_enqueue_uart_message(&fixture.core, &response,
                                            fixture.now_ms) == GATEWAY_CORE_OK);
    CHECK(gateway_core_process_next(&fixture.core, fixture.now_ms) ==
          GATEWAY_CORE_QUERY_CAPACITY_OVERFLOW);
    gateway_core_get_query_state(&fixture.core, &query, &outcome);
    CHECK(!query.active);
    CHECK(outcome == GATEWAY_QUERY_OUTCOME_CAPACITY_OVERFLOW);
    gateway_core_get_stats(&fixture.core, &stats);
    CHECK(stats.query_capacity_overflows == 1U);
    CHECK(stats.duplicate_query_responses == 0U);
    CHECK(fixture.ports.ble_count == GATEWAY_CORE_MAX_QUERY_RESPONSES);

    end = make_query_end(&fixture, 26U, UART_M_QUERY_RESULT,
                         UART_M_QUERY_DONE,
                         GATEWAY_CORE_MAX_QUERY_RESPONSES);
    CHECK(gateway_core_enqueue_uart_message(&fixture.core, &end,
                                            fixture.now_ms) == GATEWAY_CORE_OK);
    CHECK(gateway_core_process_next(&fixture.core, fixture.now_ms) ==
          GATEWAY_CORE_CONTEXT_MISMATCH);
    gateway_core_get_query_state(&fixture.core, &query, &outcome);
    CHECK(outcome == GATEWAY_QUERY_OUTCOME_CAPACITY_OVERFLOW);
    return true;
}

static bool test_query_timeout_and_domain_boot_change(void)
{
    fixture_t fixture;
    uart_m_message_t alive;
    gateway_query_state_t query;
    gateway_query_outcome_t outcome;
    gateway_core_context_t context;
    uint8_t request[12];
    size_t uart_before_boot;

    CHECK(fixture_init(&fixture, 8U));
    make_request(request, 55U);
    CHECK(gateway_core_enqueue_ble_command(&fixture.core, UART_M_TYPE_REQUEST,
                                           request, sizeof(request), fixture.generation,
                                           fixture.now_ms) == GATEWAY_CORE_OK);
    CHECK(gateway_core_process_next(&fixture.core, fixture.now_ms) == GATEWAY_CORE_OK);
    uart_before_boot = fixture.ports.uart_count;
    CHECK(start_query(&fixture, 30U, UART_M_QUERY_STATE));
    CHECK(gateway_core_enqueue_tick(&fixture.core, fixture.now_ms + 101U) ==
          GATEWAY_CORE_OK);
    CHECK(gateway_core_process_next(&fixture.core, fixture.now_ms + 101U) ==
          GATEWAY_CORE_OK);
    gateway_core_get_query_state(&fixture.core, &query, &outcome);
    CHECK(!query.active);
    CHECK(outcome == GATEWAY_QUERY_OUTCOME_TIMEOUT);

    fixture.now_ms += 200U;
    CHECK(start_query(&fixture, 31U, UART_M_QUERY_STATE));
    alive = make_alive(fixture.domain_boot_id + 1U);
    CHECK(gateway_core_enqueue_uart_message(&fixture.core, &alive, fixture.now_ms) ==
          GATEWAY_CORE_OK);
    CHECK(gateway_core_process_next(&fixture.core, fixture.now_ms) == GATEWAY_CORE_OK);
    gateway_core_get_query_state(&fixture.core, &query, &outcome);
    CHECK(!query.active);
    CHECK(outcome == GATEWAY_QUERY_OUTCOME_DOMAIN_BOOT_CHANGED);

    make_request(request, 56U);
    CHECK(gateway_core_enqueue_ble_command(&fixture.core, UART_M_TYPE_REQUEST,
                                           request, sizeof(request), fixture.generation,
                                           fixture.now_ms) == GATEWAY_CORE_OK);
    CHECK(gateway_core_process_next(&fixture.core, fixture.now_ms) ==
          GATEWAY_CORE_NOT_READY);
    CHECK(fixture.ports.uart_count == uart_before_boot + 2U);

    context = make_context(&fixture, fixture.session_id, fixture.generation);
    CHECK(gateway_core_enqueue_context(&fixture.core, &context, fixture.now_ms) ==
          GATEWAY_CORE_OK);
    CHECK(gateway_core_process_next(&fixture.core, fixture.now_ms) == GATEWAY_CORE_OK);
    make_request(request, 55U);
    CHECK(gateway_core_enqueue_ble_command(&fixture.core, UART_M_TYPE_REQUEST,
                                           request, sizeof(request), fixture.generation,
                                           fixture.now_ms) == GATEWAY_CORE_OK);
    CHECK(gateway_core_process_next(&fixture.core, fixture.now_ms) ==
          GATEWAY_CORE_DUPLICATE_REQUEST);
    CHECK(fixture.ports.uart_count == uart_before_boot + 2U);
    return true;
}

static bool test_duplicate_query_busy_and_session_rotation(void)
{
    fixture_t fixture;
    gateway_core_context_t context;
    gateway_query_state_t query;
    gateway_query_outcome_t outcome;
    uint8_t payload[20];

    CHECK(fixture_init(&fixture, 8U));
    CHECK(start_query(&fixture, 41U, UART_M_QUERY_STATE));

    make_query(payload, 41U, UART_M_QUERY_STATE);
    CHECK(gateway_core_enqueue_ble_command(&fixture.core, UART_M_TYPE_QUERY,
                                           payload, sizeof(payload), fixture.generation,
                                           fixture.now_ms) == GATEWAY_CORE_OK);
    CHECK(gateway_core_process_next(&fixture.core, fixture.now_ms) ==
          GATEWAY_CORE_DUPLICATE_QUERY);

    make_query(payload, 42U, UART_M_QUERY_STATE);
    CHECK(gateway_core_enqueue_ble_command(&fixture.core, UART_M_TYPE_QUERY,
                                           payload, sizeof(payload), fixture.generation,
                                           fixture.now_ms) == GATEWAY_CORE_OK);
    CHECK(gateway_core_process_next(&fixture.core, fixture.now_ms) ==
          GATEWAY_CORE_QUERY_BUSY);

    context = make_context(&fixture, fixture.session_id + 1U, fixture.generation);
    context.observed_at_ms = fixture.now_ms + 1U;
    CHECK(gateway_core_enqueue_context(&fixture.core, &context,
                                       fixture.now_ms + 1U) == GATEWAY_CORE_OK);
    CHECK(gateway_core_process_next(&fixture.core, fixture.now_ms + 1U) ==
          GATEWAY_CORE_OK);
    gateway_core_get_query_state(&fixture.core, &query, &outcome);
    CHECK(!query.active);
    CHECK(outcome == GATEWAY_QUERY_OUTCOME_SESSION_ROTATED);
    return true;
}

static bool test_disconnect_session_rotation_and_no_replay(void)
{
    fixture_t fixture;
    gateway_core_context_t context;
    uint8_t request[12];
    size_t sent_before_disconnect;

    CHECK(fixture_init(&fixture, 8U));
    make_request(request, 77U);
    CHECK(gateway_core_enqueue_ble_command(&fixture.core, UART_M_TYPE_REQUEST,
                                           request, sizeof(request), fixture.generation,
                                           fixture.now_ms) == GATEWAY_CORE_OK);
    CHECK(gateway_core_process_next(&fixture.core, fixture.now_ms) == GATEWAY_CORE_OK);
    sent_before_disconnect = fixture.ports.uart_count;

    CHECK(gateway_core_enqueue_disconnect(&fixture.core, fixture.generation,
                                          fixture.now_ms) == GATEWAY_CORE_OK);
    CHECK(gateway_core_process_next(&fixture.core, fixture.now_ms) == GATEWAY_CORE_OK);
    CHECK(gateway_core_enqueue_tick(&fixture.core, fixture.now_ms + 1U) ==
          GATEWAY_CORE_OK);
    CHECK(gateway_core_process_next(&fixture.core, fixture.now_ms + 1U) ==
          GATEWAY_CORE_OK);
    CHECK(fixture.ports.uart_count == sent_before_disconnect);

    context = make_context(&fixture, fixture.session_id, fixture.generation + 1U);
    context.observed_at_ms = fixture.now_ms + 2U;
    CHECK(gateway_core_enqueue_context(&fixture.core, &context,
                                       fixture.now_ms + 2U) == GATEWAY_CORE_OK);
    CHECK(gateway_core_process_next(&fixture.core, fixture.now_ms + 2U) ==
          GATEWAY_CORE_OK);
    CHECK(gateway_core_enqueue_ble_command(&fixture.core, UART_M_TYPE_REQUEST,
                                           request, sizeof(request),
                                           fixture.generation + 1U,
                                           fixture.now_ms + 2U) == GATEWAY_CORE_OK);
    CHECK(gateway_core_process_next(&fixture.core, fixture.now_ms + 2U) ==
          GATEWAY_CORE_DUPLICATE_REQUEST);
    CHECK(fixture.ports.uart_count == sent_before_disconnect);

    context = make_context(&fixture, fixture.session_id + 1U,
                           fixture.generation + 1U);
    context.observed_at_ms = fixture.now_ms + 3U;
    CHECK(gateway_core_enqueue_context(&fixture.core, &context,
                                       fixture.now_ms + 3U) == GATEWAY_CORE_OK);
    CHECK(gateway_core_process_next(&fixture.core, fixture.now_ms + 3U) ==
          GATEWAY_CORE_OK);
    CHECK(fixture.ports.uart_count == sent_before_disconnect);
    return true;
}

typedef bool (*test_fn_t)(void);

typedef struct
{
    const char *name;
    test_fn_t function;
} test_case_t;

int main(void)
{
    static const test_case_t tests[] = {
        {"mapper_uplink_golden_and_validation", test_mapper_uplink_golden_and_validation},
        {"mapper_downlink_and_context_schema", test_mapper_downlink_and_context_schema},
        {"deep_copy_duplicate_request_and_mock_only", test_deep_copy_duplicate_request_and_mock_only},
        {"explicit_authorization_gate", test_explicit_authorization_gate},
        {"request_cache_full_and_session_rotation", test_request_cache_full_and_session_rotation},
        {"invalid_type_length_and_queue_full", test_invalid_type_length_and_queue_full},
        {"context_mismatch_and_stale", test_context_mismatch_and_stale},
        {"query_duplicate_partial_and_count_mismatch", test_query_duplicate_partial_and_count_mismatch},
        {"query_scope_mismatch", test_query_scope_mismatch},
        {"query_capacity_overflow_is_not_duplicate", test_query_capacity_overflow_is_not_duplicate},
        {"query_timeout_and_domain_boot_change", test_query_timeout_and_domain_boot_change},
        {"duplicate_query_busy_and_session_rotation", test_duplicate_query_busy_and_session_rotation},
        {"disconnect_session_rotation_and_no_replay", test_disconnect_session_rotation_and_no_replay},
    };
    size_t passed = 0U;
    size_t i;

    for (i = 0U; i < (sizeof(tests) / sizeof(tests[0])); ++i) {
        if (!tests[i].function()) {
            printf("FAILED: %s\n", tests[i].name);
            return 1;
        }
        passed++;
    }
    printf("PASS: %zu/%zu gateway tests\n", passed,
           sizeof(tests) / sizeof(tests[0]));
    return 0;
}
