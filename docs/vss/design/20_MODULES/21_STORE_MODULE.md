# STORE Module — 발생 이력과 현재 후보

> R1 설계 의미 유지 · R1.1 한국어 표현 교정 · 2026-10-07

[Module Overview](00_MODULE_OVERVIEW.md) · [상위 Layer](../10_LAYERS/20_SERVICE_LAYER.md)

## 1. 목적

수용한 One-shot 발생의 이력·replay 방어와 현재 Stateful 경고 후보를 분리해 보존한다.

## 2. 역할

STORE(C05)는 검증 입력과 귀속된 재생 사실을 적용하고 원본 age·품질·Hold·Rear 결합을 관리한다. 값+Meta·품질·기한·Rear 정보는 함께 판단 가능한 상태로 노출한다.

## 3. 책임

- accepted 전에 최종까지의 추적 자원을 확보하고 occurrence ledger/replay를 유지한다.
- PLAYBACK의 적법한 최초 시작과 전체 최종 결과를 해당 발생에 반영한다.
- Stateful 유효성·품질·Hold·Rear 위험/Activation을 갱신하고 기한을 진행한다.

## 4. 책임이 아닌 것

INPUT의 검증 출처를 적용할 뿐 별도 source authority가 아니다. Session/Attempt·출력 owner, 선택 정책, decoder/PCM, 진단 Fault는 소유하지 않는다.

## 5. 소유 상태 / 데이터

accepted 발생·현재 경고 후보, 품질·Hold·Rear 결합, occurrence ledger·원본 age·replay guard의 단일 writer는 STORE다. One-shot 이력은 Session 종료 뒤에도 필요하며 Stateful의 마지막 유효 판단·현재 품질·사용 가능한 후보는 구분해 보존한다.

## 6. 입력

INPUT의 검증된 적용 문맥·사건/상태, PLAYBACK의 원래 발생·출력 시도에 귀속된 재생 사실, 비교 가능한 시간과 기한 진행 요구를 받는다.

## 7. 출력

수용/duplicate/rejected 결과, 함께 판단 가능한 현재 후보·기한·품질 관측, 발생 이력/replay 근거를 제공한다. FLOW가 관측을 연결하고 POLICY/PLAYBACK 및 HEALTH 보고가 소비한다.

## 8. 사용하는 / 제공하는 주요 Core Function — R2

[Store_ApplyInput](../30_FUNCTIONS/20_INPUT_STORE_FUNCTIONS.md#store-applyinput) · [Store_ApplyPlaybackFact](../30_FUNCTIONS/20_INPUT_STORE_FUNCTIONS.md#store-applyplaybackfact) · [Store_AdvanceDeadlines](../30_FUNCTIONS/20_INPUT_STORE_FUNCTIONS.md#store-advancedeadlines)

기존 `Store_Process`는 입력 수용, 확정 재생 사실 반영, 최신 출력 사실 재확인 뒤 기한 진행으로 분리했다. 세 Core 모두 STORE의 원본만 변경하며 신규 수용 거부와 기존 통지 반영 의무를 구별한다.

## 9. 의존 Module

INPUT·FLOW·PLAYBACK → STORE → 필요한 RUNTIME이다. POLICY/HEALTH에 제공하는 읽기 관측은 FLOW가 연결하며 STORE가 선택·보고 Module을 직접 호출하는 새 의존을 만들지 않는다.

## 10. 상태 전이 또는 주요 lifecycle

<a id="occurrence"></a>
### One-shot occurrence

One-shot은 Welcome, Goodbye, Lock Complete, Unlock Complete, Lock Error다. Welcome/Goodbye는 전이 사건이며 Lock/Unlock Complete는 새 요청의 목표 확인이다. Lock Error는 실제 LOCK 뒤 목표 확인 실패다. 현재 상태·실행 전 거부·결과 미확인을 새 발생으로 만들지 않는다.

| 상태/결과 | STORE 적용 |
| --- | --- |
| Pending | accepted이나 첫 실제 시작 전. in-flight/불명확 시도는 age 도달만으로 Expired 확정 금지 |
| Started | PLAYBACK의 원래 발생·출력 시도·사실 시각에 귀속된 최초 적법한 actual start. 같은 시작 중복 통지는 이력 재변경 없음 |
| Completed / Interrupted | Started 후 전체 정책 완료 / 선점·고장 중단이며 안전 전체 출력 종료와 후속 권한 차단이 확인됨 |
| Expired | 확정 미시작이며 원본 새 시작 기한에 도달. 안전 retirement 후 아직 유효한 미시작은 기존 조건 아래 Pending 재평가 가능 |
| uncertain 최종 | 시작 결과 불명확 최종·replay 금지. 실제 old 출력 정리와는 독립 |

<a id="stateful"></a>
### Stateful·품질·Hold·Rear

Anti-Pinch, Occupant Hazard, Rear Obstacle은 현재 유효 경고를 후보로 유지한다. 같은 ACTIVE 반복은 cue/Session 재시작이 아니다. 유효 CLEAR는 해당 후보를 제거한다. INVALID·STALE·미수신·무음은 CLEAR가 아니며 마지막 유효 판단, 현재 품질, 사용 가능한 후보를 분리한다.

기존 유효 경고가 신뢰성을 잃으면 **최초 신뢰성 상실과 원본 유효기한 중 더 이른 기준**으로 Hold를 기산한다. 반복 INVALID·재전달·선점·복구로 연장하지 않는다. Hold 만료는 후보 제거이며 품질 정상화가 아니다. 기존 경고 없이 INVALID만으로 새 경고를 만들지 않는다.

Rear 위험과 Activation은 최신 적용 가능한 값으로 결합한다. Activation 자체는 소리 후보가 아니다. DISABLED는 Rear 후보/Hold 제거이며 CLEAR와 다르다. 재활성화만으로 옛 EMERGENCY를 부활시키지 않는다. EMERGENCY→CAUTION은 old 출력 종료 뒤 재선택한다. 선점 자체는 CLEAR가 아니며 살아 있는 Stateful은 새 Session 정책 시작점으로 재생한다. PCM offset resume는 없다. WINDOW 상태는 [INPUT](20_INPUT_MODULE.md)의 보류 기준을 따른다.

## 11. Module-local Contract

최초 시작·원 age·Started 정상 후속·USE_LIMIT 구분은 [Timing §2~3](../50_CONTRACTS/30_TIMING_FRESHNESS.md#first-start)을 따른다. STORE는 동일 발생의 최초 적법한 Started와 전체 최종만 원래 ledger에 적용하며 동일 발생의 전체 정책은 한 Session으로 읽는다.

accepted는 Pending와 identity·최종 처리까지의 추적 자원을 함께 확보해야 노출한다. 신규 공간 확보를 위해 기존 accepted/final 이력을 임의 eviction하지 않는다. 이력 회수는 참조 종료와 재전달/replay 방어 근거가 있을 때만 가능하다. 재부팅·시간/출처 연속성 상실도 옛 발생을 새 사건으로 바꾸지 않는다. 신규 One-shot 수용과 Stateful 갱신 자원은 구별한다.

시작/만료 경합에서는 FLOW가 연결한 최신 PLAYBACK 사실을 재확인한다. 함께 판단 가능한 일관된 읽기 관측은 STORE 원본의 두 번째 writer가 아니다.

## 12. 대표 오류 / 예외 처리 원칙

- 같은 occurrence 재전달: 기존 결과/replay를 확인하고 새 Pending·age를 만들지 않는다.
- 신규 추적 공간 부족: 신규 수용을 거부하고 기존 수용/이력을 보존한다.
- in-flight·uncertain·시간 근거 상실: 정상 만료/Started/완료를 추정하지 않는다. 최종 처리는 [PLAYBACK 결과](23_PLAYBACK_MODULE.md#outcomes)를 따른다.

## 13. 관련 Function 문서

[Store_ApplyInput](../30_FUNCTIONS/20_INPUT_STORE_FUNCTIONS.md#store-applyinput) · [Store_ApplyPlaybackFact](../30_FUNCTIONS/20_INPUT_STORE_FUNCTIONS.md#store-applyplaybackfact) · [Store_AdvanceDeadlines](../30_FUNCTIONS/20_INPUT_STORE_FUNCTIONS.md#store-advancedeadlines) · [기존 후보의 R2 판정](../30_FUNCTIONS/00_FUNCTION_OVERVIEW.md#candidate-decisions) · [이전 명칭의 의미 연결](../30_FUNCTIONS/00_FUNCTION_OVERVIEW.md#merged).

## 14. 관련 Data 문서

[Occurrence / Stateful Store](../40_DATA/20_STORE_DATA.md#store)

## 15. 관련 공통 Contract

[Ownership](../50_CONTRACTS/10_OWNERSHIP_LIFETIME.md) · [Timing](../50_CONTRACTS/30_TIMING_FRESHNESS.md) · [Async](../50_CONTRACTS/20_ASYNC_RESULT_EVIDENCE.md)

공통 규칙은 위 R4 계약을 따르며 이 문서의 owner·상태 적용·실패/예외는 로컬 조건으로 유지한다.

## 16. TBD / Deferred

identity/이력 회수·보호 용량은 [identity TBD](../90_BINDING/91_IMPLEMENTATION_TBD.md#identity), 미정 Hold/정책값은 [정책 TBD](../90_BINDING/91_IMPLEMENTATION_TBD.md#policy)를 확인한다.

R2 Function 상세화·R3 필드/타입 설계 전이다. age/Hold/replay와 기존 잠정 정책을 이번 정규화로 재결정하지 않는다.
