# BCM 요청·실행·결과·진단의 종단 간 흐름

[B17 전체](../../09_E2E_FAILURE_RECOVERY.md) · [공통 실패·복구](COMMON_FAILURE_RECOVERY.md) · [DOMAIN 경계](../../nodes/DOMAIN.md) · [BCM 경계](../../nodes/BCM.md) · [ESP32/MOBILE 경계](../../nodes/ESP32_MOBILE.md)

이 문서는 SC01~SC04와 SC09의 **조건부 정상 경로와 실패·복구 경로를 연결하는 B17 설계**다. 이 설계는 제품 실행·실제 채택의 증거가 아니다. 원 시나리오 제목·직접 Wire/Rx·Owner ID는 제출물 `B17_E2E_SCENARIO_18_COVERAGE.csv`의 canonical 열과 같고, 아래 보조 경로는 그 직접 집합에 더하지 않는다. G6 당시 Interface39 봉인본과 현재 검토 중인 Interface40개 후보의 출처·해시 기준 참조를 구분한다. Owner 미결35·외부 HOLD와 C2 v0.6/Vehicle v0.3 후보 지위를 유지한다.

## 다섯 흐름에 공통인 관측 단계

| 단계 | 실행 주체·정본 | 실제로 확인하는 것 / 확인할 수 없는 것 |
|---|---|---|
| 원 입력 | MOBILE 사용자·ESP32 관측 / [상향](../UART_M_UPLINK.md) | 현재 Device/Session의 원 Request 또는 등록·연결/App/근접 각 관측. BLE connected만으로 ACTIVE·NEAR를 만들지 않음 |
| DOMAIN 수용·판단 | L01/M01, L02/M03/F08 / [Rx Adapter](../../functions/DOMAIN_RX_ADAPTER.md) | 검증된 typed 입력의 Core 수용·Observer 처리를 구분. DOMAIN 기능 Owner가 권한·목표·CommandContext를 결정 |
| 로컬 송신 접수 | L02/M03/F07, L03/M04/F11 / [TxPort](../../functions/DOMAIN_TX_PORT.md) | 전체 자료·원 ID·AGE 기준·슬롯을 원자적으로 확보한 뒤 OK. caller 포인터 보관 또는 부분 접수 금지 |
| 운송 | L03/M04/F12, L04/M05·M06, L05/M07·M08 / [CAN](../../functions/CAN_TRANSFER_CONTROL.md), [UART](../../functions/UART_M_TX_RX_CONTROL.md) | Driver accepted, terminal TX, reference release는 각각의 관측. CAN ACK/최종 UART TX로 원격 typed 수신·실행 완료를 판정하지 않음 |
| BCM 인계·실행 | I02/F10 / [명령·결과](../../nodes/BCM.md#command-result-flow) | BCM Task/Superloop의 App callback은 `void`. 실제 실행·피드백·로컬 보호 결과는 BCM 생산자가 별도로 보고 |
| 원 결과의 DOMAIN 귀속 | P01/R01→M01·M03/F08 / [BCM_RESULT](../../functions/can/BCM_RESULT.md#rx) | 원 DOMAIN Boot/commandId/kind와 송신 BCM Boot를 확인. confirmation 없는 Wire에서 CONFIRMED 생성 금지 |
| 표시·음향 | P02/R05→P04, 조건부 P01/R03 / [하향](../UART_M_DOWNLINK.md), [VSS](../../nodes/VSS.md) | DOMAIN 결과, UART 송신, ESP32 유효 수신, BLE 인계, App 표시, VSS 실제 재생은 서로 다른 종료 근거 |

DOMAIN의 C2 호출은 초기 Superloop의 ISR 밖 단일 담당 문맥에서 직렬 실행하고 ISR에서 의미 API를 실행하지 않는다. 다른 MCU의 `BcmApp_*`·`VssApp_*`·`Gateway_Interface_*`는 각 MCU 안의 경계다. DOMAIN이 원격 C 함수를 직접 호출하는 설명이 아니다. 실제 Task/직렬화·Core/Observer·TxPort26 및 필수 실패 sink·Driver epoch/token 바인딩은 OPEN이다. 새 Task, Queue, 타이머, 공통 FSM 또는 공개 API를 추가하지 않는다. [5 Layer·9 Module·F01~F25 색인](../../03_LAYER_MODULE_INDEX.md)을 그대로 사용한다.

### 원본과 메모리의 수명

- 원 사용자 `RequestContext.sessionId/requestId`와 현재 하향 recipient Session을 따로 보존한다. DOMAIN의 `CommandContext.commandId/decisionSequence/hasRequestContext/requestDeviceContextId/origin/commandAgeMs/meta`를 전체 사본으로 인수한다. 없는 원 사용자 요청에는 RequestID를 만들지 않는다.
- commandAgeMs는 명령 생성 이후 경과이고 meta.ageMs는 판단 자료의 나이다. 대상 BCM Boot, 원 DOMAIN Boot, C2 sourceGeneration, CAN TX_SEQUENCE, UART LINK_SEQUENCE는 대체 관계가 아니다. Generation0·Boot 대응·명령 수용 200ms 후보↔구 1초 및 동등 경계는 미결이다.
- Tx의 const 입력은 반환 전 안정 사본 또는 안정 frame으로 확보한다. Rx DTO/meta와 Gateway의 중첩 ByteView는 호출 중 borrowed이며 비동기 소비자는 반환 전에 바이트와 메타까지 보존해야 한다. 값복사는 Wire 구조체 통복사가 아니다.
- `NET_SUBMIT_OK`/`DOMAIN_IF_OK`는 안정 로컬 접수다. `NO_CAPACITY`/`QUEUE_FULL`은 새 항목 전체를 거부하고 기존 Result/Event를 조용히 덮지 않는다. 형식상 유효한 INVALID/STALE 자료는 열화 품질 그대로 운반한다.
- Driver 참조 해제 전 immutable frame을 덮지 않는다. release가 먼저면 물리 frame 반환 가능성과 원 결과·예약 identity의 생존을 분리한다. terminal이 먼저면 reference release까지 참조를 유지한다. 구 epoch/token의 후발 callback은 현재 명령에 붙이지 않는다. 자세한 전이는 [Service 수명](../../functions/NETWORK_SERVICE_LIFETIME.md#retirement)에 한 번만 둔다.
- 원 AGE에 같은 노드에서 아직 반영하지 않은 경과만 한 번 더한다. 노드 간 시각 차감, 재접속 시 AGE0, 미측정 Driver 대기0, 포화65535를 fresh로 승인하는 처리는 금지한다.

<a id="api-map"></a>
## 정확한 기존 API·Data와 함수 정본

아래는 호출을 연결하는 표이며 공개 본체나 Codec을 복제하지 않는다. `D`는 [DOMAIN 원 API](../../../interface/02_NODES/DOMAIN/20_API.md), `B`는 [BCM 원 API](../../../interface/02_NODES/BCM/20_API.md), `G`는 [ESP32 원 API](../../../interface/02_NODES/ESP32/20_API.md)다. 전체 타입은 [DOMAIN Data](../../../interface/02_NODES/DOMAIN/10_DATA.md), [BCM Data](../../../interface/02_NODES/BCM/10_DATA.md), [UART-M Data](../../data/UART_M_DATA.md), 각 정본의 source anchor에서 확인한다.

| 직접 경계·SC | 정확한 기존 선언 / callback | 자료·Function 정본 |
|---|---|---|
| M_REQUEST / 01~04 | D: `DomainIf_Status_t DomainIf_RxMobileRequest(const DomainIf_MobileRequest_t *request);` | [M_REQUEST](../../functions/uart_m/M_REQUEST.md#data-api); `NetMobileRequest`→8종 C2 request union |
| Door command / 01·04 | D: `DomainIf_Status_t DomainIf_TxBcmDoorCommand(const DomainIf_BcmDoorCommand_t *command);` / `txBcmDoorCommand` | [BCM_DOOR_COMMAND](../../functions/can/BCM_DOOR_COMMAND.md); 같은 인자·반환의 TxPort callback |
| Door typed Rx / 01·04 | B: `void BcmApp_OnDomainDoorCommand(const NetBcmDoorCommand *message, const NetCanRxMeta *meta);` | [BCM_DOOR_COMMAND Rx](../../functions/can/BCM_DOOR_COMMAND.md#rx) |
| Climate command / 02 | D: `DomainIf_Status_t DomainIf_TxBcmClimateCommand(const DomainIf_BcmClimateCommand_t *command);` / `txBcmClimateCommand`; B: `void BcmApp_OnDomainClimateCommand(const NetBcmClimateCommand *message, const NetCanRxMeta *meta);` | [BCM_CLIMATE_COMMAND](../../functions/can/BCM_CLIMATE_COMMAND.md) |
| Light command / 03 | D: `DomainIf_Status_t DomainIf_TxBcmInteriorLightCommand(const DomainIf_BcmInteriorLightCommand_t *command);` / `txBcmInteriorLightCommand`; B: `void BcmApp_OnDomainLightCommand(const NetBcmLightCommand *message, const NetCanRxMeta *meta);` | [BCM_LIGHT_COMMAND](../../functions/can/BCM_LIGHT_COMMAND.md) |
| Result / 01~04 | B: `NetSubmitResult BcmNet_SendResult(const NetBcmResult *message);`; D: `DomainIf_Status_t DomainIf_RxBcmCommandResult(const DomainIf_BcmCommandResult_t *result);` | [BCM_RESULT](../../functions/can/BCM_RESULT.md#rx); 결과와 선택 도어 evidence를 원자적으로 인계 |
| Door state / 01 | B: `NetSubmitResult BcmNet_PublishDoorState(const NetBcmDoorState *message);`; D: `DomainIf_Status_t DomainIf_RxBcmDoorState(const DoorState_t *state);` | [BCM_DOOR_STATE](../../functions/can/BCM_DOOR_STATE.md) |
| Climate state / 02 | B: `NetSubmitResult BcmNet_PublishClimateState(const NetBcmClimateState *message);`; D: `DomainIf_Status_t DomainIf_RxBcmClimateState(const ClimateState_t *state);` | [BCM_CLIMATE_STATE](../../functions/can/BCM_CLIMATE_STATE.md) |
| Light state / 03 | B: `NetSubmitResult BcmNet_PublishLightState(const NetBcmLightState *message);`; D: `DomainIf_Status_t DomainIf_RxBcmInteriorLightState(const InteriorLightState_t *state);` | [BCM_LIGHT_STATE](../../functions/can/BCM_LIGHT_STATE.md) |
| 보호 Event / 02·09 | B: `NetSubmitResult BcmNet_SendEvent(const NetBcmEvent *message);`; D: `DomainIf_Status_t DomainIf_RxBcmProtectionEvent(const DomainIf_BcmProtectionEvent_t *event);` | [BCM_EVENT](../../functions/can/BCM_EVENT.md) |
| 현재 진단 / 09 | B: `NetSubmitResult BcmNet_PublishStatus(const NetBcmStatus *message);`; D: `DomainIf_Status_t DomainIf_RxBcmStatus(const DomainIf_BcmStatusReport_t *status);` | [BCM_STATUS](../../functions/can/BCM_STATUS.md) |
| CIS 입력 / 02 | `NetSubmitResult CisNet_PublishEnvironment(const NetCisEnvironment *message);`; `NetSubmitResult CisNet_PublishOccupant(const NetCisOccupant *message);`; D: `DomainIf_Status_t DomainIf_RxCisCabinEnvironment(const CisCabinEnvironmentState_t *state);`; `DomainIf_Status_t DomainIf_RxCisOccupant(const CisOccupantState_t *state);` | [CIS API](../../../interface/02_NODES/CIS/20_API.md), [CIS/Pi](../../nodes/CIS_PI.md), [환경](../../functions/can/CIS_ENVIRONMENT.md), [점유](../../functions/can/CIS_OCCUPANT.md) |
| 사용자 결과 / 01~03 | D: `DomainIf_Status_t DomainIf_TxMobileRequestResult(const DomainIf_MobileRequestResult_t *result);` / `txMobileRequestResult` | [M_RESULT](../../functions/uart_m/M_RESULT.md); 원 RequestSession과 recipient 분리 |
| 설정 / 02·03 | D: `DomainIf_Status_t DomainIf_TxMobileUserSettings(const DomainIf_UserSettingsState_t *settings);` / `txMobileUserSettings` | [M_USER_SETTINGS](../../functions/uart_m/M_USER_SETTINGS.md#conversion) |
| 도어 표시 / 01 | D: `DomainIf_Status_t DomainIf_TxMobileBcmDoorState(const DoorState_t *state);` / `txMobileBcmDoorState` | [M_BCM_DOOR_STATE](../../functions/uart_m/M_BCM_DOOR_STATE.md) |
| 공조 표시 / 02 | D: `DomainIf_Status_t DomainIf_TxMobileBcmClimateState(const ClimateState_t *state);` / `txMobileBcmClimateState` | [M_BCM_CLIMATE_STATE](../../functions/uart_m/M_BCM_CLIMATE_STATE.md) |
| 조명 표시 / 03 | D: `DomainIf_Status_t DomainIf_TxMobileBcmInteriorLightState(const InteriorLightState_t *state);` / `txMobileBcmInteriorLightState` | [M_BCM_LIGHT_STATE](../../functions/uart_m/M_BCM_LIGHT_STATE.md) |
| 진단 표시 / 09 | D: `DomainIf_Status_t DomainIf_TxMobileBcmStatus(const DomainIf_BcmStatusReport_t *state);` / `txMobileBcmStatus` | [M_BCM_STATUS](../../functions/uart_m/M_BCM_STATUS.md) |
| 경고·가용성 / 09 | D: `DomainIf_Status_t DomainIf_TxMobileWarning(const DomainIf_WarningInfo_t *warning);` / `txMobileWarning`; `DomainIf_Status_t DomainIf_TxMobileFunctionAvailability(const DomainIf_FunctionAvailabilitySet_t *availability);` / `txMobileFunctionAvailability` | [M_WARNING](../../functions/uart_m/M_WARNING.md), [M_AVAILABILITY](../../functions/uart_m/M_AVAILABILITY.md) |
| 디지털 키 입력 / 04 | `NetSubmitResult EspNetPriv_PublishContext(const NetMobileContext *message);`; `DomainIf_Status_t DomainNetPriv_OnGatewayContext(const NetMobileContext *message, const NetUartRxMeta *meta);`; D: `DomainIf_Status_t DomainIf_RxDigitalKeyConnection(DeviceContextId_t deviceContextId, const DigitalKeyConnectionState_t *connection);`; `DomainIf_Status_t DomainIf_RxProximity(DeviceContextId_t deviceContextId, const ProximityInput_t *proximity);` | [M_CONTEXT](../../functions/uart_m/M_CONTEXT.md#data-api), [DOMAIN private](../../../interface/02_NODES/DOMAIN/25_PRIVATE_ROUTING.md) |
| 디지털 키 출력 / 04 | D: `DomainIf_Status_t DomainIf_TxMobileDigitalKeyState(const DigitalKeyState_t *state);` / `txMobileDigitalKeyState`; `DomainIf_Status_t DomainIf_TxMobileDigitalKeyResult(const DomainIf_DigitalKeyResult_t *result);` / `txMobileDigitalKeyResult` | [M_DIGITAL_STATUS](../../functions/uart_m/M_DIGITAL_STATUS.md), [M_DIGITAL_RESULT](../../functions/uart_m/M_DIGITAL_RESULT.md#conversion) |
| 음향 / 01 | D: `DomainIf_Status_t DomainIf_TxVssOneShotEvent(const DomainIf_VssOneShotEvent_t *event);` / `txVssOneShotEvent`; `void VssApp_OnDomainEvent(const NetVssEvent *message, const NetCanRxMeta *meta);` | [VSS_EVENT](../../functions/can/VSS_EVENT.md), [VSS API](../../../interface/02_NODES/VSS/20_API.md) |
| Gateway relay / 전 SC | G: `Gateway_Status_t Gateway_Interface_OnMobileMessage(const Gateway_RelayMessageView_t *message);`; `Gateway_Status_t Gateway_Interface_SendToDomain(const Gateway_RelayMessageView_t *message);`; `Gateway_Status_t Gateway_Interface_OnDomainMessage(const Gateway_RelayMessageView_t *message);`; `Gateway_Status_t Gateway_Interface_SendToMobile(const Gateway_RelayMessageView_t *message);` | [Gateway 경계](../../nodes/ESP32_MOBILE.md); 내부 Core/GATT/App 본체는 ESP32·MOBILE 소유 |

각 callback 멤버의 전체 원형·구현 본체는 [TxPort26 정본](../../functions/DOMAIN_TX_PORT.md)에 있다. 송신 실패의 보조 API는 `DomainIf_Status_t DomainIf_RxNetworkLocalFailure(const NetworkLocalFailure_t *failure);`이며 [세 사유·원 identity 범위](../../functions/NETWORK_FAILURE_HEALTH.md)에서만 사용한다. 이것은 아래 각 SC에 새 Wire를 추가하지 않는다. DOMAIN Permission 감독, Link Health, 현재 문맥 준비, 결과 복원 Query도 보조 의존성이다.

<a id="callsite"></a>
## 원 명령·결과를 합성하지 않는 연결 의사코드

다음은 **서로 다른 시점·노드의 기존 call site**다. 함수 본체, 새 공개 wrapper 또는 실행 가능한 제품 C가 아니다. 인자는 이미 원 Owner가 작성한 전체 typed 객체이며 잠금·사본·바인딩의 실제 증거는 앞 표의 정본이 담당한다. `sourceBindingVerified` 같은 문서상 전제는 임의 true를 넣는 스위치가 아니다. 근거가 없으면 해당 정본의 HOLD에서 멈춘다.

```c
// DOMAIN 기능 Owner가 원 CommandContext와 목표를 확정한 시점.
// 공개 Tx가 등록된 callback을 호출하므로 private callback을 이중 호출하지 않는다.
DomainIf_Status_t local = DomainIf_TxBcmDoorCommand(&ownerDoorCommand);
switch (local) {
case DOMAIN_IF_OK:
    // 같은 commandId와 원 RequestContext의 로컬 접수만 기록한다.
    // Driver terminal, BCM typed 인수, BCM 실제 결과는 아직 모른다.
    break;
case DOMAIN_IF_INVALID_ARGUMENT:
case DOMAIN_IF_NOT_INITIALIZED:
case DOMAIN_IF_NOT_AVAILABLE:
case DOMAIN_IF_QUEUE_FULL:
case DOMAIN_IF_REJECTED:
case DOMAIN_IF_UNSUPPORTED:
default:
    // 실제 반환을 원 요청에 귀속한다. 새 명령 ID 발급·BCM FAILED 생성 없음.
    // caller가 가진 입력 포인터를 Network pending으로 취급하지 않는다.
    break;
}

// 시간·노드 경계: 위 반환 다음 줄에서 BCM 실행을 호출하지 않는다.
// 실제 CAN 수신 후 기존 BCM_DOOR_COMMAND Rx가 검증한 message/meta를
// BcmApp_OnDomainDoorCommand()에 한 번 인계한다. void는 실행 성공 아님.

// BCM 실행 Owner가 원 명령별 실제 처리 단계/사유를 평가한 별도 시점.
NetSubmitResult sent = BcmNet_SendResult(&actualBcmResult);
switch (sent) {
case NET_SUBMIT_OK:
    // Network 소유 사본을 접수. 원 Result를 DOMAIN이 받았다는 뜻은 아니다.
    break;
case NET_SUBMIT_NO_CAPACITY:
    // 다른 command의 Result를 제거하지 않는다. 원 결과 Owner의 실패 관측 유지.
    // 실제 재제출 정책이 있다면 원 ID와 증가한 AGE를 사용한다.
    break;
case NET_SUBMIT_NOT_READY:
case NET_SUBMIT_INVALID_ARGUMENT:
default:
    // 거부 원인·원 결과를 보존한다. 새 DONE 또는 다른 command 결과 생성 없음.
    break;
}

// DOMAIN의 실제 Result frame 수신 시점. 입력 frame은 안정 사본이고 profile은
// 양단 채택 근거가 있는 정본 값이어야 한다. consumerStatus는 실제 호출 때만 OUT.
DomainIf_Status_t consumerStatus;
NetCanResult received = NetCan_RxBcmResult(frame, profile, &consumerStatus);
if (received != NET_CAN_OK) {
    // Decode/Boot/순서/confirmation 투영/consumer의 원 거부 단계를 보존한다.
    // 실제 C2 호출이 없었다면 consumerStatus를 읽지 않는다.
    // Core 반영 여부 불명확 또는 Observer만 실패한 입력을 전체 재호출하지 않는다.
} else {
    // SAME_INPUT일 수도 있으므로 OK만 보고 consumerStatus를 읽거나 새 결과를
    // 발행하지 않는다. 정본의 실제 C2 호출·Core 반영 관측으로 원 요청에 연결한다.
}
// DOMAIN 기능 Owner만 원 결과 정책을 결정하여 기존 M_RESULT/DigitalResult를 생산.
// Network는 위 코드 어느 분기에서도 실제 차량 결과·음향 재생·App 표시를 합성하지 않는다.
```

공조·조명은 [API 표](#api-map)의 정확한 대응 Tx/App callback과 기존 leaf를 사용한다. Result kind와 원 명령 종류를 함께 대조하므로 도어의 결과를 공조 요청에 연결하지 않는다. 도어의 좁은 원 귀속 보조 검사는 기존 [NetBcmBoundary_CheckDoorResultOrigin](../../nodes/BCM.md#correlation-function)에 있으며 B17에서 두 번째 본체를 만들지 않는다.

<a id="sc01"></a>
## SC01 — 수동 도어 요청·실행 결과·상태·음향

**직접 집합 7 Wire / 7 Rx.** `BCM_DOOR_COMMAND→BCM`, `BCM_DOOR_STATE→DOMAIN`, `BCM_RESULT→DOMAIN`, `VSS_EVENT→VSS`, `M_REQUEST→DOMAIN`, `M_RESULT→ESP32`, `M_BCM_DOOR_STATE→ESP32`. CAN은 `WIRE-P01-*`와 `RX-P01-*-<수신노드>`, UART-M은 `WIRE-P02-*`와 `RX-P02-*-<수신노드>`의 원 ID다. Node N08/N05/N01/N02/N04, Link P04/P02/P01/R05/R01/R03/I05/I01/I02/I04를 보존한다.

<a id="sc01-normal"></a>
### 정상 조건부 연결과 종결

MOBILE의 실제 도어 요청이 시작 사건이다. ESP32는 현재 peer/Device/Session과 요청 내용을 보존해 Gateway→M_REQUEST를 송신한다. DOMAIN의 Frame/CRC/정확 schema와 수신 순서·문맥 검증 후 `DomainIf_RxMobileRequest`에 인계하며 DOMAIN이 권한·목표·원 commandId를 결정한다. 원 MOBILE SENT AGE가 없는 24B M_REQUEST로 구 SENT부터 3초 정책을 증명하지 못하므로 그 정책에 의존하는 실행은 SAFE_RESTRICTED다.

DOMAIN의 Door Tx→TxPort→CAN 명령→BCM Rx/App 실행→BCM_RESULT와 독립 DoorState→DOMAIN의 원 요청 연결 순서는 [위 관측 단계](#callsite)를 따른다. DoorState의 lock/open/composite 품질을 각각 보존하고 현재값이 목표와 같다는 사실을 해당 명령 DONE으로 바꾸지 않는다. 실제 Result의 COMMAND_DOMAIN_BOOT/commandId/kind, BCM 송신 Boot, 선택 doorEvent/occurrence/eventAge/quality를 함께 연결한다. confirmation 투영 바인딩이 없으면 해당 C2 인계가 HOLD다.

DOMAIN이 실제 결과 의미로 M_RESULT를 생산하고 같은 원 입력에 연결된 DoorState를 M_BCM_DOOR_STATE로 보낸다. DOMAIN이 기존 음향 정책에 따른 의미 사건을 생산한 경우에만 VSS_EVENT가 생성된다. CAN 송신이나 BCM 상태값 일치로 Network가 확인음을 생성하지 않는다. VSS의 callback `void`도 실제 재생 시작·종료가 아니다. 요청 결과 귀속, App 표시, VSS 음향 완료는 독립 종료 증거다. App 소비 증거가 없으면 표시 완료는 미확인으로 남는다.

<a id="sc01-failure"></a>
### 실패와 복구

CAN terminal TX 성공 뒤 BCM 실행 거부/실패가 발생하면 실제 Result/Reason을 BCM Owner가 보고한다. 결과가 오지 않으면 결과 미확인이지 ECU FAILED가 아니다. bus-off·만료·Queue 부족은 해당 전송 시도에 귀속하며 도어 동작 취소 완료로 만들지 않는다. 동일 Request/Command 재전달·늦은 IN_PROGRESS·구 Boot 결과는 새 실행·새 음향·현재 완료를 생성하지 않는다.

<a id="sc01-recovery"></a>
링크 복귀 후 현재 BCM Boot와 새 유효 DoorState, 현재 recipient를 확인한다. 원 결과 이력 복원은 [SC15 Query](QUERY_WARNING_SESSION.md#sc15)이며 과거 DoorCommand/VSS_EVENT를 재실행하지 않는다. 과거 요청 Session A의 결과를 현재 B에 표시할 권한이 확인돼도 원 RequestSession A를 B로 덮지 않는다. 미결 `IF-O03;IF-O12;G01;IF-O22;CF01`과 Driver/실제 소비 HOLD를 유지한다.

<a id="sc02"></a>
## SC02 — 공조 설정·센서 입력·실제 출력 및 보호

**직접 집합 10 Wire / 10 Rx.** `BCM_CLIMATE_COMMAND→BCM`, `BCM_CLIMATE_STATE→DOMAIN`, `BCM_RESULT→DOMAIN`, `BCM_EVENT→DOMAIN`, `CIS_ENVIRONMENT→DOMAIN`, `CIS_OCCUPANT→DOMAIN`, `M_REQUEST→DOMAIN`, `M_RESULT→ESP32`, `M_USER_SETTINGS→ESP32`, `M_BCM_CLIMATE_STATE→ESP32`. Node N08/N05/N01/N02/N03, Link P04/P02/P01/R05/R01/R02/I05/I01/I02/I03다.

<a id="sc02-normal"></a>
### 정상 조건부 연결과 종결

원 사용자 공조 설정과 CIS의 환경·점유 입력이 DOMAIN 정책에 모인다. `CisNet_PublishEnvironment/Occupant`→CAN→해당 C2 Rx는 **각 원 평가의 값·품질·AGE·순번**을 보존한다. 온도·습도·조도 한 항목의 무효가 정상인 다른 항목을 지우지 않는다. Pi 유래 Presence/Count·원 quality와 CIS 중계 평가는 [CIS/Pi 경계](../../nodes/CIS_PI.md)를 따른다. 사용 가능 입력의 정책과 공조 목표는 DOMAIN 책임이다.

DOMAIN Climate Tx→BCM Climate App은 지시 Fan·열 방향·출력 목표를 전달하고, BCM은 실측·방열 보호로 실제 처리를 평가한다. BCM_RESULT, ClimateState, 보호 BCM_EVENT는 각각 다른 수명이다. ClimateState의 Fan 지시/실측, 열 방향/출력, 방열 다섯 품질을 보존한다. DONE을 실내 목표온도 도달로 읽지 않는다. 과열·FAN_MISMATCH·RECOVERY_CONFIRMED의 원 occurrence와 eventAge를 현재 상태 sourceAge로 대체하지 않는다.

DOMAIN은 M_USER_SETTINGS의 목표/설정 존재, M_RESULT의 원 요청 결과, M_BCM_CLIMATE_STATE의 실제 상태를 별도로 MOBILE에 제공한다. C2 설정 부재를 확정 OFF0으로 채우지 않는다. NORMAL 밝기/RGB 부재가 있어 M_USER_SETTINGS composite를 표현할 수 없으면 그 출력만 HOLD하며 공조 State·원 결과의 독립 경로를 자동 삭제하지 않는다. 각 출력의 final TX/ESP32 수신/표시는 따로 관측한다.

<a id="sc02-failure"></a>
### 실패와 복구

온도 STALE·조도 OK 또는 점유 한쪽 누락에서는 원 품질을 보존하며 Network가 공조 정상 목표값을 합성하지 않는다. sensor TTL은 IF-O20/C2 협의 대기다. 공조 명령 지연 만료는 원 commandAge로 제한하고, Result 누락은 목표 미달이나 BCM 고장이 아니다. BCM_EVENT 저장소 부족은 current State 교체와 구분해 손실/거부를 관측한다. CAN 단절을 과열 사건으로 발행하지 않는다.

<a id="sc02-recovery"></a>
BCM의 RECOVERY_CONFIRMED 또는 링크 복귀 후 새 유효 센서·출력·보호 사실을 각각 확인한다. 이전 목표의 자동 재구동은 허가되지 않는다. Query는 원 결과/설정을 복원할 뿐 새 제어가 아니다. `IF-O03;IF-O19;IF-O20;IF-O21;G01` 및 부재 표현·SourceGeneration·양단 binding HOLD를 유지한다.

<a id="sc03"></a>
## SC03 — 실내 조명 설정·적용·표시

**직접 집합 7 Wire / 7 Rx.** `BCM_LIGHT_COMMAND→BCM`, `BCM_LIGHT_STATE→DOMAIN`, `BCM_RESULT→DOMAIN`, `M_REQUEST→DOMAIN`, `M_RESULT→ESP32`, `M_USER_SETTINGS→ESP32`, `M_BCM_LIGHT_STATE→ESP32`. Node N08/N05/N01/N02, Link P04/P02/P01/R05/R01/I05/I01/I02다.

<a id="sc03-normal"></a>
### 정상 조건부 연결과 종결

MOBILE 실내 조명 요청을 DOMAIN이 수용해 NORMAL 설정과 알림 출력의 의미를 구분하고 원 LightCommand를 생성한다. C2 `DomainIf_TxBcmInteriorLightCommand`→TxPort→CAN `BCM_LIGHT_COMMAND`→`BcmApp_OnDomainLightCommand`가 종류·밝기·RGB를 전달한다. 이름에 Interior가 없는 BCM 로컬 API도 실내 조명만 담당한다. EXTERIOR_LIGHT는 추가하지 않는다.

BCM은 적용 결과와 LightState를 각각 보고한다. `light_physical_feedback_capability=0`일 때 APPLIED는 출력 지시 적용이며 실제 점등 확인이 아니다. DOMAIN의 M_RESULT, M_USER_SETTINGS, M_BCM_LIGHT_STATE는 원 요청·설정·현재 상태를 각각 유지한다. `hasNormalLightLevel/Color=false`이면 해당 stale 값은 읽지 않고 현 UART에 부재 sentinel이 없는 전체 설정 composite를 HOLD한다. 반대로 확인된 밝기0, RGB0 및 성분255는 정상 값이다. App 실제 표시 여부는 Gateway local OK와 별개다.

<a id="sc03-failure"></a>
### 실패와 복구

명령이 송신 전 만료되면 아직 미송신인 해당 시도만 종료하며 실제 소등/점등 완료를 만들지 않는다. Result 또는 LightState 한쪽만 오면 적용 결과와 실제 상태의 증거를 분리한다. 최신 State 뒤 과거 Boot/평가 자료가 오면 표시를 후퇴시키지 않는다. 수용 후 운송 실패·공간 부족은 원 결과에 귀속하고 다른 discrete 결과를 덮지 않는다.

<a id="sc03-recovery"></a>
재접속 후 현재 Boot의 유효 LightState와 원 설정을 확인하고 필요하면 Query로 복원한다. 과거 LightCommand를 자동 재실행하거나 물리 피드백 미지원을 지원됨으로 바꾸지 않는다. `IF-O21;IF-O12;G01` 및 command 수명·결과 확인·profile HOLD를 유지한다.

<a id="sc04"></a>
## SC04 — 디지털 키 문맥·자동 Unlock 상태/결과

**직접 집합 6 Wire / 6 Rx.** `BCM_DOOR_COMMAND→BCM`, `BCM_RESULT→DOMAIN`, `M_CONTEXT→DOMAIN`, `M_REQUEST→DOMAIN`, `M_DIGITAL_STATUS→ESP32`, `M_DIGITAL_RESULT→ESP32`. Node N08/N05/N01/N02, Link P04/P02/P01/R05/R01/I05/I01/I02다. 실제 설정 요청이 있는 가지에만 M_REQUEST가 관여한다. 도어 현재값·Query·Permission은 보조 근거이며 직접 집합을 늘리지 않는다.

<a id="sc04-normal"></a>
### 정상 조건부 연결과 종결

ESP32가 생산한 등록·현재 연결 공동 평가, MOBILE의 실제 App 활성, ESP32의 실제 근접 관측이 각 품질/sequence/AGE로 M_CONTEXT에 실린다. `EspNetPriv_PublishContext`→UART→`DomainNetPriv_OnGatewayContext`는 `DomainIf_RxDigitalKeyConnection`과 `DomainIf_RxProximity`에 분리 인계한다. 등록·연결을 갱신해도 App/근접 나이를 새로 시작하지 않는다. Core 일부 반영/Observer 실패가 있으면 실제 반영 단계를 남기며 전체 M_CONTEXT를 재호출하여 중복 실행하지 않는다.

DOMAIN 자동 정책 Owner가 유효한 원 관측과 기존 설정에 따라 자동 Unlock 필요성을 판단한다. Network가 자체 NEAR 판정이나 자동 명령을 만들지 않는다. 필요한 경우 원 DoorCommand→BCM 실제 Result가 연결된다. 원 자동 occurrence와 relatedDoorCommandId는 서로 다르며 실제 MOBILE 요청이 없으면 M_RESULT용 RequestID를 만들지 않는다.

DOMAIN은 M_DIGITAL_STATUS로 설정·허용 상태를, M_DIGITAL_RESULT로 자동 사건의 실제 결과를 제공한다. C2의 `actualUnlockTransition`은 현 Wire에 없으므로 이를 표시할 필요가 있는 입력은 IF-O22 HOLD다. `DONE+ALREADY_AT_TARGET`을 실제 Unlock 전이로 확장하지 않는다. `hasVehicleResult=false`일 때 비활성 result를 읽지 않는다. 기본 결과 전송 성공도 없는 actualUnlockTransition=false를 복원했다는 뜻은 아니다.

<a id="sc04-failure"></a>
### 실패와 복구

BLE가 connected인데 App 활성 관측이 끊겼거나 proximity가 INVALID/STALE이면 연결 사실로 ACTIVE·NEAR를 채우지 않는다. 같은 접근 원 관측의 반복과 이전 Session의 지연 Context/DigitalResult는 새 접근·새 Unlock을 만들지 않는다. 자동 명령의 bus-off는 운송 관측이며 디지털 키 성공 또는 물리 해제 완료를 생성하지 않는다. Generation0·발급·Session 재사용 정책은 회신 대기다.

<a id="sc04-recovery"></a>
재접속 뒤 현재 peer/Device/Session과 세 관측의 원 유효성을 각각 확인한다. 새 문맥·Query 가능성은 차량 자동동작 승인과 다르며 과거 접근/결과는 재생하지 않는다. `IF-O08;IF-O09;IF-O22;G01;IF-N01`과 CF01·실제 C2/Gateway binding HOLD를 유지한다.

<a id="sc09"></a>
## SC09 — BCM 보호 발생·현재 진단·가용성

**직접 집합 5 Wire / 5 Rx.** `BCM_EVENT→DOMAIN`, `BCM_STATUS→DOMAIN`, `M_WARNING→ESP32`, `M_AVAILABILITY→ESP32`, `M_BCM_STATUS→ESP32`. Node N02/N01/N05/N08, Link P01/P02/P04/R01/R05/I02/I01/I05다. local Network Health는 보조 관측이며 새 Wire나 BCM 실행 Result가 아니다.

<a id="sc09-normal"></a>
### 정상 조건부 연결과 종결

BCM 보호 담당이 실제 과열/불일치/복구 occurrence를 생산해 `BcmNet_SendEvent`로 제출한다. 현재 진단 담당은 current fault_mask/대표 code/category/affected function/recovering와 평가 sequence/AGE를 `BcmNet_PublishStatus`에 제공한다. 각 CAN frame 검증과 원 기동·순서 수용 후 DOMAIN의 `DomainIf_RxBcmProtectionEvent`와 `DomainIf_RxBcmStatus`에 따로 인계한다.

**BCM_STATUS에는 LAST_FAULT가 없다.** 발생/최근 이력은 기존 진단 Owner가 보존하고, 현재 SOURCE_AGE는 그 이력 나이를 대신하지 않는다. BCM_RESULT에 붙는 선택 도어 근거도 BCM_EVENT로 복제하지 않는다. Network가 공급한 실제 bus-off/Rx-loss와 BCM 보호 평가를 구분하고, 진단·가용성·경고의 생성/해제는 기능 Owner가 결정한다.

DOMAIN은 원 현재 진단의 동일 입력에 연결된 M_BCM_STATUS, 기능별 M_AVAILABILITY, 원 warning occurrence의 M_WARNING을 생성한다. Gateway→BLE→App은 세 의미를 혼합하지 않는다. 지원되지 않는 가용성이나 Warning 표현은 [하향 정본](../UART_M_DOWNLINK.md)의 해당 출력 HOLD이며 UNKNOWN을 정상0/CLEAR로 채우지 않는다. 진단 해제와 App 경고 읽음, 현재 상태 전달과 과거 이력 종료는 별개다.

<a id="sc09-failure"></a>
### 실패와 복구

현재 정상 Status 뒤 과거 Fault Event가 도착하면 발생 이력으로만 연결하고 current fault를 역전시키지 않는다. BCM_EVENT가 없다는 이유로 정상/실패를 확정하거나 사건형 무발생 timeout fault를 만들지 않는다. bus-off는 Network 관측이며 임의 BCM 전체 실패 또는 보호 code가 아니다. Event 저장소 부족은 silent overwrite 없이 거부/손실 관측을 남기고 그 실제 BCM Owner sink가 없으면 바인딩 HOLD다.

<a id="sc09-recovery"></a>
새 Boot 또는 링크 up만으로 실제 보호 해제를 판정하지 않는다. 새 현재 진단과 BCM의 실제 회복 근거를 기능 Owner가 재평가한다. 이력은 유지하고 이전 공조/도어 동작을 재구동하지 않는다. App 복원은 현재 recipient와 성공 Getter의 같은 입력 증거를 확인한 Query 경계다. `IF-O15;IF-O19;IF-O12` 및 health→진단 실제 경계·대표 fault 우선순위·표현 HOLD를 유지한다.

<a id="counterexamples"></a>
## 전문 자체검수 반례와 제한

아래는 원 S4C의 각 SC별 10개 반례를 전문 확인한 뒤 B17 연결에서 재대입한 우선 반례다. **STATIC_DOCUMENT_WALKTHROUGH_NOT_RUNTIME_TEST**이며 제품·Host·보드·BLE/App 시험은 NOT_EXECUTED다. 문서의 기능 경로가 연결되어도 남는 계약 HOLD가 해소된 것은 아니다.

| ID / SC | 이전 상태·입력 | 경계 함수·부작용 | 관측·사후 상태 / 이유·남는 HOLD |
|---|---|---|---|
| B17-BCM-01 / SC01 | 원 DOMAIN Boot7/command8, target BCM9 접수. CAN TX 성공, BCM 구동 실패 또는 Result 누락 | `DomainIf_TxBcmDoorCommand`·`BcmNet_SendResult`·`NetCan_RxBcmResult`; Network DONE 생성 없음 | 실제 FAILED는 BCM 원 보고가 있어야 함. 없으면 결과 미확인. confirmation/G01·IF-O12 HOLD |
| B17-BCM-02 / SC01 | target BCM9 명령 대기 중 BCM10으로 재기동; 늦은 BCM9 Result 또는 옛 callback | 기존 원 귀속 검사/Service epoch-token 검사; 새 명령·슬롯 불변 | 과거 원 결과와 새 Boot 분리, 자동 재실행/새 완료 금지. Boot lifecycle·Driver 바인딩 HOLD |
| B17-BCM-03 / SC02 | 조도 OK·온도 STALE, fan_command OK·fan_measured INVALID | `DomainIf_RxCisCabinEnvironment`·`DomainIf_RxBcmClimateState`; 나머지 정상 품질 유지 | 단일 센서/출력 누락을 일괄 정상화·일괄 폐기하지 않음. 사용 판단은 DOMAIN, IF-O19/20 HOLD |
| B17-BCM-04 / SC02 | BCM이 새 복구 발생으로 평가한 RECOVERY_CONFIRMED 수신, 예전 공조 목표 존재. 재전송은 그 복구 occurrence 유지 | `DomainIf_RxBcmProtectionEvent`; 원 발생 인계만, 자동 Climate Tx 없음 | 회복 근거와 새 제어 판단 분리. 이전 목표 자동 재구동 금지 |
| B17-BCM-05 / SC03 | LightState APPLIED·feedback_capability0, NORMAL color 존재 flag false | `DomainIf_RxBcmInteriorLightState`, `NetUartM_PrepareUserSettings`; 설정 OUT 불변/HOLD | 물리 점등 확인 생성 없음. RGB0/255로 부재 채움 금지. IF-O21; 실제 sourceBinding 미확인 |
| B17-BCM-06 / SC04 | BLE connected 정상, App 원 관측 STALE, proximity INVALID; 과거 자동 occurrence 있음 | `DomainNetPriv_OnGatewayContext`→두 C2 Rx, 독립 meta 보존 | ACTIVE/NEAR·새 occurrence·Unlock 합성 없음. 현재 Session 검증과 기능 허용 분리. IF-O08/09/IF-N01 |
| B17-BCM-07 / SC04 | 실제 전이 표시가 필요한 C2 actualUnlockTransition=true, 또는 DONE+ALREADY_AT_TARGET | `NetUartM_PrepareDigitalResult`; 전이 손실 입력 CONTRACT_HOLD | 없는 Wire bool 생성·실제 Unlock 확인음 합성 금지. IF-O22 유지 |
| B17-BCM-08 / SC09 | 최신 정상 BCM_STATUS 이후 오래된 BCM_EVENT 또는 bus-off 도착 | `DomainIf_RxBcmStatus`·`DomainIf_RxBcmProtectionEvent`·기존 Health; current 상태와 발생/통신 기록 분리 | 현재 fault를 옛 사건으로 덮지 않음. LAST_FAULT 필드 창작/침묵 timeout Fault 없음. IF-O15/19/12 |

반례의 원 출처는 S4C `E2E_SCENARIO_18.md#sc01/#sc02/#sc03/#sc04/#sc09`의 50개 셀과 시간·AGE·자원·복구 계약, 현재 Interface/B16 정본이며 제출물 coverage와 전문 읽기 로그에 경로·SHA를 기록했다. 정상/실패는 같은 API·Function 정본을 가리킨다. 협의 변경은 원 S6Q-01~14에 영향만 누적하고 S6 완료 후 실제 합의·별도 승인으로 통합 반영한다.
