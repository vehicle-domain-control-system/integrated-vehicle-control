#include <stdio.h>
#include <string.h>

#include "Domain_Interface_v0.5_C2_target.h"
#include "digital_key_context_manager.h"
#include "domain_interface_digital_key_binding.h"
#include "gateway_context_dto.h"
#include "gateway_context_source_metadata.h"
#include "network/net_domain_c2.h"
#include "network/net_upper.h"
#include "network/net_upper_uart.h"
#include "uart_m_frame.h"

#define CHECK(condition)                                                     \
    do {                                                                     \
        if (!(condition)) {                                                  \
            fprintf(stderr, "line %d: %s\n", __LINE__, #condition);       \
            return false;                                                    \
        }                                                                    \
    } while (0)

static NetUpper s_network;
static NetDomainC2 s_adapter;
static DigitalKeyContextManager_t s_manager;
static uint64_t s_now_ms = 1000U;

static NetStatus clock_read(void *context, uint64_t *out_now_ms)
{
    (void)context;
    *out_now_ms = s_now_ms;
    return NET_OK;
}

static NetStatus submit_hold(void *context, const NetUpperPacket *packet,
                             NetTxKey key)
{
    (void)context;
    (void)packet;
    (void)key;
    return NET_HOLD;
}

static NetStatus poll_empty(void *context, NetUpperEvent *out_event)
{
    (void)context;
    (void)out_event;
    return NET_NO_WORK;
}

static const NetUpperPortOps s_port_ops = {
    clock_read,
    submit_hold,
    poll_empty,
};

static p6_authorization_snapshot_t authorized_snapshot(uint32_t device_id,
                                                        uint64_t session_id,
                                                        uint32_t generation,
                                                        bool app_active)
{
    p6_authorization_snapshot_t snapshot;

    memset(&snapshot, 0, sizeof(snapshot));
    snapshot.state = P6_SESSION_STATE_AUTHORIZED;
    snapshot.reset_state = P6_RESET_STATE_NONE;
    snapshot.registered = true;
    snapshot.registration_bond_reconciled = true;
    snapshot.registration_bond_present = true;
    snapshot.connected = true;
    snapshot.bonded = true;
    snapshot.encrypted = true;
    snapshot.authorization_verified = true;
    snapshot.session_valid = true;
    snapshot.device_context_id = device_id;
    snapshot.session_id = session_id;
    snapshot.connection_generation = generation;
    snapshot.app_instance_known = true;
    snapshot.app_instance_id = UINT32_C(0x10203040);
    snapshot.app_active = app_active;
    return snapshot;
}

static bool start_domain_path(void)
{
    NetDomainC2Api api;
    NetDomainC2Config binding;
    NetUpperConfig config;
    size_t index;

    memset(&api, 0, sizeof(api));
    memset(&binding, 0, sizeof(binding));
    memset(&config, 0, sizeof(config));
    CHECK(DomainIf_Init() == DOMAIN_IF_OK);
    CHECK(DigitalKeyContextManager_Init(&s_manager) ==
          DIGITAL_KEY_CONTEXT_OK);
    CHECK(DomainIfDigitalKeyBinding_Configure(&s_manager) == DOMAIN_IF_OK);

    api.IsInitialized = DomainIf_IsInitialized;
    api.SetRxObserver = DomainIf_SetRxObserver;
    api.SetTxPort = DomainIf_SetTxPort;
    api.RxDigitalKeyConnection = DomainIf_RxDigitalKeyConnection;
    api.RxProximity = DomainIf_RxProximity;
    binding.domain_boot = 301U;
    binding.first_can_sequence = 1U;
    binding.value_quality = net_domain_c2_value_quality_provisional;
    CHECK(net_domain_c2_init(&s_adapter, &s_network, NULL, &api, &binding) ==
          NET_OK);

    config.node = NET_WIRE_NODE_DOMAIN;
    config.local_epoch = 1U;
    config.profile = NET_WIRE_CANDIDATE_ONLY;
    config.port.ops = &s_port_ops;
    config.owner = net_domain_c2_owner(&s_adapter);
    for (index = 0U; index < 3U; ++index) {
        config.port_epoch[index] = 1U;
    }
    for (index = 0U; index < NET_UPPER_ROUTE_COUNT; ++index) {
        config.rx_policy[index].bound = true;
        config.rx_policy[index].maximum_age_ms = 1000U;
    }
    for (index = 0U; index < 2U; ++index) {
        config.uart_policy[index].candidate_time_policy_selected = true;
        config.uart_policy[index].assembly_limit_ms = 100U;
    }
    CHECK(net_upper_init(&s_network, &config) == NET_OK);
    CHECK(net_domain_c2_register(&s_adapter) == NET_OK);
    return true;
}

static bool encode_esp_context(const p6_authorization_snapshot_t *snapshot,
                               gateway_context_source_metadata_t *metadata,
                               uint64_t now_ms,
                               uint16_t link_sequence,
                               uint8_t *frame,
                               size_t *frame_length,
                               uart_m_context_t *context_out)
{
    gateway_context_sources_t sources;
    uint8_t payload[UART_M_MAX_PAYLOAD_SIZE];
    size_t payload_length = 0U;

    gateway_context_source_build(metadata, snapshot, true, now_ms, &sources);
    CHECK(gateway_context_dto_from_p6_snapshot(snapshot, &sources,
                                               context_out) ==
          GATEWAY_MAPPER_OK);
    CHECK(gateway_mapper_encode_context_schema(context_out, payload,
                                               sizeof(payload),
                                               &payload_length) ==
          GATEWAY_MAPPER_OK);
    CHECK(payload_length == 38U);
    CHECK(uart_m_frame_encode((uint8_t)UART_M_TYPE_CONTEXT, link_sequence,
                              payload, payload_length, frame,
                              UART_M_MAX_FRAME_SIZE, frame_length) ==
          UART_M_FRAME_STATUS_OK);
    return true;
}

static NetStatus decode_and_deliver(const uint8_t *bytes, size_t length,
                                    NetUpperUartMessage *decoded_out)
{
    NetWireUartFrame frame;
    NetUpperUartDelivery delivery;
    NetUpperDelivery metadata;
    NetUpperOwner owner = net_domain_c2_owner(&s_adapter);
    NetWireStatus decode_status;

    memset(&frame, 0, sizeof(frame));
    memset(&delivery, 0, sizeof(delivery));
    memset(&metadata, 0, sizeof(metadata));
    if (length > sizeof(frame.bytes)) {
        return NET_INVALID_ARGUMENT;
    }
    memcpy(frame.bytes, bytes, length);
    frame.length = length;
    frame.captured_at_ms = s_now_ms;
    decode_status = net_upper_uart_decode(
        NET_WIRE_CANDIDATE_ONLY, NET_WIRE_UART_M, NET_WIRE_UPLINK, &frame,
        decoded_out);
    if (decode_status != NET_WIRE_OK) {
        return NET_REJECTED;
    }

    delivery.message = *decoded_out;
    delivery.frame = frame;
    delivery.port_epoch = 1U;
    metadata.capture.timestamp_known = true;
    metadata.capture.port_epoch = 1U;
    metadata.capture.captured_at_ms = s_now_ms;
    metadata.delivered_at_ms = s_now_ms;
    metadata.local_elapsed_ms = 0U;
    metadata.freshness_known = true;
    return owner.uart(owner.context, &delivery, &metadata);
}

static bool test_esp_network_domain_context_path(void)
{
    gateway_context_source_policy_t policy = {
        .registration_connection_ttl_ms = 20U,
        .app_ttl_ms = 20U,
    };
    gateway_context_source_metadata_t source;
    p6_authorization_snapshot_t snapshot =
        authorized_snapshot(7U, UINT64_C(0x1122334455667788), 1U, true);
    uart_m_context_t encoded_context;
    NetUpperUartMessage decoded;
    DigitalKeyContextSnapshot_t domain_snapshot;
    uint8_t frame[UART_M_MAX_FRAME_SIZE];
    size_t frame_length = 0U;

    gateway_context_source_metadata_init(&source, &policy);
    CHECK(gateway_context_source_observe_registration_connection(
        &source, &snapshot, true, 100U));
    CHECK(gateway_context_source_observe_app(&source, &snapshot, true, 100U));
    CHECK(encode_esp_context(&snapshot, &source, 105U, 1U, frame,
                             &frame_length, &encoded_context));
    s_now_ms = 1000U;
    CHECK(decode_and_deliver(frame, frame_length, &decoded) == NET_OK);
    CHECK(decoded.type == 0x01U && decoded.sequence == 1U);
    CHECK(decoded.data.m_context.device_context_id ==
          encoded_context.context.device_context_id);
    CHECK(decoded.data.m_context.session_id ==
          encoded_context.context.session_id);
    CHECK(decoded.data.m_context.app_quality == UART_M_QUALITY_OK);
    CHECK(decoded.data.m_context.app_update_sequence == 1U);
    CHECK(decoded.data.m_context.app_age_ms == 5U);
    CHECK(decoded.data.m_context.registration_connection_quality ==
          UART_M_QUALITY_OK);
    CHECK(decoded.data.m_context.registration_connection_update_sequence ==
          1U);
    CHECK(decoded.data.m_context.registration_connection_age_ms == 5U);
    CHECK(DigitalKeyContextManager_GetSnapshot(&s_manager, &domain_snapshot) ==
          DIGITAL_KEY_CONTEXT_OK);
    CHECK(domain_snapshot.hasConnection && domain_snapshot.hasProximity);
    CHECK(domain_snapshot.deviceContextId == 7U);
    CHECK(domain_snapshot.connection.sessionId ==
          UINT64_C(0x1122334455667788));
    CHECK(domain_snapshot.connection.meta.quality == DATA_QUALITY_OK);
    CHECK(domain_snapshot.connection.meta.updateSequence == 1U);
    CHECK(domain_snapshot.connection.meta.ageMs == 5U);
    CHECK(domain_snapshot.connection.appMeta.quality == DATA_QUALITY_OK);
    CHECK(domain_snapshot.proximity.state == PROXIMITY_UNKNOWN);
    CHECK(domain_snapshot.proximity.meta.quality == DATA_QUALITY_NO_DATA);

    /* Same observation at TTL equality is a quality restriction, not a new
     * source sequence. The actual Domain manager accepts this transition. */
    CHECK(encode_esp_context(&snapshot, &source, 120U, 2U, frame,
                             &frame_length, &encoded_context));
    s_now_ms = 1001U;
    CHECK(decode_and_deliver(frame, frame_length, &decoded) == NET_OK);
    CHECK(decoded.data.m_context.app_quality == UART_M_QUALITY_STALE);
    CHECK(decoded.data.m_context.app_update_sequence == 1U);
    CHECK(decoded.data.m_context.app_age_ms == 20U);
    CHECK(DigitalKeyContextManager_GetSnapshot(&s_manager, &domain_snapshot) ==
          DIGITAL_KEY_CONTEXT_OK);
    CHECK(domain_snapshot.connection.meta.quality == DATA_QUALITY_STALE);
    CHECK(domain_snapshot.connection.appMeta.quality == DATA_QUALITY_STALE);

    snapshot.session_id = UINT64_C(0x8877665544332211);
    snapshot.connection_generation = 2U;
    CHECK(gateway_context_source_observe_registration_connection(
        &source, &snapshot, true, 130U));
    CHECK(gateway_context_source_observe_app(&source, &snapshot, true, 130U));
    CHECK(encode_esp_context(&snapshot, &source, 131U, 3U, frame,
                             &frame_length, &encoded_context));
    s_now_ms = 1002U;
    CHECK(decode_and_deliver(frame, frame_length, &decoded) == NET_OK);
    CHECK(DigitalKeyContextManager_GetSnapshot(&s_manager, &domain_snapshot) ==
          DIGITAL_KEY_CONTEXT_OK);
    CHECK(domain_snapshot.connection.sessionId ==
          UINT64_C(0x8877665544332211));
    CHECK(domain_snapshot.connection.meta.updateSequence == 2U);
    CHECK(domain_snapshot.connection.appMeta.updateSequence == 2U);
    return true;
}

static bool test_no_data_is_preserved(void)
{
    gateway_context_source_policy_t disabled_policy = {0};
    gateway_context_source_metadata_t source;
    p6_authorization_snapshot_t snapshot =
        authorized_snapshot(8U, UINT64_C(0x1020304050607080), 3U, false);
    uart_m_context_t encoded_context;
    NetUpperUartMessage decoded;
    DigitalKeyContextSnapshot_t domain_snapshot;
    uint8_t frame[UART_M_MAX_FRAME_SIZE];
    size_t frame_length = 0U;

    /* Isolate this semantic case while keeping the same actual bound manager. */
    CHECK(DigitalKeyContextManager_Init(&s_manager) ==
          DIGITAL_KEY_CONTEXT_OK);
    gateway_context_source_metadata_init(&source, &disabled_policy);
    CHECK(gateway_context_source_observe_registration_connection(
        &source, &snapshot, true, 200U));
    CHECK(gateway_context_source_observe_app(&source, &snapshot, true, 200U));
    CHECK(encode_esp_context(&snapshot, &source, 201U, 4U, frame,
                             &frame_length, &encoded_context));
    CHECK(encoded_context.registration_connection_quality ==
          UART_M_QUALITY_NO_DATA);
    CHECK(encoded_context.registration_connection_update_sequence == 0U);
    CHECK(encoded_context.registration_connection_age_ms == UINT16_MAX);
    CHECK(encoded_context.app_quality == UART_M_QUALITY_NO_DATA);
    s_now_ms = 1003U;
    CHECK(decode_and_deliver(frame, frame_length, &decoded) == NET_OK);
    CHECK(decoded.data.m_context.app_quality == UART_M_QUALITY_NO_DATA);
    CHECK(decoded.data.m_context.proximity_state == UINT8_MAX);
    CHECK(DigitalKeyContextManager_GetSnapshot(&s_manager, &domain_snapshot) ==
          DIGITAL_KEY_CONTEXT_OK);
    CHECK(domain_snapshot.connection.meta.quality == DATA_QUALITY_NO_DATA);
    CHECK(domain_snapshot.connection.appMeta.quality == DATA_QUALITY_NO_DATA);
    CHECK(domain_snapshot.proximity.state == PROXIMITY_UNKNOWN);
    CHECK(domain_snapshot.proximity.meta.quality == DATA_QUALITY_NO_DATA);
    return true;
}

int main(void)
{
    if (!start_domain_path() || !test_esp_network_domain_context_path() ||
        !test_no_data_is_preserved()) {
        return 1;
    }
    puts("ESP32 encoder -> Network decoder/C2 -> Domain context manager PASS");
    return 0;
}
