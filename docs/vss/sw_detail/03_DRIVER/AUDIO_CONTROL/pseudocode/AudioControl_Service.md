# AudioControl_Service — 한국어 C형 의사코드

[VSS 지도](../../../01_VSS_MAP.md) · [DRIVER](../../00_LAYER_OVERVIEW.md) · [AUDIO_CONTROL](../00_MODULE_OVERVIEW.md) · [상세 설명](../functions/AudioControl_Service.md)

> CORE-15 · D5-2 SELF_CHECKED / INDEPENDENT_REVIEW_PENDING · GD5 전체 NOT_RUN. LOGICAL_R6 / C ABI BINDING_TBD / HW_UNVERIFIED / TEST_NOT_RUN.

<a id="contract"></a>
## R6 논리 원형

```text
void AudioControl_Service(
    OPTIONAL IN const VssDeviceControlIntent *intent,
    IN const VssTimeEvidence *time,
    OUT VssDeviceReadinessState devices[capacity -> written],
    OUT VssDeviceControlResult results[capacity -> written],
    OUT VssDiagnosticEvidence diagnostics[capacity -> written],
    OUT VssRecoveryPerformedEvidence performed[capacity -> written],
    OUT VssRecoveryVerificationEvidence verified[capacity -> written]);
```

[R6 정본](../../../../design/70_C_INTERFACE/00_HEADER_OWNERSHIP_MAP.md#r6-audiocontrol-service)을 정확히 한 번 표시합니다. 아래 C형 본문·한글 조건과 `군.written/capacity`는 **(의사연산)**입니다. 실제 C ABI/필드·helper 함수·새 자료 구조/Task/Queue가 아닙니다. 원 의도는 VssDeviceControlIntent, readiness는 VssDeviceReadinessState, 원 result와 복구 사실은 각각 기존 R3 타입입니다.

<a id="procedure"></a>
## C1. 원 수명·독립 OUT 준비

```c
void AudioControl_Service(위의_논리_인자) { // (의사연산), compile-ready C 아님
    // 기존 원 제어/결과/참조·정리 의무가 우선. 초기화/eviction 없음.
    for (devices, results, diagnostics, performed, verified 각_군) {
        if (주소_메타저장소_범위_산술_비간섭을_안전하게_검증한_유효군) {
            해당_군의_written = 0; // 검증 후에만, 군별 독립
        } else {
            무효군의_capacity_written_및_결과메모리_읽기쓰기_금지;
            그_군의_미인계원사실_참조_후속_보호;
        }
    }
    if (time == NULL || time의_외피접근_또는_필요시간비교_근거_무효) {
        시간의존_신규효력_보류; 원차단_참조_부족근거_보호;
        goto publish; // 원시각이 이미 보호된 사실의 안전 인계만
    }
    // 이미 수용한 제어의 bounded 후속은 아래 C5에서 항상 검토한다.
    if (intent == NULL) {
        새_HW명령_result_permission_및_Session_Attempt_생성_없음;
        goto followup; // 기존 작업 없으면 유효 현재 devices 관측만 가능
    }
```

<a id="admission"></a>
## C2. 신규·중복·충돌 의도

```c
    if (intent_외피를_안전하게_읽을_수_없음 ||
        원operation_device_configuration_action_유효성_또는_내부참조_검증실패) {
        가짜_원result_없이_확인가능한_신규거부진단만_보호;
        기존_ready_수용본_원결과_보존;
        goto followup;
    }
    if (intent->action != VSS_DEVICE_RECOVER && intent->recoveryPermission != NULL) {
        부적합_신규허용연결_거부; 기존_ready_원제어_보존; goto followup;
    }
    if (같은_operation이_이미_보호됨) {
        if (보호된_5필드_및_permission의_의미가_동일) {
            기존_원결과와_후속_재사용; HW_중복발행_없음;
        } else {
            새_충돌진단_격리; 기존_불변수용본_변경_없음;
        }
        goto followup;
    }
    if (신규_불변의도_내부permission_등록_결과의_보호예산_불충분) {
        새_수용과_HW효력_거부; 기존_원기록_덮기삭제_없음;
        goto followup;
    }
    // 원 operation은 STREAM 제출본이며 current key로 대체/재생성하지 않는다.
```

<a id="configuration"></a>
## C3. 구성별 PREPARE·INVALIDATE

```c
    if (intent->action == VSS_DEVICE_PREPARE &&
        같은_device_configuration의_현재_READY_실제근거가_유효) {
        실제_원의도를_수용보호; 그_원operation의_제어결과_보호;
        현_READY_재사용; 모든장치_reset_또는_매Attempt_재초기화_없음;
        goto followup;
    }
    if (intent->action == VSS_DEVICE_INVALIDATE) {
        실제_원의도를_수용보호;
        if (대상device_configuration이_현재_준비근거와_일치) {
            해당_근거만_VSS_DEVICE_INVALIDATED로_적용;
            실제_변경반영_완료뒤_그_revision_반영;
        } else {
            old_configuration의_근거_정리만; 다른_current_READY_변경없음;
        }
        원operation_device_configuration의_실제상태결과_독립보호;
        // 논리 무효화는 실제 HW reset·참조 종료가 아니다.
        goto followup;
    }
    if (intent->action == VSS_DEVICE_RECOVER) { goto recover; }
    if (intent->action != VSS_DEVICE_PREPARE) {
        새_거부진단_보호; goto followup;
    }
    goto protect_and_effect;
```

<a id="recovery"></a>
## C4. RECOVER 검사와 효력 전 등록

```c
recover:
    if (recoveryPermission_없음_또는_안전읽기불가 ||
        현재_permission_faultInstance_faultRevision_target_configuration_action_불일치_철회) {
        새_수행금지; 원제한_기존정리_진단_보호; goto followup;
    }
    if (실제_device_target_configuration_허용action_대응_미확인 ||
        old_PB전체retirement_provider_read_PCM_device_위험종료_미입증 ||
        남은_late기록의_alias없는_보호_미입증 ||
        기존_action매핑_및_별도효과검증기준_미정) {
        그_제품수행만_보류; 미결_안전근거_보호; goto followup;
    }
    // 같은 permission의 실제 동일 원 수행이 이미 진행되면 재발행하지 않는다.
    if (동일_permission의_원수행이_이미_제출_진행_완료됨) {
        원operation_수행결과_별도검증후속_계승; goto followup;
    }

protect_and_effect:
    원_5필드의도_실제참조와_독립결과_불변보호_확보;
    HAL이_원registration_operation_device_configuration_binding을_효력전에_보호;
    if (보호완료_실패) {
        새_HW효력_없음; current_READY_보존; 부분확보참조_원수명_추적;
        goto followup;
    }
    if (효력직전_대상구성_현재허용_및_실제자원안전_재검사_불충족) {
        새_HW효력_없음; 원보호_진단_후속유지; goto followup;
    }
    // 확인부터 효력까지의 실제 직렬화/원자성·시간·저장은 Binding TBD.
    기존_HAL경계로_허용된_원장치_제어를_연결;
    // 반환 전 즉시 callback도 사전 불변 원등록으로만 귀속.
    if (실제_reconfig_reset이_그_장치의_옛준비근거에_영향을_줌) {
        영향받는_그_옛근거만_무효화; 새_실제구성을_PREPARING으로_보호;
        실제_변경반영_완료뒤_revision_반영;
    }
    if (vendor_수락_또는_진행만_확인) {
        원_진행사실만_보호; READY_performed성공_verified성공_합성없음;
    }
    if (실제_부분효력_실패반환_또는_결과미확인) {
        원의도_등록_참조_불확실근거_진단_후속유지;
        무효력rollback_또는_무조건_FAILED_READY_합성없음;
    }
```

<a id="followup"></a>
## C5. 원 raw·세 장치의 후속

```c
followup:
    for (이번_bounded기회에_방문할_기존수용제어_보호raw_각각) {
        원registration_operation_device_configuration_binding_수명과_raw범위_대조;
        if (실제_누락_overflow_시각연속성_손실) {
            그_범위_LOST_보호; 확정READY_차단; continue;
        }
        if (개별원귀속_관측_시각_필요충분성_부족) {
            그_범위_INSUFFICIENT_보호; 확정READY_차단; continue;
        }
        if (old_device_configuration_operation의_후속) {
            그_원결과_정리_진단_참조에만_적용;
            current_READY_새Session_PCM_현재Fault_쓰기없음;
            continue; // old 복구 사실도 C6의 원 귀속 인계 대상
        }
        if (그_현재장치_구성의_실제준비_충분성_확인) {
            해당_readiness.state = VSS_DEVICE_READY;
        } else if (그_현재장치_구성의_실제준비실패_확인) {
            해당_readiness.state = VSS_DEVICE_FAILED;
            실제_원producer_실패진단_독립보호;
        } else {
            실제_UNASSESSED_PREPARING_INVALIDATED_상태와_부족후속_유지;
        }
        실제_변경반영_완료뒤에만_그_revision_반영;
        실제_수용원operation_device_configuration_state_제어결과_불변보호;
        // 한 장치 완료로 다른 장치 상태/전체 성공을 쓰지 않는다.
    }
    현재_device_configuration이_확인되고_적용완료한_안정관측만_devices후보로_보호;
    // mute/ACK/timeout·READY는 actual/NO_START/termination/safe-return 아님.
```

<a id="recovery-facts"></a>
## C6. 실제 수행과 별도 효과 사실

```c
    for (방문할_보호된_원recovery의_실제수행_또는_별도효과관측_각각) {
        원permission_6필드_target_configuration_operation_인과와_실제시각_대조;
        if (실제_원수행_근거가_있고_R3필수문맥을_표현가능) {
            performed의_원permission_target_configuration_operation을_불변보호;
            effect와_occurredAt은_그_실제수행단계의_확인범위만;
            // 수용/ACK만으로 SUCCEEDED 없음.
        }
        if (실제수행뒤_별도효과관측과_performedOperation_원인과_충분 &&
            R3필수문맥과_verifiedAt을_실제근거로_표현가능) {
            verified의_원permission_performedOperation_target_configuration을_불변보호;
            effect와_verifiedAt은_별도실제검증범위만;
        }
        원_수행_검증을_독립보호; 전달순서와_실제수행후_관측순서_구분;
        if (현재permission_철회_revision_target_configuration_action_변경) {
            새수행_금지; old_사실은_원정리_진단_인계만;
        }
        // HEALTH만 현재 허용과 pair를 대조하여 제한 해제. CONTROL clear 없음.
    }
```

<a id="publish"></a>
## C7. 유효 군별 전달과 종료

```c
publish:
    for (devices, results, diagnostics, performed, verified 각_군을_독립적으로) {
        for (그_군의_보호된_원미인계항목_각각_기존bounded예산안에서) {
            if (그_군의_유효성_선행검증_성공 &&
                그_군.written < 그_군.capacity &&
                실제_항목과_전달후_내부참조_수명보호_가능) {
                그_군의_다음유효자리에_원타입항목_기록;
                실제_기록성공뒤_그_군.written만_증가;
            } else {
                그_원사실_참조_미완료의무_보호; break;
                // break는 이 군만. 다른 OUT군은 계속한다.
            }
        }
    }
    if (기존_bounded보호도_실제손실) {
        손실범위_LOST_최소관측_진단_후속을_기존경계에_남김;
        충분성판정_차단; 신규효력_제한; 기존원기록_eviction_무한pool_없음;
        // loss 수단 자체도 용량/원자성 실증 전 무손실 보장을 하지 않는다.
    }
    consumer반영_또는_독립보호추적완료전_원사실_참조_폐기없음;
    // HAL 등록 회수는 HAL의 HW/대기/처리/late refs 종료 조건, 별도.
    return; // 통합 성공/준비 완료·Fault clear·참조 종료가 아니다.
}
```

<a id="postconditions"></a>
## 사후조건과 정본

유효한 각 군만 0≤written≤capacity를 만족합니다. NULL·신규 거부도 기존 수용 작업과 원 facts 보호를 유지합니다. 현재 config와 old 결과, 세 장치, 수행/검증과 전달 count를 분리합니다. HAL 원등록 수명은 readiness 무효화·PCM 반환·PB retirement와 다릅니다. 실제 C/물리 구현과 Firmware/Host/Board 시험은 수행하지 않습니다.

[설명 원형/수명](../functions/AudioControl_Service.md#contract) · [구성 전환](../functions/AudioControl_Service.md#configuration) · [복구](../functions/AudioControl_Service.md#recovery) · [OUT](../functions/AudioControl_Service.md#publish) · [F1~F6/미결](../functions/AudioControl_Service.md#feasibility) · [R2](../../../../design/30_FUNCTIONS/50_DRIVER_HAL_FUNCTIONS.md#audiocontrol-service) · [R3 Intent/결과](../../../../design/40_DATA/60_AUDIO_TX_DATA.md#vssdevicecontrolintent) · [R3 수행](../../../../design/40_DATA/70_DIAGNOSTIC_DATA.md#vssrecoveryperformedevidence) · [R3 검증](../../../../design/40_DATA/70_DIAGNOSTIC_DATA.md#vssrecoveryverificationevidence) · [R4 수명](../../../../design/50_CONTRACTS/10_OWNERSHIP_LIFETIME.md#references-and-release) · [R4 복구](../../../../design/50_CONTRACTS/40_FAULT_RECOVERY.md#current-target) · [R5 Startup](../../../../design/60_PSEUDOCODE/10_STARTUP_FLOW.md#audiocontrol-service) · [R5 복구](../../../../design/60_PSEUDOCODE/60_FAULT_RECOVERY_FLOW.md#device-recovery) · [R6 표기](../../../../design/70_C_INTERFACE/00_HEADER_OWNERSHIP_MAP.md#prototype-notation) · [HAL 의사코드](../../../04_HAL_BSP/AUDIOHAL_BOUNDARY/pseudocode/AudioHAL_Callback.md#procedure)
