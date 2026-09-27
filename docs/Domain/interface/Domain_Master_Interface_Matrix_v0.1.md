# Domain Master Interface Matrix

- 문서 버전: v0.1
- 상태: Logical Interface Freeze Review Candidate
- 작성일: 2026-09-24
- 기준 문서:
  - `Domain_Interface_Definition_v0.8.md`
  - `MOBILE_ESP32_Interface_Definition_v0.1.md`
  - ECU 담당자 회신이 반영된 BCM / CIS / WINDOW / VSS 논리 Interface
- 범위:
  - `MOBILE ↔ ESP32 ↔ S32K344 Domain`
  - `Domain ↔ BCM / CIS / WINDOW / VSS`
  - `EXTERIOR_LIGHT`는 현재 `OUT_OF_SCOPE_CANDIDATE`

> 이 문서는 **논리 Interface**를 Freeze하기 위한 Master Matrix다.
> CAN ID, UART Frame, Byte/Bit, Endianness, 실제 자료형, Cycle, Timeout, CRC/E2E 등 물리/네트워크 상세값은 포함하지 않는다.

---

# 1. 전체 구조

```text
MOBILE APP
    ↕ Bluetooth
ESP32
    ↕ UART
S32K344 Domain
    ├─ BCM
    ├─ CIS
    ├─ WINDOW
    └─ VSS
```

현재 역할 경계:

```text
MOBILE
→ 사용자 Request 생성 / 상태·결과·경고 표시

ESP32
→ Bluetooth 등록·현재 연결 관리
→ RSSI 기반 NEAR/FAR/UNKNOWN
→ MOBILE ↔ Domain Gateway

Domain
→ Request 검증
→ 차량 수준 정책/중재
→ 실행 ECU Command 생성
→ ECU Result를 원 Request에 연결
→ Vehicle State / Warning / Availability 생성

BCM / WINDOW
→ 실제 액추에이터 실행 + Local Safety

CIS
→ 센싱 / 필터링 / 품질 제공

VSS
→ Semantic Event / Warning State를 실제 음향으로 표현
```

---

# 2. End-to-End Interface 대조

## 2.1 수동 Door LOCK / UNLOCK

```text
MOBILE
MOBILE_REQUEST
- REQUEST_ID
- Door LOCK / UNLOCK
        ↓ Bluetooth
ESP32
- 의미 변경 없이 중계
- Session / Request 연계 유지
        ↓ UART
Domain
- Registration / Connection / Session 검증
- Request 허용 판단
        ↓
BCM
DOOR_LOCK_TARGET
COMMAND_ID
COMMAND_VALIDITY_INFO
        ↓
BCM
DOOR_LOCK_STATE / DOOR_OPEN_STATE
BCM_COMMAND_RESULT
LOCK_COMPLETED / UNLOCK_COMPLETED
또는 REJECTED / FAILED
        ↓
Domain
- 원 REQUEST_ID와 실행 결과 연결
- DONE / REJECTED / FAILED 등 공통 Result 생성
- 필요 시 DOOR_LOCK_COMPLETE / DOOR_UNLOCK_COMPLETE 생성
        ├─→ VSS
        │    Semantic Event
        │
        └─→ ESP32 → MOBILE
             REQUEST_RESULT
             VEHICLE_STATE_UPDATE
```

**대조 결과: 연결 완료.**

---

## 2.2 Climate 사용자 요청

```text
MOBILE
- 목표 온도
- AUTO ON/OFF
- Fan OFF/LOW/MEDIUM/HIGH
        ↓
ESP32
        ↓
Domain
        ├─ CIS
        │   CABIN_TEMPERATURE
        │   CABIN_HUMIDITY
        │   OCCUPANT_PRESENCE
        │   + Quality
        │
        ↓
Domain Climate 판단 / 중재
        ↓
BCM
FAN_TARGET_LEVEL
THERMAL_DIRECTION
THERMAL_TARGET_LEVEL
        ↓
BCM
FAN_COMMAND_LEVEL
FAN_MEASURED_LEVEL
THERMAL_DIRECTION_STATE
THERMAL_OUTPUT_LEVEL
BCM_COMMAND_RESULT / Fault
        ↓
Domain
        ↓
ESP32
        ↓
MOBILE
REQUEST_RESULT + VEHICLE_STATE_UPDATE
```

**대조 결과: 연결 완료.**

MOBILE의 목표 온도와 BCM의 실제 Fan/Thermal Command는 같은 정보가 아니다.
Domain이 사용자 설정과 CIS/BCM 상태를 바탕으로 최종 실행 목표를 만든다.

---

## 2.3 Interior Light 사용자 요청

```text
MOBILE
- ON/OFF
- Brightness
- NORMAL RGB
        ↓
ESP32
        ↓
Domain
- 차량 상태 / 알림 상태와 중재
        ↓
BCM
INTERIOR_LIGHT_TYPE
INTERIOR_LIGHT_LEVEL
INTERIOR_LIGHT_COLOR
        ↓
BCM
INTERIOR_LIGHT_*_STATE
INTERIOR_LIGHT_APPLY_RESULT
LIGHT_APPLY_FAULT
        ↓
Domain → ESP32 → MOBILE
```

**대조 결과: 연결 완료.**

현재 물리 점등 여부 Feedback은 지원하지 않으므로 `적용 성공`과 `실제 빛이 켜졌음`을 같은 의미로 표시하면 안 된다.

---

## 2.4 Digital Key 자동 Unlock

```text
MOBILE
DIGITAL_KEY ON/OFF
APP_ACTIVE_STATE
        ↓
ESP32
- Registered?
- Bluetooth Connected?
- RSSI 평가
- NEAR / FAR / UNKNOWN
- Quality / Freshness
        ↓
Domain DigitalKeyManager
- 반영 Setting ON?
- 등록된 현재 연결?
- 유효 Session?
- App Active?
- 유효 FAR → NEAR 새 접근?
- Door LOCKED?
- 실행 가능?
        ↓
BCM
DOOR_LOCK_TARGET = UNLOCK
        ↓
BCM Result + Door State
        ↓
Domain
DIGITAL_KEY_RESULT
Unlock Origin = PROXIMITY_AUTO
        ↓
ESP32 → MOBILE
```

**대조 결과: 연결 완료.**

중요:

```text
RSSI → NEAR/FAR 판단 = ESP32
자동 Unlock 최종 판단 = Domain
실제 Unlock 실행 = BCM
표시 = MOBILE
```

---

## 2.5 CIS 환경 상태 → MOBILE 표시

```text
CIS
CABIN_TEMPERATURE / HUMIDITY / ILLUMINANCE
OCCUPANT_PRESENCE / COUNT
+ VALIDITY / QUALITY_REASON / AGE
        ↓
Domain VehicleStateManager
        ↓
VEHICLE_STATE_UPDATE
        ↓
ESP32
        ↓
MOBILE
```

**대조 결과: 연결 완료.**

ESP32/MOBILE이 센서 Raw 의미를 재판정하지 않는다.

---

## 2.6 Rear Obstacle Warning

```text
CIS
REAR_DISTANCE
PROXIMITY_STATUS
QUALITY / AGE
        ↓
Domain
CLEAR / CAUTION / EMERGENCY 판단
        ├─→ VSS
        │   REAR_OBSTACLE_STATE
        │   REAR_DETECTION_ACTIVATION
        │
        └─→ ESP32
            WARNING_INFO / VEHICLE_STATE_UPDATE
                 ↓
               MOBILE
```

**대조 결과: 연결 완료.**

`NO_OBJECT`와 `UNAVAILABLE`은 반드시 구분하며, 측정 불가를 `CLEAR`로 바꾸지 않는다.

---

## 2.7 Window Anti-Pinch

```text
WINDOW
WINDOW_ANTIPINCH
ANTIPINCH_PROTECTION_STATUS
WINDOW_COMMAND_RESULT
        ↓
Domain
- 원 Request 연결
- WINDOW_ANTIPINCH_STATE 생성
- Result = FAILED + ANTIPINCH
        ├─→ VSS
        │   WINDOW_ANTIPINCH_STATE
        │
        └─→ ESP32
            REQUEST_RESULT / WARNING_INFO
                 ↓
               MOBILE
```

**대조 결과: 연결 완료.**

WINDOW 내부 우선순위는:

```text
Anti-Pinch > Local Switch > Domain
```

이다.

---

## 2.8 Request Result Lifecycle

```text
MOBILE
SENT
 ↓
Domain
ACCEPTED
 ↓
실행 ECU
IN_PROGRESS
 ↓
DONE / REJECTED / CANCELLED / FAILED
 ↓
Domain ResultManager
 ↓
ESP32
 ↓
MOBILE
```

공통 차량 Result:

```text
ACCEPTED
IN_PROGRESS
DONE
REJECTED
CANCELLED
FAILED
```

`UNKNOWN`은 차량 실행 Result가 아니다.

```text
결과를 확인할 수 없음
→ RESULT_CONFIRMATION = UNCONFIRMED
→ MOBILE 내부 표시 = UNKNOWN
```

---

## 2.9 연결 상실 / 재연결

```text
Bluetooth Lost
→ 새 MOBILE Request 차단
→ Digital Key 새 Unlock 차단

UART / Domain Link Lost
→ ESP32가 ACCEPTED/DONE/FAILED 등을 자체 생성하지 않음

Reconnect
→ Registration / Session 재확인
→ 현재 Vehicle State 재동기화
→ 과거 미완료 Request 자동 재실행 금지
```

**대조 결과: MOBILE ↔ ESP32 ↔ Domain 규칙이 일관됨.**

---

# 3. End-to-End 대조 결과

| 항목 | 결과 | 비고 |
|---|---|---|
| MOBILE Request → Domain | PASS | ESP32는 Gateway 역할만 수행 |
| Domain Request → BCM | PASS | 사용자 Request와 ECU Command 분리 |
| Domain Request → WINDOW | PASS | 현재 MOBILE Window 제어는 범위 외 |
| CIS State → Domain → MOBILE | PASS | Quality/Freshness 유지 필요 |
| CIS Rear Distance → Domain → VSS | PASS | Domain이 위험 의미 결정 |
| BCM Result → Domain → MOBILE | PASS | 공통 Result로 정규화 |
| BCM Door Result → Domain → VSS | PASS | Local Event와 Vehicle Event 구분 |
| WINDOW Anti-Pinch → Domain → VSS/MOBILE | PASS | Event/State/Result 분리 |
| VSS State/Fault → Domain → MOBILE | PASS | Domain이 앱 표시용으로 연결 |
| Digital Key | PASS | ESP32 근접 판단 / Domain 최종 결정 / BCM 실행 |
| Result UNKNOWN 처리 | PASS | MOBILE 표시 상태로만 유지 |
| Reconnect / Replay | PASS | 자동 재실행 금지 |
| MOBILE → WINDOW 제어 | OUT OF SCOPE | 현재 MOBILE Interface에 Request 없음 |
| EXTERIOR_LIGHT | OUT_OF_SCOPE_CANDIDATE | 팀 기능 삭제 결정 대기 |

현재 발견된 **논리 Interface 단절은 없다.**

---

# 4. Domain Master Interface Matrix

## 4.1 ESP32 ↔ Domain

| ID | Direction | Interface | Producer | Consumer | Type | Meaning / Values | Domain Usage | Logical Status | Remaining Detail |
|---|---|---|---|---|---|---|---|---|---|
| E2D-001 | ESP32 → Domain | DEVICE_REGISTRATION_STATE | ESP32 | Domain | State | REGISTERED / NOT_REGISTERED / UNKNOWN | 등록된 단말 여부 확인 | CONFIRMED | Encoding TBD |
| E2D-002 | ESP32 → Domain | BT_CONNECTION_STATE | ESP32 | Domain | State | CONNECTED / DISCONNECTED / UNKNOWN | 현재 Bluetooth 연결 확인 | CONFIRMED | Encoding TBD |
| E2D-003 | ESP32 → Domain | DEVICE_CONTEXT_ID | ESP32 | Domain | Meta | 현재 등록·연결 단말 식별 근거 | 요청 출처/연결 문맥 연결 | CONFIRMED | 실제 표현 TBD |
| E2D-004 | ESP32 → Domain | SESSION_ID | ESP32/MOBILE Context | Domain | Meta | 현재 Session 식별 | 재연결 전후/과거 요청 구분 | CONFIRMED | 생성 주체·폭·수명 TBD |
| E2D-005 | ESP32 → Domain | APP_ACTIVE_STATE | MOBILE→ESP32 | Domain | State | ACTIVE / INACTIVE / UNKNOWN | Digital Key 활성 조건 | CONFIRMED | 확인 방법 TBD |
| E2D-006 | ESP32 → Domain | PROXIMITY_STATE | ESP32 | Domain | State | NEAR / FAR / UNKNOWN | Digital Key 새 접근 판단 | CONFIRMED | RSSI 임계값 TBD |
| E2D-007 | ESP32 → Domain | PROXIMITY_QUALITY | ESP32 | Domain | Quality | 근접 정보 사용 가능 여부 | 근접값 사용/차단 판단 | CONFIRMED | Enum/Encoding TBD |
| E2D-008 | ESP32 → Domain | PROXIMITY_UPDATE_INFO | ESP32 | Domain | Meta | 새 평가·Ordering·Age/Freshness 근거 | STALE/과거/중복 판단 | CONFIRMED | 표현 TBD |
| E2D-009 | ESP32 → Domain | MOBILE_REQUEST | MOBILE via ESP32 | Domain | Request | Door / Climate / Interior Light / Digital Key Setting | 사용자 요청 검증 및 차량 명령 변환 | CONFIRMED | Wire format TBD |
| E2D-010 | ESP32 → Domain | REQUEST_ID | MOBILE via ESP32 | Domain | Meta | 동일 요청 식별 | 중복 방지/Result 연결 | CONFIRMED | 폭/rollover TBD |
| E2D-011 | ESP32 → Domain | STATE_QUERY | MOBILE via ESP32 | Domain | Query | 현재 State / Request Result / Warning 조회 | 조회 응답 생성 | CONFIRMED | Message format TBD |
| E2D-012 | ESP32 → Domain | WARNING_ACK | MOBILE via ESP32 | Domain | Ack/Event | 사용자의 Warning 확인 | READ 상태 갱신; CLEAR와 구분 | CONFIRMED | Occurrence 연결 방식 TBD |
| D2E-001 | Domain → ESP32 | REQUEST_RESULT | Domain | ESP32 | Result | ACCEPTED / IN_PROGRESS / DONE / REJECTED / CANCELLED / FAILED | MOBILE 원 요청 결과 제공 | CONFIRMED | Encoding TBD |
| D2E-002 | Domain → ESP32 | RESULT_REASON | Domain | ESP32 | Meta/Data | 거부·취소·실패 사유 | MOBILE 표시/진단 | CONFIRMED | Reason Code TBD |
| D2E-003 | Domain → ESP32 | RESULT_CONFIRMATION | Domain | ESP32 | Quality/Meta | CONFIRMED / UNCONFIRMED 성격 | MOBILE UNKNOWN 표시 근거 | CONFIRMED | Encoding TBD |
| D2E-004 | Domain → ESP32 | VEHICLE_STATE_UPDATE | Domain | ESP32 | State/Data | 앱 표시 대상 차량 상태 + Quality/Freshness | MOBILE 표시 | CONFIRMED | Message 분할 TBD |
| D2E-005 | Domain → ESP32 | WARNING_INFO | Domain | ESP32 | State/Event | Warning Type/Occurrence/Severity/Quality/Freshness | MOBILE 경고 표시 | CONFIRMED | Warning code TBD |
| D2E-006 | Domain → ESP32 | FUNCTION_AVAILABILITY | Domain | ESP32 | Availability | 기능 사용 가능/제한/불가 + Reason | MOBILE UI 기능 가용성 | CONFIRMED | 공통 Enum TBD |
| D2E-007 | Domain → ESP32 | DIGITAL_KEY_SETTING_STATE | Domain | ESP32 | State | Domain이 반영한 자동 Unlock 설정 | 앱 설정 표시 | CONFIRMED | Encoding TBD |
| D2E-008 | Domain → ESP32 | DIGITAL_KEY_AVAILABILITY | Domain | ESP32 | Availability | 현재 Digital Key 사용 가능 여부 | 앱 기능 활성/비활성 표시 | CONFIRMED | Reason/Enum TBD |
| D2E-009 | Domain → ESP32 | DIGITAL_KEY_RESULT | Domain | ESP32 | Result/Event | 자동 Unlock 판단/실행 결과 + Origin | 앱 결과 표시 | CONFIRMED | 관련 Door Result 연결 표현 TBD |

---

## 4.2 BCM ↔ Domain

| ID | Direction | Interface | Producer | Consumer | Type | Meaning / Values | Domain Usage | Logical Status | Remaining Detail |
|---|---|---|---|---|---|---|---|---|---|
| D2B-001 | Domain → BCM | DOOR_LOCK_TARGET | Domain | BCM | Command | LOCK / UNLOCK | Domain 확정 도어 목표 | CONFIRMED | Network mapping TBD |
| D2B-002 | Domain → BCM | COMMAND_ID | Domain | BCM | Meta | 모든 BCM 명령 식별 | 중복·역순·재전송 방지 | CONFIRMED | 폭/Frame 위치 TBD |
| D2B-003 | Domain → BCM | FAN_TARGET_LEVEL | Domain | BCM | Command | OFF / LOW / MEDIUM / HIGH | Fan 출력 목표 | CONFIRMED | Network mapping TBD |
| D2B-004 | Domain → BCM | THERMAL_DIRECTION | Domain | BCM | Command | COOL / HEAT / IDLE | 온도 장치 방향 | CONFIRMED | Network mapping TBD |
| D2B-005 | Domain → BCM | THERMAL_TARGET_LEVEL | Domain | BCM | Command/Data | 0~100% 논리 수준 | 온도 출력 목표 | CONFIRMED | Scaling/Encoding TBD |
| D2B-006 | Domain → BCM | INTERIOR_LIGHT_TYPE | Domain | BCM | Command | NORMAL / GOODBYE / WARNING / FAULT | 실내 조명 의미 선택 | CONFIRMED | Encoding TBD |
| D2B-007 | Domain → BCM | INTERIOR_LIGHT_LEVEL | Domain | BCM | Command/Data | 0~100% | 실내 조명 밝기 목표 | CONFIRMED | Scaling TBD |
| D2B-008 | Domain → BCM | INTERIOR_LIGHT_COLOR | Domain | BCM | Command/Data | R/G/B 비율 | NORMAL 색상 적용 | CONFIRMED | Encoding TBD |
| D2B-009 | Domain → BCM | COMMAND_VALIDITY_INFO | Domain | BCM | Quality/Meta | 명령 신선도·수용 가능 근거 | 오래된/무효 명령 차단 | CONFIRMED | 시간 표현 TBD |
| B2D-001 | BCM → Domain | BCM_ECU_STATE | BCM | Domain | State/Availability | INIT / READY / DEGRADED / FAULT | BCM 가용성 관리 | CONFIRMED | Encoding TBD |
| B2D-002 | BCM → Domain | DOOR_LOCK_STATE | BCM | Domain | State | LOCKED / UNLOCKED / UNKNOWN | 차량 Door 상태/결과 판단 | CONFIRMED | Network mapping TBD |
| B2D-003 | BCM → Domain | DOOR_OPEN_STATE | BCM | Domain | State | CLOSED / OPEN / UNKNOWN | Lock 허용/상태 표시 | CONFIRMED | Network mapping TBD |
| B2D-004 | BCM → Domain | DOOR_COMPOSITE_STATE | BCM | Domain | State | NORMAL / INCONSISTENT / UNTRUSTED | 도어 상태 신뢰성 판단 | CONFIRMED | Encoding TBD |
| B2D-005 | BCM → Domain | FAN_COMMAND_LEVEL | BCM | Domain | State | OFF / LOW / MEDIUM / HIGH | 현재 BCM 지시 수준 | CONFIRMED | Network mapping TBD |
| B2D-006 | BCM → Domain | FAN_MEASURED_LEVEL | BCM | Domain | State | OFF / LOW / MEDIUM / HIGH / UNKNOWN | 실제 Fan 동작 확인 | CONFIRMED | Network mapping TBD |
| B2D-007 | BCM → Domain | THERMAL_DIRECTION_STATE | BCM | Domain | State | COOL / HEAT / IDLE | 현재 온도 장치 방향 | CONFIRMED | Network mapping TBD |
| B2D-008 | BCM → Domain | THERMAL_OUTPUT_LEVEL | BCM | Domain | Data/State | 현재 적용 출력 수준 | 공조 상태 표시/판단 | CONFIRMED | Scaling TBD |
| B2D-009 | BCM → Domain | HEAT_REMOVAL_STATE | BCM | Domain | State | 정상 / 과열 차단 / 측정 불가 | 방열 보호 상태 | CONFIRMED | Enum TBD |
| B2D-010 | BCM → Domain | INTERIOR_LIGHT_TYPE_STATE | BCM | Domain | State | 현재 조명 알림 종류 | 앱 표시/상태 관리 | CONFIRMED | Encoding TBD |
| B2D-011 | BCM → Domain | INTERIOR_LIGHT_LEVEL_STATE | BCM | Domain | Data/State | 현재 적용 밝기 | 앱 표시 | CONFIRMED | Scaling TBD |
| B2D-012 | BCM → Domain | INTERIOR_LIGHT_COLOR_STATE | BCM | Domain | Data/State | 현재 적용 R/G/B | 앱 표시 | CONFIRMED | Encoding TBD |
| B2D-013 | BCM → Domain | INTERIOR_LIGHT_APPLY_RESULT | BCM | Domain | State/Result | 적용됨 / 실패 / UNKNOWN | 조명 결과 판단 | CONFIRMED | Enum TBD |
| B2D-014 | BCM → Domain | LIGHT_PHYSICAL_FEEDBACK_CAPABILITY | BCM | Domain | Availability | 실제 점등 확인 지원 여부 | 앱/Domain이 적용과 실제 점등을 구분 | CONFIRMED | 현재 물리 점등 확인 미지원 |
| B2D-015 | BCM → Domain | VALUE_QUALITY | BCM | Domain | Quality | OK / STALE / INVALID / NO_DATA | 각 State/Data 사용 가능 여부 | CONFIRMED | 실제 표현 TBD |
| B2D-016 | BCM → Domain | BCM_COMMAND_RESULT | BCM | Domain | Result | 원 명령 처리 결과 | 원 MOBILE Request와 연결 | CONFIRMED | Domain 공통 Result로 정규화 |
| B2D-017 | BCM → Domain | BCM_RESULT_REASON | BCM | Domain | Meta/Data | 거부·실패 사유 | ResultManager/MOBILE 표시 | CONFIRMED | Code TBD |
| B2D-018 | BCM → Domain | LOCK_COMPLETED | BCM | Domain | Event | 목표 잠금 상태 도달 | DOOR_LOCK_COMPLETE 생성 근거 | CONFIRMED | Occurrence encoding TBD |
| B2D-019 | BCM → Domain | UNLOCK_COMPLETED | BCM | Domain | Event | 목표 잠금 해제 도달 | DOOR_UNLOCK_COMPLETE 생성 근거 | CONFIRMED | Occurrence encoding TBD |
| B2D-020 | BCM → Domain | ALREADY_AT_TARGET | BCM | Domain | Event/Result | 추가 구동 없이 목표 상태 | DONE + Reason 연결 | CONFIRMED | Reason mapping TBD |
| B2D-021 | BCM → Domain | REQUEST_REJECTED | BCM | Domain | Result/Event | 실행 전 거부 | REJECTED + Reason | CONFIRMED | Reason: DOOR_OPEN/STATE_UNTRUSTED/CMD_INVALID |
| B2D-022 | BCM → Domain | REQUEST_FAILED | BCM | Domain | Result/Event | 실행/목표 확인 실패 | FAILED + Reason | CONFIRMED | Reason: NO_FEEDBACK/DRIVE_LIMIT_EXCEEDED 등 |
| B2D-023 | BCM → Domain | OVERHEAT_DETECTED | BCM | Domain | Event | 과열 차단 발생 | 공조 보호/Warning 판단 | CONFIRMED | Event encoding TBD |
| B2D-024 | BCM → Domain | FAN_MISMATCH_DETECTED | BCM | Domain | Event | Fan 지시-측정 불일치 확정 | 공조 오류 판단 | CONFIRMED | Event encoding TBD |
| B2D-025 | BCM → Domain | RECOVERY_CONFIRMED | BCM | Domain | Event | 오류 복구 조건 충족 | 복구 상태 갱신; 자동 재구동 금지 | CONFIRMED | Event encoding TBD |
| B2D-026 | BCM → Domain | DOOR_SENSOR_FAULT | BCM | Domain | Fault | SENSOR: 도어 접점 판독 불가 | Fault/Availability/표시 반영 | CONFIRMED | Fault encoding/Timeout 상세 TBD |
| B2D-027 | BCM → Domain | FAN_SENSOR_FAULT | BCM | Domain | Fault | SENSOR: Fan 회전 측정 신뢰 불가 | Fault/Availability/표시 반영 | CONFIRMED | Fault encoding/Timeout 상세 TBD |
| B2D-028 | BCM → Domain | TEMPERATURE_SENSOR_FAULT | BCM | Domain | Fault | SENSOR: 방열부 온도 측정 불가 | Fault/Availability/표시 반영 | CONFIRMED | Fault encoding/Timeout 상세 TBD |
| B2D-029 | BCM → Domain | OVERHEAT_FAULT | BCM | Domain | Fault | FUNCTION: 방열부 차단 임계 초과; 80°C/60°C는 후보값 | Fault/Availability/표시 반영 | CONFIRMED | Fault encoding/Timeout 상세 TBD |
| B2D-030 | BCM → Domain | FAN_FAULT | BCM | Domain | Fault | FUNCTION: Fan 지시-측정 불일치 확정; 2s는 현재 기준 | Fault/Availability/표시 반영 | CONFIRMED | Fault encoding/Timeout 상세 TBD |
| B2D-031 | BCM → Domain | LOCK_ACTUATOR_FAULT | BCM | Domain | Fault | FUNCTION: 목표 잠금 상태 도달 실패 | Fault/Availability/표시 반영 | CONFIRMED | Fault encoding/Timeout 상세 TBD |
| B2D-032 | BCM → Domain | LIGHT_APPLY_FAULT | BCM | Domain | Fault | FUNCTION: 조명 출력 지시 적용 실패 | Fault/Availability/표시 반영 | CONFIRMED | Fault encoding/Timeout 상세 TBD |
| B2D-033 | BCM → Domain | COMM_TIMEOUT_FAULT | BCM | Domain | Fault | COMM: BCM 감시 대상 통신 상실 | Fault/Availability/표시 반영 | CONFIRMED | Fault encoding/Timeout 상세 TBD |

---

## 4.3 CIS ↔ Domain

| ID | Direction | Interface | Producer | Consumer | Type | Meaning / Values | Domain Usage | Logical Status | Remaining Detail |
|---|---|---|---|---|---|---|---|---|---|
| D2C-001 | Domain → CIS | VEHICLE_POWER_PERMISSION | Domain | CIS | State/Permission | 전원 허용 상태 | CIS 센싱 동작 허용 | CONFIRMED | Encoding TBD |
| D2C-002 | Domain → CIS | VEHICLE_POWER_PERMISSION_QUALITY | Domain | CIS | Quality | 허용/불허/확인 불가 품질 | 전원 상태 신뢰성 확인 | CONFIRMED | Encoding TBD |
| C2D-001 | CIS → Domain | OCCUPANT_PRESENCE | CIS | Domain | State/Data | 탑승자 존재 여부 | 잔류 탑승자/공조/상태 표시 | CONFIRMED | Network mapping TBD |
| C2D-002 | CIS → Domain | OCCUPANT_COUNT | CIS | Domain | Data | 0~5명 | 탑승자 상태/표시 | CONFIRMED | Network mapping TBD |
| C2D-003 | CIS → Domain | CABIN_TEMPERATURE | CIS | Domain | Data | 실내 온도 | 공조 판단/앱 표시 | CONFIRMED | Scaling TBD |
| C2D-004 | CIS → Domain | CABIN_HUMIDITY | CIS | Domain | Data | 실내 습도 | 공조/자동 환기 판단 | CONFIRMED | Scaling TBD |
| C2D-005 | CIS → Domain | CABIN_ILLUMINANCE | CIS | Domain | Data | 실내 조도 | 차량 정책/표시 | CONFIRMED | Scaling TBD |
| C2D-006 | CIS → Domain | REAR_DISTANCE | CIS | Domain | Data | cm 단위 후방 거리 | Domain 후방 위험 판단 | CONFIRMED | 최종 유효 범위 TBD; 10~100cm 후보 |
| C2D-007 | CIS → Domain | PROXIMITY_STATUS | CIS | Domain | State | VALID_DISTANCE / NO_OBJECT / UNAVAILABLE / FAULT / RECOVERING 성격 | NO_OBJECT와 측정불가 구분 | CONFIRMED | 최종 enum TBD |
| C2D-008 | CIS → Domain | CIS_STATE | CIS | Domain | State/Availability | STARTUP / READY / ACTIVE / FAULT | CIS 전체 가용성 | CONFIRMED | Encoding TBD |
| C2D-009 | CIS → Domain | FUNCTION_STATUS | CIS | Domain | State/Availability | 기능별 준비·오류·복구 상태 | 부분 Fault 격리 | CONFIRMED | 세부 enum TBD |
| C2D-010 | CIS → Domain | INTERFACE_STATUS | CIS | Domain | Availability/Fault | 제공 경로 가용성·통신 이상 | 센서 품질과 통신 상태 분리 | CONFIRMED | Mapping TBD |
| C2D-011 | CIS → Domain | VALIDITY | CIS | Domain | Quality | VALID / INVALID | 각 관측값 사용 여부 | CONFIRMED | Encoding TBD |
| C2D-012 | CIS → Domain | QUALITY_REASON | CIS | Domain | Quality/Meta | NOT_READY / OUT_OF_RANGE / SENSOR_FAULT / VISION_FAULT / STALE / NO_DATA 등 | 무효 사유 보존 | CONFIRMED | Code TBD |
| C2D-013 | CIS → Domain | SOURCE_TIMESTAMP_OR_AGE | CIS | Domain | Meta | 원본 생성 시점/경과 시간 근거 | Freshness 평가 | CONFIRMED | 표현 TBD |
| C2D-014 | CIS → Domain | UPDATE_SEQUENCE | CIS | Domain | Meta | 새 결과/중복/과거 결과 구분 | Ordering 관리 | CONFIRMED | 폭/rollover TBD |
| C2D-015 | CIS → Domain | INITIALIZATION_FAILURE | CIS | Domain | Fault/State/Meta | 공통 처리·제공 기반 초기화 실패 | Fault/Quality/Availability 반영 | CONFIRMED | Encoding TBD |
| C2D-016 | CIS → Domain | VISION_FAULT | CIS | Domain | Fault/State/Meta | 탑승자 존재/인원수 기능 오류 | Fault/Quality/Availability 반영 | CONFIRMED | Encoding TBD |
| C2D-017 | CIS → Domain | TEMPERATURE_SENSOR_FAULT | CIS | Domain | Fault/State/Meta | 온도 기능 오류 | Fault/Quality/Availability 반영 | CONFIRMED | Encoding TBD |
| C2D-018 | CIS → Domain | HUMIDITY_SENSOR_FAULT | CIS | Domain | Fault/State/Meta | 습도 기능 오류 | Fault/Quality/Availability 반영 | CONFIRMED | Encoding TBD |
| C2D-019 | CIS → Domain | ILLUMINANCE_SENSOR_FAULT | CIS | Domain | Fault/State/Meta | 조도 기능 오류 | Fault/Quality/Availability 반영 | CONFIRMED | Encoding TBD |
| C2D-020 | CIS → Domain | PROXIMITY_SENSOR_FAULT | CIS | Domain | Fault/State/Meta | 후방 거리 기능 오류 | Fault/Quality/Availability 반영 | CONFIRMED | Encoding TBD |
| C2D-021 | CIS → Domain | COMMUNICATION_FAULT | CIS | Domain | Fault/State/Meta | 영향 받는 제공 경로 통신 오류 | Fault/Quality/Availability 반영 | CONFIRMED | 전체 상태 mapping TBD |
| C2D-022 | CIS → Domain | DATA_INVALID | CIS | Domain | Fault/State/Meta | 해당 값 정상 사용 불가 | Fault/Quality/Availability 반영 | CONFIRMED | Quality/Fault encoding TBD |
| C2D-023 | CIS → Domain | OUT_OF_RANGE | CIS | Domain | Fault/State/Meta | 값이 유효 범위를 벗어남 | Fault/Quality/Availability 반영 | CONFIRMED | Range 최종값 일부 TBD |
| C2D-024 | CIS → Domain | ACTIVE_FAULT | CIS | Domain | Fault/State/Meta | 현재 제공/판정을 방해하는 오류 | Fault/Quality/Availability 반영 | CONFIRMED | Encoding TBD |
| C2D-025 | CIS → Domain | RECOVERING | CIS | Domain | Fault/State/Meta | 복구 중이며 새 유효 결과 미확인 | Fault/Quality/Availability 반영 | CONFIRMED | Encoding TBD |
| C2D-026 | CIS → Domain | LAST_FAULT | CIS | Domain | Fault/State/Meta | 최근 주요 오류 이력 | Fault/Quality/Availability 반영 | CONFIRMED | Code TBD |
| C2D-027 | CIS → Domain | AFFECTED_FUNCTION | CIS | Domain | Fault/State/Meta | Fault 영향 기능/값/경로 | Fault/Quality/Availability 반영 | CONFIRMED | Encoding TBD |

---

## 4.4 WINDOW ↔ Domain

| ID | Direction | Interface | Producer | Consumer | Type | Meaning / Values | Domain Usage | Logical Status | Remaining Detail |
|---|---|---|---|---|---|---|---|---|---|
| D2W-001 | Domain → WINDOW | WINDOW_COMMAND | Domain | WINDOW | Request/Command | OPEN / CLOSE / STOP / VENT / MOVE_TO_POSITION | 확정 창문 동작 요청 | CONFIRMED | Network mapping TBD |
| D2W-002 | Domain → WINDOW | TARGET_CHANNEL | Domain | WINDOW | Meta | 현재 1채널, 확장 가능 | 대상 Window 선택 | CONFIRMED | Channel encoding TBD |
| D2W-003 | Domain → WINDOW | REQUEST_SEQUENCE | Domain | WINDOW | Meta | 중복·역순·결과 연결 식별 | 중복 실행 방지/Result 연결 | CONFIRMED | 폭 TBD |
| D2W-004 | Domain → WINDOW | REQUEST_VALIDITY | Domain | WINDOW | Quality/Meta | 요청 유효성/만료 근거 | 오래된 명령 차단 | CONFIRMED | 표현 TBD |
| D2W-005 | Domain → WINDOW | SOURCE_CONTEXT | Domain | WINDOW | Meta | 상위 요청 출처/문맥 | Local/Domain/자동 기능 추적 | CONFIRMED | Encoding TBD |
| D2W-006 | Domain → WINDOW | TARGET_POSITION | Domain | WINDOW | Data | VENT/MOVE 목표 위치; VENT 기본 70% Closed | 목표 위치 전달 | CONFIRMED | 위치 scaling/향후 변경 구조 |
| D2W-007 | Domain → WINDOW | POWER_OPERATION_STATE | Domain | WINDOW | State/Permission | 창문 이동 허용 전원/운전 상태 | 상위 실행 허용 | CONFIRMED | Encoding TBD |
| D2W-008 | Domain → WINDOW | OPERATION_ALLOWED | Domain | WINDOW | State/Permission | 현재 이동 허용 여부 | Domain 명령 허용 | CONFIRMED | Encoding TBD |
| D2W-009 | Domain → WINDOW | DOMAIN_ALIVE/FRESHNESS_BASIS | Domain | WINDOW | Availability/Meta | Domain 통신 생존/최근 유효 수신 근거 | 메시지 상실 시 Domain 명령 비활성화 | CONFIRMED | 주기/Timeout/구현 방식 TBD |
| W2D-001 | WINDOW → Domain | WINDOW_STATE | WINDOW | Domain | State | 실제 이동 상태 + ECU 상태 + 품질 | VehicleStateManager | CONFIRMED | Network mapping TBD |
| W2D-002 | WINDOW → Domain | MOTION_STATE | WINDOW | Domain | State | STOPPED / OPENING / CLOSING / ANTIPINCH_REVERSING / UNKNOWN | 현재 동작 상태 | CONFIRMED | Encoding TBD |
| W2D-003 | WINDOW → Domain | WINDOW_ECU_STATE | WINDOW | Domain | State/Availability | INIT / READY / DEGRADED / FAULT | Window 기능 가용성 | CONFIRMED | Encoding TBD |
| W2D-004 | WINDOW → Domain | WINDOW_POSITION | WINDOW | Domain | Data | 0%=Fully Open, 100%=Fully Closed | 상태/자동 환기 판단 | CONFIRMED | Scaling/허용오차 TBD |
| W2D-005 | WINDOW → Domain | WINDOW_VALUE_QUALITY | WINDOW | Domain | Quality | OK / STALE / INVALID / NO_DATA | 상태 사용 여부 | CONFIRMED | Encoding TBD |
| W2D-006 | WINDOW → Domain | FULLY_OPEN_STATE | WINDOW | Domain | State | 현재 완전 열림 | 끝단 상태 | CONFIRMED | Encoding TBD |
| W2D-007 | WINDOW → Domain | FULLY_CLOSED_STATE | WINDOW | Domain | State | 현재 완전 닫힘 | 끝단 상태 | CONFIRMED | Encoding TBD |
| W2D-008 | WINDOW → Domain | FULLY_OPENED | WINDOW | Domain | Event | 완전 열림으로 새 전이 | 이벤트/결과 갱신 | CONFIRMED | Occurrence ID TBD |
| W2D-009 | WINDOW → Domain | FULLY_CLOSED | WINDOW | Domain | Event | 완전 닫힘으로 새 전이 | 이벤트/결과 갱신 | CONFIRMED | Occurrence ID TBD |
| W2D-010 | WINDOW → Domain | WINDOW_ANTIPINCH | WINDOW | Domain | Event | 닫힘 중 유효 끼임 발생 | WarningManager | CONFIRMED | Event ID/Sequence TBD |
| W2D-011 | WINDOW → Domain | ANTIPINCH_PROTECTION_STATUS | WINDOW | Domain | State | 보호 진행/완료/중단 + Reverse 상태 | Warning/VehicleState 관리 | CONFIRMED | 최대시간 500~1000ms 후보 |
| W2D-012 | WINDOW → Domain | WINDOW_COMMAND_RESULT | WINDOW | Domain | Result | ACCEPTED / IN_PROGRESS / DONE / REJECTED / CANCELLED / FAILED | ResultManager | CONFIRMED | Reason code TBD |
| W2D-013 | WINDOW → Domain | WINDOW_RESULT_REASON | WINDOW | Domain | Meta | LOCAL_OVERRIDE / STOP_REQUESTED / SUPERSEDED / ANTIPINCH / Fault 등 | CANCELLED/FAILED 원인 구분 | CONFIRMED | Code TBD |
| W2D-014 | WINDOW → Domain | WINDOW_FAULT | WINDOW | Domain | Fault | Window ECU 현재/복구 Fault + Category/Code | Fault/Availability 반영 | CONFIRMED | Fault Code TBD |

---

## 4.5 VSS ↔ Domain

| ID | Direction | Interface | Producer | Consumer | Type | Meaning / Values | Domain Usage | Logical Status | Remaining Detail |
|---|---|---|---|---|---|---|---|---|---|
| D2V-001 | Domain → VSS | VEHICLE_WELCOME | Domain | VSS | Event | 차량 사용 시작 전이 | One-shot 음향 의미 | CONFIRMED | Occurrence/Age encoding TBD |
| D2V-002 | Domain → VSS | VEHICLE_GOODBYE | Domain | VSS | Event | 차량 사용 종료 전이 | One-shot 음향 의미 | CONFIRMED | Occurrence/Age encoding TBD |
| D2V-003 | Domain → VSS | DOOR_LOCK_COMPLETE | Domain | VSS | Event | 새 LOCK 요청 목표 상태 정상 확인 | Lock 완료 음향 의미 | CONFIRMED | Occurrence/Age encoding TBD |
| D2V-004 | Domain → VSS | DOOR_UNLOCK_COMPLETE | Domain | VSS | Event | 새 UNLOCK 요청 목표 상태 정상 확인 | Unlock 완료 음향 의미 | CONFIRMED | Occurrence/Age encoding TBD |
| D2V-005 | Domain → VSS | DOOR_LOCK_ERROR | Domain | VSS | Event | 실제 LOCK 실행 후 목표 확인 실패 | Lock 오류 음향 의미 | CONFIRMED | Occurrence/Age encoding TBD |
| D2V-006 | Domain → VSS | WINDOW_ANTIPINCH_STATE | Domain | VSS | State | CLEAR / ACTIVE | Stateful 끼임 경고 | CONFIRMED | Quality/Age mapping TBD |
| D2V-007 | Domain → VSS | OCCUPANT_HAZARD_STATE | Domain | VSS | State | CLEAR / ACTIVE | 잔류 탑승자 경고 | CONFIRMED | Quality/Age mapping TBD |
| D2V-008 | Domain → VSS | REAR_OBSTACLE_STATE | Domain | VSS | State | CLEAR / CAUTION / EMERGENCY | 후방 위험 음향 | CONFIRMED | Quality/Age mapping TBD |
| D2V-009 | Domain → VSS | REAR_DETECTION_ACTIVATION | Domain | VSS | State/Availability | ACTIVE / DISABLED | 후방 기능 활성과 CLEAR 구분 | CONFIRMED | Encoding TBD |
| D2V-010 | Domain → VSS | VSS_INPUT_QUALITY_META | Domain | VSS | Quality/Meta | Validity/Availability/Original Age/Ordering/Generation | Stale/late/재시작 구분 | CONFIRMED | 실제 필드 구성 TBD |
| V2D-001 | VSS → Domain | VSS_STATE | VSS | Domain | State | STARTUP / READY / PLAYING / FAULT | VSS 현재 상태 | CONFIRMED | Encoding TBD |
| V2D-002 | VSS → Domain | VSS_AVAILABILITY | VSS | Domain | Availability | FULL / DEGRADED / UNAVAILABLE | 음향 서비스 가용성 | CONFIRMED | Encoding TBD |
| V2D-003 | VSS → Domain | VSS_ACCEPTING_EVENTS | VSS | Domain | Availability | Semantic Event/State 수용 가능 여부 | Warning 전달 가능성 판단 | CONFIRMED | Encoding TBD |
| V2D-004 | VSS → Domain | VSS_FAULT_ACTIVE | VSS | Domain | Fault/State | Internal Output Fault 존재 여부 | Fault 상태 표시 | CONFIRMED | Encoding TBD |
| V2D-005 | VSS → Domain | VSS_LAST_FAULT | VSS | Domain | Fault/Data | 최근 주요 Internal Output Fault | 진단/MOBILE 표시 | CONFIRMED | Fault code TBD |
| V2D-006 | VSS → Domain | SOUND_ASSET_UNAVAILABLE | VSS | Domain | Fault | 필요 저장 Sound Asset 사용 불가 | VSS Fault 원인 관리 | CONFIRMED | 실제 Fault encoding TBD |
| V2D-007 | VSS → Domain | PLAYBACK_START_FAILURE | VSS | Domain | Fault | Playback 시작 실패 | VSS Fault 원인 관리 | CONFIRMED | 실제 Fault encoding TBD |
| V2D-008 | VSS → Domain | AUDIO_OUTPUT_FAILURE | VSS | Domain | Fault | Audio Output Path 이상 | VSS Fault 원인 관리 | CONFIRMED | 실제 Fault encoding TBD |
| V2D-009 | VSS → Domain | PLAYBACK_STATE_FAILURE | VSS | Domain | Fault | Playback 상태 관리 이상 | VSS Fault 원인 관리 | CONFIRMED | 실제 Fault encoding TBD |
| V2D-010 | VSS → Domain | INITIALIZATION_FAILURE | VSS | Domain | Fault | VSS 초기화 실패 | VSS Fault 원인 관리 | CONFIRMED | 실제 Fault encoding TBD |

---

# 5. MOBILE ↔ ESP32 Logical Message Summary

이 표는 Domain Master Matrix의 직접 네트워크 대상은 아니지만 End-to-End 연결 검증을 위해 유지한다.

| Direction | Interface | Meaning | Status |
|---|---|---|---|
| MOBILE → ESP32 | `MOBILE_REQUEST` | Door / Climate / Interior Light / Digital Key 설정 요청 | CONFIRMED |
| MOBILE → ESP32 | `REQUEST_ID` | MOBILE 생성 Request 식별 | CONFIRMED |
| MOBILE → ESP32 | Session Context | 재연결 전후/지연 메시지 구분 | Logical CONFIRMED / Detail TBD |
| MOBILE → ESP32 | `STATE_QUERY` | State / Result / Warning 조회 | CONFIRMED |
| MOBILE → ESP32 | `WARNING_ACK` | 사용자의 Warning 확인 | CONFIRMED |
| MOBILE → ESP32 | `APP_ACTIVE_STATE` | Digital Key 활성 조건 | Meaning CONFIRMED / Method TBD |
| ESP32 → MOBILE | `CONNECTION_STATUS` | 등록/차량 Link/Session 상태 | CONFIRMED |
| ESP32 → MOBILE | `REQUEST_RESULT` | Domain이 확정한 차량 처리 결과 | CONFIRMED |
| ESP32 → MOBILE | `RESULT_REASON` | Reject/Cancel/Fail 사유 | CONFIRMED / Code TBD |
| ESP32 → MOBILE | `RESULT_CONFIRMATION` | 결과 확인/미확인 구분 | CONFIRMED |
| ESP32 → MOBILE | `VEHICLE_STATE_UPDATE` | 사용자 표시용 차량 상태 | CONFIRMED |
| ESP32 → MOBILE | `WARNING_INFO` | 사용자 표시용 Warning | CONFIRMED |
| ESP32 → MOBILE | `FUNCTION_AVAILABILITY` | 기능 가용성/제한 이유 | CONFIRMED / Enum TBD |
| ESP32 → MOBILE | `DIGITAL_KEY_SETTING_STATE` | Domain 반영 설정 | CONFIRMED |
| ESP32 → MOBILE | `DIGITAL_KEY_AVAILABILITY` | Digital Key 현재 가용성 | CONFIRMED |
| ESP32 → MOBILE | `DIGITAL_KEY_RESULT` | 자동 Unlock 결과/Origin | CONFIRMED |

---

# 6. 남은 TBD Register

## 6.1 논리 의미는 확정, 상세 설계만 남음

| 항목 | 상태 | 이후 담당 |
|---|---|---|
| `REQUEST_ID` 폭 / rollover | TBD | MOBILE + ESP32 + Domain |
| `SESSION_ID` 생성 주체 / 폭 / 수명 | TBD | MOBILE + ESP32 + Domain |
| `DEVICE_CONTEXT_ID` 실제 표현 | TBD | ESP32 + Domain |
| APP_ACTIVE 확인 방법 | TBD | MOBILE + ESP32 |
| Result Reason Code | TBD | Domain Interface 상세 설계 |
| Function Availability enum | TBD | Domain Interface 상세 설계 |
| Warning Type / Severity Code | TBD | Domain Interface 상세 설계 |
| Quality 공통 enum/Raw 표현 | TBD | Domain + Network |
| RSSI NEAR/FAR 임계값 | TBD | ESP32 실기 검증 |
| RSSI Filter/Hysteresis | TBD | ESP32 실기 검증 |
| CIS `REAR_DISTANCE` 최종 유효 범위 | TBD | CIS + Domain |
| WINDOW Anti-Pinch 최대 시간 | TBD | WINDOW 실기 검증 |
| WINDOW Position 허용 오차 | TBD | WINDOW |
| VSS One-shot Pending 복구 정책 | TBD | VSS + Domain |

## 6.2 범위 결정 필요

| 항목 | 현재 상태 | 처리 |
|---|---|---|
| EXTERIOR_LIGHT | OUT_OF_SCOPE_CANDIDATE | 삭제 확정 시 SR/SysRS/Interface/State/Test에서 일괄 제거 |
| MOBILE Window 제어 | OUT_OF_SCOPE | 기능 추가 시 MOBILE 요구사항부터 변경 |

---

# 7. 네트워크 담당자 전달 시 추가할 열

논리 Freeze 후 네트워크 담당자는 각 Interface에 다음 정보를 붙인다.

```text
Message ID
Signal / Field ID
Start Byte / Start Bit
Length
DLC
Endianness
Data Type
Scaling / Offset
Invalid Raw Value
Cycle / Event Trigger
Timeout
Alive Counter
CRC / E2E
Sequence
UART Message Type
UART Frame Layout
```

논리 의미와 네트워크 표현은 분리한다.

예:

```text
논리:
DOOR_LOCK_STATE = LOCKED / UNLOCKED / UNKNOWN

네트워크:
어느 CAN ID의 몇 번째 Bit로 보낼지
→ 네트워크 설계 단계
```

---

# 8. Freeze 판단

현재 기준으로 다음 ECU의 **논리 Interface는 Freeze Review 가능**하다.

```text
BCM      → Freeze Review Ready
CIS      → Freeze Review Ready
WINDOW   → Freeze Review Ready
VSS      → Freeze Review Ready
ESP32    → Freeze Review Ready
MOBILE↔ESP32 → Freeze Review Ready
```

남은 TBD 중 대부분은 **Wire/Timing/Encoding 또는 실기 Calibration**이므로 논리 Interface Freeze 자체를 막지 않는다.

단, 다음 두 범위 결정은 문서에서 명확히 표시해야 한다.

```text
EXTERIOR_LIGHT
→ 삭제 여부 최종 결정

MOBILE Window Control
→ 현재 미지원 유지 또는 기능 추가 결정
```

현재 범위를 유지한다면 둘 다 논리 Freeze의 Blocker가 아니다.

---

# 9. 다음 작업 순서

```text
1. 이 Master Matrix 팀 리뷰
        ↓
2. CONFIRMED / TBD / OUT_OF_SCOPE 최종 확인
        ↓
3. Logical Interface Freeze
        ↓
4. 네트워크 담당자에게 Matrix 전달
        ↓
5. CAN / UART / Bluetooth 상세 Mapping
        ↓
6. Vehicle_Types.h
        ↓
7. Domain_Interface.h
        ↓
8. VehicleStateManager
        ↓
9. RequestManager / ResultManager
        ↓
10. DigitalKeyManager / WarningManager / ClimateManager
        ↓
11. End-to-End Scenario Test
```

추천 Freeze 리뷰 질문은 단순하다.

```text
Producer가 이 정보를 실제 만들 수 있는가?
Consumer가 실제 사용하는가?
State / Event / Result 의미가 섞이지 않았는가?
같은 정보가 중복 정의되지 않았는가?
Fault와 정상 거부가 구분되는가?
Quality/Freshness 없이 위험한 값을 사용하고 있지 않은가?
```
