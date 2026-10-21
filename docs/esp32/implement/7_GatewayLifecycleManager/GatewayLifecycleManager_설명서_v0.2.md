# GatewayLifecycleManager 설계 설명서 v0.2

> 대상 코드  
> - `ESP32/service/GatewayLifecycleManager.h`  
> - `ESP32/service/GatewayLifecycleManager.c`
>
> 관련 테스트  
> - `ESP32/test/test_gateway_lifecycle_manager.c`
>
> 함께 변경된 코드  
> - `ESP32/core/GatewayRouter.c`
>
> 기준 문서  
> - `ESP32 Wireless Gateway Software Architecture v0.3`
> - `LinkStateManager 설계 설명서 v0.1`
> - `MessageContextManager 설계 설명서 v0.1`
> - `GatewayRouter 설계 설명서`
> - SR / SysRS의 ESP32 재시작·연결 복구 요구

---

# 1. 목적

`GatewayLifecycleManager`는 ESP32 Wireless Gateway 전체가 현재 어떤 **운영 단계**에 있는지를 관리한다.

지금까지 구현한 Manager들은 각각 다음 질문에 답했다.

```text
LinkStateManager
→ Bluetooth/UART 경로가 살아 있는가?

MessageContextManager
→ 현재 Vehicle/Device/Session Context는 무엇인가?

GatewayRouter
→ 지금 이 메시지를 중계할 수 있는가?
```

LifecycleManager는 그 위에서:

> **Gateway 전체가 지금 정상적인 새 차량 제어 요청을 중계할 준비가 되었는가?**

를 관리한다.

---

# 2. 내부 Lifecycle 상태

현재 내부 DESIGN 상태:

```text
STARTUP
LINK_WAIT
SYNCING
READY
DEGRADED
```

이다.

중요:

이 상태들은 SysRS에서 정의된 공통 차량 Enum이 아니다.

ESP32 Architecture 구현을 위해 만든 내부 운영 상태다.

---

# 3. 전체 흐름

기본 흐름:

```text
RESET
  ↓
Gateway Core Init
  ↓
STARTUP
  ↓
Start
  ↓
LINK_WAIT
  ↓
Bluetooth AVAILABLE
UART Path AVAILABLE
Current Session 확인
  ↓
SYNCING
  ↓
현재 Connection / Session / 필요한 상태 재확인
  ↓
Synchronization Complete
  ↓
READY
```

통신 상실:

```text
READY
  ↓
Bluetooth 또는 UART Path 상실
  ↓
현재 Session confirmation 무효화
  ↓
DEGRADED
```

복구:

```text
DEGRADED
  ↓
Link 복구
  ↓
Current Session 재확인
  ↓
SYNCING
  ↓
Resynchronization
  ↓
READY
```

---

# 4. STARTUP

`GatewayLifecycleManager_Init()` 직후 상태다.

```text
initialized = true
started = false
state = STARTUP
```

이 단계는 Core 구조체가 초기화됐지만 Gateway 운영을 아직 시작하지 않은 상태다.

---

# 5. LINK_WAIT

`Start()` 호출 뒤 필요한 통신 조건을 기다리는 단계다.

필요 조건:

```text
Bluetooth Link AVAILABLE
UART Domain Path AVAILABLE
Current Session ACTIVE
```

셋 중 하나라도 아직 없으면 `LINK_WAIT`을 유지한다.

---

# 6. 왜 Driver Init만으로 LINK_WAIT을 끝내지 않는가?

특히 UART는:

```text
UART Peripheral Init 완료
```

와:

```text
Domain과 현재 유효 통신 가능
```

이 다르다.

따라서 `LinkStateManager`의:

```text
GATEWAY_LINK_STATE_AVAILABLE
```

상태를 사용한다.

실제 UART Availability 확인 방식은 아직 `NETWORK-TBD`다.

---

# 7. SYNCING

필수 Link와 Current Session이 확보됐지만 아직 정상 운영을 시작하기 전 단계다.

이 단계에서 논리적으로 확인해야 하는 것:

```text
현재 Bluetooth 연결
현재 Session
Domain과의 UART 통신 가능성
필요한 Vehicle State
필요한 Result / Warning / Availability
과거 실행 구간 데이터가 현재 데이터로 오인되지 않는지
```

실제 Query 메시지나 UART Sync Frame은 아직 구현하지 않는다.

---

# 8. 왜 SYNCING이 필요한가?

잘못된 흐름:

```text
UART reconnect
  ↓
UART AVAILABLE
  ↓
즉시 새 Door Unlock Request 허용
```

이렇게 하면 아직:

```text
현재 Session이 맞는가?
현재 차량 상태를 다시 받았는가?
이전 RX Buffer가 남아 있지 않은가?
```

를 확인하지 않은 상태일 수 있다.

따라서:

```text
Link Recovery
≠
READY
```

로 분리한다.

---

# 9. READY

다음이 모두 만족된 상태다.

```text
Bluetooth AVAILABLE
UART AVAILABLE
Current Session ACTIVE
Synchronization Complete
```

이 상태에서만:

```c
GatewayLifecycleManager_CanRelayNewControlRequest()
```

가 true다.

---

# 10. READY와 Function Availability는 다르다

매우 중요하다.

```text
Gateway READY
```

의 의미는:

```text
ESP32 Gateway가 현재 통신/Session 관점에서
정상 운영 준비가 됨
```

이다.

다음 의미가 아니다.

```text
Door Unlock 가능
Climate 사용 가능
Window 사용 가능
차량 전체 정상
```

차량 기능별 `Function Availability`는 Domain 책임이다.

---

# 11. DEGRADED

한 번 READY까지 도달했던 Gateway에서 중요한 통신/Session 조건이 상실된 상태다.

예:

```text
UART Path Loss
Bluetooth Disconnect
Current Session invalidation
명시적 Resynchronization 요청
```

이다.

---

# 12. 초기 실패와 DEGRADED 구분

아직 한 번도 READY가 된 적이 없고 초기 Sync 도중 Link를 잃었다면:

```text
LINK_WAIT
```

으로 돌아간다.

이미 정상 운영한 적이 있다면:

```text
DEGRADED
```

로 간다.

이를 위해:

```c
has_reached_ready
```

를 관리한다.

---

# 13. `has_reached_ready`

```text
false
→ 아직 정상 운영 준비 완료 경험 없음

true
→ 적어도 한 번 READY 도달
```

복구 상태 표현을 구분하기 위한 내부 정보다.

---

# 14. `synchronization_required`

현재 정상 운영 전 Resynchronization이 필요한지 나타낸다.

초기값:

```text
true
```

이다.

READY 진입 시:

```text
false
```

가 된다.

통신 상실/명시적 Resync 발생 시:

```text
true
```

로 돌아간다.

---

# 15. `session_reconfirmation_required`

현재 Session을 다시 확인해야 하는지를 표현한다.

Link Loss 또는 명시적 Resync에서:

```text
true
```

가 된다.

새 Current Session이 확인되고 SYNCING에 진입하면:

```text
false
```

로 바뀐다.

---

# 16. 왜 Link Loss 시 Session을 무효화하는가?

SysRS는 UART 복구 후:

```text
현재 연결·세션과 필요한 차량 상태를 다시 확인
```

해야 한다고 요구한다.

따라서 이전에:

```text
Session 10 = current
```

였더라도 UART/BT 경로가 중요한 방식으로 상실되면 그 확인을 계속 신뢰하지 않는다.

현재 DESIGN:

```text
Link Loss
  ↓
MessageContextManager_InvalidateSession()
```

이다.

---

# 17. Session 무효화의 의미

이것은:

```text
Bluetooth 등록 정보 삭제
```

라는 뜻이 아니다.

또:

```text
실제 Session이 반드시 종료됨
```

을 단정하는 것도 아니다.

의미는:

> **ESP32 Gateway가 현재 Session이라고 신뢰하던 confirmation을 다시 확인하기 전까지 사용하지 않는다.**

이다.

---

# 18. 등록 정보와 Session 확인은 별개

향후:

```text
DeviceRegistrationManager
```

는 등록된 단말을 관리한다.

Lifecycle이 Session을 무효화한다고 해서:

```text
등록된 단말 삭제
Bonding 삭제
```

를 수행하지 않는다.

---

# 19. Link Loss 처리

READY에서 UART가 끊긴 경우:

```text
READY
 ↓
UART UNAVAILABLE
 ↓
Lifecycle Update
 ↓
Session confirmation invalidated
 ↓
DEGRADED
```

그리고:

```text
CanRelayNewControlRequest = false
```

가 된다.

---

# 20. UART만 복구되면 바로 READY인가?

아니다.

```text
UART AVAILABLE
```

만 복구되어도 현재 Session은 무효화된 상태다.

따라서:

```text
DEGRADED 유지
```

한다.

---

# 21. Session 재확인

외부 Connection/Registration logic이 현재 Session을 다시 확인한 뒤:

```c
MessageContextManager_ActivateSession(...)
```

한다.

그 뒤 Lifecycle Update:

```text
Links AVAILABLE
Session ACTIVE
  ↓
SYNCING
```

으로 전환한다.

---

# 22. Sync 완료

실제 Integration Layer가 필요한 재조회/동기화를 마쳤을 때:

```c
GatewayLifecycleManager_MarkSynchronizationComplete();
```

를 호출한다.

조건:

```text
현재 상태 = SYNCING
Bluetooth AVAILABLE
UART AVAILABLE
Current Session ACTIVE
```

이다.

조건이 부족하면:

```c
GATEWAY_STATUS_NOT_READY
```

다.

---

# 23. Sync 완료 후

```text
SYNCING
  ↓
MarkSynchronizationComplete
  ↓
READY
```

가 된다.

동시에:

```text
synchronization_required = false
session_reconfirmation_required = false
has_reached_ready = true
```

로 갱신한다.

---

# 24. 실제 Sync Protocol은 아직 만들지 않음

`MarkSynchronizationComplete()`가 있다는 것은:

```text
SYNC_COMPLETE UART Message
```

가 반드시 존재한다는 뜻이 아니다.

후속 설계에서는 다음 중 하나가 될 수 있다.

```text
State Query 응답 집합
Domain Status 응답
Handshake
여러 상태 수신 완료 판단
별도 Sync message
```

현재는 `NETWORK-TBD`다.

---

# 25. `RequestResynchronization()`

다음 상황에서 사용할 수 있다.

```text
Domain restart 감지
UART 실행 구간 재시작 감지
Protocol-level resync 필요
운영 중 현재 상태 재검증 필요
```

호출:

```c
GatewayLifecycleManager_RequestResynchronization();
```

---

# 26. 명시적 Resync 동작

현재 Active Session confirmation을 무효화하고:

```text
READY → DEGRADED
```

또는 초기 단계라면:

```text
→ LINK_WAIT
```

로 이동한다.

그리고:

```text
synchronization_required = true
```

로 만든다.

---

# 27. Resync와 Request Replay

Resync가 발생해도:

```text
이전 Request 자동 재전송
```

하지 않는다.

LifecycleManager는 Request Payload를 저장하지 않는다.

---

# 28. `CanRelayNewControlRequest()`

```c
bool GatewayLifecycleManager_CanRelayNewControlRequest(void);
```

현재 구현은 단순하다.

```text
READY → true
그 외 → false
```

이다.

---

# 29. Router와 연결

이번 단계에서 `GatewayRouter`도 수정했다.

새 `MOBILE_REQUEST` 처리 시:

```text
Current Session?
  ↓
Lifecycle READY?
  ↓
UART AVAILABLE?
  ↓
Forward
```

한다.

---

# 30. 왜 Router에서 Lifecycle READY를 확인하는가?

Link와 Session만 확인하면:

```text
SYNCING
```

상태에서도 새 차량 제어 요청이 전달될 수 있다.

하지만 복구 직후에는 아직 필요한 상태 재확인이 끝나지 않았을 수 있다.

그래서:

```text
MOBILE_REQUEST
```

는 READY 전까지 차단한다.

---

# 31. STATE_QUERY는 왜 SYNCING에서도 허용하는가?

재동기화 과정에서:

```text
현재 차량 상태 조회
```

가 필요할 수 있다.

따라서 현재 Router 정책:

```text
MOBILE_REQUEST
→ READY 필요

STATE_QUERY
→ Current Session + UART AVAILABLE이면 허용

WARNING_ACK
→ Current Session + UART AVAILABLE이면 허용
```

이다.

이는 현재 DESIGN이다.

후속 Interface Contract에서 더 엄격한 Message별 정책이 정해지면 조정한다.

---

# 32. STATE_QUERY와 차량 제어는 다름

SysRS에서도 조회는 실행 재요청이 아니다.

따라서:

```text
State Query
```

를 새 차량 actuator command와 동일하게 막지 않는다.

---

# 33. WARNING_ACK도 차량 위험 해제 명령이 아님

경고 확인은:

```text
사용자가 경고를 확인했다는 의미
```

이지:

```text
실제 위험 상태 CLEAR
```

를 뜻하지 않는다.

현재 Router에서는 Control Request와 분리한다.

---

# 34. Router의 현재 MOBILE 방향 Gate

현재 순서:

```text
Message Valid?
  ↓
Current Session?
  ↓
MOBILE_REQUEST라면 Lifecycle READY?
  ↓
UART AVAILABLE?
  ↓
Context Copy
  ↓
SendToDomain
```

---

# 35. Domain → MOBILE 쪽 Lifecycle Gate

현재 Domain → MOBILE Generic Relay는:

```text
Current Session
Bluetooth AVAILABLE
```

을 확인한다.

`READY` 자체를 모든 메시지에 강제하지 않는다.

이유는 Sync 과정의 최신 State/Result 정보가 필요할 수 있기 때문이다.

---

# 36. stale Domain 메시지 방어

Lifecycle READY 여부 대신:

```text
Current Session Context
```

를 필수로 확인한다.

이전 Session의 Result/State는 Router에서 거부한다.

---

# 37. Lifecycle과 MessageContextManager 관계

```text
Lifecycle
  ↓
Session 신뢰 상실
  ↓
MessageContextManager_InvalidateSession
```

복구 후 외부에서:

```text
현재 Session 재확인
  ↓
MessageContextManager_ActivateSession
```

한다.

---

# 38. Lifecycle과 LinkStateManager 관계

Lifecycle이 직접 Timeout을 계산하지 않는다.

```text
LinkStateManager
→ Link 상태 판정

Lifecycle
→ 그 결과를 이용해 운영 상태 전환
```

이다.

---

# 39. Lifecycle과 GatewayRouter 관계

```text
Lifecycle
→ 지금 새 Control Request 가능한가?

Router
→ 그 값을 이용해 Relay Gate
```

이다.

Router가 STARTUP/SYNCING 상태 전이 자체를 관리하지 않는다.

---

# 40. Lifecycle과 Adapter 관계

Lifecycle은:

```text
Bluetooth reconnect 함수
UART reset 함수
```

를 직접 호출하지 않는다.

그 실제 Integration은 이후:

```text
Gateway_Task
BluetoothAdapter
UartAdapter
```

에서 연결한다.

---

# 41. 초기 Boot 예시

```text
Init
 ↓
STARTUP

Start
 ↓
LINK_WAIT

Bluetooth AVAILABLE
UART AVAILABLE
Session Confirmed
 ↓
SYNCING

현재 State 재확인
 ↓
MarkSynchronizationComplete
 ↓
READY
```

---

# 42. 초기 Boot 중 Link Loss

아직 READY가 된 적 없는 SYNCING 상태에서 Link가 끊기면:

```text
SYNCING
 ↓
Session confirmation invalidate
 ↓
LINK_WAIT
```

한다.

`DEGRADED`라고 부르지 않는다.

---

# 43. 정상 운영 후 Link Loss

한 번 READY 상태였다면:

```text
READY
 ↓
Link Loss
 ↓
DEGRADED
```

한다.

---

# 44. DEGRADED 복구 예시

```text
DEGRADED
 ↓
UART Recovery
 ↓
Session 아직 invalid
 ↓
DEGRADED

Session 재확인
 ↓
SYNCING

State/Context Resync 완료
 ↓
READY
```

---

# 45. Bluetooth Disconnect

같은 원칙을 적용한다.

```text
Bluetooth UNAVAILABLE
 ↓
Current Session confirmation invalidate
 ↓
DEGRADED
```

하지만 다음을 하지 않는다.

```text
Proximity = FAR
등록 삭제
Door Lock
Request FAILED
```

---

# 46. UART Loss

```text
UART UNAVAILABLE
 ↓
DEGRADED
```

하지만:

```text
차량 Request FAILED
```

를 생성하지 않는다.

---

# 47. `GatewayLifecycle_Snapshot_t`

현재 관찰 가능한 상태:

```c
initialized
started
state
state_since_ms
synchronization_required
session_reconfirmation_required
has_reached_ready
transition_count
```

이다.

---

# 48. `state_since_ms`

현재 Lifecycle State에 들어간 시각이다.

Debug/Test 및 향후 supervision 근거로 사용할 수 있다.

현재 Lifecycle Timeout 정책은 아직 추가하지 않는다.

---

# 49. `transition_count`

상태 전이가 몇 번 발생했는지 나타내는 Debug 정보다.

차량 Diagnostic Counter나 DTC가 아니다.

---

# 50. Lifecycle Timeout을 아직 두지 않는 이유

현재:

```text
SYNCING 최대 시간
LINK_WAIT 최대 시간
DEGRADED 최대 시간
```

에 대한 SysRS 확정값이 없다.

따라서 임의 Timeout을 추가하지 않는다.

필요하면 후속 설계에서 추가한다.

---

# 51. Lifecycle Manager 자체는 Event Queue를 가지지 않음

주기적:

```c
GatewayLifecycleManager_UpdateNow();
```

또는 상태 이벤트 후:

```c
GatewayLifecycleManager_UpdateAt(now);
```

를 호출한다.

실제 호출 위치는 향후 `Gateway_Task`다.

---

# 52. 예상 Gateway Task 흐름

후속 단계 예상:

```text
while (...)
{
    LinkStateManager_UpdateNow();

    process Bluetooth/UART events;

    GatewayLifecycleManager_UpdateNow();

    process Router events;
}
```

정확한 RTOS 주기와 Queue 구조는 이후 구현한다.

---

# 53. Unit Test — 초기 상태

검증:

```text
Init
→ STARTUP
→ Not Ready
```

이다.

---

# 54. Unit Test — Link 대기

Link/Session 없이 Start:

```text
STARTUP
 ↓
LINK_WAIT
```

을 확인한다.

---

# 55. Unit Test — SYNCING 진입

```text
Bluetooth AVAILABLE
UART AVAILABLE
Session ACTIVE
```

후 Start하면:

```text
SYNCING
```

인지 확인한다.

---

# 56. Unit Test — READY

SYNCING 상태에서:

```c
MarkSynchronizationComplete()
```

후:

```text
READY
CanRelayNewControlRequest = true
```

인지 확인한다.

---

# 57. Unit Test — READY 중 Link Loss

```text
READY
 ↓
UART Loss
 ↓
DEGRADED
```

을 확인한다.

동시에:

```text
Active Session = false
session_reconfirmation_required = true
```

인지 확인한다.

---

# 58. Unit Test — Link만 복구

UART를 다시 AVAILABLE로 만들어도:

```text
Session invalid
```

이므로 DEGRADED를 유지하는지 확인한다.

---

# 59. Unit Test — Session 재확인

Session을 다시 Activate한 뒤:

```text
DEGRADED → SYNCING
```

인지 확인한다.

아직 READY는 아니다.

---

# 60. Unit Test — 재동기화 완료

다시:

```c
MarkSynchronizationComplete()
```

해야 READY로 돌아가는지 확인한다.

---

# 61. Unit Test — 명시적 Resync

READY 상태에서:

```c
RequestResynchronization()
```

호출 시:

```text
Session invalidated
DEGRADED
Not Ready
```

가 되는지 확인한다.

---

# 62. Router Integration Test — Control Block

SYNCING 상태에서:

```text
MOBILE_REQUEST
```

가 들어오면:

```text
NOT_READY
SendToDomain = 0
```

인지 확인한다.

---

# 63. Router Integration Test — Query Pass

같은 SYNCING 상태에서:

```text
STATE_QUERY
```

는 Current Session과 UART가 유효하면 Domain으로 전달되는지 확인한다.

이 테스트로:

```text
Control
≠
Synchronization traffic
```

경계를 확인한다.

---

# 64. 전체 테스트 상태

이번 단계에서 다음 모듈을 모두 회귀 테스트한다.

```text
Gateway_Time
Gateway_Interface
LinkStateManager
MessageContextManager
GatewayLifecycleManager
GatewayRouter
```

모두 Compile/Test PASS여야 한다.

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
│  ├─ LinkStateManager.h/.c
│  ├─ MessageContextManager.h/.c
│  └─ GatewayRouter.h/.c
│
├─ service/
│  └─ GatewayLifecycleManager.h/.c   ✅
│
├─ docs/
│  └─ ...
│
└─ test/
   ├─ test_gateway_time.c
   ├─ test_gateway_interface.c
   ├─ test_link_state_manager.c
   ├─ test_message_context_manager.c
   ├─ test_gateway_router.c
   └─ test_gateway_lifecycle_manager.c
```

---

# 66. 다음 단계

다음 구현 대상:

```text
DeviceRegistrationManager.h/.c
```

이다.

현재 Lifecycle은:

```text
Current Session이 다시 확인됐다
```

라는 결과를 필요로 하지만,

```text
등록된 단말인가?
현재 Bluetooth peer인가?
등록 상태가 바뀌었는가?
```

는 아직 별도 모듈이 없다.

이를 다음 단계에서 구현한다.

---

# 67. DeviceRegistrationManager가 맡을 역할

예상:

```text
Registered / Not Registered / Unknown
Current Bluetooth Connection
현재 Device Context
Registration 변경
Unregistration
Reconnect 후 current peer 재확인
Domain에 Registration/Connection Context Publish
```

---

# 68. Lifecycle과 DeviceRegistrationManager 연결

향후 흐름:

```text
Bluetooth Adapter
       ↓
DeviceRegistrationManager
       ↓
현재 등록/연결 Context 확인
       ↓
MessageContextManager Session 활성화에 필요한 근거 제공
       ↓
GatewayLifecycleManager
       ↓
SYNCING
```

실제 Session ID 생성 주체/형식은 아직 TBD로 유지한다.

---

# 69. 핵심 정리

`GatewayLifecycleManager`를 한 문장으로 정리하면:

> **ESP32 Gateway가 단순히 통신 경로가 살아 있는 것을 넘어, 현재 Session과 복구 후 동기화까지 완료해 새 차량 제어 요청을 안전하게 중계할 준비가 되었는지를 관리하는 Service 모듈이다.**

가장 중요한 규칙:

```text
Link AVAILABLE
≠
Gateway READY

Reconnect
≠
이전 Session 그대로 신뢰

Recovery
→ Session 재확인 필요
→ Resynchronization 필요

SYNCING
→ 새 MOBILE control request 차단

READY
→ 새 MOBILE control request relay 가능

Resync
≠
Old Request Replay
```


---

# Device Registration Integration Addendum

v0.2부터 READY 전제조건에 현재 Bluetooth peer 인증 확인이 추가되었다.

```text
Bluetooth AVAILABLE
UART AVAILABLE
REGISTERED + CONNECTED current peer
Current Session ACTIVE
Synchronization Complete
        ↓
READY
```

현재 peer가 등록되지 않았거나 등록이 제거되면 READY/SYNCING 상태를 유지하지 않는다.

```text
Registration loss
      ↓
Authenticated = false
      ↓
Session confirmation invalidation
      ↓
DEGRADED 또는 LINK_WAIT
```

단, 등록 제거 자체가 Bluetooth bonding key 삭제 구현을 대신하는 것은 아니다. 실제 등록 절차와 내부 key 관리는 Bluetooth 계층 책임이다.

Lifecycle Update가 Event 직후 아직 수행되지 않은 짧은 구간에도 새 `MOBILE_REQUEST`가 통과하지 않도록 `GatewayRouter`가 `DeviceRegistrationManager_IsCurrentPeerAuthenticated()`를 추가로 확인한다.
