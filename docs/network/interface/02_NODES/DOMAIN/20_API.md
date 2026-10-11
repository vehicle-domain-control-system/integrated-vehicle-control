# DOMAIN — 기존 공개/소비 경계 API

**DECLARED_SOURCE / BINDING_OPEN.** 제공된 원 헤더에서 읽은 정확한 반환형·함수명·인자다. 선언 보존은 실제 Firmware 구현·연결·컴파일 완료를 의미하지 않는다.

[개요](00_OVERVIEW.md) · [Data](10_DATA.md) · [내부 Adapter](25_PRIVATE_ROUTING.md) · [메시지 48](../../05_MESSAGE_INDEX.md)

<a id="door-flow"></a>
## 도어 명령·실행 결과부터 찾기

| 읽는 순서 | 정확한 API·Data | 호출과 결과의 경계 |
|---|---|---|
| 1 · 명령 결정 | [TxBcmDoorCommand 원형·인자](#api-domainif_txbcmdoorcommand), [전체 Command](12_DOMAIN_TYPES.md#type-domainif-bcmdoorcommand-t), [원 CommandContext](12_DOMAIN_TYPES.md#type-domainif-commandcontext-t) | Domain 기능 owner가 원 commandId를 한 번 생성하고 목표를 정한다. |
| 2 · Network 제출 | [TxPort 등록](#api-domainif_settxport), [txBcmDoorCommand callback](25_PRIVATE_ROUTING.md#tx-port), [반환·복사 수명](#ports) | Network Adapter가 안정된 복사/encode 후 로컬 OK를 반환한다. 실제 BCM DONE은 별도다. |
| 3 · 실제 결과 수신 | [RxBcmCommandResult 원형·인자](#api-domainif_rxbcmcommandresult), [전체 결과·도어 근거](12_DOMAIN_TYPES.md#type-domainif-bcmcommandresult-t), [BCM_RESULT Wire·확인 제한](../../03_COMMUNICATION/10_CAN_FD.md#message-bcm-result) | BCM owner의 실행 결과를 원 commandId/kind에 연결한다. confirmation은 CAN에 없으며 확인 근거 연결은 PROPOSED/BINDING_OPEN이다. |

## 호출 경계

Application TX는 `DomainIf_Tx...()`를 호출한다. Network 수신은 Codec 검증과 typed 투영 후 `DomainIf_Rx...()`로 인계한다. `DomainIf_QueryGet...()` 18 종은 Domain 내부 snapshot Getter 이며 다른 ECU의 RPC가 아니다. TX Port26 종이 실제 Network Adapter로 이어진다. `M_QUERY_END`와 `M_DOMAIN_ALIVE`는 내부에 둔다.

## 원형 바로 찾기

아래 원형을 선택하면 바로 아래의 호출 주체·인자·수명·실패 설명으로 이동한다. **DECLARED_SOURCE / BINDING_OPEN**이며 Header의 실제 선언을 유지한다.

### 로컬 초기화·검증

| 정확한 반환형·함수·전체 인자 | 사용 설명 |
|---|---|
| `DomainIf_Status_t DomainIf_Init(void);` | [호출·결과·실패](#api-domainif_init) |
| `bool DomainIf_IsInitialized(void);` | [호출·결과·실패](#api-domainif_isinitialized) |
| `DomainIf_Status_t DomainIf_SetRxObserver( const DomainIf_RxObserver_t *observer);` | [호출·결과·실패](#api-domainif_setrxobserver) |
| `DomainIf_Status_t DomainIf_SetTxPort( const DomainIf_TxPort_t *port);` | [호출·결과·실패](#api-domainif_settxport) |
| `DomainIf_Status_t DomainIf_ValidatePassiveQueryContext( DeviceContextId_t deviceContextId, SessionId_t sessionId);` | [호출·결과·실패](#api-domainif_validatepassivequerycontext) |
| `DomainIf_Status_t DomainIf_GetNetworkHealthSnapshot( NetworkLinkId_t link, uint32_t nowMs, NetworkHealthSnapshot_t *outSnapshot);` | [호출·결과·실패](#api-domainif_getnetworkhealthsnapshot) |

### 수신: Network → C2

| 정확한 반환형·함수·전체 인자 | 사용 설명 |
|---|---|
| `DomainIf_Status_t DomainIf_RxDigitalKeyConnection( DeviceContextId_t deviceContextId, const DigitalKeyConnectionState_t *connection);` | [호출·결과·실패](#api-domainif_rxdigitalkeyconnection) |
| `DomainIf_Status_t DomainIf_RxProximity( DeviceContextId_t deviceContextId, const ProximityInput_t *proximity);` | [호출·결과·실패](#api-domainif_rxproximity) |
| `DomainIf_Status_t DomainIf_RxMobileRequest( const DomainIf_MobileRequest_t *request);` | [호출·결과·실패](#api-domainif_rxmobilerequest) |
| `DomainIf_Status_t DomainIf_RxWarningAck( const DomainIf_WarningAck_t *ack);` | [호출·결과·실패](#api-domainif_rxwarningack) |
| `DomainIf_Status_t DomainIf_RxBcmDoorState( const DoorState_t *state);` | [호출·결과·실패](#api-domainif_rxbcmdoorstate) |
| `DomainIf_Status_t DomainIf_RxBcmClimateState( const ClimateState_t *state);` | [호출·결과·실패](#api-domainif_rxbcmclimatestate) |
| `DomainIf_Status_t DomainIf_RxBcmInteriorLightState( const InteriorLightState_t *state);` | [호출·결과·실패](#api-domainif_rxbcminteriorlightstate) |
| `DomainIf_Status_t DomainIf_RxCisStatus( const DomainIf_CisStatusReport_t *status);` | [호출·결과·실패](#api-domainif_rxcisstatus) |
| `DomainIf_Status_t DomainIf_RxCisOccupant( const CisOccupantState_t *state);` | [호출·결과·실패](#api-domainif_rxcisoccupant) |
| `DomainIf_Status_t DomainIf_RxCisCabinEnvironment( const CisCabinEnvironmentState_t *state);` | [호출·결과·실패](#api-domainif_rxciscabinenvironment) |
| `DomainIf_Status_t DomainIf_RxCisRear( const CisRearState_t *state);` | [호출·결과·실패](#api-domainif_rxcisrear) |
| `DomainIf_Status_t DomainIf_RxCisFault( const FaultInfo_t *fault);` | [호출·결과·실패](#api-domainif_rxcisfault) |
| `DomainIf_Status_t DomainIf_RxWindowState( const WindowState_t *state);` | [호출·결과·실패](#api-domainif_rxwindowstate) |
| `DomainIf_Status_t DomainIf_RxWindowCommandResult( const DomainIf_WindowCommandResult_t *result);` | [호출·결과·실패](#api-domainif_rxwindowcommandresult) |
| `DomainIf_Status_t DomainIf_RxWindowAntiPinch( const DomainIf_AntiPinchReport_t *report);` | [호출·결과·실패](#api-domainif_rxwindowantipinch) |
| `DomainIf_Status_t DomainIf_RxWindowFault( const FaultInfo_t *fault);` | [호출·결과·실패](#api-domainif_rxwindowfault) |
| `DomainIf_Status_t DomainIf_RxVssState( const DomainIf_VssStateReport_t *state);` | [호출·결과·실패](#api-domainif_rxvssstate) |
| `DomainIf_Status_t DomainIf_RxVssFault( const FaultInfo_t *fault);` | [호출·결과·실패](#api-domainif_rxvssfault) |
| `DomainIf_Status_t DomainIf_RxBcmStatus( const DomainIf_BcmStatusReport_t *status);` | [호출·결과·실패](#api-domainif_rxbcmstatus) |
| `DomainIf_Status_t DomainIf_RxBcmCommandResult( const DomainIf_BcmCommandResult_t *result);` | [호출·결과·실패](#api-domainif_rxbcmcommandresult) |
| `DomainIf_Status_t DomainIf_RxBcmProtectionEvent( const DomainIf_BcmProtectionEvent_t *event);` | [호출·결과·실패](#api-domainif_rxbcmprotectionevent) |
| `DomainIf_Status_t DomainIf_RxNetworkLocalFailure(const NetworkLocalFailure_t *failure);` | [호출·결과·실패](#api-domainif_rxnetworklocalfailure) |

### 송신: Application → Network

| 정확한 반환형·함수·전체 인자 | 사용 설명 |
|---|---|
| `DomainIf_Status_t DomainIf_TxBcmDoorCommand( const DomainIf_BcmDoorCommand_t *command);` | [호출·결과·실패](#api-domainif_txbcmdoorcommand) |
| `DomainIf_Status_t DomainIf_TxBcmClimateCommand( const DomainIf_BcmClimateCommand_t *command);` | [호출·결과·실패](#api-domainif_txbcmclimatecommand) |
| `DomainIf_Status_t DomainIf_TxBcmInteriorLightCommand( const DomainIf_BcmInteriorLightCommand_t *command);` | [호출·결과·실패](#api-domainif_txbcminteriorlightcommand) |
| `DomainIf_Status_t DomainIf_TxCisPowerPermission( const DomainIf_CisPowerPermission_t *permission);` | [호출·결과·실패](#api-domainif_txcispowerpermission) |
| `DomainIf_Status_t DomainIf_TxWindowCommand( const DomainIf_WindowCommand_t *command);` | [호출·결과·실패](#api-domainif_txwindowcommand) |
| `DomainIf_Status_t DomainIf_TxWindowOperationPermission( const DomainIf_WindowOperationPermission_t *permission);` | [호출·결과·실패](#api-domainif_txwindowoperationpermission) |
| `DomainIf_Status_t DomainIf_TxWindowDomainAlive( const DomainIf_WindowDomainAlive_t *alive);` | [호출·결과·실패](#api-domainif_txwindowdomainalive) |
| `DomainIf_Status_t DomainIf_TxVssOneShotEvent( const DomainIf_VssOneShotEvent_t *event);` | [호출·결과·실패](#api-domainif_txvssoneshotevent) |
| `DomainIf_Status_t DomainIf_TxVssWarningState( const DomainIf_VssWarningState_t *state);` | [호출·결과·실패](#api-domainif_txvsswarningstate) |
| `DomainIf_Status_t DomainIf_TxMobileRequestResult( const DomainIf_MobileRequestResult_t *result);` | [호출·결과·실패](#api-domainif_txmobilerequestresult) |
| `DomainIf_Status_t DomainIf_TxMobileWarning( const DomainIf_WarningInfo_t *warning);` | [호출·결과·실패](#api-domainif_txmobilewarning) |
| `DomainIf_Status_t DomainIf_TxMobileFunctionAvailability( const DomainIf_FunctionAvailabilitySet_t *availability);` | [호출·결과·실패](#api-domainif_txmobilefunctionavailability) |
| `DomainIf_Status_t DomainIf_TxMobileDigitalKeyState( const DigitalKeyState_t *state);` | [호출·결과·실패](#api-domainif_txmobiledigitalkeystate) |
| `DomainIf_Status_t DomainIf_TxMobileDigitalKeyResult( const DomainIf_DigitalKeyResult_t *result);` | [호출·결과·실패](#api-domainif_txmobiledigitalkeyresult) |
| `DomainIf_Status_t DomainIf_TxMobileBcmDoorState(const DoorState_t *state);` | [호출·결과·실패](#api-domainif_txmobilebcmdoorstate) |
| `DomainIf_Status_t DomainIf_TxMobileBcmClimateState(const ClimateState_t *state);` | [호출·결과·실패](#api-domainif_txmobilebcmclimatestate) |
| `DomainIf_Status_t DomainIf_TxMobileBcmInteriorLightState(const InteriorLightState_t *state);` | [호출·결과·실패](#api-domainif_txmobilebcminteriorlightstate) |
| `DomainIf_Status_t DomainIf_TxMobileBcmStatus(const DomainIf_BcmStatusReport_t *state);` | [호출·결과·실패](#api-domainif_txmobilebcmstatus) |
| `DomainIf_Status_t DomainIf_TxMobileCisEnvironment(const CisCabinEnvironmentState_t *state);` | [호출·결과·실패](#api-domainif_txmobilecisenvironment) |
| `DomainIf_Status_t DomainIf_TxMobileCisOccupant(const CisOccupantState_t *state);` | [호출·결과·실패](#api-domainif_txmobilecisoccupant) |
| `DomainIf_Status_t DomainIf_TxMobileCisRear(const CisRearState_t *state);` | [호출·결과·실패](#api-domainif_txmobilecisrear) |
| `DomainIf_Status_t DomainIf_TxMobileCisStatus(const DomainIf_CisStatusReport_t *state);` | [호출·결과·실패](#api-domainif_txmobilecisstatus) |
| `DomainIf_Status_t DomainIf_TxMobileVssStatus(const DomainIf_VssStateReport_t *state);` | [호출·결과·실패](#api-domainif_txmobilevssstatus) |
| `DomainIf_Status_t DomainIf_TxMobileWindowState(const WindowState_t *state);` | [호출·결과·실패](#api-domainif_txmobilewindowstate) |
| `DomainIf_Status_t DomainIf_TxMobileWindowFault(const DomainIf_WindowFaultState_t *state);` | [호출·결과·실패](#api-domainif_txmobilewindowfault) |
| `DomainIf_Status_t DomainIf_TxMobileUserSettings( const DomainIf_UserSettingsState_t *settings);` | [호출·결과·실패](#api-domainif_txmobileusersettings) |

### 조회: Network → Domain snapshot

| 정확한 반환형·함수·전체 인자 | 사용 설명 |
|---|---|
| `DomainIf_Status_t DomainIf_QueryGetBcmDoorState(uint32_t nowMs, DoorState_t *outState);` | [호출·결과·실패](#api-domainif_querygetbcmdoorstate) |
| `DomainIf_Status_t DomainIf_QueryGetBcmClimateState(uint32_t nowMs, ClimateState_t *outState);` | [호출·결과·실패](#api-domainif_querygetbcmclimatestate) |
| `DomainIf_Status_t DomainIf_QueryGetBcmInteriorLightState(uint32_t nowMs, InteriorLightState_t *outState);` | [호출·결과·실패](#api-domainif_querygetbcminteriorlightstate) |
| `DomainIf_Status_t DomainIf_QueryGetBcmStatus(uint32_t nowMs, DomainIf_BcmStatusReport_t *outState);` | [호출·결과·실패](#api-domainif_querygetbcmstatus) |
| `DomainIf_Status_t DomainIf_QueryGetCisEnvironment(uint32_t nowMs, CisCabinEnvironmentState_t *outState);` | [호출·결과·실패](#api-domainif_querygetcisenvironment) |
| `DomainIf_Status_t DomainIf_QueryGetCisOccupant(uint32_t nowMs, CisOccupantState_t *outState);` | [호출·결과·실패](#api-domainif_querygetcisoccupant) |
| `DomainIf_Status_t DomainIf_QueryGetCisRear(uint32_t nowMs, CisRearState_t *outState);` | [호출·결과·실패](#api-domainif_querygetcisrear) |
| `DomainIf_Status_t DomainIf_QueryGetCisStatus(uint32_t nowMs, DomainIf_CisStatusReport_t *outState);` | [호출·결과·실패](#api-domainif_querygetcisstatus) |
| `DomainIf_Status_t DomainIf_QueryGetVssStatus(uint32_t nowMs, DomainIf_VssStateReport_t *outState);` | [호출·결과·실패](#api-domainif_querygetvssstatus) |
| `DomainIf_Status_t DomainIf_QueryGetWindowState(uint32_t nowMs, WindowState_t *outState);` | [호출·결과·실패](#api-domainif_querygetwindowstate) |
| `DomainIf_Status_t DomainIf_QueryGetWindowFault(uint32_t nowMs, DomainIf_WindowFaultState_t *outState);` | [호출·결과·실패](#api-domainif_querygetwindowfault) |
| `DomainIf_Status_t DomainIf_QueryGetUserSettings( uint32_t nowMs, DomainIf_UserSettingsState_t *outSettings);` | [호출·결과·실패](#api-domainif_querygetusersettings) |
| `DomainIf_Status_t DomainIf_QueryGetFunctionAvailability( uint32_t nowMs, DomainIf_FunctionAvailabilitySet_t *outAvailability);` | [호출·결과·실패](#api-domainif_querygetfunctionavailability) |
| `DomainIf_Status_t DomainIf_QueryGetDigitalKeyState( uint32_t nowMs, DigitalKeyState_t *outState);` | [호출·결과·실패](#api-domainif_querygetdigitalkeystate) |
| `DomainIf_Status_t DomainIf_QueryGetCurrentWarning( DomainWarningType_t type, uint32_t nowMs, DomainIf_WarningInfo_t *outWarning);` | [호출·결과·실패](#api-domainif_querygetcurrentwarning) |
| `DomainIf_Status_t DomainIf_QueryGetRequestResult( DeviceContextId_t requestingDevice, const RequestContext_t *targetRequest, uint32_t nowMs, DomainIf_MobileRequestResult_t *outResult, bool *outHasRecord);` | [호출·결과·실패](#api-domainif_querygetrequestresult) |
| `DomainIf_Status_t DomainIf_QueryGetRecentRequestResults( DeviceContextId_t requestingDevice, uint32_t nowMs, DomainIf_MobileRequestResult_t *outResults, size_t capacity, size_t *outCount);` | [호출·결과·실패](#api-domainif_querygetrecentrequestresults) |
| `DomainIf_Status_t DomainIf_QueryGetLastDigitalKeyResult( DeviceContextId_t requestingDevice, uint32_t nowMs, DomainIf_DigitalKeyResult_t *outResult, bool *outHasRecord);` | [호출·결과·실패](#api-domainif_querygetlastdigitalkeyresult) |

<a id="ports"></a>
## 공통 호출·반환·수명 계약

C2는 **Task/owner 처리 문맥에서 비차단 호출**한다. ISR 에서 직접 호출하지 않는다. RX const 객체는 반환 때까지 borrowed 다. TX는 local OK 전에 Network 소유 bounded 값복사/deep copy 또는 동기 encode를 확보하고 송신 직전 AGE/CRC를 갱신한다. caller pointer 보관은 금지한다. Core commit은 optional RxObserver 보다 먼저이며 observer 실패는 commit 재실행 근거가 아니다.

| source 상태 | 뜻·처리 |
|---|---|
| `DOMAIN_IF_OK` | 로컬 수용/출력 성공. 차량 DONE 아님. |
| `DOMAIN_IF_INVALID_ARGUMENT` | NULL/enum/필드 조합/범위 오류. 같은 잘못된 값 무조건 재시도 금지. |
| `DOMAIN_IF_NOT_INITIALIZED` | C2 로컬 초기화 전. 공개 헤더 source 전용 상태. |
| `DOMAIN_IF_NOT_AVAILABLE` | Port/현재 링크·문맥·필수 failure sink 등 이 인계를 수행할 조건이 없음. |
| `DOMAIN_IF_QUEUE_FULL` | bounded capacity 부족. 개별 Event/Result 덮어쓰기 금지. |
| `DOMAIN_IF_REJECTED` | C2 의미상 거부. Network local submit의 4 상태와 같지 않다. |
| `DOMAIN_IF_UNSUPPORTED` | 지원하지 않는 기능/링크/형식. WINDOW는 [구현 보류 — 설계 유지]. |

### Network local submit → C2 TxPort 반환 변환

| Network 내부 상태 | C2 callback 반환 | 변환 조건 |
|---|---|---|
| `NET_SUBMIT_OK=0` | `DOMAIN_IF_OK` | 안정된 복사/동기 encode 성공한 경우만. |
| `NET_SUBMIT_INVALID_ARGUMENT=1` | `DOMAIN_IF_INVALID_ARGUMENT` | 잘못된 인자/불법 코드/표현 불가 schema. |
| `NET_SUBMIT_NOT_READY=2` | `DOMAIN_IF_NOT_AVAILABLE` | 현재 링크·대상 Boot·Session·binding 준비 안 됨. |
| `NET_SUBMIT_NO_CAPACITY=3` | `DOMAIN_IF_QUEUE_FULL` | bounded 큐/보관 공간 부족. |
| 등록 전 C2 검사 | `NOT_INITIALIZED/REJECTED/UNSUPPORTED` | Network4 상태로 cast 하지 않고 C2 원 검사 결과 유지. |
| 알 수 없는 내부 반환값 | `DOMAIN_IF_NOT_AVAILABLE` | fail-closed 제안. 진단 근거 보존, 성공이나 차량 FAILED 합성 금지. |

**PROPOSED_DESIGN:** 반환 mapping은 명시 switch 형태의 계약이며 코드 구현이 아니다. enum 수치 cast는 금지. 필요한 local-failure identity sink가 없으면 correlated async failure를 지원한다고 표시하지 않는다.

<a id="query-getters"></a>
## Getter 공통 사용·품질·실패

**snapshot 계약:** 기존 owner getter의 현재 cache를 caller OUT에 투영한다. `ValidatePassiveQueryContext()`를 같은 직렬 owner 문맥에서 먼저 적용한다. Getter 성공은 source quality=OK/새 관측/원 ECU RPC 완료를 뜻하지 않는다. 기록 존재 OUT을 가진 Getter만 `outHasRecord=false`를 사용한다. 그 외 Getter는 typed 품질의 stale/no data/부분 결과를 보존한다. 반복 송신으로 updateSequence/occurrence/원 AGE를 새로 만들지 않는다.

Getter 실패 시 OUT은 undefined이며 읽지 않는다. `outHasRecord=false`는 성공한 해당 기록 Getter에서만 확인된 기록 부재를 뜻한다. Query의 실제 응답 TX 완료·부분/0건 종결은 [Network Query 규칙](25_PRIVATE_ROUTING.md#r2-query-producer)을 별도로 적용한다.

<a id="api-domainif_init"></a>
## `DomainIf_Init()`

```c
DomainIf_Status_t DomainIf_Init(void);
```

**방향:** 로컬 관리/검증 · **지위:** DECLARED_SOURCE / BINDING_OPEN.

**용도·호출자·시점:** Domain 초기화 owner가 C2 논리 경계를 초기화한다. CAN/UART 또는 peer 준비 완료를 선언하는 호출이 아니다.

**반환·호출:** 로컬 snapshot/검증/관리 결과. OK의 범위와 실패 상태는 [공통 호출 계약](#ports)을 따른다.

<a id="api-domainif_isinitialized"></a>
## `DomainIf_IsInitialized()`

```c
bool DomainIf_IsInitialized(void);
```

**방향:** 로컬 관리/검증 · **지위:** DECLARED_SOURCE / BINDING_OPEN.

**용도·호출자·시점:** 로컬 호출자가 C2 초기화 여부를 검사한다. 원 ECU 통신·차량 기능 가용성과 구분한다.

**반환·호출:** 로컬 초기화 여부만 반환. 통신/차량 준비 완료를 뜻하지 않는다.

<a id="api-domainif_setrxobserver"></a>
## `DomainIf_SetRxObserver()`

```c
DomainIf_Status_t DomainIf_SetRxObserver( const DomainIf_RxObserver_t *observer);
```

**방향:** 로컬 관리/검증 · **지위:** DECLARED_SOURCE / BINDING_OPEN.

**용도·호출자·시점:** Domain 통합 owner가 기존 선택적 observer table을 등록한다. C2 core commit 뒤 관찰 전달이며 observer가 정책/상태 저장 owner를 대체하지 않는다.

| 인자 | 정확한 타입 | IN/OUT | 사용·수명 |
|---|---|---|---|
| `observer` | [`const DomainIf_RxObserver_t *`](12_DOMAIN_TYPES.md#type-domainif-rxobserver-t) | IN / borrowed | 기존 typed callback table. table은 값복사; Core commit 후 optional observer 전달. observer 실패 때문에 commit/구동 재실행 금지. |

**반환·호출:** 로컬 snapshot/검증/관리 결과. OK의 범위와 실패 상태는 [공통 호출 계약](#ports)을 따른다.

<a id="api-domainif_settxport"></a>
## `DomainIf_SetTxPort()`

```c
DomainIf_Status_t DomainIf_SetTxPort( const DomainIf_TxPort_t *port);
```

**방향:** 로컬 관리/검증 · **지위:** DECLARED_SOURCE / BINDING_OPEN.

**용도·호출자·시점:** Network Adapter 등록 owner가 typed callback26 종을 등록한다. 기능 TX는 이 table을 통해 실제 Network local admission으로 이어진다.

| 인자 | 정확한 타입 | IN/OUT | 사용·수명 |
|---|---|---|---|
| `port` | [`const DomainIf_TxPort_t *`](12_DOMAIN_TYPES.md#type-domainif-txport-t) | IN / borrowed | 기존 typed TX callback26 종 table. table은 값복사, 미연결은 NOT_AVAILABLE. 비동기 TX 수용 시 모든 borrowed 인자 안정된 복사 필요. |

**반환·호출:** 로컬 snapshot/검증/관리 결과. OK의 범위와 실패 상태는 [공통 호출 계약](#ports)을 따른다.

<a id="api-domainif_rxdigitalkeyconnection"></a>
## `DomainIf_RxDigitalKeyConnection()`

```c
DomainIf_Status_t DomainIf_RxDigitalKeyConnection( DeviceContextId_t deviceContextId, const DigitalKeyConnectionState_t *connection);
```

**방향:** Network→C2 · **지위:** DECLARED_SOURCE / BINDING_OPEN.

**용도·호출자·시점:** DOMAIN UART-M Codec가 M_CONTEXT의 등록·연결 공동 tuple와 App 활성 독립 tuple를 DeviceContext와 함께 C2로 인계한다. 근접 정보는 별도 RxProximity 호출이다.

| 인자 | 정확한 타입 | IN/OUT | 사용·수명 |
|---|---|---|---|
| `deviceContextId` | [`DeviceContextId_t`](11_VEHICLE_TYPES.md#type-devicecontextid-t) | IN / value | 검증된 단말 u32. 현재 Gateway peer/session binding과 일치해야 함. |
| `connection` | [`const DigitalKeyConnectionState_t *`](11_VEHICLE_TYPES.md#type-digitalkeyconnectionstate-t) | IN / borrowed | 호출 동안 유효한 `DigitalKeyConnectionState_t` 객체. 아래 용도와 연결된 원 owner의 M_CONTEXT 의미를 제공한다. [전체 Data](10_DATA.md)의 필드별 품질/식별/AGE를 적용한다. |

**반환·호출:** 로컬 C2 입력 처리 결과. Network 수신 및 Core commit/observer 계약은 [공통 호출 계약](#ports)을 따른다.

**메시지:** `M_CONTEXT` — [송수신 색인](../../05_MESSAGE_INDEX.md). 원 wire 객체를 C2/Gateway App 객체로 raw cast 하지 않고 명시 decode/projection 후 호출한다.

<a id="api-domainif_rxproximity"></a>
## `DomainIf_RxProximity()`

```c
DomainIf_Status_t DomainIf_RxProximity( DeviceContextId_t deviceContextId, const ProximityInput_t *proximity);
```

**방향:** Network→C2 · **지위:** DECLARED_SOURCE / BINDING_OPEN.

**용도·호출자·시점:** DOMAIN UART-M Codec가 M_CONTEXT의 근접 상태·독립 품질/원 AGE/평가번호를 검증한 뒤 DigitalKey 입력 owner에 인계한다. INVALID/STALE를 FAR로 만들지 않는다.

| 인자 | 정확한 타입 | IN/OUT | 사용·수명 |
|---|---|---|---|
| `deviceContextId` | [`DeviceContextId_t`](11_VEHICLE_TYPES.md#type-devicecontextid-t) | IN / value | 검증된 단말 u32. 현재 Gateway peer/session binding과 일치해야 함. |
| `proximity` | [`const ProximityInput_t *`](11_VEHICLE_TYPES.md#type-proximityinput-t) | IN / borrowed | 호출 동안 유효한 `ProximityInput_t` 객체. 아래 용도와 연결된 원 owner의 M_CONTEXT 의미를 제공한다. [전체 Data](10_DATA.md)의 필드별 품질/식별/AGE를 적용한다. |

**반환·호출:** 로컬 C2 입력 처리 결과. Network 수신 및 Core commit/observer 계약은 [공통 호출 계약](#ports)을 따른다.

**메시지:** `M_CONTEXT` — [송수신 색인](../../05_MESSAGE_INDEX.md). 원 wire 객체를 C2/Gateway App 객체로 raw cast 하지 않고 명시 decode/projection 후 호출한다.

<a id="api-domainif_rxmobilerequest"></a>
## `DomainIf_RxMobileRequest()`

```c
DomainIf_Status_t DomainIf_RxMobileRequest( const DomainIf_MobileRequest_t *request);
```

**방향:** Network→C2 · **지위:** DECLARED_SOURCE / BINDING_OPEN.

**용도·호출자·시점:** DOMAIN UART-M Codec가 검증된 단말/현재 Session의 M_REQUEST를 8 종 source Request union으로 decode 해 Domain 수용 owner에 전달한다. Gateway local OK는 차량 ACCEPTED가 아니다.

| 인자 | 정확한 타입 | IN/OUT | 사용·수명 |
|---|---|---|---|
| `request` | [`const DomainIf_MobileRequest_t *`](12_DOMAIN_TYPES.md#type-domainif-mobilerequest-t) | IN / borrowed | 호출 동안 유효한 `DomainIf_MobileRequest_t` 객체. 아래 용도와 연결된 원 owner의 M_REQUEST 의미를 제공한다. [전체 Data](10_DATA.md)의 필드별 품질/식별/AGE를 적용한다. |

**반환·호출:** 로컬 C2 입력 처리 결과. Network 수신 및 Core commit/observer 계약은 [공통 호출 계약](#ports)을 따른다.

**메시지:** `M_REQUEST` — [송수신 색인](../../05_MESSAGE_INDEX.md). 원 wire 객체를 C2/Gateway App 객체로 raw cast 하지 않고 명시 decode/projection 후 호출한다.

<a id="api-domainif_rxwarningack"></a>
## `DomainIf_RxWarningAck()`

```c
DomainIf_Status_t DomainIf_RxWarningAck( const DomainIf_WarningAck_t *ack);
```

**방향:** Network→C2 · **지위:** DECLARED_SOURCE / BINDING_OPEN.

**용도·호출자·시점:** DOMAIN UART-M Codec가 M_WARNING_ACK의 경고 종류를 역변환하고 원 Domain Boot/Occurrence/단말 Session을 확인한 뒤 읽음 owner에 인계한다. 읽음은 위험 CLEAR가 아니다.

| 인자 | 정확한 타입 | IN/OUT | 사용·수명 |
|---|---|---|---|
| `ack` | [`const DomainIf_WarningAck_t *`](12_DOMAIN_TYPES.md#type-domainif-warningack-t) | IN / borrowed | 호출 동안 유효한 `DomainIf_WarningAck_t` 객체. 아래 용도와 연결된 원 owner의 M_WARNING_ACK 의미를 제공한다. [전체 Data](10_DATA.md)의 필드별 품질/식별/AGE를 적용한다. |

**반환·호출:** 로컬 C2 입력 처리 결과. Network 수신 및 Core commit/observer 계약은 [공통 호출 계약](#ports)을 따른다.

**메시지:** `M_WARNING_ACK` — [송수신 색인](../../05_MESSAGE_INDEX.md). 원 wire 객체를 C2/Gateway App 객체로 raw cast 하지 않고 명시 decode/projection 후 호출한다.

<a id="api-domainif_rxbcmdoorstate"></a>
## `DomainIf_RxBcmDoorState()`

```c
DomainIf_Status_t DomainIf_RxBcmDoorState( const DoorState_t *state);
```

**방향:** Network→C2 · **지위:** DECLARED_SOURCE / BINDING_OPEN.

**용도·호출자·시점:** DOMAIN CAN Codec가 BCM_DOOR_STATE의 실제 잠금/개폐/조합·독립 품질/원 AGE를 C2 도어 담당 모듈 로 전달한다. 목표 Command로 실제 상태를 합성하지 않는다.

| 인자 | 정확한 타입 | IN/OUT | 사용·수명 |
|---|---|---|---|
| `state` | [`const DoorState_t *`](11_VEHICLE_TYPES.md#type-doorstate-t) | IN / borrowed | 호출 동안 유효한 `DoorState_t` 객체. 아래 용도와 연결된 원 owner의 BCM_DOOR_STATE 의미를 제공한다. [전체 Data](10_DATA.md)의 필드별 품질/식별/AGE를 적용한다. |

**반환·호출:** 로컬 C2 입력 처리 결과. Network 수신 및 Core commit/observer 계약은 [공통 호출 계약](#ports)을 따른다.

**메시지:** `BCM_DOOR_STATE` — [송수신 색인](../../05_MESSAGE_INDEX.md). 원 wire 객체를 C2/Gateway App 객체로 raw cast 하지 않고 명시 decode/projection 후 호출한다.

<a id="api-domainif_rxbcmclimatestate"></a>
## `DomainIf_RxBcmClimateState()`

```c
DomainIf_Status_t DomainIf_RxBcmClimateState( const ClimateState_t *state);
```

**방향:** Network→C2 · **지위:** DECLARED_SOURCE / BINDING_OPEN.

**용도·호출자·시점:** DOMAIN CAN Codec가 BCM_CLIMATE_STATE의 지시팬/측정팬·열방향/출력·잔열제거와 각각의 품질을 C2 공조 담당 모듈 에 전달한다.

| 인자 | 정확한 타입 | IN/OUT | 사용·수명 |
|---|---|---|---|
| `state` | [`const ClimateState_t *`](11_VEHICLE_TYPES.md#type-climatestate-t) | IN / borrowed | 호출 동안 유효한 `ClimateState_t` 객체. 아래 용도와 연결된 원 owner의 BCM_CLIMATE_STATE 의미를 제공한다. [전체 Data](10_DATA.md)의 필드별 품질/식별/AGE를 적용한다. |

**반환·호출:** 로컬 C2 입력 처리 결과. Network 수신 및 Core commit/observer 계약은 [공통 호출 계약](#ports)을 따른다.

**메시지:** `BCM_CLIMATE_STATE` — [송수신 색인](../../05_MESSAGE_INDEX.md). 원 wire 객체를 C2/Gateway App 객체로 raw cast 하지 않고 명시 decode/projection 후 호출한다.

<a id="api-domainif_rxbcminteriorlightstate"></a>
## `DomainIf_RxBcmInteriorLightState()`

```c
DomainIf_Status_t DomainIf_RxBcmInteriorLightState( const InteriorLightState_t *state);
```

**방향:** Network→C2 · **지위:** DECLARED_SOURCE / BINDING_OPEN.

**용도·호출자·시점:** DOMAIN CAN Codec가 BCM_LIGHT_STATE의 지시 적용/실제 feedback 지원·색/밝기/종류 독립 품질을 C2 실내 조명 담당 모듈 에 전달한다.

| 인자 | 정확한 타입 | IN/OUT | 사용·수명 |
|---|---|---|---|
| `state` | [`const InteriorLightState_t *`](11_VEHICLE_TYPES.md#type-interiorlightstate-t) | IN / borrowed | 호출 동안 유효한 `InteriorLightState_t` 객체. 아래 용도와 연결된 원 owner의 BCM_LIGHT_STATE 의미를 제공한다. [전체 Data](10_DATA.md)의 필드별 품질/식별/AGE를 적용한다. |

**반환·호출:** 로컬 C2 입력 처리 결과. Network 수신 및 Core commit/observer 계약은 [공통 호출 계약](#ports)을 따른다.

**메시지:** `BCM_LIGHT_STATE` — [송수신 색인](../../05_MESSAGE_INDEX.md). 원 wire 객체를 C2/Gateway App 객체로 raw cast 하지 않고 명시 decode/projection 후 호출한다.

<a id="api-domainif_rxcisstatus"></a>
## `DomainIf_RxCisStatus()`

```c
DomainIf_Status_t DomainIf_RxCisStatus( const DomainIf_CisStatusReport_t *status);
```

**방향:** Network→C2 · **지위:** DECLARED_SOURCE / BINDING_OPEN.

**용도·호출자·시점:** DOMAIN CAN Codec가 CIS_STATUS의 기능별 준비/고장·UART/CAN 링크근거·원 발생정보를 C2로 인계한다. Pi 상태만 보고 CIS 전체 정상을 합성하지 않는다.

| 인자 | 정확한 타입 | IN/OUT | 사용·수명 |
|---|---|---|---|
| `status` | [`const DomainIf_CisStatusReport_t *`](12_DOMAIN_TYPES.md#type-domainif-cisstatusreport-t) | IN / borrowed | 호출 동안 유효한 `DomainIf_CisStatusReport_t` 객체. 아래 용도와 연결된 원 owner의 CIS_STATUS 의미를 제공한다. [전체 Data](10_DATA.md)의 필드별 품질/식별/AGE를 적용한다. |

**반환·호출:** 로컬 C2 입력 처리 결과. Network 수신 및 Core commit/observer 계약은 [공통 호출 계약](#ports)을 따른다.

**메시지:** `CIS_STATUS` — [송수신 색인](../../05_MESSAGE_INDEX.md). 원 wire 객체를 C2/Gateway App 객체로 raw cast 하지 않고 명시 decode/projection 후 호출한다.

<a id="api-domainif_rxcisoccupant"></a>
## `DomainIf_RxCisOccupant()`

```c
DomainIf_Status_t DomainIf_RxCisOccupant( const CisOccupantState_t *state);
```

**방향:** Network→C2 · **지위:** DECLARED_SOURCE / BINDING_OPEN.

**용도·호출자·시점:** DOMAIN CAN Codec가 CIS_OCCUPANT의 Pi 원 Boot/판정번호/원 AGE·존재/인원수 독립 품질을 C2로 인계한다. 추가 CIS 품질 합성규칙이 확정되지 않으면 source 정보손실을 숨기지 않는다.

| 인자 | 정확한 타입 | IN/OUT | 사용·수명 |
|---|---|---|---|
| `state` | [`const CisOccupantState_t *`](11_VEHICLE_TYPES.md#type-cisoccupantstate-t) | IN / borrowed | 호출 동안 유효한 `CisOccupantState_t` 객체. 아래 용도와 연결된 원 owner의 CIS_OCCUPANT 의미를 제공한다. [전체 Data](10_DATA.md)의 필드별 품질/식별/AGE를 적용한다. |

**반환·호출:** 로컬 C2 입력 처리 결과. Network 수신 및 Core commit/observer 계약은 [공통 호출 계약](#ports)을 따른다.

**메시지:** `CIS_OCCUPANT` — [송수신 색인](../../05_MESSAGE_INDEX.md). 원 wire 객체를 C2/Gateway App 객체로 raw cast 하지 않고 명시 decode/projection 후 호출한다.

<a id="api-domainif_rxciscabinenvironment"></a>
## `DomainIf_RxCisCabinEnvironment()`

```c
DomainIf_Status_t DomainIf_RxCisCabinEnvironment( const CisCabinEnvironmentState_t *state);
```

**방향:** Network→C2 · **지위:** DECLARED_SOURCE / BINDING_OPEN.

**용도·호출자·시점:** DOMAIN CAN Codec가 CIS_ENVIRONMENT의 온도/습도/실내조도와 각기독립 validity/reason/AGE/sequence를 C2 환경 owner에 전달한다.

| 인자 | 정확한 타입 | IN/OUT | 사용·수명 |
|---|---|---|---|
| `state` | [`const CisCabinEnvironmentState_t *`](11_VEHICLE_TYPES.md#type-ciscabinenvironmentstate-t) | IN / borrowed | 호출 동안 유효한 `CisCabinEnvironmentState_t` 객체. 아래 용도와 연결된 원 owner의 CIS_ENVIRONMENT 의미를 제공한다. [전체 Data](10_DATA.md)의 필드별 품질/식별/AGE를 적용한다. |

**반환·호출:** 로컬 C2 입력 처리 결과. Network 수신 및 Core commit/observer 계약은 [공통 호출 계약](#ports)을 따른다.

**메시지:** `CIS_ENVIRONMENT` — [송수신 색인](../../05_MESSAGE_INDEX.md). 원 wire 객체를 C2/Gateway App 객체로 raw cast 하지 않고 명시 decode/projection 후 호출한다.

<a id="api-domainif_rxcisrear"></a>
## `DomainIf_RxCisRear()`

```c
DomainIf_Status_t DomainIf_RxCisRear( const CisRearState_t *state);
```

**방향:** Network→C2 · **지위:** DECLARED_SOURCE / BINDING_OPEN.

**용도·호출자·시점:** DOMAIN CAN Codec가 CIS_REAR의 실제 초음파거리/측정상태/원 품질·AGE·세대를 C2 위험판정 owner에 전달한다. VSS가 이거리를 직접 위험재판정하지 않는다.

| 인자 | 정확한 타입 | IN/OUT | 사용·수명 |
|---|---|---|---|
| `state` | [`const CisRearState_t *`](11_VEHICLE_TYPES.md#type-cisrearstate-t) | IN / borrowed | 호출 동안 유효한 `CisRearState_t` 객체. 아래 용도와 연결된 원 owner의 CIS_REAR 의미를 제공한다. [전체 Data](10_DATA.md)의 필드별 품질/식별/AGE를 적용한다. |

**반환·호출:** 로컬 C2 입력 처리 결과. Network 수신 및 Core commit/observer 계약은 [공통 호출 계약](#ports)을 따른다.

**메시지:** `CIS_REAR` — [송수신 색인](../../05_MESSAGE_INDEX.md). 원 wire 객체를 C2/Gateway App 객체로 raw cast 하지 않고 명시 decode/projection 후 호출한다.

<a id="api-domainif_rxcisfault"></a>
## `DomainIf_RxCisFault()`

```c
DomainIf_Status_t DomainIf_RxCisFault( const FaultInfo_t *fault);
```

**방향:** Network→C2 · **지위:** DECLARED_SOURCE / BINDING_OPEN.

**용도·호출자·시점:** DOMAIN 수신 adapter가 CIS_STATUS의 고장 부분을 FaultInfo로 명시투영해 기존 고장 owner에 전달한다. occurrence/AGE/세대 원 Evidence는 private 수신 문맥에서 별도보존한다.

| 인자 | 정확한 타입 | IN/OUT | 사용·수명 |
|---|---|---|---|
| `fault` | [`const FaultInfo_t *`](11_VEHICLE_TYPES.md#type-faultinfo-t) | IN / borrowed | 호출 동안 유효한 `FaultInfo_t` 객체. 아래 용도와 연결된 원 owner의 CIS_STATUS 의미를 제공한다. [전체 Data](10_DATA.md)의 필드별 품질/식별/AGE를 적용한다. |

**반환·호출:** 로컬 C2 입력 처리 결과. Network 수신 및 Core commit/observer 계약은 [공통 호출 계약](#ports)을 따른다.

**메시지:** `CIS_STATUS` — [송수신 색인](../../05_MESSAGE_INDEX.md). 원 wire 객체를 C2/Gateway App 객체로 raw cast 하지 않고 명시 decode/projection 후 호출한다.

<a id="api-domainif_rxwindowstate"></a>
## `DomainIf_RxWindowState()`

```c
DomainIf_Status_t DomainIf_RxWindowState( const WindowState_t *state);
```

**방향:** Network→C2 · **지위:** DECLARED_SOURCE / BINDING_OPEN / [구현 보류 — 설계 유지].

**용도·호출자·시점:** DOMAIN CAN Codec가 WINDOW_STATE의 실제 움직임/위치/끝단/끼임·원 품질을 전달한다. [구현 보류 — 설계 유지].

| 인자 | 정확한 타입 | IN/OUT | 사용·수명 |
|---|---|---|---|
| `state` | [`const WindowState_t *`](11_VEHICLE_TYPES.md#type-windowstate-t) | IN / borrowed | 호출 동안 유효한 `WindowState_t` 객체. 아래 용도와 연결된 원 owner의 WINDOW_STATE 의미를 제공한다. [전체 Data](10_DATA.md)의 필드별 품질/식별/AGE를 적용한다. |

**반환·호출:** 로컬 C2 입력 처리 결과. Network 수신 및 Core commit/observer 계약은 [공통 호출 계약](#ports)을 따른다.

**메시지:** `WINDOW_STATE` — [송수신 색인](../../05_MESSAGE_INDEX.md). 원 wire 객체를 C2/Gateway App 객체로 raw cast 하지 않고 명시 decode/projection 후 호출한다.

<a id="api-domainif_rxwindowcommandresult"></a>
## `DomainIf_RxWindowCommandResult()`

```c
DomainIf_Status_t DomainIf_RxWindowCommandResult( const DomainIf_WindowCommandResult_t *result);
```

**방향:** Network→C2 · **지위:** DECLARED_SOURCE / BINDING_OPEN / [구현 보류 — 설계 유지].

**용도·호출자·시점:** DOMAIN CAN Codec가 WINDOW_RESULT의 원 Command 연결을 검증해 결과 owner에 인계한다. REQUEST_SEQUENCE↔commandId의 정확관계는 팀 결정 전 전환보류.

| 인자 | 정확한 타입 | IN/OUT | 사용·수명 |
|---|---|---|---|
| `result` | [`const DomainIf_WindowCommandResult_t *`](12_DOMAIN_TYPES.md#type-domainif-windowcommandresult-t) | IN / borrowed | 호출 동안 유효한 `DomainIf_WindowCommandResult_t` 객체. 아래 용도와 연결된 원 owner의 WINDOW_RESULT 의미를 제공한다. [전체 Data](10_DATA.md)의 필드별 품질/식별/AGE를 적용한다. |

**반환·호출:** 로컬 C2 입력 처리 결과. Network 수신 및 Core commit/observer 계약은 [공통 호출 계약](#ports)을 따른다.

**메시지:** `WINDOW_RESULT` — [송수신 색인](../../05_MESSAGE_INDEX.md). 원 wire 객체를 C2/Gateway App 객체로 raw cast 하지 않고 명시 decode/projection 후 호출한다.

<a id="api-domainif_rxwindowantipinch"></a>
## `DomainIf_RxWindowAntiPinch()`

```c
DomainIf_Status_t DomainIf_RxWindowAntiPinch( const DomainIf_AntiPinchReport_t *report);
```

**방향:** Network→C2 · **지위:** DECLARED_SOURCE / BINDING_OPEN / [구현 보류 — 설계 유지].

**용도·호출자·시점:** WINDOW_EVENT 중 검증된 AntiPinch 보호상태만 원 발생/품질/세대와 인계한다. END Event를 이 호출로 지우지 않는다. [구현 보류 — 설계 유지].

| 인자 | 정확한 타입 | IN/OUT | 사용·수명 |
|---|---|---|---|
| `report` | [`const DomainIf_AntiPinchReport_t *`](12_DOMAIN_TYPES.md#type-domainif-antipinchreport-t) | IN / borrowed | 호출 동안 유효한 `DomainIf_AntiPinchReport_t` 객체. 아래 용도와 연결된 원 owner의 RxWindowAntiPinch 의미를 제공한다. [전체 Data](10_DATA.md)의 필드별 품질/식별/AGE를 적용한다. |

**반환·호출:** 로컬 C2 입력 처리 결과. Network 수신 및 Core commit/observer 계약은 [공통 호출 계약](#ports)을 따른다.

<a id="api-domainif_rxwindowfault"></a>
## `DomainIf_RxWindowFault()`

```c
DomainIf_Status_t DomainIf_RxWindowFault( const FaultInfo_t *fault);
```

**방향:** Network→C2 · **지위:** DECLARED_SOURCE / BINDING_OPEN / [구현 보류 — 설계 유지].

**용도·호출자·시점:** WINDOW_FAULT의 좁은 FaultInfo 투영을 기존 고장 owner에 인계한다. private 원 객체가 occurrence/AGE/세대를 보존해야 하며 실제 owner binding은 미정.

| 인자 | 정확한 타입 | IN/OUT | 사용·수명 |
|---|---|---|---|
| `fault` | [`const FaultInfo_t *`](11_VEHICLE_TYPES.md#type-faultinfo-t) | IN / borrowed | 호출 동안 유효한 `FaultInfo_t` 객체. 아래 용도와 연결된 원 owner의 WINDOW_FAULT 의미를 제공한다. [전체 Data](10_DATA.md)의 필드별 품질/식별/AGE를 적용한다. |

**반환·호출:** 로컬 C2 입력 처리 결과. Network 수신 및 Core commit/observer 계약은 [공통 호출 계약](#ports)을 따른다.

**메시지:** `WINDOW_FAULT` — [송수신 색인](../../05_MESSAGE_INDEX.md). 원 wire 객체를 C2/Gateway App 객체로 raw cast 하지 않고 명시 decode/projection 후 호출한다.

<a id="api-domainif_rxvssstate"></a>
## `DomainIf_RxVssState()`

```c
DomainIf_Status_t DomainIf_RxVssState( const DomainIf_VssStateReport_t *state);
```

**방향:** Network→C2 · **지위:** DECLARED_SOURCE / BINDING_OPEN.

**용도·호출자·시점:** DOMAIN CAN Codec가 VSS_STATUS를 C2 서비스상태/원 fault 묶음으로 명시투영해 인계한다. R4/R6 원 Snapshot→DTO 변환위치 합의와 별도 단계다.

| 인자 | 정확한 타입 | IN/OUT | 사용·수명 |
|---|---|---|---|
| `state` | [`const DomainIf_VssStateReport_t *`](12_DOMAIN_TYPES.md#type-domainif-vssstatereport-t) | IN / borrowed | 호출 동안 유효한 `DomainIf_VssStateReport_t` 객체. 아래 용도와 연결된 원 owner의 VSS_STATUS 의미를 제공한다. [전체 Data](10_DATA.md)의 필드별 품질/식별/AGE를 적용한다. |

**반환·호출:** 로컬 C2 입력 처리 결과. Network 수신 및 Core commit/observer 계약은 [공통 호출 계약](#ports)을 따른다.

**메시지:** `VSS_STATUS` — [송수신 색인](../../05_MESSAGE_INDEX.md). 원 wire 객체를 C2/Gateway App 객체로 raw cast 하지 않고 명시 decode/projection 후 호출한다.

<a id="api-domainif_rxvssfault"></a>
## `DomainIf_RxVssFault()`

```c
DomainIf_Status_t DomainIf_RxVssFault( const FaultInfo_t *fault);
```

**방향:** Network→C2 · **지위:** DECLARED_SOURCE / BINDING_OPEN.

**용도·호출자·시점:** VSS_STATUS의 FaultInfo 투영을 기존 Domain 고장 owner에 인계한다. 음향서비스상태와 최근 fault occurrence/AGE를 소실시키지 않는다.

| 인자 | 정확한 타입 | IN/OUT | 사용·수명 |
|---|---|---|---|
| `fault` | [`const FaultInfo_t *`](11_VEHICLE_TYPES.md#type-faultinfo-t) | IN / borrowed | 호출 동안 유효한 `FaultInfo_t` 객체. 아래 용도와 연결된 원 owner의 VSS_STATUS 의미를 제공한다. [전체 Data](10_DATA.md)의 필드별 품질/식별/AGE를 적용한다. |

**반환·호출:** 로컬 C2 입력 처리 결과. Network 수신 및 Core commit/observer 계약은 [공통 호출 계약](#ports)을 따른다.

**메시지:** `VSS_STATUS` — [송수신 색인](../../05_MESSAGE_INDEX.md). 원 wire 객체를 C2/Gateway App 객체로 raw cast 하지 않고 명시 decode/projection 후 호출한다.

<a id="api-domainif_txbcmdoorcommand"></a>
## `DomainIf_TxBcmDoorCommand()`

```c
DomainIf_Status_t DomainIf_TxBcmDoorCommand( const DomainIf_BcmDoorCommand_t *command);
```

**방향:** Application→Network · **지위:** DECLARED_SOURCE / BINDING_OPEN.

**용도·호출자·시점:** Domain 기능 owner가 확정한 `BCM_DOOR_COMMAND` 의미 객체를 C2에 제출한다. 등록된 typed TxPort callback이 실제 Network로 인계하고 source ID/품질/AGE를 보존한다.

| 인자 | 정확한 타입 | IN/OUT | 사용·수명 |
|---|---|---|---|
| `command` | [`const DomainIf_BcmDoorCommand_t *`](12_DOMAIN_TYPES.md#type-domainif-bcmdoorcommand-t) | IN / borrowed | 호출 동안 유효한 `DomainIf_BcmDoorCommand_t` 객체. 아래 용도와 연결된 원 owner의 BCM_DOOR_COMMAND 의미를 제공한다. [전체 Data](10_DATA.md)의 필드별 품질/식별/AGE를 적용한다. |

**반환·호출:** 로컬 TX 수용 결과. 안정된 복사/encode 전 OK 금지; ECU 실행 DONE과 다르다.

**메시지:** `BCM_DOOR_COMMAND` — [송수신 색인](../../05_MESSAGE_INDEX.md). 원 wire 객체를 C2/Gateway App 객체로 raw cast 하지 않고 명시 decode/projection 후 호출한다.

<a id="api-domainif_txbcmclimatecommand"></a>
## `DomainIf_TxBcmClimateCommand()`

```c
DomainIf_Status_t DomainIf_TxBcmClimateCommand( const DomainIf_BcmClimateCommand_t *command);
```

**방향:** Application→Network · **지위:** DECLARED_SOURCE / BINDING_OPEN.

**용도·호출자·시점:** Domain 기능 owner가 확정한 `BCM_CLIMATE_COMMAND` 의미 객체를 C2에 제출한다. 등록된 typed TxPort callback이 실제 Network로 인계하고 source ID/품질/AGE를 보존한다.

| 인자 | 정확한 타입 | IN/OUT | 사용·수명 |
|---|---|---|---|
| `command` | [`const DomainIf_BcmClimateCommand_t *`](12_DOMAIN_TYPES.md#type-domainif-bcmclimatecommand-t) | IN / borrowed | 호출 동안 유효한 `DomainIf_BcmClimateCommand_t` 객체. 아래 용도와 연결된 원 owner의 BCM_CLIMATE_COMMAND 의미를 제공한다. [전체 Data](10_DATA.md)의 필드별 품질/식별/AGE를 적용한다. |

**반환·호출:** 로컬 TX 수용 결과. 안정된 복사/encode 전 OK 금지; ECU 실행 DONE과 다르다.

**메시지:** `BCM_CLIMATE_COMMAND` — [송수신 색인](../../05_MESSAGE_INDEX.md). 원 wire 객체를 C2/Gateway App 객체로 raw cast 하지 않고 명시 decode/projection 후 호출한다.

<a id="api-domainif_txbcminteriorlightcommand"></a>
## `DomainIf_TxBcmInteriorLightCommand()`

```c
DomainIf_Status_t DomainIf_TxBcmInteriorLightCommand( const DomainIf_BcmInteriorLightCommand_t *command);
```

**방향:** Application→Network · **지위:** DECLARED_SOURCE / BINDING_OPEN.

**용도·호출자·시점:** Domain 기능 owner가 확정한 `BCM_LIGHT_COMMAND` 의미 객체를 C2에 제출한다. 등록된 typed TxPort callback이 실제 Network로 인계하고 source ID/품질/AGE를 보존한다.

| 인자 | 정확한 타입 | IN/OUT | 사용·수명 |
|---|---|---|---|
| `command` | [`const DomainIf_BcmInteriorLightCommand_t *`](12_DOMAIN_TYPES.md#type-domainif-bcminteriorlightcommand-t) | IN / borrowed | 호출 동안 유효한 `DomainIf_BcmInteriorLightCommand_t` 객체. 아래 용도와 연결된 원 owner의 BCM_LIGHT_COMMAND 의미를 제공한다. [전체 Data](10_DATA.md)의 필드별 품질/식별/AGE를 적용한다. |

**반환·호출:** 로컬 TX 수용 결과. 안정된 복사/encode 전 OK 금지; ECU 실행 DONE과 다르다.

**메시지:** `BCM_LIGHT_COMMAND` — [송수신 색인](../../05_MESSAGE_INDEX.md). 원 wire 객체를 C2/Gateway App 객체로 raw cast 하지 않고 명시 decode/projection 후 호출한다.

<a id="api-domainif_txcispowerpermission"></a>
## `DomainIf_TxCisPowerPermission()`

```c
DomainIf_Status_t DomainIf_TxCisPowerPermission( const DomainIf_CisPowerPermission_t *permission);
```

**방향:** Application→Network · **지위:** DECLARED_SOURCE / BINDING_OPEN.

**용도·호출자·시점:** Domain 기능 owner가 확정한 `DOMAIN_PERMISSION` 의미 객체를 C2에 제출한다. 등록된 typed TxPort callback이 실제 Network로 인계하고 source ID/품질/AGE를 보존한다.

| 인자 | 정확한 타입 | IN/OUT | 사용·수명 |
|---|---|---|---|
| `permission` | [`const DomainIf_CisPowerPermission_t *`](12_DOMAIN_TYPES.md#type-domainif-cispowerpermission-t) | IN / borrowed | 호출 동안 유효한 `DomainIf_CisPowerPermission_t` 객체. 아래 용도와 연결된 원 owner의 DOMAIN_PERMISSION 의미를 제공한다. [전체 Data](10_DATA.md)의 필드별 품질/식별/AGE를 적용한다. |

**반환·호출:** 로컬 TX 수용 결과. 안정된 복사/encode 전 OK 금지; ECU 실행 DONE과 다르다.

**메시지:** `DOMAIN_PERMISSION` — [송수신 색인](../../05_MESSAGE_INDEX.md). 원 wire 객체를 C2/Gateway App 객체로 raw cast 하지 않고 명시 decode/projection 후 호출한다.

<a id="api-domainif_txwindowcommand"></a>
## `DomainIf_TxWindowCommand()`

```c
DomainIf_Status_t DomainIf_TxWindowCommand( const DomainIf_WindowCommand_t *command);
```

**방향:** Application→Network · **지위:** DECLARED_SOURCE / BINDING_OPEN / [구현 보류 — 설계 유지].

**용도·호출자·시점:** Domain 기능 owner가 확정한 `WINDOW_COMMAND` 의미 객체를 C2에 제출한다. 등록된 typed TxPort callback이 실제 Network로 인계하고 source ID/품질/AGE를 보존한다.

| 인자 | 정확한 타입 | IN/OUT | 사용·수명 |
|---|---|---|---|
| `command` | [`const DomainIf_WindowCommand_t *`](12_DOMAIN_TYPES.md#type-domainif-windowcommand-t) | IN / borrowed | 호출 동안 유효한 `DomainIf_WindowCommand_t` 객체. 아래 용도와 연결된 원 owner의 WINDOW_COMMAND 의미를 제공한다. [전체 Data](10_DATA.md)의 필드별 품질/식별/AGE를 적용한다. |

**반환·호출:** 로컬 TX 수용 결과. 안정된 복사/encode 전 OK 금지; ECU 실행 DONE과 다르다.

**메시지:** `WINDOW_COMMAND` — [송수신 색인](../../05_MESSAGE_INDEX.md). 원 wire 객체를 C2/Gateway App 객체로 raw cast 하지 않고 명시 decode/projection 후 호출한다.

<a id="api-domainif_txwindowoperationpermission"></a>
## `DomainIf_TxWindowOperationPermission()`

```c
DomainIf_Status_t DomainIf_TxWindowOperationPermission( const DomainIf_WindowOperationPermission_t *permission);
```

**방향:** Application→Network · **지위:** DECLARED_SOURCE / BINDING_OPEN / [구현 보류 — 설계 유지].

**용도·호출자·시점:** Domain 기능 owner가 확정한 `DOMAIN_PERMISSION` 의미 객체를 C2에 제출한다. 등록된 typed TxPort callback이 실제 Network로 인계하고 source ID/품질/AGE를 보존한다.

| 인자 | 정확한 타입 | IN/OUT | 사용·수명 |
|---|---|---|---|
| `permission` | [`const DomainIf_WindowOperationPermission_t *`](12_DOMAIN_TYPES.md#type-domainif-windowoperationpermission-t) | IN / borrowed | 호출 동안 유효한 `DomainIf_WindowOperationPermission_t` 객체. 아래 용도와 연결된 원 owner의 DOMAIN_PERMISSION 의미를 제공한다. [전체 Data](10_DATA.md)의 필드별 품질/식별/AGE를 적용한다. |

**반환·호출:** 로컬 TX 수용 결과. 안정된 복사/encode 전 OK 금지; ECU 실행 DONE과 다르다.

**메시지:** `DOMAIN_PERMISSION` — [송수신 색인](../../05_MESSAGE_INDEX.md). 원 wire 객체를 C2/Gateway App 객체로 raw cast 하지 않고 명시 decode/projection 후 호출한다.

<a id="api-domainif_txwindowdomainalive"></a>
## `DomainIf_TxWindowDomainAlive()`

```c
DomainIf_Status_t DomainIf_TxWindowDomainAlive( const DomainIf_WindowDomainAlive_t *alive);
```

**방향:** Application→Network · **지위:** DECLARED_SOURCE / BINDING_OPEN / [구현 보류 — 설계 유지].

**용도·호출자·시점:** Domain 기능 owner가 확정한 `DOMAIN_PERMISSION` 의미 객체를 C2에 제출한다. 등록된 typed TxPort callback이 실제 Network로 인계하고 source ID/품질/AGE를 보존한다.

| 인자 | 정확한 타입 | IN/OUT | 사용·수명 |
|---|---|---|---|
| `alive` | [`const DomainIf_WindowDomainAlive_t *`](12_DOMAIN_TYPES.md#type-domainif-windowdomainalive-t) | IN / borrowed | 호출 동안 유효한 `DomainIf_WindowDomainAlive_t` 객체. 아래 용도와 연결된 원 owner의 DOMAIN_PERMISSION 의미를 제공한다. [전체 Data](10_DATA.md)의 필드별 품질/식별/AGE를 적용한다. |

**반환·호출:** 로컬 TX 수용 결과. 안정된 복사/encode 전 OK 금지; ECU 실행 DONE과 다르다.

**메시지:** `DOMAIN_PERMISSION` — [송수신 색인](../../05_MESSAGE_INDEX.md). 원 wire 객체를 C2/Gateway App 객체로 raw cast 하지 않고 명시 decode/projection 후 호출한다.

<a id="api-domainif_txvssoneshotevent"></a>
## `DomainIf_TxVssOneShotEvent()`

```c
DomainIf_Status_t DomainIf_TxVssOneShotEvent( const DomainIf_VssOneShotEvent_t *event);
```

**방향:** Application→Network · **지위:** DECLARED_SOURCE / BINDING_OPEN.

**용도·호출자·시점:** Domain 기능 owner가 확정한 `VSS_EVENT` 의미 객체를 C2에 제출한다. 등록된 typed TxPort callback이 실제 Network로 인계하고 source ID/품질/AGE를 보존한다.

| 인자 | 정확한 타입 | IN/OUT | 사용·수명 |
|---|---|---|---|
| `event` | [`const DomainIf_VssOneShotEvent_t *`](12_DOMAIN_TYPES.md#type-domainif-vssoneshotevent-t) | IN / borrowed | 호출 동안 유효한 `DomainIf_VssOneShotEvent_t` 객체. 아래 용도와 연결된 원 owner의 VSS_EVENT 의미를 제공한다. [전체 Data](10_DATA.md)의 필드별 품질/식별/AGE를 적용한다. |

**반환·호출:** 로컬 TX 수용 결과. 안정된 복사/encode 전 OK 금지; ECU 실행 DONE과 다르다.

**메시지:** `VSS_EVENT` — [송수신 색인](../../05_MESSAGE_INDEX.md). 원 wire 객체를 C2/Gateway App 객체로 raw cast 하지 않고 명시 decode/projection 후 호출한다.

<a id="api-domainif_txvsswarningstate"></a>
## `DomainIf_TxVssWarningState()`

```c
DomainIf_Status_t DomainIf_TxVssWarningState( const DomainIf_VssWarningState_t *state);
```

**방향:** Application→Network · **지위:** DECLARED_SOURCE / BINDING_OPEN.

**용도·호출자·시점:** Domain 기능 owner가 확정한 `VSS_WARNING_STATE` 의미 객체를 C2에 제출한다. 등록된 typed TxPort callback이 실제 Network로 인계하고 source ID/품질/AGE를 보존한다.

| 인자 | 정확한 타입 | IN/OUT | 사용·수명 |
|---|---|---|---|
| `state` | [`const DomainIf_VssWarningState_t *`](12_DOMAIN_TYPES.md#type-domainif-vsswarningstate-t) | IN / borrowed | 호출 동안 유효한 `DomainIf_VssWarningState_t` 객체. 아래 용도와 연결된 원 owner의 VSS_WARNING_STATE 의미를 제공한다. [전체 Data](10_DATA.md)의 필드별 품질/식별/AGE를 적용한다. |

**반환·호출:** 로컬 TX 수용 결과. 안정된 복사/encode 전 OK 금지; ECU 실행 DONE과 다르다.

**메시지:** `VSS_WARNING_STATE` — [송수신 색인](../../05_MESSAGE_INDEX.md). 원 wire 객체를 C2/Gateway App 객체로 raw cast 하지 않고 명시 decode/projection 후 호출한다.

<a id="api-domainif_txmobilerequestresult"></a>
## `DomainIf_TxMobileRequestResult()`

```c
DomainIf_Status_t DomainIf_TxMobileRequestResult( const DomainIf_MobileRequestResult_t *result);
```

**방향:** Application→Network · **지위:** DECLARED_SOURCE / BINDING_OPEN.

**용도·호출자·시점:** Domain 기능 owner가 확정한 `M_RESULT` 의미 객체를 C2에 제출한다. 등록된 typed TxPort callback이 실제 Network로 인계하고 source ID/품질/AGE를 보존한다.

| 인자 | 정확한 타입 | IN/OUT | 사용·수명 |
|---|---|---|---|
| `result` | [`const DomainIf_MobileRequestResult_t *`](12_DOMAIN_TYPES.md#type-domainif-mobilerequestresult-t) | IN / borrowed | 호출 동안 유효한 `DomainIf_MobileRequestResult_t` 객체. 아래 용도와 연결된 원 owner의 M_RESULT 의미를 제공한다. [전체 Data](10_DATA.md)의 필드별 품질/식별/AGE를 적용한다. |

**반환·호출:** 로컬 TX 수용 결과. 안정된 복사/encode 전 OK 금지; ECU 실행 DONE과 다르다.

**메시지:** `M_RESULT` — [송수신 색인](../../05_MESSAGE_INDEX.md). 원 wire 객체를 C2/Gateway App 객체로 raw cast 하지 않고 명시 decode/projection 후 호출한다.

<a id="api-domainif_txmobilewarning"></a>
## `DomainIf_TxMobileWarning()`

```c
DomainIf_Status_t DomainIf_TxMobileWarning( const DomainIf_WarningInfo_t *warning);
```

**방향:** Application→Network · **지위:** DECLARED_SOURCE / BINDING_OPEN.

**용도·호출자·시점:** Domain 기능 owner가 확정한 `M_WARNING` 의미 객체를 C2에 제출한다. 등록된 typed TxPort callback이 실제 Network로 인계하고 source ID/품질/AGE를 보존한다.

| 인자 | 정확한 타입 | IN/OUT | 사용·수명 |
|---|---|---|---|
| `warning` | [`const DomainIf_WarningInfo_t *`](12_DOMAIN_TYPES.md#type-domainif-warninginfo-t) | IN / borrowed | 호출 동안 유효한 `DomainIf_WarningInfo_t` 객체. 아래 용도와 연결된 원 owner의 M_WARNING 의미를 제공한다. [전체 Data](10_DATA.md)의 필드별 품질/식별/AGE를 적용한다. |

**반환·호출:** 로컬 TX 수용 결과. 안정된 복사/encode 전 OK 금지; ECU 실행 DONE과 다르다.

**메시지:** `M_WARNING` — [송수신 색인](../../05_MESSAGE_INDEX.md). 원 wire 객체를 C2/Gateway App 객체로 raw cast 하지 않고 명시 decode/projection 후 호출한다.

<a id="api-domainif_txmobilefunctionavailability"></a>
## `DomainIf_TxMobileFunctionAvailability()`

```c
DomainIf_Status_t DomainIf_TxMobileFunctionAvailability( const DomainIf_FunctionAvailabilitySet_t *availability);
```

**방향:** Application→Network · **지위:** DECLARED_SOURCE / BINDING_OPEN.

**용도·호출자·시점:** Domain 기능 owner가 확정한 `M_AVAILABILITY` 의미 객체를 C2에 제출한다. 등록된 typed TxPort callback이 실제 Network로 인계하고 source ID/품질/AGE를 보존한다.

| 인자 | 정확한 타입 | IN/OUT | 사용·수명 |
|---|---|---|---|
| `availability` | [`const DomainIf_FunctionAvailabilitySet_t *`](12_DOMAIN_TYPES.md#type-domainif-functionavailabilityset-t) | IN / borrowed | 호출 동안 유효한 `DomainIf_FunctionAvailabilitySet_t` 객체. 아래 용도와 연결된 원 owner의 M_AVAILABILITY 의미를 제공한다. [전체 Data](10_DATA.md)의 필드별 품질/식별/AGE를 적용한다. |

**반환·호출:** 로컬 TX 수용 결과. 안정된 복사/encode 전 OK 금지; ECU 실행 DONE과 다르다.

**메시지:** `M_AVAILABILITY` — [송수신 색인](../../05_MESSAGE_INDEX.md). 원 wire 객체를 C2/Gateway App 객체로 raw cast 하지 않고 명시 decode/projection 후 호출한다.

<a id="api-domainif_txmobiledigitalkeystate"></a>
## `DomainIf_TxMobileDigitalKeyState()`

```c
DomainIf_Status_t DomainIf_TxMobileDigitalKeyState( const DigitalKeyState_t *state);
```

**방향:** Application→Network · **지위:** DECLARED_SOURCE / BINDING_OPEN.

**용도·호출자·시점:** Domain 기능 owner가 확정한 `M_DIGITAL_STATUS` 의미 객체를 C2에 제출한다. 등록된 typed TxPort callback이 실제 Network로 인계하고 source ID/품질/AGE를 보존한다.

| 인자 | 정확한 타입 | IN/OUT | 사용·수명 |
|---|---|---|---|
| `state` | [`const DigitalKeyState_t *`](11_VEHICLE_TYPES.md#type-digitalkeystate-t) | IN / borrowed | 호출 동안 유효한 `DigitalKeyState_t` 객체. 아래 용도와 연결된 원 owner의 M_DIGITAL_STATUS 의미를 제공한다. [전체 Data](10_DATA.md)의 필드별 품질/식별/AGE를 적용한다. |

**반환·호출:** 로컬 TX 수용 결과. 안정된 복사/encode 전 OK 금지; ECU 실행 DONE과 다르다.

**메시지:** `M_DIGITAL_STATUS` — [송수신 색인](../../05_MESSAGE_INDEX.md). 원 wire 객체를 C2/Gateway App 객체로 raw cast 하지 않고 명시 decode/projection 후 호출한다.

<a id="api-domainif_txmobiledigitalkeyresult"></a>
## `DomainIf_TxMobileDigitalKeyResult()`

```c
DomainIf_Status_t DomainIf_TxMobileDigitalKeyResult( const DomainIf_DigitalKeyResult_t *result);
```

**방향:** Application→Network · **지위:** DECLARED_SOURCE / BINDING_OPEN.

**용도·호출자·시점:** Domain 기능 owner가 확정한 `M_DIGITAL_RESULT` 의미 객체를 C2에 제출한다. 등록된 typed TxPort callback이 실제 Network로 인계하고 source ID/품질/AGE를 보존한다.

| 인자 | 정확한 타입 | IN/OUT | 사용·수명 |
|---|---|---|---|
| `result` | [`const DomainIf_DigitalKeyResult_t *`](12_DOMAIN_TYPES.md#type-domainif-digitalkeyresult-t) | IN / borrowed | 호출 동안 유효한 `DomainIf_DigitalKeyResult_t` 객체. 아래 용도와 연결된 원 owner의 M_DIGITAL_RESULT 의미를 제공한다. [전체 Data](10_DATA.md)의 필드별 품질/식별/AGE를 적용한다. |

**반환·호출:** 로컬 TX 수용 결과. 안정된 복사/encode 전 OK 금지; ECU 실행 DONE과 다르다.

**메시지:** `M_DIGITAL_RESULT` — [송수신 색인](../../05_MESSAGE_INDEX.md). 원 wire 객체를 C2/Gateway App 객체로 raw cast 하지 않고 명시 decode/projection 후 호출한다.

<a id="api-domainif_rxbcmstatus"></a>
## `DomainIf_RxBcmStatus()`

```c
DomainIf_Status_t DomainIf_RxBcmStatus( const DomainIf_BcmStatusReport_t *status);
```

**방향:** Network→C2 · **지위:** DECLARED_SOURCE / BINDING_OPEN.

**용도·호출자·시점:** DOMAIN CAN Codec가 BCM_STATUS의 ECU/activefault/affectedfunction/recovering/원 세대·품질을 원자적 status로 인계한다. 링크 health와 차량 기능 가용성 를 혼동하지 않는다.

| 인자 | 정확한 타입 | IN/OUT | 사용·수명 |
|---|---|---|---|
| `status` | [`const DomainIf_BcmStatusReport_t *`](12_DOMAIN_TYPES.md#type-domainif-bcmstatusreport-t) | IN / borrowed | 호출 동안 유효한 `DomainIf_BcmStatusReport_t` 객체. 아래 용도와 연결된 원 owner의 BCM_STATUS 의미를 제공한다. [전체 Data](10_DATA.md)의 필드별 품질/식별/AGE를 적용한다. |

**반환·호출:** 로컬 C2 입력 처리 결과. Network 수신 및 Core commit/observer 계약은 [공통 호출 계약](#ports)을 따른다.

**메시지:** `BCM_STATUS` — [송수신 색인](../../05_MESSAGE_INDEX.md). 원 wire 객체를 C2/Gateway App 객체로 raw cast 하지 않고 명시 decode/projection 후 호출한다.

<a id="api-domainif_rxbcmcommandresult"></a>
## `DomainIf_RxBcmCommandResult()`

```c
DomainIf_Status_t DomainIf_RxBcmCommandResult( const DomainIf_BcmCommandResult_t *result);
```

**방향:** Network→C2 · **지위:** DECLARED_SOURCE / BINDING_OPEN.

**용도·호출자·시점:** DOMAIN CAN Codec가 BCM_RESULT의 원 commandId·kind·result/reason·atomic 도어 근거를 인계하고 Domain owner가 원 요청과 연결한다. **CAN에는 confirmation 필드가 없다.** 별도 확인 근거가 없으면 기존 UNCONFIRMED로 인계하는 권장안을 적용하며 실제 확인 근거 연결은 PROPOSED/BINDING_OPEN이다. [BCM 결과·confirmation의 정확한 제한](../../03_COMMUNICATION/10_CAN_FD.md#message-bcm-result)을 따른다.

| 인자 | 정확한 타입 | IN/OUT | 사용·수명 |
|---|---|---|---|
| `result` | [`const DomainIf_BcmCommandResult_t *`](12_DOMAIN_TYPES.md#type-domainif-bcmcommandresult-t) | IN / borrowed | 호출 동안 유효한 `DomainIf_BcmCommandResult_t` 객체. 아래 용도와 연결된 원 owner의 BCM_RESULT 의미를 제공한다. [전체 Data](10_DATA.md)의 필드별 품질/식별/AGE를 적용한다. |

**반환·호출:** 로컬 C2 입력 처리 결과. Network 수신 및 Core commit/observer 계약은 [공통 호출 계약](#ports)을 따른다.

**메시지:** `BCM_RESULT` — [송수신 색인](../../05_MESSAGE_INDEX.md). 원 wire 객체를 C2/Gateway App 객체로 raw cast 하지 않고 명시 decode/projection 후 호출한다.

<a id="api-domainif_rxbcmprotectionevent"></a>
## `DomainIf_RxBcmProtectionEvent()`

```c
DomainIf_Status_t DomainIf_RxBcmProtectionEvent( const DomainIf_BcmProtectionEvent_t *event);
```

**방향:** Network→C2 · **지위:** DECLARED_SOURCE / BINDING_OPEN.

**용도·호출자·시점:** DOMAIN CAN Codec가 BCM_EVENT의 과열/팬불일치/회복확인·occurrence·related command/원 AGE를 인계한다. 도어실행 완료와 독립 보호 사건이다.

| 인자 | 정확한 타입 | IN/OUT | 사용·수명 |
|---|---|---|---|
| `event` | [`const DomainIf_BcmProtectionEvent_t *`](12_DOMAIN_TYPES.md#type-domainif-bcmprotectionevent-t) | IN / borrowed | 호출 동안 유효한 `DomainIf_BcmProtectionEvent_t` 객체. 아래 용도와 연결된 원 owner의 BCM_EVENT 의미를 제공한다. [전체 Data](10_DATA.md)의 필드별 품질/식별/AGE를 적용한다. |

**반환·호출:** 로컬 C2 입력 처리 결과. Network 수신 및 Core commit/observer 계약은 [공통 호출 계약](#ports)을 따른다.

**메시지:** `BCM_EVENT` — [송수신 색인](../../05_MESSAGE_INDEX.md). 원 wire 객체를 C2/Gateway App 객체로 raw cast 하지 않고 명시 decode/projection 후 호출한다.

<a id="api-domainif_txmobilebcmdoorstate"></a>
## `DomainIf_TxMobileBcmDoorState()`

```c
DomainIf_Status_t DomainIf_TxMobileBcmDoorState(const DoorState_t *state);
```

**방향:** Application→Network · **지위:** DECLARED_SOURCE / BINDING_OPEN.

**용도·호출자·시점:** Domain 기능 owner가 확정한 `M_BCM_DOOR_STATE` 의미 객체를 C2에 제출한다. 등록된 typed TxPort callback이 실제 Network로 인계하고 source ID/품질/AGE를 보존한다.

| 인자 | 정확한 타입 | IN/OUT | 사용·수명 |
|---|---|---|---|
| `state` | [`const DoorState_t *`](11_VEHICLE_TYPES.md#type-doorstate-t) | IN / borrowed | 호출 동안 유효한 `DoorState_t` 객체. 아래 용도와 연결된 원 owner의 M_BCM_DOOR_STATE 의미를 제공한다. [전체 Data](10_DATA.md)의 필드별 품질/식별/AGE를 적용한다. |

**반환·호출:** 로컬 TX 수용 결과. 안정된 복사/encode 전 OK 금지; ECU 실행 DONE과 다르다.

**메시지:** `M_BCM_DOOR_STATE` — [송수신 색인](../../05_MESSAGE_INDEX.md). 원 wire 객체를 C2/Gateway App 객체로 raw cast 하지 않고 명시 decode/projection 후 호출한다.

<a id="api-domainif_txmobilebcmclimatestate"></a>
## `DomainIf_TxMobileBcmClimateState()`

```c
DomainIf_Status_t DomainIf_TxMobileBcmClimateState(const ClimateState_t *state);
```

**방향:** Application→Network · **지위:** DECLARED_SOURCE / BINDING_OPEN.

**용도·호출자·시점:** Domain 기능 owner가 확정한 `M_BCM_CLIMATE_STATE` 의미 객체를 C2에 제출한다. 등록된 typed TxPort callback이 실제 Network로 인계하고 source ID/품질/AGE를 보존한다.

| 인자 | 정확한 타입 | IN/OUT | 사용·수명 |
|---|---|---|---|
| `state` | [`const ClimateState_t *`](11_VEHICLE_TYPES.md#type-climatestate-t) | IN / borrowed | 호출 동안 유효한 `ClimateState_t` 객체. 아래 용도와 연결된 원 owner의 M_BCM_CLIMATE_STATE 의미를 제공한다. [전체 Data](10_DATA.md)의 필드별 품질/식별/AGE를 적용한다. |

**반환·호출:** 로컬 TX 수용 결과. 안정된 복사/encode 전 OK 금지; ECU 실행 DONE과 다르다.

**메시지:** `M_BCM_CLIMATE_STATE` — [송수신 색인](../../05_MESSAGE_INDEX.md). 원 wire 객체를 C2/Gateway App 객체로 raw cast 하지 않고 명시 decode/projection 후 호출한다.

<a id="api-domainif_txmobilebcminteriorlightstate"></a>
## `DomainIf_TxMobileBcmInteriorLightState()`

```c
DomainIf_Status_t DomainIf_TxMobileBcmInteriorLightState(const InteriorLightState_t *state);
```

**방향:** Application→Network · **지위:** DECLARED_SOURCE / BINDING_OPEN.

**용도·호출자·시점:** Domain 기능 owner가 확정한 `M_BCM_LIGHT_STATE` 의미 객체를 C2에 제출한다. 등록된 typed TxPort callback이 실제 Network로 인계하고 source ID/품질/AGE를 보존한다.

| 인자 | 정확한 타입 | IN/OUT | 사용·수명 |
|---|---|---|---|
| `state` | [`const InteriorLightState_t *`](11_VEHICLE_TYPES.md#type-interiorlightstate-t) | IN / borrowed | 호출 동안 유효한 `InteriorLightState_t` 객체. 아래 용도와 연결된 원 owner의 M_BCM_LIGHT_STATE 의미를 제공한다. [전체 Data](10_DATA.md)의 필드별 품질/식별/AGE를 적용한다. |

**반환·호출:** 로컬 TX 수용 결과. 안정된 복사/encode 전 OK 금지; ECU 실행 DONE과 다르다.

**메시지:** `M_BCM_LIGHT_STATE` — [송수신 색인](../../05_MESSAGE_INDEX.md). 원 wire 객체를 C2/Gateway App 객체로 raw cast 하지 않고 명시 decode/projection 후 호출한다.

<a id="api-domainif_txmobilebcmstatus"></a>
## `DomainIf_TxMobileBcmStatus()`

```c
DomainIf_Status_t DomainIf_TxMobileBcmStatus(const DomainIf_BcmStatusReport_t *state);
```

**방향:** Application→Network · **지위:** DECLARED_SOURCE / BINDING_OPEN.

**용도·호출자·시점:** Domain 기능 owner가 확정한 `M_BCM_STATUS` 의미 객체를 C2에 제출한다. 등록된 typed TxPort callback이 실제 Network로 인계하고 source ID/품질/AGE를 보존한다.

| 인자 | 정확한 타입 | IN/OUT | 사용·수명 |
|---|---|---|---|
| `state` | [`const DomainIf_BcmStatusReport_t *`](12_DOMAIN_TYPES.md#type-domainif-bcmstatusreport-t) | IN / borrowed | 호출 동안 유효한 `DomainIf_BcmStatusReport_t` 객체. 아래 용도와 연결된 원 owner의 M_BCM_STATUS 의미를 제공한다. [전체 Data](10_DATA.md)의 필드별 품질/식별/AGE를 적용한다. |

**반환·호출:** 로컬 TX 수용 결과. 안정된 복사/encode 전 OK 금지; ECU 실행 DONE과 다르다.

**메시지:** `M_BCM_STATUS` — [송수신 색인](../../05_MESSAGE_INDEX.md). 원 wire 객체를 C2/Gateway App 객체로 raw cast 하지 않고 명시 decode/projection 후 호출한다.

<a id="api-domainif_txmobilecisenvironment"></a>
## `DomainIf_TxMobileCisEnvironment()`

```c
DomainIf_Status_t DomainIf_TxMobileCisEnvironment(const CisCabinEnvironmentState_t *state);
```

**방향:** Application→Network · **지위:** DECLARED_SOURCE / BINDING_OPEN.

**용도·호출자·시점:** Domain 기능 owner가 확정한 `M_CIS_ENVIRONMENT` 의미 객체를 C2에 제출한다. 등록된 typed TxPort callback이 실제 Network로 인계하고 source ID/품질/AGE를 보존한다.

| 인자 | 정확한 타입 | IN/OUT | 사용·수명 |
|---|---|---|---|
| `state` | [`const CisCabinEnvironmentState_t *`](11_VEHICLE_TYPES.md#type-ciscabinenvironmentstate-t) | IN / borrowed | 호출 동안 유효한 `CisCabinEnvironmentState_t` 객체. 아래 용도와 연결된 원 owner의 M_CIS_ENVIRONMENT 의미를 제공한다. [전체 Data](10_DATA.md)의 필드별 품질/식별/AGE를 적용한다. |

**반환·호출:** 로컬 TX 수용 결과. 안정된 복사/encode 전 OK 금지; ECU 실행 DONE과 다르다.

**메시지:** `M_CIS_ENVIRONMENT` — [송수신 색인](../../05_MESSAGE_INDEX.md). 원 wire 객체를 C2/Gateway App 객체로 raw cast 하지 않고 명시 decode/projection 후 호출한다.

<a id="api-domainif_txmobilecisoccupant"></a>
## `DomainIf_TxMobileCisOccupant()`

```c
DomainIf_Status_t DomainIf_TxMobileCisOccupant(const CisOccupantState_t *state);
```

**방향:** Application→Network · **지위:** DECLARED_SOURCE / BINDING_OPEN.

**용도·호출자·시점:** Domain 기능 owner가 확정한 `M_CIS_OCCUPANT` 의미 객체를 C2에 제출한다. 등록된 typed TxPort callback이 실제 Network로 인계하고 source ID/품질/AGE를 보존한다.

| 인자 | 정확한 타입 | IN/OUT | 사용·수명 |
|---|---|---|---|
| `state` | [`const CisOccupantState_t *`](11_VEHICLE_TYPES.md#type-cisoccupantstate-t) | IN / borrowed | 호출 동안 유효한 `CisOccupantState_t` 객체. 아래 용도와 연결된 원 owner의 M_CIS_OCCUPANT 의미를 제공한다. [전체 Data](10_DATA.md)의 필드별 품질/식별/AGE를 적용한다. |

**반환·호출:** 로컬 TX 수용 결과. 안정된 복사/encode 전 OK 금지; ECU 실행 DONE과 다르다.

**메시지:** `M_CIS_OCCUPANT` — [송수신 색인](../../05_MESSAGE_INDEX.md). 원 wire 객체를 C2/Gateway App 객체로 raw cast 하지 않고 명시 decode/projection 후 호출한다.

<a id="api-domainif_txmobilecisrear"></a>
## `DomainIf_TxMobileCisRear()`

```c
DomainIf_Status_t DomainIf_TxMobileCisRear(const CisRearState_t *state);
```

**방향:** Application→Network · **지위:** DECLARED_SOURCE / BINDING_OPEN.

**용도·호출자·시점:** Domain 기능 owner가 확정한 `M_CIS_REAR` 의미 객체를 C2에 제출한다. 등록된 typed TxPort callback이 실제 Network로 인계하고 source ID/품질/AGE를 보존한다.

| 인자 | 정확한 타입 | IN/OUT | 사용·수명 |
|---|---|---|---|
| `state` | [`const CisRearState_t *`](11_VEHICLE_TYPES.md#type-cisrearstate-t) | IN / borrowed | 호출 동안 유효한 `CisRearState_t` 객체. 아래 용도와 연결된 원 owner의 M_CIS_REAR 의미를 제공한다. [전체 Data](10_DATA.md)의 필드별 품질/식별/AGE를 적용한다. |

**반환·호출:** 로컬 TX 수용 결과. 안정된 복사/encode 전 OK 금지; ECU 실행 DONE과 다르다.

**메시지:** `M_CIS_REAR` — [송수신 색인](../../05_MESSAGE_INDEX.md). 원 wire 객체를 C2/Gateway App 객체로 raw cast 하지 않고 명시 decode/projection 후 호출한다.

<a id="api-domainif_txmobilecisstatus"></a>
## `DomainIf_TxMobileCisStatus()`

```c
DomainIf_Status_t DomainIf_TxMobileCisStatus(const DomainIf_CisStatusReport_t *state);
```

**방향:** Application→Network · **지위:** DECLARED_SOURCE / BINDING_OPEN.

**용도·호출자·시점:** Domain 기능 owner가 확정한 `M_CIS_STATUS` 의미 객체를 C2에 제출한다. 등록된 typed TxPort callback이 실제 Network로 인계하고 source ID/품질/AGE를 보존한다.

| 인자 | 정확한 타입 | IN/OUT | 사용·수명 |
|---|---|---|---|
| `state` | [`const DomainIf_CisStatusReport_t *`](12_DOMAIN_TYPES.md#type-domainif-cisstatusreport-t) | IN / borrowed | 호출 동안 유효한 `DomainIf_CisStatusReport_t` 객체. 아래 용도와 연결된 원 owner의 M_CIS_STATUS 의미를 제공한다. [전체 Data](10_DATA.md)의 필드별 품질/식별/AGE를 적용한다. |

**반환·호출:** 로컬 TX 수용 결과. 안정된 복사/encode 전 OK 금지; ECU 실행 DONE과 다르다.

**메시지:** `M_CIS_STATUS` — [송수신 색인](../../05_MESSAGE_INDEX.md). 원 wire 객체를 C2/Gateway App 객체로 raw cast 하지 않고 명시 decode/projection 후 호출한다.

<a id="api-domainif_txmobilevssstatus"></a>
## `DomainIf_TxMobileVssStatus()`

```c
DomainIf_Status_t DomainIf_TxMobileVssStatus(const DomainIf_VssStateReport_t *state);
```

**방향:** Application→Network · **지위:** DECLARED_SOURCE / BINDING_OPEN.

**용도·호출자·시점:** Domain 기능 owner가 확정한 `M_VSS_STATUS` 의미 객체를 C2에 제출한다. 등록된 typed TxPort callback이 실제 Network로 인계하고 source ID/품질/AGE를 보존한다.

| 인자 | 정확한 타입 | IN/OUT | 사용·수명 |
|---|---|---|---|
| `state` | [`const DomainIf_VssStateReport_t *`](12_DOMAIN_TYPES.md#type-domainif-vssstatereport-t) | IN / borrowed | 호출 동안 유효한 `DomainIf_VssStateReport_t` 객체. 아래 용도와 연결된 원 owner의 M_VSS_STATUS 의미를 제공한다. [전체 Data](10_DATA.md)의 필드별 품질/식별/AGE를 적용한다. |

**반환·호출:** 로컬 TX 수용 결과. 안정된 복사/encode 전 OK 금지; ECU 실행 DONE과 다르다.

**메시지:** `M_VSS_STATUS` — [송수신 색인](../../05_MESSAGE_INDEX.md). 원 wire 객체를 C2/Gateway App 객체로 raw cast 하지 않고 명시 decode/projection 후 호출한다.

<a id="api-domainif_txmobilewindowstate"></a>
## `DomainIf_TxMobileWindowState()`

```c
DomainIf_Status_t DomainIf_TxMobileWindowState(const WindowState_t *state);
```

**방향:** Application→Network · **지위:** DECLARED_SOURCE / BINDING_OPEN / [구현 보류 — 설계 유지].

**용도·호출자·시점:** Domain 기능 owner가 확정한 `M_WINDOW_STATE` 의미 객체를 C2에 제출한다. 등록된 typed TxPort callback이 실제 Network로 인계하고 source ID/품질/AGE를 보존한다.

| 인자 | 정확한 타입 | IN/OUT | 사용·수명 |
|---|---|---|---|
| `state` | [`const WindowState_t *`](11_VEHICLE_TYPES.md#type-windowstate-t) | IN / borrowed | 호출 동안 유효한 `WindowState_t` 객체. 아래 용도와 연결된 원 owner의 M_WINDOW_STATE 의미를 제공한다. [전체 Data](10_DATA.md)의 필드별 품질/식별/AGE를 적용한다. |

**반환·호출:** 로컬 TX 수용 결과. 안정된 복사/encode 전 OK 금지; ECU 실행 DONE과 다르다.

**메시지:** `M_WINDOW_STATE` — [송수신 색인](../../05_MESSAGE_INDEX.md). 원 wire 객체를 C2/Gateway App 객체로 raw cast 하지 않고 명시 decode/projection 후 호출한다.

<a id="api-domainif_txmobilewindowfault"></a>
## `DomainIf_TxMobileWindowFault()`

```c
DomainIf_Status_t DomainIf_TxMobileWindowFault(const DomainIf_WindowFaultState_t *state);
```

**방향:** Application→Network · **지위:** DECLARED_SOURCE / BINDING_OPEN / [구현 보류 — 설계 유지].

**용도·호출자·시점:** Domain 기능 owner가 확정한 `M_WINDOW_FAULT` 의미 객체를 C2에 제출한다. 등록된 typed TxPort callback이 실제 Network로 인계하고 source ID/품질/AGE를 보존한다.

| 인자 | 정확한 타입 | IN/OUT | 사용·수명 |
|---|---|---|---|
| `state` | [`const DomainIf_WindowFaultState_t *`](12_DOMAIN_TYPES.md#type-domainif-windowfaultstate-t) | IN / borrowed | 호출 동안 유효한 `DomainIf_WindowFaultState_t` 객체. 아래 용도와 연결된 원 owner의 M_WINDOW_FAULT 의미를 제공한다. [전체 Data](10_DATA.md)의 필드별 품질/식별/AGE를 적용한다. |

**반환·호출:** 로컬 TX 수용 결과. 안정된 복사/encode 전 OK 금지; ECU 실행 DONE과 다르다.

**메시지:** `M_WINDOW_FAULT` — [송수신 색인](../../05_MESSAGE_INDEX.md). 원 wire 객체를 C2/Gateway App 객체로 raw cast 하지 않고 명시 decode/projection 후 호출한다.

<a id="api-domainif_txmobileusersettings"></a>
## `DomainIf_TxMobileUserSettings()`

```c
DomainIf_Status_t DomainIf_TxMobileUserSettings( const DomainIf_UserSettingsState_t *settings);
```

**방향:** Application→Network · **지위:** DECLARED_SOURCE / BINDING_OPEN.

**용도·호출자·시점:** Domain 기능 owner가 확정한 `M_USER_SETTINGS` 의미 객체를 C2에 제출한다. 등록된 typed TxPort callback이 실제 Network로 인계하고 source ID/품질/AGE를 보존한다.

| 인자 | 정확한 타입 | IN/OUT | 사용·수명 |
|---|---|---|---|
| `settings` | [`const DomainIf_UserSettingsState_t *`](12_DOMAIN_TYPES.md#type-domainif-usersettingsstate-t) | IN / borrowed | 호출 동안 유효한 `DomainIf_UserSettingsState_t` 객체. 아래 용도와 연결된 원 owner의 M_USER_SETTINGS 의미를 제공한다. [전체 Data](10_DATA.md)의 필드별 품질/식별/AGE를 적용한다. |

**반환·호출:** 로컬 TX 수용 결과. 안정된 복사/encode 전 OK 금지; ECU 실행 DONE과 다르다.

**메시지:** `M_USER_SETTINGS` — [송수신 색인](../../05_MESSAGE_INDEX.md). 원 wire 객체를 C2/Gateway App 객체로 raw cast 하지 않고 명시 decode/projection 후 호출한다.

<a id="api-domainif_validatepassivequerycontext"></a>
## `DomainIf_ValidatePassiveQueryContext()`

```c
DomainIf_Status_t DomainIf_ValidatePassiveQueryContext( DeviceContextId_t deviceContextId, SessionId_t sessionId);
```

**방향:** 로컬 관리/검증 · **지위:** DECLARED_SOURCE / BINDING_OPEN.

**용도·호출자·시점:** Network Query 처리기가 prior M_CONTEXT를 먼저반영한 동일직렬 owner 문맥에서 단말/Session을 수동검증한다. 그 뒤 QueryGet을 호출하며 Session 을생성/활성화하지 않는다.

| 인자 | 정확한 타입 | IN/OUT | 사용·수명 |
|---|---|---|---|
| `deviceContextId` | [`DeviceContextId_t`](11_VEHICLE_TYPES.md#type-devicecontextid-t) | IN / value | 검증된 단말 u32. 현재 Gateway peer/session binding과 일치해야 함. |
| `sessionId` | [`SessionId_t`](11_VEHICLE_TYPES.md#type-sessionid-t) | IN / value | 현재 수동 조회 요청 Session u64. QueryGet 전 동일 직렬 owner 문맥에서 검증. |

**반환·호출:** 로컬 snapshot/검증/관리 결과. OK의 범위와 실패 상태는 [공통 호출 계약](#ports)을 따른다.

**순서:** prior M_CONTEXT 반영→passive context 검사→owner Getter. SESSION_READY=0 인 유효한 최초 Query를 거부하지 않는다. 이 호출은 Session을 생성·활성화하지 않고 source TTL·M_CONTEXT 수신 timeout300ms를 재정의하지 않는다.

<a id="api-domainif_querygetbcmdoorstate"></a>
## `DomainIf_QueryGetBcmDoorState()`

```c
DomainIf_Status_t DomainIf_QueryGetBcmDoorState(uint32_t nowMs, DoorState_t *outState);
```

**방향:** Network→Domain snapshot · **지위:** DECLARED_SOURCE / BINDING_OPEN.

**용도·호출자·시점:** Network Query 처리기가 현재 Context 검증 후 기존 `BcmDoorState` owner snapshot을 caller OUT으로 읽는다. 새 원 ECU 요청·새 관측·명령 생성은 하지 않는다.

| 인자 | 정확한 타입 | IN/OUT | 사용·수명 |
|---|---|---|---|
| `nowMs` | `uint32_t` | IN / value | 해당 owner 현재 monotonic u32 ms. Query snapshot/projection의 AGE 계산용이며 생산자 source TTL 정책을 새로 정하지 않는다. |
| `outState` | [`DoorState_t *`](11_VEHICLE_TYPES.md#type-doorstate-t) | OUT / caller-owned | 성공 시 caller-owned outState snapshot. 실패시 OUT은 undefined 이므로 읽지 않는다. |

**반환·호출:** 로컬 snapshot/검증/관리 결과. OK의 범위와 실패 상태는 [공통 호출 계약](#ports)을 따른다.

**snapshot 사용:** 실패한 OUT은 읽지 않는다. [공통 Getter 품질·기록·순서](#query-getters)를 적용한다.

<a id="api-domainif_querygetbcmclimatestate"></a>
## `DomainIf_QueryGetBcmClimateState()`

```c
DomainIf_Status_t DomainIf_QueryGetBcmClimateState(uint32_t nowMs, ClimateState_t *outState);
```

**방향:** Network→Domain snapshot · **지위:** DECLARED_SOURCE / BINDING_OPEN.

**용도·호출자·시점:** Network Query 처리기가 현재 Context 검증 후 기존 `BcmClimateState` owner snapshot을 caller OUT으로 읽는다. 새 원 ECU 요청·새 관측·명령 생성은 하지 않는다.

| 인자 | 정확한 타입 | IN/OUT | 사용·수명 |
|---|---|---|---|
| `nowMs` | `uint32_t` | IN / value | 해당 owner 현재 monotonic u32 ms. Query snapshot/projection의 AGE 계산용이며 생산자 source TTL 정책을 새로 정하지 않는다. |
| `outState` | [`ClimateState_t *`](11_VEHICLE_TYPES.md#type-climatestate-t) | OUT / caller-owned | 성공 시 caller-owned outState snapshot. 실패시 OUT은 undefined 이므로 읽지 않는다. |

**반환·호출:** 로컬 snapshot/검증/관리 결과. OK의 범위와 실패 상태는 [공통 호출 계약](#ports)을 따른다.

**snapshot 사용:** 실패한 OUT은 읽지 않는다. [공통 Getter 품질·기록·순서](#query-getters)를 적용한다.

<a id="api-domainif_querygetbcminteriorlightstate"></a>
## `DomainIf_QueryGetBcmInteriorLightState()`

```c
DomainIf_Status_t DomainIf_QueryGetBcmInteriorLightState(uint32_t nowMs, InteriorLightState_t *outState);
```

**방향:** Network→Domain snapshot · **지위:** DECLARED_SOURCE / BINDING_OPEN.

**용도·호출자·시점:** Network Query 처리기가 현재 Context 검증 후 기존 `BcmInteriorLightState` owner snapshot을 caller OUT으로 읽는다. 새 원 ECU 요청·새 관측·명령 생성은 하지 않는다.

| 인자 | 정확한 타입 | IN/OUT | 사용·수명 |
|---|---|---|---|
| `nowMs` | `uint32_t` | IN / value | 해당 owner 현재 monotonic u32 ms. Query snapshot/projection의 AGE 계산용이며 생산자 source TTL 정책을 새로 정하지 않는다. |
| `outState` | [`InteriorLightState_t *`](11_VEHICLE_TYPES.md#type-interiorlightstate-t) | OUT / caller-owned | 성공 시 caller-owned outState snapshot. 실패시 OUT은 undefined 이므로 읽지 않는다. |

**반환·호출:** 로컬 snapshot/검증/관리 결과. OK의 범위와 실패 상태는 [공통 호출 계약](#ports)을 따른다.

**snapshot 사용:** 실패한 OUT은 읽지 않는다. [공통 Getter 품질·기록·순서](#query-getters)를 적용한다.

<a id="api-domainif_querygetbcmstatus"></a>
## `DomainIf_QueryGetBcmStatus()`

```c
DomainIf_Status_t DomainIf_QueryGetBcmStatus(uint32_t nowMs, DomainIf_BcmStatusReport_t *outState);
```

**방향:** Network→Domain snapshot · **지위:** DECLARED_SOURCE / BINDING_OPEN.

**용도·호출자·시점:** Network Query 처리기가 현재 Context 검증 후 기존 `BcmStatus` owner snapshot을 caller OUT으로 읽는다. 새 원 ECU 요청·새 관측·명령 생성은 하지 않는다.

| 인자 | 정확한 타입 | IN/OUT | 사용·수명 |
|---|---|---|---|
| `nowMs` | `uint32_t` | IN / value | 해당 owner 현재 monotonic u32 ms. Query snapshot/projection의 AGE 계산용이며 생산자 source TTL 정책을 새로 정하지 않는다. |
| `outState` | [`DomainIf_BcmStatusReport_t *`](12_DOMAIN_TYPES.md#type-domainif-bcmstatusreport-t) | OUT / caller-owned | 성공 시 caller-owned outState snapshot. 실패시 OUT은 undefined 이므로 읽지 않는다. |

**반환·호출:** 로컬 snapshot/검증/관리 결과. OK의 범위와 실패 상태는 [공통 호출 계약](#ports)을 따른다.

**snapshot 사용:** 실패한 OUT은 읽지 않는다. [공통 Getter 품질·기록·순서](#query-getters)를 적용한다.

<a id="api-domainif_querygetcisenvironment"></a>
## `DomainIf_QueryGetCisEnvironment()`

```c
DomainIf_Status_t DomainIf_QueryGetCisEnvironment(uint32_t nowMs, CisCabinEnvironmentState_t *outState);
```

**방향:** Network→Domain snapshot · **지위:** DECLARED_SOURCE / BINDING_OPEN.

**용도·호출자·시점:** Network Query 처리기가 현재 Context 검증 후 기존 `CisEnvironment` owner snapshot을 caller OUT으로 읽는다. 새 원 ECU 요청·새 관측·명령 생성은 하지 않는다.

| 인자 | 정확한 타입 | IN/OUT | 사용·수명 |
|---|---|---|---|
| `nowMs` | `uint32_t` | IN / value | 해당 owner 현재 monotonic u32 ms. Query snapshot/projection의 AGE 계산용이며 생산자 source TTL 정책을 새로 정하지 않는다. |
| `outState` | [`CisCabinEnvironmentState_t *`](11_VEHICLE_TYPES.md#type-ciscabinenvironmentstate-t) | OUT / caller-owned | 성공 시 caller-owned outState snapshot. 실패시 OUT은 undefined 이므로 읽지 않는다. |

**반환·호출:** 로컬 snapshot/검증/관리 결과. OK의 범위와 실패 상태는 [공통 호출 계약](#ports)을 따른다.

**snapshot 사용:** 실패한 OUT은 읽지 않는다. [공통 Getter 품질·기록·순서](#query-getters)를 적용한다.

<a id="api-domainif_querygetcisoccupant"></a>
## `DomainIf_QueryGetCisOccupant()`

```c
DomainIf_Status_t DomainIf_QueryGetCisOccupant(uint32_t nowMs, CisOccupantState_t *outState);
```

**방향:** Network→Domain snapshot · **지위:** DECLARED_SOURCE / BINDING_OPEN.

**용도·호출자·시점:** Network Query 처리기가 현재 Context 검증 후 기존 `CisOccupant` owner snapshot을 caller OUT으로 읽는다. 새 원 ECU 요청·새 관측·명령 생성은 하지 않는다.

| 인자 | 정확한 타입 | IN/OUT | 사용·수명 |
|---|---|---|---|
| `nowMs` | `uint32_t` | IN / value | 해당 owner 현재 monotonic u32 ms. Query snapshot/projection의 AGE 계산용이며 생산자 source TTL 정책을 새로 정하지 않는다. |
| `outState` | [`CisOccupantState_t *`](11_VEHICLE_TYPES.md#type-cisoccupantstate-t) | OUT / caller-owned | 성공 시 caller-owned outState snapshot. 실패시 OUT은 undefined 이므로 읽지 않는다. |

**반환·호출:** 로컬 snapshot/검증/관리 결과. OK의 범위와 실패 상태는 [공통 호출 계약](#ports)을 따른다.

**snapshot 사용:** 실패한 OUT은 읽지 않는다. [공통 Getter 품질·기록·순서](#query-getters)를 적용한다.

<a id="api-domainif_querygetcisrear"></a>
## `DomainIf_QueryGetCisRear()`

```c
DomainIf_Status_t DomainIf_QueryGetCisRear(uint32_t nowMs, CisRearState_t *outState);
```

**방향:** Network→Domain snapshot · **지위:** DECLARED_SOURCE / BINDING_OPEN.

**용도·호출자·시점:** Network Query 처리기가 현재 Context 검증 후 기존 `CisRear` owner snapshot을 caller OUT으로 읽는다. 새 원 ECU 요청·새 관측·명령 생성은 하지 않는다.

| 인자 | 정확한 타입 | IN/OUT | 사용·수명 |
|---|---|---|---|
| `nowMs` | `uint32_t` | IN / value | 해당 owner 현재 monotonic u32 ms. Query snapshot/projection의 AGE 계산용이며 생산자 source TTL 정책을 새로 정하지 않는다. |
| `outState` | [`CisRearState_t *`](11_VEHICLE_TYPES.md#type-cisrearstate-t) | OUT / caller-owned | 성공 시 caller-owned outState snapshot. 실패시 OUT은 undefined 이므로 읽지 않는다. |

**반환·호출:** 로컬 snapshot/검증/관리 결과. OK의 범위와 실패 상태는 [공통 호출 계약](#ports)을 따른다.

**snapshot 사용:** 실패한 OUT은 읽지 않는다. [공통 Getter 품질·기록·순서](#query-getters)를 적용한다.

<a id="api-domainif_querygetcisstatus"></a>
## `DomainIf_QueryGetCisStatus()`

```c
DomainIf_Status_t DomainIf_QueryGetCisStatus(uint32_t nowMs, DomainIf_CisStatusReport_t *outState);
```

**방향:** Network→Domain snapshot · **지위:** DECLARED_SOURCE / BINDING_OPEN.

**용도·호출자·시점:** Network Query 처리기가 현재 Context 검증 후 기존 `CisStatus` owner snapshot을 caller OUT으로 읽는다. 새 원 ECU 요청·새 관측·명령 생성은 하지 않는다.

| 인자 | 정확한 타입 | IN/OUT | 사용·수명 |
|---|---|---|---|
| `nowMs` | `uint32_t` | IN / value | 해당 owner 현재 monotonic u32 ms. Query snapshot/projection의 AGE 계산용이며 생산자 source TTL 정책을 새로 정하지 않는다. |
| `outState` | [`DomainIf_CisStatusReport_t *`](12_DOMAIN_TYPES.md#type-domainif-cisstatusreport-t) | OUT / caller-owned | 성공 시 caller-owned outState snapshot. 실패시 OUT은 undefined 이므로 읽지 않는다. |

**반환·호출:** 로컬 snapshot/검증/관리 결과. OK의 범위와 실패 상태는 [공통 호출 계약](#ports)을 따른다.

**snapshot 사용:** 실패한 OUT은 읽지 않는다. [공통 Getter 품질·기록·순서](#query-getters)를 적용한다.

<a id="api-domainif_querygetvssstatus"></a>
## `DomainIf_QueryGetVssStatus()`

```c
DomainIf_Status_t DomainIf_QueryGetVssStatus(uint32_t nowMs, DomainIf_VssStateReport_t *outState);
```

**방향:** Network→Domain snapshot · **지위:** DECLARED_SOURCE / BINDING_OPEN.

**용도·호출자·시점:** Network Query 처리기가 현재 Context 검증 후 기존 `VssStatus` owner snapshot을 caller OUT으로 읽는다. 새 원 ECU 요청·새 관측·명령 생성은 하지 않는다.

| 인자 | 정확한 타입 | IN/OUT | 사용·수명 |
|---|---|---|---|
| `nowMs` | `uint32_t` | IN / value | 해당 owner 현재 monotonic u32 ms. Query snapshot/projection의 AGE 계산용이며 생산자 source TTL 정책을 새로 정하지 않는다. |
| `outState` | [`DomainIf_VssStateReport_t *`](12_DOMAIN_TYPES.md#type-domainif-vssstatereport-t) | OUT / caller-owned | 성공 시 caller-owned outState snapshot. 실패시 OUT은 undefined 이므로 읽지 않는다. |

**반환·호출:** 로컬 snapshot/검증/관리 결과. OK의 범위와 실패 상태는 [공통 호출 계약](#ports)을 따른다.

**snapshot 사용:** 실패한 OUT은 읽지 않는다. [공통 Getter 품질·기록·순서](#query-getters)를 적용한다.

<a id="api-domainif_querygetwindowstate"></a>
## `DomainIf_QueryGetWindowState()`

```c
DomainIf_Status_t DomainIf_QueryGetWindowState(uint32_t nowMs, WindowState_t *outState);
```

**방향:** Network→Domain snapshot · **지위:** DECLARED_SOURCE / BINDING_OPEN / [구현 보류 — 설계 유지].

**용도·호출자·시점:** Network Query 처리기가 현재 Context 검증 후 기존 `WindowState` owner snapshot을 caller OUT으로 읽는다. 새 원 ECU 요청·새 관측·명령 생성은 하지 않는다.

| 인자 | 정확한 타입 | IN/OUT | 사용·수명 |
|---|---|---|---|
| `nowMs` | `uint32_t` | IN / value | 해당 owner 현재 monotonic u32 ms. Query snapshot/projection의 AGE 계산용이며 생산자 source TTL 정책을 새로 정하지 않는다. |
| `outState` | [`WindowState_t *`](11_VEHICLE_TYPES.md#type-windowstate-t) | OUT / caller-owned | 성공 시 caller-owned outState snapshot. 실패시 OUT은 undefined 이므로 읽지 않는다. |

**반환·호출:** 로컬 snapshot/검증/관리 결과. OK의 범위와 실패 상태는 [공통 호출 계약](#ports)을 따른다.

**snapshot 사용:** 실패한 OUT은 읽지 않는다. [공통 Getter 품질·기록·순서](#query-getters)를 적용한다.

<a id="api-domainif_querygetwindowfault"></a>
## `DomainIf_QueryGetWindowFault()`

```c
DomainIf_Status_t DomainIf_QueryGetWindowFault(uint32_t nowMs, DomainIf_WindowFaultState_t *outState);
```

**방향:** Network→Domain snapshot · **지위:** DECLARED_SOURCE / BINDING_OPEN / [구현 보류 — 설계 유지].

**용도·호출자·시점:** Network Query 처리기가 현재 Context 검증 후 기존 `WindowFault` owner snapshot을 caller OUT으로 읽는다. 새 원 ECU 요청·새 관측·명령 생성은 하지 않는다.

| 인자 | 정확한 타입 | IN/OUT | 사용·수명 |
|---|---|---|---|
| `nowMs` | `uint32_t` | IN / value | 해당 owner 현재 monotonic u32 ms. Query snapshot/projection의 AGE 계산용이며 생산자 source TTL 정책을 새로 정하지 않는다. |
| `outState` | [`DomainIf_WindowFaultState_t *`](12_DOMAIN_TYPES.md#type-domainif-windowfaultstate-t) | OUT / caller-owned | 성공 시 caller-owned outState snapshot. 실패시 OUT은 undefined 이므로 읽지 않는다. |

**반환·호출:** 로컬 snapshot/검증/관리 결과. OK의 범위와 실패 상태는 [공통 호출 계약](#ports)을 따른다.

**snapshot 사용:** 실패한 OUT은 읽지 않는다. [공통 Getter 품질·기록·순서](#query-getters)를 적용한다.

<a id="api-domainif_querygetusersettings"></a>
## `DomainIf_QueryGetUserSettings()`

```c
DomainIf_Status_t DomainIf_QueryGetUserSettings( uint32_t nowMs, DomainIf_UserSettingsState_t *outSettings);
```

**방향:** Network→Domain snapshot · **지위:** DECLARED_SOURCE / BINDING_OPEN.

**용도·호출자·시점:** Network Query 처리기가 현재 Context 검증 후 기존 `UserSettings` owner snapshot을 caller OUT으로 읽는다. 새 원 ECU 요청·새 관측·명령 생성은 하지 않는다.

| 인자 | 정확한 타입 | IN/OUT | 사용·수명 |
|---|---|---|---|
| `nowMs` | `uint32_t` | IN / value | 해당 owner 현재 monotonic u32 ms. Query snapshot/projection의 AGE 계산용이며 생산자 source TTL 정책을 새로 정하지 않는다. |
| `outSettings` | [`DomainIf_UserSettingsState_t *`](12_DOMAIN_TYPES.md#type-domainif-usersettingsstate-t) | OUT / caller-owned | 성공 시 caller-owned outSettings snapshot. 실패시 OUT은 undefined 이므로 읽지 않는다. |

**반환·호출:** 로컬 snapshot/검증/관리 결과. OK의 범위와 실패 상태는 [공통 호출 계약](#ports)을 따른다.

**snapshot 사용:** 실패한 OUT은 읽지 않는다. [공통 Getter 품질·기록·순서](#query-getters)를 적용한다.

<a id="api-domainif_querygetfunctionavailability"></a>
## `DomainIf_QueryGetFunctionAvailability()`

```c
DomainIf_Status_t DomainIf_QueryGetFunctionAvailability( uint32_t nowMs, DomainIf_FunctionAvailabilitySet_t *outAvailability);
```

**방향:** Network→Domain snapshot · **지위:** DECLARED_SOURCE / BINDING_OPEN.

**용도·호출자·시점:** Network Query 처리기가 현재 Context 검증 후 기존 `FunctionAvailability` owner snapshot을 caller OUT으로 읽는다. 새 원 ECU 요청·새 관측·명령 생성은 하지 않는다.

| 인자 | 정확한 타입 | IN/OUT | 사용·수명 |
|---|---|---|---|
| `nowMs` | `uint32_t` | IN / value | 해당 owner 현재 monotonic u32 ms. Query snapshot/projection의 AGE 계산용이며 생산자 source TTL 정책을 새로 정하지 않는다. |
| `outAvailability` | [`DomainIf_FunctionAvailabilitySet_t *`](12_DOMAIN_TYPES.md#type-domainif-functionavailabilityset-t) | OUT / caller-owned | 성공 시 caller-owned outAvailability snapshot. 실패시 OUT은 undefined 이므로 읽지 않는다. |

**반환·호출:** 로컬 snapshot/검증/관리 결과. OK의 범위와 실패 상태는 [공통 호출 계약](#ports)을 따른다.

**snapshot 사용:** 실패한 OUT은 읽지 않는다. [공통 Getter 품질·기록·순서](#query-getters)를 적용한다.

<a id="api-domainif_querygetdigitalkeystate"></a>
## `DomainIf_QueryGetDigitalKeyState()`

```c
DomainIf_Status_t DomainIf_QueryGetDigitalKeyState( uint32_t nowMs, DigitalKeyState_t *outState);
```

**방향:** Network→Domain snapshot · **지위:** DECLARED_SOURCE / BINDING_OPEN.

**용도·호출자·시점:** Network Query 처리기가 현재 Context 검증 후 기존 `DigitalKeyState` owner snapshot을 caller OUT으로 읽는다. 새 원 ECU 요청·새 관측·명령 생성은 하지 않는다.

| 인자 | 정확한 타입 | IN/OUT | 사용·수명 |
|---|---|---|---|
| `nowMs` | `uint32_t` | IN / value | 해당 owner 현재 monotonic u32 ms. Query snapshot/projection의 AGE 계산용이며 생산자 source TTL 정책을 새로 정하지 않는다. |
| `outState` | [`DigitalKeyState_t *`](11_VEHICLE_TYPES.md#type-digitalkeystate-t) | OUT / caller-owned | 성공 시 caller-owned outState snapshot. 실패시 OUT은 undefined 이므로 읽지 않는다. |

**반환·호출:** 로컬 snapshot/검증/관리 결과. OK의 범위와 실패 상태는 [공통 호출 계약](#ports)을 따른다.

**snapshot 사용:** 실패한 OUT은 읽지 않는다. [공통 Getter 품질·기록·순서](#query-getters)를 적용한다.

<a id="api-domainif_querygetcurrentwarning"></a>
## `DomainIf_QueryGetCurrentWarning()`

```c
DomainIf_Status_t DomainIf_QueryGetCurrentWarning( DomainWarningType_t type, uint32_t nowMs, DomainIf_WarningInfo_t *outWarning);
```

**방향:** Network→Domain snapshot · **지위:** DECLARED_SOURCE / BINDING_OPEN.

**용도·호출자·시점:** Network Query 처리기가 현재 Context 검증 후 기존 `CurrentWarning` owner snapshot을 caller OUT으로 읽는다. 새 원 ECU 요청·새 관측·명령 생성은 하지 않는다.

| 인자 | 정확한 타입 | IN/OUT | 사용·수명 |
|---|---|---|---|
| `type` | [`DomainWarningType_t`](12_DOMAIN_TYPES.md#type-domainwarningtype-t) | IN / value | 조회할 Domain 경고 8 종. COUNT 센티널/잘못된 enum 거부. |
| `nowMs` | `uint32_t` | IN / value | 해당 owner 현재 monotonic u32 ms. Query snapshot/projection의 AGE 계산용이며 생산자 source TTL 정책을 새로 정하지 않는다. |
| `outWarning` | [`DomainIf_WarningInfo_t *`](12_DOMAIN_TYPES.md#type-domainif-warninginfo-t) | OUT / caller-owned | 성공 시 caller-owned outWarning snapshot. 실패시 OUT은 undefined 이므로 읽지 않는다. |

**반환·호출:** 로컬 snapshot/검증/관리 결과. OK의 범위와 실패 상태는 [공통 호출 계약](#ports)을 따른다.

**snapshot 사용:** 실패한 OUT은 읽지 않는다. [공통 Getter 품질·기록·순서](#query-getters)를 적용한다.

<a id="api-domainif_querygetrequestresult"></a>
## `DomainIf_QueryGetRequestResult()`

```c
DomainIf_Status_t DomainIf_QueryGetRequestResult( DeviceContextId_t requestingDevice, const RequestContext_t *targetRequest, uint32_t nowMs, DomainIf_MobileRequestResult_t *outResult, bool *outHasRecord);
```

**방향:** Network→Domain snapshot · **지위:** DECLARED_SOURCE / BINDING_OPEN.

**용도·호출자·시점:** Network Query 처리기가 현재 Context 검증 후 기존 `RequestResult` owner snapshot을 caller OUT으로 읽는다. 새 원 ECU 요청·새 관측·명령 생성은 하지 않는다.

| 인자 | 정확한 타입 | IN/OUT | 사용·수명 |
|---|---|---|---|
| `requestingDevice` | [`DeviceContextId_t`](11_VEHICLE_TYPES.md#type-devicecontextid-t) | IN / value | 조회 수신 단말. 다른 단말의 기록을 반환하지 않도록 owner 접근 계약 적용. |
| `targetRequest` | [`const RequestContext_t *`](11_VEHICLE_TYPES.md#type-requestcontext-t) | IN / borrowed | 조회 대상의 원 Session/Request. 현재 Session과 다를 수 있고 requestingDevice 권한과 대조. |
| `nowMs` | `uint32_t` | IN / value | 해당 owner 현재 monotonic u32 ms. Query snapshot/projection의 AGE 계산용이며 생산자 source TTL 정책을 새로 정하지 않는다. |
| `outResult` | [`DomainIf_MobileRequestResult_t *`](12_DOMAIN_TYPES.md#type-domainif-mobilerequestresult-t) | OUT / caller-owned | 성공 시 caller-owned outResult snapshot. 실패시 OUT은 undefined 이므로 읽지 않는다. |
| `outHasRecord` | `bool *` | OUT / caller-owned | 성공 시 결과 기록 존재 여부 bool. false 이면 OUT result는 차량 결과로 소비하지 않는다. |

**반환·호출:** 로컬 snapshot/검증/관리 결과. OK의 범위와 실패 상태는 [공통 호출 계약](#ports)을 따른다.

**snapshot 사용:** 실패한 OUT은 읽지 않는다. [공통 Getter 품질·기록·순서](#query-getters)를 적용한다.

<a id="api-domainif_querygetrecentrequestresults"></a>
## `DomainIf_QueryGetRecentRequestResults()`

```c
DomainIf_Status_t DomainIf_QueryGetRecentRequestResults( DeviceContextId_t requestingDevice, uint32_t nowMs, DomainIf_MobileRequestResult_t *outResults, size_t capacity, size_t *outCount);
```

**방향:** Network→Domain snapshot · **지위:** DECLARED_SOURCE / BINDING_OPEN.

**용도·호출자·시점:** Network Query 처리기가 현재 Context 검증 후 기존 `RecentRequestResults` owner snapshot을 caller OUT으로 읽는다. 새 원 ECU 요청·새 관측·명령 생성은 하지 않는다.

| 인자 | 정확한 타입 | IN/OUT | 사용·수명 |
|---|---|---|---|
| `requestingDevice` | [`DeviceContextId_t`](11_VEHICLE_TYPES.md#type-devicecontextid-t) | IN / value | 조회 수신 단말. 다른 단말의 기록을 반환하지 않도록 owner 접근 계약 적용. |
| `nowMs` | `uint32_t` | IN / value | 해당 owner 현재 monotonic u32 ms. Query snapshot/projection의 AGE 계산용이며 생산자 source TTL 정책을 새로 정하지 않는다. |
| `outResults` | [`DomainIf_MobileRequestResult_t *`](12_DOMAIN_TYPES.md#type-domainif-mobilerequestresult-t) | OUT / caller-owned | 성공 시 caller-owned outResults snapshot. 실패시 OUT은 undefined 이므로 읽지 않는다. |
| `capacity` | `size_t` | IN / value | caller-owned outResults의 실제 원소 수 size_t. 0/공간 부족 처리 source binding 검증 필요; 초과 write 금지. |
| `outCount` | `size_t *` | OUT / caller-owned | 성공 시 반환된 결과 원소 수. failure 시 다른 OUT과 같이 사용 금지. |

**반환·호출:** 로컬 snapshot/검증/관리 결과. OK의 범위와 실패 상태는 [공통 호출 계약](#ports)을 따른다.

**snapshot 사용:** 실패한 OUT은 읽지 않는다. [공통 Getter 품질·기록·순서](#query-getters)를 적용한다.

<a id="api-domainif_querygetlastdigitalkeyresult"></a>
## `DomainIf_QueryGetLastDigitalKeyResult()`

```c
DomainIf_Status_t DomainIf_QueryGetLastDigitalKeyResult( DeviceContextId_t requestingDevice, uint32_t nowMs, DomainIf_DigitalKeyResult_t *outResult, bool *outHasRecord);
```

**방향:** Network→Domain snapshot · **지위:** DECLARED_SOURCE / BINDING_OPEN.

**용도·호출자·시점:** Network Query 처리기가 현재 Context 검증 후 기존 `LastDigitalKeyResult` owner snapshot을 caller OUT으로 읽는다. 새 원 ECU 요청·새 관측·명령 생성은 하지 않는다.

| 인자 | 정확한 타입 | IN/OUT | 사용·수명 |
|---|---|---|---|
| `requestingDevice` | [`DeviceContextId_t`](11_VEHICLE_TYPES.md#type-devicecontextid-t) | IN / value | 조회 수신 단말. 다른 단말의 기록을 반환하지 않도록 owner 접근 계약 적용. |
| `nowMs` | `uint32_t` | IN / value | 해당 owner 현재 monotonic u32 ms. Query snapshot/projection의 AGE 계산용이며 생산자 source TTL 정책을 새로 정하지 않는다. |
| `outResult` | [`DomainIf_DigitalKeyResult_t *`](12_DOMAIN_TYPES.md#type-domainif-digitalkeyresult-t) | OUT / caller-owned | 성공 시 caller-owned outResult snapshot. 실패시 OUT은 undefined 이므로 읽지 않는다. |
| `outHasRecord` | `bool *` | OUT / caller-owned | 성공 시 결과 기록 존재 여부 bool. false 이면 OUT result는 차량 결과로 소비하지 않는다. |

**반환·호출:** 로컬 snapshot/검증/관리 결과. OK의 범위와 실패 상태는 [공통 호출 계약](#ports)을 따른다.

**snapshot 사용:** 실패한 OUT은 읽지 않는다. [공통 Getter 품질·기록·순서](#query-getters)를 적용한다.

<a id="api-domainif_getnetworkhealthsnapshot"></a>
## `DomainIf_GetNetworkHealthSnapshot()`

```c
DomainIf_Status_t DomainIf_GetNetworkHealthSnapshot( NetworkLinkId_t link, uint32_t nowMs, NetworkHealthSnapshot_t *outSnapshot);
```

**방향:** 로컬 관리/검증 · **지위:** DECLARED_SOURCE / BINDING_OPEN.

**용도·호출자·시점:** Domain CommunicationMonitor/DiagnosticManager가 링크로컬 상태·마지막유효 Rx AGE·운영 counter를 조회한다. 링크 UNAVAILABLE 여도 snapshot 자체는 OK 일수있다.

| 인자 | 정확한 타입 | IN/OUT | 사용·수명 |
|---|---|---|---|
| `link` | [`NetworkLinkId_t`](12_DOMAIN_TYPES.md#type-networklinkid-t) | IN / value | 현재 Domain Network의 CAN/UART_M 등 링크. Pi 직결 UART-C가 없으므로 unsupported 처리. |
| `nowMs` | `uint32_t` | IN / value | 현재 monotonic u32 ms. 링크 lastValidRxAgeMs와 운영 snapshot 경과를 계산한다. Query 용 생산자 TTL 재정의와 무관. |
| `outSnapshot` | [`NetworkHealthSnapshot_t *`](12_DOMAIN_TYPES.md#type-networkhealthsnapshot-t) | OUT / caller-owned | 성공 시 caller-owned outSnapshot snapshot. 실패시 OUT은 undefined 이므로 읽지 않는다. |

**반환·호출:** 로컬 snapshot/검증/관리 결과. OK의 범위와 실패 상태는 [공통 호출 계약](#ports)을 따른다.

<a id="api-domainif_rxnetworklocalfailure"></a>
## `DomainIf_RxNetworkLocalFailure()`

```c
DomainIf_Status_t DomainIf_RxNetworkLocalFailure(const NetworkLocalFailure_t *failure);
```

**방향:** Network→C2 · **지위:** DECLARED_SOURCE / BINDING_OPEN.

**용도·호출자·시점:** Network가 local OK 후 identity 관련 최종실패가 발생했을때 기존 필수 담당 모듈/supervision sink에 전달한다. 이미 DISPATCHED 한명령을 AbortBeforeDispatch 로되돌리거나 차량 FAILED 를만들지 않는다.

| 인자 | 정확한 타입 | IN/OUT | 사용·수명 |
|---|---|---|---|
| `failure` | [`const NetworkLocalFailure_t *`](12_DOMAIN_TYPES.md#type-networklocalfailure-t) | IN / borrowed | 로컬 성공 admission 이후 발생한 identity가 있는 최종 실패 근거. 기존 필수 sink에 인계; vehicle FAILED/AbortBeforeDispatch 합성 금지. |

**반환·호출:** 로컬 C2 입력 처리 결과. Network 수신 및 Core commit/observer 계약은 [공통 호출 계약](#ports)을 따른다.

**필수 sink:** 기존 Domain owner/supervision에 인계하여 identity를 소비해야 한다. optional observer만 sole sink로 사용 금지. sync reject/CRC 거부/일반 periodic loss는 이 callback 대상 아님. 이미 DISPATCHED 된 Command에 AbortBeforeDispatch 또는 차량 FAILED를 합성하지 않는다.
