# AudioStream_RequestControl — 한국어 C 형태 의사코드

[VSS 지도](../../../01_VSS_MAP.md) / [SERVICE](../../00_LAYER_OVERVIEW.md) / [AUDIO_STREAM](../00_MODULE_OVERVIEW.md) / CORE-11 · [상세 설명](../functions/AudioStream_RequestControl.md)

> GUIDE · 한국어 조건·대괄호 절차와 CALL은 설명용 논리 연산이다. 새 helper/API·자료 구조·Task/Queue·실제 C ABI를 선언하지 않는다.

<a id="contract"></a>
## R6 논리 계약

```text
VssRequestDisposition AudioStream_RequestControl(
    ONE_OF const VssPlaybackControlCommand *playback,
           const VssRecoveryPermission *recovery,
    IN const VssPlaybackObservation *currentPlayback,
    IN const VssCandidateObservation candidates[count],
    IN const VssFaultState restrictions[count],
    IN const VssDeviceReadinessState devices[count],
    IN const VssBackendCleanupObservation cleanup[count],
    IN const VssReadBasis *conditionsBasis,
    OUT VssAudioRequestResult requests[capacity -> written],
    OUT VssDiagnosticEvidence diagnostics[capacity -> written]);
```

동기 `VssRequestDisposition`과 독립 `VssAudioRequestResult`/`VssDiagnosticEvidence`를 구분한다. [R6 원형](../../../../design/70_C_INTERFACE/00_HEADER_OWNERSHIP_MAP.md#r6-audiostream-requestcontrol)의 [count/OUT 후보 전개](../../../../design/70_C_INTERFACE/00_HEADER_OWNERSHIP_MAP.md#prototype-notation)를 아래에서 보여준다. `provider/tx/intent/원_사실`은 기존 R3 자료의 설명용 참조다. `conditionsBasis->time`은 기존 `VssReadBasis`의 시간 근거이며 새 time IN을 추가하지 않는다.

<a id="procedure"></a>
## lane 검사·START·Session 전체 STOP·Recovery 절차

```c
VssRequestDisposition AudioStream_RequestControl(
    const VssPlaybackControlCommand *playback,
    const VssRecoveryPermission *recovery,
    const VssPlaybackObservation *currentPlayback,
    const VssCandidateObservation *candidates, SemanticCount candidateCount,
    const VssFaultState *restrictions, SemanticCount restrictionCount,
    const VssDeviceReadinessState *devices, SemanticCount deviceCount,
    const VssBackendCleanupObservation *cleanup, SemanticCount cleanupCount,
    const VssReadBasis *conditionsBasis,
    VssAudioRequestResult *requests,
    SemanticCount requestsCapacity, SemanticCount *requestsWritten,
    VssDiagnosticEvidence *diagnostics,
    SemanticCount diagnosticsCapacity, SemanticCount *diagnosticsWritten)
{
    // C1: 메타 저장소도 안전 검증 전에 읽거나 쓰지 않는다.
    [각 OUT의 주소/전달영역/capacity/written 저장소/산술/비간섭을 독립 확인];
    if (requests군_안전) { *requestsWritten = 0; }
    if (diagnostics군_안전) { *diagnosticsWritten = 0; }
    if ((playback == NULL && recovery == NULL) ||
        (playback != NULL && recovery != NULL)) {
        [새 하위 호출/owner 효력 없음; 원 참조·기존 결과/차단 보존];
        return VSS_REQUEST_REJECTED;
    }
    if (선택lane_외피_및_필수_원identity_안전검증_실패) {
        return VSS_REQUEST_REJECTED; // 가짜 Session/Attempt/permission 없음.
    }
    [optional PB/basis 및 count>0 배열의 접근 가능 영역을 읽기 전에 검증];
    [접근 가능한 내부 plan/Meta/stamps 등도 실제 borrow 범위 확인];
    [부족/무효 관측은 정상값으로 채우지 않고 해당 판단의 부족으로 보존];

    VssRequestDisposition disposition;
    if (recovery != NULL) { goto recovery_lane; }
    if (playback->action == VSS_AUDIO_STOP) { goto stop_lane; }
    if (playback->action != VSS_AUDIO_START) {
        [유효 원 playback 귀속일 때만 REQUEST_REJECTED 사실 보호];
        disposition = VSS_REQUEST_REJECTED;
        goto publish;
    }

    // C2: START의 같은 원 prepared 연결과 최신 실행 조건.
    if (playback->session_attempt가_보호된_provider_preparation과_불일치 ||
        원_plan_position_asset_구성의_동일성_위반) {
        [현재 provider/새 Session 불변; 확인 가능한 원 거부·old 진단 보호];
        disposition = VSS_REQUEST_REJECTED;
        goto publish;
    }
    if (!provider.productionAllowed || 원Session_STOP_취소_차단 ||
        provider.phase == VSS_PROVIDER_CLOSING) {
        [차단 유지; 적법한 원 REQUEST_REJECTED 단계 보호; 새 START 없음];
        disposition = VSS_REQUEST_REJECTED;
        goto publish;
    }
    if (동일_실제원요구의_START가_이미_제출_또는_진행됨) {
        [확인된 원 operation으로만 후속 연결; 새 HW 명령 없음];
        [operation key/수용 미확인도 원 추적 유지; tuple만으로 다른 구간 합침 없음];
        disposition = VSS_REQUEST_KEPT;
        goto publish;
    }
    if (provider.phase != VSS_PROVIDER_PREPARED ||
        필수_borrow_관측_basis_시간_현재_device_config_최신제한_근거_부족) {
        [기존 참조/준비 후속·조건 재수집 유지; 준비/actual 합성 없음];
        disposition = VSS_REQUEST_WAIT;
        goto publish;
    }

    VssTxControlCommand tx;
    tx.attempt = playback->attempt;
    tx.action = VSS_TX_START;
    [tx.scope를 원 준비의 실제 SEGMENT 또는 ATTEMPT 범위에 연결];
    if (원_미시작_One_shot의_최초START_gate가_필요) {
        tx.firstStartMeta = 원_preparation의_보호된_firstStartMeta;
        if (원_Meta_시간비교_및_최초허용_근거_미확인) {
            disposition = VSS_REQUEST_WAIT;
            goto publish;
        }
    } else {
        tx.firstStartMeta = NULL; // 적법 Started 후속 cue: age gate 재적용 없음.
    }
    [원 요구/Meta/scope 및 실제 하위 operation·결과·진단의 수명 보호 확보];
    if (효력전_원참조_또는_결과보호_공간_부족) {
        disposition = VSS_REQUEST_WAIT;
        goto publish;
    }
    // C3: 보호 뒤 효과 직전 실제 owner 권한·자원/현재 basis를 다시 대조.
    if (원귀속_최신조건_제한_장치config_시간_보호예산이_바뀜 ||
        !provider.productionAllowed || 원Session_STOP_차단) {
        [새 TX 호출 없음; 기존 보호/차단 유지];
        disposition = VSS_REQUEST_WAIT;
        goto publish;
    }
    [원 CONTROL_RECEIVED 단계 사실을 불변 보호];
    VssTxRequestState txState = CALL AudioTx_Request(
        NULL, &tx, NULL, targetOperations[count=0],
        &conditionsBasis->time,
        txRequests[capacity -> written], returned[capacity -> written],
        txDiagnostics[capacity -> written]);
    // PCM=NULL: START는 PCM handoff가 아니다. 실제 R6 OUT/시간 경계를 모두 표시.
    if (원operation_생성전_확인된_새무효력거부) {
        [원 STREAM REQUEST_REJECTED만 보호; fake TX result/return 없음];
    } else {
        for (이번_실제_원operation에_귀속된_txRequests 각각) {
            [원 operation/attempt·보호된 control/scope 인과를 대조];
            if (귀속_또는_참조_부족) {
                [부족/LOST·진단·retain·후속 보호; current로 재귀속 없음];
                continue;
            }
            switch (원_txRequest.state) {
            case VSS_TX_REJECTED_WITH_NO_ACCESS:
                [이 원 요구의 REQUEST_REJECTED 사실 보호; 다른 old 작업 유지];
                break;
            case VSS_TX_SUBMITTED_OR_QUEUED:
                [원 START_SUBMITTED 단계만 보호];
                break;
            case VSS_TX_ACCEPTED_AND_ENABLED:
                [원 START_ACCEPTED_AND_ENABLED 사실 보호; actual 생성 없음];
                break;
            case VSS_TX_EFFECT_UNCERTAIN:
                [원 EFFECT_UNCERTAIN·부분효력/retain·정리 보호];
                break;
            default:
                [확인된 CONTROL_RECEIVED 또는 원 단계만 보호; 제출/성공 합성 없음];
                break;
            }
        }
        [동시에 확인된 submitted/accepted/partial 원 facts 모두 별도 보존];
    }
    [동기 txState만으로 operation 없는 typed facts 또는 actual 만들지 않음];
    [returned/진단은 원 typed 수명으로 보호; 제어 operation에 PCM return 강제 없음];
    disposition = VSS_REQUEST_ADOPTED; // STREAM 의도 수용. TX 성공이 아니다.
    goto publish;

stop_lane:
    // C5: stop.attempt는 anchor. 유효한 원 Session STOP 의도를 먼저 보호한다.
    if (playback->session_attempt의_원연결을_기존문맥에서_확인할_수_없음) {
        [귀속 부족/원 진단 유지; 다른 current Session을 차단하지 않음];
        disposition = VSS_REQUEST_REJECTED;
        goto publish;
    }
    [기존 STREAM 제어 문맥에 이 원 Session의 STOP 의도를 먼저 보호];
    [미방문 관련 Attempt까지 decode/refill/새 sound handoff 금지 적용];
    disposition = 기존_동일STOP_차단이면_VSS_REQUEST_KEPT_아니면_VSS_REQUEST_ADOPTED;
    for (이_원Session에_관련된_기존_provider 각각) {
        provider.productionAllowed = false;
        provider.phase = VSS_PROVIDER_CLOSING;
        [원 bytes/PCM·진행중 decoder·하위 retain·미인계 사실 보호 유지];
    }
    [진행중 producer가 HANDOFF_PENDING/하위 효력 직전 STOP을 재확인하게 연결];
    // basis/device/공간 부족 때문에 이미 적용한 차단을 rollback하지 않는다.
    for (이번_bounded_기회에_방문할_원Session_관련_미완료_범위 각각) {
        [보호된 prepare/control/handoff와 공개 TX 사실로 실제 관련 대상을 대조];
        if (원Attempt_scope_실제operation_key_및_잔류범위_대응_부족 ||
            하위조건_시간borrow_또는_새결과보호_공간_부족) {
            [그 원대상의 retain·부족/진단·차단·Advance 후속 정리 유지];
            continue;
        }
        VssTxControlCommand stopControl;
        stopControl.attempt = 이_대상의_확인된_원Attempt;
        stopControl.action = VSS_TX_STOP;
        stopControl.scope = 확인된_원_SEGMENT_또는_ATTEMPT_scope;
        stopControl.firstStartMeta = NULL;
        [실제 구간과 관련 old targetOperations key 범위를 불변 보호];
        [같은 scope enum의 다른 SEGMENT를 합치지 않음; 목록 누락은 종료 아님];
        if (이미_이_원STOP요구가_제출됨 && 원operation_key_미확인) {
            [원 수용/키 확인 후속만 유지; 새로운 STOP HW 동작 중복 발행 없음];
            continue;
        }
        [계속이면 확인된 continuingOperation, 새 원 요구이면 NULL을 선택];
        CALL AudioTx_Request(
            NULL, &stopControl, 확인된_원STOP_operation_또는_NULL,
            원_targetOperations[count], 보호된_유효time,
            txRequests[capacity -> written], returned[capacity -> written],
            txDiagnostics[capacity -> written]);
        [새 STOP control operation과 정리 대상 old operations를 구분해 결과 보호];
        if (하위_거부_partial_지연_또는_확인부족) {
            [생산차단 유지; EFFECT_UNCERTAIN 등 실제 원 단계·진단/retain 후속 유지];
        }
        [한 ACK/idle/abort/mute로 NO_START/TERMINATION/PCM return/whole 성공 생성 없음];
    }
    [예산 끝·미방문/미확인 대상도 Session 차단과 기존 Advance 후속으로 보호];
    [안전 반환은 old 정리만; 현재 쓰기 미증명인 zero-fill도 금지];
    [다른 old Session의 STOP/결과는 그 old 정리만, 새 provider/PCM 불변];
    [정상 STOP만으로 PREPARE_FAILED/Fault 합성 없음];
    goto publish;

recovery_lane:
    // C8: 6필드 현재 permission. PB command/Session/Attempt를 만들지 않는다.
    [원 permission/faultInstance/faultRevision/target/configuration/action을 대조];
    if (현재_HEALTH허용_철회_또는_6필드_대상관계_부적합) {
        [새 수행 없음; 원 제한/old 정리/진단 유지];
        disposition = VSS_REQUEST_REJECTED;
        goto publish;
    }
    if (필수_현재허용_basis_시간_관측_부족 ||
        old_전체retirement_및_provider_read_PCM_device_실제위험종료_미입증 ||
        남은_late기록의_alias없는_보호_미입증) {
        disposition = VSS_REQUEST_WAIT;
        goto publish;
    }
    if (동일_permission의_원수행이_이미_제출_진행_또는_완료됨) {
        [기존 operation/원 결과·별도 검증 후속 유지; 중복 명령 발행 없음];
        disposition = VSS_REQUEST_KEPT;
        goto publish;
    }
    [6필드 원 허용 및 operation/결과·내부 참조의 실제 후속 수명 보호];
    if (보호_공간_부족) {
        disposition = VSS_REQUEST_WAIT;
        goto publish;
    }
    if (현재_permission_revision_target_config_action_또는_안전조건이_바뀜) {
        [수행 직전 재검사 실패; 새 호출 없음];
        disposition = VSS_REQUEST_WAIT;
        goto publish;
    }
    if (허용action_실제Driver매핑_및_효과검증기준_미정) {
        [그 제품 수행만 BINDING_TBD; 독립 ONE_OF/중복/허용 논리는 계속 보존];
        disposition = VSS_REQUEST_WAIT;
        goto publish;
    }
    if (기존_장치제어_대상) {
        VssDeviceControlIntent intent;
        [intent.operation을 원 허용의 실제 개별 수행 key로 효력 전에 보호];
        [intent.device/configuration을 현재 permission의 확인된 대상에 연결];
        intent.action = VSS_DEVICE_RECOVER;
        intent.recoveryPermission = 보호된_원permission;
        CALL AudioControl_Service(
            &intent, &conditionsBasis->time,
            controlDevices[capacity -> written], controlResults[capacity -> written],
            controlDiagnostics[capacity -> written],
            lowerPerformed[capacity -> written], lowerVerified[capacity -> written]);
        [void/수용을 performed/verified 성공으로 만들지 않고 원 facts를 보호];
    } else {
        [확인된 허용 범위의 기존 Backend/Driver 연결만 수행; 새 reset/API 없음];
    }
    [Advance에 원 수행 operation·안전/permission·독립 결과 수명 연결];
    disposition = VSS_REQUEST_ADOPTED;
    // Recovery의 requestsWritten은 유효 군에서 0 유지.
    goto publish;

publish:
    if (playback != NULL) {
        for (이_원재생제어에_연결된_보호된_미인계_stage_사실 각각) {
            if (!requests군_안전 || *requestsWritten >= requestsCapacity) {
                [미인계 원 session/attempt/stage·참조와 후속 의무 유지];
                break;
            }
            [requests[*requestsWritten]에 실제 불변 원 사실을 기록하고 인계 수명 보호];
            if (실제_기록과_필요참조보호_성공) { *requestsWritten += 1; }
            else { [미인계 유지]; break; }
        }
    }
    // Recovery lane은 requests를 초기 0 외에 쓰지 않는다.
    for (보호된_이_원제어_또는_복구의_미인계_진단 각각) {
        if (!diagnostics군_안전 || *diagnosticsWritten >= diagnosticsCapacity) {
            [원 producer/stage/cause/target/config/operation/시각·coverage/참조 보호];
            break;
        }
        [diagnostics[*diagnosticsWritten]에 원 사실과 실제 후속 참조를 인계];
        if (실제_기록과_참조보호_성공) { *diagnosticsWritten += 1; }
        else { [미인계/실제 손실 근거 유지]; break; }
    }
    return disposition;
}
```

<a id="call-boundaries"></a>
## 하위 효과·결과 연결

`AudioTx_Request`는 PCM/control ONE_OF, continuingOperation, targetOperations, time 및 requests/returned/diagnostics를 갖는 [기존 R6](../../../../design/70_C_INTERFACE/00_HEADER_OWNERSHIP_MAP.md#r6-audiotx-request)다. 새 START control은 PCM=NULL/target count=0, STOP은 실제 확인된 관련 old key 범위다. 원 command와 결과는 효력 전에 보호하고 이미 효력 있는 요구를 NULL continuingOperation으로 재발행하지 않는다. 실제 멱등·직렬화 수단은 TBD다.

`AudioControl_Service`의 device/result/diagnostic/performed/verified도 [기존 R6](../../../../design/70_C_INTERFACE/00_HEADER_OWNERSHIP_MAP.md#r6-audiocontrol-service)이며 STREAM은 readiness writer가 아니다. 어느 CALL의 scalar/void·빈 결과도 실제 STOP/actual/복구 성공이 아니다. 각 원 lower facts는 [Advance](../functions/AudioStream_Advance.md#lower-results)로 이어가며 잠시 빌린 OUT 주소를 장기 보관하지 않는다.

<a id="postconditions"></a>
## 분기별 사후 조건과 현재 안전

START는 같은 원 준비·권한과 보호가 있을 때만 새 control을 발행한다. STOP은 아직 방문 못 한 Attempt까지 미래 생산 차단을 유지하고 하위 실패/partial/안전 반환에도 되돌리지 않는다. Recovery는 6필드 허용·old retirement/실제 자원 안전을 재확인하며 requests.written=0, 수행/검증은 Advance의 별도 자료다.

유효 OUT의 written은 실제 인계 건수이며 requests/diagnostics를 동일 count로 강제하지 않는다. invalid/full/0이면 producer의 미인계 사실·참조는 유지한다. OLD 귀속은 OLD 정리/진단에만 적용한다. `SOURCE_CONSUMED ≠ SAFE_RETURN ≠ OUTPUT_TERMINATED ≠ WHOLE_SESSION_RETIRED`이고 zero-fill도 현재 next-DMA 쓰기 안전을 우회하지 않는다.

<a id="gates"></a>
## 미정과 정본

ONE_OF·OUT·원 identity·STOP 선차단/Session 전체 범위·복구 중복/6필드·원 단계 수명은 독립 Host 후보이며 이번 실행 NOT_RUN이다. 효과 직전 검사에서 하위 효력까지의 원자성·시간/저장 ABI·실제 safe-write/STOP fence·Recovery action의 실물 충분성은 BINDING_TBD/HW_UNVERIFIED다. 새 값/상태/API를 추가해 부족 근거를 채우지 않는다.

[상세 입력/반환](../functions/AudioStream_RequestControl.md#contract) · [상세 START](../functions/AudioStream_RequestControl.md#start) · [상세 STOP](../functions/AudioStream_RequestControl.md#stop) · [상세 Recovery](../functions/AudioStream_RequestControl.md#recovery) · [R2](../../../../design/30_FUNCTIONS/40_AUDIO_STREAM_ASSET_FUNCTIONS.md#audiostream-requestcontrol) · [R3 provider](../../../../design/40_DATA/50_AUDIO_STREAM_DATA.md#vssprovidercontext) · [R3 요청 단계](../../../../design/40_DATA/50_AUDIO_STREAM_DATA.md#vssaudiorequestresult) · [R3 Recovery](../../../../design/40_DATA/70_DIAGNOSTIC_DATA.md#vssrecoverypermission) · [R4 참조 보호](../../../../design/50_CONTRACTS/10_OWNERSHIP_LIFETIME.md#borrow-and-protect) · [R4 late](../../../../design/50_CONTRACTS/10_OWNERSHIP_LIFETIME.md#late-attribution) · [R4 현재 안전](../../../../design/50_CONTRACTS/50_AUDIO_BUFFER_TRANSPORT.md#safe-write) · [R5 START](../../../../design/60_PSEUDOCODE/40_AUDIO_PREPARE_TX_FLOW.md#audiostream-requestcontrol) · [R5 STOP](../../../../design/60_PSEUDOCODE/50_STOP_LATE_UNCERTAIN_FLOW.md#stream-stop) · [R5 Recovery](../../../../design/60_PSEUDOCODE/60_FAULT_RECOVERY_FLOW.md#stream-recovery) · [R6 typed STREAM](../../../../design/70_C_INTERFACE/00_HEADER_OWNERSHIP_MAP.md#typed-stream) · [R6 STOP scope](../../../../design/70_C_INTERFACE/00_HEADER_OWNERSHIP_MAP.md#stop-scope)
