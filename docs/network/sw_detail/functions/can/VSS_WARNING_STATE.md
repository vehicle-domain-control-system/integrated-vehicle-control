# VSS_WARNING_STATE — 세 경고 근거와 독립 활성 평가

[Network](../../00_START_HERE.md) · [M03](../../modules/M03_TYPED_HANDOFF.md) · [M05](../../modules/M05_CAN_CODEC.md) · [Data/전체 필드](../../data/CAN_NODE_DATA.md#VSS_WARNING_STATE) · [흐름](../../flows/CAN_CIS_WINDOW_VSS.md#VSS_WARNING_STATE)

`WIRE-P01-VSS_WARNING_STATE` / `RX-P01-VSS_WARNING_STATE-VSS`. DOMAIN→VSS, 후보 ID 0x0C0 / used60 / frame64 / DLC15. Wire는 다른 MCU 사이의 프레임 전달이며 원격 C 호출이 아니다.

## 책임과 호출 경계

DOMAIN 기능 Owner가 원 의미를 만들고 Network가 M03 typed 인계→M04 보관·시간→M05 Codec→M07/M08 Driver를 맡는다. 수신 측은 역방향으로 검사하여 VSS 로컬 소비자에게 전달한다. ISR은 안정된 장치 관측 확보까지만 맡으며 의미 API는 Task/소유자 문맥에서 호출한다. 실제 Task·Lock·queue 용량·Driver HAL 이름은 미검증이다.

<a id="api-DomainIf_TxVssWarningState"></a>
```c
DomainIf_Status_t DomainIf_TxVssWarningState( const DomainIf_VssWarningState_t *state);
```

출처: S4A api/domain_tx.md#api-DomainIf_TxVssWarningState.
<a id="api-VssApp_OnDomainWarningState"></a>
```c
void VssApp_OnDomainWarningState(const NetVssWarningState *message, const NetCanRxMeta *meta);
```

출처: S4A api/vss.md#api-VssApp_OnDomainWarningState.

송신 API 인자는 NULL 불가 IN 객체이며 호출 동안만 읽는다. 수신 callback의 message/meta 또는 state/status/result도 NULL 불가이고, 이후 필요하면 소비자가 반환 전에 복사한다. 공개 DomainIf 본체·Manager 상태는 DOMAIN 소유이고 Network가 중복 구현하지 않는다. NetSubmitResult/C2 반환의 전체 의미는 [기존 반환 정본](../../data/CIS_REAR_DATA.md#declared), 내부 함수 반환은 [공통 자료](../../data/CAN_COMMON_DATA.md)에 있다.

**실행에 필요한 미결:** CF02, IF-O01, IF-O02, IF-O06, IF-O17, WINDOW_ANTIPINCH_HOLD: used51/60 양단 선택 전 정상 encode/decode/typed 소비 HOLD. profile enum 값 자체는 팀 채택 증거가 아니다.

<a id="submit"></a>
## 송신 인계

공개 `DomainIf_TxVssWarningState` 본체는 DOMAIN 소유다. Network는 기존 `txVssWarningState` 멤버와 동일한 인자·반환형을 갖는 아래 `PROPOSED_PRIVATE_FUNCTION` 콜백 본체를 제안한다. 등록 성공·실제 지원 확인 전 정상 접수를 하지 않는다.

<a id="api-txVssWarningState"></a>
```c
DomainIf_Status_t (*txVssWarningState)( const DomainIf_VssWarningState_t *state);
```

출처: S4A api/callbacks.md#api-txVssWarningState.

```c
DomainIf_Status_t NetCan_TxVssWarningState(const DomainIf_VssWarningState_t *state)
{
    if (state == NULL || C2 선언의 코드·필드 조합이 잘못되면)
        return DOMAIN_IF_INVALID_ARGUMENT;
    if (CF02 양단 선택·DTO 표현 계약이 미합의이면)
        return DOMAIN_IF_NOT_AVAILABLE;
    if (TxPort·현재 기동·상대 profile·소유자 문맥이 준비되지 않았으면)
        return DOMAIN_IF_NOT_AVAILABLE;
    if (명시 투영에서 원 정보가 손실되거나 source-only enum의 대응이 없으면)
        return DOMAIN_IF_NOT_AVAILABLE; // 외부 계약을 임의 축약하지 않는다.
    // 세 DomainIf_WarningInputEvidence_t의 source.generation/sequence/ageMs, quality/reason/availability를 각 독립 Wire 묶음으로 연결한다. C2 UNCONFIRMED·UNSUPPORTED는 동등 raw가 없어 CLEAR/UNAVAILABLE로 바꾸지 않는다. rearDetectionActivation과 rearActivationMeta/reason은 독립 DOMAIN 평가다. C2에는 activation_meta_format 멤버가 없고 확장 marker는 채택 profile의 Codec 책임 후보다.
    if (전체 의미 사본과 원 식별·시간 기준을 함께 확보할 공간이 없으면)
        return DOMAIN_IF_QUEUE_FULL;
    // 미송신 저장소에 전체 사본을 한 번에 반영한다. 실패 시 기존 항목은 유지한다.
    // caller 주소를 남기지 않는다. 공통 송신 선택이 AGE를 다시 확인한 뒤 Codec를 호출한다.
    return DOMAIN_IF_OK; // 안정된 로컬 인계이며 실제 TX/상대 수신/기능 완료가 아니다.
}
```

인자는 NULL 불가인 IN `DomainIf_VssWarningState_t *`이며 호출 동안만 읽는다. 콜백의 성공 전에 필요한 중첩 문맥도 값으로 확보한다. C2 NOT_INITIALIZED/REJECTED/UNSUPPORTED는 공개 경계의 별도 의미이며 네 NetSubmitResult로 강제 축약하지 않는다. 콜백의 반환 대응은 명시 후보이며 실제 지원 반환·필수 실패 sink는 Owner 바인딩 대상이다.

<a id="codec"></a>
## 메시지 Codec: 내부 함수 후보

두 원형은 `PROPOSED_PRIVATE_FUNCTION`이다. 호출자는 DOMAIN의 M04 송신 선택과 VSS의 M07/M04 수신 처리다. 전체 공통 검사 순서는 [공통 Codec](../CAN_COMMON_CODEC.md#check-frame), 전체 필드 변환은 [단일 필드표](../../data/CAN_NODE_DATA.md#wire-VSS_WARNING_STATE)에 있다. 이 함수는 시간원·Driver·기능 API를 호출하지 않는다. 실행은 Network 소유자 Task/Superloop이며 ISR에서 프레임 해석을 하지 않는다.

| 인자 | 방향·범위·소유·수명 |
|---|---|
| message | IN `NetVssWarningState` 전체, NULL 불가. 송신 시점까지 AGE를 반영한 호출 내 불변 사본 |
| source | IN `NetCanSource`, NULL 불가. 확인한 로컬 Boot/TX; Codec는 새 번호 발급/소비를 하지 않음 |
| frame | IN `NetCanFrame`, NULL 불가. 실제 Driver flags/길이/수신시각과 바이트가 동일 입력인 안정 사본 |
| profile | IN 값. 후보 종류를 표시할 뿐 양단 채택 증거를 대신하지 않음 |
| out, meta | OUT caller 저장소, NULL 불가. 입력 및 서로와 겹치지 않음. 모든 검사 성공 뒤 함께 반영; 실패 시 불변 |

Encode `NET_CAN_OK`는 완전 프레임 작성, Decode `NET_CAN_OK`는 형식 변환만 뜻한다. INVALID_ARGUMENT은 NULL/alias 또는 송신 값 오류, MALFORMED는 수신 ID/flags/DLC/길이/padding/코드/명시 조합 오류, CONTRACT_HOLD는 형식·실행 계약 미확인, ORDER_HOLD는 송신 Boot/TX 전제 미확인이다. Decode 성공은 admission·신선도·C2 성공이 아니다.

```c
NetCanResult NetCan_EncodeVssWarningState(const NetVssWarningState *message,
    const NetCanSource *source, NetCanProfile profile, NetCanFrame *out)
{
    if (message == NULL || source == NULL || out == NULL || 입출력이 겹치면)
        return NET_CAN_INVALID_ARGUMENT;
    if (CF02 양단 선택 증거가 없거나 profile != NET_CAN_PROFILE_WARNING60)
        return NET_CAN_CONTRACT_HOLD; // WARNING51은 빠진 활성 메타를 현 DTO에 채울 계약이 없다.
    if (profile에 대한 실제 양단 선택·지원 근거가 없으면)
        return NET_CAN_CONTRACT_HOLD;
    if (source의 Boot가0이거나 송신 순번 수용 전제를 확인하지 못하면)
        return NET_CAN_ORDER_HOLD;
    if (단일 필드표의 모든 코드·범위·sentinel·명시 조합을 만족하지 못하면)
        return NET_CAN_INVALID_ARGUMENT;
    // 세 품질0..3·사유0..9·가용성0..2, 끼임/탑승자/활성 flag0/1, 후방 상태0..2를 검증한다. 확장후보 B51 format1과 format0의 근거 부재를 구분한다.
    NetCanFrame tmp = {0};
    tmp.canId = 0x0C0; tmp.dlc = 15;
    tmp.length = 64;
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
    // padding 영역(60..63)이 있으면 초기값0을 유지한다.
    // 전체 byte/flags/필드표 충족을 재확인한 뒤 한 번에 반영한다.
    *out = tmp; // Driver 호출·TX 순번 발급은 없다.
    return NET_CAN_OK;
}

NetCanResult NetCan_DecodeVssWarningState(const NetCanFrame *frame,
    NetCanProfile profile, NetVssWarningState *out, NetCanRxMeta *meta)
{
    if (frame == NULL || out == NULL || meta == NULL || 저장소가 겹치면)
        return NET_CAN_INVALID_ARGUMENT;
    if (CF02 양단 선택 증거가 없거나 profile != NET_CAN_PROFILE_WARNING60)
        return NET_CAN_CONTRACT_HOLD; // WARNING51은 빠진 활성 메타를 현 DTO에 채울 계약이 없다.
    NetCanResult rc = NetCan_CheckFrame(frame, NET_CAN_VSS_WARNING_STATE,
        NET_CAN_NODE_VSS, profile);
    if (rc != NET_CAN_OK) return rc;
    NetVssWarningState tmp;
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
    // 세 품질0..3·사유0..9·가용성0..2, 끼임/탑승자/활성 flag0/1, 후방 상태0..2를 검증한다. 확장후보 B51 format1과 format0의 근거 부재를 구분한다.
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

아래 `PROPOSED_PRIVATE_FUNCTION`은 수신 Network M03의 투영·호출 절차다. 원 public `VssApp_OnDomainWarningState` 본체는 해당 Application Owner 소유다. message/meta는 admission에 성공한 동일 입력의 IN 읽기 전용 쌍이고 NULL 불가다. caller는 원 수신시각·원 AGE를 보존하며 [공통 수신/시간 절차](../CAN_TRANSFER_CONTROL.md)로 소비시점의 미반영 경과를 정확히 한 번 계산한 소유자 문맥이다. 이 함수는 별도 현재 시각을 발급하지 않는다.

세 DomainIf_WarningInputEvidence_t의 source.generation/sequence/ageMs, quality/reason/availability를 각 독립 Wire 묶음으로 연결한다. C2 UNCONFIRMED·UNSUPPORTED는 동등 raw가 없어 CLEAR/UNAVAILABLE로 바꾸지 않는다. rearDetectionActivation과 rearActivationMeta/reason은 독립 DOMAIN 평가다. C2에는 activation_meta_format 멤버가 없고 확장 marker는 채택 profile의 Codec 책임 후보다.

```c
NetCanResult NetCan_DeliverVssWarningState(const NetVssWarningState *message,
    const NetCanRxMeta *meta)
{
    if (message == NULL || meta == NULL || 같은 수신 사건의 쌍이 아니면)
        return NET_CAN_INVALID_ARGUMENT;
    if (CF02 양단 선택·수신 DTO의 확장 메타 존재·소비 규칙이 미확인이라면)
        return NET_CAN_CONTRACT_HOLD; // profile 인자를 새 공개 API에 추가하지 않는다.
    if (입력 코드·명시 조합이 잘못되면) return NET_CAN_INVALID_ARGUMENT;
    if (수신 소유자 문맥·실제 소비자 지원·초기화가 준비되지 않았으면)
        return NET_CAN_NOT_READY;
    if (원 생산 세대·순서·identity 대응이 입증되지 않았으면)
        return NET_CAN_ORDER_HOLD;
    if (소비시점의 미반영 홉·수신 대기 또는 필요한 원 사용 한도를 입증하지 못하면)
        return NET_CAN_TIME_HOLD;
    if (본문의 필수 품질·enum·원본 투영 계약이 미합의이거나 정보가 손실되면)
        return NET_CAN_CONTRACT_HOLD;
    // 세 DomainIf_WarningInputEvidence_t의 source.generation/sequence/ageMs, quality/reason/availability를 각 독립 Wire 묶음으로 연결한다. C2 UNCONFIRMED·UNSUPPORTED는 동등 raw가 없어 CLEAR/UNAVAILABLE로 바꾸지 않는다. rearDetectionActivation과 rearActivationMeta/reason은 독립 DOMAIN 평가다. C2에는 activation_meta_format 멤버가 없고 확장 marker는 채택 profile의 Codec 책임 후보다.
    VssApp_OnDomainWarningState(message, meta);
    // void는 호출 전달의 끝이다. 소비자의 저장·실행·재생 성공을 선언하지 않는다.
    return NET_CAN_OK;
}
```

OK 뒤 해당 인계를 다시 호출하지 않는다. 실패 시 원 사본은 보존하되 API의 미접수·부분 변경 여부가 확인되기 전 자동 재호출을 하지 않는다. 같은 원본의 순서·AGE를 다음 시도에서 다시 확인한다. 새로운 관측·권한·차량 결과를 합성하지 않는다.

## 원본·시간·보관·후발 실패

끼임·탑승자·후방 각 source_boot/source_sequence와 DOMAIN rear_activation_sequence는 서로 다른 판단 근거다. 세 original_age_ms와 rear_activation_age_ms를 각각 누적한다. 후방 거리 근거의 시각을 활성 평가의 시각으로 쓰지 않는다.

통신 주기/수신 중단: 50 / 150ms 후보. CF02와 WINDOW 끼임 실행 보류가 해제되지 않는다. 이 수치는 채택·실측되지 않은 후보이며 `>`/`>=` 경계나 원 TTL을 임의 추가하지 않는다. 형식 오류·중복을 정상 heartbeat나 semantic fresh로 쓰지 않는다.

현재 상태는 동일 의미 key의 아직 송신을 시작하지 않은 항목만 교체 후보가 된다. 채널별 원 순서와 품질을 모두 보존해야 한다. Driver가 참조 중인 프레임은 AGE나TX를 수정하지 않는다. 저장 공간 부족·사본 준비 실패는 전체 새 접수 실패이며 순번 장부부터 진전시키지 않는다.

접수 OK, Driver accepted, 최종 TX, 참조 해제, 특정 상대 수신, 기능 완료는 각각 별도 사실이다. bus-off·옛 callback·중복 callback·미확인 결과 처리와 AGE 계산의 단일 절차는 [전송 공통](../CAN_TRANSFER_CONTROL.md), [전체 경로](../../flows/CAN_NETWORK_PATHS.md)를 따른다. 현재 품질 보고를 명령 실패 kind로 위조하지 않는다. identity 있는 discrete의 후발 실패는 원 항목에 연결하며 DOMAIN 송신은 D-03/D-05/O-C10 필수 sink가 준비되기 전 안정 접수 성공을 약속하지 않는다. 다른 ECU에 DomainIf 실패 API를 복제하지 않는다.

## 정상·경계·거부 사례

| 사례 | 기대 처리 |
|---|---|
| 정상 또는 조건부 경로 | 현재 정상 진입은 CF02 HOLD다. used60이 향후 채택되어도 세 위험의 원 근거와 후방 활성 평가를 별도로 전달하며 VSS가 거리를 재평가하지 않는다. |
| 고유 경계 | 두 후보 모두64B다. used51은B51..63 padding0, used60은B51..59 의미+ B60..63 padding0이다. format0을 활성 품질OK로 합성하지 않는다. |
| 의미 반례 | DLC15나 B51=1로used60을 자동 선택하지 않는다. B51..59가0이어도used51을 선택하지 않는다. |
| ID/FDF/BRS/DLC/길이/padding 또는 raw 예약값 오류 | 임시 변환 전체 폐기, typed 호출·정상 수신 시각·순번 갱신 없음 |
| 같은 원 identity 재송신·AGE 감소 | 같은 소비시점으로 환산해 원본을 젊게 만들지 않음. 순서/clock/wrap 미확인이면 HOLD |
| Queue full·C2 미준비·소비 거부 | 새 항목 전체 인계 실패. 기존 상태·Driver 참조 버퍼 불변, 반환 원인 보존 |
| accepted 뒤 Driver 실패·옛 callback·bus-off | 실제 관측을 원 제출과 연결하고 참조 해제 전 재사용 금지. 자동 재송신·기능 완료 생성 없음 |

근거: B07 can/VSS_WARNING_STATE.md 전문, S4A의 위 API/Data 절, 최신 S4C TIME_IDENTITY_CONTRACT/FRESHNESS_AGE_TIMEOUT/RESOURCE_QUEUE_DRIVER_FLOW/FAILURE_RECOVERY_CONTRACT. 출처 채택·팀 변경은 별도이며 본문에서 기존 공개 선언이나 Wire를 변경하지 않는다.
