# ESP32 — Gateway_Types.h 전체 의미 타입

Interface v2 · r1 · [규격과 연동 조건](../../07_CROSS_NODE_INTERFACE_RULES.md#status)

**DECLARED_SOURCE / BINDING_OPEN.** 아래 code block은 동결한 기존 Interface의 전체 선언이다. 참고 Gateway 헤더 사본과 문서의 차이는 확인했으나 실제 채택 공통 헤더·Commit은 확정되지 않았다. 따라서 실채택 헤더와 동일하다고 단정하지 않는다. 설명 표는 원 생성자·유효성·수명을 명시한다. 이 문서는 컴파일용 헤더가 아니며 C enum/padding을 Wire로 복사하지 않는다.

[노드 Data](10_DATA.md) · [노드 API](20_API.md) · [이 문서의 전체 기준 선언](#타입-바로-찾기)

헤더 출처 확인 보류 헤더 연결: 저장소의 Gateway_Types 헤더에는 `Gateway_UpdateBasis_t.update_sequence`가 없지만 기존 Interface에는 있다. 최신 헤더를 임의로 채택하거나 이 필드를 삭제하지 않고 기존 선언을 유지한다. ESP32 모듈은 사용할 공통 헤더와 UART 변환 경계를 대조해야 한다. 아래 전체 선언은 문서 계약이며 컴파일 헤더를 새로 만든 것이 아니다.

## 타입 바로 찾기

| 사용하는 의미 | 전체 선언·필드 표 |
|---|---|
| 반환·링크 | [`Gateway_Status_t`](#type-gateway-status-t), [`Gateway_LinkId_t`](#type-gateway-linkid-t), [`Gateway_LinkState_t`](#type-gateway-linkstate-t) |
| 등록·연결·활성·근접 값 | [`Gateway_RegistrationState_t`](#type-gateway-registrationstate-t), [`Gateway_ConnectionState_t`](#type-gateway-connectionstate-t), [`Gateway_AppActivityState_t`](#type-gateway-appactivitystate-t), [`Gateway_ProximityState_t`](#type-gateway-proximitystate-t) |
| 품질·독립 관측 | [`Gateway_DataQuality_t`](#type-gateway-dataquality-t), [`Gateway_UpdateBasis_t`](#type-gateway-updatebasis-t), [`Gateway_RegistrationInfo_t`](#type-gateway-registrationinfo-t), [`Gateway_ConnectionInfo_t`](#type-gateway-connectioninfo-t), [`Gateway_AppActivityInfo_t`](#type-gateway-appactivityinfo-t), [`Gateway_BluetoothContext_t`](#type-gateway-bluetoothcontext-t), [`Gateway_ProximityInfo_t`](#type-gateway-proximityinfo-t) |
| 논리 중계 분류 | [`Gateway_LogicalMessageType_t`](#type-gateway-logicalmessagetype-t), [`Gateway_MessageDirection_t`](#type-gateway-messagedirection-t) |

<a id="type-gateway-status-t"></a>
## `Gateway_Status_t`

**원 소속:** `Gateway_Types.h` · **지위:** DECLARED_SOURCE · **소비:** [노드 API](20_API.md).

```c
typedef enum
{
    GATEWAY_STATUS_OK = 0,
    GATEWAY_STATUS_INVALID_ARGUMENT,
    GATEWAY_STATUS_NOT_READY,
    GATEWAY_STATUS_UNSUPPORTED,
    GATEWAY_STATUS_INTERNAL_ERROR
} Gateway_Status_t;
```

| C 열거자 | 실제 C 정수 | 유효 조건 |
|---|---:|---|
| `GATEWAY_STATUS_OK` | 0 | 이 enum 의미로만 소비. raw code는 명시 mapping 후 사용. |
| `GATEWAY_STATUS_INVALID_ARGUMENT` | 1 | 이 enum 의미로만 소비. raw code는 명시 mapping 후 사용. |
| `GATEWAY_STATUS_NOT_READY` | 2 | 이 enum 의미로만 소비. raw code는 명시 mapping 후 사용. |
| `GATEWAY_STATUS_UNSUPPORTED` | 3 | 이 enum 의미로만 소비. raw code는 명시 mapping 후 사용. |
| `GATEWAY_STATUS_INTERNAL_ERROR` | 4 | 이 enum 의미로만 소비. raw code는 명시 mapping 후 사용. |

<a id="type-gateway-linkid-t"></a>
## `Gateway_LinkId_t`

**원 소속:** `Gateway_Types.h` · **지위:** DECLARED_SOURCE · **소비:** [노드 API](20_API.md).

```c
typedef enum
{
    GATEWAY_LINK_BLUETOOTH = 0,
    GATEWAY_LINK_DOMAIN_UART
} Gateway_LinkId_t;
```

| C 열거자 | 실제 C 정수 | 유효 조건 |
|---|---:|---|
| `GATEWAY_LINK_BLUETOOTH` | 0 | 이 enum 의미로만 소비. raw code는 명시 mapping 후 사용. |
| `GATEWAY_LINK_DOMAIN_UART` | 1 | 이 enum 의미로만 소비. raw code는 명시 mapping 후 사용. |

<a id="type-gateway-linkstate-t"></a>
## `Gateway_LinkState_t`

**원 소속:** `Gateway_Types.h` · **지위:** DECLARED_SOURCE · **소비:** [노드 API](20_API.md).

```c
typedef enum
{
    GATEWAY_LINK_STATE_UNKNOWN = 0,
    GATEWAY_LINK_STATE_UNAVAILABLE,
    GATEWAY_LINK_STATE_AVAILABLE,
    GATEWAY_LINK_STATE_RECOVERING
} Gateway_LinkState_t;
```

| C 열거자 | 실제 C 정수 | 유효 조건 |
|---|---:|---|
| `GATEWAY_LINK_STATE_UNKNOWN` | 0 | 이 enum 의미로만 소비. raw code는 명시 mapping 후 사용. |
| `GATEWAY_LINK_STATE_UNAVAILABLE` | 1 | 이 enum 의미로만 소비. raw code는 명시 mapping 후 사용. |
| `GATEWAY_LINK_STATE_AVAILABLE` | 2 | 이 enum 의미로만 소비. raw code는 명시 mapping 후 사용. |
| `GATEWAY_LINK_STATE_RECOVERING` | 3 | 이 enum 의미로만 소비. raw code는 명시 mapping 후 사용. |

<a id="type-gateway-registrationstate-t"></a>
## `Gateway_RegistrationState_t`

**원 소속:** `Gateway_Types.h` · **지위:** DECLARED_SOURCE · **소비:** [노드 API](20_API.md).

```c
typedef enum
{
    GATEWAY_REGISTRATION_UNKNOWN = 0,
    GATEWAY_NOT_REGISTERED,
    GATEWAY_REGISTERED
} Gateway_RegistrationState_t;
```

| C 열거자 | 실제 C 정수 | 유효 조건 |
|---|---:|---|
| `GATEWAY_REGISTRATION_UNKNOWN` | 0 | 이 enum 의미로만 소비. raw code는 명시 mapping 후 사용. |
| `GATEWAY_NOT_REGISTERED` | 1 | 이 enum 의미로만 소비. raw code는 명시 mapping 후 사용. |
| `GATEWAY_REGISTERED` | 2 | 이 enum 의미로만 소비. raw code는 명시 mapping 후 사용. |

<a id="type-gateway-connectionstate-t"></a>
## `Gateway_ConnectionState_t`

**원 소속:** `Gateway_Types.h` · **지위:** DECLARED_SOURCE · **소비:** [노드 API](20_API.md).

```c
typedef enum
{
    GATEWAY_CONNECTION_UNKNOWN = 0,
    GATEWAY_DISCONNECTED,
    GATEWAY_CONNECTED
} Gateway_ConnectionState_t;
```

| C 열거자 | 실제 C 정수 | 유효 조건 |
|---|---:|---|
| `GATEWAY_CONNECTION_UNKNOWN` | 0 | 이 enum 의미로만 소비. raw code는 명시 mapping 후 사용. |
| `GATEWAY_DISCONNECTED` | 1 | 이 enum 의미로만 소비. raw code는 명시 mapping 후 사용. |
| `GATEWAY_CONNECTED` | 2 | 이 enum 의미로만 소비. raw code는 명시 mapping 후 사용. |

<a id="type-gateway-appactivitystate-t"></a>
## `Gateway_AppActivityState_t`

**원 소속:** `Gateway_Types.h` · **지위:** DECLARED_SOURCE · **소비:** [노드 API](20_API.md).

```c
typedef enum
{
    GATEWAY_APP_ACTIVITY_UNKNOWN = 0,
    GATEWAY_APP_INACTIVE,
    GATEWAY_APP_ACTIVE
} Gateway_AppActivityState_t;
```

| C 열거자 | 실제 C 정수 | 유효 조건 |
|---|---:|---|
| `GATEWAY_APP_ACTIVITY_UNKNOWN` | 0 | 이 enum 의미로만 소비. raw code는 명시 mapping 후 사용. |
| `GATEWAY_APP_INACTIVE` | 1 | 이 enum 의미로만 소비. raw code는 명시 mapping 후 사용. |
| `GATEWAY_APP_ACTIVE` | 2 | 이 enum 의미로만 소비. raw code는 명시 mapping 후 사용. |

<a id="type-gateway-proximitystate-t"></a>
## `Gateway_ProximityState_t`

**원 소속:** `Gateway_Types.h` · **지위:** DECLARED_SOURCE · **소비:** [노드 API](20_API.md).

```c
typedef enum
{
    GATEWAY_PROXIMITY_UNKNOWN = 0,
    GATEWAY_PROXIMITY_FAR,
    GATEWAY_PROXIMITY_NEAR
} Gateway_ProximityState_t;
```

| C 열거자 | 실제 C 정수 | 유효 조건 |
|---|---:|---|
| `GATEWAY_PROXIMITY_UNKNOWN` | 0 | 이 enum 의미로만 소비. raw code는 명시 mapping 후 사용. |
| `GATEWAY_PROXIMITY_FAR` | 1 | 이 enum 의미로만 소비. raw code는 명시 mapping 후 사용. |
| `GATEWAY_PROXIMITY_NEAR` | 2 | 이 enum 의미로만 소비. raw code는 명시 mapping 후 사용. |

<a id="type-gateway-dataquality-t"></a>
## `Gateway_DataQuality_t`

**원 소속:** `Gateway_Types.h` · **지위:** DECLARED_SOURCE · **소비:** [노드 API](20_API.md).

```c
typedef enum
{
    GATEWAY_DATA_QUALITY_UNKNOWN = 0,
    GATEWAY_DATA_QUALITY_VALID,
    GATEWAY_DATA_QUALITY_INVALID,
    GATEWAY_DATA_QUALITY_NO_DATA,
    GATEWAY_DATA_QUALITY_STALE
} Gateway_DataQuality_t;
```

| C 열거자 | 실제 C 정수 | 유효 조건 |
|---|---:|---|
| `GATEWAY_DATA_QUALITY_UNKNOWN` | 0 | 이 enum 의미로만 소비. raw code는 명시 mapping 후 사용. |
| `GATEWAY_DATA_QUALITY_VALID` | 1 | 이 enum 의미로만 소비. raw code는 명시 mapping 후 사용. |
| `GATEWAY_DATA_QUALITY_INVALID` | 2 | 이 enum 의미로만 소비. raw code는 명시 mapping 후 사용. |
| `GATEWAY_DATA_QUALITY_NO_DATA` | 3 | 이 enum 의미로만 소비. raw code는 명시 mapping 후 사용. |
| `GATEWAY_DATA_QUALITY_STALE` | 4 | 이 enum 의미로만 소비. raw code는 명시 mapping 후 사용. |

<a id="type-gateway-updatebasis-t"></a>
## `Gateway_UpdateBasis_t`

<a id="r1-updatebasis-binding"></a>
**H01 권장안 / PROPOSED_DESIGN:** 문서의 u32 `update_sequence`를 유지하고 실채택 ESP32 공통 Header에 해당 필드를 반영하는 방식을 기본 권고로 한다. 기존 Header를 유지해야 한다면 생산원별 독립 순번을 보존하는 Adapter 대안도 가능하다. 등록·연결 공동 순번, App 활성 순번, 근접 순번을 각각 추적하며 `is_new_update` bool·LINK_SEQUENCE·로컬 시각으로 대신하지 않는다. 실제 적용 Header 버전, 세 생산원→M_CONTEXT Encoder의 변환, Golden Vector byte 일치, 재기동/반복 경계, 전체 빌드·ABI 호환 근거가 확보되기 전에는 **HOLD_BINDING / BINDING_NOT_VERIFIED**다. 이 권장안의 실제 헤더·ABI·구현 연결은 검증 전이다. 공개 C 선언과 UART 필드는 아래 정의를 따른다.

**원 소속:** `Gateway_Types.h` · **지위:** DECLARED_SOURCE · **소비:** [노드 API](20_API.md).

```c
typedef struct
{
    Gateway_DataQuality_t quality;
    uint32_t age_ms;
    uint32_t update_sequence;
    bool is_new_update;
} Gateway_UpdateBasis_t;
```

| 멤버 | 정확한 C 타입 | 역할·범위·유효성 | 원 생성자 |
|---|---|---|---|
| `quality` | [`Gateway_DataQuality_t`](11_GATEWAY_TYPES.md#type-gateway-dataquality-t) | 이 값의 품질. OK/STALE/INVALID/NO_DATA를 정상 값/위험 해제 여부와 분리한다. | ESP32 Gateway / 해당 BLE·등록·link owner |
| `age_ms` | `uint32_t` | ESP32 원 관측/평가 후 u32 ms. cached retransmission 에서 증가하며 0으로 리셋 금지. | ESP32 Gateway / 해당 BLE·등록·link owner |
| `update_sequence` | `uint32_t` | ESP32 각 원 생산자의 새 관측/평가 번호. 등록·연결 공동/앱 활성/근접은 별도 owner·번호. | ESP32 Gateway / 해당 BLE·등록·link owner |
| `is_new_update` | `bool` | 로컬 새 관측 helper bool. 순번 대체값이 아니며 Wire 필드가 아니다. | ESP32 Gateway / 해당 BLE·등록·link owner |

<a id="type-gateway-registrationinfo-t"></a>
## `Gateway_RegistrationInfo_t`

**원 소속:** `Gateway_Types.h` · **지위:** DECLARED_SOURCE · **소비:** [노드 API](20_API.md).

```c
typedef struct
{
    Gateway_RegistrationState_t state;
    Gateway_UpdateBasis_t update;
} Gateway_RegistrationInfo_t;
```

| 멤버 | 정확한 C 타입 | 역할·범위·유효성 | 원 생성자 |
|---|---|---|---|
| `state` | [`Gateway_RegistrationState_t`](11_GATEWAY_TYPES.md#type-gateway-registrationstate-t) | 해당 타입의 의미 상태. 타입별 enum의 UNKNOWN/UNCONFIRMED/STALE를 정상 CLEAR/FAR로 만들지 않는다. | ESP32 Gateway / 해당 BLE·등록·link owner |
| `update` | [`Gateway_UpdateBasis_t`](11_GATEWAY_TYPES.md#type-gateway-updatebasis-t) | 해당 Gateway 관측의 quality/age_ms/update_sequence. 타입 공유는 카운터 공유가 아니다. | ESP32 Gateway / 해당 BLE·등록·link owner |

<a id="type-gateway-connectioninfo-t"></a>
## `Gateway_ConnectionInfo_t`

**원 소속:** `Gateway_Types.h` · **지위:** DECLARED_SOURCE · **소비:** [노드 API](20_API.md).

```c
typedef struct
{
    Gateway_ConnectionState_t state;
    Gateway_UpdateBasis_t update;
} Gateway_ConnectionInfo_t;
```

| 멤버 | 정확한 C 타입 | 역할·범위·유효성 | 원 생성자 |
|---|---|---|---|
| `state` | [`Gateway_ConnectionState_t`](11_GATEWAY_TYPES.md#type-gateway-connectionstate-t) | 해당 타입의 의미 상태. 타입별 enum의 UNKNOWN/UNCONFIRMED/STALE를 정상 CLEAR/FAR로 만들지 않는다. | ESP32 Gateway / 해당 BLE·등록·link owner |
| `update` | [`Gateway_UpdateBasis_t`](11_GATEWAY_TYPES.md#type-gateway-updatebasis-t) | 해당 Gateway 관측의 quality/age_ms/update_sequence. 타입 공유는 카운터 공유가 아니다. | ESP32 Gateway / 해당 BLE·등록·link owner |

<a id="type-gateway-appactivityinfo-t"></a>
## `Gateway_AppActivityInfo_t`

**원 소속:** `Gateway_Types.h` · **지위:** DECLARED_SOURCE · **소비:** [노드 API](20_API.md).

```c
typedef struct
{
    Gateway_AppActivityState_t state;
    Gateway_UpdateBasis_t update;
} Gateway_AppActivityInfo_t;
```

| 멤버 | 정확한 C 타입 | 역할·범위·유효성 | 원 생성자 |
|---|---|---|---|
| `state` | [`Gateway_AppActivityState_t`](11_GATEWAY_TYPES.md#type-gateway-appactivitystate-t) | 해당 타입의 의미 상태. 타입별 enum의 UNKNOWN/UNCONFIRMED/STALE를 정상 CLEAR/FAR로 만들지 않는다. | MOBILE 원 활성 producer→ESP32 검증 owner |
| `update` | [`Gateway_UpdateBasis_t`](11_GATEWAY_TYPES.md#type-gateway-updatebasis-t) | 해당 Gateway 관측의 quality/age_ms/update_sequence. 타입 공유는 카운터 공유가 아니다. | MOBILE 원 활성 producer→ESP32 검증 owner |

<a id="type-gateway-bluetoothcontext-t"></a>
## `Gateway_BluetoothContext_t`

**원 소속:** `Gateway_Types.h` · **지위:** DECLARED_SOURCE · **소비:** [노드 API](20_API.md).

```c
typedef struct
{
    Gateway_RegistrationInfo_t registration;
    Gateway_ConnectionInfo_t connection;
    Gateway_AppActivityInfo_t app_activity;
} Gateway_BluetoothContext_t;
```

| 멤버 | 정확한 C 타입 | 역할·범위·유효성 | 원 생성자 |
|---|---|---|---|
| `registration` | [`Gateway_RegistrationInfo_t`](11_GATEWAY_TYPES.md#type-gateway-registrationinfo-t) | 검증된 등록 단말 여부. BLE 연결·이름·주소만으로 등록 성공 생성 금지. | ESP32 Gateway / 해당 BLE·등록·link owner |
| `connection` | [`Gateway_ConnectionInfo_t`](11_GATEWAY_TYPES.md#type-gateway-connectioninfo-t) | 해당 peer 연결 관측/문맥. BLE 연결 여부로 사용자 요청 수용을 대신하지 않는다. | ESP32 Gateway / 해당 BLE·등록·link owner |
| `app_activity` | [`Gateway_AppActivityInfo_t`](11_GATEWAY_TYPES.md#type-gateway-appactivityinfo-t) | MOBILE App 실제 활성 의미+독립 업데이트 근거. producer/TTL OPEN. | MOBILE 원 활성 producer→ESP32 검증 owner |

<a id="type-gateway-proximityinfo-t"></a>
## `Gateway_ProximityInfo_t`

**원 소속:** `Gateway_Types.h` · **지위:** DECLARED_SOURCE · **소비:** [노드 API](20_API.md).

```c
typedef struct
{
    Gateway_ProximityState_t state;
    Gateway_UpdateBasis_t update;
} Gateway_ProximityInfo_t;
```

| 멤버 | 정확한 C 타입 | 역할·범위·유효성 | 원 생성자 |
|---|---|---|---|
| `state` | [`Gateway_ProximityState_t`](11_GATEWAY_TYPES.md#type-gateway-proximitystate-t) | 해당 타입의 의미 상태. 타입별 enum의 UNKNOWN/UNCONFIRMED/STALE를 정상 CLEAR/FAR로 만들지 않는다. | ESP32 Proximity owner |
| `update` | [`Gateway_UpdateBasis_t`](11_GATEWAY_TYPES.md#type-gateway-updatebasis-t) | 해당 Gateway 관측의 quality/age_ms/update_sequence. 타입 공유는 카운터 공유가 아니다. | ESP32 Proximity owner |

<a id="type-gateway-logicalmessagetype-t"></a>
## `Gateway_LogicalMessageType_t`

**원 소속:** `Gateway_Types.h` · **지위:** DECLARED_SOURCE · **소비:** [노드 API](20_API.md).

```c
typedef enum
{
    GATEWAY_LOGICAL_MSG_UNKNOWN = 0,

    /* MOBILE -> Domain relay */
    GATEWAY_LOGICAL_MSG_MOBILE_REQUEST,
    GATEWAY_LOGICAL_MSG_STATE_QUERY,
    GATEWAY_LOGICAL_MSG_WARNING_ACK,

    /* ESP32 -> Domain generated/context information */
    GATEWAY_LOGICAL_MSG_REGISTRATION_CONNECTION,
    GATEWAY_LOGICAL_MSG_APP_ACTIVITY,
    GATEWAY_LOGICAL_MSG_PROXIMITY,

    /* Domain -> MOBILE relay */
    GATEWAY_LOGICAL_MSG_REQUEST_RESULT,
    GATEWAY_LOGICAL_MSG_VEHICLE_STATE,
    GATEWAY_LOGICAL_MSG_WARNING,
    GATEWAY_LOGICAL_MSG_FUNCTION_AVAILABILITY,
    GATEWAY_LOGICAL_MSG_DIGITAL_KEY_STATE_RESULT
} Gateway_LogicalMessageType_t;
```

| C 열거자 | 실제 C 정수 | 유효 조건 |
|---|---:|---|
| `GATEWAY_LOGICAL_MSG_UNKNOWN` | 0 | 이 enum 의미로만 소비. raw code는 명시 mapping 후 사용. |
| `GATEWAY_LOGICAL_MSG_MOBILE_REQUEST` | 1 | 이 enum 의미로만 소비. raw code는 명시 mapping 후 사용. |
| `GATEWAY_LOGICAL_MSG_STATE_QUERY` | 2 | 이 enum 의미로만 소비. raw code는 명시 mapping 후 사용. |
| `GATEWAY_LOGICAL_MSG_WARNING_ACK` | 3 | 이 enum 의미로만 소비. raw code는 명시 mapping 후 사용. |
| `GATEWAY_LOGICAL_MSG_REGISTRATION_CONNECTION` | 4 | 이 enum 의미로만 소비. raw code는 명시 mapping 후 사용. |
| `GATEWAY_LOGICAL_MSG_APP_ACTIVITY` | 5 | 이 enum 의미로만 소비. raw code는 명시 mapping 후 사용. |
| `GATEWAY_LOGICAL_MSG_PROXIMITY` | 6 | 이 enum 의미로만 소비. raw code는 명시 mapping 후 사용. |
| `GATEWAY_LOGICAL_MSG_REQUEST_RESULT` | 7 | 이 enum 의미로만 소비. raw code는 명시 mapping 후 사용. |
| `GATEWAY_LOGICAL_MSG_VEHICLE_STATE` | 8 | 이 enum 의미로만 소비. raw code는 명시 mapping 후 사용. |
| `GATEWAY_LOGICAL_MSG_WARNING` | 9 | 이 enum 의미로만 소비. raw code는 명시 mapping 후 사용. |
| `GATEWAY_LOGICAL_MSG_FUNCTION_AVAILABILITY` | 10 | 이 enum 의미로만 소비. raw code는 명시 mapping 후 사용. |
| `GATEWAY_LOGICAL_MSG_DIGITAL_KEY_STATE_RESULT` | 11 | 이 enum 의미로만 소비. raw code는 명시 mapping 후 사용. |

<a id="type-gateway-messagedirection-t"></a>
## `Gateway_MessageDirection_t`

**원 소속:** `Gateway_Types.h` · **지위:** DECLARED_SOURCE · **소비:** [노드 API](20_API.md).

```c
typedef enum
{
    GATEWAY_DIRECTION_UNKNOWN = 0,
    GATEWAY_DIRECTION_MOBILE_TO_DOMAIN,
    GATEWAY_DIRECTION_ESP32_TO_DOMAIN,
    GATEWAY_DIRECTION_DOMAIN_TO_MOBILE
} Gateway_MessageDirection_t;
```

| C 열거자 | 실제 C 정수 | 유효 조건 |
|---|---:|---|
| `GATEWAY_DIRECTION_UNKNOWN` | 0 | 이 enum 의미로만 소비. raw code는 명시 mapping 후 사용. |
| `GATEWAY_DIRECTION_MOBILE_TO_DOMAIN` | 1 | 이 enum 의미로만 소비. raw code는 명시 mapping 후 사용. |
| `GATEWAY_DIRECTION_ESP32_TO_DOMAIN` | 2 | 이 enum 의미로만 소비. raw code는 명시 mapping 후 사용. |
| `GATEWAY_DIRECTION_DOMAIN_TO_MOBILE` | 3 | 이 enum 의미로만 소비. raw code는 명시 mapping 후 사용. |
