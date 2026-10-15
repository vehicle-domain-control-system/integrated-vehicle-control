# CIS — 노드 인터페이스 안내

Interface v2 · r1 · [규격과 연동 조건](../../07_CROSS_NODE_INTERFACE_RULES.md#status)

권장 프로필과 실제 연동 검증 전 조건을 구분한다. 각 API의 호출·품질·시간 조건을 적용한다.

[전체 시작](../../00_START_HERE.md) · [개요](00_OVERVIEW.md) · [Data](10_DATA.md) · [API](20_API.md)

센서 ECU에서 환경·후방 관측을 생성하고 Pi 카메라 결과를 취합하여 DOMAIN에 보고한다. Pi는 UART-C로 CIS에만 연결되며 DOMAIN과 직접 연결하지 않는다.

**PROPOSED_DESIGN · IMPLEMENTATION_BINDING_HOLD.** Data/API는 계약 후보이며 실제 firmware·보드 연결은 검증 전이다. 공용 CAN FD는 단일 공유 버스이고 nominal 500kbps/data 2Mbps는 WIRE_PROPOSED다. WINDOW는 **[구현 보류 — 설계 유지]**다.

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

Network는 CIS의 CAN/UART-C 통신 스택과 API를 제공하고, CIS 모듈은 센서·비전 취합·후방 활성 기능을 연결한다. [온습도 2500ms와 환경 무효화](10_DATA.md#환경-원본-최신성--o20)는 원 사용 기한과 실제 구현 연결을 구분하여 적용한다. Network는 센서 알고리즘을 중복 구현하지 않는다.

- 온도/습도/조도/후방의 원 품질·Age·새 측정 순번은 CIS 센서 owner가 만든다. 환경 세 값은 독립 품질이다.
- Pi boot/vision_sequence/관측 Age·각 원 품질을 보존한다. CIS 기능 상태/추가 평가는 별도 원천이며 현재 CAN에 양측 품질 한 벌씩을 동시에 표현할 수 없다.
- [Pi 원 품질과 CIS 추가 평가](10_DATA.md#pi-원-관측과-cis-추가-평가)는 권장 프로필로 구체화했다. 원 판정의 품질을 보존하면서 이후 실제 통신 단절·만료·기동 변경을 현재 사용 상태에 반영한다. 상세 상태 변환과 기능 코드 연결은 미검증이다.
- 전원 허용은 DOMAIN 원 판단이다. [CIS App 소비·private Pi relay](10_DATA.md#허용-입력과-pi-relay)를 나누며 공개 두 번째 허용 생성 API를 두지 않는다. 후방 활성 원 사용 한도 500ms와 DOMAIN 약 200ms 재평가를 적용한다. CAN 50ms 송신/150ms 수신 중단, UART-C 500ms 반복/1500ms 수신 중단은 유지한다. `USE_LIMIT=1500ms`는 기존 필드 값 후보이며 `500ms` 변경은 조건부 권장 프로필이다. 어느 값도 원 허용 기한을 연장하지 않는다. 실제 중계·소비자 연결은 확인이 필요하다.

[팀 미결](../../06_TECHNICAL_CONSTRAINTS.md) · [48 메시지 색인](../../05_MESSAGE_INDEX.md). 실제 SDK/Firmware/Codec/Board/무선/카메라 통합 시험은 **NOT_EXECUTED**다.
