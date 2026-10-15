# ESP32 — MOBILE↔Domain Wireless Gateway

Interface v2 · r1 · [규격과 연동 조건](../../07_CROSS_NODE_INTERFACE_RULES.md#status)

ESP32는 BLE MOBILE과 UART-M Domain 사이의 검증된 논리 Gateway 다. 등록·연결/근접 관측과 현재 peer/session binding을 소유하며 MOBILE App 활성은 신뢰 가능한 원 평가를 받아 검증한다. 차량 기능 명령 허용·실행·위험 판단은 Domain/기능 ECU가 담당한다.

**설계 초안 / BINDING_OPEN.** 기존 Gateway API는 선언이고 상세 Adapter convention은 제안이다. 실제 BLE/SDK/Firmware/Codec/Board 시험은 NOT_EXECUTED다.

| 먼저 볼 항목 | 문서 |
|---|---|
| 원 view·업데이트·멤버 | [Gateway Data](10_DATA.md) |
| Adapter→Core / Core→Transport | [반환형·전체 인자가 있는 Gateway API](20_API.md), [Query 호출 순서](20_API.md#query-entry) |
| 각 M_* subtype 처리와 private Query/Alive | [23 TYPE 검증·dispatch](25_PRIVATE_ROUTING.md#relay-schema), [Query 종결](25_PRIVATE_ROUTING.md#r2-query-consumer) |
| 초기 115200bps UART-M v2 | [통신 설정](../../03_COMMUNICATION/20_UART_M.md) |
| 실제 BLE 규격과 미정 | [BLE](../../03_COMMUNICATION/40_BLE.md) |
| 상대 Domain 입력/출력 | [Domain API](../DOMAIN/20_API.md) |

## 주요 처리 흐름

1. BLE Adapter는 유효한 MOBILE 논리 메시지·원 Request/Query ID를 검증된 현재 peer/session에 묶어 `Gateway_Interface_OnMobileMessage()`로 전달한다.
2. Gateway Core가 `Gateway_Interface_SendToDomain()`을 호출한다. Transport는 `M_REQUEST`/`M_QUERY`/`M_WARNING_ACK` schema를 구분하여 encode 한다.
3. UART Adapter는 Domain의 검증된 상태·결과·경고를 `Gateway_Interface_OnDomainMessage()`로 인계하고, Core가 `SendToMobile()`을 호출한다. vehicle 의미·원 AGE/식별을 보존한다.
4. 등록·연결/앱 활성/근접 Publish3 종은 각각 별도 producer 근거를 제공하고 Network-private M_CONTEXT compose 에서 합쳐 보낸다.

M_DOMAIN_ALIVE는 **Gateway/Network 링크 감독 private**이며 BLE MOBILE로 직접 중계하지 않는다. M_QUERY_END는 **Query stream 종결 private**이며 차량 명령 DONE으로 바꾸지 않는다. MOBILE 조회 UI 종결에 필요한 projection은 BLE schema가 확정된 뒤 연결한다.

기존 view의 broad category를 상세 TYPE으로 구분하는 [내부 convention](25_PRIVATE_ROUTING.md#relay-schema)은 PROPOSED/BINDING_OPEN이며 source/TTL/Session owner도 [기술 제약](../../06_TECHNICAL_CONSTRAINTS.md) 상태다. 상세 subtype·품질·수명 검사를 없애고 하나의 byte 중계로 압축하지 않는다.
