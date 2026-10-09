# INPUT / STORE Core Functions — R2

> 2026-10-07 · Baseline / Execution Plan v1.1 · Function 상세 설계만 수행

[최종 Core 색인·후보 판정](00_FUNCTION_OVERVIEW.md) · [provisional type 기준](00_FUNCTION_OVERVIEW.md#provisional-types)

<a id="input-store"></a>
## 그룹 경계

INPUT은 출처·시간·순서의 적용을 소유하고 STORE는 발생 이력과 현재 후보를 소유한다. STORE의 새 입력 수용, 이미 확정된 재생 사실 반영, 현재 기한 진행은 호출자·실패/거부 의미가 달라 세 계약으로 분리한다. INPUT의 RX와 RX 없는 신뢰성 변화는 같은 적용 문맥의 내부 분기다.

Core 상세: [Input_Process](20_INPUT_STORE_FUNCTIONS.md#input-process) · [Store_ApplyInput](20_INPUT_STORE_FUNCTIONS.md#store-applyinput) · [Store_ApplyPlaybackFact](20_INPUT_STORE_FUNCTIONS.md#store-applyplaybackfact) · [Store_AdvanceDeadlines](20_INPUT_STORE_FUNCTIONS.md#store-advancedeadlines)

아래 `const`·포인터·반환 표기는 읽기/반환 방향을 보이는 **pseudo prototype**이다. 타입은 R3에서 상세화할 opaque provisional semantic type이며 C ABI·구조체/필드·enum·Header·실제 symbol은 확정하지 않는다. 후속 처리 동안 필요한 자료는 owner의 원래 문맥으로 보호하고, 임시 인자 포인터를 그대로 장기 보관한다는 뜻은 아니다.

<a id="input-process"></a>
## Input_Process

| 항목 | 상세 계약 |
| --- | --- |
| 역할 | 제품/TEST 자료와 시간·출처 연속성의 변화를 같은 적용 문맥으로 검증하고, 적용 가능한 사건·상태·품질 근거만 STORE에 전달한다. |
| Owner | [INPUT](../20_MODULES/20_INPUT_MODULE.md) Module. Caller의 요청은 이 owner의 원본 상태를 직접 쓰는 권한이 아니다. |
| Caller | FLOW. 기존 Node Communication/격리 TEST 자료는 FLOW가 이 의미 경계로 연결한다. |
| Callee | [Store_ApplyInput](20_INPUT_STORE_FUNCTIONS.md#store-applyinput) 및 필요한 기존 Runtime 도구. 기존 Network 보고 연결은 HEALTH가 만든 보고를 전달하는 내부 연결이며 별도 Core publisher를 추가하지 않는다. |
| Input | VssInputEvidence의 값+Meta/출처 근거와 VssTimeEvidence. RX가 없는 호출도 허용하고, 그때 INPUT이 자신의 기존 문맥에서 신뢰성 변화를 판단한다. FLOW가 INVALID/CLEAR를 만들어 전달하는 계약이 아니다. |
| Return | VssInputApplication: 검증/거부·적용 문맥/품질 변화와 STORE의 개별 수용 결과를 구별해 반환한다. valid만으로 accepted를 반환하지 않는다. |
| Reads | INPUT의 검증된 출처·세대·순서·원본 시간/연속성 및 제품/TEST 변환 기준. |
| Writes | INPUT의 적용 문맥만 쓴다. 수용 이력·현재 후보는 [Store_ApplyInput](20_INPUT_STORE_FUNCTIONS.md#store-applyinput)이 STORE owner로 변경한다. |
| Precondition | 자료 참조가 검증/전달 동안 유효해야 한다. 시간 비교 가능성을 확인하며 부족하면 부족한 근거로 처리한다. RX 없음·최신 품질 상실·과거 입력은 정상 처리 대상이다. |
| Postcondition | 적용 가능한 출처/순서는 STORE 호출 전에 반영한다. STORE 수용 거부에도 이를 되돌리지 않는다. 과거 INVALID는 현재 품질을 덮지 않고 최신 품질 상실은 정상 사건/CLEAR와 구별해 전달한다. |
| Side Effect | 검증된 INPUT 문맥 변경, 필요한 STORE 적용과 그 결과의 전달. 보고 연결을 이용하더라도 보고값은 계산하지 않는다. |
| Failure behavior | 부적합한 정상 사건은 거부하되 적용 가능한 품질 상실은 보존한다. 시간/출처 손실을 새 수신으로 보정하지 않는다. STORE 자원 부족은 해당 수용 실패이며 입력 순서 rollback이나 기존 이력 삭제를 유발하지 않는다. |
| Invariant | received ≠ valid ≠ accepted. PRODUCT/TEST의 출처·시간·저장·실행·이력 격리와 WINDOW [구현 보류 — 설계 유지]의 TEST 경로를 유지한다. |
| Forbidden behavior | CAN/payload/Network TX lifecycle 재설계, STORE authoritative 상태 직접 쓰기, INVALID/STALE을 CLEAR로 변환, WINDOW 보류를 Fault/DEGRADED로 자동 변환 금지. |
| 동기 / 비동기 | 검증·적용은 동기 의미 경계다. Network 수신/송신의 비동기 수명은 기존 계약을 따른다. 새 수신과 RX 없는 신뢰성 변화는 같은 출처 적용·거부 계약의 내부 분기다. |
| 관련 Data | [입력 문맥·Meta](../40_DATA/10_INPUT_DATA.md#input) · [발생 이력·현재 후보](../40_DATA/20_STORE_DATA.md#store) · [Runtime 시간·알림·손실](../10_LAYERS/details/50_TIMEBASE_RUNTIME_BOUNDARY.md) |
| 관련 Contract | [소유·수명](../50_CONTRACTS/00_CONTRACT_OVERVIEW.md#ownership) · [시간·신뢰성](../50_CONTRACTS/00_CONTRACT_OVERVIEW.md#timing) — 기존 원문 연결이며 R4 통합은 수행하지 않는다. |

### Pseudo prototype — provisional

```c
VssInputApplication Input_Process(
    const VssInputEvidence *input,
    const VssTimeEvidence *time
);
```

**Core 경계 판정:** KEEP. 호출자·출처 적용 owner·검증 후 STORE 전달과 거부 의미가 같다. RX 유무로 분리하면 같은 세대/순서 검증을 복제하므로 별도 Core보다 내부 분기가 적절하다.

<a id="store-applyinput"></a>
## Store_ApplyInput

| 항목 | 상세 계약 |
| --- | --- |
| 역할 | 검증 입력의 One-shot 수용 또는 Stateful/품질/Rear 갱신을 STORE 원본에 적용하고 개별 수용 결과를 반환한다. |
| Owner | [STORE](../20_MODULES/21_STORE_MODULE.md) Module. Caller의 요청은 이 owner의 원본 상태를 직접 쓰는 권한이 아니다. |
| Caller | INPUT의 [Input_Process](20_INPUT_STORE_FUNCTIONS.md#input-process). |
| Callee | 필요한 기존 Runtime 도구. 선택/보고/PLAYBACK을 역호출하지 않는다. |
| Input | VssValidatedInput: INPUT이 검증·적용한 사건/상태/품질·출처 문맥과 보존된 원본 시간 근거. INPUT의 출처 판정을 다시 소유하지 않는다. |
| Return | VssStoreAdmission: accepted/동일 발생 재전달/신규 수용 거부와 갱신 결과. 기존 의미의 결과이며 enum 정의가 아니다. |
| Reads | STORE의 발생 이력·재실행·수용 자원, 현재 Stateful 판단/품질/Hold·Rear 결합 및 INPUT의 검증 문맥. |
| Writes | STORE의 해당 발생 수용 이력과 현재 경고 후보·품질/Hold/Rear만 쓴다. INPUT의 검증 문맥은 읽기 자료다. |
| Precondition | INPUT의 적용 가능한 검증 근거를 받아야 한다. 신규 One-shot accepted에는 최종 통지/재실행 방어까지 추적할 자원이 필요하다. duplicate·유효 CLEAR·품질 갱신을 신규 수용과 구별한다. |
| Postcondition | 신규 accepted는 Pending과 identity/원본 age/추적 확보가 함께 성립한 뒤 노출한다. 공간 부족이면 기존 accepted/final 이력을 보존한다. Stateful은 최신 ACTIVE/CLEAR와 품질을 구별하며 최초 Hold 기산을 연장하지 않는다. Hold 기산은 최초 신뢰성 상실과 원본 유효기한 중 더 이른 기준을 따르고 반복 품질 상실로 연장하지 않는다. |
| Side Effect | 발생 추적 자원 확보 또는 현재 후보 변경. Session을 생성하거나 출력 요청을 발행하지 않는다. |
| Failure behavior | 동일 발생은 기존 결과를 반환한다. 신규 자원 부족은 신규 수용만 거부한다. 현재 품질 상실은 마지막 유효 판단과 분리하며 기존 유효 경고 없이 INVALID만으로 경고를 만들지 않는다. |
| Invariant | One-shot 이력과 Stateful 현재 후보는 별개다. Rear DISABLED는 후보/Hold 제거이며 CLEAR와 다르고, 재활성화로 옛 위험을 부활시키지 않는다. |
| Forbidden behavior | 기존 이력 임의 eviction, 재전달에 새 age/Pending 생성, 같은 ACTIVE에 Session 재시작, 입력 검증 순서 rollback, 출력/Fault 원본 쓰기 금지. |
| 동기 / 비동기 | 동기 적용·수용 경계. accepted는 실제 출력 시작이나 비동기 재생 완료의 증명이 아니다. |
| 관련 Data | [입력 문맥·Meta](../40_DATA/10_INPUT_DATA.md#input) · [발생 이력·현재 후보](../40_DATA/20_STORE_DATA.md#store) |
| 관련 Contract | [소유·수명](../50_CONTRACTS/00_CONTRACT_OVERVIEW.md#ownership) · [시간·신뢰성](../50_CONTRACTS/00_CONTRACT_OVERVIEW.md#timing) — 기존 원문 연결이며 R4 통합은 수행하지 않는다. |

### Pseudo prototype — provisional

```c
VssStoreAdmission Store_ApplyInput(
    const VssValidatedInput *input
);
```

**Core 경계 판정:** Store_Process의 SPLIT 대상. INPUT 호출의 자원 확보/거부 계약은 PLAYBACK 확정 사실 반영과 다르다. 병합하면 신규 수용 거부를 추적 중 사실 반영의 거부와 혼동할 위험이 있다.

<a id="store-applyplaybackfact"></a>
## Store_ApplyPlaybackFact

| 항목 | 상세 계약 |
| --- | --- |
| 역할 | PLAYBACK이 확정한 최초 시작·전체 완료/중단·미시작 재평가·불확실 최종을 원래 발생 이력에 적용한다. |
| Owner | [STORE](../20_MODULES/21_STORE_MODULE.md) Module. Caller의 요청은 이 owner의 원본 상태를 직접 쓰는 권한이 아니다. |
| Caller | PLAYBACK의 [Playback_Advance](30_POLICY_PLAYBACK_FUNCTIONS.md#playback-advance). 결과 통지가 보호된 상태로 남았을 때의 재적용도 같은 경계다. |
| Callee | 필요한 기존 Runtime 도구만 사용한다. Backend/HAL에서 직접 재생 결과를 받는 경계를 추가하지 않는다. |
| Input | VssPlaybackFact: PLAYBACK이 Session/Attempt 범위에서 판정한 귀속·사실 시각·기한/권한·최종 여부의 의미 근거. 해석 전 IRQ나 cue 소비 사실은 이 입력을 대신하지 못한다. One-shot 발생 이력에 연결할 사실이며 Stateful의 현재 ACTIVE/CLEAR·품질은 이 입력으로 재작성하지 않는다. |
| Return | VssFactApplication: 적용 완료/동일 사실 이미 반영/근거 부족을 구별한다. 반영 완료까지 PLAYBACK의 통지 의무를 끝냈다고 간주하지 않는다. |
| Reads | STORE의 해당 occurrence·최초 시작/최종/재실행 이력 및 PLAYBACK의 원래 발생·출력 시도에 귀속된 확정 사실. |
| Writes | STORE의 해당 발생 재생 이력·재실행 방어만 쓴다. PLAYBACK Session·AUDIO STREAM PCM·Fault를 변경하지 않는다. |
| Precondition | 기존 수용 발생과 보호된 추적 문맥에 연결할 수 있어야 한다. 늦음/중복/최종 뒤 사실은 격리 처리 대상이며 현재 발생으로 재귀속하지 않는다. |
| Postcondition | 적법한 최초 실제 시작만 Started로 한 번 반영한다. 한 cue 끝은 전체 Completed가 아니다. 불확실 최종은 재실행를 금지하고 이후 정상 이력을 부활시키지 않는다. 재적용은 이력을 중복 변경하지 않는다. |
| Side Effect | 해당 occurrence 이력과 결과 반영 의무의 완료 근거를 갱신한다. 신규 occurrence나 새 Session을 만들지 않는다. |
| Failure behavior | 귀속/추적 근거가 부족하면 적용 완료를 반환하지 않고 원래 사실·통지 의무의 보호/진단을 유지한다. 이전 최종과 충돌한 late 사실로 현재 이력을 덮지 않는다. 자원 부족을 이유로 추적 중 사실을 임의 삭제하지 않는다. |
| Invariant | 최초 실제 시작·전체 최종의 authoritative 기록은 STORE이고 판정 주체는 PLAYBACK이다. 시작 여부가 불확실한 최종과 물리 출력 정리는 별개다. |
| Forbidden behavior | 원본 구간 소비 사실을 Started/Completed로 적용, 신규 입력으로 취급해 자원 재수용/age 초기화, 최종 이력 부활, 타 Module 원본 직접 변경 금지. |
| 동기 / 비동기 | 동기·중복에 안전한 적용 경계. 입력 사실은 비동기 출력에서 나올 수 있지만 호출은 완료/보호된 재적용 의무를 명시해 반환한다. |
| 관련 Data | [발생 이력·현재 후보](../40_DATA/20_STORE_DATA.md#store) · [Session·Attempt 문맥](../40_DATA/40_PLAYBACK_DATA.md#playback) |
| 관련 Contract | [소유·수명](../50_CONTRACTS/00_CONTRACT_OVERVIEW.md#ownership) · [비동기 결과·근거](../50_CONTRACTS/00_CONTRACT_OVERVIEW.md#async) · [시간·신뢰성](../50_CONTRACTS/00_CONTRACT_OVERVIEW.md#timing) — 기존 원문 연결이며 R4 통합은 수행하지 않는다. |

### Pseudo prototype — provisional

```c
VssFactApplication Store_ApplyPlaybackFact(
    const VssPlaybackFact *fact
);
```

**Core 경계 판정:** Store_Process의 SPLIT 대상. 호출자·확정 사실의 적용 의무·중복/늦은 결과/최종 격리가 입력 수용이나 기한 진행과 독립된 관측 행동이다.

<a id="store-advancedeadlines"></a>
## Store_AdvanceDeadlines

| 항목 | 상세 계약 |
| --- | --- |
| 역할 | RX 없이 원본 age·품질/Hold 기한을 진행하고 최신 PLAYBACK 사실과 함께 만료 가능한 후보만 갱신한다. |
| Owner | [STORE](../20_MODULES/21_STORE_MODULE.md) Module. Caller의 요청은 이 owner의 원본 상태를 직접 쓰는 권한이 아니다. |
| Caller | FLOW의 [Flow_Process](10_FLOW_FUNCTIONS.md#flow-process). |
| Callee | 필요한 기존 Runtime 도구. PLAYBACK 관측은 FLOW가 연결하고 STORE가 PLAYBACK을 역호출하지 않는다. |
| Input | VssTimeEvidence와 VssPlaybackObservations: 비교 가능한 시간/연속성 및 이미 반영된 최신 출력 시도·시작/미시작/불확실·통지 진행의 읽기 근거. |
| Return | VssStoreProgress: 후보/품질·기한 변화와 재확인 필요성. 관측은 STORE의 두 번째 원본이 아니다. |
| Reads | STORE의 원본 age·Hold 기산·품질/후보와 FLOW가 연결한 PLAYBACK 최신 사실. |
| Writes | STORE의 해당 기한/후보와 확정 가능한 Expired 이력만 쓴다. 새로운 발생·새 입력 시각은 만들지 않는다. |
| Precondition | 원본 시간과 비교 가능한 근거가 있어야 한다. 시작/만료 경합에서 PLAYBACK 사실이 미반영/변화 중이면 먼저 반영·재확인해야 하며, 부족한 호출은 만료 확정을 보류한다. |
| Postcondition | Hold 만료는 후보 제거이며 품질 정상화가 아니다. 확정 미시작·시작 여부가 불확실한 최종 없음·원본 기한 도달의 One-shot만 Expired로 확정한다. in-flight/불명확 시도와 Started Session은 age만으로 만료시키지 않는다. |
| Side Effect | STORE 기한·현재 후보를 변경하고 재평가 필요성을 반환한다. 입력/재생 사실을 대신 생성하지 않는다. |
| Failure behavior | 시간 비교/연속성이나 최신 사실이 부족하면 해당 불확실성을 보존한다. 기한을 새 현재 시각으로 재기산하거나 정상 만료를 추정하지 않는다. |
| Invariant | 원본 age와 Hold는 재전달·선점·복구·새 Attempt로 초기화하지 않는다. 기존 [잠정] 새 실제 시작 age < 2초와 정확히 2초 불가, USE_LIMIT 별도 조건을 유지한다. |
| Forbidden behavior | timeout만으로 no-start 확정, INPUT의 source authority 재생산, Started 이후 정상 정책을 2초 경과만으로 중단, 참조/재실행 근거 없는 이력 회수 금지. |
| 동기 / 비동기 | 동기 기한 적용 경계. 비동기 시작 사실과의 경합은 최신 관측 재확인으로 처리하며 실행 보호 수단은 정하지 않는다. |
| 관련 Data | [발생 이력·현재 후보](../40_DATA/20_STORE_DATA.md#store) · [Session·Attempt 문맥](../40_DATA/40_PLAYBACK_DATA.md#playback) · [Runtime 시간·알림·손실](../10_LAYERS/details/50_TIMEBASE_RUNTIME_BOUNDARY.md) |
| 관련 Contract | [시간·신뢰성](../50_CONTRACTS/00_CONTRACT_OVERVIEW.md#timing) · [소유·수명](../50_CONTRACTS/00_CONTRACT_OVERVIEW.md#ownership) · [비동기 결과·근거](../50_CONTRACTS/00_CONTRACT_OVERVIEW.md#async) — 기존 원문 연결이며 R4 통합은 수행하지 않는다. |

### Pseudo prototype — provisional

```c
VssStoreProgress Store_AdvanceDeadlines(
    const VssTimeEvidence *time,
    const VssPlaybackObservations *playback
);
```

**Core 경계 판정:** Store_Process의 SPLIT 대상. FLOW의 기한 호출은 새 사건 수용/확정 사실 반영이 아니라 시간+최신 사실에 따른 조건부 만료다. 합치면 age 도달과 실제 미시작 확정을 혼동하기 쉽다.

## 계약 구분 검토 사례

실제 C 테스트 구현이 아니라, 함수 병합/내부화 시 보존해야 할 observable behavior를 확인하는 R2 설계 검수다.

| 상황 | 계약상 기대 결과 |
| --- | --- |
| 새 One-shot 수용 공간 부족 | [Store_ApplyInput](20_INPUT_STORE_FUNCTIONS.md#store-applyinput)은 신규 accepted만 거부한다. INPUT 적용 문맥과 기존 accepted/재실행 이력은 유지된다. |
| 이미 추적 중인 occurrence의 actual/최종 통지 | [Store_ApplyPlaybackFact](20_INPUT_STORE_FUNCTIONS.md#store-applyplaybackfact)는 보호된 사실을 원래 이력에 멱등 적용한다. 새 수용 공간 부족을 이유로 통지를 조용히 버리지 않는다. |
| first-start 기한과 비동기 actual 경합 | [Store_AdvanceDeadlines](20_INPUT_STORE_FUNCTIONS.md#store-advancedeadlines)는 최신 PLAYBACK 사실을 먼저 반영·재확인한다. 미확정 Attempt를 Expired로 확정하지 않는다. 확정 시각이 유효한 늦게 도착한 실제 출력 사실은 현재 적용 시각으로 기한을 바꾸지 않는다. |
| 같은 ACTIVE·중복 INVALID·Rear DISABLED | [Input_Process](20_INPUT_STORE_FUNCTIONS.md#input-process)/[Store_ApplyInput](20_INPUT_STORE_FUNCTIONS.md#store-applyinput)은 원래 출처/품질 기준을 보존한다. Hold를 연장하거나 INVALID/DISABLED를 CLEAR로 바꾸지 않는다. |

## 후속 범위

[Data 의미](../40_DATA/00_DATA_OVERVIEW.md) · [공통 계약의 기존 위치](../50_CONTRACTS/00_CONTRACT_OVERVIEW.md) · [기존 의사코드](../60_PSEUDOCODE/00_PSEUDOCODE_OVERVIEW.md) · [Binding / 정책 TBD](../90_BINDING/91_IMPLEMENTATION_TBD.md)

R3에서 Data/Command/Result/Evidence의 표현·필드·수명 전달을 구체화한다. R4 Contract 통합, R5 Pseudocode 재작성, R6 Header ownership, R7 통합 검수와 B2-R Binding은 후속이다. 이번 문서는 실제 `.c/.h`/RTD·DMA·TCD/Task·Queue·Mutex/구현 순서와 실제 C 구현을 결정하지 않는다.
