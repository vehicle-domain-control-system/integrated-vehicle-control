# INFRA Layer

> R1 Layer 설계 유지 · R1.1 한국어 표현 교정 · 2026-10-07

[Layer Overview](00_LAYER_OVERVIEW.md)

## 1. 목적

각 owner가 의미 기한과 비동기 진행을 판단할 비교 가능한 공통 근거를 제공한다.

## 2. 역할

RUNTIME은 기존 공통 시간·알림/제한된 전달 도구를 제공한다. 시간의 비교 가능성·연속성과 도구의 진행/손실만 소유하고 상위 정책/상태는 변경하지 않는다.

## 3. 책임

- 비교 가능한 시간·연속성을 제공하고 해당 요청·출력 시도에 연결된 사실의 후속 처리 기회를 제공한다.
- coalesce/overflow·귀속/연속성 손실을 owner가 알 수 있게 전달한다.
- 기존 발생 주체의 측정 표시(marker)를 비교하고 연결하는 처리를 지원한다.

## 4. 책임이 아닌 것

age/Hold/start·정리 timeout의 의미 판단, 출력 성공/미시작/종료 판정, Session/발생 이력/Fault 변경은 해당 owner 책임이다. 알림만으로 실제 사실을 생성하지 않는다.

## 5. 포함 Module / 실제 내부 책임

**RUNTIME은 공통 Boundary로 유지한다.** 공통 시간·알림 도구의 유효성/손실 owner와 구현 수명은 존재하지만, 현재 설계에는 이를 별도 도메인 Module의 상태/lifecycle로 분리할 필요가 없다. 필요한 Layer가 기존 공개 도구를 사용하며 새 Runtime Module·getter·measurement wrapper는 추가하지 않는다.

## 6. 입력 / 출력

| 구분 | 의미 |
| --- | --- |
| 입력 | 기존 시간원/연속성 변화, 각 책임이 전달하는 요청·출력 시도에 연결된 사실의 보존·알림/진행 요구 |
| 출력 | 비교 가능한 시간/연속성, 후속 처리 기회와 coalesce/overflow·귀속/연속성 손실 근거 |

## 7. 허용 의존 방향

필요한 APP/SERVICE/DRIVER/HAL → 기존 INFRA 공개 도구다. 도구는 기존 시간원·실행/알림 근거를 사용하며 원본 사실의 측정 표시(marker)를 owner가 제공한다. 새 정책 역호출을 만들지 않는다.

## 8. 금지 의존 방향

INFRA → 차량 정책·STORE/PLAYBACK/HEALTH 원본 변경을 금지한다. 전달/처리 현재 시각으로 발생 주체의 원본 사실을 재생산하거나 시간 연속성 상실을 정상 성공/새 age로 보정하지 않는다.

## 9. Layer 수준 Contract

공통 근거와 의미 판단 owner를 분리한다. PRODUCT/TEST 시간 문맥은 격리하고 손실/불확실성은 숨기지 않는다. 시간·알림·marker의 실제 적용 상세는 Layer 하위 Boundary를 따른다.

## 10. 대표 처리 흐름

기존 시간/알림 근거 → 비교/연속성·손실 보존 → 해당 owner의 처리 기회 → owner의 의미 판단이다. RX 없는 진행 조율은 FLOW, ISR에서 요청 전에 저장한 귀속 정보로 사실을 포착하는 처리는 HAL 경계에서 수행한다.

## 11. Module / 하위 상세 링크

독립 Module을 추가하지 않았다. [RUNTIME 시간·알림·marker 상세](details/50_TIMEBASE_RUNTIME_BOUNDARY.md) · [FLOW](../20_MODULES/10_FLOW_MODULE.md) · [HAL/BSP](40_HAL_BSP_LAYER.md)

## 12. 관련 공통 Contract

[Timing / Freshness](../50_CONTRACTS/00_CONTRACT_OVERVIEW.md#timing) · [Async / Evidence](../50_CONTRACTS/00_CONTRACT_OVERVIEW.md#async)

## 13. TBD / Deferred

timebase·wrap/reset/timestamp·포착 지점/정밀도·직렬화·예산/보호 용량은 [실행 TBD](../90_BINDING/91_IMPLEMENTATION_TBD.md#execution), 물리 출력 상관은 [HW TBD](../90_BINDING/91_IMPLEMENTATION_TBD.md#hardware)다. 기존 성능 요구사항(PER-001~010)의 후보 상태를 유지하고 성능 PASS·Queue/Mutex/Task/Cache를 선결정하지 않는다.
