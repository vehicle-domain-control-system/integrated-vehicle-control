# BCM Data — 명령, 실제 상태, 결과와 보호 발생

Interface v2 · r1 · [규격과 연동 조건](../../07_CROSS_NODE_INTERFACE_RULES.md#status)

[전체 시작](../../00_START_HERE.md) · [개요](00_OVERVIEW.md) · [Data](10_DATA.md) · [API](20_API.md)

**PROPOSED_DESIGN · BINDING_OPEN.** 아래 전체 선언은 의미 타입을 표시하는 설계용 C 표현이다. 실제 설치 헤더/firmware가 아니며 struct padding·enum 크기를 raw bytes로 전송하지 않는다. [공통 ID·수명](../../04_COMMON/10_DATA_AND_ID_RULES.md)과 [C enum↔Wire 명시 변환](../../04_COMMON/30_ENUM_WIRE_MAPPING.md)을 함께 적용한다.

## 타입 빠른 찾기

| 타입 | 생성·소비 / 관련 메시지 |
|---|---|
| [`NetBcmDoorCommand`](#type-netbcmdoorcommand) | DOMAIN 기능 결정 → BCM 실행 · [`BCM_DOOR_COMMAND`](../../03_COMMUNICATION/10_CAN_FD.md#message-bcm-door-command) |
| [`NetBcmClimateCommand`](#type-netbcmclimatecommand) | DOMAIN 기능 결정 → BCM 실행 · [`BCM_CLIMATE_COMMAND`](../../03_COMMUNICATION/10_CAN_FD.md#message-bcm-climate-command) |
| [`NetBcmLightCommand`](#type-netbcmlightcommand) | DOMAIN 기능 결정 → BCM 실행 · [`BCM_LIGHT_COMMAND`](../../03_COMMUNICATION/10_CAN_FD.md#message-bcm-light-command) |
| [`NetBcmDoorState`](#type-netbcmdoorstate) | BCM 실제 관측·실행·진단 → DOMAIN · [`BCM_DOOR_STATE`](../../03_COMMUNICATION/10_CAN_FD.md#message-bcm-door-state) |
| [`NetBcmClimateState`](#type-netbcmclimatestate) | BCM 실제 관측·실행·진단 → DOMAIN · [`BCM_CLIMATE_STATE`](../../03_COMMUNICATION/10_CAN_FD.md#message-bcm-climate-state) |
| [`NetBcmLightState`](#type-netbcmlightstate) | BCM 실제 관측·실행·진단 → DOMAIN · [`BCM_LIGHT_STATE`](../../03_COMMUNICATION/10_CAN_FD.md#message-bcm-light-state) |
| [`NetBcmResult`](#type-netbcmresult) | BCM 실제 관측·실행·진단 → DOMAIN · [`BCM_RESULT`](../../03_COMMUNICATION/10_CAN_FD.md#message-bcm-result) |
| [`NetBcmEvent`](#type-netbcmevent) | BCM 실제 관측·실행·진단 → DOMAIN · [`BCM_EVENT`](../../03_COMMUNICATION/10_CAN_FD.md#message-bcm-event) |
| [`NetBcmStatus`](#type-netbcmstatus) | BCM 실제 관측·실행·진단 → DOMAIN · [`BCM_STATUS`](../../03_COMMUNICATION/10_CAN_FD.md#message-bcm-status) |
| [`NetDoorTarget`](#type-netdoortarget) | 노드 전용 코드·bit 집합 |
| [`NetDoorLockState`](#type-netdoorlockstate) | 노드 전용 코드·bit 집합 |
| [`NetDoorOpenState`](#type-netdooropenstate) | 노드 전용 코드·bit 집합 |
| [`NetDoorCompositeState`](#type-netdoorcompositestate) | 노드 전용 코드·bit 집합 |
| [`NetHeatRemovalState`](#type-netheatremovalstate) | 노드 전용 코드·bit 집합 |
| [`NetLightApplyResult`](#type-netlightapplyresult) | 노드 전용 코드·bit 집합 |
| [`NetBcmCommandKind`](#type-netbcmcommandkind) | 노드 전용 코드·bit 집합 |
| [`NetDoorEvent`](#type-netdoorevent) | 노드 전용 코드·bit 집합 |
| [`NetBcmEventKind`](#type-netbcmeventkind) | 노드 전용 코드·bit 집합 |
| [`NetBcmFaultMask`](#type-netbcmfaultmask) | 노드 전용 코드·bit 집합 |
| [`NetBcmFaultCode`](#type-netbcmfaultcode) | 노드 전용 코드·bit 집합 |

이 문서와 로컬 API가 사용하는 공용 타입의 전체 선언: [`NetSubmitResult`](../../04_COMMON/10_DATA_AND_ID_RULES.md#type-netsubmitresult), [`NetCanRxMeta`](../../04_COMMON/10_DATA_AND_ID_RULES.md#type-netcanrxmeta), [`NetCommandMeta`](../../04_COMMON/10_DATA_AND_ID_RULES.md#type-netcommandmeta), [`NetDomainPermission`](../../04_COMMON/10_DATA_AND_ID_RULES.md#type-netdomainpermission), [`NetRgb`](../../04_COMMON/10_DATA_AND_ID_RULES.md#type-netrgb), [`NetValueQuality`](../../04_COMMON/10_DATA_AND_ID_RULES.md#type-netvaluequality), [`NetVehicleResult`](../../04_COMMON/10_DATA_AND_ID_RULES.md#type-netvehicleresult), [`NetReason`](../../04_COMMON/10_DATA_AND_ID_RULES.md#type-netreason), [`NetFlag`](../../04_COMMON/10_DATA_AND_ID_RULES.md#type-netflag), [`NetFanLevel`](../../04_COMMON/10_DATA_AND_ID_RULES.md#type-netfanlevel), [`NetThermalDirection`](../../04_COMMON/10_DATA_AND_ID_RULES.md#type-netthermaldirection), [`NetLightType`](../../04_COMMON/10_DATA_AND_ID_RULES.md#type-netlighttype), [`NetOrigin`](../../04_COMMON/10_DATA_AND_ID_RULES.md#type-netorigin), [`NetFaultCategory`](../../04_COMMON/10_DATA_AND_ID_RULES.md#type-netfaultcategory), [`NetControllerState`](../../04_COMMON/10_DATA_AND_ID_RULES.md#type-netcontrollerstate).

<a id="type-netbcmdoorcommand"></a>
## `NetBcmDoorCommand`

DOMAIN 기능 결정 → BCM 실행 · [`BCM_DOOR_COMMAND`](../../03_COMMUNICATION/10_CAN_FD.md#message-bcm-door-command)

```c
typedef struct NetBcmDoorCommand
{
    NetCommandMeta command; /**< 원본 명령 공통부의 의미 필드. CAN 송신 헤더는 C2 `commandId`를 보존하여 Network가 보완한다. */
    NetDoorTarget door_lock_target; /**< DOOR_LOCK_TARGET: 0=LOCK; 1=UNLOCK */
    NetOrigin origin; /**< ORIGIN: 요청 출처 코드 */
} NetBcmDoorCommand;
```

**로컬 사용:** [`BcmApp_OnDomainDoorCommand()`](20_API.md#api-bcmapp_ondomaindoorcommand) · 전체 타입/인자·호출자·수명·실패는 API에서 확인한다.

| 멤버 | 정확한 C 타입 | 값·단위·범위 / 역할 | 원 소유자·유효 조건 |
|---|---|---|---|
| `command` | `NetCommandMeta` | 원본 명령 공통부의 의미 필드. CAN 송신 헤더는 C2 `commandId`를 보존하여 Network가 보완한다. | 원 ID·목표: DOMAIN 기능; 대상 Boot·전송 Age: DOMAIN Network. BCM은 검증한 수신 문맥만 소비. |
| `door_lock_target` | `NetDoorTarget` | 0=LOCK; 1=UNLOCK | DOMAIN 기능 결정. BCM은 로컬 보호·가용성을 별도 판단하며 Network 접수로 실행을 확정하지 않음. |
| `origin` | `NetOrigin` | 요청 출처 코드 | DOMAIN 기능 결정. BCM은 로컬 보호·가용성을 별도 판단하며 Network 접수로 실행을 확정하지 않음. |

<a id="command-context"></a>
`command`의 전체 선언·4개 멤버는 [`NetCommandMeta`](../../04_COMMON/10_DATA_AND_ID_RULES.md#type-netcommandmeta)에 둔다. `command_id`는 DOMAIN C2 원 결정 ID, `target_boot_id`는 DOMAIN Network가 확인한 대상 BCM 세대, `command_age_ms`는 원 명령 생성 이후 경과, `accept_limit_ms`는 제안 수용 상한이다. BCM은 `meta.source.boot_id`로 원 DOMAIN 세대를 함께 보존한다. 200ms↔구 SysRS 1초는 CONTRACT_OPEN이며 이 문서에서 확정하지 않는다.
<a id="type-netbcmclimatecommand"></a>
## `NetBcmClimateCommand`

DOMAIN 기능 결정 → BCM 실행 · [`BCM_CLIMATE_COMMAND`](../../03_COMMUNICATION/10_CAN_FD.md#message-bcm-climate-command)

```c
typedef struct NetBcmClimateCommand
{
    NetCommandMeta command; /**< 원본 명령 공통부의 의미 필드. CAN 송신 헤더는 C2 `commandId`를 보존하여 Network가 보완한다. */
    NetFanLevel fan_target_level; /**< FAN_TARGET_LEVEL: FAN 코드; UNKNOWN 금지 */
    NetThermalDirection thermal_direction; /**< THERMAL_DIRECTION: 0=IDLE; 1=COOL; 2=HEAT */
    uint8_t thermal_target_percent; /**< THERMAL_TARGET_LEVEL: 0~100%, 1%/raw */
} NetBcmClimateCommand;
```

**로컬 사용:** [`BcmApp_OnDomainClimateCommand()`](20_API.md#api-bcmapp_ondomainclimatecommand) · 전체 타입/인자·호출자·수명·실패는 API에서 확인한다.

| 멤버 | 정확한 C 타입 | 값·단위·범위 / 역할 | 원 소유자·유효 조건 |
|---|---|---|---|
| `command` | `NetCommandMeta` | 원본 명령 공통부의 의미 필드. CAN 송신 헤더는 C2 `commandId`를 보존하여 Network가 보완한다. | 원 ID·목표: DOMAIN 기능; 대상 Boot·전송 Age: DOMAIN Network. BCM은 검증한 수신 문맥만 소비. |
| `fan_target_level` | `NetFanLevel` | FAN 코드; UNKNOWN 금지 | DOMAIN 기능 결정. BCM은 로컬 보호·가용성을 별도 판단하며 Network 접수로 실행을 확정하지 않음. |
| `thermal_direction` | `NetThermalDirection` | 0=IDLE; 1=COOL; 2=HEAT | DOMAIN 기능 결정. BCM은 로컬 보호·가용성을 별도 판단하며 Network 접수로 실행을 확정하지 않음. |
| `thermal_target_percent` | `uint8_t` | 0~100%, 1%/raw | DOMAIN 기능 결정. BCM은 로컬 보호·가용성을 별도 판단하며 Network 접수로 실행을 확정하지 않음. |

`command`는 [명령 공통부의 ID·기동·Age·수용 한도](#command-context)를 그대로 적용한다. 구체 기능 목표와 로컬 보호는 이 타입의 멤버 계약을 따른다.
<a id="type-netbcmlightcommand"></a>
## `NetBcmLightCommand`

DOMAIN 기능 결정 → BCM 실행 · [`BCM_LIGHT_COMMAND`](../../03_COMMUNICATION/10_CAN_FD.md#message-bcm-light-command)

```c
typedef struct NetBcmLightCommand
{
    NetCommandMeta command; /**< 원본 명령 공통부의 의미 필드. CAN 송신 헤더는 C2 `commandId`를 보존하여 Network가 보완한다. */
    NetLightType interior_light_type; /**< INTERIOR_LIGHT_TYPE: LIGHT_TYPE 코드 */
    uint8_t light_level_percent; /**< INTERIOR_LIGHT_LEVEL: 0~100%, 1%/raw */
    NetRgb interior_light_color; /**< INTERIOR_LIGHT_COLOR: R/G/B 각각 0~255 */
} NetBcmLightCommand;
```

**로컬 사용:** [`BcmApp_OnDomainLightCommand()`](20_API.md#api-bcmapp_ondomainlightcommand) · 전체 타입/인자·호출자·수명·실패는 API에서 확인한다.

| 멤버 | 정확한 C 타입 | 값·단위·범위 / 역할 | 원 소유자·유효 조건 |
|---|---|---|---|
| `command` | `NetCommandMeta` | 원본 명령 공통부의 의미 필드. CAN 송신 헤더는 C2 `commandId`를 보존하여 Network가 보완한다. | 원 ID·목표: DOMAIN 기능; 대상 Boot·전송 Age: DOMAIN Network. BCM은 검증한 수신 문맥만 소비. |
| `interior_light_type` | `NetLightType` | LIGHT_TYPE 코드 | DOMAIN 기능 결정. BCM은 로컬 보호·가용성을 별도 판단하며 Network 접수로 실행을 확정하지 않음. |
| `light_level_percent` | `uint8_t` | 0~100%, 1%/raw | DOMAIN 기능 결정. BCM은 로컬 보호·가용성을 별도 판단하며 Network 접수로 실행을 확정하지 않음. |
| `interior_light_color` | `NetRgb` | R/G/B 각각 0~255 | DOMAIN 기능 결정. BCM은 로컬 보호·가용성을 별도 판단하며 Network 접수로 실행을 확정하지 않음. |

`command`는 [명령 공통부의 ID·기동·Age·수용 한도](#command-context)를 그대로 적용한다. 구체 기능 목표와 로컬 보호는 이 타입의 멤버 계약을 따른다.

`interior_light_color`는 [`NetRgb`](../../04_COMMON/10_DATA_AND_ID_RULES.md#type-netrgb)의 `r/g/b: uint8_t` 각 0~255다. EXTERIOR_LIGHT는 범위에서 제외되며 이 명령은 BCM INTERIOR_LIGHT만 대상으로 한다.
<a id="type-netbcmdoorstate"></a>
## `NetBcmDoorState`

BCM 실제 관측·실행·진단 → DOMAIN · [`BCM_DOOR_STATE`](../../03_COMMUNICATION/10_CAN_FD.md#message-bcm-door-state)

```c
typedef struct NetBcmDoorState
{
    NetDoorLockState door_lock_state; /**< DOOR_LOCK_STATE: 0=LOCKED; 1=UNLOCKED; 255=UNKNOWN */
    NetDoorOpenState door_open_state; /**< DOOR_OPEN_STATE: 0=CLOSED; 1=OPEN; 255=UNKNOWN */
    NetDoorCompositeState door_composite_state; /**< DOOR_COMPOSITE_STATE: 0=NORMAL; 1=INCONSISTENT; 2=UNTRUSTED */
    NetValueQuality lock_quality; /**< LOCK_QUALITY: VALUE_QUALITY */
    NetValueQuality open_quality; /**< OPEN_QUALITY: VALUE_QUALITY */
    NetValueQuality composite_quality; /**< COMPOSITE_QUALITY: VALUE_QUALITY */
    uint32_t update_sequence; /**< UPDATE_SEQUENCE: 이번 도어 상태 평가 번호 */
    uint16_t source_age_ms; /**< SOURCE_AGE: 평가 후 경과 시간, ms */
} NetBcmDoorState;
```

**로컬 사용:** [`BcmNet_PublishDoorState()`](20_API.md#api-bcmnet_publishdoorstate) · 전체 타입/인자·호출자·수명·실패는 API에서 확인한다.

| 멤버 | 정확한 C 타입 | 값·단위·범위 / 역할 | 원 소유자·유효 조건 |
|---|---|---|---|
| `door_lock_state` | `NetDoorLockState` | 0=LOCKED; 1=UNLOCKED; 255=UNKNOWN | BCM 해당 실행/관측 담당. 연관 quality가 OK일 때 현재 제어 근거; UNKNOWN/INVALID를 정상값으로 승격하지 않음. |
| `door_open_state` | `NetDoorOpenState` | 0=CLOSED; 1=OPEN; 255=UNKNOWN | BCM 해당 실행/관측 담당. 연관 quality가 OK일 때 현재 제어 근거; UNKNOWN/INVALID를 정상값으로 승격하지 않음. |
| `door_composite_state` | `NetDoorCompositeState` | 0=NORMAL; 1=INCONSISTENT; 2=UNTRUSTED | BCM 해당 실행/관측 담당. 연관 quality가 OK일 때 현재 제어 근거; UNKNOWN/INVALID를 정상값으로 승격하지 않음. |
| `lock_quality` | `NetValueQuality` | VALUE_QUALITY | BCM 해당 실행/관측 담당. 연관 quality가 OK일 때 현재 제어 근거; UNKNOWN/INVALID를 정상값으로 승격하지 않음. |
| `open_quality` | `NetValueQuality` | VALUE_QUALITY | BCM 해당 실행/관측 담당. 연관 quality가 OK일 때 현재 제어 근거; UNKNOWN/INVALID를 정상값으로 승격하지 않음. |
| `composite_quality` | `NetValueQuality` | VALUE_QUALITY | BCM 해당 실행/관측 담당. 연관 quality가 OK일 때 현재 제어 근거; UNKNOWN/INVALID를 정상값으로 승격하지 않음. |
| `update_sequence` | `uint32_t` | 이번 도어 상태 평가 번호 | BCM 해당 상태의 새 의미 평가에서만 증가. 프레임 재전송 TX_SEQUENCE와 별개. |
| `source_age_ms` | `uint16_t` | 평가 후 경과 시간, ms | BCM 원 평가 후 ms; App 제출 시 현재 원 Age, Network 보관→송신 대기만 한 번 누적·65535 포화. |
<a id="type-netbcmclimatestate"></a>
## `NetBcmClimateState`

BCM 실제 관측·실행·진단 → DOMAIN · [`BCM_CLIMATE_STATE`](../../03_COMMUNICATION/10_CAN_FD.md#message-bcm-climate-state)

```c
typedef struct NetBcmClimateState
{
    NetFanLevel fan_command_level; /**< FAN_COMMAND_LEVEL: 0=OFF; 1=LOW; 2=MEDIUM; 3=HIGH */
    NetFanLevel fan_measured_level; /**< FAN_MEASURED_LEVEL: FAN 코드 및 255=UNKNOWN */
    NetThermalDirection thermal_direction_state; /**< THERMAL_DIRECTION_STATE: 0=IDLE; 1=COOL; 2=HEAT */
    uint8_t thermal_output_percent; /**< THERMAL_OUTPUT_LEVEL: 0~100%, 1%/raw */
    NetHeatRemovalState heat_removal_state; /**< HEAT_REMOVAL_STATE: 0=정상; 1=과열 차단; 255=측정 불가 */
    NetValueQuality fan_command_quality; /**< FAN_COMMAND_QUALITY: VALUE_QUALITY */
    NetValueQuality fan_measured_quality; /**< FAN_MEASURED_QUALITY: VALUE_QUALITY */
    NetValueQuality thermal_direction_quality; /**< THERMAL_DIRECTION_QUALITY: VALUE_QUALITY */
    NetValueQuality thermal_output_quality; /**< THERMAL_OUTPUT_QUALITY: VALUE_QUALITY */
    NetValueQuality heat_removal_quality; /**< HEAT_REMOVAL_QUALITY: VALUE_QUALITY */
    uint32_t update_sequence; /**< UPDATE_SEQUENCE: 이번 공조 상태 평가 번호 */
    uint16_t source_age_ms; /**< SOURCE_AGE: 평가 후 경과 시간, ms */
} NetBcmClimateState;
```

**로컬 사용:** [`BcmNet_PublishClimateState()`](20_API.md#api-bcmnet_publishclimatestate) · 전체 타입/인자·호출자·수명·실패는 API에서 확인한다.

| 멤버 | 정확한 C 타입 | 값·단위·범위 / 역할 | 원 소유자·유효 조건 |
|---|---|---|---|
| `fan_command_level` | `NetFanLevel` | 0=OFF; 1=LOW; 2=MEDIUM; 3=HIGH | BCM 해당 실행/관측 담당. 연관 quality가 OK일 때 현재 제어 근거; UNKNOWN/INVALID를 정상값으로 승격하지 않음. |
| `fan_measured_level` | `NetFanLevel` | FAN 코드 및 255=UNKNOWN | BCM 해당 실행/관측 담당. 연관 quality가 OK일 때 현재 제어 근거; UNKNOWN/INVALID를 정상값으로 승격하지 않음. |
| `thermal_direction_state` | `NetThermalDirection` | 0=IDLE; 1=COOL; 2=HEAT | BCM 해당 실행/관측 담당. 연관 quality가 OK일 때 현재 제어 근거; UNKNOWN/INVALID를 정상값으로 승격하지 않음. |
| `thermal_output_percent` | `uint8_t` | 0~100%, 1%/raw | BCM 해당 실행/관측 담당. 연관 quality가 OK일 때 현재 제어 근거; UNKNOWN/INVALID를 정상값으로 승격하지 않음. |
| `heat_removal_state` | `NetHeatRemovalState` | 0=정상; 1=과열 차단; 255=측정 불가 | BCM 해당 실행/관측 담당. 연관 quality가 OK일 때 현재 제어 근거; UNKNOWN/INVALID를 정상값으로 승격하지 않음. |
| `fan_command_quality` | `NetValueQuality` | VALUE_QUALITY | BCM 해당 실행/관측 담당. 연관 quality가 OK일 때 현재 제어 근거; UNKNOWN/INVALID를 정상값으로 승격하지 않음. |
| `fan_measured_quality` | `NetValueQuality` | VALUE_QUALITY | BCM 해당 실행/관측 담당. 연관 quality가 OK일 때 현재 제어 근거; UNKNOWN/INVALID를 정상값으로 승격하지 않음. |
| `thermal_direction_quality` | `NetValueQuality` | VALUE_QUALITY | BCM 해당 실행/관측 담당. 연관 quality가 OK일 때 현재 제어 근거; UNKNOWN/INVALID를 정상값으로 승격하지 않음. |
| `thermal_output_quality` | `NetValueQuality` | VALUE_QUALITY | BCM 해당 실행/관측 담당. 연관 quality가 OK일 때 현재 제어 근거; UNKNOWN/INVALID를 정상값으로 승격하지 않음. |
| `heat_removal_quality` | `NetValueQuality` | VALUE_QUALITY | BCM 해당 실행/관측 담당. 연관 quality가 OK일 때 현재 제어 근거; UNKNOWN/INVALID를 정상값으로 승격하지 않음. |
| `update_sequence` | `uint32_t` | 이번 공조 상태 평가 번호 | BCM 해당 상태의 새 의미 평가에서만 증가. 프레임 재전송 TX_SEQUENCE와 별개. |
| `source_age_ms` | `uint16_t` | 평가 후 경과 시간, ms | BCM 원 평가 후 ms; App 제출 시 현재 원 Age, Network 보관→송신 대기만 한 번 누적·65535 포화. |
<a id="type-netbcmlightstate"></a>
## `NetBcmLightState`

BCM 실제 관측·실행·진단 → DOMAIN · [`BCM_LIGHT_STATE`](../../03_COMMUNICATION/10_CAN_FD.md#message-bcm-light-state)

```c
typedef struct NetBcmLightState
{
    NetLightType interior_light_type_state; /**< INTERIOR_LIGHT_TYPE_STATE: LIGHT_TYPE 코드 */
    uint8_t light_level_percent; /**< INTERIOR_LIGHT_LEVEL_STATE: 0~100% */
    NetRgb interior_light_color_state; /**< INTERIOR_LIGHT_COLOR_STATE: R/G/B 각각 0~255 */
    NetLightApplyResult interior_light_apply_result; /**< INTERIOR_LIGHT_APPLY_RESULT: 0=적용됨; 1=실패; 255=UNKNOWN */
    NetFlag light_physical_feedback_capability; /**< LIGHT_PHYSICAL_FEEDBACK_CAPABILITY: 0=미지원; 1=지원; 현재 0 */
    NetValueQuality light_type_quality; /**< LIGHT_TYPE_QUALITY: VALUE_QUALITY */
    NetValueQuality light_level_quality; /**< LIGHT_LEVEL_QUALITY: VALUE_QUALITY */
    NetValueQuality light_color_quality; /**< LIGHT_COLOR_QUALITY: VALUE_QUALITY */
    NetValueQuality light_apply_quality; /**< LIGHT_APPLY_QUALITY: VALUE_QUALITY */
    uint32_t update_sequence; /**< UPDATE_SEQUENCE: 이번 조명 적용 상태 평가 번호 */
    uint16_t source_age_ms; /**< SOURCE_AGE: 평가 후 경과 시간, ms */
} NetBcmLightState;
```

**로컬 사용:** [`BcmNet_PublishLightState()`](20_API.md#api-bcmnet_publishlightstate) · 전체 타입/인자·호출자·수명·실패는 API에서 확인한다.

| 멤버 | 정확한 C 타입 | 값·단위·범위 / 역할 | 원 소유자·유효 조건 |
|---|---|---|---|
| `interior_light_type_state` | `NetLightType` | LIGHT_TYPE 코드 | BCM 해당 실행/관측 담당. 연관 quality가 OK일 때 현재 제어 근거; UNKNOWN/INVALID를 정상값으로 승격하지 않음. |
| `light_level_percent` | `uint8_t` | 0~100% | BCM 해당 실행/관측 담당. 연관 quality가 OK일 때 현재 제어 근거; UNKNOWN/INVALID를 정상값으로 승격하지 않음. |
| `interior_light_color_state` | `NetRgb` | R/G/B 각각 0~255 | BCM 해당 실행/관측 담당. 연관 quality가 OK일 때 현재 제어 근거; UNKNOWN/INVALID를 정상값으로 승격하지 않음. |
| `interior_light_apply_result` | `NetLightApplyResult` | 0=적용됨; 1=실패; 255=UNKNOWN | BCM 해당 실행/관측 담당. 연관 quality가 OK일 때 현재 제어 근거; UNKNOWN/INVALID를 정상값으로 승격하지 않음. |
| `light_physical_feedback_capability` | `NetFlag` | 0=미지원; 1=지원; 현재 0 | BCM 구성 owner. 현재 물리 점등 확인 미지원(0); 적용 성공을 실제 점등 확인으로 표시하지 않음. |
| `light_type_quality` | `NetValueQuality` | VALUE_QUALITY | BCM 해당 실행/관측 담당. 연관 quality가 OK일 때 현재 제어 근거; UNKNOWN/INVALID를 정상값으로 승격하지 않음. |
| `light_level_quality` | `NetValueQuality` | VALUE_QUALITY | BCM 해당 실행/관측 담당. 연관 quality가 OK일 때 현재 제어 근거; UNKNOWN/INVALID를 정상값으로 승격하지 않음. |
| `light_color_quality` | `NetValueQuality` | VALUE_QUALITY | BCM 해당 실행/관측 담당. 연관 quality가 OK일 때 현재 제어 근거; UNKNOWN/INVALID를 정상값으로 승격하지 않음. |
| `light_apply_quality` | `NetValueQuality` | VALUE_QUALITY | BCM 해당 실행/관측 담당. 연관 quality가 OK일 때 현재 제어 근거; UNKNOWN/INVALID를 정상값으로 승격하지 않음. |
| `update_sequence` | `uint32_t` | 이번 조명 적용 상태 평가 번호 | BCM 해당 상태의 새 의미 평가에서만 증가. 프레임 재전송 TX_SEQUENCE와 별개. |
| `source_age_ms` | `uint16_t` | 평가 후 경과 시간, ms | BCM 원 평가 후 ms; App 제출 시 현재 원 Age, Network 보관→송신 대기만 한 번 누적·65535 포화. |

`interior_light_color_state`는 [`NetRgb`](../../04_COMMON/10_DATA_AND_ID_RULES.md#type-netrgb)의 `r/g/b: uint8_t` 각 0~255다. 조명 출력 지시의 적용 상태이며 물리 점등 확인 미지원과 별개다.
<a id="type-netbcmresult"></a>
## `NetBcmResult`

BCM 실제 관측·실행·진단 → DOMAIN · [`BCM_RESULT`](../../03_COMMUNICATION/10_CAN_FD.md#message-bcm-result)

```c
typedef struct NetBcmResult
{
    uint32_t command_domain_boot; /**< COMMAND_DOMAIN_BOOT: 원 명령을 생성한 Domain 기동 번호 */
    uint32_t command_id; /**< COMMAND_ID: 처리한 Domain 명령 ID */
    NetBcmCommandKind command_kind; /**< COMMAND_KIND: 1=도어; 2=공조; 3=조명 */
    NetVehicleResult result; /**< RESULT: 공통 차량 Result 코드 */
    NetReason result_reason; /**< RESULT_REASON: 공통 사유 코드 */
    NetDoorEvent door_event; /**< DOOR_EVENT: 0=없음; 1=LOCK_COMPLETED; 2=UNLOCK_COMPLETED; 3=ALREADY_AT_TARGET; 4=REQUEST_REJECTED; 5=REQUEST_FAILED */
    uint32_t occurrence_id; /**< OCCURRENCE_ID: 도어 이벤트 번호; 없으면 0 */
    uint16_t event_age_ms; /**< EVENT_AGE: 이벤트 발생 후 경과 시간, ms */
    NetValueQuality event_quality; /**< EVENT_QUALITY: VALUE_QUALITY */
    NetDoorTarget door_target; /**< DOOR_TARGET: 0=LOCK; 1=UNLOCK; 255=비도어 */
} NetBcmResult;
```

**로컬 사용:** [`BcmNet_SendResult()`](20_API.md#api-bcmnet_sendresult). 원 명령의 실제 처리 결과이며 local OK로 DONE을 만들지 않는다. 이 선언에는 C2 `confirmation` 필드가 없다. 원 DONE을 보존하되 실제 확인 근거 없는 CONFIRMED는 만들지 않는 [BCM_RESULT 권장안](../../03_COMMUNICATION/10_CAN_FD.md#message-bcm-result)은 PROPOSED/BINDING_OPEN이다.

| 멤버 | 정확한 C 타입 | 값·단위·범위 / 역할 | 원 소유자·유효 조건 |
|---|---|---|---|
| `command_domain_boot` | `uint32_t` | 원 명령을 생성한 Domain 기동 번호 | BCM이 받은 원 DOMAIN Boot/명령 ID를 복사. 새 발급 금지; BCM CAN Boot와 별개. |
| `command_id` | `uint32_t` | 처리한 Domain 명령 ID | BCM이 받은 원 DOMAIN Boot/명령 ID를 복사. 새 발급 금지; BCM CAN Boot와 별개. |
| `command_kind` | `NetBcmCommandKind` | 1=도어; 2=공조; 3=조명 | BCM 실행 담당. 실제 처리 단계/사유이며 Network OK로 DONE 합성 금지. |
| `result` | `NetVehicleResult` | 공통 차량 Result 코드 | BCM 실행 담당. 실제 처리 단계/사유이며 Network OK로 DONE 합성 금지. |
| `result_reason` | `NetReason` | 공통 사유 코드 | BCM 실행 담당. 실제 처리 단계/사유이며 Network OK로 DONE 합성 금지. |
| `door_event` | `NetDoorEvent` | 0=없음; 1=LOCK_COMPLETED; 2=UNLOCK_COMPLETED; 3=ALREADY_AT_TARGET; 4=REQUEST_REJECTED; 5=REQUEST_FAILED | BCM 도어 실행 근거. 이벤트 없음/비도어이면 optional evidence를 소비하지 않음; 이벤트 시 occurrence와 Age/품질 보존. |
| `occurrence_id` | `uint32_t` | 도어 이벤트 번호; 없으면 0 | BCM 도어 실행 근거. 이벤트 없음/비도어이면 optional evidence를 소비하지 않음; 이벤트 시 occurrence와 Age/품질 보존. |
| `event_age_ms` | `uint16_t` | 이벤트 발생 후 경과 시간, ms | BCM 도어 실행 근거. 이벤트 없음/비도어이면 optional evidence를 소비하지 않음; 이벤트 시 occurrence와 Age/품질 보존. |
| `event_quality` | `NetValueQuality` | VALUE_QUALITY | BCM 도어 실행 근거. 이벤트 없음/비도어이면 optional evidence를 소비하지 않음; 이벤트 시 occurrence와 Age/품질 보존. |
| `door_target` | `NetDoorTarget` | 0=LOCK; 1=UNLOCK; 255=비도어 | BCM 도어 실행 근거. 이벤트 없음/비도어이면 optional evidence를 소비하지 않음; 이벤트 시 occurrence와 Age/품질 보존. |
<a id="type-netbcmevent"></a>
## `NetBcmEvent`

BCM 실제 관측·실행·진단 → DOMAIN · [`BCM_EVENT`](../../03_COMMUNICATION/10_CAN_FD.md#message-bcm-event)

```c
typedef struct NetBcmEvent
{
    NetBcmEventKind event_kind; /**< EVENT_KIND: 1=OVERHEAT_DETECTED; 2=FAN_MISMATCH_DETECTED; 3=RECOVERY_CONFIRMED */
    uint32_t occurrence_id; /**< OCCURRENCE_ID: 이벤트 식별 */
    uint16_t event_age_ms; /**< EVENT_AGE: 발생 후 경과 시간, ms */
    NetValueQuality event_quality; /**< EVENT_QUALITY: VALUE_QUALITY */
    uint32_t related_domain_boot; /**< RELATED_DOMAIN_BOOT: 연결 명령의 Domain 기동 번호; 없으면 0 */
    uint32_t related_command_id; /**< RELATED_COMMAND_ID: 연결된 명령, 없으면 0 */
    uint8_t affected_function; /**< AFFECTED_FUNCTION: BCM 기능 비트 */
} NetBcmEvent;
```

**로컬 사용:** [`BcmNet_SendEvent()`](20_API.md#api-bcmnet_sendevent) · 전체 타입/인자·호출자·수명·실패는 API에서 확인한다.

| 멤버 | 정확한 C 타입 | 값·단위·범위 / 역할 | 원 소유자·유효 조건 |
|---|---|---|---|
| `event_kind` | `NetBcmEventKind` | 1=OVERHEAT_DETECTED; 2=FAN_MISMATCH_DETECTED; 3=RECOVERY_CONFIRMED | BCM 보호/복구 평가. 같은 발생의 반복은 occurrence 유지, 새 실제 발생에서만 새 ID. |
| `occurrence_id` | `uint32_t` | 이벤트 식별 | BCM 보호/복구 평가. 같은 발생의 반복은 occurrence 유지, 새 실제 발생에서만 새 ID. |
| `event_age_ms` | `uint16_t` | 발생 후 경과 시간, ms | BCM 보호/복구 평가. 같은 발생의 반복은 occurrence 유지, 새 실제 발생에서만 새 ID. |
| `event_quality` | `NetValueQuality` | VALUE_QUALITY | BCM 보호/복구 평가. 같은 발생의 반복은 occurrence 유지, 새 실제 발생에서만 새 ID. |
| `related_domain_boot` | `uint32_t` | 연결 명령의 Domain 기동 번호; 없으면 0 | BCM이 보관한 원 DOMAIN 명령 참조. 독립 로컬 발생에 연결 명령 없으면 0; 임의 관련 명령 생성 금지. |
| `related_command_id` | `uint32_t` | 연결된 명령, 없으면 0 | BCM이 보관한 원 DOMAIN 명령 참조. 독립 로컬 발생에 연결 명령 없으면 0; 임의 관련 명령 생성 금지. |
| `affected_function` | `uint8_t` | BCM 기능 비트 | BCM 보호/복구 평가. 같은 발생의 반복은 occurrence 유지, 새 실제 발생에서만 새 ID. |
<a id="type-netbcmstatus"></a>
## `NetBcmStatus`

BCM 실제 관측·실행·진단 → DOMAIN · [`BCM_STATUS`](../../03_COMMUNICATION/10_CAN_FD.md#message-bcm-status)

```c
typedef struct NetBcmStatus
{
    NetControllerState bcm_ecu_state; /**< BCM_ECU_STATE: 0=INIT; 1=READY; 2=DEGRADED; 3=FAULT */
    uint16_t fault_mask; /**< FAULT_MASK: BCM 고장 비트 0~7 */
    NetFaultCategory active_fault_category; /**< ACTIVE_FAULT_CATEGORY: 0=없음; 1=SENSOR; 2=FUNCTION; 3=COMM */
    uint16_t active_fault_code; /**< ACTIVE_FAULT_CODE: 현재 주요 고장; bit번호+1 */
    uint8_t affected_function; /**< AFFECTED_FUNCTION: bit0=도어; bit1=공조; bit2=조명 */
    NetFlag recovering; /**< RECOVERING: 0=아님; 1=복구 중 */
    uint32_t update_sequence; /**< UPDATE_SEQUENCE: 새 상태 평가 */
    uint16_t source_age_ms; /**< SOURCE_AGE: 평가 후 경과 시간, ms */
} NetBcmStatus;
```

**로컬 사용:** [`BcmNet_PublishStatus()`](20_API.md#api-bcmnet_publishstatus) · 전체 타입/인자·호출자·수명·실패는 API에서 확인한다.

| 멤버 | 정확한 C 타입 | 값·단위·범위 / 역할 | 원 소유자·유효 조건 |
|---|---|---|---|
| `bcm_ecu_state` | `NetControllerState` | 0=INIT; 1=READY; 2=DEGRADED; 3=FAULT | BCM 해당 실행/관측 담당. 연관 quality가 OK일 때 현재 제어 근거; UNKNOWN/INVALID를 정상값으로 승격하지 않음. |
| `fault_mask` | `uint16_t` | BCM 고장 비트 0~7 | BCM 기능 진단 + Network 제공 통신 근거. 현재 active 비트만; 없는 통신 근거를 App이 추정하지 않음. |
| `active_fault_category` | `NetFaultCategory` | 0=없음; 1=SENSOR; 2=FUNCTION; 3=COMM | BCM 해당 실행/관측 담당. 연관 quality가 OK일 때 현재 제어 근거; UNKNOWN/INVALID를 정상값으로 승격하지 않음. |
| `active_fault_code` | `uint16_t` | 현재 주요 고장; bit번호+1 | BCM 진단. 0이면 현재 고장 없음이며 category/영향 상세를 고장 근거로 사용하지 않음. |
| `affected_function` | `uint8_t` | bit0=도어; bit1=공조; bit2=조명 | BCM 해당 실행/관측 담당. 연관 quality가 OK일 때 현재 제어 근거; UNKNOWN/INVALID를 정상값으로 승격하지 않음. |
| `recovering` | `NetFlag` | 0=아님; 1=복구 중 | BCM 해당 실행/관측 담당. 연관 quality가 OK일 때 현재 제어 근거; UNKNOWN/INVALID를 정상값으로 승격하지 않음. |
| `update_sequence` | `uint32_t` | 새 상태 평가 | BCM 해당 상태의 새 의미 평가에서만 증가. 프레임 재전송 TX_SEQUENCE와 별개. |
| `source_age_ms` | `uint16_t` | 평가 후 경과 시간, ms | BCM 원 평가 후 ms; App 제출 시 현재 원 Age, Network 보관→송신 대기만 한 번 누적·65535 포화. |

<a id="type-netdoortarget"></a>
## `NetDoorTarget`

```c
typedef uint8_t NetDoorTarget;
enum NetDoorTargetCode
{
    NET_DOOR_TARGET_LOCK = 0,
    NET_DOOR_TARGET_UNLOCK = 1,
    NET_DOOR_TARGET_NOT_APPLICABLE = 255
};
```

<a id="fixed-width-codes"></a>
별칭은 위의 고정 폭 C 정수다. 이어지는 `enum ...Code`는 상수 집합으로, enum 메모리 크기가 별칭/Wire 폭이라는 뜻이 아니다. 아래 숫자는 **이 설계 후보의 의미 코드**이며 기존 C2 타입은 [명시 mapping](../../04_COMMON/30_ENUM_WIRE_MAPPING.md)을 거친다. 예약/지원하지 않는 코드는 형식 오류로 처리한다.

| 상수 | 제안 값 / 의미 |
|---|---|
| `NET_DOOR_TARGET_LOCK` | `0` · 잠금 목표 |
| `NET_DOOR_TARGET_UNLOCK` | `1` · 해제 목표 |
| `NET_DOOR_TARGET_NOT_APPLICABLE` | `255` · 해당 결과에 대상 없음 |

<a id="type-netdoorlockstate"></a>
## `NetDoorLockState`

```c
typedef uint8_t NetDoorLockState;
enum NetDoorLockStateCode
{
    NET_DOOR_LOCK_LOCKED = 0,
    NET_DOOR_LOCK_UNLOCKED = 1,
    NET_DOOR_LOCK_UNKNOWN = 255
};
```

코드 폭·예약값·C2 변환은 [고정 폭 코드 규칙](#fixed-width-codes)을 적용한다.

| 상수 | 제안 값 / 의미 |
|---|---|
| `NET_DOOR_LOCK_LOCKED` | `0` · 잠금 확인 |
| `NET_DOOR_LOCK_UNLOCKED` | `1` · 잠금 해제 확인 |
| `NET_DOOR_LOCK_UNKNOWN` | `255` · 확인 불가 |

<a id="type-netdooropenstate"></a>
## `NetDoorOpenState`

```c
typedef uint8_t NetDoorOpenState;
enum NetDoorOpenStateCode
{
    NET_DOOR_OPEN_CLOSED = 0,
    NET_DOOR_OPEN_OPEN = 1,
    NET_DOOR_OPEN_UNKNOWN = 255
};
```

코드 폭·예약값·C2 변환은 [고정 폭 코드 규칙](#fixed-width-codes)을 적용한다.

| 상수 | 제안 값 / 의미 |
|---|---|
| `NET_DOOR_OPEN_CLOSED` | `0` · 닫힘 확인 |
| `NET_DOOR_OPEN_OPEN` | `1` · 열림 확인 |
| `NET_DOOR_OPEN_UNKNOWN` | `255` · 확인 불가 |

<a id="type-netdoorcompositestate"></a>
## `NetDoorCompositeState`

```c
typedef uint8_t NetDoorCompositeState;
enum NetDoorCompositeStateCode
{
    NET_DOOR_COMPOSITE_NORMAL = 0,
    NET_DOOR_COMPOSITE_INCONSISTENT = 1,
    NET_DOOR_COMPOSITE_UNTRUSTED = 2
};
```

코드 폭·예약값·C2 변환은 [고정 폭 코드 규칙](#fixed-width-codes)을 적용한다.

| 상수 | 제안 값 / 의미 |
|---|---|
| `NET_DOOR_COMPOSITE_NORMAL` | `0` · 정상 조합/상태 |
| `NET_DOOR_COMPOSITE_INCONSISTENT` | `1` · 상호 불일치 |
| `NET_DOOR_COMPOSITE_UNTRUSTED` | `2` · 상태 신뢰 불가 |

<a id="type-netheatremovalstate"></a>
## `NetHeatRemovalState`

```c
typedef uint8_t NetHeatRemovalState;
enum NetHeatRemovalStateCode
{
    NET_HEAT_REMOVAL_NORMAL = 0,
    NET_HEAT_REMOVAL_OVERHEAT_CUTOFF = 1,
    NET_HEAT_REMOVAL_UNAVAILABLE = 255
};
```

코드 폭·예약값·C2 변환은 [고정 폭 코드 규칙](#fixed-width-codes)을 적용한다.

| 상수 | 제안 값 / 의미 |
|---|---|
| `NET_HEAT_REMOVAL_NORMAL` | `0` · 정상 조합/상태 |
| `NET_HEAT_REMOVAL_OVERHEAT_CUTOFF` | `1` · 과열에 따른 차단 |
| `NET_HEAT_REMOVAL_UNAVAILABLE` | `255` · 제공 불가 |

<a id="type-netlightapplyresult"></a>
## `NetLightApplyResult`

```c
typedef uint8_t NetLightApplyResult;
enum NetLightApplyResultCode
{
    NET_LIGHT_APPLY_APPLIED = 0,
    NET_LIGHT_APPLY_FAILED = 1,
    NET_LIGHT_APPLY_UNKNOWN = 255
};
```

코드 폭·예약값·C2 변환은 [고정 폭 코드 규칙](#fixed-width-codes)을 적용한다.

| 상수 | 제안 값 / 의미 |
|---|---|
| `NET_LIGHT_APPLY_APPLIED` | `0` · 출력 지시 적용 |
| `NET_LIGHT_APPLY_FAILED` | `1` · 실패 |
| `NET_LIGHT_APPLY_UNKNOWN` | `255` · 확인 불가 |

<a id="type-netbcmcommandkind"></a>
## `NetBcmCommandKind`

```c
typedef uint8_t NetBcmCommandKind;
enum NetBcmCommandKindCode
{
    NET_BCM_COMMAND_DOOR = 1,
    NET_BCM_COMMAND_CLIMATE = 2,
    NET_BCM_COMMAND_LIGHT = 3
};
```

코드 폭·예약값·C2 변환은 [고정 폭 코드 규칙](#fixed-width-codes)을 적용한다.

| 상수 | 제안 값 / 의미 |
|---|---|
| `NET_BCM_COMMAND_DOOR` | `1` · 도어 명령 |
| `NET_BCM_COMMAND_CLIMATE` | `2` · 공조 명령 |
| `NET_BCM_COMMAND_LIGHT` | `3` · 실내 조명 명령 |

<a id="type-netdoorevent"></a>
## `NetDoorEvent`

```c
typedef uint8_t NetDoorEvent;
enum NetDoorEventCode
{
    NET_DOOR_EVENT_NONE = 0,
    NET_DOOR_EVENT_LOCK_COMPLETED = 1,
    NET_DOOR_EVENT_UNLOCK_COMPLETED = 2,
    NET_DOOR_EVENT_ALREADY_AT_TARGET = 3,
    NET_DOOR_EVENT_REQUEST_REJECTED = 4,
    NET_DOOR_EVENT_REQUEST_FAILED = 5
};
```

코드 폭·예약값·C2 변환은 [고정 폭 코드 규칙](#fixed-width-codes)을 적용한다.

| 상수 | 제안 값 / 의미 |
|---|---|
| `NET_DOOR_EVENT_NONE` | `0` · 없음 |
| `NET_DOOR_EVENT_LOCK_COMPLETED` | `1` · 목표 잠금 실제 확인 |
| `NET_DOOR_EVENT_UNLOCK_COMPLETED` | `2` · 목표 해제 실제 확인 |
| `NET_DOOR_EVENT_ALREADY_AT_TARGET` | `3` · 추가 구동 없이 목표 상태 |
| `NET_DOOR_EVENT_REQUEST_REJECTED` | `4` · 실행 전 거부 |
| `NET_DOOR_EVENT_REQUEST_FAILED` | `5` · 실행/확인 실패 |

<a id="type-netbcmeventkind"></a>
## `NetBcmEventKind`

```c
typedef uint8_t NetBcmEventKind;
enum NetBcmEventKindCode
{
    NET_BCM_EVENT_OVERHEAT_DETECTED = 1,
    NET_BCM_EVENT_FAN_MISMATCH_DETECTED = 2,
    NET_BCM_EVENT_RECOVERY_CONFIRMED = 3
};
```

코드 폭·예약값·C2 변환은 [고정 폭 코드 규칙](#fixed-width-codes)을 적용한다.

| 상수 | 제안 값 / 의미 |
|---|---|
| `NET_BCM_EVENT_OVERHEAT_DETECTED` | `1` · 과열 보호 발생 |
| `NET_BCM_EVENT_FAN_MISMATCH_DETECTED` | `2` · 지시/측정 불일치 확정 |
| `NET_BCM_EVENT_RECOVERY_CONFIRMED` | `3` · 복구 조건 확인(자동 재구동 아님) |

<a id="type-netbcmfaultmask"></a>
## `NetBcmFaultMask`

```c
enum NetBcmFaultMask
{
    NET_BCM_FAULT_DOOR_SENSOR_FAULT = (1u << 0),
    NET_BCM_FAULT_FAN_SENSOR_FAULT = (1u << 1),
    NET_BCM_FAULT_TEMPERATURE_SENSOR_FAULT = (1u << 2),
    NET_BCM_FAULT_OVERHEAT_FAULT = (1u << 3),
    NET_BCM_FAULT_FAN_FAULT = (1u << 4),
    NET_BCM_FAULT_LOCK_ACTUATOR_FAULT = (1u << 5),
    NET_BCM_FAULT_LIGHT_APPLY_FAULT = (1u << 6),
    NET_BCM_FAULT_COMM_TIMEOUT_FAULT = (1u << 7)
};
```

현재 고장 비트는 동시 active를, 주요 고장 코드는 단일 대표를 표현한다. 주요 고장 선택 우선순위는 기능 owner의 OPEN이며 Network가 임의 우선순위를 만들지 않는다.

| 상수 | 제안 값 / 의미 |
|---|---|
| `NET_BCM_FAULT_DOOR_SENSOR_FAULT` | `(1u << 0)` · 도어 센서 오류 |
| `NET_BCM_FAULT_FAN_SENSOR_FAULT` | `(1u << 1)` · Fan 센서 오류 |
| `NET_BCM_FAULT_TEMPERATURE_SENSOR_FAULT` | `(1u << 2)` · 온도 센서 오류 |
| `NET_BCM_FAULT_OVERHEAT_FAULT` | `(1u << 3)` · 과열 기능 차단 |
| `NET_BCM_FAULT_FAN_FAULT` | `(1u << 4)` · Fan 기능 오류 |
| `NET_BCM_FAULT_LOCK_ACTUATOR_FAULT` | `(1u << 5)` · 잠금 구동/목표 확인 오류 |
| `NET_BCM_FAULT_LIGHT_APPLY_FAULT` | `(1u << 6)` · 조명 지시 적용 오류 |
| `NET_BCM_FAULT_COMM_TIMEOUT_FAULT` | `(1u << 7)` · 감시 대상 통신 상실 |

<a id="type-netbcmfaultcode"></a>
## `NetBcmFaultCode`

```c
enum NetBcmFaultCode
{
    NET_BCM_FAULT_CODE_NONE = 0,
    NET_BCM_FAULT_CODE_DOOR_SENSOR_FAULT = 1,
    NET_BCM_FAULT_CODE_FAN_SENSOR_FAULT = 2,
    NET_BCM_FAULT_CODE_TEMPERATURE_SENSOR_FAULT = 3,
    NET_BCM_FAULT_CODE_OVERHEAT_FAULT = 4,
    NET_BCM_FAULT_CODE_FAN_FAULT = 5,
    NET_BCM_FAULT_CODE_LOCK_ACTUATOR_FAULT = 6,
    NET_BCM_FAULT_CODE_LIGHT_APPLY_FAULT = 7,
    NET_BCM_FAULT_CODE_COMM_TIMEOUT_FAULT = 8
};
```

현재 고장 비트는 동시 active를, 주요 고장 코드는 단일 대표를 표현한다. 주요 고장 선택 우선순위는 기능 owner의 OPEN이며 Network가 임의 우선순위를 만들지 않는다.

| 상수 | 제안 값 / 의미 |
|---|---|
| `NET_BCM_FAULT_CODE_NONE` | `0` · 없음 |
| `NET_BCM_FAULT_CODE_DOOR_SENSOR_FAULT` | `1` · 도어 센서 오류 |
| `NET_BCM_FAULT_CODE_FAN_SENSOR_FAULT` | `2` · Fan 센서 오류 |
| `NET_BCM_FAULT_CODE_TEMPERATURE_SENSOR_FAULT` | `3` · 온도 센서 오류 |
| `NET_BCM_FAULT_CODE_OVERHEAT_FAULT` | `4` · 과열 기능 차단 |
| `NET_BCM_FAULT_CODE_FAN_FAULT` | `5` · Fan 기능 오류 |
| `NET_BCM_FAULT_CODE_LOCK_ACTUATOR_FAULT` | `6` · 잠금 구동/목표 확인 오류 |
| `NET_BCM_FAULT_CODE_LIGHT_APPLY_FAULT` | `7` · 조명 지시 적용 오류 |
| `NET_BCM_FAULT_CODE_COMM_TIMEOUT_FAULT` | `8` · 감시 대상 통신 상실 |

## 송신 입력과 Network 보완

BCM App은 위 `NetBcm*State/Result/Event/Status`의 의미 필드를 준비한다. 모든 CAN `BOOT_ID/TX_SEQUENCE`, DLC, padding, endian, CRC/프레이밍은 Network/Codec 책임이며 이 타입에 App 초기화 의무로 추가하지 않는다. `update_sequence`·`occurrence_id`는 실제 평가/발생 담당이 생산한다. 같은 자료 반복은 원 sequence/occurrence 유지, TX_SEQUENCE는 송신 프레임마다 증가한다.

BCM_RESULT는 원 명령별 처리 결과와 선택적 도어 근거를 **같이** 인계한다. 도어 결과 근거를 BCM_EVENT로 중복 송신하지 않는다. BCM_EVENT는 과열/불일치/복구의 새 로컬 발생이며 현재 BCM_STATUS와 다른 수명이다. 현재 고장 해제가 과거 발생을 삭제하거나 이전 출력 자동 재개를 허용하지 않는다.

상태 `quality=STALE/INVALID/NO_DATA`인 자료도 정상 형식으로 송신할 수 있다. 숫자 상태값만 보고 정상으로 해석하지 않는다. `event_age_ms`는 발생 Age이며 현재 상태 `source_age_ms`와 서로 대체할 수 없다. CAN→DOMAIN→MOBILE 중계에서는 원 BCM Boot/sequence와 의미 Age를 보존한다.

## 구현 전 결정

- 명령 수용 상한 200ms 후보↔구 SysRS 1초: DOMAIN/BCM/Network 공동 결정 필요. 확정 전 서로 다른 수용 규칙으로 통합하지 않는다.
- BCM Network health 근거를 BCM 진단 owner에 제공하는 실제 기존 getter/sink와 고장 우선순위: BINDING_OPEN. 링크 장애를 로컬 실행 DONE/FAILED로 임의 변환하지 않는다.
- 상태/결과/이벤트의 실제 Port·실행 문맥·재시도/수용 후 실패 관측: BINDING_OPEN. 실제 SDK/Firmware/Codec/Board 시험 NOT_EXECUTED.
