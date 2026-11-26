#include "Gateway_Interface.h"

#include <string.h>

static Gateway_InterfaceCoreHandlers_t s_core_handlers;
static Gateway_InterfaceTransportPorts_t s_transport_ports;
static bool s_initialized = false;


/* -------------------------------------------------------------------------- */
/* Internal helpers                                                           */
/* -------------------------------------------------------------------------- */

static bool Gateway_Interface_IsMobileToDomainType(
    Gateway_LogicalMessageType_t type)
{
    switch (type)
    {
        case GATEWAY_LOGICAL_MSG_MOBILE_REQUEST:
        case GATEWAY_LOGICAL_MSG_STATE_QUERY:
        case GATEWAY_LOGICAL_MSG_WARNING_ACK:
            return true;

        default:
            return false;
    }
}

static bool Gateway_Interface_IsDomainToMobileType(
    Gateway_LogicalMessageType_t type)
{
    switch (type)
    {
        case GATEWAY_LOGICAL_MSG_REQUEST_RESULT:
        case GATEWAY_LOGICAL_MSG_VEHICLE_STATE:
        case GATEWAY_LOGICAL_MSG_WARNING:
        case GATEWAY_LOGICAL_MSG_FUNCTION_AVAILABILITY:
        case GATEWAY_LOGICAL_MSG_DIGITAL_KEY_STATE_RESULT:
            return true;

        default:
            return false;
    }
}

static bool Gateway_Interface_IsValidUpdateBasis(
    const Gateway_UpdateBasis_t *update)
{
    if (update == NULL)
    {
        return false;
    }

    switch (update->quality)
    {
        case GATEWAY_DATA_QUALITY_UNKNOWN:
        case GATEWAY_DATA_QUALITY_VALID:
        case GATEWAY_DATA_QUALITY_INVALID:
        case GATEWAY_DATA_QUALITY_NO_DATA:
        case GATEWAY_DATA_QUALITY_STALE:
            return true;

        default:
            return false;
    }
}

static bool Gateway_Interface_IsValidRegistrationState(
    Gateway_RegistrationState_t state)
{
    switch (state)
    {
        case GATEWAY_REGISTRATION_UNKNOWN:
        case GATEWAY_NOT_REGISTERED:
        case GATEWAY_REGISTERED:
            return true;

        default:
            return false;
    }
}

static bool Gateway_Interface_IsValidConnectionState(
    Gateway_ConnectionState_t state)
{
    switch (state)
    {
        case GATEWAY_CONNECTION_UNKNOWN:
        case GATEWAY_DISCONNECTED:
        case GATEWAY_CONNECTED:
            return true;

        default:
            return false;
    }
}

static bool Gateway_Interface_IsValidAppActivityState(
    Gateway_AppActivityState_t state)
{
    switch (state)
    {
        case GATEWAY_APP_ACTIVITY_UNKNOWN:
        case GATEWAY_APP_INACTIVE:
        case GATEWAY_APP_ACTIVE:
            return true;

        default:
            return false;
    }
}

static bool Gateway_Interface_IsValidProximityState(
    Gateway_ProximityState_t state)
{
    switch (state)
    {
        case GATEWAY_PROXIMITY_UNKNOWN:
        case GATEWAY_PROXIMITY_FAR:
        case GATEWAY_PROXIMITY_NEAR:
            return true;

        default:
            return false;
    }
}


/* -------------------------------------------------------------------------- */
/* Lifecycle                                                                  */
/* -------------------------------------------------------------------------- */

Gateway_Status_t Gateway_Interface_Init(
    const Gateway_InterfaceCoreHandlers_t *core_handlers,
    const Gateway_InterfaceTransportPorts_t *transport_ports)
{
    if ((core_handlers == NULL) || (transport_ports == NULL))
    {
        return GATEWAY_STATUS_INVALID_ARGUMENT;
    }

    s_core_handlers = *core_handlers;
    s_transport_ports = *transport_ports;
    s_initialized = true;

    return GATEWAY_STATUS_OK;
}

void Gateway_Interface_Reset(void)
{
    memset(&s_core_handlers, 0, sizeof(s_core_handlers));
    memset(&s_transport_ports, 0, sizeof(s_transport_ports));
    s_initialized = false;
}

bool Gateway_Interface_IsInitialized(void)
{
    return s_initialized;
}


/* -------------------------------------------------------------------------- */
/* Validation                                                                 */
/* -------------------------------------------------------------------------- */

bool Gateway_Interface_IsValidByteView(
    const Gateway_ByteView_t *view)
{
    if (view == NULL)
    {
        return false;
    }

    if (view->length == 0U)
    {
        return view->data == NULL;
    }

    return view->data != NULL;
}

bool Gateway_Interface_IsValidMessageContext(
    const Gateway_MessageContextView_t *context)
{
    if (context == NULL)
    {
        return false;
    }

    return Gateway_Interface_IsValidByteView(&context->vehicle_id)
        && Gateway_Interface_IsValidByteView(&context->device_context_id)
        && Gateway_Interface_IsValidByteView(&context->session_id)
        && Gateway_Interface_IsValidByteView(&context->request_id);
}

bool Gateway_Interface_IsValidRelayMessage(
    const Gateway_RelayMessageView_t *message)
{
    if (message == NULL)
    {
        return false;
    }

    if (!Gateway_Interface_IsValidMessageContext(&message->context))
    {
        return false;
    }

    if (!Gateway_Interface_IsValidByteView(&message->payload))
    {
        return false;
    }

    switch (message->direction)
    {
        case GATEWAY_DIRECTION_MOBILE_TO_DOMAIN:
            return Gateway_Interface_IsMobileToDomainType(message->type);

        case GATEWAY_DIRECTION_DOMAIN_TO_MOBILE:
            return Gateway_Interface_IsDomainToMobileType(message->type);

        default:
            /*
             * ESP32 -> Domain의 Registration / App Activity / Proximity는
             * 별도의 typed Publish API를 사용한다.
             */
            return false;
    }
}


/* -------------------------------------------------------------------------- */
/* Adapter -> Core                                                            */
/* -------------------------------------------------------------------------- */

Gateway_Status_t Gateway_Interface_OnMobileMessage(
    const Gateway_RelayMessageView_t *message)
{
    if (!s_initialized)
    {
        return GATEWAY_STATUS_NOT_READY;
    }

    if (!Gateway_Interface_IsValidRelayMessage(message)
        || (message->direction != GATEWAY_DIRECTION_MOBILE_TO_DOMAIN))
    {
        return GATEWAY_STATUS_INVALID_ARGUMENT;
    }

    if (s_core_handlers.on_mobile_message == NULL)
    {
        return GATEWAY_STATUS_UNSUPPORTED;
    }

    return s_core_handlers.on_mobile_message(
        message,
        s_core_handlers.user_context);
}

Gateway_Status_t Gateway_Interface_OnDomainMessage(
    const Gateway_RelayMessageView_t *message)
{
    if (!s_initialized)
    {
        return GATEWAY_STATUS_NOT_READY;
    }

    if (!Gateway_Interface_IsValidRelayMessage(message)
        || (message->direction != GATEWAY_DIRECTION_DOMAIN_TO_MOBILE))
    {
        return GATEWAY_STATUS_INVALID_ARGUMENT;
    }

    if (s_core_handlers.on_domain_message == NULL)
    {
        return GATEWAY_STATUS_UNSUPPORTED;
    }

    return s_core_handlers.on_domain_message(
        message,
        s_core_handlers.user_context);
}


/* -------------------------------------------------------------------------- */
/* Core -> Transport                                                          */
/* -------------------------------------------------------------------------- */

Gateway_Status_t Gateway_Interface_SendToDomain(
    const Gateway_RelayMessageView_t *message)
{
    if (!s_initialized)
    {
        return GATEWAY_STATUS_NOT_READY;
    }

    if (!Gateway_Interface_IsValidRelayMessage(message)
        || (message->direction != GATEWAY_DIRECTION_MOBILE_TO_DOMAIN))
    {
        return GATEWAY_STATUS_INVALID_ARGUMENT;
    }

    if (s_transport_ports.send_to_domain == NULL)
    {
        return GATEWAY_STATUS_UNSUPPORTED;
    }

    return s_transport_ports.send_to_domain(
        message,
        s_transport_ports.user_context);
}

Gateway_Status_t Gateway_Interface_SendToMobile(
    const Gateway_RelayMessageView_t *message)
{
    if (!s_initialized)
    {
        return GATEWAY_STATUS_NOT_READY;
    }

    if (!Gateway_Interface_IsValidRelayMessage(message)
        || (message->direction != GATEWAY_DIRECTION_DOMAIN_TO_MOBILE))
    {
        return GATEWAY_STATUS_INVALID_ARGUMENT;
    }

    if (s_transport_ports.send_to_mobile == NULL)
    {
        return GATEWAY_STATUS_UNSUPPORTED;
    }

    return s_transport_ports.send_to_mobile(
        message,
        s_transport_ports.user_context);
}


/* -------------------------------------------------------------------------- */
/* ESP32-produced updates -> Domain                                           */
/* -------------------------------------------------------------------------- */

Gateway_Status_t Gateway_Interface_PublishRegistrationConnection(
    const Gateway_RegistrationConnectionUpdate_t *update)
{
    if (!s_initialized)
    {
        return GATEWAY_STATUS_NOT_READY;
    }

    if ((update == NULL)
        || !Gateway_Interface_IsValidMessageContext(&update->context)
        || !Gateway_Interface_IsValidRegistrationState(update->registration)
        || !Gateway_Interface_IsValidConnectionState(update->connection)
        || !Gateway_Interface_IsValidUpdateBasis(&update->update))
    {
        return GATEWAY_STATUS_INVALID_ARGUMENT;
    }

    if (s_transport_ports.send_registration_to_domain == NULL)
    {
        return GATEWAY_STATUS_UNSUPPORTED;
    }

    return s_transport_ports.send_registration_to_domain(
        update,
        s_transport_ports.user_context);
}

Gateway_Status_t Gateway_Interface_PublishAppActivity(
    const Gateway_AppActivityUpdate_t *update)
{
    if (!s_initialized)
    {
        return GATEWAY_STATUS_NOT_READY;
    }

    if ((update == NULL)
        || !Gateway_Interface_IsValidMessageContext(&update->context)
        || !Gateway_Interface_IsValidAppActivityState(update->state)
        || !Gateway_Interface_IsValidUpdateBasis(&update->update))
    {
        return GATEWAY_STATUS_INVALID_ARGUMENT;
    }

    if (s_transport_ports.send_app_activity_to_domain == NULL)
    {
        return GATEWAY_STATUS_UNSUPPORTED;
    }

    return s_transport_ports.send_app_activity_to_domain(
        update,
        s_transport_ports.user_context);
}

Gateway_Status_t Gateway_Interface_PublishProximity(
    const Gateway_ProximityUpdate_t *update)
{
    if (!s_initialized)
    {
        return GATEWAY_STATUS_NOT_READY;
    }

    if ((update == NULL)
        || !Gateway_Interface_IsValidMessageContext(&update->context)
        || !Gateway_Interface_IsValidProximityState(update->proximity.state)
        || !Gateway_Interface_IsValidUpdateBasis(&update->proximity.update))
    {
        return GATEWAY_STATUS_INVALID_ARGUMENT;
    }

    if (s_transport_ports.send_proximity_to_domain == NULL)
    {
        return GATEWAY_STATUS_UNSUPPORTED;
    }

    return s_transport_ports.send_proximity_to_domain(
        update,
        s_transport_ports.user_context);
}
