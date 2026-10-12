# Select_Choose — 상세 동작 설명

[상세 설명](../functions/Select_Choose.md) · [의사코드](../pseudocode/Select_Choose.md) · [POLICY](../00_MODULE_OVERVIEW.md) · [VSS 지도](../../../01_VSS_MAP.md) · CORE-06

> GUIDE · SELF_CHECKED / D3_2_REVIEW_PENDING · LOGICAL_APPROVED / 실제 C ABI BINDING_TBD · TEST_NOT_RUN / HW_UNVERIFIED.

<a id="contract"></a>
## 목적과 최신 논리 원형

FLOW가 연결한 반영 완료 관측을 읽어 후보·현재 출력·제한을 비교하고, 기존 중앙 정책의 선택 판단과 의미 기반 **전체 plan**을 반환합니다. POLICY는 재생을 실행하지 않습니다.

```text
VssSelectionDecision Select_Choose(
    IN const VssCandidateObservation candidates[count],
    IN const VssPlaybackObservation *playback,
    IN const VssFaultState restrictions[count],
    IN const VssAssetDescriptor assets[count],
    IN const VssDeviceReadinessState devices[count],
    IN const VssBackendCleanupObservation cleanup[count],
    IN const VssReadBasis *basis);
```

[R6 계약](../../../../design/70_C_INTERFACE/00_HEADER_OWNERSHIP_MAP.md#r6-select-choose)과 [인자·결과 표기](../../../../design/70_C_INTERFACE/00_HEADER_OWNERSHIP_MAP.md#prototype-notation)를 그대로 읽습니다. 각 count/capacity/written은 독립 논리 범위입니다. 배열 저장 방식·수치·동기 보호·enum 폭·Header는 실제 ABI로 확정하지 않습니다. 과거 R2 provisional 묶음을 새 public DTO로 선언하지 않습니다.

<a id="inputs"></a>
## 입력과 읽기 수명

| R6 인자 | 원본 owner와 읽을 핵심 | 유효 조건 |
|---|---|---|
| `candidates[count]` / `VssCandidateObservation` | STORE의 key/mode/signal/validity/originalMeta/revision, 적용되는 별도 useLimit | 각 count 범위와 원 Meta 보호. 현재 APPLICABLE만 정책 비교 |
| `playback` / `VssPlaybackObservation` | PLAYBACK의 session/attempt/candidate/phase/startKnowledge/stamp | 문맥 없음은 session 없음으로 표현. 없는 phase를 정상 ACTIVE로 합성하지 않음 |
| `restrictions[count]` / `VssFaultState` | HEALTH의 현재 instance/target/config/revision·제한 | 해당 대상의 최신 제한만 사용; 최근 기록과 현재 제한 구별 |
| `assets[count]` / `VssAssetDescriptor` | ASSET의 의미 ID·image·확인된 가용 범위 | lookup/metadata 확인은 전체 decode 성공 아님 |
| `devices[count]` / `VssDeviceReadinessState` | AUDIO CONTROL의 장치별 현재 구성·준비 관측 | 단일 ready bool로 합치거나 다른 구성 성공을 전용하지 않음 |
| `cleanup[count]` / `VssBackendCleanupObservation` | AUDIO STREAM의 원 Attempt·provider/PCM 접근 종료·출력 차단·late 보호 | 각 조건의 근거를 구분; cleanup 하나는 whole retirement가 아님 |
| `basis` / `VssReadBasis` | FLOW가 연결한 owner stamps/revision/appliedThrough·원 시간/validity | 같은 CPU 수집 시각만으로 coherent가 아님. 평가 기간만 유효 |

count=0은 해당 군의 자료 없음이며 임의 정상값을 넣지 않습니다. count>0은 읽을 저장 범위가 유효해야 합니다. 판단에 필수인 군·현재 문맥/순서·인과의 부족은 RECOLLECT입니다. 관측은 한 평가의 borrow이고 POLICY가 STORE/PLAYBACK 원본이나 owner stamp를 새로 작성하지 않습니다.

<a id="return"></a>
## 반환값과 호출 경계

`VssSelectionDecision`은 기존 `action`, 조건부 `candidate`/`plan`, 평가 `basis`를 연결합니다. WAIT/RECOLLECT의 후보·plan은 없을 수 있습니다. 정의되지 않은 plan을 붙이지 않습니다. 반환 참조는 해당 평가 기간까지 보호하고 PLAYBACK 채택 때 다시 확인합니다.

| `VssSelectionAction` | 반환 의미 |
|---|---|
| `VSS_SELECTION_RECOLLECT` | 필요한 owner 반영·원 시간/순서·비교 근거 부족/변화로 재수집 |
| `VSS_SELECTION_WAIT` | 실행 가능한 winner/정의된 plan이 없거나 제한·old cleanup 때문에 대기 |
| `VSS_SELECTION_KEEP` | 유효한 동일 current owner와 전체 plan 계속 진행 |
| `VSS_SELECTION_REPLACE` | 정책상 교체 판단. old 종료·정리 후 최신 재선택 필요 |
| `VSS_SELECTION_CHOOSE` | 새 후보·전체 plan 선택. PB 채택/prepare/start 허가가 아님 |

Caller는 [FLOW](../../../01_APP/FLOW/functions/Flow_Process.md)입니다. 결과는 FLOW가 PLAYBACK 요청에 연결합니다. **POLICY→PLAYBACK/STREAM/TX 실행 호출은 없습니다.** 정책 읽기와 이번 판단만 다루며 STORE occurrence/Stateful, PLAYBACK Session, HEALTH Fault의 두 번째 writer가 되지 않습니다.

<a id="normal"></a>
## 선택 순서와 정상 경로

1. 저장 범위·참조 수명과 함께 적용 가능한 basis를 확인합니다. `VSS_READ_CHANGED`/`VSS_READ_INCOMPLETE`, 필수 stamp의 미반영·시간 불연속이면 RECOLLECT입니다. VALID도 이후 START의 영구 허가가 아닙니다.
2. 유효 APPLICABLE 후보에서 기존 Emergency > Warning/Caution > Feedback Class와 중앙 정책의 **원 [잠정] 세부 순위**를 읽습니다. 같은 의미 미시작 One-shot은 originalMeta의 원발생 순서, 동률이면 고정 key 순서를 비교합니다. 수신/Queue 순서로 추월시키지 않습니다.
3. 후보와 의미·image가 맞는 Asset ID 및 **전체 cue/무음/반복 plan**을 연결합니다. cue 수·gapAfter·repeatRule이 미정이면 정상 plan을 0/1 등으로 채우지 않습니다. PCM 주소·decoder·DMA 설정은 plan에 없습니다.
4. 현재 유효한 One-shot을 낮은 후보·새 후보 없음·같은 의미 늦은 옛 발생으로 끊지 않습니다. 현재 후보/plan의 보호 연결이 충분하면 KEEP입니다. 최소 PB observation에 없는 plan을 새 getter나 private PB 접근으로 꺼내지 않으며, 필요한 현재 연결 근거가 없으면 RECOLLECT로 재수집하고 STOP을 생성하지 않습니다.
5. 같은 current의 유효 진행이면 KEEP, 정책상 교체 후보와 old owner가 있으면 REPLACE입니다. old가 STOPPING/ABORTING/QUARANTINED인 동안은 WAIT이며 종료 의도를 취소하지 않습니다. old retirement·장치/제한 때문에 새 실행이 불가능하면 WAIT, 충분한 새 선택이면 CHOOSE입니다.

<a id="exceptions"></a>
## 실패·대기와 원본 보존

- 후보 없음/음원·plan 부적합은 WAIT 등 해당 원 근거를 유지하며 다른 의미 음원 fallback이 없습니다. 선택될 높은 후보를 단순 장치 제한 때문에 낮은 의미로 대체하지 않습니다.
- 현재 Stateful CLEAR/DISABLED/만료·Rear 변화에 따른 종료 필요성은 관측에 남깁니다. POLICY가 직접 STOP하거나 STORE 값을 CLEAR로 쓰지 않고 PLAYBACK이 최신 실행 조건을 검사합니다. WAIT 자체는 유효 One-shot 중단 명령이 아닙니다.
- 원 순서/identity 비교가 불확실하면 새 rank·epoch 변환·수신순서를 만들지 않습니다. 기한·원 age는 재평가/새 Session으로 갱신하지 않고 `USE_LIMIT`/Network 500ms 후보와 최초 actual `<2초`를 새 min/AND 규칙으로 결합하지 않습니다.
- WINDOW 제품은 [구현 보류 — 설계 유지], 격리 TEST의 기존 의미만 검토합니다. 입력 품질 상실을 새 Fault로 합성하지 않습니다.

<a id="feasibility"></a>
## 구현 후보와 종속 미정

읽기 일관성, 원 순서/key 동률, 기존 Class·잠정 순위의 KEEP/REPLACE/WAIT 판정은 `LOGICAL_APPROVED / PROVISIONAL_TESTABLE` 독립 SW/Host 후보입니다. 순서 역전·basis 변화·현재 정상 One-shot·old cleanup·음원 미정 사례를 주입해 반환과 원본 무수정을 검증할 수 있습니다. 잠정 값은 원 정책의 한 정의 지점에서 교체·철회할 후보이며 새 설정 framework를 만들지 않습니다. 시험은 `TEST_NOT_RUN`입니다.

실제 read-basis 포착/직렬화·자료 표현/보호 용량·시간 ABI는 VSS/Runtime owner, 음원 image는 source/ASSET owner, 장치 ready·STOP 충분성은 HW/Backend owner의 확인 대상입니다. 해당 제품 연결만 `BINDING_TBD / HW_UNVERIFIED`이며 독립 선택 로직 전체를 HOLD하지 않습니다. 보관 `VssAudioArbitrator`는 우선순위/반복 경고의 정적 재사용 후보지만 실제 Session을 직접 호출하는 과거 구현이므로 이 읽기 전용 R6 Core와 동일 API로 확정하지 않습니다.

<a id="sources"></a>
## 정본과 인접 문서

[한국어 의사코드](../pseudocode/Select_Choose.md) · [R2 역할/금지](../../../../design/30_FUNCTIONS/30_POLICY_PLAYBACK_FUNCTIONS.md#select-choose) · [R3 후보·basis](../../../../design/40_DATA/30_SELECTION_DATA.md#vssreadbasis) · [판단/전체 plan](../../../../design/40_DATA/30_SELECTION_DATA.md#vssselectiondecision) · [R5 선택 절차](../../../../design/60_PSEUDOCODE/20_INPUT_TO_SELECTION.md#select-choose) · [R4 coherent read](../../../../design/50_CONTRACTS/30_TIMING_FRESHNESS.md#coherent-read) · [참조 보호](../../../../design/50_CONTRACTS/10_OWNERSHIP_LIFETIME.md#borrow-and-protect) · [PB 전환 요청](../../PLAYBACK/functions/Playback_RequestTransition.md) · [Binding/정책 미정](../../../../design/90_BINDING/91_IMPLEMENTATION_TBD.md#policy)
