# ESP32 — Gateway_Interface.h 전체 의미 타입

Interface v2 · r1 · [규격과 연동 조건](../../07_CROSS_NODE_INTERFACE_RULES.md#status)

**DECLARED_SOURCE / BINDING_OPEN.** 아래 코드 블록은 참조 헤더의 typedef 선언이다. 설명 표는 원본 데이터의 생성 주체·유효성·수명을 명시한다. 이 문서는 컴파일용 헤더가 아니며 C enum/padding을 Wire로 복사하지 않는다.

[노드 Data](10_DATA.md) · [노드 API](20_API.md) · [원 선언 헤더](https://github.com/vehicle-domain-control-system/integrated-vehicle-control/blob/9f0bbed4f2a2444111bc71e756103912c983993b/docs/esp32/3_Gateway_Interface/Gateway_Interface.h)

이 헤더 사본은 본문 선언 13개와 일치함을 확인한 참고자료다. include 의존성을 갖춘 빌드 헤더 묶음은 아니며, 실제 ESP32 구현 채택을 뜻하지 않는다.

## 타입 바로 찾기

| 사용하는 의미 | 전체 선언·필드 표 |
|---|---|
| Borrowed view·Context | [`Gateway_ByteView_t`](#type-gateway-byteview-t), [`Gateway_MessageContextView_t`](#type-gateway-messagecontextview-t), [`Gateway_RelayMessageView_t`](#type-gateway-relaymessageview-t) |
| 서로 다른 실제 producer update | [`Gateway_RegistrationConnectionUpdate_t`](#type-gateway-registrationconnectionupdate-t), [`Gateway_AppActivityUpdate_t`](#type-gateway-appactivityupdate-t), [`Gateway_ProximityUpdate_t`](#type-gateway-proximityupdate-t) |
| Adapter → Core callback | [`Gateway_InterfaceMessageHandler_t`](#type-gateway-interfacemessagehandler-t), [`Gateway_InterfaceCoreHandlers_t`](#type-gateway-interfacecorehandlers-t) |
| Core → Transport callback | [`Gateway_InterfaceRelaySender_t`](#type-gateway-interfacerelaysender-t), [`Gateway_InterfaceRegistrationSender_t`](#type-gateway-interfaceregistrationsender-t), [`Gateway_InterfaceAppActivitySender_t`](#type-gateway-interfaceappactivitysender-t), [`Gateway_InterfaceProximitySender_t`](#type-gateway-interfaceproximitysender-t), [`Gateway_InterfaceTransportPorts_t`](#type-gateway-interfacetransportports-t) |

<a id="type-gateway-byteview-t"></a>
## `Gateway_ByteView_t`

**원 소속:** `Gateway_Interface.h` · **지위:** DECLARED_SOURCE · **소비:** [노드 API](20_API.md).

```c
typedef struct
{
    const uint8_t *data;
    size_t length;
} Gateway_ByteView_t;
```

| 멤버 | 정확한 C 타입 | 역할·범위·유효성 | 원 생성자 |
|---|---|---|---|
| `data` | `const uint8_t *` | 호출 동안 유효한 borrowed byte pointer. length=0 이면 NULL, length>0 이면 non-NULL. | ESP32 Gateway / 해당 BLE·등록·link owner |
| `length` | `size_t` | view byte 길이 size_t. NULL 조합 및 ID 별 폭 검사 필요. 용량은 길이와 별개. | ESP32 Gateway / 해당 BLE·등록·link owner |

<a id="type-gateway-messagecontextview-t"></a>
## `Gateway_MessageContextView_t`

**원 소속:** `Gateway_Interface.h` · **지위:** DECLARED_SOURCE · **소비:** [노드 API](20_API.md).

```c
typedef struct
{
    Gateway_ByteView_t vehicle_id;
    Gateway_ByteView_t device_context_id;
    Gateway_ByteView_t session_id;
    Gateway_ByteView_t request_id;
} Gateway_MessageContextView_t;
```

| 멤버 | 정확한 C 타입 | 역할·범위·유효성 | 원 생성자 |
|---|---|---|---|
| `vehicle_id` | [`Gateway_ByteView_t`](12_GATEWAY_INTERFACE_TYPES.md#type-gateway-byteview-t) | 논리 차량 식별 opaque view. UART-M에 필드 없으므로 차량 binding owner의 사전 확인으로 소비, 새 wire 필드 금지. | ESP32 Gateway / 해당 BLE·등록·link owner |
| `device_context_id` | [`Gateway_ByteView_t`](12_GATEWAY_INTERFACE_TYPES.md#type-gateway-byteview-t) | 검증된 단말 context view. UART-M 투영은 Device u32. raw MAC/이름/비밀 key로 대신하지 않는다. | ESP32 Gateway / 해당 BLE·등록·link owner |
| `session_id` | [`Gateway_ByteView_t`](12_GATEWAY_INTERFACE_TYPES.md#type-gateway-byteview-t) | 검증된 현재 Session view. UART-M 투영 Session u64. 발급 owner/교체 규칙 FUNCTION_POLICY_OPEN. | ESP32 Gateway / 해당 BLE·등록·link owner |
| `request_id` | [`Gateway_ByteView_t`](12_GATEWAY_INTERFACE_TYPES.md#type-gateway-byteview-t) | 원 MOBILE Request view(u32). QueryID/Occurrence가 아니며, Request 없는 schema는 empty view다. M_QUERY에 대상 REQUEST_ID가 있으면 그 원값을 사용한다. [Context 존재·종류 검사](25_PRIVATE_ROUTING.md#relay-schema). | ESP32 Gateway / 해당 BLE·등록·link owner |

<a id="type-gateway-relaymessageview-t"></a>
## `Gateway_RelayMessageView_t`

**원 소속:** `Gateway_Interface.h` · **지위:** DECLARED_SOURCE · **소비:** [노드 API](20_API.md).

```c
typedef struct
{
    Gateway_LogicalMessageType_t type;
    Gateway_MessageDirection_t direction;
    Gateway_MessageContextView_t context;
    Gateway_ByteView_t payload;
} Gateway_RelayMessageView_t;
```

| 멤버 | 정확한 C 타입 | 역할·범위·유효성 | 원 생성자 |
|---|---|---|---|
| `type` | [`Gateway_LogicalMessageType_t`](11_GATEWAY_TYPES.md#type-gateway-logicalmessagetype-t) | Gateway 논리 category. M_* raw TYPE가 아니며 상세 subtype은 schema registry와 payload로 검증. | ESP32 Gateway / 해당 BLE·등록·link owner |
| `direction` | [`Gateway_MessageDirection_t`](11_GATEWAY_TYPES.md#type-gateway-messagedirection-t) | 논리 흐름 MOBILE_TO_DOMAIN/DOMAIN_TO_MOBILE 등. transport port가 허용 방향과 대조. | ESP32 Gateway / 해당 BLE·등록·link owner |
| `context` | [`Gateway_MessageContextView_t`](12_GATEWAY_INTERFACE_TYPES.md#type-gateway-messagecontextview-t) | 해당 객체의 원 요청/명령 문맥. 하위 타입 전체 정의와 유효성 조건을 함께 적용한다. | ESP32 Gateway / 해당 BLE·등록·link owner |
| `payload` | [`Gateway_ByteView_t`](12_GATEWAY_INTERFACE_TYPES.md#type-gateway-byteview-t) | transport framing을 제거한 논리 payload borrowed view. [PROPOSED TYPE+P convention](25_PRIVATE_ROUTING.md#relay-schema)은 BINDING_OPEN이다. Wire 전체/C struct raw bytes 아님; subtype·정확한 길이 검증. | ESP32 Gateway / 해당 BLE·등록·link owner |

<a id="type-gateway-registrationconnectionupdate-t"></a>
## `Gateway_RegistrationConnectionUpdate_t`

**원 소속:** `Gateway_Interface.h` · **지위:** DECLARED_SOURCE · **소비:** [노드 API](20_API.md).

```c
typedef struct
{
    Gateway_MessageContextView_t context;
    Gateway_RegistrationState_t registration;
    Gateway_ConnectionState_t connection;
    Gateway_UpdateBasis_t update; /* ONE shared quality/age/update_sequence;
        DeviceRegistrationManager.observation_revision -> update_sequence */
} Gateway_RegistrationConnectionUpdate_t;
```

| 멤버 | 정확한 C 타입 | 역할·범위·유효성 | 원 생성자 |
|---|---|---|---|
| `context` | [`Gateway_MessageContextView_t`](12_GATEWAY_INTERFACE_TYPES.md#type-gateway-messagecontextview-t) | 검증된 peer/device/current Session binding. 각 ByteView 폭/존재와 source TTL 대조. | ESP32 Gateway / 해당 BLE·등록·link owner |
| `registration` | [`Gateway_RegistrationState_t`](11_GATEWAY_TYPES.md#type-gateway-registrationstate-t) | 검증된 등록 단말 여부. BLE 연결·이름·주소만으로 등록 성공 생성 금지. | ESP32 Gateway / 해당 BLE·등록·link owner |
| `connection` | [`Gateway_ConnectionState_t`](11_GATEWAY_TYPES.md#type-gateway-connectionstate-t) | 검증된 현재 BLE CONNECTED/DISCONNECTED. registration과 한 공동 update tuple을 사용. | ESP32 Gateway / 해당 BLE·등록·link owner |
| `update` | [`Gateway_UpdateBasis_t`](11_GATEWAY_TYPES.md#type-gateway-updatebasis-t) | 해당 Gateway 관측의 quality/age_ms/update_sequence. 타입 공유는 카운터 공유가 아니다. | ESP32 Gateway / 해당 BLE·등록·link owner |

<a id="type-gateway-appactivityupdate-t"></a>
## `Gateway_AppActivityUpdate_t`

**원 소속:** `Gateway_Interface.h` · **지위:** DECLARED_SOURCE · **소비:** [노드 API](20_API.md).

```c
typedef struct
{
    Gateway_MessageContextView_t context;
    Gateway_AppActivityState_t state;
    Gateway_UpdateBasis_t update;
} Gateway_AppActivityUpdate_t;
```

| 멤버 | 정확한 C 타입 | 역할·범위·유효성 | 원 생성자 |
|---|---|---|---|
| `context` | [`Gateway_MessageContextView_t`](12_GATEWAY_INTERFACE_TYPES.md#type-gateway-messagecontextview-t) | 해당 객체의 원 요청/명령 문맥. 하위 타입 전체 정의와 유효성 조건을 함께 적용한다. | MOBILE 원 활성 producer→ESP32 검증 owner |
| `state` | [`Gateway_AppActivityState_t`](11_GATEWAY_TYPES.md#type-gateway-appactivitystate-t) | 해당 타입의 의미 상태. 타입별 enum의 UNKNOWN/UNCONFIRMED/STALE를 정상 CLEAR/FAR로 만들지 않는다. | MOBILE 원 활성 producer→ESP32 검증 owner |
| `update` | [`Gateway_UpdateBasis_t`](11_GATEWAY_TYPES.md#type-gateway-updatebasis-t) | 해당 Gateway 관측의 quality/age_ms/update_sequence. 타입 공유는 카운터 공유가 아니다. | MOBILE 원 활성 producer→ESP32 검증 owner |

<a id="type-gateway-proximityupdate-t"></a>
## `Gateway_ProximityUpdate_t`

**원 소속:** `Gateway_Interface.h` · **지위:** DECLARED_SOURCE · **소비:** [노드 API](20_API.md).

```c
typedef struct
{
    Gateway_MessageContextView_t context;
    Gateway_ProximityInfo_t proximity;
} Gateway_ProximityUpdate_t;
```

| 멤버 | 정확한 C 타입 | 역할·범위·유효성 | 원 생성자 |
|---|---|---|---|
| `context` | [`Gateway_MessageContextView_t`](12_GATEWAY_INTERFACE_TYPES.md#type-gateway-messagecontextview-t) | 해당 객체의 원 요청/명령 문맥. 하위 타입 전체 정의와 유효성 조건을 함께 적용한다. | ESP32 Proximity owner |
| `proximity` | [`Gateway_ProximityInfo_t`](11_GATEWAY_TYPES.md#type-gateway-proximityinfo-t) | ESP32 근접 판정과 독립 평가 근거. STALE/INVALID/미수신을 FAR로 바꾸지 않는다. | ESP32 Proximity owner |

<a id="type-gateway-interfacemessagehandler-t"></a>
## `Gateway_InterfaceMessageHandler_t`

**원 소속:** `Gateway_Interface.h` · **지위:** DECLARED_SOURCE · **소비:** [노드 API](20_API.md).

```c
typedef Gateway_Status_t (*Gateway_InterfaceMessageHandler_t)(
    const Gateway_RelayMessageView_t *message,
    void *user_context);
```

**범위·소유:** 타입이 지정된 콜백 또는 로컬 별칭이다. 가리키는 객체의 수명과 해당 Interface 콜백 계약을 함께 적용한다.

<a id="type-gateway-interfacecorehandlers-t"></a>
## `Gateway_InterfaceCoreHandlers_t`

**원 소속:** `Gateway_Interface.h` · **지위:** DECLARED_SOURCE · **소비:** [노드 API](20_API.md).

```c
typedef struct
{
    Gateway_InterfaceMessageHandler_t on_mobile_message;
    Gateway_InterfaceMessageHandler_t on_domain_message;
    void *user_context;
} Gateway_InterfaceCoreHandlers_t;
```

| 멤버 | 정확한 C 타입 | 역할·범위·유효성 | 원 생성자 |
|---|---|---|---|
| `on_mobile_message` | [`Gateway_InterfaceMessageHandler_t`](12_GATEWAY_INTERFACE_TYPES.md#type-gateway-interfacemessagehandler-t) | BLE adapter가 검증한 MOBILE 논리 메시지를 Core 처리로 전달하는 borrowed handler. | ESP32 Gateway / 해당 BLE·등록·link owner |
| `on_domain_message` | [`Gateway_InterfaceMessageHandler_t`](12_GATEWAY_INTERFACE_TYPES.md#type-gateway-interfacemessagehandler-t) | UART adapter가 검증한 Domain 논리 메시지를 Core 중계 처리로 전달하는 borrowed handler. | ESP32 Gateway / 해당 BLE·등록·link owner |
| `user_context` | `void *` | 등록자 소유 callback context pointer. 함수포인터 table만 값복사해도 이 pointed object 수명은 자동복사 아님; 등록 동안 유효. | ESP32 Gateway / 해당 BLE·등록·link owner |

<a id="type-gateway-interfacerelaysender-t"></a>
## `Gateway_InterfaceRelaySender_t`

**원 소속:** `Gateway_Interface.h` · **지위:** DECLARED_SOURCE · **소비:** [노드 API](20_API.md).

```c
typedef Gateway_Status_t (*Gateway_InterfaceRelaySender_t)(
    const Gateway_RelayMessageView_t *message,
    void *user_context);
```

**범위·소유:** 타입이 지정된 콜백 또는 로컬 별칭이다. 가리키는 객체의 수명과 해당 Interface 콜백 계약을 함께 적용한다.

<a id="type-gateway-interfaceregistrationsender-t"></a>
## `Gateway_InterfaceRegistrationSender_t`

**원 소속:** `Gateway_Interface.h` · **지위:** DECLARED_SOURCE · **소비:** [노드 API](20_API.md).

```c
typedef Gateway_Status_t (*Gateway_InterfaceRegistrationSender_t)(
    const Gateway_RegistrationConnectionUpdate_t *update,
    void *user_context);
```

**범위·소유:** 타입이 지정된 콜백 또는 로컬 별칭이다. 가리키는 객체의 수명과 해당 Interface 콜백 계약을 함께 적용한다.

<a id="type-gateway-interfaceappactivitysender-t"></a>
## `Gateway_InterfaceAppActivitySender_t`

**원 소속:** `Gateway_Interface.h` · **지위:** DECLARED_SOURCE · **소비:** [노드 API](20_API.md).

```c
typedef Gateway_Status_t (*Gateway_InterfaceAppActivitySender_t)(
    const Gateway_AppActivityUpdate_t *update,
    void *user_context);
```

**범위·소유:** 타입이 지정된 콜백 또는 로컬 별칭이다. 가리키는 객체의 수명과 해당 Interface 콜백 계약을 함께 적용한다.

<a id="type-gateway-interfaceproximitysender-t"></a>
## `Gateway_InterfaceProximitySender_t`

**원 소속:** `Gateway_Interface.h` · **지위:** DECLARED_SOURCE · **소비:** [노드 API](20_API.md).

```c
typedef Gateway_Status_t (*Gateway_InterfaceProximitySender_t)(
    const Gateway_ProximityUpdate_t *update,
    void *user_context);
```

**범위·소유:** 타입이 지정된 콜백 또는 로컬 별칭이다. 가리키는 객체의 수명과 해당 Interface 콜백 계약을 함께 적용한다.

<a id="type-gateway-interfacetransportports-t"></a>
## `Gateway_InterfaceTransportPorts_t`

**원 소속:** `Gateway_Interface.h` · **지위:** DECLARED_SOURCE · **소비:** [노드 API](20_API.md).

```c
typedef struct
{
    Gateway_InterfaceRelaySender_t send_to_domain;
    Gateway_InterfaceRelaySender_t send_to_mobile;

    Gateway_InterfaceRegistrationSender_t send_registration_to_domain;
    Gateway_InterfaceAppActivitySender_t send_app_activity_to_domain;
    Gateway_InterfaceProximitySender_t send_proximity_to_domain;

    void *user_context;
} Gateway_InterfaceTransportPorts_t;
```

| 멤버 | 정확한 C 타입 | 역할·범위·유효성 | 원 생성자 |
|---|---|---|---|
| `send_to_domain` | [`Gateway_InterfaceRelaySender_t`](12_GATEWAY_INTERFACE_TYPES.md#type-gateway-interfacerelaysender-t) | Core→UART transport callback. 논리 schema/현재 peer context 검증 후 Network-owned 복사/encode. | ESP32 Gateway / 해당 BLE·등록·link owner |
| `send_to_mobile` | [`Gateway_InterfaceRelaySender_t`](12_GATEWAY_INTERFACE_TYPES.md#type-gateway-interfacerelaysender-t) | Core→BLE transport callback. 차량 의미 원본 보존, 실제 BLE fragmentation/ACK/기술 규격 OPEN. | ESP32 Gateway / 해당 BLE·등록·link owner |
| `send_registration_to_domain` | [`Gateway_InterfaceRegistrationSender_t`](12_GATEWAY_INTERFACE_TYPES.md#type-gateway-interfaceregistrationsender-t) | 등록·연결 공동 typed update를 M_CONTEXT compose에 제공. 독립 현재 Session을 만들지 않는다. | ESP32 Gateway / 해당 BLE·등록·link owner |
| `send_app_activity_to_domain` | [`Gateway_InterfaceAppActivitySender_t`](12_GATEWAY_INTERFACE_TYPES.md#type-gateway-interfaceappactivitysender-t) | App 활성 typed update를 M_CONTEXT compose에 제공. 등록·근접 갱신에 편승해 fresh로 만들지 않는다. | ESP32 Gateway / 해당 BLE·등록·link owner |
| `send_proximity_to_domain` | [`Gateway_InterfaceProximitySender_t`](12_GATEWAY_INTERFACE_TYPES.md#type-gateway-interfaceproximitysender-t) | 근접 typed update를 M_CONTEXT compose에 제공. FAR/NEAR 판단은 Proximity owner. | ESP32 Gateway / 해당 BLE·등록·link owner |
| `user_context` | `void *` | 등록자 소유 callback context pointer. 함수포인터 table만 값복사해도 이 pointed object 수명은 자동복사 아님; 등록 동안 유효. | ESP32 Gateway / 해당 BLE·등록·link owner |
