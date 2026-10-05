/**
 * @file DomainLifecycleManager.h
 * @brief Domain 전체 STARTUP / SYNCING / READY / DEGRADED 운영 상태 관리
 *
 * 목적:
 * - MCU/Domain 초기화 직후 오래된 상태를 기반으로 기능을 실행하지 않는다.
 * - 재연결/통신 복구 시 필요한 논리 정보를 다시 동기화한다.
 * - 특정 ECU/Interface가 불가하더라도 전체 Domain Reset으로 확대하지 않고
 *   DEGRADED 상태와 Feature Availability를 구분할 수 있게 한다.
 *
 * 주의:
 * - 이 상태 enum은 내부 Software Architecture용 DESIGN이다.
 * - SR/SysRS의 새 외부 차량 상태 Signal을 추가하는 것이 아니다.
 * - 각 기능의 실제 실행 허용은 PermissionManager + DiagnosticManager가
 *   Feature 단위 Availability와 함께 최종 판단한다.
 */

#ifndef DOMAIN_LIFECYCLE_MANAGER_H
#define DOMAIN_LIFECYCLE_MANAGER_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum
{
    DOMAIN_LIFECYCLE_UNINITIALIZED = 0,
    DOMAIN_LIFECYCLE_STARTUP,
    DOMAIN_LIFECYCLE_SYNCING,
    DOMAIN_LIFECYCLE_READY,
    DOMAIN_LIFECYCLE_DEGRADED
} DomainLifecycleState_t;

/**
 * @brief 초기/재동기화가 필요한 논리 정보 단위.
 *
 * 물리 Message ID가 아니라 Logical Group이다.
 */
typedef uint32_t DomainLifecycleSyncMask_t;

#define DLM_SYNC_BCM_DOOR            (1UL << 0)
#define DLM_SYNC_BCM_CLIMATE         (1UL << 1)
#define DLM_SYNC_BCM_INTERIOR_LIGHT  (1UL << 2)
#define DLM_SYNC_CIS_OCCUPANT        (1UL << 3)
#define DLM_SYNC_CIS_CABIN           (1UL << 4)
#define DLM_SYNC_CIS_REAR            (1UL << 5)
#define DLM_SYNC_WINDOW_STATE        (1UL << 6)
#define DLM_SYNC_VSS_STATE           (1UL << 7)
#define DLM_SYNC_ESP32_CONNECTION    (1UL << 8)
#define DLM_SYNC_ESP32_PROXIMITY     (1UL << 9)

/*
 * Vehicle Usage 입력은 Architecture상 필요하지만 현재 Producer가 TBD.
 * Producer/Interface가 확정되기 전까지 required mask에 넣지 않아도 된다.
 */
#define DLM_SYNC_VEHICLE_USAGE       (1UL << 10)

#define DLM_SYNC_ALL_DEFINED         ((1UL << 11) - 1UL)

typedef enum
{
    DLM_STATUS_OK = 0,
    DLM_STATUS_INVALID_ARGUMENT,
    DLM_STATUS_NOT_INITIALIZED
} DomainLifecycleStatus_t;

/**
 * @brief Lifecycle 설정.
 *
 * requiredInitialSyncMask:
 *   READY 판단 전에 반드시 현재 상태가 확인되어야 하는 Logical Group.
 *   어떤 Group을 필수로 할지는 프로젝트 통합 설계에서 명시적으로 결정한다.
 *
 * syncTimeoutMs:
 *   0이면 전역 Sync Timeout 비활성.
 *   값이 있으면 미완료 Sync가 해당 시간에 도달할 때 DEGRADED로 전이한다.
 *   Timeout이 곧 ECU 실행 실패를 의미하지는 않는다.
 */
typedef struct
{
    DomainLifecycleSyncMask_t requiredInitialSyncMask;
    uint32_t syncTimeoutMs;
} DomainLifecycleConfig_t;

typedef struct
{
    DomainLifecycleState_t state;

    bool platformReady;
    bool syncTimedOut;

    DomainLifecycleSyncMask_t requiredMask;
    DomainLifecycleSyncMask_t resolvedMask;
    DomainLifecycleSyncMask_t healthyMask;

    uint32_t syncStartedAtMs;
    uint32_t lastTransitionAtMs;
    uint32_t generation;
} DomainLifecycleSnapshot_t;

/**
 * @brief Lifecycle 초기화.
 *
 * config는 필수다.
 * Required sync mask를 임의로 만들어내지 않기 위해 NULL default를 제공하지 않는다.
 */
DomainLifecycleStatus_t DomainLifecycle_Init(
    const DomainLifecycleConfig_t *config,
    uint32_t nowMs);

bool DomainLifecycle_IsInitialized(void);

/**
 * @brief RTD/Task/기본 통신 인프라가 사용할 준비가 되었는지 반영.
 *
 * false:
 * - STARTUP 전이
 * - 기존 sync/health 근거 삭제
 *
 * true:
 * - STARTUP -> SYNCING
 */
DomainLifecycleStatus_t DomainLifecycle_SetPlatformReady(
    bool ready,
    uint32_t nowMs);

/**
 * @brief Logical Group의 "현재 유효 상태 확인" 완료.
 *
 * 새 유효 State/Connection/Proximity 등 현재 동기화 근거가 확보된 경우 호출.
 */
DomainLifecycleStatus_t DomainLifecycle_MarkHealthy(
    DomainLifecycleSyncMask_t item,
    uint32_t nowMs);

/**
 * @brief 해당 Group이 현재 사용 불가임을 확인.
 *
 * "미수신"과 다르게 상태는 확인했지만 사용할 수 없는 경우.
 * Required item이면 전체 Lifecycle은 DEGRADED가 될 수 있다.
 */
DomainLifecycleStatus_t DomainLifecycle_MarkUnavailable(
    DomainLifecycleSyncMask_t item,
    uint32_t nowMs);

/**
 * @brief 통신 상실/재연결 등으로 특정 Logical Group을 다시 동기화.
 *
 * mask에 해당하는 resolved/healthy 근거를 지운다.
 * Required item이 포함되면 SYNCING으로 전이한다.
 */
DomainLifecycleStatus_t DomainLifecycle_BeginResync(
    DomainLifecycleSyncMask_t mask,
    uint32_t nowMs);

/**
 * @brief Sync Timeout 및 현재 Mask를 평가.
 */
DomainLifecycleStatus_t DomainLifecycle_Process(
    uint32_t nowMs);

DomainLifecycleStatus_t DomainLifecycle_GetSnapshot(
    DomainLifecycleSnapshot_t *outSnapshot);

/**
 * @brief 차량 기능 평가 자체가 가능한 운영 상태인지.
 *
 * READY 또는 DEGRADED에서 true.
 * DEGRADED에서는 Permission/Diagnostic이 Feature 단위로 추가 제한한다.
 */
bool DomainLifecycle_AllowsFeatureEvaluation(void);

bool DomainLifecycle_IsFullyReady(void);

#ifdef __cplusplus
}
#endif

#endif /* DOMAIN_LIFECYCLE_MANAGER_H */
