# CIS Data — 값별 원 관측과 Pi 취합·상태

[전체 시작](../../00_START_HERE.md) · [개요](00_OVERVIEW.md) · [Data](10_DATA.md) · [API](20_API.md)

**PROPOSED_DESIGN · BINDING_OPEN.** 아래 전체 선언은 의미 타입을 표시하는 설계용 C 표현이다. 실제 설치 헤더/firmware가 아니며 struct padding·enum 크기를 raw bytes로 전송하지 않는다. [공통 ID·수명](../../04_COMMON/10_DATA_AND_ID_RULES.md)과 [C enum↔Wire 명시 변환](../../04_COMMON/30_ENUM_WIRE_MAPPING.md)을 함께 적용한다.

## 타입 빠른 찾기

| 타입 | 생성·소비 / 관련 메시지 |
|---|---|
| [`NetCisEnvironment`](#type-netcisenvironment) | CIS 센싱·진단 → DOMAIN · [`CIS_ENVIRONMENT`](../../03_COMMUNICATION/10_CAN_FD.md#message-cis-environment) |
| [`NetCisOccupant`](#type-netcisoccupant) | Pi 원 관측 + CIS 품질/기능 평가 → DOMAIN · [`CIS_OCCUPANT`](../../03_COMMUNICATION/10_CAN_FD.md#message-cis-occupant) |
| [`NetCisRear`](#type-netcisrear) | CIS 센싱·진단 → DOMAIN · [`CIS_REAR`](../../03_COMMUNICATION/10_CAN_FD.md#message-cis-rear) |
| [`NetCisStatus`](#type-netcisstatus) | CIS 센싱·진단 → DOMAIN · [`CIS_STATUS`](../../03_COMMUNICATION/10_CAN_FD.md#message-cis-status) |
| [`NetCisState`](#type-netcisstate) | 노드 전용 코드·bit 집합 |
| [`NetInterfaceState`](#type-netinterfacestate) | 노드 전용 코드·bit 집합 |
| [`NetProximityStatus`](#type-netproximitystatus) | 노드 전용 코드·bit 집합 |
| [`NetCisFaultMask`](#type-netcisfaultmask) | 노드 전용 코드·bit 집합 |
| [`NetCisFaultCode`](#type-netcisfaultcode) | 노드 전용 코드·bit 집합 |

이 문서와 로컬 API가 사용하는 공용 타입의 전체 선언: [`NetSubmitResult`](../../04_COMMON/10_DATA_AND_ID_RULES.md#type-netsubmitresult), [`NetCanRxMeta`](../../04_COMMON/10_DATA_AND_ID_RULES.md#type-netcanrxmeta), [`NetUartRxMeta`](../../04_COMMON/10_DATA_AND_ID_RULES.md#type-netuartrxmeta), [`NetDomainPermission`](../../04_COMMON/10_DATA_AND_ID_RULES.md#type-netdomainpermission), [`NetValueQuality`](../../04_COMMON/10_DATA_AND_ID_RULES.md#type-netvaluequality), [`NetValidity`](../../04_COMMON/10_DATA_AND_ID_RULES.md#type-netvalidity), [`NetQualityReason`](../../04_COMMON/10_DATA_AND_ID_RULES.md#type-netqualityreason), [`NetTriState`](../../04_COMMON/10_DATA_AND_ID_RULES.md#type-nettristate), [`NetFlag`](../../04_COMMON/10_DATA_AND_ID_RULES.md#type-netflag), [`NetSensorFunctionState`](../../04_COMMON/10_DATA_AND_ID_RULES.md#type-netsensorfunctionstate).

CIS가 소비하는 UART-C 타입은 [Pi의 전체 record](../PI/10_DATA.md)에 한 번만 정의한다: [`NetVisionResult`](../PI/10_DATA.md#type-netvisionresult), [`NetVisionStatus`](../PI/10_DATA.md#type-netvisionstatus), [`NetPiPermission`](../PI/10_DATA.md#type-netpipermission). 허용 입력은 [`NetDomainPermission`](../../04_COMMON/10_DATA_AND_ID_RULES.md#type-netdomainpermission)이다.

<a id="type-netcisenvironment"></a>
## `NetCisEnvironment`

CIS 센싱·진단 → DOMAIN · [`CIS_ENVIRONMENT`](../../03_COMMUNICATION/10_CAN_FD.md#message-cis-environment)

```c
typedef struct NetCisEnvironment
{
    int16_t temperature_centi_c; /**< TEMPERATURE: 0.01°C/raw; -4000~12500; -32768=무효 */
    uint32_t temperature_sequence; /**< TEMPERATURE_SEQUENCE: 해당 값의 새 측정 번호 */
    uint16_t temperature_age_ms; /**< TEMPERATURE_AGE: 해당 값 원본 측정 후 ms */
    NetValidity temperature_validity; /**< TEMPERATURE_VALIDITY: 0=INVALID; 1=VALID */
    NetQualityReason temperature_reason; /**< TEMPERATURE_REASON: CIS 품질 사유 */
    uint16_t humidity_centi_percent; /**< HUMIDITY: 0.01%RH/raw; 0~10000; 65535=무효 */
    uint32_t humidity_sequence; /**< HUMIDITY_SEQUENCE: 해당 값의 새 측정 번호 */
    uint16_t humidity_age_ms; /**< HUMIDITY_AGE: 해당 값 원본 측정 후 ms */
    NetValidity humidity_validity; /**< HUMIDITY_VALIDITY: 0=INVALID; 1=VALID */
    NetQualityReason humidity_reason; /**< HUMIDITY_REASON: CIS 품질 사유 */
    uint32_t illuminance_lux; /**< ILLUMINANCE: 1 lx/raw; 표현 범위 0~200000; 0xFFFFFFFF=무효 */
    uint32_t illuminance_sequence; /**< ILLUMINANCE_SEQUENCE: 해당 값의 새 측정 번호 */
    uint16_t illuminance_age_ms; /**< ILLUMINANCE_AGE: 해당 값 원본 측정 후 ms */
    NetValidity illuminance_validity; /**< ILLUMINANCE_VALIDITY: 0=INVALID; 1=VALID */
    NetQualityReason illuminance_reason; /**< ILLUMINANCE_REASON: CIS 품질 사유 */
} NetCisEnvironment;
```

**로컬 사용:** [`CisNet_PublishEnvironment()`](20_API.md#api-cisnet_publishenvironment) · 전체 타입/인자·호출자·수명·실패는 API에서 확인한다.

| 멤버 | 정확한 C 타입 | 값·단위·범위 / 역할 | 원 소유자·유효 조건 |
|---|---|---|---|
| `temperature_centi_c` | `int16_t` | 0.01°C/raw; -4000~12500; -32768=무효 | CIS 해당 센서 담당. 같은 센서 validity/reason과 함께 해석; 다른 정상 센서까지 무효화하지 않음. |
| `temperature_sequence` | `uint32_t` | 해당 값의 새 측정 번호 | CIS 해당 센서의 실제 새 측정 번호. 다른 센서 갱신·단순 재전송으로 증가 금지. |
| `temperature_age_ms` | `uint16_t` | 해당 값 원본 측정 후 ms | CIS 해당 센서 원 측정 후 ms. 센서별 독립; Network 대기만 추가·65535 포화. |
| `temperature_validity` | `NetValidity` | 0=INVALID; 1=VALID | CIS 해당 센서 담당. 같은 센서 validity/reason과 함께 해석; 다른 정상 센서까지 무효화하지 않음. |
| `temperature_reason` | `NetQualityReason` | CIS 품질 사유 | CIS 해당 센서 담당. 같은 센서 validity/reason과 함께 해석; 다른 정상 센서까지 무효화하지 않음. |
| `humidity_centi_percent` | `uint16_t` | 0.01%RH/raw; 0~10000; 65535=무효 | CIS 해당 센서 담당. 같은 센서 validity/reason과 함께 해석; 다른 정상 센서까지 무효화하지 않음. |
| `humidity_sequence` | `uint32_t` | 해당 값의 새 측정 번호 | CIS 해당 센서의 실제 새 측정 번호. 다른 센서 갱신·단순 재전송으로 증가 금지. |
| `humidity_age_ms` | `uint16_t` | 해당 값 원본 측정 후 ms | CIS 해당 센서 원 측정 후 ms. 센서별 독립; Network 대기만 추가·65535 포화. |
| `humidity_validity` | `NetValidity` | 0=INVALID; 1=VALID | CIS 해당 센서 담당. 같은 센서 validity/reason과 함께 해석; 다른 정상 센서까지 무효화하지 않음. |
| `humidity_reason` | `NetQualityReason` | CIS 품질 사유 | CIS 해당 센서 담당. 같은 센서 validity/reason과 함께 해석; 다른 정상 센서까지 무효화하지 않음. |
| `illuminance_lux` | `uint32_t` | 1 lx/raw; 표현 범위 0~200000; 0xFFFFFFFF=무효 | CIS 해당 센서 담당. 같은 센서 validity/reason과 함께 해석; 다른 정상 센서까지 무효화하지 않음. |
| `illuminance_sequence` | `uint32_t` | 해당 값의 새 측정 번호 | CIS 해당 센서의 실제 새 측정 번호. 다른 센서 갱신·단순 재전송으로 증가 금지. |
| `illuminance_age_ms` | `uint16_t` | 해당 값 원본 측정 후 ms | CIS 해당 센서 원 측정 후 ms. 센서별 독립; Network 대기만 추가·65535 포화. |
| `illuminance_validity` | `NetValidity` | 0=INVALID; 1=VALID | CIS 해당 센서 담당. 같은 센서 validity/reason과 함께 해석; 다른 정상 센서까지 무효화하지 않음. |
| `illuminance_reason` | `NetQualityReason` | CIS 품질 사유 | CIS 해당 센서 담당. 같은 센서 validity/reason과 함께 해석; 다른 정상 센서까지 무효화하지 않음. |
<a id="type-netcisoccupant"></a>
## `NetCisOccupant`

Pi 원 관측 + CIS 품질/기능 평가 → DOMAIN · [`CIS_OCCUPANT`](../../03_COMMUNICATION/10_CAN_FD.md#message-cis-occupant)

```c
typedef struct NetCisOccupant
{
    uint32_t pi_boot_id; /**< PI_BOOT_ID: Pi 원본 기동 번호 */
    uint32_t vision_sequence; /**< VISION_SEQUENCE: Pi 새 판정 번호, 중계 시 보존 */
    uint16_t vision_age_ms; /**< VISION_AGE: Pi 원본 판정 후 누적 ms */
    NetTriState occupant_presence; /**< OCCUPANT_PRESENCE: 0=없음; 1=있음; 255=확인 불가 */
    NetValidity presence_validity; /**< PRESENCE_VALIDITY: 0=INVALID; 1=VALID */
    NetQualityReason presence_reason; /**< PRESENCE_REASON: CIS 품질 사유 */
    uint8_t occupant_count; /**< OCCUPANT_COUNT: 0~5명,255=확인 불가 */
    NetValidity count_validity; /**< COUNT_VALIDITY: 0=INVALID; 1=VALID */
    NetQualityReason count_reason; /**< COUNT_REASON: CIS 품질 사유 */
    NetSensorFunctionState vision_function_status; /**< VISION_FUNCTION_STATUS: CIS 기능 상태 */
} NetCisOccupant;
```

**로컬 사용:** [`CisNet_PublishOccupant()`](20_API.md#api-cisnet_publishoccupant) · 전체 타입/인자·호출자·수명·실패는 API에서 확인한다.

| 멤버 | 정확한 C 타입 | 값·단위·범위 / 역할 | 원 소유자·유효 조건 |
|---|---|---|---|
| `pi_boot_id` | `uint32_t` | Pi 원본 기동 번호 | Pi runtime/비전 원본. CIS는 수신한 값을 보존; CIS CAN Boot/TX_SEQUENCE로 대체하지 않음. |
| `vision_sequence` | `uint32_t` | Pi 새 판정 번호, 중계 시 보존 | Pi runtime/비전 원본. CIS는 수신한 값을 보존; CIS CAN Boot/TX_SEQUENCE로 대체하지 않음. |
| `vision_age_ms` | `uint16_t` | Pi 원본 판정 후 누적 ms | Pi 원 판정 Age + CIS 수신 후 보관/송신 대기. 각 구간을 한 번만 추가; 새 수신·상태평가로 0 초기화 금지. |
| `occupant_presence` | `NetTriState` | 0=없음; 1=있음; 255=확인 불가 | Pi 원 값. CIS 추가 검사 실패 때 정상 없음/0명을 합성하지 않음; 해당 validity를 함께 소비. |
| `presence_validity` | `NetValidity` | 0=INVALID; 1=VALID | CIS 최종 전달 평가 후보. Pi 원 품질을 cache에 보존하고 추가 검사로만 제한; 이 CAN 한 벌로 양측 품질을 동시에 표현할 수 없음(OPEN). |
| `presence_reason` | `NetQualityReason` | CIS 품질 사유 | CIS 최종 전달 평가 후보. Pi 원 품질을 cache에 보존하고 추가 검사로만 제한; 이 CAN 한 벌로 양측 품질을 동시에 표현할 수 없음(OPEN). |
| `occupant_count` | `uint8_t` | 0~5명,255=확인 불가 | Pi 원 값. CIS 추가 검사 실패 때 정상 없음/0명을 합성하지 않음; 해당 validity를 함께 소비. |
| `count_validity` | `NetValidity` | 0=INVALID; 1=VALID | CIS 최종 전달 평가 후보. Pi 원 품질을 cache에 보존하고 추가 검사로만 제한; 이 CAN 한 벌로 양측 품질을 동시에 표현할 수 없음(OPEN). |
| `count_reason` | `NetQualityReason` | CIS 품질 사유 | CIS 최종 전달 평가 후보. Pi 원 품질을 cache에 보존하고 추가 검사로만 제한; 이 CAN 한 벌로 양측 품질을 동시에 표현할 수 없음(OPEN). |
| `vision_function_status` | `NetSensorFunctionState` | CIS 기능 상태 | CIS 기능 평가 owner. 이 occupant update와 함께 고정한 CIS 비전 기능 상태; Pi VISION_STATUS 단순 raw cast 금지. |
<a id="type-netcisrear"></a>
## `NetCisRear`

CIS 센싱·진단 → DOMAIN · [`CIS_REAR`](../../03_COMMUNICATION/10_CAN_FD.md#message-cis-rear)

```c
typedef struct NetCisRear
{
    uint32_t update_sequence; /**< UPDATE_SEQUENCE: 새 거리 관측 번호 */
    uint16_t source_age_ms; /**< SOURCE_AGE: 거리 원본 관측 후 ms */
    uint16_t rear_distance_deci_cm; /**< REAR_DISTANCE: 0.1cm/raw; 65535=거리 없음/무효, 상태로 구분 */
    NetValidity validity; /**< VALIDITY: 0=INVALID; 1=VALID */
    NetQualityReason quality_reason; /**< QUALITY_REASON: CIS 품질 사유 */
    NetProximityStatus proximity_status; /**< PROXIMITY_STATUS: 0=VALID_DISTANCE; 1=NO_OBJECT; 2=UNAVAILABLE; 3=FAULT; 4=RECOVERING; 5=INACTIVE */
} NetCisRear;
```

**로컬 사용:** [`CisNet_PublishRear()`](20_API.md#api-cisnet_publishrear) · 전체 타입/인자·호출자·수명·실패는 API에서 확인한다.

| 멤버 | 정확한 C 타입 | 값·단위·범위 / 역할 | 원 소유자·유효 조건 |
|---|---|---|---|
| `update_sequence` | `uint32_t` | 새 거리 관측 번호 | CIS 새 초음파 관측. 위험 수준 CAUTION/EMERGENCY/CLEAR 생산은 DOMAIN 책임. |
| `source_age_ms` | `uint16_t` | 거리 원본 관측 후 ms | CIS 원 거리 관측 Age; Network 대기만 추가·65535 포화. |
| `rear_distance_deci_cm` | `uint16_t` | 0.1cm/raw; 65535=거리 없음/무효, 상태로 구분 | CIS 측정. 65535는 정상 거리 아님; NO_OBJECT와 UNAVAILABLE은 proximity_status로 구분. 최종 센서 범위 OPEN. |
| `validity` | `NetValidity` | 0=INVALID; 1=VALID | CIS 초음파 담당. VALID_DISTANCE일 때 유효 거리, NO_OBJECT는 유효한 무검출 상태, FAULT/UNAVAILABLE을 CLEAR로 변환 금지. |
| `quality_reason` | `NetQualityReason` | CIS 품질 사유 | CIS 초음파 담당. VALID_DISTANCE일 때 유효 거리, NO_OBJECT는 유효한 무검출 상태, FAULT/UNAVAILABLE을 CLEAR로 변환 금지. |
| `proximity_status` | `NetProximityStatus` | 0=VALID_DISTANCE; 1=NO_OBJECT; 2=UNAVAILABLE; 3=FAULT; 4=RECOVERING; 5=INACTIVE | CIS 초음파 담당. VALID_DISTANCE일 때 유효 거리, NO_OBJECT는 유효한 무검출 상태, FAULT/UNAVAILABLE을 CLEAR로 변환 금지. |
<a id="type-netcisstatus"></a>
## `NetCisStatus`

CIS 센싱·진단 → DOMAIN · [`CIS_STATUS`](../../03_COMMUNICATION/10_CAN_FD.md#message-cis-status)

```c
typedef struct NetCisStatus
{
    NetCisState cis_state; /**< CIS_STATE: 0=STARTUP; 1=READY; 2=ACTIVE; 3=FAULT */
    NetSensorFunctionState vision_status; /**< VISION_STATUS: 0=NOT_READY; 1=READY; 2=ACTIVE; 3=FAULT; 4=RECOVERING */
    NetSensorFunctionState temperature_status; /**< TEMPERATURE_STATUS: 0=NOT_READY; 1=READY; 2=ACTIVE; 3=FAULT; 4=RECOVERING */
    NetSensorFunctionState humidity_status; /**< HUMIDITY_STATUS: 0=NOT_READY; 1=READY; 2=ACTIVE; 3=FAULT; 4=RECOVERING */
    NetSensorFunctionState illuminance_status; /**< ILLUMINANCE_STATUS: 0=NOT_READY; 1=READY; 2=ACTIVE; 3=FAULT; 4=RECOVERING */
    NetSensorFunctionState rear_status; /**< REAR_STATUS: 0=NOT_READY; 1=READY; 2=ACTIVE; 3=FAULT; 4=RECOVERING */
    NetInterfaceState pi_uart_status; /**< PI_UART_STATUS: 0=READY; 1=UNAVAILABLE; 2=RECOVERING */
    NetInterfaceState can_interface_status; /**< CAN_INTERFACE_STATUS: 0=READY; 1=UNAVAILABLE; 2=RECOVERING */
    uint16_t fault_mask; /**< FAULT_MASK: CIS 고장 비트 0~8 */
    uint16_t active_fault; /**< ACTIVE_FAULT: 현재 주요 고장 코드 */
    NetFlag recovering; /**< RECOVERING: 0=아님; 1=복구 중 */
    uint16_t last_fault; /**< LAST_FAULT: 이번 기동 이후 최근 주요 고장 코드 */
    uint32_t last_fault_occurrence; /**< LAST_FAULT_OCCURRENCE: 최근 고장 발생 번호 */
    uint8_t affected_function; /**< AFFECTED_FUNCTION: bit0=비전; bit1=온도; bit2=습도; bit3=조도; bit4=후방; bit5=제공 경로 */
    uint32_t update_sequence; /**< UPDATE_SEQUENCE: 이번 상태 평가 번호 */
    uint16_t source_age_ms; /**< SOURCE_AGE: 평가 후 ms */
} NetCisStatus;
```

**로컬 사용:** [`CisNet_PublishStatus()`](20_API.md#api-cisnet_publishstatus) · 전체 타입/인자·호출자·수명·실패는 API에서 확인한다.

| 멤버 | 정확한 C 타입 | 값·단위·범위 / 역할 | 원 소유자·유효 조건 |
|---|---|---|---|
| `cis_state` | `NetCisState` | 0=STARTUP; 1=READY; 2=ACTIVE; 3=FAULT | CIS 기능 담당. 부분 고장과 정상 기능을 구분하며 복구 상태만으로 이전 측정을 VALID로 바꾸지 않음. |
| `vision_status` | `NetSensorFunctionState` | 0=NOT_READY; 1=READY; 2=ACTIVE; 3=FAULT; 4=RECOVERING | CIS 기능 담당. 부분 고장과 정상 기능을 구분하며 복구 상태만으로 이전 측정을 VALID로 바꾸지 않음. |
| `temperature_status` | `NetSensorFunctionState` | 0=NOT_READY; 1=READY; 2=ACTIVE; 3=FAULT; 4=RECOVERING | CIS 기능 담당. 부분 고장과 정상 기능을 구분하며 복구 상태만으로 이전 측정을 VALID로 바꾸지 않음. |
| `humidity_status` | `NetSensorFunctionState` | 0=NOT_READY; 1=READY; 2=ACTIVE; 3=FAULT; 4=RECOVERING | CIS 기능 담당. 부분 고장과 정상 기능을 구분하며 복구 상태만으로 이전 측정을 VALID로 바꾸지 않음. |
| `illuminance_status` | `NetSensorFunctionState` | 0=NOT_READY; 1=READY; 2=ACTIVE; 3=FAULT; 4=RECOVERING | CIS 기능 담당. 부분 고장과 정상 기능을 구분하며 복구 상태만으로 이전 측정을 VALID로 바꾸지 않음. |
| `rear_status` | `NetSensorFunctionState` | 0=NOT_READY; 1=READY; 2=ACTIVE; 3=FAULT; 4=RECOVERING | CIS 기능 담당. 부분 고장과 정상 기능을 구분하며 복구 상태만으로 이전 측정을 VALID로 바꾸지 않음. |
| `pi_uart_status` | `NetInterfaceState` | 0=READY; 1=UNAVAILABLE; 2=RECOVERING | 원천은 CIS Network의 해당 링크 관측; CIS 진단 owner가 상태에 반영. concrete health getter/sink BINDING_OPEN. |
| `can_interface_status` | `NetInterfaceState` | 0=READY; 1=UNAVAILABLE; 2=RECOVERING | 원천은 CIS Network의 해당 링크 관측; CIS 진단 owner가 상태에 반영. concrete health getter/sink BINDING_OPEN. |
| `fault_mask` | `uint16_t` | CIS 고장 비트 0~8 | CIS 진단 owner. 현재 고장과 영향 기능/제공 경로 구분; 별도 CIS_FAULT frame 추가 없음. |
| `active_fault` | `uint16_t` | 현재 주요 고장 코드 | CIS 진단 owner. 현재 고장과 영향 기능/제공 경로 구분; 별도 CIS_FAULT frame 추가 없음. |
| `recovering` | `NetFlag` | 0=아님; 1=복구 중 | CIS 기능 담당. 부분 고장과 정상 기능을 구분하며 복구 상태만으로 이전 측정을 VALID로 바꾸지 않음. |
| `last_fault` | `uint16_t` | 이번 기동 이후 최근 주요 고장 코드 | CIS 진단의 이번 기동 이후 최근 이력. active 해제·현재 상태 TTL로 이력을 삭제하지 않음. |
| `last_fault_occurrence` | `uint32_t` | 최근 고장 발생 번호 | CIS 진단의 이번 기동 이후 최근 이력. active 해제·현재 상태 TTL로 이력을 삭제하지 않음. |
| `affected_function` | `uint8_t` | bit0=비전; bit1=온도; bit2=습도; bit3=조도; bit4=후방; bit5=제공 경로 | CIS 진단 owner. 현재 고장과 영향 기능/제공 경로 구분; 별도 CIS_FAULT frame 추가 없음. |
| `update_sequence` | `uint32_t` | 이번 상태 평가 번호 | CIS 새 상태 평가에서 증가; VISION_SEQUENCE/센서 원 측정 순번을 갱신하지 않음. |
| `source_age_ms` | `uint16_t` | 평가 후 ms | CIS 원 상태 평가 후 ms; Network 대기만 한 번 누적·65535 포화. |

<a id="type-netcisstate"></a>
## `NetCisState`

```c
typedef uint8_t NetCisState;
enum NetCisStateCode
{
    NET_CIS_STATE_STARTUP = 0,
    NET_CIS_STATE_READY = 1,
    NET_CIS_STATE_ACTIVE = 2,
    NET_CIS_STATE_FAULT = 3
};
```

<a id="fixed-width-codes"></a>
별칭은 위의 고정 폭 C 정수다. 이어지는 `enum ...Code`는 상수 집합으로, enum 메모리 크기가 별칭/Wire 폭이라는 뜻이 아니다. 아래 숫자는 **이 설계 후보의 의미 코드**이며 기존 C2 타입은 [명시 mapping](../../04_COMMON/30_ENUM_WIRE_MAPPING.md)을 거친다. 예약/지원하지 않는 코드는 형식 오류로 처리한다.

| 상수 | 제안 값 / 의미 |
|---|---|
| `NET_CIS_STATE_STARTUP` | `0` · 시작 중 |
| `NET_CIS_STATE_READY` | `1` · 준비됨 |
| `NET_CIS_STATE_ACTIVE` | `2` · 동작 중 |
| `NET_CIS_STATE_FAULT` | `3` · 오류 |

<a id="type-netinterfacestate"></a>
## `NetInterfaceState`

```c
typedef uint8_t NetInterfaceState;
enum NetInterfaceStateCode
{
    NET_INTERFACE_READY = 0,
    NET_INTERFACE_UNAVAILABLE = 1,
    NET_INTERFACE_RECOVERING = 2
};
```

코드 폭·예약값·C2 변환은 [고정 폭 코드 규칙](#fixed-width-codes)을 적용한다.

| 상수 | 제안 값 / 의미 |
|---|---|
| `NET_INTERFACE_READY` | `0` · 제공 경로 준비됨 |
| `NET_INTERFACE_UNAVAILABLE` | `1` · 제공 불가 |
| `NET_INTERFACE_RECOVERING` | `2` · 경로 복구 중 |

<a id="type-netproximitystatus"></a>
## `NetProximityStatus`

```c
typedef uint8_t NetProximityStatus;
enum NetProximityStatusCode
{
    NET_PROXIMITY_VALID_DISTANCE = 0,
    NET_PROXIMITY_NO_OBJECT = 1,
    NET_PROXIMITY_UNAVAILABLE = 2,
    NET_PROXIMITY_FAULT = 3,
    NET_PROXIMITY_RECOVERING = 4,
    NET_PROXIMITY_INACTIVE = 5
};
```

코드 폭·예약값·C2 변환은 [고정 폭 코드 규칙](#fixed-width-codes)을 적용한다.

| 상수 | 제안 값 / 의미 |
|---|---|
| `NET_PROXIMITY_VALID_DISTANCE` | `0` · 유효한 실제 거리 |
| `NET_PROXIMITY_NO_OBJECT` | `1` · 유효한 무검출; 오류와 별개 |
| `NET_PROXIMITY_UNAVAILABLE` | `2` · 제공 불가 |
| `NET_PROXIMITY_FAULT` | `3` · 측정 기능 오류 |
| `NET_PROXIMITY_RECOVERING` | `4` · 복구 확인 중 |
| `NET_PROXIMITY_INACTIVE` | `5` · 의도적으로 센싱하지 않음 |

<a id="type-netcisfaultmask"></a>
## `NetCisFaultMask`

```c
enum NetCisFaultMask
{
    NET_CIS_FAULT_INITIALIZATION_FAILURE = (1u << 0),
    NET_CIS_FAULT_VISION_FAULT = (1u << 1),
    NET_CIS_FAULT_TEMPERATURE_SENSOR_FAULT = (1u << 2),
    NET_CIS_FAULT_HUMIDITY_SENSOR_FAULT = (1u << 3),
    NET_CIS_FAULT_ILLUMINANCE_SENSOR_FAULT = (1u << 4),
    NET_CIS_FAULT_PROXIMITY_SENSOR_FAULT = (1u << 5),
    NET_CIS_FAULT_COMMUNICATION_FAULT = (1u << 6),
    NET_CIS_FAULT_DATA_INVALID = (1u << 7),
    NET_CIS_FAULT_OUT_OF_RANGE = (1u << 8)
};
```

현재 고장 비트는 동시 active를, 주요 고장 코드는 단일 대표를 표현한다. 주요 고장 선택 우선순위는 기능 owner의 OPEN이며 Network가 임의 우선순위를 만들지 않는다.

| 상수 | 제안 값 / 의미 |
|---|---|
| `NET_CIS_FAULT_INITIALIZATION_FAILURE` | `(1u << 0)` · 초기화 실패 |
| `NET_CIS_FAULT_VISION_FAULT` | `(1u << 1)` · 비전 기능 오류 |
| `NET_CIS_FAULT_TEMPERATURE_SENSOR_FAULT` | `(1u << 2)` · 온도 센서 오류 |
| `NET_CIS_FAULT_HUMIDITY_SENSOR_FAULT` | `(1u << 3)` · 습도 센서 오류 |
| `NET_CIS_FAULT_ILLUMINANCE_SENSOR_FAULT` | `(1u << 4)` · 조도 센서 오류 |
| `NET_CIS_FAULT_PROXIMITY_SENSOR_FAULT` | `(1u << 5)` · 후방 센서 오류 |
| `NET_CIS_FAULT_COMMUNICATION_FAULT` | `(1u << 6)` · 제공 경로 통신 오류 |
| `NET_CIS_FAULT_DATA_INVALID` | `(1u << 7)` · 사용 불가 데이터 |
| `NET_CIS_FAULT_OUT_OF_RANGE` | `(1u << 8)` · 유효 범위 밖 |

<a id="type-netcisfaultcode"></a>
## `NetCisFaultCode`

```c
enum NetCisFaultCode
{
    NET_CIS_FAULT_CODE_NONE = 0,
    NET_CIS_FAULT_CODE_INITIALIZATION_FAILURE = 1,
    NET_CIS_FAULT_CODE_VISION_FAULT = 2,
    NET_CIS_FAULT_CODE_TEMPERATURE_SENSOR_FAULT = 3,
    NET_CIS_FAULT_CODE_HUMIDITY_SENSOR_FAULT = 4,
    NET_CIS_FAULT_CODE_ILLUMINANCE_SENSOR_FAULT = 5,
    NET_CIS_FAULT_CODE_PROXIMITY_SENSOR_FAULT = 6,
    NET_CIS_FAULT_CODE_COMMUNICATION_FAULT = 7,
    NET_CIS_FAULT_CODE_DATA_INVALID = 8,
    NET_CIS_FAULT_CODE_OUT_OF_RANGE = 9
};
```

현재 고장 비트는 동시 active를, 주요 고장 코드는 단일 대표를 표현한다. 주요 고장 선택 우선순위는 기능 owner의 OPEN이며 Network가 임의 우선순위를 만들지 않는다.

| 상수 | 제안 값 / 의미 |
|---|---|
| `NET_CIS_FAULT_CODE_NONE` | `0` · 없음 |
| `NET_CIS_FAULT_CODE_INITIALIZATION_FAILURE` | `1` · 초기화 실패 |
| `NET_CIS_FAULT_CODE_VISION_FAULT` | `2` · 비전 기능 오류 |
| `NET_CIS_FAULT_CODE_TEMPERATURE_SENSOR_FAULT` | `3` · 온도 센서 오류 |
| `NET_CIS_FAULT_CODE_HUMIDITY_SENSOR_FAULT` | `4` · 습도 센서 오류 |
| `NET_CIS_FAULT_CODE_ILLUMINANCE_SENSOR_FAULT` | `5` · 조도 센서 오류 |
| `NET_CIS_FAULT_CODE_PROXIMITY_SENSOR_FAULT` | `6` · 후방 센서 오류 |
| `NET_CIS_FAULT_CODE_COMMUNICATION_FAULT` | `7` · 제공 경로 통신 오류 |
| `NET_CIS_FAULT_CODE_DATA_INVALID` | `8` · 사용 불가 데이터 |
| `NET_CIS_FAULT_CODE_OUT_OF_RANGE` | `9` · 유효 범위 밖 |

<a id="pi-원-관측과-cis-추가-평가"></a>
## Pi 원 관측과 CIS 추가 평가

Pi의 `pi_boot_id/vision_sequence/vision_age_ms`와 Presence/Count·각 원 validity/reason은 [Pi Data](../PI/10_DATA.md)에서 생산된다. UART-C 수신 후 CIS owner는 **원 record를 보존**한다. CIS_OCCUPANT에는 같은 Pi 원 identity/값과 CIS 관리의 `vision_function_status`를 연결한다. 독립 최신 CIS_STATUS를 과거 occupant record의 동반 상태에 덮어쓰지 않는다.

**PROPOSED — 원 품질을 우선 보존하는 제한 평가(O13):** Network는 UART-C TYPE/길이/코드와 수용한 Pi 세대를 검사한 뒤 `CisApp_OnPiVisionResult()`에 원 record와 수신 메타를 전달한다. CIS owner는 반환 전에 원 record·수신 기준점을 복사하고, Presence와 Count를 각각 평가하여 기존 `CisNet_PublishOccupant()`로 제출하는 순서를 권장한다. 두 값의 동일 Pi Boot/VisionSequence/원 AGE를 유지한다. 원 INVALID인 값은 그 값·INVALID·Pi 원 사유를 보존한다. 원 VALID인 값만 CIS의 실제 사용 조건을 추가 검사하며, 확인된 만료이면 해당 validity를 INVALID/사유 STALE로, 확인된 전달 장애이면 INVALID/사유 COMMUNICATION_FAULT로 제한한다. 나머지 원 품질은 승격하지 않는다. CIS READY나 새 CAN frame만으로 이 평가를 통과시키지 않는다.

Pi 원 사유와 CIS 추가 사유가 동시에 있으면 **Pi INVALID의 원 사유를 전송하고 CIS 추가 근거는 로컬 원 record와 함께 보존**하는 최소안을 권장한다. Pi가 VALID인 경우에만 CIS에서 실제 확인한 차단 원인을 기존 사유 코드로 전달한다. 한 벌의 Wire validity/reason이 두 근거를 모두 표현한다고 주장하지 않는다. 여러 CIS 차단 원인의 대표 선택과 `vision_function_status` 변환은 CIS 기능 owner가 확인해야 하며, 표현 근거 없는 조합은 해당 값만 제어 입력으로 사용하지 않는다. Count 실패로 정상 Presence까지 INVALID로 만들거나 반대로 Presence 실패를 Count 0으로 치환하지 않는다. 이 안은 `NETWORK_OWNED` 수신 검증과 `CROSS_NODE_AGREEMENT` 품질 전달 권고이며, Pi/CIS/DOMAIN 담당자가 “이 한 벌을 최종 사용 품질로 소비하며 Pi INVALID 원 사유 우선을 수용하는가?”를 확인해야 한다. 채택·실제 변환/Codec·시험 근거는 아직 없으며 `AGREED/IMPLEMENTED/TESTED`로 표시하지 않는다.

`P_VISION_STATUS` READY/RECOVERING은 기능 상태 변경이다. 이 수신으로 `VISION_SEQUENCE`나 원 판정 생성시각을 갱신하지 않고 이전 Presence/Count를 새 관측으로 표시하지 않는다. 후방 위험 판단은 DOMAIN이 수행하므로 CIS_REAR가 VSS 위험 수준을 생성하지 않는다. NO_OBJECT는 측정 불가와 구분한다.

## Source AGE를 한 번씩 누적

Pi Network는 원 판정 Age에 **Pi 제출→실제 UART 송신 대기**만 더한다. CIS RX는 원 Age에 아직 반영하지 않은 UART 전달 여유만 한 번 반영한다. CIS는 이 수신 record의 age를 cache에 두고 `received_at_ms`를 기준점으로 보관한다. Common의 UART50ms/CAN5ms 전달 여유는 PROPOSED이며 이미 반영한 여유를 다시 더하지 않는다. CAN 제출 시 CIS 수신→제출 경과를 한 번 더한 뒤, CIS Network는 제출→실제 CAN 송신 대기만 더한다. callback 지연을 이미 반영했다면 같은 수신 기준점으로 다시 더하지 않는다. App·Network의 실제 기준점 연결은 BINDING_OPEN이다.

링크 전송 시간을 공통 clock 없이 정확히 복원할 수 없으므로 필요한 여유/최신성 상한은 팀 Wire·측정 조건으로 결정한다. 65535 포화는 충분히 오래된 값이며 정상 Age0으로 되돌리지 않는다. DOMAIN은 이후 수신→사용/중계 경과만 한 번 더한다.

<a id="허용-입력과-pi-relay"></a>
## 허용 입력과 Pi relay

허용의 세 입력 합성·독립 품질·보수 AGE 정본은 [DOMAIN_PERMISSION](../../03_COMMUNICATION/10_CAN_FD.md#message-domain-permission), Pi 실제 사용 조건은 [Pi 허용 소비](../PI/10_DATA.md#허용-소비와-원본-age)다. 아래는 CIS의 App 소비·내부 중계 역할과 수명만 설명한다.

DOMAIN_PERMISSION의 전체 타입은 [Common Data](../../04_COMMON/10_DATA_AND_ID_RULES.md#type-netdomainpermission)다. CIS App은 원 전원 허용을 소비하며, `P_PERMISSION` 구성은 내부 `CisNetPriv_RelayPiPermission()`이 맡는다. 공개 App이 임의 `NetPiPermission`을 작성해 새 허용을 보내는 경계는 채택하지 않는다.

P_PERMISSION의 `cis_boot_id`는 CIS runtime, `update_sequence`는 CIS relay 의미 평가 번호 후보다. DOMAIN 원 Boot/순번을 이 두 값과 동일시하지 않는다. 원 DOMAIN 세대 변경 시 기존 relay 허용을 무효화하고 현재 원 허용 재확인 뒤 새 relay 의미 평가를 만든다. 반복 송신은 sequence 유지·Age 증가다. 최종 use_limit 1500ms 후보, relay 품질/사유와 무수신 consumer 무효화 연결은 TEAM_DECISION_REQUIRED/BINDING_OPEN이다.

**PROPOSED — 동일 입력의 두 소비와 제한 relay(O05):** 검증된 동일 `NetDomainPermission`과 `NetCanRxMeta`를 기존 `CisApp_OnDomainPermission()` 및 private `CisNetPriv_RelayPiPermission()`에 분배한다. CIS/Pi의 전원 소비는 `vehicle_power_permission`, `power_permission_quality`, 수용한 DOMAIN 세대와 공통 보수 AGE를 함께 확인한다. 같은 frame의 WINDOW operation 정보로 전원 허용을 새로 만들거나, operation 근거 부재만으로 독립적으로 확인된 전원 판단을 새 불허 정책으로 바꾸지 않는다. 다만 공통 AGE로 전원 원 기한 이내임을 입증할 수 없으면 그 전원 사용은 제한한다. power 품질이 OK이고 값이 1이며 원 기한 이내임을 확인한 경우에만 relay 허용 후보를 전달한다. 원 값 0/OK는 확인된 불허 그대로, 원 255 또는 NO_DATA/INVALID/STALE는 해당 비유효 의미 그대로 전달하고 Pi가 허용으로 사용하지 않게 한다.

새 원 DOMAIN 평가/세대 또는 relay 품질 제한이 생길 때 기존 relay record를 갱신·로컬 제출하고, 단순 반복은 기존 relay revision과 증가한 원 AGE를 유지하는 안을 권장한다. 새 DOMAIN TX_SEQUENCE만으로 relay 원 평가를 발급하지 않는다. CAN 수신 후 경과·UART 대기를 각각 한 번 누적하고, Pi가 원 DOMAIN 150ms 사용 상한 후보와 `use_limit_ms` 중 더 짧은 한도를 확인하도록 양단 합의하는 안을 권장한다. 따라서 500ms 반복/1500ms `USE_LIMIT` 후보는 원 허용을 연장하는 근거가 아니다. 제출 실패는 기존 `NOT_READY/NO_CAPACITY` 등 로컬 실패로 관측하며 이전 허용을 새 OK로 재제출하지 않는다. 실패·무수신·만료 때 실제 Pi 소비자가 허용 상실을 관측하는 연결은 `IMPLEMENTATION_BINDING` 미정이다. 이 `CROSS_NODE_AGREEMENT` 권고에 대해 DOMAIN/CIS/Pi 담당자는 원 전원 판단의 사용 상한, 새 원 평가·실효성 상실의 전달 조건, 실제 소비자 무효화 연결을 확인해야 한다. 연속 유효 허용이 필요하면 500ms 반복만으로 충분하지 않다는 점과 실제 전달 지연도 함께 확인한다. 새 주기·Wire·공개 API는 확정하지 않는다.

## 센서별 부분 실패와 링크 관측 — O19

**PROPOSED — 독립 원 관측을 그대로 보고:** CIS owner는 `NetCisEnvironment`의 각 값·sequence·AGE·validity·reason 묶음을 독립 갱신해 기존 `CisNet_PublishEnvironment()`로 제출한다. 예를 들어 온도 NO_DATA는 기존 온도 무효 표현과 INVALID/NO_DATA 사유로 보고하되, 유효한 습도·조도의 값과 원 측정 번호·AGE를 보존한다. 환경 frame 수신이나 CIS READY는 세 값의 원 측정을 갱신하지 않는다. 후방 UNAVAILABLE/FAULT 및 거리 65535를 정상 거리 0 또는 NO_OBJECT로 바꾸지 않으며, DOMAIN의 위험/CLEAR 판단을 CIS가 대신 생성하지 않는다. 기존 타입·raw를 사용하는 `NETWORK_OWNED` 전달 권고이며 센서 사용 범위·기능 판단은 `FUNCTION_OWNER` 확인 사항이다.

Network가 관측한 Pi UART/CAN 제공 경로 상태는 CIS 진단 owner가 실제 근거와 영향 기능을 확인한 뒤 기존 `NetCisStatus`의 interface 상태·현재 fault·affected_function에 반영하고 `CisNet_PublishStatus()`로 제출하는 안을 권장한다. 단순 수신 누락을 센서 자체 고장이나 최근 fault의 새 발생으로 직접 변환하지 않는다. 다른 정상 센서의 값/품질은 보존하고, 링크 실패가 영향을 주는 제공·소비 경로만 사용을 제한한다. CIS 담당자에게 “현재 Network 관측을 진단 owner에 전달하는 실제 기존 연결은 무엇이며, 어떤 기능/제공 경로의 사용이 제한되는가?”를 확인한다. 미확인 health 전달 함수나 신규 fault frame을 만들지 않는다. SDK/Firmware/Codec/Board 시험은 `NOT_EXECUTED`다.

## 환경 원본 최신성 — O20

**CONFLICT / TEAM_DECISION_REQUIRED:** 온도·습도 원본 사용 AGE 한도는 pinned Network §8과 제공 Stage B의 **2500ms [초안 제안]**이 구 SysRS §2.5의 **2000ms [잠정]**과 다르다. CIS/DOMAIN/Network 담당자가 결정하기 전 어느 수치를 최종 승인값으로 적용하지 않는다. 조도 원본 한도 **1000ms 후보**, CIS_ENVIRONMENT CAN 보고 주기 **200ms 후보** 및 정상 frame 수신 중단 **600ms 후보**는 각각 원 측정 최신성·전송 주기·링크 감시이며 온도·습도 원본 한도와 대체할 수 없다. 구 SysRS §2.5의 2000ms 행은 **실내 조도에도 적용**된다. 따라서 실내 조도 1000ms 후보↔구 2000ms의 관계도 담당자가 확인한다. 더 짧은 1000ms 후보는 사용을 보수적으로 제한하는 안으로 양립할 수 있으므로 수치 차이만으로 일괄 계약 충돌을 판정하지 않는다. 실제 센서 갱신 조건·수신 경과와 값을 사용하는 기능의 기준을 함께 대조한다. 새 frame 수신이나 다른 센서 갱신으로 오래된 원 측정 AGE를 초기화하지 않는다. [팀 결정 O20](../../06_OPEN_FOR_TEAM.md)과 [CAN 최신성 설정](../../03_COMMUNICATION/10_CAN_FD.md)을 함께 확인한다.

## 구현 전 결정

환경 스케일/표현 범위, 후방 센서 최종 유효 범위·Age 상한, Pi process/node 기동 문맥 admission, CIS 최종 품질/상태·fault mapping, 실제 UART/CAN health 전달 경계는 OPEN이다. 별도 CIS_FAULT 메시지나 Pi↔DOMAIN 직결, 원격 Query API를 추가하지 않는다. 실제 SDK/Firmware/Codec/Board 시험 NOT_EXECUTED.
