# M_CIS_ENVIRONMENT — 원 CAN 관측의 UART-M 투영

`WIRE-P02-M_CIS_ENVIRONMENT` / `RX-P02-M_CIS_ENVIRONMENT-ESP32` · DOMAIN → ESP32 · Type `0x34` · VERSION2 후보 · payload 60B / frame 70B. `DESIGNED_CANDIDATE`: 양단 채택·원본 association 미검증인 정상 기능 경로는 HOLD다.

[전체 UART-M](../../05_UART_M_OVERVIEW.md) → [M06 Codec](../../modules/M06_UART_CODEC.md) → [이 메시지 필드](../../data/UART_M_PROJECTION_FIELDS.md#m_cis_environment) → [원 CAN 상세](../can/CIS_ENVIRONMENT.md) → [원본 투영 흐름](../../flows/UART_M_CAN_PROJECTION.md). [M02 Query](../../modules/M02_QUERY_CONTROL.md) · [공통 Router](../UART_M_ROUTER.md) · [송수신·Driver](../UART_M_TX_RX_CONTROL.md).

## 책임·자료·수명

L01/M01/F01~F03은 원 입력 연결, L02/M03/F07·F10은 TxPort/상대 Gateway 경계, L01/M02/F04·F05는 Query, L03/M04/F11~F13은 접수·순서, L04/M06/F17·F18은 typed Codec, L05/M07·M08/F19~F23은 저장소·Driver 수명, M09/F24·F25는 실제 관측이다. 기존5 Layer·9 Module·25 책임 수를 바꾸지 않는다.

DOMAIN 공개 Wrapper/Getter 본체와 Manager 상태는 DOMAIN 소유다. Network는 등록된 TxPort callback 본체에서 해당 private submit을 호출한다. ISR은 이 API/Codec/Getter를 호출하지 않고 실제 소유 Task에서 직렬 처리한다. ESP32 내부 Gateway/BLE 구현은 ESP32 담당자 소유이며 아래 수신 경계는 계약 추적이다.

자료 정본: [NetMobileCisEnvironment](../../data/UART_M_DATA.md#type-NetMobileCisEnvironment), [CisCabinEnvironmentState_t](../../data/CAN_NODE_DATA.md#type-CisCabinEnvironmentState_t), [NetCanSource](../../data/CIS_REAR_DATA.md#type-NetCanSource), [DomainQueryResponseContext_t](../../data/UART_M_DATA.md#type-DomainQueryResponseContext_t), [NetUartRxMeta](../../data/UART_M_DATA.md#type-NetUartRxMeta), [NetSubmitResult](../../data/CIS_REAR_DATA.md#type-NetSubmitResult), [DomainIf_Status_t](../../data/CIS_REAR_DATA.md#type-DomainIf_Status_t). private Frame/결과는 [UART Frame 자료](../../data/UART_FRAME_DATA.md).

`state`/`query`/`message`/`frame`은 호출 동안 빌린 IN이며 NULL 허용은 `query`만이다. Query Getter의 OUT은 성공한 경우만 읽는다. 새로운 private Codec의 입력과 모든 OUT은 서로 겹치지 않으며 실패 시 OUT을 유지한다. local OK 전에 DTO·원본 연계·시간 기준·중첩 바이트 전체를 자기 소유로 확보한다. 부모 callback table 값 복사는 pointed buffer 수명의 연장이 아니다.

## 기존 C 원형과 소유자

<a id="api-DomainIf_TxMobileCisEnvironment"></a>
`DomainIf_TxMobileCisEnvironment` — DOMAIN (공개 본체 소유 합의 완료), 근거 `S4A api/domain_tx.md#api-DomainIf_TxMobileCisEnvironment`.

```c
DomainIf_Status_t DomainIf_TxMobileCisEnvironment(const CisCabinEnvironmentState_t *state);
```

<a id="api-DomainNetPriv_SubmitMobileCisEnvironment"></a>
`DomainNetPriv_SubmitMobileCisEnvironment` — DOMAIN Network, 근거 `S4A api/domain_submit.md#api-DomainNetPriv_SubmitMobileCisEnvironment`.

```c
NetSubmitResult DomainNetPriv_SubmitMobileCisEnvironment(const CisCabinEnvironmentState_t *state, const DomainQueryResponseContext_t *query);
```

<a id="api-Gateway_Interface_OnDomainMessage"></a>
`Gateway_Interface_OnDomainMessage` — ESP32 Gateway/각 Core 또는 Transport Owner, 근거 `S4A api/esp32.md#api-Gateway_Interface_OnDomainMessage`.

```c
Gateway_Status_t Gateway_Interface_OnDomainMessage( const Gateway_RelayMessageView_t *message);
```

<a id="api-txMobileCisEnvironment"></a>
`txMobileCisEnvironment` — DOMAIN Network callback 본체, 근거 `S4A api/callbacks.md#api-txMobileCisEnvironment`.

```c
DomainIf_Status_t (*txMobileCisEnvironment)(const CisCabinEnvironmentState_t *state);
```

<a id="api-DomainIf_QueryGetCisEnvironment"></a>
`DomainIf_QueryGetCisEnvironment` — DOMAIN Getter 본체, 근거 `S4A api/domain_query.md#api-DomainIf_QueryGetCisEnvironment`.

```c
DomainIf_Status_t DomainIf_QueryGetCisEnvironment(uint32_t nowMs, CisCabinEnvironmentState_t *outState);
```

<a id="api-Gateway_Interface_SendToMobile"></a>
`Gateway_Interface_SendToMobile` — ESP32 Gateway/Core/Transport Owner, 근거 `S4A api/esp32.md#api-Gateway_Interface_SendToMobile`.

```c
Gateway_Status_t Gateway_Interface_SendToMobile( const Gateway_RelayMessageView_t *message);
```

`DomainIf_TxMobileCisEnvironment` → `txMobileCisEnvironment` → `DomainNetPriv_SubmitMobileCisEnvironment(state,NULL)`은 자발 경로다. Query는 `DomainIf_QueryGetCisEnvironment`의 성공 OUT만 같은 submit의 non-NULL query와 연결한다. C2 public72/TxPort26/Observer11의 원형·Owner를 변경하지 않는다. NetSubmitResult→DomainIf_Status_t 대응은 [공통 제어](../UART_M_TX_RX_CONTROL.md)의 실제 반환 의미를 따르며 cast하지 않는다.

## 필드와 원 관측 연결

모든 필드의 정확한 offset·폭·LE·부호·enum·sentinel은 [단일 필드 정본](../../data/UART_M_PROJECTION_FIELDS.md#m_cis_environment)이다. `20+원 CAN used40=60`이며 CAN padding은 복제하지 않는다. outer DOMAIN_BOOT_ID는 payload B12, 원 ECU BOOT_ID/TX_SEQUENCE는 B20/B24다. LINK_SEQUENCE는 frame B6이다. 어느 하나를 다른 순번·부트로 대체하지 않는다.

온도·습도·조도의 측정번호, AGE, validity/reason을 각각 보존한다. 정상 부호화된 온도 무효 보고만으로 유효한 습도·조도를 폐기하지 않는다. 기존 CisCabinEnvironmentState_t에 sourceGeneration이 없으며 공통/센서별 Generation 정책 IF-O20은 회신 대기다. 세 채널의 값을 하나의 원 CAN 수신과 연계할 증거가 없으면 이 composite 투영을 제공하지 않는다.

AGE 적용 위치는 `B34 state.temperature_age_ms`, `B44 state.humidity_age_ms`, `B56 state.illuminance_age_ms`다. 송신 재포장으로0을 만들지 않는다. 원 association의 관측시각/이미 반영된 AGE 기준부터 UART 제출 전까지 아직 더하지 않은 로컬 경과만 한 번 누적한다. Getter nowMs로 이미 증가된 부분을 또 더하지 않고, 원 raw AGE 사본을 쓰는 경우에는 그 기준시각에서 계산한다. 65535는 포화 하한이며 미측정 Driver 지연은0으로 처리하지 않는다. 원 raw의 quality/reason을 그대로 사용하는 경로와 검증된 로컬 현재 제한의 파생 경로를 구분한다. 현재 제한은 아래 추가 조건을 만족할 때만 투영하고 구 GOOD를 복원하지 않는다.

## Private Codec 후보와 한국어 C형 의사코드

아래 세 함수는 `PROPOSED_PRIVATE_FUNCTION`이다. 제품 헤더/코드를 생성하지 않는다. Validator는 명시 raw 형식만 검사하며 같은 입력 association·Session·freshness 승인은 하지 않는다. raw Net 코드와 C2 enum은 별도다. 조건상 정상 INVALID/UNKNOWN/NO_DATA는 허용된 의미로 보존하고, 원문에서 정하지 않은 센서 품질 조합을 새 MALFORMED 규칙으로 만들지 않는다. 불명확한 소비/표현은 공통 admission에서 CONTRACT_HOLD다.

[CheckFrame](../UART_FRAME_STREAM.md#check-frame)과 [BuildFrame](../UART_FRAME_STREAM.md#build-frame)은 정확한 VERSION/Type/방향/P/CRC와 실제 profile 채택 근거를 검사한다. enum V2_CANDIDATE를 고른 것만으로 채택되었다고 처리하지 않는다. 구조체 memcpy/sizeof를 Wire로 쓰지 않는다. [LE·i16 helper 정본](../UART_FRAME_STREAM.md#le-fields)은 아래 explicit offset의2/4/8B 변환만 수행하며 필드 의미 검사를 대신하지 않는다.

```c
NetUartMResult NetUartM_ValidateCisEnvironment(const NetMobileCisEnvironment *message)
{
    if (message == NULL) return NET_UART_M_INVALID_ARGUMENT;
    if (message->context.recipient.device_context_id == 0) return NET_UART_M_MALFORMED;
    if (message->context.domain_boot_id == 0) return NET_UART_M_MALFORMED;
    if (message->source.boot_id == 0) return NET_UART_M_MALFORMED;
    if (message->state.temperature_centi_c != -32768 && (message->state.temperature_centi_c < -4000 || message->state.temperature_centi_c > 12500)) return NET_UART_M_MALFORMED;
    if (message->state.temperature_validity > 1) return NET_UART_M_MALFORMED;
    if (message->state.temperature_reason > 9) return NET_UART_M_MALFORMED;
    if (message->state.humidity_centi_percent > 10000 && message->state.humidity_centi_percent != 65535) return NET_UART_M_MALFORMED;
    if (message->state.humidity_validity > 1) return NET_UART_M_MALFORMED;
    if (message->state.humidity_reason > 9) return NET_UART_M_MALFORMED;
    if (message->state.illuminance_lux > 200000u && message->state.illuminance_lux != 0xFFFFFFFFu) return NET_UART_M_MALFORMED;
    if (message->state.illuminance_validity > 1) return NET_UART_M_MALFORMED;
    if (message->state.illuminance_reason > 9) return NET_UART_M_MALFORMED;
    // enum 저장형은 Net raw 코드 정본이다. C2 enum을 직접 cast한 입력은 허용하지 않는다.
    return NET_UART_M_OK;
}

NetUartMResult NetUartM_EncodeCisEnvironment(const NetMobileCisEnvironment *message,
    uint16_t linkSequence, NetUartMProfile profile, NetUartMFrame *out)
{
    if (out == NULL) return NET_UART_M_INVALID_ARGUMENT;
    NetUartMResult rc=NetUartM_ValidateCisEnvironment(message);
    if (rc != NET_UART_M_OK) return rc;
    uint8_t p[60]={0}; // 정확한 payload. 원 CAN padding을 넣을 여지가 없다.
    NetUartM_WriteLe32(&p[0], message->context.recipient.device_context_id);
    NetUartM_WriteLe64(&p[4], message->context.recipient.session_id);
    NetUartM_WriteLe32(&p[12], message->context.domain_boot_id);
    NetUartM_WriteLe32(&p[16], message->context.query_id);
    NetUartM_WriteLe32(&p[20], message->source.boot_id);
    NetUartM_WriteLe32(&p[24], message->source.tx_sequence);
    NetUartM_WriteI16(&p[28], message->state.temperature_centi_c);
    NetUartM_WriteLe32(&p[30], message->state.temperature_sequence);
    NetUartM_WriteLe16(&p[34], message->state.temperature_age_ms);
    p[36] = message->state.temperature_validity;
    p[37] = message->state.temperature_reason;
    NetUartM_WriteLe16(&p[38], message->state.humidity_centi_percent);
    NetUartM_WriteLe32(&p[40], message->state.humidity_sequence);
    NetUartM_WriteLe16(&p[44], message->state.humidity_age_ms);
    p[46] = message->state.humidity_validity;
    p[47] = message->state.humidity_reason;
    NetUartM_WriteLe32(&p[48], message->state.illuminance_lux);
    NetUartM_WriteLe32(&p[52], message->state.illuminance_sequence);
    NetUartM_WriteLe16(&p[56], message->state.illuminance_age_ms);
    p[58] = message->state.illuminance_validity;
    p[59] = message->state.illuminance_reason;
    // BuildFrame은 전체 profile 증거·방향·Type별 P 검사 후 CRC를 만들고 성공 때만 out을 바꾼다.
    return NetUartM_BuildFrame(p,60,0x34,linkSequence,NET_UART_M_DOWNLINK,profile,out);
}

NetUartMResult NetUartM_DecodeCisEnvironment(const NetUartMFrame *frame,
    NetUartMProfile profile, NetMobileCisEnvironment *out, NetUartRxMeta *meta)
{
    if (frame == NULL || out == NULL || meta == NULL) return NET_UART_M_INVALID_ARGUMENT;
    NetUartMResult rc=NetUartM_CheckFrame(frame,NET_UART_M_DOWNLINK,0x34,profile);
    if (rc != NET_UART_M_OK) return rc;
    const uint8_t *p=&frame->bytes[8];
    NetMobileCisEnvironment tmp={0};
    tmp.context.recipient.device_context_id = NetUartM_ReadLe32(&p[0]);
    tmp.context.recipient.session_id = NetUartM_ReadLe64(&p[4]);
    tmp.context.domain_boot_id = NetUartM_ReadLe32(&p[12]);
    tmp.context.query_id = NetUartM_ReadLe32(&p[16]);
    tmp.source.boot_id = NetUartM_ReadLe32(&p[20]);
    tmp.source.tx_sequence = NetUartM_ReadLe32(&p[24]);
    tmp.state.temperature_centi_c = NetUartM_ReadI16(&p[28]);
    tmp.state.temperature_sequence = NetUartM_ReadLe32(&p[30]);
    tmp.state.temperature_age_ms = NetUartM_ReadLe16(&p[34]);
    tmp.state.temperature_validity = p[36];
    tmp.state.temperature_reason = p[37];
    tmp.state.humidity_centi_percent = NetUartM_ReadLe16(&p[38]);
    tmp.state.humidity_sequence = NetUartM_ReadLe32(&p[40]);
    tmp.state.humidity_age_ms = NetUartM_ReadLe16(&p[44]);
    tmp.state.humidity_validity = p[46];
    tmp.state.humidity_reason = p[47];
    tmp.state.illuminance_lux = NetUartM_ReadLe32(&p[48]);
    tmp.state.illuminance_sequence = NetUartM_ReadLe32(&p[52]);
    tmp.state.illuminance_age_ms = NetUartM_ReadLe16(&p[56]);
    tmp.state.illuminance_validity = p[58];
    tmp.state.illuminance_reason = p[59];
    rc=NetUartM_ValidateCisEnvironment(&tmp);
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
NetSubmitResult DomainNetPriv_SubmitMobileCisEnvironment(const CisCabinEnvironmentState_t *state, const DomainQueryResponseContext_t *query)
{
    if (state == NULL) return NET_SUBMIT_INVALID_ARGUMENT;
    if (/* UART-M v2 양단 layout 채택 또는 현재 수신 Device/Session·DOMAIN Boot 근거 없음 */)
        return NET_SUBMIT_NOT_READY;
    if (query != NULL && query->queryId == 0)
        return NET_SUBMIT_INVALID_ARGUMENT;
    if (query != NULL && (/* 현재 Device/Session/관측된 DOMAIN Boot·scope에 맞지 않음 */))
        return NET_SUBMIT_NOT_READY;
    if (/* Getter 또는 TxPort의 state와 원 CIS_ENVIRONMENT 수신·C2 commit 사이 동일입력 연결 미입증 */)
        return NET_SUBMIT_NOT_READY; // IF-O24. 값이 같은 최신 raw로 대신하지 않는다.
    if (/* 원 Boot/TX/관측 identity·품질의 의미 대응 또는 전 구간 AGE 계보 미확인 */)
        return NET_SUBMIT_NOT_READY;
    if (BOOT 전이의 영향을 받는 환경 입력이고 (현재 요청 내용·수용 BOOT·적용 Epoch·명확한 private 완료가 미확인이거나
        DOMAIN 후속 commit 봉쇄·무효화·Query 구 GOOD 방지 또는 B 공개 Rx의 동일 원 입력 연결이 없으면))
        return NET_SUBMIT_NOT_READY; // 관련 환경 항목만 기존 Query 미제공/부분 처리.
    // 일반 만료/중단의 기존 공개 Rx 제한 연결은 아래 품질 투영 경로로 유지한다.
    // M01/M04는 미시작 A 환경 Query 재게시를 막는다. Driver 참조 중 frame 임의취소 없음.
    // B gate 차단만으로 A GOOD 제거를 증명하지 않으며 구 raw에 B BOOT를 붙이지 않는다.
    // 명확한 Core 완료 뒤 Observer 오류로 이 submit에서 Rx/private commit을 재호출하지 않는다.
    NetMobileCisEnvironment owned={0};
    owned.context.recipient = query == NULL ? /* 현재 수신 NetAppContext 값 */ : query->recipient;
    owned.context.domain_boot_id = /* 현재 DOMAIN 기동 */;
    owned.context.query_id = query == NULL ? 0 : query->queryId;
    owned.source = /* 동일입력 연결 증거가 가리키는 원 CAN NetCanSource 값 사본 */;
    owned.state = /* 동일입력 연결 증거가 가리키는 원 CAN Net 상태 객체 값 사본 */;
    // 원값/원 Boot·TX·관측번호는 그대로 유지한다.
    if (state의 현재 품질이 보존 raw의 품질과 다르면) {
        if (같은 원 관측의 제한 사건·실제 Core 반영·Getter/TxPort 연결이 입증되지 않았으면)
            return NET_SUBMIT_NOT_READY;
        if (제한된 각 품질/사유에 기존 Wire의 명시 대응이 없으면)
            return NET_SUBMIT_NOT_READY;
        if (같은 관측의 원 INVALID 또는 이미 확인된 제한을 VALID로 회복시키면)
            return NET_SUBMIT_NOT_READY;
        // state의 temperatureQuality/humidityQuality/illuminanceQuality에서 검증된
        // 현재 validity/reason만 owned.state의 해당 채널 validity/reason에 이름 대응한다.
        // 원값/번호/원시각은 보존한다.
        // raw GOOD로 덮지 않으며 새 CAN TX·seq·Boot·수신시각을 생성하지 않는다.
    }
    // 아래 AGE 위치만 같은 로컬 now의 미반영 경과로 증가시킨다.
    if (/* 동기화된 위 값 사본과 경과 근거를 실제 확보하지 못함 */)
        return NET_SUBMIT_NOT_READY;
    uint32_t deltaMs = /* 원 raw AGE 기준시각부터 같은 로컬 now까지 미반영 경과, wrap·재시작 검증 완료 */;
    { uint64_t age=(uint64_t)owned.state.temperature_age_ms+deltaMs;
      owned.state.temperature_age_ms=(uint16_t)(age > 65535u ? 65535u : age); }
    { uint64_t age=(uint64_t)owned.state.humidity_age_ms+deltaMs;
      owned.state.humidity_age_ms=(uint16_t)(age > 65535u ? 65535u : age); }
    { uint64_t age=(uint64_t)owned.state.illuminance_age_ms+deltaMs;
      owned.state.illuminance_age_ms=(uint16_t)(age > 65535u ? 65535u : age); }
    // owned={0}는 초기 저장소일 뿐이다. 원본·문맥을 전부 채웠다는 증거 전 Encode/접수 금지.
    if (NetUartM_ValidateCisEnvironment(&owned) != NET_UART_M_OK)
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
| 조건부 정상: temperature=-32768/INVALID, humidity=4567/VALID, illuminance=1234/VALID; profile·원본 association·현재 문맥·시간·수명 전제 확보 | 무효 채널과 유효 형제 채널을 독립 보존; 정확한 60B payload·70B frame, 성공 때만 OUT commit |
| 59B 또는 61B payload / CRC 오류 / 반대 방향 / VERSION1 또는 다른 Type | 공통 CheckFrame에서 거부; typed·정상 liveness·Gateway 호출 없음 |
| 이 표의 enum·reserved bit·범위 위반 | Validator MALFORMED, OUT 불변; 허용 UNKNOWN/NO_DATA와 구분 |
| C2 snapshot 값은 같으나 원 관측번호 또는 C2 commit association 미입증 | submit NOT_READY; 최신 raw의 Boot/TX를 붙이지 않음; Query 일부 미제공 |
| 원 AGE65535 / 미측정 대기 / Getter와 raw 나이 기준 중복 | 포화 하한 보존·필요 소비 TIME_HOLD; AGE0/임의 상한/이중 누적 금지 |
| borrowed 입력 반환 / 공간 부족 / Driver accepted 뒤 최종 결과 pending | 전체 사본 전 OK 없음; NO_CAPACITY, 참조 중 frame 불변; Query count 증가 없음 |
| 예전 Session·DOMAIN Boot 또는 LINK_SEQUENCE 중복/wrap 판단 불가 | 현재 표시·Query 완료로 소비하지 않음; 중복 no-op 또는 문맥/순서 HOLD |
| Query 최종 TX 일부 실패 또는 End보다 늦은 항목 | 공통 Query 부분·미완료; enqueue 수로 count/정상 End를 만들지 않음 |
| WINDOW 관련 입력·재기동·Permission 회복 | 두 WINDOW 투영의 실행·표시·timeout·liveness HOLD 유지 |

원 출처: `S4C evidence/b08/uart_m/cis_projection.md#m_cis_environment`, `uart_m_contract.json`, `uart_trace_v0.2.json`, `S4A data/T15.md#type-NetMobileCisEnvironment` 및 위 원형 source, `S4C TIME_IDENTITY_CONTRACT.md#projection`, `FRESHNESS_AGE_TIMEOUT.md`, `RESOURCE_QUEUE_DRIVER_FLOW.md`, `FAILURE_RECOVERY_CONTRACT.md`. 미결: UART_M_VERSION_LAYOUT_ADOPTION_OPEN, IF-O24, IF-O23, IF-O09, C2-H01, IF-O19, IF-O20, CF01, D-02/D-03/D-05, IF-O12/IF-O25. 협의 차이는 기록만 하고 S6 완료 후 확인된 승인에 따라 통합 반영한다.

### PG6-SW01 현재 제한의 투영 근거

[NetCisEnvironmentLocalEvent](../../data/CAN_NODE_DATA.md#type-NetCisEnvironmentLocalEvent)와 [제한 인입](../can/CIS_ENVIRONMENT.md#local-invalidation)의 같은 원 기록·사건·실제 Core 반영이 Getter/TxPort 사본과 연결되어야 한다. 단순 값 일치나 C2 OK는 이 연결의 증거가 아니다. source BOOT/TX는 마지막 실제 CAN 관측의 출처이며 파생된 제한을 새 CAN 관측으로 주장하지 않는다. 기존 Wire에 이 파생관계를 해석할 양단 근거가 없으면 해당 투영은 HOLD하고 Query에서는 기존 미제공/부분 경로를 따른다. payload·TYPE·offset·CRC는 불변이다.

PG6-SW02는 [r1 환경 저장 순서](../can/CIS_ENVIRONMENT.md#environment-storage-order)를 Getter/TxPort 출처 연결에 적용한다. BOOT에서는 단일 동기 private 결과가 현재 요청 내용·수용 BOOT·적용 Epoch·명확한 완료와 일치하고 DOMAIN A 작업의 미래 commit 봉쇄·VSM/Meta·순서 문맥 무효화·구 GOOD Query 방지를 입증해야 한다. 구 객체 잔존만으로 미완료라 하지 않지만 Queue/일반 C2 OK/Getter NO_DATA 값만으로 완료를 만들지 않는다. 이후 B의 기존 공개 Rx 실제 반영과 동일 원 입력 Getter/TxPort 연결이 확인된 항목만 투영한다. 미시작 A 환경 Query 재게시 차단은 Network, 이미 인계된 구 작업/Getter의 무효화는 DOMAIN 책임이다. 구 GOOD 잔존·association 불명은 환경 해당 항목만 기존 미제공/부분 처리한다. 현재 다른 Epoch EPOCH_ORDER_UNAVAILABLE·실제 private ABI/Binding 미검증은 HOLD다. 일반 센서 NO_DATA·만료·수신중단의 기존 공개 Rx 품질 투영, 원 seq와 한 번 누적하는 AGE, 기존 선언·Codec·60B payload·TYPE·CRC·M_QUERY_END/count는 유지한다.
