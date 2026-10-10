#ifndef GATEWAY_INTERFACE_H
#define GATEWAY_INTERFACE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "Gateway_Types.h"

/*
 * Gateway_Interface.h
 *
 * ESP32 Wireless Gateway의 논리 Interface Boundary.
 *
 * 역할:
 * 1) Bluetooth Adapter -> Core 로 들어오는 MOBILE 논리 메시지 경계
 * 2) UART Adapter -> Core 로 들어오는 Domain 논리 메시지 경계
 * 3) Core -> Domain / MOBILE 로 나가는 논리 메시지 경계
 * 4) ESP32가 생산하는 Registration / App Activity / Proximity 정보 경계
 *
 * 이 파일은 다음을 정의하지 않는다.
 * - Bluetooth GATT/Classic profile
 * - UART Baud Rate / Header / Message ID / CRC / Sequence
 * - 실제 Vehicle/Device/Session/Request ID의 폭과 Wire 표현
 * - 차량 Request 수용/실행/Result 정책
 */

/* -------------------------------------------------------------------------- */
/* Opaque byte views                                                          */
/* -------------------------------------------------------------------------- */

/*
 * 아직 표현 형식이 확정되지 않은 식별자/논리 payload를 참조하기 위한 view.
 *
 * 이 구조체는 데이터를 소유하지 않는다.
 * 함수 호출이 끝날 때까지 data가 유효해야 한다.
 *
 * length == 0U 이면 data == NULL 이어야 한다.
 * length > 0U 이면 data != NULL 이어야 한다.
 */
typedef struct
{
    const uint8_t *data;
    size_t length;
} Gateway_ByteView_t;

/*
 * MOBILE -> ESP32 -> Domain 사이에서 보존해야 할 식별 Context.
 *
 * 각 ID의 실제 타입/폭은 아직 TBD이므로 ByteView로 유지한다.
 * 빈 view는 해당 Context가 현재 메시지에 없음을 의미한다.
 */
typedef struct
{
    Gateway_ByteView_t vehicle_id;
    Gateway_ByteView_t device_context_id;
    Gateway_ByteView_t session_id;
    Gateway_ByteView_t request_id;
} Gateway_MessageContextView_t;


/* -------------------------------------------------------------------------- */
/* Generic relay message                                                      */
/* -------------------------------------------------------------------------- */

/*
 * MOBILE <-> Domain 사이에서 ESP32가 차량 의미를 바꾸지 않고 중계하는
 * 논리 메시지 view.
 *
 * payload는 Bluetooth frame 또는 UART frame 전체가 아니다.
 * Transport framing을 제거한 "논리 payload"를 의미한다.
 *
 * 실제 payload schema가 확정되면 기능별 DTO로 점진적으로 대체할 수 있다.
 */
typedef struct
{
    Gateway_LogicalMessageType_t type;
    Gateway_MessageDirection_t direction;
    Gateway_MessageContextView_t context;
    Gateway_ByteView_t payload;
} Gateway_RelayMessageView_t;


/* -------------------------------------------------------------------------- */
/* ESP32-produced context updates                                             */
/* -------------------------------------------------------------------------- */

typedef struct
{
    Gateway_MessageContextView_t context;
    Gateway_RegistrationState_t registration;
    Gateway_ConnectionState_t connection;
    Gateway_UpdateBasis_t update;
} Gateway_RegistrationConnectionUpdate_t;

typedef struct
{
    Gateway_MessageContextView_t context;
    Gateway_AppActivityState_t state;
    Gateway_UpdateBasis_t update;
} Gateway_AppActivityUpdate_t;

typedef struct
{
    Gateway_MessageContextView_t context;
    Gateway_ProximityInfo_t proximity;
} Gateway_ProximityUpdate_t;


/* -------------------------------------------------------------------------- */
/* Core handlers                                                              */
/* -------------------------------------------------------------------------- */

typedef Gateway_Status_t (*Gateway_InterfaceMessageHandler_t)(
    const Gateway_RelayMessageView_t *message,
    void *user_context);

typedef struct
{
    Gateway_InterfaceMessageHandler_t on_mobile_message;
    Gateway_InterfaceMessageHandler_t on_domain_message;
    void *user_context;
} Gateway_InterfaceCoreHandlers_t;


/* -------------------------------------------------------------------------- */
/* Transport ports                                                            */
/* -------------------------------------------------------------------------- */

typedef Gateway_Status_t (*Gateway_InterfaceRelaySender_t)(
    const Gateway_RelayMessageView_t *message,
    void *user_context);

typedef Gateway_Status_t (*Gateway_InterfaceRegistrationSender_t)(
    const Gateway_RegistrationConnectionUpdate_t *update,
    void *user_context);

typedef Gateway_Status_t (*Gateway_InterfaceAppActivitySender_t)(
    const Gateway_AppActivityUpdate_t *update,
    void *user_context);

typedef Gateway_Status_t (*Gateway_InterfaceProximitySender_t)(
    const Gateway_ProximityUpdate_t *update,
    void *user_context);

typedef struct
{
    Gateway_InterfaceRelaySender_t send_to_domain;
    Gateway_InterfaceRelaySender_t send_to_mobile;

    Gateway_InterfaceRegistrationSender_t send_registration_to_domain;
    Gateway_InterfaceAppActivitySender_t send_app_activity_to_domain;
    Gateway_InterfaceProximitySender_t send_proximity_to_domain;

    void *user_context;
} Gateway_InterfaceTransportPorts_t;


/* -------------------------------------------------------------------------- */
/* Lifecycle                                                                  */
/* -------------------------------------------------------------------------- */

/*
 * Core Handler와 Transport Port를 등록한다.
 *
 * NULL callback은 허용한다.
 * 해당 기능 호출 시 GATEWAY_STATUS_UNSUPPORTED를 반환한다.
 */
Gateway_Status_t Gateway_Interface_Init(
    const Gateway_InterfaceCoreHandlers_t *core_handlers,
    const Gateway_InterfaceTransportPorts_t *transport_ports);

void Gateway_Interface_Reset(void);

bool Gateway_Interface_IsInitialized(void);


/* -------------------------------------------------------------------------- */
/* Adapter -> Core                                                            */
/* -------------------------------------------------------------------------- */

/*
 * Bluetooth Adapter/Profile이 MOBILE에서 유효한 논리 메시지를 얻었을 때 호출.
 *
 * 허용 direction:
 *   GATEWAY_DIRECTION_MOBILE_TO_DOMAIN
 */
Gateway_Status_t Gateway_Interface_OnMobileMessage(
    const Gateway_RelayMessageView_t *message);

/*
 * UART Adapter/Codec이 Domain에서 유효한 논리 메시지를 얻었을 때 호출.
 *
 * 허용 direction:
 *   GATEWAY_DIRECTION_DOMAIN_TO_MOBILE
 */
Gateway_Status_t Gateway_Interface_OnDomainMessage(
    const Gateway_RelayMessageView_t *message);


/* -------------------------------------------------------------------------- */
/* Core -> Transport                                                          */
/* -------------------------------------------------------------------------- */

/*
 * MOBILE에서 받은 요청/조회/확인 정보를 Domain으로 중계할 때 사용.
 *
 * 허용 direction:
 *   GATEWAY_DIRECTION_MOBILE_TO_DOMAIN
 */
Gateway_Status_t Gateway_Interface_SendToDomain(
    const Gateway_RelayMessageView_t *message);

/*
 * Domain에서 받은 상태/결과/경고/가용성 정보를 MOBILE로 중계할 때 사용.
 *
 * 허용 direction:
 *   GATEWAY_DIRECTION_DOMAIN_TO_MOBILE
 */
Gateway_Status_t Gateway_Interface_SendToMobile(
    const Gateway_RelayMessageView_t *message);


/* -------------------------------------------------------------------------- */
/* ESP32-produced updates -> Domain                                           */
/* -------------------------------------------------------------------------- */

Gateway_Status_t Gateway_Interface_PublishRegistrationConnection(
    const Gateway_RegistrationConnectionUpdate_t *update);

Gateway_Status_t Gateway_Interface_PublishAppActivity(
    const Gateway_AppActivityUpdate_t *update);

Gateway_Status_t Gateway_Interface_PublishProximity(
    const Gateway_ProximityUpdate_t *update);


/* -------------------------------------------------------------------------- */
/* Validation helpers                                                         */
/* -------------------------------------------------------------------------- */

bool Gateway_Interface_IsValidByteView(
    const Gateway_ByteView_t *view);

bool Gateway_Interface_IsValidMessageContext(
    const Gateway_MessageContextView_t *context);

bool Gateway_Interface_IsValidRelayMessage(
    const Gateway_RelayMessageView_t *message);

#endif /* GATEWAY_INTERFACE_H */
