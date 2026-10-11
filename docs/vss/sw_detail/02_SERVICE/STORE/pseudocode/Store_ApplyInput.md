# Store_ApplyInput — 한국어 C 형태 의사코드

[VSS 지도](../../../01_VSS_MAP.md) / [SERVICE](../../00_LAYER_OVERVIEW.md) / [STORE](../00_MODULE_OVERVIEW.md) / CORE-03 · [상세 설명](../functions/Store_ApplyInput.md)

> GUIDE · D2_SELF_CHECKED / GD2_PENDING. 동결 설계 `0641b2c2268cf47450aeb50302638a45a7a3277f`의 적용 순서를 설명한다. 실제 C 구현·Host·보드 시험 NOT_RUN.

STORE가 검증된 입력 하나를 동기 적용하고 `VssStoreAdmission`을 반환한다. accepted는 최종까지의 추적 확보이고 actual 시작은 별도다. 한국어 조건·순회·대괄호 절차는 **설명용 의사 연산**이며 실제 변수/컨테이너/API가 아니다.

<a id="contract"></a>
## R6 논리 계약

```text
VssStoreAdmission Store_ApplyInput(
    ONE_OF const VssOneShotEvent *event,
           const VssStateUpdate *state,
           const VssQualityChange *quality);
```

`ONE_OF`는 정확히 하나 제약이며 C 키워드가 아니다. **R6 logical declarations != compile-ready C**. 아래 C 모양은 실제 Header/ABI·enum 폭·capacity를 정하지 않는다. R2의 옛 `VssValidatedInput *input` 단일 인자는 이력이며 [R6](../../../../design/70_C_INTERFACE/00_HEADER_OWNERSHIP_MAP.md#r6-store-applyinput)가 현재 논리 원형이다.

<a id="procedure"></a>
## 적용 절차

`기존_기록`·`해당_Stateful`·`Rear`는 [R3 STORE 원본](../../../../design/40_DATA/20_STORE_DATA.md#store)의 설명용 참조다. 새 전역 Context나 구현 helper 선언이 아니다. 모든 반영은 필요한 보호를 먼저 확보한 후 관측 공개/revision을 완료한다. 판정/보호 실패에서는 **기존 원본 불변**이다.

```c
VssStoreAdmission Store_ApplyInput(
    const VssOneShotEvent *event,
    const VssStateUpdate *state,
    const VssQualityChange *quality)  /* 논리 3배타 입력; C ABI TBD */
{
    // S1 — 0개 또는 다중 입력은 신규 적용 거부. NOT_SUBMITTED를 반환하지 않는다.
    if (((event != NULL) + (state != NULL) + (quality != NULL)) != 1) {
        return VSS_STORE_REJECTED;  // occurrence/Stateful/Rear/INPUT 문맥 불변.
    }

    if (event != NULL) {
        // S2 — INPUT의 검증 근거를 소비한다. source authority를 새로 판정하지 않는다.
        if (검증된_OneShot_의미_또는_원본_연결_근거가_부적합) {
            return VSS_STORE_REJECTED;
        }

        // S3 — 실제 구현이 제공하는 방식으로 기존 보호 occurrence/replay 기록을 순회.
        // 동일성은 R3/R4의 원 key와 PRODUCT/TEST·source/generation 문맥을 따른다.
        // 저장 표현/alias·reboot·wrap은 B2-R TBD; requestId/signal 하나로 비교하지 않는다.
        for (각_기존_보존_occurrence_및_replay_기록에_대해) {
            if (기존_기록이_동일_원발생임이_확인됨) {
                return VSS_STORE_DUPLICATE;  // 원 age/state/facts 유지; 새 Pending 없음.
            }
        }
        if (새_발생과_동일_발생을_구별할_근거가_부족) {
            return VSS_STORE_REJECTED;  // 현재 시간/새 key로 보충하지 않는다.
        }
        // 이미 LOCK 목표 상태여도 Domain이 검증한 새 occurrence이면 여기로 진행.

        // S4 — 수용 노출 전에 Pending→최종 통지/replay까지 보호 자원을 함께 확보.
        if (신규_identity_Meta_최종추적_보호_확보가_불가능) {
            /* [의사 연산] 일부 확보한 신규 임시 자원만 무참조 확인 범위에서 정리.
               기존 accepted/final/replay 기록 eviction 및 INPUT 순서 rollback 없음. */
            return VSS_STORE_REJECTED;
        }
        /* [의사 연산] event.occurrence/signal/meta를 STORE 추적 수명으로 보호.
           originalMeta 내부 validUntil 참조도 보호; 임시 event 주소 영구 저장 없음.
           새 record.state = VSS_OCCURRENCE_PENDING;
           firstStartFact/finalFact는 아직 없음; tracking = VSS_TRACKING_PROTECTED.
           원본 반영/revision 완료 후 후보 관측을 공개한다. */
        return VSS_STORE_ACCEPTED;  // Session 생성/출력 요청 없음.
    }

    if (state != NULL) {
        // S5 — 최신 적용 여부는 INPUT 검증 근거. 모호함을 UPDATED로 채우지 않는다.
        if (검증된_유효_STATE_허용조합_또는_해당_문맥_연결이_부적합) {
            return VSS_STORE_REJECTED;
        }
        if (갱신에_필요한_원Meta_참조보호를_확보할_수_없음) {
            return VSS_STORE_REJECTED;  // One-shot 신규 용량과 별도로 판단.
        }

        if (state->signal == VSS_SIGNAL_REAR_ACTIVATION) {
            // S6 — Activation은 단독 음향 후보가 아니다.
            if (state->value == VSS_VALUE_DISABLED) {
                /* [의사 연산] 검증된 activation/activationMeta 반영.
                   riskApplicable=false; Rear 후보/Hold 제거.
                   실제 참조 종료 뒤 Hold 보호 회수. 위험을 CLEAR로 재작성하지 않음. */
            } else if (state->value == VSS_VALUE_ENABLED) {
                /* [의사 연산] 최신 activation/Meta 반영.
                   DISABLED→ENABLED 자체로 old riskApplicable/Hold 부활 없음.
                   계속 ENABLED여도 메시지 자체는 새 위험 근거가 아니다.
                   현재 activation에 적용 가능한 위험의 별도 검증 전 새 후보 없음. */
            } else {
                return VSS_STORE_REJECTED;
            }
            /* [의사 연산] Rear 변경 반영 완료/revision 갱신. */
            return VSS_STORE_UPDATED;
        }

        // S7 — 유효 최신 Stateful. 대상별 허용 value는 R3/INPUT 근거에서 확인.
        if (state->value == VSS_VALUE_CLEAR) {
            /* [의사 연산] lastValidValue/보호된 lastValidMeta 및 VALID 품질 반영.
               해당 후보/Hold 제거; 옛 firstLossAt/holdAnchor 참조 안전 종료.
               다른 Stateful·One-shot 이력 보존. */
        } else if (state->signal == VSS_SIGNAL_REAR_RISK &&
                   (state->value == VSS_VALUE_EMERGENCY ||
                    state->value == VSS_VALUE_CAUTION)) {
            /* [의사 연산] 유효 위험 판단/Meta/VALID 품질 반영; 이전 Hold 안전 종료. */
            if (현재_Rear_Activation이_DISABLED) {
                /* 후보 생성 없음; riskApplicable 차단 유지. */
            } else if (최신_Activation과_원위험의_적용가능_결합이_확인됨) {
                /* riskApplicable=true; 확인된 현재 위험 후보만 노출.
                   EMERGENCY→CAUTION은 후속 교체 판단; TX/Session 직접 write 없음. */
            } else {
                /* 결합 불충분 보존; 적용 가능한 정상 후보 합성 없음.
                   품질/위험 원본 갱신과 후보 충분성은 다르다. */
            }
        } else if ((state->signal == VSS_SIGNAL_ANTI_PINCH ||
                    state->signal == VSS_SIGNAL_OCCUPANT_HAZARD) &&
                   state->value == VSS_VALUE_ACTIVE) {
            /* [의사 연산] lastValidValue/Meta/VALID 반영; 이전 Hold 안전 종료.
               검증된 ACTIVE 후보 노출; 반복 ACTIVE는 새 Session 요청 아님.
               Anti-Pinch PRODUCT 보류, 격리 TEST는 INPUT 검증 문맥 그대로. */
        } else {
            return VSS_STORE_REJECTED;  // 위 분기 전 어떤 상태도 먼저 수정하지 않음.
        }
        /* [의사 연산] 해당 상태·변경된 Rear 결합의 반영 완료/revision 갱신. */
        return VSS_STORE_UPDATED;
    }

    // S8 — 정확히 하나 전제에 따라 quality만 남는다.
    if (현재_적용가능한_품질변화_근거_또는_대상_연결이_부적합 ||
        정상_EVENT_CLEAR_STATE를_quality로_대신하려는_요구) {
        return VSS_STORE_REJECTED;
    }
    if (현재_품질과_필요한_최초근거의_보호를_확보할_수_없음) {
        return VSS_STORE_REJECTED;  // 기존 값/기산·INPUT 적용 문맥 불변.
    }
    /* [의사 연산] 현재 quality만 반영; lastValidValue/lastValidMeta 유지.
       INVALID/STALE/UNASSESSED는 R3 의미; NO_DATA는 미수신 의미이고 새 enum 아님. */
    if (기존_유효경고가_없음 ||
        CLEAR_DISABLED_Hold만료로_후보가_이미_제거됨) {
        /* 품질만 적용 완료/revision 갱신. 품질 상실로 새 후보/Hold 부활 없음. */
        return VSS_STORE_UPDATED;
    }

    // S9 — 최초 상실 기준을 한 번 보호한다. 한국어 조건은 설명용 의사 연산.
    if (이_경고의_최초_신뢰상실이_아직_기록되지_않음) {
        /* [의사 연산] quality.lossAt와 basis의 원 문맥/연속성을 보호한다.
           시각 미확인이어도 최초 사실과 부족 근거 보존. 처리 now로 생성하지 않음. */
        if (최초상실의_원시각이_비교가능) {
            if (원_lastValidMeta에_validUntil이_정의됨) {
                if (최초상실과_그_validUntil이_비교가능) {
                    /* holdAnchor = 두 원 경계 중 이른 값; 보호된 지속 근거. */
                } else {
                    /* 기산 확정 보류; fresh/정밀 Hold 합성 없음. */
                }
            } else {
                /* holdAnchor = 원 최초 상실 기준만; 임의 validUntil 생성 없음. */
            }
        } else {
            /* firstLossAt/기산의 부족 근거 유지; 정밀 시각이나 정상 후보 없음. */
        }
    } else {
        /* 반복 INVALID/STALE·재전달·선점·복구로 firstLossAt/holdAnchor 연장 없음.
           최초 시각이 부족했던 경우도 이번 lossAt로 재기산하지 않는다.
           동일 최초 사실의 추가 근거만 후속 보완 후보; 저장/표현은 B2-R TBD. */
    }
    /* [의사 연산] 품질 및 확보된 최초 근거 반영 완료/revision 갱신.
       Hold 만료는 Store_AdvanceDeadlines 후속; 여기서 CLEAR/VALID 합성 없음. */
    return VSS_STORE_UPDATED;
}
```

<a id="postconditions"></a>
## 종료 후 남는 책임

REJECTED/DUPLICATE에서 원 발생·원 시간·기존 상태/품질/Rear와 INPUT 문맥을 보존한다. ACCEPTED는 보호 확보와 반영 완료 이후에만 공개한다. UPDATED는 해당 상태 적용 완료이며 시간/Rear 결합 부족을 정상 후보로 숨기지 않는다. STORE는 출력 시작·종료·Fault를 생성하지 않는다.

원발생→최초 actual `<2초`는 [잠정 Timing 조건](../../../../design/50_CONTRACTS/30_TIMING_FRESHNESS.md#first-start)으로 후속 STORE/PLAYBACK에 인계한다. 이 함수에 2초/Network 500ms를 합친 수용 timeout을 만들지 않는다. identity/추적 용량·Hold 기간·epoch·직렬화의 미정은 해당 제품 연결만 제한한다. 입력 배타·동일/새 발생·거부 불변·품질/Hold·Rear fixture는 별도 SW/Host 구현 후보이며 이번 NOT_RUN이다.

<a id="references"></a>
## 상세·원문 연결

[상세 설명·소스 근거](../functions/Store_ApplyInput.md#source) · [독립 SW 후보/국소 미정](../functions/Store_ApplyInput.md#feasibility) · [R2](../../../../design/30_FUNCTIONS/20_INPUT_STORE_FUNCTIONS.md#store-applyinput) · [R3 입력](../../../../design/40_DATA/10_INPUT_DATA.md#vssqualitychange) · [R3 반환/원본](../../../../design/40_DATA/20_STORE_DATA.md#vssstoreadmission) · [R4 Ownership](../../../../design/50_CONTRACTS/10_OWNERSHIP_LIFETIME.md#borrow-and-protect) · [R4 Timing](../../../../design/50_CONTRACTS/30_TIMING_FRESHNESS.md#stateful-hold) · [R5-C1 적용](../../../../design/60_PSEUDOCODE/20_INPUT_TO_SELECTION.md#store-applyinput) · [R6](../../../../design/70_C_INTERFACE/00_HEADER_OWNERSHIP_MAP.md#r6-store-applyinput) · [R6 미정](../../../../design/70_C_INTERFACE/00_HEADER_OWNERSHIP_MAP.md#tbd-register)
