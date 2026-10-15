# DOMAIN — S32K344 차량 정책·통합 경계

Interface v2 · r1 · [규격과 연동 조건](../../07_CROSS_NODE_INTERFACE_RULES.md#status)

DOMAIN은 MOBILE 원 요청을 수용하고 차량 허용·기능 정책·명령 중재·원 요청별 결과를 소유한다. Network는 CAN/UART framing·검증·ID 보존·AGE 누적·typed 인계를 맡는다. 실제 ECU 실행 결과는 BCM/WINDOW 등 실행 owner가 생산한다.

**설계 초안 / BINDING_OPEN.** Source API는 선언이며 Network Adapter는 제안이다. 실제 SDK/Firmware/Codec/보드 시험은 NOT_EXECUTED다.

| 먼저 볼 항목 | 문서 |
|---|---|
| 무엇을 만들고 어떤 필드를 채우나 | [Data와 원 소유권](10_DATA.md) |
| App이 호출할 TX / Network가 인계할 RX | [반환형·전체 인자가 있는 C2 API](20_API.md), [Door 송신→결과](20_API.md#door-flow) |
| Port26·Query18·내부 routing | [TxPort 연결](25_PRIVATE_ROUTING.md#tx-port), [Query 처리](25_PRIVATE_ROUTING.md#query-flow) |
| UART-M과 shared CAN frame | [UART-M](../../03_COMMUNICATION/20_UART_M.md), [CAN FD](../../03_COMMUNICATION/10_CAN_FD.md) |
| 전체 동료 API | [메시지 48 색인](../../05_MESSAGE_INDEX.md) |

## 실제 연결과 책임

MOBILE↔BLE↔ESP32↔UART-M v2(초기 115200bps)↔DOMAIN↔**단일 공유 CAN FD**↔BCM/CIS/별도 S32K344 VSS/WINDOW. Pi(camera)는 UART-C↔CIS 경로이고 DOMAIN과 직접 연결하지 않는다. EXTERIOR_LIGHT는 제외하며 BCM INTERIOR_LIGHT는 유지한다. WINDOW는 **[구현 보류 — 설계 유지]**다. nominal500kbps/data2Mbps는 제안 상태다.

## 주요 구현 흐름

1. Domain owner가 `DomainIf_BcmDoorCommand_t`를 만들어 `DomainIf_TxBcmDoorCommand()`를 호출한다. commandId는 원 Domain ID 하나다.
2. C2의 `DomainIf_TxPort_t.txBcmDoorCommand`가 등록된 Network Adapter로 연결된다. local OK 전에 안정된 복사/encode를 완료한다.
3. Network가 대상 Boot/프레임순번/CRC/AGE를 보완한다. BCM은 명령을 수용·실행하고 실제 `BCM_RESULT`를 보낸다.
4. DOMAIN Codec는 result를 `DomainIf_BcmCommandResult_t`로 명시 투영한 뒤 `DomainIf_RxBcmCommandResult()`에 인계한다. Domain owner가 원 요청 결과를 기록·출력한다.

Query는 차량 명령이 아니다. passive Session 검사 후 C2 snapshot Getter를 호출하고 같은 상태 schema encoder로 응답한다. M_QUERY_END는 stream 종결이며 M_DOMAIN_ALIVE는 링크 감독 전용이다. 두 메시지를 별도 App 명령 결과 함수로 공개하지 않는다.

## 연동 전 확인이 필요한 기술 조건

TxPort/RxObserver·필수 local-failure sink의 Firmware 등록, raw 미지원 enum, source TTL/명령 수용 기한은 실제 binding을 확인해야 한다. [권한 합성·품질](../../03_COMMUNICATION/10_CAN_FD.md#message-domain-permission), [원 CAN↔C2 Snapshot 연결](25_PRIVATE_ROUTING.md#r2-origin-join), [VSS 변환](../VSS/10_DATA.md#r2-status-proposal), WINDOW END/Fault owner도 PROPOSED/BINDING_OPEN 또는 구현 보류 상태다. 이 조건이 있는 항목은 상태를 임의 완성하지 않고 [기술 조건 확인 목록](../../06_TECHNICAL_CONSTRAINTS.md)을 적용한다.
