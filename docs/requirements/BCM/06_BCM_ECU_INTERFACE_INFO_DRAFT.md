# BCM ECU — Interface 정보 (Domain 전달용)

> ECU Interface 정보 요청 가이드의 7개 항목 양식에 따라 작성했다.
> 근거: `SR §5 BCM 기능 요구`, `sysRS §4 BCM 상세 요구사항`.
> 명칭은 Interface 가이드의 예시 규칙(`LOCK_COMPLETED` · `*_FAULT` · `*_DETECTED`)에 맞췄다. 추가 조정이 필요하면 Domain 담당자의 통일안을 따른다.
> 수치는 `sysRS §4.13 설계값 후보`의 값으로, 실측 후 조정 대상이다.

---

## 1. 주요 기능

- 도어 잠금 / 잠금 해제 (액추에이터 1)
- 실내 공기 순환 — Fan 4단계 (Fan 1)
- 실내 온도 조절 — 냉각/가열 (펠티어 + H브릿지 1)
- 실내 조명 — 알림 4종 × RGB 3채널 (LED 3 ch)
- 자체 안전 차단 (과열·도어 열림·구동 시간 상한)

BCM은 **확정된 명령만 실행**한다. 센싱 값(실내 온도·조도·탑승자)을 받지 않고, 판단하지 않으며, 다른 기능 사이의 정보를 중계하지 않는다.

---

## 2. State

모든 State는 **유효 상태(`OK` / `STALE` / `INVALID` / `NO_DATA`)와 함께** 제공한다. 값과 유효 상태를 분리하면 수신 측에서 신뢰성 표시가 누락된다.

### ECU

- `INIT` / `READY` / `DEGRADED` / `FAULT`
  - `DEGRADED` = 일부 기능만 정지, 나머지는 정상 수행

### Door

- 잠금 상태: `LOCKED` / `UNLOCKED` / `UNKNOWN`
- 개폐 상태: `CLOSED` / `OPEN` / `UNKNOWN` (잠금과 **독립된 접점 입력**)
- 종합 상태: `NORMAL` / `INCONSISTENT` / `UNTRUSTED`
  - `LOCKED` + `OPEN` 은 물리적으로 불가 → `INCONSISTENT`
  - 어느 한쪽이 `UNKNOWN` → `UNTRUSTED`

### Climate

- Fan 지시 수준: `OFF` / `LOW` / `MEDIUM` / `HIGH` (수신 명령 보관값)
- Fan 측정 수준: `OFF` / `LOW` / `MEDIUM` / `HIGH` / `UNKNOWN` (실제 회전 측정 분류)
  - **지시와 측정을 별도 제공** — 합치면 불일치를 검출할 수 없다
- 온도 장치 방향: `COOL` / `HEAT` / `IDLE`
- 방열(열 제거 수단) 상태: `정상` / `과열 차단` / `측정 불가`

### Light

- 적용 중 알림 종류: `NORMAL` / `GOODBYE` / `WARNING` / `FAULT`
- 적용 결과: `적용됨` / `적용 실패` / `UNKNOWN`
  - 현재 구성은 **출력 지시 적용까지만 확인** — 실제 점등(단선·광량)은 확인하지 않으며, 적용 성공을 점등 확인으로 보고하지 않는다

---

## 3. Request / Command

Domain → BCM. **모든 명령에 명령 식별자(중복·역순 판별)와 신선도 근거가 포함**되어야 한다.

### Door

- `LOCK`
- `UNLOCK`

### Climate

- Fan 출력 수준 설정: `OFF` / `LOW` / `MEDIUM` / `HIGH`
- 온도 장치 설정: 방향(`COOL` / `HEAT` / `IDLE`) + 출력 수준(%) — `IDLE`의 출력은 0

### Light

- 조명 알림 설정: 알림 종류(4종) + 전체 밝기(0~100 %)
- 사용자 색상 설정: R/G/B 비율 — **`NORMAL`에만 적용**, 안전·상황 알림 색상은 사용자 설정으로 바꾸지 않음

### 받지 않는 것

- 센싱 값 (실내 온도·조도·탑승자 유무), 목표 온도 등 사용자 설정 원본
- 액추에이터의 구동량·구동 시간을 직접 지정하는 명령 (PWM 듀티 등) — 수용 거부

---

## 4. Event

- `LOCK_COMPLETED` / `UNLOCK_COMPLETED` — 목표 잠금 상태 도달을 측정으로 확인
- `ALREADY_AT_TARGET` — 이미 목표 상태여서 구동 없이 완료 (**오류·거부 아님**, 중앙 확인 이벤트에 연결)
- `REQUEST_REJECTED` + 사유 — `DOOR_OPEN` / `STATE_UNTRUSTED` / `CMD_INVALID`
- `REQUEST_FAILED` + 사유 — `NO_FEEDBACK` / `DRIVE_LIMIT_EXCEEDED`
- `OVERHEAT_DETECTED` — 과열 차단 발생
- `FAN_MISMATCH_DETECTED` — Fan 지시-측정 불일치가 허용 시간을 초과해 확정됨
- `RECOVERY_CONFIRMED` — 오류별 복구 조건 충족 확인 (이 이벤트만으로 재구동하지 않음, 새 명령 필요)

> 자체 안전 판단에 의한 거부(`DOOR_OPEN` 등)는 **정상 응답**이며 BCM Fault로 분류하지 않는다.

---

## 5. Fault

오류 분류는 3종으로 제공: `SENSOR` / `COMM` / `FUNCTION`

- `DOOR_SENSOR_FAULT` — 접점 판독 불가 (`UNKNOWN` 지속) — SENSOR
- `FAN_SENSOR_FAULT` — 회전 측정 신호 상실/신뢰 불가 — SENSOR
- `TEMPERATURE_SENSOR_FAULT` — 방열부 온도 측정 불가 — SENSOR
- `OVERHEAT_FAULT` — 방열부 차단 임계값 초과 — FUNCTION
- `FAN_FAULT` — 지시-측정 불일치 확정 — FUNCTION
- `LOCK_ACTUATOR_FAULT` — 재시도 후에도 목표 미도달 — FUNCTION
- `LIGHT_APPLY_FAULT` — 출력 지시 적용 실패 — FUNCTION
- `COMM_TIMEOUT_FAULT` — 주기 감시 대상의 연속 미수신 (3회) — COMM

각 Fault는 **정의된 복구 조건 충족 시에만 해소** 처리한다 (복구 확인 시간 1 s). 복구 후 구동은 **새 유효 명령으로만** 재개한다.

---

## 6. Data

### Data 1

```text
Name  : 온도 장치 출력 수준
Type  : uint8 (예상)
Range : 0 ~ 100
Unit  : %
Desc  : 현재 적용 중인 냉각/가열 출력. IDLE 이면 0.
        최대 출력은 방열 시험 후 확정 (100 은 후보값)
```

### Data 2

```text
Name  : 조명 전체 밝기
Type  : uint8 (예상)
Range : 0 ~ 100
Unit  : %
Desc  : 각 채널 켜짐 비율 = 색상 비율 × 전체 밝기 / 100.
        NORMAL 은 사용자·자동 밝기, 나머지 알림은 기본 100
```

### Data 3

```text
Name  : 조명 색상 (R / G / B)
Type  : uint8 × 3 (예상)
Range : 각 0 ~ 100
Unit  : %
Desc  : 알림별 기본 — NORMAL 100/100/100 · GOODBYE 0/70/70
        · WARNING 100/70/0 · FAULT 100/0/0. 사용자 지정은 NORMAL 만
```

### Data 4 (내부 측정 — 상태로 가공되어 제공)

```text
Name  : 방열부 온도
Type  : int16 (예상, 0.1 °C 단위)
Range : 센서 유효 범위 (NTC 10 kΩ · B 3950)
Unit  : °C
Desc  : 차단 80 / 복귀 60 °C 판정의 근거 (후보값).
        Invalid : 측정 신호 타임아웃 500 ms 초과 시 '측정 불가'
```

> 실내 온도·조도는 BCM 데이터가 아니다 (중앙이 별도 경로로 취득).

---

## 7. Local Logic

- **명령 유효성 평가** — 정의된 집합·범위 확인, 중복·역순 명령 미재실행, 신선도는 **사용 시점에** 재평가 (수신 시점 판정 재사용 금지)
- **도어 접점 안정화** — 상태 변화가 30 ms 유지될 때만 확정 (채터링 제거)
- **도어 제어 시퀀스** — LOCK 시 `CLOSED` 확인 → 구동 (상한 600 ms) → 800 ms 내 접점으로 목표 도달 확인 → 미도달 시 1회 재시도 → 실패 보고
- **동일 목표 반복 요청** — 액추에이터 구동 없이 `DONE` + `ALREADY_AT_TARGET`
- **Fan 지시-측정 대조** — 현재 적용 출력 기준, 불일치 허용 2 s 초과 시 오류
- **온도 장치 로컬 허용** — 방열 온도 + Fan 동작 확인이 함께 유효할 때만 구동. 확인 불가 시 출력 불허
- **방향 전환 보호** — 출력 0 → 500 ms 지연 → 최신 유효 목표·로컬 허용 재확인 후 반대 방향 구동 (이전 목표 자동 실행 금지)
- **과열 차단·복구** — 80 °C 초과 시 즉시 정지. 60 °C 이하 + 1 s 유지 + Fan 조건 확인 + **새 온도 명령** 이 모두 갖춰져야 재개
- **Fan 고장 복구 확인** — OFF 일치는 정지 근거일 뿐. OFF 아닌 새 Fan 명령이 있을 때만 제한된 회전 확인 수행, 자동 반복 시험 없음
- **조명 색상 변환** — 알림별 고정 색상 × 전체 밝기 → 채널 듀티. 핀 극성(LOW 점등) 변환은 BCM 내부 처리
- **기능 간 독립** — 한 기능의 오류가 다른 기능의 주기 처리를 차단하지 않음

---

## 8. Fail-safe

- **Domain 통신 두절 (연속 3회 미수신)**
  → Fan·온도 장치 출력 OFF (목표 신선도 상실)
  → 도어는 **현재 잠금 상태 유지**, 임의 해제 금지
  → 조명은 기존 유효 출력 유지
  → `COMM_TIMEOUT_FAULT` 제공, 복구 후 마지막 수신 내용으로 재구동하지 않음 — 새 명령 대기

- **방열 온도 측정 불가**
  → 온도 장치 출력 정지, 측정 불가 동안 재개 금지

- **과열 (차단 임계값 초과)**
  → 온도 장치 출력 즉시 정지, 복귀 조건 충족 + 새 명령 전까지 재개 금지

- **Fan Fault / 회전 확인 상실**
  → 온도 장치 출력 정지 (열 배출 보장 불가), Fan 정상 확인 + 새 온도 명령으로만 재개

- **잘못된 Request 수신** (집합 밖 · 범위 밖 · 신선도 상실)
  → 실행하지 않고 `REJECTED` + `CMD_INVALID`
  → **무관한 새 요청이 못 믿을 값이라는 이유만으로 기존 유효 동작을 멈추지 않음**

- **도어 구동 실패**
  → 1회 재시도 후 `FAILED` + 사유, 실제 상태를 `UNKNOWN`/실측값 그대로 보고 (그럴듯한 정상값으로 치환 금지)

- **조명 적용 실패**
  → 정상 적용으로 처리하지 않고 해당 출력 OFF 시도, OFF 확인도 불가하면 `UNKNOWN`

- **ECU 기동 / 내부 오류**
  → 전 출력 OFF 로 시작, 이전 동작 미복원
  → 구동 시간 상한은 **소프트웨어가 멈춘 상태에서도** 동작을 정지시킬 수 있는 경로로 구현 (안전 감시 주기 10 ms)
