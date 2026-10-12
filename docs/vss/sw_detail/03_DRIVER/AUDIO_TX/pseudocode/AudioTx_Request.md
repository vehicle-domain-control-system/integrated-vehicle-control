# AudioTx_Request — 한국어 C형 의사코드

[상세 설명](../functions/AudioTx_Request.md) · [AUDIO_TX](../00_MODULE_OVERVIEW.md) · [VSS 지도](../../../01_VSS_MAP.md)

> CORE-13 · D5-1 SELF_CHECKED / 독립 부분 검수 대기. 아래 조건·서술형 연산은 논리 의사코드이며 새 public API·자료 타입·실제 C ABI가 아닙니다.

<a id="contract"></a>
## 1. R6 공개 경계

```text
VssTxRequestState AudioTx_Request(
    ONE_OF const VssPcmHandoffCommand *pcm,
           const VssTxControlCommand *control,
    OPTIONAL IN const VssOperationKeyType *continuingOperation,
    IN const VssOperationKeyType targetOperations[count],
    IN const VssTimeEvidence *time,
    OUT VssTxRequestResult requests[capacity -> written],
    OUT VssPcmReturnEvidence returned[capacity -> written],
    OUT VssDiagnosticEvidence diagnostics[capacity -> written]);
```

[R6 원문](../../../../design/70_C_INTERFACE/00_HEADER_OWNERSHIP_MAP.md#r6-audiotx-request)의 순서와 scalar/독립 3 OUT를 보존합니다. 별도 pcm/control 중 정확히 하나입니다. continuingOperation은 실제 기존 operation의 의미 key이며 NULL은 진짜 신규·효력 전만 허용합니다. PCM/START는 targetOperations count=0, STOP은 원래 실제 대상 key 목록입니다. 실제 optional/길이/written/메모리 표현은 Binding TBD입니다.

<a id="notation"></a>
## 2. 서술형 조건과 원본

`원 작업`은 [VssTxOperation](../../../../design/40_DATA/60_AUDIO_TX_DATA.md#vsstxoperation), `원등록`은 HAL Boundary가 보호하는 [VssHalTxRegistration](../../../../design/40_DATA/60_AUDIO_TX_DATA.md#vsshaltxregistration)입니다. `보호 확보`, `동일 요구 대조`, `충분조건 확인`, `군별 전달`은 아래 명시한 조건의 약칭입니다. 새 helper 함수·Queue·Task·pool을 정의하지 않습니다. 읽기 가능성·직렬화·bounded 저장 방식은 후속 Binding에서 실증합니다.

STREAM은 PCM key/usage를 소유하고 호출 전 HANDOFF_PENDING으로 보호합니다. STOP 전에 관련 Session의 모든 Attempt productionAllowed=false를 적용합니다. TX는 그 원본을 대신 쓰지 않습니다. HAL은 짧은 raw 포착, PLAYBACK은 전체 최종/retirement, HEALTH는 Fault를 담당합니다.

<a id="r1-storage"></a>
## R1. 입력 접근과 독립 OUT 준비

```c
// 각 군의 storage가 읽기/쓰기 가능한지 먼저 확인한다.
// 무효 storage의 capacity/written을 읽거나 쓰지 않는다.
for (각 OUT군 requests, returned, diagnostics를 독립적으로) {
    if (해당 storage 유효) {
        해당 군의 용량/범위를 안전하게 확인;
        해당 군의 written = 0;
    } else {
        해당 군에 접근하지 않고 원래 미전달 사실 보호 유지;
    }
}
// 입력 포인터/목록 읽기 유효성 확인 뒤에만 원 필드를 읽는다.
if (ONE_OF 0개 또는 2개, 입력/목록 범위 무효, time의 접근 불가) {
    새 하위 효력 금지;
    R2의 신규/기존 구별로 반환 단계 결정;
    R8의 유효한 diagnostics 및 보호된 후속만 처리;
}
```

한 군이 무효/가득 차도 다른 유효한 군은 진행합니다. 새 효과의 후속 사실을 보호할 제한된 공간이 없다면 새 수용을 제한합니다. 기존 사실의 삭제·무제한 저장으로 해결하지 않습니다.

<a id="r2-identity"></a>
## R2. 진짜 신규와 continuingOperation

```c
if (continuingOperation != NULL) {
    안전한 원 key 조회 및 원 command/Attempt/PCM key 또는 제어 범위 대조;
    if (실제 key/동일 요구 확인 불가) {
        옛 작업·참조·기존 정리 보호 유지; 새 key 생성/하위 재제출 금지;
        확인된 원 작업의 현재 단계 유지;
        효과 미확인 범위는 VSS_TX_EFFECT_UNCERTAIN;
        이미 진행 중인 정리의 VSS_TX_CLEANUP_PENDING 유지;
        미확인 key로 requests/returned 생성 금지; 원 진단은 R8;
        return 위의 원 범위 단계; // 새 무접근 완료로 바꾸지 않음
    }
    이미 제출한 동일 요구의 하위 호출을 반복하지 않음;
    보호된 원 사실의 후속/전달을 R8에서 처리;
    return 원 작업의 현재 requestState;
}
if (같은 원 요구가 이미 진행 중이거나 key가 불명확) {
    NULL을 새 요구로 간주하지 않음; 보호된 조회/후속·진단 유지;
    하위 재제출·가짜 operation 금지;
    return 확인되는 기존 단계 또는 VSS_TX_EFFECT_UNCERTAIN;
}
// 실제 새 요구이며 아직 operation/효력 없음. R3~R5 검사 후 R6.
if (신규 검사 실패 또는 필요한 보호 확보 불가) {
    하위 호출/operation 생성 없음; 유효 requests/returned written = 0;
    원 발견 진단만 R8; 가짜 Result/ReturnEvidence 없음;
    return VSS_TX_REJECTED_WITH_NO_ACCESS; // 새 요구 무효과가 확인됨
}
```

동일성은 원 operation과 불변 요구를 기준으로 합니다. 같은 Attempt/action/scope만으로 서로 다른 SEGMENT를 중복 제거하지 않습니다. key lookup·alias/wrap·후속 전달 식별 수단은 R6 기존 TBD입니다. 신규 거부는 같은 주소의 옛 작업 참조를 해제하지 않습니다.

<a id="r3-pcm"></a>
## R3. PCM lane

```c
if (pcm != NULL) {
    targetOperations count == 0인지 검사;
    원 key = Attempt / A 또는 B / 해당 cycle 전체로 확인;
    stable HANDOFF_PENDING 인계와 samples 참조/format 확인;
    validFrameCount > 0인지 검사;
    firstValidFrame와 validFrameCount를 overflow 없이 실제 span과 대조;
    if (어느 조건이라도 불충분) R2 신규 무효과 거부;
    실제 짧은 유효 구간/lastForSegment만 보호; 전체 용량 padding 금지;
}
```

원음의 0 sample은 유효 PCM입니다. 정리용 zero-fill·빈 B는 신규 유효 frame이 아닙니다. lastForSegment/EOF는 Session 끝이 아닙니다. const 인자의 얕은 저장 대신 실제 samples·불변 인계는 HW/대기/처리 참조 종료까지 보호합니다. 같은 A 주소의 옛 key로 새 A/B usage를 풀지 않습니다.

<a id="r4-start"></a>
## R4. 제어 lane·START gate

```c
if (control != NULL) {
    attempt/action/scope/firstStartMeta 4필드의 원 의미 검사;
    action은 START 또는 STOP; scope는 SEGMENT 또는 ATTEMPT;
    제어는 PCM 주소 없음; 새 제어 operation의 handoff = NULL;
    if (control.action == START) {
        targetOperations count == 0인지 검사;
        원래 문맥의 새 START 권한/현재 gate를 효력 직전에 확인;
        if (최초 One-shot gate 필요) {
            보호할 원래 firstStartMeta와 time의 비교 가능한 연속성 확인;
            원발생 age < 2초 [잠정] 검사; 정확히 2초/부족은 신규 거부;
        } else {
            원래 최초 Started 확인된 정상 후속 등 면제 문맥을 확인;
            firstStartMeta가 NULL인지 검사; 최초 age gate 재적용 없음;
        }
        STOPPING/ABORTING·uncertain 최종/retired 권한 부활 금지;
    }
}
```

Meta NULL만으로 최초 gate 면제를 만들지 않습니다. now로 원 age를 리셋하지 않습니다. 제출/수용 시각과 실제 활성화 activationAt, actual의 원 사건 시각은 다릅니다. Network USE_LIMIT 후보와 임의 timeout을 만들지 않습니다.

<a id="r5-stop"></a>
## R5. STOP targets·원 scope

```c
if (control != NULL && control.action == STOP) {
    firstStartMeta = NULL인 원 요구 확인;
    STREAM의 Session-wide 생산/제출 선차단 전제 보존;
    targetOperations의 실제 보호된 원 key들을 대조;
    각 원 operation/Attempt·scope·구간·descriptor/pending/FIFO/frame 확인;
    if (빈 목록 또는 귀속/범위/전체 coverage 불명확) {
        전체 STOP 완료 근거 생성 금지; 미확인 옛 작업 보호/진단/후속;
        안전하게 적용할 원 대상도 확인 불가하면 진짜 신규 거부는 R2;
        확인된 subset 정리는 그 실제 범위로만 보호/진행;
    }
    // 처리 가능한 보호된 subset도 그 범위만 정리한다.
    미방문/범위 미확인 target을 유지하고 완료 범위를 확대하지 않음;
    동일 SEGMENT enum을 전체/다른 구간으로 합치지 않음;
}
```

새 STOP 제어 operation의 단계와 대상 옛 operation의 접근/출력 종료는 별개입니다. ACK·abort·IRQ disable·현재 idle로 과거 무출력/출력 끝/PCM 반환을 만들지 않습니다. TX가 STREAM 생산 원본이나 PLAYBACK Session 완료를 쓰지 않습니다.

<a id="r6-protect"></a>
## R6. 제한된 보호 확보 → 원 operation/등록 → 하위 효력

```c
원 command/Meta·samples 또는 원 target key와 후속 사실 보호 공간 확보;
if (효력 전 보호 확보 실패) R2 신규 거부; // 기존 기록 eviction 없음
실제 개별 VssTxOperation의 operation/attempt/scope/handoff 보호;
requestState = VSS_TX_SUBMITTED_OR_QUEUED;
activationAt = NULL; 미확인 deviceAccessPossible/futureOutputPossible 보수적 유지;
HAL Boundary가 registration/operation/attempt/pcm/binding을 불변 보호;
if (등록 보호 미완료) {
    retain/arm/제출 금지; 실제 생성된 작업은 원 수명/무접근 범위로 추적;
    R8에서 실제 작업 결과와 충분한 경우만 PCM 반환;
} else {
    기존 HAL 경계에서 허용된 원 요구의 하위 retain/arm/제출;
    // 반환 전 즉시 callback도 사전 보호된 원등록을 사용.
}
```

ISR은 raw·원등록·occurredAt/capturedAt·coverage/손실을 짧게 보호하고 알립니다. ISR에서 AudioTx_Request/AudioTx_Advance 호출·decode/refill·상위 적용을 하지 않습니다. Request는 callback raw를 후속 적용하지 않고 [승인 Advance](AudioTx_Advance.md#procedure)에 맡깁니다. vendor 지속 스트림은 Attempt 경계를 넘을 수 있으며 매 START마다 재시작하거나 software 귀속과 vendor 등록을 1:1로 고정하지 않습니다.

<a id="r7-effect"></a>
## R7. 동기 반환과 부분효과

```c
if (원 정리 요구가 수락/진행 중 && 실제 정리 아직 미확인) {
    requestState = VSS_TX_CLEANUP_PENDING; // STOP ACK는 종료가 아님
} else if (원 요구의 수락과 실제 활성화 근거 확인) {
    requestState = VSS_TX_ACCEPTED_AND_ENABLED;
    activationAt = 실제 활성화 시각/가능 구간; // now 합성 금지
} else if (제출/대기만 확인) {
    requestState = VSS_TX_SUBMITTED_OR_QUEUED;
} else if (그 실제 작업의 무접근/접근 종료 충분조건 확인) {
    requestState = VSS_TX_REJECTED_WITH_NO_ACCESS;
} else {
    requestState = 기존 정리 진행이면 VSS_TX_CLEANUP_PENDING,
                   그 외 부분효과/효과 확인 불가는 VSS_TX_EFFECT_UNCERTAIN;
    실제 operation/원등록/samples/미래 접근·출력·결과 추적 보호 유지;
}
// 어떤 하위 실패라도 일반 신규 거부 rollback으로 옛 보호를 풀지 않음.
```

accepted/enabled는 actual sound start가 아닙니다. STOP 처리 단계는 targets의 실제 종료 근거를 대체하지 않습니다. activationAt 없는 사실에 현재 시각을 채우지 않습니다.

<a id="r8-publish"></a>
## R8. 실제 사실과 세 OUT의 개별 전달

```c
if (실제 원 operation 생성됨) {
    그 operation/attempt/state의 불변 VssTxRequestResult 보호;
}
if (실제 PCM operation이며 원 operation/key 대응 확인 &&
    모든 해당 HW/대기/처리 참조 종료 && 옛 내용 재접근/재소비 차단 &&
    원 scope/기간/binding의 basis.validity == VSS_COVERAGE_SUFFICIENT) {
    VssPcmReturnEvidence에 원 operation/key/noFutureAccess=true/basis 보호;
}
// 생성 전 신규 거부에는 typed 작업/반환 기록 없음.
// handoff=NULL 제어 operation의 반환 없음; 옛 target PCM은 Advance 후속.
실제 발견 producer/stage/cause/target/configuration/occurredAt 진단 보호;
진단 operation은 확인된 실제 원 key만 연결; 없으면 없음;
for (각 군을 독립적으로) {
    if (storage 유효 && 해당 군에 용량 있음 && 보호된 미전달 해당 사실 있음) {
        해당 사실 전달; 그 군의 written만 증가;
        소비자 반영/보호 추적 종료까지 사실·참조 유지;
    } else {
        그 군의 사실/참조 보호; 다른 군은 계속;
    }
}
return 원래 요구/후속의 requestState;
```

returned full이어도 requests/diagnostics는 진행합니다. requests invalid이어도 diagnostics valid이면 진단 전달 가능합니다. 여러 요청/반환/진단의 각 count는 독립이며 union·마지막 슬롯로 합치지 않습니다. count=0과 written 전달은 완료/반영 ACK가 아닙니다. 이미 전달·보호 추적 중인 동일 사실은 Advance가 중복 새 사실로 만들지 않고, 후속 새 변화는 별도 보호합니다. 저장/반영 추적 방법은 Binding TBD입니다.

<a id="r9-followup"></a>
## R9. late·부족·손실과 종료 후 보호

VSS_COVERAGE_INSUFFICIENT(관측/상관 부족)와 VSS_COVERAGE_LOST(귀속·시각·저장 손실)를 구분해 유지합니다. 원 key/시각 미확인에 현재 key/now를 채우지 않습니다. OUT full/등록 보호 공간 부족은 새 진행 제한과 보호된 후속으로 처리하며 사실을 조용히 삭제하지 않습니다.

최종 전 원 Attempt의 적법한 late actual을 후속 적용해도 STOPPING/ABORTING 유지, ACTIVE/새 cue 재개 없음입니다. uncertain 최종 뒤·retired 결과는 옛 정리/진단/참조 종료만 담당합니다. 새 Session·PCM 회차·ready/Fault에 옛 성공을 적용하지 않습니다. PCM 반환은 옛 회차만 증명하며 STREAM 현재 safe-write·생산 권한, 출력 종료, HAL 기록 참조 종료, Session retirement는 각각 별도 확인합니다.

<a id="postconditions"></a>
## 3. 종료 조건과 가능한 후속

유효 OUT는 전달한 개별 사실 수만 기록합니다. 미전달 사실과 남은 참조는 보호된 원 문맥으로 Advance 후속에 남습니다. 신규 거부는 해당 새 요구만 무효과이며 기존 작업의 보호는 유지됩니다. 부분효과/정리 대기는 실제 key와 원 범위를 보존합니다. 반환 scalar로 actual/no-start/전체 종료·무기한 CPU 쓰기 허가를 발급하지 않습니다.

배타·bounds·중복·회차/late·군별 용량은 F1/F3 독립 구현/비구동 후보입니다. 원발생→최초 actual `<2초`는 F2 잠정값이며 현재 gate와 사실 적용을 분리합니다. 지속 raw 대응/PCM 재쓰기/STOP 잔류·반환 충분성은 F5 물리 Gate입니다. 구체 ABI·key/저장·직렬화는 기존 TBD, 실제 symbol 재사용은 미조사입니다. 후속 Host 후보와 team/HW 결정은 [설명 구현 가능성 표](../functions/AudioTx_Request.md#feasibility)에 남기며 이번 실제 시험은 NOT_RUN입니다.

<a id="references"></a>
## 4. 동결 원문과 승인 후속

- [설명·입력](../functions/AudioTx_Request.md#inputs) · [설명·신규/후속](../functions/AudioTx_Request.md#new-and-continuing) · [설명·3 OUT](../functions/AudioTx_Request.md#outputs) · [설명·반환](../functions/AudioTx_Request.md#returned)
- [R2 Caller/Callee](../../../../design/30_FUNCTIONS/50_DRIVER_HAL_FUNCTIONS.md#audiotx-request) · [R3 PCM/제어](../../../../design/40_DATA/60_AUDIO_TX_DATA.md#vsspcmhandoffcommand) · [회차](../../../../design/40_DATA/50_AUDIO_STREAM_DATA.md#vsspcmcyclekey) · [진단](../../../../design/40_DATA/70_DIAGNOSTIC_DATA.md#vssdiagnosticevidence)
- [R4 소유권](../../../../design/50_CONTRACTS/10_OWNERSHIP_LIFETIME.md#late-attribution) · [Async](../../../../design/50_CONTRACTS/20_ASYNC_RESULT_EVIDENCE.md#late-results) · [Timing](../../../../design/50_CONTRACTS/30_TIMING_FRESHNESS.md#first-start) · [Buffer safe-write](../../../../design/50_CONTRACTS/50_AUDIO_BUFFER_TRANSPORT.md#safe-write)
- [R5 Request](../../../../design/60_PSEUDOCODE/40_AUDIO_PREPARE_TX_FLOW.md#audiotx-request) · [STOP/반환](../../../../design/60_PSEUDOCODE/50_STOP_LATE_UNCERTAIN_FLOW.md#tx-safe-return) · [R6 typed TX](../../../../design/70_C_INTERFACE/00_HEADER_OWNERSHIP_MAP.md#typed-tx) · [STOP scope](../../../../design/70_C_INTERFACE/00_HEADER_OWNERSHIP_MAP.md#stop-scope)
- [승인 AudioTx_Advance 설명](../functions/AudioTx_Advance.md#outputs) · [승인 의사코드](AudioTx_Advance.md#procedure)
