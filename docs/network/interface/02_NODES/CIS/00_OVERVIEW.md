# CIS — 노드 인터페이스 안내

[전체 시작](../../00_START_HERE.md) · [개요](00_OVERVIEW.md) · [Data](10_DATA.md) · [API](20_API.md)

센서 ECU에서 환경·후방 관측을 생성하고 Pi 카메라 결과를 취합하여 DOMAIN에 보고한다. Pi는 UART-C로 CIS에만 연결되며 DOMAIN과 직접 연결하지 않는다.

**공유 초안: PROPOSED_DESIGN · IMPLEMENTATION_BINDING_HOLD.** 아래 Data/API는 구체 계약 후보이며 실제 firmware 설치/보드 통합 승인이 아니다. 공용 CAN FD는 단일 공유 버스이고 nominal 500kbps/data 2Mbps는 WIRE_PROPOSED다. WINDOW는 **[구현 보류 — 설계 유지]**다.

## 구현자가 먼저 볼 문서

| 문서 | 바로 확인할 내용 |
|---|---|
| [10_DATA](10_DATA.md) | 전체 struct/alias/enum과 멤버별 타입·범위·품질·원 소유자 |
| [20_API](20_API.md) | 송신/수신 정확한 반환형·함수명·인자·호출·실패·비동기 수명 |
| [공통 Data/ID](../../04_COMMON/10_DATA_AND_ID_RULES.md) · [호출·실패](../../04_COMMON/20_API_USAGE_AND_FAILURE.md) | BOOT/TX_SEQUENCE와 원 관측 순번/Age/command/result 구분 |
| [CAN FD](../../03_COMMUNICATION/10_CAN_FD.md) · [UART-C](../../03_COMMUNICATION/30_UART_C.md) | 실제 메시지 후보 byte/DLC/시간·프레이밍·CRC OPEN |

## 이 노드의 송수신 경계

| 메시지 | 방향·상대 | 로컬 함수 / 정확한 상세 |
|---|---|---|
| [`DOMAIN_PERMISSION`](../../03_COMMUNICATION/10_CAN_FD.md#message-domain-permission) | DOMAIN→CIS | [`CisApp_OnDomainPermission()`](20_API.md#api-cisapp_ondomainpermission) |
| [`CIS_REAR`](../../03_COMMUNICATION/10_CAN_FD.md#message-cis-rear) | CIS→DOMAIN | [`CisNet_PublishRear()`](20_API.md#api-cisnet_publishrear) |
| [`CIS_ENVIRONMENT`](../../03_COMMUNICATION/10_CAN_FD.md#message-cis-environment) | CIS→DOMAIN | [`CisNet_PublishEnvironment()`](20_API.md#api-cisnet_publishenvironment) |
| [`CIS_OCCUPANT`](../../03_COMMUNICATION/10_CAN_FD.md#message-cis-occupant) | CIS→DOMAIN | [`CisNet_PublishOccupant()`](20_API.md#api-cisnet_publishoccupant) |
| [`CIS_STATUS`](../../03_COMMUNICATION/10_CAN_FD.md#message-cis-status) | CIS→DOMAIN | [`CisNet_PublishStatus()`](20_API.md#api-cisnet_publishstatus) |
| [`P_VISION_RESULT`](../../03_COMMUNICATION/30_UART_C.md#message-p-vision-result) | PI→CIS | [`CisApp_OnPiVisionResult()`](20_API.md#api-cisapp_onpivisionresult) |
| [`P_VISION_STATUS`](../../03_COMMUNICATION/30_UART_C.md#message-p-vision-status) | PI→CIS | [`CisApp_OnPiVisionStatus()`](20_API.md#api-cisapp_onpivisionstatus) |
| [`P_PERMISSION`](../../03_COMMUNICATION/30_UART_C.md#message-p-permission) | CIS→PI | [`CisNetPriv_RelayPiPermission()`](20_API.md#api-cisnetpriv_relaypipermission) · NETWORK_PRIVATE |

## 소유권과 현재 결정

- 온도/습도/조도/후방의 원 품질·Age·새 측정 순번은 CIS 센서 owner가 만든다. 환경 세 값은 독립 품질이다.
- Pi boot/vision_sequence/관측 Age·각 원 품질을 보존한다. CIS 기능 상태/추가 평가는 별도 원천이며 현재 CAN에 양측 품질 한 벌씩을 동시에 표현할 수 없다.
- [Pi 원 품질을 승격하지 않는 CIS 제한 평가](10_DATA.md#pi-원-관측과-cis-추가-평가)는 PROPOSED다. 사유 우선순위·상태 변환은 TEAM_DECISION_REQUIRED이며 미정 조합을 VALID로 승격하지 않는다.
- 전원 허용은 DOMAIN 원 판단이다. [CIS App 소비·private Pi relay](10_DATA.md#허용-입력과-pi-relay)를 나누며 공개 두 번째 허용 생성 API를 두지 않는다. 원 150ms·반복 500ms·use_limit 1500ms 후보의 관계와 consumer 무효화는 OPEN이다.

[팀 미결](../../06_OPEN_FOR_TEAM.md) · [48 메시지 색인](../../05_MESSAGE_INDEX.md). 실제 SDK/Firmware/Codec/Board/무선/카메라 통합 시험은 **NOT_EXECUTED**다.
