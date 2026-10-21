#include <assert.h>
#include <stddef.h>
#include <stdint.h>

#include "Gateway_Interface.h"

typedef struct
{
    int mobile_handler_calls;
    int domain_handler_calls;
    int domain_send_calls;
    int mobile_send_calls;
    int registration_send_calls;
    int app_activity_send_calls;
    int proximity_send_calls;
} TestContext_t;

static Gateway_Status_t on_mobile(
    const Gateway_RelayMessageView_t *message,
    void *user_context)
{
    TestContext_t *ctx = (TestContext_t *)user_context;

    assert(message != NULL);
    assert(message->direction == GATEWAY_DIRECTION_MOBILE_TO_DOMAIN);

    ctx->mobile_handler_calls++;
    return GATEWAY_STATUS_OK;
}

static Gateway_Status_t on_domain(
    const Gateway_RelayMessageView_t *message,
    void *user_context)
{
    TestContext_t *ctx = (TestContext_t *)user_context;

    assert(message != NULL);
    assert(message->direction == GATEWAY_DIRECTION_DOMAIN_TO_MOBILE);

    ctx->domain_handler_calls++;
    return GATEWAY_STATUS_OK;
}

static Gateway_Status_t send_to_domain(
    const Gateway_RelayMessageView_t *message,
    void *user_context)
{
    TestContext_t *ctx = (TestContext_t *)user_context;

    assert(message != NULL);
    assert(message->direction == GATEWAY_DIRECTION_MOBILE_TO_DOMAIN);

    ctx->domain_send_calls++;
    return GATEWAY_STATUS_OK;
}

static Gateway_Status_t send_to_mobile(
    const Gateway_RelayMessageView_t *message,
    void *user_context)
{
    TestContext_t *ctx = (TestContext_t *)user_context;

    assert(message != NULL);
    assert(message->direction == GATEWAY_DIRECTION_DOMAIN_TO_MOBILE);

    ctx->mobile_send_calls++;
    return GATEWAY_STATUS_OK;
}

static Gateway_Status_t send_registration(
    const Gateway_RegistrationConnectionUpdate_t *update,
    void *user_context)
{
    TestContext_t *ctx = (TestContext_t *)user_context;

    assert(update != NULL);
    ctx->registration_send_calls++;
    return GATEWAY_STATUS_OK;
}

static Gateway_Status_t send_app_activity(
    const Gateway_AppActivityUpdate_t *update,
    void *user_context)
{
    TestContext_t *ctx = (TestContext_t *)user_context;

    assert(update != NULL);
    ctx->app_activity_send_calls++;
    return GATEWAY_STATUS_OK;
}

static Gateway_Status_t send_proximity(
    const Gateway_ProximityUpdate_t *update,
    void *user_context)
{
    TestContext_t *ctx = (TestContext_t *)user_context;

    assert(update != NULL);
    ctx->proximity_send_calls++;
    return GATEWAY_STATUS_OK;
}

static Gateway_MessageContextView_t empty_context(void)
{
    const Gateway_ByteView_t empty = { .data = NULL, .length = 0U };

    Gateway_MessageContextView_t context = {
        .vehicle_id = empty,
        .device_context_id = empty,
        .session_id = empty,
        .request_id = empty
    };

    return context;
}

static void test_not_initialized(void)
{
    Gateway_RelayMessageView_t message = {
        .type = GATEWAY_LOGICAL_MSG_MOBILE_REQUEST,
        .direction = GATEWAY_DIRECTION_MOBILE_TO_DOMAIN,
        .context = empty_context(),
        .payload = { .data = NULL, .length = 0U }
    };

    Gateway_Interface_Reset();

    assert(
        Gateway_Interface_OnMobileMessage(&message)
        == GATEWAY_STATUS_NOT_READY);
}

static void test_valid_routing(void)
{
    TestContext_t ctx = {0};

    const Gateway_InterfaceCoreHandlers_t core = {
        .on_mobile_message = on_mobile,
        .on_domain_message = on_domain,
        .user_context = &ctx
    };

    const Gateway_InterfaceTransportPorts_t transport = {
        .send_to_domain = send_to_domain,
        .send_to_mobile = send_to_mobile,
        .send_registration_to_domain = send_registration,
        .send_app_activity_to_domain = send_app_activity,
        .send_proximity_to_domain = send_proximity,
        .user_context = &ctx
    };

    assert(Gateway_Interface_Init(&core, &transport) == GATEWAY_STATUS_OK);
    assert(Gateway_Interface_IsInitialized());

    Gateway_RelayMessageView_t mobile_message = {
        .type = GATEWAY_LOGICAL_MSG_MOBILE_REQUEST,
        .direction = GATEWAY_DIRECTION_MOBILE_TO_DOMAIN,
        .context = empty_context(),
        .payload = { .data = NULL, .length = 0U }
    };

    Gateway_RelayMessageView_t domain_message = {
        .type = GATEWAY_LOGICAL_MSG_VEHICLE_STATE,
        .direction = GATEWAY_DIRECTION_DOMAIN_TO_MOBILE,
        .context = empty_context(),
        .payload = { .data = NULL, .length = 0U }
    };

    assert(
        Gateway_Interface_OnMobileMessage(&mobile_message)
        == GATEWAY_STATUS_OK);

    assert(
        Gateway_Interface_SendToDomain(&mobile_message)
        == GATEWAY_STATUS_OK);

    assert(
        Gateway_Interface_OnDomainMessage(&domain_message)
        == GATEWAY_STATUS_OK);

    assert(
        Gateway_Interface_SendToMobile(&domain_message)
        == GATEWAY_STATUS_OK);

    assert(ctx.mobile_handler_calls == 1);
    assert(ctx.domain_handler_calls == 1);
    assert(ctx.domain_send_calls == 1);
    assert(ctx.mobile_send_calls == 1);
}

static void test_invalid_direction_type_pair(void)
{
    Gateway_RelayMessageView_t invalid = {
        .type = GATEWAY_LOGICAL_MSG_WARNING,
        .direction = GATEWAY_DIRECTION_MOBILE_TO_DOMAIN,
        .context = empty_context(),
        .payload = { .data = NULL, .length = 0U }
    };

    assert(!Gateway_Interface_IsValidRelayMessage(&invalid));
    assert(
        Gateway_Interface_OnMobileMessage(&invalid)
        == GATEWAY_STATUS_INVALID_ARGUMENT);
}

static void test_invalid_byte_view(void)
{
    Gateway_RelayMessageView_t invalid = {
        .type = GATEWAY_LOGICAL_MSG_MOBILE_REQUEST,
        .direction = GATEWAY_DIRECTION_MOBILE_TO_DOMAIN,
        .context = empty_context(),
        .payload = { .data = NULL, .length = 1U }
    };

    assert(!Gateway_Interface_IsValidRelayMessage(&invalid));
}

static void test_typed_publish(void)
{
    Gateway_RegistrationConnectionUpdate_t registration = {
        .context = empty_context(),
        .registration = GATEWAY_REGISTERED,
        .connection = GATEWAY_CONNECTED,
        .update = {
            .quality = GATEWAY_DATA_QUALITY_VALID,
            .age_ms = 0U,
            .is_new_update = true
        }
    };

    Gateway_AppActivityUpdate_t app_activity = {
        .context = empty_context(),
        .state = GATEWAY_APP_ACTIVITY_UNKNOWN,
        .update = {
            .quality = GATEWAY_DATA_QUALITY_UNKNOWN,
            .age_ms = 0U,
            .is_new_update = false
        }
    };

    Gateway_ProximityUpdate_t proximity = {
        .context = empty_context(),
        .proximity = {
            .state = GATEWAY_PROXIMITY_UNKNOWN,
            .update = {
                .quality = GATEWAY_DATA_QUALITY_NO_DATA,
                .age_ms = 0U,
                .is_new_update = false
            }
        }
    };

    assert(
        Gateway_Interface_PublishRegistrationConnection(&registration)
        == GATEWAY_STATUS_OK);

    assert(
        Gateway_Interface_PublishAppActivity(&app_activity)
        == GATEWAY_STATUS_OK);

    assert(
        Gateway_Interface_PublishProximity(&proximity)
        == GATEWAY_STATUS_OK);
}


static void test_invalid_typed_state(void)
{
    Gateway_ProximityUpdate_t proximity = {
        .context = empty_context(),
        .proximity = {
            .state = (Gateway_ProximityState_t)99,
            .update = {
                .quality = GATEWAY_DATA_QUALITY_VALID,
                .age_ms = 0U,
                .is_new_update = true
            }
        }
    };

    assert(
        Gateway_Interface_PublishProximity(&proximity)
        == GATEWAY_STATUS_INVALID_ARGUMENT);
}

int main(void)
{
    test_not_initialized();
    test_valid_routing();
    test_invalid_direction_type_pair();
    test_invalid_byte_view();
    test_typed_publish();
    test_invalid_typed_state();

    return 0;
}
