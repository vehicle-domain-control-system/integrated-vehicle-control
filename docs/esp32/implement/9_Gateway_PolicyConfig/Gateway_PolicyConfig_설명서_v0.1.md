# Gateway_PolicyConfig 설계 설명서 v0.1

> 대상 코드  
> - `ESP32/common/Gateway_PolicyConfig.h`  
> - `ESP32/common/Gateway_PolicyConfig.c`
>
> 현재 사용 대상  
> - `ProximityManager`
>
> 상태  
> - `DESIGN`
> - 실제 RSSI calibration 값은 `PROVISIONAL / TBD`

---

# 1. 목적

`Gateway_PolicyConfig`는 ESP32 Gateway의 알고리즘 코드와 실제 calibration 값을 분리하기 위한 공통 설정 모듈이다.

현재 첫 사용 대상은 Bluetooth RSSI 기반 근접 판단이다.

```text
ProximityManager
      ↓
Gateway_PolicyConfig
      ↓
실제 차량/단말 시험 후 결정할 값
```

---

# 2. 왜 설정을 분리하는가?

현재 SysRS는 다음 항목을 실제 구성에서 정하도록 남겨 두고 있다.

```text
근접 진입 기준
근접 이탈 기준
안정화 조건
근접 정보 유효 시간
제공 주기
```

따라서 코드 안에 임의로:

```c
#define NEAR_RSSI (-60)
#define FAR_RSSI  (-70)
```

처럼 프로젝트 확정값으로 박아 넣으면 안 된다.

현재 테스트에서는 알고리즘 검증을 위해 예시 수치를 사용하지만,
그 수치는 실제 차량 calibration 값이 아니다.

---

# 3. 현재 Proximity Config

```c
typedef struct
{
    int16_t near_enter_rssi_dbm;
    int16_t far_exit_rssi_dbm;
    uint16_t stable_sample_count;
    Gateway_TimeMs_t proximity_expiry_ms;
} Gateway_ProximityPolicyConfig_t;
```

---

# 4. Near Enter Threshold

```text
RSSI >= near_enter_rssi_dbm
```

이면 NEAR 후보가 될 수 있다.

RSSI는 일반적으로 값이 클수록 신호가 강하다.

예:

```text
-55 dBm > -75 dBm
```

이다.

실제 threshold는 아직 확정하지 않는다.

---

# 5. Far Exit Threshold

```text
RSSI <= far_exit_rssi_dbm
```

이면 FAR 후보가 될 수 있다.

---

# 6. Hysteresis 조건

설정 유효 조건:

```text
near_enter_rssi_dbm > far_exit_rssi_dbm
```

이다.

예시:

```text
NEAR enter = -60
FAR exit   = -70
```

이면:

```text
-70 < RSSI < -60
```

구간은 hysteresis band가 된다.

---

# 7. 왜 Hysteresis가 필요한가?

Threshold가 하나뿐이면 RSSI가 경계 부근에서:

```text
NEAR
FAR
NEAR
FAR
```

로 빠르게 흔들릴 수 있다.

두 threshold를 분리하면 현재 상태에 따라 전환 기준이 달라져
불필요한 토글을 줄일 수 있다.

---

# 8. Stable Sample Count

```c
stable_sample_count
```

는 상태를 실제로 바꾸기 전에 같은 전환 후보가 연속으로 몇 번 필요한지 나타낸다.

예:

```text
stable_sample_count = 3
```

이라면 NEAR 후보가 세 번 연속 확인되어야 실제 NEAR가 된다.

실제 횟수는 아직 TBD다.

---

# 9. Proximity Expiry

```c
proximity_expiry_ms
```

는 마지막 실제 RSSI 관측 후 이 시간에 도달했을 때 입력을 STALE로 만드는 기준이다.

예:

```text
last sample = 1000 ms
expiry      = 500 ms

1499 ms → 아직 expiry 전
1500 ms → STALE
```

실제 값은 아직 TBD다.

---

# 10. 기본값을 두지 않는 이유

현재 실제 calibration이 확정되지 않았으므로:

```text
Init
→ Config 없음
```

상태로 시작한다.

설정이 들어오기 전 `ProximityManager`는 RSSI를 정상 판단하지 않는다.

---

# 11. Validation

현재 설정은 다음 조건을 모두 만족해야 한다.

```text
near threshold > far threshold
stable sample count >= 1
expiry > 0
```

조건을 만족하지 않으면:

```c
GATEWAY_STATUS_INVALID_ARGUMENT
```

이다.

---

# 12. 실제 Integration

향후 실제 시험값이 정해지면 Boot 설정 단계에서:

```c
Gateway_ProximityPolicyConfig_t config = {
    ...
};

Gateway_PolicyConfig_SetProximity(&config);
```

형태로 연결할 수 있다.

설정 값을 어디에 저장할지(NVS/compile config 등)는 아직 확정하지 않는다.

---

# 13. 핵심 정리

`Gateway_PolicyConfig`는:

> **아직 시험으로 결정해야 하는 근접 calibration 값을 Proximity 알고리즘 코드와 분리하는 설정 계층이다.**

현재 구조는 값 자체가 아니라 **어떤 설정 항목이 필요한지**만 코드로 확정한다.
