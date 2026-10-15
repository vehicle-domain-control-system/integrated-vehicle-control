# Health_Evaluate — 원진단과 현재 Fault의 제한·복구 허용

[VSS 지도](../../../01_VSS_MAP.md) · [SERVICE](../../00_LAYER_OVERVIEW.md) · [HEALTH](../00_MODULE_OVERVIEW.md) · CORE-17 · [한국어 C형 의사코드](../pseudocode/Health_Evaluate.md)

> GUIDE · CORE-17 · Master v1.1. R6 논리 계약 / C ABI BINDING_TBD / HW_UNVERIFIED / TEST_NOT_RUN.

<a id="contract"></a>
## R6 논리 원형과 반환

```text
VssHealthChange Health_Evaluate(
    IN const VssDiagnosticEvidence diagnostics[count],
    IN const VssRecoveryPerformedEvidence performed[count],
    IN const VssRecoveryVerificationEvidence verified[count],
    IN const VssPlaybackObservation *playback,
    IN const VssDeviceReadinessState devices[count],
    IN const VssBackendCleanupObservation cleanup[count],
    IN const VssReadBasis *basis,
    OUT VssFaultState faults[capacity -> written],
    OUT VssRecoveryPermission permissions[capacity -> written]);
```

[R6 정본](../../../../design/70_C_INTERFACE/00_HEADER_OWNERSHIP_MAP.md#r6-health-evaluate)의 순서·타입·독립 범위를 유지한다. IN/OUT/count/capacity→written은 논리 전달 주석이다. 배열별 count와 OUT별 capacity/written은 서로 독립이며 실제 C 인자 전개·정수 폭·Header·저장/직렬화 수단은 미정이다. 과거 R2 provisional 묶음을 통합 결과나 새 API로 복원하지 않는다.

| 반환 VssHealthChange | 이 함수가 확인한 결과 |
|---|---|
| VSS_HEALTH_CHANGED | 현재 제한·허용·해제의 실제 반영 완료 변화. 다른 대상의 미결·미인계까지 완료됐다는 뜻은 없음 |
| VSS_HEALTH_RECONFIRM_REQUIRED | 현재 대상/시간/basis 또는 수행·검증 pair가 부족하여 재확인 필요 |
| VSS_HEALTH_UNCHANGED | 필요한 판단에서 변경·재확인 없음. 물리 정상·복구 완료·모든 OUT 소비 ACK가 아님 |

[R5 순서](../../../../design/60_PSEUDOCODE/60_FAULT_RECOVERY_FLOW.md#health-evaluate)를 따른다. 진단·불명확 최종 제한을 보호/적용한 뒤 basis 부족 분기는 RECONFIRM_REQUIRED를 반환할 수 있다. 정상 끝에서는 변경 → 미결 → UNCHANGED 순이다. 반환 하나로 이미 적용한 변화나 아직 부족한 다른 사실을 지우지 않는다.

<a id="inputs"></a>
## 인자별 원본·유효성·수명

| 방향·타입 | 읽는 근거와 보호 기간 |
|---|---|
| diagnostics / const VssDiagnosticEvidence[count] | 원 producer/stage/cause/target/configuration/optional operation/occurredAt. 외피·count·내부 참조 검증 뒤 읽고 진단/현재·최근 추적에 필요한 원본을 불변 보호 |
| performed / const VssRecoveryPerformedEvidence[count] | 실제 수행의 permission/target/configuration/operation/effect/occurredAt. 요청 수용과 구분하고 원 수행 추적까지 보호 |
| verified / const VssRecoveryVerificationEvidence[count] | 별도 효과 관측의 permission/performedOperation/target/configuration/effect/verifiedAt. 수행과 독립 전달·보호 |
| playback / const VssPlaybackObservation * | PLAYBACK이 반영한 session/attempt/phase/startKnowledge/stamp. singular 읽기 참조이며 raw ISR을 새 actual/final로 판정하지 않음 |
| devices / const VssDeviceReadinessState[count] | CONTROL의 장치별 현재 device/configuration/state/revision. 옛 READY·한 장치 성공으로 전체 정상 합성 없음 |
| cleanup / const VssBackendCleanupObservation[count] | 원 Attempt의 providerReferencesEnded/pcmAccessEnded/lowerOutputBlocked/lateIdentityProtected. 네 조건·whole retirement·기록 수명은 별개 |
| basis / const VssReadBasis * | FLOW의 owner/revision/appliedThrough·원 시간·validity. stamps/count/time 내부 borrow도 검증. 평가 수명이며 영구 수행 허가 아님 |
| faults / VssFaultState[capacity→written] | 실제 보호한 현재/관련 Fault 관측. diagnosis 참조도 현재·최근/소비 추적 수명으로 보호. HEALTH mutable 원본을 공유하지 않음 |
| permissions / VssRecoveryPermission[capacity→written] | 현재 유효한 6필드 허용의 불변 전달. 소비 시점의 현재 허용 재확인이 별도로 필요 |

각 IN count=0은 그 군에 전달된 항목 없음이다. diagnostics=0으로 Fault를 clear하거나, performed=0을 수행 성공으로, verified=0을 효과 성공으로 바꾸지 않는다. devices/cleanup=0도 준비·안전 완료가 아니다. 양수 범위와 모든 필수 참조는 읽기 전에 확인한다. NULL/무효 playback·basis는 필요한 판단의 근거 부족이며 정상 관측을 합성하지 않는다. 안전하게 보호 가능한 독립 원진단은 유지한다.

<a id="ownership"></a>
## Caller·단일 writer와 다섯 복구 경계

Caller는 [FLOW](../../../01_APP/FLOW/functions/Flow_Process.md#startup-recovery), Callee는 필요한 기존 Runtime 경계뿐이다. HEALTH만 현재·최근 Fault, 대상 제한과 복구 허용/해제를 쓴다. PLAYBACK/STORE/INPUT·CONTROL readiness·STREAM provider/PCM A/B·TX operation 원본은 읽기 전용이다. Backend reset·Driver/HAL·Network 송신을 직접 호출하지 않는다.

**HEALTH permission → FLOW 안전 연결 → Backend/Driver 실제 performed → 별도 verified → HEALTH 현재 제한 해제**를 분리한다. [STREAM Recovery](../../AUDIO_STREAM/functions/AudioStream_RequestControl.md#recovery)는 fault/permission identity를 사용하며 Session 없는 복구에 재생 Session/Attempt를 만들지 않는다. [CONTROL](../../../03_DRIVER/AUDIO_CONTROL/functions/AudioControl_Service.md#recovery)·[TX](../../../03_DRIVER/AUDIO_TX/functions/AudioTx_Advance.md#outputs)·[HAL](../../../04_HAL_BSP/AUDIOHAL_BOUNDARY/functions/AudioHAL_Callback.md#ownership)의 원 producer·귀속·원 시각은 Driver→STREAM→FLOW→HEALTH 연결에서도 유지된다.

<a id="diagnostics"></a>
## 원진단 보호와 Fault 5종

1. 기존 bounded 기회 안에서 원진단을 독립 보호한다. 마지막 한 슬롯으로 다른 진단/수행/검증을 덮지 않고, 처리 now로 발생 시각을 다시 찍지 않는다.
2. INPUT INVALID/STALE/NO_DATA 의미, startup/device UNASSESSED, 단순 WINDOW 제품 보류는 출력 Fault·DEGRADED·UNAVAILABLE로 자동 변환하지 않는다. 확인된 초기화 실패와 아직 평가하지 않은 상태를 구분한다.
3. 확인된 실패이고 승인된 기존 분류 근거가 있는 범위만 아래 5종으로 연결한다. exact cause→Fault/restriction/action이 미정이면 원진단과 필요한 제한 판단 미결을 보존한다. 임의 reset/retry·새 Fault를 만들지 않는다.

| 기존 Fault kind | 기존 의미 |
|---|---|
| SOUND_ASSET_UNAVAILABLE | 필요한 저장 음원 사용 불가 |
| PLAYBACK_START_FAILURE | 확인된 재생 시작 실패 |
| AUDIO_OUTPUT_FAILURE | 공통 출력 경로 이상 |
| PLAYBACK_STATE_FAILURE | 재생 상태·출력 소유권 정합성 이상 |
| INITIALIZATION_FAILURE | 확인된 초기화 실패 |

원 instance/kind/target/configuration/diagnosis/restriction을 보호한 뒤 CURRENT와 실제 revision 반영 완료를 공개한다. 다른 current와 recent는 유지한다. 숫자 코드·발생 key 생성/폭·증가폭/회수 정책을 새로 정하지 않는다.

<a id="uncertain"></a>
## PLAYBACK 불명확 최종과 현재 제한

반영 완료된 원 PLAYBACK의 START_OUTCOME_UNCERTAIN 최종 근거는 diagnostics count=0이어도 **PLAYBACK_STATE_FAILURE + FAULT + UNAVAILABLE** 제한으로 유지한다. 원 final/phase/stamp·대상 연결을 확인한 경우만 적용하며 HEALTH가 timeout·START_IN_FLIGHT 대기를 불명확 최종으로 판정하지 않는다. raw IRQ·ACK·mute·READY·cleanup 하나로 이 제한을 정상화하지 않는다. [PLAYBACK 전체 최종](../../PLAYBACK/functions/Playback_Advance.md#termination)의 원 사실을 따른다.

<a id="permission"></a>
## 현재 허용과 철회·대상 변경

basis가 READ_VALID이고 필요한 owner 반영·인과·현재 target/configuration·시간이 충분한 경우만 기존 recoverable 정책과 허용 조건을 평가한다. owner revision 숫자가 모두 같아야 한다는 규칙은 없다. 부족/변경이면 현재 제한을 보존하고 새 허용·해제를 보류한다. 현재 유효성을 확인할 수 없는 허용은 permissions OUT에 공개하지 않으며 old 원자료만 추적에 보존한다.

허용은 `permission/faultInstance/faultRevision/target/configuration/action` 6필드를 현재 Fault와 승인 범위로 불변 보호한 자료다. 발급·수락만으로 performed/verified·성공을 만들지 않는다. instance/revision/configuration/action 변경·철회는 old 허용의 새 수행을 금지한다. old 허용 값을 소급 수정하지 않고 원 추적과 old 결과를 보존한다. FLOW→STREAM→Driver는 효력 직전 현재 허용·old 실제 위험 종료·대상을 재검사하며 실물 action 매핑 미정이면 해당 수행을 보류한다.

<a id="pair"></a>
## 실제 수행·별도 검증 pair와 한 대상의 해제

수행만 또는 검증만 전달되면 원 사실을 독립 보호하고 현재 제한을 유지한다. 후속에 보호된 두 사실을 함께 연결할 수 있으나, 이 호출에서 없는 군을 성공/가짜 pair로 채우지 않는다. 둘의 count·전달 순서는 같을 필요가 없다. 원 수행 뒤 별도 검증이라는 인과는 전달 순서와 별개다.

해제 전 다음을 **각 원 pair와 현재 대상에 모두** 대조한다.

- 현재 permission key = performed.permission = verified.permission; 현재 Fault instance/revision = permission.faultInstance/faultRevision.
- target/configuration과 현재 허용 action 연결이 일치하며 `performed.operation == verified.performedOperation`이다. 같은 Fault kind·old Session 성공은 동일성 증명이 아니다.
- performed.occurredAt → verified.verifiedAt의 domain/epoch·연속성·가능 구간과 수행 이후 별도 효과 관측의 인과·해당 효과 충분성이 입증된다. 도착 순서/처리 now로 대체하지 않는다.
- 양쪽 effect가 VSS_RECOVERY_SUCCEEDED이고, 해제 반영 직전에도 현재 instance/revision/허용·대상 조건이 유효하다.

통과한 현재 Fault/대상의 해당 제한만 해제하고 기존 정책대로 RECENT 이력을 보존한다. 다른 current/recent는 그대로다. 실제 변경과 permission 종료/무효화의 revision을 반영 완료한다. 실패/UNCONFIRMED/부분 효과·LOST·operation 불일치·옛 revision·짝 부족이면 clear 없이 원 사실과 제한·후속을 유지한다. Backend 완료 통지·performed 성공·장치 READY·무음만으로 verified 성공을 합성하지 않는다.

복구 뒤 최신 후보의 별도 재평가 필요성을 FLOW에 남긴다. HEALTH가 START·One-shot 부활·STORE final/replay 변경·age/Hold/UseLimit 초기화·PCM offset resume를 하지 않는다. [동결 복구 계약](../../../../design/50_CONTRACTS/40_FAULT_RECOVERY.md#current-target)을 유지한다.

<a id="outputs"></a>
## 두 OUT의 독립 인계와 보호 실패

각 군의 주소·metadata/written 저장소·capacity·산술 범위·비간섭을 검증한 **뒤** 유효 군만 written=0으로 시작한다. 무효 군은 written도 읽거나 쓰지 않는다. 유효 군의 실제 기록과 diagnosis/permission 참조 보호 완료 뒤 자기 written만 증가한다. faults full/invalid여도 안전한 permissions는 독립 전달할 수 있고 그 역도 같다.

capacity=0/full·부분 기록·참조 보호 실패는 인계 완료가 아니다. protected owner 사실·제한과 미인계/재확인 의무를 유지한다. 일부 OUT를 전체 Fault 목록·전체 해제 ACK로 해석하지 않는다. 원자료 보호 자체가 불가능하면 해당 신규 적용/허용을 완료로 주장하지 않고 실제 부족·손실을 기존 경계에 남긴다. 기존 current/recent·accepted/final 추적 eviction, 무한 pool·새 Queue/manager/ack API로 해결하지 않는다. 보호 수단의 실제 용량·원자성은 TBD다.

<a id="limits"></a>
## 구현 후보·확인 한계와 정본

F1/F3 독립 후보는 count/독립 OUT, 원 identity·revision·operation pair, 단독/실패/역순 사실 보존, 미평가 제외, 현재 한 대상 해제다. 명시적 충분/부족 관측을 주입하는 향후 Host 후보이며 실제 시험은 NOT_RUN이다. F2 원 시간 표현은 근거 있는 시험 가정으로만 다루고 제품 수치로 확정하지 않는다.

F4 실제 C ABI·key/storage/직렬화·source→image·cause/action 매핑, F5 실제 수행/별도 효과·DMA/STOP/actual·cache/MPU/WCET는 BINDING_TBD/HW_UNVERIFIED다. 현재 target에 제품 C/H 근거가 없으며 SOURCE_NOT_VERIFIED다. 미정은 관련 제품 효과만 제한한다. Q01~14/R6-01~08·OPEN-009/010·P2 HOLD·과거 FAIL/CR·WINDOW PRODUCT 보류/TEST 격리를 보존한다. Host/Firmware/Build/Flash/Board는 착수하지 않았다.

- [R2 역할](../../../../design/30_FUNCTIONS/60_HEALTH_RUNTIME_FUNCTIONS.md#health-evaluate) · [R3 Fault](../../../../design/40_DATA/70_DIAGNOSTIC_DATA.md#vssfaultstate) · [R3 허용](../../../../design/40_DATA/70_DIAGNOSTIC_DATA.md#vssrecoverypermission) · [R3 수행](../../../../design/40_DATA/70_DIAGNOSTIC_DATA.md#vssrecoveryperformedevidence) · [R3 검증](../../../../design/40_DATA/70_DIAGNOSTIC_DATA.md#vssrecoveryverificationevidence)

- [R4 수명](../../../../design/50_CONTRACTS/10_OWNERSHIP_LIFETIME.md#borrow-and-protect) · [R4 복구](../../../../design/50_CONTRACTS/40_FAULT_RECOVERY.md#current-target) · [R5 절차](../../../../design/60_PSEUDOCODE/60_FAULT_RECOVERY_FLOW.md#health-evaluate)

- [R6 원형](../../../../design/70_C_INTERFACE/00_HEADER_OWNERSHIP_MAP.md#r6-health-evaluate) · [의사코드 분기](../pseudocode/Health_Evaluate.md#procedure) · [Status 읽기 전용 파생](Health_BuildStatus.md#ownership)
