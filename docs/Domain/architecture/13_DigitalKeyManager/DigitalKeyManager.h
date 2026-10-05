/**
 * @file DigitalKeyManager.h
 * @brief Digital Key 근접 자동 잠금 해제 Feature Policy
 *
 * ============================================================================
 * 요구 의미
 * ============================================================================
 *
 * 자동 잠금 해제는 다음을 모두 만족할 때만 후보가 된다.
 *
 * - 등록된 현재 연결
 * - App Active
 * - 현재 Session 유효
 * - 해당 연결의 Digital Key setting == ON
 * - 유효한 새 접근
 * - BCM Door state 신뢰 가능
 * - 공통 Permission Gate 허용
 *
 * 새 접근:
 *   활성 조건 이후 VALID FAR를 확인한 다음 VALID NEAR로 전이
 *
 * 금지:
 *   - 새 연결 직후 이미 NEAR -> 자동 Unlock
 *   - STALE/INVALID/UNKNOWN/연결 상실을 FAR로 사용
 *   - 같은 접근에서 거부/실패/이미 해제됨 이후 자동 재요청
 *   - 같은 접근에서 수동/외부 Lock 이후 다시 자동 Unlock
 *
 * 실제 성공:
 *   BCM의 실제 UNLOCK_COMPLETED 전이가 확인된 경우만
 *   actualUnlockTransition=true.
 */

#ifndef DIGITAL_KEY_MANAGER_H
#define DIGITAL_KEY_MANAGER_H

#include <stdbool.h>
#include <stdint.h>

#include "CommandManager.h"
#include "Domain_Interface.h"
#include "PermissionManager.h"
#include "ResultManager.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum
{
    DIGITAL_KEY_MANAGER_STATUS_OK = 0,
    DIGITAL_KEY_MANAGER_STATUS_INVALID_ARGUMENT,
    DIGITAL_KEY_MANAGER_STATUS_NOT_INITIALIZED,
    DIGITAL_KEY_MANAGER_STATUS_DEPENDENCY_NOT_READY,
    DIGITAL_KEY_MANAGER_STATUS_COMMAND_ERROR
} DigitalKeyManager_Status_t;

typedef enum
{
    DIGITAL_KEY_APPROACH_WAIT_FAR = 0,
    DIGITAL_KEY_APPROACH_ARMED_FAR,
    DIGITAL_KEY_APPROACH_ACTIVE,
    DIGITAL_KEY_APPROACH_CONSUMED
} DigitalKeyApproachState_t;

typedef enum
{
    DIGITAL_KEY_DECISION_NONE = 0,
    DIGITAL_KEY_DECISION_SETTING_OFF,
    DIGITAL_KEY_DECISION_NO_ACTIVE_SESSION,
    DIGITAL_KEY_DECISION_APP_NOT_ACTIVE,
    DIGITAL_KEY_DECISION_FEATURE_UNAVAILABLE,
    DIGITAL_KEY_DECISION_PROXIMITY_UNTRUSTED,
    DIGITAL_KEY_DECISION_WAITING_VALID_FAR,
    DIGITAL_KEY_DECISION_FAR_ARMED,
    DIGITAL_KEY_DECISION_NEW_APPROACH,
    DIGITAL_KEY_DECISION_DOOR_UNTRUSTED,
    DIGITAL_KEY_DECISION_ALREADY_UNLOCKED,
    DIGITAL_KEY_DECISION_PERMISSION_DENIED,
    DIGITAL_KEY_DECISION_COMMAND_ALREADY_PENDING,
    DIGITAL_KEY_DECISION_UNLOCK_COMMAND_DISPATCHED,
    DIGITAL_KEY_DECISION_UNLOCK_COMMAND_DISPATCH_FAILED,
    DIGITAL_KEY_DECISION_RESULT_TIMEOUT
} DigitalKeyDecisionReason_t;

typedef struct
{
    bool supported;

    /**
     * DiagnosticManager가 아직 구현되지 않은 현재 단계의 초기값.
     * 이후 DiagnosticManager가 SetAvailability()로 갱신한다.
     */
    FunctionAvailability_t initialAvailability;

    PermissionDegradedPolicy_t degradedPolicy;
} DigitalKeyManager_Config_t;

typedef struct
{
    bool contextKnown;
    DeviceContextId_t deviceContextId;
    SessionId_t sessionId;
    uint32_t sessionGeneration;

    DigitalKeySetting_t setting;
    FunctionAvailability_t availability;
    AppActiveState_t appActive;

    ProximityState_t lastProximity;
    DigitalKeyApproachState_t approachState;

    bool hasPendingCommand;
    DomainCommandId_t pendingCommandId;
    uint32_t commandIssuedAtMs;

    bool hasActualUnlockHistory;
    UnlockOrigin_t lastUnlockOrigin;
    DomainCommandId_t lastCompletedCommandId;

    uint32_t observedSettingRevision;
} DigitalKeyManager_Snapshot_t;

typedef struct
{
    DigitalKeyDecisionReason_t reason;

    bool commandCreated;
    bool commandDispatched;
    DomainCommandId_t commandId;
    DomainIf_Status_t txStatus;

    PermissionResult_t permission;
} DigitalKeyManager_ProcessOutput_t;

typedef struct
{
    bool relevant;
    DomainIf_DigitalKeyResult_t mobileResult;
} DigitalKeyManager_ResultOutput_t;


/* ============================================================================
 * Init / Configuration
 * ============================================================================
 */

void DigitalKeyManager_LoadCurrentProjectDefaults(
    DigitalKeyManager_Config_t *outConfig);

DigitalKeyManager_Status_t DigitalKeyManager_Init(
    const DigitalKeyManager_Config_t *config);

bool DigitalKeyManager_IsInitialized(void);

DigitalKeyManager_Status_t DigitalKeyManager_SetAvailability(
    FunctionAvailability_t availability);


/* ============================================================================
 * Main processing
 * ============================================================================
 */

/**
 * @brief 현재 Device Context의 Digital Key 조건을 평가.
 *
 * Domain Task에서 관련 상태 변경 후 또는 주기적으로 호출할 수 있다.
 *
 * 필요한 상태는 다음 Manager에서 읽는다.
 * - GatewaySessionManager
 * - SettingsManager
 * - VehicleStateManager
 * - DomainLifecycleManager
 * - PermissionManager
 *
 * 자동 Unlock이 필요하면:
 * - CommandManager_Create()
 * - DomainIf_TxBcmDoorCommand()
 * 까지 수행한다.
 */
DigitalKeyManager_Status_t DigitalKeyManager_Process(
    DeviceContextId_t deviceContextId,
    uint32_t nowMs,
    DigitalKeyManager_ProcessOutput_t *outResult);


/* ============================================================================
 * BCM result / manual lock observation
 * ============================================================================
 */

/**
 * @brief Domain_Interface가 ResultManager 처리 후 전달한 BCM Event를 해석.
 *
 * - 자동 Unlock command 결과 추적
 * - 실제 UNLOCK_COMPLETED만 actual unlock으로 기록
 * - ALREADY_AT_TARGET는 실제 unlock 성공으로 기록하지 않음
 * - LOCK_COMPLETED가 현재 NEAR 접근에서 발생하면 같은 접근 재해제를 차단
 */
DigitalKeyManager_Status_t DigitalKeyManager_OnBcmEvent(
    const DomainIf_BcmEvent_t *event,
    uint32_t nowMs,
    DigitalKeyManager_ResultOutput_t *outResult);


/* ============================================================================
 * Query
 * ============================================================================
 */

DigitalKeyManager_Status_t DigitalKeyManager_GetSnapshot(
    DigitalKeyManager_Snapshot_t *outSnapshot);

#ifdef __cplusplus
}
#endif

#endif /* DIGITAL_KEY_MANAGER_H */
