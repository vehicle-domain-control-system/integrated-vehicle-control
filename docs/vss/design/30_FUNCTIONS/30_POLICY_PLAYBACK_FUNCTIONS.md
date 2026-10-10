# POLICY / PLAYBACK Core Functions — R2

> 2026-10-07 · Baseline / Execution Plan v1.1 · Function 상세 설계만 수행

[최종 Core 색인·후보 판정](00_FUNCTION_OVERVIEW.md) · [provisional type 기준](00_FUNCTION_OVERVIEW.md#provisional-types)

<a id="selection-playback"></a>
## 그룹 경계

POLICY는 읽기 전용 선택 판단, PLAYBACK은 단일 전체 Session의 실행 owner다. PLAYBACK의 새 의도 수용과 진행 중 사실 적용을 분리하며 Start/Stop/Actual/Timeout/Retire마다 별도 함수를 만들지 않는다. Session/Attempt의 실제 표현·cue당 Attempt 수는 R3 이후에 남긴다.

Core 상세: [Select_Choose](30_POLICY_PLAYBACK_FUNCTIONS.md#select-choose) · [Playback_RequestTransition](30_POLICY_PLAYBACK_FUNCTIONS.md#playback-requesttransition) · [Playback_Advance](30_POLICY_PLAYBACK_FUNCTIONS.md#playback-advance)

아래 `const`·포인터·반환 표기는 읽기/반환 방향을 보이는 **pseudo prototype**이다. 타입은 R3에서 상세화할 opaque provisional semantic type이며 C ABI·구조체/필드·enum·Header·실제 symbol은 확정하지 않는다. 후속 처리 동안 필요한 자료는 owner의 원래 문맥으로 보호하고, 임시 인자 포인터를 그대로 장기 보관한다는 뜻은 아니다.

<a id="select-choose"></a>
## Select_Choose

| 항목 | 상세 계약 |
| --- | --- |
| 역할 | 함께 판단 가능한 후보·현재 출력 owner·제한에서 중앙 정책의 실행 판단과 의미 기반 전체 계획을 반환한다. |
| Owner | [POLICY](../20_MODULES/22_POLICY_MODULE.md) Module. Caller의 요청은 이 owner의 원본 상태를 직접 쓰는 권한이 아니다. |
| Caller | FLOW의 [Flow_Process](10_FLOW_FUNCTIONS.md#flow-process). |
| Callee | 필요한 기존 Runtime 도구만 사용한다. PLAYBACK 실행 함수는 호출하지 않는다. |
| Input | VssSelectionView: 적용·시간·인과 관계가 일관된 후보/기한·현재 출력·Backend/HEALTH 제한 관측. 같은 CPU 순간의 복사라는 뜻은 아니다. |
| Return | VssSelectionDecision: 기존 winner/keep/replace/wait와 후보·Asset ID·전체 계획의 연결. 판단 반환은 시작 허가가 아니다. |
| Reads | POLICY의 읽기 전용 중앙 정책과 FLOW가 연결한 STORE/PLAYBACK/Backend/HEALTH 관측. |
| Writes | 한 평가의 판단 자료만 만든다. STORE·PLAYBACK·Fault 원본과 실행 중 중앙 정책은 변경하지 않는다. |
| Precondition | 함께 적용 가능한 최신 관측이어야 한다. 수집 중 관련 변화/근거 부족이면 판단을 적용 가능한 것으로 내보내지 않고 재수집 필요성을 반환한다. |
| Postcondition | 기존 Class/잠정 세부 순위 및 동일 의미 미시작 One-shot의 원본 발생 순서·고정 identity 비교를 적용한다. 높은 후보라도 이전 출력/기한/제한으로 실행 대기가 가능함을 보존한다. |
| Side Effect | 의미 판단 자료만 반환한다. 장치 요청·Session 변경·음원 fallback은 없다. |
| Failure behavior | 음원/패턴 또는 관측 근거 부족을 다른 의미의 소리로 대체하지 않는다. 낮은 후보나 새 후보 없음만으로 유효한 현재 One-shot 종료를 결정하지 않는다. |
| Invariant | 선택은 POLICY, 채택/실행은 PLAYBACK 책임이다. 선택된 후보 ≠ 준비 완료 ≠ 시작 요청 수락 ≠ 실제 출력 시작. |
| Forbidden behavior | 수신/queue 순서로 발생 우선순위 변경, 같은 의미의 늦은 옛 발생으로 Started 현재 Session 선점, PCM 주소/디코더/DMA 설정을 계획에 포함, 실행 Module 직접 호출 금지. |
| 동기 / 비동기 | 동기 읽기 전용 판단. 출력 결과를 기다리는 비동기 실행은 수행하지 않는다. |
| 관련 Data | [선택 관측·판단·전체 계획](../40_DATA/30_SELECTION_DATA.md#selection) · [Session·Attempt 문맥](../40_DATA/40_PLAYBACK_DATA.md#playback) |
| 관련 Contract | [소유·수명](../50_CONTRACTS/00_CONTRACT_OVERVIEW.md#ownership) · [시간·신뢰성](../50_CONTRACTS/00_CONTRACT_OVERVIEW.md#timing) — 기존 원문 연결이며 R4 통합은 수행하지 않는다. |

### Pseudo prototype — provisional

```c
VssSelectionDecision Select_Choose(
    const VssSelectionView *view
);
```

**Core 경계 판정:** KEEP. POLICY→FLOW의 읽기 전용 판단 경계이며 실행과 owner가 다르다. 기준 조회/helper를 추가하거나 PLAYBACK과 합치면 선택/실행 책임이 흐려진다.

<a id="playback-requesttransition"></a>
## Playback_RequestTransition

| 항목 | 상세 계약 |
| --- | --- |
| 역할 | FLOW가 전달한 선택/유지/교체 또는 종료 의도를 최신 조건으로 검토하여 Session 채택·유지·후속 권한 차단을 PLAYBACK 상태에 반영한다. |
| Owner | [PLAYBACK](../20_MODULES/23_PLAYBACK_MODULE.md) Module. Caller의 요청은 이 owner의 원본 상태를 직접 쓰는 권한이 아니다. |
| Caller | FLOW의 [Flow_Process](10_FLOW_FUNCTIONS.md#flow-process). |
| Callee | [AudioStream_Prepare](40_AUDIO_STREAM_ASSET_FUNCTIONS.md#audiostream-prepare) 또는 [AudioStream_RequestControl](40_AUDIO_STREAM_ASSET_FUNCTIONS.md#audiostream-requestcontrol). 같은 PLAYBACK의 공통 전이 처리는 내부에서 공유하며 별도 Start/Stop wrapper를 만들지 않는다. |
| Input | VssPlaybackIntent: POLICY 판단에 연결된 실행 의도 또는 현재 출력의 종료 필요성. VssExecutionConditions: 최신 후보/CLEAR·원본 기한·owner·Fault/Backend 제한·보호 자원 근거. raw Backend 결과는 이 요청 입력과 섞지 않는다. |
| Return | VssRequestDisposition: 의도 반영/기존 진행 유지/대기/거부와 재평가 필요성. 요청 수용이 실제 출력 수락·시작·종료를 증명하지 않는다. |
| Reads | PLAYBACK의 현재 Session/Attempt·전체 계획·종료 의도·권한 및 전달된 최신 조건. |
| Writes | PLAYBACK의 채택/종료 의도·Session/Attempt 귀속·후속 start/cue/반복 권한만 쓴다. AUDIO STREAM 변경은 callee owner가 수행한다. |
| Precondition | 의도와 원래 후보/현재 Session의 연결을 검증한다. 새 Session은 이전 전체 출력 정리와 사용권 종료·자원 보호·현재 후보/기한/제한이 충분해야 한다. 충족되지 않는 요청은 대기/거부 경로다. |
| Postcondition | 새 채택은 불변 Session/Attempt 문맥을 먼저 확보하고 PREPARING 의미로 준비 요청을 연결한다. 종료/선점은 후속 권한 차단을 먼저 반영한 뒤 Audio 정리를 요청한다. 정리 중 새 winner는 종료 의도를 취소하지 않는다. |
| Side Effect | 의미 준비/정리 요청 발행과 PLAYBACK 요청 상태 변경. 하위의 즉시 반환 결과는 원래 문맥으로 보호해 [Playback_Advance](30_POLICY_PLAYBACK_FUNCTIONS.md#playback-advance)의 결과 적용 계약으로 연결한다. |
| Failure behavior | 새 요청의 근거/보호 자원 부족은 기존 출력 사실을 덮지 않는 대기/거부다. 하위 부분 효력 또는 불명확한 시작은 정상 rollback으로 취급하지 않고 권한 차단·정리/결과 추적을 유지한다. |
| Invariant | 전체 계획은 한 Session이며 Occurrence와 Attempt를 같게 만들지 않는다. prepare 수용/완료는 실제 새 start 권한이 아니다. 다른 Session을 이전 출력 정리와 사용권 종료 전 prepare/start하지 않는다. |
| Forbidden behavior | STORE 발생 이력 직접 쓰기, PCM 직접 반환/수정, 기한/identity 재기산, Started One-shot 자동 resume, 종료 요청 수락만으로 owner 해제, 보류 중 오래된 winner 자동 실행 금지. |
| 동기 / 비동기 | 호출은 동기 의도 반영이고 하위 준비/정리 완료는 비동기일 수 있다. 요청을 거부할 수 있는 계약과 이미 발생한 결과의 적용 의무를 분리한다. |
| 관련 Data | [선택 관측·판단·전체 계획](../40_DATA/30_SELECTION_DATA.md#selection) · [Session·Attempt 문맥](../40_DATA/40_PLAYBACK_DATA.md#playback) · [준비·음원 처리 문맥·Backend 결과](../40_DATA/50_AUDIO_STREAM_DATA.md#provider) |
| 관련 Contract | [소유·수명](../50_CONTRACTS/00_CONTRACT_OVERVIEW.md#ownership) · [비동기 결과·근거](../50_CONTRACTS/00_CONTRACT_OVERVIEW.md#async) · [시간·신뢰성](../50_CONTRACTS/00_CONTRACT_OVERVIEW.md#timing) — 기존 원문 연결이며 R4 통합은 수행하지 않는다. |

### Pseudo prototype — provisional

```c
VssRequestDisposition Playback_RequestTransition(
    const VssPlaybackIntent *intent,
    const VssExecutionConditions *conditions
);
```

**Core 경계 판정:** Playback_Process의 SPLIT 대상. 새 실행/종료 의도는 최신 조건으로 수용/거부할 수 있지만 이미 발생한 출력 사실은 별도 귀속·최종 격리가 필요하다. Start/Stop은 같은 권한 전이의 내부 분기로 묶는다.

<a id="playback-advance"></a>
## Playback_Advance

| 항목 | 상세 계약 |
| --- | --- |
| 역할 | 준비/출력의 귀속된 Backend 결과와 기한을 적용하고 정상 cue 진행·미시작/전체 종료·늦은 결과/시작 여부 불확실·출력 정리와 사용권 종료 및 STORE 통지를 진행한다. |
| Owner | [PLAYBACK](../20_MODULES/23_PLAYBACK_MODULE.md) Module. Caller의 요청은 이 owner의 원본 상태를 직접 쓰는 권한이 아니다. |
| Caller | FLOW의 [Flow_Process](10_FLOW_FUNCTIONS.md#flow-process) 및 PLAYBACK 내부에서 하위 즉시 결과를 같은 적용 계약으로 연결하는 경로. |
| Callee | [AudioStream_Prepare](40_AUDIO_STREAM_ASSET_FUNCTIONS.md#audiostream-prepare), [AudioStream_RequestControl](40_AUDIO_STREAM_ASSET_FUNCTIONS.md#audiostream-requestcontrol), [Store_ApplyPlaybackFact](20_INPUT_STORE_FUNCTIONS.md#store-applyplaybackfact). POLICY를 직접 호출해 새 Session을 선택하지 않는다. |
| Input | VssAudioBackendResult와 VssExecutionConditions, VssTimeEvidence. 결과 없는 기한/정책 진행도 허용한다. 준비·수락+장치 활성화·실제 출력·범위별 종료·손실 사실을 원래 요청·Attempt에 연결해 해석한다. |
| Return | VssPlaybackProgress: 현재 출력 관측·원래 발생의 통지 반영/보호 상태·재평가/정리 필요 및 진단 근거. 실제 Session 최종 판정은 이 owner에서만 만든다. |
| Reads | PLAYBACK의 Session/Attempt·종료 의도/권한·최종/통지 의무, 해당 Backend 결과·최신 조건·원본 시간 근거. |
| Writes | PLAYBACK의 진행·실제 시작 적용·후속 권한·시작 여부가 불확실한 최종/출력 정리와 사용권 종료·통지 의무만 쓴다. STORE의 이력은 [Store_ApplyPlaybackFact](20_INPUT_STORE_FUNCTIONS.md#store-applyplaybackfact)가 변경한다. |
| Precondition | 처리할 결과를 원래 문맥에 연결하거나 귀속 부족으로 구별할 수 있어야 한다. 늦은/중복/최종 뒤 결과도 버릴 전제조건이 아니라 격리해야 할 입력이다. 새 start 직전에는 최신 조건을 다시 확인한다. |
| Postcondition | 준비 완료 뒤 최신 CLEAR/owner/Fault·첫 시작 기한을 재검사하고 START_IN_FLIGHT에서 하위 start를 연결한다. 적법한 OUTPUT_START_CONFIRMED만 최초 Started로 통지한다. 정상 다음 cue는 해당 구간 안전 끝/현재 계획을 확인하며 새 occurrence로 만들지 않는다. 첫 실제 시작 전에는 원본 기한을 적용하지만 Started 정상 Session의 후속 cue/반복에 첫 시작 2초 조건을 다시 적용해 중단하지 않는다. |
| Side Effect | 하위 start/정리·정상 후속 준비를 발행하고 STORE에 확정 사실을 적용한다. 시작 여부가 불확실한 최종은 QUARANTINED와 전체 권한 차단·재실행 금지 및 기존 PLAYBACK_STATE_FAILURE/FAULT/UNAVAILABLE 근거를 남기며 이전 출력 정리는 계속한다. |
| Failure behavior | 최종 전 같은 Attempt의 적법한 늦게 도착한 실제 출력 사실은 Started/보고 사실만 적용하고 STOPPING/ABORTING을 유지한다. 시작 여부가 불확실한 최종 뒤/출력 정리와 사용권 종료 뒤의 결과는 옛 정리·진단만 한다. 늦은 수락으로 actual/종료를 되돌리지 않는다. 통지 미반영은 보호된 의무로 남긴다. 기한/권한 밖 실제 출력은 물리 사실과 위반 근거로 보존해 정리하며 적법한 최초 Started로 합성하지 않는다. |
| Invariant | NO_START_CONFIRMED는 충분한 Backend 무출력/미래 PCM 차단과 모든 상위 권한 차단이 필요하다. OUTPUT_TERMINATION_CONFIRMED도 전체 출력 종료·상위 차단·late 보호 뒤만 retire한다. 불확실 최종 기록은 출력 정리와 사용권 종료보다 먼저 가능하다. |
| Forbidden behavior | timeout/현재 idle/abort return만으로 성공·미시작·종료 확정, cue/EOS로 전체 Completed, 종료 중 ACTIVE/후속 cue 복원, 최종 이력 부활, owner 해제를 Fault/이력/callback 저장 해제와 동일시 금지. |
| 동기 / 비동기 | 적용 호출은 동기이며 입력은 비동기 결과일 수 있다. 결과/기한/정상 cue 진행은 같은 이미 채택한 lifecycle의 내부 분기다. 새 선택 채택은 RequestTransition에 둔다. |
| 관련 Data | [Session·Attempt 문맥](../40_DATA/40_PLAYBACK_DATA.md#playback) · [발생 이력·현재 후보](../40_DATA/20_STORE_DATA.md#store) · [준비·음원 처리 문맥·Backend 결과](../40_DATA/50_AUDIO_STREAM_DATA.md#provider) · [Runtime 시간·알림·손실](../10_LAYERS/details/50_TIMEBASE_RUNTIME_BOUNDARY.md) |
| 관련 Contract | [소유·수명](../50_CONTRACTS/00_CONTRACT_OVERVIEW.md#ownership) · [비동기 결과·근거](../50_CONTRACTS/00_CONTRACT_OVERVIEW.md#async) · [시간·신뢰성](../50_CONTRACTS/00_CONTRACT_OVERVIEW.md#timing) · [고장·복구](../50_CONTRACTS/00_CONTRACT_OVERVIEW.md#fault) — 기존 원문 연결이며 R4 통합은 수행하지 않는다. |

### Pseudo prototype — provisional

```c
VssPlaybackProgress Playback_Advance(
    const VssAudioBackendResult *result,
    const VssExecutionConditions *conditions,
    const VssTimeEvidence *time
);
```

**Core 경계 판정:** Playback_Process의 SPLIT 대상. 귀속된 사실의 적용·종료/최종 격리·통지 의무를 새 요청 거부와 분리한다. Prepared/Actual/Timeout/Retire별 wrapper는 같은 Session 전이를 쪼개므로 내부에 남긴다.

## 계약 구분 검토 사례

실제 C 테스트 구현이 아니라, 함수 병합/내부화 시 보존해야 할 observable behavior를 확인하는 R2 설계 검수다.

| 상황 | 계약상 기대 결과 |
| --- | --- |
| 새 요청 거부와 옛 결과 도착 | [Playback_RequestTransition](30_POLICY_PLAYBACK_FUNCTIONS.md#playback-requesttransition)은 새 채택을 대기/거부할 수 있다. [Playback_Advance](30_POLICY_PLAYBACK_FUNCTIONS.md#playback-advance)는 이미 진행한 원래 Attempt의 결과를 그 거부와 무관하게 적용/격리한다. |
| prepared 후 CLEAR 또는 first-start 기한 도달 | [Playback_Advance](30_POLICY_PLAYBACK_FUNCTIONS.md#playback-advance)는 새 start 직전 최신 조건을 재검사해 차단/정리한다. 준비 성공을 실제 start나 Started로 기록하지 않는다. |
| STOPPING/ABORTING 중 같은 Attempt의 적법한 늦게 도착한 실제 출력 사실 | 최종 전이면 Started/보고 사실만 반영하고 종료 상태·후속 권한 차단을 유지한다. 시작 여부가 불확실한 최종 뒤 또는 retired이면 옛 정리/진단만 한다. |
| cue EOS 또는 첫 cue 끝 | 현재 cue/구간 사실로만 적용한다. 전체 계획의 정상 끝과 종료 근거 전에는 전체 Completed/출력 정리와 사용권 종료로 바꾸지 않는다. |

## 후속 범위

[Data 의미](../40_DATA/00_DATA_OVERVIEW.md) · [공통 계약의 기존 위치](../50_CONTRACTS/00_CONTRACT_OVERVIEW.md) · [기존 의사코드](../60_PSEUDOCODE/00_PSEUDOCODE_OVERVIEW.md) · [Binding / 정책 TBD](../90_BINDING/91_IMPLEMENTATION_TBD.md)

R3에서 Data/Command/Result/Evidence의 표현·필드·수명 전달을 구체화한다. R4 Contract 통합, R5 Pseudocode 재작성, R6 Header ownership, R7 통합 검수와 B2-R Binding은 후속이다. 이번 문서는 실제 `.c/.h`/RTD·DMA·TCD/Task·Queue·Mutex/구현 순서와 실제 C 구현을 결정하지 않는다.
