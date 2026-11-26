# CAN 공통 Data — 의미 자료와 전송 관측

이 정본은 CAN 메시지에 공통으로 쓰이는 기존 선언과 새 내부 제안 자료를 구분한다. 값·품질·원 식별은 생산자가 제공하며 Network가 재발급하지 않는다. C 구조체의 크기·정렬은 Wire 형식이 아니다.

[CAN 개요](../04_CAN_FD_OVERVIEW.md) · [공통 Codec](../functions/CAN_COMMON_CODEC.md) · [전송 제어](../functions/CAN_TRANSFER_CONTROL.md) · [공통 흐름](../flows/CAN_NETWORK_PATHS.md)

## 기존 선언의 위치와 상태

`NetCanSource`, `NetCanRxMeta`, `NetSubmitResult`, `NetValidity/Code`, `NetQualityReason/Code`, `UpdateSequence_t`, `SourceGeneration_t`, `SignalMeta_t`, `ValueQuality_t`, `DataQuality_t`, `DomainIf_Status_t`는 [CIS_REAR 정본](CIS_REAR_DATA.md#declared)을 그대로 사용한다. 공통 C2 API의 공개 본체·Manager 상태는 DOMAIN 소유다. 아래 Net 선언은 `PROPOSED_DESIGN/BINDING_OPEN`, C2 선언은 `DECLARED_SOURCE`이며 제품 헤더·ABI 채택 승인이 아니다. C2 v0.6/Vehicle v0.3은 후보로 남긴다.

기존 선언은 S4A Data의 명시된 구간에서 옮겼다. 숫자값이 같아도 raw enum으로 캐스팅하지 않는다. 예를 들어 C2 LOCAL_OVERRIDE=1은 Wire reason10, STATE_UNTRUSTED=7은 Wire reason5다. 각 메시지의 명시 표가 없거나 표현을 잃으면 CONTRACT_HOLD다. `FunctionAvailability_t.UNSUPPORTED`를 UNAVAILABLE로 축소하지 않는다.

<a id="type-NetTriState"></a>
### `NetTriState`

출처: `S4A_ORIGINAL/data/T37.md#type-NetTriState`.

```c
typedef uint8_t NetTriState;
```

<a id="type-NetTriStateCode"></a>
### `NetTriStateCode`

출처: `S4A_ORIGINAL/data/T37.md#type-NetTriStateCode`.

```c
enum NetTriStateCode
{
    NET_TRISTATE_NO = 0,
    NET_TRISTATE_YES = 1,
    NET_TRISTATE_UNKNOWN = 255
};
```

<a id="type-NetFlag"></a>
### `NetFlag`

출처: `S4A_ORIGINAL/data/T37.md#type-NetFlag`.

```c
typedef uint8_t NetFlag;
```

<a id="type-NetFlagCode"></a>
### `NetFlagCode`

출처: `S4A_ORIGINAL/data/T37.md#type-NetFlagCode`.

```c
enum NetFlagCode
{
    NET_FLAG_NO = 0,
    NET_FLAG_YES = 1
};
```

<a id="type-NetValueQuality"></a>
### `NetValueQuality`

출처: `S4A_ORIGINAL/data/T37.md#type-NetValueQuality`.

```c
typedef uint8_t NetValueQuality;
```

<a id="type-NetValueQualityCode"></a>
### `NetValueQualityCode`

출처: `S4A_ORIGINAL/data/T37.md#type-NetValueQualityCode`.

```c
enum NetValueQualityCode
{
    NET_QUALITY_OK = 0,
    NET_QUALITY_STALE = 1,
    NET_QUALITY_INVALID = 2,
    NET_QUALITY_NO_DATA = 3
};
```

<a id="type-NetControllerState"></a>
### `NetControllerState`

출처: `S4A_ORIGINAL/data/T37.md#type-NetControllerState`.

```c
typedef uint8_t NetControllerState;
```

<a id="type-NetControllerStateCode"></a>
### `NetControllerStateCode`

출처: `S4A_ORIGINAL/data/T37.md#type-NetControllerStateCode`.

```c
enum NetControllerStateCode
{
    NET_CONTROLLER_INIT = 0,
    NET_CONTROLLER_READY = 1,
    NET_CONTROLLER_DEGRADED = 2,
    NET_CONTROLLER_FAULT = 3
};
```

<a id="type-NetFaultCategory"></a>
### `NetFaultCategory`

출처: `S4A_ORIGINAL/data/T37.md#type-NetFaultCategory`.

```c
typedef uint8_t NetFaultCategory;
```

<a id="type-NetFaultCategoryCode"></a>
### `NetFaultCategoryCode`

출처: `S4A_ORIGINAL/data/T37.md#type-NetFaultCategoryCode`.

```c
enum NetFaultCategoryCode
{
    NET_FAULT_CATEGORY_NONE = 0,
    NET_FAULT_CATEGORY_SENSOR = 1,
    NET_FAULT_CATEGORY_FUNCTION = 2,
    NET_FAULT_CATEGORY_COMM = 3
};
```

<a id="type-NetReason"></a>
### `NetReason`

출처: `S4A_ORIGINAL/data/T37.md#type-NetReason`.

```c
typedef uint16_t NetReason;
```

<a id="type-NetReasonCode"></a>
### `NetReasonCode`

출처: `S4A_ORIGINAL/data/T37.md#type-NetReasonCode`.

```c
enum NetReasonCode
{
    NET_REASON_NONE = 0,
    NET_REASON_NOT_REGISTERED = 1,
    NET_REASON_LINK_LOST = 2,
    NET_REASON_SESSION_INVALID = 3,
    NET_REASON_DOOR_OPEN = 4,
    NET_REASON_STATE_UNTRUSTED = 5,
    NET_REASON_CMD_INVALID = 6,
    NET_REASON_NO_FEEDBACK = 7,
    NET_REASON_DRIVE_LIMIT_EXCEEDED = 8,
    NET_REASON_ALREADY_AT_TARGET = 9,
    NET_REASON_LOCAL_OVERRIDE = 10,
    NET_REASON_STOP_REQUESTED = 11,
    NET_REASON_SUPERSEDED = 12,
    NET_REASON_ANTIPINCH = 13,
    NET_REASON_FAULT = 14,
    NET_REASON_STALE = 15,
    NET_REASON_NO_DATA = 16,
    NET_REASON_NOT_READY = 17,
    NET_REASON_OUT_OF_RANGE = 18,
    NET_REASON_SOURCE_FAILURE = 19,
    NET_REASON_UNKNOWN_REQUEST = 20,
    NET_REASON_EXPIRED = 21,
    NET_REASON_ID_CONFLICT = 22,
    NET_REASON_POWER_NOT_ALLOWED = 23,
    NET_REASON_OVERHEAT = 24,
    NET_REASON_FAN_MISMATCH = 25
};
```

<a id="type-NetVehicleResult"></a>
### `NetVehicleResult`

출처: `S4A_ORIGINAL/data/T37.md#type-NetVehicleResult`.

```c
typedef uint8_t NetVehicleResult;
```

<a id="type-NetVehicleResultCode"></a>
### `NetVehicleResultCode`

출처: `S4A_ORIGINAL/data/T37.md#type-NetVehicleResultCode`.

```c
enum NetVehicleResultCode
{
    NET_RESULT_ACCEPTED = 0,
    NET_RESULT_IN_PROGRESS = 1,
    NET_RESULT_DONE = 2,
    NET_RESULT_REJECTED = 3,
    NET_RESULT_CANCELLED = 4,
    NET_RESULT_FAILED = 5,
    NET_RESULT_NOT_PRESENT = 255
};
```

<a id="type-NetCommandMeta"></a>
### `NetCommandMeta`

출처: `S4A_ORIGINAL/data/T37.md#type-NetCommandMeta`.

```c
typedef struct NetCommandMeta
{
    uint32_t target_boot_id;  /**< 마지막 상태로 확인한 대상 ECU 기동 번호. */
    uint32_t command_id;      /**< Domain C2 기능 결정 owner가 최초 부여하는 명령 ID. Network가 중복 생성하지 않고 같은 명령 재송신 시 유지. */
    uint16_t command_age_ms;  /**< Domain C2 명령 birth AGE에 Network 대기·전송 지연을 누적한 ms; 반복 전송에서 초기화 금지. */
    uint16_t accept_limit_ms; /**< Wire 제안 200ms. SysRS Domain→ECU 1초 잠정안과 미정 차이 확인; 임의 상향 금지. */
} NetCommandMeta;
```

<a id="type-NetAvailability"></a>
### `NetAvailability`

출처: `S4A_ORIGINAL/data/T37.md#type-NetAvailability`.

```c
typedef uint8_t NetAvailability;
```

<a id="type-NetAvailabilityCode"></a>
### `NetAvailabilityCode`

출처: `S4A_ORIGINAL/data/T37.md#type-NetAvailabilityCode`.

```c
enum NetAvailabilityCode
{
    NET_AVAILABILITY_AVAILABLE = 0,
    NET_AVAILABILITY_LIMITED = 1,
    NET_AVAILABILITY_UNAVAILABLE = 2
};
```

<a id="type-NetSensorFunctionState"></a>
### `NetSensorFunctionState`

출처: `S4A_ORIGINAL/data/T37.md#type-NetSensorFunctionState`.

```c
typedef uint8_t NetSensorFunctionState;
```

<a id="type-NetSensorFunctionStateCode"></a>
### `NetSensorFunctionStateCode`

출처: `S4A_ORIGINAL/data/T37.md#type-NetSensorFunctionStateCode`.

```c
enum NetSensorFunctionStateCode
{
    NET_SENSOR_FUNCTION_NOT_READY = 0,
    NET_SENSOR_FUNCTION_READY = 1,
    NET_SENSOR_FUNCTION_ACTIVE = 2,
    NET_SENSOR_FUNCTION_FAULT = 3,
    NET_SENSOR_FUNCTION_RECOVERING = 4
};
```

<a id="type-NetOrigin"></a>
### `NetOrigin`

출처: `S4A_ORIGINAL/data/T37.md#type-NetOrigin`.

```c
typedef uint8_t NetOrigin;
```

<a id="type-NetOriginCode"></a>
### `NetOriginCode`

출처: `S4A_ORIGINAL/data/T37.md#type-NetOriginCode`.

```c
enum NetOriginCode
{
    NET_ORIGIN_NONE = 0,
    NET_ORIGIN_MOBILE_MANUAL = 1,
    NET_ORIGIN_PROXIMITY_AUTO = 2,
    NET_ORIGIN_LOCAL = 3,
    NET_ORIGIN_TEST = 4,
    NET_ORIGIN_VEHICLE_POLICY = 5
};
```

<a id="type-CommonEcuState_t"></a>
### `CommonEcuState_t`

출처: `S4A_ORIGINAL/data/T13.md#type-CommonEcuState_t`.

```c
typedef enum
{
    ECU_STATE_INIT = 0,
    ECU_STATE_READY,
    ECU_STATE_DEGRADED,
    ECU_STATE_FAULT
} CommonEcuState_t;
```

<a id="type-FaultInfo_t"></a>
### `FaultInfo_t`

출처: `S4A_ORIGINAL/data/T13.md#type-FaultInfo_t`.

```c
typedef struct
{
    FaultCategory_t category;
    FaultState_t state;
    uint16_t faultCode;
    DataQuality_t quality;
} FaultInfo_t;
```

<a id="type-FaultCategory_t"></a>
### `FaultCategory_t`

출처: `S4A_ORIGINAL/data/T13.md#type-FaultCategory_t`.

```c
typedef enum
{
    FAULT_CATEGORY_SENSOR = 0,
    FAULT_CATEGORY_COMM,
    FAULT_CATEGORY_FUNCTION
} FaultCategory_t;
```

<a id="type-FaultState_t"></a>
### `FaultState_t`

출처: `S4A_ORIGINAL/data/T13.md#type-FaultState_t`.

```c
typedef enum
{
    FAULT_STATE_INACTIVE = 0,
    FAULT_STATE_ACTIVE,
    FAULT_STATE_RECOVERING
} FaultState_t;
```

<a id="type-RequestResult_t"></a>
### `RequestResult_t`

출처: `S4A_ORIGINAL/data/T13.md#type-RequestResult_t`.

```c
typedef enum
{
    REQUEST_RESULT_ACCEPTED = 0,
    REQUEST_RESULT_IN_PROGRESS,
    REQUEST_RESULT_DONE,
    REQUEST_RESULT_REJECTED,
    REQUEST_RESULT_CANCELLED,
    REQUEST_RESULT_FAILED
} RequestResult_t;
```

<a id="type-ResultReason_t"></a>
### `ResultReason_t`

출처: `S4A_ORIGINAL/data/T13.md#type-ResultReason_t`.

```c
typedef enum
{
    RESULT_REASON_NONE = 0,
    RESULT_REASON_LOCAL_OVERRIDE,
    RESULT_REASON_STOP_REQUESTED,
    RESULT_REASON_SUPERSEDED,
    RESULT_REASON_ANTIPINCH,
    RESULT_REASON_NOT_ALLOWED,
    RESULT_REASON_INVALID_COMMAND,
    RESULT_REASON_STATE_UNTRUSTED,
    RESULT_REASON_NO_FEEDBACK,
    RESULT_REASON_DRIVE_LIMIT_EXCEEDED,
    RESULT_REASON_MOTOR_FAULT,
    RESULT_REASON_SENSOR_FAULT,
    RESULT_REASON_TIMEOUT,
    RESULT_REASON_OTHER, /* internal-only unless an explicit wire mapping exists */
    RESULT_REASON_NOT_REGISTERED,
    RESULT_REASON_LINK_LOST,
    RESULT_REASON_SESSION_INVALID,
    RESULT_REASON_DOOR_OPEN,
    RESULT_REASON_ALREADY_AT_TARGET,
    RESULT_REASON_FAULT,
    RESULT_REASON_STALE,
    RESULT_REASON_NO_DATA,
    RESULT_REASON_NOT_READY,
    RESULT_REASON_OUT_OF_RANGE,
    RESULT_REASON_SOURCE_FAILURE,
    RESULT_REASON_UNKNOWN_REQUEST,
    RESULT_REASON_EXPIRED,
    RESULT_REASON_ID_CONFLICT,
    RESULT_REASON_POWER_NOT_ALLOWED,
    RESULT_REASON_OVERHEAT,
    RESULT_REASON_FAN_MISMATCH
} ResultReason_t;
```

<a id="type-DomainIf_CommandContext_t"></a>
### `DomainIf_CommandContext_t`

출처: `S4A_ORIGINAL/data/T14.md#type-DomainIf_CommandContext_t`.

```c
typedef struct
{
    DomainCommandId_t commandId;

    /**
     * 중앙이 최종 실행 목표를 결정한 논리 순서.
     * Domain 내부 provenance로만 유지하며 Network 필수 field 또는 wire 값으로 전송하지 않는다.
     */
    uint32_t decisionSequence;

    bool hasRequestContext;
    RequestContext_t requestContext;
    DeviceContextId_t requestDeviceContextId;
    CommandOrigin_t origin;
    uint32_t commandAgeMs; /* command birth age, not sensor sourceMeta.ageMs */

    SignalMeta_t meta; /* existing Domain decision/source provenance */
} DomainIf_CommandContext_t;
```

<a id="type-DomainCommandId_t"></a>
### `DomainCommandId_t`

출처: `S4A_ORIGINAL/data/T13.md#type-DomainCommandId_t`.

```c
typedef uint32_t DomainCommandId_t;
```

<a id="type-OccurrenceId_t"></a>
### `OccurrenceId_t`

출처: `S4A_ORIGINAL/data/T13.md#type-OccurrenceId_t`.

```c
typedef uint32_t OccurrenceId_t;
```

<a id="type-ResultConfirmation_t"></a>
### `ResultConfirmation_t`

출처: `S4A_ORIGINAL/data/T13.md#type-ResultConfirmation_t`.

```c
typedef enum
{
    RESULT_CONFIRMATION_UNCONFIRMED = 0,
    RESULT_CONFIRMATION_CONFIRMED
} ResultConfirmation_t;
```

<a id="type-CommandOrigin_t"></a>
### `CommandOrigin_t`

출처: `S4A_ORIGINAL/data/T13.md#type-CommandOrigin_t`.

```c
typedef enum
{
    COMMAND_ORIGIN_MOBILE_REQUEST = 0,
    COMMAND_ORIGIN_DIGITAL_KEY,
    COMMAND_ORIGIN_CLIMATE_POLICY,
    COMMAND_ORIGIN_INTERIOR_LIGHT_POLICY,
    COMMAND_ORIGIN_AUTO_VENTILATION,
    COMMAND_ORIGIN_SYSTEM
} CommandOrigin_t;
```

<a id="type-FunctionAvailability_t"></a>
### `FunctionAvailability_t`

출처: `S4A_ORIGINAL/data/T13.md#type-FunctionAvailability_t`.

```c
typedef enum
{
    FUNCTION_AVAILABILITY_AVAILABLE = 0,
    FUNCTION_AVAILABILITY_DEGRADED,
    FUNCTION_AVAILABILITY_UNAVAILABLE,
    FUNCTION_AVAILABILITY_UNSUPPORTED
} FunctionAvailability_t;
```

<a id="type-NetworkLocalFailure_t"></a>
### `NetworkLocalFailure_t`

출처: `S4A_ORIGINAL/data/T14.md#type-NetworkLocalFailure_t`.

```c
typedef struct
{
    NetworkLinkId_t link;
    NetworkLocalItemKind_t itemKind;
    NetworkLocalFailureReason_t reason;
    SourceGeneration_t identityGeneration; /* captured at admission:
        COMMAND: command producer/current Domain generation;
        REQUEST_RESULT: result publication producer generation;
        OCCURRENCE: occurrence producer generation.
        Not target ECU BOOT_ID, RequestContext Session or current App Session.
        Never relabel an old item with a new admitted generation. */
    uint32_t elapsedSinceAdmissionMs; /* local evidence, not wire/source age */
    bool hasCommandId;
    DomainCommandId_t commandId;
    bool hasRequestContext;
    DeviceContextId_t requestDeviceContextId;
    RequestContext_t requestContext; /* original Session, never current relabel */
    bool hasOccurrenceId;
    OccurrenceId_t occurrenceId;
} NetworkLocalFailure_t;
```

<a id="type-NetworkHealthSnapshot_t"></a>
### `NetworkHealthSnapshot_t`

출처: `S4A_ORIGINAL/data/T14.md#type-NetworkHealthSnapshot_t`.

```c
typedef struct
{
    NetworkLinkId_t link;
    NetworkTransportState_t transportState;
    bool hasValidRx;
    uint32_t lastValidRxAgeMs; /* meaningful only when hasValidRx */
    NetworkOperationalCounters_t counters;
} NetworkHealthSnapshot_t;
```

<a id="type-RequestContext_t"></a>
### `RequestContext_t`

출처: `S4A_ORIGINAL/data/T13.md#type-RequestContext_t`.

```c
typedef struct
{
    SessionId_t sessionId;
    RequestId_t requestId;
} RequestContext_t;
```

<a id="type-DeviceContextId_t"></a>
### `DeviceContextId_t`

출처: `S4A_ORIGINAL/data/T13.md#type-DeviceContextId_t`.

```c
typedef uint32_t DeviceContextId_t;
```

<a id="type-NetworkLinkId_t"></a>
### `NetworkLinkId_t`

출처: `S4A_ORIGINAL/data/T14.md#type-NetworkLinkId_t`.

```c
typedef enum
{
    NETWORK_LINK_CAN = 0,
    NETWORK_LINK_UART_M,
    NETWORK_LINK_UART_C
} NetworkLinkId_t;
```

<a id="type-NetworkLocalItemKind_t"></a>
### `NetworkLocalItemKind_t`

출처: `S4A_ORIGINAL/data/T14.md#type-NetworkLocalItemKind_t`.

```c
typedef enum
{
    NETWORK_LOCAL_ITEM_COMMAND = 0,
    NETWORK_LOCAL_ITEM_REQUEST_RESULT,
    NETWORK_LOCAL_ITEM_OCCURRENCE /* one-shot Event or occurrence-keyed Result */
} NetworkLocalItemKind_t;
```

<a id="type-NetworkLocalFailureReason_t"></a>
### `NetworkLocalFailureReason_t`

출처: `S4A_ORIGINAL/data/T14.md#type-NetworkLocalFailureReason_t`.

```c
typedef enum
{
    NETWORK_LOCAL_FAILURE_DRIVER_SUBMISSION_FINAL = 0,
    NETWORK_LOCAL_FAILURE_EXPIRED_BEFORE_SEND,
    NETWORK_LOCAL_FAILURE_CONTEXT_INVALIDATED_BEFORE_SEND
} NetworkLocalFailureReason_t;
```

<a id="type-NetworkTransportState_t"></a>
### `NetworkTransportState_t`

출처: `S4A_ORIGINAL/data/T14.md#type-NetworkTransportState_t`.

```c
typedef enum
{
    NETWORK_TRANSPORT_UNKNOWN = 0,
    NETWORK_TRANSPORT_UNAVAILABLE,
    NETWORK_TRANSPORT_AVAILABLE,
    NETWORK_TRANSPORT_RECOVERING
} NetworkTransportState_t;
```

<a id="type-NetworkOperationalCounters_t"></a>
### `NetworkOperationalCounters_t`

출처: `S4A_ORIGINAL/data/T14.md#type-NetworkOperationalCounters_t`.

```c
typedef struct
{
    uint32_t rxCrcRejectCount;
    uint32_t rxFormatRejectCount;
    uint32_t rxOverflowCount;
    uint32_t txFinalFailureCount;
    uint32_t busOffCount;
} NetworkOperationalCounters_t;
```

<a id="type-SessionId_t"></a>
### `SessionId_t`

출처: `S4A_ORIGINAL/data/T13.md#type-SessionId_t`.

```c
typedef uint64_t SessionId_t;
```

<a id="type-RequestId_t"></a>
### `RequestId_t`

출처: `S4A_ORIGINAL/data/T13.md#type-RequestId_t`.

```c
typedef uint32_t RequestId_t;
```

<a id="private-types"></a>
## 내부 제안 자료

다음은 `PROPOSED_PRIVATE_TYPE`이다. 실제 SDK 자료형이나 공개 헤더를 생성하지 않는다. 초기화·동기화·token 폭과 재사용 수명은 보드/Driver 바인딩 때 확인한다. 아래 enum의 순서는 CAN raw ID가 아니다.

<a id="type-NetCanResult"></a>
```c
typedef enum NetCanResult {
    NET_CAN_OK, NET_CAN_INVALID_ARGUMENT, NET_CAN_MALFORMED,
    NET_CAN_NOT_READY, NET_CAN_NO_CAPACITY, NET_CAN_CONTRACT_HOLD,
    NET_CAN_ORDER_HOLD, NET_CAN_TIME_HOLD, NET_CAN_DUPLICATE,
    NET_CAN_NO_WORK, NET_CAN_DRIVER_PENDING, NET_CAN_DRIVER_REJECTED,
    NET_CAN_CONSUMER_REJECTED
} NetCanResult;
```

| 반환 | 정확한 의미와 출력 수명 |
|---|---|
| OK | 해당 함수의 로컬 계약만 완료. Codec 출력 완성, AGE 계산 완료, 또는 관측 기록 갱신 등 함수별 의미를 따른다. 원격 수신/기능 완료가 아니다. |
| INVALID_ARGUMENT | NULL·잘못된 내부 선택자/인자. 출력과 상태 불변. |
| MALFORMED | frame 또는 인코딩하려는 Net 표현의 형식·예약 raw·범위·조합 오류. 송신 leaf Codec가 이 값을 사용하는 경우 출력은 불변이며 정상 Rx/consumer 호출도 없다. |
| NOT_READY / NO_CAPACITY | 준비/필수 sink 부족 또는 안정 보관 공간 부족. 접수 전이면 접수 실패. |
| CONTRACT_HOLD / ORDER_HOLD / TIME_HOLD | 계약 채택·원 기동/순서·시각 근거가 부족하다. 정상 소비·새 성공 증거를 만들지 않는다. |
| DUPLICATE | 동일 frame/동일 terminal event를 이미 관측했다. 재소비·중복 해제 없음. |
| NO_WORK | 보낼 항목 없음. 전송 성공을 뜻하지 않는다. |
| DRIVER_PENDING | 실제 인계 여부/최종 관측이 미확정. 항목 식별과 필요한 저장소를 유지한다. |
| DRIVER_REJECTED | Driver가 해당 제출을 최종 거절한 관측. 원격 기능 실패로 변환하지 않는다. |
| CONSUMER_REJECTED | 호출한 typed consumer가 성공을 반환하지 않음. 원 C2 반환값을 별도 보존한다. |

<a id="type-NetCanFrame"></a>
```c
typedef struct NetCanFrame {
    uint32_t canId;
    uint8_t dlc, length;
    bool extended, fdf, brs, rtr, driverError;
    uint32_t receivedAtMs;
    uint8_t data[64];
} NetCanFrame;
```

`length`는 실제 확보한 바이트 수다. `receivedAtMs`는 로컬 수신 시각이며 노드 간 시간 비교에 쓰지 않는다. 확보하지 않은 뒷부분을 0으로 채워 짧은 프레임을 정상으로 만들지 않는다. `driverError`가 참이면 이미 invalid인 프레임이다. B11 `NetRearFrame`을 변경하지 않으며 CIS_REAR 전용 Codec는 그 기존 자료를 사용한다.

<a id="type-NetCanNode"></a>
```c
typedef enum NetCanNode {
    NET_CAN_NODE_DOMAIN, NET_CAN_NODE_BCM, NET_CAN_NODE_CIS,
    NET_CAN_NODE_WINDOW, NET_CAN_NODE_VSS
} NetCanNode;
```

<a id="type-NetCanProfile"></a>
```c
typedef enum NetCanProfile {
    NET_CAN_PROFILE_UNSELECTED, NET_CAN_PROFILE_BASE_CANDIDATE,
    NET_CAN_PROFILE_WARNING51, NET_CAN_PROFILE_WARNING60
} NetCanProfile;
```

프로파일 enum 선택은 팀 채택 증거가 아니다. 각 메시지/송수신 쌍에 동일 revision 채택 근거가 연결되어야 한다. WINDOW 실행은 별도 보류이며 WARNING51/60 선택을 base 후보로 대신하지 않는다.

<a id="type-NetCanMessageKind"></a>
```c
typedef enum NetCanMessageKind {
    NET_CAN_DOMAIN_PERMISSION,
    NET_CAN_BCM_DOOR_COMMAND,
    NET_CAN_BCM_CLIMATE_COMMAND,
    NET_CAN_BCM_LIGHT_COMMAND,
    NET_CAN_WINDOW_COMMAND,
    NET_CAN_BCM_DOOR_STATE,
    NET_CAN_BCM_CLIMATE_STATE,
    NET_CAN_BCM_LIGHT_STATE,
    NET_CAN_BCM_RESULT,
    NET_CAN_BCM_EVENT,
    NET_CAN_BCM_STATUS,
    NET_CAN_CIS_ENVIRONMENT,
    NET_CAN_CIS_OCCUPANT,
    NET_CAN_CIS_REAR,
    NET_CAN_CIS_STATUS,
    NET_CAN_WINDOW_STATE,
    NET_CAN_WINDOW_EVENT,
    NET_CAN_WINDOW_RESULT,
    NET_CAN_WINDOW_FAULT,
    NET_CAN_VSS_EVENT,
    NET_CAN_VSS_WARNING_STATE,
    NET_CAN_VSS_STATUS
} NetCanMessageKind;
```

<a id="type-NetCanRxAction"></a>
```c
typedef enum NetCanRxAction {
    NET_CAN_RX_NEW_INPUT, NET_CAN_RX_SAME_INPUT,
    NET_CAN_RX_EXACT_DUPLICATE, NET_CAN_RX_CURRENT_RESTRICTION
} NetCanRxAction;
```

<a id="type-NetCanRxComparison"></a>
```c
typedef struct NetCanRxComparison {
    bool hasAcceptedFrame;
    bool generationBindingKnown, sameGeneration, transitionAuthorized;
    bool nonWrappingOrderKnown;
    uint32_t previousTx, incomingTx;
    bool rawExactlyEqual;
    bool hasImmutableInputIdentity, sameImmutableInputIdentity;
    bool semanticExactlyEqual, ageRelationAccepted;
    bool functionalIdentityOrderKnown;
    NetCanMessageKind messageKind;
    const struct NetCanOccupantRestrictionEvidence *occupantRestriction;
} NetCanRxComparison;
```

이는 받은 Wire가 아니라 M04가 원 frame/typed 임시값/시간 근거를 대조해 만든 호출 중 자료다. `rawExactlyEqual`에는 raw AGE와 모든 의미 바이트가 포함된다. `semanticExactlyEqual` 비교는 raw AGE를 분리하되 값·품질·상태·원 identity를 빼지 않는다. `transitionAuthorized`는 실제 Owner 정책에 따른 수용 증거이며 새로운 BOOT 수치가 보인 것만으로 참이 되지 않는다. `ageRelationAccepted`는 같은 소비 시점에 환산한 AGE와 clock 근거까지 검증되었다는 의미다.

`hasImmutableInputIdentity/sameImmutableInputIdentity`의 비교 기준은 관측 UPDATE, 불변 명령 본문, occurrence, 결과 단계별로 다르다. [수신 분류](../functions/CAN_TRANSFER_CONTROL.md#rx-order)의 입력 생성 계약을 따른다. `functionalIdentityOrderKnown`은 서로 다른 기능 입력/결과 단계가 Owner 규칙상 진행 가능한 근거다. TX가 높다는 사실만으로 참으로 설정하지 않는다.

<a id="type-NetCanPortOutcome"></a>
```c
typedef enum NetCanPortOutcome {
    NET_CAN_PORT_OUTCOME_UNKNOWN,
    NET_CAN_PORT_TX_SUCCEEDED, NET_CAN_PORT_TX_FAILED
} NetCanPortOutcome;
```

<a id="type-NetCanTxRecord"></a>
```c
typedef struct NetCanTxRecord {
    uint64_t localToken;
    uint32_t localEpoch;
    bool acceptedKnown, accepted;
    bool driverReferenceOutstanding, bufferReturned;
    bool identityRetained;
    NetCanPortOutcome outcome;
} NetCanTxRecord;
```

<a id="type-NetCanPortEvent"></a>
```c
typedef struct NetCanPortEvent {
    uint64_t localToken;
    uint32_t localEpoch;
    bool admissionObserved, accepted;
    bool releaseObserved;
    NetCanPortOutcome outcome;
} NetCanPortEvent;
```

token/epoch는 CAN BOOT/TX가 아닌 로컬 Driver 제출 수명 식별 후보다. 동일 key가 늦은 callback과 충돌하지 않는 재사용 정책이 확인되기 전 연결하지 않는다. Driver 반환 순서·참조 해제 방식은 실제 SDK에서 확인해야 한다. `bufferReturned`는 이미 로컬 회수한 저장소를 다시 반환하지 않도록 남기는 증거다. `identityRetained`는 release 후에도 최종 결과/F09 귀속에 필요한 원 identity를 별도 보존한다는 뜻이다. record만으로 payload를 복사했다고 간주하지 않는다.

## 시간·단위·소유권

u16 AGE의 65535는 실제 경과가 그 이상이라는 하한이다. u32 C2 AGE도 더하기 전에 범위를 검사하고 포화한다. source AGE/command birth AGE/수신 이후 경과/로컬 접수 이후 경과는 다른 값이다. `SessionId_t`는 출처의 64비트 선언 그대로이며 CAN BOOT나 Generation으로 축소하지 않는다. C2 Generation0/BOOT 수명과 환경의 실제 VSM/Meta 연결은 회신 대기다. 온도·습도 각 원 사용기한2500ms와 기한 도달 시 비사용은 합의 기준이다.

`NetworkLocalFailure_t`의 identityGeneration은 접수 시 원 생산 계보이며 target BOOT·현재 Session으로 덮어쓰지 않는다. `elapsedSinceAdmissionMs`는 로컬 증거이고 Wire AGE가 아니다. 품질 UNKNOWN/INVALID를 0값 또는 fresh로 만들지 않는다.

### PG6-SW01 — 제한 증거와 분류의 수명

`messageKind`는 실제 검사한 CAN ID의 메시지 선택자다. 모든 기존 caller는 이를 자기 메시지로 설정하고 `occupantRestriction=NULL`로 초기화한다. CIS_OCCUPANT M04만 [NetCanOccupantRestrictionEvidence](CAN_NODE_DATA.md#type-NetCanOccupantRestrictionEvidence)의 안정 사본을 연결할 수 있다. `semanticExactlyEqual`은 계속 값·품질·상태·identity 전체를 비교한다. 제한 예외를 얻으려고 true로 덮지 않는다.

`NET_CAN_RX_CURRENT_RESTRICTION`은 같은 Pi 원 관측에 실제 새 제한을 전달하는 내부 분류이며 새 관측·Wire enum이 아니다. 증거의 이전/현재 DTO·meta와 실제 사건 원자료는 분류부터 M03 소비·M09 결과 기록까지 동일 직렬 구간에서 불변이다. caller가 임의 제한 true를 제공하는 경로는 없다. 검증 실패는 기존 HOLD이며 다른 CAN 메시지는 이 분류에 진입하지 않는다.
