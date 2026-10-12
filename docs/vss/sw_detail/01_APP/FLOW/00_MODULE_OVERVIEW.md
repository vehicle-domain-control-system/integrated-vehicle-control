# VSS / APP / FLOW

[상위 Layer](../00_LAYER_OVERVIEW.md) · [VSS 지도](../../01_VSS_MAP.md) · [문서 양식·확인 수준](../../00_START_HERE.md)

> GUIDE · GD1 독립 PASS 인수 · D3-1 SELF_CHECKED / D3_1_REVIEW_PENDING. 설계 정본: `VSS@0641b2c2268cf47450aeb50302638a45a7a3277f`.

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

## 담당 Core의 문서쌍

| Core | 함수 / 역할 | 설명 예정 경로 | 의사코드 예정 경로 | 작성 상태 |
|---|---|---|---|---|
| CORE-01 | `Flow_Process` — 동기 조율·후속 처리 연결 | [상세 설명](functions/Flow_Process.md) | [한국어 의사코드](pseudocode/Flow_Process.md) | SELF_CHECKED / D3_1_REVIEW_PENDING |

CORE-01 두 본문은 D3-1 자체검토 완료·부분 독립 검수 대기입니다. 아래 동결 원문과 함께 읽으며 D3-2/3·최종 GD3를 완료한 것으로 보지 않습니다.

## 후속 진행 후보와 미정

coherent 관측 재평가·RX 없는 기한/후속 진행·복구 순서 연결은 [작성한 함수 설명](functions/Flow_Process.md#feasibility)의 독립 SW/Host 후보입니다. 실제 호출 주기·직렬화·처리 예산은 미정입니다. 새 Scheduler/Task/Queue를 도입하지 않습니다.

위 후보는 `TEST_NOT_RUN`이며 실제 코드/Host 실행은 별도 지시·범위·Gate가 필요합니다. 미해결 Q↔Core 관계는 P1B_C1 독립 검수 전 최종 차단 지도로 채택하지 않습니다.

## 현재 상세 근거

- [기존 FLOW Module의 책임·lifecycle](../../../design/20_MODULES/10_FLOW_MODULE.md)
- [R2 담당 함수·Caller/Callee](../../../design/30_FUNCTIONS/10_FLOW_FUNCTIONS.md)
- [R3 관련 자료·writer·수명](../../../design/40_DATA/30_SELECTION_DATA.md)
- [관련 R4 공통 계약](../../../design/50_CONTRACTS/10_OWNERSHIP_LIFETIME.md)

논리 원형과 실제 C ABI의 구분은 [공통 문서 작성 기준](../../00_START_HERE.md)에서 R6 원문으로 이어집니다. 이 페이지는 탐색·설명 보조이며 승인 기술 정본을 대체하지 않습니다.
