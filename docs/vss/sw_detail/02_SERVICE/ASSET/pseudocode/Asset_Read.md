# Asset_Read — 한국어 C 형태 의사코드

[VSS 지도](../../../01_VSS_MAP.md) / [SERVICE](../../00_LAYER_OVERVIEW.md) / [ASSET](../00_MODULE_OVERVIEW.md) / CORE-09 · [상세 설명](../functions/Asset_Read.md)

> GUIDE · SELF_CHECKED / GD4_1_PENDING. 설명용 조건·대괄호 절차는 구현 helper/API가 아니다. 실제 C·Host·보드 시험 NOT_RUN.

<a id="contract"></a>
## R6 논리 계약

```text
VssAssetReadDisposition Asset_Read(
    ONE_OF const VssAssetReadRequest *request,
           const VssAssetSpanKeyType *referenceEnd,
    VALUE SemanticBool readerReferencesEnded,
    OUT VssAssetSpan span[capacity -> written],
    OUT VssDiagnosticEvidence diagnostics[capacity -> written]);
```

[최신 R6](../../../../design/70_C_INTERFACE/00_HEADER_OWNERSHIP_MAP.md#r6-asset-read)의 scalar와 독립 OUT 군을 유지한다. 아래 capacity/written 인자 전개는 [R6 후보 표기](../../../../design/70_C_INTERFACE/00_HEADER_OWNERSHIP_MAP.md#prototype-notation)이며 실제 Header/ABI가 아니다. `span군_안전` 등은 각 저장소의 접근 가능성·범위·비간섭을 이미 검증했다는 설명용 조건이다.

<a id="procedure"></a>
## 읽기와 참조 종료 절차

```c
VssAssetReadDisposition Asset_Read(
    const VssAssetReadRequest *request,
    const VssAssetSpanKeyType *referenceEnd,
    SemanticBool readerReferencesEnded,
    VssAssetSpan *span, SemanticCount spanCapacity, SemanticCount *spanWritten,
    VssDiagnosticEvidence *diagnostics,
    SemanticCount diagnosticsCapacity, SemanticCount *diagnosticsWritten)
{
    // A1: 포인터 존재 수만 검사. 입력 내용/원 참조를 아직 역참조하지 않는다.
    const SemanticBool oneLane = ((request != NULL) + (referenceEnd != NULL)) == 1;

    // A2: 두 OUT은 각각 검증. 무효 군은 written까지 완전 무접근.
    [span군의 주소·capacity·written 저장소·범위·비간섭을 확인];
    [diagnostics군의 주소·capacity·written 저장소·범위·비간섭을 확인];
    if (span군_안전) {
        *spanWritten = 0; // capacity=0이면 배열 공간은 읽거나 쓰지 않는다.
    }
    if (diagnostics군_안전) {
        *diagnosticsWritten = 0;
    }
    if (!oneLane) {
        [기존 모든 span·reader 참조 보존];
        return VSS_ASSET_ACCESS_REJECTED;
    }

    if (request != NULL) {
        // A3: READ lane에서는 readerReferencesEnded를 사용하지 않는다.
        if (request_저장소를_안전하게_읽을_수_없음) {
            return VSS_ASSET_ACCESS_REJECTED;
        }
        if (현재_asset_image_build_metadata_또는_원attempt_검증_실패) {
            [ASSET 원 lookup/metadata 실패를 원 귀속·시각으로 보호];
            goto read_rejected;
        }
        [ASSET의 확인된 read-only descriptor를 읽는다];
        if (offset_requestedBytes_표현_부적합_또는_합산_overflow_가능) {
            [합을 먼저 계산하지 않고 ASSET bounds 오류 보호];
            goto read_rejected;
        }
        if (request->offset > descriptor->compressedLength) {
            goto read_rejected;
        }
        if (request->offset == descriptor->compressedLength) {
            return VSS_ASSET_END_OF_RANGE; // decoder/PCM/Session 끝은 아님.
        }
        if (request->requestedBytes == 0 ||
            request->requestedBytes > descriptor->compressedLength - request->offset) {
            [범위 밖 요청을 조용히 정상 short span으로 바꾸지 않는다];
            goto read_rejected;
        }
        if (!span군_안전 || spanCapacity < 1 ||
            실제_bytes와_원consumer_참조를_보호할_공간_부족) {
            [기존 자료·진단·추적 유지; 가짜 span/성공 없음];
            goto read_rejected;
        }

        // A4: 실제 bounded read. 길이·한도/저장 형태의 제품 값은 TBD.
        [적법한 요청 범위 및 기존 공급 한도 안에서 read-only bytes 공급];
        if (실제_PFlash_read_실패 || 확인된_validBytes가_없음 ||
            확인된_validBytes가_요청_또는_압축_유효범위를_넘음) {
            [ASSET 발견 read 오류·실손실 근거 보호];
            goto read_rejected;
        }
        [granted.span/asset/image/attempt/offset/validBytes/bytes를 불변 보호];
        [실제 decoder reader와 대기/진행 참조를 제공 전에 원 span에 등록];
        if (등록_또는_전달후_참조보호_실패) {
            [이미 생긴 부분 참조도 안전 종료 전 유지; 인계 성공으로 계수하지 않음];
            goto read_rejected;
        }
        span[0] = granted; // VssAssetSpan의 설명용 지역 값. 얕은 복사만으로 보호 주장 금지.
        *spanWritten = 1;
        return VSS_ASSET_SPAN_GRANTED; // ASSET read만 성공. decoder 판단은 STREAM.

read_rejected:
        [실제로 확인한 lookup/metadata/bounds/read/보호 실패의 ASSET 원 진단을 보호];
        // 이미 검증한 diagnostics 공간에 실제 원 진단을 인계할 때만 +1.
        for (ASSET이_보호한_이번_호출의_관련_미인계_진단 각각) {
            if (!diagnostics군_안전 ||
                *diagnosticsWritten >= diagnosticsCapacity) {
                [원 진단·참조·미인계/실손실 추적을 기존 owner에 유지];
                break;
            }
            [diagnostics[*diagnosticsWritten]에 원 사실을 기록하고 소비 참조 보호];
            if (실제_기록과_필요_보호_성공) {
                *diagnosticsWritten += 1;
            } else {
                [미인계 유지; 실패한 공간을 유효 사실로 보이지 않게 한다];
                break;
            }
        }
        return VSS_ASSET_ACCESS_REJECTED; // 유효 spanWritten만 0; 기존 span 불변.
    } else {
        // A5: REFERENCE-END. 원 key 내용의 접근 가능성을 먼저 검증.
        if (referenceEnd_저장소를_안전하게_읽을_수_없음) {
            return VSS_ASSET_REFERENCE_STILL_PROTECTED;
        }
        if (!readerReferencesEnded ||
            실제_reader_대기_진행_참조종료_근거가_불충분 ||
            ASSET의_원span_귀속_대조가_불충분) {
            return VSS_ASSET_REFERENCE_STILL_PROTECTED;
        }
        for (해당_원span에_ASSET이_보호한_실제_consumer_참조 각각) {
            if (더_읽을_가능성_또는_대기_진행_참조가_남음) {
                return VSS_ASSET_REFERENCE_STILL_PROTECTED;
            }
        }
        [정확히 그 원 span의 종료만 반영하고 안전하게 끝난 참조를 회수];
        // A6: 새 span/새 Attempt/다른 consumer/원 image는 변경하지 않는다.
        return VSS_ASSET_REFERENCE_RELEASED; // 유효 spanWritten=0.
    }
}
```

<a id="protection"></a>
## 중단·OUT 부족과 사후조건

READ의 오류에는 가짜 성공 span을 만들지 않는다. validBytes는 실제 확인량이며 short span을 requestedBytes/버퍼 용량으로 늘리지 않는다. 진단은 [ASSET 발견 단계](../functions/Asset_Read.md#outcomes)로만 귀속한다. STREAM decoder corrupt/unsupported를 ASSET 오류로 바꾸지 않는다.

RELEASE에서 true는 실제 reader 종료 근거이며 ASSET의 원 span 참조 대조까지 필요하다. 현재 Attempt를 조회하여 옛 span을 재라벨링하지 않는다. Session retirement·새 READ·EOF는 회수 근거가 아니다. 진단·span 공간이 무효이면 그 count조차 읽지 않는다. 원 진단 보호 저장소가 부족해 실제 손실이 생겼으면 기존 손실 사실을 남기고 충분성 판정을 차단한다.

<a id="gates"></a>
## 구현 가능한 논리와 미정

ONE_OF·bounds·무효 OUT/short read·원 span 종료는 F1/F3 독립 정적·Host 후보다. 출처 있는 bounded 예산 주입은 F2 시험 후보이며 이번 시험은 실행하지 않았다. 제품 image/주소·정수 폭·참조 저장·직렬화·direct/staging은 F4 BINDING_TBD, 물리 메모리/SAI·DMA 안전은 F5 HW_UNVERIFIED다. reader 종료를 실제 출력 종료로 확대하지 않는다. 다른 Core의 D4-2/D5/D6 상세는 F6 별도 범위다.

## 연결 근거

[상세 계약·반환표](../functions/Asset_Read.md#contract) · [READ 분기](../functions/Asset_Read.md#bounded-read) · [실제 reader 종료](../functions/Asset_Read.md#reference-end) · [R2](../../../../design/30_FUNCTIONS/40_AUDIO_STREAM_ASSET_FUNCTIONS.md#asset-read) · [R3 span](../../../../design/40_DATA/50_AUDIO_STREAM_DATA.md#vssassetspan) · [R4 참조 보호](../../../../design/50_CONTRACTS/10_OWNERSHIP_LIFETIME.md#borrow-and-protect) · [R5](../../../../design/60_PSEUDOCODE/40_AUDIO_PREPARE_TX_FLOW.md#asset-read) · [R6](../../../../design/70_C_INTERFACE/00_HEADER_OWNERSHIP_MAP.md#r6-asset-read)
