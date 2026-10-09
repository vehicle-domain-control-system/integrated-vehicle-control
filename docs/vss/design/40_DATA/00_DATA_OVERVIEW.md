# Data Overview — R3

> 2026-10-07 · R2 Core 18개와 provisional 의미 표지 32개를 출발점으로 Data/Command 상세화

> 2026-10-08 R4 · R3-C1 세 보완 및 두 함수 논리 경계 독립 PASS 기준. 공통 반복 설명을 R4 계약으로 연결하고 판정/필드/owner/pseudo-C는 보존. 일반 채팅 R4 독립 검수 대기.

기준은 Architecture Baseline / Execution Plan **v1.1**이다. 기존 5개 Layer·10개 Module·HAL/BSP/Runtime Layer Boundary 구조와 Core Function 계약은 보존한다. 이번은 논리 필드·pseudo-C이며 실제 Header/ABI·폭/packing·vendor Binding·C 구현 완료가 아니다.

## 데이터 경로와 상세 위치

| 실제 boundary | 전달 의미 / owner | 상세 |
| --- | --- | --- |
| 기존 통신/TEST → INPUT → STORE | 원본 값+Meta 검증 → One-shot Event / StateUpdate / QualityChange. received·valid·accepted 구별 | [입력](10_INPUT_DATA.md#input) |
| STORE → FLOW → POLICY | 발생/현재 경고를 분리한 최소 후보 읽기. 원본 writer는 STORE | [이력/현재 상태](20_STORE_DATA.md#store) · [관측/선택](30_SELECTION_DATA.md#selection) |
| POLICY/FLOW → PLAYBACK | 읽기 판단/전체 의미 계획·현재 종료 의도. 실행 writer는 PLAYBACK | [선택](30_SELECTION_DATA.md#selection) · [Session/Attempt](40_PLAYBACK_DATA.md#playback) |
| PLAYBACK → AUDIO STREAM | 원래 준비·같은 Attempt START/STOP. 실제 start는 별도 근거 | [준비/provider](50_AUDIO_STREAM_DATA.md#provider) |
| AUDIO STREAM → ASSET | 제한된 MP3 byte 읽기·원래 span 참조 종료 | [Asset](50_AUDIO_STREAM_DATA.md#asset) |
| AUDIO STREAM → AUDIO TX | 안정된 PCM 회차/유효 구간 인계 또는 출력 범위 제어 | [PCM A/B](50_AUDIO_STREAM_DATA.md#pcm) · [TX](60_AUDIO_TX_DATA.md#tx) |
| AUDIO STREAM → AUDIO CONTROL | 현재 device/config 제어 요구·준비/수행 결과 | [구성 준비](60_AUDIO_TX_DATA.md#control) |
| AUDIO TX / CONTROL ↔ HAL | 요청 전 불변 귀속 → raw 포착/손실 → 원래 owner 적용 | [등록/실제 근거](60_AUDIO_TX_DATA.md#tx) |
| AUDIO TX → AUDIO STREAM → PLAYBACK | 요청 결과·소비·안전 반환·실제 출력/무출력/종료 근거를 분리. 전체 Session final은 PLAYBACK | [하위 근거](60_AUDIO_TX_DATA.md#tx) · [Backend 근거](50_AUDIO_STREAM_DATA.md#provider) |
| PLAYBACK → STORE | 최초 적법한 시작/전체 최종의 보호된 멱등 fact | [재생 사실](40_PLAYBACK_DATA.md#vssplaybackfact) · [STORE 적용](20_STORE_DATA.md#vssfactapplication) |
| owner 관측 → HEALTH → VSS_STATUS | 원래 진단/현재 제한·복구 허용 → 실제 수행 → 별도 검증 → 현재 대상 해제. 보고는 읽기 파생 | [진단/복구](70_DIAGNOSTIC_DATA.md#health) · [보고](70_DIAGNOSTIC_DATA.md#status) |

새 Request/Result struct를 모든 API에 붙이지 않았다. 관측 이름은 typed borrow를 나타낼 수 있고 scalar enum은 하나의 판정값이다. Context는 여러 처리 기회 동안 실제 유지되는 state/lifetime에만 둔다. Descriptor는 불변 계획/음원/귀속 설명, Snapshot은 owner 원본에서 읽기 전용으로 파생한 값이다. Event는 실제 One-shot 발생 또는 적용할 정규화 재생 사실이며 Evidence와 구별한다.

<a id="type-decisions"></a>
## R2 provisional type 32개 전수 판정

KEEP_AS_TYPE는 명시 Data가 실제 필요한 경우, MERGE는 같은 owner/생산·소비/수명의 읽기 의미 재사용, SPLIT은 다른 lifetime/validity 분리, INLINE은 scalar/enum·기존 typed 참조/인자로 충분한 경우다. REMOVE도 전 항목에서 검토했다. 독립 저장은 불필요해도 실제 전달 의미는 남겨야 하므로 의미 자체를 삭제할 항목은 없었다.

| R2 provisional 이름 | 판정 | R3 표현 | owner / lifetime / validity 근거 |
| --- | --- | --- | --- |
| `VssProcessingOpportunity` | **INLINE** | [VssProcessingOpportunity](10_INPUT_DATA.md#vssprocessingopportunity) | 알림은 기회뿐. 값 하나로 충분하며 payload/사실을 담지 않는다. |
| `VssTimeEvidence` | **KEEP_AS_TYPE** | [VssTimeEvidence](10_INPUT_DATA.md#vsstimeevidence) | Runtime 문맥/epoch·구간/연속성이 함께 유효한 실제 시간 boundary. |
| `VssFlowProgress` | **INLINE** | [VssFlowProgress](10_INPUT_DATA.md#vssflowprogress) | 다음 처리 필요 enum. 상세 조율/보고 상태는 기존 FLOW 내부에 남는다. |
| `VssInputEvidence` | **KEEP_AS_TYPE** | [VssInputEvidence](10_INPUT_DATA.md#vssinputevidence) | 하나의 원본 의미+Meta/품질이 함께 검증되는 수신 경계. RX 없으면 인자 없음. |
| `VssInputApplication` | **KEEP_AS_TYPE** | [VssInputApplication](10_INPUT_DATA.md#vssinputapplication) | 검증/문맥 적용/STORE 수용은 같은 호출 결과지만 서로 다른 값이어야 함. |
| `VssValidatedInput` | **SPLIT** | [VssOneShotEvent](10_INPUT_DATA.md#vssoneshotevent) · [VssStateUpdate](10_INPUT_DATA.md#vssstateupdate) · [VssQualityChange](10_INPUT_DATA.md#vssqualitychange) | 발생 identity/replay 수명과 현재 상태/품질의 적용·수명이 다름. |
| `VssStoreAdmission` | **INLINE** | [VssStoreAdmission](20_STORE_DATA.md#vssstoreadmission) | 동기 수용/갱신 판정 enum. 이력 복제 Result struct 불필요. |
| `VssPlaybackFact` | **KEEP_AS_TYPE** | [VssPlaybackFact](40_PLAYBACK_DATA.md#vssplaybackfact) | 원래 occurrence/Session/Attempt·불변 fact·시각을 보호해 STORE에 멱등 적용. |
| `VssFactApplication` | **INLINE** | [VssFactApplication](20_STORE_DATA.md#vssfactapplication) | 한 fact의 실제 적용/중복 완료/미반영 scalar. 통지 의무는 producer가 보호. |
| `VssPlaybackObservations` | **MERGE** | [VssPlaybackObservation](40_PLAYBACK_DATA.md#vssplaybackobservation) | 동일 PLAYBACK 반영 완료 projection을 기한/선택/보고·진행 읽기에 재사용. 원본 Context와 병합 아님. |
| `VssStoreProgress` | **INLINE** | [VssStoreProgress](20_STORE_DATA.md#vssstoreprogress) | 변화/재확인 필요 enum. 후보는 별도 STORE 관측. |
| `VssSelectionView` | **INLINE** | [VssCandidateObservation](30_SELECTION_DATA.md#vsscandidateobservation) · [VssReadBasis](30_SELECTION_DATA.md#vssreadbasis) | typed 관측 borrow의 인자 묶음. 원본 여러 owner를 복제하는 struct는 불필요. |
| `VssSelectionDecision` | **KEEP_AS_TYPE** | [VssSelectionDecision](30_SELECTION_DATA.md#vssselectiondecision) | 같은 POLICY 평가·producer·수명에서 후보/계획/action·근거 관계를 반환. |
| `VssPlaybackIntent` | **INLINE** | [VssSelectionDecision](30_SELECTION_DATA.md#vssselectiondecision) / Session key·종료 사유 scalar | 읽기 판단 또는 현재 종료 의도. 동일 정보를 새 wrapper에 복사하지 않는다. |
| `VssExecutionConditions` | **INLINE** | [VssCandidateObservation](30_SELECTION_DATA.md#vsscandidateobservation) · [VssReadBasis](30_SELECTION_DATA.md#vssreadbasis) / 현재 대상 제한 | 이번 후보의 작은 최신 읽기 근거만 전달. 자기 Session/PCM/준비 자원은 callee 원본에서 확인. |
| `VssRequestDisposition` | **INLINE** | [VssRequestDisposition](40_PLAYBACK_DATA.md#vssrequestdisposition) | 수용/유지/대기/거부 enum. 실제 effect는 별도 Evidence. |
| `VssPlaybackProgress` | **SPLIT** | [VssPlaybackObservation](40_PLAYBACK_DATA.md#vssplaybackobservation) · [VssPlaybackFact](40_PLAYBACK_DATA.md#vssplaybackfact) · [VssFactApplication](20_STORE_DATA.md#vssfactapplication) · [VssDiagnosticEvidence](70_DIAGNOSTIC_DATA.md#vssdiagnosticevidence) | 현재 읽기·보호된 통지·진단은 서로 다른 lifetime. 마지막 Progress 슬롯 금지. |
| `VssAudioPreparation` | **KEEP_AS_TYPE** | [VssAudioPreparation](50_AUDIO_STREAM_DATA.md#vssaudiopreparation) | PLAYBACK의 원래 시도·계획/구간·첫 시작 근거가 함께 준비 요구로 보호됨. |
| `VssAudioControlIntent` | **SPLIT** | [VssPlaybackControlCommand](50_AUDIO_STREAM_DATA.md#vssplaybackcontrolcommand) · [VssRecoveryPermission](70_DIAGNOSTIC_DATA.md#vssrecoverypermission) | Attempt START/STOP과 Session 없이도 가능한 Fault 대상 복구는 실제 입력/수명/권한이 다름. |
| `VssAudioBackendResult` | **SPLIT** | [VssAudioRequestResult](50_AUDIO_STREAM_DATA.md#vssaudiorequestresult) · [VssAudioOutputEvidence](50_AUDIO_STREAM_DATA.md#vssaudiooutputevidence) · [VssBackendCleanupObservation](50_AUDIO_STREAM_DATA.md#vssbackendcleanupobservation) / 진단·수행·검증 Evidence | 준비/요청 반환·물리 출력·자원 정리·복구 사실의 producer/validity·보존 의무가 다름. |
| `VssAssetAccess` | **SPLIT** | [VssAssetReadRequest](50_AUDIO_STREAM_DATA.md#vssassetreadrequest) / span key·실제 참조 종료 근거 | 새 범위 읽기와 기존 제공 자료 참조 종료는 필드/validity·자료 수명이 다름. release struct는 만들지 않음. |
| `VssAssetReadResult` | **SPLIT** | [VssAssetReadDisposition](50_AUDIO_STREAM_DATA.md#vssassetreaddisposition) · [VssAssetSpan](50_AUDIO_STREAM_DATA.md#vssassetspan) · [VssDiagnosticEvidence](70_DIAGNOSTIC_DATA.md#vssdiagnosticevidence) | 동기 결과 enum/실제 소비 bytes·참조/실패 발견 진단의 수명이 다름. |
| `VssPcmTransferIntent` | **SPLIT** | [VssPcmHandoffCommand](60_AUDIO_TX_DATA.md#vsspcmhandoffcommand) · [VssTxControlCommand](60_AUDIO_TX_DATA.md#vsstxcontrolcommand) | samples·유효 구간/회차 보호가 필요한 인계와 출력 범위 제어는 다른 shape. START/STOP끼리는 공통 유지. |
| `VssTxProgress` | **SPLIT** | [VssTxRequestResult](60_AUDIO_TX_DATA.md#vsstxrequestresult) · [VssTxOutputEvidence](60_AUDIO_TX_DATA.md#vsstxoutputevidence) · [VssPcmConsumptionEvidence](60_AUDIO_TX_DATA.md#vsspcmconsumptionevidence) · [VssPcmReturnEvidence](60_AUDIO_TX_DATA.md#vsspcmreturnevidence) | 요청 수락/출력/소비/안전 반환을 합치면 충분조건·원래 회차/자료 수명이 사라짐. 진단 별도. |
| `VssDeviceControlIntent` | **KEEP_AS_TYPE** | [VssDeviceControlIntent](60_AUDIO_TX_DATA.md#vssdevicecontrolintent) | 한 장치/구성의 제한된 제어 요청. 진행은 Intent 없음+기존 문맥. 복구 허용 1개 conditional 참조만 사용. |
| `VssDeviceReadiness` | **SPLIT** | [VssDeviceReadinessState](60_AUDIO_TX_DATA.md#vssdevicereadinessstate) · [VssDeviceControlResult](60_AUDIO_TX_DATA.md#vssdevicecontrolresult) / 진단·수행·검증 | Session보다 오래가는 준비 원본과 개별 control 결과/복구 사실 수명이 다름. |
| `VssHalRegistration` | **SPLIT** | [VssHalTxRegistration](60_AUDIO_TX_DATA.md#vsshaltxregistration) · [VssHalDeviceRegistration](60_AUDIO_TX_DATA.md#vsshaldeviceregistration) | TX PCM/시도 귀속과 device/config 귀속의 shape/참조가 다름. 공통 callback 연결은 등록 key scalar. |
| `VssRawObservation` | **KEEP_AS_TYPE** | [VssRawObservation](60_AUDIO_TX_DATA.md#vssrawobservation) | 같은 raw 포착의 원래 등록·사건/포착 시각·범위/손실은 함께 보호해야 함. |
| `VssDiagnosticObservations` | **INLINE** | [VssDiagnosticEvidence](70_DIAGNOSTIC_DATA.md#vssdiagnosticevidence) / 수행·별도 검증·읽기 근거 | producer별 typed 사실을 그대로 borrow. 종합 owner-state struct 불필요. |
| `VssHealthAssessment` | **SPLIT** | [VssFaultState](70_DIAGNOSTIC_DATA.md#vssfaultstate) · [VssRecoveryPermission](70_DIAGNOSTIC_DATA.md#vssrecoverypermission) · [VssHealthChange](70_DIAGNOSTIC_DATA.md#vsshealthchange) | 현재/최근 제한 원본·현재 대상 허용·이번 적용 결과는 수명과 소비 방식이 다름. |
| `VssStatusObservations` | **INLINE** | [VssReadBasis](30_SELECTION_DATA.md#vssreadbasis) / owner별 반영 완료 typed 관측 | 보고만 필요한 projection을 borrow하며 authoritative state 전체 복사 금지. |
| `VssStatusSnapshot` | **SPLIT** | [VssStatusSnapshot](70_DIAGNOSTIC_DATA.md#vssstatussnapshot) · [VssStatusBuildResult](70_DIAGNOSTIC_DATA.md#vssstatusbuildresult) | 보고 생성 여부 enum과 실제 생성한 불변 전달 자료를 분리. 부족 관측에서 정상 field 합성 금지. |

전수 판정 합계: **KEEP_AS_TYPE 8**, **MERGE 1**, **SPLIT 12**, **INLINE 11**, **REMOVE 0** = 32개. 이 수는 최종 C type/struct/Header 수가 아니다.

<a id="function-data-map"></a>
## Core 18개 Input / Return 대응

R2 문서의 32개 이름은 당시 불투명 의미 표지다. 아래가 R3의 구체 표현이며 R2 prototype·Function 계약은 수정하지 않았다. SPLIT/INLINE을 타입 alias/통합 union으로 되돌려 R2 이름을 억지로 유지하지 않는다. 최종 C 인자/반환 형태는 후속 국소 검토/R6에서 맞춘다.

| Core Function | R3 Input | R3 Return / observable 결과 | 정합성 / 이유 |
| --- | --- | --- | --- |
| [Flow_Process](../30_FUNCTIONS/10_FLOW_FUNCTIONS.md#flow-process) | [VssProcessingOpportunity](10_INPUT_DATA.md#vssprocessingopportunity) + [VssTimeEvidence](10_INPUT_DATA.md#vsstimeevidence) | [VssFlowProgress](10_INPUT_DATA.md#vssflowprogress) | 기회/시간과 다음 처리 필요 scalar. 사실은 각 owner에서 읽음 |
| [Input_Process](../30_FUNCTIONS/20_INPUT_STORE_FUNCTIONS.md#input-process) | [VssInputEvidence](10_INPUT_DATA.md#vssinputevidence) 또는 RX 없음 + 시간 | [VssInputApplication](10_INPUT_DATA.md#vssinputapplication) | 검증→typed 검증 입력→STORE; 수용/품질 적용을 구별 |
| [Store_ApplyInput](../30_FUNCTIONS/20_INPUT_STORE_FUNCTIONS.md#store-applyinput) | [VssOneShotEvent](10_INPUT_DATA.md#vssoneshotevent) / [VssStateUpdate](10_INPUT_DATA.md#vssstateupdate) / [VssQualityChange](10_INPUT_DATA.md#vssqualitychange) | [VssStoreAdmission](20_STORE_DATA.md#vssstoreadmission) | 발생/현재 상태/품질 원본이 분리됨. 판정 scalar |
| [Store_ApplyPlaybackFact](../30_FUNCTIONS/20_INPUT_STORE_FUNCTIONS.md#store-applyplaybackfact) | [VssPlaybackFact](40_PLAYBACK_DATA.md#vssplaybackfact) | [VssFactApplication](20_STORE_DATA.md#vssfactapplication) | 원래 fact 하나의 보호된 멱등 적용 |
| [Store_AdvanceDeadlines](../30_FUNCTIONS/20_INPUT_STORE_FUNCTIONS.md#store-advancedeadlines) | [VssTimeEvidence](10_INPUT_DATA.md#vsstimeevidence) + [VssPlaybackObservation](40_PLAYBACK_DATA.md#vssplaybackobservation) | [VssStoreProgress](20_STORE_DATA.md#vssstoreprogress) | 최신 출력·미반영 통지 재확인 후만 만료 |
| [Select_Choose](../30_FUNCTIONS/30_POLICY_PLAYBACK_FUNCTIONS.md#select-choose) | 후보/현재 출력/대상 제한/Asset 확인의 typed 읽기 + [VssReadBasis](30_SELECTION_DATA.md#vssreadbasis) | [VssSelectionDecision](30_SELECTION_DATA.md#vssselectiondecision) | SelectionView는 INLINE; 여러 owner state 복제 없음 |
| [Playback_RequestTransition](../30_FUNCTIONS/30_POLICY_PLAYBACK_FUNCTIONS.md#playback-requesttransition) | 판단 또는 현재 Session/종료 사유 + 최신 작은 읽기 조건 | [VssRequestDisposition](40_PLAYBACK_DATA.md#vssrequestdisposition) | PlaybackIntent/ExecutionConditions는 INLINE; callee 자기 자원/권한 확인 |
| [Playback_Advance](../30_FUNCTIONS/30_POLICY_PLAYBACK_FUNCTIONS.md#playback-advance) | 준비/실제 출력 typed 결과 또는 결과 없음 + 최신 조건/시간 | 현재 출력 관측·통지/적용 enum·진단의 typed 전달 | PlaybackProgress SPLIT. 결과 없는 기한 진행 가능 |
| [Asset_Read](../30_FUNCTIONS/40_AUDIO_STREAM_ASSET_FUNCTIONS.md#asset-read) | [VssAssetReadRequest](50_AUDIO_STREAM_DATA.md#vssassetreadrequest) 또는 span key·실제 참조 종료 근거 | 읽기 판정 enum + 성공 span / 발견 진단 | AssetAccess/ReadResult SPLIT; 자료 참조는 반환 뒤에도 유지 |
| [AudioStream_Prepare](../30_FUNCTIONS/40_AUDIO_STREAM_ASSET_FUNCTIONS.md#audiostream-prepare) | [VssAudioPreparation](50_AUDIO_STREAM_DATA.md#vssaudiopreparation) | 준비 단계 결과 + 필요한 별도 진단 | prepared는 start 허가/actual 아님 |
| [AudioStream_RequestControl](../30_FUNCTIONS/40_AUDIO_STREAM_ASSET_FUNCTIONS.md#audiostream-requestcontrol) | [VssPlaybackControlCommand](50_AUDIO_STREAM_DATA.md#vssplaybackcontrolcommand) 또는 [VssRecoveryPermission](70_DIAGNOSTIC_DATA.md#vssrecoverypermission) + 최신 조건 | 요구 수용 결과 / 정리·복구의 별도 사실 | 원래 미래 PCM 차단부터 반영; 입력 shape의 국소 검토 제안 |
| [AudioStream_Advance](../30_FUNCTIONS/40_AUDIO_STREAM_ASSET_FUNCTIONS.md#audiostream-advance) | 시간 + owner의 보호된 준비/제어 문맥 | 준비 결과/실제 출력/정리 관측/진단·수행·검증의 typed 전달 | BackendResult SPLIT. PCM 상세는 내부 owner 반영 |
| [AudioTx_Request](../30_FUNCTIONS/50_DRIVER_HAL_FUNCTIONS.md#audiotx-request) | [VssPcmHandoffCommand](60_AUDIO_TX_DATA.md#vsspcmhandoffcommand) 또는 [VssTxControlCommand](60_AUDIO_TX_DATA.md#vsstxcontrolcommand) | 수용 결과 / 충분한 무접근 반환 근거 / 진단 | 두 입력의 자료 수명 차이. vendor 호출 전에 원래 귀속 보호 |
| [AudioTx_Advance](../30_FUNCTIONS/50_DRIVER_HAL_FUNCTIONS.md#audiotx-advance) | 시간 + HAL 보호 raw/원래 operation | 수용·출력·소비·안전 반환/진단의 typed 전달 | TxProgress SPLIT. 현재 Attempt 재라벨링 금지 |
| [AudioControl_Service](../30_FUNCTIONS/50_DRIVER_HAL_FUNCTIONS.md#audiocontrol-service) | [VssDeviceControlIntent](60_AUDIO_TX_DATA.md#vssdevicecontrolintent) 또는 새 의도 없음 + 시간 | 장치 readiness 읽기·제어 결과/진단·수행·검증 | 현재 device/config 수명. Session 없는 startup 진행 유지 |
| [AudioHAL_Callback](../30_FUNCTIONS/50_DRIVER_HAL_FUNCTIONS.md#audiohal-callback) | 원래 TX/device 등록 key·보호 기록 + [VssRawObservation](60_AUDIO_TX_DATA.md#vssrawobservation) | void; 보호된 raw 사실·손실/기존 후속 알림 | HAL Boundary owner. 실제 callback ABI·등록 lookup은 미정 |
| [Health_Evaluate](../30_FUNCTIONS/60_HEALTH_RUNTIME_FUNCTIONS.md#health-evaluate) | 원래 진단/수행·별도 검증의 typed 관측 + 읽기 근거 | 현재/최근 Fault 읽기·복구 허용 + 변화 enum | DiagnosticObservations INLINE / HealthAssessment SPLIT |
| [Health_BuildStatus](../30_FUNCTIONS/60_HEALTH_RUNTIME_FUNCTIONS.md#health-buildstatus) | owner별 필요한 반영 완료 typed 관측 + 읽기 근거 | [VssStatusBuildResult](70_DIAGNOSTIC_DATA.md#vssstatusbuildresult) + 생성한 [VssStatusSnapshot](70_DIAGNOSTIC_DATA.md#vssstatussnapshot) | StatusObservations INLINE. 부족이면 새 정상 Snapshot 없음 |

<a id="r2-local-review"></a>
## R2 국소 검토 이력과 독립 PASS 연결

아래 표는 R3 제안 당시 기록을 보존한 것이다. 현재 두 논리 경계는 복구 패키지의 `R3_LOCAL_FUNCTION_REVIEW_PASS_20261008.md`에서 독립 국소 PASS로 종료됐다. 다음 확정은 입력 의미/권한 분리이며 C signature 확정이 아니다.

| 대상 | 이번 Data에서 드러난 강한 근거 | 후속 국소 검토 / 이번에 유지한 것 |
| --- | --- | --- |
| AudioStream_RequestControl | START/STOP은 Session/Attempt Command인데 복구는 현재 Fault instance/target/config/허용 key이며 Session이 없을 수 있다. 같이 넣으면 서로 배타적인 필드/권한이 많아진다. | 두 typed 입력이 같은 현재 owner/실행 경계 안에서 안전하게 수용되는지 검토한다. callback/상태별 wrapper를 추가하지 않는다. R2 책임/함수명/prototype은 그대로다. |
| AudioTx_Request | PCM 인계는 안정된 samples·유효 범위/회차 보호가 필요하고 start/stop은 출력 범위 제어다. 하나의 Command struct로 만들면 주소/구간이 제어 입력에 불필요하게 들어간다. | 공통 인과 전송 수명은 유지하면서 두 typed 입력의 수용 표현을 검토한다. START/STOP은 공통 shape로 남겼고 함수 분해는 수행하지 않았다. |

새 함수나 C signature 확정안은 작성하지 않았다. 다른 SPLIT 결과군은 R2가 이미 요구한 서로 다른 사실/수명 전달을 구체화한 것으로 Function 책임 재설계가 필요한 근거는 없었다. 최종 C 반환을 위한 typed 자료 전달/consumer 반영 완료 표현은 R6/Binding에서 정하고, 이를 큰 Result union으로 해결하지 않는다.

| 독립 국소 PASS 대상 | R4에서 적용한 논리 결정 | 남은 결정 |
| --- | --- | --- |
| [AudioStream_RequestControl](../30_FUNCTIONS/40_AUDIO_STREAM_ASSET_FUNCTIONS.md#audiostream-requestcontrol) | 논리 Core 1개 유지. PLAYBACK typed START/STOP와 HEALTH→FLOW typed Recovery의 identity/권한/수명 분리; 가짜 Session 없음 | 실제 C 입력/반환·진입점/원형은 R6, recovery vendor 충분조건은 B2-R |
| [AudioTx_Request](../30_FUNCTIONS/50_DRIVER_HAL_FUNCTIONS.md#audiotx-request) | 논리 Core 1개 유지. typed PCM 인계와 typed START/STOP 분리; 제어에 PCM 주소 없음; 지속 스트림≠개별 operation | 실제 C 표현은 R6, callback/회차/출력·safe return 충분조건은 B2-R |

`R2_LOCAL_REVIEW_NEEDED` 두 논리 경계는 위 독립 PASS로 종료됐다. R2 provisional 표/prototype과 R3 32개 판정·18개 함수 대응표는 이력/논리 근거로 원문 보존한다. [Recovery Contract](../50_CONTRACTS/40_FAULT_RECOVERY.md#current-target)와 [TX typed Contract](../50_CONTRACTS/50_AUDIO_BUFFER_TRANSPORT.md#tx-typed-lanes)를 따르며 실제 API와 물리 안전성은 미확정이다.

<a id="pseudo-conventions"></a>
## Pseudo-C 표기와 미정 범위

사람이 읽는 Field 표가 의미의 기준이고 뒤의 enum/typedef/struct는 그 표현이다. `SemanticIdentity/Order/Revision/Position`은 **폭/encoding이 미정인 의미 scalar**, `SemanticTimePoint/Duration`는 시간/기간 scalar, `SemanticCount/ByteCount/FrameCount`는 서로 단위가 다른 개수, `SemanticBool`은 의미 참/거짓 표기다. `SemanticOrigin/Owner/Descriptor/PolicyCondition/Format/Boundary/Stage/Cause/Action/ContractValue`도 제한된 기존 의미의 불투명 scalar다. 실제 typedef 기반 C primitive/숫자 enum/bit layout을 지정하는 표기가 아니다.

`PcmSamplesRef`, `CompressedBytesRef`, `RawFactValue`는 중립 PCM·압축 bytes 참조/해석 전 사실의 표기이며 실제 주소/포인터형·vendor type은 미정이다. `const T *`는 읽기 borrow 또는 독립 보호된 불변 자료 참조다. 단순 임시 인자 주소의 장기 저장을 허용하지 않는다. 필드에 '없음'이 허용된 경우만 logical null 의미를 쓰며 실제 C optional/복사/참조 표현은 후속에서 정한다. pointer+Count 목록은 typed 논리 자료의 범위를 보이는 표기이고 Queue/array 용량/할당 방식의 구현 선택이 아니다.

Data 상세의 owner header 후보는 **논리 의미 소유 그룹**이다. 실제 Header filename·public/internal·include 방향·기존 Header 재사용/신규 Header 필요 여부는 R6에서 확정한다. alias 이름 하나당 Header/struct/ID 생성 API를 만들지 않는다.

모든 상세에서 미정으로 유지한 항목: 실제 field width/enum 숫자·alignment/packing/ABI·메모리 배치·용량, 실제 `.c/.h`/RTD·DMA·TCD, Task/Queue/Mutex, callback/자료 복사·참조 보존 수단, 물리 actual/no-output/전체 종료 충분조건, decoder/PCM format·image/Asset table, exact cause/Recovery Action·timeout/retry·Hold/패턴/초기 Availability·전체 status reduction. [기존 TBD](../90_BINDING/91_IMPLEMENTATION_TBD.md)는 보존했다.

## 보존한 Semantic Lock과 다음 범위

Occurrence/Session/Attempt·single writer, winner ≠ prepared ≠ start accepted ≠ actual start, OUTPUT_START_CONFIRMED / NO_START_CONFIRMED / OUTPUT_TERMINATION_CONFIRMED, late/uncertain/QUARANTINED, 정확히 PCM A/B, source consumed ≠ safe return ≠ output termination ≠ retirement, 기존 Fault 5종·복구 수행 후 별도 검증·현재 대상 해제, WINDOW [구현 보류 — 설계 유지]를 유지한다. 원본 first-start age < 2초는 [잠정]이며 정확히 2초 불가, USE_LIMIT 별도 조건·Hold 기산/재실행 방어는 바꾸지 않았다.

R3 Data 상세/선언을 기준으로 R4에서 [공통 Contract](../50_CONTRACTS/00_CONTRACT_OVERVIEW.md)와 필요한 local 링크만 정리했다. [참고 의사코드](../60_PSEUDOCODE/00_PSEUDOCODE_OVERVIEW.md)·R6 skeleton·Binding은 원문 그대로다. R4 자체 검수 뒤 중단하고 독립 PASS 전 R5 진행은 금지한다. R5/R6/R7/B2-R·C 구현은 수행하지 않았다.
