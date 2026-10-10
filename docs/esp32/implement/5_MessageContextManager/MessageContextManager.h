#ifndef MESSAGE_CONTEXT_MANAGER_H
#define MESSAGE_CONTEXT_MANAGER_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "Gateway_Interface.h"
#include "Gateway_Time.h"
#include "Gateway_Types.h"

/*
 * MessageContextManager.h
 *
 * ESP32 Wireless Gateway에서 Vehicle / Device / Session / Request 식별의
 * 보존, 복사, 비교, 현재 Session Context 판정을 담당한다.
 *
 * 책임:
 * - Gateway_MessageContextView_t의 ID를 소유 가능한 Storage로 복사
 * - 현재 Vehicle / Device / Session Context 관리
 * - Session Context 변경/무효화 시 내부 generation 변경
 * - 과거 generation에서 캡처된 메시지를 현재 Context와 구분
 * - 동일 Request 식별인지 "비교"할 수 있는 도구 제공
 *
 * 하지 않는 것:
 * - Request duplicate를 차량 수준에서 수용/거부 판단
 * - Request Lifecycle 관리
 * - ACCEPTED / DONE / FAILED 등 Result 판단
 * - ID의 실제 Wire 폭/형식 결정
 * - 재연결 후 과거 Request 자동 Replay
 */

/*
 * 구현용 저장 버퍼 최대 크기.
 *
 * 중요:
 * - 실제 Wire ID 폭을 64 byte로 확정한다는 의미가 아니다.
 * - 동적 할당 없이 Context를 안전하게 소유하기 위한 DESIGN 상한이다.
 * - 실제 Interface Protocol 확정 후 build define로 조정 가능하다.
 */
#ifndef MESSAGE_CONTEXT_MANAGER_MAX_ID_BYTES
#define MESSAGE_CONTEXT_MANAGER_MAX_ID_BYTES (64U)
#endif

typedef struct
{
    uint8_t data[MESSAGE_CONTEXT_MANAGER_MAX_ID_BYTES];
    size_t length;
} MessageContextManager_IdStorage_t;

typedef struct
{
    MessageContextManager_IdStorage_t vehicle_id;
    MessageContextManager_IdStorage_t device_context_id;
    MessageContextManager_IdStorage_t session_id;
    MessageContextManager_IdStorage_t request_id;

    uint32_t generation;
    Gateway_TimeMs_t captured_at_ms;
} MessageContextManager_OwnedContext_t;

typedef struct
{
    bool active;

    MessageContextManager_IdStorage_t vehicle_id;
    MessageContextManager_IdStorage_t device_context_id;
    MessageContextManager_IdStorage_t session_id;

    uint32_t generation;
    Gateway_TimeMs_t activated_at_ms;
} MessageContextManager_ActiveSessionSnapshot_t;


/* -------------------------------------------------------------------------- */
/* Lifecycle                                                                  */
/* -------------------------------------------------------------------------- */

Gateway_Status_t MessageContextManager_Init(void);

void MessageContextManager_Reset(void);

bool MessageContextManager_IsInitialized(void);


/* -------------------------------------------------------------------------- */
/* Active Vehicle / Device / Session Context                                  */
/* -------------------------------------------------------------------------- */

/*
 * 현재 활성 Session Context를 설정한다.
 *
 * 최소 요구:
 * - vehicle_id 존재
 * - session_id 존재
 *
 * device_context_id는 현재 상세 Interface 계약이 확정되지 않아 optional이다.
 * request_id는 Session Context에 포함하지 않는다.
 *
 * 기존 활성 Context와 Vehicle/Device/Session이 완전히 같으면 generation을
 * 변경하지 않는다.
 *
 * 다른 Context이면 새 generation으로 전환한다.
 */
Gateway_Status_t MessageContextManager_ActivateSessionAt(
    const Gateway_MessageContextView_t *context,
    Gateway_TimeMs_t now_ms);

Gateway_Status_t MessageContextManager_ActivateSession(
    const Gateway_MessageContextView_t *context);

/*
 * 현재 Session을 무효화한다.
 *
 * Bluetooth disconnect / registration invalidation / restart-resync 등에서
 * 상위 Lifecycle이 호출할 수 있다.
 *
 * 이 호출은 generation을 변경하여 이전 Captured Context가 현재 Context로
 * 다시 인정되지 않도록 한다.
 */
Gateway_Status_t MessageContextManager_InvalidateSessionAt(
    Gateway_TimeMs_t now_ms);

Gateway_Status_t MessageContextManager_InvalidateSession(void);

bool MessageContextManager_HasActiveSession(void);

uint32_t MessageContextManager_GetCurrentGeneration(void);

Gateway_Status_t MessageContextManager_GetActiveSessionSnapshot(
    MessageContextManager_ActiveSessionSnapshot_t *snapshot);


/* -------------------------------------------------------------------------- */
/* Context copy / view                                                        */
/* -------------------------------------------------------------------------- */

/*
 * 입력 View를 호출자 소유 OwnedContext로 복사한다.
 *
 * 현재 활성 Session과 Vehicle/Device/Session이 일치해야 한다.
 * 일치하지 않거나 활성 Session이 없으면 GATEWAY_STATUS_NOT_READY를 반환한다.
 *
 * Request ID는 존재하면 그대로 복사한다.
 */
Gateway_Status_t MessageContextManager_CaptureForCurrentSessionAt(
    const Gateway_MessageContextView_t *context,
    Gateway_TimeMs_t now_ms,
    MessageContextManager_OwnedContext_t *owned_context);

Gateway_Status_t MessageContextManager_CaptureForCurrentSession(
    const Gateway_MessageContextView_t *context,
    MessageContextManager_OwnedContext_t *owned_context);

/*
 * OwnedContext가 소유한 내부 Storage를 가리키는 View를 생성한다.
 *
 * 반환 View의 수명은 owned_context 수명보다 길 수 없다.
 */
Gateway_Status_t MessageContextManager_MakeView(
    const MessageContextManager_OwnedContext_t *owned_context,
    Gateway_MessageContextView_t *view);


/* -------------------------------------------------------------------------- */
/* Comparison / current-session checks                                        */
/* -------------------------------------------------------------------------- */

bool MessageContextManager_IsViewCurrentSession(
    const Gateway_MessageContextView_t *context);

bool MessageContextManager_IsOwnedContextCurrent(
    const MessageContextManager_OwnedContext_t *owned_context);

/*
 * 두 Context가 같은 Request 식별을 나타내는지 비교한다.
 *
 * 조건:
 * - 둘 다 request_id가 존재해야 함
 * - Vehicle / Device / Session / Request ID가 모두 동일해야 함
 *
 * 이 함수는 "동일 식별인지"만 비교한다.
 * Duplicate 수용/거부 정책을 판단하지 않는다.
 */
bool MessageContextManager_IsSameRequestIdentity(
    const MessageContextManager_OwnedContext_t *left,
    const MessageContextManager_OwnedContext_t *right);


/* -------------------------------------------------------------------------- */
/* Validation helpers                                                         */
/* -------------------------------------------------------------------------- */

bool MessageContextManager_IsIdPresent(
    const MessageContextManager_IdStorage_t *id);

bool MessageContextManager_IsIdEqual(
    const MessageContextManager_IdStorage_t *left,
    const MessageContextManager_IdStorage_t *right);

#endif /* MESSAGE_CONTEXT_MANAGER_H */
