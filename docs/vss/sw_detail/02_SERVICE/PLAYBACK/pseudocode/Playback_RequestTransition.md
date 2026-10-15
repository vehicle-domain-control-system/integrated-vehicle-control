# Playback_RequestTransition — 한국어 C 형태 의사코드

> A1 입력 개정 2026-10-15 · [현재 Network 연결](../../../../design/90_BINDING/92_NETWORK_BINDING.md). `L=min(양수 USE_LIMIT,500ms,2000ms)`; 최초 actual에만 적용. 과거 동결/PASS는 당시 범위의 이력이며 본문 변경은 이번 A1 결정이다. 구현·시험 NOT_RUN.

[상세 설명](../functions/Playback_RequestTransition.md) · [의사코드](../pseudocode/Playback_RequestTransition.md) · [PLAYBACK](../00_MODULE_OVERVIEW.md) · [VSS 지도](../../../01_VSS_MAP.md) · CORE-07

> GUIDE · LOGICAL_APPROVED / 실제 C ABI BINDING_TBD · TEST_NOT_RUN / HW_UNVERIFIED.

<a id="contract"></a>
## 목적과 최신 논리 원형

FLOW의 선택 판단 또는 원 Session 종료 의도를 최신 조건으로 검토하고 PLAYBACK의 채택·유지·후속 권한 차단을 반영합니다. 동기 수용은 준비 완료·actual·물리 종료가 아닙니다.

```text
VssRequestDisposition Playback_RequestTransition(
    ONE_OF const VssSelectionDecision *decision,
           const VssSessionKeyType *terminationSession,
    OPTIONAL IN const VssCauseType *terminationReason,
    IN const VssCandidateObservation candidates[count],
    IN const VssFaultState restrictions[count],
    IN const VssAssetDescriptor assets[count],
    IN const VssDeviceReadinessState devices[count],
    IN const VssBackendCleanupObservation cleanup[count],
    IN const VssReadBasis *conditionsBasis);
```

[R6 계약](../../../../design/70_C_INTERFACE/00_HEADER_OWNERSHIP_MAP.md#r6-playback-requesttransition)과 [인자·결과 표기](../../../../design/70_C_INTERFACE/00_HEADER_OWNERSHIP_MAP.md#prototype-notation)를 그대로 읽습니다. 각 count/capacity/written은 독립 논리 범위입니다. 배열 저장 방식·수치·동기 보호·enum 폭·Header는 실제 ABI로 확정하지 않습니다. 과거 R2 provisional 묶음을 새 public DTO로 선언하지 않습니다.

아래 한글 조건/문장은 기존 owner의 검사·보호·반영을 뜻하는 **설명용 의사 연산**이며 새 함수/API/자료 구조가 아닙니다. `[]`와 각 군의 `.written`/`.capacity`는 R6 범위의 의사 접근이고 실제 구조체 필드 선언이 아닙니다. 반복은 전달된 유효 범위와 기존 처리 예산 안에서 끝나며 완료까지 무한 재시도하지 않습니다. Callee의 한글 OUT 인자는 해당 R6 typed 결과군별 공간을 줄여 쓴 표기입니다.

<a id="procedure"></a>
## P1~P6 — 배타 입력·종료·선택·채택

```c
VssRequestDisposition Playback_RequestTransition(
    /* ONE_OF */ const VssSelectionDecision *decision,
           const VssSessionKeyType *terminationSession,
    /* OPTIONAL IN */ const VssCauseType *terminationReason,
    /* IN */ const VssCandidateObservation candidates[],
    /* IN */ const VssFaultState restrictions[],
    /* IN */ const VssAssetDescriptor assets[],
    /* IN */ const VssDeviceReadinessState devices[],
    /* IN */ const VssBackendCleanupObservation cleanup[],
    /* IN */ const VssReadBasis *conditionsBasis)
{
    /* P1: non-NULL 개수만 먼저 확인; 각 객체 역참조보다 앞선다. */
    if ((decision == NULL) == (terminationSession == NULL)) {
        return VSS_REQUEST_REJECTED;
    }
    if (선택된_lane의_저장소가_안전하지_않음 ||
        terminationReason의_존재한_읽기참조가_부적합 ||
        (decision != NULL && terminationReason != NULL)) {
        return VSS_REQUEST_REJECTED; /* 종료 reason을 decision과 혼합하지 않는다. */
    }
    if (각_count_전달영역과_양수_IN범위를_안전하게_검증하지_못함 ||
        conditionsBasis == NULL || conditionsBasis의_주소_참조가_부적합) {
        기존_owner_사실_권한_보호와_조건_재확인_필요를_유지;
        return VSS_REQUEST_WAIT;
    }

    /* P2: 원 Session 종료만. recovery용 가짜 Session을 만들지 않는다. */
    if (terminationSession != NULL) {
        if (원_key가_보호된_종료대상과_연결되지_않음 ||
            현재_owner와_다른_옛대상이나_미존재_key임) {
            해당_원귀속_기록과_진단_필요를_보호;
            return VSS_REQUEST_REJECTED; /* 다른 current STOP/권한 변경 없음. */
        }
        if (원_종료와_후속차단이_이미_보호되어_있음) {
            기존_원STOP_정리_결과_후속을_유지;
            return VSS_REQUEST_KEPT;
        }
        원Session.startAllowed = false;
        원Session.cueAllowed = false;
        원Session.repeatAllowed = false;
        원_의도와_기존_사유근거에_따라_STOPPING_ABORTING_또는_기존_QUARANTINED_유지;
        /* reason==NULL도 유효 명시 종료 가능; 임의 원인 enum·숫자 없음. */
        VssPlaybackControlCommand stop;
        stop.session = 원Session.session;
        stop.attempt = 원Session.currentAttempt;
        stop.action = VSS_AUDIO_STOP;
        if (!원_종료command와_하위_각_typed_결과보호를_실제로_확보_성공) {
            원_후속차단과_참조_정리의도를_보호하여_다음기회로_유지;
            return VSS_REQUEST_ADOPTED; /* PB 종료 의도만 반영. 하위 호출 아직 없음. */
        }
        AudioStream_RequestControl(&stop, NULL, 반영완료_자기_PB_읽기관측,
            candidates, restrictions, devices, cleanup, conditionsBasis,
            원_하위_requests_OUT범위, 원_하위_diagnostics_OUT범위);
        모든_즉시결과를_원귀속으로_보호해_Playback_Advance의_적용계약에_연결;
        /* STREAM은 Session 전체 관련 Attempt/operation/scope를 차단·정리.
           STOP 거부/partial이어도 PB 권한을 true로 rollback하지 않는다. */
        return VSS_REQUEST_ADOPTED; /* 실제 종료 확인은 후속에서만. */
    }

    /* P3: decision이 있어도 별도 입증된 현재 조건 상실은 안전 종료한다. */
    if (현재_owner의_유효_CLEAR_기한권한상실_고장_정책끝이_독립_입증됨) {
        원Session.startAllowed = false;
        원Session.cueAllowed = false;
        원Session.repeatAllowed = false;
        원_종료의도와_STOPPING_ABORTING_기존_QUARANTINED를_보호;
        VssPlaybackControlCommand stop;
        stop.session = 원Session.session;
        stop.attempt = 원Session.currentAttempt;
        stop.action = VSS_AUDIO_STOP;
        if (원command와_모든_하위결과_참조보호를_실제로_확보_성공) {
            AudioStream_RequestControl(&stop, NULL, 반영완료_자기_PB_읽기관측,
                candidates, restrictions, devices, cleanup, conditionsBasis,
                원_하위_requests_OUT범위, 원_하위_diagnostics_OUT범위);
            즉시_요청단계_진단을_독립_보호해_Advance로_연결;
        } else {
            원_차단_참조_정리의도를_보호하여_하위요청_후속을_유지;
        }
        return VSS_REQUEST_ADOPTED;
    }
    if (현재_phase가_STOPPING_ABORTING_QUARANTINED) {
        old_정리와_차단을_유지하고_최신_재선택_필요를_남김;
        return VSS_REQUEST_WAIT; /* winner가 old 종료 의도를 취소하지 않음. */
    }
    if (decision의_basis_저장소가_안전하지_않거나_평가보호가_끝남 ||
        conditionsBasis.validity != VSS_READ_VALID ||
        판단과_현재_인과_원시간_후보revision_제한_원근거가_바뀌거나_부족) {
        return VSS_REQUEST_WAIT;
    }

    /* P4: action만으로 actual이나 새 Device 요청을 확정하지 않는다. */
    switch (decision.action) {
    case VSS_SELECTION_RECOLLECT:
    case VSS_SELECTION_WAIT:
        return VSS_REQUEST_WAIT; /* 정상 current One-shot 무근거 STOP 없음. */
    case VSS_SELECTION_KEEP:
        if (같은_원candidate_전체plan_owner가_현재도_유효하고_계속권한_충분) {
            return VSS_REQUEST_KEPT; /* Session/Attempt 새 생성·prepare 없음. */
        }
        return VSS_REQUEST_WAIT;
    case VSS_SELECTION_REPLACE:
        if (old_출력_owner가_있음) {
            old.startAllowed = false;
            old.cueAllowed = false;
            old.repeatAllowed = false;
            old_종료의도_STOPPING과_원STOP_하위결과_후속을_보호;
            VssPlaybackControlCommand stop;
            stop.session = old.session;
            stop.attempt = old.currentAttempt;
            stop.action = VSS_AUDIO_STOP;
            if (원command와_모든_하위결과_참조보호를_실제로_확보_성공) {
                AudioStream_RequestControl(&stop, NULL, 반영완료_자기_PB_읽기관측,
                    candidates, restrictions, devices, cleanup, conditionsBasis,
                    원_하위_requests_OUT범위, 원_하위_diagnostics_OUT범위);
                원_즉시결과를_독립_보호해_Advance에_연결;
            } else {
                old_차단_원참조_STOP정리_후속을_보호;
            }
        }
        FLOW의_old_retirement_후_최신_재선택_필요를_유지;
        return VSS_REQUEST_WAIT; /* 오래된 winner 자동 CHOOSE/prepare 없음. */
    case VSS_SELECTION_CHOOSE:
        break;
    default:
        return VSS_REQUEST_REJECTED;
    }

    /* P5: 평가 borrow에서 새 Session 보호로 넘어가는 실제 채택 경계. */
    if (decision.candidate == NULL || decision.plan == NULL ||
        후보와_plan_주소_참조가_부적합 || 원key_mode_signal_plan_연결이_부적합) {
        return VSS_REQUEST_REJECTED;
    }
    if (원후보_현재_APPLICABLE_revision_시간_제한_구성_근거가_부족 ||
        기존_전체plan이나_필수asset_image_근거가_부족) {
        return VSS_REQUEST_WAIT;
    }
    if (!old_전체_retirement_및_모든_관련_provider_PCM_device_안전종료_입증 ||
        새_독립_identity_plan_Meta_각결과_보호공간이_부족) {
        return VSS_REQUEST_WAIT; /* old 기록 회수/eviction으로 공간을 만들지 않음. */
    }
    if (!현재후보.newSessionAllowed) {
        return VSS_REQUEST_WAIT; /* Event 원 L/replay 또는 Stateful 현재 유효/입증된 Hold·Rear 조건 필요. */
    }
    if (원후보가_미시작_One_shot &&
        conditionsBasis.time으로_원발생_age의_가능구간전체_L미만_또는_현재후보.newSessionAllowed_확인_불가) {
        return VSS_REQUEST_WAIT; /* 정확히 L 미허용; 원 Meta reset 없음. */
    }
    if (원후보가_One_shot && 그_occurrence가_이미_Started_또는_uncertain_final_replay금지) {
        return VSS_REQUEST_REJECTED; /* Started One-shot 자동 resume 없음. */
    }

    /* P6: 부수효력 직전 재검사, 보호 실패면 아직 새 prepare를 하지 않는다. */
    if (최신_원조건이_변함 || 불변_원귀속_plan_Meta_실제보호가_성공하지_않음) {
        return VSS_REQUEST_WAIT;
    }
    Session_Attempt_key_전체plan_원Meta를_PB_수명으로_보호해_채택;
    session.position = 기존_전체정책의_시작점;
    session.phase = VSS_PLAYBACK_PREPARING;
    현재_정책이_허용한_start_cue_repeat_권한만_반영;
    attempt.startKnowledge = VSS_START_NOT_ATTEMPTED;
    attempt.retired = false;
    VssAudioPreparation preparation;
    preparation.session = 보호한_원_session_key;
    preparation.attempt = 보호한_원_attempt_key;
    preparation.plan = 보호한_전체plan_참조;
    preparation.position = session.position;
    preparation.asset = 기존_현재_cue의_의미asset;
    preparation.firstStartMeta = 최초_One_shot_gate가_필요한_보호Meta_또는_NULL;
    AudioStream_Prepare(&preparation, 원_하위_requests_OUT범위,
                        원_하위_diagnostics_OUT범위);
    하위_disposition과_모든_요청단계_진단을_원귀속으로_독립_보호;
    if (하위_거부_실패_부분효력_또는_대기) {
        원_채택문맥_보호와_차단_정리_준비후속을_그_결과에_맞게_유지;
        /* 이미 생긴 PB/하위 효과를 단순 신규 거부처럼 rollback하지 않는다. */
    }
    Playback_Advance의_같은_원적용_계약에_즉시_비동기_후속을_연결;
    return VSS_REQUEST_ADOPTED; /* PB 의도 반영. prepared/actual은 아직 별도. */
}
```

<a id="postconditions"></a>
## 사후조건과 후속

배타/불안전 의도는 효력 전 거부합니다. WAIT/KEEP은 무근거 STOP을 만들지 않으며 REPLACE는 old 차단·STOP 후 retirement와 **새 평가**를 기다립니다. CHOOSE는 전체 plan/원 Meta·identity·typed 결과 보호 후 준비로 진입합니다. ADOPTED/KEPT는 물리 성공이 아니고 하위 부분 효력/late/통지는 Advance의 원적용 계약으로 이어집니다. 준비 후 CLEAR/기한 상실은 실제 START 직전에 다시 검사합니다. 정리 중 phase/후속 cue 부활·원 age 재기산·PCM 직접 반환·새 Recovery Session이 없습니다.

[입력·반환](../functions/Playback_RequestTransition.md#inputs) · [종료](../functions/Playback_RequestTransition.md#termination) · [R5 채택](../../../../design/60_PSEUDOCODE/30_PLAYBACK_FLOW.md#playback-requesttransition) · [R6 Prepare](../../../../design/70_C_INTERFACE/00_HEADER_OWNERSHIP_MAP.md#r6-audiostream-prepare) · [R6 Control](../../../../design/70_C_INTERFACE/00_HEADER_OWNERSHIP_MAP.md#r6-audiostream-requestcontrol) · [Advance 후속](Playback_Advance.md#procedure)
