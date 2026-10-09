# FLOW Module — VSS 진행 조율

> R1 설계 의미 유지 · R1.1 한국어 표현 교정 · 2026-10-07

[Module Overview](00_MODULE_OVERVIEW.md) · [상위 Layer](../10_LAYERS/10_APP_LAYER.md)

## 1. 목적

각 owner의 처리를 연결해 입력이 없는 동안에도 POLICY의 선택 판단, PLAYBACK의 출력 실행, 각 Service의 정리·보고가 진행되게 한다.

## 2. 역할

FLOW(C01)은 초기화와 처리 기회, POLICY의 선택 판단과 PLAYBACK의 실행 흐름, 보고·복구 순서를 조율한다. Service의 공개 의미 경계를 호출하고 반영 완료 관측을 연결한다.

## 3. 책임

- 초기 준비 확인과 각 Service의 bounded 진행 기회를 연결한다.
- 함께 판단 가능한 관측을 수집하고 변화 시 재평가한다.
- POLICY 판단을 PLAYBACK에 전달하고 진단/보고 및 허용된 복구 순서를 조율한다.

## 4. 책임이 아닌 것

재생 대상·계획의 선택은 POLICY, 전체 Session 실행·결과 적용은 PLAYBACK 책임이다. 입력 검증·STORE 발생 이력·PLAYBACK Session·AUDIO STREAM PCM·HEALTH Fault의 원본 변경은 각 Service 책임이다. FLOW는 Driver/HAL/RTD 호출, PCM 주소/descriptor 처리, ISR 내부 의미 처리를 맡지 않는다.

## 5. 소유 상태 / 데이터

조율 단계·재평가 필요성·판단에 사용하는 임시 일관된 상태 묶음만 FLOW가 소유한다. 임시 관측은 소비 중인 owner 자료를 연결한 것이며 독립적인 상태 원본이 아니다.

## 6. 입력

startup·장치 준비 변화, 제품/TEST 입력, 시간·후속 알림, owner 반영 완료/변화, 진단 결과·복구 허용/검증 결과가 들어온다. 알림은 처리 기회이며 실제 출력 사실을 대신하지 않는다.

## 7. 출력

| 입력 | 연결하는 결과 |
| --- | --- |
| startup·장치 준비 변화 | AUDIO STREAM/ASSET 확인 → DIAGNOSTIC STATUS 평가/보고 |
| 제품/TEST 입력, 시간·후속 알림 | INPUT·STORE·Audio·PLAYBACK의 필요한 진행 기회 |
| owner 반영 완료/변화 | 함께 판단 가능한 일관된 상태 묶음 → POLICY의 winner/keep/replace/wait → PLAYBACK 요구 |
| 진단 결과·복구 허용/검증 | 기존 보고 전달 또는 old 정리·Backend 복구·대상 해제 평가 |

상태 보고 경로는 HEALTH → FLOW → INPUT의 통신 연결 책임(C02) → 기존 Node Communication이다.

## 8. 사용하는 / 제공하는 주요 Core Function — R2

[Flow_Process](../30_FUNCTIONS/10_FLOW_FUNCTIONS.md#flow-process)

현재 Core 진입은 [Flow_Process](../30_FUNCTIONS/10_FLOW_FUNCTIONS.md#flow-process)다. 선택·실행·보고·허용 복구는 각 Service Core를 호출해 조율한다. 별도 초기화/getter/publisher wrapper는 만들지 않는다.

## 9. 의존 Module

FLOW → INPUT / STORE / POLICY / PLAYBACK / AUDIO STREAM / ASSET / HEALTH의 공개 의미 경계와 필요한 RUNTIME을 사용한다. Service 결과는 반환/관측 자료로 연결하며 Service → FLOW의 새 정책 역호출 의존을 만들지 않는다.

## 10. 상태 전이 또는 주요 lifecycle

초기 준비 확인 → Service가 해당 요청·출력 시도에 귀속된 결과를 반영하고 기한 처리를 진행 → 반영 완료 관측 → POLICY 판단 → PLAYBACK 요구 → 변경 관측 재확인 → HEALTH 보고 연결의 흐름이다.

RX가 없어도 품질·age/Hold, 출력 결과·refill·정리, cue/기한, 재평가/보고가 bounded하게 진행되어야 한다. start/actual 반영과 STORE 만료 경합은 최신 PLAYBACK 사실을 먼저 반영·재확인해 미확정 시도를 Expired로 단정하지 않는다.

종료 중 새 winner가 생기면 재평가 필요성만 남기고, old retirement 뒤 최신 후보를 다시 읽는다. 복구는 [DIAGNOSTIC STATUS의 순서](26_HEALTH_MODULE.md#recovery)를 연결한다. 상태 보고는 HEALTH → FLOW → INPUT의 통신 연결 책임(C02) → 기존 Node Communication으로 전달한다.

## 11. Module-local Contract

함께 판단 가능한 상태 묶음(coherent view)은 동일 CPU 순간의 복사가 아니라 함께 판단할 수 있는 적용·시간·인과 일관성이다. 수집 중 관련 상태가 바뀌면 다시 읽는다. 관측 읽기가 미반영 결과를 숨겨 적용하는 getter가 되어서는 안 된다.

## 12. 대표 오류 / 예외 처리 원칙

- 관측 중 관련 변화: 해당 읽기 묶음을 버리고 재평가한다.
- 결과의 요청·출력 시도 귀속이나 시간 근거 부족: 성공·미시작·종료를 추정하지 않고 해당 owner가 손실/정리를 진행하게 한다.
- startup 미평가: 확인 실패로 바꾸지 않는다.

## 13. 관련 Function 문서

[Flow_Process](../30_FUNCTIONS/10_FLOW_FUNCTIONS.md#flow-process) · [기존 후보의 R2 판정](../30_FUNCTIONS/00_FUNCTION_OVERVIEW.md#candidate-decisions) · [이전 명칭의 의미 연결](../30_FUNCTIONS/00_FUNCTION_OVERVIEW.md#merged).

## 14. 관련 Data 문서

[Selection 관측/판단 자료](../40_DATA/30_SELECTION_DATA.md#selection)

## 15. 관련 공통 Contract

[Ownership](../50_CONTRACTS/00_CONTRACT_OVERVIEW.md#ownership) · [Timing](../50_CONTRACTS/00_CONTRACT_OVERVIEW.md#timing) · [Fault / Recovery](../50_CONTRACTS/00_CONTRACT_OVERVIEW.md#fault)

공통 원문의 위치를 연결한다. 여러 Module에 동일한 규칙의 통합·중복 제거는 R4에서 수행한다.

## 16. TBD / Deferred

실제 호출 시점·주기·직렬화·처리 예산은 [실행 TBD](../90_BINDING/91_IMPLEMENTATION_TBD.md#execution), 초기 readiness는 [준비 TBD](../90_BINDING/91_IMPLEMENTATION_TBD.md#hardware)를 확인한다.

R2에서 실제 Function 분해·조건/prototype을 검토한다. 실제 실행 문맥과 성능 PASS는 이 문서에서 확정하지 않는다.
