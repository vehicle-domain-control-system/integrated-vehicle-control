# GatewayRouter 설계 설명서 v0.3

> 대상 코드  
> - `ESP32/core/GatewayRouter.h`  
> - `ESP32/core/GatewayRouter.c`
>
> 관련 테스트  
> - `ESP32/test/test_gateway_router.c`
>
> 기준 문서  
> - `ESP32 Wireless Gateway Software Architecture v0.3`
> - `Gateway_Interface 설계 설명서 v0.1`
> - `LinkStateManager 설계 설명서 v0.1`
> - `MessageContextManager 설계 설명서 v0.1`
> - SR / SysRS의 ESP32 Gateway 책임

---

# 1. 목적

`GatewayRouter`는 지금까지 만든 ESP32 Core 구성요소를 실제 메시지 흐름으로 연결하는 모듈이다.

현재 핵심 구조:

```text
                         MOBILE
                           │
                       Bluetooth
                           │
                           ▼
                    BluetoothAdapter
                           │
                           ▼
                   Gateway_Interface
                           │
                           ▼
                     GatewayRouter
                    ┌──────┴──────┐
                    │             │
                    ▼             ▼
          MessageContext      LinkState
             Manager           Manager
                    │             │
                    └──────┬──────┘
                           │
                           ▼
                   Gateway_Interface
                           │
                           ▼
                      UartAdapter
                           │
                          UART
                           │
                           ▼
                    S32K344 Domain
```

`GatewayRouter`의 질문은 하나로 정리할 수 있다.

> **이 메시지를 현재 Session과 현재 통신 경로 기준으로 그대로 중계해도 되는가?**

---

# 2. Router의 책임

현재 Router가 하는 일:

```text
MOBILE → Domain 메시지 방향 확인
Domain → MOBILE 메시지 방향 확인

현재 Session Context인지 확인

MOBILE_REQUEST:
Gateway Lifecycle READY 확인

MOBILE → Domain:
UART Path AVAILABLE 확인

Domain → MOBILE:
Bluetooth AVAILABLE 확인

Context ID를 Owned Storage로 복사
같은 Vehicle / Device / Session / Request 식별 유지

Gateway_Interface를 통해 목적지 Transport로 즉시 전달
```

---

# 3. Router가 하지 않는 일

다음은 Router 책임이 아니다.

```text
Bluetooth 기기 등록 판단
새 Session 생성
Session 인증
Request 허용 판단
Request Duplicate 차량 정책
Door Unlock 판단
자동 Unlock 판단
ACCEPTED 생성
DONE 생성
FAILED 생성
REJECTED 생성
Warning 판단
UART Frame Encode
Bluetooth Packet Encode
Reconnect 수행
Retry Count 관리
Backoff 관리
메시지 Queue 보관
복구 후 자동 Replay
```

즉 Router는 차량 정책 Controller가 아니다.

---

# 4. 가장 중요한 설계 원칙

```text
Route
≠
Vehicle Decision
```

예를 들어 MOBILE이 Door Unlock Request를 보냈다고 해서 Router가:

```text
현재 차량 상태 확인
Door Lock 상태 확인
Permission 확인
→ Unlock 허용
```

을 하지 않는다.

Router는:

```text
현재 Session인가?
UART Path를 사용할 수 있는가?
```

만 확인하고 Domain으로 전달한다.

실제 Unlock 판단은 Domain 책임이다.

---

# 5. MOBILE → Domain 기본 흐름

```text
MOBILE Request / Query / Ack
        ↓
BluetoothAdapter
        ↓
Gateway_Interface_OnMobileMessage()
        ↓
GatewayRouter_OnMobileMessage()
        ↓
현재 Session 확인
        ↓
UART Path AVAILABLE?
        ↓
Context 안전 복사
        ↓
Gateway_Interface_SendToDomain()
        ↓
UartAdapter / Codec
        ↓
Domain
```

---

# 6. Domain → MOBILE 기본 흐름

```text
Domain Result / State / Warning / Availability
        ↓
UartAdapter / Codec
        ↓
Gateway_Interface_OnDomainMessage()
        ↓
GatewayRouter_OnDomainMessage()
        ↓
현재 Session 확인
        ↓
Bluetooth AVAILABLE?
        ↓
Context 안전 복사
        ↓
Gateway_Interface_SendToMobile()
        ↓
BluetoothAdapter
        ↓
MOBILE
```

---

# 7. `GatewayRouter_Init()`

```c
GatewayRouter_Init();
```

Router 내부 상태를 초기화한다.

현재 Router가 저장하는 상태는 매우 작다.

```text
initialized

MOBILE → Domain Forward Count
Domain → MOBILE Forward Count
Not Ready Reject Count
Invalid Reject Count
```

차량 Request나 Payload 자체를 저장하지 않는다.

---

# 8. 왜 Router가 메시지를 저장하지 않는가?

현재 SysRS의 중요한 복구 원칙은:

```text
재시작 이전의 미완료 차량 제어 요청을
자동으로 다시 실행해서는 안 됨
```

이다.

따라서 현재 Router를:

```text
UART Down
→ Request Queue 저장
→ UART Recovery
→ 자동 재전송
```

구조로 만들지 않는다.

현재 구현:

```text
UART UNAVAILABLE
→ GATEWAY_STATUS_NOT_READY
→ 메시지 저장 안 함
→ 자동 Replay 없음
```

이다.

---

# 9. `GatewayRouter_BuildCoreHandlers()`

`Gateway_Interface`는 Core Callback을 필요로 한다.

Router는 다음 Helper를 제공한다.

```c
GatewayRouter_BuildCoreHandlers(&handlers);
```

결과적으로:

```c
handlers.on_mobile_message =
    GatewayRouter_OnMobileMessage;

handlers.on_domain_message =
    GatewayRouter_OnDomainMessage;
```

가 설정된다.

---

# 10. 초기화 예상 순서

현재 권장 초기화 흐름:

```text
Gateway_Time
   ↓
LinkStateManager_Init()
   ↓
MessageContextManager_Init()
   ↓
GatewayRouter_Init()
   ↓
GatewayRouter_BuildCoreHandlers()
   ↓
Transport Port 구성
   ↓
Gateway_Interface_Init()
```

실제 전체 Boot 순서는 이후 `GatewayLifecycleManager` 단계에서 정리한다.

---

# 11. Router Dependency Check

Router가 동작하기 위해 현재 다음이 준비되어야 한다.

```text
GatewayRouter initialized
LinkStateManager initialized
MessageContextManager initialized
Gateway_Interface initialized
```

하나라도 준비되지 않으면:

```c
GATEWAY_STATUS_NOT_READY
```

를 반환한다.

---

# 12. MOBILE 메시지 Validation

MOBILE 경로에서는 먼저:

```c
Gateway_Interface_IsValidRelayMessage(message)
```

를 다시 확인한다.

정상 방향:

```text
MOBILE_TO_DOMAIN
```

이다.

현재 허용 논리 메시지:

```text
MOBILE_REQUEST
STATE_QUERY
WARNING_ACK
```

---

# 13. 왜 Interface와 Router에서 둘 다 Validation하는가?

일반 흐름에서는:

```text
Gateway_Interface
→ Validation
→ GatewayRouter
```

이므로 중복처럼 보일 수 있다.

하지만 Router 함수는 공개 Callback이므로 테스트나 향후 다른 호출 경로에서 직접 호출될 가능성이 있다.

따라서 Core Boundary에서도 최소 Validation을 다시 수행한다.

이를 방어적 Validation으로 본다.

---

# 14. Current Session 확인

MOBILE 메시지의:

```text
Vehicle ID
Device Context ID
Session ID
```

가 현재 `MessageContextManager`의 Active Session과 맞아야 한다.

호출:

```c
MessageContextManager_IsViewCurrentSession(
    &message->context);
```

---

# 15. Router가 Session을 자동 생성하지 않는 이유

잘못된 구조:

```text
MOBILE Request 수신
      ↓
Router가 메시지 안 Session ID를 보고
자동 ActivateSession
      ↓
전달
```

이 구조는 아무 메시지나 현재 Session을 바꿀 수 있다.

따라서 Router는:

```text
Session 생성자
```

가 아니다.

현재 Session은 향후:

```text
DeviceRegistrationManager
GatewayLifecycleManager
Bluetooth Connection / Registration Context
```

등에서 확인된 뒤 활성화되어야 한다.

---

# 16. 현재 Session이 아니면

```c
GATEWAY_STATUS_NOT_READY
```

를 반환하고 메시지를 전달하지 않는다.

예:

```text
Current Session = 20

MOBILE Message Session = 19
```

이면 중계하지 않는다.

---

# 17. Lifecycle READY와 새 차량 제어 Request

현재 `MOBILE_REQUEST`는 `GatewayLifecycleManager`가 `READY`일 때만 중계한다.

```text
Current Session
      ↓
Lifecycle READY?
      ↓
UART AVAILABLE?
      ↓
Forward
```

`SYNCING / LINK_WAIT / DEGRADED`에서는 새 control request를 Domain으로 보내지 않는다.

다만 `STATE_QUERY`와 `WARNING_ACK`는 새 actuator 제어 요청과 구분한다. 현재 Session과 UART Path가 유효하면 SYNCING에서도 전달할 수 있다. 이는 복구 후 상태 재조회 같은 동기화 흐름을 막지 않기 위한 DESIGN이다.

---

# 18. MOBILE → Domain Link 확인

현재 Session이 맞아도:

```text
UART Path AVAILABLE
```

이어야 한다.

확인:

```c
LinkStateManager_IsAvailable(
    GATEWAY_LINK_DOMAIN_UART);
```

---

# 18. UART가 UNAVAILABLE이면

현재 구현:

```text
MOBILE Request
        ↓
Current Session OK
        ↓
UART UNAVAILABLE
        ↓
NOT_READY
        ↓
No Send
        ↓
No Queue
```

이다.

---

# 19. UART 복구 후 자동 전송 금지

예:

```text
10:00:00 Request A 수신
UART UNAVAILABLE
→ NOT_READY

10:00:05 UART AVAILABLE
```

이라고 해도:

```text
Request A 자동 Forward
```

되지 않는다.

Router 내부에 Request Queue가 없기 때문이다.

이것이 의도된 동작이다.

---

# 20. UART Loss와 차량 Result

Router가 반환하는:

```c
GATEWAY_STATUS_NOT_READY
```

는 ESP32 내부 Routing 결과다.

다음 의미가 아니다.

```text
Request FAILED
Request REJECTED
```

차량 결과는 Domain만 생성한다.

---

# 21. Context Capture

Link와 Session 조건이 모두 맞으면:

```c
MessageContextManager_CaptureForCurrentSession()
```

을 사용한다.

입력:

```text
Gateway_MessageContextView_t
```

출력:

```text
MessageContextManager_OwnedContext_t
```

이다.

---

# 22. 왜 Router에서 Context를 한 번 복사하는가?

Bluetooth/UART RX Buffer의 ID 포인터가 Transport Callback 종료 후 바뀔 수 있다.

따라서 실제 목적지 Sender를 호출하기 전에:

```text
Vehicle ID
Device ID
Session ID
Request ID
```

를 Owned Context로 만든다.

---

# 23. Payload는 왜 복사하지 않는가?

현재 Router는 **동기식 즉시 전달** 구조다.

```text
OnMobileMessage()
   ↓
SendToDomain()
   ↓
Transport Callback 완료
   ↓
return
```

이 범위에서는 원본 Payload View를 그대로 사용할 수 있다.

즉 Router가 Payload를 Queue에 저장하지 않는다.

---

# 24. Transport Sender 계약

현재 구조에서는:

```text
Gateway_Interface_SendToDomain()
Gateway_Interface_SendToMobile()
```

에 연결된 Transport Sender가 호출 중 필요한 데이터를 소비하거나 자체 Buffer로 복사해야 한다.

잘못된 Adapter 구현:

```c
saved_pointer = message->payload.data;
return;
```

후 나중에 해당 포인터를 사용하는 것.

원본 RX Buffer 수명이 끝날 수 있기 때문이다.

---

# 25. Context는 동일 식별 유지

Forward 시 Router가:

```text
Request ID 새로 생성
Session ID 새로 생성
Vehicle ID 수정
```

하지 않는다.

입력 Context를 Owned Storage로 복사한 뒤 같은 ID로 전달한다.

---

# 26. Payload 의미도 수정하지 않음

예:

```text
MOBILE Request Payload
```

를 Router가 해석해서 다른 명령으로 변환하지 않는다.

테스트에서도 원본 Payload bytes가 그대로 Transport Sender에 도달하는지 확인한다.

---

# 27. Domain → MOBILE Session 확인

Domain 메시지도 현재 Session과 맞아야 한다.

이 규칙의 목적:

```text
이전 Session의 지연 Result
이전 연결 구간의 State
이전 Session에 속한 Warning/Result
```

을 현재 MOBILE 화면에 그대로 전달하지 않기 위함이다.

---

# 28. 지연 Result 예시

```text
Session 10
Request A
        ↓
Disconnect

Session 11 활성화

그 뒤 늦게
Session 10 Result 도착
```

현재 Router:

```text
Session 10 != Current Session 11
        ↓
NOT_READY
        ↓
MOBILE로 전달하지 않음
```

이다.

---

# 29. Domain → MOBILE Bluetooth 확인

현재 Session이 맞더라도:

```text
Bluetooth AVAILABLE
```

이어야 한다.

확인:

```c
LinkStateManager_IsAvailable(
    GATEWAY_LINK_BLUETOOTH);
```

---

# 30. Bluetooth가 끊긴 경우

현재:

```text
Domain Warning / State / Result
        ↓
Bluetooth UNAVAILABLE
        ↓
NOT_READY
        ↓
Router가 저장하지 않음
```

이다.

---

# 31. 경고와 이력의 관계

SysRS에서는 연결 중 안전 경고를 일반 주기 State를 기다리지 않고 전달하고,
단절 중 즉시 도달을 보장하지 않으며 재연결 후 경고/이력을 조회하도록 한다.

따라서 Router가 Bluetooth 단절 중 Warning을 임의 Queue에 영구 저장하지 않는다.

경고 이력의 차량 측 보존은 Domain 쪽 정책이다.

---

# 32. Warning 우선 전달

Router 내부에는 Normal State 주기 Queue가 없다.

따라서 유효한 Warning이 들어오고:

```text
Current Session
+
Bluetooth AVAILABLE
```

이면 즉시 `SendToMobile()`을 호출한다.

일반 State 주기를 기다리지 않는다.

---

# 33. `GatewayRouter_Snapshot_t`

```c
typedef struct
{
    bool initialized;
    uint32_t mobile_to_domain_forwarded;
    uint32_t domain_to_mobile_forwarded;
    uint32_t rejected_not_ready;
    uint32_t rejected_invalid;
} GatewayRouter_Snapshot_t;
```

Debug/Test용 관찰 상태다.

---

# 34. Forward Counter

성공적으로 Transport Sender까지 전달된 횟수:

```text
mobile_to_domain_forwarded
domain_to_mobile_forwarded
```

를 센다.

차량 기능 완료 횟수가 아니다.

---

# 35. Reject Counter

현재 구분:

```text
rejected_not_ready
rejected_invalid
```

이다.

### NOT_READY

예:

```text
Current Session 없음
Session mismatch
UART unavailable
Bluetooth unavailable
Dependency not initialized
```

일부 Dependency 초기화 실패는 Handler 진입 전 Counter를 증가시키지 않을 수 있다.

Counter는 진단 참고값이지 요구사항 추적용 정확한 DTC가 아니다.

### INVALID

예:

```text
잘못된 Message Direction
잘못된 Logical Type 조합
잘못된 Context View
```

---

# 36. Router Snapshot이 차량 Diagnostic은 아님

현재 Counter는:

```text
Debug
Unit Test
Integration 관찰
```

용이다.

이를 바로:

```text
Vehicle DTC
```

로 사용하지 않는다.

향후 실제 진단 요구가 생기면 별도 Diagnostic mapping이 필요하다.

---

# 37. Interface Core Handler 연결 예시

```c
Gateway_InterfaceCoreHandlers_t core_handlers;

GatewayRouter_BuildCoreHandlers(
    &core_handlers);
```

그 뒤:

```c
Gateway_Interface_Init(
    &core_handlers,
    &transport_ports);
```

한다.

---

# 38. MOBILE → Domain 실제 예시

MOBILE:

```text
Door Unlock Request
Vehicle A
Session 20
Request 100
```

Router:

```text
Session 20 == Current Session?
        ↓ yes
UART AVAILABLE?
        ↓ yes
Context Copy
        ↓
Request 100 그대로 Domain 전송
```

Router는 Door 상태를 확인하지 않는다.

---

# 39. UART Down 예시

```text
MOBILE Door Unlock Request
        ↓
Session OK
        ↓
UART UNAVAILABLE
```

결과:

```text
Router = NOT_READY
Domain Send = 0회
Request Queue = 없음
```

UART 복구 후에도 자동 전송하지 않는다.

---

# 40. Domain Result 예시

```text
Domain
Request 100 DONE
        ↓
UART Adapter
        ↓
Router
```

Router는:

```text
DONE이 맞는 판단인가?
```

를 검토하지 않는다.

현재 Session과 Bluetooth Path만 확인하고 MOBILE에 그대로 전달한다.

---

# 41. Result 재작성 금지

Router는:

```text
DONE → FAILED
REJECTED → FAILED
UNKNOWN → DONE
```

처럼 차량 결과를 수정하지 않는다.

Payload는 그대로 전달된다.

---

# 42. State 재작성 금지

Domain에서 받은:

```text
Vehicle State
Function Availability
Digital Key Result
```

역시 ESP32가 의미를 보정하지 않는다.

---

# 43. MOBILE Request 재작성 금지

MOBILE에서 받은 Request도:

```text
Target 변경
Value 변경
Request ID 변경
```

하지 않는다.

---

# 44. LinkStateManager와의 책임 분리

```text
LinkStateManager
→ Bluetooth/UART 상태 판단

GatewayRouter
→ 그 상태를 이용해 지금 Relay할지 판단
```

이다.

Router가 Timeout 계산을 직접 하지 않는다.

---

# 45. MessageContextManager와의 책임 분리

```text
MessageContextManager
→ Current Session과 Context 소유/비교

GatewayRouter
→ 현재 Context인지 확인하고 Relay
```

이다.

Router가 generation을 직접 생성하지 않는다.

---

# 46. Gateway_Interface와의 책임 분리

```text
Gateway_Interface
→ Boundary Validation + Callback Dispatch

GatewayRouter
→ Context + Link 기반 Relay Coordination
```

이다.

---

# 47. Transport Adapter와의 책임 분리

```text
BluetoothAdapter
UartAdapter
UartFrameCodec
```

는 실제 바이트 전송을 담당한다.

Router는:

```text
UART Header
CRC
Sequence
Bluetooth UUID
MTU
```

를 알지 않는다.

---

# 48. 왜 Router가 Wire Format을 몰라야 하는가?

예를 들어 UART Protocol이:

```text
Header A
CRC16
```

에서:

```text
Header B
CRC32
```

로 바뀌더라도 Router의 판단은 여전히:

```text
Current Session?
UART Available?
```

이다.

따라서 Router를 수정하지 않는 것이 목표다.

---

# 49. Thread / Task 모델

현재 권장:

```text
Bluetooth Callback ─┐
                    ├→ Queue → Gateway Main Task → Router
UART Event ─────────┘
```

Router에 Mutex는 두지 않았다.

여러 Task에서 동시에 호출하려면 별도 synchronization이 필요하다.

---

# 50. Router가 Queue를 직접 가지지 않는 이유

이미 Gateway Task 앞단에서 Event Queue를 사용할 수 있다.

Router까지 또 별도 재전송 Queue를 만들면:

```text
Queue A
Queue B
Retry
Replay
Ownership
```

문제가 복잡해진다.

현재 Router는:

```text
입력 1개
→ 판단
→ 즉시 전달 또는 거부
```

로 단순하게 유지한다.

---

# 51. Event Queue와 Replay Queue는 다름

Gateway Main Task용 Event Queue는 필요할 수 있다.

예:

```text
Bluetooth callback event
UART callback event
```

를 Task Context로 넘기는 Queue.

하지만 이것은:

```text
통신이 끊겼으니 Request를 나중에 실행하기 위해 저장하는 Queue
```

와 다르다.

후자는 현재 만들지 않는다.

---

# 52. 현재 엄격한 Session Gate

현재 Generic Relay는 모두:

```text
Current Vehicle / Device / Session
```

과 일치해야 한다.

이는 이전 Session 지연 메시지 방지에 유리한 DESIGN이다.

---

# 53. 향후 Interface Matrix 확정 시 재검토할 부분

만약 후속 Protocol에서 특정 메시지가:

```text
Session 독립 Broadcast State
Vehicle-only Warning
Pre-session Registration Query
```

등으로 정의된다면 Message Type별 Context 요구를 세분화해야 한다.

현재 SysRS는 세부 Field/Message 형식이 아직 미정이므로,
v0.1 Router에서는 안전한 쪽으로 Current Session Gate를 적용한다.

---

# 54. Registration / Proximity Update는 Router를 지나지 않음

ESP32 자체 생성 정보:

```text
Registration / Connection
App Activity
Proximity
```

는 현재:

```c
Gateway_Interface_PublishRegistrationConnection()
Gateway_Interface_PublishAppActivity()
Gateway_Interface_PublishProximity()
```

를 이용한다.

Generic Relay Router와 분리되어 있다.

---

# 55. 왜 별도로 두는가?

이 정보들은:

```text
MOBILE에서 받아 그대로 전달하는 Payload
```

가 아니라 ESP32가 직접 확인/생성하는 Context 정보이기 때문이다.

향후:

```text
DeviceRegistrationManager
ProximityManager
```

가 이 Publish API를 호출한다.

---

# 56. 테스트 — 정상 MOBILE → Domain

검증:

```text
Current Session 일치
UART AVAILABLE
MOBILE_REQUEST
```

이면:

```text
SendToDomain = 1회
Request ID 동일
Payload 동일
```

인지 확인한다.

---

# 57. 테스트 — 정상 Domain → MOBILE

검증:

```text
Current Session 일치
Bluetooth AVAILABLE
REQUEST_RESULT
```

이면:

```text
SendToMobile = 1회
Context 동일
Payload 동일
```

이다.

---

# 58. 테스트 — UART Unavailable

```text
UART UNAVAILABLE
MOBILE Request
```

결과:

```text
NOT_READY
SendToDomain = 0
```

을 확인한다.

그 뒤 UART를 AVAILABLE로 바꿔도:

```text
SendToDomain = 0
```

이다.

즉 자동 Replay가 없다.

---

# 59. 테스트 — Bluetooth Unavailable

Domain State가 와도:

```text
Bluetooth UNAVAILABLE
→ NOT_READY
→ SendToMobile = 0
```

이다.

---

# 60. 테스트 — Old Session Result

```text
Session A
→ Disconnect / Invalidate
→ Session B 활성화
→ Session A Result 도착
```

결과:

```text
NOT_READY
SendToMobile = 0
```

인지 확인한다.

---

# 61. 테스트 — Payload Preserve

Payload:

```text
DE AD BE EF
```

를 입력하고 Transport Sender가 받은 값도 동일한지 비교한다.

Router가 Payload 의미를 변경하지 않음을 확인한다.

---

# 62. 테스트 — 잘못된 Direction

예:

```text
WARNING
+
MOBILE_TO_DOMAIN
```

은 `Gateway_Interface` Validation 단계에서 거부된다.

Router까지 전달되지 않는다.

---

# 63. 테스트 — Dependency Not Ready

Router와 Interface만 초기화하고:

```text
LinkStateManager
MessageContextManager
```

를 초기화하지 않은 경우:

```text
NOT_READY
```

를 반환하는지 확인한다.

---

# 64. 전체 회귀 테스트

이번 모듈 추가 후 다음 전체 Host Test를 다시 수행한다.

```text
Gateway_Time
Gateway_Interface
LinkStateManager
MessageContextManager
GatewayRouter
```

모두 기존 동작을 유지해야 한다.

---

# 65. 현재 프로젝트 구조

```text
ESP32/
├─ common/
│  ├─ Gateway_Types.h
│  ├─ Gateway_Time.h
│  └─ Gateway_Time.c
│
├─ interface/
│  ├─ Gateway_Interface.h
│  └─ Gateway_Interface.c
│
├─ core/
│  ├─ LinkStateManager.h
│  ├─ LinkStateManager.c
│  ├─ MessageContextManager.h
│  ├─ MessageContextManager.c
│  ├─ GatewayRouter.h              ✅
│  └─ GatewayRouter.c              ✅
│
├─ docs/
│  ├─ Gateway_Types_설명서_v0.1.md
│  ├─ Gateway_Time_설명서_v0.1.md
│  ├─ Gateway_Interface_설명서_v0.1.md
│  ├─ LinkStateManager_설명서_v0.1.md
│  ├─ MessageContextManager_설명서_v0.1.md
│  └─ GatewayRouter_설명서_v0.1.md
│
└─ test/
   ├─ test_gateway_time.c
   ├─ test_gateway_interface.c
   ├─ test_link_state_manager.c
   ├─ test_message_context_manager.c
   └─ test_gateway_router.c
```

---

# 66. 다음 단계

다음 대상은:

```text
GatewayLifecycleManager.h/.c
```

이다.

지금까지는 각 Manager가 따로 존재했다.

다음 단계에서는:

```text
Boot
Link Wait
Session 확인
Syncing
Ready
Degraded / Recovery
```

흐름을 조정해야 한다.

---

# 67. GatewayLifecycleManager에서 해야 할 일

예상 책임:

```text
부팅 상태 관리
LinkStateManager 상태 관찰
Session activation/invalidation 조정
UART recovery 후 Resynchronization 요청
Ready / Degraded 상태 관리
```

---

# 68. LifecycleManager에서도 주의할 것

다음은 하면 안 된다.

```text
UART 복구
→ 과거 Request Replay

Bluetooth Disconnect
→ Proximity FAR 생성

Link Available
→ 차량 Function Available 판단
```

Lifecycle은 Gateway 동작 상태만 조정해야 한다.

---

# 69. 현재까지의 핵심 흐름

```text
Gateway_Types
      ↓
Gateway_Time
      ↓
Gateway_Interface
      ↓
┌──────────────────────────────┐
│ LinkStateManager             │
│ MessageContextManager        │
└─────────────┬────────────────┘
              ↓
         GatewayRouter
```

이제 ESP32 Gateway Core의 최소 Relay 경로가 만들어진 상태다.

---

# 70. 핵심 정리

`GatewayRouter`를 한 문장으로 표현하면:

> **현재 Session과 통신 경로가 유효할 때만 MOBILE↔Domain 논리 메시지를 의미 변경 없이 즉시 중계하는 ESP32 Core Coordinator다.**

가장 중요한 규칙:

```text
UART unavailable
→ No Send
→ No Queue
→ No Auto Replay

Bluetooth unavailable
→ No Mobile Send

Old Session
→ No Relay

Route Failure
≠
Vehicle FAILED

Payload / Request ID
→ 변경하지 않음

Router
≠
Session Creator
≠
Vehicle Policy Owner
```


---

# Lifecycle Integration Addendum

v0.2에서 `GatewayLifecycleManager` 연동이 추가되었다.

현재 MOBILE 방향 Router 조건:

```text
MOBILE_REQUEST
  → Current Session
  → Lifecycle READY
  → UART AVAILABLE
  → Relay

STATE_QUERY / WARNING_ACK
  → Current Session
  → UART AVAILABLE
  → Relay 가능
```

따라서 Link가 복구됐더라도 Session 재확인과 Resynchronization이 끝나기 전에는 새 차량 control request가 전달되지 않는다.

Router는 여전히 Lifecycle 상태 전이를 직접 만들지 않는다.

```text
GatewayLifecycleManager
→ READY 여부 제공

GatewayRouter
→ READY 값을 relay gate로 사용
```


---

# Device Registration Integration Addendum

v0.3부터 새 `MOBILE_REQUEST`의 Gateway-side gate에 현재 Bluetooth peer 인증 확인이 추가되었다.

```text
MOBILE_REQUEST
  ↓
Current Session?
  ↓
DeviceRegistrationManager:
REGISTERED + CONNECTED?
  ↓
Gateway Lifecycle READY?
  ↓
UART AVAILABLE?
  ↓
Domain Relay
```

이 확인은 ESP32가 차량 실행 허용을 판단한다는 뜻이 아니다.

ESP32가 확인하는 것은 현재 MOBILE 출처가 등록된 Bluetooth peer이며 현재 연결이 유지되고 있다는 Gateway-side 조건이다. 실제 차량 기능 지원, 상태, 인터록, 정책, 실행 허용은 Domain이 별도로 판단한다.

등록 해제 Event 이후 Lifecycle Update가 아직 호출되지 않았더라도 Router의 직접 Auth gate가 새 control request를 막는다.
