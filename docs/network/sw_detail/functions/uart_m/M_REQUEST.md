# M_REQUEST — ESP32 → DOMAIN

`WIRE-P02-M_REQUEST` / `RX-P02-M_REQUEST-DOMAIN`. VERSION2·TYPE `0x10`·payload **24B**·frame **34B** 후보다. CF01과 양단 layout 채택 증거가 없어 현재 정상 소비는 `BLOCKED_BY_CONTRACT`이며 문서상 조건부 경로를 상세화한다. 공개 API·Wire 변경·Production 승인으로 해석하지 않는다.

[UART-M 개요](../../05_UART_M_OVERVIEW.md) · [L04 Codec](../../layers/L04_FRAME_CODEC.md) · [M06](../../modules/M06_UART_CODEC.md) · [M02](../../modules/M02_QUERY_CONTROL.md) · [전역 flow](../../flows/UART_M_UPLINK.md)

<a id="data-api"></a>
## Data와 기존 API 경계

DTO 정본: [NetMobileRequest](../../data/UART_M_DATA.md#type-NetMobileRequest). RX meta: [NetUartRxMeta](../../data/UART_M_DATA.md#type-NetUartRxMeta). 공통 사본·Frame 타입: [UART Frame Data](../../data/UART_FRAME_DATA.md). C struct padding/enum 저장 폭/sizeof는 Wire가 아니며 아래 바이트 표로만 직렬화한다.

| 기존 정확한 원형 | 본체 소유/역할 | 봉인 S4A 출처 |
|---|---|---|
| `Gateway_Status_t Gateway_Interface_SendToDomain( const Gateway_RelayMessageView_t *message);` | ESP32 Gateway/각 Core 또는 Transport Owner | `api/esp32.md#api-Gateway_Interface_SendToDomain` |
| `DomainIf_Status_t DomainIf_RxMobileRequest( const DomainIf_MobileRequest_t *request);` | DOMAIN (공개 본체 소유 합의 완료) | `api/domain_rx.md#api-DomainIf_RxMobileRequest` |

송신 입력은 호출 동안 borrowed다. 로컬 성공 반환 전에 원 identity·필드·품질·시간 근거 전체를 독립 사본/Frame으로 확보한다. Decode의 `frame`은 호출 동안 안정된 입력, `out/meta`는 NULL 불가·서로 겹치지 않는 호출자 저장소다. 실패 OUT은 변경하지 않는다. 송신 out은 caller 소유이며 Driver 전달 이후 재사용·AGE 수정은 참조 해제 증거 전 금지한다. DOM C2는 Task/Owner 문맥, ESP32의 실제 Task/Superloop는 Owner 바인딩 전이다. ISR에서 codec·C2를 실행하지 않는다.

<a id="wire"></a>
## 정확한 payload 배치

payload B0은 Frame B8이다. 공통 envelope A5 5A·VERSION·TYPE·P·LINK_SEQUENCE·CRC는 [Frame 정본](../UART_FRAME_STREAM.md#check-frame)을 따른다. CRC-16/CCITT-FALSE는 Frame B2부터 payload 끝까지 P+6B에 적용하며 CRC 값은 LE다. 실패한 CRC나 정확한 P 불일치는 typed publish 이전에 거부한다.

| Payload byte / 폭 | 필드 → typed 멤버 | 부호·순서·범위/단위 | sentinel·예약·근거 |
|---|---|---|---|
| B0 / 4 | `DEVICE_CONTEXT_ID` → `context.device_context_id` | u32 / LE; 현재 단말0예약 | 0 예약; T34 L150 |
| B4 / 8 | `SESSION_ID` → `context.session_id` | u64 / LE; 현재 Session; 발급/0사용규칙 OPEN | 특수값 추가 정의 없음; T34 L150 |
| B12 / 4 | `REQUEST_ID` → `request_id` | u32 / LE; App이 새 사용자 입력마다 생성 | 특수값 추가 정의 없음; T34 L151 |
| B16 / 1 | `REQUEST_KIND` → `kind` | u8 / N/A; 1=도어; 2=공조; 3=실내 조명; 4=디지털 키; 1=NET_REQUEST_DOOR, 2=NET_REQUEST_CLIMATE, 3=NET_REQUEST_INTERIOR_LIGHT, 4=NET_REQUEST_DIGITAL_KEY | 특수값 추가 정의 없음; T34 L152 |
| B17 / 1 | `OPERATION` → `operation` | u8 / N/A; 요청 종류별 동작 코드 | 특수값 추가 정의 없음; T34 L153 |
| B18 / 1 | `ARG_0` → `argument (선택 union 멤버의 byte0)` | u8 / N/A; §6.1 요청 종류별 인수 | 특수값 추가 정의 없음; T34 L154 |
| B19 / 1 | `ARG_1` → `argument (선택 union 멤버의 byte1)` | u8 / N/A; §6.1 요청 종류별 인수 | 특수값 추가 정의 없음; T34 L155 |
| B20 / 1 | `ARG_2` → `argument (선택 union 멤버의 byte2)` | u8 / N/A; §6.1 요청 종류별 인수 | 특수값 추가 정의 없음; T34 L156 |
| B21 / 1 | `ARG_3` → `argument (선택 union 멤버의 byte3)` | u8 / N/A; §6.1 요청 종류별 인수 | 특수값 추가 정의 없음; T34 L157 |
| B22 / 1 | `ARG_4` → `argument (선택 union 멤버의 byte4)` | u8 / N/A; §6.1 요청 종류별 인수 | 특수값 추가 정의 없음; T34 L158 |
| B23 / 1 | `ARG_5` → `argument (선택 union 멤버의 byte5)` | u8 / N/A; §6.1 요청 종류별 인수 | 특수값 추가 정의 없음; T34 L159 |

이 메시지에는 원 AGE 필드가 없다. LINK_SEQUENCE·로컬 수신시각을 원 생성 AGE 또는 source TTL로 대체하지 않는다. 원 Request/Query/Occurrence와 현재·과거 문맥을 보존한다. sequence wrap·clock reset·Session0의 실제 수용 정책이 불명확하면 해당 admission을 HOLD로 남긴다.

<a id="codec"></a>
## Typed encode/decode — M06 F17/F18

아래 함수는 `PROPOSED_PRIVATE_FUNCTION`이다. Encoder 호출자는 M04에서 확정한 해당 방향 LINK_SEQUENCE를 제공하고 Codec은 번호·시각·Session을 발급하지 않는다. Decoder 성공은 형식 DTO 확보뿐이며 현재 문맥/순서 및 소비자 성공은 [Router](../UART_M_ROUTER.md)에서 별도로 확인한다. 공통 [LE 함수](../UART_FRAME_STREAM.md#le-fields)는 명시된 각 offset에서 2/4/8B만 읽거나 쓴다.

```c
NetUartMResult NetUartM_Decode_M_REQUEST(const NetUartMFrame *frame,
    NetUartMProfile profile, NetMobileRequest *out, NetUartRxMeta *meta)
{
    if (frame == NULL || out == NULL || meta == NULL) return NET_UART_M_INVALID_ARGUMENT;
    NetUartMResult r = NetUartM_CheckFrame(frame, NET_UART_M_UPLINK, 0x10, profile);
    if (r != NET_UART_M_OK) return r;
    const uint8_t *p = frame->bytes + 8; // 검증된 payload 시작; Frame offset은 payload+8이다.
    NetMobileRequest tmp = {0};
    tmp.context.device_context_id = NetUartM_ReadLe32(p + 0);
    tmp.context.session_id = NetUartM_ReadLe64(p + 4);
    tmp.request_id = NetUartM_ReadLe32(p + 12);
    tmp.kind = p[16];
    tmp.operation = p[17];
    if (tmp.context.device_context_id == 0) return NET_UART_M_MALFORMED;
    if (tmp.kind != 1 && tmp.kind != 2 && tmp.kind != 3 && tmp.kind != 4) return NET_UART_M_MALFORMED;
    // 미사용 ARG는 kind/operation별로 검사하며, C union 메모리를 복사하지 않는다.
    switch (p[16]) {
    case 1: // DOOR
        if ((p[17] != 0 && p[17] != 1) || p[18] || p[19] || p[20] || p[21] || p[22] || p[23])
            return NET_UART_M_MALFORMED;
        break; // LOCK/UNLOCK는 operation이며 활성 argument 멤버가 없다.
    case 2: // CLIMATE
        if (p[17] == 1) {
            if (p[20] || p[21] || p[22] || p[23]) return NET_UART_M_MALFORMED;
            tmp.argument.target_temperature_centi_c = NetUartM_ReadI16(p + 18);
        } else if (p[17] == 2) {
            if (p[18] > 1 || p[19] || p[20] || p[21] || p[22] || p[23]) return NET_UART_M_MALFORMED;
            tmp.argument.climate_auto = p[18];
        } else if (p[17] == 3) {
            if (p[18] > 3 || p[19] || p[20] || p[21] || p[22] || p[23]) return NET_UART_M_MALFORMED;
            tmp.argument.fan_level = p[18];
        } else return NET_UART_M_MALFORMED;
        break;
    case 3: // INTERIOR_LIGHT
        if (p[17] == 1) {
            if (p[18] > 1 || p[19] || p[20] || p[21] || p[22] || p[23]) return NET_UART_M_MALFORMED;
            tmp.argument.light_enabled = p[18];
        } else if (p[17] == 2) {
            if (p[18] > 100 || p[19] || p[20] || p[21] || p[22] || p[23]) return NET_UART_M_MALFORMED;
            tmp.argument.light_level_percent = p[18];
        } else if (p[17] == 3) {
            if (p[21] || p[22] || p[23]) return NET_UART_M_MALFORMED;
            tmp.argument.normal_rgb.r = p[18];
            tmp.argument.normal_rgb.g = p[19];
            tmp.argument.normal_rgb.b = p[20]; // 255는 유효 RGB다.
        } else return NET_UART_M_MALFORMED;
        break;
    case 4: // DIGITAL_KEY
        if (p[17] != 1 || p[18] > 1 || p[19] || p[20] || p[21] || p[22] || p[23])
            return NET_UART_M_MALFORMED;
        tmp.argument.digital_key_enabled = p[18];
        break;
    default: return NET_UART_M_MALFORMED;
    }
    NetUartRxMeta tmpMeta;
    tmpMeta.link_sequence = NetUartM_ReadLe16(frame->bytes + 6);
    tmpMeta.received_at_ms = frame->receivedAtMs; // callback 현재시각으로 대체하지 않는다.
    *out = tmp; *meta = tmpMeta; // 전 검사 후 동시 논리 commit; 실패 시 두 OUT 불변.
    return NET_UART_M_OK;
}
```

```c
NetUartMResult NetUartM_Encode_M_REQUEST(const NetMobileRequest *message,
    uint16_t linkSequence, NetUartMProfile profile, NetUartMFrame *out)
{
    if (message == NULL || out == NULL) return NET_UART_M_INVALID_ARGUMENT;
    if (message->context.device_context_id == 0) return NET_UART_M_INVALID_ARGUMENT;
    if (message->kind != 1 && message->kind != 2 && message->kind != 3 && message->kind != 4) return NET_UART_M_INVALID_ARGUMENT;
    uint8_t p[24] = {0}; // 호출 중에만 쓰는 임시 사본; 제품 queue 크기 제안 아님.
    NetUartM_WriteLe32(p + 0, message->context.device_context_id);
    NetUartM_WriteLe64(p + 4, message->context.session_id);
    NetUartM_WriteLe32(p + 12, message->request_id);
    p[16] = message->kind;
    p[17] = message->operation;
    // p[18..23]은 임시 payload의 0 초기값을 유지하고 선택된 의미만 기록한다.
    switch (message->kind) {
    case NET_REQUEST_DOOR:
        if (message->operation != 0 && message->operation != 1) return NET_UART_M_INVALID_ARGUMENT;
        break;
    case NET_REQUEST_CLIMATE:
        if (message->operation == 1) {
            NetUartM_WriteI16(p + 18, message->argument.target_temperature_centi_c);
        } else if (message->operation == 2) {
            if (message->argument.climate_auto > 1) return NET_UART_M_INVALID_ARGUMENT;
            p[18] = message->argument.climate_auto;
        } else if (message->operation == 3) {
            if (message->argument.fan_level > 3) return NET_UART_M_INVALID_ARGUMENT;
            p[18] = message->argument.fan_level;
        } else return NET_UART_M_INVALID_ARGUMENT;
        break;
    case NET_REQUEST_INTERIOR_LIGHT:
        if (message->operation == 1) {
            if (message->argument.light_enabled > 1) return NET_UART_M_INVALID_ARGUMENT;
            p[18] = message->argument.light_enabled;
        } else if (message->operation == 2) {
            if (message->argument.light_level_percent > 100) return NET_UART_M_INVALID_ARGUMENT;
            p[18] = message->argument.light_level_percent;
        } else if (message->operation == 3) {
            p[18] = message->argument.normal_rgb.r;
            p[19] = message->argument.normal_rgb.g;
            p[20] = message->argument.normal_rgb.b;
        } else return NET_UART_M_INVALID_ARGUMENT;
        break;
    case NET_REQUEST_DIGITAL_KEY:
        if (message->operation != 1 || message->argument.digital_key_enabled > 1)
            return NET_UART_M_INVALID_ARGUMENT;
        p[18] = message->argument.digital_key_enabled;
        break;
    default: return NET_UART_M_INVALID_ARGUMENT;
    }
    return NetUartM_BuildFrame(p, 24, 0x10, linkSequence,
                                NET_UART_M_UPLINK, profile, out);
}
```

<a id="request-map"></a>
## 기존 C2 8가지 요청과 원 시각의 한계

| C2 type | kind/op | ARG0..5 | C2 payload 멤버·명시 변환 |
|---|---|---|---|
| MOBILE_REQUEST_DOOR_LOCK | 1 / 0 또는 1 | 모두 0 | doorTarget: op0→DOOR_TARGET_LOCK, op1→DOOR_TARGET_UNLOCK |
| MOBILE_REQUEST_CLIMATE_TARGET_TEMPERATURE | 2 / 1 | ARG0..1 signed i16 LE, ARG2..5=0 | targetTemperature: centi-°C 정수 보존; 기능 범위 DOMAIN 확인 |
| MOBILE_REQUEST_CLIMATE_AUTO_ENABLE | 2 / 2 | ARG0=0/1, 나머지0 | enable: 0→false, 1→true |
| MOBILE_REQUEST_FAN_LEVEL | 2 / 3 | ARG0=0..3, 나머지0 | fanLevel: OFF/LOW/MEDIUM/HIGH 이름 대응; UNKNOWN255 금지 |
| MOBILE_REQUEST_INTERIOR_LIGHT_ENABLE | 3 / 1 | ARG0=0/1, 나머지0 | enable: 0→false, 1→true |
| MOBILE_REQUEST_INTERIOR_LIGHT_LEVEL | 3 / 2 | ARG0=0..100, 나머지0 | levelPercent: 1%/raw |
| MOBILE_REQUEST_INTERIOR_LIGHT_COLOR | 3 / 3 | ARG0/1/2 R/G/B, ARG3..5=0 | color.red/green/blue: 각0..255 모두 유효 |
| MOBILE_REQUEST_DIGITAL_KEY_SETTING | 4 / 1 | ARG0=0/1, 나머지0 | digitalKeySetting: OFF/ON 이름 대응 |

송신 Gateway의 넓은 category만으로 subtype를 정하지 않는다. `TYPE 1B + 논리 payload P B`는 IF-O23의 미승인 Adapter convention이며 UART A5/5A/CRC를 BLE payload로 복사하지 않는다. 중첩 borrowed view의 data와 Device/Session/RequestID를 함께 복사할 수 있을 때만 로컬 OK다. 원 App RequestID는 재접속·재전달로 새 번호를 만들지 않는다.

`DomainIf_MobileRequest_t.context.sessionId/requestId`는 Wire B4/B12, deviceContextId는 B0이다. `SignalMeta_t meta`의 원 생성 AGE/품질/평가 근거는 24B에 없다. UART received_at_ms를 MOBILE SENT 시각으로 바꾸지 않는다. 원 SENT부터 3초 정책의 현재 효력·측정 근거가 없으면 그 정책에 의존하는 실행과 완전한 C2 meta 구성이 제한된다. 이는 M_QUERY·상태 조회 전체를 막는 조건이 아니다.

<a id="deliver"></a>
```c
// PROPOSED_PRIVATE_FUNCTION, DOMAIN Network M03/F08; 공개 C2 본체는 DOMAIN 소유.
NetUartMResult NetUartM_DeliverRequest(const NetMobileRequest *message,
                                    const NetUartRxMeta *meta)
{
    if (message == NULL || meta == NULL) return NET_UART_M_INVALID_ARGUMENT;
    if (message->context.device_context_id == 0 || message->kind < 1 || message->kind > 4)
        return NET_UART_M_INVALID_ARGUMENT;
    if ((message->kind == 1 && message->operation > 1) ||
        ((message->kind == 2 || message->kind == 3) && (message->operation < 1 || message->operation > 3)) ||
        (message->kind == 4 && message->operation != 1)) return NET_UART_M_INVALID_ARGUMENT;
    if ((message->kind == 2 && message->operation == 2 && message->argument.climate_auto > 1) ||
        (message->kind == 2 && message->operation == 3 && message->argument.fan_level > 3) ||
        (message->kind == 3 && message->operation == 1 && message->argument.light_enabled > 1) ||
        (message->kind == 3 && message->operation == 2 && message->argument.light_level_percent > 100) ||
        (message->kind == 4 && message->argument.digital_key_enabled > 1)) return NET_UART_M_INVALID_ARGUMENT;
    if (/* 현재 등록 peer/Session 또는 원 Request identity 연결이 검증되지 않음 */)
        return NET_UART_M_CONTEXT_HOLD;
    if (/* 원 SignalMeta 구성·SENT AGE 수용 정책이 미합의 */)
        return NET_UART_M_CONTRACT_HOLD; // Gateway 수신시각/AGE0을 대신 넣지 않는다.
    DomainIf_MobileRequest_t request;
    request.deviceContextId = message->context.device_context_id;
    request.context.sessionId = message->context.session_id;
    request.context.requestId = message->request_id;
    request.meta = /* 동일 원 요청의 입증된 SignalMeta; 없는 SENT/AGE를 수신시각이나0으로 채우지 않음 */;
    switch (message->kind) {
    case NET_REQUEST_DOOR:
        request.type = MOBILE_REQUEST_DOOR_LOCK;
        request.payload.doorTarget = message->operation == 0 ? DOOR_TARGET_LOCK : DOOR_TARGET_UNLOCK;
        break;
    case NET_REQUEST_CLIMATE:
        if (message->operation == 1) {
            request.type = MOBILE_REQUEST_CLIMATE_TARGET_TEMPERATURE;
            request.payload.targetTemperature = message->argument.target_temperature_centi_c;
        } else if (message->operation == 2) {
            request.type = MOBILE_REQUEST_CLIMATE_AUTO_ENABLE;
            request.payload.enable = message->argument.climate_auto == 1;
        } else {
            request.type = MOBILE_REQUEST_FAN_LEVEL;
            const FanLevel_t fan[4] = { FAN_LEVEL_OFF, FAN_LEVEL_LOW, FAN_LEVEL_MEDIUM, FAN_LEVEL_HIGH };
            request.payload.fanLevel = fan[message->argument.fan_level];
        }
        break;
    case NET_REQUEST_INTERIOR_LIGHT:
        if (message->operation == 1) {
            request.type = MOBILE_REQUEST_INTERIOR_LIGHT_ENABLE;
            request.payload.enable = message->argument.light_enabled == 1;
        } else if (message->operation == 2) {
            request.type = MOBILE_REQUEST_INTERIOR_LIGHT_LEVEL;
            request.payload.levelPercent = message->argument.light_level_percent;
        } else {
            request.type = MOBILE_REQUEST_INTERIOR_LIGHT_COLOR;
            request.payload.color.red = message->argument.normal_rgb.r;
            request.payload.color.green = message->argument.normal_rgb.g;
            request.payload.color.blue = message->argument.normal_rgb.b;
        }
        break;
    case NET_REQUEST_DIGITAL_KEY:
        request.type = MOBILE_REQUEST_DIGITAL_KEY_SETTING;
        request.payload.digitalKeySetting = message->argument.digital_key_enabled == 0 ?
            DIGITAL_KEY_SETTING_OFF : DIGITAL_KEY_SETTING_ON;
        break;
    default: return NET_UART_M_MALFORMED;
    }
    // 온도 -100은 -1.00°C이며 범위 허용 판단은 DOMAIN에 남긴다.
    DomainIf_Status_t r = DomainIf_RxMobileRequest(&request);
    if (r != DOMAIN_IF_OK) {
        // 원 C2 반환 r과 원 Request identity를 보존한다. 차량 FAILED를 생성하지 않는다.
        return NET_UART_M_CONSUMER_REJECTED;
    }
    return NET_UART_M_OK; // C2 로컬 인계까지만 확인.
}
```

Decode/Encode와 표의 eight operation mapping은 형식·의미 변환 명세이며 위 주석으로 보인 C2 원 meta 생성의 미결을 해소한 구현이 아니다. 정상 예: kind3/op3, RGB(0,128,255), ARG3..5=0은 정확히 그 색의 요청을 보존한다. 반례: ARG5=1·bool2·Fan255·밝기101·kind2/op4는 C2 호출 전 거부한다. 재접속 시 미확정 과거 요청을 자동 재전송하거나 새 commandId로 실행하지 않는다.


<a id="verification"></a>
## 반례와 복구 연결

| 입력/사건 | 검사·결과 | 상태/복구 |
|---|---|---|
| 정상 후보의 정확한 P·모든 필드, profile 및 소비 바인딩의 독립 근거 존재 | Decoder→원 문맥 admission→위 기존 로컬 경계 | identity·품질·AGE 보존. 로컬 OK를 remote RX/기능 완료로 승격하지 않음 |
| frame 33B 또는 TYPE별 P 불일치 | 공통 CheckFrame 이전/내부에서 불완전·malformed 분리 | 부분 DTO/정상 liveness 갱신 없음; Stream suffix 정책 적용 |
| CRC 오류·VERSION1·반대 방향 TYPE | Frame/registry/profile에서 거부/HOLD | v1 padding으로 v2 변환하지 않음 |
| 유효 CRC 안 reserved enum·현재 문맥 불일치 | 전 field 또는 Router에서 거부 | 신뢰된 전체 frame 경계 소모; 내부 SYNC로 재분석하지 않음 |
| 동일 LINK_SEQUENCE·서로 다른 본문, wrap을 판정할 근거 없음 | ORDER_HOLD | 새 기능 실행·순번 ledger 자동진행 없음 |
| Queue 부족·Driver accepted 뒤 결과 미확정·구 callback | [공통 TX/RX](../UART_M_TX_RX_CONTROL.md)의 안정 사본·outcome·release 경계 | 접수 실패와 후발 실패 분리, 복구로 이전 Query/요청을 새 문맥에 귀속하지 않음 |

관련 기존 HOLD: UART_M_VERSION_LAYOUT_ADOPTION_OPEN, IF-O08, IF-O23, IF-N01, IF-O03, C2-H01; D-03/D-05·IF-O12/IF-O25는 해당 시간·Driver·소비 경계에 계속 적용한다. Interface 협의사항은 기록만 하며 S6 완료 후 실제 채택 근거와 별도 사용자 승인에 따라 통합한다.

봉인 근거: `S4C_RESULT/evidence/b08/uart_m/uplink.md#m_request`, `uart_m_contract.json`, `UART_M_ROUTING.md`, `UART_M_API_BINDINGS.md`; S4C `TIME_IDENTITY_CONTRACT.md`, `FRESHNESS_AGE_TIMEOUT.md`, `RESOURCE_QUEUE_DRIVER_FLOW.md`, `FAILURE_RECOVERY_CONTRACT.md`. 실제 원본 SHA는 제출 SOURCE_REGISTER에 기록한다.

[Data/API](#data-api) · [Wire](#wire) · [Function](#codec) · [M02 Query 감독](../UART_M_QUERY_CONTROL.md) · [Flow](../../flows/UART_M_UPLINK.md)
