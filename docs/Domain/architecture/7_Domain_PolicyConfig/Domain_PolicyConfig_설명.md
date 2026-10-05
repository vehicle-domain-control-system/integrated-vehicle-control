# Domain_PolicyConfig 설명

## 목적

`Domain_PolicyConfig`는 **차량 기능 정책/Calibration 값**을 한곳에서 관리합니다.

`Network_Config`와는 완전히 다른 책임입니다.

```text
Domain_PolicyConfig
= 기능 판단에 쓰는 값

Network_Config
= 전송 방법에 쓰는 값
```

예:

```text
Rear Emergency ≤ 40 cm
Auto Ventilation start = 30°C
Result wait = 3 s
```

는 `Domain_PolicyConfig`.

```text
CAN ID
Start Bit
Cycle
Timeout
Alive Counter
```

는 `Network_Config`.

## 왜 maturity를 같이 저장하나?

현재 SysRS의 여러 수치는 아직 `[CANDIDATE]` 또는 `[잠정]`입니다.

따라서:

```c
value = 40
maturity = DOMAIN_POLICY_MATURITY_CANDIDATE
```

처럼 저장합니다.

이렇게 하면 코드가 40cm를 사용하더라도 문서/시험 단계에서 이 값이
"최종 확정된 양산값"이라고 오해하지 않습니다.

## 현재 들어간 값

```text
Rear:
10~40 cm   EMERGENCY 후보
40~70 cm   CAUTION 후보
70~100 cm  CLEAR 후보

Auto Ventilation:
Start 30°C
Stop 28°C
Max 5 min
Window 80% Closed

Result wait:
Setting 2s
Door 3s
Climate 5s
Light apply 1s
Window move 10s
Window stop 1s

Event:
Duplicate assist 150ms
One-shot max age 2s

MOBILE recent result display:
5건
```

모든 값은 원 문서의 성숙도를 보존합니다.

## 중요한 해석

`doorResultWaitMs = 3000`은:

```text
3초 지나면 Door actuator FAILED
```

가 아닙니다.

정확히는:

```text
3초 동안 유효 최종 결과를 못 확인
→ 결과 확인 관점에서 UNKNOWN/UNCONFIRMED 가능
```

입니다.

Local actuator safety timeout은 BCM 자체 설계와 별개입니다.
