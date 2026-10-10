# Store Data — R3

> 2026-10-07 · 고정 Baseline / Execution Plan v1.1 · 논리 필드와 pseudo-C 상세

[Data Overview](00_DATA_OVERVIEW.md) · [R2 타입 판정](00_DATA_OVERVIEW.md#type-decisions)

<a id="store"></a>
One-shot은 동일 발생의 Started/final/replay 이력을, Stateful은 마지막 유효 판단·현재 품질·Hold와 사용 가능한 경고를 보존한다. 같은 RequestRecord로 합치지 않는다. 아래 상태는 STORE만 변경하며 Session/Attempt나 PCM 상태를 포함하지 않는다.

**타입 읽기:** `Category`는 역할이며 [선언 종류](00_DATA_OVERVIEW.md#pseudo-conventions)와 별개다. 아래 구성원 안내의 구조체 값과 `const T *` 읽기 참조를 구분한다. 값 필드 안의 내부 참조도 [보호 수명](../50_CONTRACTS/10_OWNERSHIP_LIFETIME.md#borrow-and-protect)을 따르며, 참조 표기만으로 원본 owner나 임시 주소의 수명이 바뀌지 않는다.

**논리 Header 안내(R6):** 이 문서의 선언 경계는 `H-STORE`다. [선언 경계 후보](../70_C_INTERFACE/00_HEADER_OWNERSHIP_MAP.md#header-candidates)와 [타입 대응](../70_C_INTERFACE/00_HEADER_OWNERSHIP_MAP.md#type-map)은 논리 소유권·가시성을 안내한다. 실제 `.h` 파일명·타입 구현 여부는 확인되지 않았고 C 기본형·메모리 표현·ABI는 미정이다.

<a id="vssstoreadmission"></a>
## VssStoreAdmission — scalar enum
**Category: Result**

단일 수용/갱신 결과이므로 Request/Result struct를 만들지 않는다. 최종 통지 추적까지 확보한 경우만 ACCEPTED다.

**Owner:** STORE. NOT_SUBMITTED 표기는 INPUT의 미호출 표지다. · **Producer:** Store_ApplyInput / INPUT 미호출 · **Consumer:** INPUT/FLOW

| 값 | 의미 / 유효 조건 |
| --- | --- |
| `VSS_STORE_NOT_SUBMITTED` | INPUT이 STORE 적용을 요구하지 않음 |
| `VSS_STORE_ACCEPTED` | 신규 occurrence와 Pending·최종 추적을 함께 확보 |
| `VSS_STORE_DUPLICATE` | 동일 발생이며 기존 이력/replay를 유지 |
| `VSS_STORE_UPDATED` | Stateful/품질/Rear의 적용이 완료됨 |
| `VSS_STORE_REJECTED` | 신규 수용/부적합 요구 거부. 기존 이력은 유지 |

**Lifetime / Mutability:** 호출 결과의 불변 scalar. DUPLICATE 상세 이력은 해당 STORE 읽기 관측으로 연결하며 새 occurrence로 만들지 않는다.

**관련 Function:** [Store_ApplyInput](../30_FUNCTIONS/20_INPUT_STORE_FUNCTIONS.md#store-applyinput) · [Input_Process](../30_FUNCTIONS/20_INPUT_STORE_FUNCTIONS.md#input-process)

**관련 Contract:** [소유·수명](../50_CONTRACTS/00_CONTRACT_OVERVIEW.md#ownership)

**논리 Header(R6):** `H-STORE`.

```c
typedef enum
{
    VSS_STORE_NOT_SUBMITTED,
    VSS_STORE_ACCEPTED,
    VSS_STORE_DUPLICATE,
    VSS_STORE_UPDATED,
    VSS_STORE_REJECTED
} VssStoreAdmission;
```

<a id="vssoccurrencestate"></a>
## VssOccurrenceState — scalar enum
**Category: State**

발생 이력 상태다. 원래 actual/전체 최종의 귀속 근거가 있어야 변경한다.

**Owner:** STORE · **Producer:** Store_ApplyInput/Store_ApplyPlaybackFact/Store_AdvanceDeadlines · **Consumer:** POLICY/PLAYBACK/HEALTH 읽기

| 값 | 의미 / 유효 조건 |
| --- | --- |
| `VSS_OCCURRENCE_PENDING` | accepted·첫 적법한 실제 시작 전 |
| `VSS_OCCURRENCE_STARTED` | 최초 적법한 actual이 한 번 반영됨 |
| `VSS_OCCURRENCE_COMPLETED` | Started 뒤 전체 계획 정상 완료와 종료 근거 확인 |
| `VSS_OCCURRENCE_INTERRUPTED` | Started 뒤 선점/고장 중단의 전체 종료 확인 |
| `VSS_OCCURRENCE_EXPIRED` | 확정 미시작·uncertain 최종 없음·원본 기한 도달 |
| `VSS_OCCURRENCE_UNCERTAIN_FINAL` | 시작 여부 불명확 최종. 재실행 금지 |

**Lifetime / Mutability:** ledger 수명. 상태 enum으로 원본 사실의 시각/귀속을 대체하지 않는다.

**관련 Function:** [Store_ApplyInput](../30_FUNCTIONS/20_INPUT_STORE_FUNCTIONS.md#store-applyinput) · [Store_ApplyPlaybackFact](../30_FUNCTIONS/20_INPUT_STORE_FUNCTIONS.md#store-applyplaybackfact) · [Store_AdvanceDeadlines](../30_FUNCTIONS/20_INPUT_STORE_FUNCTIONS.md#store-advancedeadlines)

**관련 Contract:** [소유·수명](../50_CONTRACTS/00_CONTRACT_OVERVIEW.md#ownership) · [비동기 근거](../50_CONTRACTS/00_CONTRACT_OVERVIEW.md#async)

**논리 Header(R6):** `H-STORE` · Private 원본.

```c
typedef enum
{
    VSS_OCCURRENCE_PENDING,
    VSS_OCCURRENCE_STARTED,
    VSS_OCCURRENCE_COMPLETED,
    VSS_OCCURRENCE_INTERRUPTED,
    VSS_OCCURRENCE_EXPIRED,
    VSS_OCCURRENCE_UNCERTAIN_FINAL
} VssOccurrenceState;
```

<a id="vssoccurrencerecord"></a>
## VssOccurrenceRecord
**Category: Context**

**타입 종류:** 구조체 (`struct`) — 이 절의 Pseudo-C 선언.

accepted 발생의 원본 identity·age·최초 시작·전체 최종·참조/replay 추적을 보존한다. 이 record는 수용 뒤 Session보다 오래 유지될 수 있다.

**Owner:** STORE 단일 writer  
**Producer:** Store_ApplyInput / Store_ApplyPlaybackFact / Store_AdvanceDeadlines  
**Consumer:** STORE / FLOW의 읽기 경계 → POLICY/PLAYBACK/HEALTH

### 사람이 읽는 Field 정의

| Field | 의미 / 필요한 이유 | Field 생성 주체 | 유효 조건 |
| --- | --- | --- | --- |
| `occurrence` | 동일 발생의 고정 식별. | INPUT 검증 → STORE | accepted 전 확보; ledger 수명 전체 |
| `signal` | 수용한 One-shot 의미. | INPUT → STORE | accepted 뒤 불변 |
| `originalMeta` | 원본 순서·시각/격리·연속성. | INPUT → STORE | accepted 뒤 불변. 수신/재시도로 재기산 금지 |
| `state` | Pending/Started/전체 final/replay 방어에 필요한 현재 이력. | STORE | 해당 사실 반영 완료 뒤 |
| `firstStartFact` | 최초 적법한 actual의 보호된 불변 사실 참조. 상태만으로 시각/귀속을 잃지 않는다. | PLAYBACK 판정 → STORE 보호 | Started 이후. 그 전에는 없음 |
| `finalFact` | 전체 최종/uncertain의 보호된 사실 참조. | PLAYBACK 판정 → STORE 보호 | 최종 사실 반영 이후; 정상 부활 금지 |
| `tracking` | 참조·미반영 통지·replay 보호의 회수 가능 여부를 나타내는 STORE 판단. 실제 저장/용량 수단은 미정이다. | STORE | accepted 전 추적 확보 후; 회수 근거 확인 시만 변경 |
| `revision` | owner 반영 완료·읽기 변화 확인. | STORE | 수용/사실/기한 변경 완료 뒤 |

### Lifetime / Mutability / 폐기

accepted 전에 identity/Pending/최종 통지까지 추적 자원을 함께 확보한다. 기존 accepted/final의 임의 eviction은 금지한다. 사실 참조는 STORE가 그 수명 동안 보호하며 임시 PLAYBACK 반환 포인터를 장기 저장하지 않는다. 회수는 모든 참조 종료와 재전달/replay 방어가 함께 입증된 경우만 가능하다. 출력 retirement나 HEALTH 복구만으로 삭제하지 않는다.

**관련 Function:** [Store_ApplyInput](../30_FUNCTIONS/20_INPUT_STORE_FUNCTIONS.md#store-applyinput) · [Store_ApplyPlaybackFact](../30_FUNCTIONS/20_INPUT_STORE_FUNCTIONS.md#store-applyplaybackfact) · [Store_AdvanceDeadlines](../30_FUNCTIONS/20_INPUT_STORE_FUNCTIONS.md#store-advancedeadlines) · [Select_Choose](../30_FUNCTIONS/30_POLICY_PLAYBACK_FUNCTIONS.md#select-choose) · [Health_BuildStatus](../30_FUNCTIONS/60_HEALTH_RUNTIME_FUNCTIONS.md#health-buildstatus)

**관련 Contract:** [소유·수명](../50_CONTRACTS/00_CONTRACT_OVERVIEW.md#ownership) · [비동기 근거](../50_CONTRACTS/00_CONTRACT_OVERVIEW.md#async) · [시간·최신성](../50_CONTRACTS/00_CONTRACT_OVERVIEW.md#timing)

**논리 Header(R6):** `H-STORE` · Private 원본.

### Pseudo-C

```c
typedef struct
{
    VssOccurrenceKeyType occurrence;
    VssInputSignal signal;
    VssInputMeta originalMeta;
    VssOccurrenceState state;
    const VssPlaybackFact * firstStartFact;
    const VssPlaybackFact * finalFact;
    VssTrackingDisposition tracking;
    VssRevisionType revision;
} VssOccurrenceRecord;
```

**구성원 타입 → 정의 위치**

- 의미 alias: `occurrence` → [VssOccurrenceKeyType](#vssoccurrencekeytype); `revision` → [VssRevisionType](10_INPUT_DATA.md#vssrevisiontype).
- enum: `signal` → [VssInputSignal](10_INPUT_DATA.md#vssinputsignal); `state` → [VssOccurrenceState](#vssoccurrencestate); `tracking` → [VssTrackingDisposition](#vsstrackingdisposition).
- 구조체 값: `originalMeta` → [VssInputMeta](10_INPUT_DATA.md#vssinputmeta).
- 구조체 읽기 참조 (`const T *`): `firstStartFact`, `finalFact` → [VssPlaybackFact](40_PLAYBACK_DATA.md#vssplaybackfact).

[잠정] 첫 실제 시작 age < 2초이며 정확히 2초는 불가다. USE_LIMIT은 별도 원문 조건으로 남기고 min/AND 정책을 만들지 않는다. Started Session의 정상 후속 cue/반복에 첫 시작 기한을 다시 적용하지 않는다.

<a id="vsstrackingdisposition"></a>
## VssTrackingDisposition — scalar enum
**Category: State**

추적 보호를 위한 의미 상태이며 refcount·pool·Queue를 설계하지 않는다.

**Owner:** STORE · **Producer:** STORE Core · **Consumer:** STORE

| 값 | 의미 / 유효 조건 |
| --- | --- |
| `VSS_TRACKING_PROTECTED` | 수용 발생·통지·참조 또는 replay 보호가 필요 |
| `VSS_TRACKING_RELEASE_PROVEN` | 참조 종료와 replay 방어를 모두 확인하여 해당 record 회수 가능 |

**Lifetime / Mutability:** 해당 occurrence의 추적 수명. 공간 부족은 새 수용 제한이며 옛 record 삭제 근거가 아니다.

**관련 Function:** [Store_ApplyInput](../30_FUNCTIONS/20_INPUT_STORE_FUNCTIONS.md#store-applyinput) · [Store_ApplyPlaybackFact](../30_FUNCTIONS/20_INPUT_STORE_FUNCTIONS.md#store-applyplaybackfact) · [Store_AdvanceDeadlines](../30_FUNCTIONS/20_INPUT_STORE_FUNCTIONS.md#store-advancedeadlines)

**관련 Contract:** [소유·수명](../50_CONTRACTS/00_CONTRACT_OVERVIEW.md#ownership)

**논리 Header(R6):** `H-STORE` · Private 원본.

```c
typedef enum
{
    VSS_TRACKING_PROTECTED,
    VSS_TRACKING_RELEASE_PROVEN
} VssTrackingDisposition;
```

<a id="vssstatefulstate"></a>
## VssStatefulState
**Category: State**

**타입 종류:** 구조체 (`struct`) — 이 절의 Pseudo-C 선언.

마지막 유효 판단과 현재 품질을 분리하고 최초 Hold 기산을 유지한다. candidate는 이 근거에서 STORE가 노출하는 파생 관측이며 두 번째 state 원본을 만들지 않는다.

**Owner:** STORE 단일 writer  
**Producer:** Store_ApplyInput / Store_AdvanceDeadlines  
**Consumer:** STORE / FLOW → POLICY/PLAYBACK/HEALTH

### 사람이 읽는 Field 정의

| Field | 의미 / 필요한 이유 | Field 생성 주체 | 유효 조건 |
| --- | --- | --- | --- |
| `stateKey` | 현재 경고의 의미/PRODUCT·TEST 문맥 연결. occurrence key가 아니다. | STORE의 의미 대상 연결 | 해당 상태 문맥 수명 |
| `signal` | Anti-Pinch/Occupant/Rear Risk 대상. | INPUT → STORE | 문맥 수명 |
| `lastValidValue` | 마지막 유효 ACTIVE/CLEAR/Rear 위험 판단. | INPUT 검증 → STORE | lastValidMeta가 존재하는 경우만 의미 유효 |
| `lastValidMeta` | 마지막 유효 판단의 보호된 원본 근거. 없으면 INVALID만으로 경고를 만들 수 없다. | STORE가 보호 | 유효 판단 후; 후보 제거 뒤에도 품질 판단 필요 기간 유지 |
| `quality` | 현재 입력 품질. lastValidValue를 지우거나 CLEAR로 바꾸지 않는다. | INPUT → STORE | 최신 적용 품질 반영 뒤 |
| `firstLossAt` | 기존 유효 경고의 최초 신뢰성 상실 시각 근거. local temporary가 아니라 RX 없는 기한에 필요한 불변 값이다. | INPUT/Runtime 근거 → STORE 보호 | 최초 상실 뒤; 반복 상실로 교체 금지 |
| `holdAnchor` | 최초 상실과 lastValidMeta 원본 validUntil 중 더 이른 기산. 별도 Hold struct를 만들지 않는다. | STORE | 비교 근거가 충분한 최초 상실 뒤; 불충분하면 기산 확정 보류 |
| `revision` | 반영 완료·관측 변화 확인. | STORE | 변경 완료 뒤 |

### Lifetime / Mutability / 폐기

STORE만 마지막 유효 판단·품질/Hold를 변경한다. valid CLEAR는 후보를 제거하고 INVALID/STALE는 품질 상실로 남긴다. Hold 만료는 후보 제거이며 품질 정상화가 아니다. 보호된 Meta/최초 상실·기산은 단순 local 계산을 저장한 것이 아니라 RX 없는 기한 판단에 필요한 지속 근거다. 회수는 현재 품질·후속 판단 참조가 끝났을 때만 가능하다.

**관련 Function:** [Store_ApplyInput](../30_FUNCTIONS/20_INPUT_STORE_FUNCTIONS.md#store-applyinput) · [Store_AdvanceDeadlines](../30_FUNCTIONS/20_INPUT_STORE_FUNCTIONS.md#store-advancedeadlines) · [Select_Choose](../30_FUNCTIONS/30_POLICY_PLAYBACK_FUNCTIONS.md#select-choose) · [Playback_RequestTransition](../30_FUNCTIONS/30_POLICY_PLAYBACK_FUNCTIONS.md#playback-requesttransition) · [Health_BuildStatus](../30_FUNCTIONS/60_HEALTH_RUNTIME_FUNCTIONS.md#health-buildstatus)

**관련 Contract:** [시간·최신성](../50_CONTRACTS/00_CONTRACT_OVERVIEW.md#timing) · [소유·수명](../50_CONTRACTS/00_CONTRACT_OVERVIEW.md#ownership)

**논리 Header(R6):** `H-STORE` · Private 원본.

### Pseudo-C

```c
typedef struct
{
    VssStatefulKeyType stateKey;
    VssInputSignal signal;
    VssInputValue lastValidValue;
    const VssInputMeta * lastValidMeta;
    VssInputQuality quality;
    const VssFactTime * firstLossAt;
    const SemanticTimePoint * holdAnchor;
    VssRevisionType revision;
} VssStatefulState;
```

**구성원 타입 → 정의 위치**

- 의미 alias: `stateKey` → [VssStatefulKeyType](#vssstatefulkeytype); `revision` → [VssRevisionType](10_INPUT_DATA.md#vssrevisiontype).
- enum: `signal` → [VssInputSignal](10_INPUT_DATA.md#vssinputsignal); `lastValidValue` → [VssInputValue](10_INPUT_DATA.md#vssinputvalue); `quality` → [VssInputQuality](10_INPUT_DATA.md#vssinputquality).
- 구조체 읽기 참조 (`const T *`): `lastValidMeta` → [VssInputMeta](10_INPUT_DATA.md#vssinputmeta); `firstLossAt` → [VssFactTime](10_INPUT_DATA.md#vssfacttime).
- pseudo-scalar 읽기 참조 (`const T *`): `holdAnchor` → [SemanticTimePoint](00_DATA_OVERVIEW.md#pseudo-conventions).



### Hold 근거의 INLINE 판정

최초 상실 시각과 Hold 기산은 같은 STORE/Stateful 문맥의 지속 필드다. 별도 VssHoldBasis struct는 만들지 않는다. 원본 유효기한/시간 문맥은 보호된 lastValidMeta에 이미 있으므로 중복 저장하지 않는다. 반복 INVALID·재전달·선점·복구로 firstLossAt/holdAnchor를 교체하지 않는다. 유효 최신 상태/해제·Rear DISABLED 등 기존 정책으로 해당 Hold가 끝나면 관련 참조를 안전 종료한다. Hold 만료는 후보 제거이고 품질 정상화가 아니다. 시간 비교 근거 부족을 정밀 기산으로 합성하지 않는다.

<a id="vssreargatestate"></a>
## VssRearGateState
**Category: State**

**타입 종류:** 구조체 (`struct`) — 이 절의 Pseudo-C 선언.

Rear Activation은 위험 판단과 별도 현재 상태다. 재활성화만으로 옛 위험/Hold를 부활시키지 않기 위해 현재 activation에 적용할 수 있는 위험 근거를 구별한다.

**Owner:** STORE  
**Producer:** Store_ApplyInput  
**Consumer:** STORE 후보 결합 / POLICY·PLAYBACK 읽기

### 사람이 읽는 Field 정의

| Field | 의미 / 필요한 이유 | Field 생성 주체 | 유효 조건 |
| --- | --- | --- | --- |
| `activation` | ENABLED/DISABLED의 최신 적용 가능한 값. | INPUT → STORE | Activation의 최신 검증 후 |
| `activationMeta` | Activation 출처·세대·순서 근거. | INPUT → STORE | 해당 activation 적용 수명 |
| `riskState` | 별도 Rear 위험 상태를 연결하는 key. 위험 원본을 여기 복제하지 않는다. | STORE | 해당 Rear 위험 문맥 |
| `riskApplicable` | 현재 activation에 적용 가능한 위험이 검증·반영됐는가. enable 자체는 true 근거가 아니다. | STORE | 최신 위험/activation 결합 검증 뒤 |
| `revision` | 결합 관측의 반영 완료. | STORE | 변경 완료 뒤 |

### Lifetime / Mutability / 폐기

STORE만 변경한다. DISABLED는 후보/Hold 제거 및 riskApplicable 차단이며 CLEAR와 다르다. 재활성화 뒤 적용 가능한 위험 근거를 확인하기 전 옛 EMERGENCY를 후보로 만들지 않는다. 정확한 source/ordering 비교 방식은 기존 Network 계약/TBD로 남긴다.

**관련 Function:** [Store_ApplyInput](../30_FUNCTIONS/20_INPUT_STORE_FUNCTIONS.md#store-applyinput) · [Store_AdvanceDeadlines](../30_FUNCTIONS/20_INPUT_STORE_FUNCTIONS.md#store-advancedeadlines) · [Select_Choose](../30_FUNCTIONS/30_POLICY_PLAYBACK_FUNCTIONS.md#select-choose)

**관련 Contract:** [시간·최신성](../50_CONTRACTS/00_CONTRACT_OVERVIEW.md#timing)

**논리 Header(R6):** `H-STORE` · Private 원본.

### Pseudo-C

```c
typedef struct
{
    VssInputValue activation;
    VssInputMeta activationMeta;
    VssStatefulKeyType riskState;
    SemanticBool riskApplicable;
    VssRevisionType revision;
} VssRearGateState;
```

**구성원 타입 → 정의 위치**

- enum: `activation` → [VssInputValue](10_INPUT_DATA.md#vssinputvalue).
- 구조체 값: `activationMeta` → [VssInputMeta](10_INPUT_DATA.md#vssinputmeta).
- 의미 alias: `riskState` → [VssStatefulKeyType](#vssstatefulkeytype); `revision` → [VssRevisionType](10_INPUT_DATA.md#vssrevisiontype).
- pseudo-scalar: `riskApplicable` → [SemanticBool](00_DATA_OVERVIEW.md#pseudo-conventions).



<a id="vssfactapplication"></a>
## VssFactApplication — scalar enum
**Category: Result**

PLAYBACK 통지 의무를 끝낼 수 있는지 보여주는 scalar다. 신규 입력 수용 enum과 합치면 기존 사실을 공간 부족으로 버리는 오류가 생긴다.

**Owner:** STORE · **Producer:** Store_ApplyPlaybackFact · **Consumer:** PLAYBACK

| 값 | 의미 / 유효 조건 |
| --- | --- |
| `VSS_FACT_APPLIED` | 귀속된 사실이 원래 occurrence에 반영 완료됨 |
| `VSS_FACT_ALREADY_APPLIED` | 같은 사실이 이미 적용됨. 중복에 안전한 완료 |
| `VSS_FACT_NOT_APPLIED` | 귀속/근거 부족·충돌로 완료하지 못함. 사실/의무 보호 유지 |

**Lifetime / Mutability:** 해당 fact 적용 호출 결과. APPLIED/ALREADY_APPLIED만 그 사실의 통지 의무 완료 근거다.

**관련 Function:** [Store_ApplyPlaybackFact](../30_FUNCTIONS/20_INPUT_STORE_FUNCTIONS.md#store-applyplaybackfact) · [Playback_Advance](../30_FUNCTIONS/30_POLICY_PLAYBACK_FUNCTIONS.md#playback-advance)

**관련 Contract:** [소유·수명](../50_CONTRACTS/00_CONTRACT_OVERVIEW.md#ownership) · [비동기 근거](../50_CONTRACTS/00_CONTRACT_OVERVIEW.md#async)

**논리 Header(R6):** `H-STORE`.

```c
typedef enum
{
    VSS_FACT_APPLIED,
    VSS_FACT_ALREADY_APPLIED,
    VSS_FACT_NOT_APPLIED
} VssFactApplication;
```

<a id="vssstoreprogress"></a>
## VssStoreProgress — scalar enum
**Category: Result**

기한 처리의 변화/재확인 결과만 반환한다. owner 원본과 현재 후보는 별도 읽기 경계로 소비한다.

**Owner:** STORE · **Producer:** Store_AdvanceDeadlines · **Consumer:** FLOW

| 값 | 의미 / 유효 조건 |
| --- | --- |
| `VSS_STORE_STABLE` | 이번 처리에서 기한/후보 변화 없음 |
| `VSS_STORE_CHANGED` | 기한/후보 변경이 반영 완료됨 |
| `VSS_STORE_RECONFIRM_REQUIRED` | 시간 또는 최신 PLAYBACK 사실이 부족하여 만료 확정 보류 |

**Lifetime / Mutability:** 호출별 불변 scalar. age만으로 in-flight/uncertain을 Expired로 만들지 않는다.

**관련 Function:** [Store_AdvanceDeadlines](../30_FUNCTIONS/20_INPUT_STORE_FUNCTIONS.md#store-advancedeadlines) · [Flow_Process](../30_FUNCTIONS/10_FLOW_FUNCTIONS.md#flow-process)

**관련 Contract:** [시간·최신성](../50_CONTRACTS/00_CONTRACT_OVERVIEW.md#timing) · [비동기 근거](../50_CONTRACTS/00_CONTRACT_OVERVIEW.md#async)

**논리 Header(R6):** `H-STORE`.

```c
typedef enum
{
    VSS_STORE_STABLE,
    VSS_STORE_CHANGED,
    VSS_STORE_RECONFIRM_REQUIRED
} VssStoreProgress;
```

<a id="scalar-aliases"></a>
## 보조 scalar / 불투명 의미 표기

아래는 경계 identity·정책 값의 의미 alias다. 각각 독립 struct/새 ID 생성 API를 만드는 목록이 아니다. 모든 값의 폭/encoding·확정 Header는 미정이다. 실제 producer의 원래 문맥에서 보호하고 전달 뒤 불변으로 사용한다. 회수/무효화는 해당 주 Data의 lifetime을 따른다.

| 표기 | Producer / 의미 owner | 목적 / validity |
| --- | --- | --- |
| <a id="vssoccurrencekeytype"></a>`VssOccurrenceKeyType` | 원래 의미 identity → INPUT 검증/STORE 보호 | 동일 One-shot 발생. 재전달마다 생성 금지; 생성/재부팅/회수 규칙 TBD |
| <a id="vssstatefulkeytype"></a>`VssStatefulKeyType` | STORE 의미 대상 연결 | 현재 경고·PRODUCT/TEST 문맥. occurrence identity와 교환 금지 |

```c
typedef SemanticIdentity VssOccurrenceKeyType;
typedef SemanticIdentity VssStatefulKeyType;
```

두 alias의 R6 논리 선언 경계는 `H-STORE`다. [타입 대응](../70_C_INTERFACE/00_HEADER_OWNERSHIP_MAP.md#type-map)의 선언 hosting과 위 producer·의미 owner를 구별한다. 실제 C typedef·Header 구현은 미정이다.

## 공통 TBD / 후속 범위

논리 필드의 실제 폭/숫자·alignment/packing/ABI·storage/복사·참조 수단과 Header filename은 미정이다. RTD/DMA/TCD·`.c/.h` Mapping·Task/Queue/Mutex·실제 C 구현은 작성하지 않았다. [pseudo-C 표기](00_DATA_OVERVIEW.md#pseudo-conventions)와 [기존 Implementation TBD](../90_BINDING/91_IMPLEMENTATION_TBD.md)를 함께 확인한다. 관련 계약은 기존 원문 링크이며 R4의 Cross Contract 원문 통합을 선행하지 않았다.
