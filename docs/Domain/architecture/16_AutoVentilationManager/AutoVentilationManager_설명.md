# AutoVentilationManager 상세 설명

## 1. 역할

`AutoVentilationManager`는 중앙 S32K344가 소유하는 **상태를 가진 자동 환기 작업(Job)** 입니다.

창문 명령 하나와 환기 작업 전체를 동일하게 취급하지 않습니다.

```text
Auto Ventilation Job
        │
        ├─ WINDOW VENT/MOVE Command
        │       └─ DONE = 창문 이동 완료
        │
        └─ Job 종료
                ├─ 실내 온도 <= 28°C
                └─ 시작 후 5분
```

따라서:

```text
Window Command DONE
≠
Auto Ventilation Job DONE
```

입니다.

---

## 2. 재시작 규칙

SysRS의 핵심 규칙:

```text
종료 후
OFF
→ 새 ON
```

이 있어야 새 환기가 가능합니다.

그래서 Manager 상태는:

```text
WAIT_ENABLE_OFF
      │ valid OFF
      ▼
READY_FOR_ENABLE
      │ new valid ON
      ▼
ARMED
      │ start conditions
      ▼
RUNNING
```

입니다.

정상 종료/실패 후 Enable이 계속 ON이면:

```text
WAIT_ENABLE_OFF
```

로 돌아갑니다.

온도가 다시 30°C 이상이 되어도 자동 재시작하지 않습니다.

### 부팅 시

부팅 직후 Enable input이 이미 ON이라고 해서 새 환기를 시작하지 않습니다.

반드시 부팅 이후 유효한 OFF를 한 번 관측한 뒤 새 ON이 필요합니다.

이렇게 해서:

```text
reboot only
→ ventilation restart
```

를 방지합니다.

---

## 3. 현재 외부 Semantic Input

현재 SysRS에서 물리 Producer/전달 경로가 아직 확정되지 않은 정보:

```text
현장 환기 사용 허용
차량 사용 상태
WINDOW 실제 구동 허용
```

은 Manager가 직접 wire signal을 발명하지 않습니다.

대신:

```c
AutoVentilationManager_SetEnableInput()
AutoVentilationManager_SetVehicleUseInput()
AutoVentilationManager_SetWindowOperationPermission()
```

을 제공합니다.

나중에:

```text
VehicleUsageManager
실제 현장 switch/input
Window permission source
```

가 확정되면 이 API에 연결합니다.

---

## 4. 시작 조건

모든 조건이 확인되어야 합니다.

```text
Enable = ON
차량 사용 종료 상태
탑승자 없음
Cabin Temp >= 30°C
일반 공조 정지
WINDOW state 신뢰 가능
WINDOW anti-pinch CLEAR
WINDOW 현재 정지
Window Operation Allowed
Domain/Function available
```

30°C는 현재 문서의 `[잠정]` 시연값입니다.

---

## 5. Temperature Scale

`cabinTemperature`는 `int16_t`이지만 실제 raw scale이 아직 SysRS에서 확정되지 않았습니다.

따라서:

```c
temperatureUnitsPerDegC
```

를 Config로 사용합니다.

예:

```text
10 units/C
300 = 30.0°C
280 = 28.0°C
```

기본값은 0(TBD)이며 값이 확정되기 전에는:

```text
CONFIGURATION_REQUIRED
```

로 자동 환기를 시작하지 않습니다.

---

## 6. Window 목표

현재 기준:

```text
0%   = fully open
100% = fully closed
target = 80%
```

즉 약 20% 열린 상태입니다.

### 현재 위치 > 80%

```text
100%
  ↓
VENT target 80%
```

Command를 보냅니다.

### 현재 위치 <= 80%

이미 충분히 열려 있으므로:

```text
추가 이동 없음
```

입니다.

중요:

```text
60% 상태
→ 80%로 닫지 않음
```

입니다.

---

## 7. 정상 종료

둘 중 하나:

```text
valid cabin temp <= 28°C
OR
monotonic elapsed >= 5 min
```

이면 환기 Job을 정상 종료합니다.

그리고:

```text
Auto CLOSE
```

명령을 절대 만들지 않습니다.

현재 Window 위치를 유지합니다.

---

## 8. 취소

현재 Job을 CANCELLED로 끝내는 조건:

```text
수동 Window 조작 / STOP
환기 Enable OFF
Window Operation Permission 철회
차량 사용 재개
탑승자 확인
다른 공조 시작
```

다른 공조는:

```text
Settings activeMode != NONE
또는
실제 BCM Climate가 OFF 상태가 아님
```

으로 확인합니다.

---

## 9. 실패

Job FAILED:

```text
필수 입력 신뢰 상실
Operation Permission 신뢰 상실
Window State 신뢰 불가
Anti-pinch / 보호 Active
VENT 이동 REJECTED / CANCELLED / FAILED
VENT 이동 결과 timeout
```

입니다.

`결과를 확인하지 못함` 자체를 WINDOW `FAILED`라고 꾸미지 않고:

```text
ResultManager -> UNCONFIRMED
Auto Ventilation Job -> FAIL_MOVE_RESULT_TIMEOUT
```

로 구분합니다.

---

## 10. 종료 시 STOP

종료/취소/실패했다고 무조건 STOP을 보내지 않습니다.

조건:

```text
창문이 아직
이 Auto Ventilation Job 때문에
움직이는 중
```

일 때만 STOP을 보냅니다.

예:

```text
Auto Ventilation 움직이는 중
Temp <= 28
→ STOP
```

반면:

```text
VENT 이동 DONE
이미 STOPPED
Temp <= 28
→ 새 STOP 없음
```

입니다.

또 수동 Window 조작이 들어오면 그 새 동작이 현재 owner이므로:

```text
Auto Ventilation이 별도 STOP을 보내
사용자의 새 조작을 끊지 않음
```

으로 처리합니다.

---

## 11. STOP 결과

STOP은 Job 종료와 별도로 확인합니다.

```text
Job = COMPLETED/CANCELLED/FAILED

Stop Confirmation
= CONFIRMED / UNCONFIRMED
```

STOP 결과를 못 받으면:

```text
STOP 성공이라고 표시하지 않음
```

입니다.

---

## 12. 이동 결과와 Job 결과 분리

예:

```text
31°C
Window 100%
    ↓
VENT 80%
    ↓
Window DONE
```

여기까지는:

```text
Job = RUNNING
```

입니다.

이후:

```text
29°C
→ 계속 RUNNING

28°C
→ Job COMPLETED
```

가 됩니다.

---

## 13. Manual Window Override

실제 Local Switch / STOP producer는 아직 상세 계약이 TBD입니다.

향후 해당 정보가 들어오면:

```c
AutoVentilationManager_NotifyManualWindowOverride();
```

를 호출합니다.

이 순간 AutoVentilation은 Window ownership을 포기합니다.

따라서 종료 처리에서 별도의 STOP으로 사용자의 새 동작을 끊지 않습니다.

---

## 14. 현재 정책값의 지위

```text
Start 30°C
Stop 28°C
Max 5 min
Target 80%
```

는 `Domain_PolicyConfig`에 이미 포함되어 있으며 모두 시연용 후보/잠정값의 maturity를 유지합니다.

양산 안전값으로 해석하면 안 됩니다.

---

## 15. 다음 단계

다음 Feature는 `WarningManager`가 적절합니다.

다만 최신 SysRS에는 `EXTERIOR_LIGHT`도 실제 범위로 존재하므로 Architecture v1.3부터:

```text
ExteriorLightManager
```

를 TODO Feature로 복구해 두었습니다.
