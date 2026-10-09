# VSS Pseudocode — Structured + Compact

> 2026-10-07 · 핵심 흐름 11개 · 의미 기반 가칭

[정제된 API 후보](../../30_FUNCTIONS/00_FUNCTION_OVERVIEW.md)와 [82 Data Model](../../40_DATA/00_DATA_OVERVIEW.md)의 owner 경계를 기존 11개 실행 흐름에 연결한다. 입력과 상태는 의미 표현이며 실제 C signature/type은 [91 TBD](../../90_BINDING/91_IMPLEMENTATION_TBD.md)에 남긴다. 하위 요청문은 해당 owner가 수행하는 동작이다. 호출자는 다른 Module의 원본 상태를 직접 변경하지 않는다.

## 1. Flow_Process — 초기화·진행·선택 조율

연결: [FLOW](../../20_MODULES/10_FLOW_MODULE.md), [POLICY/SELECT](../../20_MODULES/22_POLICY_MODULE.md), [RUNTIME](../../10_LAYERS/details/50_TIMEBASE_RUNTIME_BOUNDARY.md).

```c
Flow_Process(입력 또는 후속 처리 기회, 비교 가능한 시간)
{
    if (최초 처리 기회라면)
    {
        AUDIO STREAM에 장치 준비를, ASSET에 공개 가용 확인을 요청한다;
        미평가 결과는 확인 실패로 바꾸지 않는다;
    }

    Input_Process(수신 입력 또는 입력 신뢰성 변화);
    Backend 결과 = AudioStream_Process(원결과와 필요한 진행);
    Playback_Process(Backend 결과와 재생 기한);
    Store_Process(반영된 최신 재생 사실과 age/Hold 기한);
    복구 허용 = Health_Evaluate(원진단 사실과 복구 검증 결과);

    if (현재 대상의 복구가 허용되었다면)
    {
        Playback_Process(전체 후속 권한 차단과 old 정리 요구);
        if (old retirement와 관련 자원 안전 종료가 확인되었다면)
        {
            AudioStream_Process(허용된 대상의 실제 복구와 효과 검증 요구);
        }
    }

    새 원결과를 해당 owner에 반영하거나 보호된 후속 처리로 연결한다;
    함께 판단 가능한 owner 관측을 읽는다;
    if (수집 중 관련 상태가 바뀌었다면)
    {
        재평가 필요성을 남기고 다음 처리 기회에 다시 읽는다;
        return;
    }

    판단 = Select_Choose(함께 판단 가능한 후보/owner/제한 관측);
    Playback_Process(판단과 최신 실행 조건);
    실행 요구로 달라진 원사실/제한을 반영하고 보고 관측을 재확인한다;
    보고 = Health_BuildStatus(반영 완료 사실과 현재 제한);
    INPUT을 통해 기존 Node Communication에 보고를 전달한다;
}
```

RX가 없어도 각 owner의 기한·refill·정리·보고가 bounded하게 진행된다. 새 실행 요구 뒤 달라진 사실은 보고 전에 재확인한다. 원producer marker와 사실 시각은 유지하며 getter나 알림으로 미반영 사실을 합성하지 않는다.

## 2. Input_Process — 검증과 STORE 전달

연결: [INPUT](../../20_MODULES/20_INPUT_MODULE.md) → STORE.

```c
Input_Process(값과 Meta 또는 신뢰성 변화)
{
    PRODUCT/TEST를 격리한 원출처에서 값, 품질, 시간, 세대와 순서를 검증한다;
    if (부적합하거나 과거 문맥이라면)
    {
        if (최신 적용 가능한 품질 상실 근거라면)
        {
            Store_Process(원기산점을 보존한 품질 상실);
        }
        정상 사건으로 수용하지 않고 return;
    }

    검증된 출처와 적용 순서를 INPUT 문맥에 반영한다;
    결과 = Store_Process(검증 입력과 원본 시간);
    STORE 수용 거부에도 검증된 출처/순서를 rollback하지 않는다;
    수용 결과를 반환한다;
}
```

INVALID·STALE은 CLEAR가 아니다. WINDOW / Anti-Pinch의 제품 연결은 **[구현 보류 — 설계 유지]**이며 TEST 의미 경로를 유지한다.

## 3. Store_Process — 입력·재생 사실·기한 반영

연결: [STORE](../../20_MODULES/21_STORE_MODULE.md). 입력, 재생 결과, 기한 중 주어진 처리 사유만 적용한다.

```c
Store_Process(검증 입력 또는 원재생 사실 또는 기한)
{
    if (원재생 사실이 있다면)
    {
        원 Occurrence와 최종 이력을 확인한다;
        적법한 최초 actual만 Started로, 전체 최종 결과만 해당 이력으로 반영한다;
        uncertain 최종은 replay를 막고 이후 late 사실로 정상 이력을 부활시키지 않는다;
    }

    if (One-shot 입력이라면)
    {
        if (동일 발생 재전달이라면) { 기존 결과를 반환한다; }
        else if (Pending부터 최종/재전달 방어까지 추적 자원을 확보할 수 없다면)
        {
            기존 accepted/final 이력을 보존하고 신규 수용을 거부한다;
        }
        else { 원본 age와 identity를 보존한 Pending을 만들고 accepted를 반환한다; }
    }
    else if (Stateful 또는 품질/Rear 변화라면)
    {
        최신 유효 ACTIVE/CLEAR와 품질을 구별해 해당 후보만 갱신한다;
        같은 ACTIVE로 Session을 재시작하지 않는다;
        기존 유효 경고의 신뢰성 상실에만 Hold를 적용하고 새 INVALID 경고를 만들지 않는다;
        Hold는 최초 신뢰성 상실과 원본 유효기한 중 이른 기준으로 기산한다;
        Rear는 최신 위험/Activation을 결합하고 DISABLED는 후보/Hold만 제거한다;
    }

    if (기한 처리라면)
    {
        최신 PLAYBACK 사실을 재확인하고 Hold 만료 후보를 제거한다;
        uncertain 최종이 없고 확정 미시작이며 원본 age가 2초 이상인 One-shot만 Expired로 확정한다;
        in-flight/불명확 시도나 Started Session을 age만으로 만료시키지 않는다;
    }
}
```

**[잠정] 최초 새 actual start는 age < 2초**다. 미시작 재평가는 안전 retirement와 원조건을 유지한다. Hold·원본 age·replay는 재전달/선점/복구로 갱신하지 않으며 Rear 재활성화로 옛 위험을 부활시키지 않는다. 시간/출처 연속성 상실도 새 발생으로 처리하지 않는다.

## 4. Playback_Process — 실행·종료·retirement

연결: [PLAYBACK](../../20_MODULES/23_PLAYBACK_MODULE.md) → AUDIO STREAM / STORE.

```c
Playback_Process(판단 또는 원 Backend 결과 또는 기한/종료 요구)
{
    if (결과가 현재 owner와 다른 원시도이거나 uncertain 최종 뒤의 late 결과라면)
    {
        원시도 정리/진단만 진행하고 현재 Session을 변경하지 않는다;
        확인된 old fence로만 retirement를 검토한다;
        return;
    }

    if (OUTPUT_START_CONFIRMED가 원시도와 사실 시각/권한/원기한에 적법하다면)
    {
        Store_Process(최초 Started 사실);
        종료 중이면 STOPPING/ABORTING을 유지하고 그 외에는 ACTIVE로 진행한다;
    }
    if (start accepted 결과라면) { 수락+arm만 기록하고 actual/종료 상태를 되돌리지 않는다; }

    if (START_OUTCOME_UNCERTAIN 최종이라면)
    {
        전체 start/cue/반복 권한을 차단하고 QUARANTINED로 둔다;
        Store_Process(uncertain 최종과 replay 금지);
        기존 PLAYBACK_STATE_FAILURE와 FAULT/UNAVAILABLE 근거를 FLOW에 반환한다;
        AudioStream_Process(old stop/정리 요구);
        return;
    }

    if (선점, 유효 CLEAR, 실패, 전체 정책 끝 또는 최신 실행 조건 상실로 종료해야 한다면)
    {
        종료 의도와 전체 후속 권한 차단을 먼저 반영한다;
        STOPPING/ABORTING에서 AudioStream_Process(old stop/정리 요구)를 연결한다;
    }

    if (Backend 전체 범위의 no-start 또는 termination과 상위 권한 차단/late 보호가 충분하다면)
    {
        원결과와 전체 완료/중단 사유를 STORE에 반영하거나 보호된 통지로 유지한다;
        미시작 재평가는 uncertain 최종이 없는 원발생에만 허용한다;
        RETIRED로 출력 owner를 해제하고 최신 재선택을 요구한다;
        return;
    }
    if (종료/QUARANTINED 진행 중이라면) { old 정리를 계속 추적하고 return; }

    if (현재 owner 없이 새 winner를 실행하려 한다면)
    {
        if (old retirement, 최신 후보/원기한/제한 또는 보호 자원이 불충분하다면) { return; }
        원후보의 전체 정책 Session과 원 Attempt 문맥을 채택한다;
        PREPARING에서 AudioStream_Prepare(원계획과 원기한)를 요청한다;
    }
    if (현재 기다리는 원준비의 prepared가 도착했다면)
    {
        최신 CLEAR/owner/Fault와 준비 구성을 다시 검사한다;
        첫 실제 시작 전에는 원본 age를 검사하고 Started Session의 정상 후속 cue에는 이를 재적용하지 않는다;
        if (이제 시작할 수 없다면)
        {
            전체 후속 권한을 차단하고 AudioStream_Process(old stop/정리 요구)를 연결한다;
            return;
        }
        START_IN_FLIGHT에서 AudioStream_Process(원 start 요구)를 연결한다;
    }
    if (ACTIVE에서 해당 cue 구간의 안전 끝과 정상 후속 정책이 확인되었다면)
    {
        같은 Session 문맥으로 AudioStream_Prepare(정책상 다음 cue/반복 준비)를 연결한다;
    }
}
```

종료 중의 최종 전 적법한 late actual은 Started 사실만 반영하며 종료 의도를 취소하지 않는다. 기한/권한 밖 출력은 물리 사실·위반 근거를 보존해 정리한다. cue/EOS는 전체 완료가 아니며 다른 Session은 old retirement 전 prepare/start하지 않는다. Started One-shot 자동 resume와 PCM offset resume는 하지 않는다. 충분조건은 [Driver evidence](../../20_MODULES/30_AUDIO_TX_MODULE.md#evidence)와 [Playback 결과](../../20_MODULES/23_PLAYBACK_MODULE.md#outcomes)를 따른다.

## 5. AudioStream_Prepare — Asset·PCM·장치 준비

연결: [AUDIO STREAM](../../20_MODULES/24_AUDIO_STREAM_MODULE.md) → [ASSET](../../20_MODULES/25_ASSET_MODULE.md) / [AUDIOCTRL](../../20_MODULES/31_AUDIO_CONTROL_MODULE.md).

```c
AudioStream_Prepare(원 Session/Attempt, Asset와 전체 계획, 원기한)
{
    원시도와 provider/PCM 자원의 안전 사용·보호 공간을 확인한다;
    if (원준비가 취소되었거나 자원 보호가 불충분하다면) { return; }

    Asset_Read(원 Asset와 요청 MP3 범위의 metadata/bytes);
    AudioControl_Service(현재 구성의 Codec/Generator/Reference별 준비);
    if (Asset 또는 장치 준비의 확인 실패라면)
    {
        원producer/대상/단계의 실패를 반환하고 기존 정리를 요청한다;
        return;
    }

    원시도 decoder/provider를 진행한다;
    PCM 결과 = AudioStream_FillBuffer(사용 가능한 A/B와 초기 prime 요구);
    decode 실패는 원근거를 보존한 준비실패로 반환한다;
    if (유효 PCM/format와 필요한 초기 전송 준비, 장치 구성이 확인되었다면)
    {
        원시도 prepared를 PLAYBACK에 반환한다;
    }
}
```

준비는 출력 허가가 아니다. AUDIOCTRL이 장치별 원결과/구성 무효화를 반영하며 매 Attempt마다 전체 재초기화하지 않는다. 유지된 장치 ready가 새 Asset/provider/A/B 준비를 대신하지 않는다.

## 6. AudioStream_FillBuffer — MP3 decode·A/B prime/refill

구분: C09 내부 공유 처리. 연결: [AUDIO STREAM / PCM](../../20_MODULES/24_AUDIO_STREAM_MODULE.md#pcm) → ASSET / TX DRIVER.

```c
AudioStream_FillBuffer(원 provider와 Buffer/회차, prime 또는 refill 요구)
{
    if (해당 scope가 차단되었거나 Buffer가 CPU 사용 가능하지 않다면) { return; }

    MP3 span = Asset_Read(필요한 원 Asset와 bounded 압축 범위); 참조를 보존한다;
    CPU decoder로 유효 PCM과 실제 입력 소비 위치/EOF를 갱신한다;
    if (Asset read 또는 decode 실패라면)
    {
        발견 producer/단계와 원시도 실패를 반환하고 기존 정리를 요청한다;
        return;
    }

    if (decode 중 취소/stop 또는 해당 제출 권한 상실이 생겼다면)
    {
        handoff하지 않고 provider/read의 안전 종료를 진행한다;
        return;
    }
    if (유효 PCM이 없다면) { EOF/진행 사실만 반환하고 return; }

    해당 Buffer/새 회차/유효 구간을 handoff pending으로 먼저 보호한다;
    AudioTx_Process(원귀속 PCM 인계 요구);
    consumer 참조 종료가 확인된 MP3 span만 반환한다;
}
```

PCM 전송 Buffer는 정확히 A/B 두 개다. 짧은 tail이나 빈 B는 유효 길이로 처리하고 임의 padding/반복/추가 PCM Buffer를 만들지 않는다. 출력 경로는 **PFlash MP3 → CPU decode → PCM A/B → eDMA → SAI → SGTL5000**이다.

## 7. AudioStream_Process — start·stop·반환·Backend 진행

연결: [AUDIO STREAM](../../20_MODULES/24_AUDIO_STREAM_MODULE.md) → TX / AUDIOCTRL.

```c
AudioStream_Process(원 start/stop/준비/복구 요구 또는 lower 결과와 진행 기회)
{
    if (stop/abort 요구라면)
    {
        해당 scope의 미래 decode/refill/제출을 먼저 차단한다;
        AudioTx_Process(old 장치 정리 요구);
    }
    AudioControl_Service(필요한 준비/제어와 원구성 결과);
    전송 사실 = AudioTx_Process(원 lower 결과와 필요한 장치 진행);
    if (미완료 원준비가 있고 차단되지 않았다면)
    {
        AudioStream_Prepare(같은 원준비의 후속 진행);
    }

    if (결과가 old provider/Buffer 회차라면)
    {
        old 자원 정리/원진단만 진행하고 새 provider/A/B usage를 변경하지 않는다;
    }
    else if (같은 회차의 safe return 또는 명확한 미사용 거부가 확인되었다면)
    {
        해당 Buffer만 CPU 사용 가능으로 회수한다;
        if (정상 후속 제출이 허용된다면) { AudioStream_FillBuffer(해당 Buffer의 refill); }
    }

    if (원 start 요구이고 최신 prepared/권한/구성/원기한이 유효하다면)
    {
        AudioTx_Process(원문맥과 기한을 유지한 fresh start 요구);
    }
    if (Asset/decode/start/output 실패, 원 start의 최신 gate 거부 또는 관측 손실이라면)
    {
        원근거를 반환하고 미래 제출 차단/정리를 진행한다;
    }
    if (현재 대상의 복구 허용과 old retirement/자원 안전 종료가 확인되었다면)
    {
        해당 Driver에 허용 범위의 실제 복구/후속 진행을 요청하고 수행한 동작을 재발행하지 않는다;
        수행 완료 뒤 별도로 효과를 검증하고 원대상 결과를 FLOW에 반환한다;
    }

    장치 근거와 해당 scope의 미래 PCM 불가를 결합해 Backend 결과를 반환한다;
    provider/read와 PCM 자원은 각 실제 참조 종료 조건에 따라 정리한다;
}
```

consumed만으로 쓰기 권한을 회수하지 않으며 error/partial arm도 보호를 유지한다. 반환은 stop 후 refill 허가가 아니다. 수락+arm과 actual은 독립 반환하며, 정상 구간 drain과 전체 stop/abort scope를 구별한다.

## 8. AudioTx_Process — 등록·장치 사실·정리

연결: [TX DRIVER](../../20_MODULES/30_AUDIO_TX_MODULE.md) → [HAL](../../10_LAYERS/details/40_HAL_BSP_BOUNDARY.md).

```c
AudioTx_Process(원 PCM/start/정리 요구 또는 원 HAL 사실)
{
    if (새 장치 요구라면)
    {
        원시도/operation, 기한, Buffer/회차/유효 구간을 보호한다;
        HAL에 vendor 요청 전 불변 등록을 확보하게 한다;
        원범위의 인계 또는 fresh arm/정리 동작을 요청한다;
        return과 실제 retain/arm 효력을 따로 기록한다;
    }

    if (HAL 사실이 있다면)
    {
        원등록/회차에만 적용하고 old A로 new A/B의 retain을 해제하지 않는다;
        수락과 실제 arm이 확인된 원 start만 start accepted로 반환한다;
        fresh actual 후보의 충분성을 판정하고 확인/미확인 사실을 반환한다;
        source consumed와 safe return은 별개 원사실로 반환한다;
    }
    if (출력/귀속/coverage가 불명확하거나 partial error라면)
    {
        보호를 유지하고 확인 범위와 부족/손실을 AUDIO STREAM에 반환한다;
    }

    충분한 과거 무출력 coverage와 old 미래 출력 차단이 있으면 no-start 근거를 반환한다;
    해당 scope의 장치/FIFO/frame 정리와 old 출력 불가가 있으면 termination 근거를 반환한다;
    실제 미래 접근이 없는 원 Buffer/회차만 safe return으로 반환한다;
}
```

근거의 세부 충분조건은 [31 evidence](../../20_MODULES/30_AUDIO_TX_MODULE.md#evidence)를 사용한다. DMA complete/현재 idle/mute/timeout/abort return만으로 actual·no-start·종료를 만들지 않는다.

## 9. AudioHAL_Callback — 짧은 원귀속 capture

연결: [HAL / callback lifetime](../../10_LAYERS/details/40_HAL_BSP_BOUNDARY.md#lifetime), RUNTIME.

```c
AudioHAL_Callback(요청 전 등록과 raw IRQ/vendor 사실)
{
    필요한 raw 사실과 발생/관측 시각, coverage/손실을 짧게 보존한다;
    원등록 귀속 또는 귀속 부족을 남기고 필요한 acknowledge를 연결한다;
    현재 Attempt/Buffer를 나중에 붙여 재라벨링하지 않는다;
    기존 도구로 후속 처리 기회를 알리고 return;
}
```

ISR에서 decode/정책/복구를 실행하지 않는다. retirement/IRQ disable만으로 등록 저장을 폐기하지 않으며 HW·대기·처리 중 참조 종료를 확인한다. 알림은 사실이 아니고 중요 사실 손실은 관측 가능하게 남긴다.

## 10. Health_Evaluate — Fault·복구 허용/해제

연결: [HEALTH/C11](../../20_MODULES/26_HEALTH_MODULE.md#recovery). HW 수행은 FLOW → Backend가 연결한다.

```c
Health_Evaluate(원진단 사실, 현재 대상과 복구 검증 결과)
{
    원producer/원인/단계/대상/구성/사실 시각을 유지한다;
    입력 품질 상실은 입력 진단으로, 확인된 출력 실패는 기존 5종 Fault로 반영한다;
    미평가 startup을 INITIALIZATION_FAILURE로 바꾸지 않는다;
    uncertain 최종은 PLAYBACK_STATE_FAILURE와 해당 출력 제한으로 반영한다;

    if (현재 Fault가 recoverable이고 허용 조건을 만족한다면)
    {
        현재 대상/구성에 한정한 복구 허용을 FLOW에 반환한다;
    }
    if (현재 허용 대상의 실제 수행 뒤 효과 검증 성공이 확인되었다면)
    {
        해당 제한만 해제하고 최신 후보 재평가를 요구한다;
    }
    다른 현재 Fault/최근 기록과 원본 age/Hold/replay/연속성은 보존한다;
}
```

복구 접수·무음·owner 해제는 검증 성공이 아니다. 옛 복구 결과로 새 Fault/구성을 해제하지 않는다. exact cause→Fault/Action과 미정 timeout/retry는 [정책 TBD](../../90_BINDING/91_IMPLEMENTATION_TBD.md#policy)를 유지한다.

## 11. Health_BuildStatus — 파생 보고

연결: [C12 / Status](../../20_MODULES/26_HEALTH_MODULE.md#status) → FLOW → INPUT.

```c
Health_BuildStatus(함께 판단 가능한 반영 완료 owner 사실과 C11 제한)
{
    if (QUARANTINED 또는 공통 정상 출력 불가와 새 start 차단이 확인된 제한이라면)
    {
        FAULT/UNAVAILABLE을 보고한다;
    }
    else if (적법한 actual이 확인된 Session이 아직 진행 중이라면)
    {
        cue 무음/반복 대기/시작 후 종료 진행도 PLAYING으로 보고한다;
    }
    else { 정상 초기화/미평가와 정상 준비를 구별해 STARTUP/READY를 파생한다; }

    FULL/DEGRADED는 지원 서비스 수준에서 파생한다;
    accepting은 입력/검증/저장 자원과 시간/출처/replay 준비에서 파생한다;
    기존 VSS_STATUS 묶음을 FLOW에 반환한다;
}
```

FULL/READY와 accepting=true는 개별 start/수용 허가가 아니다. accepting=false여도 duplicate·유효 CLEAR·품질/기한 진행은 유지한다. WINDOW 보류만으로 Fault/DEGRADED를 만들지 않으며 미정 보고 reduction은 기존 TBD다.


> R0 보존 위치: 기존 11개 흐름의 본문·코드 블록은 변경하지 않았다. 이 파일은 R5 재작성의 의미 참고본이며 최종 C 직전 의사코드가 아니다. [Pseudocode Overview](../00_PSEUDOCODE_OVERVIEW.md)에서 읽는 범위와 후속 단계를 확인한다.
