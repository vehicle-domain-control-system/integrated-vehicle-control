# CIS Data — 값별 원 관측과 Pi 취합·상태

Interface v2 · r1 · [규격과 연동 조건](../../07_CROSS_NODE_INTERFACE_RULES.md#status)

타입 9종의 선언과 멤버·단위·예약값을 정의한다. 확정된 시간 규칙, 권장 프로필과 실제 연동 검증 전 조건을 구분한다. [공통 시간·식별](../../04_COMMON/10_DATA_AND_ID_RULES.md#section-timing)을 Data·API·Wire에 동일하게 적용한다.

[전체 시작](../../00_START_HERE.md) · [개요](00_OVERVIEW.md) · [Data](10_DATA.md) · [API](20_API.md)

**PROPOSED_DESIGN · BINDING_OPEN.** 아래 전체 선언은 의미 타입을 표시하는 설계용 C 표현이다. 실제로 설치된 헤더나 펌웨어 코드가 아니며 struct padding·enum 크기를 raw bytes로 전송하지 않는다. [공통 ID·수명](../../04_COMMON/10_DATA_AND_ID_RULES.md)과 [C enum↔Wire 명시 변환](../../04_COMMON/30_ENUM_WIRE_MAPPING.md)을 함께 적용한다.

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
| `presence_validity` | `NetValidity` | 0=INVALID; 1=VALID | CIS 최종 전달 평가. 원 Pi 품질을 보존하고 이후 실제 장애로만 사용을 제한한다. 한 벌의 CAN 품질로 두 근거 전체를 동시에 표현하지 않는다. 아래 품질 권장안 참조. |
| `presence_reason` | `NetQualityReason` | CIS 품질 사유 | CIS 최종 전달 평가. 원 Pi 품질을 보존하고 이후 실제 장애로만 사용을 제한한다. 한 벌의 CAN 품질로 두 근거 전체를 동시에 표현하지 않는다. 아래 품질 권장안 참조. |
| `occupant_count` | `uint8_t` | 0~5명,255=확인 불가 | Pi 원 값. CIS 추가 검사 실패 때 정상 없음/0명을 합성하지 않음; 해당 validity를 함께 소비. |
| `count_validity` | `NetValidity` | 0=INVALID; 1=VALID | CIS 최종 전달 평가. 원 Pi 품질을 보존하고 이후 실제 장애로만 사용을 제한한다. 한 벌의 CAN 품질로 두 근거 전체를 동시에 표현하지 않는다. 아래 품질 권장안 참조. |
| `count_reason` | `NetQualityReason` | CIS 품질 사유 | CIS 최종 전달 평가. 원 Pi 품질을 보존하고 이후 실제 장애로만 사용을 제한한다. 한 벌의 CAN 품질로 두 근거 전체를 동시에 표현하지 않는다. 아래 품질 권장안 참조. |
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

통신 처리 규칙 — 원 관측의 동일성은 현재 품질이 영구히 같다는 뜻이 아니다. CIS는 같은 Pi BOOT/VISION_SEQUENCE의 확인된 품질 저하를 `CisNet_PublishOccupant()`로 제출한다. 새 CAN 프레임을 받은 DOMAIN 쪽 Network는 메시지별 TX_SEQUENCE를 확인하고 원 식별·누적 AGE를 유지한 채 로컬 `DomainIf_RxCisOccupant()`에 전달한다. 관측 순번 중복만으로 무효 품질을 버리지 않는다. 지연된 구 VALID, 단순 READY 또는 링크 복구만으로 이전 정상값을 복원하지 않는다. 정상 복원에는 새 유효 관측과 해당 기능의 복구 규칙을 적용한다. 이 규칙은 관측 번호·품질 필드·새 공개 API를 추가하지 않는다. DOMAIN 저장값과 Getter까지 반영되는지는 실제 연결 시험 대상이다.

권장 프로필 — 원 판정과 현재 사용 품질을 분리한다. 기존 CAN에는 최종 validity/reason 한 벌만 유지하며 새로운 품질 필드를 추가하지 않는다. 이 권장안은 실제 Codec·기능 연결 시험 완료를 뜻하지 않는다.

Pi의 `pi_boot_id/vision_sequence/vision_age_ms`와 Presence/Count의 원 validity/reason은 Pi가 생산한다. Network는 형식·수용한 세대·순서를 검증한 뒤 `CisApp_OnPiVisionResult()`로 인계한다. CIS 기능은 콜백이 끝나기 전에 필요한 원 값·품질·수신 기준점을 복사하고, 같은 원 판정에 대한 제한 결과를 `CisNet_PublishOccupant()`로 제출한다.

| 확인한 사실 | 현재 전달 품질 | 보존할 원 근거 |
|---|---|---|
| 같은 판정에서 각 필드가 VALID이고 값 범위·기능 상태가 사용 가능 | 해당 필드 VALID | Pi BOOT·VisionSequence·원 AGE |
| 한 필드가 원 INVALID이고 지원되는 사유가 있음 | 후속 제한이 없으면 그 필드 INVALID와 원 사유 사용. 실제 CIS 추가 제한이 생기면 현재 제한 사유를 적용하되 Pi 당시 사유를 원 근거로 별도 보존 | 다른 정상 필드는 독립 유지 |
| 같은 판정의 두 필드가 각각 사용 가능한데 ABSENT/count>0 또는 PRESENT/count=0 | 양쪽 INVALID/DATA_INVALID | 모순을 일으킨 원 값과 품질 |
| 이후 실제 관측 수신 중단 또는 원 AGE 만료 | COMMUNICATION_FAULT 또는 STALE 등 확인한 사유로 현재 사용 제한 | 이전 Pi 사유와 이후 장애를 별도로 보존 |
| 수용된 최신 STATUS의 FAULT와 영향 필드가 확인됨 | 해당 영향 필드 제한 | 판정 원본과 이후 상태·고장 근거 분리 |
| STATUS READY/RECOVERING만 수신 | 상태만 갱신 | 과거 VisionSequence·AGE·유효성을 새 판정으로 복원하지 않음 |
| 다른 CIS 센서에만 고장이 발생 | Pi 정상 관측을 일괄 무효화하지 않음 | 영향 기능별 격리 |

상호 모순 검사는 두 값이 각각 유효·사용 가능한 경우에만 한다. `presence=ABSENT/VALID`, `count=255/INVALID/NO_DATA`에서 255를 정상 인원수로 비교하여 Presence까지 무효화하지 않는다.

원 Pi 사유를 언제나 최종 사유로 고정하지 않는다. 수용된 판정의 원 사유는 보존하되, 이후 통신 단절·만료·기동 변경·확인된 기능 고장이 생기면 현재 사용 품질을 더 제한한다. 예약 코드·미지원 사유 등 형식 오류는 수신 검증 규칙을 따르며 임의의 정상 코드로 바꾸어 수용하지 않는다.

`vision_function_status`는 해당 취합 시점의 CIS 기능 평가다. 독립적으로 갱신된 최신 CIS_STATUS를 과거 occupant record에 붙이지 않는다. 한 벌의 CAN reason만으로 Pi 원 사유와 모든 후속 CIS 장애를 역복원할 수는 없다. 실제 최종 사유·상태 변환 및 C2 원본 연결은 구현 검증으로 확인한다.

## Source AGE를 한 번씩 누적

Pi Network는 원 판정 Age에 **Pi 제출→실제 UART 송신 대기**만 더한다. CIS RX는 원 Age에 아직 반영하지 않은 UART 전달 여유만 한 번 반영한다. CIS는 이 수신 기록의 AGE를 캐시에 두고 `received_at_ms`를 기준점으로 보관한다. Common의 UART50ms/CAN5ms 전달 여유는 PROPOSED이며 이미 반영한 여유를 다시 더하지 않는다. CAN 제출 시 CIS 수신→제출 경과를 한 번 더한 뒤, CIS Network는 제출→실제 CAN 송신 대기만 더한다. 콜백 지연을 이미 반영했다면 같은 수신 기준점으로 다시 더하지 않는다. App·Network의 실제 기준점 연결은 BINDING_OPEN이다.

링크 전송 시간을 공통 clock 없이 정확히 복원할 수 없으므로 필요한 여유/최신성 상한은 팀 Wire·측정 조건으로 결정한다. 65535 포화는 충분히 오래된 값이며 정상 Age0으로 되돌리지 않는다. DOMAIN은 이후 수신→사용/중계 경과만 한 번 더한다.

<a id="허용-입력과-pi-relay"></a>
## 허용 입력과 Pi relay

DOMAIN 원 허용의 합성은 [DOMAIN_PERMISSION](../../03_COMMUNICATION/10_CAN_FD.md#message-domain-permission), Pi 수신의 사용 기한 검사는 [공통 시간표](../../04_COMMON/10_DATA_AND_ID_RULES.md#section-timing)를 따른다. [Pi 허용 소비](../PI/10_DATA.md#허용-소비와-원본-age)에도 같은 원본 사용 기한을 적용한다. 실제 기능 사용과 통신 중계를 나눈다.

후방 활성 Permission은 약 200ms마다 실제 재평가하고 원본 평가의 사용 기한은 500ms다. 값이 바뀌면 즉시 제출하며, 같은 값이어도 실제 새 평가가 성공하면 그 평가 번호·시각을 전달한다. CAN 50ms 송신/150ms 수신 중단과 UART-C 500ms 반복/1500ms 수신 중단은 유지한다. 반복 프레임 때문에 원 AGE를 0으로 만들거나 이전 허용을 연장하지 않는다.

CIS Network는 같은 검증된 `NetDomainPermission`/`NetCanRxMeta`를 `CisApp_OnDomainPermission()`과 private `CisNetPriv_RelayPiPermission()`에 분배한다. CIS App은 후방 기능을 제어하고, Network 내부 relay는 원 허용을 제한·전달한다. 공개 App에 별도의 Pi 허용 생성 API를 요구하지 않는다.

새 원 DOMAIN 평가·수용한 세대 변경·실제 중계 품질 제한이 생기면 중계 record를 갱신해 가능한 즉시 제출한다. 단순 반복은 기존 relay revision과 증가한 AGE를 유지한다. 매 CAN 전송의 TX_SEQUENCE를 새 허용 평가 번호로 쓰지 않는다. `CIS_BOOT_ID`는 CIS 통신 기동이며 원 DOMAIN BOOT가 아니다.

전원 값·전원 품질·수용한 세대·신뢰 가능한 원 AGE를 함께 검사한다. 0/OK는 확인된 불허, 255 또는 NO_DATA/INVALID/STALE는 허용 확인 불가다. 같은 프레임의 WINDOW operation 부재만으로 확인된 Power를 새 불허 정책으로 바꾸지 않으며, 확인되지 않은 공통 AGE로 ALLOW를 생성하지 않는다. `POWER_OPERATION_STATE` 생성원과 독립 입력의 공통 AGE·순번 합성은 미결이다.

`P_PERMISSION.USE_LIMIT=1500ms`는 기존 필드 값 후보로 보존한다. 같은 후방 Power 원 판단의 중계라는 근거를 확인한 경우 `500ms`로 전달하는 안은 조건부 권장 프로필이다. 배포 전 양단 바이트·AGE·만료 해석을 검증하며, 실제 송신 필드 값은 미확정이다. 어떤 `USE_LIMIT`도 원 판단의 검증된 사용 기한을 연장하지 않는다.

제출 거절은 NOT_READY/NO_CAPACITY 등 로컬 반환으로 구분한다. 지연·실패·무수신으로 원 기한을 만족하지 못하면 이전 ALLOW를 새 OK로 재사용하지 않는다. 실제 송신 완료·수신 중단 관측과 소비자 무효화 연결은 BINDING_OPEN이다. Pi가 이 값을 비전 기능에 사용하는 정책은 별도이며, 그 선택을 통신 문서·수신 시험의 선행조건으로 삼지 않는다.

<a id="delta-cis-permission"></a>
**DOMAIN Core 실패 철회 구현 완료 보고 / C2 품질 투영·Network 전달 미검증:** FAILED/NO_DATA 시 기존 ALLOW 즉시 철회는 DOMAIN 담당자 구현 완료 보고다. 원인별 UNKNOWN의 C2 Quality 매핑은 수용 대기이며, 검증된 평가 실패·무효 품질이 실제 전달된 경우 원500ms 안이라도 과거 ALLOW를 사용하지 않는다. 원 성공 평가 순번이 같다는 이유로 품질 하향을 누락하지 않고 검증된 입력을 기존 App callback과 private relay에 함께 전달한다. 원 성공 평가·CAN 합성·CIS relay 번호 공간은 별개이며 무효 보고로 성공 순번·AGE0을 만들지 않는다. relay 갱신·즉시 전달 요청과 원격 실제 적용을 구분하고 적체·송신 실패 뒤 구 ALLOW 재전달 방지는 연결 시험에서 확인한다. [권장 투영과 실패의 지위](../../07_CROSS_NODE_INTERFACE_RULES.md#permission-delta)

## 센서별 부분 실패와 링크 관측 —

**PROPOSED — 독립 원 관측을 그대로 보고:** CIS owner는 `NetCisEnvironment`의 각 값·sequence·AGE·validity·reason 묶음을 독립 갱신해 기존 `CisNet_PublishEnvironment()`로 제출한다. 예를 들어 온도 NO_DATA는 기존 온도 무효 표현과 INVALID/NO_DATA 사유로 보고하되, 유효한 습도·조도의 값과 원 측정 번호·AGE를 보존한다. 환경 frame 수신이나 CIS READY는 세 값의 원 측정을 갱신하지 않는다. 후방 UNAVAILABLE/FAULT 및 거리 65535를 정상 거리 0 또는 NO_OBJECT로 바꾸지 않으며, DOMAIN의 위험/CLEAR 판단을 CIS가 대신 생성하지 않는다. 기존 타입·raw를 사용하는 `NETWORK_OWNED` 전달 권고이며 센서 사용 범위·기능 판단은 `FUNCTION_OWNER` 확인 사항이다.

Network가 관측한 Pi UART/CAN 제공 경로 상태는 CIS 진단 owner가 실제 근거와 영향 기능을 확인한 뒤 기존 `NetCisStatus`의 interface 상태·현재 fault·affected_function에 반영하고 `CisNet_PublishStatus()`로 제출하는 안을 권장한다. 단순 수신 누락을 센서 자체 고장이나 최근 fault의 새 발생으로 직접 변환하지 않는다. 다른 정상 센서의 값/품질은 보존하고, 링크 실패가 영향을 주는 제공·소비 경로만 사용을 제한한다. CIS 진단 모듈은 기존 Health 연결과 실제 영향 기능을 기능 코드에서 확인한다. 그 연결 증거는 아직 제공되지 않았다. 미확인 health 전달 함수나 신규 fault frame을 만들지 않는다. SDK/Firmware/Codec/Board 시험은 `NOT_EXECUTED`다.

<a id="r1-permission-consumption"></a>
**후방 허용 소비:** DOMAIN의 원 Power 평가 주기 약 200ms·원본 판단의 사용 기한 500ms를 적용한다. 원 평가 AGE 499ms라도 출처·품질·기능 조건이 모두 유효해야 하며, 500ms/501ms에는 허용을 사용하지 않는다. CIS relay 송신이나 Pi 재수신으로 원 AGE를 0으로 만들지 않는다. P_PERMISSION.USE_LIMIT 1500ms 후보를 500ms로 바꾸는 안은 원 갱신·전송·기능 owner 근거를 필요로 하는 CONDITIONAL이며 자동 적용하지 않는다. 공유 Operation/Alive/Power의 미확인 source는 미결이며 UNKNOWN을 ALLOW로 승격하지 않는다. WINDOW 부재만으로 이미 확인된 별도 Power를 DENIED로 덮어쓰지도 않는다.

<a id="환경-원본-최신성--o20"></a>
## 환경 원본 최신성 —

<a id="r1-environment-transition"></a>
온도·습도 데이터의 사용 기한은 원본 관측 시점부터 각각 2500ms다. 2499ms에는 해당 센서의 품질까지 확인하고, 2500ms·2501ms에는 사용을 제한한다. 한 센서의 새 유효 관측은 그 센서만 회복시키며 다른 센서의 AGE/품질을 초기화하지 않는다. 조도 1000ms는 미확정 권장 후보다. 새 CIS BOOT를 수용하면 이전 BOOT의 캐시를 정상으로 재사용하지 않고, 낮은 새 관측 순번을 이전 세대의 높은 순번과 비교하지 않는다. 뒤늦게 도착한 이전 BOOT 프레임은 새 세대의 복구 근거가 아니다. 기존 C2→VSM 저장·독립 Meta·Getter까지의 무효화/회복은 실제 연결 검증 대기이며, 현재 구조로 불가능하다는 근거가 있을 때만 최소 계약 변경을 검토한다.

온도·습도는 각각 원본 관측 시점부터 2500ms까지만 사용할 수 있다. 원 AGE가 2500ms에 도달하면 해당 값을 사용하지 않는다. 다른 센서의 정상 측정이나 CAN 반복 송신으로 원 AGE·측정 순번을 갱신하지 않는다.

CIS의 참고 코드에는 약 2000ms 측정 주기와 `ENV_STALE_MS=5000ms` 후보가 있다. 5000ms는 현재 코드의 값 유효성 검사 후보이며 진단 전용 한도로 확정된 것이 아니다. 이 값이 DOMAIN의 2500ms 소비 한도를 연장하지 않는다. CIS 쪽 적용 방식과 `age >= limit` 경계는 기능 코드 연결 시 확인한다.

실내 조도 1000ms는 기능 정책이 미확정인 후보다. CIS 로컬 `LUX_STALE_MS=1000`은 DOMAIN 사용 기한을 확정하는 근거가 아니다. SysRS의 온습도·실내/외 조도 묶음 기준과 별개로 온습도에는 각 2500ms를 적용하며 조도는 후보로 남는다. CAN 환경 보고 200ms/수신 중단 600ms 후보는 원본 사용 기한과 별개다.

환경 C2는 현재 구조를 유지하고 Network는 CIS BOOT와 센서별 Sequence·AGE를 보존한다. 새 BOOT를 확인하면 이전 BOOT에서 대기 중인 환경 데이터의 전달을 차단한다. DOMAIN의 동기 Private Transition이 연결되어 이전 저장값의 무효화 완료를 확인할 수 있는 경우에만 새 BOOT 값을 기존 환경 Rx에 전달한다. 완료를 확인하지 못하면 해당 환경 입력은 제한한다. 일반 환경 NO_DATA·STALE은 재부팅 신호로 사용하지 않는다.

<a id="delta-cis-environment"></a>
연속 BOOT 자동 복귀는 DOMAIN Core의 순차 Epoch 지원과 별개로 Network의 실제 Private 호출·Receipt 대조 및 환경 VSM/Query 연동을 확인한 후에만 허용한다. DOMAIN Production Private Core는 첫 Epoch 1·완료 후 N+1·중복/충돌·STALE/ORDER_ERROR를 구현한다(Host 시험은 담당자 보고). 다만 실제 Network Private Binding은 확인되지 않았다. 환경 수명 2500ms, 기존 공개 C2/Wire, 원 Sequence의 유지 원칙은 그대로 적용한다. [환경 BOOT 처리](../../07_CROSS_NODE_INTERFACE_RULES.md#environment-delta)

## 구현 전 결정

기존 Wire의 단위·범위·형식은 보존한다. 실제 센서 유효 범위, runtime BOOT 생성·수용, 기능 상태·fault 변환, UART/CAN Health의 실제 연결은 확인이 남아 있다. 통신 처리는 Network가 담당하고, 센서·비전 기능 판단은 CIS·Pi가 구현한다. 별도 CIS_FAULT 메시지·Pi↔DOMAIN 직결·원격 Query API는 추가하지 않는다. 실제 Firmware/Codec/Board 시험은 수행하지 않았다.

## 후방 비활성·재활성 및 기동 경계

후방 거리 원 관측 사용 한도는 `CIS_REAR` 150ms 후보이며 Permission 원 500ms와 별개다. SysRS 후방 거리 최대 500ms 잠정 기준보다 이르게 사용을 제한하지만 실제 센서·전송·소비 주기에서의 가용성은 미검증이다. 이 프로필에서는 누적 AGE 150ms에 도달하면 거리값을 사용하지 않으며 149ms에서도 품질·상태·수신 감시 조건을 만족해야 한다. 실제 DOMAIN 소비 기준 변경에는 해당 거리 한도와 가용성 검증이 필요하다.

후방 허용 상실 시 `INACTIVE=5`를 사용하되, 확인된 불허와 허용 미확인·만료는 validity/reason으로 구분한다. 어떤 경우에도 가짜 거리나 NO_OBJECT를 만들어 정상으로 표시하지 않는다. 재활성 후에는 새 실제 관측이 확인된 뒤 정상 결과를 제공한다. raw 0.1cm는 mm와 같으며 `rearDistanceMm`로 전달할 때 정수 cm로 줄이지 않는다.

권장 프로필: 같은 CIS BOOT가 유지되는 동안 센서만 재초기화하더라도 그 센서의 관측 순번을 임의로 되감지 않는다. 새 CIS BOOT를 수용한 뒤에는 이전 세대의 높은 Sequence와 단순 비교하지 않는다. 이미 종료한 Pi BOOT A를 B 수용 후 다시 받는 경우에는 구세대 차단 규칙을 적용한다. 구세대 기억의 유지 범위·기동 값 충돌·runtime 재시작 뒤의 처리 방식은 검증해야 하며, BOOT 값만으로 영구 재생 공격 방지를 보장하지 않는다.

C2의 기존 `sourceGeneration` 필드가 있는 타입에서는 유효 관측의 nonzero 후보 규칙과 `STALE + generation 0` 정책 미결을 구분한다. 이 예외를 임의로 NO_DATA나 만들어 낸 nonzero로 치환하지 않는다. 환경 C2에 새 Generation 필드를 추가하라는 의미도 아니다. 해당 generation 정책은 DOMAIN 연동 전에 확정해야 한다.
