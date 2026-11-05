#include "gateway_core.h"

#include <string.h>

static uint16_t read_u16_le(const uint8_t *data)
{
    return (uint16_t)((uint16_t)data[0] | ((uint16_t)data[1] << 8U));
}

static uint32_t read_u32_le(const uint8_t *data)
{
    return (uint32_t)data[0] |
           ((uint32_t)data[1] << 8U) |
           ((uint32_t)data[2] << 16U) |
           ((uint32_t)data[3] << 24U);
}

static uint64_t read_u64_le(const uint8_t *data)
{
    return (uint64_t)read_u32_le(data) |
           ((uint64_t)read_u32_le(&data[4]) << 32U);
}

static bool elapsed_more_than(uint64_t now_ms, uint64_t then_ms, uint32_t limit_ms)
{
    return (now_ms >= then_ms) && ((now_ms - then_ms) > (uint64_t)limit_ms);
}

static gateway_core_status_t enqueue_event(gateway_core_t *core,
                                           const gateway_core_event_t *event)
{
    if ((core == NULL) || (event == NULL) || !core->initialized) {
        return GATEWAY_CORE_INVALID_ARGUMENT;
    }
    if (core->queue_count >= core->config.queue_depth) {
        core->stats.queue_full++;
        return GATEWAY_CORE_QUEUE_FULL;
    }

    core->events[core->queue_tail] = *event;
    core->queue_tail = (core->queue_tail + 1U) % core->config.queue_depth;
    core->queue_count++;
    core->stats.events_enqueued++;
    return GATEWAY_CORE_OK;
}

static gateway_core_event_t dequeue_event(gateway_core_t *core)
{
    gateway_core_event_t event = core->events[core->queue_head];

    core->queue_head = (core->queue_head + 1U) % core->config.queue_depth;
    core->queue_count--;
    return event;
}

static bool context_identity_valid(const gateway_core_context_t *context)
{
    return context->identity.verified &&
           (context->identity.device_context_id != 0U) &&
           (context->identity.session_id != 0U);
}

static bool context_is_admitted(const gateway_core_t *core)
{
    return core->has_context && context_identity_valid(&core->context) &&
           core->context.connected && core->context.encrypted &&
           core->context.registered && core->context.authorization_verified;
}

static bool context_is_current(const gateway_core_t *core, uint64_t now_ms)
{
    if (!context_is_admitted(core)) {
        return false;
    }
    return !elapsed_more_than(now_ms, core->context.observed_at_ms,
                              core->config.context_stale_after_ms);
}

static void finish_query(gateway_core_t *core, gateway_query_outcome_t outcome)
{
    core->query.active = false;
    core->last_query_outcome = outcome;
    core->response_key_count = 0U;
    memset(core->response_keys, 0, sizeof(core->response_keys));
}

static void expire_state(gateway_core_t *core, uint64_t now_ms)
{
    if (core->domain_link_up &&
        elapsed_more_than(now_ms, core->domain_last_seen_ms,
                          core->config.domain_alive_timeout_ms)) {
        core->domain_link_up = false;
    }
    if (core->query.active &&
        elapsed_more_than(now_ms, core->query.started_at_ms,
                          core->config.query_timeout_ms)) {
        core->stats.query_timeouts++;
        finish_query(core, GATEWAY_QUERY_OUTCOME_TIMEOUT);
    }
}

static bool request_key_exists(const gateway_core_t *core,
                               const uart_m_request_t *request)
{
    size_t i;

    for (i = 0U; i < GATEWAY_CORE_REQUEST_CACHE_DEPTH; ++i) {
        const gateway_request_key_t *key = &core->request_keys[i];
        if (key->used &&
            (key->device_context_id == request->context.device_context_id) &&
            (key->session_id == request->context.session_id) &&
            (key->request_id == request->request_id)) {
            return true;
        }
    }
    return false;
}

static void clear_request_cache(gateway_core_t *core)
{
    memset(core->request_keys, 0, sizeof(core->request_keys));
    core->request_key_count = 0U;
}

static void remember_request(gateway_core_t *core, const uart_m_request_t *request)
{
    gateway_request_key_t *key = &core->request_keys[core->request_key_count];

    key->device_context_id = request->context.device_context_id;
    key->session_id = request->context.session_id;
    key->request_id = request->request_id;
    key->used = true;
    core->request_key_count++;
}

static bool observe_uart_candidate(gateway_core_t *core,
                                   const uart_m_message_t *message)
{
    if ((core->config.mock_ports.observe_uart_candidate == NULL) ||
        !core->config.mock_ports.observe_uart_candidate(
            message, core->config.mock_ports.user_context)) {
        core->stats.mock_port_rejects++;
        return false;
    }
    core->stats.uart_candidates++;
    return true;
}

static bool observe_ble_candidate(gateway_core_t *core,
                                  const gateway_ble_message_t *message)
{
    if ((core->config.mock_ports.observe_ble_candidate == NULL) ||
        !core->config.mock_ports.observe_ble_candidate(
            message, core->config.mock_ports.user_context)) {
        core->stats.mock_port_rejects++;
        return false;
    }
    core->stats.ble_candidates++;
    return true;
}

static gateway_core_status_t process_context(gateway_core_t *core,
                                             const gateway_core_context_t *context)
{
    bool session_rotated = core->has_context &&
        ((core->context.identity.device_context_id != context->identity.device_context_id) ||
         (core->context.identity.session_id != context->identity.session_id));

    if (!context_identity_valid(context)) {
        core->stats.context_rejects++;
        return GATEWAY_CORE_CONTEXT_MISMATCH;
    }
    if (session_rotated) {
        core->stats.session_rotations++;
        clear_request_cache(core);
        if (core->query.active) {
            finish_query(core, GATEWAY_QUERY_OUTCOME_SESSION_ROTATED);
        }
    }
    core->context = *context;
    core->has_context = true;
    return GATEWAY_CORE_OK;
}

static gateway_core_status_t process_disconnect(gateway_core_t *core,
                                                uint32_t generation)
{
    if (!core->has_context ||
        (core->context.connection_generation != generation)) {
        return GATEWAY_CORE_CONTEXT_MISMATCH;
    }

    core->stats.disconnects++;
    core->context.connected = false;
    core->context.encrypted = false;
    core->context.authenticated = false;
    core->context.authorization_verified = false;
    core->context.session_ready = false;
    if (core->query.active) {
        finish_query(core, GATEWAY_QUERY_OUTCOME_DISCONNECTED);
    }
    return GATEWAY_CORE_OK;
}

static gateway_core_status_t process_domain_alive(gateway_core_t *core,
                                                  const uart_m_domain_alive_t *alive,
                                                  uint64_t now_ms)
{
    bool changed = (core->domain_boot_id != 0U) &&
                   (core->domain_boot_id != alive->domain_boot_id);

    if (changed) {
        core->stats.domain_boot_changes++;
        if (core->has_context) {
            core->context.session_ready = false;
        }
        if (core->query.active) {
            finish_query(core, GATEWAY_QUERY_OUTCOME_DOMAIN_BOOT_CHANGED);
        }
    }
    core->domain_boot_id = alive->domain_boot_id;
    core->domain_last_seen_ms = now_ms;
    core->domain_link_up = (alive->quality == UART_M_QUALITY_OK) &&
                           (alive->source_age_ms <= core->config.domain_alive_timeout_ms);
    return core->domain_link_up ? GATEWAY_CORE_OK : GATEWAY_CORE_STALE;
}

static gateway_core_status_t process_ble_command(gateway_core_t *core,
                                                 const gateway_core_ble_command_t *command,
                                                 uint64_t now_ms)
{
    uart_m_message_t message;
    gateway_mapper_status_t mapper_status;
    gateway_verified_context_t verified_context;

    if (!context_is_current(core, now_ms)) {
        core->stats.context_rejects++;
        return context_is_admitted(core) ? GATEWAY_CORE_STALE
                                         : GATEWAY_CORE_NOT_READY;
    }
    if (command->connection_generation != core->context.connection_generation) {
        core->stats.context_rejects++;
        return GATEWAY_CORE_CONTEXT_MISMATCH;
    }

    verified_context = core->context.identity;
    mapper_status = gateway_mapper_ble_uplink_to_uart(
        command->message_type, command->payload, command->payload_length,
        &verified_context, &message);
    if (mapper_status != GATEWAY_MAPPER_OK) {
        core->stats.invalid_messages++;
        return GATEWAY_CORE_INVALID_MESSAGE;
    }
    if (!core->domain_link_up) {
        return GATEWAY_CORE_NOT_READY;
    }

    switch (message.type) {
    case UART_M_TYPE_REQUEST:
        if (!core->context.session_ready) {
            return GATEWAY_CORE_NOT_READY;
        }
        if (request_key_exists(core, &message.body.request)) {
            core->stats.duplicate_requests++;
            return GATEWAY_CORE_DUPLICATE_REQUEST;
        }
        if (core->request_key_count >= GATEWAY_CORE_REQUEST_CACHE_DEPTH) {
            core->stats.request_cache_full++;
            return GATEWAY_CORE_REQUEST_CACHE_FULL;
        }
        if (!observe_uart_candidate(core, &message)) {
            return GATEWAY_CORE_MOCK_PORT_REJECTED;
        }
        remember_request(core, &message.body.request);
        return GATEWAY_CORE_OK;

    case UART_M_TYPE_QUERY:
        if (core->query.active) {
            if ((core->query.device_context_id == message.body.query.context.device_context_id) &&
                (core->query.session_id == message.body.query.context.session_id) &&
                (core->query.query_id == message.body.query.query_id) &&
                (core->query.scope == message.body.query.scope)) {
                core->stats.duplicate_queries++;
                return GATEWAY_CORE_DUPLICATE_QUERY;
            }
            return GATEWAY_CORE_QUERY_BUSY;
        }
        if (!observe_uart_candidate(core, &message)) {
            return GATEWAY_CORE_MOCK_PORT_REJECTED;
        }
        memset(&core->query, 0, sizeof(core->query));
        core->query.active = true;
        core->query.device_context_id = message.body.query.context.device_context_id;
        core->query.session_id = message.body.query.context.session_id;
        core->query.domain_boot_id = core->domain_boot_id;
        core->query.query_id = message.body.query.query_id;
        core->query.scope = message.body.query.scope;
        core->query.started_at_ms = now_ms;
        core->last_query_outcome = GATEWAY_QUERY_OUTCOME_NONE;
        core->response_key_count = 0U;
        memset(core->response_keys, 0, sizeof(core->response_keys));
        return GATEWAY_CORE_OK;

    case UART_M_TYPE_WARNING_ACK:
        if (!core->context.session_ready ||
            (message.body.warning_ack.warning_domain_boot != core->domain_boot_id)) {
            return GATEWAY_CORE_CONTEXT_MISMATCH;
        }
        return observe_uart_candidate(core, &message)
                   ? GATEWAY_CORE_OK
                   : GATEWAY_CORE_MOCK_PORT_REJECTED;

    default:
        core->stats.invalid_messages++;
        return GATEWAY_CORE_INVALID_MESSAGE;
    }
}

static uint64_t fnv1a64(const uint8_t *data, size_t length, uint64_t hash)
{
    size_t i;

    for (i = 0U; i < length; ++i) {
        hash ^= (uint64_t)data[i];
        hash *= UINT64_C(1099511628211);
    }
    return hash;
}

static uint64_t response_key(const gateway_ble_message_t *message)
{
    uint64_t hash = UINT64_C(1469598103934665603);

    hash = fnv1a64(&message->message_type, 1U, hash);
    switch (message->message_type) {
    case UART_M_TYPE_RESULT:
        return fnv1a64(&message->payload[20], 12U, hash);
    case UART_M_TYPE_WARNING:
        hash = fnv1a64(&message->payload[20], 5U, hash);
        return fnv1a64(&message->payload[30], 4U, hash);
    case UART_M_TYPE_DIGITAL_RESULT:
        return fnv1a64(&message->payload[20], 4U, hash);
    default:
        return hash;
    }
}

typedef enum
{
    RESPONSE_TRACK_NEW = 0,
    RESPONSE_TRACK_DUPLICATE,
    RESPONSE_TRACK_CAPACITY_OVERFLOW
} response_track_status_t;

static response_track_status_t remember_unique_response(
    gateway_core_t *core,
    const gateway_ble_message_t *message)
{
    uint64_t key = response_key(message);
    size_t i;

    for (i = 0U; i < core->response_key_count; ++i) {
        if (core->response_keys[i].used &&
            (core->response_keys[i].value == key)) {
            return RESPONSE_TRACK_DUPLICATE;
        }
    }
    if (core->response_key_count >= GATEWAY_CORE_MAX_QUERY_RESPONSES) {
        return RESPONSE_TRACK_CAPACITY_OVERFLOW;
    }
    core->response_keys[core->response_key_count].value = key;
    core->response_keys[core->response_key_count].used = true;
    core->response_key_count++;
    core->query.unique_response_count++;
    return RESPONSE_TRACK_NEW;
}

static bool response_matches_scope(uint8_t message_type,
                                   uart_m_query_scope_t scope)
{
    if (scope == UART_M_QUERY_ALL) {
        return true;
    }
    if (scope == UART_M_QUERY_RESULT) {
        return (message_type == (uint8_t)UART_M_TYPE_RESULT) ||
               (message_type == (uint8_t)UART_M_TYPE_DIGITAL_RESULT);
    }
    if (scope == UART_M_QUERY_WARNING) {
        return message_type == (uint8_t)UART_M_TYPE_WARNING;
    }
    if (scope != UART_M_QUERY_STATE) {
        return false;
    }

    switch ((uart_m_message_type_t)message_type) {
    case UART_M_TYPE_AVAILABILITY:
    case UART_M_TYPE_DIGITAL_STATUS:
    case UART_M_TYPE_USER_SETTINGS:
    case UART_M_TYPE_BCM_DOOR_STATE:
    case UART_M_TYPE_BCM_CLIMATE_STATE:
    case UART_M_TYPE_BCM_LIGHT_STATE:
    case UART_M_TYPE_BCM_STATUS:
    case UART_M_TYPE_CIS_ENVIRONMENT:
    case UART_M_TYPE_CIS_OCCUPANT:
    case UART_M_TYPE_CIS_REAR:
    case UART_M_TYPE_CIS_STATUS:
    case UART_M_TYPE_VSS_STATUS:
        return true;
    default:
        return false;
    }
}

static gateway_core_status_t validate_down_context(const gateway_core_t *core,
                                                   const gateway_ble_message_t *message,
                                                   uint32_t *query_id)
{
    uint32_t device_context_id;
    uint64_t session_id;
    uint32_t domain_boot_id;

    if ((message->payload_length < 20U) || !core->has_context) {
        return GATEWAY_CORE_CONTEXT_MISMATCH;
    }
    device_context_id = read_u32_le(&message->payload[0]);
    session_id = read_u64_le(&message->payload[4]);
    domain_boot_id = read_u32_le(&message->payload[12]);
    *query_id = read_u32_le(&message->payload[16]);

    if ((device_context_id != core->context.identity.device_context_id) ||
        (session_id != core->context.identity.session_id) ||
        (domain_boot_id != core->domain_boot_id)) {
        return GATEWAY_CORE_CONTEXT_MISMATCH;
    }
    return GATEWAY_CORE_OK;
}

static gateway_core_status_t process_query_end(gateway_core_t *core,
                                               const gateway_ble_message_t *message)
{
    uint8_t scope = message->payload[20];
    uint8_t status = message->payload[21];
    uint16_t item_count = read_u16_le(&message->payload[22]);

    core->stats.routing_holds++;
    if (!core->query.active || (scope != (uint8_t)core->query.scope)) {
        return GATEWAY_CORE_CONTEXT_MISMATCH;
    }
    if (item_count != core->query.unique_response_count) {
        core->stats.query_count_mismatches++;
        finish_query(core, GATEWAY_QUERY_OUTCOME_COUNT_MISMATCH);
        return GATEWAY_CORE_QUERY_COUNT_MISMATCH;
    }

    switch ((uart_m_query_status_t)status) {
    case UART_M_QUERY_DONE:
        finish_query(core, GATEWAY_QUERY_OUTCOME_COMPLETE_CANDIDATE_HOLD);
        return GATEWAY_CORE_ROUTING_HOLD;
    case UART_M_QUERY_NOT_FOUND:
        if ((scope != (uint8_t)UART_M_QUERY_RESULT) || (item_count != 0U)) {
            finish_query(core, GATEWAY_QUERY_OUTCOME_COUNT_MISMATCH);
            return GATEWAY_CORE_QUERY_COUNT_MISMATCH;
        }
        finish_query(core, GATEWAY_QUERY_OUTCOME_NOT_FOUND_HOLD);
        return GATEWAY_CORE_ROUTING_HOLD;
    case UART_M_QUERY_PARTIAL_UNCONFIRMED:
        finish_query(core, GATEWAY_QUERY_OUTCOME_PARTIAL);
        return GATEWAY_CORE_ROUTING_HOLD;
    case UART_M_QUERY_CONTEXT_REJECTED:
        if (item_count != 0U) {
            finish_query(core, GATEWAY_QUERY_OUTCOME_COUNT_MISMATCH);
            return GATEWAY_CORE_QUERY_COUNT_MISMATCH;
        }
        finish_query(core, GATEWAY_QUERY_OUTCOME_CONTEXT_REJECTED);
        return GATEWAY_CORE_ROUTING_HOLD;
    default:
        finish_query(core, GATEWAY_QUERY_OUTCOME_PARTIAL);
        return GATEWAY_CORE_INVALID_MESSAGE;
    }
}

static gateway_core_status_t process_uart_message(gateway_core_t *core,
                                                  const uart_m_message_t *message,
                                                  uint64_t now_ms)
{
    gateway_ble_message_t ble_message;
    gateway_mapper_status_t mapper_status;
    gateway_core_status_t context_status;
    response_track_status_t response_status;
    uint32_t query_id = 0U;

    if (message->type == UART_M_TYPE_DOMAIN_ALIVE) {
        return process_domain_alive(core, &message->body.domain_alive, now_ms);
    }
    if (!core->domain_link_up || !context_is_current(core, now_ms)) {
        return GATEWAY_CORE_NOT_READY;
    }

    mapper_status = gateway_mapper_uart_downlink_to_ble(message, &ble_message);
    if (mapper_status != GATEWAY_MAPPER_OK) {
        core->stats.invalid_messages++;
        return (mapper_status == GATEWAY_MAPPER_SCHEMA_HOLD)
                   ? GATEWAY_CORE_ROUTING_HOLD
                   : GATEWAY_CORE_INVALID_MESSAGE;
    }
    context_status = validate_down_context(core, &ble_message, &query_id);
    if (context_status != GATEWAY_CORE_OK) {
        core->stats.context_rejects++;
        return context_status;
    }

    if (message->type == UART_M_TYPE_QUERY_END) {
        if (!core->query.active || (query_id != core->query.query_id)) {
            return GATEWAY_CORE_CONTEXT_MISMATCH;
        }
        return process_query_end(core, &ble_message);
    }

    if (query_id != 0U) {
        if (!core->query.active ||
            (query_id != core->query.query_id) ||
            (core->query.device_context_id != core->context.identity.device_context_id) ||
            (core->query.session_id != core->context.identity.session_id) ||
            (core->query.domain_boot_id != core->domain_boot_id)) {
            return GATEWAY_CORE_CONTEXT_MISMATCH;
        }
        if (!response_matches_scope(ble_message.message_type, core->query.scope)) {
            core->stats.invalid_messages++;
            return GATEWAY_CORE_INVALID_MESSAGE;
        }
        response_status = remember_unique_response(core, &ble_message);
        if (response_status == RESPONSE_TRACK_DUPLICATE) {
            core->stats.duplicate_query_responses++;
            return GATEWAY_CORE_DUPLICATE_RESPONSE;
        }
        if (response_status == RESPONSE_TRACK_CAPACITY_OVERFLOW) {
            core->stats.query_capacity_overflows++;
            finish_query(core, GATEWAY_QUERY_OUTCOME_CAPACITY_OVERFLOW);
            return GATEWAY_CORE_QUERY_CAPACITY_OVERFLOW;
        }
    }

    return observe_ble_candidate(core, &ble_message)
               ? GATEWAY_CORE_OK
               : GATEWAY_CORE_MOCK_PORT_REJECTED;
}

gateway_core_config_t gateway_core_isolated_mock_config(void)
{
    gateway_core_config_t config;

    memset(&config, 0, sizeof(config));
    config.queue_depth = GATEWAY_CORE_MAX_QUEUE_DEPTH;
    config.query_timeout_ms = 3000U;
    config.context_stale_after_ms = 3000U;
    config.event_stale_after_ms = 3000U;
    config.domain_alive_timeout_ms = 750U;
    return config;
}

gateway_core_status_t gateway_core_init(gateway_core_t *core,
                                        const gateway_core_config_t *config)
{
    if ((core == NULL) || (config == NULL) || (config->queue_depth == 0U) ||
        (config->queue_depth > GATEWAY_CORE_MAX_QUEUE_DEPTH) ||
        (config->query_timeout_ms == 0U) ||
        (config->context_stale_after_ms == 0U) ||
        (config->event_stale_after_ms == 0U) ||
        (config->domain_alive_timeout_ms == 0U)) {
        return GATEWAY_CORE_INVALID_ARGUMENT;
    }

    memset(core, 0, sizeof(*core));
    core->config = *config;
    core->initialized = true;
    return GATEWAY_CORE_OK;
}

void gateway_core_reset(gateway_core_t *core)
{
    if (core != NULL) {
        memset(core, 0, sizeof(*core));
    }
}

gateway_core_status_t gateway_core_enqueue_context(gateway_core_t *core,
                                                   const gateway_core_context_t *context,
                                                   uint64_t received_at_ms)
{
    gateway_core_event_t event;

    if (context == NULL) {
        return GATEWAY_CORE_INVALID_ARGUMENT;
    }
    memset(&event, 0, sizeof(event));
    event.type = GATEWAY_CORE_EVENT_CONTEXT;
    event.received_at_ms = received_at_ms;
    event.data.context = *context;
    return enqueue_event(core, &event);
}

gateway_core_status_t gateway_core_enqueue_ble_command(gateway_core_t *core,
                                                       uint8_t message_type,
                                                       const uint8_t *payload,
                                                       size_t payload_length,
                                                       uint32_t connection_generation,
                                                       uint64_t received_at_ms)
{
    gateway_core_event_t event;

    if ((payload == NULL) || (payload_length == 0U) ||
        (payload_length > GATEWAY_CORE_MAX_BLE_UPLINK_PAYLOAD)) {
        return GATEWAY_CORE_INVALID_ARGUMENT;
    }
    memset(&event, 0, sizeof(event));
    event.type = GATEWAY_CORE_EVENT_BLE_COMMAND;
    event.received_at_ms = received_at_ms;
    event.data.ble_command.message_type = message_type;
    event.data.ble_command.payload_length = payload_length;
    event.data.ble_command.connection_generation = connection_generation;
    memcpy(event.data.ble_command.payload, payload, payload_length);
    return enqueue_event(core, &event);
}

gateway_core_status_t gateway_core_enqueue_uart_message(gateway_core_t *core,
                                                        const uart_m_message_t *message,
                                                        uint64_t received_at_ms)
{
    gateway_core_event_t event;

    if (message == NULL) {
        return GATEWAY_CORE_INVALID_ARGUMENT;
    }
    memset(&event, 0, sizeof(event));
    event.type = GATEWAY_CORE_EVENT_UART_MESSAGE;
    event.received_at_ms = received_at_ms;
    event.data.uart_message = *message;
    return enqueue_event(core, &event);
}

gateway_core_status_t gateway_core_enqueue_disconnect(gateway_core_t *core,
                                                      uint32_t connection_generation,
                                                      uint64_t received_at_ms)
{
    gateway_core_event_t event;

    memset(&event, 0, sizeof(event));
    event.type = GATEWAY_CORE_EVENT_DISCONNECT;
    event.received_at_ms = received_at_ms;
    event.data.disconnect_generation = connection_generation;
    return enqueue_event(core, &event);
}

gateway_core_status_t gateway_core_enqueue_tick(gateway_core_t *core,
                                                uint64_t now_ms)
{
    gateway_core_event_t event;

    memset(&event, 0, sizeof(event));
    event.type = GATEWAY_CORE_EVENT_TICK;
    event.received_at_ms = now_ms;
    return enqueue_event(core, &event);
}

gateway_core_status_t gateway_core_process_next(gateway_core_t *core,
                                                uint64_t now_ms)
{
    gateway_core_event_t event;
    gateway_core_status_t status;

    if ((core == NULL) || !core->initialized) {
        return GATEWAY_CORE_INVALID_ARGUMENT;
    }
    expire_state(core, now_ms);
    if (core->queue_count == 0U) {
        return GATEWAY_CORE_NO_EVENT;
    }

    event = dequeue_event(core);
    core->stats.events_processed++;
    if (elapsed_more_than(now_ms, event.received_at_ms,
                          core->config.event_stale_after_ms)) {
        core->stats.stale_events++;
        return GATEWAY_CORE_STALE;
    }

    switch (event.type) {
    case GATEWAY_CORE_EVENT_CONTEXT:
        status = process_context(core, &event.data.context);
        break;
    case GATEWAY_CORE_EVENT_BLE_COMMAND:
        status = process_ble_command(core, &event.data.ble_command, now_ms);
        break;
    case GATEWAY_CORE_EVENT_UART_MESSAGE:
        status = process_uart_message(core, &event.data.uart_message, now_ms);
        break;
    case GATEWAY_CORE_EVENT_DISCONNECT:
        status = process_disconnect(core, event.data.disconnect_generation);
        break;
    case GATEWAY_CORE_EVENT_TICK:
        expire_state(core, now_ms);
        status = GATEWAY_CORE_OK;
        break;
    default:
        status = GATEWAY_CORE_INVALID_MESSAGE;
        break;
    }
    return status;
}

void gateway_core_get_stats(const gateway_core_t *core,
                            gateway_core_stats_t *out_stats)
{
    if ((core != NULL) && (out_stats != NULL)) {
        *out_stats = core->stats;
    }
}

void gateway_core_get_query_state(const gateway_core_t *core,
                                  gateway_query_state_t *out_query,
                                  gateway_query_outcome_t *out_last_outcome)
{
    if ((core != NULL) && (out_query != NULL)) {
        *out_query = core->query;
    }
    if ((core != NULL) && (out_last_outcome != NULL)) {
        *out_last_outcome = core->last_query_outcome;
    }
}

bool gateway_core_is_domain_link_up(const gateway_core_t *core)
{
    return (core != NULL) && core->initialized && core->domain_link_up;
}

bool gateway_core_production_routing_enabled(void)
{
    return GATEWAY_CORE_PRODUCTION_ROUTING_ENABLED != 0;
}
