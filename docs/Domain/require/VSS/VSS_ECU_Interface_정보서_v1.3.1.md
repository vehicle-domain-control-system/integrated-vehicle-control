# VSS ECU Interface 정보서

**문서 버전**: v1.3.1
**작성일**: 2026-09-19  
**대상 ECU**: VSS (Vehicle Sound System) / S32K344  
**기준 문서**: SR v0.46 / SysRS v0.46 / ECU Interface 정보 요청 가이드  
**목적**: Domain Controller가 `VSS_Interface.h`와 차량 상태 관리 구조를 설계할 수 있도록 VSS의 논리 Interface 정보를 정리한다.

> 본 문서는 **State / Request / Event / Fault / Data / Local Logic / Fail-safe**를 정의한다.  
> CAN ID, Start Bit, Length, DLC, Byte Order, 실제 Cycle Time, Counter 폭, CRC/E2E 등 **통신 규격은 본 문서의 정의 범위에 포함하지 않는다.**

---

# 1. 주요 기능

- 차량 사용 시작 시 Welcome 음향 제공
- 차량 사용 종료 시 Goodbye 음향 제공
- 도어 잠금 완료 확인음 제공
- 도어 잠금 해제 완료 확인음 제공
- 도어 잠금 실행 실패 경고음 제공
- 파워윈도우 Anti-Pinch 긴급 경고음 제공
- 잔류 탑승자 위험 긴급 경고음 제공
- 후방 장애물 `CAUTION / EMERGENCY` 경고음 제공
- 여러 음향 조건에 대한 우선순위 중재(Priority Arbitration) 및 선점(Preemption)
- One-shot Event의 중복·지연·역순 재생 방지
- Stateful Warning의 품질(Quality), 최신성(Freshness), Hold 관리
- VSS 동작 상태·가용성·Fault 정보 제공
- 출력 Fault 복구 후 현재 유효한 Warning State 재평가

## 책임 경계

VSS는 **차량에서 의미가 확정된 Event와 Warning State를 입력받아 음향으로 표현하는 ECU**이다.

VSS가 직접 판단하지 않는 항목은 다음과 같다.

- 후방 거리 측정 및 위험 등급 계산
- Anti-Pinch 발생 여부 판단
- 잔류 탑승자 위험 여부 판단
- 차량 사용 시작·종료 판단
- 도어 요청 성공·실패 판단
- Sensor Raw Data 해석
- 차량 전체 정책 판단

각 기능의 의미 판단은 Domain Controller 또는 해당 기능을 담당하는 ECU에서 수행한다. VSS는 확정된 의미를 입력받아 음향으로 표현한다.

---

# 2. State

## 2.1 VSS State

| State | 의미 |
|---|---|
| `STARTUP` | 전원 인가 후 초기화가 완료되지 않은 상태 |
| `READY` | 초기화가 완료되었으며 현재 활성 Playback Session이 없는 상태 |
| `PLAYING` | 하나의 Playback Session이 활성화된 상태 |
| `FAULT` | 정상적인 VSS 음향 출력을 보장할 수 없는 상태 |

### 해석

- `PLAYING`은 실제 음향이 출력되는 순간뿐 아니라 동일한 Playback Session 내의 반복 간격이나 cue 사이의 무음 구간도 포함한다.
- `READY`와 `PLAYING` 상태에서는 모두 새로운 유효 Event / State를 수용할 수 있다.
- `STARTUP` 자체는 Fault를 의미하지 않는다.
- `READY` 상태에서도 정상 음향 출력을 보장할 수 없는 중대 Fault가 검출되면 `FAULT`로 전이할 수 있다.
- `SLEEP / Wake` 관련 전원 상태를 상위 `VSS_STATE`에 추가할지는 Power Architecture 확정 후 결정한다.
- **[후보]** 전원 인가 후 `READY` 또는 `FAULT` 상태 확정: `≤ 1000 ms`

---

## 2.2 VSS Availability

VSS의 서비스 가용성은 `VSS_STATE`와 별도의 상태로 관리한다.

| Availability | 의미 |
|---|---|
| `FULL` | 정의된 VSS 기능을 정상적으로 제공할 수 있는 상태 |
| `DEGRADED` | 일부 Sound Asset 또는 일부 기능이 제한되지만 나머지 VSS 기능은 제공할 수 있는 상태 |
| `UNAVAILABLE` | 정상적인 음향 출력을 보장할 수 없는 상태 |

### 관계

- `FULL` 또는 `DEGRADED` 상태에서도 `READY` 또는 `PLAYING`이 가능하다.
- `UNAVAILABLE`은 정상 출력을 보장할 수 없는 상태이므로 `FAULT`와 연계한다.
- `VSS_FAULT_ACTIVE = true`라고 해서 반드시 `VSS_STATE = FAULT`인 것은 아니다.
- `STARTUP` 동안 Availability는 아직 평가되지 않은 상태이며, 실제 Interface 표현 방식은 `TBD`이다.

---

## 2.3 VSS Input Acceptance

### `VSS_ACCEPTING_EVENTS`

- 예상 자료형: `bool`
- 의미: 현재 VSS가 Semantic Event / State를 수용·검증·중재할 수 있는지 여부
- `READY`뿐 아니라 입력을 수용할 수 있는 `PLAYING` 상태에서도 `true`가 될 수 있음
- `VSS_STATE` 또는 `VSS_AVAILABILITY`와 동일한 의미가 아님
- 독립 Signal로 구현할지는 후속 Interface/통신 설계에서 결정

---

# 3. Request / Command

## 제품용 Request / Command

현재 Baseline에서는 Domain Controller가 VSS에 Sound Asset이나 Playback 동작을 직접 지정하는 제품용 Request / Command를 사용하지 않는다.

다음과 같은 명령은 사용하지 않는다.

- `PLAY_SOUND_xxx`
- `STOP_SOUND`
- `SET_PRIORITY`
- `SET_VOLUME`

Domain Controller는 VSS에 다음과 같은 **의미 정보**를 제공한다.

```text
무슨 일이 발생했는가
현재 어떤 경고 상태인가
```

VSS는 해당 의미를 바탕으로 다음 항목을 내부에서 결정한다.

```text
어떤 Sound Asset을 사용할지
어떤 우선순위를 적용할지
어떤 Pattern으로 재생할지
```

예:

```text
DOOR_LOCK_COMPLETE
→ VSS 내부에서 Lock 확인음 선택

REAR_OBSTACLE_STATE = EMERGENCY
→ VSS 내부에서 Emergency 음향 선택
```

시험용 강제 입력, Fault Injection, 음원 누락 모의 등은 제품용 Request / Command와 분리한다. 별도의 Test/Diagnostic Interface는 `TBD`이다.

---

# 4. Event

## 4.1 Domain → VSS One-shot Event

VSS가 입력받는 One-shot Event는 다음과 같다.

| Event | 의미 | Class |
|---|---|---|
| `VEHICLE_WELCOME` | 유효한 차량 사용 시작 전이가 확정된 경우 | Feedback |
| `VEHICLE_GOODBYE` | 유효한 차량 사용 종료 전이가 확정된 경우 | Feedback |
| `DOOR_LOCK_COMPLETE` | 유효한 새 LOCK 요청에 대해 목표 잠금 상태가 정상적으로 확인된 경우 | Feedback |
| `DOOR_UNLOCK_COMPLETE` | 유효한 새 UNLOCK 요청에 대해 목표 잠금 해제 상태가 정상적으로 확인된 경우 | Feedback |
| `DOOR_LOCK_ERROR` | 수용된 LOCK 요청을 실제로 실행한 뒤 목표 잠금 상태를 확인하지 못한 경우 | Warning |

### Door Event 해석

`DOOR_LOCK_COMPLETE / DOOR_UNLOCK_COMPLETE`는 **새로운 유효 요청에 대한 확인 Event**이다.

다음 경우도 정상적인 Event 생성 대상이다.

```text
새 LOCK 요청
+ 이미 LOCKED 상태
+ 현재 목표 상태를 정상적으로 확인
→ DOOR_LOCK_COMPLETE

새 UNLOCK 요청
+ 이미 UNLOCKED 상태
+ 현재 목표 상태를 정상적으로 확인
→ DOOR_UNLOCK_COMPLETE
```

다음 경우는 새로운 Event로 취급하지 않는다.

- 동일 Request의 재전달
- 결과 조회
- 주기적인 Door State 보고
- 실행 전 `REJECTED`
- 결과 `UNKNOWN`
- 해당 요청과 무관한 상태 변화

`DOOR_LOCK_ERROR`는 **수용된 LOCK 요청을 실제로 실행한 뒤 목표 상태 확인에 실패한 경우**에만 사용한다.

---

## 4.2 One-shot Event에 필요한 논리 정보

각 One-shot Event는 최소한 다음 사항을 판정할 수 있어야 한다.

- Event 종류
- 새로운 발생인지, 동일 발생의 재전달인지
- 원 발생 시점 또는 원 발생 이후의 경과 시간
- Event의 유효성
- 늦게 도착한 과거 Event인지 여부
- ECU 재부팅 전후의 발생이 서로 구분되는지 여부

이 정보는 **논리적으로 필요한 정보 또는 동등한 판정 근거**를 의미하며, 각각을 독립 Signal로 구현해야 한다는 뜻은 아니다.  
실제 표현 방식은 통신 담당자가 결정한다.

---

## 4.3 One-shot Event 재생 시작 유효기간

**[잠정]**

```text
원 Event 발생
→ 실제 음향 출력 시작
```

까지의 경과 시간이 `2초 미만`인 경우에만 새로운 Playback을 시작한다.

```text
Age < 2 s   → 시작 가능
Age >= 2 s  → 새 재생 시작 금지
```

다음 상황에서도 원 발생 기준 시점을 초기화하지 않는다.

- 동일 Event 재전달
- 통신 복구
- VSS `STARTUP` 완료
- VSS 출력 기능 복구
- 높은 우선순위 음향으로 인한 대기

실제 Playback을 시작하기 직전에 Event Age와 동일 발생 여부를 다시 확인한다.

이미 시작한 One-shot Event가 높은 우선순위 음향 또는 Fault로 중단된 경우에는 자동으로 재개하지 않는다.

---

## 4.4 Startup / Wake 중 Event

VSS가 `STARTUP` 상태이거나 Wake 전환 중일 때 발생한 유효 Event가 **재생 시작 유효기간 내에 유실되지 않도록** 전달 계약이 필요하다.

최소한 다음 정보의 의미가 유지되어야 한다.

- 발생 식별 정보
- 원 발생 시점 또는 경과 시간
- 중복 여부
- Event 유효성

실제 전달 방식은 Power / Interface / 통신 설계에서 결정한다.

---

## 4.5 VSS → Domain Event

현재 Baseline에서 VSS가 Domain Controller에 반드시 제공해야 하는 별도의 One-shot Event는 없다.

VSS의 현재 동작 상태와 Fault는 각각 State 및 Fault 정보로 제공한다.

---

# 5. Fault

Fault는 다음 두 범주로 구분한다.

1. **VSS Internal Output Fault**
2. **Input / Interface Diagnostic**

외부 입력 이상을 VSS 내부 출력 Fault로 자동 변환하지 않는다.

---

## 5.1 VSS Internal Output Fault

| Fault | 의미 |
|---|---|
| `SOUND_ASSET_UNAVAILABLE` | 필요한 저장 음향 Asset을 사용할 수 없는 상태 |
| `PLAYBACK_START_FAILURE` | Playback 시작에 실패한 상태 |
| `AUDIO_OUTPUT_FAILURE` | 공통 Audio Output Path 또는 실제 출력 경로에 이상이 발생한 상태 |
| `PLAYBACK_STATE_FAILURE` | 내부 Playback 상태 관리에 이상이 발생한 상태 |
| `INITIALIZATION_FAILURE` | VSS 초기화에 실패한 상태 |

### 기본 영향

- 일부 Sound Asset만 사용할 수 없는 경우 해당 음향만 제외하고 다른 정상 음향은 계속 제공할 수 있다.
- 일부 기능만 제한되는 경우 `DEGRADED` 상태가 될 수 있다.
- 공통 Audio Output 경로의 정상 동작을 보장할 수 없는 경우 `FAULT / UNAVAILABLE`로 전환한다.
- `INITIALIZATION_FAILURE`가 확정되면 `STARTUP → FAULT`로 전이하고 Availability는 `UNAVAILABLE`이 된다.
- 사용할 수 없는 Sound Asset을 다른 의미의 Sound로 임의 대체하지 않는다.

Fault별 세부 Severity, Retry, Re-initialization 횟수는 `TBD`이다.

---

## 5.2 Fault 상태 정보

### `VSS_FAULT_ACTIVE`

- 예상 자료형: `bool`
- 의미: 현재 활성 상태인 Internal Output Fault가 하나 이상 존재하는지 여부

`true`라고 해서 반드시 VSS 전체 음향 출력이 불가능한 것은 아니다.

---

### `VSS_LAST_FAULT`

가장 최근의 주요 Internal Output Fault를 나타낸다.

값 후보:

- `NONE`
- `SOUND_ASSET_UNAVAILABLE`
- `PLAYBACK_START_FAILURE`
- `AUDIO_OUTPUT_FAILURE`
- `PLAYBACK_STATE_FAILURE`
- `INITIALIZATION_FAILURE`

현재 활성 Fault와 최근 주요 Fault는 서로 다른 의미이다.

최근 주요 Fault 정보는 정의된 Diagnostic Clear 정책이 적용될 때까지 확인할 수 있어야 한다.

다음 항목은 `TBD`이다.

- Fault History 삭제 조건
- NVM 보존 여부
- 전원 재인가 후 유지 여부
- 저장 개수

---

## 5.3 복수 Fault 관리

둘 이상의 Internal Output Fault가 동시에 존재할 수 있으므로 VSS 내부에서는 각 Active Fault를 손실 없이 독립적으로 관리해야 한다.

제품 Interface에서 기본적으로 제공하는 요약 정보는 다음과 같다.

- `VSS_FAULT_ACTIVE`
- `VSS_LAST_FAULT`
- `VSS_AVAILABILITY`
- `VSS_STATE`

전체 Active Fault Bitmap/List의 외부 제공 여부는 `OPTIONAL / TBD`이다.

---

## 5.4 Input / Interface Diagnostic

| Diagnostic | 의미 |
|---|---|
| `INVALID_EVENT` | 지원하지 않거나 정의된 의미 계약을 만족하지 않는 Event |
| `INPUT_NOT_RECEIVED` | 주기적으로 제공되어야 하는 Stateful 입력을 정상적으로 수신하지 못한 상태 |
| `INPUT_STALE` | 과거에 유효한 입력은 있었으나 Original Max Age에 도달한 상태 |
| `INPUT_INVALID` | 입력값·형식·유효성 검증에 실패한 상태 |

### 주의

- One-shot Event가 발생하지 않았다는 이유만으로 `INPUT_NOT_RECEIVED`를 설정하지 않는다.
- `SNA / NOT_AVAILABLE / UNCONFIRMED`는 정상 `CLEAR`와 구분한다.
- Input Diagnostic만으로 `VSS_FAULT_ACTIVE`를 활성화하지 않는다.

---

# 6. Data

VSS는 후방 거리나 센서 Raw 값과 같은 **실제 물리 측정값을 직접 입력받아 위험 상태를 판단하지 않는다.**

VSS가 사용하는 Data는 Domain Controller가 의미를 확정한 **Stateful Warning 정보와 해당 정보의 품질·최신성 정보**이다.

---

## 6.1 Window Anti-Pinch State

**Name**: `WINDOW_ANTIPINCH_STATE`  
**Type**: Enum  
**Range**: `CLEAR / ACTIVE`  
**Unit**: 없음  
**Semantic Owner**: Domain Controller

**Description**

- `ACTIVE`: 현재 유효한 Anti-Pinch 경고가 활성화된 상태
- `CLEAR`: Domain Controller가 정상적인 해제 조건을 확인한 상태
- 미수신, `STALE`, `INVALID`, `SNA` 상태를 `CLEAR`로 해석하지 않음

**Freshness**

- **[잠정] 논리 제공 주기 P (Logical Provision P)**: `200 ms`
- **[잠정] 원본 정보 최대 경과 시간 (Original Max Age)**: `1000 ms`
- **[잠정] Hold**: 마지막 유효 상태가 `ACTIVE`였고 이후 입력의 신뢰성을 잃은 경우 `1000 ms`

---

## 6.2 Occupant Hazard State

**Name**: `OCCUPANT_HAZARD_STATE`  
**Type**: Enum  
**Range**: `CLEAR / ACTIVE`  
**Unit**: 없음  
**Semantic Owner**: Domain Controller

**Description**

- `ACTIVE`: 잔류 탑승자 위험 조건이 현재 유효하게 성립한 상태
- `CLEAR`: 정상적인 위험 해제 조건이 확인된 상태
- VSS는 탑승자 수, 온도, 차량 사용 상태 등을 이용해 위험 여부를 다시 계산하지 않음

**Freshness**

- **[잠정] 논리 제공 주기 P (Logical Provision P)**: `200 ms`
- **[잠정] 원본 정보 최대 경과 시간 (Original Max Age)**: `1000 ms`
- **[잠정] Hold**: 마지막 유효 상태가 `ACTIVE`였고 이후 입력의 신뢰성을 잃은 경우 `1000 ms`

여러 입력을 조합하여 판단하는 정보이므로 정확한 Original Age 산정 방식은 `TBD`이다.

---

## 6.3 Rear Obstacle State

**Name**: `REAR_OBSTACLE_STATE`  
**Type**: Enum  
**Range**: `CLEAR / CAUTION / EMERGENCY`  
**Unit**: 없음  
**Semantic Owner**: Domain Controller  
**Upstream Basis**: CIS 거리·품질 정보

**Description**

- `CLEAR`: 후방 감지 기능이 활성화된 상태에서, 유효한 근거를 통해 장애물이 위험 기준 밖에 있거나 장애물이 감지되지 않았음이 확인된 상태
- `CAUTION`: 후방 장애물이 주의 수준으로 판단된 상태
- `EMERGENCY`: 후방 장애물이 긴급 수준으로 판단된 상태
- VSS는 거리값을 이용해 위험 수준을 다시 계산하지 않음

**Freshness**

- **[잠정] 논리 제공 주기 P (Logical Provision P)**: `200 ms`
- **[잠정] 원본 정보 최대 경과 시간 (Original Max Age)**: `500 ms`
- **[잠정] Hold**: 마지막 유효 상태가 `CAUTION / EMERGENCY`였고 이후 입력의 신뢰성을 잃은 경우 `300 ms`

---

## 6.4 Rear Detection Activation

**Name**: `REAR_DETECTION_ACTIVATION`  
**Type**: Enum + Quality / Confirmation  
**Range**: `ACTIVE / DISABLED`  
**Unit**: 없음

**Description**

- `ACTIVE`: 후방 감지 기능이 활성화된 상태
- `DISABLED`: 후방 감지 기능이 정상적으로 비활성화된 상태
- `DISABLED`는 위험 상태의 `CLEAR`와 같은 의미가 아님
- 활성 여부를 정상적으로 확인할 수 없는 경우 `UNCONFIRMED / NOT_AVAILABLE`로 구분

### 논리 축

```text
Activation Value
- ACTIVE
- DISABLED

Reception Quality
- NOT_RECEIVED
- VALID
- STALE
- INVALID

Semantic Availability / Confirmation
- CONFIRMED / AVAILABLE
- UNCONFIRMED / NOT_AVAILABLE
```

각 논리 축을 실제 Signal로 분리할지와 Encoding 방식은 통신 설계에서 결정한다.

---

## 6.5 Stateful Quality / Freshness

VSS는 Stateful 입력별로 다음 품질 상태를 구분할 수 있어야 한다.

| Quality | 의미 |
|---|---|
| `NOT_RECEIVED` | 정상 정보를 아직 수용하지 못한 상태 |
| `VALID` | 현재 유효성 및 최신성 조건을 충족한 상태 |
| `STALE` | 과거 유효 정보는 있으나 최대 허용 경과 시간에 도달한 상태 |
| `INVALID` | 입력값·형식·유효성 검증에 실패한 상태 |

송신 측에서 제공하는 `SNA / NOT_AVAILABLE / UNCONFIRMED`는 정상 상태 값과 구분한다.

### 핵심 원칙

다음과 같은 상태가 가능하다.

```text
Value = EMERGENCY
Quality = STALE
```

이는 다음을 의미한다.

```text
마지막으로 확인된 유효 상태는 EMERGENCY였지만
현재는 이를 유효한 EMERGENCY 상태로 사용할 수 없음
```

따라서 VSS는 최소한 다음 항목을 서로 구분하여 관리한다.

- Last Received Value
- Last Valid Value
- Current Quality
- Effective State

---

## 6.6 Stateful Ordering / Restart

같은 값이 반복되더라도 다음 두 경우를 구분할 수 있어야 한다.

```text
새로운 유효 평가
```

```text
동일한 과거 정보의 재전달
```

또한 Central 또는 Source ECU의 재부팅 전후 정보가 서로 혼동되지 않아야 한다.

이를 위해 다음과 같은 논리 정보 또는 동등한 판정 근거가 필요하다.

- 원본 시점 또는 Age
- Update Ordering
- Restart / Generation Context

실제 Counter, Timestamp, Generation의 표현 방식은 통신 설계에서 결정한다.

---

## 6.7 Raw Data 제외

다음 데이터는 VSS 제품 Interface의 입력으로 사용하지 않는다.

- `rear_distance_cm`
- Ultrasonic Raw Echo
- Camera Raw Image
- Occupant Raw Detection Score
- Window Motor Current
- Anti-Pinch Raw Sensor Value
- Door Raw Switch State

---

# 7. Local Logic

VSS가 자체적으로 수행하는 주요 판단 및 처리 로직은 다음과 같다.

---

## 7.1 Input Validation

수신한 Event / State에 대해 다음 사항을 확인한다.

- 정의된 Event / State인지
- 현재 사용할 수 있는 유효한 정보인지
- 허용된 최신성 기준을 초과하지 않았는지
- 동일 발생의 중복 정보인지
- 이전 실행 구간에서 늦게 도착한 정보인지
- Stateful 입력의 Quality / Availability를 정상적으로 사용할 수 있는지

잘못된 입력을 임의의 정상 의미로 보정하지 않는다.

---

## 7.2 One-shot / Stateful 분리 처리

### One-shot Event

```text
새 발생 확인
→ 원 발생 Age 및 유효성 확인
→ 대기 후보(Pending Candidate) 등록
→ Arbitration
→ 실제 시작 직전 유효성 재확인
→ Playback 또는 폐기
```

### Stateful Warning

```text
Value / Quality / Freshness 갱신
→ Last Valid / Effective State 계산
→ Candidate 갱신
→ Arbitration
```

동일한 Stateful 값이 반복 수신되어도 매번 Playback Session을 다시 시작하지 않는다.

---

## 7.3 Priority Arbitration

우선순위 Class는 다음과 같다.

```text
Emergency
>
Warning / Caution
>
Feedback
```

**[잠정] 동일 Class 내 우선순위**

### Emergency

```text
Anti-Pinch
>
Rear Emergency
>
Occupant Hazard
```

### Warning / Caution

```text
Rear Caution
>
Door Lock Error
```

### Feedback

```text
Door Unlock Complete
>
Door Lock Complete
>
Vehicle Goodbye
>
Vehicle Welcome
```

같은 의미의 서로 다른 One-shot Event가 동시에 후보가 된 경우 원 발생 순서를 우선한다. 원 발생 시점도 같다면 발생 식별자의 고정 순서로 결정한다.

단순한 수신 순서만으로 재생 대상(Winner)을 결정하지 않는다.

---

## 7.4 Playback

- 한 시점에는 Active Playback Session을 최대 1개만 유지한다.
- 여러 Sound를 동시에 재생하는 Mixing은 현재 범위에 포함하지 않는다.
- 높은 우선순위 후보는 낮은 우선순위 Playback을 선점할 수 있다.
- 낮은 우선순위의 신규 후보는 현재 재생 중인 높은 우선순위 Playback을 중단시키지 않는다.
- 선점된 Stateful Warning은 현재 상태가 계속 유효한 경우 Candidate로 유지한다.
- 높은 우선순위 음향이 종료된 뒤 Stateful Warning이 여전히 유효하면 다시 선택할 수 있다.
- 시작 후 중단된 One-shot Event는 자동으로 재개하지 않는다.
- 재중재 결과 Winner가 동일하면 현재 Playback Session을 불필요하게 다시 시작하지 않는다.

---

## 7.5 Sound Mapping

VSS는 다음 항목을 내부 정책에 따라 결정한다.

- Semantic Event / Warning State에 대응하는 Sound Asset
- 우선순위
- 재생 Pattern
- Playback Session 구성

Domain Controller는 Sound ID나 우선순위를 직접 지정하지 않는다.

---

## 7.6 Playback Timing

현재 SysRS의 후보값은 다음과 같다.

| 조건 | 기준 |
|---|---:|
| Feedback Winner 선택 → 실제 출력 시작 | `[후보] ≤ 200 ms` |
| Warning Winner 선택 → 실제 출력 시작 | `[후보] ≤ 100 ms` |
| Emergency Winner 선택 → 실제 출력 시작 | `[후보] ≤ 50 ms` |
| Rear `CAUTION → EMERGENCY` 변경 후 재중재 결과 즉시 Winner가 된 경우 | `[CANDIDATE] ≤ 50 ms` |
| Rear `EMERGENCY → CAUTION` 변경 후 기존 긴급 출력 종료 | `[CANDIDATE] ≤ 100 ms` |

위 시간은 실제 Network 전송 시간과 구분한다.

---

# 8. Fail-safe

## 8.1 Invalid / Unsupported Input

```text
잘못된 Event / State 수신
→ 해당 신규 Candidate 생성 또는 갱신 금지
→ 잘못된 의미의 Sound 출력 금지
→ Input Diagnostic 기록
→ 다른 정상 Playback 유지
```

---

## 8.2 Stateful 입력 신뢰성 상실

`NOT_RECEIVED / STALE / INVALID / SNA / UNCONFIRMED` 상태를 정상 `CLEAR`로 변환하지 않는다.

### 마지막 유효 활성 경고가 없는 경우

```text
최초 미수신
또는 Last Valid = CLEAR
→ 새 Warning Candidate 생성 금지
→ Hold 적용하지 않음
```

### 마지막 유효 활성 경고가 있는 경우

```text
Last Valid Warning 존재
→ 현재 입력의 신뢰성 상실
→ 기능별 Hold 동안 마지막 Warning Candidate를 제한적으로 유지
```

---

## 8.3 Hold

**[잠정]**

| Warning | Hold |
|---|---:|
| Rear `CAUTION / EMERGENCY` | `300 ms` |
| Anti-Pinch `ACTIVE` | `1000 ms` |
| Occupant Hazard `ACTIVE` | `1000 ms` |

Hold는 **입력을 처음 신뢰할 수 없게 된 시점**부터 시작한다.

다음 상황에서는 Hold 타이머를 다시 시작하지 않는다.

- 동일한 신뢰 불가 상태의 반복
- 동일 정보의 재전달
- `STALE → INVALID → SNA` 등 신뢰성 상실 원인의 변경
- 다른 높은 우선순위 Sound에 의한 선점

Hold 중 새로운 VALID 입력이 수신되면 Hold 만료를 기다리지 않고 즉시 최신 상태를 적용한다.

---

## 8.4 CLEAR / DISABLED / Hold Expiry

### Valid CLEAR

- 해당 Warning Candidate를 제거한다.
- 남아 있는 Candidate를 기준으로 다시 중재한다.
- **[잠정]** 출력 종료 정책을 적용한다.
- **[후보]** 해당 Warning이 실제 출력 중인 경우, CLEAR 수용 시점부터 `≤ 100 ms` 이내에 해당 출력을 종료한다.

### Valid Rear DISABLED

- Rear Candidate를 제거한다.
- Hold를 적용하지 않는다.
- `DISABLED`를 위험 상태의 `CLEAR`로 해석하지 않는다.
- **[후보]** Rear 경고가 실제 출력 중인 경우, DISABLED 수용 시점부터 `≤ 100 ms` 이내에 해당 출력을 종료한다.

### Hold Expiry

- 해당 Candidate를 제거한다.
- 남아 있는 Candidate를 기준으로 다시 중재한다.
- Hold 만료를 위험 상태의 `CLEAR`로 해석하지 않는다.
- **[후보]** 해당 경고가 실제 출력 중인 경우, Hold 만료 시점부터 `≤ 100 ms` 이내에 해당 출력을 종료한다.

---

## 8.5 Sound Asset Fault

일부 Sound Asset만 사용할 수 없는 경우 다음과 같이 처리한다.

```text
해당 의미의 Playback 후보에서 제외
→ 다른 의미의 Sound로 대체하지 않음
→ VSS_AVAILABILITY = DEGRADED
→ 다른 정상 Candidate는 계속 처리
```

Sound Asset이 복구된 뒤 Stateful Warning이 여전히 유효한 경우 다시 Candidate로 평가할 수 있다.

One-shot Event를 Sound Asset 복구 시점까지 Pending 상태로 유지할지는 `TBD`이다.

---

## 8.6 Common Audio Output Fault

공통 Audio Output 경로의 정상 동작을 보장할 수 없는 경우 다음과 같이 처리한다.

```text
현재 Playback 중단
→ VSS_STATE = FAULT
→ VSS_AVAILABILITY = UNAVAILABLE
→ 해당 Internal Fault 활성화
→ Fault 정보를 Domain Controller에 제공
```

VSS가 `READY` 상태이더라도 정상 출력을 보장할 수 없는 Fault가 확인되면 `FAULT / UNAVAILABLE`로 전환할 수 있다.

---

## 8.7 Initialization Failure

```text
초기화 실패
→ VSS_STATE = FAULT
→ VSS_AVAILABILITY = UNAVAILABLE
→ INITIALIZATION_FAILURE 활성화
```

정상적인 `STARTUP` 자체는 Fault를 의미하지 않는다.

---

## 8.8 Recovery

복구 대상 Fault가 해제되면 남아 있는 다른 Active Fault와 VSS 전체의 출력 가능 여부를 다시 평가한다.

```text
정상 음향 출력 가능
→ READY
→ Availability 재평가
→ 현재 유효한 Stateful Warning 재평가
→ 아직 시작하지 않은 유효 One-shot Event 재평가
→ Arbitration
```

일부 기능만 제한된 경우에는 다음 상태가 가능하다.

```text
READY 또는 PLAYING
+
DEGRADED
```

정상 출력을 보장할 수 없는 Fault가 남아 있으면 `FAULT / UNAVAILABLE` 상태를 유지한다.

Fault가 복구되었다는 이유만으로 과거 음향을 다시 재생하지 않는다.

**[CANDIDATE]** 복구 가능한 Output Fault가 검출된 시점부터 복구 성공 또는 실패를 결정할 때까지: `≤ 2000 ms`

---

## 8.9 Restart / Communication Recovery

- VSS 재시작 후 이전 실행 구간의 상태를 현재 `VALID` 상태로 자동 복원하지 않는다.
- 새로운 유효 State / Quality / Ordering 근거가 확인될 때까지 해당 입력은 현재 사용 불가·미확인 상태로 처리한다.
- VSS 재시작 자체로 Welcome / Goodbye 등의 One-shot Event를 새로 생성하지 않는다.
- 통신 복구 시 누락된 과거 Stateful 전이를 순서대로 재생하지 않고 현재 유효 상태를 기준으로 재동기화한다.
- 이미 완료·중단·만료된 One-shot Event를 통신 복구를 이유로 다시 재생하지 않는다.

---

## 8.10 기능 격리

VSS Fault가 도어·창문·공조·CIS 등 다른 차량 기능을 VSS에서 직접 중단시키지 않는다.

VSS는 자신의 State / Availability / Fault 정보를 Domain Controller에 제공하며, 차량 전체 차원의 대응은 Domain Controller가 담당한다.

---

# 최종 요약

```text
State
- VSS_STATE
- VSS_AVAILABILITY
- VSS_ACCEPTING_EVENTS

Request / Command
- 제품용 Direct Sound Command 없음

Event
- VEHICLE_WELCOME
- VEHICLE_GOODBYE
- DOOR_LOCK_COMPLETE
- DOOR_UNLOCK_COMPLETE
- DOOR_LOCK_ERROR

Fault
- SOUND_ASSET_UNAVAILABLE
- PLAYBACK_START_FAILURE
- AUDIO_OUTPUT_FAILURE
- PLAYBACK_STATE_FAILURE
- INITIALIZATION_FAILURE
- Input Diagnostic 4종

Data
- WINDOW_ANTIPINCH_STATE
- OCCUPANT_HAZARD_STATE
- REAR_OBSTACLE_STATE
- REAR_DETECTION_ACTIVATION
- Quality / Freshness / Ordering 정보

Local Logic
- Input Validation
- Sound Mapping
- Priority Arbitration
- Preemption
- One-shot / Stateful 관리
- Playback Session 관리

Fail-safe
- Invalid 입력으로 잘못된 Sound 출력 금지
- Stateful 입력 신뢰성 상실 시 제한적인 Hold 적용
- CLEAR / DISABLED / Hold 만료 처리
- Sound Asset Fault 시 DEGRADED
- 공통 Audio Output Fault 시 FAULT / UNAVAILABLE
- 복구 후 현재 유효 상태만 재평가
```

## 통신 담당자 후속 결정 항목

본 정보서를 기반으로 통신 담당자가 다음 항목을 별도로 결정한다.

- CAN ID
- Start Bit
- Length
- DLC
- Byte Order
- Enum 실제 숫자값
- Cycle Time / Timeout 구현값
- Occurrence / Sequence / Generation 실제 표현 방식
- Timestamp / Age Encoding
- CRC / E2E
- Alive / Rolling Counter
- Bus-Off / Recovery
- Message Packing
