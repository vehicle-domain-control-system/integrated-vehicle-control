/**
 * @file GatewaySessionManager.h
 * @brief ESP32가 제공하는 단말 등록/현재 연결/Session Context의 유효성 관리
 *
 * ============================================================================
 * 목적
 * ============================================================================
 *
 * GatewaySessionManager(GSM)는 ESP32가 제공하는:
 *
 *   - Device Registration
 *   - Current Bluetooth Connection
 *   - App Active
 *   - Session ID
 *   - Connection Quality/Freshness
 *
 * 를 바탕으로 "현재 MOBILE Request를 어떤 Session Context로 수용할 수 있는가"
 * 를 관리한다.
 *
 * 이 모듈은 Request ID 중복/결과 History를 관리하지 않는다.
 * 그 책임은 RequestManager에 있다.
 *
 * 이 모듈은 Bluetooth Pairing/Bonding을 직접 수행하지 않는다.
 * ESP32가 판정한 등록/현재 연결 결과를 사용한다.
 *
 * UART Frame CRC/Length/Sequence 검증은 UartAdapter/Communication 계층 책임이다.
 *
 * ============================================================================
 * 핵심 안전 원칙
 * ============================================================================
 *
 * - Registration/Connection/Quality 근거가 유효하지 않으면 새 Request 수용 금지
 * - 재연결/재시작 후 이전 Session의 지연 Packet을 새 Request로 수용 금지
 * - Session 유효성 상실은 과거 Request History 삭제를 의미하지 않음
 * - App Active는 일반 Session 유효성과 분리하여 보관
 *   (Digital Key 등의 Feature가 필요할 때 별도로 검사)
 *
 * SESSION_ID 생성 주체/폭/수명은 아직 Interface TBD이다.
 * 따라서 SESSION_ID == 0 같은 임의 Invalid 규칙은 사용하지 않는다.
 */

#ifndef GATEWAY_SESSION_MANAGER_H
#define GATEWAY_SESSION_MANAGER_H

#include <stdbool.h>
#include <stdint.h>

#include "Domain_Interface.h"
#include "Vehicle_Types.h"

#ifdef __cplusplus
extern "C" {
#endif

#ifndef GATEWAY_SESSION_MAX_DEVICES
#define GATEWAY_SESSION_MAX_DEVICES (4U)
#endif

#ifndef GATEWAY_SESSION_RETIRED_PER_DEVICE
#define GATEWAY_SESSION_RETIRED_PER_DEVICE (4U)
#endif

typedef enum
{
    GSM_STATUS_OK = 0,
    GSM_STATUS_INVALID_ARGUMENT,
    GSM_STATUS_NOT_INITIALIZED,
    GSM_STATUS_CAPACITY_FULL,
    GSM_STATUS_NOT_FOUND,
    GSM_STATUS_SESSION_REUSE_BLOCKED
} GatewaySessionStatus_t;

typedef enum
{
    GSM_SESSION_STATE_UNKNOWN = 0,
    GSM_SESSION_STATE_INACTIVE,
    GSM_SESSION_STATE_ACTIVE,
    GSM_SESSION_STATE_REJECTED_REUSE
} GatewaySessionState_t;

/**
 * @brief 같은 Session ID를 연결 복구 후 재사용할지에 대한 정책.
 *
 * STRICT:
 *   runtime 중 한 번 폐기(retired)된 Session ID를 다시 활성화하지 않는다.
 *   이전 실행 구간의 지연 Packet과 새 Packet을 구분할 별도 Generation 정보가
 *   아직 없으므로 현재 권장값이다.
 *
 * ALLOW:
 *   동일 ID 재사용 허용.
 *   실제 Protocol에 Session Generation/Boot Counter 등 별도 구분 근거가
 *   확정된 경우에만 사용 권장.
 */
typedef enum
{
    GSM_SESSION_REUSE_STRICT = 0,
    GSM_SESSION_REUSE_ALLOW
} GatewaySessionReusePolicy_t;

typedef struct
{
    GatewaySessionReusePolicy_t reusePolicy;
} GatewaySessionConfig_t;

typedef enum
{
    GSM_TRANSITION_NONE = 0,
    GSM_TRANSITION_SESSION_ACTIVATED,
    GSM_TRANSITION_SESSION_CHANGED,
    GSM_TRANSITION_SESSION_INVALIDATED,
    GSM_TRANSITION_SESSION_REUSE_REJECTED
} GatewaySessionTransitionType_t;

typedef struct
{
    GatewaySessionTransitionType_t type;
    DeviceContextId_t deviceContextId;

    bool hadPreviousSession;
    SessionId_t previousSessionId;

    bool hasCurrentSession;
    SessionId_t currentSessionId;

    uint32_t sessionGeneration;
} GatewaySessionTransition_t;

typedef enum
{
    GSM_REQUEST_VALID = 0,
    GSM_REQUEST_NO_DEVICE_CONTEXT,
    GSM_REQUEST_CONNECTION_QUALITY_INVALID,
    GSM_REQUEST_DEVICE_NOT_REGISTERED,
    GSM_REQUEST_BT_NOT_CONNECTED,
    GSM_REQUEST_NO_ACTIVE_SESSION,
    GSM_REQUEST_SESSION_MISMATCH,
    GSM_REQUEST_RETIRED_SESSION,
    GSM_REQUEST_SESSION_REUSE_BLOCKED
} GatewayRequestValidation_t;

typedef struct
{
    bool inUse;
    DeviceContextId_t deviceContextId;

    DigitalKeyConnectionState_t connection;

    GatewaySessionState_t sessionState;
    bool hasActiveSession;
    SessionId_t activeSessionId;

    uint32_t sessionGeneration;
    uint32_t lastUpdatedAtMs;

    SessionId_t retiredSessions[GATEWAY_SESSION_RETIRED_PER_DEVICE];
    uint8_t retiredCount;
    uint8_t retiredWriteIndex;
} GatewaySessionRecord_t;


/* ============================================================================
 * Init
 * ============================================================================
 */

GatewaySessionStatus_t GatewaySessionManager_Init(
    const GatewaySessionConfig_t *config);

bool GatewaySessionManager_IsInitialized(void);


/* ============================================================================
 * Connection / Session update
 * ============================================================================
 */

/**
 * @brief ESP32가 제공한 현재 등록/BT 연결/App Active/Session 정보를 반영.
 *
 * outTransition은 선택(NULL 허용).
 *
 * ACTIVE 조건:
 * - connection.meta.quality == DATA_QUALITY_OK
 * - registration == REGISTERED
 * - btConnection == CONNECTED
 *
 * Session ID 자체의 숫자값(0 포함)은 여기서 의미를 임의 해석하지 않는다.
 */
GatewaySessionStatus_t GatewaySessionManager_UpdateConnection(
    DeviceContextId_t deviceContextId,
    const DigitalKeyConnectionState_t *connection,
    uint32_t nowMs,
    GatewaySessionTransition_t *outTransition);

/**
 * @brief 특정 Device Context를 현재 수용 불가로 만든다.
 *
 * UART/ESP32 restart, 명시적 reconnect boundary 등에서 사용 가능.
 * 현재 active session은 retired 처리한다.
 */
GatewaySessionStatus_t GatewaySessionManager_InvalidateDevice(
    DeviceContextId_t deviceContextId,
    uint32_t nowMs,
    GatewaySessionTransition_t *outTransition);

/**
 * @brief 모든 현재 Session을 무효화한다.
 *
 * Gateway restart / global resync 시 사용한다.
 * Record와 retired evidence는 RAM 안에서 유지한다.
 */
GatewaySessionStatus_t GatewaySessionManager_InvalidateAll(
    uint32_t nowMs);


/* ============================================================================
 * Request validation
 * ============================================================================
 */

/**
 * @brief Request가 현재 Device/Session Context에 속하는지 검증.
 *
 * 이 함수가 VALID을 반환한 뒤 RequestManager_Register()를 호출한다.
 *
 * 다음은 이 함수의 책임 밖:
 * - Request ID duplicate/conflict
 * - 기능별 차량 실행 허용
 * - UART CRC/Frame validation
 */
GatewayRequestValidation_t GatewaySessionManager_ValidateRequest(
    DeviceContextId_t deviceContextId,
    const RequestContext_t *requestContext);

/**
 * @brief 현재 Device가 ACTIVE Session을 가지고 있는지.
 */
bool GatewaySessionManager_HasActiveSession(
    DeviceContextId_t deviceContextId);

/**
 * @brief Digital Key 등에 필요한 App Active 조회.
 *
 * App Active는 일반 Session validity와 동일한 개념이 아니다.
 */
GatewaySessionStatus_t GatewaySessionManager_GetAppActive(
    DeviceContextId_t deviceContextId,
    AppActiveState_t *outState);


/* ============================================================================
 * Query
 * ============================================================================
 */

GatewaySessionStatus_t GatewaySessionManager_GetRecord(
    DeviceContextId_t deviceContextId,
    GatewaySessionRecord_t *outRecord);

GatewaySessionStatus_t GatewaySessionManager_GetActiveSession(
    DeviceContextId_t deviceContextId,
    SessionId_t *outSessionId,
    uint32_t *outGeneration);

#ifdef __cplusplus
}
#endif

#endif /* GATEWAY_SESSION_MANAGER_H */
