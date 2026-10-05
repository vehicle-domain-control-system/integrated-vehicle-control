# GatewaySessionManager 상세 설명

## 1. 한 문장으로

`GatewaySessionManager`는:

> **ESP32가 알려준 등록 단말·현재 Bluetooth 연결·Session 정보를 이용해, 지금 들어온 MOBILE Request가 "현재 유효한 연결 구간"의 요청인지 판단하는 모듈**

입니다.

---

# 2. 왜 RequestManager와 별도로 필요한가?

둘은 질문이 다릅니다.

```text
GatewaySessionManager
"이 Request가 현재 신뢰 가능한 Device/Session에서 왔는가?"

RequestManager
"이 Request ID를 이미 처리했는가?
 같은 ID로 다른 내용이 들어왔는가?"
```

실행 순서:

```text
UART Frame
   ↓
UartAdapter Validation
   ↓
GatewaySessionManager
   ↓ valid current session?
RequestManager
   ↓ new/duplicate/conflict?
PermissionManager
```

---

# 3. SysRS에서 가져온 핵심 규칙

현재 SysRS의 논리 규칙은 다음과 같습니다.

```text
ESP32:
- 등록된 Bluetooth 단말 확인 결과 제공
- 현재 Bluetooth 연결 확인 결과 제공
- 대상/세션/요청 식별 연계 유지

Domain:
- 확인 결과를 차량 실행 조건과 결합
- 재시작 전 미완료 요청 자동 재실행 금지
- 연결 복구 후 현재 연결/Session 재확인
- 이전 실행 구간 지연 Packet을 새 요청으로 사용 금지
```

따라서 Session과 Request ID만 동일하다고 무조건 실행할 수 없습니다.

---

# 4. ACTIVE Session 조건

현재 구현은 다음 조건일 때 Session을 ACTIVE로 봅니다.

```text
Connection Quality = OK
Registration = REGISTERED
Bluetooth Connection = CONNECTED
```

`APP_ACTIVE`는 여기에 넣지 않았습니다.

이유:

```text
App Active
= Digital Key 자동 Unlock 등의 Feature 조건

Session Valid
= 현재 등록/연결된 요청 Context인지
```

이 둘은 같은 개념이 아니기 때문입니다.

DigitalKeyManager에서는 이후 별도로:

```text
GatewaySessionManager_GetAppActive()
```

를 확인할 수 있습니다.

---

# 5. Session ID 0을 invalid로 하지 않은 이유

현재 문서에서 Session ID의:

```text
생성 주체
폭
수명
숫자 범위
Invalid Raw Value
```

가 아직 확정되지 않았습니다.

따라서 코드가 임의로:

```c
if (sessionId == 0)
    invalid;
```

라고 만들지 않았습니다.

현재 Session 유효성은 **ESP32가 제공한 현재 Connection Context 안의 Session ID와 Request의 Session ID가 같은가**를 기준으로 합니다.

---

# 6. 재연결과 Retired Session

예:

```text
Session 10 ACTIVE
     ↓
BT disconnect
     ↓
Session 10 RETIRED
```

이후 늦은 Packet:

```text
Session 10 / Request 42
```

가 도착해도:

```text
GSM_REQUEST_RETIRED_SESSION
```

으로 거부합니다.

이것은 Request History를 지우는 것과 다릅니다.

```text
GatewaySessionManager
→ 현재 실행 Session 차단

RequestManager
→ 과거 Request 42 결과 History 유지 가능
```

---

# 7. 왜 Session ID 재사용을 기본 차단했나?

SysRS는:

```text
이전 실행 구간의 지연 메시지를 새 요청으로 처리하지 말 것
```

을 요구합니다.

그런데 아직 UART Protocol에:

```text
Boot Counter
Session Generation
Connection Epoch
```

같은 추가 식별자가 확정되지 않았습니다.

만약:

```text
연결 1: Session ID 10
연결 끊김
연결 2: 다시 Session ID 10
```

을 허용하면 늦게 도착한 과거 `Session 10` Packet과 새 `Session 10` Packet을 Domain이 구분할 근거가 없습니다.

따라서 기본:

```c
GSM_SESSION_REUSE_STRICT
```

로 했습니다.

추후 네트워크/Interface에서:

```text
Session Generation
Boot Counter
```

등이 확정되면:

```c
GSM_SESSION_REUSE_ALLOW
```

또는 더 강한 `(Session ID + Generation)` 구조로 변경할 수 있습니다.

---

# 8. Session Transition을 반환하는 이유

GatewaySessionManager가 RequestManager를 직접 호출하지 않습니다.

대신:

```text
SESSION_ACTIVATED
SESSION_CHANGED
SESSION_INVALIDATED
SESSION_REUSE_REJECTED
```

를 반환합니다.

예:

```c
GatewaySessionTransition_t transition;

GatewaySessionManager_UpdateConnection(
    device,
    &connection,
    nowMs,
    &transition);
```

그 뒤 상위 Orchestrator:

```text
if SESSION_INVALIDATED
    RequestManager_DeactivateSession(old session)
    DomainLifecycle_BeginResync(...)

if SESSION_CHANGED
    RequestManager_DeactivateSession(old session)
    현재 State 재동기화
```

를 수행합니다.

이렇게 해야:

```text
Session 관리
Request History 관리
Domain Lifecycle
```

가 서로 직접 강결합되지 않습니다.

---

# 9. 일반 MOBILE Request 처리

권장:

```c
GatewayRequestValidation_t result =
    GatewaySessionManager_ValidateRequest(
        request.deviceContextId,
        &request.context);

if (result != GSM_REQUEST_VALID)
{
    /* 실행하지 않음 */
}
else
{
    RequestManager_Register(...);
}
```

그 다음 RequestManager가:

```text
NEW
DUPLICATE
ID_CONFLICT
STALE
```

를 판단합니다.

---

# 10. 재부팅

## S32K344 자체 Reset

RAM이 초기화되면 이전 Request 중복 근거가 사라질 수 있습니다.

SysRS 원칙:

```text
보존된 근거가 없으면
기존 Session 무효화
→ 새 인증/Session 필요
```

따라서 부팅 후:

```text
DomainLifecycle = SYNCING
GatewaySessionManager = 새 Current Context 대기
```

로 시작합니다.

## ESP32 Restart

ESP32 Restart를 알 수 있는 근거가 생기면:

```c
GatewaySessionManager_InvalidateAll(nowMs);
```

을 호출합니다.

그 후 새 Connection/Session이 들어오기 전까지 Request를 수용하지 않습니다.

---

# 11. UART Link 상태는 왜 여기서 관리하지 않나?

SysRS는:

```text
Bluetooth 연결
UART 연결
```

을 독립적으로 관리하라고 합니다.

`GatewaySessionManager`는 MOBILE/ESP32의 논리 Session을 관리합니다.

향후:

```text
CommunicationMonitor
```

가:

```text
ESP32 UART Link ONLINE/OFFLINE
```

을 따로 관리합니다.

최종 Request 수용은:

```text
Gateway Session Valid
+
UART/Communication Available
+
Domain Lifecycle
+
Feature Permission
```

의 결합입니다.

---

# 12. 다음 연결

이 모듈 다음에는 `SettingsManager`를 구현합니다.

그 이후:

```text
PermissionManager
CommandManager
ResultManager
```

까지 만들면 일반 MOBILE Request의 Core Chain:

```text
Session
→ Request
→ Setting/Permission
→ Command
→ Result
```

이 완성됩니다.
