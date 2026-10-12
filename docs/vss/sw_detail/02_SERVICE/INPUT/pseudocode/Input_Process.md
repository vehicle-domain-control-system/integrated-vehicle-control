# Input_Process — 한국어 C 형태 의사코드

[상세 설명](../functions/Input_Process.md) · [INPUT](../00_MODULE_OVERVIEW.md) · [VSS 지도](../../../01_VSS_MAP.md) · CORE-02

> GUIDE · D3-1 SELF_CHECKED / D3_1_REVIEW_PENDING · TEST_NOT_RUN / BINDING_TBD / HW_UNVERIFIED. 실제 C/Host 실행 결과가 아니다.

<a id="contract"></a>
## 최신 논리 계약

```text
VssInputApplication Input_Process(
    OPTIONAL IN const VssInputEvidence *input,
    IN const VssTimeEvidence *time,
    OUT VssDiagnosticEvidence diagnostics[capacity -> written]);
```

[R6](../../../../design/70_C_INTERFACE/00_HEADER_OWNERSHIP_MAP.md#r6-input-process) 기호는 전달 방향 주석이다. 아래 한글 조건/연산, `diagnostics.written/capacity`, bounded 반복은 설명용이며 새 helper·필드·collection·실제 ABI가 아니다. `VssInputApplication` 세 필드와 기존 enum만 실제 논리 자료에 해당한다. 상세 설명의 [원형/과거 R2 차이](../functions/Input_Process.md#contract), [후속 Host·Gate](../functions/Input_Process.md#feasibility)를 함께 읽는다.

<a id="procedure"></a>
## I1~I7 — 검증, 문맥 적용, 한 typed lane

```c
VssInputApplication Input_Process(
    /* OPTIONAL IN */ const VssInputEvidence *input,
    /* IN */ const VssTimeEvidence *time,
    /* OUT */ VssDiagnosticEvidence diagnostics[/* capacity -> written */])
{
    VssInputApplication result;
    result.validation = VSS_VALIDATION_NO_CHANGE;
    result.contextApplied = false;
    result.storeAdmission = VSS_STORE_NOT_SUBMITTED;

    /* I1: INPUT 원본 초기화가 아니다. 진단 공간만 먼저 독립 검증. */
    if (진단군_주소_capacity_전달영역_written_저장공간을_안전_확인했고
        그_뒤_필요한_capacity_값과_출력범위_검증을_완료) {
        diagnostics.written = 0; /* 검증된 이 군만 초기화하는 의사 접근. */
    } else {
        /* 무효 written/capacity 메모리 읽기·쓰기·초기화 금지.
           발견 진단은 기존 INPUT owner 미인계 보호·손실 관측/후속.
           독립적으로 안전한 입력 검증/STORE 전달은 계속 가능. */
    }

    /* I2: NULL은 새 RX 없음. time 부족은 정밀 시각 합성의 이유가 아님. */
    if (time == NULL || 시간_저장공간_또는_연속성_원문맥_비교가_부족) {
        시간_의존_최신성_상실시각_판단을_불확실로_유지;
        실제_발견한_부족_원사실을_기존_INPUT_진단_문맥에_보호;
    }
    if (input == NULL) {
        /* I3: RX 없는 기존 문맥 점검. 아래 lane은 QUALITY 또는 미호출. */
        기존_보호_INPUT_문맥으로_현재_품질_변화를_점검;
        if (현재_적용_가능한_INVALID_또는_STALE_변화가_확인됨) {
            원_basis와_최초_lossAt_근거를_보호;
            if (필요한_문맥과_참조_보호를_완료) {
                INPUT_현재_품질_문맥을_반영하고_반영완료_관측을_공개;
                result.validation = VSS_VALIDATION_QUALITY_CHANGE;
                result.contextApplied = true;
                /* 기존 VssQualityChange를 동기 borrow. EVENT/CLEAR 합성 없음. */
                result.storeAdmission = Store_ApplyInput(
                    NULL, NULL, 검증된_현재_품질변화의_보호_참조);
            } else {
                result.validation = VSS_VALIDATION_REJECTED;
                보호실패_원사실을_진단_대상으로_남김;
            }
        }
        /* 변화 없음/시간 부족이면 기존 문맥 보존, NOT_SUBMITTED 유지. */
    } else {
        /* I4: 최신 적용 가능성은 quality 쓰기보다 먼저 검사. */
        if (입력_및_Meta_저장공간이_부적합) {
            result.validation = VSS_VALIDATION_REJECTED;
            /* 무효 입력/Meta 내부는 읽지 않고 기존 문맥을 보존. */
            실제_검증실패_원사실만_진단_대상으로_보호;
        } else if (WINDOW_PRODUCT_연동_입력임) {
            result.validation = VSS_VALIDATION_REJECTED;
            /* 구현 보류 — 설계 유지. TEST를 PRODUCT로 재분류하지 않음. */
            보류된_원출처_입력_관측만_유지;
        } else if (값_Meta_결합이_불충분 ||
                   원_source_authority_generation_ordering을_확인할_수_없음 ||
                   과거_또는_순서역전_자료라_정상_최신_문맥에_적용_불가 ||
                   원_출처_또는_시간_연속성_근거가_부적합) {
            /* I4-QE: 부적합 정상입력과 독립적인 현재 품질 근거를 구분. */
            if (현재_보호_context의_대상_PRODUCT_TEST_격리_출처_원시간에_대해
                적용가능한_최신_INVALID_또는_STALE_상실근거를_별도로_검증) {
                /* 옛 INVALID/값/Meta로 증명하지 않음. 독립 검증된 근거만 사용. */
                if (같은_품질상실의_반복이고_품질_검증문맥의_적용변화가_없음) {
                    /* NO_CHANGE/NOT_SUBMITTED. 최초 상실/Hold 연장 없음. */
                } else if (독립_최신상실의_basis_최초_lossAt와_필요참조를_보호_완료) {
                    INPUT_기존_출처_appliedMeta_generation_ordering을_유지하고
                    그_독립근거의_현재_품질_검증문맥만_반영완료;
                    result.validation = VSS_VALIDATION_QUALITY_CHANGE;
                    result.contextApplied = true;
                    result.storeAdmission = Store_ApplyInput(
                        NULL, NULL, 독립_검증된_현재_품질변화의_보호_참조);
                    /* EVENT/STATE/CLEAR·새 occurrence/Meta/age 합성 없음. */
                } else {
                    result.validation = VSS_VALIDATION_REJECTED;
                    품질근거_보호실패를_원문맥에_남김;
                }
            } else {
                result.validation = VSS_VALIDATION_REJECTED;
                /* 독립 최신 근거 없음: NOT_SUBMITTED·기존 문맥/STORE 불변. */
                실제_검증실패_원사실만_진단_대상으로_보호;
            }
        } else {
            /* 격리 TEST도 출처/세대/순서/원시간 보호를 그대로 적용한다. */
            원본_품질_값조합_시간_유효기한과_현재_적용가능성을_검증;
            if (최신_적용가능한_품질상실_INVALID_또는_STALE이고_현재_품질변화가_있음) {
                /* I5-Q: 값에 CLEAR를 채우지 않음. 반복 lossAt 재기산 없음. */
                원_basis_최초상실_시간문맥과_필요한_참조를_보호;
                if (필요한_보호를_완료) {
                    INPUT_출처_순서_품질_문맥을_반영완료;
                    result.validation = VSS_VALIDATION_QUALITY_CHANGE;
                    result.contextApplied = true;
                    result.storeAdmission = Store_ApplyInput(
                        NULL, NULL, 검증된_현재_품질변화의_보호_참조);
                } else {
                    result.validation = VSS_VALIDATION_REJECTED;
                    보호실패_원사실을_진단_대상으로_남김;
                }
            } else if (같은_품질상실의_반복이고_적용_변화가_없음) {
                /* NO_CHANGE/NOT_SUBMITTED. 최초 lossAt/Hold 연장 없음. */
            } else if (정상_사건_또는_상태의_현재_유효성을_확인했고
                       필요한_원Meta_내부참조_문맥_보호를_완료) {
                /* I5-V: Domain 확정 occurrence / 허용 Stateful 조합만 사용. */
                INPUT_출처_generation_ordering_품질_문맥을_먼저_반영완료;
                result.validation = VSS_VALIDATION_APPLICABLE;
                result.contextApplied = true;
                if (검증된_OneShot_EVENT) {
                    result.storeAdmission = Store_ApplyInput(
                        검증된_원사건의_보호_참조, NULL, NULL);
                } else {
                    result.storeAdmission = Store_ApplyInput(
                        NULL, 검증된_현재상태의_보호_참조, NULL);
                }
                /* I6: STORE REJECTED여도 contextApplied/세대/순서 rollback 없음.
                   result.storeAdmission은 실제 callee 결과, 새 ACCEPTED 합성 없음. */
            } else if (현재_문맥의_적용_변화가_없음) {
                /* NO_CHANGE/NOT_SUBMITTED; 시간 부족을 정상 fresh로 채우지 않음. */
            } else {
                result.validation = VSS_VALIDATION_REJECTED;
                실제_부적합_또는_보호부족_원사실을_진단_대상으로_남김;
            }
        }
    }

    /* I7: 이번 기존 처리 예산 내 INPUT 발견 진단만. 새로운 Queue 없음. */
    for (각_보호된_미인계_INPUT_진단_중_이번_기회에_다룰_범위) {
        if (진단군의_선행_검증_전체가_유효 &&
            diagnostics.written < diagnostics.capacity &&
            해당_인계의_원근거와_범위가_아직_유효 &&
            producer_cause_target_원시점과_필요참조를_출력수명까지_보호_가능) {
            해당_VssDiagnosticEvidence를_검증된_다음_출력위치에_기록_시도;
            if (해당_기록이_실제로_성공했고_필요참조의_출력수명_보호도_완료) {
                diagnostics.written = diagnostics.written + 1;
                /* 성공한 이 건만 유효 OUT 수에 포함. 인계/소비 보호 전 해제 금지. */
            } else {
                해당_진단을_INPUT_미인계로_유지하고_후속_관측에_남김;
                /* 부분기록/참조 보호 실패는 written 증가·유효 출력 공개 없음.
                   원본 보호도 불가능한 실제 손실은 별도 loss 근거로 보존. */
            }
        } else {
            해당_진단을_INPUT_미인계로_유지하고_후속_관측에_남김;
            /* 보호 불능에 따른 실제 손실은 loss 근거를 남기며 충분성 차단.
               capacity=0/가득 참/무효 공간을 성공 전달로 주장하지 않음. */
        }
    }
    return result;
}
```

<a id="postconditions"></a>
## 경로별 사후조건

NULL은 QUALITY 또는 미호출, 과거/부적합 정상 자료는 미적용이다. 독립 검증된 최신 상실 예외만 현재 품질/검증 문맥과 CORE-03 quality lane에 적용하며 옛 값·Meta·출처/순서를 덮지 않는다. 그 근거가 없으면 REJECTED/NOT_SUBMITTED로 기존 문맥을 보존한다. 정상 EVENT/STATE는 INPUT 반영 후 CORE-03 한 lane이다. 입력 공간 자체가 무효이면 내부 값·Meta를 읽지 않는 short-circuit 조건이다. INPUT 보호 실패는 신규 적용만 거부하고 옛 문맥을 보존한다. OUT 유효성 조건은 I1 전체를 뜻하며 무효 군의 count/capacity를 읽지 않는다. written은 검증 후 0, 실제 기록·참조 보호 완료 후에만 증가한다.

result 세 필드는 호출 결과이고 diagnostics.written은 보호된 원 진단 수다. validation과 written 모두 실제 출력 성공을 뜻하지 않는다. 품질 상실·STORE REJECTED·진단 미인계는 각 owner의 상태/후속으로 드러난다. Store_ApplyInput은 [D2 승인 의사코드](../../STORE/pseudocode/Store_ApplyInput.md) 그대로 연결한다.

[R2 검증 경계](../../../../design/30_FUNCTIONS/20_INPUT_STORE_FUNCTIONS.md#input-process) · [R3 반환/문맥](../../../../design/40_DATA/10_INPUT_DATA.md#vssinputapplication) · [R4 참조 보호](../../../../design/50_CONTRACTS/10_OWNERSHIP_LIFETIME.md#borrow-and-protect) · [R5 원 흐름](../../../../design/60_PSEUDOCODE/20_INPUT_TO_SELECTION.md#input-process) · [R6](../../../../design/70_C_INTERFACE/00_HEADER_OWNERSHIP_MAP.md#r6-input-process)
