# Health_BuildStatus — 반영 완료 관측의 읽기 전용 Status 파생

[VSS 지도](../../../01_VSS_MAP.md) · [SERVICE](../../00_LAYER_OVERVIEW.md) · [HEALTH](../00_MODULE_OVERVIEW.md) · CORE-18 · [한국어 C형 의사코드](../pseudocode/Health_BuildStatus.md)

> GUIDE · D6-1 SELF_CHECKED / INDEPENDENT_REVIEW_PENDING. 기준 VSS@5a17e22, Master v1.1, GD5 INDEPENDENT_PASS. R6 논리 계약 / C ABI BINDING_TBD / HW_UNVERIFIED / TEST_NOT_RUN.

<a id="contract"></a>
## R6 논리 원형과 두 반환

```text
VssStatusBuildResult Health_BuildStatus(
    IN const VssInputContext inputs[count],
    IN const VssCandidateObservation candidates[count],
    IN const VssPlaybackObservation *playback,
    IN const VssAssetDescriptor assets[count],
    IN const VssDeviceReadinessState devices[count],
    IN const VssBackendCleanupObservation cleanup[count],
    OPTIONAL IN const SemanticBool *inputAcceptingBasis,
    OPTIONAL IN const SemanticBool *storeTrackingBasis,
    IN const VssReadBasis *basis,
    OUT VssStatusSnapshot snapshot[capacity -> written]);
```

[R6 정본](../../../../design/70_C_INTERFACE/00_HEADER_OWNERSHIP_MAP.md#r6-health-buildstatus)의 두 OPTIONAL은 각각 `const SemanticBool *`이며 하나의 union/집합 DTO가 아니다. count·capacity→written은 논리 전달 주석으로 실제 C 인자 배치/폭·Header·저장 방식은 TBD다. [R2](../../../../design/30_FUNCTIONS/60_HEALTH_RUNTIME_FUNCTIONS.md#health-buildstatus)의 과거 provisional 반환 대신 scalar와 Snapshot OUT을 분리한다.

| VssStatusBuildResult | 유효 snapshot 군의 written·사후조건 |
|---|---|
| VSS_STATUS_CREATED | 정확히 1. 전체 필드와 목록/basis 내부 참조의 실제 보고 소비 수명 보호 완료 |
| VSS_STATUS_REEVALUATE | 0. 필수 근거·coherence·reduction·보호 부족, 새 Snapshot 공개 없음 |

무효 OUT는 metadata/written까지 접근하지 않으며 0을 썼다고 주장하지 않는다. invalid 인자를 위한 새 enum을 선언하지 않는다. 기존 REEVALUATE는 보고 미생성 의미이며 실제 C 오류 표현은 Binding에서 결정한다. CREATED·written=1은 송신 완료·Domain ACK·실제 음향 성공을 뜻하지 않는다.

<a id="inputs"></a>
## 모든 입력의 원본과 읽기 수명

| 방향·타입 | 소유·유효성·수명 |
|---|---|
| inputs / const VssInputContext[count] | INPUT의 source/authority/generation/ordering/품질·원 적용 근거. HEALTH가 품질을 적용하거나 정상값을 채우지 않음 |
| candidates / const VssCandidateObservation[count] | STORE의 원 key/mode/signal/validity/Meta/revision·기한 관측. current 후보와 occurrence 추적 준비를 동일시하지 않음 |
| playback / const VssPlaybackObservation * | PLAYBACK의 singular Session/Attempt·phase/startKnowledge/notificationPending/stamp. 실제 legal actual/whole lifecycle의 반영 완료 관측 |
| assets / const VssAssetDescriptor[count] | ASSET의 현재 의미/image 가용 근거. lookup 성공은 전체 decode 성공 아님 |
| devices / const VssDeviceReadinessState[count] | CONTROL의 장치/configuration별 적용 관측. 한 장치나 옛 configuration READY를 전체 정상으로 합치지 않음 |
| cleanup / const VssBackendCleanupObservation[count] | STREAM의 원 Attempt에 대한 네 독립 안전 조건. whole retirement나 currentFault clear를 대체하지 않음 |
| inputAcceptingBasis / OPTIONAL const SemanticBool * | INPUT의 이번 평가에 반영 완료된 입력 경로·검증/출처/시간 준비 근거. NULL/false/true 구분 |
| storeTrackingBasis / OPTIONAL const SemanticBool * | STORE의 이번 평가에 반영 완료된 자원·추적/replay 준비 근거. 단순 후보 존재로 합성하지 않음 |
| basis / const VssReadBasis * | FLOW의 보고 전용 owner stamps/count/time/validity. 평가 동안 불변 borrow, 전달할 복사본의 stamps 수명은 별도로 보호 |
| snapshot / VssStatusSnapshot[capacity→written] | caller의 유효 OUT 영역. 보고 소유 불변 값·Fault kind 목록·basis를 실제 소비 기간까지 보호한 결과만 한 건 |

IN count는 각 군의 실제 유효 범위다. 0은 전달 항목 없음이며 필수 owner의 공집합이 확인된 경우와 관측이 빠진 경우를 구분한다. 없는 자료를 정상/FULL/READY로 채우지 않는다. count>0의 범위·산술·내부 Meta/참조와 singular playback·basis의 접근/수명을 먼저 검증한다. 원자료는 평가 동안 안정되어야 한다. 부족/NULL 필수 참조는 REEVALUATE다.

<a id="ownership"></a>
## Caller·원본 불변과 보고 연결

Caller는 [FLOW의 별도 보고 단계](../../../01_APP/FLOW/functions/Flow_Process.md#order), Callee는 필요한 기존 Runtime 도구다. HEALTH 현재/최근 Fault·제한은 기존 HEALTH 내부 읽기 경계에서 같은 평가 기준으로 소비한다. public IN에 새 Fault 배열을 추가하거나 mutable 내부 원본 주소를 공유하지 않는다.

이 Core는 **파생 보고만** 만든다. [Health_Evaluate](Health_Evaluate.md#ownership)가 현재 Fault/허용의 유일한 writer다. BuildStatus에서 숨은 Evaluate·기한 처리·Recovery/reset·STORE/PB/INPUT/PCM/device 변경은 없다. current/recent·품질·가용성·serviceLevel·accepting/tracking을 독립 의미로 유지한다.

보고는 **HEALTH → FLOW → INPUT의 통신 연결 → 기존 Node Communication**이다. HEALTH가 CAN 송신/Domain DTO·Wire 직렬화를 수행하거나 새 publisher/API를 만들지 않는다. Q08~10 실제 DTO·복사/참조·Domain 소비는 미합의로 유지한다.

<a id="coherence"></a>
## 먼저 반영 완료·인과·원 시간을 확인한다

1. 이번 보고에 필요한 INPUT/STORE/PB/ASSET/DEVICE/CLEANUP 및 HEALTH 원본의 owner/revision/appliedThrough를 원 인과·시간과 대조한다. 모든 revision 숫자를 같게 맞추는 전역 조건은 없다.
2. basis READ_CHANGED/READ_INCOMPLETE, 수집 도중 필요한 owner 변경·미반영 사실·불충분 시간/domain/epoch·부족 stamp이면 REEVALUATE/written0이다. 처리 now를 원 발생·actual·수행/검증 시각으로 대체하지 않는다.
3. Health_Evaluate가 제한/허용을 변경했다면 FLOW가 그 변경 뒤 **보고용 새 basis**를 수집한다. 선택용 또는 변경 전 basis를 재사용하지 않는다. getter가 미반영 사실을 몰래 적용하지 않는다.
4. 모든 파생 필드를 공개 전 비공개 계산 범위에서 준비하고, 공개 직전 관련 owner 안정성·참조 보호를 다시 확인한다. 도중 변경이면 새 Snapshot 없이 재평가한다. 이 절은 새 revision manager/transaction framework가 아니며 실제 직렬화·원자성은 TBD다.

<a id="optional"></a>
## 두 OPTIONAL의 NULL·false·true

| 근거 | NULL | false | true |
|---|---|---|---|
| inputAcceptingBasis | 입력 경로 준비 미확인 | 확인된 입력 경로 제한 | 해당 owner가 확인한 이번 조건 |
| storeTrackingBasis | 추적/replay 준비 미확인 | 확인된 저장·추적 제한 | 해당 owner가 확인한 이번 조건 |

각 non-NULL 저장소·출처·scope·평가 수명은 먼저 확인한다. NULL을 false 또는 true로 쓰지 않는다. false는 unknown이 아니며 기존 accepting reduction에서 확인된 제한으로 소비한다. true도 개별 STORE accepted/출력 보장이 아니다. 현재 관측에 없는 성공을 fake true로 주입하지 않는다.

accepting 파생에 필요한 근거가 NULL/불충분 또는 기존 조합 정책이 미정이면 REEVALUATE다. 두 bool의 임의 AND·전체 truth table을 새 정책으로 확정하지 않는다. INPUT 경로와 STORE 추적 준비는 원 owner 결정이며 HEALTH는 확인된 관측의 파생만 한다. accepting=false여도 정상인 duplicate 조회·유효 CLEAR·품질/기한·정리 경로는 계속된다. Audio Fault 자체를 입력 수용 불가로 치환하지 않는다.

<a id="states"></a>
## 기존 state와 서비스 수준의 파생 순서

아래는 [R5 Status 절차](../../../../design/60_PSEUDOCODE/60_FAULT_RECOVERY_FLOW.md#health-buildstatus)의 이미 정해진 분기다. 미정 조합 전체를 확정하는 reduction 표가 아니다.

| 충분히 확인한 원 관측 | state 및 필요한 의미 |
|---|---|
| QUARANTINED/불명확 최종 또는 확인된 공통 정상 출력 불가·새 start 차단 제한 | VSS_REPORT_FAULT와 기존 UNAVAILABLE 연결 유지. PLAYBACK_STATE_FAILURE 불명확 최종을 normal로 바꾸지 않음 |
| 적법 actual로 시작이 확인된 전체 Session이 아직 retirement 전 | VSS_REPORT_PLAYING. cue 사이 무음·반복 대기·시작 후 STOPPING/ABORTING도 포함 |
| 초기 준비 미평가/진행이고 startup 보고 근거 충분 | VSS_REPORT_STARTUP. 미평가를 INITIALIZATION_FAILURE나 기본 가용성으로 채우지 않음 |
| 정상 초기화/준비 및 기존 READY reduction 근거 충분 | VSS_REPORT_READY. 개별 START 허가·old 자원 정리 완료 보장은 별도 |
| 위 근거가 부족하거나 필요한 조합 정책이 미정 | REEVALUATE / written0 |

START/PREPARED/ADOPTED·TX ACK·DMA ISR·mute·Session 객체 존재만으로 PLAYING을 만들지 않는다. actual의 legal 판정은 [PB 원 관측](../../PLAYBACK/functions/Playback_Advance.md#actual)에 남기며 raw를 재해석하지 않는다. 충분 NO_START는 실제 Started로 보고하지 않고, 원 retirement/초기 준비·제한에 맞는 기존 분기로 평가한다. uncertain final은 late cleanup/안전 retirement가 나중에 와도 정상 이력으로 부활시키지 않는다.

availability는 R3 opaque VssAvailabilityValueType의 기존 의미이며 새 enum/숫자 인코딩을 만들지 않는다. serviceLevel FULL/DEGRADED는 지원 서비스·실제 제한 근거에서 별도로 파생한다. WINDOW 보류만으로 DEGRADED/FAULT를 만들지 않는다. INPUT STALE/INVALID/NO_DATA와 current outputFault, recentFault, 서비스 가용성을 동일시하지 않는다.

state를 결정할 수 있어도 availability/serviceLevel/accepting/Fault projection 중 **필요한 reduction이 미정이면 전체 Snapshot을 공개하지 않는다**. 임의 기본 FULL/true·초기 모든 availability·Fault→상태 전체 matrix·새 timeout을 확정하지 않는다. FAULT/UNAVAILABLE의 고정 연결과 최근 Fault 보존은 그대로다.

<a id="snapshot"></a>
## Snapshot 값·목록·basis와 소비 수명

[VssStatusSnapshot](../../../../design/40_DATA/70_DIAGNOSTIC_DATA.md#vssstatussnapshot)의 기존 9필드만 채운다: state/availability/serviceLevel/accepting/currentFaults/currentFaultCount/recentFaults/recentFaultCount/basis. currentFaults와 recentFaults는 각각 **const VssFaultKind 의미 값 목록**이다. mutable VssFaultState 포인터를 넣지 않고 실제 포함한 count·범위를 보호한다. 공간에 맞추려고 누락한 Fault를 완전한 집합으로 보고하지 않는다. current 해제는 recent 삭제가 아니다.

basis 값 복사에는 내부 stamps 참조가 남는다. 목록과 stamps/time 및 필요한 모든 내부 참조는 보고 소유 불변 저장 또는 독립 불변 보호 자료로 유지한다. 포인터/struct의 얕은 복사는 보호 완료가 아니다. FLOW→INPUT→Node Communication이 실제로 복사하면 복사 완료와 관련 참조 종료까지, 참조 방식이면 최종 실제 소비 종료까지 보호한다. 방식·bounded storage·회수/직렬화 ABI는 미정이다.

이미 소비 중인 이전 Snapshot을 새 계산/재평가로 덮어쓰지 않는다. caller가 지정한 OUT도 아직 다른 소비자가 참조 중이면 새 생성의 비간섭/보호 조건을 충족하지 못한다. HEADER·Domain DTO·Network payload·통신 ACK는 이 함수의 사후조건이 아니다.

<a id="outputs"></a>
## OUT 선검증·공개 실패와 종료

주소·metadata/written 저장소·capacity·전체 범위/산술·비간섭을 먼저 검증하고 유효 군만 written=0으로 시작한다. capacity<1·보고 불변 storage 부족·목록/basis 보호 불가·파생 도중 원본 변경에서는 새 Snapshot 공개 없이 REEVALUATE다. 기존 caller 자료·원본/최근 이력·소비 중 자료를 유지한다.

전체 field/목록·원 basis 보호와 최종 coherence를 확인한 **뒤** caller의 유효 영역에 한 결과를 공개하고 written=1/CREATED를 반환한다. 부분 쓰기·보호 실패를 CREATED로 세지 않는다. 실제 게시 실패 가능성과 동시 읽기 보호는 Binding에서 입증할 대상이며 확정 무손실·원자적 ABI를 주장하지 않는다. 새 framework·비상 슬롯·무한 재시도는 없다.

<a id="limits"></a>
## 독립 후보와 남는 Gate

F1/F3 후보는 mixed owner 관측→REEVALUATE, OPTIONAL NULL/false/true, legal actual/전체 Session 보고, 독립 current/recent projection, OUT bounds·비동기 수명 보호다. 향후 Host fixture 아이디어만 기술했고 실행하지 않았다. F4 Q08~10 DTO/Node 소비·source/image·C ABI/자료 보호·전체/초기 reduction, F5 실제 actual/NO_START/STOP/Recovery 효과·DMA/메모리/시간은 BINDING_TBD/HW_UNVERIFIED다. source 관측 없이 새 C 심볼/ABI를 확정하지 않는다.

OPEN-009/010·Q01~14/R6-01~08·P1B_C1 REQUIRED_NOT_RUN·P2 HOLD·CR/과거 FAIL·WINDOW PRODUCT 보류/TEST 격리를 유지한다. GD5 승인과 D5-1~3 독립 PASS는 기존 근거로 보존한다. D6-2/GD6/D7 및 제품 C/H·Host/Firmware/Build/Flash/Board는 착수하지 않았다.

[R2](../../../../design/30_FUNCTIONS/60_HEALTH_RUNTIME_FUNCTIONS.md#health-buildstatus) · [R3 Snapshot](../../../../design/40_DATA/70_DIAGNOSTIC_DATA.md#vssstatussnapshot) · [R3 ReadBasis](../../../../design/40_DATA/30_SELECTION_DATA.md#vssreadbasis) · [R4 복구/보고](../../../../design/50_CONTRACTS/40_FAULT_RECOVERY.md) · [R4 수명](../../../../design/50_CONTRACTS/10_OWNERSHIP_LIFETIME.md#borrow-and-protect) · [R4 coherent](../../../../design/50_CONTRACTS/30_TIMING_FRESHNESS.md#coherent-read) · [R5](../../../../design/60_PSEUDOCODE/60_FAULT_RECOVERY_FLOW.md#health-buildstatus) · [R6](../../../../design/70_C_INTERFACE/00_HEADER_OWNERSHIP_MAP.md#r6-health-buildstatus) · [의사코드 분기](../pseudocode/Health_BuildStatus.md#procedure)
