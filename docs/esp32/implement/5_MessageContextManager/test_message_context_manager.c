#include <assert.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "MessageContextManager.h"

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

static void test_initial_state(void)
{
    MessageContextManager_ActiveSessionSnapshot_t snapshot;

    MessageContextManager_Reset();

    assert(!MessageContextManager_IsInitialized());

    assert(
        MessageContextManager_GetActiveSessionSnapshot(&snapshot)
        == GATEWAY_STATUS_NOT_READY);

    assert(MessageContextManager_Init() == GATEWAY_STATUS_OK);
    assert(MessageContextManager_IsInitialized());
    assert(!MessageContextManager_HasActiveSession());
    assert(MessageContextManager_GetCurrentGeneration() != 0U);
}

static void test_activate_copies_input_storage(void)
{
    uint8_t vehicle[] = {0x01U, 0x02U};
    uint8_t device[] = {0x10U};
    uint8_t session[] = {0x20U, 0x21U};

    const Gateway_MessageContextView_t context = make_context(
        vehicle, sizeof(vehicle),
        device, sizeof(device),
        session, sizeof(session),
        NULL, 0U);

    MessageContextManager_ActiveSessionSnapshot_t snapshot;

    assert(
        MessageContextManager_ActivateSessionAt(
            &context,
            100U)
        == GATEWAY_STATUS_OK);

    assert(MessageContextManager_HasActiveSession());

    /*
     * 원본 Buffer를 바꿔도 Manager 내부 복사본은 변하면 안 된다.
     */
    vehicle[0] = 0xFFU;
    device[0] = 0xEEU;
    session[0] = 0xDDU;

    assert(
        MessageContextManager_GetActiveSessionSnapshot(&snapshot)
        == GATEWAY_STATUS_OK);

    assert(snapshot.active);
    assert(snapshot.vehicle_id.length == 2U);
    assert(snapshot.vehicle_id.data[0] == 0x01U);
    assert(snapshot.device_context_id.data[0] == 0x10U);
    assert(snapshot.session_id.data[0] == 0x20U);
    assert(snapshot.activated_at_ms == 100U);
}

static void test_same_session_does_not_change_generation(void)
{
    static const uint8_t vehicle[] = {0x01U};
    static const uint8_t session[] = {0x02U};

    const Gateway_MessageContextView_t context = make_context(
        vehicle, sizeof(vehicle),
        NULL, 0U,
        session, sizeof(session),
        NULL, 0U);

    uint32_t before;
    uint32_t after;

    MessageContextManager_Reset();
    assert(MessageContextManager_Init() == GATEWAY_STATUS_OK);

    assert(
        MessageContextManager_ActivateSessionAt(
            &context,
            100U)
        == GATEWAY_STATUS_OK);

    before = MessageContextManager_GetCurrentGeneration();

    assert(
        MessageContextManager_ActivateSessionAt(
            &context,
            200U)
        == GATEWAY_STATUS_OK);

    after = MessageContextManager_GetCurrentGeneration();

    assert(before == after);
}

static void test_capture_owns_request_context(void)
{
    uint8_t vehicle[] = {0x01U};
    uint8_t session[] = {0x02U};
    uint8_t request[] = {0xA0U, 0xA1U, 0xA2U};

    const Gateway_MessageContextView_t context = make_context(
        vehicle, sizeof(vehicle),
        NULL, 0U,
        session, sizeof(session),
        request, sizeof(request));

    MessageContextManager_OwnedContext_t owned;
    Gateway_MessageContextView_t owned_view;

    MessageContextManager_Reset();
    assert(MessageContextManager_Init() == GATEWAY_STATUS_OK);

    assert(
        MessageContextManager_ActivateSessionAt(
            &context,
            100U)
        == GATEWAY_STATUS_OK);

    assert(
        MessageContextManager_CaptureForCurrentSessionAt(
            &context,
            120U,
            &owned)
        == GATEWAY_STATUS_OK);

    request[0] = 0xFFU;

    assert(owned.request_id.data[0] == 0xA0U);
    assert(owned.captured_at_ms == 120U);
    assert(
        owned.generation
        == MessageContextManager_GetCurrentGeneration());

    assert(
        MessageContextManager_MakeView(
            &owned,
            &owned_view)
        == GATEWAY_STATUS_OK);

    assert(owned_view.request_id.length == 3U);
    assert(owned_view.request_id.data[0] == 0xA0U);
}

static void test_session_change_invalidates_old_context(void)
{
    static const uint8_t vehicle[] = {0x01U};
    static const uint8_t session_a[] = {0x10U};
    static const uint8_t session_b[] = {0x20U};
    static const uint8_t request[] = {0x30U};

    const Gateway_MessageContextView_t context_a = make_context(
        vehicle, sizeof(vehicle),
        NULL, 0U,
        session_a, sizeof(session_a),
        request, sizeof(request));

    const Gateway_MessageContextView_t context_b = make_context(
        vehicle, sizeof(vehicle),
        NULL, 0U,
        session_b, sizeof(session_b),
        NULL, 0U);

    MessageContextManager_OwnedContext_t old_context;
    uint32_t generation_a;
    uint32_t generation_b;

    MessageContextManager_Reset();
    assert(MessageContextManager_Init() == GATEWAY_STATUS_OK);

    assert(
        MessageContextManager_ActivateSessionAt(
            &context_a,
            100U)
        == GATEWAY_STATUS_OK);

    generation_a = MessageContextManager_GetCurrentGeneration();

    assert(
        MessageContextManager_CaptureForCurrentSessionAt(
            &context_a,
            110U,
            &old_context)
        == GATEWAY_STATUS_OK);

    assert(MessageContextManager_IsOwnedContextCurrent(&old_context));

    assert(
        MessageContextManager_ActivateSessionAt(
            &context_b,
            200U)
        == GATEWAY_STATUS_OK);

    generation_b = MessageContextManager_GetCurrentGeneration();

    assert(generation_b != generation_a);
    assert(!MessageContextManager_IsOwnedContextCurrent(&old_context));
    assert(!MessageContextManager_IsViewCurrentSession(&context_a));
    assert(MessageContextManager_IsViewCurrentSession(&context_b));
}

static void test_invalidate_session_invalidates_old_context(void)
{
    static const uint8_t vehicle[] = {0x01U};
    static const uint8_t session[] = {0x02U};
    static const uint8_t request[] = {0x03U};

    const Gateway_MessageContextView_t context = make_context(
        vehicle, sizeof(vehicle),
        NULL, 0U,
        session, sizeof(session),
        request, sizeof(request));

    MessageContextManager_OwnedContext_t owned;
    uint32_t before;
    uint32_t after;

    MessageContextManager_Reset();
    assert(MessageContextManager_Init() == GATEWAY_STATUS_OK);

    assert(
        MessageContextManager_ActivateSessionAt(
            &context,
            100U)
        == GATEWAY_STATUS_OK);

    assert(
        MessageContextManager_CaptureForCurrentSessionAt(
            &context,
            110U,
            &owned)
        == GATEWAY_STATUS_OK);

    before = MessageContextManager_GetCurrentGeneration();

    assert(
        MessageContextManager_InvalidateSessionAt(
            200U)
        == GATEWAY_STATUS_OK);

    after = MessageContextManager_GetCurrentGeneration();

    assert(after != before);
    assert(!MessageContextManager_HasActiveSession());
    assert(!MessageContextManager_IsOwnedContextCurrent(&owned));
}

static void test_capture_rejects_other_session(void)
{
    static const uint8_t vehicle[] = {0x01U};
    static const uint8_t session_a[] = {0x10U};
    static const uint8_t session_b[] = {0x20U};

    const Gateway_MessageContextView_t context_a = make_context(
        vehicle, sizeof(vehicle),
        NULL, 0U,
        session_a, sizeof(session_a),
        NULL, 0U);

    const Gateway_MessageContextView_t context_b = make_context(
        vehicle, sizeof(vehicle),
        NULL, 0U,
        session_b, sizeof(session_b),
        NULL, 0U);

    MessageContextManager_OwnedContext_t owned;

    MessageContextManager_Reset();
    assert(MessageContextManager_Init() == GATEWAY_STATUS_OK);

    assert(
        MessageContextManager_ActivateSessionAt(
            &context_a,
            100U)
        == GATEWAY_STATUS_OK);

    assert(
        MessageContextManager_CaptureForCurrentSessionAt(
            &context_b,
            120U,
            &owned)
        == GATEWAY_STATUS_NOT_READY);
}

static void test_same_request_identity(void)
{
    static const uint8_t vehicle[] = {0x01U};
    static const uint8_t session[] = {0x02U};
    static const uint8_t request_a[] = {0xA0U, 0xA1U};
    static const uint8_t request_b[] = {0xB0U, 0xB1U};

    const Gateway_MessageContextView_t context_a1 = make_context(
        vehicle, sizeof(vehicle),
        NULL, 0U,
        session, sizeof(session),
        request_a, sizeof(request_a));

    const Gateway_MessageContextView_t context_a2 = make_context(
        vehicle, sizeof(vehicle),
        NULL, 0U,
        session, sizeof(session),
        request_a, sizeof(request_a));

    const Gateway_MessageContextView_t context_b = make_context(
        vehicle, sizeof(vehicle),
        NULL, 0U,
        session, sizeof(session),
        request_b, sizeof(request_b));

    const Gateway_MessageContextView_t context_no_request = make_context(
        vehicle, sizeof(vehicle),
        NULL, 0U,
        session, sizeof(session),
        NULL, 0U);

    MessageContextManager_OwnedContext_t owned_a1;
    MessageContextManager_OwnedContext_t owned_a2;
    MessageContextManager_OwnedContext_t owned_b;
    MessageContextManager_OwnedContext_t owned_no_request;

    MessageContextManager_Reset();
    assert(MessageContextManager_Init() == GATEWAY_STATUS_OK);

    assert(
        MessageContextManager_ActivateSessionAt(
            &context_a1,
            100U)
        == GATEWAY_STATUS_OK);

    assert(
        MessageContextManager_CaptureForCurrentSessionAt(
            &context_a1, 110U, &owned_a1)
        == GATEWAY_STATUS_OK);

    assert(
        MessageContextManager_CaptureForCurrentSessionAt(
            &context_a2, 120U, &owned_a2)
        == GATEWAY_STATUS_OK);

    assert(
        MessageContextManager_CaptureForCurrentSessionAt(
            &context_b, 130U, &owned_b)
        == GATEWAY_STATUS_OK);

    assert(
        MessageContextManager_CaptureForCurrentSessionAt(
            &context_no_request, 140U, &owned_no_request)
        == GATEWAY_STATUS_OK);

    assert(
        MessageContextManager_IsSameRequestIdentity(
            &owned_a1,
            &owned_a2));

    assert(
        !MessageContextManager_IsSameRequestIdentity(
            &owned_a1,
            &owned_b));

    assert(
        !MessageContextManager_IsSameRequestIdentity(
            &owned_a1,
            &owned_no_request));
}


static void test_same_wire_request_id_across_generation_is_not_same_current_request(void)
{
    static const uint8_t vehicle[] = {0x01U};
    static const uint8_t session[] = {0x02U};
    static const uint8_t request[] = {0x03U};

    const Gateway_MessageContextView_t context = make_context(
        vehicle, sizeof(vehicle),
        NULL, 0U,
        session, sizeof(session),
        request, sizeof(request));

    MessageContextManager_OwnedContext_t old_request;
    MessageContextManager_OwnedContext_t new_request;

    MessageContextManager_Reset();
    assert(MessageContextManager_Init() == GATEWAY_STATUS_OK);

    assert(
        MessageContextManager_ActivateSessionAt(
            &context,
            100U)
        == GATEWAY_STATUS_OK);

    assert(
        MessageContextManager_CaptureForCurrentSessionAt(
            &context,
            110U,
            &old_request)
        == GATEWAY_STATUS_OK);

    assert(
        MessageContextManager_InvalidateSessionAt(
            200U)
        == GATEWAY_STATUS_OK);

    assert(
        MessageContextManager_ActivateSessionAt(
            &context,
            300U)
        == GATEWAY_STATUS_OK);

    assert(
        MessageContextManager_CaptureForCurrentSessionAt(
            &context,
            310U,
            &new_request)
        == GATEWAY_STATUS_OK);

    assert(old_request.generation != new_request.generation);

    assert(
        !MessageContextManager_IsSameRequestIdentity(
            &old_request,
            &new_request));
}

static void test_missing_required_activation_ids(void)
{
    static const uint8_t vehicle[] = {0x01U};
    static const uint8_t session[] = {0x02U};

    const Gateway_MessageContextView_t no_vehicle = make_context(
        NULL, 0U,
        NULL, 0U,
        session, sizeof(session),
        NULL, 0U);

    const Gateway_MessageContextView_t no_session = make_context(
        vehicle, sizeof(vehicle),
        NULL, 0U,
        NULL, 0U,
        NULL, 0U);

    MessageContextManager_Reset();
    assert(MessageContextManager_Init() == GATEWAY_STATUS_OK);

    assert(
        MessageContextManager_ActivateSessionAt(
            &no_vehicle,
            100U)
        == GATEWAY_STATUS_INVALID_ARGUMENT);

    assert(
        MessageContextManager_ActivateSessionAt(
            &no_session,
            100U)
        == GATEWAY_STATUS_INVALID_ARGUMENT);
}

static void test_capacity_limit_is_not_wire_contract(void)
{
    uint8_t too_long[MESSAGE_CONTEXT_MANAGER_MAX_ID_BYTES + 1U];
    static const uint8_t session[] = {0x01U};

    memset(too_long, 0xAB, sizeof(too_long));

    const Gateway_MessageContextView_t context = make_context(
        too_long, sizeof(too_long),
        NULL, 0U,
        session, sizeof(session),
        NULL, 0U);

    MessageContextManager_Reset();
    assert(MessageContextManager_Init() == GATEWAY_STATUS_OK);

    assert(
        MessageContextManager_ActivateSessionAt(
            &context,
            100U)
        == GATEWAY_STATUS_UNSUPPORTED);
}

static void test_reset_reinit_does_not_reuse_generation(void)
{
    static const uint8_t vehicle[] = {0x01U};
    static const uint8_t session[] = {0x02U};

    const Gateway_MessageContextView_t context = make_context(
        vehicle, sizeof(vehicle),
        NULL, 0U,
        session, sizeof(session),
        NULL, 0U);

    uint32_t old_generation;
    uint32_t new_generation;

    MessageContextManager_Reset();
    assert(MessageContextManager_Init() == GATEWAY_STATUS_OK);

    assert(
        MessageContextManager_ActivateSessionAt(
            &context,
            100U)
        == GATEWAY_STATUS_OK);

    old_generation = MessageContextManager_GetCurrentGeneration();

    MessageContextManager_Reset();
    assert(MessageContextManager_Init() == GATEWAY_STATUS_OK);

    new_generation = MessageContextManager_GetCurrentGeneration();

    assert(new_generation != 0U);
    assert(new_generation != old_generation);
}

int main(void)
{
    test_initial_state();
    test_activate_copies_input_storage();
    test_same_session_does_not_change_generation();
    test_capture_owns_request_context();
    test_session_change_invalidates_old_context();
    test_invalidate_session_invalidates_old_context();
    test_capture_rejects_other_session();
    test_same_request_identity();
    test_same_wire_request_id_across_generation_is_not_same_current_request();
    test_missing_required_activation_ids();
    test_capacity_limit_is_not_wire_contract();
    test_reset_reinit_does_not_reuse_generation();

    return 0;
}
