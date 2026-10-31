# ESP32 Gateway와 MOBILE — UART·BLE 사이의 의미 인계

N05 ESP32는 Gateway이며 내부 Network 구현 전체는 ESP32 담당자 소유다. N08 MOBILE은 외부 App이다. P02 UART-M, P04 BLE, 두 링크를 잇는 R05, ESP32 내부 typed 경계 I05를 설명한다. MOBILE 내부에는 S32K의 Layer·Module·Function을 강제하지 않는다. 아래 C형 서술은 이미 문서에 존재하는 API의 계약과 호출 조건이며 ESP32 제품 코드나 App API를 새로 제공하지 않는다.

## 책임과 자료의 위치

| 경계 | 책임·관련 설계 | 담당 밖의 항목 |
|---|---|---|
| MOBILE → BLE Adapter → Gateway Core | 실제 사용자 요청·활성의 원 근거, 원 Device/Session/Request 문맥을 검증하여 typed view로 인계 | App UI·등록 정책·키·인증·BLE UUID/GATT/MTU·재연결 구현 |
| Gateway Core → UART Transport | 23 registry 중 해당 schema·방향·정확한 P 확인, 현재 peer·문맥에 결합, 안정 사본 또는 동기 encode | DOMAIN의 차량 허용·명령 결과 판단 |
| DOMAIN UART → Gateway Core → MOBILE | 유효 프레임·원 품질·AGE·원 요청 identity를 유지하며 외부 전송 계약에 인계 | UART 성공을 App 표시 완료로 선언 |
| Publish3 → M_CONTEXT | 등록/연결 공동 tuple, App 활성, 근접의 독립 생산자 평가를 보존 | BLE 연결에서 ACTIVE·REGISTERED·NEAR를 합성 |
| ALIVE/QUERY_END private | 현재 Domain Boot·유효 수신·Query 종결 근거를 별도 감독 | private 메시지를 일반 차량 결과 또는 BLE payload로 그대로 전송 |

S32K에서 상대하는 책임은 L01/M01/F01·F02의 문맥, L01/M02/F04·F05의 Query, L02/M03/F10의 typed 인계, L03/M04/F11~F13, L04/M06/F16~F18, L05/M07~M09/F19~F25다. 이는 ESP32에 같은 Task/Queue/RTOS 구조를 강제하는 적용표가 아니다. 기존 [UART-M Router](../functions/UART_M_ROUTER.md), [상향 흐름](../flows/UART_M_UPLINK.md), [하향 흐름](../flows/UART_M_DOWNLINK.md)의 정본을 재사용한다.

## 원형·Data/API 추적

아래 13개는 [ESP32 공개 API 원문](../../interface/02_NODES/ESP32/20_API.md)의 `EXISTING_DECLARATION`, 이어지는3개는 [기존 private 선언](../../interface/02_NODES/ESP32/25_PRIVATE_ROUTING.md)이다. 모두 원 Owner를 유지하며 실제 구현·등록·호출 바인딩은 OPEN이다.

```c
Gateway_Status_t Gateway_Interface_Init(const Gateway_InterfaceCoreHandlers_t *core_handlers,
                                      const Gateway_InterfaceTransportPorts_t *transport_ports);
void Gateway_Interface_Reset(void);
bool Gateway_Interface_IsInitialized(void);
bool Gateway_Interface_IsValidByteView(const Gateway_ByteView_t *view);
bool Gateway_Interface_IsValidMessageContext(const Gateway_MessageContextView_t *context);
bool Gateway_Interface_IsValidRelayMessage(const Gateway_RelayMessageView_t *message);
Gateway_Status_t Gateway_Interface_OnMobileMessage(const Gateway_RelayMessageView_t *message);
Gateway_Status_t Gateway_Interface_OnDomainMessage(const Gateway_RelayMessageView_t *message);
Gateway_Status_t Gateway_Interface_SendToDomain(const Gateway_RelayMessageView_t *message);
Gateway_Status_t Gateway_Interface_SendToMobile(const Gateway_RelayMessageView_t *message);
Gateway_Status_t Gateway_Interface_PublishRegistrationConnection(const Gateway_RegistrationConnectionUpdate_t *update);
Gateway_Status_t Gateway_Interface_PublishAppActivity(const Gateway_AppActivityUpdate_t *update);
Gateway_Status_t Gateway_Interface_PublishProximity(const Gateway_ProximityUpdate_t *update);
NetSubmitResult EspNetPriv_PublishContext(const NetMobileContext *message);
void EspNetPriv_OnDomainAlive(const NetMobileDomainAlive *message, const NetUartRxMeta *meta);
void EspNetPriv_OnQueryEnd(const NetMobileQueryEnd *message, const NetUartRxMeta *meta);
```

[중첩 타입의 전체 선언](../../interface/02_NODES/ESP32/12_GATEWAY_INTERFACE_TYPES.md)은 원문을 따른다.

| 자료 | 필드·생산자·수명 |
|---|---|
| `Gateway_ByteView_t` | `const uint8_t *data`, `size_t length`; length=0이면 NULL, 양수면 non-NULL. caller가 pointed bytes를 호출 내내 유효하게 소유 |
| `Gateway_MessageContextView_t` | vehicle_id/device_context_id/session_id/request_id 각각 ByteView. 실제 UART 투영 Device4/Session8/Request4바이트를 검증하되 vehicle_id는 UART에 없는 opaque 외부 binding |
| `Gateway_RelayMessageView_t` | type/direction/context/payload. 논리 category와 UART TYPE은 별개. struct padding·enum 정수를 Wire로 복사하지 않음 |
| `Gateway_RegistrationConnectionUpdate_t` | context, registration, connection, update. 등록·연결은 하나의 quality/age_ms/update_sequence tuple이며 observation_revision에서 연결 |
| `Gateway_AppActivityUpdate_t` | context, state, update. 실제 MOBILE 활성 생산 근거를 ESP32가 검증; 연결 관측으로 대체 불가 |
| `Gateway_ProximityUpdate_t` | context, proximity. 독립 근접 값·평가 근거; STALE/INVALID를 FAR로 만들지 않음 |
| Core/Transport 표 | Core2 slot+user_context / Transport5 slot+user_context. callback typedef5종과 slot7개를 혼동하지 않음. 표 사본은 pointed user_context의 수명을 보장하지 않음 |

정확한 열거자는 [Gateway 타입](../../interface/02_NODES/ESP32/11_GATEWAY_TYPES.md)을 따른다. Gateway_Status는 OK0/INVALID_ARGUMENT1/NOT_READY2/UNSUPPORTED3/INTERNAL_ERROR4이며 NetSubmit enum과 숫자 cast하지 않는다. `NO_CAPACITY → NOT_READY`는 원문상의 후보 매핑으로 남기고 임의로 채택하지 않는다. Gateway에 QUEUE_FULL 열거자를 추가하지 않는다. Gateway 품질 UNKNOWN0/VALID1/INVALID2/NO_DATA3/STALE4와 Wire의 품질 코드도 숫자가 다르며 UNKNOWN의 정확한 Wire 매핑은 회신 대기다.

## 함수별 정상·실패·자원 계약

| 함수 | 호출자·정상 동작 | 입력·실패·자원·복구 |
|---|---|---|
| Init | ESP32 startup Owner가 Core와 Transport의 수명 있는 표를 등록 | 표 전체 NULL 의미·실제 재등록 직렬화는 OPEN. 개별 미지원 NULL slot은 이후 해당 호출 UNSUPPORTED. 임의 unregister 의미를 만들지 않음 |
| Reset | ESP32 lifecycle Owner가 Interface 상태 초기화 요청 | void이며 Driver drain·참조 해제·Session 취소·차량 명령 완료를 보증하지 않음. 이전 callback의 epoch/token과 저장소 정리는 별도 계약 |
| IsInitialized | caller가 등록 상태를 읽음 | true가 peer/Session/전송 준비를 증명하지 않음. 동시 Reset/호출 직렬화는 실제 Owner 바인딩 |
| IsValidByteView | caller가 NULL/length 조합 검사 | 아래 순수 분기. true가 메모리 접근 가능성·내용·실제 capacity를 증명하지 않음 |
| IsValidMessageContext | caller가 네 ByteView의 구조와 해당 schema의 존재 조건 검사 | Request 없는 schema는 empty. 실제 인증/현재 Session 검사는 별도; 원형만으로 현재 peer를 읽거나 새 Session을 발급하지 않음 |
| IsValidRelayMessage | caller가 context·payload·direction/type의 유효한 조합 검사 | 중첩 pointer와 길이, 선택 schema의 정확 P 필요. 길이가 같은 메시지를 type 없이 추정하지 않음 |
| OnMobileMessage | BLE Adapter → Core의 mobile handler | 형식/방향/현재 peer 검증 실패 인계 금지. 미초기화 NOT_READY, slot미지원 UNSUPPORTED, 잘못된 인자 INVALID_ARGUMENT. OK는 Core 로컬 인수 |
| OnDomainMessage | UART Adapter → Core의 domain handler | 검증된 UART 타입/문맥/원 품질·AGE만 인계. private2 제외. OK는 BLE/App 전달 보증 아님 |
| SendToDomain | Core → UART relay transport | 유효한 상향 schema·현재 문맥·필요 복사·용량 확보 후 로컬 접수. Transport 반환 의미를 그대로 처리; 지연 참조 시 deep copy 필수 |
| SendToMobile | Core → BLE relay transport | 유효 하향 논리 자료만 인계. 실제 BLE 분할/보안/완료 계약 OPEN이며 UART Driver terminal과 별개 |
| PublishRegistrationConnection | 등록/연결 Owner → context transport | 검증된 같은 Device/Session의 registration+connection+공동update 전부 복사. BLE 이름/주소만으로 등록 성공 생성 금지 |
| PublishAppActivity | 검증된 MOBILE 활성 producer → context transport | 다른 component의 갱신과 분리. 활성 관측 소실 시 정책에 맞는 열화·기한 판단이 필요하며 최신 연결 때문에 ACTIVE 연장 금지 |
| PublishProximity | 근접 Owner → context transport | 독립 근접 평가·AGE·순번 그대로 제출. 거리/근접 정책·미수신 처리 OPEN; 다른 component로 refresh 금지 |
| EspNetPriv_PublishContext | Gateway Transport가 세 component의 동일 현재 peer binding을 확보한 뒤 제출 | NetSubmit4값. 각 평가의 품질 raw 매핑·AGE·원 순번이 증명되지 않으면 접수하지 않음. 반복송신은 LINK_SEQUENCE만 별도로 증가 |
| EspNetPriv_OnDomainAlive | 유효 UART private 수신 → Gateway 감독 | message/meta가 같은 프레임이고 현재 Domain Boot·순서·감독 품질 확인. void가 성공/SessionReady 설정의 증거 아님 |
| EspNetPriv_OnQueryEnd | 유효 UART private 수신 → Query 감독 | 현재 recipient/Boot/QueryID/scope와 실제 수신 집계에 연결. stale/unknown/mismatched End는 현재 Query 완료에 사용하지 않음 |

공통 입력 const와 nested ByteView는 borrow다. callback이 반환할 때 참조를 더 보유하려면 자기 소유 저장소에 모든 하위 bytes를 복사해야 한다. Queue에는 view 구조만 복사하지 않는다. capacity 확보·복사·enqueue는 한 인수 사건이며 실패 후 caller 소유 입력은 변경하지 않는다. 실제 Queue 깊이, lock/ISR/Task, 취소·release callback은 ESP32 Owner 바인딩을 기다린다.

<a id="relay-flow"></a>
## MOBILE 요청 상향과 결과 하향

M_REQUEST/M_QUERY/M_WARNING_ACK의 상향3종 relay는 BLE Adapter가 현재 peer의 실제 수신과 논리 schema를 검증한 후 OnMobileMessage, Core가 SendToDomain, UART Transport가 기존 Codec와 전송 수명을 적용하는 순서다. M_CONTEXT는 별도로 PublishRegistrationConnection/PublishAppActivity/PublishProximity의 세 생산 입력을 Transport가 결합하여 EspNetPriv_PublishContext에 제출한다. Publish3를 일반 MOBILE relay 호출로 대체하지 않는다. 호출 원형이 같아도 각 단계의 책임과 반환은 독립이다. 이 두 경로를 합친 상향4종의 수신 경계는 [Router](../functions/UART_M_ROUTER.md)와 [상향 정본](../flows/UART_M_UPLINK.md)을 따른다.

하향은 기존 `NetUartM_DecodeDispatch`의 정확 VERSION/profile/TYPE/P/필드 검증을 먼저 통과한다. recipient binding과 성공 OUT으로 `NetUartM_BuildDownlinkView`를 구성하고 OnDomainMessage → Core → SendToMobile로 연결한다. 원 Clock 기준이 없는 경우 RX시각을 원 AGE=0으로 사용하지 않는다. Adapter의 bindingVerified는 실제 같은 사건의 근거를 축약한 입력이며 설정 스위치나 승인 대용으로 true를 넣지 않는다.

원문 후보 `TYPE 1바이트 + P`의 내부 payload convention을 채택하려면 양단 계약이 필요하다. 이는 BLE Wire 규격이 아니다. P42가 같은 BCM_STATUS/WINDOW_STATE/WINDOW_FAULT를 길이만으로 선택하지 않는다. UART VERSION2 채택도 BLE 버전·프로파일 승인이 아니다. WINDOW 두 schema는 [WINDOW](WINDOW.md)의 실행/표시 HOLD를 유지한다.

M_RESULT의 payload 속 과거 원 RequestSession과 현재 recipient Session은 별개다. `request_id` view에는 원 RequestID 또는 M_QUERY의 선택 대상 RequestID를 넣고 QueryID·WarningOccurrence를 대입하지 않는다. Request 없는 schema는 NULL/0 empty view다. opaque vehicle_id를 새 UART 필드 또는 MAC/비밀 키로 대체하지 않는다.

### ByteView 순수 조건의 C형 의사코드

기존 원형의 계약을 설명하는 부분 본체이며 ESP32 구현 제안/제품 시험은 아니다. 별도 공개 선언을 만들지 않는다.

```c
bool Gateway_Interface_IsValidByteView(const Gateway_ByteView_t *view)
{
    if (view == NULL) {
        return false;
    }
    if (view->length == 0U) {
        return view->data == NULL;
    }
    return view->data != NULL;
}
```

위 조건을 통과해도 caller가 실제 readable 길이와 호출 중 유효성을 보장해야 한다. MessageContext와 Relay는 이 검사를 모든 중첩 view에 적용한 다음, 선택된 schema의 필수 ID·정확한 폭·type/direction/P·현재 peer 바인딩을 별도로 대조한다. 불일치하면 On/Send를 호출하지 않는다. 그 검사·변환의 C형 분기는 [기존 Router 정본](../functions/UART_M_ROUTER.md)에 있고 23개 Codec 복사본을 만들지 않는다.

<a id="private-supervision"></a>
## private 감독과 Query 종결

`M_DOMAIN_ALIVE`는 **12바이트**다. domain_boot_id u32, update_sequence u32, source_age_ms u16, domain_state u8, value_quality u8만 있다. Device/Session/SessionReady 필드가 없다. 원 Interface private 설명의 “SessionReady” 표현은 B13부터 기록된 표현 충돌이며 해당 bit가 Wire에 있다고 해석하지 않는다. 별도 실제 Session 감독 근거를 ALIVE로 대체하지 않는다. ALIVE를 App에 중계하거나 domain_state READY를 차량 기능 가용성으로 승격하지 않는다.

`M_QUERY_END`는 **24바이트**이며 AGE 필드가 없다. End의 status0=완료,1=Owner가 확정한 요청기록 없음,2=일부 미확인,3=문맥 거절이라는 기존 후보 정의를 유지한다. status0을 모든 값이 fresh라는 뜻으로 소비하지 않는다. QueryID는 nonzero이고 현재 Device/Session/DomainBoot/scope를 모두 같은 원 Query와 대조해야 한다.

| 사건 | Gateway가 할 수 있는 판단 | 완료를 막는 조건 |
|---|---|---|
| DOMAIN 응답 enqueue | 외부 결과가 아직 도착하지 않았다는 사실만 유지 | 실제 final UART TX·ESP 유효 RX 미확인 |
| End enqueue 후 Driver 실패 | 완료 근거 없음 | End가 실제 도달하지 않음; BLE/App 완료 합성 금지 |
| 유효 End 수신 | 같은 Query의 유효 응답 수·중복·누락·부분 상태와 대조 | 응답 개수 불일치, outstanding, staleBoot/Session, scope 불일치 |
| status1 요청기록 없음 | 해당 Owner의 실제 조회 결과로만 소비 | Getter 오류·실패 OUT을 없음으로 변경 금지 |
| 로컬 Query 감독 종결 | 해당 로컬 계약의 끝점 | BLE/App 종결 표시/ack 계약은 별도 OPEN |

유효 response count는 enqueue 횟수나 중복 프레임 수가 아니다. 실제 Gateway 수신 집계와 DOMAIN의 final TX 연결·중복 정의가 미확정이면 현재 Query 완료 투영은 HOLD한다. 첫 Query의 SessionReady0만으로 일괄 거절하지 않으며 필요한 현재 문맥 검증은 생략하지 않는다.

## 독립 평가·수명·재연결 반례

BLE CONNECTED가 지속되어도 App 활성 보고를 잃었다면 연결 component만 최신일 수 있다. App update_sequence/age/quality를 그대로 보존하고 실제 Owner 정책이 정한 만료·열화에 따라 처리한다. 등록·연결의 공동 tuple을 App·근접까지 공유하지 않는다. UNKNOWN 품질은 raw 코드 합의 전 해당 encode가 HOLD이며 정상 NO_DATA로 자동 변경하지 않는다.

Reset이나 재연결 뒤 과거 ByteView를 새 Session Queue에 붙이지 않는다. 이전 callback의 token/epoch는 과거 전송 정리에만 쓰고, memory release가 먼저 왔어도 결과·예약 identity는 terminal 근거까지 보존한다. 반대로 terminal이 먼저 와도 참조 해제 전 buffer를 재사용하지 않는다. 실제 ESP32 Driver/API가 이 증거를 어떻게 제공하는지는 외부 Owner의 책임이다.

ByteView NULL/0, non-NULL/0, NULL/양수, 같은 길이의 다른 TYPE, 과거 RequestSession/현재 recipient 차이, BLE연결/App활성소실, Endenqueue/실제TX실패, 재연결/latecallback을 조건표와 정본에 대입해 구조·수명·완료의 혼동을 확인했다. 실제 BLE/App 실행 시험은 수행하지 않았다.

IF-O08/09/23, CF01 및 S6Q-01~14 관련 기존 미결과 G55-A01~A05를 보존한다. 양단 raw 매핑·Session/Generation/Boot·Query 소비·BLE 계약·실제 ABI는 BINDING_OPEN이다. G6 당시 Interface39 봉인본은 유지하고 현재 별도 후보의 참조는 고정 r1 Interface40개로 구분한다. r1 문서 동기화 완료와 별개로 공식·제품 통합에는 해당 근거와 별도 승인이 필요하다.

[노드 전체](../08_NODE_BOUNDARIES.md) · [DOMAIN](DOMAIN.md) · [Gateway 원 개요](../../interface/02_NODES/ESP32/00_OVERVIEW.md) · [BLE 원 계약](../../interface/03_COMMUNICATION/40_BLE.md)
