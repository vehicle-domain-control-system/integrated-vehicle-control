# Stop / Late / Uncertain — 차단, 독립 회수와 원발생 최종

> R5 · R4 독립 PASS 기준. 이 파일은 기존 Core의 종료/예외 slice이며 별도 Stop/Retire Core를 추가하지 않는다.

[표기 규칙](00_PSEUDOCODE_OVERVIEW.md#notation) · [PB 정상 slice](30_PLAYBACK_FLOW.md#playback-advance) · [Audio 정상 slice](40_AUDIO_PREPARE_TX_FLOW.md) · [Fault 후속](60_FAULT_RECOVERY_FLOW.md)

## 목적·진입·전제·주요 typed 자료

선점/CLEAR/고장/정책 끝, 준비·START 부분 효력, 기한/관측 손실, 늦은 output/return이 진입 사유다. PLAYBACK은 전체 start/cue/repeat 권한, STREAM은 미래 decode/refill/PCM 제출, TX는 원 device 접근/출력 추적만 쓴다. HAL은 원등록의 raw를 보호한다. 원 귀속·scope·time·coverage가 불충분한 결과도 명시적으로 처리한다.

| 경계 | 자료 | 끝났다는 자동 추론 금지 |
| --- | --- | --- |
| TX→STREAM | TxOutputEvidence, PcmConsumptionEvidence, PcmReturnEvidence | consumed≠safe return≠output end |
| STREAM→PB | AudioOutputEvidence, BackendCleanupObservation | cleanup 하나로 모든 참조/Session 종료 추정 금지 |
| PB→STORE | PlaybackFact, FactApplication | final 통지≠retirement≠ledger 삭제 |
| HAL 원등록 | HalTxRegistration, HalDeviceRegistration, RawObservation | IRQ disable/owner 해제로 기록 폐기 금지 |

<a id="stream-stop"></a>
## AudioStream_RequestControl STOP + AudioTx_Request STOP

```text
CORE AudioStream_RequestControl / PlaybackControlCommand STOP slice
INPUT VssPlaybackControlCommand stop; 최신 원정리 조건
OUTPUT 원 요청 단계·진단·보호된 차단/정리 진행
OWNER STREAM; own VssProviderContext provider

STEP stop.session/attempt를 원 provider 및 진행 요청에 연결한다.
if 다른 old 문맥: STEP 그 old 정리만 진행; 새 provider 차단/해제 없음; return
STEP 원 Session의 STOP 의도를 기존 STREAM 제어 문맥에 먼저 보호한다.
STEP 그 Session의 모든 관련 provider/Attempt의 미래 decode/refill/새 sound handoff를
     차단한다; 아직 개별 정리를 방문하지 못한 대상도 STOP 의도로 생산/제출 금지.
provider.productionAllowed = false // 하위 STOP 호출·실패보다 먼저 고정.
provider.phase = VSS_PROVIDER_CLOSING
STEP 진행 중 producer가 마지막 인계 직전 차단을 재확인하도록 연결한다.
STEP 기존 ASSET span/PCM/원결과/등록 보호는 그대로 유지한다.
STEP STREAM이 보호한 원 준비/제어/인계 연결과 공개된 TX 결과/출력 관측으로
     stop.session의 모든 관련 원 Attempt/operation/출력 범위를 대조한다.
     stop.attempt 하나나 current만으로 한정하지 않고 active/대기 작업과
     descriptor/FIFO/frame의 잔류 출력·접근 가능성을 포함한다.
for budget 안의 원 Session 관련 미완료 TX 작업/출력 범위:
    if 원 Attempt/scope 귀속 또는 실제 대상 연결 근거가 부족·손실:
        STEP 해당 대상/부족 근거·진단·retain과 후속 정리 의무를 보호한다.
        STEP 임의 scope STOP이나 termination/retirement 생성 없음; continue
    VssTxControlCommand control
    control.attempt = 이 대상에 귀속 확인된 원 Attempt // stop.attempt로 일괄 대체 금지.
    control.action = VSS_TX_STOP
    VssOutputScope scope = 원 요청/관측과 일치하는 VSS_SCOPE_SEGMENT 또는 VSS_SCOPE_ATTEMPT
    control.scope = scope // 원 범위 그대로; SEGMENT를 ATTEMPT/WHOLE로 확대 금지.
    control.firstStartMeta = null
    STEP 이 STOP의 대상인 원 operation들과 실제 범위의 대응/결과 추적을 보호한다.
    CALL AudioTx_Request(control)
    if 하위 STOP 거부/partial/지연/결과 미확인:
        STEP 그 대상의 차단·retain·원결과·진단·후속 정리 유지; 성공으로 합치지 않음.
    STEP 수용은 요청 단계만 반영; 각 원 operation/범위의 실제 출력·접근 종료는
         후속 근거로 각각 확인하며 다른 Attempt/구간을 완료 처리하지 않는다.
if 귀속/범위 근거 부족 또는 budget/보호 공간 한계로 미처리 대상이 남음:
    STEP 새 출력 차단·기존 보호/진단 유지; 기존 Advance 기회에 원 정리를 계속한다.
    STEP 일부 STOP 성공이나 미처리 목록의 누락을 전체 출력 종료로 처리하지 않는다.
STEP 필요한 현재 장치 제어만 AudioControl_Service로 연결한다; mute≠termination.
if 하위 STOP 실패/partial/지연:
    STEP producer/대상/단계 진단과 미래 차단·원 retain을 유지한다.
    // productionAllowed=true rollback, 반환 뒤 자동 refill 없음.
STEP 후속 AudioStream_Advance(now)를 위해 원 cleanup을 보호한다.
return 요청 적용/후속 필요성 // 실제 stop·safe return 성공 아님.

CORE AudioTx_Request / TxControlCommand STOP slice (common 등록은 [40 audiotx-request])
INPUT VssTxControlCommand control; OUTPUT VssTxRequestResult/진단
OWNER TX; own VssTxOperation operation
STEP PCM 없는 operation.handoff=null과 원 Attempt/scope를 불변 보호한다.
STEP HAL 원귀속 확보 뒤 해당 old request/active work/descriptor/FIFO/frame 정리를 요구한다.
if 실패/partial/미래 접근 또는 출력 가능성 남음:
    STEP operation/requestState=CLEANUP_PENDING 또는 EFFECT_UNCERTAIN;
         retain/원결과/등록 보호와 old cleanup 지속.
STEP abort return/현재 idle만으로 OUTPUT_ENDED/PAST_NO_OUTPUT_PROVEN/PCM return 생성 없음.
return 원 단계 // STREAM/PB 원본 write 없음.
```

STOP 이후 물리 stale 방지용 zero-fill은 [기존 A/B의 정리 범위](40_AUDIO_PREPARE_TX_FLOW.md#physical-zero-fill)에서만 가능하다. 쓰기 안전 미확인 시 정리용 쓰기도 금지한다. 차단 상태를 정상 생산으로 되돌리지 않는다.

`scope`는 기존 `VssOutputScope`의 SEGMENT/ATTEMPT다. 같은 Attempt/scope에 관련 원 operation이 여럿이면 각 작업·실제 출력 구간의 대응과 잔류를 보호하며, 한 STOP/종료 근거가 그 작업들을 모두 덮는다는 확인 없이 일괄 완료하지 않는다. STREAM은 자기 원 요청과 공개된 결과를 연결하며 TX의 원본 operation을 직접 수정하지 않는다. 위 반복은 기존 정리 문맥의 bounded 처리이고 새 목록 타입/Queue/API가 아니다. 실제 대상 연결의 C 표현·파라미터 배치 및 물리 STOP 효과는 R6/B2-R TBD이며, 연결 불충분은 보호·차단·후속 필요성으로 남긴다.

<a id="tx-safe-return"></a>
## AudioTx_Advance — 소비와 옛 회차 접근 종료

```text
CORE AudioTx_Advance / return-cleanup slice
INPUT 원 VssTxOperation operation; 보호된 raw/physical 접근 근거
OUTPUT optional VssPcmReturnEvidence returned; 독립 consumption/output/진단
OWNER TX

if 원 operation.handoff == null:
    STEP 주소 없는 제어 작업에는 PCM 반환을 만들지 않는다.
else:
    VssPcmHandoffCommand handoff = operation.handoff의 보호된 원 읽기 참조
    STEP 원 handoff.key의 attempt/buffer/cycle과 실제 descriptor/active/대기 참조 대조.
    if 일부 source consumed만 확인됨:
        STEP PcmConsumptionEvidence만 반환; 장치 retain 해제나 CPU 쓰기 허가 없음.
    if HW/대기/처리 중 해당 old PCM refs가 모두 끝났고
       반복/지속 전송으로 옛 내용 재소비 가능성이 없다는 실제 충분 근거:
        VssPcmReturnEvidence returned
        returned.operation = operation.operation
        returned.key = 원 handoff.key
        returned.noFutureAccess = true // 해당 옛 작업/내용; 물리 주소 영구 비접근 아님.
        STEP returned.basis에 실제 boundary/interval/binding/충분 coverage를 보호한다.
        STEP returned를 STREAM 적용/보호 추적까지 유지한다.
    else:
        STEP 반환 확정 없음; 원 cycle/retain/정리/부족 또는 손실 진단 보존.
        // HALF/MAJOR/단일 DMA complete/abort 반환/회차 번호 변경만으로 true 없음.
STEP 실제 출력 종료·미래 출력 차단은 별도 TxOutputEvidence로 반환한다.
STEP 필요한 과거 실제 무출력 coverage가 없으면 NO_START 근거를 만들지 않는다.
return 각 독립 보호 사실
```

<a id="stream-safe-return"></a>
## AudioStream_Advance — 반환 적용과 지금의 쓰기 안전성

```text
CORE AudioStream_Advance / cleanup-return slice
INPUT VssPcmReturnEvidence returned; own VssPcmBufferState pcm;
      own VssProviderContext provider; 현재 실제 쓰기 조건
OUTPUT 원 usage/cleanup/진단·후속
OWNER STREAM

if returned.operation/returned.key가 원 protected 인계와 대응되지 않음:
    STEP 귀속 부족/손실 보호·진단; 현재 PCM usage 해제 없음; return
if returned.key.attempt/buffer/cycle != pcm.key.attempt/buffer/cycle:
    STEP old cycle의 정리/refs만 반영; 새 A 회차 또는 B를 해제하지 않는다; return
if !returned.noFutureAccess 또는 returned.basis.validity != VSS_COVERAGE_SUFFICIENT:
    STEP 원 usage/보호 유지; return 후속 필요성
STEP 옛 return의 현재 적용 유효성과 차기 동일 A/B DMA 방문 전 쓰기 안전 구간,
     실제 refs/메모리 조건·이번 refill/정리 쓰기의 완료 가능성을 다시 확인한다.
if [TBD-WRITE] 미확인 또는 적용 시 안전 구간 경과 또는 옛 PCM 재소비 가능성:
    STEP usage 해제/쓰기 없이 RETURN_PENDING 등 기존 보호를 유지; 진단/정리; return
pcm.usage = VSS_PCM_CPU_WRITABLE
if provider.productionAllowed && 원 provider/Attempt가 정상 후속 허용 상태:
    STEP [40 fill-buffer]를 bounded 수행; 인계 전 권한/쓰기 조건을 다시 확인.
else:
    STEP 신규 decode/refill/sound handoff 없음; 필요 물리 정리만 같은 안전 조건에서.
STEP 실제 decoder/read 참조 종료를 별도 확인하여 Asset_Read REFERENCE-END 연결.
STEP providerReferencesEnded/pcmAccessEnded/lowerOutputBlocked/lateIdentityProtected는
     각각의 원 evidence로 BackendCleanupObservation에 기록한다.
return 원 cleanup // CPU_WRITABLE은 무기한 쓰기 허가증도 retirement도 아님.
```

현재 쓰기 안전 조건은 R3에 없는 `safeUntil` 같은 필드를 새로 만들지 않고 실제 경계의 읽기 근거/TBD로 남긴다. `noFutureAccess`의 옛 회차 의미, 지속 DMA 다음 방문과 현재 적용 유효성은 C1-03 그대로다. 옛 HAL 불변 기록이 독립 보호된 채 새 A/B cycle을 사용하는 경우도 실제 old 위험 종료·현재 쓰기 안전·alias 없는 보호 공간을 먼저 충족해야 한다.

<a id="playback-stop-late"></a>
## Playback_Advance — whole 차단, 최종 전/뒤 late와 retirement

```text
CORE Playback_Advance / stop-late-uncertain slice
INPUT 원 VssAudioOutputEvidence output; VssBackendCleanupObservation cleanup;
      원 request/최신 조건/시간
OUTPUT 현재 관측·원 PlaybackFact 통지·진단·정리 필요
OWNER PLAYBACK; own VssSessionContext session; VssAttemptContext attempt

STEP 원 Session/Attempt를 찾는다; current 조회로 원귀속을 만들지 않는다.
if 원 uncertain 최종 뒤 또는 attempt.retired:
    STEP 원 lower 정리/late refs/진단만 진행한다.
    STEP 새 Session/A/B/ready/Fault 및 정상 Pending/Started/Completed 변경 없음.
    STEP 확인된 원 future/출력/refs 끝으로 old 독립 회수만 검토; return
if 충분한 원 actual이 최종 전에 늦게 도착:
    STEP [30 first-actual-gate]로 원 사실 시각/당시 권한 적법성 확인.
    if 적법: STEP 최초 Started 사실/보고만 적용; STOPPING/ABORTING 유지.
    else: STEP [30]의 확정 위반/불확실 분리를 따르며 원 차단·정리 유지;
               정상 Started 합성 없음.
if 요청 단계 late prepared/acceptance:
    STEP 원 단계 사실만 보존; 종료 의도/실제 지식 rollback 없음.

if 원 시작 결과 불명확의 기존 최종 판정 조건이 충족됨:
    // [TBD-FINAL] 조건/timeout 값 미정이면 이 branch 충족을 가정하지 않음.
    attempt.startKnowledge = VSS_START_UNCERTAIN_FINAL
    session.startAllowed = false; session.cueAllowed = false; session.repeatAllowed = false
    session.phase = VSS_PLAYBACK_QUARANTINED
    STEP 원 VSS_FACT_START_OUTCOME_UNCERTAIN을 만들어 통지 보호/STORE 적용한다.
    STEP PLAYBACK_STATE_FAILURE + FAULT + UNAVAILABLE 근거를 FLOW→HEALTH에 연결.
    STEP AudioStream_RequestControl(원 STOP)과 old cleanup을 계속한다.
    // final 기록은 실제 retirement보다 먼저 가능.
else if start pending/coverage insufficient/lost:
    STEP 원 uncertainty와 사실/참조 보호 유지; 현재 무음/timeout만으로 최종/NO_START 없음.

if 종료 의도/고장/정책 끝/현재 조건 상실:
    session.startAllowed = false; session.cueAllowed = false; session.repeatAllowed = false
    STEP STOPPING/ABORTING 또는 기존 QUARANTINED 유지; 원 STOP/후속 정리.

STEP 각 원 Attempt/출력 구간의 STOP 요청 단계·실제 종료·과거 무출력·safe return을
     독립 적용한다. 한 구간의 성공/cleanup은 다른 구간·Attempt의 종료 근거가 아니다.
STEP 아래 retirement 조건을 전체 관련 원 Attempt/구간을 빠짐없이 덮는 근거로 확인한다:
    (a) Backend의 충분 NO_START 또는 실제/잔류 TERMINATION+future output 차단,
    (b) 해당 Backend scope의 future PCM 차단,
    (c) session 전체 start/cue/repeat 권한 차단,
    (d) 원 late 귀속 보호 및 새 owner 준비의 실제 자원 안전 조건.
if scope가 SEGMENT 끝뿐이고 전체 정상 정책 후속이 유효:
    STEP [30] 같은 Session의 정상 다음 cue/대기; WHOLE final/retirement 아님; return
if 관련 원 Attempt/구간 중 미처리·잔류 또는 종료/차단 근거 부족·손실이 남거나
   위 전체 조건 중 하나라도 부족:
    STEP 전체 owner/참조 보호·새 출력 차단·old 정리 유지;
         whole final/retirement 생성 없음; return 후속 필요성
if attempt.startKnowledge == VSS_START_UNCERTAIN_FINAL:
    STEP uncertain replay 금지 유지; 정상 Completed/Interrupted로 재해석 없음.
else if 충분한 전체 NO_START이고 과거 실제 시작 없음이 확인됨:
    attempt.startKnowledge = VSS_START_NO_START_CONFIRMED
    STEP VSS_FACT_NO_START_REEVALUATE 통지; 원 Pending/Expiry는 STORE가 원 age로 재판정.
else if 원 전체 Session의 적법 Started가 확인됨:
    STEP 전체 정책 정상 끝이면 VSS_FACT_WHOLE_COMPLETED,
         선점/고장/조건 상실 중단이면 VSS_FACT_WHOLE_INTERRUPTED 통지.
else:
    STEP termination은 과거 no-start가 아님; 정상 재평가/완료 통지 금지.
    STEP 미확정 지식·replay 보호와 [TBD-FINAL] 후속 판정 필요를 남긴다.
STEP 모든 생성 fact는 APPLIED/ALREADY_APPLIED까지 또는 독립 보호된 추적으로 유지.
attempt.retired = true; session.phase = VSS_PLAYBACK_RETIRED
STEP 출력 owner 해제·FLOW 최신 재선택 요청. 통지·ledger·HAL 저장 폐기는 별도.
return 원 cleanup/통지 상태와 반영 완료 관측
```

서로 다른 scope를 합칠 때 근거가 전체 관련 출력을 실제로 덮는지 확인하며, segment 하나를 attempt/Session 전체로 확대하지 않는다. 미래 차단만 충분하고 과거 출력 여부가 불명확하면 replay를 풀지 않는다. 최종화 정책 미정이면 unresolved 지식/보호를 명시하고 안전 확인 밖의 진전을 막는다.

<a id="fact-notification"></a>
## PLAYBACK→STORE 통지 보호

```text
LOCAL STEP / Playback_Advance 내부, 외부 새 Core 아님.
INPUT own VssAttemptContext attempt
for budget 안의 attempt.unappliedFacts[0..unappliedFactCount):
    CALL Store_ApplyPlaybackFact(원 VssPlaybackFact fact) -> VssFactApplication applied
    if applied == VSS_FACT_APPLIED 또는 applied == VSS_FACT_ALREADY_APPLIED:
        STEP 해당 통지 의무만 완료한다.
        STEP STORE ledger가 필요한 정규화 fact를 자기 수명으로 보호했는지 확인한다.
    else:
        STEP 원 fact/identity/time/근거·통지 의무를 그대로 보호; 다음 기회 재적용/진단.
STEP 시작/전체 final/diagnostic 동시 존재를 마지막 progress 하나로 덮지 않는다.
```

<a id="store-applyplaybackfact"></a>
## Store_ApplyPlaybackFact — 발생 이력의 idempotent 적용

```text
CORE Store_ApplyPlaybackFact
INPUT VssPlaybackFact fact
OUTPUT VssFactApplication
OWNER STORE; own VssOccurrenceRecord record

if fact.fact 동일 사실이 원 ledger에 이미 반영됨: return VSS_FACT_ALREADY_APPLIED
if fact.occurrence/원 Session·Attempt/보호된 accepted 추적 연결 불충분:
    STEP 원 적용 부족 진단; return VSS_FACT_NOT_APPLIED
if record.state가 UNCERTAIN_FINAL 또는 기존 whole final이며 late fact가 정상 이력과 충돌:
    STEP 원 final/replay 유지·옛 진단 격리; return VSS_FACT_NOT_APPLIED
    // 처리 완료 표현/격리 보존의 실물 형태는 TBD; 거짓 ALREADY_APPLIED로 ack하지 않음.
switch fact.kind:
    VSS_FACT_FIRST_ACTUAL_START:
        if PLAYBACK의 최초 적법 actual 근거가 확인되지 않음: return VSS_FACT_NOT_APPLIED
        if record.firstStartFact 없음:
            STEP fact를 보호해 record.firstStartFact에 연결.
            record.state = VSS_OCCURRENCE_STARTED
        STEP 같은 적법 최초 fact 중복으로 원본 age/최초 근거 재작성 없음.
    VSS_FACT_WHOLE_COMPLETED 또는 VSS_FACT_WHOLE_INTERRUPTED:
        if record.firstStartFact 없음 또는 전체 안전 final 근거 부족: return VSS_FACT_NOT_APPLIED
        STEP fact를 record.finalFact로 보호; state=COMPLETED 또는 INTERRUPTED.
    VSS_FACT_START_OUTCOME_UNCERTAIN:
        STEP 원 불명확 최종 fact를 record.finalFact로 보호.
        record.state = VSS_OCCURRENCE_UNCERTAIN_FINAL
        STEP replay 금지; 기존 firstStartFact가 있다면 삭제/normal 변환 없음.
    VSS_FACT_NO_START_REEVALUATE:
        if firstStartFact 있음 또는 uncertain final 또는 전체 NO_START/retirement 부족:
            return VSS_FACT_NOT_APPLIED
        STEP 원 identity/age/replay를 보존한 Pending/Expiry 재확인만 허용한다.
        STEP 만료 자체는 [20 store-advancedeadlines]의 원시간 근거로 판단.
record.tracking = VSS_TRACKING_PROTECTED
STEP 해당 사실 적용/record.revision을 반영 완료한 뒤 return VSS_FACT_APPLIED
```

Stateful의 유효 ACTIVE/CLEAR·품질은 이 Core가 다시 쓰지 않는다. 이 통지는 One-shot 발생 이력용이다. `Store_AdvanceDeadlines`는 최신 적용 완료 PB 관측/notificationPending을 먼저 확인하며 pending/unknown/uncertain 원발생을 age만으로 Expired 처리하지 않는다.

## 실패·종료·미정과 추적

| 관측/사건 | 허용 결과 | 계속 막는 것 |
| --- | --- | --- |
| 현재 idle/timeout/WSF 없음/mute | 현재 관측·부족 근거 | 과거 NO_START 확정 |
| OUTPUT_ENDED + future 차단 | 해당 scope termination 근거 | 과거 무출력 추정/segment→whole 확대 |
| 최종 전 적법한 late actual | 원 Started/보고 사실 | 종료 취소·정상 cue 재개 |
| uncertain final 뒤/retired 결과 | old cleanup/diag/refs | 정상 이력 부활·새 owner 변경 |
| old cycle return | 정확한 원작업 회수 근거 | new A/B usage 해제·현재 안전 미확인 쓰기 |
| partial STOP/error | 원차단·retain·정리 지속 | 일반 거부 rollback·자동 refill |

`[TBD-FINAL]`은 불명확 최종 조건/정리 timeout/retry 및 지식 부족의 최종 보고 상세다. `[TBD-WRITE]`는 물리 쓰기 안전/차기 방문/WCET·IRQ/cache 조건이며 [기존 TBD](../90_BINDING/91_IMPLEMENTATION_TBD.md)를 유지한다. final 정책/물리 근거 미확정은 C/보드 성공을 주장할 수 없는 후속 gate지만, 문서에서 unknown을 성공으로 처리하는 이유가 아니다.

[STORE fact R2](../30_FUNCTIONS/20_INPUT_STORE_FUNCTIONS.md#store-applyplaybackfact) · [PB Advance R2](../30_FUNCTIONS/30_POLICY_PLAYBACK_FUNCTIONS.md#playback-advance) · [STREAM/TX R2](../30_FUNCTIONS/40_AUDIO_STREAM_ASSET_FUNCTIONS.md#audiostream-requestcontrol) · [TX Advance R2](../30_FUNCTIONS/50_DRIVER_HAL_FUNCTIONS.md#audiotx-advance) · [Occurrence R3](../40_DATA/20_STORE_DATA.md#vssoccurrencerecord) · [Attempt R3](../40_DATA/40_PLAYBACK_DATA.md#vssattemptcontext) · [Return R3](../40_DATA/60_AUDIO_TX_DATA.md#vsspcmreturnevidence) · [Cleanup R3](../40_DATA/50_AUDIO_STREAM_DATA.md#vssbackendcleanupobservation) · [Async/late R4](../50_CONTRACTS/20_ASYNC_RESULT_EVIDENCE.md#late-results) · [네 경계 R4](../50_CONTRACTS/50_AUDIO_BUFFER_TRANSPORT.md#four-boundaries) · [HAL lifetime R4](../50_CONTRACTS/10_OWNERSHIP_LIFETIME.md#references-and-release) · [PB local](../20_MODULES/23_PLAYBACK_MODULE.md#outcomes)
