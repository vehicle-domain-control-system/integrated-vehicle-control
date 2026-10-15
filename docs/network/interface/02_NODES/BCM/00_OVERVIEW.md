# BCM — 노드 인터페이스 안내

Interface v2 · r1 · [규격과 연동 조건](../../07_CROSS_NODE_INTERFACE_RULES.md#status)

[전체 시작](../../00_START_HERE.md) · [개요](00_OVERVIEW.md) · [Data](10_DATA.md) · [API](20_API.md)

S32K144에서 도어·Fan/온도 출력·실내 조명을 실행하고 실제 관측·결과·보호 발생을 보고한다. DOMAIN이 상위 목표/권한을 결정하며 BCM은 로컬 보호와 실제 피드백 확인을 담당한다.

**PROPOSED_DESIGN · IMPLEMENTATION_BINDING_HOLD.** Data/API는 계약 후보이며 실제 firmware·보드 연결은 검증 전이다. BCM INTERIOR_LIGHT는 유지, EXTERIOR_LIGHT 제외. 공용 CAN FD는 단일 공유 버스이고 nominal 500kbps/data 2Mbps는 WIRE_PROPOSED다. WINDOW는 **[구현 보류 — 설계 유지]**다.

## 구현자가 먼저 볼 문서

| 문서 | 바로 확인할 내용 |
|---|---|
| [10_DATA](10_DATA.md) | 전체 struct/alias/enum과 멤버별 타입·범위·품질·원 소유자 |
| [20_API](20_API.md) | 송신/수신 정확한 반환형·함수명·인자·호출·실패·비동기 수명 |
| [공통 Data/ID](../../04_COMMON/10_DATA_AND_ID_RULES.md) · [호출·실패](../../04_COMMON/20_API_USAGE_AND_FAILURE.md) | BOOT/TX_SEQUENCE와 원 관측 순번/Age/command/result 구분 |
| [CAN FD](../../03_COMMUNICATION/10_CAN_FD.md) | 실제 메시지 후보 byte/DLC/시간·프레이밍·CRC OPEN |

## 이 노드의 송수신 경계

| 메시지 | 방향·상대 | 로컬 함수 / 정확한 상세 |
|---|---|---|
| [`BCM_DOOR_COMMAND`](../../03_COMMUNICATION/10_CAN_FD.md#message-bcm-door-command) | DOMAIN→BCM | [`BcmApp_OnDomainDoorCommand()`](20_API.md#api-bcmapp_ondomaindoorcommand) |
| [`BCM_CLIMATE_COMMAND`](../../03_COMMUNICATION/10_CAN_FD.md#message-bcm-climate-command) | DOMAIN→BCM | [`BcmApp_OnDomainClimateCommand()`](20_API.md#api-bcmapp_ondomainclimatecommand) |
| [`BCM_LIGHT_COMMAND`](../../03_COMMUNICATION/10_CAN_FD.md#message-bcm-light-command) | DOMAIN→BCM | [`BcmApp_OnDomainLightCommand()`](20_API.md#api-bcmapp_ondomainlightcommand) |
| [`BCM_RESULT`](../../03_COMMUNICATION/10_CAN_FD.md#message-bcm-result) | BCM→DOMAIN | [`BcmNet_SendResult()`](20_API.md#api-bcmnet_sendresult) |
| [`BCM_EVENT`](../../03_COMMUNICATION/10_CAN_FD.md#message-bcm-event) | BCM→DOMAIN | [`BcmNet_SendEvent()`](20_API.md#api-bcmnet_sendevent) |
| [`BCM_DOOR_STATE`](../../03_COMMUNICATION/10_CAN_FD.md#message-bcm-door-state) | BCM→DOMAIN | [`BcmNet_PublishDoorState()`](20_API.md#api-bcmnet_publishdoorstate) |
| [`BCM_CLIMATE_STATE`](../../03_COMMUNICATION/10_CAN_FD.md#message-bcm-climate-state) | BCM→DOMAIN | [`BcmNet_PublishClimateState()`](20_API.md#api-bcmnet_publishclimatestate) |
| [`BCM_LIGHT_STATE`](../../03_COMMUNICATION/10_CAN_FD.md#message-bcm-light-state) | BCM→DOMAIN | [`BcmNet_PublishLightState()`](20_API.md#api-bcmnet_publishlightstate) |
| [`BCM_STATUS`](../../03_COMMUNICATION/10_CAN_FD.md#message-bcm-status) | BCM→DOMAIN | [`BcmNet_PublishStatus()`](20_API.md#api-bcmnet_publishstatus) |
| [`DOMAIN_PERMISSION`](../../03_COMMUNICATION/10_CAN_FD.md#message-domain-permission) | DOMAIN→BCM | [`BcmNetPriv_OnDomainPermission()`](20_API.md#api-bcmnetpriv_ondomainpermission) · NETWORK_PRIVATE 감시 |

## 소유권과 현재 결정

- DOMAIN C2가 최초 command_id를 만든다. BCM은 받은 원 DOMAIN Boot/명령 ID를 결과에 보존하고 실행 결과·로컬 발생은 직접 판단한다.
- Network는 세대/형식/수용 Age·버퍼/순번을 관리한다. App 상태 평가 sequence와 CAN frame TX_SEQUENCE는 다르다.
- 상태 Publish와 Result/Event는 수명이 달라 typed 경계를 유지한다. 동일 책임의 실제 기존 Port가 확인되면 새 wrapper와 중복 설치하지 않는다.
- 실제 명령 결과·`confirmation`의 제한은 [BCM_RESULT](../../03_COMMUNICATION/10_CAN_FD.md#message-bcm-result), 통신 관측→BCM 진단은 [BCM_STATUS](../../03_COMMUNICATION/10_CAN_FD.md#message-bcm-status)의 PROPOSED 권장안을 따른다. 명령 상한 200ms 후보↔구 SysRS 1초, health→진단 owner 연결과 로컬 실패 sink는 OPEN이다. 물리 점등 확인 미지원에서 적용 성공을 점등 완료로 표시하지 않는다.

[팀 미결](../../06_TECHNICAL_CONSTRAINTS.md) · [48 메시지 색인](../../05_MESSAGE_INDEX.md). 실제 SDK/Firmware/Codec/Board/무선/카메라 통합 시험은 **NOT_EXECUTED**다.
