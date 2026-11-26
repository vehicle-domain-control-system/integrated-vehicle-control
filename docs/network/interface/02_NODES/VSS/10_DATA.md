# VSS Data — 전체 타입과 필드

Interface v2 · r1 · [규격과 연동 조건](../../07_CROSS_NODE_INTERFACE_RULES.md#status)

VSS는 DOMAIN의 확정 Event·Warning을 받아 음향 출력을 중재하고, 자신의 서비스·고장 상태를 보고한다. 위험 판정은 DOMAIN, 음향 선택·실제 출력 상태는 VSS가 소유한다. **TX App은 링크 BOOT_ID/TX_SEQUENCE를 채우지 않는다.** RX에서 수신한 원본 출처의 BOOT는 판단 근거의 식별이며 링크 송신 Boot와 목적이 다르다.

[노드 개요](00_OVERVIEW.md) · [Data](10_DATA.md) · [API](20_API.md) · [전체 시작](../../00_START_HERE.md) · [공통 의미·수명](../../04_COMMON/10_DATA_AND_ID_RULES.md) · [CAN FD](../../03_COMMUNICATION/10_CAN_FD.md)

모든 아래 신규 로컬 타입은 **PROPOSED_DESIGN / BINDING_OPEN**이다. WINDOW 관련 경계는 **[구현 보류 — 설계 유지]**다. `Net*` 별칭의 숫자는 이 초안의 의미 코드이고 실제 C2 enum은 [명시 변환표](../../04_COMMON/30_ENUM_WIRE_MAPPING.md)로 변환한다. C 구조체의 패딩·enum 폭을 포함한 `memcpy` 전송은 금지한다. `<stdint.h>`와 공통 Net 타입은 [공통 Data](../../04_COMMON/10_DATA_AND_ID_RULES.md)에서 읽는다.

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
| `use_limit_ms` | `uint16_t` | 일회성 원 사용 한도 ms; A1 설계 프로필 500ms | ms | 최초 actual에 L=min(검증된 양의 USE_LIMIT,500ms,2000ms), 0 비사용·L 도달 만료. 실제 시각/Binding 검증 대기 |
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

`VSS_WARNING_STATE`는 **A1 내부 선택 WARNING60**(사용60B/전송64B/DLC15)이다. WARNING51은 이력이며 DLC/B51로 자동 감지하거나 혼용하지 않는다. format1은 독립 활성 meta를 검증하고, format0은 B50=0 및 B52..59=0인 정규 근거 부재다. format0의 0을 활성 DISABLED/품질 OK의 새 판단으로 소비하지 않으며 다른 세 유효 경고는 독립 검증한다. 그 외 marker·비정규 부재는 프레임 오류다. 실제 DOMAIN producer/VSS receiver·ABI 지원은 검증 대기다. [CAN 상세](../../03_COMMUNICATION/10_CAN_FD.md#message-vss-warning-state).

A1 입력 설계는 끼임 원 TTL/Hold 250/250ms(TEST만, 제품 경로 보류), 탑승자 1000/500ms, 후방 위험 150/150ms, 후방 활성 500/0ms를 선택한다. 후방 활성은 DOMAIN Power의 실제 약200ms 평가/원500ms에 연결하며 다른 정책이면 별도 생성 주기≤200ms·원500ms 근거가 필요하다. frame no-RX150ms와 후방 위험 TTL150ms를 활성 TTL로 바꾸지 않는다. 실제 생산·Clock·경계 시험은 미검증이다.

**A1 Event 원 기한:** `L=min(검증된 양의 USE_LIMIT, 500ms 설계 프로필, 2000ms 상한)`이고 USE_LIMIT0은 사용 불가, 정확히 L부터 만료다. 원 발생→Network 전달→VSS 대기→최초 실제 음향 시작까지 같은 기한을 사용한다. 적법하게 Started가 확인된 같은 Session의 후속 cue에는 최초 시작 검사를 다시 적용하지 않는다. in-flight 기한 경과를 전체 NO_START로 바꾸거나 이미 시작한 사실을 지우지 않는다. 실제 원 시각 구간·actual 시각·오차 근거가 없으면 해당 fresh 사용을 제한한다.

<a id="vss-source-conversion"></a>
## R4/R6 서비스·C2 변환 — BINDING_OPEN

별도 VSS [진단·보고 논리 설계](https://github.com/vehicle-domain-control-system/integrated-vehicle-control/blob/develop/docs/vss/design/40_DATA/70_DIAGNOSTIC_DATA.md#vssstatussnapshot)와 [고장·복구 계약](https://github.com/vehicle-domain-control-system/integrated-vehicle-control/blob/develop/docs/vss/design/50_CONTRACTS/40_FAULT_RECOVERY.md#report-and-tbd)은 참조할 수 있다. 이 자료의 Snapshot·Header label은 논리 후보이며 실제 C Header·Firmware 구현의 증거가 아니다. HEALTH가 보고를 생성하고 FLOW가 기존 INPUT 통신 연결로 전달하는 책임을 보존한다. HEALTH→Network 직접 호출을 새 요구로 만들지 않는다.

| C2 측 기존 자료 | 로컬 후보 자료 | 보존·변환 결정 |
|---|---|---|
| `DomainIf_VssOneShotEvent_t` | `NetVssEvent` | Event enum C0~4→raw1~5; `hasRelatedCommand` false이면 wire related0. `meta.updateSequence`는 Event wire/중복 식별에 없음 |
| `DomainIf_VssWarningState_t` | `NetVssWarningState` | 각 evidence의 generation/sequence/Age/quality/reason/availability 보존. rearActivationMeta는 별도 DOMAIN 평가. A1 기존 INPUT이 의미·시간 변환을 맡으며 실제 원 source/clock/consumer는 E02~E04 확인 대기 |
| `DomainIf_VssStateReport_t` / 기존 `VssStatusSnapshot` 의미 | `NetVssStatus` | A2 기존 INPUT 단일 변환 선택. HEALTH가 완전 current/원 최근 이력·평가를 생산하며 실제 원형·세대·clock·DOMAIN profile은 E-S01~05 확인 대기 |
| C2 `service.meta.quality`, `faultQuality`, `sourceGeneration` | 현재 Status wire에는 같은 폭·멤버가 모두 존재하지 않음 | 현재 서비스 품질·부분진단·lineage reduction을 단순 cast나 정상0으로 보완하지 않음. DOMAIN ingress 매핑 조건 미확정 |

`lastFaultAgeMs:uint32_t`→wire `last_fault_age_ms:uint16_t`는65535 포화하고 원 이력 Age를 보존한다. TX source_age 역시 새로운 생성시점으로 바꾸지 않는다. A2의 변환 owner는 기존 INPUT이며 [PublishStatus](20_API.md#api-vssnet_publishstatus)는 완전한 기존 NetVssStatus만 받는다. 원 Snapshot을 받는 공개 API를 추가하지 않고 실제 복사·수명·송신 관측은 E-S04에서 확인한다.

<a id="r2-status-proposal"></a>
### 기존 DTO를 통한 A2 Status 생산·수용 — 내부 선택 / BINDING_OPEN

**내부 선택:** HEALTH는 서비스·현재/최근 고장의 완전하고 일관된 원 보고를 생산하고, FLOW→기존 INPUT 한 곳에서 `NetVssStatus` 11필드로 명시 변환한다. Network는 원 key·두 AGE의 기준·전체 DTO를 반환 전에 자기 저장소에 복사한다. 새 공개 변환 API·Manager·동적 Queue를 추가하지 않는다. 실제 생산 완전성(E-S01), 원 VSS 세대와 CAN BOOT 대응(E-S02), 시간(E-S03), 복사·terminal sink(E-S04), DOMAIN 설치 profile·consumer(E-S05)는 별도 확인 대기이며 Firmware/Host/Board 시험은 `NOT_EXECUTED`다.

`VSS_STATUS_CREATED`는 완전 current·원 최근 이력·state/availability/accepting·평가 시각/번호가 함께 확보된 새 완료 평가다. 단순 tick·읽기·재전송은 reportSequence를 증가시키지 않는다. 불완전 조합은 `VSS_STATUS_REEVALUATE`/written0으로 남겨 이전 번호·시각을 바꾸지 않는다. HEALTH 기존 보고 책임의 정적2슬롯은 생성용/미인계용으로 구분하며 current/recent kind 각각 최대5개와 원 basis/stamp를 보호한다. 원 Fault pool과 stamp 고정 상한 초과도 REEVALUATE다. 아직 미접수인 보고만 더 새 coherent 보고로 교체할 수 있고 accepted/Driver 참조 중 사본은 바꾸지 않는다.

| 기존 원 자료 → DTO 필드 | A2 내부 선택·근거 누락 시 제한 |
|---|---|
| Snapshot `state` → `vss_state` | 의미 이름으로 대응한다. 우선순위는 QUARANTINED/불명확 최종 또는 확인된 Fault로 공통 정상 출력·새 start 불가→FAULT, 확인된 초기화 진행→STARTUP, 초기화 완료·적법 actual로 시작한 Session이 retirement 전→PLAYING, 초기화 완료·Started Session 없음·하나 이상 서비스 가능→READY다. 어떤 조건도 입증 못 하면 REEVALUATE이며 current bit만으로 FAULT를 정하지 않는다. PLAYING의 cue 무음/STOPPING은 개별 DONE이 아니다. |
| Snapshot `availability` → `vss_availability` | 첫 두 상태 조건은 UNAVAILABLE, PLAYING/READY는 실제 지원 집합의 전체 준비→FULL·일부 준비→DEGRADED다. 보류 WINDOW는 지원 집합에 넣지 않는다. 초기화 완료·무고장인데 출력 가능한 서비스가 전혀 없으면 원 모순/부족으로 REEVALUATE한다. mask0/serviceLevel만으로 FULL을 합성하지 않는다. |
| Snapshot `accepting` → `vss_accepting_events` | 출처/scope가 검증된 INPUT 준비와 STORE 추적/replay 준비의 결합이다. 둘 다 true일 때 true, 확인된 false 하나가 있으면 false, 그 외 NULL unknown 조합은 REEVALUATE다. false가 다른 NULL을 false로 확정하지 않으며 필수 다른 보고 근거는 독립 요구다. FAULT/UNAVAILABLE과 accepting=true도 가능하나 새 Event/출력 보장은 아니다. duplicate/CLEAR/품질/기한 정리는 계속한다. |
| 완전성이 확인된 `currentFaults` → `fault_mask`, `vss_fault_active` | 원 5종 이름을 이 문서의 같은 이름 `NET_VSS_FAULT_*` bit로 변환하고 현재 항목만 OR한다. 여러 대상에서 같은 종류면 bit 하나이며 원 instance/대상 정보는 HEALTH에 보존한다. 현재 목록의 완전성이 확인된 경우에만 빈 목록→mask0/current inactive를 사용할 수 있다. 최근 목록을 합쳐 current bit를 만들지 않는다. |
| HEALTH의 원 현재/최근 Fault 기록 → `vss_last_fault`, `last_fault_occurrence`, `last_fault_age_ms`, `last_fault_quality` | Fault5의 현재/해제 원 기록 중 최초 발생시각이 가장 늦음이 입증된 하나를 선택한다. 정확히 같은 시각만 큰 reportOccurrence로 정하고 불확실 구간 겹침은 REEVALUATE다. 뒤늦게 온 과거 진단의 큰 번호로 최신 이력을 바꾸지 않는다. 원 kind/occurrence/최초 시각은 current 해제 뒤에도 보존하며 목록 순서에서 추정하지 않는다. `lastFaultPresent=false`가 확인된 경우 무효 lastFaultKind/lastFaultAt을 읽지 않고 code/occurrence/historyAGE0·quality3을 함께 만든다. 존재 quality0일 때만 historyAGE를 누적한다. 원 기록/시각 미확인은 부재3으로 바꾸지 않는다. |
| 원 보고 basis/평가 → `update_sequence`, `source_age_ms` | 실제 새 반영 완료 평가 번호와 평가부터 경과 시간을 기존 필드에 연결한다. basis를 uint32로 cast하거나 CAN TX_SEQUENCE·수신 now로 대체하지 않는다. 원 번호/시각 연결이 미확인인 경우 새 fresh Status를 제출하지 않는다. AGE는 65535 포화이며 원 사용 한도 검증은 공통 규칙을 따른다. |
| 원 VSS 보고 → DOMAIN C2/모바일 | 원 CAN BOOT/TX·UPDATE/AGE를 함께 보존한다. C2 `service.meta.quality`, `faultQuality`, `sourceGeneration`은 해당 원 의미 근거가 있을 때만 채운다. Link 수신 성공이나 이력 존재 raw0은 서비스·전체 Fault 품질 OK를 증명하지 않는다. VSS 원 세대와 수용 CAN BOOT의 실제 lifecycle 대응을 양단이 확인하기 전 자동 동치로 처리하지 않는다. |

**부분 품질의 안전 제한:** 기존 `LAST_FAULT_QUALITY` Wire는 이력 존재0/부재3만 표현하며 current service 품질·부분 진단의 `INVALID`를 표현하는 필드가 아니다. C2/로컬 `faultQuality=INVALID`를 이력0/3이나 READY/mask0으로 바꾸지 않는다. Wire B21에 그 외 값을 받은 경우 지원하는 정상 이력 형식으로 수용하지 않는다. 현재 서비스/현재 고장/평가 메타의 필수 근거가 부족하면 해당 새 `VSS_STATUS`의 fresh 제출과 그에 의존하는 C2/`M_VSS_STATUS` 투영을 제한한다. 원본 근거가 다른 유효 Event/Warning까지 일괄 비지원 처리하지 않는다. 마지막 확정 보고는 원 평가 번호·원 AGE를 유지하고 경과만 한 번 누적하며 현재 입력 상실을 숨기는 fresh 근거로 쓰지 않는다. 수신자는 기존 freshness/timeout 기준으로 더 이상 현재 근거가 아님을 처리한다.

DOMAIN은 같은 원 `K=(VSS 생산 세대, 실제 평가 sequence)`에서 State와 Fault 결과를 독립 기록한다. 현재 mask의 대표 Fault는 정의된 활성 code1..5 중 최소 code이며 최근 이력을 현재 고장으로 쓰지 않는다. 수용한 State는 Fault 실패로 롤백·재호출하지 않고 같은 K 재수신은 새 Rx 호출을 만들지 않는다. CALLING/UNKNOWN_FAILED는 부작용 확인 전 자동 재시도하지 않으며 old K의 늦은 완료가 새 상태를 덮지 않도록 fence한다. v0.5 service-only State와 observer-only Fault를 확장 C2 전체 이력 소비 완료나 Core APPLIED로 표시하지 않는다. [DOMAIN 수용 정본](../../../sw_detail/functions/can/VSS_STATUS.md#a2-domain-contract)의 조건과 E-S05를 따른다.

Network local OK는 전체 사본 접수일 뿐 TX/DOMAIN 성공이 아니다. 접수 전 terminal 관측 공간과 INPUT sink 준비를 확인하고 미준비는 NOT_READY, 전체 미접수·공간 부족은 NO_CAPACITY다. 기존 M09 terminal 관측의 원 key·BOOT/TX·raw·단계·참조 종료 여부를 INPUT 기존 진단16/loss latch에 보호한 뒤 FLOW의 FOLLOW_UP_REQUIRED/RECOLLECT로 연결한다. 이 bounded drain은 RX/새 CREATED 유무와 독립이며 IRQ에서 FLOW/HEALTH를 호출하지 않는다. 통신 실패를 AUDIO_OUTPUT_FAILURE로 만들거나 옛 보고를 새 평가로 재실행하지 않는다. 실제 sink/직렬화는 E-S04 미검증이다.

<a id="vss-input-recovery"></a>
## 입력 상실·복구

INVALID/STALE/NO_DATA 또는 원 기한이 지난 의미로 새 음향을 시작하지 않는다. 지속 Warning의 제한 Hold는 아래 A1 조건에 한정되며 정상 유효값의 새 평가가 아니다. 음향 중단은 차량 위험 CLEAR가 아니다. 입력 신뢰 상실 전 일회 Pending의 재생 자격은 닫고 occurrence별 원 이력/ledger와 replayAllowed=false를 보존한다. Started/Interrupted/uncertain final은 자동 재개하지 않는다. 비주기 Event 침묵 자체는 loss가 아니며 실제 DOMAIN 감독 상실·명시 품질·계보·시간 상실과 구별한다. 출력 장치 Fault만 있고 입력 신뢰/replayAllowed가 유지되면 전체 NO_START·retirement·HEALTH 제한 해제·원 L 내 freshness를 모두 확인한 같은 occurrence만 재평가한다.

WINDOW 끼임 제품 연결은 **[구현 보류 — 설계 유지]**다. VSS 단품 후보를 검토해도 WINDOW 실물 E2E PASS를 주장하지 않는다.

<a id="a1-binding"></a>
## A1 입력 연결·시간·복구의 한정 계약

Event key는 (PRODUCT 또는 TEST, DOMAIN, 수용한 DOMAIN Boot, occurrence_id)다. `event_kind`는 같은 key의 내용 검증에 사용하며 종류만 달라진 입력은 새 사건이 아닌 내용 충돌이다. CAN TX는 전달 순서이고 occurrence·원 센서 sequence에서 DOMAIN 새 판단 순번을 만들지 않는다. related_command_id는 별도 상관 값이다. VSS 재기동 때 이전 ledger의 신뢰 가능한 복원 또는 동기화 이후 DOMAIN의 실제 새 승인 세대 없이 옛 Event를 받지 않는다. 이는 기존 VSS lifecycle 요구이며 CIS Epoch 전환을 다른 노드로 확장하지 않는다.

Network가 callback에 넘기는 Age는 `meta.received_at_ms`의 **RX 기준**이다. 원 생산→송신 대기→검증된 전달 공백을 포함하고, callback 직전 임시 소비 Age는 admission에만 사용한다. INPUT이 RX→owner 소비 경과를 한 번 더하므로 callback payload를 임시 Age로 덮어쓰지 않는다. RX 시각 구간 `[rLo,rHi]`와 RX Age 구간 `[aLo,aHi]`가 확인되면 원 시각은 `[rLo-aHi,rHi-aLo]`이며 65535는 하한 포화다. ECU 절대시각 동기화를 가정하지 않고 구간/epoch/경과 근거가 없으면 해당 시간 사용을 제한한다. CAN 전달5ms/owner10ms는 설계 예산으로 실제 보장이 아니다.

Event 최초 START 직전에는 `now.latestNow-originalAt < L`와 신뢰/replayAllowed/권한이 필요하다. 충분한 물리 actual·당시 권한·같은 clock/epoch를 먼저 확인하고 `actual.earliest >= originalAtLatest` 및 `actual.latest-originalAt < L`일 때만 적법 Started다. 구간 전체가 원 L 밖이면 확정 위반·정리, 경계 걸침/비교 부족은 불확실로 남겨 원 actual/coverage를 보존하고 차단·정리한다. 300ms 수신 뒤700ms 최초 actual은 부적법하지만 이미 적법 Started인 같은 Session의 후속 cue700ms는 최초 gate 대상이 아니다.

세 위험과 후방 활성의 네 lane은 값·quality·원 sequence·AGE를 독립 보존한다. 최초 NO_DATA는 UNASSESSED, 유효 이력 후 NO_DATA는 INVALID로 제한하며 비정상 품질의 state0은 CLEAR가 아니다. 정상값은 OK·reason NONE·유효 출처/시간·허용된 availability를 함께 요구한다. [VSS 한정 제한 투영](../../04_COMMON/30_ENUM_WIRE_MAPPING.md#vss-warning-limited-projection)은 원 raw/사유를 별도 보존하며 다른 노드의 enum 축약을 허용하지 않는다. 원 sourceBoot0의 정규 상실과 frame DOMAIN BOOT0 오류를 구별한다.

증가한 CAN TX의 같은 source sequence라도 새 품질 제한은 전달한다. 의미가 같은 반복은 liveness만 갱신하고 원 기한을 늘리지 않는다. 같은 원 evidence에서 정상값 변경·정상 복귀는 DOMAIN 판단 순서 증거(E01)가 있어야 한다. 후방 활성 상실은 현재 위험을 fence하며 새 활성만으로 옛 후방 위험을 복원하지 않고 엄격히 새 source key의 위험을 요구한다.

Warning no-RX는 구조 검증되고 TX가 전진한 마지막 수신을 기준으로150ms다. Event 무발생은 이 timeout 대상이 아니다. 유효 경고의 최초 상실 시 Hold anchor=`min(최초 상실, 마지막 validUntil)`를 고정하고 반복 상실로 연장하지 않는다. 원 freshness가 끝난 Hold는 이전 유효 경고의 제한 보유이며 새 정상값/새 Event 권한이 아니다. 같은 clock/epoch·경과 근거 아래 `latestNow < anchor+Hold`만 Hold 내, `earliestNow >= anchor+Hold`는 CONFIRMED_EXPIRED다. 경계 걸침·시간/출처 미입증은 UNCERTAIN/newSessionAllowed=false이며 현재 Session 유지 허가로도 쓰지 않는다. 복구 시 새 유효 현재 Warning으로 재중재하거나 같은 세대·신뢰·원 Hold 기한·Rear gate/fence 자격이 남은 후보만 old 전체 retirement 뒤 새 Session 정책 시작점에서 재선택한다. PB의 새 채택·최초 START 직전에도 최신 candidate revision/newSessionAllowed를 재확인한다. 후방 활성은 Hold0이고 WINDOW 끼임은 격리 TEST 전용이다.

Event1/Warning최대4의 Evidence·QualityChange·StateUpdate는 기존 직렬 owner의 동기 borrow bundle이다. STORE는 필요한 원 meta·기한·최초 상실을 복사한 뒤 반환한다. 입력 NULL/새 입력 없음도 기존 INPUT 처리 기회에서18 context의 원 만료·후속 작업을 예산 내 방문하며 새 ISR/Task/Queue를 요구하지 않는다. replay ledger128/진단16의 고정 예산과 overflow 시 첫/최근·count/loss latch는 실제 자원 검증 전이다.

E01(DOMAIN 판단/동일 evidence 순서), E02(원 source/C2 연결), E03(후방 활성 실제 생산), E04(clock·actual/NO_START·자원), E05(lifecycle) 및 E-S01~05는 실제 Binding·Host/Board 미검증으로 유지한다.
