# Store_AdvanceDeadlines — 재생 사실 확인 뒤 원 age와 Hold 진행

> A1 입력 개정 2026-10-15 · [현재 Network 연결](../../../../design/90_BINDING/92_NETWORK_BINDING.md). `L=min(양수 USE_LIMIT,500ms,2000ms)`; 최초 actual에만 적용. 과거 동결/PASS는 당시 범위의 이력이며 본문 변경은 이번 A1 결정이다. 구현·시험 NOT_RUN.

[VSS 지도](../../../01_VSS_MAP.md) · [STORE](../00_MODULE_OVERVIEW.md) · CORE-05 · [한국어 의사코드](../pseudocode/Store_AdvanceDeadlines.md)

> GUIDE · 동결 의미 `0641b2c2268cf47450aeb50302638a45a7a3277f`. TEST_NOT_RUN / BINDING_TBD / HW_UNVERIFIED.

RX 없는 시간 기회에도 accepted occurrence의 원 age와 기존 Stateful Hold/Rear 후보를 진행한다. **먼저 PLAYBACK 통지 반영·관측 최신성·시작 지식, 다음 원 시간 비교, 마지막 만료**다. 현재 idle나 처리 now만으로 과거 무출력을 만들지 않는다.

<a id="contract"></a>
## R6 원형과 반환

```text
VssStoreProgress Store_AdvanceDeadlines(
    IN const VssTimeEvidence *time,
    IN const VssPlaybackObservation *playback);
```

[최신 R6](../../../../design/70_C_INTERFACE/00_HEADER_OWNERSHIP_MAP.md#r6-store-advancedeadlines)는 singular `VssPlaybackObservation`을 쓴다. [R2](../../../../design/30_FUNCTIONS/20_INPUT_STORE_FUNCTIONS.md#store-advancedeadlines)의 plural `VssPlaybackObservations` 원형은 과거 provisional이며 새 배열/통합 context를 만들 근거가 아니다. IN은 불변 동기 borrow 주석이고 실제 C ABI는 TBD다.

| 인자/결과 | 의미·유효 조건·수명 |
|---|---|
| `time` / [VssTimeEvidence](../../../../design/40_DATA/10_INPUT_DATA.md#vsstimeevidence) | 가능한 현재 구간·domain/epoch/continuity. 원 Meta·상실 사실과 비교 가능해야 하며 reset/wrap을 임의 보정하지 않음 |
| `playback` / [VssPlaybackObservation](../../../../design/40_DATA/40_PLAYBACK_DATA.md#vssplaybackobservation) | 기존 PLAYBACK 반영 완료 읽기 경계의 singular 관측. session/attempt/candidate/phase/startKnowledge/notificationPending/stamp를 함께 적용 가능한 범위에서 확인. session 없음은 과거 NO_START 증명이 아님 |
| `VSS_STORE_STABLE` | 이번 기회에 확인한 기한/후보 변화도 재확인 필요도 없음. 미래 처리/재생 완료 아님 |
| `VSS_STORE_CHANGED` | 해당 기회에 안전하게 확인한 기한/후보 변화 반영 완료. 다른 원 발생의 부족/미처리 의무가 없어졌다는 의미 아님 |
| `VSS_STORE_RECONFIRM_REQUIRED` | 반영 완료 변화 없이 최신 PB 또는 시간/기한 근거 부족. 기존 ledger/authority·원 시각 보존, FLOW 재수집 필요 |

반환 [VssStoreProgress](../../../../design/40_DATA/20_STORE_DATA.md#vssstoreprogress)는 R5의 `변경 있으면 CHANGED, 부족하면 RECONFIRM_REQUIRED, 그 외 STABLE` 우선순위를 유지한다. 독립 후보 변경과 다른 원 발생의 부족이 함께 있으면 CHANGED와 함께 기존 owner 관측에 부족/후속이 남는다. FLOW는 이 enum 하나로 모든 부족이 해결됐다고 판단하지 않는다.

<a id="ownership"></a>
## Owner와 먼저 확인할 관측

Caller는 [Flow_Process](../../../01_APP/FLOW/functions/Flow_Process.md)다. STORE만 occurrence Pending→Expired와 Stateful/Hold/Rear 파생 후보·revision을 변경한다. PLAYBACK actual/no-start 판정·Session/Attempt·HEALTH Fault·INPUT source authority와 원본 시간은 read-only다. STREAM/HAL을 직접 호출하거나 PLAYBACK을 역호출하지 않는다. Callee는 필요한 기존 Runtime 의미 경계다.

1. 인자 공간·stamp의 owner/revision/appliedThrough 인과·관련 STORE 연결을 확인한다. 모든 owner revision이 숫자로 같아야 한다는 조건은 아니다.
2. notificationPending이면 [CORE-04 사실 적용](Store_ApplyPlaybackFact.md) 또는 PLAYBACK 후속을 FLOW가 먼저 연결하도록 RECONFIRM_REQUIRED다. 오래된 관측으로 expiry나 새 Session을 확정하지 않는다.
3. 반영 도중 관측/관련 원본이 바뀌면 그 판단의 근거를 재수집한다. 적용 전 마지막 확인과 실제 직렬화 수단은 BINDING_TBD이며 새 lock/manager를 정하지 않는다.
4. time이 원 domain/epoch와 비교 불가하거나 연속성 부족이면 원 age/holdAnchor를 now로 채우지 않는다. fresh/정밀 expiry를 확정하지 않고 재확인한다.

<a id="one-shot"></a>
## One-shot 만료, 진행 중 START, 정상 후속 cue

대상은 **Pending인 원 occurrence**다. Started/Completed/Interrupted/Expired/UncertainFinal은 이 age 만료 경로에서 제외한다. 시작 시도와 관련된 관측이 START_IN_FLIGHT 또는 PENDING_OR_UNKNOWN이면 age가 L이어도 NO_START/Expired를 합성하지 않는다. actual 확인은 CORE-04 반영을 먼저 확인하고, 불명확 최종은 replay 보호를 유지한다.

확정 미시도 또는 전체 NO_START·상하위 미래 차단·retirement가 충분한 원 발생만 원 originalMeta 기준 가능한 age 구간을 비교한다. 가능한 구간 **전체가 >=L**이면 `[잠정]` 최초 시작 기회가 만료되어 Expired로 반영하고 추적/replay는 계속 보호한다. 전체 <L이면 아직 이 gate로 만료하지 않는다. L 경계를 걸치거나 원 발생 대응이 불충분하면 재확인이다. 정확히 L 첫 actual은 허용되지 않지만, **age=L만으로 진행 중 시도를 미시작으로 판정할 수는 없다**.

Already Started인 같은 전체 Session의 정상 후속 cue/반복에는 첫 actual gate를 재적용하지 않는다. 원발생부터 최초 actual의 `<L`은 [Timing 잠정 조건](../../../../design/50_CONTRACTS/30_TIMING_FRESHNESS.md#first-start)이며 A1 D04에서 L=min(양수 USE_LIMIT,500ms,2000ms)를 선택했다. 실제 시간/성능은 별도 검증한다.

<a id="hold-rear"></a>
## Stateful Hold와 Rear

[D2 Store_ApplyInput](Store_ApplyInput.md#quality-hold)이 유지한 마지막 유효 판단, 현재 quality, **최초** firstLossAt와 원 validUntil 중 이른 holdAnchor를 읽는다. validUntil이 없으면 최초 상실만 기준이다. 기산 근거가 부족했던 경우에도 다음 INVALID의 lossAt/now로 교체하지 않는다. 동일 최초 사실의 비교 근거 보완만 가능하다.

단위/domain/epoch/reset/wrap 연속성·원 출처·고정 anchor를 확인한 뒤 `earliestNow>=anchor+Hold`만 확정 Hold 만료(EXPIRED·후보 제거)다. `latestNow<anchor+Hold`이면 [원본/Rear 조건](../../../../design/50_CONTRACTS/30_TIMING_FRESHNESS.md#hold-reselection)도 충분할 때 APPLICABLE/newSessionAllowed=true로 현재 유지 또는 old retirement 뒤 새 Session 재선택을 검토한다. 경계 걸침/비교 부족은 UNCERTAIN/newSessionAllowed=false와 원 부족 사유·revision을 노출하여 FLOW/PB의 보수적 종료·재확인을 요청한다. time 조기 반환 경로에서도 정상 후보를 남기지 않는다. CHANGED/RECONFIRM 반환 하나로 확정 만료를 추정하지 않는다. lastValidValue/Meta/품질/anchor·이력은 보존하고 CLEAR/위험 정상화/실제 STOP 완료를 합성하지 않는다. 유효 CLEAR/Rear DISABLED·activation 상실/riskFence 위반/이미 확정 만료는 부활시키지 않는다.

Rear Activation/위험 결합은 CORE-03이 반영한 현재 적용 가능한 관계를 소비한다. DISABLED는 계속 차단하고 ENABLED만으로 옛 위험/Hold를 되살리지 않는다. Emergency→Caution과 Hold 종료에 따른 실행 교체는 최신 후보 관측을 통해 FLOW/POLICY/PLAYBACK이 판단하며 STORE가 실제 출력을 중단하지 않는다.

<a id="feasibility"></a>
## 구현 가능 slice와 Gate

| 상태·근거 | 후속 독립 Host 후보 / 단일 종속 Gate |
|---|---|
| F1_LOCKED_SEMANTIC / [R5 기한](../../../../design/60_PSEUDOCODE/20_INPUT_TO_SELECTION.md#store-advancedeadlines) | notificationPending/stamp 불일치·START_IN_FLIGHT·Started+age>L·NO_START+만료를 주입. 기대 순서와 원 이력/replay 보존·RECONFIRM 대조. TEST_NOT_RUN |
| F2_PROVISIONAL_TESTABLE / [Timing](../../../../design/50_CONTRACTS/30_TIMING_FRESHNESS.md#first-start) | <L/정확히L/경계 걸침·domain/epoch 변경 fixture. Hold 기간은 **시험 전용 명시 가정**으로만 경계 검사 가능하며 제품값 채택 아님 |
| F3_PARTIAL_INDEPENDENT / [D2 quality/Hold](Store_ApplyInput.md#quality-hold) | ACTIVE→INVALID 반복/validUntil 없음·선행/후행·Rear disable·보호 부족. 최초 loss 고정·후보 제거만·now 합성 없음 검사 |
| F4_EXTERNAL_GATE / [Time/실행 TBD](../../../../design/90_BINDING/91_IMPLEMENTATION_TBD.md#execution) | 실제 timebase/wrap/정밀도·owner 직렬화·추적 표현과 Hold 정책 확정만 미정. 논리 분기 작성 가능 |
| F5_PHYSICAL_SAFETY_GATE / [HW](../../../../design/90_BINDING/91_IMPLEMENTATION_TBD.md#hardware) | PB가 제공할 과거 무출력/actual/전체 retirement의 실제 충분성 검증만 HW_UNVERIFIED. 현재 idle를 물리 증명으로 쓰지 않음 |
| F6_DEFERRED_SCOPE / [Binding](../../../../design/90_BINDING/90_C_FILE_API_MAPPING.md) | 펌웨어/Host/보드 실행·WINDOW 제품 연동은 별도 범위 |

현재 PLAYBACK 관측의 적용·통지는 [Playback_Advance 상세 설명](../../PLAYBACK/functions/Playback_Advance.md#pairing)에서 읽는다. 보관 C/H 범위에서 `Store_AdvanceDeadlines` 심볼과 제품 deadline ledger 대응은 미확인이며 BINDING_TBD다. 보관 pending-slot 구현의 정적 관측만 SOURCE_CONFIRMED이고, 그 존재가 원 occurrence/통지/expiry 충분성의 증거는 아니다. 동결 의미 근거는 LOGICAL_APPROVED, 외부 IF는 PROPOSED_IF다. 제품 채택 시 기존 STORE의 시간 비교/관측 adapter·정책 값 바인딩만 최소 변경하고 후보가 틀리면 그 부분을 재검토한다. 동결 `<L`/Hold 의미·D2를 바꾸려면 별도 CR이다.

[R2](../../../../design/30_FUNCTIONS/20_INPUT_STORE_FUNCTIONS.md#store-advancedeadlines) · [R3 Store](../../../../design/40_DATA/20_STORE_DATA.md#vssstatefulstate) · [R3 PB](../../../../design/40_DATA/40_PLAYBACK_DATA.md#vssplaybackobservation) · [R4 coherent read](../../../../design/50_CONTRACTS/30_TIMING_FRESHNESS.md#coherent-read) · [R5](../../../../design/60_PSEUDOCODE/20_INPUT_TO_SELECTION.md#store-advancedeadlines) · [R6](../../../../design/70_C_INTERFACE/00_HEADER_OWNERSHIP_MAP.md#r6-store-advancedeadlines)

## A1 입력 상실과 시간 경계

원 L과 원 시각 구간은 [Timing](../../../../design/50_CONTRACTS/30_TIMING_FRESHNESS.md#first-start)을 따른다. 입력 신뢰 상실의 replayAllowed=false는 NO_START_REEVALUATE/복구로 복원하지 않는다. in-flight/unknown의 만료는 미래 시작 차단·종료 요구이며 NO_START/Expired 합성이 아니다. 충분한 미시도/NO_START 및 retirement에서만 기한 종결을 확정한다. 늦게 도착한 적법 actual은 실제 상실 이전 권한/원 시각이 입증된 경우 원 occurrence에만 반영하고 종료를 취소하지 않는다.

Warning의 원 TTL 도달/첫 loss/고정 Hold는 별개다. no-RX에도 후보 제거와 재평가를 진행하며 lossAt을 점검 now로 바꾸지 않는다. 마지막 유효 Stateful의 Hold 내 재선택은 원 출처·시각·고정 anchor·Rear gate/fence와 old retirement 조건에서 허용한다. 확정 만료는 earliestNow>=anchor+Hold이며 경계 걸침/비교 부족은 UNCERTAIN·보수적 종료·재확인으로 구별한다.
