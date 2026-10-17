# M_DIGITAL_STATUS — DOMAIN 보고의 UART-M 수직 경로

`WIRE-P02-M_DIGITAL_STATUS` / `RX-P02-M_DIGITAL_STATUS-ESP32`. DOMAIN→ESP32, P02, Type `0x23`, VERSION2 **후보**, payload **30B**, frame **40B**. 정적 설계 상태 `DESIGNED_CANDIDATE`; CF01 profile 채택 증거가 없는 현재 정상 기능 소비는 제한한다.

[UART-M 개요](../../05_UART_M_OVERVIEW.md) → [L02](../../layers/L02_TYPED_HANDOFF.md) / [L03](../../layers/L03_MESSAGE_SERVICE.md) / [L04](../../layers/L04_FRAME_CODEC.md) → [M03](../../modules/M03_TYPED_HANDOFF.md) / [M04](../../modules/M04_MESSAGE_SERVICE.md) / [M06](../../modules/M06_UART_CODEC.md) → 본 함수 → [필드 정본](../../data/UART_M_REPORT_FIELDS.md#m_digital_status) → [하향 Flow](../../flows/UART_M_DOWNLINK.md).

## 기존 공개·콜백·private 경계

```c
DomainIf_Status_t DomainIf_TxMobileDigitalKeyState( const DigitalKeyState_t *state);
DomainIf_Status_t (*txMobileDigitalKeyState)( const DigitalKeyState_t *state);
NetSubmitResult DomainNetPriv_SubmitMobileDigitalKeyState(const DigitalKeyState_t *state, const DomainQueryResponseContext_t *query);
Gateway_Status_t Gateway_Interface_OnDomainMessage( const Gateway_RelayMessageView_t *message);
Gateway_Status_t Gateway_Interface_SendToMobile( const Gateway_RelayMessageView_t *message);
```

`DomainIf_TxMobileDigitalKeyState` 공개 본체와 Manager 상태는 DOMAIN 소유다. Wrapper가 등록한 `txMobileDigitalKeyState`의 **콜백 본체**와 `DomainNetPriv_SubmitMobileDigitalKeyState`의 Adapter/Codec 경로는 DOMAIN MCU의 Network 책임이다(M03/F07, M04/F11·F12, M06/F17·F18). Application이 공개 Wrapper와 private submit을 이중 호출하지 않는다. Query 생산자 M02/F04도 같은 private submit을 사용하고 query!=NULL을 넘긴다.

`state`는 `const DigitalKeyState_t *` IN으로 NULL 불가, query는 `const DomainQueryResponseContext_t *` IN으로 NULL이면 자발 QUERY_ID0, non-NULL이면 검증된 현재 recipient·원 QueryID·scope다. 둘 다 호출 동안 borrowed이며 반환 후 포인터를 저장하지 않는다. 콜백 성공 전 전체 값·현재 수신 문맥·원 identity·AGE 기준시각을 함께 사본으로 확보한다. Query Getter의 실패 OUT은 읽지 않는다. [C2 입력 선언](../../data/UART_M_DATA.md#type-DigitalKeyState_t) · [Codec DTO](../../data/UART_M_DATA.md#type-NetMobileDigitalStatus) · [Query 문맥](../../data/UART_M_DATA.md#type-DomainQueryResponseContext_t).

NetSubmitResult `OK`는 전체 사본의 로컬 접수만, `INVALID_ARGUMENT`는 입력·표현 오류, `NOT_READY`는 profile·문맥·sink 미준비, `NO_CAPACITY`는 완전 사본 공간 없음이다. DOMAIN_IF의 `OK/INVALID_ARGUMENT/NOT_INITIALIZED/NOT_AVAILABLE/QUEUE_FULL/REJECTED/UNSUPPORTED`와 Gateway status를 숫자 cast하지 않는다. 실제 바인딩된 반환 의미로 전달하며 미정 mapping은 HOLD다. 모든 C2 호출은 Task/Owner 문맥이고 ISR에서 본문 해석·공개 함수를 호출하지 않는다.

ESP32 측 `Gateway_Interface_OnDomainMessage` 및 Core→`SendToMobile`은 **상대 노드의 기존 소비 계약**이다. DOMAIN Network가 원격 C 함수를 호출하거나 ESP32 내부 구현을 작성하는 뜻이 아니다. Gateway ByteView의 TYPE+논리 payload convention은 IF-O14/IF-O23 후보이며 해당 subtype·context 바인딩 증거가 없으면 인계하지 않는다. nested byte는 콜백 반환까지 유효하고 비동기 수용 전 전체 복사가 필요하다. UART SYNC/CRC를 BLE payload로 복제하지 않는다.

<a id="conversion"></a>
## 생산 자료를 잃지 않는 변환

설정(setting)과 실행 가용성(availability)은 별도 축이다. OFF를 NO_DATA로 만들거나 ON을 사용 가능으로 추론하지 않는다. setting OFF/ON→raw0/1, availability AVAILABLE/DEGRADED/UNAVAILABLE→raw0/1/2를 명시 변환하고 UNSUPPORTED는 HOLD다. `reason`은 공통 reason 의미 mapping, `meta.updateSequence/ageMs`는 원 평가 번호/경과다. `lastUnlockOrigin`은 S4A에서 Domain internal observation으로 분류된 별도 관측이며 이 상태 프레임이 전송하거나 실제 unlock 결과를 증명한다고 주장하지 않는다. 원 meta 품질을 raw 상태로 표현할 수 없다면 해당 출력은 제한한다.


각 변환은 현재 recipient/DOMAIN Boot/QueryID를 먼저 확인한 owner 직렬 처리에서 임시 DTO에 수행한다. 제출 직전까지 실제 증명된 미반영 경과만 누적한다. AGE65535는 포화 하한이며 최신성이 아니고 원 값과 순번을 바꾸지 않는다. 시간원·wrap·callback 세대가 미확인이면 `TIME_HOLD/ORDER_HOLD`로 남기며0ms로 채우지 않는다. [공통 송수신·Driver 수명](../UART_M_TX_RX_CONTROL.md)과 [Router](../UART_M_ROUTER.md)의 문맥 검사가 모두 필요하다.

<a id="prepare"></a>
## Network 내부 준비 함수

**PROPOSED_PRIVATE_FUNCTION**. M03/F07의 기존 submit 내부 변환을 상세화한 제안이며 새 공개 API가 아니다. source/context는 호출 동안 borrowed IN, out은 호출자 소유 OUT이고 서로 겹치지 않는다. NULL→INVALID_ARGUMENT, 시간 근거 미확인→TIME_HOLD, 표현/바인딩 미확인→CONTRACT_HOLD, 범위/중복 오류→MALFORMED이며 실패 OUT은 불변이다. OK는 Net DTO가 만들어졌다는 뜻으로 Queue 접수나 Driver 전송을 대신하지 않는다.

context는 상위 검증에서 얻은 현재 recipient·DOMAIN Boot·QueryID다. 이 함수 안의 예약0 검사는 현재 peer/Session 증거의 대체가 아니다. uncountedElapsedMs는 원 API/Getter AGE에 아직 더하지 않은 동일 노드 경과이고 timeBasisVerified는 실제 시간원·wrap·리셋 검증의 내부 결과다. 호출자가 임의 true로 채우는 승인 스위치가 아니다. 그 바인딩이 없는 현재 경로는 TIME_HOLD를 유지한다. 이후 Queue 대기는 다른 구간으로 단 한 번만 누적한다.

공통 enum mapping과 포화 산술은 [M_RESULT의 단일 정의](M_RESULT.md#report-mapping)를 사용한다. 각 switch의 default는 없는 의미를 NONE/OK로 바꾸지 않는 차단 분기다.

```c
NetUartMResult NetUartM_PrepareDigitalStatus(const DigitalKeyState_t *source,
    const NetDownContext *context, uint32_t uncountedElapsedMs,
    bool timeBasisVerified, NetMobileDigitalStatus *out)
{
    if (source == NULL || context == NULL || out == NULL)
        return NET_UART_M_INVALID_ARGUMENT;
    if (!timeBasisVerified) return NET_UART_M_TIME_HOLD;
    if (context->recipient.device_context_id == 0 || context->domain_boot_id == 0)
        return NET_UART_M_CONTEXT_HOLD;
    // 현재 peer/Session 및 query 수용은 상위 Router의 실제 검증 근거가 선행한다.
    NetMobileDigitalStatus tmp = {0};
    tmp.context = *context;
    NetUartMResult mapped;
    if (source->meta.quality != DATA_QUALITY_OK) return NET_UART_M_CONTRACT_HOLD;
    switch (source->setting) {
    case DIGITAL_KEY_SETTING_OFF: tmp.digital_key_setting_state = NET_FLAG_NO; break;
    case DIGITAL_KEY_SETTING_ON: tmp.digital_key_setting_state = NET_FLAG_YES; break;
    default: return NET_UART_M_CONTRACT_HOLD;
    }
    mapped = NetUartM_ReportMapAvailability(source->availability, &tmp.digital_key_availability);
    if (mapped != NET_UART_M_OK) return mapped;
    mapped = NetUartM_ReportMapReason(source->reason, &tmp.reason);
    if (mapped != NET_UART_M_OK) return mapped;
    // lastUnlockOrigin은 이 Wire의 필드가 아닌 Domain 내부 관측이다.
    tmp.update_sequence = source->meta.updateSequence;
    tmp.source_age_ms = NetUartM_ReportAge(source->meta.ageMs, uncountedElapsedMs);
    *out = tmp; // 불완전 변환은 호출자 OUT에 보이지 않는다.
    return NET_UART_M_OK;
}
```


<a id="codec"></a>
## 명시 byte Encoder / Decoder

아래 두 함수는 **PROPOSED_PRIVATE_FUNCTION**이다. 기존 공개 API나 제품 C 코드가 아니다. Encoder의 message는 위 변환에서 생성한 Net raw 의미 DTO이며 frame OUT은 호출자 소유다. linkSequence는 송신 방향의 순번으로 원 Request/Occurrence와 별개다. profile enum값만으로 양단 채택을 증명하지 않는다. Decoder의 frame은 안정된 입력 사본이고 out/meta는 서로 겹치지 않는 호출자 OUT이다. 실패 시 out/meta 불변, 성공은 형식 검증 완료만 뜻하며 recipient 수용·Gateway/Manager commit은 후속이다.

[CheckFrame/BuildFrame](../UART_FRAME_STREAM.md#check-frame)는 정확 Type·P·VERSION·방향·CRC·profile 근거를 검사하고 [LE 도우미](../UART_FRAME_STREAM.md#le-fields)는 지정 byte폭만 처리한다. 모든 enum 범위를 먼저 검증한 다음 정본 offset을 하나씩 읽고 쓴다. struct memcpy/sizeof 전송은 없다.

```c
NetUartMResult NetUartM_EncodeDigitalStatus(const NetMobileDigitalStatus *message,
    uint16_t linkSequence, NetUartMProfile profile, NetUartMFrame *out)
{
    if (message == NULL || out == NULL) return NET_UART_M_INVALID_ARGUMENT;
    // Net DTO의 raw 의미를 검증한다. C2 enum 숫자를 이 DTO로 cast하지 않는다.
    if (message->context.recipient.device_context_id == 0) return NET_UART_M_MALFORMED;
    if (message->context.domain_boot_id == 0) return NET_UART_M_MALFORMED;
    if (message->digital_key_setting_state > 1) return NET_UART_M_MALFORMED;
    if (message->digital_key_availability > 2) return NET_UART_M_MALFORMED;
    if (message->reason > 25) return NET_UART_M_MALFORMED;
    uint8_t payload[30] = {0};
    NetUartM_WriteLe32(&payload[0], message->context.recipient.device_context_id);
    NetUartM_WriteLe64(&payload[4], message->context.recipient.session_id);
    NetUartM_WriteLe32(&payload[12], message->context.domain_boot_id);
    NetUartM_WriteLe32(&payload[16], message->context.query_id);
    payload[20] = message->digital_key_setting_state;
    payload[21] = message->digital_key_availability;
    NetUartM_WriteLe16(&payload[22], message->reason);
    NetUartM_WriteLe32(&payload[24], message->update_sequence);
    NetUartM_WriteLe16(&payload[28], message->source_age_ms);
    // BuildFrame가 정확한 P·방향·profile 채택 근거를 검사하고 CRC를 붙인다.
    return NetUartM_BuildFrame(payload, 30, 0x23, linkSequence,
        NET_UART_M_DOWNLINK, profile, out);
}
```

```c
NetUartMResult NetUartM_DecodeDigitalStatus(const NetUartMFrame *frame,
    NetUartMProfile profile, NetMobileDigitalStatus *out, NetUartRxMeta *meta)
{
    if (frame == NULL || out == NULL || meta == NULL)
        return NET_UART_M_INVALID_ARGUMENT;
    NetUartMResult checked = NetUartM_CheckFrame(frame, NET_UART_M_DOWNLINK,
        0x23, profile);
    if (checked != NET_UART_M_OK) return checked;
    const uint8_t *payload = &frame->bytes[8];
    NetMobileDigitalStatus tmp = {0};
    tmp.context.recipient.device_context_id = NetUartM_ReadLe32(&payload[0]);
    tmp.context.recipient.session_id = NetUartM_ReadLe64(&payload[4]);
    tmp.context.domain_boot_id = NetUartM_ReadLe32(&payload[12]);
    tmp.context.query_id = NetUartM_ReadLe32(&payload[16]);
    tmp.digital_key_setting_state = payload[20];
    tmp.digital_key_availability = payload[21];
    tmp.reason = NetUartM_ReadLe16(&payload[22]);
    tmp.update_sequence = NetUartM_ReadLe32(&payload[24]);
    tmp.source_age_ms = NetUartM_ReadLe16(&payload[28]);
    if (tmp.context.recipient.device_context_id == 0) return NET_UART_M_MALFORMED;
    if (tmp.context.domain_boot_id == 0) return NET_UART_M_MALFORMED;
    if (tmp.digital_key_setting_state > 1) return NET_UART_M_MALFORMED;
    if (tmp.digital_key_availability > 2) return NET_UART_M_MALFORMED;
    if (tmp.reason > 25) return NET_UART_M_MALFORMED;
    NetUartRxMeta tmpMeta = {0};
    tmpMeta.link_sequence = NetUartM_ReadLe16(&frame->bytes[6]);
    tmpMeta.received_at_ms = frame->receivedAtMs;
    // 전체 필드가 유효한 뒤 한 번에 공개한다. Session·순서·소비자 판정은 Router 단계다.
    *out = tmp;
    *meta = tmpMeta;
    return NET_UART_M_OK;
}
```

<a id="admission"></a>
## 접수·Driver·수신 소비 순서

1. 기존 public Wrapper→TxPort 또는 성공 Getter→private submit이 원 자료를 제공한다. CF01·해당 표현 HOLD·현재 recipient 확인을 먼저 수행한다. 사본·현재 Query 문맥이 없으면 NOT_READY 또는 입력 오류로 돌아가고 접수 기록을 남기지 않는다.
2. 완전 사본 확보 후에만 로컬 OK를 반환한다. discrete 결과·사건은 다른 항목으로 덮지 않는다. 상태 교체도 같은 key의 미제출 사본으로 한정하고 Driver 참조 중인 frame은 변경하지 않는다. 용량 부족이면 새 항목 전체를 거부한다.
3. dispatch 시 원 AGE에 그동안 미반영된 대기만 더한다. 위 Encoder로 새 안정된 frame을 만든 뒤 Driver에 인계한다. Driver accepted, 최종 UART TX outcome, 참조 해제는 별도 관측이다. 모호한 submit은 같은 항목의 자동 재시도 근거가 아니다.
4. 결과·사건의 실제 후발 실패가 기존 REQUEST_RESULT/OCCURRENCE 범위에 해당하면 원 identity로 F09 필수 sink에 연결한다. NETWORK_LOCAL_FAILURE 세 reason의 시점 조건을 만족해야 하며 차량 FAILED를 합성하지 않는다. Query 자체 실패나 일반 주기 손실을 해당 kind로 위장하지 않는다. 필요한 sink가 미연결이면 최초 OK를 약속하지 않는다.
5. ESP32 문서상 경계는 byte stream→완전 Frame→위 Decoder→현재 Session/Boot·순서/중복·품질/AGE→허용 subtype Adapter→OnDomainMessage다. 형식·문맥·소비 거부이면 정상 liveness·조회 수를 갱신하지 않는다. CRC 정상이면서 의미가 잘못된 Frame은 전체 P+10 경계를 소비하여 payload 내부 SYNC를 재해석하지 않는다.
6. Query 응답수는 enqueue나 Encoder OK가 아니라 같은 원 Query의 확인된 **최종 로컬 UART TX 완료**만 센다. End 자체·자발 QUERY_ID0·중복은 제외한다. ESP32 수신·BLE 전송·App 표시 성공은 별도이다. [Query 경로](../../flows/UART_M_QUERY_AND_PROJECTION.md)를 적용한다.

## 정상·반례 전문 검토

정상 조건부 사례: setting ON/raw 1이고 availability UNAVAILABLE/raw 2인 상태를 그대로 보고한다. UART 정상 통신이 디지털키 사용 허용의 근거가 되지 않는다.

| 입력·사건 | 차단 위치·상태 보존 |
|---|---|
| P가 29 또는 31, 반대 방향 Type/CF01 v1 | CheckFrame가 exact registry/profile에서 거부, typed OUT·정상 liveness 변경 없음 |
| CRC 불일치·부분 Frame | Stream이 완전 후보를 확보하기 전 Decoder/소비 호출 없음; 신뢰 못하는 시작 다음 byte부터 재동기화 |
| 예약 Device 0/DOMAIN Boot 0·불법 enum | Decoder의 임시 DTO에서 MALFORMED, out/meta 불변 |
| 이전 Session 또는 오래된 DOMAIN Boot | Router CONTEXT_HOLD, 원 요청 Session을 현재 값으로 덮지 않음 |
| 동일 LINK_SEQUENCE와 동일 전체 bytes | 완전 identity 저장이 입증된 경우만 중복 무동작. wrap 또는 같은 순번의 다른 본문은 ORDER_HOLD |
| 원 메시지에서 표현 가능한 STALE/INVALID/NO_DATA | 정상 형식의 열화 보고와 malformed를 구분한다. 해당 필드가 없으면 가짜 품질을 추가하지 않고 변환 제한 적용 |
| borrowed 자료가 콜백 중 변경·사본 공간 부족 | 원자 사본 근거 미확인 또는 NO_CAPACITY, 부분 접수·미초기화 필드 참조 금지 |
| 접수 뒤 실패·구 callback·release 미확인 | 원 항목 outcome과 참조 수명 보존, 중복 해제·재송신·Query count 반영 금지 |
| ON을 AVAILABLE로 승격, meta STALE의 AGE 초기화, UNSUPPORTED의 C2 코드 3 송신, 재전송마다 updateSequence 증가 | 위 변환 단계/Router에서 해당 출력 HOLD 또는 정본 raw 오류 거부. 독립된 다른 메시지는 해당 계약대로 처리 |

주기·감시 기록 `1000ms / 3000ms`는 B08 T34:94 후보이며 실제 TTL·Driver 측정이 아니다. 사건형 메시지의 무발생에는 timeout을 만들지 않는다. 정상 재접속도 과거 결과를 새 차량 실행으로 만들지 않는다.

## 원문·미결·되돌아가기

원문: `S4C_RESULT/evidence/b08/uart_m/domain_reports.md#m_digital_status` 및 `uart_m_contract.json`; S4A `data/T15.md#type-NetMobileDigitalStatus`, `api/domain_tx.md#api-DomainIf_TxMobileDigitalKeyState`, `api/domain_submit.md#api-DomainNetPriv_SubmitMobileDigitalKeyState`, `api/callbacks.md#api-txMobileDigitalKeyState`, `api/esp32.md#api-Gateway_Interface_OnDomainMessage`. B09 `TIME_IDENTITY_CONTRACT.md`, `FAILURE_RECOVERY_CONTRACT.md`, `RESOURCE_QUEUE_DRIVER_FLOW.md`가 과거 B08의 미착수 문구보다 현재 시간·실패 설명에 우선한다.

기존 HOLD `UART_M_VERSION_LAYOUT_ADOPTION_OPEN, IF-O02, IF-O23, C2-H01` 및 CF01·IF-O08/09/12/14/25·D-03/05·O-C10을 유지한다. 실제 합의 변경은 S6 이후 승인된 통합 반영 대상이며 이 문서가 Interface를 수정하거나 팀 승인을 부여하지 않는다.

[Data/Wire 정본](../../data/UART_M_REPORT_FIELDS.md#m_digital_status) · [M06](../../modules/M06_UART_CODEC.md) · [M02](../../modules/M02_QUERY_CONTROL.md) · [하향 Flow](../../flows/UART_M_DOWNLINK.md) · [상위 개요](../../05_UART_M_OVERVIEW.md)
