# Gateway_Interface 설계 설명서 v0.1

> 대상 코드  
> - `ESP32/interface/Gateway_Interface.h`  
> - `ESP32/interface/Gateway_Interface.c`
>
> 관련 테스트  
> - `ESP32/test/test_gateway_interface.c`
>
> 기준 문서  
> - `ESP32 Wireless Gateway Software Architecture v0.3`
> - `Gateway_Types.h 설계 설명서 v0.1`
> - `Gateway_Time 설계 설명서 v0.1`
> - SR / SysRS의 ESP32 Gateway 책임
>
> 상태  
> - Logical Interface: `DESIGN`
> - Bluetooth 상세 Protocol: `NETWORK-TBD`
> - UART 상세 Protocol: `NETWORK-TBD`
> - Vehicle / Device / Session / Request ID 실제 표현: `TBD`

---

# 1. 목적

`Gateway_Interface`는 ESP32 Wireless Gateway에서

```text
Communication / Transport
        ↕
     Core Logic
```

사이의 **논리 경계**를 정의한다.

현재 시스템 흐름은 다음과 같다.

```text
                  MOBILE
                    │
                Bluetooth
                    │
                    ▼
        ┌──────────────────────┐
        │ BluetoothAdapter     │
        │ BluetoothProfile     │
        └──────────┬───────────┘
                   │
                   ▼
        ┌──────────────────────┐
        │ Gateway_Interface    │
        └──────────┬───────────┘
                   │
                   ▼
        ┌──────────────────────┐
        │ Gateway Core         │
        │ GatewayRouter        │
        │ LinkStateManager     │
        │ MessageContextMgr    │
        └──────────┬───────────┘
                   │
                   ▼
        ┌──────────────────────┐
        │ Gateway_Interface    │
        └──────────┬───────────┘
                   │
                   ▼
        ┌──────────────────────┐
        │ UartFrameCodec       │
        │ UartAdapter          │
        └──────────┬───────────┘
                   │
                  UART
                   │
                   ▼
              S32K344 Domain
```

핵심 목적:

```text
Bluetooth Code
≠
Core Logic

UART Code
≠
Core Logic
```

이 되도록 분리하는 것이다.

---

# 2. 이 단계에서 결정하는 것

이번 단계에서는 다음을 결정한다.

```text
MOBILE에서 들어온 논리 메시지를 Core로 어떻게 넘길 것인가?
Domain에서 들어온 논리 메시지를 Core로 어떻게 넘길 것인가?

Core에서 Domain으로 어떻게 요청할 것인가?
Core에서 MOBILE로 어떻게 요청할 것인가?

ESP32 자체가 생성하는
Registration / Connection / App Active / Proximity 정보를
어떤 Interface로 Domain에 넘길 것인가?
```

---

# 3. 이 단계에서 결정하지 않는 것

중요하게도 아직 다음은 결정하지 않는다.

```text
BLE GATT UUID
Bluetooth Characteristic
Bluetooth Packet Format

UART Port
Baud Rate
UART Header
UART Message ID
Length Field
CRC
Sequence
Byte Allocation

VehicleId 크기
DeviceContextId 크기
SessionId 크기
RequestId 크기
```

즉:

```text
Logical Interface
```

와

```text
Wire Interface
```

를 분리한다.

---

# 4. 왜 Logical Interface를 먼저 만드는가?

잘못된 접근:

```text
UART Payload[0] = RequestId
UART Payload[1] = Command
UART Payload[2] = Value
```

부터 만들면 네트워크 담당자가 Frame 구조를 바꿀 때 Core까지 같이 수정해야 한다.

권장 구조:

```text
Logical Request
      ↓
Gateway_Interface
      ↓
GatewayRouter
      ↓
UartFrameCodec
      ↓
UART Frame
```

UART 구조가 바뀌면:

```text
UartFrameCodec
Network_Config
```

만 변경하면 된다.

Core는 가능한 그대로 유지한다.

---

# 5. 전체 Interface 방향

현재 인터페이스는 크게 5가지다.

```text
① MOBILE → ESP32 Core
② Domain → ESP32 Core

③ ESP32 Core → Domain
④ ESP32 Core → MOBILE

⑤ ESP32 자체 Context → Domain
```

---

# 6. MOBILE → Core

Bluetooth 계층에서 유효한 논리 메시지를 획득하면:

```c
Gateway_Interface_OnMobileMessage(...)
```

를 호출한다.

흐름:

```text
Bluetooth Rx
   ↓
Bluetooth Frame Decode
   ↓
Logical Message 생성
   ↓
Gateway_Interface_OnMobileMessage()
   ↓
GatewayRouter / Core Handler
```

현재 허용되는 논리 분류:

```text
MOBILE_REQUEST
STATE_QUERY
WARNING_ACK
```

---

# 7. Domain → Core

UART에서 유효한 Domain 메시지를 획득하면:

```c
Gateway_Interface_OnDomainMessage(...)
```

를 호출한다.

흐름:

```text
UART Bytes
   ↓
UartFrameCodec
   ↓
Frame Validity 확인
   ↓
Logical Message 생성
   ↓
Gateway_Interface_OnDomainMessage()
   ↓
GatewayRouter / Core Handler
```

허용되는 논리 분류:

```text
REQUEST_RESULT
VEHICLE_STATE
WARNING
FUNCTION_AVAILABILITY
DIGITAL_KEY_STATE_RESULT
```

---

# 8. Core → Domain

Core에서 MOBILE 메시지를 Domain으로 중계할 때:

```c
Gateway_Interface_SendToDomain(...)
```

을 사용한다.

예:

```text
MOBILE
  │
Door Unlock Request
  ▼
BluetoothAdapter
  ▼
Gateway_Interface_OnMobileMessage
  ▼
GatewayRouter
  ▼
Gateway_Interface_SendToDomain
  ▼
UartFrameCodec
  ▼
Domain
```

---

# 9. Core → MOBILE

Domain의 상태나 결과를 MOBILE로 전달할 때:

```c
Gateway_Interface_SendToMobile(...)
```

을 사용한다.

예:

```text
Domain
  │
Request Result
  ▼
UartFrameCodec
  ▼
Gateway_Interface_OnDomainMessage
  ▼
GatewayRouter
  ▼
Gateway_Interface_SendToMobile
  ▼
BluetoothAdapter
  ▼
MOBILE
```

---

# 10. ESP32가 직접 만드는 정보

ESP32는 단순 중계만 하는 것은 아니다.

다음 정보는 ESP32가 차량 측에서 생성하거나 확인하여 Domain에 제공한다.

```text
Registration
Current Bluetooth Connection
App Activity Context
Proximity
```

따라서 Generic Relay와 분리해 typed API를 둔다.

```c
Gateway_Interface_PublishRegistrationConnection(...)
Gateway_Interface_PublishAppActivity(...)
Gateway_Interface_PublishProximity(...)
```

---

# 11. `Gateway_ByteView_t`

```c
typedef struct
{
    const uint8_t *data;
    size_t length;
} Gateway_ByteView_t;
```

아직 실제 표현 방식이 확정되지 않은 정보를 참조하기 위해 사용한다.

대표적으로:

```text
Vehicle ID
Device Context ID
Session ID
Request ID
Logical Payload
```

이다.

---

# 12. 왜 `uint32_t RequestId`로 만들지 않았는가?

현재는 아직 다음이 정해지지 않았다.

```text
RequestId = uint16_t?
RequestId = uint32_t?
RequestId = uint64_t?
UUID?
문자열?
```

지금 임의로:

```c
typedef uint32_t Gateway_RequestId_t;
```

를 만들면 Architecture가 Network 상세 설계를 선점하게 된다.

따라서 현재는:

```c
Gateway_ByteView_t request_id;
```

로 표현한다.

---

# 13. ByteView는 데이터를 소유하지 않는다

중요한 규칙:

```c
Gateway_ByteView_t
```

는 `malloc()`해서 데이터를 보관하는 구조가 아니다.

단순히:

```text
Pointer
+
Length
```

를 갖는 View다.

예:

```c
uint8_t request_id_buffer[4];

Gateway_ByteView_t request_id = {
    .data = request_id_buffer,
    .length = sizeof(request_id_buffer)
};
```

즉 함수 호출 동안 해당 메모리가 유효해야 한다.

---

# 14. ByteView 유효 규칙

허용:

```c
.data = NULL
.length = 0
```

의미:

```text
현재 해당 정보 없음
```

또는:

```c
.data = valid_pointer
.length > 0
```

잘못된 상태:

```c
.data = NULL
.length = 4
```

이다.

이를:

```c
Gateway_Interface_IsValidByteView()
```

에서 확인한다.

---

# 15. `Gateway_MessageContextView_t`

```c
typedef struct
{
    Gateway_ByteView_t vehicle_id;
    Gateway_ByteView_t device_context_id;
    Gateway_ByteView_t session_id;
    Gateway_ByteView_t request_id;
} Gateway_MessageContextView_t;
```

ESP32가 중계 과정에서 유지해야 하는 식별 Context를 모은 구조다.

개념적으로:

```text
Vehicle
 └─ Device
     └─ Session
         └─ Request
```

관계를 유지하기 위한 정보다.

---

# 16. Context와 Request 중복 방지

ESP32의 중요한 책임 중 하나는:

```text
같은 Request를 다시 보낼 때
새 Request로 바꾸지 않는 것
```

이다.

즉:

```text
Request ID = A
```

를 받은 요청을 재전달한다면:

```text
Request ID = B
```

로 새로 만들면 안 된다.

실제 중복 판단은 이후 `MessageContextManager`와 Domain `RequestManager`의 책임을 구분해서 구현한다.

`Gateway_Interface`는 식별자를 **변경하지 않고 통과시킬 수 있는 경계**만 제공한다.

---

# 17. Context의 필수 여부를 아직 강제하지 않는 이유

현재 Validator는:

```text
모든 Message에 RequestId 필수
모든 Message에 SessionId 필수
```

같은 규칙을 아직 강제하지 않는다.

왜냐하면:

```text
STATE_QUERY에 RequestId가 반드시 필요한가?
WARNING_ACK에 어떤 식별자가 필수인가?
Registration Update에서 SessionId가 필요한가?
```

등의 상세 계약이 아직 최종 확정되지 않았기 때문이다.

현재 단계에서는:

```text
있다면 보존한다.
형식적으로 깨진 View는 거부한다.
```

까지만 처리한다.

추후 Interface Matrix가 확정되면 Message Type별 Mandatory Context Validation을 추가한다.

---

# 18. `Gateway_RelayMessageView_t`

```c
typedef struct
{
    Gateway_LogicalMessageType_t type;
    Gateway_MessageDirection_t direction;
    Gateway_MessageContextView_t context;
    Gateway_ByteView_t payload;
} Gateway_RelayMessageView_t;
```

MOBILE ↔ Domain 사이의 일반 중계 정보를 표현한다.

구성:

```text
type
→ 이 정보가 Request인가, State인가, Warning인가?

direction
→ 어느 방향인가?

context
→ Vehicle / Device / Session / Request 연계 정보

payload
→ 실제 논리 내용
```

---

# 19. Payload가 의미하는 것

`payload`는 절대로:

```text
Bluetooth 전체 Packet
```

또는:

```text
UART 전체 Frame
```

이 아니다.

예:

```text
UART Header
Length
CRC
```

는 포함하지 않는다.

개념:

```text
Transport Frame
      ↓ Decode
Logical Payload
      ↓
Gateway_RelayMessageView
```

이다.

---

# 20. Payload를 아직 Struct로 만들지 않은 이유

예를 들어 차량 상태를 지금부터:

```c
typedef struct
{
    uint8_t door;
    uint8_t climate;
    ...
} VehicleState_t;
```

로 만들 수도 있다.

하지만 MOBILE Interface 상세 Field가 아직 확정되지 않았다.

따라서 현재 Generic Relay에서는:

```c
Gateway_ByteView_t payload;
```

를 사용한다.

추후 확정되면:

```text
Generic Payload
      ↓
기능별 DTO
```

로 변경할 수 있다.

예:

```c
Gateway_MobileRequest_t
Gateway_RequestResult_t
Gateway_VehicleState_t
Gateway_Warning_t
```

등이다.

---

# 21. Generic Relay와 ESP32 Context를 분리한 이유

다음 두 종류는 의미가 다르다.

### 단순 중계

```text
MOBILE Request
Domain Vehicle State
Domain Request Result
Domain Warning
```

ESP32가 의미를 만들지 않는다.

### ESP32 Producer

```text
Registration
Current Connection
Proximity
App Activity Context
```

ESP32가 직접 확인하거나 생산하는 정보다.

따라서 같은 Generic Payload로 처리하지 않고 typed update를 둔다.

---

# 22. `Gateway_RegistrationConnectionUpdate_t`

```c
typedef struct
{
    Gateway_MessageContextView_t context;
    Gateway_RegistrationState_t registration;
    Gateway_ConnectionState_t connection;
    Gateway_UpdateBasis_t update;
} Gateway_RegistrationConnectionUpdate_t;
```

Domain에 제공할:

```text
등록 상태
현재 연결 상태
갱신 근거
```

를 묶는다.

중요:

```text
REGISTERED
≠
CONNECTED
```

이므로 두 값을 별도로 둔다.

---

# 23. Registration 예시

```c
Gateway_RegistrationConnectionUpdate_t update = {
    .context = context,
    .registration = GATEWAY_REGISTERED,
    .connection = GATEWAY_CONNECTED,
    .update = {
        .quality = GATEWAY_DATA_QUALITY_VALID,
        .age_ms = 0,
        .is_new_update = true
    }
};
```

전달:

```c
Gateway_Interface_PublishRegistrationConnection(&update);
```

---

# 24. `Gateway_AppActivityUpdate_t`

```c
typedef struct
{
    Gateway_MessageContextView_t context;
    Gateway_AppActivityState_t state;
    Gateway_UpdateBasis_t update;
} Gateway_AppActivityUpdate_t;
```

App Activity는 필요 정보이지만 획득 방법은 아직 `INPUT-TBD`다.

따라서 이 Interface가 존재한다고 해서 ESP32가 Bluetooth 연결만 보고:

```text
CONNECTED → APP_ACTIVE
```

를 생성하는 것이 아니다.

실제 Producer가 확정된 뒤 이 API를 사용한다.

---

# 25. `Gateway_ProximityUpdate_t`

```c
typedef struct
{
    Gateway_MessageContextView_t context;
    Gateway_ProximityInfo_t proximity;
} Gateway_ProximityUpdate_t;
```

포함:

```text
NEAR / FAR / UNKNOWN
Quality
Age
New Update 근거
```

이다.

현재 Proximity의 입력 방법은:

```text
Bluetooth RSSI
```

를 잠정 사용한다.

하지만 Interface 자체는 RSSI 값을 직접 노출하지 않는다.

이유:

```text
RSSI
→ ProximityManager 내부 입력

NEAR/FAR/UNKNOWN
→ Domain과의 논리 Interface
```

로 분리하기 위해서다.

---

# 26. Core Handler

```c
typedef Gateway_Status_t (*Gateway_InterfaceMessageHandler_t)(
    const Gateway_RelayMessageView_t *message,
    void *user_context);
```

Core가 다음 이벤트를 받기 위한 Callback이다.

```text
MOBILE Message 들어옴
Domain Message 들어옴
```

초기화 시:

```c
Gateway_InterfaceCoreHandlers_t
```

에 등록한다.

---

# 27. Transport Port

Transport 쪽 Callback은 다음 방향에 사용한다.

```text
Core
 ↓
Bluetooth / UART Adapter
```

대표적으로:

```c
send_to_domain
send_to_mobile
send_registration_to_domain
send_app_activity_to_domain
send_proximity_to_domain
```

이다.

이 구조는 Dependency Inversion 역할을 한다.

---

# 28. 왜 Adapter 함수를 직접 호출하지 않는가?

예를 들어 Core에서 바로:

```c
UartAdapter_Send(...)
```

를 호출하면:

```text
GatewayRouter
→ UartAdapter 직접 의존
```

하게 된다.

현재 구조:

```text
GatewayRouter
     ↓
Gateway_Interface
     ↓
Transport Port
     ↓
UartAdapter
```

로 만들면 Core가 UART 구현에 직접 의존하지 않는다.

---

# 29. 초기화

```c
Gateway_Interface_Init(
    &core_handlers,
    &transport_ports);
```

두 종류의 binding을 등록한다.

### Core Handlers

```text
Adapter → Core
```

### Transport Ports

```text
Core → Adapter
```

이다.

---

# 30. NULL Callback 정책

모든 Callback이 항상 구현될 필요는 없다.

예를 들어 개발 초기에는 Proximity 송신이 아직 없을 수 있다.

따라서 Callback이 NULL이면:

```c
GATEWAY_STATUS_UNSUPPORTED
```

를 반환한다.

이를 통해:

```text
아직 미구현
```

과

```text
잘못된 입력
```

을 구분한다.

---

# 31. 초기화 전 호출

초기화하지 않고:

```c
Gateway_Interface_OnMobileMessage(...)
```

등을 호출하면:

```c
GATEWAY_STATUS_NOT_READY
```

를 반환한다.

즉:

```text
Boot
 ↓
Interface Init
 ↓
Adapter/Core 연결
 ↓
Message Processing
```

순서를 강제할 수 있다.

---

# 32. Direction Validation

예를 들어 다음 메시지는 잘못된 조합이다.

```text
Type      = WARNING
Direction = MOBILE_TO_DOMAIN
```

현재 Warning은:

```text
Domain → MOBILE
```

방향이므로 Interface에서 거부한다.

반대로:

```text
MOBILE_REQUEST
+
MOBILE_TO_DOMAIN
```

은 허용한다.

---

# 33. 현재 Direction Matrix

| Direction | 허용 Logical Type |
|---|---|
| MOBILE → Domain | `MOBILE_REQUEST` |
| MOBILE → Domain | `STATE_QUERY` |
| MOBILE → Domain | `WARNING_ACK` |
| Domain → MOBILE | `REQUEST_RESULT` |
| Domain → MOBILE | `VEHICLE_STATE` |
| Domain → MOBILE | `WARNING` |
| Domain → MOBILE | `FUNCTION_AVAILABILITY` |
| Domain → MOBILE | `DIGITAL_KEY_STATE_RESULT` |

ESP32 자체 Context:

```text
Registration / Connection
App Activity
Proximity
```

는 Generic Relay가 아니라 typed Publish API를 사용한다.

---

# 34. `ESP32_TO_DOMAIN` Direction은 왜 Generic Relay에서 사용하지 않는가?

`Gateway_Types.h`에는:

```c
GATEWAY_DIRECTION_ESP32_TO_DOMAIN
```

이 존재한다.

하지만 현재 `Gateway_RelayMessageView_t` Validator에서는 허용하지 않는다.

이유:

ESP32가 직접 생성하는 값은:

```text
Registration
Connection
App Activity
Proximity
```

처럼 의미가 명확하다.

이를 opaque payload로 보내기보다:

```c
Gateway_Interface_PublishProximity(...)
```

같은 typed Interface로 전달하는 편이 안전하다.

---

# 35. 차량 결과를 ESP32가 만들지 않는다

이 Interface의 중요한 원칙이다.

UART 경로가 끊겼다고:

```text
FAILED
```

를 생성하지 않는다.

Bluetooth가 끊겼다고:

```text
REJECTED
```

를 생성하지 않는다.

ESP32 내부 오류:

```c
GATEWAY_STATUS_INTERNAL_ERROR
```

도 차량 Request Result가 아니다.

즉:

```text
Gateway_Status_t
```

와

```text
Vehicle Request Result
```

는 완전히 별개다.

---

# 36. Link 상태와 Interface의 관계

이후 `LinkStateManager`가 다음 정보를 관리한다.

```text
Bluetooth Available?
UART Path Available?
```

GatewayRouter는 이를 확인한 뒤:

```text
Domain으로 지금 전달 가능한가?
MOBILE로 지금 전달 가능한가?
```

를 결정할 수 있다.

`Gateway_Interface` 자체는 Link 정책을 판단하지 않는다.

현재 역할은:

```text
입력 검증
방향 검증
적절한 Callback으로 전달
```

이다.

---

# 37. MessageContextManager와의 관계

현재 Interface는 Context를:

```text
View
```

형태로 받는다.

하지만 이후 Request 중복, 지연 메시지, Session 전환 등을 관리하려면 단순 Pointer만으로는 부족하다.

이 역할은 다음 단계 이후의:

```text
MessageContextManager
```

가 담당한다.

구조:

```text
Gateway_Interface
       ↓
MessageContextManager
       ↓
필요 Context 보관 / 비교
```

이다.

즉 Interface는 **경계**, MessageContextManager는 **상태 보관과 비교**다.

---

# 38. GatewayRouter와의 관계

향후 기본 흐름:

```text
Gateway_Interface_OnMobileMessage
        ↓
GatewayRouter_OnMobileMessage
        ↓
MessageContextManager 확인
        ↓
LinkStateManager 확인
        ↓
Gateway_Interface_SendToDomain
```

반대:

```text
Gateway_Interface_OnDomainMessage
        ↓
GatewayRouter_OnDomainMessage
        ↓
Context / Direction 확인
        ↓
Gateway_Interface_SendToMobile
```

이다.

---

# 39. 메모리 소유권

현재 `Gateway_RelayMessageView_t`는 데이터를 소유하지 않는다.

즉 이런 코드는 위험할 수 있다.

```c
Gateway_RelayMessageView_t *saved = message;
```

Callback 종료 후 Adapter의 RX Buffer가 재사용될 수 있기 때문이다.

장기간 보관하려면:

```text
MessageContextManager
또는
GatewayRouter Queue
```

가 필요한 데이터를 **자신의 Storage에 Copy**해야 한다.

---

# 40. 왜 지금 Dynamic Allocation을 쓰지 않는가?

ESP32에서:

```c
malloc()
free()
```

를 Gateway Interface마다 사용하도록 만들지 않았다.

이유:

```text
메모리 파편화
Ownership 복잡도
실시간 동작 예측 어려움
오류 처리 증가
```

때문이다.

현재 Interface는 View를 사용하고,

```text
필요한 모듈이 필요한 데이터만 Copy
```

하는 구조를 선택한다.

---

# 41. Validation Layer

현재 `Gateway_Interface.c`는 다음을 검증한다.

```text
ByteView 유효성
Message Context View 유효성
Message Direction
Direction ↔ Logical Type 관계
Registration enum 유효성
Connection enum 유효성
App Activity enum 유효성
Proximity enum 유효성
Update Quality enum 유효성
```

---

# 42. 아직 Validation하지 않는 것

현재는 다음을 강제하지 않는다.

```text
MOBILE_REQUEST는 RequestId가 반드시 있어야 한다.
STATE_QUERY는 SessionId가 반드시 있어야 한다.
WARNING_ACK는 WarningId가 반드시 있어야 한다.
```

이유는 세부 Interface 계약이 아직 완전히 확정되지 않았기 때문이다.

이 부분은 이후:

```text
Interface Matrix
Network Protocol
MOBILE App Protocol
```

이 확정되면 추가한다.

---

# 43. 단위 테스트

현재 파일:

```text
test_gateway_interface.c
```

에서 다음을 확인한다.

```text
초기화 전 호출 → NOT_READY
MOBILE 정상 Message → Core Callback 호출
Domain 정상 Message → Core Callback 호출
Core → Domain 정상 송신
Core → MOBILE 정상 송신
잘못된 Direction/Type 조합 거부
깨진 ByteView 거부
Registration Publish
App Activity Publish
Proximity Publish
잘못된 Proximity enum 거부
```

---

# 44. 테스트 결과

Host GCC:

```text
-std=c11
-Wall
-Wextra
-Werror
```

조건:

```text
Compile PASS
Unit Test PASS
```

다.

현재 검증 범위는:

```text
C 문법
Warning
Callback 연결
Direction Validation
기본 Context Validation
Typed Update Validation
```

까지다.

---

# 45. 실제 ESP-IDF 연결 시 예상 구조

향후:

```c
Gateway_InterfaceTransportPorts_t ports = {
    .send_to_domain = UartGateway_SendRelay,
    .send_to_mobile = BluetoothGateway_SendRelay,
    .send_registration_to_domain = UartGateway_SendRegistration,
    .send_app_activity_to_domain = UartGateway_SendAppActivity,
    .send_proximity_to_domain = UartGateway_SendProximity
};
```

같은 구조가 될 수 있다.

하지만 함수명은 현재 확정이 아니다.

실제 Adapter 구현 단계에서 정한다.

---

# 46. MOBILE Request 예시

가정:

```text
MOBILE이 Door Unlock Request 생성
```

Bluetooth Adapter가 논리 메시지로 Decode했다고 하자.

```c
Gateway_RelayMessageView_t message = {
    .type = GATEWAY_LOGICAL_MSG_MOBILE_REQUEST,
    .direction = GATEWAY_DIRECTION_MOBILE_TO_DOMAIN,
    .context = context,
    .payload = request_payload
};
```

호출:

```c
Gateway_Interface_OnMobileMessage(&message);
```

이후 Core에서 검증 후:

```c
Gateway_Interface_SendToDomain(&message);
```

한다.

중요:

ESP32는 여기서:

```text
Door Unlock 허용?
```

을 판단하지 않는다.

---

# 47. Domain Result 예시

Domain에서:

```text
Request Result
```

가 UART로 왔다고 하자.

```c
Gateway_RelayMessageView_t result = {
    .type = GATEWAY_LOGICAL_MSG_REQUEST_RESULT,
    .direction = GATEWAY_DIRECTION_DOMAIN_TO_MOBILE,
    .context = context,
    .payload = result_payload
};
```

UART Adapter:

```c
Gateway_Interface_OnDomainMessage(&result);
```

Core 처리 후:

```c
Gateway_Interface_SendToMobile(&result);
```

한다.

ESP32는:

```text
DONE
FAILED
REJECTED
```

의 의미를 재판정하지 않는다.

---

# 48. Proximity 예시

`ProximityManager`가:

```text
NEAR
```

를 생성했다고 하자.

```c
Gateway_ProximityUpdate_t update = {
    .context = context,
    .proximity = {
        .state = GATEWAY_PROXIMITY_NEAR,
        .update = {
            .quality = GATEWAY_DATA_QUALITY_VALID,
            .age_ms = 0U,
            .is_new_update = true
        }
    }
};
```

전송:

```c
Gateway_Interface_PublishProximity(&update);
```

Domain에서는 이 정보를 이용해서:

```text
등록
연결
Auto Unlock 설정
새 접근
Door 상태
Permission
```

등을 함께 확인한다.

최종 Unlock 여부는 Domain이 판단한다.

---

# 49. Bluetooth Disconnect 예시

Bluetooth가 끊겼다고 하자.

ESP32가 해야 할 것:

```text
Connection 상태 갱신
Proximity를 신뢰할 수 없다면 UNKNOWN 처리
Domain에 현재 Context 갱신
```

하면 안 되는 것:

```text
Disconnect → FAR
Disconnect → Door Lock
Disconnect → Request FAILED
```

이다.

---

# 50. UART Path Loss 예시

UART Path가 사용할 수 없다고 판단되면:

```text
LinkStateManager
→ UART Path UNAVAILABLE
```

로 처리한다.

새 MOBILE Request가 들어왔을 때 ESP32가:

```text
FAILED
REJECTED
```

를 만들어 보내면 안 된다.

또한:

```text
UART 복구 후 과거 Request 자동 Replay
```

도 하면 안 된다.

이 정책은 이후 `GatewayRouter + LinkStateManager + MessageContextManager`에서 구현한다.

---

# 51. 현재 코드의 중요한 의도

`Gateway_Interface`는 매우 많은 기능을 가진 Manager가 아니다.

역할은 좁다.

```text
Interface Boundary
Validation
Callback Dispatch
```

다.

즉 이 파일에 다음 로직을 넣지 않는다.

```text
RSSI Filter
Retry
Reconnect
Request Duplicate
Session 판단
Door Permission
Warning 판단
```

---

# 52. 현재 구현 완료 상태

```text
Gateway_Types.h           ✅
Gateway_Time.h/.c         ✅
Gateway_Interface.h/.c    ✅
```

테스트:

```text
test_gateway_time.c       ✅
test_gateway_interface.c  ✅
```

---

# 53. 다음 단계

다음 구현은:

```text
LinkStateManager.h/.c
```

가 적절하다.

현재 Interface가 통신 메시지의 입출구를 만들었으므로,
이제 Core에서:

```text
Bluetooth 경로를 사용할 수 있는가?
UART Domain 경로를 사용할 수 있는가?
마지막 정상 통신은 언제였는가?
경로가 상실됐는가?
복구 중인가?
```

를 관리할 모듈이 필요하다.

예상 구조:

```text
Gateway_Time
     ↓
LinkStateManager
     ↑
BluetoothAdapter / UartAdapter
```

다음 단계에서는:

```text
UNKNOWN
UNAVAILABLE
RECOVERING
AVAILABLE
```

상태 전이와 Timeout 기반 Path 상태를 구현한다.

---

# 54. 핵심 정리

`Gateway_Interface`를 한 문장으로 표현하면:

> **Bluetooth/UART의 실제 프로토콜과 ESP32 Core Logic 사이에서 논리 메시지와 ESP32 Context의 입출구를 정의하는 Boundary Layer다.**

전체 원칙은 다음과 같다.

```text
Wire Format
    ↓
Adapter / Codec
    ↓
Gateway_Interface
    ↓
Core
```

그리고 반대 방향도:

```text
Core
    ↓
Gateway_Interface
    ↓
Adapter / Codec
    ↓
Wire Format
```

으로 유지한다.

이를 통해 앞으로 Bluetooth Profile이나 UART Frame이 변경되더라도
Core Manager의 수정 범위를 최소화할 수 있다.
