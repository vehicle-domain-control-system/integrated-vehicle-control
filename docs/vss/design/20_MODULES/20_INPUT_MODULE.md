# INPUT Module — 의미 입력 검증

> R1 설계 의미 유지 · R1.1 한국어 표현 교정 · 2026-10-07

[Module Overview](00_MODULE_OVERVIEW.md) · [상위 Layer](../10_LAYERS/20_SERVICE_LAYER.md)

## 1. 목적

제품/TEST 입력을 현재 적용 가능한 VSS 의미와 검증 근거로 STORE에 전달한다.

## 2. 역할

INPUT(C02~C04)는 제품/TEST 변환과 값·Meta·품질·원본 시간·출처/Generation/Ordering 검증을 맡는다. 보고의 기존 Network 연결 경계도 유지한다.

## 3. 책임

- 제품/TEST 변환 문맥과 검증된 출처·세대·순서·연속성을 반영한다.
- 최신 적용 가능한 품질 상실과 정상 검증 사건을 구별해 STORE에 전달한다.
- HEALTH의 보고 책임(C12)이 생성한 보고 묶음을 기존 Node Communication에 연결한다.

## 4. 책임이 아닌 것

accepted 후보와 occurrence 이력의 owner는 STORE다. CAN ID·payload·전송 주기·Network TX lifecycle은 기존 Network 책임이다. 보고값은 HEALTH가 파생하며 INPUT은 독자 계산하지 않는다. Network TX/context pending은 occurrence Pending과 다르다.

## 5. 소유 상태 / 데이터

제품/TEST 변환·검증 문맥과 검증된 출처·세대·순서·연속성의 단일 writer는 INPUT이다. STORE 수용/이력, PLAYBACK 출력, HEALTH의 보고 책임(C12)이 생성한 보고의 원본을 소유하지 않는다.

## 6. 입력

기존 10 D2V / 10 V2D와 `VSS_EVENT` / `VSS_WARNING_STATE` / `VSS_STATUS` 경계를 사용한다. Node Communication의 값+Meta, 격리 TEST, 입력 신뢰성 기한/연속성 변화와 HEALTH의 보고 책임(C12)이 생성한 보고 묶음을 받는다.

## 7. 출력

| 입력 | 처리 결과 |
| --- | --- |
| Node Communication의 값+Meta / 격리 TEST | received → 의미·품질·시간·순서 검증 → valid/최신 품질 상실/거부 |
| 적용 가능한 출처 전환·순서 | INPUT의 출처·세대·순서 문맥(C04) 반영 → STORE에 검증 근거 전달 |
| valid 사건 / Stateful·품질 변화 | STORE 수용 또는 상태 갱신 요청 → accepted/duplicate/rejected 등 결과 |
| 입력 신뢰성 기한·연속성 변화 | RX 없이도 최신 품질 상실 근거 전달. CLEAR나 새 수신 시각을 만들지 않음 |
| HEALTH의 보고 책임(C12)의 보고 묶음 | 기존 Node Communication 접수/전달 연결. 보고값을 독자 계산하지 않음 |

## 8. 사용하는 / 제공하는 주요 Core Function — R2

[Input_Process](../30_FUNCTIONS/20_INPUT_STORE_FUNCTIONS.md#input-process)

현재 Core는 [Input_Process](../30_FUNCTIONS/20_INPUT_STORE_FUNCTIONS.md#input-process)다. RX와 RX 없는 품질 변화는 같은 적용 문맥에서 다루며 검증된 의미 입력은 STORE의 [Store_ApplyInput](../30_FUNCTIONS/20_INPUT_STORE_FUNCTIONS.md#store-applyinput)으로 연결한다.

## 9. 의존 Module

FLOW → INPUT → STORE / 필요한 RUNTIME이다. 기존 Node Communication과는 의미 입력·보고 전달 경계로 연결한다. STORE 결과는 개별 수용 결과이며 INPUT의 출처 authority를 대신하지 않는다.

## 10. 상태 전이 또는 주요 lifecycle

received → 출처/품질/시간/순서 검증 → 적용 가능한 INPUT 문맥 반영 → STORE 요청 → 수용 결과 반환의 흐름이다. 수신 자료는 검증/전달 기간 동안, 적용 문맥은 해당 출처·세대가 유효한 동안 유지한다. RX 없이도 신뢰성 변화는 진행한다.

## 11. Module-local Contract

received는 도착 사실, valid는 검증 결과다. accepted는 [STORE의 추적 자원 확보](21_STORE_MODULE.md#occurrence)까지 완료한 결과이며 입력 검증만으로 성립하지 않는다. 개별 수용이 실패해도 검증된 출처/순서 반영을 되돌리거나 기존 이력을 삭제하지 않는다.

최신 INVALID는 품질 상실 근거다. 과거 세대/순서의 INVALID는 현재 품질을 덮어쓰지 않는다. PRODUCT와 TEST의 출처·저장·시간·실행·이력은 격리하며 TEST도 같은 검증/정책/재생 경로를 거친다. **WINDOW / Anti-Pinch는 [구현 보류 — 설계 유지]**이며 제품 source/ECU 연결은 보류하고 TEST 의미 경로를 유지한다. 이 보류 자체는 Fault/DEGRADED 근거가 아니다.

## 12. 대표 오류 / 예외 처리 원칙

- 값·Meta·시간/출처 검증 실패: 부적합한 정상 사건을 수용하지 않는다. 현재 품질 상실은 별도 근거로 전달한다.
- 과거 입력/같은 발생 재전달: 현재 판단을 덮지 않으며 동일 발생 처리는 STORE의 replay 기준을 따른다.
- STORE 신규 수용 거부: 입력 문맥을 rollback하지 않는다.

## 13. 관련 Function 문서

[Input_Process](../30_FUNCTIONS/20_INPUT_STORE_FUNCTIONS.md#input-process) · [기존 후보의 R2 판정](../30_FUNCTIONS/00_FUNCTION_OVERVIEW.md#candidate-decisions) · [이전 명칭의 의미 연결](../30_FUNCTIONS/00_FUNCTION_OVERVIEW.md#merged).

## 14. 관련 Data 문서

[Input Context / Meta](../40_DATA/10_INPUT_DATA.md#input) · [파생 보고](../40_DATA/70_DIAGNOSTIC_DATA.md#status)

## 15. 관련 공통 Contract

[Timing](../50_CONTRACTS/00_CONTRACT_OVERVIEW.md#timing) · [Ownership](../50_CONTRACTS/00_CONTRACT_OVERVIEW.md#ownership)

공통 원문의 위치를 연결한다. 여러 Module에 동일한 규칙의 통합·중복 제거는 R4에서 수행한다.

## 16. TBD / Deferred

기존 Network의 실제 Meta 전달 형태와 source/시간 표현은 [입력·identity TBD](../90_BINDING/91_IMPLEMENTATION_TBD.md#identity)를 확인한다.

WINDOW / Anti-Pinch의 제품 source/ECU 연결은 [구현 보류 — 설계 유지]다. 실제 Meta 표현·필드/prototype은 후속 R2/R3 및 Binding에서 확인한다.
