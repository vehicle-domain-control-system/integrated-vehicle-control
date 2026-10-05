#include <assert.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "GatewayRouter.h"
#include "GatewayLifecycleManager.h"
#include "LinkStateManager.h"
#include "MessageContextManager.h"

typedef struct
{
    int send_to_domain_calls;
    int send_to_mobile_calls;

    uint8_t last_request_id[16];
    size_t last_request_id_length;

    uint8_t last_payload[32];
    size_t last_payload_length;
} RouterTestTransport_t;

static Gateway_ByteView_t make_view(
    const uint8_t *data,
    size_t length)
{
    Gateway_ByteView_t view = {
        .data = (length > 0U) ? data : NULL,
        .length = length
    };

    return view;
}

static Gateway_MessageContextView_t make_context(
    const uint8_t *vehicle,
    size_t vehicle_len,
    const uint8_t *device,
    size_t device_len,
    const uint8_t *session,
    size_t session_len,
    const uint8_t *request,
    size_t request_len)
{
    Gateway_MessageContextView_t context = {
        .vehicle_id = make_view(vehicle, vehicle_len),
        .device_context_id = make_view(device, device_len),
        .session_id = make_view(session, session_len),
        .request_id = make_view(request, request_len)
    };

    return context;
}

static Gateway_Status_t transport_send_to_domain(
    const Gateway_RelayMessageView_t *message,
    void *user_context)
{
    RouterTestTransport_t *transport =
        (RouterTestTransport_t *)user_context;

    assert(message != NULL);
    assert(
        message->direction
        == GATEWAY_DIRECTION_MOBILE_TO_DOMAIN);

    transport->send_to_domain_calls++;

    transport->last_request_id_length =
        message->context.request_id.length;

    if (message->context.request_id.length > 0U)
    {
        assert(
            message->context.request_id.length
            <= sizeof(transport->last_request_id));

        memcpy(
            transport->last_request_id,
            message->context.request_id.data,
            message->context.request_id.length);
    }

    transport->last_payload_length = message->payload.length;

    if (message->payload.length > 0U)
    {
        assert(
            message->payload.length
            <= sizeof(transport->last_payload));

        memcpy(
            transport->last_payload,
            message->payload.data,
            message->payload.length);
    }

    return GATEWAY_STATUS_OK;
}

static Gateway_Status_t transport_send_to_mobile(
    const Gateway_RelayMessageView_t *message,
    void *user_context)
{
    RouterTestTransport_t *transport =
        (RouterTestTransport_t *)user_context;

    assert(message != NULL);
    assert(
        message->direction
        == GATEWAY_DIRECTION_DOMAIN_TO_MOBILE);

    transport->send_to_mobile_calls++;

    transport->last_request_id_length =
        message->context.request_id.length;

    if (message->context.request_id.length > 0U)
    {
        assert(
            message->context.request_id.length
            <= sizeof(transport->last_request_id));

        memcpy(
            transport->last_request_id,
            message->context.request_id.data,
            message->context.request_id.length);
    }

    transport->last_payload_length = message->payload.length;

    if (message->payload.length > 0U)
    {
        assert(
            message->payload.length
            <= sizeof(transport->last_payload));

        memcpy(
            transport->last_payload,
            message->payload.data,
            message->payload.length);
    }

    return GATEWAY_STATUS_OK;
}

static Gateway_Status_t unused_registration_sender(
    const Gateway_RegistrationConnectionUpdate_t *update,
    void *user_context)
{
    (void)update;
    (void)user_context;
    return GATEWAY_STATUS_OK;
}

static Gateway_Status_t unused_app_sender(
    const Gateway_AppActivityUpdate_t *update,
    void *user_context)
{
    (void)update;
    (void)user_context;
    return GATEWAY_STATUS_OK;
}

static Gateway_Status_t unused_proximity_sender(
    const Gateway_ProximityUpdate_t *update,
    void *user_context)
{
    (void)update;
    (void)user_context;
    return GATEWAY_STATUS_OK;
}

static void setup_router(
    RouterTestTransport_t *transport,
    const Gateway_MessageContextView_t *active_session)
{
    Gateway_InterfaceCoreHandlers_t core_handlers;
    Gateway_InterfaceTransportPorts_t transport_ports = {
        .send_to_domain = transport_send_to_domain,
        .send_to_mobile = transport_send_to_mobile,
        .send_registration_to_domain = unused_registration_sender,
        .send_app_activity_to_domain = unused_app_sender,
        .send_proximity_to_domain = unused_proximity_sender,
        .user_context = transport
    };

    Gateway_Interface_Reset();
    LinkStateManager_Reset();
    MessageContextManager_Reset();
    GatewayLifecycleManager_Reset();
    GatewayRouter_Reset();

    memset(transport, 0, sizeof(*transport));

    assert(LinkStateManager_Init() == GATEWAY_STATUS_OK);
    assert(MessageContextManager_Init() == GATEWAY_STATUS_OK);
    assert(GatewayLifecycleManager_Init() == GATEWAY_STATUS_OK);
    assert(GatewayRouter_Init() == GATEWAY_STATUS_OK);

    assert(
        MessageContextManager_ActivateSessionAt(
            active_session,
            100U)
        == GATEWAY_STATUS_OK);

    assert(
        LinkStateManager_MarkAvailableAt(
            GATEWAY_LINK_BLUETOOTH,
            100U)
        == GATEWAY_STATUS_OK);

    assert(
        LinkStateManager_MarkAvailableAt(
            GATEWAY_LINK_DOMAIN_UART,
            100U)
        == GATEWAY_STATUS_OK);

    assert(
        GatewayLifecycleManager_StartAt(100U)
        == GATEWAY_STATUS_OK);

    assert(
        GatewayLifecycleManager_MarkSynchronizationCompleteAt(
            101U)
        == GATEWAY_STATUS_OK);

    assert(
        GatewayRouter_BuildCoreHandlers(&core_handlers)
        == GATEWAY_STATUS_OK);

    assert(
        Gateway_Interface_Init(
            &core_handlers,
            &transport_ports)
        == GATEWAY_STATUS_OK);
}

static void test_mobile_to_domain_forward(void)
{
    static const uint8_t vehicle[] = {0x01U};
    static const uint8_t device[] = {0x02U};
    static const uint8_t session[] = {0x03U};
    static const uint8_t request[] = {0xA0U, 0xA1U};
    static const uint8_t payload[] = {0x10U, 0x20U, 0x30U};

    const Gateway_MessageContextView_t context = make_context(
        vehicle, sizeof(vehicle),
        device, sizeof(device),
        session, sizeof(session),
        request, sizeof(request));

    Gateway_RelayMessageView_t message = {
        .type = GATEWAY_LOGICAL_MSG_MOBILE_REQUEST,
        .direction = GATEWAY_DIRECTION_MOBILE_TO_DOMAIN,
        .context = context,
        .payload = make_view(payload, sizeof(payload))
    };

    RouterTestTransport_t transport;

    setup_router(&transport, &context);

    assert(
        LinkStateManager_MarkAvailableAt(
            GATEWAY_LINK_DOMAIN_UART,
            110U)
        == GATEWAY_STATUS_OK);

    assert(
        Gateway_Interface_OnMobileMessage(&message)
        == GATEWAY_STATUS_OK);

    assert(transport.send_to_domain_calls == 1);
    assert(transport.last_request_id_length == sizeof(request));
    assert(
        memcmp(
            transport.last_request_id,
            request,
            sizeof(request)) == 0);

    assert(transport.last_payload_length == sizeof(payload));
    assert(
        memcmp(
            transport.last_payload,
            payload,
            sizeof(payload)) == 0);
}

static void test_domain_to_mobile_forward(void)
{
    static const uint8_t vehicle[] = {0x01U};
    static const uint8_t device[] = {0x02U};
    static const uint8_t session[] = {0x03U};
    static const uint8_t request[] = {0xA0U};
    static const uint8_t payload[] = {0x55U};

    const Gateway_MessageContextView_t context = make_context(
        vehicle, sizeof(vehicle),
        device, sizeof(device),
        session, sizeof(session),
        request, sizeof(request));

    Gateway_RelayMessageView_t message = {
        .type = GATEWAY_LOGICAL_MSG_REQUEST_RESULT,
        .direction = GATEWAY_DIRECTION_DOMAIN_TO_MOBILE,
        .context = context,
        .payload = make_view(payload, sizeof(payload))
    };

    RouterTestTransport_t transport;

    setup_router(&transport, &context);

    assert(
        LinkStateManager_MarkAvailableAt(
            GATEWAY_LINK_BLUETOOTH,
            110U)
        == GATEWAY_STATUS_OK);

    assert(
        Gateway_Interface_OnDomainMessage(&message)
        == GATEWAY_STATUS_OK);

    assert(transport.send_to_mobile_calls == 1);
    assert(transport.last_request_id_length == sizeof(request));
    assert(transport.last_request_id[0] == request[0]);
    assert(transport.last_payload_length == sizeof(payload));
    assert(transport.last_payload[0] == payload[0]);
}

static void test_uart_unavailable_does_not_queue_or_send(void)
{
    static const uint8_t vehicle[] = {0x01U};
    static const uint8_t session[] = {0x03U};
    static const uint8_t request[] = {0xA0U};

    const Gateway_MessageContextView_t context = make_context(
        vehicle, sizeof(vehicle),
        NULL, 0U,
        session, sizeof(session),
        request, sizeof(request));

    Gateway_RelayMessageView_t message = {
        .type = GATEWAY_LOGICAL_MSG_MOBILE_REQUEST,
        .direction = GATEWAY_DIRECTION_MOBILE_TO_DOMAIN,
        .context = context,
        .payload = { .data = NULL, .length = 0U }
    };

    RouterTestTransport_t transport;

    setup_router(&transport, &context);

    assert(
        LinkStateManager_MarkUnavailableAt(
            GATEWAY_LINK_DOMAIN_UART,
            110U)
        == GATEWAY_STATUS_OK);

    assert(
        GatewayLifecycleManager_UpdateAt(110U)
        == GATEWAY_STATUS_OK);

    assert(
        Gateway_Interface_OnMobileMessage(&message)
        == GATEWAY_STATUS_NOT_READY);

    assert(transport.send_to_domain_calls == 0);

    /*
     * 이후 UART가 복구되어도 Router 내부에 Queue가 없으므로
     * 이전 Request가 자동 전송되지 않는다.
     */
    assert(
        LinkStateManager_MarkAvailableAt(
            GATEWAY_LINK_DOMAIN_UART,
            200U)
        == GATEWAY_STATUS_OK);

    assert(transport.send_to_domain_calls == 0);
}

static void test_bluetooth_unavailable_does_not_send(void)
{
    static const uint8_t vehicle[] = {0x01U};
    static const uint8_t session[] = {0x03U};

    const Gateway_MessageContextView_t context = make_context(
        vehicle, sizeof(vehicle),
        NULL, 0U,
        session, sizeof(session),
        NULL, 0U);

    Gateway_RelayMessageView_t message = {
        .type = GATEWAY_LOGICAL_MSG_VEHICLE_STATE,
        .direction = GATEWAY_DIRECTION_DOMAIN_TO_MOBILE,
        .context = context,
        .payload = { .data = NULL, .length = 0U }
    };

    RouterTestTransport_t transport;

    setup_router(&transport, &context);

    assert(
        LinkStateManager_MarkUnavailableAt(
            GATEWAY_LINK_BLUETOOTH,
            110U)
        == GATEWAY_STATUS_OK);

    assert(
        GatewayLifecycleManager_UpdateAt(110U)
        == GATEWAY_STATUS_OK);

    assert(
        Gateway_Interface_OnDomainMessage(&message)
        == GATEWAY_STATUS_NOT_READY);

    assert(transport.send_to_mobile_calls == 0);
}

static void test_old_session_message_is_rejected(void)
{
    static const uint8_t vehicle[] = {0x01U};
    static const uint8_t old_session[] = {0x10U};
    static const uint8_t new_session[] = {0x20U};
    static const uint8_t request[] = {0x30U};

    const Gateway_MessageContextView_t old_context = make_context(
        vehicle, sizeof(vehicle),
        NULL, 0U,
        old_session, sizeof(old_session),
        request, sizeof(request));

    const Gateway_MessageContextView_t new_context = make_context(
        vehicle, sizeof(vehicle),
        NULL, 0U,
        new_session, sizeof(new_session),
        NULL, 0U);

    Gateway_RelayMessageView_t delayed_result = {
        .type = GATEWAY_LOGICAL_MSG_REQUEST_RESULT,
        .direction = GATEWAY_DIRECTION_DOMAIN_TO_MOBILE,
        .context = old_context,
        .payload = { .data = NULL, .length = 0U }
    };

    RouterTestTransport_t transport;

    setup_router(&transport, &old_context);

    assert(
        LinkStateManager_MarkAvailableAt(
            GATEWAY_LINK_BLUETOOTH,
            110U)
        == GATEWAY_STATUS_OK);

    assert(
        GatewayLifecycleManager_RequestResynchronizationAt(
            150U)
        == GATEWAY_STATUS_OK);

    assert(
        MessageContextManager_ActivateSessionAt(
            &new_context,
            160U)
        == GATEWAY_STATUS_OK);

    assert(
        GatewayLifecycleManager_UpdateAt(160U)
        == GATEWAY_STATUS_OK);

    assert(
        GatewayLifecycleManager_MarkSynchronizationCompleteAt(
            170U)
        == GATEWAY_STATUS_OK);

    assert(
        Gateway_Interface_OnDomainMessage(&delayed_result)
        == GATEWAY_STATUS_NOT_READY);

    assert(transport.send_to_mobile_calls == 0);
}

static void test_payload_is_not_modified(void)
{
    static const uint8_t vehicle[] = {0x01U};
    static const uint8_t session[] = {0x02U};
    static const uint8_t request[] = {0x03U};
    static const uint8_t payload[] = {
        0xDEU, 0xADU, 0xBEU, 0xEFU
    };

    const Gateway_MessageContextView_t context = make_context(
        vehicle, sizeof(vehicle),
        NULL, 0U,
        session, sizeof(session),
        request, sizeof(request));

    Gateway_RelayMessageView_t message = {
        .type = GATEWAY_LOGICAL_MSG_MOBILE_REQUEST,
        .direction = GATEWAY_DIRECTION_MOBILE_TO_DOMAIN,
        .context = context,
        .payload = make_view(payload, sizeof(payload))
    };

    RouterTestTransport_t transport;

    setup_router(&transport, &context);

    assert(
        LinkStateManager_MarkAvailableAt(
            GATEWAY_LINK_DOMAIN_UART,
            110U)
        == GATEWAY_STATUS_OK);

    assert(
        Gateway_Interface_OnMobileMessage(&message)
        == GATEWAY_STATUS_OK);

    assert(transport.last_payload_length == sizeof(payload));
    assert(
        memcmp(
            transport.last_payload,
            payload,
            sizeof(payload)) == 0);
}

static void test_invalid_direction_is_rejected(void)
{
    static const uint8_t vehicle[] = {0x01U};
    static const uint8_t session[] = {0x02U};

    const Gateway_MessageContextView_t context = make_context(
        vehicle, sizeof(vehicle),
        NULL, 0U,
        session, sizeof(session),
        NULL, 0U);

    Gateway_RelayMessageView_t invalid = {
        .type = GATEWAY_LOGICAL_MSG_WARNING,
        .direction = GATEWAY_DIRECTION_MOBILE_TO_DOMAIN,
        .context = context,
        .payload = { .data = NULL, .length = 0U }
    };

    RouterTestTransport_t transport;
    GatewayRouter_Snapshot_t snapshot;

    setup_router(&transport, &context);

    /*
     * Gateway_Interface가 먼저 Validation하므로 Router callback까지
     * 들어오지 않는다.
     */
    assert(
        Gateway_Interface_OnMobileMessage(&invalid)
        == GATEWAY_STATUS_INVALID_ARGUMENT);

    assert(
        GatewayRouter_GetSnapshot(&snapshot)
        == GATEWAY_STATUS_OK);

    assert(snapshot.mobile_to_domain_forwarded == 0U);
    assert(transport.send_to_domain_calls == 0);
}

static void test_router_snapshot(void)
{
    static const uint8_t vehicle[] = {0x01U};
    static const uint8_t session[] = {0x02U};

    const Gateway_MessageContextView_t context = make_context(
        vehicle, sizeof(vehicle),
        NULL, 0U,
        session, sizeof(session),
        NULL, 0U);

    Gateway_RelayMessageView_t state = {
        .type = GATEWAY_LOGICAL_MSG_VEHICLE_STATE,
        .direction = GATEWAY_DIRECTION_DOMAIN_TO_MOBILE,
        .context = context,
        .payload = { .data = NULL, .length = 0U }
    };

    RouterTestTransport_t transport;
    GatewayRouter_Snapshot_t snapshot;

    setup_router(&transport, &context);

    assert(
        LinkStateManager_MarkAvailableAt(
            GATEWAY_LINK_BLUETOOTH,
            100U)
        == GATEWAY_STATUS_OK);

    assert(
        Gateway_Interface_OnDomainMessage(&state)
        == GATEWAY_STATUS_OK);

    assert(
        GatewayRouter_GetSnapshot(&snapshot)
        == GATEWAY_STATUS_OK);

    assert(snapshot.initialized);
    assert(snapshot.domain_to_mobile_forwarded == 1U);
    assert(snapshot.mobile_to_domain_forwarded == 0U);
}

static void test_router_dependency_not_ready(void)
{
    Gateway_InterfaceCoreHandlers_t core_handlers;
    Gateway_InterfaceTransportPorts_t transport_ports = {0};
    RouterTestTransport_t transport = {0};

    static const uint8_t vehicle[] = {0x01U};
    static const uint8_t session[] = {0x02U};

    const Gateway_MessageContextView_t context = make_context(
        vehicle, sizeof(vehicle),
        NULL, 0U,
        session, sizeof(session),
        NULL, 0U);

    Gateway_RelayMessageView_t message = {
        .type = GATEWAY_LOGICAL_MSG_STATE_QUERY,
        .direction = GATEWAY_DIRECTION_MOBILE_TO_DOMAIN,
        .context = context,
        .payload = { .data = NULL, .length = 0U }
    };

    Gateway_Interface_Reset();
    LinkStateManager_Reset();
    MessageContextManager_Reset();
    GatewayLifecycleManager_Reset();
    GatewayRouter_Reset();

    assert(GatewayRouter_Init() == GATEWAY_STATUS_OK);
    assert(
        GatewayRouter_BuildCoreHandlers(&core_handlers)
        == GATEWAY_STATUS_OK);

    transport_ports.send_to_domain = transport_send_to_domain;
    transport_ports.send_to_mobile = transport_send_to_mobile;
    transport_ports.user_context = &transport;

    assert(
        Gateway_Interface_Init(
            &core_handlers,
            &transport_ports)
        == GATEWAY_STATUS_OK);

    /*
     * LinkStateManager / MessageContextManager가 초기화되지 않았으므로
     * Router dependency check에서 NOT_READY.
     */
    assert(
        Gateway_Interface_OnMobileMessage(&message)
        == GATEWAY_STATUS_NOT_READY);
}


static void test_control_request_blocked_while_syncing(void)
{
    static const uint8_t vehicle[] = {0x01U};
    static const uint8_t session[] = {0x02U};
    static const uint8_t request[] = {0x03U};

    const Gateway_MessageContextView_t context = make_context(
        vehicle, sizeof(vehicle),
        NULL, 0U,
        session, sizeof(session),
        request, sizeof(request));

    Gateway_RelayMessageView_t message = {
        .type = GATEWAY_LOGICAL_MSG_MOBILE_REQUEST,
        .direction = GATEWAY_DIRECTION_MOBILE_TO_DOMAIN,
        .context = context,
        .payload = { .data = NULL, .length = 0U }
    };

    RouterTestTransport_t transport;
    Gateway_InterfaceCoreHandlers_t core_handlers;
    Gateway_InterfaceTransportPorts_t transport_ports = {
        .send_to_domain = transport_send_to_domain,
        .send_to_mobile = transport_send_to_mobile,
        .user_context = &transport
    };

    Gateway_Interface_Reset();
    LinkStateManager_Reset();
    MessageContextManager_Reset();
    GatewayLifecycleManager_Reset();
    GatewayRouter_Reset();
    memset(&transport, 0, sizeof(transport));

    assert(LinkStateManager_Init() == GATEWAY_STATUS_OK);
    assert(MessageContextManager_Init() == GATEWAY_STATUS_OK);
    assert(GatewayLifecycleManager_Init() == GATEWAY_STATUS_OK);
    assert(GatewayRouter_Init() == GATEWAY_STATUS_OK);

    assert(
        MessageContextManager_ActivateSessionAt(
            &context, 100U)
        == GATEWAY_STATUS_OK);

    assert(
        LinkStateManager_MarkAvailableAt(
            GATEWAY_LINK_BLUETOOTH, 100U)
        == GATEWAY_STATUS_OK);

    assert(
        LinkStateManager_MarkAvailableAt(
            GATEWAY_LINK_DOMAIN_UART, 100U)
        == GATEWAY_STATUS_OK);

    assert(
        GatewayLifecycleManager_StartAt(100U)
        == GATEWAY_STATUS_OK);

    assert(
        GatewayLifecycleManager_GetState()
        == GATEWAY_LIFECYCLE_SYNCING);

    assert(
        GatewayRouter_BuildCoreHandlers(&core_handlers)
        == GATEWAY_STATUS_OK);

    assert(
        Gateway_Interface_Init(
            &core_handlers,
            &transport_ports)
        == GATEWAY_STATUS_OK);

    assert(
        Gateway_Interface_OnMobileMessage(&message)
        == GATEWAY_STATUS_NOT_READY);

    assert(transport.send_to_domain_calls == 0);
}

static void test_state_query_allowed_while_syncing(void)
{
    static const uint8_t vehicle[] = {0x01U};
    static const uint8_t session[] = {0x02U};

    const Gateway_MessageContextView_t context = make_context(
        vehicle, sizeof(vehicle),
        NULL, 0U,
        session, sizeof(session),
        NULL, 0U);

    Gateway_RelayMessageView_t message = {
        .type = GATEWAY_LOGICAL_MSG_STATE_QUERY,
        .direction = GATEWAY_DIRECTION_MOBILE_TO_DOMAIN,
        .context = context,
        .payload = { .data = NULL, .length = 0U }
    };

    RouterTestTransport_t transport;
    Gateway_InterfaceCoreHandlers_t core_handlers;
    Gateway_InterfaceTransportPorts_t transport_ports = {
        .send_to_domain = transport_send_to_domain,
        .send_to_mobile = transport_send_to_mobile,
        .user_context = &transport
    };

    Gateway_Interface_Reset();
    LinkStateManager_Reset();
    MessageContextManager_Reset();
    GatewayLifecycleManager_Reset();
    GatewayRouter_Reset();
    memset(&transport, 0, sizeof(transport));

    assert(LinkStateManager_Init() == GATEWAY_STATUS_OK);
    assert(MessageContextManager_Init() == GATEWAY_STATUS_OK);
    assert(GatewayLifecycleManager_Init() == GATEWAY_STATUS_OK);
    assert(GatewayRouter_Init() == GATEWAY_STATUS_OK);

    assert(
        MessageContextManager_ActivateSessionAt(
            &context, 100U)
        == GATEWAY_STATUS_OK);

    assert(
        LinkStateManager_MarkAvailableAt(
            GATEWAY_LINK_BLUETOOTH, 100U)
        == GATEWAY_STATUS_OK);

    assert(
        LinkStateManager_MarkAvailableAt(
            GATEWAY_LINK_DOMAIN_UART, 100U)
        == GATEWAY_STATUS_OK);

    assert(
        GatewayLifecycleManager_StartAt(100U)
        == GATEWAY_STATUS_OK);

    assert(
        GatewayRouter_BuildCoreHandlers(&core_handlers)
        == GATEWAY_STATUS_OK);

    assert(
        Gateway_Interface_Init(
            &core_handlers,
            &transport_ports)
        == GATEWAY_STATUS_OK);

    assert(
        Gateway_Interface_OnMobileMessage(&message)
        == GATEWAY_STATUS_OK);

    assert(transport.send_to_domain_calls == 1);
}

int main(void)
{
    test_mobile_to_domain_forward();
    test_domain_to_mobile_forward();
    test_uart_unavailable_does_not_queue_or_send();
    test_bluetooth_unavailable_does_not_send();
    test_old_session_message_is_rejected();
    test_payload_is_not_modified();
    test_invalid_direction_is_rejected();
    test_router_snapshot();
    test_router_dependency_not_ready();
    test_control_request_blocked_while_syncing();
    test_state_query_allowed_while_syncing();

    return 0;
}
