# CommandManager 상세 설명

## 1. 한 문장 정의

`CommandManager`는:

> **S32K344 Domain이 최종 결정하여 실행 ECU로 내린 명령을 Command ID와 결정 순서로 추적하고, 그 명령이 어떤 Request 또는 자동 기능에서 나왔는지 연결하는 모듈**

입니다.

---

# 2. RequestManager와 차이

```text
RequestManager
= MOBILE이 보낸 Request

CommandManager
= Domain이 ECU에 내린 Command
```

예:

```text
MOBILE Request #100
UNLOCK
      ↓
Domain
      ↓
BCM Command #25
UNLOCK
```

둘은 1:1이라고 보장할 수 없습니다.

예:

```text
MOBILE Setting Request
→ Domain 설정만 변경
→ ECU Command가 없을 수도 있음
```

또는:

```text
AutoVentilation Job #3
→ WINDOW Command #26

MOBILE Request 없음
```

이 가능합니다.

그래서 Request와 Command를 분리합니다.

---

# 3. 이번에 발견해서 수정한 WINDOW Interface 문제

기존 WINDOW 구조는:

```text
Window Command
RequestContext 중심

Window Result
RequestContext 중심
```

이었습니다.

하지만:

```text
AutoVentilation
```

은 MOBILE Request 없이 Window Command를 만들 수 있습니다.

그러면:

```text
SessionId
RequestId
```

가 존재하지 않습니다.

따라서 `Domain_Interface_v0.3.h`에서 WINDOW도:

```text
DomainCommandId
DecisionSequence
Optional RequestContext
```

를 사용하는 구조로 수정했습니다.

이제:

```text
AutoVentilation Job
      ↓
CommandManager
Command #30
      ↓
WINDOW
      ↓
Result Command #30
```

으로 연결할 수 있습니다.

---

# 4. Command ID

CommandManager가 내부에서:

```text
1
2
3
4
...
```

형태로 발급합니다.

`0`은 내부적으로 미할당 값으로 예약했습니다.

이 숫자의 실제 CAN/UART Wire 폭은 아직 결정하지 않습니다.

예를 들어 네트워크 담당자가 나중에:

```text
Command Sequence = 8 bit
```

로 정한다고 해서 Domain 내부 ID까지 8 bit로 줄일 필요는 없습니다.

Adapter에서 Mapping할 수 있습니다.

---

# 5. Decision Sequence

SysRS에서는 단순히 요청이 도착한 순서가 아니라:

> 중앙이 최종 목표를 결정한 순서

가 중요합니다.

그래서:

```text
Command ID
+
Decision Sequence
```

를 따로 관리합니다.

예:

```text
Request A 도착
Request B 도착

Domain 최종 판단:
B 먼저 결정
A는 나중 결정
```

이면 실행 노드에서 중요한 건 Domain의 최종 결정 순서입니다.

현재 `DomainIf_CommandContext_t`에도:

```c
uint32_t decisionSequence;
```

를 추가했습니다.

실제 Wire에서 Rolling Counter/Sequence를 어떻게 표현할지는 Network 설계에서 결정합니다.

---

# 6. Origin

Command가 어디에서 생성됐는지 구분합니다.

```text
MOBILE_REQUEST
DIGITAL_KEY
CLIMATE_POLICY
INTERIOR_LIGHT_POLICY
AUTO_VENTILATION
SYSTEM
```

예:

```text
Door Unlock

User button
→ MOBILE_REQUEST

Proximity
→ DIGITAL_KEY
```

같은 BCM Unlock이라도 발생 원인을 구분할 수 있습니다.

---

# 7. Request Context와 Job Context

Command에는 선택적으로 둘 중 하나를 연결합니다.

## Request Context

```text
MOBILE Request
Session 10
Request 100
       ↓
Command #25
```

## Job Context

```text
AutoVentilation Job #3
       ↓
Command #26
```

현재 Architecture에서는 하나의 Command에 Primary Origin을 하나만 둡니다.

실제 요구에서 Request가 자동 Job을 시작하고 그 Job이 여러 Command를 만드는 형태가 필요하면:

```text
Request
→ Job
→ Command
```

으로 Feature/Job Manager에서 상위 연계를 보존하고, CommandManager는 Job을 Primary Context로 사용할 수 있습니다.

---

# 8. Command Lifecycle

```text
CREATED
   ↓
DISPATCHED
   ↓
ACCEPTED
   ↓
IN_PROGRESS
   ↓
FINAL
```

실행 ECU가 바로:

```text
REJECTED
FAILED
DONE
```

을 반환할 수 있으므로:

```text
DISPATCHED
    ↓
FINAL
```

도 허용합니다.

하지만:

```text
CREATED
↓
FINAL
```

은 허용하지 않습니다.

아직 외부로 전달되지 않은 Command가 ECU 실행 결과를 가진다고 보기 어렵기 때문입니다.

---

# 9. FINAL 결과

```text
DONE
REJECTED
CANCELLED
FAILED
```

만 Final로 사용합니다.

`UNKNOWN`은 실제 ECU 결과가 아닙니다.

통신 문제로 마지막 확인이 불가능해지면:

```text
기존 Result
+
UNCONFIRMED
```

으로 표시하거나 아직 결과가 없다면 `hasExecutionResult=false` 상태를 유지합니다.

---

# 10. Capacity

정적 메모리:

```c
COMMAND_MANAGER_MAX_RECORDS = 24
```

를 사용합니다.

가득 찼을 때:

```text
진행 중 Command
→ 절대 덮어쓰지 않음

가장 오래된 FINAL Command
→ 재사용 가능
```

입니다.

S32K344 Embedded 환경에서 동적 메모리 없이 운영하기 위한 구조입니다.

---

# 11. 전체 흐름

## MOBILE Door

```text
MOBILE Request #100
      ↓
RequestManager
      ↓
Permission
      ↓
Door Feature
      ↓
CommandManager_Create()
      ↓
Command #25
      ↓
BCM
      ↓
Result Command #25
      ↓
ResultManager
      ↓
CommandManager_Finalize()
      ↓
Request #100 Result
```

## Auto Ventilation

```text
AutoVentilation Job #3
      ↓
CommandManager_Create()
      ↓
Command #26
      ↓
WINDOW MOVE_TO_POSITION
      ↓
WINDOW Result #26
      ↓
ResultManager
      ↓
AutoVentilation Job #3
```

MOBILE Request가 없어도 문제없습니다.

---

# 12. Domain_Interface v0.3 변경

이번 단계에서:

```text
DomainIf_CommandContext_t
```

에:

```c
uint32_t decisionSequence;
```

를 추가했습니다.

또 WINDOW:

```c
DomainIf_WindowCommand_t
```

를 새로 정의했고:

```c
DomainIf_WindowCommandResult_t
```

는 `RequestContext`가 아니라:

```c
DomainCommandId_t commandId;
```

를 기준으로 결과를 연결하도록 수정했습니다.

이 변경은 앞으로 `ResultManager`에서 중요합니다.

---

# 13. 다음 단계

다음은:

```text
ResultManager
```

입니다.

여기에서:

```text
BCM Event / Result
WINDOW Command Result
      ↓
Command ID 조회
      ↓
CommandManager
      ↓
Origin Request / Job 확인
      ↓
RequestManager 또는 Feature Job에 결과 전달
```

구조를 완성합니다.
