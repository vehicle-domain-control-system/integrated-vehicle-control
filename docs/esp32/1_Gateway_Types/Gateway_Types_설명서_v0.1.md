# Gateway_Types.h 설계 설명서 v0.1

> 대상 코드: `ESP32/common/Gateway_Types.h`  
> 기준 문서: `ESP32 Wireless Gateway Software Architecture v0.3`  
> 목적: ESP32 Wireless Gateway 전체 모듈이 공통으로 사용할 **논리 타입과 책임 경계**를 정의한다.

---

# 1. 문서 목적

`Gateway_Types.h`는 ESP32 프로젝트의 여러 모듈이 서로 같은 의미의 타입을 사용하도록 만드는 공통 헤더다.

예를 들어 다음 모듈들이 모두 같은 `NEAR / FAR / UNKNOWN` 정의를 사용해야 한다.

```text
BluetoothAdapter
        ↓
DeviceRegistrationManager
        ↓
ProximityManager
        ↓
Gateway_Interface
        ↓
GatewayRouter
        ↓
UART Adapter
        ↓
S32K344 Domain
```

따라서 각 `.c` 파일에서 임의로 enum이나 숫자를 새로 정의하지 않고,

```c
Gateway_ProximityState_t
Gateway_LinkState_t
Gateway_DataQuality_t
```

같은 공통 타입을 사용하도록 한다.

이 파일의 핵심 목적은 다음 두 가지다.

```text
1. ESP32 내부에서 동일한 의미를 동일한 타입으로 사용
2. ESP32가 판단하면 안 되는 차량 정책까지 공통 타입에 넣지 않음
```

---

# 2. 설계 원칙

## 2.1 차량 정책과 Gateway 상태를 분리한다

ESP32의 역할은 다음과 같다.

```text
MOBILE ↔ ESP32 Bluetooth 통신
ESP32 ↔ Domain UART 통신

등록된 단말 확인
현재 Bluetooth 연결 확인
근접 정보 제공
요청 / 상태 / 결과 / 경고 중계
```

반대로 다음 판단은 ESP32 책임이 아니다.

```text
차량 Request 수용 여부
Door Unlock 최종 판단
차량 기능 Availability 판단
Request DONE / FAILED 판단
Warning 발생 판단
BCM / WINDOW / VSS 직접 제어
```

따라서 `Gateway_Types.h`에는 차량 정책 enum을 새로 만들지 않는다.

예:

```text
ACCEPTED
DONE
FAILED
REJECTED
```

이러한 값은 ESP32가 생성하는 상태가 아니라 Domain에서 받은 의미를 그대로 중계해야 한다.

---

# 3. `Gateway_Status_t`

```c
typedef enum
{
    GATEWAY_STATUS_OK = 0,
    GATEWAY_STATUS_INVALID_ARGUMENT,
    GATEWAY_STATUS_NOT_READY,
    GATEWAY_STATUS_UNSUPPORTED,
    GATEWAY_STATUS_INTERNAL_ERROR
} Gateway_Status_t;
```

이 타입은 **ESP32 내부 함수의 수행 결과**를 나타낸다.

예:

```c
Gateway_Status_t GatewayRouter_Forward(...);
```

가능한 의미:

```text
OK
→ 함수가 정상 처리됨

INVALID_ARGUMENT
→ 잘못된 포인터 또는 잘못된 입력

NOT_READY
→ 현재 Gateway 내부 상태상 처리 불가

UNSUPPORTED
→ 아직 지원하지 않는 기능

INTERNAL_ERROR
→ 내부 처리 오류
```

중요:

```text
GATEWAY_STATUS_INTERNAL_ERROR
≠
차량 Request FAILED
```

즉 이 enum은 프로그램 내부 함수의 성공/실패를 표현할 뿐,
차량 제어 요청의 최종 결과를 표현하지 않는다.

---

# 4. Communication Link 타입

## 4.1 `Gateway_LinkId_t`

```c
typedef enum
{
    GATEWAY_LINK_BLUETOOTH = 0,
    GATEWAY_LINK_DOMAIN_UART
} Gateway_LinkId_t;
```

ESP32에는 현재 핵심 통신 경로가 두 개 있다.

```text
MOBILE
  │
Bluetooth
  │
ESP32
  │
UART
  │
Domain
```

따라서 동일한 Link 관리 로직에서 어느 통신 경로인지 구분할 때 사용한다.

예:

```c
LinkStateManager_GetState(GATEWAY_LINK_BLUETOOTH);
LinkStateManager_GetState(GATEWAY_LINK_DOMAIN_UART);
```

---

## 4.2 `Gateway_LinkState_t`

```c
typedef enum
{
    GATEWAY_LINK_STATE_UNKNOWN = 0,
    GATEWAY_LINK_STATE_UNAVAILABLE,
    GATEWAY_LINK_STATE_AVAILABLE,
    GATEWAY_LINK_STATE_RECOVERING
} Gateway_LinkState_t;
```

### UNKNOWN

아직 현재 상태를 판단할 근거가 없는 상태다.

예:

```text
부팅 직후
UART 통신 확인 전
Bluetooth Stack 초기화 직후
```

### UNAVAILABLE

현재 통신 경로를 사용할 수 없는 상태다.

예:

```text
Bluetooth 연결 해제
Domain UART 통신 확인 실패
UART Timeout
```

### AVAILABLE

현재 통신 경로를 사용할 수 있다고 확인된 상태다.

특히 UART의 경우:

```text
UART Driver Init 성공
=
UART AVAILABLE
```

로 바로 처리하면 안 된다.

실제로 Domain과 메시지를 교환할 수 있는지 확인되어야 한다.

그 확인 방법은 아직 `NETWORK-TBD`다.

### RECOVERING

통신 경로가 끊긴 뒤 다시 복구 중인 상태를 표현하기 위한 내부 상태다.

예:

```text
UART Path Loss
      ↓
RECOVERING
      ↓
Domain 통신 확인
      ↓
AVAILABLE
```

---

# 5. Bluetooth Device Context

## 5.1 `Gateway_RegistrationState_t`

```c
typedef enum
{
    GATEWAY_REGISTRATION_UNKNOWN = 0,
    GATEWAY_NOT_REGISTERED,
    GATEWAY_REGISTERED
} Gateway_RegistrationState_t;
```

등록된 단말인지 표현한다.

```text
UNKNOWN
→ 아직 등록 여부를 확인하지 못함

NOT_REGISTERED
→ 현재 단말이 차량에 등록되지 않음

REGISTERED
→ Bluetooth 연결 계층 기준 등록된 단말
```

중요:

```text
REGISTERED
≠
현재 연결됨
```

등록된 단말이라도 현재 연결되지 않을 수 있다.

---

## 5.2 `Gateway_ConnectionState_t`

```c
typedef enum
{
    GATEWAY_CONNECTION_UNKNOWN = 0,
    GATEWAY_DISCONNECTED,
    GATEWAY_CONNECTED
} Gateway_ConnectionState_t;
```

현재 Bluetooth 연결 여부를 나타낸다.

등록 상태와 연결 상태는 별개로 관리한다.

예:

```text
REGISTERED + DISCONNECTED
REGISTERED + CONNECTED
NOT_REGISTERED + DISCONNECTED
```

이 구분이 필요한 이유는 Digital Key 판단에서

```text
등록된 단말인지
+
현재 실제로 연결되어 있는지
```

를 각각 확인해야 하기 때문이다.

---

# 6. App Activity

```c
typedef enum
{
    GATEWAY_APP_ACTIVITY_UNKNOWN = 0,
    GATEWAY_APP_INACTIVE,
    GATEWAY_APP_ACTIVE
} Gateway_AppActivityState_t;
```

현재 기본 Digital Key 시연에서는 앱 활성 조건이 필요하다.

하지만 아직 다음 방식은 확정되지 않았다.

```text
MOBILE이 직접 APP_ACTIVE 메시지를 보내는가?
Bluetooth Profile 상태로 확인 가능한가?
별도 Heartbeat를 사용할 것인가?
갱신 주기는 얼마인가?
```

따라서 이 enum은 **필요한 논리 의미만 먼저 정의한 상태**다.

가장 중요한 규칙:

```text
Bluetooth CONNECTED
≠
APP ACTIVE
```

그리고

```text
Bluetooth DISCONNECTED
≠
반드시 APP INACTIVE
```

연결 정보만으로 App Activity 상태를 임의 생성하지 않는다.

---

# 7. Proximity

## 7.1 `Gateway_ProximityState_t`

```c
typedef enum
{
    GATEWAY_PROXIMITY_UNKNOWN = 0,
    GATEWAY_PROXIMITY_FAR,
    GATEWAY_PROXIMITY_NEAR
} Gateway_ProximityState_t;
```

ESP32가 Domain에 제공할 근접 상태다.

```text
NEAR
→ 현재 근접 조건 충족

FAR
→ 현재 비근접 조건 충족

UNKNOWN
→ 신뢰 가능한 근접 판단 불가
```

중요한 경계:

```text
Bluetooth Disconnect
→ UNKNOWN

RSSI 미수신
→ UNKNOWN 또는 품질 저하

RSSI Invalid
→ UNKNOWN

오래된 근접 정보
→ UNKNOWN / STALE 처리
```

위 상태를 단순히 `FAR`로 만들어서는 안 된다.

---

## 7.2 RSSI와 Proximity의 관계

현재 Architecture에서는 Bluetooth RSSI를 사용하는 방법을 적용하고 있지만,
이 방식은 `PROVISIONAL`이다.

현재 예상 흐름:

```text
Bluetooth RSSI
      ↓
Validity Check
      ↓
Filter
      ↓
Hysteresis
      ↓
NEAR / FAR / UNKNOWN
```

아직 결정해야 하는 값:

```text
NEAR Threshold
FAR Threshold
Hysteresis
Sampling Period
Filter Window
Stable Sample Count
Timeout / Expiry
```

따라서 이 값들은 `Gateway_Types.h`에 하드코딩하지 않고 이후

```text
Gateway_PolicyConfig.h
Gateway_PolicyConfig.c
```

에서 관리한다.

---

# 8. Data Quality

## 8.1 `Gateway_DataQuality_t`

```c
typedef enum
{
    GATEWAY_DATA_QUALITY_UNKNOWN = 0,
    GATEWAY_DATA_QUALITY_VALID,
    GATEWAY_DATA_QUALITY_INVALID,
    GATEWAY_DATA_QUALITY_NO_DATA,
    GATEWAY_DATA_QUALITY_STALE
} Gateway_DataQuality_t;
```

### UNKNOWN

품질 판단 자체가 아직 되지 않은 상태.

### VALID

현재 Gateway 관점에서 사용할 수 있는 정보.

### INVALID

값을 받았지만 형식 또는 측정 결과가 유효하지 않음.

### NO_DATA

현재 사용할 입력 자체가 없음.

### STALE

Gateway가 관리하는 로컬 갱신 기준에서 오래된 정보임을 나타낼 수 있다.

주의:

```text
Gateway의 STALE
≠
Domain의 최종 Vehicle Freshness 판단
```

ESP32는 자신의 입력이 오래되었음을 판단할 수 있지만,
차량 전체 정책에서 이 데이터를 사용할 수 있는지는 Domain이 판단한다.

---

# 9. Update Basis

```c
typedef struct
{
    Gateway_DataQuality_t quality;
    uint32_t age_ms;
    bool is_new_update;
} Gateway_UpdateBasis_t;
```

단순히 값만 전달하면 다음 문제가 생긴다.

예:

```text
12:00:00 RSSI 측정 → NEAR
12:00:05 같은 NEAR 값을 다시 전송
```

두 번째 전송이 실제 새 측정인지,
5초 전 값을 그냥 다시 보낸 것인지 구분할 수 없다.

따라서 다음 정보를 같이 관리한다.

---

## 9.1 `quality`

현재 값 자체의 품질.

```c
Gateway_DataQuality_t quality;
```

---

## 9.2 `age_ms`

마지막 실제 관측 또는 평가 이후 경과 시간.

```c
uint32_t age_ms;
```

예:

```text
RSSI 측정 시각 = 1000 ms
현재 시각      = 1300 ms

age_ms = 300
```

주의:

이 값은 현재 **ESP32 내부 논리 타입**이다.

UART에서 반드시 `uint32_t` 4 byte로 보내겠다는 의미는 아니다.

실제 Wire 표현은 네트워크 설계에서 정한다.

---

## 9.3 `is_new_update`

```c
bool is_new_update;
```

같은 값이라도 실제 새 관측인지 구분하기 위한 내부 근거다.

예:

```text
첫 RSSI 측정  : -60 dBm → NEAR
두 번째 측정 : -61 dBm → NEAR
```

논리 값은 둘 다 `NEAR`지만 두 번째는 새로운 측정이다.

```text
NEAR 재전송
```

과

```text
새 측정 결과가 다시 NEAR
```

를 구분할 수 있어야 한다.

---

# 10. Bluetooth Context

```c
typedef struct
{
    Gateway_RegistrationState_t registration;
    Gateway_ConnectionState_t connection;
    Gateway_AppActivityState_t app_activity;
} Gateway_BluetoothContext_t;
```

Bluetooth Digital Key 관련 Context를 묶기 위한 구조체다.

예:

```c
Gateway_BluetoothContext_t context;

context.registration = GATEWAY_REGISTERED;
context.connection   = GATEWAY_CONNECTED;
context.app_activity = GATEWAY_APP_ACTIVE;
```

이 구조체 자체가 Unlock 허용 여부를 뜻하지 않는다.

예:

```text
REGISTERED
CONNECTED
APP_ACTIVE
NEAR
```

라고 해도 ESP32는 다음 판단을 하지 않는다.

```text
"그러므로 문을 열자."
```

Domain이 추가로 다음을 확인한다.

```text
자동 Unlock 설정
새 접근 여부
현재 Door 상태
Permission
기능 Availability
```

그 후 BCM Unlock Command를 생성한다.

---

# 11. Proximity Info

```c
typedef struct
{
    Gateway_ProximityState_t state;
    Gateway_UpdateBasis_t update;
} Gateway_ProximityInfo_t;
```

근접 상태와 품질/갱신 근거를 하나로 묶는다.

예:

```c
Gateway_ProximityInfo_t proximity = {
    .state = GATEWAY_PROXIMITY_NEAR,
    .update = {
        .quality = GATEWAY_DATA_QUALITY_VALID,
        .age_ms = 30U,
        .is_new_update = true
    }
};
```

이렇게 하면 Domain에 단순히

```text
NEAR
```

만 보내는 것보다 의미가 명확해진다.

---

# 12. Logical Message Type

```c
typedef enum
{
    GATEWAY_LOGICAL_MSG_UNKNOWN = 0,

    GATEWAY_LOGICAL_MSG_MOBILE_REQUEST,
    GATEWAY_LOGICAL_MSG_STATE_QUERY,
    GATEWAY_LOGICAL_MSG_WARNING_ACK,

    GATEWAY_LOGICAL_MSG_REGISTRATION_CONNECTION,
    GATEWAY_LOGICAL_MSG_APP_ACTIVITY,
    GATEWAY_LOGICAL_MSG_PROXIMITY,

    GATEWAY_LOGICAL_MSG_REQUEST_RESULT,
    GATEWAY_LOGICAL_MSG_VEHICLE_STATE,
    GATEWAY_LOGICAL_MSG_WARNING,
    GATEWAY_LOGICAL_MSG_FUNCTION_AVAILABILITY,
    GATEWAY_LOGICAL_MSG_DIGITAL_KEY_STATE_RESULT
} Gateway_LogicalMessageType_t;
```

이 enum에서 가장 중요한 것은 이것이 **UART Message ID가 아니라는 것**이다.

예:

```text
GATEWAY_LOGICAL_MSG_MOBILE_REQUEST = 1
```

이라고 해서 실제 UART에서

```text
Message ID = 0x01
```

로 사용한다는 의미가 아니다.

현재는 Architecture의 논리 분류만 표현한다.

실제 UART ID는 이후 네트워크 설계에서 별도로 Mapping한다.

예:

```text
Logical Message Type
        ↓
Network Mapping
        ↓
UART Message Type / ID
```

---

# 13. Message Direction

```c
typedef enum
{
    GATEWAY_DIRECTION_UNKNOWN = 0,
    GATEWAY_DIRECTION_MOBILE_TO_DOMAIN,
    GATEWAY_DIRECTION_ESP32_TO_DOMAIN,
    GATEWAY_DIRECTION_DOMAIN_TO_MOBILE
} Gateway_MessageDirection_t;
```

Gateway Router가 메시지의 흐름을 구분할 때 사용한다.

### MOBILE → DOMAIN

예:

```text
Door Unlock Request
Climate Setting Request
State Query
Warning Ack
```

### ESP32 → DOMAIN

ESP32 자체가 생산하는 Gateway Context 정보.

예:

```text
Registration / Connection
App Active Context
Proximity
```

### DOMAIN → MOBILE

예:

```text
Request Result
Vehicle State
Warning
Function Availability
Digital Key Result
```

---

# 14. 일부 타입을 일부러 만들지 않은 이유

현재 `Gateway_Types.h`에는 다음 타입이 없다.

```text
DeviceContextId
SessionId
RequestId
VehicleId
```

이유는 아직 다음 항목이 확정되지 않았기 때문이다.

```text
누가 생성하는가?
몇 bit인가?
숫자인가 문자열인가?
수명은 얼마인가?
재연결 시 변경되는가?
UART에서 어떻게 표현하는가?
```

예를 들어 지금 임의로

```c
typedef uint32_t Gateway_SessionId_t;
```

라고 선언하면 나중에 실제 프로토콜에서

```text
16 bit
64 bit
UUID
Composite ID
```

중 하나로 결정될 때 공통 Architecture까지 다시 수정해야 한다.

따라서 현재는 **의미가 확정된 타입부터 구현한다.**

---

# 15. Wire Format과 Logical Type의 분리

전체 구조는 다음과 같이 유지한다.

```text
          Logical Layer
               │
               │ Gateway_Types
               │ Gateway_Interface
               ▼
        GatewayRouter
               │
               ▼
        UartFrameCodec
               │
               ▼
          UartAdapter
               │
               ▼
            UART
```

예를 들어:

```text
GATEWAY_PROXIMITY_NEAR
```

라는 내부 의미가 있다고 해서

```text
UART Payload[2] = 0x02
```

라고 여기서 정하지 않는다.

실제 Byte Mapping은:

```text
Network_Config
UartFrameCodec
```

에서 담당한다.

이렇게 하면 UART Protocol이 변경되어도

```text
ProximityManager
GatewayRouter
DeviceRegistrationManager
```

를 수정하지 않을 수 있다.

---

# 16. 사용 예시

## 16.1 Bluetooth Context 초기화

```c
Gateway_BluetoothContext_t bt_context = {
    .registration = GATEWAY_REGISTRATION_UNKNOWN,
    .connection = GATEWAY_CONNECTION_UNKNOWN,
    .app_activity = GATEWAY_APP_ACTIVITY_UNKNOWN
};
```

부팅 직후에는 모르는 상태를 명확하게 표현한다.

---

## 16.2 Bluetooth 연결 확인

```c
bt_context.registration = GATEWAY_REGISTERED;
bt_context.connection = GATEWAY_CONNECTED;
```

이 상태만으로 다음을 만들면 안 된다.

```c
bt_context.app_activity = GATEWAY_APP_ACTIVE;
```

App Active는 별도 근거가 필요하다.

---

## 16.3 Proximity 생성

```c
Gateway_ProximityInfo_t proximity = {
    .state = GATEWAY_PROXIMITY_NEAR,
    .update = {
        .quality = GATEWAY_DATA_QUALITY_VALID,
        .age_ms = 0U,
        .is_new_update = true
    }
};
```

---

## 16.4 Bluetooth Disconnect

권장 의미:

```c
bt_context.connection = GATEWAY_DISCONNECTED;

proximity.state = GATEWAY_PROXIMITY_UNKNOWN;
proximity.update.quality = GATEWAY_DATA_QUALITY_NO_DATA;
```

잘못된 처리:

```c
proximity.state = GATEWAY_PROXIMITY_FAR;
```

연결 상실은 실제 이탈을 확인한 것이 아니기 때문이다.

---

# 17. 이후 파일과의 관계

`Gateway_Types.h`는 앞으로 다음 구조의 가장 아래 공통 기반이 된다.

```text
Gateway_Types.h
      │
      ├── Gateway_Time
      │
      ├── Gateway_Interface
      │
      ├── LinkStateManager
      │
      ├── MessageContextManager
      │
      ├── GatewayRouter
      │
      ├── DeviceRegistrationManager
      │
      └── ProximityManager
```

Manager끼리 서로 다른 enum을 만들지 않고 모두 이 파일을 사용한다.

---

# 18. 다음 구현 단계

권장 순서:

```text
1. Gateway_Types.h                ✅ 완료
2. Gateway_Time.h/.c
3. Gateway_Interface.h/.c
4. LinkStateManager.h/.c
5. MessageContextManager.h/.c
6. GatewayRouter.h/.c
7. GatewayLifecycleManager.h/.c
8. DeviceRegistrationManager.h/.c
9. ProximityManager.h/.c
```

다음 단계의 `Gateway_Time`은 아주 작게 유지한다.

예상 Interface:

```c
uint32_t Gateway_Time_GetMs(void);
```

목적:

```text
ESP-IDF Tick API를 Manager가 직접 호출하지 않게 함
시간 계산을 한곳으로 모음
Unit Test에서 가짜 시간을 주입하기 쉽게 함
```

그 다음 `Gateway_Interface.h`에서는 이번에 정의한 타입을 이용해

```text
Bluetooth → Gateway
Gateway → Domain
Domain → Gateway
Gateway → MOBILE
```

의 논리적 경계를 정의한다.

---

# 19. 핵심 정리

`Gateway_Types.h`의 역할을 한 문장으로 정리하면:

> **ESP32가 알아야 할 공통 논리 상태만 정의하고, 차량 정책과 실제 Wire Format은 정의하지 않는 파일이다.**

현재 가장 중요한 경계는 다음과 같다.

```text
Registration     → ESP32
Connection       → ESP32
Proximity        → ESP32

Request Relay    → ESP32
State Relay      → ESP32
Result Relay     → ESP32

Request Decision → Domain
Auto Unlock      → Domain
Vehicle Result   → Domain
Warning Decision → Domain

Bluetooth Format → 후속 설계
UART Format      → 후속 설계
```

이 경계를 유지하면 이후 Bluetooth/UART Protocol이 변경되더라도
ESP32 Core Architecture를 크게 수정하지 않고 확장할 수 있다.
