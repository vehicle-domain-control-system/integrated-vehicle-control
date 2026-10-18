# BCM_CLIMATE_STATE — 송신·Codec·수신 상세설계

`WIRE-P01-BCM_CLIMATE_STATE` · 수신 `RX-P01-BCM_CLIMATE_STATE-DOMAIN`.
원 후보: ID `0x210`, used_len `24`B, CAN frame `24`B, DLC 코드 `12`. padding 없음. ID/offset/시간/profile은 `WIRE_PROPOSED/CAN_PROFILE_NOT_TEAM_VERIFIED`이며 외부 계약 확정이 아니다. 근거 `B07 can/BCM_CLIMATE_STATE.md#wire`, `B07 CAN_CODEC_SPEC.md`, `S4C TIME_IDENTITY_CONTRACT.md`, `S4C RESOURCE_QUEUE_DRIVER_FLOW.md`.

[자료 정본](../../data/CAN_BCM_DATA.md#type-NetBcmClimateState) · [공통 Codec](../CAN_COMMON_CODEC.md#check-frame) · [전송·수명](../CAN_TRANSFER_CONTROL.md) · [전체 공통 흐름](../../flows/CAN_NETWORK_PATHS.md) · [BCM·Permission 흐름](../../flows/CAN_BCM_AND_PERMISSION.md)

## 입력·출력과 실행 문맥

Network의 Codec·queue·driver 인계·수신 검증·typed 투영이 이 문서의 본체다. 기존 공개 API 원형을 변경하지 않는다. DOMAIN 공개 DomainIf_Tx 본체는 DOMAIN 소유이고 Network는 TxPort callback 본체와 등록 요청을 제공한다. 상대 ECU의 App/DomainIf_Rx 함수는 해당 노드 내부에서만 호출한다. DOMAIN C2 호출은 초기 Superloop의 ISR 밖 단일 담당 문맥에서 직렬 처리하며 ISR에서 직접 호출하지 않는다. BCM/CIS/WINDOW/VSS의 Task 또는 superloop 실제 바인딩·동기화는 해당 노드 합의 전 계약 HOLD다.


<a id="api-BcmNet_PublishClimateState"></a>
### `BcmNet_PublishClimateState`

기존 선언 · 본체 담당 `BCM Network` · `S4A api/bcm.md#api-BcmNet_PublishClimateState`.

```c
NetSubmitResult BcmNet_PublishClimateState(
    const NetBcmClimateState *message
);
```


<a id="api-DomainIf_RxBcmClimateState"></a>
### `DomainIf_RxBcmClimateState`

기존 선언 · 본체 담당 `DOMAIN (공개 본체 소유 합의 완료)` · `S4A api/domain_rx.md#api-DomainIf_RxBcmClimateState`.

```c
DomainIf_Status_t DomainIf_RxBcmClimateState( const ClimateState_t *state);
```


## Wire 필드와 완전 자료형

아래 표는 원 후보의 모든 사용 byte를 지정한다. 헤더 BOOT_ID/TX_SEQUENCE는 DTO에 추가하지 않고 NetCanRxMeta.source에 보존한다. 구조체 memcpy/직접 cast는 사용하지 않는다.

| Offset / 폭(B) | Wire 필드 → typed 멤버(타입) | Raw / 부호 / 순서 | 코드·범위·단위 | sentinel / unknown | T33 행 |
|---|---|---|---|---|---|
| 0 / 4 | `BOOT_ID` → `meta.source.boot_id` (`uint32_t`) | u32 / unsigned / LE | 0 예약; 단위=NOT_SPECIFIED | 0 예약 | 196 |
| 4 / 4 | `TX_SEQUENCE` → `meta.source.tx_sequence` (`uint32_t`) | u32 / unsigned / LE | 최초1, 프레임마다 증가; wrap 정책 OPEN; 단위=NOT_SPECIFIED | NOT_SPECIFIED | 196 |
| 8 / 1 | `FAN_COMMAND_LEVEL` → `fan_command_level` (`NetFanLevel`) | u8 / unsigned / N/A | 0=NET_FAN_OFF; 1=NET_FAN_LOW; 2=NET_FAN_MEDIUM; 3=NET_FAN_HIGH; 단위=NOT_SPECIFIED | NOT_SPECIFIED | 197 |
| 9 / 1 | `FAN_MEASURED_LEVEL` → `fan_measured_level` (`NetFanLevel`) | u8 / unsigned / N/A | 0=NET_FAN_OFF; 1=NET_FAN_LOW; 2=NET_FAN_MEDIUM; 3=NET_FAN_HIGH; 255=NET_FAN_UNKNOWN; 단위=NOT_SPECIFIED | 255=UNKNOWN | 198 |
| 10 / 1 | `THERMAL_DIRECTION_STATE` → `thermal_direction_state` (`NetThermalDirection`) | u8 / unsigned / N/A | 0=NET_THERMAL_IDLE; 1=NET_THERMAL_COOL; 2=NET_THERMAL_HEAT; 단위=NOT_SPECIFIED | NOT_SPECIFIED | 199 |
| 11 / 1 | `THERMAL_OUTPUT_LEVEL` → `thermal_output_percent` (`uint8_t`) | u8 / unsigned / N/A | 0~100%, 1%/raw; 단위=% (1 percent/raw) | NOT_SPECIFIED | 200 |
| 12 / 1 | `HEAT_REMOVAL_STATE` → `heat_removal_state` (`NetHeatRemovalState`) | u8 / unsigned / N/A | 0=NET_HEAT_REMOVAL_NORMAL; 1=NET_HEAT_REMOVAL_OVERHEAT_CUTOFF; 255=NET_HEAT_REMOVAL_UNAVAILABLE; 단위=NOT_SPECIFIED | 255=측정 불가 | 201 |
| 13 / 1 | `FAN_COMMAND_QUALITY` → `fan_command_quality` (`NetValueQuality`) | u8 / unsigned / N/A | 0=NET_QUALITY_OK; 1=NET_QUALITY_STALE; 2=NET_QUALITY_INVALID; 3=NET_QUALITY_NO_DATA; 단위=NOT_SPECIFIED | NOT_SPECIFIED | 202 |
| 14 / 1 | `FAN_MEASURED_QUALITY` → `fan_measured_quality` (`NetValueQuality`) | u8 / unsigned / N/A | 0=NET_QUALITY_OK; 1=NET_QUALITY_STALE; 2=NET_QUALITY_INVALID; 3=NET_QUALITY_NO_DATA; 단위=NOT_SPECIFIED | NOT_SPECIFIED | 203 |
| 15 / 1 | `THERMAL_DIRECTION_QUALITY` → `thermal_direction_quality` (`NetValueQuality`) | u8 / unsigned / N/A | 0=NET_QUALITY_OK; 1=NET_QUALITY_STALE; 2=NET_QUALITY_INVALID; 3=NET_QUALITY_NO_DATA; 단위=NOT_SPECIFIED | NOT_SPECIFIED | 204 |
| 16 / 1 | `THERMAL_OUTPUT_QUALITY` → `thermal_output_quality` (`NetValueQuality`) | u8 / unsigned / N/A | 0=NET_QUALITY_OK; 1=NET_QUALITY_STALE; 2=NET_QUALITY_INVALID; 3=NET_QUALITY_NO_DATA; 단위=NOT_SPECIFIED | NOT_SPECIFIED | 205 |
| 17 / 1 | `HEAT_REMOVAL_QUALITY` → `heat_removal_quality` (`NetValueQuality`) | u8 / unsigned / N/A | 0=NET_QUALITY_OK; 1=NET_QUALITY_STALE; 2=NET_QUALITY_INVALID; 3=NET_QUALITY_NO_DATA; 단위=NOT_SPECIFIED | NOT_SPECIFIED | 206 |
| 18 / 4 | `UPDATE_SEQUENCE` → `update_sequence` (`uint32_t`) | u32 / unsigned / LE | 이번 공조 상태 평가 번호; 단위=NOT_SPECIFIED | NOT_SPECIFIED | 207 |
| 22 / 2 | `SOURCE_AGE` → `source_age_ms` (`uint16_t`) | u16 / unsigned / LE | 평가 후 경과 시간, ms; 단위=ms | 65535 포화 하한; 정확한 경과 상한 또는 fresh의 증거 아님 | 208 |

| C2 선언 멤버 / 타입 | 확인한 raw·Net 대응 또는 표현 제한 |
|---|---|
| `ClimateState_t.commandedFanLevel` / `FanLevel_t` | FAN_COMMAND_LEVEL; 이름 대응 |
| `ClimateState_t.measuredFanLevel` / `FanLevel_t` | FAN_MEASURED_LEVEL; UNKNOWN255 보존 |
| `ClimateState_t.thermalDirection` / `ThermalDirection_t` | THERMAL_DIRECTION_STATE; 이름 대응 |
| `ClimateState_t.thermalOutputLevelPercent` / `uint8_t` | THERMAL_OUTPUT_LEVEL; 0..100% |
| `ClimateState_t.heatRemovalState` / `HeatRemovalState_t` | HEAT_REMOVAL_STATE; 이름 대응 |
| `ClimateState_t.commandedFanQuality` / `DataQuality_t` | FAN_COMMAND_QUALITY; 이름 대응 |
| `ClimateState_t.measuredFanQuality` / `DataQuality_t` | FAN_MEASURED_QUALITY; 이름 대응 |
| `ClimateState_t.thermalDirectionQuality` / `DataQuality_t` | THERMAL_DIRECTION_QUALITY; 이름 대응 |
| `ClimateState_t.thermalOutputQuality` / `DataQuality_t` | THERMAL_OUTPUT_QUALITY; 이름 대응 |
| `ClimateState_t.heatRemovalQuality` / `DataQuality_t` | HEAT_REMOVAL_QUALITY; 이름 대응 |
| `ClimateState_t.meta` / `SignalMeta_t` | UPDATE_SEQUENCE/SOURCE_AGE; 필드별 품질이 우선, 공유 요약·바인딩 HOLD |
| `ClimateState_t.sourceGeneration` / `SourceGeneration_t` | 같은 원 관측의 생산 세대. CAN BOOT_ID 자동 복사 금지; 실제 대응·generation 0 정책 미확정이면 계약 HOLD |

`NetBcmClimateState`와 관련 C2 타입의 전체 선언·필드 표·enum은 [BCM 자료 정본](../../data/CAN_BCM_DATA.md)에 한 번만 정의한다. 공통 품질/원본 Context는 [공통 자료](../../data/CAN_COMMON_DATA.md), Meta/Status는 [B11 자료](../../data/CIS_REAR_DATA.md)에 있다.

## 메시지별 변환과 판단 경계

commanded fan과 measured fan, direction/output/heat removal 및 다섯 품질을 각각 보존한다. measured255는 UNKNOWN이며 commanded255는 malformed이다. heat255는 UNMEASURABLE로 변환한다. SOURCE_AGE는 이번 평가의 경과시간이다. measured fan 보고가 current command의 성공을 증명하지 않는다. ClimateState_t 후보의 sourceGeneration은 원 상태 생산 세대이며 CAN BOOT_ID와의 대응을 확인한 같은 관측의 근거로만 설정한다. 알려진 세대는 INVALID/STALE이라는 이유로 0으로 지우지 않는다. generation 0 수용 정책과 실제 BCM 연결은 미확정이다. SignalMeta_t.quality 합성 규칙이 없으면 ingress 계약 HOLD다.

<a id="codec"></a>
## 순수 Codec 원형과 한국어 C 의사코드

아래 함수만 `PROPOSED_PRIVATE_FUNCTION`이며 제품에 존재한다는 주장이 아니다. 입력은 호출 동안 빌리고 출력은 성공 때만 한 번 commit한다. 호출자는 입력과 모든 출력 객체의 저장 영역을 서로 겹치지 않게 제공한다. Decode 성공은 바이트 해석 성공이며 새 기동/순서/AGE/기능 소비 승인과 별개다. 공통 CheckFrame은 선언된 [정본](../CAN_COMMON_CODEC.md#check-frame)을 호출한다. 모든 u16/u32는 unsigned LE이며 정수 승격 후 이동한다. 지원되지 않는 raw와 padding은 전체 프레임을 거부한다. 정확한 65535 AGE는 포화 하한으로 보존하며 fresh로 승인하지 않는다.

```c
/* PROPOSED_PRIVATE_FUNCTION */
NetCanResult NetCan_EncodeBcmClimateState(const NetBcmClimateState *message, const NetCanSource *source, NetCanProfile profile, NetCanFrame *frame);
NetCanResult NetCan_DecodeBcmClimateState(const NetCanFrame *frame, NetCanProfile profile, NetBcmClimateState *message, NetCanRxMeta *meta);
```


```c
NetCanResult NetCan_EncodeBcmClimateState(const NetBcmClimateState *message, const NetCanSource *source, NetCanProfile profile, NetCanFrame *frame)
{
    if (message == NULL || source == NULL || frame == NULL) return NET_CAN_INVALID_ARGUMENT;
    if (source->boot_id == 0) return NET_CAN_MALFORMED;
    if (profile == NET_CAN_PROFILE_UNSELECTED) return NET_CAN_CONTRACT_HOLD;
    if (message->fan_command_level > 3) return NET_CAN_MALFORMED;
    if ((message->fan_measured_level != 0 && message->fan_measured_level != 1 && message->fan_measured_level != 2 && message->fan_measured_level != 3 && message->fan_measured_level != 255)) return NET_CAN_MALFORMED;
    if (message->thermal_direction_state > 2) return NET_CAN_MALFORMED;
    if (message->thermal_output_percent > 100) return NET_CAN_MALFORMED;
    if ((message->heat_removal_state != 0 && message->heat_removal_state != 1 && message->heat_removal_state != 255)) return NET_CAN_MALFORMED;
    if (message->fan_command_quality > 3) return NET_CAN_MALFORMED;
    if (message->fan_measured_quality > 3) return NET_CAN_MALFORMED;
    if (message->thermal_direction_quality > 3) return NET_CAN_MALFORMED;
    if (message->thermal_output_quality > 3) return NET_CAN_MALFORMED;
    if (message->heat_removal_quality > 3) return NET_CAN_MALFORMED;
    NetCanFrame local = {0};
    local.canId = 0x210; local.dlc = 12; local.length = 24;
    local.extended = false; local.fdf = true; local.brs = true; local.rtr = false;
    // 원 CAN FD profile의 BRS=1 후보. 양단 선택/채택 근거가 없으면 공통 CheckFrame이 CONTRACT_HOLD.
    // local.receivedAtMs는 Tx 관측시각이 아니므로 Rx 시각을 만들지 않는다.
    for (uint8_t k = 0; k < 4; ++k) local.data[0+k] = (uint8_t)(((uint32_t)source->boot_id >> (8u*k)) & 0xFFu);
    for (uint8_t k = 0; k < 4; ++k) local.data[4+k] = (uint8_t)(((uint32_t)source->tx_sequence >> (8u*k)) & 0xFFu);
    for (uint8_t k = 0; k < 1; ++k) local.data[8+k] = (uint8_t)(((uint32_t)message->fan_command_level >> (8u*k)) & 0xFFu);
    for (uint8_t k = 0; k < 1; ++k) local.data[9+k] = (uint8_t)(((uint32_t)message->fan_measured_level >> (8u*k)) & 0xFFu);
    for (uint8_t k = 0; k < 1; ++k) local.data[10+k] = (uint8_t)(((uint32_t)message->thermal_direction_state >> (8u*k)) & 0xFFu);
    for (uint8_t k = 0; k < 1; ++k) local.data[11+k] = (uint8_t)(((uint32_t)message->thermal_output_percent >> (8u*k)) & 0xFFu);
    for (uint8_t k = 0; k < 1; ++k) local.data[12+k] = (uint8_t)(((uint32_t)message->heat_removal_state >> (8u*k)) & 0xFFu);
    for (uint8_t k = 0; k < 1; ++k) local.data[13+k] = (uint8_t)(((uint32_t)message->fan_command_quality >> (8u*k)) & 0xFFu);
    for (uint8_t k = 0; k < 1; ++k) local.data[14+k] = (uint8_t)(((uint32_t)message->fan_measured_quality >> (8u*k)) & 0xFFu);
    for (uint8_t k = 0; k < 1; ++k) local.data[15+k] = (uint8_t)(((uint32_t)message->thermal_direction_quality >> (8u*k)) & 0xFFu);
    for (uint8_t k = 0; k < 1; ++k) local.data[16+k] = (uint8_t)(((uint32_t)message->thermal_output_quality >> (8u*k)) & 0xFFu);
    for (uint8_t k = 0; k < 1; ++k) local.data[17+k] = (uint8_t)(((uint32_t)message->heat_removal_quality >> (8u*k)) & 0xFFu);
    for (uint8_t k = 0; k < 4; ++k) local.data[18+k] = (uint8_t)(((uint32_t)message->update_sequence >> (8u*k)) & 0xFFu);
    for (uint8_t k = 0; k < 2; ++k) local.data[22+k] = (uint8_t)(((uint32_t)message->source_age_ms >> (8u*k)) & 0xFFu);
    // 0으로 초기화한 padding을 유지. 수신시간/driver 상태는 송신 성공 근거 아님.
    NetCanResult checked = NetCan_CheckFrame(&local, NET_CAN_BCM_CLIMATE_STATE, NET_CAN_NODE_DOMAIN, profile);
    if (checked != NET_CAN_OK) return checked;
    *frame = local;
    return NET_CAN_OK;
}

NetCanResult NetCan_DecodeBcmClimateState(const NetCanFrame *frame, NetCanProfile profile, NetBcmClimateState *message, NetCanRxMeta *meta)
{
    if (frame == NULL || message == NULL || meta == NULL) return NET_CAN_INVALID_ARGUMENT;
    NetCanResult checked = NetCan_CheckFrame(frame, NET_CAN_BCM_CLIMATE_STATE, NET_CAN_NODE_DOMAIN, profile);
    if (checked != NET_CAN_OK) return checked;
    NetBcmClimateState local = {0}; NetCanRxMeta localMeta = {0};
    { uint32_t value = 0;
      for (uint8_t k = 0; k < 4; ++k) value |= ((uint32_t)frame->data[0+k] << (8u*k));
      localMeta.source.boot_id = value; }
    { uint32_t value = 0;
      for (uint8_t k = 0; k < 4; ++k) value |= ((uint32_t)frame->data[4+k] << (8u*k));
      localMeta.source.tx_sequence = value; }
    { uint32_t value = 0;
      for (uint8_t k = 0; k < 1; ++k) value |= ((uint32_t)frame->data[8+k] << (8u*k));
      local.fan_command_level = value; }
    { uint32_t value = 0;
      for (uint8_t k = 0; k < 1; ++k) value |= ((uint32_t)frame->data[9+k] << (8u*k));
      local.fan_measured_level = value; }
    { uint32_t value = 0;
      for (uint8_t k = 0; k < 1; ++k) value |= ((uint32_t)frame->data[10+k] << (8u*k));
      local.thermal_direction_state = value; }
    { uint32_t value = 0;
      for (uint8_t k = 0; k < 1; ++k) value |= ((uint32_t)frame->data[11+k] << (8u*k));
      local.thermal_output_percent = value; }
    { uint32_t value = 0;
      for (uint8_t k = 0; k < 1; ++k) value |= ((uint32_t)frame->data[12+k] << (8u*k));
      local.heat_removal_state = value; }
    { uint32_t value = 0;
      for (uint8_t k = 0; k < 1; ++k) value |= ((uint32_t)frame->data[13+k] << (8u*k));
      local.fan_command_quality = value; }
    { uint32_t value = 0;
      for (uint8_t k = 0; k < 1; ++k) value |= ((uint32_t)frame->data[14+k] << (8u*k));
      local.fan_measured_quality = value; }
    { uint32_t value = 0;
      for (uint8_t k = 0; k < 1; ++k) value |= ((uint32_t)frame->data[15+k] << (8u*k));
      local.thermal_direction_quality = value; }
    { uint32_t value = 0;
      for (uint8_t k = 0; k < 1; ++k) value |= ((uint32_t)frame->data[16+k] << (8u*k));
      local.thermal_output_quality = value; }
    { uint32_t value = 0;
      for (uint8_t k = 0; k < 1; ++k) value |= ((uint32_t)frame->data[17+k] << (8u*k));
      local.heat_removal_quality = value; }
    { uint32_t value = 0;
      for (uint8_t k = 0; k < 4; ++k) value |= ((uint32_t)frame->data[18+k] << (8u*k));
      local.update_sequence = value; }
    { uint32_t value = 0;
      for (uint8_t k = 0; k < 2; ++k) value |= ((uint32_t)frame->data[22+k] << (8u*k));
      local.source_age_ms = value; }
    if (localMeta.source.boot_id == 0) return NET_CAN_MALFORMED;
    if (local.fan_command_level > 3) return NET_CAN_MALFORMED;
    if ((local.fan_measured_level != 0 && local.fan_measured_level != 1 && local.fan_measured_level != 2 && local.fan_measured_level != 3 && local.fan_measured_level != 255)) return NET_CAN_MALFORMED;
    if (local.thermal_direction_state > 2) return NET_CAN_MALFORMED;
    if (local.thermal_output_percent > 100) return NET_CAN_MALFORMED;
    if ((local.heat_removal_state != 0 && local.heat_removal_state != 1 && local.heat_removal_state != 255)) return NET_CAN_MALFORMED;
    if (local.fan_command_quality > 3) return NET_CAN_MALFORMED;
    if (local.fan_measured_quality > 3) return NET_CAN_MALFORMED;
    if (local.thermal_direction_quality > 3) return NET_CAN_MALFORMED;
    if (local.thermal_output_quality > 3) return NET_CAN_MALFORMED;
    if (local.heat_removal_quality > 3) return NET_CAN_MALFORMED;
    localMeta.received_at_ms = frame->receivedAtMs; // 하드웨어/driver 경계에서 확보한 로컬 시각
    *message = local; *meta = localMeta;
    return NET_CAN_OK;
}
```

## 기존 Net 송신 본체의 한국어 C 의사코드

아래는 위에 선언한 기존 `BcmNet_PublishClimateState`의 Network 소유 본체 설계다. DTO 구조체의 값 복사는 자기 소유 의미 사본이며 Wire 통복사가 아니다. 공통 전송 제어의 Admission·AGE·dispatch·후발 실패 절차를 순서대로 구체화한다.

```c
NetSubmitResult BcmNet_PublishClimateState(const NetBcmClimateState *message)
{
    if (message == NULL) return NET_SUBMIT_INVALID_ARGUMENT;
    // 아래 Codec에서 열거한 각 필드의 전체 허용 집합/범위와 메시지별 조합을 먼저 검사한다.
    if (/* 값 또는 금지 조합 오류 */) return NET_SUBMIT_INVALID_ARGUMENT;
    if (/* 실제 profile 채택·생산기동·원시간·필수 consumer/실패 경계가 준비되지 않음 */)
        return NET_SUBMIT_NOT_READY;
    // 원 source/evaluation key별 아직 시작하지 않은 상태 item에 필요한 안정 보관소를 확보한다.
    if (/* 해당 item과 원본 시간/identity를 보관할 공간 부족 */) return NET_SUBMIT_NO_CAPACITY;
    NetBcmClimateState owned = *message;
    // owned와 원본 AGE 기준·평가/명령/사건 식별자를 같은 item에 값으로 복사한다.
    // 원 포인터를 queue에 남기지 않고, 임시 예약 이후 실패하면 예약만 반환한다.
    // 공통의 유효 순서/교체 정책을 확인하고 owned item을 원자적으로 게시한다.
    // 이 게시가 완료되지 않으면 OK를 반환하지 않는다.
    return NET_SUBMIT_OK;
}

// 이 item의 F12 dispatch 계속:
// 현재 peer/원문맥·원 AGE와 누적 경과를 재검사한다. 미확정시간/만료는 실제 송신 전 정지한다.
// 승인된 생산 BOOT 및 전송 순서를 공통 정책으로 취득한 뒤 별도 임시 frame을 확보한다.
// NetCan_EncodeBcmClimateState에 owned DTO와 source, 채택 profile, 임시 frame을 전달한다.
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
NetCanResult NetCan_RxBcmClimateState(const NetCanFrame *frame, NetCanProfile profile, DomainIf_Status_t *consumerStatus);

NetCanResult NetCan_RxBcmClimateState(const NetCanFrame *frame, NetCanProfile profile, DomainIf_Status_t *consumerStatus)
{
    if (consumerStatus == NULL) return NET_CAN_INVALID_ARGUMENT;
    // consumerStatus는 실제 DomainIf_Rx 호출이 있을 때만 출력한다. 그 전에는 호출자 값을 유지한다.
    NetBcmClimateState message; NetCanRxMeta meta;
    NetCanResult result = NetCan_DecodeBcmClimateState(frame, profile, &message, &meta);
    if (result != NET_CAN_OK) return result;
    // 공통 Rx admission 단계: 기동 승인·현재/이전세대·순서·중복·AGE·자원·실행문맥을 각각 판정.
    // 미확정은 CONTRACT_HOLD/TIME_HOLD/ORDER_HOLD, 중복은 DUPLICATE, 공간 부족은 NO_CAPACITY.
    // 격리 항목은 정상 last_rx/heartbeat 또는 기능 cache를 갱신하지 않는다.
    // SAME_INPUT이면 높은 TX의 raw 기준선과 같은 now의 보수적 AGE를 함께 commit한다.
    // 이 경우 C2/App 관측을 재발행하지 않고 NET_CAN_OK로 종료한다. 부분 갱신 실패는 둘 다 이전값 유지.
    // command/result/event의 의미 전이는 Owner 규칙으로 분류하며 같은 command의 ACCEPTED→DONE을 불변 관측 충돌로 오인하지 않는다.
    // 승인 자료가 모두 갖춰진 경우에만 다음 typed 투영/해당 소비 경계로 진행한다.
    ClimateState_t value = {0};
    const DataQuality_t qualityMap[4] = {DATA_QUALITY_OK, DATA_QUALITY_STALE, DATA_QUALITY_INVALID, DATA_QUALITY_NO_DATA};
    const FanLevel_t fanMap[4] = {FAN_LEVEL_OFF, FAN_LEVEL_LOW, FAN_LEVEL_MEDIUM, FAN_LEVEL_HIGH};
    const ThermalDirection_t thermalMap[3] = {THERMAL_DIRECTION_IDLE, THERMAL_DIRECTION_COOL, THERMAL_DIRECTION_HEAT};
    value.commandedFanLevel=fanMap[message.fan_command_level];
    value.measuredFanLevel=message.fan_measured_level==255 ? FAN_LEVEL_UNKNOWN : fanMap[message.fan_measured_level];
    value.thermalDirection=thermalMap[message.thermal_direction_state];
    value.thermalOutputLevelPercent=message.thermal_output_percent;
    switch (message.heat_removal_state) {
    case 0: value.heatRemovalState=HEAT_REMOVAL_STATE_NORMAL; break;
    case 1: value.heatRemovalState=HEAT_REMOVAL_STATE_BLOCKED_OVERHEAT; break;
    case 255: value.heatRemovalState=HEAT_REMOVAL_STATE_UNMEASURABLE; break;
    default: return NET_CAN_MALFORMED;
    }
    value.commandedFanQuality=qualityMap[message.fan_command_quality];
    value.measuredFanQuality=qualityMap[message.fan_measured_quality];
    value.thermalDirectionQuality=qualityMap[message.thermal_direction_quality];
    value.thermalOutputQuality=qualityMap[message.thermal_output_quality];
    value.heatRemovalQuality=qualityMap[message.heat_removal_quality];
    if (/* 생산자가 제공/승인한 summary quality 또는 평가 연결 근거가 없음 */) return NET_CAN_CONTRACT_HOLD;
    value.meta.quality = /* 생산자 평가에 연결된 summary quality; enum max 또는 상수 OK 금지 */;
    value.meta.updateSequence = message.update_sequence;
    value.meta.ageMs = /* 공통 AGE 단계에서 같은 로컬 now로 누적한 source_age_ms + 경과의 uint32 값 */;
    // 후보 C2 전체 멤버를 채운다. 초기값 0은 producer generation의 근거가 아니다.
    if (/* 같은 수용 CAN 입력의 BCM 생산 세대와 BOOT_ID 대응이 실제로 확인되지 않음 */)
        return NET_CAN_CONTRACT_HOLD;
    value.sourceGeneration = /* 해당 원 관측에 결합된 검증된 SourceGeneration_t 값 */;
    if (value.sourceGeneration == 0 && /* 이 품질에서 generation 0을 받는 양단 규칙이 미확정 */)
        return NET_CAN_CONTRACT_HOLD;
    // 알려진 원 세대는 품질 저하로 0으로 바꾸지 않고, 통과하려고 nonzero/NO_DATA를 합성하지 않는다.
    // 원 AGE65535는 하한이며 미측정 전송구간은 UNKNOWN. fresh 증거가 필요한 소비는 TIME_HOLD.
    if (/* 원 identity·호출 대상·C2 반환/반영 단계를 기록할 관측 공간 미확보 */) return NET_CAN_NOT_READY;
    DomainIf_Status_t returned = DomainIf_RxBcmClimateState(&value);
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

이 정기 상태/Permission은 DomainIf_RxNetworkLocalFailure의 개별 결과 귀속 대상이 아니다. 전송 중단·실패는 원 item과 로컬 전송/health 관측으로 보존하며, 위 DOMAIN 명령 후발 실패 설명이 이 메시지에 F09 통지를 추가하지 않는다.
