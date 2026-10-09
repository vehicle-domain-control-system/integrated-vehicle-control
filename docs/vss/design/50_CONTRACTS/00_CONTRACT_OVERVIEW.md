# Contract Overview — R4 Cross Contract

> 2026-10-08 · R3-C1 및 잔여 함수 독립 국소 PASS 기준 · 일반 채팅 R4 독립 검수 대기

여러 owner에 반복된 공통 불변조건은 아래 다섯 계약에서 읽는다. Module의 책임·실패/예외, Function의 owner/Caller/Callee, Data의 필드·writer·수명은 각 local 문서에서 읽는다. 계약 링크는 상태 owner를 옮기거나 실제 C/RTD Binding을 확정하지 않는다. 기존 다섯 탐색 anchor는 유지했다.

| 공통 축 | 공통 규칙의 원문 | local 적용 진입점 |
| --- | --- | --- |
| Ownership / Lifetime | [10_OWNERSHIP_LIFETIME.md](10_OWNERSHIP_LIFETIME.md) | [STORE](../20_MODULES/21_STORE_MODULE.md#occurrence) · [PLAYBACK](../20_MODULES/23_PLAYBACK_MODULE.md) · [HAL lifetime](../10_LAYERS/details/40_HAL_BSP_BOUNDARY.md#lifetime) |
| Async Result / Evidence | [20_ASYNC_RESULT_EVIDENCE.md](20_ASYNC_RESULT_EVIDENCE.md) | [PLAYBACK 결과 적용](../20_MODULES/23_PLAYBACK_MODULE.md#outcomes) · [AUDIO TX](../20_MODULES/30_AUDIO_TX_MODULE.md#evidence) · [TX/HAL 자료](../40_DATA/60_AUDIO_TX_DATA.md#tx) |
| Timing / Freshness | [30_TIMING_FRESHNESS.md](30_TIMING_FRESHNESS.md) | [INPUT](../20_MODULES/20_INPUT_MODULE.md) · [STORE Stateful](../20_MODULES/21_STORE_MODULE.md#stateful) · [Runtime marker](../10_LAYERS/details/50_TIMEBASE_RUNTIME_BOUNDARY.md#measurement) |
| Fault / Recovery | [40_FAULT_RECOVERY.md](40_FAULT_RECOVERY.md) | [HEALTH](../20_MODULES/26_HEALTH_MODULE.md#recovery) · [Fault/복구 자료](../40_DATA/70_DIAGNOSTIC_DATA.md#health) · [FLOW](../20_MODULES/10_FLOW_MODULE.md) |
| Audio Buffer / Transport | [50_AUDIO_BUFFER_TRANSPORT.md](50_AUDIO_BUFFER_TRANSPORT.md) | [STREAM usage](../20_MODULES/24_AUDIO_STREAM_MODULE.md#pcm) · [TX](../20_MODULES/30_AUDIO_TX_MODULE.md#evidence) · [PCM 자료](../40_DATA/50_AUDIO_STREAM_DATA.md#pcm) |

<a id="ownership"></a>
## Ownership / Lifetime 탐색

[Single writer](10_OWNERSHIP_LIFETIME.md#single-writer) · [Occurrence/Session/Attempt와 자료별 수명](10_OWNERSHIP_LIFETIME.md#identity-lifetimes) · [borrow와 보호](10_OWNERSHIP_LIFETIME.md#borrow-and-protect) · [독립 회수 조건](10_OWNERSHIP_LIFETIME.md#references-and-release) · [late 원귀속](10_OWNERSHIP_LIFETIME.md#late-attribution)

원본 owner는 [Module Overview](../20_MODULES/00_MODULE_OVERVIEW.md)와 [Data Overview](../40_DATA/00_DATA_OVERVIEW.md)에 유지한다.

<a id="async"></a>
## Async Result / Evidence 탐색

[단계 차이](20_ASYNC_RESULT_EVIDENCE.md#stages) · [관측 범위/부족·손실](20_ASYNC_RESULT_EVIDENCE.md#evidence-basis) · [actual/no-start/termination 충분조건](20_ASYNC_RESULT_EVIDENCE.md#confirmed-outcomes) · [partial/uncertain](20_ASYNC_RESULT_EVIDENCE.md#partial-and-uncertain) · [최종 전/뒤 late](20_ASYNC_RESULT_EVIDENCE.md#late-results)

장치·Backend·전체 Session은 각 owner의 확인 범위를 유지한다. 물리 충분성은 B2-R/보드 검증 TBD다.

<a id="timing"></a>
## Timing / Freshness 탐색

[발생/포착/처리 시간](30_TIMING_FRESHNESS.md#time-context) · [원 발생 age](30_TIMING_FRESHNESS.md#source-age) · [최초 시작과 정상 후속 cue](30_TIMING_FRESHNESS.md#first-start) · [Stateful Hold/Rear](30_TIMING_FRESHNESS.md#stateful-hold) · [반영 완료 관측](30_TIMING_FRESHNESS.md#coherent-read)

원본 첫 시작 `< 2초`는 잠정이고 Hold/UseLimit 및 그 관계는 [정책 TBD](../90_BINDING/91_IMPLEMENTATION_TBD.md#policy)로 남긴다.

<a id="fault"></a>
## Fault / Recovery 탐색

[기존 Fault 5종](40_FAULT_RECOVERY.md#taxonomy) · [현재 instance/revision/target](40_FAULT_RECOVERY.md#current-target) · [허용/수행/별도 검증/HEALTH 해제](40_FAULT_RECOVERY.md#recovery-flow) · [현재 후보 재평가](40_FAULT_RECOVERY.md#resume-eligibility)

복구는 Session 없는 typed permission으로도 성립한다. 원인별 매핑·효과 충분조건·가용성 상세 정책은 TBD다.

<a id="buffer"></a>
## Audio Buffer / Transport 탐색

[A/B 두 개](50_AUDIO_BUFFER_TRANSPORT.md#pcm-boundary) · [C1-01 유효 PCM/물리 무음](50_AUDIO_BUFFER_TRANSPORT.md#valid-and-silence) · [사전 pending 보호](50_AUDIO_BUFFER_TRANSPORT.md#access-cycle) · [C1-03 안전 반환/차기 방문](50_AUDIO_BUFFER_TRANSPORT.md#safe-write) · [네 경계](50_AUDIO_BUFFER_TRANSPORT.md#four-boundaries) · [C1-02 지속 등록/개별 귀속](50_AUDIO_BUFFER_TRANSPORT.md#continuous-stream) · [TX typed 입력](50_AUDIO_BUFFER_TRANSPORT.md#tx-typed-lanes)

R4의 논리 계약은 물리 안전성의 실측 PASS가 아니다. [기존 Binding 근거](../90_BINDING/90_C_FILE_API_MAPPING.md#transport)와 [메모리/실행 TBD](../90_BINDING/91_IMPLEMENTATION_TBD.md#memory)를 유지한다.

## 범위와 게이트

실제로 반복된 규칙만 통합했으며 local 판정·상태 적용·독립 예외는 해당 원문에 남겼다. Source→Contract·파일별 변경 전후·해시/링크 검사·미해결 TBD는 결과 ZIP의 `R4_RESULT_SUMMARY.md`에 기록한다. 논리 Core 18개·5 Layer/10 Module·32 provisional 판정·기존 Data 선언/필드/owner는 유지한다. R5/R6/R7/B2-R·실제 C 구현·보드 테스트를 수행하지 않는다. R4 자체 검수 뒤 중단하고 일반 채팅 독립 PASS 전에는 R5로 진행하지 않는다.

[Document Map](../00_OVERVIEW/01_DOCUMENT_MAP.md) · [기존 의사코드 참고](../60_PSEUDOCODE/00_PSEUDOCODE_OVERVIEW.md) · [C Interface 미확정](../70_C_INTERFACE/00_HEADER_OWNERSHIP_MAP.md)
