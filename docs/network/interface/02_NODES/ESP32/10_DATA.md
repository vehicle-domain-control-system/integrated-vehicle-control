# ESP32 — Gateway 공개 Data와 typed schema 중계

**DECLARED_SOURCE / PROPOSED_DESIGN / BINDING_OPEN.** Gateway는 등록·연결·근접의 신뢰 근거를 생산/검증하고 차량 의미를 보존해 중계한다. 도어·팬·조명 실행 정책, 차량 위험·DONE 판정은 Domain/실행 ECU owner 다.

[개요](00_OVERVIEW.md) · [기존 Gateway API13 종](20_API.md) · [subtype 별 내부 경로](25_PRIVATE_ROUTING.md) · [UART-M](../../03_COMMUNICATION/20_UART_M.md)

| Data | 전체 선언·멤버 표 | 의미·owner |
|---|---|---|
| Gateway enum과 update quality/age/sequence | [Gateway 전체 의미 타입](11_GATEWAY_TYPES.md) | 등록·연결 공동/앱 활성/근접의 서로 다른 원 producer |
| ByteView·MessageContext·RelayMessage·Publish update·callback tables | [Gateway Interface 전체 타입](12_GATEWAY_INTERFACE_TYPES.md) | borrowed view/API contract; local transport framing 아님 |
| M_* subtype 별 완전한 typed schema | [공유 Mobile Codec 타입](../DOMAIN/14_MOBILE_CODEC_DATA.md) | Network-private encode/decode 모델. broad category 안에 payload schema 분기 유지 |

<a id="gateway-views"></a>
## 핵심 view — 표현과 소유를 분리

```c
typedef struct
{
    const uint8_t *data;
    size_t length;
} Gateway_ByteView_t;

typedef struct
{
    Gateway_ByteView_t vehicle_id;
    Gateway_ByteView_t device_context_id;
    Gateway_ByteView_t session_id;
    Gateway_ByteView_t request_id;
} Gateway_MessageContextView_t;

typedef struct
{
    Gateway_LogicalMessageType_t type;
    Gateway_MessageDirection_t direction;
    Gateway_MessageContextView_t context;
    Gateway_ByteView_t payload;
} Gateway_RelayMessageView_t;
```

| 항목 | exact 타입/폭 | 수명·검사·owner |
|---|---|---|
| `data`/`length` | `const uint8_t *` / `size_t` | borrowed, 반환까지 유효. zero length↔NULL, nonzero↔non-NULL. 비동기 보관은 Network-owned deep 복사/encode 필요. |
| Device view | `Gateway_ByteView_t` → UART u32 | 등록 확인한 실제 peer와 현재 BLE context를 검사. arbitrary byte view를 C2 u32로 cast 금지. |
| Session view | `Gateway_ByteView_t` → UART u64 | 검증된 활성 연결 문맥. ID 발급/재연결 교체 방식은 TEAM_DECISION_REQUIRED; DeviceRegistrationManager가 Session을 만들지 않음. |
| Request view | `Gateway_ByteView_t` → UART u32 | 원 MOBILE Request, 재시도에서도 원값 유지. QueryID는 별도 payload 필드이며 조회 대상 Request가 없는 schema는 empty view다. [존재·종류 검사](25_PRIVATE_ROUTING.md#relay-schema). |
| Vehicle view | `Gateway_ByteView_t` | 원 header의 opaque view 유지. UART-M에 직접 VehicleID 필드 없으므로 logical binding 에서 소비, 새 raw 필드 추가 금지. |
| `type`/`direction` | 실제 C enum | broad 논리 category 일 뿐 UART TYPE/byte code 아님. 상세 schema와 허용 방향 확인 필수. |
| `payload` | `Gateway_ByteView_t` | transport framing 제거한 논리 payload. 알려진 subtype typed schema를 가리키는 제한적 view; 알 수 없는 schema 중계 금지. |

**PROPOSED / BINDING_OPEN:** broad `VEHICLE_STATE`나 길이만으로 schema를 고르지 않는다. [23 TYPE registry와 TYPE+P ByteView convention](25_PRIVATE_ROUTING.md#relay-schema)이 상세 표현·방향·Context 검사의 정본이다. 이는 내부 Adapter 호출 규약이며 새 UART/BLE Wire나 C 구조체 메모리 복사가 아니다. Core/두 Transport Adapter 합의 전 해당 subtype 중계는 hold다.

<a id="context-updates"></a>
## 독립 update tuple과 M_CONTEXT 합성

| 생산자 | 기존 typed update | 원 품질/AGE/Sequence | 금지 |
|---|---|---|---|
| ESP32 등록·연결 | `Gateway_RegistrationConnectionUpdate_t` | ONE shared update tuple; DeviceRegistrationManager.observation_revision 근거 | 등록/연결을 각기 새 counter로 분리하거나 BLE CONNECTED 만으로 REGISTERED 합성 |
| MOBILE App 활성→ESP32 검증 | `Gateway_AppActivityUpdate_t` | 독립 App source AGE/update sequence/quality; producer TTL OPEN | 다른 관측 갱신 때 ACTIVE fresh 합성 |
| ESP32 근접 | `Gateway_ProximityUpdate_t` | 독립 Proximity observation revision/age/quality | disconnected·STALE·invalid를 FAR로 바꿈 |

캐시 재전달은 원 update_sequence를 유지하고 elapsed AGE만 증가한다. Network UART LINK_SEQUENCE는 frame마다 별도 증가한다. M_CONTEXT v2 payload38B는 등록·연결 공동 근거 7B를 추가한 제안이며 기존 31B와 호환되지 않는다. UNKNOWN quality는 Wire에 exact code가 없으므로 원 NO_DATA 여부에 대한 합의 없이 VALID/OK로 보내지 않는다.

MOBILE Request/Query/WarningAck의 device/session은 모바일 payload의 주장만 신뢰하지 않는다. 현재 등록 peer/검증된 BLE 연결과 대조해 Domain으로 보내며 원 RequestID/QueryID/읽음 대상 Domain Boot+Occurrence를 보존한다. App 읽음은 위험 해제 의미가 아니다. UUID/MTU/GATT/분할 및 활성 Session owner는 [BLE 결정 항목](../../03_COMMUNICATION/40_BLE.md)을 확인한다.
