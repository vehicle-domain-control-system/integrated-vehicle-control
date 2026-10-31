# M_DOMAIN_ALIVE — DOMAIN → ESP32

`WIRE-P02-M_DOMAIN_ALIVE` / `RX-P02-M_DOMAIN_ALIVE-ESP32`. VERSION2·TYPE `0x02`·payload **12B**·frame **22B** 후보다. CF01과 양단 layout 채택 증거가 없어 현재 정상 소비는 `BLOCKED_BY_CONTRACT`이며 문서상 조건부 경로를 상세화한다. 공개 API·Wire 변경·Production 승인으로 해석하지 않는다.

[UART-M 개요](../../05_UART_M_OVERVIEW.md) · [L04 Codec](../../layers/L04_FRAME_CODEC.md) · [M06](../../modules/M06_UART_CODEC.md) · [M02](../../modules/M02_QUERY_CONTROL.md) · [전역 flow](../../flows/UART_M_QUERY_AND_PROJECTION.md)

<a id="data-api"></a>
## Data와 기존 API 경계

DTO 정본: [NetMobileDomainAlive](../../data/UART_M_DATA.md#type-NetMobileDomainAlive). RX meta: [NetUartRxMeta](../../data/UART_M_DATA.md#type-NetUartRxMeta). 공통 사본·Frame 타입: [UART Frame Data](../../data/UART_FRAME_DATA.md). C struct padding/enum 저장 폭/sizeof는 Wire가 아니며 아래 바이트 표로만 직렬화한다.

| 기존 정확한 원형 | 본체 소유/역할 | 봉인 S4A 출처 |
|---|---|---|
| `NetSubmitResult DomainNetPriv_PublishAlive(const NetMobileDomainAlive *message);` | DOMAIN Network | `api/domain_private.md#api-DomainNetPriv_PublishAlive` |
| `void EspNetPriv_OnDomainAlive(const NetMobileDomainAlive *message, const NetUartRxMeta *meta);` | ESP32 Network | `api/esp32.md#api-EspNetPriv_OnDomainAlive` |

송신 입력은 호출 동안 borrowed다. 로컬 성공 반환 전에 원 identity·필드·품질·시간 근거 전체를 독립 사본/Frame으로 확보한다. Decode의 `frame`은 호출 동안 안정된 입력, `out/meta`는 NULL 불가·서로 겹치지 않는 호출자 저장소다. 실패 OUT은 변경하지 않는다. 송신 out은 caller 소유이며 Driver 전달 이후 재사용·AGE 수정은 참조 해제 증거 전 금지한다. DOM C2는 Task/Owner 문맥, ESP32의 실제 Task/Superloop는 Owner 바인딩 전이다. ISR에서 codec·C2를 실행하지 않는다.

<a id="wire"></a>
## 정확한 payload 배치

payload B0은 Frame B8이다. 공통 envelope A5 5A·VERSION·TYPE·P·LINK_SEQUENCE·CRC는 [Frame 정본](../UART_FRAME_STREAM.md#check-frame)을 따른다. CRC-16/CCITT-FALSE는 Frame B2부터 payload 끝까지 P+6B에 적용하며 CRC 값은 LE다. 실패한 CRC나 정확한 P 불일치는 typed publish 이전에 거부한다.

| Payload byte / 폭 | 필드 → typed 멤버 | 부호·순서·범위/단위 | sentinel·예약·근거 |
|---|---|---|---|
| B0 / 4 | `DOMAIN_BOOT_ID` → `domain_boot_id` | u32 / LE; Domain 현재 기동 번호 | 0 예약 (공통 기동 규칙); T34 L221 |
| B4 / 4 | `UPDATE_SEQUENCE` → `update_sequence` | u32 / LE; Domain 생존 평가 번호 | 특수값 추가 정의 없음; T34 L222 |
| B8 / 2 | `SOURCE_AGE` → `source_age_ms` | u16 / LE; 평가 후 ms | 65535 포화 하한; 새 관측/최신성 증거 아님; T34 L223 |
| B10 / 1 | `DOMAIN_STATE` → `domain_state` | u8 / N/A; 0=INIT; 1=READY; 2=DEGRADED; 3=FAULT; 0=NET_CONTROLLER_INIT, 1=NET_CONTROLLER_READY, 2=NET_CONTROLLER_DEGRADED, 3=NET_CONTROLLER_FAULT | 특수값 추가 정의 없음; T34 L224 |
| B11 / 1 | `VALUE_QUALITY` → `value_quality` | u8 / N/A; VALUE_QUALITY; 0=NET_QUALITY_OK, 1=NET_QUALITY_STALE, 2=NET_QUALITY_INVALID, 3=NET_QUALITY_NO_DATA | 특수값 추가 정의 없음; T34 L225 |

SOURCE_AGE의 u16 값65535는 포화 하한이며 정확한 시간·fresh 증거가 아니다. 동일 생존 평가의 반복은 원 Boot/update를 보존하고 미반영 경과만 한 번 누적한다. 이 메시지에는 Device/Session·QueryID가 없다. Boot/sequence·clock reset 수용 근거가 불명확하면 해당 private 감독 admission을 HOLD로 남긴다. 정의된 INVALID/STALE/NO_DATA는 malformed와 다르다.

<a id="codec"></a>
## Typed encode/decode — M06 F17/F18

아래 함수는 `PROPOSED_PRIVATE_FUNCTION`이다. Encoder 호출자는 M04에서 확정한 해당 방향 LINK_SEQUENCE를 제공하고 Codec은 번호·시각·Session을 발급하지 않는다. Decoder 성공은 형식 DTO 확보뿐이며 현재 문맥/순서 및 소비자 성공은 [Router](../UART_M_ROUTER.md)에서 별도로 확인한다. 공통 [LE 함수](../UART_FRAME_STREAM.md#le-fields)는 명시된 각 offset에서 2/4/8B만 읽거나 쓴다.

```c
NetUartMResult NetUartM_Decode_M_DOMAIN_ALIVE(const NetUartMFrame *frame,
    NetUartMProfile profile, NetMobileDomainAlive *out, NetUartRxMeta *meta)
{
    if (frame == NULL || out == NULL || meta == NULL) return NET_UART_M_INVALID_ARGUMENT;
    NetUartMResult r = NetUartM_CheckFrame(frame, NET_UART_M_DOWNLINK, 0x02, profile);
    if (r != NET_UART_M_OK) return r;
    const uint8_t *p = frame->bytes + 8; // 검증된 payload 시작; Frame offset은 payload+8이다.
    NetMobileDomainAlive tmp = {0};
    tmp.domain_boot_id = NetUartM_ReadLe32(p + 0);
    tmp.update_sequence = NetUartM_ReadLe32(p + 4);
    tmp.source_age_ms = NetUartM_ReadLe16(p + 8);
    tmp.domain_state = p[10];
    tmp.value_quality = p[11];
    if (tmp.domain_boot_id == 0) return NET_UART_M_MALFORMED;
    if (tmp.domain_state != 0 && tmp.domain_state != 1 && tmp.domain_state != 2 && tmp.domain_state != 3) return NET_UART_M_MALFORMED;
    if (tmp.value_quality != 0 && tmp.value_quality != 1 && tmp.value_quality != 2 && tmp.value_quality != 3) return NET_UART_M_MALFORMED;
    NetUartRxMeta tmpMeta;
    tmpMeta.link_sequence = NetUartM_ReadLe16(frame->bytes + 6);
    tmpMeta.received_at_ms = frame->receivedAtMs; // callback 현재시각으로 대체하지 않는다.
    *out = tmp; *meta = tmpMeta; // 전 검사 후 동시 논리 commit; 실패 시 두 OUT 불변.
    return NET_UART_M_OK;
}
```

```c
NetUartMResult NetUartM_Encode_M_DOMAIN_ALIVE(const NetMobileDomainAlive *message,
    uint16_t linkSequence, NetUartMProfile profile, NetUartMFrame *out)
{
    if (message == NULL || out == NULL) return NET_UART_M_INVALID_ARGUMENT;
    if (message->domain_boot_id == 0) return NET_UART_M_INVALID_ARGUMENT;
    if (message->domain_state != 0 && message->domain_state != 1 && message->domain_state != 2 && message->domain_state != 3) return NET_UART_M_INVALID_ARGUMENT;
    if (message->value_quality != 0 && message->value_quality != 1 && message->value_quality != 2 && message->value_quality != 3) return NET_UART_M_INVALID_ARGUMENT;
    uint8_t p[12] = {0}; // 호출 중에만 쓰는 임시 사본; 제품 queue 크기 제안 아님.
    NetUartM_WriteLe32(p + 0, message->domain_boot_id);
    NetUartM_WriteLe32(p + 4, message->update_sequence);
    NetUartM_WriteLe16(p + 8, message->source_age_ms);
    p[10] = message->domain_state;
    p[11] = message->value_quality;
    return NetUartM_BuildFrame(p, 12, 0x02, linkSequence,
                                NET_UART_M_DOWNLINK, profile, out);
}
```

<a id="admission"></a>
## Private 링크 감독

DOMAIN Network M01/F02·M04/F11은 원 생존 평가의 Boot·update_sequence·source_age_ms·품질을 안정된 사본으로 접수한다. 반복 전송이 새 평가가 아니면 update_sequence를 올리지 않으며 AGE는 미반영 대기만 누적한다. 최종 frame을 Driver가 참조하는 동안 AGE/CRC를 수정하지 않는다. 250ms 반복/750ms 수신 중단은 후보이며 실제 clock·timeout 비교식/용량/재시도는 미검증이다.

ESP32 기존 `EspNetPriv_OnDomainAlive(message,meta)`는 정상 profile·전 필드·Boot/순서·시간 검증 뒤 private link 감시로만 전달된다. void callback 반환은 저장 완료의 증거가 아니다. DOMAIN_STATE READY와 VALUE_QUALITY OK가 와도 CAN ECU 생존, 모든 Manager 정상, BLE 연결, App ACTIVE, 차량 Permission·DONE을 생성하지 않는다. DOWN_CONTEXT를 붙이거나 Gateway SendToMobile로 BLE에 중계하지 않는다.

정상 예: Boot9/update20/age30ms/DEGRADED/STALE는 원 품질 저하를 보존하는 유효 후보다. 반례: Boot0·state4·quality4는 malformed; 뒤늦은 구 Boot ALIVE로 현 Query를 복구하지 않는다. CRC나 Session과 무관한 링크 오류를 받았다고 마지막 정상 감독시각을 연장하지 않는다.


<a id="verification"></a>
## 반례와 복구 연결

| 입력/사건 | 검사·결과 | 상태/복구 |
|---|---|---|
| 정상 후보의 정확한 P·모든 필드, profile 및 소비 바인딩의 독립 근거 존재 | Decoder→원 문맥 admission→위 기존 로컬 경계 | identity·품질·AGE 보존. 로컬 OK를 remote RX/기능 완료로 승격하지 않음 |
| frame 21B 또는 TYPE별 P 불일치 | 공통 CheckFrame 이전/내부에서 불완전·malformed 분리 | 부분 DTO/정상 liveness 갱신 없음; Stream suffix 정책 적용 |
| CRC 오류·VERSION1·반대 방향 TYPE | Frame/registry/profile에서 거부/HOLD | v1 padding으로 v2 변환하지 않음 |
| 유효 CRC 안 reserved enum·현재 문맥 불일치 | 전 field 또는 Router에서 거부 | 신뢰된 전체 frame 경계 소모; 내부 SYNC로 재분석하지 않음 |
| 동일 LINK_SEQUENCE·서로 다른 본문, wrap을 판정할 근거 없음 | ORDER_HOLD | 새 기능 실행·순번 ledger 자동진행 없음 |
| Queue 부족·Driver accepted 뒤 결과 미확정·구 callback | [공통 TX/RX](../UART_M_TX_RX_CONTROL.md)의 안정 사본·outcome·release 경계 | 접수 실패와 후발 실패 분리, 복구로 이전 Query/요청을 새 문맥에 귀속하지 않음 |

관련 기존 HOLD: UART_M_VERSION_LAYOUT_ADOPTION_OPEN, IF-O08, D-05; D-03/D-05·IF-O12/IF-O25는 해당 시간·Driver·소비 경계에 계속 적용한다. Interface 협의사항은 기록만 하며 S6 완료 후 실제 채택 근거와 별도 사용자 승인에 따라 통합한다.

봉인 근거: `S4C_RESULT/evidence/b08/uart_m/private.md#m_domain_alive`, `uart_m_contract.json`, `UART_M_ROUTING.md`, `UART_M_API_BINDINGS.md`; S4C `TIME_IDENTITY_CONTRACT.md`, `FRESHNESS_AGE_TIMEOUT.md`, `RESOURCE_QUEUE_DRIVER_FLOW.md`, `FAILURE_RECOVERY_CONTRACT.md`. 실제 원본 SHA는 제출 SOURCE_REGISTER에 기록한다.

[Data/API](#data-api) · [Wire](#wire) · [Function](#codec) · [M02 Query 감독](../UART_M_QUERY_CONTROL.md) · [Flow](../../flows/UART_M_QUERY_AND_PROJECTION.md)
