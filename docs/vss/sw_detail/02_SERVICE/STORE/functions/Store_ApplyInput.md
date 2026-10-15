# Store_ApplyInput — 검증 입력의 수용과 현재 상태 적용

> A1 입력 개정 2026-10-15 · [현재 Network 연결](../../../../design/90_BINDING/92_NETWORK_BINDING.md). `L=min(양수 USE_LIMIT,500ms,2000ms)`; 최초 actual에만 적용. 과거 동결/PASS는 당시 범위의 이력이며 본문 변경은 이번 A1 결정이다. 구현·시험 NOT_RUN.

[VSS 지도](../../../01_VSS_MAP.md) / [SERVICE](../../00_LAYER_OVERVIEW.md) / [STORE](../00_MODULE_OVERVIEW.md) / CORE-03 · [한국어 의사코드](../pseudocode/Store_ApplyInput.md)

> GUIDE · 의미 기준: `VSS@0641b2c2268cf47450aeb50302638a45a7a3277f`. 실제 C 구현·Host·보드 시험은 NOT_RUN.

INPUT이 검증한 사건·상태·품질 중 하나를 STORE 원본에 적용하고, **신규 수용·동일 발생·갱신·거부**를 동기 반환한다.

<a id="contract"></a>
## 논리 원형과 반환

아래는 [최신 R6 논리 계약](../../../../design/70_C_INTERFACE/00_HEADER_OWNERSHIP_MAP.md#r6-store-applyinput)의 읽기 전용 표기다.

```text
VssStoreAdmission Store_ApplyInput(
    ONE_OF const VssOneShotEvent *event,
           const VssStateUpdate *state,
           const VssQualityChange *quality);
```

`ONE_OF`는 세 포인터 중 **정확히 하나만 존재**한다는 주석이며 C 키워드가 아니다. **R6 logical declarations != compile-ready C**. 실제 매개변수 배치·폭·Header·복사/참조 ABI는 TBD다. [R2](../../../../design/30_FUNCTIONS/20_INPUT_STORE_FUNCTIONS.md#store-applyinput)의 `const VssValidatedInput *input` 한 인자 원형은 과거 provisional 표기다. 별도 통합 타입·union·세 public Core를 만들지 않는다.

| 인자 | 필요한 의미와 유효 조건 | 자료 수명 |
|---|---|---|
| `event` / [VssOneShotEvent](../../../../design/40_DATA/10_INPUT_DATA.md#vssoneshotevent) | 검증된 One-shot `occurrence`, `signal`, 원본 `meta`. 현재 도어 상태만으로 사건을 만들지 않는다 | 호출 중 불변 borrow. 수용에 필요한 identity·Meta는 STORE가 자기 추적 수명으로 보호 |
| `state` / [VssStateUpdate](../../../../design/40_DATA/10_INPUT_DATA.md#vssstateupdate) | 검증된 최신 적용 가능한 Stateful 또는 Rear Activation의 `signal/value/meta` | 마지막 유효 판단·Activation에 필요한 값과 내부 참조를 보호 |
| `quality` / [VssQualityChange](../../../../design/40_DATA/10_INPUT_DATA.md#vssqualitychange) | 현재 적용 가능한 품질 변화 `signal/quality/basis/lossAt`. RX 없는 신뢰 상실도 해당 | 마지막 유효 판단과 별도로 최초 loss의 원 시간 문맥을 보호 |

반환 타입은 [VssStoreAdmission](../../../../design/40_DATA/20_STORE_DATA.md#vssstoreadmission)이다. 반환 후 불변 scalar이며 enum 숫자를 새로 정하지 않는다.

| 기존 값 | 호출 결과 | 반환 시 사후조건 |
|---|---|---|
| `VSS_STORE_ACCEPTED` | 신규 occurrence 수용 | 원 identity·Meta, Pending, 최종 통지 및 replay 추적 자원이 함께 확보되어 반영 완료 |
| `VSS_STORE_DUPLICATE` | 동일 원 발생의 재전달 | 기존 상태·원 age·최초/최종 사실·replay 보호 유지. 상세 이력은 STORE 읽기 관측에서 확인 |
| `VSS_STORE_UPDATED` | Stateful/품질/Rear 적용 완료 | 해당 STORE 원본만 반영. 품질 적용 완료는 유효 경고나 정밀 Hold 확정과 다름 |
| `VSS_STORE_REJECTED` | 부적합 요구 또는 신규 보호 자원 부족 | 기존 occurrence·Stateful·Rear 보존. INPUT의 이미 적용한 출처/순서를 rollback하지 않음 |
| `VSS_STORE_NOT_SUBMITTED` | **INPUT이 이 함수를 호출하지 않음** | INPUT 측 미호출 표지. STORE 내부의 0개 입력 거부 반환으로 쓰지 않음 |

`ACCEPTED`는 재생 시작 약속이 아니다. `UPDATED`도 새 Session/Attempt 생성이나 실제 출력 성공을 뜻하지 않는다.

<a id="ownership"></a>
## 호출과 변경 권한

Caller는 [Input_Process](../../../../design/30_FUNCTIONS/20_INPUT_STORE_FUNCTIONS.md#input-process)다. INPUT이 먼저 출처·generation·ordering·원 시간·PRODUCT/TEST 적용 문맥을 검증한다. STORE는 그 근거를 읽고 저장 대상과 연결하며 INPUT authority를 재검증하는 새 validator가 되지 않는다. 정확한 실물 direct call site는 BINDING_TBD다.

STORE의 writer 범위는 [VssOccurrenceRecord](../../../../design/40_DATA/20_STORE_DATA.md#vssoccurrencerecord), [VssStatefulState](../../../../design/40_DATA/20_STORE_DATA.md#vssstatefulstate), [VssRearGateState](../../../../design/40_DATA/20_STORE_DATA.md#vssreargatestate)다. Callee는 필요한 기존 Runtime 도구뿐이며 POLICY·HEALTH·PLAYBACK을 역호출하지 않는다. PLAYBACK Session/Attempt·actual 판정과 HEALTH Fault는 해당 owner에 남긴다.

신규 추적 부족을 Stateful 갱신의 일괄 거부로 확대하지 않는다. 기존 상태/품질 적용의 보호 조건은 해당 문맥에서 별도로 판단한다. 반영 중 관측 공개와 revision 갱신은 owner 반영 완료 뒤이며 실행 직렬화 수단은 TBD다.

<a id="admission"></a>
## One-shot 정상·중복·거부 경로

1. 먼저 배타 입력과 INPUT의 검증 근거·STORE 대상 연결을 확인한다. 0개·다중·부적합 요구는 변경 전에 `REJECTED`다.
2. 보호된 occurrence 및 accepted/final/replay 기록에서 **동일 원 발생**을 찾는다. 동일하면 `DUPLICATE`이며 Pending·age·Session을 새로 만들지 않는다. 저장 표현·재부팅/wrap/alias 처리·identity 직렬화는 미정이다. 같은 signal·requestId·현재 물리 상태만으로 동일성을 판정하지 않는다.
3. Domain이 새 LOCK/UNLOCK 요청의 목표 확인을 **새로운 유효 occurrence**로 확정했다면 이미 목표 상태라는 이유로 중복 처리하지 않는다. 현재 상태 반복, 실행 전 거부, 결과 미확인은 그 사건을 대신하지 않는다. 의미 생성은 Domain/INPUT 책임이다.
4. 신규 identity와 Pending부터 최초/최종 통지·replay까지의 자원을 수용 전에 확보한다. 불충분하거나 확보에 실패하면 신규만 `REJECTED`다. 기존 accepted/final 기록을 eviction하지 않는다. 일부 확보한 신규 임시 자원만 실제 무참조가 확인된 범위에서 정리한다.
5. 원 `occurrence/signal/originalMeta`, `VSS_OCCURRENCE_PENDING`, `VSS_TRACKING_PROTECTED`를 반영하고 최초/최종 fact는 아직 없는 상태로 공개한 뒤 `ACCEPTED`를 반환한다.

<a id="stateful"></a>
## 유효 상태와 Rear 결합

검증된 허용 조합만 적용한다. Anti-Pinch/Occupant는 유효 ACTIVE/CLEAR, Rear Risk는 EMERGENCY/CAUTION/CLEAR, Rear Activation은 ENABLED/DISABLED다. 모호한 순서·대상·조합을 성공 `UPDATED`로 채우지 않는다. 원 source/order의 판정은 INPUT의 근거를 소비하고 비교 방법을 새로 결정하지 않는다.

유효 상태는 `lastValidValue/lastValidMeta`와 현재 VALID 품질을 반영한다. 이전 Hold가 끝나는 유효 최신 판단은 옛 `firstLossAt/holdAnchor`의 참조를 안전 종료한다. CLEAR는 해당 후보를 제거한다. 반복 ACTIVE는 현재 판단 갱신이며 Session 재시작이 아니다. 다른 Stateful과 One-shot ledger는 유지한다.

Rear Activation은 위험 원본과 별개다. DISABLED이면 Rear 후보/Hold를 제거하고 `riskApplicable=false`로 차단한다. ENABLED로 바뀐 것만으로 옛 위험/Hold를 부활시키지 않는다. 현재 Activation에 적용 가능한 위험 근거를 확인한 뒤에만 결합한다. 계속 ENABLED인 경우에도 enable 메시지 자체가 위험 근거를 새로 생성하지 않는다. EMERGENCY→CAUTION은 관측으로 노출하고 old 출력 종료·재선택은 PLAYBACK/FLOW/POLICY가 맡는다.

<a id="quality-hold"></a>
## 품질 상실과 Hold

INVALID/STALE/미수신(NO_DATA 의미)은 유효 CLEAR가 아니다. R3 품질 enum은 `VSS_INPUT_VALID/INVALID/STALE/UNASSESSED`이고 **`VSS_INPUT_NO_DATA`를 추가하지 않는다**. 미수신은 INPUT이 기존 문맥에서 판단한 품질 변화 또는 미변화로 전달한다.

적용 가능한 품질 변화이면 현재 `quality`를 반영하되 마지막 유효 판단·Meta를 유지한다. 유효 경고가 없거나 이미 Rear DISABLED/CLEAR/Hold 만료로 후보가 제거된 경우 품질 상실만으로 새 후보·새 Hold를 만들지 않는다. 반복 상실은 제거된 후보를 되살리는 근거도 아니다.

기존 유효 경고의 **최초 상실**에서만 원 `lossAt`와 시간 문맥을 보호한다. 정의된 원 `validUntil`과 비교 가능하면 `min(firstLossAt, original validUntil)`을 기산으로 사용한다. validUntil이 없는 입력은 비교 가능한 최초 상실만 사용한다. epoch/연속성/시각이 부족하면 기산 확정을 보류하고 근거 부족을 남긴다. 현재 시각으로 채우거나 fresh 후보로 노출하지 않는다. 품질 자체를 적용했다면 반환은 `UPDATED`일 수 있다.

반복 INVALID/STALE·재전달·선점·복구에서 최초 loss/기산을 교체·연장하지 않는다. 최초 시각 자체가 부족했던 경우에도 다음 호출의 lossAt로 새 기산을 만들지 않는다. 보호한 **동일 최초 사실**의 비교 근거가 보완되는 경로만 가능하며 실물 수단은 TBD다. Hold 만료 처리는 `Store_AdvanceDeadlines`의 후속 책임이고 후보 제거를 CLEAR/VALID로 바꾸지 않는다.

<a id="lifetime-time"></a>
## 시간·참조와 후속 처리

인자 포인터와 그 안의 `validUntil` 같은 참조는 호출 수명과 분리한다. 후속에 필요한 값/참조는 [Ownership 보호 계약](../../../../design/50_CONTRACTS/10_OWNERSHIP_LIFETIME.md#borrow-and-protect)에 따라 보호한다. 얕은 struct 복사만으로 장기 보존을 충족했다고 가정하지 않는다. occurrence 회수는 참조·미반영 통지 종료와 replay 방어가 모두 입증될 때만 가능하다.

[Timing의 A1 최초 actual age <L](../../../../design/50_CONTRACTS/30_TIMING_FRESHNESS.md#first-start)는 수용 성공 자체의 판정이나 이 함수의 새 timeout이 아니다. 원발생 기준 최초 실제 시작에만 적용하며 정확히 L은 미허용이다. 이미 Started된 같은 Session의 정상 후속 cue에는 재적용하지 않는다. 최신 Network 70028972의 원 USE_LIMIT를 A1의 L로 보존한다.

이 함수 후 FLOW는 반영 완료 STORE 관측을 연결한다. 실제 시작·전체 최종 fact 적용은 [Store_ApplyPlaybackFact 상세 설명](Store_ApplyPlaybackFact.md), RX 없는 age/Hold 진행은 [Store_AdvanceDeadlines 상세 설명](Store_AdvanceDeadlines.md)에서 읽는다. 각각의 책임에 대한 정본은 [Store_ApplyPlaybackFact의 동결 설명](../../../../design/30_FUNCTIONS/20_INPUT_STORE_FUNCTIONS.md#store-applyplaybackfact)과 [Store_AdvanceDeadlines의 동결 설명](../../../../design/30_FUNCTIONS/20_INPUT_STORE_FUNCTIONS.md#store-advancedeadlines)이다.

<a id="source"></a>
## 실제 소스 근거와 확인 한계

보관 Arbitrator의 반복 WARNING_ACTIVE 무재시작·CLEAR 처리 경계만 SOURCE_CONFIRMED인 정적 관측이다. occurrence·Meta·최초 loss/Hold·Rear 전체 계약의 완성을 뜻하지 않는다. 보관 Session의 single pending overwrite와 수용 count 역시 최종/replay 추적 ledger의 대체 근거로 사용할 수 없다.

보관 C/H 범위에서 `Store_ApplyInput` 실물 symbol은 NOT_FOUND_IN_INSPECTED_SCOPE이며 실제 binding은 BINDING_TBD다. 기존 Arbitrator/Session 내부 책임의 재사용·최소 보정 후보로 연결하며 새 Store 파일·pool·public API를 선결정하지 않는다. [과거 Binding](../../../../design/90_BINDING/90_C_FILE_API_MAPPING.md)은 당시 13개 후보 이력이고 최신 Core18/R6를 대체하지 않는다.

<a id="feasibility"></a>
## 독립 SW 후보와 국소 미정

| 후보·지위 | 후속 Host 계획(모두 NOT_RUN) | 종속 제한·결정 owner / 변경 지점 |
|---|---|---|
| F1/F3 배타 입력·동일 발생·신규 자원 부족 | 0/1/다중 입력, 동일/새 occurrence, 추적 부족에서 기존 accepted/final 불변 | 실제 Meta/identity encoding·bounded 용량/회수는 Network/VSS/B2-R. 기존 STORE 적용·추적 위치를 최소 보정하는 후보 |
| F1/F3 상태/품질/Hold·Rear | ACTIVE→INVALID→반복→유효 CLEAR; 정의된/없는 validUntil; 비교 불가; DISABLED→ENABLED 후 옛 위험 부활 금지 | Hold 기간은 VSS 정책, 원 시간/ordering은 INPUT/Runtime/Network. 기산 불변식부터 독립 검증 가능 |
| F2 원 최초 actual <L 잠정 조건의 인계 | 원 fact 구간이 경계 앞/정확히/뒤/걸침인지 비교하는 후속 STORE/PLAYBACK fixture | 기존 시간 gate/설정 위치에서 최종 값 교체. 실제 actual 생산은 HAL/보드 Gate; 이 함수에 임의 기한 검사 추가 없음 |

후보가 원 계약과 불일치하거나 출처가 개정되면 해당 채택만 중단하고 기존 accepted/final·원 age·replay·품질 보호를 유지한다. 새 대체 수치 없이 owner 근거와 별도 구현 승인으로 재채택한다. Q01~14·R6-01~08 미결, 외부 IF/source-image/HW 미확정·P2 HOLD는 유지한다. 문서 관계 보정의 승인을 제품 ABI·실제 구현·보드 검증으로 확대하지 않는다. WINDOW/Anti-Pinch PRODUCT는 [구현 보류 — 설계 유지], 격리 TEST 의미만 유지한다.

<a id="references"></a>
## 원문으로 확인하기

[R2 함수](../../../../design/30_FUNCTIONS/20_INPUT_STORE_FUNCTIONS.md#store-applyinput) · [R3 입력](../../../../design/40_DATA/10_INPUT_DATA.md#vssoneshotevent) · [R3 STORE](../../../../design/40_DATA/20_STORE_DATA.md#store) · [R4 Ownership](../../../../design/50_CONTRACTS/10_OWNERSHIP_LIFETIME.md) · [R4 Timing](../../../../design/50_CONTRACTS/30_TIMING_FRESHNESS.md#stateful-hold) · [R5-C1 입력 흐름](../../../../design/60_PSEUDOCODE/20_INPUT_TO_SELECTION.md#store-applyinput) · [R6 논리 원형](../../../../design/70_C_INTERFACE/00_HEADER_OWNERSHIP_MAP.md#r6-store-applyinput) · [R6 미정 8항목](../../../../design/70_C_INTERFACE/00_HEADER_OWNERSHIP_MAP.md#tbd-register) · [Binding 미정](../../../../design/90_BINDING/91_IMPLEMENTATION_TBD.md) · [의사코드로 이동](../pseudocode/Store_ApplyInput.md)

## A1 적용 분기

```text
QUALITY lane: Event 신뢰 상실이면 관련 record.replayAllowed=false (단조 latch).
  미시도/충분한 NO_START+retirement만 Expired; in-flight/unknown은 원 상태·추적 유지.
  Stateful은 최초 loss/원 validUntil의 이른 anchor를 고정, 반복으로 연장하지 않음.
  activation 상실은 gate/riskApplicable=false, 현재 위험 Meta를 riskFence에 보호.
EVENT lane: 원 occurrence와 Meta/L 및 최초/최종/replay 자원 함께 예약(ledger128).
  같은 key/내용은 duplicate; key 같고 종류·원 발생 정보 충돌이면 거부/진단.
  full은 새 Event REJECTED; 기존 accepted/final/tombstone eviction 없음.
  신규 accepted만 replayAllowed=true; 같은 key 재전달/복구는 false를 되돌리지 않음.
STATE lane: DISABLED는 위험/Hold 제거와 riskFence 보호. ENABLED만으로 복원 없음.
  현재 fresh 활성과 fence보다 새로운 위험 근거를 확인한 때만 riskApplicable=true.
```

원 구조/별도 필드의 정본은 [STORE Data](../../../../design/40_DATA/20_STORE_DATA.md)와 [A1](../../../../design/90_BINDING/92_NETWORK_BINDING.md)다. 품질/기한/기존 사실 적용은 Event 신규 슬롯을 요구하지 않는다. INPUT contextApplied=true/STORE REJECTED는 동시에 가능하다. 거부의 원 key/L/단계를 기존 INPUT 진단에 남기며 Network callback을 자동 재호출하지 않는다.
