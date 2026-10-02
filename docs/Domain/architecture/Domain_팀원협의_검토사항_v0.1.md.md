# Domain 관점 팀원 협의·검토사항 v0.1

> **목적:** S32K344 Domain Controller의 1차 Software Architecture 및 Manager 구현 내용을 Git에 공유하고, 각 ECU/네트워크/앱 담당자와 **구현 전에 합의해야 할 정보·책임·예외 동작**을 정리한다.  
> **문서 성격:** 회의 안건 + 결정 기록용 체크리스트. **미확정 사항을 확정 사양으로 선언하지 않는다.**  
> **작성 기준:** 2026-10-02 / Domain 1차 개발 정리 시점  
> **상태:** `REVIEW_REQUIRED` — 회의 전 초안

## 0. 문서 기준 및 읽는 방법

### 0.1 함께 검토할 문서

| 문서 | 현재 참고본 | 용도 |
|---|---|---|
| 상위 요구사항 | `SR(8).md` (SR v0.46, 2026-09-18) | 기능 범위·상위 책임 |
| 시스템 요구사항 | `sysRS(7).md` (SysRS v0.47, 2026-09-22) | ECU별 조건·결과·품질·보호 |
| Domain 논리 인터페이스 | `Domain_Interface_Definition_v0.8.md` (2026-09-24) | 인터페이스 의미·기존 담당자 회신 |
| Domain SW 구조 | `Domain_Software_Architecture_v1.3.md` | 구현 모듈/호출 흐름/현 시점 Open 항목 |
| ECU 통합 인터페이스 | `Domain_Master_Interface_Matrix_v0.1.md` | ECU별 Producer / Consumer 및 정보 방향 |
| 모바일 측 논리 인터페이스 | `MOBILE_ESP32_Interface_Definition_v0.1.md` | MOBILE ↔ ESP32 요청·표시·세션 |

> **중요:** 위 문서는 서로 작성 시점과 변경 반영 범위가 다르다. 예를 들어 WINDOW VENT 목표 `70%`와 `80%`, ELS 범위 등에 차이가 있다. **본 문서는 이를 임의로 통일하지 않는다.** 회의 결과를 기록한 뒤 기준 버전/변경 책임자를 정한다.  
> **주의:** 이 작업 환경에서 확인한 원본은 위 파일이다. 팀에서 이후 수정·공유한 SR/SysRS가 별도로 있다면 **그 최신본과 재대조 후** Git의 공식 Baseline을 확정한다.

### 0.2 결정 상태 표기

- `확정`: 팀이 합의하고 담당자·근거 문서가 지정된 내용.
- `잠정`: 현재 시연에 적용할 계획이나 변경 가능성이 있는 내용.
- `TBD`: 정보나 결정이 없는 내용.
- `불일치`: 문서 간 값·책임·의미가 서로 다른 내용.
- `구현 초안`: 코드가 존재하지만 실제 ECU/통신 통합 검증은 되지 않은 내용.
- `Out of Scope`: 현재 기능 범위에서 명시적으로 제외한 내용.

**회의의 목표는 모든 TBD를 당일 닫는 것이 아니다.** Domain과 다른 팀이 병렬 개발하는 데 방해되는 `P0` 항목부터 논리 의미를 확정하고, 물리 Wire 상세는 네트워크 담당 일정에 맞춰 별도 결정한다.

---

## 1. 회의에서 우선 결정할 안건 요약

| ID | 우선순위 | 결정할 사항 | 관련 담당 | 현재 상태 | 결정되면 반영할 곳 |
|---|---|---|---|---|---|
| `P0-01` | P0 | **자동 환기 VENT 위치 70% vs 80%** | Domain · WINDOW | **불일치** | SysRS / WINDOW Interface / `Domain_PolicyConfig` / `AutoVentilationManager` |
| `P0-02` | P0 | 자동 환기 실행 시 **현재 움직임이 해당 Job 소유인지 확인하는 근거** | Domain · WINDOW · Network | TBD | WINDOW State/Result/Command Context, `AutoVentilationManager` |
| `P0-03` | P0 | **자동 환기 Enable·Vehicle Use·Window Operation Permission의 실제 Producer** | Domain · WINDOW · H/W·시스템 | TBD | `VehicleUsageManager`, `Domain_Interface`, `AutoVentilationManager` |
| `P0-04` | P0 | **끼임 경고 CLEAR에 필요한 현장 확인**: 실제 입력 vs 데모 모의 | WINDOW · Domain · VSS | **문서 간 검토 필요** | SysRS / WINDOW Interface / `WarningManager` |
| `P0-05` | P0 | WINDOW **명령·결과 식별**, Local Override, STOP 및 뒤늦은 결과 처리 | WINDOW · Domain · Network | 논리 일부 확정, Wire TBD | `CommandManager`, `ResultManager`, WINDOW Adapter |
| `P0-06` | P0 | 전 ECU의 **Quality/Freshness/Timeout/Recovery 공통 계약** | 모든 ECU · Network | 일부 TBD | `VehicleStateManager`, `CommunicationMonitor`, Adapter |
| `P0-07` | P0 | **Vehicle Use Start/End, User Exit, Power Permission**의 신뢰 가능한 입력 근거 | Domain · 관련 ECU · H/W | TBD | `VehicleUsageManager`, Warning/Event/AutoVentilation |
| `P0-08` | P0 | **EXTERIOR_LIGHT 구현 범위 및 실행 ECU 배치** | 전체 · ELS/BCM · Network | **문서 상태 불일치** | SR/SysRS / Architecture / `ExteriorLightManager` |
| `P1-01` | P1 | ESP32 Session/Device/Request ID, APP_ACTIVE, proximity 최신성 | ESP32 · MOBILE · Domain | 논리 일부 확정, 방법 TBD | Gateway/Request/DigitalKey |
| `P1-02` | P1 | BCM 결과·적용 확인 및 공조·조명 실행 조건 | BCM · Domain | 일부 TBD | Climate/InteriorLight/Result |
| `P1-03` | P1 | CIS 온도·조도 Scaling과 값별 Validity/Freshness | CIS · Domain · Network | TBD | Climate/InteriorLight/AutoVentilation |
| `P1-04` | P1 | Warning ACTIVE/CLEAR/UNCONFIRMED 및 VSS/MOBILE 표시·확인 | CIS · WINDOW · VSS · MOBILE · Domain | 정책 일부 정의, 인터페이스 TBD | Warning/VehicleEvent/Mobile Mapper |
| `P1-05` | P1 | 통신 종류/속도/프레임·CAN Signal Matrix·UART Protocol | Network · 각 ECU | TBD | `Network_Config`, `CanAdapter`, `UartAdapter` |
| `P2-01` | P2 | RTOS Task/Queue/ISR 경계, RAM·메시지 용량, Watchdog | Domain · Network | 설계 초안 | `Domain_Task`, 통합 테스트 |
| `P2-02` | P2 | 재부팅 후 설정·요청 이력·경고 이력 보존 범위 | Domain · MOBILE · ESP32 | TBD | Settings/Request/Gateway/Persistence 검토 |
| `P2-03` | P2 | 초기화·통신 복구 시 정보 재동기화와 정상 상태 복귀 | 전체 | 설계 초안 | DomainLifecycle/CommunicationMonitor/각 Manager |

> **첫 회의에서 우선 볼 항목:** `P0-01`~`P0-08`. `P1`은 논리 의미만 합의하고 세부 전송 설계는 분리해도 된다.

---

## 2. WINDOW 담당자와 검토할 사항 — 최우선

**기준:** SysRS §3.2.5, §3.2.4, §8 / `Domain_Interface_Definition_v0.8` §8 / `AutoVentilationManager` 1차 구현.

### 2.1 자동 환기와 WINDOW 목표

- [ ] **VENT 목표를 70%로 할지 80%로 할지 결정한다.**
  - SysRS v0.47 §3.2.5: **80% Closed** (약 20% 열림).
  - Domain Interface Definition v0.8 §8.1/§8.5: **70% Closed**.
  - 현재 `AutoVentilationManager`/`Domain_PolicyConfig`: **80% 가정**.
  - 결과: **하나를 선택한 다음** SR/SysRS, WINDOW Interface, Domain Policy와 테스트 기대값을 함께 수정.
- [ ] 위치 정의가 `0%=Fully Open / 100%=Fully Closed`로 동일한지 확인한다.
- [ ] 현재 위치가 목표 이하(이미 더 열림)인 경우 **추가 이동하거나 더 닫지 않는 것**에 합의한다.
- [ ] `VENT`가 명령 유형인지, `MOVE_TO_POSITION` + Origin=`AUTO_VENT`로 표현할지 결정한다.
- [ ] 현재 WINDOW 구현 채널 **1개**의 식별값·채널 번호(0/1 등)를 결정한다.
- [ ] 목표 위치 허용 오차 및 **위치 판단 완료(DONE)** 기준을 정한다.
- [ ] WINDOW가 위치를 실제로 측정할 수 있는지, 모의/추정 위치인지를 구분한다.
- [ ] 위치 센서 장애 시 목표 이동 거부·정지·보고 규칙을 확인한다.

### 2.2 명령 소유권과 우선순위

**현재 요구된 로컬 책임:** `Anti-Pinch > Local Switch > Domain Command`. 중앙에서 허용하더라도 WINDOW의 즉시 보호는 독립 동작한다.

- [ ] WINDOW가 지금 수행 중인 동작의 `Source`를 **LOCAL / DOMAIN**으로 구분할 수 있는가?
- [ ] DOMAIN 동작인 경우 **원 `Command ID/Decision Sequence`**를 상태·결과에 연관시킬 수 있는가?
- [ ] AutoVentilation이 발행한 VENT가 진행 중일 때 Local Switch 또는 새 Domain 명령이 들어오면, **구 소유권 종료 근거**를 어떻게 알릴 것인가?
- [ ] 기존 상위 명령이 대체되면 이전 결과를 `CANCELLED + LOCAL_OVERRIDE/SUPERSEDED/STOP_REQUESTED` 등으로 제공하는가?
- [ ] 뒤늦은 이전 Command Result가 최신 명령의 완료·상태를 덮어쓰지 않도록 **순서 처리 기준**을 정한다.
- [ ] Domain이 STOP을 보내도 되는 조건을 `현재 해당 Vent Job이 실제로 구동 중`인 경우로 한정하는 데 동의하는가?
- [ ] Domain이 소유권을 확인할 수 없으면 **임의로 새로운 STOP을 보내지 않되**, WINDOW 자체의 통신상실·로컬 안전 정지는 그대로 동작하도록 할 것인가?

**현재 구현 Gap:** `AutoVentilationManager`는 로컬 `ownsWindowMotion`과 `motionState`로 판단하지만, `WindowState_t`에 실행 원 `Command ID`/현재 Owner가 직접 표현되지 않는다. **이 정보를 어디서 확인할지 합의 전에는 실 ECU에서 소유권 판단이 검증된 것으로 취급하지 않는다.**

### 2.3 STOP / Anti-Pinch / Local Safety

- [ ] STOP은 **어떤 상태에서도** 수용할 수 있는가? 무효 명령/잘못된 출처까지 허용한다는 뜻은 아님.
- [ ] Operation Permission 상실 시 진행 중 상위 이동을 정지하며, **정지 요청 자체를 동작 유지 허용 부족만으로 거부하지 않는지** 확인한다.
- [ ] STOP DONE은 **모터 출력 비활성 및 이동 정지** 중 무엇을 확인한 상태인가?
- [ ] Anti-Pinch 발생 직후 WINDOW가 **중앙 응답 대기 없이** 모터를 멈추는가?
- [ ] Anti-Pinch 반전 조건(열림 방향, 범위의 10%, 시간 `500~1000ms` 후보)의 최종값 및 보호 상태 보고 형식을 확인한다.
- [ ] `DEGRADED`에서 로컬 수동 이동만 허용한다는 WINDOW 검토안과 Domain 정책이 일치하는가?
- [ ] 보호 상태와 ECU Fault를 `REJECTED`, `FAILED`, `CANCELLED`에 어떤 사유로 대응시킬지 결정한다.

### 2.4 끼임 경고 CLEAR — **문서 충돌 가능 항목**

- SysRS §3.2.4는 끼임 발생 후 **그 발생에 연결된 새로운 현장 해제 확인**, 모터 비활성·이동 정지·보호 처리 종료를 요구한다. 앱 `READ`, 반전 완료, STOP만으로 CLEAR하지 않는다.
- Domain Interface v0.8 §8.2.6은 **별도 현장 해제 센서/HW는 구현하지 않으며 데모에서는 모터 상태 등으로 모사**한다고 정리한다.

회의 질문:

- [ ] 제품 의미의 `FIELD_CLEAR_CONFIRMED`를 현재 데모 범위에서 제외하는가? 제외하면 **SysRS의 경고 CLEAR 요구를 어떻게 정정/제한**할 것인가?
- [ ] 모의 CLEAR는 실제 해제와 **별도 식별(SIMULATED)** 하여 MOBILE/VSS/테스트에 전달할 것인가?
- [ ] 끼임 `Occurrence ID`, 해당 보호 상태, 현장 확인 이벤트의 연계 정보를 어떤 ECU가 보존하는가?
- [ ] WINDOW 재부팅/이벤트 유실 후 **미해제 끼임 현재 상태를 재동기화**할 수 있는가?
- [ ] `Warning ACK/READ`와 위험 `CLEAR`는 별개라는 점을 팀 전체가 동의하는가?

**회의 결정 전 주의:** 물리 해제 확인이 없는 데모의 모의 상태를 실제 끼임 위험 해소로 표시하지 않는다.

### 2.5 WINDOW가 Domain에 제공해야 할 최소 데이터

| 정보 | 의미 | 주된 Domain 소비처 | 확인할 값 |
|---|---|---|---|
| `WINDOW_STATE` | 이동 방향·정지·ECU 상태 | VehicleState / AutoVentilation | Enum, Quality, Source Age |
| `WINDOW_POSITION` | 0~100% 현재 위치 | VehicleState / AutoVentilation | Scaling, Invalid, Update 기준 |
| `WINDOW_OWNER / RELATED_COMMAND` **(확인 필요)** | 지금 움직임의 출처·원 명령 | AutoVentilation | 별도 State 필요한지 / 기존 Result로 충분한지 |
| `WINDOW_COMMAND_RESULT` | 실행 결과 및 사유 | Result / AutoVentilation | Command ID, Source, Channel, Sequence |
| `WINDOW_ANTIPINCH` | 새 끼임 발생 | Warning / VehicleEvent | Occurrence ID, 발생 시각/중복 기준 |
| `ANTIPINCH_PROTECTION_STATUS` | 보호 진행·종료·정지 근거 | Warning / VehicleState | Active/Complete, Related Occurrence |
| `WINDOW_FAULT` | 활성·복구 고장 | Diagnostic / Availability | Category, Code, Quality |
| `LOCAL_OVERRIDE / STOP` **(표현 확인 필요)** | 상위 이동 대체 발생 | AutoVentilation | Result Reason 또는 별도 Event |

> `WINDOW_OWNER / RELATED_COMMAND`와 `LOCAL_OVERRIDE / STOP`의 **별도 Signal 신설은 아직 제안**이다. 이미 정의된 `Window State + Command Result`만으로 안전하게 식별할 수 있다면 중복 신호를 추가하지 않는다.

**WINDOW 회의 산출물:** 논리 신호 목록 1부, 소유권/STOP 시퀀스 1개, 끼임 ACTIVE→CLEAR 시퀀스 1개, 미정값 목록.

---

## 3. 네트워크 담당자와 검토할 사항

### 3.1 먼저 논리 계약, 나중에 Wire Mapping

**Domain 쪽에서 먼저 제공할 것:**

1. Producer/Consumer, State/Request/Event/Result/Fault/Data 분류.
2. 의미·유효값·Invalid/Unknown 의미.
3. 필수 신호/선택 신호.
4. 동일 요청 재전송·중복·역순 처리 방식.
5. 필수 Freshness 및 통신 상실 시 기능별 반응.
6. Request → Command → Result → MOBILE 연결에 필요한 식별자.

**네트워크 담당자가 협의/설계할 것:**

- 실제 Transport (각 ECU 간 물리 연결 구조)
- CAN Message ID, DLC, Signal Bit/Byte, Endianness, Scaling
- UART Header, Length, Type, Payload, Sequence, CRC/Checksum 등 프레임 구조
- 주기/이벤트 기반 송신, 수신 Timeout, 재전송, Alive Counter, 오류 감시
- 데이터 무결성/최신성 근거 및 메시지 우선순위
- ISR/Queue/Adapter 경계

**현재 논리 Interface가 존재한다고 해서 CAN ID·Frame이 확정된 것은 아니다.**

### 3.2 공통 표준 결정 체크리스트

- [ ] `Command ID`와 `Decision Sequence`를 Wire에서 어떻게 표현할지 결정한다. 서로 목적이 다르다.
- [ ] `DeviceContextId / SessionId / RequestId`는 어디서 생성하고 언제 바뀌는가?
- [ ] 재부팅 직후 ID 재사용·Counter Wraparound가 발생해도 오래된 결과/명령과 충돌하지 않는가?
- [ ] `ACCEPTED / IN_PROGRESS / DONE / REJECTED / CANCELLED / FAILED`를 모두 구분하는가?
- [ ] `UNKNOWN`은 **실제 실행 결과가 아니라 결과 미확인 표시**임을 공통으로 사용한다.
- [ ] 요청 수용, 전송 큐 등록, 버스 전송, 실행 ECU 수용, 적용 완료를 **서로 다른 단계**로 구분하는가?
- [ ] `Result Reason`을 최소 공통 + ECU 고유 확장으로 나눌 필요가 있는가?
- [ ] `Quality=OK/STALE/INVALID/NO_DATA`와 `Validity`의 차이를 표현할 수 있는가?
- [ ] 원본 생성 시점/Source Age와 단순 Domain 수신 시각을 구분하는가?
- [ ] 연속 갱신 State와 한 번 발생한 Event의 송신/중복 정책이 다른가?
- [ ] 통신 복구 시 최신 **현재 State 재동기화**와 과거 단발 Event 재생을 분리하는가?
- [ ] Domain Alive를 별도 신호로 할지 주기 메시지 수신 감시로 할지 결정한다.
- [ ] STOP/OFF 등 안전 정지 경로의 전송 우선순위와 Timeout을 정한다.
- [ ] S32K344/ESP32 UART 전압 레벨, 기준 GND, 포트, Baud Rate 및 Flow Control을 실제 보드에서 확인한다.

### 3.3 네트워크 담당자에게 전달할 Signal Matrix 양식

| Signal / Message | Producer | Consumer | 분류 | 유효값/단위 | 발생 조건/주기 | Source Age/Timeout | 식별·순서 | 통신 상실 시 처리 |
|---|---|---|---|---|---|---|---|---|
| `WINDOW_COMMAND` | Domain | WINDOW | Command | OPEN/CLOSE/STOP/VENT/POS | 목표 변경 | TBD | Command ID + Decision Sequence | 오래된 명령 거부; 진행 상위 이동 안전 정지 |
| `WINDOW_STATE` | WINDOW | Domain | State | Motion / ECU / Position | TBD | TBD | Update/Freshness | 신뢰 불가 표시·자동 판단 보류 |
| `WINDOW_COMMAND_RESULT` | WINDOW | Domain | Result | 6종 + Reason | 수용/변경/최종 | TBD | 원 Command ID | 미확인은 FAILED로 추정하지 않음 |
| `CIS_CABIN_TEMPERATURE` | CIS | Domain | Data | Scale TBD | TBD | TBD | Sample Age/Sequence | 자동 공조/환기 제한 |
| `PROXIMITY_STATE` | ESP32 | Domain | State | NEAR/FAR/UNKNOWN | TBD | TBD | Session + freshness | 신뢰 불가=FAR 아님 |
| `REQUEST_RESULT` | Domain | MOBILE via ESP32 | Result | 6종 + Confirmation | 상태 변경 | TBD | Device/Session/Request | UNKNOWN 표시, 자동 재실행 금지 |

> 실제 주기(ms), Timeout(ms), Message ID, DLC, Byte Allocation은 **아직 합의 전**이므로 위 표에 임의 값을 채우지 않는다. 필요한 경우 SysRS의 `[잠정]/[후보]`를 **별도 Candidate 열**로 복사해 실측 전 확정값과 섞지 않는다.

---

## 4. ESP32 / MOBILE 담당자와 검토할 사항

### 4.1 등록·연결·세션

- [ ] 등록된 Device 여부를 ESP32가 어떤 근거로 확인하고 Domain에 어떤 정보로 제공하는가?
- [ ] `DeviceContextId`를 어떤 단말에 귀속시키고, 재연결·등록 해제 시 어떻게 처리하는가?
- [ ] `SessionId`의 **생성 주체, 유효기간, 재사용 금지·초기화 조건**은 무엇인가?
- [ ] BLE 연결 상실과 ESP32↔Domain UART 링크 상실을 별도로 감지/보고할 수 있는가?
- [ ] APP_ACTIVE는 앱의 실제 활성 상태를 무엇으로 확인하고 어떤 주기로 갱신하는가?
- [ ] 재부팅 후 과거 Request/Session을 새 명령으로 받아들이지 않는 근거는 무엇인가?
- [ ] 동시에 여러 단말이 연결될 수 있는가? 현재 `VehicleStateManager`/`DigitalKeyManager` 초안은 **단일 현재 Proximity Context 중심**이라 복수 단말 지원 시 구조 변경이 필요하다.

### 4.2 Digital Key

- [ ] RSSI 기반 `NEAR/FAR/UNKNOWN` 판단 주체는 ESP32임을 확인한다.
- [ ] RSSI 임계값·히스테리시스·갱신 주기·STALE 기준을 정한다.
- [ ] 연결 상실·UNKNOWN·STALE을 `FAR`로 재해석하지 않는다.
- [ ] 신규 세션 Digital Key Setting은 OFF로 시작하는가?
- [ ] **유효 FAR를 먼저 확인하고 새 NEAR가 될 때 한 번만 자동 해제**한다는 정책에 동의하는가?
- [ ] Door가 이미 UNLOCK일 때의 `ALREADY_AT_TARGET + DONE`을 실제 근접 자동 해제 발생으로 표시하지 않는가?
- [ ] 앱에서 Digital Key 설정 상태와 실행 결과, 사용 가능 여부를 서로 구분하여 표시하는가?
- [ ] 실제 무선 인증/등록 보안의 책임 범위를 명확히 하고, 단순 RSSI만으로 인증을 대체하지 않는다.

### 4.3 MOBILE 요청·상태·경고

- [ ] 현재 MOBILE Request 지원 범위(도어/공조/실내조명/디지털키 설정)를 공유한다.
- [ ] MOBILE → WINDOW 원격 이동 요청은 현행 `Out of Scope`임을 재확인한다.
- [ ] 동일 `Request ID + 동일 Payload` 재전송 시 실행을 재시작하지 않고 기존 결과를 응답하는가?
- [ ] 동일 ID에 다른 Payload가 오면 충돌로 거부하는가?
- [ ] `ACCEPTED`, 차량 `DONE`, 설정만 `CONFIRMED`, 결과 미확인 `UNKNOWN`을 UI에서 구분하는가?
- [ ] WARNING `READ/ACK`는 **읽음 처리**일 뿐 위험 `CLEAR`가 아니라는 점에 동의하는가?
- [ ] 앱에서 사용할 Vehicle State/Warning/Availability 필수 표시 목록을 확정한다.
- [ ] 연결 끊김 후 오래된 상태를 현재 유효 State처럼 계속 표시하지 않는가?

---

## 5. BCM 담당자와 검토할 사항

### 5.1 Door

- [ ] `LOCK/UNLOCK`의 수용·거부·완료 조건을 합의한다.
- [ ] `DOOR_OPEN`, `INCONSISTENT`, `UNTRUSTED` 등에서 LOCK 허용 여부와 사유를 명확히 한다.
- [ ] `LOCK_COMPLETED/UNLOCK_COMPLETED`는 현재 목표 도달과 실제 전이를 어떻게 구분하는가?
- [ ] `ALREADY_AT_TARGET`는 새 액추에이터 동작 발생과 구분되는가?
- [ ] 도어 구동 실패와 명령 실행 전 거부를 별도로 보고하는가?
- [ ] 새/오래된 Command ID 및 Decision Sequence를 어떻게 판단하는가?

### 5.2 Climate/Fan/Thermal

- [ ] Domain이 Fan/Heat/Cool 최종 목표를 결정하고 BCM이 실행·보호를 담당하는 경계를 재확인한다.
- [ ] Fan `commandedLevel`과 `measuredLevel`은 실제 다른 정보인가? 측정 근거는 무엇인가?
- [ ] Thermal 출력 이전 **필요 Fan 정상 측정 및 방열 조건**을 확인할 수 있는가?
- [ ] `Thermal Direction`, 출력 `0~100%`의 실제 물리적 의미·스케일을 확정한다.
- [ ] 자동 공조 임계값(온도차·히스테리시스)과 최대 출력은 시연 후보이며 실측 후 보정하는가?
- [ ] Manual Fan 명령은 Thermal OFF이며 AUTO 모드를 대체한다는 정책에 동의하는가?
- [ ] 통신 상실·Fan 이상·과열·센서 오류 시 **BCM 로컬 보호**가 단독으로 동작하는가?
- [ ] `ClimateState`의 적용/측정 필드가 Command DONE의 충분한 근거인지 정의한다.

### 5.3 Interior Light

- [ ] `FAULT > WARNING > GOODBYE > NORMAL` 우선순위를 동일하게 이해하는가?
- [ ] User OFF는 NORMAL에만 적용되고 WARNING/FAULT 출력은 허용된다는 데 동의하는가?
- [ ] 색상 비율, 전체 밝기(0~100%)와 LED PWM 듀티를 구분하는가?
- [ ] `commandApplied`가 **논리 명령 적용**인지 **실제 물리 점등 확인**인지 분리 보고하는가?
- [ ] GOODBYE 유지 시간·점멸 패턴은 아직 미확정으로 두는가?
- [ ] 더 높은 우선순위 조명 목표가 오면 기존 Pending Command를 어떤 결과로 대체하는가?

---

## 6. CIS 담당자와 검토할 사항

- [ ] Occupant Presence / Count, Cabin Temperature / Humidity / Illuminance, Rear Distance 각각의 Producer·갱신 주기를 확정한다.
- [ ] 각 측정값의 `Validity`, `Quality`, 원본 시각 또는 Age, `Update Sequence`를 **독립적으로 제공**할 수 있는가?
- [ ] `cabinTemperature`의 단위/Scaling을 확정한다. (예: raw 230이 23.0°C인지 여부 **현재 미확정**)
- [ ] `cabinIlluminance`의 단위/Scaling을 확정한다. (`lux` 직접값인지 현재 미확정)
- [ ] Occupant `false`와 `UNKNOWN/INVALID/NO_DATA`를 명확히 구분하는가?
- [ ] 후방 거리 센서의 유효 범위/미측정/장애를 구분하고, 차량 수준 CLEAR/CAUTION/EMERGENCY 판정은 Domain에서 하는가?
- [ ] Sensor Fault와 통신 Timeout을 서로 다른 Fault/Quality 근거로 제공하는가?
- [ ] 신뢰 불가 항목 하나로 무관한 기능까지 일괄 중지시키지 않도록 **값별 Quality**를 지원하는가?
- [ ] 재부팅/회복 후 이전 캐시를 현재 유효 관측처럼 보고하지 않는가?

**Domain 영향:** 미확정 온도/조도 Scaling으로 인해 `ClimateManager`, `InteriorLightManager`, `AutoVentilationManager`의 자동 판단은 현재 Config TBD를 유지한다.

---

## 7. VSS / Warning / Vehicle Event 담당자와 검토할 사항

### 7.1 후방 위험

- [ ] CIS의 **거리·Quality/경과 시간**을 받아 위험 단계 선택은 Domain에서 수행하는가?
- [ ] `CLEAR / CAUTION / EMERGENCY` 경계값은 어떤 값이며 후보·실측 상태가 무엇인가?
- [ ] invalid/stale 입력을 유효 CLEAR로 취급하지 않는가?
- [ ] VSS는 의미 기반 경고를 받아 자체 우선순위·음원 선택·재생을 하는가?

### 7.2 잔류 탑승자 / 끼임 / 열린 도어

- [ ] 잔류 탑승자 경고의 필수 입력은 **Vehicle Use End + User Exit + CIS Occupant**인가? 실제 Producer가 누구인지 지정한다.
- [ ] 끼임 `ACTIVE`와 `CLEAR`의 근거는 2.4절 검토 결과와 일치하는가?
- [ ] 사용자가 앱에서 Warning을 읽어도 ACTIVE/CLEAR 상태는 바뀌지 않는가?
- [ ] 경고 입력 유실 시 `UNCONFIRMED`와 유효 `CLEAR`를 구분하는가?
- [ ] 기동/재연결 시 과거 단발 Event를 새 Event로 재생하지 않는가?

### 7.3 One-shot Event 및 표시

- [ ] `WELCOME/GOODBYE/DOOR_LOCK/UNLOCK`의 유효한 **새 발생**을 누가 확정하는가?
- [ ] Event ID, 중복 검출, 발생 시각/경과 시간, 재전송 시 동일 식별을 정한다.
- [ ] VSS에 Sound 파일 이름이 아니라 **Semantic Event/Warning**을 전달한다는 책임 경계를 유지한다.
- [ ] `WarningManager`, `VehicleEventManager`, `MobileStateMapper`를 실제 Domain Task의 어디에 연결할지 합의한다.

---

## 8. EXTERIOR_LIGHT(ELS) 및 차량 사용 상태 — 범위 결정

### 8.1 ELS 문서 불일치

- SysRS v0.47와 SR v0.46에는 `EXTERIOR_LIGHT` 상세 범위가 존재한다.
- `Domain_Interface_Definition_v0.8`에는 `OUT_OF_SCOPE_CANDIDATE`라고 남아 있다.
- `Domain_Software_Architecture_v1.3`에서는 최신 SysRS 범위로 해석하여 `ExteriorLightManager`를 TODO에 다시 포함했다.

**결정 질문:**

- [ ] 이번 시연에서 ELS를 **포함/제외/후속으로 보류** 중 무엇으로 결정하는가?
- [ ] 포함 시 어떤 ECU/보드가 실행 책임자인가?
- [ ] `SET_ON/SET_OFF`, 자동 조도 기반 목표, 차량 사용 이벤트 기반 임시 점등 중 어디까지 구현하는가?
- [ ] 임시 10초, 정상 출력 상태, 실제 피드백 미지원 등 후보 기준을 유지하는가?
- [ ] 제외한다면 SR/SysRS/Interface/Architecture에서 동일하게 제외 상태를 표시할 것인가?

### 8.2 VehicleUsageManager 입력 경로

- [ ] `Vehicle Use Start/End`, `User Exit`, `Power Permission`을 **누가 관측하고 어떤 Quality와 함께 제공**하는가?
- [ ] 모의 스위치/데모 입력이라면 실제 차량 상태와 구분할 수 있는가?
- [ ] Boot 때 이전 상태를 사용 시작/종료 **새 Event로 잘못 변환하지 않는가?**
- [ ] 사용 상태 정보가 없을 때 AutoVentilation/Occupant Hazard/Welcome/Goodbye를 기능별로 어떻게 제한하는가?
- [ ] 실제 Producer 확정 전엔 `VehicleUsageManager`를 Skeleton/Adapter 경계로 유지할 것인가?

---

## 9. Domain 내부에서 추가 검토할 기술 항목

이 절은 **팀 협의 안건**이면서 동시에 Domain 자체 코드 리뷰 목록이다. 이미 구현됐다는 사실이 실제 ECU 검증이나 모든 예외 동작의 정확성을 보장하지 않는다.

### 9.1 Interface / Core 간 정합성

- [ ] `Domain_Interface.h` 최신 정식본을 하나로 고정한다. (`v0.5` 변경분 포함)
- [ ] `CommandManager` 최신 정식본을 하나로 고정한다. (`v0.4` 변경분 포함)
- [ ] `RequestManager v0.3`, `ResultManager v0.2` 등 실제 구현과 Architecture의 **버전 표기가 일부 옛 버전으로 남아 있는지** 재점검한다.
- [ ] `Domain_Interface.c`의 `Rx Observer`를 Feature/Diagnostic/Query 소비자와 연결할 실제 시점을 정한다.
- [ ] `Domain_Interface`가 `Tx Port OK`를 받는 시점을 **Queue 접수**로 해석하는지 **실제 버스 송신 완료**로 해석하는지 명확히 한다.
- [ ] Command 완료가 State 관측 기반인 기능과 전용 Command Result 기반인 기능의 기준이 충돌하지 않는가?
- [ ] Pending Request/Command가 세션 변경, ECU 재시작, 재동기화, 늦은 결과 수신으로 중복 확정되지 않는가?
- [ ] 정적 Record Capacity 초과 시 실행 중 항목·세션의 중복 방지 근거가 사라지지 않는가?
- [ ] 여러 Task에서 State/Command/Result를 동시에 갱신하지 않도록 소유 Task/Lock을 정의한다.

### 9.2 보안·안전·입력 품질

- [ ] 등록/연결/APP_ACTIVE와 **실제 인증 보장**을 구분한다.
- [ ] 관련 ECU가 `DEGRADED/FAULT`일 때 기능별 Allow/Deny 조건을 확정한다.
- [ ] 신뢰 불가 신호를 자동으로 `false`, `FAR`, `CLEAR`, `CLOSED`, `OFF` 등으로 치환하지 않는다.
- [ ] `STOP/OFF`와 `새 동작 START`의 Gate를 동일하게 적용하지 않는다.
- [ ] Domain의 정책 취소 기록(`CancelByPolicy`)이 **물리적인 실행 ECU 정지 성공**을 뜻하지 않음을 검증한다.
- [ ] 자동 환기 STOP 발행 시 **현재 WINDOW 실제 Owner**가 이전 자동 환기인지 확인할 수 있다.
- [ ] ISO 26262/AUTOSAR 등 **정식 표준 준수·양산 안전 검증**을 이 Demo의 컴파일/모의 시험 성과와 혼동하지 않는다.

### 9.3 정책·설정 미정 사항

- [ ] 공조 목표 온도 범위, 단위/Scaling, Thermal 최대 출력 후보를 최종화한다.
- [ ] 실내 조도 단위와 NORMAL 자동 밝기/히스테리시스 후보를 실측한다.
- [ ] GOODBYE 유지 시간·조명 패턴 결정 주체를 정한다.
- [ ] Warning Hold/Recovery·Rear 위험 임계값의 실제 구현값을 정한다.
- [ ] 자동 환기 시작 30°C / 종료 28°C / 최대 5분의 시연 후보임을 표시한다.
- [ ] 재부팅 후 User Setting/Request/Warning History의 저장 범위를 결정한다.

---

## 10. 통합 검증 시나리오 — 회의에서 테스트 담당 지정

| TEST ID | 입력/상황 | 기대 결과 | 함께 확인할 담당 |
|---|---|---|---|
| `IT-01` | 유효 WINDOW VENT 목표 실행 | 목표 위치까지 실행, MOVE DONE과 Vent Job RUNNING 분리 | Domain/WINDOW |
| `IT-02` | 현재 WINDOW 위치가 목표보다 이미 더 열림 | 추가 닫기 명령 없음 | Domain/WINDOW |
| `IT-03` | VENT 이동 중 Local Switch 개입 | 이전 VENT 대체/취소 추적, Domain의 불필요한 STOP 없음 | Domain/WINDOW |
| `IT-04` | VENT 이동 중 28°C 도달 또는 5분 | Job 종료; 아직 해당 Job 소유 이동 중일 때만 STOP | Domain/WINDOW/CIS |
| `IT-05` | VENT 이동 완료 후 종료 | 창문 자동 닫힘 없음; 불필요한 STOP 없음 | Domain/WINDOW |
| `IT-06` | 환기 종료 직후 다시 30°C 이상 | OFF→새 ON 없이는 재시작 없음 | Domain/입력 담당 |
| `IT-07` | WINDOW 통신 상실·복구 | 상위 이동 로컬 안전 정지, 오래된 명령 자동 재개 없음 | WINDOW/Network |
| `IT-08` | 끼임 발생 → 반전 완료만 보고 | Warning을 임의 CLEAR하지 않음 | WINDOW/Domain/VSS |
| `IT-09` | 끼임 발생 + 새 현장 확인/모의 처리 | 실제 확인과 SIMULATED 표시를 구분 | WINDOW/Domain/MOBILE |
| `IT-10` | 동일 Request 반복 전달 | Command 재실행 없음, 기존 결과 연계 | MOBILE/ESP32/Domain |
| `IT-11` | Disconnect/STALE proximity 뒤 NEAR | 기존 FAR를 재사용한 자동 Unlock 없음 | ESP32/Domain/BCM |
| `IT-12` | BCM ALREADY_AT_TARGET + DONE | Digital Key 실제 Unlock 전이 성공으로 표시하지 않음 | BCM/Domain/MOBILE |
| `IT-13` | Fan measured 미도달 상태에서 AUTO | Thermal 구동 지시 보류 | BCM/Domain |
| `IT-14` | Interior Light User OFF + WARNING | WARNING 표시 유지 | BCM/Domain |
| `IT-15` | 오래된 ECU Result/상태 도착 | 최신 Command/Request 및 State로 회귀하지 않음 | 모든 ECU/Network |
| `IT-16` | ECU 수신 결과를 확인하지 못함 | UNKNOWN/UNCONFIRMED, 임의 FAILED 생성 금지 | Domain/앱 |
| `IT-17` | 복수 ECU 중 CIS 조도만 STALE | 무관한 수동 Fan/도어 기능까지 차단하지 않음 | Domain/BCM/CIS |
| `IT-18` | Domain/ECU 재부팅 | 이전 세션·일회성 이벤트·자동 Job 무단 재개 방지 | 전체 |

> TEST별 실제 입력 주입 방법, 수용 기준(시간·오차), 결과 캡처·로그 담당은 회의 후 채운다. 여기의 시나리오 기재는 **시험 완료**를 뜻하지 않는다.

---

## 11. 회의 결정 기록 양식

회의마다 이 표를 복사해 `docs/domain/decisions/` 등으로 보관한다.

| 결정 ID | 회의 일자 | 안건 | 결론 | 적용 문서/코드 | 담당자 | 검증 방법 | 상태 |
|---|---|---|---|---|---|---|---|
| `DEC-001` | YYYY-MM-DD | VENT 목표 70/80 | 미정 | SysRS, WINDOW Interface, Policy | 미정 | WINDOW 목표 위치 시험 | OPEN |
| `DEC-002` | YYYY-MM-DD | WINDOW 원 Command/Owner 추적 | 미정 | WINDOW State/Result, AutoVentilation | 미정 | Local Override/STOP 시험 | OPEN |
| `DEC-003` | YYYY-MM-DD | 끼임 CLEAR 실물/모의 | 미정 | SysRS, Warning Interface | 미정 | Anti-Pinch CLEAR 시험 | OPEN |
| `DEC-004` | YYYY-MM-DD | Vehicle Use/Enable Producer | 미정 | VehicleUsage/AutoVentilation | 미정 | 사용 종료/재개 시험 | OPEN |
| `DEC-005` | YYYY-MM-DD | ELS 범위 및 ECU 배치 | 미정 | SR/SysRS/Architecture | 미정 | 시연 Scope Review | OPEN |

### 11.1 Issue 작성 시 포함할 내용

```markdown
## 배경 / 기준 요구
- 문서 / 절:
- 현재 구현:

## 불일치 또는 미정 사항
-

## 선택지
- A:
- B:

## 팀 결정
- [ ] 확정
- [ ] 추가 검토

## 영향 범위
- SR/SysRS:
- Interface:
- Domain 코드:
- 타 ECU 코드:
- 네트워크:
- 테스트:

## 담당 / 완료 조건
- 담당:
- 확인할 증거:
```

**권장:** 결정이 필요한 항목은 GitHub Issue로 분리하고, 코드·문서 수정 PR은 해당 Issue와 연결한다.

---

## 12. Git 업로드 및 문서 관리 제안

### 12.1 권장 위치

```text
docs/
└─ domain/
   ├─ Domain_Software_Architecture_v1.3.md
   ├─ Domain_Interface_Definition_v0.8.md
   ├─ Domain_Master_Interface_Matrix_v0.1.md
   ├─ Domain_팀원협의_검토사항_v0.1.md
   └─ decisions/
      └─ README.md  # 회의 결정 기록 목록 (추후)
```

> 위 디렉터리는 **권장 배치 예시**다. 현재 Repository의 실제 구조에 맞춰 조정한다.

### 12.2 업로드 원칙

- [ ] **Architecture/Interface의 1차 설계 상태**와 **실제 통합 검증 완료**를 구분해 표시한다.
- [ ] 같은 모듈의 `v0.1 ~ v0.4` 소스 파일을 모두 현재 사용 소스로 혼용하지 않는다. **최신 정식 헤더/소스 한 쌍**을 지정한다.
- [ ] 이전 버전은 필요하면 Git History 또는 별도 Archive/Release에 보존한다.
- [ ] 모듈별 `.h/.c`가 컴파일된 조합을 명시하고, 최신 인터페이스 버전과 일치시킨다.
- [ ] 문서의 `CONFIRMED`는 **논리 의미 합의**이지 H/W 실측 검증 완료가 아님을 구분한다.
- [ ] P0 결정 반영은 **Issue → 회의 결정 → 요구사항 변경 → Interface 변경 → 관련 Code/Tests 변경 → PR** 순서로 추적한다.
- [ ] CAN/UART Message ID·bit allocation 등 네트워크 결정 전에는 임의의 값을 `CONFIRMED`로 표기하지 않는다.
- [ ] WINDOW 및 ELS 관련 문서 불일치는 **회의 결정 없이 코드·문서에서 묵시적으로 해결하지 않는다.**

---

## 13. 다음 회의 권장 진행 순서

1. **기준 문서 버전 5분 확인**: 실제 최신 SR/SysRS/Interface와 v1.3 Architecture 차이 확인.
2. **WINDOW 최우선 안건**: VENT 70/80, 소유권·STOP, Anti-Pinch CLEAR.
3. **Producer/Gateway 책임**: Vehicle Use/Enable/Power, ESP32 Session/APP_ACTIVE.
4. **Network 공통 계약**: Command/Result ID, Quality/Freshness, STOP/Alive, Signal Matrix 입력.
5. **범위 확정**: ELS 포함 여부, 기타 Feature 구현 우선순위.
6. **결정 기록**: 결정 사항·담당·후속 Issue·검증 조건 지정.

### 회의 종료 시 확인할 산출물

- [ ] 기준으로 사용할 SR/SysRS/Interface **문서 버전**
- [ ] `P0`별 **결정 / 보류 / 담당 / 다음 확인 방법**
- [ ] WINDOW 논리 Interface 변경 목록
- [ ] Network 담당자에게 보낼 Signal Matrix 입력 자료
- [ ] Domain 코드 변경이 필요한 파일 목록
- [ ] 검증 시나리오와 테스트 담당자

---

## 14. Domain 담당자 메모

현재 Domain에는 다음과 같은 1차 구현이 있다.

```text
VehicleStateManager / RequestManager / SettingsManager
PermissionManager / CommandManager / ResultManager
DomainLifecycleManager / GatewaySessionManager
Domain_Interface Core Routing
DigitalKeyManager / ClimateManager
InteriorLightManager / AutoVentilationManager
```

반면 다음은 **미구현 또는 통합 미완료**다.

```text
WarningManager
VehicleEventManager
ExteriorLightManager
VehicleUsageManager (실제 Producer TBD)
DiagnosticManager / MobileStateMapper
CommunicationMonitor
CAN/UART Adapter 및 Network Config
Domain_Task / FreeRTOS 통합
실제 ECU End-to-End 검증
```

**현재의 목표는 새 Manager를 무조건 계속 만드는 것이 아니라, 팀 간 논리 계약을 먼저 정리하고 변경 영향이 큰 부분부터 확정하는 것이다.** 이후 네트워크 설계 결과를 반영할 때는 원칙적으로 `Network_Config/Adapter`부터 수정하고, 논리 정보가 달라진 부분에 한해 `Vehicle_Types/Domain_Interface/Manager`를 변경한다.
