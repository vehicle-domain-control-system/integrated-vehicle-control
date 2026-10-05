# PermissionManager 상세 설명

## 1. 한 문장 정의

`PermissionManager`는:

> **어떤 기능을 실행하기 전에 Domain 전체에서 공통으로 확인해야 하는 Gate만 검사하는 모듈**

입니다.

---

# 2. 왜 별도 Manager가 필요한가?

MOBILE 요청 하나를 실행하려면 여러 공통 조건이 반복됩니다.

```text
Domain 준비됨?
현재 Session 유효?
기능 지원됨?
현재 기능 Available?
필요한 입력이 신뢰 가능?
Power 허용?
Operation 허용?
```

Door, Climate, Digital Key, Window 기능마다 이 코드를 반복하면:

```text
기능마다 서로 다른 방식으로 Session 검사
기능마다 STALE 처리 방식 달라짐
기능마다 Availability 해석 달라짐
```

같은 문제가 생깁니다.

그래서 공통 조건만 한곳에 모읍니다.

---

# 3. 가장 중요한 설계 원칙

## PermissionManager는 기능 정책을 판단하지 않는다

PermissionManager:

```text
공통 Gate
```

Feature Manager:

```text
기능 고유 정책
```

예를 들면:

```text
Domain READY?
→ PermissionManager

현재 Session Valid?
→ GatewaySessionManager 결과를 PermissionManager에 제공

Rear Sensor Available?
→ DiagnosticManager 결과를 PermissionManager에 제공

FAR → NEAR 전이인가?
→ DigitalKeyManager

30°C 이상인가?
→ AutoVentilationManager

Door가 CLOSED인가?
→ Door Feature 정책 + BCM Local Safety
```

PermissionManager가 커다란 God Manager가 되지 않게 한 것입니다.

---

# 4. Requirement Mask

모든 기능이 모든 조건을 필요로 하는 것은 아닙니다.

따라서:

```c
PERMISSION_REQUIRE_DOMAIN_OPERATIONAL
PERMISSION_REQUIRE_SESSION_VALID
PERMISSION_REQUIRE_FUNCTION_SUPPORTED
PERMISSION_REQUIRE_FUNCTION_AVAILABLE
PERMISSION_REQUIRE_INPUT_QUALITY
PERMISSION_REQUIRE_POWER_PERMISSION
PERMISSION_REQUIRE_OPERATION_PERMISSION
```

중 필요한 것만 선택합니다.

예:

```c
evaluation.requirements =
    PERMISSION_REQUIRE_DOMAIN_OPERATIONAL |
    PERMISSION_REQUIRE_SESSION_VALID |
    PERMISSION_REQUIRE_FUNCTION_SUPPORTED |
    PERMISSION_REQUIRE_FUNCTION_AVAILABLE;
```

---

# 5. 왜 Required Input Quality를 하나만 받나?

PermissionManager가:

```text
Door State
Temperature
Rear Distance
Occupant
...
```

를 직접 알기 시작하면 Feature 의존성이 생깁니다.

대신 호출자가 해당 기능이 **실제로 필요로 하는 입력만** 먼저 평가해서:

```text
OK
STALE
INVALID
NO_DATA
```

로 요약해 전달합니다.

예:

```text
Manual Fan
```

은 CIS Temperature가 필요 없을 수 있습니다.

따라서 Temperature가 STALE이라고:

```text
Manual Fan 전체 차단
```

하면 안 됩니다.

반면:

```text
Auto Climate
```

은 유효한 Temperature가 필요하므로:

```text
PERMISSION_REQUIRE_INPUT_QUALITY
```

를 사용합니다.

이렇게 SysRS의:

```text
무관한 센서 STALE 하나로 전체 제어를 막지 않는다
```

원칙을 지킬 수 있습니다.

---

# 6. DEGRADED 처리

`DEGRADED`는 무조건 사용 불가라는 뜻이 아닙니다.

예:

```text
ECU 일부 진단 기능 제한
BUT
현재 요청 기능은 안전하게 수행 가능
```

할 수 있습니다.

그래서:

```c
PERMISSION_DEGRADED_ALLOW
PERMISSION_DEGRADED_DENY
```

를 호출자가 선택합니다.

실제 어떤 기능이 DEGRADED에서도 가능한지는:

```text
DiagnosticManager
+
Feature 설계
```

가 결정합니다.

---

# 7. STOP/OFF의 중요한 예외

SysRS에서는 STOP/OFF 요청은:

```text
인증
대상
새 요청 유효성
```

은 확인하되,

```text
현재 동작 유지 허용이 없음
```

만으로 정지 자체를 거부하면 안 됩니다.

그래서 STOP/OFF 평가에서는:

```c
PERMISSION_REQUIRE_OPERATION_PERMISSION
```

을 넣지 않을 수 있습니다.

예:

```text
정상 활성 명령:
Power + Operation Permission 필요

STOP:
Power/안전 경로에 필요한 최소 조건만 사용
Operation Keep-Alive 상실 때문에 STOP 거부하지 않음
```

이 차이를 `Requirement Mask`로 표현합니다.

---

# 8. UNKNOWN과 NOT_ALLOWED를 구분

Power/Operation Permission을 bool 하나로 만들지 않았습니다.

```text
UNKNOWN
ALLOWED
NOT_ALLOWED
```

로 구분합니다.

왜냐하면:

```text
정보를 못 받음
```

과:

```text
명시적으로 실행 금지
```

는 서로 다른 상태이기 때문입니다.

---

# 9. PermissionResult

결과:

```text
ALLOW
DENY
```

와 함께 상세 사유를 제공합니다.

예:

```text
SESSION_INVALID
FUNCTION_UNAVAILABLE
REQUIRED_INPUT_STALE
POWER_NOT_ALLOWED
```

이 상세 사유는 Domain 내부 진단에 사용할 수 있습니다.

MOBILE 공통 Request Result가 필요하면:

```c
PermissionManager_ToResultReason()
```

으로:

```text
STATE_UNTRUSTED
NOT_ALLOWED
```

같은 기존 `ResultReason_t`로 축약할 수 있습니다.

---

# 10. 현재 전체 Request 흐름

이제 Core 앞단은:

```text
ESP32
 ↓
GatewaySessionManager
 ↓
RequestManager
 ↓
SettingsManager
 ↓
PermissionManager
```

까지 구현되었습니다.

기능에 따라 Settings와 Permission의 실제 호출 순서는 달라질 수 있습니다.

예:

```text
Target Temperature 설정
→ Permission/지원 범위 확인
→ Settings Commit

Door Unlock
→ SettingsManager 사용 안 함
→ Permission
→ Command
```

즉 모든 Request가 무조건 SettingsManager를 지나는 것은 아닙니다.

---

# 11. 실제 사용 예

## Auto Climate

```text
Domain READY
Session Valid
Climate Supported
Climate Available
Required Temperature Quality OK
Power Allowed
```

모두 필요하다면:

```c
requirements =
    DOMAIN |
    SESSION |
    SUPPORT |
    AVAILABILITY |
    INPUT_QUALITY |
    POWER;
```

## Manual Fan

Temperature는 필요하지 않으면:

```text
INPUT_QUALITY requirement 제외
```

## Digital Key Auto Unlock

공통 Gate:

```text
Domain
Session
Support
Availability
```

이후:

```text
Registered?
App Active?
FAR→NEAR?
Door state?
Approach already consumed?
```

등은 `DigitalKeyManager`에서 판단합니다.

---

# 12. 다음 단계

현재:

```text
GatewaySessionManager       ✅
RequestManager              ✅
SettingsManager             ✅
PermissionManager           ✅
```

다음은:

```text
CommandManager
```

입니다.

여기서:

```text
MOBILE Request
      ↓
Domain Feature Decision
      ↓
Command ID
      ↓
Target ECU
```

를 추적하게 됩니다.

그 이후 `ResultManager`가 ECU 결과를 다시 Command/Request로 연결합니다.
