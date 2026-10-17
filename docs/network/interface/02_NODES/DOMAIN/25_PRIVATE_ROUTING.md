# DOMAIN — typed Port·수신 투영·Query 내부 경로

Interface v2 · r1 · [규격과 연동 조건](../../07_CROSS_NODE_INTERFACE_RULES.md#status)

**PROPOSED_DESIGN / BINDING_OPEN.** 여기에 있는 `Net...Priv` 함수는 필요한 Network 내부 경계 후보다. Application의 새 공개 호출 의무가 아니며 펌웨어 심벌·Codec 구현의 존재를 주장하지 않는다.

[Data](10_DATA.md) · [공개 API](20_API.md) · [공통 API·실패](../../04_COMMON/20_API_USAGE_AND_FAILURE.md) · [메시지 48](../../05_MESSAGE_INDEX.md)

## 내부 경계부터 찾기

| 실제 호출자 → 소비자 | 원형·연결·조건 |
|---|---|
| C2 TxPort → Network encode | [기존 26 callback·메시지](#tx-port), [반환 변환](20_API.md#ports); App은 C2 TX와 Adapter를 중복 호출하지 않는다. |
| CAN/UART Codec → C2 ingress | [수신 투영](#rx-projection), [같은 원 CAN↔accepted Snapshot 연결](#r2-origin-join); 원 BOOT/TX/AGE가 확인되지 않은 모바일 투영만 제한한다. |
| Query 처리 → owner Getter → 기존 encoder | [ProcessQuery 원형](#api-domainnetpriv_processquery), [Query 문맥·순서](#query-flow), [TX 완료·부분·0건 기준](#r2-query-producer) |
| Query 감독/링크 감독 → Network 송신 | [SendQueryEnd](#api-domainnetpriv_sendqueryend), [PublishAlive](#api-domainnetpriv_publishalive); 차량 App 명령 결과로 공개하지 않는다. |

## 내부 함수 원형


<a id="api-domainnet_publishpermission"></a>
### `DomainNet_PublishPermission()`

```c
NetSubmitResult DomainNet_PublishPermission(const NetDomainPermission *message);
```

**분류·호출 주체:** Network 내부 Permission composer가 기존 C2 허용 입력을 일관되게 읽어 구성한 Snapshot을 제출하는 경계다(PROPOSED_DESIGN/BINDING_OPEN). Application에 C2 허용 TX와 이 함수를 함께 호출할 의무를 만들지 않는다.

**소비·조건:** 단일 `DOMAIN_PERMISSION`을 BCM/CIS/VSS/WINDOW 4개 수신 경계로 보낸다. 개별 품질·보존 근거 가운데 가장 오래된 원본의 AGE·operation의 필수 입력/기한은 [Permission 합성 정본](../../03_COMMUNICATION/10_CAN_FD.md#message-domain-permission)을 따른다. 실제 composer/Port 등록 및 기능 정책은 OPEN이다.

| 인자 | 정확한 타입 | 사용·수명 |
|---|---|---|
| `message` | `const NetDomainPermission *` | 검증된 DOMAIN_PERMISSION typed 객체. 호출 동안 borrowed; 반환 후 보관은 원 필드 전체 안정된 복사. |

**반환:** Network 로컬 접수 결과 4종. `NET_SUBMIT_OK`는 원격 수신 또는 요청 DONE 아님.

<a id="api-domainnetpriv_onwindowevent"></a>
### `DomainNetPriv_OnWindowEvent()`

```c
DomainIf_Status_t DomainNetPriv_OnWindowEvent(const NetWindowEvent *message, const NetCanRxMeta *meta);
```

**호출·소비:** END·AntiPinch를 타입이 명시된 원본 사건으로 그대로 보존. AntiPinch C2 변환과 END owner 연결은 별도 OPEN. **메시지:** `WINDOW_EVENT`.

| 인자 | 정확한 타입 | 사용·수명 |
|---|---|---|
| `message` | `const NetWindowEvent *` | 검증된 WINDOW_EVENT typed 객체. 호출 동안 borrowed; 반환 후 보관은 원 필드 전체 안정된 복사. |
| `meta` | `const NetCanRxMeta *` | Network 생성 수신 메타. CRC/길이/enum/현재 세대 검증 후 사용; source와 링크순번을 구별. |

**반환:** 로컬 C2 인계/검증 결과. 추정한 차량 결과를 반환하지 않으며 수정하지 않는 원본 객체를 보존한다.

**[구현 보류 — 설계 유지] / BINDING_OPEN:** occurrence·원 AGE·원 세대·관련 command·END/Fault owner binding이 확정되기 전 좁은 C2 투영으로 원본 근거를 삭제하지 않는다.

<a id="api-domainnetpriv_onwindowfault"></a>
### `DomainNetPriv_OnWindowFault()`

```c
DomainIf_Status_t DomainNetPriv_OnWindowFault(const NetWindowFault *message, const NetCanRxMeta *meta);
```

**호출·소비:** FaultInfo만 전달하면 발생 식별·경과 시간·세대 정보가 손실된다. 원 객체 보존 후 RxWindowFault의 별도 투영. **메시지:** `WINDOW_FAULT`.

| 인자 | 정확한 타입 | 사용·수명 |
|---|---|---|
| `message` | `const NetWindowFault *` | 검증된 WINDOW_FAULT typed 객체. 호출 동안 borrowed; 반환 후 보관은 원 필드 전체 안정된 복사. |
| `meta` | `const NetCanRxMeta *` | Network 생성 수신 메타. CRC/길이/enum/현재 세대 검증 후 사용; source와 링크순번을 구별. |

**반환:** 로컬 C2 인계/검증 결과. 추정한 차량 결과를 반환하지 않으며 수정하지 않는 원본 객체를 보존한다.

**[구현 보류 — 설계 유지] / BINDING_OPEN:** occurrence·원 AGE·원 세대·관련 command·END/Fault owner binding이 확정되기 전 좁은 C2 투영으로 원본 근거를 삭제하지 않는다.

<a id="api-domainnetpriv_onvssstatus"></a>
### `DomainNetPriv_OnVssStatus()`

```c
DomainIf_Status_t DomainNetPriv_OnVssStatus(const NetVssStatus *message, const NetCanRxMeta *meta);
```

**호출·소비:** 기존 VSS INPUT 한 곳의 완전 Snapshot→NetVssStatus 생산(A2 선택) 뒤 DOMAIN Network가 VSS_STATUS→C2 State/Fault를 명시 투영한다. 원 K·원 AGE·전체 자료를 보호하고 State/Fault의 미호출·원 반환·수용을 독립 기록한다. 이미 수용한 State는 Fault 실패로 재실행하지 않는다. 설치 프로필·동일 입력의 연결 관계·과거 K 차단 경계는 E-S05 실제 검증 대기다. **메시지:** `VSS_STATUS`.

| 인자 | 정확한 타입 | 사용·수명 |
|---|---|---|
| `message` | `const NetVssStatus *` | 검증된 VSS_STATUS typed 객체. 호출 동안 borrowed; 반환 후 보관은 원 필드 전체 안정된 복사. |
| `meta` | `const NetCanRxMeta *` | Network 생성 수신 메타. CRC/길이/enum/현재 세대 검증 후 사용; source와 링크순번을 구별. |

**반환:** 로컬 C2 인계/검증 결과. 추정한 차량 결과를 반환하지 않으며 immutable 원 객체를 보존한다.

<a id="api-domainnetpriv_ongatewaycontext"></a>
### `DomainNetPriv_OnGatewayContext()`

```c
DomainIf_Status_t DomainNetPriv_OnGatewayContext(const NetMobileContext *message, const NetUartRxMeta *meta);
```

**호출·소비:** M_CONTEXT를 두 C2 ingress로 명시 투영; 등록·연결 공동 meta/앱/근접 독립. **메시지:** `M_CONTEXT`.

| 인자 | 정확한 타입 | 사용·수명 |
|---|---|---|
| `message` | `const NetMobileContext *` | 검증된 M_CONTEXT typed 객체. 호출 동안 borrowed; 반환 후 보관은 원 필드 전체 안정된 복사. |
| `meta` | `const NetUartRxMeta *` | Network 생성 수신 메타. CRC/길이/enum/현재 세대 검증 후 사용; source와 링크순번을 구별. |

**반환:** 로컬 C2 인계/검증 결과. 추정한 차량 결과를 반환하지 않으며 immutable 원 객체를 보존한다.

<a id="api-domainnetpriv_processquery"></a>
### `DomainNetPriv_ProcessQuery()`

```c
DomainIf_Status_t DomainNetPriv_ProcessQuery(const NetMobileQuery *message, const NetUartRxMeta *meta);
```

**호출·소비:** Query는 Network-private; 수동 Session 검증→18개 Snapshot Getter 호출→타입별 공통 인코더. **메시지:** `M_QUERY`.

| 인자 | 정확한 타입 | 사용·수명 |
|---|---|---|
| `message` | `const NetMobileQuery *` | 검증된 M_QUERY 타입이 명시된 객체. 호출 중에만 참조한다. 반환 이후 보관할 경우에는 원 필드 전체를 안정된 저장소에 복사한다. |
| `meta` | `const NetUartRxMeta *` | Network 생성 수신 메타. CRC/길이/enum/현재 세대 검증 후 사용; source와 링크순번을 구별. |

**반환:** 로컬 C2 인계/검증 결과. 추정한 차량 결과를 반환하지 않으며 immutable 원 객체를 보존한다.

<a id="api-domainnetpriv_publishalive"></a>
### `DomainNetPriv_PublishAlive()`

```c
NetSubmitResult DomainNetPriv_PublishAlive(const NetMobileDomainAlive *message);
```

**호출·소비:** 링크 감독 메시지 생성/주기 전송. 차량 App 공개 API 필요 없음. **메시지:** `M_DOMAIN_ALIVE`.

| 인자 | 정확한 타입 | 사용·수명 |
|---|---|---|
| `message` | `const NetMobileDomainAlive *` | 검증된 M_DOMAIN_ALIVE typed 객체. 호출 동안 borrowed; 반환 후 보관은 원 필드 전체 안정된 복사. |

**반환:** Network 로컬 접수 결과 4종. `NET_SUBMIT_OK`는 원격 수신 또는 요청 DONE 아님.

<a id="api-domainnetpriv_sendqueryend"></a>
### `DomainNetPriv_SendQueryEnd()`

```c
NetSubmitResult DomainNetPriv_SendQueryEnd(const NetMobileQueryEnd *message);
```

**호출·소비:** Query ID/scope/item count 종결 전용. 차량 요청 DONE 아님. **메시지:** `M_QUERY_END`.

| 인자 | 정확한 타입 | 사용·수명 |
|---|---|---|
| `message` | `const NetMobileQueryEnd *` | 검증된 M_QUERY_END 타입이 명시된 객체. 호출 중에만 참조한다. 반환 이후 보관할 경우에는 원 필드 전체를 안정된 저장소에 복사한다. |

**반환:** Network 로컬 접수 결과 4종. `NET_SUBMIT_OK`는 원격 수신 또는 요청 DONE 아님.

<a id="tx-port"></a>
## TxPort26 종 — 기존 callback 선언을 실제 인계 경계로 사용

기존 원본 선언의 `DomainIf_TxPort_t`의 정확한 전체 선언은 [C2 Data](12_DOMAIN_TYPES.md#type-domainif-txport-t)에 있다. 다음 callback을 등록하면 C2 TX가 한 경로로 Network에 도달한다. 별도 `DomainNet_Send*` 공개 API를 Application에 함께 호출하게 하지 않는다. callback은 source 반환 `DomainIf_Status_t`를 유지하고 내부 local submit 결과를 [명시 반환표](20_API.md#ports)로 변환한다.

| 기존 callback / 인자 | 정확한 source 인자 타입 | 메시지·직접 투영 |
|---|---|---|
| `port->txBcmDoorCommand(command)` | [`const DomainIf_BcmDoorCommand_t *`](12_DOMAIN_TYPES.md#type-domainif-bcmdoorcommand-t) | `BCM_DOOR_COMMAND` — C2 원 commandId/origin/목표 보존. Network targetBoot/commandAge/linkBoot/TxSeq/CRC 보완. |
| `port->txBcmClimateCommand(command)` | [`const DomainIf_BcmClimateCommand_t *`](12_DOMAIN_TYPES.md#type-domainif-bcmclimatecommand-t) | `BCM_CLIMATE_COMMAND` — C2 원 commandId/origin/목표 보존. Network targetBoot/commandAge/linkBoot/TxSeq/CRC 보완. |
| `port->txBcmInteriorLightCommand(command)` | [`const DomainIf_BcmInteriorLightCommand_t *`](12_DOMAIN_TYPES.md#type-domainif-bcminteriorlightcommand-t) | `BCM_LIGHT_COMMAND` — C2 원 commandId/origin/목표 보존. Network targetBoot/commandAge/linkBoot/TxSeq/CRC 보완. |
| `port->txCisPowerPermission(permission)` | [`const DomainIf_CisPowerPermission_t *`](12_DOMAIN_TYPES.md#type-domainif-cispowerpermission-t) | `DOMAIN_PERMISSION` — 한 shared CAN 허용 snapshot에 연계. 독립 source tuple을 섞는 집계/원 powerOperationState binding은 FUNCTION_POLICY_OPEN. |
| `port->txWindowCommand(command)` | [`const DomainIf_WindowCommand_t *`](12_DOMAIN_TYPES.md#type-domainif-windowcommand-t) | `WINDOW_COMMAND` — C2 원 commandId/origin/목표 보존. Network targetBoot/commandAge/linkBoot/TxSeq/CRC 보완. |
| `port->txWindowOperationPermission(permission)` | [`const DomainIf_WindowOperationPermission_t *`](12_DOMAIN_TYPES.md#type-domainif-windowoperationpermission-t) | `DOMAIN_PERMISSION` — 한 shared CAN 허용 snapshot에 연계. 독립 source tuple을 섞는 집계/원 powerOperationState binding은 FUNCTION_POLICY_OPEN. |
| `port->txWindowDomainAlive(alive)` | [`const DomainIf_WindowDomainAlive_t *`](12_DOMAIN_TYPES.md#type-domainif-windowdomainalive-t) | `DOMAIN_PERMISSION` — 기존 WINDOW liveness 의미 callback 보존. 신규 frame 없음. shared DOMAIN_PERMISSION 수신 감독과 중복된 sourcebool의 정확한 사용/binding OPEN; [구현 보류 — 설계 유지]. |
| `port->txVssOneShotEvent(event)` | [`const DomainIf_VssOneShotEvent_t *`](12_DOMAIN_TYPES.md#type-domainif-vssoneshotevent-t) | `VSS_EVENT` — 원 Event/Warning의 occurrence/quality/provenance 보존. VSS는 음향 소비만. |
| `port->txVssWarningState(state)` | [`const DomainIf_VssWarningState_t *`](12_DOMAIN_TYPES.md#type-domainif-vsswarningstate-t) | `VSS_WARNING_STATE` — 원 Event/Warning의 occurrence/quality/provenance 보존. VSS는 음향 소비만. |
| `port->txMobileRequestResult(result)` | [`const DomainIf_MobileRequestResult_t *`](12_DOMAIN_TYPES.md#type-domainif-mobilerequestresult-t) | `M_RESULT` — source 의미 객체에서 UART projection 직접 encode. query=NULL 일 때 현재 수신단말·Session을 확인; 원 요청 Session은 보존. |
| `port->txMobileBcmDoorState(state)` | [`const DoorState_t *`](11_VEHICLE_TYPES.md#type-doorstate-t) | `M_BCM_DOOR_STATE` — source 의미 객체에서 UART projection 직접 encode. query=NULL 일 때 현재 수신단말·Session을 확인; 원 요청 Session은 보존. |
| `port->txMobileBcmClimateState(state)` | [`const ClimateState_t *`](11_VEHICLE_TYPES.md#type-climatestate-t) | `M_BCM_CLIMATE_STATE` — source 의미 객체에서 UART projection 직접 encode. query=NULL 일 때 현재 수신단말·Session을 확인; 원 요청 Session은 보존. |
| `port->txMobileBcmInteriorLightState(state)` | [`const InteriorLightState_t *`](11_VEHICLE_TYPES.md#type-interiorlightstate-t) | `M_BCM_LIGHT_STATE` — source 의미 객체에서 UART projection 직접 encode. query=NULL 일 때 현재 수신단말·Session을 확인; 원 요청 Session은 보존. |
| `port->txMobileBcmStatus(state)` | [`const DomainIf_BcmStatusReport_t *`](12_DOMAIN_TYPES.md#type-domainif-bcmstatusreport-t) | `M_BCM_STATUS` — source 의미 객체에서 UART projection 직접 encode. query=NULL 일 때 현재 수신단말·Session을 확인; 원 요청 Session은 보존. |
| `port->txMobileCisEnvironment(state)` | [`const CisCabinEnvironmentState_t *`](11_VEHICLE_TYPES.md#type-ciscabinenvironmentstate-t) | `M_CIS_ENVIRONMENT` — source 의미 객체에서 UART projection 직접 encode. query=NULL 일 때 현재 수신단말·Session을 확인; 원 요청 Session은 보존. |
| `port->txMobileCisOccupant(state)` | [`const CisOccupantState_t *`](11_VEHICLE_TYPES.md#type-cisoccupantstate-t) | `M_CIS_OCCUPANT` — source 의미 객체에서 UART projection 직접 encode. query=NULL 일 때 현재 수신단말·Session을 확인; 원 요청 Session은 보존. |
| `port->txMobileCisRear(state)` | [`const CisRearState_t *`](11_VEHICLE_TYPES.md#type-cisrearstate-t) | `M_CIS_REAR` — source 의미 객체에서 UART projection 직접 encode. query=NULL 일 때 현재 수신단말·Session을 확인; 원 요청 Session은 보존. |
| `port->txMobileCisStatus(state)` | [`const DomainIf_CisStatusReport_t *`](12_DOMAIN_TYPES.md#type-domainif-cisstatusreport-t) | `M_CIS_STATUS` — source 의미 객체에서 UART projection 직접 encode. query=NULL 일 때 현재 수신단말·Session을 확인; 원 요청 Session은 보존. |
| `port->txMobileVssStatus(state)` | [`const DomainIf_VssStateReport_t *`](12_DOMAIN_TYPES.md#type-domainif-vssstatereport-t) | `M_VSS_STATUS` — source 의미 객체에서 UART projection 직접 encode. query=NULL 일 때 현재 수신단말·Session을 확인; 원 요청 Session은 보존. |
| `port->txMobileWindowState(state)` | [`const WindowState_t *`](11_VEHICLE_TYPES.md#type-windowstate-t) | `M_WINDOW_STATE` — source 의미 객체에서 UART projection 직접 encode. query=NULL 일 때 현재 수신단말·Session을 확인; 원 요청 Session은 보존. |
| `port->txMobileWindowFault(state)` | [`const DomainIf_WindowFaultState_t *`](12_DOMAIN_TYPES.md#type-domainif-windowfaultstate-t) | `M_WINDOW_FAULT` — source 의미 객체에서 UART projection 직접 encode. query=NULL 일 때 현재 수신단말·Session을 확인; 원 요청 Session은 보존. |
| `port->txMobileWarning(warning)` | [`const DomainIf_WarningInfo_t *`](12_DOMAIN_TYPES.md#type-domainif-warninginfo-t) | `M_WARNING` — source 의미 객체에서 UART projection 직접 encode. query=NULL 일 때 현재 수신단말·Session을 확인; 원 요청 Session은 보존. |
| `port->txMobileFunctionAvailability(availability)` | [`const DomainIf_FunctionAvailabilitySet_t *`](12_DOMAIN_TYPES.md#type-domainif-functionavailabilityset-t) | `M_AVAILABILITY` — source 의미 객체에서 UART projection 직접 encode. query=NULL 일 때 현재 수신단말·Session을 확인; 원 요청 Session은 보존. |
| `port->txMobileDigitalKeyState(state)` | [`const DigitalKeyState_t *`](11_VEHICLE_TYPES.md#type-digitalkeystate-t) | `M_DIGITAL_STATUS` — source 의미 객체에서 UART projection 직접 encode. query=NULL 일 때 현재 수신단말·Session을 확인; 원 요청 Session은 보존. |
| `port->txMobileDigitalKeyResult(result)` | [`const DomainIf_DigitalKeyResult_t *`](12_DOMAIN_TYPES.md#type-domainif-digitalkeyresult-t) | `M_DIGITAL_RESULT` — source 의미 객체에서 UART projection 직접 encode. query=NULL 일 때 현재 수신단말·Session을 확인; 원 요청 Session은 보존. |
| `port->txMobileUserSettings(settings)` | [`const DomainIf_UserSettingsState_t *`](12_DOMAIN_TYPES.md#type-domainif-usersettingsstate-t) | `M_USER_SETTINGS` — source 의미 객체에서 UART projection 직접 encode. query=NULL 일 때 현재 수신단말·Session을 확인; 원 요청 Session은 보존. |

**포인터·원 ID:** callback이 OK를 반환하기 전에 immutable 값복사/encode와 required failure sink를 확보한다. target Boot가 불명확하거나 source enum을 raw로 표현할 수 없으면 local NOT_AVAILABLE/INVALID_ARGUMENT 등 실패를 반환하며 vehicle outcome을 만들지 않는다. 원본 테이블의 26개 항목은 보존했으나 실제 등록 테이블·구현 심벌·큐 용량은 NOT_EXECUTED 상태다.

<a id="delta-permission-routing"></a>
**DOMAIN Core 철회 완료 보고 / Network 연결 미검증:** 평가 실패·NO_DATA 시 ALLOW 철회와 원 성공 Sequence 유지·내부 stateRevision 증가는 담당자 구현 완료 보고다. DOMAIN Permission Adapter의 원인별 C2 Quality 투영은 수용 대기이며 Network TxPort/composer는 품질·시간·수용 문맥을 검증해 합성·전달한다. 같은 원 성공 Sequence의 값·품질 저하를 버리지 않고, Queue·재전송·재기동으로 구 ALLOW가 철회 이후 다시 소비되지 않도록 해야 한다. 실제 TxPort 도달·Driver 완료·CIS/Pi 적용은 **BINDING_NOT_VERIFIED**다. POWER_OPERATION_STATE·필수 입력·공통 AGE/Quality/세대 조건은 여전히 미결이며 stateRevision을 공개 필드나 새 성공 평가 번호로 노출하지 않는다. [원 평가·합성·전송 순번 분리](../../04_COMMON/10_DATA_AND_ID_RULES.md#delta-permission-sequences)를 따른다.

<a id="rx-projection"></a>
## CAN/UART 수신 → source typed 인계

| 메시지 | 실제 C2/내부 named sink | 보존·변환 조건 |
|---|---|---|
| `BCM_RESULT` | [`DomainIf_RxBcmCommandResult()`](20_API.md#api-domainif_rxbcmcommandresult) | 원 COMMAND_DOMAIN_BOOT/commandId/kind·result/reason·atomic doorEvidence 보존. 현재 Wire에는 C2 confirmation 필드가 없으며 아래 권장 확인 제한을 적용한다. event-age를 result-age로 합성하지 않는다. |
| `BCM_EVENT` | [`DomainIf_RxBcmProtectionEvent()`](20_API.md#api-domainif_rxbcmprotectionevent) | 보호 event type/occurrence/generation/quality/AGE/related Command/affected mask. door result와 분리. |
| `BCM_DOOR_STATE` | [`DomainIf_RxBcmDoorState()`](20_API.md#api-domainif_rxbcmdoorstate) | 원 의미 상태·독립 품질/원 AGE/원 generation 보존. enum 및 raw source 숫자는 명시 전환. |
| `BCM_CLIMATE_STATE` | [`DomainIf_RxBcmClimateState()`](20_API.md#api-domainif_rxbcmclimatestate) | 원 의미 상태·독립 품질/원 AGE/원 generation 보존. enum 및 raw source 숫자는 명시 전환. |
| `BCM_LIGHT_STATE` | [`DomainIf_RxBcmInteriorLightState()`](20_API.md#api-domainif_rxbcminteriorlightstate) | 원 의미 상태·독립 품질/원 AGE/원 generation 보존. enum 및 raw source 숫자는 명시 전환. |
| `BCM_STATUS` | [`DomainIf_RxBcmStatus()`](20_API.md#api-domainif_rxbcmstatus) | 원 의미 상태·독립 품질/원 AGE/원 generation 보존. enum 및 raw source 숫자는 명시 전환. |
| `CIS_REAR` | [`DomainIf_RxCisRear()`](20_API.md#api-domainif_rxcisrear) | 원 의미 상태·독립 품질/원 AGE/원 generation 보존. enum 및 raw source 숫자는 명시 전환. |
| `CIS_ENVIRONMENT` | [`DomainIf_RxCisCabinEnvironment()`](20_API.md#api-domainif_rxciscabinenvironment) | 온도·습도·조도 각각 원 validity/reason/quality/AGE/sequence. |
| `CIS_OCCUPANT` | [`DomainIf_RxCisOccupant()`](20_API.md#api-domainif_rxcisoccupant) | Pi 원 Boot/vision sequence/AGE와 존재·인원수 독립 품질. CIS 재평가 overlay 관계 OPEN. |
| `CIS_STATUS` | [`DomainIf_RxCisStatus()`](20_API.md#api-domainif_rxcisstatus) | 원 기능별 상태/활성·최근 fault/복구와 Network 링크 상태; DEGRADED 미지원 raw code는 임의 READY로 바꾸지 않음. |
| `WINDOW_RESULT` | [`DomainIf_RxWindowCommandResult()`](20_API.md#api-domainif_rxwindowcommandresult) | 원 의미 상태·독립 품질/원 AGE/원 generation 보존. enum 및 raw source 숫자는 명시 전환. |
| `WINDOW_STATE` | [`DomainIf_RxWindowState()`](20_API.md#api-domainif_rxwindowstate) | 원 의미 상태·독립 품질/원 AGE/원 generation 보존. enum 및 raw source 숫자는 명시 전환. |
| `M_REQUEST` | [`DomainIf_RxMobileRequest()`](20_API.md#api-domainif_rxmobilerequest) | MOBILE 원 Request context,8 종 type union, 현재 단말/Session/등록 검사. |
| `M_WARNING_ACK` | [`DomainIf_RxWarningAck()`](20_API.md#api-domainif_rxwarningack) | warning kind 역변환+Domain Boot/occurrence/단말/Session. 읽음≠위험 해제. |
| `M_CONTEXT` | `DomainNetPriv_OnGatewayContext()` → `DomainIf_RxDigitalKeyConnection()` + `DomainIf_RxProximity()` | 등록·연결 한 공동 tuple, App tuple/근접 tuple 독립. Session 원 현재 binding 반영. |
| `WINDOW_EVENT` | `DomainNetPriv_OnWindowEvent()` → AntiPinch만 `RxWindowAntiPinch()` | END owner와 relatedCommand 관계 OPEN. 모든 event를 anti-pinch로 압축 금지. DEFERRED. |
| `WINDOW_FAULT` | `DomainNetPriv_OnWindowFault()` → 좁은 `RxWindowFault()` | fault+occurrence+AGE+quality+원 generation을 private 원 객체로 보존. sourceFaultInfo 만으로 소실 금지. DEFERRED. |
| `VSS_STATUS` | `DomainNetPriv_OnVssStatus()` → `DomainIf_RxVssState()`/`RxVssFault()` | INPUT 단일 완전 보고 생산은 A2 선택. 원 음향 상태/current/최근 occurrence·AGE·품질·세대 보존 후 State→Fault 독립 수용. 같은 K 반복 호출0, 실제 profile/Getter association·old K fence는 E-S05 확인 대기. |


<a id="delta-environment-routing"></a>
### CIS 환경 BOOT 변경 경계

현재 환경값 수신은 기존 `DomainIf_RxCisCabinEnvironment()`를 사용한다. 새로운 CIS BOOT가 확인되면 Network는 해당 환경 경로에서 이전 BOOT의 대기 입력 전달을 차단하고, DOMAIN의 이전 저장값을 무효화하는 동기 Private Transition이 실제로 연결돼 있다면 해당 경계를 한 번 호출한다. DOMAIN은 이전 저장값·순번 비교 문맥·Query의 과거 정상값을 무효화한 완료 결과를 반환한다. Network는 **현재 전환의 완료를 확인한 후** 새 BOOT 관측을 기존 환경 Rx로 전달한다. BOOT 무효화용 Rx NO_DATA는 중복 호출하지 않는다.

| 경우 | 초기 처리 |
|---|---|
| 평상시 동일 BOOT 수신 | 기존 Codec·환경 Rx로 전달 |
| 처음 확인한 새 BOOT | 해당 환경에서 이전 BOOT에 속한 대기 입력 차단 → Private 적용 완료 확인 → 새 입력 전달 |
| Private API 미연결·실패·완료 불명 | 환경 입력 제한 및 오류 보고. 다른 메시지 경로는 유지 |
| 같은 전환 요청이 다시 입력됨 | 기존 완료 여부 확인. 새 환경값을 무조건 다시 지우지 않음 |
| 추가 BOOT 변경 | DOMAIN의 다음 전환 수용이 확인될 때까지 환경 자동 복귀 보류 |

DOMAIN 담당자는 첫 전환·동일 Epoch 중복/충돌·동기 무효화 Core Host 시험 완료를 보고했다. **현재 Core는 서로 다른 Epoch를 거부**하며, 다음 전환 수용과 C2 Rx→VSM→Query Binding은 미확인이다. `transitionEpoch`는 설치된 Private API가 사용하는 내부 전환 식별값이며 새 CAN/UART 필드가 아니다. 초기 연결에서는 영속 Epoch, 자동 재시작 동기화, 공통 BOOT 관리자 및 추가 Retry/Handshake를 사용하지 않는다. 실제 다음 전환을 연결할 때 단일 담당 문맥에서의 호출·동기 완료·이전 저장값 재사용 차단을 검증한다.

<a id="r2-origin-join"></a>
### PROPOSED — 수신 Evidence와 C2 Snapshot의 결합

<a id="r1-atomic-provenance"></a>
같은 원 관측의 품질 저하도 수용된 C2 commit과 원 raw/BOOT/관측번호·현재 품질/사유·순서 상태를 함께 연결한다. 높은 전송 순번은 원값 변경이나 INVALID→VALID 근거가 아니다. 인계·저장 실패 시 provenance나 순서만 먼저 전진시키지 않는다. 최신 Getter 결과와 가장 최근에 수신한 원시 데이터를 시각이 가깝다는 이유만으로 연결하지 않으며, 동일 수용 결과를 증명하지 못하는 Query projection은 제한한다. 원 생성 기준점은 고정하고 현재 AGE는 증가한다. 실제 C2 저장/Meta/Getter의 원자적 연결은 BINDING_NOT_VERIFIED다.

통신 처리 규칙 — 원 관측의 동일성은 현재 품질이 영구히 같다는 뜻이 아니다. 같은 Pi BOOT/VISION_SEQUENCE에 대한 새 CAN 프레임이 확인된 품질 저하를 전달하면, 새로 수용한 메시지별 TX_SEQUENCE를 확인하고 원 식별·누적 AGE를 유지한 채 기존 `DomainIf_RxCisOccupant()`에 반영한다. 관측 순번 중복만으로 무효 품질을 버리지 않는다. 지연된 구 VALID, 단순 READY 또는 링크 복구만으로 이전 정상값을 복원하지 않는다. 정상 복원에는 새 유효 관측과 해당 기능의 복구 규칙을 적용한다. 이 규칙은 관측 번호·품질 필드·새 공개 API를 추가하지 않는다. DOMAIN 저장값과 Getter까지 반영되는지는 실제 연결 시험 대상이다.

환경도 CIS BOOT 변경 시 이전 값의 무효화와 새 기동의 낮은 순번 수용을 분리해야 한다. 원 수신 객체를 최신 Getter에 단순 부착하지 않는다. DOMAIN이 처리한 입력과 Network 원 기록의 연결은 직렬 처리/commit 증거로 검증한다. 실제 VSM 확인 대기는 완료로 바꾸지 않는다. [노드 간 공통 규칙](../../07_CROSS_NODE_INTERFACE_RULES.md#environment)

**NETWORK_OWNED + CROSS_NODE_AGREEMENT.** 권장안은 기존 메시지별 **검증된 원 `Net*` 객체와 `NetCanRxMeta`의 안정된 복사**를 보존하여 이미 존재하는 C2 ingress/Getter와 연결하는 것이다. 별도 공개 DTO·새 store/범용 관리자·새 원본 번호를 만들지 않는다. 수신 검증 단계는 현재 CAN ID/원 sender Boot·TX, 메시지별 원 관측/사건 식별자, 품질과 원 AGE를 한 immutable evidence로 취급한다. C2의 `sourceGeneration`이 Pi Boot인 경우 그것을 CIS CAN Boot로 바꾸지 않는다.

1. 원 DTO+meta를 확보하고 기존 C2 `DomainIf_Rx*`에 명시 변환한다. C2 owner가 동일 입력을 실제 상태에 반영했다고 확인한 처리 문맥에서만 그 원 Evidence와의 association을 유지한다. 단순히 콜백 큐 접수에 성공했다는 사실을 캐시 저장 완료의 증거로 사용하지 않는다. ingress 거부 또는 association 확인 불가이면 이전 정상 원본의 identity/AGE를 새로 쓰지 않는다.
2. Query Getter 또는 unsolicited TxPort와 evidence 결합은 **같은 owner 직렬 처리 안에서 중간 갱신 없이 해당 원 입력의 Snapshot임을 확인**한 경우만 수행한다. 확인 가능한 원 generation/update/occurrence, 원 값·필드별 품질 및 경과 AGE 관계도 대조한다. 값 또는 PiBoot만 같은 것은 충분하지 않다. 예를 들어 인원수1/PiBoot7이 같은 서로 다른 VisionSeq9/10을 같은 원본으로 join하지 않는다. 해당 C2 타입에 관측 번호가 없으면 실제 atomic association 증거가 없을 때 join하지 않는다.
3. 11 CAN-origin M_*는 해당 evidence의 `meta.source.boot_id/tx_sequence`를 B20~27에 보존하고 원 CAN 사용부를 +20 B로 encode한다. CAN padding 제외, 각 source/last-fault/result AGE에 수신 후 로컬 대기를 각각 한 번 추가하고 포화·역행 방지 공통 계약을 적용한다. 원 PI_BOOT/VISION_SEQUENCE·독립 field quality도 그대로 보존한다. 새 UART LINK_SEQUENCE/Domain Boot는 외부 envelope에만 쓴다.
4. provenance 없는 해당 projection은 `NET_SUBMIT_NOT_READY`/source `DOMAIN_IF_NOT_AVAILABLE`의 로컬 인계 실패로 남기고 Query에서는 해당 항목 누락을 `STATUS=2`로 인계한다. 다른 검증된 메시지·센서 값을 일괄 차단하지 않는다. 잘못된 source enum/인자는 기존 INVALID_ARGUMENT으로 실패시키며 가짜 NO_DATA 프레임 또는 새 DOMAIN 원 관측을 생성하지 않는다.

**원자적 연결 전제:** C2 Getter의 Snapshot을 최신 원시 데이터 캐시에 값만 맞춰 연결하면 동일값 새 관측/구 Boot가 오결합할 수 있다. 기존 원 DTO+meta 재사용과 owner의 원자적 연결을 권장한다. 각 `Rx*`→Getter/TxPort가 같은 원 입력임을 증명할 commit·직렬 처리 위치가 필요하다. CIS_OCCUPANT처럼 C2에 VisionSeq가 없는 경우에도 동시 갱신 없는 association이 입증돼야 한다. 확인 전 해당 projection은 `BINDING_OPEN`이다. BOOT/TX/추가 DTO는 현재 공개 C2 선언에 포함되지 않으며 후보는 `PROPOSED`, 실제 Codec 연결은 검증 전이다.

### 좁은 source 구조에서 표현되지 않는 정보

**PROPOSED — BCM_RESULT confirmation (NETWORK_OWNED + FUNCTION_OWNER):** C2의 `confirmation`을 CAN에서 직접 읽은 값처럼 취급하지 않는다. 원 COMMAND_DOMAIN_BOOT/commandId/kind를 해당 원 명령에 연결하고 result/reason은 기존 명시 mapping을 보존한다. 확인 근거가 없으면 기존 `RESULT_CONFIRMATION_UNCONFIRMED`로 인계하며 CAN 수신·Network submit OK·DONE raw만으로 CONFIRMED를 생성하지 않는다. 같은 원 명령의 door COMPLETED·quality OK 등 현재 door evidence가 C2 confirmation을 입증하는 충분조건인지는 BCM/DOMAIN 기능 정책에서 확정해야 한다. 그 확인 근거가 없으면 결과의 confirmation 경로만 `BINDING_OPEN`; 새 Wire confirmation byte/원형을 추가하지 않는다. 실제 BCM/Codec 시험 `NOT_EXECUTED`.

| 원 객체 | source 구조의 경계 | 안전한 설계 상태 |
|---|---|---|
| WINDOW END event | `DomainIf_AntiPinchReport_t`는 END 타입·관련 Command 전부를 표현하지 못함 | NetWindowEvent 전체 유지. 실제 END 소비 owner의 typed handoff 조건 확정 전 activation hold. |
| WINDOW fault / CIS status fault / VSS status fault | `FaultInfo_t`는 occurrence·원 generation·AGE가 없음 | 넓은 원 상태+Network 원 Evidence 유지한 뒤 좁은 callback 별도 투영. 새 Fault store/정책 owner를 Network에 만들지 않음. |
| CIS 탑승자 | C2의 sourceGeneration/quality와 Pi 원 Boot·CIS 품질의 정확한 대응 미정 | 원 NetCisOccupant metadata를 preserved provenance로 유지. source C2 품질을 Pi 판정으로 relabel 하지 않음. |
| C2 snapshot→M_* raw CAN origin | SOURCE QueryGetter는 NetCanSource의 원 TxSeq를 반환하지 않음 | 수신 provenance와 source snapshot 동일 identity 확인 후 부착. 확인불가 시 부분 결과/no data 또는 전송차단; 현재 frame 번호로 가짜 원본 출처를 만들지 않는다. |
| source enum unsupported 값 | UNCONFIRMED/UNSUPPORTED/source-only reason 등 raw code 부재 | encode 차단/WIRE_MAPPING_OPEN. CLEAR/AVAILABLE/NONE으로 정상화 금지. |

<a id="query-flow"></a>
## Query —18 snapshot Getter와 17 schema encoder 재사용

15 개의 `SendQueryMobile*`를 공개 함수로 유지하지 않는다. 기존 C2 unsolicited TX와 QueryGet이 같은 source 의미 타입을 사용하므로 schema 별 17 개의 내부 submit encoder를 재사용한다(15 개 상태/경고/설정 + RequestResult + DigitalKeyResult). 외부 source72 개와 원 Wire48 종은 그대로 보존한다.

### 내부 Query response context 전체 선언

```c
typedef struct
{
    NetAppContext recipient;
    uint32_t queryId;
    NetQueryScope scope;
} DomainQueryResponseContext_t;
```

| 멤버 | 정확한 타입 | 생성·사용·유효성 |
|---|---|---|
| `recipient` | `NetAppContext` | 검증된 현재 조회 단말·Session. Network가 M_QUERY 현재 binding 에서 구성; 원 과거 RequestSession과 별개. |
| `queryId` | `uint32_t` | MOBILE 원 QUERY_ID;0이 아닌 조회식별. 재전달로 새 ID 발급 금지. |
| `scope` | `NetQueryScope` | 원 조회범위. frame scope/종결 scope 일치 검사. raw 범위는 UART-M 제안표와 명시전환. |

**NETWORK_PRIVATE / PROPOSED_DESIGN.** 이 작은 envelope는 필요 QueryId/수신문맥만 전달하며 App의 공개 transaction/store가 아니다. Source DTO를 중간복제하는 계층을 추가하지 않는다. `query=NULL`이면 자발출력, non-NULL 이면 원 Query 응답문맥이다. 송신 Domain Boot/LINK_SEQUENCE/CRC/원래 기다린 AGE는 Codec/Network가 보완한다.

### 최소 호출 순서

최근 요청 결과는 해당 Device의 내부 8건 중 최신순 최대 5건을 조회해 M_RESULT 0~5개로 순차 전송하고 M_QUERY_END로 끝낸다. 다른 Query 종류를 포함한 전체 응답 수의 상한이 아니다. Getter 실패, 확인된 빈 목록, 부분 응답을 구분하고 로컬 접수 성공만으로 완료 count를 만들지 않는다. 이력 8/5 규칙과 기존 QueryEnd의 생산·소비 후보를 별도로 유지한다. [조회 범위](../../07_CROSS_NODE_INTERFACE_RULES.md#query)

1. UART-M v2 CRC/length/type/context를 검사하고 prior M_CONTEXT를 C2 owner 문맥에 먼저 반영한다.
2. `DomainIf_ValidatePassiveQueryContext(device, session)`를 같은 직렬 owner 처리에서 호출한다. SESSION_READY=0 이라도 유효한 최초 Query 허용. Session 생성/활성화 하지 않음.
3. 현재 monotonic nowMs로 scope에 맞는 기존 owner snapshot Getter를 호출한다. Getter 실패 시 OUT을 읽지 않는다. fresh/validity/UNKNOWN/없는기록을 그대로 보존. scope 여러항목의 snapshot 일관성을 owner 직렬화로 보장하며 실제 binding OPEN.
4. 동일 schema의 내부 typed submitter에 source snapshot과 Query context를 넣는다. 원 Update/Occurrence/RequestSession을 유지하고 Network 자체대기 AGE만 한 번 누적.
5. M_QUERY_END의 ITEM_COUNT는 종료 frame을 뺀 실제 응답 frame 수다. 단순 queue admission count가 아니며 Gateway/MOBILE 실제 수신 완료도 아니다. local OK 후 최종송신실패가 있으면 성공종결을 만들지 않는다. driver 송신 완료/ordering의 실제 countcommit 지점은 BINDING_OPEN.
6. QueryId/scope/current recipient/Domain Boot를 묶어 stream을 종결한다. ESP32는 같은 stream 수신분과 종결개수를 대조. M_RESULT의 원과거 request_session을 현재 recipient.session으로 바꾸지 않음.


<a id="r2-query-producer"></a>
### PROPOSED — 기존 Query 상태·실송신 종결 규칙

**NETWORK_OWNED + CROSS_NODE_AGREEMENT.** 기존 `DomainQueryResponseContext_t`와 18 Getter/17 encoder를 재사용한다. 조회 시작에 현재 recipient/QueryID/scope/Domain Boot와 처리할 기존 Getter·기록 범위를 잡고 owner 직렬 처리로 응답을 준비한다. 하나의 Query를 처리하는 동안 다른 Query의 응답을 같은 stream으로 합치지 않는다. Getter 실패 시 OUT 읽기 금지, `outHasRecord=false`는 실패와 구분한다.

`ITEM_COUNT` 권장 기준은 해당 조회 응답 frame의 **성공한 최종 로컬 UART TX를 실제 증명한 수**다. 확정 실패 terminal은 개수에 넣지 않는다. source copy/encode·큐 등록·`NET_SUBMIT_OK`, End, QUERY_ID0 자발 갱신, 재전송 복제도 제외한다. 모든 예정 응답이 TX 성공 또는 확정 실패 terminal에 도달한 뒤 End를 같은 UART 송신 순서로 뒤에 보낸다. pending이 남으면 End를 보류한다. End submit 성공은 전송 완료가 아니고 UART TX 성공은 ESP32/BLE/MOBILE 수신·차량 완료를 보증하지 않는다. `u16` count 초과, 부분 frame 도착 여부 불명 또는 완료 관측 부재이면 정확한 count/완료를 주장하지 않는다. 응답 ordinal이 없으므로 미확인 송신을 새 LINK_SEQUENCE로 자동 재시도하여 건수를 부풀리지 않는다.

| 실제 사실 | 기존 QUERY_STATUS / count 권장 사용 | 소비 제한 |
|---|---|---|
| scope의 모든 필요한 항목을 확인하고 모든 응답의 최종 TX 완료를 증명 | `0` / 실제 TX 완료 N | 전송 목록 종결만; 각 품질/AGE 별도 |
| encode 미표현·provenance 미확인·Getter 실패·실송신 일부 실패의 누락을 확인 | `2` / 최종 TX 완료를 증명한 N | 부분 결과; N=0도 완전 성공 아님 |
| 특정 과거 Request 조회에서 owner가 기록 없음을 확인 | `1` / `0` | 원 요청 부재; 차량 FAILED 생성 금지 |
| 현재 passive Query 문맥 거부 | `3` / `0` | 문맥 거부; Session 발급/활성화 금지 |
| 실제로 빈 응답집합임을 양단의 기존 scope/Getter 의미로 증명 | `0` / `0`만 조건부 허용 | Getter 실패·미확인 목록을 0건 성공으로 대체 금지 |
| TX 결과/순서/End 송신 결과를 증명할 수 없음 | 성공 End 금지; 로컬 불완전/만료 인계 | count를 추정하지 않고 완료 binding 제한 |

**0건/부분 검증:** 응답0 자체는 성공 증거가 아니다. 10개 enqueue 중 **8개 TX 성공/2개 확정 실패**이면 기존 `STATUS=2, ITEM_COUNT=8` 후보이며 `0,10`과 실패 terminal을 더한 count는 금지한다. **8개 TX 성공/2개 pending**이면 아직 End를 보내지 않는다. 값이 UNCONFIRMED인 응답도 TX 성공 근거가 있으면 count에 포함하지만 송신 성공 미확인은 제외한다. 일부 frame 도착 여부가 불명확하면 End 수치만으로 완료를 만들지 않는다. 실제 terminal/순서를 관측할 driver binding이 없으면 이 권장 count는 `BINDING_OPEN`이다. owner 처리 기한 수치는 OPEN이며 timeout 후 새 QueryID 없이 뒤늦은 End로 성공을 재개하지 않는다. 재연결·Domain reboot는 이전 조회를 불완전 종료하고 원 RequestSession을 유지한다.

**Query 종결 전제:** enqueue count는 비동기 손실을 반영하지 못하므로 완료 count로 사용하지 않는다. ESP32 ACK 수는 현재 Wire에 포함되지 않는다. 최종 TX 완료 관측 지점, 확정 실패 후 End 순서, 동일 stream 중복 수신과 scope의 실제 빈 집합을 DOMAIN/ESP32 경계에서 확인해야 한다. timeout 수치와 MOBILE 완료/부분 UI는 기능 정책이 미확정이다. 규칙은 `PROPOSED`이며 실제 queue/Codec/board 연결은 검증 전이다.

### Source Getter18 종 → 직접 typed encoder

| 기존 snapshot Getter | 정확한 source OUT | 응답 schema |
|---|---|---|
| [`DomainIf_QueryGetBcmDoorState()`](20_API.md#api-domainif_querygetbcmdoorstate) | `DoorState_t * outState` | `M_BCM_DOOR_STATE` |
| [`DomainIf_QueryGetBcmClimateState()`](20_API.md#api-domainif_querygetbcmclimatestate) | `ClimateState_t * outState` | `M_BCM_CLIMATE_STATE` |
| [`DomainIf_QueryGetBcmInteriorLightState()`](20_API.md#api-domainif_querygetbcminteriorlightstate) | `InteriorLightState_t * outState` | `M_BCM_LIGHT_STATE` |
| [`DomainIf_QueryGetBcmStatus()`](20_API.md#api-domainif_querygetbcmstatus) | `DomainIf_BcmStatusReport_t * outState` | `M_BCM_STATUS` |
| [`DomainIf_QueryGetCisEnvironment()`](20_API.md#api-domainif_querygetcisenvironment) | `CisCabinEnvironmentState_t * outState` | `M_CIS_ENVIRONMENT` |
| [`DomainIf_QueryGetCisOccupant()`](20_API.md#api-domainif_querygetcisoccupant) | `CisOccupantState_t * outState` | `M_CIS_OCCUPANT` |
| [`DomainIf_QueryGetCisRear()`](20_API.md#api-domainif_querygetcisrear) | `CisRearState_t * outState` | `M_CIS_REAR` |
| [`DomainIf_QueryGetCisStatus()`](20_API.md#api-domainif_querygetcisstatus) | `DomainIf_CisStatusReport_t * outState` | `M_CIS_STATUS` |
| [`DomainIf_QueryGetVssStatus()`](20_API.md#api-domainif_querygetvssstatus) | `DomainIf_VssStateReport_t * outState` | `M_VSS_STATUS` |
| [`DomainIf_QueryGetWindowState()`](20_API.md#api-domainif_querygetwindowstate) | `WindowState_t * outState` | `M_WINDOW_STATE` |
| [`DomainIf_QueryGetWindowFault()`](20_API.md#api-domainif_querygetwindowfault) | `DomainIf_WindowFaultState_t * outState` | `M_WINDOW_FAULT` |
| [`DomainIf_QueryGetUserSettings()`](20_API.md#api-domainif_querygetusersettings) | `DomainIf_UserSettingsState_t * outSettings` | `M_USER_SETTINGS` |
| [`DomainIf_QueryGetFunctionAvailability()`](20_API.md#api-domainif_querygetfunctionavailability) | `DomainIf_FunctionAvailabilitySet_t * outAvailability` | `M_AVAILABILITY` |
| [`DomainIf_QueryGetDigitalKeyState()`](20_API.md#api-domainif_querygetdigitalkeystate) | `DigitalKeyState_t * outState` | `M_DIGITAL_STATUS` |
| [`DomainIf_QueryGetCurrentWarning()`](20_API.md#api-domainif_querygetcurrentwarning) | `DomainIf_WarningInfo_t * outWarning` | `M_WARNING` |
| [`DomainIf_QueryGetRequestResult()`](20_API.md#api-domainif_querygetrequestresult) | `DomainIf_MobileRequestResult_t * outResult; bool * outHasRecord` | `M_RESULT` |
| [`DomainIf_QueryGetRecentRequestResults()`](20_API.md#api-domainif_querygetrecentrequestresults) | `DomainIf_MobileRequestResult_t * outResults; size_t * outCount` | `M_RESULT (기록당1frame)` |
| [`DomainIf_QueryGetLastDigitalKeyResult()`](20_API.md#api-domainif_querygetlastdigitalkeyresult) | `DomainIf_DigitalKeyResult_t * outResult; bool * outHasRecord` | `M_DIGITAL_RESULT` |

<a id="mobile-submit-contract"></a>
### 재사용하는 내부 typed submit17 종

**PROPOSED_DESIGN / NETWORK_PRIVATE / BINDING_OPEN.** caller는 C2 TxPort Adapter(자발 출력) 또는 Query 처리 Adapter(조회 출력)다. 아래 source 인자는 호출 동안 borrowed이며 `query=NULL`은 자발 출력, non-NULL은 검증된 Query response 문맥이다. 모든 submitter는 local OK 전에 source 의미·원 ID·원 AGE와 Query envelope를 안정된 복사/encode한다. 실패는 기존 NetSubmitResult 4종으로 인계하고 TX 완료 count를 증가시키지 않는다. CAN 기반 상태는 [원 입력 association](#r2-origin-join), Settings/Digital은 [미표현 export 제한](14_MOBILE_CODEC_DATA.md#r2-partial-export)을 함께 적용한다.


<a id="api-domainnetpriv_submitmobilerequestresult"></a>
```c
NetSubmitResult DomainNetPriv_SubmitMobileRequestResult(const DomainIf_MobileRequestResult_t *result, const DomainQueryResponseContext_t *query);
```

`M_RESULT` · [공통 호출자·수명·실패 조건](#mobile-submit-contract)을 적용한다.

<a id="api-domainnetpriv_submitmobilewarning"></a>
```c
NetSubmitResult DomainNetPriv_SubmitMobileWarning(const DomainIf_WarningInfo_t *warning, const DomainQueryResponseContext_t *query);
```

`M_WARNING` · [공통 호출자·수명·실패 조건](#mobile-submit-contract)을 적용한다.

<a id="api-domainnetpriv_submitmobilefunctionavailability"></a>
```c
NetSubmitResult DomainNetPriv_SubmitMobileFunctionAvailability(const DomainIf_FunctionAvailabilitySet_t *availability, const DomainQueryResponseContext_t *query);
```

`M_AVAILABILITY` · [공통 호출자·수명·실패 조건](#mobile-submit-contract)을 적용한다.

<a id="api-domainnetpriv_submitmobiledigitalkeystate"></a>
```c
NetSubmitResult DomainNetPriv_SubmitMobileDigitalKeyState(const DigitalKeyState_t *state, const DomainQueryResponseContext_t *query);
```

`M_DIGITAL_STATUS` · [공통 호출자·수명·실패 조건](#mobile-submit-contract)을 적용한다.

<a id="api-domainnetpriv_submitmobiledigitalkeyresult"></a>
```c
NetSubmitResult DomainNetPriv_SubmitMobileDigitalKeyResult(const DomainIf_DigitalKeyResult_t *result, const DomainQueryResponseContext_t *query);
```

`M_DIGITAL_RESULT` · [공통 호출자·수명·실패 조건](#mobile-submit-contract)을 적용한다.

<a id="api-domainnetpriv_submitmobileusersettings"></a>
```c
NetSubmitResult DomainNetPriv_SubmitMobileUserSettings(const DomainIf_UserSettingsState_t *settings, const DomainQueryResponseContext_t *query);
```

`M_USER_SETTINGS` · [공통 호출자·수명·실패 조건](#mobile-submit-contract)을 적용한다.

<a id="api-domainnetpriv_submitmobilebcmdoorstate"></a>
```c
NetSubmitResult DomainNetPriv_SubmitMobileBcmDoorState(const DoorState_t *state, const DomainQueryResponseContext_t *query);
```

`M_BCM_DOOR_STATE` · [공통 호출자·수명·실패 조건](#mobile-submit-contract)을 적용한다.

<a id="api-domainnetpriv_submitmobilebcmclimatestate"></a>
```c
NetSubmitResult DomainNetPriv_SubmitMobileBcmClimateState(const ClimateState_t *state, const DomainQueryResponseContext_t *query);
```

`M_BCM_CLIMATE_STATE` · [공통 호출자·수명·실패 조건](#mobile-submit-contract)을 적용한다.

<a id="api-domainnetpriv_submitmobilebcminteriorlightstate"></a>
```c
NetSubmitResult DomainNetPriv_SubmitMobileBcmInteriorLightState(const InteriorLightState_t *state, const DomainQueryResponseContext_t *query);
```

`M_BCM_LIGHT_STATE` · [공통 호출자·수명·실패 조건](#mobile-submit-contract)을 적용한다.

<a id="api-domainnetpriv_submitmobilebcmstatus"></a>
```c
NetSubmitResult DomainNetPriv_SubmitMobileBcmStatus(const DomainIf_BcmStatusReport_t *state, const DomainQueryResponseContext_t *query);
```

`M_BCM_STATUS` · [공통 호출자·수명·실패 조건](#mobile-submit-contract)을 적용한다.

<a id="api-domainnetpriv_submitmobilecisenvironment"></a>
```c
NetSubmitResult DomainNetPriv_SubmitMobileCisEnvironment(const CisCabinEnvironmentState_t *state, const DomainQueryResponseContext_t *query);
```

`M_CIS_ENVIRONMENT` · [공통 호출자·수명·실패 조건](#mobile-submit-contract)을 적용한다.

<a id="api-domainnetpriv_submitmobilecisoccupant"></a>
```c
NetSubmitResult DomainNetPriv_SubmitMobileCisOccupant(const CisOccupantState_t *state, const DomainQueryResponseContext_t *query);
```

`M_CIS_OCCUPANT` · [공통 호출자·수명·실패 조건](#mobile-submit-contract)을 적용한다.

<a id="api-domainnetpriv_submitmobilecisrear"></a>
```c
NetSubmitResult DomainNetPriv_SubmitMobileCisRear(const CisRearState_t *state, const DomainQueryResponseContext_t *query);
```

`M_CIS_REAR` · [공통 호출자·수명·실패 조건](#mobile-submit-contract)을 적용한다.

<a id="api-domainnetpriv_submitmobilecisstatus"></a>
```c
NetSubmitResult DomainNetPriv_SubmitMobileCisStatus(const DomainIf_CisStatusReport_t *state, const DomainQueryResponseContext_t *query);
```

`M_CIS_STATUS` · [공통 호출자·수명·실패 조건](#mobile-submit-contract)을 적용한다.

<a id="api-domainnetpriv_submitmobilevssstatus"></a>
```c
NetSubmitResult DomainNetPriv_SubmitMobileVssStatus(const DomainIf_VssStateReport_t *state, const DomainQueryResponseContext_t *query);
```

`M_VSS_STATUS` · [공통 호출자·수명·실패 조건](#mobile-submit-contract)을 적용한다.

<a id="api-domainnetpriv_submitmobilewindowstate"></a>
```c
NetSubmitResult DomainNetPriv_SubmitMobileWindowState(const WindowState_t *state, const DomainQueryResponseContext_t *query);
```

`M_WINDOW_STATE` · [공통 호출자·수명·실패 조건](#mobile-submit-contract)을 적용한다.

<a id="api-domainnetpriv_submitmobilewindowfault"></a>
```c
NetSubmitResult DomainNetPriv_SubmitMobileWindowFault(const DomainIf_WindowFaultState_t *state, const DomainQueryResponseContext_t *query);
```

`M_WINDOW_FAULT` · [공통 호출자·수명·실패 조건](#mobile-submit-contract)을 적용한다.

### 호출 예 — source 함수 사용, 구현/Codec 시험 아님

```c
/* DECLARED_SOURCE call example; owner-managed source fields already valid. */
DomainIf_Status_t local = DomainIf_TxBcmDoorCommand(&ownerDoorCommand);
/* local == DOMAIN_IF_OK is not BCM DONE. Keep the original commandId. */

/* Network query owner: prior M_CONTEXT already applied in serialized context. */
DomainIf_Status_t contextStatus =
    DomainIf_ValidatePassiveQueryContext(requestingDevice, currentSession);
if (contextStatus == DOMAIN_IF_OK) {
    DoorState_t snapshot;
    DomainIf_Status_t got = DomainIf_QueryGetBcmDoorState(nowMs, &snapshot);
    if (got == DOMAIN_IF_OK) {
        /* PROPOSED private schema submit; quality/source age remain unchanged. */
        NetSubmitResult submitted =
            DomainNetPriv_SubmitMobileBcmDoorState(&snapshot, &queryContext);
        /* Count actual emitted response frames, not local submission status. */
    }
}
```

**구현 상태:** 실제 callback 등록·SDK/Firmware compile·Codec round-trip·TX 완료/Query ordering/capacity/burst·ECU/무선 시험은 NOT_EXECUTED다.
