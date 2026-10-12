# Input_Process — 검증 문맥 적용과 STORE 전달

[VSS 지도](../../../01_VSS_MAP.md) · [INPUT](../00_MODULE_OVERVIEW.md) · CORE-02 · [한국어 의사코드](../pseudocode/Input_Process.md)

> GUIDE · D3-1 SELF_CHECKED / D3_1_REVIEW_PENDING. 시작 `VSS@7bb5859b6aef909ed80f527c477280b09df50568`; 의미 기준 `0641b2c2268cf47450aeb50302638a45a7a3277f`. TEST_NOT_RUN / BINDING_TBD / HW_UNVERIFIED.

도착 자료의 값·Meta를 검증하고 현재 적용 가능한 INPUT 문맥을 먼저 반영한다. 이어 사건·상태·품질 중 해당하는 하나를 STORE에 전달한다. RX가 없는 호출도 기존 문맥의 품질을 점검한다. Domain 사건을 VSS가 새로 결정하지 않는다.

<a id="contract"></a>
## R6 논리 원형과 반환

```text
VssInputApplication Input_Process(
    OPTIONAL IN const VssInputEvidence *input,
    IN const VssTimeEvidence *time,
    OUT VssDiagnosticEvidence diagnostics[capacity -> written]);
```

[R6 원형](../../../../design/70_C_INTERFACE/00_HEADER_OWNERSHIP_MAP.md#r6-input-process)은 논리 전달 계약이며 compile-ready C가 아니다. `OPTIONAL/IN/OUT`, capacity/written의 실제 인자 배치·폭·주소 검증 수단은 미정이다. [R2 원형](../../../../design/30_FUNCTIONS/20_INPUT_STORE_FUNCTIONS.md#input-process)보다 INPUT 발견 진단의 독립 OUT을 명시한다. 새로운 통신 API·통합 반환 타입을 정의하지 않는다.

| 전달 | 필요한 의미·유효성·수명 |
|---|---|
| `input` / [VssInputEvidence](../../../../design/40_DATA/10_INPUT_DATA.md#vssinputevidence) | NULL 허용. 존재하면 값·원 Meta·PRODUCT/TEST 출처·세대/순서·원 시간 문맥을 호출 동안 불변 borrow. 현재 값이나 수신 시각으로 빠진 원본을 합성하지 않음 |
| `time` / [VssTimeEvidence](../../../../design/40_DATA/10_INPUT_DATA.md#vsstimeevidence) | 동기 읽기 근거. domain/epoch/continuity와 가능한 시각 구간을 사용. 부족하면 시간 의존 판단을 확정하지 않음 |
| `diagnostics` / [VssDiagnosticEvidence](../../../../design/40_DATA/70_DIAGNOSTIC_DATA.md#vssdiagnosticevidence) | INPUT이 실제 발견한 원인·대상·시점의 typed 원사실만. 공간 안전 확인 후 written 초기화/기록; capacity 부족은 owner 미인계 보호·손실 관측과 후속으로 남김 |

반환 [VssInputApplication](../../../../design/40_DATA/10_INPUT_DATA.md#vssinputapplication)의 세 필드는 다음처럼 독립적이다.

| 필드 | 값·사후조건 |
|---|---|
| `validation` | APPLICABLE: 정상 적용 가능, QUALITY_CHANGE: 최신 품질 변화, REJECTED: 부적합/과거 자료, NO_CHANGE: 적용 변화 없음. 실제 enum은 의사코드에 표시 |
| `contextApplied` | INPUT의 출처·순서·품질 문맥 반영이 완료됐을 때 true. STORE 거부에도 true일 수 있음 |
| `storeAdmission` | 호출한 CORE-03의 실제 결과를 그대로 연결. 미호출은 `VSS_STORE_NOT_SUBMITTED`; validation으로 ACCEPTED 합성 금지 |

<a id="ownership"></a>
## Owner와 호출 경계

FLOW가 호출한다. INPUT만 [VssInputContext](../../../../design/40_DATA/10_INPUT_DATA.md#vssinputcontext)의 authority/generation/ordering/품질 및 원 근거를 쓴다. Callee는 [D2 승인 Store_ApplyInput](../../STORE/functions/Store_ApplyInput.md#contract)과 필요한 기존 Runtime 경계다. STORE의 occurrence·Hold·Rear, PLAYBACK Session/Attempt, HEALTH Fault는 INPUT이 변경하지 않는다. 보고 연결은 기존 INPUT/Node Communication 책임이며 이 함수가 CAN 송신 결과나 Domain 수용을 반환하지 않는다.

<a id="validation"></a>
## 정상 값, 과거 자료, PRODUCT와 TEST

1. 입력 저장공간과 값+Meta 결합, 출처 authority·세대/순서의 최신 적용 가능성을 확인한다. 과거 INVALID/늦은 source 자료는 REJECTED이며 최신 품질·출처 적용 흔적을 덮지 않는다. 같은 메시지라는 추정만으로 새 occurrence나 최신성을 만들지 않는다.
2. WINDOW PRODUCT 연결은 `[구현 보류 — 설계 유지]`로 거부한다. 격리 TEST는 원 TEST 출처/시간 문맥을 보호하여 기존 의미 검증을 계속한다. TEST를 PRODUCT authority로 승격하지 않는다.
3. 정상 사건/상태의 유효한 조합을 검증한다. One-shot은 Domain이 확정한 원 occurrence를 필요로 한다. 현재 도어 상태 반복이나 목표 이미 도달은 새 LOCK/UNLOCK 사건의 대용이 아니다. Stateful/Rear의 허용 값 조합은 D2 계약을 따른다.
4. 원본 시각과 연속성·정의된 validUntil을 대조한다. 비교 부족은 정상 fresh로 확정하지 않는다. 먼저 보호 가능한 INPUT 문맥을 반영한 뒤 typed EVENT 또는 STATE lane 하나로 CORE-03을 호출한다.
5. STORE가 자원 부족으로 REJECTED를 반환해도 이미 반영한 INPUT 세대/순서/품질을 rollback하지 않는다. source 문맥과 개별 STORE 수용은 서로 다른 사실이다.

<a id="quality"></a>
## RX 없는 품질과 상실

NULL이면 새 EVENT/STATE/CLEAR를 만들지 않는다. 기존 보호 문맥을 점검하고 현재 적용 가능한 신뢰 상실이 확인된 경우 `VssQualityChange`만 전달한다. 최신 INVALID 또는 기한에 따른 STALE은 품질-only lane이다. 기존 `VSS_INPUT_VALID/INVALID/STALE/UNASSESSED`를 사용하며 NO_DATA라는 새 enum을 만들지 않는다.

처음 상실의 원 basis/lossAt를 보호한다. 반복 상실의 처리 now를 최초 시각으로 바꾸지 않는다. 같은 품질의 반복 점검에 적용 변화가 없으면 NO_CHANGE/NOT_SUBMITTED다. 시간 부족/아직 미평가라면 정상값·정밀 lossAt를 합성하지 않고 부족 근거를 유지한다. Hold 기산·최종 후보 제거는 STORE의 책임이다. 이 함수가 CLEAR를 생성해 STORE의 마지막 유효 경고를 없애지 않는다.

<a id="diagnostics"></a>
## OUT 안전성과 참조 보호

진단군 주소·capacity 전달영역·written 저장공간을 안전 확인하고 필요한 capacity 값과 출력범위를 검증한 **이후**에만 그 군의 written=0을 쓴다. 무효 군은 메모리 접근/초기화/기록을 하지 않는다. 유효 capacity=0 또는 가득 찬 군에서도 written을 증가시키지 않는다. 원 발견 사실은 INPUT의 기존 보호 문맥에 미인계로 남기고 FLOW의 후속 관측으로 연결한다. 보호 자체가 불가능한 실제 손실은 해당 범위의 손실 근거를 남기며 충분한 전달로 주장하지 않는다.

OUT 실패는 진단 완료/통지 ack가 아니다. 독립적으로 안전한 INPUT 검증·STORE 호출은 계속할 수 있다. 단, 필요한 입력/문맥 보호까지 실패하면 해당 신규 적용을 거부한다. Meta의 optional validUntil 및 내부 참조를 후속 수명까지 보호하며 얕은 복사를 장기 보호로 간주하지 않는다. 직렬화와 저장 방식은 [R6/TBD](../../../../design/70_C_INTERFACE/00_HEADER_OWNERSHIP_MAP.md#prototype-notation)에 남긴다.

<a id="feasibility"></a>
## 구현 가능 slice와 정확한 Gate

| 상태·근거 | 후속 독립 Host 입력/기대·실패 및 제품 종속성 |
|---|---|
| F1_LOCKED_SEMANTIC / [R5 INPUT](../../../../design/60_PSEUDOCODE/20_INPUT_TO_SELECTION.md#input-process) | NULL·과거 INVALID·VALID EVENT+STORE REJECTED·quality-only를 주입하여 NO_CHANGE/REJECTED/contextApplied/typed lane 대조. TEST_NOT_RUN |
| F2_PROVISIONAL_TESTABLE / [Timing](../../../../design/50_CONTRACTS/30_TIMING_FRESHNESS.md) | 비교 가능/불가 원 시간·validUntil 경계 fixture. 후보 시간 표현에 의존하는 부분만 잠정이며 실제 timestamp 해상도 승인 아님 |
| F3_PARTIAL_INDEPENDENT / [Ownership](../../../../design/50_CONTRACTS/10_OWNERSHIP_LIFETIME.md#borrow-and-protect) | 무효 written·capacity=0·공간 고갈과 원문 참조 보호 실패를 분리. 무접근·미인계/손실 유지·다른 안전한 논리 진행을 검사 |
| F4_EXTERNAL_GATE / [Binding identity](../../../../design/90_BINDING/91_IMPLEMENTATION_TBD.md#identity) | 제품 Node Communication DTO/Meta/세대·순서·시간 변환의 실제 연결만 미확정. Network PR #68·500ms는 제안이며 새 확정 규격 아님 |
| F5_PHYSICAL_SAFETY_GATE / [HW TBD](../../../../design/90_BINDING/91_IMPLEMENTATION_TBD.md#hardware) | INPUT의 논리 검증 자체는 물리 actual/DMA 충분성에 의존하지 않는다. 반환을 실제 출력·보드 성공으로 해석하는 제품 검증은 HW_UNVERIFIED |
| F6_DEFERRED_SCOPE / [INPUT Module](../../../../design/20_MODULES/20_INPUT_MODULE.md) | WINDOW 제품 연결 보류, TEST 유지. D3-2·실제 firmware/Host 실행은 이번 범위 밖 |

실제 소스 매핑은 BINDING_TBD다. 보관 브링업 `src/AudioAppControl.c:255`의 private `AudioAppControl_ProcessVss`와 `VssAudioSession_SubmitRequest`의 single-pending slot은 관측했지만, 보관 원본 108개 중 C/H 검색 범위에서 이 논리 함수 심볼은 찾지 못했다. 외부 참고를 실제 PRODUCT INPUT 구현으로 주장하지 않는다. 실제 ABI 확정 때 INPUT private adapter/시간 비교 경계만 대응하고 위 의미 분기를 유지한다. 후보가 부적합하면 그 adapter·후보 표현만 재검토하며 동결 계약이나 D2를 바꾸려면 별도 CR이 필요하다.

위 코드 조각의 정적 관측만 SOURCE_CONFIRMED, 동결 의미 근거는 LOGICAL_APPROVED, 외부 IF는 PROPOSED_IF다. 원본 보관 위치는 Git 제외 `.vss_work/reference/bringup_20261007/`이고 읽은 소스 SHA는 로컬 `evidence/VSD_D3_1_SOURCE_MAP.json`과 검수 ZIP에 기록한다. GitHub 실소스 클릭 링크나 제품 승인으로 제시하지 않는다.

[R2](../../../../design/30_FUNCTIONS/20_INPUT_STORE_FUNCTIONS.md#input-process) · [R3 자료](../../../../design/40_DATA/10_INPUT_DATA.md#vssinputapplication) · [R4 시간](../../../../design/50_CONTRACTS/30_TIMING_FRESHNESS.md) · [R5 흐름](../../../../design/60_PSEUDOCODE/20_INPUT_TO_SELECTION.md#input-process) · [R6](../../../../design/70_C_INTERFACE/00_HEADER_OWNERSHIP_MAP.md#r6-input-process) · [실소스 Mapping 수준](../../../../design/90_BINDING/90_C_FILE_API_MAPPING.md)
