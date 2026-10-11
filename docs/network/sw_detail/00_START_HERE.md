# Network SW 탐색 시작 — U1 지도 / U2 상세 후보

이 지도는 기존 Network 문서를 **Network → Layer → Module → Function** 순서로 찾는 입구다. 함수 이름·Data·Wire의 정본은 연결된 기존 문서이며, 여기의 FN-ID는 탐색용 식별자다.

| 현재 지위 | 값 |
|---|---|
| 단계 / 문서 | `U2_DOCUMENT_BEHAVIOR_PSEUDOCODE / DESIGN_CANDIDATE` |
| 이전 판정 | `G0=CONDITIONAL_PASS`, `G1_INDEPENDENT=PASS` (DOCUMENT_MAP 범위) |
| 이번 검수 | `U2_SELF_CHECK` 대상, `G2_INDEPENDENT=PENDING` |
| 기존 Source 기준 | `docs/network@c60dcf65de52b7fb28e0464d616ba00d350890bb` |
| G1 지도 인수 SHA | `ad934964618c7f11260ed58dcbd86f881cb99086` |
| 새 문서 반영 SHA | `target_sha=PENDING_USER_PUSH` |
| 구현·계약 | 기존 `DECLARED_SOURCE / PROPOSED_DESIGN / WIRE_PROPOSED / BINDING_OPEN` 유지 |

## 원하는 자료로 이동

- [Layer 지도](01_LAYER_MAP.md): 기존 7계층과 Common Infra의 책임.
- [Node 지도](02_NODE_MAP.md): DOMAIN/CIS/BCM/VSS/WINDOW 및 외부 ESP32/Pi 자료.
- [Function 색인](03_FUNCTION_INDEX.md): 원형 출처와 Module 복귀 경로.
- [Flow 색인](04_FLOW_INDEX.md): `CIS_REAR`, `DOMAIN_PERMISSION`의 논리 출처.
- U2 양단 상세 후보: [CIS_REAR](flows/10_CIS_REAR_FLOW.md), [DOMAIN_PERMISSION](flows/20_DOMAIN_PERMISSION_FLOW.md).
- [최소 CAN LinkPort 의미](layers/06_DRIVER_PORT/10_CAN_LINKPORT_SEMANTICS.md): 능력·소유·accept/terminal/RX/health/time 경계.

Function 색인은 U0의 `DOCUMENTED_API 116 + DOCUMENTED_PRIVATE_API 27` **143개 목록 행**을 인수하고, 기존 Source에서 확인한 `DomainNet_PublishPermission` 1행을 별도 출처로 더한 **144개 선언 탐색 항목**을 제공한다. 구현 함수 수 또는 신규 API 수가 아니다. Header 메타데이터·실제 구헤더·첨부 I1 함수는 [별도 선언 근거](03_FUNCTION_INDEX.md#source-evidence)로 구분한다.

## 기존 정본과 전체 Source

[SW Architecture](../network_sw_architecture.md)와 [Network 설계 배경](../network_design_draft_v0.1.md)은 원래 책임·토폴로지를 설명한다. Interface 39개 문서는 아래 공통 입구와 [Node별 원문](02_NODE_MAP.md#source-coverage)에서 직접 찾을 수 있다.

| 자료 | 기존 Source |
|---|---|
| Interface 읽기·지위 | [시작](../interface/00_START_HERE.md), [전체 연결·책임](../interface/01_NETWORK_OVERVIEW.md) |
| CAN 계약 후보 | [공유 CAN FD](../interface/03_COMMUNICATION/10_CAN_FD.md) |
| 제품 UART 계약 후보 | [ESP32↔DOMAIN UART-M](../interface/03_COMMUNICATION/20_UART_M.md), [Pi↔CIS UART-C](../interface/03_COMMUNICATION/30_UART_C.md) |
| 외부 Peer 계약 | [MOBILE↔ESP32 BLE 경계](../interface/03_COMMUNICATION/40_BLE.md) |
| 공통 의미·수명 | [Data·ID·AGE](../interface/04_COMMON/10_DATA_AND_ID_RULES.md), [API 사용·실패](../interface/04_COMMON/20_API_USAGE_AND_FAILURE.md), [Enum↔Wire](../interface/04_COMMON/30_ENUM_WIRE_MAPPING.md) |
| 상대 메시지·미합의 | [48 메시지 색인](../interface/05_MESSAGE_INDEX.md), [팀 OPEN](../interface/06_OPEN_FOR_TEAM.md) |

## 사용 범위와 다음 문서

S32K144/344 측 Network가 개발 범위이며 ESP32 Gateway 내부·Pi Application·기능 정책은 해당 Owner의 책임이다. WINDOW는 **[구현 보류 — 설계 유지]**, BCM `INTERIOR_LIGHT`는 유지, `EXTERIOR_LIGHT`는 제외다. 공유 하위 계층의 현 Source는 설계 문서이며 현재 Network 트리에 실행 구현이 없다(`SOURCE_ONLY / IMPLEMENTATION_NOT_PRESENT`).

U2 ACTIVE 7개 함수의 `10_BEHAVIOR.md`와 `20_PSEUDOCODE.md`를 5개 Module에 분리 작성했다. [활성 함수별 왕복 링크](03_FUNCTION_INDEX.md#u2-active-detail)에서 찾는다. 나머지 137개 선언은 상세 `PLANNED_ONLY / BEHAVIOR_NOT_DESIGNED`이며, 구현·Header·Wire 채택을 승인한 것이 아니다. 후속 U3/U4D는 G2 독립 판정과 별도 사용자 허용 범위에 따른다.

`NET_SUBMIT_OK`는 로컬 안정 인계만 의미한다. Wire 채택·mm↔cm·enum/raw·Permission 원 Producer와 **150 ms `SOURCE_LIMIT_NOT_AGREED`**·SDK/HW/시간 능력은 OPEN이다. [원본 부재 C2 Header 3개](03_FUNCTION_INDEX.md#missing-source)는 `MISSING_SOURCE / OPEN-C2-SOURCE`로 유지한다. 추가로 발견한 `Gateway_Types.h` 선언 Snapshot 부재는 [별도 OPEN](02_NODE_MAP.md#missing-source)인 `KNOWN_PREEXISTING / OPEN-GW-TYPES-SOURCE`이며 기존 세 경로와 구분한다. 새 Host/Firmware/Board 시험 결과가 이 지도에 추가된 것은 아니다.
