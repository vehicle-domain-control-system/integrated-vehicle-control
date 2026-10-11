# DOMAIN↔ESP32 — Mobile Codec 전용 typed Data

**PROPOSED_DESIGN / WIRE_PROPOSED / NETWORK_PRIVATE.** 아래 전체 선언과 멤버 표는 Network 내부의 typed Codec 계약이다. 공개 source C2/Gateway 객체를 이 schema로 직접 투영하며 App에 최종 raw envelope 초기화를 요구하지 않는다. packed C layout/실제 Firmware 구현이 아니다.

[DOMAIN Data](10_DATA.md) · [ESP32 Data](../ESP32/10_DATA.md) · [UART-M](../../03_COMMUNICATION/20_UART_M.md) · [명시 enum 변환](../../04_COMMON/30_ENUM_WIRE_MAPPING.md)

<a id="codec-types"></a>
## 필요한 모델부터 찾기

| 사용 경로 | 전체 선언·멤버·원 owner |
|---|---|
| 공통 수신 문맥 | [AppContext](#type-netappcontext), [DownContext](#type-netdowncontext) |
| MOBILE → DOMAIN | [Context](#type-netmobilecontext), [Request·union](#type-netmobilerequest), [RequestArgument](#type-netmobilerequestargument), [Query](#type-netmobilequery), [WarningAck](#type-netmobilewarningack) |
| DOMAIN → MOBILE 의미 출력 | [Result](#type-netmobileresult), [Warning](#type-netmobilewarning), [Availability](#type-netmobileavailability), [AvailabilityItem](#type-netavailabilityitem), [DigitalState](#type-netmobiledigitalstatus), [DigitalResult](#type-netmobiledigitalresult), [UserSettings](#type-netmobileusersettings) |
| BCM 원 CAN 투영 | [Door](#type-netmobilebcmdoorstate), [Climate](#type-netmobilebcmclimatestate), [InteriorLight](#type-netmobilebcmlightstate), [Status](#type-netmobilebcmstatus) |
| CIS·VSS 원 CAN 투영 | [Environment](#type-netmobilecisenvironment), [Occupant](#type-netmobilecisoccupant), [Rear](#type-netmobilecisrear), [CIS Status](#type-netmobilecisstatus), [VSS Status](#type-netmobilevssstatus) |
| WINDOW · 구현 보류/설계 유지 | [State](#type-netmobilewindowstate), [Fault](#type-netmobilewindowfault) |
| Network private 감독 | [DomainAlive](#type-netmobiledomainalive), [QueryEnd](#type-netmobilequeryend) |
| enum·선택자 | [RequestKind](#type-netrequestkind), [DoorOperation](#type-netdooroperation), [ClimateOperation](#type-netclimateoperation), [LightOperation](#type-netlightoperation), [DigitalOperation](#type-netdigitalkeyoperation), [QueryScope](#type-netqueryscope), [QueryStatus](#type-netquerystatus), [CommandTarget](#type-netcommandtarget) |

각 `NetMobile*`는 C2/Gateway 객체를 직접 encode/decode할 내부 표현이다. App에 추가 DTO 초기화나 새 공개 API 호출을 요구하지 않는다. CAN 원 출처 투영은 [accepted Snapshot association](25_PRIVATE_ROUTING.md#r2-origin-join), Settings/Digital 의미 손실은 [제한 export 규칙](#r2-partial-export)을 적용하며 모두 PROPOSED/BINDING_OPEN이다.

<a id="type-netmobilerequestargument"></a>
### `NetMobileRequestArgument`

kind/operation으로 선택하는 요청 인수. 선택된 멤버 하나만 의미를 가진다.

**선언 종류:** 공용체

```c
typedef union NetMobileRequestArgument
{
    int16_t target_temperature_centi_c; /**< CLIMATE / TARGET_TEMPERATURE. 허용 범위는 Domain 확인 항목. */
    NetFlag climate_auto;              /**< CLIMATE / AUTO. */
    NetFanLevel fan_level;             /**< CLIMATE / FAN. UNKNOWN 금지. */
    NetFlag light_enabled;             /**< INTERIOR_LIGHT / ENABLE. */
    uint8_t light_level_percent;       /**< INTERIOR_LIGHT / LEVEL. 0~100. */
    NetRgb normal_rgb;                 /**< INTERIOR_LIGHT / RGB. 각 0~255. */
    NetFlag digital_key_enabled;       /**< DIGITAL_KEY / ENABLE. */
} NetMobileRequestArgument;
```

#### 필드 구성
| 필드 | 타입 | 소유자 | 의미·유효 조건 |
| --- | --- | --- | --- |
| `target_temperature_centi_c` | `int16_t` | MOBILE 원 사용자 의도 · Gateway 검증 | CLIMATE / TARGET_TEMPERATURE. 허용 범위는 Domain 확인 항목. |
| `climate_auto` | [`NetFlag`](../../04_COMMON/10_DATA_AND_ID_RULES.md#type-netflag) | MOBILE 원 사용자 의도 · Gateway 검증 | CLIMATE / AUTO. |
| `fan_level` | [`NetFanLevel`](../../04_COMMON/10_DATA_AND_ID_RULES.md#type-netfanlevel) | MOBILE 원 사용자 의도 · Gateway 검증 | CLIMATE / FAN. UNKNOWN 금지. |
| `light_enabled` | [`NetFlag`](../../04_COMMON/10_DATA_AND_ID_RULES.md#type-netflag) | MOBILE 원 사용자 의도 · Gateway 검증 | INTERIOR_LIGHT / ENABLE. |
| `light_level_percent` | `uint8_t` | MOBILE 원 사용자 의도 · Gateway 검증 | INTERIOR_LIGHT / LEVEL. 0~100. |
| `normal_rgb` | [`NetRgb`](../../04_COMMON/10_DATA_AND_ID_RULES.md#type-netrgb) | MOBILE 원 사용자 의도 · Gateway 검증 | INTERIOR_LIGHT / RGB. 각 0~255. |
| `digital_key_enabled` | [`NetFlag`](../../04_COMMON/10_DATA_AND_ID_RULES.md#type-netflag) | MOBILE 원 사용자 의도 · Gateway 검증 | DIGITAL_KEY / ENABLE. |


#### 필요한 중첩 필드만 펼쳐보기
직접 멤버는 앞의 표를 사용한다. 아래 표는 nested context/source 안의 경로만 표시하며 별도 DTO나 raw 필드를 추가하지 않는다.
| 필드 | 타입 | 소유자 | 의미·유효 조건 |
| --- | --- | --- | --- |
| `normal_rgb.r` | `uint8_t` | MOBILE 원 사용자 의도 · Gateway 검증 | 빨강. 0~255. |
| `normal_rgb.g` | `uint8_t` | MOBILE 원 사용자 의도 · Gateway 검증 | 초록. 0~255. |
| `normal_rgb.b` | `uint8_t` | MOBILE 원 사용자 의도 · Gateway 검증 | 파랑. 0~255. |


<a id="type-netappcontext"></a>
### `NetAppContext`

**선언 종류:** 구조체

```c
typedef struct NetAppContext
{
    uint32_t device_context_id; /**< ESP32 등록 단말 식별. 0 예약. */
    uint64_t session_id;        /**< 현재 Session. 생성 방식은 ESP32/MOBILE 담당. */
} NetAppContext;
```

#### 필드 구성
| 필드 | 타입 | 소유자 | 의미·유효 조건 |
| --- | --- | --- | --- |
| `device_context_id` | `uint32_t` | MOBILE 원 의도 / Gateway 검증된 peer·Session 문맥 | ESP32 등록 단말 식별. 0 예약. |
| `session_id` | `uint64_t` | MOBILE 원 의도 / Gateway 검증된 peer·Session 문맥 | 현재 Session. 생성 방식은 ESP32/MOBILE 담당. |


<a id="type-netdowncontext"></a>
### `NetDownContext`

**선언 종류:** 구조체

```c
typedef struct NetDownContext
{
    NetAppContext recipient;   /**< 현재 수신 단말·Session. */
    uint32_t domain_boot_id;    /**< 정보를 제공하는 현재 Domain 기동 번호. */
    uint32_t query_id;          /**< 자발 전송은 0. 조회 응답은 원 QUERY_ID. */
} NetDownContext;
```

#### 필드 구성
| 필드 | 타입 | 소유자 | 의미·유효 조건 |
| --- | --- | --- | --- |
| `recipient` | [`NetAppContext`](14_MOBILE_CODEC_DATA.md#type-netappcontext) | 검증된 현재 수신 context owner · Network compose | 현재 수신 단말·Session. |
| `domain_boot_id` | `uint32_t` | Domain Network | 정보를 제공하는 현재 Domain 기동 번호. |
| `query_id` | `uint32_t` | 원 요청/조회 생산자(보존) | 자발 전송은 0. 조회 응답은 원 QUERY_ID. |


#### 필요한 중첩 필드만 펼쳐보기
직접 멤버는 앞의 표를 사용한다. 아래 표는 nested context/source 안의 경로만 표시하며 별도 DTO나 raw 필드를 추가하지 않는다.
| 필드 | 타입 | 소유자 | 의미·유효 조건 |
| --- | --- | --- | --- |
| `recipient.device_context_id` | `uint32_t` | 원 단말/Session 문맥 · Network 검증 | ESP32 등록 단말 식별. 0 예약. |
| `recipient.session_id` | `uint64_t` | 원 단말/Session 문맥 · Network 검증 | 현재 Session. 생성 방식은 ESP32/MOBILE 담당. |


<a id="type-netavailabilityitem"></a>
### `NetAvailabilityItem`

**선언 종류:** 구조체

```c
typedef struct NetAvailabilityItem
{
    NetFunctionId function_id;
    NetAvailability availability;
    NetReason reason;
} NetAvailabilityItem;
```

#### 필드 구성
| 필드 | 타입 | 소유자 | 의미·유효 조건 |
| --- | --- | --- | --- |
| `function_id` | [`NetFunctionId`](../../04_COMMON/10_DATA_AND_ID_RULES.md#type-netfunctionid) | Domain 기능별 가용성 | typed 의미 필드. 아래 범위와 UART-M의 같은 이름 필드 계약을 함께 적용한다. |
| `availability` | [`NetAvailability`](../../04_COMMON/10_DATA_AND_ID_RULES.md#type-netavailability) | Domain 기능별 가용성 | typed 의미 필드. 아래 범위와 UART-M의 같은 이름 필드 계약을 함께 적용한다. |
| `reason` | [`NetReason`](../../04_COMMON/10_DATA_AND_ID_RULES.md#type-netreason) | Domain 기능별 가용성 | typed 의미 필드. 아래 범위와 UART-M의 같은 이름 필드 계약을 함께 적용한다. |


<a id="type-netmobilecontext"></a>
### `NetMobileContext`

M_CONTEXT v2. 38B 제안 payload: 기존 앱 활성·근접 값에 등록·연결 공동 평가 품질/순서/나이 7B를 추가한다. v0.2의 31B와 바이너리 호환이 아니다.

**선언 종류:** 구조체

```c
typedef struct NetMobileContext
{
    NetAppContext context; /**< 요청 단말·Session 문맥. */
    NetTriState device_registration_state; /**< DEVICE_REGISTRATION_STATE: 0=NOT_REGISTERED; 1=REGISTERED; 255=UNKNOWN */
    NetTriState bt_connection_state; /**< BT_CONNECTION_STATE: 0=DISCONNECTED; 1=CONNECTED; 255=UNKNOWN */
    NetTriState app_active_state; /**< APP_ACTIVE_STATE: 0=INACTIVE; 1=ACTIVE; 255=UNKNOWN */
    NetValueQuality app_quality; /**< APP_QUALITY: VALUE_QUALITY */
    uint32_t app_update_sequence; /**< APP_UPDATE_SEQUENCE: App의 새 활성 정보 번호 */
    uint16_t app_age_ms; /**< APP_AGE: App 활성 정보 후 ms */
    NetTriState proximity_state; /**< PROXIMITY_STATE: 0=FAR; 1=NEAR; 255=UNKNOWN */
    NetValidity proximity_validity; /**< PROXIMITY_VALIDITY: 0=INVALID; 1=VALID */
    NetQualityReason proximity_reason; /**< PROXIMITY_REASON: 품질 사유 */
    uint32_t proximity_update_sequence; /**< PROXIMITY_UPDATE_SEQUENCE: ESP32 새 RSSI 평가 번호 */
    uint16_t proximity_age_ms; /**< PROXIMITY_AGE: RSSI 평가 후 ms */
    NetValueQuality registration_connection_quality; /**< REGISTRATION_CONNECTION_QUALITY: 현재 등록·연결 공동 평가의 품질 */
    uint32_t registration_connection_update_sequence; /**< REGISTRATION_CONNECTION_UPDATE_SEQUENCE: 새 공동 평가 번호(재전달 증가 금지) */
    uint16_t registration_connection_age_ms; /**< REGISTRATION_CONNECTION_AGE: 공동 평가 원본 이후 누적 경과 시간(ms) */
} NetMobileContext;
```

#### 필드 구성
| 필드 | 타입 | 소유자 | 의미·유효 조건 |
| --- | --- | --- | --- |
| `context` | [`NetAppContext`](14_MOBILE_CODEC_DATA.md#type-netappcontext) | ESP32·MOBILE 기능 | 요청 단말·Session 문맥. |
| `device_registration_state` | [`NetTriState`](../../04_COMMON/10_DATA_AND_ID_RULES.md#type-nettristate) | ESP32·MOBILE 기능 | DEVICE_REGISTRATION_STATE: 0=NOT_REGISTERED; 1=REGISTERED; 255=UNKNOWN |
| `bt_connection_state` | [`NetTriState`](../../04_COMMON/10_DATA_AND_ID_RULES.md#type-nettristate) | ESP32 등록/연결/근접 해당 원 owner | BT_CONNECTION_STATE: 0=DISCONNECTED; 1=CONNECTED; 255=UNKNOWN |
| `app_active_state` | [`NetTriState`](../../04_COMMON/10_DATA_AND_ID_RULES.md#type-nettristate) | MOBILE 원 활성 평가 · ESP32 검증 | APP_ACTIVE_STATE: 0=INACTIVE; 1=ACTIVE; 255=UNKNOWN |
| `app_quality` | [`NetValueQuality`](../../04_COMMON/10_DATA_AND_ID_RULES.md#type-netvaluequality) | ESP32·MOBILE 기능 | APP_QUALITY: VALUE_QUALITY |
| `app_update_sequence` | `uint32_t` | MOBILE 원 활성 평가 · ESP32 검증 | APP_UPDATE_SEQUENCE: App의 새 활성 정보 번호 |
| `app_age_ms` | `uint16_t` | ESP32·MOBILE 기능 | APP_AGE: App 활성 정보 후 ms |
| `proximity_state` | [`NetTriState`](../../04_COMMON/10_DATA_AND_ID_RULES.md#type-nettristate) | ESP32 등록/연결/근접 해당 원 owner | PROXIMITY_STATE: 0=FAR; 1=NEAR; 255=UNKNOWN |
| `proximity_validity` | [`NetValidity`](../../04_COMMON/10_DATA_AND_ID_RULES.md#type-netvalidity) | ESP32 등록/연결/근접 해당 원 owner | PROXIMITY_VALIDITY: 0=INVALID; 1=VALID |
| `proximity_reason` | [`NetQualityReason`](../../04_COMMON/10_DATA_AND_ID_RULES.md#type-netqualityreason) | ESP32 등록/연결/근접 해당 원 owner | PROXIMITY_REASON: 품질 사유 |
| `proximity_update_sequence` | `uint32_t` | ESP32 등록/연결/근접 해당 원 owner | PROXIMITY_UPDATE_SEQUENCE: ESP32 새 RSSI 평가 번호 |
| `proximity_age_ms` | `uint16_t` | ESP32 등록/연결/근접 해당 원 owner | PROXIMITY_AGE: RSSI 평가 후 ms |
| `registration_connection_quality` | [`NetValueQuality`](../../04_COMMON/10_DATA_AND_ID_RULES.md#type-netvaluequality) | ESP32 등록·현재 연결 owner | REGISTRATION_CONNECTION_QUALITY: 현재 등록·연결 공동 평가의 품질 |
| `registration_connection_update_sequence` | `uint32_t` | ESP32 등록·현재 연결 owner | REGISTRATION_CONNECTION_UPDATE_SEQUENCE: 새 공동 평가 번호(재전달 증가 금지) |
| `registration_connection_age_ms` | `uint16_t` | ESP32 등록·현재 연결 owner | REGISTRATION_CONNECTION_AGE: 공동 평가 원본 이후 누적 경과 시간(ms) |


#### 필요한 중첩 필드만 펼쳐보기
직접 멤버는 앞의 표를 사용한다. 아래 표는 nested context/source 안의 경로만 표시하며 별도 DTO나 raw 필드를 추가하지 않는다.
| 필드 | 타입 | 소유자 | 의미·유효 조건 |
| --- | --- | --- | --- |
| `context.device_context_id` | `uint32_t` | ESP32·MOBILE 기능 | ESP32 등록 단말 식별. 0 예약. |
| `context.session_id` | `uint64_t` | ESP32·MOBILE 기능 | 현재 Session. 생성 방식은 ESP32/MOBILE 담당. |


<a id="type-netmobilerequest"></a>
### `NetMobileRequest`

M_REQUEST. 원본 필드 의미를 보존한 통신 자료형.

**선언 종류:** 구조체

```c
typedef struct NetMobileRequest
{
    NetAppContext context; /**< 요청 단말·Session 문맥. */
    uint32_t request_id; /**< REQUEST_ID: App이 새 사용자 입력마다 생성 */
    NetRequestKind kind; /**< REQUEST_KIND: 1=도어; 2=공조; 3=실내 조명; 4=디지털 키 */
    uint8_t operation; /**< OPERATION: 요청 종류별 동작 코드 */
    NetMobileRequestArgument argument; /**< ARG_0~5의 기능별 의미. Codec이 선택 멤버를 인코딩하고 나머지를 0으로 채운다. */
} NetMobileRequest;
```

#### 필드 구성
| 필드 | 타입 | 소유자 | 의미·유효 조건 |
| --- | --- | --- | --- |
| `context` | [`NetAppContext`](14_MOBILE_CODEC_DATA.md#type-netappcontext) | Gateway 검증된 현재 peer/Session 문맥 | 요청 단말·Session 문맥. |
| `request_id` | `uint32_t` | 원 요청/조회 생산자(보존) | REQUEST_ID: App이 새 사용자 입력마다 생성 |
| `kind` | [`NetRequestKind`](14_MOBILE_CODEC_DATA.md#type-netrequestkind) | MOBILE 원 요청·사건 | REQUEST_KIND: 1=도어; 2=공조; 3=실내 조명; 4=디지털 키 |
| `operation` | `uint8_t` | MOBILE 원 요청·사건 | OPERATION: 요청 종류별 동작 코드 |
| `argument` | [`NetMobileRequestArgument`](14_MOBILE_CODEC_DATA.md#type-netmobilerequestargument) | MOBILE 원 요청·사건 | ARG_0~5의 기능별 의미. Codec이 선택 멤버를 인코딩하고 나머지를 0으로 채운다. |


#### 필요한 중첩 필드만 펼쳐보기
직접 멤버는 앞의 표를 사용한다. 아래 표는 nested context/source 안의 경로만 표시하며 별도 DTO나 raw 필드를 추가하지 않는다.
| 필드 | 타입 | 소유자 | 의미·유효 조건 |
| --- | --- | --- | --- |
| `context.device_context_id` | `uint32_t` | Gateway 검증된 현재 peer/Session 문맥 | ESP32 등록 단말 식별. 0 예약. |
| `context.session_id` | `uint64_t` | Gateway 검증된 현재 peer/Session 문맥 | 현재 Session. 생성 방식은 ESP32/MOBILE 담당. |
| `argument.target_temperature_centi_c` | `int16_t` | MOBILE 원 요청·사건 | CLIMATE / TARGET_TEMPERATURE. 허용 범위는 Domain 확인 항목. |
| `argument.climate_auto` | [`NetFlag`](../../04_COMMON/10_DATA_AND_ID_RULES.md#type-netflag) | MOBILE 원 요청·사건 | CLIMATE / AUTO. |
| `argument.fan_level` | [`NetFanLevel`](../../04_COMMON/10_DATA_AND_ID_RULES.md#type-netfanlevel) | MOBILE 원 요청·사건 | CLIMATE / FAN. UNKNOWN 금지. |
| `argument.light_enabled` | [`NetFlag`](../../04_COMMON/10_DATA_AND_ID_RULES.md#type-netflag) | MOBILE 원 요청·사건 | INTERIOR_LIGHT / ENABLE. |
| `argument.light_level_percent` | `uint8_t` | MOBILE 원 요청·사건 | INTERIOR_LIGHT / LEVEL. 0~100. |
| `argument.normal_rgb.r` | `uint8_t` | MOBILE 원 요청·사건 | 빨강. 0~255. |
| `argument.normal_rgb.g` | `uint8_t` | MOBILE 원 요청·사건 | 초록. 0~255. |
| `argument.normal_rgb.b` | `uint8_t` | MOBILE 원 요청·사건 | 파랑. 0~255. |
| `argument.digital_key_enabled` | [`NetFlag`](../../04_COMMON/10_DATA_AND_ID_RULES.md#type-netflag) | MOBILE 원 요청·사건 | DIGITAL_KEY / ENABLE. |


<a id="type-netmobilequery"></a>
### `NetMobileQuery`

M_QUERY. 원본 필드 의미를 보존한 통신 자료형.

**선언 종류:** 구조체

```c
typedef struct NetMobileQuery
{
    NetAppContext context; /**< 요청 단말·Session 문맥. */
    uint32_t query_id; /**< QUERY_ID: 이번 조회 식별, 0 금지 */
    NetQueryScope query_scope; /**< QUERY_SCOPE: 0=현재 상태; 1=요청 결과; 2=경고; 3=전체 */
    uint64_t request_session; /**< REQUEST_SESSION: 조회할 원 요청 Session; 상태 조회면 0 */
    uint32_t request_id; /**< REQUEST_ID: 조회할 요청; 상태 조회면 0 */
    uint16_t cursor; /**< CURSOR: 이 초안에서는 0 */
} NetMobileQuery;
```

#### 필드 구성
| 필드 | 타입 | 소유자 | 의미·유효 조건 |
| --- | --- | --- | --- |
| `context` | [`NetAppContext`](14_MOBILE_CODEC_DATA.md#type-netappcontext) | Gateway 검증된 현재 peer/Session 문맥 | 요청 단말·Session 문맥. |
| `query_id` | `uint32_t` | 원 요청/조회 생산자(보존) | QUERY_ID: 이번 조회 식별, 0 금지 |
| `query_scope` | [`NetQueryScope`](14_MOBILE_CODEC_DATA.md#type-netqueryscope) | MOBILE 원 요청·사건 | QUERY_SCOPE: 0=현재 상태; 1=요청 결과; 2=경고; 3=전체 |
| `request_session` | `uint64_t` | MOBILE 원 요청·사건 | REQUEST_SESSION: 조회할 원 요청 Session; 상태 조회면 0 |
| `request_id` | `uint32_t` | 원 요청/조회 생산자(보존) | REQUEST_ID: 조회할 요청; 상태 조회면 0 |
| `cursor` | `uint16_t` | MOBILE 원 요청·사건 | CURSOR: 이 초안에서는 0 |


#### 필요한 중첩 필드만 펼쳐보기
직접 멤버는 앞의 표를 사용한다. 아래 표는 nested context/source 안의 경로만 표시하며 별도 DTO나 raw 필드를 추가하지 않는다.
| 필드 | 타입 | 소유자 | 의미·유효 조건 |
| --- | --- | --- | --- |
| `context.device_context_id` | `uint32_t` | Gateway 검증된 현재 peer/Session 문맥 | ESP32 등록 단말 식별. 0 예약. |
| `context.session_id` | `uint64_t` | Gateway 검증된 현재 peer/Session 문맥 | 현재 Session. 생성 방식은 ESP32/MOBILE 담당. |


<a id="type-netmobilewarningack"></a>
### `NetMobileWarningAck`

M_WARNING_ACK. 원본 필드 의미를 보존한 통신 자료형.

**선언 종류:** 구조체

```c
typedef struct NetMobileWarningAck
{
    NetAppContext context; /**< 요청 단말·Session 문맥. */
    NetWarningType warning_type; /**< WARNING_TYPE: 경고 종류 */
    uint32_t occurrence_id; /**< OCCURRENCE_ID: 읽은 경고 발생 번호 */
    uint32_t warning_domain_boot; /**< WARNING_DOMAIN_BOOT: 읽은 경고의 Domain 기동 번호 */
} NetMobileWarningAck;
```

#### 필드 구성
| 필드 | 타입 | 소유자 | 의미·유효 조건 |
| --- | --- | --- | --- |
| `context` | [`NetAppContext`](14_MOBILE_CODEC_DATA.md#type-netappcontext) | Gateway 검증된 현재 peer/Session 문맥 | 요청 단말·Session 문맥. |
| `warning_type` | [`NetWarningType`](../../04_COMMON/10_DATA_AND_ID_RULES.md#type-netwarningtype) | MOBILE 원 요청·사건 | WARNING_TYPE: 경고 종류 |
| `occurrence_id` | `uint32_t` | MOBILE 원 요청·사건 | OCCURRENCE_ID: 읽은 경고 발생 번호 |
| `warning_domain_boot` | `uint32_t` | MOBILE 원 요청·사건 | WARNING_DOMAIN_BOOT: 읽은 경고의 Domain 기동 번호 |


#### 필요한 중첩 필드만 펼쳐보기
직접 멤버는 앞의 표를 사용한다. 아래 표는 nested context/source 안의 경로만 표시하며 별도 DTO나 raw 필드를 추가하지 않는다.
| 필드 | 타입 | 소유자 | 의미·유효 조건 |
| --- | --- | --- | --- |
| `context.device_context_id` | `uint32_t` | Gateway 검증된 현재 peer/Session 문맥 | ESP32 등록 단말 식별. 0 예약. |
| `context.session_id` | `uint64_t` | Gateway 검증된 현재 peer/Session 문맥 | 현재 Session. 생성 방식은 ESP32/MOBILE 담당. |


<a id="type-netmobiledomainalive"></a>
### `NetMobileDomainAlive`

M_DOMAIN_ALIVE. 원본 필드 의미를 보존한 통신 자료형.

**선언 종류:** 구조체

```c
typedef struct NetMobileDomainAlive
{
    uint32_t domain_boot_id; /**< DOMAIN_BOOT_ID: Domain 현재 기동 번호 */
    uint32_t update_sequence; /**< UPDATE_SEQUENCE: Domain 생존 평가 번호 */
    uint16_t source_age_ms; /**< SOURCE_AGE: 평가 후 ms */
    NetControllerState domain_state; /**< DOMAIN_STATE: 0=INIT; 1=READY; 2=DEGRADED; 3=FAULT */
    NetValueQuality value_quality; /**< VALUE_QUALITY: VALUE_QUALITY */
} NetMobileDomainAlive;
```

#### 필드 구성
| 필드 | 타입 | 소유자 | 의미·유효 조건 |
| --- | --- | --- | --- |
| `domain_boot_id` | `uint32_t` | Domain Network | DOMAIN_BOOT_ID: Domain 현재 기동 번호 |
| `update_sequence` | `uint32_t` | Domain 기능 데이터 | UPDATE_SEQUENCE: Domain 생존 평가 번호 |
| `source_age_ms` | `uint16_t` | Domain 기능 데이터 | SOURCE_AGE: 평가 후 ms |
| `domain_state` | [`NetControllerState`](../../04_COMMON/10_DATA_AND_ID_RULES.md#type-netcontrollerstate) | Domain 기능 데이터 | DOMAIN_STATE: 0=INIT; 1=READY; 2=DEGRADED; 3=FAULT |
| `value_quality` | [`NetValueQuality`](../../04_COMMON/10_DATA_AND_ID_RULES.md#type-netvaluequality) | Domain 기능 데이터 | VALUE_QUALITY: VALUE_QUALITY |


<a id="type-netmobileresult"></a>
### `NetMobileResult`

M_RESULT. 원본 필드 의미를 보존한 통신 자료형.

**선언 종류:** 구조체

```c
typedef struct NetMobileResult
{
    NetDownContext context; /**< 현재 수신 문맥·Domain 기동·조회 식별. */
    uint64_t request_session; /**< REQUEST_SESSION: 원 요청 Session */
    uint32_t request_id; /**< REQUEST_ID: 원 요청 ID */
    NetVehicleResult result; /**< RESULT: 공통 차량 Result; 확인 기록 없으면 255=필드 없음 */
    NetReason result_reason; /**< RESULT_REASON: 공통 사유 코드 */
    NetResultConfirmation result_confirmation; /**< RESULT_CONFIRMATION: 0=UNCONFIRMED; 1=CONFIRMED */
    NetCommandTarget command_target; /**< COMMAND_TARGET: 0=Domain 설정; 1=BCM; 2=WINDOW */
    uint32_t command_id; /**< COMMAND_ID: 실행 명령 연계; 설정만 반영한 경우 0 */
    NetOrigin origin; /**< ORIGIN: 요청 출처 코드 */
    uint16_t result_age_ms; /**< RESULT_AGE: 결과 확인 후 ms */
} NetMobileResult;
```

#### 필드 구성
| 필드 | 타입 | 소유자 | 의미·유효 조건 |
| --- | --- | --- | --- |
| `context` | [`NetDownContext`](14_MOBILE_CODEC_DATA.md#type-netdowncontext) | 현재 수신 대상 문맥 owner · Network 조립 | 현재 수신 문맥·Domain 기동·조회 식별. |
| `request_session` | `uint64_t` | Domain 기능 데이터 | REQUEST_SESSION: 원 요청 Session |
| `request_id` | `uint32_t` | 원 요청/조회 생산자(보존) | REQUEST_ID: 원 요청 ID |
| `result` | [`NetVehicleResult`](../../04_COMMON/10_DATA_AND_ID_RULES.md#type-netvehicleresult) | Domain 기능 데이터 | RESULT: 공통 차량 Result; 확인 기록 없으면 255=필드 없음 |
| `result_reason` | [`NetReason`](../../04_COMMON/10_DATA_AND_ID_RULES.md#type-netreason) | Domain 기능 데이터 | RESULT_REASON: 공통 사유 코드 |
| `result_confirmation` | [`NetResultConfirmation`](../../04_COMMON/10_DATA_AND_ID_RULES.md#type-netresultconfirmation) | Domain 기능 데이터 | RESULT_CONFIRMATION: 0=UNCONFIRMED; 1=CONFIRMED |
| `command_target` | [`NetCommandTarget`](14_MOBILE_CODEC_DATA.md#type-netcommandtarget) | Domain 기능 데이터 | COMMAND_TARGET: 0=Domain 설정; 1=BCM; 2=WINDOW |
| `command_id` | `uint32_t` | Domain C2 원 명령 owner · Network 값 보존 | COMMAND_ID: 실행 명령 연계; 설정만 반영한 경우 0 |
| `origin` | [`NetOrigin`](../../04_COMMON/10_DATA_AND_ID_RULES.md#type-netorigin) | Domain 기능 데이터 | ORIGIN: 요청 출처 코드 |
| `result_age_ms` | `uint16_t` | Domain 기능 데이터 | RESULT_AGE: 결과 확인 후 ms |


#### 필요한 중첩 필드만 펼쳐보기
직접 멤버는 앞의 표를 사용한다. 아래 표는 nested context/source 안의 경로만 표시하며 별도 DTO나 raw 필드를 추가하지 않는다.
| 필드 | 타입 | 소유자 | 의미·유효 조건 |
| --- | --- | --- | --- |
| `context.recipient.device_context_id` | `uint32_t` | 현재 단말·Session owner · Network 검증 | ESP32 등록 단말 식별. 0 예약. |
| `context.recipient.session_id` | `uint64_t` | 현재 단말·Session owner · Network 검증 | 현재 Session. 생성 방식은 ESP32/MOBILE 담당. |
| `context.domain_boot_id` | `uint32_t` | Domain Network | 정보를 제공하는 현재 Domain 기동 번호. |
| `context.query_id` | `uint32_t` | 원 Query ID (보존) | 자발 전송은 0. 조회 응답은 원 QUERY_ID. |


<a id="type-netmobilewarning"></a>
### `NetMobileWarning`

M_WARNING. 원본 필드 의미를 보존한 통신 자료형.

**선언 종류:** 구조체

```c
typedef struct NetMobileWarning
{
    NetDownContext context; /**< 현재 수신 문맥·Domain 기동·조회 식별. */
    NetWarningType warning_type; /**< WARNING_TYPE: 경고 종류 */
    uint32_t occurrence_id; /**< OCCURRENCE_ID: Domain 기동 내 경고 발생 번호 */
    NetSeverity severity; /**< SEVERITY: 0=INFO; 1=CAUTION; 2=EMERGENCY */
    NetFlag warning_state; /**< WARNING_STATE: 0=CLEAR; 1=ACTIVE */
    NetFlag read_state; /**< READ_STATE: 0=UNREAD; 1=READ */
    NetValueQuality value_quality; /**< VALUE_QUALITY: VALUE_QUALITY */
    NetQualityReason quality_reason; /**< QUALITY_REASON: 품질 사유 */
    uint32_t source_boot; /**< SOURCE_BOOT: 원 관측 생성 노드 기동 번호 */
    uint32_t source_sequence; /**< SOURCE_SEQUENCE: 원 관측 번호 */
    uint16_t source_age_ms; /**< SOURCE_AGE: 원 관측 후 누적 ms */
} NetMobileWarning;
```

#### 필드 구성
| 필드 | 타입 | 소유자 | 의미·유효 조건 |
| --- | --- | --- | --- |
| `context` | [`NetDownContext`](14_MOBILE_CODEC_DATA.md#type-netdowncontext) | 현재 수신 대상 문맥 owner · Network 조립 | 현재 수신 문맥·Domain 기동·조회 식별. |
| `warning_type` | [`NetWarningType`](../../04_COMMON/10_DATA_AND_ID_RULES.md#type-netwarningtype) | Domain 기능 데이터 | WARNING_TYPE: 경고 종류 |
| `occurrence_id` | `uint32_t` | Domain 기능 데이터 | OCCURRENCE_ID: Domain 기동 내 경고 발생 번호 |
| `severity` | [`NetSeverity`](../../04_COMMON/10_DATA_AND_ID_RULES.md#type-netseverity) | Domain 기능 데이터 | SEVERITY: 0=INFO; 1=CAUTION; 2=EMERGENCY |
| `warning_state` | [`NetFlag`](../../04_COMMON/10_DATA_AND_ID_RULES.md#type-netflag) | Domain 기능 데이터 | WARNING_STATE: 0=CLEAR; 1=ACTIVE |
| `read_state` | [`NetFlag`](../../04_COMMON/10_DATA_AND_ID_RULES.md#type-netflag) | Domain 기능 데이터 | READ_STATE: 0=UNREAD; 1=READ |
| `value_quality` | [`NetValueQuality`](../../04_COMMON/10_DATA_AND_ID_RULES.md#type-netvaluequality) | Domain 기능 데이터 | VALUE_QUALITY: VALUE_QUALITY |
| `quality_reason` | [`NetQualityReason`](../../04_COMMON/10_DATA_AND_ID_RULES.md#type-netqualityreason) | Domain 기능 데이터 | QUALITY_REASON: 품질 사유 |
| `source_boot` | `uint32_t` | Domain 기능 데이터 | SOURCE_BOOT: 원 관측 생성 노드 기동 번호 |
| `source_sequence` | `uint32_t` | Domain 기능 데이터 | SOURCE_SEQUENCE: 원 관측 번호 |
| `source_age_ms` | `uint16_t` | Domain 기능 데이터 | SOURCE_AGE: 원 관측 후 누적 ms |


#### 필요한 중첩 필드만 펼쳐보기
직접 멤버는 앞의 표를 사용한다. 아래 표는 nested context/source 안의 경로만 표시하며 별도 DTO나 raw 필드를 추가하지 않는다.
| 필드 | 타입 | 소유자 | 의미·유효 조건 |
| --- | --- | --- | --- |
| `context.recipient.device_context_id` | `uint32_t` | 현재 단말·Session owner · Network 검증 | ESP32 등록 단말 식별. 0 예약. |
| `context.recipient.session_id` | `uint64_t` | 현재 단말·Session owner · Network 검증 | 현재 Session. 생성 방식은 ESP32/MOBILE 담당. |
| `context.domain_boot_id` | `uint32_t` | Domain Network | 정보를 제공하는 현재 Domain 기동 번호. |
| `context.query_id` | `uint32_t` | 원 Query ID (보존) | 자발 전송은 0. 조회 응답은 원 QUERY_ID. |


<a id="type-netmobileavailability"></a>
### `NetMobileAvailability`

M_AVAILABILITY. 원본 필드 의미를 보존한 통신 자료형.

**선언 종류:** 구조체

```c
typedef struct NetMobileAvailability
{
    NetDownContext context; /**< 현재 수신 문맥·Domain 기동·조회 식별. */
    NetAvailabilityItem items[NET_AVAILABILITY_ITEM_COUNT]; /**< 원본 기능 ID 1~9의 순서. WINDOW 항목의 설계를 유지한다. */
    uint32_t update_sequence; /**< 새 가용성 평가 번호. */
    uint16_t source_age_ms; /**< 평가 후 제출 시점까지의 경과 시간. */
} NetMobileAvailability;
```

#### 필드 구성
| 필드 | 타입 | 소유자 | 의미·유효 조건 |
| --- | --- | --- | --- |
| `context` | [`NetDownContext`](14_MOBILE_CODEC_DATA.md#type-netdowncontext) | 현재 수신 대상 문맥 owner · Network 조립 | 현재 수신 문맥·Domain 기동·조회 식별. |
| `items[NET_AVAILABILITY_ITEM_COUNT]` | [`NetAvailabilityItem`](14_MOBILE_CODEC_DATA.md#type-netavailabilityitem) | Domain 기능 데이터 | 원본 기능 ID 1~9의 순서. WINDOW 항목의 설계를 유지한다. |
| `update_sequence` | `uint32_t` | Domain 기능 데이터 | 새 가용성 평가 번호. |
| `source_age_ms` | `uint16_t` | Domain 기능 데이터 | 평가 후 제출 시점까지의 경과 시간. |


#### 필요한 중첩 필드만 펼쳐보기
직접 멤버는 앞의 표를 사용한다. 아래 표는 nested context/source 안의 경로만 표시하며 별도 DTO나 raw 필드를 추가하지 않는다.
| 필드 | 타입 | 소유자 | 의미·유효 조건 |
| --- | --- | --- | --- |
| `context.recipient.device_context_id` | `uint32_t` | 현재 단말·Session owner · Network 검증 | ESP32 등록 단말 식별. 0 예약. |
| `context.recipient.session_id` | `uint64_t` | 현재 단말·Session owner · Network 검증 | 현재 Session. 생성 방식은 ESP32/MOBILE 담당. |
| `context.domain_boot_id` | `uint32_t` | Domain Network | 정보를 제공하는 현재 Domain 기동 번호. |
| `context.query_id` | `uint32_t` | 원 Query ID (보존) | 자발 전송은 0. 조회 응답은 원 QUERY_ID. |
| `items.function_id` | [`NetFunctionId`](../../04_COMMON/10_DATA_AND_ID_RULES.md#type-netfunctionid) | Domain 기능 데이터 | 원본 필드 규칙 참조 |
| `items.availability` | [`NetAvailability`](../../04_COMMON/10_DATA_AND_ID_RULES.md#type-netavailability) | Domain 기능 데이터 | 원본 필드 규칙 참조 |
| `items.reason` | [`NetReason`](../../04_COMMON/10_DATA_AND_ID_RULES.md#type-netreason) | Domain 기능 데이터 | 원본 필드 규칙 참조 |


<a id="type-netmobiledigitalstatus"></a>
### `NetMobileDigitalStatus`

M_DIGITAL_STATUS. 원본 필드 의미를 보존한 통신 자료형.

**선언 종류:** 구조체

```c
typedef struct NetMobileDigitalStatus
{
    NetDownContext context; /**< 현재 수신 문맥·Domain 기동·조회 식별. */
    NetFlag digital_key_setting_state; /**< DIGITAL_KEY_SETTING_STATE: 0=OFF; 1=ON */
    NetAvailability digital_key_availability; /**< DIGITAL_KEY_AVAILABILITY: 0=AVAILABLE; 1=LIMITED; 2=UNAVAILABLE */
    NetReason reason; /**< REASON: 공통 사유 코드 */
    uint32_t update_sequence; /**< UPDATE_SEQUENCE: 새 판단/설정 평가 */
    uint16_t source_age_ms; /**< SOURCE_AGE: 평가 후 ms */
} NetMobileDigitalStatus;
```

#### 필드 구성
| 필드 | 타입 | 소유자 | 의미·유효 조건 |
| --- | --- | --- | --- |
| `context` | [`NetDownContext`](14_MOBILE_CODEC_DATA.md#type-netdowncontext) | 현재 수신 대상 문맥 owner · Network 조립 | 현재 수신 문맥·Domain 기동·조회 식별. |
| `digital_key_setting_state` | [`NetFlag`](../../04_COMMON/10_DATA_AND_ID_RULES.md#type-netflag) | Domain 기능 데이터 | DIGITAL_KEY_SETTING_STATE: 0=OFF; 1=ON |
| `digital_key_availability` | [`NetAvailability`](../../04_COMMON/10_DATA_AND_ID_RULES.md#type-netavailability) | Domain 기능 데이터 | DIGITAL_KEY_AVAILABILITY: 0=AVAILABLE; 1=LIMITED; 2=UNAVAILABLE |
| `reason` | [`NetReason`](../../04_COMMON/10_DATA_AND_ID_RULES.md#type-netreason) | Domain 기능 데이터 | REASON: 공통 사유 코드 |
| `update_sequence` | `uint32_t` | Domain 기능 데이터 | UPDATE_SEQUENCE: 새 판단/설정 평가 |
| `source_age_ms` | `uint16_t` | Domain 기능 데이터 | SOURCE_AGE: 평가 후 ms |


#### 필요한 중첩 필드만 펼쳐보기
직접 멤버는 앞의 표를 사용한다. 아래 표는 nested context/source 안의 경로만 표시하며 별도 DTO나 raw 필드를 추가하지 않는다.
| 필드 | 타입 | 소유자 | 의미·유효 조건 |
| --- | --- | --- | --- |
| `context.recipient.device_context_id` | `uint32_t` | 현재 단말·Session owner · Network 검증 | ESP32 등록 단말 식별. 0 예약. |
| `context.recipient.session_id` | `uint64_t` | 현재 단말·Session owner · Network 검증 | 현재 Session. 생성 방식은 ESP32/MOBILE 담당. |
| `context.domain_boot_id` | `uint32_t` | Domain Network | 정보를 제공하는 현재 Domain 기동 번호. |
| `context.query_id` | `uint32_t` | 원 Query ID (보존) | 자발 전송은 0. 조회 응답은 원 QUERY_ID. |


<a id="type-netmobiledigitalresult"></a>
### `NetMobileDigitalResult`

M_DIGITAL_RESULT. 원본 필드 의미를 보존한 통신 자료형.

**선언 종류:** 구조체

```c
typedef struct NetMobileDigitalResult
{
    NetDownContext context; /**< 현재 수신 문맥·Domain 기동·조회 식별. */
    uint32_t auto_occurrence_id; /**< AUTO_OCCURRENCE_ID: 자동 Unlock 실행 식별 */
    uint32_t related_door_command_id; /**< RELATED_DOOR_COMMAND_ID: BCM 도어 명령 연계 */
    NetOrigin origin; /**< ORIGIN: 2=PROXIMITY_AUTO */
    NetVehicleResult result; /**< RESULT: 공통 Result; 기록 없으면 255=필드 없음 */
    NetReason result_reason; /**< RESULT_REASON: 공통 사유 코드 */
    NetResultConfirmation result_confirmation; /**< RESULT_CONFIRMATION: 0=UNCONFIRMED; 1=CONFIRMED */
    uint16_t result_age_ms; /**< RESULT_AGE: 결과 확인 후 ms */
} NetMobileDigitalResult;
```

#### 필드 구성
| 필드 | 타입 | 소유자 | 의미·유효 조건 |
| --- | --- | --- | --- |
| `context` | [`NetDownContext`](14_MOBILE_CODEC_DATA.md#type-netdowncontext) | 현재 수신 대상 문맥 owner · Network 조립 | 현재 수신 문맥·Domain 기동·조회 식별. |
| `auto_occurrence_id` | `uint32_t` | Domain 기능 데이터 | AUTO_OCCURRENCE_ID: 자동 Unlock 실행 식별 |
| `related_door_command_id` | `uint32_t` | Domain C2 원 명령 owner · Network 값 보존 | RELATED_DOOR_COMMAND_ID: BCM 도어 명령 연계 |
| `origin` | [`NetOrigin`](../../04_COMMON/10_DATA_AND_ID_RULES.md#type-netorigin) | Domain 기능 데이터 | ORIGIN: 2=PROXIMITY_AUTO |
| `result` | [`NetVehicleResult`](../../04_COMMON/10_DATA_AND_ID_RULES.md#type-netvehicleresult) | Domain 기능 데이터 | RESULT: 공통 Result; 기록 없으면 255=필드 없음 |
| `result_reason` | [`NetReason`](../../04_COMMON/10_DATA_AND_ID_RULES.md#type-netreason) | Domain 기능 데이터 | RESULT_REASON: 공통 사유 코드 |
| `result_confirmation` | [`NetResultConfirmation`](../../04_COMMON/10_DATA_AND_ID_RULES.md#type-netresultconfirmation) | Domain 기능 데이터 | RESULT_CONFIRMATION: 0=UNCONFIRMED; 1=CONFIRMED |
| `result_age_ms` | `uint16_t` | Domain 기능 데이터 | RESULT_AGE: 결과 확인 후 ms |


#### 필요한 중첩 필드만 펼쳐보기
직접 멤버는 앞의 표를 사용한다. 아래 표는 nested context/source 안의 경로만 표시하며 별도 DTO나 raw 필드를 추가하지 않는다.
| 필드 | 타입 | 소유자 | 의미·유효 조건 |
| --- | --- | --- | --- |
| `context.recipient.device_context_id` | `uint32_t` | 현재 단말·Session owner · Network 검증 | ESP32 등록 단말 식별. 0 예약. |
| `context.recipient.session_id` | `uint64_t` | 현재 단말·Session owner · Network 검증 | 현재 Session. 생성 방식은 ESP32/MOBILE 담당. |
| `context.domain_boot_id` | `uint32_t` | Domain Network | 정보를 제공하는 현재 Domain 기동 번호. |
| `context.query_id` | `uint32_t` | 원 Query ID (보존) | 자발 전송은 0. 조회 응답은 원 QUERY_ID. |


<a id="type-netmobileusersettings"></a>
### `NetMobileUserSettings`

M_USER_SETTINGS. 원본 필드 의미를 보존한 통신 자료형.

**선언 종류:** 구조체

```c
typedef struct NetMobileUserSettings
{
    NetDownContext context; /**< 현재 수신 문맥·Domain 기동·조회 식별. */
    int16_t target_temperature_centi_c; /**< TARGET_TEMPERATURE: Domain이 반영한 목표 온도, 0.01°C/raw, -32768=미확인 */
    NetTriState climate_auto; /**< CLIMATE_AUTO: 0=OFF; 1=ON; 255=미확인 */
    NetFanLevel user_fan_level; /**< USER_FAN_LEVEL: 사용자가 설정한 FAN 코드 */
    NetTriState interior_light_enabled; /**< INTERIOR_LIGHT_ENABLED: 0=OFF; 1=ON; 255=미확인 */
    uint8_t normal_light_level_percent; /**< NORMAL_LIGHT_LEVEL: 사용자 NORMAL 밝기,0~100% */
    NetRgb normal_light_rgb; /**< NORMAL_LIGHT_RGB: 사용자 NORMAL 색상 R/G/B */
    NetValueQuality value_quality; /**< VALUE_QUALITY: 설정 확인 품질 */
    uint32_t update_sequence; /**< UPDATE_SEQUENCE: 현재 설정 확인 평가 번호 */
    uint16_t source_age_ms; /**< SOURCE_AGE: 설정 확인 평가 후 ms */
} NetMobileUserSettings;
```

#### 필드 구성
| 필드 | 타입 | 소유자 | 의미·유효 조건 |
| --- | --- | --- | --- |
| `context` | [`NetDownContext`](14_MOBILE_CODEC_DATA.md#type-netdowncontext) | 현재 수신 대상 문맥 owner · Network 조립 | 현재 수신 문맥·Domain 기동·조회 식별. |
| `target_temperature_centi_c` | `int16_t` | Domain 기능 데이터 | TARGET_TEMPERATURE: Domain이 반영한 목표 온도, 0.01°C/raw, -32768=미확인 |
| `climate_auto` | [`NetTriState`](../../04_COMMON/10_DATA_AND_ID_RULES.md#type-nettristate) | Domain 기능 데이터 | CLIMATE_AUTO: 0=OFF; 1=ON; 255=미확인 |
| `user_fan_level` | [`NetFanLevel`](../../04_COMMON/10_DATA_AND_ID_RULES.md#type-netfanlevel) | Domain 기능 데이터 | USER_FAN_LEVEL: 사용자가 설정한 FAN 코드 |
| `interior_light_enabled` | [`NetTriState`](../../04_COMMON/10_DATA_AND_ID_RULES.md#type-nettristate) | Domain 기능 데이터 | INTERIOR_LIGHT_ENABLED: 0=OFF; 1=ON; 255=미확인 |
| `normal_light_level_percent` | `uint8_t` | Domain 기능 데이터 | NORMAL_LIGHT_LEVEL: 사용자 NORMAL 밝기,0~100% |
| `normal_light_rgb` | [`NetRgb`](../../04_COMMON/10_DATA_AND_ID_RULES.md#type-netrgb) | Domain 기능 데이터 | NORMAL_LIGHT_RGB: 사용자 NORMAL 색상 R/G/B |
| `value_quality` | [`NetValueQuality`](../../04_COMMON/10_DATA_AND_ID_RULES.md#type-netvaluequality) | Domain 기능 데이터 | VALUE_QUALITY: 설정 확인 품질 |
| `update_sequence` | `uint32_t` | Domain 기능 데이터 | UPDATE_SEQUENCE: 현재 설정 확인 평가 번호 |
| `source_age_ms` | `uint16_t` | Domain 기능 데이터 | SOURCE_AGE: 설정 확인 평가 후 ms |


#### 필요한 중첩 필드만 펼쳐보기
직접 멤버는 앞의 표를 사용한다. 아래 표는 nested context/source 안의 경로만 표시하며 별도 DTO나 raw 필드를 추가하지 않는다.
| 필드 | 타입 | 소유자 | 의미·유효 조건 |
| --- | --- | --- | --- |
| `context.recipient.device_context_id` | `uint32_t` | 현재 단말·Session owner · Network 검증 | ESP32 등록 단말 식별. 0 예약. |
| `context.recipient.session_id` | `uint64_t` | 현재 단말·Session owner · Network 검증 | 현재 Session. 생성 방식은 ESP32/MOBILE 담당. |
| `context.domain_boot_id` | `uint32_t` | Domain Network | 정보를 제공하는 현재 Domain 기동 번호. |
| `context.query_id` | `uint32_t` | 원 Query ID (보존) | 자발 전송은 0. 조회 응답은 원 QUERY_ID. |
| `normal_light_rgb.r` | `uint8_t` | Domain 기능 데이터 | 빨강. 0~255. |
| `normal_light_rgb.g` | `uint8_t` | Domain 기능 데이터 | 초록. 0~255. |
| `normal_light_rgb.b` | `uint8_t` | Domain 기능 데이터 | 파랑. 0~255. |


<a id="type-netmobilequeryend"></a>
### `NetMobileQueryEnd`

M_QUERY_END. 원본 필드 의미를 보존한 통신 자료형.

**선언 종류:** 구조체

```c
typedef struct NetMobileQueryEnd
{
    NetDownContext context; /**< 현재 수신 문맥·Domain 기동·조회 식별. */
    NetQueryScope query_scope; /**< QUERY_SCOPE: 원 조회 범위 */
    NetQueryStatus query_status; /**< QUERY_STATUS: 0=완료; 1=요청 없음; 2=일부 확인 불가; 3=문맥 거부 */
    uint16_t item_count; /**< ITEM_COUNT: 조회 종료를 뺀 응답 메시지 수 */
} NetMobileQueryEnd;
```

#### 필드 구성
| 필드 | 타입 | 소유자 | 의미·유효 조건 |
| --- | --- | --- | --- |
| `context` | [`NetDownContext`](14_MOBILE_CODEC_DATA.md#type-netdowncontext) | 현재 수신 대상 문맥 owner · Network 조립 | 현재 수신 문맥·Domain 기동·조회 식별. |
| `query_scope` | [`NetQueryScope`](14_MOBILE_CODEC_DATA.md#type-netqueryscope) | MOBILE 원 Query → Network 보존 | 원 조회 범위. 활성 recipient/Boot/QueryID와 일치해야 한다. |
| `query_status` | [`NetQueryStatus`](14_MOBILE_CODEC_DATA.md#type-netquerystatus) | Domain owner 근거 + Network Query 처리 | 0=전송 목록 완료 후보, 1=확인된 원 요청 기록 부재, 2=부분/확인 불가, 3=문맥 거부. [종결 조건](25_PRIVATE_ROUTING.md#r2-query-producer); 차량 DONE 아님. |
| `item_count` | `uint16_t` | Network Query 송신 추적 | 응답 frame 수(개), u16. End/자발 갱신 제외. PROPOSED: 같은 조회의 증명된 최종 UART TX 완료 N만 계수; queue 접수나 BLE 수신 수가 아니다. [관측·0건·부분 제한](25_PRIVATE_ROUTING.md#r2-query-producer)은 BINDING_OPEN. |


#### 필요한 중첩 필드만 펼쳐보기
직접 멤버는 앞의 표를 사용한다. 아래 표는 nested context/source 안의 경로만 표시하며 별도 DTO나 raw 필드를 추가하지 않는다.
| 필드 | 타입 | 소유자 | 의미·유효 조건 |
| --- | --- | --- | --- |
| `context.recipient.device_context_id` | `uint32_t` | 현재 단말·Session owner · Network 검증 | ESP32 등록 단말 식별. 0 예약. |
| `context.recipient.session_id` | `uint64_t` | 현재 단말·Session owner · Network 검증 | 현재 Session. 생성 방식은 ESP32/MOBILE 담당. |
| `context.domain_boot_id` | `uint32_t` | Domain Network | 정보를 제공하는 현재 Domain 기동 번호. |
| `context.query_id` | `uint32_t` | 원 Query ID (보존) | 자발 전송은 0. 조회 응답은 원 QUERY_ID. |


<a id="type-netmobilebcmdoorstate"></a>
### `NetMobileBcmDoorState`

M_BCM_DOOR_STATE. CAN 원 메시지의 출처와 의미 필드를 하향 문맥과 함께 보존한다.

**선언 종류:** 구조체

```c
typedef struct NetMobileBcmDoorState
{
    NetDownContext context;
    NetCanSource source; /**< 원 CAN BOOT_ID/TX_SEQUENCE. Domain의 송신 번호로 바꾸지 않는다. */
    NetBcmDoorState state; /**< 원 값·품질·갱신 번호를 보존하고 AGE는 누적한다. CAN 패딩은 포함하지 않는다. */
} NetMobileBcmDoorState;
```

#### 필드 구성
| 필드 | 타입 | 소유자 | 의미·유효 조건 |
| --- | --- | --- | --- |
| `context` | [`NetDownContext`](14_MOBILE_CODEC_DATA.md#type-netdowncontext) | 현재 수신 대상 문맥 owner · Network 조립 | typed 의미 필드. 아래 범위와 UART-M의 같은 이름 필드 계약을 함께 적용한다. |
| `source` | [`NetCanSource`](../../04_COMMON/10_DATA_AND_ID_RULES.md#type-netcansource) | 원 CAN Network 출처 보존 | 원 CAN BOOT_ID/TX_SEQUENCE. Domain의 송신 번호로 바꾸지 않는다. |
| `state` | [`NetBcmDoorState`](../BCM/10_DATA.md#type-netbcmdoorstate) | BCM 원본 의미·품질 (Domain 중계 보존) | 원 값·품질·갱신 번호를 보존하고 AGE는 누적한다. CAN 패딩은 포함하지 않는다. |


#### 필요한 중첩 필드만 펼쳐보기
직접 멤버는 앞의 표를 사용한다. 아래 표는 nested context/source 안의 경로만 표시하며 별도 DTO나 raw 필드를 추가하지 않는다.
| 필드 | 타입 | 소유자 | 의미·유효 조건 |
| --- | --- | --- | --- |
| `context.recipient.device_context_id` | `uint32_t` | 현재 단말·Session owner · Network 검증 | ESP32 등록 단말 식별. 0 예약. |
| `context.recipient.session_id` | `uint64_t` | 현재 단말·Session owner · Network 검증 | 현재 Session. 생성 방식은 ESP32/MOBILE 담당. |
| `context.domain_boot_id` | `uint32_t` | Domain Network | 정보를 제공하는 현재 Domain 기동 번호. |
| `context.query_id` | `uint32_t` | 원 Query ID (보존) | 자발 전송은 0. 조회 응답은 원 QUERY_ID. |
| `source.boot_id` | `uint32_t` | 원 CAN Network (보존) | 송신 노드 기동 번호. 0 예약. |
| `source.tx_sequence` | `uint32_t` | 원 CAN Network (보존) | 같은 메시지의 전송 순번. 새 관측 번호가 아니다. |
| `state.door_lock_state` | [`NetDoorLockState`](../BCM/10_DATA.md#type-netdoorlockstate) | BCM 원본 의미·품질 (Domain 중계 보존) | DOOR_LOCK_STATE: 0=LOCKED; 1=UNLOCKED; 255=UNKNOWN |
| `state.door_open_state` | [`NetDoorOpenState`](../BCM/10_DATA.md#type-netdooropenstate) | BCM 원본 의미·품질 (Domain 중계 보존) | DOOR_OPEN_STATE: 0=CLOSED; 1=OPEN; 255=UNKNOWN |
| `state.door_composite_state` | [`NetDoorCompositeState`](../BCM/10_DATA.md#type-netdoorcompositestate) | BCM 원본 의미·품질 (Domain 중계 보존) | DOOR_COMPOSITE_STATE: 0=NORMAL; 1=INCONSISTENT; 2=UNTRUSTED |
| `state.lock_quality` | [`NetValueQuality`](../../04_COMMON/10_DATA_AND_ID_RULES.md#type-netvaluequality) | BCM 원본 의미·품질 (Domain 중계 보존) | LOCK_QUALITY: VALUE_QUALITY |
| `state.open_quality` | [`NetValueQuality`](../../04_COMMON/10_DATA_AND_ID_RULES.md#type-netvaluequality) | BCM 원본 의미·품질 (Domain 중계 보존) | OPEN_QUALITY: VALUE_QUALITY |
| `state.composite_quality` | [`NetValueQuality`](../../04_COMMON/10_DATA_AND_ID_RULES.md#type-netvaluequality) | BCM 원본 의미·품질 (Domain 중계 보존) | COMPOSITE_QUALITY: VALUE_QUALITY |
| `state.update_sequence` | `uint32_t` | BCM 원본 의미·품질 (Domain 중계 보존) | UPDATE_SEQUENCE: 이번 도어 상태 평가 번호 |
| `state.source_age_ms` | `uint16_t` | BCM 원본 의미·품질 (Domain 중계 보존) | SOURCE_AGE: 평가 후 경과 시간, ms |


<a id="type-netmobilebcmclimatestate"></a>
### `NetMobileBcmClimateState`

M_BCM_CLIMATE_STATE. CAN 원 메시지의 출처와 의미 필드를 하향 문맥과 함께 보존한다.

**선언 종류:** 구조체

```c
typedef struct NetMobileBcmClimateState
{
    NetDownContext context;
    NetCanSource source; /**< 원 CAN BOOT_ID/TX_SEQUENCE. Domain의 송신 번호로 바꾸지 않는다. */
    NetBcmClimateState state; /**< 원 값·품질·갱신 번호를 보존하고 AGE는 누적한다. CAN 패딩은 포함하지 않는다. */
} NetMobileBcmClimateState;
```

#### 필드 구성
| 필드 | 타입 | 소유자 | 의미·유효 조건 |
| --- | --- | --- | --- |
| `context` | [`NetDownContext`](14_MOBILE_CODEC_DATA.md#type-netdowncontext) | 현재 수신 대상 문맥 owner · Network 조립 | typed 의미 필드. 아래 범위와 UART-M의 같은 이름 필드 계약을 함께 적용한다. |
| `source` | [`NetCanSource`](../../04_COMMON/10_DATA_AND_ID_RULES.md#type-netcansource) | 원 CAN Network 출처 보존 | 원 CAN BOOT_ID/TX_SEQUENCE. Domain의 송신 번호로 바꾸지 않는다. |
| `state` | [`NetBcmClimateState`](../BCM/10_DATA.md#type-netbcmclimatestate) | BCM 원본 의미·품질 (Domain 중계 보존) | 원 값·품질·갱신 번호를 보존하고 AGE는 누적한다. CAN 패딩은 포함하지 않는다. |


#### 필요한 중첩 필드만 펼쳐보기
직접 멤버는 앞의 표를 사용한다. 아래 표는 nested context/source 안의 경로만 표시하며 별도 DTO나 raw 필드를 추가하지 않는다.
| 필드 | 타입 | 소유자 | 의미·유효 조건 |
| --- | --- | --- | --- |
| `context.recipient.device_context_id` | `uint32_t` | 현재 단말·Session owner · Network 검증 | ESP32 등록 단말 식별. 0 예약. |
| `context.recipient.session_id` | `uint64_t` | 현재 단말·Session owner · Network 검증 | 현재 Session. 생성 방식은 ESP32/MOBILE 담당. |
| `context.domain_boot_id` | `uint32_t` | Domain Network | 정보를 제공하는 현재 Domain 기동 번호. |
| `context.query_id` | `uint32_t` | 원 Query ID (보존) | 자발 전송은 0. 조회 응답은 원 QUERY_ID. |
| `source.boot_id` | `uint32_t` | 원 CAN Network (보존) | 송신 노드 기동 번호. 0 예약. |
| `source.tx_sequence` | `uint32_t` | 원 CAN Network (보존) | 같은 메시지의 전송 순번. 새 관측 번호가 아니다. |
| `state.fan_command_level` | [`NetFanLevel`](../../04_COMMON/10_DATA_AND_ID_RULES.md#type-netfanlevel) | BCM 원본 의미·품질 (Domain 중계 보존) | FAN_COMMAND_LEVEL: 0=OFF; 1=LOW; 2=MEDIUM; 3=HIGH |
| `state.fan_measured_level` | [`NetFanLevel`](../../04_COMMON/10_DATA_AND_ID_RULES.md#type-netfanlevel) | BCM 원본 의미·품질 (Domain 중계 보존) | FAN_MEASURED_LEVEL: FAN 코드 및 255=UNKNOWN |
| `state.thermal_direction_state` | [`NetThermalDirection`](../../04_COMMON/10_DATA_AND_ID_RULES.md#type-netthermaldirection) | BCM 원본 의미·품질 (Domain 중계 보존) | THERMAL_DIRECTION_STATE: 0=IDLE; 1=COOL; 2=HEAT |
| `state.thermal_output_percent` | `uint8_t` | BCM 원본 의미·품질 (Domain 중계 보존) | THERMAL_OUTPUT_LEVEL: 0~100%, 1%/raw |
| `state.heat_removal_state` | [`NetHeatRemovalState`](../BCM/10_DATA.md#type-netheatremovalstate) | BCM 원본 의미·품질 (Domain 중계 보존) | HEAT_REMOVAL_STATE: 0=정상; 1=과열 차단; 255=측정 불가 |
| `state.fan_command_quality` | [`NetValueQuality`](../../04_COMMON/10_DATA_AND_ID_RULES.md#type-netvaluequality) | BCM 원본 의미·품질 (Domain 중계 보존) | FAN_COMMAND_QUALITY: VALUE_QUALITY |
| `state.fan_measured_quality` | [`NetValueQuality`](../../04_COMMON/10_DATA_AND_ID_RULES.md#type-netvaluequality) | BCM 원본 의미·품질 (Domain 중계 보존) | FAN_MEASURED_QUALITY: VALUE_QUALITY |
| `state.thermal_direction_quality` | [`NetValueQuality`](../../04_COMMON/10_DATA_AND_ID_RULES.md#type-netvaluequality) | BCM 원본 의미·품질 (Domain 중계 보존) | THERMAL_DIRECTION_QUALITY: VALUE_QUALITY |
| `state.thermal_output_quality` | [`NetValueQuality`](../../04_COMMON/10_DATA_AND_ID_RULES.md#type-netvaluequality) | BCM 원본 의미·품질 (Domain 중계 보존) | THERMAL_OUTPUT_QUALITY: VALUE_QUALITY |
| `state.heat_removal_quality` | [`NetValueQuality`](../../04_COMMON/10_DATA_AND_ID_RULES.md#type-netvaluequality) | BCM 원본 의미·품질 (Domain 중계 보존) | HEAT_REMOVAL_QUALITY: VALUE_QUALITY |
| `state.update_sequence` | `uint32_t` | BCM 원본 의미·품질 (Domain 중계 보존) | UPDATE_SEQUENCE: 이번 공조 상태 평가 번호 |
| `state.source_age_ms` | `uint16_t` | BCM 원본 의미·품질 (Domain 중계 보존) | SOURCE_AGE: 평가 후 경과 시간, ms |


<a id="type-netmobilebcmlightstate"></a>
### `NetMobileBcmLightState`

M_BCM_LIGHT_STATE. CAN 원 메시지의 출처와 의미 필드를 하향 문맥과 함께 보존한다.

**선언 종류:** 구조체

```c
typedef struct NetMobileBcmLightState
{
    NetDownContext context;
    NetCanSource source; /**< 원 CAN BOOT_ID/TX_SEQUENCE. Domain의 송신 번호로 바꾸지 않는다. */
    NetBcmLightState state; /**< 원 값·품질·갱신 번호를 보존하고 AGE는 누적한다. CAN 패딩은 포함하지 않는다. */
} NetMobileBcmLightState;
```

#### 필드 구성
| 필드 | 타입 | 소유자 | 의미·유효 조건 |
| --- | --- | --- | --- |
| `context` | [`NetDownContext`](14_MOBILE_CODEC_DATA.md#type-netdowncontext) | 현재 수신 대상 문맥 owner · Network 조립 | typed 의미 필드. 아래 범위와 UART-M의 같은 이름 필드 계약을 함께 적용한다. |
| `source` | [`NetCanSource`](../../04_COMMON/10_DATA_AND_ID_RULES.md#type-netcansource) | 원 CAN Network 출처 보존 | 원 CAN BOOT_ID/TX_SEQUENCE. Domain의 송신 번호로 바꾸지 않는다. |
| `state` | [`NetBcmLightState`](../BCM/10_DATA.md#type-netbcmlightstate) | BCM 원본 의미·품질 (Domain 중계 보존) | 원 값·품질·갱신 번호를 보존하고 AGE는 누적한다. CAN 패딩은 포함하지 않는다. |


#### 필요한 중첩 필드만 펼쳐보기
직접 멤버는 앞의 표를 사용한다. 아래 표는 nested context/source 안의 경로만 표시하며 별도 DTO나 raw 필드를 추가하지 않는다.
| 필드 | 타입 | 소유자 | 의미·유효 조건 |
| --- | --- | --- | --- |
| `context.recipient.device_context_id` | `uint32_t` | 현재 단말·Session owner · Network 검증 | ESP32 등록 단말 식별. 0 예약. |
| `context.recipient.session_id` | `uint64_t` | 현재 단말·Session owner · Network 검증 | 현재 Session. 생성 방식은 ESP32/MOBILE 담당. |
| `context.domain_boot_id` | `uint32_t` | Domain Network | 정보를 제공하는 현재 Domain 기동 번호. |
| `context.query_id` | `uint32_t` | 원 Query ID (보존) | 자발 전송은 0. 조회 응답은 원 QUERY_ID. |
| `source.boot_id` | `uint32_t` | 원 CAN Network (보존) | 송신 노드 기동 번호. 0 예약. |
| `source.tx_sequence` | `uint32_t` | 원 CAN Network (보존) | 같은 메시지의 전송 순번. 새 관측 번호가 아니다. |
| `state.interior_light_type_state` | [`NetLightType`](../../04_COMMON/10_DATA_AND_ID_RULES.md#type-netlighttype) | BCM 원본 의미·품질 (Domain 중계 보존) | INTERIOR_LIGHT_TYPE_STATE: LIGHT_TYPE 코드 |
| `state.light_level_percent` | `uint8_t` | BCM 원본 의미·품질 (Domain 중계 보존) | INTERIOR_LIGHT_LEVEL_STATE: 0~100% |
| `state.interior_light_color_state.r` | `uint8_t` | BCM 원본 의미·품질 (Domain 중계 보존) | 빨강. 0~255. |
| `state.interior_light_color_state.g` | `uint8_t` | BCM 원본 의미·품질 (Domain 중계 보존) | 초록. 0~255. |
| `state.interior_light_color_state.b` | `uint8_t` | BCM 원본 의미·품질 (Domain 중계 보존) | 파랑. 0~255. |
| `state.interior_light_apply_result` | [`NetLightApplyResult`](../BCM/10_DATA.md#type-netlightapplyresult) | BCM 원본 의미·품질 (Domain 중계 보존) | INTERIOR_LIGHT_APPLY_RESULT: 0=적용됨; 1=실패; 255=UNKNOWN |
| `state.light_physical_feedback_capability` | [`NetFlag`](../../04_COMMON/10_DATA_AND_ID_RULES.md#type-netflag) | BCM 원본 의미·품질 (Domain 중계 보존) | LIGHT_PHYSICAL_FEEDBACK_CAPABILITY: 0=미지원; 1=지원; 현재 0 |
| `state.light_type_quality` | [`NetValueQuality`](../../04_COMMON/10_DATA_AND_ID_RULES.md#type-netvaluequality) | BCM 원본 의미·품질 (Domain 중계 보존) | LIGHT_TYPE_QUALITY: VALUE_QUALITY |
| `state.light_level_quality` | [`NetValueQuality`](../../04_COMMON/10_DATA_AND_ID_RULES.md#type-netvaluequality) | BCM 원본 의미·품질 (Domain 중계 보존) | LIGHT_LEVEL_QUALITY: VALUE_QUALITY |
| `state.light_color_quality` | [`NetValueQuality`](../../04_COMMON/10_DATA_AND_ID_RULES.md#type-netvaluequality) | BCM 원본 의미·품질 (Domain 중계 보존) | LIGHT_COLOR_QUALITY: VALUE_QUALITY |
| `state.light_apply_quality` | [`NetValueQuality`](../../04_COMMON/10_DATA_AND_ID_RULES.md#type-netvaluequality) | BCM 원본 의미·품질 (Domain 중계 보존) | LIGHT_APPLY_QUALITY: VALUE_QUALITY |
| `state.update_sequence` | `uint32_t` | BCM 원본 의미·품질 (Domain 중계 보존) | UPDATE_SEQUENCE: 이번 조명 적용 상태 평가 번호 |
| `state.source_age_ms` | `uint16_t` | BCM 원본 의미·품질 (Domain 중계 보존) | SOURCE_AGE: 평가 후 경과 시간, ms |


<a id="type-netmobilebcmstatus"></a>
### `NetMobileBcmStatus`

M_BCM_STATUS. CAN 원 메시지의 출처와 의미 필드를 하향 문맥과 함께 보존한다.

**선언 종류:** 구조체

```c
typedef struct NetMobileBcmStatus
{
    NetDownContext context;
    NetCanSource source; /**< 원 CAN BOOT_ID/TX_SEQUENCE. Domain의 송신 번호로 바꾸지 않는다. */
    NetBcmStatus state; /**< 원 값·품질·갱신 번호를 보존하고 AGE는 누적한다. CAN 패딩은 포함하지 않는다. */
} NetMobileBcmStatus;
```

#### 필드 구성
| 필드 | 타입 | 소유자 | 의미·유효 조건 |
| --- | --- | --- | --- |
| `context` | [`NetDownContext`](14_MOBILE_CODEC_DATA.md#type-netdowncontext) | 현재 수신 대상 문맥 owner · Network 조립 | typed 의미 필드. 아래 범위와 UART-M의 같은 이름 필드 계약을 함께 적용한다. |
| `source` | [`NetCanSource`](../../04_COMMON/10_DATA_AND_ID_RULES.md#type-netcansource) | 원 CAN Network 출처 보존 | 원 CAN BOOT_ID/TX_SEQUENCE. Domain의 송신 번호로 바꾸지 않는다. |
| `state` | [`NetBcmStatus`](../BCM/10_DATA.md#type-netbcmstatus) | BCM 원본 의미·품질 (Domain 중계 보존) | 원 값·품질·갱신 번호를 보존하고 AGE는 누적한다. CAN 패딩은 포함하지 않는다. |


#### 필요한 중첩 필드만 펼쳐보기
직접 멤버는 앞의 표를 사용한다. 아래 표는 nested context/source 안의 경로만 표시하며 별도 DTO나 raw 필드를 추가하지 않는다.
| 필드 | 타입 | 소유자 | 의미·유효 조건 |
| --- | --- | --- | --- |
| `context.recipient.device_context_id` | `uint32_t` | 현재 단말·Session owner · Network 검증 | ESP32 등록 단말 식별. 0 예약. |
| `context.recipient.session_id` | `uint64_t` | 현재 단말·Session owner · Network 검증 | 현재 Session. 생성 방식은 ESP32/MOBILE 담당. |
| `context.domain_boot_id` | `uint32_t` | Domain Network | 정보를 제공하는 현재 Domain 기동 번호. |
| `context.query_id` | `uint32_t` | 원 Query ID (보존) | 자발 전송은 0. 조회 응답은 원 QUERY_ID. |
| `source.boot_id` | `uint32_t` | 원 CAN Network (보존) | 송신 노드 기동 번호. 0 예약. |
| `source.tx_sequence` | `uint32_t` | 원 CAN Network (보존) | 같은 메시지의 전송 순번. 새 관측 번호가 아니다. |
| `state.bcm_ecu_state` | [`NetControllerState`](../../04_COMMON/10_DATA_AND_ID_RULES.md#type-netcontrollerstate) | BCM 원본 의미·품질 (Domain 중계 보존) | BCM_ECU_STATE: 0=INIT; 1=READY; 2=DEGRADED; 3=FAULT |
| `state.fault_mask` | `uint16_t` | BCM 원본 의미·품질 (Domain 중계 보존) | FAULT_MASK: BCM 고장 비트 0~7 |
| `state.active_fault_category` | [`NetFaultCategory`](../../04_COMMON/10_DATA_AND_ID_RULES.md#type-netfaultcategory) | BCM 원본 의미·품질 (Domain 중계 보존) | ACTIVE_FAULT_CATEGORY: 0=없음; 1=SENSOR; 2=FUNCTION; 3=COMM |
| `state.active_fault_code` | `uint16_t` | BCM 원본 의미·품질 (Domain 중계 보존) | ACTIVE_FAULT_CODE: 현재 주요 고장; bit 번호+1 |
| `state.affected_function` | `uint8_t` | BCM 원본 의미·품질 (Domain 중계 보존) | AFFECTED_FUNCTION: bit0=도어; bit1=공조; bit2=조명 |
| `state.recovering` | [`NetFlag`](../../04_COMMON/10_DATA_AND_ID_RULES.md#type-netflag) | BCM 원본 의미·품질 (Domain 중계 보존) | RECOVERING: 0=아님; 1=복구 중 |
| `state.update_sequence` | `uint32_t` | BCM 원본 의미·품질 (Domain 중계 보존) | UPDATE_SEQUENCE: 새 상태 평가 |
| `state.source_age_ms` | `uint16_t` | BCM 원본 의미·품질 (Domain 중계 보존) | SOURCE_AGE: 평가 후 경과 시간, ms |


<a id="type-netmobilecisenvironment"></a>
### `NetMobileCisEnvironment`

M_CIS_ENVIRONMENT. CAN 원 메시지의 출처와 의미 필드를 하향 문맥과 함께 보존한다.

**선언 종류:** 구조체

```c
typedef struct NetMobileCisEnvironment
{
    NetDownContext context;
    NetCanSource source; /**< 원 CAN BOOT_ID/TX_SEQUENCE. Domain의 송신 번호로 바꾸지 않는다. */
    NetCisEnvironment state; /**< 원 값·품질·갱신 번호를 보존하고 AGE는 누적한다. CAN 패딩은 포함하지 않는다. */
} NetMobileCisEnvironment;
```

#### 필드 구성
| 필드 | 타입 | 소유자 | 의미·유효 조건 |
| --- | --- | --- | --- |
| `context` | [`NetDownContext`](14_MOBILE_CODEC_DATA.md#type-netdowncontext) | 현재 수신 대상 문맥 owner · Network 조립 | typed 의미 필드. 아래 범위와 UART-M의 같은 이름 필드 계약을 함께 적용한다. |
| `source` | [`NetCanSource`](../../04_COMMON/10_DATA_AND_ID_RULES.md#type-netcansource) | 원 CAN Network 출처 보존 | 원 CAN BOOT_ID/TX_SEQUENCE. Domain의 송신 번호로 바꾸지 않는다. |
| `state` | [`NetCisEnvironment`](../CIS/10_DATA.md#type-netcisenvironment) | CIS 원본 의미·품질 (Domain 중계 보존) | 원 값·품질·갱신 번호를 보존하고 AGE는 누적한다. CAN 패딩은 포함하지 않는다. |


#### 필요한 중첩 필드만 펼쳐보기
직접 멤버는 앞의 표를 사용한다. 아래 표는 nested context/source 안의 경로만 표시하며 별도 DTO나 raw 필드를 추가하지 않는다.
| 필드 | 타입 | 소유자 | 의미·유효 조건 |
| --- | --- | --- | --- |
| `context.recipient.device_context_id` | `uint32_t` | 현재 단말·Session owner · Network 검증 | ESP32 등록 단말 식별. 0 예약. |
| `context.recipient.session_id` | `uint64_t` | 현재 단말·Session owner · Network 검증 | 현재 Session. 생성 방식은 ESP32/MOBILE 담당. |
| `context.domain_boot_id` | `uint32_t` | Domain Network | 정보를 제공하는 현재 Domain 기동 번호. |
| `context.query_id` | `uint32_t` | 원 Query ID (보존) | 자발 전송은 0. 조회 응답은 원 QUERY_ID. |
| `source.boot_id` | `uint32_t` | 원 CAN Network (보존) | 송신 노드 기동 번호. 0 예약. |
| `source.tx_sequence` | `uint32_t` | 원 CAN Network (보존) | 같은 메시지의 전송 순번. 새 관측 번호가 아니다. |
| `state.temperature_centi_c` | `int16_t` | CIS 원본 의미·품질 (Domain 중계 보존) | TEMPERATURE: 0.01°C/raw; -4000~12500; -32768=무효 |
| `state.temperature_sequence` | `uint32_t` | CIS 원본 의미·품질 (Domain 중계 보존) | TEMPERATURE_SEQUENCE: 해당 값의 새 측정 번호 |
| `state.temperature_age_ms` | `uint16_t` | CIS 원본 의미·품질 (Domain 중계 보존) | TEMPERATURE_AGE: 해당 값 원본 측정 후 ms |
| `state.temperature_validity` | [`NetValidity`](../../04_COMMON/10_DATA_AND_ID_RULES.md#type-netvalidity) | CIS 원본 의미·품질 (Domain 중계 보존) | TEMPERATURE_VALIDITY: 0=INVALID; 1=VALID |
| `state.temperature_reason` | [`NetQualityReason`](../../04_COMMON/10_DATA_AND_ID_RULES.md#type-netqualityreason) | CIS 원본 의미·품질 (Domain 중계 보존) | TEMPERATURE_REASON: CIS 품질 사유 |
| `state.humidity_centi_percent` | `uint16_t` | CIS 원본 의미·품질 (Domain 중계 보존) | HUMIDITY: 0.01%RH/raw; 0~10000; 65535=무효 |
| `state.humidity_sequence` | `uint32_t` | CIS 원본 의미·품질 (Domain 중계 보존) | HUMIDITY_SEQUENCE: 해당 값의 새 측정 번호 |
| `state.humidity_age_ms` | `uint16_t` | CIS 원본 의미·품질 (Domain 중계 보존) | HUMIDITY_AGE: 해당 값 원본 측정 후 ms |
| `state.humidity_validity` | [`NetValidity`](../../04_COMMON/10_DATA_AND_ID_RULES.md#type-netvalidity) | CIS 원본 의미·품질 (Domain 중계 보존) | HUMIDITY_VALIDITY: 0=INVALID; 1=VALID |
| `state.humidity_reason` | [`NetQualityReason`](../../04_COMMON/10_DATA_AND_ID_RULES.md#type-netqualityreason) | CIS 원본 의미·품질 (Domain 중계 보존) | HUMIDITY_REASON: CIS 품질 사유 |
| `state.illuminance_lux` | `uint32_t` | CIS 원본 의미·품질 (Domain 중계 보존) | ILLUMINANCE: 1 lx/raw; 표현 범위 0~200000; 0xFFFFFFFF=무효 |
| `state.illuminance_sequence` | `uint32_t` | CIS 원본 의미·품질 (Domain 중계 보존) | ILLUMINANCE_SEQUENCE: 해당 값의 새 측정 번호 |
| `state.illuminance_age_ms` | `uint16_t` | CIS 원본 의미·품질 (Domain 중계 보존) | ILLUMINANCE_AGE: 해당 값 원본 측정 후 ms |
| `state.illuminance_validity` | [`NetValidity`](../../04_COMMON/10_DATA_AND_ID_RULES.md#type-netvalidity) | CIS 원본 의미·품질 (Domain 중계 보존) | ILLUMINANCE_VALIDITY: 0=INVALID; 1=VALID |
| `state.illuminance_reason` | [`NetQualityReason`](../../04_COMMON/10_DATA_AND_ID_RULES.md#type-netqualityreason) | CIS 원본 의미·품질 (Domain 중계 보존) | ILLUMINANCE_REASON: CIS 품질 사유 |


<a id="type-netmobilecisoccupant"></a>
### `NetMobileCisOccupant`

M_CIS_OCCUPANT. CAN 원 메시지의 출처와 의미 필드를 하향 문맥과 함께 보존한다.

**선언 종류:** 구조체

```c
typedef struct NetMobileCisOccupant
{
    NetDownContext context;
    NetCanSource source; /**< 원 CAN BOOT_ID/TX_SEQUENCE. Domain의 송신 번호로 바꾸지 않는다. */
    NetCisOccupant state; /**< 원 값·품질·갱신 번호를 보존하고 AGE는 누적한다. CAN 패딩은 포함하지 않는다. */
} NetMobileCisOccupant;
```

#### 필드 구성
| 필드 | 타입 | 소유자 | 의미·유효 조건 |
| --- | --- | --- | --- |
| `context` | [`NetDownContext`](14_MOBILE_CODEC_DATA.md#type-netdowncontext) | 현재 수신 대상 문맥 owner · Network 조립 | typed 의미 필드. 아래 범위와 UART-M의 같은 이름 필드 계약을 함께 적용한다. |
| `source` | [`NetCanSource`](../../04_COMMON/10_DATA_AND_ID_RULES.md#type-netcansource) | 원 CAN Network 출처 보존 | 원 CAN BOOT_ID/TX_SEQUENCE. Domain의 송신 번호로 바꾸지 않는다. |
| `state` | [`NetCisOccupant`](../CIS/10_DATA.md#type-netcisoccupant) | CIS 원본 의미·품질 (Domain 중계 보존) | 원 값·품질·갱신 번호를 보존하고 AGE는 누적한다. CAN 패딩은 포함하지 않는다. |


#### 필요한 중첩 필드만 펼쳐보기
직접 멤버는 앞의 표를 사용한다. 아래 표는 nested context/source 안의 경로만 표시하며 별도 DTO나 raw 필드를 추가하지 않는다.
| 필드 | 타입 | 소유자 | 의미·유효 조건 |
| --- | --- | --- | --- |
| `context.recipient.device_context_id` | `uint32_t` | 현재 단말·Session owner · Network 검증 | ESP32 등록 단말 식별. 0 예약. |
| `context.recipient.session_id` | `uint64_t` | 현재 단말·Session owner · Network 검증 | 현재 Session. 생성 방식은 ESP32/MOBILE 담당. |
| `context.domain_boot_id` | `uint32_t` | Domain Network | 정보를 제공하는 현재 Domain 기동 번호. |
| `context.query_id` | `uint32_t` | 원 Query ID (보존) | 자발 전송은 0. 조회 응답은 원 QUERY_ID. |
| `source.boot_id` | `uint32_t` | 원 CAN Network (보존) | 송신 노드 기동 번호. 0 예약. |
| `source.tx_sequence` | `uint32_t` | 원 CAN Network (보존) | 같은 메시지의 전송 순번. 새 관측 번호가 아니다. |
| `state.pi_boot_id` | `uint32_t` | CIS 원본 의미·품질 (Domain 중계 보존) | PI_BOOT_ID: Pi 원본 기동 번호 |
| `state.vision_sequence` | `uint32_t` | CIS 원본 의미·품질 (Domain 중계 보존) | VISION_SEQUENCE: Pi 새 판정 번호, 중계 시 보존 |
| `state.vision_age_ms` | `uint16_t` | CIS 원본 의미·품질 (Domain 중계 보존) | VISION_AGE: Pi 원본 판정 후 누적 ms |
| `state.occupant_presence` | [`NetTriState`](../../04_COMMON/10_DATA_AND_ID_RULES.md#type-nettristate) | CIS 원본 의미·품질 (Domain 중계 보존) | OCCUPANT_PRESENCE: 0=없음; 1=있음; 255=확인 불가 |
| `state.presence_validity` | [`NetValidity`](../../04_COMMON/10_DATA_AND_ID_RULES.md#type-netvalidity) | CIS 원본 의미·품질 (Domain 중계 보존) | PRESENCE_VALIDITY: 0=INVALID; 1=VALID |
| `state.presence_reason` | [`NetQualityReason`](../../04_COMMON/10_DATA_AND_ID_RULES.md#type-netqualityreason) | CIS 원본 의미·품질 (Domain 중계 보존) | PRESENCE_REASON: CIS 품질 사유 |
| `state.occupant_count` | `uint8_t` | CIS 원본 의미·품질 (Domain 중계 보존) | OCCUPANT_COUNT: 0~5 명,255=확인 불가 |
| `state.count_validity` | [`NetValidity`](../../04_COMMON/10_DATA_AND_ID_RULES.md#type-netvalidity) | CIS 원본 의미·품질 (Domain 중계 보존) | COUNT_VALIDITY: 0=INVALID; 1=VALID |
| `state.count_reason` | [`NetQualityReason`](../../04_COMMON/10_DATA_AND_ID_RULES.md#type-netqualityreason) | CIS 원본 의미·품질 (Domain 중계 보존) | COUNT_REASON: CIS 품질 사유 |
| `state.vision_function_status` | [`NetSensorFunctionState`](../../04_COMMON/10_DATA_AND_ID_RULES.md#type-netsensorfunctionstate) | CIS 원본 의미·품질 (Domain 중계 보존) | VISION_FUNCTION_STATUS: CIS 기능 상태 |


<a id="type-netmobilecisrear"></a>
### `NetMobileCisRear`

M_CIS_REAR. CAN 원 메시지의 출처와 의미 필드를 하향 문맥과 함께 보존한다.

**선언 종류:** 구조체

```c
typedef struct NetMobileCisRear
{
    NetDownContext context;
    NetCanSource source; /**< 원 CAN BOOT_ID/TX_SEQUENCE. Domain의 송신 번호로 바꾸지 않는다. */
    NetCisRear state; /**< 원 값·품질·갱신 번호를 보존하고 AGE는 누적한다. CAN 패딩은 포함하지 않는다. */
} NetMobileCisRear;
```

#### 필드 구성
| 필드 | 타입 | 소유자 | 의미·유효 조건 |
| --- | --- | --- | --- |
| `context` | [`NetDownContext`](14_MOBILE_CODEC_DATA.md#type-netdowncontext) | 현재 수신 대상 문맥 owner · Network 조립 | typed 의미 필드. 아래 범위와 UART-M의 같은 이름 필드 계약을 함께 적용한다. |
| `source` | [`NetCanSource`](../../04_COMMON/10_DATA_AND_ID_RULES.md#type-netcansource) | 원 CAN Network 출처 보존 | 원 CAN BOOT_ID/TX_SEQUENCE. Domain의 송신 번호로 바꾸지 않는다. |
| `state` | [`NetCisRear`](../CIS/10_DATA.md#type-netcisrear) | CIS 원본 의미·품질 (Domain 중계 보존) | 원 값·품질·갱신 번호를 보존하고 AGE는 누적한다. CAN 패딩은 포함하지 않는다. |


#### 필요한 중첩 필드만 펼쳐보기
직접 멤버는 앞의 표를 사용한다. 아래 표는 nested context/source 안의 경로만 표시하며 별도 DTO나 raw 필드를 추가하지 않는다.
| 필드 | 타입 | 소유자 | 의미·유효 조건 |
| --- | --- | --- | --- |
| `context.recipient.device_context_id` | `uint32_t` | 현재 단말·Session owner · Network 검증 | ESP32 등록 단말 식별. 0 예약. |
| `context.recipient.session_id` | `uint64_t` | 현재 단말·Session owner · Network 검증 | 현재 Session. 생성 방식은 ESP32/MOBILE 담당. |
| `context.domain_boot_id` | `uint32_t` | Domain Network | 정보를 제공하는 현재 Domain 기동 번호. |
| `context.query_id` | `uint32_t` | 원 Query ID (보존) | 자발 전송은 0. 조회 응답은 원 QUERY_ID. |
| `source.boot_id` | `uint32_t` | 원 CAN Network (보존) | 송신 노드 기동 번호. 0 예약. |
| `source.tx_sequence` | `uint32_t` | 원 CAN Network (보존) | 같은 메시지의 전송 순번. 새 관측 번호가 아니다. |
| `state.update_sequence` | `uint32_t` | CIS 원본 의미·품질 (Domain 중계 보존) | UPDATE_SEQUENCE: 새 거리 관측 번호 |
| `state.source_age_ms` | `uint16_t` | CIS 원본 의미·품질 (Domain 중계 보존) | SOURCE_AGE: 거리 원본 관측 후 ms |
| `state.rear_distance_deci_cm` | `uint16_t` | CIS 원본 의미·품질 (Domain 중계 보존) | REAR_DISTANCE: 0.1cm/raw; 65535=거리 없음/무효, 상태로 구분 |
| `state.validity` | [`NetValidity`](../../04_COMMON/10_DATA_AND_ID_RULES.md#type-netvalidity) | CIS 원본 의미·품질 (Domain 중계 보존) | VALIDITY: 0=INVALID; 1=VALID |
| `state.quality_reason` | [`NetQualityReason`](../../04_COMMON/10_DATA_AND_ID_RULES.md#type-netqualityreason) | CIS 원본 의미·품질 (Domain 중계 보존) | QUALITY_REASON: CIS 품질 사유 |
| `state.proximity_status` | [`NetProximityStatus`](../CIS/10_DATA.md#type-netproximitystatus) | CIS 원본 의미·품질 (Domain 중계 보존) | PROXIMITY_STATUS: 0=VALID_DISTANCE; 1=NO_OBJECT; 2=UNAVAILABLE; 3=FAULT; 4=RECOVERING; 5=INACTIVE |


<a id="type-netmobilecisstatus"></a>
### `NetMobileCisStatus`

M_CIS_STATUS. CAN 원 메시지의 출처와 의미 필드를 하향 문맥과 함께 보존한다.

**선언 종류:** 구조체

```c
typedef struct NetMobileCisStatus
{
    NetDownContext context;
    NetCanSource source; /**< 원 CAN BOOT_ID/TX_SEQUENCE. Domain의 송신 번호로 바꾸지 않는다. */
    NetCisStatus state; /**< 원 값·품질·갱신 번호를 보존하고 AGE는 누적한다. CAN 패딩은 포함하지 않는다. */
} NetMobileCisStatus;
```

#### 필드 구성
| 필드 | 타입 | 소유자 | 의미·유효 조건 |
| --- | --- | --- | --- |
| `context` | [`NetDownContext`](14_MOBILE_CODEC_DATA.md#type-netdowncontext) | 현재 수신 대상 문맥 owner · Network 조립 | typed 의미 필드. 아래 범위와 UART-M의 같은 이름 필드 계약을 함께 적용한다. |
| `source` | [`NetCanSource`](../../04_COMMON/10_DATA_AND_ID_RULES.md#type-netcansource) | 원 CAN Network 출처 보존 | 원 CAN BOOT_ID/TX_SEQUENCE. Domain의 송신 번호로 바꾸지 않는다. |
| `state` | [`NetCisStatus`](../CIS/10_DATA.md#type-netcisstatus) | CIS 원본 의미·품질 (Domain 중계 보존) | 원 값·품질·갱신 번호를 보존하고 AGE는 누적한다. CAN 패딩은 포함하지 않는다. |


#### 필요한 중첩 필드만 펼쳐보기
직접 멤버는 앞의 표를 사용한다. 아래 표는 nested context/source 안의 경로만 표시하며 별도 DTO나 raw 필드를 추가하지 않는다.
| 필드 | 타입 | 소유자 | 의미·유효 조건 |
| --- | --- | --- | --- |
| `context.recipient.device_context_id` | `uint32_t` | 현재 단말·Session owner · Network 검증 | ESP32 등록 단말 식별. 0 예약. |
| `context.recipient.session_id` | `uint64_t` | 현재 단말·Session owner · Network 검증 | 현재 Session. 생성 방식은 ESP32/MOBILE 담당. |
| `context.domain_boot_id` | `uint32_t` | Domain Network | 정보를 제공하는 현재 Domain 기동 번호. |
| `context.query_id` | `uint32_t` | 원 Query ID (보존) | 자발 전송은 0. 조회 응답은 원 QUERY_ID. |
| `source.boot_id` | `uint32_t` | 원 CAN Network (보존) | 송신 노드 기동 번호. 0 예약. |
| `source.tx_sequence` | `uint32_t` | 원 CAN Network (보존) | 같은 메시지의 전송 순번. 새 관측 번호가 아니다. |
| `state.cis_state` | [`NetCisState`](../CIS/10_DATA.md#type-netcisstate) | CIS 원본 의미·품질 (Domain 중계 보존) | CIS_STATE: 0=STARTUP; 1=READY; 2=ACTIVE; 3=FAULT |
| `state.vision_status` | [`NetSensorFunctionState`](../../04_COMMON/10_DATA_AND_ID_RULES.md#type-netsensorfunctionstate) | CIS 원본 의미·품질 (Domain 중계 보존) | VISION_STATUS: 0=NOT_READY; 1=READY; 2=ACTIVE; 3=FAULT; 4=RECOVERING |
| `state.temperature_status` | [`NetSensorFunctionState`](../../04_COMMON/10_DATA_AND_ID_RULES.md#type-netsensorfunctionstate) | CIS 원본 의미·품질 (Domain 중계 보존) | TEMPERATURE_STATUS: 0=NOT_READY; 1=READY; 2=ACTIVE; 3=FAULT; 4=RECOVERING |
| `state.humidity_status` | [`NetSensorFunctionState`](../../04_COMMON/10_DATA_AND_ID_RULES.md#type-netsensorfunctionstate) | CIS 원본 의미·품질 (Domain 중계 보존) | HUMIDITY_STATUS: 0=NOT_READY; 1=READY; 2=ACTIVE; 3=FAULT; 4=RECOVERING |
| `state.illuminance_status` | [`NetSensorFunctionState`](../../04_COMMON/10_DATA_AND_ID_RULES.md#type-netsensorfunctionstate) | CIS 원본 의미·품질 (Domain 중계 보존) | ILLUMINANCE_STATUS: 0=NOT_READY; 1=READY; 2=ACTIVE; 3=FAULT; 4=RECOVERING |
| `state.rear_status` | [`NetSensorFunctionState`](../../04_COMMON/10_DATA_AND_ID_RULES.md#type-netsensorfunctionstate) | CIS 원본 의미·품질 (Domain 중계 보존) | REAR_STATUS: 0=NOT_READY; 1=READY; 2=ACTIVE; 3=FAULT; 4=RECOVERING |
| `state.pi_uart_status` | [`NetInterfaceState`](../CIS/10_DATA.md#type-netinterfacestate) | CIS 원본 의미·품질 (Domain 중계 보존) | PI_UART_STATUS: 0=READY; 1=UNAVAILABLE; 2=RECOVERING |
| `state.can_interface_status` | [`NetInterfaceState`](../CIS/10_DATA.md#type-netinterfacestate) | CIS 원본 의미·품질 (Domain 중계 보존) | CAN_INTERFACE_STATUS: 0=READY; 1=UNAVAILABLE; 2=RECOVERING |
| `state.fault_mask` | `uint16_t` | CIS 원본 의미·품질 (Domain 중계 보존) | FAULT_MASK: CIS 고장 비트 0~8 |
| `state.active_fault` | `uint16_t` | CIS 원본 의미·품질 (Domain 중계 보존) | ACTIVE_FAULT: 현재 주요 고장 코드 |
| `state.recovering` | [`NetFlag`](../../04_COMMON/10_DATA_AND_ID_RULES.md#type-netflag) | CIS 원본 의미·품질 (Domain 중계 보존) | RECOVERING: 0=아님; 1=복구 중 |
| `state.last_fault` | `uint16_t` | CIS 원본 의미·품질 (Domain 중계 보존) | LAST_FAULT: 이번 기동 이후 최근 주요 고장 코드 |
| `state.last_fault_occurrence` | `uint32_t` | CIS 원본 의미·품질 (Domain 중계 보존) | LAST_FAULT_OCCURRENCE: 최근 고장 발생 번호 |
| `state.affected_function` | `uint8_t` | CIS 원본 의미·품질 (Domain 중계 보존) | AFFECTED_FUNCTION: bit0=비전; bit1=온도; bit2=습도; bit3=조도; bit4=후방; bit5=제공 경로 |
| `state.update_sequence` | `uint32_t` | CIS 원본 의미·품질 (Domain 중계 보존) | UPDATE_SEQUENCE: 이번 상태 평가 번호 |
| `state.source_age_ms` | `uint16_t` | CIS 원본 의미·품질 (Domain 중계 보존) | SOURCE_AGE: 평가 후 ms |


<a id="type-netmobilewindowstate"></a>
### `NetMobileWindowState`

M_WINDOW_STATE. CAN 원 메시지의 출처와 의미 필드를 하향 문맥과 함께 보존한다.

**선언 종류:** 구조체

```c
typedef struct NetMobileWindowState
{
    NetDownContext context;
    NetCanSource source; /**< 원 CAN BOOT_ID/TX_SEQUENCE. Domain의 송신 번호로 바꾸지 않는다. */
    NetWindowState state; /**< 원 값·품질·갱신 번호를 보존하고 AGE는 누적한다. CAN 패딩은 포함하지 않는다. */
} NetMobileWindowState;
```

#### 필드 구성
| 필드 | 타입 | 소유자 | 의미·유효 조건 |
| --- | --- | --- | --- |
| `context` | [`NetDownContext`](14_MOBILE_CODEC_DATA.md#type-netdowncontext) | 현재 수신 대상 문맥 owner · Network 조립 | typed 의미 필드. 아래 범위와 UART-M의 같은 이름 필드 계약을 함께 적용한다. |
| `source` | [`NetCanSource`](../../04_COMMON/10_DATA_AND_ID_RULES.md#type-netcansource) | 원 CAN Network 출처 보존 | 원 CAN BOOT_ID/TX_SEQUENCE. Domain의 송신 번호로 바꾸지 않는다. |
| `state` | [`NetWindowState`](../WINDOW/10_DATA.md#type-netwindowstate) | WINDOW 원본 의미·품질 (Domain 중계 보존) | 원 값·품질·갱신 번호를 보존하고 AGE는 누적한다. CAN 패딩은 포함하지 않는다. |


#### 필요한 중첩 필드만 펼쳐보기
직접 멤버는 앞의 표를 사용한다. 아래 표는 nested context/source 안의 경로만 표시하며 별도 DTO나 raw 필드를 추가하지 않는다.
| 필드 | 타입 | 소유자 | 의미·유효 조건 |
| --- | --- | --- | --- |
| `context.recipient.device_context_id` | `uint32_t` | 현재 단말·Session owner · Network 검증 | ESP32 등록 단말 식별. 0 예약. |
| `context.recipient.session_id` | `uint64_t` | 현재 단말·Session owner · Network 검증 | 현재 Session. 생성 방식은 ESP32/MOBILE 담당. |
| `context.domain_boot_id` | `uint32_t` | Domain Network | 정보를 제공하는 현재 Domain 기동 번호. |
| `context.query_id` | `uint32_t` | 원 Query ID (보존) | 자발 전송은 0. 조회 응답은 원 QUERY_ID. |
| `source.boot_id` | `uint32_t` | 원 CAN Network (보존) | 송신 노드 기동 번호. 0 예약. |
| `source.tx_sequence` | `uint32_t` | 원 CAN Network (보존) | 같은 메시지의 전송 순번. 새 관측 번호가 아니다. |
| `state.motion_state` | [`NetWindowMotionState`](../WINDOW/10_DATA.md#type-netwindowmotionstate) | WINDOW 원본 의미·품질 (Domain 중계 보존) | MOTION_STATE: 0=STOPPED; 1=OPENING; 2=CLOSING; 3=ANTIPINCH_REVERSING; 255=UNKNOWN |
| `state.window_ecu_state` | [`NetControllerState`](../../04_COMMON/10_DATA_AND_ID_RULES.md#type-netcontrollerstate) | WINDOW 원본 의미·품질 (Domain 중계 보존) | WINDOW_ECU_STATE: 0=INIT; 1=READY; 2=DEGRADED; 3=FAULT |
| `state.position_closed_percent` | `uint8_t` | WINDOW 원본 의미·품질 (Domain 중계 보존) | WINDOW_POSITION: 0~100% 닫힘 비율,255=확인 불가 |
| `state.window_value_quality` | [`NetValueQuality`](../../04_COMMON/10_DATA_AND_ID_RULES.md#type-netvaluequality) | WINDOW 원본 의미·품질 (Domain 중계 보존) | WINDOW_VALUE_QUALITY: VALUE_QUALITY |
| `state.fully_open_state` | [`NetTriState`](../../04_COMMON/10_DATA_AND_ID_RULES.md#type-nettristate) | WINDOW 원본 의미·품질 (Domain 중계 보존) | FULLY_OPEN_STATE: 0=아님; 1=완전 열림; 255=확인 불가 |
| `state.fully_closed_state` | [`NetTriState`](../../04_COMMON/10_DATA_AND_ID_RULES.md#type-nettristate) | WINDOW 원본 의미·품질 (Domain 중계 보존) | FULLY_CLOSED_STATE: 0=아님; 1=완전 닫힘; 255=확인 불가 |
| `state.antipinch_protection_status` | [`NetAntiPinchProtectionState`](../WINDOW/10_DATA.md#type-netantipinchprotectionstate) | WINDOW 원본 의미·품질 (Domain 중계 보존) | ANTIPINCH_PROTECTION_STATUS: 0=대기; 1=진행; 2=완료; 3=중단 |
| `state.reverse_state` | [`NetTriState`](../../04_COMMON/10_DATA_AND_ID_RULES.md#type-nettristate) | WINDOW 원본 의미·품질 (Domain 중계 보존) | REVERSE_STATE: 0=역전 아님; 1=역전 중; 255=확인 불가 |
| `state.update_sequence` | `uint32_t` | WINDOW 원본 의미·품질 (Domain 중계 보존) | UPDATE_SEQUENCE: 새 상태 평가 |
| `state.source_age_ms` | `uint16_t` | WINDOW 원본 의미·품질 (Domain 중계 보존) | SOURCE_AGE: 평가 후 ms |


<a id="type-netmobilewindowfault"></a>
### `NetMobileWindowFault`

M_WINDOW_FAULT. CAN 원 메시지의 출처와 의미 필드를 하향 문맥과 함께 보존한다.

**선언 종류:** 구조체

```c
typedef struct NetMobileWindowFault
{
    NetDownContext context;
    NetCanSource source; /**< 원 CAN BOOT_ID/TX_SEQUENCE. Domain의 송신 번호로 바꾸지 않는다. */
    NetWindowFault state; /**< 원 값·품질·갱신 번호를 보존하고 AGE는 누적한다. CAN 패딩은 포함하지 않는다. */
} NetMobileWindowFault;
```

#### 필드 구성
| 필드 | 타입 | 소유자 | 의미·유효 조건 |
| --- | --- | --- | --- |
| `context` | [`NetDownContext`](14_MOBILE_CODEC_DATA.md#type-netdowncontext) | 현재 수신 대상 문맥 owner · Network 조립 | typed 의미 필드. 아래 범위와 UART-M의 같은 이름 필드 계약을 함께 적용한다. |
| `source` | [`NetCanSource`](../../04_COMMON/10_DATA_AND_ID_RULES.md#type-netcansource) | 원 CAN Network 출처 보존 | 원 CAN BOOT_ID/TX_SEQUENCE. Domain의 송신 번호로 바꾸지 않는다. |
| `state` | [`NetWindowFault`](../WINDOW/10_DATA.md#type-netwindowfault) | WINDOW 원본 의미·품질 (Domain 중계 보존) | 원 값·품질·갱신 번호를 보존하고 AGE는 누적한다. CAN 패딩은 포함하지 않는다. |


#### 필요한 중첩 필드만 펼쳐보기
직접 멤버는 앞의 표를 사용한다. 아래 표는 nested context/source 안의 경로만 표시하며 별도 DTO나 raw 필드를 추가하지 않는다.
| 필드 | 타입 | 소유자 | 의미·유효 조건 |
| --- | --- | --- | --- |
| `context.recipient.device_context_id` | `uint32_t` | 현재 단말·Session owner · Network 검증 | ESP32 등록 단말 식별. 0 예약. |
| `context.recipient.session_id` | `uint64_t` | 현재 단말·Session owner · Network 검증 | 현재 Session. 생성 방식은 ESP32/MOBILE 담당. |
| `context.domain_boot_id` | `uint32_t` | Domain Network | 정보를 제공하는 현재 Domain 기동 번호. |
| `context.query_id` | `uint32_t` | 원 Query ID (보존) | 자발 전송은 0. 조회 응답은 원 QUERY_ID. |
| `source.boot_id` | `uint32_t` | 원 CAN Network (보존) | 송신 노드 기동 번호. 0 예약. |
| `source.tx_sequence` | `uint32_t` | 원 CAN Network (보존) | 같은 메시지의 전송 순번. 새 관측 번호가 아니다. |
| `state.fault_category` | [`NetFaultCategory`](../../04_COMMON/10_DATA_AND_ID_RULES.md#type-netfaultcategory) | WINDOW 원본 의미·품질 (Domain 중계 보존) | FAULT_CATEGORY: 0=없음; 1=SENSOR; 2=FUNCTION; 3=COMM |
| `state.fault_code` | [`NetWindowFaultCode`](../WINDOW/10_DATA.md#type-netwindowfaultcode) | WINDOW 원본 의미·품질 (Domain 중계 보존) | FAULT_CODE: WINDOW 고장 코드 |
| `state.fault_status` | [`NetWindowFaultState`](../WINDOW/10_DATA.md#type-netwindowfaultstate) | WINDOW 원본 의미·품질 (Domain 중계 보존) | FAULT_STATUS: 0=없음; 1=현재 고장; 2=복구 중 |
| `state.occurrence_id` | `uint32_t` | WINDOW 원본 의미·품질 (Domain 중계 보존) | OCCURRENCE_ID: 현재/최근 고장 연결 |
| `state.update_sequence` | `uint32_t` | WINDOW 원본 의미·품질 (Domain 중계 보존) | UPDATE_SEQUENCE: 상태 평가 번호 |
| `state.source_age_ms` | `uint16_t` | WINDOW 원본 의미·품질 (Domain 중계 보존) | SOURCE_AGE: 평가 후 ms |


<a id="type-netmobilevssstatus"></a>
### `NetMobileVssStatus`

M_VSS_STATUS. CAN 원 메시지의 출처와 의미 필드를 하향 문맥과 함께 보존한다.

**선언 종류:** 구조체

```c
typedef struct NetMobileVssStatus
{
    NetDownContext context;
    NetCanSource source; /**< 원 CAN BOOT_ID/TX_SEQUENCE. Domain의 송신 번호로 바꾸지 않는다. */
    NetVssStatus state; /**< 원 값·품질·갱신 번호를 보존하고 AGE는 누적한다. CAN 패딩은 포함하지 않는다. */
} NetMobileVssStatus;
```

#### 필드 구성
| 필드 | 타입 | 소유자 | 의미·유효 조건 |
| --- | --- | --- | --- |
| `context` | [`NetDownContext`](14_MOBILE_CODEC_DATA.md#type-netdowncontext) | 현재 수신 대상 문맥 owner · Network 조립 | typed 의미 필드. 아래 범위와 UART-M의 같은 이름 필드 계약을 함께 적용한다. |
| `source` | [`NetCanSource`](../../04_COMMON/10_DATA_AND_ID_RULES.md#type-netcansource) | 원 CAN Network 출처 보존 | 원 CAN BOOT_ID/TX_SEQUENCE. Domain의 송신 번호로 바꾸지 않는다. |
| `state` | [`NetVssStatus`](../VSS/10_DATA.md#type-netvssstatus) | VSS 원본 의미·품질 (Domain 중계 보존) | 원 값·품질·갱신 번호를 보존하고 AGE는 누적한다. CAN 패딩은 포함하지 않는다. |


#### 필요한 중첩 필드만 펼쳐보기
직접 멤버는 앞의 표를 사용한다. 아래 표는 nested context/source 안의 경로만 표시하며 별도 DTO나 raw 필드를 추가하지 않는다.
| 필드 | 타입 | 소유자 | 의미·유효 조건 |
| --- | --- | --- | --- |
| `context.recipient.device_context_id` | `uint32_t` | 현재 단말·Session owner · Network 검증 | ESP32 등록 단말 식별. 0 예약. |
| `context.recipient.session_id` | `uint64_t` | 현재 단말·Session owner · Network 검증 | 현재 Session. 생성 방식은 ESP32/MOBILE 담당. |
| `context.domain_boot_id` | `uint32_t` | Domain Network | 정보를 제공하는 현재 Domain 기동 번호. |
| `context.query_id` | `uint32_t` | 원 Query ID (보존) | 자발 전송은 0. 조회 응답은 원 QUERY_ID. |
| `source.boot_id` | `uint32_t` | 원 CAN Network (보존) | 송신 노드 기동 번호. 0 예약. |
| `source.tx_sequence` | `uint32_t` | 원 CAN Network (보존) | 같은 메시지의 전송 순번. 새 관측 번호가 아니다. |
| `state.vss_state` | [`NetVssState`](../VSS/10_DATA.md#type-netvssstate) | VSS 원본 의미·품질 (Domain 중계 보존) | VSS_STATE: 0=STARTUP; 1=READY; 2=PLAYING; 3=FAULT |
| `state.vss_availability` | [`NetVssAvailability`](../VSS/10_DATA.md#type-netvssavailability) | VSS 원본 의미·품질 (Domain 중계 보존) | VSS_AVAILABILITY: 0=FULL; 1=DEGRADED; 2=UNAVAILABLE |
| `state.vss_accepting_events` | [`NetFlag`](../../04_COMMON/10_DATA_AND_ID_RULES.md#type-netflag) | VSS 원본 의미·품질 (Domain 중계 보존) | VSS_ACCEPTING_EVENTS: 0=불가; 1=수용 가능 |
| `state.vss_fault_active` | [`NetFlag`](../../04_COMMON/10_DATA_AND_ID_RULES.md#type-netflag) | VSS 원본 의미·품질 (Domain 중계 보존) | VSS_FAULT_ACTIVE: 0=없음; 1=현재 고장 있음 |
| `state.fault_mask` | `uint8_t` | VSS 원본 의미·품질 (Domain 중계 보존) | FAULT_MASK: VSS 고장 비트 0~4 |
| `state.vss_last_fault` | `uint16_t` | VSS 원본 의미·품질 (Domain 중계 보존) | VSS_LAST_FAULT: 이번 기동 이후 최근 주요 내부 출력 고장 |
| `state.last_fault_occurrence` | `uint32_t` | VSS 원본 의미·품질 (Domain 중계 보존) | LAST_FAULT_OCCURRENCE: 최근 고장 발생 번호 |
| `state.last_fault_age_ms` | `uint16_t` | VSS 원본 의미·품질 (Domain 중계 보존) | LAST_FAULT_AGE: 최근 고장 발생 후 ms; 포화 허용 |
| `state.last_fault_quality` | [`NetValueQuality`](../../04_COMMON/10_DATA_AND_ID_RULES.md#type-netvaluequality) | VSS 원본 의미·품질 (Domain 중계 보존) | LAST_FAULT_QUALITY: 0=기록 있음; 3=NO_DATA |
| `state.update_sequence` | `uint32_t` | VSS 원본 의미·품질 (Domain 중계 보존) | UPDATE_SEQUENCE: 이번 상태 평가 |
| `state.source_age_ms` | `uint16_t` | VSS 원본 의미·품질 (Domain 중계 보존) | SOURCE_AGE: 평가 후 ms |


<a id="type-netrequestkind"></a>
### `NetRequestKind`

MOBILE 사용자 요청 종류.

**표현:** `uint8_t` 별칭 + `enum NetRequestKindCode` 상수 집합. C 에서 `NetRequestKind` 자체는 `enum` 자료형이 아니다.

```c
typedef uint8_t NetRequestKind;
enum NetRequestKindCode
{
    NET_REQUEST_DOOR = 1,
    NET_REQUEST_CLIMATE = 2,
    NET_REQUEST_INTERIOR_LIGHT = 3,
    NET_REQUEST_DIGITAL_KEY = 4
};
```

#### 허용 코드/상수
| 코드 | 값 | 설명 |
| --- | ---: | --- |
| `NET_REQUEST_DOOR` | `1` | MOBILE 사용자 요청 종류. |
| `NET_REQUEST_CLIMATE` | `2` | MOBILE 사용자 요청 종류. |
| `NET_REQUEST_INTERIOR_LIGHT` | `3` | MOBILE 사용자 요청 종류. |
| `NET_REQUEST_DIGITAL_KEY` | `4` | MOBILE 사용자 요청 종류. |


<a id="type-netdooroperation"></a>
### `NetDoorOperation`

도어 사용자 요청 동작.

**표현:** `uint8_t` 별칭 + `enum NetDoorOperationCode` 상수 집합. C 에서 `NetDoorOperation` 자체는 `enum` 자료형이 아니다.

```c
typedef uint8_t NetDoorOperation;
enum NetDoorOperationCode
{
    NET_DOOR_OPERATION_LOCK = 0,
    NET_DOOR_OPERATION_UNLOCK = 1
};
```

#### 허용 코드/상수
| 코드 | 값 | 설명 |
| --- | ---: | --- |
| `NET_DOOR_OPERATION_LOCK` | `0` | 도어 사용자 요청 동작. |
| `NET_DOOR_OPERATION_UNLOCK` | `1` | 도어 사용자 요청 동작. |


<a id="type-netclimateoperation"></a>
### `NetClimateOperation`

공조 설정 요청 동작.

**표현:** `uint8_t` 별칭 + `enum NetClimateOperationCode` 상수 집합. C 에서 `NetClimateOperation` 자체는 `enum` 자료형이 아니다.

```c
typedef uint8_t NetClimateOperation;
enum NetClimateOperationCode
{
    NET_CLIMATE_OPERATION_TARGET_TEMPERATURE = 1,
    NET_CLIMATE_OPERATION_AUTO = 2,
    NET_CLIMATE_OPERATION_FAN = 3
};
```

#### 허용 코드/상수
| 코드 | 값 | 설명 |
| --- | ---: | --- |
| `NET_CLIMATE_OPERATION_TARGET_TEMPERATURE` | `1` | 공조 설정 요청 동작. |
| `NET_CLIMATE_OPERATION_AUTO` | `2` | 공조 설정 요청 동작. |
| `NET_CLIMATE_OPERATION_FAN` | `3` | 공조 설정 요청 동작. |


<a id="type-netlightoperation"></a>
### `NetLightOperation`

실내 조명 설정 요청 동작.

**표현:** `uint8_t` 별칭 + `enum NetLightOperationCode` 상수 집합. C 에서 `NetLightOperation` 자체는 `enum` 자료형이 아니다.

```c
typedef uint8_t NetLightOperation;
enum NetLightOperationCode
{
    NET_LIGHT_OPERATION_ENABLE = 1,
    NET_LIGHT_OPERATION_LEVEL = 2,
    NET_LIGHT_OPERATION_RGB = 3
};
```

#### 허용 코드/상수
| 코드 | 값 | 설명 |
| --- | ---: | --- |
| `NET_LIGHT_OPERATION_ENABLE` | `1` | 실내 조명 설정 요청 동작. |
| `NET_LIGHT_OPERATION_LEVEL` | `2` | 실내 조명 설정 요청 동작. |
| `NET_LIGHT_OPERATION_RGB` | `3` | 실내 조명 설정 요청 동작. |


<a id="type-netdigitalkeyoperation"></a>
### `NetDigitalKeyOperation`

디지털 키 자동 Unlock 설정 요청 동작.

**표현:** `uint8_t` 별칭 + `enum NetDigitalKeyOperationCode` 상수 집합. C 에서 `NetDigitalKeyOperation` 자체는 `enum` 자료형이 아니다.

```c
typedef uint8_t NetDigitalKeyOperation;
enum NetDigitalKeyOperationCode
{
    NET_DIGITAL_KEY_OPERATION_ENABLE = 1
};
```

#### 허용 코드/상수
| 코드 | 값 | 설명 |
| --- | ---: | --- |
| `NET_DIGITAL_KEY_OPERATION_ENABLE` | `1` | 디지털 키 자동 Unlock 설정 요청 동작. |


<a id="type-netqueryscope"></a>
### `NetQueryScope`

조회 범위.

**표현:** `uint8_t` 별칭 + `enum NetQueryScopeCode` 상수 집합. C 에서 `NetQueryScope` 자체는 `enum` 자료형이 아니다.

```c
typedef uint8_t NetQueryScope;
enum NetQueryScopeCode
{
    NET_QUERY_SCOPE_STATE = 0,
    NET_QUERY_SCOPE_RESULT = 1,
    NET_QUERY_SCOPE_WARNING = 2,
    NET_QUERY_SCOPE_ALL = 3
};
```

#### 허용 코드/상수
| 코드 | 값 | 설명 |
| --- | ---: | --- |
| `NET_QUERY_SCOPE_STATE` | `0` | 조회 범위. |
| `NET_QUERY_SCOPE_RESULT` | `1` | 조회 범위. |
| `NET_QUERY_SCOPE_WARNING` | `2` | 조회 범위. |
| `NET_QUERY_SCOPE_ALL` | `3` | 조회 범위. |


<a id="type-netquerystatus"></a>
### `NetQueryStatus`

조회 종료 상태. 차량 실행 결과와 구분한다.

**표현:** `uint8_t` 별칭 + `enum NetQueryStatusCode` 상수 집합. C 에서 `NetQueryStatus` 자체는 `enum` 자료형이 아니다.

```c
typedef uint8_t NetQueryStatus;
enum NetQueryStatusCode
{
    NET_QUERY_STATUS_DONE = 0,
    NET_QUERY_STATUS_NOT_FOUND = 1,
    NET_QUERY_STATUS_PARTIAL_UNCONFIRMED = 2,
    NET_QUERY_STATUS_CONTEXT_REJECTED = 3
};
```

#### 허용 코드/상수
| 코드 | 값 | 설명 |
| --- | ---: | --- |
| `NET_QUERY_STATUS_DONE` | `0` | 조회 종료 상태. 차량 실행 결과와 구분한다. |
| `NET_QUERY_STATUS_NOT_FOUND` | `1` | 조회 종료 상태. 차량 실행 결과와 구분한다. |
| `NET_QUERY_STATUS_PARTIAL_UNCONFIRMED` | `2` | 조회 종료 상태. 차량 실행 결과와 구분한다. |
| `NET_QUERY_STATUS_CONTEXT_REJECTED` | `3` | 조회 종료 상태. 차량 실행 결과와 구분한다. |


<a id="type-netcommandtarget"></a>
### `NetCommandTarget`

M_RESULT가 연결하는 반영·실행 대상.

**표현:** `uint8_t` 별칭 + `enum NetCommandTargetCode` 상수 집합. C 에서 `NetCommandTarget` 자체는 `enum` 자료형이 아니다.

```c
typedef uint8_t NetCommandTarget;
enum NetCommandTargetCode
{
    NET_COMMAND_TARGET_DOMAIN_SETTING = 0,
    NET_COMMAND_TARGET_BCM = 1,
    NET_COMMAND_TARGET_WINDOW = 2
};
```

#### 허용 코드/상수
| 코드 | 값 | 설명 |
| --- | ---: | --- |
| `NET_COMMAND_TARGET_DOMAIN_SETTING` | `0` | M_RESULT가 연결하는 반영·실행 대상. |
| `NET_COMMAND_TARGET_BCM` | `1` | M_RESULT가 연결하는 반영·실행 대상. |
| `NET_COMMAND_TARGET_WINDOW` | `2` | M_RESULT가 연결하는 반영·실행 대상. |



## App 공개 인자와 Codec owner 경계

| Codec 필드 | 원 소유/생성 | 사용·수명 |
|---|---|---|
| context.recipient.device_context_id/session_id | 현재 검증된 peer/Session owner | Gateway 실제 binding과 Domain currentcontext 대조. 원 과거 RequestSession과 별도. |
| context.domain_boot_id | Domain Network 기동 owner | source generation/Occurrence와 혼동하지 않음. |
| context.query_id | MOBILE 원 Query 또는 Network 자발출력 0 | sourceApp 함수에 query 공개인자를추가하지 않음. privatecontext 로만 채움. |
| request_id/request_session | 원 MOBILE Request | 재전달/결과조회/현재 Session 변경에도원값 유지. |
| command_id/related_command_id | Domain 기능결정 owner | 원 sourceID echo, Network 재발급 금지. |
| source.boot_id/tx_sequence | 원 CAN 송신 Network | 표시중계에서도 원값보존. 원 source의 UPDATE_SEQUENCE/AGE 와별개. |
| 각 update/vision sequence | 원 관측/평가 producer | 새원 관측에만증가, cache 전달/Query 로증가금지. |
| age/source_age/result_age | 원 관측/판정/명령/결과 producer | 원 AGE+각 Network 자체대기만한 번 누적, raw u16 포화. |

M_CONTEXT38B 제안과 legacy31B는 서로호환되지 않으며 양 끝 Codec 지원/version 협의 필요. Gateway source 품질 UNKNOWN, C2 warning UNCONFIRMED,availability UNSUPPORTED,일부 source ResultReason·simulated는 정확한 raw 표현이없어 영향받는 encode 차단/TEAM_DECISION_REQUIRED 다. 전체역변환/SDK/실보드/무선검증은 NOT_EXECUTED 다.

## Source projection의 부분 존재·실제 전이 보존 조건

| source 정보 | 현재 제안 raw 표현 | 안전한 처리 |
|---|---|---|
| `DomainIf_UserSettingsState_t.hasTargetTemperature` 등 존재 bool | 온도·공조/조명 enable·Fan의 미확인 sentinel은 일부 표현 가능하나 NORMAL 밝기/RGB의 독립 존재 여부를 온전히 표현하는 raw flag 없음 | 일부 설정만 확인된 source snapshot을 모두 확인된 값으로 바꾸지 않는다. 독립 존재 여부와 종합 VALUE_QUALITY 관계가 합의되지 않은 부분 설정은 **TEAM_DECISION_REQUIRED / 해당 projection hold**. |
| `DomainIf_DigitalKeyResult_t.hasVehicleResult` | `RESULT=255` 및 confirmation으로 없는 기록 표현 가능 | result 없는 상태를 DONE/FAILED로 합성하지 않는다. |
| `DomainIf_DigitalKeyResult_t.actualUnlockTransition` | 직접 bool 필드 없음. ALREADY_AT_TARGET 사유는 false 근거지만 전체 sourcebool↔원 raw result/reason의 동등성은 미증명 | 실제 전이와 동일목표 DONE이 구별되는 source 계약을 보존한다. raw로 동등하게 표현할 수 없는 source 조합은 **TEAM_DECISION_REQUIRED / 해당 result export hold**. 임의 추론으로 true 생성 금지. |


<a id="r2-partial-export"></a>
### PROPOSED — 기존 sentinel·결과 의미의 최소 export 규칙 (O21/O22)

**NETWORK_OWNED + CROSS_NODE_AGREEMENT; 원 설정/실제 전이는 FUNCTION_OWNER.** 원 C2 has flags와 bool을 삭제하거나 새 raw로 바꾸지 않는다.

- `M_USER_SETTINGS`: `hasTargetTemperature=false`이면 기존 `-32768`, `hasClimateAutoSetting=false`/`hasInteriorLightEnableSetting=false`이면 기존 `255`, `hasUserFanLevel=false`이면 기존 FAN UNKNOWN `255`를 사용한다. 존재=true인 실제0/OFF/RGB0은 그대로 정상 설정으로 구분한다. 실제 값 범위·원 meta 품질/AGE를 검사하며 미확인 값을 현재 유효 설정으로 표시하지 않는다.
- NORMAL 밝기/RGB는 독립 absent sentinel이 없으므로 **`hasNormalLightLevel`과 `hasNormalLightColor` 모두 true이고 현재 값의 원 품질을 표현할 수 있을 때만** 위 M_USER_SETTINGS composite export를 권장한다. 어느 하나 false이면 해당 설정 frame을 제출하지 않고 로컬 NOT_READY/source NOT_AVAILABLE, Query는 누락을 기존 STATUS2로 인계한다. 확인된 다른 설정은 C2 원본에 보존하며 다른 상태 메시지는 계속 제공한다. absent 밝기/RGB를0으로 채운 뒤 VALUE_QUALITY OK로 보내거나, 전체 NO_DATA로 보내고 일부 숫자만 유효하다고 추론시키는 안은 독립 존재를 표현하지 못하므로 제외한다.
- `M_DIGITAL_RESULT`: hasVehicleResult=false인 source는 기존 RESULT255/UNCONFIRMED 의미로 처리하며 확인되지 않은 result/reason을 읽어 DONE/FAILED를 만들지 않는다. 기록 자체 없음(outHasRecord=false)은 결과 조회의 원 기록 부재로 처리한다. 원 occurrence/relatedDoorCommandId/origin/confirmation/resultAge를 보존한다.
- 실제 전이 bool을 소비하는 경로의 권장안은 **결과 처리 완료와 잠금 해제 전이를 분리**하는 것이다. `DONE + ALREADY_AT_TARGET`은 이미 목표 상태였다는 결과이며 전이 성공 알림이 아니다. 이 프레임을 단순 결과로 소비하는 경로는 유지할 수 있지만, 현 Wire에 actualUnlockTransition이 없으므로 다른 DONE 조합도 true로 추론하지 않는다. 실제 전이 표시는 원 C2 bool과 BCM door evidence를 가진 기능 경로에서만 한다. MOBILE이 bool이 꼭 필요한 경우 그 전이 표시 export만 제한하고 별도 schema 합의 요청을 남긴다. 미표현 reason을 다른 reason으로 덮거나 frame 전체를 임의 성공으로 축약하지 않는다.

**확인 질문:** DOMAIN Settings/ESP32/MOBILE owner가 위 sentinel별 미확인과 밝기/RGB 미확인 시 frame 누락+Query 부분 표시를 채택하는가? MOBILE이 M_DIGITAL_RESULT를 '결과 완료'로 표시하면서 실제 전이 확정을 요구하지 않을 수 있는가? 요구한다면 어떤 기존 기능 evidence로 소비할 수 있는가? 새 presence/transition bit·DTO/Header 추가는 이번에 하지 않는다. 허용된 export 규칙은 `PROPOSED`; 실제 변환/BLE 표시 시험 `NOT_EXECUTED`, 미합의 경로는 해당 설정/전이 `BINDING_OPEN`이다.

기존 source 헤더와 Wire 계약을 유지하며 미표현 의미의 export만 제한한다.
