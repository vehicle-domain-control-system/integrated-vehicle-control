# Message Service

[↑ Layer 지도](../../01_LAYER_MAP.md) · [Network 시작](../../00_START_HERE.md) · [Function](../../03_FUNCTION_INDEX.md)

링크별 TX pending·scheduling·전송 순번과 periodic/change/event 처리를 맡는 기존 계층이다. [Architecture의 전송 의미](../../../network_sw_architecture.md#6-message-service의-전송-의미)는 Current State/Warning, Discrete Command/Result/Event, Query 응답을 구별한다. 기능값의 정책적 의미는 기존 Owner에 남긴다.

| 현재 탐색 대상 | 원 Source |
|---|---|
| 보관·대체·순서의 지위 | [Message Service 책임](../../../network_sw_architecture.md#6-message-service의-전송-의미) |
| local 인계·accepted 이후 실패 | [API 보관·재전송](../../../interface/04_COMMON/20_API_USAGE_AND_FAILURE.md#section-lifetime-ownership), [Health·실패](../../../interface/04_COMMON/20_API_USAGE_AND_FAILURE.md#section-network-health) |
| 호출의 typed 입구 | [Application Port Module들](../02_APPLICATION_PORT/00_LAYER_OVERVIEW.md) |

현재 Network 트리에 이 하위 계층의 실행 구현·함수 선언 근거가 없다(`SOURCE_ONLY / IMPLEMENTATION_NOT_PRESENT`). U1은 기존 설계에 연결하며 빈 함수 Module·새 submit API·shared runtime을 생성하지 않는다. CAN/UART-C/UART-M의 runtime은 독립이며 [Protocol/Codec](../04_PROTOCOL_CODEC/00_LAYER_OVERVIEW.md)와 [Transport](../05_TRANSPORT/00_LAYER_OVERVIEW.md)의 책임을 구별한다.

U4D 이후 상세 설계·Host 구현은 별도 Gate 대상이다. 현재 상세 문서는 `PLANNED_ONLY / BEHAVIOR_NOT_DESIGNED`다.


## U4D CAN common detailed design navigation

2026-10-12 사용자 직접지시에 따른 문서 상세설계 후보. G3 독립/로컬인수 PASS 유지; 기준 `9e0b1e5d97d32971bb0863e144dd2ef515f2ccf4`. U4D_PROPOSED_PRIVATE / DESIGN_ONLY, G4D 독립검수 PENDING, U4C NOT_STARTED, target PENDING_USER_PUSH.

[CAN 16개 private 후보 함수](../../05_CAN_COMMON_FUNCTION_INDEX.md) · [CAN TX/RX Flow와 22개 후속시험 설계](../../flows/30_CAN_COMMON_TX_RX_FLOW.md). 기존본문·144FN·U2계약은보존한다.

[CAN Module Map](10_CAN_SERVICE/00_MODULE_MAP.md) · [Behavior](10_CAN_SERVICE/10_BEHAVIOR.md) · [Pseudocode](10_CAN_SERVICE/20_PSEUDOCODE.md)
