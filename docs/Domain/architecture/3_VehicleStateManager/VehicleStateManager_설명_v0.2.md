# `VehicleStateManager` v0.2 수정 설명

## 기존 문제

기존 코드는 Freshness를:

```text
BCM
CIS
WINDOW
VSS
ESP32
```

처럼 ECU 단위로 관리했습니다.

문제:

```text
BCM Door      = 방금 수신
BCM Climate   = 2초째 미수신
```

이어도 Door가 들어오면 BCM 전체가 FRESH로 갱신될 수 있었습니다.

## v0.2 구조

이제 다음처럼 Interface Group 단위로 관리합니다.

```text
BCM_DOOR
BCM_CLIMATE
BCM_INTERIOR_LIGHT

CIS_OCCUPANT
CIS_CABIN
CIS_REAR

WINDOW_STATE
VSS_STATE

ESP32_CONNECTION
ESP32_PROXIMITY
```

각 Group마다:

```text
NO_DATA
FRESH
STALE

lastUpdateMs
elapsedSinceUpdateMs
timeoutMs
```

를 독립 관리합니다.

## 예시

```text
BCM Door message
100 ms 주기

BCM Climate
500 ms 주기
```

라면:

```c
config.timeoutMs[VSM_GROUP_BCM_DOOR] = 300U;
config.timeoutMs[VSM_GROUP_BCM_CLIMATE] = 1500U;
```

처럼 서로 다른 Timeout을 줄 수 있습니다.

## CIS STALE

CIS Cabin이 STALE이라고 Rear까지 STALE로 만들지 않습니다.

```text
CIS Cabin STALE
→ Temperature/Humidity/Illuminance만 STALE

CIS Rear 정상
→ Rear Distance는 계속 사용 가능
```

## ESP32 STALE

Connection과 Proximity도 분리했습니다.

```text
ESP32 Connection FRESH
ESP32 Proximity STALE
```

이 가능하며, Proximity가 STALE이면:

```text
NEAR/FAR 판단값 → UNKNOWN
```

으로 처리합니다.

절대 STALE을 FAR로 바꾸지 않습니다.
