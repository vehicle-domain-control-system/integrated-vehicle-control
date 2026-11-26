# UART-C Frame·Stream·Typed Router

L04/M06의 F16 수신조립·F17 검증·F18 송신생성과 L02/M03 인계를 연결한다. 모든 NetUartC_* helper는 `PROPOSED_PRIVATE_FUNCTION`이며 실제 SDK/API가 아니다. 아래 공통 LE/CRC/stream 논리는 B13 [UART-M Frame](UART_FRAME_STREAM.md)의 검수된 문서 흐름을 출처로 삼아 **UART-C 전용 이름·VERSION1·3종 registry**로 분리했다. UART-M과 실제 함수·저장소를 공유한다고 가정하지 않는다.

[자료 정본](../data/UART_C_DATA.md) · [송수신 수명](UART_C_TX_RX_CONTROL.md) · [개요](../06_UART_C_OVERVIEW.md)

<a id="registry"></a>
## P03 고정 후보와 경계

| Type | 방향 | Payload / Frame | Codec / Rx |
|---|---|---|---|
| 0x60 | PI→CIS | 19 / 29B | [P_VISION_RESULT](uart_c/P_VISION_RESULT.md) / RX-P03-P_VISION_RESULT-CIS |
| 0x61 | PI→CIS | 16 / 26B | [P_VISION_STATUS](uart_c/P_VISION_STATUS.md) / RX-P03-P_VISION_STATUS-CIS |
| 0x62 | CIS→PI | 15 / 25B | [P_PERMISSION](uart_c/P_PERMISSION.md) / RX-P03-P_PERMISSION-PI |

Frame은 A5 5A / VERSION1 / TYPE / P u16LE / LINK_SEQUENCE u16LE / P payload / CRC u16LE다. payload B0=Frame B8, 전체P+10, CRC B2..B(P+7)의 P+6B다. 115200bps 시작 기준, 8N1/전이중/RTSCTS 미사용·P128·조립50ms는 후보/바인딩 OPEN이다. 양단 profile 채택 근거 없는 실제 소비는 HOLD이며 정적 후보 분석과 구별한다.

```c
static const NetUartCRegistryEntry uartCRegistry[3] = {
    {0x60, 19, NET_UART_C_PI_TO_CIS},
    {0x61, 16, NET_UART_C_PI_TO_CIS},
    {0x62, 15, NET_UART_C_CIS_TO_PI}
};
NetUartCResult NetUartC_Lookup(uint8_t type, NetUartCDirection direction,
    NetUartCRegistryEntry *out)
{
    if (out == NULL || (direction != NET_UART_C_PI_TO_CIS && direction != NET_UART_C_CIS_TO_PI))
        return NET_UART_C_INVALID_ARGUMENT;
    for (uint16_t i = 0; i < 3; ++i) {
        if (uartCRegistry[i].type == type && uartCRegistry[i].direction == direction) {
            *out = uartCRegistry[i]; return NET_UART_C_OK;
        }
    }
    return NET_UART_C_MALFORMED;
}
```

P0/P128/129·다른 Type·반대 방향·VERSION2는 등록되지 않았다. 길이/Type만으로 구2종↔신3종을 추정하지 않는다. Lookup OUT은 OK에만 갱신된다. Payload enum과 peer Boot·품질은 다음 단계에서 별도 검사한다.

<a id="le-fields"></a>
## 폭이 정해진 LE 연산

이 private 연산은 전체 Frame/field 범위를 확인한 Codec만 호출한다. bytes는 NULL 불가, 읽기/쓰기2·4B가 실제 확보되어 있어야 한다. 쓰기 저장소는 호출자 소유의 임시 버퍼이며 Driver 참조/공개 중이 아니다. 연산 자체는 enum·sentinel·현재 문맥을 판정하지 않는다. 전제 위반 호출을 값0으로 정상화하는 함수가 아니다. 호출자가 전체 길이·offset을 먼저 검사한다.

```c
uint16_t NetUartC_ReadLe16(const uint8_t *bytes)
{
    return (uint16_t)((uint16_t)bytes[0] | ((uint16_t)bytes[1] << 8));
}
uint32_t NetUartC_ReadLe32(const uint8_t *bytes)
{
    uint32_t value = 0;
    for (uint8_t k = 0; k < 4; ++k) value |= (uint32_t)bytes[k] << (8u * k);
    return value;
}
void NetUartC_WriteLe16(uint8_t *bytes, uint16_t value)
{
    bytes[0] = (uint8_t)value; bytes[1] = (uint8_t)(value >> 8);
}
void NetUartC_WriteLe32(uint8_t *bytes, uint32_t value)
{
    for (uint8_t k = 0; k < 4; ++k) bytes[k] = (uint8_t)(value >> (8u * k));
}
```

메시지 Codec는 각 상수 offset에서 이 함수를 호출한다. `sizeof(struct)`/struct memcpy/enum cast는 쓰지 않는다. raw→typed 매핑은 각 메시지 이름 대응 표를 따르며 같은 정수 또는 +1을 일반화하지 않는다.

<a id="crc"></a>
## `NetUartC_Crc16`

```c
NetUartCResult NetUartC_Crc16(const uint8_t *bytes, uint16_t byteCount,
    uint16_t *outCrc)
{
    if (outCrc == NULL || (byteCount != 0 && bytes == NULL) || byteCount > 134)
        return NET_UART_C_INVALID_ARGUMENT;
    uint16_t crc = 0xFFFF;
    for (uint16_t i = 0; i < byteCount; ++i) {
        crc ^= (uint16_t)((uint16_t)bytes[i] << 8);
        for (uint8_t bit = 0; bit < 8; ++bit) {
            uint32_t shifted = (uint32_t)crc << 1;
            if ((crc & 0x8000u) != 0) shifted ^= 0x1021u;
            crc = (uint16_t)(shifted & 0xFFFFu);
        }
    }
    *outCrc = crc;
    return NET_UART_C_OK;
}
```

IN bytes는 byteCount개 실제 확보된 불변 바이트이며 OUT은 입력과 겹치지 않는다. 호출 실패면 outCrc 불변이다. CRC-16/CCITT-FALSE: poly1021/initFFFF, RefIn/RefOut false, xorout0. Frame 호출은 **B2부터 B(P+7)까지 P+6개**를 전달한다. SYNC/마지막CRC는 제외한다. 최대134는 P128+6 계산 범위다. 빈 CRC 산술 입력의 결과FFFF를 등록 P0 Frame 승인으로 해석하지 않는다. ASCII `123456789`는29B1이고 저장 바이트는 B1 29다. 계산 정확성은 인증·문맥·실제 UART 전송을 보장하지 않는다.

<a id="check-frame"></a>
## `NetUartC_CheckFrame`

```c
NetUartCResult NetUartC_CheckFrame(const NetUartCFrame *frame,
    NetUartCDirection direction, uint8_t expectedType, NetUartCProfile profile)
{
    if (frame == NULL) return NET_UART_C_INVALID_ARGUMENT;
    if (profile == NET_UART_C_PROFILE_UNSELECTED) return NET_UART_C_CONTRACT_HOLD;
    if (profile != NET_UART_C_PROFILE_V1_CANDIDATE) return NET_UART_C_INVALID_ARGUMENT;
    if (Pi·CIS 양단의 이 profile revision 채택 근거가 없으면)
        return NET_UART_C_CONTRACT_HOLD;
    if (frame->length < 10 || frame->length > 138) return NET_UART_C_MALFORMED;
    const uint8_t *b = frame->bytes;
    if (b[0] != 0xA5 || b[1] != 0x5A || b[2] != 1 || b[3] != expectedType)
        return NET_UART_C_MALFORMED;
    NetUartCRegistryEntry entry;
    NetUartCResult found = NetUartC_Lookup(expectedType, direction, &entry);
    if (found != NET_UART_C_OK) return found;
    uint16_t p = NetUartC_ReadLe16(&b[4]);
    if (p > 128 || p != entry.payloadLength || frame->length != p + 10u)
        return NET_UART_C_MALFORMED;
    uint16_t crc;
    NetUartCResult calculated = NetUartC_Crc16(&b[2], p + 6u, &crc);
    if (calculated != NET_UART_C_OK) return calculated;
    if (crc != NetUartC_ReadLe16(&b[p + 8u])) return NET_UART_C_MALFORMED;
    return NET_UART_C_OK;
}
```

frame은 호출 중 빌린 안정 사본이며 부작용/OUT이 없다. 한국어 조건은 실제 검증된 Owner 근거가 필요하다는 의사코드 분기다. profile enum 자체를 채택 증거로 쓰지 않는다. OK는 형식·CRC만 유효하다는 뜻이다. Payload enum·reserved·원 Boot/AGE·기능 HOLD·consumer는 아직 검사하지 않았다. LINK_SEQUENCE0은 u16 표현이며 wrap/최초/역전 수용은 Router/M04에서 별도로 판단한다. source sequence 최초값을 정하지 않는다. `expectedType`은 전달하려는 특정 메시지의 기존 Type이며 길이로 추측하지 않는다.

<a id="build-frame"></a>
## `NetUartC_BuildFrame`

```c
NetUartCResult NetUartC_BuildFrame(const uint8_t *payload, uint16_t payloadLength,
    uint8_t type, uint16_t linkSequence, NetUartCDirection direction,
    NetUartCProfile profile, NetUartCFrame *out)
{
    if (payload == NULL || out == NULL || 입력 payload와 OUT이 겹치면)
        return NET_UART_C_INVALID_ARGUMENT;
    NetUartCRegistryEntry entry;
    NetUartCResult found = NetUartC_Lookup(type, direction, &entry);
    if (found != NET_UART_C_OK) return found;
    if (payloadLength > 128 || payloadLength != entry.payloadLength)
        return NET_UART_C_MALFORMED;
    NetUartCFrame tmp = {0};
    tmp.length = payloadLength + 10u;
    tmp.bytes[0] = 0xA5; tmp.bytes[1] = 0x5A;
    tmp.bytes[2] = 1; tmp.bytes[3] = type;
    NetUartC_WriteLe16(&tmp.bytes[4], payloadLength);
    NetUartC_WriteLe16(&tmp.bytes[6], linkSequence);
    for (uint16_t i = 0; i < payloadLength; ++i) tmp.bytes[8u+i] = payload[i];
    uint16_t crc;
    NetUartCResult rc = NetUartC_Crc16(&tmp.bytes[2], payloadLength + 6u, &crc);
    if (rc != NET_UART_C_OK) return rc;
    NetUartC_WriteLe16(&tmp.bytes[payloadLength + 8u], crc);
    rc = NetUartC_CheckFrame(&tmp, direction, type, profile);
    if (rc != NET_UART_C_OK) return rc;
    *out = tmp;
    return NET_UART_C_OK;
}
```

payload는 **typed Codec가 모든 필드를 명시 직렬화한 P바이트 사본**이다. 이 함수의 byte 복사는 struct 메모리 복사가 아니다. caller는 실제 P바이트를 제공하고 LinkSequence의 할당·AGE 확정/기동·원 품질/permission 등 기능 HOLD를 M04/메시지 함수에서 먼저 확인한다. OK일 때 전체 out을 인수하며 실패는 out 불변이다. Frame 생성 시계 읽기/원 sequence 발급/Driver 제출이 없다. TX receivedAtMs=0을 실제 수신/AGE0 의미로 사용하지 않는다.

<a id="stream"></a>
## Stream의 상태와 저장소 계약

[NetUartCParser](../data/UART_C_DATA.md#type-NetUartCParser)는 count0의 탐색, 1B A5 보관, 2..7B 헤더 조립, 8B 헤더 검사, P+10B 전체 조립 상태를 count/registry로 나타낸다. 새 공개 상태 enum이나 Task를 만들지 않는다. 어느 상태에서도 borrowed Driver 포인터를 보관하지 않는다. byte와 원 수신시각을 같은 index에 복사한다.

정상 CRC의 완전한 경계가 확보되면 전체 P+10B를 소모하고 Router에 넘긴다. 이후 enum/원 Boot/consumer 거부로 payload 안 SYNC를 다시 찾지 않는다. CRC/헤더/조립 기한 오류는 경계가 불명확하므로 후보 시작+1부터 **이미 보관한 suffix도** 다시 검사한다. 마지막 A5와 다음 부분 header를 보존한다. 매 반복에서 입력을 소비하거나 후보 시작을 전진시키거나 반환하므로 같은 오류 위치에 머물지 않는다.

```c
/* private 전제: count <= parser->count, parser가 호출자의 단독 소유다. */
void NetUartC_DiscardPrefix(NetUartCParser *parser, uint16_t count)
{
    uint16_t remain = parser->count - count;
    for (uint16_t i = 0; i < remain; ++i) {
        parser->bytes[i] = parser->bytes[i + count];
        parser->capturedAtMs[i] = parser->capturedAtMs[i + count];
    }
    parser->count = remain;
}
```

새 후보 SYNC가 과거 suffix에 있었으면 그 byte의 원 capturedAtMs가 조립 기준이다. 재탐색 시 Worker now를 부여해 기한을 연장하지 않는다. 실제 경과를 명확히 계산할 수 없는 clock/긴 중단/wrap은 TIME_HOLD다. 아래 unsigned 차는 elapsedUnambiguous와 실제 같은 clock·허용 관측 간격이 입증된 경우에만 사용한다. 조립50ms·UART 전송여유50ms·source AGE/TTL·Session 만료는 다른 규칙이다.

<a id="stream-next"></a>
## `NetUartC_StreamNext`

```c
NetUartCResult NetUartC_StreamNext(NetUartCParser *parser,
    const NetUartCStreamInput *input, NetUartCDirection direction,
    NetUartCProfile profile, uint16_t *consumed, NetUartCFrame *out)
{
    if (parser == NULL || input == NULL || consumed == NULL || out == NULL)
        return NET_UART_C_INVALID_ARGUMENT;
    if (input->length != 0 && (input->bytes == NULL || input->capturedAtMs == NULL))
        return NET_UART_C_INVALID_ARGUMENT;
    // 모든 입력/OUT/parser는 서로 겹치지 않고 out 저장공간을 미리 확보했다.
    *consumed = 0;
    if (direction != NET_UART_C_PI_TO_CIS && direction != NET_UART_C_CIS_TO_PI)
        return NET_UART_C_INVALID_ARGUMENT;
    if (profile != NET_UART_C_PROFILE_UNSELECTED && profile != NET_UART_C_PROFILE_V1_CANDIDATE)
        return NET_UART_C_INVALID_ARGUMENT;
    if (profile == NET_UART_C_PROFILE_UNSELECTED || 실제 양단 profile 채택 근거가 없으면)
        return NET_UART_C_CONTRACT_HOLD;
    if (input->linkEpoch != parser->linkEpoch) return NET_UART_C_CONTEXT_HOLD;
    if (!parser->timePolicyBound || !input->elapsedUnambiguous)
        return NET_UART_C_TIME_HOLD;
    if (parser->count > 138) return NET_UART_C_INVALID_ARGUMENT;
    for (;;) {
        if (parser->count >= 1 && parser->bytes[0] != 0xA5) {
            NetUartC_DiscardPrefix(parser, 1); continue;
        }
        if (parser->count >= 2 && parser->bytes[1] != 0x5A) {
            NetUartC_DiscardPrefix(parser, 1); continue; // A5 A5 5A도 보존
        }
        uint16_t wanted = 0;
        if (parser->count >= 8) {
            NetUartCRegistryEntry entry;
            uint16_t p = NetUartC_ReadLe16(&parser->bytes[4]);
            NetUartCResult rc = NetUartC_Lookup(parser->bytes[3], direction, &entry);
            if (parser->bytes[2] != 1 || rc != NET_UART_C_OK ||
                p > 128 || p != entry.payloadLength) {
                if (parser->malformedHeaders != UINT32_MAX) ++parser->malformedHeaders;
                NetUartC_DiscardPrefix(parser, 1); continue;
            }
            wanted = p + 10u;
        }
        if (parser->count != 0) {
            bool complete = wanted != 0 && parser->count >= wanted;
            uint32_t at = parser->capturedAtMs[complete ? wanted - 1u : parser->count - 1u];
            if (!complete && *consumed == input->length && input->timerCheckPermitted)
                at = input->nowMs;
            uint32_t elapsed = at - parser->capturedAtMs[0]; // 선행 clock 근거 필수
            bool expired = parser->expiresAtEquality ? elapsed >= parser->assemblyLimitMs
                                                   : elapsed > parser->assemblyLimitMs;
            if (expired) {
                if (parser->assemblyTimeouts != UINT32_MAX) ++parser->assemblyTimeouts;
                NetUartC_DiscardPrefix(parser, 1); continue;
            }
        }
        if (wanted != 0 && parser->count >= wanted) {
            NetUartCFrame tmp = {0}; tmp.length = wanted;
            for (uint16_t i = 0; i < wanted; ++i) tmp.bytes[i] = parser->bytes[i];
            tmp.receivedAtMs = parser->capturedAtMs[wanted - 1u];
            uint16_t crc;
            NetUartCResult crcResult = NetUartC_Crc16(&tmp.bytes[2], wanted - 4u, &crc); // P+6
            if (crcResult != NET_UART_C_OK) return crcResult;
            if (crc != NetUartC_ReadLe16(&tmp.bytes[wanted - 2u])) {
                if (parser->badCrc != UINT32_MAX) ++parser->badCrc;
                NetUartC_DiscardPrefix(parser, 1); continue;
            }
            // 길이·방향·header·CRC로 신뢰한 하나의 경계를 먼저 확정한다.
            *out = tmp;
            NetUartC_DiscardPrefix(parser, wanted);
            return NET_UART_C_OK;
        }
        if (*consumed == input->length) return NET_UART_C_NEED_MORE;
        if (parser->count == 138) return NET_UART_C_NO_CAPACITY;
        uint16_t i = parser->count;
        parser->bytes[i] = input->bytes[*consumed];
        parser->capturedAtMs[i] = input->capturedAtMs[*consumed];
        ++parser->count; ++*consumed;
    }
}
```

이 함수는 한 번에 Frame **최대 한 건**을 반환한다. out은 OK일 때만 변경된다. consumed는 입력에서 사본으로 인수한 byte 수이며 OK가 아니어도 NEED_MORE에서 늘어날 수 있다. caller는 반환 뒤 consumed만큼 입력 시작을 전진시키고 나머지를 다시 호출한다. 이미 suffix에 다음 Frame이 있으면 length0 호출에서도 추출된다. 원 input을 재전달해 두 번 소비하지 않는다. count138은 논리 방어이며 실제 Driver/queue 공간 부족과 같지 않다. NO_CAPACITY이면 아직 소비하지 않은 입력을 완료로 표시하지 않는다.

프레임이 제때 도착했으나 worker가 늦게 처리한 경우 complete frame의 마지막 **수신시각**으로 조립시간을 계산한다. 입력에 오래된 미처리 bytes가 남아 있는데 nowMs만으로 후보를 지우지 않는다. timerCheckPermitted가 거짓이면 이번 입력의 실제 byte 시각까지만 진행하며 미수신 이후 시간은 timer 이벤트에서 재평가한다. timestamp 불확실성과 실제 clock 비교·동기화 정책을 확인하지 못하면 정상 실행 연결은 HOLD다.

OUT 공간과 소비자용 안정 사본은 Router 이전에 확보한다. 호출자는 parser 내부 주소를 consumer에게 비동기로 넘기지 않는다. 다음 Feed가 같은 배열을 바꾸어도 이미 복사된 Frame/DTO는 불변이어야 한다. CRC 정상 Frame 뒤 enum/원 Boot/기능 거부는 Router가 해당 Frame만 거부하고 stream 뒤 정상 Frame의 처리 기회를 보존한다.

<a id="reset"></a>
## `NetUartC_ResetStream`

```c
NetUartCResult NetUartC_ResetStream(NetUartCParser *parser,
    uint32_t newLinkEpoch, bool bytesLost)
{
    if (parser == NULL) return NET_UART_C_INVALID_ARGUMENT;
    // 호출자는 실제 로컬 재접속/reset 또는 byte 손실 근거를 확인했다.
    // 진행 중 Feed/출력 복사와 직렬화한 후 불완전 후보만 폐기한다.
    parser->count = 0;
    parser->linkEpoch = newLinkEpoch;
    if (bytesLost && parser->byteLosses != UINT32_MAX) ++parser->byteLosses;
    return NET_UART_C_OK;
}
```

이 함수는 source 원 Boot·기능 상태를 발급/초기화하지 않는다. 이미 안정 인계한 Frame·pending Tx/Driver reference도 해제하지 않는다. transport gap/overflow는 잃은 bytes를 복원할 수 없으므로 해당 부분 후보를 무효화하고 다음 SYNC에서 시작한다. 조립 timeout은 이 전체 reset 함수가 아니라 시작+1 suffix 재탐색을 사용한다. 실제 초기화에서만 diagnostic 초기값을 마련하고 정상수신/원격 reboot마다 counters를 지우지 않는다.


<a id="decode"></a>
## 완전 Frame의 임시 typed decode

```c
NetUartCResult NetUartC_Decode(const NetUartCFrame *frame,
    NetUartCDirection direction, NetUartCProfile profile, NetUartCDecoded *out)
{
    if (frame == NULL || out == NULL || frame->length < 10)
        return NET_UART_C_INVALID_ARGUMENT;
    NetUartCResult rc = NetUartC_CheckFrame(frame, direction, frame->bytes[3], profile);
    if (rc != NET_UART_C_OK) return rc;
    NetUartCDecoded tmp = {0}; tmp.type = frame->bytes[3]; tmp.direction = direction;
    tmp.meta.link_sequence = NetUartC_ReadLe16(&frame->bytes[6]);
    tmp.meta.received_at_ms = frame->receivedAtMs;
    uint16_t p = NetUartC_ReadLe16(&frame->bytes[4]); bool decoded = false;
    switch (tmp.type) {
    case 0x60: decoded = NetUartC_DecodeVisionResultPayload(&frame->bytes[8], p, &tmp.value.visionResult); break;
    case 0x61: decoded = NetUartC_DecodeVisionStatusPayload(&frame->bytes[8], p, &tmp.value.visionStatus); break;
    case 0x62: decoded = NetUartC_DecodePermissionPayload(&frame->bytes[8], p, &tmp.value.permission); break;
    default: return NET_UART_C_MALFORMED;
    }
    if (!decoded) return NET_UART_C_MALFORMED;
    *out = tmp; return NET_UART_C_OK;
}
```

Frame/OUT은 서로 겹치지 않는 안정 저장소다. leaf는 정확 길이·25필드·코드를 임시 객체에 읽고 성공에만 OUT을 교체한다. decode의 OK는 raw typed 준비만 뜻하며 Boot 수용·의미 순서·실제 소비는 다음 경계다. CRC 정상 경계는 stream에서 이미 전체 소비했으므로 raw enum 거부 후 payload 안 SYNC를 다시 해석하지 않는다.

<a id="dispatch"></a>
## 실제 로컬 consumer 인계

```c
NetUartCResult NetUartC_Deliver(const NetUartCDecoded *d,
    bool admittedSourceAndOrder, bool ownerContextReady, bool callbackBindingVerified)
{
    if (d == NULL) return NET_UART_C_INVALID_ARGUMENT;
    if (!ownerContextReady || !callbackBindingVerified) return NET_UART_C_NOT_READY;
    if (!admittedSourceAndOrder) return NET_UART_C_CONTEXT_HOLD;
    switch (d->type) {
    case 0x60:
        if (d->direction != NET_UART_C_PI_TO_CIS) return NET_UART_C_MALFORMED;
        CisApp_OnPiVisionResult(&d->value.visionResult, &d->meta); break;
    case 0x61:
        if (d->direction != NET_UART_C_PI_TO_CIS) return NET_UART_C_MALFORMED;
        CisApp_OnPiVisionStatus(&d->value.visionStatus, &d->meta); break;
    case 0x62:
        if (d->direction != NET_UART_C_CIS_TO_PI) return NET_UART_C_MALFORMED;
        PiApp_OnCisPermission(&d->value.permission, &d->meta); break;
    default: return NET_UART_C_MALFORMED;
    }
    return NET_UART_C_OK; // void callback를 호출해 반환받음. 기능 commit/허용 성공 아님.
}
```

입력 d는 이 Decode가 만든 불변 사본이며 임의 constructed union을 받는 공개 경계가 아니다. bool 세 개는 [수신 제어](UART_C_TX_RX_CONTROL.md#order)가 실제 source/순서, Pi/CIS 담당 실행문맥, 기존 consumer 연결을 확인한 근거다. 상수 true로 대체하지 않는다. source/순서 미확인이면 정적 decode 분석만 가능하고 정상 semantic 인계는 HOLD다. 반면 수용된 source의 합법 INVALID/NO_DATA/FAULT/UNKNOWN 보고는 전달하여 열화 사실을 보존한다. 소비자의 기능 사용 가능 판단은 별도이며 sourceAGE를 fresh로 만들지 않는다.

소유자 Task 또는 Superloop에서만 실행한다. ISR은 byte/오류의 짧은 실제 관측 인계만 수행하며 parser/CRC/의미 callback은 금지한다. 호출 중 d/meta는 read-only borrowed, 이후 사용하면 consumer가 전체 사본을 확보한다. Pi 언어/C ABI·CIS callback 구현은 담당자 확인 사항이다.

<a id="encode"></a>
## typed 자료에서 Frame 생성

```c
NetUartCResult NetUartC_Encode(const NetUartCDecoded *d, uint16_t linkSequence,
    NetUartCProfile profile, NetUartCFrame *out)
{
    if (d == NULL || out == NULL) return NET_UART_C_INVALID_ARGUMENT;
    uint8_t p[19]; uint16_t n = 0; bool valid = false;
    switch (d->type) {
    case 0x60: n = 19; valid = NetUartC_EncodeVisionResultPayload(&d->value.visionResult, p, 19); break;
    case 0x61: n = 16; valid = NetUartC_EncodeVisionStatusPayload(&d->value.visionStatus, p, 19); break;
    case 0x62: n = 15; valid = NetUartC_EncodePermissionPayload(&d->value.permission, p, 19); break;
    default: return NET_UART_C_MALFORMED;
    }
    if (!valid) return NET_UART_C_MALFORMED;
    return NetUartC_BuildFrame(p, n, d->type, linkSequence, d->direction, profile, out);
}
```

이는 문서 내부 임시 최대 payload19B이며 실제 Queue/DMA 용량이 아니다. 호출자는 현재 원 관측/중계 세대와 대기 AGE를 확정한 owned typed사본을 제공한다. BuildFrame이 type/방향/exactP/profile을 재검사하고 성공일 때만 OUT 전체를 인수한다. linkSequence 할당은 M04에서 동일 방향 송신 직렬화로 수행하며 source sequence는 변경하지 않는다. 생성 뒤 Driver 제출을 이 함수가 수행하지 않는다.

## 정적 경계 검토

1B 분할, CRC high 누락, 둘 이상 연결, noise/A5 A5 5A, payload 내부 SYNC, malformed header/CRC/조립timeout의 suffix 재검색을 검토한다. 정상 CRC 뒤 enum/Boot/consumer 거부는 전체 경계 소비다. worker 지연과 실제 수신 지연을 구분하고 suffix timestamp를 유지한다. P0/128/129·wrong direction·VERSION2·구후보 자동탐지는 거부다. 알려진 링크 reset/byte손실에서 old half-frame만 폐기하며 원 관측이나 허용을 새로 발급하지 않는다.

출처: T35 L7~24/30~93, S4C `UART_FRAME_CRC_CONTRACT.md`·`UART_STREAM_PARSER_SPEC.md`·`TIME_IDENTITY_CONTRACT.md`·`RESOURCE_QUEUE_DRIVER_FLOW.md`, B13 [Frame/Stream](UART_FRAME_STREAM.md). ASCII123456789→29B1은 산술 확인이며 제품 Codec/Host/실보드 통과가 아니다.

<a id="named-callers"></a>
## 상위 Flow의 이름 명시 호출 위치

`NetUartC_Decode`·`NetUartC_Deliver`의 설계상 호출자는 [CIS 수신 Flow](../flows/UART_C_PI_TO_CIS.md#named-private-paths)와 [Pi Permission 수신 Flow](../flows/UART_C_CIS_TO_PI_PERMISSION.md#named-private-paths)의 M06/M04/M03 담당 문맥이다. 두 Flow는 Decode OUT을 성공일 때만 ClassifyRx 입력 근거로 사용하고, 수용 근거가 확인된 새 의미 입력과 Pi→CIS의 유효 새 Link/동일 Vision 관측 반복을 Deliver로 인계한다. 정확한 동일 Link/raw 중복·오류·과거 입력은 인계하지 않는다. Pi Permission의 반복 callback 규칙은 해당 Flow에서 별도로 유지한다. `NetUartC_Encode`의 호출자는 같은 두 Flow의 M04 송신 담당이며, 생산자 접수 이후 확보한 자기 소유 사본과 원 AGE/기한을 검증한 뒤 호출한다. 각 Flow의 호출 본문과 [관측·회수 연결](UART_C_TX_RX_CONTROL.md#named-caller-lifecycle)을 따른다. 실제 실행 함수/Task 심볼과 Driver/소비자 바인딩은 미확정이며, 이 연결은 새 함수 또는 제품 API 선언이 아니다.
