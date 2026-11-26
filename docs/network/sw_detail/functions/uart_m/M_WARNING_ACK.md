# M_WARNING_ACK — ESP32 → DOMAIN

`WIRE-P02-M_WARNING_ACK` / `RX-P02-M_WARNING_ACK-DOMAIN`. VERSION2·TYPE `0x12`·payload **21B**·frame **31B** 후보다. CF01과 양단 layout 채택 증거가 없어 현재 정상 소비는 `BLOCKED_BY_CONTRACT`이며 문서상 조건부 경로를 상세화한다. 공개 API·Wire 변경·Production 승인으로 해석하지 않는다.

[UART-M 개요](../../05_UART_M_OVERVIEW.md) · [L04 Codec](../../layers/L04_FRAME_CODEC.md) · [M06](../../modules/M06_UART_CODEC.md) · [M02](../../modules/M02_QUERY_CONTROL.md) · [전역 flow](../../flows/UART_M_UPLINK.md)

<a id="data-api"></a>
## Data와 기존 API 경계

DTO 정본: [NetMobileWarningAck](../../data/UART_M_DATA.md#type-NetMobileWarningAck). RX meta: [NetUartRxMeta](../../data/UART_M_DATA.md#type-NetUartRxMeta). 공통 사본·Frame 타입: [UART Frame Data](../../data/UART_FRAME_DATA.md). C struct padding/enum 저장 폭/sizeof는 Wire가 아니며 아래 바이트 표로만 직렬화한다.

| 기존 정확한 원형 | 본체 소유/역할 | 봉인 S4A 출처 |
|---|---|---|
| `Gateway_Status_t Gateway_Interface_SendToDomain( const Gateway_RelayMessageView_t *message);` | ESP32 Gateway/각 Core 또는 Transport Owner | `api/esp32.md#api-Gateway_Interface_SendToDomain` |
| `DomainIf_Status_t DomainIf_RxWarningAck( const DomainIf_WarningAck_t *ack);` | DOMAIN (공개 본체 소유 합의 완료) | `api/domain_rx.md#api-DomainIf_RxWarningAck` |

송신 입력은 호출 동안 borrowed다. 로컬 성공 반환 전에 원 identity·필드·품질·시간 근거 전체를 독립 사본/Frame으로 확보한다. Decode의 `frame`은 호출 동안 안정된 입력, `out/meta`는 NULL 불가·서로 겹치지 않는 호출자 저장소다. 실패 OUT은 변경하지 않는다. 송신 out은 caller 소유이며 Driver 전달 이후 재사용·AGE 수정은 참조 해제 증거 전 금지한다. DOM C2는 Task/Owner 문맥, ESP32의 실제 Task/Superloop는 Owner 바인딩 전이다. ISR에서 codec·C2를 실행하지 않는다.

<a id="wire"></a>
## 정확한 payload 배치

payload B0은 Frame B8이다. 공통 envelope A5 5A·VERSION·TYPE·P·LINK_SEQUENCE·CRC는 [Frame 정본](../UART_FRAME_STREAM.md#check-frame)을 따른다. CRC-16/CCITT-FALSE는 Frame B2부터 payload 끝까지 P+6B에 적용하며 CRC 값은 LE다. 실패한 CRC나 정확한 P 불일치는 typed publish 이전에 거부한다.

| Payload byte / 폭 | 필드 → typed 멤버 | 부호·순서·범위/단위 | sentinel·예약·근거 |
|---|---|---|---|
| B0 / 4 | `DEVICE_CONTEXT_ID` → `context.device_context_id` | u32 / LE; 현재 단말0예약 | 0 예약; T34 L203 |
| B4 / 8 | `SESSION_ID` → `context.session_id` | u64 / LE; 현재 Session; 발급/0사용규칙 OPEN | 특수값 추가 정의 없음; T34 L203 |
| B12 / 1 | `WARNING_TYPE` → `warning_type` | u8 / N/A; 경고 종류; 1=NET_WARNING_REAR, 2=NET_WARNING_OCCUPANT, 3=NET_WARNING_ANTIPINCH, 4=NET_WARNING_BCM_FAULT, 5=NET_WARNING_CIS_FAULT, 6=NET_WARNING_WINDOW_FAULT, 7=NET_WARNING_VSS_FAULT, 8=NET_WARNING_OPEN_DOOR_AFTER_LEAVE | 특수값 추가 정의 없음; T34 L204 |
| B13 / 4 | `OCCURRENCE_ID` → `occurrence_id` | u32 / LE; 읽은 경고 발생 번호 | 특수값 추가 정의 없음; T34 L205 |
| B17 / 4 | `WARNING_DOMAIN_BOOT` → `warning_domain_boot` | u32 / LE; 읽은 경고의 Domain 기동 번호 | 특수값 추가 정의 없음; T34 L206 |

이 메시지에는 원 AGE 필드가 없다. LINK_SEQUENCE·로컬 수신시각을 원 생성 AGE 또는 source TTL로 대체하지 않는다. 원 Request/Query/Occurrence와 현재·과거 문맥을 보존한다. sequence wrap·clock reset·Session0의 실제 수용 정책이 불명확하면 해당 admission을 HOLD로 남긴다.

<a id="codec"></a>
## Typed encode/decode — M06 F17/F18

아래 함수는 `PROPOSED_PRIVATE_FUNCTION`이다. Encoder 호출자는 M04에서 확정한 해당 방향 LINK_SEQUENCE를 제공하고 Codec은 번호·시각·Session을 발급하지 않는다. Decoder 성공은 형식 DTO 확보뿐이며 현재 문맥/순서 및 소비자 성공은 [Router](../UART_M_ROUTER.md)에서 별도로 확인한다. 공통 [LE 함수](../UART_FRAME_STREAM.md#le-fields)는 명시된 각 offset에서 2/4/8B만 읽거나 쓴다.

```c
NetUartMResult NetUartM_Decode_M_WARNING_ACK(const NetUartMFrame *frame,
    NetUartMProfile profile, NetMobileWarningAck *out, NetUartRxMeta *meta)
{
    if (frame == NULL || out == NULL || meta == NULL) return NET_UART_M_INVALID_ARGUMENT;
    NetUartMResult r = NetUartM_CheckFrame(frame, NET_UART_M_UPLINK, 0x12, profile);
    if (r != NET_UART_M_OK) return r;
    const uint8_t *p = frame->bytes + 8; // 검증된 payload 시작; Frame offset은 payload+8이다.
    NetMobileWarningAck tmp = {0};
    tmp.context.device_context_id = NetUartM_ReadLe32(p + 0);
    tmp.context.session_id = NetUartM_ReadLe64(p + 4);
    tmp.warning_type = p[12];
    tmp.occurrence_id = NetUartM_ReadLe32(p + 13);
    tmp.warning_domain_boot = NetUartM_ReadLe32(p + 17);
    if (tmp.context.device_context_id == 0) return NET_UART_M_MALFORMED;
    if (tmp.warning_type != 1 && tmp.warning_type != 2 && tmp.warning_type != 3 && tmp.warning_type != 4 && tmp.warning_type != 5 && tmp.warning_type != 6 && tmp.warning_type != 7 && tmp.warning_type != 8) return NET_UART_M_MALFORMED;
    NetUartRxMeta tmpMeta;
    tmpMeta.link_sequence = NetUartM_ReadLe16(frame->bytes + 6);
    tmpMeta.received_at_ms = frame->receivedAtMs; // callback 현재시각으로 대체하지 않는다.
    *out = tmp; *meta = tmpMeta; // 전 검사 후 동시 논리 commit; 실패 시 두 OUT 불변.
    return NET_UART_M_OK;
}
```

```c
NetUartMResult NetUartM_Encode_M_WARNING_ACK(const NetMobileWarningAck *message,
    uint16_t linkSequence, NetUartMProfile profile, NetUartMFrame *out)
{
    if (message == NULL || out == NULL) return NET_UART_M_INVALID_ARGUMENT;
    if (message->context.device_context_id == 0) return NET_UART_M_INVALID_ARGUMENT;
    if (message->warning_type != 1 && message->warning_type != 2 && message->warning_type != 3 && message->warning_type != 4 && message->warning_type != 5 && message->warning_type != 6 && message->warning_type != 7 && message->warning_type != 8) return NET_UART_M_INVALID_ARGUMENT;
    uint8_t p[21] = {0}; // 호출 중에만 쓰는 임시 사본; 제품 queue 크기 제안 아님.
    NetUartM_WriteLe32(p + 0, message->context.device_context_id);
    NetUartM_WriteLe64(p + 4, message->context.session_id);
    p[12] = message->warning_type;
    NetUartM_WriteLe32(p + 13, message->occurrence_id);
    NetUartM_WriteLe32(p + 17, message->warning_domain_boot);
    return NetUartM_BuildFrame(p, 21, 0x12, linkSequence,
                                NET_UART_M_UPLINK, profile, out);
}
```

<a id="deliver"></a>
## 원 경고와 읽음 인계

현재 Device/Session과 경고 원 DOMAIN Boot·종류·Occurrence를 함께 확인한다. `DomainIf_WarningAck_t`에는 warning_domain_boot 필드가 없다. Network가 원 경고의 기동을 확인한 뒤 네 C2 멤버를 구성해야 하며 Boot를 확인하지 못하면 해당 정상 인계를 보류한다. Session이 같아도 다른 DOMAIN Boot의 같은 Occurrence는 같은 경고라는 증거가 아니다.

| Wire warning_type | C2 DomainWarningType_t |
|---:|---|
| 1 | DOMAIN_WARNING_REAR_OBSTACLE |
| 2 | DOMAIN_WARNING_OCCUPANT_HAZARD |
| 3 | DOMAIN_WARNING_WINDOW_ANTIPINCH |
| 4 | DOMAIN_WARNING_BCM_FAULT |
| 5 | DOMAIN_WARNING_CIS_FAULT |
| 6 | DOMAIN_WARNING_WINDOW_FAULT |
| 7 | DOMAIN_WARNING_VSS_FAULT |
| 8 | DOMAIN_WARNING_DOOR_LEFT_OPEN |

```c
// PROPOSED_PRIVATE_FUNCTION, DOMAIN Network M03/F08. ISR 호출 금지.
NetUartMResult NetUartM_DeliverWarningAck(const NetMobileWarningAck *message,
                                       const NetUartRxMeta *meta)
{
    if (message == NULL || meta == NULL) return NET_UART_M_INVALID_ARGUMENT;
    if (/* 현재 peer/Device/Session 및 원 경고 Boot/Kind/Occurrence 연결 미입증 */)
        return NET_UART_M_CONTEXT_HOLD;
    DomainIf_WarningAck_t ack;
    ack.deviceContextId = message->context.device_context_id;
    ack.sessionId = message->context.session_id;
    ack.occurrenceId = message->occurrence_id;
    switch (message->warning_type) {
    case 1: ack.type = DOMAIN_WARNING_REAR_OBSTACLE; break;
    case 2: ack.type = DOMAIN_WARNING_OCCUPANT_HAZARD; break;
    case 3: ack.type = DOMAIN_WARNING_WINDOW_ANTIPINCH; break;
    case 4: ack.type = DOMAIN_WARNING_BCM_FAULT; break;
    case 5: ack.type = DOMAIN_WARNING_CIS_FAULT; break;
    case 6: ack.type = DOMAIN_WARNING_WINDOW_FAULT; break;
    case 7: ack.type = DOMAIN_WARNING_VSS_FAULT; break;
    case 8: ack.type = DOMAIN_WARNING_DOOR_LEFT_OPEN; break;
    default: return NET_UART_M_MALFORMED;
    }
    DomainIf_Status_t r = DomainIf_RxWarningAck(&ack);
    if (r != DOMAIN_IF_OK) {
        // 원 C2 status와 경고 identity를 그대로 진단/Owner 경계에 보존한다.
        return NET_UART_M_CONSUMER_REJECTED;
    }
    return NET_UART_M_OK; // 읽음 로컬 인계. 위험 CLEAR·소리 STOP·차량 DONE 아님.
}
```

입력과 meta는 callback 동안 borrowed이며 소비자가 지연 보관하면 동일 경고 검증 근거와 함께 복사한다. Network가 별도 DomainIf 본체를 구현하지 않는다. 정상 예: 부트12의 REAR 경고 occurrence7을 같은 현재 Session에서 읽으면 C2 type REAR_OBSTACLE/occurrence7로 인계한다. 반례: 부트11 경고7, warning_type9, 이전 Session은 정상 인계하지 않는다. WINDOW 경고 ACK 명세가 WINDOW 실제 실행·표시 HOLD를 해제하지 않는다.


<a id="verification"></a>
## 반례와 복구 연결

| 입력/사건 | 검사·결과 | 상태/복구 |
|---|---|---|
| 정상 후보의 정확한 P·모든 필드, profile 및 소비 바인딩의 독립 근거 존재 | Decoder→원 문맥 admission→위 기존 로컬 경계 | identity·품질·AGE 보존. 로컬 OK를 remote RX/기능 완료로 승격하지 않음 |
| frame 30B 또는 TYPE별 P 불일치 | 공통 CheckFrame 이전/내부에서 불완전·malformed 분리 | 부분 DTO/정상 liveness 갱신 없음; Stream suffix 정책 적용 |
| CRC 오류·VERSION1·반대 방향 TYPE | Frame/registry/profile에서 거부/HOLD | v1 padding으로 v2 변환하지 않음 |
| 유효 CRC 안 reserved enum·현재 문맥 불일치 | 전 field 또는 Router에서 거부 | 신뢰된 전체 frame 경계 소모; 내부 SYNC로 재분석하지 않음 |
| 동일 LINK_SEQUENCE·서로 다른 본문, wrap을 판정할 근거 없음 | ORDER_HOLD | 새 기능 실행·순번 ledger 자동진행 없음 |
| Queue 부족·Driver accepted 뒤 결과 미확정·구 callback | [공통 TX/RX](../UART_M_TX_RX_CONTROL.md)의 안정 사본·outcome·release 경계 | 접수 실패와 후발 실패 분리, 복구로 이전 Query/요청을 새 문맥에 귀속하지 않음 |

관련 기존 HOLD: UART_M_VERSION_LAYOUT_ADOPTION_OPEN, IF-O08, IF-O23, C2-H01; D-03/D-05·IF-O12/IF-O25는 해당 시간·Driver·소비 경계에 계속 적용한다. Interface 협의사항은 기록만 하며 S6 완료 후 실제 채택 근거와 별도 사용자 승인에 따라 통합한다.

봉인 근거: `S4C_RESULT/evidence/b08/uart_m/uplink.md#m_warning_ack`, `uart_m_contract.json`, `UART_M_ROUTING.md`, `UART_M_API_BINDINGS.md`; S4C `TIME_IDENTITY_CONTRACT.md`, `FRESHNESS_AGE_TIMEOUT.md`, `RESOURCE_QUEUE_DRIVER_FLOW.md`, `FAILURE_RECOVERY_CONTRACT.md`. 실제 원본 SHA는 제출 SOURCE_REGISTER에 기록한다.

[Data/API](#data-api) · [Wire](#wire) · [Function](#codec) · [M02 Query 감독](../UART_M_QUERY_CONTROL.md) · [Flow](../../flows/UART_M_UPLINK.md)
