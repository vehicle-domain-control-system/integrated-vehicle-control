# ESP32 — 기존 공개/소비 경계 API

Interface v2 · r1 · [규격과 연동 조건](../../07_CROSS_NODE_INTERFACE_RULES.md#status)

**H01 실제 적용 확인 대기:** 아래 선언의 update_sequence 필드가 실제 채택된 헤더에 존재하는지는 확인되지 않았다. 실제 공통 헤더 경로·버전과 필드 채택, 또는 UART-M Adapter의 세 생산원별 독립 순번 관리 중 선택을 확인해야 한다. 등록·연결 공동 관측, App 활성, 근접을 구분해 M_CONTEXT38B로 변환하고 최초·반복·재기동·wrap을 검증하기 전에는 [H01 HOLD](11_GATEWAY_TYPES.md#r1-updatebasis-binding)를 유지한다. BLE 채택과 내부 코드 변경은 ESP32/Android 담당 범위다.

**DECLARED_SOURCE / BINDING_OPEN.** 제공된 원 헤더에서 읽은 정확한 반환형·함수명·인자다. 선언 보존은 실제 펌웨어의 구현·연결·컴파일 완료를 의미하지 않는다.

[개요](00_OVERVIEW.md) · [Data](10_DATA.md) · [내부 Adapter](25_PRIVATE_ROUTING.md) · [메시지 48](../../05_MESSAGE_INDEX.md)

<a id="query-entry"></a>
<a id="r1-context-binding"></a>
M_CONTEXT 생산자 호출은 등록·연결/App/근접의 독립 u32 순번과 원 AGE를 보존해야 한다. [UpdateBasis Header/Adapter 권고](11_GATEWAY_TYPES.md#r1-updatebasis-binding)의 세 생산원에 대한 변환과 ABI 검증 전에는 H01 HOLD_BINDING을 유지한다. 문서에 필드가 존재하는 것만으로 실제 ESP32 헤더와 Encoder 연결이 완료된 것은 아니다.

## Query가 통과하는 호출부터 찾기

| 단계 | 정확한 원형·Data로 이동 | 역할·미정 경계 |
|---|---|---|
| BLE → Core | [OnMobileMessage](#api-gateway_interface_onmobilemessage), [RelayMessage 전체 선언](12_GATEWAY_INTERFACE_TYPES.md#type-gateway-relaymessageview-t), [NetMobileQuery 전체 선언](../DOMAIN/14_MOBILE_CODEC_DATA.md#type-netmobilequery) | 현재 인증 peer/Session, 원 QueryID/scope와 조회 대상 RequestID를 구분한다. |
| Core → UART-M | [SendToDomain](#api-gateway_interface_sendtodomain), [23 TYPE 내부 convention](25_PRIVATE_ROUTING.md#relay-schema) | local OK 전에 안정된 복사/encode. TYPE+P convention은 PROPOSED/BINDING_OPEN이다. |
| Domain 처리·응답 | [Domain private Query 처리](../DOMAIN/25_PRIVATE_ROUTING.md#query-flow), [원 TX 완료 count 규칙](../DOMAIN/25_PRIVATE_ROUTING.md#r2-query-producer) | Getter 출력 성공과 응답 송신 완료는 다르다. M_QUERY_END count는 queue 접수 수가 아니다. |
| End 수신 | [EspNetPriv_OnQueryEnd 정확한 원형](25_PRIVATE_ROUTING.md#api-espnetpriv_onqueryend), [수신·종결 판단](25_PRIVATE_ROUTING.md#r2-query-consumer) | End는 private; 현재 문맥/유효 수신 수/부분 상태를 확인한다. BLE 종결 표현·실제 TX/수신 연결은 BINDING_OPEN이다. |

## 호출 경계

BLE/UART Adapter→Core는 `Gateway_Interface_OnMobileMessage/OnDomainMessage`이다. Core→Transport는 `SendToDomain/SendToMobile`이다. 등록·연결/앱 활성/근접의 typed Publish3 종을 M_CONTEXT로 합성한다. 메시지 subtype 별 중계에 새 23 개 공개 함수를 요구하지 않는다.

## 원형 바로 찾기

아래 원형을 선택하면 바로 아래의 호출 주체·인자·수명·실패 설명으로 이동한다. **DECLARED_SOURCE / BINDING_OPEN**이며 Header의 실제 선언을 유지한다.

### 로컬 초기화·형식 검사

| 정확한 반환형·함수·전체 인자 | 사용 설명 |
|---|---|
| `Gateway_Status_t Gateway_Interface_Init( const Gateway_InterfaceCoreHandlers_t *core_handlers, const Gateway_InterfaceTransportPorts_t *transport_ports);` | [호출·결과·실패](#api-gateway_interface_init) |
| `void Gateway_Interface_Reset(void);` | [호출·결과·실패](#api-gateway_interface_reset) |
| `bool Gateway_Interface_IsInitialized(void);` | [호출·결과·실패](#api-gateway_interface_isinitialized) |
| `bool Gateway_Interface_IsValidByteView( const Gateway_ByteView_t *view);` | [호출·결과·실패](#api-gateway_interface_isvalidbyteview) |
| `bool Gateway_Interface_IsValidMessageContext( const Gateway_MessageContextView_t *context);` | [호출·결과·실패](#api-gateway_interface_isvalidmessagecontext) |
| `bool Gateway_Interface_IsValidRelayMessage( const Gateway_RelayMessageView_t *message);` | [호출·결과·실패](#api-gateway_interface_isvalidrelaymessage) |

### 수신: Adapter → Core

| 정확한 반환형·함수·전체 인자 | 사용 설명 |
|---|---|
| `Gateway_Status_t Gateway_Interface_OnMobileMessage( const Gateway_RelayMessageView_t *message);` | [호출·결과·실패](#api-gateway_interface_onmobilemessage) |
| `Gateway_Status_t Gateway_Interface_OnDomainMessage( const Gateway_RelayMessageView_t *message);` | [호출·결과·실패](#api-gateway_interface_ondomainmessage) |

### 송신: Core → Transport

| 정확한 반환형·함수·전체 인자 | 사용 설명 |
|---|---|
| `Gateway_Status_t Gateway_Interface_SendToDomain( const Gateway_RelayMessageView_t *message);` | [호출·결과·실패](#api-gateway_interface_sendtodomain) |
| `Gateway_Status_t Gateway_Interface_SendToMobile( const Gateway_RelayMessageView_t *message);` | [호출·결과·실패](#api-gateway_interface_sendtomobile) |
| `Gateway_Status_t Gateway_Interface_PublishRegistrationConnection( const Gateway_RegistrationConnectionUpdate_t *update);` | [호출·결과·실패](#api-gateway_interface_publishregistrationconnection) |
| `Gateway_Status_t Gateway_Interface_PublishAppActivity( const Gateway_AppActivityUpdate_t *update);` | [호출·결과·실패](#api-gateway_interface_publishappactivity) |
| `Gateway_Status_t Gateway_Interface_PublishProximity( const Gateway_ProximityUpdate_t *update);` | [호출·결과·실패](#api-gateway_interface_publishproximity) |

<a id="ports"></a>
## 공통 호출·반환·수명 계약

ByteView와 nested payload는 호출이 끝날 때까지 참조로 빌려 사용하는 자료다. Core/Transport callback이 비동기로 보관하려면 **local OK 전에 Network-owned bounded deep copy/동기 encode**를 확보한다. 등록 table 값복사는 pointed byte data 또는 user_context의 자동복사를 뜻하지 않는다. owner 처리 문맥과 비차단 callback을 제안하며 실제 Firmware/SDK binding은 미확인이다.

| Network 내부 상태 | Gateway callback 반환 제안 | 한계 |
|---|---|---|
| `NET_SUBMIT_OK` | `GATEWAY_STATUS_OK` | local 복사/encode만 보장. |
| `NET_SUBMIT_INVALID_ARGUMENT` | `GATEWAY_STATUS_INVALID_ARGUMENT` | schema·길이·방향/예약 코드 오류. |
| `NET_SUBMIT_NOT_READY` | `GATEWAY_STATUS_NOT_READY` | 현재 peer/Session/링크 준비 안 됨. |
| `NET_SUBMIT_NO_CAPACITY` | `GATEWAY_STATUS_NOT_READY` | source에 QUEUE_FULL 없음. capacity 원인은 기존 Network 진단 근거에 보존하며 유효해진 뒤 원 ID로 재제출. |
| NULL callback/지원 안 됨 | `GATEWAY_STATUS_UNSUPPORTED` | init 자체 실패로 소급하지 않음. |
| 설명할 수 없는 내부 오류 | `GATEWAY_STATUS_INTERNAL_ERROR` | 성공·차량 FAILED로 바꾸지 않음. |

**PROPOSED / NETWORK_OWNED + IMPLEMENTATION_BINDING:** 기존 source enum 안에서 **enqueue 전의 일시적 capacity 부족은 `GATEWAY_STATUS_NOT_READY`**로 반환하는 안을 권장한다. 호출자 borrowed bytes를 보관하지 않고 local OK/차량 ACCEPTED를 만들지 않는다. 재제출은 동일 원 Request/Session/Query ID를 유지하며 현재 peer·수명·중복을 다시 검사한다. 공개 반환코드 확장 없이 일시 준비 불가라는 의미를 보존하므로, 일시적인 큐 공간 부족을 구조·인자 오류나 성공으로 바꾸는 안보다 적합하다. 실제 capacity 원인은 기존 Network 로컬 진단에 보존한다.

이미 OK로 안정된 복사/encode를 인수한 뒤 실패하면 과거 반환을 NOT_READY로 소급하거나 사용자에게 무조건 같은 명령을 재실행시키지 않는다. 최종 실패는 기존 Transport/Network 감독의 해당 work/Query 식별자와 함께 인계하며 source callback 시점의 내부 오류는 기존 `GATEWAY_STATUS_INTERNAL_ERROR`를 사용한다. Request 실패를 차량 FAILED로, Query 실패를 STATUS0 완료로 합성하지 않는다. sender가 실제 송신 여부를 모르면 결과 확인/새 Query 경로를 사용하고 자동 중복 실행을 만들지 않는다.

**Gateway 반환·연동 조건:** 호출자는 NOT_READY 이후의 안전한 재제출과 실패 원인 진단을 구분해야 한다. 로컬 접수 성공 이후 작업 ID와 최종 송신 실패를 기존 Transport 감독에 연결할 관측 지점이 필요하다. 그 연결 전에는 후발 실패 관측이 `BINDING_OPEN`이다. 새 공개 실패 처리 API/DTO는 정의하지 않는다. 반환 규칙은 `PROPOSED`이며 실제 Firmware/Codec 동작은 검증 전이다.

<a id="api-gateway_interface_init"></a>
## `Gateway_Interface_Init()`

```c
Gateway_Status_t Gateway_Interface_Init( const Gateway_InterfaceCoreHandlers_t *core_handlers, const Gateway_InterfaceTransportPorts_t *transport_ports);
```

**방향:** 로컬 관리/검증 · **지위:** DECLARED_SOURCE / BINDING_OPEN.

**용도·호출자·시점:** ESP32 통합 owner가 Core 핸들러/Transport Port table을 등록한다. NULL callback은 허용되며 해당 기능 호출 때 UNSUPPORTED를 반환한다.

| 인자 | 정확한 타입 | IN/OUT | 사용·수명 |
|---|---|---|---|
| `core_handlers` | [`const Gateway_InterfaceCoreHandlers_t *`](12_GATEWAY_INTERFACE_TYPES.md#type-gateway-interfacecorehandlers-t) | IN / borrowed | Gateway Core handler table. callback/user_context의 등록 수명 유지; NULL callback은 사용 시 UNSUPPORTED. |
| `transport_ports` | [`const Gateway_InterfaceTransportPorts_t *`](12_GATEWAY_INTERFACE_TYPES.md#type-gateway-interfacetransportports-t) | IN / borrowed | Gateway logical Transport callback table. NULL callback은 사용 시 UNSUPPORTED; borrowed view 포인터를 비동기에 그대로 보관 금지. |

**반환·호출:** 로컬 Core 입력/관리 결과. source 상태·실패·수명은 [공통 호출 계약](#ports)을 따른다.

<a id="api-gateway_interface_reset"></a>
## `Gateway_Interface_Reset()`

```c
void Gateway_Interface_Reset(void);
```

**방향:** 로컬 관리/검증 · **지위:** DECLARED_SOURCE / BINDING_OPEN.

**용도·호출자·시점:** Gateway 로컬 lifecycle owner가 경계등록/상태를 reset 한다. 원 Request/Session의 in-flight 정리와 실제 BLE 연결은 별도 binding 계약이다.

**반환·호출:** 로컬 등록/상태 reset. 그 이후 in-flight borrowed 객체·Session의 처리 정책을 별도로 검증하며 차량 결과를 만들지 않는다.

<a id="api-gateway_interface_isinitialized"></a>
## `Gateway_Interface_IsInitialized()`

```c
bool Gateway_Interface_IsInitialized(void);
```

**방향:** 로컬 관리/검증 · **지위:** DECLARED_SOURCE / BINDING_OPEN.

**용도·호출자·시점:** Gateway 호출자가 로컬 논리경계의 초기화 여부를 확인한다. BLE/Domain peer 유효 통신이 확보됐다는 뜻이 아니다.

**반환·호출:** 로컬 초기화 여부만 반환. 통신/차량 준비 완료를 뜻하지 않는다.

<a id="api-gateway_interface_onmobilemessage"></a>
## `Gateway_Interface_OnMobileMessage()`

```c
Gateway_Status_t Gateway_Interface_OnMobileMessage( const Gateway_RelayMessageView_t *message);
```

**방향:** Adapter→Core · **지위:** DECLARED_SOURCE / BINDING_OPEN.

**용도·호출자·시점:** BLE Adapter가 현재 등록 peer/검증된 Session에 묶인 MOBILE Request/Query/ACK를 schema 검증 후 Core handler로 전달한다.

| 인자 | 정확한 타입 | IN/OUT | 사용·수명 |
|---|---|---|---|
| `message` | [`const Gateway_RelayMessageView_t *`](12_GATEWAY_INTERFACE_TYPES.md#type-gateway-relaymessageview-t) | IN / borrowed | 호출 동안 유효한 `Gateway_RelayMessageView_t` 객체. 아래 용도와 연결된 원 owner의 OnMobileMessage 의미를 제공한다. [전체 Data](10_DATA.md)의 필드별 품질/식별/AGE를 적용한다. |

**반환·호출:** 로컬 Core 입력/관리 결과. source 상태·실패·수명은 [공통 호출 계약](#ports)을 따른다.

**사용:** [TYPE·방향·Context·payload 검증](25_PRIVATE_ROUTING.md#relay-schema)을 적용한다. ALIVE/QUERY_END는 일반 relay 대신 private로 처리한다.

<a id="api-gateway_interface_ondomainmessage"></a>
## `Gateway_Interface_OnDomainMessage()`

```c
Gateway_Status_t Gateway_Interface_OnDomainMessage( const Gateway_RelayMessageView_t *message);
```

**방향:** Adapter→Core · **지위:** DECLARED_SOURCE / BINDING_OPEN.

**용도·호출자·시점:** UART-M Adapter가 DOMAIN에서 수신한 일반 상태·결과·경고의 원시 데이터를 검증하고 형식별로 분류한 뒤 Core 핸들러에 전달한다. ALIVE/QUERY_END는 private 감독경로다.

| 인자 | 정확한 타입 | IN/OUT | 사용·수명 |
|---|---|---|---|
| `message` | [`const Gateway_RelayMessageView_t *`](12_GATEWAY_INTERFACE_TYPES.md#type-gateway-relaymessageview-t) | IN / borrowed | 호출 동안 유효한 `Gateway_RelayMessageView_t` 객체. 아래 용도와 연결된 원 owner의 OnDomainMessage 의미를 제공한다. [전체 Data](10_DATA.md)의 필드별 품질/식별/AGE를 적용한다. |

**반환·호출:** 로컬 Core 입력/관리 결과. source 상태·실패·수명은 [공통 호출 계약](#ports)을 따른다.

**사용:** [TYPE·방향·Context·payload 검증](25_PRIVATE_ROUTING.md#relay-schema)을 적용한다. ALIVE/QUERY_END는 일반 relay 대신 private로 처리한다.

<a id="api-gateway_interface_sendtodomain"></a>
## `Gateway_Interface_SendToDomain()`

```c
Gateway_Status_t Gateway_Interface_SendToDomain( const Gateway_RelayMessageView_t *message);
```

**방향:** Core→Transport · **지위:** DECLARED_SOURCE / BINDING_OPEN.

**용도·호출자·시점:** Gateway Core가 검증된 MOBILE Request/Query/ACK를 UART Transport에 제출한다. schema별 원 ID를 보존하고 차량 수용/실행정책은 Domain에 맡긴다.

| 인자 | 정확한 타입 | IN/OUT | 사용·수명 |
|---|---|---|---|
| `message` | [`const Gateway_RelayMessageView_t *`](12_GATEWAY_INTERFACE_TYPES.md#type-gateway-relaymessageview-t) | IN / borrowed | 호출 동안 유효한 `Gateway_RelayMessageView_t` 객체. 아래 용도와 연결된 원 owner의 SendToDomain 의미를 제공한다. [전체 Data](10_DATA.md)의 필드별 품질/식별/AGE를 적용한다. |

**반환·호출:** 로컬 Transport 수용 결과. 실제 BLE/ECU 수신·DONE 아님; borrowed 수명과 반환 mapping은 [공통 호출 계약](#ports)을 따른다.

**사용:** [TYPE·방향·Context·payload 검증](25_PRIVATE_ROUTING.md#relay-schema)을 적용한다. ALIVE/QUERY_END는 일반 relay 대신 private로 처리한다.

<a id="api-gateway_interface_sendtomobile"></a>
## `Gateway_Interface_SendToMobile()`

```c
Gateway_Status_t Gateway_Interface_SendToMobile( const Gateway_RelayMessageView_t *message);
```

**방향:** Core→Transport · **지위:** DECLARED_SOURCE / BINDING_OPEN.

**용도·호출자·시점:** Gateway Core가 검증된 DOMAIN 상태·결과·경고를 BLE Transport로 중계한다. 원본 차량 의미·RequestSession·품질·AGE를 변경하지 않는다.

| 인자 | 정확한 타입 | IN/OUT | 사용·수명 |
|---|---|---|---|
| `message` | [`const Gateway_RelayMessageView_t *`](12_GATEWAY_INTERFACE_TYPES.md#type-gateway-relaymessageview-t) | IN / borrowed | 호출 동안 유효한 `Gateway_RelayMessageView_t` 객체. 아래 용도와 연결된 원 owner의 SendToMobile 의미를 제공한다. [전체 Data](10_DATA.md)의 필드별 품질/식별/AGE를 적용한다. |

**반환·호출:** 로컬 Transport 수용 결과. 실제 BLE/ECU 수신·DONE 아님; borrowed 수명과 반환 mapping은 [공통 호출 계약](#ports)을 따른다.

**사용:** [TYPE·방향·Context·payload 검증](25_PRIVATE_ROUTING.md#relay-schema)을 적용한다. ALIVE/QUERY_END는 일반 relay 대신 private로 처리한다.

<a id="api-gateway_interface_publishregistrationconnection"></a>
## `Gateway_Interface_PublishRegistrationConnection()`

```c
Gateway_Status_t Gateway_Interface_PublishRegistrationConnection( const Gateway_RegistrationConnectionUpdate_t *update);
```

**방향:** Core→Transport · **지위:** DECLARED_SOURCE / BINDING_OPEN.

**용도·호출자·시점:** DeviceRegistrationManager 등 실제 등록/연결 owner가 공동 관측 tuple을 Transport에 제출한다. 원 observation_revision을 사용하고 Session을 새로 발급하지 않는다.

| 인자 | 정확한 타입 | IN/OUT | 사용·수명 |
|---|---|---|---|
| `update` | [`const Gateway_RegistrationConnectionUpdate_t *`](12_GATEWAY_INTERFACE_TYPES.md#type-gateway-registrationconnectionupdate-t) | IN / borrowed | 호출 동안 유효한 `Gateway_RegistrationConnectionUpdate_t` 객체. 아래 용도와 연결된 원 owner의 PublishRegistrationConnection 의미를 제공한다. [전체 Data](10_DATA.md)의 필드별 품질/식별/AGE를 적용한다. |

**반환·호출:** 로컬 Transport 수용 결과. 실제 BLE/ECU 수신·DONE 아님; borrowed 수명과 반환 mapping은 [공통 호출 계약](#ports)을 따른다.

<a id="api-gateway_interface_publishappactivity"></a>
## `Gateway_Interface_PublishAppActivity()`

```c
Gateway_Status_t Gateway_Interface_PublishAppActivity( const Gateway_AppActivityUpdate_t *update);
```

**방향:** Core→Transport · **지위:** DECLARED_SOURCE / BINDING_OPEN.

**용도·호출자·시점:** 신뢰 가능한 MOBILE App 활성 producer를 검증한 owner가 독립 update를 Transport에 제출한다. BLE 연결 또는 근접 갱신으로 ACTIVE를 합성하지 않는다.

| 인자 | 정확한 타입 | IN/OUT | 사용·수명 |
|---|---|---|---|
| `update` | [`const Gateway_AppActivityUpdate_t *`](12_GATEWAY_INTERFACE_TYPES.md#type-gateway-appactivityupdate-t) | IN / borrowed | 호출 동안 유효한 `Gateway_AppActivityUpdate_t` 객체. 아래 용도와 연결된 원 owner의 PublishAppActivity 의미를 제공한다. [전체 Data](10_DATA.md)의 필드별 품질/식별/AGE를 적용한다. |

**반환·호출:** 로컬 Transport 수용 결과. 실제 BLE/ECU 수신·DONE 아님; borrowed 수명과 반환 mapping은 [공통 호출 계약](#ports)을 따른다.

<a id="api-gateway_interface_publishproximity"></a>
## `Gateway_Interface_PublishProximity()`

```c
Gateway_Status_t Gateway_Interface_PublishProximity( const Gateway_ProximityUpdate_t *update);
```

**방향:** Core→Transport · **지위:** DECLARED_SOURCE / BINDING_OPEN.

**용도·호출자·시점:** ESP32 근접 담당 모듈이 실제 근접평가와 독립 revision/AGE/quality를 Transport에 제출한다. 미수신/STALE/INVALID를 FAR로 만들지 않는다.

| 인자 | 정확한 타입 | IN/OUT | 사용·수명 |
|---|---|---|---|
| `update` | [`const Gateway_ProximityUpdate_t *`](12_GATEWAY_INTERFACE_TYPES.md#type-gateway-proximityupdate-t) | IN / borrowed | 호출 동안 유효한 `Gateway_ProximityUpdate_t` 객체. 아래 용도와 연결된 원 owner의 PublishProximity 의미를 제공한다. [전체 Data](10_DATA.md)의 필드별 품질/식별/AGE를 적용한다. |

**반환·호출:** 로컬 Transport 수용 결과. 실제 BLE/ECU 수신·DONE 아님; borrowed 수명과 반환 mapping은 [공통 호출 계약](#ports)을 따른다.

<a id="api-gateway_interface_isvalidbyteview"></a>
## `Gateway_Interface_IsValidByteView()`

```c
bool Gateway_Interface_IsValidByteView( const Gateway_ByteView_t *view);
```

**방향:** 로컬 관리/검증 · **지위:** DECLARED_SOURCE / BINDING_OPEN.

**용도·호출자·시점:** Gateway 로컬 validator가 byteview의 NULL/length 형식만 검사한다. 인증·실제 ID 폭·payload schema 검증은 추가 책임이다.

| 인자 | 정확한 타입 | IN/OUT | 사용·수명 |
|---|---|---|---|
| `view` | [`const Gateway_ByteView_t *`](12_GATEWAY_INTERFACE_TYPES.md#type-gateway-byteview-t) | IN / borrowed | borrowed ByteView의 NULL/length 조합 검사. 길이 0은 data NULL, 길이>0은 data non-NULL. |

**반환·호출:** 로컬 객체 형식 검증 true/false. ByteView 검사 성공만으로 인증·Session·raw schema 유효성을 주장하지 않는다.

<a id="api-gateway_interface_isvalidmessagecontext"></a>
## `Gateway_Interface_IsValidMessageContext()`

```c
bool Gateway_Interface_IsValidMessageContext( const Gateway_MessageContextView_t *context);
```

**방향:** 로컬 관리/검증 · **지위:** DECLARED_SOURCE / BINDING_OPEN.

**용도·호출자·시점:** Gateway 로컬 validator가 contextview 들의 형식을 검사한다. 실제 등록 peer/Session 일치·u32/u64/u32 폭 검증을 이것만으로 완료했다고 주장하지 않는다.

| 인자 | 정확한 타입 | IN/OUT | 사용·수명 |
|---|---|---|---|
| `context` | [`const Gateway_MessageContextView_t *`](12_GATEWAY_INTERFACE_TYPES.md#type-gateway-messagecontextview-t) | IN / borrowed | 현재 schema가 요구하는 context views. 단말 u32/Session u64/Request u32 정확 폭·peerbinding은 adapter 추가 검증. |

**반환·호출:** 로컬 객체 형식 검증 true/false. ByteView 검사 성공만으로 인증·Session·raw schema 유효성을 주장하지 않는다.

**사용:** [TYPE·방향·Context·payload 검증](25_PRIVATE_ROUTING.md#relay-schema)을 적용한다. ALIVE/QUERY_END는 일반 relay 대신 private로 처리한다.

<a id="api-gateway_interface_isvalidrelaymessage"></a>
## `Gateway_Interface_IsValidRelayMessage()`

```c
bool Gateway_Interface_IsValidRelayMessage( const Gateway_RelayMessageView_t *message);
```

**방향:** 로컬 관리/검증 · **지위:** DECLARED_SOURCE / BINDING_OPEN.

**용도·호출자·시점:** Gateway 로컬 validator가 relaytype/direction/context/payload 형식을 검사한다. 상세 상태 subtype의 정확한 schema와 의미 유효성은 registry 검증을 추가한다.

| 인자 | 정확한 타입 | IN/OUT | 사용·수명 |
|---|---|---|---|
| `message` | [`const Gateway_RelayMessageView_t *`](12_GATEWAY_INTERFACE_TYPES.md#type-gateway-relaymessageview-t) | IN / borrowed | 호출 동안 유효한 `Gateway_RelayMessageView_t` 객체. 아래 용도와 연결된 원 owner의 IsValidRelayMessage 의미를 제공한다. [전체 Data](10_DATA.md)의 필드별 품질/식별/AGE를 적용한다. |

**반환·호출:** 로컬 객체 형식 검증 true/false. ByteView 검사 성공만으로 인증·Session·raw schema 유효성을 주장하지 않는다.

**사용:** [TYPE·방향·Context·payload 검증](25_PRIVATE_ROUTING.md#relay-schema)을 적용한다. ALIVE/QUERY_END는 일반 relay 대신 private로 처리한다.
