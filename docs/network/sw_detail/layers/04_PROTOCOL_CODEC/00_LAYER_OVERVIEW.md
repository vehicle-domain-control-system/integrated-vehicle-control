# Protocol / Codec

[↑ Layer 지도](../../01_LAYER_MAP.md) · [Network 시작](../../00_START_HERE.md) · [Flow](../../04_FLOW_INDEX.md)

기존 [Layer 책임](../../../network_sw_architecture.md#4-layer별-책임)에 따라 field encode/decode와 Wire validation의 원 Source를 찾는다. UART Framer는 byte stream의 frame 경계·CRC·resync 책임이며 CAN은 Controller의 frame 경계를 사용한다. UART-C/M 외형이 일부 같아도 TYPE·Payload·Runtime은 독립이다.

| 계약 출처 | 현재 지위 |
|---|---|
| [공유 CAN FD](../../../interface/03_COMMUNICATION/10_CAN_FD.md) | 메시지 이름·방향과 R4 Wire 후보의 지위를 구별 |
| [UART-M](../../../interface/03_COMMUNICATION/20_UART_M.md) | ESP32↔DOMAIN 제품 경로 / `WIRE_PROPOSED` |
| [UART-C](../../../interface/03_COMMUNICATION/30_UART_C.md) | Pi↔CIS 제품 경로 / `WIRE_PROPOSED` |
| [명시 Enum↔Wire](../../../interface/04_COMMON/30_ENUM_WIRE_MAPPING.md) | 미표현 raw·UNKNOWN/INVALID 정상값 치환 금지 |
| [48 메시지 색인](../../../interface/05_MESSAGE_INDEX.md) | 상대 typed API·Data의 기존 연결 |

현재 공식 Network 트리는 `SOURCE_ONLY / IMPLEMENTATION_NOT_PRESENT`다. 과거 첨부 I1 Host Codec는 역사 증거이며 현재 저장소의 Codec 구현이나 이번 새 시험으로 표시하지 않는다. 존재하지 않는 C/H 함수 Module을 만들어 채우지 않고 [Function 색인의 선언 근거](../../03_FUNCTION_INDEX.md#source-evidence)를 구별한다.

[Transport](../05_TRANSPORT/00_LAYER_OVERVIEW.md)의 scheduling/lifetime과 기능 정책은 이 계층에서 재정의하지 않는다. 실제 Codec·Framer 상세 Behavior/Pseudocode·Host 작업은 이후 단계이며 U1 상태는 `BEHAVIOR_NOT_DESIGNED`다.
