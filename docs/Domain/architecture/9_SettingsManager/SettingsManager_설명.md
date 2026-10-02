# SettingsManager 상세 설명

## 1. 한 문장으로

`SettingsManager`는:

> **사용자가 요청했고 S32K344 Domain이 확정한 "설정값"을 ECU 실제 적용 상태나 센서 실측값과 분리해서 보관하는 모듈**

입니다.

---

# 2. 왜 별도 Manager가 필요한가?

예를 들어 공조에는 서로 다른 값이 동시에 존재합니다.

```text
Target Temperature = 23°C
→ 사용자 확정 설정

Fan Target = HIGH
→ ClimateManager가 계산한 실행 목표

Fan Measured = MEDIUM
→ BCM 실제 측정

Cabin Temperature = 28°C
→ CIS 실측
```

이 값들을 전부 `VehicleState` 하나의 현재 상태로 취급하면 의미가 섞입니다.

따라서:

```text
SettingsManager
= 사용자가 설정했고 중앙이 확정한 값

VehicleStateManager
= ECU/센서가 실제로 보고한 현재 상태

ClimateManager
= 현재 설정+상태를 이용해 실행 목표 생성
```

으로 분리합니다.

---

# 3. 현재 관리하는 설정

## 차량 공통 설정

```text
Target Temperature
Auto Climate ON/OFF
Manual Fan / Circulation Selection
Interior Light User ON/OFF
Interior Light Brightness
Interior Light RGB
```

## 연결 Context별 설정

```text
Digital Key Auto Unlock ON/OFF
```

Digital Key는 차량 전역 하나로 저장하지 않습니다.

SysRS에서:

```text
단말
차량
현재 연결
```

에 연결하고 새 연결에서는 OFF로 시작하도록 되어 있기 때문입니다.

---

# 4. Target Temperature 범위를 왜 하드코딩하지 않았나?

현재 SysRS는:

```text
지원 범위를 확인해야 한다
```

고 요구하지만, 현재 검토한 문서에서는:

```text
16~30°C
18~28°C
```

같은 실제 숫자 범위가 확정되어 있지 않습니다.

따라서 임의로 범위를 만들지 않았습니다.

기본:

```c
targetTemperatureRangeConfigured = false;
```

입니다.

이 상태에서 Target Temperature 요청이 오면:

```text
SETTINGS_STATUS_CONFIG_REQUIRED
```

을 반환합니다.

통합 시 실제 지원 범위가 정해지면:

```c
config.targetTemperatureRangeConfigured = true;
config.targetTemperatureMin = ...;
config.targetTemperatureMax = ...;
```

로 넣습니다.

이렇게 해야 "문서에 없는 범위"를 코드가 몰래 만들어내지 않습니다.

---

# 5. 설정 DONE과 실제 실행 DONE의 차이

SysRS의 중요한 규칙입니다.

예:

```text
MOBILE
Target Temperature = 23°C
       ↓
Domain
23°C 설정 반영
       ↓
Setting Request DONE
```

이 시점은:

```text
실내가 23°C가 됐다
```

는 뜻이 아닙니다.

실제:

```text
CIS Cabin Temperature
BCM Fan
BCM Thermal Output
```

은 별도 상태입니다.

조명도 마찬가지입니다.

```text
Brightness = 80%
설정 반영 DONE
```

과:

```text
BCM이 실제 출력 지시 적용
실제 LED가 물리적으로 켜짐
```

은 다른 의미입니다.

---

# 6. 즉시 확정하는 Setting

현재 코드에서는 다음 요청은 중앙 반영 시 바로 `CONFIRMED` 가능합니다.

```text
Target Temperature
Auto Climate Enable
Interior Light Enable
Interior Light Brightness
Interior Light Color
Digital Key Setting
```

상위 흐름에서:

```text
SettingsManager_ApplyMobileSettingRequest()
→ SETTINGS_APPLY_CONFIRMED
```

이면 RequestManager에 설정 결과 `DONE`을 연결할 수 있습니다.

실제 기능 적용 상태는 이후 별도로 업데이트합니다.

---

# 7. Manual Fan만 2단계인 이유

수동 Fan 요청은 문서에서 단순 설정 반영이 아니라:

```text
중앙 Mode 선택
+
BCM의 해당 Fan 목표 확인
```

까지 완료 의미에 포함됩니다.

그래서:

```text
MOBILE Fan HIGH
      ↓
SettingsManager
Stage Pending
      ↓
ClimateManager / CommandManager
      ↓
BCM Fan HIGH
      ↓
BCM Result
```

정상 확인 뒤:

```c
SettingsManager_CommitManualFanSelection()
```

을 호출합니다.

그때 비로소:

```text
Manual Fan Setting = HIGH
Climate Mode = MANUAL
Auto Climate = OFF
```

가 확정됩니다.

실패하면:

```c
SettingsManager_CancelPendingManualFan()
```

을 사용하고 기존 Confirmed Setting은 유지합니다.

---

# 8. 공조 Mode

내부적으로 다음 Mode를 둡니다.

```text
NONE
MANUAL
AUTO
```

이것은 Wire Signal을 새로 만든 것이 아니라 Domain 내부 설계 타입입니다.

### Auto ON

```text
activeMode = AUTO
```

기존 Manual 실행을 대체합니다.

### Auto OFF

현재 AUTO였다면:

```text
activeMode = NONE
```

으로 갑니다.

과거 Manual mode를 자동 복원하지 않습니다.

### Manual Fan 성공

```text
activeMode = MANUAL
autoClimateEnabled = false
```

가 됩니다.

### Target Temperature 변경

설정값만 바꿉니다.

```text
activeMode
```

를 바꾸지 않습니다.

즉 꺼져 있던 공조를 Target Temperature 변경만으로 새로 시작하지 않습니다.

---

# 9. Interior Light 설정

사용자 설정:

```text
User Enable
Brightness
RGB
```

만 저장합니다.

실제 현재 출력:

```text
NORMAL
GOODBYE
WARNING
FAULT
```

중 무엇이 나갈지는 이후 `InteriorLightManager`가 결정합니다.

중요:

```text
User Light OFF
```

여도:

```text
Safety WARNING
FAULT
GOODBYE
```

같은 알림 출력까지 무조건 막는 설정으로 쓰지 않습니다.

사용자 OFF는 NORMAL 설정에만 적용됩니다.

---

# 10. Digital Key 설정

새 Session이 활성화되면:

```c
SettingsManager_OnSessionActivated(...)
```

을 호출합니다.

그러면:

```text
Digital Key Setting = OFF
```

로 시작합니다.

예:

```text
Device A
Session 10
Generation 1
Digital Key OFF
```

사용자가 ON 요청:

```text
Digital Key ON
```

연결 종료:

```text
contextActive = false
```

새 Session:

```text
Device A
Session 11
Generation 2
Digital Key OFF
```

즉 이전 연결의 ON 설정을 새 연결에 자동 이월하지 않습니다.

---

# 11. GatewaySessionManager와 연결

권장 흐름:

```text
ESP32 Connection
      ↓
GatewaySessionManager
      ↓
SESSION_ACTIVATED
      ↓
SettingsManager_OnSessionActivated()
```

Session 변경/무효화:

```text
GatewaySessionManager
      ↓
SESSION_INVALIDATED
      ↓
SettingsManager_OnSessionInvalidated()
```

따라서 Digital Key 설정이 현재 Connection Context와 항상 연계됩니다.

---

# 12. 설정 Persistence

현재 코드에서 설정은 RAM 상태입니다.

즉:

```text
S32K344 Reset
→ SettingsManager_Init()
→ 기존 설정 자동 복구 안 함
```

입니다.

이건 의도적입니다.

현재 SysRS는:

```text
Setting DONE
≠
재부팅 뒤에도 저장
```

이라고 명시하고 있고 저장 지속성은 후속 결정입니다.

따라서 지금 `FlashStorage/NvMManager`를 임의 추가하지 않았습니다.

---

# 13. 다음 연결

이제 Core Chain은:

```text
GatewaySessionManager       ✅
        ↓
RequestManager              ✅
        ↓
SettingsManager             ✅
```

까지 내려왔습니다.

다음 `PermissionManager`에서는:

```text
Domain READY?
Session Valid?
Function Available?
필수 State/Quality 확인 가능?
```

같은 **공통 실행 Gate**를 구현합니다.

그 다음:

```text
CommandManager
ResultManager
```

를 만들면 실행 ECU와의 명령/결과 추적 Core가 완성됩니다.
