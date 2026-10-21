#ifndef GATEWAY_TYPES_H
#define GATEWAY_TYPES_H

#include <stdbool.h>
#include <stdint.h>

/*
 * Gateway_Types.h
 *
 * ESP32 Wireless Gateway 공통 논리 타입.
 *
 * 이 파일은 차량 정책이나 실제 Bluetooth/UART Wire Format을 정의하지 않는다.
 *
 * 의도적으로 여기서 확정하지 않는 항목:
 * - Vehicle ID 표현
 * - DeviceContextId 표현/폭
 * - SessionId 표현/폭
 * - RequestId 표현/폭
 * - Bluetooth Service / Characteristic ID
 * - UART Message ID / Header / Byte layout
 * - CRC / Sequence / Integrity field
 *
 * 위 항목은 Interface / Network 설계가 확정된 뒤 별도 DTO 또는
 * Network_Config 계층에서 정의한다.
 */

/* -------------------------------------------------------------------------- */
/* Local implementation result                                                */
/* -------------------------------------------------------------------------- */

/*
 * Gateway 내부 함수의 반환 상태다.
 * 차량 Request Result(DONE/FAILED/REJECTED 등)와는 완전히 다른 개념이다.
 */
typedef enum
{
    GATEWAY_STATUS_OK = 0,
    GATEWAY_STATUS_INVALID_ARGUMENT,
    GATEWAY_STATUS_NOT_READY,
    GATEWAY_STATUS_UNSUPPORTED,
    GATEWAY_STATUS_INTERNAL_ERROR
} Gateway_Status_t;


/* -------------------------------------------------------------------------- */
/* Communication path                                                         */
/* -------------------------------------------------------------------------- */

typedef enum
{
    GATEWAY_LINK_BLUETOOTH = 0,
    GATEWAY_LINK_DOMAIN_UART
} Gateway_LinkId_t;

/*
 * "AVAILABLE"은 해당 통신 경로를 현재 사용할 수 있다는 Gateway 내부 의미다.
 *
 * 특히 UART의 경우 Driver Init 완료만으로 AVAILABLE로 간주하지 않는다.
 * Domain과 유효한 통신이 가능한지 확인하는 실제 기준은 NETWORK-TBD다.
 */
typedef enum
{
    GATEWAY_LINK_STATE_UNKNOWN = 0,
    GATEWAY_LINK_STATE_UNAVAILABLE,
    GATEWAY_LINK_STATE_AVAILABLE,
    GATEWAY_LINK_STATE_RECOVERING
} Gateway_LinkState_t;


/* -------------------------------------------------------------------------- */
/* Bluetooth device context                                                   */
/* -------------------------------------------------------------------------- */

typedef enum
{
    GATEWAY_REGISTRATION_UNKNOWN = 0,
    GATEWAY_NOT_REGISTERED,
    GATEWAY_REGISTERED
} Gateway_RegistrationState_t;

typedef enum
{
    GATEWAY_CONNECTION_UNKNOWN = 0,
    GATEWAY_DISCONNECTED,
    GATEWAY_CONNECTED
} Gateway_ConnectionState_t;

/*
 * App Active는 필요한 논리 정보이지만 실제 Producer/획득 방법은 INPUT-TBD다.
 * Bluetooth 연결 여부만으로 ACTIVE/INACTIVE를 만들어서는 안 된다.
 */
typedef enum
{
    GATEWAY_APP_ACTIVITY_UNKNOWN = 0,
    GATEWAY_APP_INACTIVE,
    GATEWAY_APP_ACTIVE
} Gateway_AppActivityState_t;


/* -------------------------------------------------------------------------- */
/* Proximity                                                                  */
/* -------------------------------------------------------------------------- */

/*
 * ESP32가 Domain에 제공하는 근접 의미.
 *
 * 현재 RSSI 기반 판단 방법은 PROVISIONAL이다.
 * Disconnect / STALE / INVALID / 미수신을 FAR로 치환해서는 안 된다.
 */
typedef enum
{
    GATEWAY_PROXIMITY_UNKNOWN = 0,
    GATEWAY_PROXIMITY_FAR,
    GATEWAY_PROXIMITY_NEAR
} Gateway_ProximityState_t;


/* -------------------------------------------------------------------------- */
/* Data quality / update basis                                                */
/* -------------------------------------------------------------------------- */

/*
 * Logical value 자체의 사용 가능성을 나타낸다.
 *
 * Freshness의 최종 사용 판단은 Domain이 Update 근거와 사용 시점을 이용해
 * 수행하는 것을 기본 경계로 한다.
 */
typedef enum
{
    GATEWAY_DATA_QUALITY_UNKNOWN = 0,
    GATEWAY_DATA_QUALITY_VALID,
    GATEWAY_DATA_QUALITY_INVALID,
    GATEWAY_DATA_QUALITY_NO_DATA
} Gateway_DataQuality_t;

/*
 * ESP32가 알고 있는 갱신 근거.
 *
 * age_ms:
 *   가장 최근의 실제 관측/평가 시점부터 현재까지의 Gateway-local 경과 시간.
 *
 * is_new_update:
 *   동일한 값이더라도 실제 새 관측/평가 결과인지 구분하기 위한 내부 근거.
 *
 * 주의:
 *   이 구조체를 UART Wire Format으로 그대로 전송한다는 의미가 아니다.
 *   실제 전송 필드와 폭은 NETWORK-TBD다.
 */
typedef struct
{
    Gateway_DataQuality_t quality;
    uint32_t age_ms;
    bool is_new_update;
} Gateway_UpdateBasis_t;


/* -------------------------------------------------------------------------- */
/* Bluetooth-side logical state                                               */
/* -------------------------------------------------------------------------- */

typedef struct
{
    Gateway_RegistrationState_t state;
    Gateway_UpdateBasis_t update;
} Gateway_RegistrationInfo_t;

typedef struct
{
    Gateway_ConnectionState_t state;
    Gateway_UpdateBasis_t update;
} Gateway_ConnectionInfo_t;

typedef struct
{
    Gateway_AppActivityState_t state;
    Gateway_UpdateBasis_t update;
} Gateway_AppActivityInfo_t;

typedef struct
{
    Gateway_RegistrationInfo_t registration;
    Gateway_ConnectionInfo_t connection;
    Gateway_AppActivityInfo_t app_activity;
} Gateway_BluetoothContext_t;

typedef struct
{
    Gateway_ProximityState_t state;
    Gateway_UpdateBasis_t update;
} Gateway_ProximityInfo_t;


/* -------------------------------------------------------------------------- */
/* Logical message classification                                             */
/* -------------------------------------------------------------------------- */

/*
 * 아래 값은 "논리 정보 분류"다.
 * Bluetooth Opcode 또는 UART Message ID를 확정하는 enum이 아니다.
 */
typedef enum
{
    GATEWAY_LOGICAL_MSG_UNKNOWN = 0,

    /* MOBILE -> Domain relay */
    GATEWAY_LOGICAL_MSG_MOBILE_REQUEST,
    GATEWAY_LOGICAL_MSG_STATE_QUERY,
    GATEWAY_LOGICAL_MSG_WARNING_ACK,

    /* ESP32 -> Domain generated/context information */
    GATEWAY_LOGICAL_MSG_REGISTRATION_CONNECTION,
    GATEWAY_LOGICAL_MSG_APP_ACTIVITY,
    GATEWAY_LOGICAL_MSG_PROXIMITY,

    /* Domain -> MOBILE relay */
    GATEWAY_LOGICAL_MSG_REQUEST_RESULT,
    GATEWAY_LOGICAL_MSG_VEHICLE_STATE,
    GATEWAY_LOGICAL_MSG_WARNING,
    GATEWAY_LOGICAL_MSG_FUNCTION_AVAILABILITY,
    GATEWAY_LOGICAL_MSG_DIGITAL_KEY_STATE_RESULT
} Gateway_LogicalMessageType_t;

typedef enum
{
    GATEWAY_DIRECTION_UNKNOWN = 0,
    GATEWAY_DIRECTION_MOBILE_TO_DOMAIN,
    GATEWAY_DIRECTION_ESP32_TO_DOMAIN,
    GATEWAY_DIRECTION_DOMAIN_TO_MOBILE
} Gateway_MessageDirection_t;


/* -------------------------------------------------------------------------- */
/* Important semantic boundaries                                              */
/* -------------------------------------------------------------------------- */

/*
 * 이 파일에는 의도적으로 다음 차량 의미 enum을 정의하지 않는다.
 *
 * - ACCEPTED / IN_PROGRESS / DONE / REJECTED / CANCELLED / FAILED / UNKNOWN
 * - Door / Climate / Window command
 * - Warning severity / vehicle warning state
 * - Function Availability의 차량 정책 의미
 *
 * 위 의미의 최종 Owner는 S32K344 Domain 또는 각 차량 기능 ECU다.
 * ESP32는 해당 정보를 변경하지 않고 중계한다.
 */

#endif /* GATEWAY_TYPES_H */
