# AudioStream_Advance — 한국어 C 형태 의사코드

[VSS 지도](../../../01_VSS_MAP.md) / [SERVICE](../../00_LAYER_OVERVIEW.md) / [AUDIO_STREAM](../00_MODULE_OVERVIEW.md) / CORE-12 · [상세 설명](../functions/AudioStream_Advance.md)

> GUIDE · 설명용 한국어 조건·대괄호 절차·CALL 표기다. 실제 C·helper/public API·새 DTO/Task/Queue·전송 버퍼를 생성하지 않는다.

<a id="contract"></a>
## 외부 time 하나와 독립 7군

```text
void AudioStream_Advance(
    IN const VssTimeEvidence *time,
    OUT VssAudioRequestResult requests[capacity -> written],
    OUT VssAudioOutputEvidence outputs[capacity -> written],
    OUT VssBackendCleanupObservation cleanup[capacity -> written],
    OUT VssDeviceReadinessState devices[capacity -> written],
    OUT VssDiagnosticEvidence diagnostics[capacity -> written],
    OUT VssRecoveryPerformedEvidence performed[capacity -> written],
    OUT VssRecoveryVerificationEvidence verified[capacity -> written]);
```

[최신 R6](../../../../design/70_C_INTERFACE/00_HEADER_OWNERSHIP_MAP.md#r6-audiostream-advance)의 OUT을 [후보 표기](../../../../design/70_C_INTERFACE/00_HEADER_OWNERSHIP_MAP.md#prototype-notation)로 펼친다. lower OUT·provider/pcm/원_사실은 기존 R3 보호 자료를 가리키는 설명이며 새 public IN이나 aggregate BackendResult가 아니다. 아래 `void`는 성공/완료 응답이 없다.

<a id="procedure"></a>
## 하위 진행·회차 반환·생산·충분조건 결합 절차

```c
void AudioStream_Advance(
    const VssTimeEvidence *time,
    VssAudioRequestResult *requests,
    SemanticCount requestsCapacity, SemanticCount *requestsWritten,
    VssAudioOutputEvidence *outputs,
    SemanticCount outputsCapacity, SemanticCount *outputsWritten,
    VssBackendCleanupObservation *cleanup,
    SemanticCount cleanupCapacity, SemanticCount *cleanupWritten,
    VssDeviceReadinessState *devices,
    SemanticCount devicesCapacity, SemanticCount *devicesWritten,
    VssDiagnosticEvidence *diagnostics,
    SemanticCount diagnosticsCapacity, SemanticCount *diagnosticsWritten,
    VssRecoveryPerformedEvidence *performed,
    SemanticCount performedCapacity, SemanticCount *performedWritten,
    VssRecoveryVerificationEvidence *verified,
    SemanticCount verifiedCapacity, SemanticCount *verifiedWritten)
{
    // A1: 저장소 외피/메타 접근 가능성·범위/산술·군 간 비간섭 확인 후만 초기화.
    [7개 OUT 각각 주소/전달영역/capacity/written 저장소를 안전하게 검증];
    if (requests군_안전) { *requestsWritten = 0; }
    if (outputs군_안전) { *outputsWritten = 0; }
    if (cleanup군_안전) { *cleanupWritten = 0; }
    if (devices군_안전) { *devicesWritten = 0; }
    if (diagnostics군_안전) { *diagnosticsWritten = 0; }
    if (performed군_안전) { *performedWritten = 0; }
    if (verified군_안전) { *verifiedWritten = 0; }
    [time의 외피·원 시간 domain/epoch/continuity·비교 근거를 읽기 전에 검증];
    if (time == NULL || 시간근거_무효_또는_접근불가) {
        [시간 의존 새 lower 효과/생산/새 confirmed 결합 보류];
        [원 차단·refs·pending·부족 진단 보호; 기존 불변 facts의 안전 인계만 가능];
        goto publish;
    }

    // A2: count=0이어도 기존 pending/cleanup/device startup의 bounded 기회.
    [하위 결과/내부참조의 보호 공간 및 원 의도/time borrow 준비];
    if (TX_독립결과의_안전수집_보호_가능) {
        CALL AudioTx_Advance(
            time, txRequests[capacity -> written], txOutputs[capacity -> written],
            consumed[capacity -> written], returned[capacity -> written],
            txDiagnostics[capacity -> written]);
    } else {
        [그 신규 수집/효과 보류; TX producer에 미인계 사실/후속 보호 유지];
    }
    if (CONTROL_독립결과의_안전수집_보호_가능) {
        CALL AudioControl_Service(
            확인된_기존원device_intent_또는_NULL, time,
            controlDevices[capacity -> written], controlResults[capacity -> written],
            controlDiagnostics[capacity -> written],
            lowerPerformed[capacity -> written], lowerVerified[capacity -> written]);
        [NULL은 이미 수용한 제어의 후속; 새 device/Recovery command를 만들지 않음];
    } else {
        [CONTROL 원사실/진단/원 제어 후속 보호 유지];
    }
    [보호된 미완료 START/STOP control도 기존 원 operation key로만 bounded 연결];
    [RequestControl의 STOP 선차단/실제 target key·scope 규칙 그대로 사용];
    [키/수용 미확인이면 새 NULL continuingOperation 요구로 재발행하지 않음];

    // A3: 원 lower facts를 종류별로 독립 소비. 여러 결과를 lastResult로 덮지 않음.
    for (이번_bounded_기회에_방문할_원_txRequests 각각) {
        [operation/attempt와 STREAM의 원 prepare/control/handoff 연결을 검증];
        if (원귀속_부족_또는_손실) {
            [INSUFFICIENT/LOST·원 producer 진단·추적 보호; current key 대입 없음];
            continue;
        }
        if (old_또는_이미종료의_원문맥) {
            [그 old 사실/정리·refs에만 반영; 새 provider/PCM/ready/HEALTH 불변];
            continue;
        }
        if (원_START_control과_연결된_결과) {
            switch (원_txRequest.state) {
            case VSS_TX_SUBMITTED_OR_QUEUED: [원 START_SUBMITTED 보호]; break;
            case VSS_TX_ACCEPTED_AND_ENABLED: [원 START_ACCEPTED_AND_ENABLED 보호]; break;
            case VSS_TX_REJECTED_WITH_NO_ACCESS: [원 REQUEST_REJECTED 보호]; break;
            case VSS_TX_EFFECT_UNCERTAIN: [원 EFFECT_UNCERTAIN·retain/정리 보호]; break;
            default: [확인된 원 요청 단계/후속만 보호]; break;
            }
        } else {
            [PCM handoff 수락/STOP 단계는 각각 원 사실로 보호];
            [PCM 수락을 START_ACCEPTED_AND_ENABLED로, STOP ACK를 물리 끝으로 변환 없음];
        }
        [실제 producer 실패/partial이면 원 근거로 미래 생산 차단·cleanup];
        [수락/지연/빈 OUT만으로 producer 실패 또는 actual 합성 없음];
    }
    for (보호된_현재controlDevices 각각) {
        if (CONTROL이_그_device_config에_이미_적용했고_현재귀속_유효) {
            [CONTROL의 원 VssDeviceReadinessState 관측만 devices 인계 후보로 보호];
        } else {
            [old target/config 관측은 old 사실·진단으로 보호; 새 READY 쓰기 없음];
        }
    }
    // PB Session 없는 장치 startup 관측도 이 경계에서 전달한다.
    [CONTROL result/diagnostics 및 TX diagnostics를 원 producer/stage/시각으로 보호];
    [지속 Sai_Ip_Send/raw IRQ 등록이 여러 Attempt를 덮으면 개별 대응을 따로 검증];
    [등록/Buffer ID 또는 current key만으로 operation/cycle 귀속을 만들지 않음];

    for (보호된_원_consumed 각각) {
        if (operation_key_원인계구간_대응_부족) {
            [원 귀속 부족/손실·진단/refs 유지]; continue;
        }
        [원 회차 firstFrame/frameCount의 실제 source 소비 사실만 반영];
        [PCM usage/장치 retain·미래 재접근 보호 유지];
        // SOURCE_CONSUMED ≠ SAFE_RETURN ≠ OUTPUT_TERMINATED ≠ WHOLE_SESSION_RETIRED.
    }

    // A4: 같은 옛 회차 반환 + 현재 쓰기 안전. old A로 new A/B release 없음.
    for (보호된_원_returned 각각) {
        if (returned.operation_및_key가_원protected_handoff와_불일치) {
            [귀속 부족/LOST·진단·기존 보호 유지]; continue;
        }
        if (returned.key.attempt_buffer_cycle != 해당_현재_pcm.key) {
            [old 회차의 실제 refs/cleanup만 반영; 새 A/B usage 변경 없음];
            continue;
        }
        if (!returned.noFutureAccess ||
            returned.basis.validity != VSS_COVERAGE_SUFFICIENT ||
            원scope_시간구성_실제coverage_부족) {
            [usage 회수 없음; 원 HW_PROTECTED/RETURN_PENDING·근거/후속 유지];
            continue;
        }
        [적용 시점의 다음 동일 A/B DMA 방문·이번 쓰기 완료 가능성 확인];
        [현재 memory/cache/참조·old 내용 재소비 금지·안전 구간 유효성 재검사];
        if (현재_쓰기안전_미입증_또는_구간경과_또는_실제대응불명확) {
            [usage 해제/WRITE/zero-fill/refill 금지; RETURN_PENDING 등 원 보호 유지];
            continue;
        }
        pcm.usage = VSS_PCM_CPU_WRITABLE; // 원 회차만. 생산권한 true 설정 없음.
        if (!provider.productionAllowed || 원Session_STOP_취소_차단) {
            [old 사용권/참조 정리만; decode/refill/새 sound 제출 없음];
            if (물리_정리zero_fill_필요 && 쓰기직전_현재안전_다시충분) {
                [기존 A/B의 확인된 안전 범위만 정리용 물리 zero-fill];
                [validFrameCount 증가·음향길이/padding·실제DMA_STOP 추론 없음];
            }
        }
    }

    // A5: 동일 준비의 후속. 새 Session/Attempt/age/decoder를 재생성하지 않는다.
    for (이번_bounded_기회의_보호된_원preparation 각각) {
        if (!provider.productionAllowed || 원Session_STOP_취소_차단) {
            provider.productionAllowed = false;
            provider.phase = VSS_PROVIDER_CLOSING;
            [준비 prime 중단·old read/PCM 정리만; STOP만으로 PREPARE_FAILED 없음];
        } else if (원_pending준비이고_최신조건_참조_현재쓰기안전이_충분) {
            [승인 AudioStream_Prepare 내부 prime의 동일 원문맥 bounded 후속];
            [원 consumed/produced·실제 준비조건에 따른 독립 PENDING/PREPARED 보호];
            [준비 단계 공개 직전 STOP/원귀속 다시 확인; actual 합성 없음];
        } else {
            [원 pending/불충분·참조/후속 보호];
        }
        if (늦은_원PREPARED_또는_수락_사실_도착) {
            [원 단계 사실 수명만 보호; STOP/CLOSING/actual/종료를 rollback하지 않음];
        }
    }

    // A6: R5 AudioStream_FillBuffer는 STREAM 내부 단계, public API/Core 추가 아님.
    for (이번_bounded_기회에_정상refill_검토할_A_또는_B 각각) {
        if (!provider.productionAllowed || 원Session_STOP_취소_차단 ||
            원_provider_정상후속phase_또는_현재원귀속_불충분) {
            [신규 음향 생산/제출 없음]; continue;
        }
        if (pcm.usage != VSS_PCM_CPU_WRITABLE ||
            같은_원회차_안전종료_또는_현재_nextDMA_쓰기완료_메모리참조조건_불충분) {
            [기존 usage/근거 유지; WRITE/zero-fill/refill 없음]; continue;
        }
        if (현재_실제decoder_sourceSpan_읽기참조_미확보) {
            [원 asset/image/attempt/sourcePosition의 bounded readRequest 보호];
            VssAssetReadDisposition readState = CALL Asset_Read(
                &원_readRequest, NULL, false,
                spans[capacity -> written], assetDiagnostics[capacity -> written]);
            switch (readState) {
            case VSS_ASSET_SPAN_GRANTED:
                [실제 제공한 1span/validBytes·내부 bytes 참조 보호·원귀속 대조];
                if (실제범위_참조검증_미충족) { [보호 pending]; continue; }
                break;
            case VSS_ASSET_END_OF_RANGE:
                provider.endOfSource = true; // decoder drain/tail은 아직 별도.
                break;
            case VSS_ASSET_ACCESS_REJECTED:
                [ASSET producer의 lookup/metadata/bounds/PFlash read 원 실패 보호];
                [해당 provider 생산차단/CLOSING·기존 인계 자원 정리];
                continue;
            default: [성공 bytes로 사용 없음; 원 보호 pending]; continue;
            }
        }
        if (!provider.productionAllowed || 원Session_STOP_차단 ||
            decode_write_직전_현재안전구간_경과_또는_필수참조_부족) {
            [CPU write/decode 없음; 원 refs·차단·후속 유지]; continue;
        }
        [보호된 원 decoder/span 또는 실제 drain 상태에서 bounded CPU MP3 decode];
        if (실제_corrupt_unsupported_decode_실패_또는_소비생산범위산술_위반) {
            [STREAM 발견 producer/stage의 원 진단 보호];
            [해당 provider 생산차단/CLOSING; 이미 인계한 PCM/TX/read retain 유지];
            [준비 중 실제 실패이면 그 원 PREPARE_FAILED, 정상 STOP이면 만들지 않음];
            continue;
        }
        [실제 consumed compressedBytes/produced frame 수·bounds/format 먼저 검증];
        provider.sourcePosition += 실제_소비한_compressedBytes;
        [pcm의 원 cycle/format/firstValidFrame/validFrameCount를 실제 유효 범위로 안정화];
        if (pcm.validFrameCount == 0) {
            [새 sound handoff 없음; DMA 실제 중단/NO_START/whole final 아님];
            continue;
        }
        [디코딩된 zero sample도 유효 frame; short tail/빈 B/full 두 블록 강제 없음];
        pcm.usage = VSS_PCM_CPU_READY;
        VssPcmHandoffCommand handoff;
        handoff.key = pcm.key;
        handoff.samples = pcm.samples;
        handoff.firstValidFrame = pcm.firstValidFrame;
        handoff.validFrameCount = pcm.validFrameCount;
        handoff.format = pcm.format;
        [handoff.lastForSegment는 실제 해당 구간의 마지막 유효 tail 근거로만 설정];
        [안정 samples/key/format/유효범위·handoff 및 결과 수명을 먼저 보호];
        if (!provider.productionAllowed || 원Session_STOP_취소_차단 ||
            원_Session_Attempt_A또는B_cycle_최신대조_실패 ||
            현재_쓰기안전_nextDMA_안정성_또는_효력전보호_불충분) {
            [새 TX 호출 없이 원 usage/부분refs/차단·pending 보호; 가짜 no-access 없음];
            continue;
        }
        pcm.usage = VSS_PCM_HANDOFF_PENDING; // DRIVER 효과 이전. 이후 CPU 덮어쓰기 금지.
        CALL AudioTx_Request(
            &handoff, NULL, NULL, targetOperations[count=0], time,
            txRequests[capacity -> written], returned[capacity -> written],
            txDiagnostics[capacity -> written]);
        [원 operation/cycle의 각 실제 수락/partial/return/진단을 독립 보호];
        [부분효력에 HW_PROTECTED/RETURN_PENDING 보호 유지; 현재 안전 전 사용권 회수 없음];
        [이 PCM 인계 수락을 START/actual로 만들지 않음];
    }

    // A7: 실제 원 span의 모든 decoder/read·대기 참조 종료 뒤만 REFERENCE_END.
    for (이번_bounded_기회에_참조종료를_검토할_원span 각각) {
        if (실제_원consumer가_더읽지_않음이_확인되지_않음) {
            [span/bytes 보호 유지]; continue;
        }
        VssAssetReadDisposition releaseState = CALL Asset_Read(
            NULL, &원_spanKey, true,
            releaseSpans[capacity -> written], releaseDiagnostics[capacity -> written]);
        if (releaseState == VSS_ASSET_REFERENCE_RELEASED) {
            [그 원 span의 STREAM 참조만 종료];
        } else {
            [STILL_PROTECTED/미확인 원 참조·종료 후속 의무 유지];
        }
        // EOF·source consumed·새 read·PB retirement는 원 reader 참조 종료가 아니다.
    }

    // A8: 출력 kind별 충분조건을 독립 검토. 원 실제 범위를 확대한 성공 없음.
    for (이번_bounded_기회에_검토할_보호된_원Backend_scope 각각) {
        [원 session/attempt/실제구간과 모든 관련 operation·활성화 인과·시각 대조];
        if (원귀속_scope_또는_필요coverage_부족_손실) {
            [원 transportFacts·INSUFFICIENT/LOST·진단/cleanup 보호];
            continue; // incomplete confirmed output 생성/인계 없음.
        }
        if (uncertain_final_뒤_또는_retired의_원문맥) {
            [원 old 사실/refs/cleanup·진단에만 연결; 정상 Session 부활 없음];
            continue;
        }
        if (원활성화후_새실제물리출력과_원시각_충분coverage_확인) {
            [그 원 OUTPUT_START_CONFIRMED를 불변 보호];
            [actual만이면 futurePcmBlocked=false 가능; 원 transportFacts/count 보존];
            [최초 적법성/Started와 기한·권한 비교는 PLAYBACK에 남김];
        }
        if (필요과거범위의_충분_PAST_NO_OUTPUT_PROVEN &&
            모든_관련old_미래출력_FUTURE_OUTPUT_BLOCKED &&
            그_scope의_미래decode_refill_PCM제출_실제차단) {
            [그 원 NO_START_CONFIRMED·futurePcmBlocked=true·각 원 facts 보호];
        }
        if (그_scope_실제장치_FIFO_frame_잔류_OUTPUT_ENDED &&
            모든_관련old_미래출력_FUTURE_OUTPUT_BLOCKED &&
            그_scope의_미래PCM생산_제출_실제차단) {
            [그 원 OUTPUT_TERMINATION_CONFIRMED·futurePcmBlocked=true·각 원 facts 보호];
        }
        [위 조건 미충족 부분은 confirmed 생성 없이 원 근거/부족 진단/후속 유지];
        [START_OUTCOME_UNCERTAIN의 기존 지위 보존; timeout만으로 상위 최종 생성 없음];
        [한 SEGMENT/cue/EOS·STOP ACK를 whole Completed/retirement로 확대 없음];
        [transportFacts의 실제 PB 소비 수명까지 참조 보호];
    }

    // A9: cleanup 네 조건은 각각 실제 해당 원 범위의 근거로만.
    for (이번_bounded_기회에_검토할_원Attempt_정리 각각) {
        VssBackendCleanupObservation observation;
        observation.attempt = 원_attempt;
        observation.providerReferencesEnded = 관련_provider_read_실제refs_모두종료_확인;
        observation.pcmAccessEnded = 관련_A또는B_모든회차의_하위미래접근종료_확인;
        observation.lowerOutputBlocked = 해당_old출력_잔류가능성_충분차단_확인;
        observation.lateIdentityProtected = 남은_callback_result_원귀속_alias없는보호_확인;
        [미확인은 해당 완료 false로 남기고 부족/진단을 별도 보호];
        [원 cleanup 관측 보호; one true=>all true/whole retirement/기록폐기 합성 없음];
    }

    // A10: Session 없는 Recovery 후속. 허용/실제 수행/별도 검증/HEALTH clear 분리.
    for (이번_bounded_기회에_검토할_원recovery_operation 각각) {
        [보호된 6필드 permission와 실제 operation/target/config 인과를 대조];
        if (현재허용_철회_또는_instance_revision_target_config_action_변경) {
            [새 수행 금지; old performed/verified·원 진단/정리 보호];
            [현재 READY/Fault 적용 없음]; continue;
        }
        if (실제_원수행_근거_확인) {
            [VssRecoveryPerformedEvidence의 permission/target/configuration/operation 보호];
            [effect/occurredAt은 실제 SUCCEEDED/FAILED/UNCONFIRMED 원 사실];
            [performed 원 fact와 HEALTH 소비까지 수명 보호];
        }
        if (수행뒤_별도효과관측_원인과_충분성_확인) {
            [VssRecoveryVerificationEvidence의 permission/performedOperation 대조];
            [target/configuration/effect/verifiedAt을 별도 실제 검증 근거로 보호];
            [verified 원 fact와 HEALTH 소비까지 수명 보호];
        } else {
            [별도 성공 verified 합성 없음; 실제 미확인/실패 근거·후속 유지];
        }
        [두 facts를 동시에 만들거나 동일 written으로 강제하지 않음];
        [old 결과는 원 대상·구성으로만 인계; 현재 HEALTH만 pair+현재허용 대조 후 해제];
    }

publish:
    // A11: 한 군이 무효/full/0이면 그 군의 원 fact를 유지하고 다른 군은 독립 진행.
    for (보호된_미인계_requests_사실 각각) {
        if (!requests군_안전 || *requestsWritten >= requestsCapacity) {
            [원 stage/참조·후속 유지]; break;
        }
        [requests[*requestsWritten]에 실제 원 session/attempt/stage 기록];
        if (실제_기록과_후속참조보호_성공) { *requestsWritten += 1; }
        else { [미인계 유지]; break; }
    }
    for (보호된_미인계_outputs_사실 각각) {
        if (!outputs군_안전 || *outputsWritten >= outputsCapacity) {
            [원 kind/scope/transportFacts 및 소비 수명 보호]; break;
        }
        [outputs[*outputsWritten]에 실제 충분한 kind의 불변 원 evidence 기록];
        if (실제_기록과_transportFacts_참조보호_성공) { *outputsWritten += 1; }
        else { [미인계·원 참조 유지]; break; }
    }
    for (보호된_미인계_cleanup_관측 각각) {
        if (!cleanup군_안전 || *cleanupWritten >= cleanupCapacity) {
            [원 4조건 관측/refs/후속 유지]; break;
        }
        [cleanup[*cleanupWritten]에 원 attempt/4개 독립 조건 기록];
        if (실제_기록과_수명보호_성공) { *cleanupWritten += 1; }
        else { [미인계 유지]; break; }
    }
    for (보호된_미인계_devices_관측 각각) {
        if (!devices군_안전 || *devicesWritten >= devicesCapacity) {
            [CONTROL producer의 원 관측/참조 유지]; break;
        }
        [devices[*devicesWritten]에 CONTROL 적용 완료된 원 관측만 기록];
        if (실제_기록과_수명보호_성공) { *devicesWritten += 1; }
        else { [미인계 유지]; break; }
    }
    for (보호된_미인계_diagnostics_사실 각각) {
        if (!diagnostics군_안전 || *diagnosticsWritten >= diagnosticsCapacity) {
            [원 producer/stage/시각/coverage·내부참조/후속 유지]; break;
        }
        [diagnostics[*diagnosticsWritten]에 원 진단과 실제 참조 기록];
        if (실제_기록과_참조보호_성공) { *diagnosticsWritten += 1; }
        else { [미인계/실제 LOST를 숨기지 않고 보호]; break; }
    }
    for (보호된_미인계_performed_사실 각각) {
        if (!performed군_안전 || *performedWritten >= performedCapacity) {
            [원 수행 operation/permission/시각/참조 유지]; break;
        }
        [performed[*performedWritten]에 실제 원 수행 evidence 기록];
        if (실제_기록과_수명보호_성공) { *performedWritten += 1; }
        else { [미인계 유지]; break; }
    }
    for (보호된_미인계_verified_사실 각각) {
        if (!verified군_안전 || *verifiedWritten >= verifiedCapacity) {
            [원 별도 검증/permission/performedOperation/시각/참조 유지]; break;
        }
        [verified[*verifiedWritten]에 실제 별도 검증 evidence 기록];
        if (실제_기록과_수명보호_성공) { *verifiedWritten += 1; }
        else { [미인계 유지]; break; }
    }
    [consumer 반영/독립 보호 추적 종료 전 producer의 원 facts를 폐기하지 않음];
    [보호 실패/관측 손실은 실제 LOST·진단/후속으로 드러내고 성공으로 숨기지 않음];
    return; // 처리 기회 종료. 통합 성공/whole 완료가 아니다.
}
```

<a id="boundaries"></a>
## 참조·writer·출력 범위 사후 조건

STREAM은 자기 provider/PCM A/B usage·미래 생산 권한·Backend 결합만 쓴다. TX 사실은 원 operation/cycle로 보존하고 CONTROL devices는 적용된 관측의 전달이다. source consumed/safe return/물리 output end/PB retirement는 다른 경계다. 실제 span reader 종료와 `Asset_Read` RELEASED는 별도이고 PCM return/Session retirement만으로 bytes·HAL 원 기록을 폐기하지 않는다.

원 actual이 충분해도 최초 적법성/Started는 PB가 원발생/원 actual 시각·당시 권한으로 판단한다. 과거 무출력과 미래 출력 차단은 서로 다른 TX 사실이며 NO_START/TERMINATION은 각 scope의 PCM 미래 차단도 필요하다. unknown·timeout/ACK/현재 무음은 confirmed로 출력하지 않는다. performed/verified는 각각 실제 원 수행과 별도 관측이고 HEALTH가 현재 대상/구성과 pair를 대조해 최종 제한 해제를 판단한다.

<a id="gates"></a>
## 미정과 독립 구현 후보

정확한 C ABI·저장 예산/직렬화·효력 직전 검사와 실제 하위 효과의 원자성·decoder/image/PCM format·DMA next-visit/WCET·cache/barrier·물리 actual/NO_START/STOP/TERMINATION fence·복구 action map/효과 검증은 BINDING_TBD/HW_UNVERIFIED다. 그 미정은 필요한 제품 효과만 막고 identity/7 OUT/STOP 차단/old cycle/충분조건/cleanup/복구 분리의 독립 Host 후보는 유지한다. 이번 Firmware/Host/Build/Flash/Board NOT_RUN, P2 HOLD다.

## 정본·인접 문서

[상세 원형](../functions/AudioStream_Advance.md#contract) · [상세 lower](../functions/AudioStream_Advance.md#lower-results) · [상세 반환](../functions/AudioStream_Advance.md#pcm-return) · [상세 생산](../functions/AudioStream_Advance.md#production) · [상세 수명](../functions/AudioStream_Advance.md#references) · [상세 출력](../functions/AudioStream_Advance.md#outputs) · [상세 cleanup](../functions/AudioStream_Advance.md#cleanup) · [상세 복구](../functions/AudioStream_Advance.md#recovery) · [RequestControl 의사코드](AudioStream_RequestControl.md#procedure) · [승인 Prepare](AudioStream_Prepare.md#procedure) · [승인 Asset_Read](../../ASSET/pseudocode/Asset_Read.md) · [R2](../../../../design/30_FUNCTIONS/40_AUDIO_STREAM_ASSET_FUNCTIONS.md#audiostream-advance) · [R3 PCM](../../../../design/40_DATA/50_AUDIO_STREAM_DATA.md#vsspcmbufferstate) · [R3 Return](../../../../design/40_DATA/60_AUDIO_TX_DATA.md#vsspcmreturnevidence) · [R4 actual](../../../../design/50_CONTRACTS/20_ASYNC_RESULT_EVIDENCE.md#confirmed-outcomes) · [R4 late](../../../../design/50_CONTRACTS/10_OWNERSHIP_LIFETIME.md#late-attribution) · [R4 현재 안전](../../../../design/50_CONTRACTS/50_AUDIO_BUFFER_TRANSPORT.md#safe-write) · [R5 내부 fill](../../../../design/60_PSEUDOCODE/40_AUDIO_PREPARE_TX_FLOW.md#fill-buffer) · [R5 old return](../../../../design/60_PSEUDOCODE/50_STOP_LATE_UNCERTAIN_FLOW.md#stream-safe-return) · [R5 복구](../../../../design/60_PSEUDOCODE/60_FAULT_RECOVERY_FLOW.md#stream-recovery) · [R6 TX](../../../../design/70_C_INTERFACE/00_HEADER_OWNERSHIP_MAP.md#r6-audiotx-advance) · [R6 CONTROL](../../../../design/70_C_INTERFACE/00_HEADER_OWNERSHIP_MAP.md#r6-audiocontrol-service)
