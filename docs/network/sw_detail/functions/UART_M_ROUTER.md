# UART-M Router와 Gateway 인계

M06/F17의 typed 분류와 M03/F10의 외부 ESP32 Gateway 경계를 설명한다. 아래 NetUartM 함수는 PROPOSED_PRIVATE_FUNCTION이며 제품 Header나 Gateway 공개 API를 추가하지 않는다. DOMAIN 공개 C2 본체는 DOMAIN, Network는 Codec·Adapter·TxPort 콜백 본체·등록 요청·Rx 호출을 맡는다. Gateway 공개13개 본체와 BLE/Core 구현은 ESP32 Owner 영역이다.

[개요](../05_UART_M_OVERVIEW.md) · [Stream](UART_FRAME_STREAM.md) · [수명·순서](UART_M_TX_RX_CONTROL.md) · [Data](../data/UART_M_DATA.md) · [Query](UART_M_QUERY_CONTROL.md)

<a id="decode-dispatch"></a>
## 정확한 Type·방향에 따른 typed 분류

`NetUartM_DecodeDispatch`의 frame은 안정된 P+10바이트 사본이다. out은 호출자 소유의 전체 union이며 입력과 겹치지 않는다. NULL·길이·profile·registry 오류는 OUT 불변, 성공은 정확한 한 union 멤버와 같은 프레임의 meta만 유효하게 한다. enum 범위는 메시지별 Decoder가 검사한다. 성공 후에도 원 문맥/순서/AGE/지원·HOLD와 소비자 준비를 따로 확인한다. 구조체 저장폭을 Wire 길이로 쓰지 않는다. Task/Owner의 단일 처리 문맥이며 ISR은 호출하지 않는다.

```c
NetUartMResult NetUartM_DecodeDispatch(const NetUartMFrame *frame,
    NetUartMDirection direction, NetUartMProfile profile, NetUartMDecoded *out)
{
    if (frame == NULL || out == NULL) return NET_UART_M_INVALID_ARGUMENT;
    if (frame->length < 10 || frame->length > 138) return NET_UART_M_MALFORMED;
    uint8_t type = frame->bytes[3];
    NetUartMResult rc = NetUartM_CheckFrame(frame, direction, type, profile);
    if (rc != NET_UART_M_OK) return rc;
    NetUartMDecoded tmp = {0}; tmp.type = type; tmp.direction = direction;
    switch (type) {
    case 0x22: rc = NetUartM_DecodeAvailability(frame, profile, &tmp.value.M_AVAILABILITY, &tmp.meta); break;
    case 0x31: rc = NetUartM_DecodeBcmClimateState(frame, profile, &tmp.value.M_BCM_CLIMATE_STATE, &tmp.meta); break;
    case 0x30: rc = NetUartM_DecodeBcmDoorState(frame, profile, &tmp.value.M_BCM_DOOR_STATE, &tmp.meta); break;
    case 0x32: rc = NetUartM_DecodeBcmLightState(frame, profile, &tmp.value.M_BCM_LIGHT_STATE, &tmp.meta); break;
    case 0x33: rc = NetUartM_DecodeBcmStatus(frame, profile, &tmp.value.M_BCM_STATUS, &tmp.meta); break;
    case 0x34: rc = NetUartM_DecodeCisEnvironment(frame, profile, &tmp.value.M_CIS_ENVIRONMENT, &tmp.meta); break;
    case 0x35: rc = NetUartM_DecodeCisOccupant(frame, profile, &tmp.value.M_CIS_OCCUPANT, &tmp.meta); break;
    case 0x36: rc = NetUartM_DecodeCisRear(frame, profile, &tmp.value.M_CIS_REAR, &tmp.meta); break;
    case 0x37: rc = NetUartM_DecodeCisStatus(frame, profile, &tmp.value.M_CIS_STATUS, &tmp.meta); break;
    case 0x01: rc = NetUartM_Decode_M_CONTEXT(frame, profile, &tmp.value.M_CONTEXT, &tmp.meta); break;
    case 0x24: rc = NetUartM_DecodeDigitalResult(frame, profile, &tmp.value.M_DIGITAL_RESULT, &tmp.meta); break;
    case 0x23: rc = NetUartM_DecodeDigitalStatus(frame, profile, &tmp.value.M_DIGITAL_STATUS, &tmp.meta); break;
    case 0x02: rc = NetUartM_Decode_M_DOMAIN_ALIVE(frame, profile, &tmp.value.M_DOMAIN_ALIVE, &tmp.meta); break;
    case 0x11: rc = NetUartM_Decode_M_QUERY(frame, profile, &tmp.value.M_QUERY, &tmp.meta); break;
    case 0x40: rc = NetUartM_Decode_M_QUERY_END(frame, profile, &tmp.value.M_QUERY_END, &tmp.meta); break;
    case 0x10: rc = NetUartM_Decode_M_REQUEST(frame, profile, &tmp.value.M_REQUEST, &tmp.meta); break;
    case 0x20: rc = NetUartM_DecodeResult(frame, profile, &tmp.value.M_RESULT, &tmp.meta); break;
    case 0x25: rc = NetUartM_DecodeUserSettings(frame, profile, &tmp.value.M_USER_SETTINGS, &tmp.meta); break;
    case 0x3A: rc = NetUartM_DecodeVssStatus(frame, profile, &tmp.value.M_VSS_STATUS, &tmp.meta); break;
    case 0x21: rc = NetUartM_DecodeWarning(frame, profile, &tmp.value.M_WARNING, &tmp.meta); break;
    case 0x12: rc = NetUartM_Decode_M_WARNING_ACK(frame, profile, &tmp.value.M_WARNING_ACK, &tmp.meta); break;
    case 0x39: rc = NetUartM_DecodeWindowFault(frame, profile, &tmp.value.M_WINDOW_FAULT, &tmp.meta); break;
    case 0x38: rc = NetUartM_DecodeWindowState(frame, profile, &tmp.value.M_WINDOW_STATE, &tmp.meta); break;
    default: return NET_UART_M_MALFORMED;
    }
    if (rc != NET_UART_M_OK) return rc;
    *out = tmp;
    return NET_UART_M_OK;
}
```

WINDOW2의 Decoder 진입 HOLD를 공통 switch로 우회하지 않는다. 두 UART-M 방향에 같은 Type을 임의 재사용하지 않는다. 별도 UART-C typed schema는 이 함수에 없다. 유효 CRC 이후 enum/Session/consumer가 거부해도 이미 소모한 frame payload 내부 SYNC로 되돌아가지 않는다.

## 수신자별 실제 인계 순서

| Type/입력 | 호출할 기존 로컬 경계와 검사 | 성공·실패의 보존 |
|---|---|---|
| M_CONTEXT | [DomainNetPriv_OnGatewayContext](uart_m/M_CONTEXT.md#admission): 공동 등록/연결과 독립 App·근접의 동일 Device/Session·원 평가·품질·AGE 연결 후 C2 두 경계 | 부분 commit 가능성을 선행 확인. 첫 C2 성공 뒤 둘째 실패로 전체를 재실행하지 않음 |
| M_REQUEST | [8개 요청 분기](uart_m/M_REQUEST.md): RequestId/원 Session/지원 연산·현재 문맥을 확인하고 정확한 C2 Rx* 요청 한 개 | SENT AGE 부재를 로컬 수신시각으로 메우지 않음. 재접속으로 과거 요청 재실행 없음 |
| M_QUERY | [DomainNetPriv_ProcessQuery](UART_M_QUERY_CONTROL.md): passive 문맥 검증→18개 기존 Getter→동일 원본 응답 | SESSION_READY0 단독 거부 금지; Getter 실패 OUT 미소비. UART 최종 TX·End 순서 별도 |
| M_WARNING_ACK | [WarningAck 인계](uart_m/M_WARNING_ACK.md): 원 Warning DOMAIN Boot/종류/Occurrence+현재 Device/Session | 원 위험 CLEAR·음향 STOP·기능 DONE을 합성하지 않음 |
| M_DOMAIN_ALIVE | [EspNetPriv_OnDomainAlive](uart_m/M_DOMAIN_ALIVE.md) | private 감시. 12B에 Session/SessionReady 없음. 주기 수신으로 Session을 발급하지 않음 |
| M_QUERY_END | [EspNetPriv_OnQueryEnd](uart_m/M_QUERY_END.md) | private Query 감독. 현재 Query/Boot/Session·중복·유효 응답수 검증. void는 App 완료 아님 |
| 일반 하향17개 | [6 DOMAIN 보고](../flows/UART_M_DOWNLINK.md)와 [11 CAN projection](../flows/UART_M_CAN_PROJECTION.md)의 모든 필드 검사 후 Gateway_Interface_OnDomainMessage | 타입별 원본 association·현재 recipient·기능 HOLD·schema 적용. DOMAIN Boot를 내부 CAN Boot로 쓰지 않음 |

M04의 순서 장부는 실제 동일 입력과 소비 commit에 묶는다. 형식 검증만으로 정상 기능 최신성·현재 peer 생존을 모두 갱신하지 않는다. valid INVALID/NO_DATA/STALE 자료는 malformed와 다르게 원 품질을 보존할 수 있다. callback 반환 성공과 Manager commit 증거가 다르면 후자를 만들어내지 않는다. Core가 반영한 입력에서 optional Observer만 실패했을 때 Core를 다시 호출하지 않는다.

<a id="gateway-view"></a>
## Gateway ByteView convention과 subtype

`Gateway_RelayMessageView_t`는 byte-view 경계다. **TYPE1B+논리 payload P바이트, 길이 P+1**는 B08의 미승인 subtype convention이다. UART SYNC·VERSION·P·LINK_SEQUENCE·CRC를 통째로 Gateway/BLE payload에 넣는 규칙이 아니다. 실제 Core/Transport가 이 convention을 함께 수용했다는 증거가 없으면 해당 인계를 HOLD한다. 세션 인증·BLE GATT/MTU·보안 구현은 이번에 만들지 않는다.

| Type | Gateway category 후보 | device/session/request view |
|---|---|---|
| 0x10 M_REQUEST | MOBILE_REQUEST | Device4, Session8, Request4(B12) |
| 0x11 M_QUERY | STATE_QUERY | Device4, Session8. QueryID를 RequestID로 쓰지 않음; 원 요청 필터의 request view 연결은 scope와 실제 convention 확인 |
| 0x12 M_WARNING_ACK | WARNING_ACK | Device4, Session8, request 없음. Occurrence를 Request로 쓰지 않음 |
| 0x20 M_RESULT | REQUEST_RESULT | 현재 recipient Device4/Session8, 원 Request4(B28); payload의 과거 REQUEST_SESSION 별도 보존 |
| 0x21 M_WARNING | WARNING | 현재 recipient, request 없음 |
| 0x22 M_AVAILABILITY | FUNCTION_AVAILABILITY | 현재 recipient, request 없음 |
| 0x23/24 | DIGITAL_KEY_STATE_RESULT | 현재 recipient, request 없음; automatic occurrence는 Request가 아님 |
| 0x25·0x30..0x3A | VEHICLE_STATE | 현재 recipient, request 없음; subtype는 반드시 실제 Type로 구별 |
| M_CONTEXT | 세 전용 Publish* 경계 | general RelayMessage로 임의 통합하지 않음 |
| M_DOMAIN_ALIVE/M_QUERY_END | private | broad category에 강제 편입하지 않음 |

동일 P42인 BCM_STATUS/WINDOW_STATE/WINDOW_FAULT를 길이로 구분할 수 없다. vehicle_id는 이 Wire에 없다. 기존에 확인된 등록 문맥의 ID가 있으면 그 값과 수명을 함께 바인딩하고 없으면 NULL/0을 유지한다. 새 vehicle 문자열을 만든 뒤 등록 완료라 하지 않는다. absent view의 NULL/0과 존재하지만 값0인 실제 필드는 구분한다.

<a id="gateway-build"></a>
## 하향 view 생성의 전개

아래 함수는 이미 수신된 frame을 다시 typed 검사하고 payload 사본을 만드는 private 제안이다. `bindingVerified`는 현재 수신의 profile·현재 recipient/Boot/순서/AGE·기능 제한 및 **실제 Gateway subtype 소비 계약**의 확인 결과다. 외부 승인으로 바꿀 수 있는 설정 스위치가 아니며 현재 미확인 경로에서는 false다. 호출자는 이 근거를 같은 frame identity에 묶고 처리 중 바뀌지 않게 해야 한다. Decoder 성공만으로 true를 만들지 않는다.

```c
NetUartMResult NetUartM_BuildDownlinkView(const NetUartMFrame *frame,
    NetUartMProfile profile, bool bindingVerified,
    uint8_t *logicalPayload, size_t capacity, Gateway_RelayMessageView_t *out)
{
    if (frame == NULL || logicalPayload == NULL || out == NULL)
        return NET_UART_M_INVALID_ARGUMENT;
    // frame, logicalPayload, out은 실제 저장영역 전체가 서로 겹치지 않는다.
    if (!bindingVerified) return NET_UART_M_CONTRACT_HOLD;
    NetUartMDecoded decoded;
    NetUartMResult rc = NetUartM_DecodeDispatch(frame, NET_UART_M_DOWNLINK, profile, &decoded);
    if (rc != NET_UART_M_OK) return rc;
    Gateway_LogicalMessageType_t category;
    switch (decoded.type) {
    case 0x20: category = GATEWAY_LOGICAL_MSG_REQUEST_RESULT; break;
    case 0x21: category = GATEWAY_LOGICAL_MSG_WARNING; break;
    case 0x22: category = GATEWAY_LOGICAL_MSG_FUNCTION_AVAILABILITY; break;
    case 0x23: case 0x24: category = GATEWAY_LOGICAL_MSG_DIGITAL_KEY_STATE_RESULT; break;
    case 0x25: case 0x30: case 0x31: case 0x32: case 0x33: case 0x34:
    case 0x35: case 0x36: case 0x37: case 0x3A:
        category = GATEWAY_LOGICAL_MSG_VEHICLE_STATE; break;
    case 0x38: case 0x39: return NET_UART_M_CONTRACT_HOLD; // WINDOW inactive
    default: return NET_UART_M_INVALID_ARGUMENT; // ALIVE/END private
    }
    uint16_t p = NetUartM_ReadLe16(frame->bytes + 4);
    if (capacity < (size_t)p + 1u) return NET_UART_M_NO_CAPACITY;
    Gateway_RelayMessageView_t tmp = {0};
    tmp.type = category; tmp.direction = GATEWAY_DIRECTION_DOMAIN_TO_MOBILE;
    tmp.payload.data = logicalPayload; tmp.payload.length = (size_t)p + 1u;
    tmp.context.device_context_id.data = logicalPayload + 1;
    tmp.context.device_context_id.length = 4;
    tmp.context.session_id.data = logicalPayload + 5;
    tmp.context.session_id.length = 8;
    if (decoded.type == 0x20) {
        tmp.context.request_id.data = logicalPayload + 29; // TYPE1 + B28
        tmp.context.request_id.length = 4;
    }
    // vehicle_id와 request 없는 타입은 {NULL,0}; 필요 시 검증된 외부 문맥 계약 별도.
    logicalPayload[0] = decoded.type;
    for (uint16_t i = 0; i < p; ++i) logicalPayload[i+1u] = frame->bytes[i+8u];
    *out = tmp; // 모든 실패 분기를 지난 뒤 두 OUT의 논리 commit
    return NET_UART_M_OK;
}
```

OUT view는 logicalPayload를 참조하므로 view 구조체만 복사해 수명을 연장할 수 없다. 호출자는 Gateway의 동기 호출 반환까지 logicalPayload를 보존한다. 비동기 consumer는 OK 전 nested bytes 전체를 자기 저장소에 확보해야 한다. 이후 Gateway_Interface_OnDomainMessage의 OK는 로컬 인계 성공이며 BLE 전송·App 표시 완료가 아니다. 정상 view에 대해 SendToMobile을 언제 호출하는지는 기존 ESP32 Core Owner의 계약을 따른다.

이 함수는 원 payload를 복사하므로 view 안의 AGE는 **원 Frame의 기준점까지 반영한 값**이며 현재 Gateway 소비시각 AGE가 아니다. Gateway 공개 view에는 received_at_ms가 없다. ESP32 Owner가 같은 frame identity에 실제 NetUartRxMeta·이미 반영된 홉·미반영 로컬 대기를 안정적으로 연결하는 근거가 없으면 bindingVerified=false로 두고 인계를 HOLD한다. 기존 공개 타입에 timestamp를 추가하거나 callback now를 원 RX로 대체하지 않는다. 이후 AGE를 갱신할 필요가 있으면 타입별 명시 필드와 동일 원본 근거로, 해당 송신/소비 계약이 승인된 경계에서만 수행한다.

<a id="gateway-api"></a>
## 기존 Gateway 공개13 경계

모든 원형은 S4A/T22 선언 그대로다. 공개 함수를 Network가 재구현하지 않는다. 다음 표와 각 원형은 호출 전 조건·원 인자 수명·반환을 함께 적용한다. const IN은 NULL 불가(단 Init table 전체 NULL 허용 여부는 미확인), 호출 중만 차용한다. 내부 ByteView의 실제 길이·메모리 확보는 caller 책임이다. bool 검증 함수의 true는 포인터/논리형식 검증의 최소 결과이며 인증·현재 Session·subtype 지원·Driver 준비의 증거가 아니다.

<a id="api-Gateway_Interface_Init"></a>
```c
Gateway_Status_t Gateway_Interface_Init( const Gateway_InterfaceCoreHandlers_t *core_handlers, const Gateway_InterfaceTransportPorts_t *transport_ports);
```

<a id="api-Gateway_Interface_Reset"></a>
```c
void Gateway_Interface_Reset(void);
```

<a id="api-Gateway_Interface_IsInitialized"></a>
```c
bool Gateway_Interface_IsInitialized(void);
```

<a id="api-Gateway_Interface_OnMobileMessage"></a>
```c
Gateway_Status_t Gateway_Interface_OnMobileMessage( const Gateway_RelayMessageView_t *message);
```

<a id="api-Gateway_Interface_OnDomainMessage"></a>
```c
Gateway_Status_t Gateway_Interface_OnDomainMessage( const Gateway_RelayMessageView_t *message);
```

<a id="api-Gateway_Interface_SendToDomain"></a>
```c
Gateway_Status_t Gateway_Interface_SendToDomain( const Gateway_RelayMessageView_t *message);
```

<a id="api-Gateway_Interface_SendToMobile"></a>
```c
Gateway_Status_t Gateway_Interface_SendToMobile( const Gateway_RelayMessageView_t *message);
```

<a id="api-Gateway_Interface_PublishRegistrationConnection"></a>
```c
Gateway_Status_t Gateway_Interface_PublishRegistrationConnection( const Gateway_RegistrationConnectionUpdate_t *update);
```

<a id="api-Gateway_Interface_PublishAppActivity"></a>
```c
Gateway_Status_t Gateway_Interface_PublishAppActivity( const Gateway_AppActivityUpdate_t *update);
```

<a id="api-Gateway_Interface_PublishProximity"></a>
```c
Gateway_Status_t Gateway_Interface_PublishProximity( const Gateway_ProximityUpdate_t *update);
```

<a id="api-Gateway_Interface_IsValidByteView"></a>
```c
bool Gateway_Interface_IsValidByteView( const Gateway_ByteView_t *view);
```

<a id="api-Gateway_Interface_IsValidMessageContext"></a>
```c
bool Gateway_Interface_IsValidMessageContext( const Gateway_MessageContextView_t *context);
```

<a id="api-Gateway_Interface_IsValidRelayMessage"></a>
```c
bool Gateway_Interface_IsValidRelayMessage( const Gateway_RelayMessageView_t *message);
```

| 함수 | caller → Owner·효과 / 최소 반환 보증 |
|---|---|
| Init | ESP32 초기화 Owner→Gateway. Core2/Transport5 slots 등록; 개별 NULL callback은 호출 시 UNSUPPORTED. table 값복사 방식·전체NULL·함수주소/user_context 장기수명 실제 확인 전 HOLD |
| Reset | ESP32 lifecycle Owner. 반환없음. 진행 중 Driver 참조해제·원 Request 취소·기능종결의 증거가 아님 |
| IsInitialized | ESP32 호출자. bool 로컬 초기화 상태만, 실제 UART/BLE/peer 준비 아님 |
| OnMobileMessage | BLE Adapter→Core on_mobile_message. 검증한 Request/Query/ACK 로컬 인계 |
| OnDomainMessage | UART Adapter→Core on_domain_message. 일반17 schema 로컬 인계; private2 제외 |
| SendToDomain | Core→Transport send_to_domain. 안정 사본 확보 후 로컬 OK; 원본 SENT 정책은 별도 |
| SendToMobile | Core→Transport send_to_mobile. BLE 전송 완료·App 결과를 보증하지 않음 |
| PublishRegistrationConnection | 등록/연결 Owner→send_registration_to_domain. 공동 평가 tuple·원 observation_revision 유지 |
| PublishAppActivity | App Owner→send_app_activity_to_domain. BLE 연결을 App활성으로 합성하지 않음 |
| PublishProximity | 근접 Owner→send_proximity_to_domain. UNKNOWN/INVALID/품질·원 AGE 보존 |
| IsValidByteView | NULL/length/data의 원 형식 검증; 실제 메모리 접근성·차용수명·인증을 보증하지 않음 |
| IsValidMessageContext | 중첩 ID view들의 논리형식. 현재 등록 peer와 같은 ID인지 별도 확인 |
| IsValidRelayMessage | direction/category/context/view의 논리형식. Type별 P·모든 enum·문맥·HOLD를 Codec에서 따로 확인 |

Gateway_Status_t는 OK0(로컬 안정 인계), INVALID_ARGUMENT1(입력 오류), NOT_READY2(해당 경계 미준비), UNSUPPORTED3(미지원 callback/기능), INTERNAL_ERROR4(내부 오류)다. 용량 전용 값은 없고 Net NO_CAPACITY→Gateway NOT_READY는 미승인 후보이므로 실제 반환·재제출 계약을 확인해야 한다. 서로 다른 enum의 숫자 cast는 금지다. 즉시 거부는 후발 차량 FAILED가 아니며, OK 이후 오류는 원 입력에 귀속한 기존 감독/Owner 경계로 보존한다.

<a id="gateway-callback"></a>
## Callback5 types / 7 slots

타입 정본은 [UART-M Data](../data/UART_M_DATA.md)의 Gateway_InterfaceMessageHandler_t, Gateway_InterfaceRelaySender_t, Gateway_InterfaceRegistrationSender_t, Gateway_InterfaceAppActivitySender_t, Gateway_InterfaceProximitySender_t다. 앞 두 타입은 각각 Core2/Transport2 slots, 뒤3타입은 각각1slot으로 총7이다. user_context는 빌린 opaque IN이며 원 Owner 수명을 따라야 한다. callback형식이 같아도 양단 역할을 바꾸지 않는다. 반환은 위 Gateway5개 값이며 NULL callback/해당기능지원·현재문맥 미확인 경로는 정상처리로 승격하지 않는다. 함수주소가 non-NULL이라는 사실은 지원/등록성공/실행안전 증거가 아니다.

## 반례·복구

category VEHICLE_STATE만 보고 CIS_REAR로 decode하기, QUERY_ID/Occurrence를 request_id로 보내기, 과거 RequestSession을 현재 recipient로 덮기, stack ByteView를 queue에 보관하기, Core 반영 뒤 Observer 실패로 재실행하기를 거부한다. 같은 Query의 중복/늦은 END는 private Query에서 처리한다. Driver·연결 회복으로 현재 문맥·원 시간·지원 schema 근거를 자동 복원하지 않는다. 기존 IF-O08/14/23/24/25, CF01, D-05, C2-H01 및 외부 HOLD를 유지한다. 협의 변경은 기존 S6 큐에 기록만 한다.
