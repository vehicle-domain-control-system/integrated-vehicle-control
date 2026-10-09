# Selection / Read View Data — R3

> 2026-10-07 · 고정 Baseline / Execution Plan v1.1 · 논리 필드와 pseudo-C 상세

[Data Overview](00_DATA_OVERVIEW.md) · [R2 타입 판정](00_DATA_OVERVIEW.md#type-decisions)

<a id="selection"></a>
선택은 POLICY, 실행은 PLAYBACK 책임이다. FLOW는 원본을 변경하지 않고 각 owner가 반영한 읽기 관측을 연결한다. `VssSelectionView`와 `VssExecutionConditions`는 아래 작은 관측의 읽기 전용 인자 묶음이며 **그 이름의 struct는 만들지 않는다**.

VssSelectionView: STORE의 VssCandidateObservation들 + PLAYBACK의 VssPlaybackObservation + HEALTH의 현재 대상 제한 + ASSET의 확인 범위/가용 근거 + VssReadBasis. 각 자료는 owner가 생성한다.

VssExecutionConditions: 이번 후보의 현재 적용 가능 관측 + 해당 HEALTH 제한/복구 허용 + 비교 가능한 시간/읽기 근거. PLAYBACK의 현재 owner·후속 권한, AUDIO STREAM의 PCM/준비 자원은 각 callee가 자기 원본에서 확인한다. 이 묶음은 새 실행 허가나 여러 owner 상태의 복제 원본이 아니다.

<a id="vssownerstamp"></a>
## VssOwnerStamp
**Category: Snapshot**

owner가 어디까지 실제 반영했는지의 읽기 표지다. FLOW가 결과를 몰래 적용하는 getter나 전체 상태 복사를 만들지 않도록 한다.

**Owner:** 각 원본 owner의 읽기 표지  
**Producer:** INPUT/STORE/PLAYBACK/AUDIO STREAM/HEALTH의 반영 완료 읽기 경계  
**Consumer:** FLOW의 관측 수집/일관성 검토

### 사람이 읽는 Field 정의

| Field | 의미 / 필요한 이유 | Field 생성 주체 | 유효 조건 |
| --- | --- | --- | --- |
| `owner` | 어느 원본 owner의 표지인지 구별한다. | 해당 owner | 원본 읽기 때 |
| `revision` | 원본 변경의 반영 완료 revision. | 해당 owner | 반영 완료 뒤; 수집 뒤 변경되면 새 평가 필요 |
| `appliedThrough` | 필요 사실이 어디까지 반영됐는지의 의미 경계. 단순 CPU 복사 시각과 다르다. | 해당 owner | 해당 인과 범위의 반영 확인 뒤 |

### Lifetime / Mutability / 폐기

읽기 평가 동안 불변으로 빌리거나 복사한다. 포착 방식/원자성·lock 수단은 미정이다. 오래 보관해 실행 권한으로 쓰지 않고 채택·새 start 전 최신 원본을 재확인한다.

**관련 Function:** [Flow_Process](../30_FUNCTIONS/10_FLOW_FUNCTIONS.md#flow-process) · [Select_Choose](../30_FUNCTIONS/30_POLICY_PLAYBACK_FUNCTIONS.md#select-choose) · [Playback_RequestTransition](../30_FUNCTIONS/30_POLICY_PLAYBACK_FUNCTIONS.md#playback-requesttransition) · [Playback_Advance](../30_FUNCTIONS/30_POLICY_PLAYBACK_FUNCTIONS.md#playback-advance) · [Health_Evaluate](../30_FUNCTIONS/60_HEALTH_RUNTIME_FUNCTIONS.md#health-evaluate) · [Health_BuildStatus](../30_FUNCTIONS/60_HEALTH_RUNTIME_FUNCTIONS.md#health-buildstatus)

**관련 Contract:** [소유·수명](../50_CONTRACTS/00_CONTRACT_OVERVIEW.md#ownership) · [시간·최신성](../50_CONTRACTS/00_CONTRACT_OVERVIEW.md#timing)

**Provisional owner header 후보:** 각 owner의 읽기 관측 인터페이스 후보 — R6에서 확정. 실제 파일명·신규 Header 필요 여부는 정하지 않는다.

### Pseudo-C

```c
typedef struct
{
    VssOwnerKeyType owner;
    VssRevisionType revision;
    VssFactWatermarkType appliedThrough;
} VssOwnerStamp;
```



<a id="vssreadvalidity"></a>
## VssReadValidity — scalar enum
**Category: Snapshot**

한 평가에서 시간·인과·반영 관계를 함께 판단할 수 있는지 보여준다.

**Owner:** FLOW의 수집 판단 · **Producer:** Flow_Process · **Consumer:** POLICY/PLAYBACK/HEALTH

| 값 | 의미 / 유효 조건 |
| --- | --- |
| `VSS_READ_VALID` | 필요 owner의 반영/시간·인과 근거가 함께 적용 가능 |
| `VSS_READ_CHANGED` | 수집 중 관련 owner가 변경됨 |
| `VSS_READ_INCOMPLETE` | 필수 사실/시간·연속성 근거가 부족하거나 손실됨 |

**Lifetime / Mutability:** 평가 동안만 유효하다. VALID도 후속 start 권한을 영구 보장하지 않는다.

**관련 Function:** [Flow_Process](../30_FUNCTIONS/10_FLOW_FUNCTIONS.md#flow-process) · [Select_Choose](../30_FUNCTIONS/30_POLICY_PLAYBACK_FUNCTIONS.md#select-choose) · [Health_BuildStatus](../30_FUNCTIONS/60_HEALTH_RUNTIME_FUNCTIONS.md#health-buildstatus)

**관련 Contract:** [시간·최신성](../50_CONTRACTS/00_CONTRACT_OVERVIEW.md#timing) · [소유·수명](../50_CONTRACTS/00_CONTRACT_OVERVIEW.md#ownership)

**Provisional owner header 후보:** FLOW 읽기 관측 후보 — R6에서 확정.

```c
typedef enum
{
    VSS_READ_VALID,
    VSS_READ_CHANGED,
    VSS_READ_INCOMPLETE
} VssReadValidity;
```

<a id="vssreadbasis"></a>
## VssReadBasis
**Category: Snapshot**

함께 판단 가능한 관측의 적용 근거만 묶는다. STORE·PLAYBACK·HEALTH의 authoritative state를 포함하지 않는다.

**Owner:** FLOW의 임시 수집 근거. stamp 원본은 각 owner다.  
**Producer:** 각 owner stamp + Runtime → FLOW 수집  
**Consumer:** POLICY/PLAYBACK/HEALTH

### 사람이 읽는 Field 정의

| Field | 의미 / 필요한 이유 | Field 생성 주체 | 유효 조건 |
| --- | --- | --- | --- |
| `stamps` | 필요 owner들의 불변 읽기 표지 목록. 상태 자체를 복제하지 않는다. | 각 owner → FLOW | 목록 storage가 보호된 평가 동안 |
| `stampCount` | 이 평가가 실제 포함한 표지 수. 고정 owner 수/배열 용량을 정하지 않는다. | FLOW | stamps의 유효 범위 |
| `time` | 시간 비교에 사용한 원래 읽기 근거. | Runtime → FLOW | 해당 평가 기간 |
| `validity` | 수집 중 변화/미반영·손실을 구별한다. | FLOW | 필수 범위 대조 뒤 |

### Lifetime / Mutability / 폐기

한 평가 동안 읽기 전용이다. 관련 변화/손실이면 무효화하고 다시 수집한다. Session은 이 묶음 포인터를 장기 저장하지 않고 필요한 불변 후보·계획 identity만 보호한다. 보고가 비동기 전달되면 보고 소유 storage에 불변 표지를 보호하며 mutable 원본 포인터를 운반하지 않는다.

**관련 Function:** [Flow_Process](../30_FUNCTIONS/10_FLOW_FUNCTIONS.md#flow-process) · [Select_Choose](../30_FUNCTIONS/30_POLICY_PLAYBACK_FUNCTIONS.md#select-choose) · [Playback_RequestTransition](../30_FUNCTIONS/30_POLICY_PLAYBACK_FUNCTIONS.md#playback-requesttransition) · [Playback_Advance](../30_FUNCTIONS/30_POLICY_PLAYBACK_FUNCTIONS.md#playback-advance) · [Health_Evaluate](../30_FUNCTIONS/60_HEALTH_RUNTIME_FUNCTIONS.md#health-evaluate) · [Health_BuildStatus](../30_FUNCTIONS/60_HEALTH_RUNTIME_FUNCTIONS.md#health-buildstatus)

**관련 Contract:** [시간·최신성](../50_CONTRACTS/00_CONTRACT_OVERVIEW.md#timing) · [소유·수명](../50_CONTRACTS/00_CONTRACT_OVERVIEW.md#ownership)

**Provisional owner header 후보:** FLOW 읽기 관측 후보 — R6에서 확정. 실제 파일명·신규 Header 필요 여부는 정하지 않는다.

### Pseudo-C

```c
typedef struct
{
    const VssOwnerStamp * stamps;
    SemanticCount stampCount;
    VssTimeEvidence time;
    VssReadValidity validity;
} VssReadBasis;
```



<a id="vsscandidatevalidity"></a>
## VssCandidateValidity — scalar enum
**Category: Snapshot**

STORE 후보의 현재 적용 가능 의미다. 출력 Fault·처리 성공 enum과 합치지 않는다.

**Owner:** STORE 파생 읽기 · **Producer:** Store_ApplyInput/Store_AdvanceDeadlines 이후 읽기 경계 · **Consumer:** POLICY/PLAYBACK

| 값 | 의미 / 유효 조건 |
| --- | --- |
| `VSS_CANDIDATE_APPLICABLE` | 현재 원본 상태/품질·Hold와 기한 기준으로 검토 가능한 후보 |
| `VSS_CANDIDATE_CLEARED` | 유효 CLEAR로 후보 제거 |
| `VSS_CANDIDATE_DISABLED` | Rear DISABLED로 후보/Hold 제거 |
| `VSS_CANDIDATE_EXPIRED` | 원본 기한/Hold로 후보 불가. in-flight/uncertain의 이력 Expired 확정과 동일시하지 않음 |
| `VSS_CANDIDATE_UNCERTAIN` | 현재 적용 가능 근거 부족 |

**Lifetime / Mutability:** 읽기 평가 기간. 새 기한/입력 변화 시 재평가.

**관련 Function:** [Store_ApplyInput](../30_FUNCTIONS/20_INPUT_STORE_FUNCTIONS.md#store-applyinput) · [Store_AdvanceDeadlines](../30_FUNCTIONS/20_INPUT_STORE_FUNCTIONS.md#store-advancedeadlines) · [Select_Choose](../30_FUNCTIONS/30_POLICY_PLAYBACK_FUNCTIONS.md#select-choose) · [Playback_RequestTransition](../30_FUNCTIONS/30_POLICY_PLAYBACK_FUNCTIONS.md#playback-requesttransition)

**관련 Contract:** [시간·최신성](../50_CONTRACTS/00_CONTRACT_OVERVIEW.md#timing)

**Provisional owner header 후보:** STORE 후보 관측 인터페이스 후보 — R6에서 확정.

```c
typedef enum
{
    VSS_CANDIDATE_APPLICABLE,
    VSS_CANDIDATE_CLEARED,
    VSS_CANDIDATE_DISABLED,
    VSS_CANDIDATE_EXPIRED,
    VSS_CANDIDATE_UNCERTAIN
} VssCandidateValidity;
```

<a id="vsscandidateobservation"></a>
## VssCandidateObservation
**Category: Snapshot**

POLICY/PLAYBACK이 필요한 후보 의미·원본 기한·identity만 보는 STORE projection이다. One-shot ledger와 StatefulState를 통합 저장하지 않는다.

**Owner:** STORE 파생 읽기  
**Producer:** STORE Core 반영 후 기존 읽기 관측  
**Consumer:** FLOW → POLICY/PLAYBACK / HEALTH 보고

### 사람이 읽는 Field 정의

| Field | 의미 / 필요한 이유 | Field 생성 주체 | 유효 조건 |
| --- | --- | --- | --- |
| `key` | 원본 occurrence 또는 Stateful 상태 key의 안정된 의미 연결. mode와 함께만 해석한다. | STORE | 현재 읽기 대상과 연결된 동안 |
| `mode` | ONE_SHOT/STATEFUL을 구별한다. | STORE | key와 같은 원본 |
| `signal` | 우선순위·음원 정책에 필요한 의미. | STORE | 원본 읽기 기간 |
| `validity` | 현재 검토 가능한 후보인가. | STORE | 반영 완료 뒤; 정책 판단/start 직전 재확인 |
| `originalMeta` | One-shot 원본 발생 또는 Stateful 마지막 유효 판단 근거의 읽기 참조. | STORE 보호 원본 | 참조가 보호된 읽기 기간 |
| `useLimit` | 원문 USE_LIMIT이 적용되는 경우의 별도 의미 조건. 새 min/AND 정책을 정하지 않는다. | 기존 정책 → STORE 읽기 | 조건이 있는 후보에만 존재; 미정이면 TBD 근거를 유지 |
| `revision` | 후속 채택에서 같은 읽기 대상이 유지되는지 대조할 표지. | STORE | 해당 읽기 기간 |

### Lifetime / Mutability / 폐기

STATEFUL과 ONE_SHOT의 공유 부분만 읽기 projection으로 표현한다. 원본 저장과 lifespan은 20_STORE_DATA의 두 기록으로 분리되어 있다. 이 관측은 평가 기간의 borrow이며 PLAYBACK이 Session 채택 시 안정된 key/계획·필요 원본 시간만 자기 문맥에 보호한다. 늦은 요청은 key가 같아도 revision/최신 조건을 재확인한다.

**관련 Function:** [Store_ApplyInput](../30_FUNCTIONS/20_INPUT_STORE_FUNCTIONS.md#store-applyinput) · [Store_AdvanceDeadlines](../30_FUNCTIONS/20_INPUT_STORE_FUNCTIONS.md#store-advancedeadlines) · [Select_Choose](../30_FUNCTIONS/30_POLICY_PLAYBACK_FUNCTIONS.md#select-choose) · [Playback_RequestTransition](../30_FUNCTIONS/30_POLICY_PLAYBACK_FUNCTIONS.md#playback-requesttransition) · [Playback_Advance](../30_FUNCTIONS/30_POLICY_PLAYBACK_FUNCTIONS.md#playback-advance) · [Health_BuildStatus](../30_FUNCTIONS/60_HEALTH_RUNTIME_FUNCTIONS.md#health-buildstatus)

**관련 Contract:** [소유·수명](../50_CONTRACTS/00_CONTRACT_OVERVIEW.md#ownership) · [시간·최신성](../50_CONTRACTS/00_CONTRACT_OVERVIEW.md#timing)

**Provisional owner header 후보:** STORE 후보 읽기 인터페이스 후보 — R6에서 확정. 실제 파일명·신규 Header 필요 여부는 정하지 않는다.

### Pseudo-C

```c
typedef struct
{
    VssCandidateKeyType key;
    VssPlaybackMode mode;
    VssInputSignal signal;
    VssCandidateValidity validity;
    const VssInputMeta * originalMeta;
    const VssUseLimitType * useLimit;
    VssRevisionType revision;
} VssCandidateObservation;
```



<a id="vssplaybackmode"></a>
## VssPlaybackMode — scalar enum
**Category: Descriptor**

발생형 정책과 유지형 정책을 구별한다. Stateful에는 One-shot 재실행 이력을 붙이지 않는다.

**Owner:** POLICY 의미 / STORE 후보 분류 · **Producer:** 중앙 정책·STORE 의미 · **Consumer:** POLICY/PLAYBACK

| 값 | 의미 / 유효 조건 |
| --- | --- |
| `VSS_MODE_ONE_SHOT` | 동일 occurrence의 전체 계획. Started 뒤 자동 resume 금지 |
| `VSS_MODE_STATEFUL` | 현재 유효 경고의 정책. 선점 뒤 여전히 유효하면 새 Session의 정책 시작점 |

**Lifetime / Mutability:** 후보/채택 계획의 불변 분류.

**관련 Function:** [Select_Choose](../30_FUNCTIONS/30_POLICY_PLAYBACK_FUNCTIONS.md#select-choose) · [Playback_RequestTransition](../30_FUNCTIONS/30_POLICY_PLAYBACK_FUNCTIONS.md#playback-requesttransition)

**관련 Contract:** [소유·수명](../50_CONTRACTS/00_CONTRACT_OVERVIEW.md#ownership)

**Provisional owner header 후보:** POLICY 의미 인터페이스 후보 — R6에서 확정.

```c
typedef enum
{
    VSS_MODE_ONE_SHOT,
    VSS_MODE_STATEFUL
} VssPlaybackMode;
```

<a id="vsscuedescriptor"></a>
## VssCueDescriptor
**Category: Descriptor**

전체 계획의 음원/무음 의미를 표현한다. cue 수와 Attempt 수를 1:1로 고정하지 않고 PCM/decoder/장치 설정을 넣지 않는다.

**Owner:** POLICY 읽기 전용 중앙 정책  
**Producer:** 기존 의미→전체 패턴 정책  
**Consumer:** Select_Choose / PLAYBACK 정상 cue 진행

### 사람이 읽는 Field 정의

| Field | 의미 / 필요한 이유 | Field 생성 주체 | 유효 조건 |
| --- | --- | --- | --- |
| `asset` | 이 cue가 사용할 의미 음원 ID. 주소가 아니다. | POLICY | 정의된 패턴에서만 |
| `gapAfter` | 해당 cue 뒤 정책상 무음/대기 의미. 실제 값은 미정 패턴의 TBD를 따른다. | POLICY | 정의된 cue의 정책 수명 |

### Lifetime / Mutability / 폐기

정책/image 문맥이 유효한 동안 불변이다. 미정 값을 0 또는 임의 횟수로 채우지 않는다. 채택 Session이 참조하는 동안 정책 문맥을 덮지 않는다.

**관련 Function:** [Select_Choose](../30_FUNCTIONS/30_POLICY_PLAYBACK_FUNCTIONS.md#select-choose) · [Playback_RequestTransition](../30_FUNCTIONS/30_POLICY_PLAYBACK_FUNCTIONS.md#playback-requesttransition) · [Playback_Advance](../30_FUNCTIONS/30_POLICY_PLAYBACK_FUNCTIONS.md#playback-advance)

**관련 Contract:** [소유·수명](../50_CONTRACTS/00_CONTRACT_OVERVIEW.md#ownership)

**Provisional owner header 후보:** POLICY 계획 Descriptor 후보 — R6에서 확정. 실제 파일명·신규 Header 필요 여부는 정하지 않는다.

### Pseudo-C

```c
typedef struct
{
    VssAssetKeyType asset;
    SemanticDuration gapAfter;
} VssCueDescriptor;
```



<a id="vssplandescriptor"></a>
## VssPlanDescriptor
**Category: Descriptor**

전체 정책의 의미 계획이다. 실행 중 cue 위치·반복 진행은 PLAYBACK이 별도로 소유한다.

**Owner:** POLICY  
**Producer:** 중앙 읽기 전용 정책 / Select_Choose 연결  
**Consumer:** FLOW → PLAYBACK → AUDIO STREAM의 의미 준비

### 사람이 읽는 Field 정의

| Field | 의미 / 필요한 이유 | Field 생성 주체 | 유효 조건 |
| --- | --- | --- | --- |
| `planKey` | 채택한 전체 패턴과 정책 문맥을 안정적으로 식별한다. | POLICY | 정책 정의 확인 뒤 |
| `signal` | 어떤 의미의 계획인지 확인한다. 다른 의미 fallback 방지에 필요하다. | POLICY | 계획 수명 |
| `cues` | 정의된 cue 의미 목록의 불변 참조. | POLICY | 목록/정책 참조 보호 동안 |
| `cueCount` | 정의된 전체 계획의 cue 수. 수치/용량은 이번에 정하지 않는다. | POLICY | 정의된 패턴에 한함 |
| `repeatRule` | 기존 전체 패턴의 반복 규칙. 세부 횟수/조건은 기존 [잠정/TBD]. | POLICY | 정의된 정책 문맥 |

### Lifetime / Mutability / 폐기

평가 시 후보와 연결하고 Session 채택 뒤 전체 계획/참조를 보호한다. 정책은 실행 중 읽기 전용이며 미정 패턴은 정상 계획으로 합성하지 않는다. 한 cue EOS는 전체 plan 완료가 아니다.

**관련 Function:** [Select_Choose](../30_FUNCTIONS/30_POLICY_PLAYBACK_FUNCTIONS.md#select-choose) · [Playback_RequestTransition](../30_FUNCTIONS/30_POLICY_PLAYBACK_FUNCTIONS.md#playback-requesttransition) · [Playback_Advance](../30_FUNCTIONS/30_POLICY_PLAYBACK_FUNCTIONS.md#playback-advance) · [AudioStream_Prepare](../30_FUNCTIONS/40_AUDIO_STREAM_ASSET_FUNCTIONS.md#audiostream-prepare)

**관련 Contract:** [소유·수명](../50_CONTRACTS/00_CONTRACT_OVERVIEW.md#ownership) · [시간·최신성](../50_CONTRACTS/00_CONTRACT_OVERVIEW.md#timing)

**Provisional owner header 후보:** POLICY 계획 인터페이스 후보 — R6에서 확정. 실제 파일명·신규 Header 필요 여부는 정하지 않는다.

### Pseudo-C

```c
typedef struct
{
    VssPlanKeyType planKey;
    VssInputSignal signal;
    const VssCueDescriptor * cues;
    SemanticCount cueCount;
    VssRepeatRuleType repeatRule;
} VssPlanDescriptor;
```



<a id="vssselectionaction"></a>
## VssSelectionAction — scalar enum
**Category: Result**

선택 판단이며 하위 요청 수용/출력 시작과 구별한다.

**Owner:** POLICY · **Producer:** Select_Choose · **Consumer:** FLOW/PLAYBACK

| 값 | 의미 / 유효 조건 |
| --- | --- |
| `VSS_SELECTION_CHOOSE` | 새 검토 후보/계획 선택 |
| `VSS_SELECTION_KEEP` | 현재 유효 전체 실행 유지 |
| `VSS_SELECTION_REPLACE` | 정책상 높은 후보로 교체 판단. 이전 정리 전에 실행 권한은 없음 |
| `VSS_SELECTION_WAIT` | 후보/제한/이전 정리 등으로 실행 대기 |
| `VSS_SELECTION_RECOLLECT` | 관측 근거 부족으로 판단 재수집 |

**Lifetime / Mutability:** 현재 평가 자료의 유효 기간.

**관련 Function:** [Select_Choose](../30_FUNCTIONS/30_POLICY_PLAYBACK_FUNCTIONS.md#select-choose)

**관련 Contract:** [소유·수명](../50_CONTRACTS/00_CONTRACT_OVERVIEW.md#ownership) · [시간·최신성](../50_CONTRACTS/00_CONTRACT_OVERVIEW.md#timing)

**Provisional owner header 후보:** POLICY 판단 인터페이스 후보 — R6에서 확정.

```c
typedef enum
{
    VSS_SELECTION_CHOOSE,
    VSS_SELECTION_KEEP,
    VSS_SELECTION_REPLACE,
    VSS_SELECTION_WAIT,
    VSS_SELECTION_RECOLLECT
} VssSelectionAction;
```

<a id="vssselectiondecision"></a>
## VssSelectionDecision
**Category: Result**

정책 판단과 후보·전체 계획의 관계를 함께 반환한다. 같은 평가·producer·lifetime이므로 작은 묶음이 적절하다.

**Owner:** POLICY  
**Producer:** Select_Choose  
**Consumer:** FLOW → Playback_RequestTransition

### 사람이 읽는 Field 정의

| Field | 의미 / 필요한 이유 | Field 생성 주체 | 유효 조건 |
| --- | --- | --- | --- |
| `action` | choose/keep/replace/wait·재수집의 읽기 전용 판단. | POLICY | 평가 완료 뒤 |
| `candidate` | 선택/유지 판단이 연결된 원래 후보. 후보가 없는 WAIT/RECOLLECT에는 없음. | STORE 관측 → POLICY 연결 | 보호된 해당 평가 기간 |
| `plan` | 선택된 의미 전체 계획. 정의 없는 판단에는 없음. | POLICY | CHOOSE/REPLACE의 계획이 정의됐거나 KEEP의 현 계획에 연결된 때 |
| `basis` | 판단에 사용한 적용·시간·인과 근거. | FLOW → POLICY 그대로 연결 | 동일 평가 기간 |

### Lifetime / Mutability / 폐기

POLICY 생성 뒤 불변이다. candidate/plan이 없는 WAIT/RECOLLECT로 prepare를 만들지 않는다. 두 conditional 참조는 하나의 읽기 판단에만 한정하며 START/STOP/RECOVERY payload를 섞지 않는다. PLAYBACK은 채택 시 최신 조건을 재검사하고 필요한 불변 자료를 보호한다.

**관련 Function:** [Select_Choose](../30_FUNCTIONS/30_POLICY_PLAYBACK_FUNCTIONS.md#select-choose) · [Playback_RequestTransition](../30_FUNCTIONS/30_POLICY_PLAYBACK_FUNCTIONS.md#playback-requesttransition)

**관련 Contract:** [소유·수명](../50_CONTRACTS/00_CONTRACT_OVERVIEW.md#ownership) · [시간·최신성](../50_CONTRACTS/00_CONTRACT_OVERVIEW.md#timing)

**Provisional owner header 후보:** POLICY 판단 인터페이스 후보 — R6에서 확정. 실제 파일명·신규 Header 필요 여부는 정하지 않는다.

### Pseudo-C

```c
typedef struct
{
    VssSelectionAction action;
    const VssCandidateObservation * candidate;
    const VssPlanDescriptor * plan;
    const VssReadBasis * basis;
} VssSelectionDecision;
```



## INLINE 경계의 소비 규칙

- `VssPlaybackIntent`: POLICY의 VssSelectionDecision을 읽기 전용으로 연결하거나 현재 Session identity와 종료 사유 scalar를 전달한다. 별도 통합 Intent struct는 만들지 않는다. 종료 사유는 기존 선점/유효 CLEAR/고장/전체 계획 끝의 의미이며 새 Fault 분류가 아니다.
- `VssExecutionConditions`: candidate/read basis/대상 제한을 현재 평가 동안 빌린다. 자기 Session·PCM·장치 준비 상태는 각각 PLAYBACK/AUDIO STREAM이 자기 owner 경계에서 확인한다. FLOW가 최신 조건을 조립했다고 직접 실행 허가를 만들지 않는다.
- POLICY 순위는 기존 Class와 [잠정] 세부 순위를 유지한다. 같은 의미의 미시작 One-shot 비교는 원본 발생 순서·고정 identity를 쓰며 수신 순서를 쓰지 않는다. 낮은 후보/후보 없음만으로 유효한 현재 One-shot을 끝내지 않는다.

<a id="scalar-aliases"></a>
## 보조 scalar / 불투명 의미 표기

아래는 경계 identity·정책 값의 의미 alias다. 각각 독립 struct/새 ID 생성 API를 만드는 목록이 아니다. 모든 값의 폭/encoding·확정 Header는 미정이다. 실제 producer의 원래 문맥에서 보호하고 전달 뒤 불변으로 사용한다. 회수/무효화는 해당 주 Data의 lifetime을 따른다.

| 표기 | Producer / 의미 owner | 목적 / validity |
| --- | --- | --- |
| `VssOwnerKeyType` | 해당 근거 producer | 기존 FLOW/Module/Layer Boundary의 책임 식별. 새 계층 아님 |
| `VssFactWatermarkType` | 각 사실 적용 owner | 필수 인과 사실의 반영 완료 범위. 저장/물리 counter 방식 미정 |
| `VssCandidateKeyType` | STORE | mode와 함께 원래 occurrence 또는 Stateful key에 연결. identity 인코딩/별도 wrapper 강제하지 않음 |
| `VssUseLimitType` | 기존 중앙/요구사항 정책 | 원문 USE_LIMIT 조건. first-start age와 새 min/AND 정책을 만들지 않음 |
| `VssPlanKeyType` | POLICY | 불변 전체 정책/패턴 문맥. 단순 평가마다 새 plan identity를 만들지 않음 |
| `VssRepeatRuleType` | POLICY | 기존 패턴 반복 규칙. 횟수/무음/조건의 [잠정/TBD] 유지 |
| `VssPlanPositionType` | PLAYBACK | 해당 plan의 의미 cue/반복 진행 위치. 표현·cue당 Attempt 수 미정 |

```c
typedef SemanticOwner VssOwnerKeyType;
typedef SemanticRevision VssFactWatermarkType;
typedef SemanticIdentity VssCandidateKeyType;
typedef SemanticPolicyCondition VssUseLimitType;
typedef SemanticIdentity VssPlanKeyType;
typedef SemanticPolicyCondition VssRepeatRuleType;
typedef SemanticPosition VssPlanPositionType;
```

Owner header 후보는 해당 Data의 의미 owner 그룹을 따른다. R6에서 기존 Header 재사용/소유를 확인한다. 별도 공통 God Header를 만들지 않는다.

## 공통 TBD / 후속 범위

논리 필드의 실제 폭/숫자·alignment/packing/ABI·storage/복사·참조 수단과 Header filename은 미정이다. RTD/DMA/TCD·`.c/.h` Mapping·Task/Queue/Mutex·실제 C 구현은 작성하지 않았다. [pseudo-C 표기](00_DATA_OVERVIEW.md#pseudo-conventions)와 [기존 Implementation TBD](../90_BINDING/91_IMPLEMENTATION_TBD.md)를 함께 확인한다. 관련 계약은 기존 원문 링크이며 R4의 Cross Contract 원문 통합을 선행하지 않았다.
