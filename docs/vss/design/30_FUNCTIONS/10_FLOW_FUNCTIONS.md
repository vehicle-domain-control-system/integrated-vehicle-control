# FLOW Core Function — R2

> 2026-10-07 · Baseline / Execution Plan v1.1 · Function 상세 설계만 수행

[최종 Core 색인·후보 판정](00_FUNCTION_OVERVIEW.md) · [provisional type 기준](00_FUNCTION_OVERVIEW.md#provisional-types)

<a id="flow"></a>
## 그룹 경계

FLOW의 조율 상태만 이 문서가 소유한다. POLICY의 선택과 PLAYBACK의 Session 실행은 해당 Core의 owner에게 남긴다. 초기화/입력/보고별 작은 wrapper를 만들지 않고, 반영 완료 관측을 모아 연결하는 하나의 실행 진입 계약을 유지한다.

Core 상세: [Flow_Process](10_FLOW_FUNCTIONS.md#flow-process)

아래 `const`·포인터·반환 표기는 읽기/반환 방향을 보이는 **pseudo prototype**이다. 타입은 R3에서 상세화할 opaque provisional semantic type이며 C ABI·구조체/필드·enum·Header·실제 symbol은 확정하지 않는다. 후속 처리 동안 필요한 자료는 owner의 원래 문맥으로 보호하고, 임시 인자 포인터를 그대로 장기 보관한다는 뜻은 아니다.

<a id="flow-process"></a>
## Flow_Process

| 항목 | 상세 계약 |
| --- | --- |
| 역할 | 입력과 후속 처리 기회를 각 owner에게 연결하고, 반영 완료 관측으로 POLICY 판단·PLAYBACK 실행·HEALTH 보고 및 허용된 복구 순서를 조율한다. |
| Owner | [FLOW](../20_MODULES/10_FLOW_MODULE.md) Module. Caller의 요청은 이 owner의 원본 상태를 직접 쓰는 권한이 아니다. |
| Caller | 기존 애플리케이션 실행 진입점. 실제 실행 문맥·주기는 TBD이며 ISR에서 호출하는 의미 처리 진입점으로 정하지 않는다. |
| Callee | [Input_Process](20_INPUT_STORE_FUNCTIONS.md#input-process), [Store_AdvanceDeadlines](20_INPUT_STORE_FUNCTIONS.md#store-advancedeadlines), [Select_Choose](30_POLICY_PLAYBACK_FUNCTIONS.md#select-choose), [Playback_RequestTransition](30_POLICY_PLAYBACK_FUNCTIONS.md#playback-requesttransition), [Playback_Advance](30_POLICY_PLAYBACK_FUNCTIONS.md#playback-advance), [AudioStream_Advance](40_AUDIO_STREAM_ASSET_FUNCTIONS.md#audiostream-advance), [AudioStream_RequestControl](40_AUDIO_STREAM_ASSET_FUNCTIONS.md#audiostream-requestcontrol), [Health_Evaluate](60_HEALTH_RUNTIME_FUNCTIONS.md#health-evaluate), [Health_BuildStatus](60_HEALTH_RUNTIME_FUNCTIONS.md#health-buildstatus). ASSET 가용 관측·INPUT의 기존 보고 연결·Runtime 공통 도구도 사용한다. |
| Input | 처리 기회 VssProcessingOpportunity와 비교 가능한 시간 VssTimeEvidence. 도착 자료는 기존 INPUT 경계로, 하위 사실은 AUDIO STREAM의 반환 경계로 받는다. 알림 자체를 출력 사실로 해석하지 않는다. |
| Return | VssFlowProgress: 이번 조율의 진행·재평가 필요성·보호된 후속 처리와 보고 전달 상태. 재생 성공 판정이나 다른 owner 상태의 새 원본이 아니다. |
| Reads | 각 Module의 반영 완료 관측·진단/복구 결과, Runtime의 시간 연속성·손실, FLOW의 기존 조율 단계. |
| Writes | FLOW의 조율 단계·재평가 필요성·임시 읽기 묶음만 쓴다. 다른 원본의 변경은 아래 callee가 자신의 공개 계약에서 수행한다. |
| Precondition | 호출 중 같은 owner의 변경이 충돌하지 않게 조율할 수 있어야 한다. 직렬화 수단·처리 예산은 미정이다. 관측 일관성을 확보하지 못하는 호출은 판단 대신 재평가를 남긴다. |
| Postcondition | 각 owner가 실제 반영한 사실만 다음 판단/보고로 연결한다. 하위/PLAYBACK 사실을 재확인하기 전 미시작을 Expired로 단정하지 않는다. 종료 중 새 후보는 이전 출력 정리와 사용권 종료 뒤 재평가한다. |
| Side Effect | Service 진행·의미 요청과 기존 보고 전달을 연결한다. 복구는 HEALTH 허용 → PLAYBACK의 이전 출력 정리 → 관련 자원 안전 종료 → Backend 수행/별도 검증 → HEALTH 대상 해제 순서로 조율한다. |
| Failure behavior | 초기 미평가는 실패로 바꾸지 않는다. 수집 중 상태 변화나 시간/사실 손실이면 해당 판단 묶음을 버리고 재평가한다. 실패 결과는 원래 owner·대상으로 연결하고 성공을 합성하지 않는다. |
| Invariant | FLOW는 POLICY의 선택 책임이나 PLAYBACK의 Session 실행 책임을 소유하지 않는다. 입력이 없어도 기한·결과·PCM 보충·정리·보고의 필요한 처리 기회를 보장한다. |
| Forbidden behavior | DRIVER/HAL/RTD 직접 호출, PCM 주소/descriptor 처리, getter에서 미반영 결과를 몰래 적용, 현재 시각으로 하위 발생 시각 재생산, 무제한 대기·재시도 금지. |
| 동기 / 비동기 | 호출 자체는 제한된 동기 조율이다. 미완료 하위 동작은 반환 후 후속 처리로 이어지며 완료까지 호출을 붙잡지 않는다. 시간/알림의 물리 구현은 결정하지 않는다. |
| 관련 Data | [선택 관측·판단·전체 계획](../40_DATA/30_SELECTION_DATA.md#selection) · [Runtime 시간·알림·손실](../10_LAYERS/details/50_TIMEBASE_RUNTIME_BOUNDARY.md) · [파생 상태 보고](../40_DATA/70_DIAGNOSTIC_DATA.md#status) |
| 관련 Contract | [소유·수명](../50_CONTRACTS/00_CONTRACT_OVERVIEW.md#ownership) · [시간·신뢰성](../50_CONTRACTS/00_CONTRACT_OVERVIEW.md#timing) · [고장·복구](../50_CONTRACTS/00_CONTRACT_OVERVIEW.md#fault) — 기존 원문 연결이며 R4 통합은 수행하지 않는다. |

### Pseudo prototype — provisional

```c
VssFlowProgress Flow_Process(
    const VssProcessingOpportunity *opportunity,
    const VssTimeEvidence *time
);
```

**Core 경계 판정:** KEEP. 단일 외부 실행 진입점의 관측 일관성·처리 순서가 조율 상태 전이다. 개별 초기화/보고 wrapper로 나누어도 독립 owner/실패 계약이 생기지 않는다. 다른 Module의 처리는 별도 Core로 내려가므로 큰 도메인 Process가 아니다.

## 계약 구분 검토 사례

실제 C 테스트 구현이 아니라, 함수 병합/내부화 시 보존해야 할 observable behavior를 확인하는 R2 설계 검수다.

| 상황 | 계약상 기대 결과 |
| --- | --- |
| RX 없이 결과/기한이 도착 | [Flow_Process](10_FLOW_FUNCTIONS.md#flow-process)가 각 owner에 처리 기회를 연결하고 반영 완료 관측 뒤 선택/보고를 연결한다. 다른 Module의 상태를 직접 쓰거나 알림만으로 성공을 만들지 않는다. |
| 종료 중 winner 변경 | 현재 종료 의도를 유지하고 이전 전체 출력 정리와 사용권 종료 뒤 최신 후보를 다시 판단하게 한다. 이전 읽기 묶음으로 새 start를 발행하지 않는다. |

## 후속 범위

[Data 의미](../40_DATA/00_DATA_OVERVIEW.md) · [공통 계약의 기존 위치](../50_CONTRACTS/00_CONTRACT_OVERVIEW.md) · [기존 의사코드](../60_PSEUDOCODE/00_PSEUDOCODE_OVERVIEW.md) · [Binding / 정책 TBD](../90_BINDING/91_IMPLEMENTATION_TBD.md)

R3에서 Data/Command/Result/Evidence의 표현·필드·수명 전달을 구체화한다. R4 Contract 통합, R5 Pseudocode 재작성, R6 Header ownership, R7 통합 검수와 B2-R Binding은 후속이다. 이번 문서는 실제 `.c/.h`/RTD·DMA·TCD/Task·Queue·Mutex/구현 순서와 실제 C 구현을 결정하지 않는다.
