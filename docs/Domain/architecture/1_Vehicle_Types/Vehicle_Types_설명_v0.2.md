# `Vehicle_Types.h` v0.2 수정 설명

## 핵심 변경

기존 구조는 `CisEnvironmentState_t` 전체에 `validity / qualityReason / meta`가 하나만 존재했습니다. 이 경우 온도는 정상인데 Vision만 고장인 경우처럼 **같은 CIS 내부 값들의 품질이 서로 다른 상황을 표현하기 어렵습니다.**

v0.2에서는 다음처럼 분리했습니다.

```text
CIS
├─ Occupant
│  ├─ Presence
│  ├─ Count
│  └─ 독립 Quality
│
├─ Cabin
│  ├─ Temperature + Quality
│  ├─ Humidity + Quality
│  └─ Illuminance + Quality
│
└─ Rear
   ├─ Distance
   ├─ Measurement State
   └─ 독립 Quality
```

공통으로 다음 구조를 사용합니다.

```c
typedef struct
{
    ValueValidity_t validity;
    QualityReason_t reason;
    SignalMeta_t meta;
} ValueQuality_t;
```

따라서 `NO_DATA`, `INVALID`, `STALE`을 각 값별로 독립적으로 표현할 수 있습니다.

## ESP32 입력도 분리

기존 `DigitalKeyInput_t`는 Connection과 Proximity가 한 덩어리였지만 실제로 등록/연결 정보와 RSSI 기반 근접 정보의 갱신 주기는 달라질 수 있습니다.

v0.2:

```text
DigitalKeyInput
├─ DigitalKeyConnectionState
│  ├─ Registration
│  ├─ Bluetooth Connection
│  ├─ App Active
│  ├─ Session
│  └─ Meta
│
└─ ProximityInput
   ├─ NEAR / FAR / UNKNOWN
   └─ Meta
```

이렇게 해야 Connection 메시지가 새로 왔다는 이유로 오래된 Proximity까지 새 값처럼 취급하지 않습니다.

## 네트워크 설계와의 관계

여전히 다음 값은 이 파일에 넣지 않습니다.

```text
CAN ID
Signal ID
Byte/Bit
DLC
Baud Rate
UART Frame
CRC/E2E 배치
```

이 파일은 오직 차량 논리 의미만 정의합니다.
