# ESP32 — Gateway schema dispatch와 Network private 경로

**PROPOSED_DESIGN / BINDING_OPEN.** 공개 API는 [기존 Gateway13 종](20_API.md)을 재사용한다. 세 개 MOBILE relay TX와 17 개 ordinary M_* RX마다 새로운 공개 함수를 만들지 않는다. 상세 typed schema를 제거하지 않고 기존 Interface Core/Transport port에 정확하게 연결한다.

[Data](10_DATA.md) · [공개 API](20_API.md) · [공통 API·실패](../../04_COMMON/20_API_USAGE_AND_FAILURE.md) · [메시지 48](../../05_MESSAGE_INDEX.md)

## 실제 caller·private 처리부터 찾기

| caller → consumer | 정확한 원형·규칙 |
|---|---|
| 세 Gateway 관측 Port → Context compose | [PublishContext 원형](#api-espnetpriv_publishcontext), [M_CONTEXT 생산자 연결](#context-compose) |
| BLE/UART Adapter ↔ 기존 Core/Transport | [23 TYPE·방향·Context registry](#relay-schema), [공개13 원형·실패](20_API.md) |
| UART DomainAlive → 링크 감독 | [OnDomainAlive 원형](#api-espnetpriv_ondomainalive); 일반 MOBILE 중계 금지 |
| UART QueryEnd → Query 감독 | [OnQueryEnd 원형](#api-espnetpriv_onqueryend), [수신·부분·종결](#r2-query-consumer); BLE 완료 연결은 BINDING_OPEN |

## 필요한 내부 함수 원형


<a id="api-espnetpriv_publishcontext"></a>
### `EspNetPriv_PublishContext()`

```c
NetSubmitResult EspNetPriv_PublishContext(const NetMobileContext *message);
```

**호출·사용:** Gateway3 종 원 관측 업데이트의 독립 근거를 보존해 M_CONTEXT compose. App 최종 wire DTO 생성 의무 제거.

| 인자 | 정확한 타입 | 수명·검증 |
|---|---|---|
| `message` | `const NetMobileContext *` | 검증된 M_CONTEXT 의미/Codec 객체. 호출 동안 borrowed, nested 원 ID/AGE/품질 유지. |

**반환:** Network의 로컬 인계 결과 4 종이다. 안정된 복사 또는 encode 전에는 OK를 반환하지 않는다. 공간 부족 시 기존 개별 항목을 덮어쓰지 않는다.

<a id="api-espnetpriv_ondomainalive"></a>
### `EspNetPriv_OnDomainAlive()`

```c
void EspNetPriv_OnDomainAlive(const NetMobileDomainAlive *message, const NetUartRxMeta *meta);
```

**호출·사용:** 링크 감독 private. BLE MOBILE 중계 금지.

| 인자 | 정확한 타입 | 수명·검증 |
|---|---|---|
| `message` | `const NetMobileDomainAlive *` | 검증된 M_DOMAIN_ALIVE 의미/Codec 객체. 호출 동안 borrowed, nested 원 ID/AGE/품질 유지. |
| `meta` | `const NetUartRxMeta *` | UART Network 수신시각/링크순번 근거. 원 Session/원 관측 순번과 별개. |

**반환:** void — Network/Gateway 내부 감독에 인계한다. 호출자에게 차량 결과를 생성해 반환하지 않는다. 필요한 감독 근거는 기존 감독 담당 모듈에 전달한다.

<a id="api-espnetpriv_onqueryend"></a>
### `EspNetPriv_OnQueryEnd()`

```c
void EspNetPriv_OnQueryEnd(const NetMobileQueryEnd *message, const NetUartRxMeta *meta);
```

**호출·사용:** Query 종결 관리 private. MOBILE 조회 UI 용 종결 projection은 별도 BLE schema OPEN; 차량 DONE으로 중계 금지.

| 인자 | 정확한 타입 | 수명·검증 |
|---|---|---|
| `message` | `const NetMobileQueryEnd *` | 검증된 M_QUERY_END 의미/Codec 객체. 호출 동안 borrowed, nested 원 ID/AGE/품질 유지. |
| `meta` | `const NetUartRxMeta *` | UART Network 수신시각/링크순번 근거. 원 Session/원 관측 순번과 별개. |

**반환:** void — Network/Gateway 내부 감독에 인계한다. 호출자에게 차량 결과를 생성해 반환하지 않는다. 필요한 감독 근거는 기존 감독 담당 모듈에 전달한다.

<a id="relay-schema"></a>
## 기존 RelayMessageView의 상세 schema registry

source `Gateway_LogicalMessageType_t`는 broad 논리 분류다. `Gateway_RelayMessageView_t.payload`를 검사할 때 아래 **논리 subtype → 실제 typed schema → 현재 context → 허용 API 방향**을 함께 확인한다. category 하나만으로 모든 상태를 같은 구조로 decode 하지 않는다.

**PROPOSED / NETWORK_OWNED + CROSS_NODE_AGREEMENT — 권장 Adapter binding:** 기존 `Gateway_RelayMessageView_t.payload` ByteView에 **현재 UART-M TYPE 1 B + 해당 TYPE의 명시적으로 직렬화한 논리 payload P B**를 보관하여 `length=P+1`로 구분한다. TYPE은 아래 기존 값만 재사용하며 신규 tag/category 숫자를 배정하지 않는다. 이는 Adapter 내부 호출 convention이다. UART-M의 TYPE·PAYLOAD_LENGTH·payload offset이나 BLE 전송 format/Opcode/UUID/MTU를 변경하지 않는다. `Gateway_Interface.h`는 payload를 transport framing을 제거한 논리 payload로 정의하고 구체 표현은 정하지 않았으므로, 현재 ByteView와 공개 13개 경계만 재사용한다.

수신 Adapter는 원 frame의 VERSION/방향/TYPE/정확한 P/CRC/필수 필드와 현재 peer·Session을 검증하고 typed decode한 뒤 위 논리 view를 구성한다. Core의 `OnMobileMessage`/`OnDomainMessage`와 Transport의 `SendToDomain`/`SendToMobile` 및 등록한 relay sender는 같은 registry/convention을 검사한다. UART 송신 Adapter는 논리 TYPE을 기존 frame TYPE으로 옮기고 P B만 기존 payload에 encode한다. BLE 송신 Adapter는 subtype별 의미를 기존 모바일 profile로 인계하며 이 내부 prefix를 BLE wire에 자동 복제하지 않는다. C 구조체 주소 cast/memcpy나 검증 없는 raw frame relay는 허용하지 않는다. 현재 ID ByteView(u32 Device/u64 Session/u32 Request)와 payload 문맥도 서로 일치해야 한다.

**Context의 존재·종류 검사:** `device_context_id`/`session_id` view는 recipient/상향 현재 Device(u32)/Session(u64)와 일치시킨다. `request_id` view는 **Request ID**이며 QueryID나 Occurrence를 대입하지 않는다. M_REQUEST/M_RESULT에는 해당 원 REQUEST_ID(u32)를 연결한다. M_QUERY에서 조회 대상 REQUEST_ID가 있으면 그 값과 일치시키고, 상태 조회처럼 대상 Request가 없으면 empty view(`data=NULL,length=0`)를 허용한다. QUERY_ID는 M_QUERY/M_* payload의 별도 조회 식별자로만 보존한다. M_WARNING_ACK·M_WARNING·상태·가용성·M_DIGITAL_RESULT 등 RequestID가 없는 schema의 request_id view도 empty이며 자동 occurrence를 Request로 꾸미지 않는다. M_RESULT의 과거 REQUEST_SESSION은 payload에 그대로 두고 context.session_id에는 현재 recipient Session을 쓴다. UART-M payload에 VEHICLE_ID가 없으므로 `vehicle_id`는 인증된 configured vehicle 문맥이 있을 때만 별도 검증하고 없으면 source의 empty-view 계약을 따른다. Wire payload에 vehicle 값을 신설하지 않는다. private ALIVE는 recipient context가 없고, private QueryEnd는 현재 recipient/Query context만 검사한다.

category 또는 payload 길이만으로 subtype을 추론하는 안은 42 B의 BCM_STATUS/WINDOW_STATE/WINDOW_FAULT 충돌과 DIGITAL_STATUS/RESULT의 공통 category 때문에 제외한다. 가변 전역 subtype나 `user_context`의 최신 tag에 의존하는 안도 비동기 수명·동시 호출에서 원 payload와 분리되므로 제외한다. 권장안은 공개 DTO/API를 추가하지 않으며 borrowed 수명·local OK 전 안정된 복사/동기 encode 조건을 유지한다. **ESP32 Core/두 Transport Adapter owner 확인 질문:** 위 ByteView convention과 context의 중복 일치 검사를 함께 채택할 수 있는가? 채택 전 다른 convention의 buffer를 묵시적으로 수용하지 않고 해당 subtype binding은 `BINDING_OPEN`이다. 이 권장안은 `PROPOSED`; 새 `AGREED/IMPLEMENTED/TESTED` 증거는 없다. 실제 Codec 시험 `NOT_EXECUTED`.

| UART-M subtype / TYPE | 방향 / 정확한 payload P | 기존 typed schema | 기존 Gateway category·실제 경계 / Session 검사 |
|---|---|---|---|
| `M_CONTEXT` / `0x01` | ESP32 → DOMAIN / 38 B | [`NetMobileContext`](../DOMAIN/14_MOBILE_CODEC_DATA.md#type-netmobilecontext) | PublishRegistrationConnection / PublishAppActivity / PublishProximity → EspNetPriv_PublishContext; 검증된 동일 현재 peer/Session 3성분 |
| `M_REQUEST` / `0x10` | ESP32 → DOMAIN / 24 B | [`NetMobileRequest`](../DOMAIN/14_MOBILE_CODEC_DATA.md#type-netmobilerequest) | `GATEWAY_LOGICAL_MSG_MOBILE_REQUEST`; OnMobileMessage → SendToDomain; 현재 등록 peer/Session 검사 |
| `M_QUERY` / `0x11` | ESP32 → DOMAIN / 32 B | [`NetMobileQuery`](../DOMAIN/14_MOBILE_CODEC_DATA.md#type-netmobilequery) | `GATEWAY_LOGICAL_MSG_STATE_QUERY`; OnMobileMessage → SendToDomain; 현재 등록 peer/Session 검사 |
| `M_WARNING_ACK` / `0x12` | ESP32 → DOMAIN / 21 B | [`NetMobileWarningAck`](../DOMAIN/14_MOBILE_CODEC_DATA.md#type-netmobilewarningack) | `GATEWAY_LOGICAL_MSG_WARNING_ACK`; OnMobileMessage → SendToDomain; 현재 등록 peer/Session 검사 |
| `M_DOMAIN_ALIVE` / `0x02` | DOMAIN → ESP32 / 12 B | [`NetMobileDomainAlive`](../DOMAIN/14_MOBILE_CODEC_DATA.md#type-netmobiledomainalive) | EspNetPriv_OnDomainAlive private; recipient Session 없음, 원 Domain Boot·감독 품질 검사; App relay 없음 |
| `M_RESULT` / `0x20` | DOMAIN → ESP32 / 44 B | [`NetMobileResult`](../DOMAIN/14_MOBILE_CODEC_DATA.md#type-netmobileresult) | `GATEWAY_LOGICAL_MSG_REQUEST_RESULT`; OnDomainMessage → SendToMobile; 현재 recipient Session/Domain Boot 검사 |
| `M_WARNING` / `0x21` | DOMAIN → ESP32 / 40 B | [`NetMobileWarning`](../DOMAIN/14_MOBILE_CODEC_DATA.md#type-netmobilewarning) | `GATEWAY_LOGICAL_MSG_WARNING`; OnDomainMessage → SendToMobile; 현재 recipient Session/Domain Boot 검사 |
| `M_AVAILABILITY` / `0x22` | DOMAIN → ESP32 / 62 B | [`NetMobileAvailability`](../DOMAIN/14_MOBILE_CODEC_DATA.md#type-netmobileavailability) | `GATEWAY_LOGICAL_MSG_FUNCTION_AVAILABILITY`; OnDomainMessage → SendToMobile; 현재 recipient Session/Domain Boot 검사 |
| `M_DIGITAL_STATUS` / `0x23` | DOMAIN → ESP32 / 30 B | [`NetMobileDigitalStatus`](../DOMAIN/14_MOBILE_CODEC_DATA.md#type-netmobiledigitalstatus) | `GATEWAY_LOGICAL_MSG_DIGITAL_KEY_STATE_RESULT`; OnDomainMessage → SendToMobile; 현재 recipient Session/Domain Boot 검사 |
| `M_DIGITAL_RESULT` / `0x24` | DOMAIN → ESP32 / 35 B | [`NetMobileDigitalResult`](../DOMAIN/14_MOBILE_CODEC_DATA.md#type-netmobiledigitalresult) | `GATEWAY_LOGICAL_MSG_DIGITAL_KEY_STATE_RESULT`; OnDomainMessage → SendToMobile; 현재 recipient Session/Domain Boot 검사 |
| `M_USER_SETTINGS` / `0x25` | DOMAIN → ESP32 / 36 B | [`NetMobileUserSettings`](../DOMAIN/14_MOBILE_CODEC_DATA.md#type-netmobileusersettings) | `GATEWAY_LOGICAL_MSG_VEHICLE_STATE`; OnDomainMessage → SendToMobile; 현재 recipient Session/Domain Boot 검사 |
| `M_QUERY_END` / `0x40` | DOMAIN → ESP32 / 24 B | [`NetMobileQueryEnd`](../DOMAIN/14_MOBILE_CODEC_DATA.md#type-netmobilequeryend) | EspNetPriv_OnQueryEnd private; 현재 recipient Session/Boot/QueryID/scope 검사; App 차량 결과 relay 없음 |
| `M_BCM_DOOR_STATE` / `0x30` | DOMAIN → ESP32 / 40 B | [`NetMobileBcmDoorState`](../DOMAIN/14_MOBILE_CODEC_DATA.md#type-netmobilebcmdoorstate) | `GATEWAY_LOGICAL_MSG_VEHICLE_STATE`; OnDomainMessage → SendToMobile; 현재 recipient Session/Domain Boot 검사 |
| `M_BCM_CLIMATE_STATE` / `0x31` | DOMAIN → ESP32 / 44 B | [`NetMobileBcmClimateState`](../DOMAIN/14_MOBILE_CODEC_DATA.md#type-netmobilebcmclimatestate) | `GATEWAY_LOGICAL_MSG_VEHICLE_STATE`; OnDomainMessage → SendToMobile; 현재 recipient Session/Domain Boot 검사 |
| `M_BCM_LIGHT_STATE` / `0x32` | DOMAIN → ESP32 / 45 B | [`NetMobileBcmLightState`](../DOMAIN/14_MOBILE_CODEC_DATA.md#type-netmobilebcmlightstate) | `GATEWAY_LOGICAL_MSG_VEHICLE_STATE`; OnDomainMessage → SendToMobile; 현재 recipient Session/Domain Boot 검사 |
| `M_BCM_STATUS` / `0x33` | DOMAIN → ESP32 / 42 B | [`NetMobileBcmStatus`](../DOMAIN/14_MOBILE_CODEC_DATA.md#type-netmobilebcmstatus) | `GATEWAY_LOGICAL_MSG_VEHICLE_STATE`; OnDomainMessage → SendToMobile; 현재 recipient Session/Domain Boot 검사 |
| `M_CIS_ENVIRONMENT` / `0x34` | DOMAIN → ESP32 / 60 B | [`NetMobileCisEnvironment`](../DOMAIN/14_MOBILE_CODEC_DATA.md#type-netmobilecisenvironment) | `GATEWAY_LOGICAL_MSG_VEHICLE_STATE`; OnDomainMessage → SendToMobile; 현재 recipient Session/Domain Boot 검사 |
| `M_CIS_OCCUPANT` / `0x35` | DOMAIN → ESP32 / 45 B | [`NetMobileCisOccupant`](../DOMAIN/14_MOBILE_CODEC_DATA.md#type-netmobilecisoccupant) | `GATEWAY_LOGICAL_MSG_VEHICLE_STATE`; OnDomainMessage → SendToMobile; 현재 recipient Session/Domain Boot 검사 |
| `M_CIS_REAR` / `0x36` | DOMAIN → ESP32 / 39 B | [`NetMobileCisRear`](../DOMAIN/14_MOBILE_CODEC_DATA.md#type-netmobilecisrear) | `GATEWAY_LOGICAL_MSG_VEHICLE_STATE`; OnDomainMessage → SendToMobile; 현재 recipient Session/Domain Boot 검사 |
| `M_CIS_STATUS` / `0x37` | DOMAIN → ESP32 / 54 B | [`NetMobileCisStatus`](../DOMAIN/14_MOBILE_CODEC_DATA.md#type-netmobilecisstatus) | `GATEWAY_LOGICAL_MSG_VEHICLE_STATE`; OnDomainMessage → SendToMobile; 현재 recipient Session/Domain Boot 검사 |
| `M_VSS_STATUS` / `0x3A` | DOMAIN → ESP32 / 48 B | [`NetMobileVssStatus`](../DOMAIN/14_MOBILE_CODEC_DATA.md#type-netmobilevssstatus) | `GATEWAY_LOGICAL_MSG_VEHICLE_STATE`; OnDomainMessage → SendToMobile; 현재 recipient Session/Domain Boot 검사 |
| `M_WINDOW_STATE` / `0x38` | DOMAIN → ESP32 / 42 B | [`NetMobileWindowState`](../DOMAIN/14_MOBILE_CODEC_DATA.md#type-netmobilewindowstate) | `GATEWAY_LOGICAL_MSG_VEHICLE_STATE`; OnDomainMessage → SendToMobile; 현재 recipient Session/Domain Boot 검사; [구현 보류 — 설계 유지] |
| `M_WINDOW_FAULT` / `0x39` | DOMAIN → ESP32 / 42 B | [`NetMobileWindowFault`](../DOMAIN/14_MOBILE_CODEC_DATA.md#type-netmobilewindowfault) | `GATEWAY_LOGICAL_MSG_VEHICLE_STATE`; OnDomainMessage → SendToMobile; 현재 recipient Session/Domain Boot 검사; [구현 보류 — 설계 유지] |

**PROPOSED 수신 순서:** framing 검증 → 위 23종 registry의 방향/정확한 P → schema 필수 enum·예약 byte·kind/operation/ARG → 현재 peer/Device/Session 및 필요한 Boot → 해당 기존 경계. 알려진 TYPE의 잘못된 길이/방향/context는 `INVALID_ARGUMENT`, 지원하지 않는 TYPE/convention/callback은 `UNSUPPORTED`, 준비되지 않은 현재 binding은 `NOT_READY`로 로컬 실패시킨다. 미지원/구 Session을 다른 정상 DTO에 대입하거나 liveness·새 실행 근거로 쓰지 않는다. M_REQUEST/M_QUERY/M_WARNING_ACK는 원 ID를 보존하고 Domain에서 현재 문맥을 다시 검증한다. M_RESULT의 과거 REQUEST_SESSION은 현재 recipient SESSION과 구분한다. M_CONTEXT는 일반 relay가 아니며 세 원 평가의 독립 sequence/AGE/quality를 유지한다.

### 각 schema의 유효성·원 owner·수명

| subtype 묶음 | 검사·보존·소비 |
|---|---|
| M_REQUEST | 검증된 현재 단말/Session과 일치, 원 MOBILE RequestId를 유지한다. request kind/operation/ARG를 C2의 8 종 Request union으로 전환하고 예약 코드·허용 범위를 검사한다. Network local OK와 Domain 차량 ACCEPTED를 구분한다. |
| M_QUERY | 원 QueryId/scope/현재 Session과 선택한 과거 Request 참조를 유지한다. 전송 함수는 Query 요청을 보낼 때 사용하며 Query 결과를 제출하는 함수가 아니다. |
| M_WARNING_ACK | 원 Domain Boot·경고 종류·Occurrence를 echo 한다. 현재 Session/단말과 대조하고 읽음 의도만 전달한다. 위험 해제를 생성하지 않는다. |
| M_RESULT | 현재 수신 대상 Session과 과거 request_session을 구분한다. hasVehicleResult/confirmation/commandId/origin/AGE를 보존하고 확인할 수 없는 기록을 FAILED로 바꾸지 않는다. |
| M_WARNING | C2 경고 종류↔Wire 코드 명시 변환, 원 occurrence/provenance/quality/AGE 유지. UNCONFIRMED에 대응하는 raw 코드가 없으면 CLEAR를 합성하지 않는다. |
| M_AVAILABILITY | 기능별 availability/reason. C2 UNSUPPORTED raw 부재이면 차단/팀 결정, AVAILABLE로 대체하지 않는다. |
| M_DIGITAL_STATUS/RESULT | 설정·가용성과 실제 자동 해제 결과를 구분한다. 실제 잠금 해제 전이가 없는 ALREADY_AT_TARGET+DONE을 전이 성공으로 만들지 않는다. |
| M_USER_SETTINGS | 확정된 설정과 필드 존재 여부·품질을 보존한다. 생산자 TTL·허용 온도 범위가 미정인 상태에서 임의 정상 값을 합성하지 않는다. |
| M_BCM_* / M_CIS_* / M_VSS_STATUS | 원 CAN header의 Boot/TxSeq와 의미 객체의 원 평가 번호/AGE를 구분한다. BCM/CIS/Pi/VSS 원 관측을 Domain의 새 관측으로 재라벨하지 않는다. |
| M_WINDOW_STATE/FAULT | 전체 typed 원 상태/Fault evidence 보존. [구현 보류 — 설계 유지]. |

<a id="context-compose"></a>
## M_CONTEXT — 기존 생산자 Port3 종→M_CONTEXT 내부 구성

```c
Gateway_Status_t Gateway_Interface_PublishRegistrationConnection(
    const Gateway_RegistrationConnectionUpdate_t *update);
Gateway_Status_t Gateway_Interface_PublishAppActivity(
    const Gateway_AppActivityUpdate_t *update);
Gateway_Status_t Gateway_Interface_PublishProximity(
    const Gateway_ProximityUpdate_t *update);
```

| source Transport callback | M_CONTEXT fields / ownership |
|---|---|
| `send_registration_to_domain` | 등록·연결 공동 state/quality/update_sequence/age. 기존 observation_revision 원 owner를 유지. |
| `send_app_activity_to_domain` | App 활성 state/독립 quality/sequence/age. source producer·TTL OPEN, 연결만으로 ACTIVE 만들지 않음. |
| `send_proximity_to_domain` | 근접 state/독립 validity/reason/quality/sequence/age. 근접 담당 모듈의 원 평가를 사용하며 생산자 TTL은 OPEN 이다. |

Transport Adapter는 한 검증된 현재 peer/Device/Session에 속한 세 component를 조립하여 `EspNetPriv_PublishContext()`로 제출한다. 존재하지 않는 component는 정상 값을 합성하지 않고 명시 no-data/unknown 계약을 적용한다. quality UNKNOWN의 raw 매핑이 없으므로 합의 전 해당 필드 encode는 차단하고 TEAM_DECISION_REQUIRED로 표시한다. View와 각 update snapshot은 호출이 끝날 때까지 유효해야 하며 local OK 전에 deep copy 또는 동기 encode를 확보한다. 같은 M_CONTEXT 반복은 세 원 평가 순번을 올리지 않고 AGE만 증가시킨다. UART LINK_SEQUENCE는 별도로 frame마다 증가한다.

## Alive·Query 종료는 Gateway private

| private subtype | 실제 처리 | 금지 |
|---|---|---|
| M_DOMAIN_ALIVE | `EspNetPriv_OnDomainAlive()`가 Domain Boot/준비·SessionReady·감독 시각을 Gateway 감독에 반영 | BLE App에 그대로 전달하거나 차량 기능 가용성으로 임의 변환 |
| M_QUERY_END | `EspNetPriv_OnQueryEnd()`가 같은 QueryId/scope/현재 recipient/Domain Boot·수신 응답 수·부분 상태를 Query 감독에 반영 | M_RESULT DONE 또는 차량 실행 완료로 전환 |

SESSION_READY=0인 최초 유효 Query도 Domain의 passive 검증 후 처리할 수 있다. Query 종료는 원 Request Session 변경이나 새 Session 생성의 근거가 아니다. End의 ITEM_COUNT는 아래 수신 수와 대조하며 queue 접수/BLE 수신 완료로 해석하지 않는다. MOBILE 조회 UI의 종료·부분 표시는 별도 BLE 논리 projection 합의가 필요하다.


<a id="r2-query-consumer"></a>
### PROPOSED — Query 수신·종결 판단 (O25/O12)

**NETWORK_OWNED + CROSS_NODE_AGREEMENT.** 활성 조회는 `(Device, 현재 Session, Domain Boot, QueryID, scope)`로 묶고, 종료 전 같은 조회의 실제 유효 수신 응답만 센다. `QUERY_ID=0` 자발 갱신·ALIVE·END·동일 원 frame의 중복은 제외한다. 응답별 품질·원 BOOT/TX/관측 순번/AGE와 과거 RequestSession은 유지한다. 한 조회를 원자적 차량 전체 최신 Snapshot으로 주장하지 않는다.

- End의 기존 `STATUS=0`은 DOMAIN의 해당 scope 처리가 누락/encode 불가/송신 실패 없이 끝났다는 **전송 목록 완료 후보**이다. ESP32는 현재 문맥·scope·Boot 일치, 유효 응답 수=`ITEM_COUNT`, 수신 reject/미확인 손실 없음까지 확인한 경우만 목록 완료로 기록한다. 각 값이 모두 fresh/정상이라는 뜻은 아니다.
- `STATUS=2`는 실제 도착한 독립 항목을 부분 자료로 유지하고 조회 전체 완료를 만들지 않는다. `ITEM_COUNT=0`이라도 확인 불가이면 부분이다. `STATUS=1`은 요청 결과 조회에서 owner가 해당 원 Request 기록 없음까지 확인한 경우이며 조회 0건과 구분한다. `STATUS=3`은 문맥 거부이고 차량 실행 실패가 아니다.
- End count 불일치·종료 frame 자체 실패·수신 reject·확인할 수 없는 순서/손실은 완료 보류 후 로컬 Query 만료/실패로 인계한다. 만료 기한 수치 및 BLE의 조회 완료/부분/실패 표현은 아직 합의되지 않았다. 이를 M_RESULT DONE/FAILED나 새 raw 값으로 바꾸지 않는다.
- 활성 Query 중 원 Session/Domain Boot가 바뀌면 이전 조회를 불완전 종료한다. 동일 종료를 다시 받으면 결과를 재발행하지 않는다. 종료·만료 후 도착한 End는 이미 닫힌 원 Query를 다시 열지 않는다. 다시 조회하려면 MOBILE가 새 QueryID를 생성한다. 단일 현재 recipient의 조회를 직렬 처리하는 안을 권장하며 중복/동시 조회 admission과 큐 구현은 Adapter owner binding이다.

**확인 질문:** DOMAIN/ESP32가 아래 [UART-M 종료 규칙](../../03_COMMUNICATION/20_UART_M.md#r2-query-contract)의 TX 완료 count·0건·부분 기준을 같이 채택하는가? ESP32가 End를 private에 남기면서 MOBILE에 조회 결과의 부분/미완료를 어떤 기존 BLE 논리 경계로 인계하는가? 실제 TX 완료/수신 dedup/큐 순서/BLE 결과 표현이 확인되기 전 해당 완료 binding은 `BINDING_OPEN`; 미합의 입력은 완전 완료로 표시하지 않는다. `PROPOSED`, 실제 Codec/무선 시험 `NOT_EXECUTED`.

## 실제 source API 호출 예 — 구현 시험 아님

```c
/* Trusted producer update objects already valid; views remain alive. */
Gateway_Status_t localContext =
    Gateway_Interface_PublishRegistrationConnection(&registrationUpdate);
Gateway_Status_t localRelay = Gateway_Interface_SendToDomain(&validatedRequest);
/* localRelay OK admits the local relay; it is not Domain ACCEPTED/vehicle DONE. */

/* A validated ordinary Domain schema; ALIVE/QUERY_END route privately. */
Gateway_Status_t ingested = Gateway_Interface_OnDomainMessage(&domainStateView);
```

**구현 상태:** BLE/SDK/Firmware/Codec/Board·인증/현재 Session binding·frame ordering·Query count/capacity/burst/MTU 시험은 NOT_EXECUTED다.
