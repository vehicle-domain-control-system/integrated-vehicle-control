# VSS Data — 전체 타입과 필드

Interface v2 · r1 · [규격과 연동 조건](../../07_CROSS_NODE_INTERFACE_RULES.md#status)

VSS는 DOMAIN의 확정 Event·Warning을 받아 음향을 중재하고, 자신의 서비스·고장 상태를 보고한다. 위험 판정은 DOMAIN, 음향 선택·실제 출력 상태는 VSS가 소유한다. **TX App은 링크 BOOT_ID/TX_SEQUENCE를 채우지 않는다.** RX의 원 source Boot는 판단 근거의 식별이며 링크 송신 Boot와 목적이 다르다.

[노드 개요](00_OVERVIEW.md) · [Data](10_DATA.md) · [API](20_API.md) · [전체 시작](../../00_START_HERE.md) · [공통 의미·수명](../../04_COMMON/10_DATA_AND_ID_RULES.md) · [CAN FD](../../03_COMMUNICATION/10_CAN_FD.md)

모든 아래 신규 로컬 타입은 **PROPOSED_DESIGN / BINDING_OPEN**이다. WINDOW 관련 경계는 **[구현 보류 — 설계 유지]**다. `Net*` 별칭의 숫자는 이 초안의 의미 코드이고 실제 C2 enum은 [명시 변환표](../../04_COMMON/30_ENUM_WIRE_MAPPING.md)로 변환한다. C struct padding·enum 폭을 포함한 `memcpy` 전송은 금지한다. `<stdint.h>`와 공통 Net 타입은 [공통 Data](../../04_COMMON/10_DATA_AND_ID_RULES.md)에서 읽는다.

## 타입 색인

| 타입 | 목적·owner |
|---|---|
| [NetVssEvent](#type-netvssevent) | 일회성 의미 발생; DOMAIN→VSS |
| [NetVssWarningState](#type-netvsswarningstate) | 독립3경고·후방 활성 근거; DOMAIN→VSS |
| [NetVssStatus](#type-netvssstatus) | 현재 서비스·현재고장·최근이력; VSS→DOMAIN |
| [NetVssEventKind](#type-netvsseventkind) | 의미 코드·폭·허용 값은 아래 전체 선언 |
| [NetVssState](#type-netvssstate) | 의미 코드·폭·허용 값은 아래 전체 선언 |
| [NetVssAvailability](#type-netvssavailability) | 의미 코드·폭·허용 값은 아래 전체 선언 |
| [NetRearWarningState](#type-netrearwarningstate) | 의미 코드·폭·허용 값은 아래 전체 선언 |
| [NetVssFaultMask](#type-netvssfaultmask) | 의미 코드·폭·허용 값은 아래 전체 선언 |
| [NetVssFaultCode](#type-netvssfaultcode) | 의미 코드·폭·허용 값은 아래 전체 선언 |

송수신하는 세 구조체는 아래에서 **전체 선언 → 멤버 계약** 순서로 읽는다. 공통 RX 메타는 [NetCanRxMeta](../../04_COMMON/10_DATA_AND_ID_RULES.md#type-netcanrxmeta), Permission의 전체 선언은 [NetDomainPermission](../../04_COMMON/10_DATA_AND_ID_RULES.md#type-netdomainpermission)에 있다. [메시지 사용·시간 조건](#vss-time-contract), [Status 변환 권장안](#r2-status-proposal), [입력 상실·복구](#vss-input-recovery)를 함께 확인한다.

## 전체 선언·멤버 계약

<a id="type-netvssevent"></a>
### `NetVssEvent`

```c
typedef struct NetVssEvent
{
    NetVssEventKind event_kind;
    uint32_t occurrence_id;
    uint16_t original_age_ms;
    uint16_t use_limit_ms;
    NetValueQuality value_quality;
    NetAvailability input_availability;
    uint32_t related_command_id;
} NetVssEvent;
```

| 멤버 | 정확한 C 타입 | 역할·범위 | 단위 | 원 owner·유효 조건 |
|---|---|---|---|---|
| `event_kind` | [NetVssEventKind](10_DATA.md#type-netvsseventkind) | 완료·오류·웰컴·굿바이 의미; 허용 코드 1~5 | 의미 코드 · 무단위 | DOMAIN 기능 판단; 입력 품질·가용성·사용 한도를 함께 확인 |
| `occurrence_id` | `uint32_t` | 기동 내 일회성 발생 번호, uint32 범위 | 발생 식별자 · 무단위 | DOMAIN 발생 owner; 같은 발생 재전달 때 유지. 수용된 DOMAIN 기동과 함께 중복 판정 |
| `original_age_ms` | `uint16_t` | 최초 의미 발생 이후 누적 ms, 0~65535 포화 | ms | DOMAIN 원본 + 각 Network 구간 대기만 1회 누적; 새 RX로 0 재설정 금지 |
| `use_limit_ms` | `uint16_t` | 일회성 사용 한도 ms; 500ms 후보(PROPOSED) | ms | Network profile500ms↔구 SysRS 실제 음향시작2초: CONFLICT/FUNCTION_POLICY_OPEN. 수용·시작 기준 미확정 |
| `value_quality` | [NetValueQuality](../../04_COMMON/10_DATA_AND_ID_RULES.md#type-netvaluequality) | 의미 입력 품질; OK/STALE/INVALID/NO_DATA | 의미 코드 · 무단위 | DOMAIN 의미 owner; 정상 의미값과 독립 |
| `input_availability` | [NetAvailability](../../04_COMMON/10_DATA_AND_ID_RULES.md#type-netavailability) | 의미 생성 기능의 가용성; 0 가용/1 제한/2 불가 | 의미 코드 · 무단위 | DOMAIN 기능 owner; VSS 서비스 가용성과 구분 |
| `related_command_id` | `uint32_t` | 원 DOMAIN 명령 ID; 연결이 없으면 0 | 명령 식별자 · 무단위 | DOMAIN C2 명령 owner; 새 발급 금지, occurrence·MOBILE RequestId와 별개 |

<a id="type-netvsswarningstate"></a>
### `NetVssWarningState`

```c
typedef struct NetVssWarningState
{
    uint32_t antipinch_source_boot;
    uint32_t antipinch_source_sequence;
    uint16_t antipinch_original_age_ms;
    NetFlag antipinch_state;
    NetValueQuality antipinch_quality;
    NetQualityReason antipinch_reason;
    NetAvailability antipinch_availability;
    uint32_t occupant_hazard_source_boot;
    uint32_t occupant_hazard_source_sequence;
    uint16_t occupant_hazard_original_age_ms;
    NetFlag occupant_hazard_state;
    NetValueQuality occupant_hazard_quality;
    NetQualityReason occupant_hazard_reason;
    NetAvailability occupant_hazard_availability;
    uint32_t rear_obstacle_source_boot;
    uint32_t rear_obstacle_source_sequence;
    uint16_t rear_obstacle_original_age_ms;
    NetRearWarningState rear_obstacle_state;
    NetValueQuality rear_obstacle_quality;
    NetQualityReason rear_obstacle_reason;
    NetAvailability rear_obstacle_availability;
    NetFlag rear_detection_activation;
    uint8_t activation_meta_format;
    NetValueQuality rear_activation_quality;
    NetQualityReason rear_activation_reason;
    uint32_t rear_activation_sequence;
    uint16_t rear_activation_age_ms;
} NetVssWarningState;
```

| 멤버 | 정확한 C 타입 | 역할·범위 | 단위 | 원 owner·유효 조건 |
|---|---|---|---|---|
| `antipinch_source_boot` | `uint32_t` | 끼임 원 판단 근거 노드 기동 식별 | 기동 식별자 · 무단위 | WINDOW 원 보호 근거 owner; 제품경로 DEFERRED→DOMAIN 보존→VSS 소비. DOMAIN 링크 Boot로 치환 금지 |
| `antipinch_source_sequence` | `uint32_t` | 끼임 원 근거의 실제 새 평가 번호 | 평가 번호 · 무단위 | 원 evidence owner; DOMAIN 재포장·CAN 재전송으로 새 번호 생성 금지 |
| `antipinch_original_age_ms` | `uint16_t` | 끼임 원 관측 이후 누적 ms, uint16 포화 | ms | 원 관측 + 각 중계 대기 1회 누적; 세 경고 독립 Age |
| `antipinch_state` | [NetFlag](../../04_COMMON/10_DATA_AND_ID_RULES.md#type-netflag) | 끼임 DOMAIN 확정 현재 의미 | 0/1 의미값 · 무단위 | DOMAIN 위험 판단 owner→VSS 음향 중재. 품질상실·음향중단을 CLEAR로 합성 금지 |
| `antipinch_quality` | [NetValueQuality](../../04_COMMON/10_DATA_AND_ID_RULES.md#type-netvaluequality) | 끼임 입력 근거 품질 | 의미 코드 · 무단위 | 해당 원 evidence + DOMAIN 판단; 다른 경고의 품질과 독립 |
| `antipinch_reason` | [NetQualityReason](../../04_COMMON/10_DATA_AND_ID_RULES.md#type-netqualityreason) | 끼임 품질 사유 | 의미 코드 · 무단위 | 해당 evidence 평가 owner; 무효·만료 사유 보존 |
| `antipinch_availability` | [NetAvailability](../../04_COMMON/10_DATA_AND_ID_RULES.md#type-netavailability) | 끼임 기능 가용성 0/1/2 | 의미 코드 · 무단위 | 해당 기능 owner; quality와 별개. WINDOW 제품 경로는 DEFERRED |
| `occupant_hazard_source_boot` | `uint32_t` | 탑승자 위험 원 판단 근거 노드 기동 식별 | 기동 식별자 · 무단위 | Pi 원 비전 관측 owner; CIS 추가 품질평가와 별개, C2 lineage 매핑 OPEN→DOMAIN 보존→VSS 소비. DOMAIN 링크 Boot로 치환 금지 |
| `occupant_hazard_source_sequence` | `uint32_t` | 탑승자 위험 원 근거의 실제 새 평가 번호 | 평가 번호 · 무단위 | 원 evidence owner; DOMAIN 재포장·CAN 재전송으로 새 번호 생성 금지 |
| `occupant_hazard_original_age_ms` | `uint16_t` | 탑승자 위험 원 관측 이후 누적 ms, uint16 포화 | ms | 원 관측 + 각 중계 대기 1회 누적; 세 경고 독립 Age |
| `occupant_hazard_state` | [NetFlag](../../04_COMMON/10_DATA_AND_ID_RULES.md#type-netflag) | 탑승자 위험 DOMAIN 확정 현재 의미 | 0/1 의미값 · 무단위 | DOMAIN 위험 판단 owner→VSS 음향 중재. 품질상실·음향중단을 CLEAR로 합성 금지 |
| `occupant_hazard_quality` | [NetValueQuality](../../04_COMMON/10_DATA_AND_ID_RULES.md#type-netvaluequality) | 탑승자 위험 입력 근거 품질 | 의미 코드 · 무단위 | 해당 원 evidence + DOMAIN 판단; 다른 경고의 품질과 독립 |
| `occupant_hazard_reason` | [NetQualityReason](../../04_COMMON/10_DATA_AND_ID_RULES.md#type-netqualityreason) | 탑승자 위험 품질 사유 | 의미 코드 · 무단위 | 해당 evidence 평가 owner; 무효·만료 사유 보존 |
| `occupant_hazard_availability` | [NetAvailability](../../04_COMMON/10_DATA_AND_ID_RULES.md#type-netavailability) | 탑승자 위험 기능 가용성 0/1/2 | 의미 코드 · 무단위 | 해당 기능 owner; quality와 별개. |
| `rear_obstacle_source_boot` | `uint32_t` | 후방 위험 원 판단 근거 노드 기동 식별 | 기동 식별자 · 무단위 | CIS 후방 원 측정 근거 owner→DOMAIN 보존→VSS 소비. DOMAIN 링크 Boot로 치환 금지 |
| `rear_obstacle_source_sequence` | `uint32_t` | 후방 위험 원 근거의 실제 새 평가 번호 | 평가 번호 · 무단위 | 원 evidence owner; DOMAIN 재포장·CAN 재전송으로 새 번호 생성 금지 |
| `rear_obstacle_original_age_ms` | `uint16_t` | 후방 위험 원 관측 이후 누적 ms, uint16 포화 | ms | 원 관측 + 각 중계 대기 1회 누적; 세 경고 독립 Age |
| `rear_obstacle_state` | [NetRearWarningState](10_DATA.md#type-netrearwarningstate) | 후방 위험 DOMAIN 확정 현재 의미 | 의미 코드 · 무단위 | DOMAIN 위험 판단 owner→VSS 음향 중재. 품질상실·음향중단을 CLEAR로 합성 금지 |
| `rear_obstacle_quality` | [NetValueQuality](../../04_COMMON/10_DATA_AND_ID_RULES.md#type-netvaluequality) | 후방 위험 입력 근거 품질 | 의미 코드 · 무단위 | 해당 원 evidence + DOMAIN 판단; 다른 경고의 품질과 독립 |
| `rear_obstacle_reason` | [NetQualityReason](../../04_COMMON/10_DATA_AND_ID_RULES.md#type-netqualityreason) | 후방 위험 품질 사유 | 의미 코드 · 무단위 | 해당 evidence 평가 owner; 무효·만료 사유 보존 |
| `rear_obstacle_availability` | [NetAvailability](../../04_COMMON/10_DATA_AND_ID_RULES.md#type-netavailability) | 후방 위험 기능 가용성 0/1/2 | 의미 코드 · 무단위 | 해당 기능 owner; quality와 별개. |
| `rear_detection_activation` | [NetFlag](../../04_COMMON/10_DATA_AND_ID_RULES.md#type-netflag) | 0 DISABLED/1 ACTIVE; 위험 CLEAR와 독립 | 0/1 의미값 · 무단위 | DOMAIN 실제 활성 정책 owner; 활성 품질·근거 한도 함께 검사 |
| `activation_meta_format` | `uint8_t` | 0 근거미지원/1 활성 meta 형식/그외 미지원 | 의미 코드 · 무단위 | DOMAIN/VSS Codec format marker; App 위험값이 아님.1 양측 동시 이관 필요 |
| `rear_activation_quality` | [NetValueQuality](../../04_COMMON/10_DATA_AND_ID_RULES.md#type-netvaluequality) | 별도 DOMAIN 활성 평가 품질 | 의미 코드 · 무단위 | DOMAIN 활성 owner; generation은 수용한 DOMAIN Boot. 후방 거리 evidence·TX_SEQUENCE와 별개 |
| `rear_activation_reason` | [NetQualityReason](../../04_COMMON/10_DATA_AND_ID_RULES.md#type-netqualityreason) | 활성 평가 품질 사유 | 의미 코드 · 무단위 | DOMAIN 활성 owner; generation은 수용한 DOMAIN Boot. 후방 거리 evidence·TX_SEQUENCE와 별개 |
| `rear_activation_sequence` | `uint32_t` | 새 실제 활성 평가 번호 | 평가 번호 · 무단위 | DOMAIN 활성 owner; generation은 수용한 DOMAIN Boot. 후방 거리 evidence·TX_SEQUENCE와 별개 |
| `rear_activation_age_ms` | `uint16_t` | 활성 원 평가 이후 누적 ms, uint16 포화 | ms | DOMAIN 활성 owner; generation은 수용한 DOMAIN Boot. 후방 거리 evidence·TX_SEQUENCE와 별개 |

<a id="type-netvssstatus"></a>
### `NetVssStatus`

```c
typedef struct NetVssStatus
{
    NetVssState vss_state;
    NetVssAvailability vss_availability;
    NetFlag vss_accepting_events;
    NetFlag vss_fault_active;
    uint8_t fault_mask;
    uint16_t vss_last_fault;
    uint32_t last_fault_occurrence;
    uint16_t last_fault_age_ms;
    NetValueQuality last_fault_quality;
    uint32_t update_sequence;
    uint16_t source_age_ms;
} NetVssStatus;
```

| 멤버 | 정확한 C 타입 | 역할·범위 | 단위 | 원 owner·유효 조건 |
|---|---|---|---|---|
| `vss_state` | [NetVssState](10_DATA.md#type-netvssstate) | 현재 음향 서비스; STARTUP/READY/PLAYING/FAULT | 의미 코드 · 무단위 | VSS 서비스 owner; READY/PLAYING을 개별 이벤트 재생 DONE으로 해석 금지 |
| `vss_availability` | [NetVssAvailability](10_DATA.md#type-netvssavailability) | 현재 서비스 가용성; FULL/DEGRADED/UNAVAILABLE | 의미 코드 · 무단위 | VSS 서비스 owner; 입력 Event availability와 별개 |
| `vss_accepting_events` | [NetFlag](../../04_COMMON/10_DATA_AND_ID_RULES.md#type-netflag) | 현재 의미 Event/State 수용 가능, 0/1 | 0/1 의미값 · 무단위 | VSS 서비스 owner; 실제 음향 시작·완료 보장 아님 |
| `vss_fault_active` | [NetFlag](../../04_COMMON/10_DATA_AND_ID_RULES.md#type-netflag) | 현재 내부 출력 고장 활성, 0/1 | 0/1 의미값 · 무단위 | VSS 진단 owner; 최근 고장 이력이 있다는 이유로 1 생성 금지 |
| `fault_mask` | `uint8_t` | 현재 고장 bit0~4; 상위 bit 예약 | bit 집합 · 무단위 | VSS 진단 owner; 현재 발생 중인 고장만. 최근 이력 mask 아님 |
| `vss_last_fault` | `uint16_t` | 기동 이후 최근 주요 고장 code0~5 | 의미 코드 · 무단위 | VSS 진단 owner; current mask 복구 후에도 이력의 의미 유지 |
| `last_fault_occurrence` | `uint32_t` | 최근 고장 원 발생 번호 | 발생 식별자 · 무단위 | VSS 진단 owner; 같은 고장 반복 보고 때 유지 |
| `last_fault_age_ms` | `uint16_t` | 최근 고장 발생부터 누적 ms, uint16 포화 | ms | VSS 원 기록 + 송신 대기; 서비스 평가 Age와 별개 |
| `last_fault_quality` | [NetValueQuality](../../04_COMMON/10_DATA_AND_ID_RULES.md#type-netvaluequality) | 이력 있음=OK(0), 기록 없음=NO_DATA(3) | 의미 코드 · 무단위 | VSS 진단 owner; 최근 이력의 존재 품질. C2 faultQuality 전체 의미 투영은 OPEN |
| `update_sequence` | `uint32_t` | 새 서비스·현재 고장 평가 번호 | 평가 번호 · 무단위 | VSS 실제 평가에서만 갱신; CAN TX_SEQUENCE와 별개 |
| `source_age_ms` | `uint16_t` | 현재 서비스 평가 이후 누적 ms, uint16 포화 | ms | VSS 원 평가 + 송신 대기; last_fault_age_ms를 대체하지 않음 |

<a id="type-netvsseventkind"></a>
### `NetVssEventKind`

```c
typedef uint8_t NetVssEventKind;
enum NetVssEventKindCode
{
    NET_VSS_EVENT_WELCOME = 1,
    NET_VSS_EVENT_GOODBYE = 2,
    NET_VSS_EVENT_LOCK_COMPLETE = 3,
    NET_VSS_EVENT_UNLOCK_COMPLETE = 4,
    NET_VSS_EVENT_LOCK_ERROR = 5
};
```

이 타입은 `uint8_t` 별칭이며 뒤의 `enum`은 코드 상수 집합이다.

| 상수 | 제안 코드값 | 의미 |
|---|---|---|
| `NET_VSS_EVENT_WELCOME` | `1` | 웰컴 발생 |
| `NET_VSS_EVENT_GOODBYE` | `2` | 굿바이 발생 |
| `NET_VSS_EVENT_LOCK_COMPLETE` | `3` | 도어 잠금 완료 |
| `NET_VSS_EVENT_UNLOCK_COMPLETE` | `4` | 도어 잠금 해제 완료 |
| `NET_VSS_EVENT_LOCK_ERROR` | `5` | 실제 도어 잠금 실행 후 확인 실패 |

<a id="type-netvssstate"></a>
### `NetVssState`

```c
typedef uint8_t NetVssState;
enum NetVssStateCode
{
    NET_VSS_STATE_STARTUP = 0,
    NET_VSS_STATE_READY = 1,
    NET_VSS_STATE_PLAYING = 2,
    NET_VSS_STATE_FAULT = 3
};
```

이 타입은 `uint8_t` 별칭이며 뒤의 `enum`은 코드 상수 집합이다.

| 상수 | 제안 코드값 | 의미 |
|---|---|---|
| `NET_VSS_STATE_STARTUP` | `0` | 서비스 시작 중 |
| `NET_VSS_STATE_READY` | `1` | 서비스 준비 |
| `NET_VSS_STATE_PLAYING` | `2` | 재생 중 |
| `NET_VSS_STATE_FAULT` | `3` | 고장 상태 |

<a id="type-netvssavailability"></a>
### `NetVssAvailability`

```c
typedef uint8_t NetVssAvailability;
enum NetVssAvailabilityCode
{
    NET_VSS_AVAILABILITY_FULL = 0,
    NET_VSS_AVAILABILITY_DEGRADED = 1,
    NET_VSS_AVAILABILITY_UNAVAILABLE = 2
};
```

이 타입은 `uint8_t` 별칭이며 뒤의 `enum`은 코드 상수 집합이다.

| 상수 | 제안 코드값 | 의미 |
|---|---|---|
| `NET_VSS_AVAILABILITY_FULL` | `0` | 전체 가용 |
| `NET_VSS_AVAILABILITY_DEGRADED` | `1` | 가용성 저하 |
| `NET_VSS_AVAILABILITY_UNAVAILABLE` | `2` | 사용 불가 |

<a id="type-netrearwarningstate"></a>
### `NetRearWarningState`

```c
typedef uint8_t NetRearWarningState;
enum NetRearWarningStateCode
{
    NET_REAR_WARNING_CLEAR = 0,
    NET_REAR_WARNING_CAUTION = 1,
    NET_REAR_WARNING_EMERGENCY = 2
};
```

이 타입은 `uint8_t` 별칭이며 뒤의 `enum`은 코드 상수 집합이다.

| 상수 | 제안 코드값 | 의미 |
|---|---|---|
| `NET_REAR_WARNING_CLEAR` | `0` | 위험 해제 의미 |
| `NET_REAR_WARNING_CAUTION` | `1` | 후방 주의 |
| `NET_REAR_WARNING_EMERGENCY` | `2` | 후방 긴급 |

<a id="type-netvssfaultmask"></a>
### `NetVssFaultMask`

```c
enum NetVssFaultMask
{
    NET_VSS_FAULT_SOUND_ASSET_UNAVAILABLE = (1u << 0),
    NET_VSS_FAULT_PLAYBACK_START_FAILURE = (1u << 1),
    NET_VSS_FAULT_AUDIO_OUTPUT_FAILURE = (1u << 2),
    NET_VSS_FAULT_PLAYBACK_STATE_FAILURE = (1u << 3),
    NET_VSS_FAULT_INITIALIZATION_FAILURE = (1u << 4)
};
```

이 선언은 코드 상수 집합이다. 구조체 멤버의 실제 저장 폭은 해당 멤버 선언을 따른다.

| 상수 | 제안 코드값 | 의미 |
|---|---|---|
| `NET_VSS_FAULT_SOUND_ASSET_UNAVAILABLE` | `(1u << 0)` | 필요 음원 사용 불가 |
| `NET_VSS_FAULT_PLAYBACK_START_FAILURE` | `(1u << 1)` | 재생 시작 실패 |
| `NET_VSS_FAULT_AUDIO_OUTPUT_FAILURE` | `(1u << 2)` | 출력 경로 이상 |
| `NET_VSS_FAULT_PLAYBACK_STATE_FAILURE` | `(1u << 3)` | 재생 상태 관리 이상 |
| `NET_VSS_FAULT_INITIALIZATION_FAILURE` | `(1u << 4)` | 초기화 실패 |

<a id="type-netvssfaultcode"></a>
### `NetVssFaultCode`

```c
enum NetVssFaultCode
{
    NET_VSS_FAULT_CODE_NONE = 0,
    NET_VSS_FAULT_CODE_SOUND_ASSET_UNAVAILABLE = 1,
    NET_VSS_FAULT_CODE_PLAYBACK_START_FAILURE = 2,
    NET_VSS_FAULT_CODE_AUDIO_OUTPUT_FAILURE = 3,
    NET_VSS_FAULT_CODE_PLAYBACK_STATE_FAILURE = 4,
    NET_VSS_FAULT_CODE_INITIALIZATION_FAILURE = 5
};
```

이 선언은 코드 상수 집합이다. 구조체 멤버의 실제 저장 폭은 해당 멤버 선언을 따른다.

| 상수 | 제안 코드값 | 의미 |
|---|---|---|
| `NET_VSS_FAULT_CODE_NONE` | `0` | 없음 |
| `NET_VSS_FAULT_CODE_SOUND_ASSET_UNAVAILABLE` | `1` | 필요 음원 사용 불가 |
| `NET_VSS_FAULT_CODE_PLAYBACK_START_FAILURE` | `2` | 재생 시작 실패 |
| `NET_VSS_FAULT_CODE_AUDIO_OUTPUT_FAILURE` | `3` | 출력 경로 이상 |
| `NET_VSS_FAULT_CODE_PLAYBACK_STATE_FAILURE` | `4` | 재생 상태 관리 이상 |
| `NET_VSS_FAULT_CODE_INITIALIZATION_FAILURE` | `5` | 초기화 실패 |

<a id="vss-time-contract"></a>
## 메시지별 사용 조건

| 자료 | 호출 경계·소비 | 수명·확인해야 할 것 |
|---|---|---|
| `NetVssEvent` | [VssApp_OnDomainEvent](20_API.md#api-vssapp_ondomainevent) | 수용된 DOMAIN 기동 + occurrence. 같은 발생은1회 의미로 소비; `UPDATE_SEQUENCE`가 없는 wire에 새 발생 번호를 추정하지 않음 |
| `NetVssWarningState` | [VssApp_OnDomainWarningState](20_API.md#api-vssapp_ondomainwarningstate) | 세 경고의 품질·Age·source 독립. 후방은 유효 ACTIVE와 유효 rear evidence를 함께 요구 |
| `NetVssStatus` | [VssNet_PublishStatus](20_API.md#api-vssnet_publishstatus) | 현재 service/fault와 last_fault 이력 별개. 송신 미시작 pending 현재 상태만 최신 평가로 교체 가능 |
| `NetDomainPermission` | [VssNetPriv_OnDomainPermission](20_API.md#api-vssnetpriv_ondomainpermission) | [공통 전체7필드](../../04_COMMON/10_DATA_AND_ID_RULES.md#type-netdomainpermission). 기본 소비는 Network 수신 감시. App 소비 필요성·항목은 FUNCTION_POLICY_OPEN |

`VSS_WARNING_STATE`는 사용60B/전송64B/DLC15의 **WIRE_PROPOSED** 정본 후보다. format0/미지원은 후방 활성 근거가 없다는 뜻이며 다른 유효 경고를 일괄 폐기하지 않는다. 구 VSS receiver와 새 DOMAIN encoder의 혼용은 지원하는 것으로 간주하지 않는다. 양측 Codec·정책을 함께 확정해야 한다. [CAN 상세](../../03_COMMUNICATION/10_CAN_FD.md#message-vss-warning-state).

후방 활성 근거 사용 한도150ms는 기존 활성 근거 계약을 따른다. frame 수신 중단150ms와 같은 숫자여도 다른 시간 개념이다. 후방 원 근거150ms/끼임250ms/탑승자1000ms 및 Event500ms 등 나머지 일반 후보 시간은 [CAN 설정](../../03_COMMUNICATION/10_CAN_FD.md)의 PROPOSED 범위를 따른다.

**CONFLICT / FUNCTION_POLICY_OPEN — 일회성 시간:** Network `VSS_EVENT.USE_LIMIT=500ms` 후보와 구 SysRS의 원 발생→실제 음향 시작 2초 미만 잠정 기준은 적용 위치·허용 범위가 다르다. Network 전달·VSS 대기·출력 준비를 포함하는 실제 시작 Age를 검증해야 한다. 500ms를 2초로 임의 상향하거나 2초 이내라는 이유로 만료 Event를 재유효화하지 않는다. DOMAIN/VSS/Network 경계에서 전달 수용 한도와 실제 시작 한도의 합성·우선 규칙이 확정돼야 한다. Wire 숫자 후보는 최종 음향 정책 확정을 뜻하지 않는다.

<a id="vss-source-conversion"></a>
## R4/R6 서비스·C2 변환 — BINDING_OPEN

별도 VSS [진단·보고 논리 설계](https://github.com/vehicle-domain-control-system/integrated-vehicle-control/blob/develop/docs/vss/design/40_DATA/70_DIAGNOSTIC_DATA.md#vssstatussnapshot)와 [고장·복구 계약](https://github.com/vehicle-domain-control-system/integrated-vehicle-control/blob/develop/docs/vss/design/50_CONTRACTS/40_FAULT_RECOVERY.md#report-and-tbd)은 참조할 수 있다. 이 자료의 Snapshot·Header label은 논리 후보이며 실제 C Header·Firmware 구현의 증거가 아니다. HEALTH가 보고를 생성하고 FLOW가 기존 INPUT 통신 연결로 전달하는 책임을 보존한다. HEALTH→Network 직접 호출을 새 요구로 만들지 않는다.

| C2 측 기존 자료 | 로컬 후보 자료 | 보존·변환 결정 |
|---|---|---|
| `DomainIf_VssOneShotEvent_t` | `NetVssEvent` | Event enum C0~4→raw1~5; `hasRelatedCommand` false이면 wire related0. `meta.updateSequence`는 Event wire/중복 식별에 없음 |
| `DomainIf_VssWarningState_t` | `NetVssWarningState` | 각 evidence의 generation/sequence/Age/quality/reason/availability 보존. rearActivationMeta는 별도 DOMAIN 평가. R4 입력 단위·시간 변환 owner OPEN |
| `DomainIf_VssStateReport_t` / 기존 `VssStatusSnapshot` 의미 | `NetVssStatus` | Snapshot→DTO 인계 형식·변환 위치·목록축약 owner OPEN. mask·최근 code/occurrence/Age/quality 원 근거 필요 |
| C2 `service.meta.quality`, `faultQuality`, `sourceGeneration` | 현재 Status wire에는 같은 폭·멤버가 모두 존재하지 않음 | 현재 서비스 품질·부분진단·lineage reduction을 단순 cast나 정상0으로 보완하지 않음. DOMAIN ingress 매핑 조건 미확정 |

`lastFaultAgeMs:uint32_t`→wire `last_fault_age_ms:uint16_t`는65535 포화하고 원 이력 Age를 보존한다. TX source_age 역시 새로운 생성시점으로 바꾸지 않는다. Status 변환의 책임이 정해지지 않았으므로 [PublishStatus](20_API.md#api-vssnet_publishstatus) 인자 후보가 App 선행변환이나 Network 변환을 확정하지 않는다.

<a id="r2-status-proposal"></a>
### 기존 DTO를 통한 Status 보고 권장안 — PROPOSED

**NETWORK_OWNED:** 기존 `NetVssStatus`와 `VssNet_PublishStatus()`를 재사용한다. **CROSS_NODE_AGREEMENT:** VSS/DOMAIN은 아래 축약의 완전성·품질·원 세대 연결을 확인한다. **FUNCTION_OWNER:** HEALTH가 서비스·현재/최근 고장의 사실과 제한을 생성한다. **IMPLEMENTATION_BINDING:** 기존 INPUT 통신 경계의 변환 위치·복사/참조·송신 실패 관측을 연결한다. 아래는 `PROPOSED`이며 해당 양단의 `AGREED`, 실제 source의 `IMPLEMENTED`, Codec/Board의 `TESTED` 근거는 확인되지 않았다. 실제 Firmware/Codec/Board 시험은 `NOT_EXECUTED`다.

권장 순서는 HEALTH의 `VSS_STATUS_CREATED` 보고 → FLOW → 기존 INPUT 통신 연결에서 원 Snapshot·진단 근거 대조 → 기존 DTO 제출 → Network 직렬화다. INPUT 연결 안의 변환 또는 기존 Network Adapter 중 실제 source가 재사용 가능한 한 곳에만 둔다. 새 공개 변환 함수·Snapshot DTO·진단 관리자는 추가하지 않는다. Snapshot과 목록/basis는 복사 완료 또는 실제 송신 소비 종료까지 보호한다. `VSS_STATUS_REEVALUATE`는 새 정상 보고를 생성하지 않는 결과이며, 이전 Snapshot의 Age/평가 번호를 새 것으로 바꾸는 근거가 아니다.

| 기존 원 자료 → DTO 필드 | 가장 단순한 권장 처리·누락 시 제한 |
|---|---|
| Snapshot `state` → `vss_state` | STARTUP/READY/PLAYING/FAULT의 **의미 이름**을 기존 Net 코드로 명시 변환한다. enum cast를 쓰지 않는다. PLAYING은 원 owner가 확인한 Session 보고이며 개별 Event DONE이나 현재 매 순간 실제 소리가 난다는 뜻은 아니다. |
| Snapshot `availability` → `vss_availability` | 원 HEALTH의 확정된 외부 가용성 의미만 기존 FULL/DEGRADED/UNAVAILABLE에 대응한다. `serviceLevel=FULL`이나 mask0만으로 FULL을 만들지 않는다. startup 미평가·전체 조합 reduction이 미정이면 해당 새 보고를 확정하지 않는다. |
| Snapshot `accepting` → `vss_accepting_events` | INPUT/STORE의 반영된 수용 근거를 그대로 0/1로 옮긴다. 이것은 개별 수용·재생 시작·복구 완료 보장이 아니다. false라도 기존 duplicate·유효 CLEAR·품질/기한의 안전 처리 경로는 유지한다. |
| 완전성이 확인된 `currentFaults` → `fault_mask`, `vss_fault_active` | 원 5종 이름을 이 문서의 같은 이름 `NET_VSS_FAULT_*` bit로 변환하고 현재 항목만 OR한다. 여러 대상에서 같은 종류면 bit 하나이며 원 instance/대상 정보는 HEALTH에 보존한다. 현재 목록의 완전성이 확인된 경우에만 빈 목록→mask0/current inactive를 사용할 수 있다. 최근 목록을 합쳐 current bit를 만들지 않는다. |
| HEALTH의 원 현재/최근 Fault 기록 → `vss_last_fault`, `last_fault_occurrence`, `last_fault_age_ms`, `last_fault_quality` | 권장 선택은 해당 기동의 원 기록 중 실제 발생 순서로 최근임이 확인된 주요 기록 하나다. 그 기록의 kind를 기존 code로 명시 변환하고 원 occurrence·원 발생 AGE를 함께 보존한다. 복구로 current가 해제돼도 이력의 발생은 바뀌지 않는다. Snapshot의 `recentFaults`는 kind 목록뿐이므로 목록 순서에서 occurrence·발생 시각을 추정할 수 없다. 원 기록 연결·주요 기록 선택이 미확인인 경우 이력 투영을 확정하지 않는다. `NO_DATA=3`은 확인된 기록 부재일 때만 사용한다. |
| 원 보고 basis/평가 → `update_sequence`, `source_age_ms` | 실제 새 반영 완료 평가 번호와 평가부터 경과 시간을 기존 필드에 연결한다. basis를 uint32로 cast하거나 CAN TX_SEQUENCE·수신 now로 대체하지 않는다. 원 번호/시각 연결이 미확인인 경우 새 fresh Status를 제출하지 않는다. AGE는 65535 포화이며 원 사용 한도 검증은 공통 규칙을 따른다. |
| 원 VSS 보고 → DOMAIN C2/모바일 | 원 CAN BOOT/TX·UPDATE/AGE를 함께 보존한다. C2 `service.meta.quality`, `faultQuality`, `sourceGeneration`은 해당 원 의미 근거가 있을 때만 채운다. Link 수신 성공이나 이력 존재 raw0은 서비스·전체 Fault 품질 OK를 증명하지 않는다. VSS 원 세대와 수용 CAN BOOT의 실제 lifecycle 대응을 양단이 확인하기 전 자동 동치로 처리하지 않는다. |

**부분 품질의 안전 제한:** 기존 `LAST_FAULT_QUALITY` Wire는 이력 존재0/부재3만 표현하며 current service 품질·부분 진단의 `INVALID`를 표현하는 필드가 아니다. C2/로컬 `faultQuality=INVALID`를 이력0/3이나 READY/mask0으로 바꾸지 않는다. Wire B21에 그 외 값을 받은 경우 지원하는 정상 이력 형식으로 수용하지 않는다. 현재 서비스/현재 고장/평가 메타의 필수 근거가 부족하면 해당 새 `VSS_STATUS`의 fresh 제출과 그에 의존하는 C2/`M_VSS_STATUS` 투영을 제한한다. 원본 근거가 다른 유효 Event/Warning까지 일괄 비지원 처리하지 않는다. 마지막 확정 보고는 원 평가 번호·원 AGE를 유지하고 경과만 한 번 누적하며 현재 입력 상실을 숨기는 fresh 근거로 쓰지 않는다. 수신자는 기존 freshness/timeout 기준으로 더 이상 현재 근거가 아님을 처리한다.

원 보고 목록이 현재 제한 전체를 포함하는 조건, 최근 주요 기록의 선택·원 occurrence/시각과 새 보고 번호/시각은 VSS HEALTH의 관측 근거가 필요하다. VSS INPUT/Network는 보호 자료를 한 번 변환·복사하고 실패를 FLOW에 인계해야 한다. DOMAIN 소비는 축약된 이력 품질과 서비스/전체 Fault 품질을 구분하고 원 세대 대응·부분 품질에 따른 투영 제한을 적용해야 한다. `lastFaultCode`로 current bit를 합성하거나 enum cast·기본 0으로 누락 품질/시각을 보완하면 허위 정상이 되므로 금지한다. 완전한 원 보고를 명시 변환하는 기존 DTO 재사용은 권장안이며 위 조건이 확인되기 전 미표현 품질을 NORMAL로 보완하지 않는다.

<a id="vss-input-recovery"></a>
## 입력 상실·복구

INVALID/STALE/NO_DATA 또는 기한이 지난 의미로 새 음향을 시작하지 않는다. 기존 지속 경고는 그 원본 기한·품질 안에서만 제한적으로 유지하며 더 이상 사용할 수 없으면 음향을 중단한다. 음향 중단은 차량 위험 CLEAR가 아니다. 복구 전 일회성 Pending은 폐기하고 중단된 일회성 음향도 자동 재개하지 않는다. 복구 후 새로 수신한 유효 현재 Warning을 기준으로 다시 중재한다. 새 Event는 기존 원 Age·사용한도·품질·가용성을 모두 만족해야 한다. 비주기 Event가 발생하지 않는 상태는 정상이다.

WINDOW 끼임 제품 연결은 **[구현 보류 — 설계 유지]**다. VSS 단품 후보를 검토해도 WINDOW 실물 E2E PASS를 주장하지 않는다.
