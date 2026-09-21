# WINDOW Interface 정의 초안

> 기준 문서: `SR(4).md` §9 WINDOW 기능 요구 / `sysRS(4).md` §8 WINDOW 상세 요구사항 / `ECU_Interface_정보요청_가이드(2).md`  
> 목적: 중앙 Domain Controller와 Window ECU 사이의 논리 Interface를 정의하기 위해 WINDOW 영역의 State / Request / Event / Fault / Data / Local Logic / Fail-safe 정보를 정리한다.  
> 주의: 본 문서는 **논리 Interface 초안**이다. CAN ID, Signal ID, byte/bit layout, endianness, 실제 C 자료형, scaling, invalid raw value, 실제 네트워크 송신 주기 및 E2E 방식은 후속 인터페이스/네트워크 설계에서 확정한다.

---

## 1. 주요 기능

Window ECU는 단일 창문 채널을 기준으로 다음 기능을 담당한다.

- 로컬 창문 스위치 입력 처리
- 중앙 Domain Controller의 확정된 창문 명령 수신 및 검증
- 창문 열림 / 닫힘 / 정지
- 목표 위치 이동
- 수동 유지 동작
- 원터치 동작
- 창문 위치 및 완전 열림 / 완전 닫힘 상태 관리
- 끼임(Anti-Pinch) 감지 시 로컬 즉시 정지
- 조건이 유효한 경우 안전 방향 제한 반전
- 모터 방향 상호 배타 제어
- 명령 처리 결과 제공
- State / Position / Event / Fault 제공
- 통신 상실 및 내부 고장 시 안전 상태 전환

### 1.1 책임 경계

**중앙 Domain Controller 담당**

- 차량 수준의 요청 권한 판단
- 상위 요청 간 차량 수준 우선순위 판단
- 자동 환기 조건 판단
- 자동 환기 목표 위치 결정
- WINDOW가 제공한 상태/결과를 이용한 차량 상태 갱신
- WINDOW 끼임 정보를 이용한 차량 수준 경고 판단

**Window ECU 담당**

- 전달받은 확정 명령의 실행
- 로컬 스위치 처리
- 모터 구동
- 끝단 처리
- 위치 관리
- 끼임 즉시 보호
- 실제 상태 및 결과 제공
- 내부 Fault 감지와 안전 상태 전환

> 중앙 Domain Controller가 명령을 확정했다는 사실이 Window ECU의 로컬 보호를 해제하지 않는다.

---

## 2. Interface 방향 요약

### 2.1 Domain → Window ECU

| Interface | 의미 | 비고 |
|---|---|---|
| `WINDOW_COMMAND` | 중앙이 확정한 창문 동작 요청 | `OPEN`, `CLOSE`, `STOP`, `VENT`, `MOVE_TO_POSITION` |
| 상위 목표 유지 허용 | 현재 상위 이동을 계속 수행해도 되는지에 대한 근거 | `[잠정]`, §2.6 공통 계약 적용 |
| `POWER_OPERATION_STATE` | 창문 이동이 허용되는 전원/운전 상태 | 값과 유효성 필요 |

### 2.2 Window ECU → Domain

| Interface | 의미 |
|---|---|
| `WINDOW_STATE` | 실제 창문 이동 상태와 ECU 상태 |
| `WINDOW_POSITION` | 창문 위치와 위치 유효성 |
| `WINDOW_ANTIPINCH` | 끼임 발생 Event |
| 끼임 보호 처리 상태 | 반전 진행 / 완료 / 중단 및 사유 |
| 끼임 확인 상태 | 미해제 끼임 발생 및 현장 해제 확인 상태 `[잠정]` |
| `WINDOW_COMMAND_RESULT` | 상위 요청 또는 로컬 조작의 처리 결과 |
| `WINDOW_FAULT` | Window ECU Fault 정보 |

### 2.3 Window ECU 내부/현장 입력

| Interface | 의미 |
|---|---|
| `LOCAL_WINDOW_SWITCH` | 로컬 수동 유지 / 원터치 조작 |
| `WINDOW_POSITION_FEEDBACK` | 위치 및 끝단 정보 |
| `WINDOW_ANTIPINCH_INPUT` | 끼임 판단 입력 |
| 현장 해제 확인 | 끼임 원인 제거 확인 입력 `[잠정]` |

---

## 3. State

Window 영역에서는 **ECU 상태**와 **창문 실제 이동 상태**를 분리한다.

### 3.1 ECU State

| State | 의미 | 모터 출력 원칙 |
|---|---|---|
| `INIT` | 초기화 및 입출력 안전 확인 중 | OFF |
| `READY` | 정상 명령 수용 가능 | 명령에 따라 제어 |
| `DEGRADED` | 일부 센서 또는 통신 기능 제한 | 정의된 제한 동작만 수행 |
| `FAULT` | 안전한 이동을 보장할 수 없음 | OFF |

> `DEGRADED` 상태에서 허용할 제한 수동 동작의 조건과 방향은 현재 미정이다. 정해지지 않은 제한 동작을 임의로 허용하지 않는다.

### 3.2 Window Motion State

| State | 의미 |
|---|---|
| `STOPPED` | 모터 출력이 해제된 정지 상태 |
| `OPENING` | 열림 방향 이동 중 |
| `CLOSING` | 닫힘 방향 이동 중 |
| `ANTIPINCH_REVERSING` | 끼임 대응 안전 반전 중 |
| `UNKNOWN` | 신뢰할 수 있는 실제 이동 상태를 확정할 수 없음 |

### 3.3 End Position State

위치값과 별도로 다음 끝단 상태를 구분할 수 있어야 한다.

- `FULLY_OPEN`
- `FULLY_CLOSED`
- Neither / intermediate position
- Validity와 함께 제공

> 완전 열림/닫힘 상태는 단순 위치값만으로 대체하지 않고 구분 가능한 상태 데이터로 제공한다.

---

## 4. Request / Command

Window ECU가 중앙 Domain Controller로부터 해석하는 요청 종류는 다음과 같다.

| Request | 의미 | 필요 조건 |
|---|---|---|
| `OPEN` | 완전 열림 방향으로 이동 | 요청 유효성, 전원/운전 허용 |
| `CLOSE` | 완전 닫힘 방향으로 이동 | 요청 유효성, 전원/운전 허용, 끼임 보호 조건 |
| `STOP` | 현재 이동/반전 정지 | 일반 이동 및 반전보다 우선 |
| `VENT` | 환기 목표 위치로 이동 | 위치 정보 유효 필요 |
| `MOVE_TO_POSITION` | 지정 목표 위치로 이동 | 위치 정보 유효, target position 필요 |

### 4.1 `WINDOW_COMMAND` 최소 정보

- Target Channel
- Action
- Request Sequence / Sequence Identifier
- Validity
- Source Context
- Target Position — `VENT`, `MOVE_TO_POSITION` 등 필요한 명령에서 사용
- 요청 수용 기한/만료 판단에 필요한 정보

### 4.2 Request 검증 규칙

Window ECU는 요청 수용 전에 다음을 확인해야 한다.

- Request 종류
- 대상 Channel
- Sequence Identifier
- 요청 유효성
- 필요한 Parameter 존재 여부
- 요청 수용 기한/만료 여부
- 현재 동작과 충돌 여부
- 전원/운전 허용 여부
- 필요한 위치/끼임 정보의 유효성
- ECU Fault/Degraded 상태

### 4.3 중복 요청

동일 Sequence Identifier의 동일 요청을 다시 수신해도 이동을 다시 시작하지 않는다.

### 4.4 요청 우선순위

기본 우선순위는 다음 의미를 따른다.

1. 전기적 보호 / 전원 불허 / 안전 이동 불가 Fault
2. 닫힘 중 Anti-Pinch
3. `STOP`
4. Anti-Pinch 제한 반전
5. 유효한 Local 조작
6. 상위 이동 요청

> Local 조작으로 상위 요청이 대체된 경우 Local 입력 해제 후 과거 상위 요청을 자동 재개하지 않는다.

---

## 5. Command Result

각 명령의 처리 결과는 State와 별도로 제공한다.

| Result | 의미 |
|---|---|
| `ACCEPTED` | 요청 수용 |
| `IN_PROGRESS` | 실제 처리 진행 중 |
| `DONE` | 요청 목표가 유효한 피드백으로 확인됨 |
| `REJECTED` | 실행 전에 요청을 거부 |
| `CANCELLED` | 정상 STOP, 사용자 해제, 유효한 대체 등으로 정상 중단 |
| `FAILED` | Fault, Anti-Pinch, 필수 정보 상실, 목표 미도달 등으로 실패 종결 |

### 5.1 Result 연결 정보

`WINDOW_COMMAND_RESULT`는 최소 다음 의미를 연결할 수 있어야 한다.

- 원 요청 Sequence
- 대상 Channel
- Result
- Reason
- 상위 요청 결과 / Local 조작 결과 구분

### 5.2 대표 결과 규칙

- 목표 위치 확인 → `DONE`
- 요청 수용 조건을 만족하고 이미 목표 위치 → 추가 구동 없이 `DONE`
- 실행 전 무효/만료/미정의/불완전 요청 → `REJECTED`
- 유효한 새 요청에 의해 기존 정상 이동 대체 → 기존 요청 `CANCELLED`
- 사용자 `STOP`으로 기존 이동 중단 → 기존 이동 `CANCELLED`
- 끼임으로 원 닫힘 중단 → 원 닫힘 요청 `FAILED`
- Fault 또는 실행 중 필수 정보 상실 → `FAILED`
- `STOP` 요청 자체는 출력 비활성 확인 시 `DONE`

---

## 6. Event

State와 Event는 분리한다. Event는 특정 순간에 새로 발생한 사실을 의미한다.

### 6.1 `WINDOW_ANTIPINCH`

**의미**

닫힘 이동 중 유효한 끼임이 새로 감지되었음을 알리는 Event.

**최소 정보**

- Channel
- Event Sequence / Event Identifier
- Occurrence Information

**처리 원칙**

- 한 번의 끼임 발생에 한 번 생성
- 재전송 시 동일 Event Identifier 유지
- 중앙에서 중복 제거 가능해야 함
- 현재 Anti-Pinch 상태와 일회성 Event를 같은 데이터로 대체하지 않음

### 6.2 `FULLY_OPENED`

창문이 완전 열림 상태로 **전이**한 사실을 나타내는 Event.

### 6.3 `FULLY_CLOSED`

창문이 완전 닫힘 상태로 **전이**한 사실을 나타내는 Event.

> `FULLY_OPENED` / `FULLY_CLOSED` Event와 현재 `FULLY_OPEN` / `FULLY_CLOSED` 상태 의미는 구분하여 사용한다.

---

## 7. Fault

### 7.1 Fault Category

| Category | 예시 |
|---|---|
| `MOTOR_DRIVER` | 상반 출력, 드라이버 보호, 구동 실패 |
| `POSITION_SENSOR` | 범위 초과, 변화 없음, 유효성 상실 |
| `ANTIPINCH_SENSOR` | 입력 고정, 범위 오류, 유효성 상실 |
| `COMMUNICATION` | 약속된 주기 정보 미수신, 통신 경로 상실 |
| `INITIALIZATION` | 필수 초기화 또는 자체 점검 실패 |
| `FUNCTION` | 상태 전이 불일치, 목표 미도달/시간 초과, 요청 만료/유효성 상실 |

### 7.2 `WINDOW_FAULT` 최소 정보

- Category
- Fault Code
- Active / Recovered 상태
- Validity

### 7.3 Fault 처리 원칙

- Active Fault와 Recovered Fault를 구분
- Fault Category와 세부 Code를 제공
- 실행 중 Fault로 중단된 명령은 `FAILED`와 사유 제공
- 실행 전 거부(`REJECTED`)와 정상 중단(`CANCELLED`)을 Fault 실패와 구분
- 물리 피드백이 없는 구성에서 검출할 수 없는 Fault를 검출된 것으로 보고하지 않음

---

## 8. Data

### 8.1 Window Position

**Name:** `WINDOW_POSITION`  
**Logical Type:** Position value + validity  
**Actual C Type:** TBD  
**Range:** `0 ~ 100`  
**Unit:** `%`

```text
0   = Fully Open
100 = Fully Closed
```

**Description**

- 현재 창문 위치
- 위치 유효성과 함께 제공
- 위치 Sensor Fault 중 이전 위치를 현재 정상값처럼 재사용하지 않음

**Validity**

- `OK`
- `STALE`
- `INVALID`
- `NO_DATA`

**Invalid Raw Value:** TBD

---

### 8.2 Window State

**Name:** `WINDOW_STATE`  
**Logical Type:** Motion State + ECU State + Quality  
**Actual C Type:** TBD

**Data**

- Channel
- Motion State
- ECU State
- Validity
- Timestamp / Age 판단 근거

---

### 8.3 End Position

**Name:** End Position Information  
**Logical Type:** Boolean/State 의미  
**Actual C Type:** TBD

**Data**

- Fully Open
- Fully Closed
- Validity

---

### 8.4 Anti-Pinch Input

**Name:** `WINDOW_ANTIPINCH_INPUT`  
**Logical Type:** Detection + Validity  
**Actual C Type:** TBD

**Data**

- Detected
- Validity

---

### 8.5 Anti-Pinch Protection Status

**Name:** Anti-Pinch Protection Status  
**Actual C Type:** TBD

**Data**

- 연결된 Anti-Pinch Event Identifier
- Protection State
  - 진행
  - 완료
  - 중단
- Reason
- Input Validity
- Reverse 진행/완료/중단 정보
- Motor Output Disabled 근거
- Window Movement Stopped 근거

---

### 8.6 Field Clear Confirmation `[잠정]`

**Name:** 현장 해제 확인 / 끼임 확인 상태  
**Actual C Type:** TBD

**Data**

- Channel
- 관련 Anti-Pinch Event Identifier
- Confirmation Identifier
- Accepted Confirmation 여부
- Output Disabled 확인 근거
- Movement Stopped 확인 근거
- Quality
- Simulation 여부

> 반전 완료 또는 `STOP`만으로 현장 끼임 원인이 제거되었다고 판단하지 않는다.

---

### 8.7 Power / Operation State

**Name:** `POWER_OPERATION_STATE`  
**Actual C Type:** TBD

**Data**

- Operation Allowed
- Validity

> 전원/운전 허용이 없거나 신뢰할 수 없으면 새 이동을 시작하지 않으며 진행 중 이동과 반전도 정지한다.

---

### 8.8 Upper Target Keep-Alive / Execution Permission `[잠정]`

**Name:** 상위 목표 유지 허용  
**Actual C Type:** TBD

**Data**

- Target / Current Command Identifier
- Continue Allowed
- Evaluation Sequence
- Original Information Age

**Current provisional timing**

- 중앙의 실행 유지 허용 평가: `100 ms`마다
- 마지막 유효 평가 후 `300 ms` 경과 시 상위 이동 유지 허용 상실로 처리
- 명시적 철회 / `INVALID`는 즉시 반영

---

## 9. Local Logic

Window ECU가 중앙 Domain Controller의 판단을 기다리지 않고 자체적으로 수행해야 하는 Logic이다.

### 9.1 입력 처리

- Local Switch Sampling
- Switch Debounce / Stable Input 판정
- Manual Hold / One-Touch 입력 구분
- 충돌하는 Local Open/Close 입력 검출
- 상위 Request 유효성 / 순서 / 중복 / 만료 검증

### 9.2 Command Arbitration

- 전기적 보호 최우선
- Anti-Pinch 보호 우선
- `STOP` 우선
- Local 입력과 상위 요청 충돌 시 Local 우선
- 서로 반대 방향의 Motor Output 동시 활성 금지
- 반대 방향 전환 전 현재 출력 해제 및 Dead-Time 적용

### 9.3 Position Logic

- `0 % = Fully Open`
- `100 % = Fully Closed`
- 끝단 도달 시 해당 방향 출력 정지
- 목표 위치 도달 시 출력 정지 후 `DONE`
- 이미 목표 위치인 유효 요청은 추가 구동 없이 `DONE`
- Position Validity 상실 시 위치 의존 동작 중지

### 9.4 Anti-Pinch Logic

```text
Closing
→ Valid Anti-Pinch Detected
→ Closing Output OFF
→ 필요 시 무출력 대기
→ 전원/위치/구동 조건 + STOP 없음 확인
→ 제한 Reverse
→ 목표 확인 또는 중단
→ Event / Protection Status / Command Result 제공
```

- 중앙 명령을 기다리지 않고 즉시 닫힘 출력 해제
- 일반 닫힘보다 보호 동작 우선
- Anti-Pinch Sensor가 신뢰 불가하면 자동 닫힘 / 원터치 닫힘 금지
- 새 Anti-Pinch 발생 후 기존 입력만으로 닫힘 재개 금지
- 재동작 조건 + 새로운 유효 닫힘 입력이 필요

### 9.5 Result / Diagnostic Logic

- Request Result와 현재 State 분리
- Event와 현재 State 분리
- Fault와 정상 State 분리
- 동일 Request 재수신 시 중복 실행 방지
- Event 재전송 시 동일 Event Identifier 유지
- Active Fault / Recovered Fault 구분

---

## 10. Fail-safe

### 10.1 Domain 통신 상실

```text
Domain Communication Lost
→ 진행 중인 상위 이동 정지
→ 상위 이동 자동 재개 금지
→ 독립 Local 보호 유지
→ 통신 복구 + 필요한 입력 정상 확인
→ 새로운 유효 Request 수신 후에만 상위 이동 재개
```

### 10.2 상위 실행 허용 상실

```text
Upper Command Expired / Withdrawn / Invalid
→ 해당 상위 이동 정지
→ 과거 Request 자동 재실행 금지
```

### 10.3 Position Sensor Fault

```text
Position Invalid / Fault
→ 과거 Position을 정상값처럼 사용 금지
→ 위치 기반 One-Touch / Target Move / Anti-Pinch Reverse 정지 또는 제한
→ 제한 Manual 동작 허용 여부는 안전 분석 후 결정
```

> 현재 초안에서는 제한 Manual 동작을 임의로 허용하지 않는다.

### 10.4 Anti-Pinch Sensor Fault

```text
Anti-Pinch Sensor Invalid / Fault
→ Auto Close 금지
→ One-Touch Close 금지
→ 복구 조건 확인 전 자동 닫힘 재개 금지
```

### 10.5 Motor / Driver Fault

```text
Motor / Driver Fault
→ Open Output OFF
→ Close Output OFF
→ Anti-Pinch Reverse도 강제 실행 금지
→ 새 이동 Request 거부
→ 원인 해소 + 정상 입력 확인 + 새 Request 후 재동작
```

### 10.6 Initialization Fault

```text
Initialization Failed
→ FAULT 또는 정의된 DEGRADED
→ Motor Output OFF
→ 초기화 완료 전 의도하지 않은 움직임 금지
```

### 10.7 Power / Operation State Invalid

```text
Operation Not Allowed / Invalid
→ 새 이동 시작 금지
→ 진행 중 이동 정지
→ Anti-Pinch Reverse도 정지
```

### 10.8 Conflicting Local Input

```text
Local OPEN + CLOSE Conflict
→ Motor Stop
→ Input Error로 구분
→ 동시에 반대 방향 출력 금지
```

---

## 11. Timing / Freshness / Quality

아래 값은 현재 SysRS의 **잠정/후보값**이며 실기 검증 전 확정값으로 사용하지 않는다.

| 항목 | 현재 값 | 상태 |
|---|---:|---|
| ECU 초기화 완료 목표 | `≤ 1000 ms` | `[CANDIDATE]` |
| Local Switch Sample | `≤ 10 ms` | `[CANDIDATE]` |
| Switch Debounce | `30 ms` | `[CANDIDATE]` |
| 일반 이동 명령 수용 후 출력 시작 여부 결정 | `≤ 200 ms` | `[CANDIDATE]` |
| `STOP` 후 Motor Output OFF | `≤ 100 ms` | `[CANDIDATE]` |
| Anti-Pinch 판단 후 Closing Output OFF | `≤ 50 ms` | `[CANDIDATE]` |
| 방향 전환 전 Motor Output Dead-Time | `≥ 100 ms` | `[CANDIDATE]` |
| 내부 State 변화 → 외부 제공 State 갱신 | `≤ 100 ms` | `[CANDIDATE]` |
| 상위 실행 유지 허용 평가 | `100 ms` | `[잠정]` |
| 마지막 유효 유지 평가 후 상실 판정 | `300 ms` | `[잠정]` |
| 중앙 → WINDOW 새 실행 명령 수용 기한 | `1 s` | `[잠정]` |
| 상위 목표 이동 최종 결과 대기 | `10 s` | `[잠정]`, 결과 관측용 |
| `STOP` 최종 결과 대기 | `1 s` | `[잠정]`, 결과 관측용 |

### 11.1 중앙 상태 제공 관련 공통 기준

WINDOW의 끼임 / 정지 / 보호 상태 → 중앙 경로는 현재 논리 상태 제공 기준에서 다음 잠정값을 사용한다.

- State 제공 주기 `P = 200 ms`
- 원본 정보 최대 허용 경과 시간 `1000 ms`

> 위 값은 논리적인 상태 최신성 기준이다. 실제 CAN Frame 주기, Message ID, Signal 배치, Timeout 및 E2E 방식은 후속 네트워크 설계에서 확정한다.

---

## 12. 현재 결정되지 않은 항목

다음 항목은 SR/SysRS에서 아직 확정되지 않았으므로 Interface 구현 전에 별도로 결정해야 한다.

### 12.1 Interface / Network

- CAN/LIN/Ethernet 등 실제 전송 매체
- Message ID / Signal ID
- Byte / Bit Layout
- Endianness
- 실제 C Data Type
- Scaling / Offset
- Invalid Raw Value
- 실제 송신 Cycle
- Event Trigger 정책
- Network Timeout
- Retry 횟수
- Alive Counter
- E2E 보호 방식
- Request Sequence 형식
- Event 중복 제거용 Identifier 형식

### 12.2 Window 기능

- 실제 Local Switch 부품 및 접점 구성
- Manual / One-Touch 입력 실제 구현
- Position Calibration 방법
- Anti-Pinch Sensor 방식
- Anti-Pinch Threshold
- Reverse Target / Reverse Distance
- Reverse 최대 시간
- 이동 최대 시간
- 목표 미도달 판정 기준
- 현장 끼임 해제 확인 입력 장치
- 고장 상태에서 제한 Manual 이동 허용 여부
- Motor / H-Bridge 사양
- PWM Frequency / Duty
- 전류 / 온도 보호
- Position 허용 오차
- 다중 Window Channel 수 및 실제 ECU 배치

### 12.3 자동 환기 관련

현재 SysRS의 잠정 기준:

- `POS_VENT = 80 %`
- Position 기준은 `0 % = Fully Open`, `100 % = Fully Closed`
- 약 20% 열린 위치를 의미
- 이미 목표보다 더 열린 상태에서는 자동 환기를 위해 닫지 않는 방향의 정책을 사용

> 자동 환기의 조건 판단 자체는 중앙 Domain Controller의 책임이며 Window ECU에는 최종 목표만 전달한다.

---

## 13. Domain 구현 시 필요한 최소 논리 구조

Window ECU 담당자와 최종 합의가 완료되면 Domain 측에서는 최소 다음 구조를 정의할 수 있다.

```text
WindowRequest
 ├─ channel
 ├─ action
 ├─ target_position
 ├─ sequence
 └─ validity / lifetime

WindowState
 ├─ channel
 ├─ motion_state
 ├─ ecu_state
 ├─ fully_open
 ├─ fully_closed
 └─ validity / age

WindowPosition
 ├─ channel
 ├─ position_percent
 └─ validity / age

WindowEvent
 ├─ channel
 ├─ event_type
 └─ event_id

WindowProtectionStatus
 ├─ channel
 ├─ related_event_id
 ├─ protection_state
 ├─ reason
 └─ validity

WindowCommandResult
 ├─ request_sequence
 ├─ result
 └─ reason

WindowFault
 ├─ category
 ├─ code
 ├─ active / recovered
 └─ validity
```

> 위 구조는 **논리 구조 예시**이며 실제 `struct`, `enum`, 필드 폭 및 C 자료형은 통신/인터페이스 설계 후 확정한다.

---

## 14. 핵심 정리

```text
Request
= Domain이 Window ECU에 무엇을 하라고 확정해서 요청하는가

State
= Window ECU와 창문이 현재 어떤 상태인가

Event
= FULLY_OPENED / FULLY_CLOSED / WINDOW_ANTIPINCH처럼 방금 무엇이 발생했는가

Fault
= Motor / Position / Anti-Pinch / Communication / Initialization / Function에 어떤 문제가 있는가

Data
= Position, Validity, Protection Status 등 실제 값과 품질 정보

Local Logic
= 스위치 처리, Motor 제어, 중재, 끝단 정지, Anti-Pinch 즉시 보호

Fail-safe
= 통신·센서·전원·Motor 이상 시 의도하지 않은 이동을 어떻게 막는가
```

Window ECU의 핵심 원칙은 **중앙에서 확정된 의미 기반 명령을 실행하되, 실제 Motor 제어와 Anti-Pinch 같은 즉시 보호는 ECU 내부에서 독립적으로 수행하고, State / Event / Fault / Result / Data를 분리하여 중앙에 제공하는 것**이다.
