# AudioStream_RequestControl — 상세 동작 설명

[VSS 지도](../../../01_VSS_MAP.md) / [SERVICE](../../00_LAYER_OVERVIEW.md) / [AUDIO_STREAM](../00_MODULE_OVERVIEW.md) / CORE-11 · [한국어 C 의사코드](../pseudocode/AudioStream_RequestControl.md)

> GUIDE · LOGICAL R6 CONTRACT · 실제 C ABI/동기화 수단 BINDING_TBD.

PLAYBACK의 START·STOP 의도 또는 HEALTH가 허용한 Session 없는 Recovery를 수용한다. AUDIO_STREAM은 자기 provider·PCM 생산 권한과 원 제어 연결만 변경하며, PLAYBACK의 Session 판정·TX operation 원본·CONTROL readiness·HEALTH Fault를 직접 쓰지 않는다.

<a id="contract"></a>
## 원형과 동기 반환

```text
VssRequestDisposition AudioStream_RequestControl(
    ONE_OF const VssPlaybackControlCommand *playback,
           const VssRecoveryPermission *recovery,
    IN const VssPlaybackObservation *currentPlayback,
    IN const VssCandidateObservation candidates[count],
    IN const VssFaultState restrictions[count],
    IN const VssDeviceReadinessState devices[count],
    IN const VssBackendCleanupObservation cleanup[count],
    IN const VssReadBasis *conditionsBasis,
    OUT VssAudioRequestResult requests[capacity -> written],
    OUT VssDiagnosticEvidence diagnostics[capacity -> written]);
```

[최신 R6 원형](../../../../design/70_C_INTERFACE/00_HEADER_OWNERSHIP_MAP.md#r6-audiostream-requestcontrol)의 `ONE_OF`는 두 포인터 중 정확히 하나다. count/capacity/written은 [R6 논리 전달 표기](../../../../design/70_C_INTERFACE/00_HEADER_OWNERSHIP_MAP.md#prototype-notation)이며 실제 Header·배치·entry point 수를 확정하지 않는다.

`VssRequestDisposition`은 `VSS_REQUEST_ADOPTED`(owner가 의도 수용/반영), `VSS_REQUEST_KEPT`(동일 원 진행/차단 유지), `VSS_REQUEST_WAIT`(최신 조건/자원/정리 때문에 대기), `VSS_REQUEST_REJECTED`(새 의도 부적합)이다. 이 문서의 반환 분기는 이 기존 의미를 설명하며 제품의 새 정책표가 아니다. 수용·유지는 준비 완료·하위 수락/활성화·실제 출력·실제 STOP·복구 효과를 증명하지 않는다.

| 인자 | 의미 / 실제 사용하는 분기 | 유효성·수명 |
|---|---|---|
| `playback` | `VssPlaybackControlCommand`의 원 `session/attempt/action`; START 또는 STOP | PLAYBACK 불변 command. 외피/원 귀속 확인 후 읽고 필요한 의미를 STREAM에 보호. 임시 포인터 장기 저장 금지 |
| `recovery` | `VssRecoveryPermission`의 `permission/faultInstance/faultRevision/target/configuration/action` | HEALTH→FLOW의 현재 허용. PB Session/Attempt가 아니며 철회·대상/구성 변경 시 새 수행 불가 |
| `currentPlayback` | 적용 완료 Session/Attempt·권한/최초 Started·정리 관측 | START의 원 준비·현재 허용 대조. Session 없는 Recovery에서는 실제 absence를 유지하며 정상 PB 관측을 만들지 않음 |
| `candidates[count]` | 원 후보/plan/Meta·품질/실행 조건 | 각 범위와 내부 참조를 확인; START 조건에 필요한 원 후보만 대조. Recovery에 재생 후보를 강제하지 않음 |
| `restrictions[count]` | HEALTH의 현재 Fault/제한 및 허용 대상 판단 근거 | 현재 instance/revision/target/config와 함께 확인. STREAM에서 제한 해제 금지 |
| `devices[count]` | CONTROL이 적용한 device/config readiness | 현재 구성만 조건에 사용. 과거 READY를 새 구성에 적용하지 않음 |
| `cleanup[count]` | 원 Attempt의 provider/PCM/lower output/late 보호 관측 | 관련 old 정리 안전을 각각 확인; 하나의 true를 전체 retirement로 쓰지 않음 |
| `conditionsBasis` | owner stamps·원 시간·validity의 평가 근거 | 내부 stamps/count/time도 borrow 검증. START/Recovery 효과 직전 실제 최신 권한·자원 조건을 별도 재확인 |
| `requests[capacity -> written]` | 원 Session/Attempt의 `VssAudioRequestResult` 단계들 | 재생 lane에서 유효 귀속일 때만. Recovery에서는 유효 군 `written=0` |
| `diagnostics[capacity -> written]` | 원 producer/stage/cause/target/config/operation/시각·coverage의 `VssDiagnosticEvidence` | requests와 독립. 전달 공간 부족/무효에도 원 진단·참조/의무 보존 |

<a id="validation"></a>
## 입력 검사와 효력 전 보호

1. 두 OUT 군을 각각 검사한다. 주소·전달 영역·capacity/written 저장소·산술 범위·군 간 비간섭을 안전하게 확인하기 전 capacity/written 역참조나 초기화를 하지 않는다. 유효 군만 `written=0`으로 시작한다.
2. `playback/recovery`가 둘 다 없거나 둘 다 있으면 하위 호출·새 owner 효력 전에 REJECTED다. 선택 lane의 외피, 필수 원 identity, optional borrow 및 count>0인 각 배열의 실제 범위를 읽기 전에 확인한다. count=0은 자료 없음이며 가짜 정상 관측으로 채우지 않는다.
3. 실제 필요한 참조는 효력 전에 원 수명으로 보호한다. 새 START/Recovery의 결과·operation 연결 보호가 부족하면 WAIT로 남기고 새 하위 동작을 발행하지 않는다. OUT capacity=0과 내부 보호 공간 부족을 구분한다. 실제 사실 손실은 coverage LOST/진단으로 드러내고 성공으로 숨기지 않는다.
4. STOP의 유효 원 Session 의도가 확인되면 기존 제어 문맥에 먼저 차단을 반영한다. 오래된 basis·장치 미확인·하위 결과 공간 부족은 **새 START/복구/하위 정리 연결**을 제한하며 이미 확인한 STOP 차단을 뒤집는 근거가 아니다. STOP 차단의 기존 보호 수단/직렬화는 실제 C 결합에서 입증한다.

<a id="start"></a>
## START — 준비 귀속, 최초 조건, 주소 없는 TX 제어

원 command의 Session/Attempt가 보호된 PREPARED provider 및 원 preparation의 plan/position/asset와 일치해야 한다. 다른 Session/Attempt·asset/config로 현재 provider를 덮지 않는다. 준비 전, CLOSING/취소/STOP 차단, 제한 또는 현재 장치/config 불충분이면 새 START를 발행하지 않는다. 부적합한 새 요구는 REJECTED, 미확인 조건/자원은 WAIT이며 유효 원 귀속이 있을 때만 해당 단계 사실을 보호한다.

미시작 One-shot의 최초 시도는 preparation의 원 `firstStartMeta`를 보호해 TX에 연결한다. 이미 적법 Started된 같은 Session의 정상 후속 cue에는 `firstStartMeta=NULL`이고 최초 age gate를 재적용하지 않는다. 원발생→최초 actual `<2초 [잠정]` 적법성은 PLAYBACK 소유다. 처리 now로 원 age를 재기산하거나 Network 500ms 제안과 min/AND를 만들지 않는다.

원 요구와 실제 scope·필요한 Meta·operation 결과 보존을 먼저 확보하고 최신 권한/자원을 효과 직전에 재확인한다. `VssTxControlCommand`의 **기존 4필드** `attempt/action/scope/firstStartMeta`를 `action=VSS_TX_START`로 전달한다. PCM 주소 없는 control lane이며 준비된 PCM의 새 handoff를 START 대신 만들지 않는다. 새 START의 `targetOperations`는 count=0, `continuingOperation=NULL`이다. 이미 제출된 동일 **원 요구**는 확인된 원 operation으로만 이어가며, key 전달/수용이 미확인인 경우 새 동작을 재발행하지 않는다. Attempt/action/scope tuple만으로 서로 다른 구간을 동일 operation으로 판단하지 않는다.

하위 `AudioTx_Request`의 동기 state와 독립 typed OUT을 함께 검토한다. operation 생성 전의 확인된 무효력 거부에는 가짜 TX operation/result/PCM return을 만들지 않는다. operation 생성 뒤 원 facts의 상태에 따라 REQUEST_REJECTED / START_SUBMITTED / START_ACCEPTED_AND_ENABLED / EFFECT_UNCERTAIN 또는 아직 제출 미확인인 CONTROL_RECEIVED를 보호한다. SUBMITTED는 수락/활성화가 아니며 ACCEPTED_AND_ENABLED도 실제 출력이 아니다. 동시에 확인된 단계들을 한 lastResult로 덮지 않는다. 부분 효력·retain 가능성은 원 작업/참조/정리를 유지하고 일반 거부로 rollback하지 않는다.

<a id="stop"></a>
## STOP — Session 전체의 선차단과 원 대상별 정리

`stop.attempt`는 인과 anchor이며 정리 대상을 그 하나로 제한하지 않는다. 유효한 `stop.session`의 기존 STOP 의도를 먼저 보호해 **아직 bounded 정리에서 방문하지 못한 관련 provider/Attempt도** 미래 decode/refill/새 sound handoff를 금지한다. 해당 provider들은 `productionAllowed=false`, `phase=CLOSING`을 유지한다. 진행 중 CPU 생산도 실제 인계 직전에 최신 차단을 재확인한다. 다른 현재 Session은 변경하지 않는다.

STREAM이 이미 보호한 prepare/control/handoff와 공개된 TX 관측으로 Session의 관련 Attempt·active/대기 operation·구간·descriptor/FIFO/frame 잔류 대응을 대조한다. `VssOutputScope`의 SEGMENT/ATTEMPT enum 자체는 구간 identity가 아니다. 같은 Attempt의 여러 SEGMENT는 각 원 operation/실제 구간 대응이 필요하다. 새 SegmentKey/Queue/공동 mutable ledger를 만들지 않는다.

bounded 방문에서 귀속과 실제 대상 key가 충분한 각 원 범위에만 `VssTxControlCommand(action=VSS_TX_STOP, firstStartMeta=NULL)`과 관련 `targetOperations`를 전달한다. 새 STOP operation과 그것이 정리할 old 대상 operations를 구분한다. 미확인 key를 current로 채우거나 SEGMENT를 ATTEMPT/whole로 확대하지 않는다. 관련 대상 일부의 귀속/coverage 부족, 예산·보호 공간 한계 또는 하위 실패/partial/지연은 원 retain·차단·진단·다음 Advance 정리 의무로 남긴다.

한 STOP ACK·control operation 종료·mute/abort/idle로 모든 대상 출력 종료, NO_START, PCM 반환 또는 Session retirement를 생성하지 않는다. 안전 반환된 옛 PCM도 정상 refill/제출을 재허가하지 않는다. 정리용 zero-fill은 기존 A/B에서 현재 쓰기 안전이 입증된 범위만 가능하며 미확인 시 쓰지 않는다. STOP 자체로 PREPARE_FAILED/Fault를 합성하지 않고 실제 producer 실패가 확인되면 별도 발견 근거로 처리한다. 정상 차단과 실제 실패의 구분은 [승인 Prepare 의사코드](../pseudocode/AudioStream_Prepare.md#procedure)를 유지한다.

<a id="recovery"></a>
## Recovery — Session 없는 현재 허용과 중복 방지

현재 HEALTH 허용의 6필드를 기존 제한·target/config와 대조한다. 철회·old instance/revision·다른 target/config/action이면 새 수행을 거부하고 원 정리/진단만 유지한다. PB Session이 없다고 command/Session/Attempt를 새로 만들지 않는다. old 전체 retirement와 실제 provider/read·PCM·device 위험 종료, 남은 불변 late 기록의 alias 없는 보호를 각각 확인한다. 부족하면 그 제품 수행만 WAIT다.

동일 permission의 실제 원 수행이 이미 제출/진행/완료됐으면 기존 operation과 별도 검증을 이어간다(KEPT). 동일 허용을 새 복구 명령으로 중복 발행하지 않는다. 원 허용과 operation/결과의 후속 수명을 먼저 보호하고, 수행 직전에 현재 revision/target/config/action·자원 안전을 다시 확인한다.

기존 허용 action의 Driver 매핑·효과 충분조건이 미정이면 그 동작은 BINDING_TBD로 대기한다. 확인된 장치 경계에는 기존 `VssDeviceControlIntent`의 원 operation·device/config·`VSS_DEVICE_RECOVER`·`recoveryPermission`을 보호해 `AudioControl_Service`로 연결한다. 다른 대상도 기존 승인 Backend/Driver 경계만 사용하고 새 reset/action matrix나 public API를 만들지 않는다.

이 함수에서는 requests가 유효해도 `requests.written=0`이다. diagnostics는 독립 전달한다. 수용을 performed/verified 성공으로 만들지 않고 [AudioStream_Advance](AudioStream_Advance.md#recovery)의 `VssRecoveryPerformedEvidence`와 `VssRecoveryVerificationEvidence`에 원 수행 연결을 넘긴다. 수행 성공만으로 CONTROL/STREAM이 HEALTH Fault를 clear하지 않는다.

<a id="publish"></a>
## 단계·진단의 독립 전달과 후속 수명

재생 lane의 requests는 실제 원 Session/Attempt/stage만 기록하며 실제 쓰고 전달 후 보호가 가능한 항목만 written을 증가시킨다. diagnostics는 별도의 capacity/written을 사용한다. 한 군이 invalid/full/0이어도 다른 유효 군의 인계를 무조건 차단하지 않는다. 미인계 원 facts·Meta·permission·진단 내부 참조는 producer가 소비자 반영 또는 독립 보호된 추적 종료까지 유지한다. 호출 인자의 수명 종료나 얕은 OUT 복사는 원 자료의 사용 종료가 아니다. 보호 공간이 부족하면 새 효과를 막거나 실제 손실을 관측 가능하게 남기며 사실을 무음 삭제하지 않는다.

후속은 기존 FLOW 처리 기회와 AudioStream_Advance에 연결된다. 동기 ADOPTED/KEPT·requests count>0이 전체 작업 완료를 뜻하지 않는다. STOP 정리·old 결과·복구 수행/검증은 각 원 수명으로 진행한다.

<a id="gates"></a>
## 구현 후보와 미정

ONE_OF/OUT bounds·원 귀속·중복·STOP 선차단/전체 범위·permission 6필드/철회·요청 단계 분리는 독립 SW/Host fixture 후보다. 실제 Host/Firmware/Build/Flash/Board 실행은 NOT_RUN이다. C ABI·보호 저장 용량/직렬화, 실제 TX START/STOP scope fence·SAI/eDMA/Codec 물리 충분성, 현재 DMA next-visit/WCET, source/image/decoder format·Recovery Driver map/검증 방법은 BINDING_TBD/HW_UNVERIFIED다. P2 HOLD를 유지하며 미정 영역을 이유로 독립 논리 전체를 HOLD하지 않는다.

## 호출 관계와 정본

Caller는 [Playback_RequestTransition](../../PLAYBACK/functions/Playback_RequestTransition.md), [Playback_Advance](../../PLAYBACK/functions/Playback_Advance.md) 및 HEALTH 허용을 조율하는 [Flow_Process](../../../01_APP/FLOW/functions/Flow_Process.md)다. Callee의 상세는 기존 [AudioTx_Request](../../../03_DRIVER/AUDIO_TX/functions/AudioTx_Request.md)/[AudioControl_Service](../../../03_DRIVER/AUDIO_CONTROL/functions/AudioControl_Service.md)에서 읽고, 논리 계약은 아래 R6 정본으로 확인한다.

[R2 역할](../../../../design/30_FUNCTIONS/40_AUDIO_STREAM_ASSET_FUNCTIONS.md#audiostream-requestcontrol) · [R3 command](../../../../design/40_DATA/50_AUDIO_STREAM_DATA.md#vssplaybackcontrolcommand) · [R3 provider](../../../../design/40_DATA/50_AUDIO_STREAM_DATA.md#vssprovidercontext) · [R3 단계](../../../../design/40_DATA/50_AUDIO_STREAM_DATA.md#vssaudiorequeststage) · [R3 동기 반환](../../../../design/40_DATA/40_PLAYBACK_DATA.md#vssrequestdisposition) · [R3 Recovery](../../../../design/40_DATA/70_DIAGNOSTIC_DATA.md#vssrecoverypermission) · [R4 참조 보호](../../../../design/50_CONTRACTS/10_OWNERSHIP_LIFETIME.md#borrow-and-protect) · [R4 late](../../../../design/50_CONTRACTS/10_OWNERSHIP_LIFETIME.md#late-attribution) · [R4 최초 시각](../../../../design/50_CONTRACTS/30_TIMING_FRESHNESS.md#first-start) · [R4 actual](../../../../design/50_CONTRACTS/20_ASYNC_RESULT_EVIDENCE.md#confirmed-outcomes) · [R4 partial](../../../../design/50_CONTRACTS/20_ASYNC_RESULT_EVIDENCE.md#partial-and-uncertain) · [R4 typed TX](../../../../design/50_CONTRACTS/50_AUDIO_BUFFER_TRANSPORT.md#tx-typed-lanes) · [R4 현재 안전](../../../../design/50_CONTRACTS/50_AUDIO_BUFFER_TRANSPORT.md#safe-write) · [R4 현재 Fault 대상](../../../../design/50_CONTRACTS/40_FAULT_RECOVERY.md#current-target) · [R5 START](../../../../design/60_PSEUDOCODE/40_AUDIO_PREPARE_TX_FLOW.md#audiostream-requestcontrol) · [R5 STOP](../../../../design/60_PSEUDOCODE/50_STOP_LATE_UNCERTAIN_FLOW.md#stream-stop) · [R5 Recovery](../../../../design/60_PSEUDOCODE/60_FAULT_RECOVERY_FLOW.md#stream-recovery) · [R6 typed STREAM](../../../../design/70_C_INTERFACE/00_HEADER_OWNERSHIP_MAP.md#typed-stream) · [R6 STOP scope](../../../../design/70_C_INTERFACE/00_HEADER_OWNERSHIP_MAP.md#stop-scope) · [R6 TX Request](../../../../design/70_C_INTERFACE/00_HEADER_OWNERSHIP_MAP.md#r6-audiotx-request) · [R6 CONTROL](../../../../design/70_C_INTERFACE/00_HEADER_OWNERSHIP_MAP.md#r6-audiocontrol-service)
