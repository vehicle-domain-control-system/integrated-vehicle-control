# Playback_Advance — 한국어 C 형태 의사코드

> A1 입력 개정 2026-10-15 · [현재 Network 연결](../../../../design/90_BINDING/92_NETWORK_BINDING.md). `L=min(양수 USE_LIMIT,500ms,2000ms)`; 최초 actual에만 적용. 과거 동결/PASS는 당시 범위의 이력이며 본문 변경은 이번 A1 결정이다. 구현·시험 NOT_RUN.

[상세 설명](../functions/Playback_Advance.md) · [의사코드](../pseudocode/Playback_Advance.md) · [PLAYBACK](../00_MODULE_OVERVIEW.md) · [VSS 지도](../../../01_VSS_MAP.md) · CORE-08

> GUIDE · LOGICAL_APPROVED / 실제 C ABI BINDING_TBD · TEST_NOT_RUN / HW_UNVERIFIED.

<a id="contract"></a>
## 목적과 최신 논리 원형

원 Session/Attempt의 typed 요청·출력·정리 사실을 적용하고, 실제 시작 적법성·정상 cue·전체 종료/retirement·STORE 통지와 독립 OUT 보호를 진행합니다. **void 반환은 전체 성공·완료를 뜻하지 않습니다.**

```text
void Playback_Advance(
    IN const VssAudioRequestResult requests[count],
    IN const VssAudioOutputEvidence outputs[count],
    IN const VssBackendCleanupObservation cleanup[count],
    IN const VssCandidateObservation candidates[count],
    IN const VssFaultState restrictions[count],
    IN const VssAssetDescriptor assets[count],
    IN const VssDeviceReadinessState devices[count],
    IN const VssReadBasis *conditionsBasis,
    IN const VssTimeEvidence *time,
    OUT VssPlaybackObservation observation[capacity -> written],
    OUT VssPlaybackFact facts[capacity -> written],
    OUT VssFactApplication applications[capacity -> written],
    OUT VssDiagnosticEvidence diagnostics[capacity -> written]);
```

[R6 계약](../../../../design/70_C_INTERFACE/00_HEADER_OWNERSHIP_MAP.md#r6-playback-advance)과 [인자·결과 표기](../../../../design/70_C_INTERFACE/00_HEADER_OWNERSHIP_MAP.md#prototype-notation)를 그대로 읽습니다. 각 count/capacity/written은 독립 논리 범위입니다. 배열 저장 방식·수치·동기 보호·enum 폭·Header는 실제 ABI로 확정하지 않습니다. 과거 R2 provisional 묶음을 새 public DTO로 선언하지 않습니다.

아래 한글 조건/문장은 기존 owner의 검사·보호·반영을 뜻하는 **설명용 의사 연산**이며 새 함수/API/자료 구조가 아닙니다. `[]`와 각 군의 `.written`/`.capacity`는 R6 범위의 의사 접근이고 실제 구조체 필드 선언이 아닙니다. 반복은 전달된 유효 범위와 기존 처리 예산 안에서 끝나며 완료까지 무한 재시도하지 않습니다. Callee의 한글 OUT 인자는 해당 R6 typed 결과군별 공간을 줄여 쓴 표기입니다.

<a id="procedure"></a>
## B1~B8 — 독립 검증·원 사실·START·whole·짝 공개

```c
void Playback_Advance(
    /* IN */ const VssAudioRequestResult requests[],
    /* IN */ const VssAudioOutputEvidence outputs[],
    /* IN */ const VssBackendCleanupObservation cleanup[],
    /* IN */ const VssCandidateObservation candidates[],
    /* IN */ const VssFaultState restrictions[],
    /* IN */ const VssAssetDescriptor assets[],
    /* IN */ const VssDeviceReadinessState devices[],
    /* IN */ const VssReadBasis *conditionsBasis,
    /* IN */ const VssTimeEvidence *time,
    /* OUT */ VssPlaybackObservation observation[],
    /* OUT */ VssPlaybackFact facts[],
    /* OUT */ VssFactApplication applications[],
    /* OUT */ VssDiagnosticEvidence diagnostics[])
{
    /* B1: 각각의 주소/전달영역/written 저장소를 먼저 검증한다.
       capacity 값 읽기·범위 계산도 안전한 해당 군에서만 한다. */
    if (observation_주소_capacity전달영역_written저장소가_유효 &&
        그뒤_observation_capacity값과_출력범위검증_통과) {
        observation.written = 0;
        observation_유효 = true;
    } else {
        observation_유효 = false; /* 무효 군 무읽기·무초기화·무쓰기. */
    }
    if (facts_주소_capacity전달영역_written저장소가_유효 &&
        그뒤_facts_capacity값과_출력범위검증_통과) {
        facts.written = 0;
        facts_유효 = true;
    } else {
        facts_유효 = false;
    }
    if (applications_주소_capacity전달영역_written저장소가_유효 &&
        그뒤_applications_capacity값과_출력범위검증_통과) {
        applications.written = 0;
        applications_유효 = true;
    } else {
        applications_유효 = false;
    }
    if (diagnostics_주소_capacity전달영역_written저장소가_유효 &&
        그뒤_diagnostics_capacity값과_출력범위검증_통과) {
        diagnostics.written = 0;
        diagnostics_유효 = true;
    } else {
        diagnostics_유효 = false;
    }
    각_IN군의_count전달영역_양수범위_내부참조를_군별로_안전하게_검증;
    /* count=0은 자료 없음. unsafe 군 읽지 않고 손실/후속을 보호.
       다른 안전한 lane·기존 원사실 적용/차단까지 일괄 중단하지 않는다. */
    최신_conditionsBasis와_time의_주소_원근거_연속성을_안전하게_확인;
    if (새_START에_필수인_최신조건_시간_자원근거가_부족) {
        새_START_권한을_주지_않고_조건_재확인_필요를_유지;
        /* 원 actual의 보호된 과거 시각/권한 검사·old 통지는 독립 진행 가능. */
    }
    if (현재_owner의_유효_CLEAR_기한권한상실_고장_정책끝이_별도_입증됨) {
        모든_미발행_start_cue_repeat_권한을_먼저_차단;
        원STOP_의도와_정리_하위_각결과_후속을_보호;
    }

    /* B2: 실제 output을 request acceptance보다 먼저 적용한다. */
    for (이번_예산안의_안전한_outputs_유효_count범위) {
        원Session_Attempt_position_scope_activation_불변귀속을_대조;
        if (귀속_대응이_불명확하거나_참조보호_부족) {
            원producer_근거와_귀속부족_또는_실손실_coverage_진단을_보호;
            continue; /* current Attempt로 사후 라벨링하지 않음. */
        }
        if (원_uncertain_final_뒤 || 원Attempt.retired) {
            해당_old_정리_참조종료_진단만_보호하여_적용;
            continue; /* 정상 Pending/Started/Completed·새 owner 부활 없음. */
        }
        output의_독립_transportFacts_원operation_scope_시각_coverage를_보호;
        switch (output.kind) {
        case OUTPUT_START_CONFIRMED:
            if (!원_활성화이후_새실제출력_상관_원범위_충분coverage_입증) {
                원관측과_부족범위를_보호하고_정상Started_확정위반_합성없음;
                원_후속권한_차단과_안전정리_필요를_보호;
                break; /* actual 이름/IRQ/accepted만으로 통과하지 않는다. */
            }

            /* B3: actual 충분성과 상위 적법성은 별도다. */
            if (이미_적법_Started된_같은Session의_정상_후속cue) {
                최초_age_gate_재적용없이_원Session_권한_정책_시도범위를_확인;
            } else if (최초_One_shot_actual) {
                if (!원Meta와_occurredAt의_domain_epoch_continuity_비교가능 ||
                    원_당시activation_권한_대응을_입증하지_못함) {
                    적법성_근거부족으로_분류;
                } else if (occurredAt.earliest >= 원Meta.originalAtLatest &&
                           occurredAt.latest - 원Meta.originalAt < 원Meta.useLimit &&
                           가능한_구간전체가_원_당시권한_범위안) {
                    적법한_최초_actual로_분류;
                } else if (가능한_구간전체가_기한이나_권한밖임이_입증됨) {
                    확정된_물리_출력위반으로_분류; /* 정확히 L 미허용. */
                } else {
                    경계걸침_등_적법성_불확실로_분류;
                }
            } else {
                Stateful의_원_실제출력_현재정책_당시권한_범위를_확인;
                /* 새 One-shot gate/occurrence를 만들지 않는다. */
            }
            if (원_actual의_적법성이_충분히_확인됨) {
                attempt.startKnowledge = VSS_START_ACTUAL_CONFIRMED;
                attempt.startEvidence = 원_불변출력근거의_보호참조;
                if (원_One_shot의_최초적법_actual && 기존_최초fact_생성보호_아직없음) {
                    원fact_identity_occurrence_session_attempt_원시각_revision을_보호;
                    fact.kind = VSS_FACT_FIRST_ACTUAL_START;
                    원fact를_개별_STORE_통지의무에_연결; /* 실제 호출/OUT은 B7. */
                }
                if (session.phase != VSS_PLAYBACK_STOPPING &&
                    session.phase != VSS_PLAYBACK_ABORTING) {
                    session.phase = VSS_PLAYBACK_ACTIVE;
                }
                /* 종료 중 적법 late: Started 사실만. 권한은 false 그대로. */
            } else {
                충분한_원물리fact_시각구간_coverage를_변조없이_보호;
                if (원_구간전체의_기한권한밖_위반이_입증됨) {
                    원producer_시각_대상_확정위반_진단을_보호;
                } else {
                    비교_당시권한_부족이나_경계걸침만_진단;
                    /* 정상 Started도 확정 위반도 만들지 않는다. */
                }
                session.startAllowed = false;
                session.cueAllowed = false;
                session.repeatAllowed = false;
                기존_STOPPING_ABORTING_유지_또는_ABORTING_원안전정리에_연결;
                /* 부족만으로 uncertain final/NO_START/retirement 생성 없음. */
            }
            break;
        case NO_START_CONFIRMED:
            if (해당_원scope의_충분한_과거_실제무출력 &&
                모든_옛request_active_descriptor_FIFO_frame_미래출력불가 &&
                해당_scope의_미래_decode_refill_PCM제출_차단) {
                원scope의_무출력_차단근거를_보호;
            } else {
                부족_또는_손실과_원_정리의무를_보호;
            }
            break; /* 전체 재평가/retirement는 B6에서 모든 범위를 확인. */
        case OUTPUT_TERMINATION_CONFIRMED:
            if (해당_원scope의_실제_잔류출력_종료 &&
                옛_미래출력불가 && 해당_scope의_미래PCM_차단) {
                원scope_출력종료_차단근거를_보호;
            } else {
                원scope_근거부족_정리의무를_보호;
            }
            break; /* SEGMENT/cue EOS는 whole completion 아님. */
        case START_OUTCOME_UNCERTAIN:
            원_시작지식_부족_coverage_관측을_보호;
            break; /* lower 불명확 하나가 상위 최종화 조건을 대신하지 않음. */
        default:
            부적합_원근거_진단과_후속을_보호;
        }
    }

    /* B4: 독립 cleanup/요청단계. 늦은 prepared/acceptance가 역전하지 않음. */
    for (이번_예산안의_안전한_cleanup_유효_count범위) {
        if (원Attempt에_귀속과_실제_ref_PCM_lower출력_late보호가_대응됨) {
            각_확인범위만_원_정리관측에_반영;
        } else {
            원_부족손실_정리의무를_보호; /* current 자원 해제 없음. */
        }
    }
    for (이번_예산안의_안전한_requests_유효_count범위) {
        if (!원Session_Attempt_연결과_원결과_보호_충분) {
            원_귀속부족_손실_진단을_보호;
            continue;
        }
        if (원_uncertain_final_뒤 || 원Attempt.retired) {
            old_요청단계_정리_진단만_보호;
            continue;
        }
        switch (request.stage) {
        case VSS_AUDIO_PREPARE_RECEIVED:
        case VSS_AUDIO_PREPARE_PENDING:
        case VSS_AUDIO_CONTROL_RECEIVED:
            원_요청단계_사실만_독립_보호;
            break;
        case VSS_AUDIO_PREPARED:
            attempt.preparation = VSS_AUDIO_PREPARED;
            if (session.phase != VSS_PLAYBACK_PREPARING ||
                이미_실제단계_또는_종료차단이_진행) {
                늦은_준비사실만_보호; /* actual/phase/권한 rollback 없음. */
            } else if (실제_START_직전_최신_CLEAR_후보_기한_제한_장치_자원_basis_충분 &&
                       (이미_적법Started된_동일Session의_정상후속START || 현재후보.newSessionAllowed) &&
                       session.startAllowed && 아직_이_원START를_발행하지_않음) {
                if (!원START_문맥과_독립_결과보호를_효력전_실제로_확보_성공) {
                    원prepared_근거_보호와_새START_대기_후속을_유지;
                    break; /* phase/지식 변경·하위 START 발행 전의 실패. */
                }
                session.phase = VSS_PLAYBACK_START_IN_FLIGHT;
                attempt.startKnowledge = VSS_START_PENDING_OR_UNKNOWN;
                VssPlaybackControlCommand start;
                start.session = session.session;
                start.attempt = attempt.attempt;
                start.action = VSS_AUDIO_START;
                AudioStream_RequestControl(&start, NULL, 반영완료_자기_PB관측,
                    candidates, restrictions, devices, cleanup, conditionsBasis,
                    원_하위_requests_OUT범위, 원_하위_diagnostics_OUT범위);
                모든_즉시_후속결과를_원시도에_독립_보호;
            } else {
                모든_미발행_start_cue_repeat_권한_차단;
                원STOP_의도와_하위정리_결과후속을_보호;
            }
            break;
        case VSS_AUDIO_START_SUBMITTED:
            제출사실을_보호하되_더진행된_수용_부분효력_실제_종료지식_rollback없음;
            break;
        case VSS_AUDIO_START_ACCEPTED_AND_ENABLED:
            원수용_활성화사실만_보호_반영;
            break; /* actual/NO_START/정상 최종이 아님. */
        case VSS_AUDIO_PREPARE_FAILED:
        case VSS_AUDIO_REQUEST_REJECTED:
        case VSS_AUDIO_EFFECT_UNCERTAIN:
            원단계_부분효력_retain_참조_손실근거를_각각_보호;
            해당_원권한_차단과_STOP_정리_미확정지식을_유지;
            break; /* 거부로 실제 무접근/전체 종료를 추정하지 않는다. */
        default:
            원_단계_부적합_진단_후속을_보호;
        }
    }

    /* B5: 결과 count=0이어도 현재 정책·기한·기존 의무를 bounded 진행.
       앞 loop의 old 참조를 current로 재사용하지 않는다. 아래 전이는 PB가
       권한·원문맥을 갖는 해당 Session만 대상으로 하며 다른 새 owner는 불변. */
    if (정상_ACTIVE && 현재_관련_Backend구간의_안전끝_입증 &&
        전체plan에_정상_다음cue_반복_있음) {
        if (해당_cue_repeat_권한과_최신_원조건이_충분) {
            정의된_gapAfter_repeatRule의_원정책_대기를_진행;
            if (정책상_다음_준비기회이고_원참조와_새시도_보호_충분) {
                같은Session의_다음_position과_필요_원Attempt를_보호;
                VssAudioPreparation preparation;
                preparation.session = session.session;
                preparation.attempt = 보호한_그_후속_원Attempt_key;
                preparation.plan = session.plan의_보호참조;
                preparation.position = 보호한_다음_원position;
                preparation.asset = 기존_해당cue의_의미asset;
                preparation.firstStartMeta = NULL;
                AudioStream_Prepare(&preparation, 원_하위_requests_OUT범위,
                                    원_하위_diagnostics_OUT범위);
                원_모든_즉시결과와_후속을_보호;
            }
        } else {
            원_후속권한_차단과_정리의무를_유지;
        }
        /* 같은 Session. 최초 L gate 재적용/새 occurrence/offset resume 없음. */
    }

    /* B6: uncertain final과 whole retirement는 각기 별도 충분조건. */
    if (해당_원Session이_아직_retire되지_않았고 && !원_uncertain_final_이미있음 &&
        기존_TBD_FINAL의_원_불명확최종_판정조건이_실제로_충족) {
        attempt.startKnowledge = VSS_START_UNCERTAIN_FINAL;
        session.startAllowed = false;
        session.cueAllowed = false;
        session.repeatAllowed = false;
        session.phase = VSS_PLAYBACK_QUARANTINED;
        원_One_shot이면_VSS_FACT_START_OUTCOME_UNCERTAIN의_개별통지를_보호;
        PLAYBACK_STATE_FAILURE_FAULT_UNAVAILABLE의_기존진단근거를_FLOW에_보호;
        원STOP을_기존_R6_playback_lane으로_연결하고_old정리_결과를_보호;
        /* timeout/미정조건/현재 무음만으로 이 조건을 true로 만들지 않는다. */
    }
    if (전체_종료의도_또는_정상_전체plan끝_또는_원조건상실) {
        모든_미발행_start_cue_repeat_권한을_차단;
        원STOP과_각_related_Attempt_segment의_후속정리를_유지;
    }
    /* scope별 조건은 B2에서 원 identity/operation/scope·시각·coverage를
       확인·보호한 충분 근거만 소비한다. 각 scope는 NO_START 또는 TERMINATION으로
       충족할 수 있으나 다른 scope의 근거로 대체하지 않는다. 미처리/부족/손실이나
       현재 idle/STOP ack/zero-fill/IRQ만으로 아래 조건을 true로 만들지 않는다. */
    if (해당_원Session이_아직_retire되지_않았고 &&
        모든_관련_원Attempt_scope를_빠짐없이_검토했고 &&
        모든_관련_원Attempt_scope_각각에서 (
            (해당_scope의_과거_실제무출력_전체범위_충분입증 &&
             해당_scope의_모든_옛작업_미래출력불가_충분입증) ||
            (해당_scope의_실제_잔류출력종료_충분입증 &&
             해당_scope의_모든_옛작업_미래출력불가_충분입증)
        ) &&
        모든_관련scope_미래PCM제출차단 && PB_전체_start_cue_repeat_차단 &&
        provider_read_PCM_새owner자원_실제안전조건_충분 && late_원귀속보호_충분) {
        if (원_uncertain_final_이미있음) {
            uncertain_최종_replay금지_유지; /* 정상 Completed/NO_START 부활 없음. */
        } else if (충분한_전체NO_START이고_과거실제시작없음_입증) {
            attempt.startKnowledge = VSS_START_NO_START_CONFIRMED;
            원_One_shot이면_VSS_FACT_NO_START_REEVALUATE_통지를_보호;
        } else if (원_전체Session의_적법Started_확인됨) {
            if (전체_정상plan끝) {
                원_One_shot이면_VSS_FACT_WHOLE_COMPLETED_통지를_보호;
            } else {
                원_One_shot이면_VSS_FACT_WHOLE_INTERRUPTED_통지를_보호;
            }
        } else {
            과거시작_미확정지식_replay_추적_후속최종판정을_보호;
            /* termination을 과거 NO_START나 정상 Completed로 만들지 않음. */
        }
        모든_미반영fact_진단_참조_의무를_독립_보호;
        for (전체조건을_확인한_해당Session의_관련_원Attempt) {
            원Attempt.retired = true;
        }
        session.phase = VSS_PLAYBACK_RETIRED;
        출력owner_사용권_해제와_FLOW_최신재선택_필요를_남김;
        /* ledger/Fault/HAL callback storage 폐기는 별도. */
    } else {
        원owner_참조_차단_정리와_미확정지식을_유지;
        /* scope 하나/EOS/cleanup 누락으로 whole final/retirement 생성 없음. */
    }

    /* B7: 실제 STORE 시도와 원 fact의 짝만 공개한다. */
    for (이번_예산안의_각_보호된_개별_STORE_미반영fact) {
        if (!원fact_identity_시각_참조와_반환결론_보호공간을_효력전_실제로_확보_성공) {
            가능한_원producer_보호와_실손실_coverage_후속을_유지;
            continue; /* 미시도에 NOT_APPLIED/ALREADY_APPLIED를 만들어 붙이지 않음. */
        }
        VssFactApplication applied = Store_ApplyPlaybackFact(보호된_원fact_참조);
        그_실제시도의_원fact와_applied_결론을_독립_불변보호;
        if (facts_유효 && applications_유효 &&
            facts.written == applications.written &&
            facts.written < facts.capacity && applications.written < applications.capacity &&
            두_전달자료의_참조수명과_같은index_쌍기록_보호가_완료) {
            i = facts.written; /* 의사 index: 기존 유효 공개 pair 수. */
            facts[i] = 보호된_원fact_값;
            applications[i] = 그_실제시도의_applied;
            if (양쪽_실제기록과_필요한_전달참조보호가_모두_성공) {
                facts.written = facts.written + 1;
                applications.written = applications.written + 1;
                그_실제시도의_원fact_결론짝을_이번호출에_인계한_상태로_반영;
            } else {
                두_count를_늘리지_않고_원fact_실제결론_미공개짝을_보호;
            }
        } else {
            원fact와_실제적용결론_미공개짝을_보호;
            /* 한쪽 invalid/0/full이면 불완전 pair 공개·count 증가 없음. */
        }
        if (applied == VSS_FACT_APPLIED || applied == VSS_FACT_ALREADY_APPLIED) {
            그_원fact의_STORE_통지의무만_완료;
            미공개결론과_실제consumer_참조보호는_별도로_유지;
        } else {
            원fact_원identity_시각_참조_STORE의무를_그대로_보호;
            /* NOT_APPLIED: 다음 bounded 기회 재적용. 거짓 ack 없음. */
        }
    }
    for (이번_예산안의_이전실제_STORE시도_완료후_미공개_보호짝) {
        if (이번_호출에_아직_공개하지_않았고 &&
            facts_유효 && applications_유효 &&
            facts.written == applications.written &&
            facts.written < facts.capacity && applications.written < applications.capacity &&
            그_원fact_실제결론의_같은index_전달참조_보호가_완료) {
            i = facts.written;
            facts[i] = 그_보호짝의_원fact;
            applications[i] = 그_원STORE시도의_실제결론;
            if (양쪽_실제기록과_필요참조보호가_모두_성공) {
                facts.written = facts.written + 1;
                applications.written = applications.written + 1;
                그_미공개짝의_인계상태만_반영;
            } else {
                원fact_실제결론_미공개짝_보호를_유지;
            }
        } else {
            그_미공개짝과_원소비수명_보호를_유지;
        }
        /* 미시도 fact·다른 결론 합성 없음. STORE 통지 의무와 OUT 인계는 별도. */
    }

    /* B8: observation/diagnostics는 pair 공간과 독립이다. */
    if (observation_유효 && observation.written < observation.capacity &&
        반영완료_현재또는추적_PB관측과_원stamp_필요참조_보호가_충분) {
        observation[observation.written] = 반영완료_유효_PB관측;
        if (실제관측기록과_필요참조수명_보호가_성공) {
            observation.written = observation.written + 1;
        }
    } /* 관측할 원 Session이 없으면 written=0·진단/후속, 정상 Session 합성 없음. */
    for (이번_예산안의_각_보호된_미인계_원진단) {
        if (diagnostics_유효 && diagnostics.written < diagnostics.capacity &&
            해당_진단의_원producer_원범위_시각_인계보호가_충분) {
            diagnostics[diagnostics.written] = 보호된_원진단_값;
            if (실제진단기록과_필요참조수명_보호가_성공) {
                diagnostics.written = diagnostics.written + 1;
            } else {
                원진단_미인계_보호와_실손실_범위를_유지;
            }
        } else {
            원진단_미인계_후속을_보호; /* 0/full/invalid로 진단 의무 삭제 없음. */
        }
    }
    방문하지_못한_모든_IN_즉시결과_미반영fact_진단_정리의무를_각원owner에_보호;
    return; /* void: 전체 실제 출력/종료·STORE/HEALTH 완료 표현 아님. */
}
```

<a id="postconditions"></a>
## 사후조건과 남은 처리

각 군의 written은 유효 저장·실제 기록·참조 보호가 완료된 자료만 셉니다. 유효 공개된 facts/applications는 동일 원 fact의 실제 STORE 시도와 같은 index·count이며 미시도/불완전 짝은 없습니다. 무효 OUT은 무접근, NOT_APPLIED와 미공개 결과/진단·방문 못한 입력은 각 원 수명으로 보호합니다. Started와 whole final·진단이 동시에 생겨도 마지막 결과 하나로 덮지 않습니다.

actual 충분성·당시 적법성·whole 범위·retirement는 별도입니다. uncertain/retired late는 old 정리만, 종료 중 적법 late는 사실만 반영합니다. 정상 동일 Session cue는 최초 gate를 재적용하지 않습니다. 물리 충분성·final/저장/시간 ABI의 미정은 위 동작의 주입 검증 후보를 없애지 않으며 실제 Host/Build/Flash/보드는 NOT_RUN입니다.

[입력/OUT](../functions/Playback_Advance.md#inputs) · [짝 대응](../functions/Playback_Advance.md#pairing) · [actual](../functions/Playback_Advance.md#actual) · [전체 종료](../functions/Playback_Advance.md#termination) · [R5 최초 gate](../../../../design/60_PSEUDOCODE/30_PLAYBACK_FLOW.md#first-actual-gate) · [R5 STOP/late](../../../../design/60_PSEUDOCODE/50_STOP_LATE_UNCERTAIN_FLOW.md#playback-stop-late) · [R5 통지](../../../../design/60_PSEUDOCODE/50_STOP_LATE_UNCERTAIN_FLOW.md#fact-notification) · [R4 충분조건](../../../../design/50_CONTRACTS/20_ASYNC_RESULT_EVIDENCE.md#confirmed-outcomes) · [R6 OUT 표기](../../../../design/70_C_INTERFACE/00_HEADER_OWNERSHIP_MAP.md#prototype-notation)
