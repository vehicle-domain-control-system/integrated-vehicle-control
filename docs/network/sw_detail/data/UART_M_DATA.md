# UART-M의 기존 Data·API 자료 정본

UART-M의 typed 자료와 외부 경계에 필요한 기존 선언을 S4A에서 그대로 옮겼다. Net 설계 선언은 `PROPOSED_DESIGN/BINDING_OPEN`, C2·Gateway 출처 선언은 `DECLARED_SOURCE`이며 제품 ABI·양단 채택을 확인한 결과가 아니다. C2 v0.6/Vehicle v0.3 후보는 이 선언을 자동 교체하지 않는다.

[UART-M 개요](../05_UART_M_OVERVIEW.md) · [Frame/내부 자료](UART_FRAME_DATA.md) · [Router](../functions/UART_M_ROUTER.md) · [BCM/CIS/VSS/WINDOW 투영 필드](UART_M_PROJECTION_FIELDS.md) · [DOMAIN 보고 필드](UART_M_REPORT_FIELDS.md)

## 기존 B12 정본 재사용

아래 이름은 기존 Data 정본에 이미 정의되어 있으므로 중복 C 정의하지 않는다. Wire offset/단위·enum 변환은 각 UART-M 메시지의 필드표를 따른다. C 구조체 정렬·enum 숫자 캐스트로 바이트를 생성하지 않는다.

| 기존 자료 | 단일 정의 |
|---|---|
| `CisCabinEnvironmentState_t` | [기존 선언](CAN_NODE_DATA.md#type-CisCabinEnvironmentState_t) |
| `CisOccupantState_t` | [기존 선언](CAN_NODE_DATA.md#type-CisOccupantState_t) |
| `CisRearState_t` | [기존 선언](CIS_REAR_DATA.md#type-CisRearState_t) |
| `ClimateState_t` | [기존 선언](CAN_BCM_DATA.md#type-ClimateState_t) |
| `CommandOrigin_t` | [기존 선언](CAN_COMMON_DATA.md#type-CommandOrigin_t) |
| `DataQuality_t` | [기존 선언](CIS_REAR_DATA.md#type-DataQuality_t) |
| `DeviceContextId_t` | [기존 선언](CAN_COMMON_DATA.md#type-DeviceContextId_t) |
| `DomainCommandId_t` | [기존 선언](CAN_COMMON_DATA.md#type-DomainCommandId_t) |
| `DomainIf_BcmStatusReport_t` | [기존 선언](CAN_BCM_DATA.md#type-DomainIf_BcmStatusReport_t) |
| `DomainIf_CisStatusReport_t` | [기존 선언](CAN_NODE_DATA.md#type-DomainIf_CisStatusReport_t) |
| `DomainIf_Status_t` | [기존 선언](CIS_REAR_DATA.md#type-DomainIf_Status_t) |
| `DomainIf_VssStateReport_t` | [기존 선언](CAN_NODE_DATA.md#type-DomainIf_VssStateReport_t) |
| `DoorLockTarget_t` | [기존 선언](CAN_BCM_DATA.md#type-DoorLockTarget_t) |
| `DoorState_t` | [기존 선언](CAN_BCM_DATA.md#type-DoorState_t) |
| `FanLevel_t` | [기존 선언](CAN_BCM_DATA.md#type-FanLevel_t) |
| `FaultInfo_t` | [기존 선언](CAN_COMMON_DATA.md#type-FaultInfo_t) |
| `FunctionAvailability_t` | [기존 선언](CAN_COMMON_DATA.md#type-FunctionAvailability_t) |
| `InteriorLightState_t` | [기존 선언](CAN_BCM_DATA.md#type-InteriorLightState_t) |
| `NetAvailability` | [기존 선언](CAN_COMMON_DATA.md#type-NetAvailability) |
| `NetBcmClimateState` | [기존 선언](CAN_BCM_DATA.md#type-NetBcmClimateState) |
| `NetBcmDoorState` | [기존 선언](CAN_BCM_DATA.md#type-NetBcmDoorState) |
| `NetBcmLightState` | [기존 선언](CAN_BCM_DATA.md#type-NetBcmLightState) |
| `NetBcmStatus` | [기존 선언](CAN_BCM_DATA.md#type-NetBcmStatus) |
| `NetCanRxMeta` | [기존 선언](CIS_REAR_DATA.md#type-NetCanRxMeta) |
| `NetCanSource` | [기존 선언](CIS_REAR_DATA.md#type-NetCanSource) |
| `NetCisEnvironment` | [기존 선언](CAN_NODE_DATA.md#type-NetCisEnvironment) |
| `NetCisOccupant` | [기존 선언](CAN_NODE_DATA.md#type-NetCisOccupant) |
| `NetCisRear` | [기존 선언](CIS_REAR_DATA.md#type-NetCisRear) |
| `NetCisStatus` | [기존 선언](CAN_NODE_DATA.md#type-NetCisStatus) |
| `NetControllerState` | [기존 선언](CAN_COMMON_DATA.md#type-NetControllerState) |
| `NetFanLevel` | [기존 선언](CAN_BCM_DATA.md#type-NetFanLevel) |
| `NetFlag` | [기존 선언](CAN_COMMON_DATA.md#type-NetFlag) |
| `NetOrigin` | [기존 선언](CAN_COMMON_DATA.md#type-NetOrigin) |
| `NetQualityReason` | [기존 선언](CIS_REAR_DATA.md#type-NetQualityReason) |
| `NetReason` | [기존 선언](CAN_COMMON_DATA.md#type-NetReason) |
| `NetRgb` | [기존 선언](CAN_BCM_DATA.md#type-NetRgb) |
| `NetSubmitResult` | [기존 선언](CIS_REAR_DATA.md#type-NetSubmitResult) |
| `NetTriState` | [기존 선언](CAN_COMMON_DATA.md#type-NetTriState) |
| `NetValidity` | [기존 선언](CIS_REAR_DATA.md#type-NetValidity) |
| `NetValueQuality` | [기존 선언](CAN_COMMON_DATA.md#type-NetValueQuality) |
| `NetVehicleResult` | [기존 선언](CAN_COMMON_DATA.md#type-NetVehicleResult) |
| `NetVssStatus` | [기존 선언](CAN_NODE_DATA.md#type-NetVssStatus) |
| `NetWindowEvent` | [기존 선언](CAN_NODE_DATA.md#type-NetWindowEvent) |
| `NetWindowFault` | [기존 선언](CAN_NODE_DATA.md#type-NetWindowFault) |
| `NetWindowState` | [기존 선언](CAN_NODE_DATA.md#type-NetWindowState) |
| `OccurrenceId_t` | [기존 선언](CAN_COMMON_DATA.md#type-OccurrenceId_t) |
| `QualityReason_t` | [기존 선언](CIS_REAR_DATA.md#type-QualityReason_t) |
| `RequestContext_t` | [기존 선언](CAN_COMMON_DATA.md#type-RequestContext_t) |
| `RequestResult_t` | [기존 선언](CAN_COMMON_DATA.md#type-RequestResult_t) |
| `ResultConfirmation_t` | [기존 선언](CAN_COMMON_DATA.md#type-ResultConfirmation_t) |
| `ResultReason_t` | [기존 선언](CAN_COMMON_DATA.md#type-ResultReason_t) |
| `RgbColor_t` | [기존 선언](CAN_BCM_DATA.md#type-RgbColor_t) |
| `SessionId_t` | [기존 선언](CAN_COMMON_DATA.md#type-SessionId_t) |
| `SignalMeta_t` | [기존 선언](CIS_REAR_DATA.md#type-SignalMeta_t) |
| `SourceEvidence_t` | [기존 선언](CAN_NODE_DATA.md#type-SourceEvidence_t) |
| `SourceGeneration_t` | [기존 선언](CIS_REAR_DATA.md#type-SourceGeneration_t) |
| `ValueValidity_t` | [기존 선언](CIS_REAR_DATA.md#type-ValueValidity_t) |
| `WindowState_t` | [기존 선언](CAN_NODE_DATA.md#type-WindowState_t) |

## UART-M 기존 선언

SOURCE는 결과 패키지 출처 색인의 `S4A_RESULT/data/...`에 대응한다. 멤버·주석을 포함해 원 선언을 보존한다. 뒤의 제약 문장이 실제 계약 바인딩과 Wire 해석의 한계를 설명한다.

<a id="type-NetMobileRequestArgument"></a>
### `NetMobileRequestArgument`

출처: `S4A_RESULT/data/T15.md#type-NetMobileRequestArgument`.

```c
typedef union NetMobileRequestArgument
{
    int16_t target_temperature_centi_c; /**< CLIMATE / TARGET_TEMPERATURE. 허용 범위는 Domain 확인 항목. */
    NetFlag climate_auto;              /**< CLIMATE / AUTO. */
    NetFanLevel fan_level;             /**< CLIMATE / FAN. UNKNOWN 금지. */
    NetFlag light_enabled;             /**< INTERIOR_LIGHT / ENABLE. */
    uint8_t light_level_percent;       /**< INTERIOR_LIGHT / LEVEL. 0~100. */
    NetRgb normal_rgb;                 /**< INTERIOR_LIGHT / RGB. 각 0~255. */
    NetFlag digital_key_enabled;       /**< DIGITAL_KEY / ENABLE. */
} NetMobileRequestArgument;
```

<a id="type-NetMobileContext"></a>
### `NetMobileContext`

출처: `S4A_RESULT/data/T15.md#type-NetMobileContext`.

```c
typedef struct NetMobileContext
{
    NetAppContext context; /**< 요청 단말·Session 문맥. */
    NetTriState device_registration_state; /**< DEVICE_REGISTRATION_STATE: 0=NOT_REGISTERED; 1=REGISTERED; 255=UNKNOWN */
    NetTriState bt_connection_state; /**< BT_CONNECTION_STATE: 0=DISCONNECTED; 1=CONNECTED; 255=UNKNOWN */
    NetTriState app_active_state; /**< APP_ACTIVE_STATE: 0=INACTIVE; 1=ACTIVE; 255=UNKNOWN */
    NetValueQuality app_quality; /**< APP_QUALITY: VALUE_QUALITY */
    uint32_t app_update_sequence; /**< APP_UPDATE_SEQUENCE: App의 새 활성 정보 번호 */
    uint16_t app_age_ms; /**< APP_AGE: App 활성 정보 후 ms */
    NetTriState proximity_state; /**< PROXIMITY_STATE: 0=FAR; 1=NEAR; 255=UNKNOWN */
    NetValidity proximity_validity; /**< PROXIMITY_VALIDITY: 0=INVALID; 1=VALID */
    NetQualityReason proximity_reason; /**< PROXIMITY_REASON: 품질 사유 */
    uint32_t proximity_update_sequence; /**< PROXIMITY_UPDATE_SEQUENCE: ESP32 새 RSSI 평가 번호 */
    uint16_t proximity_age_ms; /**< PROXIMITY_AGE: RSSI 평가 후 ms */
    NetValueQuality registration_connection_quality; /**< REGISTRATION_CONNECTION_QUALITY: 현재 등록·연결 공동 평가의 품질 */
    uint32_t registration_connection_update_sequence; /**< REGISTRATION_CONNECTION_UPDATE_SEQUENCE: 새 공동 평가 번호(재전달 증가 금지) */
    uint16_t registration_connection_age_ms; /**< REGISTRATION_CONNECTION_AGE: 공동 평가 원본 이후 누적 경과 시간(ms) */
} NetMobileContext;
```

<a id="type-NetMobileRequest"></a>
### `NetMobileRequest`

출처: `S4A_RESULT/data/T15.md#type-NetMobileRequest`.

```c
typedef struct NetMobileRequest
{
    NetAppContext context; /**< 요청 단말·Session 문맥. */
    uint32_t request_id; /**< REQUEST_ID: App이 새 사용자 입력마다 생성 */
    NetRequestKind kind; /**< REQUEST_KIND: 1=도어; 2=공조; 3=실내 조명; 4=디지털 키 */
    uint8_t operation; /**< OPERATION: 요청 종류별 동작 코드 */
    NetMobileRequestArgument argument; /**< ARG_0~5의 기능별 의미. Codec이 선택 멤버를 인코딩하고 나머지를 0으로 채운다. */
} NetMobileRequest;
```

<a id="type-NetMobileQuery"></a>
### `NetMobileQuery`

출처: `S4A_RESULT/data/T15.md#type-NetMobileQuery`.

```c
typedef struct NetMobileQuery
{
    NetAppContext context; /**< 요청 단말·Session 문맥. */
    uint32_t query_id; /**< QUERY_ID: 이번 조회 식별, 0 금지 */
    NetQueryScope query_scope; /**< QUERY_SCOPE: 0=현재 상태; 1=요청 결과; 2=경고; 3=전체 */
    uint64_t request_session; /**< REQUEST_SESSION: 조회할 원 요청 Session; 상태 조회면 0 */
    uint32_t request_id; /**< REQUEST_ID: 조회할 요청; 상태 조회면 0 */
    uint16_t cursor; /**< CURSOR: 이 초안에서는 0 */
} NetMobileQuery;
```

<a id="type-NetMobileWarningAck"></a>
### `NetMobileWarningAck`

출처: `S4A_RESULT/data/T15.md#type-NetMobileWarningAck`.

```c
typedef struct NetMobileWarningAck
{
    NetAppContext context; /**< 요청 단말·Session 문맥. */
    NetWarningType warning_type; /**< WARNING_TYPE: 경고 종류 */
    uint32_t occurrence_id; /**< OCCURRENCE_ID: 읽은 경고 발생 번호 */
    uint32_t warning_domain_boot; /**< WARNING_DOMAIN_BOOT: 읽은 경고의 Domain 기동 번호 */
} NetMobileWarningAck;
```

<a id="type-NetMobileDomainAlive"></a>
### `NetMobileDomainAlive`

출처: `S4A_RESULT/data/T15.md#type-NetMobileDomainAlive`.

```c
typedef struct NetMobileDomainAlive
{
    uint32_t domain_boot_id; /**< DOMAIN_BOOT_ID: Domain 현재 기동 번호 */
    uint32_t update_sequence; /**< UPDATE_SEQUENCE: Domain 생존 평가 번호 */
    uint16_t source_age_ms; /**< SOURCE_AGE: 평가 후 ms */
    NetControllerState domain_state; /**< DOMAIN_STATE: 0=INIT; 1=READY; 2=DEGRADED; 3=FAULT */
    NetValueQuality value_quality; /**< VALUE_QUALITY: VALUE_QUALITY */
} NetMobileDomainAlive;
```

<a id="type-NetMobileResult"></a>
### `NetMobileResult`

출처: `S4A_RESULT/data/T15.md#type-NetMobileResult`.

```c
typedef struct NetMobileResult
{
    NetDownContext context; /**< 현재 수신 문맥·Domain 기동·조회 식별. */
    uint64_t request_session; /**< REQUEST_SESSION: 원 요청 Session */
    uint32_t request_id; /**< REQUEST_ID: 원 요청 ID */
    NetVehicleResult result; /**< RESULT: 공통 차량 Result; 확인 기록 없으면 255=필드 없음 */
    NetReason result_reason; /**< RESULT_REASON: 공통 사유 코드 */
    NetResultConfirmation result_confirmation; /**< RESULT_CONFIRMATION: 0=UNCONFIRMED; 1=CONFIRMED */
    NetCommandTarget command_target; /**< COMMAND_TARGET: 0=Domain 설정; 1=BCM; 2=WINDOW */
    uint32_t command_id; /**< COMMAND_ID: 실행 명령 연계; 설정만 반영한 경우 0 */
    NetOrigin origin; /**< ORIGIN: 요청 출처 코드 */
    uint16_t result_age_ms; /**< RESULT_AGE: 결과 확인 후 ms */
} NetMobileResult;
```

<a id="type-NetMobileWarning"></a>
### `NetMobileWarning`

출처: `S4A_RESULT/data/T15.md#type-NetMobileWarning`.

```c
typedef struct NetMobileWarning
{
    NetDownContext context; /**< 현재 수신 문맥·Domain 기동·조회 식별. */
    NetWarningType warning_type; /**< WARNING_TYPE: 경고 종류 */
    uint32_t occurrence_id; /**< OCCURRENCE_ID: Domain 기동 내 경고 발생 번호 */
    NetSeverity severity; /**< SEVERITY: 0=INFO; 1=CAUTION; 2=EMERGENCY */
    NetFlag warning_state; /**< WARNING_STATE: 0=CLEAR; 1=ACTIVE */
    NetFlag read_state; /**< READ_STATE: 0=UNREAD; 1=READ */
    NetValueQuality value_quality; /**< VALUE_QUALITY: VALUE_QUALITY */
    NetQualityReason quality_reason; /**< QUALITY_REASON: 품질 사유 */
    uint32_t source_boot; /**< SOURCE_BOOT: 원 관측 생성 노드 기동 번호 */
    uint32_t source_sequence; /**< SOURCE_SEQUENCE: 원 관측 번호 */
    uint16_t source_age_ms; /**< SOURCE_AGE: 원 관측 후 누적 ms */
} NetMobileWarning;
```

<a id="type-NetMobileAvailability"></a>
### `NetMobileAvailability`

출처: `S4A_RESULT/data/T15.md#type-NetMobileAvailability`.

```c
typedef struct NetMobileAvailability
{
    NetDownContext context; /**< 현재 수신 문맥·Domain 기동·조회 식별. */
    NetAvailabilityItem items[NET_AVAILABILITY_ITEM_COUNT]; /**< 원본 기능 ID 1~9의 순서. WINDOW 항목의 설계를 유지한다. */
    uint32_t update_sequence; /**< 새 가용성 평가 번호. */
    uint16_t source_age_ms; /**< 평가 후 제출 시점까지의 경과 시간. */
} NetMobileAvailability;
```

<a id="type-NetMobileDigitalStatus"></a>
### `NetMobileDigitalStatus`

출처: `S4A_RESULT/data/T15.md#type-NetMobileDigitalStatus`.

```c
typedef struct NetMobileDigitalStatus
{
    NetDownContext context; /**< 현재 수신 문맥·Domain 기동·조회 식별. */
    NetFlag digital_key_setting_state; /**< DIGITAL_KEY_SETTING_STATE: 0=OFF; 1=ON */
    NetAvailability digital_key_availability; /**< DIGITAL_KEY_AVAILABILITY: 0=AVAILABLE; 1=LIMITED; 2=UNAVAILABLE */
    NetReason reason; /**< REASON: 공통 사유 코드 */
    uint32_t update_sequence; /**< UPDATE_SEQUENCE: 새 판단/설정 평가 */
    uint16_t source_age_ms; /**< SOURCE_AGE: 평가 후 ms */
} NetMobileDigitalStatus;
```

<a id="type-NetMobileDigitalResult"></a>
### `NetMobileDigitalResult`

출처: `S4A_RESULT/data/T15.md#type-NetMobileDigitalResult`.

```c
typedef struct NetMobileDigitalResult
{
    NetDownContext context; /**< 현재 수신 문맥·Domain 기동·조회 식별. */
    uint32_t auto_occurrence_id; /**< AUTO_OCCURRENCE_ID: 자동 Unlock 실행 식별 */
    uint32_t related_door_command_id; /**< RELATED_DOOR_COMMAND_ID: BCM 도어 명령 연계 */
    NetOrigin origin; /**< ORIGIN: 2=PROXIMITY_AUTO */
    NetVehicleResult result; /**< RESULT: 공통 Result; 기록 없으면 255=필드 없음 */
    NetReason result_reason; /**< RESULT_REASON: 공통 사유 코드 */
    NetResultConfirmation result_confirmation; /**< RESULT_CONFIRMATION: 0=UNCONFIRMED; 1=CONFIRMED */
    uint16_t result_age_ms; /**< RESULT_AGE: 결과 확인 후 ms */
} NetMobileDigitalResult;
```

<a id="type-NetMobileUserSettings"></a>
### `NetMobileUserSettings`

출처: `S4A_RESULT/data/T15.md#type-NetMobileUserSettings`.

```c
typedef struct NetMobileUserSettings
{
    NetDownContext context; /**< 현재 수신 문맥·Domain 기동·조회 식별. */
    int16_t target_temperature_centi_c; /**< TARGET_TEMPERATURE: Domain이 반영한 목표 온도, 0.01°C/raw, -32768=미확인 */
    NetTriState climate_auto; /**< CLIMATE_AUTO: 0=OFF; 1=ON; 255=미확인 */
    NetFanLevel user_fan_level; /**< USER_FAN_LEVEL: 사용자가 설정한 FAN 코드 */
    NetTriState interior_light_enabled; /**< INTERIOR_LIGHT_ENABLED: 0=OFF; 1=ON; 255=미확인 */
    uint8_t normal_light_level_percent; /**< NORMAL_LIGHT_LEVEL: 사용자 NORMAL 밝기,0~100% */
    NetRgb normal_light_rgb; /**< NORMAL_LIGHT_RGB: 사용자 NORMAL 색상 R/G/B */
    NetValueQuality value_quality; /**< VALUE_QUALITY: 설정 확인 품질 */
    uint32_t update_sequence; /**< UPDATE_SEQUENCE: 현재 설정 확인 평가 번호 */
    uint16_t source_age_ms; /**< SOURCE_AGE: 설정 확인 평가 후 ms */
} NetMobileUserSettings;
```

<a id="type-NetMobileQueryEnd"></a>
### `NetMobileQueryEnd`

출처: `S4A_RESULT/data/T15.md#type-NetMobileQueryEnd`.

```c
typedef struct NetMobileQueryEnd
{
    NetDownContext context; /**< 현재 수신 문맥·Domain 기동·조회 식별. */
    NetQueryScope query_scope; /**< QUERY_SCOPE: 원 조회 범위 */
    NetQueryStatus query_status; /**< QUERY_STATUS: 0=완료; 1=요청 없음; 2=일부 확인 불가; 3=문맥 거부 */
    uint16_t item_count; /**< ITEM_COUNT: 조회 종료를 뺀 응답 메시지 수 */
} NetMobileQueryEnd;
```

<a id="type-NetMobileBcmDoorState"></a>
### `NetMobileBcmDoorState`

출처: `S4A_RESULT/data/T15.md#type-NetMobileBcmDoorState`.

```c
typedef struct NetMobileBcmDoorState
{
    NetDownContext context;
    NetCanSource source; /**< 원 CAN BOOT_ID/TX_SEQUENCE. Domain의 송신 번호로 바꾸지 않는다. */
    NetBcmDoorState state; /**< 원 값·품질·갱신 번호를 보존하고 AGE는 누적한다. CAN 패딩은 포함하지 않는다. */
} NetMobileBcmDoorState;
```

<a id="type-NetMobileBcmClimateState"></a>
### `NetMobileBcmClimateState`

출처: `S4A_RESULT/data/T15.md#type-NetMobileBcmClimateState`.

```c
typedef struct NetMobileBcmClimateState
{
    NetDownContext context;
    NetCanSource source; /**< 원 CAN BOOT_ID/TX_SEQUENCE. Domain의 송신 번호로 바꾸지 않는다. */
    NetBcmClimateState state; /**< 원 값·품질·갱신 번호를 보존하고 AGE는 누적한다. CAN 패딩은 포함하지 않는다. */
} NetMobileBcmClimateState;
```

<a id="type-NetMobileBcmLightState"></a>
### `NetMobileBcmLightState`

출처: `S4A_RESULT/data/T15.md#type-NetMobileBcmLightState`.

```c
typedef struct NetMobileBcmLightState
{
    NetDownContext context;
    NetCanSource source; /**< 원 CAN BOOT_ID/TX_SEQUENCE. Domain의 송신 번호로 바꾸지 않는다. */
    NetBcmLightState state; /**< 원 값·품질·갱신 번호를 보존하고 AGE는 누적한다. CAN 패딩은 포함하지 않는다. */
} NetMobileBcmLightState;
```

<a id="type-NetMobileBcmStatus"></a>
### `NetMobileBcmStatus`

출처: `S4A_RESULT/data/T15.md#type-NetMobileBcmStatus`.

```c
typedef struct NetMobileBcmStatus
{
    NetDownContext context;
    NetCanSource source; /**< 원 CAN BOOT_ID/TX_SEQUENCE. Domain의 송신 번호로 바꾸지 않는다. */
    NetBcmStatus state; /**< 원 값·품질·갱신 번호를 보존하고 AGE는 누적한다. CAN 패딩은 포함하지 않는다. */
} NetMobileBcmStatus;
```

<a id="type-NetMobileCisEnvironment"></a>
### `NetMobileCisEnvironment`

출처: `S4A_RESULT/data/T15.md#type-NetMobileCisEnvironment`.

```c
typedef struct NetMobileCisEnvironment
{
    NetDownContext context;
    NetCanSource source; /**< 원 CAN BOOT_ID/TX_SEQUENCE. Domain의 송신 번호로 바꾸지 않는다. */
    NetCisEnvironment state; /**< 원 값·품질·갱신 번호를 보존하고 AGE는 누적한다. CAN 패딩은 포함하지 않는다. */
} NetMobileCisEnvironment;
```

<a id="type-NetMobileCisOccupant"></a>
### `NetMobileCisOccupant`

출처: `S4A_RESULT/data/T15.md#type-NetMobileCisOccupant`.

```c
typedef struct NetMobileCisOccupant
{
    NetDownContext context;
    NetCanSource source; /**< 원 CAN BOOT_ID/TX_SEQUENCE. Domain의 송신 번호로 바꾸지 않는다. */
    NetCisOccupant state; /**< 원 값·품질·갱신 번호를 보존하고 AGE는 누적한다. CAN 패딩은 포함하지 않는다. */
} NetMobileCisOccupant;
```

<a id="type-NetMobileCisRear"></a>
### `NetMobileCisRear`

출처: `S4A_RESULT/data/T15.md#type-NetMobileCisRear`.

```c
typedef struct NetMobileCisRear
{
    NetDownContext context;
    NetCanSource source; /**< 원 CAN BOOT_ID/TX_SEQUENCE. Domain의 송신 번호로 바꾸지 않는다. */
    NetCisRear state; /**< 원 값·품질·갱신 번호를 보존하고 AGE는 누적한다. CAN 패딩은 포함하지 않는다. */
} NetMobileCisRear;
```

<a id="type-NetMobileCisStatus"></a>
### `NetMobileCisStatus`

출처: `S4A_RESULT/data/T15.md#type-NetMobileCisStatus`.

```c
typedef struct NetMobileCisStatus
{
    NetDownContext context;
    NetCanSource source; /**< 원 CAN BOOT_ID/TX_SEQUENCE. Domain의 송신 번호로 바꾸지 않는다. */
    NetCisStatus state; /**< 원 값·품질·갱신 번호를 보존하고 AGE는 누적한다. CAN 패딩은 포함하지 않는다. */
} NetMobileCisStatus;
```

<a id="type-NetMobileWindowState"></a>
### `NetMobileWindowState`

출처: `S4A_RESULT/data/T15.md#type-NetMobileWindowState`.

```c
typedef struct NetMobileWindowState
{
    NetDownContext context;
    NetCanSource source; /**< 원 CAN BOOT_ID/TX_SEQUENCE. Domain의 송신 번호로 바꾸지 않는다. */
    NetWindowState state; /**< 원 값·품질·갱신 번호를 보존하고 AGE는 누적한다. CAN 패딩은 포함하지 않는다. */
} NetMobileWindowState;
```

<a id="type-NetMobileWindowFault"></a>
### `NetMobileWindowFault`

출처: `S4A_RESULT/data/T15.md#type-NetMobileWindowFault`.

```c
typedef struct NetMobileWindowFault
{
    NetDownContext context;
    NetCanSource source; /**< 원 CAN BOOT_ID/TX_SEQUENCE. Domain의 송신 번호로 바꾸지 않는다. */
    NetWindowFault state; /**< 원 값·품질·갱신 번호를 보존하고 AGE는 누적한다. CAN 패딩은 포함하지 않는다. */
} NetMobileWindowFault;
```

<a id="type-NetMobileVssStatus"></a>
### `NetMobileVssStatus`

출처: `S4A_RESULT/data/T15.md#type-NetMobileVssStatus`.

```c
typedef struct NetMobileVssStatus
{
    NetDownContext context;
    NetCanSource source; /**< 원 CAN BOOT_ID/TX_SEQUENCE. Domain의 송신 번호로 바꾸지 않는다. */
    NetVssStatus state; /**< 원 값·품질·갱신 번호를 보존하고 AGE는 누적한다. CAN 패딩은 포함하지 않는다. */
} NetMobileVssStatus;
```

<a id="type-Gateway_Status_t"></a>
### `Gateway_Status_t`

출처: `S4A_RESULT/data/T20.md#type-Gateway_Status_t`.

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

<a id="type-Gateway_LinkId_t"></a>
### `Gateway_LinkId_t`

출처: `S4A_RESULT/data/T20.md#type-Gateway_LinkId_t`.

```c
typedef enum
{
    GATEWAY_LINK_BLUETOOTH = 0,
    GATEWAY_LINK_DOMAIN_UART
} Gateway_LinkId_t;
```

<a id="type-Gateway_LinkState_t"></a>
### `Gateway_LinkState_t`

출처: `S4A_RESULT/data/T20.md#type-Gateway_LinkState_t`.

```c
typedef enum
{
    GATEWAY_LINK_STATE_UNKNOWN = 0,
    GATEWAY_LINK_STATE_UNAVAILABLE,
    GATEWAY_LINK_STATE_AVAILABLE,
    GATEWAY_LINK_STATE_RECOVERING
} Gateway_LinkState_t;
```

<a id="type-Gateway_RegistrationState_t"></a>
### `Gateway_RegistrationState_t`

출처: `S4A_RESULT/data/T20.md#type-Gateway_RegistrationState_t`.

```c
typedef enum
{
    GATEWAY_REGISTRATION_UNKNOWN = 0,
    GATEWAY_NOT_REGISTERED,
    GATEWAY_REGISTERED
} Gateway_RegistrationState_t;
```

<a id="type-Gateway_ConnectionState_t"></a>
### `Gateway_ConnectionState_t`

출처: `S4A_RESULT/data/T20.md#type-Gateway_ConnectionState_t`.

```c
typedef enum
{
    GATEWAY_CONNECTION_UNKNOWN = 0,
    GATEWAY_DISCONNECTED,
    GATEWAY_CONNECTED
} Gateway_ConnectionState_t;
```

<a id="type-Gateway_AppActivityState_t"></a>
### `Gateway_AppActivityState_t`

출처: `S4A_RESULT/data/T20.md#type-Gateway_AppActivityState_t`.

```c
typedef enum
{
    GATEWAY_APP_ACTIVITY_UNKNOWN = 0,
    GATEWAY_APP_INACTIVE,
    GATEWAY_APP_ACTIVE
} Gateway_AppActivityState_t;
```

<a id="type-Gateway_ProximityState_t"></a>
### `Gateway_ProximityState_t`

출처: `S4A_RESULT/data/T20.md#type-Gateway_ProximityState_t`.

```c
typedef enum
{
    GATEWAY_PROXIMITY_UNKNOWN = 0,
    GATEWAY_PROXIMITY_FAR,
    GATEWAY_PROXIMITY_NEAR
} Gateway_ProximityState_t;
```

<a id="type-Gateway_DataQuality_t"></a>
### `Gateway_DataQuality_t`

출처: `S4A_RESULT/data/T20.md#type-Gateway_DataQuality_t`.

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

<a id="type-Gateway_UpdateBasis_t"></a>
### `Gateway_UpdateBasis_t`

출처: `S4A_RESULT/data/T20.md#type-Gateway_UpdateBasis_t`.

```c
typedef struct
{
    Gateway_DataQuality_t quality;
    uint32_t age_ms;
    uint32_t update_sequence;
    bool is_new_update;
} Gateway_UpdateBasis_t;
```

<a id="type-Gateway_RegistrationInfo_t"></a>
### `Gateway_RegistrationInfo_t`

출처: `S4A_RESULT/data/T20.md#type-Gateway_RegistrationInfo_t`.

```c
typedef struct
{
    Gateway_RegistrationState_t state;
    Gateway_UpdateBasis_t update;
} Gateway_RegistrationInfo_t;
```

<a id="type-Gateway_ConnectionInfo_t"></a>
### `Gateway_ConnectionInfo_t`

출처: `S4A_RESULT/data/T20.md#type-Gateway_ConnectionInfo_t`.

```c
typedef struct
{
    Gateway_ConnectionState_t state;
    Gateway_UpdateBasis_t update;
} Gateway_ConnectionInfo_t;
```

<a id="type-Gateway_AppActivityInfo_t"></a>
### `Gateway_AppActivityInfo_t`

출처: `S4A_RESULT/data/T20.md#type-Gateway_AppActivityInfo_t`.

```c
typedef struct
{
    Gateway_AppActivityState_t state;
    Gateway_UpdateBasis_t update;
} Gateway_AppActivityInfo_t;
```

<a id="type-Gateway_BluetoothContext_t"></a>
### `Gateway_BluetoothContext_t`

출처: `S4A_RESULT/data/T20.md#type-Gateway_BluetoothContext_t`.

```c
typedef struct
{
    Gateway_RegistrationInfo_t registration;
    Gateway_ConnectionInfo_t connection;
    Gateway_AppActivityInfo_t app_activity;
} Gateway_BluetoothContext_t;
```

<a id="type-Gateway_ProximityInfo_t"></a>
### `Gateway_ProximityInfo_t`

출처: `S4A_RESULT/data/T20.md#type-Gateway_ProximityInfo_t`.

```c
typedef struct
{
    Gateway_ProximityState_t state;
    Gateway_UpdateBasis_t update;
} Gateway_ProximityInfo_t;
```

<a id="type-Gateway_LogicalMessageType_t"></a>
### `Gateway_LogicalMessageType_t`

출처: `S4A_RESULT/data/T20.md#type-Gateway_LogicalMessageType_t`.

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

<a id="type-Gateway_MessageDirection_t"></a>
### `Gateway_MessageDirection_t`

출처: `S4A_RESULT/data/T20.md#type-Gateway_MessageDirection_t`.

```c
typedef enum
{
    GATEWAY_DIRECTION_UNKNOWN = 0,
    GATEWAY_DIRECTION_MOBILE_TO_DOMAIN,
    GATEWAY_DIRECTION_ESP32_TO_DOMAIN,
    GATEWAY_DIRECTION_DOMAIN_TO_MOBILE
} Gateway_MessageDirection_t;
```

<a id="type-Gateway_ByteView_t"></a>
### `Gateway_ByteView_t`

출처: `S4A_RESULT/data/T21.md#type-Gateway_ByteView_t`.

```c
typedef struct
{
    const uint8_t *data;
    size_t length;
} Gateway_ByteView_t;
```

<a id="type-Gateway_MessageContextView_t"></a>
### `Gateway_MessageContextView_t`

출처: `S4A_RESULT/data/T21.md#type-Gateway_MessageContextView_t`.

```c
typedef struct
{
    Gateway_ByteView_t vehicle_id;
    Gateway_ByteView_t device_context_id;
    Gateway_ByteView_t session_id;
    Gateway_ByteView_t request_id;
} Gateway_MessageContextView_t;
```

<a id="type-Gateway_RelayMessageView_t"></a>
### `Gateway_RelayMessageView_t`

출처: `S4A_RESULT/data/T21.md#type-Gateway_RelayMessageView_t`.

```c
typedef struct
{
    Gateway_LogicalMessageType_t type;
    Gateway_MessageDirection_t direction;
    Gateway_MessageContextView_t context;
    Gateway_ByteView_t payload;
} Gateway_RelayMessageView_t;
```

<a id="type-Gateway_RegistrationConnectionUpdate_t"></a>
### `Gateway_RegistrationConnectionUpdate_t`

출처: `S4A_RESULT/data/T21.md#type-Gateway_RegistrationConnectionUpdate_t`.

```c
typedef struct
{
    Gateway_MessageContextView_t context;
    Gateway_RegistrationState_t registration;
    Gateway_ConnectionState_t connection;
    Gateway_UpdateBasis_t update; /* ONE shared quality/age/update_sequence;
        DeviceRegistrationManager.observation_revision -> update_sequence */
} Gateway_RegistrationConnectionUpdate_t;
```

<a id="type-Gateway_AppActivityUpdate_t"></a>
### `Gateway_AppActivityUpdate_t`

출처: `S4A_RESULT/data/T21.md#type-Gateway_AppActivityUpdate_t`.

```c
typedef struct
{
    Gateway_MessageContextView_t context;
    Gateway_AppActivityState_t state;
    Gateway_UpdateBasis_t update;
} Gateway_AppActivityUpdate_t;
```

<a id="type-Gateway_ProximityUpdate_t"></a>
### `Gateway_ProximityUpdate_t`

출처: `S4A_RESULT/data/T21.md#type-Gateway_ProximityUpdate_t`.

```c
typedef struct
{
    Gateway_MessageContextView_t context;
    Gateway_ProximityInfo_t proximity;
} Gateway_ProximityUpdate_t;
```

<a id="type-Gateway_InterfaceMessageHandler_t"></a>
### `Gateway_InterfaceMessageHandler_t`

출처: `S4A_RESULT/data/T21.md#type-Gateway_InterfaceMessageHandler_t`.

```c
typedef Gateway_Status_t (*Gateway_InterfaceMessageHandler_t)(
    const Gateway_RelayMessageView_t *message,
    void *user_context);
```

<a id="type-Gateway_InterfaceCoreHandlers_t"></a>
### `Gateway_InterfaceCoreHandlers_t`

출처: `S4A_RESULT/data/T21.md#type-Gateway_InterfaceCoreHandlers_t`.

```c
typedef struct
{
    Gateway_InterfaceMessageHandler_t on_mobile_message;
    Gateway_InterfaceMessageHandler_t on_domain_message;
    void *user_context;
} Gateway_InterfaceCoreHandlers_t;
```

<a id="type-Gateway_InterfaceRelaySender_t"></a>
### `Gateway_InterfaceRelaySender_t`

출처: `S4A_RESULT/data/T21.md#type-Gateway_InterfaceRelaySender_t`.

```c
typedef Gateway_Status_t (*Gateway_InterfaceRelaySender_t)(
    const Gateway_RelayMessageView_t *message,
    void *user_context);
```

<a id="type-Gateway_InterfaceRegistrationSender_t"></a>
### `Gateway_InterfaceRegistrationSender_t`

출처: `S4A_RESULT/data/T21.md#type-Gateway_InterfaceRegistrationSender_t`.

```c
typedef Gateway_Status_t (*Gateway_InterfaceRegistrationSender_t)(
    const Gateway_RegistrationConnectionUpdate_t *update,
    void *user_context);
```

<a id="type-Gateway_InterfaceAppActivitySender_t"></a>
### `Gateway_InterfaceAppActivitySender_t`

출처: `S4A_RESULT/data/T21.md#type-Gateway_InterfaceAppActivitySender_t`.

```c
typedef Gateway_Status_t (*Gateway_InterfaceAppActivitySender_t)(
    const Gateway_AppActivityUpdate_t *update,
    void *user_context);
```

<a id="type-Gateway_InterfaceProximitySender_t"></a>
### `Gateway_InterfaceProximitySender_t`

출처: `S4A_RESULT/data/T21.md#type-Gateway_InterfaceProximitySender_t`.

```c
typedef Gateway_Status_t (*Gateway_InterfaceProximitySender_t)(
    const Gateway_ProximityUpdate_t *update,
    void *user_context);
```

<a id="type-Gateway_InterfaceTransportPorts_t"></a>
### `Gateway_InterfaceTransportPorts_t`

출처: `S4A_RESULT/data/T21.md#type-Gateway_InterfaceTransportPorts_t`.

```c
typedef struct
{
    Gateway_InterfaceRelaySender_t send_to_domain;
    Gateway_InterfaceRelaySender_t send_to_mobile;

    Gateway_InterfaceRegistrationSender_t send_registration_to_domain;
    Gateway_InterfaceAppActivitySender_t send_app_activity_to_domain;
    Gateway_InterfaceProximitySender_t send_proximity_to_domain;

    void *user_context;
} Gateway_InterfaceTransportPorts_t;
```

<a id="type-NetUartRxMeta"></a>
### `NetUartRxMeta`

출처: `S4A_RESULT/data/T37.md#type-NetUartRxMeta`.

```c
typedef struct NetUartRxMeta
{
    uint16_t link_sequence;  /**< 해당 방향의 전송 순번. 차량 요청 ID가 아니다. */
    uint32_t received_at_ms; /**< 로컬 단조 증가 ms. */
} NetUartRxMeta;
```

<a id="type-NetAppContext"></a>
### `NetAppContext`

출처: `S4A_RESULT/data/T15.md#type-NetAppContext`.

```c
typedef struct NetAppContext
{
    uint32_t device_context_id; /**< ESP32 등록 단말 식별. 0 예약. */
    uint64_t session_id;        /**< 현재 Session. 생성 방식은 ESP32/MOBILE 담당. */
} NetAppContext;
```

<a id="type-NetDownContext"></a>
### `NetDownContext`

출처: `S4A_RESULT/data/T15.md#type-NetDownContext`.

```c
typedef struct NetDownContext
{
    NetAppContext recipient;   /**< 현재 수신 단말·Session. */
    uint32_t domain_boot_id;    /**< 정보를 제공하는 현재 Domain 기동 번호. */
    uint32_t query_id;          /**< 자발 전송은 0. 조회 응답은 원 QUERY_ID. */
} NetDownContext;
```

<a id="type-DomainQueryResponseContext_t"></a>
### `DomainQueryResponseContext_t`

출처: `S4A_RESULT/data/T17.md#type-DomainQueryResponseContext_t`.

```c
typedef struct
{
    NetAppContext recipient;
    uint32_t queryId;
    NetQueryScope scope;
} DomainQueryResponseContext_t;
```

<a id="type-DigitalKeyConnectionState_t"></a>
### `DigitalKeyConnectionState_t`

출처: `S4A_RESULT/data/T13.md#type-DigitalKeyConnectionState_t`.

```c
typedef struct
{
    DeviceRegistrationState_t registration;
    BtConnectionState_t btConnection;
    AppActiveState_t appActive;
    SessionId_t sessionId;
    SignalMeta_t meta;    /* ONE coherent Registration/Connection quality/sequence/age */
    SignalMeta_t appMeta; /* App activity sequence/age/quality, independently */
} DigitalKeyConnectionState_t;
```

<a id="type-ProximityInput_t"></a>
### `ProximityInput_t`

출처: `S4A_RESULT/data/T13.md#type-ProximityInput_t`.

```c
typedef struct
{
    ProximityState_t state;
    ValueValidity_t validity;
    QualityReason_t reason;
    SignalMeta_t meta;
} ProximityInput_t;
```

<a id="type-DomainIf_MobileRequest_t"></a>
### `DomainIf_MobileRequest_t`

출처: `S4A_RESULT/data/T14.md#type-DomainIf_MobileRequest_t`.

```c
typedef struct
{
    DeviceContextId_t deviceContextId;
    RequestContext_t context;
    DomainIf_MobileRequestType_t type;
    DomainIf_MobileRequestPayload_t payload;
    SignalMeta_t meta;
} DomainIf_MobileRequest_t;
```

<a id="type-DomainIf_MobileRequestPayload_t"></a>
### `DomainIf_MobileRequestPayload_t`

출처: `S4A_RESULT/data/T14.md#type-DomainIf_MobileRequestPayload_t`.

```c
typedef union
{
    DoorLockTarget_t doorTarget;
    int16_t targetTemperature;
    bool enable;
    FanLevel_t fanLevel;
    uint8_t levelPercent;
    RgbColor_t color;
    DigitalKeySetting_t digitalKeySetting;
} DomainIf_MobileRequestPayload_t;
```

<a id="type-DomainIf_MobileRequestType_t"></a>
### `DomainIf_MobileRequestType_t`

출처: `S4A_RESULT/data/T14.md#type-DomainIf_MobileRequestType_t`.

```c
typedef enum
{
    MOBILE_REQUEST_DOOR_LOCK = 0,
    MOBILE_REQUEST_CLIMATE_TARGET_TEMPERATURE,
    MOBILE_REQUEST_CLIMATE_AUTO_ENABLE,
    MOBILE_REQUEST_FAN_LEVEL,
    MOBILE_REQUEST_INTERIOR_LIGHT_ENABLE,
    MOBILE_REQUEST_INTERIOR_LIGHT_LEVEL,
    MOBILE_REQUEST_INTERIOR_LIGHT_COLOR,
    MOBILE_REQUEST_DIGITAL_KEY_SETTING
} DomainIf_MobileRequestType_t;
```

<a id="type-DomainIf_WarningAck_t"></a>
### `DomainIf_WarningAck_t`

출처: `S4A_RESULT/data/T14.md#type-DomainIf_WarningAck_t`.

```c
typedef struct
{
    DeviceContextId_t deviceContextId;
    SessionId_t sessionId;
    DomainWarningType_t type;
    OccurrenceId_t occurrenceId;
} DomainIf_WarningAck_t;
```

<a id="type-DomainWarningType_t"></a>
### `DomainWarningType_t`

출처: `S4A_RESULT/data/T14.md#type-DomainWarningType_t`.

```c
typedef enum
{
    DOMAIN_WARNING_WINDOW_ANTIPINCH = 0,
    DOMAIN_WARNING_OCCUPANT_HAZARD,
    DOMAIN_WARNING_REAR_OBSTACLE,
    DOMAIN_WARNING_DOOR_LEFT_OPEN,
    DOMAIN_WARNING_BCM_FAULT,
    DOMAIN_WARNING_CIS_FAULT,
    DOMAIN_WARNING_WINDOW_FAULT,
    DOMAIN_WARNING_VSS_FAULT,
    DOMAIN_WARNING_COUNT
} DomainWarningType_t;
```

<a id="type-DomainIf_MobileRequestResult_t"></a>
### `DomainIf_MobileRequestResult_t`

출처: `S4A_RESULT/data/T14.md#type-DomainIf_MobileRequestResult_t`.

```c
typedef struct
{
    DeviceContextId_t deviceContextId;
    RequestResultInfo_t resultInfo; /* original Session + Request ID */
    bool hasVehicleResult;         /* false: resultInfo.result is not inspected */
    DomainIf_MobileResultTarget_t target;
    DomainCommandId_t commandId;   /* zero for a setting-only result */
    CommandOrigin_t origin;
    uint32_t resultAgeMs;          /* at API call; Network adds its own waiting */
} DomainIf_MobileRequestResult_t;
```

<a id="type-DomainIf_WarningInfo_t"></a>
### `DomainIf_WarningInfo_t`

출처: `S4A_RESULT/data/T14.md#type-DomainIf_WarningInfo_t`.

```c
typedef struct
{
    DomainWarningType_t type;
    DomainWarningSeverity_t severity;
    DomainWarningState_t state;
    OccurrenceId_t occurrenceId;
    bool isRead;
    bool simulated; /* Domain-local; no silent drop on export, see O-C05 */
    DataQuality_t quality;
    QualityReason_t qualityReason;
    SourceEvidence_t source;
} DomainIf_WarningInfo_t;
```

<a id="type-DomainIf_FunctionAvailabilitySet_t"></a>
### `DomainIf_FunctionAvailabilitySet_t`

출처: `S4A_RESULT/data/T14.md#type-DomainIf_FunctionAvailabilitySet_t`.

```c
typedef struct
{
    DomainIf_FunctionAvailability_t items[DOMAIN_FUNCTION_COUNT];
    SignalMeta_t meta;
} DomainIf_FunctionAvailabilitySet_t;
```

<a id="type-DigitalKeyState_t"></a>
### `DigitalKeyState_t`

출처: `S4A_RESULT/data/T13.md#type-DigitalKeyState_t`.

```c
typedef struct
{
    DigitalKeySetting_t setting;
    FunctionAvailability_t availability;
    UnlockOrigin_t lastUnlockOrigin; /* Domain internal observation */
    ResultReason_t reason;
    SignalMeta_t meta;
} DigitalKeyState_t;
```

<a id="type-DomainIf_DigitalKeyResult_t"></a>
### `DomainIf_DigitalKeyResult_t`

출처: `S4A_RESULT/data/T14.md#type-DomainIf_DigitalKeyResult_t`.

```c
typedef struct
{
    /**
     * false이면 현재 확정 가능한 차량 실행 결과가 없음.
     * confirmation=UNCONFIRMED과 함께 MOBILE에서는 UNKNOWN 의미로 표시 가능.
     */
    bool hasVehicleResult;

    RequestResult_t result;
    ResultReason_t reason;
    ResultConfirmation_t confirmation;

    UnlockOrigin_t origin;
    DomainCommandId_t relatedDoorCommandId;
    OccurrenceId_t occurrenceId; /* automatic unlock execution identity */
    uint32_t resultAgeMs;

    /**
     * 실제 BCM 잠금 해제 전이가 확인된 경우만 true.
     * ALREADY_AT_TARGET + DONE은 false.
     */
    bool actualUnlockTransition;
} DomainIf_DigitalKeyResult_t;
```

<a id="type-DomainIf_UserSettingsState_t"></a>
### `DomainIf_UserSettingsState_t`

출처: `S4A_RESULT/data/T14.md#type-DomainIf_UserSettingsState_t`.

```c
typedef struct
{
    bool hasTargetTemperature;
    int16_t targetTemperatureCentiC;
    bool hasClimateAutoSetting;
    bool climateAutoEnabled;
    bool hasUserFanLevel;
    FanLevel_t userFanLevel;
    bool hasInteriorLightEnableSetting;
    bool interiorLightEnabled;
    bool hasNormalLightLevel;
    uint8_t normalLightLevelPercent;
    bool hasNormalLightColor;
    RgbColor_t normalLightColor;
    SignalMeta_t meta;
} DomainIf_UserSettingsState_t;
```

<a id="type-NetAvailabilityItem"></a>
### `NetAvailabilityItem`

출처: `S4A_RESULT/data/T15.md#type-NetAvailabilityItem`.

```c
typedef struct NetAvailabilityItem
{
    NetFunctionId function_id;
    NetAvailability availability;
    NetReason reason;
} NetAvailabilityItem;
```

<a id="type-DomainIf_WindowFaultState_t"></a>
### `DomainIf_WindowFaultState_t`

출처: `S4A_RESULT/data/T14.md#type-DomainIf_WindowFaultState_t`.

```c
typedef struct
{
    FaultInfo_t fault;
    OccurrenceId_t occurrenceId;
    SignalMeta_t meta;
    SourceGeneration_t sourceGeneration;
} DomainIf_WindowFaultState_t;
```

<a id="type-NetRequestKind"></a>
### `NetRequestKind`

출처: `S4A_RESULT/data/T15.md#type-NetRequestKind`.

```c
typedef uint8_t NetRequestKind;
```

<a id="type-NetQueryScope"></a>
### `NetQueryScope`

출처: `S4A_RESULT/data/T15.md#type-NetQueryScope`.

```c
typedef uint8_t NetQueryScope;
```

<a id="type-NetWarningType"></a>
### `NetWarningType`

출처: `S4A_RESULT/data/T37.md#type-NetWarningType`.

```c
typedef uint8_t NetWarningType;
```

<a id="type-NetResultConfirmation"></a>
### `NetResultConfirmation`

출처: `S4A_RESULT/data/T37.md#type-NetResultConfirmation`.

```c
typedef uint8_t NetResultConfirmation;
```

<a id="type-NetCommandTarget"></a>
### `NetCommandTarget`

출처: `S4A_RESULT/data/T15.md#type-NetCommandTarget`.

```c
typedef uint8_t NetCommandTarget;
```

<a id="type-NetSeverity"></a>
### `NetSeverity`

출처: `S4A_RESULT/data/T37.md#type-NetSeverity`.

```c
typedef uint8_t NetSeverity;
```

<a id="type-NetQueryStatus"></a>
### `NetQueryStatus`

출처: `S4A_RESULT/data/T15.md#type-NetQueryStatus`.

```c
typedef uint8_t NetQueryStatus;
```

<a id="type-DeviceRegistrationState_t"></a>
### `DeviceRegistrationState_t`

출처: `S4A_RESULT/data/T13.md#type-DeviceRegistrationState_t`.

```c
typedef enum
{
    DEVICE_REGISTRATION_UNKNOWN = 0,
    DEVICE_REGISTRATION_NOT_REGISTERED,
    DEVICE_REGISTRATION_REGISTERED
} DeviceRegistrationState_t;
```

<a id="type-BtConnectionState_t"></a>
### `BtConnectionState_t`

출처: `S4A_RESULT/data/T13.md#type-BtConnectionState_t`.

```c
typedef enum
{
    BT_CONNECTION_UNKNOWN = 0,
    BT_CONNECTION_DISCONNECTED,
    BT_CONNECTION_CONNECTED
} BtConnectionState_t;
```

<a id="type-AppActiveState_t"></a>
### `AppActiveState_t`

출처: `S4A_RESULT/data/T13.md#type-AppActiveState_t`.

```c
typedef enum
{
    APP_ACTIVE_UNKNOWN = 0,
    APP_ACTIVE_INACTIVE,
    APP_ACTIVE_ACTIVE
} AppActiveState_t;
```

<a id="type-ProximityState_t"></a>
### `ProximityState_t`

출처: `S4A_RESULT/data/T13.md#type-ProximityState_t`.

```c
typedef enum
{
    PROXIMITY_UNKNOWN = 0,
    PROXIMITY_FAR,
    PROXIMITY_NEAR
} ProximityState_t;
```

<a id="type-DigitalKeySetting_t"></a>
### `DigitalKeySetting_t`

출처: `S4A_RESULT/data/T13.md#type-DigitalKeySetting_t`.

```c
typedef enum
{
    DIGITAL_KEY_SETTING_OFF = 0,
    DIGITAL_KEY_SETTING_ON
} DigitalKeySetting_t;
```

<a id="type-RequestResultInfo_t"></a>
### `RequestResultInfo_t`

출처: `S4A_RESULT/data/T13.md#type-RequestResultInfo_t`.

```c
typedef struct
{
    RequestContext_t context;
    RequestResult_t result;
    ResultReason_t reason;
    ResultConfirmation_t confirmation;
} RequestResultInfo_t;
```

<a id="type-DomainIf_MobileResultTarget_t"></a>
### `DomainIf_MobileResultTarget_t`

출처: `S4A_RESULT/data/T14.md#type-DomainIf_MobileResultTarget_t`.

```c
typedef enum
{
    MOBILE_RESULT_TARGET_DOMAIN_SETTING = 0,
    MOBILE_RESULT_TARGET_BCM,
    MOBILE_RESULT_TARGET_WINDOW /* [구현 보류 — 설계 유지] */
} DomainIf_MobileResultTarget_t;
```

<a id="type-DomainWarningSeverity_t"></a>
### `DomainWarningSeverity_t`

출처: `S4A_RESULT/data/T14.md#type-DomainWarningSeverity_t`.

```c
typedef enum
{
    DOMAIN_WARNING_SEVERITY_INFO = 0,
    DOMAIN_WARNING_SEVERITY_CAUTION,
    DOMAIN_WARNING_SEVERITY_EMERGENCY
} DomainWarningSeverity_t;
```

<a id="type-DomainWarningState_t"></a>
### `DomainWarningState_t`

출처: `S4A_RESULT/data/T14.md#type-DomainWarningState_t`.

```c
typedef enum
{
    DOMAIN_WARNING_STATE_CLEAR = 0,
    DOMAIN_WARNING_STATE_ACTIVE,
    DOMAIN_WARNING_STATE_UNCONFIRMED
} DomainWarningState_t;
```

<a id="type-DomainIf_FunctionAvailability_t"></a>
### `DomainIf_FunctionAvailability_t`

출처: `S4A_RESULT/data/T14.md#type-DomainIf_FunctionAvailability_t`.

```c
typedef struct
{
    DomainFunctionId_t functionId;
    FunctionAvailability_t availability;
    ResultReason_t reason;
} DomainIf_FunctionAvailability_t;
```

<a id="type-UnlockOrigin_t"></a>
### `UnlockOrigin_t`

출처: `S4A_RESULT/data/T13.md#type-UnlockOrigin_t`.

```c
typedef enum
{
    UNLOCK_ORIGIN_USER_REQUEST = 0,
    UNLOCK_ORIGIN_PROXIMITY_AUTO
} UnlockOrigin_t;
```

<a id="type-NetFunctionId"></a>
### `NetFunctionId`

출처: `S4A_RESULT/data/T37.md#type-NetFunctionId`.

```c
typedef uint8_t NetFunctionId;
```

<a id="type-NetRequestKindCode"></a>
### `NetRequestKindCode`

출처: `S4A_RESULT/data/T15.md#type-NetRequestKindCode`.

```c
enum NetRequestKindCode
{
    NET_REQUEST_DOOR = 1,
    NET_REQUEST_CLIMATE = 2,
    NET_REQUEST_INTERIOR_LIGHT = 3,
    NET_REQUEST_DIGITAL_KEY = 4
};
```

<a id="type-NetQueryScopeCode"></a>
### `NetQueryScopeCode`

출처: `S4A_RESULT/data/T15.md#type-NetQueryScopeCode`.

```c
enum NetQueryScopeCode
{
    NET_QUERY_SCOPE_STATE = 0,
    NET_QUERY_SCOPE_RESULT = 1,
    NET_QUERY_SCOPE_WARNING = 2,
    NET_QUERY_SCOPE_ALL = 3
};
```

<a id="type-NetWarningTypeCode"></a>
### `NetWarningTypeCode`

출처: `S4A_RESULT/data/T37.md#type-NetWarningTypeCode`.

```c
enum NetWarningTypeCode
{
    NET_WARNING_REAR = 1,
    NET_WARNING_OCCUPANT = 2,
    NET_WARNING_ANTIPINCH = 3,
    NET_WARNING_BCM_FAULT = 4,
    NET_WARNING_CIS_FAULT = 5,
    NET_WARNING_WINDOW_FAULT = 6,
    NET_WARNING_VSS_FAULT = 7,
    NET_WARNING_OPEN_DOOR_AFTER_LEAVE = 8
};
```

<a id="type-NetResultConfirmationCode"></a>
### `NetResultConfirmationCode`

출처: `S4A_RESULT/data/T37.md#type-NetResultConfirmationCode`.

```c
enum NetResultConfirmationCode
{
    NET_CONFIRMATION_UNCONFIRMED = 0,
    NET_CONFIRMATION_CONFIRMED = 1
};
```

<a id="type-NetCommandTargetCode"></a>
### `NetCommandTargetCode`

출처: `S4A_RESULT/data/T15.md#type-NetCommandTargetCode`.

```c
enum NetCommandTargetCode
{
    NET_COMMAND_TARGET_DOMAIN_SETTING = 0,
    NET_COMMAND_TARGET_BCM = 1,
    NET_COMMAND_TARGET_WINDOW = 2
};
```

<a id="type-NetSeverityCode"></a>
### `NetSeverityCode`

출처: `S4A_RESULT/data/T37.md#type-NetSeverityCode`.

```c
enum NetSeverityCode
{
    NET_SEVERITY_INFO = 0,
    NET_SEVERITY_CAUTION = 1,
    NET_SEVERITY_EMERGENCY = 2
};
```

<a id="type-NetQueryStatusCode"></a>
### `NetQueryStatusCode`

출처: `S4A_RESULT/data/T15.md#type-NetQueryStatusCode`.

```c
enum NetQueryStatusCode
{
    NET_QUERY_STATUS_DONE = 0,
    NET_QUERY_STATUS_NOT_FOUND = 1,
    NET_QUERY_STATUS_PARTIAL_UNCONFIRMED = 2,
    NET_QUERY_STATUS_CONTEXT_REJECTED = 3
};
```

<a id="type-DomainFunctionId_t"></a>
### `DomainFunctionId_t`

출처: `S4A_RESULT/data/T14.md#type-DomainFunctionId_t`.

```c
typedef enum
{
    DOMAIN_FUNCTION_DOOR = 0,
    DOMAIN_FUNCTION_CLIMATE,
    DOMAIN_FUNCTION_INTERIOR_LIGHT,
    DOMAIN_FUNCTION_DIGITAL_KEY,
    DOMAIN_FUNCTION_WINDOW,
    DOMAIN_FUNCTION_REAR_WARNING,
    DOMAIN_FUNCTION_OCCUPANT_WARNING,
    DOMAIN_FUNCTION_VSS,
    DOMAIN_FUNCTION_ENVIRONMENT,
    DOMAIN_FUNCTION_COUNT
} DomainFunctionId_t;
```

<a id="type-NetFunctionIdCode"></a>
### `NetFunctionIdCode`

출처: `S4A_RESULT/data/T37.md#type-NetFunctionIdCode`.

```c
enum NetFunctionIdCode
{
    NET_FUNCTION_DOOR = 1,
    NET_FUNCTION_CLIMATE = 2,
    NET_FUNCTION_INTERIOR_LIGHT = 3,
    NET_FUNCTION_DIGITAL_KEY = 4,
    NET_FUNCTION_OCCUPANT = 5,
    NET_FUNCTION_ENVIRONMENT = 6,
    NET_FUNCTION_REAR_WARNING = 7,
    NET_FUNCTION_WINDOW = 8,
    NET_FUNCTION_VSS = 9
};
```

## identity·시간과 borrowed 자료

`NetMobileAvailability.items[NET_AVAILABILITY_ITEM_COUNT]`는 봉인 S4A의 원 선언 그대로다. Wire 필드표는 9개 기능 항목을 명시하지만 제공된 선언 자료에서는 이 매크로의 실제 정의·값·제품 Header/ABI를 확인하지 못했다. 문서의 9항목 배치를 매크로 정의 검증 완료로 표현하지 않는다. 실제 정의와 양단 고정 순서 확인은 기존 IF-O23/C2-H01·S6Q-10/14에 연결된 HOLD이며 이번에 헤더/매크로를 생성하지 않는다.

`NetAppContext.session_id`는 현재 상대의 u64 Session, `NetDownContext.domain_boot_id`는 현재 DOMAIN 기동, projection 안의 CAN BOOT/TX는 원 ECU, PiBoot는 원 비전 기동이다. 현재 recipient Session과 M_RESULT의 과거 Request Session을 합치지 않는다. M_DIGITAL_RESULT에는 과거 Request Session 필드가 없으며 자동 occurrence와 관련 command 식별을 보존한다. QueryId는 RequestId·OccurrenceId·LINK_SEQUENCE와 다른 값이다.

ByteView의 주소/length는 호출 중 borrowed다. local OK 전에는 중첩 data까지 안정 복사 또는 안전한 동기 encode를 끝낸다. table/user_context의 수명도 외부 Owner 계약으로 확인한다. 폭이 4/8/4인 Device/Session/Request view와 empty view를 형식 검사하고, Query/Occurrence를 request_id view에 끼워 넣지 않는다.

u16 AGE65535는 포화 하한이다. 프레임 수신·새 LINK_SEQUENCE·Query 재포장으로 원 AGE를 0으로 바꾸지 않는다. `DomainIf_CommandContext_t`의 commandAgeMs와 meta.ageMs는 별개이며 M_REQUEST Wire에는 원 MOBILE SENT/AGE가 없다. 그 부재를 임의 로컬시각으로 채우지 않는다.

기존 Gateway category와 내부 ByteView `TYPE1B+논리payload` 후보는 UART raw `SYNC/VERSION/CRC`와 구분한다. subtype의 실제 Core/Transport 합의가 없으면 형식이 맞아도 소비는 HOLD다. UNKNOWN/UNSUPPORTED 등에 대응 raw가 없으면 의미를 줄여 전송하지 않는다.
