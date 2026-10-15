# WINDOW Data — 전체 타입과 필드

Interface v2 · r1 · [규격과 연동 조건](../../07_CROSS_NODE_INTERFACE_RULES.md#status)

**[구현 보류 — 설계 유지]**. DOMAIN은 상위 허용·확정 목표를 만들고 WINDOW는 실제 이동·위치·끝단·끼임 보호·결과·고장 근거를 만든다. State, 새 Event, 원 명령 Result, 현재/복구 Fault를 각각 유지한다. 로컬 STOP·끼임 보호는 CAN 수신·DOMAIN 허용·응답을 기다리지 않는다.

[노드 개요](00_OVERVIEW.md) · [Data](10_DATA.md) · [API](20_API.md) · [전체 시작](../../00_START_HERE.md) · [공통 의미·수명](../../04_COMMON/10_DATA_AND_ID_RULES.md) · [CAN FD](../../03_COMMUNICATION/10_CAN_FD.md)

모든 아래 신규 로컬 타입은 **PROPOSED_DESIGN / BINDING_OPEN**이다. WINDOW 관련 경계는 **[구현 보류 — 설계 유지]**다. `Net*` 별칭의 숫자는 이 초안의 의미 코드이고 실제 C2 enum은 [명시 변환표](../../04_COMMON/30_ENUM_WIRE_MAPPING.md)로 변환한다. C struct padding·enum 폭을 포함한 `memcpy` 전송은 금지한다. `<stdint.h>`와 공통 Net 타입은 [공통 Data](../../04_COMMON/10_DATA_AND_ID_RULES.md)에서 읽는다.

<a id="r1-window-deferred"></a>
**DEFERRED:** VENT 70/80은 목표 위치 충돌로 미확정이다. 목표 숫자·위치 단위·재개 시 Local Safety를 owner가 확인하기 전 어느 값도 배포 기본값으로 적용하지 않는다. END와 AntiPinch를 구분하고 Command ID의 원 귀속·Fault lineage를 유지하며 로컬 STOP/끼임 보호를 우선한다. WINDOW 기능 구현은 보류하며 실제 보호 동작도 검증 전이다.

## 타입 색인

| 타입 | 목적·owner |
|---|---|
| [NetWindowCommand](#type-netwindowcommand) | DOMAIN 확정 목표·원 명령 문맥 |
| [NetWindowState](#type-netwindowstate) | WINDOW 실제 현재 위치·보호 상태 |
| [NetWindowEvent](#type-netwindowevent) | WINDOW 새 끝단 전이·끼임 발생 |
| [NetWindowResult](#type-netwindowresult) | WINDOW 원 DOMAIN 명령 실행 결과 |
| [NetWindowFault](#type-netwindowfault) | WINDOW 현재·복구 고장·발생 근거 |
| [NetWindowCommandKind](#type-netwindowcommandkind) | 의미 코드·폭·허용 값은 아래 전체 선언 |
| [NetSourceContext](#type-netsourcecontext) | 의미 코드·폭·허용 값은 아래 전체 선언 |
| [NetWindowMotionState](#type-netwindowmotionstate) | 의미 코드·폭·허용 값은 아래 전체 선언 |
| [NetAntiPinchProtectionState](#type-netantipinchprotectionstate) | 의미 코드·폭·허용 값은 아래 전체 선언 |
| [NetWindowEventKind](#type-netwindoweventkind) | 의미 코드·폭·허용 값은 아래 전체 선언 |
| [NetWindowFaultState](#type-netwindowfaultstate) | 의미 코드·폭·허용 값은 아래 전체 선언 |
| [NetWindowFaultCode](#type-netwindowfaultcode) | 의미 코드·폭·허용 값은 아래 전체 선언 |

## 전체 선언·멤버 계약

<a id="type-netwindowcommand"></a>
### `NetWindowCommand`

```c
typedef struct NetWindowCommand
{
    NetCommandMeta command;
    NetWindowCommandKind window_command;
    uint8_t target_channel;
    uint8_t target_closed_percent;
    NetSourceContext source_context;
} NetWindowCommand;
```

| 멤버 | 정확한 C 타입 | 역할·단위·범위 | 원 owner·유효 조건 |
|---|---|---|---|
| `command` | [NetCommandMeta](../../04_COMMON/10_DATA_AND_ID_RULES.md#type-netcommandmeta) | 대상 기동·원 명령 ID·생성 Age·수용 한도 | DOMAIN C2와 Network가 분담. 수신 WINDOW App은 검증된 읽기 전용 문맥 소비 |
| `window_command` | [NetWindowCommandKind](10_DATA.md#type-netwindowcommandkind) | OPEN/CLOSE/STOP/VENT/MOVE_TO_POSITION raw1~5 | DOMAIN 실행 목표 owner; STOP도 같은 명령 형식 |
| `target_channel` | `uint8_t` | 현재 단일 채널 0 | DOMAIN 목표 owner; WINDOW 수신 검증 |
| `target_closed_percent` | `uint8_t` | 닫힘 비율 0~100%; 0 완전열림,100 완전닫힘 | DOMAIN 목표 owner; VENT 70↔구 SysRS80 충돌은 FUNCTION_POLICY_OPEN |
| `source_context` | [NetSourceContext](10_DATA.md#type-netsourcecontext) | 상위 출처 NONE/DOMAIN_POLICY/AUTO_FUNCTION/LOCAL/TEST | DOMAIN 원 문맥; MOBILE 창문 원격제어 추가 의미 아님 |

<a id="type-netwindowstate"></a>
### `NetWindowState`

<a id="r1-window-protection"></a>
**DEFERRED:** `outputDisabled`와 `movementStopped`는 실제 출력 차단·운동 정지의 물리/기능 근거가 확인된 경우에만 true로 투영한다. 명령 접수·통신 성공·추정 시간 경과를 보호 완료로 바꾸지 않는다. 실제 WINDOW 기능 연결과 검증은 보류한다.

```c
typedef struct NetWindowState
{
    NetWindowMotionState motion_state;
    NetControllerState window_ecu_state;
    uint8_t position_closed_percent;
    NetValueQuality window_value_quality;
    NetTriState fully_open_state;
    NetTriState fully_closed_state;
    NetAntiPinchProtectionState antipinch_protection_status;
    NetTriState reverse_state;
    uint32_t update_sequence;
    uint16_t source_age_ms;
} NetWindowState;
```

| 멤버 | 정확한 C 타입 | 역할·단위·범위 | 원 owner·유효 조건 |
|---|---|---|---|
| `motion_state` | [NetWindowMotionState](10_DATA.md#type-netwindowmotionstate) | STOPPED/OPENING/CLOSING/ANTIPINCH_REVERSING/UNKNOWN | WINDOW 실제 동작 owner; Network 접수로 STOPPED 합성 금지 |
| `window_ecu_state` | [NetControllerState](../../04_COMMON/10_DATA_AND_ID_RULES.md#type-netcontrollerstate) | INIT/READY/DEGRADED/FAULT | WINDOW 기능 owner; 다른 노드 상태·CAN ACK로 정상화 금지 |
| `position_closed_percent` | `uint8_t` | 0~100% 닫힘 비율,255 확인 불가 | WINDOW 실제 위치 owner; UNKNOWN을0으로 치환 금지 |
| `window_value_quality` | [NetValueQuality](../../04_COMMON/10_DATA_AND_ID_RULES.md#type-netvaluequality) | WINDOW 상태값의 품질 | WINDOW 실제 평가 owner; 무효·만료를 정상 끝단 값으로 치환 금지 |
| `fully_open_state` | [NetTriState](../../04_COMMON/10_DATA_AND_ID_RULES.md#type-nettristate) | 0 완전열림 아님/1 완전열림/255 확인불가 | WINDOW 실제 끝단 owner; 현재 State이며 새 FULLY_OPENED Event와 별개 |
| `fully_closed_state` | [NetTriState](../../04_COMMON/10_DATA_AND_ID_RULES.md#type-nettristate) | 0 완전닫힘 아님/1 완전닫힘/255 확인불가 | WINDOW 실제 끝단 owner; 확인된 위치와 정합성 검사 |
| `antipinch_protection_status` | [NetAntiPinchProtectionState](10_DATA.md#type-netantipinchprotectionstate) | 0 대기/1 진행/2 완료/3 중단 | WINDOW 로컬 보호 owner; CLEAR/ACTIVE 경고 의미와 별개. 실제 outputDisabled 근거 아님 |
| `reverse_state` | [NetTriState](../../04_COMMON/10_DATA_AND_ID_RULES.md#type-nettristate) | 0 역전아님/1 역전중/255 확인불가 | WINDOW 실제 구동 owner; timeout으로 역전·이동정지 추정 금지 |
| `update_sequence` | `uint32_t` | 실제 새 상태·고장 평가 번호, uint32 | WINDOW 평가 owner; CAN TX_SEQUENCE와 별개, cached 반복 때 유지 |
| `source_age_ms` | `uint16_t` | 원 상태·고장 평가 이후 누적 ms, uint16 포화 | WINDOW 원 평가 + Network 대기만 누적; 재전송으로 초기화 금지 |

<a id="type-netwindowevent"></a>
### `NetWindowEvent`

```c
typedef struct NetWindowEvent
{
    NetWindowEventKind event_kind;
    uint8_t target_channel;
    uint32_t occurrence_id;
    uint32_t related_domain_boot;
    uint32_t related_request_sequence;
    NetOrigin origin;
    uint16_t event_age_ms;
    NetValueQuality event_quality;
} NetWindowEvent;
```

| 멤버 | 정확한 C 타입 | 역할·단위·범위 | 원 owner·유효 조건 |
|---|---|---|---|
| `event_kind` | [NetWindowEventKind](10_DATA.md#type-netwindoweventkind) | 1 FULLY_OPENED/2 FULLY_CLOSED/3 ANTIPINCH | WINDOW 실제 새 전이·끼임 owner; 현재 State 재송신을 Event로 만들지 않음 |
| `target_channel` | `uint8_t` | 현재 채널0 | WINDOW 발생 owner; 원 명령 채널과 대응 |
| `occurrence_id` | `uint32_t` | 새 끝단 전이 또는 끼임 발생 번호 | WINDOW 발생 owner; WINDOW 기동 + occurrence로 식별. 재전송은 동일 발생 |
| `related_domain_boot` | `uint32_t` | 관련 명령의 원 DOMAIN 기동; 연결 없으면0 | 수용한 원 명령 문맥 echo; 로컬 사건에 임의 DOMAIN 문맥 생성 금지 |
| `related_request_sequence` | `uint32_t` | 원 DOMAIN 명령 연결; 연결 없으면0 | 수용 명령 ID echo. C2 commandId와의 정확 대응은 CONTRACT_OPEN |
| `origin` | [NetOrigin](../../04_COMMON/10_DATA_AND_ID_RULES.md#type-netorigin) | 발생·상위 요청 출처 code | WINDOW 실제 발생 owner + 보존한 DOMAIN origin; SOURCE_CONTEXT와 자동 동일시 금지 |
| `event_age_ms` | `uint16_t` | 최초 Event 발생 후 ms, uint16 포화 | WINDOW 원 발생 + Network 대기만 누적 |
| `event_quality` | [NetValueQuality](../../04_COMMON/10_DATA_AND_ID_RULES.md#type-netvaluequality) | Event 원 발생 근거 품질 | WINDOW 발생 owner; 수신 성공을 실제 보호 성공으로 변환 금지 |

<a id="type-netwindowresult"></a>
### `NetWindowResult`

```c
typedef struct NetWindowResult
{
    uint32_t request_domain_boot;
    uint32_t request_sequence;
    uint8_t target_channel;
    NetVehicleResult result;
    NetReason result_reason;
} NetWindowResult;
```

| 멤버 | 정확한 C 타입 | 역할·단위·범위 | 원 owner·유효 조건 |
|---|---|---|---|
| `request_domain_boot` | `uint32_t` | 결과의 원 명령을 만든 DOMAIN 기동 | WINDOW 수용 명령 문맥 echo; 현재 DOMAIN 재기동과 같다고 가정 금지 |
| `request_sequence` | `uint32_t` | 결과의 원 DOMAIN 명령 식별 | WINDOW 명령 echo; C2 commandId 대응은 CONTRACT_OPEN, 새 발급 금지 |
| `target_channel` | `uint8_t` | 원 명령 채널; 현재0 | WINDOW 실행 owner; 원 명령과 일치 |
| `result` | [NetVehicleResult](../../04_COMMON/10_DATA_AND_ID_RULES.md#type-netvehicleresult) | 실제 차량 실행 Result code | WINDOW 실행 owner; local OK/CAN ACK로 DONE 생성 금지 |
| `result_reason` | [NetReason](../../04_COMMON/10_DATA_AND_ID_RULES.md#type-netreason) | 실제 수용·실행·취소·실패 사유 | WINDOW 실행 owner; Result와 함께 해석 |

<a id="type-netwindowfault"></a>
### `NetWindowFault`

```c
typedef struct NetWindowFault
{
    NetFaultCategory fault_category;
    NetWindowFaultCode fault_code;
    NetWindowFaultState fault_status;
    uint32_t occurrence_id;
    uint32_t update_sequence;
    uint16_t source_age_ms;
} NetWindowFault;
```

| 멤버 | 정확한 C 타입 | 역할·단위·범위 | 원 owner·유효 조건 |
|---|---|---|---|
| `fault_category` | [NetFaultCategory](../../04_COMMON/10_DATA_AND_ID_RULES.md#type-netfaultcategory) | 0 없음/1 SENSOR/2 FUNCTION/3 COMM | WINDOW 기능 진단; COMM 원 근거는 WINDOW Network 관측. C2 enum cast 금지 |
| `fault_code` | [NetWindowFaultCode](10_DATA.md#type-netwindowfaultcode) | 0 없음/1 위치/2 구동/3 끝단/4 보호/5 DOMAIN timeout/6 초기화/255기타 후보 | WINDOW 진단 owner; 발생 조건·코드 채택 FUNCTION_POLICY_OPEN |
| `fault_status` | [NetWindowFaultState](10_DATA.md#type-netwindowfaultstate) | 0 현재고장 없음/1 활성/2 복구중 | WINDOW 진단 owner; 없음과 최근 occurrence 보존은 공존 가능 |
| `occurrence_id` | `uint32_t` | 현재 또는 최근 고장 원 발생 번호 | WINDOW 진단 owner; 현재고장 해제와 이력 삭제를 동일시하지 않음 |
| `update_sequence` | `uint32_t` | 실제 새 상태·고장 평가 번호, uint32 | WINDOW 평가 owner; CAN TX_SEQUENCE와 별개, cached 반복 때 유지 |
| `source_age_ms` | `uint16_t` | 원 상태·고장 평가 이후 누적 ms, uint16 포화 | WINDOW 원 평가 + Network 대기만 누적; 재전송으로 초기화 금지 |

<a id="type-netwindowcommandkind"></a>
### `NetWindowCommandKind`

```c
typedef uint8_t NetWindowCommandKind;
enum NetWindowCommandKindCode
{
    NET_WINDOW_COMMAND_OPEN = 1,
    NET_WINDOW_COMMAND_CLOSE = 2,
    NET_WINDOW_COMMAND_STOP = 3,
    NET_WINDOW_COMMAND_VENT = 4,
    NET_WINDOW_COMMAND_MOVE_TO_POSITION = 5
};
```

이 타입은 `uint8_t` 별칭이며 뒤의 `enum`은 코드 상수 집합이다.

| 상수 | 제안 코드값 | 의미 |
|---|---|---|
| `NET_WINDOW_COMMAND_OPEN` | `1` | 완전 열림 목표 |
| `NET_WINDOW_COMMAND_CLOSE` | `2` | 완전 닫힘 목표 |
| `NET_WINDOW_COMMAND_STOP` | `3` | 이동 정지 목표 |
| `NET_WINDOW_COMMAND_VENT` | `4` | 환기 위치 목표;70↔80% 정책 미확정 |
| `NET_WINDOW_COMMAND_MOVE_TO_POSITION` | `5` | 지정 닫힘 비율로 이동 |

<a id="type-netsourcecontext"></a>
### `NetSourceContext`

```c
typedef uint8_t NetSourceContext;
enum NetSourceContextCode
{
    NET_SOURCE_CONTEXT_NONE = 0,
    NET_SOURCE_CONTEXT_DOMAIN_POLICY = 1,
    NET_SOURCE_CONTEXT_AUTO_FUNCTION = 2,
    NET_SOURCE_CONTEXT_LOCAL = 3,
    NET_SOURCE_CONTEXT_TEST = 4
};
```

이 타입은 `uint8_t` 별칭이며 뒤의 `enum`은 코드 상수 집합이다.

| 상수 | 제안 코드값 | 의미 |
|---|---|---|
| `NET_SOURCE_CONTEXT_NONE` | `0` | 없음 |
| `NET_SOURCE_CONTEXT_DOMAIN_POLICY` | `1` | DOMAIN 정책 출처 |
| `NET_SOURCE_CONTEXT_AUTO_FUNCTION` | `2` | 자동 기능 출처 |
| `NET_SOURCE_CONTEXT_LOCAL` | `3` | 로컬 출처 |
| `NET_SOURCE_CONTEXT_TEST` | `4` | 시험 출처 |

<a id="type-netwindowmotionstate"></a>
### `NetWindowMotionState`

```c
typedef uint8_t NetWindowMotionState;
enum NetWindowMotionStateCode
{
    NET_WINDOW_MOTION_STOPPED = 0,
    NET_WINDOW_MOTION_OPENING = 1,
    NET_WINDOW_MOTION_CLOSING = 2,
    NET_WINDOW_MOTION_ANTIPINCH_REVERSING = 3,
    NET_WINDOW_MOTION_UNKNOWN = 255
};
```

이 타입은 `uint8_t` 별칭이며 뒤의 `enum`은 코드 상수 집합이다.

| 상수 | 제안 코드값 | 의미 |
|---|---|---|
| `NET_WINDOW_MOTION_STOPPED` | `0` | 이동 정지 상태 |
| `NET_WINDOW_MOTION_OPENING` | `1` | 열림 이동 중 |
| `NET_WINDOW_MOTION_CLOSING` | `2` | 닫힘 이동 중 |
| `NET_WINDOW_MOTION_ANTIPINCH_REVERSING` | `3` | 끼임 보호 역전 중 |
| `NET_WINDOW_MOTION_UNKNOWN` | `255` | 확인 불가 |

<a id="type-netantipinchprotectionstate"></a>
### `NetAntiPinchProtectionState`

```c
typedef uint8_t NetAntiPinchProtectionState;
enum NetAntiPinchProtectionStateCode
{
    NET_ANTIPINCH_PROTECTION_IDLE = 0,
    NET_ANTIPINCH_PROTECTION_IN_PROGRESS = 1,
    NET_ANTIPINCH_PROTECTION_DONE = 2,
    NET_ANTIPINCH_PROTECTION_INTERRUPTED = 3
};
```

이 타입은 `uint8_t` 별칭이며 뒤의 `enum`은 코드 상수 집합이다.

| 상수 | 제안 코드값 | 의미 |
|---|---|---|
| `NET_ANTIPINCH_PROTECTION_IDLE` | `0` | 보호 대기 |
| `NET_ANTIPINCH_PROTECTION_IN_PROGRESS` | `1` | 보호 진행 |
| `NET_ANTIPINCH_PROTECTION_DONE` | `2` | 보호 완료 |
| `NET_ANTIPINCH_PROTECTION_INTERRUPTED` | `3` | 보호 중단 |

<a id="type-netwindoweventkind"></a>
### `NetWindowEventKind`

```c
typedef uint8_t NetWindowEventKind;
enum NetWindowEventKindCode
{
    NET_WINDOW_EVENT_FULLY_OPENED = 1,
    NET_WINDOW_EVENT_FULLY_CLOSED = 2,
    NET_WINDOW_EVENT_ANTIPINCH = 3
};
```

이 타입은 `uint8_t` 별칭이며 뒤의 `enum`은 코드 상수 집합이다.

| 상수 | 제안 코드값 | 의미 |
|---|---|---|
| `NET_WINDOW_EVENT_FULLY_OPENED` | `1` | 완전 열림으로 새 전이 |
| `NET_WINDOW_EVENT_FULLY_CLOSED` | `2` | 완전 닫힘으로 새 전이 |
| `NET_WINDOW_EVENT_ANTIPINCH` | `3` | 새 끼임 발생 |

<a id="type-netwindowfaultstate"></a>
### `NetWindowFaultState`

```c
typedef uint8_t NetWindowFaultState;
enum NetWindowFaultStateCode
{
    NET_WINDOW_FAULT_STATE_NONE = 0,
    NET_WINDOW_FAULT_STATE_ACTIVE = 1,
    NET_WINDOW_FAULT_STATE_RECOVERING = 2
};
```

이 타입은 `uint8_t` 별칭이며 뒤의 `enum`은 코드 상수 집합이다.

| 상수 | 제안 코드값 | 의미 |
|---|---|---|
| `NET_WINDOW_FAULT_STATE_NONE` | `0` | 없음 |
| `NET_WINDOW_FAULT_STATE_ACTIVE` | `1` | 현재 고장 활성 |
| `NET_WINDOW_FAULT_STATE_RECOVERING` | `2` | 고장 복구 중 |

<a id="type-netwindowfaultcode"></a>
### `NetWindowFaultCode`

```c
typedef uint16_t NetWindowFaultCode;
enum NetWindowFaultCodeCode
{
    NET_WINDOW_FAULT_NONE = 0,
    NET_WINDOW_FAULT_POSITION = 1,
    NET_WINDOW_FAULT_DRIVE = 2,
    NET_WINDOW_FAULT_ENDPOINT = 3,
    NET_WINDOW_FAULT_PROTECTION = 4,
    NET_WINDOW_FAULT_DOMAIN_TIMEOUT = 5,
    NET_WINDOW_FAULT_INITIALIZATION = 6,
    NET_WINDOW_FAULT_OTHER = 255
};
```

이 타입은 `uint16_t` 별칭이며 뒤의 `enum`은 코드 상수 집합이다.

| 상수 | 제안 코드값 | 의미 |
|---|---|---|
| `NET_WINDOW_FAULT_NONE` | `0` | 없음 |
| `NET_WINDOW_FAULT_POSITION` | `1` | 위치 확인 고장 후보 |
| `NET_WINDOW_FAULT_DRIVE` | `2` | 구동 고장 후보 |
| `NET_WINDOW_FAULT_ENDPOINT` | `3` | 끝단 확인 고장 후보 |
| `NET_WINDOW_FAULT_PROTECTION` | `4` | 보호 동작 고장 후보 |
| `NET_WINDOW_FAULT_DOMAIN_TIMEOUT` | `5` | DOMAIN 수신 중단 고장 후보 |
| `NET_WINDOW_FAULT_INITIALIZATION` | `6` | 초기화 고장 후보 |
| `NET_WINDOW_FAULT_OTHER` | `255` | 기타 고장 후보 |

## 명령 공통부와 링크 메타의 소유

`NetWindowCommand.command`의 전체 선언은 [NetCommandMeta](../../04_COMMON/10_DATA_AND_ID_RULES.md#type-netcommandmeta)다. DOMAIN C2 원 `commandId`는 그대로 보존한다. Network가 두 번째 명령 ID를 만들지 않는다.

| 중첩 멤버 | 정확한 C 타입 | 의미·owner |
|---|---|---|
| `command.target_boot_id` | `uint32_t` | DOMAIN Network가 확인한 WINDOW 대상 기동; App의 새 기동 추정값 아님 |
| `command.command_id` | `uint32_t` | DOMAIN C2 원 명령 ID. 같은 명령 재전송에서 불변 |
| `command.command_age_ms` | `uint16_t` | 원 명령 생성 Age + Network 대기/재전송 지연,65535 포화 |
| `command.accept_limit_ms` | `uint16_t` | 수용 한도200ms 후보↔구 SysRS1초 충돌. CONTRACT_OPEN |

TX용 WINDOW report에는 링크 BOOT_ID/TX_SEQUENCE 필드가 없다. Network가 송신마다 TX_SEQUENCE를 증가시키며 App update_sequence/occurrence와 분리한다. RX의 [NetCanRxMeta](../../04_COMMON/10_DATA_AND_ID_RULES.md#type-netcanrxmeta)는 Network가 만든 읽기 전용 문맥이다.

## END·끼임·고장의 다른 근거 — FUNCTION_POLICY_OPEN

| 자료 | 보존한 의미 | 실제 C2 ingress 연결의 남은 조건 |
|---|---|---|
| `WINDOW_EVENT` FULLY_OPENED/FULLY_CLOSED | 새 끝단 전이·채널·occurrence·선택 관련 DOMAIN 기동/명령·origin·Age·품질 | 끝단 전이는 `DomainIf_RxWindowAntiPinch`의 입력이 아님. DOMAIN 내부 named adapter가 원근거를 보존하되 끝단 소비 owner의 concrete C2 경계는 OPEN |
| `WINDOW_EVENT` ANTIPINCH + `WINDOW_STATE` 보호 상태 | 끼임 발생과 현재 보호 진행/완료/중단·reverse를 별개로 유지 | C2 `DomainIf_AntiPinchReport_t`의 `outputDisabled/movementStopped`가 wire에 없음. State/Event/timeout으로 true 추정 금지. 실제 보호 확인 근거·연결 owner OPEN |
| `WINDOW_FAULT` | category/code/current-or-recovering/occurrence/update_sequence/source_age + 수신 WINDOW 기동 | C2 ingress `FaultInfo_t`는 category/state/faultCode/quality뿐. occurrence·sequence·Age·generation 수용 owner, wire에 없는 원 Fault 품질 결정 OPEN |
| `WINDOW_RESULT` | 원 DOMAIN 기동 + 원 명령 식별 + 채널 + 실제 Result/Reason | `REQUEST_SEQUENCE`↔C2 `commandId` 대응 OPEN. wire에 없는 C2 result `meta`의 원 품질·Age·updateSequence를 TX_SEQUENCE에서 합성 금지 |
| `WINDOW_STATE` | 현재 움직임/ECU/위치·품질/끝단/보호·역전/평가 | C2 WindowState의 bool 끝단은 wire UNKNOWN255와 다름. quality/meta 보존 규칙 및 보호 세부 lossless 연결 OPEN |

`DomainIf_WindowFaultState_t`는 DOMAIN MOBILE small projection이며 WINDOW Fault RX 인자가 아니다. 이 projection으로 occurrence를 보여줘도 ingress provenance 보존이 완료된 것은 아니다. MOBILE 창문 원격 제어는 현재 범위에 없고 App 결과·사유 표시 범위는 **[보류]**다. 정확한 기존 선언·타입은 [DOMAIN Data/API](../DOMAIN/00_OVERVIEW.md)에서 읽는다.

## 허용·STOP·결과 수명

DOMAIN_PERMISSION은 [전체7필드](../../04_COMMON/10_DATA_AND_ID_RULES.md#type-netdomainpermission)를 가진 하나의 공유 CAN frame이다. WINDOW 동작허용, DOMAIN link 생존, 전원상태는 다른 의미다. 독립 C2 원평가의 품질·순번·Age 합성과 미확정 범위는 [DOMAIN_PERMISSION 권장안](../../03_COMMUNICATION/10_CAN_FD.md#message-domain-permission)을 따른다. 합성 책임은 OPEN이며 유효하지 않은 허용을 정상 false/true로 축약하지 않는다.

새 DOMAIN 이동은 출처·대상기동·명령ID·Age·채널·형식 및 유효한 동작 허용을 확인한다. 정당한 DOMAIN STOP은 공통 출처·대상·명령·Age·형식 검증을 유지하되 계속 이동을 허용하는 permission 상실만으로 정지 전달을 막지 않는다. 로컬 STOP·끼임 보호는 기존 WINDOW 로컬 보호 경계가 우선이며 Network API 호출을 의무화하지 않는다. permission 복구만으로 과거 이동을 자동 재개하지 않는다.

State/Fault 송신 미시작 pending은 동일 의미 키의 최신 실제 평가로 교체 가능하다. Event/Result는 발생·원 명령·결과 단계 식별을 유지하고 조용히 덮어쓰지 않는다. terminal Result 뒤 늦은 ACCEPTED/IN_PROGRESS로 되돌리지 않으며 충돌하는 최종 결과는 확인 대상으로 남긴다. 명령·Event·Result의 재전송 횟수/시간은 [CAN 후보](../../03_COMMUNICATION/10_CAN_FD.md)를 따른다.

WINDOW가 미구현·미활성인 현재 구성에서 WINDOW timeout만으로 상시 Fault를 만들지 않는다. 기능 재개 전 C2 commandId 대응, 끝단 ingress, 보호 확인, Fault 품질/provenance, VENT70↔80, 로컬 보호 조건·실물 시험을 확정해야 한다. 문서 검토만 수행했으며 WINDOW SDK/Firmware/Codec/Board 시험은 **NOT_EXECUTED**다.
