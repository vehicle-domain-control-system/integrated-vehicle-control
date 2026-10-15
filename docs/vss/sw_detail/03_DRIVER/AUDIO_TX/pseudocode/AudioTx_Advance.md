# AudioTx_Advance — 한국어 C 형태 의사 절차

[VSS 지도](../../../01_VSS_MAP.md) · [DRIVER Layer](../../00_LAYER_OVERVIEW.md) · [AUDIO_TX Module](../00_MODULE_OVERVIEW.md) · [상세 설명](../functions/AudioTx_Advance.md)

> GUIDE · CORE-14. 의미 정본 `0641b2c2268cf47450aeb50302638a45a7a3277f`. 실제 ABI·HW binding 미확정. Host/Build/Flash/Board `NOT_RUN`.

<a id="contract"></a>
## 읽기 전용 R6 논리 원형

**R6 logical declarations != compile-ready C**. [R6 원문](../../../../design/70_C_INTERFACE/00_HEADER_OWNERSHIP_MAP.md#r6-audiotx-advance):

```text
void AudioTx_Advance(
    IN const VssTimeEvidence *time,
    OUT VssTxRequestResult requests[capacity -> written],
    OUT VssTxOutputEvidence outputs[capacity -> written],
    OUT VssPcmConsumptionEvidence consumed[capacity -> written],
    OUT VssPcmReturnEvidence returned[capacity -> written],
    OUT VssDiagnosticEvidence diagnostics[capacity -> written]);
```

아래는 이 Core 내부의 **의사 연산**입니다. 한글 조건/반복 대상·`requests.written/capacity` 등의 군별 표기는 실제 C symbol/구조체 필드/함수 인자가 아닙니다. 임의 helper/typedef/global buffer를 정의하지 않습니다. 실제 저장/직렬화·반복 예산·loss 보존 수단·시간/물리 Binding은 TBD입니다. 과거 R2의 `VssTxProgress` 반환과 R3 SPLIT은 [상세 계약](../functions/AudioTx_Advance.md#contract)에서 구분합니다.

<a id="procedure"></a>
## T1~T9 — 원 사실과 독립 결과군

```c
/* [의사 연산] AudioStream_Advance가 호출; HAL ISR의 역호출 없음. */
/* T1: requests/outputs/consumed/returned/diagnostics를 각각 독립 검증한다.
   아래 순회·count는 의사 연산이며 새 collection/필드/API·C ABI가 아니다. */
for (각_기존_독립_OUT_군에_대해) {
    if (그_군의_주소_capacity_전달영역_written_저장공간을_안전하게_확인했고
        그_뒤_필요한_capacity_값과_출력범위_검증까지_완료했음) {
        /* [의사 연산] 검증된 이 군의 written만 0으로 초기화한다.
           다른 군이나 owner의 보호 원본은 초기화하지 않는다. */
    } else {
        /* 무효 군의 written/capacity 메모리 읽기·쓰기·초기화 금지.
           이 군의 원사실/실패 진단은 기존 owner 내부에 미인계로 보호하고 후속.
           다른 유효 군은 독립 진행하며 전체 return/참조 해제를 하지 않는다. */
    }
}
/* T3~T8의 각 군 유효 조건은 위 선행 검증 전체를 의미한다.
   무효 군에서는 && 뒤의 written/capacity 접근과 결과 기록을 실행하지 않는다. */
if (time == NULL || 이번_시간의_유효성_연속성_비교가_부족) {
    /* 원 시각/coverage를 now로 채우지 않는다. 시간 의존 진행은 제한한다.
       확인된 시간 부족 사실은 T8 진단 대상으로 기존 경계에 보호한다.
       원 시간 문맥이 이미 보호된 독립 사실의 분류/보존은 계속 가능하다. */
}

/* T2: 기존 보호 공간과 처리 기회 내 bounded 후속; queue/횟수 신설 없음. */
for (미처리_보호_raw_또는_원_operation의_후속_사실_각각) {
    if (실제_누락_오버플로_원귀속_또는_시간연속성_손실이_기록됨) {
        /* 해당 범위는 VSS_COVERAGE_LOST. 무손실/충분으로 바꾸지 않는다. */
        해당_손실_원사실과_진단_필요성을_기존_owner_경계에_보호;
    } else if (관측_범위_시각비교_물리상관의_근거가_부족) {
        /* VSS_COVERAGE_INSUFFICIENT. 미관측을 누락으로 추정하지 않는다. */
        해당_부족_원사실과_진단_필요성을_보호;
    }
    if (사전_원등록과_실제_관측에서_원_operation_Attempt_scope_대응을_못_확인) {
        /* 최신 Session/Attempt/IRQ phase/A주소로 원키를 합성하지 않는다.
           원 작업 불명: requests/outputs/consumed/returned를 만들지 않는다.
           실제 확인된 문맥의 진단은 T8, 필수 문맥도 없으면 raw/손실 관측 보호.
           지속 vendor 등록과 개별 VssHalTxRegistration 수명을 별도 유지한다. */
        귀속불명_raw를_격리하고_원등록_후속_보호를_유지;
        continue;
    }
    /* 원 operation/handoff와 raw 원 occurredAt/capturedAt, scope/coverage 읽기.
       old key면 old에만 귀속. uncertain 최종/retired 뒤에는 old 정리/진단만;
       새 Session/PCM/정상 Started 복원 금지. 상위 원 최종 판단은 owner에게 남긴다. */

    /* T3: 요청 결과. 다른 네 군과 독립 if이며 전체 완료로 반환하지 않는다. */
    if (원_요청_state가_실제_근거로_확인되고_미인계_결과가_있음) {
        /* 기존 상태: VSS_TX_SUBMITTED_OR_QUEUED / VSS_TX_ACCEPTED_AND_ENABLED /
           VSS_TX_REJECTED_WITH_NO_ACCESS / VSS_TX_EFFECT_UNCERTAIN /
           VSS_TX_CLEANUP_PENDING. 일부 효력이 있으면 무접근 거부가 아니다.
           원 operation/attempt/state를 갖는 VssTxRequestResult를 불변 보호.
           handoff 수용은 START activation/actual이 아니다. */
        if (requests의_출력공간_범위가_유효 && requests.written < requests.capacity &&
            결과_참조_수명을_보호할_수_있음) {
            requests의_다음_자리에_원_요청_결과를_기록;
            requests.written++;
        } else {
            원_요청_결과를_미인계로_보호하고_군_공간_후속을_남김;
        }
    }

    /* T4: 서로 독립인 출력 사실은 모두 처리. 단일 union/else 하나로 소실 없음. */
    for (이_원_scope에서_미인계인_독립_출력_판정_각각) {
        if (이_판정이_새출력이고_원_요청과_실제_activation_이후_새_출력경계_관측이고
            원_시각_연속성_scope_coverage_binding이_해당_판정에_충분) {
            /* kind = VSS_TX_NEW_OUTPUT_OBSERVED */
        } else if (이_판정이_과거무출력이고_필요_과거_전체구간의_실제_무출력_관측이_충분) {
            /* kind = VSS_TX_PAST_NO_OUTPUT_PROVEN; 미래 차단과 별도. */
        } else if (이_판정이_미래차단이고_옛_요청_active_descriptor_FIFO_frame의_미래출력_차단이_입증) {
            /* kind = VSS_TX_FUTURE_OUTPUT_BLOCKED; 과거 무출력과 별도. */
        } else if (이_판정이_종료이고_해당_scope의_실제_장치_잔류출력_종료가_입증) {
            /* kind = VSS_TX_OUTPUT_ENDED; whole Session retirement 아님. */
        } else {
            /* kind = VSS_TX_OUTPUT_UNCERTAIN. INSUFFICIENT/LOST 구별 보존.
               HALF/MAJOR/mute/zero-fill/abort ack/Sai_Ip_Send 호출로 확정 없음. */
        }
        /* [의사 연산] 각 독립 판정은 자기 근거만 분류한다. 여러 충분 사실이
           동시에 있으면 각각 반복·보호; 위 순서가 둘째 사실을 버리지 않는다.
           VssTxOutputEvidence의 원 operation/attempt/kind/scope/occurredAt/coverage.
           필수 원귀속/사실 시각 문맥 자체를 표현할 근거가 없으면 결과 합성 없이
           부족 raw/진단으로 보호한다. 원 시각에 불확실성이 있으면 그 의미 보존. */
        if (해당_타입의_필수_원문맥을_실제_근거로_표현할_수_있음) {
            if (outputs의_출력공간_범위가_유효 && outputs.written < outputs.capacity &&
                원_근거_참조를_보호할_수_있음) {
                outputs의_다음_자리에_이_독립_출력_근거를_기록;
                outputs.written++;
            } else {
                이_출력_근거를_미인계로_보호하고_후속을_남김;
            }
        } else {
            원_부족_손실_raw와_진단_필요성을_보호;
        }
    }

    /* T5: 소비는 실제 확인 범위만. safe-return/CPU 사용권과 독립. */
    if (원_operation.handoff != NULL && 원_Attempt_A또는B_cycle과
        handoff_유효범위_안의_실제_소비_frame구간_원시각이_확인됨) {
        /* VssPcmConsumptionEvidence: operation/key/firstFrame/frameCount/occurredAt.
           buffer capacity로 frameCount를 늘리지 않고 old A를 new A에 적용하지 않음. */
        if (consumed의_출력공간_범위가_유효 && consumed.written < consumed.capacity &&
            소비_근거_참조를_보호할_수_있음) {
            consumed의_다음_자리에_원회차_소비_근거를_기록;
            consumed.written++;
        } else {
            원회차_소비_사실을_미인계로_보호;
        }
        /* cyclic 재접근 위험이 남으면 소비 뒤에도 device 보호 유지. */
    } else if (소비_판정이_필요하지만_원회차_범위_또는_시각_근거가_부족) {
        소비_확정없이_원_handoff_보호와_부족_손실_진단_필요성을_유지;
    }

    /* T6: 반환. 제어 handoff=null에는 반환 record를 만들지 않는다. */
    if (원_operation.handoff != NULL) {
        if (정확한_원_PCM_key의_HW_대기_처리참조가_모두_끝났고
            옛내용_반복_재접근_위험이_차단됐다는_실제_충분_basis가_있음) {
            /* VssPcmReturnEvidence: 원 operation/key, noFutureAccess=true,
               basis.validity=VSS_COVERAGE_SUFFICIENT. old 내용 한정 의미. */
            if (returned의_출력공간_범위가_유효 && returned.written < returned.capacity &&
                반환_근거_참조를_보호할_수_있음) {
                returned의_다음_자리에_이_원회차_반환_근거를_기록;
                returned.written++;
            } else {
                원회차_반환_근거를_미인계로_보호하고_정리_후속을_유지;
            }
        } else {
            /* HALF/MAJOR/abort 반환/회차 증가로 true를 만들지 않는다. */
            반환_record없이_원_cycle_device_보호_정리_부족또는손실을_유지;
        }
    }
    /* STREAM의 현재 write window/차기 DMA 방문/WCET/IRQ/cache 재확인은 별도.
       TX는 CPU_WRITABLE, decoder/refill/zero-fill, provider 생산을 직접 쓰지 않는다. */

    /* T7: STOP/부분 효력/late는 원 operation의 남은 위험별로 보존. */
    if (STOP_대상_옛작업의_일부_범위만_차단또는정리됨) {
        /* T4의 실제 출력/차단과 T6의 PCM 반환은 독립 기록.
           아직 입증 안 된 device/future 가능성·late 보호는 유지한다.
           제어 operation ack를 대상 전체 완료로 합치지 않는다. */
        남은_원_device_future_가능성과_정리_후속을_보호;
    }
    if (이_원작업의_실제_미래접근_또는_출력종료가_충분하게_입증됨) {
        /* 입증된 범위에만 TX own 가능성 반영. 결과 참조/상위/HAL 폐기는 별도. */
        해당_원작업_입증범위의_TX_보호상태만_반영;
    }
    /* raw는 모든 필요한 분류 사실이 안전하게 정규화·보호되거나 owner 소비
       조건이 충족될 때만 인계/처리 완료로 표시. OUT 복사만으로 폐기하지 않음. */
}

/* T8: raw 격리·시간 부족·결과군 공간 실패 등을 포함한 실제 진단 후속. */
for (이번_기회에_보호된_미인계_진단_사실_각각) {
    if (R3_필수_producer_stage_cause_target_configuration_원시각_문맥을_확인) {
        /* VssDiagnosticEvidence. operation은 확인된 원 optional 참조만;
           귀속불명을 현재 operation에 붙이지 않는다. 새 Fault/enum 없음. */
        if (diagnostics의_출력공간_범위가_유효 && diagnostics.written < diagnostics.capacity &&
            내부_참조를_보호할_수_있음) {
            diagnostics의_다음_자리에_불변_원진단을_기록;
            diagnostics.written++;
        } else {
            원진단을_미인계로_보호하고_후속을_남김;
        }
    } else {
        실제_확인된_raw_부족_손실_관측을_owner에_보호;
    }
}

/* T9: 공간 부족을 숨기거나 accepted/final/late 기록을 지우지 않는다. */
if (미인계_사실의_기존_bounded_보호공간도_부족또는_실제보존실패) {
    /* 실제 누락은 LOST로 observable 보존, 위험 증가 신규 진행 제한.
       구체 loss 수단/용량은 Binding TBD. 무한 queue/가짜 written 증가 금지. */
    실제_손실_관측과_정리_후속을_기존_경계에_남김;
}
/* requests/outputs/consumed/diagnostics는 downstream 반영/보호 추적까지;
   returned는 같은 key 적용·usage 회수까지. 원 내부 참조는 그 수명을 보호.
   HAL 기록/지속 vendor 등록은 각 HW·대기·처리·late 독립 종료 조건까지. */
return;  /* void/어느 군의 0 count도 완료·성공·보호 해제 근거가 아니다. */
```

<a id="postconditions"></a>
## 사후조건과 미정

5군의 `0 <= written <= capacity`를 각자 지키고 기록한 사실은 원 operation/Attempt/PCM 회차·원 시각/coverage에만 귀속합니다. 반환군이 0이면 실제 안전 반환이 없거나 미인계일 수 있으며 보호가 계속됩니다. 어느 군의 부족으로 다른 군의 확인 사실을 지우지 않습니다. 모든 사용/참조 수명은 원 owner가 끝낼 때까지 보호합니다.

`OUTPUT_START_CONFIRMED`/`NO_START_CONFIRMED`/`OUTPUT_TERMINATION_CONFIRMED`는 상위 STREAM/PLAYBACK의 별도 충분조건 결합이며 여기서 TX enum을 대체하지 않습니다. CPU 쓰기 안전·provider 재개·whole retirement도 별도 owner 판단입니다. R6-01~08, Q01~14, 외부 IF/HW, WINDOW 제품 보류·격리 TEST, P2 HOLD를 유지합니다. 논리 fixture 후보는 실행하지 않았고 물리 충분조건·RTD Binding을 확정하지 않았습니다.

<a id="references"></a>
## 분기별 정본

- T1~T2: [R3 raw/원 시각](../../../../design/40_DATA/60_AUDIO_TX_DATA.md#vssrawobservation), [R3 지속 등록](../../../../design/40_DATA/60_AUDIO_TX_DATA.md#vsshaltxregistration), [R4 원 근거](../../../../design/50_CONTRACTS/20_ASYNC_RESULT_EVIDENCE.md#evidence-basis)
- T3~T6: [R3 독립 결과군](../../../../design/40_DATA/60_AUDIO_TX_DATA.md#vsstxrequestresult), [R4 typed lanes](../../../../design/50_CONTRACTS/50_AUDIO_BUFFER_TRANSPORT.md#tx-typed-lanes), [R5 TX 반환](../../../../design/60_PSEUDOCODE/50_STOP_LATE_UNCERTAIN_FLOW.md#tx-safe-return)
- T7~T9: [R4 late](../../../../design/50_CONTRACTS/20_ASYNC_RESULT_EVIDENCE.md#late-results), [R5 TX 후속](../../../../design/60_PSEUDOCODE/40_AUDIO_PREPARE_TX_FLOW.md#audiotx-advance), [R5 STREAM 현재 쓰기](../../../../design/60_PSEUDOCODE/50_STOP_LATE_UNCERTAIN_FLOW.md#stream-safe-return)
- [실제 참고 소스와 한계](../functions/AudioTx_Advance.md#source), [SW 후보 / HW Gate](../functions/AudioTx_Advance.md#feasibility), [5군 수명·capacity 상세](../functions/AudioTx_Advance.md#capacity-lifetime)
