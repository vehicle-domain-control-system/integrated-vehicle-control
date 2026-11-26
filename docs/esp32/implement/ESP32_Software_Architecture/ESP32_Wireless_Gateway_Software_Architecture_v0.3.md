# ESP32 Wireless Gateway 전체 Software / Interface Architecture v0.3

> 작성 기준일: 2026-09-30  
> 기준 문서: 현재 SR / SysRS / S32K344 Domain Controller Software Architecture v0.2  
> 목적: ESP32의 **Bluetooth ↔ UART 게이트웨이 구조, 단말 등록·현재 연결 확인, Digital Key 근접 정보 생성, 메시지 중계, 연결 복구·오류 처리와 구현 순서**를 하나의 기준 문서로 정리한다.

---

# 0. 문서 상태 표기

이 문서에서는 요구사항과 구현 제안을 혼동하지 않도록 다음 상태를 사용한다.

| 표기 | 의미 |
|---|---|
| `CONFIRMED` | 현재 SR/SysRS에서 ESP32의 책임 또는 정보 의미가 확인됨 |
| `PROVISIONAL` | SysRS의 `[잠정]`에 해당하며 현재 설계 기준으로 사용하지만 실제 구성·시험에 따라 변경 가능 |
| `DESIGN` | 위 요구를 구현하기 위해 ESP32 내부 Architecture로 제안하는 구조 |
| `TBD` | 정책, 입력 방법, 수치 또는 담당이 아직 확정되지 않음 |
| `INPUT-TBD` | 필요한 논리 정보는 확인됐지만 실제 획득 방법 또는 Producer가 미정 |
| `NETWORK-TBD` | 논리 의미는 있지만 Bluetooth Profile, UART Frame, Baud Rate, 주기, Timeout 등 실제 Wire 설계가 미정 |
| `OUT-OF-SCOPE` | 현재 ESP32 책임 범위가 아님 |
| `OPTIONAL` | 요구 확정 시 추가할 수 있으나 현재 Architecture 필수는 아님 |

중요:

> 이 문서의 `Manager`, `Adapter`, `Router` 이름은 대부분 **구현 Architecture(DESIGN)** 이다.  
> 각 모듈이 담당하는 기능 책임은 SR/SysRS의 ESP32 논리 요구를 기반으로 한다.

---

# 1. ESP32의 역할

ESP32는 차량 기능을 최종 판단하는 Domain Controller가 아니다.

현재 ESP32의 핵심 역할은 다음과 같다.

`CONFIRMED`

```text
MOBILE ↔ Bluetooth 통신
ESP32 ↔ S32K344 Domain UART 통신

등록된 Bluetooth 단말 확인
현재 Bluetooth 연결 확인
Bluetooth Link와 UART Path 상태를 독립적으로 관리

MOBILE Request 중계
조회·확인 정보 중계
Domain Vehicle State 중계
Request Result 중계
Warning 중계
Function Availability 중계
Digital Key State / Result 중계

MOBILE 쪽 식별 정보
→ 차량 / 단말 / Session / Request 연계 유지

Proximity 상태와 Quality / Update 근거를 Domain에 제공
재시작 / 재연결 시 이전 실행 구간 정보 오사용 방지
원본 정보의 최신성 근거를 중계 과정에서 임의로 새로 만들지 않음
```

현재 SysRS의 `[잠정]`에 해당하는 부분:

`PROVISIONAL / INPUT-TBD`

```text
Bluetooth RSSI를 근접 판단의 입력으로 사용
RSSI 기반 NEAR / FAR / UNKNOWN 판정
근접 진입 / 이탈 안정화 방식
App Active 상태 확인 정보의 획득·연계 방법
```

즉, **ESP32가 근접 정보를 제공한다는 책임은 유지**하되,
현재 그 구현 수단을 RSSI로 고정하는 부분은 `PROVISIONAL`로 관리한다.

반대로 ESP32의 책임이 아닌 것은 다음과 같다.

```text
차량 Request 최종 수용 판단
차량 기능 실행 Permission 최종 판단
자동 Unlock 최종 명령 생성
BCM 직접 구동
WINDOW / VSS 직접 제어
차량 Warning 최종 판단
차량 State 자체 보정

ACCEPTED / DONE / REJECTED / FAILED 자체 생성
통신 상실을 FAR로 변환
STALE / INVALID를 FAR로 변환
같은 Request를 새로운 Request ID로 재생성
과거 Request 자동 Replay
```

핵심 원칙:

```text
ESP32 = Wireless Gateway + Bluetooth-side Context Provider

Domain = Vehicle Semantic / Policy Owner
```

---

# 2. 외부 시스템 책임 경계

## 2.1 MOBILE

`CONFIRMED`

MOBILE:

```text
사용자 Request 생성
대상 차량 / Session / Request 식별이 포함된 요청 문맥 유지·전달
Digital Key 사용 설정 요청
상태 조회
Warning 확인
차량 상태 / 결과 / 경고 / 가용성 표시
```

MOBILE이 하지 않는 것:

```text
RSSI 기반 NEAR/FAR 최종 판정
차량 실행 허용 판단
자동 Unlock Command 생성
차량 상태 자체 추정
```

ESP32는 MOBILE의 요청을 차량 의미로 재해석하지 않고 Domain으로 전달한다.

---

## 2.2 ESP32

`CONFIRMED`

ESP32:

```text
Bluetooth Gateway
UART Gateway
Bluetooth Registration / Connection 확인
Proximity 정보 제공
Logical Message Relay
Bluetooth / UART Path 상태 관리
Request 식별 연계 유지

[PROVISIONAL] RSSI 기반 Proximity 판정
[INPUT-TBD] App Active 정보 획득·연계
```

ESP32의 핵심 출력:

```text
Registration / Connection
App Active 정보
Session / Device Context 전달
Proximity NEAR / FAR / UNKNOWN
Proximity Quality / Update 근거
MOBILE Request / Query / Ack
```

---

## 2.3 S32K344 Domain

`CONFIRMED`

Domain:

```text
ESP32가 제공한 Registration / Connection 근거 확인
현재 Session 유효성 판단
Request 중복 / 충돌 관리
차량 Request 수용 판단
기능별 Permission 판단

Digital Key의 새 접근 판단
FAR → NEAR 의미 사용
자동 Unlock Intent / Command 생성

차량 상태 / 결과 / 경고 / 가용성 생성
MOBILE 표시용 의미 변환
```

중요:

```text
ESP32가 NEAR를 만들 수는 있다.
하지만 NEAR 자체가 UNLOCK 명령은 아니다.
```

---

## 2.4 BCM 및 다른 ECU

`CONFIRMED`

ESP32는 BCM / WINDOW / VSS / CIS와 직접 차량 기능 정책을 수행하지 않는다.

```text
MOBILE
  ↕ Bluetooth
ESP32
  ↕ UART
DOMAIN
  ↕ Vehicle Network
BCM / CIS / WINDOW / VSS
```

따라서 ESP32 내부에 다음 기능을 넣지 않는다.

```text
DoorManager
ClimateManager
WindowManager
WarningManager
VehicleEventManager
```

이 기능들은 차량 의미를 판단하는 Domain 또는 해당 실행 ECU의 책임이다.

---

# 3. 전체 Software / Interface Architecture

권장 구조:

```text
                                MOBILE
                                  │
                             Bluetooth
                                  │
                                  ▼
┌───────────────────────────────────────────────────────────────────────┐
│                         ESP32 WIRELESS GATEWAY                        │
│                                                                       │
│  ┌──────────────────── Communication / Transport ──────────────────┐ │
│  │ BluetoothAdapter │ UartAdapter │ FrameCodec │ Rx/Tx Queue       │ │
│  └──────────────────────────────┬──────────────────────────────────┘ │
│                                 │                                     │
│  ┌──────────────────────── Interface Boundary ─────────────────────┐ │
│  │                   Gateway_Interface.h/.c                        │ │
│  └──────────────────────────────┬──────────────────────────────────┘ │
│                                 │                                     │
│  ┌──────────────────────────── Core ───────────────────────────────┐ │
│  │ GatewayRouter        │ LinkStateManager                        │ │
│  │ MessageContextManager                                         │ │
│  └──────────────────────────────┬──────────────────────────────────┘ │
│                                 │                                     │
│  ┌──────────────────── Bluetooth-side Feature ────────────────────┐ │
│  │ DeviceRegistrationManager │ ProximityManager                   │ │
│  │ App Active Context = INPUT-TBD, 별도 Manager는 현재 미생성     │ │
│  └──────────────────────────────┬──────────────────────────────────┘ │
│                                 │                                     │
│  ┌──────────────────────── Service / Supervision ─────────────────┐ │
│  │ GatewayLifecycleManager                                       │ │
│  └──────────────────────────────┬──────────────────────────────────┘ │
│                                 │                                     │
│  ┌──────────────────────── Platform / Application ────────────────┐ │
│  │ Gateway_PolicyConfig │ Network_Config │ Gateway_Time           │ │
│  │ Gateway_Task / ESP-IDF FreeRTOS                               │ │
│  └────────────────────────────────────────────────────────────────┘ │
└───────────────────────────────────────────────────────────────────────┘
                                  │
                                  │ UART
                                  ▼
                         S32K344 DOMAIN CONTROLLER
```

### 핵심 구조 판단

ESP32에서는 Domain처럼 Feature Manager를 많이 만들 필요가 없다.

핵심은 다음 4개 영역이다.

```text
1. Bluetooth / UART 통신 분리
2. 논리 메시지의 의미 보존 중계
3. Bluetooth에서만 얻을 수 있는 Context 생성
4. Restart / Reconnect 시 오래된 정보 차단
```

---

# 4. 계층 간 의존 규칙

## 4.1 Communication Layer

가능:

```text
Bluetooth Rx / Tx
UART Rx / Tx
Frame Boundary 확인
Length 확인
채택된 Integrity 확인 (CRC 등 후보)
채택된 Sequence / 순서 정보 추출
Fragment / Reassembly
Rx / Tx Queue
```

금지:

```text
자동 Unlock 판단
차량 Request 허용 판단
Door 상태 추정
Warning Severity 판단
Domain Result 생성
```

---

## 4.2 Gateway Interface

역할:

```text
Bluetooth Logical Input → ESP32 Core 진입점
UART Logical Input → ESP32 Core 진입점

ESP32 Logical Output → Bluetooth 송신 진입점
ESP32 Logical Output → UART 송신 진입점
```

금지:

```text
차량 Feature Policy
RSSI Threshold 직접 하드코딩
UART Byte 위치 판단을 Feature 코드에 혼합
```

---

## 4.3 Core

Core는 다음 질문에 답한다.

```text
현재 Bluetooth Link는 연결되어 있는가?
현재 UART Link는 사용할 수 있는가?
이 메시지는 어느 방향으로 중계해야 하는가?
원래 Vehicle / Device / Session / Request Context는 무엇인가?
현재 Gateway 상태는 정상 / 복구 중 / 제한 상태인가?
```

Core가 하지 않는 것:

```text
이 Request를 차량이 실행해도 되는가?
NEAR이므로 문을 열어야 하는가?
차량 Result가 성공인가 실패인가?
```

---

## 4.4 Bluetooth-side Feature

ESP32가 실제로 차량 의미를 생성하는 유일한 핵심 Feature 영역이다.

```text
Bluetooth 등록 단말 확인
현재 연결 확인
[PROVISIONAL] RSSI 측정값 처리
[PROVISIONAL] NEAR / FAR / UNKNOWN 생성
Proximity Quality / Update 근거 생성
[INPUT-TBD] App Active 근거 연계
```

단:

```text
NEAR
≠
AUTO UNLOCK COMMAND
```

---

## 4.5 Service / Supervision

차량 기능이 아니라 ESP32 Gateway 운영을 지원한다.

```text
Boot / Restart
Bluetooth reconnect
UART path recovery
Link / Path timeout
Old frame / old message 폐기
Resynchronization
```

---

# 5. 권장 디렉터리 구조

```text
ESP32/
│
├─ common/
│  ├─ Gateway_Types.h                 DESIGN
│  ├─ Gateway_PolicyConfig.h          DESIGN
│  ├─ Gateway_PolicyConfig.c          DESIGN
│  ├─ Gateway_Time.h                  DESIGN
│  └─ Gateway_Time.c                  DESIGN
│
├─ interface/
│  ├─ Gateway_Interface.h             DESIGN
│  └─ Gateway_Interface.c             DESIGN
│
├─ core/
│  ├─ GatewayRouter.h                 DESIGN
│  ├─ GatewayRouter.c                 DESIGN
│  ├─ LinkStateManager.h              DESIGN
│  ├─ LinkStateManager.c              DESIGN
│  ├─ MessageContextManager.h         DESIGN
│  ├─ MessageContextManager.c         DESIGN
│
├─ feature/
│  ├─ DeviceRegistrationManager.h     DESIGN
│  ├─ DeviceRegistrationManager.c     DESIGN
│  ├─ ProximityManager.h              DESIGN / PROVISIONAL METHOD
│  └─ ProximityManager.c              DESIGN / PROVISIONAL METHOD
│
├─ service/
│  ├─ GatewayLifecycleManager.h       DESIGN
│  └─ GatewayLifecycleManager.c       DESIGN
│
├─ communication/
│  ├─ bluetooth/
│  │  ├─ BluetoothAdapter.h           NETWORK-TBD
│  │  ├─ BluetoothAdapter.c           NETWORK-TBD
│  │  ├─ BluetoothProfile.h           NETWORK-TBD
│  │  └─ BluetoothProfile.c           NETWORK-TBD
│  │
│  ├─ uart/
│  │  ├─ UartAdapter.h                NETWORK-TBD
│  │  ├─ UartAdapter.c                NETWORK-TBD
│  │  ├─ UartFrameCodec.h             NETWORK-TBD
│  │  └─ UartFrameCodec.c             NETWORK-TBD
│  │
│  └─ Network_Config.h                NETWORK-TBD
│
├─ app/
│  ├─ Gateway_Task.h                  DESIGN
│  └─ Gateway_Task.c                  DESIGN
│
└─ test/
   ├─ test_link_state_manager.c
   ├─ test_gateway_router.c
   ├─ test_message_context.c
   ├─ test_registration.c
   ├─ test_proximity.c
   ├─ test_uart_frame_codec.c
   ├─ test_restart_reconnect.c
   └─ test_e2e_gateway.c
```

### 현재 구현 상태에 대한 주의

현재 제공된 문서에는 ESP32 실제 소스 코드 구현 완료 현황이 없다.

따라서 위 파일은:

```text
✅ 구현 완료 표시가 아니라
DESIGN 기준의 권장 구조
```

로 본다.

---

# 6. Core Manager 상세 책임

## 6.1 GatewayRouter

`DESIGN`

질문:

```text
"수신한 Logical Message를 어느 Link로 전달해야 하는가?"
```

역할:

```text
MOBILE → Domain Message Routing
Domain → MOBILE Message Routing

Request
State Query
Warning Ack
State
Result
Warning
Availability
Digital Key State
```

중요:

```text
GatewayRouter는 Payload의 차량 의미를 바꾸지 않는다.
```

예:

```text
Domain Result = FAILED
→ ESP32가 임의로 UNKNOWN 또는 DONE으로 변경 금지
```

---

## 6.2 LinkStateManager

`CONFIRMED 책임 / DESIGN 모듈`

Bluetooth와 UART 경로의 **현재 사용 가능성 및 상실/복구 감시**를 한곳에서 관리한다.

현재 프로젝트 규모에서는 별도 `CommunicationMonitor`를 추가하지 않고 이 모듈에 통합한다.

관리:

```text
Bluetooth Connected?
UART Path Available?
Last Valid Bluetooth Rx
Last Valid UART Rx
Loss / Recovery state
```

`Bluetooth Registered?`는 `DeviceRegistrationManager`가 관리한다.

네트워크 상세가 확정되면 다음 감시 근거를 추가할 수 있다.

```text
Expected Period
Timeout
Handshake / Status
Alive Counter
Sequence
Recovery N회 조건
```

위 항목은 현재 `NETWORK-TBD`이며 모두 필요한 필드라는 뜻이 아니다.

특히 UART는 Bluetooth처럼 본질적으로 연결 지향 프로토콜이 아니므로,
이 문서의 `UART Path Available`은 **UART Driver가 초기화됐다는 뜻이 아니라 Domain과 현재 유효한 통신이 가능한지 확인된 상태**를 의미한다.

이를 확인할 방법은 후속 UART 프로토콜에서 정한다.

핵심:

```text
Bluetooth Connected
AND
UART Path Unavailable

→ 차량과 정상 연결된 상태로 취급 금지
```

또한 Link 상태 상실은 차량 결과를 의미하지 않는다.

```text
UART Path Loss
≠
Request FAILED

Bluetooth Disconnect
≠
Proximity FAR
```
---

## 6.3 MessageContextManager

`CONFIRMED 책임 / DESIGN 모듈`

목적:

```text
MOBILE에서 받은 대상 차량 / 단말 / Session / Request 식별을 Domain까지 보존
Domain이 반환한 식별과 결과를 의미 변경 없이 MOBILE 방향으로 보존·중계
```

중요:

> ESP32가 Request Lifecycle 또는 Result 인과관계를 새로 계산하는 모듈은 아니다.
> 원 Request와 결과의 최종 연결·판정은 Domain의 Request/Result 구조가 담당한다.

관리 후보:

```text
Vehicle Context
Device Context
Session ID
Request ID
Message Type
Original Direction
원본 Update / Freshness 근거
Receive Generation                ← 선택 설계 후보
```

하지 않는 것:

```text
새 Request ID 생성
Domain Session Validity 최종 판단
Request DONE/FAILED 판단
```

중요:

```text
같은 Request 재전달
→ 새로운 Request로 변환 금지

같은 상태 / 결과 재전달
→ 원본 정보의 생성·갱신 시점을 새로 만든 것으로 처리 금지
```

---

## 6.4 GatewayLifecycleManager

`DESIGN`

ESP32 전체 Gateway의 Boot / Link wait / 재동기화 상태를 관리한다. 별도 `GatewayStateManager`를 두지 않고 운영 상태를 이 모듈에 합친다.

후보 내부 상태:

```text
STARTUP
LINK_WAIT
SYNCING
READY
DEGRADED
```

이 상태명은 구현 후보이며 SysRS 공통 Enum을 새로 정의하라는 뜻이 아니다.

관리:

```text
Driver / Bluetooth / UART 초기화 완료 여부
현재 Bluetooth Connection 확인 여부
현재 Registration 확인 여부
Domain과의 UART 통신 가능 상태 확인 여부
재시작 / 재연결 후 Resynchronization 완료 여부
Gateway가 현재 Request를 **전송 경로 관점에서** 중계 가능한 상태인지
(차량의 Request 수용 가능 여부를 판단하는 뜻은 아님)
```

주의:

```text
UART Driver Init 완료
≠
Domain 통신 가능 확인 완료
```

실제 UART 통신 가능성 확인 방법은 후속 프로토콜 설계에서 정한다.

---

# 7. Bluetooth-side Feature 상세 책임

## 7.1 DeviceRegistrationManager

`CONFIRMED 책임 / DESIGN 모듈`

관리:

```text
Bluetooth 계층 등록 여부
현재 연결 상대
등록된 Device와 현재 Connection의 연결
Registration change
Unregistration
Reconnect
```

중요:

```text
Bluetooth Address가 보인다
≠
등록된 단말이다
```

또한 차량 실행 허용 최종 판단은 하지 않는다.

```text
Registered / Connected 정보
→ Domain 제공

Domain
→ 차량 수준 Request 수용 판단
```

---

## 7.2 ProximityManager

`CONFIRMED 책임 / PROVISIONAL 입력 방식 / DESIGN 모듈`

확정된 책임:

```text
등록·현재 연결과 연결된 근접 상태 제공
NEAR / FAR / UNKNOWN 의미 제공
Quality / Update 근거 제공
```

현재 잠정 입력 방식:

```text
Bluetooth RSSI
Current Registered Device
Current Connection
Sample Time
Measurement validity
```

출력:

```text
NEAR
FAR
UNKNOWN

Quality
Update / Age 판단 근거
```

주의:

> SysRS는 현재 Bluetooth RSSI를 근접 판단 방법으로 `[잠정]` 적용한다.
> 따라서 `ProximityManager`의 책임은 유지하되 RSSI 자체를 변경 불가능한 Architecture 전제로 고정하지 않는다.

현재 확정되지 않은 값:

```text
NEAR RSSI Threshold
FAR RSSI Threshold
Hysteresis
Filter window
Sample period
Minimum stable count
Timeout / expiry
```

채택되는 수치는 `Gateway_PolicyConfig`에서 관리한다.

### 중요한 상태 규칙

다음은 `FAR`가 아니다.

```text
Bluetooth Disconnect
RSSI 미수신
RSSI STALE
RSSI INVALID
Device 변경
초기 상태
```

위 상황은:

```text
UNKNOWN / unavailable
```

계열로 처리하는 것이 요구 의미와 일치한다.

### ESP32와 Domain의 역할 분리

```text
ESP32
[PROVISIONAL] RSSI
 ↓
Filter / Validity
 ↓
NEAR / FAR / UNKNOWN
 ↓ UART
Domain
 ↓
FAR → NEAR + Session + Setting + Door + Permission
 ↓
Auto Unlock 판단
```

---

## 7.3 App Active Context

`CONFIRMED 필요 정보 / INPUT-TBD`

필요 의미:

```text
App Active
App Inactive
App Activity Unknown
```

하지만 현재 다음은 확정되지 않았다.

```text
MOBILE이 별도 상태 메시지로 제공하는가?
Bluetooth 연결 계층이 제공 가능한 정보가 있는가?
갱신 주기와 유효 시간은 얼마인가?
```

중요:

```text
Bluetooth Connected
≠
App Active

Bluetooth Disconnect
≠
반드시 App Inactive
```

따라서 현재 Architecture에서는 별도 `AppActivityTracker` 모듈을 먼저 만들지 않는다.

권장:

```text
App Active Producer / 갱신 계약 확정
        ↓
Gateway_Interface 입력으로 수용
        ↓
단순 전달이면 별도 Manager 없이 Domain으로 중계
        ↓
필터링 / 시간 판단 책임이 실제로 생길 때만 모듈 분리 검토
```

실제 획득 방법이 확정되기 전에는 Bluetooth 연결 상태만으로 `APP_ACTIVE`를 생성하지 않는다.
---

# 8. ESP32 ↔ Domain Logical Interface Matrix

## 8.1 ESP32 → Domain

| Logical Information | ESP32 생성/중계 | 권장 내부 담당 |
|---|---|---|
| Registration | 생성 | DeviceRegistrationManager |
| Current Bluetooth Connection | 생성 | LinkStateManager / DeviceRegistrationManager |
| App Active | 획득 방법 미정, 확인 정보 연계 | Gateway_Interface / 별도 Manager 미정 |
| Session / Device Context | 보존·중계 | MessageContextManager |
| Proximity `NEAR/FAR/UNKNOWN` | 생성 | ProximityManager |
| Proximity Quality / Update basis | 생성 | ProximityManager |
| MOBILE_REQUEST | 중계 | GatewayRouter |
| STATE_QUERY | 중계 | GatewayRouter |
| WARNING_ACK | 중계 | GatewayRouter |
| 기타 MOBILE Logical Input | 중계 | GatewayRouter |

---

## 8.2 Domain → ESP32 → MOBILE

| Logical Information | ESP32 역할 | 권장 내부 담당 |
|---|---|---|
| Request Result | 의미 변경 없이 중계 | GatewayRouter |
| Vehicle/MOBILE State | 의미 변경 없이 중계 | GatewayRouter |
| Warning | 우선 전달 경로로 중계 | GatewayRouter |
| Function Availability | 의미 변경 없이 중계 | GatewayRouter |
| Digital Key State / Result | 의미 변경 없이 중계 | GatewayRouter |
| Fault / Display Data | 의미 변경 없이 중계 | GatewayRouter |

ESP32는 위 정보를 자체 재판정하지 않는다.

> `MOBILE_REQUEST`, `STATE_QUERY`, `WARNING_ACK` 같은 이름은 **논리 정보 종류를 설명하기 위한 표기**이다. SysRS는 조회·경고 확인의 의미와 방향을 정의하지만, 이를 반드시 서로 다른 Wire Message Type으로 만들라고 확정하지 않는다. 실제 메시지 분할 방식은 `NETWORK-TBD`이다.

---

# 9. Request 중계 구조

```text
MOBILE
  │
  │ Bluetooth Request
  ▼
BluetoothAdapter
  │
  ▼
Gateway_Interface
  │
  ▼
MessageContextManager
  │ ID / Session / Vehicle Context 유지
  ▼
GatewayRouter
  │
  ▼
UartFrameCodec
  │
  ▼
UartAdapter
  │
  ▼
S32K344 Domain
```

Domain 처리 후:

```text
S32K344 Domain
  │
  │ Result / State / Warning
  ▼
UartAdapter
  │
  ▼
UartFrameCodec
  │
  ▼
Gateway_Interface
  │
  ▼
GatewayRouter
  │
  ▼
BluetoothAdapter
  │
  ▼
MOBILE
```

### 중계 시 보존해야 하는 의미

```text
Vehicle Identity
Device Context
Session
Request ID
Message Type
Result State
Original Freshness basis
Warning occurrence identity
Restart / Generation context      ← 채택 시 DESIGN 후보
```

Wire Format은 `NETWORK-TBD`.


### Request 전달 실패 / 경로 상실 원칙

ESP32는 차량 결과를 생성하지 않지만, **전송 경로 상태는 구분**해야 한다.

```text
Request 수신 시 UART Path unavailable
→ 차량이 ACCEPTED / REJECTED / FAILED 했다고 만들지 않음
→ 해당 Request를 연결 복구 후 자동 실행되도록 보관하지 않음
→ 현재 연결 / 전송 불가 상태를 MOBILE이 구분할 수 있는 방식으로 처리
```

특히:

```text
UART 복구
≠
과거 Request 자동 Forward / Replay
```

재전송이 필요한 경우에도 원 Request 식별과 처음 정한 시간 근거를 유지해야 하며,
재전송 때문에 Request Age 또는 원본 정보 Freshness가 새로 시작되어서는 안 된다.

---

# 10. Bluetooth Link / UART Link 분리

ESP32에서 가장 중요한 설계 원칙 중 하나다.

```text
Bluetooth Link State
≠
UART Link State
```

예:

```text
Bluetooth = CONNECTED
UART = OFFLINE

→ MOBILE과 ESP32는 연결됨
→ 하지만 차량 Domain과 정상 연결된 것은 아님
```

따라서 사용자에게 보여 줄 전체 연결 상태를 ESP32가 임의로 `CONNECTED`로 확정하지 않는다.

권장 내부 정보:

```text
BtLinkState
UartLinkState
RelayReadiness
```

`RelayReadiness`는 ESP32의 **중계 경로 준비 상태만 나타내는 내부 운영값**으로 둘 수 있다.

```text
READY
DEGRADED
UNAVAILABLE
```

단, `RelayReadiness`는 차량 기능별 `Function Availability`와 다른 개념이다.
차량 기능의 사용 가능 여부는 Domain이 판단한다.

---

# 11. Digital Key 전체 흐름

## 11.1 근접 상태 생성

```text
Registered Bluetooth Device
        │
        ▼
Current Connection
        │
        ▼
RSSI samples
        │
        ▼
ProximityManager
        │
        ├─ NEAR
        ├─ FAR
        └─ UNKNOWN
        │
        ▼
Quality / Update basis
        │
        ▼
UART → Domain
```

---

## 11.2 자동 Unlock 판단

ESP32에서 하지 않는다.

```text
ESP32
Registration
Connection
App Active
Proximity
Quality
        │
        ▼
Domain
        │
        ├─ Session Valid?
        ├─ Auto Unlock Setting ON?
        ├─ Valid FAR → NEAR?
        ├─ Door State valid?
        ├─ Function Available?
        └─ Permission pass?
        │
        ▼
BCM UNLOCK Command
```

즉:

```text
ProximityManager
→ "가까워졌다"까지만 판단

DigitalKeyManager(Domain)
→ "문을 열 것인가" 판단
```

---

# 12. Freshness / Link Liveness / Proximity Quality 분리

세 개를 같은 의미로 사용하지 않는다.

```text
Proximity Quality
= 현재 근접 판단 결과를 사용할 수 있는가?

Update / Freshness basis
= 그 근접 정보가 언제 새로 생성·평가됐는지 판단할 근거

Domain의 Freshness 평가
= 전달받은 Update 근거와 사용 시점을 이용해 현재 사용 가능한 최신 정보인지 판단

Link Liveness
= Bluetooth/UART 통신 자체가 살아 있는가?
```

예:

```text
Bluetooth Link = CONNECTED
RSSI 갱신 중단
→ Link는 살아 있을 수 있음
→ Proximity는 STALE / UNKNOWN 가능

UART Link = OFFLINE
→ 과거 Proximity를 새 정보처럼 Domain에 사용 금지
```

중계의 중요한 규칙:

```text
Domain / MOBILE 원본 정보가 다시 전달됨
→ ESP32가 재전송 시점으로 원본 Freshness를 초기화하지 않음

같은 상태값을 다시 전송함
→ 새 관측 / 새 평가인지 확인 근거가 없으면 새 정보로 만들지 않음
```

---

# 13. Message Framing / Integrity Boundary

`NETWORK-TBD`

UART는 Byte Stream이므로 **개별 메시지의 시작·길이·완료 여부와 유효성**을 구분할 수 있어야 한다. 다만 이를 어떤 필드로 구현할지는 아직 확정되지 않았다.

예를 들어 다음 요소들은 후속 프로토콜에서 선택할 수 있는 설계 후보이다.

```text
Header / Start pattern
Message Type
Length
Payload
Sequence 또는 Restart Generation
CRC / Checksum / 기타 Integrity
Delimiter 또는 Length 기반 종료
```

후속 설계에서 결정:

```text
UART Port
Baud Rate
Data Bits / Parity / Stop
메시지 Boundary 표현
Message Type 구성
Length 표현
순서 식별 방식
무결성 확인 방식
Fragmentation 필요 여부
Timeout / Recovery 기준
```

중요:

```text
불완전 Message
Boundary / Length 불일치
채택된 무결성 확인 실패
지원하지 않는 Message Type
복구 후 이전 실행 구간의 지연 Message

→ 정상 Request / State / Result로 사용 금지
```

`CRC`, `Sequence`, `Generation`은 현재 **필수 확정 필드가 아니라 NETWORK-TBD 설계 후보**이다.

---

# 14. Bluetooth Protocol Boundary

`NETWORK-TBD`

확정:

```text
MOBILE ↔ ESP32 = Bluetooth
```

미정:

```text
BLE / Classic 세부 사용 형태
GATT Service / Characteristic
Profile UUID
Notification / Indication
Write type
MTU
Fragmentation
JSON / Binary
Connection parameter
Pairing / Bonding 세부 절차
```

따라서 Feature Logic에서 GATT UUID나 Packet Byte 위치를 직접 사용하지 않는다.

```text
BluetoothAdapter / BluetoothProfile
       ↓
Logical DTO
       ↓
Gateway_Interface
```

구조를 유지한다.

---

# 15. Gateway_PolicyConfig와 Network_Config 분리

## 15.1 Gateway_PolicyConfig

Bluetooth-side 기능 정책 / Calibration:

```text
RSSI NEAR Threshold
RSSI FAR Threshold
Hysteresis
Filter window
Stable sample count
Proximity expiry
Proximity filtering / stabilization policy
```

수치는 현재 `TBD`.

---

## 15.2 Network_Config

Wire / Transport 상세:

```text
Bluetooth Service / Characteristic
MTU
Bluetooth message format

UART Port
UART Baud Rate
UART Message boundary / length
Message Type 구성
Integrity 방식 (CRC 등 후보)
순서 식별 방식 (Sequence 등 후보)
Timeout / Recovery
```

원칙:

```text
ProximityManager가 UART Byte 위치를 알지 않는다.
GatewayRouter가 Bluetooth UUID를 직접 알지 않는다.
```

---

# 16. Boot / Restart / Resynchronization

## 16.1 ESP32 Boot

권장 흐름:

```text
RESET
 ↓
ESP-IDF / Driver Init
 ↓
Bluetooth Init
 ↓
UART Init
 ↓
Gateway Core Init
 ↓
STARTUP
 ↓
LINK_WAIT
 ↓
현재 Bluetooth Registration / Connection 확인
 ↓
Domain과 UART 통신 가능 상태 확인
 ↓
SYNCING
 ↓
새 Current Context 확보
 ↓
READY
```

---

## 16.2 Bluetooth reconnect

```text
Bluetooth Disconnect
 ↓
Current Connection invalid
 ↓
Proximity 사용 불가 / UNKNOWN 처리
 ↓
App Active 확인 근거도 별도로 무효화하거나 UNKNOWN 처리
(Disconnect만으로 APP_INACTIVE를 확정하지 않음)
 ↓
새 Request 전달 중단
 ↓
해당 단절 구간 Request를 복구 후 자동 Forward할 Queue로 보존하지 않음
 ↓
Reconnect
 ↓
현재 Registered Device 재확인
 ↓
현재 Connection 재확인
 ↓
새 Context 확보
 ↓
Domain에 현재 상태 갱신
```

금지:

```text
Disconnect를 FAR로 변환
Reconnect 직후 과거 NEAR 사용
과거 Request 자동 Replay
```

---

## 16.3 UART reconnect

```text
UART unavailable
 ↓
Domain path unavailable
 ↓
MOBILE Request를 차량 Accepted로 표시 금지
 ↓
해당 Request를 UART 복구 후 자동 실행되도록 보관하지 않음
 ↓
UART path recovery 확인
 ↓
Rx buffer / 이전 실행 구간 Message 폐기·검증
 ↓
현재 Registration / Connection / Session 관련 Context 재확인
 ↓
필요 차량 State 재조회 / 재전달
 ↓
새 정상 통신

※ 실제 Resync Message와 재조회 Trigger는 NETWORK-TBD
```

금지:

```text
이전 실행 구간의 지연 Frame을 최신 Result로 사용
남아 있던 Request 자동 재실행
```

---

# 17. Security / Robustness Boundary

## Bluetooth Layer

```text
Bluetooth 연결 계층의 기기 등록 절차
Pairing / Bonding 등 실제 구현 방식
Registered Device
Current Connection
```

별도 앱 계정·사용자별 권한 체계를 ESP32에 추가하지 않는다. 등록용 내부 키는 Bluetooth 연결 계층이 관리하며, 실제 저장·삭제 방법은 사용하는 Bluetooth Stack과 인터페이스 설계에서 확정한다.

## ESP32 Gateway

```text
Device / Connection Context 유지
RSSI 근접 품질
Message Boundary
Request 식별 보존
Old frame 차단
```

## Domain

```text
Session Validity
Vehicle-level Request Admission
Permission
Duplicate / Conflict
Command / Result tracking
```

주의:

```text
CRC
≠
단말 인증
```

또한:

```text
Bluetooth Connected
≠
Registered Device

Registered Device
≠
Vehicle Request Permission

NEAR
≠
Unlock Permission
```

---

# 18. ESP-IDF / FreeRTOS 권장 Task 구조

ESP32는 이벤트 중심 구조로 잡는 것을 권장한다.

```text
Bluetooth Event / Callback
        ↓
Bluetooth Rx Queue
        │
        │
UART Driver Event
        ↓
UART Rx Queue
        │
        └──────────────┐
                       ▼
                 Gateway Task
                       │
                       ├─ GatewayLifecycleManager
                       │    └─ STARTUP / LINK_WAIT / SYNCING / READY / DEGRADED 후보
                       ├─ LinkStateManager
                       ├─ MessageContextManager
                       ├─ DeviceRegistrationManager
                       ├─ ProximityManager
                       ├─ App Active Context 전달 (INPUT-TBD)
                       └─ GatewayRouter
                       │
                ┌──────┴──────┐
                ▼             ▼
          Bluetooth Tx     UART Tx Queue
                │             │
                ▼             ▼
        BluetoothAdapter   UartAdapter
```

### 권장 원칙

```text
Bluetooth callback에서 차량 정책 실행 금지
UART driver callback에서 복잡한 Routing 실행 금지

Callback / ISR
→ 최소 수신 처리
→ Queue
→ Gateway Task에서 상태 / Routing 처리
```

### Task를 과도하게 나누지 않는다

현재 프로젝트 규모에서는 다음 정도로 충분하다.

```text
1. Gateway Main Task
2. UART Rx/Event Task
3. Bluetooth Stack / Event Context
4. 필요 시 Proximity sampling timer/event
```

`ProximityManager`, `LinkStateManager` 등을 각각 독립 Task로 만들 필요는 없다.

---

# 19. 주요 End-to-End 흐름

## 19.1 MOBILE Door Unlock Request

```text
MOBILE
 ↓ Bluetooth
ESP32 BluetoothAdapter
 ↓
Gateway_Interface
 ↓
MessageContextManager
 ↓
GatewayRouter
 ↓
UartFrameCodec
 ↓ UART
S32K344 Domain
 ↓
Request / Permission / Command
 ↓
BCM
 ↓
Result
 ↓
Domain
 ↓ UART
ESP32 GatewayRouter
 ↓ Bluetooth
MOBILE
```

ESP32는 Door Unlock을 판단하지 않는다.

---

## 19.2 Digital Key Proximity

```text
MOBILE / Bluetooth Device
 ↓
Bluetooth RSSI
 ↓
ProximityManager
 ↓
NEAR / FAR / UNKNOWN
+ Quality / Update basis
 ↓
UART
 ↓
Domain DigitalKeyManager
 ↓
BCM UNLOCK 여부 판단
```

---

## 19.3 Domain State Update

```text
Domain MobileStateMapper
 ↓
UART Logical DTO
 ↓
ESP32 UartAdapter
 ↓
GatewayRouter
 ↓
BluetoothAdapter
 ↓
MOBILE
```

ESP32는 차량 상태를 다시 계산하지 않는다.

---

## 19.4 Warning

```text
Domain Warning
 ↓
UART
 ↓
ESP32
 ↓
Bluetooth
 ↓
MOBILE
```

안전 경고는 일반 주기 State 전송을 기다리지 않고 전달해야 하므로, 실제 구현에서는 Queue 분리·우선 처리·즉시 Notify 등 그 요구를 만족하는 방식을 선택해야 한다. 특정 Queue 구조 자체는 `DESIGN`이다.

단, Warning의 위험도와 해제 조건은 ESP32가 판단하지 않는다.

---

# 20. Error Handling 원칙

## Bluetooth 오류

```text
Disconnect
Pairing/Bonding invalid
Unknown device
RSSI unavailable
App activity unknown
```

처리:

```text
Link / Context invalid 처리
필요 상태 Domain 전달
새 차량 Request 제한
Proximity를 FAR로 조작하지 않음
```

---

## UART 오류

```text
Message incomplete
Length / Boundary invalid
채택된 Integrity 확인 실패
채택된 순서 식별 오류
Timeout
UART Path unavailable
```

처리:

```text
해당 Frame 폐기
차량 Result 임의 생성 금지
현재 UART Path 상태 갱신
차량 Result가 아닌 통신/연결 상태로 처리
복구 후 Resynchronization
```

---

# 21. Open / TBD Register

## Bluetooth

- Bluetooth 세부 Profile / Service 구조
- BLE / Classic 세부 선택
- GATT UUID / Characteristic
- MTU / Fragmentation
- Notification / Indication 방식
- Pairing / Bonding 실제 절차
- 등록 참조 저장 / 삭제 방식

## UART

- UART Port
- Baud Rate
- Message Boundary 표현
- Message Type 구성
- Length 표현
- 순서 식별 방식
- Integrity 방식
- Timeout
- Recovery 조건

## Message

- JSON / Binary
- Logical DTO Field
- Field Type / Width
- Session ID 표현
- DeviceContextId 표현
- Message Generation / Restart Generation
- Warning occurrence 식별 표현

## Digital Key

- RSSI NEAR Threshold
- RSSI FAR Threshold
- Hysteresis
- RSSI Filter 방식
- Sampling 주기
- Stable 판정 조건
- Proximity 유효 시간
- Quality 정의
- App Active 획득 방법

## Reconnect

- Bluetooth 자동 재연결 정책
- UART 연결 상실 판단
- Resync Message
- 상태 재조회 방법
- 이전 Frame 폐기 기준

---

# 22. 현재 명시적으로 추가하지 않는 모듈

## VehiclePolicyManager

추가하지 않는다.

차량 정책은 Domain 책임이다.

---

## Door / Climate / Window Manager

추가하지 않는다.

ESP32는 개별 차량 기능을 직접 실행하지 않는다.

---

## RequestResultManager

Domain과 같은 의미로는 추가하지 않는다.

ESP32는 `DONE/FAILED`를 판단하지 않는다.

필요한 것은:

```text
MessageContextManager
```

를 통한 식별 보존이다.

---

## PersistenceManager

범용 `PersistenceManager`를 현재 Architecture 필수로 추가하지 않는다.

다만 **Bluetooth 기기 등록용 내부 정보의 저장·삭제 자체가 불필요하다는 뜻은 아니다.** SysRS상 등록용 내부 키는 Bluetooth 연결 계층이 관리하고, 차량 측 등록 기록은 별도로 제거할 수 있어야 한다. 실제 NVS 사용 여부, Bluetooth Stack의 Bonding DB 사용 여부와 등록 해제 절차는 후속 상세 설계에서 확정한다.

---

## Global Security Manager

현재 별도 모듈로 만들지 않는다.

Bluetooth Pairing/Bonding, Frame Integrity, Domain Session 책임을 각 경계에서 유지한다.

향후 별도 암호학적 인증 요구가 추가되면 재검토한다.

---

# 23. 권장 구현 순서 v0.3

## Phase 1 - Gateway 공통 기반

```text
1. Gateway_Types
2. Gateway_Time
3. Gateway_Interface
4. LinkStateManager
5. MessageContextManager
6. GatewayRouter
7. GatewayLifecycleManager
```

이 단계에서는 실제 Bluetooth/UART Wire Format이 없어도 Mock Interface로 테스트할 수 있다.

---

## Phase 2 - Bluetooth-side Feature

```text
8. DeviceRegistrationManager
9. ProximityManager
10. App Active 입력 계약 확정 시 Gateway_Interface에 반영
```

App Active는 Producer와 갱신 계약이 확정되기 전까지 별도 Manager 구현을 보류한다.

---

## Phase 3 - Communication Integration

네트워크 / 인터페이스 결정 후:

```text
11. Network_Config
12. UartFrameCodec
13. UartAdapter
14. BluetoothProfile
15. BluetoothAdapter
16. LinkStateManager에 실제 Timeout / Recovery 감시 연동
```

별도 `CommunicationMonitor`는 현재 만들지 않는다.

---

## Phase 4 - ESP-IDF / FreeRTOS Integration

```text
17. Gateway_Task
18. UART Event / Queue 연결
19. Bluetooth Callback / Queue 연결
20. Proximity Update Timer/Event
```

---

## Phase 5 - Verification

```text
21. Module Unit Test
22. Bluetooth Disconnect Test
23. UART Path Loss Test
24. Restart / Reconnect Test
25. Old / Delayed Message Injection Test
26. Duplicate Request Relay Test
27. [PROVISIONAL] RSSI Boundary / Stability Test
28. Freshness Preservation Test
29. End-to-End MOBILE ↔ Domain Test
```

---

# 24. 테스트 우선순위

최소 다음 Scenario를 검증한다.

```text
1. Bluetooth CONNECTED + UART Path AVAILABLE
   → MOBILE Request가 동일 ID로 Domain까지 전달됨

2. Bluetooth CONNECTED + UART Path UNAVAILABLE
   → 차량 정상 연결로 표시하지 않음

3. UART Path UNAVAILABLE 상태에서 Request
   → ESP32가 ACCEPTED / DONE 등을 생성하지 않음

4. Same Request retransmission
   → 새 Request ID 생성 없음

5. Malformed UART Frame
   → Logical Message로 전달하지 않음

6. ESP32 Restart
   → 이전 미완료 Request 자동 Replay 없음

7. UART Path Recovery
   → 선택된 순서/동기화 방식에 따라 이전 실행 구간의 지연 Message를 새 상태로 오인하지 않음

8. Bluetooth Disconnect
   → Proximity FAR 생성 금지

9. [PROVISIONAL] RSSI 입력 STALE / INVALID
   → Proximity FAR 생성 금지

10. Valid FAR → NEAR
    → ESP32는 NEAR만 제공
    → Unlock 최종 판단은 Domain

11. Reconnect 직후 이미 NEAR
    → ESP32는 현재 Proximity를 제공할 수 있으나
    → 새 접근 인정 여부는 Domain이 판단

12. Domain Result
    → ESP32가 의미 변경 없이 MOBILE로 중계

13. Warning
    → 일반 State 주기 전송을 기다리지 않고 전달되는지 확인

14. App Activity Unknown
    → Active로 추정하지 않음

15. UART Path unavailable 동안 Request 수신
    → 복구 후 자동 Forward / Replay하지 않음
    → 차량 Result를 ESP32가 생성하지 않음

16. 동일 State / Result 재중계
    → ESP32 재전송 시점으로 원본 Freshness / Age를 초기화하지 않음
```

---

# 25. Domain Architecture와의 연결

Domain 문서 기준으로 ESP32 관련 입력은 다음과 연결된다.

```text
ESP32 Registration / Connection
→ GatewaySessionManager
→ VehicleStateManager

ESP32 App Active
→ GatewaySessionManager
→ DigitalKeyManager

ESP32 Proximity
→ VehicleStateManager
→ DigitalKeyManager

ESP32 MOBILE_REQUEST
→ GatewaySessionManager
→ RequestManager

Domain Request Result / State / Warning / Availability
→ MobileStateMapper 등
→ ESP32
→ MOBILE
```

따라서 양쪽 Architecture는 다음 경계에서 맞춘다.

```text
ESP32 Gateway_Interface
         ↕
UART Logical Protocol
         ↕
Domain UartAdapter
         ↕
Domain_Interface
```

---

# 26. ESP32와 Domain의 가장 중요한 책임 분리

## ESP32가 판단하는 것

```text
Bluetooth 등록된 단말인가?
현재 Bluetooth 연결이 있는가?
현재 근접 판단 입력은 사용 가능한가?
[PROVISIONAL] RSSI 기준으로 현재 NEAR / FAR / UNKNOWN 중 무엇인가?
Bluetooth Link와 UART Path는 사용할 수 있는가?
수신 Message의 Boundary / Length / 채택된 무결성 조건은 유효한가?
```

## Domain이 판단하는 것

```text
현재 Session을 차량 Request에 사용할 수 있는가?
이 Request를 수용할 수 있는가?
이 기능을 실행해도 되는가?
FAR → NEAR가 새로운 Approach인가?
자동 Unlock을 생성해야 하는가?
어떤 ECU Command를 내려야 하는가?
결과가 DONE / FAILED / REJECTED인가?
어떤 Warning을 사용자에게 보여야 하는가?
```

---

# 27. v0.1 → v0.2 주요 수정 사항

```text
1. GatewayStateManager 제거
   → GatewayLifecycleManager에 운영 상태를 통합하여 중복 모듈 축소

2. UART Link 의미 보완
   → Driver Init과 Domain 통신 가능 상태를 분리
   → 실제 확인 방식은 NETWORK-TBD 유지

3. App Active 처리 보완
   → Bluetooth Disconnect만으로 APP_INACTIVE를 생성하지 않음
   → 획득 방법은 INPUT-TBD 유지

4. UART Frame 요구 완화
   → Header/CRC/Sequence/Generation을 필수 확정 구조처럼 표현하지 않음
   → Message Boundary/Length/Validity 요구만 확정, 세부 필드는 NETWORK-TBD

5. MessageContextManager 책임 축소·명확화
   → 식별 보존/중계만 담당
   → Request Lifecycle 및 Result 인과 판정은 Domain 책임

6. STATE_QUERY / WARNING_ACK 표기 보완
   → 논리 정보 종류이지 반드시 독립 Wire Message Type이라는 뜻은 아님

7. Persistence 경계 보완
   → 범용 PersistenceManager는 미추가
   → Bluetooth 등록 내부 정보 저장/삭제 책임 자체는 유지

8. Old Frame / Generation 표현 보완
   → Generation 필드는 선택 설계 후보
   → 복구 후 이전 실행 구간 메시지 차단이라는 요구만 유지
```

---

# 28. v0.2 → v0.3 주요 수정 사항

```text
1. Source status 정교화
   → ESP32의 근접 정보 제공 책임과 RSSI라는 구현 수단을 분리
   → RSSI 기반 판정은 SysRS의 [잠정]을 반영해 PROVISIONAL로 변경

2. CommunicationMonitor 제거
   → ESP32는 Bluetooth/UART 두 경로 중심이므로
      LinkStateManager에 Loss / Timeout / Recovery 감시를 통합

3. GatewayDiagnosticManager 제거
   → 현재 SysRS에 별도 Gateway 진단 Manager 책임이 없음
   → Link / Adapter 오류를 우선 해당 경계에서 관리

4. AppActivityTracker 구현 보류
   → App Active는 필요 정보이나 Producer / 갱신 방법이 INPUT-TBD
   → 단순 전달이면 별도 Manager를 만들지 않음

5. GatewayAvailability 명칭 수정
   → 차량 Function Availability와 혼동을 막기 위해 RelayReadiness로 변경

6. Request outage 처리 보강
   → UART Path 상실 중 Request를 복구 후 자동 Forward / Replay하지 않음
   → ESP32가 차량 ACCEPTED / FAILED 등을 생성하지 않음

7. Freshness relay 규칙 추가
   → 중계·재전송으로 원본 생성/갱신 시점을 새로 만들지 않음
   → 같은 상태 재전달을 새 관측으로 오인하지 않음

8. Network 후보 표현 추가 정리
   → CRC / Sequence / Header 등은 채택 가능한 후보일 뿐
      SysRS에서 고정된 필드가 아님을 유지
```

---

# 29. 최종 구조 판단

현재 프로젝트 규모에서는 ESP32를 다음과 같이 보는 것이 가장 적절하다.

```text
ESP32
=
Communication Gateway
+
Bluetooth Device Context Provider
+
Proximity Provider
(현재 RSSI 방식은 PROVISIONAL)
```

구조 원칙은 다음과 같다.

```text
Bluetooth / UART Link 상태 분리
UART Driver 초기화 / Domain 통신 가능 상태 분리
Transport / Logical Message 분리
Gateway / Vehicle Policy 분리
Registration / Connection / Proximity 분리
RSSI Raw / Proximity State 분리
Proximity State / Auto Unlock 판단 분리
Request Identity / Request Result 판단 분리
Freshness / Link Liveness 분리
원본 Freshness / Relay 시점 분리
Reconnect / New Request 분리
Link 감시 / Vehicle Result 분리
Wire Config / Proximity Policy Config 분리
```

가장 중요한 경계는 다음 한 줄이다.

```text
ESP32는 "무슨 정보가 들어왔고, 어떤 Bluetooth 상황인가"를 책임지고,
S32K344 Domain은 "그래서 차량이 무엇을 해야 하는가"를 책임진다.
```

이 구조를 기준으로 ESP32 구현을 진행하면,
Bluetooth Profile이나 UART Frame이 이후 변경되더라도
`ProximityManager`, `GatewayRouter`, `MessageContextManager`, `LinkStateManager`와 Domain의 차량 정책 코드를 최대한 독립적으로 유지할 수 있다.
