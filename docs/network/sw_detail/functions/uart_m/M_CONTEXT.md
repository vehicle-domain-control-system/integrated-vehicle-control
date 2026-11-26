# M_CONTEXT — ESP32 → DOMAIN

`WIRE-P02-M_CONTEXT` / `RX-P02-M_CONTEXT-DOMAIN`. VERSION2·TYPE `0x01`·payload **38B**·frame **48B** 후보다. CF01과 양단 layout 채택 증거가 없어 현재 정상 소비는 `BLOCKED_BY_CONTRACT`이며 문서상 조건부 경로를 상세화한다. 공개 API·Wire 변경·Production 승인으로 해석하지 않는다.

[UART-M 개요](../../05_UART_M_OVERVIEW.md) · [L04 Codec](../../layers/L04_FRAME_CODEC.md) · [M06](../../modules/M06_UART_CODEC.md) · [M02](../../modules/M02_QUERY_CONTROL.md) · [전역 flow](../../flows/UART_M_UPLINK.md)

<a id="data-api"></a>
## Data와 기존 API 경계

DTO 정본: [NetMobileContext](../../data/UART_M_DATA.md#type-NetMobileContext). RX meta: [NetUartRxMeta](../../data/UART_M_DATA.md#type-NetUartRxMeta). 공통 사본·Frame 타입: [UART Frame Data](../../data/UART_FRAME_DATA.md). C struct padding/enum 저장 폭/sizeof는 Wire가 아니며 아래 바이트 표로만 직렬화한다.

| 기존 정확한 원형 | 본체 소유/역할 | 봉인 S4A 출처 |
|---|---|---|
| `NetSubmitResult EspNetPriv_PublishContext(const NetMobileContext *message);` | ESP32 Network | `api/esp32.md#api-EspNetPriv_PublishContext` |
| `DomainIf_Status_t DomainNetPriv_OnGatewayContext(const NetMobileContext *message, const NetUartRxMeta *meta);` | DOMAIN Network | `api/domain_private.md#api-DomainNetPriv_OnGatewayContext` |
| `DomainIf_Status_t DomainIf_RxDigitalKeyConnection( DeviceContextId_t deviceContextId, const DigitalKeyConnectionState_t *connection);` | DOMAIN 공개 본체; Network는 호출·변환 | `api/domain_rx.md#api-DomainIf_RxDigitalKeyConnection` |
| `DomainIf_Status_t DomainIf_RxProximity( DeviceContextId_t deviceContextId, const ProximityInput_t *proximity);` | DOMAIN 공개 본체; Network는 호출·변환 | `api/domain_rx.md#api-DomainIf_RxProximity` |

송신 입력은 호출 동안 borrowed다. 로컬 성공 반환 전에 원 identity·필드·품질·시간 근거 전체를 독립 사본/Frame으로 확보한다. Decode의 `frame`은 호출 동안 안정된 입력, `out/meta`는 NULL 불가·서로 겹치지 않는 호출자 저장소다. 실패 OUT은 변경하지 않는다. 송신 out은 caller 소유이며 Driver 전달 이후 재사용·AGE 수정은 참조 해제 증거 전 금지한다. DOM C2는 Task/Owner 문맥, ESP32의 실제 Task/Superloop는 Owner 바인딩 전이다. ISR에서 codec·C2를 실행하지 않는다.

<a id="wire"></a>
## 정확한 payload 배치

payload B0은 Frame B8이다. 공통 envelope A5 5A·VERSION·TYPE·P·LINK_SEQUENCE·CRC는 [Frame 정본](../UART_FRAME_STREAM.md#check-frame)을 따른다. CRC-16/CCITT-FALSE는 Frame B2부터 payload 끝까지 P+6B에 적용하며 CRC 값은 LE다. 실패한 CRC나 정확한 P 불일치는 typed publish 이전에 거부한다.

| Payload byte / 폭 | 필드 → typed 멤버 | 부호·순서·범위/단위 | sentinel·예약·근거 |
|---|---|---|---|
| B0 / 4 | `DEVICE_CONTEXT_ID` → `context.device_context_id` | u32 / LE; 현재 단말0예약 | 0 예약; T34 L123 |
| B4 / 8 | `SESSION_ID` → `context.session_id` | u64 / LE; 현재 Session; 발급/0사용규칙 OPEN | 특수값 추가 정의 없음; T34 L123 |
| B12 / 1 | `DEVICE_REGISTRATION_STATE` → `device_registration_state` | u8 / N/A; 0=NOT_REGISTERED; 1=REGISTERED; 255=UNKNOWN; 0=NET_TRISTATE_NO, 1=NET_TRISTATE_YES, 255=NET_TRISTATE_UNKNOWN | 255=UNKNOWN; T34 L124 |
| B13 / 1 | `BT_CONNECTION_STATE` → `bt_connection_state` | u8 / N/A; 0=DISCONNECTED; 1=CONNECTED; 255=UNKNOWN; 0=NET_TRISTATE_NO, 1=NET_TRISTATE_YES, 255=NET_TRISTATE_UNKNOWN | 255=UNKNOWN; T34 L125 |
| B14 / 1 | `APP_ACTIVE_STATE` → `app_active_state` | u8 / N/A; 0=INACTIVE; 1=ACTIVE; 255=UNKNOWN; 0=NET_TRISTATE_NO, 1=NET_TRISTATE_YES, 255=NET_TRISTATE_UNKNOWN | 255=UNKNOWN; T34 L126 |
| B15 / 1 | `APP_QUALITY` → `app_quality` | u8 / N/A; VALUE_QUALITY; 0=NET_QUALITY_OK, 1=NET_QUALITY_STALE, 2=NET_QUALITY_INVALID, 3=NET_QUALITY_NO_DATA | 특수값 추가 정의 없음; T34 L127 |
| B16 / 4 | `APP_UPDATE_SEQUENCE` → `app_update_sequence` | u32 / LE; App의 새 활성 정보 번호 | 특수값 추가 정의 없음; T34 L128 |
| B20 / 2 | `APP_AGE` → `app_age_ms` | u16 / LE; App 활성 정보 후 ms | 65535 포화 하한; 새 관측/최신성 증거 아님; T34 L129 |
| B22 / 1 | `PROXIMITY_STATE` → `proximity_state` | u8 / N/A; 0=FAR; 1=NEAR; 255=UNKNOWN; 0=NET_TRISTATE_NO, 1=NET_TRISTATE_YES, 255=NET_TRISTATE_UNKNOWN | 255=UNKNOWN; T34 L130 |
| B23 / 1 | `PROXIMITY_VALIDITY` → `proximity_validity` | u8 / N/A; 0=INVALID; 1=VALID; 0=NET_VALIDITY_INVALID, 1=NET_VALIDITY_VALID | 특수값 추가 정의 없음; T34 L131 |
| B24 / 1 | `PROXIMITY_REASON` → `proximity_reason` | u8 / N/A; 품질 사유; 0=NET_QUALITY_REASON_NONE, 1=NET_QUALITY_REASON_NOT_READY, 2=NET_QUALITY_REASON_OUT_OF_RANGE, 3=NET_QUALITY_REASON_SENSOR_FAULT, 4=NET_QUALITY_REASON_VISION_FAULT, 5=NET_QUALITY_REASON_STALE, 6=NET_QUALITY_REASON_NO_DATA, 7=NET_QUALITY_REASON_COMMUNICATION_FAULT, 8=NET_QUALITY_REASON_RECOVERING, 9=NET_QUALITY_REASON_DATA_INVALID | 특수값 추가 정의 없음; T34 L132 |
| B25 / 4 | `PROXIMITY_UPDATE_SEQUENCE` → `proximity_update_sequence` | u32 / LE; ESP32 새 RSSI 평가 번호 | 특수값 추가 정의 없음; T34 L133 |
| B29 / 2 | `PROXIMITY_AGE` → `proximity_age_ms` | u16 / LE; RSSI 평가 후 ms | 65535 포화 하한; 새 관측/최신성 증거 아님; T34 L134 |
| B31 / 1 | `REGISTRATION_CONNECTION_QUALITY` → `registration_connection_quality` | u8 / N/A; 등록·현재 연결 coherent context의 공유 VALUE_QUALITY; 0=NET_QUALITY_OK, 1=NET_QUALITY_STALE, 2=NET_QUALITY_INVALID, 3=NET_QUALITY_NO_DATA | 특수값 추가 정의 없음; T34 L135 |
| B32 / 4 | `REGISTRATION_CONNECTION_UPDATE_SEQUENCE` → `registration_connection_update_sequence` | u32 / LE; 기존 owner의 shared observation revision을 반영한 실제 공동 평가 순서 | 특수값 추가 정의 없음; T34 L136 |
| B36 / 2 | `REGISTRATION_CONNECTION_AGE` → `registration_connection_age_ms` | u16 / LE; 같은 등록·현재 peer/연결 평가 이후 누적 ms | 65535 포화 하한; 새 관측/최신성 증거 아님; T34 L137 |

이 메시지의 세 u16 AGE65535는 포화 하한이며 정확한 시간·fresh 증거가 아니다. 동일 원 관측의 반복은 각 평가번호를 보존하고 미반영 경과만 한 번 누적한다. sequence wrap·clock reset·Session0의 실제 수용 정책이 불명확하면 해당 admission을 HOLD로 남긴다. 정의된 UNKNOWN/INVALID/STALE/NO_DATA는 malformed와 다르다.

<a id="codec"></a>
## Typed encode/decode — M06 F17/F18

아래 함수는 `PROPOSED_PRIVATE_FUNCTION`이다. Encoder 호출자는 M04에서 확정한 해당 방향 LINK_SEQUENCE를 제공하고 Codec은 번호·시각·Session을 발급하지 않는다. Decoder 성공은 형식 DTO 확보뿐이며 현재 문맥/순서 및 소비자 성공은 [Router](../UART_M_ROUTER.md)에서 별도로 확인한다. 공통 [LE 함수](../UART_FRAME_STREAM.md#le-fields)는 명시된 각 offset에서 2/4/8B만 읽거나 쓴다.

```c
NetUartMResult NetUartM_Decode_M_CONTEXT(const NetUartMFrame *frame,
    NetUartMProfile profile, NetMobileContext *out, NetUartRxMeta *meta)
{
    if (frame == NULL || out == NULL || meta == NULL) return NET_UART_M_INVALID_ARGUMENT;
    NetUartMResult r = NetUartM_CheckFrame(frame, NET_UART_M_UPLINK, 0x01, profile);
    if (r != NET_UART_M_OK) return r;
    const uint8_t *p = frame->bytes + 8; // 검증된 payload 시작; Frame offset은 payload+8이다.
    NetMobileContext tmp = {0};
    tmp.context.device_context_id = NetUartM_ReadLe32(p + 0);
    tmp.context.session_id = NetUartM_ReadLe64(p + 4);
    tmp.device_registration_state = p[12];
    tmp.bt_connection_state = p[13];
    tmp.app_active_state = p[14];
    tmp.app_quality = p[15];
    tmp.app_update_sequence = NetUartM_ReadLe32(p + 16);
    tmp.app_age_ms = NetUartM_ReadLe16(p + 20);
    tmp.proximity_state = p[22];
    tmp.proximity_validity = p[23];
    tmp.proximity_reason = p[24];
    tmp.proximity_update_sequence = NetUartM_ReadLe32(p + 25);
    tmp.proximity_age_ms = NetUartM_ReadLe16(p + 29);
    tmp.registration_connection_quality = p[31];
    tmp.registration_connection_update_sequence = NetUartM_ReadLe32(p + 32);
    tmp.registration_connection_age_ms = NetUartM_ReadLe16(p + 36);
    if (tmp.context.device_context_id == 0) return NET_UART_M_MALFORMED;
    if (tmp.device_registration_state != 0 && tmp.device_registration_state != 1 && tmp.device_registration_state != 255) return NET_UART_M_MALFORMED;
    if (tmp.bt_connection_state != 0 && tmp.bt_connection_state != 1 && tmp.bt_connection_state != 255) return NET_UART_M_MALFORMED;
    if (tmp.app_active_state != 0 && tmp.app_active_state != 1 && tmp.app_active_state != 255) return NET_UART_M_MALFORMED;
    if (tmp.app_quality != 0 && tmp.app_quality != 1 && tmp.app_quality != 2 && tmp.app_quality != 3) return NET_UART_M_MALFORMED;
    if (tmp.proximity_state != 0 && tmp.proximity_state != 1 && tmp.proximity_state != 255) return NET_UART_M_MALFORMED;
    if (tmp.proximity_validity != 0 && tmp.proximity_validity != 1) return NET_UART_M_MALFORMED;
    if (tmp.proximity_reason != 0 && tmp.proximity_reason != 1 && tmp.proximity_reason != 2 && tmp.proximity_reason != 3 && tmp.proximity_reason != 4 && tmp.proximity_reason != 5 && tmp.proximity_reason != 6 && tmp.proximity_reason != 7 && tmp.proximity_reason != 8 && tmp.proximity_reason != 9) return NET_UART_M_MALFORMED;
    if (tmp.registration_connection_quality != 0 && tmp.registration_connection_quality != 1 && tmp.registration_connection_quality != 2 && tmp.registration_connection_quality != 3) return NET_UART_M_MALFORMED;
    NetUartRxMeta tmpMeta;
    tmpMeta.link_sequence = NetUartM_ReadLe16(frame->bytes + 6);
    tmpMeta.received_at_ms = frame->receivedAtMs; // callback 현재시각으로 대체하지 않는다.
    *out = tmp; *meta = tmpMeta; // 전 검사 후 동시 논리 commit; 실패 시 두 OUT 불변.
    return NET_UART_M_OK;
}
```

```c
NetUartMResult NetUartM_Encode_M_CONTEXT(const NetMobileContext *message,
    uint16_t linkSequence, NetUartMProfile profile, NetUartMFrame *out)
{
    if (message == NULL || out == NULL) return NET_UART_M_INVALID_ARGUMENT;
    if (message->context.device_context_id == 0) return NET_UART_M_INVALID_ARGUMENT;
    if (message->device_registration_state != 0 && message->device_registration_state != 1 && message->device_registration_state != 255) return NET_UART_M_INVALID_ARGUMENT;
    if (message->bt_connection_state != 0 && message->bt_connection_state != 1 && message->bt_connection_state != 255) return NET_UART_M_INVALID_ARGUMENT;
    if (message->app_active_state != 0 && message->app_active_state != 1 && message->app_active_state != 255) return NET_UART_M_INVALID_ARGUMENT;
    if (message->app_quality != 0 && message->app_quality != 1 && message->app_quality != 2 && message->app_quality != 3) return NET_UART_M_INVALID_ARGUMENT;
    if (message->proximity_state != 0 && message->proximity_state != 1 && message->proximity_state != 255) return NET_UART_M_INVALID_ARGUMENT;
    if (message->proximity_validity != 0 && message->proximity_validity != 1) return NET_UART_M_INVALID_ARGUMENT;
    if (message->proximity_reason != 0 && message->proximity_reason != 1 && message->proximity_reason != 2 && message->proximity_reason != 3 && message->proximity_reason != 4 && message->proximity_reason != 5 && message->proximity_reason != 6 && message->proximity_reason != 7 && message->proximity_reason != 8 && message->proximity_reason != 9) return NET_UART_M_INVALID_ARGUMENT;
    if (message->registration_connection_quality != 0 && message->registration_connection_quality != 1 && message->registration_connection_quality != 2 && message->registration_connection_quality != 3) return NET_UART_M_INVALID_ARGUMENT;
    uint8_t p[38] = {0}; // 호출 중에만 쓰는 임시 사본; 제품 queue 크기 제안 아님.
    NetUartM_WriteLe32(p + 0, message->context.device_context_id);
    NetUartM_WriteLe64(p + 4, message->context.session_id);
    p[12] = message->device_registration_state;
    p[13] = message->bt_connection_state;
    p[14] = message->app_active_state;
    p[15] = message->app_quality;
    NetUartM_WriteLe32(p + 16, message->app_update_sequence);
    NetUartM_WriteLe16(p + 20, message->app_age_ms);
    p[22] = message->proximity_state;
    p[23] = message->proximity_validity;
    p[24] = message->proximity_reason;
    NetUartM_WriteLe32(p + 25, message->proximity_update_sequence);
    NetUartM_WriteLe16(p + 29, message->proximity_age_ms);
    p[31] = message->registration_connection_quality;
    NetUartM_WriteLe32(p + 32, message->registration_connection_update_sequence);
    NetUartM_WriteLe16(p + 36, message->registration_connection_age_ms);
    return NetUartM_BuildFrame(p, 38, 0x01, linkSequence,
                                NET_UART_M_UPLINK, profile, out);
}
```

<a id="admission"></a>
## 현재 문맥과 C2 인계

ESP32의 기존 `Gateway_Interface_PublishRegistrationConnection`, `PublishAppActivity`, `PublishProximity` 입력은 서로 다른 원 관측이다. 등록·현재 peer/연결은 공동 품질·번호·AGE 한 벌이고 App과 근접은 독립이다. 동일 Device/현재 Session의 근거가 없으면 세 입력을 합성하지 않는다. Gateway callback의 중첩 ByteView도 성공 반환 전에 사본을 확보한다. ESP32 내부 BLE/등록 구현은 ESP32 Owner 영역이며 이 문서는 구현하지 않는다.

수신 Network의 기존 `DomainNetPriv_OnGatewayContext`는 C2의 `DomainIf_RxDigitalKeyConnection`과 `DomainIf_RxProximity`를 호출한다. 두 호출의 부분 반영·재시도·원자성은 미합의다. 같은 입력의 coherent 문맥을 소비한다는 근거와 부분 실패 처리 계약이 없으면 호출 전에 `DOMAIN_IF_NOT_AVAILABLE`로 제한한다. 첫 호출이 반영된 뒤 둘째가 실패했다면 첫 호출을 rollback했다고 주장하거나 두 호출을 통째로 반복하지 않는다. C2 성공과 Manager commit 증거는 별개다.

| Raw | registration | btConnection | appActive | proximity.state |
|---:|---|---|---|---|
| 0 | DEVICE_REGISTRATION_NOT_REGISTERED | BT_CONNECTION_DISCONNECTED | APP_ACTIVE_INACTIVE | PROXIMITY_FAR |
| 1 | DEVICE_REGISTRATION_REGISTERED | BT_CONNECTION_CONNECTED | APP_ACTIVE_ACTIVE | PROXIMITY_NEAR |
| 255 | DEVICE_REGISTRATION_UNKNOWN | BT_CONNECTION_UNKNOWN | APP_ACTIVE_UNKNOWN | PROXIMITY_UNKNOWN |

Raw 숫자를 C2 enum으로 cast하지 않는다. `SignalMeta_t`의 `quality/ageMs/updateSequence`는 등록·연결 B31/36/32, App B15/20/16에 각각 대응한다. NetValueQuality 0/1/2/3은 이름으로 DATA_QUALITY_OK/STALE/INVALID/NO_DATA에 대응한다. 근접 validity 0/1은 VALUE_VALIDITY_INVALID/VALUE_VALIDITY_VALID, reason 0..9는 같은 이름의 QUALITY_REASON_*에 대응한다. 근접 `SignalMeta.quality`를 validity/reason에서 생성하는 상세 바인딩과 실제 소비시각 AGE 산출이 입증되지 않으면 해당 정상 composite 인계는 HOLD다. 새로운 sourceGeneration 필드를 넣지 않는다.

```c
DomainIf_Status_t DomainNetPriv_OnGatewayContext(
    const NetMobileContext *message, const NetUartRxMeta *meta)
{
    if (message == NULL || meta == NULL) return DOMAIN_IF_INVALID_ARGUMENT;
    if (message->context.device_context_id == 0 || message->app_quality > 3 ||
        message->registration_connection_quality > 3 || message->proximity_validity > 1 ||
        message->proximity_reason > 9) return DOMAIN_IF_INVALID_ARGUMENT;
    if ((message->device_registration_state != 0 && message->device_registration_state != 1 && message->device_registration_state != 255) ||
        (message->bt_connection_state != 0 && message->bt_connection_state != 1 && message->bt_connection_state != 255) ||
        (message->app_active_state != 0 && message->app_active_state != 1 && message->app_active_state != 255) ||
        (message->proximity_state != 0 && message->proximity_state != 1 && message->proximity_state != 255))
        return DOMAIN_IF_INVALID_ARGUMENT;
    // 현재 peer/Device/Session·순서는 Router의 해당 수신 사건 근거로 재확인한다.
    if (/* CF01 채택·직렬화·두 C2 경계의 부분 실패 처리 근거가 없음 */)
        return DOMAIN_IF_NOT_AVAILABLE;
    if (/* 세 독립 평가의 AGE/품질 및 근접 quality 변환을 입증할 수 없음 */)
        return DOMAIN_IF_NOT_AVAILABLE;
    DigitalKeyConnectionState_t connection;
    ProximityInput_t proximity;
    connection.registration = message->device_registration_state == 0 ? DEVICE_REGISTRATION_NOT_REGISTERED :
        message->device_registration_state == 1 ? DEVICE_REGISTRATION_REGISTERED : DEVICE_REGISTRATION_UNKNOWN;
    connection.btConnection = message->bt_connection_state == 0 ? BT_CONNECTION_DISCONNECTED :
        message->bt_connection_state == 1 ? BT_CONNECTION_CONNECTED : BT_CONNECTION_UNKNOWN;
    connection.appActive = message->app_active_state == 0 ? APP_ACTIVE_INACTIVE :
        message->app_active_state == 1 ? APP_ACTIVE_ACTIVE : APP_ACTIVE_UNKNOWN;
    connection.sessionId = message->context.session_id;
    const DataQuality_t qualities[4] = {
        DATA_QUALITY_OK, DATA_QUALITY_STALE, DATA_QUALITY_INVALID, DATA_QUALITY_NO_DATA
    };
    connection.meta.quality = qualities[message->registration_connection_quality];
    connection.meta.updateSequence = message->registration_connection_update_sequence;
    connection.meta.ageMs = /* registration_connection_age_ms + 입증된 미반영 경과를 포화 누적한 소비시각 AGE */;
    connection.appMeta.quality = qualities[message->app_quality];
    connection.appMeta.updateSequence = message->app_update_sequence;
    connection.appMeta.ageMs = /* app_age_ms + 같은 소비시각까지 미반영 경과; 중복 반영 금지 */;
    proximity.state = message->proximity_state == 0 ? PROXIMITY_FAR :
        message->proximity_state == 1 ? PROXIMITY_NEAR : PROXIMITY_UNKNOWN;
    proximity.validity = message->proximity_validity == 0 ? VALUE_VALIDITY_INVALID : VALUE_VALIDITY_VALID;
    const QualityReason_t reasons[10] = {
        QUALITY_REASON_NONE, QUALITY_REASON_NOT_READY, QUALITY_REASON_OUT_OF_RANGE,
        QUALITY_REASON_SENSOR_FAULT, QUALITY_REASON_VISION_FAULT, QUALITY_REASON_STALE,
        QUALITY_REASON_NO_DATA, QUALITY_REASON_COMMUNICATION_FAULT,
        QUALITY_REASON_RECOVERING, QUALITY_REASON_DATA_INVALID
    };
    proximity.reason = reasons[message->proximity_reason];
    proximity.meta.updateSequence = message->proximity_update_sequence;
    proximity.meta.ageMs = /* proximity_age_ms + 입증된 미반영 경과의 포화 누적 */;
    proximity.meta.quality = /* Owner가 승인한 동일 validity/reason→quality 대응 결과 */;
    // 주석식의 시간·quality 근거가 없으면 위 HOLD 분기에서 반환하므로 미정 객체를 호출하지 않는다.
    DomainIf_Status_t c = DomainIf_RxDigitalKeyConnection(message->context.device_context_id, &connection);
    if (c != DOMAIN_IF_OK) return c;
    DomainIf_Status_t p = DomainIf_RxProximity(message->context.device_context_id, &proximity);
    if (p != DOMAIN_IF_OK) {
        // 연결 인계 성공/근접 실패를 각각 보존. 전체 성공·자동 rollback·전체 재호출 금지.
        return p; // 집계 반환과 소비 commit의 실제 계약은 D-05/C2-H01 HOLD다.
    }
    return DOMAIN_IF_OK; // 두 로컬 인계 성공만, 기능 활성/Manager 원자 commit 보증 아님.
}
```

위 C형 절차의 주석 대입은 명시 표·시간 계약을 적용하는 설명 단계다. 출처에 없는 품질 합성 함수를 구현한 것으로 읽지 않는다. 재전송은 세 원 평가번호를 증가시키지 않는다. 등록/현재 연결의 100ms 반복·300ms 중단은 후보이며 App·근접 source TTL이나 Session TTL이 아니다. v1(31/41B)의 빠진 B31..37을 0으로 채워 v2(38/48B)로 소비하지 않는다.

정상 예: 동일 Device7/Session9의 공동 등록·연결 평가11, App 평가22, 근접 평가33은 번호 세 개를 유지한다. App UNKNOWN/NO_DATA를 BLE CONNECTED 때문에 ACTIVE로 바꾸지 않는다. 반례: 같은 TYPE0x01의 v1 frame, Session 변경 중 부분 인계, 근접 INVALID를 FAR로 정상화하려는 입력은 각각 profile/문맥/품질 경계에서 제한된다.


<a id="verification"></a>
## 반례와 복구 연결

| 입력/사건 | 검사·결과 | 상태/복구 |
|---|---|---|
| 정상 후보의 정확한 P·모든 필드, profile 및 소비 바인딩의 독립 근거 존재 | Decoder→원 문맥 admission→위 기존 로컬 경계 | identity·품질·AGE 보존. 로컬 OK를 remote RX/기능 완료로 승격하지 않음 |
| frame 47B 또는 TYPE별 P 불일치 | 공통 CheckFrame 이전/내부에서 불완전·malformed 분리 | 부분 DTO/정상 liveness 갱신 없음; Stream suffix 정책 적용 |
| CRC 오류·VERSION1·반대 방향 TYPE | Frame/registry/profile에서 거부/HOLD | v1 padding으로 v2 변환하지 않음 |
| 유효 CRC 안 reserved enum·현재 문맥 불일치 | 전 field 또는 Router에서 거부 | 신뢰된 전체 frame 경계 소모; 내부 SYNC로 재분석하지 않음 |
| 동일 LINK_SEQUENCE·서로 다른 본문, wrap을 판정할 근거 없음 | ORDER_HOLD | 새 기능 실행·순번 ledger 자동진행 없음 |
| Queue 부족·Driver accepted 뒤 결과 미확정·구 callback | [공통 TX/RX](../UART_M_TX_RX_CONTROL.md)의 안정 사본·outcome·release 경계 | 접수 실패와 후발 실패 분리, 복구로 이전 Query/요청을 새 문맥에 귀속하지 않음 |

관련 기존 HOLD: UART_M_VERSION_LAYOUT_ADOPTION_OPEN, CF01, IF-O08, IF-O23, C2-H01; D-03/D-05·IF-O12/IF-O25는 해당 시간·Driver·소비 경계에 계속 적용한다. Interface 협의사항은 기록만 하며 S6 완료 후 실제 채택 근거와 별도 사용자 승인에 따라 통합한다.

봉인 근거: `S4C_RESULT/evidence/b08/uart_m/uplink.md#m_context`, `uart_m_contract.json`, `UART_M_ROUTING.md`, `UART_M_API_BINDINGS.md`; S4C `TIME_IDENTITY_CONTRACT.md`, `FRESHNESS_AGE_TIMEOUT.md`, `RESOURCE_QUEUE_DRIVER_FLOW.md`, `FAILURE_RECOVERY_CONTRACT.md`. 실제 원본 SHA는 제출 SOURCE_REGISTER에 기록한다.

[Data/API](#data-api) · [Wire](#wire) · [Function](#codec) · [M02 Query 감독](../UART_M_QUERY_CONTROL.md) · [Flow](../../flows/UART_M_UPLINK.md)
