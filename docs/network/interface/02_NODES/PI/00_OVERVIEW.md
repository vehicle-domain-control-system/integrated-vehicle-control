# PI — 노드 인터페이스 안내

[전체 시작](../../00_START_HERE.md) · [개요](00_OVERVIEW.md) · [Data](10_DATA.md) · [API](20_API.md)

Raspberry Pi의 카메라 서비스가 원 탑승자 판정·비전 기능/고장을 만든다. UART-C 115200bps 시작으로 CIS에 제공하고 CIS가 전달한 DOMAIN 원 전원 허용을 소비한다.

**공유 초안: PROPOSED_DESIGN · IMPLEMENTATION_BINDING_HOLD.** 아래 Data/API는 구체 계약 후보이며 실제 firmware 설치/보드 통합 승인이 아니다. 공용 CAN FD는 단일 공유 버스이고 nominal 500kbps/data 2Mbps는 WIRE_PROPOSED다. WINDOW는 **[구현 보류 — 설계 유지]**다.

## 구현자가 먼저 볼 문서

| 문서 | 바로 확인할 내용 |
|---|---|
| [10_DATA](10_DATA.md) | 전체 struct/alias/enum과 멤버별 타입·범위·품질·원 소유자 |
| [20_API](20_API.md) | 송신/수신 정확한 반환형·함수명·인자·호출·실패·비동기 수명 |
| [공통 Data/ID](../../04_COMMON/10_DATA_AND_ID_RULES.md) · [호출·실패](../../04_COMMON/20_API_USAGE_AND_FAILURE.md) | BOOT/TX_SEQUENCE와 원 관측 순번/Age/command/result 구분 |
| [UART-C](../../03_COMMUNICATION/30_UART_C.md) · [CIS→DOMAIN CAN 경로](../CIS/00_OVERVIEW.md) | 실제 메시지 후보 byte/DLC/시간·프레이밍·CRC OPEN |

## 이 노드의 송수신 경계

| 메시지 | 방향·상대 | 로컬 함수 / 정확한 상세 |
|---|---|---|
| [`P_VISION_RESULT`](../../03_COMMUNICATION/30_UART_C.md#message-p-vision-result) | PI→CIS | [`PiNet_PublishVisionResult()`](20_API.md#api-pinet_publishvisionresult) |
| [`P_VISION_STATUS`](../../03_COMMUNICATION/30_UART_C.md#message-p-vision-status) | PI→CIS | [`PiNet_PublishVisionStatus()`](20_API.md#api-pinet_publishvisionstatus) |
| [`P_PERMISSION`](../../03_COMMUNICATION/30_UART_C.md#message-p-permission) | CIS→PI | [`PiApp_OnCisPermission()`](20_API.md#api-piapp_oncispermission) |

## 소유권과 현재 결정

- 관측값·판정 품질·VISION_SEQUENCE는 Pi 비전 owner, Pi Boot는 runtime 단일 owner, UART LINK_SEQUENCE는 Network 전송 owner다.
- 실제 구현 언어는 미정이다. Data는 언어 중립 record이며 API 페이지는 언어 중립 호출과 동일 계약의 정확한 C 바인딩 후보를 병기한다.
- [P_PERMISSION 소비](10_DATA.md#허용-소비와-원본-age)는 원 DOMAIN 허용의 CIS relay를 제한해서 사용한다. 값1만으로 허용하지 않고 원 품질·누적 Age·use_limit·수신 중단/세대 변경을 확인한다. 원 150ms 후보를 1500ms use_limit 후보로 늘리지 않으며 500ms 반복만으로 지속 허용을 보증하지 않는다.
- process 재시작·node boot 연결, Pi/CIS 상태 mapping, 허용 소비자 무효화 연결·1500ms 후보는 OPEN이다. Pi↔DOMAIN 직결/원격 Query는 없다.

[팀 미결](../../06_OPEN_FOR_TEAM.md) · [48 메시지 색인](../../05_MESSAGE_INDEX.md). 실제 SDK/Firmware/Codec/Board/무선/카메라 통합 시험은 **NOT_EXECUTED**다.

**Generation 수용 조건:** 생산자와 runtime의 실제 기동/관측 세대 연결은 BINDING_OPEN이다. runtime reset 후 이전 record를 다시 제출하여 새 BOOT를 얻는 방식은 금지하며 새 판정/상태 평가가 필요하다. 이는 신규 wire 필드·별도 App DTO를 추가하는 계약이 아니다.
