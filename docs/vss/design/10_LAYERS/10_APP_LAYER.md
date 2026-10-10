# APP Layer

> R1 Layer 설계 유지 · R1.1 한국어 표현 교정 · 2026-10-07

[Layer Overview](00_LAYER_OVERVIEW.md)

## 1. 목적

VSS의 Service 처리와 판단·실행·보고·복구 순서를 하나의 애플리케이션 흐름으로 연결한다.

## 2. 역할

APP의 FLOW는 초기화·입력과 독립된 진행, POLICY의 선택 판단과 PLAYBACK의 실행 흐름, 보고·허용된 복구를 조율한다. POLICY가 재생 대상과 계획을 선택하고 PLAYBACK이 전체 Session을 실행한다. APP는 각 Service의 결과를 연결하며 업무 상태 원본은 해당 Module에 둔다.

## 3. 책임

- 필요한 Service에 처리 기회를 제공한다.
- 반영 완료 관측의 일관성을 확인하고 POLICY의 판단과 PLAYBACK의 실행을 연결한다.
- 진단 보고와 허용된 복구의 순서를 조율한다.

## 4. 책임이 아닌 것

입력 검증, 발생 이력, 출력 Session, PCM usage, Fault 원본의 판단·변경은 해당 Service 책임이다. vendor/RTD·장치 전송·IRQ 처리는 하위 책임이다.

## 5. 포함 Module / 실제 내부 책임

[FLOW](../20_MODULES/10_FLOW_MODULE.md) 1개 논리 Module이다. APP의 조율 상태와 임시 읽기 묶음만 소유하며 실제 Task/file 하나를 만든다는 뜻이 아니다.

## 6. 입력 / 출력

| 구분 | 의미 |
| --- | --- |
| 입력 | startup·제품/TEST 입력·처리 기회, Service의 반영 완료 관측·진단/복구 결과 |
| 출력 | Service 처리/선택/재생/보고 연결 요구와 허용된 복구 진행 조율 |

## 7. 허용 의존 방향

APP → INPUT / STORE / POLICY / PLAYBACK / AUDIO STREAM / ASSET / HEALTH의 공개 의미 경계와 필요한 RUNTIME이다. 하위 결과는 반환/관측 자료로 받는다.

## 8. 금지 의존 방향

APP → DRIVER/HAL/RTD 직접 호출, PCM 주소/descriptor 처리, 다른 Module 원본 상태 직접 변경을 금지한다. Service가 FLOW 정책을 역호출하는 새 의존을 만들지 않는다.

## 9. Layer 수준 Contract

APP는 조율만 수행하며 상태 변경은 해당 owner의 공개 경계에서 완료한다. 의미 처리를 ISR로 옮기지 않는다. 함께 판단 가능한 일관된 상태 묶음과 해당 요청·출력 시도에 귀속된 결과 적용의 구체 조건은 FLOW 상세를 따른다.

## 10. 대표 처리 흐름

입력/owner 변화 → Service 진행 → 반영이 끝나 함께 판단 가능한 일관된 상태 묶음 → POLICY 판단 → PLAYBACK 실행 요구 → 진단/보고 연결이다. 초기 준비·종료/복구 순서는 FLOW가 해당 owner 결과를 연결한다.

## 11. Module / 하위 상세 링크

[FLOW 책임·owner·lifecycle](../20_MODULES/10_FLOW_MODULE.md) · [SERVICE](20_SERVICE_LAYER.md)

## 12. 관련 공통 Contract

[Ownership](../50_CONTRACTS/00_CONTRACT_OVERVIEW.md#ownership) · [Timing](../50_CONTRACTS/00_CONTRACT_OVERVIEW.md#timing) · [Fault / Recovery](../50_CONTRACTS/00_CONTRACT_OVERVIEW.md#fault)

## 13. TBD / Deferred

실제 실행 문맥·주기·직렬화·처리 예산은 [실행 TBD](../90_BINDING/91_IMPLEMENTATION_TBD.md#execution)다. R1은 조율 경계 정규화이며 Task/Queue/Mutex·Function/prototype을 확정하지 않는다.
