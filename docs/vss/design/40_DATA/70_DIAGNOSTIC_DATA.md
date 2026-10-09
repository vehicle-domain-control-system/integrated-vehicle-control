# Diagnostic / Recovery / Status Data — R3

> 2026-10-07 · 고정 Baseline / Execution Plan v1.1 · 논리 필드와 pseudo-C 상세

[Data Overview](00_DATA_OVERVIEW.md) · [R2 타입 판정](00_DATA_OVERVIEW.md#type-decisions)

<a id="health"></a>
HEALTH의 진단 책임은 현재/최근 Fault·대상 제한·복구 허용/해제를 소유한다. Backend가 실제 수행하고 별도 효과 검증을 제공한다. HEALTH의 보고 책임은 반영 완료된 읽기 관측에서 외부 VSS_STATUS만 파생한다. Fault 원본과 Snapshot은 다른 수명/쓰기 계약이다.

<a id="vssfaultkind"></a>
## VssFaultKind — scalar enum
**Category: State**

기존 Fault taxonomy 5개만 유지한다. 발견 원인/단계는 DiagnosticEvidence이며 새 Fault로 승격하지 않는다.

**Owner:** HEALTH · **Producer:** Health_Evaluate · **Consumer:** FLOW/보고 소비자

| 값 | 의미 / 유효 조건 |
| --- | --- |
| `SOUND_ASSET_UNAVAILABLE` | 필요한 저장 음원 사용 불가 |
| `PLAYBACK_START_FAILURE` | 재생 시작 실패 |
| `AUDIO_OUTPUT_FAILURE` | 공통 Audio Output Path 이상 |
| `PLAYBACK_STATE_FAILURE` | 출력 owner/상태 정합성 이상·시작 결과 불명확 최종 포함 |
| `INITIALIZATION_FAILURE` | 확인된 초기화 실패. startup 미평가와 다름 |

**Lifetime / Mutability:** 해당 대상 Fault/최근 기록 수명. exact cause 매핑은 기존 TBD.

**관련 Function:** [Health_Evaluate](../30_FUNCTIONS/60_HEALTH_RUNTIME_FUNCTIONS.md#health-evaluate) · [Health_BuildStatus](../30_FUNCTIONS/60_HEALTH_RUNTIME_FUNCTIONS.md#health-buildstatus)

**관련 Contract:** [고장·복구](../50_CONTRACTS/00_CONTRACT_OVERVIEW.md#fault)

**Provisional owner header 후보:** HEALTH 진단 의미 후보 — R6에서 확정.

```c
typedef enum
{
    SOUND_ASSET_UNAVAILABLE,
    PLAYBACK_START_FAILURE,
    AUDIO_OUTPUT_FAILURE,
    PLAYBACK_STATE_FAILURE,
    INITIALIZATION_FAILURE
} VssFaultKind;
```

<a id="vssdiagnosticevidence"></a>
## VssDiagnosticEvidence
**Category: Evidence**

오류를 처음 발견한 주체·단계·원인·대상/구성·원래 사실 시각을 보존한다. Fault 판정 전에도 전달 가능한 근거이며 실행 owner를 바꾸지 않는다.

**Owner:** 원래 발견 주체의 불변 사실. Fault 분류/제한 writer는 HEALTH다.  
**Producer:** INPUT/ASSET/PLAYBACK/AUDIO STREAM/AUDIO TX/AUDIO CONTROL/HAL의 해당 실패 경계  
**Consumer:** FLOW → Health_Evaluate

### 사람이 읽는 Field 정의

| Field | 의미 / 필요한 이유 | Field 생성 주체 | 유효 조건 |
| --- | --- | --- | --- |
| `producer` | 처음 발견/근거 생성한 주체. FLOW로 대체하지 않는다. | 원래 producer | 진단 생성 뒤 불변 |
| `stage` | lookup/metadata/bounds/read·decode·start·전송/정리·상태/초기화 등 기존 발견 단계. | 원래 producer | 실제 발견 단계에만 |
| `cause` | 확인한 원인. exact cause→Fault/RecoveryAction은 기존 TBD. | 원래 producer | 확인 범위만 |
| `target` | 오류 영향의 원래 Asset/출력 경로/상태 대상 identity. | 원래 producer | 대상 귀속 확인 뒤 |
| `configuration` | 오류가 발생한 원래 구성/image 문맥. | 원래 producer | 대상과 함께 |
| `operation` | 원래 하위 작업이 있는 진단의 인과 연결. startup/입력 진단에는 없을 수 있음. | 원래 producer | 해당 operation이 확인된 경우만 |
| `occurredAt` | 원래 발견/효력 시각·구간. | 원래 producer | 시간 근거 부족은 continuity로 드러냄 |

### Lifetime / Mutability / 폐기

생성 뒤 불변으로 HEALTH 반영/보호 추적까지 유지한다. ASSET 발견 read 실패와 AUDIO STREAM decode 실패를 바꾸지 않는다. 시간/귀속/관측 손실도 범위가 드러나는 진단으로 보존하며 성공을 합성하지 않는다. 입력 품질 상실/보류 상태가 자동 출력 Fault라는 뜻은 아니다.

**관련 Function:** [Input_Process](../30_FUNCTIONS/20_INPUT_STORE_FUNCTIONS.md#input-process) · [Asset_Read](../30_FUNCTIONS/40_AUDIO_STREAM_ASSET_FUNCTIONS.md#asset-read) · [Playback_Advance](../30_FUNCTIONS/30_POLICY_PLAYBACK_FUNCTIONS.md#playback-advance) · [AudioStream_Advance](../30_FUNCTIONS/40_AUDIO_STREAM_ASSET_FUNCTIONS.md#audiostream-advance) · [AudioTx_Advance](../30_FUNCTIONS/50_DRIVER_HAL_FUNCTIONS.md#audiotx-advance) · [AudioControl_Service](../30_FUNCTIONS/50_DRIVER_HAL_FUNCTIONS.md#audiocontrol-service) · [AudioHAL_Callback](../30_FUNCTIONS/50_DRIVER_HAL_FUNCTIONS.md#audiohal-callback) · [Health_Evaluate](../30_FUNCTIONS/60_HEALTH_RUNTIME_FUNCTIONS.md#health-evaluate)

**관련 Contract:** [고장·복구](../50_CONTRACTS/00_CONTRACT_OVERVIEW.md#fault) · [비동기 근거](../50_CONTRACTS/00_CONTRACT_OVERVIEW.md#async) · [시간·최신성](../50_CONTRACTS/00_CONTRACT_OVERVIEW.md#timing)

**Provisional owner header 후보:** 각 발견 owner의 진단 Evidence 인터페이스 후보 — R6에서 확정. 실제 파일명·신규 Header 필요 여부는 정하지 않는다.

### Pseudo-C

```c
typedef struct
{
    VssOwnerKeyType producer;
    VssDiagnosticStageType stage;
    VssCauseType cause;
    VssTargetKeyType target;
    VssConfigurationKeyType configuration;
    const VssOperationKeyType * operation;
    VssFactTime occurredAt;
} VssDiagnosticEvidence;
```



<a id="vssfaultrecordstate"></a>
## VssFaultRecordState — scalar enum
**Category: State**

현재 제한과 최근 기록을 구별한다. clear는 모든 진단 기록 삭제가 아니다.

**Owner:** HEALTH · **Producer:** Health_Evaluate · **Consumer:** FLOW/Health_BuildStatus

| 값 | 의미 / 유효 조건 |
| --- | --- |
| `VSS_FAULT_CURRENT` | 현재 대상에 활성인 Fault/제한 |
| `VSS_FAULT_RECENT` | 해당 현재 제한 해제 뒤 보존하는 최근 기록 |

**Lifetime / Mutability:** 현재는 해제 근거까지, 최근은 기존 보존 정책까지. 보존 기간 숫자는 TBD.

**관련 Function:** [Health_Evaluate](../30_FUNCTIONS/60_HEALTH_RUNTIME_FUNCTIONS.md#health-evaluate) · [Health_BuildStatus](../30_FUNCTIONS/60_HEALTH_RUNTIME_FUNCTIONS.md#health-buildstatus)

**관련 Contract:** [고장·복구](../50_CONTRACTS/00_CONTRACT_OVERVIEW.md#fault)

**Provisional owner header 후보:** HEALTH 내부 Fault 후보 — R6에서 확정.

```c
typedef enum
{
    VSS_FAULT_CURRENT,
    VSS_FAULT_RECENT
} VssFaultRecordState;
```

<a id="vssfaultstate"></a>
## VssFaultState
**Category: State**

현재 대상의 Fault instance와 제한·원래 진단을 보존한다. 새 현재 Fault가 옛 복구 성공으로 지워지지 않게 한다.

**Owner:** HEALTH 단일 writer  
**Producer:** Health_Evaluate  
**Consumer:** FLOW 실행 제한 / Health_BuildStatus

### 사람이 읽는 Field 정의

| Field | 의미 / 필요한 이유 | Field 생성 주체 | 유효 조건 |
| --- | --- | --- | --- |
| `instance` | 현재/최근 해당 Fault 발생의 identity. 같은 kind여도 새 발생과 구별한다. | HEALTH | 해당 Fault 생성 뒤 |
| `kind` | 기존 5종 분류. | HEALTH | 기존 근거 매핑 판단 후 |
| `target` | 제한/해제의 원래 대상. | 원래 진단 → HEALTH | instance 수명 |
| `configuration` | 해당 대상의 오류 구성 문맥. | 원래 진단 → HEALTH | instance 수명 |
| `recordState` | 현재 제한/최근 이력 구별. | HEALTH | 현재 평가 반영 뒤 |
| `diagnosis` | 보호된 정규화 원래 원인/단계·시각. | 원래 producer → HEALTH 보호 | 진단/최근 기록이 필요할 때까지 |
| `restriction` | 현재 대상의 제한 의미. 전체 availability/reduction 정책을 이번에 정하지 않는다. | HEALTH 기존 정책 | 현재 대상에서만 |
| `revision` | 해당 Fault instance/대상 판단의 변경 표지. | HEALTH | 제한/해제 변경 반영 뒤 |

### Lifetime / Mutability / 폐기

HEALTH만 current/recent·제한을 변경한다. 해제는 같은 현재 대상의 실제 복구 수행 뒤 별도 효과 검증이 확인됐을 때만 가능하다. 다른 현재 Fault와 최근 기록은 보존한다. diagnosis는 normalized 불변 값을 보호하고 transient 반환 주소를 장기 저장하지 않는다. exact cause·복구/해제 정책·최근 보존 기간은 기존 TBD다.

**관련 Function:** [Health_Evaluate](../30_FUNCTIONS/60_HEALTH_RUNTIME_FUNCTIONS.md#health-evaluate) · [Flow_Process](../30_FUNCTIONS/10_FLOW_FUNCTIONS.md#flow-process) · [Health_BuildStatus](../30_FUNCTIONS/60_HEALTH_RUNTIME_FUNCTIONS.md#health-buildstatus)

**관련 Contract:** [고장·복구](../50_CONTRACTS/00_CONTRACT_OVERVIEW.md#fault) · [소유·수명](../50_CONTRACTS/00_CONTRACT_OVERVIEW.md#ownership)

**Provisional owner header 후보:** HEALTH 내부 Fault 상태 후보 — R6에서 확정. 실제 파일명·신규 Header 필요 여부는 정하지 않는다.

### Pseudo-C

```c
typedef struct
{
    VssFaultInstanceKeyType instance;
    VssFaultKind kind;
    VssTargetKeyType target;
    VssConfigurationKeyType configuration;
    VssFaultRecordState recordState;
    const VssDiagnosticEvidence * diagnosis;
    VssRestrictionType restriction;
    VssRevisionType revision;
} VssFaultState;
```



<a id="vssrecoverypermission"></a>
## VssRecoveryPermission
**Category: Command**

HEALTH가 현재 대상/범위에 허용한 제한된 복구 의미다. FLOW가 이전 출력 retirement·자원 안전 종료를 확인해 Backend에 이 허용을 연결한다. 허용이 실제 수행은 아니다.

**Owner:** HEALTH의 허용 writer. FLOW는 요구 연결만 한다.  
**Producer:** Health_Evaluate → Flow_Process  
**Consumer:** AudioStream_RequestControl → 실제 Backend 수행 경계

### 사람이 읽는 Field 정의

| Field | 의미 / 필요한 이유 | Field 생성 주체 | 유효 조건 |
| --- | --- | --- | --- |
| `permission` | 허용/수행/별도 검증을 같은 복구 문맥으로 연결하는 key. | HEALTH | 현재 대상 허용 생성 뒤 |
| `faultInstance` | 허용한 현재 Fault instance. | HEALTH | 해당 instance가 현재인 동안 |
| `faultRevision` | 허용 대상/제한 판단의 적용 revision. 다른 새 대상과 옛 허용을 구별한다. | HEALTH | 해당 대상 허용이 유효한 범위 |
| `target` | 복구할 원래 대상. | HEALTH | 허용된 대상만 |
| `configuration` | 허용이 연결된 현재 구성. | HEALTH | 같은 구성/대상에만 |
| `action` | 허용된 기존 Recovery Action/범위. exact cause 매핑·새 retry matrix는 TBD. | HEALTH 기존 정책 | 확인된 허용 범위만 |

### Lifetime / Mutability / 폐기

생성 뒤 전달 의미는 불변이고 허용 철회·새 대상/Fault·구성 변화로 무효화한다. FLOW/Backend가 현재 대상/안전 정리 조건을 재확인해야 한다. 이전 전체 출력 retirement·provider/PCM 안전 종료 전 수행하지 않는다. 작업 수용은 이 허용의 소비일 뿐 효과 검증/HEALTH clear가 아니다.

**관련 Function:** [Health_Evaluate](../30_FUNCTIONS/60_HEALTH_RUNTIME_FUNCTIONS.md#health-evaluate) · [Flow_Process](../30_FUNCTIONS/10_FLOW_FUNCTIONS.md#flow-process) · [AudioStream_RequestControl](../30_FUNCTIONS/40_AUDIO_STREAM_ASSET_FUNCTIONS.md#audiostream-requestcontrol) · [AudioControl_Service](../30_FUNCTIONS/50_DRIVER_HAL_FUNCTIONS.md#audiocontrol-service)

**관련 Contract:** [고장·복구](../50_CONTRACTS/00_CONTRACT_OVERVIEW.md#fault) · [소유·수명](../50_CONTRACTS/00_CONTRACT_OVERVIEW.md#ownership)

**Provisional owner header 후보:** HEALTH 복구 허용 인터페이스 후보 — R6에서 확정. 실제 파일명·신규 Header 필요 여부는 정하지 않는다.

### Pseudo-C

```c
typedef struct
{
    VssRecoveryKeyType permission;
    VssFaultInstanceKeyType faultInstance;
    VssRevisionType faultRevision;
    VssTargetKeyType target;
    VssConfigurationKeyType configuration;
    VssRecoveryActionType action;
} VssRecoveryPermission;
```



<a id="vssrecoveryeffect"></a>
## VssRecoveryEffect — scalar enum
**Category: Evidence**

실제 수행 결과와 별도 효과 검증 값에 쓰는 작은 상태다. 둘을 같은 Evidence로 합치지 않는다.

**Owner:** 실제 Backend 수행/검증 주체 · **Producer:** AUDIO STREAM/AUDIO CONTROL 실제 처리 · **Consumer:** FLOW/HEALTH

| 값 | 의미 / 유효 조건 |
| --- | --- |
| `VSS_RECOVERY_SUCCEEDED` | 이 record가 정의한 수행 또는 별도 검증을 실제 확인 |
| `VSS_RECOVERY_FAILED` | 수행 또는 별도 검증의 해당 단계 실패 |
| `VSS_RECOVERY_UNCONFIRMED` | 단계 효과 미확인/근거 부족 |

**Lifetime / Mutability:** 각 해당 Evidence의 단계에 한정. receipt/return 성공으로 합성 금지.

**관련 Function:** [AudioStream_Advance](../30_FUNCTIONS/40_AUDIO_STREAM_ASSET_FUNCTIONS.md#audiostream-advance) · [AudioControl_Service](../30_FUNCTIONS/50_DRIVER_HAL_FUNCTIONS.md#audiocontrol-service) · [Health_Evaluate](../30_FUNCTIONS/60_HEALTH_RUNTIME_FUNCTIONS.md#health-evaluate)

**관련 Contract:** [고장·복구](../50_CONTRACTS/00_CONTRACT_OVERVIEW.md#fault)

**Provisional owner header 후보:** Backend 복구 Evidence 후보 — R6에서 확정.

```c
typedef enum
{
    VSS_RECOVERY_SUCCEEDED,
    VSS_RECOVERY_FAILED,
    VSS_RECOVERY_UNCONFIRMED
} VssRecoveryEffect;
```

<a id="vssrecoveryperformedevidence"></a>
## VssRecoveryPerformedEvidence
**Category: Evidence**

허용된 실제 복구 동작을 수행했는지의 사실이다. re-init API 성공만으로 정상/clear를 선언하지 않는다.

**Owner:** 실제 Backend 수행 주체  
**Producer:** AudioStream_Advance / AudioControl_Service의 실제 수행 근거  
**Consumer:** FLOW → Health_Evaluate

### 사람이 읽는 Field 정의

| Field | 의미 / 필요한 이유 | Field 생성 주체 | 유효 조건 |
| --- | --- | --- | --- |
| `permission` | 원래 허용 문맥. | 수용 문맥 | 원래 수행 수명 |
| `target` | 실제로 수행한 대상. | Backend | 같은 허용 대상 |
| `configuration` | 실제 수행 구성. | Backend | 같은 허용/수행 문맥 |
| `operation` | 실제 수행 동작과 별도 검증을 연결할 인과 key. | Backend | 수행 요청 전에 보호 |
| `effect` | 실제 수행 확인/실패/미확인. | Backend 실제 확인 | 실제 수행 근거 후 |
| `occurredAt` | 실제 수행의 시각/구간. | 실제 수행 주체 | 원래 시간 근거 |

### Lifetime / Mutability / 폐기

생성 뒤 불변이고 HEALTH 적용/추적까지 보존한다. 수행 성공만 있고 별도 검증이 없으면 현재 제한은 유지한다. 옛 target/config의 성공을 현재 것으로 재라벨링하지 않는다.

**관련 Function:** [AudioStream_Advance](../30_FUNCTIONS/40_AUDIO_STREAM_ASSET_FUNCTIONS.md#audiostream-advance) · [AudioControl_Service](../30_FUNCTIONS/50_DRIVER_HAL_FUNCTIONS.md#audiocontrol-service) · [Flow_Process](../30_FUNCTIONS/10_FLOW_FUNCTIONS.md#flow-process) · [Health_Evaluate](../30_FUNCTIONS/60_HEALTH_RUNTIME_FUNCTIONS.md#health-evaluate)

**관련 Contract:** [고장·복구](../50_CONTRACTS/00_CONTRACT_OVERVIEW.md#fault) · [비동기 근거](../50_CONTRACTS/00_CONTRACT_OVERVIEW.md#async)

**Provisional owner header 후보:** Backend 수행 Evidence 인터페이스 후보 — R6에서 확정. 실제 파일명·신규 Header 필요 여부는 정하지 않는다.

### Pseudo-C

```c
typedef struct
{
    VssRecoveryKeyType permission;
    VssTargetKeyType target;
    VssConfigurationKeyType configuration;
    VssOperationKeyType operation;
    VssRecoveryEffect effect;
    VssFactTime occurredAt;
} VssRecoveryPerformedEvidence;
```



<a id="vssrecoveryverificationevidence"></a>
## VssRecoveryVerificationEvidence
**Category: Evidence**

실제 수행 뒤 효과를 별도로 검증한 근거다. 수행 결과와 producer 시점/충분조건이 달라 분리한다.

**Owner:** 실제 Backend 효과 검증 주체  
**Producer:** AudioStream_Advance / AudioControl_Service의 별도 검증  
**Consumer:** FLOW → Health_Evaluate

### 사람이 읽는 Field 정의

| Field | 의미 / 필요한 이유 | Field 생성 주체 | 유효 조건 |
| --- | --- | --- | --- |
| `permission` | 검증이 연결된 원래 허용. | 보호된 수행 문맥 | 원래 허용 관계 |
| `performedOperation` | 검증 전에 실제 수행한 동작. 단순 허용/수용 key로 대체하지 않는다. | Backend 수행 근거 → 검증 | 수행 근거와 연결된 뒤 |
| `target` | 실제로 효과 검증한 대상. | 검증 주체 | 현재 제한 해제 대상과 동일해야 함 |
| `configuration` | 검증한 구성. | 검증 주체 | 원래 수행/현재 대상과 비교 가능한 경우 |
| `effect` | 별도 효과 검증의 성공/실패/미확인. | 검증 주체 | 실제 검증 근거 후 |
| `verifiedAt` | 수행 뒤 검증한 원래 시각/구간. | 검증 주체 | 수행→검증 인과 관계가 확인된 범위 |

### Lifetime / Mutability / 폐기

생성 뒤 불변이며 HEALTH가 현재 instance/target/config·permission/수행 인과를 확인해 적용한다. 현재 대상이 바뀌거나 비교 근거 부족이면 clear를 보류하고 기존 제한/최근 기록을 유지한다. 실제 검증 방법/숫자·retry는 TBD다.

**관련 Function:** [AudioStream_Advance](../30_FUNCTIONS/40_AUDIO_STREAM_ASSET_FUNCTIONS.md#audiostream-advance) · [AudioControl_Service](../30_FUNCTIONS/50_DRIVER_HAL_FUNCTIONS.md#audiocontrol-service) · [Health_Evaluate](../30_FUNCTIONS/60_HEALTH_RUNTIME_FUNCTIONS.md#health-evaluate)

**관련 Contract:** [고장·복구](../50_CONTRACTS/00_CONTRACT_OVERVIEW.md#fault) · [비동기 근거](../50_CONTRACTS/00_CONTRACT_OVERVIEW.md#async) · [시간·최신성](../50_CONTRACTS/00_CONTRACT_OVERVIEW.md#timing)

**Provisional owner header 후보:** Backend 검증 Evidence 인터페이스 후보 — R6에서 확정. 실제 파일명·신규 Header 필요 여부는 정하지 않는다.

### Pseudo-C

```c
typedef struct
{
    VssRecoveryKeyType permission;
    VssOperationKeyType performedOperation;
    VssTargetKeyType target;
    VssConfigurationKeyType configuration;
    VssRecoveryEffect effect;
    VssFactTime verifiedAt;
} VssRecoveryVerificationEvidence;
```



<a id="vsshealthchange"></a>
## VssHealthChange — scalar enum
**Category: Result**

진단/현재 대상 제한 변화의 적용 결과만 반환한다. 기존 관측·허용/수행/검증을 하나의 Assessment struct로 합치지 않는다.

**Owner:** HEALTH · **Producer:** Health_Evaluate · **Consumer:** FLOW

| 값 | 의미 / 유효 조건 |
| --- | --- |
| `VSS_HEALTH_UNCHANGED` | 이번 평가에서 현재 제한 변화 없음 |
| `VSS_HEALTH_CHANGED` | 현재 대상/제한·허용 또는 해제가 반영 완료됨 |
| `VSS_HEALTH_RECONFIRM_REQUIRED` | 대상/시간/검증 근거가 부족하여 해제/판정 재확인 필요 |

**Lifetime / Mutability:** 호출별 불변 결과. current/recent/permission은 보호된 해당 자료로 읽는다.

**관련 Function:** [Health_Evaluate](../30_FUNCTIONS/60_HEALTH_RUNTIME_FUNCTIONS.md#health-evaluate) · [Flow_Process](../30_FUNCTIONS/10_FLOW_FUNCTIONS.md#flow-process)

**관련 Contract:** [고장·복구](../50_CONTRACTS/00_CONTRACT_OVERVIEW.md#fault)

**Provisional owner header 후보:** HEALTH 평가 반환 후보 — R6에서 확정.

```c
typedef enum
{
    VSS_HEALTH_UNCHANGED,
    VSS_HEALTH_CHANGED,
    VSS_HEALTH_RECONFIRM_REQUIRED
} VssHealthChange;
```

## 관측/평가군 — INLINE / SPLIT

`VssDiagnosticObservations`는 VssDiagnosticEvidence들과 실제 VssRecoveryPerformedEvidence/VssRecoveryVerificationEvidence 및 VssReadBasis의 typed 읽기 전달이다. 통합 Observations struct를 만들지 않는다. 각 발견 주체의 사실은 HEALTH가 반영할 때까지 개별 보존하고 복구 허용/수행/검증을 같은 슬롯으로 덮지 않는다.

`VssHealthAssessment`는 VssFaultState의 읽기 관측, VssRecoveryPermission, VssHealthChange로 나눈다. 허용과 현재 제한의 producer는 HEALTH이며 실제 수행 근거의 producer는 Backend다. FLOW는 writer를 대신하지 않는다.

`VssStatusObservations`는 반영 완료된 VssPlaybackObservation·STORE 후보/수용 자원 근거·INPUT 출처/시간·Backend readiness/정리·HEALTH 현재/최근 제한과 VssReadBasis의 typed borrow다. 기존 보고 파생에 필요한 값만 읽고 각 owner Context를 한 struct에 복제하지 않는다. input accepting의 저장/출처/replay 준비는 INPUT/STORE의 현재 관측 근거이며 새 accepting manager를 만들지 않는다.

<a id="status"></a>
## VSS_STATUS — 파생 보고와 생성 결과

<a id="vssstatusbuildresult"></a>
## VssStatusBuildResult — scalar enum
**Category: Result**

관측 부족으로 새 정상 보고를 만들지 않는 경계다. invalid Snapshot에 정상 field를 억지로 채우지 않는다.

**Owner:** HEALTH 보고 책임 · **Producer:** Health_BuildStatus · **Consumer:** FLOW

| 값 | 의미 / 유효 조건 |
| --- | --- |
| `VSS_STATUS_CREATED` | 일관된 반영 완료 관측에서 새 Snapshot 생성 |
| `VSS_STATUS_REEVALUATE` | 관측 부족/혼합·기존 미정 reduction 때문에 새 보고 확정 불가 |

**Lifetime / Mutability:** 해당 호출 결과. REEVALUATE에서는 새 VssStatusSnapshot을 생성하지 않는다.

**관련 Function:** [Health_BuildStatus](../30_FUNCTIONS/60_HEALTH_RUNTIME_FUNCTIONS.md#health-buildstatus) · [Flow_Process](../30_FUNCTIONS/10_FLOW_FUNCTIONS.md#flow-process)

**관련 Contract:** [시간·최신성](../50_CONTRACTS/00_CONTRACT_OVERVIEW.md#timing) · [고장·복구](../50_CONTRACTS/00_CONTRACT_OVERVIEW.md#fault)

**Provisional owner header 후보:** HEALTH 보고 생성 결과 후보 — R6에서 확정.

```c
typedef enum
{
    VSS_STATUS_CREATED,
    VSS_STATUS_REEVALUATE
} VssStatusBuildResult;
```

<a id="vssreportedplaybackstate"></a>
## VssReportedPlaybackState — scalar enum
**Category: Snapshot**

기존 외부 보고의 의미 상태다. 물리 bit/enum 숫자와 전체 조합 reduction은 확정하지 않는다.

**Owner:** HEALTH 파생 보고 · **Producer:** Health_BuildStatus · **Consumer:** FLOW → INPUT → Node Communication

| 값 | 의미 / 유효 조건 |
| --- | --- |
| `VSS_REPORT_STARTUP` | 정상 초기화/미평가 의미. 미평가를 실패로 바꾸지 않음 |
| `VSS_REPORT_READY` | 정상 준비이나 실제 시작된 활성 Session 미확인 |
| `VSS_REPORT_PLAYING` | 적법한 actual이 확인된 전체 Session. cue 무음/반복 대기·시작 뒤 종료 진행 포함 |
| `VSS_REPORT_FAULT` | 정상 출력 보장 불가/QUARANTINED 등 기존 보고 Fault 의미 |

**Lifetime / Mutability:** 생성한 불변 Snapshot의 전달 수명. 상태 우선순위/reduction의 미정은 유지.

**관련 Function:** [Health_BuildStatus](../30_FUNCTIONS/60_HEALTH_RUNTIME_FUNCTIONS.md#health-buildstatus)

**관련 Contract:** [고장·복구](../50_CONTRACTS/00_CONTRACT_OVERVIEW.md#fault)

**Provisional owner header 후보:** HEALTH 보고 의미 후보 — R6에서 확정.

```c
typedef enum
{
    VSS_REPORT_STARTUP,
    VSS_REPORT_READY,
    VSS_REPORT_PLAYING,
    VSS_REPORT_FAULT
} VssReportedPlaybackState;
```

<a id="vssservicelevel"></a>
## VssServiceLevel — scalar enum
**Category: Snapshot**

기존 지원 서비스 수준이다. 개별 start 허가/입력 수용 보장이 아니다.

**Owner:** HEALTH 파생 보고 · **Producer:** Health_BuildStatus · **Consumer:** 외부 보고 소비자

| 값 | 의미 / 유효 조건 |
| --- | --- |
| `VSS_SERVICE_FULL` | 기존 지원 서비스가 정상 범위 |
| `VSS_SERVICE_DEGRADED` | 일부 서비스 제한의 기존 의미 |

**Lifetime / Mutability:** 지원 수준의 불변 보고. WINDOW 보류만으로 DEGRADED로 만들지 않는다.

**관련 Function:** [Health_BuildStatus](../30_FUNCTIONS/60_HEALTH_RUNTIME_FUNCTIONS.md#health-buildstatus)

**관련 Contract:** [고장·복구](../50_CONTRACTS/00_CONTRACT_OVERVIEW.md#fault)

**Provisional owner header 후보:** HEALTH 보고 의미 후보 — R6에서 확정.

```c
typedef enum
{
    VSS_SERVICE_FULL,
    VSS_SERVICE_DEGRADED
} VssServiceLevel;
```

<a id="vssstatussnapshot"></a>
## VssStatusSnapshot
**Category: Snapshot**

현재 원본의 반영 완료·일관된 관측에서 파생한 VSS_STATUS다. Fault/ledger/Session의 두 번째 authoritative state가 아니다.

**Owner:** HEALTH 보고 책임  
**Producer:** Health_BuildStatus  
**Consumer:** FLOW → INPUT의 기존 통신 연결 → Node Communication

### 사람이 읽는 Field 정의

| Field | 의미 / 필요한 이유 | Field 생성 주체 | 유효 조건 |
| --- | --- | --- | --- |
| `state` | 기존 STARTUP/READY/PLAYING/FAULT 의미. | HEALTH | 보고 조합 근거가 충분한 경우 |
| `availability` | 기존 외부 Availability 의미. UNAVAILABLE 연결·초기/전체 reduction TBD를 유지한다. | HEALTH | 기존 계약으로 파생 가능할 때만 |
| `serviceLevel` | FULL/DEGRADED 지원 수준. | HEALTH | 지원/제한 근거로 파생 가능한 경우 |
| `accepting` | INPUT·STORE 자원/출처·시간/replay 준비의 수용 가능 근거. | HEALTH의 반영된 owner 관측 파생 | 개별 accepted 보장이 아님 |
| `currentFaults` | 보고 소유 자료에 보호한 현재 Fault 의미 값 목록. mutable FaultState 포인터가 아니다. | HEALTH | Snapshot의 불변 storage가 보호되는 동안 |
| `currentFaultCount` | 실제 보고에 포함한 현재 Fault 수. bit layout/고정 용량을 정하지 않는다. | HEALTH | currentFaults 유효 범위 |
| `recentFaults` | 보고 소유 자료에 보호한 최근 Fault 의미 목록. 현재 제한과 별개다. | HEALTH | Snapshot의 불변 storage가 보호되는 동안 |
| `recentFaultCount` | 보고에 포함한 최근 의미 수. | HEALTH | recentFaults 유효 범위 |
| `basis` | 보고 생성에 사용한 불변 적용/시간 근거. 최신 실행 허가로 재사용하지 않는다. | FLOW 관측 → HEALTH 보고 소유로 보호 | 해당 보고의 생성·전달 기간 |

### Lifetime / Mutability / 폐기

VssStatusBuildResult=CREATED일 때만 생성하고 전달 뒤 불변이다. 포함한 목록/basis stamp는 보고 소유 또는 독립 불변 보호 자료이며 mutable 원본을 그대로 가리키지 않는다. 기존 송신 연결이 자료를 복사하면 복사 완료 뒤, 참조하면 실제 전송 소비 종료까지 유지한다. 실제 Node Communication 복사/참조 방식은 TBD다. 부족/혼합 관측에서는 새 정상 Snapshot을 만들지 않고 재평가를 반환한다.

**관련 Function:** [Health_BuildStatus](../30_FUNCTIONS/60_HEALTH_RUNTIME_FUNCTIONS.md#health-buildstatus) · [Flow_Process](../30_FUNCTIONS/10_FLOW_FUNCTIONS.md#flow-process) · [Input_Process](../30_FUNCTIONS/20_INPUT_STORE_FUNCTIONS.md#input-process)

**관련 Contract:** [소유·수명](../50_CONTRACTS/00_CONTRACT_OVERVIEW.md#ownership) · [고장·복구](../50_CONTRACTS/00_CONTRACT_OVERVIEW.md#fault) · [시간·최신성](../50_CONTRACTS/00_CONTRACT_OVERVIEW.md#timing)

**Provisional owner header 후보:** HEALTH 보고 Snapshot 인터페이스 후보 — R6에서 확정. 실제 파일명·신규 Header 필요 여부는 정하지 않는다.

### Pseudo-C

```c
typedef struct
{
    VssReportedPlaybackState state;
    VssAvailabilityValueType availability;
    VssServiceLevel serviceLevel;
    SemanticBool accepting;
    const VssFaultKind * currentFaults;
    SemanticCount currentFaultCount;
    const VssFaultKind * recentFaults;
    SemanticCount recentFaultCount;
    VssReadBasis basis;
} VssStatusSnapshot;
```

accepting=false여도 기존 duplicate·유효 CLEAR·품질/기한의 안전 경로는 계속된다. Audio Fault와 입력 수용 가능성은 별개다. WINDOW [구현 보류 — 설계 유지] 자체는 Fault/DEGRADED 근거가 아니다. QUARANTINED는 기존 FAULT/UNAVAILABLE 연결을 유지한다. 전체 보고 조합/초기 Availability를 새 정책으로 확정하지 않는다.

<a id="scalar-aliases"></a>
## 보조 scalar / 불투명 의미 표기

아래는 경계 identity·정책 값의 의미 alias다. 각각 독립 struct/새 ID 생성 API를 만드는 목록이 아니다. 모든 값의 폭/encoding·확정 Header는 미정이다. 실제 producer의 원래 문맥에서 보호하고 전달 뒤 불변으로 사용한다. 회수/무효화는 해당 주 Data의 lifetime을 따른다.

| 표기 | Producer / 의미 owner | 목적 / validity |
| --- | --- | --- |
| `VssDiagnosticStageType` | 원래 발견 producer | 기존 실제 발견 단계. debug counter/새 Fault enum 아님 |
| `VssCauseType` | 원래 발견 producer | 확인한 원인. exact cause 매핑/새 taxonomy 확정 아님 |
| `VssTargetKeyType` | 원래 대상 관측/HEALTH 연결 | 오류/제한/복구의 실제 의미 대상. 불투명 scalar key이며 void payload/union 객체 아님 |
| `VssFaultInstanceKeyType` | HEALTH | 현재/최근 해당 Fault 발생. 옛 복구로 새 발생 해제 방지 |
| `VssRestrictionType` | HEALTH 기존 정책 | 현재 대상의 출력/지원 제한 의미. 전체 reduction 확정 아님 |
| `VssRecoveryKeyType` | HEALTH | 같은 허용·수행/검증 관계. 실제 숫자/token encoding 미정 |
| `VssRecoveryActionType` | HEALTH 기존 정책 | 허용한 기존 대상 Action/범위. exact cause 매핑·retry/fallback TBD |
| `VssAvailabilityValueType` | HEALTH 보고 파생 | 기존 외부 Availability 값의 의미. UNAVAILABLE 연결은 유지; 초기/전체 조합 정책 TBD |

```c
typedef SemanticStage VssDiagnosticStageType;
typedef SemanticCause VssCauseType;
typedef SemanticIdentity VssTargetKeyType;
typedef SemanticIdentity VssFaultInstanceKeyType;
typedef SemanticPolicyCondition VssRestrictionType;
typedef SemanticIdentity VssRecoveryKeyType;
typedef SemanticAction VssRecoveryActionType;
typedef SemanticContractValue VssAvailabilityValueType;
```

Owner header 후보는 해당 Data의 의미 owner 그룹을 따른다. R6에서 기존 Header 재사용/소유를 확인한다. 별도 공통 God Header를 만들지 않는다.

## 공통 TBD / 후속 범위

논리 필드의 실제 폭/숫자·alignment/packing/ABI·storage/복사·참조 수단과 Header filename은 미정이다. RTD/DMA/TCD·`.c/.h` Mapping·Task/Queue/Mutex·실제 C 구현은 작성하지 않았다. [pseudo-C 표기](00_DATA_OVERVIEW.md#pseudo-conventions)와 [기존 Implementation TBD](../90_BINDING/91_IMPLEMENTATION_TBD.md)를 함께 확인한다. 관련 계약은 기존 원문 링크이며 R4의 Cross Contract 원문 통합을 선행하지 않았다.
