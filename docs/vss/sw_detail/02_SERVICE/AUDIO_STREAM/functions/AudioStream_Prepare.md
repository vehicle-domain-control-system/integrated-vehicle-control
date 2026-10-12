# AudioStream_Prepare — 원 준비 수용과 안전한 초기 PCM

[VSS 지도](../../../01_VSS_MAP.md) / [SERVICE](../../00_LAYER_OVERVIEW.md) / [AUDIO_STREAM](../00_MODULE_OVERVIEW.md) / CORE-10 · [한국어 의사코드](../pseudocode/AudioStream_Prepare.md)

> GUIDE · SELF_CHECKED / GD4_1_PENDING. 원 준비·CPU prime의 논리 설명이며 R6 실제 ABI와 제품/HW 안전성은 미확정이다. 실제 C·Host·보드 시험 NOT_RUN.

<a id="contract"></a>
## 반환·인자와 R6 논리 계약

| 구분 | 타입·역할·수명 |
|---|---|
| 동기 반환 | `VssRequestDisposition`: STREAM이 이 준비 의도를 ADOPTED/KEPT/WAIT/REJECTED로 처리한 결과. 준비 stage나 actual과 독립 |
| 입력 | `const VssAudioPreparation *preparation`: 원 session/attempt/plan/position/asset 및 필요한 최초 firstStartMeta. 호출 인자와 내부 borrowed 참조를 효력 전에 검증 |
| 준비 OUT | `VssAudioRequestResult requests[capacity -> written]`: 원 session/attempt의 독립 stage 사실. 생성 후 불변·PLAYBACK 반영/보호된 추적 종료까지 보호 |
| 진단 OUT | `VssDiagnosticEvidence diagnostics[capacity -> written]`: 원 발견 producer·단계·대상/시각의 독립 사실. 준비 count와 별개 |

```text
VssRequestDisposition AudioStream_Prepare(
    IN const VssAudioPreparation *preparation,
    OUT VssAudioRequestResult requests[capacity -> written],
    OUT VssDiagnosticEvidence diagnostics[capacity -> written]);
```

[최신 R6](../../../../design/70_C_INTERFACE/00_HEADER_OWNERSHIP_MAP.md#r6-audiostream-prepare)는 동기 scalar와 두 OUT 군을 분리한다. R5의 단일 result 설명을 통합 BackendResult로 구현하지 않는다. [R6 표기·범위 규칙](../../../../design/70_C_INTERFACE/00_HEADER_OWNERSHIP_MAP.md#prototype-notation)은 논리 후보이며 compile-ready C·Header/폭·저장 방식 확정이 아니다.

<a id="ownership"></a>
## Owner·Caller/Callee와 원 자료

AUDIO_STREAM은 [provider](../../../../design/40_DATA/50_AUDIO_STREAM_DATA.md#vssprovidercontext)의 소비 진행/phase/productionAllowed와 [정확히 A/B 두 PCM 상태](../../../../design/40_DATA/50_AUDIO_STREAM_DATA.md#vsspcmbufferstate)의 내용/usage를 쓴다. PLAYBACK의 [RequestTransition](../../PLAYBACK/functions/Playback_RequestTransition.md)·[Advance](../../PLAYBACK/functions/Playback_Advance.md)가 준비 의도를 전달하며 같은 준비의 후속은 기존 [AudioStream_Advance 경계](../../../../design/30_FUNCTIONS/40_AUDIO_STREAM_ASSET_FUNCTIONS.md#audiostream-advance)가 진행한다.

| 자료/경계 | 반드시 보존할 원본 |
|---|---|
| [VssAudioPreparation](../../../../design/40_DATA/50_AUDIO_STREAM_DATA.md#vssaudiopreparation) | 원 session/attempt·의미 plan·position·asset 관계. firstStartMeta는 처음 시작에 필요한 원 Meta만 |
| [VssProviderContext](../../../../design/40_DATA/50_AUDIO_STREAM_DATA.md#vssprovidercontext) | session/attempt/asset/sourceSpan/sourcePosition/format/phase/productionAllowed/endOfSource. STREAM 단일 writer |
| [Asset_Read](../../ASSET/functions/Asset_Read.md) | ASSET의 image/bounded bytes·span 참조 원본. STREAM은 실제 decoder 소비 위치만 변경 |
| [AudioControl_Service](../../../../design/70_C_INTERFACE/00_HEADER_OWNERSHIP_MAP.md#r6-audiocontrol-service) | CONTROL의 현재 device/config readiness·원 제어 사실. void 반환은 READY가 아님 |
| [AudioTx_Request](../../../../design/70_C_INTERFACE/00_HEADER_OWNERSHIP_MAP.md#r6-audiotx-request) | 필요한 초기 PCM 인계의 typed lane. TX만 operation·장치 retain을 씀. 준비 인계 수용은 START 활성화가 아님 |
| PLAYBACK | Session/Attempt 및 실제 started/whole final 판단. STREAM이 그 원본이나 STORE ledger를 직접 쓰지 않음 |

plan/Meta/span/samples의 포인터를 얕게 복사한 것만으로 후속 수명을 확보했다고 하지 않는다. [실제 consumer 수명 보호](../../../../design/50_CONTRACTS/10_OWNERSHIP_LIFETIME.md#borrow-and-protect)는 하위 요청 효력 전에 필요하다. decoder는 기존 STREAM 내부 local step이며 `AudioStream_FillBuffer`를 새로운 Core/public API로 만들지 않는다.

<a id="admission"></a>
## 검증·중복 준비·현재 차단

preparation 외피를 읽기 전에 주소/범위를 확인하고, 내부 plan/Meta 등 실제 참조의 접근 가능성·귀속·수명을 차례로 검증한다. 원 session/attempt/plan/position/asset 연결이 부적합하면 새 의도를 REJECTED로 거부한다. 새로 이용할 provider/PCM 자원의 실제 안전 종료와 새 보호 예산이 미충족이면 WAIT이며 새 위험 있는 prime을 실행하지 않는다. 옛 불변 callback 귀속 기록은 alias 없는 별도 보호로 남을 수 있으며, 모든 기록의 동시 폐기를 강제하지 않는다.

동일 preparation이 이미 수용됐으면 원 identity/시간·decoder/plan을 유지한 채 pending 부분만 제한적으로 진행한다. 동일 Attempt에 의미가 다른 plan/asset/position을 덮지 않는다. 이미 PREPARED인 동일 문맥의 최신 준비가 여전히 유효하면 기존 상태를 KEPT로 유지한다. 취소/STOP·CLOSING 또는 productionAllowed 차단된 동일 원 preparation의 재호출은 동기 KEPT로 기존 차단·정리를 유지한다. 이와 별개로 적법한 원 Session/Attempt의 `VSS_AUDIO_REQUEST_REJECTED` stage를 독립 보호·인계하며 새 준비/생산/제출이나 새 age/Attempt를 만들지 않는다. KEPT는 기존 차단 유지이지 준비 성공이 아니다. 별도의 부적합한 신규 의도는 동기 REJECTED로 거부하며, 원 귀속이 없는 입력에 가짜 stage를 생성하지 않는다. 이번 재요구에 옛 PREPARED를 준비 성공으로 재발행하지 않고 기존 원 사실/미인계·정리 의무는 계속 보호한다.

새 요구를 적법하게 수용할 때만 하위 호출 전에 원 preparation·provider·후속 참조/사실의 보호를 확보한다. 그 뒤 provider의 원 session/attempt/asset·PREPARING·productionAllowed를 원 근거로 반영하고 ADOPTED와 PREPARE_RECEIVED를 별개로 보호한다. 최초 Meta·원 age를 재시도 시각으로 갱신하지 않는다. 정상 Started Session의 후속 cue에는 새로운 최초 firstStartMeta를 생성하지 않는다.

<a id="read-and-consume"></a>
## 현재 장치 준비와 압축 자료 소비

현재 필요한 device/config만 기존 CONTROL 경계로 준비한다. 이미 유효한 READY는 재사용하고 매 Attempt 전체 장치 reset을 강제하지 않는다. 이전 config의 READY나 void 반환은 현재 준비 충분조건이 아니다. 구성 준비 미확인은 PENDING, 확인된 현재 구성 실패는 producer CONTROL의 원 사실로 보존한다.

[Asset_Read](../../ASSET/functions/Asset_Read.md#bounded-read)는 원 image/attempt와 bounded 요청을 검증한다. `SPAN_GRANTED` 및 span 1개·bytes 참조가 보호됐을 때만 CPU decoder가 읽는다. END_OF_RANGE는 compressed 끝만 표시하며 decoded 유효 frame/실제 출력 종료를 뜻하지 않는다. 거부는 ASSET 원 진단과 STREAM 준비 실패를 연결하되 원 producer를 바꾸지 않는다.

sourcePosition은 **실제로 소비한 compressed bytes만** 따라간다. ASSET의 제공 offset/validBytes 전체를 즉시 소비한 것으로 보지 않는다. bytes와 span descriptor는 실제 reader·대기/진행 작업이 종료할 때까지 유지한다. RELEASE는 그 사실을 원 span key로 ASSET에 전달하고 ASSET이 다시 대조한다. false/불충분/STILL_PROTECTED이면 기존 참조를 유지한다.

<a id="prime-safety"></a>
## 초기 prime — PCM A/B의 현재 쓰기 기회

MP3 PFlash bytes → CPU decoder → PCM A/B → TX/eDMA/SAI → Codec 경로다. compressed bytes를 PCM으로 DMA에 넘기거나 Codec에 직접 보내지 않는다.

A와 B 각각의 원 attempt/buffer/cycle을 확인하여, 아래 조건을 **실제 쓰기 시점마다** 모두 만족할 때만 bounded decode/갱신한다.

- 해당 옛 회차의 실제 접근·참조 종료/옛 내용 재접근 금지 근거가 충분하다.
- usage가 `VSS_PCM_CPU_WRITABLE`이며 차기 DMA 방문 전에 실제 쓰기를 끝낼 수 있는 현재 기회와 메모리/참조 조건이 확인됐다.
- 원 provider의 productionAllowed와 현재 원 귀속·미취소 조건이 유효하다.

`HW_PROTECTED`·`RETURN_PENDING`·`HANDOFF_PENDING`에는 CPU가 쓰지 않는다. 옛 IRQ/consumed/returned·회차 번호 교체·CPU_WRITABLE은 무기한 현재 쓰기 허가가 아니다. 안전 구간 만료/미확인/실패에서는 쓰지 않고 보호된 후속·PENDING으로 남긴다. 두 버퍼의 안전·format/용량·cache/barrier/alignment·기한은 실물 Gate다.

| decode 결과 | PCM·인계 의미 |
|---|---|
| 실제 decoded frame >0 | 확인된 firstValidFrame/validFrameCount/format과 원 cycle을 안정시키고 CPU_READY. sample 값 0도 실제 decoded 유효 frame |
| decoded 유효 frame 0 | 새 sound handoff 없음. 지속 DMA STOP·NO_START·전체종료 근거 아님 |
| 마지막 short tail·빈 B | 실제 유효 수만 유지. full 용량 padding·반복·두 sound block 강제 없음 |
| 물리 zero-fill | 현재 쓰기 안전이 확인된 기존 A/B 범위의 정리만. validFrameCount/재생횟수를 늘리지 않음 |

초기 준비에 실제 PCM 하위 인계가 필요하면, validFrameCount>0인 안정된 samples/유효범위/format/key와 원 handoff 참조를 먼저 보호한다. **그 보호를 마친 뒤 DRIVER 부수효력 직전**, 원 Session/Attempt/A 또는 B/cycle·productionAllowed·STOP/취소·현재 CPU 쓰기 안전 및 다음 DMA 방문 전 안정성을 다시 확인한다. 부족·변화·안전구간 만료이면 TX를 호출하지 않고 새 sound handoff를 중단하며, 원 PCM/cycle·이미 확보한 부분 참조·해당 usage의 보호/정리 후속을 유지한다. 이 경로를 하위 무효력 거부/no-access·출력 종료로 꾸미거나 CPU 쓰기 권한을 임의 회수하지 않는다. 조건을 통과할 때만 DRIVER 호출 **전에 HANDOFF_PENDING**으로 보호한다. 최종 확인부터 하위 효력까지 근거를 유효하게 유지하는 직렬화·원자성·메모리 장벽/실물 타이밍 수단은 BINDING_TBD/HW_UNVERIFIED이며 여기서 확정하지 않는다. 정확한 R6 PCM lane만 연결하며 START 제어를 생성하지 않는다. 새 무효력 거부/안전 반환은 원 operation/cycle의 충분 근거와 현재 쓰기 조건을 각각 확인하고 보호한다. 수용·소비·반환은 prepared/actual을 자동 증명하지 않는다. 상세 제어/Advance는 D4-2·D5 범위다.

<a id="stages"></a>
## 동기 disposition과 보호된 준비 단계

| 사실 | 이 함수에서의 의미·반영 경계 |
|---|---|
| `VSS_REQUEST_ADOPTED` | STREAM이 새 준비 문맥을 적법하게 수용. 그 호출에서 PENDING/실패가 함께 생길 수 있음 |
| `VSS_REQUEST_KEPT` | 같은 원 진행·유효 준비 또는 차단 상태 유지. 신규 identity/기한 생성 없음 |
| `VSS_REQUEST_WAIT` | 최신 조건/old 자원/보호 예산 때문에 새 수용 대기. stage나 actual을 합성하지 않음 |
| `VSS_REQUEST_REJECTED` | 부적합한 새 준비 의도 거부. 기존 사실/정리 의무는 유지 |
| `VSS_AUDIO_PREPARE_RECEIVED` | 원 준비 수용 사실 |
| `VSS_AUDIO_PREPARE_PENDING` | 이미 수용한 준비의 조건·후속 미완료 |
| `VSS_AUDIO_PREPARED` | 원 provider·현재 음원/format·필요 초기 유효 PCM·현재 장치 구성·필요 TX 준비가 모두 확인됨 |
| `VSS_AUDIO_PREPARE_FAILED` | 수용 뒤 확인된 실패. 부분 참조/장치 보호·정리 의무가 남음 |
| `VSS_AUDIO_REQUEST_REJECTED` | 적법한 원 귀속을 검증한 새 거부 사실이 있을 때만. 무효 인자에 가짜 identity를 생성하지 않음 |

[request stage](../../../../design/40_DATA/50_AUDIO_STREAM_DATA.md#vssaudiorequeststage)는 원 사실이며 [실제 output evidence](../../../../design/50_CONTRACTS/20_ASYNC_RESULT_EVIDENCE.md#stages)와 다르다. `ADOPTED != PREPARED != START_ACCEPTED != OUTPUT_START_CONFIRMED`. 두 개의 full PCM block이 PREPARED의 무조건 전제는 아니다. 형식/장치/물리 초기 준비 충분성 미확인은 PENDING으로 보호하며 actual로 승격하지 않는다. 늦은 PREPARED는 이미 적용한 actual/종료를 되돌리지 않는다.

<a id="outputs-and-failure"></a>
## 독립 OUT·실패·부분 효력

requests/diagnostics는 군별 주소·capacity·written 저장소·전체 범위·비간섭을 확인한 뒤에만 각 count를 0으로 초기화한다. 무효 군은 written까지 무접근이다. 유효 군에 실제 기록하고 전달 후 필요한 참조 보호까지 확보한 건만 증가시킨다. capacity=0/full/무효 때문에 PREPARED나 원 진단이 없어진 것으로 판단하지 않는다. producer의 미인계 보호·기존 후속을 유지하고, 실제 보존 손실은 기존 coverage/진단으로 드러내 충분성 판단을 막는다. 새 stage 보호 예산 부족이면 하위 효력 전 수용/진행을 제한하며 기존 사실을 덮지 않는다.

ASSET lookup/metadata/bounds/read 오류, STREAM corrupt/unsupported/decoder 오류, CONTROL device/config 실패를 각 producer의 원 target/operation/시각으로 구분한다. 확인된 수용 후 실제 ASSET read·STREAM decode/provider·CONTROL 준비 실패만 provider.productionAllowed=false·CLOSING 및 원 PREPARE_FAILED/producer 진단으로 연결한다. 준비 도중 정상 STOP/취소·생산 차단만 발생했으면 같은 생산 차단·CLOSING과 참조 정리는 유지하되 PREPARE_FAILED·실패 진단/Fault를 합성하지 않는다. 불명확한 하위 부분 효력이나 안전 근거 부족도 준비 성공·완전 취소로 표시하지 않고 원 사실/보호·후속으로 남긴다. 이미 인계한 PCM·하위 장치·ASSET span·decoder 작업의 참조는 실제 안전 종료까지 유지하며 즉시 rollback/fallback하지 않는다. 기존 STOP 차단 문맥에 Prepare를 다시 호출하거나 옛 callback을 새 Attempt에 붙여 재시작하지 않는다.

## 구현 후보·미정과 연결 근거

F1은 identity/단일 writer/분리된 stage·OUT·취소 불변조건, F2는 출처 있는 bounded fixture·safe-write 조건의 시험 주입, F3는 short-tail·OUT 부족·read/decode 실패·취소 경합의 stub 검증 후보다. 실제 Host/ASan/Build 시험은 NOT_RUN이다. F4 제품 source/image·C ABI/보호 저장/직렬화, F5 현재 DMA 방문 안전/WCET·SAI/STOP/actual·메모리 조건은 BINDING_TBD/HW_UNVERIFIED로 해당 연결만 보류한다. F6 D4-2 RequestControl/Advance·D5 TX·D6 HEALTH는 별도 범위다. P1B_C1/Q/R6/외부 IF 미해결·P2 HOLD·WINDOW PRODUCT 보류/TEST 격리 및 원 [최초 actual <2초 잠정 조건](../../../../design/50_CONTRACTS/30_TIMING_FRESHNESS.md#first-start)을 유지한다.

[의사코드 분기](../pseudocode/AudioStream_Prepare.md#procedure) · [R2 Prepare](../../../../design/30_FUNCTIONS/40_AUDIO_STREAM_ASSET_FUNCTIONS.md#audiostream-prepare) · [R3 준비](../../../../design/40_DATA/50_AUDIO_STREAM_DATA.md#vssaudiopreparation) · [R3 provider](../../../../design/40_DATA/50_AUDIO_STREAM_DATA.md#vssprovidercontext) · [R4 현재 쓰기 안전](../../../../design/50_CONTRACTS/50_AUDIO_BUFFER_TRANSPORT.md#safe-write) · [R4 유효 PCM/무음](../../../../design/50_CONTRACTS/50_AUDIO_BUFFER_TRANSPORT.md#valid-and-silence) · [R5 Prepare](../../../../design/60_PSEUDOCODE/40_AUDIO_PREPARE_TX_FLOW.md#audiostream-prepare) · [R5 내부 prime](../../../../design/60_PSEUDOCODE/40_AUDIO_PREPARE_TX_FLOW.md#fill-buffer) · [R6](../../../../design/70_C_INTERFACE/00_HEADER_OWNERSHIP_MAP.md#r6-audiostream-prepare)
