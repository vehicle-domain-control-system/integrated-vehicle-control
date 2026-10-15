# UART-C 자료 정본과 내부 인계 자료

[UART-C 개요](../06_UART_C_OVERVIEW.md) · [Frame/Router](../functions/UART_C_FRAME_ROUTER.md) · [전송·수신](../functions/UART_C_TX_RX_CONTROL.md)

S4A T25의 기존 5개 선언(3 DTO·고정폭 고장 타입·상수 집합)을 아래에 보존한다. 원문은 `S4A_RESULT/data/T25.md`와 `evidence/sources/T25.txt`이며 코드 선언은 변경하지 않고 탐색 링크만 현재 정본으로 연결했다. 자료 이름의 존재는 제품 헤더/ABI 구현 증거가 아니다. UART-C payload25필드는 각 메시지 문서가 정확 offset·폭·LE로 직렬화한다.

| 재사용 기존 자료 | 현재 단일 정본 |
|---|---|
| NetSubmitResult | [CIS_REAR_DATA](CIS_REAR_DATA.md#type-NetSubmitResult) |
| NetCanRxMeta | [CIS_REAR_DATA](CIS_REAR_DATA.md#type-NetCanRxMeta) |
| NetUartRxMeta | [UART_M_DATA](UART_M_DATA.md#type-NetUartRxMeta) — 선언만 공통, M 전용 함수 호출 권한 아님 |
| NetDomainPermission | [CAN_BCM_DATA](CAN_BCM_DATA.md#type-NetDomainPermission) — quality_reason 멤버 없음 |
| NetCisOccupant | [CAN_NODE_DATA](CAN_NODE_DATA.md#type-NetCisOccupant) |
| NetTriState / NetValueQuality / NetSensorFunctionState | [CAN_COMMON_DATA](CAN_COMMON_DATA.md) |
| NetValidity / NetQualityReason | [CIS_REAR_DATA](CIS_REAR_DATA.md) |

허용 중계의 내부 근거 자료 `NetUartCPermissionRelayEvidence`와 사용 판정 자료 `NetUartCPermissionUse`·`NetUartCPermissionUseEvidence`의 단일 선언은 [CIS→Pi permission 흐름](../flows/UART_C_CIS_TO_PI_PERMISSION.md#relay-projection)에 있다. 이들은 문서 내부의 조건부 판정 자료이며 기존 공개 API나 제품 헤더에 추가한 형식이 아니다.

`NetCisOccupant`에는 presence와 count 각각 validity/reason이 있다. 각 값에 대해 Pi 원 사유와 CIS 추가 사유를 동시에 별도 전달할 필드는 없다는 것이 IF-O13/19의 표현 손실이다. 전체 메시지 품질이 한 벌뿐이라는 뜻이 아니다.

<a id="type-NetVisionResult"></a>
## `NetVisionResult`

```c
typedef struct NetVisionResult
{
    uint32_t pi_boot_id; /**< PI_BOOT_ID: Pi runtime 단일 기동 owner가 local admission에서 고정 */
    uint32_t vision_sequence; /**< VISION_SEQUENCE: 새 카메라 판정 번호 */
    uint16_t vision_age_ms; /**< VISION_AGE: 판정 원본 후 ms */
    NetTriState occupant_presence; /**< OCCUPANT_PRESENCE: 0=없음; 1=있음; 255=미확인 */
    NetValidity presence_validity; /**< PRESENCE_VALIDITY: 0=INVALID; 1=VALID */
    NetQualityReason presence_reason; /**< PRESENCE_REASON: Pi 원 비전 품질 사유 */
    uint8_t occupant_count; /**< OCCUPANT_COUNT: 0~5명,255=미확인 */
    NetValidity count_validity; /**< COUNT_VALIDITY: 0=INVALID; 1=VALID */
    NetQualityReason count_reason; /**< COUNT_REASON: Pi 원 비전 품질 사유 */
    NetSensorFunctionState vision_status; /**< VISION_STATUS: Pi 비전 기능 상태 */
    NetVisionFaultCode vision_fault_code; /**< VISION_FAULT_CODE: 0=없음; 1=초기화; 2=카메라; 3=판정; 4=데이터 오류 */
} NetVisionResult;
```

근거: T25 26–39행. 정본은 이 절이다.

| 필드 | C 타입·폭 | 값·단위·유효 조건 | 생성·관리 주체 |
|---|---|---|---|
| `pi_boot_id` | `uint32_t`; 32비트 부호 없음 | Pi 실행 환경 단일 기동 담당자가 로컬 복사 접수에서 고정 | Pi 실행 환경 단일 기동 담당자. 송신 App 초기화 의무 없음; Pi Network가 로컬 복사 접수 때 현재 관측 세대와 결합하고 송신 전 같은 세대인지 확인. CIS 수신에서는 원 값을 보존. |
| `vision_sequence` | `uint32_t`; 32비트 부호 없음 | 새 카메라 판정 번호 | Pi 비전/상태 담당자의 새 평가 번호. 관측 sequence와 상태 sequence 독립, 반복 UART frame으로 증가 금지. |
| `vision_age_ms` | `uint16_t`; 16비트 부호 없음 | 판정 원본 후 ms | Pi 원 판정/상태 평가 후 ms. App이 현재 원본 생성 후 경과 시간 제공; Pi Network 대기만 한 번 누적·65535 포화. |
| `occupant_presence` | [`NetTriState`](CAN_COMMON_DATA.md#type-NetTriState); 8비트 부호 없음 | 0=없음; 1=있음; 255=미확인 | Pi 비전 기능 담당자. presence/count 품질 독립; 상태 READY/복구만으로 과거 관측을 새 VALID로 만들지 않음. |
| `presence_validity` | [`NetValidity`](CIS_REAR_DATA.md#type-NetValidity); 8비트 부호 없음 | 0=INVALID; 1=VALID | Pi 비전 담당자의 해당 필드 원 품질·사유. CIS는 원 record를 보존하고 전달 평가로만 추가 제한한다. |
| `presence_reason` | [`NetQualityReason`](CIS_REAR_DATA.md#type-NetQualityReason); 8비트 부호 없음 | Pi 원 비전 품질 사유 | Pi 비전 담당자의 해당 필드 원 품질·사유. CIS는 원 record를 보존하고 전달 평가로만 추가 제한한다. |
| `occupant_count` | `uint8_t`; 8비트 부호 없음 | 0~5명,255=미확인 | Pi 비전 기능 담당자. presence/count 품질 독립; 상태 READY/복구만으로 과거 관측을 새 VALID로 만들지 않음. |
| `count_validity` | [`NetValidity`](CIS_REAR_DATA.md#type-NetValidity); 8비트 부호 없음 | 0=INVALID; 1=VALID | Pi 비전 담당자의 해당 필드 원 품질·사유. CIS는 원 record를 보존하고 전달 평가로만 추가 제한한다. |
| `count_reason` | [`NetQualityReason`](CIS_REAR_DATA.md#type-NetQualityReason); 8비트 부호 없음 | Pi 원 비전 품질 사유 | Pi 비전 담당자의 해당 필드 원 품질·사유. CIS는 원 record를 보존하고 전달 평가로만 추가 제한한다. |
| `vision_status` | [`NetSensorFunctionState`](CAN_COMMON_DATA.md#type-NetSensorFunctionState); 8비트 부호 없음 | Pi 비전 기능 상태 | Pi 비전 기능 담당자. presence/count 품질 독립; 상태 READY/복구만으로 과거 관측을 새 VALID로 만들지 않음. |
| `vision_fault_code` | [`NetVisionFaultCode`](#type-NetVisionFaultCode); 16비트 부호 없음 | 0=없음; 1=초기화; 2=카메라; 3=판정; 4=데이터 오류 | Pi 비전 기능 담당자. presence/count 품질 독립; 상태 READY/복구만으로 과거 관측을 새 VALID로 만들지 않음. |

사용 타입: [`NetQualityReason`](CIS_REAR_DATA.md#type-NetQualityReason), [`NetSensorFunctionState`](CAN_COMMON_DATA.md#type-NetSensorFunctionState), [`NetTriState`](CAN_COMMON_DATA.md#type-NetTriState), [`NetValidity`](CIS_REAR_DATA.md#type-NetValidity), [`NetVisionFaultCode`](#type-NetVisionFaultCode).

직접 사용 API: [`CisApp_OnPiVisionResult`](../functions/uart_c/P_VISION_RESULT.md#api-CisApp_OnPiVisionResult), [`PiNet_PublishVisionResult`](../functions/uart_c/P_VISION_RESULT.md#api-PiNet_PublishVisionResult).

[색인](#type-NetVisionResult)

<a id="type-NetVisionStatus"></a>
## `NetVisionStatus`

```c
typedef struct NetVisionStatus
{
    uint32_t pi_boot_id; /**< PI_BOOT_ID: Pi runtime 단일 기동 owner가 local admission에서 고정 */
    uint32_t status_sequence; /**< STATUS_SEQUENCE: 새 기능 상태 평가 */
    NetSensorFunctionState vision_status; /**< VISION_STATUS: Pi 비전 기능 상태 */
    NetVisionFaultCode active_fault; /**< ACTIVE_FAULT: 비전 고장 코드 */
    NetVisionFaultCode last_fault; /**< LAST_FAULT: Pi 이번 기동 이후 최근 비전 고장 */
    uint16_t status_age_ms; /**< STATUS_AGE: 상태 평가 후 ms */
    uint8_t affected_function; /**< AFFECTED_FUNCTION: bit0=탑승자 존재; bit1=인원수 */
} NetVisionStatus;
```

근거: T25 63–72행. 정본은 이 절이다.

| 필드 | C 타입·폭 | 값·단위·유효 조건 | 생성·관리 주체 |
|---|---|---|---|
| `pi_boot_id` | `uint32_t`; 32비트 부호 없음 | Pi 실행 환경 단일 기동 담당자가 로컬 복사 접수에서 고정 | Pi 실행 환경 단일 기동 담당자. 송신 App 초기화 의무 없음; Pi Network가 로컬 복사 접수 때 현재 관측 세대와 결합하고 송신 전 같은 세대인지 확인. CIS 수신에서는 원 값을 보존. |
| `status_sequence` | `uint32_t`; 32비트 부호 없음 | 새 기능 상태 평가 | Pi 비전/상태 담당자의 새 평가 번호. 관측 sequence와 상태 sequence 독립, 반복 UART frame으로 증가 금지. |
| `vision_status` | [`NetSensorFunctionState`](CAN_COMMON_DATA.md#type-NetSensorFunctionState); 8비트 부호 없음 | Pi 비전 기능 상태 | Pi 비전 기능 담당자. presence/count 품질 독립; 상태 READY/복구만으로 과거 관측을 새 VALID로 만들지 않음. |
| `active_fault` | [`NetVisionFaultCode`](#type-NetVisionFaultCode); 16비트 부호 없음 | 비전 고장 코드 | Pi 비전 기능 담당자. presence/count 품질 독립; 상태 READY/복구만으로 과거 관측을 새 VALID로 만들지 않음. |
| `last_fault` | [`NetVisionFaultCode`](#type-NetVisionFaultCode); 16비트 부호 없음 | Pi 이번 기동 이후 최근 비전 고장 | Pi 이번 기동 이후 최근 기능 고장. current active 해제와 구분. |
| `status_age_ms` | `uint16_t`; 16비트 부호 없음 | 상태 평가 후 ms | Pi 원 판정/상태 평가 후 ms. App이 현재 원본 생성 후 경과 시간 제공; Pi Network 대기만 한 번 누적·65535 포화. |
| `affected_function` | `uint8_t`; 8비트 부호 없음 | bit0=탑승자 존재; bit1=인원수 | Pi 비전 기능 담당자. presence/count 품질 독립; 상태 READY/복구만으로 과거 관측을 새 VALID로 만들지 않음. |

사용 타입: [`NetSensorFunctionState`](CAN_COMMON_DATA.md#type-NetSensorFunctionState), [`NetVisionFaultCode`](#type-NetVisionFaultCode).

직접 사용 API: [`CisApp_OnPiVisionStatus`](../functions/uart_c/P_VISION_STATUS.md#api-CisApp_OnPiVisionStatus), [`PiNet_PublishVisionStatus`](../functions/uart_c/P_VISION_STATUS.md#api-PiNet_PublishVisionStatus).

[색인](#type-NetVisionStatus)

<a id="type-NetPiPermission"></a>
## `NetPiPermission`

```c
typedef struct NetPiPermission
{
    uint32_t cis_boot_id; /**< CIS_BOOT_ID: CIS runtime이 채우는 중계 세대 */
    uint32_t update_sequence; /**< UPDATE_SEQUENCE: CIS 허용 중계의 새 의미 평가 번호; 반복 프레임에서는 유지 */
    NetTriState vehicle_power_permission; /**< VEHICLE_POWER_PERMISSION: 0=불허; 1=허용; 255=확인 불가 */
    NetValueQuality value_quality; /**< VALUE_QUALITY: 허용 상태 품질 */
    NetQualityReason quality_reason; /**< QUALITY_REASON: 품질 사유 */
    uint16_t use_limit_ms; /**< USE_LIMIT: 초안 1500ms */
    uint16_t source_age_ms; /**< SOURCE_AGE: Domain 허용 판단 후 누적 ms */
} NetPiPermission;
```

근거: T25 92–101행. 정본은 이 절이다.

| 필드 | C 타입·폭 | 값·단위·유효 조건 | 생성·관리 주체 |
|---|---|---|---|
| `cis_boot_id` | `uint32_t`; 32비트 부호 없음 | CIS 실행 환경이 채우는 중계 세대 | CIS 실행 환경. 원 DOMAIN Boot가 아니며 Pi는 수용된 CIS 세대 기준으로 stale relay를 배제. |
| `update_sequence` | `uint32_t`; 32비트 부호 없음 | CIS 허용 중계의 새 의미 평가 번호; 반복 프레임에서는 유지 | CIS private relay의 별도 의미·제한 revision 후보. DOMAIN 원 성공 Sequence 및 CAN 합성 UPDATE와 구별; 실제 새 원 평가/수용 세대/제한 변화 반영, 단순500ms 반복 유지. |
| `vehicle_power_permission` | [`NetTriState`](CAN_COMMON_DATA.md#type-NetTriState); 8비트 부호 없음 | 0=불허; 1=허용; 255=확인 불가 | DOMAIN 원 판단에서 파생. CIS가 확인하지 않은 허용을 새로 생성하지 않음. |
| `value_quality` | [`NetValueQuality`](CAN_COMMON_DATA.md#type-NetValueQuality); 8비트 부호 없음 | 허용 상태 품질 | 원 DOMAIN power 품질 + CIS 수신/만료 검사. 전송 가능한 보수 평가만; 품질 제한/사유 우선순위 TEAM_DECISION_REQUIRED. |
| `quality_reason` | [`NetQualityReason`](CIS_REAR_DATA.md#type-NetQualityReason); 8비트 부호 없음 | 품질 사유 | 원 DOMAIN power 품질 + CIS 수신/만료 검사. 전송 가능한 보수 평가만; 품질 제한/사유 우선순위 TEAM_DECISION_REQUIRED. |
| `use_limit_ms` | `uint16_t`; 16비트 부호 없음 | 초안 1500ms | 기존 CIS UART-C 후보1500ms. 같은 후방 Power 원 판단을 양끝이 전달·소비한다는 근거가 있을 때만500ms 후보 변경. 원500ms 합의와 독립이며 양끝 확인 전 송신값을 자동 변경하지 않음. |
| `source_age_ms` | `uint16_t`; 16비트 부호 없음 | Domain 허용 판단 후 누적 ms | DOMAIN 원 판단 Age + CAN 수신 후 CIS relay 대기. 한 번 누적; Pi 현재 Age는 여기에 Pi 수신 후 경과를 더함. |

사용 타입: [`NetQualityReason`](CIS_REAR_DATA.md#type-NetQualityReason), [`NetTriState`](CAN_COMMON_DATA.md#type-NetTriState), [`NetValueQuality`](CAN_COMMON_DATA.md#type-NetValueQuality).

원 성공17 뒤 실패에서도 원17·원 시각은 유지하고, 확인된 현재 제한의 합성/relay 번호만 조건부로 변경한다. [번호 공간](../flows/CAN_BCM_AND_PERMISSION.md#permission-sequences)의 원 성공번호를 이 update_sequence나 UART LINK로 복원하지 않는다. UNKNOWN 조건부 투영과 실패 즉시 제한은 DOMAIN 정책 OPEN이며 기존 DTO에 새 원인/원 번호 필드를 추가하지 않는다.

직접 사용 API: [`PiApp_OnCisPermission`](../functions/uart_c/P_PERMISSION.md#api-PiApp_OnCisPermission).

[색인](#type-NetPiPermission)

<a id="type-NetVisionFaultCode"></a>
## `NetVisionFaultCode`

```c
typedef uint16_t NetVisionFaultCode;
```

근거: T25 120–120행. 정본은 이 절이다.

값 저장 폭: 16비트 부호 없음. NULL은 값 자체에 적용하지 않으며 이 타입을 가리키는 API 포인터에 적용한다.

허용 코드·예약값은 [`NetVisionFaultCodeCode`](#type-NetVisionFaultCodeCode)의 상수 집합을 따른다. 정의되지 않은 코드값을 정상값으로 변환하지 않는다.

이 타입을 포함하는 자료: [`NetVisionResult`](#type-NetVisionResult), [`NetVisionStatus`](#type-NetVisionStatus). 포함 타입을 통해 API로 연결된다.

[색인](#type-NetVisionFaultCode)

<a id="type-NetVisionFaultCodeCode"></a>
## `NetVisionFaultCodeCode`

```c
enum NetVisionFaultCodeCode
{
    NET_VISION_FAULT_NONE = 0,
    NET_VISION_FAULT_INITIALIZATION = 1,
    NET_VISION_FAULT_CAMERA = 2,
    NET_VISION_FAULT_INFERENCE = 3,
    NET_VISION_FAULT_DATA_INVALID = 4
};
```

근거: T25 121–128행. 정본은 이 절이다.

이 tag는 [`NetVisionFaultCode`](#type-NetVisionFaultCode)에 쓰는 상수 집합이다. `enum NetVisionFaultCodeCode`와 해당 정수 typedef는 서로 다른 C 타입이다.

| 상수 | 값·의미 |
|---|---|
| `NET_VISION_FAULT_NONE` | `0` · 없음 |
| `NET_VISION_FAULT_INITIALIZATION` | `1` · 비전 초기화 오류 |
| `NET_VISION_FAULT_CAMERA` | `2` · 카메라 오류 |
| `NET_VISION_FAULT_INFERENCE` | `3` · 판정 처리 오류 |
| `NET_VISION_FAULT_DATA_INVALID` | `4` · 비전 결과 형식/내용 오류 |

직접 API 인자 없음. 의미/상수 집합이며 새 API를 추정하지 않는다.

[색인](#type-NetVisionFaultCodeCode)


## UART-C private 자료의 적용 범위

아래 선언은 전부 `PROPOSED_PRIVATE_TYPE`이다. 실제 공개 API/헤더·새 Wire 필드·Driver ABI가 아니다. UART-M의 알고리즘 의미를 참고했지만 `NetUartM*`와 저장소/registry를 공유한다는 구현 가정은 없다. 실제 할당 크기·Task·ISR handoff·lock·callback 연결은 Owner 바인딩 전 HOLD다.

<a id="type-NetUartCResult"></a>
```c
typedef enum NetUartCResult {
    NET_UART_C_OK, NET_UART_C_INVALID_ARGUMENT, NET_UART_C_MALFORMED,
    NET_UART_C_CONTRACT_HOLD, NET_UART_C_CONTEXT_HOLD,
    NET_UART_C_ORDER_HOLD, NET_UART_C_TIME_HOLD,
    NET_UART_C_NOT_READY, NET_UART_C_NO_CAPACITY, NET_UART_C_DUPLICATE,
    NET_UART_C_NEED_MORE, NET_UART_C_NO_WORK,
    NET_UART_C_DRIVER_PENDING, NET_UART_C_DRIVER_REJECTED
} NetUartCResult;
```

OK는 각 함수의 로컬 완료만 뜻한다. NEED_MORE는 입력 소비량과 별개이며 OUT Frame은 불변이다. DUPLICATE는 새 의미 관측 없음, HOLD는 확인되지 않은 근거로 정상 기능 소비를 허용하지 않음, MALFORMED는 실제 표현 오류다. 정상 열화 품질은 MALFORMED가 아니다. 이 private 결과를 공개 NetSubmitResult로 숫자 cast하지 않는다.

<a id="type-NetUartCDirection"></a>
<a id="type-NetUartCProfile"></a>
<a id="type-NetUartCRegistryEntry"></a>
```c
typedef enum NetUartCDirection { NET_UART_C_PI_TO_CIS, NET_UART_C_CIS_TO_PI } NetUartCDirection;
typedef enum NetUartCProfile { NET_UART_C_PROFILE_UNSELECTED,
    NET_UART_C_PROFILE_V1_CANDIDATE } NetUartCProfile;
typedef struct NetUartCRegistryEntry {
    uint8_t type; uint16_t payloadLength; NetUartCDirection direction;
} NetUartCRegistryEntry;
```

direction은 실제 로컬 endpoint/port로 확인한 방향이다. profile의 enum 선택이나 bool=true는 양단 채택 증거가 아니다. 버전/3종 schema/실행문맥의 같은 revision 채택 근거를 별도 확인해야 한다. 구 2종 후보와의 자동 판별은 없다.

<a id="type-NetUartCFrame"></a>
<a id="type-NetUartCStreamInput"></a>
<a id="type-NetUartCParser"></a>
```c
typedef struct NetUartCFrame {
    uint16_t length; uint32_t receivedAtMs; uint8_t bytes[138];
} NetUartCFrame;
typedef struct NetUartCStreamInput {
    const uint8_t *bytes; const uint32_t *capturedAtMs;
    uint16_t length; uint32_t linkEpoch, nowMs;
    bool elapsedUnambiguous, timerCheckPermitted;
} NetUartCStreamInput;
typedef struct NetUartCParser {
    uint8_t bytes[138]; uint32_t capturedAtMs[138]; uint16_t count;
    uint32_t linkEpoch; bool timePolicyBound;
    uint32_t assemblyLimitMs; bool expiresAtEquality;
    uint32_t malformedHeaders, badCrc, assemblyTimeouts, byteLosses;
} NetUartCParser;
```

138은 P≤128+외형10의 논리 상한이며 제품 저장소 설정이 아니다. 현재 등록 P는19/16/15, 최대 Frame29다. RX의 receivedAtMs는 마지막 Frame byte의 실제 로컬 관측이며 worker 처리시각으로 대체하지 않는다. TX에서는 이 필드를 읽지 않는다. 수신 byte와 capturedAtMs를 같은 index로 안정 복사하고 suffix 이동 때 함께 이동한다. Driver가 이 관측점을 제공할 수 있는지는 OPEN이다. bool 입력들은 실제 근거의 평가 결과이지 없던 clock/양단 합의를 생성하는 설정값이 아니다.

elapsedUnambiguous는 같은 로컬 clock·wrap·관측 간격의 명확성을 뜻한다. timerCheckPermitted는 nowMs 이전에 도착한 byte가 upstream에 더 남아 있지 않다는 근거다. 조립50ms와 경계 > 또는 ≥는 채택 미확정이며 timePolicyBound 전에는 TIME_HOLD다. count/진단 counter는 원 Pi 관측 cache가 아니다. 진단은 u32 포화, 정상 수신/상대 reboot에서 임의 초기화하지 않는다.

<a id="type-NetUartCDecodedValue"></a>
<a id="type-NetUartCDecoded"></a>
```c
typedef union NetUartCDecodedValue {
    NetVisionResult visionResult; NetVisionStatus visionStatus; NetPiPermission permission;
} NetUartCDecodedValue;
typedef struct NetUartCDecoded {
    uint8_t type; NetUartCDirection direction;
    NetUartRxMeta meta; NetUartCDecodedValue value;
} NetUartCDecoded;
```

type에 해당하는 union 멤버만 읽는다. 임시 DTO와 meta는 하나의 실제 수신 사건에 묶이고 callback 반환까지만 borrowed다. 비동기 소비자는 전체 값/품질/원 identity/AGE/meta를 함께 복사한다. private union을 memcpy하여 Wire로 보내지 않는다.

<a id="type-NetUartCPortOutcome"></a>
<a id="type-NetUartCTxRecord"></a>
<a id="type-NetUartCPortEvent"></a>
```c
typedef enum NetUartCPortOutcome {
    NET_UART_C_TX_UNKNOWN, NET_UART_C_TX_SUCCEEDED, NET_UART_C_TX_FAILED
} NetUartCPortOutcome;
typedef struct NetUartCTxRecord {
    uint64_t localToken; uint32_t localEpoch;
    bool acceptedKnown, accepted, driverReferenceOutstanding;
    bool bufferReturned, identityRetained;
    NetUartCPortOutcome outcome;
} NetUartCTxRecord;
typedef struct NetUartCPortEvent {
    uint64_t localToken; uint32_t localEpoch;
    bool admissionObserved, accepted, releaseObserved;
    NetUartCPortOutcome outcome;
} NetUartCPortEvent;
```

localToken/epoch는 제안 로컬 제출수명 식별이다. LINK_SEQUENCE/PiBoot/CISBoot/원 DOMAIN 판단을 callback token으로 대신하지 않는다. 실제 SDK가 이러한 identity와 release/terminal을 제공하는 방식은 HOLD다. record는 pointer-free 의미/메타 사본과 immutable Frame 소유를 동반해야 하며 이 구조만 복사해 접수 완료라 하지 않는다. release가 먼저면 메모리는 반환 가능해도 outcome 미정 identity는 보존한다.

<a id="type-NetUartCRxComparison"></a>
```c
typedef struct NetUartCRxComparison {
    bool contextBound, sameContext, transitionAuthorized;
    bool hasAcceptedFrame, nonWrappingOrderKnown;
    uint16_t previousLinkSequence, incomingLinkSequence;
    bool rawExactlyEqual, sameImmutableInput, semanticExactlyEqual;
    bool functionalTransitionKnown, ageRelationAccepted;
} NetUartCRxComparison;
```

M04가 실제 원 Boot/순서·같은 관측·AGE 근거를 대조한 입력이다. permission의 sameImmutableInput은 원 DOMAIN association과 동일한 relay 의미·현재 제한 상태까지 입증해야 하며 CIS relay 번호만 같아서는 부족하다. 같은 원 성공의 확인된 제한 변경은 새 relay revision/기능 전이 근거로 분리하고 원 성공번호만 비교해 DUPLICATE로 버리지 않는다. 같은 relay revision에서 의미만 바뀐 자료를 임의 통과시키지는 않는다. VisionResult와 VisionStatus는 독립 sequence 공간이다. 순서 판정은 [수신 제어](../functions/UART_C_TX_RX_CONTROL.md#order)를 따른다. 동일 Link/raw와 새 Link/동일 원 관측은 모두 기존 DUPLICATE를 반환하지만, 호출자는 분류에 사용한 안정 comparison의 Link 차이로 구분한다. comparison·원 raw·typed/meta·보수 AGE는 분류부터 게시/callback까지 같은 직렬 구간에 고정한다. 정확한 중복은 수신감시를 갱신하지 않고 유효 Vision 반복은 기존 callback과 해당 메시지 감시에 연결한다. raw 사본의 AGE와 현재 소비시점 보수 AGE는 분리 보관한다.

<a id="type-NetVisionRestrictionKind"></a>
<a id="type-NetVisionRestrictionEvidence"></a>
## CIS 현재 제한의 private 근거

아래 두 선언은 PG6-SW01의 **PROPOSED_PRIVATE_TYPE**이다. 기존 원 Pi record와 M09 통신 관측·수용된 Status를 같은 관측에 결합하기 위한 호출 중 자료이며 제품 Header·CAN/UART 필드·새 상태 Manager가 아니다.

```c
typedef enum NetVisionRestrictionKind {
    NET_VISION_RESTRICTION_NONE,
    NET_VISION_RESTRICTION_RX_INTERRUPTED,
    NET_VISION_RESTRICTION_ORIGIN_EXPIRED,
    NET_VISION_RESTRICTION_BOOT_CHANGED,
    NET_VISION_RESTRICTION_FUNCTION_FAULT,
    NET_VISION_RESTRICTION_VALUE_CONFLICT
} NetVisionRestrictionKind;

typedef struct NetVisionRestrictionEvidence {
    NetVisionRestrictionKind kind;
    uint32_t sourcePiBoot, sourceVisionSequence;
    NetUartRxMeta sourceMeta;
    uint64_t eventIdentity;
    uint32_t linkEpoch, clockEpoch, observedAtMs;
    uint8_t affectedFunctions;       /* bit0 Presence, bit1 Count */
    NetQualityReason mappedReason;   /* 실제 사건에 대응하는 기존 reason */
    uint32_t elapsedMs, limitMs;     /* 중단 또는 원 사용기한의 실제 근거 */
    bool limitBound, expiresAtEquality;
    uint32_t currentPiBoot;          /* 수용된 기동 전이에만 사용 */
    NetVisionStatus status;          /* 기능 Fault이면 수용된 상태 전 필드 */
    NetUartRxMeta statusMeta;
} NetVisionRestrictionEvidence;
```

호출자는 M01/M03의 CIS 원 관측 소유자다. `restrictions`는 Presence/Count 순서의 현재 제한 두 안정 사본을 가리킨다. NONE/NULL은 보존된 현재 제한이 없음을 같은 원 관측의 담당 기록에서 확인한 경우에만 허용한다. 이미 적용한 제한을 사본에서 누락하거나 READY·재접속·늦은 VALID를 이유로 지우지 않는다. 해제는 실제 새 원 평가와 확인된 복구 근거를 함께 요구하며, 근거가 없으면 현재 제한 또는 HOLD를 유지한다. 원 record·현재 제한·meta는 검사와 기존 `CisNet_PublishOccupant`의 owned-copy 인수까지 같은 직렬 구간에서 바뀌지 않는다. 입력은 borrowed/read-only이고 검사 함수는 저장하지 않는다.

| kind | 생성에 필요한 실제 자료 / 읽는 멤버 | 실패·제한 |
|---|---|---|
| RX_INTERRUPTED | M09가 해당 Result의 마지막 **수용된 새 Link**와 관측한 중단 사건, 같은 link/clock epoch의 observedAtMs·elapsedMs·채택된 limitMs/경계 | 정확한 중복·CRC 오류·Status만 수신한 사건으로 Result 중단을 복구하지 않음. 상한/경계 미채택이면 생성하지 않음 |
| ORIGIN_EXPIRED | 보존한 Pi 원 관측·같은 기준으로 누적한 현재 AGE, 해당 기능에서 채택한 원 limitMs/경계 | 새 frame을 위조하지 않음. 제한용 경과는 source AGE를 포함한 값이며 원 AGE와 중복 합산하지 않음 |
| BOOT_CHANGED | M04가 실제 수용한 새 PiBoot와 종료된 sourcePiBoot의 연결·fence 사건 | 숫자만 다른 Boot나 임의 랜덤값은 사건 근거가 아님. 구 Pi 관측을 새 Boot로 수정하지 않음 |
| FUNCTION_FAULT | 실제로 수용된 최신 `status` 전 필드/`statusMeta`, 동일 Pi 기동·선행 원 관측에 대한 이후 기능 사건·affected_function | READY/RECOVERING 또는 다른 기능 Fault는 이 사유로 만들지 않음. StatusSequence를 VisionSequence로 복사하지 않음 |
| VALUE_CONFLICT | 같은 원 판정에서 두 필드 각각 사용 가능, ABSENT/count>0 또는 PRESENT/count=0을 확인한 CIS 평가 사건 | Count255/INVALID 등 한쪽 비사용이면 생성 금지; 기존 DATA_INVALID(9)만 사용 |

`eventIdentity`는 보존된 실제 M09/M04 또는 CIS 기능 평가 기록의 로컬 identity다. 임의 nonzero는 증거가 아니며 검사 본문은 그 기록의 원 관측·meta·epoch·시점·영향 필드와 이 사본이 일치하는지 대조한다. 기록 생산/저장/동기화의 제품 연결은 **BINDING_NOT_VERIFIED**다. mappedReason은 해당 kind/영향 필드에 대한 선택 Interface의 기존 코드·확인된 매핑에서만 가져온다. 여러 동시 사건의 대표 사유 우선순위가 정해지지 않았다면 하나를 임의 선택하지 않고 투영 HOLD로 남긴다. Wire 한 쌍으로 원 사유와 모든 현재 사건이 전달된다고 주장하지 않는다.
