# Playback_RequestTransition — 상세 동작 설명

> A1 입력 개정 2026-10-15 · [현재 Network 연결](../../../../design/90_BINDING/92_NETWORK_BINDING.md). `L=min(양수 USE_LIMIT,500ms,2000ms)`; 최초 actual에만 적용. 과거 동결/PASS는 당시 범위의 이력이며 본문 변경은 이번 A1 결정이다. 구현·시험 NOT_RUN.

[상세 설명](../functions/Playback_RequestTransition.md) · [의사코드](../pseudocode/Playback_RequestTransition.md) · [PLAYBACK](../00_MODULE_OVERVIEW.md) · [VSS 지도](../../../01_VSS_MAP.md) · CORE-07

> GUIDE · LOGICAL_APPROVED / 실제 C ABI BINDING_TBD · TEST_NOT_RUN / HW_UNVERIFIED.

<a id="contract"></a>
## 목적과 최신 논리 원형

FLOW의 선택 판단 또는 원 Session 종료 의도를 최신 조건으로 검토하고 PLAYBACK의 채택·유지·후속 권한 차단을 반영합니다. 동기 수용은 준비 완료·actual·물리 종료가 아닙니다.

```text
VssRequestDisposition Playback_RequestTransition(
    ONE_OF const VssSelectionDecision *decision,
           const VssSessionKeyType *terminationSession,
    OPTIONAL IN const VssCauseType *terminationReason,
    IN const VssCandidateObservation candidates[count],
    IN const VssFaultState restrictions[count],
    IN const VssAssetDescriptor assets[count],
    IN const VssDeviceReadinessState devices[count],
    IN const VssBackendCleanupObservation cleanup[count],
    IN const VssReadBasis *conditionsBasis);
```

[R6 계약](../../../../design/70_C_INTERFACE/00_HEADER_OWNERSHIP_MAP.md#r6-playback-requesttransition)과 [인자·결과 표기](../../../../design/70_C_INTERFACE/00_HEADER_OWNERSHIP_MAP.md#prototype-notation)를 그대로 읽습니다. 각 count/capacity/written은 독립 논리 범위입니다. 배열 저장 방식·수치·동기 보호·enum 폭·Header는 실제 ABI로 확정하지 않습니다. 과거 R2 provisional 묶음을 새 public DTO로 선언하지 않습니다.

<a id="inputs"></a>
## 인자·배타 조건과 최신 실행 조건

| 인자/타입 | 의미·소유·검증 |
|---|---|
| `decision` / `VssSelectionDecision` | POLICY의 평가 borrow. action/원 candidate·전체 plan/basis는 채택 직전 다시 확인 |
| `terminationSession` / `VssSessionKeyType` | 종료할 **원 Session** key의 읽기 참조. 현재 owner와 다른 옛 대상이면 current를 종료하지 않음 |
| `terminationReason` / optional `VssCauseType` | termination lane의 기존 의미 사유. 없음은 허용하지만 임의 원인/정상 종료 근거를 채우지 않음 |
| `candidates[count]` | STORE의 최신 원 후보·key/revision·원 Meta/현재 품질·기한 관측 |
| `restrictions[count]` | HEALTH의 해당 현재 instance/target/config 제한. PB가 Fault clear를 쓰지 않음 |
| `assets[count]` | ASSET의 원 의미 ID/image·정의된 plan의 가용 근거 |
| `devices[count]` | AUDIO CONTROL의 각 현재 구성 준비 근거; 실제 종료 증명이 아님 |
| `cleanup[count]` | STREAM의 각 원 Attempt/provider·PCM 접근 종료·lower 출력 차단·late 보호 |
| `conditionsBasis` / `VssReadBasis` | FLOW의 이번 평가 반영 완료·시간·인과 근거. 영구 실행 권한 아님 |

**ONE_OF:** decision/terminationSession은 정확히 하나입니다. 둘 다 없음/둘 다 존재하면 효력 전 REJECTED, 기존 owner·출력 사실은 불변입니다. reason만으로 종료/가짜 Session/복구를 만들지 않습니다. decision lane에 termination 사유를 혼합하지 않습니다. 배열 count는 각각 독립하며 양수 범위가 unsafe면 읽지 않습니다.

PB는 자기 원본의 Session/Attempt·phase·후속 권한·보호 자원과 old 전체 retirement를 별도로 검사합니다. FLOW의 basis VALID 하나가 owner의 실행 허가를 대신하지 않습니다. 채택 후 plan/원 Meta·identity는 PB의 전체 정상/정리/통지 수명으로 보호하고 평가 포인터를 그대로 장기 저장하지 않습니다.

<a id="return"></a>
## 반환·Writer·Caller/Callee

| `VssRequestDisposition` | 이 Core의 동기 결과 |
|---|---|
| `VSS_REQUEST_REJECTED` | 배타/대상/의도 근거 부적합. 새 효력 없음·기존 보호 유지 |
| `VSS_REQUEST_WAIT` | 최신 조건/old 정리·보호 자원 부족 또는 재선택 필요. 새 Session 준비 없음 |
| `VSS_REQUEST_KEPT` | 같은 유효 owner/plan 진행 또는 이미 보호한 같은 차단 의도 유지 |
| `VSS_REQUEST_ADOPTED` | PB가 채택/종료 의도를 반영함. 하위 prepared/actual/물리 STOP/retirement가 아님 |

Caller는 [FLOW](../../../01_APP/FLOW/functions/Flow_Process.md), 단일 writer는 PLAYBACK입니다. Callee는 `AudioStream_Prepare` 또는 `AudioStream_RequestControl`의 **기존 R6 typed lane**입니다. RequestControl은 playback command와 recovery 중 정확히 하나이며 여기서는 원 playback command만 전달합니다. Session 없는 recovery를 위해 PB Session을 생성하지 않습니다.

하위 `VssRequestDisposition`과 `requests`/`diagnostics`는 각각 원문맥으로 보호하고 [Playback_Advance](Playback_Advance.md)에 연결합니다. PB disposition과 STREAM disposition은 각 callee의 의도 반영 결과이므로 하나의 전체 성공으로 합치지 않습니다. STORE ledger/Stateful·PCM 사용권/장치 retain·HEALTH Fault는 PB가 직접 쓰지 않습니다.

<a id="termination"></a>
## 원 종료 의도와 현재 조건 상실

원 termination key와 보호된 owner 연결을 확인한 뒤 **start/cue/repeat 권한을 먼저 차단**하고 STOP 의도를 STREAM에 전달합니다. reason이 없어도 유효한 명시 종료 의도는 처리할 수 있으나 없는 세부 사유를 합성하지 않습니다. 선점/유효 CLEAR/정책 끝은 STOPPING, 실패/확정 위반은 ABORTING, 기존 QUARANTINED는 유지합니다.

`stop.session`/`stop.attempt`는 원 문맥입니다. 현재 Attempt는 귀속 anchor이며 STREAM의 Session 종료 의미를 한 Attempt에만 축소하지 않습니다. 관련 원 Attempt/operation/segment·잔류 출력 전체의 차단·정리는 하위 STOP 계약으로 연결합니다. STOP 거부/부분 효력/지연 뒤에도 PB 후속 권한 차단과 원 보호를 유지합니다. 반환 ADOPTED/KEPT는 실제 종료를 확정하지 않습니다.

다른 옛 Session 또는 미존재 key는 current 종료/권한 변경 없이 거부·원 귀속 기록/진단만 보호합니다. decision lane에서도 별도로 입증된 현재 유효 CLEAR·기한/권한 상실·고장은 PB가 안전 종료할 수 있습니다. 불완전 basis만으로 CLEAR/종료 성공을 만들지 않으며 새로운 START는 대기합니다.

<a id="actions"></a>
## 선택 action별 채택과 실패

| action/조건 | 실제 행동·반환 |
|---|---|
| `RECOLLECT` / `WAIT` | WAIT. 단순 대기만으로 current 정상 One-shot STOP 없음 |
| `KEEP` | 같은 원 candidate/plan·현재 권한·최신조건이 유효하면 KEPT, 근거 변화는 WAIT. 새 Session/Attempt/prepare 없음 |
| `REPLACE` + old ACTIVE | old 전체 후속 권한 차단·원 STOP/결과 보호, WAIT. old retirement 뒤 FLOW 최신 재선택 필요 |
| old STOPPING/ABORTING/QUARANTINED | old 정리 유지, WAIT. late winner/KEEP/acceptance로 ACTIVE 복귀 없음 |
| `REPLACE`인데 old retirement로 원 평가가 바뀜 | 그 오래된 winner를 자동 실행하지 않고 WAIT·새 선택 요구 |
| `CHOOSE` + old 전체 retirement/실제 자원 안전·최신 원 후보/plan 부족 | WAIT/REJECTED. old 참조/사실 보호, 새 준비 없음 |
| `CHOOSE` + 모든 선결조건·독립 보호 자원 충분 | 원 identity/전체 plan/Meta를 **효력 전 보호**, PREPARING·원 Prepare 요청, PB ADOPTED |

새 채택은 원 후보의 현재 적용 가능성·revision·newSessionAllowed, 의미 plan/image·제한·장치 구성·읽기 basis를 부수효력 직전에 재검사합니다. 미시작 One-shot의 원 timebase/epoch/연속성과 `A1 원 age <L`가 부족/만료하면 WAIT이며 `conditionsBasis.time`을 원 발생 시각으로 덮지 않습니다. 원 Meta.useLimit=L을 처음 실제 시작까지 보존합니다. 정상 Started One-shot은 재채택/자동 resume하지 않으며 현재 유효 또는 [입증된 Hold 내 Stateful](../../../../design/50_CONTRACTS/30_TIMING_FRESHNESS.md#hold-reselection)은 old retirement 뒤 **새 Session의 정책 시작점**부터 가능합니다. Hold 경계 걸침/근거 부족은 기존 종료 lane으로 보수적 종료를 요청하며 확정 만료나 물리 STOP 완료로 취급하지 않습니다.

하위 호출 이전에 Session/Attempt 원귀속·첫 Meta/전체 plan·결과 보호를 확보합니다. PCM/Provider는 각 owner의 안전 근거를 확인할 뿐 직접 재사용/반환하지 않습니다. 하위 prepare가 WAIT/실패/부분 효력을 반환하면 원 채택 문맥과 후속 정리/결과 추적을 보존합니다. PB가 이미 채택한 의도는 하위 실패를 단순 신규 거부로 rollback하지 않습니다. 준비 직후 CLEAR·기한/권한 상실은 Advance의 **실제 START 직전** 재검사로 차단합니다.

<a id="feasibility"></a>
## 구현 후보·한정 Gate

ONE_OF, 같은 owner KEEP, REPLACE 권한 선차단, old retirement 후 재선택, 자원 부족 WAIT, 원 plan/Meta 보호는 `LOGICAL_APPROVED` 독립 구현 후보입니다. Null 조합·옛 Session·CLEAR 경합·하위 거부/부분 효력·cleanup 부족을 주입하는 Host fixture를 만들 수 있으며 현재 `TEST_NOT_RUN`입니다. 기존 `VssAudioSession`의 상태·StartSound/StopSound 호출은 정적 재사용 후보이고 이 R6 채택/actual/retirement 충분성 구현 완료를 증명하지 않습니다.

key wrap/alias·저장 예산/참조·직렬화/시간 ABI는 VSS/Runtime owner, 실제 provider/PCM 접근 종료·STOP 범위·출력 경계는 STREAM/TX/HW owner가 입증해야 합니다. 해당 제품 효과만 미확정으로 유지합니다. 미시작 gate의 원 잠정 값은 한 정의 위치와 경계 fixture로 후속 채택/교체·철회할 대상입니다. WINDOW 제품 보류·TEST 격리, P1B_C1/Q/R6·외부IF 및 P2 HOLD는 해제하지 않습니다.

<a id="sources"></a>
## 정본과 인접 문서

[의사코드](../pseudocode/Playback_RequestTransition.md) · [선택 설명](../../POLICY/functions/Select_Choose.md) · [Advance](Playback_Advance.md) · [R2](../../../../design/30_FUNCTIONS/30_POLICY_PLAYBACK_FUNCTIONS.md#playback-requesttransition) · [R3 Session/Attempt](../../../../design/40_DATA/40_PLAYBACK_DATA.md#vsssessioncontext) · [R3 Prepare/Control](../../../../design/40_DATA/50_AUDIO_STREAM_DATA.md#vssaudiopreparation) · [R5 채택](../../../../design/60_PSEUDOCODE/30_PLAYBACK_FLOW.md#playback-requesttransition) · [R5 전체 STOP](../../../../design/60_PSEUDOCODE/50_STOP_LATE_UNCERTAIN_FLOW.md#stream-stop) · [R4 보호](../../../../design/50_CONTRACTS/10_OWNERSHIP_LIFETIME.md#borrow-and-protect) · [R4 최초 시작](../../../../design/50_CONTRACTS/30_TIMING_FRESHNESS.md#first-start) · [R6 STREAM 제어](../../../../design/70_C_INTERFACE/00_HEADER_OWNERSHIP_MAP.md#r6-audiostream-requestcontrol)
