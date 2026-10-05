# S32K344 Domain Controller 전체 Software / Interface Architecture v0.4

> 작성 기준일: 2026-09-30  
> 구현 상태 업데이트: Domain_PolicyConfig / Domain_Time / DomainLifecycleManager / GatewaySessionManager 구현 완료  
> 기준 문서: 현재 SR / SysRS / Domain Interface Definition / Domain Master Interface Matrix / 구현된 Domain Core v0.2  
> 목적: S32K344 Domain Controller의 **외부 논리 Interface, 내부 Software 계층, 모듈 책임, 데이터 흐름, 오류·보안·재동기화 구조와 구현 순서**를 하나의 기준 문서로 관리한다.

---

# 0. 문서 상태 표기

이 문서에서는 요구사항과 구현 제안을 혼동하지 않도록 다음 상태를 사용한다.

| 표기 | 의미 |
|---|---|
| `CONFIRMED` | 현재 SR/SysRS/검토된 Logical Interface에서 책임 또는 의미가 확인됨 |
| `DESIGN` | 위 요구를 구현하기 위해 Domain 내부 Architecture로 제안·채택한 구조 |
| `TBD` | Producer, 수치, 실제 전송 방식 또는 정책이 아직 확정되지 않음 |
| `NETWORK-TBD` | 논리 의미는 있지만 CAN/UART ID·Byte·주기·Timeout 등 실제 Wire 설계가 미정 |
| `OUT-OF-SCOPE` | 현재 범위에서 구현하지 않음 |
| `OUT-OF-SCOPE-CANDIDATE` | 제외 방향이나 팀 최종 확정 필요 |

중요:

> `Manager`라는 파일/모듈 이름은 대부분 **구현 Architecture(DESIGN)** 이다.  
> 그 Manager가 담당하는 차량 기능 책임은 SR/SysRS의 논리 요구를 기반으로 한다.

---

# 1. Domain Controller의 역할

S32K344 Domain Controller는 단순 CAN/UART Gateway가 아니다.

현재 Domain의 주요 책임은 다음과 같다.

```text
MOBILE Request 수용
등록 / 현재 연결 / Session 근거 확인
Request 중복·충돌 관리
기능별 공통 실행 허용 확인
차량 현재 상태와 Quality/Freshness 관리
사용자 확정 설정 관리
차량 기능별 정책 판단
자동 기능 판단
ECU Command 생성·추적
Command ↔ Request ↔ Result 연결
차량 수준 Warning 판단
단발 Semantic Event 생성
ECU/Link Liveness 감시
Fault / Function Availability 집계
MOBILE 표시용 상태 의미 변환
재시작·재연결 후 재동기화
```

반대로 다음은 Domain의 책임이 아니다.

```text
센서 Raw 측정
WINDOW 로컬 Anti-Pinch 즉시 보호
BCM 액추에이터 로컬 보호
VSS 실제 Sound Asset 선택/재생 중재
ESP32 RSSI Raw 측정
CAN/UART 실제 Byte/Bit Mapping
```

---

# 2. 외부 시스템 책임 경계

## 2.1 MOBILE

`CONFIRMED`

MOBILE:

```text
사용자 Request 생성
Request ID 관리
차량에서 확인한 설정/상태/결과/경고/가용성 표시
```

MOBILE이 하지 않는 것:

```text
차량 실행 허용 최종 판단
NEAR/FAR 판정
액추에이터 직접 제어
차량 상태 자체 보정
```

현재 MOBILE → WINDOW 원격 제어는 `OUT-OF-SCOPE`.

---

## 2.2 ESP32

`CONFIRMED`

ESP32:

```text
MOBILE ↔ Bluetooth 통신
ESP32 ↔ Domain UART Gateway
등록된 Bluetooth 단말 확인
현재 Bluetooth 연결 확인
App Active 정보 제공 방법 연계
RSSI 기반 NEAR / FAR / UNKNOWN
Proximity Quality / Update 근거 제공
Request / State / Result / Warning 중계
```

ESP32가 하지 않는 것:

```text
자동 Unlock 최종 결정
차량 Request 수용 최종 판단
BCM 직접 구동
결과를 임의로 DONE/FAILED로 생성
통신 상실을 FAR로 생성
```

---

## 2.3 BCM

`CONFIRMED`

BCM:

```text
Door 명령 실행
Climate/Fan/Thermal 명령 실행
Interior Light 명령 실행
직접 관측 가능한 적용 상태 제공
로컬 보호
명령 Result / Event / Fault 제공
```

Domain:

```text
사용자 설정 관리
최종 차량 수준 목표 생성
상위 실행 허용 판단
결과의 원 Request 연결
```

중앙이 실행을 허용해도 BCM의 구동 직전 Local Safety 확인은 유지된다.

---

## 2.4 CIS

`CONFIRMED`

CIS:

```text
Occupant Presence / Count
Cabin Temperature
Cabin Humidity
Cabin Illuminance
Rear Distance
각 값의 Validity / Quality / Freshness 근거
CIS State / Function Status / Fault
```

Domain:

```text
Rear Distance → 차량 수준 위험 판단
센싱 정보를 Climate / Warning / Auto Function에 활용
```

CIS가 하지 않는 것:

```text
Rear CLEAR / CAUTION / EMERGENCY 차량 수준 판단
VSS 직접 경고 명령
```

CIS ↔ Domain의 **최종 물리 전송 경로는 Network 설계 결과를 따른다.**

---

## 2.5 WINDOW

`CONFIRMED`

WINDOW:

```text
Local Switch 및 확정 Domain Command 실행
Window Position / Motion / ECU State 제공
Anti-Pinch 검출
즉시 정지/반전 로컬 보호
Command Result
Fault
```

Domain:

```text
상위 Window 목표 생성
Auto Ventilation 작업 판단
Anti-Pinch 차량 경고 의미 생성
```

MOBILE → WINDOW 직접 원격 제어는 현재 `OUT-OF-SCOPE`.

---

## 2.6 VSS

`CONFIRMED`

Domain은 Sound ID를 보내지 않는다.

Domain → VSS:

```text
Semantic One-shot Event
Stateful Warning State
```

예:

```text
VEHICLE_WELCOME
VEHICLE_GOODBYE
DOOR_LOCK_COMPLETE
WINDOW_ANTIPINCH_STATE
OCCUPANT_HAZARD_STATE
REAR_OBSTACLE_STATE
```

VSS:

```text
입력 의미/품질 확인
Sound 선택
재생 우선순위 중재
출력
서비스 상태/Fault 제공
```

---

## 2.7 차량 사용 상태 입력

`TBD - IMPORTANT`

다음 기능은 차량 사용 상태가 필요하다.

```text
Vehicle Use Start / End
User Exit
Vehicle Power / Use Permission
```

이 입력은 다음 기능에 영향을 준다.

```text
Welcome / Goodbye
Occupant Hazard
Auto Ventilation
일부 Permission
```

하지만 현재 SysRS에서는 **실제 Producer/물리 입력 출처가 완전히 확정되지 않은 부분이 존재한다.**

따라서:

```text
VehicleUsageManager는 Architecture에 포함
BUT
실제 VehicleUsageInput Interface는 Producer 확정 후 Logical Interface에 추가
```

한다.

임의의 GPIO/CAN Signal을 현재 문서에서 만들어 넣지 않는다.

---

# 3. 전체 Interface / Software Architecture

```text
                                MOBILE
                                  │
                              Bluetooth
                                  │
                                  ▼
                                ESP32
                                  │
                           UART (CONFIRMED)
                                  │
                                  ▼
┌─────────────────────────────────────────────────────────────────────────┐
│                       S32K344 DOMAIN CONTROLLER                        │
│                                                                         │
│  ┌──────────────────── Communication / Transport ────────────────────┐  │
│  │ UartAdapter │ CanAdapter │ Network_Config │ CRC/E2E │ Rx/Tx Queue │  │
│  └───────────────────────────────┬───────────────────────────────────┘  │
│                                  │                                      │
│  ┌──────────────────────── Interface Boundary ───────────────────────┐  │
│  │          Domain_Interface.h / Domain_Interface.c                  │  │
│  └───────────────────────────────┬───────────────────────────────────┘  │
│                                  │                                      │
│  ┌──────────────────────────── Core Model ───────────────────────────┐  │
│  │ VehicleStateManager │ RequestManager │ CommandManager             │  │
│  │ ResultManager       │ SettingsManager                             │  │
│  └───────────────────────────────┬───────────────────────────────────┘  │
│                                  │                                      │
│  ┌────────────────────────── Feature / Policy ───────────────────────┐  │
│  │ PermissionManager      │ VehicleUsageManager                      │  │
│  │ DigitalKeyManager      │ ClimateManager                           │  │
│  │ InteriorLightManager   │ AutoVentilationManager                   │  │
│  │ WarningManager         │ VehicleEventManager                      │  │
│  └───────────────────────────────┬───────────────────────────────────┘  │
│                                  │                                      │
│  ┌──────────────────────── Service / Supervision ────────────────────┐  │
│  │ DomainLifecycleManager │ GatewaySessionManager                    │  │
│  │ CommunicationMonitor   │ DiagnosticManager                        │  │
│  │ MobileStateMapper                                                │  │
│  └───────────────────────────────┬───────────────────────────────────┘  │
│                                  │                                      │
│  ┌──────────────────────── Platform / Application ───────────────────┐  │
│  │ Domain_PolicyConfig │ Domain_Time │ Domain_Task │ FreeRTOS Port  │  │
│  └───────────────────────────────────────────────────────────────────┘  │
└─────────────────────────────────────────────────────────────────────────┘
          │                     │                     │
          │ Vehicle Network     │ Vehicle Network     │ Vehicle Network
          │ / configured        │ / configured        │ / configured
          │ transport           │ transport            │ transport
          ▼                     ▼                     ▼
         BCM                   CIS               WINDOW / VSS
```

### 물리 통신 표현 원칙

이 Architecture 문서는 다음을 확정하지 않는다.

```text
특정 ECU = 반드시 CAN
특정 CAN ID
특정 UART Port
Start Bit / Byte
Cycle
Timeout
CRC 위치
```

MOBILE ↔ ESP32 Bluetooth, ESP32 ↔ Domain UART는 현재 경로가 명확하다.

그 외 ECU의 실제 통신 매체와 세부 Mapping은 네트워크 담당 결과를 `Network_Config`와 Adapter에 반영한다.

---

# 4. 계층 간 의존 규칙

구조가 무너지지 않도록 다음 규칙을 적용한다.

## 4.1 Communication Layer

가능:

```text
Raw Frame Decode
Raw Frame Encode
CRC/E2E 검증
Alive/Sequence 원본 추출
Domain Interface 호출
```

금지:

```text
Digital Key 정책 판단
Auto Ventilation 판단
Warning 위험 단계 판단
Request를 DONE으로 추정
```

---

## 4.2 Domain Interface

역할:

```text
외부 Logical Input의 Domain 진입점
Domain Logical Output의 외부 송신 진입점
```

금지:

```text
CAN ID 판단
Feature 정책 구현
센서값 위험 단계 판단
```

---

## 4.3 Core Manager

Core는 차량 기능과 무관하게 여러 Feature가 공유하는 상태·식별·결과 기반을 관리한다.

```text
VehicleStateManager
RequestManager
CommandManager
ResultManager
SettingsManager
```

---

## 4.4 Feature Manager

Feature Manager는 특정 차량 기능의 **정책**을 담당한다.

```text
DigitalKey
Climate
Interior Light
Auto Ventilation
Warning
Vehicle Event
Vehicle Usage
```

Feature Manager는 Raw CAN/UART를 직접 읽지 않는다.

---

## 4.5 Service / Supervision

차량 기능 그 자체가 아니라 Domain 전체 운영을 지원한다.

```text
Lifecycle
Session
Communication Liveness
Diagnostics
MOBILE View Mapping
```

---

# 5. 최종 권장 디렉터리 구조

```text
Domain/
│
├─ common/
│  ├─ Vehicle_Types.h                    ✅ v0.2
│  ├─ Domain_PolicyConfig.h              ✅ v0.1
│  ├─ Domain_PolicyConfig.c              ✅ v0.1
│  ├─ Domain_Time.h                      ✅ v0.1
│  └─ Domain_Time.c                      ✅ v0.1
│
├─ interface/
│  ├─ Domain_Interface.h                 ✅ v0.2
│  └─ Domain_Interface.c                 TODO
│
├─ core/
│  ├─ VehicleStateManager.h              ✅ v0.2
│  ├─ VehicleStateManager.c              ✅ v0.2
│  ├─ RequestManager.h                   ✅ v0.2
│  ├─ RequestManager.c                   ✅ v0.2
│  ├─ CommandManager.h                   TODO
│  ├─ CommandManager.c                   TODO
│  ├─ ResultManager.h                    TODO
│  ├─ ResultManager.c                    TODO
│  ├─ SettingsManager.h                  TODO
│  └─ SettingsManager.c                  TODO
│
├─ feature/
│  ├─ PermissionManager.h                TODO
│  ├─ PermissionManager.c                TODO
│  ├─ VehicleUsageManager.h              TODO / INPUT-TBD
│  ├─ VehicleUsageManager.c              TODO / INPUT-TBD
│  ├─ DigitalKeyManager.h                TODO
│  ├─ DigitalKeyManager.c                TODO
│  ├─ ClimateManager.h                   TODO
│  ├─ ClimateManager.c                   TODO
│  ├─ InteriorLightManager.h             TODO
│  ├─ InteriorLightManager.c             TODO
│  ├─ AutoVentilationManager.h           TODO / 일부 입력 TBD
│  ├─ AutoVentilationManager.c           TODO / 일부 입력 TBD
│  ├─ WarningManager.h                   TODO
│  ├─ WarningManager.c                   TODO
│  ├─ VehicleEventManager.h              TODO
│  └─ VehicleEventManager.c              TODO
│
├─ service/
│  ├─ DomainLifecycleManager.h           ✅ v0.1
│  ├─ DomainLifecycleManager.c           ✅ v0.1
│  ├─ GatewaySessionManager.h            ✅ v0.1
│  ├─ GatewaySessionManager.c            ✅ v0.1
│  ├─ CommunicationMonitor.h             NETWORK-TBD
│  ├─ CommunicationMonitor.c             NETWORK-TBD
│  ├─ DiagnosticManager.h                TODO
│  ├─ DiagnosticManager.c                TODO
│  ├─ MobileStateMapper.h                TODO
│  └─ MobileStateMapper.c                TODO
│
├─ communication/
│  ├─ CanAdapter.h                       NETWORK-TBD
│  ├─ CanAdapter.c                       NETWORK-TBD
│  ├─ UartAdapter.h                      NETWORK-TBD
│  ├─ UartAdapter.c                      NETWORK-TBD
│  └─ Network_Config.h                   NETWORK-TBD
│
├─ app/
│  ├─ Domain_Task.h                      TODO
│  └─ Domain_Task.c                      TODO
│
└─ test/
   ├─ test_vehicle_state_manager.c
   ├─ test_request_manager.c
   ├─ test_gateway_session_manager.c
   ├─ test_command_result.c
   ├─ test_permission.c
   ├─ test_digital_key.c
   ├─ test_climate.c
   ├─ test_auto_ventilation.c
   ├─ test_warning.c
   ├─ test_vehicle_event.c
   ├─ test_lifecycle_resync.c
   └─ test_e2e_scenarios.c
```

---

# 6. 현재 구현 완료 상태

| 분류 | 모듈 | 상태 |
|---|---|---|
| Common Type | `Vehicle_Types.h` | ✅ v0.2 |
| Logical Interface | `Domain_Interface.h` | ✅ v0.2 |
| State | `VehicleStateManager.h/.c` | ✅ v0.2 |
| Request | `RequestManager.h/.c` | ✅ v0.2 |
| Interface Implementation | `Domain_Interface.c` | ⬜ |
| Policy Config | `Domain_PolicyConfig` | ✅ v0.1 |
| Time Port | `Domain_Time` | ✅ v0.1 |
| Lifecycle | `DomainLifecycleManager` | ✅ v0.1 |
| Session | `GatewaySessionManager` | ✅ v0.1 |
| Settings | `SettingsManager` | ⬜ |
| Permission | `PermissionManager` | ⬜ |
| Command | `CommandManager` | ⬜ |
| Result | `ResultManager` | ⬜ |
| Vehicle Usage | `VehicleUsageManager` | ⬜ / Input TBD |
| Digital Key | `DigitalKeyManager` | ⬜ |
| Climate | `ClimateManager` | ⬜ |
| Interior Light | `InteriorLightManager` | ⬜ |
| Auto Ventilation | `AutoVentilationManager` | ⬜ / 일부 Input TBD |
| Warning | `WarningManager` | ⬜ |
| Semantic Event | `VehicleEventManager` | ⬜ |
| Diagnostics | `DiagnosticManager` | ⬜ |
| MOBILE Mapping | `MobileStateMapper` | ⬜ |
| Communication | `Can/Uart Adapter` | NETWORK-TBD |
| Liveness | `CommunicationMonitor` | NETWORK-TBD |
| RTOS Integration | `Domain_Task` | ⬜ |

---

# 7. Core Manager 상세 책임

## 7.1 VehicleStateManager

`DESIGN / 구현 완료`

질문:

```text
"Domain이 현재 알고 있는 차량 상태는 무엇이며,
그 값은 아직 신뢰 가능한가?"
```

관리:

```text
BCM Door
BCM Climate
BCM Interior Light

CIS Occupant
CIS Cabin
CIS Rear

WINDOW State
VSS State

ESP32 Connection
ESP32 Proximity

Value Quality
Interface Group Freshness
```

하지 않는 것:

```text
기능 정책
Request 허용 판단
ECU Alive 판단
Fault 원인 분석
```

---

## 7.2 RequestManager

`DESIGN / 구현 완료`

질문:

```text
"이 MOBILE Request는 이전에 본 요청인가?
현재 어떤 Lifecycle에 있는가?"
```

관리:

```text
Device Context
Session ID
Request ID
Duplicate
ID Conflict
Request Age
Lifecycle
Result Confirmation
Recent Final History
```

중요:

```text
Session 실행 Context 비활성화
≠
과거 Result History 삭제
```

---

## 7.3 CommandManager

`DESIGN`

질문:

```text
"Domain이 어느 ECU에 어떤 실행 Command를 냈는가?"
```

관리 후보:

```text
Command ID
Target ECU / Function
Command Type
Origin Type
Origin Request Context (있으면)
Origin Automatic Job (있으면)
Command Created Time
Current Command State
Last Result Link
```

Origin 예:

```text
MOBILE_REQUEST
DIGITAL_KEY
AUTO_VENTILATION
WARNING/VEHICLE_EVENT 기반 출력
```

CommandManager는 실제 정책을 결정하지 않는다.

---

## 7.4 ResultManager

`DESIGN`

질문:

```text
"실행 ECU의 Local Result를 어떤 Domain 의미로 연결할 것인가?"
```

예:

```text
BCM UNLOCK_COMPLETED
→ Domain DONE

WINDOW INTERRUPTED + LOCAL_OVERRIDE
→ CANCELLED + LOCAL_OVERRIDE

WINDOW Anti-Pinch
→ FAILED + ANTIPINCH
```

ResultManager:

```text
ECU Result
 ↓
CommandManager의 Command 확인
 ↓
원 Request/Automatic Job 연결
 ↓
RequestManager 또는 Feature Manager에 결과 전달
```

---

## 7.5 SettingsManager

`CONFIRMED 책임 / DESIGN 모듈`

중앙이 확정·관리해야 하는 사용자 설정:

```text
Target Temperature
Auto Climate ON/OFF
User Fan/Circulation Setting
Interior Light Use
Interior Light Brightness
Interior Light RGB
Digital Key Auto Unlock ON/OFF
```

다음은 반드시 구분한다.

```text
Requested Setting
Confirmed Setting
Commanded Target
Applied ECU State
Measured State
```

예:

```text
Target Temperature = 23°C       ← Settings
Fan Target = HIGH                ← Climate policy output
Fan Measured = MEDIUM            ← BCM state
Cabin Temperature = 28°C         ← CIS measurement
```

---

# 8. Service / Supervision 상세 책임

## 8.1 DomainLifecycleManager

`DESIGN - 추가`

현재 Architecture에 반드시 추가한다.

질문:

```text
"Domain 전체가 지금 기능 판단을 수행해도 되는 운영 상태인가?"
```

후보 내부 상태:

```text
STARTUP
SYNCING
READY
DEGRADED
```

주의:

> 위 상태명은 구현 Architecture 후보이며 SysRS의 공통 Enum을 새로 정의하는 요구사항이 아니다.

관리:

```text
부팅 완료 여부
초기 Interface 동기화 여부
ESP32 Session 재확인
필수 상태의 최초 유효 수신
통신 복구 후 Resynchronization
Domain 기능 활성 가능 여부
```

예:

```text
S32K344 Reset
 ↓
STARTUP
 ↓ Driver/Task Init
SYNCING
 ↓ 최신 ECU State + Session 확보
READY
```

재부팅 직후 오래된 상태나 이전 세션을 기반으로 자동 기능을 실행하지 않는다.

---

## 8.2 GatewaySessionManager

`CONFIRMED 책임 / DESIGN 모듈`

위치를 `core/`가 아니라 `service/`로 둔다.

관리:

```text
Device Registration
Current BT Connection
Current Session Identity
Session Validity
Connection/Session change
Restart/Reconnect
```

실행 순서:

```text
ESP32 Request
 ↓
GatewaySessionManager
 ↓ Valid current context?
RequestManager
```

RequestManager가 Authentication을 대신하지 않는다.

---

## 8.3 CommunicationMonitor

`CONFIRMED 책임 / NETWORK-TBD`

질문:

```text
"ECU 또는 Link가 통신상 살아 있는가?"
```

관리:

```text
Expected Period
Last Rx
Miss Count
Alive Counter
Sequence
ONLINE / OFFLINE / RECOVERING 후보 상태
```

주의:

```text
Event가 안 왔다
≠
ECU Offline
```

주기 제공 약속이 있는 Status/Heartbeat 메시지를 감시한다.

별도 Heartbeat Frame이 반드시 필요한 것은 아니다.

---

## 8.4 DiagnosticManager

`CONFIRMED 책임 / DESIGN 모듈`

입력:

```text
각 ECU Fault
CommunicationMonitor 상태
ECU Service State
Input Quality
```

출력:

```text
현재 Active Fault
Fault Category
Function Availability
복구 상태
MOBILE용 오류 분류 근거
```

Function Availability를 별도 `AvailabilityManager`로 나누지 않고 DiagnosticManager가 집계한다.

예:

```text
CIS Rear Sensor Fault
→ Rear Warning UNAVAILABLE

CIS Cabin 정상
→ Climate sensing AVAILABLE
```

---

## 8.5 MobileStateMapper

`DESIGN`

Domain 내부 모델 전체를 그대로 MOBILE로 보내지 않는다.

입력:

```text
VehicleStateManager
SettingsManager
RequestManager
WarningManager
DiagnosticManager
DigitalKeyManager
```

출력:

```text
MOBILE 표시용 Logical DTO
```

표시 의미:

```text
Confirmed Setting
Applied State
Measured State
Quality
Freshness
Availability
Fault
Request Result
Warning
Digital Key status
```

### 현재 Interface의 보완 예정점

현재 `Domain_Interface.h`의 MOBILE State 송신이 `VehicleState_t` 전체를 직접 받는 형태라면 이는 **임시 구조**로 본다.

최종적으로는:

```text
MobileStateMapper
 ↓
MOBILE용 전용 State DTO
 ↓
Domain Interface Tx
```

형태로 바꾸는 것을 권장한다.

실제 DTO Field는 MOBILE Interface Freeze 시 확정한다.

---

# 9. Feature / Policy Manager

## 9.1 PermissionManager

`CONFIRMED 책임 / DESIGN 모듈`

PermissionManager는 **공통 Gate만 담당**한다.

검사 예:

```text
Domain READY?
Session Valid?
Function Available?
필수 입력 Quality 사용 가능?
Power / Operation Permission 확인 가능?
```

여기에 기능별 상세 정책을 몰아넣지 않는다.

금지 예:

```text
30°C 이상이면 환기
FAR→NEAR이면 Unlock
Door Open이면 Lock 거부
```

위 조건은 각각 Feature Manager 또는 실행 ECU Local Safety가 담당한다.

---

## 9.2 VehicleUsageManager

`CONFIRMED 필요 의미 / INPUT-TBD`

기존 `VehicleContextManager`를 `VehicleUsageManager`로 명확화한다.

관리해야 할 차량 수준 의미:

```text
Vehicle Use Start
Vehicle Use End
User Exit
현재 Vehicle Use State
Quality / Confirmation
```

사용처:

```text
VehicleEventManager
WarningManager
AutoVentilationManager
PermissionManager 일부
```

중요:

> 실제 입력 Producer가 아직 미정인 부분이 있으므로 현재 임의 Signal을 생성하지 않는다.

Producer 확정 후 `Vehicle_Types` / `Domain_Interface` / Master Matrix에 추가한다.

---

## 9.3 DigitalKeyManager

`CONFIRMED 책임 / DESIGN 모듈`

입력:

```text
Registered
Connected
App Active
Session Valid
Proximity + Quality
Door State + Quality
Digital Key Setting
Availability
```

핵심 정책:

```text
새 연결 직후 이미 NEAR
→ Unlock 금지

Valid FAR
 ↓
FAR → NEAR
 ↓
Approach당 최대 1회
 ↓
UNLOCK Intent 생성
```

다음은 FAR가 아니다.

```text
Disconnected
STALE
INVALID
UNKNOWN
```

일반 사용자 UNLOCK과 `PROXIMITY_AUTO`의 Origin을 구분한다.

---

## 9.4 ClimateManager

`CONFIRMED 책임 / DESIGN 모듈`

입력:

```text
Confirmed Climate Settings
CIS Cabin Temperature + Quality
BCM Fan/Thermal State
BCM Protection/Availability
다른 Mode/Automatic Job 상태
```

출력:

```text
Final Fan Target
Thermal Direction
Thermal Output Target
```

중앙만 최종 차량 수준 Fan/Thermal 목표를 만든다.

BCM Local Protection은 별도다.

---

## 9.5 InteriorLightManager

`CONFIRMED 책임 / DESIGN 모듈`

입력:

```text
User NORMAL Setting
WarningManager
VehicleEventManager
BCM Light Availability
```

목적:

```text
User Light Setting
≠
현재 최종 Light Output
```

예:

```text
User Light OFF
+
Safety Warning Active
→ 안전 알림 출력 가능
```

기능 내부에서 우선순위를 관리한다.

별도 Global ArbitrationManager는 현재 추가하지 않는다.

---

## 9.6 AutoVentilationManager

`CONFIRMED 책임 / DESIGN 모듈 / 일부 INPUT-TBD`

독립적인 Job/State Machine으로 관리한다.

예시 내부 상태:

```text
IDLE
STARTING
VENTILATING
STOPPING
COMPLETED
CANCELLED
FAILED
```

상태 이름은 구현 후보다.

현재 논리 조건:

```text
온도 시작 조건
Occupant 조건
Vehicle Usage 조건
현장 사용 허용
Window 위치/품질
Window 보호/가용성
```

목표:

```text
Window 약 80% Closed 위치
```

종료:

```text
유효 온도 28°C 이하
OR
최대 5분
```

취소 후보:

```text
Manual Window
STOP
Ventilation use disable
Operation Permission 상실
Vehicle Use 재개
Occupant 확인
다른 Climate 시작
```

주의:

```text
Window Move DONE
≠
Auto Ventilation Job DONE
```

종료 후 자동 Close 하지 않는다.

일부 수치는 `[CANDIDATE]` / `[잠정]` 성격을 유지한다.

---

## 9.7 WarningManager

`CONFIRMED 책임 / DESIGN 모듈`

지속형 차량 위험을 관리한다.

```text
Rear Obstacle
Occupant Hazard
Window Anti-Pinch
Door/User Exit Warning
```

출력:

```text
Current Warning State
Severity
Occurrence
Quality
Simulated/Measured basis
```

`WARNING_ACK`는 READ/확인이지 위험 CLEAR 명령이 아니다.

---

## 9.8 VehicleEventManager

`CONFIRMED 필요 의미 / DESIGN 모듈`

단발성 Semantic Event:

```text
VEHICLE_WELCOME
VEHICLE_GOODBYE
DOOR_LOCK_COMPLETE
DOOR_UNLOCK_COMPLETE
DOOR_LOCK_ERROR
```

관리:

```text
Occurrence ID
원 발생 시점
Duplicate 판단
유효 기간
VSS 전달 상태
```

VSS가 STARTUP/Wake 중 이벤트를 받을 수 없는 경우를 고려하여:

```text
유효 기간 내 Pending/Latch
```

기능을 VehicleEventManager 내부 책임으로 둘 수 있다.

이를 위해 별도 EventQueueManager는 현재 추가하지 않는다.

---

# 10. 왜 별도 Global ArbitrationManager를 두지 않는가

재검토 결과 현재 프로젝트에는 범용 `ArbitrationManager`를 추가하지 않는다.

이유:

```text
Climate 충돌
→ ClimateManager가 해결

Interior Light User/Warning/Event 충돌
→ InteriorLightManager가 해결

Window Local vs Domain
→ WINDOW ECU Local arbitration

VSS 음향 우선순위
→ VSS ECU
```

기능마다 자원 의미와 우선순위가 다르므로 하나의 범용 Manager에 몰아넣으면 오히려 결합도가 증가한다.

추후 서로 다른 Feature들이 동일한 Domain 자원을 실제로 경쟁하는 요구가 추가될 때 재검토한다.

---

# 11. 외부 Logical Interface Ownership Matrix

## 11.1 ESP32 ↔ Domain

| 방향 | Logical Information | Domain 소비/생성 모듈 |
|---|---|---|
| ESP32 → Domain | Registration / Connection | GatewaySessionManager, VehicleStateManager |
| ESP32 → Domain | App Active | GatewaySessionManager / DigitalKeyManager |
| ESP32 → Domain | Session Context | GatewaySessionManager |
| ESP32 → Domain | Proximity NEAR/FAR/UNKNOWN | VehicleStateManager, DigitalKeyManager |
| ESP32 → Domain | Proximity Quality/Freshness | VehicleStateManager |
| ESP32 → Domain | MOBILE_REQUEST | GatewaySessionManager → RequestManager |
| ESP32 → Domain | STATE_QUERY | RequestManager / MobileStateMapper |
| ESP32 → Domain | WARNING_ACK | WarningManager |
| Domain → ESP32 | Request Result | RequestManager → MobileStateMapper |
| Domain → ESP32 | Vehicle/MOBILE State | MobileStateMapper |
| Domain → ESP32 | Warning | WarningManager → MobileStateMapper |
| Domain → ESP32 | Function Availability | DiagnosticManager → MobileStateMapper |
| Domain → ESP32 | Digital Key State/Result | DigitalKeyManager / ResultManager |

---

## 11.2 BCM ↔ Domain

| 방향 | Logical Information | Domain 소비/생성 모듈 |
|---|---|---|
| BCM → Domain | Door State | VehicleStateManager |
| BCM → Domain | Climate State | VehicleStateManager |
| BCM → Domain | Interior Light State | VehicleStateManager |
| BCM → Domain | Event / Command Result | ResultManager, VehicleEventManager |
| BCM → Domain | Fault | DiagnosticManager |
| Domain → BCM | Door Command | DigitalKey/Request Flow → CommandManager |
| Domain → BCM | Fan/Thermal Command | ClimateManager → CommandManager |
| Domain → BCM | Interior Light Command | InteriorLightManager → CommandManager |

---

## 11.3 CIS ↔ Domain

| 방향 | Logical Information | Domain 소비/생성 모듈 |
|---|---|---|
| CIS → Domain | Occupant Presence/Count + Quality | VehicleStateManager, Warning/AutoVentilation |
| CIS → Domain | Temperature + Quality | VehicleStateManager, Climate/AutoVentilation |
| CIS → Domain | Humidity + Quality | VehicleStateManager / MOBILE Mapping |
| CIS → Domain | Illuminance + Quality | VehicleStateManager / MOBILE Mapping |
| CIS → Domain | Rear Distance/Measurement State + Quality | VehicleStateManager, WarningManager |
| CIS → Domain | CIS State / Function Status | DiagnosticManager |
| CIS → Domain | Fault | DiagnosticManager |
| Domain → CIS | Vehicle Power Permission | Permission/Vehicle Usage 근거 → Interface |

실제 Transport는 `NETWORK-TBD`.

---

## 11.4 WINDOW ↔ Domain

| 방향 | Logical Information | Domain 소비/생성 모듈 |
|---|---|---|
| WINDOW → Domain | Window State / Position | VehicleStateManager |
| WINDOW → Domain | Command Result | ResultManager |
| WINDOW → Domain | Anti-Pinch Event / Protection | WarningManager, ResultManager |
| WINDOW → Domain | Fault | DiagnosticManager |
| Domain → WINDOW | OPEN/CLOSE/STOP/VENT/MOVE_TO_POSITION | AutoVentilation / 상위 정책 → CommandManager |
| Domain → WINDOW | Operation Permission | PermissionManager |
| Domain → WINDOW | Domain Alive/Freshness basis | Communication/Interface |

MOBILE 직접 Window Request는 없음.

---

## 11.5 VSS ↔ Domain

| 방향 | Logical Information | Domain 소비/생성 모듈 |
|---|---|---|
| VSS → Domain | Service State / Availability | VehicleStateManager, DiagnosticManager |
| VSS → Domain | Fault | DiagnosticManager |
| Domain → VSS | One-shot Semantic Event | VehicleEventManager |
| Domain → VSS | Stateful Warning | WarningManager |

Domain은 Sound ID를 만들지 않는다.

---

# 12. Request → Command → Result 구조

이 구조는 Domain의 가장 중요한 추적 Chain 중 하나다.

```text
MOBILE_REQUEST
      │
      ▼
GatewaySessionManager
      │ valid session
      ▼
RequestManager
      │ new request
      ▼
PermissionManager
      │ common gate pass
      ▼
Feature Manager
      │ final intent
      ▼
CommandManager
      │ Command ID
      ▼
Domain_Interface
      ▼
Adapter
      ▼
ECU
      │
      ▼
ECU Result/Event
      ▼
ResultManager
      │
      ├─ CommandManager update
      │
      └─ RequestManager final/update
               │
               ▼
        MobileStateMapper
```

### Request와 Command를 분리하는 이유

```text
1 MOBILE Request
→ 1개 이상 ECU Command가 될 수 있음

자동 기능
→ MOBILE Request 없이 Command 생성 가능
```

---

# 13. Data Freshness / ECU Liveness / Fault 분리

세 개를 같은 의미로 사용하지 않는다.

```text
VehicleStateManager
= 이 데이터가 아직 최신인가?

CommunicationMonitor
= 이 ECU/Link가 통신상 살아 있는가?

DiagnosticManager
= 현재 어떤 Fault와 기능 제한이 존재하는가?
```

예:

```text
BCM Door State 최신
BCM Climate State 오래됨
```

가능:

```text
Door Group = FRESH
Climate Group = STALE
```

따라서 ECU 전체 하나의 Freshness로 합치지 않는다.

---

# 14. Heartbeat / Liveness

별도 `HEARTBEAT` 메시지는 필수가 아니다.

다음 조건을 만족하는 기존 주기 정보가 있다면 Liveness 근거로 사용할 수 있다.

```text
주기적으로 반드시 수신됨
새 갱신인지 구분 가능
Alive Counter / Sequence 또는 수신 시점 확인 가능
Timeout 정의 가능
```

CommunicationMonitor 입력:

```text
Expected Period
Allowed Delay
Miss Count
Alive Counter
Recovery Condition
```

Network 담당자가 확정할 항목:

```text
각 ECU의 Liveness Message
Period
Timeout
Miss Count
Alive Counter
Counter rollover
CRC/E2E
Recovery N회 조건
```

---

# 15. Boot / Restart / Resynchronization

`DomainLifecycleManager`가 전체 순서를 조정한다.

## 15.1 Boot

```text
RESET
 ↓
Driver / RTD Init
 ↓
Domain Core Init
 ↓
STARTUP
 ↓
Communication active
 ↓
SYNCING
```

SYNCING에서 확인:

```text
ESP32 현재 Registration/Connection/Session
필수 ECU의 최신 State
필수 Function Availability
과거 RAM 상태를 현재 상태로 오인하지 않는지
```

조건 만족 후:

```text
READY
```

---

## 15.2 ESP32 reconnect

```text
BT/UART reconnect
 ↓
새 Current Connection 확인
 ↓
새/유효 Session 확인
 ↓
현재 차량 State 재조회/재전달
```

금지:

```text
과거 미완료 Request 자동 Replay
과거 Proximity를 새 NEAR로 사용
지연 Packet을 새 Session Request로 수용
```

---

## 15.3 ECU communication recovery

```text
OFFLINE
 ↓
Communication restored
 ↓
새 Valid State 수신
 ↓
Freshness/Recovery condition 확인
 ↓
Availability 복구
```

과거 상태를 그대로 새 정상 상태로 복구하지 않는다.

---

# 16. Vehicle Usage / Power 입력의 현재 Gap

현재 Domain Architecture에서 가장 중요한 `TBD` 중 하나다.

필요 정보:

```text
Vehicle Use State
Vehicle Use Start
Vehicle Use End
User Exit
Vehicle Power Permission
```

필요 기능:

```text
Welcome / Goodbye
Occupant Hazard
Auto Ventilation
일부 Power Permission
```

하지만 Producer/실제 경로가 최종 확정되지 않았다.

따라서 네트워크 Mapping 전에 팀에서 다음을 확정해야 한다.

```text
Producer ECU/Module
논리 State 값
Quality
Freshness
Event transition rule
Domain 전달 경로
```

확정 후:

```text
Vehicle_Types.h
Domain_Interface.h
Domain Master Interface Matrix
VehicleUsageManager
```

에 추가한다.

---

# 17. Security / Robustness Boundary

## ESP32 / Bluetooth

```text
Pairing/Bonding
Registered Device
Current Connection
```

## GatewaySessionManager

```text
Device Context
Current Session
Session Validity
Reconnect/Restart
```

## RequestManager

```text
Request ID
Duplicate
Conflict
Request Age
History
```

## PermissionManager

```text
현재 기능 실행 공통 Gate
```

## Adapter / E2E

```text
CRC
Length
Sequence
Alive Counter
Frame validation
```

주의:

```text
CRC
≠
암호학적 Authentication
```

SecOC/MAC 등은 별도 Security 요구가 확정될 때 추가한다.

---

# 18. Domain_PolicyConfig와 Network_Config 분리

## Domain_PolicyConfig

차량 기능 정책/Calibration:

```text
Rear Warning threshold
Auto Ventilation threshold
Auto Ventilation max duration
Digital Key policy values
Result wait deadline
Warning hold candidate
Event age / duplicate suppression
```

수치는 SR/SysRS의 `CONFIRMED / CANDIDATE / TBD` 상태를 보존한다.

## Network_Config

통신 상세:

```text
CAN ID
UART Message Type
Start Bit / Byte
Length / DLC
Scaling / Offset
Invalid Raw
Cycle
Timeout
Alive Counter
CRC/E2E
Bit Rate / Baud Rate
```

Feature Manager가 `Network_Config`를 직접 include하지 않는 것을 원칙으로 한다.

---

# 19. Domain_Time

각 Manager가 FreeRTOS Tick API를 직접 호출하지 않도록 작은 Port로 둔다.

예:

```c
uint32_t DomainTime_GetMs(void);
```

사용:

```text
Request Age
Freshness
Warning Hold
Auto Ventilation duration
Event age
Result deadline
```

이 모듈은 복잡한 Scheduler가 아니다.

---

# 20. Domain_Task / FreeRTOS 구조

추천:

```text
CAN/UART ISR
     ↓
Rx Queue / Buffer
     ↓
Communication Task
     ↓
Frame Validation / Adapter Decode
     ↓
Domain Input Queue
     ↓
Domain Task
     ├─ DomainLifecycleManager
     ├─ Domain_Interface Rx routing
     ├─ VehicleStateManager
     ├─ GatewaySessionManager
     ├─ Request / Command / Result
     ├─ Settings / Permission
     ├─ Feature Managers
     ├─ Diagnostic / Mobile Mapping
     └─ Logical Tx 생성
     ↓
Tx Queue
     ↓
Communication Task
     ↓
Adapter Encode
     ↓
RTD Driver
```

원칙:

```text
ISR에서 복잡한 차량 정책 실행 금지
Manager에서 RTD Driver 직접 호출 금지
```

---

# 21. 주요 End-to-End 흐름

## 21.1 MOBILE Door Unlock

```text
MOBILE
 ↓
ESP32
 ↓
UartAdapter
 ↓
Domain_Interface
 ↓
GatewaySessionManager
 ↓
RequestManager
 ↓
PermissionManager
 ↓
Door/DigitalKey 관련 Feature Logic
 ↓
CommandManager
 ↓
Domain_Interface Tx
 ↓
Vehicle Network Adapter
 ↓
BCM
 ↓
BCM Result
 ↓
ResultManager
 ↓
CommandManager
 ↓
RequestManager
 ↓
MobileStateMapper
 ↓
ESP32
 ↓
MOBILE
```

---

## 21.2 Digital Key Auto Unlock

```text
ESP32 Registration/Connection
ESP32 Proximity
         │
         ▼
VehicleStateManager
         │
GatewaySessionManager
         │
DigitalKeyManager
         │
PermissionManager
         │
CommandManager
         │
BCM UNLOCK
         │
BCM Result
         ▼
ResultManager
         │
DigitalKey Result / Origin
         ▼
MobileStateMapper
```

---

## 21.3 Climate

```text
MOBILE Target Setting
 ↓
RequestManager
 ↓
SettingsManager
 ↓
ClimateManager
 ↑
CIS Temperature
BCM Climate State
Diagnostic Availability
 ↓
Final Fan/Thermal Target
 ↓
CommandManager
 ↓
BCM
```

설정 반영 완료와 실제 목표 온도 도달을 같은 `DONE` 의미로 취급하지 않는다.

---

## 21.4 Auto Ventilation

```text
VehicleUsage
CIS Temp/Occupant
WINDOW State/Quality
Permission/Availability
      │
      ▼
AutoVentilationManager
      │
      ▼
CommandManager
      │
      ▼
WINDOW
      │
      ▼
Window Result
      │
      ▼
ResultManager
      │
      ▼
AutoVentilation Job State
```

Window 이동 결과와 환기 Job 결과를 분리한다.

---

## 21.5 Rear Warning

```text
CIS Rear Distance
+ Measurement State
+ Quality
      ↓
VehicleStateManager
      ↓
WarningManager
      ↓
CLEAR / CAUTION / EMERGENCY
      ├────────→ VSS
      └────────→ MobileStateMapper
```

CIS가 위험 단계를 결정하지 않는다.

---

## 21.6 Vehicle Welcome / Goodbye

현재 입력 Producer는 `TBD`.

```text
Vehicle Usage Input
       ↓
VehicleUsageManager
       ↓ valid transition
VehicleEventManager
       ↓
Occurrence / Age / Duplicate check
       ↓
VSS Semantic Event
```

첫 상태 수신, Wake, 단순 재접속을 새 Event로 만들지 않는 정책을 적용한다.

---

# 22. 현재 명시적으로 추가하지 않는 모듈

재검토 결과 다음은 지금 만들지 않는다.

## Global ArbitrationManager

기능별 Manager에서 우선순위를 해결한다.

## PersistenceManager / NvMManager

재부팅 후 Settings/History 영구 저장 요구가 확정되면 추가한다.

현재는 저장을 임의 요구사항으로 만들지 않는다.

## WatchdogManager

MCU Watchdog은 Platform/RTD/Task supervision 영역에서 연결한다.

## AvailabilityManager

Function Availability는 DiagnosticManager가 집계한다.

## EventQueueManager

Event pending/latch가 필요하면 우선 VehicleEventManager 내부에서 처리한다.

---

# 23. Network 담당 결과 반영 위치

네트워크 담당자 문서 수신 후:

```text
Network_Config
CanAdapter
UartAdapter
CommunicationMonitor
```

를 우선 수정/구현한다.

일반적인 Wire 변경:

```text
CAN ID
Byte/Bit
DLC
Endian
Period
Timeout
CRC
Alive Counter
UART Header
```

은 원칙적으로 다음을 바꾸지 않는다.

```text
VehicleStateManager
RequestManager
DigitalKeyManager
ClimateManager
WarningManager
```

다만 네트워크 검토 중 **논리 정보 자체의 추가/삭제**가 발견되면:

```text
Vehicle_Types
Domain_Interface
관련 Manager
```

까지 수정한다.

---

# 24. 현재 Open / TBD Register

## Interface / Producer

- `Vehicle Use State / Start / End` 실제 Producer
- `User Exit` 실제 Producer
- `Vehicle Power Permission` 실제 Producer
- `APP_ACTIVE_STATE` 실제 획득/갱신 방법
- `DeviceContextId` 표현
- `SessionId` 생성 주체/폭/수명

## Network

- 각 ECU 실제 Transport
- CAN/UART Message/Signal ID
- Bit/Byte Allocation
- Cycle
- Timeout
- Alive Counter
- CRC/E2E
- Recovery 조건

## Policy / Calibration

- Rear 위험 Threshold
- Auto Ventilation 최종 시연 수치
- Warning Hold
- VSS Event Delivery 방식
- Function Availability 최종 Enum/Code
- Result Reason Wire Code

## Scope

- EXTERIOR_LIGHT 최종 삭제 여부
- MOBILE → WINDOW 원격 제어는 현재 OUT-OF-SCOPE 유지

## Persistence

- 재부팅 후 사용자 Setting 유지 요구
- 재부팅 후 Request/Warning History 유지 요구

위 Persistence 항목이 확정되기 전에는 Flash 저장을 Architecture 필수로 만들지 않는다.

---

# 25. 권장 구현 순서 v0.2

## Phase 1 - Domain 공통 기반

현재 완료:

```text
1. Vehicle_Types.h                 ✅
2. Domain_Interface.h              ✅
3. VehicleStateManager             ✅
4. RequestManager                  ✅
```

추가 완료:

```text
5. Domain_PolicyConfig             ✅
6. Domain_Time                     ✅
7. DomainLifecycleManager          ✅
```

추가 완료:

```text
8. GatewaySessionManager             ✅
```

다음:

```text
9. SettingsManager
10. PermissionManager
11. CommandManager
12. ResultManager
13. Domain_Interface.c
```

이 순서를 추천하는 이유:

```text
Lifecycle/Session
→ 현재 요청을 처리할 수 있는가?

Settings/Permission
→ 무엇을 확정하고 실행할 수 있는가?

Command/Result
→ 실행 ECU와 결과를 어떻게 추적하는가?

Domain_Interface.c
→ 위 Core에 실제 Rx/Tx Route 연결
```

---

## Phase 2 - Feature Logic

```text
14. VehicleUsageManager
    - 단, Producer 확정 전 Skeleton까지만 가능

15. DigitalKeyManager
16. ClimateManager
17. InteriorLightManager
18. AutoVentilationManager
19. WarningManager
20. VehicleEventManager
21. MobileStateMapper
22. DiagnosticManager
```

VehicleUsage 입력이 미정이면 DigitalKey 등 독립 구현 가능한 기능을 먼저 진행할 수 있다.

---

## Phase 3 - Network Integration

네트워크 담당자 결과 이후:

```text
23. Network_Config
24. CanAdapter
25. UartAdapter
26. CommunicationMonitor
27. Domain_Task / FreeRTOS Integration
```

---

## Phase 4 - Verification

```text
28. Module Unit Test
29. Fault / Timeout Injection
30. Reconnect / Restart Test
31. Request Duplicate / Replay Test
32. End-to-End Scenario Test
33. HW Integration
```

---

# 26. 테스트 우선순위

최소 다음 Scenario를 반드시 검증한다.

```text
1. Same Request duplicate → 재실행 없음
2. Same ID different content → Conflict
3. Reconnect → old request replay 없음
4. ESP32 Proximity STALE → FAR로 오인 없음
5. FAR→NEAR → approach당 최대 1 Auto Unlock
6. BCM result → 원 MOBILE Request 결과 연결
7. WINDOW Anti-Pinch → local protection + Domain warning/result 연결
8. CIS Rear STALE → CLEAR로 오인 없음
9. ECU Timeout → state STALE + function availability 제한
10. Recovery → 새 Valid State 후 복구
11. Auto Ventilation cancel → 잘못된 자동 Close 없음
12. VSS STARTUP → 유효 Event 전달 계약 검증
```

---

# 27. 현재 진행률

현재 상태를 구현량 관점에서 보면:

```text
Logical Interface / Responsibility      약 85~90%
Domain Common Core                      약 30%
Feature Logic                           초기 단계
Network Integration                     네트워크 결과 대기
RTOS/HW Integration                     미진행
E2E Verification                        미진행
```

퍼센트는 요구 충족률이 아니라 현재 예상 구현 작업량 기준의 대략적인 진행 지표다.

---

# 28. v0.1 → v0.2 주요 수정 사항

```text
1. VehicleContextManager
   → VehicleUsageManager로 명확화
   → 후순위에서 핵심 Feature 입력 관리로 우선순위 상승

2. DomainLifecycleManager 추가
   → STARTUP / SYNCING / READY / DEGRADED 운영 구조

3. GatewaySessionManager
   core → service 이동

4. PermissionManager 책임 축소
   → 공통 실행 Gate만 담당
   → 기능별 조건은 Feature Manager가 담당

5. Domain_Config
   → Domain_PolicyConfig로 이름 변경
   → Network_Config와 역할 명확히 분리

6. ECU 물리 경로
   → 모든 ECU를 CAN으로 고정하지 않음
   → Vehicle Network / configured transport로 표현

7. MobileStateMapper 책임 강화
   → VehicleState_t Raw 전달 대신 MOBILE 전용 DTO 권장

8. Vehicle Usage / User Exit / Power 입력
   → Architecture상 필요하지만 Producer 미정임을 명확히 표시

9. VSS Startup/Wake Event
   → VehicleEventManager 내부 Pending/Latch 책임 후보 추가

10. 불필요한 모듈 추가 방지
   → Global ArbitrationManager / PersistenceManager /
      WatchdogManager / AvailabilityManager는 현재 생성하지 않음
```

---

# 29. 최종 구조 판단

현재 프로젝트 규모에서 권장하는 Domain 구조는 다음 원칙으로 정리된다.

```text
Raw 통신을 Feature Logic과 분리
State / Request / Command / Result 분리
Setting / Applied State / Measured State 분리
Freshness / Liveness / Fault 분리
Common Permission / Feature Policy 분리
Warning / One-shot Event 분리
Session / Request 식별 분리
Logical Interface / Network Mapping 분리
```

이 구조를 기준으로 이후 파일을 구현한다.

논리 Interface가 Freeze된 뒤 Network Mapping이 바뀌어도,
Domain Core와 Feature Logic의 변경을 최소화하는 것이 목표다.


---

# 30. v0.3 구현 업데이트

이번 단계에서 다음 기반 모듈을 실제 C 코드로 추가했다.

```text
Domain_PolicyConfig
- 정책값과 Network 설정 분리
- CANDIDATE / PROVISIONAL / PROJECT_SELECTED 성숙도 보존

Domain_Time
- FreeRTOS 직접 의존 제거
- monotonic ms provider 주입

DomainLifecycleManager
- STARTUP / SYNCING / READY / DEGRADED
- required sync mask는 통합 설계가 명시적으로 설정
- 재동기화 시 과거 상태를 새 상태로 재사용하지 않음
```

다음 구현 대상은 `GatewaySessionManager`이다.


---

# 31. v0.4 구현 업데이트 - GatewaySessionManager

이번 단계에서 `GatewaySessionManager`를 구현했다.

핵심 책임:

```text
ESP32 Registration
Current Bluetooth Connection
Session ID
Connection Quality
App Active
        ↓
현재 Active Session 관리
        ↓
MOBILE Request Session Validation
```

분리 원칙:

```text
GatewaySessionManager
= 현재 Device/Session을 신뢰할 수 있는가?

RequestManager
= 같은 Request ID를 이미 처리했는가?

CommunicationMonitor
= ESP32 UART Link가 살아 있는가?

PermissionManager
= 현재 차량 기능을 실행해도 되는가?
```

현재 `SESSION_ID`의 생성 주체/폭/수명 및 reconnect 시 재사용 규칙은 Interface TBD다.

따라서 v0.1 구현은:

```text
SESSION_ID == 0 을 임의 invalid 처리하지 않음
Runtime에서 retired된 Session ID의 재활성은 기본 strict 차단
```

정책을 사용한다.

이 strict 정책은 이전 실행 구간 지연 Packet과 새 Packet을 구분할 별도
`Session Generation / Boot Counter`가 아직 확정되지 않았기 때문이다.

추후 Interface가 별도 Generation 근거를 제공하면 Session key를:

```text
DeviceContext
+ SessionId
+ SessionGeneration
```

형태로 확장하는 것을 우선 검토한다.

Session invalidation 시 `RequestManager` History를 삭제하지 않는다.
상위 Orchestrator가 transition을 받아:

```text
RequestManager_DeactivateSession(old session)
DomainLifecycle_BeginResync(...)
```

를 수행한다.

다음 구현 대상은 `SettingsManager`이다.
