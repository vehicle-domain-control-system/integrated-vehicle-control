# LinkStateManager 설계 설명서 v0.1

> 대상 코드  
> - `ESP32/core/LinkStateManager.h`  
> - `ESP32/core/LinkStateManager.c`
>
> 관련 테스트  
> - `ESP32/test/test_link_state_manager.c`
>
> 기준 문서  
> - `ESP32 Wireless Gateway Software Architecture v0.3`
> - `Gateway_Types.h 설계 설명서 v0.1`
> - `Gateway_Time 설계 설명서 v0.1`
> - `Gateway_Interface 설계 설명서 v0.1`

---

# 1. 목적

`LinkStateManager`는 ESP32 Wireless Gateway가 사용하는 두 통신 경로를 독립적으로 관리한다.

현재 관리 대상:

```text
MOBILE
  │
Bluetooth
  │
ESP32
  │
UART
  │
S32K344 Domain
```

즉:

```text
Bluetooth Link
Domain UART Path
```

두 경로의 상태를 따로 관리한다.

핵심 목적은 다음과 같다.

```text
1. Bluetooth와 UART 상태를 섞지 않음
2. 현재 통신 경로를 사용할 수 있는지 관리
3. 마지막 유효 수신 시각 관리
4. Timeout 기준이 확정된 경우 통신 상실 감시
5. 재연결/복구 과정에서 이전 통신 근거를 현재 연결에 재사용하지 않음
```

---

# 2. 왜 Link 상태를 따로 관리해야 하는가?

Bluetooth가 연결되어 있어도 UART가 끊길 수 있다.

예:

```text
MOBILE
  │
Bluetooth CONNECTED
  ▼
ESP32
  │
UART UNAVAILABLE
  X
Domain
```

이 상태에서:

```text
Bluetooth가 연결됨
```

만 보고 사용자에게:

```text
차량과 정상 연결됨
```

이라고 판단하면 잘못이다.

반대로 UART가 정상이어도 MOBILE Bluetooth가 끊길 수 있다.

따라서:

```text
Bluetooth State
UART State
```

는 반드시 독립적으로 관리해야 한다.

---

# 3. LinkStateManager의 책임

현재 Manager 책임:

```text
Bluetooth 상태
UART Path 상태

상태 변경 시각
마지막 Valid Rx 시각

Timeout 설정
Timeout 감시

Recovery 상태
```

---

# 4. 하지 않는 것

이 Manager는 다음을 판단하지 않는다.

```text
Bluetooth 등록 여부
App Active 여부
RSSI → NEAR/FAR 판단
Door Unlock 가능 여부
Request ACCEPTED
Request DONE
Request FAILED
Warning 발생
UART Frame CRC 검증
Bluetooth Packet Decode
재연결 횟수
재연결 Backoff
```

즉:

```text
LinkStateManager
=
통신 경로 상태 관리
```

로 제한한다.

---

# 5. 상태 정의

기존 `Gateway_Types.h`의:

```c
typedef enum
{
    GATEWAY_LINK_STATE_UNKNOWN = 0,
    GATEWAY_LINK_STATE_UNAVAILABLE,
    GATEWAY_LINK_STATE_AVAILABLE,
    GATEWAY_LINK_STATE_RECOVERING
} Gateway_LinkState_t;
```

를 사용한다.

---

# 6. UNKNOWN

```text
현재 경로 상태를 아직 판단하지 못함
```

대표 상황:

```text
Boot 직후
Adapter 초기화 전
UART Domain 통신 확인 전
Bluetooth 상태 이벤트 수신 전
```

초기 상태는 두 Link 모두:

```text
UNKNOWN
```

이다.

---

# 7. AVAILABLE

현재 해당 경로를 사용할 수 있다고 확인된 상태다.

예:

```text
Bluetooth:
연결 이벤트 및 현재 유효 연결 확인

UART:
후속 프로토콜 기준 Domain 통신 가능 확인
또는 유효한 Domain Message 수신
```

중요:

```text
UART Driver Init
≠
UART Path AVAILABLE
```

단순히 UART Peripheral이 초기화됐다는 이유만으로 Domain과 통신 가능하다고 판단하지 않는다.

---

# 8. UNAVAILABLE

현재 해당 경로를 사용할 수 없다고 판단한 상태다.

예:

```text
Bluetooth Disconnect
UART 통신 상실
확정된 Timeout 초과
Adapter가 명시적으로 경로 상실 통보
```

중요:

```text
UNAVAILABLE
≠
차량 Request FAILED
```

통신이 끊긴 사실과 차량 기능 수행 결과는 다른 의미다.

---

# 9. RECOVERING

경로가 끊긴 후 복구 중임을 나타낸다.

예:

```text
UNAVAILABLE
   ↓
Reconnect / Re-init 시작
   ↓
RECOVERING
   ↓
통신 확인
   ↓
AVAILABLE
```

RECOVERING 자체에서:

```text
차량 Request Replay
```

같은 동작을 만들지 않는다.

복구 이후 이전 Request 처리 정책은 상위 Manager가 담당한다.

---

# 10. 기본 상태 전이

개념적인 상태 전이:

```text
              valid path
UNKNOWN ───────────────────→ AVAILABLE
   │                            │
   │ unavailable                │ loss / timeout
   ▼                            ▼
UNAVAILABLE ←────────────── AVAILABLE
   │
   │ recovery start
   ▼
RECOVERING
   │
   │ valid rx / path confirmed
   ▼
AVAILABLE
```

실제 Adapter 상태 이벤트에 따라:

```text
UNKNOWN → RECOVERING
```

등도 가능하다.

---

# 11. `LinkStateManager_TimeoutConfig_t`

```c
typedef struct
{
    bool enabled;
    Gateway_TimeMs_t timeout_ms;
} LinkStateManager_TimeoutConfig_t;
```

각 Link별 Timeout 감시 설정이다.

---

# 12. 왜 Timeout 기본값이 없는가?

현재 다음 값은 아직 네트워크 상세 설계 대상이다.

```text
Bluetooth Update Period
Bluetooth Link Timeout
UART Expected Period
UART Timeout
Recovery 조건
```

따라서 코드가 임의로:

```text
UART Timeout = 100 ms
```

같은 값을 확정하지 않는다.

초기값:

```text
enabled = false
```

다.

즉 Timeout을 설정하지 않으면:

```text
시간이 오래 지났다는 이유만으로
AVAILABLE → UNAVAILABLE
```

전이를 만들지 않는다.

---

# 13. Timeout 활성화

예:

```c
LinkStateManager_TimeoutConfig_t uart_timeout = {
    .enabled = true,
    .timeout_ms = 500U
};

LinkStateManager_ConfigureTimeout(
    GATEWAY_LINK_DOMAIN_UART,
    uart_timeout);
```

이 값은 예시일 뿐 현재 프로젝트 확정값이 아니다.

실제 값은 `Network_Config`가 정해진 뒤 주입해야 한다.

---

# 14. Timeout 0 금지

다음은 거부한다.

```c
.enabled = true
.timeout_ms = 0
```

이유:

```text
0 ms Timeout
→ AVAILABLE 직후 즉시 UNAVAILABLE
```

같은 실수를 막기 위해서다.

Timeout을 사용하지 않으려면:

```c
.enabled = false
```

로 둔다.

---

# 15. `LinkStateManager_LinkSnapshot_t`

```c
typedef struct
{
    Gateway_LinkState_t state;

    Gateway_TimeMs_t state_since_ms;

    bool has_valid_rx;
    Gateway_TimeMs_t last_valid_rx_ms;

    LinkStateManager_TimeoutConfig_t timeout;
} LinkStateManager_LinkSnapshot_t;
```

한 Link의 현재 관리 상태를 읽기 위한 구조다.

---

# 16. `state`

현재 상태:

```text
UNKNOWN
UNAVAILABLE
AVAILABLE
RECOVERING
```

중 하나다.

---

# 17. `state_since_ms`

현재 상태에 들어온 시각이다.

예:

```text
2000 ms에 AVAILABLE 진입
```

이면:

```c
state_since_ms = 2000;
```

이 값은 유효 Rx가 아직 없는 경우 Timeout 시작 기준으로 사용할 수 있다.

---

# 18. `has_valid_rx`

현재 연결 구간에서 유효한 수신 근거가 있는지를 나타낸다.

중요한 표현은:

```text
현재 연결 구간
```

이다.

예전 연결에서 받은 Valid Rx가 있어도:

```text
Disconnect
Reconnect
```

를 거쳤다면 현재 연결의 근거가 아니다.

따라서 새 연결 구간이 시작되면 이 값은 다시 false가 된다.

---

# 19. `last_valid_rx_ms`

가장 최근 Valid Rx 시각을 저장한다.

예:

```text
UART Valid Message
→ 1250 ms
```

이면:

```c
last_valid_rx_ms = 1250;
```

를 저장한다.

단, 이 값이 메모리에 남아 있어도:

```c
has_valid_rx == false
```

라면 현재 연결 구간의 Timeout 근거로 사용하지 않는다.

---

# 20. 왜 이전 Rx 근거를 무효화해야 하는가?

예:

```text
1000 ms
UART Valid Rx

1050 ms
UART Disconnect

2000 ms
UART Reconnect
```

잘못된 구현:

```text
last_valid_rx = 1000 ms
```

를 그대로 현재 연결의 Timeout 기준으로 사용.

그러면 2000 ms에 재연결되자마자:

```text
이미 1000 ms 이상 지났음
→ 즉시 Timeout
```

될 수 있다.

따라서:

```text
Link Loss
Recovery
새 Available Epoch
```

에서 이전 Rx evidence를 현재 연결 기준에서 무효화한다.

현재 구현은:

```c
has_valid_rx = false;
```

로 처리한다.

---

# 21. Available Epoch 개념

Manager 내부에서 별도 `epoch_id`를 만들지는 않았지만 의미적으로 다음처럼 동작한다.

```text
AVAILABLE Epoch #1
  ├ Valid Rx
  ├ Valid Rx
  └ Link Loss

RECOVERING

AVAILABLE Epoch #2
  ├ old Rx evidence 사용 금지
  └ 새 Valid Rx부터 현재 근거
```

이 구조는 재연결 후 이전 실행 구간 정보를 새 현재 상태로 오인하지 않기 위한 것이다.

---

# 22. `LinkStateManager_MarkAvailableAt()`

```c
Gateway_Status_t LinkStateManager_MarkAvailableAt(
    Gateway_LinkId_t link,
    Gateway_TimeMs_t now_ms);
```

Adapter 또는 Lifecycle Logic이:

```text
현재 경로를 사용할 수 있다고 확인
```

했을 때 호출한다.

예:

```c
LinkStateManager_MarkAvailableAt(
    GATEWAY_LINK_BLUETOOTH,
    now_ms);
```

---

# 23. AVAILABLE과 Valid Rx는 다르다

`MarkAvailableAt()` 호출 시 반드시 Valid Rx가 있었다는 의미는 아니다.

예:

```text
Bluetooth CONNECTED event
```

만으로 경로 사용 가능성이 확인될 수 있다.

이 경우:

```text
state = AVAILABLE
has_valid_rx = false
```

일 수 있다.

Timeout 감시를 활성화했다면 아직 Valid Rx가 없으므로:

```text
state_since_ms
```

를 기준으로 감시한다.

---

# 24. `LinkStateManager_MarkUnavailableAt()`

```c
Gateway_Status_t LinkStateManager_MarkUnavailableAt(
    Gateway_LinkId_t link,
    Gateway_TimeMs_t now_ms);
```

명시적인 경로 상실 이벤트에 사용한다.

예:

```text
Bluetooth disconnect event
UART driver/path failure
Protocol-level link loss
```

이 호출은 현재 연결 구간의 Valid Rx 근거를 무효화한다.

---

# 25. `LinkStateManager_BeginRecoveryAt()`

```c
Gateway_Status_t LinkStateManager_BeginRecoveryAt(
    Gateway_LinkId_t link,
    Gateway_TimeMs_t now_ms);
```

복구 시도에 들어갔음을 기록한다.

예:

```text
UART reinitialization
Bluetooth reconnect procedure
```

상태:

```text
RECOVERING
```

으로 전환한다.

이때도 이전 Rx evidence는 현재 연결 근거로 사용하지 않는다.

---

# 26. `LinkStateManager_OnValidRxAt()`

```c
Gateway_Status_t LinkStateManager_OnValidRxAt(
    Gateway_LinkId_t link,
    Gateway_TimeMs_t now_ms);
```

Adapter/Codec이 **유효한 새 수신**을 확인한 경우 호출한다.

처리:

```text
has_valid_rx = true
last_valid_rx_ms = now
state = AVAILABLE
```

이다.

---

# 27. 누가 Valid Rx를 판단하는가?

`LinkStateManager`가 UART Byte를 직접 보고 판단하지 않는다.

예:

```text
UART Bytes
   ↓
UartFrameCodec
   ↓
Message Boundary 확인
Length 확인
채택된 Integrity 조건 확인
   ↓
Valid Logical Message
   ↓
LinkStateManager_OnValidRx()
```

즉 Frame validation은:

```text
UartFrameCodec / UartAdapter
```

책임이다.

Bluetooth도 마찬가지다.

---

# 28. Valid Rx가 강한 Availability 근거인 이유

다음이 확인되었다면:

```text
현재 경로를 통해
유효한 새 메시지가 실제 수신됨
```

최소한 그 시점에서는 경로가 사용 가능했다는 근거가 된다.

따라서:

```text
RECOVERING
   ↓ Valid Rx
AVAILABLE
```

전이를 허용한다.

---

# 29. Timeout 평가 함수

```c
LinkStateManager_UpdateAt(now_ms);
```

를 주기적으로 호출해 Timeout을 평가한다.

예상 호출 위치:

```text
Gateway Main Task
   ↓
LinkStateManager_UpdateNow()
```

---

# 30. Timeout 기준

Timeout 감시가 활성화된 AVAILABLE Link에 대해:

### Valid Rx가 있는 경우

```text
last_valid_rx_ms
```

기준.

### Valid Rx가 아직 없는 경우

```text
state_since_ms
```

기준.

---

# 31. Timeout 예시

설정:

```text
timeout = 100 ms
```

상태:

```text
1000 ms → AVAILABLE
```

Valid Rx 없음.

```text
1099 ms → 아직 AVAILABLE
1100 ms → UNAVAILABLE
```

이다.

---

# 32. Valid Rx가 Timeout 기준을 갱신하는 예

```text
1000 ms → AVAILABLE
1050 ms → Valid Rx
```

이면 Timeout 기준이:

```text
1000
```

에서:

```text
1050
```

으로 바뀐다.

따라서:

```text
1149 ms → AVAILABLE
1150 ms → UNAVAILABLE
```

이다.

---

# 33. Timeout은 AVAILABLE에만 적용

현재 구현:

```text
AVAILABLE
```

상태에만 설정된 Link Timeout을 적용한다.

다음 상태에는 적용하지 않는다.

```text
UNKNOWN
UNAVAILABLE
RECOVERING
```

---

# 34. RECOVERING에 같은 Timeout을 적용하지 않는 이유

현재 NETWORK-TBD 상태에서:

```text
통신 유지 Timeout
```

과

```text
Recovery 전체 제한 시간
```

이 같은 값이라고 가정할 근거가 없다.

따라서:

```text
AVAILABLE Timeout
```

만 먼저 구현한다.

향후 필요하면 별도로:

```text
Recovery Timeout
Reconnect Count
Backoff
```

정책을 추가한다.

---

# 35. Bluetooth와 UART Timeout도 독립

예:

```text
Bluetooth Timeout = disabled
UART Timeout = enabled
```

도 가능하다.

둘을 하나의 공통 값으로 묶지 않는다.

---

# 36. `Gateway_Time`과의 관계

LinkStateManager는 직접:

```c
esp_timer_get_time()
```

을 사용하지 않는다.

대신:

```text
LinkStateManager
      ↓
Gateway_Time
      ↓
esp_timer
```

구조를 사용한다.

---

# 37. `At()` API와 convenience API

테스트와 deterministic logic을 위해:

```c
LinkStateManager_OnValidRxAt(link, now_ms);
```

처럼 명시적으로 시간을 넣는 API를 제공한다.

실제 Runtime에서는:

```c
LinkStateManager_OnValidRx(link);
```

를 호출할 수 있다.

내부에서:

```c
Gateway_Time_GetMs()
```

를 사용한다.

---

# 38. 왜 둘 다 제공하는가?

### Runtime

```c
LinkStateManager_MarkUnavailable(
    GATEWAY_LINK_BLUETOOTH);
```

처럼 간단히 호출 가능.

### Unit Test

```c
LinkStateManager_MarkUnavailableAt(
    GATEWAY_LINK_BLUETOOTH,
    1000U);
```

처럼 실제 시간 흐름을 완전히 통제할 수 있다.

---

# 39. Query API

현재 상태 조회:

```c
Gateway_LinkState_t LinkStateManager_GetState(
    Gateway_LinkId_t link);
```

예:

```c
if (LinkStateManager_GetState(
        GATEWAY_LINK_DOMAIN_UART)
    == GATEWAY_LINK_STATE_AVAILABLE)
{
    ...
}
```

---

# 40. `LinkStateManager_IsAvailable()`

간단한 bool 확인:

```c
if (LinkStateManager_IsAvailable(
        GATEWAY_LINK_DOMAIN_UART))
{
    ...
}
```

GatewayRouter가 중계 여부를 확인할 때 사용할 수 있다.

---

# 41. Snapshot 조회

```c
LinkStateManager_GetSnapshot(...)
```

을 사용하면:

```text
state
state_since_ms
has_valid_rx
last_valid_rx_ms
timeout config
```

를 한 번에 읽을 수 있다.

주 용도:

```text
Debug
Test
Diagnostic display
Lifecycle 판단 보조
```

다.

---

# 42. Bluetooth Disconnect와 Proximity

매우 중요한 책임 경계:

```text
Bluetooth Disconnect
```

가 발생하면 LinkStateManager는:

```text
Bluetooth = UNAVAILABLE
```

까지만 관리한다.

그 사실을 보고:

```text
Proximity = FAR
```

로 바꾸는 것은 이 Manager 책임이 아니다.

왜냐하면:

```text
통신 상실
≠
실제 단말 이탈 확인
```

이기 때문이다.

---

# 43. UART Loss와 Request Result

UART Path가:

```text
UNAVAILABLE
```

이 되었다고 해서:

```text
Request = FAILED
```

를 생성하지 않는다.

LinkStateManager는:

```text
UART Path를 지금 사용할 수 없다
```

는 사실만 제공한다.

이후 GatewayRouter/MOBILE 표시 계층에서:

```text
차량 결과를 확인할 수 없음
```

과 차량의 실제 실패를 구분해야 한다.

---

# 44. 등록 상태와 Link 상태

다음 둘은 다르다.

```text
REGISTERED
CONNECTED
```

`REGISTERED`는:

```text
DeviceRegistrationManager
```

책임.

`CONNECTED / Bluetooth Path AVAILABLE`은:

```text
LinkStateManager
```

책임.

예:

```text
REGISTERED
+
Bluetooth UNAVAILABLE
```

가능하다.

---

# 45. LinkStateManager와 Gateway_Interface

흐름 예:

```text
UART Adapter
   │
   ├── Valid Message
   │      ↓
   │ LinkStateManager_OnValidRx(UART)
   │
   └── Gateway_Interface_OnDomainMessage(...)
```

두 호출의 의미가 다르다.

```text
LinkStateManager
→ 경로 상태

Gateway_Interface
→ 논리 메시지 전달
```

이다.

---

# 46. LinkStateManager와 GatewayRouter

다음 단계 이후 예상 구조:

```text
GatewayRouter
     │
     ├─ LinkStateManager_IsAvailable(UART)?
     │
     └─ Gateway_Interface_SendToDomain(...)
```

즉 GatewayRouter가:

```text
지금 전달 경로가 있는가?
```

를 판단할 때 사용한다.

---

# 47. LinkStateManager와 GatewayLifecycleManager

부팅/복구 시:

```text
GatewayLifecycleManager
       ↓
LinkStateManager
```

를 이용해:

```text
Bluetooth 상태
UART 상태
```

를 확인할 수 있다.

예:

```text
STARTUP
 ↓
LINK_WAIT
 ↓
Bluetooth AVAILABLE?
UART AVAILABLE?
 ↓
SYNCING
```

같은 상위 Lifecycle을 만들 수 있다.

---

# 48. 현재 구현의 데이터 구조

내부적으로 Link는 2개다.

```text
index 0 → Bluetooth
index 1 → Domain UART
```

각각 별도 Entry를 갖는다.

따라서:

```text
Bluetooth 상태 변경
```

이:

```text
UART 상태
```

를 건드리지 않는다.

---

# 49. Thread Safety

현재 구현에는 Mutex가 없다.

이유:

현재 Architecture에서는 Manager를:

```text
Gateway Main Task 중심
```

으로 사용하는 방향이기 때문이다.

권장:

```text
Bluetooth Callback
UART Event
   ↓
Queue
   ↓
Gateway Task
   ↓
LinkStateManager
```

즉 여러 Callback에서 동시에 Manager를 직접 변경하지 않는 구조를 기본으로 한다.

---

# 50. ISR에서 직접 호출하지 않는 이유

Manager 함수는 일반 C 함수이고 내부 상태를 변경한다.

권장:

```text
ISR / Driver Callback
   ↓
minimal event
   ↓
Queue
   ↓
Gateway Task
   ↓
LinkStateManager
```

이다.

ISR에서 직접 복잡한 상태 전이와 Policy를 수행하지 않는다.

---

# 51. Timeout 주기 호출

예상:

```c
for (;;)
{
    LinkStateManager_UpdateNow();

    ...
}
```

처럼 Gateway Task에서 주기적으로 평가할 수 있다.

실제 Task 주기는 아직 확정하지 않는다.

---

# 52. Timeout 정확도

예를 들어:

```text
Timeout = 500 ms
Task 주기 = 50 ms
```

라면 실제 UNAVAILABLE 전환 시점은 대략:

```text
500 ~ 550 ms
```

범위가 될 수 있다.

따라서 실제 Network Timeout과 Task 주기는 함께 설계해야 한다.

현재 단계에서는 수치 자체를 정하지 않는다.

---

# 53. Wrap-around 대응

`Gateway_Time`을 사용하므로 uint32_t millisecond counter가 wrap되어도:

```text
last_valid_rx
→ timeout
```

계산이 유지된다.

테스트에서도:

```text
UINT32_MAX 근처
→ 0으로 wrap
```

상황을 확인했다.

---

# 54. 현재 단위 테스트 항목

`test_link_state_manager.c`에서 다음을 검증한다.

```text
1. 초기 상태 UNKNOWN
2. 초기화 전 호출 NOT_READY
3. Bluetooth/UART 독립 상태
4. Valid Rx → AVAILABLE
5. Timeout 기본 비활성
6. AVAILABLE 진입 기준 Timeout
7. Valid Rx 기준 Timeout 갱신
8. RECOVERING에는 AVAILABLE Timeout 미적용
9. uint32_t wrap-around Timeout
10. 재연결 후 과거 Rx 근거 재사용 금지
11. 잘못된 Link ID 거부
12. enabled + timeout 0 거부
13. NULL Snapshot 거부
```

---

# 55. 재연결 회귀 테스트

특히 다음 시나리오를 추가했다.

```text
1000 ms  Valid Rx
1050 ms  UNAVAILABLE
1900 ms  RECOVERING
2000 ms  AVAILABLE
```

과거:

```text
last_valid_rx = 1000
```

이 남아 있더라도 현재 연결에서는:

```text
has_valid_rx = false
```

로 처리한다.

따라서 Timeout 기준은:

```text
2000 ms
```

가 된다.

---

# 56. 테스트 결과

Host GCC:

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

다.

또한 기존 모듈 회귀 확인:

```text
Gateway_Time       PASS
Gateway_Interface  PASS
LinkStateManager   PASS
```

모두 통과했다.

---

# 57. 실제 ESP-IDF 연결 시 예상

Bluetooth 예:

```text
Bluetooth connected event
   ↓
LinkStateManager_MarkAvailable(
    GATEWAY_LINK_BLUETOOTH)
```

Disconnect:

```text
Bluetooth disconnect event
   ↓
LinkStateManager_MarkUnavailable(
    GATEWAY_LINK_BLUETOOTH)
```

---

# 58. UART 예

유효한 Domain Frame 수신:

```text
UART Driver
  ↓
UartFrameCodec
  ↓
Valid Logical Message
  ↓
LinkStateManager_OnValidRx(
    GATEWAY_LINK_DOMAIN_UART)
```

Path 복구 시작:

```text
LinkStateManager_BeginRecovery(
    GATEWAY_LINK_DOMAIN_UART)
```

---

# 59. 아직 정하지 않은 UART Availability 확인 방식

현재 다음 중 어떤 방식을 쓸지는 미정이다.

```text
초기 Handshake
Domain Status Message
주기 Heartbeat
기존 주기 State Message
Request/Response
```

따라서 LinkStateManager는:

```text
"무엇이 Availability 근거인가?"
```

를 결정하지 않는다.

Adapter/Protocol 계층이 유효 근거를 얻은 뒤 Manager에 이벤트를 전달한다.

---

# 60. Heartbeat 전용 메시지는 필수가 아님

통신 감시를 위해 반드시 별도:

```text
HEARTBEAT
```

메시지를 만들 필요는 없다.

후속 네트워크 설계에서:

```text
주기적으로 반드시 수신되는 기존 메시지
+
새 갱신 여부 구분
+
Timeout 정의
```

가 가능하면 그것을 Link evidence로 사용할 수 있다.

---

# 61. 상태 전이와 차량 기능은 분리

다음은 LinkStateManager의 출력:

```text
UART UNAVAILABLE
```

이다.

이후 Domain/MOBILE 시스템 의미는 별도다.

예:

```text
MOBILE에서 새 Request 입력
+
UART UNAVAILABLE
```

이면 ESP32는:

```text
차량이 FAILED했다고 생성
```

하지 않는다.

또 과거 요청을 저장했다가 복구 후 자동 실행하지 않는다.

---

# 62. 향후 Network_Config 연결

현재 Timeout 설정은 직접 구조체로 넣는다.

향후:

```text
Network_Config
```

가 만들어지면:

```c
LinkStateManager_ConfigureTimeout(
    GATEWAY_LINK_DOMAIN_UART,
    Network_Config_GetUartTimeout());
```

같이 연결할 수 있다.

정확한 API는 후속 설계에서 정한다.

---

# 63. 향후 Recovery 정책

현재 Manager는:

```text
RECOVERING
```

상태까지만 제공한다.

다음 값들은 아직 추가하지 않는다.

```text
Retry Count
Backoff
Max Recovery Time
Reconnect Attempt Interval
```

필요하면 `GatewayLifecycleManager` 또는 별도 설정에서 담당한다.

---

# 64. 현재 프로젝트 구조

현재까지:

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
│  ├─ LinkStateManager.h       ✅
│  └─ LinkStateManager.c       ✅
│
├─ docs/
│  ├─ Gateway_Types_설명서_v0.1.md
│  ├─ Gateway_Time_설명서_v0.1.md
│  ├─ Gateway_Interface_설명서_v0.1.md
│  └─ LinkStateManager_설명서_v0.1.md
│
└─ test/
   ├─ test_gateway_time.c
   ├─ test_gateway_interface.c
   └─ test_link_state_manager.c
```

---

# 65. 다음 단계

다음 구현 대상은:

```text
MessageContextManager.h/.c
```

다.

LinkStateManager가:

```text
"지금 통신 경로가 살아 있는가?"
```

를 관리한다면,

MessageContextManager는:

```text
"이 메시지가 어느 Vehicle / Device / Session / Request에 속하는가?"
"이전 연결 구간의 Context인가?"
"같은 Request 식별을 유지하고 있는가?"
```

를 관리하게 된다.

---

# 66. MessageContextManager에서 특히 주의할 것

다음 단계에서는 실제 ID 폭이 아직 TBD이므로:

```text
uint32_t SessionId
uint32_t RequestId
```

를 임의 확정하지 않는다.

현재 `Gateway_Interface.h`의:

```c
Gateway_ByteView_t
Gateway_MessageContextView_t
```

를 기반으로 **필요한 Context만 내부 Storage에 Copy**하는 구조를 설계해야 한다.

---

# 67. 핵심 정리

`LinkStateManager`를 한 문장으로 정리하면:

> **ESP32의 Bluetooth와 Domain UART 통신 경로가 현재 사용 가능한지, 언제 마지막으로 유효한 통신 근거가 있었는지를 독립적으로 관리하는 Core 모듈이다.**

가장 중요한 책임 경계:

```text
Link Loss
≠
Vehicle Failure

Bluetooth Disconnect
≠
Proximity FAR

UART Driver Init
≠
Domain Path AVAILABLE

Reconnect
≠
Old Rx Evidence 재사용
```

이 네 가지를 유지하는 것이 핵심이다.
