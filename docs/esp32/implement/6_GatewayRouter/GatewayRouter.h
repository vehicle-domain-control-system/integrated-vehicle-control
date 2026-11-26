#ifndef GATEWAY_ROUTER_H
#define GATEWAY_ROUTER_H

#include <stdbool.h>

#include "Gateway_Interface.h"
#include "Gateway_Types.h"

/*
 * GatewayRouter.h
 *
 * ESP32 Wireless Gateway의 논리 Relay Coordinator.
 *
 * 책임:
 * - MOBILE -> Domain 메시지의 현재 Session 확인
 * - Domain -> MOBILE 메시지의 현재 Session 확인
 * - 목적지 Link/Path가 AVAILABLE인지 확인
 * - 새 MOBILE control request는 Gateway Lifecycle READY에서만 허용
 * - Context 식별을 OwnedContext로 안전하게 캡처한 뒤 동일 식별로 전달
 * - Gateway_Interface의 Core Handler로 연결 가능한 Callback 제공
 *
 * 하지 않는 것:
 * - Session 생성/인증/등록 판단
 * - Request Duplicate 차량 정책
 * - ACCEPTED / DONE / REJECTED / FAILED 생성
 * - Payload 의미 변경
 * - Link Recovery 수행
 * - UART/Bluetooth 재전송 Queue 관리
 * - 경로 복구 후 과거 Request 자동 Replay
 */

typedef struct
{
    bool initialized;
    uint32_t mobile_to_domain_forwarded;
    uint32_t domain_to_mobile_forwarded;
    uint32_t rejected_not_ready;
    uint32_t rejected_invalid;
} GatewayRouter_Snapshot_t;


/* -------------------------------------------------------------------------- */
/* Lifecycle                                                                  */
/* -------------------------------------------------------------------------- */

Gateway_Status_t GatewayRouter_Init(void);

void GatewayRouter_Reset(void);

bool GatewayRouter_IsInitialized(void);

Gateway_Status_t GatewayRouter_GetSnapshot(
    GatewayRouter_Snapshot_t *snapshot);


/* -------------------------------------------------------------------------- */
/* Gateway_Interface Core Handler callbacks                                   */
/* -------------------------------------------------------------------------- */

/*
 * Gateway_InterfaceCoreHandlers_t에 직접 등록할 수 있는 Callback이다.
 *
 * user_context는 현재 사용하지 않으며 NULL이어도 된다.
 */
Gateway_Status_t GatewayRouter_OnMobileMessage(
    const Gateway_RelayMessageView_t *message,
    void *user_context);

Gateway_Status_t GatewayRouter_OnDomainMessage(
    const Gateway_RelayMessageView_t *message,
    void *user_context);


/* -------------------------------------------------------------------------- */
/* Helper                                                                     */
/* -------------------------------------------------------------------------- */

/*
 * Gateway_Interface_Init()에 전달할 Core Handler 구조체를 채운다.
 */
Gateway_Status_t GatewayRouter_BuildCoreHandlers(
    Gateway_InterfaceCoreHandlers_t *handlers);

#endif /* GATEWAY_ROUTER_H */
