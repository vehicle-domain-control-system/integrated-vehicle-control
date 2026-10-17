# BCM_DOOR_COMMAND — 송신·Codec·수신 상세설계

`WIRE-P01-BCM_DOOR_COMMAND` · 수신 `RX-P01-BCM_DOOR_COMMAND-BCM`.
원 후보: ID `0x100`, used_len `22`B, CAN frame `24`B, DLC 코드 `12`. padding B22..B23는 모두0. ID/offset/시간/profile은 `WIRE_PROPOSED/CAN_PROFILE_NOT_TEAM_VERIFIED`이며 외부 계약 확정이 아니다. 근거 `B07 can/BCM_DOOR_COMMAND.md#wire`, `B07 CAN_CODEC_SPEC.md`, `S4C TIME_IDENTITY_CONTRACT.md`, `S4C RESOURCE_QUEUE_DRIVER_FLOW.md`.

[자료 정본](../../data/CAN_BCM_DATA.md#type-NetBcmDoorCommand) · [공통 Codec](../CAN_COMMON_CODEC.md#check-frame) · [전송·수명](../CAN_TRANSFER_CONTROL.md) · [전체 공통 흐름](../../flows/CAN_NETWORK_PATHS.md) · [BCM·Permission 흐름](../../flows/CAN_BCM_AND_PERMISSION.md)

## 입력·출력과 실행 문맥

Network의 Codec·queue·driver 인계·수신 검증·typed 투영이 이 문서의 본체다. 기존 공개 API 원형을 변경하지 않는다. DOMAIN 공개 DomainIf_Tx 본체는 DOMAIN 소유이고 Network는 TxPort callback 본체와 등록 요청을 제공한다. 상대 ECU의 App/DomainIf_Rx 함수는 해당 노드 내부에서만 호출한다. DOMAIN C2 호출은 Task 문맥이며 ISR에서 호출하지 않는다. BCM/CIS/WINDOW/VSS의 Task 또는 superloop 실제 바인딩·동기화는 해당 노드 합의 전 계약 HOLD다.


<a id="api-DomainIf_TxBcmDoorCommand"></a>
### `DomainIf_TxBcmDoorCommand`

기존 선언 · 본체 담당 `DOMAIN (공개 본체 소유 합의 완료)` · `S4A api/domain_tx.md#api-DomainIf_TxBcmDoorCommand`.

```c
DomainIf_Status_t DomainIf_TxBcmDoorCommand( const DomainIf_BcmDoorCommand_t *command);
```


<a id="api-BcmApp_OnDomainDoorCommand"></a>
### `BcmApp_OnDomainDoorCommand`

기존 선언 · 본체 담당 `BCM Application Owner` · `S4A api/bcm.md#api-BcmApp_OnDomainDoorCommand`.

```c
void BcmApp_OnDomainDoorCommand(
    const NetBcmDoorCommand *message,
    const NetCanRxMeta *meta
);
```


<a id="api-txBcmDoorCommand"></a>
### `txBcmDoorCommand`

기존 선언 · 본체 담당 `Network (DOMAIN 로컬 TxPort 콜백 본체)` · `S4A api/callbacks.md#api-txBcmDoorCommand`.

```c
DomainIf_Status_t (*txBcmDoorCommand)(
        const DomainIf_BcmDoorCommand_t *command);
```


## Wire 필드와 완전 자료형

아래 표는 원 후보의 모든 사용 byte를 지정한다. 헤더 BOOT_ID/TX_SEQUENCE는 DTO에 추가하지 않고 NetCanRxMeta.source에 보존한다. 구조체 memcpy/직접 cast는 사용하지 않는다.

| Offset / 폭(B) | Wire 필드 → typed 멤버(타입) | Raw / 부호 / 순서 | 코드·범위·단위 | sentinel / unknown | T33 행 |
|---|---|---|---|---|---|
| 0 / 4 | `BOOT_ID` → `meta.source.boot_id` (`uint32_t`) | u32 / unsigned / LE | 0 예약; 단위=NOT_SPECIFIED | 0 예약 | 100 |
| 4 / 4 | `TX_SEQUENCE` → `meta.source.tx_sequence` (`uint32_t`) | u32 / unsigned / LE | 최초1, 프레임마다 증가; wrap 정책 OPEN; 단위=NOT_SPECIFIED | NOT_SPECIFIED | 100 |
| 8 / 4 | `TARGET_BOOT_ID` → `command.target_boot_id` (`uint32_t`) | u32 / unsigned / LE | 마지막 정상 대상 기동; 단위=NOT_SPECIFIED | NOT_SPECIFIED | 101 |
| 12 / 4 | `COMMAND_ID` → `command.command_id` (`uint32_t`) | u32 / unsigned / LE | C2 commandId 보존; 단위=NOT_SPECIFIED | NOT_SPECIFIED | 101 |
| 16 / 2 | `COMMAND_AGE` → `command.command_age_ms` (`uint16_t`) | u16 / unsigned / LE | 명령 최초 생성 이후 누적 ms; 단위=ms | 65535 포화 하한; 정확한 경과 상한 또는 fresh의 증거 아님 | 101 |
| 18 / 2 | `ACCEPT_LIMIT` → `command.accept_limit_ms` (`uint16_t`) | u16 / unsigned / LE | 200ms 후보/1s 충돌 OPEN; 단위=ms | NOT_SPECIFIED | 101 |
| 20 / 1 | `DOOR_LOCK_TARGET` → `door_lock_target` (`NetDoorTarget`) | u8 / unsigned / N/A | 0=NET_DOOR_TARGET_LOCK; 1=NET_DOOR_TARGET_UNLOCK; 단위=NOT_SPECIFIED | NOT_SPECIFIED | 102 |
| 21 / 1 | `ORIGIN` → `origin` (`NetOrigin`) | u8 / unsigned / N/A | 0=NET_ORIGIN_NONE; 1=NET_ORIGIN_MOBILE_MANUAL; 2=NET_ORIGIN_PROXIMITY_AUTO; 3=NET_ORIGIN_LOCAL; 4=NET_ORIGIN_TEST; 5=NET_ORIGIN_VEHICLE_POLICY; 단위=NOT_SPECIFIED | NOT_SPECIFIED | 103 |

| C2 선언 멤버 / 타입 | 확인한 raw·Net 대응 또는 표현 제한 |
|---|---|
| `DomainIf_BcmDoorCommand_t.context` / `DomainIf_CommandContext_t` | command_id는 context.commandId 보존; 대상 boot·AGE·ACCEPT_LIMIT는 Network 보완, origin은 축약 후보 |
| `DomainIf_BcmDoorCommand_t.target` / `DoorLockTarget_t` | DOOR_LOCK_TARGET; DoorLockTarget_t 이름에 따른 enum 변환 |

`NetBcmDoorCommand`와 관련 C2 타입의 전체 선언·필드 표·enum은 [BCM 자료 정본](../../data/CAN_BCM_DATA.md)에 한 번만 정의한다. 공통 품질/원본 Context는 [공통 자료](../../data/CAN_COMMON_DATA.md), Meta/Status는 [B11 자료](../../data/CIS_REAR_DATA.md)에 있다.

## 메시지별 변환과 판단 경계

context.commandId를 COMMAND_ID에 그대로 옮긴다. TARGET_BOOT_ID는 마지막 정상 BCM 상태와 승인된 기동 생명주기 근거로만 보완한다. context.commandAgeMs는 명령 최초 생성부터의 시간이고 context.meta.ageMs는 판단 자료의 시간으로 서로 대체하지 않는다. decisionSequence와 RequestContext는 원본 추적을 위해 Network 소유 항목에 유지하며 Wire로 덧붙이지 않는다. target은 이름 대응으로 LOCK→0, UNLOCK→1이다. MOBILE_REQUEST→MOBILE_MANUAL, DIGITAL_KEY→PROXIMITY_AUTO, 정책 출처→VEHICLE_POLICY는 축약 후보이며 SYSTEM/LOCAL/TEST와 실제 등록 문맥은 협의 필요다. 축약 origin으로 원 요청자를 재구성하지 않는다. BCM 수신자는 원 DOMAIN boot+command_id+kind에 따라 재전송 중복과 충돌을 구분한다. target 동일함을 실행 성공으로 보고하지 않는다.

<a id="codec"></a>
## 순수 Codec 원형과 한국어 C 의사코드

아래 함수만 `PROPOSED_PRIVATE_FUNCTION`이며 제품에 존재한다는 주장이 아니다. 입력은 호출 동안 빌리고 출력은 성공 때만 한 번 commit한다. 호출자는 입력과 모든 출력 객체의 저장 영역을 서로 겹치지 않게 제공한다. Decode 성공은 바이트 해석 성공이며 새 기동/순서/AGE/기능 소비 승인과 별개다. 공통 CheckFrame은 선언된 [정본](../CAN_COMMON_CODEC.md#check-frame)을 호출한다. 모든 u16/u32는 unsigned LE이며 정수 승격 후 이동한다. 지원되지 않는 raw와 padding은 전체 프레임을 거부한다. 정확한 65535 AGE는 포화 하한으로 보존하며 fresh로 승인하지 않는다.

```c
/* PROPOSED_PRIVATE_FUNCTION */
NetCanResult NetCan_EncodeBcmDoorCommand(const NetBcmDoorCommand *message, const NetCanSource *source, NetCanProfile profile, NetCanFrame *frame);
NetCanResult NetCan_DecodeBcmDoorCommand(const NetCanFrame *frame, NetCanProfile profile, NetBcmDoorCommand *message, NetCanRxMeta *meta);
```


```c
NetCanResult NetCan_EncodeBcmDoorCommand(const NetBcmDoorCommand *message, const NetCanSource *source, NetCanProfile profile, NetCanFrame *frame)
{
    if (message == NULL || source == NULL || frame == NULL) return NET_CAN_INVALID_ARGUMENT;
    if (source->boot_id == 0) return NET_CAN_MALFORMED;
    if (profile == NET_CAN_PROFILE_UNSELECTED) return NET_CAN_CONTRACT_HOLD;
    if (message->door_lock_target > 1) return NET_CAN_MALFORMED;
    if (message->origin > 5) return NET_CAN_MALFORMED;
    NetCanFrame local = {0};
    local.canId = 0x100; local.dlc = 12; local.length = 24;
    local.extended = false; local.fdf = true; local.brs = true; local.rtr = false;
    // 원 CAN FD profile의 BRS=1 후보. 양단 선택/채택 근거가 없으면 공통 CheckFrame이 CONTRACT_HOLD.
    // local.receivedAtMs는 Tx 관측시각이 아니므로 Rx 시각을 만들지 않는다.
    for (uint8_t k = 0; k < 4; ++k) local.data[0+k] = (uint8_t)(((uint32_t)source->boot_id >> (8u*k)) & 0xFFu);
    for (uint8_t k = 0; k < 4; ++k) local.data[4+k] = (uint8_t)(((uint32_t)source->tx_sequence >> (8u*k)) & 0xFFu);
    for (uint8_t k = 0; k < 4; ++k) local.data[8+k] = (uint8_t)(((uint32_t)message->command.target_boot_id >> (8u*k)) & 0xFFu);
    for (uint8_t k = 0; k < 4; ++k) local.data[12+k] = (uint8_t)(((uint32_t)message->command.command_id >> (8u*k)) & 0xFFu);
    for (uint8_t k = 0; k < 2; ++k) local.data[16+k] = (uint8_t)(((uint32_t)message->command.command_age_ms >> (8u*k)) & 0xFFu);
    for (uint8_t k = 0; k < 2; ++k) local.data[18+k] = (uint8_t)(((uint32_t)message->command.accept_limit_ms >> (8u*k)) & 0xFFu);
    for (uint8_t k = 0; k < 1; ++k) local.data[20+k] = (uint8_t)(((uint32_t)message->door_lock_target >> (8u*k)) & 0xFFu);
    for (uint8_t k = 0; k < 1; ++k) local.data[21+k] = (uint8_t)(((uint32_t)message->origin >> (8u*k)) & 0xFFu);
    // 0으로 초기화한 padding을 유지. 수신시간/driver 상태는 송신 성공 근거 아님.
    NetCanResult checked = NetCan_CheckFrame(&local, NET_CAN_BCM_DOOR_COMMAND, NET_CAN_NODE_BCM, profile);
    if (checked != NET_CAN_OK) return checked;
    *frame = local;
    return NET_CAN_OK;
}

NetCanResult NetCan_DecodeBcmDoorCommand(const NetCanFrame *frame, NetCanProfile profile, NetBcmDoorCommand *message, NetCanRxMeta *meta)
{
    if (frame == NULL || message == NULL || meta == NULL) return NET_CAN_INVALID_ARGUMENT;
    NetCanResult checked = NetCan_CheckFrame(frame, NET_CAN_BCM_DOOR_COMMAND, NET_CAN_NODE_BCM, profile);
    if (checked != NET_CAN_OK) return checked;
    NetBcmDoorCommand local = {0}; NetCanRxMeta localMeta = {0};
    { uint32_t value = 0;
      for (uint8_t k = 0; k < 4; ++k) value |= ((uint32_t)frame->data[0+k] << (8u*k));
      localMeta.source.boot_id = value; }
    { uint32_t value = 0;
      for (uint8_t k = 0; k < 4; ++k) value |= ((uint32_t)frame->data[4+k] << (8u*k));
      localMeta.source.tx_sequence = value; }
    { uint32_t value = 0;
      for (uint8_t k = 0; k < 4; ++k) value |= ((uint32_t)frame->data[8+k] << (8u*k));
      local.command.target_boot_id = value; }
    { uint32_t value = 0;
      for (uint8_t k = 0; k < 4; ++k) value |= ((uint32_t)frame->data[12+k] << (8u*k));
      local.command.command_id = value; }
    { uint32_t value = 0;
      for (uint8_t k = 0; k < 2; ++k) value |= ((uint32_t)frame->data[16+k] << (8u*k));
      local.command.command_age_ms = value; }
    { uint32_t value = 0;
      for (uint8_t k = 0; k < 2; ++k) value |= ((uint32_t)frame->data[18+k] << (8u*k));
      local.command.accept_limit_ms = value; }
    { uint32_t value = 0;
      for (uint8_t k = 0; k < 1; ++k) value |= ((uint32_t)frame->data[20+k] << (8u*k));
      local.door_lock_target = value; }
    { uint32_t value = 0;
      for (uint8_t k = 0; k < 1; ++k) value |= ((uint32_t)frame->data[21+k] << (8u*k));
      local.origin = value; }
    if (localMeta.source.boot_id == 0) return NET_CAN_MALFORMED;
    if (local.door_lock_target > 1) return NET_CAN_MALFORMED;
    if (local.origin > 5) return NET_CAN_MALFORMED;
    for (uint8_t k = 22; k < 24; ++k) if (frame->data[k] != 0) return NET_CAN_MALFORMED;
    localMeta.received_at_ms = frame->receivedAtMs; // 하드웨어/driver 경계에서 확보한 로컬 시각
    *message = local; *meta = localMeta;
    return NET_CAN_OK;
}
```

## DOMAIN TxPort callback의 완전 필드 투영

`PROPOSED_PRIVATE_FUNCTION` 원형은 기존 callback 멤버와 인자·반환형을 일치시킨다. Network가 아래 본체를 제공하고 DOMAIN이 소유한 공개 Tx 함수가 등록된 callback으로 전달한다. 등록 수명·반환 대응이 확인되기 전 실제 연결은 HOLD다.

```c
DomainIf_Status_t NetCan_TxBcmDoorCommand(const DomainIf_BcmDoorCommand_t *command);

DomainIf_Status_t NetCan_TxBcmDoorCommand(const DomainIf_BcmDoorCommand_t *command)
{
    if (command == NULL) return DOMAIN_IF_INVALID_ARGUMENT;
    NetBcmDoorCommand message = {0};
    NetCommandMeta *meta = &message.command;
    meta->command_id = command->context.commandId;
    if (/* 마지막 정상 BCM 기동 연결·원 birth/elapsed·ACCEPT_LIMIT 채택 근거 없음 */) return DOMAIN_IF_NOT_AVAILABLE;
    meta->target_boot_id = /* 승인된 현재 BCM target boot */;
    meta->command_age_ms = /* command->context.commandAgeMs를 공통 AGE 단계에서 누적 후 u16 포화한 값 */;
    meta->accept_limit_ms = /* 200ms/1s 충돌과 동등 경계가 해소되어 실제 채택된 값 */;
    // context.meta.ageMs는 별도 판단자료 AGE다. 원 RequestContext/decisionSequence/device/기동은 owned item에 따로 보존한다.
    switch (command->target) {
    case DOOR_TARGET_LOCK: message.door_lock_target=NET_DOOR_TARGET_LOCK; break;
    case DOOR_TARGET_UNLOCK: message.door_lock_target=NET_DOOR_TARGET_UNLOCK; break;
    default: return DOMAIN_IF_INVALID_ARGUMENT;
    }
    if (/* ORIGIN 축약 표의 양단 바인딩 미확인 */) return DOMAIN_IF_NOT_AVAILABLE;
    switch (command->context.origin) {
    case COMMAND_ORIGIN_MOBILE_REQUEST: message.origin=NET_ORIGIN_MOBILE_MANUAL; break;
    case COMMAND_ORIGIN_DIGITAL_KEY: message.origin=NET_ORIGIN_PROXIMITY_AUTO; break;
    case COMMAND_ORIGIN_CLIMATE_POLICY:
    case COMMAND_ORIGIN_INTERIOR_LIGHT_POLICY:
    case COMMAND_ORIGIN_AUTO_VENTILATION: message.origin=NET_ORIGIN_VEHICLE_POLICY; break;
    case COMMAND_ORIGIN_SYSTEM: return DOMAIN_IF_NOT_AVAILABLE; // SYSTEM/LOCAL/TEST 투영 미확정.
    default: return DOMAIN_IF_INVALID_ARGUMENT;
    }
    // 여기까지는 임시 Net DTO 생성이다. 아래 송신 접수 절차1..8을 이어 수행한다.
    // 전체 원 Context+DTO의 owned copy와 슬롯/필수실패sink/profile/현재기동을 함께 확보한다.
    if (/* 바인딩 또는 준비조건을 확보하지 못함 */) return DOMAIN_IF_NOT_AVAILABLE;
    if (/* 안정 사본/queue 슬롯을 확보하지 못함 */) return DOMAIN_IF_QUEUE_FULL;
    // 해당 Encode 및 전체 조합 검사 실패면 게시하지 않고 임시 예약만 되돌린다.
    // 성공한 owned item과 원 Context를 원자적으로 게시한다. 포인터만 저장하지 않는다.
    return DOMAIN_IF_OK; // 로컬 복사·게시 완료 경계만 의미.
}
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
NetCanResult NetCan_RxBcmDoorCommand(const NetCanFrame *frame, NetCanProfile profile);

NetCanResult NetCan_RxBcmDoorCommand(const NetCanFrame *frame, NetCanProfile profile)
{
    NetBcmDoorCommand message; NetCanRxMeta meta;
    NetCanResult result = NetCan_DecodeBcmDoorCommand(frame, profile, &message, &meta);
    if (result != NET_CAN_OK) return result;
    // 공통 Rx admission 단계: 기동 승인·현재/이전세대·순서·중복·AGE·자원·실행문맥을 각각 판정.
    // 미확정은 CONTRACT_HOLD/TIME_HOLD/ORDER_HOLD, 중복은 DUPLICATE, 공간 부족은 NO_CAPACITY.
    // 격리 항목은 정상 last_rx/heartbeat 또는 기능 cache를 갱신하지 않는다.
    // SAME_INPUT이면 높은 TX의 raw 기준선과 같은 now의 보수적 AGE를 함께 commit한다.
    // 이 경우 C2/App 관측을 재발행하지 않고 NET_CAN_OK로 종료한다. 부분 갱신 실패는 둘 다 이전값 유지.
    // command/result/event의 의미 전이는 Owner 규칙으로 분류하며 같은 command의 ACCEPTED→DONE을 불변 관측 충돌로 오인하지 않는다.
    // 승인 자료가 모두 갖춰진 경우에만 다음 typed 투영/해당 소비 경계로 진행한다.
    // 마지막 정상 BCM boot와 TARGET_BOOT_ID 연결 및 ACCEPT_LIMIT 비교계약 확인.
    // 200ms/1s 충돌·동일경계 > / >=·수송 공백이 미확정이면 TIME_HOLD/CONTRACT_HOLD.
    // 원 DOMAIN boot+command_id+kind 재시도는 기존 처리와 연결하고 중복 실행하지 않는다.
    // 이 메시지의 필드 조합·기능 admission을 통과한 owned pair에 한하여 호출.
    BcmApp_OnDomainDoorCommand(&message, &meta);
    // void 반환은 실제 실행/완료 ACK가 아니다. BCM 기능이 Result/State/Event를 별도로 생성한다.
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
