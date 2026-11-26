# M_QUERY_END — DOMAIN → ESP32

`WIRE-P02-M_QUERY_END` / `RX-P02-M_QUERY_END-ESP32`. VERSION2·TYPE `0x40`·payload **24B**·frame **34B** 후보다. CF01과 양단 layout 채택 증거가 없어 현재 정상 소비는 `BLOCKED_BY_CONTRACT`이며 문서상 조건부 경로를 상세화한다. 공개 API·Wire 변경·Production 승인으로 해석하지 않는다.

[UART-M 개요](../../05_UART_M_OVERVIEW.md) · [L04 Codec](../../layers/L04_FRAME_CODEC.md) · [M06](../../modules/M06_UART_CODEC.md) · [M02](../../modules/M02_QUERY_CONTROL.md) · [전역 flow](../../flows/UART_M_QUERY_AND_PROJECTION.md)

<a id="data-api"></a>
## Data와 기존 API 경계

DTO 정본: [NetMobileQueryEnd](../../data/UART_M_DATA.md#type-NetMobileQueryEnd). RX meta: [NetUartRxMeta](../../data/UART_M_DATA.md#type-NetUartRxMeta). 공통 사본·Frame 타입: [UART Frame Data](../../data/UART_FRAME_DATA.md). C struct padding/enum 저장 폭/sizeof는 Wire가 아니며 아래 바이트 표로만 직렬화한다.

| 기존 정확한 원형 | 본체 소유/역할 | 봉인 S4A 출처 |
|---|---|---|
| `NetSubmitResult DomainNetPriv_SendQueryEnd(const NetMobileQueryEnd *message);` | DOMAIN Network | `api/domain_private.md#api-DomainNetPriv_SendQueryEnd` |
| `void EspNetPriv_OnQueryEnd(const NetMobileQueryEnd *message, const NetUartRxMeta *meta);` | ESP32 Network | `api/esp32.md#api-EspNetPriv_OnQueryEnd` |

송신 입력은 호출 동안 borrowed다. 로컬 성공 반환 전에 원 identity·필드·품질·시간 근거 전체를 독립 사본/Frame으로 확보한다. Decode의 `frame`은 호출 동안 안정된 입력, `out/meta`는 NULL 불가·서로 겹치지 않는 호출자 저장소다. 실패 OUT은 변경하지 않는다. 송신 out은 caller 소유이며 Driver 전달 이후 재사용·AGE 수정은 참조 해제 증거 전 금지한다. DOM C2는 Task/Owner 문맥, ESP32의 실제 Task/Superloop는 Owner 바인딩 전이다. ISR에서 codec·C2를 실행하지 않는다.

<a id="wire"></a>
## 정확한 payload 배치

payload B0은 Frame B8이다. 공통 envelope A5 5A·VERSION·TYPE·P·LINK_SEQUENCE·CRC는 [Frame 정본](../UART_FRAME_STREAM.md#check-frame)을 따른다. CRC-16/CCITT-FALSE는 Frame B2부터 payload 끝까지 P+6B에 적용하며 CRC 값은 LE다. 실패한 CRC나 정확한 P 불일치는 typed publish 이전에 거부한다.

| Payload byte / 폭 | 필드 → typed 멤버 | 부호·순서·범위/단위 | sentinel·예약·근거 |
|---|---|---|---|
| B0 / 4 | `DEVICE_CONTEXT_ID` → `context.recipient.device_context_id` | u32 / LE; 현재 수신 단말0예약 | 0 예약; T34 L363 |
| B4 / 8 | `SESSION_ID` → `context.recipient.session_id` | u64 / LE; 현재 수신 Session; 원 요청 Session과 별개 | 특수값 추가 정의 없음; T34 L363 |
| B12 / 4 | `DOMAIN_BOOT_ID` → `context.domain_boot_id` | u32 / LE; 현재 DOMAIN기동0예약 | 0 예약 (공통 기동 규칙); T34 L363 |
| B16 / 4 | `QUERY_ID` → `context.query_id` | u32 / LE; 원 M_QUERY의 0이 아닌 조회 ID; 자발 출력용0은 End에 적용하지 않음 | 0 금지; 원 조회 ID 보존; T34 L363 |
| B20 / 1 | `QUERY_SCOPE` → `query_scope` | u8 / N/A; 원 조회 범위; 0=NET_QUERY_SCOPE_STATE, 1=NET_QUERY_SCOPE_RESULT, 2=NET_QUERY_SCOPE_WARNING, 3=NET_QUERY_SCOPE_ALL | 특수값 추가 정의 없음; T34 L364 |
| B21 / 1 | `QUERY_STATUS` → `query_status` | u8 / N/A; 0=완료; 1=요청 없음; 2=일부 확인 불가; 3=문맥 거부; 0=NET_QUERY_STATUS_DONE, 1=NET_QUERY_STATUS_NOT_FOUND, 2=NET_QUERY_STATUS_PARTIAL_UNCONFIRMED, 3=NET_QUERY_STATUS_CONTEXT_REJECTED | 특수값 추가 정의 없음; T34 L365 |
| B22 / 2 | `ITEM_COUNT` → `item_count` | u16 / LE; 조회 종료를 뺀 응답 메시지 수 | 특수값 추가 정의 없음; T34 L366 |

이 메시지에는 원 AGE 필드가 없다. LINK_SEQUENCE·로컬 수신시각을 원 생성 AGE 또는 source TTL로 대체하지 않는다. 원 Request/Query/Occurrence와 현재·과거 문맥을 보존한다. sequence wrap·clock reset·Session0의 실제 수용 정책이 불명확하면 해당 admission을 HOLD로 남긴다.

<a id="codec"></a>
## Typed encode/decode — M06 F17/F18

아래 함수는 `PROPOSED_PRIVATE_FUNCTION`이다. Encoder 호출자는 M04에서 확정한 해당 방향 LINK_SEQUENCE를 제공하고 Codec은 번호·시각·Session을 발급하지 않는다. Decoder 성공은 형식 DTO 확보뿐이며 현재 문맥/순서 및 소비자 성공은 [Router](../UART_M_ROUTER.md)에서 별도로 확인한다. 공통 [LE 함수](../UART_FRAME_STREAM.md#le-fields)는 명시된 각 offset에서 2/4/8B만 읽거나 쓴다.

```c
NetUartMResult NetUartM_Decode_M_QUERY_END(const NetUartMFrame *frame,
    NetUartMProfile profile, NetMobileQueryEnd *out, NetUartRxMeta *meta)
{
    if (frame == NULL || out == NULL || meta == NULL) return NET_UART_M_INVALID_ARGUMENT;
    NetUartMResult r = NetUartM_CheckFrame(frame, NET_UART_M_DOWNLINK, 0x40, profile);
    if (r != NET_UART_M_OK) return r;
    const uint8_t *p = frame->bytes + 8; // 검증된 payload 시작; Frame offset은 payload+8이다.
    NetMobileQueryEnd tmp = {0};
    tmp.context.recipient.device_context_id = NetUartM_ReadLe32(p + 0);
    tmp.context.recipient.session_id = NetUartM_ReadLe64(p + 4);
    tmp.context.domain_boot_id = NetUartM_ReadLe32(p + 12);
    tmp.context.query_id = NetUartM_ReadLe32(p + 16);
    tmp.query_scope = p[20];
    tmp.query_status = p[21];
    tmp.item_count = NetUartM_ReadLe16(p + 22);
    if (tmp.context.recipient.device_context_id == 0) return NET_UART_M_MALFORMED;
    if (tmp.context.domain_boot_id == 0) return NET_UART_M_MALFORMED;
    if (tmp.context.query_id == 0) return NET_UART_M_MALFORMED;
    if (tmp.query_scope != 0 && tmp.query_scope != 1 && tmp.query_scope != 2 && tmp.query_scope != 3) return NET_UART_M_MALFORMED;
    if (tmp.query_status != 0 && tmp.query_status != 1 && tmp.query_status != 2 && tmp.query_status != 3) return NET_UART_M_MALFORMED;
    NetUartRxMeta tmpMeta;
    tmpMeta.link_sequence = NetUartM_ReadLe16(frame->bytes + 6);
    tmpMeta.received_at_ms = frame->receivedAtMs; // callback 현재시각으로 대체하지 않는다.
    *out = tmp; *meta = tmpMeta; // 전 검사 후 동시 논리 commit; 실패 시 두 OUT 불변.
    return NET_UART_M_OK;
}
```

```c
NetUartMResult NetUartM_Encode_M_QUERY_END(const NetMobileQueryEnd *message,
    uint16_t linkSequence, NetUartMProfile profile, NetUartMFrame *out)
{
    if (message == NULL || out == NULL) return NET_UART_M_INVALID_ARGUMENT;
    if (message->context.recipient.device_context_id == 0) return NET_UART_M_INVALID_ARGUMENT;
    if (message->context.domain_boot_id == 0) return NET_UART_M_INVALID_ARGUMENT;
    if (message->context.query_id == 0) return NET_UART_M_INVALID_ARGUMENT;
    if (message->query_scope != 0 && message->query_scope != 1 && message->query_scope != 2 && message->query_scope != 3) return NET_UART_M_INVALID_ARGUMENT;
    if (message->query_status != 0 && message->query_status != 1 && message->query_status != 2 && message->query_status != 3) return NET_UART_M_INVALID_ARGUMENT;
    uint8_t p[24] = {0}; // 호출 중에만 쓰는 임시 사본; 제품 queue 크기 제안 아님.
    NetUartM_WriteLe32(p + 0, message->context.recipient.device_context_id);
    NetUartM_WriteLe64(p + 4, message->context.recipient.session_id);
    NetUartM_WriteLe32(p + 12, message->context.domain_boot_id);
    NetUartM_WriteLe32(p + 16, message->context.query_id);
    p[20] = message->query_scope;
    p[21] = message->query_status;
    NetUartM_WriteLe16(p + 22, message->item_count);
    return NetUartM_BuildFrame(p, 24, 0x40, linkSequence,
                                NET_UART_M_DOWNLINK, profile, out);
}
```

<a id="admission"></a>
## 최종 송신 관측 뒤의 End

`DomainNetPriv_SendQueryEnd` 입력에는 현재 recipient·DOMAIN Boot·원 nonzero QueryID·scope와 status/count를 한 벌로 보존한다. status0/1/2/3의 의미는 [Query 감독](../UART_M_QUERY_CONTROL.md#terminal)과 같다. local enqueue·NET_SUBMIT_OK·Driver accepted 수를 ITEM_COUNT로 세지 않는다. End 자체·자발 QUERY_ID0·같은 응답 복제는 제외한다. 전 응답 최종 결과와 같은 UART 송신 순서가 입증된 뒤에 End를 제출하며 결과가 불명확하면 완료를 보류한다.

`NetSubmitResult DomainNetPriv_SendQueryEnd(const NetMobileQueryEnd *message);`는 원 선언이며 NULL/형식 오류→INVALID_ARGUMENT, 현재 Query/송신 수/순서 미확인→NOT_READY, 안정된 사본 공간 부족→NO_CAPACITY, 전체 사본 인수→OK 후보를 따른다. C2 상태 enum으로 숫자 cast하지 않는다. 실패 시 새 End 부분 접수·기존 응답 덮기 없음. 성공 후 Driver 최종 실패는 End 미완료로 남고 Query를 NetworkLocalFailure의 새 kind로 만들지 않는다.

ESP32의 기존 `EspNetPriv_OnQueryEnd`는 동일 활성 Device/Session/Boot/QueryID/scope 및 실제 유효 수신 항목과 비교한다. 이미 만료/종료/문맥 변경된 Query는 late End로 열지 않는다. count 불일치·수신 손실·중복 판정 불명확·BLE 전달 실패가 있으면 App 완료를 만들지 않는다. 실제 수신 dedup·기한·목록 부분표시 정책은 IF-O12/IF-O25/IF-O14에 남긴다.

정상 조건 예: 응답10의 최종 UART 완료8·확정 실패2가 모두 해당 Query와 같은 순서로 귀속되면 status2/count8 후보다. 반례: 완료8·미확정2에 status0/count10을 보내거나 Getter 실패를 status0/count0으로 보내지 않는다. 원 요청 기록 부재의 확인은 status1/count0이며 차량 FAILED가 아니다. 문맥 거부 status3도 현재 상대에게 안전하게 보낼 수 있는 문맥 증거가 따로 필요하다.


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

관련 기존 HOLD: UART_M_VERSION_LAYOUT_ADOPTION_OPEN, IF-O12, IF-O25, D-03, D-05; D-03/D-05·IF-O12/IF-O25는 해당 시간·Driver·소비 경계에 계속 적용한다. Interface 협의사항은 기록만 하며 S6 완료 후 실제 채택 근거와 별도 사용자 승인에 따라 통합한다.

봉인 근거: `S4C_RESULT/evidence/b08/uart_m/private.md#m_query_end`, `uart_m_contract.json`, `UART_M_ROUTING.md`, `UART_M_API_BINDINGS.md`; S4C `TIME_IDENTITY_CONTRACT.md`, `FRESHNESS_AGE_TIMEOUT.md`, `RESOURCE_QUEUE_DRIVER_FLOW.md`, `FAILURE_RECOVERY_CONTRACT.md`. 실제 원본 SHA는 제출 SOURCE_REGISTER에 기록한다.

[Data/API](#data-api) · [Wire](#wire) · [Function](#codec) · [M02 Query 감독](../UART_M_QUERY_CONTROL.md) · [Flow](../../flows/UART_M_QUERY_AND_PROJECTION.md)
