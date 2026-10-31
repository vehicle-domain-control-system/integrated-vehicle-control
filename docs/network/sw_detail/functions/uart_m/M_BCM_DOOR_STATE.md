# M_BCM_DOOR_STATE — 원 CAN 관측의 UART-M 투영

`WIRE-P02-M_BCM_DOOR_STATE` / `RX-P02-M_BCM_DOOR_STATE-ESP32` · DOMAIN → ESP32 · Type `0x30` · VERSION2 후보 · payload 40B / frame 50B. `DESIGNED_CANDIDATE`: 양단 채택·원본 association 미검증인 정상 기능 경로는 HOLD다.

[전체 UART-M](../../05_UART_M_OVERVIEW.md) → [M06 Codec](../../modules/M06_UART_CODEC.md) → [이 메시지 필드](../../data/UART_M_PROJECTION_FIELDS.md#m_bcm_door_state) → [원 CAN 상세](../can/BCM_DOOR_STATE.md) → [원본 투영 흐름](../../flows/UART_M_CAN_PROJECTION.md). [M02 Query](../../modules/M02_QUERY_CONTROL.md) · [공통 Router](../UART_M_ROUTER.md) · [송수신·Driver](../UART_M_TX_RX_CONTROL.md).

## 책임·자료·수명

L01/M01/F01~F03은 원 입력 연결, L02/M03/F07·F10은 TxPort/상대 Gateway 경계, L01/M02/F04·F05는 Query, L03/M04/F11~F13은 접수·순서, L04/M06/F17·F18은 typed Codec, L05/M07·M08/F19~F23은 저장소·Driver 수명, M09/F24·F25는 실제 관측이다. 기존5 Layer·9 Module·25 책임 수를 바꾸지 않는다.

DOMAIN 공개 Wrapper/Getter 본체와 Manager 상태는 DOMAIN 소유다. Network는 등록된 TxPort callback 본체에서 해당 private submit을 호출한다. ISR은 이 API/Codec/Getter를 호출하지 않는다. DOMAIN 초기 경로는 Network Superloop의 ISR 밖 단일 소유자 문맥에서 직렬 처리한다. 다른 ECU는 담당자가 합의한 Task 또는 Superloop 문맥을 사용한다. ESP32 내부 Gateway/BLE 구현은 ESP32 담당자 소유이며 아래 수신 경계는 계약 추적이다.

자료 정본: [NetMobileBcmDoorState](../../data/UART_M_DATA.md#type-NetMobileBcmDoorState), [DoorState_t](../../data/CAN_BCM_DATA.md#type-DoorState_t), [NetCanSource](../../data/CIS_REAR_DATA.md#type-NetCanSource), [DomainQueryResponseContext_t](../../data/UART_M_DATA.md#type-DomainQueryResponseContext_t), [NetUartRxMeta](../../data/UART_M_DATA.md#type-NetUartRxMeta), [NetSubmitResult](../../data/CIS_REAR_DATA.md#type-NetSubmitResult), [DomainIf_Status_t](../../data/CIS_REAR_DATA.md#type-DomainIf_Status_t). private Frame/결과는 [UART Frame 자료](../../data/UART_FRAME_DATA.md).

`state`/`query`/`message`/`frame`은 호출 동안 빌린 IN이며 NULL 허용은 `query`만이다. Query Getter의 OUT은 성공한 경우만 읽는다. 새로운 private Codec의 입력과 모든 OUT은 서로 겹치지 않으며 실패 시 OUT을 유지한다. local OK 전에 DTO·원본 연계·시간 기준·중첩 바이트 전체를 자기 소유로 확보한다. 부모 callback table 값 복사는 pointed buffer 수명의 연장이 아니다.

## 기존 C 원형과 소유자

<a id="api-DomainIf_TxMobileBcmDoorState"></a>
`DomainIf_TxMobileBcmDoorState` — DOMAIN (공개 본체 소유 합의 완료), 근거 `S4A api/domain_tx.md#api-DomainIf_TxMobileBcmDoorState`.

```c
DomainIf_Status_t DomainIf_TxMobileBcmDoorState(const DoorState_t *state);
```

<a id="api-DomainNetPriv_SubmitMobileBcmDoorState"></a>
`DomainNetPriv_SubmitMobileBcmDoorState` — DOMAIN Network, 근거 `S4A api/domain_submit.md#api-DomainNetPriv_SubmitMobileBcmDoorState`.

```c
NetSubmitResult DomainNetPriv_SubmitMobileBcmDoorState(const DoorState_t *state, const DomainQueryResponseContext_t *query);
```

<a id="api-Gateway_Interface_OnDomainMessage"></a>
`Gateway_Interface_OnDomainMessage` — ESP32 Gateway/각 Core 또는 Transport Owner, 근거 `S4A api/esp32.md#api-Gateway_Interface_OnDomainMessage`.

```c
Gateway_Status_t Gateway_Interface_OnDomainMessage( const Gateway_RelayMessageView_t *message);
```

<a id="api-txMobileBcmDoorState"></a>
`txMobileBcmDoorState` — DOMAIN Network callback 본체, 근거 `S4A api/callbacks.md#api-txMobileBcmDoorState`.

```c
DomainIf_Status_t (*txMobileBcmDoorState)(const DoorState_t *state);
```

<a id="api-DomainIf_QueryGetBcmDoorState"></a>
`DomainIf_QueryGetBcmDoorState` — DOMAIN Getter 본체, 근거 `S4A api/domain_query.md#api-DomainIf_QueryGetBcmDoorState`.

```c
DomainIf_Status_t DomainIf_QueryGetBcmDoorState(uint32_t nowMs, DoorState_t *outState);
```

<a id="api-Gateway_Interface_SendToMobile"></a>
`Gateway_Interface_SendToMobile` — ESP32 Gateway/Core/Transport Owner, 근거 `S4A api/esp32.md#api-Gateway_Interface_SendToMobile`.

```c
Gateway_Status_t Gateway_Interface_SendToMobile( const Gateway_RelayMessageView_t *message);
```

`DomainIf_TxMobileBcmDoorState` → `txMobileBcmDoorState` → `DomainNetPriv_SubmitMobileBcmDoorState(state,NULL)`은 자발 경로다. Query는 `DomainIf_QueryGetBcmDoorState`의 성공 OUT만 같은 submit의 non-NULL query와 연결한다. C2 public72/TxPort26/Observer11의 원형·Owner를 변경하지 않는다. NetSubmitResult→DomainIf_Status_t 대응은 [공통 제어](../UART_M_TX_RX_CONTROL.md)의 실제 반환 의미를 따르며 cast하지 않는다.

## 필드와 원 관측 연결

모든 필드의 정확한 offset·폭·LE·부호·enum·sentinel은 [단일 필드 정본](../../data/UART_M_PROJECTION_FIELDS.md#m_bcm_door_state)이다. `20+원 CAN used20=40`이며 CAN padding은 복제하지 않는다. outer DOMAIN_BOOT_ID는 payload B12, 원 ECU BOOT_ID/TX_SEQUENCE는 B20/B24다. LINK_SEQUENCE는 frame B6이다. 어느 하나를 다른 순번·부트로 대체하지 않는다.

잠금·열림·composite와 세 품질을 독립 보존한다. UNKNOWN raw255를 잠김/닫힘으로 만들지 않는다. C2 DoorState_t의 sourceGeneration 후보가 원 BCM Boot와 대응한다는 증거 및 meta summary 품질 원 평가가 필요하다. DoorState 보고는 특정 Unlock 명령의 confirmation이 아니다.

AGE 적용 위치는 `B38 state.source_age_ms`다. 송신 재포장으로0을 만들지 않는다. 원 association의 관측시각/이미 반영된 AGE 기준부터 UART 제출 전까지 아직 더하지 않은 로컬 경과만 한 번 누적한다. Getter nowMs로 이미 증가된 부분을 또 더하지 않고, 원 raw AGE 사본을 쓰는 경우에는 그 기준시각에서 계산한다. 65535는 포화 하한이며 미측정 Driver 지연은0으로 처리하지 않는다. 전송된 원 quality/reason은 유지하고 해당 최신성의 사용 여부는 소비자 계약으로 제한한다.

## Private Codec 후보와 한국어 C형 의사코드

아래 세 함수는 `PROPOSED_PRIVATE_FUNCTION`이다. 제품 헤더/코드를 생성하지 않는다. Validator는 명시 raw 형식만 검사하며 같은 입력 association·Session·freshness 승인은 하지 않는다. raw Net 코드와 C2 enum은 별도다. 조건상 정상 INVALID/UNKNOWN/NO_DATA는 허용된 의미로 보존하고, 원문에서 정하지 않은 센서 품질 조합을 새 MALFORMED 규칙으로 만들지 않는다. 불명확한 소비/표현은 공통 admission에서 CONTRACT_HOLD다.

[CheckFrame](../UART_FRAME_STREAM.md#check-frame)과 [BuildFrame](../UART_FRAME_STREAM.md#build-frame)은 정확한 VERSION/Type/방향/P/CRC와 실제 profile 채택 근거를 검사한다. enum V2_CANDIDATE를 고른 것만으로 채택되었다고 처리하지 않는다. 구조체 memcpy/sizeof를 Wire로 쓰지 않는다. [LE·i16 helper 정본](../UART_FRAME_STREAM.md#le-fields)은 아래 explicit offset의2/4/8B 변환만 수행하며 필드 의미 검사를 대신하지 않는다.

```c
NetUartMResult NetUartM_ValidateBcmDoorState(const NetMobileBcmDoorState *message)
{
    if (message == NULL) return NET_UART_M_INVALID_ARGUMENT;
    if (message->context.recipient.device_context_id == 0) return NET_UART_M_MALFORMED;
    if (message->context.domain_boot_id == 0) return NET_UART_M_MALFORMED;
    if (message->source.boot_id == 0) return NET_UART_M_MALFORMED;
    if (message->state.door_lock_state != 0 && message->state.door_lock_state != 1 && message->state.door_lock_state != 255) return NET_UART_M_MALFORMED;
    if (message->state.door_open_state != 0 && message->state.door_open_state != 1 && message->state.door_open_state != 255) return NET_UART_M_MALFORMED;
    if (message->state.door_composite_state > 2) return NET_UART_M_MALFORMED;
    if (message->state.lock_quality > 3) return NET_UART_M_MALFORMED;
    if (message->state.open_quality > 3) return NET_UART_M_MALFORMED;
    if (message->state.composite_quality > 3) return NET_UART_M_MALFORMED;
    // enum 저장형은 Net raw 코드 정본이다. C2 enum을 직접 cast한 입력은 허용하지 않는다.
    return NET_UART_M_OK;
}

NetUartMResult NetUartM_EncodeBcmDoorState(const NetMobileBcmDoorState *message,
    uint16_t linkSequence, NetUartMProfile profile, NetUartMFrame *out)
{
    if (out == NULL) return NET_UART_M_INVALID_ARGUMENT;
    NetUartMResult rc=NetUartM_ValidateBcmDoorState(message);
    if (rc != NET_UART_M_OK) return rc;
    uint8_t p[40]={0}; // 정확한 payload. 원 CAN padding을 넣을 여지가 없다.
    NetUartM_WriteLe32(&p[0], message->context.recipient.device_context_id);
    NetUartM_WriteLe64(&p[4], message->context.recipient.session_id);
    NetUartM_WriteLe32(&p[12], message->context.domain_boot_id);
    NetUartM_WriteLe32(&p[16], message->context.query_id);
    NetUartM_WriteLe32(&p[20], message->source.boot_id);
    NetUartM_WriteLe32(&p[24], message->source.tx_sequence);
    p[28] = message->state.door_lock_state;
    p[29] = message->state.door_open_state;
    p[30] = message->state.door_composite_state;
    p[31] = message->state.lock_quality;
    p[32] = message->state.open_quality;
    p[33] = message->state.composite_quality;
    NetUartM_WriteLe32(&p[34], message->state.update_sequence);
    NetUartM_WriteLe16(&p[38], message->state.source_age_ms);
    // BuildFrame은 전체 profile 증거·방향·Type별 P 검사 후 CRC를 만들고 성공 때만 out을 바꾼다.
    return NetUartM_BuildFrame(p,40,0x30,linkSequence,NET_UART_M_DOWNLINK,profile,out);
}

NetUartMResult NetUartM_DecodeBcmDoorState(const NetUartMFrame *frame,
    NetUartMProfile profile, NetMobileBcmDoorState *out, NetUartRxMeta *meta)
{
    if (frame == NULL || out == NULL || meta == NULL) return NET_UART_M_INVALID_ARGUMENT;
    NetUartMResult rc=NetUartM_CheckFrame(frame,NET_UART_M_DOWNLINK,0x30,profile);
    if (rc != NET_UART_M_OK) return rc;
    const uint8_t *p=&frame->bytes[8];
    NetMobileBcmDoorState tmp={0};
    tmp.context.recipient.device_context_id = NetUartM_ReadLe32(&p[0]);
    tmp.context.recipient.session_id = NetUartM_ReadLe64(&p[4]);
    tmp.context.domain_boot_id = NetUartM_ReadLe32(&p[12]);
    tmp.context.query_id = NetUartM_ReadLe32(&p[16]);
    tmp.source.boot_id = NetUartM_ReadLe32(&p[20]);
    tmp.source.tx_sequence = NetUartM_ReadLe32(&p[24]);
    tmp.state.door_lock_state = p[28];
    tmp.state.door_open_state = p[29];
    tmp.state.door_composite_state = p[30];
    tmp.state.lock_quality = p[31];
    tmp.state.open_quality = p[32];
    tmp.state.composite_quality = p[33];
    tmp.state.update_sequence = NetUartM_ReadLe32(&p[34]);
    tmp.state.source_age_ms = NetUartM_ReadLe16(&p[38]);
    rc=NetUartM_ValidateBcmDoorState(&tmp);
    if (rc != NET_UART_M_OK) return rc;
    NetUartRxMeta tm={0};
    tm.link_sequence=(uint16_t)((uint16_t)frame->bytes[6] | ((uint16_t)frame->bytes[7] << 8));
    tm.received_at_ms=frame->receivedAtMs; // 실제 로컬 수신시각. callback 시각을 대용하지 않는다.
    *out=tmp; *meta=tm; // 겹치지 않는 OUT 두 개를 모든 검사 후 함께 commit한다.
    return NET_UART_M_OK;
}
```

## 기존 submit의 조건부 정상 경로

아래 한국어 조건은 실제 Owner 증거가 필요한 지점이며 가상 함수 호출이나 기본값 채택이 아니다. [원본 연결 흐름](../../flows/UART_M_CAN_PROJECTION.md)의 C2 commit/Getter 직렬화 증거가 없으면 전송을 접수하지 않는다.

```c
NetSubmitResult DomainNetPriv_SubmitMobileBcmDoorState(const DoorState_t *state, const DomainQueryResponseContext_t *query)
{
    if (state == NULL) return NET_SUBMIT_INVALID_ARGUMENT;
    if (/* UART-M v2 양단 layout 채택 또는 현재 수신 Device/Session·DOMAIN Boot 근거 없음 */)
        return NET_SUBMIT_NOT_READY;
    if (query != NULL && query->queryId == 0)
        return NET_SUBMIT_INVALID_ARGUMENT;
    if (query != NULL && (/* 현재 Device/Session/관측된 DOMAIN Boot·scope에 맞지 않음 */))
        return NET_SUBMIT_NOT_READY;
    if (/* Getter 또는 TxPort의 state와 원 BCM_DOOR_STATE 수신·C2 commit 사이 동일입력 연결 미입증 */)
        return NET_SUBMIT_NOT_READY; // IF-O24. 값이 같은 최신 raw로 대신하지 않는다.
    if (/* 원 Boot/TX/관측 identity·품질의 의미 대응 또는 전 구간 AGE 계보 미확인 */)
        return NET_SUBMIT_NOT_READY;
    NetMobileBcmDoorState owned={0};
    owned.context.recipient = query == NULL ? /* 현재 수신 NetAppContext 값 */ : query->recipient;
    owned.context.domain_boot_id = /* 현재 DOMAIN 기동 */;
    owned.context.query_id = query == NULL ? 0 : query->queryId;
    owned.source = /* 동일입력 연결 증거가 가리키는 원 CAN NetCanSource 값 사본 */;
    owned.state = /* 동일입력 연결 증거가 가리키는 원 CAN Net 상태 객체 값 사본 */;
    // 아래 메시지별 AGE 위치만 같은 로컬 now의 미반영 경과로 증가시키고 원 품질·번호는 유지한다.
    if (/* 동기화된 위 값 사본과 경과 근거를 실제 확보하지 못함 */)
        return NET_SUBMIT_NOT_READY;
    uint32_t deltaMs = /* 원 raw AGE 기준시각부터 같은 로컬 now까지 미반영 경과, wrap·재시작 검증 완료 */;
    { uint64_t age=(uint64_t)owned.state.source_age_ms+deltaMs;
      owned.state.source_age_ms=(uint16_t)(age > 65535u ? 65535u : age); }
    // owned={0}는 초기 저장소일 뿐이다. 원본·문맥을 전부 채웠다는 증거 전 Encode/접수 금지.
    if (NetUartM_ValidateBcmDoorState(&owned) != NET_UART_M_OK)
        return NET_SUBMIT_INVALID_ARGUMENT;
    if (/* owned 전체·원 context·AGE 기준·Driver 수명을 보관할 슬롯 없음 */)
        return NET_SUBMIT_NO_CAPACITY;
    // M04에 자기 소유 사본을 원자적으로 게시한다. borrowed state/query 주소는 보관하지 않는다.
    // 게시 실패면 임시 예약만 반환하고 이 항목의 OK/Query count/정상 관측은 남기지 않는다.
    if (/* 해당 소유 슬롯의 자료·원본·시간 기준 게시가 완료되지 않음 */)
        return NET_SUBMIT_NOT_READY;
    // 성공이 확인된 뒤만 다음 반환. 실제 Queue primitive·용량·동기화는 바인딩 HOLD다.
    return NET_SUBMIT_OK;
}
```

dispatch는 현재 문맥·동일 원본·누적 AGE를 다시 확인한 후 위 Encode를 호출한다. LINK_SEQUENCE는 해당 방향의 승인된 전송 순번이며 재포장마다 새 관측을 만들지 않는다. 완성 frame은 Driver가 참조하는 동안 고정한다. driver accepted/final TX/released/ESP32 RX/BLE/App 전달은 별도다. 최종 TX pending·구 callback 세대 미확인이면 Query count에 넣지 않는다. 반환한 NET_SUBMIT_OK를 후발 실패로 소급 변경하지 않고 원 Query/항목 사실을 보존한다. 이 정기 상태 투영의 손실만으로 DomainIf_RxNetworkLocalFailure의 개별 COMMAND/REQUEST_RESULT/OCCURRENCE를 위조하지 않는다.

## ESP32 수신 경계와 복구

Stream → 공통 frame 검사 → 위 Decode의 모든 필드 → 현재 Device/Session/DOMAIN Boot 및 원 ECU 기동·관측·LINK_SEQUENCE 수용 → 자기 소유 사본 확보 → Gateway_Interface_OnDomainMessage 순서다. 같은 전송 중복은 의미 소비를 반복하지 않으며 wrap·역순·원 부트 교체 미확인은 ORDER_HOLD/CONTEXT_HOLD다.

Gateway_RelayMessageView_t의 넓은 VEHICLE_STATE category만으로 subtype을 판별하지 않는다. 기존 `TYPE 1B + 논리 payload P B`/length=P+1 Adapter convention은 BINDING_OPEN이다. 양단 채택 근거가 없으면 CONTRACT_HOLD이며 raw UART SYNC/CRC를 BLE ByteView에 복사하지 않는다. 현재 Device/Session은 payload와 일치해야 하고 Request 없는 이 상태 schema의 request_id view는 empty이다. QueryID·Occurrence를 request_id로 대신 넣지 않는다. 실제 OnDomainMessage/SendToMobile 원형은 위와 같으며 ESP32 Core/Transport 내부 구현은 작성하지 않는다.

Codec OK는 부호화 성공, NetSubmit OK는 자기 소유 로컬 접수, Gateway OK는 로컬 접수다. 기능 표시·차량 성공·원본 freshness·App 목록 완료는 각각 별도 근거가 필요하다. Session/Boot 변경·재기동은 구 projection/Query를 새 identity로 재귀속하거나 buffer release를 추정할 이유가 아니다. 현재 projection의 association 실패는 해당 항목만 미제공/Query 부분으로 남긴다.

## 정상·반례 전문 검토

아래 정상 분기는 실제 profile·양단 바인딩·원본 association 승인 근거가 충족된 조건부 경로이며, 현재 계약 HOLD를 해소하지 않는다.

| 입력/상황 | 검사·출력/상태 |
|---|---|
| 조건부 정상: lock255/open1/composite2, quality2/0/2; profile·원본 association·현재 문맥·시간·수명 전제 확보 | UNKNOWN/UNTRUSTED를 그대로 보존; 정확한 40B payload·50B frame, 성공 때만 OUT commit |
| 39B 또는 41B payload / CRC 오류 / 반대 방향 / VERSION1 또는 다른 Type | 공통 CheckFrame에서 거부; typed·정상 liveness·Gateway 호출 없음 |
| 이 표의 enum·reserved bit·범위 위반 | Validator MALFORMED, OUT 불변; 허용 UNKNOWN/NO_DATA와 구분 |
| C2 snapshot 값은 같으나 원 관측번호 또는 C2 commit association 미입증 | submit NOT_READY; 최신 raw의 Boot/TX를 붙이지 않음; Query 일부 미제공 |
| 원 AGE65535 / 미측정 대기 / Getter와 raw 나이 기준 중복 | 포화 하한 보존·필요 소비 TIME_HOLD; AGE0/임의 상한/이중 누적 금지 |
| borrowed 입력 반환 / 공간 부족 / Driver accepted 뒤 최종 결과 pending | 전체 사본 전 OK 없음; NO_CAPACITY, 참조 중 frame 불변; Query count 증가 없음 |
| 예전 Session·DOMAIN Boot 또는 LINK_SEQUENCE 중복/wrap 판단 불가 | 현재 표시·Query 완료로 소비하지 않음; 중복 no-op 또는 문맥/순서 HOLD |
| Query 최종 TX 일부 실패 또는 End보다 늦은 항목 | 공통 Query 부분·미완료; enqueue 수로 count/정상 End를 만들지 않음 |
| WINDOW 관련 입력·재기동·Permission 회복 | 두 WINDOW 투영의 실행·표시·timeout·liveness HOLD 유지 |

원 출처: `S4C evidence/b08/uart_m/bcm_projection.md#m_bcm_door_state`, `uart_m_contract.json`, `uart_trace_v0.2.json`, `S4A data/T15.md#type-NetMobileBcmDoorState` 및 위 원형 source, `S4C TIME_IDENTITY_CONTRACT.md#projection`, `FRESHNESS_AGE_TIMEOUT.md`, `RESOURCE_QUEUE_DRIVER_FLOW.md`, `FAILURE_RECOVERY_CONTRACT.md`. 미결: UART_M_VERSION_LAYOUT_ADOPTION_OPEN, IF-O24, IF-O23, IF-O09, C2-H01, CF01, D-02/D-03/D-05, IF-O12/IF-O25. 협의 차이는 기록만 하고 S6 완료 후 확인된 승인에 따라 통합 반영한다.
