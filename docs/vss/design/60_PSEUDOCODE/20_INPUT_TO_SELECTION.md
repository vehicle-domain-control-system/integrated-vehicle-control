# Input → Selection — 입력 적용, 원본 기한과 읽기 판단

> R5 · R4 독립 PASS 기준. FLOW 조율 / INPUT 검증 / STORE 수용·기한 / POLICY 읽기 판단을 분리한다.

[표기 규칙](00_PSEUDOCODE_OVERVIEW.md#notation) · [Startup](10_STARTUP_FLOW.md) · [실행 전이](30_PLAYBACK_FLOW.md#playback-requesttransition)

## 목적·진입·전제

RX 또는 RX 없는 처리 기회 모두 진행한다. 정상 입력 적용, 품질 변화, STORE 수용 실패, 반영 완료 관측 수집, 읽기 선택을 별개로 반환한다. INPUT이 신뢰한 원 출처·순서·시간은 STORE가 거부해도 rollback하지 않는다. 원본 age/Stateful Hold의 부족한 시간 근거는 미확정으로 보존한다.

| Core / owner | R3 입력 → 출력 | 읽기/쓰기 경계 |
| --- | --- | --- |
| `Flow_Process` / FLOW | opportunity, time → flow progress | 다른 owner의 공개 Core 호출만 |
| `Input_Process` / INPUT | optional evidence, time → application | context만 쓰고 STORE 호출 |
| `Store_ApplyInput` / STORE | OneShotEvent **또는** StateUpdate **또는** QualityChange → admission | ledger / Stateful / Rear |
| `Store_AdvanceDeadlines` / STORE | time, PlaybackObservation → store progress | 해당 원 기한·후보 |
| `Select_Choose` / POLICY | CandidateObservation, PlaybackObservation, 제한/가용 관측, ReadBasis → decision | 판단 결과만; 원본 read-only |

도착 자료/plan/관측 참조는 해당 평가·호출 동안 유효해야 한다. 아래 typed alternatives는 새 통합 struct나 실제 API overload가 아니다.

<a id="flow-process"></a>
## Flow_Process — 정규 bounded 기회

```text
CORE Flow_Process / INPUT_OR_FOLLOW_UP 또는 TIME_CHECK slice
INPUT VssProcessingOpportunity opportunity; VssTimeEvidence now
OUTPUT VssFlowProgress

CALL Input_Process(optional INPUT 도착 자료, now)
CALL AudioStream_Advance(now) // raw 알림을 사실로 해석하지 않음.
CALL Playback_Advance(보호된 typed Backend 사실, 최신 실행 조건, now)
     // [30/50] 최초/최종 사실을 Store_ApplyPlaybackFact로 연결하고 미반영을 보호.
STEP 이미 반영된 최신 VssPlaybackObservation pb를 수집한다.
CALL Store_AdvanceDeadlines(now, pb)
CALL Health_Evaluate(원진단/복구 수행/별도 검증, 적용 근거)
STEP [60 recovery-flow]의 현재 허용과 old 안전 종료를 조율한다.

VssReadBasis basis // 각 owner가 적용 완료 뒤 제공한 stamp만 연결.
STEP STORE 후보, PLAYBACK 관측, HEALTH 제한, Backend/ASSET 가용 관측을 읽는다.
STEP basis.stamps[0..basis.stampCount)의 owner/revision/appliedThrough를 대조한다.
if 관련 사실 미반영, 인과 선행 fact 미충족, 비교 시간 부족:
    basis.validity = VSS_READ_INCOMPLETE
    STEP 관련 owner 후속 적용/재수집 필요성 보존.
    return VSS_FLOW_RECOLLECT_OBSERVATIONS
if 수집 후 관련 원본 revision 변경:
    basis.validity = VSS_READ_CHANGED
    return VSS_FLOW_RECOLLECT_OBSERVATIONS
basis.validity = VSS_READ_VALID
basis.time = now // 원 사실 시간의 대체값은 아님.
CALL Select_Choose(관측, basis) -> VssSelectionDecision decision
CALL Playback_RequestTransition(decision, 최신 실행 조건)
STEP 즉시 발생한 원결과는 같은 owner 적용 경계에 연결한다; 숨은 getter 적용 없음.
CALL Playback_Advance(원 즉시 결과, 최신 조건, now) // budget 안에서; 미반영 통지는 보호.
CALL Health_Evaluate(실행 요구에서 새로 발견된 원진단, 현재 적용 근거)
VssReadBasis reportBasis
STEP 보고용 stamp/인과/시간을 새로 수집한다; 선택에 쓴 basis를 그대로 재사용하지 않는다.
if 보고 근거 미반영/변경/부족:
    STEP 새 정상 보고 없이 재수집 필요성을 보존; return VSS_FLOW_RECOLLECT_OBSERVATIONS
reportBasis.validity = VSS_READ_VALID; reportBasis.time = now
CALL Health_BuildStatus(적용 완료 관측, reportBasis)
STEP CREATED인 보고만 기존 INPUT 보고 연결로 전달/참조 보호한다.
if 원결과 적용/정리/보고 전달/재수집이 남음: return VSS_FLOW_FOLLOW_UP_REQUIRED
return VSS_FLOW_SETTLED
```

전체 owner revision 숫자가 같을 필요는 없다. 각 `VssOwnerStamp.owner`의 revision/적용 watermark와 필요한 인과 관계를 비교한다. 전역 revision manager, 새 Queue/Task, 완료될 때까지 도는 무제한 loop는 만들지 않는다. budget/직렬화 실물 수단은 TBD이며 후속 알림은 사실을 대체하지 않는다.

<a id="input-process"></a>
## Input_Process — 검증 상태를 먼저 적용한다

```text
CORE Input_Process
INPUT optional VssInputEvidence evidence; VssTimeEvidence now
OUTPUT VssInputApplication application
OWNER INPUT; own VssInputContext context

application.validation = VSS_VALIDATION_NO_CHANGE
application.contextApplied = false
application.storeAdmission = VSS_STORE_NOT_SUBMITTED
if evidence absent:
    STEP INPUT의 현재 context.appliedMeta/quality와 now로 품질/연속성 변화만 평가한다.
    if 최신 적용 가능한 신뢰 상실 없음: return application
    STEP 아래 QUALITY_CHANGE 경로로 진행; RX 없음에서 EVENT/CLEAR 합성 금지.
else:
    STEP evidence.meta.origin/source/generation/ordering을 해당 PRODUCT/TEST context와 대조한다.
    STEP signal/value 조합, quality, timeDomain/timeEpoch/continuity를 검증한다.
    if PRODUCT의 VSS_SIGNAL_ANTI_PINCH 제품 source 연결:
        STEP WINDOW [구현 보류 — 설계 유지] 경로를 유지; 제품 경고 신규 활성화 없음.
        application.validation = VSS_VALIDATION_REJECTED; return application
    if 과거 generation/order 또는 원 출처/시간 연속성 근거 부적합:
        if 현재 context에 적용 가능한 최신 품질 상실 근거도 아님:
            application.validation = VSS_VALIDATION_REJECTED; return application
        STEP QUALITY_CHANGE 경로로 진행; 옛 invalid로 최신 품질 덮기 금지.
    else if evidence.quality != VSS_INPUT_VALID:
        STEP 적용 가능한 현재 품질 변화이면 QUALITY_CHANGE; 같은 품질 반복은 NO_CHANGE.
    else:
        STEP context.origin/signal/appliedMeta/quality를 검증한 원본으로 반영한다.
        STEP context.revision을 반영 완료 뒤 갱신한다.
        application.contextApplied = true
        application.validation = VSS_VALIDATION_APPLICABLE
        if 검증된 One-shot 의미 EVENT:
            VssOneShotEvent event
            STEP event.occurrence를 원본 identity로 연결한다. [TBD-IDENTITY]
            event.signal = evidence.signal; event.meta = evidence.meta
            CALL Store_ApplyInput(event) -> application.storeAdmission
        else:
            VssStateUpdate update
            update.signal = evidence.signal; update.value = evidence.value; update.meta = evidence.meta
            CALL Store_ApplyInput(update) -> application.storeAdmission
        return application // STORE_REJECTED에도 context를 rollback하지 않는다.

QUALITY_CHANGE:
    VssQualityChange change
    STEP change.signal/quality/basis/lossAt에 최신 적용 근거와 최초 상실 시각을 보존한다.
    STEP context 품질/검증 문맥만 반영 완료한다; ACTIVE/CLEAR 정상 사건 생성 없음.
    application.validation = VSS_VALIDATION_QUALITY_CHANGE
    application.contextApplied = true
    CALL Store_ApplyInput(change) -> application.storeAdmission
    return application
```

원 시간 비교/변환·source trust·PRODUCT/TEST 표현은 opaque alias를 임의 enum으로 만들지 않는다. `originalAt`은 sender/relay 지연 포함 원 발생이며 RX/now로 덮지 않는다. 정확한 발생 identity와 순서 비교는 `[TBD-IDENTITY/TIME]`이며 불충분하면 신규 사건 수용을 만들지 않는다.

<a id="store-applyinput"></a>
## Store_ApplyInput — 세 typed 적용 경로

```text
CORE Store_ApplyInput
INPUT VssOneShotEvent event OR VssStateUpdate update OR VssQualityChange change
OUTPUT VssStoreAdmission
OWNER STORE; own VssOccurrenceRecord record; VssStatefulState state; VssRearGateState rear

EVENT lane:
    if event.occurrence에 기존 accepted/final/replay 기록 있음:
        return VSS_STORE_DUPLICATE // 원본 age·state·최초/최종 fact 보존.
    if identity/원본 연결 불충분 또는 Pending→최종/재전달 방어 보호 용량 없음:
        return VSS_STORE_REJECTED // 기존 accepted/final eviction 금지.
    STEP 불변 event.meta와 발생 추적·통지 보호를 먼저 확보한다.
    record.occurrence = event.occurrence; record.signal = event.signal
    record.originalMeta = event.meta; record.state = VSS_OCCURRENCE_PENDING
    record.firstStartFact = null; record.finalFact = null
    record.tracking = VSS_TRACKING_PROTECTED
    STEP record.revision 반영 완료; 후보 관측 공개 뒤 return VSS_STORE_ACCEPTED

STATE lane:
    if update.signal == VSS_SIGNAL_REAR_ACTIVATION:
        rear.activation = update.value; rear.activationMeta = update.meta
        if update.value == VSS_VALUE_DISABLED:
            rear.riskApplicable = false
            STEP Rear 후보/Hold를 제거하고 실제 참조 종료 뒤 Hold 보호를 회수한다.
        else if update.value == VSS_VALUE_ENABLED:
            STEP 적용 가능한 위험 근거가 확인되기 전 riskApplicable를 true로 바꾸지 않는다.
        STEP rear.revision 반영 완료; return VSS_STORE_UPDATED
    state.lastValidValue = update.value
    STEP update.meta를 보호하고 state.lastValidMeta로 연결한다.
    state.quality = VSS_INPUT_VALID
    STEP 유효 최신 판단의 Hold 종료; 옛 firstLossAt/holdAnchor 참조를 안전 종료한다.
    if update.value == VSS_VALUE_CLEAR:
        STEP 해당 후보 제거; 다른 Stateful과 One-shot ledger 보존.
    else if update.signal == VSS_SIGNAL_REAR_RISK:
        STEP 최신 Activation과 위험 source/order 결합이 확인된 경우만 rear.riskApplicable=true.
        STEP DISABLED이면 후보 생성 금지; Emergency→Caution은 old 실행 교체 판단에 노출.
    else:
        STEP 검증된 ACTIVE를 후보에 반영; 반복 ACTIVE는 새 Session 요청 아님.
    STEP state/rear의 변경 revision 반영 완료; return VSS_STORE_UPDATED

QUALITY lane:
    state.quality = change.quality // lastValidValue/lastValidMeta는 그대로.
    if 기존 유효 경고 없음: STEP 새 후보 생성 없음; return VSS_STORE_UPDATED
    if state.firstLossAt == null:
        STEP change.lossAt와 basis 시간 문맥을 VssFactTime으로 보호한다.
        STEP state.firstLossAt을 그 원본 근거에 연결한다.
        if 원 상실과 state.lastValidMeta.validUntil 비교 가능:
            STEP 정의된 validUntil이 있으면 두 경계 중 이른 값을 holdAnchor로 보호한다.
            STEP validUntil이 없는 입력은 최초 상실 기준만 사용한다.
        else: STEP holdAnchor 확정 보류; 불명확한 fresh 후보 합성 없음.
    STEP 반복 INVALID/STALE·선점·복구로 firstLossAt/holdAnchor 연장 없음.
    STEP revision 반영 완료; return VSS_STORE_UPDATED
```

후보는 STORE가 위 원본으로 파생하는 `VssCandidateObservation`이다. 후보 생성/제거를 위한 별도 숨은 필드·상태·통합 Context는 정의하지 않는다. 시간/결합이 불충분한 관측은 `VSS_CANDIDATE_UNCERTAIN`으로 노출할 수 있으며 POLICY가 정상 적용 후보로 읽지 않는다.

<a id="store-advancedeadlines"></a>
## Store_AdvanceDeadlines — 먼저 실제 사실 반영 여부 확인

```text
CORE Store_AdvanceDeadlines
INPUT VssTimeEvidence now; latest VssPlaybackObservation pb
OUTPUT VssStoreProgress
OWNER STORE; own VssOccurrenceRecord record; VssStatefulState state

if 관련 pb.stamp 인과/최신 근거 부족 또는 pb.notificationPending:
    return VSS_STORE_RECONFIRM_REQUIRED // FLOW→PLAYBACK 통지 적용/재수집을 먼저.
if now.continuity != VSS_CONTINUITY_COMPARABLE 또는 원 domain/epoch 비교 불가:
    STEP 해당 freshness/Hold 불확실성 유지; return VSS_STORE_RECONFIRM_REQUIRED
for 이번 budget 안의 해당 원발생 record:
    if record.state != VSS_OCCURRENCE_PENDING: continue
    if 연결된 원시도 pb.startKnowledge == VSS_START_PENDING_OR_UNKNOWN:
        STEP age가 2초여도 Expired 생성 금지; continue
    if 원 uncertain 최종 또는 실제/통지 추적 근거 부족: continue
    if 확정 미시도 또는 전체 NO_START/retirement 뒤 확정 미시작:
        if 원본 age의 가능한 구간 전체가 >= 2초 [잠정]:
            record.state = VSS_OCCURRENCE_EXPIRED
            STEP replay/이력 보호 유지; revision 반영 완료.
        else if age가 경계를 걸침: STEP 후보 freshness 미확정; 재확인 필요.
for 해당 Stateful state:
    if 기존 경고의 holdAnchor 있고 기존 Hold 정책에 따른 만료가 확인됨:
        STEP 후보만 제거; lastValidValue/quality를 CLEAR/VALID로 변경하지 않는다.
    else if Hold 값/시간 근거 미정:
        STEP [TBD-HOLD] 보존; 임의 시간값으로 후보 연장/정상화 없음.
return 변경 있으면 VSS_STORE_CHANGED, 부족하면 VSS_STORE_RECONFIRM_REQUIRED,
       그 외 VSS_STORE_STABLE
```

`USE_LIMIT`은 별도 기존 조건이다. `<2s`와 임의 min/AND를 만들지 않는다. 관계가 필요한 결정의 미정은 재확인/TBD로 남긴다. Started Session의 정상 후속 cue는 이 원발생 Expired 경로 대상이 아니다.

<a id="select-choose"></a>
## Select_Choose — read-only keep / replace / wait

```text
CORE Select_Choose
INPUT VssCandidateObservation 후보들; VssPlaybackObservation pb;
      owner가 반영한 제한/가용 관측; VssReadBasis basis
OUTPUT VssSelectionDecision decision
OWNER POLICY // 관측 자료에 write 없음.

decision.basis = basis의 평가기간 보호 참조
decision.candidate = null; decision.plan = null
if basis.validity != VSS_READ_VALID:
    decision.action = VSS_SELECTION_RECOLLECT; return decision
STEP validity==VSS_CANDIDATE_APPLICABLE인 후보만 비교한다.
STEP Emergency > Warning/Caution > Feedback 순서 적용;
     Class 내부 [잠정] 순위는 POLICY 원표 사용. 값/Asset/pattern 신설 없음.
STEP 같은 의미 미시작 One-shot은 originalMeta의 원발생 순서, 동률이면 key 고정 순서.
     수신/queue 도착 순서 사용 금지. 비교 근거 부족이면 RECOLLECT.
if 유효 현재 One-shot이며 낮은 후보/새 후보 없음/같은 의미의 늦은 옛 발생만 있음:
    decision.action = VSS_SELECTION_KEEP
    STEP current 후보/현 전체 plan 보호 참조 연결; return decision
if 현재 Stateful의 유효 CLEAR/DISABLED/만료 또는 Rear Emergency→Caution:
    STEP 현재 판단에 종료/교체 필요성을 드러낸다; PB 필드 직접 변경 없음.
if winner 없음 또는 winner의 전체 plan/필수 음원·제한 근거 부족:
    decision.action = VSS_SELECTION_WAIT; return decision // 의미 다른 fallback 금지.
STEP decision.candidate/plan에 원 winner와 전체 계획을 연결한다.
if 같은 현재 유효 owner의 계속 진행: decision.action = VSS_SELECTION_KEEP
else if old 출력 owner 있음: decision.action = VSS_SELECTION_REPLACE
else if 현재 start 제한/안전 종료 대기: decision.action = VSS_SELECTION_WAIT
else: decision.action = VSS_SELECTION_CHOOSE
return decision // winner는 prepare/start 허가가 아님.
```

WAIT 판단 자체로 현재 One-shot 종료를 만들지 않는다. CLEAR·교체·기한 상실 등 현재 종료 필요성은 최신 실행 조건을 통해 [PLAYBACK 전이](30_PLAYBACK_FLOW.md#playback-requesttransition)에서 별도로 처리한다. 판단을 채택할 때도 최신 owner/후보/시간/제한을 재검사한다.

## 실패·후속·미정과 추적

입력 거부와 STORE 신규 공간 부족은 개별 수용 결과다. 기존 duplicate·유효 CLEAR·품질/기한 및 미반영 사실은 계속 처리한다. 미반영 PLAYBACK 사실의 확인은 [통지 및 STORE 적용](50_STOP_LATE_UNCERTAIN_FLOW.md#store-applyplaybackfact)으로 연결하며 STORE가 PLAYBACK을 역호출하지 않는다. 선택 결과 참조는 평가기간만 유효하고 PB가 채택할 자료는 자기 수명으로 보호한다.

`[TBD-IDENTITY/TIME]`: source·세대/순서·reboot/wrap alias 방지·시간 변환/신뢰. `[TBD-HOLD]`: 입력별 Hold 값·UseLimit 관계. 보호 예산·직렬화·정책 내부 순위/패턴 확정은 [기존 TBD](../90_BINDING/91_IMPLEMENTATION_TBD.md#policy). 미정 조건은 true로 대체하지 않는다.

[INPUT/STORE R2](../30_FUNCTIONS/20_INPUT_STORE_FUNCTIONS.md) · [POLICY R2](../30_FUNCTIONS/30_POLICY_PLAYBACK_FUNCTIONS.md#select-choose) · [INPUT R3](../40_DATA/10_INPUT_DATA.md#vssinputapplication) · [STORE R3](../40_DATA/20_STORE_DATA.md#vssstatefulstate) · [Rear R3](../40_DATA/20_STORE_DATA.md#vssreargatestate) · [읽기/선택 R3](../40_DATA/30_SELECTION_DATA.md#vssreadbasis) · [Timing R4](../50_CONTRACTS/30_TIMING_FRESHNESS.md#coherent-read) · [보호 R4](../50_CONTRACTS/10_OWNERSHIP_LIFETIME.md#borrow-and-protect) · [POLICY local 순위](../20_MODULES/22_POLICY_MODULE.md) · [STORE local](../20_MODULES/21_STORE_MODULE.md#stateful)
