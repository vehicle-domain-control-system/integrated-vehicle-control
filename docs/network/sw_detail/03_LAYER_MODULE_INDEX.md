# Layer·Module·Function 탐색

다섯 Layer와 아홉 Module의 기존 책임 분해를 유지한다. CAN22/Rx25·UART-M23/Rx23·UART-C3/Rx3, DOMAIN Adapter·공통 Service, 노드 경계를 통합해 연결한다. CIS_REAR는 대표 수직 흐름이며 전체 범위를 대체하지 않는다. 기능별 탐색은 [종단 시나리오18](09_E2E_FAILURE_RECOVERY.md)을 따른다. 실제 실행 바인딩과 외부 HOLD는 각 정본의 조건을 따른다.

| Layer | Module | 현재 설명 범위 |
|---|---|---|
| [L01 노드 통신 문맥](layers/L01_NODE_CONTEXT.md) | [M01 원본·문맥 연결](modules/M01_SOURCE_CONTEXT.md); [M02 DOMAIN 조회 감독](modules/M02_QUERY_CONTROL.md) | 원본·Permission·Pi relay와 Query 생산/종료 |
| [L02 타입 기반 인계 경계](layers/L02_TYPED_HANDOFF.md) | [M03 노드 타입 인계](modules/M03_TYPED_HANDOFF.md) | C2 등록·TxPort·Rx·노드 typed 인계 |
| [L03 링크별 메시지 처리](layers/L03_MESSAGE_SERVICE.md) | [M04 링크별 메시지 서비스](modules/M04_MESSAGE_SERVICE.md) | CAN/UART-M/UART-C 접수·순서·AGE·수명 |
| [L04 프레임 검증과 변환](layers/L04_FRAME_CODEC.md) | [M05 CAN 필드 Codec](modules/M05_CAN_CODEC.md); [M06 UART-M/C Frame·Stream·Codec](modules/M06_UART_CODEC.md) | CAN22 및 UART-M23/UART-C3 Frame·Stream·Codec |
| [L05 전송 수명과 장치 경계](layers/L05_TRANSPORT_PORT.md) | [M07 전송 버퍼·결과 관측](modules/M07_BUFFER_OUTCOME.md) · [M08 장치 Port 경계](modules/M08_DEVICE_PORT.md) · [M09 Network 관측 자료](modules/M09_OBSERVATION.md) | 링크별 버퍼·Driver 결과·참조 해제·Health |

M02는 수신된 Query의 기존 상태 조회·Getter 순회·종결 관측을 맡는다. M06은 UART-M/C별 stream 조립·재동기화·CRC·필드 변환을 맡는다. UART-M23과 UART-C3/25필드의 독립 계약을 사용하며 기존9 Module/25 Function 책임을 늘리지 않는다.

<a id="기존25개-내부-function-책임과-b11-연결"></a>
## 25개 내부 Function 책임과 현재 정본

아래 표는 현재 전체 상세의 직접 연결이다. 후방 대표 경로와 공통·링크별 정본을 함께 제시하며, 외부 바인딩 미결은 문서 미작성과 구분한다.

F번호는 기존 내부 책임 후보 식별자다. 제품 함수 개수나 공개 선언 개수가 아니다. 상세 문서의 새 내부 원형은 `PROPOSED_PRIVATE_FUNCTION`이며 기존 공개 API와 구분한다. 하나의 실제 처리에서 여러 책임을 함께 설명할 수 있다.

| 후보 | Module | 역할 | 현재 정본과 대표 경로 |
|---|---|---|---|
| F01 | M01 | 원 수신과 C2 Snapshot 연결 | [후방 대표](functions/DOMAIN_CIS_REAR_INGRESS.md) · [원본 연결](functions/DOMAIN_QUERY_PROVENANCE.md) |
| F02 | M01 | 현재 문맥·허용 입력·기동 연결 | [문맥](modules/M01_SOURCE_CONTEXT.md) · [Permission](functions/can/DOMAIN_PERMISSION.md) · [Router](functions/UART_M_ROUTER.md) |
| F03 | M01 | CIS 원 Vision·Permission 중계 | [Pi→CIS](flows/UART_C_PI_TO_CIS.md) · [CIS→Pi](flows/UART_C_CIS_TO_PI_PERMISSION.md) |
| F04 | M02 | DomainNetPriv_ProcessQuery | [Query 생산](functions/UART_M_QUERY_CONTROL.md) · [원본 연결](functions/DOMAIN_QUERY_PROVENANCE.md) |
| F05 | M02 | Query 송신 관측·종결 인계 | [Query 종료](functions/UART_M_QUERY_CONTROL.md#terminal) |
| F06 | M03 | C2 Port 등록 요청·지원 경계 확인 | [등록·지원 경계](functions/DOMAIN_REGISTRATION.md) |
| F07 | M03 | DOMAIN TxPort 의미 입력 인계 | [TxPort](functions/DOMAIN_TX_PORT.md) |
| F08 | M03 | DOMAIN C2 수신 인계 | [Rx Adapter](functions/DOMAIN_RX_ADAPTER.md) · [후방 대표](functions/DOMAIN_CIS_REAR_INGRESS.md) |
| F09 | M03 | DOMAIN 후발 local failure 인계 | [후발 실패](functions/NETWORK_FAILURE_HEALTH.md) |
| F10 | M03 | BCM/CIS/VSS/Gateway/Pi typed 경계 | [노드별 경계](08_NODE_BOUNDARIES.md) · [CIS 입력](functions/CIS_REAR_TX_SUBMIT.md) |
| F11 | M04 | 의미 데이터 안정 접수 | [공통 Service](functions/NETWORK_SERVICE_LIFETIME.md) · [CAN](functions/CAN_TRANSFER_CONTROL.md) · [UART-M](functions/UART_M_TX_RX_CONTROL.md) · [UART-C](functions/UART_C_TX_RX_CONTROL.md) |
| F12 | M04 | 전송 선택·만료·재기동 정리 | [공통 Service](functions/NETWORK_SERVICE_LIFETIME.md) · [후방 선택](functions/CIS_REAR_TX_DISPATCH.md) |
| F13 | M04 | 유효 RX 순번·기동·중복 인계 | [CAN 수용](functions/CAN_TRANSFER_CONTROL.md#rx-order) · [UART-M](functions/UART_M_TX_RX_CONTROL.md) · [UART-C](functions/UART_C_TX_RX_CONTROL.md) |
| F14 | M05 | CAN 수신 필드 검증·역변환 | [CAN 공통](functions/CAN_COMMON_CODEC.md) · [CAN22](04_CAN_FD_OVERVIEW.md#messages) |
| F15 | M05 | CAN 송신 필드 직렬화 | [CAN 공통](functions/CAN_COMMON_CODEC.md) · [후방 대표](functions/CIS_REAR_CODEC.md) |
| F16 | M06 | UART stream 조립·재동기화 | [UART stream](functions/UART_FRAME_STREAM.md) · [UART-C](functions/UART_C_FRAME_ROUTER.md) |
| F17 | M06 | UART frame 검증·역변환 | [UART-M23](05_UART_M_OVERVIEW.md#messages) · [UART-C3](06_UART_C_OVERVIEW.md#messages) |
| F18 | M06 | UART 필드 직렬화 | [UART-M23](05_UART_M_OVERVIEW.md#messages) · [UART-C3](06_UART_C_OVERVIEW.md#messages) |
| F19 | M07 | 송신 프레임 안정 인수 | [공통 수명](functions/NETWORK_SERVICE_LIFETIME.md) · [후방 선택](functions/CIS_REAR_TX_DISPATCH.md) |
| F20 | M07 | 수신 저장소 소비 인계 | [공통 수명](functions/NETWORK_SERVICE_LIFETIME.md) · [후방 수용](functions/CIS_REAR_RX_ADMISSION.md) |
| F21 | M07 | 장치 접수·실제 TX 결과 관측 | [공통 결과 관측](functions/NETWORK_SERVICE_LIFETIME.md) · [CAN Port](functions/CAN_PORT_RECOVERY.md) |
| F22 | M08 | Driver/HAL 전송·수신 경계 | [장치 경계](modules/M08_DEVICE_PORT.md) · [공통 수명](functions/NETWORK_SERVICE_LIFETIME.md) |
| F23 | M08 | 로컬 기동·정지·복구 바인딩 경계 | [장치 복구](modules/M08_DEVICE_PORT.md) · [실패·Health](functions/NETWORK_FAILURE_HEALTH.md) |
| F24 | M09 | Network 원 관측 기록 | [원 관측](functions/NETWORK_FAILURE_HEALTH.md) · [CAN Port](functions/CAN_PORT_RECOVERY.md#observe-port) |
| F25 | M09 | Network raw Health 읽기 연결 | [raw Health](functions/NETWORK_FAILURE_HEALTH.md) |

[시작](00_START_HERE.md) · [노드별 위치](02_NODE_LINK_MAP.md) · [전체 수직 흐름](flows/CIS_REAR_144_TO_344.md)

## CAN 확장 연결

| 책임 | 현재 정본 | 미완료/제약 |
|---|---|---|
| F01/F02/F03 | [원 출처·Permission](functions/can/DOMAIN_PERMISSION.md), [CIS 노드](flows/CAN_CIS_WINDOW_VSS.md) | Generation/Boot/합성/relay 바인딩 미결 |
| F06/F07/F08/F09/F10 | [등록/접수/후발 실패](functions/CAN_TRANSFER_CONTROL.md), [22종 실제 API](04_CAN_FD_OVERVIEW.md#messages) | 실제 지원표·반환·sink 연결 미결 |
| F11/F12/F13 | [접수/AGE/순서](functions/CAN_TRANSFER_CONTROL.md) | 실제 capacity/clock/동기화·wrap 미결 |
| F14/F15 | [공통 Codec](functions/CAN_COMMON_CODEC.md), 메시지별 Encode/Decode | 양단 profile 후보 채택 필요 |
| F19/F20/F21/F22/F23 | [저장소/Driver/복구](functions/CAN_TRANSFER_CONTROL.md#port) | 실제 SDK·참조/callback 바인딩 미결 |
| F24/F25 | [관측/Health](functions/CAN_TRANSFER_CONTROL.md#recovery-health) | 공개 raw 입력 연결·동기화 미결 |
| F04/F05/F16/F17/F18 | CAN 확장 자체에는 미포함; 현재 UART-M 연결은 아래 표 | 실제 바인딩은 외부 HOLD |

## UART-M 확장 연결

| 기존 책임 | 현재 상세 정본 | 남은 경계 |
|---|---|---|
| F01/F02 — M01 | [원 CAN association](flows/UART_M_CAN_PROJECTION.md), [Router](functions/UART_M_ROUTER.md) | 현재 peer/Session·원 Boot/Generation·C2 commit 미결 |
| F04/F05 — M02 | [Query 생산·종료](functions/UART_M_QUERY_CONTROL.md) |18 Getter/17 schema, 성공 OUT·scope·final TX·End 실제 바인딩 |
| F06/F07/F08/F09/F10 — M03 | [23 메시지 API](05_UART_M_OVERVIEW.md#messages), [Router](functions/UART_M_ROUTER.md), [반환·수명](functions/UART_M_TX_RX_CONTROL.md) | 공개 본체 Owner 유지·협의 변경 기록만 |
| F11/F12/F13 — M04 | [TX/RX·AGE·순서](functions/UART_M_TX_RX_CONTROL.md), [Router](functions/UART_M_ROUTER.md) | 안정 사본·원 identity·wrap/시간·현재 문맥 근거 |
| F16/F17/F18 — M06 | [Frame/Stream](functions/UART_FRAME_STREAM.md), [23 Codec](05_UART_M_OVERVIEW.md#messages) | VERSION2 양단 채택·Driver byte시각·timer 바인딩 HOLD |
| F19/F20/F21/F22/F23 — M07/M08 | [제출·참조·outcome](functions/UART_M_TX_RX_CONTROL.md), [Stream 입력](functions/UART_FRAME_STREAM.md#stream) | 실제 Driver/ISR/Task·이전 callback 세대 근거 |
| F24/F25 — M09 | [관측·Health](functions/UART_M_TX_RX_CONTROL.md), [M09](modules/M09_OBSERVATION.md) | ST12 raw 통신 관측·성공 OUT; 실제 공개 Health 공급 바인딩 미검증 |

위 표는 기존5 Layer/9 Module/25 Function과 ST01~ST12 책임의 확장 연결이다. 별도 Task·Queue·공개 API·새 State ID를 만들지 않는다. [모듈 간 탐색](modules/00_MODULE_INDEX.md)과 [UART-M 읽기 순서](05_UART_M_OVERVIEW.md)를 함께 따른다.

## UART-C 확장 연결

| 기존 책임 | UART-C 상세 | 원 상태·제약 |
|---|---|---|
| F01~03 / M01 | [Pi→CIS](flows/UART_C_PI_TO_CIS.md), [원 DOMAIN→CIS→Pi](flows/UART_C_CIS_TO_PI_PERMISSION.md) | ST01/02 동일 원본·품질/AGE·relay 계보 HOLD |
| F10 / M03 | [3종 기존 API](06_UART_C_OVERVIEW.md#messages), [Router](functions/UART_C_FRAME_ROUTER.md#dispatch) | 공개 5개/private 1개 원형·const borrow 보존 |
| F11~13 / M04 | [접수/시간/순서](functions/UART_C_TX_RX_CONTROL.md) | ST05~07 원 관측과 LINK_SEQUENCE 독립 |
| F16~18 / M06 | [Frame/Stream/Codec](functions/UART_C_FRAME_ROUTER.md) | ST08 VERSION 1·정확한 P·timestamp·양단 채택 HOLD |
| F19~23 / M07~08 | [제출/최종 결과/해제](functions/UART_C_TX_RX_CONTROL.md#port-observe) | ST09~11 실제 Driver 참조·세대 근거 |
| F24~25 / M09 | [관측/복구](functions/UART_C_TX_RX_CONTROL.md#recovery) | ST12 실제 통신 관측·제품 바인딩 OPEN |

## DOMAIN 경계·공통 Service 연결

기존 F01~F25와 ST01~ST12를 그대로 사용한다. 아래는 등록·송수신·후발 관측의 실제 연결을 찾는 표이며 제품 함수·공개 API·새 책임 ID를 추가하는 표가 아니다. 전체 읽기 순서는 [DOMAIN Adapter와 Service](07_DOMAIN_ADAPTER_SERVICE.md)에서 시작한다.

| 기존 책임 / Module | 연결할 상세 | 자료·실행 경계 |
|---|---|---|
| F01/F02 / M01 | [수신 Adapter](functions/DOMAIN_RX_ADAPTER.md), [Query·원본 연결](functions/DOMAIN_QUERY_PROVENANCE.md) | 원 DTO/meta와 C2 수용·Snapshot의 같은 관측 증거 |
| F04/F05 / M02 | [Query 재사용](functions/DOMAIN_QUERY_PROVENANCE.md), [기존 Query 정본](functions/UART_M_QUERY_CONTROL.md) | Getter18/schema17, 실패 OUT·최종TX·End 순서 |
| F06 / M03 | [등록·소비 준비](functions/DOMAIN_REGISTRATION.md) | 공개 본체와 등록 요청·Observer 소비 소유권 분리 |
| F07 / M03 | [TxPort26](functions/DOMAIN_TX_PORT.md), [명령 흐름](flows/DOMAIN_COMMAND_TO_NETWORK.md) | 전체 입력 사본, 정확한 반환과 원 identity |
| F08/F09 / M03 | [Rx22 경계](functions/DOMAIN_RX_ADAPTER.md), [후발 실패](functions/NETWORK_FAILURE_HEALTH.md), [수신 흐름](flows/NETWORK_TO_DOMAIN.md) | Core 반영과 Observer 통지 분리, 실제 실패의 원 귀속 |
| F11~F13 / M04 | [공통 Service 수명](functions/NETWORK_SERVICE_LIFETIME.md) | 대기·선택·순서·AGE와 접수 상태 |
| F14~F18 / M05/M06 | [CAN](04_CAN_FD_OVERVIEW.md), [UART-M](05_UART_M_OVERVIEW.md), [UART-C](06_UART_C_OVERVIEW.md) | 기존 Frame/Codec/필드 정본 재사용 |
| F19~F23 / M07/M08 | [공통 수명·장치 관측](functions/NETWORK_SERVICE_LIFETIME.md) | 실제 Driver 접수·terminal·release의 독립 관측 |
| F24/F25 / M09 | [로컬 실패·Health](functions/NETWORK_FAILURE_HEALTH.md) | 실제 Network 관측, DOMAIN 소유 Snapshot 경계 |

공통 자료의 선언·수명은 [Network Service Data](data/NETWORK_SERVICE_DATA.md)에 둔다. 기존 Node별 F03/F10 경계와 외부 HOLD는 보존하며 WINDOW나 VSS 미결 경로를 이 연결만으로 활성화하지 않는다.

## 노드별 경계 연결

기존 F01~F25 책임을 아래 노드 경계와 [노드 전체](08_NODE_BOUNDARIES.md)에서 구체화한다. 새 공개 API·Task·Queue·State ID를 추가하지 않는다.

| Node / Link | 기존 책임 | 노드 상세와 실제 경계 |
|---|---|---|
| N01 / I01·P01·P02·R01~R06의 DOMAIN 끝점 | M01/M02/M03, F01·F02·F04~F09 | [DOMAIN](nodes/DOMAIN.md): C2 본체와 Network callback·수신·Query 구분 |
| N02 / I02·P01·R01 | M03/M04/M05/M07~M09, F10~F15·F19~F25 | [BCM](nodes/BCM.md): 명령 인수/실행·원 결과 귀속 |
| N03/N06 / I03·I06·P01·P03·R02·R06 | M01/M03/M04/M05/M06/M07~M09, F03·F10~F25 | [CIS/Pi](nodes/CIS_PI.md): 원 품질·AGE·단위·Permission relay |
| N04 / I04·P01·R03 | M03/M04/M05/M07~M09, F10~F15·F19~F25 | [VSS](nodes/VSS.md): 실제 음향 Owner·CF02 HOLD |
| N05/N08 / I05·P02·P04·R05 | M01/M02/M03/M04/M06/M07~M09 경계 계약 | [ESP32/MOBILE](nodes/ESP32_MOBILE.md): ESP32 내부 구현 Owner 유지; MOBILE 내부 적용 없음 |
| N07 / I07·P01·R04 | CAN 관련 책임의 설계만 보존 | [WINDOW](nodes/WINDOW.md): 실행·표시·감시 HOLD |

<a id="b17-기능별-종단-연결"></a>
## 기능별 종단 연결

기존 Node8/Link17, L5/M9/F25/ST12를 유지하며 새 함수·Manager·Queue를 만들지 않는다. 각 문서에서 원 시나리오의 Data/API와 기존 함수 본체로 내려간다.

| 시나리오 | 기능·실패·복구 상세 | 주 책임 |
|---|---|---|
| SC01~04/09 | [BCM 요청·결과·진단](flows/e2e/BCM_REQUEST_RESULT.md) | M01/M03/M04/M05/M06/M07~09 |
| SC05~08/10 | [환경·Vision·Permission·Health](flows/e2e/SENSORS_VISION_PERMISSION.md) | M01/M03~M09 |
| SC11~14 | [WINDOW·VSS·고장](flows/e2e/WINDOW_VSS_HEALTH.md) | M01/M03~M09, WINDOW 실행 HOLD |
| SC15~16 | [Query·경고 읽음·Session](flows/e2e/QUERY_WARNING_SESSION.md) | M01/M02/M03/M04/M06/M07~09 |
| SC17~18 | [기동·수명·공통 오류](flows/e2e/COMMON_FAILURE_RECOVERY.md) | M01/M04/M07~09, 기존 Codec/Router 연계 |
