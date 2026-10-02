/**
 * @file PermissionManager.h
 * @brief Domain 기능 실행 전 공통 Gate 평가
 *
 * ============================================================================
 * 책임
 * ============================================================================
 *
 * PermissionManager는 "공통 실행 전제조건"만 평가한다.
 *
 * 예:
 *   - Domain이 기능 평가 가능한 Lifecycle인가?
 *   - 현재 Device/Session이 유효한가?
 *   - 현재 구성에서 해당 기능이 지원되는가?
 *   - Function Availability가 실행을 허용하는가?
 *   - 이 기능에 실제로 필요한 입력의 Quality가 사용 가능한가?
 *   - Power / Operation Permission이 필요한 기능인가? 현재 허용되는가?
 *
 * 하지 않는 것:
 *   - FAR -> NEAR 자동 Unlock 판단              -> DigitalKeyManager
 *   - Auto Ventilation 30/28°C 판단             -> AutoVentilationManager
 *   - Door CLOSED 여부 등 기능 고유 정책        -> Feature/BCM local rule
 *   - Request duplicate/session identity 자체 관리 -> GatewaySession/RequestManager
 *   - Fault 원인 집계                           -> DiagnosticManager
 *
 * ============================================================================
 * 설계 원칙
 * ============================================================================
 *
 * "무관한 입력의 STALE 하나로 전체 기능을 막지 않는다."
 *
 * 따라서 기능마다 필요한 Gate를 requirement mask로 명시한다.
 *
 * 예:
 *   Manual Fan:
 *     DOMAIN + SESSION + SUPPORT + AVAILABILITY + POWER
 *
 *   Auto Climate:
 *     위 조건 + REQUIRED_INPUT_QUALITY
 *
 *   STOP/OFF:
 *     OPERATION_PERMISSION 유지 여부를 요구하지 않을 수 있음.
 */

#ifndef PERMISSION_MANAGER_H
#define PERMISSION_MANAGER_H

#include <stdbool.h>
#include <stdint.h>

#include "DomainLifecycleManager.h"
#include "Vehicle_Types.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef uint32_t PermissionRequirementMask_t;

#define PERMISSION_REQUIRE_DOMAIN_OPERATIONAL    (1UL << 0)
#define PERMISSION_REQUIRE_SESSION_VALID         (1UL << 1)
#define PERMISSION_REQUIRE_FUNCTION_SUPPORTED    (1UL << 2)
#define PERMISSION_REQUIRE_FUNCTION_AVAILABLE    (1UL << 3)
#define PERMISSION_REQUIRE_INPUT_QUALITY          (1UL << 4)
#define PERMISSION_REQUIRE_POWER_PERMISSION       (1UL << 5)
#define PERMISSION_REQUIRE_OPERATION_PERMISSION   (1UL << 6)

#define PERMISSION_REQUIRE_ALL_DEFINED \
    ((1UL << 7) - 1UL)

typedef enum
{
    PERMISSION_STATUS_OK = 0,
    PERMISSION_STATUS_INVALID_ARGUMENT,
    PERMISSION_STATUS_NOT_INITIALIZED
} PermissionManager_Status_t;

typedef enum
{
    PERMISSION_DECISION_ALLOW = 0,
    PERMISSION_DECISION_DENY
} PermissionDecision_t;

typedef enum
{
    PERMISSION_REASON_NONE = 0,

    PERMISSION_REASON_DOMAIN_NOT_OPERATIONAL,
    PERMISSION_REASON_SESSION_INVALID,
    PERMISSION_REASON_FUNCTION_UNSUPPORTED,
    PERMISSION_REASON_FUNCTION_UNAVAILABLE,
    PERMISSION_REASON_FUNCTION_DEGRADED_NOT_ALLOWED,
    PERMISSION_REASON_REQUIRED_INPUT_STALE,
    PERMISSION_REASON_REQUIRED_INPUT_INVALID,
    PERMISSION_REASON_REQUIRED_INPUT_NO_DATA,
    PERMISSION_REASON_POWER_PERMISSION_UNKNOWN,
    PERMISSION_REASON_POWER_NOT_ALLOWED,
    PERMISSION_REASON_OPERATION_PERMISSION_UNKNOWN,
    PERMISSION_REASON_OPERATION_NOT_ALLOWED
} PermissionReason_t;

/**
 * @brief Function Availability가 DEGRADED일 때 공통 Gate 처리 정책.
 *
 * DEGRADED를 무조건 전역 차단하지 않는다.
 * 해당 기능이 제한 운용 가능한지 Feature/Diagnostic 설계가 알려준다.
 */
typedef enum
{
    PERMISSION_DEGRADED_DENY = 0,
    PERMISSION_DEGRADED_ALLOW
} PermissionDegradedPolicy_t;

/**
 * @brief Power / Operation Permission의 논리 상태.
 *
 * bool 하나만 쓰지 않는 이유:
 * "NO_DATA/UNKNOWN"과 명시적인 "NOT_ALLOWED"를 구분하기 위해서다.
 */
typedef enum
{
    PERMISSION_SIGNAL_UNKNOWN = 0,
    PERMISSION_SIGNAL_ALLOWED,
    PERMISSION_SIGNAL_NOT_ALLOWED
} PermissionSignalState_t;

/**
 * @brief 공통 Gate가 평가할 사실(Facts).
 *
 * 이 구조를 누가 채우는가:
 * - lifecycleState      : DomainLifecycleManager
 * - sessionValid        : GatewaySessionManager
 * - functionSupported   : 현재 구성/Feature Config
 * - functionAvailability: DiagnosticManager
 * - requiredInputQuality: VehicleStateManager에서 해당 기능이 실제 사용하는 입력만
 * - powerPermission     : VehicleUsage/Power logical input
 * - operationPermission : 기능/실행 노드의 유지/구동 허용 정보
 */
typedef struct
{
    DomainLifecycleState_t lifecycleState;

    bool sessionValid;
    bool functionSupported;

    FunctionAvailability_t functionAvailability;
    PermissionDegradedPolicy_t degradedPolicy;

    DataQuality_t requiredInputQuality;

    PermissionSignalState_t powerPermission;
    PermissionSignalState_t operationPermission;
} PermissionFacts_t;

/**
 * @brief 평가 요청.
 *
 * requirements:
 *   이 기능/행동에 필요한 Gate만 지정한다.
 *
 * 예:
 *   STOP/OFF는 Operation Keep-Alive 상실 자체 때문에 거부하면 안 되는 경우가
 *   있으므로 PERMISSION_REQUIRE_OPERATION_PERMISSION을 제외할 수 있다.
 */
typedef struct
{
    PermissionRequirementMask_t requirements;
    PermissionFacts_t facts;
} PermissionEvaluation_t;

typedef struct
{
    PermissionDecision_t decision;
    PermissionReason_t reason;

    /**
     * 요구한 Gate 중 최초 실패 bit.
     * ALLOW면 0.
     */
    PermissionRequirementMask_t failedRequirement;
} PermissionResult_t;


/* ============================================================================
 * Init / Evaluate
 * ============================================================================
 */

PermissionManager_Status_t PermissionManager_Init(void);

bool PermissionManager_IsInitialized(void);

/**
 * @brief 공통 Gate 평가.
 *
 * 평가 순서는:
 * Domain -> Session -> Support -> Availability
 * -> Required Input -> Power -> Operation
 *
 * 첫 실패 이유를 반환한다.
 */
PermissionManager_Status_t PermissionManager_Evaluate(
    const PermissionEvaluation_t *evaluation,
    PermissionResult_t *outResult);


/* ============================================================================
 * Helper
 * ============================================================================
 */

/**
 * @brief PermissionReason을 기존 공통 ResultReason으로 축약.
 *
 * 상세 PermissionReason은 내부 진단/로그에 유지하고,
 * Request Result에는 공통 ResultReason이 필요할 때 사용한다.
 */
ResultReason_t PermissionManager_ToResultReason(
    PermissionReason_t reason);

#ifdef __cplusplus
}
#endif

#endif /* PERMISSION_MANAGER_H */
