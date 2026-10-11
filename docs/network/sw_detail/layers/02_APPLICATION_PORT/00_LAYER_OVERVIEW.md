# Application Port

[↑ Layer 지도](../../01_LAYER_MAP.md) · [Network 시작](../../00_START_HERE.md) · [Node](../../02_NODE_MAP.md) · [Function](../../03_FUNCTION_INDEX.md)

기존 [Layer 책임](../../../network_sw_architecture.md#4-layer별-책임)에 따라 Node와 링크 Stack 사이의 typed 함수 선언·반환·포인터 수명 출처를 찾는다. 실제 이름·원형은 원 문서를 유지하며 vendor SDK 타입을 공개 호출 경계에 도입하지 않는다. 반환의 로컬 인계 의미는 [공통 API 규칙](../../../interface/04_COMMON/20_API_USAGE_AND_FAILURE.md#section-return-status)에 있다.

## Node별 Module

| Node | Module Map | 소유·상태 |
|---|---|---|
| DOMAIN | [DOMAIN](DOMAIN/00_MODULE_MAP.md#functions) | C2 선언 참고 / DOMAIN 대상 Header 원본 부재 / `BINDING_OPEN` |
| CIS | [CIS](CIS/00_MODULE_MAP.md#functions) | CAN+UART-C typed 경계 / `PROPOSED_DESIGN` |
| BCM | [BCM](BCM/00_MODULE_MAP.md#functions) | CAN Command/Result/State/Event / `INTERIOR_LIGHT` 유지 |
| VSS | [VSS](VSS/00_MODULE_MAP.md#functions) | CAN Event/Warning/Status / 기능·음향 정책은 VSS Owner |
| WINDOW | [WINDOW](WINDOW/00_MODULE_MAP.md#functions) | **DEFERRED / [구현 보류 — 설계 유지]** |
| ESP32 | [ESP32 Peer](ESP32/00_MODULE_MAP.md#functions) | 외부 계약·선언 참고 / 내부 Gateway 구현은 외부 담당 |
| Pi | [Pi Peer](PI/00_MODULE_MAP.md#functions) | 외부 UART-C 계약 참고 / Vision 앱 구현은 외부 담당 |

CIS/BCM/VSS API 문서에 들어 있는 private 함수는 각 Module의 `network-private` 절에 함께 색인하며 원 분류·Node Communication 책임을 구별한다. 공유 하위 Stack는 노드별로 복제하지 않고 [Message Service](../03_MESSAGE_SERVICE/00_LAYER_OVERVIEW.md), [Protocol/Codec](../04_PROTOCOL_CODEC/00_LAYER_OVERVIEW.md)로 연결한다.

`DECLARED_SOURCE`는 문서상 제공 선언의 지위이며 현재 대상 Header 확보·Firmware 구현·팀 채택을 보장하지 않는다. [부재 Source](../../03_FUNCTION_INDEX.md#missing-source)와 [추가 선언 근거](../../03_FUNCTION_INDEX.md#source-evidence)를 확인한다. 향후 모듈별 `10_BEHAVIOR.md`·`20_PSEUDOCODE.md`는 별도 계획이며 현재 생성되지 않았다.
