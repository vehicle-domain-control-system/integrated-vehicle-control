# Health_Evaluate — 한국어 C형 의사코드

[VSS 지도](../../../01_VSS_MAP.md) · [SERVICE](../../00_LAYER_OVERVIEW.md) · [HEALTH](../00_MODULE_OVERVIEW.md) · CORE-17 · [상세 설명](../functions/Health_Evaluate.md)

> GUIDE · CORE-17 · R6 논리 계약 / 실제 C ABI BINDING_TBD / HW_UNVERIFIED / TEST_NOT_RUN.

<a id="contract"></a>
## R6 논리 원형

```text
VssHealthChange Health_Evaluate(
    IN const VssDiagnosticEvidence diagnostics[count],
    IN const VssRecoveryPerformedEvidence performed[count],
    IN const VssRecoveryVerificationEvidence verified[count],
    IN const VssPlaybackObservation *playback,
    IN const VssDeviceReadinessState devices[count],
    IN const VssBackendCleanupObservation cleanup[count],
    IN const VssReadBasis *basis,
    OUT VssFaultState faults[capacity -> written],
    OUT VssRecoveryPermission permissions[capacity -> written]);
```

[R6](../../../../design/70_C_INTERFACE/00_HEADER_OWNERSHIP_MAP.md#r6-health-evaluate)의 독립 count/OUT을 유지한다. 아래 배열의 `[count]`·군.written/capacity와 한글 조건·대괄호 문장은 **의사 연산**이다. count는 군별 실제 길이, OUT metadata는 설명용 접근이며 C 필드/확정 ABI가 아니다. 새 helper/API·enum·집합 구조·Task/Queue/manager를 선언하지 않는다. Caller는 FLOW, Callee는 필요한 기존 Runtime뿐이며 HEALTH 현재/최근 Fault·제한·허용만 쓴다.

<a id="procedure"></a>
## H1~H7 — 보호·현재 대상·pair·독립 OUT

```c
VssHealthChange Health_Evaluate(
    /* IN */ const VssDiagnosticEvidence diagnostics[count],
    /* IN */ const VssRecoveryPerformedEvidence performed[count],
    /* IN */ const VssRecoveryVerificationEvidence verified[count],
    /* IN */ const VssPlaybackObservation *playback,
    /* IN */ const VssDeviceReadinessState devices[count],
    /* IN */ const VssBackendCleanupObservation cleanup[count],
    /* IN */ const VssReadBasis *basis,
    /* OUT */ VssFaultState faults[capacity -> written],
    /* OUT */ VssRecoveryPermission permissions[capacity -> written])
{
    // H1: owner 원본 초기화 없음. metadata/범위 검증이 역참조보다 먼저.
    [faults/permissions 주소·metadata·산술 범위·비간섭을 각각 안전 검증];
    if (faults군_유효) { faults.written = 0; }
    if (permissions군_유효) { permissions.written = 0; }
    [무효군의 written/capacity·배열은 읽기쓰기 없음];
    [각 IN count 전달과 양수 범위·내부 참조를 읽기 전에 독립 검증];
    [부족한 군은 읽지 않고 원producer 보호·재확인/실손실 근거 유지];
    // count=0: 자료 없음. diagnostics0 clear/performed0 성공/verified0 clear 없음.
    // devices0/cleanup0을 READY·전체 안전으로 합성하지 않는다.

    // H2: bounded 원진단. 입력 품질/미평가와 실제 출력 실패를 구분.
    for (이번_예산의_안전하게_읽을_원diagnostics_각각) {
        [producer/stage/cause/target/configuration/operation/occurredAt 불변 보호];
        if (INPUT_INVALID_STALE_NO_DATA의_의미만 || startup_device_UNASSESSED만 ||
            WINDOW_PRODUCT_보류만) {
            [원 진단만 유지; 출력 Fault/DEGRADED/UNAVAILABLE 합성 없음];
            continue;
        }
        if (!확인된_실패 || 승인된_cause_Fault_제한_연결근거_미정) {
            [원진단·필요 제한판단 미결 보존; 임의 Fault/reset/retry 없음];
            continue;
        }
        if (!원diagnosis_현재최근_추적수명과_적용자료_보호가능) {
            [신규 적용 완료 주장 없음; 원producer 보호·부족/손실·후속 유지];
            continue;
        }
        [기존 Fault5 중 근거 있는 kind와 instance/target/configuration 보호];
        [새 instance의 reportOccurrence/firstOccurredAt를 원 진단에서 최초 보호];
        [중복 진단은 번호/최초시각 불변; 공간/시간 부족은 완전성 미확인 기록];
        [diagnosis/restriction을 보호해 CURRENT·실제 revision 반영 완료];
        [다른 current/recent 그대로; 실제 변경과 남은 미결을 독립 관측];
    }
    // Fault5: SOUND_ASSET_UNAVAILABLE / PLAYBACK_START_FAILURE /
    // AUDIO_OUTPUT_FAILURE / PLAYBACK_STATE_FAILURE / INITIALIZATION_FAILURE.

    // H3: 원 PLAYBACK 불명확 최종은 diagnostics 수신 유무와 독립.
    if (안전한_playback의_반영완료_원귀속_불명확최종이_확인됨) {
        [기존 PLAYBACK_STATE_FAILURE + FAULT + UNAVAILABLE 제한 보호·반영];
        [해당 새 Fault의 reportOccurrence 및 PB 원 final 시각을 firstOccurredAt로 보호];
        [처리 now 보충 금지; 원시각 부족은 제한 유지/새 Status 이력 투영 제한];
        [다른 current/recent 보존; timeout/일반 START 대기를 최종으로 판정 없음];
    } else if (필수_playback_저장소_관측_원인과가_부족) {
        [기존 제한과 부족 유지; 정상 PB·새 final 합성 없음];
    }

    // H4: 보호된 수행/검증 원사실은 독립. 처리 now로 원 시간 갱신 없음.
    for (이번_예산의_안전한_performed_각각) {
        [원 permission/target/configuration/operation/effect/occurredAt 보호];
    }
    for (이번_예산의_안전한_verified_각각) {
        [원 permission/performedOperation/target/configuration/effect/verifiedAt 보호];
    }
    if (basis == NULL || basis_및_stamps_time_안전읽기불가 ||
        basis->validity != VSS_READ_VALID ||
        현재_target_config_필요owner반영_인과_시간근거_부족_또는_변경) {
        [현재 제한·old 허용 원값 보존; 새 수행 허용/해제 보류];
        [현재 유효성을 확인할 수 없는 허용은 permissions OUT에 공개하지 않음];
        [이미 안전 반영한 진단/불명확 최종 제한 rollback 없음];
        goto reconfirm_publish;
    }

    // H5: 현재 허용과 철회. Session/Attempt나 실제 HW 수행 생성 없음.
    for (이번_예산에서_현재_대상별_허용을_검토할_Fault_각각) {
        if (새_instance_revision_config_action_또는_철회로_old허용_무효) {
            [old 허용의 새 수행 금지; 불변 원값·old 결과/진단 유지];
            [현재 유효 허용 관측만 공개; 실제 변경 revision 반영];
        }
        if (기존정책상_recoverable_및_현재허용조건_충족 &&
            원6필드_허용과_후속수명_보호가능) {
            [permission/faultInstance/faultRevision/target/configuration/action 보호];
            [실제 현재 허용만 반영; 중복 발급/가짜 performed/verified 성공 없음];
        } else {
            [원 제한·미결 유지; 임의 action/reset/timeout/retry 매핑 없음];
        }
    }
    // FLOW 안전 연결→Backend 실제 performed→별도 verified→HEALTH 해제.
    // FLOW/STREAM/Driver는 효력 직전 현재 허용·대상·old 위험 종료 재검사.

    // H6: 현재 호출의 두 군에서 확인되는 원 pair만. one-sided는 clear 없음.
    if (performed.count == 0 || verified.count == 0) {
        [전달된 원사실 독립 보호; 현재 제한 유지·짝 후속 재확인];
    } else {
        for (이번_예산의_실제_대응하는_원performed_verified_pair_각각) {
            if (현재_permission_key와_양쪽_permission_불일치 ||
                현재_Fault_instance_revision과_허용의_instance_revision_불일치 ||
                현재_target_configuration_action_허용연결_불일치 ||
                performed.operation != verified.performedOperation) {
                [old/다른 대상 원사실·진단 격리; 현재 Fault clear 없음];
                continue;
            }
            if (performed.effect != VSS_RECOVERY_SUCCEEDED ||
                verified.effect != VSS_RECOVERY_SUCCEEDED ||
                !원performed이후_별도verified의_domain_epoch_구간_인과_효과_충분 ||
                실제_누락_LOST_또는_부분효과_또는_해제직전_현재조건_변경) {
                [FAILED/UNCONFIRMED/부분·부족/손실과 원 pair·현재 제한 보존];
                continue; // ACK/수행 성공/READY/무음만으로 verified 성공 없음.
            }
            if (!해제와_recent이력_허용종료의_보호반영_완료가능) {
                [현재 제한·pair 보호; 부분 clear 완료 공개 없음];
                continue;
            }
            [일치하는 그 현재 Fault/대상의 해당 제한만 해제];
            [기존 정책대로 recent 보존; 다른 current/recent 그대로];
            [해당 permission 종료/무효화와 실제 revision 반영 완료];
            [FLOW 최신 후보 별도 재평가 필요성만 남김];
        }
        [짝 없는 다른 수행/검증·미방문 사실도 원 보호/후속 유지];
    }
    // STORE/PB final/replay·age/Hold/UseLimit·PCM offset 변경/자동START 없음.
    goto normal_publish;

reconfirm_publish:
    [아래 H7의 두 군 독립 인계를 수행];
    return VSS_HEALTH_RECONFIRM_REQUIRED; // H4 부족 분기, 원 변화는 보존.

normal_publish:
    [아래 H7의 두 군 독립 인계를 수행];
    if (현재제한_허용_해제의_실제반영완료_변경있음) {
        return VSS_HEALTH_CHANGED;
    }
    if (필요판단의_미결_짝부족_또는_재확인_남음) {
        return VSS_HEALTH_RECONFIRM_REQUIRED;
    }
    return VSS_HEALTH_UNCHANGED;
}
```

<a id="outputs"></a>
## H7 — 두 반환 경로에서 사용하는 동일 독립 인계 절차

H7은 이 함수의 내부 설명이며 호출 가능한 새 helper가 아니다. faults와 permissions를 각각 한 번, 기존 bounded 예산 안에서 처리한다.

```c
for (faults와 permissions의_각_군을_독립적으로) {
    for (그_군의_보호된_유효_미인계관측_각각) {
        if (!그_군_안전검증_통과) {
            [무효 written/capacity까지 무접근; 원 사실/후속 보호]; break;
        }
        if (그_군.written >= 그_군.capacity || !실제기록과_참조보호_가능) {
            [capacity0/full·보호 부족은 미인계; 다른 군은 계속]; break;
        }
        [그_군의 다음 유효 위치에 원 typed 관측 기록·후속 참조 보호];
        if (실제기록과_참조보호_완료) { 그_군.written += 1; }
        else { [부분기록 유효 공개 없음; 원 사실/손실·후속 보호]; break; }
    }
}
```

<a id="postconditions"></a>
## 사후조건과 수명

현재/최근 Fault의 diagnosis와 허용 원자료는 소비자 반영/독립 보호 추적 종료까지 유지한다. OUT의 얕은 복사·written·반환은 ACK가 아니다. 독립 OUT 일부를 전체 current 집합·clear 완료로 쓰지 않는다. 보호 공간 부족을 기존 기록 eviction·무한 pool로 해결하지 않는다. 실제 storage/직렬화·key alias·원자성은 TBD이며 손실을 충분/성공으로 숨기지 않는다.

단독 performed/verified는 제한 유지, 둘이 있어도 현재 instance/revision/대상·operation·원 시간 인과·효과 충분성을 전부 확인한다. Fault5·불명확 최종 제한은 유지하고 다른 owner를 쓰지 않는다. F1/F3 논리 후보만 기술했으며 Host/Firmware/Build/Flash/Board NOT_RUN, P2 HOLD다.

[반환·인자 수명](../functions/Health_Evaluate.md#contract) · [원진단/Fault5](../functions/Health_Evaluate.md#diagnostics) · [현재 pair](../functions/Health_Evaluate.md#pair) · [독립 OUT](../functions/Health_Evaluate.md#outputs) · [R2](../../../../design/30_FUNCTIONS/60_HEALTH_RUNTIME_FUNCTIONS.md#health-evaluate) · [R3](../../../../design/40_DATA/70_DIAGNOSTIC_DATA.md#vssrecoverypermission) · [R4](../../../../design/50_CONTRACTS/40_FAULT_RECOVERY.md#current-target) · [R5](../../../../design/60_PSEUDOCODE/60_FAULT_RECOVERY_FLOW.md#health-evaluate) · [R6](../../../../design/70_C_INTERFACE/00_HEADER_OWNERSHIP_MAP.md#r6-health-evaluate) · [보고의 읽기 전용 경계](Health_BuildStatus.md#ownership)
