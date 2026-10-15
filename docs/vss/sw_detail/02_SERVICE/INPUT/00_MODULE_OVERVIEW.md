# VSS / SERVICE / INPUT

[상위 Layer](../00_LAYER_OVERVIEW.md) · [VSS 지도](../../01_VSS_MAP.md) · [문서 양식·확인 수준](../../00_START_HERE.md)

> GUIDE · 설계 정본: `VSS@0641b2c2268cf47450aeb50302638a45a7a3277f`.

제품/TEST의 값·Meta·출처·세대·순서·품질·원본 시간을 검증해 현재 적용 가능한 VSS 의미로 STORE에 전달합니다. HEALTH가 만든 보고는 기존 Node Communication 경계에 연결합니다.

## 상태 owner와 입출력

INPUT은 검증된 출처·세대·순서·연속성과 변환/검증 문맥의 단일 writer입니다. STORE의 수용/이력이나 HEALTH의 보고값을 독자 계산하지 않습니다.

| 구분 | 주요 의미 |
|---|---|
| 입력 | Node Communication의 값+Meta, 격리 TEST, 신뢰성 기한/연속성 변화, HEALTH 보고 묶음 |
| 출력 | 검증된 사건·상태·최신 품질 상실을 STORE에 전달하고 수용 결과 연결; 보고의 기존 통신 접수/전달 |
| 의존 | FLOW → INPUT → STORE 및 필요한 RUNTIME; 기존 Node Communication 의미 경계 |

## 인접 관계와 비동기 경계

received·valid·STORE accepted는 다릅니다. 신규 수용이 거부돼도 검증된 출처/순서 문맥을 rollback하지 않습니다. RX 없이도 신뢰성 변화를 진행하며 과거 INVALID가 현재 품질을 덮지 않게 합니다. Network TX lifecycle은 INPUT의 occurrence 이력이 아닙니다.

[STORE의 수용 책임](../STORE/00_MODULE_OVERVIEW.md) · [HEALTH의 보고 책임](../HEALTH/00_MODULE_OVERVIEW.md)

## 연결해서 읽기

[D3 의미 흐름](../../01_VSS_MAP.md#d3-semantic-flow)의 [Input_Process](functions/Input_Process.md#quality)는 NULL에서도 품질을 점검하고, 검증한 event/state/quality 중 하나만 [Store_ApplyInput](../STORE/functions/Store_ApplyInput.md#contract)에 전달합니다. contextApplied와 storeAdmission은 별개이므로 STORE 거부가 INPUT 문맥을 rollback하지 않습니다.

## 담당 Core의 문서쌍

| Core | 함수 / 역할 | 상세 설명 | 한국어 의사코드 |
|---|---|---|---|
| CORE-02 | `Input_Process` — 동기 검증·문맥 적용·STORE 전달 | [상세 설명](functions/Input_Process.md) | [한국어 의사코드](pseudocode/Input_Process.md) |

CORE-02의 상세 설명과 의사코드는 CORE-03 연결과 R6 진단 OUT/원 문맥 보호를 아래 동결 원문과 함께 설명합니다.

## 후속 진행 후보와 미정

의미 검증·중복/순서·품질 상실 전달은 [작성한 함수 설명](functions/Input_Process.md#feasibility)의 독립 SW/Host 후보입니다. 실제 Network DTO·Header·시간 변환은 PROPOSED_IF/BINDING_TBD입니다. WINDOW/Anti-Pinch 제품 source/ECU 연결은 [구현 보류 — 설계 유지]; PRODUCT와 격리된 TEST 의미 경로는 유지합니다.

위 후보는 `TEST_NOT_RUN`이며 실제 코드/Host 실행은 별도 지시·범위·Gate가 필요합니다. 미해결 Q/R6·외부 IF/HW에 종속된 제품 결합과 P2는 계속 HOLD입니다. 문서 관계 보정의 승인 범위를 원 G1-B 원본 복구나 실제 제품 C1 완료로 확대하지 않습니다.

## 현재 상세 근거

- [기존 INPUT Module의 책임·lifecycle](../../../design/20_MODULES/20_INPUT_MODULE.md)
- [R2 담당 함수·Caller/Callee](../../../design/30_FUNCTIONS/20_INPUT_STORE_FUNCTIONS.md)
- [R3 관련 자료·writer·수명](../../../design/40_DATA/10_INPUT_DATA.md)
- [관련 R4 공통 계약](../../../design/50_CONTRACTS/30_TIMING_FRESHNESS.md)

논리 원형과 실제 C ABI의 구분은 [공통 문서 작성 기준](../../00_START_HERE.md)에서 R6 원문으로 이어집니다. 이 페이지는 탐색·설명 보조이며 승인 기술 정본을 대체하지 않습니다.
