# AudioStream_Prepare — 한국어 C 형태 의사코드

[VSS 지도](../../../01_VSS_MAP.md) / [SERVICE](../../00_LAYER_OVERVIEW.md) / [AUDIO_STREAM](../00_MODULE_OVERVIEW.md) / CORE-10 · [상세 설명](../functions/AudioStream_Prepare.md)

> GUIDE · SELF_CHECKED / GD4_1_PENDING. 한국어 조건·대괄호 절차는 설명용 추상 연산이다. 새 helper/API·Task·전송 버퍼를 선언하지 않으며 실제 C·Host·보드 시험 NOT_RUN이다.

<a id="contract"></a>
## R6 논리 계약

```text
VssRequestDisposition AudioStream_Prepare(
    IN const VssAudioPreparation *preparation,
    OUT VssAudioRequestResult requests[capacity -> written],
    OUT VssDiagnosticEvidence diagnostics[capacity -> written]);
```

[최신 R6](../../../../design/70_C_INTERFACE/00_HEADER_OWNERSHIP_MAP.md#r6-audiostream-prepare)의 동기 scalar와 독립 requests/diagnostics를 유지한다. 아래 OUT 인자 전개는 [R6 후보 표기](../../../../design/70_C_INTERFACE/00_HEADER_OWNERSHIP_MAP.md#prototype-notation)이며 compile-ready C ABI가 아니다. `provider`·`pcm`·`원_단계사실`은 기존 R3 원본/불변 사실의 설명용 참조이며 새로운 aggregate BackendResult나 저장 구조가 아니다.

<a id="procedure"></a>
## 원 준비 수용·내부 prime·결과 보호 절차

```c
VssRequestDisposition AudioStream_Prepare(
    const VssAudioPreparation *preparation,
    VssAudioRequestResult *requests,
    SemanticCount requestsCapacity, SemanticCount *requestsWritten,
    VssDiagnosticEvidence *diagnostics,
    SemanticCount diagnosticsCapacity, SemanticCount *diagnosticsWritten)
{
    // P1: 각각의 OUT 저장소 검증 후에만 count 초기화. 무효 군 무접근.
    [requests와 diagnostics 각각 주소·capacity·written·범위·비간섭 확인];
    if (requests군_안전) { *requestsWritten = 0; }
    if (diagnostics군_안전) { *diagnosticsWritten = 0; }
    if (preparation == NULL || preparation_외피가_안전하게_읽히지_않음) {
        [가짜 Session/stage를 만들지 않고 기존 원 사실·정리 보호];
        return VSS_REQUEST_REJECTED;
    }
    if (plan_Meta_등_내부borrow_접근가능성_또는_원귀속_검증_실패 ||
        session_attempt_plan_position_asset_의미관계_부적합) {
        [부적합 원 인자를 하위 호출·provider 덮어쓰기에 사용하지 않음];
        return VSS_REQUEST_REJECTED;
    }

    VssRequestDisposition disposition;
    // P2: 이미 차단된 원 문맥은 원 차단 유지. OUT에 남은 원 사실은 뒤에서 인계.
    if (동일_원preparation이_취소_STOP_CLOSING_또는_생산차단됨) {
        disposition = VSS_REQUEST_KEPT; // 동기 처분은 기존 차단 유지, 준비 성공 아님.
        [productionAllowed=false·옛 참조/정리 유지; prime/새 제출 없음];
        [적법한 원 Session/Attempt의 VSS_AUDIO_REQUEST_REJECTED stage를 독립 보호];
        [같은 원 거부 사실은 기존 보호를 계승하며 count/참조를 독립 인계];
        [이번 재요구에 옛 PREPARED를 성공으로 재발행하지 않음; 원 미인계 사실/정리 보호];
        goto publish;
    }
    if (동일_귀속이지만_보호된_plan_position_asset_내용이_다름) {
        return VSS_REQUEST_REJECTED; // 동일 key로 다른 요구를 덮지 않는다.
    }
    if (동일_preparation이_이미_수용됨) {
        disposition = VSS_REQUEST_KEPT;
        [원 identity/Meta/age·decoder/plan·소비 진행을 유지];
        if (원_PREPARED가_현재도_유효) {
            goto publish; // 중복 read/decoder reset/PCM 인계 없음.
        }
    } else {
        if (old_provider_PCM_참조_안전종료_또는_새_보호예산_불충분) {
            return VSS_REQUEST_WAIT; // 기존 자원 불변, 새 prime 없음.
        }
        // P3: 효력이 가능한 하위 요청 전에 참조·결과 보존을 확보한다.
        [원 preparation/plan/position/필요한 firstStartMeta의 후속 수명 보호];
        [provider 및 생성될 독립 stage/진단·PCM의 원 귀속 보호 확보];
        if (후속_자료_또는_결과보호_확보_실패) {
            return VSS_REQUEST_WAIT;
        }
        provider.session = preparation->session;
        provider.attempt = preparation->attempt;
        provider.asset = preparation->asset;
        provider.phase = VSS_PROVIDER_PREPARING;
        provider.productionAllowed = true; // 원 수용 근거 확인 후에만.
        provider.endOfSource = false;
        [sourcePosition은 원 asset의 실제 시작 위치; 내부 decoder 준비];
        [원 PREPARE_RECEIVED 사실을 별도로 불변 보호];
        disposition = VSS_REQUEST_ADOPTED;
    }

    if (!provider.productionAllowed || 현재_원문맥_취소_STOP) {
        goto cancelled_cleanup; // 정상 차단: 실제 producer 실패가 아님.
    }
    if (현재_원귀속_또는_필요한_생산권한근거가_재확인되지_않음) {
        goto protected_pending; // 미확인으로 실패/완전 취소를 합성하지 않는다.
    }

    // P4: 필요한 현재 device/config만 제어. CONTROL 원본을 직접 쓰지 않음.
    [현재 Runtime의 보호된 시간 근거를 읽고 필요한 원 device intent 보호];
    CALL AudioControl_Service(
        필요한_현재device_intent_또는_NULL, 원_time,
        devices[capacity -> written], controlResults[capacity -> written],
        controlDiagnostics[capacity -> written],
        performed[capacity -> written], verified[capacity -> written]);
    [현재 device/config에 귀속되는 사실만 결합; void/옛 READY는 성공 아님];
    if (확인된_현재장치_준비실패) {
        [CONTROL 원 producer/target/config/operation/시각 진단 보호];
        goto preparation_failed;
    }

    // P5: pending 호출에 이미 보호된 sourceSpan이 있으면 재사용. 중복 READ 없음.
    if (기존_안전한_sourceSpan이_현재_decoder_읽기에_없음) {
        [원 asset/image/attempt와 sourcePosition으로 bounded readRequest 보호];
        VssAssetReadDisposition readDisposition = Asset_Read(
            &readRequest, NULL, false,
            sourceSpans[capacity -> written], assetDiagnostics[capacity -> written]);
        switch (readDisposition) {
        case VSS_ASSET_SPAN_GRANTED:
            if (실제_span_1개_및_bytes_범위_귀속_참조보호가_불충분) {
                [불충분 결과로 decoder 접근 금지; 원 부분참조 보호];
                goto protected_pending;
            }
            [원 span descriptor 및 bytes를 실제 decoder 수명에 연결해 보호];
            break;
        case VSS_ASSET_END_OF_RANGE:
            provider.endOfSource = true; // compressed 끝만. decoder drain/tail은 별도.
            break;
        case VSS_ASSET_ACCESS_REJECTED:
            [ASSET 발견 원 오류와 STREAM 준비 실패를 구분해 보호];
            goto preparation_failed;
        default:
            [READ에서 RELEASE/STILL_PROTECTED를 성공 span으로 사용하지 않음];
            goto protected_pending;
        }
    }

    // P6: 기존 내부 fill/prime. 전송 PCM은 정확히 A/B 두 개, 새 Core 아님.
    for (현재_한정된_초기prime_기회의_A_또는_B 각각) {
        [해당 원 attempt/buffer/cycle과 PCM 참조를 대조];
        if (!provider.productionAllowed || 현재_원문맥_취소_STOP) {
            goto cancelled_cleanup;
        }
        if (pcm.usage != VSS_PCM_CPU_WRITABLE ||
            옛회차_실제접근종료_근거_부족 ||
            차기_DMA방문전_현재_쓰기완료_안전조건_미확인) {
            [그 Buffer에는 쓰지 않고 기존 보호·후속 필요성 유지];
            continue;
        }
        // 실제 write/decode 직전 현재 안전 기회 재확인. 옛 IRQ/returned로 대체 금지.
        if (쓰기_직전_안전구간_만료_또는_메모리참조조건_미확인) {
            continue;
        }
        [보호된 원 span/decoder drain 상태로 현재 bounded 범위만 CPU decode];
        if (STREAM이_확인한_corrupt_unsupported_또는_decoder_실패) {
            [STREAM 발견 원 진단 보호; ASSET read 결과를 재라벨링하지 않음];
            goto preparation_failed;
        }
        [실제 consumed/produced 수·span/PCM 범위·format 및 소비 위치 산술을 먼저 검증];
        if (소비_또는_생산_범위_또는_위치산술_검증실패) {
            [STREAM 원 decoder/범위 실패 보호];
            goto preparation_failed;
        }
        provider.sourcePosition += 실제_소비한_compressedBytes;
        [pcm.firstValidFrame/validFrameCount/format·원 cycle을 안정화];
        if (pcm.validFrameCount == 0) {
            [새 sound handoff 없음; DMA STOP/NO_START/전체 final 합성 없음];
            continue;
        }
        pcm.usage = VSS_PCM_CPU_READY; // 실제 decode sample0도 valid frame.
        [짧은 tail/빈 B를 full로 채우거나 sound 반복하지 않음];

        if (초기_준비에_이_PCM의_하위_인계가_필요) {
            if (!provider.productionAllowed || 현재_원문맥_취소_STOP) {
                goto cancelled_cleanup;
            }
            [안정된 samples/유효범위/format/key와 원 handoff 참조 보호];
            // 보호 완료 후, HANDOFF_PENDING/TX 부수효력 직전 최신 조건 재확인.
            if (!provider.productionAllowed || 현재_원문맥_취소_STOP ||
                원_Session_Attempt_A또는B_cycle_현재귀속_재대조_실패 ||
                현재_CPU쓰기안전_및_차기DMA방문전_안정성_재확인_불충분) {
                [TX 호출 없이 새 sound handoff 중단; 원 PCM/cycle·부분 참조·현재 usage 보호];
                [가짜 무효력거부/no-access·출력 종료나 CPU 재쓰기 권한을 만들지 않음];
                if (!provider.productionAllowed || 현재_원문맥_취소_STOP) {
                    goto cancelled_cleanup;
                }
                goto protected_pending;
            }
            // 최종 확인부터 하위 효력까지 근거 유효성을 유지할 직렬화/원자성은 BINDING_TBD.
            pcm.usage = VSS_PCM_HANDOFF_PENDING; // DRIVER 효력 전에 보호.
            CALL AudioTx_Request(
                &원_pcmHandoff, NULL, continuingOperation_또는_NULL,
                targetOperations[count], 원_time,
                txRequests[capacity -> written], returned[capacity -> written],
                txDiagnostics[capacity -> written]);
            [원 operation/cycle의 실제 결과만 owner 경계로 결합];
            [무효력 거부/returned라도 현재 쓰기 안전까지 확인 전 CPU 회수 금지];
            [부분 효력·HW_PROTECTED/RETURN_PENDING의 samples와 원 귀속 유지];
            // PCM 인계만. START command/actual 근거를 만들지 않는다.
        }
    }

    [실제 decoder reader·대기/진행 참조가 모두 끝난 원 span만 종료 후보];
    if (해당_원span의_실제reader_종료가_확인됨) {
        VssAssetReadDisposition releaseDisposition = Asset_Read(
            NULL, &원_spanKey, true,
            releaseSpans[capacity -> written], releaseDiagnostics[capacity -> written]);
        if (releaseDisposition == VSS_ASSET_REFERENCE_RELEASED) {
            [ASSET 종료 확인 후 그 원 span의 STREAM 참조만 해제];
        } else {
            [STILL_PROTECTED/미확인 원 span과 후속 종료 의무 유지];
        }
    }

    // P7: stage 보호 공간·최신 권한/근거를 재검사. 2 full sound block 강제 없음.
    if (!provider.productionAllowed || 현재_원문맥_취소_STOP) {
        goto cancelled_cleanup;
    }
    if (새_단계사실_보호예산_또는_필요근거_불충분) {
        [기존 단계 사실·미인계 보호; 충분한 PREPARED 생성 금지];
    } else if (원_provider_음원_format_초기유효PCM_현재device_TX준비_모두_충분) {
        provider.phase = VSS_PROVIDER_PREPARED;
        [원 PREPARED 사실을 새 독립 기록으로 보호];
    } else {
        [원 PREPARE_PENDING 사실과 기존 후속 진행을 보호];
    }
    goto publish;

protected_pending:
    [원 미확인/부족 근거·현재 usage·원 사실/부분 참조와 보호 후속 유지];
    [보호 가능한 원 PREPARE_PENDING만 기록; 부족하면 원 미인계/손실 추적 유지];
    [새 TX 호출·준비 성공·완전 취소/no-access·CPU 사용권 회수를 합성하지 않음];
    goto publish;

cancelled_cleanup:
    // P8a: 준비 도중 정상 STOP/취소/차단. 실제 producer 실패와 분리.
    provider.productionAllowed = false;
    provider.phase = VSS_PROVIDER_CLOSING;
    [원 PCM/span/장치·진행 reader·미인계 사실의 실제 안전 정리/후속 보호];
    [새 prime/제출 금지; STOP만으로 PREPARE_FAILED·실패 진단/Fault 생성 금지];
    [불명확 하위 부분 효력은 원 보호로 유지; 완전 취소/출력 종료로 승격하지 않음];
    goto publish;

preparation_failed:
    // P8b: 이미 수용한 준비의 실제 ASSET/STREAM/CONTROL 실패가 확인된 경로만.
    provider.productionAllowed = false;
    provider.phase = VSS_PROVIDER_CLOSING;
    [적법한 원 준비의 PREPARE_FAILED 및 원 발견 producer 진단/정리 보호];
    [이미 인계한 PCM·TX/device·ASSET span·진행 reader의 실제 안전 종료 추적];
    [즉시 rollback/fallback·현재 Attempt 재귀속·원 decoder 참조 조기 회수 금지];

publish:
    // 두 OUT 군은 각각 유효할 때만 인계. stage 순서 역전으로 actual을 되돌리지 않음.
    for (이_원준비의_보호된_미인계_stage_사실 각각) {
        if (!requests군_안전 || *requestsWritten >= requestsCapacity) {
            [기존 owner에 원 사실·참조·후속 의무 유지];
            break;
        }
        [requests[*requestsWritten]에 불변 원 session/attempt/stage를 기록];
        if (실제_기록과_전달후_필요참조보호_성공) {
            *requestsWritten += 1;
        } else {
            [미인계 유지; 단일 마지막 stage 슬롯으로 덮지 않는다];
            break;
        }
    }
    for (이_원준비에_연결된_보호된_미인계_진단 각각) {
        if (!diagnostics군_안전 || *diagnosticsWritten >= diagnosticsCapacity) {
            [원 producer·발견단계·시각·참조/미인계 보호];
            break;
        }
        [diagnostics[*diagnosticsWritten]에 원 진단과 필요한 참조를 인계];
        if (실제_기록과_참조보호_성공) {
            *diagnosticsWritten += 1;
        } else {
            [미인계/실손실 근거 유지; 손실을 정상 완료로 숨기지 않음];
            break;
        }
    }
    return disposition; // ADOPTED/KEPT는 PREPARED·actual·정리 완료와 별개.
}
```

<a id="call-boundaries"></a>
## 하위 CALL 표기와 원 사실

`CALL`의 capacity/count/OPTIONAL은 [R6 논리 표기](../../../../design/70_C_INTERFACE/00_HEADER_OWNERSHIP_MAP.md#prototype-notation)다. lower OUT과 원 의도·시간은 기존 owner에 보호된 설명용 자료이며 새 Prep 입력 인자를 추가한 것이 아니다. CTRL의 performed/verified는 별도 typed 사실이고 새 RecoveryPermission을 생성하거나 복구를 이 함수에서 허가하지 않는다.

[CONTROL 경계](../../../../design/70_C_INTERFACE/00_HEADER_OWNERSHIP_MAP.md#r6-audiocontrol-service)는 현재 config만 준비한다. [TX PCM lane](../../../../design/70_C_INTERFACE/00_HEADER_OWNERSHIP_MAP.md#r6-audiotx-request)은 필요 초기 인계만 연결하며 START/STOP·operation 충분성 상세는 D4-2/D5로 남긴다. 하위 void/scalar·빈 OUT·IRQ를 READY/actual로 합성하지 않는다.

<a id="safe-write-and-tail"></a>
## PCM 유효 범위·현재 안전·STOP

CPU_WRITABLE은 옛 회차 종료만으로 무기한 쓰기를 허가하지 않는다. [차기 DMA 방문 전 현재 안전](../../../../design/50_CONTRACTS/50_AUDIO_BUFFER_TRANSPORT.md#safe-write)을 실제 decode/write마다 확인하고, HW_PROTECTED·RETURN_PENDING·옛 cycle 결과로 현재 A/B를 쓰지 않는다. 중간 준비가 이미 보호된 PCM을 인계했으면 그 Buffer에 다음 decode를 덧쓰지 않는다.

[유효 PCM/물리 무음](../../../../design/50_CONTRACTS/50_AUDIO_BUFFER_TRANSPORT.md#valid-and-silence)에 따라 zero-valued decoded sample은 유효 frame이며 zero-fill은 sound 길이에 포함하지 않는다. 빈 B·short tail은 full이나 2블록 요구가 아니다. zero-fill도 현재 안전한 기존 A/B 범위에서만 가능하고 이 코드의 sound prime과 별개다. STOP 이후 productionAllowed=false를 유지하며 zero-fill/returned가 prime 허가·NO_START·TERMINATION을 만들지 않는다.

<a id="gates"></a>
## 미확인과 후속 범위

PREPARED 충분성의 실제 format/초기 TX/장치 조건과 source/image·보호 저장소·직렬화는 BINDING_TBD다. 현재 DMA safe-write·메모리/기한·SAI/STOP/actual은 HW_UNVERIFIED/P2 HOLD이며 충분하지 않으면 보호된 PENDING/후속으로 남긴다. F1 identity·OUT·분리 단계, F2 출처 있는 시험 fixture, F3 short tail·decode/read 실패·차단 경합은 독립 Host 후보이며 실제 실행 NOT_RUN이다. D4-2 RequestControl/Advance 및 D5/D6 상세를 새 문서/코드로 추가하지 않는다.

## 연결 근거

[상세 계약](../functions/AudioStream_Prepare.md#contract) · [원 수용·중복](../functions/AudioStream_Prepare.md#admission) · [준비 단계](../functions/AudioStream_Prepare.md#stages) · [OUT·실패](../functions/AudioStream_Prepare.md#outputs-and-failure) · [R2](../../../../design/30_FUNCTIONS/40_AUDIO_STREAM_ASSET_FUNCTIONS.md#audiostream-prepare) · [R3 provider](../../../../design/40_DATA/50_AUDIO_STREAM_DATA.md#vssprovidercontext) · [R4 소유·보호](../../../../design/50_CONTRACTS/10_OWNERSHIP_LIFETIME.md#borrow-and-protect) · [R5 Prepare](../../../../design/60_PSEUDOCODE/40_AUDIO_PREPARE_TX_FLOW.md#audiostream-prepare) · [R5 내부 prime](../../../../design/60_PSEUDOCODE/40_AUDIO_PREPARE_TX_FLOW.md#fill-buffer) · [R6](../../../../design/70_C_INTERFACE/00_HEADER_OWNERSHIP_MAP.md#r6-audiostream-prepare)
