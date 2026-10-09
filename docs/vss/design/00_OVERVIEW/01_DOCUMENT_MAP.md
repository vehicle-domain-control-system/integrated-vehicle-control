# Document Map

> R7 현재 문서 탐색점. 최신 승인 기준은 R6 독립 PASS다. current 57개 경로와 기존 구조·owner·Function 계약·Data 필드·R4 Contract·R5-C1 의미를 유지한다.

읽는 순서: Overview → Layer → Module → Function → Data → Contract → Pseudocode → C Interface → Binding.

상위 문서는 요약·관계·링크, 하위 문서는 기존 상세의 위치다. Function은 R2 상세 계약·provisional prototype까지 작성했다. Data는 R3 논리 필드·수명·pseudo-C를 작성했다. 공통 Contract는 R4 독립 PASS 기준으로 보존했고 Pseudocode는 R5-C1 typed Flow의 의미를 따른다. R6는 Header 선언 owner·가시성·typed 후보 원형·자료 수명·include 방향을 정리했고 독립 PASS를 받았다. R7은 이 사이의 내부 E2E 정합성을 검수했다. 실제 Header/ABI·물리 Binding과 최신 외부 인터페이스 대조는 미확정이다. [R7 자체 검수 결과](../../../../R7_RESULT_SUMMARY.md) 뒤 일반 채팅 독립 검수를 기다리며 독립 PASS 전 B2-R로 진입하지 않는다.

## 00_OVERVIEW — 전체 구조·탐색·기존 Trace

| 문서 | 현재 역할 |
| --- | --- |
| [00_VSS_DESIGN_OVERVIEW.md](00_VSS_DESIGN_OVERVIEW.md) | VSS Design Overview |
| [02_VSS_TRACEABILITY_MAP.md](02_VSS_TRACEABILITY_MAP.md) | VSS Traceability Map |

## 10_LAYERS — Layer 관계·경계

| 문서 | 현재 역할 |
| --- | --- |
| [00_LAYER_OVERVIEW.md](../10_LAYERS/00_LAYER_OVERVIEW.md) | Layer Overview |
| [10_APP_LAYER.md](../10_LAYERS/10_APP_LAYER.md) | APP Layer |
| [20_SERVICE_LAYER.md](../10_LAYERS/20_SERVICE_LAYER.md) | SERVICE Layer |
| [30_DRIVER_LAYER.md](../10_LAYERS/30_DRIVER_LAYER.md) | Audio DRIVER Layer |
| [40_HAL_BSP_LAYER.md](../10_LAYERS/40_HAL_BSP_LAYER.md) | HAL / BSP Layer |
| [50_INFRA_LAYER.md](../10_LAYERS/50_INFRA_LAYER.md) | INFRA Layer |
| [40_HAL_BSP_BOUNDARY.md](../10_LAYERS/details/40_HAL_BSP_BOUNDARY.md) | HAL / BSP Layer 하위 Boundary 상세 |
| [50_TIMEBASE_RUNTIME_BOUNDARY.md](../10_LAYERS/details/50_TIMEBASE_RUNTIME_BOUNDARY.md) | INFRA Layer 하위 공통 Boundary 상세 |

## 20_MODULES — Module 책임·owner·lifecycle·local contract

| 문서 | 현재 역할 |
| --- | --- |
| [00_MODULE_OVERVIEW.md](../20_MODULES/00_MODULE_OVERVIEW.md) | Module Overview |
| [10_FLOW_MODULE.md](../20_MODULES/10_FLOW_MODULE.md) | FLOW Module — VSS 진행 조율 |
| [20_INPUT_MODULE.md](../20_MODULES/20_INPUT_MODULE.md) | INPUT Module — 의미 입력 검증 |
| [21_STORE_MODULE.md](../20_MODULES/21_STORE_MODULE.md) | STORE Module — 발생 이력과 현재 후보 |
| [22_POLICY_MODULE.md](../20_MODULES/22_POLICY_MODULE.md) | POLICY / SELECT Module — 읽기 전용 판단 |
| [23_PLAYBACK_MODULE.md](../20_MODULES/23_PLAYBACK_MODULE.md) | PLAYBACK Module — 단일 Session 실행 |
| [24_AUDIO_STREAM_MODULE.md](../20_MODULES/24_AUDIO_STREAM_MODULE.md) | AUDIO STREAM Module — 준비·decode·PCM |
| [25_ASSET_MODULE.md](../20_MODULES/25_ASSET_MODULE.md) | ASSET Module — 읽기 전용 MP3 공급 |
| [26_HEALTH_MODULE.md](../20_MODULES/26_HEALTH_MODULE.md) | HEALTH Module — 진단·복구 판단·파생 보고 |
| [30_AUDIO_TX_MODULE.md](../20_MODULES/30_AUDIO_TX_MODULE.md) | AUDIO TX Module — PCM 장치 전송 |
| [31_AUDIO_CONTROL_MODULE.md](../20_MODULES/31_AUDIO_CONTROL_MODULE.md) | AUDIO CONTROL Module — 장치별 구성 준비 |

## 30_FUNCTIONS — R2 Core 상세·provisional prototype

| 문서 | 현재 역할 |
| --- | --- |
| [00_FUNCTION_OVERVIEW.md](../30_FUNCTIONS/00_FUNCTION_OVERVIEW.md) | Function Overview |
| [10_FLOW_FUNCTIONS.md](../30_FUNCTIONS/10_FLOW_FUNCTIONS.md) | FLOW Core Function 상세 |
| [20_INPUT_STORE_FUNCTIONS.md](../30_FUNCTIONS/20_INPUT_STORE_FUNCTIONS.md) | INPUT / STORE Core Function 상세 |
| [30_POLICY_PLAYBACK_FUNCTIONS.md](../30_FUNCTIONS/30_POLICY_PLAYBACK_FUNCTIONS.md) | POLICY / PLAYBACK Core Function 상세 |
| [40_AUDIO_STREAM_ASSET_FUNCTIONS.md](../30_FUNCTIONS/40_AUDIO_STREAM_ASSET_FUNCTIONS.md) | AUDIO STREAM / ASSET Core Function 상세 |
| [50_DRIVER_HAL_FUNCTIONS.md](../30_FUNCTIONS/50_DRIVER_HAL_FUNCTIONS.md) | DRIVER / HAL Core Function 상세 |
| [60_HEALTH_RUNTIME_FUNCTIONS.md](../30_FUNCTIONS/60_HEALTH_RUNTIME_FUNCTIONS.md) | HEALTH Core 상세 / Runtime 연결 |

## 40_DATA — R3 Data 상세·pseudo-C·타입 판정

| 문서 | 현재 역할 |
| --- | --- |
| [00_DATA_OVERVIEW.md](../40_DATA/00_DATA_OVERVIEW.md) | Data Overview |
| [10_INPUT_DATA.md](../40_DATA/10_INPUT_DATA.md) | Input Data — R3 상세 |
| [20_STORE_DATA.md](../40_DATA/20_STORE_DATA.md) | Store Data — R3 상세 |
| [30_SELECTION_DATA.md](../40_DATA/30_SELECTION_DATA.md) | Selection Data — R3 상세 |
| [40_PLAYBACK_DATA.md](../40_DATA/40_PLAYBACK_DATA.md) | Playback Data — R3 상세 |
| [50_AUDIO_STREAM_DATA.md](../40_DATA/50_AUDIO_STREAM_DATA.md) | Audio Stream / Asset Data — R3 상세 |
| [60_AUDIO_TX_DATA.md](../40_DATA/60_AUDIO_TX_DATA.md) | Audio TX / Control / HAL Data — R3 상세 |
| [70_DIAGNOSTIC_DATA.md](../40_DATA/70_DIAGNOSTIC_DATA.md) | Diagnostic / Status Data — R3 상세 |

## 50_CONTRACTS — R4 공통 규칙과 local 적용 연결

| 문서 | 현재 역할 |
| --- | --- |
| [00_CONTRACT_OVERVIEW.md](../50_CONTRACTS/00_CONTRACT_OVERVIEW.md) | Contract Overview |
| [10_OWNERSHIP_LIFETIME.md](../50_CONTRACTS/10_OWNERSHIP_LIFETIME.md) | 원본 owner·identity/참조 수명·late 귀속 |
| [20_ASYNC_RESULT_EVIDENCE.md](../50_CONTRACTS/20_ASYNC_RESULT_EVIDENCE.md) | 단계·충분조건·부분 효력/uncertain/late |
| [30_TIMING_FRESHNESS.md](../50_CONTRACTS/30_TIMING_FRESHNESS.md) | 원 age·실제 시각·최초 시작/Hold·읽기 근거 |
| [40_FAULT_RECOVERY.md](../50_CONTRACTS/40_FAULT_RECOVERY.md) | Fault 5종·현재 허용/수행/검증/해제 |
| [50_AUDIO_BUFFER_TRANSPORT.md](../50_CONTRACTS/50_AUDIO_BUFFER_TRANSPORT.md) | A/B 회차·C1-01/02/03·네 종료 경계 |

## 60_PSEUDOCODE — R5 typed 실행 흐름·불변 의미 참고

| 문서 | 현재 역할 |
| --- | --- |
| [00_PSEUDOCODE_OVERVIEW.md](../60_PSEUDOCODE/00_PSEUDOCODE_OVERVIEW.md) | Pseudocode Overview |
| [10_STARTUP_FLOW.md](../60_PSEUDOCODE/10_STARTUP_FLOW.md) | 초기 미평가/장치 준비 |
| [20_INPUT_TO_SELECTION.md](../60_PSEUDOCODE/20_INPUT_TO_SELECTION.md) | 입력 적용/기한/읽기 선택 |
| [30_PLAYBACK_FLOW.md](../60_PSEUDOCODE/30_PLAYBACK_FLOW.md) | 채택/실제 시작/정상 cue |
| [40_AUDIO_PREPARE_TX_FLOW.md](../60_PSEUDOCODE/40_AUDIO_PREPARE_TX_FLOW.md) | read/decode/A/B/typed TX/HAL |
| [50_STOP_LATE_UNCERTAIN_FLOW.md](../60_PSEUDOCODE/50_STOP_LATE_UNCERTAIN_FLOW.md) | 차단/수명/late/uncertain/통지 |
| [60_FAULT_RECOVERY_FLOW.md](../60_PSEUDOCODE/60_FAULT_RECOVERY_FLOW.md) | 현재 허용/수행/검증/HEALTH 보고 |
| [81_VSS_PSEUDOCODE.md](../60_PSEUDOCODE/reference/81_VSS_PSEUDOCODE.md) | VSS Pseudocode — Structured + Compact |

## 70_C_INTERFACE — R6 논리 선언 경계·typed 후보 원형

| 문서 | 현재 역할 |
| --- | --- |
| [00_HEADER_OWNERSHIP_MAP.md](../70_C_INTERFACE/00_HEADER_OWNERSHIP_MAP.md) | R6 Header 재사용·owner/visibility·18 Core typed 후보·수명/include·TBD |

## 90_BINDING — 기존 근거·TBD·B2-R 대상

| 문서 | 현재 역할 |
| --- | --- |
| [90_C_FILE_API_MAPPING.md](../90_BINDING/90_C_FILE_API_MAPPING.md) | VSS C File / API Mapping — Stage A Grounding + Stage B1 Service/Session |
| [91_IMPLEMENTATION_TBD.md](../90_BINDING/91_IMPLEMENTATION_TBD.md) | Implementation TBD |

## current-level 상태와 이전 근거

[현재 Stage 상태](../../../00_README_CURRENT_PACKAGE.md) · [전체 파일 목록](../../../FILE_LIST.md) · [이전 v1.9 기록 — 현재 실행 지시 아님](../../../reference/VSS_STAGE5_STRUCTURED_COMPACT_PLAN_v1.9.md)

고정 기준은 handoff의 `00_VSS_DOCUMENT_ARCHITECTURE_BASELINE_v1.1.md`와 `01_VSS_DOCUMENT_REBUILD_EXECUTION_PLAN_v1.1.md`다. 이번 지정 결과에는 governance를 포함하지 않으며 기준은 변경하지 않았다.

## R0 재배치의 핵심

| 기존 문서/묶음 | 현재 위치·처리 |
| --- | --- |
| 기존 Implementation Overview | 전체 Overview + Layer/Module Overview로 관계·책임 표를 분리 |
| 기존 APP / SERVICE / DRIVER Layer와 Module | `10_LAYERS` / `20_MODULES`로 책임 위치 분리, 각 상세에 하위 탐색 링크 추가 |
| 기존 HAL/BSP / Runtime 혼합 문서 | R0 위치의 애매점을 R1에서 재판정. Layer 역할은 `10_LAYERS`, callback/자원/시간·marker 상세는 `10_LAYERS/details`에 보존 |
| `80_CORE_FUNCTIONS` | 짧은 Function Overview + 기존 6개 관련 그룹의 실제 내용. 기존 후보의 의미를 보존해 R2 전수 판정/상세화 |
| `82_VSS_DATA_MODEL` | Data Overview + 관련 7개 상세에 기존 11개 의미 묶음 보존 |
| 기존 ownership / async / timing / Fault / buffer 조건 | R4 다섯 공통 계약으로 반복 규칙 통합. 서로 다른 local 적용·실패/예외는 원래 상세 유지 |
| `81_VSS_PSEUDOCODE` | `60_PSEUDOCODE/reference`에 전체 본문 유지. R5 재작성의 불변 의미 참고 |
| 기존 C Mapping / TBD | `90_BINDING`에 유지. Stage A/B1 근거는 보존, 즉시 B2 진행 지시만 B2-R 순서로 교정 |
| 기존 Trace Map | `00_OVERVIEW/02_VSS_TRACEABILITY_MAP`에 기존 ID·관계 유지 |
| v1.9 이전 계획 | `current/reference`에 이전 진행 기록으로 격리. 수량/이전 다음 단계 Lock은 현 기준 아님 |

`70_C_INTERFACE/00_HEADER_OWNERSHIP_MAP.md`는 R6에서 논리 선언·수명 계약과 후보 pseudo prototype을 작성했다. 실제 C/H 파일·ABI·RTD Binding을 확정한 문서는 아니다. R4 다섯 계약은 본문을 작성했고 R5는 여섯 실행 Flow를 작성했다. R3는 기존 Data 문서 8개 안에서 상세화했다. R2는 기존 Function 그룹 7개 파일 안에서 상세화했다.

R1에서는 5개 Layer와 실제 Module 10개를 표준 구성으로 정규화했다. Function/Data/Contract/Pseudocode/Interface/Binding의 의미는 보존하고 이동한 Boundary의 링크만 갱신했다. 현재 Data 상세는 R3-C1 결과다. R4는 수신한 두 함수 독립 국소 PASS를 연결하고 공통 계약을 정리했다.

R1.1의 거버넌스 동기화·표현 Guard를 유지한다. R2는 기존 13개 후보+내부 1개를 재판정해 Core 18개 상세 계약을 작성하고 Module Function 목록/링크를 맞췄다. Data/Contract/Pseudocode/C Interface/Binding은 변경하지 않았다.

R3는 [32개 전수 판정](../40_DATA/00_DATA_OVERVIEW.md#type-decisions)과 [18개 Core 대응](../40_DATA/00_DATA_OVERVIEW.md#function-data-map)을 제공한다. Function/Module/Layer 본문과 Contract/Pseudocode/C Interface/Binding·Trace를 보존한 당시 이력은 해당 문서에 유지한다. [R2 국소 검토 이력/독립 PASS](../40_DATA/00_DATA_OVERVIEW.md#r2-local-review)를 연결한다. R4는 공통 계약, R5-C1은 최초 actual 세 갈래 판정·STOP 원 scope/다중 범위 정리, R6는 Header 후보를 작성하고 독립 PASS를 받았다. [R6 결과·자체 검수 이력](../../../../R6_RESULT_SUMMARY.md)은 불변 참고다. [R7 결과·자체 검수](../../../../R7_RESULT_SUMMARY.md)에 49 Trace·18 Core·8 시나리오·국소 보완을 기록했으며 일반 채팅 R7 독립 검수를 기다린다. B2-R·GATE-C·실제 구현/Build/보드 검증은 진행하지 않았다.
