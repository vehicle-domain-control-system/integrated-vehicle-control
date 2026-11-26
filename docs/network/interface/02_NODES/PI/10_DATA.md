# Pi Data — 언어 중립 비전 기록과 사용 허용

Interface v2 · r1 · [규격과 연동 조건](../../07_CROSS_NODE_INTERFACE_RULES.md#status)

권장 프로필과 실제 연동 검증 전 조건을 구분한다. 각 API의 호출·품질·시간 조건을 적용한다.

[전체 시작](../../00_START_HERE.md) · [개요](00_OVERVIEW.md) · [Data](10_DATA.md) · [API](20_API.md)

**PROPOSED_DESIGN · BINDING_OPEN.** 아래 전체 선언은 의미 타입을 표시하는 설계용 C 표현이다. 실제 설치된 헤더나 펌웨어가 아니며, 구조체의 패딩·enum 크기를 원시 바이트로 그대로 전송하지 않는다. [공통 ID·수명](../../04_COMMON/10_DATA_AND_ID_RULES.md)과 [C enum↔Wire 명시 변환](../../04_COMMON/30_ENUM_WIRE_MAPPING.md)을 함께 적용한다.

현재 검토한 참고 구현은 Python이다. 각 `NetVision*`/`NetPiPermission`은 필드·폭·범위·의미가 같은 언어 중립 record로 매핑한다. C 타입은 의미 폭을 표시한 참조 표현이며 구현 언어 변경이나 동일 이름의 C wrapper 설치를 강제하지 않는다.

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
| `value_quality` | `NetValueQuality` | 허용 상태 품질 | 원 DOMAIN power 품질 + CIS 수신/만료 검사. 전송 가능한 보수 평가만; 품질 제한/사유 우선순위 CONTRACT_OPEN. |
| `quality_reason` | `NetQualityReason` | 품질 사유 | 원 DOMAIN power 품질 + CIS 수신/만료 검사. 전송 가능한 보수 평가만; 품질 제한/사유 우선순위 CONTRACT_OPEN. |
| `use_limit_ms` | `uint16_t` | 초안 1500ms | 기존 1500ms 후보. 같은 후방 Power 원본이면 500ms로 전달하는 조건부 권장 프로필이 있으며, 원 수명·양단 바이트·만료 검증 전 최종 값으로 바꾸지 않음. |
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
별칭은 위의 고정 폭 C 정수다. 이어지는 `enum ...Code`는 상수 집합으로, enum 메모리 크기가 별칭/Wire 폭이라는 뜻이 아니다. 아래 숫자는 **이 설계 후보의 의미 코드**이며 기존 C2 타입은 [명시 변환표](../../04_COMMON/30_ENUM_WIRE_MAPPING.md)을 거친다. 예약/지원하지 않는 코드는 형식 오류로 처리한다.

| 상수 | 제안 값 / 의미 |
|---|---|
| `NET_VISION_FAULT_NONE` | `0` · 없음 |
| `NET_VISION_FAULT_INITIALIZATION` | `1` · 비전 초기화 오류 |
| `NET_VISION_FAULT_CAMERA` | `2` · 카메라 오류 |
| `NET_VISION_FAULT_INFERENCE` | `3` · 판정 처리 오류 |
| `NET_VISION_FAULT_DATA_INVALID` | `4` · 비전 결과 형식/내용 오류 |

## 언어 중립 record의 필드 책임

`NetVisionResult/NetVisionStatus`의 `pi_boot_id`는 Pi runtime이 local admission 시 원 관측과 결합하여 고정하는 필드다. 비전 생산자에게 임의 기동 번호 초기화를 요구하지 않는다. 실제 언어 객체에서 해당 필드를 생략하거나 읽기 전용 runtime 참조로 제공할 수 있으며, 동일 의미 record→Codec의 한 번 변환으로 충분하다. 실제 송신 전에 고정한 generation이 현재 runtime/생산자 관측 문맥과 같은지 검증한다. 기동/process 관측 세대 변경이면 이전 대기·캐시 자료를 무효화하고 새 관측 전까지 과거 기록에 새 BOOT를 부여하지 않는다. CIS에 수신된 뒤에는 **원 Pi Boot 보존**이므로 CIS가 자기 Boot로 덮어쓰지 않는다.

`vision_sequence`는 새 카메라 판정, `status_sequence`는 새 기능 상태 평가에서만 증가한다. 같은 판정/상태 반복은 원 순번 유지·Age 증가다. UART-C LINK_SEQUENCE는 별도 Network 전송 순번으로 wire frame마다 증가한다. runtime 기동 ID 생성/수용과 process 재시작→새 관측 세대 구분은 BINDING_OPEN다.

Presence INVALID를 없음으로, Count INVALID를 0명으로 합성하지 않는다. 같은 판정의 Presence와 Count가 각각 유효하고 값 범위·기능 상태 검사를 통과했을 때만 상호 모순을 검사한다. 한쪽이 미확인이면 그 숫자만으로 다른 정상 필드까지 무효화하지 않는다. Count 0~5/255는 기존 표현 범위이지 실제 검출 성능의 보증이 아니다.

**권장 프로필 — 독립 제출:** 동일 판정에 속하는 Presence/Count와 각각의 원 validity/reason은 `PiNet_PublishVisionResult()`에, 독립된 기능 상태·고장은 `PiNet_PublishVisionStatus()`에 제출한다. STATUS 갱신은 원 Vision 판단을 갱신하지 않는다. CIS는 원 판정 사유를 보존하고 이후 실제 장애가 생기면 현재 사용 품질을 추가 제한한다. 최종 규칙은 [CIS의 Pi 원 관측·추가 평가](../CIS/10_DATA.md#pi-원-관측과-cis-추가-평가)를 따른다. Wire를 늘리거나 최종 품질이 원 Pi 사유 전체를 전달한다고 주장하지 않는다. 실제 함수 연결과 시험은 미완료다.

<a id="허용-소비와-원본-age"></a>
## 허용 소비와 원본 Age

`NetPiPermission`은 DOMAIN 판단을 CIS가 전달한 record다. `vehicle_power_permission=1` 단독으로 허용하지 않고 `value_quality`, `quality_reason`, `source_age_ms`, `use_limit_ms`와 수신 후 경과를 함께 확인한다. STALE/INVALID/NO_DATA·확인 불가·기동 변경·무수신 이후에는 현재 유효 허용을 재확인한다. 이전 기동의 허용값을 startup에서 정상 복원하지 않는다.

Source AGE는 DOMAIN 원 평가부터 누적된 값이다. Pi 수신 시각은 자기 단조 clock이며 DOMAIN/CIS clock과 직접 비교하지 않는다. 현재 사용 Age는 전달된 source_age에 **Pi 수신 이후** 경과를 한 번 더한 보수 값이다. 만료/기동·연결 상실을 실제 permission consumer에 알리는 기존 supervision 연결은 BINDING_OPEN이며 API 이름만으로 timer/새 통지 API를 확정하지 않는다.

P_PERMISSION에는 DOMAIN 원 BOOT/순번이 직접 없다. CIS는 수용한 DOMAIN 기동·원 허용을 관리하고 그 변화와 제한을 relay에 반영해야 한다. Pi가 받는 CIS BOOT와 relay revision을 원 DOMAIN 식별자로 간주하지 않는다. `use_limit_ms`는 기존 1500ms 후보를 보존하며 500ms 변경은 아래 조건부 프로필로 구분한다. 실제 runtime·Codec·수신 무효화 시험은 미완료다.

**권장 프로필 — 원 기한을 연장하지 않는 수신 해석:** 수용된 CIS BOOT/relay revision·품질·원 AGE·로컬 수신 기준점을 보존한다. 현재 사용 AGE는 이미 반영한 구간을 중복 더하지 않고 원 AGE에 수신 후 경과를 더한다. 원 수명과 전달된 `use_limit_ms` 중 더 짧은 한도에 도달하면 유효한 ALLOW가 아니다. 같은 revision을 다시 받았다는 이유로 AGE를 낮추지 않는다.

원 사용 기한 500ms는 후방 활성용 원 Permission의 사용 기한이다. 이 P_PERMISSION이 같은 원 Power 판단의 중계임을 확인한 범위에서는 500ms를 상한으로 적용한다. `USE_LIMIT=500ms`로 전달하는 안은 조건부이며, 양단 바이트·AGE·만료 시험 전 배포 값으로 확정하지 않는다. 기존 1500ms 후보나 수신 중단 1500ms가 원 판단 수명을 연장하지 않는다.

허용은 값 1과 OK 품질만으로 결정하지 않고 수용한 세대·순번·원 시간 근거를 함께 확인한다. 0/OK는 확인된 불허, 255 또는 NO_DATA/INVALID/STALE는 허용 확인 불가다. 기동 변경·만료·연결 상실 뒤에는 새 유효 원 판단을 확인하기 전 이전 ALLOW를 복원하지 않는다. 공유 CAN의 Power/Operation 원천·공통 AGE 합성은 미결로 유지한다.

UART-C의 500ms 반복만으로 원 기한 500ms 이내의 연속 ALLOW를 보증하지 않는다. 새 DOMAIN 평가가 CIS에 도착하면 원 근거를 유지하여 가능한 즉시 중계하고 실제 지연·실패를 확인해야 한다. Pi의 통신 수신·해석·만료 검증과 비전 기능 적용은 별개다. 비전 기능에 사용할지는 Pi 모듈이 결정하며, 카메라 중지·프로세스 종료·물리 전원 차단을 이 계약의 새 요구로 만들지 않는다. 비전 기능을 적용하지 않는 구성에서도 통신 문서와 수신 시험은 진행할 수 있다.

<a id="delta-pi-permission"></a>
**추가 소비 조건:** FAILED/NO_DATA 시 과거 ALLOW 즉시 철회는 DOMAIN Core 담당자 구현 완료 보고다. UNKNOWN의 원인별 C2 Quality 투영과 Network→CIS→Pi 실제 전달은 수용·검증 대기이며, 정상 형식의 무효 품질을 과거 ALLOW로 대체하지 않는 기존 소비 원칙은 유지한다. 검증된 relay가 실패·품질 하향을 전달하면 원본 AGE가 500ms 이내라도 이전 ALLOW를 사용하지 않고 다음 성공 평가/TTL 만료까지 무효화를 미루지 않는다. Pi Wire에는 원 DOMAIN 성공 Sequence가 없으므로 relay revision을 그것으로 간주하지 않는다. 재수신·실패 시점에 원본 AGE를 0으로 초기화하지 않으며 지연된 구 ALLOW로 복원하지 않는다. 실제 relay·Queue·수신 연결은 미검증이고 Pi 카메라/비전 기능 적용 선택은 모듈 범위다. [적용 조건](../../07_CROSS_NODE_INTERFACE_RULES.md#permission-delta)

**Generation 수용 조건:** 생산자와 runtime의 실제 기동/관측 세대 연결은 BINDING_OPEN이다. runtime reset 후 이전 기록을 다시 제출해 새 BOOT를 부여받는 방식은 금지하며 새 판정/상태 평가가 필요하다. 이는 신규 wire 필드·별도 App DTO를 추가하는 계약이 아니다.
