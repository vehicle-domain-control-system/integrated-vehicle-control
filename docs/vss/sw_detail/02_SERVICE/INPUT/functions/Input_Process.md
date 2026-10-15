# Input_Process — 검증 문맥 적용과 STORE 전달

> A1 입력 개정 2026-10-15 · [현재 Network 연결](../../../../design/90_BINDING/92_NETWORK_BINDING.md). `L=min(양수 USE_LIMIT,500ms,2000ms)`; 최초 actual에만 적용. 과거 동결/PASS는 당시 범위의 이력이며 본문 변경은 이번 A1 결정이다. 구현·시험 NOT_RUN.

[VSS 지도](../../../01_VSS_MAP.md) · [INPUT](../00_MODULE_OVERVIEW.md) · CORE-02 · [한국어 의사코드](../pseudocode/Input_Process.md)

> GUIDE · 의미 기준 `0641b2c2268cf47450aeb50302638a45a7a3277f`. TEST_NOT_RUN / BINDING_TBD / HW_UNVERIFIED.

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

1. 입력 저장공간과 값+Meta 결합, 출처 authority·세대/순서·원 시간의 적용 가능성을 확인한다. 과거/부적합 메시지의 정상 사건·상태 적용은 거부한다. 다만 [R5](../../../../design/60_PSEUDOCODE/20_INPUT_TO_SELECTION.md#input-process)처럼 **현재 문맥에 적용 가능한 최신 품질 상실 근거가 별도로 검증되는 경우에만** 그 근거의 QUALITY_CHANGE 경로를 허용한다. 독립 근거가 없으면 REJECTED/NOT_SUBMITTED로 기존 문맥을 유지한다. 과거 INVALID 자체로 최신 품질·출처를 덮거나 같은 메시지라는 추정으로 새 occurrence·최신성을 만들지 않는다.
2. WINDOW PRODUCT 연결은 `[구현 보류 — 설계 유지]`로 거부한다. 격리 TEST는 원 TEST 출처/시간 문맥을 보호하여 기존 의미 검증을 계속한다. TEST를 PRODUCT authority로 승격하지 않는다.
3. 정상 사건/상태의 유효한 조합을 검증한다. One-shot은 Domain이 확정한 원 occurrence를 필요로 한다. 현재 도어 상태 반복이나 목표 이미 도달은 새 LOCK/UNLOCK 사건의 대용이 아니다. Stateful/Rear의 허용 값 조합은 D2 계약을 따른다.
4. 원본 시각과 연속성·정의된 validUntil을 대조한다. 비교 부족은 정상 fresh로 확정하지 않는다. 먼저 보호 가능한 INPUT 문맥을 반영한 뒤 typed EVENT 또는 STATE lane 하나로 CORE-03을 호출한다.
5. STORE가 자원 부족으로 REJECTED를 반환해도 이미 반영한 INPUT 세대/순서/품질을 rollback하지 않는다. source 문맥과 개별 STORE 수용은 서로 다른 사실이다.

<a id="quality"></a>
## RX 없는 품질과 상실

NULL이면 새 EVENT/STATE/CLEAR를 만들지 않는다. 기존 보호 문맥을 점검하고 현재 적용 가능한 신뢰 상실이 확인된 경우 `VssQualityChange`만 전달한다. 부적합한 정상 입력에도 현재 보호 문맥의 대상·PRODUCT/TEST 격리·출처/원 시간에 대해 독립 검증된 최신 상실 근거가 있으면 그 quality lane만 연결한다. 이 예외는 품질/검증 문맥만 반영하고 과거 메시지의 값·Meta·generation/ordering을 현재 유효 출처나 새 발생으로 채택하지 않는다. 최신 INVALID 또는 기한에 따른 STALE은 품질-only lane이다. 기존 `VSS_INPUT_VALID/INVALID/STALE/UNASSESSED`를 사용하며 NO_DATA라는 새 enum을 만들지 않는다.

처음 상실의 원 basis/lossAt를 보호한다. 반복 상실의 처리 now를 최초 시각으로 바꾸지 않는다. 같은 품질의 반복 점검에 적용 변화가 없으면 NO_CHANGE/NOT_SUBMITTED다. 시간 부족/아직 미평가라면 정상값·정밀 lossAt를 합성하지 않고 부족 근거를 유지한다. Hold 기산·최종 후보 제거는 STORE의 책임이다. 이 함수가 CLEAR를 생성해 STORE의 마지막 유효 경고를 없애지 않는다.

<a id="diagnostics"></a>
## OUT 안전성과 참조 보호

1. 진단군 주소·capacity 전달영역·written 저장공간을 안전 확인하고 필요한 capacity 값과 출력범위를 검증한 **이후**에만 의사 접근 `diagnostics.written = 0`을 수행한다.
2. 유효 범위 내에서 진단 기록과 출력 수명에 필요한 참조 보호가 실제 완료된 건에 한해 **그 다음** `diagnostics.written = diagnostics.written + 1`로 계수한다.
3. 무효 군은 메모리 접근/초기화/기록을 하지 않는다. capacity=0/가득 참·인계 근거 무효·기록 또는 참조 보호 실패는 증가 없이 미인계로 유지한다.
4. 원 발견 사실은 INPUT의 기존 보호 문맥에 남기고 FLOW의 후속 관측으로 연결한다. 보호 자체가 불가능한 실제 손실은 해당 범위의 손실 근거를 남기며 충분한 전달로 주장하지 않는다. 이 표기는 실제 C 필드·포인터 ABI를 확정하지 않는다.

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
| F6_DEFERRED_SCOPE / [INPUT Module](../../../../design/20_MODULES/20_INPUT_MODULE.md) | WINDOW 제품 연결 보류, TEST 유지. 실제 firmware/Host 실행은 별도 범위 |

실제 소스 매핑은 BINDING_TBD다. 보관 브링업의 private `AudioAppControl_ProcessVss`와 `VssAudioSession_SubmitRequest`의 single-pending slot에 대한 정적 관측만 SOURCE_CONFIRMED이며, 논리 `Input_Process` 심볼은 보관 C/H에서 미확인이다. 외부 참고를 실제 PRODUCT INPUT 구현으로 주장하지 않는다. 실제 ABI 확정 때 INPUT private adapter/시간 비교 경계만 대응하고 위 의미 분기를 유지한다. 후보가 부적합하면 그 adapter·후보 표현만 재검토하며 동결 계약이나 D2를 바꾸려면 별도 CR이 필요하다. 동결 의미 근거는 LOGICAL_APPROVED, 외부 IF는 PROPOSED_IF다.

[R2](../../../../design/30_FUNCTIONS/20_INPUT_STORE_FUNCTIONS.md#input-process) · [R3 자료](../../../../design/40_DATA/10_INPUT_DATA.md#vssinputapplication) · [R4 시간](../../../../design/50_CONTRACTS/30_TIMING_FRESHNESS.md) · [R5 흐름](../../../../design/60_PSEUDOCODE/20_INPUT_TO_SELECTION.md#input-process) · [R6](../../../../design/70_C_INTERFACE/00_HEADER_OWNERSHIP_MAP.md#r6-input-process) · [실소스 Mapping 수준](../../../../design/90_BINDING/90_C_FILE_API_MAPPING.md)

## A1 통신 연결과 no-RX

기존 VssApp callback을 FLOW와 같은 직렬화 owner 기회에서 동기 소비한다. Event1개 또는 Warning 최대4개(activation→risk→occupant→anti)를 [단일 연결 명세](../../../../design/90_BINDING/92_NETWORK_BINDING.md#owner)로 변환하고 bundle 뒤 한 번 재중재한다. 부분 실패로 독립 정상 반영을 rollback하지 않는다. callback void/INPUT contextApplied/STORE admission/actual은 별도 완료점이다.

원 계보·DOMAIN authority·전달 순서, 동일 순번의 최신 상실·옛 INVALID 거부, NO_DATA 초기/상실 구별과 format0은 연결 명세 §2~5를 적용한다. 같은 근거의 정상값 변경은 E01 제한이며 TX를 새 평가로 대입하지 않는다. INPUT(NULL)은 최대18 문맥을 입력 종류별로 검사한다. 주기 Warning에만 원 validUntil와 증가 정상RX+150ms를 적용하고, 비주기 Event는 원 L 및 별도 DOMAIN 감독·명시 신뢰 상실 근거를 구별한다. 현재 적용 가능한 각 상실만 quality lane으로 전달한다. 진단16이 가득 차도 손실 latch/count·첫/최근 요약을 보호하며 전체 성공으로 보고하지 않는다.

A1 no-RX 구분: 150ms RX 감시는 주기 Warning의 승인된 monitor에만 적용한다. 비주기 Event 자체의 마지막 RX+150ms를 timeout으로 쓰지 않는다. Event는 원 L 만료를 기회 종료로 처리하고, 입력 신뢰 상실은 별도 DOMAIN Warning/Permission 감독·명시 품질/세대/시간 근거에서 판단한다. Permission의 감시 주기는 그 원 계약을 유지한다.

<a id="a2-status-handoff"></a>
## A2 기존 INPUT 통신 연결의 Status 책임

[Status Binding §5](../../../../design/90_BINDING/93_NETWORK_STATUS_BINDING.md#handoff)를 적용한다. CREATED만 기존 INPUT 통신 연결 한 곳에서 NetVssStatus로 변환한다. reportGeneration/reportSequence/evaluatedAt과 last Fault 원근거를 보존하고 Network 동기 전체 복사 반환까지 불변 참조를 보호한다. INVALID는 원 진단/재수집, NOT_READY는 준비 변화, NO_CAPACITY는 다음 기존 처리 기회에서 최대1회 재검토다. 아직 미접수 1건만 더 새 coherent 평가로 교체할 수 있으며 accepted/Driver 사본은 수정하지 않는다.

localOK 뒤 terminal 실패는 원 key·BOOT/TX·raw·참조 종료 여부를 Network 기존 관측 → INPUT 진단16/loss latch → FLOW로 bounded drain한다. sink 미준비는 접수 NOT_READY다. 실패를 CAN 수신/DOMAIN 성공이나 음향 Fault로 바꾸지 않는다. 미인계/실패 관측/재수집이 남으면 FOLLOW_UP_REQUIRED/RECOLLECT이며 보고 문제로 독립 입력·정리 처리를 중단하지 않는다. 실제 함수/게시/sink 바인딩은 E-S01~04이고 신규 Core/제품 코드는 없다.

이 Status 연결은 Input_Process의 입력 R6 원형/반환을 변경하지 않는다. HEALTH 정책을 INPUT에서 재평가하지 않고 의미 대응·범위/age·전체 복사만 담당한다.
