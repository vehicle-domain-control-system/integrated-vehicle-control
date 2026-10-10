# Input / Time Data — R3

> 2026-10-07 · 고정 Baseline / Execution Plan v1.1 · 논리 필드와 pseudo-C 상세

[Data Overview](00_DATA_OVERVIEW.md) · [R2 타입 판정](00_DATA_OVERVIEW.md#type-decisions)

<a id="input"></a>
INPUT은 검증·출처 적용의 writer이고 STORE는 수용 이력의 writer다. PRODUCT/TEST는 출처·시간·저장·실행·이력을 격리한다. WINDOW / Anti-Pinch는 **[구현 보류 — 설계 유지]**이며 제품 연결 대신 TEST 의미 경로를 유지한다.

시간 자료는 Runtime Boundary가 제공한다. 모든 자료에 timestamp를 붙이는 대신 원본 age·품질 기한·실제 사실 비교에 필요한 곳에서만 사용한다. 아래 `Semantic...` 기본 표기는 [pseudo-C 표기 규칙](00_DATA_OVERVIEW.md#pseudo-conventions)을 따른다.

<a id="vssprocessingopportunity"></a>
## VssProcessingOpportunity — scalar enum
**Category: Request**

처리할 기회만 알리는 scalar다. payload·재생 사실·operation identity를 넣는 Request struct는 만들지 않는다. 여러 알림이 합쳐져도 owner의 보호된 사실은 별도로 읽는다.

**Owner:** Runtime Boundary의 알림 의미 / FLOW의 조율 · **Producer:** 기존 실행 진입·Runtime 알림 · **Consumer:** FLOW

| 값 | 의미 / 유효 조건 |
| --- | --- |
| `VSS_OPPORTUNITY_INITIAL` | 초기 준비/미평가 진행 기회 |
| `VSS_OPPORTUNITY_INPUT_OR_FOLLOW_UP` | 입력 또는 owner 후속 처리 기회. 실제 입력은 VssInputEvidence로 받는다. |
| `VSS_OPPORTUNITY_TIME_CHECK` | RX 없이 시간/품질/정리 진행 기회 |

**Lifetime / Mutability:** 호출 동안 유효하며 수락·실제 출력 여부의 증거로 보존하지 않는다. 다음 호출의 의미 값으로 교체 가능하다.

**관련 Function:** [Flow_Process](../30_FUNCTIONS/10_FLOW_FUNCTIONS.md#flow-process)

**관련 Contract:** [시간·최신성](../50_CONTRACTS/00_CONTRACT_OVERVIEW.md#timing)

**Provisional owner header 후보:** Runtime 공통 시간·알림 인터페이스 후보 — R6에서 확정.

```c
typedef enum
{
    VSS_OPPORTUNITY_INITIAL,
    VSS_OPPORTUNITY_INPUT_OR_FOLLOW_UP,
    VSS_OPPORTUNITY_TIME_CHECK
} VssProcessingOpportunity;
```

<a id="vssflowprogress"></a>
## VssFlowProgress — scalar enum
**Category: Result**

FLOW 반환은 다음 처리 필요성만 표현하는 enum이다. 보고 전달 상태·조율 단계는 기존 FLOW 내부 상태에 남기며 다른 owner 상태를 복제하지 않는다.

**Owner:** FLOW · **Producer:** Flow_Process · **Consumer:** 기존 실행 진입

| 값 | 의미 / 유효 조건 |
| --- | --- |
| `VSS_FLOW_SETTLED` | 이번 기회에서 필요한 반영/연결을 완료했다. 재생 성공이라는 뜻은 아니다. |
| `VSS_FLOW_FOLLOW_UP_REQUIRED` | 결과·정리·보고 전달·재평가 중 남은 처리가 있다. |
| `VSS_FLOW_RECOLLECT_OBSERVATIONS` | 현재 판단 묶음이 함께 적용 가능하지 않아 새 관측이 필요하다. |

**Lifetime / Mutability:** 호출 결과의 scalar. 여러 미완료 원인이 동시에 존재해도 FOLLOW_UP_REQUIRED를 반환하고 상세 원인은 FLOW/각 owner에 남긴다.

**관련 Function:** [Flow_Process](../30_FUNCTIONS/10_FLOW_FUNCTIONS.md#flow-process)

**관련 Contract:** [소유·수명](../50_CONTRACTS/00_CONTRACT_OVERVIEW.md#ownership) · [시간·최신성](../50_CONTRACTS/00_CONTRACT_OVERVIEW.md#timing)

**Provisional owner header 후보:** FLOW 진행 인터페이스 후보 — R6에서 확정.

```c
typedef enum
{
    VSS_FLOW_SETTLED,
    VSS_FLOW_FOLLOW_UP_REQUIRED,
    VSS_FLOW_RECOLLECT_OBSERVATIONS
} VssFlowProgress;
```

<a id="vsscontinuity"></a>
## VssContinuity — scalar enum
**Category: Evidence**

시간·출처 비교 가능성을 표현한다. reset/wrap을 새 시간이나 정상 성공으로 보정하지 않는다.

**Owner:** Runtime/INPUT의 해당 근거 · **Producer:** Runtime 또는 INPUT 검증 · **Consumer:** 관련 시간·출처 소비자

| 값 | 의미 / 유효 조건 |
| --- | --- |
| `VSS_CONTINUITY_COMPARABLE` | 해당 시간 문맥에서 비교 가능한 근거가 있다. |
| `VSS_CONTINUITY_UNKNOWN` | 비교 근거가 아직 부족하다. |
| `VSS_CONTINUITY_LOST` | 연속성 상실이 확인됐다. 이전 기한을 새로 시작하지 않는다. |

**Lifetime / Mutability:** 근거가 적용되는 범위 동안 불변. 새 검증은 별도 근거이며 기존 사실의 시각을 바꾸지 않는다.

**관련 Function:** [Input_Process](../30_FUNCTIONS/20_INPUT_STORE_FUNCTIONS.md#input-process) · [Store_AdvanceDeadlines](../30_FUNCTIONS/20_INPUT_STORE_FUNCTIONS.md#store-advancedeadlines) · [Playback_Advance](../30_FUNCTIONS/30_POLICY_PLAYBACK_FUNCTIONS.md#playback-advance)

**관련 Contract:** [시간·최신성](../50_CONTRACTS/00_CONTRACT_OVERVIEW.md#timing)

**Provisional owner header 후보:** Runtime 시간 근거 후보 — R6에서 확정.

```c
typedef enum
{
    VSS_CONTINUITY_COMPARABLE,
    VSS_CONTINUITY_UNKNOWN,
    VSS_CONTINUITY_LOST
} VssContinuity;
```

<a id="vsstimeevidence"></a>
## VssTimeEvidence
**Category: Evidence**

**타입 종류:** 구조체 (`struct`) — 이 절의 Pseudo-C에 논리 선언이 있다.

현재 비교에 사용할 시간 문맥과 정확도·연속성을 전달한다. 처리 현재 시각은 원본 발생/출력 사실 시각을 대체하지 못한다.

**Owner:** Runtime Boundary  
**Producer:** 기존 Runtime 시간 도구  
**Consumer:** FLOW/INPUT/STORE/PLAYBACK/AUDIO STREAM/DRIVER/HEALTH

### 사람이 읽는 Field 정의

| Field | 의미 / 필요한 이유 | Field 생성 주체 | 유효 조건 |
| --- | --- | --- | --- |
| `domain` | PRODUCT/TEST와 원본 비교 문맥을 식별한다. 다른 문맥의 시각을 빼지 않기 위해 필요하다. | Runtime | 동일 문맥의 원본·기한 비교에만 유효 |
| `epoch` | wrap/reset·연속성 범위의 구분 근거다. 실제 처리 방법은 TBD다. | Runtime | 동일 epoch 또는 입증된 변환 범위 |
| `earliestNow` | 현재 시각의 가능한 하한. 정밀 포착 불가를 숨기지 않는다. | Runtime | epoch/continuity와 함께 비교 |
| `latestNow` | 현재 시각의 가능한 상한. 정확하면 하한과 같다. | Runtime | earliestNow 이후의 유효 범위 |
| `continuity` | 비교 가능/부족/상실을 구별한다. | Runtime | 해당 시간 읽기 범위 |

### Lifetime / Mutability / 폐기

생성은 각 시간 읽기 시점이다. 전달 뒤 불변이며 호출·관측 평가 기간에만 사용한다. 사실에 저장할 시각 근거는 그 사실의 원래 문맥으로 복사·보호하고 임시 인자 포인터를 보관하지 않는다. 연속성이 바뀐 평가에는 기존 읽기 값을 재사용하지 않는다.

**관련 Function:** [Flow_Process](../30_FUNCTIONS/10_FLOW_FUNCTIONS.md#flow-process) · [Input_Process](../30_FUNCTIONS/20_INPUT_STORE_FUNCTIONS.md#input-process) · [Store_AdvanceDeadlines](../30_FUNCTIONS/20_INPUT_STORE_FUNCTIONS.md#store-advancedeadlines) · [Playback_Advance](../30_FUNCTIONS/30_POLICY_PLAYBACK_FUNCTIONS.md#playback-advance) · [AudioStream_Advance](../30_FUNCTIONS/40_AUDIO_STREAM_ASSET_FUNCTIONS.md#audiostream-advance) · [AudioTx_Advance](../30_FUNCTIONS/50_DRIVER_HAL_FUNCTIONS.md#audiotx-advance) · [AudioControl_Service](../30_FUNCTIONS/50_DRIVER_HAL_FUNCTIONS.md#audiocontrol-service)

**관련 Contract:** [소유·수명](../50_CONTRACTS/00_CONTRACT_OVERVIEW.md#ownership) · [시간·최신성](../50_CONTRACTS/00_CONTRACT_OVERVIEW.md#timing)

**논리 Header 소유권(R6):** [H-TIME](../70_C_INTERFACE/00_HEADER_OWNERSHIP_MAP.md#header-candidates) · [타입 대응](../70_C_INTERFACE/00_HEADER_OWNERSHIP_MAP.md#type-map). 실제 `.h` 파일명·C ABI는 미정이며, 해당 선언의 C Header 구현은 확인하지 않았다.

### Pseudo-C

```c
typedef struct
{
    VssTimeDomainType domain;
    VssEpochType epoch;
    SemanticTimePoint earliestNow;
    SemanticTimePoint latestNow;
    VssContinuity continuity;
} VssTimeEvidence;
```

**필드 타입과 정의 위치**

| Field | 선언 Type | 종류 / 별칭 관계 | 정의 위치 |
| --- | --- | --- | --- |
| `domain` | `VssTimeDomainType` | 의미 alias → `SemanticIdentity` | [alias 선언](#vsstimedomaintype) |
| `epoch` | `VssEpochType` | 의미 alias → `SemanticIdentity` | [alias 선언](#vssepochtype) |
| `earliestNow` | `SemanticTimePoint` | 시간 pseudo-scalar | [표기 규칙](00_DATA_OVERVIEW.md#pseudo-conventions) |
| `latestNow` | `SemanticTimePoint` | 시간 pseudo-scalar | [표기 규칙](00_DATA_OVERVIEW.md#pseudo-conventions) |
| `continuity` | `VssContinuity` | enum | [enum 선언](#vsscontinuity) |



<a id="vssinputquality"></a>
## VssInputQuality — scalar enum
**Category: State**

현재 입력 품질과 정상 의미 값을 분리한다. INVALID/STALE/미수신은 유효 CLEAR가 아니다.

**Owner:** INPUT 검증 / STORE 현재 품질 · **Producer:** INPUT · **Consumer:** STORE/HEALTH 읽기 소비자

| 값 | 의미 / 유효 조건 |
| --- | --- |
| `VSS_INPUT_VALID` | 원본 시간·출처·값을 현재 적용할 수 있음 |
| `VSS_INPUT_INVALID` | 최신 적용 가능한 부적합/품질 상실 |
| `VSS_INPUT_STALE` | 원본 유효 기한 상실 |
| `VSS_INPUT_UNASSESSED` | 아직 유효 판단이 없음 |

**Lifetime / Mutability:** INPUT 적용 후 STORE가 자신의 현재 품질에 반영한다. 과거 INVALID가 현재 품질을 덮지 않는다.

**관련 Function:** [Input_Process](../30_FUNCTIONS/20_INPUT_STORE_FUNCTIONS.md#input-process) · [Store_ApplyInput](../30_FUNCTIONS/20_INPUT_STORE_FUNCTIONS.md#store-applyinput)

**관련 Contract:** [시간·최신성](../50_CONTRACTS/00_CONTRACT_OVERVIEW.md#timing)

**Provisional owner header 후보:** INPUT 검증 의미 후보 — R6에서 확정.

```c
typedef enum
{
    VSS_INPUT_VALID,
    VSS_INPUT_INVALID,
    VSS_INPUT_STALE,
    VSS_INPUT_UNASSESSED
} VssInputQuality;
```

<a id="vssinputsignal"></a>
## VssInputSignal — scalar enum
**Category: Request**

프로젝트의 제한된 VSS 의미 식별이다. CAN ID/payload 또는 범용 EventBus 식별자가 아니다.

**Owner:** INPUT의 의미 변환 · **Producer:** 제품/TEST 변환 · **Consumer:** STORE

| 값 | 의미 / 유효 조건 |
| --- | --- |
| `VSS_SIGNAL_WELCOME` | 접근 전이 사건 |
| `VSS_SIGNAL_GOODBYE` | 이탈 전이 사건 |
| `VSS_SIGNAL_LOCK_COMPLETE` | 새 LOCK 요청의 목표 확인 |
| `VSS_SIGNAL_UNLOCK_COMPLETE` | 새 UNLOCK 요청의 목표 확인 |
| `VSS_SIGNAL_LOCK_ERROR` | 실제 LOCK 뒤 목표 확인 실패 |
| `VSS_SIGNAL_ANTI_PINCH` | 보류된 WINDOW 경고 의미 / TEST 유지 |
| `VSS_SIGNAL_OCCUPANT_HAZARD` | 잔류 탑승자 경고 |
| `VSS_SIGNAL_REAR_RISK` | Rear 위험 판단 |
| `VSS_SIGNAL_REAR_ACTIVATION` | Rear 활성화 상태. 단독 음향 후보 아님 |

**Lifetime / Mutability:** 해당 검증 입력의 수명. enum 숫자는 Network bit/ID가 아니다.

**관련 Function:** [Input_Process](../30_FUNCTIONS/20_INPUT_STORE_FUNCTIONS.md#input-process) · [Store_ApplyInput](../30_FUNCTIONS/20_INPUT_STORE_FUNCTIONS.md#store-applyinput)

**관련 Contract:** [소유·수명](../50_CONTRACTS/00_CONTRACT_OVERVIEW.md#ownership)

**Provisional owner header 후보:** INPUT 의미 인터페이스 후보 — R6에서 확정.

```c
typedef enum
{
    VSS_SIGNAL_WELCOME,
    VSS_SIGNAL_GOODBYE,
    VSS_SIGNAL_LOCK_COMPLETE,
    VSS_SIGNAL_UNLOCK_COMPLETE,
    VSS_SIGNAL_LOCK_ERROR,
    VSS_SIGNAL_ANTI_PINCH,
    VSS_SIGNAL_OCCUPANT_HAZARD,
    VSS_SIGNAL_REAR_RISK,
    VSS_SIGNAL_REAR_ACTIVATION
} VssInputSignal;
```

<a id="vssinputvalue"></a>
## VssInputValue — scalar enum
**Category: Request**

signal과 함께 읽는 작은 의미 값이다. EVENT는 One-shot 전이 사실만, ACTIVE/CLEAR는 유효 Stateful 판단만 사용한다.

**Owner:** INPUT 의미 변환 · **Producer:** 제품/TEST 변환 · **Consumer:** INPUT/STORE

| 값 | 의미 / 유효 조건 |
| --- | --- |
| `VSS_VALUE_EVENT` | One-shot의 실제 의미 사건 |
| `VSS_VALUE_ACTIVE` | Anti-Pinch/Occupant 유효 경고 |
| `VSS_VALUE_CLEAR` | 해당 경고/Rear 위험의 유효 해제 판단 |
| `VSS_VALUE_EMERGENCY` | Rear Emergency |
| `VSS_VALUE_CAUTION` | Rear Caution |
| `VSS_VALUE_ENABLED` | Rear Activation enabled |
| `VSS_VALUE_DISABLED` | Rear Activation disabled. CLEAR와 구별 |

**Lifetime / Mutability:** signal/품질과 함께만 유효하다. 부적합한 조합은 INPUT이 거부한다.

**관련 Function:** [Input_Process](../30_FUNCTIONS/20_INPUT_STORE_FUNCTIONS.md#input-process) · [Store_ApplyInput](../30_FUNCTIONS/20_INPUT_STORE_FUNCTIONS.md#store-applyinput)

**관련 Contract:** [시간·최신성](../50_CONTRACTS/00_CONTRACT_OVERVIEW.md#timing)

**Provisional owner header 후보:** INPUT 의미 인터페이스 후보 — R6에서 확정.

```c
typedef enum
{
    VSS_VALUE_EVENT,
    VSS_VALUE_ACTIVE,
    VSS_VALUE_CLEAR,
    VSS_VALUE_EMERGENCY,
    VSS_VALUE_CAUTION,
    VSS_VALUE_ENABLED,
    VSS_VALUE_DISABLED
} VssInputValue;
```

<a id="vssinputmeta"></a>
## VssInputMeta
**Category: Evidence**

**타입 종류:** 구조체 (`struct`) — 이 절의 Pseudo-C에 논리 선언이 있다.

값과 함께 검증·보존해야 할 원래 출처·순서·시간 근거다. Network Meta 전달 방식과 identity 생성 규칙은 Binding에서 확인한다.

**Owner:** INPUT의 검증된 의미 Meta. 수신 원본 생성 책임은 기존 Node Communication/TEST다.  
**Producer:** Node Communication/TEST 원본 → INPUT 검증  
**Consumer:** INPUT/STORE

### 사람이 읽는 Field 정의

| Field | 의미 / 필요한 이유 | Field 생성 주체 | 유효 조건 |
| --- | --- | --- | --- |
| `origin` | PRODUCT 또는 TEST 격리 문맥. 실행·이력까지 유지한다. | 기존 입력 경계 | 검증 및 파생 입력 수명 전체 |
| `source` | 원래 의미를 제공한 출처. 현재 수신자와 다르다. | 기존 입력 경계 | 검증된 authority와 함께 |
| `generation` | 출처의 세대 관계. 재부팅/옛 값 격리에 필요하다. | 기존 입력 경계 | INPUT이 검증한 적용 범위 |
| `ordering` | 원본 순서 근거. 도착 순서로 바꾸지 않는다. | 기존 입력 경계 | 동일 출처/세대의 비교 |
| `timeDomain` | 원본 시간의 비교 문맥. | 기존 입력 경계/INPUT 검증 | 시간 비교가 입증된 범위 |
| `timeEpoch` | 원본 시간 연속성 범위. | 기존 입력 경계/INPUT 검증 | 해당 원본 시간 범위 |
| `originalAt` | 원래 발생/판단 시각. 송신·중계·대기 비용을 age에 남긴다. | 원래 의미 producer | 시간 근거 검증 후. 확인 불가 시 정상 사건으로 확정하지 않음 |
| `validUntil` | 원본 판단의 유효기한. Hold 기산 비교에 필요하다. 정의된 기한이 없는 입력에는 없음. | 원본 의미/기존 정책 | 유효기한이 정의된 입력. 미정값은 정책 TBD |
| `continuity` | 원본 비교/연속성 근거. | INPUT 검증 | 해당 적용 문맥 |

### Lifetime / Mutability / 폐기

수신 자료 검증 뒤 의미 Meta로 보호한다. 전달 뒤 불변이며 STORE가 원본 age/마지막 유효 판단에 필요한 값만 보존한다. 수신 포인터가 끝나도 accepted 발생의 identity·원본 시간은 사라지지 않는다. 검증할 수 없는 시간 값을 현재 시각으로 채우지 않는다.

**관련 Function:** [Input_Process](../30_FUNCTIONS/20_INPUT_STORE_FUNCTIONS.md#input-process) · [Store_ApplyInput](../30_FUNCTIONS/20_INPUT_STORE_FUNCTIONS.md#store-applyinput)

**관련 Contract:** [시간·최신성](../50_CONTRACTS/00_CONTRACT_OVERVIEW.md#timing) · [소유·수명](../50_CONTRACTS/00_CONTRACT_OVERVIEW.md#ownership)

**논리 Header 소유권(R6):** [H-INPUT](../70_C_INTERFACE/00_HEADER_OWNERSHIP_MAP.md#header-candidates) · [타입 대응](../70_C_INTERFACE/00_HEADER_OWNERSHIP_MAP.md#type-map). 실제 `.h` 파일명·C ABI는 미정이며, 해당 선언의 C Header 구현은 확인하지 않았다.

### Pseudo-C

```c
typedef struct
{
    VssOriginType origin;
    VssSourceKeyType source;
    VssGenerationType generation;
    VssOrderingType ordering;
    VssTimeDomainType timeDomain;
    VssEpochType timeEpoch;
    SemanticTimePoint originalAt;
    const SemanticTimePoint * validUntil;
    VssContinuity continuity;
} VssInputMeta;
```

**필드 타입과 정의 위치**

| Field | 선언 Type | 종류 / 별칭 관계 | 정의 위치 |
| --- | --- | --- | --- |
| `origin` | `VssOriginType` | 의미 alias → `SemanticOrigin` | [alias 선언](#vssorigintype) |
| `source` | `VssSourceKeyType` | 의미 alias → `SemanticIdentity` | [alias 선언](#vsssourcekeytype) |
| `generation` | `VssGenerationType` | 의미 alias → `SemanticOrder` | [alias 선언](#vssgenerationtype) |
| `ordering` | `VssOrderingType` | 의미 alias → `SemanticOrder` | [alias 선언](#vssorderingtype) |
| `timeDomain` | `VssTimeDomainType` | 의미 alias → `SemanticIdentity` | [alias 선언](#vsstimedomaintype) |
| `timeEpoch` | `VssEpochType` | 의미 alias → `SemanticIdentity` | [alias 선언](#vssepochtype) |
| `originalAt` | `SemanticTimePoint` | 시간 pseudo-scalar | [표기 규칙](00_DATA_OVERVIEW.md#pseudo-conventions) |
| `validUntil` | `const SemanticTimePoint *` | 시간 pseudo-scalar의 읽기 참조 | [표기 규칙](00_DATA_OVERVIEW.md#pseudo-conventions) |
| `continuity` | `VssContinuity` | enum | [enum 선언](#vsscontinuity) |

`validUntil`는 정의된 기한이 없는 입력에서 '없음'을 허용한다. `const ... *`는 읽기 참조를 표현하며, 호출이 끝난 임시 주소의 장기 저장을 허용하지 않는다. 후속 처리에 필요한 값·내부 참조는 [기존 소유·수명 계약](../50_CONTRACTS/10_OWNERSHIP_LIFETIME.md#borrow-and-protect)에 따라 보호한다. 실제 C optional/복사/참조 표현과 nullable ABI는 미정이다.



<a id="vssinputevidence"></a>
## VssInputEvidence
**Category: Request**

기존 입력을 INPUT 의미 경계에 전달한다. RX 없는 점검은 이 인자 없음과 VssTimeEvidence로 표현하며 별도 empty Request struct를 만들지 않는다.

**Owner:** INPUT 경계의 해석 의미. 원본 자료 writer는 기존 통신/TEST다.  
**Producer:** 기존 Node Communication 또는 격리 TEST  
**Consumer:** Input_Process

### 사람이 읽는 Field 정의

| Field | 의미 / 필요한 이유 | Field 생성 주체 | 유효 조건 |
| --- | --- | --- | --- |
| `signal` | 어떤 VSS 의미인지 제한한다. | 기존 제품/TEST 변환 | 검증 대상. 허용 조합은 INPUT 판단 |
| `value` | 원래 사건/상태 값이다. 품질 상실을 CLEAR 값으로 바꾸지 않는다. | 원본 의미 producer | VALID이고 signal과 허용 조합일 때 정상 의미로 적용 |
| `quality` | 원본 입력의 품질 근거. 검증 결과와 동일시하지 않는다. | 기존 통신/TEST | 최신 적용 가능 여부는 INPUT 검증 |
| `meta` | 값과 동행하는 원본 Meta다. | 기존 통신/TEST → INPUT 검증 | Input_Process 검증/전달 동안 |

### Lifetime / Mutability / 폐기

호출 동안 원본을 읽기 전용으로 빌린다. INPUT이 수락해 출처 문맥에 남긴 값은 자신의 보호 자료로 보존한다. 호출이 끝난 임시 구조체 주소를 STORE가 장기 참조하지 않는다. 품질 부적합 또는 과거 입력은 실패 경로이며 현재 품질을 무조건 덮지 않는다.

**관련 Function:** [Input_Process](../30_FUNCTIONS/20_INPUT_STORE_FUNCTIONS.md#input-process)

**관련 Contract:** [시간·최신성](../50_CONTRACTS/00_CONTRACT_OVERVIEW.md#timing) · [소유·수명](../50_CONTRACTS/00_CONTRACT_OVERVIEW.md#ownership)

**Provisional owner header 후보:** INPUT 수신 의미 인터페이스 후보 — R6에서 확정. 실제 파일명·신규 Header 필요 여부는 정하지 않는다.

### Pseudo-C

```c
typedef struct
{
    VssInputSignal signal;
    VssInputValue value;
    VssInputQuality quality;
    VssInputMeta meta;
} VssInputEvidence;
```



<a id="vssinputcontext"></a>
## VssInputContext
**Category: Context**

검증된 출처·세대·순서와 최신 품질 적용 문맥이다. STORE 신규 수용 거부와 무관하게 검증된 적용을 유지해야 하므로 수신 Request와 분리한다.

**Owner:** INPUT 단일 writer  
**Producer:** Input_Process  
**Consumer:** Input_Process / FLOW·HEALTH의 읽기 관측

### 사람이 읽는 Field 정의

| Field | 의미 / 필요한 이유 | Field 생성 주체 | 유효 조건 |
| --- | --- | --- | --- |
| `origin` | PRODUCT/TEST 문맥을 분리한다. | INPUT | 문맥 생성 뒤 |
| `signal` | 적용 문맥의 의미 대상. | INPUT | 해당 문맥 수명 |
| `appliedMeta` | 검증해 적용한 출처/세대/순서. 부적합 정상 사건은 최신 유효 판단으로 만들지 않는다. | INPUT | 적용 가능한 변화 반영 후 |
| `quality` | 현재 적용된 품질. 마지막 유효 상태 값은 STORE 책임이다. | INPUT | 최신 적용 가능한 품질 반영 후 |
| `revision` | 반영 완료/수집 중 변화 확인에 필요한 owner revision. | INPUT | 문맥 변경이 실제 반영된 뒤 |

### Lifetime / Mutability / 폐기

해당 출처/세대 적용·품질 판단에 필요한 동안 유지한다. INPUT만 변경하며 읽기 관측은 수집 기간에만 유효하다. STORE 거부로 rollback하지 않는다. 문맥 회수·재부팅 정책은 identity/연속성 근거가 필요하며 원본 age를 새로 시작하지 않는다.

**관련 Function:** [Input_Process](../30_FUNCTIONS/20_INPUT_STORE_FUNCTIONS.md#input-process) · [Flow_Process](../30_FUNCTIONS/10_FLOW_FUNCTIONS.md#flow-process)

**관련 Contract:** [소유·수명](../50_CONTRACTS/00_CONTRACT_OVERVIEW.md#ownership) · [시간·최신성](../50_CONTRACTS/00_CONTRACT_OVERVIEW.md#timing)

**Provisional owner header 후보:** INPUT 내부 적용 문맥 후보 — R6에서 확정. 실제 파일명·신규 Header 필요 여부는 정하지 않는다.

### Pseudo-C

```c
typedef struct
{
    VssOriginType origin;
    VssInputSignal signal;
    VssInputMeta appliedMeta;
    VssInputQuality quality;
    VssRevisionType revision;
} VssInputContext;
```



<a id="vssoneshotevent"></a>
## VssOneShotEvent
**Category: Event**

INPUT이 검증한 실제 One-shot 발생이다. Stateful 값이나 현재 LOCK 상태를 occurrence로 승격하지 않는다.

**Owner:** INPUT의 검증 사건. 수용 이후 occurrence 원본은 STORE다.  
**Producer:** Input_Process  
**Consumer:** Store_ApplyInput

### 사람이 읽는 Field 정의

| Field | 의미 / 필요한 이유 | Field 생성 주체 | 유효 조건 |
| --- | --- | --- | --- |
| `occurrence` | 동일 발생을 재전달/replay에서도 식별한다. 수신 시마다 새 ID로 만들지 않는다. | 원본 의미 identity → INPUT 검증 | 검증 뒤; 실제 생성/재부팅 규칙 TBD |
| `signal` | Welcome/Goodbye/Lock Complete/Unlock Complete/Lock Error 중 의미. | INPUT | EVENT 조합 검증 후 |
| `meta` | 원본 발생 순서·시간과 격리 근거. | INPUT 검증 | STORE 수용/전달 동안. 수용 시 필요한 근거를 보존 |

### Lifetime / Mutability / 폐기

검증 완료부터 STORE 동기 적용까지 불변으로 전달한다. accepted이면 STORE가 occurrence/Meta를 보호한다. rejected이면 INPUT 적용 순서는 유지한다. Event 전달 자료 폐기와 ledger 회수는 별개다.

**관련 Function:** [Input_Process](../30_FUNCTIONS/20_INPUT_STORE_FUNCTIONS.md#input-process) · [Store_ApplyInput](../30_FUNCTIONS/20_INPUT_STORE_FUNCTIONS.md#store-applyinput)

**관련 Contract:** [소유·수명](../50_CONTRACTS/00_CONTRACT_OVERVIEW.md#ownership) · [시간·최신성](../50_CONTRACTS/00_CONTRACT_OVERVIEW.md#timing)

**Provisional owner header 후보:** INPUT 검증 사건 인터페이스 후보 — R6에서 확정. 실제 파일명·신규 Header 필요 여부는 정하지 않는다.

### Pseudo-C

```c
typedef struct
{
    VssOccurrenceKeyType occurrence;
    VssInputSignal signal;
    VssInputMeta meta;
} VssOneShotEvent;
```



<a id="vssstateupdate"></a>
## VssStateUpdate
**Category: Request**

유효 Stateful 또는 Rear Activation 변화의 적용 요구다. ACTIVE/CLEAR/DISABLED를 구별하며 occurrence 이력 필드를 넣지 않는다.

**Owner:** INPUT 검증 의미 / STORE 적용 상태  
**Producer:** Input_Process  
**Consumer:** Store_ApplyInput

### 사람이 읽는 Field 정의

| Field | 의미 / 필요한 이유 | Field 생성 주체 | 유효 조건 |
| --- | --- | --- | --- |
| `signal` | Anti-Pinch/Occupant/Rear Risk/Rear Activation 대상. | INPUT | 유효 검증 뒤 |
| `value` | 해당 대상의 ACTIVE/CLEAR/EMERGENCY/CAUTION/ENABLED/DISABLED. | INPUT | VALID인 허용 조합에 한함 |
| `meta` | 현재 적용 가능한 순서·원본 유효기한. | INPUT | 검증된 적용 기간 |

### Lifetime / Mutability / 폐기

검증 뒤 전달까지 불변. STORE가 마지막 유효 판단/품질/Hold·Rear 문맥에 필요한 값만 보존한다. 같은 ACTIVE 전달이 새 Session을 만들지 않는다.

**관련 Function:** [Input_Process](../30_FUNCTIONS/20_INPUT_STORE_FUNCTIONS.md#input-process) · [Store_ApplyInput](../30_FUNCTIONS/20_INPUT_STORE_FUNCTIONS.md#store-applyinput)

**관련 Contract:** [시간·최신성](../50_CONTRACTS/00_CONTRACT_OVERVIEW.md#timing) · [소유·수명](../50_CONTRACTS/00_CONTRACT_OVERVIEW.md#ownership)

**Provisional owner header 후보:** INPUT 검증 상태 인터페이스 후보 — R6에서 확정. 실제 파일명·신규 Header 필요 여부는 정하지 않는다.

### Pseudo-C

```c
typedef struct
{
    VssInputSignal signal;
    VssInputValue value;
    VssInputMeta meta;
} VssStateUpdate;
```



<a id="vssqualitychange"></a>
## VssQualityChange
**Category: Evidence**

현재 적용 가능한 신뢰성 상실을 정상 사건/CLEAR와 분리한다. RX 없는 점검에서도 기존 INPUT 문맥으로 생성한다.

**Owner:** INPUT의 품질 검증  
**Producer:** Input_Process  
**Consumer:** Store_ApplyInput / FLOW·HEALTH 읽기

### 사람이 읽는 Field 정의

| Field | 의미 / 필요한 이유 | Field 생성 주체 | 유효 조건 |
| --- | --- | --- | --- |
| `signal` | 품질 변화의 원래 대상. | INPUT | 기존 적용 문맥 확인 뒤 |
| `quality` | INVALID/STALE 등 현재 품질 변화. | INPUT | 최신 출처/순서의 근거만 적용 |
| `basis` | 변화가 연결된 원본 판단·기한/세대. 새 수신 Meta가 아니다. | INPUT | 같은 문맥의 기존 근거 |
| `lossAt` | 최초 신뢰성 상실을 판단할 근거 시각. Hold를 반복 호출로 연장하지 않기 위해 필요하다. | INPUT/Runtime 근거 | 비교 가능할 때. 불가하면 continuity 부족 유지 |

### Lifetime / Mutability / 폐기

생성 뒤 불변 전달. STORE는 최초 상실과 원본 validUntil 중 더 이른 Hold 기산을 보존한다. 반복 상실은 새 기준으로 덮지 않는다. 시간 근거 부족을 정밀 시각으로 합성하지 않는다.

**관련 Function:** [Input_Process](../30_FUNCTIONS/20_INPUT_STORE_FUNCTIONS.md#input-process) · [Store_ApplyInput](../30_FUNCTIONS/20_INPUT_STORE_FUNCTIONS.md#store-applyinput)

**관련 Contract:** [시간·최신성](../50_CONTRACTS/00_CONTRACT_OVERVIEW.md#timing)

**Provisional owner header 후보:** INPUT 품질 근거 인터페이스 후보 — R6에서 확정. 실제 파일명·신규 Header 필요 여부는 정하지 않는다.

### Pseudo-C

```c
typedef struct
{
    VssInputSignal signal;
    VssInputQuality quality;
    VssInputMeta basis;
    SemanticTimePoint lossAt;
} VssQualityChange;
```



<a id="vssinputvalidation"></a>
## VssInputValidation — scalar enum
**Category: Result**

입력 검증과 STORE의 개별 수용을 분리하는 scalar다.

**Owner:** INPUT · **Producer:** Input_Process · **Consumer:** FLOW

| 값 | 의미 / 유효 조건 |
| --- | --- |
| `VSS_VALIDATION_APPLICABLE` | 현재 적용 가능한 정상 사건/상태 |
| `VSS_VALIDATION_QUALITY_CHANGE` | 현재 적용 가능한 품질 변화 |
| `VSS_VALIDATION_REJECTED` | 부적합/과거 자료로 정상 적용하지 않음 |
| `VSS_VALIDATION_NO_CHANGE` | RX 없는 점검 또는 중복 점검에서 적용 변화 없음 |

**Lifetime / Mutability:** 해당 호출 동안의 결과. 현재 문맥의 상태 원본은 아니다.

**관련 Function:** [Input_Process](../30_FUNCTIONS/20_INPUT_STORE_FUNCTIONS.md#input-process)

**관련 Contract:** [시간·최신성](../50_CONTRACTS/00_CONTRACT_OVERVIEW.md#timing)

**Provisional owner header 후보:** INPUT 검증 반환 후보 — R6에서 확정.

```c
typedef enum
{
    VSS_VALIDATION_APPLICABLE,
    VSS_VALIDATION_QUALITY_CHANGE,
    VSS_VALIDATION_REJECTED,
    VSS_VALIDATION_NO_CHANGE
} VssInputValidation;
```

<a id="vssinputapplication"></a>
## VssInputApplication
**Category: Result**

received·검증·출처 적용·STORE 수용은 서로 다른 사실이므로 이 반환만 작은 세 필드 묶음으로 둔다.

**Owner:** INPUT  
**Producer:** Input_Process  
**Consumer:** FLOW

### 사람이 읽는 Field 정의

| Field | 의미 / 필요한 이유 | Field 생성 주체 | 유효 조건 |
| --- | --- | --- | --- |
| `validation` | 정상 적용/품질 변화/거부/변화 없음. | INPUT | 검증 종료 뒤 |
| `contextApplied` | 출처/순서/품질 문맥 변경이 실제 반영됐는가. STORE 거부에도 true가 가능하다. | INPUT | 해당 호출 반영 완료 뒤 |
| `storeAdmission` | STORE가 실제 반환한 수용/갱신 결과. 호출하지 않았으면 NOT_SUBMITTED다. | STORE → INPUT 그대로 연결 | STORE 적용 뒤 또는 명시적 미호출 |

### Lifetime / Mutability / 폐기

호출 반환 뒤 읽기 전용 값. INPUT 문맥·STORE 원본의 복제가 아니며 다음 관측은 각 owner에서 읽는다. accepted를 validation만으로 합성하지 않는다.

**관련 Function:** [Input_Process](../30_FUNCTIONS/20_INPUT_STORE_FUNCTIONS.md#input-process) · [Store_ApplyInput](../30_FUNCTIONS/20_INPUT_STORE_FUNCTIONS.md#store-applyinput) · [Flow_Process](../30_FUNCTIONS/10_FLOW_FUNCTIONS.md#flow-process)

**관련 Contract:** [소유·수명](../50_CONTRACTS/00_CONTRACT_OVERVIEW.md#ownership)

**Provisional owner header 후보:** INPUT 반환 인터페이스 후보 — R6에서 확정. 실제 파일명·신규 Header 필요 여부는 정하지 않는다.

### Pseudo-C

```c
typedef struct
{
    VssInputValidation validation;
    SemanticBool contextApplied;
    VssStoreAdmission storeAdmission;
} VssInputApplication;
```



<a id="vssfacttime"></a>
## VssFactTime
**Category: Evidence**

실제 사건의 발생 시각 또는 가능한 발생 구간이다. Runtime 현재 읽기와 분리하여 늦은 적용으로 deadline이 바뀌지 않게 한다.

**Owner:** 해당 사실을 포착한 producer의 불변 근거  
**Producer:** 원래 의미 producer / HAL 포착 / 실제 관측 주체  
**Consumer:** INPUT/DRIVER/AUDIO STREAM/PLAYBACK/HEALTH

### 사람이 읽는 Field 정의

| Field | 의미 / 필요한 이유 | Field 생성 주체 | 유효 조건 |
| --- | --- | --- | --- |
| `domain` | 사실 시각의 비교 문맥. | 원래 producer | 원래 사실과 함께 보호 |
| `epoch` | 원래 발생 시각의 연속성 범위. | 원래 producer/Runtime | 해당 비교 범위 |
| `earliest` | 가능한 사실 발생 시각의 하한. | 원래 producer | 시간 근거가 있을 때 |
| `latest` | 상한. 정밀 시각이면 earliest와 같고 처리 시각으로 교체하지 않는다. | 원래 producer | earliest 이후; 기한 경계를 걸치면 적법한 actual 확정 금지 |
| `continuity` | 정밀/구간 비교 근거가 충분한지 구별한다. | 원래 producer/Runtime | 원래 사실 범위 |

### Lifetime / Mutability / 폐기

포착 뒤 불변이며 사실 소비/보호 기록이 유지되는 동안 보존한다. 알 수 없는 시각은 continuity 부족으로 남긴다. deadline 비교는 가능한 구간 전체와 원래 문맥을 사용하고 지금 시각으로 과거를 합성하지 않는다.

**관련 Function:** [AudioHAL_Callback](../30_FUNCTIONS/50_DRIVER_HAL_FUNCTIONS.md#audiohal-callback) · [AudioTx_Advance](../30_FUNCTIONS/50_DRIVER_HAL_FUNCTIONS.md#audiotx-advance) · [AudioStream_Advance](../30_FUNCTIONS/40_AUDIO_STREAM_ASSET_FUNCTIONS.md#audiostream-advance) · [Playback_Advance](../30_FUNCTIONS/30_POLICY_PLAYBACK_FUNCTIONS.md#playback-advance) · [Store_ApplyPlaybackFact](../30_FUNCTIONS/20_INPUT_STORE_FUNCTIONS.md#store-applyplaybackfact) · [Health_Evaluate](../30_FUNCTIONS/60_HEALTH_RUNTIME_FUNCTIONS.md#health-evaluate)

**관련 Contract:** [시간·최신성](../50_CONTRACTS/00_CONTRACT_OVERVIEW.md#timing) · [비동기 근거](../50_CONTRACTS/00_CONTRACT_OVERVIEW.md#async)

**Provisional owner header 후보:** Runtime 시간 표현 후보 / 각 producer 사실 인터페이스에서 참조 — R6에서 확정. 실제 파일명·신규 Header 필요 여부는 정하지 않는다.

### Pseudo-C

```c
typedef struct
{
    VssTimeDomainType domain;
    VssEpochType epoch;
    SemanticTimePoint earliest;
    SemanticTimePoint latest;
    VssContinuity continuity;
} VssFactTime;
```



<a id="scalar-aliases"></a>
## 보조 scalar / 불투명 의미 표기

아래는 경계 identity·정책 값의 의미 alias다. 각각 독립 struct/새 ID 생성 API를 만드는 목록이 아니다. 모든 값의 폭/encoding·확정 Header는 미정이다. 실제 producer의 원래 문맥에서 보호하고 전달 뒤 불변으로 사용한다. 회수/무효화는 해당 주 Data의 lifetime을 따른다.

| 표기 | Producer / 의미 owner | 목적 / validity |
| --- | --- | --- |
| <a id="vsstimedomaintype"></a>`VssTimeDomainType` | Runtime/원본 producer | PRODUCT/TEST와 비교 시간 문맥. 문맥 간 임의 감산 금지 |
| <a id="vssepochtype"></a>`VssEpochType` | Runtime/원본 producer | wrap/reset·연속성의 비교 범위. 실제 포착 규칙 TBD |
| <a id="vssorigintype"></a>`VssOriginType` | 기존 입력 경계 → INPUT | PRODUCT/TEST 격리. 실제 Network 표현은 미정 |
| <a id="vsssourcekeytype"></a>`VssSourceKeyType` | 기존 의미 source → INPUT 검증 | 원래 출처 identity. source authority 검증 후만 적용 |
| <a id="vssgenerationtype"></a>`VssGenerationType` | 기존 source → INPUT 검증 | 원본 세대. 재부팅/연속성 변경 규칙 TBD |
| <a id="vssorderingtype"></a>`VssOrderingType` | 기존 source → INPUT 검증 | 원본 순서. 도착 순서 아님 |
| <a id="vssrevisiontype"></a>`VssRevisionType` | 각 authoritative owner | 자신의 반영 완료 변경 표지. 타 owner를 대신해 생성하지 않음 |

```c
typedef SemanticIdentity VssTimeDomainType;
typedef SemanticIdentity VssEpochType;
typedef SemanticOrigin VssOriginType;
typedef SemanticIdentity VssSourceKeyType;
typedef SemanticOrder VssGenerationType;
typedef SemanticOrder VssOrderingType;
typedef SemanticRevision VssRevisionType;
```

Owner header 후보는 해당 Data의 의미 owner 그룹을 따른다. R6에서 기존 Header 재사용/소유를 확인한다. 별도 공통 God Header를 만들지 않는다.

## 공통 TBD / 후속 범위

논리 필드의 실제 폭/숫자·alignment/packing/ABI·storage/복사·참조 수단과 Header filename은 미정이다. RTD/DMA/TCD·`.c/.h` Mapping·Task/Queue/Mutex·실제 C 구현은 작성하지 않았다. [pseudo-C 표기](00_DATA_OVERVIEW.md#pseudo-conventions)와 [기존 Implementation TBD](../90_BINDING/91_IMPLEMENTATION_TBD.md)를 함께 확인한다. 관련 계약은 기존 원문 링크이며 R4의 Cross Contract 원문 통합을 선행하지 않았다.
