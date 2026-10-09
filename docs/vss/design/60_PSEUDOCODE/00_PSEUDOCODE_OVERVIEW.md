# Pseudocode Overview — R5 typed 실행 흐름

> 2026-10-08 · R4 독립 PASS 기준 · R5 문서 자체 검수 결과는 결과 Summary에서 확인한다. 독립 PASS 전 R6 진입 금지.

R4의 51개 문서에서 18 Core의 실행 경로를 아래 여섯 Flow로 재작성했다. R2 계약·R3 실제 타입/필드·R4 공통 규칙 및 Module 예외를 그대로 따른다. 세부 상태 전이는 해당 writer의 Core 내부에만 둔다. 과거 의미 참고본은 수정하지 않았다.

| Flow | 목적·실행 경계 |
| --- | --- |
| [10_STARTUP_FLOW](10_STARTUP_FLOW.md) | Session 없는 초기 진행, device별 readiness, 최초 평가/보고 |
| [20_INPUT_TO_SELECTION](20_INPUT_TO_SELECTION.md) | INPUT 적용→STORE 수용/기한→반영 완료 읽기→POLICY 선택 |
| [30_PLAYBACK_FLOW](30_PLAYBACK_FLOW.md) | 원 Session/Attempt 채택, 준비·START·actual, 정상 cue/반복 |
| [40_AUDIO_PREPARE_TX_FLOW](40_AUDIO_PREPARE_TX_FLOW.md) | ASSET/read refs, bounded decode, A/B, typed TX, 원 raw/evidence |
| [50_STOP_LATE_UNCERTAIN_FLOW](50_STOP_LATE_UNCERTAIN_FLOW.md) | 미래 차단, no-start/termination/retire, late/uncertain, 독립 safe return/통지 |
| [60_FAULT_RECOVERY_FLOW](60_FAULT_RECOVERY_FLOW.md) | 기존 Fault 5종, 현재 허용·실제 수행·별도 검증·HEALTH 해제, 파생 보고 |

<a id="notation"></a>
## 표기 규칙 — C 작성 직전의 논리 절차

- `CORE`/`CALL`은 기존 18개의 **논리 Core** 경계다. typed input/output은 R3에서 SPLIT한 자료를 각각 전달하는 의미이며 signature, overload, ABI, Header 또는 새 union을 확정하지 않는다. 선언/필드의 실제 이름은 R3 그대로다.
- `STEP`, `LOCAL CONDITION`, `LOCAL STEP`은 표시한 owner의 내부 처리다. 한국어 조건과 처리문은 필요한 근거/보호/부수효력/실패를 기술하며 외부 helper API를 신설하지 않는다. `AudioStream_FillBuffer`는 STREAM 내부 처리다. R2의 과거 provisional wrapper 타입명을 실제 R3 struct로 되살리지 않는다.
- `own`은 해당 Core의 원본 상태에만 붙인다. 타 owner 자료는 적용 완료 읽기 관측이다. field 대입은 해당 writer 내부 또는 그 Core가 만든 새 command/result에만 허용한다. code의 점 표기는 논리 접근이며 C의 `.`/`->`나 pointer ABI 확정이 아니다. opaque alias에 가상의 subfield를 만들지 않는다.
- `보호 참조`, `독립 보존`은 실제 consumer 수명까지 안정된 귀속/자료를 확보하는 의무다. 복사/borrow/고정 저장의 실물 형태·용량·직렬화는 TBD다. 임시 command 포인터 보관이나 모든 결과를 마지막 슬롯 하나로 덮는 구현은 허용하지 않는다.
- 조건은 **확인됨 / 확인된 불충족 / 근거 부족·손실**을 구별한다. 충분성/TBD 미확인은 true로 가정하지 않는다. 코드의 충분성 branch에 들어갈 수 없으면 원 보호·차단·진단·후속 필요성을 유지한다. 현재 무음/timeout/idle은 실제 물리 충분조건이 아니다.
- `return 거부/후속`처럼 쓰인 부분은 동작 의미를 먼저 표시한다. 특히 TX operation 생성 **전** 부적합/공간 부족 거부는 가짜 operation/Attempt를 넣은 `VssTxRequestResult`를 만들지 않는다. 기존 scalar `VssTxRequestState`의 무효력 거부 의미·진단과 operation 생성 뒤 typed result를 구별하며, 이 early-return의 실제 C 표현은 `[TBD-RETURN]` R6 사항이다.
- `now`는 Runtime의 `VssTimeEvidence`, `occurredAt`은 원 사실의 `VssFactTime`이다. 구간/연속성 부족을 정밀 시각으로 합성하지 않는다. 별도 설명 없는 시간/조건은 각 Flow 입력의 현재 읽기 근거다.

실제 숫자/시각 변환·배열 storage·key allocation·plan position·vendor symbol을 이 의사코드로 확정하지 않는다. 필요한 의미가 미정인 경우 branch와 TBD를 함께 남겨 구현자가 임의 성공을 만들지 않게 한다. 여섯 Flow의 서로 연결되는 slice를 한 번씩 읽고, 같은 Core의 예외는 링크를 따라간다.

<a id="core-coverage"></a>
## 18 Core ↔ R3 ↔ R4 ↔ R5 추적

| No. / R2 Core 원문 | R3 자료 진입 | R4 공통 규칙 | R5 주 실행 위치 |
| --- | --- | --- | --- |
| 1. [Flow_Process](../30_FUNCTIONS/10_FLOW_FUNCTIONS.md#flow-process) | [VssProcessingOpportunity](../40_DATA/10_INPUT_DATA.md#vssprocessingopportunity) | [공통 근거](../50_CONTRACTS/30_TIMING_FRESHNESS.md#coherent-read) | [20_INPUT_TO_SELECTION](20_INPUT_TO_SELECTION.md#flow-process) |
| 2. [Input_Process](../30_FUNCTIONS/20_INPUT_STORE_FUNCTIONS.md#input-process) | [VssInputEvidence](../40_DATA/10_INPUT_DATA.md#vssinputevidence) | [공통 근거](../50_CONTRACTS/30_TIMING_FRESHNESS.md#time-context) | [20_INPUT_TO_SELECTION](20_INPUT_TO_SELECTION.md#input-process) |
| 3. [Store_ApplyInput](../30_FUNCTIONS/20_INPUT_STORE_FUNCTIONS.md#store-applyinput) | [VssOneShotEvent](../40_DATA/10_INPUT_DATA.md#vssoneshotevent) | [공통 근거](../50_CONTRACTS/10_OWNERSHIP_LIFETIME.md#borrow-and-protect) | [20_INPUT_TO_SELECTION](20_INPUT_TO_SELECTION.md#store-applyinput) |
| 4. [Store_ApplyPlaybackFact](../30_FUNCTIONS/20_INPUT_STORE_FUNCTIONS.md#store-applyplaybackfact) | [VssPlaybackFact](../40_DATA/40_PLAYBACK_DATA.md#vssplaybackfact) | [공통 근거](../50_CONTRACTS/10_OWNERSHIP_LIFETIME.md#borrow-and-protect) | [50_STOP_LATE_UNCERTAIN_FLOW](50_STOP_LATE_UNCERTAIN_FLOW.md#store-applyplaybackfact) |
| 5. [Store_AdvanceDeadlines](../30_FUNCTIONS/20_INPUT_STORE_FUNCTIONS.md#store-advancedeadlines) | [VssTimeEvidence](../40_DATA/10_INPUT_DATA.md#vsstimeevidence) | [공통 근거](../50_CONTRACTS/30_TIMING_FRESHNESS.md#source-age) | [20_INPUT_TO_SELECTION](20_INPUT_TO_SELECTION.md#store-advancedeadlines) |
| 6. [Select_Choose](../30_FUNCTIONS/30_POLICY_PLAYBACK_FUNCTIONS.md#select-choose) | [VssCandidateObservation](../40_DATA/30_SELECTION_DATA.md#vsscandidateobservation) | [공통 근거](../50_CONTRACTS/30_TIMING_FRESHNESS.md#coherent-read) | [20_INPUT_TO_SELECTION](20_INPUT_TO_SELECTION.md#select-choose) |
| 7. [Playback_RequestTransition](../30_FUNCTIONS/30_POLICY_PLAYBACK_FUNCTIONS.md#playback-requesttransition) | [VssSelectionDecision](../40_DATA/30_SELECTION_DATA.md#vssselectiondecision) | [공통 근거](../50_CONTRACTS/10_OWNERSHIP_LIFETIME.md#single-writer) | [30_PLAYBACK_FLOW](30_PLAYBACK_FLOW.md#playback-requesttransition) |
| 8. [Playback_Advance](../30_FUNCTIONS/30_POLICY_PLAYBACK_FUNCTIONS.md#playback-advance) | [VssAudioRequestResult](../40_DATA/50_AUDIO_STREAM_DATA.md#vssaudiorequestresult) | [공통 근거](../50_CONTRACTS/20_ASYNC_RESULT_EVIDENCE.md#confirmed-outcomes) | [30_PLAYBACK_FLOW](30_PLAYBACK_FLOW.md#playback-advance) |
| 9. [Asset_Read](../30_FUNCTIONS/40_AUDIO_STREAM_ASSET_FUNCTIONS.md#asset-read) | [VssAssetReadRequest](../40_DATA/50_AUDIO_STREAM_DATA.md#vssassetreadrequest) | [공통 근거](../50_CONTRACTS/10_OWNERSHIP_LIFETIME.md#references-and-release) | [40_AUDIO_PREPARE_TX_FLOW](40_AUDIO_PREPARE_TX_FLOW.md#asset-read) |
| 10. [AudioStream_Prepare](../30_FUNCTIONS/40_AUDIO_STREAM_ASSET_FUNCTIONS.md#audiostream-prepare) | [VssAudioPreparation](../40_DATA/50_AUDIO_STREAM_DATA.md#vssaudiopreparation) | [공통 근거](../50_CONTRACTS/50_AUDIO_BUFFER_TRANSPORT.md#access-cycle) | [40_AUDIO_PREPARE_TX_FLOW](40_AUDIO_PREPARE_TX_FLOW.md#audiostream-prepare) |
| 11. [AudioStream_RequestControl](../30_FUNCTIONS/40_AUDIO_STREAM_ASSET_FUNCTIONS.md#audiostream-requestcontrol) | [VssPlaybackControlCommand](../40_DATA/50_AUDIO_STREAM_DATA.md#vssplaybackcontrolcommand) | [공통 근거](../50_CONTRACTS/40_FAULT_RECOVERY.md#current-target) | [40_AUDIO_PREPARE_TX_FLOW](40_AUDIO_PREPARE_TX_FLOW.md#audiostream-requestcontrol) |
| 12. [AudioStream_Advance](../30_FUNCTIONS/40_AUDIO_STREAM_ASSET_FUNCTIONS.md#audiostream-advance) | [VssTimeEvidence](../40_DATA/10_INPUT_DATA.md#vsstimeevidence) | [공통 근거](../50_CONTRACTS/50_AUDIO_BUFFER_TRANSPORT.md#four-boundaries) | [40_AUDIO_PREPARE_TX_FLOW](40_AUDIO_PREPARE_TX_FLOW.md#audiostream-advance) |
| 13. [AudioTx_Request](../30_FUNCTIONS/50_DRIVER_HAL_FUNCTIONS.md#audiotx-request) | [VssPcmHandoffCommand](../40_DATA/60_AUDIO_TX_DATA.md#vsspcmhandoffcommand) | [공통 근거](../50_CONTRACTS/50_AUDIO_BUFFER_TRANSPORT.md#tx-typed-lanes) | [40_AUDIO_PREPARE_TX_FLOW](40_AUDIO_PREPARE_TX_FLOW.md#audiotx-request) |
| 14. [AudioTx_Advance](../30_FUNCTIONS/50_DRIVER_HAL_FUNCTIONS.md#audiotx-advance) | [VssRawObservation](../40_DATA/60_AUDIO_TX_DATA.md#vssrawobservation) | [공통 근거](../50_CONTRACTS/20_ASYNC_RESULT_EVIDENCE.md#evidence-basis) | [40_AUDIO_PREPARE_TX_FLOW](40_AUDIO_PREPARE_TX_FLOW.md#audiotx-advance) |
| 15. [AudioControl_Service](../30_FUNCTIONS/50_DRIVER_HAL_FUNCTIONS.md#audiocontrol-service) | [VssDeviceControlIntent](../40_DATA/60_AUDIO_TX_DATA.md#vssdevicecontrolintent) | [공통 근거](../50_CONTRACTS/40_FAULT_RECOVERY.md#recovery-flow) | [10_STARTUP_FLOW](10_STARTUP_FLOW.md#audiocontrol-service) |
| 16. [AudioHAL_Callback](../30_FUNCTIONS/50_DRIVER_HAL_FUNCTIONS.md#audiohal-callback) | [VssHalTxRegistration](../40_DATA/60_AUDIO_TX_DATA.md#vsshaltxregistration) | [공통 근거](../50_CONTRACTS/50_AUDIO_BUFFER_TRANSPORT.md#continuous-stream) | [40_AUDIO_PREPARE_TX_FLOW](40_AUDIO_PREPARE_TX_FLOW.md#audiohal-callback) |
| 17. [Health_Evaluate](../30_FUNCTIONS/60_HEALTH_RUNTIME_FUNCTIONS.md#health-evaluate) | [VssDiagnosticEvidence](../40_DATA/70_DIAGNOSTIC_DATA.md#vssdiagnosticevidence) | [공통 근거](../50_CONTRACTS/40_FAULT_RECOVERY.md#recovery-flow) | [60_FAULT_RECOVERY_FLOW](60_FAULT_RECOVERY_FLOW.md#health-evaluate) |
| 18. [Health_BuildStatus](../30_FUNCTIONS/60_HEALTH_RUNTIME_FUNCTIONS.md#health-buildstatus) | [VssReadBasis](../40_DATA/30_SELECTION_DATA.md#vssreadbasis) | [공통 근거](../50_CONTRACTS/40_FAULT_RECOVERY.md#report-and-tbd) | [60_FAULT_RECOVERY_FLOW](60_FAULT_RECOVERY_FLOW.md#health-buildstatus) |

협업 slice: [FLOW Startup](10_STARTUP_FLOW.md#flow-startup) · [PB 종료](50_STOP_LATE_UNCERTAIN_FLOW.md#playback-stop-late) · [STORE 통지](50_STOP_LATE_UNCERTAIN_FLOW.md#fact-notification) · [STREAM STOP](50_STOP_LATE_UNCERTAIN_FLOW.md#stream-stop) · [TX 반환](50_STOP_LATE_UNCERTAIN_FLOW.md#tx-safe-return) · [STREAM 현재 쓰기 검사](50_STOP_LATE_UNCERTAIN_FLOW.md#stream-safe-return) · [STREAM Recovery](60_FAULT_RECOVERY_FLOW.md#stream-recovery) · [CONTROL 별도 검증](60_FAULT_RECOVERY_FLOW.md#device-recovery).

R2 상세→R3의 관련 Function→이 표의 Core 위치→R4 규칙 및 Module local 원문을 왕복해서 확인한다. 실제 18 Core의 Owner/Caller/Callee/Reads/Writes와 모든 typed 자료·실패/예외 커버리지는 결과 ZIP의 Summary에 기록한다. 동일 Core가 여러 slice에 보여도 Core를 늘린 것이 아니다.

## 남은 TBD와 단계 게이트

`TBD-INIT`, `TBD-IDENTITY/TIME`, `TBD-HOLD`, `TBD-WRITE`, `TBD-CAPACITY`, `TBD-FINAL`, `TBD-RECOVERY`, `TBD-RETURN`은 기존 미정을 추적하기 위한 **문서 표기**다. 새로운 데이터 타입·상태·정책값이 아니다. 실제 C optional/return/borrow 형태는 R6, 지속 DMA/등록 상관과 출력/접근 충분성은 B2-R/보드, 정책값·매핑/reduction은 기존 TBD다. 물리 근거 미입증은 R5 문서 자체 검수로 해소되지 않는다.

[기존 81 의미 참고본 — 불변](reference/81_VSS_PSEUDOCODE.md) · [Function 계약](../30_FUNCTIONS/00_FUNCTION_OVERVIEW.md) · [R3 전수 판정](../40_DATA/00_DATA_OVERVIEW.md#type-decisions) · [다섯 R4 Contract](../50_CONTRACTS/00_CONTRACT_OVERVIEW.md) · [Module local](../20_MODULES/00_MODULE_OVERVIEW.md) · [Header 미확정](../70_C_INTERFACE/00_HEADER_OWNERSHIP_MAP.md) · [Implementation TBD](../90_BINDING/91_IMPLEMENTATION_TBD.md) · [현재 게이트](../../../00_README_CURRENT_PACKAGE.md).

R5 자체 검수는 독립 PASS가 아니다. R5 결과 ZIP을 일반 채팅에 전달한 뒤 중단한다. R5 독립 PASS 전 R6/R7/B2-R·실제 C 구현을 진행하지 않는다. 보존 문서의 과거 R0~R4 작업 단계 표기는 이력이며 현재 Stage/게이트는 README가 기준이다.
