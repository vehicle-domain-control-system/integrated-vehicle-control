# Playback_Advance — 상세 동작 설명

> A1 입력 개정 2026-10-15 · [현재 Network 연결](../../../../design/90_BINDING/92_NETWORK_BINDING.md). `L=min(양수 USE_LIMIT,500ms,2000ms)`; 최초 actual에만 적용. 과거 동결/PASS는 당시 범위의 이력이며 본문 변경은 이번 A1 결정이다. 구현·시험 NOT_RUN.

[상세 설명](../functions/Playback_Advance.md) · [의사코드](../pseudocode/Playback_Advance.md) · [PLAYBACK](../00_MODULE_OVERVIEW.md) · [VSS 지도](../../../01_VSS_MAP.md) · CORE-08

> GUIDE · LOGICAL_APPROVED / 실제 C ABI BINDING_TBD · TEST_NOT_RUN / HW_UNVERIFIED.

<a id="contract"></a>
## 목적과 최신 논리 원형

원 Session/Attempt의 typed 요청·출력·정리 사실을 적용하고, 실제 시작 적법성·정상 cue·전체 종료/retirement·STORE 통지와 독립 OUT 보호를 진행합니다. **void 반환은 전체 성공·완료를 뜻하지 않습니다.**

```text
void Playback_Advance(
    IN const VssAudioRequestResult requests[count],
    IN const VssAudioOutputEvidence outputs[count],
    IN const VssBackendCleanupObservation cleanup[count],
    IN const VssCandidateObservation candidates[count],
    IN const VssFaultState restrictions[count],
    IN const VssAssetDescriptor assets[count],
    IN const VssDeviceReadinessState devices[count],
    IN const VssReadBasis *conditionsBasis,
    IN const VssTimeEvidence *time,
    OUT VssPlaybackObservation observation[capacity -> written],
    OUT VssPlaybackFact facts[capacity -> written],
    OUT VssFactApplication applications[capacity -> written],
    OUT VssDiagnosticEvidence diagnostics[capacity -> written]);
```

[R6 계약](../../../../design/70_C_INTERFACE/00_HEADER_OWNERSHIP_MAP.md#r6-playback-advance)과 [인자·결과 표기](../../../../design/70_C_INTERFACE/00_HEADER_OWNERSHIP_MAP.md#prototype-notation)를 그대로 읽습니다. 각 count/capacity/written은 독립 논리 범위입니다. 배열 저장 방식·수치·동기 보호·enum 폭·Header는 실제 ABI로 확정하지 않습니다. 과거 R2 provisional 묶음을 새 public DTO로 선언하지 않습니다.

<a id="inputs"></a>
## 9 IN과 4 OUT — 처리와 공개의 경계

| R6 입력군 | 원래 의미·owner·검증 |
|---|---|
| `requests[count]` / `VssAudioRequestResult` | STREAM의 원 Session/Attempt·prepare/control 단계. PREPARED/수락은 actual이 아님 |
| `outputs[count]` / `VssAudioOutputEvidence` | STREAM의 원 Session/Attempt/scope와 보호된 transportFacts/각 원 시각·coverage·futurePcmBlocked |
| `cleanup[count]` / `VssBackendCleanupObservation` | 각 원 Attempt의 providerReferencesEnded/pcmAccessEnded/lowerOutputBlocked/lateIdentityProtected. 실제 출력 종료와 별도 |
| `candidates[count]` | STORE의 현재 적용 완료 key/revision·원 Meta·유효성/품질·기한 |
| `restrictions[count]` | HEALTH의 현재 대상·구성/instance·제한 |
| `assets[count]` | ASSET의 해당 plan/의미 ID/image 가용 근거 |
| `devices[count]` | 각 AUDIO CONTROL 현재 구성/준비 근거 |
| `conditionsBasis` | FLOW의 이번 반영 완료·원 인과/시간 근거. 과거 actual 당시 권한과 별도 |
| `time` / `VssTimeEvidence` | 현재 후속 기한·정책 기회의 시간. 과거 actual의 occurredAt을 대신하지 않음 |

| 독립 OUT 군 | 내용·written 의미 | 공간/보호 부족 때 |
|---|---|---|
| `observation` / `VssPlaybackObservation` | 반영 완료한 PB 읽기 projection. 유효한 현재/추적 Session 참조가 있는 관측만 공개, 없으면 written=0 | 관측 공개 보류·원 owner/후속 보존. 정상 Session/ACTIVE 합성 없음 |
| `facts` / `VssPlaybackFact` | **이번 또는 보호된 이전 실제 STORE 적용 시도**의 원 fact | 아래 applications와 짝 공개가 불가하면 원 fact·적용 결론/미반영 의무 보호 |
| `applications` / `VssFactApplication` | 동일 fact 실제 호출의 APPLIED/ALREADY_APPLIED/NOT_APPLIED | facts와 같은 index·written만 유효 공개. 미시도 fact에 결과를 붙이지 않음 |
| `diagnostics` / `VssDiagnosticEvidence` | 원 발견 주체·단계·cause/target/config/operation/원 시각의 독립 근거 | HEALTH 전달까지 보호. 실손실은 coverage LOST·충분성 제한으로 드러냄 |

각 OUT은 **주소/전달영역·capacity 저장소·written 저장공간 검증 → capacity 값/실제 범위 확인 → 통과 군의 written=0** 순서입니다. 무효 군은 역참조·초기화·쓰기를 하지 않습니다. capacity=0인 유효 군도 written=0이며 기록 없이 보호를 유지합니다. 일부/전부 OUT 불가가 이미 보호된 원 사실의 내부 적용·안전 차단 전체를 중단하는 이유는 아닙니다.

<a id="pairing"></a>
## facts ↔ applications의 같은 원 fact 대응

두 군은 별도 capacity를 가지면서도 **유효 공개하는 짝은 같은 index·written 수**를 유지합니다. 유효한 양쪽 저장공간·공통 잔여 용량·원 fact와 적용 결론의 전달 수명 보호·쌍 기록 완료 조건을 먼저 확인합니다. 한쪽 무효/용량0/부족이면 한쪽만 내보내거나 count를 증가시키지 않습니다. 무효 군의 written을 읽어 맞추지도 않습니다.

PB는 원 fact·원 identity/시각·참조와 실제 적용 결과를 독립 보호할 수 있을 때 `Store_ApplyPlaybackFact`를 실제 호출합니다. 해당 한 번의 반환에만 `facts[i]`/`applications[i]`를 연결합니다. 미호출이면 결과를 합성하지 않습니다. APPLIED/ALREADY_APPLIED는 **그 원 fact의 STORE 통지 의무만** 완료하며 미공개 결과/참조는 소비자 수명까지 따로 보호합니다. NOT_APPLIED는 원 fact·통지 의무를 유지해 다음 bounded 기회에 재적용합니다. 같은 fact를 재적용하는 것은 새 occurrence/fact identity 생성이 아닙니다.

사실·결론 보호가 실패하면 중요한 근거를 처리 완료/충분 coverage로 보이지 않게 합니다. 가능한 producer 보호·기존 owner의 손실/후속 문맥을 유지하고 새 효력을 제한합니다. 불완전 pair를 출력하지 않으며 공간 확보를 위해 accepted/final/옛 callback 기록을 eviction하거나 무제한 pool·새 ack API를 만들지 않습니다.

<a id="ownership"></a>
## Owner·호출·수명

Caller는 [FLOW](../../../01_APP/FLOW/functions/Flow_Process.md)와 PB의 하위 즉시 결과 적용 경계입니다. PLAYBACK만 Session/Attempt·plan position·phase·start/cue/repeat 권한·실제 시작 지식·전체 final/통지 의무를 변경합니다. Callee는 기존 `AudioStream_Prepare`, `AudioStream_RequestControl`, [Store_ApplyPlaybackFact](../../STORE/functions/Store_ApplyPlaybackFact.md)입니다. POLICY를 역호출해 새 winner/Session을 선택하지 않습니다.

STORE는 occurrence ledger/Stateful writer, STREAM은 provider/PCM A/B 사용권 writer, TX는 operation/장치 retain writer, HEALTH는 Fault/Status writer입니다. PB는 해당 원본을 직접 쓰거나 Network 송신/Status·Fault clear를 생성하지 않습니다. Occurrence ≠ Session ≠ Attempt ≠ TX Operation ≠ PCM Cycle입니다. 임시 request/output 포인터는 장기 저장하지 않고 원 identity/시간/transportFacts/plan/Meta/fact를 각 실제 소비 수명까지 보호합니다.

<a id="actual"></a>
## actual 충분성과 최초 적법성의 세 갈래

먼저 원 Session/Attempt/activation·scope·transportFacts·원 사실 시각/coverage와 **활성화 이후 새 실제 출력** 충분성을 확인합니다. enum 이름, PREPARED/START accepted+enabled, 현재 idle/HAL IRQ/abort/zero-fill만으로 actual을 만들지 않습니다. actual 충분성에는 미래 PCM 차단이 필수인 것이 아니며 NO_START/TERMINATION의 조건과 합치지 않습니다.

최초 One-shot actual의 원 Meta domain/epoch·연속성과 occurredAt의 가능한 구간이 비교 가능해야 합니다. 전체 구간이 originalAtLatest 이후이고 `occurredAt.latest - originalAt <L` [잠정]이며 원 당시 권한 범위 안일 때만 적법합니다. **정확히 L은 미허용**입니다. 구간 전체가 기한/권한 밖임이 입증되면 물리 위반, 경계 걸침·비교/당시 권한 부족이면 적법 여부 불확실입니다. 불확실에서 정상 Started/확정 위반 둘 다 합성하지 않고 원 물리 fact·충분한 실제 coverage와 부족한 적법성 범위를 분리해 보호합니다.

적법 actual은 해당 Attempt 지식을 반영하며 최초 One-shot에만 `VSS_FACT_FIRST_ACTUAL_START`를 원 occurrence에 통지합니다. Stateful은 PB 실제 지식/관측을 반영할 뿐 가짜 One-shot fact/occurrence를 만들지 않습니다. STOPPING/ABORTING의 적법 late actual은 원 Started/보고만 반영하고 **phase ACTIVE·후속 cue 권한은 복구하지 않습니다.** uncertain final/retired 뒤 결과는 old 정리/진단만이며 정상 이력을 부활시키지 않습니다.

기한/권한 위반 또는 적법성 부족은 전체 미래 권한 차단·원 STOP/안전 정리를 진행하되 부족만으로 uncertain final/NO_START/종료/retirement를 만들지 않습니다. 처리 now나 HAL capturedAt으로 과거 occurredAt을 대체하지 않고 A1의 L을 최초 gate에 적용합니다. 이미 적법 Started된 동일 Session의 정상 후속 cue에는 최초 `<L`를 재적용하지 않습니다.

<a id="requests"></a>
## 준비·제출·수락과 정상 후속 cue

actual/종료 사실을 요청 acceptance보다 우선 보존합니다. 원 귀속 PREPARED 후에도 phase PREPARING·현재 권한·latest CLEAR/후보/원 기한·HEALTH/장치/자원·basis를 **START 효력 직전** 다시 검사합니다. 통과 때만 START_IN_FLIGHT·원 결과 보호를 먼저 확보하고 STREAM START를 연결합니다. 늦은 PREPARED/submitted/accepted는 이미 진행한 actual·STOPPING/ABORTING/QUARANTINED를 rollback하지 않습니다. 정상 START_IN_FLIGHT 대기 자체는 Fault/uncertain final이 아닙니다.

원 prepare 실패/요청 거부/부분 효력·불명확은 각 단계 사실을 보호하고 원 차단/정리·후속으로 처리합니다. 명확한 무접근 종료 근거 없이 자원/owner를 회수하지 않습니다. count=0/시간·정책 기회도 기존 의무를 진행합니다.

정상 ACTIVE에서 이전 관련 Backend 구간의 **안전 끝**·현재 전체 plan·권한/조건과 정의된 gapAfter/repeatRule을 확인해 같은 Session의 다음 position/필요 Attempt를 보호하고 준비합니다. 최초 Started가 이미 확정된 정상 후속 preparation은 firstStartMeta=NULL입니다. cue↔Attempt 수를 1:1로 고정하지 않고 PCM offset resume나 새 occurrence를 만들지 않습니다. 한 cue EOS/압축 EOF/SEGMENT 종료는 전체 final이 아닙니다.

<a id="termination"></a>
## 전체 종료·uncertain final·retirement

모든 관련 원 Attempt/segment scope **각각**에서 `(필요한 과거 전체 범위의 실제 무출력 충분 입증 AND 해당 옛 작업의 미래 출력 불가 충분 입증)` 또는 `(실제/잔류 출력 종료 충분 입증 AND 해당 옛 작업의 미래 출력 불가 충분 입증)`을 확인합니다. scope A는 TERMINATION, scope B는 NO_START로 충족할 수 있지만, 각 원 scope의 근거를 서로 대신 사용하지 않습니다.

이는 [의사코드의 원 출력 근거 적용 단계(B2)](../pseudocode/Playback_Advance.md#procedure)에서 원 identity/operation/scope·시각·coverage를 확인·보호한 `NO_START_CONFIRMED` 또는 `OUTPUT_TERMINATION_CONFIRMED`의 충분 근거만 사용합니다. 현재 idle/STOP ack/zero-fill/IRQ 결과를 충분 근거로 승격하지 않으며, 관련 scope 하나라도 미처리·부족·손실이면 전체 final/retirement를 만들지 않고 원 보호·정리를 유지합니다.

위 하위 근거에 **더해**, 각 관련 scope의 미래 PCM 제출 차단, PB 전체 start/cue/repeat 차단, provider/read/PCM·새 owner 자원의 실제 안전 조건, late 원귀속 보호가 **각각 모두** 필요합니다. 한 scope/cleanup/STOP 수락을 다른 구간/전체 Session으로 확대하지 않습니다.

- 충분한 전체 NO_START + 실제 과거 미시작 + uncertain final 없음이면 원 One-shot에 `VSS_FACT_NO_START_REEVALUATE`; Pending/Expiry는 STORE가 원 age로 재판정합니다.
- 적법 Started된 전체 Session의 정상 plan 끝·전체 종료는 `VSS_FACT_WHOLE_COMPLETED`, 선점/고장/조건 상실 중단·전체 종료는 `VSS_FACT_WHOLE_INTERRUPTED`입니다. One-shot 통지이며 Stateful 상태/품질을 다시 쓰지 않습니다.
- 기존 `[TBD-FINAL]`의 원 불명확 최종 조건이 충분할 때만 `VSS_START_UNCERTAIN_FINAL`·QUARANTINED·replay 금지와 `VSS_FACT_START_OUTCOME_UNCERTAIN`을 보호합니다. PLAYBACK_STATE_FAILURE + FAULT + UNAVAILABLE 근거를 FLOW→HEALTH에 연결합니다. timeout/coverage 부족만으로 최종 조건 충족을 가정하지 않습니다.
- uncertain final은 retirement보다 먼저 기록할 수 있습니다. 뒤늦은 전체 안전 종료로 old owner를 retire할 수 있어도 uncertain을 정상 Completed/Interrupted/NO_START 재평가로 바꾸지 않습니다.
- termination은 과거 no-start 증명이 아닙니다. 과거 시작 여부 불명확이면 정상 완료/미시작 fact 없이 지식·replay/후속 판단을 보호합니다. owner retirement와 STORE 통지/ledger·provider·HAL 기록 폐기는 자동으로 동시 완료되지 않습니다.

<a id="feasibility"></a>
## 독립 후보·제품 Gate

원귀속/late·phase 단조성, 원 actual 가능한 시각 구간의 적법/위반/불확실, 최초 gate와 정상 cue, whole scope/retirement 조건, STORE 멱등 통지·OUT 짝 보호는 `LOGICAL_APPROVED / PROVISIONAL_TESTABLE` Host 주입 후보입니다. `<L` 원 정책은 기존 한 정의 위치에서 관리·경계 fixture와 채택/변경·철회 조건으로 추적합니다. 부분 효력/손실/용량 불일치도 논리적으로 검증할 수 있으며 시험은 `TEST_NOT_RUN`입니다.

실제 start/no-start/잔류 출력·STOP sufficient coverage 및 PCM/refs 안전은 STREAM/TX/HW owner의 계측이 필요합니다. 원 시간 ABI·key/storage/직렬화·실제 final/timeout 규칙은 VSS/Runtime owner 미정입니다. 해당 제품 확정만 제한하며 전체 PB 로직을 HOLD하지 않습니다. 보관 Session/AudioPlayer의 과거 완료 관측은 정적 재사용 후보이며 최신 R6의 전체 충분성·C ABI·제품 성공으로 승격하지 않습니다. Fault5/PCM A/B 두 개·WINDOW 제품 보류/TEST 격리·Q/R6/P1B_C1/P2 미결 유지입니다.

<a id="sources"></a>
## 정본과 인접 문서

[의사코드](../pseudocode/Playback_Advance.md) · [전환 요청](Playback_RequestTransition.md) · [STORE fact 적용](../../STORE/functions/Store_ApplyPlaybackFact.md) · [R2](../../../../design/30_FUNCTIONS/30_POLICY_PLAYBACK_FUNCTIONS.md#playback-advance) · [R3 Attempt/Fact](../../../../design/40_DATA/40_PLAYBACK_DATA.md#vssattemptcontext) · [R3 Backend 근거](../../../../design/40_DATA/50_AUDIO_STREAM_DATA.md#vssaudiooutputevidence) · [R4 충분조건](../../../../design/50_CONTRACTS/20_ASYNC_RESULT_EVIDENCE.md#confirmed-outcomes) · [R4 최초 시작](../../../../design/50_CONTRACTS/30_TIMING_FRESHNESS.md#first-start) · [R4 네 경계](../../../../design/50_CONTRACTS/50_AUDIO_BUFFER_TRANSPORT.md#four-boundaries) · [R5 정상](../../../../design/60_PSEUDOCODE/30_PLAYBACK_FLOW.md#playback-advance) · [R5 actual gate](../../../../design/60_PSEUDOCODE/30_PLAYBACK_FLOW.md#first-actual-gate) · [R5 STOP/late](../../../../design/60_PSEUDOCODE/50_STOP_LATE_UNCERTAIN_FLOW.md#playback-stop-late) · [R5 통지 보호](../../../../design/60_PSEUDOCODE/50_STOP_LATE_UNCERTAIN_FLOW.md#fact-notification)
