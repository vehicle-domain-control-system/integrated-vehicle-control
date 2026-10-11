# VSS / SERVICE / STORE

[상위 Layer](../00_LAYER_OVERVIEW.md) · [VSS 지도](../../01_VSS_MAP.md) · [문서 양식·확인 수준](../../00_START_HERE.md)

> GUIDE · VSD-D1 탐색 문서 · GD1 독립 검수 대기. 설계 정본: `VSS@0641b2c2268cf47450aeb50302638a45a7a3277f`.

수용한 One-shot occurrence의 이력·replay 방어와 현재 Stateful 후보를 구분해 보존합니다. 검증 입력과 원래 발생에 귀속된 재생 사실을 적용하고 품질·Hold·Rear 결합과 기한을 관리합니다.

## 상태 owner와 입출력

STORE만 occurrence ledger·원본 age·replay guard와 Stateful 최신 판단/현재 품질/Hold·Rear 후보를 씁니다. PLAYBACK의 Session/Attempt, INPUT의 출처 authority, POLICY 선택과 HEALTH Fault는 다른 owner입니다.

| 구분 | 주요 의미 |
|---|---|
| 입력 | INPUT의 검증된 사건·상태·품질, PLAYBACK의 원귀속 재생 사실, 비교 가능한 시간·최신 출력 관측 |
| 출력 | 수용/duplicate/rejected 결과, 후보·기한·품질 관측과 발생 이력 |
| 의존 | INPUT·FLOW·PLAYBACK → STORE → 필요한 RUNTIME; POLICY/HEALTH 관측은 FLOW가 연결 |

## 인접 관계와 비동기 경계

accepted 전에 최종까지의 추적 자원을 확보합니다. 동일 occurrence 재전달은 새 Pending·age가 아니며, 신규 공간 부족은 기존 accepted/final을 지우는 근거가 아닙니다. 유효 CLEAR와 INVALID/STALE/NO_DATA를 구분하고, 반복 품질 상실로 Hold 기산을 연장하지 않습니다. 실제 시작 사실과 전체 최종 결과는 원귀속으로 멱등 적용합니다.

[INPUT 검증](../INPUT/00_MODULE_OVERVIEW.md) · [PLAYBACK Session/Attempt](../PLAYBACK/00_MODULE_OVERVIEW.md)

## 담당 Core의 문서쌍

| Core | 함수 / 역할 | 설명 예정 경로 | 의사코드 예정 경로 | 작성 상태 |
|---|---|---|---|---|
| CORE-03 | `Store_ApplyInput` — 동기 신규 수용·현재 후보/품질 적용 | [상세 설명](functions/Store_ApplyInput.md) | [한국어 의사코드](pseudocode/Store_ApplyInput.md) | SELF_CHECKED / GD2_PENDING |
| CORE-04 | `Store_ApplyPlaybackFact` — 동기 원귀속 사실의 멱등 이력 반영 | `02_SERVICE/STORE/functions/Store_ApplyPlaybackFact.md` | `02_SERVICE/STORE/pseudocode/Store_ApplyPlaybackFact.md` | PLANNED_NOT_WRITTEN |
| CORE-05 | `Store_AdvanceDeadlines` — 최신 출력 사실 재확인 후 age/Hold 진행 | `02_SERVICE/STORE/functions/Store_AdvanceDeadlines.md` | `02_SERVICE/STORE/pseudocode/Store_AdvanceDeadlines.md` | PLANNED_NOT_WRITTEN |

CORE-03은 D2 시범 작성 문서로 진입할 수 있습니다. CORE-04/05의 경로는 `docs/vss/sw_detail/` 기준 미작성 비링크입니다. 작성 문서와 아래 동결 Function 원문을 함께 읽으며, 다른 함수쌍은 각 단계의 별도 지시·검수를 따릅니다.

## 후속 진행 후보와 미정

중복·기존 결과 보존·quality/Hold 기산·원 시간 비교는 독립 SW/Host 후보입니다. Hold 기간·추적 용량·실제 identity/ABI는 미정이고, 최초 actual age의 <2초는 잠정 조건입니다. Network 500ms 제안과 통합하지 않습니다. D2는 Store_ApplyInput 시범, D3에서 나머지 쌍을 이어 작성합니다.

위 후보는 `TEST_NOT_RUN`이며 실제 코드/Host 실행은 별도 지시·범위·Gate가 필요합니다. 미해결 Q↔Core 관계는 P1B_C1 독립 검수 전 최종 차단 지도로 채택하지 않습니다.

## 현재 상세 근거

- [기존 STORE Module의 책임·lifecycle](../../../design/20_MODULES/21_STORE_MODULE.md)
- [R2 담당 함수·Caller/Callee](../../../design/30_FUNCTIONS/20_INPUT_STORE_FUNCTIONS.md)
- [R3 관련 자료·writer·수명](../../../design/40_DATA/20_STORE_DATA.md)
- [관련 R4 공통 계약](../../../design/50_CONTRACTS/30_TIMING_FRESHNESS.md)

논리 원형과 실제 C ABI의 구분은 [공통 문서 작성 기준](../../00_START_HERE.md)에서 R6 원문으로 이어집니다. 이 페이지는 탐색·설명 보조이며 승인 기술 정본을 대체하지 않습니다.
