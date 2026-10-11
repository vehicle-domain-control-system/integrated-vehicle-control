# 네트워크 인터페이스 읽기 안내

**팀 검토·구현 협의용 초안 / DRAFT_SHAREABLE.** Application은 의미·판정·실제 동작 결과를 만들고, Network는 전송·검증·중계를 맡는다. 기존 C2/Gateway는 제공된 선언이며 실제 호출·SDK 바인딩은 확인되지 않았다.

<a id="section-reading-order"></a>

1. [전체 연결·담당](01_NETWORK_OVERVIEW.md)을 확인한다.
2. 아래에서 자신의 노드를 열고 Data와 API를 함께 읽는다.
3. [메시지48 색인](05_MESSAGE_INDEX.md)에서 상대 함수와 Wire 후보를 찾는다.
4. 구현에 필요한 [팀 결정 항목](06_OPEN_FOR_TEAM.md)을 먼저 확인한다.

<a id="section-nodes"></a>
## 노드별 문서

| 노드 | 개요 | 전체 Data | 송신·수신 API |
|---|---|---|---|
| DOMAIN · S32K344 | [담당](02_NODES/DOMAIN/00_OVERVIEW.md) | [Data](02_NODES/DOMAIN/10_DATA.md) | [API](02_NODES/DOMAIN/20_API.md) |
| ESP32 · BLE Gateway | [담당](02_NODES/ESP32/00_OVERVIEW.md) | [Data](02_NODES/ESP32/10_DATA.md) | [API](02_NODES/ESP32/20_API.md) |
| BCM · S32K144 | [담당](02_NODES/BCM/00_OVERVIEW.md) | [Data](02_NODES/BCM/10_DATA.md) | [API](02_NODES/BCM/20_API.md) |
| CIS · Sensor ECU | [담당](02_NODES/CIS/00_OVERVIEW.md) | [Data](02_NODES/CIS/10_DATA.md) | [API](02_NODES/CIS/20_API.md) |
| PI · Camera | [담당](02_NODES/PI/00_OVERVIEW.md) | [Data](02_NODES/PI/10_DATA.md) | [API](02_NODES/PI/20_API.md) |
| VSS · 별도 S32K344 | [담당](02_NODES/VSS/00_OVERVIEW.md) | [Data](02_NODES/VSS/10_DATA.md) | [API](02_NODES/VSS/20_API.md) |
| WINDOW · 구현 보류 | [담당](02_NODES/WINDOW/00_OVERVIEW.md) | [Data](02_NODES/WINDOW/10_DATA.md) | [API](02_NODES/WINDOW/20_API.md) |

<a id="section-common"></a>
## 통신과 공통 계약

| 통신·공통 계약 | 정본 |
|---|---|
| 단일 공유 CAN FD ·22종 | [설정·전체 Wire](03_COMMUNICATION/10_CAN_FD.md) |
| ESP32↔DOMAIN UART-M v2 ·23종 | [설정·전체 Wire](03_COMMUNICATION/20_UART_M.md) |
| PI↔CIS UART-C v1 ·3종 | [설정·전체 Wire](03_COMMUNICATION/30_UART_C.md) |
| MOBILE↔ESP32 BLE | [경계·미정](03_COMMUNICATION/40_BLE.md) |
| 공용 타입·식별·유효성 | [Data 규칙](04_COMMON/10_DATA_AND_ID_RULES.md) |
| 호출·수명·local 반환 | [API 규칙](04_COMMON/20_API_USAGE_AND_FAILURE.md) |
| C enum와 Wire raw의 명시 변환 | [Enum 변환표](04_COMMON/30_ENUM_WIRE_MAPPING.md) |

<a id="section-status"></a>
## 선언·제안·구현 상태

`DECLARED_SOURCE`는 원 헤더 선언, `PROPOSED_DESIGN`은 구체 설계 후보, `WIRE_PROPOSED`는 전송 형식 후보다. `BINDING_OPEN`은 코드·Port 연결 미확정, `TEAM_DECISION_REQUIRED`는 양단/기능 담당의 합의가 필요함을 뜻한다. WINDOW는 **[구현 보류 — 설계 유지]**다. SDK/Firmware/실 Codec/Board/무선 테스트는 **NOT_EXECUTED**다.

<a id="section-task-routes"></a>
## 작업별 출발점

API 상세는 정확한 반환형·함수명·전체 인자 선언부터 읽는다. Data 상세는 구조체 전문 다음의 멤버 표에서 타입·원 소유자·단위·유효 조건을 확인한다. 아래 경로는 노드 내부 호출과 노드 사이 Wire 전송을 함께 찾기 위한 색인이다.

| 작업 | 호출·Data 시작점 | 후속 처리·제한 |
|---|---|---|
| DOMAIN 도어 명령과 BCM 결과 | [C2 송신 선언](02_NODES/DOMAIN/20_API.md#api-domainif_txbcmdoorcommand) · [BCM 명령 Data](02_NODES/BCM/10_DATA.md#type-netbcmdoorcommand) | [BCM 수신 App](02_NODES/BCM/20_API.md#api-bcmapp_ondomaindoorcommand) · [실행 결과/confirmation](03_COMMUNICATION/10_CAN_FD.md#message-bcm-result) |
| MOBILE Query와 종료 | [Gateway 송신](02_NODES/ESP32/20_API.md#api-gateway_interface_sendtodomain) · [Query Data](02_NODES/DOMAIN/14_MOBILE_CODEC_DATA.md#type-netmobilequery) | [DOMAIN Query 생산](02_NODES/DOMAIN/25_PRIVATE_ROUTING.md#r2-query-producer) · [ESP32 private 종결](02_NODES/ESP32/25_PRIVATE_ROUTING.md#r2-query-consumer) |
| Pi 인원·품질과 역방향 허용 | [원 비전 Data](02_NODES/PI/10_DATA.md#type-netvisionresult) · [Pi 송신](02_NODES/PI/20_API.md#api-pinet_publishvisionresult) | [CIS 취합 Data](02_NODES/CIS/10_DATA.md#type-netcisoccupant) · [Pi 허용 소비](02_NODES/PI/20_API.md#api-piapp_oncispermission) |
| VSS 경고 수신과 상태 보고 | [경고 수신](02_NODES/VSS/20_API.md#api-vssapp_ondomainwarningstate) · [음향 상태 Data](02_NODES/VSS/10_DATA.md#type-netvssstatus) | [보호된 원 Snapshot 변환 후보](02_NODES/VSS/10_DATA.md#r2-status-proposal) · [상태 제출](02_NODES/VSS/20_API.md#api-vssnet_publishstatus) |
| WINDOW 설계 및 공통 의미 | [WINDOW 개요](02_NODES/WINDOW/00_OVERVIEW.md) · [원 AGE 규칙](04_COMMON/10_DATA_AND_ID_RULES.md#section-quality-freshness) | [메시지별 상대 API](05_MESSAGE_INDEX.md) · [팀 확인·사용 제한](06_OPEN_FOR_TEAM.md) |

Gateway subtype convention, Query TX 완료·BLE 전달, CAN 원 출처와 C2 Snapshot 연결, Permission 기한·품질, VSS 원 상태 변환은 PROPOSED/BINDING_OPEN이다. 노드 문서의 사용 조건과 관련 OPEN을 함께 읽고 구현 합의 범위를 확인한다.
