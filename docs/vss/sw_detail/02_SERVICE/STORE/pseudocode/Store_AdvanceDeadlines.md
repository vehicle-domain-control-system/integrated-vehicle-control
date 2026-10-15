# Store_AdvanceDeadlines — 한국어 C 형태 의사코드

> A1 입력 개정 2026-10-15 · [현재 Network 연결](../../../../design/90_BINDING/92_NETWORK_BINDING.md). `L=min(양수 USE_LIMIT,500ms,2000ms)`; 최초 actual에만 적용. 과거 동결/PASS는 당시 범위의 이력이며 본문 변경은 이번 A1 결정이다. 구현·시험 NOT_RUN.

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
        해당_Stateful_Hold를_UNCERTAIN_newSessionAllowed_false로_관측;
        원_기산_부족사유_revision과_보수적종료_재확인_필요를_보호;
        /* 시간 비교 부족은 확정 만료/CLEAR/물리 STOP 완료가 아님.
           변경 반영이면 CHANGED, 아니면 RECONFIRM; 어느 쪽도 부족/후속 해소가 아님. */
        return 이번_안전하게_관측변경_반영완료 ? VSS_STORE_CHANGED : VSS_STORE_RECONFIRM_REQUIRED;
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
            /* 원 L 도달/비교 부족이면 미래 시작 차단·종료 요구.
               NO_START/Expired 생성 금지. */
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
        if (비교불가_또는_가능한_age구간이_L_경계를_걸침) {
            해당_후보의_freshness_불확실성과_재확인_필요를_유지;
        } else if (record.replayAllowed가_false_또는_원_age_가능구간_전체가_L_이상) {
            if (반영직전_관련_pb_및_STORE_원문맥이_그대로_적용가능) {
                record.state = VSS_OCCURRENCE_EXPIRED;
                원_identity_originalMeta_age와_replay_추적보호를_유지;
                record.revision과_변경완료_관측을_공개;
                이번_안전한_변경반영을_기록;
            } else {
                해당_판단은_반영하지_않고_재확인_필요를_남김;
            }
        } else {
            /* 전체 <L: 이 gate로 아직 만료 아님. 새 age/Pending 생성 없음.
               정확히 L 첫 actual은 허용 안 됨. A1 L=min(양수 USE_LIMIT,500ms,2000ms). */
        }
    }

    /* S5-4: 기존 Stateful Hold/Rear 후보만. INPUT 품질 판정 대체 안 함. */
    for (이번_기회의_각_관련_Stateful_및_Rear_보호문맥) {
        if (유효_CLEAR_또는_Rear_DISABLED_activation상실_riskFence위반_또는_이미_확정만료로_후보가_없음) {
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
            if (!원출처_승인세대_동일최초사실_holdAnchor_단위_domain_epoch_wrap_구간비교가_충분) {
                해당_후보를_UNCERTAIN_newSessionAllowed_false로_관측;
                원_부족사유_revision과_보수적종료_필요를_보호;
                재확인_필요를_유지; /* 물리 종료 완료를 합성하지 않음. */
                continue;
            }
            A1_입력별_Hold후보와_고정_anchor를_사용;
            if (time.earliestNow가_anchor더하기Hold_이상) {
                if (반영직전_관련_owner_문맥이_그대로_적용가능) {
                    해당_Hold_Rear_후보를_EXPIRED_newSessionAllowed_false로_관측하고_제거;
                    /* lastValidValue/Meta/quality 그대로. CLEAR/VALID 합성 없음. */
                    해당_변경_revision과_반영완료_관측을_공개;
                    이번_안전한_변경반영을_기록;
                } else {
                    재확인_필요를_남김;
                }
            } else if (time.latestNow가_anchor더하기Hold_미만) {
                if (원출처_원시각_현재Rear결합_등_재선택조건도_충분) {
                    해당_후보를_APPLICABLE_newSessionAllowed_true로_관측;
                } else {
                    해당_후보를_UNCERTAIN_newSessionAllowed_false로_관측;
                    원_부족사유와_보수적종료_재확인_필요를_보호;
                }
                /* 새 Session은 PB의 old retirement/최신 제한/자원 검사 뒤 정책 시작점에서. */
            } else {
                해당_후보를_UNCERTAIN_newSessionAllowed_false로_관측;
                원_경계걸침사유와_보수적종료_재확인_필요를_보호;
                /* 확정 만료/CLEAR/정상화/물리 STOP 완료/이력 삭제 없음. */
            }
            변경된_파생관측_revision과_반영완료_부족사유를_공개;
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

STABLE/CHANGED/RECONFIRM 모두 INPUT authority·원 Meta/age·PLAYBACK 시작 지식·HEALTH Fault를 임의 변경하지 않는다. Expired라도 replay/참조/미반영 통지의 안전 회수 조건이 별도이므로 ledger 삭제가 아니다. time 비교 부족을 now로 해결하지 않는다. 정상 Started의 후속 cue에는 `<L` 최초 gate를 반복하지 않는다.

FLOW는 [새 관측 재수집](../../../01_APP/FLOW/pseudocode/Flow_Process.md#procedure)으로 부족/미처리 의무까지 확인한다. actual/final 통지는 [CORE-04](Store_ApplyPlaybackFact.md#procedure)가 담당한다. Store_ApplyInput의 [D2 품질·Hold](Store_ApplyInput.md#procedure)를 재정의하지 않는다.

[R2](../../../../design/30_FUNCTIONS/20_INPUT_STORE_FUNCTIONS.md#store-advancedeadlines) · [R3 PB](../../../../design/40_DATA/40_PLAYBACK_DATA.md#vssplaybackobservation) · [R3 Stateful](../../../../design/40_DATA/20_STORE_DATA.md#vssstatefulstate) · [R4 Timing](../../../../design/50_CONTRACTS/30_TIMING_FRESHNESS.md#first-start) · [R5](../../../../design/60_PSEUDOCODE/20_INPUT_TO_SELECTION.md#store-advancedeadlines) · [R6](../../../../design/70_C_INTERFACE/00_HEADER_OWNERSHIP_MAP.md#r6-store-advancedeadlines)

## A1 입력 상실과 시간 경계

원 L과 원 시각 구간은 [Timing](../../../../design/50_CONTRACTS/30_TIMING_FRESHNESS.md#first-start)을 따른다. 입력 신뢰 상실의 replayAllowed=false는 NO_START_REEVALUATE/복구로 복원하지 않는다. in-flight/unknown의 만료는 미래 시작 차단·종료 요구이며 NO_START/Expired 합성이 아니다. 충분한 미시도/NO_START 및 retirement에서만 기한 종결을 확정한다. 늦게 도착한 적법 actual은 실제 상실 이전 권한/원 시각이 입증된 경우 원 occurrence에만 반영하고 종료를 취소하지 않는다.

Warning의 원 TTL 도달/첫 loss/고정 Hold는 별개다. no-RX에도 후보 제거와 재평가를 진행하며 lossAt을 점검 now로 바꾸지 않는다. 마지막 유효 Stateful의 Hold 내 재선택은 원 출처·시각·고정 anchor·Rear gate/fence와 old retirement 조건에서 허용한다. 확정 만료는 earliestNow>=anchor+Hold이며 경계 걸침/비교 부족은 UNCERTAIN·보수적 종료·재확인으로 구별한다.
