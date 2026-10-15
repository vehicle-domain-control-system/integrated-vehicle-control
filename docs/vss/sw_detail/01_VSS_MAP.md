# VSS 지도 — Layer·Module·Core 위치

> A1 입력 개정 2026-10-15 · [현재 Network 연결](../design/90_BINDING/92_NETWORK_BINDING.md). `L=min(양수 USE_LIMIT,500ms,2000ms)`; 최초 actual에만 적용. 과거 동결/PASS는 당시 범위의 이력이며 본문 변경은 이번 A1 결정이다. 구현·시험 NOT_RUN.

[처음 읽기](00_START_HERE.md)

> GUIDE · 설계 정본: `VSS@0641b2c2268cf47450aeb50302638a45a7a3277f`.

VSS는 Domain이 확정한 사건·현재 경고를 검증해 STORE에 반영합니다. FLOW가 owner의 반영 완료 관측을 연결하면 POLICY가 읽기 전용으로 선택하고 PLAYBACK이 단일 전체 Session을 진행합니다. AUDIO STREAM은 저장 MP3를 PCM으로 준비하고 DRIVER/HAL의 귀속된 근거를 결합합니다. HEALTH는 현재 제한·복구 판단과 파생 보고를 담당합니다.

연결 지도: [입력·Status의 Owner/Converter/Consumer와 18 Core CALL/RETURN](02_INTERFACE_MAP.md) · [기존 6 Flow·49 Trace 및 경합·오류](03_SCENARIO_MAP.md).

Q01~14·R6-01~08·외부 미결15는 미해결이며 P2는 HOLD입니다. 문서의 호출·반환 연결은 실제 ABI·양단 채택·HW 검증을 뜻하지 않습니다.

## Layer와 Module 바로가기

| Layer | 책임 | 실제 Module 진입점 |
|---|---|---|
| [APP](01_APP/00_LAYER_OVERVIEW.md) | 처리 기회·선택/실행·보고/복구 조율 | [FLOW](01_APP/FLOW/00_MODULE_OVERVIEW.md) |
| [SERVICE](02_SERVICE/00_LAYER_OVERVIEW.md) | 의미·이력·정책·Session·PCM 준비·진단 | [INPUT](02_SERVICE/INPUT/00_MODULE_OVERVIEW.md) · [STORE](02_SERVICE/STORE/00_MODULE_OVERVIEW.md) · [POLICY](02_SERVICE/POLICY/00_MODULE_OVERVIEW.md) · [PLAYBACK](02_SERVICE/PLAYBACK/00_MODULE_OVERVIEW.md) · [AUDIO_STREAM](02_SERVICE/AUDIO_STREAM/00_MODULE_OVERVIEW.md) · [ASSET](02_SERVICE/ASSET/00_MODULE_OVERVIEW.md) · [HEALTH](02_SERVICE/HEALTH/00_MODULE_OVERVIEW.md) |
| [DRIVER](03_DRIVER/00_LAYER_OVERVIEW.md) | PCM 장치 보호·전송 근거와 장치별 구성 준비 | [AUDIO_TX](03_DRIVER/AUDIO_TX/00_MODULE_OVERVIEW.md) · [AUDIO_CONTROL](03_DRIVER/AUDIO_CONTROL/00_MODULE_OVERVIEW.md) |
| [HAL/BSP](04_HAL_BSP/00_LAYER_OVERVIEW.md) | 원등록 raw 관측과 보드/build 근거 | Layer 내부 AUDIOHAL / AUDIOPLATFORM Boundary |
| [INFRA](05_INFRA/00_LAYER_OVERVIEW.md) | 시간 비교·연속성·후속 처리/손실 근거 | 공통 RUNTIME Boundary |

5 Layer와 10 Module은 논리 책임입니다. `AudioHAL_Callback`은 18 Core에 포함되지만 10 Module 밖의 HAL/BSP 내부 Boundary입니다. INFRA에는 별도 Core가 없습니다. 이 구조를 실제 C 파일·Task·Queue 수로 복제하지 않습니다.

대표 경로는 INPUT → STORE 관측 → **FLOW를 통한 POLICY 판단/PLAYBACK 요구** → AUDIO STREAM → ASSET·DRIVER → HAL입니다. 보고는 HEALTH → FLOW → INPUT의 통신 연결 책임 → 기존 Node Communication이며, CAN 송신·Domain 수용은 별도 책임입니다.

<a id="d3-semantic-flow"></a>
## D3 의미 경로 — 입력·원 사실·선택·재생

| 진행·경계 | 확인할 함수 문서쌍 | 필요한 판단·보호 |
|---|---|---|
| FLOW → INPUT | [Flow_Process](01_APP/FLOW/functions/Flow_Process.md) / [의사코드](01_APP/FLOW/pseudocode/Flow_Process.md) → [Input_Process](02_SERVICE/INPUT/functions/Input_Process.md) / [의사코드](02_SERVICE/INPUT/pseudocode/Input_Process.md) | RX 없는 기회도 NULL 품질 점검; FLOW는 다른 owner 원본을 쓰지 않음 |
| INPUT → STORE | [Store_ApplyInput](02_SERVICE/STORE/functions/Store_ApplyInput.md) / [의사코드](02_SERVICE/STORE/pseudocode/Store_ApplyInput.md) | INPUT 문맥 적용과 STORE 수용은 별개; 원 발생·age·최초 품질상실 보호 |
| STREAM 원 사실 → PLAYBACK → STORE 사실 → STORE 기한 | [Playback_Advance](02_SERVICE/PLAYBACK/functions/Playback_Advance.md) / [의사코드](02_SERVICE/PLAYBACK/pseudocode/Playback_Advance.md) → [Store_ApplyPlaybackFact](02_SERVICE/STORE/functions/Store_ApplyPlaybackFact.md) / [의사코드](02_SERVICE/STORE/pseudocode/Store_ApplyPlaybackFact.md) → [Store_AdvanceDeadlines](02_SERVICE/STORE/functions/Store_AdvanceDeadlines.md) / [의사코드](02_SERVICE/STORE/pseudocode/Store_AdvanceDeadlines.md) | 원 fact 적용·최신 singular PB 관측이 expiry보다 먼저; NOT_APPLIED·OUT 부족은 보호·후속, void는 성공 아님 |
| FLOW → POLICY 판단 → FLOW → PLAYBACK 의도 | [Select_Choose](02_SERVICE/POLICY/functions/Select_Choose.md) / [의사코드](02_SERVICE/POLICY/pseudocode/Select_Choose.md) → [Playback_RequestTransition](02_SERVICE/PLAYBACK/functions/Playback_RequestTransition.md) / [의사코드](02_SERVICE/PLAYBACK/pseudocode/Playback_RequestTransition.md) | coherent basis 후 판단, PB는 효력 직전 재확인; CHOOSE ≠ ADOPTED ≠ prepared ≠ actual |
| PLAYBACK → STREAM 준비·STOP·후속 | [R6 typed 경계](../design/70_C_INTERFACE/00_HEADER_OWNERSHIP_MAP.md#r6-audiostream-requestcontrol) · [R4 충분조건](../design/50_CONTRACTS/20_ASYNC_RESULT_EVIDENCE.md#confirmed-outcomes) · [R5 STOP/late](../design/60_PSEUDOCODE/50_STOP_LATE_UNCERTAIN_FLOW.md#playback-stop-late) | [RequestControl](02_SERVICE/AUDIO_STREAM/functions/AudioStream_RequestControl.md) · [Advance](02_SERVICE/AUDIO_STREAM/functions/AudioStream_Advance.md) 상세 연결; 요청≠전체 안전 종료, 모든 원 scope·미래 출력/PCM·상위 권한·자원·late 보호 필요; uncertain/retired 결과는 old 정리만 |
| HEALTH → FLOW → INPUT 보고 연결 | [R5 조율·별도 보고 basis](../design/60_PSEUDOCODE/20_INPUT_TO_SELECTION.md#flow-process) · [R6 Status 경계](../design/70_C_INTERFACE/00_HEADER_OWNERSHIP_MAP.md#r6-health-buildstatus) | 보고 basis는 새로 수집; HEALTH가 보호한 CREATED만 연결, D6/D7 상세·CAN 송신/Domain 수용은 별도 |

## 18 Core의 문서쌍

아래는 18 Core의 설명·의사코드 18쌍(36개 문서)입니다. 경로는 `docs/vss/sw_detail/` 기준이며, 함수 이름·논리 owner·경로는 승인 Master의 Core 색인을 따릅니다.

| Core | 함수 | 논리 Owner / Layer | 설명 경로 | 의사코드 경로 |
|---|---|---|---|---|
| CORE-01 | `Flow_Process` | FLOW / 01_APP | [상세 설명](01_APP/FLOW/functions/Flow_Process.md) | [한국어 의사코드](01_APP/FLOW/pseudocode/Flow_Process.md) |
| CORE-02 | `Input_Process` | INPUT / 02_SERVICE | [상세 설명](02_SERVICE/INPUT/functions/Input_Process.md) | [한국어 의사코드](02_SERVICE/INPUT/pseudocode/Input_Process.md) |
| CORE-03 | `Store_ApplyInput` | STORE / 02_SERVICE | [상세 설명](02_SERVICE/STORE/functions/Store_ApplyInput.md) | [한국어 의사코드](02_SERVICE/STORE/pseudocode/Store_ApplyInput.md) |
| CORE-04 | `Store_ApplyPlaybackFact` | STORE / 02_SERVICE | [상세 설명](02_SERVICE/STORE/functions/Store_ApplyPlaybackFact.md) | [한국어 의사코드](02_SERVICE/STORE/pseudocode/Store_ApplyPlaybackFact.md) |
| CORE-05 | `Store_AdvanceDeadlines` | STORE / 02_SERVICE | [상세 설명](02_SERVICE/STORE/functions/Store_AdvanceDeadlines.md) | [한국어 의사코드](02_SERVICE/STORE/pseudocode/Store_AdvanceDeadlines.md) |
| CORE-06 | `Select_Choose` | POLICY / 02_SERVICE | [상세 설명](02_SERVICE/POLICY/functions/Select_Choose.md) | [한국어 의사코드](02_SERVICE/POLICY/pseudocode/Select_Choose.md) |
| CORE-07 | `Playback_RequestTransition` | PLAYBACK / 02_SERVICE | [상세 설명](02_SERVICE/PLAYBACK/functions/Playback_RequestTransition.md) | [한국어 의사코드](02_SERVICE/PLAYBACK/pseudocode/Playback_RequestTransition.md) |
| CORE-08 | `Playback_Advance` | PLAYBACK / 02_SERVICE | [상세 설명](02_SERVICE/PLAYBACK/functions/Playback_Advance.md) | [한국어 의사코드](02_SERVICE/PLAYBACK/pseudocode/Playback_Advance.md) |
| CORE-09 | `Asset_Read` | ASSET / 02_SERVICE | [상세 설명](02_SERVICE/ASSET/functions/Asset_Read.md) | [한국어 의사코드](02_SERVICE/ASSET/pseudocode/Asset_Read.md) |
| CORE-10 | `AudioStream_Prepare` | AUDIO_STREAM / 02_SERVICE | [상세 설명](02_SERVICE/AUDIO_STREAM/functions/AudioStream_Prepare.md) | [한국어 의사코드](02_SERVICE/AUDIO_STREAM/pseudocode/AudioStream_Prepare.md) |
| CORE-11 | `AudioStream_RequestControl` | AUDIO_STREAM / 02_SERVICE | [상세 설명](02_SERVICE/AUDIO_STREAM/functions/AudioStream_RequestControl.md) | [한국어 의사코드](02_SERVICE/AUDIO_STREAM/pseudocode/AudioStream_RequestControl.md) |
| CORE-12 | `AudioStream_Advance` | AUDIO_STREAM / 02_SERVICE | [상세 설명](02_SERVICE/AUDIO_STREAM/functions/AudioStream_Advance.md) | [한국어 의사코드](02_SERVICE/AUDIO_STREAM/pseudocode/AudioStream_Advance.md) |
| CORE-13 | `AudioTx_Request` | AUDIO_TX / 03_DRIVER | [상세 설명](03_DRIVER/AUDIO_TX/functions/AudioTx_Request.md) | [한국어 의사코드](03_DRIVER/AUDIO_TX/pseudocode/AudioTx_Request.md) |
| CORE-14 | `AudioTx_Advance` | AUDIO_TX / 03_DRIVER | [상세 설명](03_DRIVER/AUDIO_TX/functions/AudioTx_Advance.md) | [한국어 의사코드](03_DRIVER/AUDIO_TX/pseudocode/AudioTx_Advance.md) |
| CORE-15 | `AudioControl_Service` | AUDIO_CONTROL / 03_DRIVER | [상세 설명](03_DRIVER/AUDIO_CONTROL/functions/AudioControl_Service.md) | [한국어 의사코드](03_DRIVER/AUDIO_CONTROL/pseudocode/AudioControl_Service.md) |
| CORE-16 | `AudioHAL_Callback` | AUDIOHAL_BOUNDARY / 04_HAL_BSP | [상세 설명](04_HAL_BSP/AUDIOHAL_BOUNDARY/functions/AudioHAL_Callback.md) | [한국어 의사코드](04_HAL_BSP/AUDIOHAL_BOUNDARY/pseudocode/AudioHAL_Callback.md) |
| CORE-17 | `Health_Evaluate` | HEALTH / 02_SERVICE | [상세 설명](02_SERVICE/HEALTH/functions/Health_Evaluate.md) | [한국어 의사코드](02_SERVICE/HEALTH/pseudocode/Health_Evaluate.md) |
| CORE-18 | `Health_BuildStatus` | HEALTH / 02_SERVICE | [상세 설명](02_SERVICE/HEALTH/functions/Health_BuildStatus.md) | [한국어 의사코드](02_SERVICE/HEALTH/pseudocode/Health_BuildStatus.md) |

## 근거와 미결 범위

현재 상세 동작은 [R2 함수 원문](../design/30_FUNCTIONS/00_FUNCTION_OVERVIEW.md)에서, writer·자료 수명은 [R3](../design/40_DATA/00_DATA_OVERVIEW.md)에서, `winner ≠ prepared ≠ accepted/enabled ≠ actual start`와 STOP·late·PCM 회차는 [R4 공통 계약](../design/50_CONTRACTS/00_CONTRACT_OVERVIEW.md)에서 확인합니다. 이 지도는 기존 49 Trace를 재정의하지 않습니다.

P1B_C1의 **9 Core/16 Q↔Core 연결**에 대한 문서 보정과 제품 C1 검증은 별개입니다. Q01~14와 Network PR #68은 당시 미합의 제안이며, R6-01~08·실제 C ABI·제품 source/image·실제 출력/STOP·DMA 안전은 미확정입니다. [Binding·정책 미정 원문](../design/90_BINDING/91_IMPLEMENTATION_TBD.md)을 따르고 최종 Q 차단 지도를 채택하지 않습니다. 원 G1-B 판정 원본은 SOURCE_NOT_RECOVERED이며, P2는 HOLD입니다.

중복·품질/Hold 기산·읽기 전용 선택·bounded Asset read·PCM 회차/late·Unknown/Status는 독립 SW/Host 후보로 남습니다. `<L`은 [원 발생→최초 actual의 잠정 조건](../design/50_CONTRACTS/30_TIMING_FRESHNESS.md)이며 A1의 원 USE_LIMIT·500ms·2000ms 상한을 함께 적용합니다. WINDOW/Anti-Pinch 제품 연결은 [구현 보류 — 설계 유지]이며 격리 TEST 의미는 유지합니다. 관련 Module의 후속 후보·제품 Gate를 함께 읽으세요.

## A1 현재 입력 기준과 이력

현재 입력 연결은 [92_NETWORK_BINDING](../design/90_BINDING/92_NETWORK_BINDING.md)과 A1으로 개정한 Data/계약/의사코드다. Network 고정 원문은 `70028972ec713a3ec6b5f0a419e0dc89fd963844`이다. 과거 SHA·Q01~Q14·PASS·P1B HOLD 기록은 당시 검토 이력이며 현재 미결 전체가 아니다. D01 WARNING60, D04 L, D05 RX Age, D06 Hold/활성 후보, D07 owner/용량, D08 복구 분리는 설계 반영했다. Q 관련 DOMAIN 판단 순서/생산·계보·Boot·실제 시간/ABI/HW는 E01~E05로 구체화했다. Status 관련 Q/D09~D11은 A2 미착수다.
