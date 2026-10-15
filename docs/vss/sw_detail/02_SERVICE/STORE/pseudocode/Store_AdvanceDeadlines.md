# Store_AdvanceDeadlines — 한국어 C 형태 의사코드

[상세 설명](../functions/Store_AdvanceDeadlines.md) · [STORE](../00_MODULE_OVERVIEW.md) · [VSS 지도](../../../01_VSS_MAP.md) · CORE-05

> GUIDE · TEST_NOT_RUN / BINDING_TBD / HW_UNVERIFIED.

<a id="contract"></a>
## singular 관측의 R6 계약

```text
VssStoreProgress Store_AdvanceDeadlines(
    IN const VssTimeEvidence *time,
    IN const VssPlaybackObservation *playback);
```

[R6](../../../../design/70_C_INTERFACE/00_HEADER_OWNERSHIP_MAP.md#r6-store-advancedeadlines) borrow 주석을 C 형태로 읽는다. 실제 C ABI는 TBD이며 compile-ready C가 아니다. 한글 의사 조건·`변경/부족/미처리`와 반복은 함수 내부 판단 설명이며 새 enum·저장 필드·Queue·Timer·일괄 transaction framework가 아니다. `record/state/rear`는 기존 STORE owner 원본이다. [과거 R2 차이/반환 우선순위](../functions/Store_AdvanceDeadlines.md#contract)와 [Host/Gate](../functions/Store_AdvanceDeadlines.md#feasibility)를 따른다.

<a id="procedure"></a>
## S5-1~S5-6 — 관측 확인 전 expiry 금지

```c
VssStoreProgress Store_AdvanceDeadlines(
    /* IN */ const VssTimeEvidence *time,
    /* IN */ const VssPlaybackObservation *playback)
{
    /* S5-1: 인자 안전성, 최신 적용 stamp, 미통지 사실을 만료보다 먼저. */
    if (playback == NULL || playback_공간_내부참조가_유효하지_않음) {
        return VSS_STORE_RECONFIRM_REQUIRED;
    }
    if (!관련_pb_stamp_owner_revision_appliedThrough_인과가_최신_충분 ||
        playback->notificationPending) {
        기존_ledger와_후보의_부족근거를_유지;
        /* FLOW가 PB 사실 적용/재수집을 먼저 연결. STORE 역호출 없음. */
        return VSS_STORE_RECONFIRM_REQUIRED;
    }
    /* S5-2: 기존 원 시각 비교. now로 원 age/상실 시각을 합성하지 않는다. */
    if (time == NULL || time_저장공간이_유효하지_않음 ||
        !현재_시간연속성과_관련_원domain_epoch_비교가_충분) {
        원_기산과_신뢰부족_관측을_유지;
        return VSS_STORE_RECONFIRM_REQUIRED;
    }

    /* S5-3: 이번 기존 예산 안의 occurrence만. 미처리 범위는 다음 기회 보호. */
    for (이번_기회의_각_보호된_원_occurrence) {
        if (record.state != VSS_OCCURRENCE_PENDING) {
            /* Started/whole final/Expired/UncertainFinal: 최초 gate 재적용 없음. */
            continue;
        }
        if (!이_원발생에_대한_pb_후속시도와_통지_인과_연결이_충분) {
            해당_후보를_불확실로_관측하고_재확인_필요를_남김;
            continue;
        }
        if (연결된_원시도가_START_IN_FLIGHT ||
            연결된_원시도의_startKnowledge가_VSS_START_PENDING_OR_UNKNOWN) {
            /* age가 2초 이상이어도 NO_START/Expired 생성 금지. */
            해당_후속사실_적용과_재확인_필요를_유지;
            continue;
        }
        if (원_actual_확인사실이_아직_STORE에_미반영 ||
            원_uncertain_final_또는_실제_통지추적_근거가_부족) {
            해당_사실_적용과_재확인_필요를_남김;
            continue;
        }
        if (!확정_미시도_또는_전체_NO_START_미래차단_retirement_충분) {
            /* 현재 session 없음/idle/timeout만으로 과거 무출력 추정 금지. */
            해당_원발생의_부족_근거를_남김;
            continue;
        }
        원_originalMeta부터_time까지_가능한_age_구간을_비교;
        if (비교불가_또는_가능한_age구간이_2초_경계를_걸침) {
            해당_후보의_freshness_불확실성과_재확인_필요를_유지;
        } else if (원_age_가능구간_전체가_잠정_2초_이상) {
            if (반영직전_관련_pb_및_STORE_원문맥이_그대로_적용가능) {
                record.state = VSS_OCCURRENCE_EXPIRED;
                원_identity_originalMeta_age와_replay_추적보호를_유지;
                record.revision과_변경완료_관측을_공개;
                이번_안전한_변경반영을_기록;
            } else {
                해당_판단은_반영하지_않고_재확인_필요를_남김;
            }
        } else {
            /* 전체 <2초: 이 gate로 아직 만료 아님. 새 age/Pending 생성 없음.
               정확히 2초 첫 actual은 허용 안 됨. Network 500ms min/AND 없음. */
        }
    }

    /* S5-4: 기존 Stateful Hold/Rear 후보만. INPUT 품질 판정 대체 안 함. */
    for (이번_기회의_각_관련_Stateful_및_Rear_보호문맥) {
        if (유효_CLEAR_또는_Rear_DISABLED_또는_이미_만료로_후보가_없음) {
            /* 품질 상실/ENABLED만으로 옛 후보/Hold 부활 금지. */
            continue;
        }
        if (현재_품질상실_또는_기한상실의_적용가능성_재확인이_필요) {
            기존_INPUT_STORE_품질반영과_재수집_필요를_관측;
            continue;
        }
        if (기존_경고의_firstLossAt_보호근거가_있음) {
            /* 최초 상실과 정의된 원 validUntil 중 이른 기산.
               validUntil 없음은 최초 상실 기준. 반복 INVALID/now로 연장 금지. */
            if (!동일_최초사실의_holdAnchor_시간비교가_충분 ||
                !기존_Hold_정책기간이_확정됨) {
                TBD_HOLD와_후보불확실성_재확인_필요를_유지;
                continue;
            }
            기존_holdAnchor와_현재시간의_가능구간_전체를_비교;
            if (Hold_만료경계를_걸침_또는_연속성_비교가_부족) {
                해당_후보_재확인_필요를_남김;
            } else if (기존_Hold_만료가_전체구간에서_확인됨) {
                if (반영직전_관련_owner_문맥이_그대로_적용가능) {
                    해당_Hold_Rear_파생후보만_제거;
                    /* lastValidValue/Meta/quality 그대로. CLEAR/VALID 합성 없음. */
                    해당_변경_revision과_반영완료_관측을_공개;
                    이번_안전한_변경반영을_기록;
                } else {
                    재확인_필요를_남김;
                }
            }
        }
    }

    /* S5-5: R5 반환 순서. CHANGED가 다른 발생의 부족 해소를 뜻하지 않음. */
    if (이번_안전하게_반영완료한_변경이_있음) {
        return VSS_STORE_CHANGED;
    } else if (시간_pb_Hold_원발생_근거부족으로_재확인이_필요) {
        return VSS_STORE_RECONFIRM_REQUIRED;
    } else {
        return VSS_STORE_STABLE;
    }
    /* S5-6: 남은 예산 범위·unknown·미반영 facts·Hold TBD는 owner 관측과
       기존 후속 처리에 보존. stable도 미래 타이머/처리기회 제거 명령 아님. */
}
```

<a id="postconditions"></a>
## 사후조건

STABLE/CHANGED/RECONFIRM 모두 INPUT authority·원 Meta/age·PLAYBACK 시작 지식·HEALTH Fault를 임의 변경하지 않는다. Expired라도 replay/참조/미반영 통지의 안전 회수 조건이 별도이므로 ledger 삭제가 아니다. time 비교 부족을 now로 해결하지 않는다. 정상 Started의 후속 cue에는 `<2초` 최초 gate를 반복하지 않는다.

FLOW는 [새 관측 재수집](../../../01_APP/FLOW/pseudocode/Flow_Process.md#procedure)으로 부족/미처리 의무까지 확인한다. actual/final 통지는 [CORE-04](Store_ApplyPlaybackFact.md#procedure)가 담당한다. Store_ApplyInput의 [D2 품질·Hold](Store_ApplyInput.md#procedure)를 재정의하지 않는다.

[R2](../../../../design/30_FUNCTIONS/20_INPUT_STORE_FUNCTIONS.md#store-advancedeadlines) · [R3 PB](../../../../design/40_DATA/40_PLAYBACK_DATA.md#vssplaybackobservation) · [R3 Stateful](../../../../design/40_DATA/20_STORE_DATA.md#vssstatefulstate) · [R4 Timing](../../../../design/50_CONTRACTS/30_TIMING_FRESHNESS.md#first-start) · [R5](../../../../design/60_PSEUDOCODE/20_INPUT_TO_SELECTION.md#store-advancedeadlines) · [R6](../../../../design/70_C_INTERFACE/00_HEADER_OWNERSHIP_MAP.md#r6-store-advancedeadlines)
