# ClimateManager 상세 설명

## 핵심 역할

중앙 S32K344가 공조 모드를 하나만 선택하고 최종 Fan/Thermal 목표를 생성한다.

```text
SettingsManager
  ├ Target Temperature
  ├ Auto Climate
  └ Manual Fan
        +
VehicleStateManager
  ├ CIS Cabin Temperature
  └ BCM Climate State
        ↓
ClimateManager
        ↓
CommandManager
        ↓
BCM
```

## 모드

```text
NONE
MANUAL
AUTO
```

- Manual Fan 성공 → AUTO 종료, Thermal OFF
- Auto ON → 기존 Manual 대체
- Target Temperature만 변경 → AUTO가 이미 활성일 때만 재계산
- Auto OFF → Auto mode 종료, 과거 Manual 자동 복구 없음

## 자동 Fan 후보 규칙

SysRS 후보:

```text
OFF -> LOW     d >= 0.5 C
LOW -> OFF     d < 0.2 C

LOW -> MEDIUM  d >= 2.0 C
MEDIUM -> LOW  d < 1.7 C

MEDIUM -> HIGH d >= 4.0 C
HIGH -> MEDIUM d < 3.7 C
```

시작 시 상승 경계로 초기 단계 선택, 이후 히스테리시스 적용.

## Thermal 후보 규칙

`deltaT = cabin - target`

```text
deltaT > +0.5 C  -> COOL
-0.5..+0.5 C     -> IDLE
deltaT < -0.5 C  -> HEAT
```

출력:

```text
abs(delta) <= 0.5 C -> 0%
0.5 < abs < 2.0     -> 30%
2.0 <= abs < 4.0    -> 50%
4.0 <= abs           -> configured maximum
```

최대 출력은 실제 구성에서 검증된 상한을 사용한다.

## 중요한 구현: Fan 먼저

SysRS는 필요한 Fan 정상 상태를 확인하기 전 Thermal 출력을 시작하지 않도록 한다.

그래서 AUTO에서 Thermal이 필요하면:

```text
1) Fan target + Thermal OFF
2) BCM commanded Fan == target
3) BCM measured Fan == target
4) HeatRemovalState == NORMAL
5) 그 다음 Fan + COOL/HEAT + output
```

2단계 Command로 구현했다.

## 수동 Fan

```text
MOBILE Fan HIGH
→ SettingsManager Pending
→ BCM Fan HIGH + Thermal OFF
→ BCM commanded/measured HIGH 확인
→ Result DONE
→ SettingsManager Commit
→ Mode MANUAL / Auto OFF
```

실패하거나 결과를 확인할 수 없으면 기존 confirmed Manual setting으로 덮어쓰지 않는다.

## Temperature scale Gap

현재 `targetTemperature`와 `cabinTemperature`는 `int16_t`지만 SysRS에는 wire/internal scale이 정의되지 않았다.

그런데 정책은 0.5°C를 사용한다.

그래서 코드가 임의로 `23 == 23°C`라고 가정하지 않고:

```c
temperatureUnitsPerDegC
```

를 Integration Config에서 받는다.

예:

```text
10 units / C
230 = 23.0 C
235 = 23.5 C
```

실제 스케일 확정 전 기본값은 0이며 AUTO climate는 CONFIGURATION_REQUIRED로 안전하게 보류된다.

## 결과 확인

BCM Climate는 전용 완료 Event만 가정하지 않고, SysRS의 적용/측정 상태 확인 의미를 사용한다.

그래서 ResultManager v0.2에:

```c
ResultManager_ConfirmObservedState()
```

를 추가했다.

즉 신뢰 가능한 BCM Climate State가 목표와 일치할 때 Command를 DONE으로 확정한다.

## 다음

다음 Feature는 `InteriorLightManager`.
