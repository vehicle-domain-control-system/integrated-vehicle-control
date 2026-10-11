# VSS / SERVICE / POLICY

[상위 Layer](../00_LAYER_OVERVIEW.md) · [VSS 지도](../../01_VSS_MAP.md) · [문서 양식·확인 수준](../../00_START_HERE.md)

> GUIDE · VSD-D1 탐색 문서 · GD1 독립 검수 대기. 설계 정본: `VSS@0641b2c2268cf47450aeb50302638a45a7a3277f`.

FLOW가 연결한 일관된 관측에서 후보·현재 출력·제한을 비교해 winner/keep/replace/wait와 전체 계획을 반환합니다. 의미·우선순위·Asset ID·패턴의 선택과 실행을 구분합니다.

## 상태 owner와 입출력

POLICY는 실행 중 읽기 전용 중앙 정책과 한 평가의 판단 자료를 소유합니다. STORE·PLAYBACK·Fault 원본은 수정하지 않으며 채택된 전체 계획의 진행 상태는 PLAYBACK이 소유합니다.

| 구분 | 주요 의미 |
|---|---|
| 입력 | FLOW가 제공하는 현재 후보·출력 owner·Backend/HEALTH 제한·비교 가능한 시간 |
| 출력 | winner/keep/replace/wait와 해당 의미의 Asset ID·전체 계획 |
| 의존 | FLOW → POLICY → 필요한 RUNTIME; POLICY → PLAYBACK 직접 실행 호출 없음 |

## 인접 관계와 비동기 경계

winner는 준비·수락·actual start가 아닙니다. 실행 제한이나 old 정리가 있으면 선택 결과와 대기를 구분합니다. 동일 의미의 미시작 One-shot은 원본 발생 순서/identity로 비교하며 수신 순서를 대신 쓰지 않습니다. 다른 의미의 음원으로 fallback하지 않습니다.

[FLOW 조율](../../01_APP/FLOW/00_MODULE_OVERVIEW.md) · [PLAYBACK 실행](../PLAYBACK/00_MODULE_OVERVIEW.md)

## 담당 Core의 예정 문서쌍

| Core | 함수 / 역할 | 설명 예정 경로 | 의사코드 예정 경로 | 작성 상태 |
|---|---|---|---|---|
| CORE-06 | `Select_Choose` — 동기 읽기 전용 선택·계획 반환 | `02_SERVICE/POLICY/functions/Select_Choose.md` | `02_SERVICE/POLICY/pseudocode/Select_Choose.md` | PLANNED_NOT_WRITTEN |

경로는 `docs/vss/sw_detail/` 기준입니다. 설명·의사코드 본문은 아직 없으므로 위 경로는 클릭 링크가 아닙니다. 실제 동작은 아래 동결 Function 원문에서 읽고, 새 함수쌍은 각 단계 검수 뒤 작성합니다.

## 후속 진행 후보와 미정

읽기 전용 선택·동률/원순서·제한에 따른 대기 판단은 독립 SW/Host 후보입니다. 기존 세부 순위는 [잠정]이고 cue 수·무음·반복·출력 수준은 미정입니다. 이를 새 rank 숫자로 바꾸지 않습니다. WINDOW 제품 보류는 INPUT 경계를 따르며 함수쌍은 D3 예정입니다.

위 후보는 `TEST_NOT_RUN`이며 실제 코드/Host 실행은 별도 지시·범위·Gate가 필요합니다. 미해결 Q↔Core 관계는 P1B_C1 독립 검수 전 최종 차단 지도로 채택하지 않습니다.

## 현재 상세 근거

- [기존 POLICY Module의 책임·lifecycle](../../../design/20_MODULES/22_POLICY_MODULE.md)
- [R2 담당 함수·Caller/Callee](../../../design/30_FUNCTIONS/30_POLICY_PLAYBACK_FUNCTIONS.md)
- [R3 관련 자료·writer·수명](../../../design/40_DATA/30_SELECTION_DATA.md)
- [관련 R4 공통 계약](../../../design/50_CONTRACTS/30_TIMING_FRESHNESS.md)

논리 원형과 실제 C ABI의 구분은 [공통 문서 작성 기준](../../00_START_HERE.md)에서 R6 원문으로 이어집니다. 이 페이지는 탐색·설명 보조이며 승인 기술 정본을 대체하지 않습니다.
