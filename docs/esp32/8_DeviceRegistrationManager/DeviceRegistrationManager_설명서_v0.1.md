# DeviceRegistrationManager 설계 설명서 v0.1

> 대상 코드  
> - `ESP32/feature/DeviceRegistrationManager.h`  
> - `ESP32/feature/DeviceRegistrationManager.c`
>
> 관련 테스트  
> - `ESP32/test/test_device_registration_manager.c`
>
> 연동 변경  
> - `GatewayLifecycleManager`
> - `GatewayRouter`
>
> 기준 문서  
> - `ESP32 Wireless Gateway Software Architecture v0.3`
> - 현재 SR / SysRS의 Bluetooth 기기 등록·현재 연결 책임
> - `GatewayLifecycleManager 설계`
> - `GatewayRouter 설계`

---

# 1. 목적

`DeviceRegistrationManager`는 ESP32가 담당하는 Bluetooth 측:

```text
기기 등록 상태
+
현재 Bluetooth peer 연결 상태
```

를 관리하는 Feature Manager다.

현재 프로젝트에서 차량 측 Bluetooth 등록·현재 연결 확인은 ESP32 책임으로 배정되어 있다.

하지만 ESP32가 판단하는 것은:

```text
이 peer가 차량 Bluetooth 등록 절차를 거친 단말인가?
현재 이 peer가 실제로 연결되어 있는가?
```

까지다.

다음 판단은 ESP32가 하지 않는다.

```text
이 요청을 차량이 허용할 것인가?
Door Unlock을 실행할 것인가?
Climate를 사용할 수 있는가?
```

이러한 차량 수준 판단은 Domain이 담당한다.

---

# 2. 가장 중요한 구분

다음 두 상태는 반드시 따로 본다.

```text
REGISTERED
CONNECTED
```

예:

```text
등록된 휴대폰 A가 존재하지만
현재 연결은 끊긴 상태
```

이면:

```text
REGISTERED
DISCONNECTED
```

가 가능하다.

반대로:

```text
등록되지 않은 휴대폰 B가
Bluetooth로 현재 연결됨
```

이면:

```text
NOT_REGISTERED
CONNECTED
```

이다.

따라서:

```text
CONNECTED
≠
REGISTERED
```

이다.

---

# 3. AUTHENTICATED 의미

현재 SysRS의 인증 의미에 맞춰 Gateway 내부에서는:

```text
REGISTERED
+
CONNECTED
```

를 현재 peer가 인증된 연결인지 확인하는 기본 근거로 사용한다.

API:

```c
DeviceRegistrationManager_IsCurrentPeerAuthenticated();
```

이다.

---

# 4. 등록과 차량 실행 허용은 다르다

다음 상태:

```text
REGISTERED
CONNECTED
```

가 확인되어도 ESP32가:

```text
Door Unlock 허용
```

이라고 판단하지 않는다.

전체 판단 경계:

```text
ESP32
→ 등록/현재 연결 확인

Domain
→ 현재 Session
→ 기능 지원
→ 차량 상태
→ 허용 조건
→ 실행 가능 여부
```

이다.

---

# 5. Bluetooth 이름이나 주소만으로 등록하지 않는다

Manager API는 다음 값을 입력으로 받는다.

```c
Gateway_ByteView_t device_context_ref;
```

이 값은 Bluetooth 계층이 실제 등록 절차를 통해 확인한:

```text
논리 Device Context Reference
```

여야 한다.

다음만 보고 등록 성공을 만들면 안 된다.

```text
Bluetooth Device Name 동일
표시 주소 동일
사용자가 입력한 문자열 동일
```

---

# 6. 내부 Bonding Key를 Manager에 넣지 않는다

기기 등록용 내부 key는 Bluetooth 연결 계층이 관리한다.

따라서:

```text
Bonding key
Secret
Link key
Internal security key
```

자체를 `DeviceRegistrationManager`에 전달하거나 Domain에 보내지 않는다.

Manager가 보관하는 것은:

```text
등록된 peer를 논리적으로 재식별하기 위한 reference
```

다.

---

# 7. Device Context Reference 형식

현재 실제 형식은 `TBD`다.

가능한 예를 미리 확정하지 않는다.

따라서:

```c
uint32_t device_id;
```

처럼 고정하지 않고:

```text
opaque bytes + length
```

형태로 관리한다.

---

# 8. 저장 구조

```c
typedef struct
{
    uint8_t data[DEVICE_REGISTRATION_MANAGER_MAX_REF_BYTES];
    size_t length;
} DeviceRegistrationManager_DeviceRef_t;
```

를 사용한다.

기본 Buffer 상한:

```text
64 byte
```

다.

---

# 9. 64 byte는 Protocol 규격이 아니다

이 값은:

```text
Device ID = 64 byte
```

를 의미하지 않는다.

목적:

```text
Dynamic Allocation 없이
현재 구현에서 Reference를 안전하게 복사
```

하기 위한 Storage Budget이다.

실제 Protocol이 정해지면 조정 가능하다.

---

# 10. 왜 입력 Pointer를 그대로 저장하지 않는가?

Bluetooth Event Buffer는 Callback 종료 후 재사용될 수 있다.

잘못된 예:

```c
saved_ref = event->peer_ref.data;
```

이렇게 포인터만 저장하면 나중에 다른 값으로 덮일 수 있다.

현재 구현:

```text
Bluetooth View
     ↓
실제 byte copy
     ↓
Manager-owned Storage
```

를 사용한다.

---

# 11. 관리하는 두 Peer Context

Manager는 다음 두 개를 분리한다.

```text
registered_peer
current_peer
```

---

# 12. `registered_peer`

차량 측 Bluetooth 등록 절차를 완료한 peer reference다.

현재 demo 구조에서는 Manager 내부에 한 개의 registered peer record를 보관하는 형태다.

향후 다중 등록 단말 요구가 생기면 Storage 구조 확장이 필요하다.

---

# 13. `current_peer`

현재 실제 Bluetooth 연결에서 확인된 peer reference다.

`registered_peer`와 다를 수 있다.

예:

```text
registered_peer = A
current_peer    = B
```

이면:

```text
B는 현재 연결됐지만 등록 단말은 아님
```

이다.

---

# 14. 현재 Registration 상태 계산

현재 peer가 연결되어 있으면:

```text
current_peer == registered_peer
→ REGISTERED

current_peer != registered_peer
→ NOT_REGISTERED
```

로 계산한다.

---

# 15. 연결이 없을 때 Registration 상태

연결이 끊겼지만 등록 record가 남아 있으면:

```text
REGISTERED
DISCONNECTED
```

로 표현한다.

이는:

```text
등록된 단말 기록은 존재
현재 연결은 없음
```

이라는 뜻이다.

---

# 16. 연결 없이 REGISTERED가 AUTHENTICATED인가?

아니다.

`IsCurrentPeerAuthenticated()`는:

```text
registration == REGISTERED
AND
connection == CONNECTED
```

를 모두 확인한다.

따라서:

```text
REGISTERED + DISCONNECTED
```

는 false다.

---

# 17. 초기 상태

Init 직후:

```text
Registration = UNKNOWN
Connection   = UNKNOWN
Quality      = UNKNOWN
```

이다.

초기부터:

```text
NOT_REGISTERED
DISCONNECTED
```

라고 단정하지 않는다.

아직 실제 Bluetooth 상태 관측이 없기 때문이다.

---

# 18. Registration Confirmed Event

Bluetooth 계층이 등록 완료를 확인하면:

```c
DeviceRegistrationManager_OnRegistrationConfirmed(...)
```

을 호출한다.

처리:

```text
registered_peer 저장
Registration 상태 재계산
새 Observation 생성
```

이다.

---

# 19. Registration Confirmed만으로 CONNECTED를 만들지 않음

등록 성공 이벤트만 받았다고:

```text
CONNECTED
```

를 자동 생성하지 않는다.

현재 Connection 상태는 별도 연결 이벤트가 제공해야 한다.

---

# 20. Peer Connected Event

현재 연결 peer가 확인되면:

```c
DeviceRegistrationManager_OnPeerConnected(...)
```

을 호출한다.

처리:

```text
current_peer 저장
Connection = CONNECTED
registered_peer와 비교
Registration 상태 계산
Observation 갱신
```

이다.

---

# 21. 등록된 Peer가 연결된 예

```text
registered_peer = A
current_peer = A
```

결과:

```text
REGISTERED
CONNECTED
Authenticated = true
```

이다.

---

# 22. 다른 Peer가 연결된 예

```text
registered_peer = A
current_peer = B
```

결과:

```text
NOT_REGISTERED
CONNECTED
Authenticated = false
```

이다.

이 구분은 매우 중요하다.

---

# 23. 왜 `REGISTERED + CONNECTED`로 만들면 안 되는가?

차량에 등록 단말 A가 있다는 사실과:

```text
지금 연결된 상대가 A다
```

라는 사실은 다르다.

B가 연결됐는데도:

```text
registered record 존재
→ REGISTERED
current link 존재
→ CONNECTED
```

처럼 각각 따로 보고 합치면 B를 인증된 단말로 오인할 수 있다.

현재 Manager는 **peer identity까지 비교**한다.

---

# 24. Peer Disconnected Event

```c
DeviceRegistrationManager_OnPeerDisconnected();
```

처리:

```text
current_peer 삭제
Connection = DISCONNECTED
registered_peer record 유지
Registration 재계산
```

이다.

---

# 25. Disconnect가 등록 삭제를 의미하지 않는다

```text
Bluetooth Disconnect
≠
Unregister
```

따라서 등록된 peer A가 끊기면:

```text
REGISTERED
DISCONNECTED
```

가 된다.

---

# 26. Disconnect와 Proximity

이 Manager는:

```text
DISCONNECTED
→ FAR
```

를 생성하지 않는다.

Proximity는 `ProximityManager` 책임이다.

통신 상실은 실제 단말의 물리적 이탈 확인이 아니다.

---

# 27. Disconnect와 App Active

또한:

```text
DISCONNECTED
→ APP_INACTIVE
```

라고 추론하지 않는다.

App Activity의 Producer/획득 방식은 별도 `INPUT-TBD`다.

---

# 28. Registration Removed Event

차량 측 등록 기록이 제거되면:

```c
DeviceRegistrationManager_OnRegistrationRemoved();
```

을 호출한다.

처리:

```text
registered_peer 삭제
Registration 재계산
Observation 갱신
```

이다.

---

# 29. 연결 중 등록 해제

현재 peer가 연결되어 있어도 등록 기록이 제거되면:

```text
NOT_REGISTERED
CONNECTED
Authenticated = false
```

가 된다.

즉 물리 연결과 등록 유효성을 분리한다.

---

# 30. 등록 해제와 물리 Disconnect

`OnRegistrationRemoved()` 자체가 Bluetooth Driver의 실제 disconnect 함수를 호출하지 않는다.

이 Manager는 State Manager다.

실제 Bluetooth 연결 종료가 요구된다면:

```text
Gateway Task
BluetoothAdapter
```

가 수행해야 한다.

---

# 31. 등록 해제 후 새 제어 중단

등록이 해제되면:

```text
Authenticated = false
```

가 된다.

이번 연동에서:

```text
GatewayLifecycleManager
GatewayRouter
```

둘 다 이를 새 control request 허용 조건에 반영한다.

---

# 32. Lifecycle 연동

`GatewayLifecycleManager`의 READY 전제조건은 이제:

```text
Bluetooth AVAILABLE
UART AVAILABLE
Current Peer Authenticated
Current Session ACTIVE
Synchronization Complete
```

이다.

---

# 33. 등록 상실 중 READY 상태

정상 운영 중:

```text
READY
```

에서 등록이 제거되고 Lifecycle Update가 수행되면:

```text
Authenticated = false
        ↓
Session confirmation invalidate
        ↓
DEGRADED
```

로 전환한다.

---

# 34. 왜 Session도 무효화하는가?

등록/현재 연결은 Session 신뢰의 중요한 근거다.

등록이 더 이상 유효하지 않은데 기존 Session을 계속:

```text
Current Session
```

으로 유지하면 안 된다.

따라서 Lifecycle에서 다시 확인하도록 한다.

---

# 35. Router의 이중 방어

Event 처리 순서에 따라:

```text
Registration Removed
```

직후 Lifecycle Update가 아직 실행되지 않았을 가능성도 있다.

그래서 Router에서도 새 `MOBILE_REQUEST`에 대해:

```text
IsCurrentPeerAuthenticated()
```

를 직접 확인한다.

---

# 36. Router Control Gate

현재:

```text
MOBILE_REQUEST
  ↓
Current Session?
  ↓
Registered + Connected?
  ↓
Lifecycle READY?
  ↓
UART AVAILABLE?
  ↓
Forward
```

이다.

---

# 37. 이 Gate가 차량 수준의 허용 판단인가?

아니다.

ESP32가 확인하는 것은:

```text
요청 출처/현재 Bluetooth 연결 근거
```

이다.

Domain은 여전히 다음을 별도로 판단한다.

```text
기능 지원
현재 차량 상태
Door/Window/Climate 조건
자동 기능 정책
실행 허용 조건
Function Availability
```

이다.

---

# 38. STATE_QUERY와 Registration

현재 Router 정책에서는 새 actuator control인 `MOBILE_REQUEST`에 이 Auth gate를 명시 적용한다.

`STATE_QUERY / WARNING_ACK`의 세부 인증 조건은 후속 Interface Contract에서 더 세분화할 수 있다.

차량의 안전한 상태 조회가 어떤 연결 단계에서 필요한지 실제 Protocol과 함께 확정해야 한다.

---

# 39. Domain Publish

ESP32는 등록/현재 연결 결과를 Domain에 제공해야 한다.

현재 API:

```c
DeviceRegistrationManager_PublishToDomain(...)
```

이다.

내부적으로:

```text
BuildDomainUpdate
        ↓
Gateway_Interface_PublishRegistrationConnection
```

을 호출한다.

---

# 40. Domain Update 내용

```c
Gateway_RegistrationConnectionUpdate_t
```

에 다음을 넣는다.

```text
Registration
Connection
Device Context Reference
Quality
Age
New Update Evidence
```

---

# 41. 어떤 Device Reference를 Domain에 넣는가?

현재 연결 peer가 있으면:

```text
current_peer
```

를 사용한다.

이것이 중요하다.

예:

```text
registered A
current B
```

이면 Domain update는:

```text
device = B
registration = NOT_REGISTERED
connection = CONNECTED
```

가 된다.

즉 A가 등록되어 있다는 이유로 현재 B를 A처럼 보이지 않는다.

---

# 42. 연결이 없을 때

현재 peer는 없지만 등록 record A가 있으면:

```text
device = A
registration = REGISTERED
connection = DISCONNECTED
```

로 전달할 수 있다.

---

# 43. 등록도 연결도 없을 때

Device Context는 empty view를 사용한다.

실제 Wire에서 empty를 어떤 Field로 표현할지는 `NETWORK-TBD`다.

---

# 44. base context

`BuildDomainUpdate()`는:

```c
const Gateway_MessageContextView_t *base_context
```

를 받는다.

base의:

```text
vehicle_id
session_id
request_id
```

는 유지하고,

```text
device_context_id
```

만 Manager의 현재 보고 peer로 설정한다.

---

# 45. 왜 Device Context를 Manager가 넣는가?

이 정보의 Owner가 ESP32 Bluetooth 등록/연결 계층이기 때문이다.

Caller가 임의 device ID를 넣어 현재 peer 확인 결과를 바꾸지 않도록 한다.

---

# 46. 실제 Wire Format은 아직 없음

다음은 아직 정하지 않는다.

```text
Device Context Field Byte Length
UART Offset
Message ID
CRC
Sequence
Bluetooth UUID
```

Logical Update만 정의한다.

---

# 47. 품질 정보

Manager가 실제 Bluetooth registration/connection event를 관찰하면:

```text
Quality = VALID
```

로 갱신한다.

Init 직후:

```text
Quality = UNKNOWN
```

이다.

---

# 48. `last_observed_ms`

가장 최근 실제 등록/연결 상태 관측 시각이다.

예:

```text
110 ms CONNECTED 확인
현재 150 ms
```

이면:

```text
age = 40 ms
```

로 Build한다.

---

# 49. Freshness Reset 주의

단순히 같은 상태를 Domain에 다시 보냈다고:

```text
새 관측
```

으로 만들면 안 된다.

따라서 Manager는:

```text
observation_revision
last_published_revision
```

을 따로 관리한다.

---

# 50. 첫 실제 Observation

예:

```text
Registration confirmed
Peer connected
```

이벤트를 받으면:

```text
observation_revision++
```

한다.

---

# 51. Publish 성공

새 Observation 후 첫 Publish:

```text
is_new_update = true
```

이다.

Publish가 성공하면:

```text
last_published_revision =
observation_revision
```

으로 기록한다.

---

# 52. 같은 값을 재전송

새 관측 없이 단순 Publish를 다시 하면:

```text
is_new_update = false
```

이다.

즉 Relay/재전송으로 Freshness를 새로 만들지 않는다.

---

# 53. 값은 같지만 실제 새 관측

Bluetooth 계층이 상태를 실제로 다시 확인했다면:

```c
DeviceRegistrationManager_MarkCurrentStateObserved();
```

를 호출할 수 있다.

값이:

```text
REGISTERED + CONNECTED
```

로 그대로여도:

```text
새 Observation
```

이므로 다음 Publish에서:

```text
is_new_update = true
age = 새 관측 기준
```

이 된다.

---

# 54. 왜 별도 Observation API가 필요한가?

다음은 다르다.

```text
같은 값을 단순 재전송
```

과:

```text
실제로 다시 확인했는데 결과가 동일
```

후자의 경우 새 관측 근거가 있다.

---

# 55. Publish 실패

`Gateway_Interface_PublishRegistrationConnection()`이 실패하면:

```text
last_published_revision
```

을 갱신하지 않는다.

즉 새 Observation 근거가 전송 성공으로 소비되지 않는다.

---

# 56. Publish 실패가 차량 결과인가?

아니다.

```text
Gateway publish failure
≠
Vehicle Request FAILED
```

이다.

통신/Interface 내부 결과일 뿐이다.

---

# 57. LinkStateManager와의 관계

두 Manager는 다른 사실을 관리한다.

```text
LinkStateManager
→ Bluetooth 경로가 현재 사용 가능한가?

DeviceRegistrationManager
→ 연결된 peer가 등록된 peer인가?
```

둘 다 필요하다.

---

# 58. Bluetooth Adapter Event 예시

실제 Integration 예상:

```text
BT peer connected
   ↓
LinkStateManager_MarkAvailable(BLUETOOTH)
   ↓
DeviceRegistrationManager_OnPeerConnected(peer_ref)
```

순서/동기화는 이후 `Gateway_Task`에서 정한다.

---

# 59. Disconnect Event 예시

```text
BT disconnect
   ↓
LinkStateManager_MarkUnavailable(BLUETOOTH)
   ↓
DeviceRegistrationManager_OnPeerDisconnected()
   ↓
GatewayLifecycleManager_Update()
```

결과적으로 Session 재확인이 필요해질 수 있다.

---

# 60. 등록 해제 Event 예시

```text
Unregister confirmed
   ↓
DeviceRegistrationManager_OnRegistrationRemoved()
   ↓
Publish Registration/Connection
   ↓
Lifecycle Update
   ↓
DEGRADED
```

실제 Bluetooth pairing record 삭제는 Bluetooth layer가 수행한다.

---

# 61. App Active와 분리

Manager에는 다음 API가 없다.

```text
SetAppActive()
GetAppActive()
```

의도적이다.

```text
Bluetooth CONNECTED
≠
APP ACTIVE
```

이기 때문이다.

---

# 62. Proximity와 분리

Manager에는 다음도 없다.

```text
SetNear()
SetFar()
```

의도적이다.

```text
Bluetooth CONNECTED
≠
NEAR
Bluetooth DISCONNECTED
≠
FAR
```

이다.

---

# 63. Session과 분리

Manager가:

```text
SessionId 생성
```

하지 않는다.

Session ID의 생성 주체/폭/수명은 아직 TBD다.

현재 Session 보관은 `MessageContextManager`가 담당한다.

---

# 64. 등록 상태와 Session의 관계

향후 Integration은 대략:

```text
Registered Current Peer
        +
현재 연결 Context
        +
Session 확인
        ↓
MessageContextManager Activate
        ↓
Lifecycle SYNCING
```

이다.

하지만 DeviceRegistrationManager 자체가 자동으로 Session을 Activate하지 않는다.

---

# 65. 왜 자동 Session Activate를 하지 않는가?

현재:

```text
Session ID 생성 방식
Session ID 수명
Connection과 Session의 1:1 관계
```

가 아직 확정되지 않았기 때문이다.

등록 Manager가 이를 임의 결정하면 안 된다.

---

# 66. 등록 해제 시 신규 제어 중단

현재 Source 요구상 등록 해제 후에는 새 제어/디지털 키 응답 유지가 중단되어야 한다.

이번 구현에서는:

```text
Authenticated=false
        ↓
Router new MOBILE_REQUEST block
        ↓
Lifecycle DEGRADED on update
```

로 신규 제어 경로를 차단한다.

실제 Digital Key Proximity/Response 경로는 다음 `ProximityManager`와 Integration 단계에서 동일 원칙을 적용한다.

---

# 67. 다중 등록 단말

현재 Manager는:

```text
1 registered peer reference
```

구조다.

현재 프로젝트 demo 범위에서 단순화한 DESIGN이다.

다중 Digital Key 단말을 동시에 등록해야 한다면:

```text
Registered Peer Table
```

구조로 확장해야 한다.

이는 현재 SysRS에서 구체적으로 확정된 저장 개수/정책이 아니므로 지금 추가하지 않는다.

---

# 68. 등록 Reference Persistence

Manager 자체는 NVS/Persistence를 구현하지 않는다.

실제 Bluetooth bonding/registration record는 Bluetooth layer가 관리한다.

향후 별도 차량 측 논리 reference persistence가 필요하면 실제 요구 확인 후 추가한다.

---

# 69. Global PersistenceManager를 만들지 않음

현재 요구만으로 ESP32 전체 Global PersistenceManager를 추가하지 않는다.

등록 내부 key는 Bluetooth 계층이 관리하고,
추가 영속 데이터 요구가 확인될 때 설계한다.

---

# 70. Thread Safety

현재 Mutex가 없다.

기본 구조:

```text
Bluetooth Event
   ↓
Queue
   ↓
Gateway Task
   ↓
DeviceRegistrationManager
```

를 권장한다.

---

# 71. Callback/ISR에서 최소 처리

Bluetooth Stack Callback에서 복잡한 상태 관리와 Domain Publish까지 직접 하지 않는 것이 좋다.

권장:

```text
BT callback
 ↓
event + copied reference
 ↓
Queue
 ↓
Gateway Task
 ↓
Manager Update
 ↓
Domain Publish
 ↓
Lifecycle Update
```

이다.

---

# 72. Unit Test — Init

확인:

```text
Registration UNKNOWN
Connection UNKNOWN
Quality UNKNOWN
Authenticated false
```

이다.

---

# 73. Unit Test — Registered but not connected

Registration Confirmed만 호출하고:

```text
REGISTERED
Connection은 자동 CONNECTED 아님
Authenticated false
```

를 확인한다.

---

# 74. Unit Test — Registered peer connected

같은 peer를 등록 후 연결하면:

```text
REGISTERED
CONNECTED
Authenticated true
```

인지 확인한다.

---

# 75. Unit Test — Other peer connected

등록 A, 연결 B:

```text
NOT_REGISTERED
CONNECTED
Authenticated false
```

인지 확인한다.

---

# 76. Unit Test — Buffer Copy

입력 peer reference 배열을 이벤트 처리 후 수정해도 Manager 내부 등록 reference가 변경되지 않는지 확인한다.

---

# 77. Unit Test — Disconnect

등록 A + 연결 A 후 Disconnect:

```text
REGISTERED
DISCONNECTED
registered record 유지
Authenticated false
```

를 확인한다.

---

# 78. Unit Test — Unregister while connected

등록 A + 연결 A 상태에서 등록을 제거하면:

```text
NOT_REGISTERED
CONNECTED
Authenticated false
```

를 확인한다.

---

# 79. Unit Test — Domain Update Current Peer

등록 A, 현재 연결 B인 경우 Publish DTO가:

```text
device = B
registration = NOT_REGISTERED
connection = CONNECTED
```

인지 확인한다.

---

# 80. Unit Test — Freshness / New Observation

다음 흐름을 검증한다.

```text
새 관측
→ Publish
→ is_new_update true

관측 없이 재Publish
→ false

같은 상태 실제 재확인
→ 다음 Publish true
```

이다.

---

# 81. Unit Test — Oversize Reference

Storage 상한을 넘으면:

```text
UNSUPPORTED
```

를 반환한다.

기존 registered peer record를 부분적으로 덮어쓰지 않는지 확인한다.

---

# 82. Lifecycle Integration Test

READY 상태에서 등록을 제거하고 Lifecycle Update:

```text
READY
→ DEGRADED
Session invalidated
New Control disabled
```

를 확인한다.

---

# 83. Router Integration Test

등록 제거 직후 Lifecycle Update 전이라도:

```text
MOBILE_REQUEST
→ Router Auth Gate
→ NOT_READY
```

인지 확인한다.

이는 Event scheduling 사이의 짧은 틈에도 새 control request가 통과하지 않게 하는 방어다.

---

# 84. 현재 전체 구조

```text
ESP32/
├─ common/
│  ├─ Gateway_Types.h
│  └─ Gateway_Time.h/.c
│
├─ interface/
│  └─ Gateway_Interface.h/.c
│
├─ core/
│  ├─ LinkStateManager.h/.c
│  ├─ MessageContextManager.h/.c
│  └─ GatewayRouter.h/.c
│
├─ service/
│  └─ GatewayLifecycleManager.h/.c
│
├─ feature/
│  └─ DeviceRegistrationManager.h/.c   ✅
│
└─ test/
   └─ ...
```

---

# 85. 다음 단계

다음 Feature는:

```text
ProximityManager.h/.c
```

이다.

---

# 86. ProximityManager에서 연결할 내용

현재 Architecture 기준:

```text
Bluetooth RSSI       PROVISIONAL input
      ↓
Validity
      ↓
Filter / Stability   TBD
      ↓
NEAR / FAR / UNKNOWN
      ↓
Quality
      ↓
Update/Age basis
      ↓
Gateway_Interface_PublishProximity()
      ↓
Domain
```

이다.

---

# 87. DeviceRegistration과 Proximity 관계

Proximity는 현재 등록된 연결 Context와 함께 봐야 한다.

하지만:

```text
등록됨
≠
NEAR

연결됨
≠
NEAR
```

이다.

따라서 별도 Feature Manager로 유지한다.

---

# 88. Digital Key 최종 판단

ESP32가 Domain에 제공하는 것:

```text
Registered / Current Connection
Proximity NEAR/FAR/UNKNOWN
Quality / Update basis
```

Domain이 판단하는 것:

```text
Auto Unlock setting
New Approach
Door State
Permission
Function Availability
최종 BCM Unlock Command
```

이다.

---

# 89. 핵심 정리

`DeviceRegistrationManager`를 한 문장으로 표현하면:

> **Bluetooth 등록 절차가 확인한 단말 reference와 현재 연결 peer를 별도로 보관·비교하여, 현재 연결이 실제 등록된 단말인지 ESP32가 증명하고 그 결과를 Domain에 제공하는 Feature Manager다.**

가장 중요한 규칙:

```text
CONNECTED
≠
REGISTERED

등록 record A
+
현재 peer B
→ NOT_REGISTERED + CONNECTED

이름/주소 동일
≠
등록 확인

Bonding Key
→ Bluetooth layer 내부
→ Gateway/Domain에 노출하지 않음

REGISTERED + CONNECTED
→ Gateway-side authenticated connection

Authenticated
≠
Vehicle execution permission

Disconnect
≠
Unregister
≠
FAR
≠
APP_INACTIVE
```
