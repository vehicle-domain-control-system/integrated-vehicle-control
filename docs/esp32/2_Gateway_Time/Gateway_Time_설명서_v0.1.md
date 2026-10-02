# Gateway_Time 설계 설명서 v0.1

> 대상 코드  
> - `ESP32/common/Gateway_Time.h`  
> - `ESP32/common/Gateway_Time.c`
>
> 관련 테스트  
> - `ESP32/test/test_gateway_time.c`
>
> 기준 문서  
> - `ESP32 Wireless Gateway Software Architecture v0.3`
> - `Gateway_Types.h 설계 설명서 v0.1`

---

# 1. 목적

`Gateway_Time`은 ESP32 Wireless Gateway에서 사용하는 **공통 monotonic time 추상화 계층**이다.

각 Manager가 직접 ESP-IDF API를 호출하지 않고 다음 구조를 사용하게 한다.

```text
LinkStateManager
MessageContextManager
GatewayLifecycleManager
ProximityManager
        │
        ▼
 Gateway_Time.h
        │
        ▼
 Gateway_Time.c
        │
        ▼
 esp_timer_get_time()
```

이 구조의 목적은 단순하다.

```text
1. 시간 API에 대한 ESP-IDF 의존성을 한곳으로 모음
2. Timeout / Age 계산 규칙을 통일
3. uint32_t wrap-around 처리를 공통화
4. Host 단위 테스트가 가능하도록 함
```

---

# 2. 왜 별도 Time Module이 필요한가?

다음처럼 각 Manager가 직접 시간을 읽도록 만들 수도 있다.

```c
int64_t now = esp_timer_get_time();
```

하지만 이 방식은 권장하지 않는다.

예를 들어:

```text
LinkStateManager.c
ProximityManager.c
GatewayLifecycleManager.c
MessageContextManager.c
```

모두가 직접 `esp_timer.h`를 include하면 Core Logic이 ESP-IDF에 강하게 결합된다.

그 결과:

```text
Host Unit Test 어려움
시간 단위 혼용 가능성
us / ms 계산 중복
Timeout 계산 방식 불일치
Wrap-around 처리 누락
```

문제가 생길 수 있다.

따라서 다음과 같이 분리한다.

```text
Manager
   │
   │ millisecond 논리 시간
   ▼
Gateway_Time
   │
   │ ESP-IDF API
   ▼
esp_timer
```

---

# 3. 사용하는 시간의 종류

`Gateway_Time`이 다루는 시간은 **날짜와 시각이 아니다.**

예:

```text
2026-09-30 18:30:00
```

같은 시간을 관리하지 않는다.

대신 다음처럼 경과 시간을 계산하는 용도다.

```text
Bluetooth 마지막 정상 수신 후 몇 ms가 지났는가?
UART 마지막 정상 메시지 후 몇 ms가 지났는가?
Proximity 입력이 생성된 지 몇 ms인가?
Recovery 시작 후 Timeout이 지났는가?
```

즉:

```text
Monotonic Elapsed Time
```

을 사용한다.

시스템 시각 변경, Time Zone, RTC와는 무관해야 한다.

---

# 4. `Gateway_TimeMs_t`

```c
typedef uint32_t Gateway_TimeMs_t;
```

Gateway 내부에서 millisecond 시간을 표현한다.

이를 단순히 `uint32_t`로 계속 쓰지 않고 typedef한 이유는 의미를 명확하게 하기 위해서다.

예:

```c
Gateway_TimeMs_t last_rx_ms;
Gateway_TimeMs_t timeout_ms;
```

라고 쓰면 해당 숫자가

```text
Byte Count
Message ID
Counter
```

가 아니라 시간이라는 것을 코드에서 바로 알 수 있다.

---

# 5. `Gateway_Time_GetMs()`

```c
Gateway_TimeMs_t Gateway_Time_GetMs(void);
```

현재 Gateway monotonic time을 millisecond 단위로 반환한다.

ESP-IDF에서는:

```c
esp_timer_get_time()
```

을 사용한다.

`esp_timer_get_time()`은 microsecond 단위이므로:

```text
microsecond
   ↓ / 1000
millisecond
```

로 변환한다.

개념적으로:

```c
return esp_timer_get_time() / 1000;
```

과 같은 역할이다.

실제 구현에서는 signed → unsigned 변환을 명확하게 처리한다.

---

# 6. 왜 millisecond를 사용하는가?

현재 ESP32 Gateway에서 필요한 시간은 대부분 다음 범주다.

```text
Bluetooth Update
UART Timeout
Reconnect / Recovery
Proximity Sample
Freshness / Age
```

이 목적에서는 microsecond 수준 정밀도가 필요하지 않다.

또한 SysRS의 모바일 상태/통신 관련 시간들도 ms 또는 s 수준의 시간 의미를 사용한다.

따라서 Gateway Core에서는:

```text
millisecond
```

단위로 통일한다.

주의:

실제 Bluetooth/UART Driver 내부에서 더 높은 시간 정밀도가 필요하면 해당 Driver 내부에서 처리할 수 있다.

하지만 Core Manager에 microsecond 단위를 노출하지 않는다.

---

# 7. 32-bit Wrap-around

`Gateway_TimeMs_t`는 `uint32_t`다.

최대값:

```text
4,294,967,295 ms
```

대략:

```text
49.7일
```

이 지나면 다음과 같이 돌아간다.

```text
4294967294
4294967295
0
1
2
...
```

이를 wrap-around라고 한다.

잘못된 구현:

```c
if (now_ms > start_ms + timeout_ms)
{
    ...
}
```

이 방식은 wrap-around 근처에서 오류가 날 수 있다.

---

# 8. `Gateway_Time_ElapsedMs()`

```c
Gateway_TimeMs_t Gateway_Time_ElapsedMs(
    Gateway_TimeMs_t start_ms,
    Gateway_TimeMs_t now_ms);
```

구현:

```c
return (Gateway_TimeMs_t)(now_ms - start_ms);
```

`uint32_t`의 unsigned arithmetic 특성을 사용한다.

예:

```text
start = 100
now   = 150

elapsed = 50
```

일반적인 경우는 당연히 정상 동작한다.

---

# 9. Wrap-around 예시

다음 상황을 보자.

```text
start = UINT32_MAX - 9
```

즉:

```text
4294967286
```

이다.

그 뒤 counter가 wrap되고 현재 값이:

```text
15
```

가 되었다고 하자.

실제 경과:

```text
4294967286
...
4294967295
0
...
15
```

총:

```text
25 ms
```

이다.

C의 unsigned subtraction:

```c
now_ms - start_ms
```

도 이 상황에서 `25`를 만든다.

따라서 직접 다음처럼 분기할 필요가 없다.

```c
if (now < start)
{
    ...
}
```

---

# 10. `Gateway_Time_HasElapsed()`

```c
bool Gateway_Time_HasElapsed(
    Gateway_TimeMs_t start_ms,
    Gateway_TimeMs_t timeout_ms,
    Gateway_TimeMs_t now_ms);
```

용도:

```text
특정 시점 이후 Timeout 시간이 지났는가?
```

내부적으로:

```c
Gateway_Time_ElapsedMs(start_ms, now_ms) >= timeout_ms
```

를 확인한다.

예:

```text
start   = 100 ms
timeout = 50 ms
now     = 149 ms

→ false
```

그리고:

```text
now = 150 ms

→ true
```

---

# 11. `Gateway_Time_HasElapsedNow()`

```c
bool Gateway_Time_HasElapsedNow(
    Gateway_TimeMs_t start_ms,
    Gateway_TimeMs_t timeout_ms);
```

현재 시간을 직접 읽어서 Timeout을 확인하는 편의 함수다.

예:

```c
if (Gateway_Time_HasElapsedNow(last_uart_rx_ms, uart_timeout_ms))
{
    ...
}
```

내부적으로:

```text
Gateway_Time_GetMs()
        ↓
Gateway_Time_HasElapsed()
```

를 수행한다.

---

# 12. 예상 사용 예 — LinkStateManager

이후 `LinkStateManager`에서는 다음과 같이 사용할 수 있다.

```c
static Gateway_TimeMs_t s_last_uart_rx_ms;

void LinkStateManager_OnValidUartMessage(void)
{
    s_last_uart_rx_ms = Gateway_Time_GetMs();
}
```

그리고 주기적으로:

```c
if (Gateway_Time_HasElapsedNow(
        s_last_uart_rx_ms,
        uart_timeout_ms))
{
    // UART Path unavailable 처리
}
```

중요:

여기서:

```text
Timeout 발생
→ UART Path 상태 갱신
```

까지만 한다.

```text
Timeout 발생
→ 차량 Request FAILED
```

로 처리하면 안 된다.

차량 Result 의미는 Domain 책임이다.

---

# 13. 예상 사용 예 — ProximityManager

Proximity 입력이 들어왔을 때:

```c
last_proximity_update_ms = Gateway_Time_GetMs();
```

그리고 현재 Age 계산:

```c
Gateway_TimeMs_t now_ms = Gateway_Time_GetMs();

Gateway_TimeMs_t age_ms =
    Gateway_Time_ElapsedMs(
        last_proximity_update_ms,
        now_ms);
```

이 `age_ms`를 이용해서:

```text
Quality
Update Basis
```

를 만들 수 있다.

예:

```c
proximity.update.age_ms = age_ms;
```

---

# 14. Freshness와 Gateway_Time의 관계

`Gateway_Time` 자체는 다음을 판단하지 않는다.

```text
이 Proximity가 STALE인가?
이 Vehicle State를 아직 사용해도 되는가?
```

단순히:

```text
얼마나 시간이 지났는가?
Timeout 이상 지났는가?
```

만 제공한다.

정책 판단은 사용하는 Manager가 수행한다.

예:

```text
Gateway_Time
     ↓
age_ms = 1500 ms
     ↓
ProximityManager
     ↓
설정된 expiry 기준과 비교
     ↓
Quality / Update Basis 생성
```

또한 차량 수준의 최종 Freshness 판단은 Domain 쪽 책임과 구분한다.

---

# 15. ESP-IDF 의존성

실제 ESP32 firmware에서는:

```c
#ifdef ESP_PLATFORM
#include "esp_timer.h"
#endif
```

구조를 사용한다.

그리고:

```c
esp_timer_get_time()
```

을 호출한다.

따라서 Manager들은:

```c
#include "esp_timer.h"
```

를 직접 하지 않는다.

Manager가 필요한 것은 오직:

```c
#include "Gateway_Time.h"
```

이다.

---

# 16. Host Test 처리

Host에서 GCC로 단위 테스트할 때는 ESP-IDF가 존재하지 않는다.

따라서:

```text
ESP_PLATFORM
```

이 정의되어 있지 않다.

테스트에서는 명시적으로:

```text
-DGATEWAY_TIME_HOST_TEST
```

를 사용한다.

이 경우 `Gateway_Time_GetMs()`는 실제 시간원 대신 `0`을 반환한다.

이 fallback의 목적은:

```text
Gateway_Time_ElapsedMs()
Gateway_Time_HasElapsed()
```

같은 순수 계산 로직을 Host에서 검증하기 위해서다.

실제 firmware에서 사용할 시간원이 아니다.

---

# 17. 왜 Host fallback을 자동으로 허용하지 않는가?

다음 구조는 위험하다.

```c
#ifndef ESP_PLATFORM

Gateway_Time_GetMs()
{
    return 0;
}

#endif
```

만약 ESP-IDF Build 설정이 잘못되었는데도 컴파일이 된다면,
firmware가 계속:

```text
현재 시간 = 0
```

으로 동작할 수 있다.

이를 방지하기 위해 현재 구현은:

```text
ESP_PLATFORM
또는
GATEWAY_TIME_HOST_TEST
```

둘 중 하나가 반드시 있어야 한다.

그 외 환경에서는:

```c
#error
```

로 빌드를 중단한다.

---

# 18. Unit Test

현재 다음 테스트를 작성했다.

```text
test_elapsed_normal
test_elapsed_wraparound
test_timeout
test_timeout_wraparound
```

파일:

```text
ESP32/test/test_gateway_time.c
```

---

## 18.1 일반 경과 시간

```text
start = 100
now   = 150

Expected = 50
```

---

## 18.2 Wrap-around 경과 시간

```text
start = UINT32_MAX - 9
now   = 15

Expected = 25
```

---

## 18.3 Timeout

```text
start   = 100
timeout = 50

now = 149
→ false

now = 150
→ true
```

---

## 18.4 Wrap-around Timeout

Wrap-around가 발생해도 동일한 Timeout 규칙이 적용되는지 확인한다.

---

# 19. 현재 테스트 결과

Host GCC 조건:

```text
-std=c11
-Wall
-Wextra
-Werror
-DGATEWAY_TIME_HOST_TEST
```

기준으로:

```text
Compile PASS
Unit Test PASS
```

상태다.

실제 ESP-IDF target build는 이후 ESP32 프로젝트 구조와 CMake 구성이 만들어질 때 다시 확인한다.

즉 현재 검증 범위는:

```text
C 문법
Warning
시간 계산 로직
Wrap-around 계산
```

까지다.

---

# 20. Gateway_Time이 하지 않는 것

이 모듈은 다음 역할을 하지 않는다.

```text
RTC 관리
날짜 / 시각
Time Zone
NTP
Bluetooth Timeout 정책 결정
UART Timeout 값 결정
Proximity Expiry 값 결정
Vehicle Freshness 결정
Request Timeout 정책 결정
```

이 값과 정책은 각각:

```text
Gateway_PolicyConfig
Network_Config
각 Manager
Domain
```

에서 담당한다.

---

# 21. 현재 파일 관계

현재까지 구현:

```text
ESP32/
│
├─ common/
│  ├─ Gateway_Types.h          ✅
│  ├─ Gateway_Time.h           ✅
│  └─ Gateway_Time.c           ✅
│
├─ docs/
│  ├─ Gateway_Types_설명서_v0.1.md
│  └─ Gateway_Time_설명서_v0.1.md
│
└─ test/
   └─ test_gateway_time.c      ✅
```

---

# 22. 다음 단계

다음 구현 대상은:

```text
Gateway_Interface.h/.c
```

다.

현재까지 만든:

```text
Gateway_Types
Gateway_Time
```

을 이용해서 ESP32 내부에서 다음 경계를 정의한다.

```text
Bluetooth
    ↓
Gateway Interface
    ↓
Core Managers
    ↓
Gateway Interface
    ↓
UART
```

다음 단계에서 중요한 것은 실제 Byte Protocol을 만드는 것이 아니다.

먼저:

```text
MOBILE → ESP32에서 어떤 논리 정보를 받는가?
ESP32 → Domain에 어떤 논리 정보를 넘기는가?
Domain → ESP32에서 어떤 논리 정보를 받는가?
ESP32 → MOBILE에 어떤 논리 정보를 넘기는가?
```

를 C 함수 Interface로 정의한다.

---

# 23. 핵심 정리

`Gateway_Time`의 역할은 한 문장으로 정리할 수 있다.

> **ESP32 Gateway의 모든 Timeout/Age 계산이 동일한 monotonic millisecond 기준을 사용하도록 만드는 작은 Platform Abstraction이다.**

현재 설계 원칙:

```text
ESP-IDF 시간 API
        ↓
Gateway_Time
        ↓
Core Manager
```

그리고:

```text
시간 측정
≠
정책 판단
```

을 유지한다.

즉 `Gateway_Time`은 시간을 제공하지만,
그 시간이 오래되었을 때 무엇을 할지는 해당 Manager 또는 Domain이 판단한다.
