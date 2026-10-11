# Pi Data — 언어 중립 비전 record와 허용

[전체 시작](../../00_START_HERE.md) · [개요](00_OVERVIEW.md) · [Data](10_DATA.md) · [API](20_API.md)

**PROPOSED_DESIGN · BINDING_OPEN.** 아래 전체 선언은 의미 타입을 표시하는 설계용 C 표현이다. 실제 설치 헤더/firmware가 아니며 struct padding·enum 크기를 raw bytes로 전송하지 않는다. [공통 ID·수명](../../04_COMMON/10_DATA_AND_ID_RULES.md)과 [C enum↔Wire 명시 변환](../../04_COMMON/30_ENUM_WIRE_MAPPING.md)을 함께 적용한다.

Pi의 실제 구현 언어는 미정이다. 각 `NetVision*`/`NetPiPermission`을 아래 멤버·폭·범위가 같은 언어 중립 record로 매핑한다. 코드 블록의 C 타입은 의미 폭의 참조 표현이며 Python/C++/C 구현을 강제하지 않는다.

## 타입 빠른 찾기

| 타입 | 생성·소비 / 관련 메시지 |
|---|---|
| [`NetVisionResult`](#type-netvisionresult) | Pi 비전 판정 → CIS · [`P_VISION_RESULT`](../../03_COMMUNICATION/30_UART_C.md#message-p-vision-result) |
| [`NetVisionStatus`](#type-netvisionstatus) | Pi 비전 기능/고장 → CIS · [`P_VISION_STATUS`](../../03_COMMUNICATION/30_UART_C.md#message-p-vision-status) |
| [`NetPiPermission`](#type-netpipermission) | DOMAIN 원 허용 → CIS 내부 중계 → Pi · [`P_PERMISSION`](../../03_COMMUNICATION/30_UART_C.md#message-p-permission) |
| [`NetVisionFaultCode`](#type-netvisionfaultcode) | 노드 전용 코드·bit 집합 |

이 문서와 로컬 API가 사용하는 공용 타입의 전체 선언: [`NetSubmitResult`](../../04_COMMON/10_DATA_AND_ID_RULES.md#type-netsubmitresult), [`NetUartRxMeta`](../../04_COMMON/10_DATA_AND_ID_RULES.md#type-netuartrxmeta), [`NetValueQuality`](../../04_COMMON/10_DATA_AND_ID_RULES.md#type-netvaluequality), [`NetValidity`](../../04_COMMON/10_DATA_AND_ID_RULES.md#type-netvalidity), [`NetQualityReason`](../../04_COMMON/10_DATA_AND_ID_RULES.md#type-netqualityreason), [`NetTriState`](../../04_COMMON/10_DATA_AND_ID_RULES.md#type-nettristate), [`NetSensorFunctionState`](../../04_COMMON/10_DATA_AND_ID_RULES.md#type-netsensorfunctionstate).

<a id="type-netvisionresult"></a>
## `NetVisionResult`

Pi 비전 판정 → CIS · [`P_VISION_RESULT`](../../03_COMMUNICATION/30_UART_C.md#message-p-vision-result)

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

**로컬 사용:** [`PiNet_PublishVisionResult()`](20_API.md#api-pinet_publishvisionresult) · 전체 타입/인자·호출자·수명·실패는 API에서 확인한다.

| 멤버 | 정확한 C 타입 | 값·단위·범위 / 역할 | 원 소유자·유효 조건 |
|---|---|---|---|
| `pi_boot_id` | `uint32_t` | Pi runtime 단일 기동 owner가 local admission에서 고정 | Pi runtime 단일 기동 owner. 송신 App 초기화 의무 없음; Pi Network가 local admission 때 현재 관측 세대와 결합하고 송신 전 같은 세대인지 확인. CIS 수신에서는 원 값을 보존. |
| `vision_sequence` | `uint32_t` | 새 카메라 판정 번호 | Pi 비전/상태 owner의 새 평가 번호. 관측 sequence와 상태 sequence 독립, 반복 UART frame으로 증가 금지. |
| `vision_age_ms` | `uint16_t` | 판정 원본 후 ms | Pi 원 판정/상태 평가 후 ms. App이 현재 원 Age 제공; Pi Network 대기만 한 번 누적·65535 포화. |
| `occupant_presence` | `NetTriState` | 0=없음; 1=있음; 255=미확인 | Pi 비전 기능 owner. presence/count 품질 독립; 상태 READY/복구만으로 과거 관측을 새 VALID로 만들지 않음. |
| `presence_validity` | `NetValidity` | 0=INVALID; 1=VALID | Pi 비전 owner의 해당 필드 원 품질·사유. CIS는 원 record를 보존하고 전달 평가로만 추가 제한한다. |
| `presence_reason` | `NetQualityReason` | Pi 원 비전 품질 사유 | Pi 비전 owner의 해당 필드 원 품질·사유. CIS는 원 record를 보존하고 전달 평가로만 추가 제한한다. |
| `occupant_count` | `uint8_t` | 0~5명,255=미확인 | Pi 비전 기능 owner. presence/count 품질 독립; 상태 READY/복구만으로 과거 관측을 새 VALID로 만들지 않음. |
| `count_validity` | `NetValidity` | 0=INVALID; 1=VALID | Pi 비전 owner의 해당 필드 원 품질·사유. CIS는 원 record를 보존하고 전달 평가로만 추가 제한한다. |
| `count_reason` | `NetQualityReason` | Pi 원 비전 품질 사유 | Pi 비전 owner의 해당 필드 원 품질·사유. CIS는 원 record를 보존하고 전달 평가로만 추가 제한한다. |
| `vision_status` | `NetSensorFunctionState` | Pi 비전 기능 상태 | Pi 비전 기능 owner. presence/count 품질 독립; 상태 READY/복구만으로 과거 관측을 새 VALID로 만들지 않음. |
| `vision_fault_code` | `NetVisionFaultCode` | 0=없음; 1=초기화; 2=카메라; 3=판정; 4=데이터 오류 | Pi 비전 기능 owner. presence/count 품질 독립; 상태 READY/복구만으로 과거 관측을 새 VALID로 만들지 않음. |
<a id="type-netvisionstatus"></a>
## `NetVisionStatus`

Pi 비전 기능/고장 → CIS · [`P_VISION_STATUS`](../../03_COMMUNICATION/30_UART_C.md#message-p-vision-status)

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

**로컬 사용:** [`PiNet_PublishVisionStatus()`](20_API.md#api-pinet_publishvisionstatus) · 전체 타입/인자·호출자·수명·실패는 API에서 확인한다.

| 멤버 | 정확한 C 타입 | 값·단위·범위 / 역할 | 원 소유자·유효 조건 |
|---|---|---|---|
| `pi_boot_id` | `uint32_t` | Pi runtime 단일 기동 owner가 local admission에서 고정 | Pi runtime 단일 기동 owner. 송신 App 초기화 의무 없음; Pi Network가 local admission 때 현재 관측 세대와 결합하고 송신 전 같은 세대인지 확인. CIS 수신에서는 원 값을 보존. |
| `status_sequence` | `uint32_t` | 새 기능 상태 평가 | Pi 비전/상태 owner의 새 평가 번호. 관측 sequence와 상태 sequence 독립, 반복 UART frame으로 증가 금지. |
| `vision_status` | `NetSensorFunctionState` | Pi 비전 기능 상태 | Pi 비전 기능 owner. presence/count 품질 독립; 상태 READY/복구만으로 과거 관측을 새 VALID로 만들지 않음. |
| `active_fault` | `NetVisionFaultCode` | 비전 고장 코드 | Pi 비전 기능 owner. presence/count 품질 독립; 상태 READY/복구만으로 과거 관측을 새 VALID로 만들지 않음. |
| `last_fault` | `NetVisionFaultCode` | Pi 이번 기동 이후 최근 비전 고장 | Pi 이번 기동 이후 최근 기능 고장. current active 해제와 구분. |
| `status_age_ms` | `uint16_t` | 상태 평가 후 ms | Pi 원 판정/상태 평가 후 ms. App이 현재 원 Age 제공; Pi Network 대기만 한 번 누적·65535 포화. |
| `affected_function` | `uint8_t` | bit0=탑승자 존재; bit1=인원수 | Pi 비전 기능 owner. presence/count 품질 독립; 상태 READY/복구만으로 과거 관측을 새 VALID로 만들지 않음. |
<a id="type-netpipermission"></a>
## `NetPiPermission`

DOMAIN 원 허용 → CIS 내부 중계 → Pi · [`P_PERMISSION`](../../03_COMMUNICATION/30_UART_C.md#message-p-permission)

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

**로컬 사용:** [`PiApp_OnCisPermission()`](20_API.md#api-piapp_oncispermission) · 전체 타입/인자·호출자·수명·실패는 API에서 확인한다.

| 멤버 | 정확한 C 타입 | 값·단위·범위 / 역할 | 원 소유자·유효 조건 |
|---|---|---|---|
| `cis_boot_id` | `uint32_t` | CIS runtime이 채우는 중계 세대 | CIS runtime. 원 DOMAIN Boot가 아니며 Pi는 수용된 CIS 세대 기준으로 stale relay를 배제. |
| `update_sequence` | `uint32_t` | CIS 허용 중계의 새 의미 평가 번호; 반복 프레임에서는 유지 | CIS 내부 relay의 새 의미 평가 번호 후보. 원 DOMAIN revision/세대 변경 또는 허용 품질 제한을 반영; 500ms 반복 frame에서는 유지. |
| `vehicle_power_permission` | `NetTriState` | 0=불허; 1=허용; 255=확인 불가 | DOMAIN 원 판단에서 파생. CIS가 확인하지 않은 허용을 새로 생성하지 않음. |
| `value_quality` | `NetValueQuality` | 허용 상태 품질 | 원 DOMAIN power 품질 + CIS 수신/만료 검사. 전송 가능한 보수 평가만; 품질 제한/사유 우선순위 TEAM_DECISION_REQUIRED. |
| `quality_reason` | `NetQualityReason` | 품질 사유 | 원 DOMAIN power 품질 + CIS 수신/만료 검사. 전송 가능한 보수 평가만; 품질 제한/사유 우선순위 TEAM_DECISION_REQUIRED. |
| `use_limit_ms` | `uint16_t` | 초안 1500ms | CIS UART-C 설정 후보 1500ms. 양끝·DOMAIN 원 유효기한과 합의 전 구현 기준으로 확정하지 않음. |
| `source_age_ms` | `uint16_t` | Domain 허용 판단 후 누적 ms | DOMAIN 원 판단 Age + CAN 수신 후 CIS relay 대기. 한 번 누적; Pi 현재 Age는 여기에 Pi 수신 후 경과를 더함. |

<a id="type-netvisionfaultcode"></a>
## `NetVisionFaultCode`

```c
typedef uint16_t NetVisionFaultCode;
enum NetVisionFaultCodeCode
{
    NET_VISION_FAULT_NONE = 0,
    NET_VISION_FAULT_INITIALIZATION = 1,
    NET_VISION_FAULT_CAMERA = 2,
    NET_VISION_FAULT_INFERENCE = 3,
    NET_VISION_FAULT_DATA_INVALID = 4
};
```

<a id="fixed-width-codes"></a>
별칭은 위의 고정 폭 C 정수다. 이어지는 `enum ...Code`는 상수 집합으로, enum 메모리 크기가 별칭/Wire 폭이라는 뜻이 아니다. 아래 숫자는 **이 설계 후보의 의미 코드**이며 기존 C2 타입은 [명시 mapping](../../04_COMMON/30_ENUM_WIRE_MAPPING.md)을 거친다. 예약/지원하지 않는 코드는 형식 오류로 처리한다.

| 상수 | 제안 값 / 의미 |
|---|---|
| `NET_VISION_FAULT_NONE` | `0` · 없음 |
| `NET_VISION_FAULT_INITIALIZATION` | `1` · 비전 초기화 오류 |
| `NET_VISION_FAULT_CAMERA` | `2` · 카메라 오류 |
| `NET_VISION_FAULT_INFERENCE` | `3` · 판정 처리 오류 |
| `NET_VISION_FAULT_DATA_INVALID` | `4` · 비전 결과 형식/내용 오류 |

## 언어 중립 record의 필드 책임

`NetVisionResult/NetVisionStatus`의 `pi_boot_id`는 Pi runtime이 local admission 시 원 관측과 결합하여 고정하는 필드다. 비전 생산자에게 임의 기동 번호 초기화를 요구하지 않는다. 실제 언어 객체에서 해당 필드를 생략하거나 읽기 전용 runtime 참조로 제공할 수 있으며, 동일 의미 record→Codec의 한 번 변환으로 충분하다. 실제 송신 전에 고정한 generation이 현재 runtime/생산자 관측 문맥과 같은지 검증한다. 기동/process 관측 세대 변경이면 이전 pending/cache를 무효화하고 새 관측 전까지 old record를 새 Boot로 stamp하지 않는다. CIS에 수신된 뒤에는 **원 Pi Boot 보존**이므로 CIS가 자기 Boot로 덮어쓰지 않는다.

`vision_sequence`는 새 카메라 판정, `status_sequence`는 새 기능 상태 평가에서만 증가한다. 같은 판정/상태 반복은 원 순번 유지·Age 증가다. UART-C LINK_SEQUENCE는 별도 Network 전송 순번으로 wire frame마다 증가한다. runtime 기동 ID 생성/수용과 process 재시작→새 관측 세대 구분은 TEAM_DECISION_REQUIRED다.

Presence INVALID를 없음으로, Count INVALID를 0명으로 합성하지 않는다. Count 0~5/255는 표현 후보이며 실제 검출 성능·유효 범위 인증이 아니다. 정상 count/presence 조합 검사·지연 상한은 Pi/CIS 담당이 결정한다. 기능 상태 READY만으로 이전 관측 품질을 승격하지 않는다.

**PROPOSED — 독립 제출:** 같은 판정의 Presence/Count·각 원 validity/reason은 `PiNet_PublishVisionResult()`에, 기능 READY/FAULT 변경은 `PiNet_PublishVisionStatus()`에 제출한다. 원 INVALID·사유를 보존하고 Count/Presence를 독립 소비한다. CIS는 원 record를 보존한 뒤 원 품질을 승격하지 않는 제한 평가만 권장한다. 품질 전달·사유 우선순위와 확인 질문의 정본은 [CIS의 Pi 원 관측·추가 평가](../CIS/10_DATA.md#pi-원-관측과-cis-추가-평가)다. Pi/CIS/DOMAIN `CROSS_NODE_AGREEMENT`, 원 비전 사실 `FUNCTION_OWNER`, Codec/세대 수용 `IMPLEMENTATION_BINDING`은 미합의다. 실제 채택·구현·시험 근거는 없다.

<a id="허용-소비와-원본-age"></a>
## 허용 소비와 원본 Age

`NetPiPermission`은 DOMAIN 판단을 CIS가 전달한 record다. `vehicle_power_permission=1` 단독으로 허용하지 않고 `value_quality`, `quality_reason`, `source_age_ms`, `use_limit_ms`와 수신 후 경과를 함께 확인한다. STALE/INVALID/NO_DATA·확인 불가·기동 변경·무수신 이후에는 현재 유효 허용을 재확인한다. 이전 기동의 허용값을 startup에서 정상 복원하지 않는다.

Source AGE는 DOMAIN 원 평가부터 누적된 값이다. Pi 수신 시각은 자기 단조 clock이며 DOMAIN/CIS clock과 직접 비교하지 않는다. 현재 사용 Age는 전달된 source_age에 **Pi 수신 이후** 경과를 한 번 더한 보수 값이다. 만료/기동·연결 상실을 실제 permission consumer에 알리는 기존 supervision 연결은 BINDING_OPEN이며 API 이름만으로 timer/새 통지 API를 확정하지 않는다.

P_PERMISSION에는 DOMAIN 원 Boot/순번이 직접 없다. CIS relay 세대/revision이 재평가를 반영하는 후보이며 DOMAIN provenance를 완전히 복원하는 형식은 아니다. 1500ms use_limit·품질 사유 합성·relay revision 규칙은 팀 결정 전 호환 구현을 보류한다. 실제 Linux process/Serial/BLE/Camera·Codec/Board 시험 NOT_EXECUTED.

**PROPOSED — 원 기한을 연장하지 않는 소비(O05):** `PiApp_OnCisPermission()`에서 수용한 CIS Boot/revision·원 AGE·로컬 수신 기준점을 보관하고, 실제 차량용 결과 사용 시마다 원 AGE에 Pi 수신 후 경과를 한 번 누적해 확인하는 안을 권장한다. 값 1, `value_quality=OK`, 수용한 세대, 원 기한 이내의 네 조건이 모두 확인된 경우만 허용으로 사용한다. 원 DOMAIN 150ms 사용 상한 후보와 전달된 `use_limit_ms` 중 더 짧은 한도를 사용하도록 양단 합의하는 안이므로 1500ms 후보가 150ms 원 판단을 연장하지 않는다. 0/OK는 확인된 불허이며, 255/NO_DATA/INVALID/STALE는 허용을 확인할 수 없는 상태로 구분한다. 같은 relay revision의 재수신으로 AGE를 낮추지 않으며 기동 변경·만료·연결 상실 후에는 새 유효 원 판단 확인 전 허용을 재사용하지 않는다. 이 제한은 차량용 결과 사용에 적용하고 프로세스 종료·물리 전원 차단 정책을 새로 만들지 않는다.

500ms 반복 보고만으로 원 150ms 후보 한도 안의 연속 허용을 보증할 수 없다. CIS의 새 원 평가/제한 발생 시 로컬 제출 권고와 함께 실제 Pi consumer 무효화 연결·원 한도·전달 지연을 DOMAIN/CIS/Pi 담당자가 확인해야 한다. 이 `CROSS_NODE_AGREEMENT` 권고의 선택 이유는 기존 `SOURCE_AGE/USE_LIMIT/quality`를 재사용하면서 원 허용의 수명을 늘리지 않기 때문이다. 별도 새 deadline/permission DTO는 추가하지 않는다. 실제 consumer/Codec/Board 시험은 `NOT_EXECUTED`이며 `AGREED/IMPLEMENTED/TESTED`로 승격하지 않는다.

**Generation 수용 조건:** 생산자와 runtime의 실제 기동/관측 세대 연결은 BINDING_OPEN이다. runtime reset 후 이전 record를 다시 제출하여 새 BOOT를 얻는 방식은 금지하며 새 판정/상태 평가가 필요하다. 이는 신규 wire 필드·별도 App DTO를 추가하는 계약이 아니다.
