#include "MessageContextManager.h"

#include <string.h>

static MessageContextManager_ActiveSessionSnapshot_t s_active_session;
static bool s_initialized = false;

/*
 * 0은 invalid generation으로 예약한다.
 *
 * Reset 이후 재-Init이 같은 process에서 일어나는 경우에도 이전 generation을
 * 바로 재사용하지 않도록 counter 자체는 Reset에서 0으로 되돌리지 않는다.
 *
 * 실제 MCU cold reset에서는 RAM이 초기화되지만, 그 경우 이전 RAM message도
 * 함께 사라지는 것이 기본 전제다. Queue/driver residue는 Lifecycle/Adapter에서
 * 별도로 폐기해야 한다.
 */
static uint32_t s_generation_counter = 0U;


/* -------------------------------------------------------------------------- */
/* Internal helpers                                                           */
/* -------------------------------------------------------------------------- */

static uint32_t MessageContextManager_NextGeneration(void)
{
    ++s_generation_counter;

    if (s_generation_counter == 0U)
    {
        ++s_generation_counter;
    }

    return s_generation_counter;
}

static void MessageContextManager_ClearId(
    MessageContextManager_IdStorage_t *id)
{
    if (id == NULL)
    {
        return;
    }

    memset(id->data, 0, sizeof(id->data));
    id->length = 0U;
}

static Gateway_Status_t MessageContextManager_CopyViewToId(
    const Gateway_ByteView_t *view,
    MessageContextManager_IdStorage_t *id)
{
    if ((view == NULL) || (id == NULL))
    {
        return GATEWAY_STATUS_INVALID_ARGUMENT;
    }

    if (!Gateway_Interface_IsValidByteView(view))
    {
        return GATEWAY_STATUS_INVALID_ARGUMENT;
    }

    if (view->length > MESSAGE_CONTEXT_MANAGER_MAX_ID_BYTES)
    {
        return GATEWAY_STATUS_UNSUPPORTED;
    }

    MessageContextManager_ClearId(id);

    if (view->length > 0U)
    {
        memcpy(id->data, view->data, view->length);
        id->length = view->length;
    }

    return GATEWAY_STATUS_OK;
}

static bool MessageContextManager_IsIdEqualToView(
    const MessageContextManager_IdStorage_t *id,
    const Gateway_ByteView_t *view)
{
    if ((id == NULL) || (view == NULL))
    {
        return false;
    }

    if (!Gateway_Interface_IsValidByteView(view))
    {
        return false;
    }

    if (id->length != view->length)
    {
        return false;
    }

    if (id->length == 0U)
    {
        return true;
    }

    return memcmp(id->data, view->data, id->length) == 0;
}

static bool MessageContextManager_IsSessionViewEqualToActive(
    const Gateway_MessageContextView_t *context)
{
    if ((!s_initialized) || (!s_active_session.active) || (context == NULL))
    {
        return false;
    }

    if (!Gateway_Interface_IsValidMessageContext(context))
    {
        return false;
    }

    return MessageContextManager_IsIdEqualToView(
               &s_active_session.vehicle_id,
               &context->vehicle_id)
        && MessageContextManager_IsIdEqualToView(
               &s_active_session.device_context_id,
               &context->device_context_id)
        && MessageContextManager_IsIdEqualToView(
               &s_active_session.session_id,
               &context->session_id);
}

static bool MessageContextManager_IsActivationContextValid(
    const Gateway_MessageContextView_t *context)
{
    if ((context == NULL)
        || !Gateway_Interface_IsValidMessageContext(context))
    {
        return false;
    }

    /*
     * SysRS의 대상 차량·세션 식별 연계를 실제 Current Session으로
     * 취급하기 위한 최소 조건.
     *
     * DeviceContextId의 필수 여부는 아직 TBD다.
     */
    return (context->vehicle_id.length > 0U)
        && (context->session_id.length > 0U);
}

static Gateway_Status_t MessageContextManager_CopySessionPart(
    const Gateway_MessageContextView_t *context,
    MessageContextManager_ActiveSessionSnapshot_t *target)
{
    Gateway_Status_t status;

    status = MessageContextManager_CopyViewToId(
        &context->vehicle_id,
        &target->vehicle_id);
    if (status != GATEWAY_STATUS_OK)
    {
        return status;
    }

    status = MessageContextManager_CopyViewToId(
        &context->device_context_id,
        &target->device_context_id);
    if (status != GATEWAY_STATUS_OK)
    {
        return status;
    }

    status = MessageContextManager_CopyViewToId(
        &context->session_id,
        &target->session_id);
    if (status != GATEWAY_STATUS_OK)
    {
        return status;
    }

    return GATEWAY_STATUS_OK;
}

static Gateway_Status_t MessageContextManager_CopyFullContext(
    const Gateway_MessageContextView_t *context,
    MessageContextManager_OwnedContext_t *target)
{
    Gateway_Status_t status;

    status = MessageContextManager_CopyViewToId(
        &context->vehicle_id,
        &target->vehicle_id);
    if (status != GATEWAY_STATUS_OK)
    {
        return status;
    }

    status = MessageContextManager_CopyViewToId(
        &context->device_context_id,
        &target->device_context_id);
    if (status != GATEWAY_STATUS_OK)
    {
        return status;
    }

    status = MessageContextManager_CopyViewToId(
        &context->session_id,
        &target->session_id);
    if (status != GATEWAY_STATUS_OK)
    {
        return status;
    }

    status = MessageContextManager_CopyViewToId(
        &context->request_id,
        &target->request_id);
    if (status != GATEWAY_STATUS_OK)
    {
        return status;
    }

    return GATEWAY_STATUS_OK;
}


/* -------------------------------------------------------------------------- */
/* Lifecycle                                                                  */
/* -------------------------------------------------------------------------- */

Gateway_Status_t MessageContextManager_Init(void)
{
    memset(&s_active_session, 0, sizeof(s_active_session));

    s_active_session.active = false;
    s_active_session.generation = MessageContextManager_NextGeneration();
    s_active_session.activated_at_ms = Gateway_Time_GetMs();

    s_initialized = true;

    return GATEWAY_STATUS_OK;
}

void MessageContextManager_Reset(void)
{
    memset(&s_active_session, 0, sizeof(s_active_session));

    /*
     * 같은 process 내 재초기화에서 이전 OwnedContext가 다시 current로
     * 보이지 않도록 generation을 한 번 소비한다.
     */
    (void)MessageContextManager_NextGeneration();

    s_initialized = false;
}

bool MessageContextManager_IsInitialized(void)
{
    return s_initialized;
}


/* -------------------------------------------------------------------------- */
/* Active Vehicle / Device / Session Context                                  */
/* -------------------------------------------------------------------------- */

Gateway_Status_t MessageContextManager_ActivateSessionAt(
    const Gateway_MessageContextView_t *context,
    Gateway_TimeMs_t now_ms)
{
    MessageContextManager_ActiveSessionSnapshot_t candidate;
    Gateway_Status_t status;

    if (!s_initialized)
    {
        return GATEWAY_STATUS_NOT_READY;
    }

    if (!MessageContextManager_IsActivationContextValid(context))
    {
        return GATEWAY_STATUS_INVALID_ARGUMENT;
    }

    /*
     * 길이 초과 등 복사 실패 시 기존 Active Session을 건드리지 않도록
     * 먼저 임시 candidate에 완성한다.
     */
    memset(&candidate, 0, sizeof(candidate));

    status = MessageContextManager_CopySessionPart(
        context,
        &candidate);
    if (status != GATEWAY_STATUS_OK)
    {
        return status;
    }

    if (MessageContextManager_IsSessionViewEqualToActive(context))
    {
        return GATEWAY_STATUS_OK;
    }

    candidate.active = true;
    candidate.generation = MessageContextManager_NextGeneration();
    candidate.activated_at_ms = now_ms;

    s_active_session = candidate;

    return GATEWAY_STATUS_OK;
}

Gateway_Status_t MessageContextManager_ActivateSession(
    const Gateway_MessageContextView_t *context)
{
    return MessageContextManager_ActivateSessionAt(
        context,
        Gateway_Time_GetMs());
}

Gateway_Status_t MessageContextManager_InvalidateSessionAt(
    Gateway_TimeMs_t now_ms)
{
    uint32_t new_generation;

    if (!s_initialized)
    {
        return GATEWAY_STATUS_NOT_READY;
    }

    new_generation = MessageContextManager_NextGeneration();

    memset(&s_active_session, 0, sizeof(s_active_session));

    s_active_session.active = false;
    s_active_session.generation = new_generation;
    s_active_session.activated_at_ms = now_ms;

    return GATEWAY_STATUS_OK;
}

Gateway_Status_t MessageContextManager_InvalidateSession(void)
{
    return MessageContextManager_InvalidateSessionAt(
        Gateway_Time_GetMs());
}

bool MessageContextManager_HasActiveSession(void)
{
    return s_initialized && s_active_session.active;
}

uint32_t MessageContextManager_GetCurrentGeneration(void)
{
    if (!s_initialized)
    {
        return 0U;
    }

    return s_active_session.generation;
}

Gateway_Status_t MessageContextManager_GetActiveSessionSnapshot(
    MessageContextManager_ActiveSessionSnapshot_t *snapshot)
{
    if (!s_initialized)
    {
        return GATEWAY_STATUS_NOT_READY;
    }

    if (snapshot == NULL)
    {
        return GATEWAY_STATUS_INVALID_ARGUMENT;
    }

    *snapshot = s_active_session;

    return GATEWAY_STATUS_OK;
}


/* -------------------------------------------------------------------------- */
/* Context copy / view                                                        */
/* -------------------------------------------------------------------------- */

Gateway_Status_t MessageContextManager_CaptureForCurrentSessionAt(
    const Gateway_MessageContextView_t *context,
    Gateway_TimeMs_t now_ms,
    MessageContextManager_OwnedContext_t *owned_context)
{
    MessageContextManager_OwnedContext_t candidate;
    Gateway_Status_t status;

    if (!s_initialized)
    {
        return GATEWAY_STATUS_NOT_READY;
    }

    if ((context == NULL) || (owned_context == NULL))
    {
        return GATEWAY_STATUS_INVALID_ARGUMENT;
    }

    if (!MessageContextManager_IsSessionViewEqualToActive(context))
    {
        return GATEWAY_STATUS_NOT_READY;
    }

    memset(&candidate, 0, sizeof(candidate));

    status = MessageContextManager_CopyFullContext(
        context,
        &candidate);
    if (status != GATEWAY_STATUS_OK)
    {
        return status;
    }

    candidate.generation = s_active_session.generation;
    candidate.captured_at_ms = now_ms;

    *owned_context = candidate;

    return GATEWAY_STATUS_OK;
}

Gateway_Status_t MessageContextManager_CaptureForCurrentSession(
    const Gateway_MessageContextView_t *context,
    MessageContextManager_OwnedContext_t *owned_context)
{
    return MessageContextManager_CaptureForCurrentSessionAt(
        context,
        Gateway_Time_GetMs(),
        owned_context);
}

Gateway_Status_t MessageContextManager_MakeView(
    const MessageContextManager_OwnedContext_t *owned_context,
    Gateway_MessageContextView_t *view)
{
    if ((owned_context == NULL) || (view == NULL))
    {
        return GATEWAY_STATUS_INVALID_ARGUMENT;
    }

    view->vehicle_id.data =
        (owned_context->vehicle_id.length > 0U)
            ? owned_context->vehicle_id.data
            : NULL;
    view->vehicle_id.length = owned_context->vehicle_id.length;

    view->device_context_id.data =
        (owned_context->device_context_id.length > 0U)
            ? owned_context->device_context_id.data
            : NULL;
    view->device_context_id.length =
        owned_context->device_context_id.length;

    view->session_id.data =
        (owned_context->session_id.length > 0U)
            ? owned_context->session_id.data
            : NULL;
    view->session_id.length = owned_context->session_id.length;

    view->request_id.data =
        (owned_context->request_id.length > 0U)
            ? owned_context->request_id.data
            : NULL;
    view->request_id.length = owned_context->request_id.length;

    return GATEWAY_STATUS_OK;
}


/* -------------------------------------------------------------------------- */
/* Comparison / current-session checks                                        */
/* -------------------------------------------------------------------------- */

bool MessageContextManager_IsViewCurrentSession(
    const Gateway_MessageContextView_t *context)
{
    return MessageContextManager_IsSessionViewEqualToActive(context);
}

bool MessageContextManager_IsOwnedContextCurrent(
    const MessageContextManager_OwnedContext_t *owned_context)
{
    if ((!s_initialized)
        || (!s_active_session.active)
        || (owned_context == NULL))
    {
        return false;
    }

    if (owned_context->generation != s_active_session.generation)
    {
        return false;
    }

    return MessageContextManager_IsIdEqual(
               &owned_context->vehicle_id,
               &s_active_session.vehicle_id)
        && MessageContextManager_IsIdEqual(
               &owned_context->device_context_id,
               &s_active_session.device_context_id)
        && MessageContextManager_IsIdEqual(
               &owned_context->session_id,
               &s_active_session.session_id);
}

bool MessageContextManager_IsSameRequestIdentity(
    const MessageContextManager_OwnedContext_t *left,
    const MessageContextManager_OwnedContext_t *right)
{
    if ((left == NULL) || (right == NULL))
    {
        return false;
    }

    /*
     * Request ID가 없으면 "같은 Request"라고 단정하지 않는다.
     *
     * 또한 동일한 Wire ID가 재연결 전후에 재사용되더라도 이전 실행 구간과
     * 현재 실행 구간을 같은 Gateway-local Request identity로 보지 않도록
     * generation도 동일해야 한다.
     */
    if (!MessageContextManager_IsIdPresent(&left->request_id)
        || !MessageContextManager_IsIdPresent(&right->request_id)
        || (left->generation == 0U)
        || (right->generation == 0U)
        || (left->generation != right->generation))
    {
        return false;
    }

    return MessageContextManager_IsIdEqual(
               &left->vehicle_id,
               &right->vehicle_id)
        && MessageContextManager_IsIdEqual(
               &left->device_context_id,
               &right->device_context_id)
        && MessageContextManager_IsIdEqual(
               &left->session_id,
               &right->session_id)
        && MessageContextManager_IsIdEqual(
               &left->request_id,
               &right->request_id);
}


/* -------------------------------------------------------------------------- */
/* Validation helpers                                                         */
/* -------------------------------------------------------------------------- */

bool MessageContextManager_IsIdPresent(
    const MessageContextManager_IdStorage_t *id)
{
    return (id != NULL) && (id->length > 0U);
}

bool MessageContextManager_IsIdEqual(
    const MessageContextManager_IdStorage_t *left,
    const MessageContextManager_IdStorage_t *right)
{
    if ((left == NULL) || (right == NULL))
    {
        return false;
    }

    if ((left->length > MESSAGE_CONTEXT_MANAGER_MAX_ID_BYTES)
        || (right->length > MESSAGE_CONTEXT_MANAGER_MAX_ID_BYTES))
    {
        return false;
    }

    if (left->length != right->length)
    {
        return false;
    }

    if (left->length == 0U)
    {
        return true;
    }

    return memcmp(
        left->data,
        right->data,
        left->length) == 0;
}
