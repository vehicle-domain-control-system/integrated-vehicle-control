# Flow_Process — 한국어 C 형태 의사코드

[상세 설명](../functions/Flow_Process.md) · [FLOW](../00_MODULE_OVERVIEW.md) · [VSS 지도](../../../01_VSS_MAP.md) · CORE-01

> GUIDE · D3-1 SELF_CHECKED / D3_1_REVIEW_PENDING · TEST_NOT_RUN / BINDING_TBD / HW_UNVERIFIED.

<a id="contract"></a>
## 최신 논리 계약

```text
VssFlowProgress Flow_Process(
    VALUE VssProcessingOpportunity opportunity,
    IN const VssTimeEvidence *time);
```

[R6](../../../../design/70_C_INTERFACE/00_HEADER_OWNERSHIP_MAP.md#r6-flow-process)의 enum 값+time borrow다. 아래 한글 연산은 기존 owner 보호/읽기/조율의 설명용이며 새 helper·Getter·API·집합 DTO·Queue/Task가 아니다. 다른 Core 호출의 한글 인자군은 **해당 R6 독립 typed 인자/count/OUT 범위를 생략해 읽는 의사 표기**이며 ABI 인자로 그대로 구현하지 않는다. 한 번의 처리 기회 안의 bounded 후속이고 완료까지 무한 재시도하지 않는다. [과거 R2/수명](../functions/Flow_Process.md#contract)과 [Host/Gate](../functions/Flow_Process.md#feasibility)를 따른다.

<a id="procedure"></a>
## F1~F9 — 정규 조율과 재수집

```c
VssFlowProgress Flow_Process(
    /* VALUE */ VssProcessingOpportunity opportunity,
    /* IN */ const VssTimeEvidence *time)
{
    /* F1: opportunity는 알림 사실이 아닌 기존 scalar. time 공간 선검증. */
    if (!기존_opportunity_enum이_유효) {
        기존_조율문맥에_부적합_기회_관측을_남김;
        return VSS_FLOW_FOLLOW_UP_REQUIRED;
    }
    if (time == NULL || time_공간이_유효하지_않음) {
        /* 유효하지 않은 borrow를 다른 Core에 전달하거나 역참조하지 않음.
           기존 보호 사실/미처리는 owner에 그대로 두고 안전한 기회 재수집. */
        return VSS_FLOW_RECOLLECT_OBSERVATIONS;
    }
    if (!현재_시간_연속성_원문맥_비교가_충분) {
        /* 확인 가능한 독립 owner 사실 보호/적용은 계속 가능.
           시간 의존 expiry/START/정상보고는 부족 근거를 유지한다. */
        시간부족과_재수집_필요를_기존_조율문맥에_남김;
    }
    if (opportunity == VSS_OPPORTUNITY_INITIAL) {
        /* F2: 동결 startup slice. 새 INIT 함수/Session/Task를 만들지 않음. */
        기존_초기화_완료여부와_불변_자원근거를_확인;
        기존_ASSET_가용과_Session없는_STREAM_장치진행을_연결;
        /* 준비 접수는 actual 아님. UNASSESSED는 초기 정상 미평가.
           후속마다 중복 prepare/Session/기존 owner 초기화를 하지 않는다. */
    }

    /* F3: INPUT 도착은 기존 경계에서 읽는다. public 인자에 payload 추가 없음. */
    Input_Process(기존_INPUT_경계의_optional_보호_input_또는_NULL,
                  time, INPUT_발견진단의_기존_독립_OUT범위);
    /* TIME_CHECK/RX 없음도 호출. EVENT/CLEAR 합성 없음.
       실제 application/diagnostics는 각각 보호하여 다음 owner에 연결. */
    AudioStream_Advance(time, 기존_STREAM_R6_독립_typed_OUT범위들);
    /* 원 요청/출력/cleanup/device/진단/Performed/Verification을 독립 보호.
       알림/void/written은 실제 출력·복구 완료가 아님. */
    Playback_Advance(보호한_기존_R6_typed_하위사실과_최신조건_시간_OUT범위들);
    /* PB 내부 기존 통지 단계가 CORE-04로 원 최초/final facts를 적용.
       NOT_APPLIED는 원 PB fact/통지 의무 보호. Flow의 raw 판정 없음. */
    반영완료_최신_singular_PLAYBACK_관측을_수집;
    Store_AdvanceDeadlines(time, 보호한_최신_VssPlaybackObservation_참조);
    Health_Evaluate(보호한_원진단_복구수행_별도검증과_현재_적용근거);

    /* F4: 현재 복구 permission과 old 위험 종료만 기존 공개 경계로 조율. */
    if (현재_Health_복구허용_관측이_있음) {
        permission_instance_revision_target_configuration_action_basis를_대조;
        if (현재_허용_조건이_변했거나_원자료가_부족) {
            복구_재확인_필요를_남기고_새_복구효력을_채택하지_않음;
        } else if (old_실제_provider_read_PCM_device_위험참조가_남음) {
            기존_PB_종료요구와_STREAM_후속정리_진행을_연결;
            /* 임의 timeout/IRQ disable로 위험 종료 확인하지 않음. */
        } else if (효력직전_현재_permission_최신조건_재검사를_통과) {
            기존_STREAM_Recovery_lane의_동기_요구와_후속을_연결;
            /* 실제 Performed+별도 Verification의 현재 pair를 HEALTH에 적용.
               Session 없는 복구 허용. FLOW가 Fault clear/READY를 쓰지 않음. */
        }
        /* 독립 old 불변 HAL 기록의 저장 수명과 실제 위험 종료는 별개. */
    }

    /* F5: owner별 반영 완료 읽기. 여기에는 숨은 상태 적용/getter 변경 없음. */
    VssReadBasis basis;
    STORE_후보_PB_관측_HEALTH_제한_ASSET_device_cleanup을_읽기전용_수집;
    for (이번_평가에_필요한_각_owner_stamp) {
        owner_revision_appliedThrough와_필요_인과선행_사실을_확인;
        if (관련_사실_미반영_또는_notification_pending_시간비교_부족 ||
            이번_time의_평가에_필요한_연속성_비교가_부족) {
            basis.validity = VSS_READ_INCOMPLETE;
            관련_owner_후속적용과_재수집_의무를_보호;
            return VSS_FLOW_RECOLLECT_OBSERVATIONS;
        }
        if (수집_뒤_관련_owner_원본_revision이_변경됨) {
            basis.validity = VSS_READ_CHANGED;
            return VSS_FLOW_RECOLLECT_OBSERVATIONS;
        }
    }
    /* owner revision 전역 숫자 일치 조건 아님. 필요한 사실 범위만 비교. */
    basis.validity = VSS_READ_VALID;
    basis.time = 평가수명으로_보호한_이번_VssTimeEvidence_값;

    /* F6: 원문 R6 typed 관측/count를 각각 전달. 새 통합 SelectionView 없음. */
    Select_Choose(기존_후보_관측범위, 최신_pb_참조, 현재_제한범위,
                  ASSET_가용범위, device_범위, cleanup_범위, &basis);
    if (선택_뒤_채택전_관련_관측_basis가_변했거나_부족) {
        return VSS_FLOW_RECOLLECT_OBSERVATIONS;
    }
    Playback_RequestTransition(기존_선택판단_lane과_최신_R6_실행조건_OUT범위);
    /* 선택됨 != prepared != accepted/enabled != actual.
       PB는 채택 때 owner/원시간/제한/자원을 재검사하고 필요한 plan을 보호.
       FLOW가 WAIT/ADOPTED/KEPT를 실제 성공 또는 old 종료 완료로 바꾸지 않음. */

    /* F7: 이번 예산 안에 즉시 발생한 원 결과도 기존 owner 적용으로 연결. */
    if (이번_예산에서_진행가능한_즉시_원결과가_있음) {
        Playback_Advance(원_즉시결과와_최신_R6_조건_시간_OUT범위들);
    } else {
        미반영_즉시결과와_필요한_후속을_원_owner에_보호;
    }
    Health_Evaluate(새로_발견한_원진단과_현재_수행검증_적용근거);

    /* F8: 선택 basis를 그대로 보고에 재사용하지 않는다. */
    VssReadBasis reportBasis;
    보고용_각_owner_반영완료_stamp_인과_시간_관측을_새로_수집;
    if (보고에_관련된_미반영_변경_시간_자료부족이_있음) {
        새_정상_Status를_만들거나_전달하지_않음;
        return VSS_FLOW_RECOLLECT_OBSERVATIONS;
    }
    reportBasis.validity = VSS_READ_VALID;
    reportBasis.time = 평가수명으로_보호한_이번_VssTimeEvidence_값;
    Health_BuildStatus(적용완료_R6_typed_관측들, &reportBasis, 기존_snapshot_OUT);
    if (HEALTH가_CREATED했고_유효한_snapshot_보호를_완료) {
        기존_INPUT_보고연결로_불변_Status와_참조를_전달_보호;
        /* 실제 통신 소비 종료까지 필요참조 유지. CAN 송신/Domain 수용 별도. */
    } else {
        새_정상보고_합성없이_재평가_또는_미인계_후속을_유지;
    }

    /* F9: 각 owner 관측의 부족/미처리도 확인. CHANGED/void만으로 정착 아님. */
    if (원결과_적용_정리_진단_Status_전달_재평가_예산밖_처리가_남음) {
        return VSS_FLOW_FOLLOW_UP_REQUIRED;
    }
    return VSS_FLOW_SETTLED;
}
```

<a id="postconditions"></a>
## 사후조건과 문서 경계

재수집 반환 전 이미 안전하게 완료한 INPUT/STORE 사실을 되돌리지 않는다. old pending notification과 인과 부족이 풀리기 전 새 Session/START·정상 보고 확정은 없다. Rx 없다고 결과/시간/품질 진행을 생략하지 않는다. settled는 이번 조율 완료이며 실제 출력 성공 아님이다.

상세한 PB/POLICY/HEALTH 쌍은 PLANNED_NOT_WRITTEN이다. 이 문서의 calls는 아래 동결 원문에 근거하며 D3-2/3 구현 착수·새 API 정의가 아니다. 초기 HW/IF 미정은 실제 adapter/물리 수행 검증만 차단한다. 네 문서쌍의 독립 SW 순서/거부/재수집/보호 논리를 `return TBD`로 대체하지 않는다.

[INPUT 의사코드](../../../02_SERVICE/INPUT/pseudocode/Input_Process.md) · [STORE 사실](../../../02_SERVICE/STORE/pseudocode/Store_ApplyPlaybackFact.md) · [STORE 기한](../../../02_SERVICE/STORE/pseudocode/Store_AdvanceDeadlines.md) · [R2](../../../../design/30_FUNCTIONS/10_FLOW_FUNCTIONS.md#flow-process) · [R3 basis](../../../../design/40_DATA/30_SELECTION_DATA.md#vssreadbasis) · [R4 coherent](../../../../design/50_CONTRACTS/30_TIMING_FRESHNESS.md#coherent-read) · [R5 정규 순서](../../../../design/60_PSEUDOCODE/20_INPUT_TO_SELECTION.md#flow-process) · [R5 startup](../../../../design/60_PSEUDOCODE/10_STARTUP_FLOW.md#flow-startup) · [R5 복구](../../../../design/60_PSEUDOCODE/60_FAULT_RECOVERY_FLOW.md#recovery-flow) · [R6](../../../../design/70_C_INTERFACE/00_HEADER_OWNERSHIP_MAP.md#r6-flow-process)
