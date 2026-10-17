# UART-M23 상세설계 안내

[Network](01_NETWORK_OVERVIEW.md) · [노드/링크](02_NODE_LINK_MAP.md) · [Layer/Module](03_LAYER_MODULE_INDEX.md) · [M06](modules/M06_UART_CODEC.md) · [M02](modules/M02_QUERY_CONTROL.md)

이 문서는 DOMAIN↔ESP32 UART-M23 Wire/23 Rx 문서 설계다. VERSION2 및 C2 v0.6/Vehicle v0.3은 미승인 후보이며, 문서 작성만으로 실제 실행·양단 채택 또는 Driver·보드 검증 완료를 주장할 수 없다. 전체 요구103·Owner35·Wire48/Rx51·시나리오18과 기존 외부 HOLD를 유지한다. 기존 Interface 기준 S6/G6 검수 후 현재 별도 후보에는 PR #68의 고정 r1 Interface40개 참조를 동기화했다. 남은 협의 차이와 실제 양단·제품 채택은 별도 근거·승인 대상으로 유지한다.

ESP32→DOMAIN 상향4, DOMAIN→ESP32 기능 보고/투영17, private ALIVE/QueryEnd2의 합이23이다. 하향17에는 DOMAIN 보고6과 원 CAN 투영11이 포함된다. Window state/fault2는 문서 추적에 포함하지만 실제 TX/RX·표시·timeout·liveness는 HOLD다. ESP32 Core/Transport·BLE/App 내부 구현은 해당 Owner 소유다.

## 읽기 순서와 처리 위치

1. [Frame Data](data/UART_FRAME_DATA.md)와 [M06](modules/M06_UART_CODEC.md)에서 사본 수명과 함수 계약을 확인한다.
2. [Frame/CRC/Stream](functions/UART_FRAME_STREAM.md)에서 exact Type/방향/P, 원 byte 수신시각, suffix 재탐색, CRC정상 경계 전체 소비를 확인한다.
3. 아래23 표의 개별 Function에서 실제 API 원형·Owner, Data 정본·필드 offset, 한국어 C형 encode/decode와 반례를 따라간다.
4. [Router](functions/UART_M_ROUTER.md)와 [TX/RX 제어](functions/UART_M_TX_RX_CONTROL.md)에서 현재 문맥·원 Boot/순서/AGE·안정 사본·실제 소비/Driver 결과를 연결한다.
5. [상향](flows/UART_M_UPLINK.md), [하향](flows/UART_M_DOWNLINK.md), [Query/종료](flows/UART_M_QUERY_AND_PROJECTION.md), [원 CAN 투영](flows/UART_M_CAN_PROJECTION.md)에서 양단 흐름을 확인한다.

일반 envelope는 A5 5A, VERSION1B, TYPE1B, P u16LE, LINK_SEQUENCE u16LE, payload P B, CRC u16LE이며 전체 P+10이다. payload B0은 frame B8이다. CRC는 B2부터 P+6바이트에 대한 CCITT-FALSE이고 TYPE별 exact P를 강제한다. 최대 P128은 논리 방어 후보이며 등록 최대는 P62/Frame72다. 실제 Queue/FIFO/DMA 크기는 정하지 않는다. 115200bps 초기 기준과 실제 Port·시간 바인딩은 실행 증거와 구분한다.

<a id="messages"></a>
## 23 메시지·함수·자료 연결

표의 P/F는 payload/전체 frame 길이다. 실제 Sender/Receiver 원형·const·인자·반환과 전체 필드표는 각 메시지 Function에 있다. 기존 C형 선언은 [UART-M Data](data/UART_M_DATA.md) 또는 연결된 CAN Data 정본을 재사용한다. 필드 표는 C선언을 중복하지 않는다.

| Wire / Rx | 메시지 상세 | 방향 | Type | P/F | 자료 | 현재 제한 |
|---|---|---|---|---|---|---|
| WIRE-P02-M_CONTEXT / RX-P02-M_CONTEXT-DOMAIN | [M_CONTEXT](functions/uart_m/M_CONTEXT.md) | ESP32→DOMAIN | 0x01 | 38/48 | `NetMobileContext` | 양단 문맥/표현·CF01 |
| WIRE-P02-M_REQUEST / RX-P02-M_REQUEST-DOMAIN | [M_REQUEST](functions/uart_m/M_REQUEST.md) | ESP32→DOMAIN | 0x10 | 24/34 | `NetMobileRequest` | 양단 문맥/표현·CF01 |
| WIRE-P02-M_QUERY / RX-P02-M_QUERY-DOMAIN | [M_QUERY](functions/uart_m/M_QUERY.md) | ESP32→DOMAIN | 0x11 | 32/42 | `NetMobileQuery` | 양단 문맥/표현·CF01 |
| WIRE-P02-M_WARNING_ACK / RX-P02-M_WARNING_ACK-DOMAIN | [M_WARNING_ACK](functions/uart_m/M_WARNING_ACK.md) | ESP32→DOMAIN | 0x12 | 21/31 | `NetMobileWarningAck` | 양단 문맥/표현·CF01 |
| WIRE-P02-M_DOMAIN_ALIVE / RX-P02-M_DOMAIN_ALIVE-ESP32 | [M_DOMAIN_ALIVE](functions/uart_m/M_DOMAIN_ALIVE.md) | DOMAIN→ESP32 | 0x02 | 12/22 | `NetMobileDomainAlive` | 양단 문맥/표현·CF01 |
| WIRE-P02-M_RESULT / RX-P02-M_RESULT-ESP32 | [M_RESULT](functions/uart_m/M_RESULT.md) | DOMAIN→ESP32 | 0x20 | 44/54 | `NetMobileResult` | 양단 문맥/표현·CF01 |
| WIRE-P02-M_WARNING / RX-P02-M_WARNING-ESP32 | [M_WARNING](functions/uart_m/M_WARNING.md) | DOMAIN→ESP32 | 0x21 | 40/50 | `NetMobileWarning` | 양단 문맥/표현·CF01 |
| WIRE-P02-M_AVAILABILITY / RX-P02-M_AVAILABILITY-ESP32 | [M_AVAILABILITY](functions/uart_m/M_AVAILABILITY.md) | DOMAIN→ESP32 | 0x22 | 62/72 | `NetMobileAvailability` | 양단 문맥/표현·CF01 |
| WIRE-P02-M_DIGITAL_STATUS / RX-P02-M_DIGITAL_STATUS-ESP32 | [M_DIGITAL_STATUS](functions/uart_m/M_DIGITAL_STATUS.md) | DOMAIN→ESP32 | 0x23 | 30/40 | `NetMobileDigitalStatus` | 양단 문맥/표현·CF01 |
| WIRE-P02-M_DIGITAL_RESULT / RX-P02-M_DIGITAL_RESULT-ESP32 | [M_DIGITAL_RESULT](functions/uart_m/M_DIGITAL_RESULT.md) | DOMAIN→ESP32 | 0x24 | 35/45 | `NetMobileDigitalResult` | 양단 문맥/표현·CF01 |
| WIRE-P02-M_USER_SETTINGS / RX-P02-M_USER_SETTINGS-ESP32 | [M_USER_SETTINGS](functions/uart_m/M_USER_SETTINGS.md) | DOMAIN→ESP32 | 0x25 | 36/46 | `NetMobileUserSettings` | 양단 문맥/표현·CF01 |
| WIRE-P02-M_QUERY_END / RX-P02-M_QUERY_END-ESP32 | [M_QUERY_END](functions/uart_m/M_QUERY_END.md) | DOMAIN→ESP32 | 0x40 | 24/34 | `NetMobileQueryEnd` | 양단 문맥/표현·CF01 |
| WIRE-P02-M_BCM_DOOR_STATE / RX-P02-M_BCM_DOOR_STATE-ESP32 | [M_BCM_DOOR_STATE](functions/uart_m/M_BCM_DOOR_STATE.md) | DOMAIN→ESP32 | 0x30 | 40/50 | `NetMobileBcmDoorState` | 원 CAN association·CF01 |
| WIRE-P02-M_BCM_CLIMATE_STATE / RX-P02-M_BCM_CLIMATE_STATE-ESP32 | [M_BCM_CLIMATE_STATE](functions/uart_m/M_BCM_CLIMATE_STATE.md) | DOMAIN→ESP32 | 0x31 | 44/54 | `NetMobileBcmClimateState` | 원 CAN association·CF01 |
| WIRE-P02-M_BCM_LIGHT_STATE / RX-P02-M_BCM_LIGHT_STATE-ESP32 | [M_BCM_LIGHT_STATE](functions/uart_m/M_BCM_LIGHT_STATE.md) | DOMAIN→ESP32 | 0x32 | 45/55 | `NetMobileBcmLightState` | 원 CAN association·CF01 |
| WIRE-P02-M_BCM_STATUS / RX-P02-M_BCM_STATUS-ESP32 | [M_BCM_STATUS](functions/uart_m/M_BCM_STATUS.md) | DOMAIN→ESP32 | 0x33 | 42/52 | `NetMobileBcmStatus` | 원 CAN association·CF01 |
| WIRE-P02-M_CIS_ENVIRONMENT / RX-P02-M_CIS_ENVIRONMENT-ESP32 | [M_CIS_ENVIRONMENT](functions/uart_m/M_CIS_ENVIRONMENT.md) | DOMAIN→ESP32 | 0x34 | 60/70 | `NetMobileCisEnvironment` | 원 CAN association·CF01 |
| WIRE-P02-M_CIS_OCCUPANT / RX-P02-M_CIS_OCCUPANT-ESP32 | [M_CIS_OCCUPANT](functions/uart_m/M_CIS_OCCUPANT.md) | DOMAIN→ESP32 | 0x35 | 45/55 | `NetMobileCisOccupant` | 원 CAN association·CF01 |
| WIRE-P02-M_CIS_REAR / RX-P02-M_CIS_REAR-ESP32 | [M_CIS_REAR](functions/uart_m/M_CIS_REAR.md) | DOMAIN→ESP32 | 0x36 | 39/49 | `NetMobileCisRear` | 원 CAN association·CF01 |
| WIRE-P02-M_CIS_STATUS / RX-P02-M_CIS_STATUS-ESP32 | [M_CIS_STATUS](functions/uart_m/M_CIS_STATUS.md) | DOMAIN→ESP32 | 0x37 | 54/64 | `NetMobileCisStatus` | 원 CAN association·CF01 |
| WIRE-P02-M_VSS_STATUS / RX-P02-M_VSS_STATUS-ESP32 | [M_VSS_STATUS](functions/uart_m/M_VSS_STATUS.md) | DOMAIN→ESP32 | 0x3A | 48/58 | `NetMobileVssStatus` | 원 CAN association·CF01 |
| WIRE-P02-M_WINDOW_STATE / RX-P02-M_WINDOW_STATE-ESP32 | [M_WINDOW_STATE](functions/uart_m/M_WINDOW_STATE.md) | DOMAIN→ESP32 | 0x38 | 42/52 | `NetMobileWindowState` | 실행·표시·timeout·liveness HOLD |
| WIRE-P02-M_WINDOW_FAULT / RX-P02-M_WINDOW_FAULT-ESP32 | [M_WINDOW_FAULT](functions/uart_m/M_WINDOW_FAULT.md) | DOMAIN→ESP32 | 0x39 | 42/52 | `NetMobileWindowFault` | 실행·표시·timeout·liveness HOLD |

## 원본·조회·실패의 불변 조건

DOWN_CONTEXT20B는 현재 Device/Session·DOMAIN Boot·QueryID다. 뒤의 원 CAN used 필드에는 별도 원 CAN Boot/TX·Pi Boot/VisionSequence·각 원 AGE가 남는다. CAN padding을 UART payload에 넣지 않으며 Snapshot 값=최신 raw라는 이유로 결합하지 않는다. 같은 입력의 실제 C2 commit/Getter/TxPort association 근거가 없으면 해당 projection을 미제공/부분으로 제한한다. Generation0·BOOT_ID·CIS Environment 정책은 회신 대기다.

18 Getter는17 응답 schema를 사용한다. 실패 OUT/HasRecord/Count는 읽지 않고, Query item count는 enqueue/accepted/release가 아니라 해당 Query에 속한 최초 실제 final TX를 센다. 결과 미확정이면 성공 End를 보류한다. ALIVE/End와 QUERY_ID0 자발 갱신·중복은 응답 count에 합산하지 않는다. Query의 최초 SESSION_READY0을 단독 거부 사유로 삼거나 passive 검증으로 Session을 발급하지 않는다.

AGE65535는 포화 하한이다. 수신/재포장/재시도 시0으로 만들지 않고 미반영 로컬 경과만 한 번 더한다. node 간 clock을 차감하지 않으며 미측정 Driver 대기를0으로 쓰지 않는다. source·문맥·순서·형식 거부를 차량 FAILED로 만들지 않는다. M09/F24/F25/ST12는 raw 관측만 보존하며 실제 Health 공급 바인딩을 확인하기 전 공개 Getter 완료를 주장하지 않는다.

실제 profile 채택, callback 수명·연결 세대, 원본 association, Session/Query/Scope, Environment 표현과 Gateway subtype의 미결은 기존 Owner35·CF01/CF02·외부 HOLD 아래 남긴다. 확인되지 않은 profile·수명·문맥을 추정해 해당 소비를 활성화하지 않는다.
