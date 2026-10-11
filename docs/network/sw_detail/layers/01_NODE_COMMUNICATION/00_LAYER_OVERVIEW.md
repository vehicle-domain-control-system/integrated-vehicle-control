# Node Communication

[↑ Layer 지도](../../01_LAYER_MAP.md) · [Network 시작](../../00_START_HERE.md) · [Node](../../02_NODE_MAP.md) · [Function](../../03_FUNCTION_INDEX.md)

기존 [Layer 책임](../../../network_sw_architecture.md#4-layer별-책임)에 따라 원 identity·AGE, session/context, request/result correlation과 링크 사이 의미 투영을 찾는다. 기능 허용·차량 위험·실행 결과는 기존 Application/Policy Owner가 판단한다. UART와 CAN Stack 사이에 직접 호출 또는 전역 Router를 추가하지 않는다.

## Module 입구

| 탐색 Module | 실제 경계·지위 |
|---|---|
| [DOMAIN private](DOMAIN_PRIVATE/00_MODULE_MAP.md#functions) | 기존 Permission 제출·수신 투영·Query/Alive 후보. `PROPOSED_PRIVATE / BINDING_OPEN`, 실제 구현 없음 |
| [ESP32 private Peer](ESP32_PRIVATE/00_MODULE_MAP.md#functions) | 외부 Gateway의 Context/Alive/Query 종결 계약 참고. 내부 구현은 ESP32 담당 |

별도 Module 파일로 분할하지 않은 내부 경계도 찾아볼 수 있다: [CIS Permission relay](../02_APPLICATION_PORT/CIS/00_MODULE_MAP.md#network-private), [BCM 감시](../02_APPLICATION_PORT/BCM/00_MODULE_MAP.md#network-private), [VSS 감시](../02_APPLICATION_PORT/VSS/00_MODULE_MAP.md#network-private). AppPort 문서와 함께 배치했어도 Network-private 책임·원 분류를 유지하며 Application 공개 의무로 바꾸지 않는다.

입력·출력의 typed 호출은 [Application Port](../02_APPLICATION_PORT/00_LAYER_OVERVIEW.md)로, 링크별 전송 대기는 [Message Service](../03_MESSAGE_SERVICE/00_LAYER_OVERVIEW.md)로 이동한다. 상세 Behavior·Pseudocode는 `PLANNED_ONLY / BEHAVIOR_NOT_DESIGNED`, G1은 `PENDING_INDEPENDENT_REVIEW`다.
