# BCM·DOMAIN_PERMISSION — 자료 정본

범위는 BCM 9 Wire와 DOMAIN_PERMISSION 1 Wire의 Net DTO, BCM 기능 입력 및 Permission 송신 입력이다. 아래 선언은 S4A의 `DECLARED_SOURCE` 또는 `PROPOSED_DESIGN/BINDING_OPEN` 자료를 기반으로 한다. ClimateState_t·InteriorLightState_t 두 선언은 선택한 Vehicle v0.3 후보의 sourceGeneration을 반영한다. 메모리 ABI·헤더 생성·팀 채택을 뜻하지 않는다. Wire offset·ID·DLC는 원 후보 `WIRE_PROPOSED`이며 메시지별 Codec에서 분리한다.

[BCM·Permission 흐름](../flows/CAN_BCM_AND_PERMISSION.md) · [공통 자료](CAN_COMMON_DATA.md) · [기존 CIS_REAR 자료](CIS_REAR_DATA.md)

기존 B11의 20종 선언은 CIS_REAR_DATA의 정본을 사용한다. NetCanSource/NetCanRxMeta는 송신 기동·프레임 순서와 수신 로컬 시각이며 원 관측/기능 사건 번호와 서로 바꾸지 않는다. NetSubmitResult 및 DomainIf_Status_t의 의미·수명은 기존 정본을 따른다. NetCommandMeta, NetOrigin/Code, NetTriState/Code, NetFlag/Code, NetValueQuality/Code, NetControllerState/Code, NetFaultCategory/Code, NetReason/Code, NetVehicleResult/Code, DomainIf_CommandContext_t와 그 RequestContext 의존성, CommonEcuState_t/FaultInfo_t/ResultReason_t/RequestResult_t/ResultConfirmation_t는 공통 자료 정본에만 선언한다.

원 자료의 생산자·단위 표는 아래에서 각 선언과 함께 보존한다. 명시되지 않은 크기·유효기간·동기화 값은 추정하지 않는다. typedef enum 메모리 폭은 Wire u8/u16의 폭과 다르며 숫자 cast로 연결하지 않는다.


<a id="type-NetBcmClimateCommand"></a>
## `NetBcmClimateCommand`

출처: `S4A data/T06.md#type-NetBcmClimateCommand`, T06 L64..70.

```c
typedef struct NetBcmClimateCommand
{
    NetCommandMeta command; /**< 원본 명령 공통부의 의미 필드. CAN 송신 헤더는 C2 `commandId`를 보존하여 Network가 보완한다. */
    NetFanLevel fan_target_level; /**< FAN_TARGET_LEVEL: FAN 코드; UNKNOWN 금지 */
    NetThermalDirection thermal_direction; /**< THERMAL_DIRECTION: 0=IDLE; 1=COOL; 2=HEAT */
    uint8_t thermal_target_percent; /**< THERMAL_TARGET_LEVEL: 0~100%, 1%/raw */
} NetBcmClimateCommand;
```

| 필드 | C 타입·폭 | 값·단위·유효 조건 | 생성·관리 주체 |
|---|---|---|---|
| `command` | [`NetCommandMeta`](CAN_COMMON_DATA.md#type-NetCommandMeta); 중첩 타입 참조; sizeof 미검증 | 원본 명령 공통부의 의미 필드. CAN 송신 헤더는 C2 `commandId`를 보존하여 Network가 보완한다. | 원 ID·목표: DOMAIN 기능; 대상 Boot·전송 Age: DOMAIN Network. BCM은 검증한 수신 문맥만 소비. |
| `fan_target_level` | [`NetFanLevel`](CAN_BCM_DATA.md#type-NetFanLevel); 8비트 부호 없음 | FAN 코드; UNKNOWN 금지 | DOMAIN 기능 결정. BCM은 로컬 보호·가용성을 별도 판단하며 Network 접수로 실행을 확정하지 않음. |
| `thermal_direction` | [`NetThermalDirection`](CAN_BCM_DATA.md#type-NetThermalDirection); 8비트 부호 없음 | 0=IDLE; 1=COOL; 2=HEAT | DOMAIN 기능 결정. BCM은 로컬 보호·가용성을 별도 판단하며 Network 접수로 실행을 확정하지 않음. |
| `thermal_target_percent` | `uint8_t`; 8비트 부호 없음 | 0~100%, 1%/raw | DOMAIN 기능 결정. BCM은 로컬 보호·가용성을 별도 판단하며 Network 접수로 실행을 확정하지 않음. |


<a id="type-NetBcmClimateState"></a>
## `NetBcmClimateState`

출처: `S4A data/T06.md#type-NetBcmClimateState`, T06 L147..161.

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

| 필드 | C 타입·폭 | 값·단위·유효 조건 | 생성·관리 주체 |
|---|---|---|---|
| `fan_command_level` | [`NetFanLevel`](CAN_BCM_DATA.md#type-NetFanLevel); 8비트 부호 없음 | 0=OFF; 1=LOW; 2=MEDIUM; 3=HIGH | BCM 해당 실행/관측 담당. 연관 quality가 OK일 때 현재 제어 근거; UNKNOWN/INVALID를 정상값으로 승격하지 않음. |
| `fan_measured_level` | [`NetFanLevel`](CAN_BCM_DATA.md#type-NetFanLevel); 8비트 부호 없음 | FAN 코드 및 255=UNKNOWN | BCM 해당 실행/관측 담당. 연관 quality가 OK일 때 현재 제어 근거; UNKNOWN/INVALID를 정상값으로 승격하지 않음. |
| `thermal_direction_state` | [`NetThermalDirection`](CAN_BCM_DATA.md#type-NetThermalDirection); 8비트 부호 없음 | 0=IDLE; 1=COOL; 2=HEAT | BCM 해당 실행/관측 담당. 연관 quality가 OK일 때 현재 제어 근거; UNKNOWN/INVALID를 정상값으로 승격하지 않음. |
| `thermal_output_percent` | `uint8_t`; 8비트 부호 없음 | 0~100%, 1%/raw | BCM 해당 실행/관측 담당. 연관 quality가 OK일 때 현재 제어 근거; UNKNOWN/INVALID를 정상값으로 승격하지 않음. |
| `heat_removal_state` | [`NetHeatRemovalState`](CAN_BCM_DATA.md#type-NetHeatRemovalState); 8비트 부호 없음 | 0=정상; 1=과열 차단; 255=측정 불가 | BCM 해당 실행/관측 담당. 연관 quality가 OK일 때 현재 제어 근거; UNKNOWN/INVALID를 정상값으로 승격하지 않음. |
| `fan_command_quality` | [`NetValueQuality`](CAN_COMMON_DATA.md#type-NetValueQuality); 8비트 부호 없음 | VALUE_QUALITY | BCM 해당 실행/관측 담당. 연관 quality가 OK일 때 현재 제어 근거; UNKNOWN/INVALID를 정상값으로 승격하지 않음. |
| `fan_measured_quality` | [`NetValueQuality`](CAN_COMMON_DATA.md#type-NetValueQuality); 8비트 부호 없음 | VALUE_QUALITY | BCM 해당 실행/관측 담당. 연관 quality가 OK일 때 현재 제어 근거; UNKNOWN/INVALID를 정상값으로 승격하지 않음. |
| `thermal_direction_quality` | [`NetValueQuality`](CAN_COMMON_DATA.md#type-NetValueQuality); 8비트 부호 없음 | VALUE_QUALITY | BCM 해당 실행/관측 담당. 연관 quality가 OK일 때 현재 제어 근거; UNKNOWN/INVALID를 정상값으로 승격하지 않음. |
| `thermal_output_quality` | [`NetValueQuality`](CAN_COMMON_DATA.md#type-NetValueQuality); 8비트 부호 없음 | VALUE_QUALITY | BCM 해당 실행/관측 담당. 연관 quality가 OK일 때 현재 제어 근거; UNKNOWN/INVALID를 정상값으로 승격하지 않음. |
| `heat_removal_quality` | [`NetValueQuality`](CAN_COMMON_DATA.md#type-NetValueQuality); 8비트 부호 없음 | VALUE_QUALITY | BCM 해당 실행/관측 담당. 연관 quality가 OK일 때 현재 제어 근거; UNKNOWN/INVALID를 정상값으로 승격하지 않음. |
| `update_sequence` | `uint32_t`; 32비트 부호 없음 | 이번 공조 상태 평가 번호 | BCM 해당 상태의 새 의미 평가에서만 증가. 프레임 재전송 TX_SEQUENCE와 별개. |
| `source_age_ms` | `uint16_t`; 16비트 부호 없음 | 평가 후 경과 시간, ms | BCM 원본 평가 후 ms; App 제출 시 현재 원본 생성 후 경과 시간, Network 보관→송신 대기만 한 번 누적·65535 포화. |


<a id="type-NetBcmCommandKind"></a>
## `NetBcmCommandKind`

출처: `S4A data/T06.md#type-NetBcmCommandKind`, T06 L446..446.

```c
typedef uint8_t NetBcmCommandKind;
```



<a id="type-NetBcmCommandKindCode"></a>
## `NetBcmCommandKindCode`

출처: `S4A data/T06.md#type-NetBcmCommandKindCode`, T06 L447..452.

```c
enum NetBcmCommandKindCode
{
    NET_BCM_COMMAND_DOOR = 1,
    NET_BCM_COMMAND_CLIMATE = 2,
    NET_BCM_COMMAND_LIGHT = 3
};
```

| 상수 | 값·의미 |
|---|---|
| `NET_BCM_COMMAND_DOOR` | `1` · 도어 명령 |
| `NET_BCM_COMMAND_CLIMATE` | `2` · 공조 명령 |
| `NET_BCM_COMMAND_LIGHT` | `3` · 실내 조명 명령 |


<a id="type-NetBcmDoorCommand"></a>
## `NetBcmDoorCommand`

출처: `S4A data/T06.md#type-NetBcmDoorCommand`, T06 L40..45.

```c
typedef struct NetBcmDoorCommand
{
    NetCommandMeta command; /**< 원본 명령 공통부의 의미 필드. CAN 송신 헤더는 C2 `commandId`를 보존하여 Network가 보완한다. */
    NetDoorTarget door_lock_target; /**< DOOR_LOCK_TARGET: 0=LOCK; 1=UNLOCK */
    NetOrigin origin; /**< ORIGIN: 요청 출처 코드 */
} NetBcmDoorCommand;
```

| 필드 | C 타입·폭 | 값·단위·유효 조건 | 생성·관리 주체 |
|---|---|---|---|
| `command` | [`NetCommandMeta`](CAN_COMMON_DATA.md#type-NetCommandMeta); 중첩 타입 참조; sizeof 미검증 | 원본 명령 공통부의 의미 필드. CAN 송신 헤더는 C2 `commandId`를 보존하여 Network가 보완한다. | 원 ID·목표: DOMAIN 기능; 대상 Boot·전송 Age: DOMAIN Network. BCM은 검증한 수신 문맥만 소비. |
| `door_lock_target` | [`NetDoorTarget`](CAN_BCM_DATA.md#type-NetDoorTarget); 8비트 부호 없음 | 0=LOCK; 1=UNLOCK | DOMAIN 기능 결정. BCM은 로컬 보호·가용성을 별도 판단하며 Network 접수로 실행을 확정하지 않음. |
| `origin` | [`NetOrigin`](CAN_COMMON_DATA.md#type-NetOrigin); 8비트 부호 없음 | 요청 출처 코드 | DOMAIN 기능 결정. BCM은 로컬 보호·가용성을 별도 판단하며 Network 접수로 실행을 확정하지 않음. |


<a id="type-NetBcmDoorState"></a>
## `NetBcmDoorState`

출처: `S4A data/T06.md#type-NetBcmDoorState`, T06 L116..126.

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

| 필드 | C 타입·폭 | 값·단위·유효 조건 | 생성·관리 주체 |
|---|---|---|---|
| `door_lock_state` | [`NetDoorLockState`](CAN_BCM_DATA.md#type-NetDoorLockState); 8비트 부호 없음 | 0=LOCKED; 1=UNLOCKED; 255=UNKNOWN | BCM 해당 실행/관측 담당. 연관 quality가 OK일 때 현재 제어 근거; UNKNOWN/INVALID를 정상값으로 승격하지 않음. |
| `door_open_state` | [`NetDoorOpenState`](CAN_BCM_DATA.md#type-NetDoorOpenState); 8비트 부호 없음 | 0=CLOSED; 1=OPEN; 255=UNKNOWN | BCM 해당 실행/관측 담당. 연관 quality가 OK일 때 현재 제어 근거; UNKNOWN/INVALID를 정상값으로 승격하지 않음. |
| `door_composite_state` | [`NetDoorCompositeState`](CAN_BCM_DATA.md#type-NetDoorCompositeState); 8비트 부호 없음 | 0=NORMAL; 1=INCONSISTENT; 2=UNTRUSTED | BCM 해당 실행/관측 담당. 연관 quality가 OK일 때 현재 제어 근거; UNKNOWN/INVALID를 정상값으로 승격하지 않음. |
| `lock_quality` | [`NetValueQuality`](CAN_COMMON_DATA.md#type-NetValueQuality); 8비트 부호 없음 | VALUE_QUALITY | BCM 해당 실행/관측 담당. 연관 quality가 OK일 때 현재 제어 근거; UNKNOWN/INVALID를 정상값으로 승격하지 않음. |
| `open_quality` | [`NetValueQuality`](CAN_COMMON_DATA.md#type-NetValueQuality); 8비트 부호 없음 | VALUE_QUALITY | BCM 해당 실행/관측 담당. 연관 quality가 OK일 때 현재 제어 근거; UNKNOWN/INVALID를 정상값으로 승격하지 않음. |
| `composite_quality` | [`NetValueQuality`](CAN_COMMON_DATA.md#type-NetValueQuality); 8비트 부호 없음 | VALUE_QUALITY | BCM 해당 실행/관측 담당. 연관 quality가 OK일 때 현재 제어 근거; UNKNOWN/INVALID를 정상값으로 승격하지 않음. |
| `update_sequence` | `uint32_t`; 32비트 부호 없음 | 이번 도어 상태 평가 번호 | BCM 해당 상태의 새 의미 평가에서만 증가. 프레임 재전송 TX_SEQUENCE와 별개. |
| `source_age_ms` | `uint16_t`; 16비트 부호 없음 | 평가 후 경과 시간, ms | BCM 원본 평가 후 ms; App 제출 시 현재 원본 생성 후 경과 시간, Network 보관→송신 대기만 한 번 누적·65535 포화. |


<a id="type-NetBcmEvent"></a>
## `NetBcmEvent`

출처: `S4A data/T06.md#type-NetBcmEvent`, T06 L260..269.

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

| 필드 | C 타입·폭 | 값·단위·유효 조건 | 생성·관리 주체 |
|---|---|---|---|
| `event_kind` | [`NetBcmEventKind`](CAN_BCM_DATA.md#type-NetBcmEventKind); 8비트 부호 없음 | 1=OVERHEAT_DETECTED; 2=FAN_MISMATCH_DETECTED; 3=RECOVERY_CONFIRMED | BCM 보호/복구 평가. 같은 발생의 반복은 occurrence 유지, 새 실제 발생에서만 새 ID. |
| `occurrence_id` | `uint32_t`; 32비트 부호 없음 | 이벤트 식별 | BCM 보호/복구 평가. 같은 발생의 반복은 occurrence 유지, 새 실제 발생에서만 새 ID. |
| `event_age_ms` | `uint16_t`; 16비트 부호 없음 | 발생 후 경과 시간, ms | BCM 보호/복구 평가. 같은 발생의 반복은 occurrence 유지, 새 실제 발생에서만 새 ID. |
| `event_quality` | [`NetValueQuality`](CAN_COMMON_DATA.md#type-NetValueQuality); 8비트 부호 없음 | VALUE_QUALITY | BCM 보호/복구 평가. 같은 발생의 반복은 occurrence 유지, 새 실제 발생에서만 새 ID. |
| `related_domain_boot` | `uint32_t`; 32비트 부호 없음 | 연결 명령의 Domain 기동 번호; 없으면 0 | BCM이 보관한 원 DOMAIN 명령 참조. 독립 로컬 발생에 연결 명령 없으면 0; 임의 관련 명령 생성 금지. |
| `related_command_id` | `uint32_t`; 32비트 부호 없음 | 연결된 명령, 없으면 0 | BCM이 보관한 원 DOMAIN 명령 참조. 독립 로컬 발생에 연결 명령 없으면 0; 임의 관련 명령 생성 금지. |
| `affected_function` | `uint8_t`; 8비트 부호 없음 | BCM 기능 비트 | BCM 보호/복구 평가. 같은 발생의 반복은 occurrence 유지, 새 실제 발생에서만 새 ID. |


<a id="type-NetBcmEventKind"></a>
## `NetBcmEventKind`

출처: `S4A data/T06.md#type-NetBcmEventKind`, T06 L494..494.

```c
typedef uint8_t NetBcmEventKind;
```



<a id="type-NetBcmEventKindCode"></a>
## `NetBcmEventKindCode`

출처: `S4A data/T06.md#type-NetBcmEventKindCode`, T06 L495..500.

```c
enum NetBcmEventKindCode
{
    NET_BCM_EVENT_OVERHEAT_DETECTED = 1,
    NET_BCM_EVENT_FAN_MISMATCH_DETECTED = 2,
    NET_BCM_EVENT_RECOVERY_CONFIRMED = 3
};
```

| 상수 | 값·의미 |
|---|---|
| `NET_BCM_EVENT_OVERHEAT_DETECTED` | `1` · 과열 보호 발생 |
| `NET_BCM_EVENT_FAN_MISMATCH_DETECTED` | `2` · 지시/측정 불일치 확정 |
| `NET_BCM_EVENT_RECOVERY_CONFIRMED` | `3` · 복구 조건 확인(자동 재구동 아님) |


<a id="type-NetBcmFaultCode"></a>
## `NetBcmFaultCode`

출처: `S4A data/T06.md#type-NetBcmFaultCode`, T06 L545..556.

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

| 상수 | 값·의미 |
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


<a id="type-NetBcmFaultMask"></a>
## `NetBcmFaultMask`

출처: `S4A data/T06.md#type-NetBcmFaultMask`, T06 L515..525.

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

| 상수 | 값·의미 |
|---|---|
| `NET_BCM_FAULT_DOOR_SENSOR_FAULT` | `(1u << 0)` · 도어 센서 오류 |
| `NET_BCM_FAULT_FAN_SENSOR_FAULT` | `(1u << 1)` · Fan 센서 오류 |
| `NET_BCM_FAULT_TEMPERATURE_SENSOR_FAULT` | `(1u << 2)` · 온도 센서 오류 |
| `NET_BCM_FAULT_OVERHEAT_FAULT` | `(1u << 3)` · 과열 기능 차단 |
| `NET_BCM_FAULT_FAN_FAULT` | `(1u << 4)` · Fan 기능 오류 |
| `NET_BCM_FAULT_LOCK_ACTUATOR_FAULT` | `(1u << 5)` · 잠금 구동/목표 확인 오류 |
| `NET_BCM_FAULT_LIGHT_APPLY_FAULT` | `(1u << 6)` · 조명 지시 적용 오류 |
| `NET_BCM_FAULT_COMM_TIMEOUT_FAULT` | `(1u << 7)` · 감시 대상 통신 상실 |


<a id="type-NetBcmLightCommand"></a>
## `NetBcmLightCommand`

출처: `S4A data/T06.md#type-NetBcmLightCommand`, T06 L89..95.

```c
typedef struct NetBcmLightCommand
{
    NetCommandMeta command; /**< 원본 명령 공통부의 의미 필드. CAN 송신 헤더는 C2 `commandId`를 보존하여 Network가 보완한다. */
    NetLightType interior_light_type; /**< INTERIOR_LIGHT_TYPE: LIGHT_TYPE 코드 */
    uint8_t light_level_percent; /**< INTERIOR_LIGHT_LEVEL: 0~100%, 1%/raw */
    NetRgb interior_light_color; /**< INTERIOR_LIGHT_COLOR: R/G/B 각각 0~255 */
} NetBcmLightCommand;
```

| 필드 | C 타입·폭 | 값·단위·유효 조건 | 생성·관리 주체 |
|---|---|---|---|
| `command` | [`NetCommandMeta`](CAN_COMMON_DATA.md#type-NetCommandMeta); 중첩 타입 참조; sizeof 미검증 | 원본 명령 공통부의 의미 필드. CAN 송신 헤더는 C2 `commandId`를 보존하여 Network가 보완한다. | 원 ID·목표: DOMAIN 기능; 대상 Boot·전송 Age: DOMAIN Network. BCM은 검증한 수신 문맥만 소비. |
| `interior_light_type` | [`NetLightType`](CAN_BCM_DATA.md#type-NetLightType); 8비트 부호 없음 | LIGHT_TYPE 코드 | DOMAIN 기능 결정. BCM은 로컬 보호·가용성을 별도 판단하며 Network 접수로 실행을 확정하지 않음. |
| `light_level_percent` | `uint8_t`; 8비트 부호 없음 | 0~100%, 1%/raw | DOMAIN 기능 결정. BCM은 로컬 보호·가용성을 별도 판단하며 Network 접수로 실행을 확정하지 않음. |
| `interior_light_color` | [`NetRgb`](CAN_BCM_DATA.md#type-NetRgb); 중첩 타입 참조; sizeof 미검증 | R/G/B 각각 0~255 | DOMAIN 기능 결정. BCM은 로컬 보호·가용성을 별도 판단하며 Network 접수로 실행을 확정하지 않음. |


<a id="type-NetBcmLightState"></a>
## `NetBcmLightState`

출처: `S4A data/T06.md#type-NetBcmLightState`, T06 L186..199.

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

| 필드 | C 타입·폭 | 값·단위·유효 조건 | 생성·관리 주체 |
|---|---|---|---|
| `interior_light_type_state` | [`NetLightType`](CAN_BCM_DATA.md#type-NetLightType); 8비트 부호 없음 | LIGHT_TYPE 코드 | BCM 해당 실행/관측 담당. 연관 quality가 OK일 때 현재 제어 근거; UNKNOWN/INVALID를 정상값으로 승격하지 않음. |
| `light_level_percent` | `uint8_t`; 8비트 부호 없음 | 0~100% | BCM 해당 실행/관측 담당. 연관 quality가 OK일 때 현재 제어 근거; UNKNOWN/INVALID를 정상값으로 승격하지 않음. |
| `interior_light_color_state` | [`NetRgb`](CAN_BCM_DATA.md#type-NetRgb); 중첩 타입 참조; sizeof 미검증 | R/G/B 각각 0~255 | BCM 해당 실행/관측 담당. 연관 quality가 OK일 때 현재 제어 근거; UNKNOWN/INVALID를 정상값으로 승격하지 않음. |
| `interior_light_apply_result` | [`NetLightApplyResult`](CAN_BCM_DATA.md#type-NetLightApplyResult); 8비트 부호 없음 | 0=적용됨; 1=실패; 255=UNKNOWN | BCM 해당 실행/관측 담당. 연관 quality가 OK일 때 현재 제어 근거; UNKNOWN/INVALID를 정상값으로 승격하지 않음. |
| `light_physical_feedback_capability` | [`NetFlag`](CAN_COMMON_DATA.md#type-NetFlag); 8비트 부호 없음 | 0=미지원; 1=지원; 현재 0 | BCM 구성 담당자. 현재 물리 점등 확인 미지원(0); 적용 성공을 실제 점등 확인으로 표시하지 않음. |
| `light_type_quality` | [`NetValueQuality`](CAN_COMMON_DATA.md#type-NetValueQuality); 8비트 부호 없음 | VALUE_QUALITY | BCM 해당 실행/관측 담당. 연관 quality가 OK일 때 현재 제어 근거; UNKNOWN/INVALID를 정상값으로 승격하지 않음. |
| `light_level_quality` | [`NetValueQuality`](CAN_COMMON_DATA.md#type-NetValueQuality); 8비트 부호 없음 | VALUE_QUALITY | BCM 해당 실행/관측 담당. 연관 quality가 OK일 때 현재 제어 근거; UNKNOWN/INVALID를 정상값으로 승격하지 않음. |
| `light_color_quality` | [`NetValueQuality`](CAN_COMMON_DATA.md#type-NetValueQuality); 8비트 부호 없음 | VALUE_QUALITY | BCM 해당 실행/관측 담당. 연관 quality가 OK일 때 현재 제어 근거; UNKNOWN/INVALID를 정상값으로 승격하지 않음. |
| `light_apply_quality` | [`NetValueQuality`](CAN_COMMON_DATA.md#type-NetValueQuality); 8비트 부호 없음 | VALUE_QUALITY | BCM 해당 실행/관측 담당. 연관 quality가 OK일 때 현재 제어 근거; UNKNOWN/INVALID를 정상값으로 승격하지 않음. |
| `update_sequence` | `uint32_t`; 32비트 부호 없음 | 이번 조명 적용 상태 평가 번호 | BCM 해당 상태의 새 의미 평가에서만 증가. 프레임 재전송 TX_SEQUENCE와 별개. |
| `source_age_ms` | `uint16_t`; 16비트 부호 없음 | 평가 후 경과 시간, ms | BCM 원본 평가 후 ms; App 제출 시 현재 원본 생성 후 경과 시간, Network 보관→송신 대기만 한 번 누적·65535 포화. |


<a id="type-NetBcmResult"></a>
## `NetBcmResult`

출처: `S4A data/T06.md#type-NetBcmResult`, T06 L225..237.

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

| 필드 | C 타입·폭 | 값·단위·유효 조건 | 생성·관리 주체 |
|---|---|---|---|
| `command_domain_boot` | `uint32_t`; 32비트 부호 없음 | 원 명령을 생성한 Domain 기동 번호 | BCM이 받은 원 DOMAIN Boot/명령 ID를 복사. 새 발급 금지; BCM CAN Boot와 별개. |
| `command_id` | `uint32_t`; 32비트 부호 없음 | 처리한 Domain 명령 ID | BCM이 받은 원 DOMAIN Boot/명령 ID를 복사. 새 발급 금지; BCM CAN Boot와 별개. |
| `command_kind` | [`NetBcmCommandKind`](CAN_BCM_DATA.md#type-NetBcmCommandKind); 8비트 부호 없음 | 1=도어; 2=공조; 3=조명 | BCM 실행 담당. 실제 처리 단계/사유이며 Network OK로 DONE 합성 금지. |
| `result` | [`NetVehicleResult`](CAN_COMMON_DATA.md#type-NetVehicleResult); 8비트 부호 없음 | 공통 차량 Result 코드 | BCM 실행 담당. 실제 처리 단계/사유이며 Network OK로 DONE 합성 금지. |
| `result_reason` | [`NetReason`](CAN_COMMON_DATA.md#type-NetReason); 16비트 부호 없음 | 공통 사유 코드 | BCM 실행 담당. 실제 처리 단계/사유이며 Network OK로 DONE 합성 금지. |
| `door_event` | [`NetDoorEvent`](CAN_BCM_DATA.md#type-NetDoorEvent); 8비트 부호 없음 | 0=없음; 1=LOCK_COMPLETED; 2=UNLOCK_COMPLETED; 3=ALREADY_AT_TARGET; 4=REQUEST_REJECTED; 5=REQUEST_FAILED | BCM 도어 실행 근거. 이벤트 없음/비도어이면 optional evidence를 소비하지 않음; 이벤트 시 occurrence와 Age/품질 보존. |
| `occurrence_id` | `uint32_t`; 32비트 부호 없음 | 도어 이벤트 번호; 없으면 0 | BCM 도어 실행 근거. 이벤트 없음/비도어이면 optional evidence를 소비하지 않음; 이벤트 시 occurrence와 Age/품질 보존. |
| `event_age_ms` | `uint16_t`; 16비트 부호 없음 | 이벤트 발생 후 경과 시간, ms | BCM 도어 실행 근거. 이벤트 없음/비도어이면 optional evidence를 소비하지 않음; 이벤트 시 occurrence와 Age/품질 보존. |
| `event_quality` | [`NetValueQuality`](CAN_COMMON_DATA.md#type-NetValueQuality); 8비트 부호 없음 | VALUE_QUALITY | BCM 도어 실행 근거. 이벤트 없음/비도어이면 optional evidence를 소비하지 않음; 이벤트 시 occurrence와 Age/품질 보존. |
| `door_target` | [`NetDoorTarget`](CAN_BCM_DATA.md#type-NetDoorTarget); 8비트 부호 없음 | 0=LOCK; 1=UNLOCK; 255=비도어 | BCM 도어 실행 근거. 이벤트 없음/비도어이면 optional evidence를 소비하지 않음; 이벤트 시 occurrence와 Age/품질 보존. |


<a id="type-NetBcmStatus"></a>
## `NetBcmStatus`

출처: `S4A data/T06.md#type-NetBcmStatus`, T06 L289..299.

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

| 필드 | C 타입·폭 | 값·단위·유효 조건 | 생성·관리 주체 |
|---|---|---|---|
| `bcm_ecu_state` | [`NetControllerState`](CAN_COMMON_DATA.md#type-NetControllerState); 8비트 부호 없음 | 0=INIT; 1=READY; 2=DEGRADED; 3=FAULT | BCM 해당 실행·진단 Owner가 생성한다. 이 DTO/Wire에는 독립 quality 멤버가 없으며 C2 품질은 아래 투영 제약을 따른다. |
| `fault_mask` | `uint16_t`; 16비트 부호 없음 | BCM 고장 비트 0~7 | BCM 기능 진단 + Network 제공 통신 근거. 현재 active 비트만; 없는 통신 근거를 App이 추정하지 않음. |
| `active_fault_category` | [`NetFaultCategory`](CAN_COMMON_DATA.md#type-NetFaultCategory); 8비트 부호 없음 | 0=없음; 1=SENSOR; 2=FUNCTION; 3=COMM | BCM 해당 실행·진단 Owner가 생성한다. 이 DTO/Wire에는 독립 quality 멤버가 없으며 C2 품질은 아래 투영 제약을 따른다. |
| `active_fault_code` | `uint16_t`; 16비트 부호 없음 | 현재 주요 고장; bit번호+1 | BCM 진단. 0이면 현재 고장 없음이며 category/영향 상세를 고장 근거로 사용하지 않음. |
| `affected_function` | `uint8_t`; 8비트 부호 없음 | bit0=도어; bit1=공조; bit2=조명 | BCM 해당 실행·진단 Owner가 생성한다. 이 DTO/Wire에는 독립 quality 멤버가 없으며 C2 품질은 아래 투영 제약을 따른다. |
| `recovering` | [`NetFlag`](CAN_COMMON_DATA.md#type-NetFlag); 8비트 부호 없음 | 0=아님; 1=복구 중 | BCM 해당 실행·진단 Owner가 생성한다. 이 DTO/Wire에는 독립 quality 멤버가 없으며 C2 품질은 아래 투영 제약을 따른다. |
| `update_sequence` | `uint32_t`; 32비트 부호 없음 | 새 상태 평가 | BCM 해당 상태의 새 의미 평가에서만 증가. 프레임 재전송 TX_SEQUENCE와 별개. |
| `source_age_ms` | `uint16_t`; 16비트 부호 없음 | 평가 후 경과 시간, ms | BCM 원본 평가 후 ms; App 제출 시 현재 원본 생성 후 경과 시간, Network 보관→송신 대기만 한 번 누적·65535 포화. |

C2 품질 투영은 [BCM_STATUS 수신 경계](../functions/can/BCM_STATUS.md#rx)를 따른다. `active_fault_code==0`이면 고장 없음으로 처리하고 category/state/quality를 활성 고장 근거로 사용하지 않는다. `active_fault_code!=0`이어도 Wire에는 독립 고장 quality가 없으므로 확인된 원 생산자 근거·매핑 없이 `activeFault.quality`나 summary 품질을 채우지 않는다. sourceGeneration과 C2 요약 투영의 기존 HOLD도 유지한다.

<a id="type-NetDoorCompositeState"></a>
## `NetDoorCompositeState`

출처: `S4A data/T06.md#type-NetDoorCompositeState`, T06 L383..383.

```c
typedef uint8_t NetDoorCompositeState;
```



<a id="type-NetDoorCompositeStateCode"></a>
## `NetDoorCompositeStateCode`

출처: `S4A data/T06.md#type-NetDoorCompositeStateCode`, T06 L384..389.

```c
enum NetDoorCompositeStateCode
{
    NET_DOOR_COMPOSITE_NORMAL = 0,
    NET_DOOR_COMPOSITE_INCONSISTENT = 1,
    NET_DOOR_COMPOSITE_UNTRUSTED = 2
};
```

| 상수 | 값·의미 |
|---|---|
| `NET_DOOR_COMPOSITE_NORMAL` | `0` · 정상 조합/상태 |
| `NET_DOOR_COMPOSITE_INCONSISTENT` | `1` · 상호 불일치 |
| `NET_DOOR_COMPOSITE_UNTRUSTED` | `2` · 상태 신뢰 불가 |


<a id="type-NetDoorEvent"></a>
## `NetDoorEvent`

출처: `S4A data/T06.md#type-NetDoorEvent`, T06 L467..467.

```c
typedef uint8_t NetDoorEvent;
```



<a id="type-NetDoorEventCode"></a>
## `NetDoorEventCode`

출처: `S4A data/T06.md#type-NetDoorEventCode`, T06 L468..476.

```c
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

| 상수 | 값·의미 |
|---|---|
| `NET_DOOR_EVENT_NONE` | `0` · 없음 |
| `NET_DOOR_EVENT_LOCK_COMPLETED` | `1` · 목표 잠금 실제 확인 |
| `NET_DOOR_EVENT_UNLOCK_COMPLETED` | `2` · 목표 해제 실제 확인 |
| `NET_DOOR_EVENT_ALREADY_AT_TARGET` | `3` · 추가 구동 없이 목표 상태 |
| `NET_DOOR_EVENT_REQUEST_REJECTED` | `4` · 실행 전 거부 |
| `NET_DOOR_EVENT_REQUEST_FAILED` | `5` · 실행/확인 실패 |


<a id="type-NetDoorLockState"></a>
## `NetDoorLockState`

출처: `S4A data/T06.md#type-NetDoorLockState`, T06 L341..341.

```c
typedef uint8_t NetDoorLockState;
```



<a id="type-NetDoorLockStateCode"></a>
## `NetDoorLockStateCode`

출처: `S4A data/T06.md#type-NetDoorLockStateCode`, T06 L342..347.

```c
enum NetDoorLockStateCode
{
    NET_DOOR_LOCK_LOCKED = 0,
    NET_DOOR_LOCK_UNLOCKED = 1,
    NET_DOOR_LOCK_UNKNOWN = 255
};
```

| 상수 | 값·의미 |
|---|---|
| `NET_DOOR_LOCK_LOCKED` | `0` · 잠금 확인 |
| `NET_DOOR_LOCK_UNLOCKED` | `1` · 잠금 해제 확인 |
| `NET_DOOR_LOCK_UNKNOWN` | `255` · 확인 불가 |


<a id="type-NetDoorOpenState"></a>
## `NetDoorOpenState`

출처: `S4A data/T06.md#type-NetDoorOpenState`, T06 L362..362.

```c
typedef uint8_t NetDoorOpenState;
```



<a id="type-NetDoorOpenStateCode"></a>
## `NetDoorOpenStateCode`

출처: `S4A data/T06.md#type-NetDoorOpenStateCode`, T06 L363..368.

```c
enum NetDoorOpenStateCode
{
    NET_DOOR_OPEN_CLOSED = 0,
    NET_DOOR_OPEN_OPEN = 1,
    NET_DOOR_OPEN_UNKNOWN = 255
};
```

| 상수 | 값·의미 |
|---|---|
| `NET_DOOR_OPEN_CLOSED` | `0` · 닫힘 확인 |
| `NET_DOOR_OPEN_OPEN` | `1` · 열림 확인 |
| `NET_DOOR_OPEN_UNKNOWN` | `255` · 확인 불가 |


<a id="type-NetDoorTarget"></a>
## `NetDoorTarget`

출처: `S4A data/T06.md#type-NetDoorTarget`, T06 L319..319.

```c
typedef uint8_t NetDoorTarget;
```



<a id="type-NetDoorTargetCode"></a>
## `NetDoorTargetCode`

출처: `S4A data/T06.md#type-NetDoorTargetCode`, T06 L320..325.

```c
enum NetDoorTargetCode
{
    NET_DOOR_TARGET_LOCK = 0,
    NET_DOOR_TARGET_UNLOCK = 1,
    NET_DOOR_TARGET_NOT_APPLICABLE = 255
};
```

| 상수 | 값·의미 |
|---|---|
| `NET_DOOR_TARGET_LOCK` | `0` · 잠금 목표 |
| `NET_DOOR_TARGET_UNLOCK` | `1` · 해제 목표 |
| `NET_DOOR_TARGET_NOT_APPLICABLE` | `255` · 해당 결과에 대상 없음 |


<a id="type-NetHeatRemovalState"></a>
## `NetHeatRemovalState`

출처: `S4A data/T06.md#type-NetHeatRemovalState`, T06 L404..404.

```c
typedef uint8_t NetHeatRemovalState;
```



<a id="type-NetHeatRemovalStateCode"></a>
## `NetHeatRemovalStateCode`

출처: `S4A data/T06.md#type-NetHeatRemovalStateCode`, T06 L405..410.

```c
enum NetHeatRemovalStateCode
{
    NET_HEAT_REMOVAL_NORMAL = 0,
    NET_HEAT_REMOVAL_OVERHEAT_CUTOFF = 1,
    NET_HEAT_REMOVAL_UNAVAILABLE = 255
};
```

| 상수 | 값·의미 |
|---|---|
| `NET_HEAT_REMOVAL_NORMAL` | `0` · 정상 조합/상태 |
| `NET_HEAT_REMOVAL_OVERHEAT_CUTOFF` | `1` · 과열에 따른 차단 |
| `NET_HEAT_REMOVAL_UNAVAILABLE` | `255` · 제공 불가 |


<a id="type-NetLightApplyResult"></a>
## `NetLightApplyResult`

출처: `S4A data/T06.md#type-NetLightApplyResult`, T06 L425..425.

```c
typedef uint8_t NetLightApplyResult;
```



<a id="type-NetLightApplyResultCode"></a>
## `NetLightApplyResultCode`

출처: `S4A data/T06.md#type-NetLightApplyResultCode`, T06 L426..431.

```c
enum NetLightApplyResultCode
{
    NET_LIGHT_APPLY_APPLIED = 0,
    NET_LIGHT_APPLY_FAILED = 1,
    NET_LIGHT_APPLY_UNKNOWN = 255
};
```

| 상수 | 값·의미 |
|---|---|
| `NET_LIGHT_APPLY_APPLIED` | `0` · 출력 지시 적용 |
| `NET_LIGHT_APPLY_FAILED` | `1` · 실패 |
| `NET_LIGHT_APPLY_UNKNOWN` | `255` · 확인 불가 |


<a id="type-NetDomainPermission"></a>
## `NetDomainPermission`

출처: `S4A data/T37.md#type-NetDomainPermission`, T37 L786..795.

```c
typedef struct NetDomainPermission
{
    NetTriState vehicle_power_permission; /**< VEHICLE_POWER_PERMISSION: 0=불허; 1=허용; 255=확인 불가 */
    NetValueQuality power_permission_quality; /**< POWER_PERMISSION_QUALITY: VALUE_QUALITY */
    NetTriState power_operation_state; /**< POWER_OPERATION_STATE: 0=이동 불가; 1=이동 가능; 255=확인 불가 */
    NetFlag operation_allowed; /**< OPERATION_ALLOWED: 0=불허; 1=허용 */
    NetValueQuality operation_quality; /**< OPERATION_QUALITY: VALUE_QUALITY */
    uint32_t update_sequence; /**< UPDATE_SEQUENCE: Network 합성 번호; DOMAIN 원 성공 평가 번호와 별개 */
    uint16_t source_age_ms; /**< SOURCE_AGE: 합성에 포함한 원 판단의 보수적 누적 경과, ms; 단순 새 송신 시 0으로 초기화 금지 */
} NetDomainPermission;
```

| 필드 | C 타입·폭 | 값·단위·유효 조건 | 생성·관리 주체 |
|---|---|---|---|
| `vehicle_power_permission` | [`NetTriState`](CAN_COMMON_DATA.md#type-NetTriState); 8비트 부호 없음 | VEHICLE_POWER_PERMISSION: 0=불허; 1=허용; 255=확인 불가 | DOMAIN C2 전원 허용 판단 |
| `power_permission_quality` | [`NetValueQuality`](CAN_COMMON_DATA.md#type-NetValueQuality); 8비트 부호 없음 | POWER_PERMISSION_QUALITY: VALUE_QUALITY | 위 전원 허용의 원 SignalMeta·품질 |
| `power_operation_state` | [`NetTriState`](CAN_COMMON_DATA.md#type-NetTriState); 8비트 부호 없음 | POWER_OPERATION_STATE: 0=이동 불가; 1=이동 가능; 255=확인 불가 | 생성원 TEAM_DECISION_REQUIRED |
| `operation_allowed` | [`NetFlag`](CAN_COMMON_DATA.md#type-NetFlag); 8비트 부호 없음 | OPERATION_ALLOWED: 0=불허; 1=허용 | DOMAIN C2 WINDOW operation 판단 |
| `operation_quality` | [`NetValueQuality`](CAN_COMMON_DATA.md#type-NetValueQuality); 8비트 부호 없음 | OPERATION_QUALITY: VALUE_QUALITY | operation·Domain alive 원 품질의 보수 결합 후보 |
| `update_sequence` | `uint32_t`; 32비트 부호 없음 | UPDATE_SEQUENCE: 검증된 원 tuple 또는 실제 전달 의미·품질 제한 변경 후보. 원 성공/전송 번호와 별개 | Network 합성 경계 후보; 갱신 세부조건 OPEN |
| `source_age_ms` | `uint16_t`; 16비트 부호 없음 | SOURCE_AGE: 합성에 포함한 원 판단의 보수적 누적 경과, ms; 단순 새 송신 시 0으로 초기화 금지 | DOMAIN 원 성공 시각을 보존하는 Network 합성 후보; 실패/합성번호 변경에서 AGE0 금지 |


Permission의 원 성공 평가·CAN 합성 UPDATE·CAN TX·UART LINK·CIS relay UPDATE는 [서로 다른 번호 공간](../flows/CAN_BCM_AND_PERMISSION.md#permission-sequences)이다. 이 DTO에는 DOMAIN 성공 번호와 모든 원 입력별 세대를 별도 무손실 전달하는 필드가 없다. 원 tuple/원 시각은 검증된 합성 기록에 보존하며 없는 원 식별자를 송신 번호에서 복원하지 않는다. 성공17 뒤 평가 실패의 현재 제한은 원17/시각을 유지한 별도 합성 후보이며 같은 원 번호라는 이유로 폐기하지 않는다.

<a id="type-NetFanLevel"></a>
## `NetFanLevel`

출처: `S4A data/T37.md#type-NetFanLevel`, T37 L502..502.

```c
typedef uint8_t NetFanLevel;
```



<a id="type-NetFanLevelCode"></a>
## `NetFanLevelCode`

출처: `S4A data/T37.md#type-NetFanLevelCode`, T37 L503..510.

```c
enum NetFanLevelCode
{
    NET_FAN_OFF = 0,
    NET_FAN_LOW = 1,
    NET_FAN_MEDIUM = 2,
    NET_FAN_HIGH = 3,
    NET_FAN_UNKNOWN = 255
};
```

| 상수 | 값·의미 |
|---|---|
| `NET_FAN_OFF` | 0 · FAN OFF |
| `NET_FAN_LOW` | 1 · FAN LOW |
| `NET_FAN_MEDIUM` | 2 · FAN MEDIUM |
| `NET_FAN_HIGH` | 3 · FAN HIGH |
| `NET_FAN_UNKNOWN` | 255 · FAN UNKNOWN |


<a id="type-NetThermalDirection"></a>
## `NetThermalDirection`

출처: `S4A data/T37.md#type-NetThermalDirection`, T37 L530..530.

```c
typedef uint8_t NetThermalDirection;
```



<a id="type-NetThermalDirectionCode"></a>
## `NetThermalDirectionCode`

출처: `S4A data/T37.md#type-NetThermalDirectionCode`, T37 L531..536.

```c
enum NetThermalDirectionCode
{
    NET_THERMAL_IDLE = 0,
    NET_THERMAL_COOL = 1,
    NET_THERMAL_HEAT = 2
};
```

| 상수 | 값·의미 |
|---|---|
| `NET_THERMAL_IDLE` | 0 · THERMAL IDLE |
| `NET_THERMAL_COOL` | 1 · THERMAL COOL |
| `NET_THERMAL_HEAT` | 2 · THERMAL HEAT |


<a id="type-NetLightType"></a>
## `NetLightType`

출처: `S4A data/T37.md#type-NetLightType`, T37 L554..554.

```c
typedef uint8_t NetLightType;
```



<a id="type-NetLightTypeCode"></a>
## `NetLightTypeCode`

출처: `S4A data/T37.md#type-NetLightTypeCode`, T37 L555..561.

```c
enum NetLightTypeCode
{
    NET_LIGHT_NORMAL = 0,
    NET_LIGHT_GOODBYE = 1,
    NET_LIGHT_WARNING = 2,
    NET_LIGHT_FAULT = 3
};
```

| 상수 | 값·의미 |
|---|---|
| `NET_LIGHT_NORMAL` | 0 · LIGHT NORMAL |
| `NET_LIGHT_GOODBYE` | 1 · LIGHT GOODBYE |
| `NET_LIGHT_WARNING` | 2 · LIGHT WARNING |
| `NET_LIGHT_FAULT` | 3 · LIGHT FAULT |


<a id="type-NetRgb"></a>
## `NetRgb`

출처: `S4A data/T37.md#type-NetRgb`, T37 L201..206.

```c
typedef struct NetRgb
{
    uint8_t r; /**< 빨강. 0~255. */
    uint8_t g; /**< 초록. 0~255. */
    uint8_t b; /**< 파랑. 0~255. */
} NetRgb;
```

| 필드 | C 타입·폭 | 값·단위·유효 조건 | 생성·관리 주체 |
|---|---|---|---|
| `r` | `uint8_t`; 8비트 부호 없음 | 빨강. 0~255. | 해당 정보 생산 기능 |
| `g` | `uint8_t`; 8비트 부호 없음 | 초록. 0~255. | 해당 정보 생산 기능 |
| `b` | `uint8_t`; 8비트 부호 없음 | 파랑. 0~255. | 해당 정보 생산 기능 |


<a id="type-DoorLockState_t"></a>
## `DoorLockState_t`

출처: `S4A data/T13.md#type-DoorLockState_t`, T13 L531..536.

```c
typedef enum
{
    DOOR_LOCK_STATE_UNKNOWN = 0,
    DOOR_LOCK_STATE_LOCKED,
    DOOR_LOCK_STATE_UNLOCKED
} DoorLockState_t;
```



<a id="type-DoorOpenState_t"></a>
## `DoorOpenState_t`

출처: `S4A data/T13.md#type-DoorOpenState_t`, T13 L551..556.

```c
typedef enum
{
    DOOR_OPEN_STATE_UNKNOWN = 0,
    DOOR_OPEN_STATE_CLOSED,
    DOOR_OPEN_STATE_OPEN
} DoorOpenState_t;
```



<a id="type-DoorCompositeState_t"></a>
## `DoorCompositeState_t`

출처: `S4A data/T13.md#type-DoorCompositeState_t`, T13 L571..576.

```c
typedef enum
{
    DOOR_COMPOSITE_STATE_NORMAL = 0,
    DOOR_COMPOSITE_STATE_INCONSISTENT,
    DOOR_COMPOSITE_STATE_UNTRUSTED
} DoorCompositeState_t;
```



<a id="type-DoorLockTarget_t"></a>
## `DoorLockTarget_t`

출처: `S4A data/T13.md#type-DoorLockTarget_t`, T13 L591..595.

```c
typedef enum
{
    DOOR_TARGET_LOCK = 0,
    DOOR_TARGET_UNLOCK
} DoorLockTarget_t;
```



<a id="type-FanLevel_t"></a>
## `FanLevel_t`

출처: `S4A data/T13.md#type-FanLevel_t`, T13 L639..646.

```c
typedef enum
{
    FAN_LEVEL_OFF = 0,
    FAN_LEVEL_LOW,
    FAN_LEVEL_MEDIUM,
    FAN_LEVEL_HIGH,
    FAN_LEVEL_UNKNOWN
} FanLevel_t;
```



<a id="type-ThermalDirection_t"></a>
## `ThermalDirection_t`

출처: `S4A data/T13.md#type-ThermalDirection_t`, T13 L663..668.

```c
typedef enum
{
    THERMAL_DIRECTION_IDLE = 0,
    THERMAL_DIRECTION_COOL,
    THERMAL_DIRECTION_HEAT
} ThermalDirection_t;
```



<a id="type-HeatRemovalState_t"></a>
## `HeatRemovalState_t`

출처: `S4A data/T13.md#type-HeatRemovalState_t`, T13 L683..688.

```c
typedef enum
{
    HEAT_REMOVAL_STATE_NORMAL = 0,
    HEAT_REMOVAL_STATE_BLOCKED_OVERHEAT,
    HEAT_REMOVAL_STATE_UNMEASURABLE
} HeatRemovalState_t;
```



<a id="type-InteriorLightType_t"></a>
## `InteriorLightType_t`

출처: `S4A data/T13.md#type-InteriorLightType_t`, T13 L739..745.

```c
typedef enum
{
    INTERIOR_LIGHT_TYPE_NORMAL = 0,
    INTERIOR_LIGHT_TYPE_GOODBYE,
    INTERIOR_LIGHT_TYPE_WARNING,
    INTERIOR_LIGHT_TYPE_FAULT
} InteriorLightType_t;
```



<a id="type-InteriorLightApplyState_t"></a>
## `InteriorLightApplyState_t`

출처: `S4A data/T13.md#type-InteriorLightApplyState_t`, T13 L781..786.

```c
typedef enum
{
    INTERIOR_LIGHT_APPLY_UNKNOWN = 0,
    INTERIOR_LIGHT_APPLY_APPLIED,
    INTERIOR_LIGHT_APPLY_FAILED
} InteriorLightApplyState_t;
```



<a id="type-RgbColor_t"></a>
## `RgbColor_t`

출처: `S4A data/T13.md#type-RgbColor_t`, T13 L761..766.

```c
typedef struct
{
    uint8_t red;
    uint8_t green;
    uint8_t blue;
} RgbColor_t;
```

| 필드 | C 타입·폭 | 값·단위·유효 조건 | 생성·관리 주체 |
|---|---|---|---|
| `red` | `uint8_t`; 8비트 부호 없음 | RGB 빨강 채널, 0~255. | 해당 의미 객체의 원 생산자; Domain은 소비/집계 |
| `green` | `uint8_t`; 8비트 부호 없음 | RGB 초록 채널, 0~255. | 해당 의미 객체의 원 생산자; Domain은 소비/집계 |
| `blue` | `uint8_t`; 8비트 부호 없음 | RGB 파랑 채널, 0~255. | 해당 의미 객체의 원 생산자; Domain은 소비/집계 |


<a id="type-DoorState_t"></a>
## `DoorState_t`

출처: `S4A data/T13.md#type-DoorState_t`, T13 L609..619.

```c
typedef struct
{
    DoorLockState_t lockState;
    DoorOpenState_t openState;
    DoorCompositeState_t compositeState;
    DataQuality_t lockQuality;
    DataQuality_t openQuality;
    DataQuality_t compositeQuality;
    SourceGeneration_t sourceGeneration; /* DOOR_LEFT_OPEN warning source */
    SignalMeta_t meta; /* shared age/sequence; quality is conservative summary */
} DoorState_t;
```

| 필드 | C 타입·폭 | 값·단위·유효 조건 | 생성·관리 주체 |
|---|---|---|---|
| `lockState` | [`DoorLockState_t`](CAN_BCM_DATA.md#type-DoorLockState_t); C enum 저장 폭 미검증 | BCM 실제 잠금 상태. UNKNOWN/LOCKED/UNLOCKED, lockQuality와 함께 소비. | BCM 해당 구동·관측·보호 담당자 |
| `openState` | [`DoorOpenState_t`](CAN_BCM_DATA.md#type-DoorOpenState_t); C enum 저장 폭 미검증 | BCM 실제 개폐 상태. UNKNOWN/CLOSED/OPEN, openQuality와 함께 소비. | BCM 해당 구동·관측·보호 담당자 |
| `compositeState` | [`DoorCompositeState_t`](CAN_BCM_DATA.md#type-DoorCompositeState_t); C enum 저장 폭 미검증 | BCM 잠금·개폐 조합 상태. 기본 관측과 모순 없는지 검사하고 compositeQuality를 유지. | BCM 해당 구동·관측·보호 담당자 |
| `lockQuality` | [`DataQuality_t`](CIS_REAR_DATA.md#type-DataQuality_t); C enum 저장 폭 미검증 | 잠금 관측의 독립 품질. 타 필드 품질로 덮어쓰지 않는다. | BCM 해당 구동·관측·보호 담당자 |
| `openQuality` | [`DataQuality_t`](CIS_REAR_DATA.md#type-DataQuality_t); C enum 저장 폭 미검증 | 개폐 관측의 독립 품질. | BCM 해당 구동·관측·보호 담당자 |
| `compositeQuality` | [`DataQuality_t`](CIS_REAR_DATA.md#type-DataQuality_t); C enum 저장 폭 미검증 | 잠금·개폐 조합 평가 품질. | BCM 해당 구동·관측·보호 담당자 |
| `sourceGeneration` | [`SourceGeneration_t`](CIS_REAR_DATA.md#type-SourceGeneration_t); 32비트 부호 없음 | 원본 관측/사건 생산 세대. 원 warning/fault 계보에 사용하고 마지막 송신 홉 Boot로 재라벨하지 않는다. | BCM 해당 구동·관측·보호 담당자 |
| `meta` | [`SignalMeta_t`](CIS_REAR_DATA.md#type-SignalMeta_t); 중첩 타입 참조; sizeof 미검증 | 원 생산자의 품질·원본 AGE·평가 번호. 이 메타는 링크 BOOT_ID/TX_SEQUENCE가 아니다. | BCM 해당 구동·관측·보호 담당자 |


<a id="type-ClimateState_t"></a>
## `ClimateState_t`

출처: 기존 `S4A data/T13.md#type-ClimateState_t`, T13 L703..716 및 선택 Interface의 `Vehicle_Types_v0.3.h` 후보. sourceGeneration을 마지막에 추가한 12개 논리 멤버이며 제품 헤더 채택은 미완료다.

```c
typedef struct
{
    FanLevel_t commandedFanLevel;
    FanLevel_t measuredFanLevel;
    ThermalDirection_t thermalDirection;
    uint8_t thermalOutputLevelPercent;
    HeatRemovalState_t heatRemovalState;
    DataQuality_t commandedFanQuality;
    DataQuality_t measuredFanQuality;
    DataQuality_t thermalDirectionQuality;
    DataQuality_t thermalOutputQuality;
    DataQuality_t heatRemovalQuality;
    SignalMeta_t meta; /* per-field quality is authoritative */
    SourceGeneration_t sourceGeneration;
} ClimateState_t;
```

| 필드 | C 타입·폭 | 값·단위·유효 조건 | 생성·관리 주체 |
|---|---|---|---|
| `commandedFanLevel` | [`FanLevel_t`](CAN_BCM_DATA.md#type-FanLevel_t); C enum 저장 폭 미검증 | BCM에 적용된 팬 목표 OFF/LOW/MEDIUM/HIGH; measuredFanLevel과 별개. | BCM 해당 구동·관측·보호 담당자 |
| `measuredFanLevel` | [`FanLevel_t`](CAN_BCM_DATA.md#type-FanLevel_t); C enum 저장 폭 미검증 | BCM 실제 팬 피드백. 목표만으로 실제 구동을 확인했다고 판단하지 않는다. | BCM 해당 구동·관측·보호 담당자 |
| `thermalDirection` | [`ThermalDirection_t`](CAN_BCM_DATA.md#type-ThermalDirection_t); C enum 저장 폭 미검증 | BCM 열 출력 방향 IDLE/COOL/HEAT. 방향 품질과 별도로 소비. | BCM 해당 구동·관측·보호 담당자 |
| `thermalOutputLevelPercent` | `uint8_t`; 8비트 부호 없음 | BCM 열 출력 지시량 0~100%, 실제 열 효과 확인과 구분. | BCM 해당 구동·관측·보호 담당자 |
| `heatRemovalState` | [`HeatRemovalState_t`](CAN_BCM_DATA.md#type-HeatRemovalState_t); C enum 저장 폭 미검증 | BCM 잔열 제거 동작 상태. 목표 팬 값과 별도 기능 판단. | BCM 해당 구동·관측·보호 담당자 |
| `commandedFanQuality` | [`DataQuality_t`](CIS_REAR_DATA.md#type-DataQuality_t); C enum 저장 폭 미검증 | 팬 지시값 품질. | BCM 해당 구동·관측·보호 담당자 |
| `measuredFanQuality` | [`DataQuality_t`](CIS_REAR_DATA.md#type-DataQuality_t); C enum 저장 폭 미검증 | 팬 실제 피드백 품질. | BCM 해당 구동·관측·보호 담당자 |
| `thermalDirectionQuality` | [`DataQuality_t`](CIS_REAR_DATA.md#type-DataQuality_t); C enum 저장 폭 미검증 | 열 방향 평가 품질. | BCM 해당 구동·관측·보호 담당자 |
| `thermalOutputQuality` | [`DataQuality_t`](CIS_REAR_DATA.md#type-DataQuality_t); C enum 저장 폭 미검증 | 열 출력 지시량 품질. | BCM 해당 구동·관측·보호 담당자 |
| `heatRemovalQuality` | [`DataQuality_t`](CIS_REAR_DATA.md#type-DataQuality_t); C enum 저장 폭 미검증 | 잔열 제거 상태 품질. | BCM 해당 구동·관측·보호 담당자 |
| `meta` | [`SignalMeta_t`](CIS_REAR_DATA.md#type-SignalMeta_t); 중첩 타입 참조; sizeof 미검증 | 원 생산자의 품질·원본 AGE·평가 번호. 이 메타는 링크 BOOT_ID/TX_SEQUENCE가 아니다. | BCM 해당 구동·관측·보호 담당자 |
| `sourceGeneration` | [`SourceGeneration_t`](CIS_REAR_DATA.md#type-SourceGeneration_t); 32비트 부호 없음 | 원 상태 생산 세대이며 CAN BOOT_ID와의 대응을 확인한 같은 관측의 근거로만 설정한다. 알려진 세대는 INVALID/STALE이라는 이유로 0으로 지우지 않는다. generation 0 수용 정책과 실제 BCM 연결은 미확정이다. | BCM 원 생산자 → 검증된 Network 변환 → DOMAIN 보관 |


<a id="type-InteriorLightState_t"></a>
## `InteriorLightState_t`

출처: 기존 `S4A data/T13.md#type-InteriorLightState_t`, T13 L801..813 및 선택 Interface의 `Vehicle_Types_v0.3.h` 후보. sourceGeneration을 마지막에 추가한 11개 논리 멤버이며 제품 헤더 채택은 미완료다.

```c
typedef struct
{
    InteriorLightType_t type;
    uint8_t levelPercent;
    RgbColor_t color;
    InteriorLightApplyState_t applyState; /* UNKNOWN is not false/FAILED */
    bool physicalFeedbackSupported;
    DataQuality_t typeQuality;
    DataQuality_t levelQuality;
    DataQuality_t colorQuality;
    DataQuality_t applyQuality;
    SignalMeta_t meta;
    SourceGeneration_t sourceGeneration;
} InteriorLightState_t;
```

| 필드 | C 타입·폭 | 값·단위·유효 조건 | 생성·관리 주체 |
|---|---|---|---|
| `type` | [`InteriorLightType_t`](CAN_BCM_DATA.md#type-InteriorLightType_t); C enum 저장 폭 미검증 | BCM에 적용된 실내 조명 종류. 실제 점등 확인과 구분. | BCM 해당 구동·관측·보호 담당자 |
| `levelPercent` | `uint8_t`; 8비트 부호 없음 | 밝기 0~100%. 값 범위를 확인하고 enum/목표와 함께 부호화. | BCM 해당 구동·관측·보호 담당자 |
| `color` | [`RgbColor_t`](CAN_BCM_DATA.md#type-RgbColor_t); 중첩 타입 참조; sizeof 미검증 | RGB 각 0~255 인 의미 색상. C struct padding을 raw로 복사하지 않는다. | BCM 해당 구동·관측·보호 담당자 |
| `applyState` | [`InteriorLightApplyState_t`](CAN_BCM_DATA.md#type-InteriorLightApplyState_t); C enum 저장 폭 미검증 | BCM 조명 지시 적용 상태. 실제 물리 점등 확인과 구분. | BCM 해당 구동·관측·보호 담당자 |
| `physicalFeedbackSupported` | `bool`; 저장 폭은 ABI 미검증 | 실제 점등 피드백 지원 여부 bool. false 이면 실제 점등 완료를 만들어내지 않는다. | BCM 해당 구동·관측·보호 담당자 |
| `typeQuality` | [`DataQuality_t`](CIS_REAR_DATA.md#type-DataQuality_t); C enum 저장 폭 미검증 | 조명 종류 독립 품질. | BCM 해당 구동·관측·보호 담당자 |
| `levelQuality` | [`DataQuality_t`](CIS_REAR_DATA.md#type-DataQuality_t); C enum 저장 폭 미검증 | 조명 밝기 독립 품질. | BCM 해당 구동·관측·보호 담당자 |
| `colorQuality` | [`DataQuality_t`](CIS_REAR_DATA.md#type-DataQuality_t); C enum 저장 폭 미검증 | RGB 독립 품질. | BCM 해당 구동·관측·보호 담당자 |
| `applyQuality` | [`DataQuality_t`](CIS_REAR_DATA.md#type-DataQuality_t); C enum 저장 폭 미검증 | 조명 지시 적용 독립 품질. | BCM 해당 구동·관측·보호 담당자 |
| `meta` | [`SignalMeta_t`](CIS_REAR_DATA.md#type-SignalMeta_t); 중첩 타입 참조; sizeof 미검증 | 원 생산자의 품질·원본 AGE·평가 번호. 이 메타는 링크 BOOT_ID/TX_SEQUENCE가 아니다. | BCM 해당 구동·관측·보호 담당자 |
| `sourceGeneration` | [`SourceGeneration_t`](CIS_REAR_DATA.md#type-SourceGeneration_t); 32비트 부호 없음 | 원 상태 생산 세대이며 CAN BOOT_ID와의 대응을 확인한 같은 관측의 근거로만 설정한다. 알려진 세대는 INVALID/STALE이라는 이유로 0으로 지우지 않는다. generation 0 수용 정책과 실제 BCM 연결은 미확정이다. | BCM 원 생산자 → 검증된 Network 변환 → DOMAIN 보관 |


<a id="type-DomainIf_BcmDoorCommand_t"></a>
## `DomainIf_BcmDoorCommand_t`

출처: `S4A data/T14.md#type-DomainIf_BcmDoorCommand_t`, T14 L896..900.

```c
typedef struct
{
    DomainIf_CommandContext_t context;
    DoorLockTarget_t target;
} DomainIf_BcmDoorCommand_t;
```

| 필드 | C 타입·폭 | 값·단위·유효 조건 | 생성·관리 주체 |
|---|---|---|---|
| `context` | [`DomainIf_CommandContext_t`](CAN_COMMON_DATA.md#type-DomainIf_CommandContext_t); 중첩 타입 참조; sizeof 미검증 | 해당 객체의 원 요청/명령 문맥. 하위 타입 전체 정의와 유효성 조건을 함께 적용한다. | Domain 기능 결정/설정/결과 담당자 |
| `target` | [`DoorLockTarget_t`](CAN_BCM_DATA.md#type-DoorLockTarget_t); C enum 저장 폭 미검증 | Domain 명령 도어 LOCK/UNLOCK 또는 결과 대상 typed 값. 해당 타입 enum만 해석. | Domain 기능 결정/설정/결과 담당자 |


<a id="type-DomainIf_BcmClimateCommand_t"></a>
## `DomainIf_BcmClimateCommand_t`

출처: `S4A data/T14.md#type-DomainIf_BcmClimateCommand_t`, T14 L914..920.

```c
typedef struct
{
    DomainIf_CommandContext_t context;
    FanLevel_t fanTarget;
    ThermalDirection_t thermalDirection;
    uint8_t thermalTargetLevelPercent;
} DomainIf_BcmClimateCommand_t;
```

| 필드 | C 타입·폭 | 값·단위·유효 조건 | 생성·관리 주체 |
|---|---|---|---|
| `context` | [`DomainIf_CommandContext_t`](CAN_COMMON_DATA.md#type-DomainIf_CommandContext_t); 중첩 타입 참조; sizeof 미검증 | 해당 객체의 원 요청/명령 문맥. 하위 타입 전체 정의와 유효성 조건을 함께 적용한다. | Domain 기능 결정/설정/결과 담당자 |
| `fanTarget` | [`FanLevel_t`](CAN_BCM_DATA.md#type-FanLevel_t); C enum 저장 폭 미검증 | Domain 확정 팬 목표. BCM 실제 구동 피드백과 별개. | Domain 기능 결정/설정/결과 담당자 |
| `thermalDirection` | [`ThermalDirection_t`](CAN_BCM_DATA.md#type-ThermalDirection_t); C enum 저장 폭 미검증 | DOMAIN 기능 Owner가 정한 열 출력 목표 방향 IDLE/COOL/HEAT. BCM 실제 출력 방향 피드백과 구분하며 이 명령 DTO에는 독립 방향 quality 필드가 없다. | Domain 기능 결정/설정/결과 담당자 |
| `thermalTargetLevelPercent` | `uint8_t`; 8비트 부호 없음 | Domain 확정 열 출력 목표 0~100%. | Domain 기능 결정/설정/결과 담당자 |


<a id="type-DomainIf_BcmInteriorLightCommand_t"></a>
## `DomainIf_BcmInteriorLightCommand_t`

출처: `S4A data/T14.md#type-DomainIf_BcmInteriorLightCommand_t`, T14 L936..942.

```c
typedef struct
{
    DomainIf_CommandContext_t context;
    InteriorLightType_t type;
    uint8_t levelPercent;
    RgbColor_t color;
} DomainIf_BcmInteriorLightCommand_t;
```

| 필드 | C 타입·폭 | 값·단위·유효 조건 | 생성·관리 주체 |
|---|---|---|---|
| `context` | [`DomainIf_CommandContext_t`](CAN_COMMON_DATA.md#type-DomainIf_CommandContext_t); 중첩 타입 참조; sizeof 미검증 | 해당 객체의 원 요청/명령 문맥. 하위 타입 전체 정의와 유효성 조건을 함께 적용한다. | Domain 기능 결정/설정/결과 담당자 |
| `type` | [`InteriorLightType_t`](CAN_BCM_DATA.md#type-InteriorLightType_t); C enum 저장 폭 미검증 | Domain 확정 실내 조명 종류 NORMAL/GOODBYE/WARNING/FAULT. Mobile raw 요청 권한과 별도. | Domain 기능 결정/설정/결과 담당자 |
| `levelPercent` | `uint8_t`; 8비트 부호 없음 | 밝기 0~100%. 값 범위를 확인하고 enum/목표와 함께 부호화. | Domain 기능 결정/설정/결과 담당자 |
| `color` | [`RgbColor_t`](CAN_BCM_DATA.md#type-RgbColor_t); 중첩 타입 참조; sizeof 미검증 | RGB 각 0~255 인 의미 색상. C struct padding을 raw로 복사하지 않는다. | Domain 기능 결정/설정/결과 담당자 |


<a id="type-DomainIf_BcmCommandKind_t"></a>
## `DomainIf_BcmCommandKind_t`

출처: `S4A data/T14.md#type-DomainIf_BcmCommandKind_t`, T14 L415..420.

```c
typedef enum
{
    BCM_COMMAND_KIND_DOOR = 0,
    BCM_COMMAND_KIND_CLIMATE,
    BCM_COMMAND_KIND_INTERIOR_LIGHT
} DomainIf_BcmCommandKind_t;
```



<a id="type-DomainIf_BcmDoorEvidenceType_t"></a>
## `DomainIf_BcmDoorEvidenceType_t`

출처: `S4A data/T14.md#type-DomainIf_BcmDoorEvidenceType_t`, T14 L435..442.

```c
typedef enum
{
    BCM_DOOR_EVIDENCE_LOCK_COMPLETED = 0,
    BCM_DOOR_EVIDENCE_UNLOCK_COMPLETED,
    BCM_DOOR_EVIDENCE_ALREADY_AT_TARGET,
    BCM_DOOR_EVIDENCE_REQUEST_REJECTED,
    BCM_DOOR_EVIDENCE_REQUEST_FAILED
} DomainIf_BcmDoorEvidenceType_t;
```



<a id="type-DomainIf_BcmDoorResultEvidence_t"></a>
## `DomainIf_BcmDoorResultEvidence_t`

출처: `S4A data/T14.md#type-DomainIf_BcmDoorResultEvidence_t`, T14 L459..466.

```c
typedef struct
{
    DomainIf_BcmDoorEvidenceType_t type;
    OccurrenceId_t occurrenceId;
    DoorLockTarget_t target;
    DataQuality_t quality;
    uint32_t ageMs;
} DomainIf_BcmDoorResultEvidence_t;
```

| 필드 | C 타입·폭 | 값·단위·유효 조건 | 생성·관리 주체 |
|---|---|---|---|
| `type` | [`DomainIf_BcmDoorEvidenceType_t`](CAN_BCM_DATA.md#type-DomainIf_BcmDoorEvidenceType_t); C enum 저장 폭 미검증 | BCM lock/unlock 완료, 이미 목표 상태, 거부, 실패의 현장 실행 근거. | BCM 해당 구동·관측·보호 담당자 |
| `occurrenceId` | [`OccurrenceId_t`](CAN_COMMON_DATA.md#type-OccurrenceId_t); 32비트 부호 없음 | 원 사건/결과/경고 발생 식별 u32. 생산 ECU의 generation과 함께 중복 판단; 링크 전송순번과 다르다. | BCM 해당 구동·관측·보호 담당자 |
| `target` | [`DoorLockTarget_t`](CAN_BCM_DATA.md#type-DoorLockTarget_t); C enum 저장 폭 미검증 | Domain 명령 도어 LOCK/UNLOCK 또는 결과 대상 typed 값. 해당 타입 enum만 해석. | BCM 해당 구동·관측·보호 담당자 |
| `quality` | [`DataQuality_t`](CIS_REAR_DATA.md#type-DataQuality_t); C enum 저장 폭 미검증 | 이 값의 품질. OK/STALE/INVALID/NO_DATA를 정상 값/위험 해제 여부와 분리한다. | BCM 해당 구동·관측·보호 담당자 |
| `ageMs` | `uint32_t`; 32비트 부호 없음 | 원본 관측/평가/사건 후 누적 경과, ms, u32. 수신·대기 경과를 한 번만 더하고 중계에서 0으로 초기화하지 않는다. | BCM 해당 구동·관측·보호 담당자 |


<a id="type-DomainIf_BcmCommandResult_t"></a>
## `DomainIf_BcmCommandResult_t`

출처: `S4A data/T14.md#type-DomainIf_BcmCommandResult_t`, T14 L483..492.

```c
typedef struct
{
    DomainCommandId_t commandId;
    DomainIf_BcmCommandKind_t kind;
    RequestResult_t result;
    ResultReason_t reason;
    ResultConfirmation_t confirmation;
    bool hasDoorEvidence;
    DomainIf_BcmDoorResultEvidence_t doorEvidence;
} DomainIf_BcmCommandResult_t;
```

| 필드 | C 타입·폭 | 값·단위·유효 조건 | 생성·관리 주체 |
|---|---|---|---|
| `commandId` | [`DomainCommandId_t`](CAN_COMMON_DATA.md#type-DomainCommandId_t); 32비트 부호 없음 | Domain 기능 결정 담당자의 단일 Command 식별 u32. Network 에서 새로 발급하지 않으며 TX_SEQUENCE·requestId와 다르다. | BCM 해당 구동·관측·보호 담당자 |
| `kind` | [`DomainIf_BcmCommandKind_t`](CAN_BCM_DATA.md#type-DomainIf_BcmCommandKind_t); C enum 저장 폭 미검증 | BCM 실행 명령 종류 DOOR/CLIMATE/INTERIOR_LIGHT. 결과의 commandId와 원 TX kind를 대조. | BCM 해당 구동·관측·보호 담당자 |
| `result` | [`RequestResult_t`](CAN_COMMON_DATA.md#type-RequestResult_t); C enum 저장 폭 미검증 | 기능 담당자가 판정한 차량 처리 단계. Network local OK와 구분하며 DONE은 실행 근거가 있을 때만. | BCM 해당 구동·관측·보호 담당자 |
| `reason` | [`ResultReason_t`](CAN_COMMON_DATA.md#type-ResultReason_t); C enum 저장 폭 미검증 | 결과·품질·가용성을 설명하는 typed 사유. 예약 값 거부, raw 숫자 변환은 명시 표를 따른다. | BCM 해당 구동·관측·보호 담당자 |
| `confirmation` | [`ResultConfirmation_t`](CAN_COMMON_DATA.md#type-ResultConfirmation_t); C enum 저장 폭 미검증 | 결과 확인 여부. CAN BCM_RESULT에는 해당 필드가 없으므로 별도 증거 없이는 UNCONFIRMED 인계 후보. DONE만으로 CONFIRMED를 만들지 않는다. 양단 확인 제한. | BCM 실행 근거 → DOMAIN 수신 Adapter의 명시 연결(PROPOSED/BINDING_OPEN) |
| `hasDoorEvidence` | `bool`; 저장 폭은 ABI 미검증 | 도어 실행 근거 포함 bool. true 일 때 doorEvidence 해석, 보호 Event와 혼합 금지. | BCM 해당 구동·관측·보호 담당자 |
| `doorEvidence` | [`DomainIf_BcmDoorResultEvidence_t`](CAN_BCM_DATA.md#type-DomainIf_BcmDoorResultEvidence_t); 중첩 타입 참조; sizeof 미검증 | BCM 도어 결과의 atomic 실행 근거. occurrence·target·quality·age 유지. | BCM 해당 구동·관측·보호 담당자 |


<a id="type-DomainIf_BcmProtectionEventType_t"></a>
## `DomainIf_BcmProtectionEventType_t`

출처: `S4A data/T14.md#type-DomainIf_BcmProtectionEventType_t`, T14 L511..516.

```c
typedef enum
{
    BCM_PROTECTION_OVERHEAT_DETECTED = 0,
    BCM_PROTECTION_FAN_MISMATCH_DETECTED,
    BCM_PROTECTION_RECOVERY_CONFIRMED
} DomainIf_BcmProtectionEventType_t;
```



<a id="type-DomainIf_BcmProtectionEvent_t"></a>
## `DomainIf_BcmProtectionEvent_t`

출처: `S4A data/T14.md#type-DomainIf_BcmProtectionEvent_t`, T14 L531..541.

```c
typedef struct
{
    DomainIf_BcmProtectionEventType_t type;
    OccurrenceId_t occurrenceId;
    SourceGeneration_t sourceGeneration; /* identity/derived fault-warning lineage */
    DataQuality_t quality;
    uint32_t ageMs;
    bool hasRelatedCommand;
    DomainCommandId_t relatedCommandId;
    uint8_t affectedFunctionMask; /* semantic bits: door / climate / light */
} DomainIf_BcmProtectionEvent_t;
```

| 필드 | C 타입·폭 | 값·단위·유효 조건 | 생성·관리 주체 |
|---|---|---|---|
| `type` | [`DomainIf_BcmProtectionEventType_t`](CAN_BCM_DATA.md#type-DomainIf_BcmProtectionEventType_t); C enum 저장 폭 미검증 | BCM 과열·팬 불일치·회복 확인 보호 사건. 도어 실행 완료와 별도. | BCM 해당 구동·관측·보호 담당자 |
| `occurrenceId` | [`OccurrenceId_t`](CAN_COMMON_DATA.md#type-OccurrenceId_t); 32비트 부호 없음 | 원 사건/결과/경고 발생 식별 u32. 생산 ECU의 generation과 함께 중복 판단; 링크 전송순번과 다르다. | BCM 해당 구동·관측·보호 담당자 |
| `sourceGeneration` | [`SourceGeneration_t`](CIS_REAR_DATA.md#type-SourceGeneration_t); 32비트 부호 없음 | 원본 관측/사건 생산 세대. 원 warning/fault 계보에 사용하고 마지막 송신 홉 Boot로 재라벨하지 않는다. | BCM 해당 구동·관측·보호 담당자 |
| `quality` | [`DataQuality_t`](CIS_REAR_DATA.md#type-DataQuality_t); C enum 저장 폭 미검증 | 이 값의 품질. OK/STALE/INVALID/NO_DATA를 정상 값/위험 해제 여부와 분리한다. | BCM 해당 구동·관측·보호 담당자 |
| `ageMs` | `uint32_t`; 32비트 부호 없음 | 원본 관측/평가/사건 후 누적 경과, ms, u32. 수신·대기 경과를 한 번만 더하고 중계에서 0으로 초기화하지 않는다. | BCM 해당 구동·관측·보호 담당자 |
| `hasRelatedCommand` | `bool`; 저장 폭은 ABI 미검증 | 원 Domain 명령 연결 여부 bool. false 이면 relatedCommandId를 해석하지 않는다. | BCM 해당 구동·관측·보호 담당자 |
| `relatedCommandId` | [`DomainCommandId_t`](CAN_COMMON_DATA.md#type-DomainCommandId_t); 32비트 부호 없음 | 있으면 원 Domain Command ID echo. 로컬 사건에 가짜 명령 ID 생성 금지. | BCM 해당 구동·관측·보호 담당자 |
| `affectedFunctionMask` | `uint8_t`; 8비트 부호 없음 | 영향 기능 bitmask. 노드별 의미를 적용하고 알 수 없는 bit를 임의 확정하지 않는다. | BCM 해당 구동·관측·보호 담당자 |


<a id="type-DomainIf_BcmStatusReport_t"></a>
## `DomainIf_BcmStatusReport_t`

출처: `S4A data/T14.md#type-DomainIf_BcmStatusReport_t`, T14 L561..572.

```c
typedef struct
{
    CommonEcuState_t ecuState;
    uint16_t faultMask;
    FaultInfo_t activeFault; /* faultCode==0: explicitly NO active fault.
        category/state/quality are then ignored; no redundant hasActiveFault.
        If code!=0, category must be valid. Wire NONE=0 is not SENSOR=0. */
    uint8_t affectedFunctionMask;
    bool recovering;
    SourceGeneration_t sourceGeneration; /* for the derived BCM fault warning */
    SignalMeta_t meta;
} DomainIf_BcmStatusReport_t;
```

| 필드 | C 타입·폭 | 값·단위·유효 조건 | 생성·관리 주체 |
|---|---|---|---|
| `ecuState` | [`CommonEcuState_t`](CAN_COMMON_DATA.md#type-CommonEcuState_t); C enum 저장 폭 미검증 | 원 ECU 기능 상태. 링크 사용 가능성과 구별. | BCM 해당 구동·관측·보호 담당자 |
| `faultMask` | `uint16_t`; 16비트 부호 없음 | 활성/종합 fault bitmask. 노드별 bit 계약 및 활성 code와 구분. | BCM 해당 구동·관측·보호 담당자 |
| `activeFault` | [`FaultInfo_t`](CAN_COMMON_DATA.md#type-FaultInfo_t); 중첩 타입 참조; sizeof 미검증 | 현재 고장 의미 객체. faultCode=0 이면 category/state/quality는 해석하지 않는다. | BCM 해당 구동·관측·보호 담당자 |
| `affectedFunctionMask` | `uint8_t`; 8비트 부호 없음 | 영향 기능 bitmask. 노드별 의미를 적용하고 알 수 없는 bit를 임의 확정하지 않는다. | BCM 해당 구동·관측·보호 담당자 |
| `recovering` | `bool`; 저장 폭은 ABI 미검증 | 기능 회복 진행 bool. 링크 recovering 및 성공 완료와 별개. | BCM 해당 구동·관측·보호 담당자 |
| `sourceGeneration` | [`SourceGeneration_t`](CIS_REAR_DATA.md#type-SourceGeneration_t); 32비트 부호 없음 | 원본 관측/사건 생산 세대. 원 warning/fault 계보에 사용하고 마지막 송신 홉 Boot로 재라벨하지 않는다. | BCM 해당 구동·관측·보호 담당자 |
| `meta` | [`SignalMeta_t`](CIS_REAR_DATA.md#type-SignalMeta_t); 중첩 타입 참조; sizeof 미검증 | 원 생산자의 품질·원본 AGE·평가 번호. 이 메타는 링크 BOOT_ID/TX_SEQUENCE가 아니다. | BCM 해당 구동·관측·보호 담당자 |


<a id="type-DomainIf_CisPowerPermission_t"></a>
## `DomainIf_CisPowerPermission_t`

출처: `S4A data/T14.md#type-DomainIf_CisPowerPermission_t`, T14 L1024..1029.

```c
typedef struct
{
    bool vehiclePowerAllowed;
    DataQuality_t quality;
    SignalMeta_t meta;
} DomainIf_CisPowerPermission_t;
```

| 필드 | C 타입·폭 | 값·단위·유효 조건 | 생성·관리 주체 |
|---|---|---|---|
| `vehiclePowerAllowed` | `bool`; 저장 폭은 ABI 미검증 | Domain 전원 허용 bool. quality/meta 유효일 때만 허용 사용; 무효는 허용이 아니다. | Domain 기능 결정/설정/결과 담당자 |
| `quality` | [`DataQuality_t`](CIS_REAR_DATA.md#type-DataQuality_t); C enum 저장 폭 미검증 | 이 값의 품질. OK/STALE/INVALID/NO_DATA를 정상 값/위험 해제 여부와 분리한다. | Domain 기능 결정/설정/결과 담당자 |
| `meta` | [`SignalMeta_t`](CIS_REAR_DATA.md#type-SignalMeta_t); 중첩 타입 참조; sizeof 미검증 | 원 생산자의 품질·원본 AGE·평가 번호. 이 메타는 링크 BOOT_ID/TX_SEQUENCE가 아니다. | Domain 기능 결정/설정/결과 담당자 |


이 bool은 실제 정상 ALLOW/DENY와 UNKNOWN을 자체 구분하지 못한다. r1 [조건부 의미 투영](../flows/CAN_BCM_AND_PERMISSION.md#permission-projection)에 따라 실제 정상 true/OK→1/OK, false/OK→0/OK를 확인하고, DOMAIN이 원인·정책 채택을 입증한 false+NO_DATA/STALE/INVALID에만255+해당 품질을 대응한다. UNKNOWN+OK 무손실 보존은 기존 구조로 불가하므로 해당 경로 HOLD다. quality/meta.quality 충돌을 OK로 보정하지 않으며 새 reason/DTO 필드를 추가하지 않는다. meta.updateSequence는 연결된 DOMAIN 원 성공 평가를 보존하고 CAN 합성 UPDATE와 동일 번호라고 가정하지 않는다.

<a id="type-DomainIf_WindowOperationPermission_t"></a>
## `DomainIf_WindowOperationPermission_t`

출처: `S4A data/T14.md#type-DomainIf_WindowOperationPermission_t`, T14 L1140..1145.

```c
typedef struct
{
    bool operationAllowed;
    DataQuality_t quality;
    SignalMeta_t meta;
} DomainIf_WindowOperationPermission_t;
```

| 필드 | C 타입·폭 | 값·단위·유효 조건 | 생성·관리 주체 |
|---|---|---|---|
| `operationAllowed` | `bool`; 저장 폭은 ABI 미검증 | Domain WINDOW 동작 허용 bool. 품질/기한 유효할 때만 사용; 구현 보류. | Domain 기능 결정/설정/결과 담당자 |
| `quality` | [`DataQuality_t`](CIS_REAR_DATA.md#type-DataQuality_t); C enum 저장 폭 미검증 | 이 값의 품질. OK/STALE/INVALID/NO_DATA를 정상 값/위험 해제 여부와 분리한다. | Domain 기능 결정/설정/결과 담당자 |
| `meta` | [`SignalMeta_t`](CIS_REAR_DATA.md#type-SignalMeta_t); 중첩 타입 참조; sizeof 미검증 | 원 생산자의 품질·원본 AGE·평가 번호. 이 메타는 링크 BOOT_ID/TX_SEQUENCE가 아니다. | Domain 기능 결정/설정/결과 담당자 |


<a id="type-DomainIf_WindowDomainAlive_t"></a>
## `DomainIf_WindowDomainAlive_t`

출처: `S4A data/T14.md#type-DomainIf_WindowDomainAlive_t`, T14 L1160..1164.

```c
typedef struct
{
    bool domainLinkAvailable;
    SignalMeta_t meta;
} DomainIf_WindowDomainAlive_t;
```

| 필드 | C 타입·폭 | 값·단위·유효 조건 | 생성·관리 주체 |
|---|---|---|---|
| `domainLinkAvailable` | `bool`; 저장 폭은 ABI 미검증 | Domain↔WINDOW 링크 감독 근거 bool. 도메인 차량 정책 허용과 다름; 구현 보류. | DOMAIN 기능 Owner가 원 alive 평가를 생성하고 Network TxPort가 복사·보존한다. WINDOW는 조건부 수신자이며 실행 HOLD를 유지한다. |
| `meta` | [`SignalMeta_t`](CIS_REAR_DATA.md#type-SignalMeta_t); 중첩 타입 참조; sizeof 미검증 | DOMAIN 원 alive 평가의 품질·원본 AGE·평가 번호. 링크 BOOT_ID/TX_SEQUENCE로 만들지 않는다. | DOMAIN 기능 Owner 생성 → Network TxPort 복사·보존 → WINDOW 조건부 소비. 실제 생산자 바인딩·통합 Permission 대응·WINDOW 실행은 HOLD다. |


<a id="enum-mapping"></a>
## 명시 enum 변환과 수명

| Wire / Net raw | C2 이름 대응 | 범위 제한 |
|---|---|---|
| Door lock 0,1,255 | DOOR_LOCK_STATE_LOCKED, UNLOCKED, UNKNOWN | C2 UNKNOWN은 숫자0, Wire UNKNOWN은255 |
| Door open 0,1,255 | DOOR_OPEN_STATE_CLOSED, OPEN, UNKNOWN | UNKNOWN을 CLOSED로 합성하지 않음 |
| Door composite 0,1,2 | NORMAL, INCONSISTENT, UNTRUSTED | 각 품질을 독립 보존 |
| Door target 0,1 | DOOR_TARGET_LOCK, UNLOCK |255는 결과의 비도어에만 사용 |
| Fan 0,1,2,3,255 | FAN_LEVEL_OFF, LOW, MEDIUM, HIGH, UNKNOWN |255는 measured 상태에만 허용 |
| Thermal 0,1,2 | THERMAL_DIRECTION_IDLE, COOL, HEAT | 출력 퍼센트와 독립 |
| Heat 0,1,255 | HEAT_REMOVAL_STATE_NORMAL, BLOCKED_OVERHEAT, UNMEASURABLE | 과열 복구가 이전 출력을 재개하지 않음 |
| Light type 0,1,2,3 | INTERIOR_LIGHT_TYPE_NORMAL, GOODBYE, WARNING, FAULT | BCM_LIGHT의 실제 채택 profile 확인 |
| Light apply 0,1,255 | INTERIOR_LIGHT_APPLY_APPLIED, FAILED, UNKNOWN | 물리 피드백 지원과 구분 |
| RGB r/g/b 0..255 | RgbColor_t.red/green/blue |255도 유효 색상, sentinel 아님 |
| BCM kind 1,2,3 | BCM_COMMAND_KIND_DOOR, CLIMATE, INTERIOR_LIGHT | C2 숫자0,1,2에 직접 cast 금지 |
| Door event 0 | hasDoorEvidence=false | C2 evidence type에 NONE 없음 |
| Door event 1..5 | BCM_DOOR_EVIDENCE_LOCK_COMPLETED, UNLOCK_COMPLETED, ALREADY_AT_TARGET, REQUEST_REJECTED, REQUEST_FAILED | C2 숫자0..4; occurrence/age/quality/target 한 묶음 |
| Event kind 1,2,3 | BCM_PROTECTION_OVERHEAT_DETECTED, FAN_MISMATCH_DETECTED, RECOVERY_CONFIRMED | C2 숫자0,1,2 |
| Value quality 0,1,2,3 | DATA_QUALITY_OK, STALE, INVALID, NO_DATA | enum 최댓값으로 summary 만들지 않음 |
| Fault category 0,1,2,3 | NONE(표현 별도), FAULT_CATEGORY_SENSOR, FUNCTION, COMM | C2 SENSOR=0을 Wire NONE=0으로 오인 금지 |

RequestResult_t·ResultReason_t·CommandOrigin_t의 전체 정본은 공통 자료에 있다. RESULT_REASON raw0..25의 대응 순서는 NONE, NOT_REGISTERED, LINK_LOST, SESSION_INVALID, DOOR_OPEN, STATE_UNTRUSTED, INVALID_COMMAND, NO_FEEDBACK, DRIVE_LIMIT_EXCEEDED, ALREADY_AT_TARGET, LOCAL_OVERRIDE, STOP_REQUESTED, SUPERSEDED, ANTIPINCH, FAULT, STALE, NO_DATA, NOT_READY, OUT_OF_RANGE, SOURCE_FAILURE, UNKNOWN_REQUEST, EXPIRED, ID_CONFLICT, POWER_NOT_ALLOWED, OVERHEAT, FAN_MISMATCH이다. 같은 접두 `RESULT_REASON_`의 이름으로 변환하며 C2 전용 NOT_ALLOWED/MOTOR_FAULT/SENSOR_FAULT/TIMEOUT/OTHER의 raw를 새로 만들지 않는다. 출처 `B07 evidence/sources/T39.txt`.

모든 const 입력은 호출 동안만 빌린다. 비동기 처리에는 필드와 식별자를 자기 소유 메모리에 복사한 뒤에만 로컬 접수 OK를 반환한다. Network가 원본 typed 값/품질을 새로 발급하지 않는다. ClimateState_t(12멤버)와 InteriorLightState_t(11멤버)는 sourceGeneration을 포함하는 후보 선언이다. 환경 DTO·공개 API·CAN/UART Wire는 변경하지 않는다. C enum·bool·padding·sizeof·offsetof·VehicleState_t aggregate와 호출 ABI는 동일 헤더를 사용한 실제 전체 재빌드 전 미검증이며 크기가 4바이트씩 증가한다고 단정하지 않는다.
