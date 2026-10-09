# HEALTH Core Functions / Runtime 연결 — R2

> 2026-10-07 · Baseline / Execution Plan v1.1 · Function 상세 설계만 수행

[최종 Core 색인·후보 판정](00_FUNCTION_OVERVIEW.md) · [provisional type 기준](00_FUNCTION_OVERVIEW.md#provisional-types)

<a id="health"></a>
## 그룹 경계

HEALTH의 진단 책임(C11)은 현재/최근 Fault와 복구 판단의 writer이고 보고 책임(C12)은 반영 완료 관측의 파생 보고만 쓴다. Runtime은 시간·연속성/손실·알림의 Layer Boundary이다. 작은 Runtime primitive는 신규 Core로 승격하지 않는다.

Core 상세: [Health_Evaluate](60_HEALTH_RUNTIME_FUNCTIONS.md#health-evaluate) · [Health_BuildStatus](60_HEALTH_RUNTIME_FUNCTIONS.md#health-buildstatus)

아래 `const`·포인터·반환 표기는 읽기/반환 방향을 보이는 **pseudo prototype**이다. 타입은 R3에서 상세화할 opaque provisional semantic type이며 C ABI·구조체/필드·enum·Header·실제 symbol은 확정하지 않는다. 후속 처리 동안 필요한 자료는 owner의 원래 문맥으로 보호하고, 임시 인자 포인터를 그대로 장기 보관한다는 뜻은 아니다.

<a id="health-evaluate"></a>
## Health_Evaluate

| 항목 | 상세 계약 |
| --- | --- |
| 역할 | 반영된 진단/현재 관측에서 현재·최근 Fault와 대상별 출력 제한·복구 허용/해제를 판단한다. |
| Owner | [HEALTH](../20_MODULES/26_HEALTH_MODULE.md) Module. Caller의 요청은 이 owner의 원본 상태를 직접 쓰는 권한이 아니다. |
| Caller | FLOW의 [Flow_Process](10_FLOW_FUNCTIONS.md#flow-process). Backend 진단·복구 수행/효과 검증과 각 owner 관측을 FLOW가 귀속을 유지한 채 연결한다. |
| Callee | 필요한 기존 Runtime 도구만 사용한다. Backend/HW 복구·Network 송신·STORE/PLAYBACK 변경 함수는 호출하지 않는다. |
| Input | VssDiagnosticObservations: 발생 주체·원인/단계·대상/구성·사실 시각·반영 완료 관측 및 허용된 복구 수행/별도 효과 검증 근거. 수락·무음·owner 해제는 정상 복귀 근거가 아니다. |
| Return | VssHealthAssessment: 현재/최근 Fault·현재 대상 제한과 복구 허용/해제 판단·진단 근거. 허용은 실제 복구 수행이 아니며 해제는 해당 현재 대상에만 적용된다. |
| Reads | HEALTH 진단 책임의 현재/최근 Fault·대상 제한/허용 문맥과 전달된 원래 진단/검증 근거·Runtime 시간 연속성. |
| Writes | HEALTH의 진단 책임이 소유하는 현재/최근 Fault·현재 대상의 허용/제한만 쓴다. 다른 원본과 파생 보고를 새로운 상태 원본으로 만들지 않는다. |
| Precondition | 관측의 원래 주체·대상/구성·시각과 반영 여부를 비교할 수 있거나 부족한 근거로 식별해야 한다. startup 미평가/입력 품질 상실/옛 복구 결과도 구별해 처리한다. |
| Postcondition | 기존 5종 Fault 의미를 유지하고 현재 대상·원인에 한정해 recoverable 판단/허용을 만든다. 대상 제한 해제는 그 대상에서 실제 수행 뒤 효과 검증이 확인된 경우만 가능하며 다른 현재 Fault·최근 기록은 보존된다. |
| Side Effect | 진단·제한·복구 허용/해제의 authoritative 상태 변경. 실제 복구는 FLOW가 조율해 Backend가 수행한다. |
| Failure behavior | 시간/대상/검증 근거 부족이면 관련 제한 해제를 보류한다. 옛 대상의 성공은 새 Fault/구성을 정상화하지 않는다. 새 retry/fallback matrix·exact cause 정책을 임의 확정하지 않고 기존 TBD를 유지한다. |
| Invariant | SOUND_ASSET_UNAVAILABLE / PLAYBACK_START_FAILURE / AUDIO_OUTPUT_FAILURE / PLAYBACK_STATE_FAILURE / INITIALIZATION_FAILURE의 기존 5종을 유지한다. 판단 → FLOW 조율 → Backend 수행 → 별도 검증 → 현재 대상 해제의 책임을 분리한다. |
| Forbidden behavior | 입력 INVALID/STALE·WINDOW 보류를 출력 Fault/DEGRADED로 자동 전환, 복구 receipt/무음만으로 정상화, HW 직접 reset, STORE 이력/age/재실행·PLAYBACK Session 초기화 금지. |
| 동기 / 비동기 | 동기 진단/허용/해제 적용이다. 비동기 수행 결과는 귀속된 관측 자료로 입력하며 실제 완료를 기다리거나 Backend를 직접 진행하지 않는다. |
| 관련 Data | [Fault·복구 문맥](../40_DATA/70_DIAGNOSTIC_DATA.md#health) · [파생 상태 보고](../40_DATA/70_DIAGNOSTIC_DATA.md#status) · [Runtime 시간·알림·손실](../10_LAYERS/details/50_TIMEBASE_RUNTIME_BOUNDARY.md) |
| 관련 Contract | [소유·수명](../50_CONTRACTS/00_CONTRACT_OVERVIEW.md#ownership) · [고장·복구](../50_CONTRACTS/00_CONTRACT_OVERVIEW.md#fault) — 기존 원문 연결이며 R4 통합은 수행하지 않는다. |

### Pseudo prototype — provisional

```c
VssHealthAssessment Health_Evaluate(
    const VssDiagnosticObservations *observations
);
```

**Core 경계 판정:** KEEP. 현재 Fault/제한의 실제 writer 경계다. 읽기 전용 보고와 합치면 상태 조회가 진단/해제를 암묵 수행하게 되므로 분리하며 Recovery 단계별 wrapper는 내부 판단 분기로 남긴다.

<a id="health-buildstatus"></a>
## Health_BuildStatus

| 항목 | 상세 계약 |
| --- | --- |
| 역할 | 각 owner에서 반영이 완료된 사실과 HEALTH 제한을 일관되게 읽어 외부 VSS_STATUS의 파생 보고를 만든다. |
| Owner | [HEALTH](../20_MODULES/26_HEALTH_MODULE.md) Module. Caller의 요청은 이 owner의 원본 상태를 직접 쓰는 권한이 아니다. |
| Caller | FLOW의 [Flow_Process](10_FLOW_FUNCTIONS.md#flow-process). 보고 전달은 HEALTH → FLOW → INPUT의 기존 Network 연결 → Node Communication 경로다. |
| Callee | 필요한 기존 Runtime 도구만 사용한다. 다른 owner의 미반영 결과를 적용하거나 Network 송신을 직접 호출하지 않는다. |
| Input | VssStatusObservations: 반영 완료된 STORE/PLAYBACK/Backend·입력 수용/시간/출처/자원 관측과 HEALTH 판단. 다른 시점의 사실을 섞어 정상으로 합성하지 않는다. |
| Return | VssStatusSnapshot: 해당 일관된 관측에서 파생한 서비스 수준·Availability·재생/초기 상태·accepting과 원래 진단 의미. 물리 field/bit layout이나 보고 enum을 정의하지 않는다. 관측 부족으로 보고를 생성할 수 없으면 새 정상값 대신 미생성/재평가 필요를 반환하는 의미이며 실제 표현은 R3 대상이다. |
| Reads | HEALTH의 반영된 현재 제한/최근 진단과 전달된 각 owner의 관측. PLAYBACK의 actual/uncertain·전체 Session 및 STORE 수용 조건을 읽는다. |
| Writes | HEALTH 보고 책임의 파생 보고 자료만 쓴다. Fault·발생 이력·Session·입력 문맥·장치 준비의 authoritative 상태는 변경하지 않는다. |
| Precondition | 관측 일관성과 필요한 반영이 확인되어야 한다. 확인되지 않은 경우 새 정상 보고를 만들지 않으며 재평가 필요를 보존한다. 보고 자료 참조/전달 기간은 기존 계약을 따른다. |
| Postcondition | PLAYING은 적법한 actual start가 확인된 전체 Session으로 파생하며 cue 무음/반복 대기/시작 후 종료 진행을 포함한다. QUARANTINED는 기존 FAULT/UNAVAILABLE 연결을 유지한다. accepting은 개별 accepted 보장이 아니다. |
| Side Effect | 파생 보고 자료 생성/보호. 원본 상태 변경·송신 실행·복구 허용/해제는 없다. |
| Failure behavior | startup 미평가를 INITIALIZATION_FAILURE로 합성하지 않는다. 관측 부족/혼합이면 새 정상값을 확정하지 않는다. 전체 보고 조합/reduction·초기 Availability의 기존 미정 정책은 확정하지 않는다. |
| Invariant | 보고는 원본의 두 번째 writer가 아니다. Audio Fault와 입력 수용 가능성은 별개이고 accepting=false여도 기존 duplicate/CLEAR·품질/기한의 안전 경로는 계속된다. WINDOW 보류만으로 Fault/DEGRADED를 만들지 않는다. |
| Forbidden behavior | getter에서 결과/기한/Fault를 몰래 적용, HEALTH 해제 수행, PCM/Session 수정, 준비·요청 수용을 PLAYING으로 변경, 송신 Module이 보고값을 재계산하도록 위임 금지. |
| 동기 / 비동기 | 동기 읽기 전용 파생 판단이다. 보고 자료 보호를 제외하면 authoritative 원본을 쓰지 않는다. 전송의 비동기 수명/실제 field 표현은 후속 단계다. |
| 관련 Data | [파생 상태 보고](../40_DATA/70_DIAGNOSTIC_DATA.md#status) · [Fault·복구 문맥](../40_DATA/70_DIAGNOSTIC_DATA.md#health) · [발생 이력·현재 후보](../40_DATA/20_STORE_DATA.md#store) · [Session·Attempt 문맥](../40_DATA/40_PLAYBACK_DATA.md#playback) · [입력 문맥·Meta](../40_DATA/10_INPUT_DATA.md#input) |
| 관련 Contract | [소유·수명](../50_CONTRACTS/00_CONTRACT_OVERVIEW.md#ownership) · [고장·복구](../50_CONTRACTS/00_CONTRACT_OVERVIEW.md#fault) · [시간·신뢰성](../50_CONTRACTS/00_CONTRACT_OVERVIEW.md#timing) — 기존 원문 연결이며 R4 통합은 수행하지 않는다. |

### Pseudo prototype — provisional

```c
VssStatusSnapshot Health_BuildStatus(
    const VssStatusObservations *observations
);
```

**Core 경계 판정:** KEEP. 실제 외부 보고 경계이며 진단 writer와 달리 원본 읽기 전용이다. Evaluate와 합치거나 단순 getter로 내부화하면 보고 호출의 쓰기 금지·일관성 계약이 흐려진다.

## 계약 구분 검토 사례

실제 C 테스트 구현이 아니라, 함수 병합/내부화 시 보존해야 할 observable behavior를 확인하는 R2 설계 검수다.

| 상황 | 계약상 기대 결과 |
| --- | --- |
| 보고 호출만 실행 | [Health_BuildStatus](60_HEALTH_RUNTIME_FUNCTIONS.md#health-buildstatus)는 원본 Fault/Session/발생 이력을 쓰지 않는다. 판단·해제가 필요하면 [Health_Evaluate](60_HEALTH_RUNTIME_FUNCTIONS.md#health-evaluate) 계약으로 처리한다. |
| 옛 복구 성공과 새 현재 Fault | [Health_Evaluate](60_HEALTH_RUNTIME_FUNCTIONS.md#health-evaluate)는 해당 현재 대상의 수행/별도 검증 근거를 확인한다. 옛 대상 성공으로 새 제한이나 다른 Fault/최근 기록을 해제하지 않는다. |
| startup 미평가·WINDOW 보류·입력 품질 상실 | 확인된 초기화/출력 실패와 구별한다. 자동 Fault/DEGRADED 또는 PLAYING을 합성하지 않는다. |

## 후속 범위

[Data 의미](../40_DATA/00_DATA_OVERVIEW.md) · [공통 계약의 기존 위치](../50_CONTRACTS/00_CONTRACT_OVERVIEW.md) · [기존 의사코드](../60_PSEUDOCODE/00_PSEUDOCODE_OVERVIEW.md) · [Binding / 정책 TBD](../90_BINDING/91_IMPLEMENTATION_TBD.md)

R3에서 Data/Command/Result/Evidence의 표현·필드·수명 전달을 구체화한다. R4 Contract 통합, R5 Pseudocode 재작성, R6 Header ownership, R7 통합 검수와 B2-R Binding은 후속이다. 이번 문서는 실제 `.c/.h`/RTD·DMA·TCD/Task·Queue·Mutex/구현 순서와 실제 C 구현을 결정하지 않는다.
