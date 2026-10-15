# Health_BuildStatus — 반영 완료 관측의 읽기 전용 Status 파생

[VSS 지도](../../../01_VSS_MAP.md) · [SERVICE](../../00_LAYER_OVERVIEW.md) · [HEALTH](../00_MODULE_OVERVIEW.md) · CORE-18 · [한국어 C형 의사코드](../pseudocode/Health_BuildStatus.md)

> GUIDE · CORE-18 · Master v1.1. R6 논리 계약 / C ABI BINDING_TBD / HW_UNVERIFIED / TEST_NOT_RUN.

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

A2는 두 준비 근거의 결합을 선택했다: 둘 다 확인 true일 때 true, 확인 false가 하나라도 있으면 false, 나머지 NULL 조합은 REEVALUATE다. 각 non-NULL 포인터 안전/출처 검증은 먼저 수행하며 unknown을 false로 보완하지 않는다. [전체 표](../../../../design/90_BINDING/93_NETWORK_STATUS_BINDING.md#reduction). accepting=false여도 duplicate·유효 CLEAR·품질/기한·정리를 계속하고 Audio Fault만으로 accepting=false를 만들지 않는다.

<a id="states"></a>
## 기존 state와 서비스 수준의 파생 순서

아래는 [A2 조합표](../../../../design/90_BINDING/93_NETWORK_STATUS_BINDING.md#reduction)의 상태 분기다. 완전 current/history와 나머지 필드의 근거도 함께 충족해야 CREATED다.

| 충분히 확인한 원 관측 | state 및 필요한 의미 |
|---|---|
| QUARANTINED/불명확 최종 또는 확인된 공통 정상 출력 불가·새 start 차단 제한 | VSS_REPORT_FAULT와 기존 UNAVAILABLE 연결 유지. PLAYBACK_STATE_FAILURE 불명확 최종을 normal로 바꾸지 않음 |
| 정상 초기화 완료, 적법 actual로 시작이 확인된 전체 Session이 아직 retirement 전 | VSS_REPORT_PLAYING. cue 사이 무음·반복 대기·시작 후 STOPPING/ABORTING도 포함 |
| 초기화 진행·출력 준비 전임이 확인되고 startup 보고 근거 충분 | VSS_REPORT_STARTUP. 미평가를 INITIALIZATION_FAILURE나 기본 가용성으로 채우지 않음 |
| 정상 초기화/준비 및 기존 READY reduction 근거 충분 | VSS_REPORT_READY. 개별 START 허가·old 자원 정리 완료 보장은 별도 |
| 위 근거가 부족하거나 모순 | REEVALUATE / written0 |

START/PREPARED/ADOPTED·TX ACK·DMA ISR·mute·Session 객체 존재만으로 PLAYING을 만들지 않는다. actual의 legal 판정은 [PB 원 관측](../../PLAYBACK/functions/Playback_Advance.md#actual)에 남기며 raw를 재해석하지 않는다. 충분 NO_START는 실제 Started로 보고하지 않고, 원 retirement/초기 준비·제한에 맞는 기존 분기로 평가한다. uncertain final은 late cleanup/안전 retirement가 나중에 와도 정상 이력으로 부활시키지 않는다.

availability는 기존 opaque 의미를 유지한다. 정상 초기화 진행 중 아직 출력 준비 전이면 STARTUP/UNAVAILABLE, 완료 후 하나 이상 서비스 가능이면 READY 또는 적법 Started Session의 PLAYING이며 전체/부분 준비에 FULL/DEGRADED를 대응한다. 공통 실패/QUARANTINED는 FAULT/UNAVAILABLE/DEGRADED가 우선이다. 서비스 지원 수준과 accepting은 독립이며 WINDOW 보류·입력 품질 상실을 출력 Fault로 합성하지 않는다.

state를 결정할 수 있어도 availability/serviceLevel/accepting/완전 current·이력/원 시간 중 필수 근거가 부족하면 REEVALUATE다. A2 표로 확인 가능한 정상·오류 보고를 생성하며 정보 부족을 FULL/true/mask0으로 채우지 않는다.

<a id="snapshot"></a>
## Snapshot 값·목록·basis와 소비 수명

[VssStatusSnapshot](../../../../design/40_DATA/70_DIAGNOSTIC_DATA.md#vssstatussnapshot)은 기존 9필드와 A2의 reportGeneration/reportSequence/evaluatedAt/lastFaultPresent/lastFaultKind/lastFaultOccurrence/lastFaultAt을 함께 채운다. current/recent kind 집합은 각각 중복 제거 최대5종이다. 현재 전체 instance와 진단 반영/손실 상태를 확인한 완전 current만 사용하며 Health_Evaluate의 이번 부분 OUT를 재사용하지 않는다. 최근 선택은 HEALTH 원기록 firstOccurredAt/reportOccurrence를 사용한다. kind 목록 순서·TX 번호로 원 발생을 복원하지 않는다.

basis 값 복사에는 내부 stamps 참조가 남는다. [A2 인계](../../../../design/90_BINDING/93_NETWORK_STATUS_BINDING.md#handoff)의 보고 2슬롯과 불변 목록/basis 보호를 선택한다. 기존 INPUT 통신 연결에서 한 번 변환하고 Network가 전체 사본을 동기 복사한 반환까지 참조를 유지한다. 포인터 얕은 복사는 보호가 아니다. 실제 고정 stamp/pool 예산·직렬화/게시 수단은 E-S01이다.

이미 소비 중인 이전 Snapshot을 새 계산/재평가로 덮어쓰지 않는다. caller가 지정한 OUT도 아직 다른 소비자가 참조 중이면 새 생성의 비간섭/보호 조건을 충족하지 못한다. HEADER·Domain DTO·Network payload·통신 ACK는 이 함수의 사후조건이 아니다.

<a id="outputs"></a>
## OUT 선검증·공개 실패와 종료

주소·metadata/written 저장소·capacity·전체 범위/산술·비간섭을 먼저 검증하고 유효 군만 written=0으로 시작한다. capacity<1·보고 불변 storage 부족·목록/basis 보호 불가·파생 도중 원본 변경에서는 새 Snapshot 공개 없이 REEVALUATE다. 기존 caller 자료·원본/최근 이력·소비 중 자료를 유지한다.

전체 field/목록·원 basis 보호와 최종 coherence 뒤 caller 영역에 한 결과를 공개한다. 같은 직렬 경계에서 reportSequence 증가를 확정하고 원 evaluatedAt을 보호한 다음 written=1/CREATED를 반환한다. 공개 실패는 written0이며 counter/time 불변이다. source 원본 읽기 전용과 HEALTH 자체 보고 counter 갱신을 구분한다. 실제 게시 원자성은 E-S01에서 입증한다.

<a id="limits"></a>
## 독립 후보와 남는 Gate

A2 문서 검수 대상은 ST01~ST12의 coherent 평가·조합·current/history·원 시각/번호·bounded handoff·부분 성공이다. 내부 정책은 [Status Binding](../../../../design/90_BINDING/93_NETWORK_STATUS_BINDING.md)에 선택했다. 실제 C ABI/시간·기동·복사/sink·DOMAIN 설치 source/consumer 근거는 E-S01~05, 물리 actual/STOP/Recovery·DMA는 기존 외부 Gate다. Host/제품 실행은 NOT_RUN이다.

OPEN-009/010·Q01~14/R6-01~08·P2 HOLD·CR/과거 FAIL·WINDOW PRODUCT 보류/TEST 격리를 유지한다. 제품 C/H·Host/Firmware/Build/Flash/Board는 착수하지 않았다.

[R2](../../../../design/30_FUNCTIONS/60_HEALTH_RUNTIME_FUNCTIONS.md#health-buildstatus) · [R3 Snapshot](../../../../design/40_DATA/70_DIAGNOSTIC_DATA.md#vssstatussnapshot) · [R3 ReadBasis](../../../../design/40_DATA/30_SELECTION_DATA.md#vssreadbasis) · [R4 복구/보고](../../../../design/50_CONTRACTS/40_FAULT_RECOVERY.md) · [R4 수명](../../../../design/50_CONTRACTS/10_OWNERSHIP_LIFETIME.md#borrow-and-protect) · [R4 coherent](../../../../design/50_CONTRACTS/30_TIMING_FRESHNESS.md#coherent-read) · [R5](../../../../design/60_PSEUDOCODE/60_FAULT_RECOVERY_FLOW.md#health-buildstatus) · [R6](../../../../design/70_C_INTERFACE/00_HEADER_OWNERSHIP_MAP.md#r6-health-buildstatus) · [의사코드 분기](../pseudocode/Health_BuildStatus.md#procedure)
