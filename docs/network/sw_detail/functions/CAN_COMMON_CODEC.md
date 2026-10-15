# CAN 공통 Codec — 프레임 경계와 명시 변환

M05/F14·F15는 Driver가 확보한 실제 프레임과 의미 자료 사이를 변환한다. 수신 순서·Manager 상태·전송 시도는 변경하지 않는다. 호출자는 프레임 저장소를 호출 종료까지 고정하고, 출력은 모든 검증이 끝난 뒤 한 번에 인수한다. DOMAIN의 consumer 호출은 Task 문맥이며 Codec 자체가 ISR에서 공개 C2 API를 호출하지 않는다.

[CAN 개요](../04_CAN_FD_OVERVIEW.md) · [M05](../modules/M05_CAN_CODEC.md) · [공통 Data](../data/CAN_COMMON_DATA.md) · [전송 제어](CAN_TRANSFER_CONTROL.md) · [흐름](../flows/CAN_NETWORK_PATHS.md)

<a id="check-frame"></a>
## `NetCan_CheckFrame`

`PROPOSED_PRIVATE_FUNCTION`이며 실제 HAL 함수가 아니다.

```c
NetCanResult NetCan_CheckFrame(const NetCanFrame *frame,
    NetCanMessageKind kind, NetCanNode receiver, NetCanProfile profile);
```

입력 자료는 [Frame](../data/CAN_COMMON_DATA.md#type-NetCanFrame), [MessageKind](../data/CAN_COMMON_DATA.md#type-NetCanMessageKind), [Node](../data/CAN_COMMON_DATA.md#type-NetCanNode), [Profile](../data/CAN_COMMON_DATA.md#type-NetCanProfile) 정본을 따른다. `receiver`는 라우팅 대상 노드이며 송신자 인증 값이 아니다. M01은 초기화 시 송수신 양단 revision/profile 채택과 실행 허용 여부를 별도 불변 문맥으로 연결해야 한다. 단순 enum 인자는 그 근거를 대신하지 않는다. 해당 바인딩·동기화가 없으면 정상 Codec 소비는 보류된다.

| 결과 | 발생 조건 |
|---|---|
| INVALID_ARGUMENT | NULL, 존재하지 않는 kind/node/profile, 입력 인자 범위 오류 |
| CONTRACT_HOLD | 양단 profile 채택 근거 없음, CF02 profile 미선택, WINDOW 실행 경로 |
| MALFORMED | ID/receiver 조합, flags, Driver 오류, DLC/실제 길이, 고정 padding 불일치 |
| OK | 선택한 profile의 공통 frame 경계 검증만 완료. 개별 raw 필드/품질·순서·freshness는 아직 미검증 |

부작용·출력 객체가 없으며 내부 수신 시각, heartbeat, 순서 원장을 갱신하지 않는다. 부재한 배포 계약 때문에 CONTRACT_HOLD가 먼저 반환될 때는 malformed 여부까지 확인했다고 보고하지 않는다. 별도 정적 검수는 후보 배치를 검토할 수 있다.

```c
NetCanResult NetCan_CheckFrame(const NetCanFrame *frame,
    NetCanMessageKind kind, NetCanNode receiver, NetCanProfile profile)
{
    if (frame == NULL) return NET_CAN_INVALID_ARGUMENT;
    // kind/node/profile이 선언된 내부 선택자인지 확인한다.
    // 미정의 선택자면 INVALID_ARGUMENT. 입력만으로 채택 승인을 만들지 않는다.
    // M01에 해당 송수신 쌍의 동일 revision 채택 근거가 없으면 CONTRACT_HOLD.
    // WINDOW 5종 또는 Permission→WINDOW이면 실행 경로는 CONTRACT_HOLD.
    // VSS_WARNING_STATE는 합의된 WARNING51 또는 WARNING60만 사용한다.
    // 나머지는 해당 BASE_CANDIDATE revision의 채택 근거를 요구한다.
    if (frame->extended || !frame->fdf || !frame->brs || frame->rtr || frame->driverError)
        return NET_CAN_MALFORMED;
    // 아래 후보 배치 표로 kind의 ID/소비 노드/used_len/frame_len/DLC를 찾는다.
    // 실제 ID 또는 receiver가 표와 다르면 MALFORMED.
    // dlc는 0..15만; 매핑된 길이와 frame->length 및 기대 길이가 모두 같아야 한다.
    // 모든 길이 검사가 끝나기 전 data 배열을 읽지 않는다.
    // used_len부터 실제 frame_len 직전까지 전부 0이어야 한다.
    // 경고 51/60은 data[51..59]를 보고 선택하지 않는다.
    return NET_CAN_OK;
}
```

위 주석의 검사는 순서가 있는 의사코드 단계이며 실제 빌드할 제품 구현이 아니다. 후보 frame을 실제 수용하도록 허용하는 기본 참 플래그나 생략 가능한 검사는 없다.

<a id="layout"></a>
## DLC·바이트와 필드 변환 규칙

DLC 0..8은 같은 바이트 수, 9=12, 10=16, 11=20, 12=24, 13=32, 14=48, 15=64다. DLC 코드와 data 길이를 같은 값으로 비교하지 않는다. 후보는 표준 11-bit CAN FD, FDF=1/BRS=1/RTR 미사용, nominal500kbps/data2Mbps이며 실제 보드 설정은 미확인이다. 데이터 길이 검증 후 메시지별 고정 offset을 읽는다.

| 자료 | 명시 변환 |
|---|---|
| u16 LE | `b0 | (uint16_t)b1 << 8`; 계산 폭을 먼저 확보한다. |
| u32 LE | 각 바이트를 먼저 u32로 올린 뒤 0/8/16/24 shift한다. |
| i16 LE | u16 패턴을 읽고 0x8000 이상이면 32비트에서 65536을 뺀다. 구현 의존 unsigned→signed 축소로 의미를 정하지 않는다. |
| 쓰기 | 각 필드를 unsigned 패턴으로 범위 검사 후 명시적인 하위 바이트부터 기록한다. |
| enum | 메시지별 switch/table로 C2↔Net↔raw를 매핑한다. 예약 raw·매핑 없는 C2 값은 거부/HOLD다. |
| padding | TX 임시 frame을 0으로 초기화한 후 정의된 필드만 쓴다. RX는 실제 확보된 padding을 검사한다. |

응용 CRC/VERSION/ACK/Heartbeat 필드를 추가하지 않는다. CAN 자체 CRC 또는 ACK는 지정 상대의 typed 소비·기능 완료 증거가 아니다. `sizeof(struct)`, 메모리 통복사, 비트필드 배치로 payload를 만들지 않는다.

공통 B0..3 BOOT_ID는 u32LE, 0 예약이다. B4..7 TX_SEQUENCE는 메시지별 전송 식별이며 원 관측/명령 번호와 다르다. 초기1 후보와 wrap/재기동 정책이 미확정이면 0 또는 순환을 자동 최신으로 판정하지 않는다. Codec는 raw 표현을 검증하고 순서 판단은 M04로 넘긴다. targetBoot/commandId/commandAge/acceptLimit 네 필드는 네 COMMAND 형식에서만 B8/B12/B16/B18에 있다. 그 밖의 메시지에 공통 command header를 덧붙이지 않는다.

온도 i16 raw는 0.01°C, -4000..12500 후보 범위와 -32768 sentinel을 별개로 해석한다. CIS_REAR raw 거리 0.1cm=1mm, 65535 sentinel, RGB255 유효값 등은 해당 Data 정본을 따른다. INVALID 품질과 표현 가능한 열화 상태는 그대로 옮길 수 있지만 예약 raw 또는 불가능한 조합을 조용히 정상화하지 않는다. 일부 필드 실패가 전체 프레임 거부인지 품질 보존인지도 메시지 계약을 따른다.

<a id="encode-decode"></a>
## 메시지별 Encode/Decode 호출 순서

Encode는 typed 인자와 source를 호출 중 빌리고, M04가 확정한 dispatch 시점 AGE를 사용한다. Codec가 자체 clock을 읽거나 AGE를 0으로 재생성하지 않는다. 전체 typed 조합과 매핑 가능성을 먼저 검사하고 별도 임시 frame에 쓴 뒤 완성 frame을 출력한다. 실패 시 이전 출력은 그대로다. 인계된 frame은 Driver 참조 종료까지 불변이다.

Decode는 공통 검증 후 메시지별 모든 필드를 임시 typed 자료로 읽고 범위·reserved·sentinel·품질 조합을 확인한다. 같은 frame의 BOOT/TX와 실제 수신 시각을 `NetCanRxMeta` 임시값에 보존한다. typed 출력과 meta를 함께 복사하기 전 정상 consumer 호출/순서 갱신이 없다. 기존 CIS_REAR의 [NetRear Codec](CIS_REAR_CODEC.md)은 그대로 재사용한다.

| 정적 반례 | 기대 결과 |
|---|---|
| 정상 CIS_REAR ID0x0A0, used19/frame20/DLC11, B19=0 | B11 Codec의 구조 검증 가능. 추가 순서/C2 인계 조건은 별개 |
| DLC11인데 실제19바이트 | MALFORMED; B19를 생성해서 수용하지 않음 |
| ID는 맞고 FDF0/BRS0/RTR1 또는 extended1 | MALFORMED |
| BCM_LIGHT_STATE의 B25..31 중 하나 비제로 | MALFORMED; typed 일부 전달 없음 |
| 경고64바이트, B51..59가 모두0 | 어떤 profile의 승인 증거도 아님; CF02 자동 선택 없음 |
| 프레임 생성 뒤 Driver 참조 중 AGE 수정 | 금지; 별도 다음 전송 사본만 새 AGE 사용 |

근거: `B07 CAN_CODEC_SPEC.md`, `CAN_CONTRACT.md`, `can/*.md`; 시간·수명은 `S4C FRESHNESS_AGE_TIMEOUT.md`, `RESOURCE_QUEUE_DRIVER_FLOW.md`. 후보별 전체 필드표와 실제 API 원형은 [22종 색인](../04_CAN_FD_OVERVIEW.md#messages)에 연결한다.

## 22종 공통 프레임 후보 표

| Message | ID | receiver | used | frame | DLC | zero padding |
|---|---|---|---|---|---|---|
| DOMAIN_PERMISSION | 0x080 | BCM, CIS, WINDOW, VSS | 19 | 20 | 11 | 19..19 |
| BCM_DOOR_COMMAND | 0x100 | BCM | 22 | 24 | 12 | 22..23 |
| BCM_CLIMATE_COMMAND | 0x110 | BCM | 23 | 24 | 12 | 23..23 |
| BCM_LIGHT_COMMAND | 0x120 | BCM | 25 | 32 | 13 | 25..31 |
| WINDOW_COMMAND | 0x090 | WINDOW | 24 | 24 | 12 | 없음 |
| BCM_DOOR_STATE | 0x200 | DOMAIN | 20 | 20 | 11 | 없음 |
| BCM_CLIMATE_STATE | 0x210 | DOMAIN | 24 | 24 | 12 | 없음 |
| BCM_LIGHT_STATE | 0x220 | DOMAIN | 25 | 32 | 13 | 25..31 |
| BCM_RESULT | 0x140 | DOMAIN | 29 | 32 | 13 | 29..31 |
| BCM_EVENT | 0x141 | DOMAIN | 25 | 32 | 13 | 25..31 |
| BCM_STATUS | 0x230 | DOMAIN | 22 | 24 | 12 | 22..23 |
| CIS_ENVIRONMENT | 0x300 | DOMAIN | 40 | 48 | 14 | 40..47 |
| CIS_OCCUPANT | 0x310 | DOMAIN | 25 | 32 | 13 | 25..31 |
| CIS_REAR | 0x0A0 | DOMAIN | 19 | 20 | 11 | 19..19 |
| CIS_STATUS | 0x320 | DOMAIN | 34 | 48 | 14 | 34..47 |
| WINDOW_STATE | 0x0B0 | DOMAIN | 22 | 24 | 12 | 22..23 |
| WINDOW_EVENT | 0x091 | DOMAIN | 26 | 32 | 13 | 26..31 |
| WINDOW_RESULT | 0x092 | DOMAIN | 20 | 20 | 11 | 없음 |
| WINDOW_FAULT | 0x330 | DOMAIN | 22 | 24 | 12 | 22..23 |
| VSS_EVENT | 0x160 | VSS | 23 | 24 | 12 | 23..23 |
| VSS_WARNING_STATE | 0x0C0 | VSS | 60 | 64 | 15 | 60..63 |
| VSS_STATUS | 0x340 | DOMAIN | 28 | 32 | 13 | 28..31 |

WARNING51 대안은 used51/frame64/DLC15/padding51..63이다. 두 WARNING 후보는 frame 길이가 같으며 채택 근거로만 선택한다.
