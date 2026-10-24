# DOMAIN_PERMISSION — 송신·Codec·수신 상세설계

`WIRE-P01-DOMAIN_PERMISSION` · 수신 `RX-P01-DOMAIN_PERMISSION-BCM`, `RX-P01-DOMAIN_PERMISSION-CIS`, `RX-P01-DOMAIN_PERMISSION-WINDOW`, `RX-P01-DOMAIN_PERMISSION-VSS`.
원 후보: ID `0x080`, used_len `19`B, CAN frame `20`B, DLC 코드 `11`. padding B19..B19는 모두0. ID/offset/시간/profile은 `WIRE_PROPOSED/CAN_PROFILE_NOT_TEAM_VERIFIED`이며 외부 계약 확정이 아니다. 근거 `B07 can/DOMAIN_PERMISSION.md#wire`, `B07 CAN_CODEC_SPEC.md`, `S4C TIME_IDENTITY_CONTRACT.md`, `S4C RESOURCE_QUEUE_DRIVER_FLOW.md`.

[자료 정본](../../data/CAN_BCM_DATA.md#type-NetDomainPermission) · [공통 Codec](../CAN_COMMON_CODEC.md#check-frame) · [전송·수명](../CAN_TRANSFER_CONTROL.md) · [전체 공통 흐름](../../flows/CAN_NETWORK_PATHS.md) · [BCM·Permission 흐름](../../flows/CAN_BCM_AND_PERMISSION.md)

## 입력·출력과 실행 문맥

Network의 Codec·queue·driver 인계·수신 검증·typed 투영이 이 문서의 본체다. 기존 공개 API 원형을 변경하지 않는다. DOMAIN 공개 DomainIf_Tx 본체는 DOMAIN 소유이고 Network는 TxPort callback 본체와 등록 요청을 제공한다. 상대 ECU의 App/DomainIf_Rx 함수는 해당 노드 내부에서만 호출한다. DOMAIN C2 호출은 초기 Superloop의 ISR 밖 단일 담당 문맥에서 직렬 처리하며 ISR에서 직접 호출하지 않는다. BCM/CIS/WINDOW/VSS의 Task 또는 superloop 실제 바인딩·동기화는 해당 노드 합의 전 계약 HOLD다.


<a id="api-DomainNet_PublishPermission"></a>
### `DomainNet_PublishPermission`

기존 선언 · 본체 담당 `DOMAIN Network` · `S4A api/domain_private.md#api-DomainNet_PublishPermission`.

```c
NetSubmitResult DomainNet_PublishPermission(const NetDomainPermission *message);
```


<a id="api-BcmNetPriv_OnDomainPermission"></a>
### `BcmNetPriv_OnDomainPermission`

기존 선언 · 본체 담당 `BCM Network` · `S4A api/bcm.md#api-BcmNetPriv_OnDomainPermission`.

```c
void BcmNetPriv_OnDomainPermission(
    const NetDomainPermission *permission,
    const NetCanRxMeta *meta
);
```


<a id="api-CisApp_OnDomainPermission"></a>
### `CisApp_OnDomainPermission`

기존 선언 · 본체 담당 `CIS Application Owner` · `S4A api/cis.md#api-CisApp_OnDomainPermission`.

```c
void CisApp_OnDomainPermission(
    const NetDomainPermission *message,
    const NetCanRxMeta *meta
);
```


<a id="api-WindowApp_OnDomainPermission"></a>
### `WindowApp_OnDomainPermission`

기존 선언 · 본체 담당 `WINDOW Application Owner` · `S4A api/window.md#api-WindowApp_OnDomainPermission`.

```c
void WindowApp_OnDomainPermission(const NetDomainPermission *message, const NetCanRxMeta *meta);
```


<a id="api-VssNetPriv_OnDomainPermission"></a>
### `VssNetPriv_OnDomainPermission`

기존 선언 · 본체 담당 `VSS Network` · `S4A api/vss.md#api-VssNetPriv_OnDomainPermission`.

```c
void VssNetPriv_OnDomainPermission(const NetDomainPermission *message, const NetCanRxMeta *meta);
```


<a id="api-DomainIf_TxCisPowerPermission"></a>
### `DomainIf_TxCisPowerPermission`

기존 선언 · 본체 담당 `DOMAIN (공개 본체 소유 합의 완료)` · `S4A api/domain_tx.md#api-DomainIf_TxCisPowerPermission`.

```c
DomainIf_Status_t DomainIf_TxCisPowerPermission( const DomainIf_CisPowerPermission_t *permission);
```


<a id="api-txCisPowerPermission"></a>
### `txCisPowerPermission`

기존 선언 · 본체 담당 `Network (DOMAIN 로컬 TxPort 콜백 본체)` · `S4A api/callbacks.md#api-txCisPowerPermission`.

```c
DomainIf_Status_t (*txCisPowerPermission)(
        const DomainIf_CisPowerPermission_t *permission);
```


<a id="api-DomainIf_TxWindowOperationPermission"></a>
### `DomainIf_TxWindowOperationPermission`

기존 선언 · 본체 담당 `DOMAIN (공개 본체 소유 합의 완료)` · `S4A api/domain_tx.md#api-DomainIf_TxWindowOperationPermission`.

```c
DomainIf_Status_t DomainIf_TxWindowOperationPermission( const DomainIf_WindowOperationPermission_t *permission);
```


<a id="api-txWindowOperationPermission"></a>
### `txWindowOperationPermission`

기존 선언 · 본체 담당 `Network (DOMAIN 로컬 TxPort 콜백 본체)` · `S4A api/callbacks.md#api-txWindowOperationPermission`.

```c
DomainIf_Status_t (*txWindowOperationPermission)(
        const DomainIf_WindowOperationPermission_t *permission);
```


<a id="api-DomainIf_TxWindowDomainAlive"></a>
### `DomainIf_TxWindowDomainAlive`

기존 선언 · 본체 담당 `DOMAIN (공개 본체 소유 합의 완료)` · `S4A api/domain_tx.md#api-DomainIf_TxWindowDomainAlive`.

```c
DomainIf_Status_t DomainIf_TxWindowDomainAlive( const DomainIf_WindowDomainAlive_t *alive);
```


<a id="api-txWindowDomainAlive"></a>
### `txWindowDomainAlive`

기존 선언 · 본체 담당 `Network (DOMAIN 로컬 TxPort 콜백 본체)` · `S4A api/callbacks.md#api-txWindowDomainAlive`.

```c
DomainIf_Status_t (*txWindowDomainAlive)(
        const DomainIf_WindowDomainAlive_t *alive);
```


## Wire 필드와 완전 자료형

아래 표는 원 후보의 모든 사용 byte를 지정한다. 헤더 BOOT_ID/TX_SEQUENCE는 DTO에 추가하지 않고 NetCanRxMeta.source에 보존한다. 구조체 memcpy/직접 cast는 사용하지 않는다.

| Offset / 폭(B) | Wire 필드 → typed 멤버(타입) | Raw / 부호 / 순서 | 코드·범위·단위 | sentinel / unknown | T33 행 |
|---|---|---|---|---|---|
| 0 / 4 | `BOOT_ID` → `meta.source.boot_id` (`uint32_t`) | u32 / unsigned / LE | 0 예약; 단위=NOT_SPECIFIED | 0 예약 | 79 |
| 4 / 4 | `TX_SEQUENCE` → `meta.source.tx_sequence` (`uint32_t`) | u32 / unsigned / LE | 최초1, 프레임마다 증가; wrap 정책 OPEN; 단위=NOT_SPECIFIED | NOT_SPECIFIED | 79 |
| 8 / 1 | `VEHICLE_POWER_PERMISSION` → `vehicle_power_permission` (`NetTriState`) | u8 / unsigned / N/A | 0=NET_TRISTATE_NO; 1=NET_TRISTATE_YES; 255=NET_TRISTATE_UNKNOWN; 단위=NOT_SPECIFIED | 255=확인 불가 | 80 |
| 9 / 1 | `POWER_PERMISSION_QUALITY` → `power_permission_quality` (`NetValueQuality`) | u8 / unsigned / N/A | 0=NET_QUALITY_OK; 1=NET_QUALITY_STALE; 2=NET_QUALITY_INVALID; 3=NET_QUALITY_NO_DATA; 단위=NOT_SPECIFIED | NOT_SPECIFIED | 81 |
| 10 / 1 | `POWER_OPERATION_STATE` → `power_operation_state` (`NetTriState`) | u8 / unsigned / N/A | 0=NET_TRISTATE_NO; 1=NET_TRISTATE_YES; 255=NET_TRISTATE_UNKNOWN; 단위=NOT_SPECIFIED | 255=확인 불가 | 82 |
| 11 / 1 | `OPERATION_ALLOWED` → `operation_allowed` (`NetFlag`) | u8 / unsigned / N/A | 0=NET_FLAG_NO; 1=NET_FLAG_YES; 단위=NOT_SPECIFIED | NOT_SPECIFIED | 83 |
| 12 / 1 | `OPERATION_QUALITY` → `operation_quality` (`NetValueQuality`) | u8 / unsigned / N/A | 0=NET_QUALITY_OK; 1=NET_QUALITY_STALE; 2=NET_QUALITY_INVALID; 3=NET_QUALITY_NO_DATA; 단위=NOT_SPECIFIED | NOT_SPECIFIED | 84 |
| 13 / 4 | `UPDATE_SEQUENCE` → `update_sequence` (`uint32_t`) | u32 / unsigned / LE | Network 합성 UPDATE 번호; DOMAIN 원 성공 평가와 별개, 갱신 세부정책 미결; 단위=NOT_SPECIFIED | NOT_SPECIFIED | 85 |
| 17 / 2 | `SOURCE_AGE` → `source_age_ms` (`uint16_t`) | u16 / unsigned / LE | 포함된 원 판단의 보수 누적 경과, 합성 갱신/실패에서0 재시작 금지; 단위=ms | 65535 포화 하한; 정확한 경과 상한 또는 fresh의 증거 아님 | 86 |

이 메시지의 수신 입력은 같은 Net DTO+NetCanRxMeta이며 수신 노드별 검증은 아래에서 분리한다.

`NetDomainPermission`와 관련 C2 타입의 전체 선언·필드 표·enum은 [BCM 자료 정본](../../data/CAN_BCM_DATA.md)에 한 번만 정의한다. 공통 품질/원본 Context는 [공통 자료](../../data/CAN_COMMON_DATA.md), Meta/Status는 [B11 자료](../../data/CIS_REAR_DATA.md)에 있다.

## 메시지별 변환과 판단 경계

CIS power 허용, WINDOW operation 허용, DOMAIN alive는 독립 C2 입력이다. 세 callback의 새 입력은 자기 원본 revision/quality/age만 갱신한다. 새로운 power 입력으로 오래된 operation/alive를 fresh하게 만들지 않는다. POWER_OPERATION_STATE 생산자와 세 입력의 합성 revision/공유 AGE 규칙은 미바인딩이므로 Net DTO가 완결되지 않으면 송신 NOT_READY, 소비 CONTRACT_HOLD이다. 같은 now로 옮긴 각 입력의 AGE 중 최대값은 보수적 합성 후보이며 확정 규칙이 아니다. 누락 입력은 허용/OK/AGE0으로 채우지 않는다. quality_reason 필드는 이 Wire에 없으며 원 제한·사유를 간단 복사할 수 없다.

r1의 [조건부 의미 투영](../../flows/CAN_BCM_AND_PERMISSION.md#permission-projection)은 실제 정상 ALLOW1/OK·DENY0/OK와 확인된 원인/DOMAIN 채택에 따른255+NO_DATA/STALE/INVALID를 구별한다. UNKNOWN+OK 무손실 필요 또는 quality/meta 충돌은 해당 합성 HOLD이며 정상 열화 보고를 형식 오류로 숨기지 않는다. [번호 공간](../../flows/CAN_BCM_AND_PERMISSION.md#permission-sequences)의 원 성공17은 실패 뒤에도17·원 시각이고, CAN update_sequence는 별도의 합성 번호다. 의미·품질 제한이 바뀌면 조건부 합성 갱신을 검토하며 같은 원 성공번호만으로 버리지 않는다. Wire 필드/offset·Codec은 그대로다.

BCM과 VSS는 private monitor의 검증 경계이며 기능 허용을 새로 만들지 않는다. CIS의 App 소비와 Pi relay는 동일 검증 pair의 별도 형제 경계로 순서를 강제하지 않는다. UART-C 구현은 별도 범위이며 Pi 도착이 CIS 최신 판단을 의미하지 않는다. WINDOW는 실행 조건·heartbeat 갱신 기준/timeout·domain alive 투영이 미바인딩이므로 WINDOW normal callback을 실행하지 않고 CONTRACT_HOLD로 끝낸다. CAN 후보50ms/150ms와 Pi500ms/1500ms를 서로 연장하지 않는다.

<a id="codec"></a>
## 순수 Codec 원형과 한국어 C 의사코드

아래 함수만 `PROPOSED_PRIVATE_FUNCTION`이며 제품에 존재한다는 주장이 아니다. 입력은 호출 동안 빌리고 출력은 성공 때만 한 번 commit한다. 호출자는 입력과 모든 출력 객체의 저장 영역을 서로 겹치지 않게 제공한다. Decode 성공은 바이트 해석 성공이며 새 기동/순서/AGE/기능 소비 승인과 별개다. 공통 CheckFrame은 선언된 [정본](../CAN_COMMON_CODEC.md#check-frame)을 호출한다. 모든 u16/u32는 unsigned LE이며 정수 승격 후 이동한다. 지원되지 않는 raw와 padding은 전체 프레임을 거부한다. 정확한 65535 AGE는 포화 하한으로 보존하며 fresh로 승인하지 않는다.

```c
/* PROPOSED_PRIVATE_FUNCTION */
NetCanResult NetCan_EncodeDomainPermission(const NetDomainPermission *message, const NetCanSource *source, NetCanProfile profile, NetCanFrame *frame);
NetCanResult NetCan_DecodeDomainPermission(const NetCanFrame *frame, NetCanNode receiver, NetCanProfile profile, NetDomainPermission *message, NetCanRxMeta *meta);
```


```c
NetCanResult NetCan_EncodeDomainPermission(const NetDomainPermission *message, const NetCanSource *source, NetCanProfile profile, NetCanFrame *frame)
{
    if (message == NULL || source == NULL || frame == NULL) return NET_CAN_INVALID_ARGUMENT;
    if (source->boot_id == 0) return NET_CAN_MALFORMED;
    if (profile == NET_CAN_PROFILE_UNSELECTED) return NET_CAN_CONTRACT_HOLD;
    if ((message->vehicle_power_permission != 0 && message->vehicle_power_permission != 1 && message->vehicle_power_permission != 255)) return NET_CAN_MALFORMED;
    if (message->power_permission_quality > 3) return NET_CAN_MALFORMED;
    if ((message->power_operation_state != 0 && message->power_operation_state != 1 && message->power_operation_state != 255)) return NET_CAN_MALFORMED;
    if (message->operation_allowed > 1) return NET_CAN_MALFORMED;
    if (message->operation_quality > 3) return NET_CAN_MALFORMED;
    NetCanFrame local = {0};
    local.canId = 0x080; local.dlc = 11; local.length = 20;
    local.extended = false; local.fdf = true; local.brs = true; local.rtr = false;
    // 원 CAN FD profile의 BRS=1 후보. 양단 선택/채택 근거가 없으면 공통 CheckFrame이 CONTRACT_HOLD.
    // local.receivedAtMs는 Tx 관측시각이 아니므로 Rx 시각을 만들지 않는다.
    for (uint8_t k = 0; k < 4; ++k) local.data[0+k] = (uint8_t)(((uint32_t)source->boot_id >> (8u*k)) & 0xFFu);
    for (uint8_t k = 0; k < 4; ++k) local.data[4+k] = (uint8_t)(((uint32_t)source->tx_sequence >> (8u*k)) & 0xFFu);
    for (uint8_t k = 0; k < 1; ++k) local.data[8+k] = (uint8_t)(((uint32_t)message->vehicle_power_permission >> (8u*k)) & 0xFFu);
    for (uint8_t k = 0; k < 1; ++k) local.data[9+k] = (uint8_t)(((uint32_t)message->power_permission_quality >> (8u*k)) & 0xFFu);
    for (uint8_t k = 0; k < 1; ++k) local.data[10+k] = (uint8_t)(((uint32_t)message->power_operation_state >> (8u*k)) & 0xFFu);
    for (uint8_t k = 0; k < 1; ++k) local.data[11+k] = (uint8_t)(((uint32_t)message->operation_allowed >> (8u*k)) & 0xFFu);
    for (uint8_t k = 0; k < 1; ++k) local.data[12+k] = (uint8_t)(((uint32_t)message->operation_quality >> (8u*k)) & 0xFFu);
    for (uint8_t k = 0; k < 4; ++k) local.data[13+k] = (uint8_t)(((uint32_t)message->update_sequence >> (8u*k)) & 0xFFu);
    for (uint8_t k = 0; k < 2; ++k) local.data[17+k] = (uint8_t)(((uint32_t)message->source_age_ms >> (8u*k)) & 0xFFu);
    // 0으로 초기화한 padding을 유지. 수신시간/driver 상태는 송신 성공 근거 아님.
    NetCanResult checked = NetCan_CheckFrame(&local, NET_CAN_DOMAIN_PERMISSION, NET_CAN_NODE_BCM, profile);
    if (checked != NET_CAN_OK) return checked;
    *frame = local;
    return NET_CAN_OK;
}

NetCanResult NetCan_DecodeDomainPermission(const NetCanFrame *frame, NetCanNode receiver, NetCanProfile profile, NetDomainPermission *message, NetCanRxMeta *meta)
{
    if (frame == NULL || message == NULL || meta == NULL) return NET_CAN_INVALID_ARGUMENT;
    NetCanResult checked = NetCan_CheckFrame(frame, NET_CAN_DOMAIN_PERMISSION, receiver, profile);
    if (checked != NET_CAN_OK) return checked;
    NetDomainPermission local = {0}; NetCanRxMeta localMeta = {0};
    { uint32_t value = 0;
      for (uint8_t k = 0; k < 4; ++k) value |= ((uint32_t)frame->data[0+k] << (8u*k));
      localMeta.source.boot_id = value; }
    { uint32_t value = 0;
      for (uint8_t k = 0; k < 4; ++k) value |= ((uint32_t)frame->data[4+k] << (8u*k));
      localMeta.source.tx_sequence = value; }
    { uint32_t value = 0;
      for (uint8_t k = 0; k < 1; ++k) value |= ((uint32_t)frame->data[8+k] << (8u*k));
      local.vehicle_power_permission = value; }
    { uint32_t value = 0;
      for (uint8_t k = 0; k < 1; ++k) value |= ((uint32_t)frame->data[9+k] << (8u*k));
      local.power_permission_quality = value; }
    { uint32_t value = 0;
      for (uint8_t k = 0; k < 1; ++k) value |= ((uint32_t)frame->data[10+k] << (8u*k));
      local.power_operation_state = value; }
    { uint32_t value = 0;
      for (uint8_t k = 0; k < 1; ++k) value |= ((uint32_t)frame->data[11+k] << (8u*k));
      local.operation_allowed = value; }
    { uint32_t value = 0;
      for (uint8_t k = 0; k < 1; ++k) value |= ((uint32_t)frame->data[12+k] << (8u*k));
      local.operation_quality = value; }
    { uint32_t value = 0;
      for (uint8_t k = 0; k < 4; ++k) value |= ((uint32_t)frame->data[13+k] << (8u*k));
      local.update_sequence = value; }
    { uint32_t value = 0;
      for (uint8_t k = 0; k < 2; ++k) value |= ((uint32_t)frame->data[17+k] << (8u*k));
      local.source_age_ms = value; }
    if (localMeta.source.boot_id == 0) return NET_CAN_MALFORMED;
    if ((local.vehicle_power_permission != 0 && local.vehicle_power_permission != 1 && local.vehicle_power_permission != 255)) return NET_CAN_MALFORMED;
    if (local.power_permission_quality > 3) return NET_CAN_MALFORMED;
    if ((local.power_operation_state != 0 && local.power_operation_state != 1 && local.power_operation_state != 255)) return NET_CAN_MALFORMED;
    if (local.operation_allowed > 1) return NET_CAN_MALFORMED;
    if (local.operation_quality > 3) return NET_CAN_MALFORMED;
    for (uint8_t k = 19; k < 20; ++k) if (frame->data[k] != 0) return NET_CAN_MALFORMED;
    localMeta.received_at_ms = frame->receivedAtMs; // 하드웨어/driver 경계에서 확보한 로컬 시각
    *message = local; *meta = localMeta;
    return NET_CAN_OK;
}
```

## 기존 Net 송신 본체의 한국어 C 의사코드

아래는 위에 선언한 기존 `DomainNet_PublishPermission`의 Network 소유 본체 설계다. DTO 구조체의 값 복사는 자기 소유 의미 사본이며 Wire 통복사가 아니다. 공통 전송 제어의 Admission·AGE·dispatch·후발 실패 절차를 순서대로 구체화한다.

```c
NetSubmitResult DomainNet_PublishPermission(const NetDomainPermission *message)
{
    if (message == NULL) return NET_SUBMIT_INVALID_ARGUMENT;
    // 아래 Codec에서 열거한 각 필드의 전체 허용 집합/범위와 메시지별 조합을 먼저 검사한다.
    if (/* 값 또는 금지 조합 오류 */) return NET_SUBMIT_INVALID_ARGUMENT;
    // 완결된 Permission의 합성 revision/AGE·필수 원 입력과 채택된 의미 투영을 확인한다.
    // 합성 번호와 DOMAIN 원 성공 번호는 별개이며 확인된 제한을 원 성공 중복만으로 버리지 않는다.
    // UNKNOWN+OK 표현 한계/품질 충돌은 NOT_READY, 정상 열화 raw 자체는 INVALID_ARGUMENT가 아니다.
    if (/* 실제 profile 채택·생산기동·원시간·필수 consumer/실패 경계가 준비되지 않음 */)
        return NET_SUBMIT_NOT_READY;
    // 원 source/evaluation key별 아직 시작하지 않은 상태 item에 필요한 안정 보관소를 확보한다.
    if (/* 해당 item과 원본 시간/identity를 보관할 공간 부족 */) return NET_SUBMIT_NO_CAPACITY;
    NetDomainPermission owned = *message;
    // owned와 원본 AGE 기준·평가/명령/사건 식별자를 같은 item에 값으로 복사한다.
    // 원 포인터를 queue에 남기지 않고, 임시 예약 이후 실패하면 예약만 반환한다.
    // 공통의 유효 순서/교체 정책과 실제 owner 현재 제한을 확인하고 owned item을 원자적으로 게시한다.
    // 제한 뒤 구 ALLOW를 재게시하지 않는다. 새 제한의 접수 실패가 구 ALLOW 복구 근거가 아니다.
    // 이 게시가 완료되지 않으면 OK를 반환하지 않는다.
    return NET_SUBMIT_OK;
}

// 이 item의 F12 dispatch 계속:
// 현재 peer/원문맥·원 AGE·현재 검증된 제한을 재검사한다. 미확정/만료/제한된 구 ALLOW는 송신 전 정지한다.
// 제한 관측은 실제 DOMAIN owner의 동일 원 기록에서만 온다. malformed 입력/일반 제출 실패로 생성하지 않는다.
// 미시작 pending/retry는 구 ALLOW로 재게시하지 않는다. in-flight frame은 terminal/release까지 불변 보존한다.
// 확인된 열화 보고는 별도 투영 계약으로 판단하며 원 AGE를 실패 시각에서0으로 재발급하지 않는다.
// 승인된 생산 BOOT 및 전송 순서를 공통 정책으로 취득한 뒤 별도 임시 frame을 확보한다.
// NetCan_EncodeDomainPermission에 owned DTO와 source, 채택 profile, 임시 frame을 전달한다.
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
NetCanResult NetCan_RxDomainPermission(const NetCanFrame *frame, NetCanNode receiver, NetCanProfile profile);

NetCanResult NetCan_RxDomainPermission(const NetCanFrame *frame, NetCanNode receiver, NetCanProfile profile)
{
    NetDomainPermission message; NetCanRxMeta meta;
    NetCanResult result = NetCan_DecodeDomainPermission(frame, receiver, profile, &message, &meta);
    if (result != NET_CAN_OK) return result;
    // 공통 Rx admission 단계: 기동 승인·현재/이전세대·순서·중복·AGE·자원·실행문맥을 각각 판정.
    // 미확정은 CONTRACT_HOLD/TIME_HOLD/ORDER_HOLD, 중복은 DUPLICATE, 공간 부족은 NO_CAPACITY.
    // 격리 항목은 정상 last_rx/heartbeat 또는 기능 cache를 갱신하지 않는다.
    // SAME_INPUT은 같은 CAN 합성 UPDATE와 같은 전달 의미/품질이다. DOMAIN 원 성공번호만으로 판정하지 않는다.
    // 제한 변경은 채택된 새 합성 UPDATE/전이 근거로 별도 인계한다. 같은 합성 UPDATE의 의미 변경은 ORDER_HOLD다.
    // SAME_INPUT이면 높은 TX의 raw 기준선과 같은 now의 보수적 AGE를 함께 commit한다.
    // 이 경우 C2/App 관측을 재발행하지 않고 NET_CAN_OK로 종료한다. 부분 갱신 실패는 둘 다 이전값 유지.
    // command/result/event의 의미 전이는 Owner 규칙으로 분류하며 같은 command의 ACCEPTED→DONE을 불변 관측 충돌로 오인하지 않는다.
    // 승인 자료가 모두 갖춰진 경우에만 다음 typed 투영/해당 소비 경계로 진행한다.
    switch (receiver) {
    case NET_CAN_NODE_BCM:
        // BCM private monitor 승인과 로컬 문맥 확인 후 해당 pair만 전달.
        BcmNetPriv_OnDomainPermission(&message, &meta); break;
    case NET_CAN_NODE_CIS:
        // CIS power 소비의 원 판단/사용 한도 확인. Pi relay는 독립 가지로 승인 처리.
        CisApp_OnDomainPermission(&message, &meta); break;
    case NET_CAN_NODE_WINDOW:
        return NET_CAN_CONTRACT_HOLD; // WINDOW 실행/timeout 바인딩 전 normal callback 금지.
    case NET_CAN_NODE_VSS:
        // VSS private monitor만. VssRecoveryPermission이나 새 Service 지시로 변환 금지.
        VssNetPriv_OnDomainPermission(&message, &meta); break;
    default: return NET_CAN_MALFORMED;
    }
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


송신 `DomainNet_PublishPermission`은 합성이 완료된 Net DTO 입력이고 C2 입력3종의 전체 필드 투영·합성 순서는 [Permission 합성](../../flows/CAN_BCM_AND_PERMISSION.md)에 정의한다. Net DTO에 callback3종의 포인터나 내부 meta 구조체를 추가하지 않는다.

Encode의 CheckFrame(...BCM...)은 방송 payload의 대표 구조 검증이다. 실제 DOMAIN→BCM/CIS/VSS 각 송수신 쌍의 revision/profile 채택 근거는 공통 Tx admission에서 개별 확인한다. BCM 채택으로 CIS/VSS 채택을 대신하지 않는다. WINDOW는 실행 HOLD를 유지한다.

이 정기 상태/Permission은 DomainIf_RxNetworkLocalFailure의 개별 결과 귀속 대상이 아니다. 전송 중단·실패는 원 item과 로컬 전송/health 관측으로 보존하며, 위 DOMAIN 명령 후발 실패 설명이 이 메시지에 F09 통지를 추가하지 않는다.
