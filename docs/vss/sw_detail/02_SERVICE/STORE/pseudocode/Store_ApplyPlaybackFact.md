# Store_ApplyPlaybackFact — 한국어 C 형태 의사코드

[상세 설명](../functions/Store_ApplyPlaybackFact.md) · [STORE](../00_MODULE_OVERVIEW.md) · [VSS 지도](../../../01_VSS_MAP.md) · CORE-04

> GUIDE · TEST_NOT_RUN / BINDING_TBD / HW_UNVERIFIED.

<a id="contract"></a>
## 논리 원형

```text
VssFactApplication Store_ApplyPlaybackFact(
    IN const VssPlaybackFact *fact);
```

[R6](../../../../design/70_C_INTERFACE/00_HEADER_OWNERSHIP_MAP.md#r6-store-applyplaybackfact) `IN`은 borrow 주석이다. 실제 C ABI는 TBD이며 compile-ready C가 아니다. 한글 조건/연산은 CORE 내부 설명용이고 새 helper·public API·필드·fact manager가 아니다. `record`는 기존 STORE 원 발생을 가리키는 의사 표기다. 적용 흔적·참조 보호의 실물 표현은 TBD이며 상태 enum만으로 중복 완료를 주장하지 않는다. [설명의 계약](../functions/Store_ApplyPlaybackFact.md#contract)과 [후속 Host/Gate](../functions/Store_ApplyPlaybackFact.md#feasibility)를 따른다.

<a id="procedure"></a>
## S4-1~S4-7 — 귀속, 증명, 보호 후 완료

```c
VssFactApplication Store_ApplyPlaybackFact(/* IN */ const VssPlaybackFact *fact)
{
    /* S4-1: NULL/무효 공간 내부를 읽지 않음. caller는 보호된 불변 fact 전달. */
    if (fact == NULL || fact_저장공간과_내부참조가_유효하지_않음) {
        return VSS_FACT_NOT_APPLIED;
    }
    if (원_fact_occurrence_Session_Attempt_accepted_추적연결이_부족 ||
        원_PLAYBACK_basisRevision_반영완료_인과를_확인할_수_없음) {
        적용부족_관측을_원문맥에_남김;
        return VSS_FACT_NOT_APPLIED;
    }

    /* S4-2: 원 fact 키만 같거나 record가 Started인 것만으로 완료 아님. */
    if (동일_fact_key가_보호된_적용흔적에_있음) {
        if (occurrence_session_attempt_kind_원occurredAt_basis의_동일성과
            필요한_정규화_fact_보호_반영완료를_증명) {
            return VSS_FACT_ALREADY_APPLIED;
        } else {
            충돌_원사실을_보호하고_기존_적용흔적을_유지;
            return VSS_FACT_NOT_APPLIED;
        }
    }
    /* S4-3: late가 최종/Expired 이력을 정상으로 되살리거나 덮지 못함. */
    if (원_UncertainFinal_또는_whole_final_등_보호된_종결이력과_충돌 ||
        현_active_포인터로_원발생을_재귀속해야만_적용할_수_있음) {
        기존_final_replay와_old_진단_격리를_유지;
        return VSS_FACT_NOT_APPLIED;
    }

    /* S4-4: kind별 충분성 검사를 완료한 뒤 S4-5에서 실제 owner 반영.
       아래 switch는 필요한 조건을 검사하며 상태 쓰기는 아직 없음. */
    switch (fact->kind) {
    case VSS_FACT_FIRST_ACTUAL_START:
        if (!원_최초_적법_actual_PLAYBACK_판정근거가_충분 ||
            !원발생부터_원actual_가능구간_전체의_age가_잠정_2초_미만 ||
            record.firstStartFact != NULL ||
            record.state != VSS_OCCURRENCE_PENDING) {
            /* 동일 적용 증명 없는 다른 first fact, 정확히 2초, 시간부족은 미적용. */
            return VSS_FACT_NOT_APPLIED;
        }
        break;
    case VSS_FACT_WHOLE_COMPLETED:
    case VSS_FACT_WHOLE_INTERRUPTED:
        if (record.firstStartFact == NULL ||
            !원_전체계획_종료와_전체관련_후속권한차단이_충분 ||
            !Started_원Session과_해당_final의_연결이_확인됨) {
            /* 단일 cue/segment/backend 일부 종료로 전체 final 합성 금지. */
            return VSS_FACT_NOT_APPLIED;
        }
        break;
    case VSS_FACT_START_OUTCOME_UNCERTAIN:
        if (!원_PLAYBACK의_불명확_최종판정_근거와_재실행차단이_확인됨) {
            return VSS_FACT_NOT_APPLIED;
        }
        /* 정상 대기/timeout을 이 함수가 uncertain final로 새로 판정하지 않음. */
        break;
    case VSS_FACT_NO_START_REEVALUATE:
        if (record.firstStartFact != NULL || 원_uncertain_final_존재 ||
            !전체_과거_NO_START_미래차단_retirement_근거가_충분) {
            return VSS_FACT_NOT_APPLIED;
        }
        /* Pending/Expiry 재확인 허용만. 원 age/replay/종결 이력 보존. */
        break;
    default:
        return VSS_FACT_NOT_APPLIED;
    }

    /* S4-5: accepted 추적 내 필요한 정규화 fact·참조·적용 증명 보호를 선행.
       임시 borrow 얕은 포인터 저장/옛 record eviction으로 성공 처리 금지. */
    if (!해당_fact의_ledger_수명_보호와_적용증명_보호를_완료할_수_있음) {
        기존_원본을_유지하고_보호부족_관측을_남김;
        return VSS_FACT_NOT_APPLIED;
    }
    해당_fact와_필요참조를_STORE_추적수명으로_보호;
    switch (fact->kind) {
    case VSS_FACT_FIRST_ACTUAL_START:
        record.firstStartFact = 보호한_원_fact_참조;
        record.state = VSS_OCCURRENCE_STARTED;
        /* old late actual도 원 occurrence만 반영. PB 종료 phase/cue 재개 없음. */
        break;
    case VSS_FACT_WHOLE_COMPLETED:
        record.finalFact = 보호한_원_fact_참조;
        record.state = VSS_OCCURRENCE_COMPLETED;
        break;
    case VSS_FACT_WHOLE_INTERRUPTED:
        record.finalFact = 보호한_원_fact_참조;
        record.state = VSS_OCCURRENCE_INTERRUPTED;
        break;
    case VSS_FACT_START_OUTCOME_UNCERTAIN:
        record.finalFact = 보호한_원_fact_참조;
        record.state = VSS_OCCURRENCE_UNCERTAIN_FINAL;
        /* 기존 firstStartFact 유지, 정상 부활/재실행 금지. cleanup 완료 아님. */
        break;
    case VSS_FACT_NO_START_REEVALUATE:
        원_통지_적용과_Pending_Expiry_재확인_가능성을_보호;
        /* originalMeta/state를 새 Pending/age로 초기화하지 않음.
           만료 자체는 CORE-05의 최신 pb/time 검사 뒤 판단. */
        break;
    }
    /* S4-6: 해당 fact별 적용 흔적 유지. 최초+final을 마지막 하나로 덮지 않음. */
    record.tracking = VSS_TRACKING_PROTECTED;
    원_fact의_적용완료_증명을_기존_STORE_추적에_반영;
    record.revision과_반영완료_관측을_모든_필요반영_뒤_공개;
    return VSS_FACT_APPLIED;
    /* S4-7: caller는 APPLIED/ALREADY_APPLIED인 해당 통지 의무만 완료.
       NOT_APPLIED 경로는 원 fact/identity/time/근거와 통지 의무 보호·후속.
       STORE 반환이 PCM 안전해제/HAL 저장 삭제/Session retirement를 허가하지 않음. */
}
```

<a id="postconditions"></a>
## 사후조건과 후속

어느 경로도 originalMeta·원 age·INPUT authority·Stateful/Rear·HEALTH를 재작성하지 않는다. firstStartFact와 finalFact는 서로 독립 보호한다. 수정·revision은 STORE 반영 완료 경계 안에서 공개하며 실제 직렬화 방식은 미정이다. 중간 보호 실패를 부분 APPLIED로 노출하지 않는다.

NOT_APPLIED이면 PLAYBACK이 기존 통지 내부 단계를 후속 진행한다. CORE-05는 [최신 관측과 미반영 통지를 먼저 확인](Store_AdvanceDeadlines.md#procedure)한다. 보관 raw 소스는 최종 제품 binding 근거가 아니며 미정 필드를 임의 true로 채워 이 switch를 통과시키지 않는다.

[R2](../../../../design/30_FUNCTIONS/20_INPUT_STORE_FUNCTIONS.md#store-applyplaybackfact) · [R3 Fact](../../../../design/40_DATA/40_PLAYBACK_DATA.md#vssplaybackfact) · [R3 ledger](../../../../design/40_DATA/20_STORE_DATA.md#vssoccurrencerecord) · [R4 late](../../../../design/50_CONTRACTS/20_ASYNC_RESULT_EVIDENCE.md#late-results) · [R5 멱등 적용](../../../../design/60_PSEUDOCODE/50_STOP_LATE_UNCERTAIN_FLOW.md#store-applyplaybackfact) · [R6](../../../../design/70_C_INTERFACE/00_HEADER_OWNERSHIP_MAP.md#r6-store-applyplaybackfact)
