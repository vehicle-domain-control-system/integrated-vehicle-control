# PI — 노드 인터페이스 안내

Interface v2 · r1 · [규격과 연동 조건](../../07_CROSS_NODE_INTERFACE_RULES.md#status)

권장 프로필과 실제 연동 검증 전 조건을 구분한다. 각 API의 호출·품질·시간 조건을 적용한다.

[전체 시작](../../00_START_HERE.md) · [개요](00_OVERVIEW.md) · [Data](10_DATA.md) · [API](20_API.md)

Raspberry Pi의 카메라 서비스가 원 탑승자 판정·비전 기능/고장을 만든다. UART-C는 115200bps로 시작하는 기존 기준을 유지한다. Pi는 CIS가 중계한 DOMAIN Permission을 수신·해석하며, 이 값을 비전 기능 제한에 사용할지는 Pi 기능 모듈이 결정한다.

**PROPOSED_DESIGN · IMPLEMENTATION_BINDING_HOLD.** Data/API는 계약 후보이며 실제 firmware·보드 연결은 검증 전이다. 공용 CAN FD는 단일 공유 버스이고 nominal 500kbps/data 2Mbps는 WIRE_PROPOSED다. WINDOW는 **[구현 보류 — 설계 유지]**다.

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
- 현재 검토에 사용한 Pi 참고 구현은 Python이다. 인터페이스는 언어 중립 record와 호출 의미를 규정하며, C 원형과 같은 이름의 함수를 Python에 중복 설치하도록 요구하지 않는다.
- [P_PERMISSION 소비](10_DATA.md#허용-소비와-원본-age)는 원 DOMAIN 허용의 CIS relay를 제한해서 사용한다. 값1만으로 허용하지 않고 원 품질·누적 Age·use_limit·수신 중단/세대 변경을 확인한다. 후방 활성 원 사용 한도 500ms를 적용한다. 같은 Power 원본의 중계라는 근거가 있으면 그 기한을 적용하고, 전달된 `use_limit_ms`가 더 짧으면 그 한도도 지킨다. 1500ms 수신 중단은 1500ms 동안 ALLOW를 사용해도 된다는 뜻이 아니며, 500ms 반복만으로 연속 허용을 보증하지 않는다.
- 실제 BOOT 생성·수용, 품질 변환, Permission 수신·만료 처리는 양단 연결 검증이 필요하다. `USE_LIMIT=1500ms` 기존 후보와 `500ms` 조건부 변경안을 구분한다. 비전 기능 적용을 선택하지 않아도 통신 시험은 진행할 수 있다. Pi↔DOMAIN 직결/원격 Query는 없다.

[팀 미결](../../06_TECHNICAL_CONSTRAINTS.md) · [48 메시지 색인](../../05_MESSAGE_INDEX.md). 실제 SDK/Firmware/Codec/Board/무선/카메라 통합 시험은 **NOT_EXECUTED**다.

**Generation 수용 조건:** 생산자와 runtime의 실제 기동/관측 세대 연결은 BINDING_OPEN이다. runtime reset 후 이전 record를 다시 제출하여 새 BOOT를 얻는 방식은 금지하며 새 판정/상태 평가가 필요하다. 이는 신규 wire 필드·별도 App DTO를 추가하는 계약이 아니다.
