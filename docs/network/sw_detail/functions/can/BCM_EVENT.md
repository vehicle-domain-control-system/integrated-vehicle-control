# BCM_EVENT — 송신·Codec·수신 상세설계

`WIRE-P01-BCM_EVENT` · 수신 `RX-P01-BCM_EVENT-DOMAIN`.
원 후보: ID `0x141`, used_len `25`B, CAN frame `32`B, DLC 코드 `13`. padding B25..B31는 모두0. ID/offset/시간/profile은 `WIRE_PROPOSED/CAN_PROFILE_NOT_TEAM_VERIFIED`이며 외부 계약 확정이 아니다. 근거 `B07 can/BCM_EVENT.md#wire`, `B07 CAN_CODEC_SPEC.md`, `S4C TIME_IDENTITY_CONTRACT.md`, `S4C RESOURCE_QUEUE_DRIVER_FLOW.md`.

[자료 정본](../../data/CAN_BCM_DATA.md#type-NetBcmEvent) · [공통 Codec](../CAN_COMMON_CODEC.md#check-frame) · [전송·수명](../CAN_TRANSFER_CONTROL.md) · [전체 공통 흐름](../../flows/CAN_NETWORK_PATHS.md) · [BCM·Permission 흐름](../../flows/CAN_BCM_AND_PERMISSION.md)

## 입력·출력과 실행 문맥

Network의 Codec·queue·driver 인계·수신 검증·typed 투영이 이 문서의 본체다. 기존 공개 API 원형을 변경하지 않는다. DOMAIN 공개 DomainIf_Tx 본체는 DOMAIN 소유이고 Network는 TxPort callback 본체와 등록 요청을 제공한다. 상대 ECU의 App/DomainIf_Rx 함수는 해당 노드 내부에서만 호출한다. DOMAIN C2 호출은 초기 Superloop의 ISR 밖 단일 담당 문맥에서 직렬 처리하며 ISR에서 직접 호출하지 않는다. BCM/CIS/WINDOW/VSS의 Task 또는 superloop 실제 바인딩·동기화는 해당 노드 합의 전 계약 HOLD다.


<a id="api-BcmNet_SendEvent"></a>
### `BcmNet_SendEvent`

기존 선언 · 본체 담당 `BCM Network` · `S4A api/bcm.md#api-BcmNet_SendEvent`.

```c
NetSubmitResult BcmNet_SendEvent(
    const NetBcmEvent *message
);
```


<a id="api-DomainIf_RxBcmProtectionEvent"></a>
### `DomainIf_RxBcmProtectionEvent`

기존 선언 · 본체 담당 `DOMAIN (공개 본체 소유 합의 완료)` · `S4A api/domain_rx.md#api-DomainIf_RxBcmProtectionEvent`.

```c
DomainIf_Status_t DomainIf_RxBcmProtectionEvent( const DomainIf_BcmProtectionEvent_t *event);
```


## Wire 필드와 완전 자료형

아래 표는 원 후보의 모든 사용 byte를 지정한다. 헤더 BOOT_ID/TX_SEQUENCE는 DTO에 추가하지 않고 NetCanRxMeta.source에 보존한다. 구조체 memcpy/직접 cast는 사용하지 않는다.

| Offset / 폭(B) | Wire 필드 → typed 멤버(타입) | Raw / 부호 / 순서 | 코드·범위·단위 | sentinel / unknown | T33 행 |
|---|---|---|---|---|---|
| 0 / 4 | `BOOT_ID` → `meta.source.boot_id` (`uint32_t`) | u32 / unsigned / LE | 0 예약; 단위=NOT_SPECIFIED | 0 예약 | 274 |
| 4 / 4 | `TX_SEQUENCE` → `meta.source.tx_sequence` (`uint32_t`) | u32 / unsigned / LE | 최초1, 프레임마다 증가; wrap 정책 OPEN; 단위=NOT_SPECIFIED | NOT_SPECIFIED | 274 |
| 8 / 1 | `EVENT_KIND` → `event_kind` (`NetBcmEventKind`) | u8 / unsigned / N/A | 1=NET_BCM_EVENT_OVERHEAT_DETECTED; 2=NET_BCM_EVENT_FAN_MISMATCH_DETECTED; 3=NET_BCM_EVENT_RECOVERY_CONFIRMED; 단위=NOT_SPECIFIED | NOT_SPECIFIED | 275 |
| 9 / 4 | `OCCURRENCE_ID` → `occurrence_id` (`uint32_t`) | u32 / unsigned / LE | 이벤트 식별; 단위=NOT_SPECIFIED | NOT_SPECIFIED | 276 |
| 13 / 2 | `EVENT_AGE` → `event_age_ms` (`uint16_t`) | u16 / unsigned / LE | 발생 후 경과 시간, ms; 단위=ms | 65535 포화 하한; 정확한 경과 상한 또는 fresh의 증거 아님 | 277 |
| 15 / 1 | `EVENT_QUALITY` → `event_quality` (`NetValueQuality`) | u8 / unsigned / N/A | 0=NET_QUALITY_OK; 1=NET_QUALITY_STALE; 2=NET_QUALITY_INVALID; 3=NET_QUALITY_NO_DATA; 단위=NOT_SPECIFIED | NOT_SPECIFIED | 278 |
| 16 / 4 | `RELATED_DOMAIN_BOOT` → `related_domain_boot` (`uint32_t`) | u32 / unsigned / LE | 연결 명령의 Domain 기동 번호; 없으면 0; 단위=NOT_SPECIFIED | 0 = 관련 명령/사건 없음 (원문 규칙) | 279 |
| 20 / 4 | `RELATED_COMMAND_ID` → `related_command_id` (`uint32_t`) | u32 / unsigned / LE | 연결된 명령, 없으면 0; 단위=NOT_SPECIFIED | 0 = 관련 명령/사건 없음 (원문 규칙) | 280 |
| 24 / 1 | `AFFECTED_FUNCTION` → `affected_function` (`uint8_t`) | u8 / unsigned / N/A | bit0=도어; bit1=공조; bit2=조명; 상위 비트는 미정 (BCM 기능 비트 근거); 단위=NOT_SPECIFIED | NOT_SPECIFIED | 281 |

| C2 선언 멤버 / 타입 | 확인한 raw·Net 대응 또는 표현 제한 |
|---|---|
| `DomainIf_BcmProtectionEvent_t.type` / `DomainIf_BcmProtectionEventType_t` | EVENT_KIND; 이름 대응 |
| `DomainIf_BcmProtectionEvent_t.occurrenceId` / `OccurrenceId_t` | OCCURRENCE_ID; 원 사건 번호 보존 |
| `DomainIf_BcmProtectionEvent_t.sourceGeneration` / `SourceGeneration_t` | 원 BCM BOOT_ID 연결 후보; admitted 기동 바인딩 HOLD |
| `DomainIf_BcmProtectionEvent_t.quality` / `DataQuality_t` | EVENT_QUALITY; 이름 대응 |
| `DomainIf_BcmProtectionEvent_t.ageMs` / `uint32_t` | EVENT_AGE; uint16 포화 의미 보존 |
| `DomainIf_BcmProtectionEvent_t.hasRelatedCommand` / `bool` | RELATED_DOMAIN_BOOT/RELATED_COMMAND_ID의 원 0/존재 근거를 확인할 후보 |
| `DomainIf_BcmProtectionEvent_t.relatedCommandId` / `DomainCommandId_t` | RELATED_COMMAND_ID; 다른 송신 순번으로 치환 금지 |
| `DomainIf_BcmProtectionEvent_t.affectedFunctionMask` / `uint8_t` | AFFECTED_FUNCTION; 도어/공조/조명 비트 의미 |

`NetBcmEvent`와 관련 C2 타입의 전체 선언·필드 표·enum은 [BCM 자료 정본](../../data/CAN_BCM_DATA.md)에 한 번만 정의한다. 공통 품질/원본 Context는 [공통 자료](../../data/CAN_COMMON_DATA.md), Meta/Status는 [B11 자료](../../data/CIS_REAR_DATA.md)에 있다.

## 메시지별 변환과 판단 경계

보호 event type1..3을 C2 이름으로 변환하며 occurrence와 원 BCM 기동의 연결을 보존한다. RELATED_DOMAIN_BOOT/RELATED_COMMAND_ID가 모두0이면 hasRelatedCommand=false이고, 둘 모두 존재하며 원 ledger가 일치할 때만 true와 relatedCommandId를 채운다. 한쪽만0인 결합은 계약 HOLD다. affected_function bit0도어/bit1공조/bit2조명만 표현하며 상위 bits를 유효 기능으로 해석하지 않는다. sourceGeneration은 승인된 원 BCM boot 투영을 사용한다. 복구 사건은 복구 사실의 보고이며 옛 climate 명령 재개 지시가 아니다. DoorEvidence는 BCM_RESULT로만 유지한다.

<a id="codec"></a>
## 순수 Codec 원형과 한국어 C 의사코드

아래 함수만 `PROPOSED_PRIVATE_FUNCTION`이며 제품에 존재한다는 주장이 아니다. 입력은 호출 동안 빌리고 출력은 성공 때만 한 번 commit한다. 호출자는 입력과 모든 출력 객체의 저장 영역을 서로 겹치지 않게 제공한다. Decode 성공은 바이트 해석 성공이며 새 기동/순서/AGE/기능 소비 승인과 별개다. 공통 CheckFrame은 선언된 [정본](../CAN_COMMON_CODEC.md#check-frame)을 호출한다. 모든 u16/u32는 unsigned LE이며 정수 승격 후 이동한다. 지원되지 않는 raw와 padding은 전체 프레임을 거부한다. 정확한 65535 AGE는 포화 하한으로 보존하며 fresh로 승인하지 않는다.

```c
/* PROPOSED_PRIVATE_FUNCTION */
NetCanResult NetCan_EncodeBcmEvent(const NetBcmEvent *message, const NetCanSource *source, NetCanProfile profile, NetCanFrame *frame);
NetCanResult NetCan_DecodeBcmEvent(const NetCanFrame *frame, NetCanProfile profile, NetBcmEvent *message, NetCanRxMeta *meta);
```


```c
NetCanResult NetCan_EncodeBcmEvent(const NetBcmEvent *message, const NetCanSource *source, NetCanProfile profile, NetCanFrame *frame)
{
    if (message == NULL || source == NULL || frame == NULL) return NET_CAN_INVALID_ARGUMENT;
    if (source->boot_id == 0) return NET_CAN_MALFORMED;
    if (profile == NET_CAN_PROFILE_UNSELECTED) return NET_CAN_CONTRACT_HOLD;
    if ((message->event_kind < 1 || message->event_kind > 3)) return NET_CAN_MALFORMED;
    if (message->event_quality > 3) return NET_CAN_MALFORMED;
    if (message->affected_function > 7) return NET_CAN_MALFORMED;
    NetCanFrame local = {0};
    local.canId = 0x141; local.dlc = 13; local.length = 32;
    local.extended = false; local.fdf = true; local.brs = true; local.rtr = false;
    // 원 CAN FD profile의 BRS=1 후보. 양단 선택/채택 근거가 없으면 공통 CheckFrame이 CONTRACT_HOLD.
    // local.receivedAtMs는 Tx 관측시각이 아니므로 Rx 시각을 만들지 않는다.
    for (uint8_t k = 0; k < 4; ++k) local.data[0+k] = (uint8_t)(((uint32_t)source->boot_id >> (8u*k)) & 0xFFu);
    for (uint8_t k = 0; k < 4; ++k) local.data[4+k] = (uint8_t)(((uint32_t)source->tx_sequence >> (8u*k)) & 0xFFu);
    for (uint8_t k = 0; k < 1; ++k) local.data[8+k] = (uint8_t)(((uint32_t)message->event_kind >> (8u*k)) & 0xFFu);
    for (uint8_t k = 0; k < 4; ++k) local.data[9+k] = (uint8_t)(((uint32_t)message->occurrence_id >> (8u*k)) & 0xFFu);
    for (uint8_t k = 0; k < 2; ++k) local.data[13+k] = (uint8_t)(((uint32_t)message->event_age_ms >> (8u*k)) & 0xFFu);
    for (uint8_t k = 0; k < 1; ++k) local.data[15+k] = (uint8_t)(((uint32_t)message->event_quality >> (8u*k)) & 0xFFu);
    for (uint8_t k = 0; k < 4; ++k) local.data[16+k] = (uint8_t)(((uint32_t)message->related_domain_boot >> (8u*k)) & 0xFFu);
    for (uint8_t k = 0; k < 4; ++k) local.data[20+k] = (uint8_t)(((uint32_t)message->related_command_id >> (8u*k)) & 0xFFu);
    for (uint8_t k = 0; k < 1; ++k) local.data[24+k] = (uint8_t)(((uint32_t)message->affected_function >> (8u*k)) & 0xFFu);
    // 0으로 초기화한 padding을 유지. 수신시간/driver 상태는 송신 성공 근거 아님.
    NetCanResult checked = NetCan_CheckFrame(&local, NET_CAN_BCM_EVENT, NET_CAN_NODE_DOMAIN, profile);
    if (checked != NET_CAN_OK) return checked;
    *frame = local;
    return NET_CAN_OK;
}

NetCanResult NetCan_DecodeBcmEvent(const NetCanFrame *frame, NetCanProfile profile, NetBcmEvent *message, NetCanRxMeta *meta)
{
    if (frame == NULL || message == NULL || meta == NULL) return NET_CAN_INVALID_ARGUMENT;
    NetCanResult checked = NetCan_CheckFrame(frame, NET_CAN_BCM_EVENT, NET_CAN_NODE_DOMAIN, profile);
    if (checked != NET_CAN_OK) return checked;
    NetBcmEvent local = {0}; NetCanRxMeta localMeta = {0};
    { uint32_t value = 0;
      for (uint8_t k = 0; k < 4; ++k) value |= ((uint32_t)frame->data[0+k] << (8u*k));
      localMeta.source.boot_id = value; }
    { uint32_t value = 0;
      for (uint8_t k = 0; k < 4; ++k) value |= ((uint32_t)frame->data[4+k] << (8u*k));
      localMeta.source.tx_sequence = value; }
    { uint32_t value = 0;
      for (uint8_t k = 0; k < 1; ++k) value |= ((uint32_t)frame->data[8+k] << (8u*k));
      local.event_kind = value; }
    { uint32_t value = 0;
      for (uint8_t k = 0; k < 4; ++k) value |= ((uint32_t)frame->data[9+k] << (8u*k));
      local.occurrence_id = value; }
    { uint32_t value = 0;
      for (uint8_t k = 0; k < 2; ++k) value |= ((uint32_t)frame->data[13+k] << (8u*k));
      local.event_age_ms = value; }
    { uint32_t value = 0;
      for (uint8_t k = 0; k < 1; ++k) value |= ((uint32_t)frame->data[15+k] << (8u*k));
      local.event_quality = value; }
    { uint32_t value = 0;
      for (uint8_t k = 0; k < 4; ++k) value |= ((uint32_t)frame->data[16+k] << (8u*k));
      local.related_domain_boot = value; }
    { uint32_t value = 0;
      for (uint8_t k = 0; k < 4; ++k) value |= ((uint32_t)frame->data[20+k] << (8u*k));
      local.related_command_id = value; }
    { uint32_t value = 0;
      for (uint8_t k = 0; k < 1; ++k) value |= ((uint32_t)frame->data[24+k] << (8u*k));
      local.affected_function = value; }
    if (localMeta.source.boot_id == 0) return NET_CAN_MALFORMED;
    if ((local.event_kind < 1 || local.event_kind > 3)) return NET_CAN_MALFORMED;
    if (local.event_quality > 3) return NET_CAN_MALFORMED;
    if (local.affected_function > 7) return NET_CAN_MALFORMED;
    for (uint8_t k = 25; k < 32; ++k) if (frame->data[k] != 0) return NET_CAN_MALFORMED;
    localMeta.received_at_ms = frame->receivedAtMs; // 하드웨어/driver 경계에서 확보한 로컬 시각
    *message = local; *meta = localMeta;
    return NET_CAN_OK;
}
```

## 기존 Net 송신 본체의 한국어 C 의사코드

아래는 위에 선언한 기존 `BcmNet_SendEvent`의 Network 소유 본체 설계다. DTO 구조체의 값 복사는 자기 소유 의미 사본이며 Wire 통복사가 아니다. 공통 전송 제어의 Admission·AGE·dispatch·후발 실패 절차를 순서대로 구체화한다.

```c
NetSubmitResult BcmNet_SendEvent(const NetBcmEvent *message)
{
    if (message == NULL) return NET_SUBMIT_INVALID_ARGUMENT;
    // 아래 Codec에서 열거한 각 필드의 전체 허용 집합/범위와 메시지별 조합을 먼저 검사한다.
    if (/* 값 또는 금지 조합 오류 */) return NET_SUBMIT_INVALID_ARGUMENT;
    // 원 occurrence/기동·age와 optional related_domain_boot/command_id를 한 묶음으로 보존한다. 복구사건으로 목표를 재개하지 않는다.
    if (/* 실제 profile 채택·생산기동·원시간·필수 consumer/실패 경계가 준비되지 않음 */)
        return NET_SUBMIT_NOT_READY;
    // 원 command/occurrence identity별 discrete item에 필요한 안정 보관소를 확보한다.
    if (/* 해당 item과 원본 시간/identity를 보관할 공간 부족 */) return NET_SUBMIT_NO_CAPACITY;
    NetBcmEvent owned = *message;
    // owned와 원본 AGE 기준·평가/명령/사건 식별자를 같은 item에 값으로 복사한다.
    // 원 포인터를 queue에 남기지 않고, 임시 예약 이후 실패하면 예약만 반환한다.
    // 공통의 유효 순서/교체 정책을 확인하고 owned item을 원자적으로 게시한다.
    // 이 게시가 완료되지 않으면 OK를 반환하지 않는다.
    return NET_SUBMIT_OK;
}

// 이 item의 F12 dispatch 계속:
// 현재 peer/원문맥·원 AGE와 누적 경과를 재검사한다. 미확정시간/만료는 실제 송신 전 정지한다.
// 승인된 생산 BOOT 및 전송 순서를 공통 정책으로 취득한 뒤 별도 임시 frame을 확보한다.
// NetCan_EncodeBcmEvent에 owned DTO와 source, 채택 profile, 임시 frame을 전달한다.
// OK일 때만 frame을 고정하여 driver에 인계하고 token/epoch/원 identity를 보존한다.
// 그 밖에는 아래 공통 후발 실패 절차로 원 접수 item의 종료/통지를 처리한다.
```

<a id="tx"></a>
## 송신 접수부터 후발 실패까지

다음은 이 메시지가 수행하는 절차다. queue/driver 상태 전이의 단일 정본은 [전송 제어](../CAN_TRANSFER_CONTROL.md)이며 별도의 미선언 helper를 호출하지 않는다.

```c
// 1. 기존 Net submit 본체 또는 Network 소유 TxPort callback에서 실행한다.
//    NULL/명시 raw 범위/필드 조합 오류면 INVALID_ARGUMENT, 바인딩·기동·시간 근거 부재면 NOT_READY.
// 2. 호출 중 원본 DTO/Context, 원 command/occurrence/evaluation identity와 AGE 기준을 읽는다.
//    기능 필드와 원 요청 추적자료를 자기 소유 항목으로 복사한다. 원 포인터를 queue에 넣지 않는다.
// 3. 공통 Admission/AGE 단계의 동일 로컬 now에서 누적 나이를 계산한다.
//    commandAge와 sourceAge를 섞지 않는다. 65535는 포화하한, driver 미측정 공백은 UNKNOWN.
//    profile/순서/기동/허용시간 비교 경계가 미확정이면 보관 승인을 완료하지 않는다.
// 4. 이 문서의 모든 필드 및 메시지별 의미 조합을 재검사하고 private Encode를 호출한다.
//    byte/상위문맥의 snapshot과 queue 슬롯을 함께 확보한 뒤 원자적으로 게시한다.
//    실패 시 예약 자원만 되돌리며 기존 항목·원본 요청에 성공 표시를 남기지 않는다.
// 5. Net API: 복사·로컬 보관 완료만 NET_SUBMIT_OK; 미준비 NET_SUBMIT_NOT_READY;
//    공간 부족 NET_SUBMIT_NO_CAPACITY. Domain callback: 명시 대응이 확인된 경우만 DOMAIN_IF_OK,
//    INVALID_ARGUMENT/NOT_AVAILABLE/QUEUE_FULL 등을 기존 enum 의미에 따라 반환한다.
//    callback 등록/반환 대응이 미확정이면 DOMAIN 공개 본체가 성공하도록 위조하지 않는다.
// 6. dispatcher는 보내기 직전에 원 기동·현재 문맥·expiry와 누적 AGE를 다시 검사한다.
//    Encoded snapshot을 고정하고 driver 인계 식별자·세대를 저장한 뒤 driver에 제출한다.
// 7. driver 접수/최종 Tx 결과/참조 해제/상대 소비를 독립 처리한다.
//    이 경로는 보수적으로 최종 결과와 release를 모두 기다린 뒤 buffer를 재사용한다.
//    늦은 중복 callback은 항목 세대와 일치 여부를 먼저 확인하여 한 번만 반영한다.
// 8. 최종 실패 시 원 항목 식별자·문맥·실패 단계와 전달 여부를 보존한다.
//    Timeout을 release 증거 또는 상대 기능 실패의 확정으로 바꾸지 않는다.
//    공통/B11은 실제 release가 먼저 입증되면 원 identity/결과 pending을 별도 보존하여 물리 buffer를 먼저 회수할 수도 있다.
```

위 의사코드의 기존 반환 상수 이름은 [NetSubmitResult/DomainIf_Status_t 정본](../../data/CIS_REAR_DATA.md)을 따른다. 공개 원형별 상태 대응은 다음 오류 표에 명시한다. 일반 명령 공통부 12B는 BCM 3 command와 WINDOW_COMMAND에만 해당하며 Permission/Result/Event/Status에 덧붙이지 않는다.


| 관측 경계 | Net 공개 submit | DOMAIN TxPort callback / private Rx | 부수효과 |
|---|---|---|---|
| 필수 포인터 NULL·정의되지 않은 내부 선택 인자 | NET_SUBMIT_INVALID_ARGUMENT | DOMAIN_IF_INVALID_ARGUMENT / NET_CAN_INVALID_ARGUMENT | 해당 API 인자 검사에서 거부; OUT·슬롯·순서·정상 heartbeat·기능 호출 변화 없음 |
| 공개 typed 송신 값의 범위·명시 금지조합 | NET_SUBMIT_INVALID_ARGUMENT | DOMAIN_IF_INVALID_ARGUMENT | 공개 송신 검사에서 거부; private codec의 값 검증 실패는 NET_CAN_MALFORMED이며 public 반환과 구별 |
| 수신 Wire 형식·예약코드·범위·명시 금지조합 | 해당 없음 | NET_CAN_MALFORMED | 앞선 계약 HOLD 검사를 통과한 입력의 형식 검증 실패; 임시 변환 폐기, OUT·순서·정상 heartbeat·기능 호출 변화 없음 |
| 준비·계약·기동·AGE 근거 부재 | NET_SUBMIT_NOT_READY | DOMAIN_IF_NOT_AVAILABLE 또는 NOT_INITIALIZED의 실제 바인딩 필요 / NET_CAN_CONTRACT_HOLD·TIME_HOLD·ORDER_HOLD | 의미/순서 상태를 정상으로 commit하지 않음 |
| 자기 소유 공간 부족 | NET_SUBMIT_NO_CAPACITY | DOMAIN_IF_QUEUE_FULL / NET_CAN_NO_CAPACITY | 다른 discrete 항목을 조용히 덮어쓰지 않음 |
| 복사·로컬 보관 완료 | NET_SUBMIT_OK | DOMAIN_IF_OK | 전송·상대 실행 완료 아님 |
| Decode 및 admission 완료, 소비자 거부 | 해당 없음 | NET_CAN_CONSUMER_REJECTED + 기존 C2 반환값 보존 | queue 접수 실패와 snapshot 적용을 구분 |
| 접수 후 driver 최종 실패·만료·문맥무효 | 이미 돌려준 OK 변경 불가 | 원 식별자 기반 공통 후발 실패 경로 | observer 통보만으로 완료 처리하지 않음 |

DOMAIN 명령의 후발 실패는 공통 경로에서 DRIVER_SUBMISSION_FINAL, EXPIRED_BEFORE_SEND, CONTEXT_INVALIDATED_BEFORE_SEND의 기존 세 이유와 원 CommandContext를 보존한다. 실제 필수 실패 sink 바인딩이 없으면 D-03/D-05/O-C10 HOLD다. 이미 DISPATCHED인 항목에 AbortBeforeDispatch를 호출하지 않는다. BCM 자체 Publish/Send 항목의 실패는 BCM 소유자의 기존 진단/실패 소비 경계가 필요하며 DOMAIN의 실패 API에 위조 요청을 보내지 않는다. optional observer 장애는 핵심 처리 replay 이유가 아니다.


<a id="rx"></a>
## 수신 Adapter 원형과 한국어 C 의사코드

`PROPOSED_PRIVATE_FUNCTION`. 위 Codec과 공통 admission 절차의 연결점이며 원 공개 API를 대체하지 않는다. 각 주석 단계는 순차 조건이며 하나라도 증거가 없으면 지정 HOLD로 즉시 종료한다. `/* 한국어 조건 */`과 `= /* 한국어 산출값 */`은 공통 admission에서 이미 검증한 자료를 가리키는 의사코드 표기다. 실행 가능한 C 또는 미선언 helper 호출이 아니며 미확정 값을 초기값0으로 소비한다는 뜻도 아니다.

```c
NetCanResult NetCan_RxBcmEvent(const NetCanFrame *frame, NetCanProfile profile, DomainIf_Status_t *consumerStatus);

NetCanResult NetCan_RxBcmEvent(const NetCanFrame *frame, NetCanProfile profile, DomainIf_Status_t *consumerStatus)
{
    if (consumerStatus == NULL) return NET_CAN_INVALID_ARGUMENT;
    // consumerStatus는 실제 DomainIf_Rx 호출이 있을 때만 출력한다. 그 전에는 호출자 값을 유지한다.
    NetBcmEvent message; NetCanRxMeta meta;
    NetCanResult result = NetCan_DecodeBcmEvent(frame, profile, &message, &meta);
    if (result != NET_CAN_OK) return result;
    // 공통 Rx admission 단계: 기동 승인·현재/이전세대·순서·중복·AGE·자원·실행문맥을 각각 판정.
    // 미확정은 CONTRACT_HOLD/TIME_HOLD/ORDER_HOLD, 중복은 DUPLICATE, 공간 부족은 NO_CAPACITY.
    // 격리 항목은 정상 last_rx/heartbeat 또는 기능 cache를 갱신하지 않는다.
    // SAME_INPUT이면 높은 TX의 raw 기준선과 같은 now의 보수적 AGE를 함께 commit한다.
    // 이 경우 C2/App 관측을 재발행하지 않고 NET_CAN_OK로 종료한다. 부분 갱신 실패는 둘 다 이전값 유지.
    // command/result/event의 의미 전이는 Owner 규칙으로 분류하며 같은 command의 ACCEPTED→DONE을 불변 관측 충돌로 오인하지 않는다.
    // 승인 자료가 모두 갖춰진 경우에만 다음 typed 투영/해당 소비 경계로 진행한다.
    DomainIf_BcmProtectionEvent_t value = {0};
    const DataQuality_t qualityMap[4] = {DATA_QUALITY_OK, DATA_QUALITY_STALE, DATA_QUALITY_INVALID, DATA_QUALITY_NO_DATA};
    const DomainIf_BcmProtectionEventType_t eventMap[3] = {BCM_PROTECTION_OVERHEAT_DETECTED, BCM_PROTECTION_FAN_MISMATCH_DETECTED, BCM_PROTECTION_RECOVERY_CONFIRMED};
    value.type=eventMap[message.event_kind-1];
    value.occurrenceId=message.occurrence_id;
    if (/* 원 BCM boot와 sourceGeneration의 승인 연결이 없음 */) return NET_CAN_CONTRACT_HOLD;
    value.sourceGeneration = /* 승인된 기동 연결표의 원 sourceGeneration 값 */;
    value.quality=qualityMap[message.event_quality];
    value.ageMs=/* 공통 AGE 단계에서 누적한 원 event_age_ms의 uint32 값 */;
    if ((message.related_domain_boot==0)!=(message.related_command_id==0)) return NET_CAN_CONTRACT_HOLD;
    value.hasRelatedCommand=(message.related_domain_boot!=0);
    if (value.hasRelatedCommand) {
        if (/* related_domain_boot/command_id의 원 명령 연결을 확인하지 못함 */) return NET_CAN_CONTRACT_HOLD;
        value.relatedCommandId=message.related_command_id;
    } // false일 때 relatedCommandId는 소비하지 않는다.
    value.affectedFunctionMask=message.affected_function;
    // 늦은 과거 사건은 최신 정상 current 상태를 과거 fault로 후퇴시키지 않는다.

    if (/* 원 identity·호출 대상·C2 반환/반영 단계를 기록할 관측 공간 미확보 */) return NET_CAN_NOT_READY;
    DomainIf_Status_t returned = DomainIf_RxBcmProtectionEvent(&value);
    *consumerStatus = returned;
    // 기존 M09 관측에 원 BOOT/TX·기능 identity·호출 대상·원 반환과 확인된 Core/Observer 반영 단계를 기록한다.
    // non-OK만으로 미접수 또는 rollback을 단정하지 않는다. 반영 단계가 불명확하면 같은 입력 재호출 HOLD.
    // Core가 반영된 경우 선택 Observer의 실패 때문에 Core 입력을 다시 적용하지 않는다.
    if (returned != DOMAIN_IF_OK) return NET_CAN_CONSUMER_REJECTED;
    // DomainIf 접수 OK는 수신자가 자기 소유 복사를 접수했다는 경계이며 snapshot 적용 증거가 아니다.
    // 승인된 정상 Rx 증거와 핵심 delivery 상태를 공통 commit 정책에 따라 한 번 기록한다.
    // 단순 Decode/observer 성공으로 last 정상 관측 또는 source AGE를 새로 시작하지 않는다.
    return NET_CAN_OK;
}
```

## 정상·거부·후발 실패 검수 사례

| 경로 | 입력/상태 | 기대 결과 |
|---|---|---|
| 정상 | 원 후보 profile 채택 근거·전체 필드·기동·순서·시간·소비 바인딩 확보 | 전 필드 roundtrip, 성공 시에만 출력 commit; 올바른 로컬 소비자 한 번 호출 |
| 프레임 거부 | 잘못된 ID/flags/DLC/length/padding 또는 금지 enum | MALFORMED, 출력/정상 last_rx/기능 상태 불변 |
| 의미 보존 | 원문에 허용된 UNKNOWN/INVALID/NO_DATA 및 AGE65535 | 값 보존; 허용된 보고를 malformed와 혼동하지 않음; 새 실행 근거로 fresh 합성 금지 |
| 재전송·순서 | 동일/오래된 sequence, 이전 기동, 미확정 wrap | 공통 중복/ORDER_HOLD, 기능 중복 호출 없음 |
| 시간 | 원 AGE+queue 경과+측정 가능 전송 경과; driver 공백 미상 | AGE를 작게 만들지 않음; 미상시간으로 허용기간 통과 금지 |
| 소비 거부 | DomainIf queue full 또는 노드 문맥 미준비 | 원 C2 반환값·확인된 반영 단계 보존; 부분 반영 미확인이면 재호출 HOLD |
| 후발 실패 | 로컬 OK 이후 만료/driver최종 실패/문맥무효 | 원 identity로 필수 실패 경로 한 번; buffer release 증거 전 재사용 없음 |
| 늦은 callback | 해제 후 다른 항목의 슬롯을 가리키는 옛 세대 | 현재 항목을 손상시키지 않고 옛 callback 진단 |

위 사례는 정적 설계 검증 조건이다. 실제 보드·Driver·상대 ECU의 동작과 양단 바인딩은 별도 증거가 필요하며, 문서의 정상 사례만으로 제품 실행·검증 완료를 판단하지 않는다.
