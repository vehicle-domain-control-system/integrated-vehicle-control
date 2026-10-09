# PLAYBACK Module — 단일 Session 실행

> R1 설계 의미 유지 · R1.1 한국어 표현 교정 · 2026-10-07

[Module Overview](00_MODULE_OVERVIEW.md) · [상위 Layer](../10_LAYERS/20_SERVICE_LAYER.md)

## 1. 목적

선택된 전체 정책을 단일 출력 Session으로 진행하고 귀속된 실제 출력 결과로 재생 이력을 연결한다.

## 2. 역할

PLAYBACK(C08)은 단일 출력 owner, Session/Attempt·인과 요청 문맥, 종료 의도와 전체 계획의 start/cue/반복 권한을 관리한다.

## 3. 책임

- 최신 실행 조건을 확인해 원래 출력 시도의 prepare/start/stop을 AUDIO STREAM에 요청한다.
- 귀속된 Backend 결과를 전체 Session 범위에 적용하고 STORE에 재생 사실을 통지한다.
- 종료 의도·후속 권한 차단·uncertain 최종·retirement를 관리한다.

## 4. 책임이 아닌 것

Occurrence ledger/원본 age·replay는 STORE, provider/PCM usage·Backend 결과는 AUDIO STREAM, 장치 근거는 DRIVER/HAL, Fault 해제는 HEALTH 책임이다. PLAYBACK은 이 원본들과 HW를 직접 변경하지 않는다.

## 5. 소유 상태 / 데이터

Occurrence/Session/Attempt·각 인과 operation의 다른 수명은 [Ownership §2](../50_CONTRACTS/10_OWNERSHIP_LIFETIME.md#identity-lifetimes)를 따른다. PLAYBACK은 Session/Attempt 원본을 소유하며 cue당 Attempt/operation 개수·실제 표현은 고정하지 않는다.

출력 owner·전체 계획 진행·종료 의도·후속 권한·해당 요청·출력 시도에 귀속된 결과 통지 의무의 단일 writer는 PLAYBACK이다. lower 문맥은 원래 요청 귀속의 보존이며 Session의 두 번째 writer가 아니다.

## 6. 입력

FLOW가 전달한 선택 판단·최신 후보/기한/HEALTH 제한, AUDIO STREAM의 해당 요청·출력 시도에 귀속된 Backend 결과, 처리 기회·종료/기한 요구를 받는다.

## 7. 출력

AUDIO STREAM에 원래 출력 시도의 prepare/start/stop 요구, STORE에 귀속된 최초 시작·전체 완료/중단·uncertain 사실, FLOW에 현재 출력 관측·재평가/정리 필요와 발생 주체에 연결된 진단 근거를 제공한다.

## 8. 사용하는 / 제공하는 주요 Core Function — R2

[Playback_RequestTransition](../30_FUNCTIONS/30_POLICY_PLAYBACK_FUNCTIONS.md#playback-requesttransition) · [Playback_Advance](../30_FUNCTIONS/30_POLICY_PLAYBACK_FUNCTIONS.md#playback-advance)

기존 `Playback_Process`는 의도 수용/후속 권한 차단과 진행 중 Session/Attempt 사실 적용·늦은 결과/최종 격리로 분리했다. 하위 준비/제어는 AUDIO STREAM Core, 확정 이력 반영은 [Store_ApplyPlaybackFact](../30_FUNCTIONS/20_INPUT_STORE_FUNCTIONS.md#store-applyplaybackfact)에 연결한다. Start/Stop/Retire별 wrapper는 만들지 않는다.

## 9. 의존 Module

FLOW → PLAYBACK → AUDIO STREAM / STORE / 필요한 RUNTIME이다. HEALTH 제한은 FLOW가 전달한다. Backend 결과 반환은 원래 요청 경계이며 상위 정책 역호출이나 HW 직접 의존을 만들지 않는다.

## 10. 상태 전이 또는 주요 lifecycle

최신 후보를 채택하기 전에 이전 출력의 종료·차단 확인 조건(fence)·유효성·기한·제한을 검사해 원래 출력 시도의 prepare를 요청한다. prepared 결과를 받아도 실제 새 start 직전에 CLEAR·원본 age·owner·Fault 등 최신 조건을 다시 확인한다. 원본 기한과 불변 문맥은 하위의 실제 전송/출력을 위한 장치 활성화(arm) 및 실제 출력 관측까지 유지한다. 이미 Started된 동일 Session의 정상 후속 cue는 새 occurrence가 아니다.

| 상태 | 의미 |
| --- | --- |
| PREPARING | 원래 출력 시도의 Asset/decode/prime 준비이며 출력 허가 아님 |
| START_IN_FLIGHT | 수락/실제 결과 추적 중. 정상 대기 자체는 Fault 아님 |
| ACTIVE | 적법한 actual start가 확인된 정상 전체 정책 진행 |
| STOPPING / ABORTING | 후속 start/cue/반복 차단과 old 정리. 요청 수락만으로 종료 아님 |
| QUARANTINED | 시작 결과 불명확 최종·새 시작 차단. old 정리는 계속 |
| RETIRED | 전체 출력의 종료·차단 확인 조건·후속 권한 차단을 확인하고 owner 해제 |

preempt·유효 CLEAR·고장·전체 정책 끝에서 미발행 권한을 차단하고 Audio 정리를 요청한다. 종료 중 winner 변화가 종료 의도를 취소하지 않는다. old retirement 뒤 최신 후보를 재선택한다. Started One-shot 자동 resume는 없다.

## 11. Module-local Contract

요구 단계·결과 역전의 공통 규칙은 [Async §1](../50_CONTRACTS/20_ASYNC_RESULT_EVIDENCE.md#stages)을 따른다. 정상 다음 cue는 이전 관련 Backend 구간의 안전 끝과 현재 계획을 확인해 진행하며, 한 cue/EOS 종료로 전체 Completed를 기록하지 않는다. 다른 Session은 old output retirement 전 prepare/start하지 않는다.

<a id="outcomes"></a>
### 요청·출력 시도에 귀속된 결과와 늦은 결과·불확실한 결과의 로컬 적용

| 해당 요청·출력 시도에 귀속된 결과/적용 시점 | PLAYBACK 처리 |
| --- | --- |
| `OUTPUT_START_CONFIRMED` | 원래 출력 시도·사실 시각·기한/권한이 적법한 최초 실제 시작만 STORE에 통지. 정상은 ACTIVE |
| 종료 중 최종 확정 전 late actual | 같은 원래 출력 시도의 적법한 Started/보고 사실은 반영하되 STOPPING/ABORTING 유지. ACTIVE/후속 cue 재개 없음 |
| `NO_START_CONFIRMED` | Backend의 충분 무출력/미래 차단 근거와 상위 전체 권한 차단 확인 → retirement. uncertain 최종이 없는 원래 발생만 STORE Pending/Expiry 재평가 |
| `OUTPUT_TERMINATION_CONFIRMED` | Backend 전체 출력 종료 + 모든 상위 start/cue/반복 차단·late 귀속 보호. 완료/Started 중단을 STORE에 통지하고 retire |
| `START_OUTCOME_UNCERTAIN` | uncertain 최종·replay 금지와 QUARANTINED. 기존 **FAULT + UNAVAILABLE + PLAYBACK_STATE_FAILURE** 근거를 FLOW로 전달. old 정리 별도 지속 |
| uncertain 최종 뒤 / retired 문맥 | 옛 정리·진단만. 정상 Pending/Started/Completed 부활과 새 Session/Buffer 변경 없음 |

actual/no-start/end의 충분조건은 [Async §3~4](../50_CONTRACTS/20_ASYNC_RESULT_EVIDENCE.md#confirmed-outcomes)를 따른다. PLAYBACK은 위 표대로 전체 권한 차단·기한 적법성·최종 전/뒤 적용을 추가하며 기한 밖 출력은 물리 사실/위반 근거로 보존해 정리한다.

uncertain 최종 기록은 retirement보다 먼저 가능하다. 결과 통지 의무는 반영 완료 또는 보호된 추적으로 유지한다. owner 해제는 Fault 해제/이력 삭제/callback 저장 폐기가 아니며 자원 수명은 [HAL/BSP](../10_LAYERS/details/40_HAL_BSP_BOUNDARY.md#lifetime)를 따른다.

## 12. 대표 오류 / 예외 처리 원칙

준비/시작 실패는 해당 Attempt의 정리를 진행한다. 결과 불명확은 QUARANTINED이며 정상 in-flight 대기와 구별한다. late 결과는 §11의 최종 전/뒤 범위에만 적용한다. 충분근거가 없으면 성공·미시작·전체 종료를 추정하지 않으며 기한 밖 출력은 물리 사실/위반 근거를 남겨 정리한다.

## 13. 관련 Function 문서

[Playback_RequestTransition](../30_FUNCTIONS/30_POLICY_PLAYBACK_FUNCTIONS.md#playback-requesttransition) · [Playback_Advance](../30_FUNCTIONS/30_POLICY_PLAYBACK_FUNCTIONS.md#playback-advance) · [기존 후보의 R2 판정](../30_FUNCTIONS/00_FUNCTION_OVERVIEW.md#candidate-decisions) · [이전 명칭의 의미 연결](../30_FUNCTIONS/00_FUNCTION_OVERVIEW.md#merged).

## 14. 관련 Data 문서

[Playback Context — Session / Attempt](../40_DATA/40_PLAYBACK_DATA.md#playback) · [Occurrence 이력](../40_DATA/20_STORE_DATA.md#store)

## 15. 관련 공통 Contract

[Async / Evidence](../50_CONTRACTS/20_ASYNC_RESULT_EVIDENCE.md) · [Ownership / Lifetime](../50_CONTRACTS/10_OWNERSHIP_LIFETIME.md) · [Timing](../50_CONTRACTS/30_TIMING_FRESHNESS.md)

공통 규칙은 위 R4 계약을 따르며 이 문서의 owner·상태 적용·실패/예외는 로컬 조건으로 유지한다.

## 16. TBD / Deferred

identity·해당 요청·출력 시도에 귀속된 결과/통지 보호는 [identity TBD](../90_BINDING/91_IMPLEMENTATION_TBD.md#identity), 미정 timeout/retry는 [정책 TBD](../90_BINDING/91_IMPLEMENTATION_TBD.md#policy)를 확인한다. cue당 Attempt 수·실제 표현을 고정하지 않는다. 실제 함수/prototype·필드/타입은 R2/R3 후속이다.
