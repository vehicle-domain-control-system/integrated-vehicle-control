# VSS_EVENT — 의미 사건의 한 번 전달

[Network](../../00_START_HERE.md) · [M03](../../modules/M03_TYPED_HANDOFF.md) · [M05](../../modules/M05_CAN_CODEC.md) · [Data/전체 필드](../../data/CAN_NODE_DATA.md#VSS_EVENT) · [흐름](../../flows/CAN_CIS_WINDOW_VSS.md#VSS_EVENT)

`WIRE-P01-VSS_EVENT` / `RX-P01-VSS_EVENT-VSS`. DOMAIN→VSS, 후보 ID 0x160 / used23 / frame24 / DLC12. Wire는 다른 MCU 사이의 프레임 전달이며 원격 C 호출이 아니다.

## 책임과 호출 경계

DOMAIN 기능 Owner가 원 의미를 만들고 Network가 M03 typed 인계→M04 보관·시간→M05 Codec→M07/M08 Driver를 맡는다. 수신 측은 역방향으로 검사하여 VSS 로컬 소비자에게 전달한다. ISR은 안정된 장치 관측 확보까지만 맡으며 의미 API는 Task/소유자 문맥에서 호출한다. 실제 Task·Lock·queue 용량·Driver HAL 이름은 미검증이다.

<a id="api-DomainIf_TxVssOneShotEvent"></a>
```c
DomainIf_Status_t DomainIf_TxVssOneShotEvent( const DomainIf_VssOneShotEvent_t *event);
```

출처: S4A api/domain_tx.md#api-DomainIf_TxVssOneShotEvent.
<a id="api-VssApp_OnDomainEvent"></a>
```c
void VssApp_OnDomainEvent(const NetVssEvent *message, const NetCanRxMeta *meta);
```

출처: S4A api/vss.md#api-VssApp_OnDomainEvent.

송신 API 인자는 NULL 불가 IN 객체이며 호출 동안만 읽는다. 수신 callback의 message/meta 또는 state/status/result도 NULL 불가이고, 이후 필요하면 소비자가 반환 전에 복사한다. 공개 DomainIf 본체·Manager 상태는 DOMAIN 소유이고 Network가 중복 구현하지 않는다. NetSubmitResult/C2 반환의 전체 의미는 [기존 반환 정본](../../data/CIS_REAR_DATA.md#declared), 내부 함수 반환은 [공통 자료](../../data/CAN_COMMON_DATA.md)에 있다.

**실행에 필요한 미결:** IF-O02, IF-O16, D-03, D-05, IF-O09, IF-O12: use-limit/실제 시작 기준과 필수 후발 실패 sink 바인딩이 미합의다. C2 UNSUPPORTED 가용성에는 동등 raw가 없다.

<a id="submit"></a>
## 송신 인계

공개 `DomainIf_TxVssOneShotEvent` 본체는 DOMAIN 소유다. Network는 기존 `txVssOneShotEvent` 멤버와 동일한 인자·반환형을 갖는 아래 `PROPOSED_PRIVATE_FUNCTION` 콜백 본체를 제안한다. 등록 성공·실제 지원 확인 전 정상 접수를 하지 않는다.

<a id="api-txVssOneShotEvent"></a>
```c
DomainIf_Status_t (*txVssOneShotEvent)( const DomainIf_VssOneShotEvent_t *event);
```

출처: S4A api/callbacks.md#api-txVssOneShotEvent.

```c
DomainIf_Status_t NetCan_TxVssEvent(const DomainIf_VssOneShotEvent_t *event)
{
    if (event == NULL || C2 선언의 코드·필드 조합이 잘못되면)
        return DOMAIN_IF_INVALID_ARGUMENT;
    if (원 사건 사용 한도·시간 근거·필수 후발 실패 sink가 준비되지 않았으면)
        return DOMAIN_IF_NOT_AVAILABLE;
    if (TxPort·현재 기동·상대 profile·소유자 문맥이 준비되지 않았으면)
        return DOMAIN_IF_NOT_AVAILABLE;
    if (명시 투영에서 원 정보가 손실되거나 source-only enum의 대응이 없으면)
        return DOMAIN_IF_NOT_AVAILABLE; // 외부 계약을 임의 축약하지 않는다.
    // C2 Event enum0..4를 같은 의미 Net1..5로 명시 변환하고 occurrence/meta.quality/ageMs를 보존한다.
    // C2에 USE_LIMIT 필드는 없다. Network의 A1 선택 프로필500ms를 message.use_limit_ms에 명시 투영한다.
    // 프로필 실제 바인딩이 없으면 NOT_AVAILABLE이며 임의 누락 기본값/새 공개 필드를 만들지 않는다.
    // meta.updateSequence는 Event Wire/key로 쓰지 않지만 원 사본에 보존한다.
    // availability3 UNSUPPORTED는 HOLD. hasRelatedCommand가 true인 경우만 실제 relatedCommandId를 연결한다.
    if (전체 의미 사본과 원 식별·시간 기준을 함께 확보할 공간이 없으면)
        return DOMAIN_IF_QUEUE_FULL;
    // 미송신 저장소에 전체 사본을 한 번에 반영한다. 실패 시 기존 항목은 유지한다.
    // caller 주소를 남기지 않는다. 공통 송신 선택이 AGE를 다시 확인한 뒤 Codec를 호출한다.
    return DOMAIN_IF_OK; // 안정된 로컬 인계이며 실제 TX/상대 수신/기능 완료가 아니다.
}
```

인자는 NULL 불가인 IN `DomainIf_VssOneShotEvent_t *`이며 호출 동안만 읽는다. 콜백의 성공 전에 필요한 중첩 문맥도 값으로 확보한다. C2 NOT_INITIALIZED/REJECTED/UNSUPPORTED는 공개 경계의 별도 의미이며 네 NetSubmitResult로 강제 축약하지 않는다. 콜백의 반환 대응은 명시 후보이며 실제 지원 반환·필수 실패 sink는 Owner 바인딩 대상이다.

<a id="codec"></a>
## 메시지 Codec: 내부 함수 후보

두 원형은 `PROPOSED_PRIVATE_FUNCTION`이다. 호출자는 DOMAIN의 M04 송신 선택과 VSS의 M07/M04 수신 처리다. 전체 공통 검사 순서는 [공통 Codec](../CAN_COMMON_CODEC.md#check-frame), 전체 필드 변환은 [단일 필드표](../../data/CAN_NODE_DATA.md#wire-VSS_EVENT)에 있다. 이 함수는 시간원·Driver·기능 API를 호출하지 않는다. 실행은 Network 소유자 Task/Superloop이며 ISR에서 프레임 해석을 하지 않는다.

| 인자 | 방향·범위·소유·수명 |
|---|---|
| message | IN `NetVssEvent` 전체, NULL 불가. 송신 시점까지 AGE를 반영한 호출 내 불변 사본 |
| source | IN `NetCanSource`, NULL 불가. 확인한 로컬 Boot/TX; Codec는 새 번호 발급/소비를 하지 않음 |
| frame | IN `NetCanFrame`, NULL 불가. 실제 Driver flags/길이/수신시각과 바이트가 동일 입력인 안정 사본 |
| profile | IN 값. 후보 종류를 표시할 뿐 양단 채택 증거를 대신하지 않음 |
| out, meta | OUT caller 저장소, NULL 불가. 입력 및 서로와 겹치지 않음. 모든 검사 성공 뒤 함께 반영; 실패 시 불변 |

Encode `NET_CAN_OK`는 완전 프레임 작성, Decode `NET_CAN_OK`는 형식 변환만 뜻한다. INVALID_ARGUMENT은 NULL/alias 또는 송신 값 오류, MALFORMED는 수신 ID/flags/DLC/길이/padding/코드/명시 조합 오류, CONTRACT_HOLD는 형식·실행 계약 미확인, ORDER_HOLD는 송신 Boot/TX 전제 미확인이다. Decode 성공은 admission·신선도·C2 성공이 아니다.

```c
NetCanResult NetCan_EncodeVssEvent(const NetVssEvent *message,
    const NetCanSource *source, NetCanProfile profile, NetCanFrame *out)
{
    if (message == NULL || source == NULL || out == NULL || 입출력이 겹치면)
        return NET_CAN_INVALID_ARGUMENT;
    if (profile에 대한 실제 양단 선택·지원 근거가 없으면)
        return NET_CAN_CONTRACT_HOLD;
    if (source의 Boot가0이거나 송신 순번 수용 전제를 확인하지 못하면)
        return NET_CAN_ORDER_HOLD;
    if (단일 필드표의 모든 코드·범위·sentinel·명시 조합을 만족하지 못하면)
        return NET_CAN_INVALID_ARGUMENT;
    // event_kind1..5, value_quality0..3, input_availability0..2를 보존한다. 관련 명령이 없으면 wire RELATED_COMMAND_ID0 후보를 적용하고 C2 hasRelatedCommand=false인 stale 숫자는 읽지 않는다.
    NetCanFrame tmp = {0};
    tmp.canId = 0x160; tmp.dlc = 12;
    tmp.length = 24;
    tmp.extended = false; tmp.fdf = true; tmp.brs = true;
    tmp.rtr = false; tmp.driverError = false;
    // 수신시각은 송신의 의미 필드가 아니다. 송신용 receivedAtMs는 사용하지 않는다.
    // B0/4에 source.boot_id/tx_sequence를 u32LE로 기록한다.
    for (단일 필드표의 BOOT/TX를 포함한 각 필드) {
        // offset/width와 source 또는 message 멤버를 표에서 하나씩 선택한다.
        // enum은 검증한 고정폭 raw, 음수 i16은 uint16의 2의 보수로 변환한다.
        uint32_t raw = 표에서 선택한 멤버의 검증된 unsigned 비트열;
        for (uint8_t k = 0; k < 표의 width; ++k)
            tmp.data[표의 offset + k] = (uint8_t)(raw >> (8u * k));
    }
    // padding 영역(23..23)이 있으면 초기값0을 유지한다.
    // 전체 byte/flags/필드표 충족을 재확인한 뒤 한 번에 반영한다.
    *out = tmp; // Driver 호출·TX 순번 발급은 없다.
    return NET_CAN_OK;
}

NetCanResult NetCan_DecodeVssEvent(const NetCanFrame *frame,
    NetCanProfile profile, NetVssEvent *out, NetCanRxMeta *meta)
{
    if (frame == NULL || out == NULL || meta == NULL || 저장소가 겹치면)
        return NET_CAN_INVALID_ARGUMENT;
    NetCanResult rc = NetCan_CheckFrame(frame, NET_CAN_VSS_EVENT,
        NET_CAN_NODE_VSS, profile);
    if (rc != NET_CAN_OK) return rc;
    NetVssEvent tmp;
    NetCanRxMeta tmpMeta;
    for (단일 필드표의 각 필드) {
        uint32_t raw = 0;
        for (uint8_t k = 0; k < 표의 width; ++k)
            raw |= (uint32_t)frame->data[표의 offset + k] << (8u * k);
        // unsigned 필드는 해당 고정폭 멤버에 범위 확인 뒤 담는다.
        // i16은 raw<32768이면 raw, 아니면 (int32_t)raw-65536으로 복원한다.
        // 같은 표의 member를 tmp 또는 tmpMeta.source에 저장한다. enum cast는 없다.
    }
    if (단일 필드표의 코드·예약bit·범위·sentinel 조합 또는 Boot0이 발견되면)
        return NET_CAN_MALFORMED;
    // event_kind1..5, value_quality0..3, input_availability0..2를 보존한다. 관련 명령이 없으면 wire RELATED_COMMAND_ID0 후보를 적용하고 C2 hasRelatedCommand=false인 stale 숫자는 읽지 않는다.
    tmpMeta.received_at_ms = frame->receivedAtMs;
    // 이 지점에서 tmp의 모든 멤버와 tmpMeta.source가 채워졌는지 확인한다.
    *out = tmp;
    *meta = tmpMeta; // caller 소유 임시 출력에 함께 반영; 외부 공개는 한 번이다.
    // 순번 장부·liveness·기능 상태는 여기서 갱신하지 않는다.
    return NET_CAN_OK;
}
```

<a id="receive"></a>
## typed 수신과 소비

아래 `PROPOSED_PRIVATE_FUNCTION`은 수신 Network M03의 투영·호출 절차다. 원 public `VssApp_OnDomainEvent` 본체는 기존 VSS INPUT Owner 소유다. message/meta는 같은 입력의 IN 읽기 전용 쌍이며 NULL 불가다. caller는 [공통 수신/시간](../CAN_TRANSFER_CONTROL.md)과 [A1 Event 원 기한](../../../interface/02_NODES/VSS/10_DATA.md#a1-binding)에 따라 임시 소비 Age로 원 L을 확인하지만 callback payload에는 `meta.received_at_ms`의 RX 기준 AGE를 보존한다. INPUT이 RX→owner 경과를 한 번 누적하고 별도 now 발급·중복 가산을 하지 않는다.

C2 VSS_EVENT_VEHICLE_WELCOME/GOODBYE/DOOR_LOCK_COMPLETE/DOOR_UNLOCK_COMPLETE/DOOR_LOCK_ERROR0..4를 Net 대응 이름1..5로 바꾼다. occurrenceId는 보존하고 meta.quality와 meta.ageMs만 원 품질/AGE로 연결한다. meta.updateSequence는 이 Wire와 중복 판정에 사용하지 않으며 원 DTO 값을0으로 강제하지 않는다. availability3 UNSUPPORTED는 HOLD다. hasRelatedCommand=true일 때 실제 relatedCommandId 연결을 보존한다.

```c
NetCanResult NetCan_DeliverVssEvent(const NetVssEvent *message,
    const NetCanRxMeta *meta)
{
    if (message == NULL || meta == NULL || 같은 수신 사건의 쌍이 아니면)
        return NET_CAN_INVALID_ARGUMENT;
    if (입력 코드·명시 조합이 잘못되면) return NET_CAN_INVALID_ARGUMENT;
    if (수신 소유자 문맥·실제 소비자 지원·초기화가 준비되지 않았으면)
        return NET_CAN_NOT_READY;
    if (원 생산 세대·순서·identity 대응이 입증되지 않았으면)
        return NET_CAN_ORDER_HOLD;
    if (소비시점의 미반영 홉·수신 대기 또는 필요한 원 사용 한도를 입증하지 못하면)
        return NET_CAN_TIME_HOLD;
    if (본문의 필수 품질·enum·원본 투영 계약이 미합의이거나 정보가 손실되면)
        return NET_CAN_CONTRACT_HOLD;
    // C2 VSS_EVENT_VEHICLE_WELCOME/GOODBYE/DOOR_LOCK_COMPLETE/DOOR_UNLOCK_COMPLETE/DOOR_LOCK_ERROR0..4를 Net 대응 이름1..5로 바꾼다. occurrenceId는 보존하고 meta.quality와 meta.ageMs만 원 품질/AGE로 연결한다. meta.updateSequence는 이 Wire와 중복 판정에 사용하지 않으며 원 DTO 값을0으로 강제하지 않는다. availability3 UNSUPPORTED는 HOLD다. hasRelatedCommand=true일 때 실제 relatedCommandId 연결을 보존한다.
    VssApp_OnDomainEvent(message, meta);
    // void는 호출 전달의 끝이다. 소비자의 저장·실행·재생 성공을 선언하지 않는다.
    return NET_CAN_OK;
}
```

OK 뒤 해당 인계를 다시 호출하지 않는다. 실패 시 원 사본은 보존하되 API의 미접수·부분 변경 여부가 확인되기 전 자동 재호출을 하지 않는다. 같은 원본의 순서·AGE를 다음 시도에서 다시 확인한다. 새로운 관측·권한·차량 결과를 합성하지 않는다.

## 원본·시간·보관·후발 실패

의미 사건 key는 (PRODUCT 또는 TEST, DOMAIN, 수용한 DOMAIN 원 Boot, occurrence_id)다. event_kind는 같은 key의 내용 검증 값이며 종류만 다른 입력을 새 사건으로 받지 않는다. related_command_id와 CAN TX는 별도 상관/전달 순서다. original_age_ms는 원 발생 후 경과이고 최초 actual에는 `L=min(검증된 양의 use_limit_ms,500ms 설계 프로필,2000ms 상한)`을 적용한다.0은 비사용·L 도달은 만료다. 적법 Started 이후 같은 Session 후속 cue는 최초 시작 검사를 반복하지 않으며 in-flight 만료를 NO_START로 위조하지 않는다. [A1 정본](../../../interface/02_NODES/VSS/10_DATA.md#a1-binding).

통신 주기/수신 중단: 일회성 사건이며 무발생 timeout은 없다. Event 침묵과 실제 DOMAIN 감독·명시 품질·계보/시간 상실을 구별한다. 원 L은 A1 문서 선택이며 실제 RX/actual clock·오차·Binding 시험은 미검증이다. 형식 오류·중복을 정상 heartbeat나 새 semantic fresh로 쓰지 않는다.

명령·사건·결과는 개별 identity와 본문을 유지한다. 다른 사건으로 덮어쓰지 않는다. Driver가 참조 중인 프레임은 AGE나TX를 수정하지 않는다. 저장 공간 부족·사본 준비 실패는 전체 새 접수 실패이며 순번 장부부터 진전시키지 않는다.

접수 OK, Driver accepted, 최종 TX, 참조 해제, 특정 상대 수신, 기능 완료는 각각 별도 사실이다. bus-off·옛 callback·중복 callback·미확인 결과 처리와 AGE 계산의 단일 절차는 [전송 공통](../CAN_TRANSFER_CONTROL.md), [전체 경로](../../flows/CAN_NETWORK_PATHS.md)를 따른다. 현재 품질 보고를 명령 실패 kind로 위조하지 않는다. identity 있는 discrete의 후발 실패는 원 항목에 연결하며 DOMAIN 송신은 D-03/D-05/O-C10 필수 sink가 준비되기 전 안정 접수 성공을 약속하지 않는다. 다른 ECU에 DomainIf 실패 API를 복제하지 않는다.

## 정상·경계·거부 사례

| 사례 | 기대 처리 |
|---|---|
| 정상 또는 조건부 경로 | 원 occurrence17을 TX10/11로 보완 전송해도 한 사건이며 새 재생 요구로 만들지 않는다. VSS App의 void 반환은 실제 음향 시작·완료가 아니다. |
| 고유 경계 | USE_LIMIT0 또는 원 AGE가 L=min(양의USE_LIMIT,500,2000)에 도달하면 새 최초 actual을 제한한다. 실제 시각 구간·오차 근거 미확인은 HOLD이며 송신 원 use-limit에 임의 기본값을 채우지 않는다. |
| 의미 반례 | 환영 이벤트 미발생을 timeout으로 판정하거나 LOCAL TX 성공으로 VSS 재생 완료를 보고하지 않는다. |
| ID/FDF/BRS/DLC/길이/padding 또는 raw 예약값 오류 | 임시 변환 전체 폐기, typed 호출·정상 수신 시각·순번 갱신 없음 |
| 같은 원 identity 재송신·AGE 감소 | 같은 소비시점으로 환산해 원본을 젊게 만들지 않음. 순서/clock/wrap 미확인이면 HOLD |
| Queue full·C2 미준비·소비 거부 | 새 항목 전체 인계 실패. 기존 상태·Driver 참조 버퍼 불변, 반환 원인 보존 |
| accepted 뒤 Driver 실패·옛 callback·bus-off | 실제 관측을 원 제출과 연결하고 참조 해제 전 재사용 금지. 자동 재송신·기능 완료 생성 없음 |

근거: B07 can/VSS_EVENT.md 전문, S4A의 위 API/Data 절, 최신 S4C TIME_IDENTITY_CONTRACT/FRESHNESS_AGE_TIMEOUT/RESOURCE_QUEUE_DRIVER_FLOW/FAILURE_RECOVERY_CONTRACT. 출처 채택·팀 변경은 별도이며 본문에서 기존 공개 선언이나 Wire를 변경하지 않는다.
