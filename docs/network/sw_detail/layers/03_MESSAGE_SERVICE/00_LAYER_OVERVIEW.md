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
