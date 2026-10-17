# WINDOW — 이동·로컬 보호·상태 보고

Interface v2 · r1 · [규격과 연동 조건](../../07_CROSS_NODE_INTERFACE_RULES.md#status)

**[구현 보류 — 설계 유지]**. 예정 S32K144 노드는 DOMAIN의 상위 이동/STOP 목표를 소비하고 실제 이동·위치·끝단·끼임 보호 상태와 명령 처리 결과·고장을 제공한다. 설계는 유지하며 Firmware 구현과 실물 검증은 보류한다.

[Data 전체 타입](10_DATA.md) · [송수신 API](20_API.md) · [전체 시작](../../00_START_HERE.md) · [공유 CAN FD](../../03_COMMUNICATION/10_CAN_FD.md)

| 방향 | 메시지 | 로컬 함수·반환 |
|---|---|---|
| DOMAIN→WINDOW | `DOMAIN_PERMISSION` | `void WindowApp_OnDomainPermission(const NetDomainPermission *message, const NetCanRxMeta *meta)` |
| DOMAIN→WINDOW | `WINDOW_COMMAND` | `void WindowApp_OnDomainCommand(const NetWindowCommand *message, const NetCanRxMeta *meta)` |
| WINDOW→DOMAIN | `WINDOW_STATE` | `NetSubmitResult WindowNet_PublishState(const NetWindowState *message)` |
| WINDOW→DOMAIN | `WINDOW_EVENT` | `NetSubmitResult WindowNet_SendEvent(const NetWindowEvent *message)` |
| WINDOW→DOMAIN | `WINDOW_RESULT` | `NetSubmitResult WindowNet_SendResult(const NetWindowResult *message)` |
| WINDOW→DOMAIN | `WINDOW_FAULT` | `NetSubmitResult WindowNet_PublishFault(const NetWindowFault *message)` |

로컬 우선순위는 **AntiPinch > Local Switch > DOMAIN**이다. 로컬 STOP·끼임 보호는 기존 로컬 보호 모듈이 즉시 처리하며 Network API 호출·DOMAIN 허용·CAN 송수신이 끝나기를 기다리지 않는다. DOMAIN의 정당한 STOP은 이동 허용 상실만으로 전달을 막지 않되 출처·대상 기동·명령·Age·형식 검증은 유지한다.

현재 위치의 끝단 State와 새롭게 발생한 FULLY_OPENED/FULLY_CLOSED Event, 새 AntiPinch 발생과 실제 정지·출력 차단 확인, 현재 Fault와 발생 이력은 각각 다른 근거다. `DomainIf_RxWindowAntiPinch` 하나로 끝단 Event 전체를 대체하지 않는다.

**FUNCTION_POLICY_OPEN:** REQUEST_SEQUENCE↔C2 commandId 대응, END의 실제 소비 경계, 보호 완료 확인·Fault 품질/provenance 수용, VENT70↔80 및 명령 수용 200ms↔1초. 보류 중인 설계의 네 가지 보고와 명령/permission 의미는 유지한다. 미활성 WINDOW timeout으로 현재 구성에 상시 Fault를 만들지 않는다.

MOBILE 창문 원격제어는 현재 범위에 없으며 앱에 결과·사유를 표시하는 범위는 **[보류]**다. 실제 SDK/Firmware/Codec/Board/로컬 보호·CAN 통합 시험은 **NOT_EXECUTED**다. [DOMAIN API](../DOMAIN/20_API.md) · [기술 제약 목록](../../06_TECHNICAL_CONSTRAINTS.md) · [메시지48색인](../../05_MESSAGE_INDEX.md).
