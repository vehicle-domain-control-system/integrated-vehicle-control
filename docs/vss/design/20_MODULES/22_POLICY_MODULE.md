# POLICY / SELECT Module — 읽기 전용 판단

> R1 설계 의미 유지 · R1.1 한국어 표현 교정 · 2026-10-07

[Module Overview](00_MODULE_OVERVIEW.md) · [상위 Layer](../10_LAYERS/20_SERVICE_LAYER.md)

## 1. 목적

차량 의미를 중앙 정책의 우선순위·음원·전체 패턴으로 연결하고 현재 관측에서 실행 판단을 반환한다.

## 2. 역할

POLICY의 중앙 정책(C06)은 의미 → Class/세부 순위 → Asset ID → 전체 패턴의 실행 중 읽기 전용 정책이다. POLICY의 선택 책임(C07)은 한 평가의 함께 판단 가능한 일관된 자료에서 `winner / keep / replace / wait`와 계획을 만든다.

## 3. 책임

- 현재 적용 가능한 후보와 출력 owner·제한을 함께 평가한다.
- 우선순위와 동일 의미 One-shot의 원본 발생 순서/identity를 비교한다.
- 의미 기반 판단·전체 계획을 FLOW에 반환한다.

## 4. 책임이 아닌 것

STORE·PLAYBACK·Fault 원본 변경, prepare/start/stop 발행, HW/PCM/decoder 제어는 맡지 않는다. POLICY는 선택 결과를 직접 실행하거나 다른 의미의 음원으로 fallback하지 않는다.

## 5. 소유 상태 / 데이터

중앙 정책 기준과 한 평가의 판단 자료를 POLICY가 소유한다. 실행 중 정책은 읽기 전용이며 채택 후 전체 계획의 진행 상태는 PLAYBACK 소유다. 읽은 STORE·출력 owner·제한 자료는 별도 원본으로 만들지 않는다.

## 6. 입력

FLOW가 연결한 함께 판단 가능한 일관된 후보 묶음, 현재 출력 owner, Backend/HEALTH 제한, 비교 가능한 시간을 받는다.

## 7. 출력

`winner / keep / replace / wait`와 해당 후보에 연결된 Asset ID/전체 계획을 반환한다. 판단 자료는 해당 평가가 유효한 동안 사용한다.

## 8. 사용하는 / 제공하는 주요 Core Function — R2

[Select_Choose](../30_FUNCTIONS/30_POLICY_PLAYBACK_FUNCTIONS.md#select-choose)

현재 Core는 읽기 전용 [Select_Choose](../30_FUNCTIONS/30_POLICY_PLAYBACK_FUNCTIONS.md#select-choose)다. 판단 결과와 의미 계획을 반환하며 Session 채택/start/stop은 PLAYBACK의 실행 책임이다.

## 9. 의존 Module

FLOW → POLICY → 필요한 RUNTIME이다. STORE·PLAYBACK·Backend·HEALTH 자료는 FLOW가 제공한 관측으로 읽으며 POLICY → 실행 Module의 직접 호출 의존은 없다.

## 10. 상태 전이 또는 주요 lifecycle

FLOW가 수집한 함께 적용 가능한 관측으로 후보의 유효성과 우선순위를 비교하고, 현재 출력을 유지/교체/대기할지 판단한다. Class는 **Emergency > Warning/Caution > Feedback**이며 내부 순위는 **[잠정]**이다.

| Class | 높은 순위 → 낮은 순위 |
| --- | --- |
| Emergency | Anti-Pinch → Rear Emergency → Occupant Hazard |
| Warning/Caution | Rear Caution → Lock Error |
| Feedback | Unlock Complete → Lock Complete → Goodbye → Welcome |

평가 자료 수집 → 비교 → 판단 반환 → FLOW가 실행 요구 연결의 흐름이다. 다음 평가에는 최신 관측을 사용한다.

## 11. Module-local Contract

같은 의미의 미시작 One-shot은 원본 발생 순서, 동률이면 고정 identity로 비교한다. 수신/queue 도착 순서를 사용하지 않는다. 높은 후보의 선점 판단은 유지하되 같은 의미의 늦은 옛 발생으로 Started된 현재 Session을 끊지 않는다. 낮은 후보나 새 후보 없음도 유효한 현재 One-shot 종료 근거가 아니다.

winner가 있어도 old owner·기한·Fault로 실행을 기다릴 수 있다. FULL/READY만으로 start를 허가하지 않는다. [PLAYBACK](23_PLAYBACK_MODULE.md)은 준비 전/실제 새 시작 직전에 최신 조건을 재검사한다. Asset ID/전체 정책을 전달하며 주소·decoder·DMA 설정은 계획에 넣지 않는다. 사용할 수 없는 음원을 다른 의미의 소리로 대체하지 않는다.

## 12. 대표 오류 / 예외 처리 원칙

- 수집 중 관련 상태 변화: 판단 자료를 재수집한다.
- 높은 후보지만 실행 제한/old 정리 중: 비교 결과와 대기를 분리하며 old 종료 의도를 취소하지 않는다.
- 미정 패턴/음원 가용 근거 부족: 다른 의미의 fallback을 만들지 않는다.

## 13. 관련 Function 문서

[Select_Choose](../30_FUNCTIONS/30_POLICY_PLAYBACK_FUNCTIONS.md#select-choose) · [기존 후보의 R2 판정](../30_FUNCTIONS/00_FUNCTION_OVERVIEW.md#candidate-decisions) · [이전 명칭의 의미 연결](../30_FUNCTIONS/00_FUNCTION_OVERVIEW.md#merged).

## 14. 관련 Data 문서

[Selection Decision / 전체 계획](../40_DATA/30_SELECTION_DATA.md#selection)

## 15. 관련 공통 Contract

[Ownership](../50_CONTRACTS/00_CONTRACT_OVERVIEW.md#ownership) · [Timing](../50_CONTRACTS/00_CONTRACT_OVERVIEW.md#timing)

공통 원문의 위치를 연결한다. 여러 Module에 동일한 규칙의 통합·중복 제거는 R4에서 수행한다.

## 16. TBD / Deferred

cue 수·무음·반복·출력 수준 등 미정 후보는 [정책 TBD](../90_BINDING/91_IMPLEMENTATION_TBD.md#policy)를 유지한다.

세부 순위는 [잠정]이며 판단 자료의 필드/타입·실제 Function 조건/prototype은 R2/R3에서 다룬다.
