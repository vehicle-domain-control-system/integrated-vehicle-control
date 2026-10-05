# Domain Heartbeat · 오류 처리 · 보안 책임 구조

## 1. 먼저 구분해야 하는 네 가지

차량 통신에서 다음은 서로 다른 문제입니다.

```text
1. Data Freshness
   "이 상태값이 아직 최신인가?"

2. ECU Liveness / Heartbeat
   "상대 ECU 자체가 살아 있고 통신 중인가?"

3. Functional Fault
   "ECU는 살아 있지만 센서/모터/기능에 고장이 있는가?"

4. Security / Authorization
   "이 요청을 믿을 수 있는 출처가 보냈고 실행을 허용해도 되는가?"
```

이 네 가지를 한 Manager에 몰아넣지 않는 것이 좋습니다.

---

# 2. 추천 구조

```text
                   CAN / UART
                       │
                       ▼
              +-----------------+
              | Adapter / E2E   |
              | Decode / CRC    |
              | Counter/Length  |
              +-----------------+
                       │
          ┌────────────┴────────────┐
          ▼                         ▼
+---------------------+    +----------------------+
| CommunicationMonitor|    | Domain_Interface     |
| ECU Liveness        |    +----------------------+
| Heartbeat / Timeout |               │
+---------------------+               ▼
          │                  +--------------------+
          │                  | VehicleStateManager|
          │                  | State/Freshness    |
          │                  +--------------------+
          │                           │
          ▼                           ▼
+---------------------+      Domain Managers
| DiagnosticManager   |      Request / Result
| Communication Fault |      DigitalKey / Warning
| ECU Fault Aggregate |      Climate
+---------------------+
          │
          ▼
 FunctionAvailability / Warning / MOBILE
```

Security:

```text
Bluetooth Pairing/Bonding
        ↓ ESP32

Registration / Current Connection
        ↓

Session + Request Identity
        ↓

PermissionManager / RequestManager
        ↓

차량 기능별 실행 허용
```

---

# 3. Heartbeat는 어디서 처리?

추천 모듈:

```text
CommunicationMonitor
```

입니다.

`VehicleStateManager`는 상태 데이터가 오래됐는지를 관리하고,
`CommunicationMonitor`는 ECU/Link가 살아 있는지를 관리합니다.

예:

```text
BCM
매 100ms Status/Heartbeat
      ↓
Domain
CommunicationMonitor
      ↓
lastRxTime 갱신
Alive Counter 검사
      ↓
ONLINE
```

연속 수신 실패:

```text
100ms
200ms
300ms
   ↓
Timeout
   ↓
BCM OFFLINE / COMM_FAULT
```

실제 시간/누락 횟수는 네트워크 담당 결과를 Config로 넣습니다.

---

# 4. 별도 Heartbeat Message가 꼭 필요한가?

꼭 그렇지는 않습니다.

이미 ECU가 주기적으로:

```text
ECU_STATE
Status
Alive Counter
```

를 보내고 있다면 그 메시지를 Heartbeat 근거로 사용할 수 있습니다.

즉:

```text
별도 HEARTBEAT Frame
```

을 무조건 하나 더 만들 필요는 없습니다.

권장:

```text
주기 Status Message 존재
→ 그 Message + Alive Counter를 Heartbeat로 활용

주기 Message가 전혀 없음
→ 별도 Heartbeat/Node Status 고려
```

이 부분은 네트워크 담당자와 합의해야 합니다.

---

# 5. Event가 안 왔다고 ECU가 죽었다고 보면 안 되는 이유

예:

```text
WINDOW_ANTIPINCH
```

는 끼임이 발생할 때만 오는 Event입니다.

10분 동안 Anti-Pinch Event가 안 왔다고:

```text
WINDOW ECU OFFLINE
```

이라고 판단하면 안 됩니다.

따라서 Liveness는:

```text
주기 State
Node Status
Heartbeat
Network Management 정보
```

같이 "원래 주기적으로 와야 하는 정보"를 기준으로 판단해야 합니다.

---

# 6. Heartbeat와 Watchdog 차이

둘도 다릅니다.

## ECU Heartbeat

```text
Domain 입장:
"BCM이 아직 통신하고 있는가?"
```

를 확인합니다.

## MCU Watchdog

```text
BCM 자체:
"내 Software가 멈췄는가?"
```

를 확인합니다.

예:

```text
BCM S32K144
Watchdog
→ Main/Task Hang 시 Reset

Domain S32K344
CommunicationMonitor
→ BCM Heartbeat 상실 감지
```

둘 다 필요할 수 있습니다.

---

# 7. 오류 처리는 어디서 하나?

오류 종류에 따라 나눕니다.

## A. Frame/통신 오류

담당:

```text
CanAdapter / UartAdapter
+
CommunicationMonitor
```

예:

```text
CRC Error
Frame Length Error
Invalid Message Type
Alive Counter Error
Sequence Error
Timeout
```

---

## B. 데이터가 오래됨

담당:

```text
VehicleStateManager
+
CommunicationMonitor
```

예:

```text
마지막 Cabin Temperature가 2초 이상 오래됨
→ STALE
```

값을 마지막 정상값 그대로 현재 정상 상태처럼 쓰면 안 됩니다.

---

## C. ECU 기능 고장

담당:

```text
실행 ECU 자체
→ Domain DiagnosticManager
```

예:

```text
BCM:
FAN_FAULT
OVERHEAT_FAULT
LOCK_ACTUATOR_FAULT

WINDOW:
Motor Fault
Position Sensor Fault

CIS:
Sensor Fault
Vision Fault
```

실제 고장 검출은 가능한 한 해당 ECU에서 수행하고,
Domain은 전달받은 Fault를 차량 수준으로 집계합니다.

---

## D. 차량 기능 실행 실패

담당:

```text
ResultManager
+
RequestManager
```

예:

```text
BCM
NO_FEEDBACK
   ↓
ResultManager
   ↓
FAILED + NO_FEEDBACK
   ↓
RequestManager
```

---

## E. 차량 경고

담당:

```text
WarningManager
```

예:

```text
Rear Distance 30cm + Valid
      ↓
WarningManager
      ↓
EMERGENCY
      ├→ VSS
      └→ MOBILE
```

---

# 8. 보안은 어디서 처리?

보안도 한 곳이 아닙니다.

## 8.1 Bluetooth 계층 — ESP32

ESP32가 담당:

```text
Device Pairing / Bonding
Registered Device 확인
현재 Bluetooth Connection 확인
```

현재 프로젝트에서는 "등록된 단말인지"를 ESP32가 확인하여 Domain에 제공합니다.

---

## 8.2 Domain Authorization — PermissionManager

Domain이 담당:

```text
이 Device가 등록됐는가?
현재 연결인가?
Session이 유효한가?
이 기능을 현재 실행할 수 있는가?
필수 차량 상태가 유효한가?
```

즉:

```text
Authentication/Connection 정보
+
Vehicle Safety/Permission
       ↓
PermissionManager
```

로 생각하면 됩니다.

---

# 9. RequestManager도 보안과 관계가 있음

RequestManager의:

```text
Session ID
Request ID
Duplicate Detection
ID Conflict
Request Age
```

는 Replay/중복 실행을 막는 데 도움이 됩니다.

예:

```text
과거 UNLOCK Packet 재전송
     ↓
같은 Session + Request ID
     ↓
RequestManager
     ↓
새 실행 금지
```

하지만 이것만으로 완전한 암호학적 보안이 되는 것은 아닙니다.

---

# 10. CRC는 보안 기능인가?

아닙니다.

```text
CRC
= 전송 중 우발적인 데이터 손상 검출

Authentication / MAC
= 공격자가 만든 위조 Message 검출
```

은 목적이 다릅니다.

따라서:

```text
CRC가 있으니 보안됨
```

이라고 보면 안 됩니다.

---

# 11. CAN 보안이 필요하면?

현재 논리 문서에는 CAN Message의 암호학적 인증까지 확정되어 있지 않습니다.

추후 요구가 생긴다면 AUTOSAR 관점에서는:

```text
SecOC
Freshness Value
Authenticator / MAC
```

같은 구조를 별도 보안 설계로 검토할 수 있습니다.

하지만 현재 프로젝트에서 이것이 구현 범위인지 여부는 별도 결정이 필요합니다.

현재 우선 구현해야 하는 것은:

```text
등록 단말
현재 연결
Session
Request ID
Freshness
중복 방지
CRC/E2E
ECU Local Safety
```

입니다.

---

# 12. 추천 Domain 모듈 구조

현재까지:

```text
Vehicle_Types
Domain_Interface

VehicleStateManager
RequestManager
ResultManager
DigitalKeyManager
WarningManager
ClimateManager
```

여기에 통신 통합 단계에서:

```text
CommunicationMonitor
DiagnosticManager
PermissionManager
```

를 추가하는 것을 추천합니다.

최종적으로:

```text
Domain
├─ VehicleStateManager
├─ RequestManager
├─ ResultManager
├─ PermissionManager
├─ DigitalKeyManager
├─ WarningManager
├─ ClimateManager
├─ CommunicationMonitor
└─ DiagnosticManager
```

정도가 됩니다.

---

# 13. 각 Manager 역할

| Module | 역할 |
|---|---|
| VehicleStateManager | 현재 상태 / Quality / Data Freshness |
| RequestManager | Request ID / Session / 중복 / Lifecycle |
| ResultManager | ECU 결과 → Domain 공통 Result |
| PermissionManager | 요청 수용 / 기능 실행 허용 |
| DigitalKeyManager | FAR→NEAR 기반 자동 Unlock 정책 |
| WarningManager | 차량 수준 Warning 판단 |
| ClimateManager | 공조 목표 계산 / 중재 |
| CommunicationMonitor | ECU Heartbeat / Link Liveness / Timeout |
| DiagnosticManager | Fault 집계 / 기능 가용성 / 진단 상태 |

---

# 14. 네트워크 담당자에게 Heartbeat 관련 받아야 할 것

각 ECU별로 최소 다음을 확인하면 됩니다.

```text
1. Liveness 근거 Message는 무엇인가?
   - 별도 Heartbeat?
   - 주기 ECU_STATE?

2. 송신 주기

3. Timeout / Miss Count

4. Alive Counter 사용 여부

5. Counter 폭 / rollover

6. CRC/E2E 사용 여부

7. 통신 복구 판정
   - 1회 정상 수신?
   - N회 연속 정상?

8. ECU Reset/Restart를 구분할 정보가 있는가?
   - Boot Counter
   - Generation ID 등
```

이 결과를 받으면 `CommunicationMonitor_Config`와 Adapter 쪽에 넣으면 됩니다.

---

# 15. 추천 처리 예시

```text
BCM Status
Cycle = 100ms
Timeout = 300ms
Alive Counter = 0~15
```

정상:

```text
Counter 3
Counter 4
Counter 5
→ BCM ONLINE
```

문제:

```text
Counter 5
Counter 5
Counter 5
```

이면 Packet 자체는 들어오지만 새 정보가 아닐 수 있으므로
Alive Counter 이상으로 판단할 수 있습니다.

수신 없음:

```text
300ms 이상
→ Communication Timeout
→ BCM OFFLINE
→ BCM 상태 STALE
→ BCM 관련 Function Availability 제한
→ 필요 시 MOBILE에 연결/기능 오류 표시
```

---

# 16. 구현 순서 추천

지금 바로 전부 만들 필요는 없습니다.

```text
현재
VehicleStateManager
RequestManager
        ↓
ResultManager
        ↓
DigitalKeyManager
        ↓
WarningManager
        ↓
ClimateManager
```

까지 Domain Logic을 먼저 만들고,

네트워크 담당 결과가 오면:

```text
CanAdapter
UartAdapter
CommunicationMonitor
DiagnosticManager
PermissionManager 보완
```

을 연결하는 순서가 효율적입니다.
