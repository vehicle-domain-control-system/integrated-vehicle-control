# VSS / SERVICE / POLICY

[상위 Layer](../00_LAYER_OVERVIEW.md) · [VSS 지도](../../01_VSS_MAP.md) · [문서 양식·확인 수준](../../00_START_HERE.md)

> GUIDE · 설계 정본: `VSS@0641b2c2268cf47450aeb50302638a45a7a3277f`.

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

## 연결해서 읽기

[D3 의미 흐름](../../01_VSS_MAP.md#d3-semantic-flow)의 [Select_Choose](functions/Select_Choose.md#return)는 판단을 [FLOW](../../01_APP/FLOW/functions/Flow_Process.md#coherence)에 반환하고 FLOW가 [Playback_RequestTransition](../PLAYBACK/functions/Playback_RequestTransition.md#actions)에 연결합니다. POLICY는 PB를 직접 호출하거나 실행 허가·actual을 생성하지 않습니다.

## 담당 Core의 문서쌍

| Core | 함수 / 역할 | 상세 설명 | 한국어 의사코드 |
|---|---|---|---|
| CORE-06 | `Select_Choose` — 동기 읽기 전용 선택·계획 반환 | [상세 설명](functions/Select_Choose.md) | [한국어 의사코드](pseudocode/Select_Choose.md) |

위 링크에서 R6 논리 계약 기반 상세 설명과 한국어 C 형태 의사코드를 왕복합니다. 기술 의미는 아래 동결 원문을 따릅니다.

## 후속 진행 후보와 미정

읽기 전용 선택·동률/원순서·제한에 따른 대기 판단은 독립 SW/Host 후보입니다. 기존 세부 순위는 [잠정]이고 cue 수·무음·반복·출력 수준은 미정입니다. 이를 새 rank 숫자로 바꾸지 않습니다. WINDOW 제품 보류는 INPUT 경계를 따르며 기존 함수쌍은 위 링크에서 읽습니다.

위 후보는 `TEST_NOT_RUN`이며 실제 코드/Host 실행은 별도 지시·범위·Gate가 필요합니다. 미해결 Q/R6·외부 IF/HW에 종속된 제품 결합과 P2는 계속 HOLD입니다. 문서 관계 보정의 승인 범위를 원 G1-B 원본 복구나 실제 제품 C1 완료로 확대하지 않습니다.

## 현재 상세 근거

- [기존 POLICY Module의 책임·lifecycle](../../../design/20_MODULES/22_POLICY_MODULE.md)
- [R2 담당 함수·Caller/Callee](../../../design/30_FUNCTIONS/30_POLICY_PLAYBACK_FUNCTIONS.md)
- [R3 관련 자료·writer·수명](../../../design/40_DATA/30_SELECTION_DATA.md)
- [관련 R4 공통 계약](../../../design/50_CONTRACTS/30_TIMING_FRESHNESS.md)

논리 원형과 실제 C ABI의 구분은 [공통 문서 작성 기준](../../00_START_HERE.md)에서 R6 원문으로 이어집니다. 이 페이지는 탐색·설명 보조이며 승인 기술 정본을 대체하지 않습니다.
