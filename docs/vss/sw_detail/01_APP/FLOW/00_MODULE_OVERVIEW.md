# VSS / APP / FLOW

[상위 Layer](../00_LAYER_OVERVIEW.md) · [VSS 지도](../../01_VSS_MAP.md) · [문서 양식·확인 수준](../../00_START_HERE.md)

> GUIDE · 설계 정본: `VSS@0641b2c2268cf47450aeb50302638a45a7a3277f`.

각 owner의 처리를 연결해 입력이 없는 동안에도 기한·출력 결과·정리·선택/실행·보고가 진행되게 합니다. 선택은 POLICY, 전체 실행은 PLAYBACK이 맡고 FLOW는 공개 의미 경계와 반영 완료 관측을 조율합니다.

## 상태 owner와 입출력

FLOW가 쓰는 것은 조율 단계·재평가 필요성과 임시 일관된 읽기 묶음입니다. 이 묶음은 다른 owner의 원본을 소비하는 관측이며 별도 상태 원본이 아닙니다.

| 구분 | 주요 의미 |
|---|---|
| 입력 | startup·제품/TEST·시간/알림, 각 owner의 반영 완료 결과와 진단/복구 근거 |
| 출력 | POLICY 판단을 PLAYBACK 요구로 연결; 필요한 Service 진행과 HEALTH 보고 전달 |
| 의존 | FLOW → INPUT/STORE/POLICY/PLAYBACK/AUDIO_STREAM/ASSET/HEALTH 및 필요한 RUNTIME |

## 인접 관계와 비동기 경계

처리 기회는 실제 출력 사실이 아닙니다. 수집 중 관련 상태가 바뀌면 다시 읽고, start/만료 경합에서는 PLAYBACK 사실 반영을 연결합니다. 상태 보고는 HEALTH → FLOW → INPUT의 통신 연결 책임 → 기존 Node Communication입니다. FLOW가 Driver/HAL을 직접 호출하지 않습니다.

[SERVICE 책임](../../02_SERVICE/00_LAYER_OVERVIEW.md)

## 연결해서 읽기

[D3 의미 흐름](../../01_VSS_MAP.md#d3-semantic-flow)의 [INPUT 처리](../../02_SERVICE/INPUT/functions/Input_Process.md)와 [PLAYBACK 원 사실 적용](../../02_SERVICE/PLAYBACK/functions/Playback_Advance.md) 뒤 최신 관측으로 [STORE 기한](../../02_SERVICE/STORE/functions/Store_AdvanceDeadlines.md)을 진행합니다. 선택용 coherent basis와 HEALTH 보고용 새 basis는 분리하고, 반환 하나로 미인계 사실·진단/후속을 완료 처리하지 않습니다.

## 담당 Core의 문서쌍

| Core | 함수 / 역할 | 상세 설명 | 한국어 의사코드 |
|---|---|---|---|
| CORE-01 | `Flow_Process` — 동기 조율·후속 처리 연결 | [상세 설명](functions/Flow_Process.md) | [한국어 의사코드](pseudocode/Flow_Process.md) |

CORE-01의 상세 설명과 의사코드는 아래 동결 원문과 함께 읽습니다.

## 후속 진행 후보와 미정

coherent 관측 재평가·RX 없는 기한/후속 진행·복구 순서 연결은 [작성한 함수 설명](functions/Flow_Process.md#feasibility)의 독립 SW/Host 후보입니다. 실제 호출 주기·직렬화·처리 예산은 미정입니다. 새 Scheduler/Task/Queue를 도입하지 않습니다.

위 후보는 `TEST_NOT_RUN`이며 실제 코드/Host 실행은 별도 지시·범위·Gate가 필요합니다. 미해결 Q/R6·외부 IF/HW에 종속된 제품 결합과 P2는 계속 HOLD입니다. 문서 관계 보정의 승인 범위를 원 G1-B 원본 복구나 실제 제품 C1 완료로 확대하지 않습니다.

## 현재 상세 근거

- [기존 FLOW Module의 책임·lifecycle](../../../design/20_MODULES/10_FLOW_MODULE.md)
- [R2 담당 함수·Caller/Callee](../../../design/30_FUNCTIONS/10_FLOW_FUNCTIONS.md)
- [R3 관련 자료·writer·수명](../../../design/40_DATA/30_SELECTION_DATA.md)
- [관련 R4 공통 계약](../../../design/50_CONTRACTS/10_OWNERSHIP_LIFETIME.md)

논리 원형과 실제 C ABI의 구분은 [공통 문서 작성 기준](../../00_START_HERE.md)에서 R6 원문으로 이어집니다. 이 페이지는 탐색·설명 보조이며 승인 기술 정본을 대체하지 않습니다.
