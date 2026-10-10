# Playback Data — R3

> 2026-10-07 · 고정 Baseline / Execution Plan v1.1 · 논리 필드와 pseudo-C 상세

[Data Overview](00_DATA_OVERVIEW.md) · [R2 타입 판정](00_DATA_OVERVIEW.md#type-decisions)

<a id="playback"></a>
Occurrence는 STORE의 발생 이력, Session은 PLAYBACK의 전체 정책 실행, Attempt는 새 실제 출력 시도의 인과 문맥이다. Session은 cue 무음·반복 대기를 포함하며 cue당 Attempt 수/실제 표현은 이번에 고정하지 않는다. Context를 하나로 합치거나 active 포인터를 callback의 원래 귀속 대신 쓰지 않는다.

**타입 읽기:** `Category`는 역할이며 [선언 종류](00_DATA_OVERVIEW.md#pseudo-conventions)와 별개다. 아래 안내는 구조체 값, enum, 의미 alias, pseudo-scalar와 `const T *` 읽기 참조를 구분한다. key alias 참조의 대상은 해당 식별 의미이며 Context 구조체 참조와 구별한다. 값 필드 안의 내부 참조와 근거·미반영 fact의 보호는 각 절의 Lifetime 및 [소유·수명 계약](../50_CONTRACTS/10_OWNERSHIP_LIFETIME.md#borrow-and-protect)을 따른다.

**논리 Header 안내(R6):** `H-PB`의 Session/Attempt는 Private 원본이고 Fact/Observation은 불변 전달·파생 읽기 경계다. [선언 경계 후보](../70_C_INTERFACE/00_HEADER_OWNERSHIP_MAP.md#header-candidates) · [타입 대응](../70_C_INTERFACE/00_HEADER_OWNERSHIP_MAP.md#type-map)을 따른다. 실제 `.h` 파일명·타입 구현 여부는 확인되지 않았고 C 기본형·메모리 표현·ABI는 미정이다.

<a id="vssplaybackphase"></a>
## VssPlaybackPhase — scalar enum
**Category: State**

PLAYBACK의 전체 실행/종료 상태다. 실제 출력의 지식과 후속 권한은 아래 Context에서 별도로 보존한다.

**Owner:** PLAYBACK · **Producer:** Playback_RequestTransition/Playback_Advance · **Consumer:** PLAYBACK·읽기 관측 소비자

| 값 | 의미 / 유효 조건 |
| --- | --- |
| `VSS_PLAYBACK_PREPARING` | 의미 준비 중. 출력 허가 아님 |
| `VSS_PLAYBACK_START_IN_FLIGHT` | start 수용/실제 결과 추적 중. 정상 대기는 Fault 아님 |
| `VSS_PLAYBACK_ACTIVE` | 적법한 actual이 확인된 정상 전체 정책 진행 |
| `VSS_PLAYBACK_STOPPING` | 종료 의도·전체 후속 권한 차단/정리 |
| `VSS_PLAYBACK_ABORTING` | 실패/차단의 정리 진행 |
| `VSS_PLAYBACK_QUARANTINED` | 시작 결과 불명확 최종·새 시작 차단. 이전 정리 지속 |
| `VSS_PLAYBACK_RETIRED` | 전체 출력 종료/차단·후속 권한 차단·late 귀속 보호 뒤 owner 해제 |

**Lifetime / Mutability:** Session 수명. 늦은 수락/actual로 종료 phase를 되돌리지 않는다.

**관련 Function:** [Playback_RequestTransition](../30_FUNCTIONS/30_POLICY_PLAYBACK_FUNCTIONS.md#playback-requesttransition) · [Playback_Advance](../30_FUNCTIONS/30_POLICY_PLAYBACK_FUNCTIONS.md#playback-advance)

**관련 Contract:** [소유·수명](../50_CONTRACTS/00_CONTRACT_OVERVIEW.md#ownership) · [비동기 근거](../50_CONTRACTS/00_CONTRACT_OVERVIEW.md#async)

**논리 Header(R6):** `H-PB`.

```c
typedef enum
{
    VSS_PLAYBACK_PREPARING,
    VSS_PLAYBACK_START_IN_FLIGHT,
    VSS_PLAYBACK_ACTIVE,
    VSS_PLAYBACK_STOPPING,
    VSS_PLAYBACK_ABORTING,
    VSS_PLAYBACK_QUARANTINED,
    VSS_PLAYBACK_RETIRED
} VssPlaybackPhase;
```

<a id="vssstartknowledge"></a>
## VssStartKnowledge — scalar enum
**Category: State**

실제 시작 지식이다. PREPARING/요청 수락과 독립하여 무출력/불명확 최종을 구별한다.

**Owner:** PLAYBACK · **Producer:** Playback_Advance · **Consumer:** PLAYBACK/STORE·HEALTH 읽기

| 값 | 의미 / 유효 조건 |
| --- | --- |
| `VSS_START_NOT_ATTEMPTED` | 아직 실제 시작을 시도하지 않음 |
| `VSS_START_PENDING_OR_UNKNOWN` | 시도 진행 또는 근거 부족. timeout만으로 NO_START 아님 |
| `VSS_START_ACTUAL_CONFIRMED` | 적법한 실제 시작 확인 |
| `VSS_START_NO_START_CONFIRMED` | 충분한 과거 무출력·미래 하위/상위 차단 확인 |
| `VSS_START_UNCERTAIN_FINAL` | 시작 여부 불명확 최종. 정상 이력 부활 금지 |

**Lifetime / Mutability:** 원래 Attempt/Session 범위에서 보존. uncertain 최종과 물리 정리 완료는 별개다.

**관련 Function:** [Playback_Advance](../30_FUNCTIONS/30_POLICY_PLAYBACK_FUNCTIONS.md#playback-advance) · [Store_ApplyPlaybackFact](../30_FUNCTIONS/20_INPUT_STORE_FUNCTIONS.md#store-applyplaybackfact) · [Health_BuildStatus](../30_FUNCTIONS/60_HEALTH_RUNTIME_FUNCTIONS.md#health-buildstatus)

**관련 Contract:** [비동기 근거](../50_CONTRACTS/00_CONTRACT_OVERVIEW.md#async)

**논리 Header(R6):** `H-PB`.

```c
typedef enum
{
    VSS_START_NOT_ATTEMPTED,
    VSS_START_PENDING_OR_UNKNOWN,
    VSS_START_ACTUAL_CONFIRMED,
    VSS_START_NO_START_CONFIRMED,
    VSS_START_UNCERTAIN_FINAL
} VssStartKnowledge;
```

<a id="vsssessioncontext"></a>
## VssSessionContext
**Category: Context**

**타입 종류:** 구조체 (`struct`) — 이 절의 Pseudo-C 선언.

단일 출력 owner와 채택된 전체 정책·후속 start/cue/반복 권한을 보유한다. 상태 원본은 PLAYBACK 하나다.

**Owner:** PLAYBACK 단일 writer  
**Producer:** Playback_RequestTransition/Playback_Advance  
**Consumer:** PLAYBACK / FLOW·HEALTH 읽기 projection

### 사람이 읽는 Field 정의

| Field | 의미 / 필요한 이유 | Field 생성 주체 | 유효 조건 |
| --- | --- | --- | --- |
| `session` | 전체 계획의 실행 identity. occurrence와 같지 않다. | PLAYBACK | 새 Session 채택 뒤; retirement 후에도 필요한 추적 유지 |
| `candidate` | 원래 STORE 후보 연결. mode와 함께 안정된 key를 보존한다. | STORE → PLAYBACK 보호 | 채택 뒤 불변 |
| `mode` | One-shot/Stateful 정책 수명 분류. | POLICY/STORE → PLAYBACK | 채택 뒤 불변 |
| `plan` | 전체 정책의 보호된 불변 의미 참조. | POLICY → PLAYBACK 보호 | Session의 정상/정리 수명 |
| `position` | 현재 cue/반복의 의미 진행 위치. PCM offset resume가 아니다. | PLAYBACK | 현재 plan에서만 유효 |
| `phase` | 전체 실행/종료 의도. | PLAYBACK | 전이 반영 완료 뒤 |
| `startAllowed` | 아직 발행하지 않은 start 권한. | PLAYBACK | 최신 조건 검토 후; 종료/uncertain 시 차단 |
| `cueAllowed` | 후속 cue 발행 권한. | PLAYBACK | 정상 plan 진행만 허용 |
| `repeatAllowed` | 후속 반복 발행 권한. | PLAYBACK | 정상 plan 진행만 허용 |
| `currentAttempt` | 현재 시도의 연결. callback은 이 값을 뒤늦게 읽어 라벨링하지 않는다. | PLAYBACK | 현재 Attempt가 보호된 동안 |
| `firstStartMeta` | 첫 실제 시작의 원본 age/문맥. 새 Attempt·선점·복구로 갱신하지 않는다. | STORE 후보 → PLAYBACK 보호 | 첫 실제 시작 전 검토; Started 이후 기록·정상 후속 정책에서 구별 |
| `revision` | 반영 완료·읽기 변화 확인. | PLAYBACK | 전이 완료 뒤 |

### Lifetime / Mutability / 폐기

채택 때 하위 요청보다 먼저 보호한다. PLAYBACK만 phase/진행/권한을 변경한다. 종료 의도는 start/cue/반복 권한을 먼저 차단하며 새 winner로 취소하지 않는다. retirement는 충분한 전체 출력 종료·권한 차단·late 보호 뒤 가능하다. 통지 의무/STORE 이력/HAL callback 수명은 별도이며 retirement와 동시에 지우지 않는다.

**관련 Function:** [Playback_RequestTransition](../30_FUNCTIONS/30_POLICY_PLAYBACK_FUNCTIONS.md#playback-requesttransition) · [Playback_Advance](../30_FUNCTIONS/30_POLICY_PLAYBACK_FUNCTIONS.md#playback-advance) · [Health_BuildStatus](../30_FUNCTIONS/60_HEALTH_RUNTIME_FUNCTIONS.md#health-buildstatus)

**관련 Contract:** [소유·수명](../50_CONTRACTS/00_CONTRACT_OVERVIEW.md#ownership) · [비동기 근거](../50_CONTRACTS/00_CONTRACT_OVERVIEW.md#async) · [시간·최신성](../50_CONTRACTS/00_CONTRACT_OVERVIEW.md#timing)

**논리 Header(R6):** `H-PB` · Private 원본.

### Pseudo-C

```c
typedef struct
{
    VssSessionKeyType session;
    VssCandidateKeyType candidate;
    VssPlaybackMode mode;
    const VssPlanDescriptor * plan;
    VssPlanPositionType position;
    VssPlaybackPhase phase;
    SemanticBool startAllowed;
    SemanticBool cueAllowed;
    SemanticBool repeatAllowed;
    VssAttemptKeyType currentAttempt;
    VssInputMeta firstStartMeta;
    VssRevisionType revision;
} VssSessionContext;
```

**구성원 타입 → 정의 위치**

- 의미 alias: `session` → [VssSessionKeyType](#vsssessionkeytype); `candidate` → [VssCandidateKeyType](30_SELECTION_DATA.md#vsscandidatekeytype); `position` → [VssPlanPositionType](30_SELECTION_DATA.md#vssplanpositiontype); `currentAttempt` → [VssAttemptKeyType](#vssattemptkeytype); `revision` → [VssRevisionType](10_INPUT_DATA.md#vssrevisiontype).
- enum: `mode` → [VssPlaybackMode](30_SELECTION_DATA.md#vssplaybackmode); `phase` → [VssPlaybackPhase](#vssplaybackphase).
- 구조체 읽기 참조 (`const T *`): `plan` → [VssPlanDescriptor](30_SELECTION_DATA.md#vssplandescriptor).
- pseudo-scalar: `startAllowed`, `cueAllowed`, `repeatAllowed` → [SemanticBool](00_DATA_OVERVIEW.md#pseudo-conventions).
- 구조체 값: `firstStartMeta` → [VssInputMeta](10_INPUT_DATA.md#vssinputmeta).



<a id="vssattemptcontext"></a>
## VssAttemptContext
**Category: Context**

**타입 종류:** 구조체 (`struct`) — 이 절의 Pseudo-C 선언.

특정 실제 출력 시도의 준비·수락/장치 활성화·actual·종료 지식과 보호된 STORE 통지 의무를 추적한다. 새 Attempt identity는 실제 새 출력 시도마다 갱신한다.

**Owner:** PLAYBACK 단일 writer  
**Producer:** Playback_RequestTransition/Playback_Advance  
**Consumer:** PLAYBACK / AUDIO STREAM 요청 연결 / 읽기 관측

### 사람이 읽는 Field 정의

| Field | 의미 / 필요한 이유 | Field 생성 주체 | 유효 조건 |
| --- | --- | --- | --- |
| `attempt` | 이 실제 출력 시도 identity. 옛 callback 격리의 기준. | PLAYBACK | 시도 등록 뒤 불변 |
| `session` | 원래 전체 계획 identity. | PLAYBACK | 시도 등록 뒤 불변 |
| `position` | 원래 계획 구간과의 연결. cue와 Attempt 수를 고정하지 않는다. | PLAYBACK | 시도 등록 뒤 불변 |
| `preparation` | 원래 준비의 수용/진행/완료·실패. prepared는 actual이 아니다. | AUDIO STREAM 결과 → PLAYBACK | 원래 prepare 결과 반영 후 |
| `startRequest` | START 제출/수락+장치 활성화·부분 효력 단계. actual과 별개다. | AUDIO STREAM 정규화 결과 → PLAYBACK | 원래 START 단계가 반영된 뒤 |
| `startKnowledge` | 실제 시작/미시작/불명확 최종의 지식. | PLAYBACK 판정 | 충분 근거 반영 뒤 |
| `startEvidence` | 원래 actual 또는 no-start의 보호된 Backend 근거. 단순 acceptance를 저장하지 않는다. | AUDIO STREAM → PLAYBACK 보호 | 해당 근거가 있는 경우; 정상 부활용으로 재사용 금지 |
| `endEvidence` | 별도 범위의 출력 종료 근거. actual과 함께 공존할 수 있다. | AUDIO STREAM → PLAYBACK 보호 | 종료 근거 반영 후 |
| `unappliedFacts` | STORE 미반영한 개별 사실들의 보호된 typed 범위. 시작/최종 의무를 덮지 않는다. | PLAYBACK | 각 fact의 APPLIED/ALREADY_APPLIED 완료까지 |
| `unappliedFactCount` | 보호된 미반영 fact 수. 저장/할당·고정 용량을 정하지 않는다. | PLAYBACK | unappliedFacts의 유효 범위 |
| `retired` | 이 시도의 출력/권한 정리와 owner 사용권 종료를 확인했는가. | PLAYBACK | 충분한 종료 조건 뒤; callback record 회수와 다름 |

### Lifetime / Mutability / 폐기

시도 등록부터 결과·정리·통지 보호가 끝날 때까지 유지한다. Context가 끝나도 실제 callback/소비 중 압축 bytes의 참조 수명은 따로 확인한다. unappliedFacts는 모든 미반영 사실을 개별 참조할 수 있어야 하며 저장 방식/용량은 TBD다. 원래 lower evidence가 회수되기 전 필요한 의미 근거를 보호된 값으로 확보한다. 최종 뒤 late는 옛 정리/진단에만 쓴다.

**관련 Function:** [Playback_RequestTransition](../30_FUNCTIONS/30_POLICY_PLAYBACK_FUNCTIONS.md#playback-requesttransition) · [Playback_Advance](../30_FUNCTIONS/30_POLICY_PLAYBACK_FUNCTIONS.md#playback-advance) · [Store_ApplyPlaybackFact](../30_FUNCTIONS/20_INPUT_STORE_FUNCTIONS.md#store-applyplaybackfact)

**관련 Contract:** [소유·수명](../50_CONTRACTS/00_CONTRACT_OVERVIEW.md#ownership) · [비동기 근거](../50_CONTRACTS/00_CONTRACT_OVERVIEW.md#async)

**논리 Header(R6):** `H-PB` · Private 원본.

### Pseudo-C

```c
typedef struct
{
    VssAttemptKeyType attempt;
    VssSessionKeyType session;
    VssPlanPositionType position;
    VssAudioRequestStage preparation;
    VssAudioRequestStage startRequest;
    VssStartKnowledge startKnowledge;
    const VssAudioOutputEvidence * startEvidence;
    const VssAudioOutputEvidence * endEvidence;
    const VssPlaybackFact * unappliedFacts;
    SemanticCount unappliedFactCount;
    SemanticBool retired;
} VssAttemptContext;
```

**구성원 타입 → 정의 위치**

- 의미 alias: `attempt` → [VssAttemptKeyType](#vssattemptkeytype); `session` → [VssSessionKeyType](#vsssessionkeytype); `position` → [VssPlanPositionType](30_SELECTION_DATA.md#vssplanpositiontype).
- enum: `preparation`, `startRequest` → [VssAudioRequestStage](50_AUDIO_STREAM_DATA.md#vssaudiorequeststage); `startKnowledge` → [VssStartKnowledge](#vssstartknowledge).
- 구조체 읽기 참조 (`const T *`): `startEvidence`, `endEvidence` → [VssAudioOutputEvidence](50_AUDIO_STREAM_DATA.md#vssaudiooutputevidence); `unappliedFacts` → [VssPlaybackFact](#vssplaybackfact).
- pseudo-scalar: `unappliedFactCount` → [SemanticCount](00_DATA_OVERVIEW.md#pseudo-conventions); `retired` → [SemanticBool](00_DATA_OVERVIEW.md#pseudo-conventions).

최종 전 같은 Attempt의 적법한 늦은 actual은 Started/보고 사실만 적용하고 STOPPING/ABORTING을 유지한다. uncertain 최종 뒤 또는 retired 뒤 결과는 Pending/Started/Completed와 새 Session을 부활시키지 않는다.

<a id="vssplaybackfactkind"></a>
## VssPlaybackFactKind — scalar enum
**Category: Event**

STORE 발생 이력에 적용하는 PLAYBACK의 정규화된 사실이다. 물리 callback enum이 아니다.

**Owner:** PLAYBACK의 전체 판정 · **Producer:** Playback_Advance · **Consumer:** Store_ApplyPlaybackFact

| 값 | 의미 / 유효 조건 |
| --- | --- |
| `VSS_FACT_FIRST_ACTUAL_START` | 최초 적법한 actual 시작 사실 |
| `VSS_FACT_WHOLE_COMPLETED` | 전체 계획 정상 끝과 전체 종료/권한 차단 확인 |
| `VSS_FACT_WHOLE_INTERRUPTED` | Started 전체 실행의 선점/고장 중단과 종료 확인 |
| `VSS_FACT_NO_START_REEVALUATE` | 전체 무출력/차단 확인 후 원래 Pending/Expiry 재평가 가능. uncertain 최종 없어야 함 |
| `VSS_FACT_START_OUTCOME_UNCERTAIN` | 시작 여부 불명확 최종·재실행 금지 |

**Lifetime / Mutability:** 생성 뒤 불변. 한 cue/EOS를 WHOLE_COMPLETED로 만들지 않는다.

**관련 Function:** [Playback_Advance](../30_FUNCTIONS/30_POLICY_PLAYBACK_FUNCTIONS.md#playback-advance) · [Store_ApplyPlaybackFact](../30_FUNCTIONS/20_INPUT_STORE_FUNCTIONS.md#store-applyplaybackfact)

**관련 Contract:** [비동기 근거](../50_CONTRACTS/00_CONTRACT_OVERVIEW.md#async) · [소유·수명](../50_CONTRACTS/00_CONTRACT_OVERVIEW.md#ownership)

**논리 Header(R6):** `H-PB`.

```c
typedef enum
{
    VSS_FACT_FIRST_ACTUAL_START,
    VSS_FACT_WHOLE_COMPLETED,
    VSS_FACT_WHOLE_INTERRUPTED,
    VSS_FACT_NO_START_REEVALUATE,
    VSS_FACT_START_OUTCOME_UNCERTAIN
} VssPlaybackFactKind;
```

<a id="vssplaybackfact"></a>
## VssPlaybackFact
**Category: Event**

**타입 종류:** 구조체 (`struct`) — 이 절의 Pseudo-C 선언.

PLAYBACK이 충분한 근거로 판정한 원래 occurrence의 최초 시작/전체 최종 사실이다. STORE는 이를 중복에 안전하게 반영하며 raw 결과를 직접 판정하지 않는다.

**Owner:** PLAYBACK의 판정 사실 / 적용 이력 writer는 STORE  
**Producer:** Playback_Advance  
**Consumer:** Store_ApplyPlaybackFact

### 사람이 읽는 Field 정의

| Field | 의미 / 필요한 이유 | Field 생성 주체 | 유효 조건 |
| --- | --- | --- | --- |
| `fact` | 같은 사실의 재적용 식별. 중복 통지로 이력을 재변경하지 않는다. | PLAYBACK | 사실 생성 뒤 불변 |
| `occurrence` | 원래 수용한 One-shot 발생. 현재 occurrence로 재귀속 금지. | STORE key → PLAYBACK | 수용/Session 연결이 보호된 동안 |
| `session` | 전체 계획 판정 범위. | PLAYBACK | 사실의 원래 Session |
| `attempt` | 사실의 인과 시도. 전체 최종도 관련 시도를 잃지 않는다. | PLAYBACK | 원래 시도/종료 문맥 |
| `kind` | 실제 시작/전체 완료·중단/미시작 재평가/uncertain 최종. | PLAYBACK | 충분한 원래 근거·권한 대조 뒤 |
| `occurredAt` | 최초 시작은 실제 출력 시각, 전체 final은 종료/최종 판정 근거 시각이다. uncertain은 실제 출력 시각을 안다는 뜻이 아니다. | 원래 관측 → PLAYBACK 보호 | 시간 근거가 충분한 사실 범위 |
| `basisRevision` | 판정이 반영된 PLAYBACK 문맥과 연결한다. | PLAYBACK | 사실 판정 완료 뒤 |

### Lifetime / Mutability / 폐기

생성 뒤 불변이며 STORE APPLIED/ALREADY_APPLIED까지 PLAYBACK이 보호한다. STARTED와 최종 통지를 마지막 결과 하나로 덮지 않는다. STORE는 필요한 정규화 fact를 ledger 수명 동안 보존하고 raw HAL record 전체를 replay 수명까지 강제로 잡지 않는다. 미반영/귀속 부족이면 완료를 합성하지 않는다. Stateful 현재 ACTIVE/CLEAR·품질은 이 Event로 재작성하지 않는다.

**관련 Function:** [Playback_Advance](../30_FUNCTIONS/30_POLICY_PLAYBACK_FUNCTIONS.md#playback-advance) · [Store_ApplyPlaybackFact](../30_FUNCTIONS/20_INPUT_STORE_FUNCTIONS.md#store-applyplaybackfact)

**관련 Contract:** [소유·수명](../50_CONTRACTS/00_CONTRACT_OVERVIEW.md#ownership) · [비동기 근거](../50_CONTRACTS/00_CONTRACT_OVERVIEW.md#async) · [시간·최신성](../50_CONTRACTS/00_CONTRACT_OVERVIEW.md#timing)

**논리 Header(R6):** `H-PB`.

### Pseudo-C

```c
typedef struct
{
    VssFactKeyType fact;
    VssOccurrenceKeyType occurrence;
    VssSessionKeyType session;
    VssAttemptKeyType attempt;
    VssPlaybackFactKind kind;
    VssFactTime occurredAt;
    VssRevisionType basisRevision;
} VssPlaybackFact;
```

**구성원 타입 → 정의 위치**

- 의미 alias: `fact` → [VssFactKeyType](#vssfactkeytype); `occurrence` → [VssOccurrenceKeyType](20_STORE_DATA.md#vssoccurrencekeytype); `session` → [VssSessionKeyType](#vsssessionkeytype); `attempt` → [VssAttemptKeyType](#vssattemptkeytype); `basisRevision` → [VssRevisionType](10_INPUT_DATA.md#vssrevisiontype).
- enum: `kind` → [VssPlaybackFactKind](#vssplaybackfactkind).
- 구조체 값: `occurredAt` → [VssFactTime](10_INPUT_DATA.md#vssfacttime).



<a id="vssplaybackobservation"></a>
## VssPlaybackObservation
**Category: Snapshot**

**타입 종류:** 구조체 (`struct`) — 이 절의 Pseudo-C 선언.

VssPlaybackObservations와 VssPlaybackProgress의 공통 읽기 의미를 합친다. 기한 만료/보고에 필요한 최소 현재 출력 관측이며 SessionContext의 두 번째 writer가 아니다.

**Owner:** PLAYBACK 파생 읽기  
**Producer:** Playback_Advance 반영 후 기존 읽기 경계  
**Consumer:** FLOW → Store_AdvanceDeadlines / POLICY / HEALTH

### 사람이 읽는 Field 정의

| Field | 의미 / 필요한 이유 | Field 생성 주체 | 유효 조건 |
| --- | --- | --- | --- |
| `session` | 현재 또는 추적 대상 Session. 활성 문맥 없으면 없음. | PLAYBACK | 해당 읽기 기간 |
| `attempt` | 관련 원래 시도. 시작/만료 경합을 같은 대상으로 확인한다. | PLAYBACK | 해당 읽기 기간 |
| `candidate` | 연결된 STORE 후보. | PLAYBACK | 해당 읽기 기간 |
| `phase` | 현재 실행/종료/QUARANTINED 의미. 문맥 없음은 session 없음으로 구별한다. | PLAYBACK | session이 있을 때만 의미 유효 |
| `startKnowledge` | actual/no-start/in-flight/uncertain 최종 구별. | PLAYBACK | 사실 반영 완료 뒤 |
| `notificationPending` | STORE에 아직 미반영한 보호된 사실이 있는가. | PLAYBACK | 읽기 시점 |
| `stamp` | 출력 관측의 반영 완료 경계. | PLAYBACK | 동일 읽기 기간 |

### Lifetime / Mutability / 폐기

평가 중 읽기 전용이며 수집 중 관련 변화/미반영 사실이면 만료 확정을 보류한다. 원본 쓰기는 PLAYBACK Core만 수행한다. STATEFUL/전체 현재 출력 관측이지만 One-shot ledger 적용과는 별개다.

**관련 Function:** [Playback_Advance](../30_FUNCTIONS/30_POLICY_PLAYBACK_FUNCTIONS.md#playback-advance) · [Store_AdvanceDeadlines](../30_FUNCTIONS/20_INPUT_STORE_FUNCTIONS.md#store-advancedeadlines) · [Select_Choose](../30_FUNCTIONS/30_POLICY_PLAYBACK_FUNCTIONS.md#select-choose) · [Health_BuildStatus](../30_FUNCTIONS/60_HEALTH_RUNTIME_FUNCTIONS.md#health-buildstatus)

**관련 Contract:** [소유·수명](../50_CONTRACTS/00_CONTRACT_OVERVIEW.md#ownership) · [비동기 근거](../50_CONTRACTS/00_CONTRACT_OVERVIEW.md#async)

**논리 Header(R6):** `H-PB`.

### Pseudo-C

```c
typedef struct
{
    const VssSessionKeyType * session;
    const VssAttemptKeyType * attempt;
    const VssCandidateKeyType * candidate;
    VssPlaybackPhase phase;
    VssStartKnowledge startKnowledge;
    SemanticBool notificationPending;
    VssOwnerStamp stamp;
} VssPlaybackObservation;
```

**구성원 타입 → 정의 위치**

- 의미 alias 읽기 참조 (`const T *`): `session` → [VssSessionKeyType](#vsssessionkeytype); `attempt` → [VssAttemptKeyType](#vssattemptkeytype); `candidate` → [VssCandidateKeyType](30_SELECTION_DATA.md#vsscandidatekeytype).
- enum: `phase` → [VssPlaybackPhase](#vssplaybackphase); `startKnowledge` → [VssStartKnowledge](#vssstartknowledge).
- pseudo-scalar: `notificationPending` → [SemanticBool](00_DATA_OVERVIEW.md#pseudo-conventions).
- 구조체 값: `stamp` → [VssOwnerStamp](30_SELECTION_DATA.md#vssownerstamp).



<a id="vssrequestdisposition"></a>
## VssRequestDisposition — scalar enum
**Category: Result**

의도 수용/기존 진행 유지/대기/거부만 의미하는 scalar다. 실제 장치/출력 결과를 이 enum에 합치지 않는다.

**Owner:** 해당 요구를 적용하는 callee owner · **Producer:** Playback_RequestTransition 또는 AUDIO STREAM 준비/제어 수용 · **Consumer:** caller

| 값 | 의미 / 유효 조건 |
| --- | --- |
| `VSS_REQUEST_ADOPTED` | 현재 owner가 요구를 수용/반영함 |
| `VSS_REQUEST_KEPT` | 원래 동일 진행/차단 상태를 유지함 |
| `VSS_REQUEST_WAIT` | 최신 조건/보호 자원/이전 정리 때문에 대기·재평가 필요 |
| `VSS_REQUEST_REJECTED` | 새 의도 근거가 부적합하여 거부함 |

**Lifetime / Mutability:** 호출별 scalar. 수용은 prepared/accepted+장치 활성화/actual/안전 반환의 증명이 아니다.

**관련 Function:** [Playback_RequestTransition](../30_FUNCTIONS/30_POLICY_PLAYBACK_FUNCTIONS.md#playback-requesttransition) · [AudioStream_Prepare](../30_FUNCTIONS/40_AUDIO_STREAM_ASSET_FUNCTIONS.md#audiostream-prepare) · [AudioStream_RequestControl](../30_FUNCTIONS/40_AUDIO_STREAM_ASSET_FUNCTIONS.md#audiostream-requestcontrol)

**관련 Contract:** [소유·수명](../50_CONTRACTS/00_CONTRACT_OVERVIEW.md#ownership) · [비동기 근거](../50_CONTRACTS/00_CONTRACT_OVERVIEW.md#async)

**논리 Header(R6):** `H-PB`.

```c
typedef enum
{
    VSS_REQUEST_ADOPTED,
    VSS_REQUEST_KEPT,
    VSS_REQUEST_WAIT,
    VSS_REQUEST_REJECTED
} VssRequestDisposition;
```

## VssPlaybackProgress — SPLIT 결과 전달

별도 Progress struct는 만들지 않는다. 반환 의미는 `VssPlaybackObservation` 읽기 관측, `VssFactApplication` scalar, 아직 미반영한 `VssPlaybackFact`의 보호된 의무, 발생 주체가 보존된 `VssDiagnosticEvidence`로 나눈다. 관측은 현재 평가 수명, 미반영 사실은 STORE 완료까지, 진단은 HEALTH 반영/추적까지 각각 보호한다.

보고의 PLAYING은 적법한 actual이 확인된 전체 Session 기준이며 cue 무음·반복 대기·시작 후 STOPPING도 포함한다. `QUARANTINED`는 기존 `PLAYBACK_STATE_FAILURE`와 `FAULT + UNAVAILABLE` 연결을 유지한다. 기한/권한 밖 actual은 물리 사실·위반 진단으로 보호하고 적법한 최초 Started로 합성하지 않는다.

<a id="scalar-aliases"></a>
## 보조 scalar / 불투명 의미 표기

아래는 경계 identity·정책 값의 의미 alias다. 각각 독립 struct/새 ID 생성 API를 만드는 목록이 아니다. 모든 값의 폭/encoding·확정 Header는 미정이다. 실제 producer의 원래 문맥에서 보호하고 전달 뒤 불변으로 사용한다. 회수/무효화는 해당 주 Data의 lifetime을 따른다.

| 표기 | Producer / 의미 owner | 목적 / validity |
| --- | --- | --- |
| <a id="vsssessionkeytype"></a>`VssSessionKeyType` | PLAYBACK | 전체 계획 실행. occurrence와 다르고 cue 무음/반복 대기 포함 |
| <a id="vssattemptkeytype"></a>`VssAttemptKeyType` | PLAYBACK | 새 실제 출력 시도별 갱신. 다음 시도로 옛 사실 재귀속 금지 |
| <a id="vssfactkeytype"></a>`VssFactKeyType` | PLAYBACK | 정규화 fact의 중복 적용 key. 실제 생성 방식 미정 |

```c
typedef SemanticIdentity VssSessionKeyType;
typedef SemanticIdentity VssAttemptKeyType;
typedef SemanticIdentity VssFactKeyType;
```

세 key는 모두 `SemanticIdentity`의 의미 alias이며 구조체가 아니다. R6 논리 선언 경계는 `H-PB`이며 [타입 대응](../70_C_INTERFACE/00_HEADER_OWNERSHIP_MAP.md#type-map)을 따른다. occurrence/session/attempt/fact의 구분과 위 validity를 보존하고 실제 C typedef·Header 구현은 미정으로 둔다.

## 공통 TBD / 후속 범위

논리 필드의 실제 폭/숫자·alignment/packing/ABI·storage/복사·참조 수단과 Header filename은 미정이다. RTD/DMA/TCD·`.c/.h` Mapping·Task/Queue/Mutex·실제 C 구현은 작성하지 않았다. [pseudo-C 표기](00_DATA_OVERVIEW.md#pseudo-conventions)와 [기존 Implementation TBD](../90_BINDING/91_IMPLEMENTATION_TBD.md)를 함께 확인한다. 관련 계약은 기존 원문 링크이며 R4의 Cross Contract 원문 통합을 선행하지 않았다.
