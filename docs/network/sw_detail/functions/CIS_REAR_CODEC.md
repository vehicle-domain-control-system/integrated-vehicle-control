# CIS_REAR Codec — 고정 바이트와 Net 값의 변환

M05는 시간이나 차량 판단을 생성하지 않는다. [Data/Wire 정본](../data/CIS_REAR_DATA.md#wire)의 값만 읽고 쓴다. 아래 두 원형은 S2 F15/F14에 대응하는 `PROPOSED_PRIVATE_FUNCTION`이며 실제 제품 함수가 아니다.

<a id="encode"></a>
## NetRear_Encode

```c
NetRearResult NetRear_Encode(const NetCisRear *message,
                            const NetCanSource *source,
                            NetRearFrame *out);
```

Caller는 CIS [송신 선택](CIS_REAR_TX_DISPATCH.md)이다. Called에 시간원/Driver/DOMAIN은 없으며 바이트 기록을 이 함수 책임으로 수행한다.

| 인자 | 타입·방향·범위·단위 | NULL·수명 |
|---|---|---|
| message | IN `const NetCisRear *`, AGE는 최종 encode 시점까지 반영, 거리 raw1=1mm | NULL 불가, 호출 동안 borrow |
| source | IN `const NetCanSource *`, 수용된 CIS Boot nonzero·선택된 TX 순번 | NULL 불가, 호출 동안 borrow, 새 Boot 발급 없음 |
| out | OUT `NetRearFrame *`, rear20B 사본 저장소 | NULL 불가. 입력과 겹치지 않는 caller 소유 공간; 실패 시 변경 없음 |

반환은 NET_REAR_OK(완전 프레임 작성), INVALID_ARGUMENT(포인터/값/명시 모순), NOT_READY(선택 profile·Boot/순번 전제가 없음)다. 출력 준비 성공은 Driver 접수가 아니다. 순번 소비·장치 참조 상태를 바꾸지 않으며 호출 중 입력이 불변이라는 전제에서 재진입 가능한 순수 변환으로 설계한다. profile 읽기와 실제 재진입 구현은 미검증이다.

```c
NetRearResult NetRear_Encode(const NetCisRear *message,
                            const NetCanSource *source, NetRearFrame *out)
{
    if (message == NULL || source == NULL || out == NULL)
        return NET_REAR_INVALID_ARGUMENT;
    if (입출력 저장소가 겹치거나 코드 집합·VALID_DISTANCE와 sentinel 조건이 틀리면)
        return NET_REAR_INVALID_ARGUMENT;
    if (선택 profile이 없거나 Boot0 또는 송신 순번의 전제가 미확인이라면)
        return NET_REAR_NOT_READY;
    // 별도 임시 프레임20B를 0으로 준비한다. struct 메모리 복사는 하지 않는다.
    // B0에 Boot u32LE, B4에 TX_SEQUENCE u32LE, B8에 UPDATE_SEQUENCE u32LE 기록.
    // B12 AGE u16LE, B14 거리 u16LE, B16 validity, B17 reason, B18 status 기록.
    // B19는 0 그대로 둔다. ID0x0A0·DLC11·길이20·표준/FDF/BRS/RTR 조건을 설정한다.
    // payload VERSION/응용 CRC/ACK 필드를 추가하지 않는다.
    // 모든20B와 flags를 확인한 다음에만 완전한 임시 객체를 out에 반영한다.
    return NET_REAR_OK;
}
```

<a id="decode"></a>
## NetRear_Decode

```c
NetRearResult NetRear_Decode(const NetRearFrame *frame,
                            NetCisRear *out,
                            NetCanRxMeta *meta);
```

Caller는 DOMAIN M07 수신 사본을 소비하는 [NetRear_ReceiveFrame](CIS_REAR_RX_ADMISSION.md#receive-frame)이다. 성공한 두 출력은 같은 입력 프레임 한 건이며 [수신 수용](CIS_REAR_RX_ADMISSION.md#admit)으로 함께 전달한다. 다른 호출은 하지 않는다.

| 인자 | 타입·방향·범위·단위 | NULL·수명 |
|---|---|---|
| frame | IN `const NetRearFrame *`, 실제 Driver 수신 flags/시각/완전 bytes | NULL 불가, 호출 내 불변 사본; raw pointer 반환 후 보관 금지 |
| out | OUT `NetCisRear *`, 여섯 Net 멤버 | NULL 불가, caller 저장소, 성공 때만 갱신 |
| meta | OUT `NetCanRxMeta *`, 동일 입력 Boot/TX/실제 RX ms | NULL 불가, caller 저장소, 성공 때만 out과 함께 갱신 |

입출력 저장소는 겹치지 않아야 한다. 반환은 OK(형식 변환만), INVALID_ARGUMENT(NULL/alias), MALFORMED(Driver 오류/ID/flags/DLC/길이/코드/padding/명시 모순), NOT_READY(선택 profile 부재)다. C2 소비·Boot admission·순서·AGE 확정은 성공 범위가 아니다. 실패 시 두 OUT과 정상 liveness·수신 순번 장부는 변경하지 않는다.

```c
NetRearResult NetRear_Decode(const NetRearFrame *frame,
                            NetCisRear *out, NetCanRxMeta *meta)
{
    if (frame == NULL || out == NULL || meta == NULL || 저장소가 겹치면)
        return NET_REAR_INVALID_ARGUMENT;
    if (Driver 오류가 있거나 필요한 실제 입력 메타를 확보하지 못하면)
        return NET_REAR_MALFORMED;
    if (양단 profile 선택이 없으면) return NET_REAR_NOT_READY;
    if (ID0x0A0·표준11bit·FDF1·BRS1·RTR없음 조건이 다르면)
        return NET_REAR_MALFORMED;
    if (DLC != 11 또는 실제 확보 길이 != 20이면)
        return NET_REAR_MALFORMED;
    // 길이 확인 뒤에만 고정 offset을 읽는다. 모든 멀티바이트는 LE로 복원한다.
    // Net 임시 객체와 meta 임시 객체를 별도로 구성한다.
    if (Boot0 또는 validity0~1/reason0~9/status0~5 범위 밖이면)
        return NET_REAR_MALFORMED;
    if (VALID_DISTANCE인데 거리65535이거나 B19가0이 아니면)
        return NET_REAR_MALFORMED;
    // 수신시각은 frame.receivedAtMs, Boot/TX는 수신 바이트 그대로 보존한다.
    // DOMAIN의 지금 시각이나 로컬 Boot로 대체하지 않는다.
    // 정상 NO_OBJECT·열화 품질과 SOURCE_AGE65535 포화를 그대로 보존한다.
    // out과 meta 모두의 완전한 사본이 준비된 뒤 함께 반환한다.
    return NET_REAR_OK;
}
```

**범위:** TX_SEQUENCE0·wrap은 정책 미확인 상태와 연결하여 후속 admission에서 제한한다. 반면 BOOT_ID0은 원 Wire 예약값이다. “Frame 버전 검사”는 이 메시지의 payload에 가짜 VERSION byte를 읽는 뜻이 아니며 배포 profile 일치 확인이다. CRC는 Driver 관측을 사용하고 UART용 CRC를20B에 적용하지 않는다. 19B 입력의 padding 누락,21B 입력의 여분, CAN분절 복원 등은 자동 수용하지 않는다.

[M05](../modules/M05_CAN_CODEC.md) · [L04](../layers/L04_FRAME_CODEC.md) · [Data](../data/CIS_REAR_DATA.md) · [TX](CIS_REAR_TX_DISPATCH.md) · [RX](CIS_REAR_RX_ADMISSION.md)
