# M_QUERY — ESP32 → DOMAIN

`WIRE-P02-M_QUERY` / `RX-P02-M_QUERY-DOMAIN`. VERSION2·TYPE `0x11`·payload **32B**·frame **42B** 후보다. CF01과 양단 layout 채택 증거가 없어 현재 정상 소비는 `BLOCKED_BY_CONTRACT`이며 문서상 조건부 경로를 상세화한다. 공개 API·Wire 변경·Production 승인으로 해석하지 않는다.

[UART-M 개요](../../05_UART_M_OVERVIEW.md) · [L04 Codec](../../layers/L04_FRAME_CODEC.md) · [M06](../../modules/M06_UART_CODEC.md) · [M02](../../modules/M02_QUERY_CONTROL.md) · [전역 flow](../../flows/UART_M_UPLINK.md)

<a id="data-api"></a>
## Data와 기존 API 경계

DTO 정본: [NetMobileQuery](../../data/UART_M_DATA.md#type-NetMobileQuery). RX meta: [NetUartRxMeta](../../data/UART_M_DATA.md#type-NetUartRxMeta). 공통 사본·Frame 타입: [UART Frame Data](../../data/UART_FRAME_DATA.md). C struct padding/enum 저장 폭/sizeof는 Wire가 아니며 아래 바이트 표로만 직렬화한다.

| 기존 정확한 원형 | 본체 소유/역할 | 봉인 S4A 출처 |
|---|---|---|
| `Gateway_Status_t Gateway_Interface_SendToDomain( const Gateway_RelayMessageView_t *message);` | ESP32 Gateway/각 Core 또는 Transport Owner | `api/esp32.md#api-Gateway_Interface_SendToDomain` |
| `DomainIf_Status_t DomainNetPriv_ProcessQuery(const NetMobileQuery *message, const NetUartRxMeta *meta);` | DOMAIN Network | `api/domain_private.md#api-DomainNetPriv_ProcessQuery` |
| `DomainIf_Status_t DomainIf_ValidatePassiveQueryContext( DeviceContextId_t deviceContextId, SessionId_t sessionId);` | DOMAIN 공개 본체; Network는 호출·변환 | `api/domain_management.md#api-DomainIf_ValidatePassiveQueryContext` |

송신 입력은 호출 동안 borrowed다. 로컬 성공 반환 전에 원 identity·필드·품질·시간 근거 전체를 독립 사본/Frame으로 확보한다. Decode의 `frame`은 호출 동안 안정된 입력, `out/meta`는 NULL 불가·서로 겹치지 않는 호출자 저장소다. 실패 OUT은 변경하지 않는다. 송신 out은 caller 소유이며 Driver 전달 이후 재사용·AGE 수정은 참조 해제 증거 전 금지한다. DOM C2는 Task/Owner 문맥, ESP32의 실제 Task/Superloop는 Owner 바인딩 전이다. ISR에서 codec·C2를 실행하지 않는다.

<a id="wire"></a>
## 정확한 payload 배치

payload B0은 Frame B8이다. 공통 envelope A5 5A·VERSION·TYPE·P·LINK_SEQUENCE·CRC는 [Frame 정본](../UART_FRAME_STREAM.md#check-frame)을 따른다. CRC-16/CCITT-FALSE는 Frame B2부터 payload 끝까지 P+6B에 적용하며 CRC 값은 LE다. 실패한 CRC나 정확한 P 불일치는 typed publish 이전에 거부한다.

| Payload byte / 폭 | 필드 → typed 멤버 | 부호·순서·범위/단위 | sentinel·예약·근거 |
|---|---|---|---|
| B0 / 4 | `DEVICE_CONTEXT_ID` → `context.device_context_id` | u32 / LE; 현재 단말0예약 | 0 예약; T34 L182 |
| B4 / 8 | `SESSION_ID` → `context.session_id` | u64 / LE; 현재 Session; 발급/0사용규칙 OPEN | 특수값 추가 정의 없음; T34 L182 |
| B12 / 4 | `QUERY_ID` → `query_id` | u32 / LE; 이번 조회 식별, 0 금지 | 0 금지; T34 L183 |
| B16 / 1 | `QUERY_SCOPE` → `query_scope` | u8 / N/A; 0=현재 상태; 1=요청 결과; 2=경고; 3=전체; 0=NET_QUERY_SCOPE_STATE, 1=NET_QUERY_SCOPE_RESULT, 2=NET_QUERY_SCOPE_WARNING, 3=NET_QUERY_SCOPE_ALL | 특수값 추가 정의 없음; T34 L184 |
| B17 / 1 | `RESERVED` → `NONE (Wire RESERVED 전용)` | u8 / N/A; 0 | 0만 사용 (후보); T34 L185 |
| B18 / 8 | `REQUEST_SESSION` → `request_session` | u64 / LE; 조회할 원 요청 Session; 상태 조회면 0 | 특수값 추가 정의 없음; T34 L186 |
| B26 / 4 | `REQUEST_ID` → `request_id` | u32 / LE; 조회할 요청; 상태 조회면 0 | 특수값 추가 정의 없음; T34 L187 |
| B30 / 2 | `CURSOR` → `cursor` | u16 / LE; 이 초안에서는 0 | 0만 사용 (후보); T34 L188 |

이 메시지에는 원 AGE 필드가 없다. LINK_SEQUENCE·로컬 수신시각을 원 생성 AGE 또는 source TTL로 대체하지 않는다. 원 Request/Query/Occurrence와 현재·과거 문맥을 보존한다. sequence wrap·clock reset·Session0의 실제 수용 정책이 불명확하면 해당 admission을 HOLD로 남긴다.

<a id="codec"></a>
## Typed encode/decode — M06 F17/F18

아래 함수는 `PROPOSED_PRIVATE_FUNCTION`이다. Encoder 호출자는 M04에서 확정한 해당 방향 LINK_SEQUENCE를 제공하고 Codec은 번호·시각·Session을 발급하지 않는다. Decoder 성공은 형식 DTO 확보뿐이며 현재 문맥/순서 및 소비자 성공은 [Router](../UART_M_ROUTER.md)에서 별도로 확인한다. 공통 [LE 함수](../UART_FRAME_STREAM.md#le-fields)는 명시된 각 offset에서 2/4/8B만 읽거나 쓴다.

```c
NetUartMResult NetUartM_Decode_M_QUERY(const NetUartMFrame *frame,
    NetUartMProfile profile, NetMobileQuery *out, NetUartRxMeta *meta)
{
    if (frame == NULL || out == NULL || meta == NULL) return NET_UART_M_INVALID_ARGUMENT;
    NetUartMResult r = NetUartM_CheckFrame(frame, NET_UART_M_UPLINK, 0x11, profile);
    if (r != NET_UART_M_OK) return r;
    const uint8_t *p = frame->bytes + 8; // 검증된 payload 시작; Frame offset은 payload+8이다.
    NetMobileQuery tmp = {0};
    tmp.context.device_context_id = NetUartM_ReadLe32(p + 0);
    tmp.context.session_id = NetUartM_ReadLe64(p + 4);
    tmp.query_id = NetUartM_ReadLe32(p + 12);
    tmp.query_scope = p[16];
    if (p[17] != 0) return NET_UART_M_MALFORMED; // RESERVED
    tmp.request_session = NetUartM_ReadLe64(p + 18);
    tmp.request_id = NetUartM_ReadLe32(p + 26);
    tmp.cursor = NetUartM_ReadLe16(p + 30);
    if (tmp.context.device_context_id == 0) return NET_UART_M_MALFORMED;
    if (tmp.query_id == 0) return NET_UART_M_MALFORMED;
    if (tmp.query_scope != 0 && tmp.query_scope != 1 && tmp.query_scope != 2 && tmp.query_scope != 3) return NET_UART_M_MALFORMED;
    if (tmp.cursor != 0) return NET_UART_M_MALFORMED;
    if ((tmp.query_scope == 0 && (tmp.request_session != 0 || tmp.request_id != 0))) return NET_UART_M_MALFORMED;
    NetUartRxMeta tmpMeta;
    tmpMeta.link_sequence = NetUartM_ReadLe16(frame->bytes + 6);
    tmpMeta.received_at_ms = frame->receivedAtMs; // callback 현재시각으로 대체하지 않는다.
    *out = tmp; *meta = tmpMeta; // 전 검사 후 동시 논리 commit; 실패 시 두 OUT 불변.
    return NET_UART_M_OK;
}
```

```c
NetUartMResult NetUartM_Encode_M_QUERY(const NetMobileQuery *message,
    uint16_t linkSequence, NetUartMProfile profile, NetUartMFrame *out)
{
    if (message == NULL || out == NULL) return NET_UART_M_INVALID_ARGUMENT;
    if (message->context.device_context_id == 0) return NET_UART_M_INVALID_ARGUMENT;
    if (message->query_id == 0) return NET_UART_M_INVALID_ARGUMENT;
    if (message->query_scope != 0 && message->query_scope != 1 && message->query_scope != 2 && message->query_scope != 3) return NET_UART_M_INVALID_ARGUMENT;
    if (message->cursor != 0) return NET_UART_M_INVALID_ARGUMENT;
    if ((message->query_scope == 0 && (message->request_session != 0 || message->request_id != 0))) return NET_UART_M_INVALID_ARGUMENT;
    uint8_t p[32] = {0}; // 호출 중에만 쓰는 임시 사본; 제품 queue 크기 제안 아님.
    NetUartM_WriteLe32(p + 0, message->context.device_context_id);
    NetUartM_WriteLe64(p + 4, message->context.session_id);
    NetUartM_WriteLe32(p + 12, message->query_id);
    p[16] = message->query_scope;
    p[17] = 0; // RESERVED, 공개 typed 멤버를 만들지 않는다.
    NetUartM_WriteLe64(p + 18, message->request_session);
    NetUartM_WriteLe32(p + 26, message->request_id);
    NetUartM_WriteLe16(p + 30, message->cursor);
    return NetUartM_BuildFrame(p, 32, 0x11, linkSequence,
                                NET_UART_M_UPLINK, profile, out);
}
```

<a id="admission"></a>
## Query 진입과 수명

현재 Device/Session과 과거 REQUEST_SESSION/REQUEST_ID를 분리한다. 상태 scope0은 과거 두 필드를 0으로 제한한다. scope1/2/3의 세부 조합·권한·목록 선정은 IF-O25의 Owner 정책이며 decoder가 자동 보정하지 않는다. CURSOR와 RESERVED는0인 현재 후보만 지원하며 pagination opcode를 새로 만들지 않는다.

DOMAIN M02/F04의 기존 `DomainNetPriv_ProcessQuery`는 선행 M_CONTEXT와 동일 Owner 직렬 처리 문맥에서 `DomainIf_ValidatePassiveQueryContext(deviceContextId,sessionId)`를 먼저 호출한다. 최초 SESSION_READY0만으로 거부하지 않는다. passive 성공은 Session 발급·활성화가 아니다. 18 Getter의 정확한 원형·성공 OUT·부분 상태·최종 송신 수는 [M02 함수](../UART_M_QUERY_CONTROL.md)에 정의한다.

정상 예: 현재 Session20에서 과거 Request Session10/ID30을 조회할 때 응답 recipient는20, 결과의 원 Request Session은10을 보존한다. 반례: QUERY_ID0, CURSOR1, RESERVED1은 malformed다. Getter 오류의 이전 OUT, scope0의 과거 RequestID, late End를 신규 Query로 처리하는 경로는 거부/보류한다. Query를 CAN 명령·새 센서 관측으로 변환하지 않는다.


<a id="verification"></a>
## 반례와 복구 연결

| 입력/사건 | 검사·결과 | 상태/복구 |
|---|---|---|
| 정상 후보의 정확한 P·모든 필드, profile 및 소비 바인딩의 독립 근거 존재 | Decoder→원 문맥 admission→위 기존 로컬 경계 | identity·품질·AGE 보존. 로컬 OK를 remote RX/기능 완료로 승격하지 않음 |
| frame 41B 또는 TYPE별 P 불일치 | 공통 CheckFrame 이전/내부에서 불완전·malformed 분리 | 부분 DTO/정상 liveness 갱신 없음; Stream suffix 정책 적용 |
| CRC 오류·VERSION1·반대 방향 TYPE | Frame/registry/profile에서 거부/HOLD | v1 padding으로 v2 변환하지 않음 |
| 유효 CRC 안 reserved enum·현재 문맥 불일치 | 전 field 또는 Router에서 거부 | 신뢰된 전체 frame 경계 소모; 내부 SYNC로 재분석하지 않음 |
| 동일 LINK_SEQUENCE·서로 다른 본문, wrap을 판정할 근거 없음 | ORDER_HOLD | 새 기능 실행·순번 ledger 자동진행 없음 |
| Queue 부족·Driver accepted 뒤 결과 미확정·구 callback | [공통 TX/RX](../UART_M_TX_RX_CONTROL.md)의 안정 사본·outcome·release 경계 | 접수 실패와 후발 실패 분리, 복구로 이전 Query/요청을 새 문맥에 귀속하지 않음 |

관련 기존 HOLD: UART_M_VERSION_LAYOUT_ADOPTION_OPEN, IF-O08, IF-O23, IF-O25, D-05; D-03/D-05·IF-O12/IF-O25는 해당 시간·Driver·소비 경계에 계속 적용한다. Interface 협의사항은 기록만 하며 S6 완료 후 실제 채택 근거와 별도 사용자 승인에 따라 통합한다.

봉인 근거: `S4C_RESULT/evidence/b08/uart_m/uplink.md#m_query`, `uart_m_contract.json`, `UART_M_ROUTING.md`, `UART_M_API_BINDINGS.md`; S4C `TIME_IDENTITY_CONTRACT.md`, `FRESHNESS_AGE_TIMEOUT.md`, `RESOURCE_QUEUE_DRIVER_FLOW.md`, `FAILURE_RECOVERY_CONTRACT.md`. 실제 원본 SHA는 제출 SOURCE_REGISTER에 기록한다.

[Data/API](#data-api) · [Wire](#wire) · [Function](#codec) · [M02 Query 감독](../UART_M_QUERY_CONTROL.md) · [Flow](../../flows/UART_M_UPLINK.md)
