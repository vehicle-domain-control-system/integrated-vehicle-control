# Layer 지도

[↑ Network 시작](00_START_HERE.md) · [Node](02_NODE_MAP.md) · [Function](03_FUNCTION_INDEX.md) · [Flow](04_FLOW_INDEX.md)

기존 [Architecture의 Layer 책임](../network_sw_architecture.md#4-layer별-책임)을 따라 7개 Network 계층을 탐색한다. Application/Policy는 기능 Owner의 외부 경계이며 Common Infra는 횡단 도우미다. 탐색용 Module Map 배치는 실제 코드 분할이나 새 공개 API를 확정하지 않는다.

| Layer | 책임과 경계 | 하위 입구 |
|---|---|---|
| Node Communication | 원 identity·AGE·context·correlation·의미 투영. 기능 정책은 Owner에 남김 | [Node Communication](layers/01_NODE_COMMUNICATION/00_LAYER_OVERVIEW.md) |
| Application Port | Node와 Stack 사이의 typed 호출·반환·수명 경계 | [Application Port](layers/02_APPLICATION_PORT/00_LAYER_OVERVIEW.md) |
| Message Service | 링크별 pending·전송 순번·current/discrete/query 보관 의미 | [Message Service](layers/03_MESSAGE_SERVICE/00_LAYER_OVERVIEW.md) |
| Protocol / Codec | 명시 field 변환·Wire 검증, UART Framer의 byte-stream 경계 | [Protocol / Codec](layers/04_PROTOCOL_CODEC/00_LAYER_OVERVIEW.md) |
| Transport | bounded TX/RX 수명·안정 인계·Driver 수용/종결 관측 | [Transport](layers/05_TRANSPORT/00_LAYER_OVERVIEW.md) |
| Driver | 실제 HW/SDK 송수신 primitive·callback·local 오류 경계 | [Driver / Port](layers/06_DRIVER_PORT/00_LAYER_OVERVIEW.md) |
| HAL Adapter / BSP | vendor SDK·보드 차이 격리, 의미 정책은 상위 Owner에 유지 | [HAL / BSP](layers/07_HAL_BSP/00_LAYER_OVERVIEW.md) |

CAN, UART-C, UART-M의 Codec·Queue·Runtime은 독립이다. 현재 없는 하위 구현 함수나 공통 전역 Router를 탐색 구조 때문에 생성하지 않는다. 공유 하위 계층은 `SOURCE_ONLY / IMPLEMENTATION_NOT_PRESENT`로 기존 근거에 연결한다.

<a id="common-infra"></a>
## Common Infra — 횡단 도우미

Timebase, 얇은 RTOS/OS adapter, bounded diagnostics는 [기존 전체 구조](../network_sw_architecture.md#3-network-sw-전체-구조)의 보조 역할이다. 여덟 번째 Layer 또는 UART/CAN routing Owner가 아니다. 의미 자료는 [공통 Data/ID](../interface/04_COMMON/10_DATA_AND_ID_RULES.md), [호출·수명/Health](../interface/04_COMMON/20_API_USAGE_AND_FAILURE.md), [명시 Enum 변환](../interface/04_COMMON/30_ENUM_WIRE_MAPPING.md)에서 찾는다.

FreeRTOS와 superloop의 타깃 차이 및 [미확정 자원·바인딩](../network_sw_architecture.md#11-현재-확정하지-않는-항목)은 유지한다. 실제 SDK·핀·Driver/HAL 결합은 U10의 별도 근거·Gate 대상이다.

현재 지도는 `U1 / DOCUMENT_MAP_DRAFT`, 상세 Behavior·Pseudocode는 `PLANNED_ONLY`, G1은 `PENDING_INDEPENDENT_REVIEW`다.


## U4D CAN common detailed design navigation

2026-10-12 사용자 직접지시에 따른 문서 상세설계 후보. G3 독립/로컬인수 PASS 유지; 기준 `9e0b1e5d97d32971bb0863e144dd2ef515f2ccf4`. U4D_PROPOSED_PRIVATE / DESIGN_ONLY, G4D 독립검수 PENDING, U4C NOT_STARTED, target PENDING_USER_PUSH.

[CAN 16개 private 후보 함수](05_CAN_COMMON_FUNCTION_INDEX.md) · [CAN TX/RX Flow와 22개 후속시험 설계](flows/30_CAN_COMMON_TX_RX_FLOW.md). 기존본문·144FN·U2계약은보존한다.
