# ESP32 ↔ MOBILE BLE Interface Specification v0.2

> **Status:** Implementation Draft  
> **Scope:** MOBILE App ↔ ESP32 BLE/GATT 구현 계약  
> **Based on:** `network_sw_architecture.md`, `network_design_draft_v0.1.md`  
> **Target:** ESP32 + ESP-IDF v5.5.5 / MOBILE App  
> **Current App count:** 1 device  
> **Byte order:** Little Endian  
> **WINDOW:** 설계 유지 / 현재 구현·시연 보류  
>
> 본 문서는 기존 Network 문서에서 정의된 **메시지 의미, Type, Payload, Session/Request/Query 규칙**을 유지하면서,
> 기존 문서가 ESP32/MOBILE 담당 범위로 남겨 둔 **BLE GATT 구조와 전송 규칙**을 구현 가능한 수준으로 정의한다.

---

## 0. 규칙 출처 표기

본 문서의 규칙은 세 종류로 구분한다.

- **[BASE]**: 기존 `network_sw_architecture.md`, `network_design_draft_v0.1.md`에서 직접 승계한 규칙
- **[BLE-DECISION]**: 기존 문서가 ESP32/MOBILE 담당 상세로 남긴 부분에 대해 본 문서에서 정한 구현 규칙
- **[TBD]**: 기존 문서에서 확인 필요로 남아 있고 본 문서에서 임의 확정하지 않는 항목

충돌 시 우선순위:

```text
최신 팀 승인 Network contract
    >
본 BLE Interface 문서
    >
각 구현체의 내부 코드
```

`MESSAGE_TYPE`, 기존 M_* Payload 의미 또는 기존 코드값을 변경해야 할 경우
먼저 Network contract의 변경 여부를 확인한다.

---

# 1. 전체 구조

**[BASE]**

```text
┌───────────────┐
│  MOBILE App   │
└───────┬───────┘
        │
        │ BLE / GATT
        ▼
┌───────────────┐
│     ESP32     │
│ BLE ↔ UART GW │
└───────┬───────┘
        │
        │ UART-M
        │ 115200 bps / 8N1 / Full Duplex
        ▼
┌───────────────┐
│ S32K344 Domain│
└───────┬───────┘
        │
        │ CAN FD
        ▼
 BCM / CIS / VSS / WINDOW(reserved)
```

ESP32의 책임:

- BLE endpoint
- UART-M endpoint
- App ↔ Domain semantic relay
- `DEVICE_CONTEXT_ID` 관리
- `SESSION_ID` 관리
- `SESSION_READY` 판단
- M_CONTEXT 생성
- UART framing / CRC / resync
- BLE framing / fragmentation / reassembly
- App 연결 상태 및 App active 정보 관리
- RSSI 기반 proximity 정보 제공

ESP32가 하지 않는 일:

- 차량 기능 정책 재판단
- 차량 Result 임의 생성
- UART TX 성공을 `ACCEPTED`/`DONE`으로 변환
- BLE Write 성공을 차량 성공으로 변환
- Domain 결과 의미 변경
- 오래된 요청 자동 재실행

---

# 2. Transport 분리 원칙

## 2.1 UART-M

**[BASE]**

UART-M Frame:

```text
Byte 0~1   SYNC = A5 5A
Byte 2     VERSION = 1
Byte 3     MESSAGE_TYPE
Byte 4~5   PAYLOAD_LENGTH
Byte 6~7   LINK_SEQUENCE
Byte 8~    PAYLOAD
Last 2 B   CRC16
```

UART 전용 필드:

```text
SYNC
UART VERSION
PAYLOAD_LENGTH
LINK_SEQUENCE
CRC16
```

## 2.2 BLE

**[BLE-DECISION]**

BLE에서는 UART Frame 전체를 그대로 복제하지 않는다.

공유하는 것은:

```text
MESSAGE_TYPE
+
semantic PAYLOAD
```

이다.

따라서:

```text
Domain UART
[A5 5A][VER][TYPE][LEN][SEQ][PAYLOAD][CRC]
                    │
                    ▼
                  ESP32
                    │ UART 검증 완료
                    ▼
BLE
[BLE Header][TYPE에 해당하는 PAYLOAD]
                    │
                    ▼
                   App
```

반대 방향:

```text
App
[BLE Header][App Payload]
          │
          ▼
        ESP32
          │ 현재 DEVICE_CONTEXT_ID / SESSION_ID 삽입
          ▼
UART M_* Payload
          │ UART framing
          ▼
        Domain
```

---

# 3. BLE Discovery / Advertising

## 3.1 Device Name

**[BLE-DECISION]**

```text
Complete Local Name: VEHICLE-GW
```

앱은 이름만으로 장치를 신뢰하거나 식별하지 않는다.

## 3.2 Discovery 기준

**[BLE-DECISION]**

App은 **Vehicle Gateway Service UUID**를 기준으로 ESP32를 탐색한다.

Legacy advertising 기준 권장 배치:

```text
Advertising Data:
- Flags
- Vehicle Gateway Service UUID

Scan Response:
- Complete Local Name = VEHICLE-GW
```

---

# 4. GATT 구성

## 4.1 Vehicle Gateway Service

**[BLE-DECISION]**

```text
Service UUID
A0F00000-7C21-4B5A-9E32-5F4D4F42494C
```

## 4.2 Characteristics

| Characteristic | UUID | Property | 방향 | 역할 |
|---|---|---|---|---|
| Gateway Status | `A0F00001-7C21-4B5A-9E32-5F4D4F42494C` | READ + NOTIFY | ESP32 → App | Gateway / Session / Domain 상태 |
| App Command | `A0F00002-7C21-4B5A-9E32-5F4D4F42494C` | WRITE WITH RESPONSE | App → ESP32 | M_REQUEST / M_QUERY / M_WARNING_ACK |
| Vehicle Data | `A0F00003-7C21-4B5A-9E32-5F4D4F42494C` | NOTIFY | ESP32 → App | Domain 결과/상태/경고 |
| App State | `A0F00004-7C21-4B5A-9E32-5F4D4F42494C` | WRITE WITH RESPONSE | App → ESP32 | App active / App instance |

구조:

```text
Vehicle Gateway Service
│
├── Gateway Status
│     READ + NOTIFY
│
├── App Command
│     WRITE WITH RESPONSE
│
├── Vehicle Data
│     NOTIFY
│
└── App State
      WRITE WITH RESPONSE
```

## 4.3 CCCD

**[BLE-DECISION]**

App 연결 후 반드시 다음 Notify를 활성화한다.

```text
Gateway Status CCCD = Notify Enabled
Vehicle Data  CCCD = Notify Enabled
```

`App Command`를 전송하기 전에 최소 `Vehicle Data` Notify 구독이 완료되어야 한다.

---

# 5. BLE MTU / Fragmentation

## 5.1 MTU

**[BLE-DECISION]**

권장 ATT MTU:

```text
Preferred ATT MTU = 185
```

그러나 구현은 ATT MTU 23에서도 동작해야 한다.

ATT에서 실제 Characteristic Value에 사용할 수 있는 최대 크기는:

```text
ATT_VALUE_MAX = negotiated_MTU - 3
```

이다.

## 5.2 BLE Message Header

**[BLE-DECISION]**

`App Command`와 `Vehicle Data`는 다음 8 B Header를 사용한다.

| Byte | Field | Type | 규칙 |
|---:|---|---|---|
| 0 | BLE_PROTOCOL_VERSION | u8 | `1` |
| 1 | MESSAGE_TYPE | u8 | M_* Type |
| 2~3 | BLE_TRANSFER_ID | u16 | BLE 전송 식별자 |
| 4~5 | TOTAL_PAYLOAD_LENGTH | u16 | 전체 semantic Payload 길이 |
| 6 | FRAGMENT_INDEX | u8 | 0부터 시작 |
| 7 | FRAGMENT_COUNT | u8 | 전체 Fragment 개수 |
| 8~ | DATA | bytes | Payload 조각 |

모든 다중 바이트 정수는 Little Endian이다.

### 단일 Fragment 예

```text
01 20 35 00 2C 00 00 01 [44-byte M_RESULT payload]
│  │  └──┘ └──┘ │  │
│  │    ID   LEN │  COUNT=1
│  │             INDEX=0
│  TYPE=M_RESULT
VERSION=1
```

## 5.3 BLE_TRANSFER_ID

**[BLE-DECISION]**

- 방향별 u16 counter
- 최초값 1
- 0 예약
- application message마다 증가
- 65535 이후 1로 순환
- `REQUEST_ID`, `QUERY_ID`, `COMMAND_ID`, UART `LINK_SEQUENCE`와 의미적으로 독립

## 5.4 Fragment Data 크기

**[BLE-DECISION]**

```text
FRAGMENT_DATA_MAX
= negotiated_MTU - 3 - 8
```

예:

```text
MTU 185
→ ATT Value Max 182 B
→ Fragment Data Max 174 B

MTU 23
→ ATT Value Max 20 B
→ Fragment Data Max 12 B
```

## 5.5 Fragmentation 규칙

**[BLE-DECISION]**

- 모든 Fragment는 동일 `BLE_TRANSFER_ID`, `MESSAGE_TYPE`, `TOTAL_PAYLOAD_LENGTH`, `FRAGMENT_COUNT`를 사용한다.
- `FRAGMENT_INDEX`는 `0 .. FRAGMENT_COUNT-1` 순서로 전송한다.
- 한 방향에서 서로 다른 application message의 Fragment를 interleave하지 않는다.
- 모든 Fragment가 완성되기 전에는 M_* message로 처리하지 않는다.
- 재조립 후 실제 길이가 `TOTAL_PAYLOAD_LENGTH`와 다르면 폐기한다.
- Type별 고정 Payload 길이와 다르면 폐기한다.

## 5.6 Reassembly Timeout

**[BLE-DECISION]**

```text
BLE_REASSEMBLY_TIMEOUT = 1000 ms
```

첫 Fragment 수신 시점을 기준으로 한다.

Timeout 시:

```text
incomplete transfer 폐기
→ 차량 요청 실행 금지
→ 다음 BLE_TRANSFER_ID 수신은 정상적으로 새로 처리
```

---

# 6. 공통 자료형

**[BASE]**

| Type | Size | 규칙 |
|---|---:|---|
| u8 | 1 B | unsigned |
| u16 | 2 B | unsigned, little-endian |
| u32 | 4 B | unsigned, little-endian |
| u64 | 8 B | unsigned, little-endian |
| i16 | 2 B | two's complement, little-endian |
| rgb | 3 B | R, G, B 순 |

C 구조체의 자동 padding/align을 wire format으로 사용하지 않는다.

송수신은 명시적인 byte encode/decode를 사용한다.

---

# 7. 공통 코드표

## 7.1 VALUE_QUALITY

**[BASE]**

| Raw | 의미 |
|---:|---|
| 0 | OK |
| 1 | STALE |
| 2 | INVALID |
| 3 | NO_DATA |

## 7.2 CIS VALIDITY

| Raw | 의미 |
|---:|---|
| 0 | INVALID |
| 1 | VALID |

## 7.3 QUALITY_REASON

| Raw | 의미 |
|---:|---|
| 0 | NONE |
| 1 | NOT_READY |
| 2 | OUT_OF_RANGE |
| 3 | SENSOR_FAULT |
| 4 | VISION_FAULT |
| 5 | STALE |
| 6 | NO_DATA |
| 7 | COMMUNICATION_FAULT |
| 8 | RECOVERING |
| 9 | DATA_INVALID |

## 7.4 Vehicle RESULT

| Raw | 의미 |
|---:|---|
| 0 | ACCEPTED |
| 1 | IN_PROGRESS |
| 2 | DONE |
| 3 | REJECTED |
| 4 | CANCELLED |
| 5 | FAILED |
| 255 | 확인 기록 없음 / Field 없음. Vehicle Result enum이 아님 |

Result 숫자 크기로 처리 단계를 비교하지 않는다.

확정 최종 결과:

```text
DONE
REJECTED
CANCELLED
FAILED
```

가 확인된 뒤 늦게 도착한 `ACCEPTED` / `IN_PROGRESS`를 적용하지 않는다.

## 7.5 RESULT_CONFIRMATION

| Raw | 의미 |
|---:|---|
| 0 | UNCONFIRMED |
| 1 | CONFIRMED |

`UNCONFIRMED`를 `FAILED`로 바꾸지 않는다.

## 7.6 FUNCTION_AVAILABILITY

| Raw | 의미 |
|---:|---|
| 0 | AVAILABLE |
| 1 | LIMITED |
| 2 | UNAVAILABLE |

## 7.7 FAN

| Raw | 의미 |
|---:|---|
| 0 | OFF |
| 1 | LOW |
| 2 | MEDIUM |
| 3 | HIGH |
| 255 | UNKNOWN — 상태/측정용만 허용 |

## 7.8 THERMAL_DIRECTION

| Raw | 의미 |
|---:|---|
| 0 | IDLE |
| 1 | COOL |
| 2 | HEAT |

## 7.9 LIGHT_TYPE

| Raw | 의미 |
|---:|---|
| 0 | NORMAL |
| 1 | GOODBYE |
| 2 | WARNING |
| 3 | FAULT |

## 7.10 ORIGIN

| Raw | 의미 |
|---:|---|
| 0 | NONE |
| 1 | MOBILE_MANUAL |
| 2 | PROXIMITY_AUTO |
| 3 | LOCAL |
| 4 | TEST |
| 5 | VEHICLE_POLICY |

## 7.11 WARNING_TYPE

| Raw | 의미 |
|---:|---|
| 1 | 후방 |
| 2 | 잔류 탑승자 |
| 3 | 끼임 |
| 4 | BCM 고장 |
| 5 | CIS 고장 |
| 6 | WINDOW 고장 |
| 7 | VSS 고장 |

Severity:

| Raw | 의미 |
|---:|---|
| 0 | INFO |
| 1 | CAUTION |
| 2 | EMERGENCY |

## 7.12 FUNCTION_ID

M_AVAILABILITY의 순서는 다음과 같다.

| ID | 기능 |
|---:|---|
| 1 | DOOR |
| 2 | CLIMATE |
| 3 | INTERIOR_LIGHT |
| 4 | DIGITAL_KEY |
| 5 | OCCUPANT |
| 6 | ENVIRONMENT |
| 7 | REAR_WARNING |
| 8 | WINDOW |
| 9 | VSS |

## 7.13 RESULT_REASON / Availability Reason

| Code | Reason |
|---:|---|
| 0 | NONE |
| 1 | NOT_REGISTERED |
| 2 | LINK_LOST |
| 3 | SESSION_INVALID |
| 4 | DOOR_OPEN |
| 5 | STATE_UNTRUSTED |
| 6 | CMD_INVALID |
| 7 | NO_FEEDBACK |
| 8 | DRIVE_LIMIT_EXCEEDED |
| 9 | ALREADY_AT_TARGET |
| 10 | LOCAL_OVERRIDE |
| 11 | STOP_REQUESTED |
| 12 | SUPERSEDED |
| 13 | ANTIPINCH |
| 14 | FAULT |
| 15 | STALE |
| 16 | NO_DATA |
| 17 | NOT_READY |
| 18 | OUT_OF_RANGE |
| 19 | SOURCE_FAILURE |
| 20 | UNKNOWN_REQUEST |
| 21 | EXPIRED |
| 22 | ID_CONFLICT |
| 23 | POWER_NOT_ALLOWED |
| 24 | OVERHEAT |
| 25 | FAN_MISMATCH |

## 7.14 Invalid numeric values

| Data | Invalid / Unknown raw |
|---|---|
| u8 ratio / occupant count | 255 |
| i16 temperature | -32768 |
| u16 humidity / distance | 65535 |
| u32 illuminance | `0xFFFFFFFF` |

RGB의 0~255는 전부 정상 값 범위이므로 Quality로 유효성을 구분한다.

---

# 8. Gateway Status Characteristic

## 8.1 Property

**[BLE-DECISION]**

```text
READ + NOTIFY
```

## 8.2 Payload

**[BLE-DECISION]**

```text
Length = 21 B
```

| Byte | Field | Type | 규칙 |
|---:|---|---|---|
| 0 | BLE_PROTOCOL_VERSION | u8 | 1 |
| 1 | REGISTRATION_STATE | u8 | 0=NOT_REGISTERED, 1=REGISTERED, 255=UNKNOWN |
| 2 | DOMAIN_LINK_STATE | u8 | 0=DOWN, 1=UP |
| 3 | SESSION_READY | u8 | 0=NOT_READY, 1=READY |
| 4~7 | DEVICE_CONTEXT_ID | u32 | 0 예약 |
| 8~15 | SESSION_ID | u64 | 현재 Session |
| 16~19 | DOMAIN_BOOT_ID | u32 | 미확인 시 0 |
| 20 | DOMAIN_STATE | u8 | 0=INIT, 1=READY, 2=DEGRADED, 3=FAULT, 255=UNKNOWN |

## 8.3 Notify 조건

다음 중 하나가 변경되면 Notify한다.

```text
REGISTRATION_STATE
DOMAIN_LINK_STATE
SESSION_READY
DEVICE_CONTEXT_ID
SESSION_ID
DOMAIN_BOOT_ID
DOMAIN_STATE
```

App은 연결 직후 READ도 수행한다.

---

# 9. App State Characteristic

## 9.1 Property

**[BLE-DECISION]**

```text
WRITE WITH RESPONSE
```

## 9.2 Payload

**[BLE-DECISION]**

```text
Length = 5 B
```

| Byte | Field | Type | 규칙 |
|---:|---|---|---|
| 0 | APP_ACTIVE_STATE | u8 | 0=INACTIVE, 1=ACTIVE |
| 1~4 | APP_INSTANCE_ID | u32 | App process instance 식별, 0 금지 |

## 9.3 APP_INSTANCE_ID

**[BLE-DECISION]**

App은 process 시작 시 non-zero u32 `APP_INSTANCE_ID`를 생성한다.

다음 상황에서는 새 값을 생성한다.

```text
App process 재시작
REQUEST_ID counter를 초기화해야 하는 경우
사용자가 명시적으로 App session을 초기화하는 경우
```

ESP32가 기존 값과 다른 `APP_INSTANCE_ID`를 받으면:

```text
SESSION_READY = 0
기존 BLE/UART pending context 폐기
새 SESSION_ID 생성
M_CONTEXT 갱신
현재 상태/경고 Query 문맥 재확인
```

을 수행한다.

## 9.4 Active heartbeat

**[BLE-DECISION]**

App foreground 중:

```text
APP_ACTIVE_STATE = ACTIVE
1초 주기로 Write
```

background 진입 시 가능하면:

```text
APP_ACTIVE_STATE = INACTIVE
```

를 Write한다.

**[BASE]** Network contract의 App 활성 확인 한도는 3000 ms이다.

ESP32는 마지막 정상 App State 수신 이후 3000 ms를 넘으면
M_CONTEXT의 App 정보 품질을 유효한 ACTIVE로 유지하지 않는다.

---

# 10. Registration / DEVICE_CONTEXT_ID

**[BASE]**

`DEVICE_CONTEXT_ID`는 ESP32의 등록 정보와 연결되는 u32이며 0은 예약이다.

**[TBD]**

기존 Network 문서는 다음을 확정하지 않았다.

- 등록 UI
- 등록 테이블 구조
- 등록 절차
- ID 발급 방식

따라서 본 Interface는 등록 wire protocol을 새로 만들지 않는다.

구현 전제:

```text
ESP32에 non-zero DEVICE_CONTEXT_ID가 provision되어 있음
```

등록되지 않은 경우:

```text
REGISTRATION_STATE = NOT_REGISTERED
M_REQUEST 차량 제어 거부
```

---

# 11. Session 규칙

## 11.1 SESSION_ID 의미

**[BASE]**

- u64
- ESP32가 현재 App 단말 문맥에 제공
- 과거 문맥과 새 문맥을 구분
- App/ESP32/Domain 재시작 또는 REQUEST_ID 번호공간 재사용 시 과거 문맥을 재사용하지 않음

## 11.2 SESSION_ID 생성

**[BLE-DECISION]**

ESP32는 새 문맥이 필요한 경우 non-zero u64 새 값을 생성한다.

새 Session 조건:

```text
ESP32 boot
Domain BOOT_ID 변경
APP_INSTANCE_ID 변경
REQUEST_ID 번호공간 초기화를 위한 APP_INSTANCE_ID 변경
```

생성 방법은 ESP32의 random source를 사용한다.

## 11.3 BLE link-only reconnect

**[BLE-DECISION]**

단순 BLE link 재연결이며:

```text
ESP32 재시작 없음
Domain BOOT_ID 동일
APP_INSTANCE_ID 동일
```

이면 기존 `SESSION_ID`를 유지할 수 있다.

단, 연결 상실 중 남은 BLE Fragment / App Command pending은 폐기하고,
재연결 직후 새 요청 허용 전 현재 문맥을 다시 확인한다.

## 11.4 SESSION_READY

**[BASE]**

`SESSION_READY = 1`은 ESP32가 현재 차량 문맥을 확인했다는 의미다.

다음 의미는 아니다.

```text
App이 모든 초기 상태를 수신 완료함
모든 기능이 AVAILABLE임
차량 동작 자체가 성공함
```

ESP32는 현재 Domain 기동/단말/Session/Query가 일치하는 응답과 `M_QUERY_END`를 확인한 뒤 Ready로 판단한다.

---

# 12. App Command Characteristic

## 12.1 Property

```text
WRITE WITH RESPONSE
```

## 12.2 지원 Type

| Type | Message | BLE semantic Payload B | UART Payload B |
|---:|---|---:|---:|
| `0x10` | M_REQUEST | 12 | 24 |
| `0x11` | M_QUERY | 20 | 32 |
| `0x12` | M_WARNING_ACK | 9 | 21 |

BLE에서는 공통 요청 문맥 12 B를 생략한다.

ESP32가 UART 전송 직전에 다음을 추가한다.

```text
DEVICE_CONTEXT_ID : u32
SESSION_ID        : u64
```

---

# 13. M_REQUEST — 0x10

## 13.1 BLE Payload

| Byte | Field | Type |
|---:|---|---|
| 0~3 | REQUEST_ID | u32 |
| 4 | REQUEST_KIND | u8 |
| 5 | OPERATION | u8 |
| 6 | ARG_0 | u8 |
| 7 | ARG_1 | u8 |
| 8 | ARG_2 | u8 |
| 9 | ARG_3 | u8 |
| 10 | ARG_4 | u8 |
| 11 | ARG_5 | u8 |

## 13.2 Request 종류

| REQUEST_KIND | OPERATION | ARG_0~5 |
|---|---|---|
| 1 DOOR | 0=LOCK | 6 B 모두 0 |
| 1 DOOR | 1=UNLOCK | 6 B 모두 0 |
| 2 CLIMATE | 1=목표 온도 | ARG0~1 = i16, 0.01°C/raw; 나머지 0 |
| 2 CLIMATE | 2=AUTO | ARG0=0 OFF, 1 ON; 나머지 0 |
| 2 CLIMATE | 3=Fan | ARG0=0 OFF, 1 LOW, 2 MEDIUM, 3 HIGH; 나머지 0 |
| 3 INTERIOR_LIGHT | 1=사용 ON/OFF | ARG0=0 OFF, 1 ON; 나머지 0 |
| 3 INTERIOR_LIGHT | 2=밝기 | ARG0=0~100%; 나머지 0 |
| 3 INTERIOR_LIGHT | 3=NORMAL RGB | ARG0=R, ARG1=G, ARG2=B; 나머지 0 |
| 4 DIGITAL_KEY | 1=자동 Unlock ON/OFF | ARG0=0 OFF, 1 ON; 나머지 0 |

## 13.3 REQUEST_ID

**[BASE]**

- App이 생성
- u32
- 0 예약
- 새 사용자 입력마다 새 ID
- 같은 `DEVICE_CONTEXT_ID + SESSION_ID + REQUEST_ID`는 같은 요청
- 같은 ID인데 내용이 다르면 새 실행하지 않음
- ID 번호 공간을 재사용하기 전에 새 Session 필요

## 13.4 ESP32 UART mapping

BLE:

```text
[REQUEST_ID][KIND][OP][ARG0..5]
```

ESP32가:

```text
[DEVICE_CONTEXT_ID][SESSION_ID]
```

를 앞에 추가한다.

UART M_REQUEST Payload:

| UART Payload Byte | Field |
|---:|---|
| 0~3 | DEVICE_CONTEXT_ID |
| 4~11 | SESSION_ID |
| 12~15 | REQUEST_ID |
| 16 | REQUEST_KIND |
| 17 | OPERATION |
| 18~23 | ARG_0~5 |

---

# 14. M_QUERY — 0x11

## 14.1 BLE Payload

| Byte | Field | Type | 규칙 |
|---:|---|---|---|
| 0~3 | QUERY_ID | u32 | 0 금지 |
| 4 | QUERY_SCOPE | u8 | 0~3 |
| 5 | RESERVED | u8 | 반드시 0 |
| 6~13 | REQUEST_SESSION | u64 | 상태 조회면 0 |
| 14~17 | REQUEST_ID | u32 | 상태 조회면 0 |
| 18~19 | CURSOR | u16 | 현재 0 |

## 14.2 QUERY_SCOPE

| Raw | 조회 |
|---:|---|
| 0 | 현재 상태 |
| 1 | 요청 결과 |
| 2 | 경고 |
| 3 | 전체 |

## 14.3 Query 진행 규칙

**[BASE]**

- 한 App 단말은 Query 1개씩 진행
- `QUERY_ID = 0` 금지
- Query response는 같은 QUERY_ID를 유지
- `M_QUERY_END`가 Query 종료 메시지
- `M_QUERY_END.ITEM_COUNT`는 Query End 자체를 제외한 실제 응답 메시지 수

App은:

```text
expected query id
+
수신 response count
+
M_QUERY_END.ITEM_COUNT
```

를 비교하여 Query 수신 완료를 판단한다.

---

# 15. M_WARNING_ACK — 0x12

## 15.1 BLE Payload

| Byte | Field | Type |
|---:|---|---|
| 0 | WARNING_TYPE | u8 |
| 1~4 | OCCURRENCE_ID | u32 |
| 5~8 | WARNING_DOMAIN_BOOT | u32 |

## 15.2 의미

**[BASE]**

```text
WARNING_ACK = 사용자 "읽음"
```

아래로 변환하지 않는다.

```text
위험 CLEAR
위험 해제
VSS 음향 강제 정지
```

경고 읽음은:

```text
DOMAIN_BOOT_ID
WARNING_TYPE
OCCURRENCE_ID
```

가 일치할 때 연결한다.

---

# 16. Vehicle Data Characteristic

## 16.1 Property

```text
NOTIFY
```

## 16.2 Message Type 목록

**[BASE]**

| Type | Message | Payload B | 송신 조건 |
|---:|---|---:|---|
| `0x20` | M_RESULT | 44 | Result 단계/확인 상태 변경, Query |
| `0x21` | M_WARNING | 40 | 변경, 7종 각 1000ms 보완, Query |
| `0x22` | M_AVAILABILITY | 62 | 1000ms, 변경 |
| `0x23` | M_DIGITAL_STATUS | 30 | 1000ms, 변경 |
| `0x24` | M_DIGITAL_RESULT | 35 | 자동 Unlock Result 변경, Query |
| `0x25` | M_USER_SETTINGS | 36 | 1000ms, 변경 |
| `0x30` | M_BCM_DOOR_STATE | 40 | 200ms, 변경 |
| `0x31` | M_BCM_CLIMATE_STATE | 44 | 200ms, 변경 |
| `0x32` | M_BCM_LIGHT_STATE | 45 | 200ms, 변경 |
| `0x33` | M_BCM_STATUS | 42 | 1000ms, 변경 |
| `0x34` | M_CIS_ENVIRONMENT | 60 | 200ms, 변경 |
| `0x35` | M_CIS_OCCUPANT | 45 | 200ms, 변경 |
| `0x36` | M_CIS_REAR | 39 | 200ms, 변경 |
| `0x37` | M_CIS_STATUS | 54 | 1000ms, 변경 |
| `0x38` | M_WINDOW_STATE | 42 | 200ms, 변경 — 구현 보류 |
| `0x39` | M_WINDOW_FAULT | 42 | 1000ms, 변경 — 구현 보류 |
| `0x3A` | M_VSS_STATUS | 48 | 1000ms, 변경 |
| `0x40` | M_QUERY_END | 24 | Query 종료 |

`0x02 M_DOMAIN_ALIVE`는 App에 전달하지 않는다.

---

# 17. Domain → App 공통 하향 문맥

**[BASE]**

모든 `0x20~0x40` App 대상 메시지의 Payload는 필요한 경우
다음 20 B 하향 문맥으로 시작한다.

| Payload Byte | Field | Type | 규칙 |
|---:|---|---|---|
| 0~3 | DEVICE_CONTEXT_ID | u32 | 현재 단말 |
| 4~11 | SESSION_ID | u64 | 현재 수신 App 문맥 |
| 12~15 | DOMAIN_BOOT_ID | u32 | 정보 제공 Domain 기동 |
| 16~19 | QUERY_ID | u32 | Query 응답이면 원 QUERY_ID, unsolicited이면 0 |

App은 현재 `DEVICE_CONTEXT_ID`, `SESSION_ID`, `DOMAIN_BOOT_ID`와 맞지 않는 데이터를
현재 UI 문맥에 임의 적용하지 않는다.

---

# 18. M_RESULT — 0x20

```text
Payload Length = 44 B
```

| Byte | Field | Type | 규칙 |
|---:|---|---|---|
| 0~19 | DOWNSTREAM_CONTEXT | 20 B | §17 |
| 20~27 | REQUEST_SESSION | u64 | 원 요청 Session |
| 28~31 | REQUEST_ID | u32 | 원 요청 ID |
| 32 | RESULT | u8 | Vehicle Result / 255 값 없음 |
| 33~34 | RESULT_REASON | u16 | §7.13 |
| 35 | RESULT_CONFIRMATION | u8 | 0 UNCONFIRMED / 1 CONFIRMED |
| 36 | COMMAND_TARGET | u8 | 0 Domain 설정 / 1 BCM / 2 WINDOW |
| 37~40 | COMMAND_ID | u32 | 설정만 반영이면 0 |
| 41 | ORIGIN | u8 | §7.10 |
| 42~43 | RESULT_AGE | u16 | ms |

App은:

```text
REQUEST_SESSION + REQUEST_ID
```

로 원 요청과 Result를 연결한다.

---

# 19. M_WARNING — 0x21

```text
Payload Length = 40 B
```

| Byte | Field | Type | 규칙 |
|---:|---|---|---|
| 0~19 | DOWNSTREAM_CONTEXT | 20 B | §17 |
| 20 | WARNING_TYPE | u8 | §7.11 |
| 21~24 | OCCURRENCE_ID | u32 | Domain 기동 내 발생 ID |
| 25 | SEVERITY | u8 | INFO/CAUTION/EMERGENCY |
| 26 | WARNING_STATE | u8 | 0=CLEAR, 1=ACTIVE |
| 27 | READ_STATE | u8 | 0=UNREAD, 1=READ |
| 28 | VALUE_QUALITY | u8 | §7.1 |
| 29 | QUALITY_REASON | u8 | §7.3 |
| 30~33 | SOURCE_BOOT | u32 | 원 관측 생성 노드 |
| 34~37 | SOURCE_SEQUENCE | u32 | 원 관측 번호 |
| 38~39 | SOURCE_AGE | u16 | ms |

`WARNING_STATE=CLEAR`이어도 Quality가 `NO_DATA/INVALID/STALE`이면
정상 상태로 단정하지 않는다.

---

# 20. M_AVAILABILITY — 0x22

```text
Payload Length = 62 B
```

| Byte | Field | Type |
|---:|---|---|
| 0~19 | DOWNSTREAM_CONTEXT | 20 B |
| 20 | FUNCTION_ID = DOOR(1) | u8 |
| 21 | DOOR_AVAILABILITY | u8 |
| 22~23 | DOOR_REASON | u16 |
| 24 | FUNCTION_ID = CLIMATE(2) | u8 |
| 25 | CLIMATE_AVAILABILITY | u8 |
| 26~27 | CLIMATE_REASON | u16 |
| 28 | FUNCTION_ID = INTERIOR_LIGHT(3) | u8 |
| 29 | LIGHT_AVAILABILITY | u8 |
| 30~31 | LIGHT_REASON | u16 |
| 32 | FUNCTION_ID = DIGITAL_KEY(4) | u8 |
| 33 | DIGITAL_KEY_AVAILABILITY | u8 |
| 34~35 | DIGITAL_KEY_REASON | u16 |
| 36 | FUNCTION_ID = OCCUPANT(5) | u8 |
| 37 | OCCUPANT_AVAILABILITY | u8 |
| 38~39 | OCCUPANT_REASON | u16 |
| 40 | FUNCTION_ID = ENVIRONMENT(6) | u8 |
| 41 | ENVIRONMENT_AVAILABILITY | u8 |
| 42~43 | ENVIRONMENT_REASON | u16 |
| 44 | FUNCTION_ID = REAR_WARNING(7) | u8 |
| 45 | REAR_WARNING_AVAILABILITY | u8 |
| 46~47 | REAR_WARNING_REASON | u16 |
| 48 | FUNCTION_ID = WINDOW(8) | u8 |
| 49 | WINDOW_AVAILABILITY | u8 |
| 50~51 | WINDOW_REASON | u16 |
| 52 | FUNCTION_ID = VSS(9) | u8 |
| 53 | VSS_AVAILABILITY | u8 |
| 54~55 | VSS_REASON | u16 |
| 56~59 | UPDATE_SEQUENCE | u32 |
| 60~61 | SOURCE_AGE | u16 |

WINDOW 항목은 계약상 유지하지만 현재 구현/시연 보류다.

---

# 21. M_DIGITAL_STATUS — 0x23

```text
Payload Length = 30 B
```

| Byte | Field | Type | 규칙 |
|---:|---|---|---|
| 0~19 | DOWNSTREAM_CONTEXT | 20 B | §17 |
| 20 | DIGITAL_KEY_SETTING_STATE | u8 | 0 OFF / 1 ON |
| 21 | DIGITAL_KEY_AVAILABILITY | u8 | AVAILABLE/LIMITED/UNAVAILABLE |
| 22~23 | REASON | u16 | §7.13 |
| 24~27 | UPDATE_SEQUENCE | u32 | 새 판단/설정 평가 |
| 28~29 | SOURCE_AGE | u16 | ms |

---

# 22. M_DIGITAL_RESULT — 0x24

```text
Payload Length = 35 B
```

| Byte | Field | Type | 규칙 |
|---:|---|---|---|
| 0~19 | DOWNSTREAM_CONTEXT | 20 B | §17 |
| 20~23 | AUTO_OCCURRENCE_ID | u32 | 자동 Unlock 식별 |
| 24~27 | RELATED_DOOR_COMMAND_ID | u32 | BCM Door Command 연결 |
| 28 | ORIGIN | u8 | 2=PROXIMITY_AUTO |
| 29 | RESULT | u8 | Result / 255 값 없음 |
| 30~31 | RESULT_REASON | u16 | §7.13 |
| 32 | RESULT_CONFIRMATION | u8 | UNCONFIRMED/CONFIRMED |
| 33~34 | RESULT_AGE | u16 | ms |

자동 Unlock은 App `REQUEST_ID`를 생성하지 않는다.

---

# 23. M_USER_SETTINGS — 0x25

```text
Payload Length = 36 B
```

| Byte | Field | Type | 규칙 |
|---:|---|---|---|
| 0~19 | DOWNSTREAM_CONTEXT | 20 B | §17 |
| 20~21 | TARGET_TEMPERATURE | i16 | 0.01°C/raw, -32768 미확인 |
| 22 | CLIMATE_AUTO | u8 | 0 OFF / 1 ON / 255 미확인 |
| 23 | USER_FAN_LEVEL | u8 | FAN 코드 |
| 24 | INTERIOR_LIGHT_ENABLED | u8 | 0 OFF / 1 ON / 255 미확인 |
| 25 | NORMAL_LIGHT_LEVEL | u8 | 0~100% |
| 26~28 | NORMAL_LIGHT_RGB | rgb | R/G/B |
| 29 | VALUE_QUALITY | u8 | §7.1 |
| 30~33 | UPDATE_SEQUENCE | u32 | 설정 평가 |
| 34~35 | SOURCE_AGE | u16 | ms |

이 메시지는 사용자 반영 설정이며 실제 BCM 출력 상태와 구분한다.

---

# 24. M_BCM_DOOR_STATE — 0x30

```text
Payload Length = 40 B
```

| Byte | Field | Type | 규칙 |
|---:|---|---|---|
| 0~19 | DOWNSTREAM_CONTEXT | 20 B | §17 |
| 20~23 | BCM_BOOT_ID | u32 | 원 BCM BOOT_ID |
| 24~27 | BCM_TX_SEQUENCE | u32 | 원 CAN TX_SEQUENCE |
| 28 | DOOR_LOCK_STATE | u8 | 0 LOCKED / 1 UNLOCKED / 255 UNKNOWN |
| 29 | DOOR_OPEN_STATE | u8 | 0 CLOSED / 1 OPEN / 255 UNKNOWN |
| 30 | DOOR_COMPOSITE_STATE | u8 | 0 NORMAL / 1 INCONSISTENT / 2 UNTRUSTED |
| 31 | LOCK_QUALITY | u8 | VALUE_QUALITY |
| 32 | OPEN_QUALITY | u8 | VALUE_QUALITY |
| 33 | COMPOSITE_QUALITY | u8 | VALUE_QUALITY |
| 34~37 | UPDATE_SEQUENCE | u32 | 도어 상태 평가 |
| 38~39 | SOURCE_AGE | u16 | ms |

---

# 25. M_BCM_CLIMATE_STATE — 0x31

```text
Payload Length = 44 B
```

| Byte | Field | Type | 규칙 |
|---:|---|---|---|
| 0~19 | DOWNSTREAM_CONTEXT | 20 B | §17 |
| 20~23 | BCM_BOOT_ID | u32 | 원 BCM |
| 24~27 | BCM_TX_SEQUENCE | u32 | 원 CAN sequence |
| 28 | FAN_COMMAND_LEVEL | u8 | FAN |
| 29 | FAN_MEASURED_LEVEL | u8 | FAN / 255 UNKNOWN |
| 30 | THERMAL_DIRECTION_STATE | u8 | IDLE/COOL/HEAT |
| 31 | THERMAL_OUTPUT_LEVEL | u8 | 0~100% |
| 32 | HEAT_REMOVAL_STATE | u8 | 0 정상 / 1 과열 차단 / 255 측정 불가 |
| 33 | FAN_COMMAND_QUALITY | u8 | VALUE_QUALITY |
| 34 | FAN_MEASURED_QUALITY | u8 | VALUE_QUALITY |
| 35 | THERMAL_DIRECTION_QUALITY | u8 | VALUE_QUALITY |
| 36 | THERMAL_OUTPUT_QUALITY | u8 | VALUE_QUALITY |
| 37 | HEAT_REMOVAL_QUALITY | u8 | VALUE_QUALITY |
| 38~41 | UPDATE_SEQUENCE | u32 | 평가 번호 |
| 42~43 | SOURCE_AGE | u16 | ms |

---

# 26. M_BCM_LIGHT_STATE — 0x32

```text
Payload Length = 45 B
```

| Byte | Field | Type | 규칙 |
|---:|---|---|---|
| 0~19 | DOWNSTREAM_CONTEXT | 20 B | §17 |
| 20~23 | BCM_BOOT_ID | u32 | 원 BCM |
| 24~27 | BCM_TX_SEQUENCE | u32 | 원 CAN sequence |
| 28 | INTERIOR_LIGHT_TYPE_STATE | u8 | LIGHT_TYPE |
| 29 | INTERIOR_LIGHT_LEVEL_STATE | u8 | 0~100% |
| 30~32 | INTERIOR_LIGHT_COLOR_STATE | rgb | R/G/B |
| 33 | INTERIOR_LIGHT_APPLY_RESULT | u8 | 0 적용됨 / 1 실패 / 255 UNKNOWN |
| 34 | LIGHT_PHYSICAL_FEEDBACK_CAPABILITY | u8 | 0 미지원 / 1 지원; 현재 0 |
| 35 | LIGHT_TYPE_QUALITY | u8 | VALUE_QUALITY |
| 36 | LIGHT_LEVEL_QUALITY | u8 | VALUE_QUALITY |
| 37 | LIGHT_COLOR_QUALITY | u8 | VALUE_QUALITY |
| 38 | LIGHT_APPLY_QUALITY | u8 | VALUE_QUALITY |
| 39~42 | UPDATE_SEQUENCE | u32 | 평가 번호 |
| 43~44 | SOURCE_AGE | u16 | ms |

---

# 27. M_BCM_STATUS — 0x33

```text
Payload Length = 42 B
```

| Byte | Field | Type | 규칙 |
|---:|---|---|---|
| 0~19 | DOWNSTREAM_CONTEXT | 20 B | §17 |
| 20~23 | BCM_BOOT_ID | u32 | 원 BCM |
| 24~27 | BCM_TX_SEQUENCE | u32 | 원 CAN sequence |
| 28 | BCM_ECU_STATE | u8 | 0 INIT / 1 READY / 2 DEGRADED / 3 FAULT |
| 29~30 | FAULT_MASK | u16 | BCM bit0~7 |
| 31 | ACTIVE_FAULT_CATEGORY | u8 | 0 없음 / 1 SENSOR / 2 FUNCTION / 3 COMM |
| 32~33 | ACTIVE_FAULT_CODE | u16 | 현재 주요 고장 |
| 34 | AFFECTED_FUNCTION | u8 | bit0 Door / bit1 Climate / bit2 Light |
| 35 | RECOVERING | u8 | 0 아니오 / 1 복구중 |
| 36~39 | UPDATE_SEQUENCE | u32 | 평가 번호 |
| 40~41 | SOURCE_AGE | u16 | ms |

BCM FAULT_MASK:

```text
bit0 DOOR_SENSOR_FAULT
bit1 FAN_SENSOR_FAULT
bit2 TEMPERATURE_SENSOR_FAULT
bit3 OVERHEAT_FAULT
bit4 FAN_FAULT
bit5 LOCK_ACTUATOR_FAULT
bit6 LIGHT_APPLY_FAULT
bit7 COMM_TIMEOUT_FAULT
```

---

# 28. M_CIS_ENVIRONMENT — 0x34

```text
Payload Length = 60 B
```

| Byte | Field | Type | 규칙 |
|---:|---|---|---|
| 0~19 | DOWNSTREAM_CONTEXT | 20 B | §17 |
| 20~23 | CIS_BOOT_ID | u32 | 원 CIS |
| 24~27 | CIS_TX_SEQUENCE | u32 | 원 CAN sequence |
| 28~29 | TEMPERATURE | i16 | 0.01°C/raw, -32768 무효 |
| 30~33 | TEMPERATURE_SEQUENCE | u32 | 새 측정 번호 |
| 34~35 | TEMPERATURE_AGE | u16 | ms |
| 36 | TEMPERATURE_VALIDITY | u8 | 0 INVALID / 1 VALID |
| 37 | TEMPERATURE_REASON | u8 | QUALITY_REASON |
| 38~39 | HUMIDITY | u16 | 0.01%RH/raw, 65535 무효 |
| 40~43 | HUMIDITY_SEQUENCE | u32 | 새 측정 번호 |
| 44~45 | HUMIDITY_AGE | u16 | ms |
| 46 | HUMIDITY_VALIDITY | u8 | 0 INVALID / 1 VALID |
| 47 | HUMIDITY_REASON | u8 | QUALITY_REASON |
| 48~51 | ILLUMINANCE | u32 | 1 lx/raw, 0xFFFFFFFF 무효 |
| 52~55 | ILLUMINANCE_SEQUENCE | u32 | 새 측정 번호 |
| 56~57 | ILLUMINANCE_AGE | u16 | ms |
| 58 | ILLUMINANCE_VALIDITY | u8 | 0 INVALID / 1 VALID |
| 59 | ILLUMINANCE_REASON | u8 | QUALITY_REASON |

온도/습도/조도는 각각 독립적인 Sequence / Age / Validity / Reason을 가진다.

---

# 29. M_CIS_OCCUPANT — 0x35

```text
Payload Length = 45 B
```

| Byte | Field | Type | 규칙 |
|---:|---|---|---|
| 0~19 | DOWNSTREAM_CONTEXT | 20 B | §17 |
| 20~23 | CIS_BOOT_ID | u32 | 원 CIS |
| 24~27 | CIS_TX_SEQUENCE | u32 | 원 CAN sequence |
| 28~31 | PI_BOOT_ID | u32 | Pi 원본 기동 |
| 32~35 | VISION_SEQUENCE | u32 | Pi 새 판정 번호 |
| 36~37 | VISION_AGE | u16 | 원 판정 후 누적 ms |
| 38 | OCCUPANT_PRESENCE | u8 | 0 없음 / 1 있음 / 255 확인 불가 |
| 39 | PRESENCE_VALIDITY | u8 | 0 INVALID / 1 VALID |
| 40 | PRESENCE_REASON | u8 | QUALITY_REASON |
| 41 | OCCUPANT_COUNT | u8 | 0~5 / 255 확인 불가 |
| 42 | COUNT_VALIDITY | u8 | 0 INVALID / 1 VALID |
| 43 | COUNT_REASON | u8 | QUALITY_REASON |
| 44 | VISION_FUNCTION_STATUS | u8 | CIS 기능 상태 |

---

# 30. M_CIS_REAR — 0x36

```text
Payload Length = 39 B
```

| Byte | Field | Type | 규칙 |
|---:|---|---|---|
| 0~19 | DOWNSTREAM_CONTEXT | 20 B | §17 |
| 20~23 | CIS_BOOT_ID | u32 | 원 CIS |
| 24~27 | CIS_TX_SEQUENCE | u32 | 원 CAN sequence |
| 28~31 | UPDATE_SEQUENCE | u32 | 새 거리 관측 |
| 32~33 | SOURCE_AGE | u16 | ms |
| 34~35 | REAR_DISTANCE | u16 | 0.1cm/raw, 65535 거리 없음/무효 |
| 36 | VALIDITY | u8 | 0 INVALID / 1 VALID |
| 37 | QUALITY_REASON | u8 | 품질 사유 |
| 38 | PROXIMITY_STATUS | u8 | 0 VALID_DISTANCE / 1 NO_OBJECT / 2 UNAVAILABLE / 3 FAULT / 4 RECOVERING |

`REAR_DISTANCE=65535`만으로 NO_OBJECT와 측정 불가를 구분하지 않는다.
`PROXIMITY_STATUS + VALIDITY + QUALITY_REASON`을 함께 사용한다.

---

# 31. M_CIS_STATUS — 0x37

```text
Payload Length = 54 B
```

| Byte | Field | Type | 규칙 |
|---:|---|---|---|
| 0~19 | DOWNSTREAM_CONTEXT | 20 B | §17 |
| 20~23 | CIS_BOOT_ID | u32 | 원 CIS |
| 24~27 | CIS_TX_SEQUENCE | u32 | 원 CAN sequence |
| 28 | CIS_STATE | u8 | 0 STARTUP / 1 READY / 2 ACTIVE / 3 FAULT |
| 29 | VISION_STATUS | u8 | 0 NOT_READY / 1 READY / 2 ACTIVE / 3 FAULT / 4 RECOVERING |
| 30 | TEMPERATURE_STATUS | u8 | 동일 상태 코드 |
| 31 | HUMIDITY_STATUS | u8 | 동일 상태 코드 |
| 32 | ILLUMINANCE_STATUS | u8 | 동일 상태 코드 |
| 33 | REAR_STATUS | u8 | 동일 상태 코드 |
| 34 | PI_UART_STATUS | u8 | 0 READY / 1 UNAVAILABLE / 2 RECOVERING |
| 35 | CAN_INTERFACE_STATUS | u8 | 0 READY / 1 UNAVAILABLE / 2 RECOVERING |
| 36~37 | FAULT_MASK | u16 | CIS bit0~8 |
| 38~39 | ACTIVE_FAULT | u16 | 현재 주요 고장 |
| 40 | RECOVERING | u8 | 0 아니오 / 1 복구 중 |
| 41~42 | LAST_FAULT | u16 | 이번 기동 최근 주요 고장 |
| 43~46 | LAST_FAULT_OCCURRENCE | u32 | 최근 발생 번호 |
| 47 | AFFECTED_FUNCTION | u8 | bit0 vision / bit1 temp / bit2 humidity / bit3 illumination / bit4 rear / bit5 제공 경로 |
| 48~51 | UPDATE_SEQUENCE | u32 | 평가 번호 |
| 52~53 | SOURCE_AGE | u16 | ms |

CIS FAULT_MASK:

```text
bit0 INITIALIZATION_FAILURE
bit1 VISION_FAULT
bit2 TEMPERATURE_SENSOR_FAULT
bit3 HUMIDITY_SENSOR_FAULT
bit4 ILLUMINANCE_SENSOR_FAULT
bit5 PROXIMITY_SENSOR_FAULT
bit6 COMMUNICATION_FAULT
bit7 DATA_INVALID
bit8 OUT_OF_RANGE
```

---

# 32. M_WINDOW_STATE — 0x38

> **[구현 보류 — 설계 유지]**

```text
Payload Length = 42 B
```

| Byte | Field | Type | 규칙 |
|---:|---|---|---|
| 0~19 | DOWNSTREAM_CONTEXT | 20 B | §17 |
| 20~23 | WINDOW_BOOT_ID | u32 | 원 WINDOW |
| 24~27 | WINDOW_TX_SEQUENCE | u32 | 원 CAN sequence |
| 28 | MOTION_STATE | u8 | 0 STOPPED / 1 OPENING / 2 CLOSING / 3 ANTIPINCH_REVERSING / 255 UNKNOWN |
| 29 | WINDOW_ECU_STATE | u8 | 0 INIT / 1 READY / 2 DEGRADED / 3 FAULT |
| 30 | WINDOW_POSITION | u8 | 0~100% closed / 255 unknown |
| 31 | WINDOW_VALUE_QUALITY | u8 | VALUE_QUALITY |
| 32 | FULLY_OPEN_STATE | u8 | 0 아니오 / 1 완전 열림 / 255 unknown |
| 33 | FULLY_CLOSED_STATE | u8 | 0 아니오 / 1 완전 닫힘 / 255 unknown |
| 34 | ANTIPINCH_PROTECTION_STATUS | u8 | 0 대기 / 1 진행 / 2 완료 / 3 중단 |
| 35 | REVERSE_STATE | u8 | 0 아님 / 1 역전 중 / 255 unknown |
| 36~39 | UPDATE_SEQUENCE | u32 | 평가 번호 |
| 40~41 | SOURCE_AGE | u16 | ms |

App parser는 Type을 예약하여 지원할 수 있으나
현재 구현/시연에서 정상 End-to-End 동작을 전제로 하지 않는다.

---

# 33. M_WINDOW_FAULT — 0x39

> **[구현 보류 — 설계 유지]**

```text
Payload Length = 42 B
```

| Byte | Field | Type | 규칙 |
|---:|---|---|---|
| 0~19 | DOWNSTREAM_CONTEXT | 20 B | §17 |
| 20~23 | WINDOW_BOOT_ID | u32 | 원 WINDOW |
| 24~27 | WINDOW_TX_SEQUENCE | u32 | 원 CAN sequence |
| 28 | FAULT_CATEGORY | u8 | 0 없음 / 1 SENSOR / 2 FUNCTION / 3 COMM |
| 29~30 | FAULT_CODE | u16 | WINDOW fault code |
| 31 | FAULT_STATUS | u8 | 0 없음 / 1 현재 고장 / 2 복구 중 |
| 32~35 | OCCURRENCE_ID | u32 | 고장 occurrence |
| 36~39 | UPDATE_SEQUENCE | u32 | 평가 번호 |
| 40~41 | SOURCE_AGE | u16 | ms |

---

# 34. M_VSS_STATUS — 0x3A

```text
Payload Length = 48 B
```

| Byte | Field | Type | 규칙 |
|---:|---|---|---|
| 0~19 | DOWNSTREAM_CONTEXT | 20 B | §17 |
| 20~23 | VSS_BOOT_ID | u32 | 원 VSS |
| 24~27 | VSS_TX_SEQUENCE | u32 | 원 CAN sequence |
| 28 | VSS_STATE | u8 | 0 STARTUP / 1 READY / 2 PLAYING / 3 FAULT |
| 29 | VSS_AVAILABILITY | u8 | 0 FULL / 1 DEGRADED / 2 UNAVAILABLE |
| 30 | VSS_ACCEPTING_EVENTS | u8 | 0 불가 / 1 수용 가능 |
| 31 | VSS_FAULT_ACTIVE | u8 | 0 없음 / 1 현재 고장 |
| 32 | FAULT_MASK | u8 | VSS bit0~4 |
| 33~34 | VSS_LAST_FAULT | u16 | 최근 주요 내부 출력 고장 |
| 35~38 | LAST_FAULT_OCCURRENCE | u32 | 발생 번호 |
| 39~40 | LAST_FAULT_AGE | u16 | ms |
| 41 | LAST_FAULT_QUALITY | u8 | 0 기록 있음 / 3 NO_DATA |
| 42~45 | UPDATE_SEQUENCE | u32 | 평가 번호 |
| 46~47 | SOURCE_AGE | u16 | ms |

VSS FAULT_MASK:

```text
bit0 SOUND_ASSET_UNAVAILABLE
bit1 PLAYBACK_START_FAILURE
bit2 AUDIO_OUTPUT_FAILURE
bit3 PLAYBACK_STATE_FAILURE
bit4 INITIALIZATION_FAILURE
```

---

# 35. M_QUERY_END — 0x40

```text
Payload Length = 24 B
```

| Byte | Field | Type | 규칙 |
|---:|---|---|---|
| 0~19 | DOWNSTREAM_CONTEXT | 20 B | §17 |
| 20 | QUERY_SCOPE | u8 | 원 조회 범위 |
| 21 | QUERY_STATUS | u8 | 0 완료 / 1 요청 없음 / 2 일부 확인 불가 / 3 문맥 거부 |
| 22~23 | ITEM_COUNT | u16 | Query End 제외 응답 개수 |

App은 `M_QUERY_END`만 수신했다고 Query를 완전한 것으로 간주하지 않는다.

다음을 모두 확인한다.

```text
DEVICE_CONTEXT_ID 일치
SESSION_ID 일치
DOMAIN_BOOT_ID 일치
QUERY_ID 일치
수신 메시지 수 == ITEM_COUNT
```

필요한 항목이 빠졌으면 해당 정보는 동기화 완료로 표시하지 않는다.

---

# 36. Query 응답 범위

**[BASE]**

| QUERY_SCOPE | 응답 |
|---:|---|
| 0 현재 상태 | ECU 상태 11종 + M_USER_SETTINGS + M_AVAILABILITY + M_DIGITAL_STATUS + M_QUERY_END |
| 1 요청 결과 | 지정 M_RESULT 또는 최근 최대 8건, 필요 시 마지막 M_DIGITAL_RESULT + M_QUERY_END |
| 2 경고 | 현재 7종 M_WARNING + M_QUERY_END |
| 3 전체 | 상태 14종 + 최근 결과 최대 8건 + 현재 경고 7종 + 마지막 자동 Unlock 결과 최대 1건 + M_QUERY_END |

WINDOW가 현재 구현 보류여도 기존 계약의 Type/조회 구조를 임의로 재정의하지 않는다.
실제 미구현 구성의 WINDOW 응답/표시 세부는 팀 확인 대상이다.

---

# 37. M_DOMAIN_ALIVE — ESP32 내부 사용

**[BASE]**

```text
MESSAGE_TYPE = 0x02
Domain → ESP32
```

App으로 그대로 Notify하지 않는다.

UART Payload:

| Byte | Field | Type |
|---:|---|---|
| 0~3 | DOMAIN_BOOT_ID | u32 |
| 4~7 | UPDATE_SEQUENCE | u32 |
| 8~9 | SOURCE_AGE | u16 |
| 10 | DOMAIN_STATE | u8 |
| 11 | VALUE_QUALITY | u8 |

ESP32는 이를 이용해:

```text
DOMAIN_LINK_STATE
DOMAIN_BOOT_ID
DOMAIN_STATE
SESSION_READY
```

를 관리한다.

수신 중단 기준:

```text
M_DOMAIN_ALIVE period = 250 ms
Link timeout = 750 ms
```

750 ms 동안 정상 M_DOMAIN_ALIVE가 없으면
새 차량 요청 허용을 중단한다.

---

# 38. App 표시 갱신 / Timeout

**[BASE]**

App은 각 메시지의 마지막 **정상 형식 완성 메시지** 수신 시점을 기준으로 갱신을 감시한다.

| Type | 기본 갱신 | App 수신 중단 기준 |
|---|---:|---:|
| M_BCM_DOOR_STATE | 200 ms | 600 ms |
| M_BCM_CLIMATE_STATE | 200 ms | 600 ms |
| M_BCM_LIGHT_STATE | 200 ms | 600 ms |
| M_CIS_ENVIRONMENT | 200 ms | 600 ms |
| M_CIS_OCCUPANT | 200 ms | 600 ms |
| M_CIS_REAR | 200 ms | 600 ms |
| M_WINDOW_STATE | 200 ms | 600 ms — 구현 보류 |
| M_BCM_STATUS | 1000 ms | 3000 ms |
| M_CIS_STATUS | 1000 ms | 3000 ms |
| M_WINDOW_FAULT | 1000 ms | 3000 ms — 구현 보류 |
| M_VSS_STATUS | 1000 ms | 3000 ms |
| M_AVAILABILITY | 1000 ms | 3000 ms |
| M_DIGITAL_STATUS | 1000 ms | 3000 ms |
| M_USER_SETTINGS | 1000 ms | 3000 ms |
| M_WARNING 각 Type | 1000 ms 보완 | 3000 ms |

수신 중단 시:

```text
마지막 값 그대로 "정상"으로 유지 금지
→ 표시를 확인 불가/오래됨 상태로 전환
→ 필요 시 M_QUERY로 재확인
```

Domain이 제공한 `VALUE_QUALITY`, `VALIDITY`, `SOURCE_AGE` 의미를 App에서 임의로 정상화하지 않는다.

---

# 39. Request Result 관측

**[BASE]**

App의 첫 Result 관측 기준:

```text
약 1 s
```

1초 동안 Result가 없다고 `FAILED`로 만들지 않는다.

```text
M_REQUEST
    ↓
1 s
    ↓
Result 없음
    ↓
UNKNOWN / pending
    ↓
M_QUERY(REQUEST RESULT)
```

Result 조회로도 Domain이 확인하지 못한 ECU 최종 결과를 새로 복구할 수 있다고 가정하지 않는다.

---

# 40. Reconnect / Restart

## 40.1 BLE disconnect

**[BASE + BLE-DECISION]**

disconnect 시:

```text
BT_CONNECTION_STATE = DISCONNECTED
진행 중 BLE reassembly 폐기
아직 Domain으로 전달되지 않은 App Command 폐기
새 요청 보류 저장 금지
```

재연결 시 기존 미완료 요청을 자동 재전송하지 않는다.

## 40.2 Domain restart

Domain `BOOT_ID` 변경 감지 시:

```text
SESSION_READY = 0
기존 request/query/reassembly context 폐기
새 SESSION_ID 생성
M_CONTEXT 갱신
현재 상태/경고 Query
현재 문맥 M_QUERY_END 확인
SESSION_READY = 1
```

## 40.3 ESP32 restart

```text
기존 BLE/UART pending 없음
새 SESSION_ID 생성
등록 / Domain link / Query 문맥 재확인
```

## 40.4 App restart

새 `APP_INSTANCE_ID`를 받으면:

```text
이전 App pending context 폐기
새 SESSION_ID 생성
REQUEST_ID 새 번호공간 허용
문맥 재확인
```

---

# 41. BLE Write Validation / ATT Error

**[BLE-DECISION]**

`App Command`, `App State`는 Write With Response를 사용한다.

GATT Write 성공은:

```text
ESP32 BLE layer가 해당 write를 수용함
```

만 의미한다.

차량 Result가 아니다.

## 41.1 Custom ATT Application Error

| ATT Error | 이름 | 조건 |
|---:|---|---|
| `0x80` | BLE_VERSION_UNSUPPORTED | BLE_PROTOCOL_VERSION != 1 |
| `0x81` | MESSAGE_TYPE_UNSUPPORTED | App Command에서 0x10/0x11/0x12 외 Type |
| `0x82` | FRAGMENT_INVALID | Index/Count/Transfer 조합 오류 |
| `0x83` | PAYLOAD_LENGTH_INVALID | Type 고정 길이 불일치 |
| `0x84` | DOMAIN_LINK_DOWN | 필요한 Domain link 없음 |
| `0x85` | SESSION_NOT_READY | M_REQUEST 허용 문맥 아님 |
| `0x86` | NOT_REGISTERED | 등록 문맥 없음 |
| `0x87` | QUERY_BUSY | 다른 Query 진행 중 |
| `0x88` | VALUE_INVALID | 예약값/필수 enum/RESERVED 오류 |
| `0x89` | NOTIFY_NOT_SUBSCRIBED | Vehicle Data Notify 미구독 |

M_QUERY의 Session 확인 목적 사용은 `SESSION_READY=0`에서도 허용할 수 있다.
M_REQUEST는 Ready가 아니면 실행하지 않는다.

---

# 42. BLE TX Scheduling

**[BASE 원칙 + BLE-DECISION 적용]**

송신 데이터는 모두 동일한 저장 의미를 가지지 않는다.

## 42.1 Discrete / 보존 필요

silent overwrite 금지:

```text
M_RESULT
M_DIGITAL_RESULT
M_QUERY_END
Query-scoped response
```

## 42.2 Replaceable current state

아직 송신을 시작하지 않은 같은 semantic key의 pending current state는 최신값으로 교체할 수 있다.

예:

```text
M_BCM_DOOR_STATE
M_BCM_CLIMATE_STATE
M_CIS_ENVIRONMENT
M_AVAILABILITY
unsolicited M_WARNING current update
```

## 42.3 Query-scoped ordered response

`QUERY_ID != 0`인 응답은 해당 Query 순서/개수를 유지한다.

동일 Type의 unsolicited 최신 상태가
진행 중 Query response를 덮어쓰지 않는다.

---

# 43. M_CONTEXT 생성 규칙

**[BASE]**

ESP32 → Domain:

```text
MESSAGE_TYPE = 0x01
Payload = 31 B
주기 = 100 ms + 변경 시
```

Payload:

| Byte | Field |
|---:|---|
| 0~3 | DEVICE_CONTEXT_ID |
| 4~11 | SESSION_ID |
| 12 | DEVICE_REGISTRATION_STATE |
| 13 | BT_CONNECTION_STATE |
| 14 | APP_ACTIVE_STATE |
| 15 | APP_QUALITY |
| 16~19 | APP_UPDATE_SEQUENCE |
| 20~21 | APP_AGE |
| 22 | PROXIMITY_STATE |
| 23 | PROXIMITY_VALIDITY |
| 24 | PROXIMITY_REASON |
| 25~28 | PROXIMITY_UPDATE_SEQUENCE |
| 29~30 | PROXIMITY_AGE |

BLE 연결 성공:

```text
BT_CONNECTION_STATE = CONNECTED
```

BLE disconnect:

```text
BT_CONNECTION_STATE = DISCONNECTED
```

App State write는 APP_ACTIVE 관련 필드를 갱신한다.

---

# 44. Proximity / RSSI

**[BASE + TBD]**

Network contract는 ESP32가 RSSI 기반 근접 문맥을 제공하도록 하지만
실제 RSSI 임계값은 확정하지 않았다.

Payload 의미:

```text
PROXIMITY_STATE
0 FAR
1 NEAR
255 UNKNOWN

PROXIMITY_VALIDITY
0 INVALID
1 VALID
```

**[TBD-PROX-01]**

확정 전에는 임의 RSSI threshold를 production constant로 사용하지 않는다.

권장 안전 동작:

```text
검증된 threshold 없음
→ PROXIMITY_VALIDITY = INVALID
→ Digital Key auto unlock의 근접 조건으로 사용 금지
```

---

# 45. Security Profile

## 45.1 Integration Profile

**[BLE-DECISION]**

본 v0.2는 **Bench / Integration 구현 계약**을 우선한다.

현재 프로토콜 기능 검증 단계에서는 BLE Application Layer 인증 방식을 정의하지 않는다.

## 45.2 Production

**[TBD-SEC-01]**

차량 제어 Production 사용 전 다음을 별도 확정해야 한다.

- Pairing
- Bonding
- BLE Link Encryption
- Device registration
- App authentication
- Key provisioning
- Bond 삭제 / 재등록
- Session authorization
- replay 대응
- lost phone 대응

이 항목이 확정되기 전 본 문서를 Production 보안 승인으로 간주하지 않는다.

---

# 46. App Connection State Machine

**[BLE-DECISION]**

```text
DISCONNECTED
    │
    ▼
CONNECTED
    │ Service Discovery
    ▼
DISCOVERED
    │ CCCD enable
    ▼
SUBSCRIBED
    │ App State write
    ▼
CONTEXT_WAIT
    │ Gateway Status
    ▼
QUERY_SYNC
    │ M_QUERY
    │ response...
    │ M_QUERY_END
    ▼
READY
```

READY 조건 권장:

```text
REGISTRATION_STATE == REGISTERED
DOMAIN_LINK_STATE == UP
SESSION_READY == READY
Initial Query complete
필요 기능 Availability 허용
```

---

# 47. ESP32 Processing State Machine

```text
BLE Connect
    ↓
BT_CONNECTION_STATE = CONNECTED
    ↓
App State / APP_INSTANCE_ID 수신
    ↓
Context 생성/유지
    ↓
M_CONTEXT 주기 송신
    ↓
Domain link + Session 확인
    ↓
SESSION_READY
```

App Command:

```text
Write Fragment
    ↓
Header validation
    ↓
Reassembly
    ↓
Type / Length / Field validation
    ↓
Context prepend
    ↓
UART M_* encode
    ↓
UART Framer / CRC
    ↓
Domain
```

Domain RX:

```text
UART byte stream
    ↓
SYNC / Length / CRC / Type validation
    ↓
typed M_* message
    ↓
M_DOMAIN_ALIVE ?
  ├─ yes → Gateway internal state
  └─ no  → App-target message?
              ↓
         BLE Header
              ↓
         fragmentation
              ↓
         Vehicle Data Notify
```

---

# 48. Implementation API 제안

> **[BLE-DECISION]** 내부 함수명은 구현 편의를 위한 제안이며 wire contract가 아니다.

ESP32:

```c
typedef enum {
    BLE_MSG_M_REQUEST       = 0x10,
    BLE_MSG_M_QUERY         = 0x11,
    BLE_MSG_M_WARNING_ACK   = 0x12,

    BLE_MSG_M_RESULT        = 0x20,
    BLE_MSG_M_WARNING       = 0x21,
    BLE_MSG_M_AVAILABILITY  = 0x22,
    BLE_MSG_M_DIGITAL_STATUS= 0x23,
    BLE_MSG_M_DIGITAL_RESULT= 0x24,
    BLE_MSG_M_USER_SETTINGS = 0x25,

    BLE_MSG_M_BCM_DOOR_STATE    = 0x30,
    BLE_MSG_M_BCM_CLIMATE_STATE = 0x31,
    BLE_MSG_M_BCM_LIGHT_STATE   = 0x32,
    BLE_MSG_M_BCM_STATUS        = 0x33,
    BLE_MSG_M_CIS_ENVIRONMENT   = 0x34,
    BLE_MSG_M_CIS_OCCUPANT      = 0x35,
    BLE_MSG_M_CIS_REAR          = 0x36,
    BLE_MSG_M_CIS_STATUS        = 0x37,
    BLE_MSG_M_WINDOW_STATE      = 0x38,
    BLE_MSG_M_WINDOW_FAULT      = 0x39,
    BLE_MSG_M_VSS_STATUS        = 0x3A,

    BLE_MSG_M_QUERY_END      = 0x40,
} mobile_msg_type_t;
```

권장 API:

```c
bool ble_gateway_init(void);

bool ble_vehicle_notify(
    uint8_t message_type,
    const uint8_t *payload,
    uint16_t payload_len);

bool gateway_handle_app_message(
    uint8_t message_type,
    const uint8_t *payload,
    uint16_t payload_len);

bool uart_domain_send_typed(
    uint8_t message_type,
    const uint8_t *payload,
    uint16_t payload_len);

void gateway_on_domain_message(
    uint8_t message_type,
    const uint8_t *payload,
    uint16_t payload_len);
```

Callback에서는 긴 parsing/라우팅을 하지 않고 bounded queue/worker task로 넘기는 구조를 권장한다.

---

# 49. Byte Encode / Decode 규칙

wire struct cast 금지 권장.

예:

```c
static uint16_t get_u16_le(const uint8_t *p)
{
    return (uint16_t)p[0]
         | ((uint16_t)p[1] << 8);
}

static uint32_t get_u32_le(const uint8_t *p)
{
    return (uint32_t)p[0]
         | ((uint32_t)p[1] << 8)
         | ((uint32_t)p[2] << 16)
         | ((uint32_t)p[3] << 24);
}
```

송신도 field별 명시 encode를 사용한다.

이렇게 하면:

- compiler padding
- alignment
- endian
- packed struct 의존

문제를 줄일 수 있다.

---

# 50. End-to-End 예시 — Door Unlock

App:

```text
REQUEST_ID = 52
REQUEST_KIND = DOOR(1)
OPERATION = UNLOCK(1)
ARG0~5 = 0
```

M_REQUEST BLE semantic payload:

```text
34 00 00 00
01
01
00 00 00 00 00 00
```

BLE:

```text
[BLE Header TYPE=0x10][12 B payload]
```

ESP32 현재 문맥:

```text
DEVICE_CONTEXT_ID = 7
SESSION_ID = 0x1122334455667788
```

UART M_REQUEST payload:

```text
07 00 00 00
88 77 66 55 44 33 22 11
34 00 00 00
01
01
00 00 00 00 00 00
```

ESP32 UART stack:

```text
A5 5A
01
10
18 00
LINK_SEQUENCE
[24 B payload]
CRC16
```

Domain 처리 후 M_RESULT:

```text
REQUEST_SESSION = 0x1122334455667788
REQUEST_ID = 52
RESULT = DONE
RESULT_REASON = NONE
RESULT_CONFIRMATION = CONFIRMED
```

ESP32는 M_RESULT의 의미를 수정하지 않고 Vehicle Data `0x20`으로 Notify한다.

App은:

```text
REQUEST_SESSION
+
REQUEST_ID
```

로 자신이 보낸 Door Unlock과 연결한다.

---

# 51. End-to-End 예시 — Current State Query

App:

```text
QUERY_ID = 100
QUERY_SCOPE = CURRENT_STATE(0)
REQUEST_SESSION = 0
REQUEST_ID = 0
CURSOR = 0
```

BLE `0x11 M_QUERY` 전송.

ESP32가 현재 Context를 추가해 UART M_QUERY 전송.

Domain 응답:

```text
M_BCM_DOOR_STATE
M_BCM_CLIMATE_STATE
...
M_USER_SETTINGS
M_AVAILABILITY
M_DIGITAL_STATUS
M_QUERY_END
```

각 응답:

```text
QUERY_ID = 100
```

App은 다른 `QUERY_ID` 또는 unsolicited `QUERY_ID=0` 데이터를
Query 100의 응답 개수에 포함하지 않는다.

마지막:

```text
M_QUERY_END
QUERY_ID = 100
ITEM_COUNT = N
```

App의 Query 100 수신 메시지 개수가 N과 일치해야 완료다.

---

# 52. Integration Test Checklist

## 52.1 BLE 기본

- [ ] `VEHICLE-GW` 발견
- [ ] Service UUID 발견
- [ ] 4개 Characteristic 발견
- [ ] Gateway Status READ 성공
- [ ] Gateway Status Notify 성공
- [ ] Vehicle Data Notify subscribe 성공
- [ ] App State write 성공
- [ ] App Command write response 성공

## 52.2 Fragmentation

- [ ] MTU 185 단일 fragment
- [ ] 낮은 MTU에서 multi-fragment
- [ ] Fragment 누락
- [ ] 잘못된 index
- [ ] 길이 불일치
- [ ] 1 s reassembly timeout
- [ ] 연속 message ordering

## 52.3 Request

- [ ] Door LOCK
- [ ] Door UNLOCK
- [ ] Climate target
- [ ] Climate AUTO
- [ ] Fan level
- [ ] Interior light ON/OFF
- [ ] Brightness
- [ ] RGB
- [ ] Digital Key setting

## 52.4 Result

- [ ] ACCEPTED
- [ ] IN_PROGRESS
- [ ] DONE
- [ ] REJECTED
- [ ] CANCELLED
- [ ] FAILED
- [ ] UNCONFIRMED
- [ ] late ACCEPTED ignored after confirmed final result

## 52.5 Query

- [ ] Current State
- [ ] Request Result
- [ ] Warning
- [ ] Full
- [ ] QUERY_ID mismatch
- [ ] ITEM_COUNT mismatch
- [ ] M_QUERY_END missing
- [ ] query while another query active

## 52.6 Reconnect / Restart

- [ ] BLE disconnect
- [ ] BLE reconnect
- [ ] App restart / APP_INSTANCE_ID change
- [ ] ESP32 restart
- [ ] Domain BOOT_ID change
- [ ] old pending command not replayed
- [ ] new Session created when required

## 52.7 Timeout / display

- [ ] 200 ms class message 600 ms loss
- [ ] 1000 ms class message 3000 ms loss
- [ ] request 1 s first-result timeout → Query
- [ ] Domain alive 750 ms timeout
- [ ] invalid Quality does not display as normal

---

# 53. Open Items / Blocking Decisions

## TBD-REG-01 Registration

결정 필요:

```text
DEVICE_CONTEXT_ID 발급
등록 저장소
재등록 절차
```

## TBD-PROX-01 RSSI Proximity

결정 필요:

```text
FAR / NEAR threshold
hysteresis
sample window
loss handling
```

확정 전 Digital Key proximity 자동 기능에 사용하지 않는다.

## TBD-SEC-01 Security

결정 필요:

```text
Pairing / Bonding
Encryption
App authentication
Key lifecycle
```

## TBD-WINDOW-01

WINDOW는 기존 계약 유지 / 구현 보류.

App은 parser/type 예약을 유지하되
실제 WINDOW End-to-End 동작을 완료된 기능으로 취급하지 않는다.

---

# 54. Source Traceability

본 문서가 승계한 핵심 Network contract:

### `network_sw_architecture.md`

- App ↔ ESP32 = BLE
- ESP32 ↔ Domain = UART-M
- ESP32 = UART-M endpoint / SESSION_READY 판단 / App 중계
- raw frame 대신 typed message/evidence 전달
- callback 최소 작업
- parsing/codec/scheduling은 task context
- transport result와 vehicle result 분리
- restart/session 변경 시 old pending 정리

### `network_design_draft_v0.1.md`

- UART-M 115200 / 8N1 / Full Duplex
- UART framing / CRC / Type / Payload
- DEVICE_CONTEXT_ID / SESSION_ID
- M_CONTEXT
- M_REQUEST
- M_QUERY
- M_WARNING_ACK
- M_RESULT
- M_WARNING
- M_AVAILABILITY
- M_DIGITAL_STATUS
- M_DIGITAL_RESULT
- M_USER_SETTINGS
- M_BCM_* State
- M_CIS_* State
- M_WINDOW_* reserved
- M_VSS_STATUS
- M_QUERY_END
- Query / Result / restart / timeout 규칙
- App–ESP32 GATT/MTU/fragmentation 상세는 ESP32/MOBILE 담당

---

# 55. v0.2 구현 결론

이 인터페이스의 핵심 경계는 다음과 같다.

```text
App
  │
  │ BLE-specific framing
  ▼
ESP32
  │
  ├─ BLE transport 해제
  ├─ Session / Context 관리
  ├─ semantic message 유지
  └─ UART framing 적용
  │
  ▼
Domain
```

공유해야 하는 것은:

```text
MESSAGE_TYPE
Payload Layout
Request / Query / Result identity
Session / Context
Quality / Availability / Result 의미
```

각 링크에서 독립적인 것은:

```text
BLE_TRANSFER_ID
BLE Fragment
ATT MTU
GATT UUID

UART LINK_SEQUENCE
UART SYNC
UART CRC16
```

따라서:

```text
BLE Write 성공 != Vehicle ACCEPTED
UART TX 성공  != Vehicle DONE
Notify 성공   != Vehicle 기능 성공
```

차량 기능 결과의 owner는 Domain/각 기능 ECU이며,
ESP32와 App은 기존 Result contract를 보존한다.
