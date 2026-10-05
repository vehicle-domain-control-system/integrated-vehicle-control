# InteriorLightManager 상세 설명

## 1. 역할

`InteriorLightManager`는 중앙 S32K344에서 **현재 어떤 실내 조명 표현 하나를 BCM에 내려야 하는지** 결정합니다.

우선순위는 SysRS 기준 그대로:

```text
FAULT
  >
WARNING
  >
GOODBYE
  >
NORMAL
```

입니다.

## 2. 사용자 OFF 범위

사용자 `Interior Light OFF`는 `NORMAL`에만 적용합니다.

따라서:

```text
User Light OFF
+
WARNING ACTIVE
```

이면 최종 목표는 WARNING입니다.

안전·상황 알림을 사용자 설정으로 막지 않습니다.

## 3. 색상

프로젝트 기준 색상:

```text
NORMAL   white   (100,100,100)  또는 사용자 색상
GOODBYE  cyan    (0,70,70)
WARNING  yellow  (100,70,0)
FAULT    red     (100,0,0)
```

GOODBYE/WARNING/FAULT는 사용자 색상으로 바꾸지 않습니다.

NORMAL 외 알림의 기본 전체 밝기는 100%입니다.

## 4. NORMAL 밝기

사용자 밝기 설정이 있으면 그대로 사용합니다.

```text
User brightness exists
→ illuminance 무관
```

사용자 밝기가 없을 때만 조도 기반 후보를 사용합니다.

초기 구간:

```text
< 30 lux       25%
30..299        50%
300..2999      75%
>= 3000        100%
```

이후 30% 히스테리시스:

```text
30 lux boundary:
rise >= 39
fall < 21

300:
rise >= 390
fall < 210

3000:
rise >= 3900
fall < 2100
```

## 5. 조도 Scale TBD

CIS SysRS에는 조도 측정 단위가 아직 `lux 등 TBD`로 남아 있습니다.

그래서 코드가 `cabinIlluminance == lux`라고 임의 가정하지 않습니다.

```c
illuminanceUnitsPerLux
```

를 Config에서 받습니다.

기본값:

```text
0 = TBD
```

사용자 brightness가 직접 설정된 경우에는 이 값이 없어도 NORMAL 조명을 실행할 수 있습니다.

자동 밝기가 필요한 경우만 `CONFIGURATION_REQUIRED`입니다.

## 6. GOODBYE

GOODBYE의 실제 유지 시간은 문서에서 아직 정해지지 않았습니다.

따라서 InteriorLightManager가 임의로:

```text
2 sec
5 sec
10 sec
```

를 만들지 않습니다.

향후 `VehicleEventManager`가:

```text
SetGoodbyeActive(true)
...
SetGoodbyeActive(false)
```

로 semantic lifetime을 제공합니다.

중요:

```text
GOODBYE 중 WARNING/FAULT 발생
→ GOODBYE 폐기
→ 높은 우선순위 적용

WARNING 종료
→ 과거 GOODBYE 자동 재개 X
```

또 WARNING/FAULT가 이미 활성인데 GOODBYE가 새로 발생해도 나중 재생용으로 Queue하지 않습니다.

## 7. 매 평가 때 Winner 재선택

발생 순간에만 우선순위를 정하지 않습니다.

`Process()`마다 현재 활성 집합에서 다시 선택합니다.

예:

```text
WARNING + FAULT
→ FAULT

FAULT CLEAR
WARNING still ACTIVE
→ WARNING

WARNING CLEAR
→ NORMAL
```

이 구조 때문에 하위 지속 상태로 자연스럽게 복귀합니다.

단, GOODBYE는 일시적이므로 preempt되면 폐기됩니다.

## 8. 빠른 우선순위 전환

NORMAL Command 결과 대기 중 WARNING이 발생해도 기존 결과를 1초 기다리지 않습니다.

자동 정책 Command는:

```text
old Command
→ CANCELLED by policy

new higher-priority Command
→ new decisionSequence
```

으로 즉시 대체합니다.

이를 위해 `CommandManager v0.4`에:

```c
CommandManager_CancelByPolicy()
```

를 추가했습니다.

이 API는 MOBILE Request와 연결된 Command에는 사용할 수 없습니다.

## 9. BCM 적용 확인

BCM `InteriorLightState`에서:

```text
quality = OK
commandApplied = true
type / level / color == latest target
```

이면 `ResultManager_ConfirmObservedState()`를 사용해 최신 Command를 DONE 처리합니다.

중요:

```text
commandApplied
!=
physical LED confirmation
```

입니다.

`physicalFeedbackSupported == false`인 구성에서는 실제 점등 확인으로 해석하지 않습니다.

## 10. Timeout

조명 적용 결과 대기 후보는 Domain Policy의 1초를 사용합니다.

시간이 지나면:

```text
FAILED 추정 X
UNCONFIRMED
```

으로 처리합니다.

Timeout 자체만으로 같은 목표를 자동 재실행하지 않습니다.

## 11. 다음 단계

다음 Feature는 `AutoVentilationManager`입니다.
