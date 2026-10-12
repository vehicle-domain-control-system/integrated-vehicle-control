# CAN Codec — 한국어 C형 의사코드

[↑ Module](00_MODULE_MAP.md) · [설명](10_BEHAVIOR.md) · [함수 색인](../../../05_CAN_COMMON_FUNCTION_INDEX.md) · [CAN Flow](../../../flows/30_CAN_COMMON_TX_RX_FLOW.md)

**PROPOSED_PRIVATE / DESIGN_ONLY / NOT_COMPILED.** 실제 설치 C/H가 아니다. [공유 타입 전체 선언](../../05_TRANSPORT/10_CAN_TRANSPORT/00_MODULE_MAP.md#common-types), [Source/SHA/실존 I1 원형](00_MODULE_MAP.md#source-evidence), [반환 mapping](10_BEHAVIOR.md#result-mapping)을 따른다. 아래 코드의 local scratch만 새로 작성할 후보이며 I1 내부 byte Codec는 그대로 호출한다. IN/OUT은 생존하는 전체 객체이고 비중첩이어야 한다. 논리 commit은 owner 직렬화로 관측을 보호하며 CPU word 원자성을 뜻하지 않는다.

<a id="fn-u4d-can-codec-encode"></a>
## FN-U4D-CAN-CODEC-ENCODE — CanCodec_Encode

**원형:** `CanCodecResult CanCodec_Encode(const CanRawMessage *input, CanFrame *out);`  
[같은 함수 설명](10_BEHAVIOR.md#fn-u4d-can-codec-encode)

```c
CanCodecResult CanCodec_Encode(const CanRawMessage *input, CanFrame *out)
{
    CanCodecResult result = { CAN_INVALID_ARG, I1_OK, 0 };
    CanFrame scratch = {0};
    size_t written = 0;

    /* CE01: 전체 객체의 NULL만 검사한다. nonalias/안정성은 caller 소유 계약이다. */
    if (input == NULL || out == NULL) return result;

    /* CE02: 두 원 profile만 지원한다. future discrete schema를 만들지 않는다. */
    if (input->kind != CAN_REAR && input->kind != CAN_PERMISSION) {
        result.status = CAN_UNSUPPORTED;
        return result;
    }

    /* CE03: 실제 I1 함수 하나를 scratch20에 호출하고 실제 결과를 보존한다. */
    result.i1_called = 1;
    if (input->kind == CAN_REAR) {
        result.i1 = i1_cis_rear_encode(&input->fields.rear,
            scratch.bytes, 20, &written);
    } else {
        result.i1 = i1_domain_permission_encode(&input->fields.permission,
            scratch.bytes, 20, &written);
    }

    /* CE04: 원 Code/Boot/인자/용량 오류와 예상 밖 I1 오류도 fail closed다. */
    if (result.i1 != I1_OK) {
        result.status = CAN_CODEC_REJECT;
        return result; /* scratch 폐기, OUT/IN 불변 */
    }

    /* CE05: I1_OK만으로 완료하지 않고 실제20B/패딩 불변식을 확인한다. */
    if (written != 20 || scratch.bytes[19] != 0) {
        result.status = CAN_CODEC_REJECT;
        return result; /* 실제 I1_OK는 유지, 자체 불변식 실패를 성공으로 숨기지 않음 */
    }

    /* CE06: 완전한 프로파일·사본을 논리 commit한다. 시각/순번/AGE는 변경하지 않는다. */
    scratch.can_id = input->kind == CAN_REAR ? 0x0A0u : 0x080u;
    scratch.ide = 0; scratch.fdf = 1; scratch.brs = 1; scratch.rtr = 0;
    scratch.dlc = 11; scratch.length = 20;
    *out = scratch;
    result.status = CAN_OK;
    return result;
}
```

### 설명 분기 ↔ 의사코드 대응

| 분기 ID | 코드 위치·조건 | 자원/OUT | 반환·관측 | U4C 예약 |
|---|---|---|---|---|
| CE01 | NULL 선행 검사 | OUT 불변, I1 미호출 | CAN_INVALID_ARG | CAN-D03 |
| CE02 | kind 선행 검사 | OUT 불변, 새 schema 없음 | CAN_UNSUPPORTED | CAN-D07 |
| CE03 | 두 encoder 중 하나 호출 | scratch20만 사용 | 실제 i1/i1_called 보존 | CAN-D01, CAN-D22 |
| CE04 | 실제 I1 non-OK | scratch 폐기, OUT 불변 | CAN_CODEC_REJECT + 원 i1 | CAN-D04, CAN-D18 |
| CE05 | written 또는 패딩 불변식 실패 | scratch 폐기, OUT 불변 | CAN_CODEC_REJECT + 실제 I1_OK | CAN-D03, CAN-D04 |
| CE06 | metadata 완성·전체 commit | caller OUT에만 완성 사본 | CAN_OK, 송신 성공 비증명 | CAN-D08, CAN-D13/14/15/20/21/22 |

원 I1 검사가 BOOT!=0과 문서화된 code 범위를 확인한다. Codec가 same-UPDATE·AGE saturation을 다시 해석하거나 기능 ALLOW를 만들지 않는다. AGE 계산·TX allocation은 호출 전에 Service에서 완료하고 Port accepted 이후 원 bytes를 수정하지 않는다. busy/fail/unknown/reset은 이 함수의 상태가 아니라 상위 Transport/Port의 후속 사건이다.

<a id="fn-u4d-can-codec-decode"></a>
## FN-U4D-CAN-CODEC-DECODE — CanCodec_Decode

**원형:** `CanCodecResult CanCodec_Decode(const CanFrame *input, CanRawMessage *out);`  
[같은 함수 설명](10_BEHAVIOR.md#fn-u4d-can-codec-decode)

```c
CanCodecResult CanCodec_Decode(const CanFrame *input, CanRawMessage *out)
{
    CanCodecResult result = { CAN_INVALID_ARG, I1_OK, 0 };
    CanRawMessage scratch = {0};

    /* CD01: NULL 검사. 원 RX 사본과 OUT은 서로 다른 owner 저장소다. */
    if (input == NULL || out == NULL) return result;

    /* CD02: standard11/IDE 원 값을 검사하고 숫자 보정을 하지 않는다. */
    if (input->can_id > 0x7FFu || input->ide != 0) {
        result.status = CAN_PROFILE_REJECT;
        return result;
    }

    /* CD03: 두 ID만 지원한다. 다른 CAN 메시지를 새 schema로 추정하지 않는다. */
    if (input->can_id != 0x0A0u && input->can_id != 0x080u) {
        result.status = CAN_UNSUPPORTED;
        return result;
    }

    /* CD04: bool 정규화 없이 0/1/1/0을 원 metadata 그대로 검사한다. */
    if (input->fdf != 1 || input->brs != 1 || input->rtr != 0) {
        result.status = CAN_PROFILE_REJECT;
        return result;
    }

    /* CD05: payload/B19를 읽기 전에 실제 byte 길이와 DLC code를 검사한다. */
    if (input->dlc != 11 || input->length != 20) {
        result.status = CAN_PROFILE_REJECT;
        return result;
    }

    /* CD06: ID로 tag를 선택하고 실제 I1 decoder가 원 fields/패딩을 검사한다. */
    result.i1_called = 1;
    if (input->can_id == 0x0A0u) {
        scratch.kind = CAN_REAR;
        result.i1 = i1_cis_rear_decode(input->can_id, input->dlc,
            input->bytes, input->length, &scratch.fields.rear);
    } else {
        scratch.kind = CAN_PERMISSION;
        result.i1 = i1_domain_permission_decode(input->can_id, input->dlc,
            input->bytes, input->length, &scratch.fields.permission);
    }

    /* CD07: I1 reject는 원 오류 그대로. fake DTO/callback/liveness를 만들지 않는다. */
    if (result.i1 != I1_OK) {
        result.status = CAN_CODEC_REJECT;
        return result; /* scratch 폐기, OUT/원 RX 사본 불변 */
    }

    /* CD08: 정상 구조의 열화 자료도 원 값·품질·식별자·AGE 그대로 전체 commit한다. */
    *out = scratch;
    result.status = CAN_OK;
    return result;
}
```

### 설명 분기 ↔ 의사코드 대응

| 분기 ID | 코드 위치·조건 | 자원/OUT | 반환·관측 | U4C 예약 |
|---|---|---|---|---|
| CD01 | NULL 선행 검사 | OUT 불변, I1 미호출 | CAN_INVALID_ARG | CAN-D03 |
| CD02 | ID/IDE 검사 | OUT 불변 | CAN_PROFILE_REJECT | CAN-D03 |
| CD03 | 지원 ID 검사 | OUT 불변 | CAN_UNSUPPORTED | CAN-D03, CAN-D07 |
| CD04 | FDF/BRS/RTR 검사 | OUT 불변, byte 접근 없음 | CAN_PROFILE_REJECT | CAN-D03 |
| CD05 | DLC/length 검사 | OUT 불변, B19 접근 없음 | CAN_PROFILE_REJECT | CAN-D03 |
| CD06 | 실제 I1 decoder 호출 | scratch union에만 수용 | 실제 i1/i1_called 보존 | CAN-D04, CAN-D22 |
| CD07 | 실제 I1 non-OK | scratch 폐기, OUT 불변 | CAN_CODEC_REJECT + 원 i1 | CAN-D04, CAN-D18 |
| CD08 | 전체 scratch commit | caller DTO만 갱신 | CAN_OK, 기능·freshness 비증명 | CAN-D12/13/14/15/18/20/21/22 |

caller는 `CanRxRecord`를 보존한 상태에서 성공 OUT과 함께 Service에 인계한다. `captured_at/backend_generation`을 Codec가 반환 DTO에 붙이지 않는 것은 다른 노드 시계 차감·원 시각 초기화를 허용하지 않는다. CAN-D03은 OUT 가드가 불변인지, CAN-D04/18은 구조 reject와 정상 열화 성공이 다른지, CAN-D13~15는 counter/AGE 원 값이 불변인지, CAN-D20/21은 새 단위 변환·ALLOW가 없는지, CAN-D22는 두 owner의 별도 저장소가 섞이지 않는지를 U4C에서 관측한다. 이번 단계의 새로운 컴파일·Mock 실행·보드 시험은 NOT_EXECUTED다.
