# 기존9 Module 탐색

[전체 Layer/Function](../03_LAYER_MODULE_INDEX.md) · [UART-M23](../05_UART_M_OVERVIEW.md) · [CAN22](../04_CAN_FD_OVERVIEW.md)

| Module | Layer | 기존 책임 | 상세 |
|---|---|---|---|
| [M01](M01_SOURCE_CONTEXT.md) | L01 | F01~F03 | 원본·문맥 |
| [M02](M02_QUERY_CONTROL.md) | L01 | F04/F05 | Query |
| [M03](M03_TYPED_HANDOFF.md) | L02 | F06~F10 | typed 인계 |
| [M04](M04_MESSAGE_SERVICE.md) | L03 | F11~F13 | 접수·순서·AGE |
| [M05](M05_CAN_CODEC.md) | L04 | F14/F15 | CAN22 |
| [M06](M06_UART_CODEC.md) | L04 | F16~F18 | UART-M23 / UART-C3 독립 Stream·Codec |
| [M07](M07_BUFFER_OUTCOME.md) | L05 | F19~F21 | 안정 버퍼·outcome |
| [M08](M08_DEVICE_PORT.md) | L05 | F22/F23 | Driver/복구 |
| [M09](M09_OBSERVATION.md) | L05 | F24/F25 | 원 관측·ST12 Health |

문서 추가는 Module/Function/State 개수를 늘리지 않는다. 기존 5 Layer·9 Module·25 Function 책임·12 State에 UART-M과 [UART-C 3종 상세](../06_UART_C_OVERVIEW.md)를 연결하며, 실제 제품 바인딩은 별도다. 기존 Interface·C2 합의·Owner 35개 미결과 HOLD를 유지한다.

## DOMAIN Adapter·공통 Service 찾기

[전체 개요](../07_DOMAIN_ADAPTER_SERVICE.md)에서 M01/M02의 원본·Query, M03의 등록·TxPort·Rx, M04/M07/M08의 접수·Driver 수명, M09의 실패·Health로 이동한다. 기존9 Module과25 Function 책임을 유지하며 C2 공개 본체나 새 전역 Service Manager를 추가하지 않는다.

[공통 자료](../data/NETWORK_SERVICE_DATA.md) · [DOMAIN 명령 흐름](../flows/DOMAIN_COMMAND_TO_NETWORK.md) · [Network 수신 흐름](../flows/NETWORK_TO_DOMAIN.md)
