# Function Overview — R2

> 2026-10-07 · 고정 Architecture Baseline / Execution Plan v1.1 · Function 실제 분해 및 상세 설계 완료

기존 **13개 경계/진입 후보와 내부 처리 1개**를 모두 재검토했다. 최종 Core는 **18개**다. KEEP 9개, SPLIT 4개, INTERNALIZE 1개이며 MERGE/REMOVE 0개다. 이 수는 입력/출력·호출자·상태전이·비동기 사실 적용·거부/실패의 실제 계약을 비교한 결과다. 목표 개수나 상태/결과별 wrapper 수를 맞춘 것이 아니다.

Layer 5개·Module 10개와 `10_LAYERS/details/`의 HAL/BSP·Runtime Boundary 구조/책임은 그대로다. Function owner는 기본적으로 Module이며 `AudioHAL_Callback`은 HAL/BSP Layer의 HAL Boundary가 owner다. 가짜 HAL/RUNTIME Module이나 새 Manager/Repository/Controller를 만들지 않는다.

Function명은 논리 설계명이다. 공개/내부 경계와 역할을 보이며 최종 C symbol·Header·실제 `.c/.h` 배치/개수를 정하지 않는다. 기존 [11개 의사코드 흐름](../60_PSEUDOCODE/reference/81_VSS_PSEUDOCODE.md)은 의미 참고본으로 그대로 두며 Function 수와 1:1로 대응하지 않는다.

<a id="core-index"></a>
## 최종 Core Function 색인

| 그룹 | Core 수 | 상세 문서 |
| --- | --- | --- |
| FLOW Core Function | 1 | [10_FLOW_FUNCTIONS.md](10_FLOW_FUNCTIONS.md#flow) |
| INPUT / STORE Core Functions | 4 | [20_INPUT_STORE_FUNCTIONS.md](20_INPUT_STORE_FUNCTIONS.md#input-store) |
| POLICY / PLAYBACK Core Functions | 3 | [30_POLICY_PLAYBACK_FUNCTIONS.md](30_POLICY_PLAYBACK_FUNCTIONS.md#selection-playback) |
| AUDIO STREAM / ASSET Core Functions | 4 | [40_AUDIO_STREAM_ASSET_FUNCTIONS.md](40_AUDIO_STREAM_ASSET_FUNCTIONS.md#audio-service) |
| DRIVER / HAL Core Functions | 4 | [50_DRIVER_HAL_FUNCTIONS.md](50_DRIVER_HAL_FUNCTIONS.md#driver-hal) |
| HEALTH Core Functions / Runtime 연결 | 2 | [60_HEALTH_RUNTIME_FUNCTIONS.md](60_HEALTH_RUNTIME_FUNCTIONS.md#health) |

| Core Function 상세 | Owner | 실제 경계 / 역할 | Provisional Input → Return | 동기·비동기 |
| --- | --- | --- | --- | --- |
| [Flow_Process](10_FLOW_FUNCTIONS.md#flow-process) | [FLOW](../20_MODULES/10_FLOW_MODULE.md) | 반영 완료 관측·후속 처리·선택/실행/보고·복구의 조율 | `VssProcessingOpportunity` + `VssTimeEvidence` → `VssFlowProgress` | 동기 조율 / 후속 처리 |
| [Input_Process](20_INPUT_STORE_FUNCTIONS.md#input-process) | [INPUT](../20_MODULES/20_INPUT_MODULE.md) | 출처·순서·품질 검증/적용 후 STORE 전달 | `VssInputEvidence` + `VssTimeEvidence` → `VssInputApplication` | 동기 적용 |
| [Store_ApplyInput](20_INPUT_STORE_FUNCTIONS.md#store-applyinput) | [STORE](../20_MODULES/21_STORE_MODULE.md) | 신규 수용/현재 후보·품질의 적용 | `VssValidatedInput` → `VssStoreAdmission` | 동기 수용 |
| [Store_ApplyPlaybackFact](20_INPUT_STORE_FUNCTIONS.md#store-applyplaybackfact) | [STORE](../20_MODULES/21_STORE_MODULE.md) | 귀속된 확정 재생 사실의 멱등 이력 반영 | `VssPlaybackFact` → `VssFactApplication` | 동기 사실 반영 |
| [Store_AdvanceDeadlines](20_INPUT_STORE_FUNCTIONS.md#store-advancedeadlines) | [STORE](../20_MODULES/21_STORE_MODULE.md) | 최신 출력 사실 재확인 후 age/Hold 진행 | `VssTimeEvidence` + `VssPlaybackObservations` → `VssStoreProgress` | 동기 기한 진행 |
| [Select_Choose](30_POLICY_PLAYBACK_FUNCTIONS.md#select-choose) | [POLICY](../20_MODULES/22_POLICY_MODULE.md) | 읽기 전용 winner/keep/replace/wait 판단 | `VssSelectionView` → `VssSelectionDecision` | 동기 읽기 전용 |
| [Playback_RequestTransition](30_POLICY_PLAYBACK_FUNCTIONS.md#playback-requesttransition) | [PLAYBACK](../20_MODULES/23_PLAYBACK_MODULE.md) | 새 실행/종료 의도 수용·Session 권한 변경 | `VssPlaybackIntent` + `VssExecutionConditions` → `VssRequestDisposition` | 동기 수용 / 하위 비동기 |
| [Playback_Advance](30_POLICY_PLAYBACK_FUNCTIONS.md#playback-advance) | [PLAYBACK](../20_MODULES/23_PLAYBACK_MODULE.md) | 진행 중 사실·cue·기한·늦은 결과/최종 격리·출력 정리와 사용권 종료 | `VssAudioBackendResult` + `VssExecutionConditions` + `VssTimeEvidence` → `VssPlaybackProgress` | 동기 반영 / 비동기 결과 |
| [Asset_Read](40_AUDIO_STREAM_ASSET_FUNCTIONS.md#asset-read) | [ASSET](../20_MODULES/25_ASSET_MODULE.md) | 압축 자료의 범위 읽기/참조 수명 | `VssAssetAccess` → `VssAssetReadResult` | 동기 읽기 / 참조 지속 |
| [AudioStream_Prepare](40_AUDIO_STREAM_ASSET_FUNCTIONS.md#audiostream-prepare) | [AUDIO STREAM](../20_MODULES/24_AUDIO_STREAM_MODULE.md) | 의미 준비→음원 처리 문맥/장치 준비·초기 PCM | `VssAudioPreparation` → `VssAudioBackendResult` | 동기 수용 / 준비 후속 |
| [AudioStream_RequestControl](40_AUDIO_STREAM_ASSET_FUNCTIONS.md#audiostream-requestcontrol) | [AUDIO STREAM](../20_MODULES/24_AUDIO_STREAM_MODULE.md) | start/stop/허용 복구 수용·미래 생산 차단 | `VssAudioControlIntent` + `VssExecutionConditions` → `VssAudioBackendResult` | 동기 수용 / 하위 비동기 |
| [AudioStream_Advance](40_AUDIO_STREAM_ASSET_FUNCTIONS.md#audiostream-advance) | [AUDIO STREAM](../20_MODULES/24_AUDIO_STREAM_MODULE.md) | 하위 사실·PCM 재사용/보충·Backend/복구 결과 | `VssTimeEvidence` → `VssAudioBackendResult` | 동기 반영 / 하위 비동기 |
| [AudioTx_Request](50_DRIVER_HAL_FUNCTIONS.md#audiotx-request) | [AUDIO TX](../20_MODULES/30_AUDIO_TX_MODULE.md) | 요청 전 원래 등록·PCM 접근 보호와 실제 요청 | `VssPcmTransferIntent` → `VssTxProgress` | 동기 요청 / 효과 후속 |
| [AudioTx_Advance](50_DRIVER_HAL_FUNCTIONS.md#audiotx-advance) | [AUDIO TX](../20_MODULES/30_AUDIO_TX_MODULE.md) | HAL 사실 적용·전송/안전 반환/종료 근거 | `VssTimeEvidence` → `VssTxProgress` | 동기 반영 / raw 비동기 |
| [AudioControl_Service](50_DRIVER_HAL_FUNCTIONS.md#audiocontrol-service) | [AUDIO CONTROL](../20_MODULES/31_AUDIO_CONTROL_MODULE.md) | 현재 장치 구성의 준비/무효화·제어 진행 | `VssDeviceControlIntent` + `VssTimeEvidence` → `VssDeviceReadiness` | 동기 진행 / 제어 후속 |
| [AudioHAL_Callback](50_DRIVER_HAL_FUNCTIONS.md#audiohal-callback) | [HAL Boundary](../10_LAYERS/details/40_HAL_BSP_BOUNDARY.md) | 원래 등록의 해석 전 장치 사실/시각/손실 보존 | `VssHalRegistration` + `VssRawObservation` → `void` | 비동기 진입 / 짧은 보존 |
| [Health_Evaluate](60_HEALTH_RUNTIME_FUNCTIONS.md#health-evaluate) | [HEALTH](../20_MODULES/26_HEALTH_MODULE.md) | 현재/최근 Fault·대상별 복구 허용/해제 판단 | `VssDiagnosticObservations` → `VssHealthAssessment` | 동기 판단·적용 |
| [Health_BuildStatus](60_HEALTH_RUNTIME_FUNCTIONS.md#health-buildstatus) | [HEALTH](../20_MODULES/26_HEALTH_MODULE.md) | 원본 읽기 전용 일관된 VSS_STATUS 파생 | `VssStatusObservations` → `VssStatusSnapshot` | 동기 읽기 전용 |

<a id="candidate-decisions"></a>
## 기존 14개 항목의 전수 판정

| 기존 항목 | 판정 | 최종 연결 | 상태전이 / 호출 / 실패 계약 근거 |
| --- | --- | --- | --- |
| `Flow_Process` | **KEEP** | [Flow_Process](10_FLOW_FUNCTIONS.md#flow-process) | 조율 자체의 적용 완료 관측/재평가 상태전이와 단일 외부 진입 계약. 다른 Module의 선택/실행/원본 쓰기는 callee로 내려간다. |
| `Input_Process` | **KEEP** | [Input_Process](20_INPUT_STORE_FUNCTIONS.md#input-process) | RX와 RX 없는 품질 변화 모두 동일 출처 적용 owner·검증/거부·STORE 전달 계약. 출처 순서 검증 복제를 피한다. |
| `Store_Process` | **SPLIT** | [Store_ApplyInput](20_INPUT_STORE_FUNCTIONS.md#store-applyinput) · [Store_ApplyPlaybackFact](20_INPUT_STORE_FUNCTIONS.md#store-applyplaybackfact) · [Store_AdvanceDeadlines](20_INPUT_STORE_FUNCTIONS.md#store-advancedeadlines) | INPUT 신규 수용 거부, PLAYBACK 확정 사실의 보호된 멱등 반영, FLOW 기한/최신 출력 재확인은 caller·전이·실패 의무가 다르다. |
| `Select_Choose` | **KEEP** | [Select_Choose](30_POLICY_PLAYBACK_FUNCTIONS.md#select-choose) | POLICY의 읽기 전용 선택 경계. PLAYBACK 실행과 owner/입출력 책임이 다르다. |
| `Playback_Process` | **SPLIT** | [Playback_RequestTransition](30_POLICY_PLAYBACK_FUNCTIONS.md#playback-requesttransition) · [Playback_Advance](30_POLICY_PLAYBACK_FUNCTIONS.md#playback-advance) | 새 의도 채택/후속 권한 차단의 수용 계약과 이미 진행한 Session/Attempt 사실 적용·늦은 결과/최종 격리의 의무를 분리한다. |
| `Asset_Read` | **KEEP** | [Asset_Read](40_AUDIO_STREAM_ASSET_FUNCTIONS.md#asset-read) | 압축 자료 공급과 실제 소비 주체 참조 수명의 ASSET 경계. lookup/read/참조 완료는 같은 자료 수명의 내부 분기다. |
| `AudioStream_Prepare` | **KEEP** | [AudioStream_Prepare](40_AUDIO_STREAM_ASSET_FUNCTIONS.md#audiostream-prepare) | 의미 준비→음원 처리 문맥/음원/장치 준비·초기 PCM 경계. 준비 완료는 start/actual과 다르므로 제어 요청에 합치지 않는다. |
| `AudioStream_Process` | **SPLIT** | [AudioStream_RequestControl](40_AUDIO_STREAM_ASSET_FUNCTIONS.md#audiostream-requestcontrol) · [AudioStream_Advance](40_AUDIO_STREAM_ASSET_FUNCTIONS.md#audiostream-advance) | 새 start/stop/허용 복구의 수용·미래 생산 권한 변경과 하위 사실/PCM 안전 반환·복구 수행/검증의 후속 적용 의무가 다르다. |
| `AudioTx_Process` | **SPLIT** | [AudioTx_Request](50_DRIVER_HAL_FUNCTIONS.md#audiotx-request) · [AudioTx_Advance](50_DRIVER_HAL_FUNCTIONS.md#audiotx-advance) | 하위 요청 전 등록·pending 보호/신규 거부와 이미 발생한 해석 전 장치 사실·부분 효력·미래 접근 종료의 후속 적용은 다른 실패/async 계약이다. |
| `AudioControl_Service` | **KEEP** | [AudioControl_Service](50_DRIVER_HAL_FUNCTIONS.md#audiocontrol-service) | 하나의 지속되는 장치 구성 준비/무효화 lifecycle. 제어 요청과 후속 진행의 caller/owner/실패 범위가 같아 상태별 wrapper가 불필요하다. |
| `AudioHAL_Callback` | **KEEP** | [AudioHAL_Callback](50_DRIVER_HAL_FUNCTIONS.md#audiohal-callback) | HAL Layer Boundary의 비동기 진입·짧은 raw 보존. DRIVER 의미 적용과 합치면 ISR/귀속 수명 계약이 사라진다. |
| `Health_Evaluate` | **KEEP** | [Health_Evaluate](60_HEALTH_RUNTIME_FUNCTIONS.md#health-evaluate) | 현재/최근 Fault·허용/제한의 writer 경계. 판단은 Backend 수행·검증·해제와 구별된다. |
| `Health_BuildStatus` | **KEEP** | [Health_BuildStatus](60_HEALTH_RUNTIME_FUNCTIONS.md#health-buildstatus) | 원본을 변경하지 않는 외부 파생 보고 경계. 진단 writer와 병합하면 읽기 전용 보고 계약이 사라진다. |
| `AudioStream_FillBuffer` | **INTERNALIZE** | [AUDIO STREAM 내부 메모](40_AUDIO_STREAM_ASSET_FUNCTIONS.md#internal-fill-buffer) — Core 제외 | 현재 내부 처리 유지. 같은 AUDIO STREAM 음원 처리 문맥/PCM 주기의 CPU 생산이며 독립 외부 caller·수용/거부·async 계약이 없다. |

MERGE는 owner/lifecycle/실패 계약이 같은지, REMOVE는 실제 상태전이/외부 경계가 없는지를 전 항목에 적용해 검토했다. 9개 KEEP는 각각 실제 경계 또는 원본 상태의 전이를 맡고 서로 합치면 owner·읽기/쓰기·ISR·준비/실행 계약이 사라진다. 4개 큰 후보는 내부 책임을 제거하지 않고 수용/기한/사실 적용 계약을 나눈다. 내부 PCM 처리 외에 삭제할 잔여 getter/setter/trivial wrapper 후보는 없다.

분리한 각 그룹의 [계약 구분 검토 사례](20_INPUT_STORE_FUNCTIONS.md#input-store)와 상세 Core 판정에서 신규 요청 거부·이미 발생한 사실·늦게 도착한 결과·반환/관측 손실을 비교한다. Start/Stop/Result/Timeout/상태/장치마다 함수 하나를 만드는 방식은 사용하지 않았다.

<a id="boundary-review"></a>
## 큰 처리 함수의 재검토

| 검토 대상 | 유지 / 분리할 계약 | 압축 또는 기계 분할을 피한 근거 |
| --- | --- | --- |
| FLOW | `Flow_Process` 유지 | FLOW의 외부 진입·관측 일관성·조율 진행만 소유한다. 선택/Session/PCM/Fault는 각 owner Core로 내려가고 결과를 기다리며 무한 진행하지 않는다. |
| INPUT | `Input_Process` 유지 | 새 RX·RX 없는 품질 변화는 같은 출처/순서 적용 문맥과 거부 계약이며 generic EventBus가 아니다. |
| STORE | 입력 수용 / 확정 사실 적용 / 기한 진행 | caller와 신규 자원 거부·보호된 통지 의무·비동기 출력 재확인의 실패 의미가 다르다. 각 이력 상태마다 별도 API는 없다. |
| PLAYBACK | 의도 수용 / 진행 중 사실 적용 | 새 Session 채택·차단 권한과 이미 채택한 Attempt의 실제/늦은 결과/최종 의무를 분리한다. Start/Stop/Retire의 작은 상태 분기는 내부다. |
| AUDIO STREAM | 준비 유지 + 제어 수용 / 하위 진행 | prepared/start/actual 및 미래 생산 차단/PCM 안전 반환을 구별한다. 음원 처리 문맥 결과·PCM 보충·복구 진행은 같은 원래 문맥의 내부 분기다. |
| AUDIO TX | 요청 전 보호 / 해석 전 장치 사실 적용 | 즉시 callback 가능·부분 효력·미래 접근 종료가 요청 거부와 다르다. callback별 wrapper와 가상의 DMA 절차는 없다. |
| AUDIO CONTROL | `AudioControl_Service` 유지 | 한 현재 장치 구성의 지속되는 준비/무효화·제어 수명이다. 같은 caller/owner/범위의 요청과 결과를 나눌 독립 근거가 없으며 새 Attempt마다 초기화하지 않는다. |
| HEALTH | 판단 writer / 읽기 전용 보고 유지 | 진단·허용/해제와 파생 보고의 Writes가 다르다. Recovery 단계별 API나 publisher wrapper는 없다. |

<a id="merged"></a>
## 이전 병합 명칭과 R2 연결

이 표는 보존한 R0/R1/Stage5 문서의 옛 명칭을 읽기 위한 연결이다. alias API를 만들거나 이전 함수 개수·분할을 복원하라는 뜻이 아니다. 현재 목록과 상세 계약은 위 R2 색인을 따른다.

| 이전 설명의 명칭 | R2의 의미 위치 |
| --- | --- |
| Store_ApplyInput / Store_AdvanceDeadlines / Store_ApplyPlaybackResult → Store_Process | R2에서 입력 수용·기한 진행·확정 사실의 계약을 다시 분리했다. 결과 적용은 `Store_ApplyPlaybackFact`이며 옛 Result alias는 만들지 않는다. |
| Playback_Request / Playback_Process / Playback_Stop | 의도 수용은 `Playback_RequestTransition`, 이미 진행한 Session/Attempt 사실·정상 cue/출력 정리와 사용권 종료는 `Playback_Advance`다. Start/Stop은 내부 분기다. |
| AudioStream_Start / AudioStream_Stop / AudioStream_Process | 제어 수용은 `AudioStream_RequestControl`, 진행 중 하위 사실/PCM/복구는 `AudioStream_Advance`다. |
| AudioTx_HandleEvent / AudioTx_Process | 요청 전 보호/수용은 `AudioTx_Request`, 원래 해석 전 장치 사실 적용은 `AudioTx_Advance`다. generic Event 입력은 만들지 않는다. |
| AudioStream_FillBuffer | 같은 owner 내부 PCM 생산 메모. 외부 Core/prototype로 승격하지 않는다. |

<a id="provisional-types"></a>
## Pseudo prototype의 provisional semantic type

모든 `Vss...`는 **R3에서 상세화할 불투명한 의미 표지**다. 아래는 함수가 요구/반환하는 의미와 기존 자료 위치의 대응이며 새 Data 설계가 아니다. 타입 수와 실제 struct·enum·Header 수는 대응하지 않는다. 서로 합치거나 나눌 수 있으며 field/enum member/폭/layout/상수·용량·메모리 방식은 정의하지 않는다.

`const`와 포인터는 callee가 전달받은 원본을 직접 쓰지 않는 읽기 방향을, 반환은 의미 결과의 전달을 보인다. NULL/optional/복사/참조/수명 전달·결과군 보존 방식·C ABI는 R3 이후 대상이다. 이미 수용한 준비/제어의 진행이나 RX 없는 기회, 결과 없는 기한 처리는 상세 Input의 의미로 허용하지만 실제 표현을 지금 고정하지 않는다.

| Provisional type | 의미 책임 / 전달 | 의미와 R3 구체화 대상 | 기존 근거 |
| --- | --- | --- | --- |
| `VssProcessingOpportunity` | FLOW / Runtime 연결 | 입력 또는 후속 처리의 기회. 알림 자체는 출력 사실이 아니다. | [Runtime 시간·알림·손실](../10_LAYERS/details/50_TIMEBASE_RUNTIME_BOUNDARY.md) |
| `VssTimeEvidence` | Runtime Boundary | 원본 시각 비교·경과·연속성/손실의 근거. 후속 적용 시각으로 사실 시각을 교체하지 않는다. | [Runtime 시간·알림·손실](../10_LAYERS/details/50_TIMEBASE_RUNTIME_BOUNDARY.md) |
| `VssFlowProgress` | FLOW | 조율 진행·재평가/보호된 후속 처리·보고 전달의 의미 결과. 타 owner 원본의 새 복제가 아니다. | [선택 관측·판단·전체 계획](../40_DATA/30_SELECTION_DATA.md#selection) |
| `VssInputEvidence` | INPUT | 제품/TEST 값·Meta/출처 근거 또는 RX 없는 품질 점검 기회. | [입력 문맥·Meta](../40_DATA/10_INPUT_DATA.md#input) |
| `VssInputApplication` | INPUT | 검증/문맥 적용·품질 변화와 STORE의 실제 수용 여부를 구별하는 결과. | [입력 문맥·Meta](../40_DATA/10_INPUT_DATA.md#input) |
| `VssValidatedInput` | INPUT → STORE | INPUT이 검증·적용한 사건/상태/품질과 원래 출처/시간 근거. | [입력 문맥·Meta](../40_DATA/10_INPUT_DATA.md#input) |
| `VssStoreAdmission` | STORE | 신규 수용·동일 발생·거부/현재 후보 갱신의 의미 결과. 출력 시작이 아니다. | [발생 이력·현재 후보](../40_DATA/20_STORE_DATA.md#store) |
| `VssPlaybackFact` | PLAYBACK → STORE | Session/Attempt에서 확정한 귀속된 시작·전체 최종·미시작/불확실 사실. | [Session·Attempt 문맥](../40_DATA/40_PLAYBACK_DATA.md#playback) |
| `VssFactApplication` | STORE | 확정 사실 반영 완료/중복·근거 부족과 보호된 통지 의무의 반영 여부. | [발생 이력·현재 후보](../40_DATA/20_STORE_DATA.md#store) |
| `VssPlaybackObservations` | PLAYBACK → FLOW → STORE | 미시작 기한 진행 전에 재확인할 반영 완료 출력/권한 관측. | [Session·Attempt 문맥](../40_DATA/40_PLAYBACK_DATA.md#playback) |
| `VssStoreProgress` | STORE | age/Hold·만료/보류와 현재 후보·재평가의 의미 진행 결과. | [발생 이력·현재 후보](../40_DATA/20_STORE_DATA.md#store) |
| `VssSelectionView` | FLOW → POLICY | STORE/PLAYBACK/Backend/HEALTH의 함께 판단 가능한 관측 묶음. 원본 writer가 아니다. | [선택 관측·판단·전체 계획](../40_DATA/30_SELECTION_DATA.md#selection) |
| `VssSelectionDecision` | POLICY | winner/keep/replace/wait와 의미상 전체 계획의 읽기 전용 판단. | [선택 관측·판단·전체 계획](../40_DATA/30_SELECTION_DATA.md#selection) |
| `VssPlaybackIntent` | FLOW → PLAYBACK | POLICY 판단에 연결된 채택/유지/교체 또는 종료 의도. | [Session·Attempt 문맥](../40_DATA/40_PLAYBACK_DATA.md#playback) |
| `VssExecutionConditions` | FLOW / PLAYBACK → AUDIO STREAM | 새 실행 직전의 최신 후보/CLEAR·기한·제한·owner/보호 자원·복구 허용 의미. 상위 상태 변경 권한이 아니다. | [선택 관측·판단·전체 계획](../40_DATA/30_SELECTION_DATA.md#selection) |
| `VssRequestDisposition` | PLAYBACK | 의도 수용/유지/대기/거부와 재평가 필요성. | [Session·Attempt 문맥](../40_DATA/40_PLAYBACK_DATA.md#playback) |
| `VssPlaybackProgress` | PLAYBACK | 현재 출력/최종·출력 정리와 사용권 종료·보호된 STORE 통지·재평가/정리의 의미 결과. | [Session·Attempt 문맥](../40_DATA/40_PLAYBACK_DATA.md#playback) |
| `VssAudioPreparation` | PLAYBACK → AUDIO STREAM | 원래 Session/Attempt·계획/cue·음원·첫 시작 기한의 의미 준비 요구. | [준비·음원 처리 문맥·Backend 결과](../40_DATA/50_AUDIO_STREAM_DATA.md#provider) |
| `VssAudioControlIntent` | PLAYBACK / FLOW → AUDIO STREAM | 원래 start/정리 또는 HEALTH가 허용한 복구 대상·범위의 제한된 제어 의도. | [준비·음원 처리 문맥·Backend 결과](../40_DATA/50_AUDIO_STREAM_DATA.md#provider) |
| `VssAudioBackendResult` | AUDIO STREAM → 상위 owner | 준비·제어 수용·실제 출력/안전 반환/종료·불명/손실 및 복구 수행/별도 검증을 범위/귀속과 함께 구별하는 결과군. 단일 덮어쓰기 슬롯을 의미하지 않는다. | [준비·음원 처리 문맥·Backend 결과](../40_DATA/50_AUDIO_STREAM_DATA.md#provider) |
| `VssAssetAccess` | AUDIO STREAM → ASSET | 압축 자료의 제한된 읽기 또는 제공 자료의 실제 소비 주체 참조 진행 의미. | [Asset 설명·읽기 참조](../40_DATA/50_AUDIO_STREAM_DATA.md#asset) |
| `VssAssetReadResult` | ASSET | 확인 범위/bytes·읽기 또는 참조 반영·발견 단계의 의미 결과. 전체 디코딩 성공은 아니다. | [Asset 설명·읽기 참조](../40_DATA/50_AUDIO_STREAM_DATA.md#asset) |
| `VssPcmTransferIntent` | AUDIO STREAM → AUDIO TX | 원래 Attempt/PCM 주기·유효 구간의 인계/start/정리 의도. | [PCM A/B 사용권](../40_DATA/50_AUDIO_STREAM_DATA.md#pcm) |
| `VssTxProgress` | AUDIO TX | 요청 수용/활성화·접근 보호·출력 후보/소비/안전 반환·무출력/종료·손실의 구별된 전송 근거군. | [TX operation·HAL 귀속 기록](../40_DATA/60_AUDIO_TX_DATA.md#tx) |
| `VssDeviceControlIntent` | AUDIO STREAM → AUDIO CONTROL | 현재 대상/구성의 제어 의도 또는 이미 수용한 제어 진행 기회. | [장치별 구성 준비](../40_DATA/60_AUDIO_TX_DATA.md#control) |
| `VssDeviceReadiness` | AUDIO CONTROL | 해당 구성 준비/무효화·제어 진행·수행/검증·단계별 실패 근거. | [장치별 구성 준비](../40_DATA/60_AUDIO_TX_DATA.md#control) |
| `VssHalRegistration` | HAL Boundary | 실제 등록 당시 operation/PCM 주기 귀속과 callback/참조의 별도 수명. | [TX operation·HAL 귀속 기록](../40_DATA/60_AUDIO_TX_DATA.md#tx) |
| `VssRawObservation` | vendor / HAL Boundary | 원래 장치 해석 전 장치 사실·발생 시각·관측 범위/손실. 의미상 Session outcome이 아니다. | [TX operation·HAL 귀속 기록](../40_DATA/60_AUDIO_TX_DATA.md#tx) |
| `VssDiagnosticObservations` | 각 owner → FLOW → HEALTH | 원래 발견 주체·단계·대상·시각/반영 완료 및 복구 수행/별도 검증 근거. | [Fault·복구 문맥](../40_DATA/70_DIAGNOSTIC_DATA.md#health) |
| `VssHealthAssessment` | HEALTH | 현재/최근 Fault·대상별 제한/복구 허용·현재 대상 해제의 판단. | [Fault·복구 문맥](../40_DATA/70_DIAGNOSTIC_DATA.md#health) |
| `VssStatusObservations` | 각 owner → FLOW → HEALTH | 보고를 함께 판단할 수 있는 반영 완료 관측과 HEALTH의 현재 판단. | [파생 상태 보고](../40_DATA/70_DIAGNOSTIC_DATA.md#status) |
| `VssStatusSnapshot` | HEALTH → FLOW → INPUT | 파생 VSS_STATUS와 관측 일관성/보고 생성 가능 여부. 부족하면 새 정상 보고를 만들지 않는다. | [파생 상태 보고](../40_DATA/70_DIAGNOSTIC_DATA.md#status) |

<a id="r3-handoff"></a>
## R3 인계와 후속 범위

R3는 위 의미 자료의 전달/참조 기간, 원래 발생·Session/Attempt·PCM 주기/장치 구성의 연관 표현, 수용/진행/확정·중복/손실 구분, 실제 안전 반환/과거 무출력/미래 차단 근거, 수행/검증/현재 대상 해제의 연결을 구체화한다. 하위 사실을 덮지 않는 보존과 반영 완료 관측의 표현도 필요하다. 정확한 field·enum·struct·Header는 이번에 정하지 않는다.

원래 첫 시작 age·Hold/재실행·One-shot/Stateful·5 Fault·WINDOW [구현 보류 — 설계 유지]를 다시 결정하지 않는다. Cross Contract 원문 통합은 R4, 기존 Pseudocode 재작성은 R5, Header ownership은 R6, 통합 검수는 R7, 실제 source/RTD·DMA·TCD Binding은 B2-R이다. 실제 `.c/.h` 구현·Build·Flash·Task/Queue/Mutex 결정은 수행하지 않았다.

[Module Overview](../20_MODULES/00_MODULE_OVERVIEW.md) · [Data 의미](../40_DATA/00_DATA_OVERVIEW.md) · [공통 계약 위치](../50_CONTRACTS/00_CONTRACT_OVERVIEW.md) · [기존 의사코드](../60_PSEUDOCODE/00_PSEUDOCODE_OVERVIEW.md) · [미정 Binding / 정책](../90_BINDING/91_IMPLEMENTATION_TBD.md)
