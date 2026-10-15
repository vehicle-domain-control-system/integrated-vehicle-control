# Asset_Read — 압축 범위 읽기와 실제 consumer 참조 종료

> A1 입력 개정 2026-10-15 · [현재 Network 연결](../../../../design/90_BINDING/92_NETWORK_BINDING.md). `L=min(양수 USE_LIMIT,500ms,2000ms)`; 최초 actual에만 적용. 과거 동결/PASS는 당시 범위의 이력이며 본문 변경은 이번 A1 결정이다. 구현·시험 NOT_RUN.

[VSS 지도](../../../01_VSS_MAP.md) / [SERVICE](../../00_LAYER_OVERVIEW.md) / [ASSET](../00_MODULE_OVERVIEW.md) / CORE-09 · [한국어 의사코드](../pseudocode/Asset_Read.md)

> GUIDE · R6 논리 계약을 설명하며 실제 C ABI·제품 image 연결·Host/보드 시험은 미확정 또는 NOT_RUN이다.

<a id="contract"></a>
## 반환·인자와 R6 논리 계약

| 구분 | 타입과 의미 |
|---|---|
| 동기 반환 | `VssAssetReadDisposition`: 이 호출의 읽기 또는 참조 종료 처분. span·진단을 포함한 통합 성공 객체가 아니다 |
| READ 입력 | `const VssAssetReadRequest *request`: asset/image/attempt/offset/requestedBytes. 읽기 동안 안정된 원 요구 |
| REFERENCE-END 입력 | `const VssAssetSpanKeyType *referenceEnd`: 실제 consumer가 사용했던 원 span key |
| 값 입력 | `SemanticBool readerReferencesEnded`: RELEASE lane에서 실제 reader 및 대기/진행 참조 종료 근거. READ에서는 사용하지 않음 |
| span OUT | `VssAssetSpan span[capacity -> written]`: 성공 READ에만 정확히 1개. 자체 capacity/written과 bytes 참조 보호 |
| 진단 OUT | `VssDiagnosticEvidence diagnostics[capacity -> written]`: ASSET 발견 원 사실을 독립 전달. span 계수와 별개 |

`request`와 `referenceEnd`는 정확히 한 개만 non-NULL인 `ONE_OF`다. 0개·2개 조합은 입력을 역참조하거나 read/release를 실행하지 않는다. 유효 OUT은 별도 검증한 뒤 0으로 초기화한다.

```text
VssAssetReadDisposition Asset_Read(
    ONE_OF const VssAssetReadRequest *request,
           const VssAssetSpanKeyType *referenceEnd,
    VALUE SemanticBool readerReferencesEnded,
    OUT VssAssetSpan span[capacity -> written],
    OUT VssDiagnosticEvidence diagnostics[capacity -> written]);
```

이는 [최신 R6 원형](../../../../design/70_C_INTERFACE/00_HEADER_OWNERSHIP_MAP.md#r6-asset-read)이다. `ONE_OF`·`VALUE`·`OUT`은 논리 표기다. [capacity/written·borrow 규칙](../../../../design/70_C_INTERFACE/00_HEADER_OWNERSHIP_MAP.md#prototype-notation)에 따른 C 인자 전개는 후보이며 실제 Header·정수 폭·메모리 배치는 미정이다. R2의 provisional `VssAssetAccess/VssAssetReadResult`는 [R3에서 분리](../../../../design/40_DATA/50_AUDIO_STREAM_DATA.md#vssassetreaddisposition)됐으므로 public union/통합 결과로 복원하지 않는다.

<a id="ownership"></a>
## Owner·Caller/Callee와 보호 자료

ASSET은 현재 image/build의 [descriptor·metadata](../../../../design/40_DATA/50_AUDIO_STREAM_DATA.md#vssassetdescriptor), 실제 공급 범위와 consumer 참조의 단일 writer다. AUDIO_STREAM의 [Prepare](../../AUDIO_STREAM/functions/AudioStream_Prepare.md) 및 [후속 Advance 경계](../../../../design/30_FUNCTIONS/40_AUDIO_STREAM_ASSET_FUNCTIONS.md#audiostream-advance)가 호출한다. 현재 [Advance 상세의 참조 수명](../../AUDIO_STREAM/functions/AudioStream_Advance.md#references)에서 후속을 확인한다. callee는 read-only image와 기존 Runtime 경계이며 runtime Flash erase/program이나 새 Storage Driver를 호출하지 않는다.

| 원 자료 | 사용하는 핵심 필드·수명 |
|---|---|
| [VssAssetReadRequest](../../../../design/40_DATA/50_AUDIO_STREAM_DATA.md#vssassetreadrequest) | 원 asset/image/attempt, 압축 offset/requestedBytes. PCM 진행 위치가 아니다 |
| [VssAssetSpan](../../../../design/40_DATA/50_AUDIO_STREAM_DATA.md#vssassetspan) | span/asset/image/attempt/offset/validBytes/bytes. 제공 후 불변이며 실제 reader가 끝날 때까지 보호 |
| consumer 참조 | decoder의 현재·대기·진행 참조와 원 span 연결을 ASSET이 대조. 호출 인자 포인터 수명과 다름 |
| [진단 원 사실](../../../../design/40_DATA/70_DIAGNOSTIC_DATA.md#vssdiagnosticevidence) | 원 발견 producer·대상·단계·시각·참조를 consumer 반영/보호된 추적 종료까지 유지 |

ASSET은 decoder 소비 위치·PCM·PLAYBACK Session·Fault 원본을 쓰지 않는다. read-only compressed bytes는 CPU decode 입력이며 PCM DMA 전송 자료가 아니다. 실제 복사/direct Flash/staging 선택은 이 함수의 논리 계약으로 확정하지 않는다.

<a id="out-protection"></a>
## 독립 OUT 군과 실패 시 보호

각 OUT 군은 주소·capacity·written 저장소·전체 범위 및 다른 자료와의 비간섭을 확인한 뒤에만 `written=0`으로 초기화한다. capacity=0이면 배열 자료에 접근하지 않는다. 무효 군은 `written`도 역참조하지 않으며, 다른 안전한 군은 독립적으로 사용할 수 있다.

READ에서 span 군이 무효·공간 부족이면 자료를 공급한 척하지 않고 `ACCESS_REJECTED`다. 성공 span은 실제 bytes 공급·원 consumer 등록·전달 이후 필요한 보호를 모두 확보한 뒤에만 기록하며 `span.written=1`이다. 진단 군이 무효/0/full이어도 원 오류가 없어지는 것이 아니다. 유효 공간에 실제 기록하고 필요한 참조 보호까지 완료된 건만 진단 count를 올린다. 미인계 사실은 ASSET이 보호하며 실제 보존 손실은 기존 손실 근거로 관측 가능하게 남긴다. 새 전달 실패를 기존 span 회수 근거로 쓰지 않는다.

<a id="bounded-read"></a>
## READ — 검증한 compressed bytes만 공급

1. READ 입력의 접근 가능성을 확인하고 현재 asset/image/build descriptor·metadata·원 attempt 연결을 대조한다. 과거 Bring-up image와 현재 제품 image를 동일시하지 않는다.
2. offset/요청량의 산술 overflow와 실제 압축 범위를 검사한다. 합을 먼저 계산해 overflow시킨 뒤 검사하지 않는다. `offset > compressedLength`는 거부한다.
3. 유효한 원 요청에서 `offset == compressedLength`이면 실제 byte 읽기 없이 `END_OF_RANGE`다. `span.written=0`이며 decoder EOS·PCM 종료·Session 완료를 뜻하지 않는다. 시작 위치가 범위 안이면 `requestedBytes <= compressedLength - offset`을 검증한다.
4. 요청 범위·남은 자료·기존 bounded 공급 한도 안에서 실제 읽는다. 요청이 압축 범위를 넘는 것과, 적법한 범위 안에서 공급 한도/확인된 마지막 읽기로 더 짧은 span을 주는 것은 다르다. 유효 공급량·참조 보호 공간이 없거나 PFlash read가 실패하면 거부한다.
5. `validBytes`는 실제 확인한 길이다. requestedBytes/용량으로 padding하지 않는다. read-only bytes와 원 span identity·consumer 관계를 제공 전에 보호한 뒤에만 `SPAN_GRANTED`와 span 1개를 인계한다.

STREAM만 실제 소비한 compressed bytes에 따라 sourcePosition을 변경한다. ASSET offset·공급 길이는 소비 완료가 아니다. read 성공 뒤 decoder corrupt/unsupported가 발견되면 ASSET read 성공은 유지하고 STREAM 발견 오류로 다룬다. 다른 Asset fallback은 없다.

<a id="reference-end"></a>
## REFERENCE-END — 원 span만 안전 종료

`readerReferencesEnded=false` 또는 근거 미확인은 `REFERENCE_STILL_PROTECTED`다. true도 단독 해제 허가가 아니다. 원 span key와 ASSET이 보호한 원 consumer/attempt·대기·진행 참조를 대조하여 더 읽을 가능성이 없음을 확인한다. 다른 span/새 Attempt의 근거를 대신 사용하지 않는다.

모두 종료됐으면 해당 원 span/staging 참조만 `REFERENCE_RELEASED`로 반영한다. 현재 새 span, 다른 consumer, read-only image는 변경하지 않는다. 오래된 span의 정리 결과는 그 원 span에만 귀속한다. Session retirement·새 READ·EOF·출력 종료는 실제 reader 종료의 대체 근거가 아니다.

<a id="outcomes"></a>
## 반환과 사후조건

| 반환 | span.written / 원 상태 |
|---|---|
| `VSS_ASSET_SPAN_GRANTED` | 검증·공급·참조 보호를 끝낸 READ에서만 1 |
| `VSS_ASSET_END_OF_RANGE` | 유효 span OUT의 count 0. compressed 범위 끝만 확인 |
| `VSS_ASSET_ACCESS_REJECTED` | 유효 span OUT의 count 0. ASSET 발견 오류/미인계 사실 및 기존 참조 보호 |
| `VSS_ASSET_REFERENCE_RELEASED` | 유효 span OUT의 count 0. 실제 종료를 확인한 원 span만 반영 |
| `VSS_ASSET_REFERENCE_STILL_PROTECTED` | 유효 span OUT의 count 0. 원 span/reader 수명 유지 |

무효 OUT의 count를 0이라고 읽거나 썼다고 주장하지 않는다. lookup/metadata/bounds/PFlash read 실패는 ASSET 발견 사실이고 decode 실패는 STREAM, 장치 준비 실패는 CONTROL 발견 사실이다. 원 producer·발생 시각/귀속을 유지하며 처리 현재 시각으로 덮지 않는다.

<a id="implementation-boundary"></a>
## 독립 검증 후보와 제품 Gate

| 분류 | 진행 가능한 논리·제한 |
|---|---|
| F1_LOCKED_SEMANTIC | ONE_OF, 산술 bounds, 원 identity, 실제 reader 참조 종료, typed 반환/OUT 불변조건 |
| F2_PROVISIONAL_TESTABLE | 출처가 있는 bounded read/보호 예산을 시험 fixture로 주입하는 후보. 새 제품 수치를 만들지 않음 |
| F3_PARTIAL_INDEPENDENT | short span, 무효/부족 OUT, read 실패·중복/late release를 stub 사실로 검증하는 후보 |
| F4_EXTERNAL_GATE | 현재 제품 table/image/source 매핑, C ABI·주소/길이 타입·보호 저장소·직렬화·direct/staging은 BINDING_TBD |
| F5_PHYSICAL_SAFETY_GATE | 실제 메모리 보호·DMA/SAI/STOP 충분성과 P2는 HW_UNVERIFIED/HOLD. 압축 참조 종료만으로 해제 불가 |
| 인접 상세 | [STREAM RequestControl](../../AUDIO_STREAM/functions/AudioStream_RequestControl.md), [STREAM Advance](../../AUDIO_STREAM/functions/AudioStream_Advance.md), [TX Request](../../../03_DRIVER/AUDIO_TX/functions/AudioTx_Request.md), [HEALTH Evaluate](../../HEALTH/functions/Health_Evaluate.md)·[BuildStatus](../../HEALTH/functions/Health_BuildStatus.md)에서 각각의 계약을 읽음 |

F1~F3는 별도 코드/Host 검증의 착수 후보이며 실제 시험은 NOT_RUN이다. 실제 시험 방법·단일 잠정 정의·교체/철회 조건은 해당 별도 인계에서 추적한다. 원 [발생→최초 actual의 잠정 <L](../../../../design/50_CONTRACTS/30_TIMING_FRESHNESS.md#first-start)를 read 완료 시각으로 갱신하지 않는다. P1B_C1/Q/R6/외부 IF 미해결과 WINDOW PRODUCT 보류·TEST 격리를 보존한다.

## 정본으로 이어 읽기

[분기 의사코드](../pseudocode/Asset_Read.md#procedure) · [R2 함수](../../../../design/30_FUNCTIONS/40_AUDIO_STREAM_ASSET_FUNCTIONS.md#asset-read) · [R3 자료](../../../../design/40_DATA/50_AUDIO_STREAM_DATA.md#asset) · [R4 borrow/보호](../../../../design/50_CONTRACTS/10_OWNERSHIP_LIFETIME.md#borrow-and-protect) · [R4 실제 참조 종료](../../../../design/50_CONTRACTS/10_OWNERSHIP_LIFETIME.md#references-and-release) · [R5 READ/참조](../../../../design/60_PSEUDOCODE/40_AUDIO_PREPARE_TX_FLOW.md#asset-read) · [R6 논리 원형](../../../../design/70_C_INTERFACE/00_HEADER_OWNERSHIP_MAP.md#r6-asset-read)
