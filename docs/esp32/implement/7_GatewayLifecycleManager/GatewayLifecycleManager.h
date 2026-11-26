#ifndef GATEWAY_LIFECYCLE_MANAGER_H
#define GATEWAY_LIFECYCLE_MANAGER_H

#include <stdbool.h>
#include <stdint.h>

#include "Gateway_Time.h"
#include "Gateway_Types.h"

/*
 * GatewayLifecycleManager.h
 *
 * ESP32 Wireless Gateway 전체 운영 상태를 조정한다.
 *
 * DESIGN 상태:
 * STARTUP
 * LINK_WAIT
 * SYNCING
 * READY
 * DEGRADED
 *
 * 위 상태명은 SysRS 공통 enum이 아니라 ESP32 내부 Architecture 상태다.
 *
 * 책임:
 * - Boot 이후 Gateway 운영 상태 관리
 * - Bluetooth/UART Path 및 Current Session 준비 상태 관찰
 * - Link/Session 상실 시 동기화 필요 상태로 전환
 * - 복구 후 Session 재확인 + Resynchronization 완료 전 READY 금지
 * - 새 MOBILE control request를 Relay할 수 있는지 제공
 *
 * 하지 않는 것:
 * - Bluetooth reconnect 구현
 * - UART handshake/frame 구현
 * - 필요한 Vehicle State Query의 실제 Wire 메시지 생성
 * - 차량 Function Availability 판단
 * - 과거 Request Replay
 */

typedef enum
{
    GATEWAY_LIFECYCLE_STARTUP = 0,
    GATEWAY_LIFECYCLE_LINK_WAIT,
    GATEWAY_LIFECYCLE_SYNCING,
    GATEWAY_LIFECYCLE_READY,
    GATEWAY_LIFECYCLE_DEGRADED
} GatewayLifecycle_State_t;

typedef struct
{
    bool initialized;
    bool started;

    GatewayLifecycle_State_t state;
    Gateway_TimeMs_t state_since_ms;

    bool synchronization_required;
    bool session_reconfirmation_required;
    bool has_reached_ready;

    uint32_t transition_count;
} GatewayLifecycle_Snapshot_t;


/* -------------------------------------------------------------------------- */
/* Lifecycle                                                                  */
/* -------------------------------------------------------------------------- */

Gateway_Status_t GatewayLifecycleManager_Init(void);

void GatewayLifecycleManager_Reset(void);

bool GatewayLifecycleManager_IsInitialized(void);

/*
 * Driver / Adapter / Core binding이 끝난 뒤 호출한다.
 *
 * STARTUP -> LINK_WAIT
 * 이미 Link + Session이 준비되어 있으면 같은 호출에서 SYNCING까지 진입할 수 있다.
 */
Gateway_Status_t GatewayLifecycleManager_StartAt(
    Gateway_TimeMs_t now_ms);

Gateway_Status_t GatewayLifecycleManager_Start(void);


/* -------------------------------------------------------------------------- */
/* Periodic evaluation                                                        */
/* -------------------------------------------------------------------------- */

/*
 * LinkStateManager / MessageContextManager의 현재 상태를 관찰해
 * Lifecycle을 갱신한다.
 *
 * READY 또는 SYNCING 중 필수 Link가 상실되면 현재 Session confirmation을
 * 무효화하여 recovery 후 재확인을 강제한다.
 */
Gateway_Status_t GatewayLifecycleManager_UpdateAt(
    Gateway_TimeMs_t now_ms);

Gateway_Status_t GatewayLifecycleManager_UpdateNow(void);


/* -------------------------------------------------------------------------- */
/* Synchronization                                                            */
/* -------------------------------------------------------------------------- */

/*
 * 현재 Connection/Session/필요 상태의 재동기화가 완료됐음을 상위 integration
 * layer가 통보한다.
 *
 * 실제 sync wire protocol은 NETWORK-TBD다.
 */
Gateway_Status_t GatewayLifecycleManager_MarkSynchronizationCompleteAt(
    Gateway_TimeMs_t now_ms);

Gateway_Status_t GatewayLifecycleManager_MarkSynchronizationComplete(void);

/*
 * 중앙 재시작, 통신 복구, 명시적 resync 필요 시 호출한다.
 *
 * 현재 Session confirmation을 무효화하고 다시 확인하도록 한다.
 * 과거 Request를 자동 Replay하지 않는다.
 */
Gateway_Status_t GatewayLifecycleManager_RequestResynchronizationAt(
    Gateway_TimeMs_t now_ms);

Gateway_Status_t GatewayLifecycleManager_RequestResynchronization(void);


/* -------------------------------------------------------------------------- */
/* Query                                                                      */
/* -------------------------------------------------------------------------- */

GatewayLifecycle_State_t GatewayLifecycleManager_GetState(void);

bool GatewayLifecycleManager_IsReady(void);

/*
 * 새 MOBILE vehicle control request relay 가능 여부.
 *
 * READY일 때만 true.
 * STATE_QUERY / 내부 Sync traffic의 허용 여부는 별도 경로에서 결정한다.
 */
bool GatewayLifecycleManager_CanRelayNewControlRequest(void);

Gateway_Status_t GatewayLifecycleManager_GetSnapshot(
    GatewayLifecycle_Snapshot_t *snapshot);

#endif /* GATEWAY_LIFECYCLE_MANAGER_H */
