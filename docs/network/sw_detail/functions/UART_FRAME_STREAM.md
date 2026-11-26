# UART-M Frame·CRC·Stream 함수

L04/M06의 F16은 byte stream 조립, F17은 수신 필드 검증, F18은 송신 Frame 생성을 맡는다. 여기의 모든 함수는 `PROPOSED_PRIVATE_FUNCTION`이며 실제 SDK/공개 API가 아니다. DOMAIN에서는 ISR 밖의 Network 단일 담당 실행 문맥에서, ESP32에서는 담당자가 확인한 소유자 문맥에서 호출한다. ISR은 raw byte와 실제 수신/오류 근거를 짧게 인계하며 parser/CRC/의미 판정·C2를 실행하지 않는다.

[UART-M](../05_UART_M_OVERVIEW.md) · [M06](../modules/M06_UART_CODEC.md) · [Frame Data](../data/UART_FRAME_DATA.md) · [기존 typed 자료](../data/UART_M_DATA.md) · [Router](UART_M_ROUTER.md) · [전송 수명](UART_M_TX_RX_CONTROL.md)

<a id="registry"></a>
## 현재 문서 기준 후보 Registry

B08 v0.2의 UART-M VERSION2 후보23종만 설명한다. A5 5A / VERSION1B / TYPE1B / P u16LE / LINK_SEQUENCE u16LE / P바이트 payload / CRC u16LE이며 전체 P+10이다. payload B0은 Frame B8이다. 115200bps는 초기 사용자 기준, 8N1/전이중/RTSCTS 미사용·실제 Port/Driver는 후보/미확인이다. VERSION2 선택 값은 양단 채택을 뜻하지 않는다.

```c
/* 후보 표: 새 Type/길이를 발명하지 않는다. */
static const NetUartMRegistryEntry uartMRegistry[23] = {
    {0x01, 38, NET_UART_M_UPLINK}, /* M_CONTEXT */
    {0x10, 24, NET_UART_M_UPLINK}, /* M_REQUEST */
    {0x11, 32, NET_UART_M_UPLINK}, /* M_QUERY */
    {0x12, 21, NET_UART_M_UPLINK}, /* M_WARNING_ACK */
    {0x02, 12, NET_UART_M_DOWNLINK}, /* M_DOMAIN_ALIVE */
    {0x20, 44, NET_UART_M_DOWNLINK}, /* M_RESULT */
    {0x21, 40, NET_UART_M_DOWNLINK}, /* M_WARNING */
    {0x22, 62, NET_UART_M_DOWNLINK}, /* M_AVAILABILITY */
    {0x23, 30, NET_UART_M_DOWNLINK}, /* M_DIGITAL_STATUS */
    {0x24, 35, NET_UART_M_DOWNLINK}, /* M_DIGITAL_RESULT */
    {0x25, 36, NET_UART_M_DOWNLINK}, /* M_USER_SETTINGS */
    {0x40, 24, NET_UART_M_DOWNLINK}, /* M_QUERY_END */
    {0x30, 40, NET_UART_M_DOWNLINK}, /* M_BCM_DOOR_STATE */
    {0x31, 44, NET_UART_M_DOWNLINK}, /* M_BCM_CLIMATE_STATE */
    {0x32, 45, NET_UART_M_DOWNLINK}, /* M_BCM_LIGHT_STATE */
    {0x33, 42, NET_UART_M_DOWNLINK}, /* M_BCM_STATUS */
    {0x34, 60, NET_UART_M_DOWNLINK}, /* M_CIS_ENVIRONMENT */
    {0x35, 45, NET_UART_M_DOWNLINK}, /* M_CIS_OCCUPANT */
    {0x36, 39, NET_UART_M_DOWNLINK}, /* M_CIS_REAR */
    {0x37, 54, NET_UART_M_DOWNLINK}, /* M_CIS_STATUS */
    {0x3A, 48, NET_UART_M_DOWNLINK}, /* M_VSS_STATUS */
    {0x38, 42, NET_UART_M_DOWNLINK}, /* M_WINDOW_STATE */
    {0x39, 42, NET_UART_M_DOWNLINK}, /* M_WINDOW_FAULT */
};
```

```c
NetUartMResult NetUartM_Lookup(uint8_t type, NetUartMDirection direction,
    NetUartMRegistryEntry *out)
{
    if (out == NULL || (direction != NET_UART_M_UPLINK && direction != NET_UART_M_DOWNLINK))
        return NET_UART_M_INVALID_ARGUMENT;
    for (uint16_t i = 0; i < 23; ++i) {
        if (uartMRegistry[i].type == type && uartMRegistry[i].direction == direction) {
            *out = uartMRegistry[i];
            return NET_UART_M_OK;
        }
    }
    return NET_UART_M_MALFORMED;
}
```

Lookup의 OUT은 OK 때만 갱신한다. registry 검색 성공은 frame·profile 채택·소비 성공이 아니다. P≤128만 만족하는 미등록 길이를 허용하지 않는다. 현재 최대 P62/Frame72, P0·128은 등록되지 않았다. UART-C의 Type/Version/typed Router는 이 표에 추가하지 않는다.

<a id="le-fields"></a>
## 폭이 정해진 LE 연산

이 private 연산은 전체 Frame/field 범위를 확인한 Codec만 호출한다. bytes는 NULL 불가, 읽기/쓰기2·4·8B가 실제 확보되어 있어야 한다. 쓰기 저장소는 호출자 소유의 임시 버퍼이며 Driver 참조/공개 중이 아니다. 연산 자체는 enum·sentinel·현재 문맥을 판정하지 않는다. 전제 위반 호출을 값0으로 정상화하는 함수가 아니다. 호출자가 전체 길이·offset을 먼저 검사한다.

```c
uint16_t NetUartM_ReadLe16(const uint8_t *bytes)
{
    return (uint16_t)((uint16_t)bytes[0] | ((uint16_t)bytes[1] << 8));
}
uint32_t NetUartM_ReadLe32(const uint8_t *bytes)
{
    uint32_t value = 0;
    for (uint8_t k = 0; k < 4; ++k) value |= (uint32_t)bytes[k] << (8u * k);
    return value;
}
uint64_t NetUartM_ReadLe64(const uint8_t *bytes)
{
    uint64_t value = 0;
    for (uint8_t k = 0; k < 8; ++k) value |= (uint64_t)bytes[k] << (8u * k);
    return value;
}
void NetUartM_WriteLe16(uint8_t *bytes, uint16_t value)
{
    bytes[0] = (uint8_t)value; bytes[1] = (uint8_t)(value >> 8);
}
void NetUartM_WriteLe32(uint8_t *bytes, uint32_t value)
{
    for (uint8_t k = 0; k < 4; ++k) bytes[k] = (uint8_t)(value >> (8u * k));
}
void NetUartM_WriteLe64(uint8_t *bytes, uint64_t value)
{
    for (uint8_t k = 0; k < 8; ++k) bytes[k] = (uint8_t)(value >> (8u * k));
}
int16_t NetUartM_ReadI16(const uint8_t *bytes)
{
    uint16_t raw = NetUartM_ReadLe16(bytes);
    int32_t value = raw < 32768u ? (int32_t)raw : (int32_t)raw - 65536;
    return (int16_t)value; // 이미 i16 표현 범위 안이다.
}
void NetUartM_WriteI16(uint8_t *bytes, int16_t value)
{
    // signed→unsigned16 변환의 modulo 65536 패턴은 명시한 2의 보수 표현이다.
    NetUartM_WriteLe16(bytes, (uint16_t)value);
}
```

메시지 Codec는 각 상수 offset에서 이 함수를 호출한다. `sizeof(struct)`/struct memcpy/enum cast는 쓰지 않는다. raw→C2 매핑은 각 메시지 이름 대응 표를 따르며 같은 정수 또는 +1을 일반화하지 않는다.

<a id="crc"></a>
## `NetUartM_Crc16`

```c
NetUartMResult NetUartM_Crc16(const uint8_t *bytes, uint16_t byteCount,
    uint16_t *outCrc)
{
    if (outCrc == NULL || (byteCount != 0 && bytes == NULL) || byteCount > 134)
        return NET_UART_M_INVALID_ARGUMENT;
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
    return NET_UART_M_OK;
}
```

IN bytes는 byteCount개 실제 확보된 불변 바이트이며 OUT은 입력과 겹치지 않는다. 호출 실패면 outCrc 불변이다. CRC-16/CCITT-FALSE: poly1021/initFFFF, RefIn/RefOut false, xorout0. Frame 호출은 **B2부터 B(P+7)까지 P+6개**를 전달한다. SYNC/마지막CRC는 제외한다. 최대134는 P128+6 계산 범위다. 빈 CRC 산술 입력의 결과FFFF를 등록 P0 Frame 승인으로 해석하지 않는다. ASCII `123456789`는29B1이고 저장 바이트는 B1 29다. 계산 정확성은 인증·문맥·실제 UART 전송을 보장하지 않는다.

<a id="check-frame"></a>
## `NetUartM_CheckFrame`

```c
NetUartMResult NetUartM_CheckFrame(const NetUartMFrame *frame,
    NetUartMDirection direction, uint8_t expectedType, NetUartMProfile profile)
{
    if (frame == NULL) return NET_UART_M_INVALID_ARGUMENT;
    if (profile == NET_UART_M_PROFILE_UNSELECTED) return NET_UART_M_CONTRACT_HOLD;
    if (profile != NET_UART_M_PROFILE_V2_CANDIDATE) return NET_UART_M_INVALID_ARGUMENT;
    if (DOMAIN·ESP32 양단의 이 profile revision 채택 근거가 없으면)
        return NET_UART_M_CONTRACT_HOLD;
    if (frame->length < 10 || frame->length > 138) return NET_UART_M_MALFORMED;
    const uint8_t *b = frame->bytes;
    if (b[0] != 0xA5 || b[1] != 0x5A || b[2] != 2 || b[3] != expectedType)
        return NET_UART_M_MALFORMED;
    NetUartMRegistryEntry entry;
    NetUartMResult found = NetUartM_Lookup(expectedType, direction, &entry);
    if (found != NET_UART_M_OK) return found;
    uint16_t p = NetUartM_ReadLe16(&b[4]);
    if (p > 128 || p != entry.payloadLength || frame->length != p + 10u)
        return NET_UART_M_MALFORMED;
    uint16_t crc;
    NetUartMResult calculated = NetUartM_Crc16(&b[2], p + 6u, &crc);
    if (calculated != NET_UART_M_OK) return calculated;
    if (crc != NetUartM_ReadLe16(&b[p + 8u])) return NET_UART_M_MALFORMED;
    return NET_UART_M_OK;
}
```

frame은 호출 중 빌린 안정 사본이며 부작용/OUT이 없다. 한국어 조건은 실제 검증된 Owner 근거가 필요하다는 의사코드 분기다. profile enum 자체를 채택 증거로 쓰지 않는다. OK는 형식·CRC만 유효하다는 뜻이다. Payload enum·reserved·Session·원 Boot/AGE·기능 HOLD·consumer는 아직 검사하지 않았다. LINK_SEQUENCE0은 u16 표현이며 wrap/최초/역전 수용은 Router/M04에서 별도로 판단한다. `expectedType`은 전달하려는 특정 메시지의 기존 Type이며 길이로 추측하지 않는다.

<a id="build-frame"></a>
## `NetUartM_BuildFrame`

```c
NetUartMResult NetUartM_BuildFrame(const uint8_t *payload, uint16_t payloadLength,
    uint8_t type, uint16_t linkSequence, NetUartMDirection direction,
    NetUartMProfile profile, NetUartMFrame *out)
{
    if (payload == NULL || out == NULL || 입력 payload와 OUT이 겹치면)
        return NET_UART_M_INVALID_ARGUMENT;
    NetUartMRegistryEntry entry;
    NetUartMResult found = NetUartM_Lookup(type, direction, &entry);
    if (found != NET_UART_M_OK) return found;
    if (payloadLength > 128 || payloadLength != entry.payloadLength)
        return NET_UART_M_MALFORMED;
    NetUartMFrame tmp = {0};
    tmp.length = payloadLength + 10u;
    tmp.bytes[0] = 0xA5; tmp.bytes[1] = 0x5A;
    tmp.bytes[2] = 2; tmp.bytes[3] = type;
    NetUartM_WriteLe16(&tmp.bytes[4], payloadLength);
    NetUartM_WriteLe16(&tmp.bytes[6], linkSequence);
    for (uint16_t i = 0; i < payloadLength; ++i) tmp.bytes[8u+i] = payload[i];
    uint16_t crc;
    NetUartMResult rc = NetUartM_Crc16(&tmp.bytes[2], payloadLength + 6u, &crc);
    if (rc != NET_UART_M_OK) return rc;
    NetUartM_WriteLe16(&tmp.bytes[payloadLength + 8u], crc);
    rc = NetUartM_CheckFrame(&tmp, direction, type, profile);
    if (rc != NET_UART_M_OK) return rc;
    *out = tmp;
    return NET_UART_M_OK;
}
```

payload는 **typed Codec가 모든 필드를 명시 직렬화한 P바이트 사본**이다. 이 함수의 byte 복사는 struct 메모리 복사가 아니다. caller는 실제 P바이트를 제공하고 LinkSequence의 할당·AGE 확정/기동·WINDOW 등 기능 HOLD를 M04/메시지 함수에서 먼저 확인한다. OK일 때 전체 out을 인수하며 실패는 out 불변이다. Frame 생성 시계 읽기/원 sequence 발급/Driver 제출이 없다. TX receivedAtMs=0을 실제 수신/AGE0 의미로 사용하지 않는다.

<a id="stream"></a>
## Stream의 상태와 저장소 계약

[NetUartMParser](../data/UART_FRAME_DATA.md#type-NetUartMParser)는 count0의 탐색, 1B A5 보관, 2..7B 헤더 조립, 8B 헤더 검사, P+10B 전체 조립 상태를 count/registry로 나타낸다. 새 공개 상태 enum이나 Task를 만들지 않는다. 어느 상태에서도 borrowed Driver 포인터를 보관하지 않는다. byte와 원 수신시각을 같은 index에 복사한다.

정상 CRC의 완전한 경계가 확보되면 전체 P+10B를 소모하고 Router에 넘긴다. 이후 enum/Session/consumer 거부로 payload 안 SYNC를 다시 찾지 않는다. CRC/헤더/조립 기한 오류는 경계가 불명확하므로 후보 시작+1부터 **이미 보관한 suffix도** 다시 검사한다. 마지막 A5와 다음 부분 header를 보존한다. 매 반복에서 입력을 소비하거나 후보 시작을 전진시키거나 반환하므로 같은 오류 위치에 머물지 않는다.

```c
/* private 전제: count <= parser->count, parser가 호출자의 단독 소유다. */
void NetUartM_DiscardPrefix(NetUartMParser *parser, uint16_t count)
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
## `NetUartM_StreamNext`

```c
NetUartMResult NetUartM_StreamNext(NetUartMParser *parser,
    const NetUartMStreamInput *input, NetUartMDirection direction,
    NetUartMProfile profile, uint16_t *consumed, NetUartMFrame *out)
{
    if (parser == NULL || input == NULL || consumed == NULL || out == NULL)
        return NET_UART_M_INVALID_ARGUMENT;
    if (input->length != 0 && (input->bytes == NULL || input->capturedAtMs == NULL))
        return NET_UART_M_INVALID_ARGUMENT;
    // 모든 입력/OUT/parser는 서로 겹치지 않고 out 저장공간을 미리 확보했다.
    *consumed = 0;
    if (direction != NET_UART_M_UPLINK && direction != NET_UART_M_DOWNLINK)
        return NET_UART_M_INVALID_ARGUMENT;
    if (profile != NET_UART_M_PROFILE_UNSELECTED && profile != NET_UART_M_PROFILE_V2_CANDIDATE)
        return NET_UART_M_INVALID_ARGUMENT;
    if (profile == NET_UART_M_PROFILE_UNSELECTED || 실제 양단 profile 채택 근거가 없으면)
        return NET_UART_M_CONTRACT_HOLD;
    if (input->linkEpoch != parser->linkEpoch) return NET_UART_M_CONTEXT_HOLD;
    if (!parser->timePolicyBound || !input->elapsedUnambiguous)
        return NET_UART_M_TIME_HOLD;
    if (parser->count > 138) return NET_UART_M_INVALID_ARGUMENT;
    for (;;) {
        if (parser->count >= 1 && parser->bytes[0] != 0xA5) {
            NetUartM_DiscardPrefix(parser, 1); continue;
        }
        if (parser->count >= 2 && parser->bytes[1] != 0x5A) {
            NetUartM_DiscardPrefix(parser, 1); continue; // A5 A5 5A도 보존
        }
        uint16_t wanted = 0;
        if (parser->count >= 8) {
            NetUartMRegistryEntry entry;
            uint16_t p = NetUartM_ReadLe16(&parser->bytes[4]);
            NetUartMResult rc = NetUartM_Lookup(parser->bytes[3], direction, &entry);
            if (parser->bytes[2] != 2 || rc != NET_UART_M_OK ||
                p > 128 || p != entry.payloadLength) {
                if (parser->malformedHeaders != UINT32_MAX) ++parser->malformedHeaders;
                NetUartM_DiscardPrefix(parser, 1); continue;
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
                NetUartM_DiscardPrefix(parser, 1); continue;
            }
        }
        if (wanted != 0 && parser->count >= wanted) {
            NetUartMFrame tmp = {0}; tmp.length = wanted;
            for (uint16_t i = 0; i < wanted; ++i) tmp.bytes[i] = parser->bytes[i];
            tmp.receivedAtMs = parser->capturedAtMs[wanted - 1u];
            uint16_t crc;
            NetUartMResult crcResult = NetUartM_Crc16(&tmp.bytes[2], wanted - 4u, &crc); // P+6
            if (crcResult != NET_UART_M_OK) return crcResult;
            if (crc != NetUartM_ReadLe16(&tmp.bytes[wanted - 2u])) {
                if (parser->badCrc != UINT32_MAX) ++parser->badCrc;
                NetUartM_DiscardPrefix(parser, 1); continue;
            }
            // 길이·방향·header·CRC로 신뢰한 하나의 경계를 먼저 확정한다.
            *out = tmp;
            NetUartM_DiscardPrefix(parser, wanted);
            return NET_UART_M_OK;
        }
        if (*consumed == input->length) return NET_UART_M_NEED_MORE;
        if (parser->count == 138) return NET_UART_M_NO_CAPACITY;
        uint16_t i = parser->count;
        parser->bytes[i] = input->bytes[*consumed];
        parser->capturedAtMs[i] = input->capturedAtMs[*consumed];
        ++parser->count; ++*consumed;
    }
}
```

이 함수는 한 번에 Frame **최대 한 건**을 반환한다. out은 OK일 때만 변경된다. consumed는 입력에서 사본으로 인수한 byte 수이며 OK가 아니어도 NEED_MORE에서 늘어날 수 있다. caller는 반환 뒤 consumed만큼 입력 시작을 전진시키고 나머지를 다시 호출한다. 이미 suffix에 다음 Frame이 있으면 length0 호출에서도 추출된다. 원 input을 재전달해 두 번 소비하지 않는다. count138은 논리 방어이며 실제 Driver/queue 공간 부족과 같지 않다. NO_CAPACITY이면 아직 소비하지 않은 입력을 완료로 표시하지 않는다.

프레임이 제때 도착했으나 worker가 늦게 처리한 경우 complete frame의 마지막 **수신시각**으로 조립시간을 계산한다. 입력에 오래된 미처리 bytes가 남아 있는데 nowMs만으로 후보를 지우지 않는다. timerCheckPermitted가 거짓이면 이번 입력의 실제 byte 시각까지만 진행하며 미수신 이후 시간은 timer 이벤트에서 재평가한다. timestamp 불확실성과 실제 clock 비교·동기화 정책을 확인하지 못하면 정상 실행 연결은 HOLD다.

OUT 공간과 소비자용 안정 사본은 Router 이전에 확보한다. 호출자는 parser 내부 주소를 consumer에게 비동기로 넘기지 않는다. 다음 Feed가 같은 배열을 바꾸어도 이미 복사된 Frame/DTO는 불변이어야 한다. CRC 정상 Frame 뒤 enum/Session/기능 거부는 Router가 해당 Frame만 거부하고 stream 뒤 정상 Frame의 처리 기회를 보존한다.

<a id="reset"></a>
## `NetUartM_ResetStream`

```c
NetUartMResult NetUartM_ResetStream(NetUartMParser *parser,
    uint32_t newLinkEpoch, bool bytesLost)
{
    if (parser == NULL) return NET_UART_M_INVALID_ARGUMENT;
    // 호출자는 실제 로컬 재접속/reset 또는 byte 손실 근거를 확인했다.
    // 진행 중 Feed/출력 복사와 직렬화한 후 불완전 후보만 폐기한다.
    parser->count = 0;
    parser->linkEpoch = newLinkEpoch;
    if (bytesLost && parser->byteLosses != UINT32_MAX) ++parser->byteLosses;
    return NET_UART_M_OK;
}
```

이 함수는 source Session·원 Boot·기능 상태를 발급/초기화하지 않는다. 이미 안정 인계한 Frame·pending Tx/Driver reference도 해제하지 않는다. transport gap/overflow는 잃은 bytes를 복원할 수 없으므로 해당 부분 후보를 무효화하고 다음 SYNC에서 시작한다. 조립 timeout은 이 전체 reset 함수가 아니라 시작+1 suffix 재탐색을 사용한다. 실제 초기화에서만 diagnostic 초기값을 마련하고 정상수신/원격 reboot마다 counters를 지우지 않는다.

## 정상·경계·반례

| 입력/상태 | 처리 위치와 결과 |
|---|---|
| 1B씩 A5,5A,나머지 header/body/CRC 공급 | 전체 P+10 전에 NEED_MORE; 단 한 번 Frame OK |
| A5 A5 5A / 앞뒤 noise | 시작+1 이동으로 두 번째 A5 보존; 잡음을 heartbeat로 세지 않음 |
| 정상 Frame 둘이 한 batch / CRC low만 있음 | 각각 추출 / high가 오기 전 NEED_MORE |
| header P129·P128·P0 또는 잘못된 exact P | header8B에서 거부, 거짓 P만큼 대기/할당 없음 |
| CRC 오류 후보 안에 다음 정상 Frame SYNC 존재 | 보관 suffix를 시작+1 재탐색; 새 input만 검색하지 않음 |
| CRC 정상 payload 내부 A5 5A, enum/Session 오류 | 정상 경계 전체를 소모하고 의미 거부; payload SYNC를 frame으로 재해석하지 않음 |
| 조립50ms 후보 경계에서 동등조건/clock 미확인 | TIME_HOLD; > 또는 ≥를 임의 채택하지 않음 |
| partial header timeout 뒤 저장된 정상 suffix | 시작+1 재탐색, 원 byte 시각 유지 |
| worker 지연 중 실제로 제때 도착한 완전 Frame | 수신시각으로 조립기한 평가; 처리시각으로 원본을 stale/fresh 재발급하지 않음 |
| old half-frame 뒤 새 연결 byte | 실제 reset/epoch 경계로 분리, 과거 요청 자동 재개 없음 |
| v1 M_CONTEXT31B/41B를 같은 Type01로 입력 | v2로 변환하지 않음, CF01 HOLD |
| typed 소비자가 buffer주소를 다음 callback까지 보관 | 금지; 안정 사본/명확한 lifetime 없으면 인계하지 않음 |

근거: S4C/evidence/b08 `UART_FRAME_CRC_CONTRACT`, `UART_STREAM_PARSER_SPEC`, `UART_CODEC_ROUTER`, T34; 최신 시간/Driver 판단은 S4C와 현재 CAN 공통의 수명 불변식을 함께 따른다. 문서 알고리즘·반례이며 제품 코드/실보드 실행 결과가 아니다.
