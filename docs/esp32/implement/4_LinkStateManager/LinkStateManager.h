#ifndef LINK_STATE_MANAGER_H
#define LINK_STATE_MANAGER_H

#include <stdbool.h>

#include "Gateway_Time.h"
#include "Gateway_Types.h"

/*
 * LinkStateManager.h
 *
 * ESP32 Wireless Gateway의 Bluetooth / Domain UART Path 상태 관리자.
 *
 * 책임:
 * - Bluetooth와 UART Path 상태를 독립적으로 관리
 * - 상태 변경 시각 관리
 * - 마지막 유효 수신 시각 관리
 * - NETWORK 설계에서 확정된 경우 Timeout 기반 상실 감시
 * - Recovery 상태 관리
 *
 * 하지 않는 것:
 * - Bluetooth 등록 여부 판단
 * - Proximity NEAR/FAR 판단
 * - 차량 Request ACCEPTED/DONE/FAILED 판단
 * - 재연결 시도 횟수/Backoff 정책
 * - UART Frame/CRC/Sequence 검증
 */

typedef struct
{
    bool enabled;
    Gateway_TimeMs_t timeout_ms;
} LinkStateManager_TimeoutConfig_t;

typedef struct
{
    Gateway_LinkState_t state;

    Gateway_TimeMs_t state_since_ms;

    bool has_valid_rx;
    Gateway_TimeMs_t last_valid_rx_ms;

    LinkStateManager_TimeoutConfig_t timeout;
} LinkStateManager_LinkSnapshot_t;


/* -------------------------------------------------------------------------- */
/* Lifecycle                                                                  */
/* -------------------------------------------------------------------------- */

Gateway_Status_t LinkStateManager_Init(void);

void LinkStateManager_Reset(void);

bool LinkStateManager_IsInitialized(void);


/* -------------------------------------------------------------------------- */
/* Configuration                                                              */
/* -------------------------------------------------------------------------- */

/*
 * Timeout 감시를 설정한다.
 *
 * enabled == false:
 *   timeout_ms 값은 사용하지 않으며 Timeout 기반 상태 변경을 하지 않는다.
 *
 * enabled == true:
 *   timeout_ms > 0 이어야 한다.
 *
 * 실제 Timeout 값은 NETWORK-TBD이며 이 Manager가 임의 기본값을 정하지 않는다.
 */
Gateway_Status_t LinkStateManager_ConfigureTimeout(
    Gateway_LinkId_t link,
    LinkStateManager_TimeoutConfig_t config);


/* -------------------------------------------------------------------------- */
/* Explicit state events                                                      */
/* -------------------------------------------------------------------------- */

Gateway_Status_t LinkStateManager_MarkAvailableAt(
    Gateway_LinkId_t link,
    Gateway_TimeMs_t now_ms);

Gateway_Status_t LinkStateManager_MarkUnavailableAt(
    Gateway_LinkId_t link,
    Gateway_TimeMs_t now_ms);

Gateway_Status_t LinkStateManager_BeginRecoveryAt(
    Gateway_LinkId_t link,
    Gateway_TimeMs_t now_ms);

/*
 * Adapter/Codec이 "유효한 새 수신"을 확인했을 때 호출한다.
 *
 * 유효 수신은 해당 경로가 현재 사용 가능하다는 강한 근거이므로
 * 상태를 AVAILABLE로 갱신한다.
 *
 * Frame 유효성 판단 자체는 Adapter/Codec 책임이다.
 */
Gateway_Status_t LinkStateManager_OnValidRxAt(
    Gateway_LinkId_t link,
    Gateway_TimeMs_t now_ms);


/* 현재 Gateway_Time을 사용하는 convenience 함수 */

Gateway_Status_t LinkStateManager_MarkAvailable(
    Gateway_LinkId_t link);

Gateway_Status_t LinkStateManager_MarkUnavailable(
    Gateway_LinkId_t link);

Gateway_Status_t LinkStateManager_BeginRecovery(
    Gateway_LinkId_t link);

Gateway_Status_t LinkStateManager_OnValidRx(
    Gateway_LinkId_t link);


/* -------------------------------------------------------------------------- */
/* Periodic supervision                                                       */
/* -------------------------------------------------------------------------- */

/*
 * Timeout 감시가 활성화된 AVAILABLE Link를 평가한다.
 *
 * 기준:
 * - 유효 수신 이력이 있으면 last_valid_rx_ms 기준
 * - 아직 유효 수신이 없으면 AVAILABLE 진입 시각 기준
 *
 * Timeout 만료 시:
 * AVAILABLE -> UNAVAILABLE
 *
 * RECOVERING에는 동일 Timeout을 자동 적용하지 않는다.
 * Recovery 자체의 별도 기한은 후속 정책에서 필요 시 추가한다.
 */
Gateway_Status_t LinkStateManager_UpdateAt(
    Gateway_TimeMs_t now_ms);

Gateway_Status_t LinkStateManager_UpdateNow(void);


/* -------------------------------------------------------------------------- */
/* Query                                                                      */
/* -------------------------------------------------------------------------- */

Gateway_Status_t LinkStateManager_GetSnapshot(
    Gateway_LinkId_t link,
    LinkStateManager_LinkSnapshot_t *snapshot);

Gateway_LinkState_t LinkStateManager_GetState(
    Gateway_LinkId_t link);

bool LinkStateManager_IsAvailable(
    Gateway_LinkId_t link);

#endif /* LINK_STATE_MANAGER_H */
