# M03 노드 타입 인계

기존 공개 API의 타입·호출 조건·반환 의미를 지키며 의미 자료를 전달한다.

## 입력·출력과 책임

| 경계 | 자료와 의미 |
|---|---|
| 대표 입력 | NetCisRear와 검증된 출처, CIS 기능 모듈의 제출 |
| 대표 출력 | CisNet_PublishRear 접수 또는 DomainIf_RxCisRear 호출 |
| 보유 상태와 수명 | 실제 소비자 준비·등록 근거(ST04). DOMAIN Manager cache와 기능 판단 상태는 DOMAIN 소유다. |

F10의 CIS 송신과 F08의 DOMAIN 수신을 대표 경로로 구체화한다. 전체 F06~F10의 등록·TxPort·Rx·후발 실패·노드 경계는 아래 CAN/UART/DOMAIN 정본을 따른다. F01의 원본 연결은 M01과 함께 유지하며 공개 API 본체의 소유권을 옮기지 않는다.

## 정상 처리

CIS 제출은 안정 복사 후 접수 여부를 반환한다. DOMAIN 수신은 rearDistanceMm·measurementState·distanceQuality·sourceGeneration을 같은 원 관측에서 투영한다. 소비자는 호출 동안만 입력 포인터를 빌려 읽고, 반환 이후 필요한 값은 그 전에 복사한다. 접수 성공 뒤 caller는 자기 입력 저장소를 재사용할 수 있다.

## 실패와 복구

변환 불가·NULL·소비자 미준비는 정상 호출로 세지 않는다. DomainIf 실패 후 소비 성공을 기록하지 않으며 API 본체를 Network가 중복 구현하지 않는다.

CIS 접수와 DOMAIN 인계는 서로 다른 MCU의 담당 문맥에서 실행한다. DOMAIN의 API 지원·초기화·등록 정보는 인계 전에 확인하고 ISR에서 공개 Rx를 직접 호출하지 않는다.

## 함수와 자료 탐색

`NetRear_*`는 `PROPOSED_PRIVATE_FUNCTION`인 새 내부 설계 원형이다. 기존 `CisNet_PublishRear`는 제공된 `PROPOSED_DESIGN/BINDING_OPEN` 경계다. `DomainIf_RxCisRear`는 `DECLARED_SOURCE`인 DOMAIN 소유 선언이다. 아래 표는 해당 모듈의 책임 연결이며 제품 함수의 존재나 외부 승인 사실을 뜻하지 않는다.

| 전체 원형 | 구분·해당 모듈 역할 | 상세 |
|---|---|---|
| `NetSubmitResult CisNet_PublishRear(const NetCisRear *message);` | 기존 설계 경계. F10 송신 경계: CIS 기능 관측을 M04의 안정 사본으로 넘기고 접수 결과를 반환한다. | [CisNet_PublishRear](../functions/CIS_REAR_TX_SUBMIT.md#publish-rear) |
| `NetRearResult NetRear_Deliver(uint32_t nowMs);` | 새 내부 함수. F08 수신 경계: 단위·품질·계보를 명시 변환하고 DOMAIN 공개 Rx를 호출한다. | [NetRear_Deliver](../functions/DOMAIN_CIS_REAR_INGRESS.md#adapter) |
| `DomainIf_Status_t DomainIf_RxCisRear( const CisRearState_t *state);` | 기존 외부 소유 선언. DOMAIN이 소유한 수신 본체의 호출 경계다. Network는 이 본체를 작성하지 않는다. | [DomainIf_RxCisRear](../functions/DOMAIN_CIS_REAR_INGRESS.md#domain-api) |

타입·멤버의 완전 정의는 Data 정본에만 두며 각 함수의 인자 수명·반환·의사코드는 연결된 상세에서 확인한다.

[Data 정본](../data/CIS_REAR_DATA.md) · [전체 흐름](../flows/CIS_REAR_144_TO_344.md) · [상위 Layer](../layers/L02_TYPED_HANDOFF.md) · [전체 색인](../03_LAYER_MODULE_INDEX.md)

## CAN 전체 적용 — F06/F07/F08/F09/F10

CAN9 TxPort 의미 입력과 각 노드의 실제 publish/Rx API를 연결한다. copy-before-OK, 정확한 C2 반환, 필수 failure sink 준비를 확인하며 DOMAIN 공개 본체를 구현하지 않는다.

[공통 제어 함수·기존 API](../functions/CAN_TRANSFER_CONTROL.md#admission) · [공통 Codec 원형](../functions/CAN_COMMON_CODEC.md#check-frame) · [메시지별 실제 API/제안 함수](../04_CAN_FD_OVERVIEW.md#messages) · [Data](../data/CAN_COMMON_DATA.md) · [흐름](../flows/CAN_NETWORK_PATHS.md)

## CAN 함수 직접 탐색

위 원형 표는 후방 대표 경로다. 아래 공개 경계의 외부 본체 소유권과 Network 내부 제안을 구분하며, 같은 함수가 여러 Module 책임의 접점일 수 있다. F 번호별 래퍼를 추가하지 않는다.

| 전체 원형 | 이 Module의 역할/소유 | 상세 |
|---|---|---|
| `DomainIf_Status_t DomainIf_SetTxPort(const DomainIf_TxPort_t *port);` | DOMAIN 본체; Network는 실제 지원/수명 확인 후 등록 요청 | [함수 상세](../functions/CAN_TRANSFER_CONTROL.md#registration) |
| `DomainIf_Status_t DomainIf_RxNetworkLocalFailure(const NetworkLocalFailure_t *failure);` | DOMAIN 본체; Network가 원 identity에 귀속하여 필수 sink 호출 | [함수 상세](../functions/CAN_TRANSFER_CONTROL.md#failure) |
| `DomainIf_Status_t DomainIf_TxBcmClimateCommand( const DomainIf_BcmClimateCommand_t *command);` | DOMAIN 공개 본체/노드 App 소유; Network 호출 경계 | [함수 상세](../functions/can/BCM_CLIMATE_COMMAND.md#api-DomainIf_TxBcmClimateCommand) |
| `void BcmApp_OnDomainClimateCommand( const NetBcmClimateCommand *message, const NetCanRxMeta *meta );` | DOMAIN 공개 본체/노드 App 소유; Network 호출 경계 | [함수 상세](../functions/can/BCM_CLIMATE_COMMAND.md#api-BcmApp_OnDomainClimateCommand) |
| `DomainIf_Status_t NetCan_TxBcmClimateCommand(const DomainIf_BcmClimateCommand_t *command);` | Network 로컬 기존 경계 또는 명시된 내부 제안; typed 접수·투영·호출 | [함수 상세](../functions/can/BCM_CLIMATE_COMMAND.md) |
| `NetCanResult NetCan_RxBcmClimateCommand(const NetCanFrame *frame, NetCanProfile profile);` | Network 로컬 기존 경계 또는 명시된 내부 제안; typed 접수·투영·호출 | [함수 상세](../functions/can/BCM_CLIMATE_COMMAND.md#rx) |
| `NetSubmitResult BcmNet_PublishClimateState( const NetBcmClimateState *message );` | Network 로컬 기존 경계 또는 명시된 내부 제안; typed 접수·투영·호출 | [함수 상세](../functions/can/BCM_CLIMATE_STATE.md#api-BcmNet_PublishClimateState) |
| `DomainIf_Status_t DomainIf_RxBcmClimateState( const ClimateState_t *state);` | DOMAIN 공개 본체/노드 App 소유; Network 호출 경계 | [함수 상세](../functions/can/BCM_CLIMATE_STATE.md#api-DomainIf_RxBcmClimateState) |
| `NetCanResult NetCan_RxBcmClimateState(const NetCanFrame *frame, NetCanProfile profile, DomainIf_Status_t *consumerStatus);` | Network 로컬 기존 경계 또는 명시된 내부 제안; typed 접수·투영·호출 | [함수 상세](../functions/can/BCM_CLIMATE_STATE.md#rx) |
| `DomainIf_Status_t DomainIf_TxBcmDoorCommand( const DomainIf_BcmDoorCommand_t *command);` | DOMAIN 공개 본체/노드 App 소유; Network 호출 경계 | [함수 상세](../functions/can/BCM_DOOR_COMMAND.md#api-DomainIf_TxBcmDoorCommand) |
| `void BcmApp_OnDomainDoorCommand( const NetBcmDoorCommand *message, const NetCanRxMeta *meta );` | DOMAIN 공개 본체/노드 App 소유; Network 호출 경계 | [함수 상세](../functions/can/BCM_DOOR_COMMAND.md#api-BcmApp_OnDomainDoorCommand) |
| `DomainIf_Status_t NetCan_TxBcmDoorCommand(const DomainIf_BcmDoorCommand_t *command);` | Network 로컬 기존 경계 또는 명시된 내부 제안; typed 접수·투영·호출 | [함수 상세](../functions/can/BCM_DOOR_COMMAND.md) |
| `NetCanResult NetCan_RxBcmDoorCommand(const NetCanFrame *frame, NetCanProfile profile);` | Network 로컬 기존 경계 또는 명시된 내부 제안; typed 접수·투영·호출 | [함수 상세](../functions/can/BCM_DOOR_COMMAND.md#rx) |
| `NetSubmitResult BcmNet_PublishDoorState( const NetBcmDoorState *message );` | Network 로컬 기존 경계 또는 명시된 내부 제안; typed 접수·투영·호출 | [함수 상세](../functions/can/BCM_DOOR_STATE.md#api-BcmNet_PublishDoorState) |
| `DomainIf_Status_t DomainIf_RxBcmDoorState( const DoorState_t *state);` | DOMAIN 공개 본체/노드 App 소유; Network 호출 경계 | [함수 상세](../functions/can/BCM_DOOR_STATE.md#api-DomainIf_RxBcmDoorState) |
| `NetCanResult NetCan_RxBcmDoorState(const NetCanFrame *frame, NetCanProfile profile, DomainIf_Status_t *consumerStatus);` | Network 로컬 기존 경계 또는 명시된 내부 제안; typed 접수·투영·호출 | [함수 상세](../functions/can/BCM_DOOR_STATE.md#rx) |
| `NetSubmitResult BcmNet_SendEvent( const NetBcmEvent *message );` | Network 로컬 기존 경계 또는 명시된 내부 제안; typed 접수·투영·호출 | [함수 상세](../functions/can/BCM_EVENT.md#api-BcmNet_SendEvent) |
| `DomainIf_Status_t DomainIf_RxBcmProtectionEvent( const DomainIf_BcmProtectionEvent_t *event);` | DOMAIN 공개 본체/노드 App 소유; Network 호출 경계 | [함수 상세](../functions/can/BCM_EVENT.md#api-DomainIf_RxBcmProtectionEvent) |
| `NetCanResult NetCan_RxBcmEvent(const NetCanFrame *frame, NetCanProfile profile, DomainIf_Status_t *consumerStatus);` | Network 로컬 기존 경계 또는 명시된 내부 제안; typed 접수·투영·호출 | [함수 상세](../functions/can/BCM_EVENT.md#rx) |
| `DomainIf_Status_t DomainIf_TxBcmInteriorLightCommand( const DomainIf_BcmInteriorLightCommand_t *command);` | DOMAIN 공개 본체/노드 App 소유; Network 호출 경계 | [함수 상세](../functions/can/BCM_LIGHT_COMMAND.md#api-DomainIf_TxBcmInteriorLightCommand) |
| `void BcmApp_OnDomainLightCommand( const NetBcmLightCommand *message, const NetCanRxMeta *meta );` | DOMAIN 공개 본체/노드 App 소유; Network 호출 경계 | [함수 상세](../functions/can/BCM_LIGHT_COMMAND.md#api-BcmApp_OnDomainLightCommand) |
| `DomainIf_Status_t NetCan_TxBcmLightCommand(const DomainIf_BcmInteriorLightCommand_t *command);` | Network 로컬 기존 경계 또는 명시된 내부 제안; typed 접수·투영·호출 | [함수 상세](../functions/can/BCM_LIGHT_COMMAND.md) |
| `NetCanResult NetCan_RxBcmLightCommand(const NetCanFrame *frame, NetCanProfile profile);` | Network 로컬 기존 경계 또는 명시된 내부 제안; typed 접수·투영·호출 | [함수 상세](../functions/can/BCM_LIGHT_COMMAND.md#rx) |
| `NetSubmitResult BcmNet_PublishLightState( const NetBcmLightState *message );` | Network 로컬 기존 경계 또는 명시된 내부 제안; typed 접수·투영·호출 | [함수 상세](../functions/can/BCM_LIGHT_STATE.md#api-BcmNet_PublishLightState) |
| `DomainIf_Status_t DomainIf_RxBcmInteriorLightState( const InteriorLightState_t *state);` | DOMAIN 공개 본체/노드 App 소유; Network 호출 경계 | [함수 상세](../functions/can/BCM_LIGHT_STATE.md#api-DomainIf_RxBcmInteriorLightState) |
| `NetCanResult NetCan_RxBcmLightState(const NetCanFrame *frame, NetCanProfile profile, DomainIf_Status_t *consumerStatus);` | Network 로컬 기존 경계 또는 명시된 내부 제안; typed 접수·투영·호출 | [함수 상세](../functions/can/BCM_LIGHT_STATE.md#rx) |
| `NetSubmitResult BcmNet_SendResult( const NetBcmResult *message );` | Network 로컬 기존 경계 또는 명시된 내부 제안; typed 접수·투영·호출 | [함수 상세](../functions/can/BCM_RESULT.md#api-BcmNet_SendResult) |
| `DomainIf_Status_t DomainIf_RxBcmCommandResult( const DomainIf_BcmCommandResult_t *result);` | DOMAIN 공개 본체/노드 App 소유; Network 호출 경계 | [함수 상세](../functions/can/BCM_RESULT.md#api-DomainIf_RxBcmCommandResult) |
| `NetCanResult NetCan_RxBcmResult(const NetCanFrame *frame, NetCanProfile profile, DomainIf_Status_t *consumerStatus);` | Network 로컬 기존 경계 또는 명시된 내부 제안; typed 접수·투영·호출 | [함수 상세](../functions/can/BCM_RESULT.md#rx) |
| `NetSubmitResult BcmNet_PublishStatus( const NetBcmStatus *message );` | Network 로컬 기존 경계 또는 명시된 내부 제안; typed 접수·투영·호출 | [함수 상세](../functions/can/BCM_STATUS.md#api-BcmNet_PublishStatus) |
| `DomainIf_Status_t DomainIf_RxBcmStatus( const DomainIf_BcmStatusReport_t *status);` | DOMAIN 공개 본체/노드 App 소유; Network 호출 경계 | [함수 상세](../functions/can/BCM_STATUS.md#api-DomainIf_RxBcmStatus) |
| `NetCanResult NetCan_RxBcmStatus(const NetCanFrame *frame, NetCanProfile profile, DomainIf_Status_t *consumerStatus);` | Network 로컬 기존 경계 또는 명시된 내부 제안; typed 접수·투영·호출 | [함수 상세](../functions/can/BCM_STATUS.md#rx) |
| `NetSubmitResult CisNet_PublishEnvironment( const NetCisEnvironment *message );` | Network 로컬 기존 경계 또는 명시된 내부 제안; typed 접수·투영·호출 | [함수 상세](../functions/can/CIS_ENVIRONMENT.md#api-CisNet_PublishEnvironment) |
| `DomainIf_Status_t DomainIf_RxCisCabinEnvironment( const CisCabinEnvironmentState_t *state);` | DOMAIN 공개 본체/노드 App 소유; Network 호출 경계 | [함수 상세](../functions/can/CIS_ENVIRONMENT.md#api-DomainIf_RxCisCabinEnvironment) |
| `NetCanResult NetCan_DeliverCisEnvironment(const NetCisEnvironment *message, const NetCanRxMeta *meta);` | Network 로컬 기존 경계 또는 명시된 내부 제안; typed 접수·투영·호출 | [함수 상세](../functions/can/CIS_ENVIRONMENT.md#receive) |
| `NetSubmitResult CisNet_PublishOccupant( const NetCisOccupant *message );` | Network 로컬 기존 경계 또는 명시된 내부 제안; typed 접수·투영·호출 | [함수 상세](../functions/can/CIS_OCCUPANT.md#api-CisNet_PublishOccupant) |
| `DomainIf_Status_t DomainIf_RxCisOccupant( const CisOccupantState_t *state);` | DOMAIN 공개 본체/노드 App 소유; Network 호출 경계 | [함수 상세](../functions/can/CIS_OCCUPANT.md#api-DomainIf_RxCisOccupant) |
| `NetCanResult NetCan_DeliverCisOccupant(const NetCisOccupant *message, const NetCanRxMeta *meta);` | Network 로컬 기존 경계 또는 명시된 내부 제안; typed 접수·투영·호출 | [함수 상세](../functions/can/CIS_OCCUPANT.md#receive) |
| `NetSubmitResult CisNet_PublishStatus( const NetCisStatus *message );` | Network 로컬 기존 경계 또는 명시된 내부 제안; typed 접수·투영·호출 | [함수 상세](../functions/can/CIS_STATUS.md#api-CisNet_PublishStatus) |
| `DomainIf_Status_t DomainIf_RxCisStatus( const DomainIf_CisStatusReport_t *status);` | DOMAIN 공개 본체/노드 App 소유; Network 호출 경계 | [함수 상세](../functions/can/CIS_STATUS.md#api-DomainIf_RxCisStatus) |
| `NetCanResult NetCan_DeliverCisStatus(const NetCisStatus *message, const NetCanRxMeta *meta);` | Network 로컬 기존 경계 또는 명시된 내부 제안; typed 접수·투영·호출 | [함수 상세](../functions/can/CIS_STATUS.md#receive) |
| `NetSubmitResult DomainNet_PublishPermission(const NetDomainPermission *message);` | Network 로컬 기존 경계 또는 명시된 내부 제안; typed 접수·투영·호출 | [함수 상세](../functions/can/DOMAIN_PERMISSION.md#api-DomainNet_PublishPermission) |
| `void BcmNetPriv_OnDomainPermission( const NetDomainPermission *permission, const NetCanRxMeta *meta );` | Network 로컬 기존 경계 또는 명시된 내부 제안; typed 접수·투영·호출 | [함수 상세](../functions/can/DOMAIN_PERMISSION.md#api-BcmNetPriv_OnDomainPermission) |
| `void CisApp_OnDomainPermission( const NetDomainPermission *message, const NetCanRxMeta *meta );` | DOMAIN 공개 본체/노드 App 소유; Network 호출 경계 | [함수 상세](../functions/can/DOMAIN_PERMISSION.md#api-CisApp_OnDomainPermission) |
| `void WindowApp_OnDomainPermission(const NetDomainPermission *message, const NetCanRxMeta *meta);` | DOMAIN 공개 본체/노드 App 소유; Network 호출 경계 | [함수 상세](../functions/can/DOMAIN_PERMISSION.md#api-WindowApp_OnDomainPermission) |
| `void VssNetPriv_OnDomainPermission(const NetDomainPermission *message, const NetCanRxMeta *meta);` | Network 로컬 기존 경계 또는 명시된 내부 제안; typed 접수·투영·호출 | [함수 상세](../functions/can/DOMAIN_PERMISSION.md#api-VssNetPriv_OnDomainPermission) |
| `DomainIf_Status_t DomainIf_TxCisPowerPermission( const DomainIf_CisPowerPermission_t *permission);` | DOMAIN 공개 본체/노드 App 소유; Network 호출 경계 | [함수 상세](../functions/can/DOMAIN_PERMISSION.md#api-DomainIf_TxCisPowerPermission) |
| `DomainIf_Status_t DomainIf_TxWindowOperationPermission( const DomainIf_WindowOperationPermission_t *permission);` | DOMAIN 공개 본체/노드 App 소유; Network 호출 경계 | [함수 상세](../functions/can/DOMAIN_PERMISSION.md#api-DomainIf_TxWindowOperationPermission) |
| `DomainIf_Status_t DomainIf_TxWindowDomainAlive( const DomainIf_WindowDomainAlive_t *alive);` | DOMAIN 공개 본체/노드 App 소유; Network 호출 경계 | [함수 상세](../functions/can/DOMAIN_PERMISSION.md#api-DomainIf_TxWindowDomainAlive) |
| `NetCanResult NetCan_RxDomainPermission(const NetCanFrame *frame, NetCanNode receiver, NetCanProfile profile);` | Network 로컬 기존 경계 또는 명시된 내부 제안; typed 접수·투영·호출 | [함수 상세](../functions/can/DOMAIN_PERMISSION.md#rx) |
| `DomainIf_Status_t DomainIf_TxVssOneShotEvent( const DomainIf_VssOneShotEvent_t *event);` | DOMAIN 공개 본체/노드 App 소유; Network 호출 경계 | [함수 상세](../functions/can/VSS_EVENT.md#api-DomainIf_TxVssOneShotEvent) |
| `void VssApp_OnDomainEvent(const NetVssEvent *message, const NetCanRxMeta *meta);` | DOMAIN 공개 본체/노드 App 소유; Network 호출 경계 | [함수 상세](../functions/can/VSS_EVENT.md#api-VssApp_OnDomainEvent) |
| `DomainIf_Status_t NetCan_TxVssEvent(const DomainIf_VssOneShotEvent_t *event);` | Network 로컬 기존 경계 또는 명시된 내부 제안; typed 접수·투영·호출 | [함수 상세](../functions/can/VSS_EVENT.md) |
| `NetCanResult NetCan_DeliverVssEvent(const NetVssEvent *message, const NetCanRxMeta *meta);` | Network 로컬 기존 경계 또는 명시된 내부 제안; typed 접수·투영·호출 | [함수 상세](../functions/can/VSS_EVENT.md#receive) |
| `NetSubmitResult VssNet_PublishStatus(const NetVssStatus *message);` | Network 로컬 기존 경계 또는 명시된 내부 제안; typed 접수·투영·호출 | [함수 상세](../functions/can/VSS_STATUS.md#api-VssNet_PublishStatus) |
| `DomainIf_Status_t DomainNetPriv_OnVssStatus(const NetVssStatus *message, const NetCanRxMeta *meta);` | Network 로컬 기존 경계 또는 명시된 내부 제안; typed 접수·투영·호출 | [함수 상세](../functions/can/VSS_STATUS.md#api-DomainNetPriv_OnVssStatus) |
| `DomainIf_Status_t DomainIf_RxVssState( const DomainIf_VssStateReport_t *state);` | DOMAIN 공개 본체/노드 App 소유; Network 호출 경계 | [함수 상세](../functions/can/VSS_STATUS.md#api-DomainIf_RxVssState) |
| `DomainIf_Status_t DomainIf_RxVssFault( const FaultInfo_t *fault);` | DOMAIN 공개 본체/노드 App 소유; Network 호출 경계 | [함수 상세](../functions/can/VSS_STATUS.md#api-DomainIf_RxVssFault) |
| `DomainIf_Status_t DomainIf_TxVssWarningState( const DomainIf_VssWarningState_t *state);` | DOMAIN 공개 본체/노드 App 소유; Network 호출 경계 | [함수 상세](../functions/can/VSS_WARNING_STATE.md#api-DomainIf_TxVssWarningState) |
| `void VssApp_OnDomainWarningState(const NetVssWarningState *message, const NetCanRxMeta *meta);` | DOMAIN 공개 본체/노드 App 소유; Network 호출 경계 | [함수 상세](../functions/can/VSS_WARNING_STATE.md#api-VssApp_OnDomainWarningState) |
| `DomainIf_Status_t NetCan_TxVssWarningState(const DomainIf_VssWarningState_t *state);` | Network 로컬 기존 경계 또는 명시된 내부 제안; typed 접수·투영·호출 | [함수 상세](../functions/can/VSS_WARNING_STATE.md) |
| `NetCanResult NetCan_DeliverVssWarningState(const NetVssWarningState *message, const NetCanRxMeta *meta);` | Network 로컬 기존 경계 또는 명시된 내부 제안; typed 접수·투영·호출 | [함수 상세](../functions/can/VSS_WARNING_STATE.md#receive) |
| `DomainIf_Status_t DomainIf_TxWindowCommand( const DomainIf_WindowCommand_t *command);` | DOMAIN 공개 본체/노드 App 소유; Network 호출 경계 | [함수 상세](../functions/can/WINDOW_COMMAND.md#api-DomainIf_TxWindowCommand) |
| `void WindowApp_OnDomainCommand(const NetWindowCommand *message, const NetCanRxMeta *meta);` | DOMAIN 공개 본체/노드 App 소유; Network 호출 경계 | [함수 상세](../functions/can/WINDOW_COMMAND.md#api-WindowApp_OnDomainCommand) |
| `DomainIf_Status_t NetCan_TxWindowCommand(const DomainIf_WindowCommand_t *command);` | Network 로컬 기존 경계 또는 명시된 내부 제안; typed 접수·투영·호출 | [함수 상세](../functions/can/WINDOW_COMMAND.md) |
| `NetCanResult NetCan_DeliverWindowCommand(const NetWindowCommand *message, const NetCanRxMeta *meta);` | Network 로컬 기존 경계 또는 명시된 내부 제안; typed 접수·투영·호출 | [함수 상세](../functions/can/WINDOW_COMMAND.md#receive) |
| `NetSubmitResult WindowNet_SendEvent(const NetWindowEvent *message);` | Network 로컬 기존 경계 또는 명시된 내부 제안; typed 접수·투영·호출 | [함수 상세](../functions/can/WINDOW_EVENT.md#api-WindowNet_SendEvent) |
| `DomainIf_Status_t DomainNetPriv_OnWindowEvent(const NetWindowEvent *message, const NetCanRxMeta *meta);` | Network 로컬 기존 경계 또는 명시된 내부 제안; typed 접수·투영·호출 | [함수 상세](../functions/can/WINDOW_EVENT.md#api-DomainNetPriv_OnWindowEvent) |
| `DomainIf_Status_t DomainIf_RxWindowAntiPinch( const DomainIf_AntiPinchReport_t *report);` | DOMAIN 공개 본체/노드 App 소유; Network 호출 경계 | [함수 상세](../functions/can/WINDOW_EVENT.md#api-DomainIf_RxWindowAntiPinch) |
| `NetSubmitResult WindowNet_PublishFault(const NetWindowFault *message);` | Network 로컬 기존 경계 또는 명시된 내부 제안; typed 접수·투영·호출 | [함수 상세](../functions/can/WINDOW_FAULT.md#api-WindowNet_PublishFault) |
| `DomainIf_Status_t DomainNetPriv_OnWindowFault(const NetWindowFault *message, const NetCanRxMeta *meta);` | Network 로컬 기존 경계 또는 명시된 내부 제안; typed 접수·투영·호출 | [함수 상세](../functions/can/WINDOW_FAULT.md#api-DomainNetPriv_OnWindowFault) |
| `DomainIf_Status_t DomainIf_RxWindowFault( const FaultInfo_t *fault);` | DOMAIN 공개 본체/노드 App 소유; Network 호출 경계 | [함수 상세](../functions/can/WINDOW_FAULT.md#api-DomainIf_RxWindowFault) |
| `NetSubmitResult WindowNet_SendResult(const NetWindowResult *message);` | Network 로컬 기존 경계 또는 명시된 내부 제안; typed 접수·투영·호출 | [함수 상세](../functions/can/WINDOW_RESULT.md#api-WindowNet_SendResult) |
| `DomainIf_Status_t DomainIf_RxWindowCommandResult( const DomainIf_WindowCommandResult_t *result);` | DOMAIN 공개 본체/노드 App 소유; Network 호출 경계 | [함수 상세](../functions/can/WINDOW_RESULT.md#api-DomainIf_RxWindowCommandResult) |
| `NetCanResult NetCan_DeliverWindowResult(const NetWindowResult *message, const NetCanRxMeta *meta);` | Network 로컬 기존 경계 또는 명시된 내부 제안; typed 접수·투영·호출 | [함수 상세](../functions/can/WINDOW_RESULT.md#receive) |
| `NetSubmitResult WindowNet_PublishState(const NetWindowState *message);` | Network 로컬 기존 경계 또는 명시된 내부 제안; typed 접수·투영·호출 | [함수 상세](../functions/can/WINDOW_STATE.md#api-WindowNet_PublishState) |
| `DomainIf_Status_t DomainIf_RxWindowState( const WindowState_t *state);` | DOMAIN 공개 본체/노드 App 소유; Network 호출 경계 | [함수 상세](../functions/can/WINDOW_STATE.md#api-DomainIf_RxWindowState) |
| `NetCanResult NetCan_DeliverWindowState(const NetWindowState *message, const NetCanRxMeta *meta);` | Network 로컬 기존 경계 또는 명시된 내부 제안; typed 접수·투영·호출 | [함수 상세](../functions/can/WINDOW_STATE.md#receive) |

## UART-M 연결 — F06~F10

상향4 C2/private와17개 하향 submit/기존TxPort 경계를 각 메시지에 연결한다. API 본체·Manager·등록 지원은 원 Owner 소유다. Query 및 정기 상태를 COMMAND/REQUEST_RESULT/OCCURRENCE 후발 실패로 위조하지 않는다.

[23 실제 API](../05_UART_M_OVERVIEW.md#messages) · [Router](../functions/UART_M_ROUTER.md) · [접수/반환](../functions/UART_M_TX_RX_CONTROL.md) · [UART-M23](../05_UART_M_OVERVIEW.md#messages)

<a id="uart-c"></a>
## UART-C 추가 연결

F10은 공개 5개/private 1개의 기존 Pi/CIS 원형을 연결한다. 송신 API가 로컬 접수 OK를 반환하기 전에 전체 입력 사본을 확보한다. 수신 void callback 반환은 호출 사실만 나타낸다. DOMAIN C2 sink를 다른 ECU에 복제하지 않는다. [함수/흐름](../functions/UART_C_FRAME_ROUTER.md#dispatch) · [3종 API/Codec](../06_UART_C_OVERVIEW.md#messages) · [Data](../data/UART_C_DATA.md)

## DOMAIN Adapter 상세 탐색 — F06/F07/F08/F09

[등록·소비 준비](../functions/DOMAIN_REGISTRATION.md)는 기존 Init/IsInitialized/SetTxPort/SetRxObserver의 Owner와 호출 조건을 연결한다. [TxPort26](../functions/DOMAIN_TX_PORT.md)은 공개 Tx26에서 Network callback·사본·Queue·기존 Codec로 내려가는 경로를, [Rx Adapter](../functions/DOMAIN_RX_ADAPTER.md)는22 공개 Rx의 실제 입력·변환·반환·Observer 경계를 설명한다. 공개 함수 본체와 차량 판단은 DOMAIN 소유다.

[후발 실패·Health](../functions/NETWORK_FAILURE_HEALTH.md)에서 실제 관측과 원 identity에 맞는 실패만 기존 sink로 인계한다. Observer11의 구현·등록 소유는 해당 DOMAIN/기능 Owner 경계로 남긴다. [명령 송신](../flows/DOMAIN_COMMAND_TO_NETWORK.md)과 [수신 인계](../flows/NETWORK_TO_DOMAIN.md)에서 전체 호출 순서를 확인한다.

## PG6-SW01 — 같은 관측 제한과 선택 후보 타입

F08은 CIS_OCCUPANT의 새 관측과 [CURRENT_RESTRICTION](../functions/can/CIS_OCCUPANT.md#restriction-check)을 구분한다. 후자는 Pi 원 Boot/seq/값을 유지하고 검증된 현재 제한만 기존 Rx에 전달한다. 원 rc·Core 단계 불명확은 자동 재호출/rollback 없이 보존한다. 환경의 정상 frame 인입과 [로컬 사건 인입](../functions/can/CIS_ENVIRONMENT.md#local-invalidation)은 별개이며 후자는 아래 private 함수로 같은 기존 C2에 연결한다.

```c
NetCanResult NetCan_InvalidateCisEnvironment(const NetCisEnvironmentLocalEvent *event);
```

event는 [완전 Data 정의](../data/CAN_NODE_DATA.md#type-NetCisEnvironmentLocalEvent)를 따르는 호출 중 불변 IN이다. M09에서 실제 원 기록/시간/Boot 사건을 고정한 뒤 F08 직렬 문맥에서 호출한다. 미준비·시간 불명·부분 반영은 leaf의 반환/보존 규칙을 따른다. [r1의 환경 저장 순서](../functions/can/CIS_ENVIRONMENT.md#environment-storage-order)는 조건부로 작성되어 있다. M01의 구 미실행 입력 차단 뒤 실행 중 A 및 후속 비동기 쓰기가 다시 commit하지 못하는 실제 근거를 확인해야 무효화를 호출한다. owner 동기대기나 임의취소는 하지 않고 NOT_READY/CONTRACT_HOLD로 반환한다. 호출 후 VSM·독립 Meta의 실제 무효화 commit과 같은 전이 연결을 확인한 다음 B 원seq/누적AGE 인계를 연다. callback 반환/Queue/OK를 완료로 보지 않으며 Core 완료 뒤 Observer 실패만으로 재호출하지 않는다. 그 실제 기존 DOMAIN 경계가 없는 [BG-R05-BOOT](../functions/can/CIS_ENVIRONMENT.md#boot-binding-gap)는 유지한다.

F07/F08은 선택 v2 후보의 `ClimateState_t`12멤버·`InteriorLightState_t`11멤버를 `sourceGeneration`까지 전체 복사/변환한다. 원 생산 세대는 검증된 동일 원본 연결에서만 얻고 CAN BOOT 직접 대입·품질 저하 시0 초기화를 하지 않는다. 실제 제품 Header/ABI/Producer mapping 및 generation0 정책은 미검증이며 환경 DTO에는 필드를 추가하지 않는다. L5/M9/F25/ST12와 공개 함수 수는 유지한다.
