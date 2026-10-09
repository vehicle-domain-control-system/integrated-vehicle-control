# Audio Prepare → TX — bounded decode, A/B와 원 출력 근거

> R5 · R4 독립 PASS / R3-C1 세 결정 유지. 전송 경로는 PFlash MP3 → CPU decode → PCM A/B → eDMA → SAI → SGTL5000.

[표기 규칙](00_PSEUDOCODE_OVERVIEW.md#notation) · [STOP/회수](50_STOP_LATE_UNCERTAIN_FLOW.md) · [장치 준비](10_STARTUP_FLOW.md#audiocontrol-service) · [Recovery typed lane](60_FAULT_RECOVERY_FLOW.md#stream-recovery)

## 목적·진입·전제·owner

PLAYBACK의 의미 prepare/START를 AUDIO STREAM이 ASSET/CONTROL/TX에 연결한다. 압축 bytes는 DMA 전송 대상이 아니며 decoder는 STREAM 내부 처리다. STREAM은 PCM usage/생산, TX는 장치 접근·output/operation, HAL은 raw/불변 등록 보호를 쓴다. 타 owner 필드를 직접 갱신하지 않는다. 먼저 보호 공간과 원 귀속을 확보하고 나서 효력이 가능한 하위 요구를 연결한다.

| Core | 입력 → 독립 typed 출력 | writer / 실제 참조 |
| --- | --- | --- |
| `Asset_Read` | AssetReadRequest 또는 원 span 참조 종료 → disposition/span/diagnostic | ASSET / bytes 실제 reader |
| `AudioStream_Prepare` | AudioPreparation → AudioRequestResult/진단 | STREAM / plan, provider, A/B |
| `AudioStream_RequestControl` | PlaybackControlCommand **또는** RecoveryPermission → 원 요구 결과 | STREAM / 두 typed lane |
| `AudioStream_Advance` | 시간·보호된 제어 → request/output/cleanup/diagnostic, recovery facts | STREAM / 하위 사실 적용 |
| `AudioTx_Request` | PcmHandoffCommand **또는** TxControlCommand → TxRequestResult | TX / operation 및 필요 samples |
| `AudioTx_Advance` | 시간·보호된 HAL raw → request/output/consumption/return/diagnostic | TX / 원 software 작업 |
| `AudioHAL_Callback` | 원 보호 문맥 + RawObservation → void | HAL Boundary / raw capture만 |

실제 callback 상관, 장치 출력 경계, A/B 용량/format/alignment, 차기 DMA 방문 전 쓰기 안전성은 B2-R TBD다. 알고 있지 않은 물리 조건은 아래 조건의 **미확인 분기**로 진행하며 confirmed 결과를 만들지 않는다.

<a id="asset-read"></a>
## Asset_Read — 범위 검사와 실제 참조 종료

```text
CORE Asset_Read
INPUT VssAssetReadRequest request OR VssAssetSpanKeyType 원 span + 실제 reader 종료 근거
OUTPUT VssAssetReadDisposition; optional VssAssetSpan span; protected VssDiagnosticEvidence
OWNER ASSET; read-only VssAssetDescriptor descriptor

READ lane:
    STEP request.asset/image의 build descriptor와 metadata를 확인한다.
    if 누락/metadata 불가/다른 image: STEP ASSET 원 lookup/metadata 진단; return VSS_ASSET_ACCESS_REJECTED
    if request.offset > descriptor.compressedLength 또는 범위 계산 overflow:
        STEP ASSET bounds 진단; return VSS_ASSET_ACCESS_REJECTED
    if request.offset == descriptor.compressedLength: return VSS_ASSET_END_OF_RANGE
    STEP 요청량·남은 길이·기존 bounded 공급 한도 안의 길이를 계산한다.
    if 유효 읽기량/참조 보호 공간 없음:
        STEP 기존 span/consumer 참조 보존; return VSS_ASSET_ACCESS_REJECTED
    STEP 해당 PFlash 범위를 read-only 공급한다; 실물 direct/staging 방식 TBD.
    if 확인된 PFlash read 실패: STEP 원 read 진단; return VSS_ASSET_ACCESS_REJECTED
    STEP span.span/asset/image/attempt/offset/validBytes/bytes를 불변 보호한다.
    STEP 실제 decoder/read consumer의 참조 관계를 제공 전에 등록한다.
    return VSS_ASSET_SPAN_GRANTED // 전체 MP3 decode 성공이라는 뜻 아님.
REFERENCE-END lane:
    STEP 원 span과 실제 reader/대기/진행 참조를 대조한다.
    if 귀속 불충분 또는 더 읽을 가능성 남음: return VSS_ASSET_REFERENCE_STILL_PROTECTED
    STEP 해당 span/staging만 회수; 다른 제공 자료나 원 read-only image 변경 없음.
    return VSS_ASSET_REFERENCE_RELEASED
```

retirement/EOF/새 Asset 요청만으로 span을 해제하지 않는다. decode corrupt/unsupported는 STREAM 발견 원인이며 ASSET read 오류로 재라벨링하지 않는다.

<a id="audiostream-prepare"></a>
## AudioStream_Prepare — 원 준비 수용과 bounded prime

```text
CORE AudioStream_Prepare
INPUT VssAudioPreparation preparation
OUTPUT VssAudioRequestResult result; protected 진단
OWNER AUDIO STREAM; own VssProviderContext provider; VssPcmBufferState pcm (A 또는 B)

result.session = preparation.session; result.attempt = preparation.attempt
if 같은 원 preparation 문맥이 취소되었거나 생산 차단/정리 중:
    result.stage = VSS_AUDIO_REQUEST_REJECTED
    STEP 원 참조/차단/정리를 보존; 준비/생산 재개 없음; return result
if 같은 원 preparation이 이미 수용됨:
    STEP 보호된 원본으로 아래 pending 진행을 이어간다; 기한/identity 새로 만들지 않는다.
else:
    if 원 plan/position/asset 불일치 또는 old 자원 안전/보호 공간 불충분:
        result.stage = VSS_AUDIO_REQUEST_REJECTED; return result
    STEP preparation의 plan/position/firstStartMeta와 provider 원귀속을 먼저 보호한다.
    provider.session = preparation.session; provider.attempt = preparation.attempt
    provider.asset = preparation.asset; provider.phase = VSS_PROVIDER_PREPARING
    provider.productionAllowed = true; provider.endOfSource = false
    STEP sourcePosition은 원 Asset 시작 위치; decoder 초기화 형태는 내부/R6 TBD.
    result.stage = VSS_AUDIO_PREPARE_RECEIVED; STEP result 독립 보존.
STEP 기존 Runtime 시간 근거 VssTimeEvidence now를 읽는다.
STEP 필요한 현재 device/config에만 VssDeviceControlIntent PREPARE를 생성·보호한다.
CALL AudioControl_Service(optional 원 device intent, now)
     // 기존 현재 구성 READY를 재사용; 매 Attempt 전체 reset 없음.
STEP VssAssetReadRequest.asset/image/attempt/offset/requestedBytes를 원 읽기 문맥으로 구성.
CALL Asset_Read(request)
if 원 read 실패 또는 STREAM decode/provider 실패 또는 확인된 현재 장치 준비 실패:
    provider.productionAllowed = false; provider.phase = VSS_PROVIDER_CLOSING
    result.stage = VSS_AUDIO_PREPARE_FAILED
    STEP 발견 producer/단계/target/config/operation/occurredAt을 보호하고 원정리를 연결.
    return result // 이미 인계한 PCM/bytes 보호 유지; 자동 fallback/rollback 없음.
STEP 아래 [fill-buffer]를 기존 A/B의 안전한 각 prime 기회에만 수행한다.
if 필요한 초기 유효 PCM/format·provider·현재 장치별 구성·초기 TX 준비가 모두 확인됨:
    provider.phase = VSS_PROVIDER_PREPARED
    result.stage = VSS_AUDIO_PREPARED
else:
    result.stage = VSS_AUDIO_PREPARE_PENDING
STEP 원 준비 단계/진단을 PLAYBACK 적용까지 보호한다.
return result // prepared가 실제 출력 허가나 actual은 아님.
```

유효 PCM이 없거나 형식/장치/prime 근거가 미확정이면 PREPARED를 생성하지 않는다. 짧은 마지막 음원 때문에 B를 임의 채우거나 최소 2개 유효 sound block을 강제하지 않는다. 필요한 초기 준비 조건의 물리 상세는 TBD다.

<a id="fill-buffer"></a>
## AudioStream_FillBuffer — STREAM 내부 local step, Core 19가 아님

```text
LOCAL STEP AudioStream_FillBuffer / 호출: STREAM Prepare 또는 Advance 내부
INPUT own VssProviderContext provider; own VssPcmBufferState pcm; prime/refill 기회
OUTPUT 원 provider/PCM 진행, 필요 handoff/진단; 외부 신규 API 없음.

if !provider.productionAllowed 또는 원 Attempt/Buffer/cycle 불일치: return 후속/정리
if pcm.usage != VSS_PCM_CPU_WRITABLE 또는 지금의 실제 쓰기 안전 근거 미확인:
    return 보호 유지 // 과거 safe return/usage가 무기한 허가증 아님.
STEP 차기 DMA 방문 전 이번 bounded 쓰기의 완료 가능성과 cache/barrier 등 조건 확인.
if [TBD-WRITE] 미확인/안전 구간 경과: return 보호 유지·원 위험 진단
if provider.sourceSpan 없거나 추가 원 압축 bytes 필요:
    CALL Asset_Read(원 provider의 bounded VssAssetReadRequest)
    STEP granted span은 실제 decoder 참조로 보호한다.
STEP CPU decoder가 실제 소비 bytes/생산 frame/format/endOfSource를 반환하도록 bounded 진행.
if read/decode/provider 실패:
    provider.productionAllowed = false; provider.phase = VSS_PROVIDER_CLOSING
    STEP 발견 주체 진단·하위 정리 연결; 실제 refs 종료까지 보존; return 실패
STEP 실제 decoder 소비량만 provider.sourcePosition에 반영한다.
STEP pcm.firstValidFrame/validFrameCount/format/samples의 실제 생성 범위를 기록한다.
STEP consumer가 더 읽지 않는 span만 Asset_Read의 REFERENCE-END lane에 연결한다.
if STOP/권한 상실이 생산 중 반영됨 또는 !provider.productionAllowed:
    STEP 새 sound handoff 금지; 원 span/PCM의 안전 정리로 연결; return
if pcm.validFrameCount == 0:
    STEP 원 EOF/진행 사실만 보호한다; 새 VssPcmHandoffCommand 발행 없음.
    STEP 지속 SAI/eDMA의 정지를 추정하지 않는다. [physical-zero-fill] 참조.
    return 후속/구간 끝 관측
STEP 실제 decode된 0 sample은 위 유효 frame 안의 정상 sound PCM이다.
pcm.usage = VSS_PCM_CPU_READY
STEP 해당 A/B·원 Attempt·새 cycle의 안정된 인계 key를 보호한다. 번호 변경≠물리 안전.
VssPcmHandoffCommand handoff
handoff.key = pcm.key; handoff.samples = pcm.samples
handoff.firstValidFrame = pcm.firstValidFrame; handoff.validFrameCount = pcm.validFrameCount
handoff.format = pcm.format; STEP lastForSegment는 실제 해당 구간 마지막 근거로만 설정.
STEP STOP/쓰기 안전/생산 허용을 인계 직전에 재확인한다.
if 재확인 실패: STEP sound 인계 금지·원 정리; return
pcm.usage = VSS_PCM_HANDOFF_PENDING // DRIVER 호출/부수효력 전에 samples/key/범위 보호.
CALL AudioTx_Request(handoff)
if operation 생성 전 새 요구가 무효력 거부됨:
    STEP 가짜 operation result 없이 원 cycle 무접근 근거/현재 쓰기 조건으로 회수를 검토.
    if 현재 실제 쓰기 안전까지 확인됨: pcm.usage = VSS_PCM_CPU_WRITABLE
    else: STEP 원 pending/보호·정리 유지
    return 원 거부/후속 필요성
VssTxRequestResult result = 원 operation 생성 뒤의 보호된 결과
if result.state == VSS_TX_REJECTED_WITH_NO_ACCESS 그리고 해당 원요구의 실제 무접근/참조 종료 확인:
    STEP 같은 cycle의 현재 쓰기 안전성 재확인 뒤 CPU 회수 가능; 자동 refill 아님.
else:
    STEP 원 인계 보호 유지; 수용/부분 효력에 맞는 HW_PROTECTED 또는 RETURN_PENDING 진행.
return 보호된 단계/원진단 // handoff accepted≠START 활성화.
```

<a id="physical-zero-fill"></a>
### 유효 sound와 물리 zero-fill — C1-01

sound 범위 밖 stale PCM 방지/물리 무음 처리가 필요하면 **기존 A/B 안에서** 현재 실제 CPU 쓰기 안전이 확인된 범위만 정리한다. `validFrameCount`를 늘리거나 tail padding을 sound 길이/반복으로 세지 않는다. 빈 PCM과 실제 decode된 0 sample은 다르다. STOP 뒤에도 `productionAllowed=false`를 유지하며 정리용 zero-fill은 신규 decode/refill/sound handoff 허가가 아니다. 미확인 안전 구간에서는 zero-fill도 쓰지 않는다. zero-fill/현재 무음은 NO_START/TERMINATION/retirement 근거가 아니다. 실물 방법은 B2-R TBD다.

<a id="audiostream-requestcontrol"></a>
## AudioStream_RequestControl — 두 typed lane의 dispatcher와 START

```text
CORE AudioStream_RequestControl // 하나의 논리 Core; 실제 C entry point 수는 R6 TBD.
INPUT VssPlaybackControlCommand command OR VssRecoveryPermission permission;
      최신 실행/안전/현재 허용 관측
OUTPUT 원 typed 요청/차단/진단 또는 Session 없는 복구 진행
OWNER AUDIO STREAM; own VssProviderContext provider

if 입력이 VssRecoveryPermission:
    continue [60 stream-recovery] // command/session/attempt를 fabricate하지 않음.
if command.action == VSS_AUDIO_STOP:
    continue [50 stream-stop] // 미래 생산 차단을 하위 요청보다 먼저.
if command.action != VSS_AUDIO_START: return 원 요청 거부
VssAudioRequestResult result
result.session = command.session; result.attempt = command.attempt
if 원 prepared provider/session/attempt·현재 장치/config·권한/첫 시작 조건 불충분:
    result.stage = VSS_AUDIO_REQUEST_REJECTED
    STEP 기존 하위 부분 효력은 별도 보호/정리; return result
VssAudioPreparation prepared = 해당 원 prepared 요청의 보호된 읽기 자료
STEP 원 START 요구와 prepared.firstStartMeta의 최초 gate 필요 여부/원 Meta를 사전에 보호한다.
VssTxControlCommand tx
tx.attempt = command.attempt; tx.action = VSS_TX_START
STEP tx.scope를 원 요청의 실제 SEGMENT/ATTEMPT 범위에 연결한다.
tx.firstStartMeta = prepared.firstStartMeta // 최초 One-shot 원본 참조; 정상 후속은 null.
CALL AudioTx_Request(tx)
if operation 생성 전 무효력 거부:
    result.stage = VSS_AUDIO_REQUEST_REJECTED
else:
    VssTxRequestResult txResult = 원 TX operation의 보호된 결과
    if txResult.state == VSS_TX_REJECTED_WITH_NO_ACCESS:
        result.stage = VSS_AUDIO_REQUEST_REJECTED
    else if txResult.state == VSS_TX_ACCEPTED_AND_ENABLED:
        result.stage = VSS_AUDIO_START_ACCEPTED_AND_ENABLED
    else if txResult.state == VSS_TX_EFFECT_UNCERTAIN:
        result.stage = VSS_AUDIO_EFFECT_UNCERTAIN
    else if 실제 원 하위 요구 제출/queue 접수가 확인됨:
        result.stage = VSS_AUDIO_START_SUBMITTED
    else:
        result.stage = VSS_AUDIO_CONTROL_RECEIVED // 제출 미확인을 SUBMITTED로 만들지 않음.
STEP 실제로 함께 확인된 submitted/accepted·partial 사실은 각각 독립 보호해 Advance에 연결.
STEP 이 어떤 반환도 actual 출력·NO_START·정리를 자동 증명하지 않는다.
return result
```

<a id="audiostream-advance"></a>
## AudioStream_Advance — 하위 적용과 Backend 충분조건 결합

```text
CORE AudioStream_Advance
INPUT VssTimeEvidence now; 보호된 STREAM prepare/control/recovery 문맥
OUTPUT 독립 VssAudioRequestResult / VssAudioOutputEvidence / VssBackendCleanupObservation /
       VssDiagnosticEvidence / VssRecoveryPerformedEvidence / VssRecoveryVerificationEvidence
OWNER AUDIO STREAM

CALL AudioTx_Advance(now)
CALL AudioControl_Service(optional 기존 원 device intent 또는 진행만, now)
STEP Session 없는 초기 장치 관측과 진단도 반환한다. [10 startup]
for 이번 budget의 원 lower 사실:
    if 현재 provider/PCM cycle과 다른 원귀속:
        STEP [50 stream-safe-return]의 old 정리/진단만; 현재 usage/생산/ready 변경 없음.
        continue
    STEP 요청 단계·source consumption·safe return·output를 각각 보호/적용한다.
    STEP returned만으로 생산 허용을 복원하지 않는다; [50 stream-safe-return]의 현재 쓰기 검사.
if 원 pending preparation이고 생산 차단 안 됨:
    STEP AudioStream_Prepare의 동일 원본 bounded 진행을 이어간다.
if 현재 원 provider가 정상 후속 생산 가능:
    STEP 현재 쓰기 안전 A/B에만 [fill-buffer] local step 수행.
if read/decode/start/output/관측 손실이 새 실패/정리 사유:
    STEP 원진단 보호; 해당 provider의 미래 생산/제출 차단·원 lower cleanup 요구.

for 해당 scope의 보호된 VssTxOutputEvidence transport:
    VssAudioOutputEvidence output
    STEP output.session/attempt/scope/transportFacts/transportFactCount를 원 불변 근거에 연결한다.
    STEP output.futurePcmBlocked는 해당 원 scope의 실제 미래 PCM 차단 근거로 설정한다.
         actual만 확인한 경우 false이며 차단 미확인을 true로 합성하지 않는다.
    if transport.kind == VSS_TX_NEW_OUTPUT_OBSERVED 그리고 원 새 출력 충분성 확인:
        output.kind = OUTPUT_START_CONFIRMED
        STEP 실제 출력 사실 반환; 상위 적법성 판정은 PLAYBACK에 남긴다.
    else if 원 scope의 충분한 PAST_NO_OUTPUT_PROVEN && FUTURE_OUTPUT_BLOCKED 근거 있음:
        if 해당 scope의 future decode/refill/PCM handoff가 실제 차단됨:
            output.kind = NO_START_CONFIRMED; output.futurePcmBlocked = true
        else: STEP no-start confirmed 생성 없이 차단/정리 진행.
    else if 원 scope의 충분한 OUTPUT_ENDED && FUTURE_OUTPUT_BLOCKED 근거 있음:
        if 해당 scope의 future PCM 생산/제출 차단됨:
            output.kind = OUTPUT_TERMINATION_CONFIRMED; output.futurePcmBlocked = true
        else: STEP termination confirmed 생성 없이 정리 진행.
    else:
        STEP coverage 부족/손실과 실제 지식 범위를 반환; 자동 confirmed/final 없음.
    STEP 위 충분조건에서 kind가 완성된 output만 PLAYBACK에 전달하고 적용/보호 추적까지 독립 유지.
         kind 미완성은 typed output으로 내보내지 않고 원 transport 사실·부족/손실 진단·후속을 보호한다.
STEP cleanup의 providerReferencesEnded/pcmAccessEnded/lowerOutputBlocked/lateIdentityProtected를
     각각 실제 해당 참조/차단/보호 근거로만 생성한다. 한 조건으로 나머지 true 합성 없음.
STEP 복구 수행/별도 검증은 [60] typed 원허용 경로로 반환한다.
return 독립 보호된 사실들; 상위 역호출/STORE ledger/HEALTH clear 없음.
```

<a id="audiotx-request"></a>
## AudioTx_Request — PCM 인계와 주소 없는 TX 제어

```text
CORE AudioTx_Request
INPUT VssPcmHandoffCommand handoff OR VssTxControlCommand control
OUTPUT VssTxRequestResult result; protected 원진단
OWNER AUDIO TX; own VssTxOperation operation

PCM lane:
    if handoff.validFrameCount == 0 또는 안정 samples/format/유효 범위/원 key 근거 불충분:
        STEP 이 새 요구의 무접근 거부 근거만 생성; 기존 operation retain 해제 없음.
        return operation 생성 전 무효력 거부 의미 [TBD-RETURN]
    STEP 원 Attempt/A 또는 B/cycle·bounded firstValidFrame+validFrameCount overflow 확인.
    STEP HANDOFF_PENDING 보호된 samples/범위/format를 실제 consumer 수명으로 보존한다.
    operation.attempt = handoff.key.attempt
    operation.handoff = handoff의 보호된 불변 참조
    STEP operation.scope는 원 PCM 요청 범위에서 유지한다.
CONTROL lane:
    if control.action이 START/STOP 아님 또는 원 Attempt/scope 근거 불충분:
        STEP 이 새 요구 거부/진단; 기존 보호 유지; return
    operation.attempt = control.attempt; operation.scope = control.scope
    operation.handoff = null // 제어에 PCM 주소를 강제하지 않는다.
    if control.action == VSS_TX_START && control.firstStartMeta exists:
        STEP 원본 기한/비교 문맥/현재 gate를 하위 활성화 전 재확인.
        if fresh 조건 확인 불가: STEP 신규 START 거부; 기존 정리/근거는 유지; return

COMMON:
    if 같은 원 요구가 이미 제출됨: STEP 기존 결과/후속 진행; HW 동작 중복 발행 없음; return
    if 새 operation/개별 HAL 귀속 보호 공간 없음:
        STEP 신규 요구만 무효력 거부; 기존 accepted/미반영 facts 보호; return
    STEP 불변 operation.operation/attempt/scope와 필요 handoff/원 Meta를 효력 전에 보호한다.
    STEP HAL의 기존 내부 등록 경계에서만 아래 기록을 생성한다 (신규 Core/API 아님):
        VssHalTxRegistration reg
        reg.operation = operation.operation; reg.attempt = operation.attempt
        reg.pcm = operation.handoff
        STEP HAL이 개별 reg.registration/binding을 보호한다.
    STEP TX는 보호된 HAL 기록의 원귀속 확인만 받는다.
    operation.activationAt = null
    STEP 효력 가능성이 불명확한 요구 동안 deviceAccessPossible/futureOutputPossible을
         보수적으로 보호한다; 실제 충분 차단/무접근 거부 뒤에만 해당 작업의 가능성을 해제.
    STEP 원효력과 callback가 보호 문맥에 연결될 수 있을 때만 기존 HAL 요구를 연결한다.
    operation.requestState = VSS_TX_SUBMITTED_OR_QUEUED
    STEP vendor return과 실제 retain/activation/미래 접근·출력 가능성은 별도로 확인한다.
    if 원 START의 수락과 실제 활성화가 둘 다 확인됨:
        operation.requestState = VSS_TX_ACCEPTED_AND_ENABLED
        STEP 원 activationAt을 실제 VssFactTime으로 보호; actual은 아직 별도.
    else if 이 새 요구가 어떤 접근/효력도 없었음이 충분히 확인됨:
        operation.requestState = VSS_TX_REJECTED_WITH_NO_ACCESS
        STEP 해당 원요구의 refs만 안전 회수; 다른 과거 operation 참조 유지.
    else if 실패 반환/부분 활성화/retain/관측 불명확:
        operation.requestState = VSS_TX_EFFECT_UNCERTAIN
        STEP 가능한 deviceAccessPossible/futureOutputPossible과 보호/cleanup을 유지한다.
    else if STOP 정리 진행:
        operation.requestState = VSS_TX_CLEANUP_PENDING
    STEP result.operation/attempt/state를 원작업에 연결해 독립 보호한다.
    return result // HAL 등록 폐기·PCM 쓰기 권한 부여·actual 판정 아님.
```

operation 생성 전 거부에는 가짜 operation을 넣은 typed result를 만들지 않는다. 생성 뒤에만 result.operation/attempt/state를 완성한다. early rejection과 optional result의 실제 C 반환 형태는 [TBD-RETURN] R6 TBD다. 같은 원요구 판별·operation identity 전달의 실제 C 형태도 R6 TBD다. PCM handoff의 accepted/enabled도 새 sound START의 활성화 증거를 대신하지 않는다. 무접근 거부가 확인된 해당 새 요구와 이미 효력 있는 옛 요구를 구별한다.

<a id="audiotx-advance"></a>
## AudioTx_Advance — raw → 개별 작업의 충분 근거

```text
CORE AudioTx_Advance
INPUT VssTimeEvidence now; 원 software operation; HAL 보호 경계의 VssRawObservation raw
OUTPUT VssTxRequestResult / VssTxOutputEvidence / VssPcmConsumptionEvidence /
       VssPcmReturnEvidence / VssDiagnosticEvidence (각 독립 보호)
OWNER AUDIO TX; own VssTxOperation operation

for 이번 budget 안의 raw:
    STEP 원 registration/binding과 실제 boundary·시각·source/처리 문맥을 대조한다.
    if 지속 vendor stream raw↔개별 operation/PCM cycle 대응이 입증되지 않음:
        STEP VSS_COVERAGE_INSUFFICIENT 또는 실제 누락이면 VSS_COVERAGE_LOST를 유지.
        STEP 불확실 진단/보호/cleanup 추적; 현재 Attempt로 재라벨 금지; continue
    STEP 원 operation에만 적용한다; request/activation stage와 실제 출력은 독립.
    VssTxOutputEvidence evidence
    STEP evidence.operation/attempt/scope/occurredAt/coverage를 원 근거로 보호한다.
    if 실제 활성화 이후 새 해당 출력 boundary 관측이며 coverage 충분:
        evidence.kind = VSS_TX_NEW_OUTPUT_OBSERVED
    else if 필요한 과거 실제 무출력 기간/범위가 충분히 관측됨:
        evidence.kind = VSS_TX_PAST_NO_OUTPUT_PROVEN // 현재 idle/WSF 없음만으로 생성 금지.
    else if old request/active work/descriptor/FIFO/frame의 미래 출력 불가 확인:
        evidence.kind = VSS_TX_FUTURE_OUTPUT_BLOCKED
    else if 해당 scope의 실제/잔류 출력 종료 확인:
        evidence.kind = VSS_TX_OUTPUT_ENDED // 과거 무출력 증명 아님.
    else:
        evidence.kind = VSS_TX_OUTPUT_UNCERTAIN // 단계/부족/손실 보존.
    STEP 동시에 존재하는 각 증거를 독립 보존한다; else-if가 다른 사실 삭제를 의미하지 않음.
    if 원 PCM source 소비 범위가 확인됨:
        VssPcmConsumptionEvidence consumption
        STEP operation/key/firstFrame/frameCount/occurredAt을 해당 원 cycle로 보호한다.
        STEP 소비만으로 safe return/OUTPUT_ENDED 생성 없음.
    STEP 안전 반환은 [50 tx-safe-return] 조건을 별도로 판정한다.
STEP 필요한 old cleanup을 기존 HAL 경계로 진행; 실패/partial에도 보호 유지.
STEP operation/device future 가능성은 실제 충분한 차단/참조 종료로만 해제한다.
STEP HAL 기록은 HAL의 독립 폐기 조건; TX 결과 참조는 STREAM 적용/보호 추적까지 유지.
return 보호된 사실들 // STREAM CPU usage나 PLAYBACK Session을 직접 쓰지 않음.
```

HALF/MAJOR·WSF/FEF/EOF, vendor abort 반환을 위 충분성 조건의 자동 true로 쓰지 않는다. 물리 boundary가 미입증이면 confirmed 근거 생성 경로가 막힌다. 과거 무출력, 미래 출력 차단, 잔류 출력 종료는 독립 사실이므로 동시에 있으면 전부 보호한다.

<a id="audiohal-callback"></a>
## AudioHAL_Callback — 짧은 raw capture, 원 귀속 부족도 보존

```text
CORE AudioHAL_Callback
INPUT 실제 callback/IRQ의 보호된 원 등록 문맥; VssRawObservation raw
OUTPUT void
OWNER HAL Boundary (HAL/BSP Layer; Module 11 추가 없음)

STEP 안전하게 읽을 수 있는 실제 vendor raw와 당시 발생/포착 시간·coverage를 짧게 capture.
if 개별 원 VssHalTxRegistration 또는 VssHalDeviceRegistration 대응이 확인됨:
    STEP raw.registration을 그 원 불변 key로 연결한다.
else:
    raw.registration = null
    STEP 귀속 부족이면 raw.coverage.validity=VSS_COVERAGE_INSUFFICIENT,
         누락/overflow/연속성 손실이면 VSS_COVERAGE_LOST; 현재 Attempt 조회 없음.
STEP raw.rawFact/occurredAt/capturedAt/coverage와 원등록 참조를 기존 보호 경계에 보존한다.
if 보존 한계/저장 실패:
    STEP 중요한 사실 손실을 별도 observable coverage/진단 근거로 남긴다. [TBD-CAPACITY]
STEP 필요한 HW acknowledge와 최소 Runtime 후속 기회 알림; return
// decode/refill/PCM 복사/동적 할당/정책/복구/Core 역호출 없음.
```

### 지속 vendor 등록과 불변 개별 기록 — C1-02

`VssHalTxRegistration`은 개별 software TX operation의 보호 기록이다. 하나의 `Sai_Ip_Send` 기반 지속 vendor stream과 1:1이 아니다. stream은 여러 Session/Attempt를 가로지를 수 있다. 매 Attempt vendor 재등록, vendor callback token 존재, raw마다 최신 Attempt를 붙이는 방식은 확정하지 않는다. 기존 지속 stream과 개별 기록은 각각 HW·대기·처리 참조/late 대응이 끝날 때까지 독립 보호한다. 실제 상관이 부족하면 결과 확인을 막고 INSUFFICIENT/LOST를 반환한다.

## 실패·후속·자원·종료·TBD와 추적

Prepare/Advance는 원 공간/기회 안에서 bounded 진행하고 RX 없어도 후속된다. ASSET bytes 실제 읽기 종료, PCM source 소비, 안전 반환, 출력 종료, Session retirement, HAL 기록 폐기는 서로 다른 경계다. STOP/partial/late 회수는 [50](50_STOP_LATE_UNCERTAIN_FLOW.md#stream-safe-return)에 이어진다.

`[TBD-WRITE]` 차기 DMA 방문 전 안전 구간·refill WCET/IRQ 지연·메모리 조건, `[TBD-CAPACITY]` bounded 저장/직렬화·key alias·손실 표시 수단, vendor raw↔operation 물리 상관/activation/출력·무출력·종료, format/용량, 실제 API/ABI는 [기존 TBD](../90_BINDING/91_IMPLEMENTATION_TBD.md#memory). R5는 이를 실측 PASS로 주장하지 않는다.

[STREAM/ASSET R2](../30_FUNCTIONS/40_AUDIO_STREAM_ASSET_FUNCTIONS.md) · [TX/HAL R2](../30_FUNCTIONS/50_DRIVER_HAL_FUNCTIONS.md) · [Asset/provider R3](../40_DATA/50_AUDIO_STREAM_DATA.md#vssprovidercontext) · [PCM R3](../40_DATA/50_AUDIO_STREAM_DATA.md#vsspcmbufferstate) · [TX typed R3](../40_DATA/60_AUDIO_TX_DATA.md#vsstxcontrolcommand) · [HAL R3](../40_DATA/60_AUDIO_TX_DATA.md#vsshaltxregistration) · [Buffer R4](../50_CONTRACTS/50_AUDIO_BUFFER_TRANSPORT.md#continuous-stream) · [Evidence R4](../50_CONTRACTS/20_ASYNC_RESULT_EVIDENCE.md#evidence-basis) · [STREAM local](../20_MODULES/24_AUDIO_STREAM_MODULE.md#pcm) · [TX local](../20_MODULES/30_AUDIO_TX_MODULE.md#evidence) · [HAL lifetime](../10_LAYERS/details/40_HAL_BSP_BOUNDARY.md#lifetime)
