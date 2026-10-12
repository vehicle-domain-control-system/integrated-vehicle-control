# VSS / SERVICE / PLAYBACK

[상위 Layer](../00_LAYER_OVERVIEW.md) · [VSS 지도](../../01_VSS_MAP.md) · [문서 양식·확인 수준](../../00_START_HERE.md)

> GUIDE · VSD-D1 탐색 문서 · GD1 독립 PASS 인수. 설계 정본: `VSS@0641b2c2268cf47450aeb50302638a45a7a3277f`.

선택된 전체 정책을 단일 출력 Session으로 진행합니다. 최신 실행 조건을 확인해 AUDIO_STREAM에 준비/제어를 요구하고, 귀속된 Backend 사실을 적용해 STORE의 재생 이력으로 연결합니다.

## 상태 owner와 입출력

PLAYBACK은 Session/Attempt·출력 owner·전체 계획 진행·종료 의도·후속 start/cue/반복 권한의 단일 writer입니다. occurrence ledger는 STORE, PCM/provider 진행은 AUDIO_STREAM 소유입니다.

| 구분 | 주요 의미 |
|---|---|
| 입력 | FLOW의 선택 의도·최신 후보/기한/제한, AUDIO_STREAM Backend 결과와 시간 근거 |
| 출력 | AUDIO_STREAM 준비/제어 요구, STORE의 원귀속 최초 시작·전체 최종 사실, FLOW의 재평가·정리 관측 |
| 의존 | FLOW → PLAYBACK → AUDIO_STREAM/STORE 및 필요한 RUNTIME |

## 인접 관계와 비동기 경계

요구 수용과 비동기 actual/no-start/termination을 구분합니다. prepared 뒤에도 실제 새 start 직전에 최신 조건을 재검사합니다. STOPPING 중 적법한 late 사실을 반영해도 ACTIVE·후속 cue 권한을 되살리지 않습니다. 한 cue/EOS나 하위 종료만으로 전체 Session Completed·retirement를 확정하지 않습니다.

[STORE 발생 이력](../STORE/00_MODULE_OVERVIEW.md) · [AUDIO_STREAM Backend](../AUDIO_STREAM/00_MODULE_OVERVIEW.md)

## 연결해서 읽기

[D3 의미 흐름](../../01_VSS_MAP.md#d3-semantic-flow)의 RequestTransition 수용과 Advance의 원 actual/STOP·late·전체 retirement는 별개이며, 원 fact는 [STORE 적용](../STORE/functions/Store_ApplyPlaybackFact.md#branches)으로만 ledger에 반영합니다. 하위 D4 상세는 미작성이고 [R4 scope별 충분조건](../../../design/50_CONTRACTS/20_ASYNC_RESULT_EVIDENCE.md#confirmed-outcomes)으로 요청 수락·한 cue 종료를 전체 안전 종료로 승격하지 않습니다.

## 담당 Core의 문서쌍

| Core | 함수 / 역할 | 상세 설명 | 한국어 의사코드 | 작성 상태 |
|---|---|---|---|---|
| CORE-07 | `Playback_RequestTransition` — 동기 의도 수용·후속 권한 변경 / 하위 비동기 | [상세 설명](functions/Playback_RequestTransition.md) | [한국어 의사코드](pseudocode/Playback_RequestTransition.md) | D3-2 PARTIAL_INDEPENDENT_PASS / COMPLETED |
| CORE-08 | `Playback_Advance` — 귀속 결과·기한·late·전체 정리의 후속 반영 | [상세 설명](functions/Playback_Advance.md) | [한국어 의사코드](pseudocode/Playback_Advance.md) | D3-2 PARTIAL_INDEPENDENT_PASS / COMPLETED |

위 링크에서 R6 논리 계약 기반 상세 설명과 한국어 C 형태 의사코드를 왕복합니다. 기존 함수쌍은 D3-2 부분 독립 PASS 인수 완료이며 기술 의미는 아래 동결 원문을 따릅니다.

## 후속 진행 후보와 미정

종료 의도·미래 권한 차단·late 귀속·불확실 최종 보존은 독립 SW/Host 후보입니다. 물리 actual/전체 출력 종료와 실제 STOP scope·timeout/retry·저장 표현은 미정입니다. STREAM·TX 충분 근거를 대신 생성하지 않습니다. 기존 함수쌍은 위 링크에서 읽습니다.

위 후보는 `TEST_NOT_RUN`이며 실제 코드/Host 실행은 별도 지시·범위·Gate가 필요합니다. 미해결 Q↔Core 관계는 P1B_C1 독립 검수 전 최종 차단 지도로 채택하지 않습니다.

## 현재 상세 근거

- [기존 PLAYBACK Module의 책임·lifecycle](../../../design/20_MODULES/23_PLAYBACK_MODULE.md)
- [R2 담당 함수·Caller/Callee](../../../design/30_FUNCTIONS/30_POLICY_PLAYBACK_FUNCTIONS.md)
- [R3 관련 자료·writer·수명](../../../design/40_DATA/40_PLAYBACK_DATA.md)
- [관련 R4 공통 계약](../../../design/50_CONTRACTS/20_ASYNC_RESULT_EVIDENCE.md)

논리 원형과 실제 C ABI의 구분은 [공통 문서 작성 기준](../../00_START_HERE.md)에서 R6 원문으로 이어집니다. 이 페이지는 탐색·설명 보조이며 승인 기술 정본을 대체하지 않습니다.
