# Playback — 채택, 최초 실제 시작과 정상 정책 진행

> R5 · R4 독립 PASS 기준 · 전체 Session 전이의 writer는 PLAYBACK이다.

[표기 규칙](00_PSEUDOCODE_OVERVIEW.md#notation) · [읽기 선택](20_INPUT_TO_SELECTION.md#select-choose) · [Audio 실행](40_AUDIO_PREPARE_TX_FLOW.md) · [종료/late/uncertain](50_STOP_LATE_UNCERTAIN_FLOW.md)

## 목적·진입·전제·자료

FLOW가 전달한 판단/종료 의도는 `Playback_RequestTransition`, typed Backend 사실·시간 기회는 `Playback_Advance`에 들어온다. 결과 없는 진행도 허용한다. 원본 candidate/plan/조건은 반영 완료된 읽기 자료이며 PLAYBACK이 채택하기 전에 재확인한다. 새로운 Session은 old 전체 retirement와 실제 provider/PCM 안전 조건 및 독립 보호 공간이 충족된 후에만 준비한다.

| 기존 R3 타입 | 이 흐름의 역할 |
| --- | --- |
| `VssSelectionDecision`, `VssReadBasis`, `VssCandidateObservation` | 판단·현재 후보/freshness; 타 owner 원본 write 금지 |
| `VssSessionContext`, `VssAttemptContext` | PLAYBACK의 전체 정책/원시도/후속 권한·결과 보호 |
| `VssAudioPreparation`, `VssPlaybackControlCommand` | STREAM 요청; PCM 주소 없음 |
| `VssAudioRequestResult`, `VssAudioOutputEvidence`, `VssBackendCleanupObservation` | 서로 독립된 typed 사실; 단일 성공값 없음 |
| `VssPlaybackFact`, `VssPlaybackObservation` | 원발생 통지·적용 완료 읽기 관측 |

`position`, repeatRule/UseLimit의 실제 표현과 Session/Attempt key 생성·개수는 미정 alias다. cue마다 Attempt를 하나씩 강제하지 않는다. 이 문서는 plan opaque position을 구체 index/주소로 바꾸지 않는다.

<a id="playback-requesttransition"></a>
## Playback_RequestTransition — 판단 채택과 종료 의도

```text
CORE Playback_RequestTransition
INPUT VssSelectionDecision decision OR 원 Session의 종료 의도/이유;
      반영 완료 후보·기한·owner·HEALTH/Backend 제한·자원 조건
OUTPUT VssRequestDisposition
OWNER PLAYBACK; own VssSessionContext session; VssAttemptContext attempt

if 현재 Session의 유효 CLEAR/선점/고장/정책 끝/실행 조건 상실 또는 종료 의도:
    STEP 원 Session/의도 연결을 검증한다; 다른 old Session 요구는 원정리로 격리.
    if 현재 Session에 적용 가능한 종료:
        session.startAllowed = false; session.cueAllowed = false; session.repeatAllowed = false
        STEP 정상 끝/선점은 STOPPING, 실패/위반은 ABORTING; QUARANTINED는 유지.
        VssPlaybackControlCommand stop
        stop.session = session.session; stop.attempt = session.currentAttempt
        stop.action = VSS_AUDIO_STOP
        CALL AudioStream_RequestControl(stop, 최신 조건)
        STEP 즉시 결과도 Playback_Advance의 같은 원적용 계약으로 연결한다.
        return VSS_REQUEST_ADOPTED // 실제 종료/retirement는 아님.
if 이미 STOPPING/ABORTING/QUARANTINED:
    STEP old 정리를 계속하고 최신 재선택 필요성을 남긴다.
    return VSS_REQUEST_WAIT // winner 변화로 종료 의도 취소 없음.
if decision.basis == null 또는 최신 basis 재확인 실패:
    return VSS_REQUEST_WAIT
switch decision.action:
    VSS_SELECTION_RECOLLECT: return VSS_REQUEST_WAIT
    VSS_SELECTION_WAIT: return VSS_REQUEST_WAIT // 현재 One-shot을 무근거 중단하지 않음.
    VSS_SELECTION_KEEP:
        if 원 current candidate/plan과 계속 진행 조건 일치: return VSS_REQUEST_KEPT
        else: return VSS_REQUEST_WAIT
    VSS_SELECTION_REPLACE:
        if old 출력 owner 있음:
            STEP 위 종료 경로로 원 후속 권한 차단·STOP 요청.
            STEP old retirement 뒤 FLOW가 최신 후보를 재선택하도록 남긴다.
            return VSS_REQUEST_WAIT // 아직 새 prepare/start 없음.
    VSS_SELECTION_CHOOSE:
        continue
if decision.candidate == null 또는 decision.plan == null: return VSS_REQUEST_REJECTED
VssCandidateObservation candidate = 읽기 참조된 원후보
if candidate.validity != VSS_CANDIDATE_APPLICABLE 또는 최신 후보/제한 불충분:
    return VSS_REQUEST_WAIT
if old 전체 retirement/실제 provider·PCM 안전/독립 귀속 보호 공간 불충분:
    return VSS_REQUEST_WAIT
if 미시작 One-shot의 원본 시간 비교 불가 또는 age < 2초 [잠정] 확인 불가:
    return VSS_REQUEST_WAIT // age/UseLimit 관계 미정도 임의 합성 없음.
STEP Session/Attempt key·전체 plan·원본 Meta를 PLAYBACK 수명으로 먼저 보호한다.
session.candidate = candidate.key; session.mode = candidate.mode
session.plan = decision.plan의 보호 참조; session.firstStartMeta = 원본 Meta
STEP session.position을 전체 정책 시작점에 둔다. Stateful도 PCM offset resume 없음.
session.phase = VSS_PLAYBACK_PREPARING
STEP 현재 정책이 허용한 start/cue/repeat 권한만 설정한다.
attempt.session = session.session; attempt.attempt = session.currentAttempt
attempt.startKnowledge = VSS_START_NOT_ATTEMPTED; attempt.retired = false
VssAudioPreparation preparation
preparation.session = session.session; preparation.attempt = attempt.attempt
preparation.plan = session.plan; preparation.position = session.position
VssCueDescriptor cue = 원 정책의 현재 cue 읽기 참조
STEP preparation.asset은 원 정책 cue.asset에 연결한다; fallback 없음.
STEP preparation.firstStartMeta는 최초 One-shot gate 필요 시만 원본 보호 참조, 그 외 null.
CALL AudioStream_Prepare(preparation)
STEP 원 즉시/비동기 준비 결과를 Playback_Advance로 연결한다.
return VSS_REQUEST_ADOPTED
```

Started One-shot은 재선택 후보로 부활시키지 않는다. 살아 있는 Stateful은 old retirement 뒤 새 Session에서 정책 시작점부터 진행한다. 단일 owner retirement와 HAL 기록 전부 폐기는 서로 다른 조건이다.

<a id="playback-advance"></a>
## Playback_Advance — 준비/수락/actual을 각각 반영

```text
CORE Playback_Advance / normal slice
INPUT optional VssAudioRequestResult request;
      protected VssAudioOutputEvidence output; VssBackendCleanupObservation cleanup;
      최신 적용 조건; VssTimeEvidence now
OUTPUT VssPlaybackObservation; protected VssPlaybackFact/진단/후속 필요성
OWNER PLAYBACK; own VssSessionContext session; VssAttemptContext attempt

STEP 원 Session/Attempt/정책 position을 찾아 연결한다.
if 귀속 부족: STEP 원근거 부족/손실 진단·보호; return 후속 필요성
if 원 uncertain 최종 뒤 또는 attempt.retired:
    continue [50 playback-stop-late의 old 정리/진단 slice]; 정상 전이 없음.
STEP actual 근거와 종료 의도를 request acceptance보다 우선 보존한다.
if output.kind == OUTPUT_START_CONFIRMED:
    STEP output.transportFacts[0..transportFactCount)의 원 operation/attempt/scope/
         occurredAt/coverage와 활성화 이후 새 실제 출력 근거를 확인한다.
    if 원 actual 충분성 확인 안 됨:
        STEP confirmed Started/확정 위반 생성 없이 원 물리 관측과 부족·손실 coverage 보호.
    else if [first-actual-gate] 원 사실 시각/당시 권한이 적법:
        attempt.startKnowledge = VSS_START_ACTUAL_CONFIRMED
        STEP attempt.startEvidence에 불변 output을 보호한다.
        if 이 One-shot Session의 최초 적법 actual 통지가 아직 없음:
            VssPlaybackFact fact
            STEP fact.fact/occurrence/session/attempt/kind=VSS_FACT_FIRST_ACTUAL_START/
                 occurredAt/basisRevision을 원본 문맥으로 보호한다.
            STEP attempt.unappliedFacts/unappliedFactCount의 독립 통지 의무에 연결한다.
            CALL Store_ApplyPlaybackFact(fact)
            STEP [50 fact-notification]의 APPLIED/ALREADY_APPLIED 규칙 적용.
        if session.phase != VSS_PLAYBACK_STOPPING && session.phase != VSS_PLAYBACK_ABORTING:
            session.phase = VSS_PLAYBACK_ACTIVE
        // 종료 중이면 Started/보고 사실만; ACTIVE/cue 재개 없음.
    else if [first-actual-gate] 구간 전체가 기한/권한 밖임이 확인된 물리 출력 위반:
        STEP 기한/권한 밖 실제 출력과 위반 producer/원시각을 보존한다.
        STEP 확정 위반 진단만 생성; 적법 Started 합성 없음.
    else:
        STEP 시간 비교/당시 권한 근거 부족 또는 경계 걸침을 구별해 보호한다.
        STEP 원 물리 fact/시각 구간/coverage는 보존하고 부족·손실 범위를 진단한다.
        STEP 정상 Started도 확정 위반도 생성하지 않는다.
    if 이번 actual의 충분성 또는 적법성이 확인되지 않음:
        session.startAllowed = false; session.cueAllowed = false; session.repeatAllowed = false
        STEP 기존 STOPPING/ABORTING 유지, 그 외 ABORTING 안전 정리로 연결한다.
        STEP [50 stream-stop]의 원 STOP·차단·참조 보호·후속 정리를 유지한다.
        STEP 기존 적법 Started 사실은 지우지 않는다. 부족만으로 uncertain final/
             NO_START/출력 종료/retirement를 만들지 않고 [50]의 별도 충분조건을 기다린다.

if request exists:
    if request.session/attempt != 해당 원 문맥: STEP 원 late로 격리
    else if request.stage == VSS_AUDIO_PREPARED:
        attempt.preparation = VSS_AUDIO_PREPARED
        if 종료 의도/실제 단계가 이미 진행됨: STEP prepared로 되돌리지 않음
        else if 최신 CLEAR/후보/기한/HEALTH/owner/장치 구성이 유효하고 session.startAllowed:
            session.phase = VSS_PLAYBACK_START_IN_FLIGHT
            attempt.startKnowledge = VSS_START_PENDING_OR_UNKNOWN
            STEP START 의도·결과 보호를 호출 전에 확보한다.
            VssPlaybackControlCommand start
            start.session = session.session; start.attempt = attempt.attempt
            start.action = VSS_AUDIO_START
            CALL AudioStream_RequestControl(start, 최신 조건)
            STEP 즉시/후속 단계 결과를 같은 원시도에 적용한다.
        else:
            STEP 전체 후속 권한 차단·원 STOP; 준비 완료로 start 강제 없음.
    else if request.stage == VSS_AUDIO_START_SUBMITTED:
        STEP 제출 사실을 독립 보호한다; submitted≠accepted/enabled≠actual.
        if 원 acceptance/부분 효력/종료의 더 진행된 근거가 아직 없음:
            attempt.startRequest = request.stage
        else: STEP 늦은 submitted로 기존 단계/근거를 되돌리지 않는다.
    else if request.stage == VSS_AUDIO_START_ACCEPTED_AND_ENABLED:
        STEP 원 수용/활성화 사실을 보호한다; late acceptance로 actual/종료 상태 rollback 없음.
        STEP 현재 원 인과에 맞을 때만 attempt.startRequest에 수용 단계를 반영한다.
        STEP 더 진행된 actual/uncertain/종료 근거와 독립 사실은 그대로 보호한다.
    else if PREPARE_FAILED/REQUEST_REJECTED/EFFECT_UNCERTAIN:
        STEP 부분 효력 및 보호를 보존하고 [50] 차단/정리/미시작 지식 경로로 진행.

if 정상 ACTIVE이며 현 구간이 안전 종료되고 전체 계획에 정상 후속 cue/반복 있음:
    if session.cueAllowed 또는 해당 repeatAllowed가 정책상 허용되고 최신 조건 유효:
        VssCueDescriptor cue = 현재 정책 cue의 보호된 읽기 참조
        STEP cue.gapAfter/repeatRule에 따른 대기를 진행; waiting 자체는 owner 해제 아님.
        if 정책상 다음 준비 기회:
            STEP 같은 Session의 다음 position/필요 원 Attempt를 보호한다.
            STEP firstStartMeta=null인 정상 후속 VssAudioPreparation을 구성한다.
            CALL AudioStream_Prepare(정상 후속 preparation)
    else: STEP 전체 후속 권한 차단·정리 경로 연결.
if 전체 계획 끝:
    STEP 전체 후속 권한 차단; [50] 전체 출력 범위와 종료/최종/retirement 검증.
STEP 압축 EOF/lastForSegment/SEGMENT end만으로 WHOLE_COMPLETED 생성 없음.
STEP 미반영 통지/진단은 독립 보호하고 적용 완료 관측 stamp만 공개한다.
return 현재 관측 + 보호된 후속 사실 // 시작·최종·진단 하나로 overwrite 없음.
```

accepted 뒤 결과가 없다는 이유만으로 정상 in-flight를 uncertain 최종으로 만들지 않는다. AGE 경과로 actual 지식을 NO_START로 바꾸지도 않는다. 종료/uncertain 판정은 [50 후속 slice](50_STOP_LATE_UNCERTAIN_FLOW.md#playback-stop-late)에서 충분조건을 추가한다.

<a id="first-actual-gate"></a>
## 최초 actual gate — 원 사실 시각의 가능한 구간 전체

```text
LOCAL CONDITION / PLAYBACK; 외부 helper API나 새 데이터 필드 아님.
INPUT 원 VssInputMeta meta; actual의 VssFactTime occurredAt;
      원래 Session/Attempt 권한 문맥과 기존 최초 적법 Started 기록

if 이미 적법 Started된 같은 Session의 정상 후속 cue:
    STEP 원 Session/권한/정책 유효성 확인; 최초 age<2초 규칙 재적용 없음.
else if 최초 One-shot actual:
    if meta.timeDomain/timeEpoch와 occurredAt.domain/epoch 비교 근거 없음,
       continuity가 COMPARABLE 아님, 원래 activation/권한 대응 불충분:
        결과 = 근거 부족 // now나 capturedAt으로 대체 금지.
    else if occurredAt.earliest >= meta.originalAt 이고
            occurredAt.latest - meta.originalAt < 2초 [잠정]이며
            구간 전체가 원래 실제 시작 권한 범위 안:
        결과 = 적법 최초 actual
    else if 구간 전체가 기한/권한 밖:
        결과 = 물리 출력 위반 // 정확히 2초도 미허용.
    else:
        결과 = 적법 여부 불확실 // 일부 구간만 2초 안이어도 충분하지 않음.
else:
    STEP Stateful 현재 정책/권한·원출력 범위 확인; 새 One-shot gate 신설 없음.
```

시각 비교는 실제 숫자 타입/epoch 변환 확정이 아니다. 적법 확인 / 구간 전체의 기한·권한 위반 확정 / 시간·권한 근거 부족 또는 경계 걸침을 호출부에서도 구별한다. 충분한 물리 출력 fact가 있어도 적법성 근거가 부족할 수 있으며, 그 fact의 충분한 coverage를 임의로 부족으로 바꾸지 않고 비교·권한 등 부족한 범위를 따로 보존한다. 불확실하면 정상 Started와 확정 위반을 모두 보류하고 원 차단·안전 정리·불확실 지식을 유지한다. `USE_LIMIT` 관계, actual 시각의 물리 확보·최종화 조건은 TBD다. 허용되었을 처리 now와 과거 실제 occurredAt을 혼동하지 않는다.

## 실패·자원·종료와 원문 추적

새 보호 공간 부족은 기존 owner/통지/계획 참조를 보존한 WAIT/REJECTED다. 부분 prepare/start 실패는 재사용 rollback 없이 [STOP·자원 반환](50_STOP_LATE_UNCERTAIN_FLOW.md#stream-stop)으로 연결한다. plan/firstStartMeta/evidence/fact는 임시 command 포인터로 장기 보관하지 않으며 독립 실제 consumer 수명까지 보호한다. 전체 final 통지와 owner retirement는 STORE 이력 삭제·Fault clear가 아니다.

[PB 두 Core R2](../30_FUNCTIONS/30_POLICY_PLAYBACK_FUNCTIONS.md#playback-requesttransition) · [Advance R2](../30_FUNCTIONS/30_POLICY_PLAYBACK_FUNCTIONS.md#playback-advance) · [Session/Attempt R3](../40_DATA/40_PLAYBACK_DATA.md#vsssessioncontext) · [PlaybackFact R3](../40_DATA/40_PLAYBACK_DATA.md#vssplaybackfact) · [Backend R3](../40_DATA/50_AUDIO_STREAM_DATA.md#vssaudiooutputevidence) · [Async R4 충분조건](../50_CONTRACTS/20_ASYNC_RESULT_EVIDENCE.md#confirmed-outcomes) · [Timing R4](../50_CONTRACTS/30_TIMING_FRESHNESS.md#first-start) · [Ownership R4](../50_CONTRACTS/10_OWNERSHIP_LIFETIME.md#identity-lifetimes) · [PLAYBACK local](../20_MODULES/23_PLAYBACK_MODULE.md#outcomes) · [정책/물리 TBD](../90_BINDING/91_IMPLEMENTATION_TBD.md)
