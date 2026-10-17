# ESP32 — MOBILE↔DOMAIN 무선 게이트웨이

Interface v2 · r1 · [규격과 연동 조건](../../07_CROSS_NODE_INTERFACE_RULES.md#status)

**연동 상태:** P4-A 호환 구현은 담당자 보고이며 실제 UUID/Properties·MTU·fragmentation의 Network 채택과 P4-B 보드·휴대폰 검증은 대기다. [BLE 채택 자료](../../03_COMMUNICATION/40_BLE.md#p4a-adoption) 확인 전 OPEN을 유지한다. H01은 공통 헤더 필드 또는 생산원별 Adapter 순번의 실제 선택·경로/버전·M_CONTEXT38B 연결 확인 전 HOLD이며, 내부 구현 책임은 ESP32 담당 범위다.

ESP32는 BLE MOBILE과 UART-M DOMAIN 사이에서 검증된 논리 메시지를 중계하는 게이트웨이다. 등록·현재 연결의 공동 관측과 근접 관측을 관리하며, 현재 상대 기기·Session의 연결 관계를 유지한다. MOBILE 앱 활성 상태는 신뢰할 수 있는 원본 평가에 근거해 검증한다. 차량 기능 명령 허용·실행·위험 판단은 DOMAIN/기능 ECU가 담당한다.

**설계 초안 / BINDING_OPEN.** 기존 Gateway API는 선언이고 상세 Adapter 연결 규칙은 제안 상태다. 실제 BLE/SDK/Firmware/Codec/Board 시험은 NOT_EXECUTED다.

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
2. Gateway Core가 `Gateway_Interface_SendToDomain()`을 호출한다. Transport는 `M_REQUEST`/`M_QUERY`/`M_WARNING_ACK` 메시지별 형식을 구분해 인코딩한다.
3. UART Adapter는 DOMAIN의 검증된 상태·결과·경고를 `Gateway_Interface_OnDomainMessage()`로 인계하고, Core가 `SendToMobile()`을 호출한다. 차량 기능의 의미·원본 AGE·식별자를 보존한다.
4. 등록·연결/앱 활성/근접 Publish 3종은 각각 독립된 생산자 근거를 제공하며, 내부 M_CONTEXT 조립 단계에서 이를 합쳐 전송한다.

M_DOMAIN_ALIVE는 **Gateway/Network 링크 감독 private**이며 BLE MOBILE로 직접 중계하지 않는다. M_QUERY_END는 **Query stream 종결 private**이며 차량 명령 DONE으로 바꾸지 않는다. MOBILE 조회 UI에 종료 상태를 전달하는 변환은 BLE 형식이 확정된 뒤 연결한다.

기존 view의 포괄적인 분류를 상세 TYPE으로 구분하는 [내부 처리 규칙](25_PRIVATE_ROUTING.md#relay-schema)은 PROPOSED/BINDING_OPEN이며 source/TTL/Session owner도 [기술 제약](../../06_TECHNICAL_CONSTRAINTS.md) 상태다. 상세 subtype·품질·수명 검사를 없애고 하나의 byte 중계로 압축하지 않는다.
