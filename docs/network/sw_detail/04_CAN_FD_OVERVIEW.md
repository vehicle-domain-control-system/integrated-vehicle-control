# CAN FD 전체 상세 — 22개 Wire와 25개 수신 경계

CAN은 DOMAIN·BCM·CIS·VSS·WINDOW가 공유하는 물리 버스다. Network는 의미 자료를 보존해 송수신하고 원 기능의 승인·측정·실행을 대신하지 않는다. 메시지 22개 중 DOMAIN_PERMISSION은 소비 노드가 넷이므로 수신 의무는 25개다.

[Network](01_NETWORK_OVERVIEW.md) → [노드·링크](02_NODE_LINK_MAP.md) → [Layer/Module](03_LAYER_MODULE_INDEX.md) → 아래 함수 → Data/API → [공통 흐름](flows/CAN_NETWORK_PATHS.md) 순서로 읽는다. 타입은 공통·BCM·CIS/WINDOW/VSS Data 정본과 기존 후방 정본에 나누고 중복 정의하지 않는다.

## 공통 처리의 위치

| 처리 | 정본 |
|---|---|
| 표준 ID, 실제 flags/DLC/길이, padding, 명시 LE와 enum 변환 | [공통 Codec](functions/CAN_COMMON_CODEC.md) |
| typed 안정 접수, latest 상태와 개별 사건, dispatch/AGE | [전송 제어](functions/CAN_TRANSFER_CONTROL.md#admission) |
| BOOT/TX와 원 관측, 중복/순서 수용, 원자적 저장 | [수신 제어](functions/CAN_TRANSFER_CONTROL.md#rx-order) |
| Driver accepted/최종 결과/release·후발 실패·복구 | [Driver 제어](functions/CAN_TRANSFER_CONTROL.md#port) |
| 기존 공통 enum/식별/실패·Health 자료와 내부 제안 자료 | [공통 Data](data/CAN_COMMON_DATA.md) |

<a id="messages"></a>
## 메시지별 양단 경계

아래 수치는 B07/T33의 후보 배치를 보존한다. 메시지 이름/방향의 문서상 고정과 실제 양단 Wire revision 채택은 구분한다. `BLOCKED_BY_CONTRACT`도 정상/거부 경로를 설명한 상세설계이며 실행 허가가 아니다. 모든 profile/Driver/Generation 연결은 채택·바인딩 증거를 요구한다. WINDOW 5 Wire와 공유 Permission의 WINDOW 수신 하나는 실행 HOLD이고, VSS_WARNING_STATE의 CF02는 별도 HOLD다.

| 메시지·함수 | 방향 | 후보 ID | used/frame/DLC | Data | 흐름 |
|---|---|---|---|---|---|
| [DOMAIN_PERMISSION](functions/can/DOMAIN_PERMISSION.md) | DOMAIN→BCM, CIS, WINDOW, VSS | 0x080 | 19/20/11 | [CAN_BCM_DATA](data/CAN_BCM_DATA.md) | [양단](flows/CAN_BCM_AND_PERMISSION.md) |
| [BCM_DOOR_COMMAND](functions/can/BCM_DOOR_COMMAND.md) | DOMAIN→BCM | 0x100 | 22/24/12 | [CAN_BCM_DATA](data/CAN_BCM_DATA.md) | [양단](flows/CAN_BCM_AND_PERMISSION.md) |
| [BCM_CLIMATE_COMMAND](functions/can/BCM_CLIMATE_COMMAND.md) | DOMAIN→BCM | 0x110 | 23/24/12 | [CAN_BCM_DATA](data/CAN_BCM_DATA.md) | [양단](flows/CAN_BCM_AND_PERMISSION.md) |
| [BCM_LIGHT_COMMAND](functions/can/BCM_LIGHT_COMMAND.md) | DOMAIN→BCM | 0x120 | 25/32/13 | [CAN_BCM_DATA](data/CAN_BCM_DATA.md) | [양단](flows/CAN_BCM_AND_PERMISSION.md) |
| [WINDOW_COMMAND](functions/can/WINDOW_COMMAND.md) | DOMAIN→WINDOW | 0x090 | 24/24/12 | [CAN_NODE_DATA](data/CAN_NODE_DATA.md) | [양단](flows/CAN_CIS_WINDOW_VSS.md) |
| [BCM_DOOR_STATE](functions/can/BCM_DOOR_STATE.md) | BCM→DOMAIN | 0x200 | 20/20/11 | [CAN_BCM_DATA](data/CAN_BCM_DATA.md) | [양단](flows/CAN_BCM_AND_PERMISSION.md) |
| [BCM_CLIMATE_STATE](functions/can/BCM_CLIMATE_STATE.md) | BCM→DOMAIN | 0x210 | 24/24/12 | [CAN_BCM_DATA](data/CAN_BCM_DATA.md) | [양단](flows/CAN_BCM_AND_PERMISSION.md) |
| [BCM_LIGHT_STATE](functions/can/BCM_LIGHT_STATE.md) | BCM→DOMAIN | 0x220 | 25/32/13 | [CAN_BCM_DATA](data/CAN_BCM_DATA.md) | [양단](flows/CAN_BCM_AND_PERMISSION.md) |
| [BCM_RESULT](functions/can/BCM_RESULT.md) | BCM→DOMAIN | 0x140 | 29/32/13 | [CAN_BCM_DATA](data/CAN_BCM_DATA.md) | [양단](flows/CAN_BCM_AND_PERMISSION.md) |
| [BCM_EVENT](functions/can/BCM_EVENT.md) | BCM→DOMAIN | 0x141 | 25/32/13 | [CAN_BCM_DATA](data/CAN_BCM_DATA.md) | [양단](flows/CAN_BCM_AND_PERMISSION.md) |
| [BCM_STATUS](functions/can/BCM_STATUS.md) | BCM→DOMAIN | 0x230 | 22/24/12 | [CAN_BCM_DATA](data/CAN_BCM_DATA.md) | [양단](flows/CAN_BCM_AND_PERMISSION.md) |
| [CIS_ENVIRONMENT](functions/can/CIS_ENVIRONMENT.md) | CIS→DOMAIN | 0x300 | 40/48/14 | [CAN_NODE_DATA](data/CAN_NODE_DATA.md) | [양단](flows/CAN_CIS_WINDOW_VSS.md) |
| [CIS_OCCUPANT](functions/can/CIS_OCCUPANT.md) | CIS→DOMAIN | 0x310 | 25/32/13 | [CAN_NODE_DATA](data/CAN_NODE_DATA.md) | [양단](flows/CAN_CIS_WINDOW_VSS.md) |
| [CIS_REAR](functions/can/CIS_REAR.md) | CIS→DOMAIN | 0x0A0 | 19/20/11 | [CIS_REAR_DATA](data/CIS_REAR_DATA.md) | [양단](flows/CAN_CIS_WINDOW_VSS.md) |
| [CIS_STATUS](functions/can/CIS_STATUS.md) | CIS→DOMAIN | 0x320 | 34/48/14 | [CAN_NODE_DATA](data/CAN_NODE_DATA.md) | [양단](flows/CAN_CIS_WINDOW_VSS.md) |
| [WINDOW_STATE](functions/can/WINDOW_STATE.md) | WINDOW→DOMAIN | 0x0B0 | 22/24/12 | [CAN_NODE_DATA](data/CAN_NODE_DATA.md) | [양단](flows/CAN_CIS_WINDOW_VSS.md) |
| [WINDOW_EVENT](functions/can/WINDOW_EVENT.md) | WINDOW→DOMAIN | 0x091 | 26/32/13 | [CAN_NODE_DATA](data/CAN_NODE_DATA.md) | [양단](flows/CAN_CIS_WINDOW_VSS.md) |
| [WINDOW_RESULT](functions/can/WINDOW_RESULT.md) | WINDOW→DOMAIN | 0x092 | 20/20/11 | [CAN_NODE_DATA](data/CAN_NODE_DATA.md) | [양단](flows/CAN_CIS_WINDOW_VSS.md) |
| [WINDOW_FAULT](functions/can/WINDOW_FAULT.md) | WINDOW→DOMAIN | 0x330 | 22/24/12 | [CAN_NODE_DATA](data/CAN_NODE_DATA.md) | [양단](flows/CAN_CIS_WINDOW_VSS.md) |
| [VSS_EVENT](functions/can/VSS_EVENT.md) | DOMAIN→VSS | 0x160 | 23/24/12 | [CAN_NODE_DATA](data/CAN_NODE_DATA.md) | [양단](flows/CAN_CIS_WINDOW_VSS.md) |
| [VSS_WARNING_STATE](functions/can/VSS_WARNING_STATE.md) | DOMAIN→VSS | 0x0C0 | 60/64/15 | [CAN_NODE_DATA](data/CAN_NODE_DATA.md) | [양단](flows/CAN_CIS_WINDOW_VSS.md) |
| [VSS_STATUS](functions/can/VSS_STATUS.md) | VSS→DOMAIN | 0x340 | 28/32/13 | [CAN_NODE_DATA](data/CAN_NODE_DATA.md) | [양단](flows/CAN_CIS_WINDOW_VSS.md) |

VSS_WARNING_STATE는 A1 WARNING60(used60/frame64/DLC15) 단일 설계다. WARNING51(used51/frame64/DLC15)은 이력이며 B51..59·길이·DLC로 profile을 추측하거나 구형을 수용하지 않는다. format0은 B50=0/B52..59=0 정규 부재이며 실제 양단 producer/지원·시간 검증은 CF02 HOLD다. 원 선언 없는 활성 근거·Generation 필드는 추가하지 않는다.

## 공유 Permission의 네 수신 의무

BCM과 VSS의 Network private 감시는 해당 ECU의 DOMAIN 통신 관측이다. CIS에는 기능 power permission 입력과 Pi relay 경계가 있으며 동일 수신 입력의 분기다. 둘을 합의 없이 순차 호출로 고정하지 않는다. WINDOW의 수신 명세는 보존하되 실행/timeout/liveness는 비활성이다. DOMAIN에서 한 번 CAN 전송한 사실이나 한 소비자의 정상 수신으로 나머지 세 소비자의 수신을 증명하지 않는다.

전원 허용·동작 허용·alive는 세 독립 typed 입력이다. 하나의 Wire로 합성하는 필수 입력, 평가 시점, 공통 SOURCE_AGE, reason/relay/USE_LIMIT은 합의 전 HOLD다. 하나의 최신 입력이 나머지를 새 판단으로 만들지 않는다.

## 완료와 미결의 경계

후방 대표 흐름은 기존 [CIS_REAR 수직 명세](flows/CIS_REAR_144_TO_344.md)를 그대로 사용한다. 다른 CAN 메시지는 각 자료/함수/흐름에서 정상 경로의 전제와 거부·후발 실패를 설명한다. CAN FD ACK, 로컬 OK, 최종 전송 성공, 지정 ECU의 수신, 차량 상태·액추에이터 완료는 서로 다른 사실이다.

Query/M02와 UART/M06은 이 상세의 대상이 아니다. 소유권·미결을 확정하지 않으며 Interface 협의 변경은 S6 완료 뒤 통합할 별도 대장에만 남긴다. 기존 [Interface](../interface/00_START_HERE.md)는 그대로 유지한다.
