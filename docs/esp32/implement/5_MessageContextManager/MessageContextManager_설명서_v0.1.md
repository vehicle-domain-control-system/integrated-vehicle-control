# MessageContextManager 설계 설명서 v0.1

> 대상 코드  
> - `ESP32/core/MessageContextManager.h`  
> - `ESP32/core/MessageContextManager.c`
>
> 관련 테스트  
> - `ESP32/test/test_message_context_manager.c`
>
> 기준 문서  
> - `ESP32 Wireless Gateway Software Architecture v0.3`
> - `Gateway_Types.h 설계 설명서 v0.1`
> - `Gateway_Interface 설계 설명서 v0.1`
> - `LinkStateManager 설계 설명서 v0.1`

---

# 1. 목적

`MessageContextManager`는 ESP32 Gateway가 MOBILE과 Domain 사이에서 메시지를 중계할 때 필요한 식별 관계를 보존한다.

핵심 대상:

```text
Vehicle ID
Device Context ID
Session ID
Request ID
```

이다.

ESP32의 역할은 이 ID들의 차량 의미를 새로 만드는 것이 아니라,

```text
MOBILE에서 받은 식별
        ↓
      ESP32
        ↓
Domain으로 같은 식별 관계 유지
```

하는 것이다.

---

# 2. 왜 별도 Manager가 필요한가?

`Gateway_Interface`의 ID는 다음과 같이 `ByteView`다.

```c
typedef struct
{
    const uint8_t *data;
    size_t length;
} Gateway_ByteView_t;
```

이 구조는 메모리를 직접 소유하지 않는다.

예를 들어 Bluetooth RX Buffer가:

```text
RX Buffer
   ↓
Gateway_MessageContextView_t
```

로 전달되었다가 Callback이 끝난 뒤 재사용되면 기존 포인터를 저장해 두는 것은 위험하다.

따라서 장시간 보존이 필요한 Context는:

```text
View
 ↓ Copy
Owned Storage
```

구조로 바꿔야 한다.

이 역할을 `MessageContextManager`가 담당한다.

---

# 3. SysRS와의 연결

현재 요구사항의 핵심은 다음과 같다.

```text
대상 차량·세션·요청 식별과 요청 내용의 연계를 유지
같은 요청의 재전달을 새로운 차량 요청으로 바꾸지 않음
재전송 시 원 요청 식별과 중복 방지 근거 유지
재시작/복구 후 이전 실행 구간의 지연 메시지를
새 요청 또는 최신 상태로 사용하지 않음
```

따라서 Manager는 단순 ID 복사뿐 아니라:

```text
현재 Session Context
+
Gateway-local generation
```

을 같이 관리한다.

---

# 4. 이 Manager가 하는 것

```text
ID 복사
ID 비교
현재 Session Context 저장
Session Context 변경 감지
Session 무효화
generation 관리
Owned Context 생성
현재 Context인지 확인
동일 Request 식별인지 비교
```

---

# 5. 하지 않는 것

다음은 이 Manager 책임이 아니다.

```text
Request ACCEPTED 판단
Request REJECTED 판단
Request DONE / FAILED 판단
Request Lifecycle
Retry 정책
차량 Duplicate 수용/거부 정책
UART Sequence 생성
CRC 검사
Bluetooth 등록 판단
Door Unlock 판단
```

특히:

```text
같은 Request ID인지 비교
```

와

```text
중복 Request이므로 거부
```

는 다른 문제다.

Manager는 앞의 비교만 제공한다.

최종 Request 처리 정책은 Domain의 `RequestManager` 책임이다.

---

# 6. 전체 위치

현재 구조:

```text
Bluetooth / UART
      ↓
Gateway_Interface
      ↓
GatewayRouter
      ↓
MessageContextManager
      ↓
Owned Context / Current Session
```

`GatewayRouter`는 이후 이 Manager를 이용해:

```text
현재 Session 메시지인가?
이전 연결 구간 메시지인가?
Request ID를 그대로 보존할 수 있는가?
```

를 확인할 수 있다.

---

# 7. `Gateway_MessageContextView_t`

기존 Interface 타입:

```c
typedef struct
{
    Gateway_ByteView_t vehicle_id;
    Gateway_ByteView_t device_context_id;
    Gateway_ByteView_t session_id;
    Gateway_ByteView_t request_id;
} Gateway_MessageContextView_t;
```

이 타입은 데이터를 소유하지 않는다.

따라서 `MessageContextManager`가 이를 직접 장기 저장하지 않는다.

---

# 8. Owned ID Storage

```c
typedef struct
{
    uint8_t data[MESSAGE_CONTEXT_MANAGER_MAX_ID_BYTES];
    size_t length;
} MessageContextManager_IdStorage_t;
```

실제 ID 바이트를 내부 배열에 복사한다.

이제 원본 Bluetooth/UART Buffer가 재사용되어도 저장된 ID는 유지된다.

---

# 9. 왜 ID 타입을 아직 `uint32_t`로 만들지 않는가?

현재 실제 ID 표현은 아직 확정되지 않았다.

예:

```text
Vehicle ID
→ 숫자?
→ 문자열?
→ UUID?

Session ID
→ 16 bit?
→ 32 bit?
→ 64 bit?

Request ID
→ Counter?
→ UUID?
```

따라서:

```c
uint32_t request_id;
```

처럼 Architecture 단계에서 Wire 표현을 선점하지 않는다.

현재는:

```text
opaque bytes + length
```

으로 유지한다.

---

# 10. `MESSAGE_CONTEXT_MANAGER_MAX_ID_BYTES`

현재 기본:

```c
#define MESSAGE_CONTEXT_MANAGER_MAX_ID_BYTES (64U)
```

이다.

이 값은 **프로토콜 ID 길이를 64 byte로 확정한다는 뜻이 아니다.**

목적:

```text
동적 할당 없이
ID를 안전하게 복사할 수 있는
현재 구현용 Buffer Budget
```

이다.

실제 Protocol이 확정되면 build define 또는 Header 수정으로 조정할 수 있다.

---

# 11. 왜 64 byte Buffer를 사용하는가?

현재 단계에서는:

```text
ID 형식은 TBD
Dynamic Allocation은 피하고 싶음
```

이라는 두 조건이 있다.

따라서 임시로 충분한 고정 Buffer를 두고:

```text
Wire Contract
```

와:

```text
Implementation Storage Capacity
```

를 분리한다.

만약 실제 ID가 이보다 크다면 현재 구현은:

```c
GATEWAY_STATUS_UNSUPPORTED
```

를 반환한다.

즉 조용히 잘라서 저장하지 않는다.

---

# 12. `MessageContextManager_OwnedContext_t`

```c
typedef struct
{
    MessageContextManager_IdStorage_t vehicle_id;
    MessageContextManager_IdStorage_t device_context_id;
    MessageContextManager_IdStorage_t session_id;
    MessageContextManager_IdStorage_t request_id;

    uint32_t generation;
    Gateway_TimeMs_t captured_at_ms;
} MessageContextManager_OwnedContext_t;
```

하나의 메시지 Context를 안전하게 소유한다.

---

# 13. 왜 `captured_at_ms`가 필요한가?

현재 단계에서는 Request Timeout을 판단하지 않는다.

하지만 이후 다음 판단에 사용할 근거가 될 수 있다.

```text
이 Context가 언제 Gateway에 저장되었는가?
재전송 시 너무 오래된 Context인가?
Debug 시 어느 시점 메시지인가?
```

단:

```text
captured_at_ms
```

를 새 차량 Request 생성 시각으로 해석하면 안 된다.

Gateway 내부 캡처 시각일 뿐이다.

---

# 14. Active Session Context

Manager는 하나의 현재 Session Context를 관리한다.

```c
typedef struct
{
    bool active;

    Vehicle ID
    Device Context ID
    Session ID

    uint32_t generation;
    Gateway_TimeMs_t activated_at_ms;
} MessageContextManager_ActiveSessionSnapshot_t;
```

Request ID는 Active Session에 포함하지 않는다.

이유:

```text
Session 하나
 ├ Request A
 ├ Request B
 └ Request C
```

처럼 하나의 Session에 여러 Request가 존재할 수 있기 때문이다.

---

# 15. Active Session 최소 조건

현재 구현에서는 Active Session을 만들기 위해:

```text
Vehicle ID
Session ID
```

가 있어야 한다.

`DeviceContextId`는 아직 상세 Interface 계약이 미정이므로 optional이다.

이 규칙은 현재 SysRS의:

```text
대상 차량·세션·요청 식별
```

을 코드 경계에 반영한 DESIGN 규칙이다.

향후 단일 차량 구조에서 Vehicle ID가 implicit하도록 Protocol이 확정된다면 조정할 수 있다.

---

# 16. `MessageContextManager_ActivateSessionAt()`

```c
MessageContextManager_ActivateSessionAt(
    &context,
    now_ms);
```

현재 Vehicle / Device / Session Context를 활성화한다.

예:

```text
Vehicle = VEHICLE_A
Device  = PHONE_A
Session = SESSION_10
```

---

# 17. 같은 Session을 다시 활성화하면?

현재 활성 Context와:

```text
Vehicle ID
Device Context ID
Session ID
```

가 모두 같다면 generation을 바꾸지 않는다.

예:

```text
Generation 7
Session A
```

상태에서 다시 Session A가 확인되면:

```text
Generation 7 유지
```

한다.

단순 상태 재확인만으로 기존 메시지를 stale 처리하지 않기 위해서다.

---

# 18. Session이 변경되면?

예:

```text
Vehicle A
Session 10
```

에서:

```text
Vehicle A
Session 11
```

로 바뀌면 새 generation을 만든다.

```text
Generation 7
   ↓
Generation 8
```

이제 generation 7에 캡처된 메시지는 현재 Context가 아니다.

---

# 19. Generation의 목적

Generation은 실제 UART/Bluetooth Protocol ID가 아니다.

ESP32 내부의 실행 구간 구분값이다.

예:

```text
Session / Connection Epoch #10
Request A

Disconnect

Session / Connection Epoch #11
Request A
```

Wire 상 Request ID가 우연히 같아도:

```text
generation 10
≠
generation 11
```

이므로 동일한 현재 Request로 취급하지 않는다.

---

# 20. 왜 Generation이 필요한가?

다음 상황을 생각할 수 있다.

```text
Session 1
Request ID = 5
        ↓
통신 단절
        ↓
Reconnect
        ↓
새 Session
Request ID = 5
```

Request ID 숫자만 비교하면 같은 요청처럼 보일 수 있다.

하지만 SysRS는:

```text
이전 실행 구간의 지연 메시지
```

를 새 현재 메시지로 처리하면 안 된다고 한다.

따라서 Gateway 내부에서는:

```text
Wire Context
+
Local Generation
```

을 함께 본다.

---

# 21. Generation은 Wire에 보내지 않는다

현재 `generation`은:

```text
ESP32-local anti-stale evidence
```

다.

자동으로 UART Frame에 넣는 값이 아니다.

향후 Network Protocol에서 실제 Sequence/Generation 개념이 필요하면 별도 Wire 설계가 필요하다.

---

# 22. Session 무효화

```c
MessageContextManager_InvalidateSessionAt(now_ms);
```

를 호출하면:

```text
active = false
generation = next
```

가 된다.

사용 예상 상황:

```text
Bluetooth Disconnect
등록 무효
Gateway Restart / Resync
Session 종료 확인
```

---

# 23. 왜 무효화할 때 Generation도 바꾸는가?

단순히:

```text
active = false
```

만 하면 나중에 같은 ID가 다시 활성화되었을 때 이전 Context와 구분이 약해질 수 있다.

그래서:

```text
Invalidate
→ Generation advance
```

한다.

---

# 24. Reset과 Generation

같은 프로세스 안에서:

```text
Reset
Init
```

이 발생해도 이전 generation을 바로 재사용하지 않는다.

현재 구현은 내부 counter를 계속 증가시킨다.

목적:

```text
이전 Owned Context
```

가 우연히 재초기화 후 다시 current처럼 보이는 것을 막기 위해서다.

---

# 25. MCU Cold Reset

실제 MCU 전체 Reset에서는 일반적으로 RAM도 다시 초기화된다.

따라서:

```text
기존 Owned Context
```

자체가 사라지는 것이 기본이다.

하지만 UART Driver Buffer, Queue, 지연 Frame 같은 문제는 별도다.

이들은 이후:

```text
GatewayLifecycleManager
UartAdapter
GatewayRouter
```

에서 재동기화 과정 중 폐기/검증해야 한다.

MessageContextManager 하나만으로 모든 Restart 안전성을 해결하지 않는다.

---

# 26. Context Capture

현재 Session에 속하는 메시지를 소유 가능한 구조로 복사:

```c
MessageContextManager_CaptureForCurrentSessionAt(
    &view,
    now_ms,
    &owned);
```

한다.

---

# 27. Capture 전 확인

다음이 모두 맞아야 한다.

```text
Active Session 존재
Vehicle ID 동일
Device Context ID 동일
Session ID 동일
```

다르면:

```c
GATEWAY_STATUS_NOT_READY
```

를 반환한다.

즉 현재 Session으로 확인되지 않은 메시지를 조용히 현재 Context에 넣지 않는다.

---

# 28. Request ID는 Capture 시 복사

Request ID는 Active Session의 일부가 아니지만 각 메시지에는 포함될 수 있다.

예:

```text
Session A
Request 100
```

을 Capture하면:

```text
Vehicle A
Device A
Session A
Request 100
Generation 7
```

형태로 보관한다.

---

# 29. View → Owned Copy

원래:

```text
Bluetooth Rx Buffer
   ↓ pointer
Gateway_MessageContextView_t
```

였다면 Capture 후:

```text
MessageContextManager_OwnedContext_t
   ├ Vehicle bytes copy
   ├ Device bytes copy
   ├ Session bytes copy
   └ Request bytes copy
```

가 된다.

따라서 원본 Buffer가 바뀌어도 저장된 Context는 유지된다.

---

# 30. `MakeView()`

Owned Context를 다시 Gateway Interface에 전달하려면:

```c
MessageContextManager_MakeView(
    &owned,
    &view);
```

를 사용할 수 있다.

생성된 View는:

```text
owned 내부 배열
```

을 가리킨다.

따라서 `owned`보다 오래 사용하면 안 된다.

---

# 31. Current Session 확인

```c
MessageContextManager_IsViewCurrentSession(...)
```

은 입력 View의:

```text
Vehicle
Device
Session
```

이 현재 활성 Session과 같은지 확인한다.

Request ID는 비교하지 않는다.

---

# 32. Owned Context Current 확인

```c
MessageContextManager_IsOwnedContextCurrent(...)
```

은 두 가지를 확인한다.

```text
Generation 동일?
Vehicle/Device/Session 동일?
```

둘 다 맞아야 current다.

---

# 33. 왜 ID만 비교하지 않는가?

다음 상황:

```text
Session ID가 재사용됨
또는
지연 메시지가 도착함
```

이 있을 수 있다.

Generation까지 확인하면:

```text
같은 bytes
+
다른 실행 구간
```

을 구분할 수 있다.

---

# 34. Same Request Identity

```c
MessageContextManager_IsSameRequestIdentity(
    &left,
    &right);
```

는 다음을 비교한다.

```text
Generation
Vehicle ID
Device Context ID
Session ID
Request ID
```

모두 같아야 true다.

---

# 35. Request ID가 없으면?

두 메시지에 Request ID가 모두 비어 있다고 해서:

```text
같은 Request
```

라고 판단하지 않는다.

따라서:

```text
request_id 없음
→ false
```

이다.

이는 매우 중요하다.

```text
"둘 다 Request ID가 없음"
```

은 동일 Request라는 근거가 아니다.

---

# 36. 동일 Request 판단과 Duplicate 정책

`IsSameRequestIdentity()`가 true여도 Manager는:

```text
거부
허용
재전송
```

을 결정하지 않는다.

의미:

```text
이 두 Context의 식별이 같다
```

뿐이다.

실제 Duplicate 판단은 Domain RequestManager 또는 향후 Router 정책과 결합해야 한다.

---

# 37. 재전송과 Original Request ID

같은 요청을 다시 전달해야 하는 경우:

```text
Request ID 새로 생성
```

하면 안 된다.

기존 Owned Context를 사용하면:

```text
같은 Vehicle
같은 Session
같은 Request ID
```

를 유지할 수 있다.

단, 자동 Replay 자체는 별도 정책상 금지될 수 있다.

---

# 38. 자동 Replay는 하지 않는다

MessageContextManager가 Context를 저장한다고 해서:

```text
UART Down
→ Request 저장
→ UART Recovery
→ 자동 Send
```

구조를 만드는 것이 아니다.

이 Manager는:

```text
식별 정보 저장
```

만 한다.

Replay 여부는 상위 설계에서 결정하며, 현재 SysRS상 재시작/복구 후 이전 미완료 요청 자동 실행은 금지한다.

---

# 39. 이전 실행 구간 Request

예:

```text
Generation 10
Request ID 3
```

가 저장되어 있다.

Disconnect 후:

```text
Generation 12
```

가 되었다면:

```c
MessageContextManager_IsOwnedContextCurrent(...)
```

는 false다.

같은 Wire Request ID `3`이 새로 들어와도:

```text
Generation 12
```

이므로 old context와 동일 current request로 보지 않는다.

---

# 40. Capacity 초과

ID가:

```text
MESSAGE_CONTEXT_MANAGER_MAX_ID_BYTES
```

보다 크면:

```c
GATEWAY_STATUS_UNSUPPORTED
```

를 반환한다.

잘라서 저장하지 않는다.

잘라서 저장하면 서로 다른 ID가 같은 ID처럼 비교될 수 있기 때문이다.

---

# 41. Partial Update 방지

새 Active Session을 복사할 때 바로 전역 상태에 쓰지 않는다.

먼저:

```text
candidate
```

임시 구조체에 전체 복사를 완료한다.

모든 ID Copy 성공 후에만:

```text
s_active_session = candidate
```

한다.

따라서 중간에 길이 초과 같은 오류가 나도 기존 Active Session이 반쯤 덮어써지지 않는다.

---

# 42. Thread Safety

현재 별도 Mutex는 없다.

기본 Architecture:

```text
Bluetooth Callback
UART Callback
      ↓
    Queue
      ↓
Gateway Main Task
      ↓
MessageContextManager
```

를 전제로 한다.

여러 Task가 직접 동시에 Manager를 호출한다면 보호 전략을 추가해야 한다.

---

# 43. ISR 사용

ISR에서 직접 호출하는 것을 권장하지 않는다.

Context Copy에는:

```text
memcpy
상태 비교
```

등이 포함된다.

권장:

```text
ISR / Driver
  ↓ minimal event
Queue
  ↓
Gateway Task
  ↓
MessageContextManager
```

이다.

---

# 44. 현재 Session 활성화 예시

```c
Gateway_MessageContextView_t context = {
    .vehicle_id = ...,
    .device_context_id = ...,
    .session_id = ...,
    .request_id = {NULL, 0U}
};

MessageContextManager_ActivateSession(&context);
```

---

# 45. Request Capture 예시

```c
MessageContextManager_OwnedContext_t owned;

if (MessageContextManager_CaptureForCurrentSession(
        &message.context,
        &owned) == GATEWAY_STATUS_OK)
{
    ...
}
```

이후 원본 RX Buffer가 재사용되어도 `owned`는 안전하다.

---

# 46. Request 재중계 예시

Owned Context에서 View를 다시 만든다.

```c
Gateway_MessageContextView_t context_view;

MessageContextManager_MakeView(
    &owned,
    &context_view);
```

그리고 Relay Message에 넣을 수 있다.

```c
relay.context = context_view;
```

단 실제 payload도 별도의 유효 수명이 필요하다.

Context만 소유한다고 Payload까지 자동 소유되는 것은 아니다.

---

# 47. Payload는 관리하지 않는다

현재 Manager는:

```text
Vehicle ID
Device ID
Session ID
Request ID
```

만 관리한다.

다음은 관리하지 않는다.

```text
Door Unlock payload
Climate payload
Warning payload
Vehicle State payload
```

향후 Queue에 메시지 전체를 보관하려면 별도 Message Storage 설계가 필요하다.

---

# 48. LinkStateManager와의 관계

두 Manager 역할:

```text
LinkStateManager
→ 지금 경로가 살아 있는가?

MessageContextManager
→ 이 메시지는 현재 Vehicle/Session에 속하는가?
```

예:

```text
UART AVAILABLE
```

이어도:

```text
old Session Result
```

이면 current로 취급하면 안 된다.

둘 다 확인해야 한다.

---

# 49. GatewayRouter와의 예상 결합

MOBILE → Domain:

```text
OnMobileMessage
  ↓
현재 Session 확인
  ↓
Context Capture
  ↓
UART AVAILABLE 확인
  ↓
SendToDomain
```

Domain → MOBILE:

```text
OnDomainMessage
  ↓
현재 Session Context 확인
  ↓
Bluetooth AVAILABLE 확인
  ↓
SendToMobile
```

정확한 흐름은 다음 `GatewayRouter` 단계에서 구현한다.

---

# 50. Session 전환 예시

```text
Vehicle A / Session 10 / Generation 5

Request A Capture
Request B Capture

Bluetooth Disconnect
        ↓
InvalidateSession
        ↓
Generation 6 / inactive

Reconnect
        ↓
Vehicle A / Session 11
        ↓
Generation 7
```

이제 Generation 5 Request는 current가 아니다.

---

# 51. 같은 Session 재확인 예시

```text
Generation 7
Vehicle A
Session 11
```

상태에서 동일 Context를 다시 수신:

```text
Vehicle A
Session 11
```

이면 generation을 바꾸지 않는다.

이것은 단순 재확인을 Session 전환으로 오인하지 않기 위해서다.

---

# 52. Device Context 변경

현재 비교에는:

```text
DeviceContextId
```

도 포함한다.

따라서 같은 Vehicle/Session ID라도 Device Context가 바뀌면 새 Session Context로 간주한다.

이것은 현재 Domain Architecture의 Device Context 구분을 보존하기 위한 DESIGN이다.

---

# 53. Device Context가 없는 경우

현재 Device Context는 optional이다.

두 Context 모두 비어 있으면:

```text
empty == empty
```

으로 비교한다.

한쪽만 값이 있으면 다른 Context로 본다.

---

# 54. Vehicle ID와 Session ID

Active Session에서는 현재:

```text
vehicle_id 필수
session_id 필수
```

로 두었다.

이는 Target Vehicle / Session 연계 요구를 코드에서 명시적으로 유지하기 위한 것이다.

추후 Protocol에서 특정 값이 implicit하다고 확정되면 Validation을 조정해야 한다.

---

# 55. 테스트 — 원본 Buffer 복사

테스트에서:

```text
ActivateSession
```

후 원본 배열 값을 강제로 변경한다.

Manager Snapshot이 바뀌지 않는지 확인한다.

이를 통해:

```text
Pointer 보관
```

이 아니라:

```text
실제 Copy
```

임을 검증한다.

---

# 56. 테스트 — 같은 Session Generation 유지

동일 Session을 두 번 활성화하고:

```text
generation before
generation after
```

가 같은지 확인한다.

---

# 57. 테스트 — Session 변경

Session A:

```text
generation X
```

에서 Session B로 바꾸면:

```text
generation Y
```

가 되고:

```text
X != Y
```

인지 확인한다.

이전 Owned Context도 current가 아니어야 한다.

---

# 58. 테스트 — Session Invalidate

Active Session을 무효화하면:

```text
active = false
generation 변경
```

이 되고 기존 Owned Context가 current가 아닌지 확인한다.

---

# 59. 테스트 — Other Session Capture 거부

현재:

```text
Session A
```

인데:

```text
Session B
```

Context를 Capture하려 하면:

```c
GATEWAY_STATUS_NOT_READY
```

를 반환한다.

즉 잘못된 Session을 현재 Context에 조용히 섞지 않는다.

---

# 60. 테스트 — Same Request Identity

동일:

```text
Generation
Vehicle
Device
Session
Request
```

이면 true다.

Request ID 하나라도 다르면 false다.

---

# 61. 테스트 — Request ID 없음

Request ID가 없으면:

```text
same request
```

로 판단하지 않는다.

이는 조회/상태 메시지 등을 Request와 혼동하지 않기 위해 중요하다.

---

# 62. 테스트 — 재연결 전후 동일 Wire ID

특히 다음 회귀 테스트를 포함한다.

```text
Generation A
Vehicle 1
Session 1
Request 3

Invalidate / Reconnect

Generation B
Vehicle 1
Session 1
Request 3
```

Wire ID들이 모두 같더라도:

```text
A != B
```

이므로:

```c
IsSameRequestIdentity(...)
```

는 false다.

이는 지연 메시지의 새 현재 요청 오인을 줄이기 위한 Gateway-local 방어다.

---

# 63. 테스트 — Storage Capacity

64 byte를 초과하는 ID를 넣으면:

```c
GATEWAY_STATUS_UNSUPPORTED
```

를 반환하는지 확인한다.

잘라서 저장하지 않는다.

---

# 64. 테스트 — Reset / Reinit

같은 Process 안에서:

```text
Init
Activate
Reset
Init
```

했을 때 이전 generation을 재사용하지 않는지 확인한다.

---

# 65. 테스트 결과

현재 Host GCC:

```text
-std=c11
-Wall
-Wextra
-Werror
-DGATEWAY_TIME_HOST_TEST
```

조건에서:

```text
Compile PASS
Unit Test PASS
```

이다.

---

# 66. 이전 모듈과 회귀 검증

이번 모듈 추가 후 전체 Host 단위 테스트를 다시 수행했다.

```text
Gateway_Time
Gateway_Interface
LinkStateManager
MessageContextManager
```

모두 Compile / Test PASS 상태를 유지한다.

---

# 67. 현재 구현 구조

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
│  ├─ MessageContextManager.h       ✅
│  └─ MessageContextManager.c       ✅
│
├─ docs/
│  ├─ Gateway_Types_설명서_v0.1.md
│  ├─ Gateway_Time_설명서_v0.1.md
│  ├─ Gateway_Interface_설명서_v0.1.md
│  ├─ LinkStateManager_설명서_v0.1.md
│  └─ MessageContextManager_설명서_v0.1.md
│
└─ test/
   ├─ test_gateway_time.c
   ├─ test_gateway_interface.c
   ├─ test_link_state_manager.c
   └─ test_message_context_manager.c
```

---

# 68. 다음 단계

다음 구현 대상은:

```text
GatewayRouter.h/.c
```

이다.

지금까지 만든 기반:

```text
Gateway_Interface
        ↓
MessageContextManager
        ↓
LinkStateManager
```

를 실제 메시지 흐름으로 연결한다.

---

# 69. GatewayRouter에서 구현할 핵심

MOBILE → Domain 요청:

```text
MOBILE Message
  ↓
Direction / Type validation
  ↓
Current Session?
  ↓
Context Capture
  ↓
UART Path AVAILABLE?
  ↓
Domain으로 Relay
```

Domain → MOBILE:

```text
Domain Message
  ↓
Current Session?
  ↓
Bluetooth AVAILABLE?
  ↓
MOBILE로 Relay
```

---

# 70. GatewayRouter에서도 하지 않을 것

다음 단계에서도 Router는:

```text
Door Unlock 허용 판단
Request ACCEPTED 생성
Request FAILED 생성
Auto Unlock Policy
```

를 하지 않는다.

Router의 질문은:

```text
"이 메시지를 현재 경로와 Context 기준으로
그대로 중계해도 되는가?"
```

까지다.

---

# 71. 핵심 정리

`MessageContextManager`를 한 문장으로 표현하면:

> **MOBILE↔Domain 메시지의 Vehicle / Device / Session / Request 식별을 ESP32 내부에서 안전하게 소유·보존하고, 재연결 전후의 실행 구간을 분리하는 Core 모듈이다.**

가장 중요한 원칙:

```text
ByteView
≠
Owned Storage

같은 Request ID
≠
항상 같은 현재 Request

Reconnect
≠
이전 Generation 재사용

ID 비교
≠
차량 Duplicate 정책 판단

Context 저장
≠
자동 Request Replay
```

이 경계를 유지해야 한다.
