#include "can_core_private.h"

CanCodecResult CanCodec_Encode(const CanRawMessage *input, CanFrame *out)
{
    CanCodecResult result = { CAN_INVALID_ARG, I1_OK, 0 };
    CanFrame scratch = {0};
    size_t written = 0;

    CAN_BRANCH("CE01"); /* 전체 객체의 NULL만 검사한다. nonalias/안정성은 caller 소유 계약이다. */
    if (input == NULL || out == NULL || can_overlap(input,sizeof(*input),out,sizeof(*out))) return result;

    CAN_BRANCH("CE02"); /* 두 원 profile만 지원한다. future discrete schema를 만들지 않는다. */
    if (input->kind != CAN_REAR && input->kind != CAN_PERMISSION) {
        result.status = CAN_UNSUPPORTED;
        return result;
    }

    CAN_BRANCH("CE03"); /* 실제 I1 함수 하나를 scratch20에 호출하고 실제 결과를 보존한다. */
    result.i1_called = 1;
    if (input->kind == CAN_REAR) {
        result.i1 = i1_cis_rear_encode(&input->fields.rear,
            scratch.bytes, 20, &written);
    } else {
        result.i1 = i1_domain_permission_encode(&input->fields.permission,
            scratch.bytes, 20, &written);
    }

    CAN_BRANCH("CE04"); /* 원 Code/Boot/인자/용량 오류와 예상 밖 I1 오류도 fail closed다. */
    if (result.i1 != I1_OK) {
        result.status = CAN_CODEC_REJECT;
        return result; /* scratch 폐기, OUT/IN 불변 */
    }

    CAN_BRANCH("CE05"); /* I1_OK만으로 완료하지 않고 실제20B/패딩 불변식을 확인한다. */
    if (written != 20 || scratch.bytes[19] != 0) {
        result.status = CAN_CODEC_REJECT;
        return result; /* 실제 I1_OK는 유지, 자체 불변식 실패를 성공으로 숨기지 않음 */
    }

    CAN_BRANCH("CE06"); /* 완전한 프로파일·사본을 논리 commit한다. 시각/순번/AGE는 변경하지 않는다. */
    scratch.can_id = input->kind == CAN_REAR ? 0x0A0u : 0x080u;
    scratch.ide = 0; scratch.fdf = 1; scratch.brs = 1; scratch.rtr = 0;
    scratch.dlc = 11; scratch.length = 20;
    *out = scratch;
    result.status = CAN_OK;
    return result;
}

CanCodecResult CanCodec_Decode(const CanFrame *input, CanRawMessage *out)
{
    CanCodecResult result = { CAN_INVALID_ARG, I1_OK, 0 };
    CanRawMessage scratch = {0};

    CAN_BRANCH("CD01"); /* NULL 검사. 원 RX 사본과 OUT은 서로 다른 owner 저장소다. */
    if (input == NULL || out == NULL || can_overlap(input,sizeof(*input),out,sizeof(*out))) return result;

    CAN_BRANCH("CD02"); /* standard11/IDE 원 값을 검사하고 숫자 보정을 하지 않는다. */
    if (input->can_id > 0x7FFu || input->ide != 0) {
        result.status = CAN_PROFILE_REJECT;
        return result;
    }

    CAN_BRANCH("CD03"); /* 두 ID만 지원한다. 다른 CAN 메시지를 새 schema로 추정하지 않는다. */
    if (input->can_id != 0x0A0u && input->can_id != 0x080u) {
        result.status = CAN_UNSUPPORTED;
        return result;
    }

    CAN_BRANCH("CD04"); /* bool 정규화 없이 0/1/1/0을 원 metadata 그대로 검사한다. */
    if (input->fdf != 1 || input->brs != 1 || input->rtr != 0) {
        result.status = CAN_PROFILE_REJECT;
        return result;
    }

    CAN_BRANCH("CD05"); /* payload/B19를 읽기 전에 실제 byte 길이와 DLC code를 검사한다. */
    if (input->dlc != 11 || input->length != 20) {
        result.status = CAN_PROFILE_REJECT;
        return result;
    }

    CAN_BRANCH("CD06"); /* ID로 tag를 선택하고 실제 I1 decoder가 원 fields/패딩을 검사한다. */
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

    CAN_BRANCH("CD07"); /* I1 reject는 원 오류 그대로. fake DTO/callback/liveness를 만들지 않는다. */
    if (result.i1 != I1_OK) {
        result.status = CAN_CODEC_REJECT;
        return result; /* scratch 폐기, OUT/원 RX 사본 불변 */
    }

    CAN_BRANCH("CD08"); /* 정상 구조의 열화 자료도 원 값·품질·식별자·AGE 그대로 전체 commit한다. */
    *out = scratch;
    result.status = CAN_OK;
    return result;
}
