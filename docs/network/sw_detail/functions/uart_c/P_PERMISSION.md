# P_PERMISSION — DOMAIN 원 판단의 CIS→Pi 중계

`WIRE-P03-P_PERMISSION` / `RX-P03-P_PERMISSION-PI`. CIS→PI, **P03 / VERSION 1 / TYPE 0x62 / Payload 15B / Frame 25B / 7필드**의 `WIRE_PROPOSED` 문서 후보다. 양단 규격 채택·현 펌웨어 지원·실제 송수신을 확인한 결과가 아니다. 과거 2종 후보와 현 3종 후보를 자동 변환하거나 UART-M VERSION2로 해석하지 않는다.

[전체 개요](../../06_UART_C_OVERVIEW.md) · [M06 책임](../../modules/M06_UART_CODEC.md) · [Data 정본](../../data/UART_C_DATA.md) · [Frame·Router](../UART_C_FRAME_ROUTER.md) · [송수신·수명](../UART_C_TX_RX_CONTROL.md) · [DOMAIN→CIS→Pi 흐름](../../flows/UART_C_CIS_TO_PI_PERMISSION.md) · [기존 CAN 원 판단](../can/DOMAIN_PERMISSION.md).

## 근거와 책임

최초 필드 근거는 `S4C_RESULT.zip::evidence/b08/evidence/sources/T35.txt` 78~93행이다. S4C의 `evidence/b08/uart_c_contract.json`에서 같은 Wire ID를 따른다. API는 T10 235~238행과 T26 99~102행, 의미 자료는 T25 `NetPiPermission`, T37 `NetDomainPermission/NetCanRxMeta/NetUartRxMeta`, 시간은 S4C `TIME_IDENTITY_CONTRACT.md`와 `FRESHNESS_AGE_TIMEOUT.md`가 근거다. 후방 활성 Power 원 판단 기한500ms/실제 재평가 약200ms는 후속 직접 합의를 적용한다. `USE_LIMIT`의 조건부500ms·relay 사유·revision/합성 생성 근거는 IF-O05/G02/Q11에 남긴다.

DOMAIN은 원 전원 판단을 생산한다. CIS CAN Network는 같은 검증 사건의 원 DTO·meta를 private relay에 인계하며, CIS relay는 확인된 전원 판단의 전달과 추가 제한만 담당한다. Pi는 로컬 수신 DTO를 자기 기능 사용 조건과 대조한다. Network가 원 전원 허용 판단·Pi 내부 기능·프로세스 종료·물리 전원 차단을 새로 구현하지 않는다.

## 정확한 Data/API와 참조 수명

전체 원 선언은 [NetPiPermission 정본](../../data/UART_C_DATA.md#type-NetPiPermission)에 한 번만 둔다. CAN 입력 [NetDomainPermission](../../data/CAN_BCM_DATA.md#type-NetDomainPermission)의 power/operation·각 quality·원 update/AGE와 **같은 CAN event**의 meta를 통째로 보존한다. 원 타입에는 `quality_reason`이 없다. CIS relay Boot/update는 CAN meta의 DOMAIN Boot/TX와 CAN 합성 UPDATE 및 DOMAIN 원 성공 평가 Sequence를 대체하지 않는다. 이 두 원 번호도 서로 다르며 payload에는 별도 원 성공번호 필드가 없다.

<a id="api-CisNetPriv_RelayPiPermission"></a>
<a id="api-PiApp_OnCisPermission"></a>
기존 private 생산 경계와 공개 소비 경계의 정확한 원형:

```c
NetSubmitResult CisNetPriv_RelayPiPermission(
    const NetDomainPermission *permission,
    const NetCanRxMeta *meta
);
void PiApp_OnCisPermission(
    const NetPiPermission *message,
    const NetUartRxMeta *meta
);
```

`CisNetPriv_RelayPiPermission`은 **NETWORK_PRIVATE**다. Pi 앱이 임의로 작성한 `NetPiPermission*`을 받는 새 공개 송신 API를 만들지 않는다. 기존 CIS App 소비는 `void CisApp_OnDomainPermission(const NetDomainPermission *message, const NetCanRxMeta *meta);`이며 같은 CAN event의 별도 소비 가지다. App이 relay를 다시 호출해야 한다는 중복 의무를 추가하지 않는다.

Relay 입력은 함수 반환까지 borrowed/read-only다. `NET_SUBMIT_OK` 전 원 DTO의 **7개 필드 전체**, 중첩 `meta.source.boot_id/tx_sequence`, `meta.received_at_ms`, 원 판단과 relay 연결·AGE 기준을 자기 소유 보관소에 함께 복사해야 한다. 원 포인터를 큐에 남기지 않는다. Pi callback의 DTO·UART meta도 반환까지만 borrowed다. Pi가 나중에 사용하려면 전체 값과 같은 수신 기준점을 함께 복사한다. `void`는 통지 호출 사실이고 허용 승인·동작 성공 반환이 아니다. Pi의 C 선언은 언어 중립 후보의 표현으로 실제 구현 언어를 강제하지 않는다.

<a id="fields"></a>
## 전 필드와 정확한 byte 위치

모든 정수 raw는 **unsigned**다. Payload B0=Frame B8. 다중 바이트는 LE, 1byte는 endian 적용 대상이 아니다. C enum 크기·struct padding은 Wire가 아니다. 아래 7행은 B0..14를 겹침·빈 byte 없이 사용한다.

| 필드 → typed member | Payload offset / 폭 | Frame offset | Raw / endian / 단위 | 허용 값·특수값과 보존 의미 | T35행 |
|---|---|---|---|---|---|
| `CIS_BOOT_ID` → `cis_boot_id` | 0 / 4 | 8..11 | u32 / LE / 해당 없음 | 0 예약. CIS runtime relay 세대이며 원 DOMAIN Boot가 아니다 | 87 |
| `UPDATE_SEQUENCE` → `update_sequence` | 4 / 4 | 12..15 | u32 / LE / 해당 없음 | CIS relay 의미·제한 revision. 원 성공/CAN 합성/UART LINK와 별개; 초기값·wrap 미정, 0 임의 금지 없음; 단순 반복 유지 | 88 |
| `VEHICLE_POWER_PERMISSION` → `vehicle_power_permission` | 8 / 1 | 16 | u8 / N/A / 해당 없음 | 0 불허, 1 허용, 255 미확인. 값 1 단독으로 허용하지 않음 | 89 |
| `VALUE_QUALITY` → `value_quality` | 9 / 1 | 17 | u8 / N/A / 해당 없음 | 0 OK, 1 STALE, 2 INVALID, 3 NO_DATA. 원 power 품질과 CIS 제한의 대응은 HOLD | 90 |
| `QUALITY_REASON` → `quality_reason` | 10 / 1 | 18 | u8 / N/A / 해당 없음 | 0 NONE, 1 NOT_READY, 2 OUT_OF_RANGE, 3 SENSOR_FAULT, 4 VISION_FAULT, 5 STALE, 6 NO_DATA, 7 COMMUNICATION_FAULT, 8 RECOVERING, 9 DATA_INVALID. 원 DTO에 없는 사유를 복사했다고 하지 않음 | 91 |
| `USE_LIMIT` → `use_limit_ms` | 11 / 2 | 19..20 | u16 / LE / ms | raw 0..65535, 1500ms 후보. 최종 허용 값·0 의미 미정이므로 raw0은 표현 가능하되 사용 승인은 HOLD | 92 |
| `SOURCE_AGE` → `source_age_ms` | 13 / 2 | 21..22 | u16 / LE / ms | 원 DOMAIN 판단 이후 누적. 65535는 포화 하한이며 정확한 나이 상한·fresh 증거가 아니다 | 93 |

Header는 `A5 5A 01 62 0F 00 LINK_LO LINK_HI`다. CRC는 Frame B2..22의 **21바이트**를 CRC-16/CCITT-FALSE(poly0x1021/init0xFFFF/refin·refout false/xorout0)로 계산하여 B23=low, B24=high에 저장한다. `LINK_SEQUENCE` u16은 방향별 frame 순번이며 relay update/DOMAIN 평가 번호가 아니다. Header·CRC 생성과 검사, profile·방향 수용은 [Frame·Router 정본](../UART_C_FRAME_ROUTER.md)이 담당한다.

<a id="codec"></a>
## 순수 Payload Codec — 한국어 C형 의사코드

다음 세 함수는 **PROPOSED_PRIVATE_FUNCTION**이다. 이름을 실제 제품 API·Driver·SDK로 주장하지 않는다. 값 의미 검사는 이 페이지, Frame/CRC·호출 문맥 검사는 공통 정본에 둔다. 호출자는 입력/출력 영역을 겹치지 않게 제공한다. 실패 시 OUT byte/DTO를 전혀 수정하지 않고 성공 시 전 필드를 한 번 commit한다. Decode/Validate의 true는 raw 형식 성립만 뜻하며 기동 수용·시간·허용 소비를 승인하지 않는다.

```c
bool NetUartC_ValidatePermission(const NetPiPermission *message)
{
    if (message == NULL || message->cis_boot_id == 0u) return false;
    if (message->vehicle_power_permission != 0u &&
        message->vehicle_power_permission != 1u &&
        message->vehicle_power_permission != 255u) return false;
    if (message->value_quality > 3u) return false;
    if (message->quality_reason > 9u) return false;
    // u32 update 및 u16 use_limit/source_age의 전체 표현을 보존한다.
    // 미제공된 quality/reason/permission 조합 규칙을 만들지 않는다.
    // use_limit=0, source_age=65535는 raw 성공이어도 허용 소비의 증거가 아니다.
    return true;
}

bool NetUartC_EncodePermissionPayload(
    const NetPiPermission *message, uint8_t *out, uint16_t capacity)
{
    if (out == NULL || capacity < 15u) return false;
    if (!NetUartC_ValidatePermission(message)) return false;
    uint8_t local[15];
    for (uint8_t k = 0u; k < 4u; ++k)
        local[0u+k] = (uint8_t)((message->cis_boot_id >> (8u*k)) & 0xFFu);
    for (uint8_t k = 0u; k < 4u; ++k)
        local[4u+k] = (uint8_t)((message->update_sequence >> (8u*k)) & 0xFFu);
    // 위에서 허용 집합을 검사한 고정폭 의미 alias를 명시된 raw에 대응한다.
    switch (message->vehicle_power_permission) {
    case 0u: local[8] = 0u; break;
    case 1u: local[8] = 1u; break;
    case 255u: local[8] = 255u; break;
    default: return false;
    }
    switch (message->value_quality) {
    case 0u: local[9] = 0u; break;
    case 1u: local[9] = 1u; break;
    case 2u: local[9] = 2u; break;
    case 3u: local[9] = 3u; break;
    default: return false;
    }
    switch (message->quality_reason) {
    case 0u: local[10] = 0u; break; case 1u: local[10] = 1u; break;
    case 2u: local[10] = 2u; break; case 3u: local[10] = 3u; break;
    case 4u: local[10] = 4u; break; case 5u: local[10] = 5u; break;
    case 6u: local[10] = 6u; break; case 7u: local[10] = 7u; break;
    case 8u: local[10] = 8u; break; case 9u: local[10] = 9u; break;
    default: return false;
    }
    local[11] = (uint8_t)(message->use_limit_ms & 0xFFu);
    local[12] = (uint8_t)(((uint32_t)message->use_limit_ms >> 8u) & 0xFFu);
    local[13] = (uint8_t)(message->source_age_ms & 0xFFu);
    local[14] = (uint8_t)(((uint32_t)message->source_age_ms >> 8u) & 0xFFu);
    for (uint8_t k = 0u; k < 15u; ++k) out[k] = local[k];
    return true;
}

bool NetUartC_DecodePermissionPayload(
    const uint8_t *payload, uint16_t length, NetPiPermission *out)
{
    if (payload == NULL || out == NULL || length != 15u) return false;
    NetPiPermission local;
    local.cis_boot_id = (uint32_t)payload[0] |
        ((uint32_t)payload[1] << 8u) | ((uint32_t)payload[2] << 16u) |
        ((uint32_t)payload[3] << 24u);
    local.update_sequence = (uint32_t)payload[4] |
        ((uint32_t)payload[5] << 8u) | ((uint32_t)payload[6] << 16u) |
        ((uint32_t)payload[7] << 24u);
    switch (payload[8]) {
    case 0u: local.vehicle_power_permission = 0u; break;
    case 1u: local.vehicle_power_permission = 1u; break;
    case 255u: local.vehicle_power_permission = 255u; break;
    default: return false;
    }
    switch (payload[9]) {
    case 0u: local.value_quality = 0u; break;
    case 1u: local.value_quality = 1u; break;
    case 2u: local.value_quality = 2u; break;
    case 3u: local.value_quality = 3u; break;
    default: return false;
    }
    switch (payload[10]) {
    case 0u: local.quality_reason = 0u; break; case 1u: local.quality_reason = 1u; break;
    case 2u: local.quality_reason = 2u; break; case 3u: local.quality_reason = 3u; break;
    case 4u: local.quality_reason = 4u; break; case 5u: local.quality_reason = 5u; break;
    case 6u: local.quality_reason = 6u; break; case 7u: local.quality_reason = 7u; break;
    case 8u: local.quality_reason = 8u; break; case 9u: local.quality_reason = 9u; break;
    default: return false;
    }
    local.use_limit_ms = (uint16_t)((uint32_t)payload[11] |
        ((uint32_t)payload[12] << 8u));
    local.source_age_ms = (uint16_t)((uint32_t)payload[13] |
        ((uint32_t)payload[14] << 8u));
    if (!NetUartC_ValidatePermission(&local)) return false;
    *out = local;
    return true;
}
```

여기서 `NetTriState` 등은 T37의 고정폭 정수 alias와 명명 상수 집합이다. 위 분기는 원 숫자와 의미의 명시 대응이며 enum의 메모리를 복사하거나 다른 C2 enum을 cast하는 변환이 아니다. C2 원 판단을 relay 자료로 바꾸는 의미 투영은 이 Codec 밖에서 별도 증거를 요구한다.

## 생산부터 상대 소비까지의 구체 연결

| 단계 | 이 메시지의 처리 | 실패·미확인 시 보존 |
|---|---|---|
| CAN 입력 | 검증된 NetDomainPermission + 같은 NetCanRxMeta를 CIS App/relay에 분배 | 다른 event meta나 값이 같다는 이유로 연결하지 않음 |
| Relay 평가 | 원 power와 독립 operation/alive 근거를 유지, CIS Boot/relay revision·품질 제한·사유·USE_LIMIT 산출 근거 확인 | reason 부재를 NONE으로, revision 부재를0 또는 새 번호로, use-limit 부재를1500으로 채우지 않음 |
| 로컬 제출 | 원 자료/중첩 meta와 relay DTO·시간 기준을 안정 복사 후 게시 | NULL/raw 오류 INVALID_ARGUMENT, 미준비 NOT_READY, 공간 부족 NO_CAPACITY 후보 의미. 부분 접수 없음 |
| 송신 전 | 원 DOMAIN/CIS 문맥·동일 평가·원 AGE 재검사, 미반영 CIS 대기만 한 번 누적·포화 | 재기동/association 상실/미상 시간·만료를 이전 허용의 새 발급으로 바꾸지 않음 |
| Frame/Driver | 15B payload→25B frame, 독립 LinkSequence, CRC, 소유 frame 불변 인계 | accepted·final TX·reference release는 별도 사건. timeout으로 release를 추측하지 않음 |
| Pi Stream | split/noise/CRC/header 검사와 정확한 방향 CIS→PI/V1/0x62/P15 | 잘못된 header/CRC는 start+1 suffix 재탐색. CRC 정상 raw 거부는 전체25B 소비 |
| Typed 인계 | 임시 전 필드 decode·meta를 완성, 수용한 CIS세대/순서·Pi consumer 문맥 확인 | 무근거 Boot/구 형식/순서 wrap은 정상 소비 보류. 정상 INVALID/NO_DATA는 품질 그대로 |
| 사용 시점 | 값1/OK·원 DOMAIN 기한·relay 한도·원 연결과 동일 세대·현재 AGE 증거를 함께 확인 | 하나라도 없으면 새 허용 근거 없음. callback void/CRC 정상은 허용 승인 아님 |

[r1 조건부 실패/번호 분리](../../flows/CAN_BCM_AND_PERMISSION.md#permission-sequences)에 따라 성공17 뒤 평가 실패는 원17·마지막 성공 시각을 유지한 현재 제한이다. 확인된 제한의 새 relay 의미를 원 성공번호가 같다는 이유로 버리지 않는다. FAILED/NO_DATA 시 기존 ALLOW 즉시 철회는 DOMAIN Core 담당자 구현 완료 보고다. UNKNOWN의 원인별 C2 Quality 투영과 false+비정상 품질→255는 DOMAIN Adapter 수용 대기인 권장안이며 Network→CIS→Pi 실제 전달·소비는 BINDING_NOT_VERIFIED다. 이 Codec이 원인을 추측해 값/품질을 바꾸지 않는다. 원 UNKNOWN+OK의 표현 한계를 false/OK로 숨기지 않는다. 이미 확인된 현재 제한·만료 뒤 구 ALLOW pending/retry 재게시를 막고 실제 in-flight frame은 참조 해제까지 불변 유지한다. 전송 실패·원격 동시 반영 미확인은 별도로 남는다.

각 로컬 반환의 실제 바인딩·동기화·후발 실패 관측은 D-03/D-05/IF-O12/G06 미결이다. 이미 반환한 OK를 소급 FAILED로 바꾸지 않고 원 item identity와 실패 관측을 보존한다. DOMAIN C2 failure sink를 CIS/Pi에 복제하지 않는다. 상세 분기는 [relay·소비 후보 의사코드](../../flows/UART_C_CIS_TO_PI_PERMISSION.md#relay-projection), 버퍼/Driver는 [공통 송수신](../UART_C_TX_RX_CONTROL.md)을 따른다.

## 시간·반복·정상 열화와 반례

후방 활성 Power의 원 사용500ms와 실제 재평가 약200ms는 **확정 합의**다. 원 AGE가500ms에 도달하면 사용하지 않는다. CAN50ms 송신/150ms 수신 중단, UART-C 1500ms 수신 중단(P_PERMISSION 반복 없음)은 별도 통신 기준을 유지한다. USE_LIMIT1500ms는 기존 후보이며 동일 후방 원 판단의 양끝 의미가 검증될 때500ms로 바꾸는 안만 조건부다. 원 판단 시작점의 SOURCE_AGE를 쓰며 송신·relay·재수신 때0으로 시작하지 않는다. 원500ms와 검증된 relay 한도 중 더 짧은 기한을 지킨다. 실제 새 원 평가가 완료되면 같은 값도 새 원 순번/평가시각과 함께 즉시 relay한다(반복 없음). 같은 CIS Boot/update의 재수신은 새 허용이 아니며 현재 시간에 환산한 이전·새 AGE 중 더 젊은 것으로 기존 원 판단을 교체하지 않는다. 동일시점 비교·시계 wrap·실제 미반영 UART 전달 여유를 확인하지 못하면 그 영향을 받는 사용은 HOLD다.

| 반례 ID | 입력/관측 | 실제 분기·기대 결과 |
|---|---|---|
| C-P01 정상 후보 | 전 필드 합법·같은 원 CAN event·기동/품질/사유/시간/수용 근거 확보 | Codec true, 15B/25B. 문서 후보로 인계 가능; 제품 적용/실제 허용 성공은 미확인 |
| C-P02 정상 열화 | value1/qualityINVALID 또는 value255/NO_DATA, 사유가 합법 | raw decode true; 원 값·품질 보존, 허용 근거 없음. value0/OK의 확인된 불허와 구분 |
| C-P03 형식 거부 | value2, quality4, reason10, CIS_BOOT0 또는 P14/P16 | Validate/Decode false, OUT 불변. 정확한 P15·CRC 정상 뒤 raw 거부는25B 전체 소비; Header의 P14/P16은 start+1 suffix 재탐색 |
| C-P04 원 사유 부재 | NetDomainPermission에는 reason 없음, 새 relay reason 근거도 없음 | 의미 투영 HOLD. NONE/qualityOK/source_age0 기본치 합성 없음 |
| C-P05 원 기한 경계 | 후방 원 AGE499/500/501ms, original500ms/relay1500ms 후보 |499는 품질·Boot·원 association도 필요.500/501은 새 허용 불가(`>=500`). relay 후보1500은 원 기한을 연장하지 않음 |
| C-P06 원 출처 상실 | DOMAIN Boot/update 부재를 CIS Boot/update로 대체 | same-source 미입증 HOLD. 원 DOMAIN identity는 payload에 없으며 LinkSequence도 대체 불가 |
| C-P07 반복/포화 | 같은 relay update·새 LinkSequence·AGE65535 또는 AGE 감소 | 포화 raw 보존, fresh 승인 없음. 재수신으로 AGE0·원 유효기간 갱신 금지 |
| C-P08 재부팅/늦은 자료 | CIS 재기동/PI 재연결 후 구 허용 대기 frame | 이전 세대·원 판단을 재수용하지 않음. 새 원 판단과 현재 수용 근거 전 허용 없음 |
| C-P09 USE_LIMIT0 | 정확한15B/CRC, use-limit0 | raw decode true. 0을 무제한/즉시만료/기본1500 중 하나로 정하지 않고 사용 조건 HOLD |
| C-P10 접수 뒤 실패 | OK 반환 뒤 Driver fail/timeout, release 미확인 | 과거 OK 변경 없음; identity 보존, release 증거 전 frame 재사용 없음, Pi 허용 성공 주장 없음 |

미결은 **IF-O05/G02/Q11 → S6Q-05·S6Q-14**, 기동/원 연결은 IF-O09/D-02, Driver/실행 문맥은 IF-O12/D-03/D-05/G06 및 S6Q-06·S6Q-12에 같은 ID로 누적한다. 기존 Interface는 고치지 않으며 S6 후 확보된 합의만 별도 승인 범위로 통합한다. 정적 설계 설명은 실제 양단 채택·소비자 연결·허용 판단 완료를 뜻하지 않는다.
