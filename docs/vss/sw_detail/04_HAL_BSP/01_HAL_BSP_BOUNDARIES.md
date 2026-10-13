# HAL/BSP·DRIVER·RTD — 원등록과 물리 근거의 연결

[VSS 지도](../01_VSS_MAP.md) · [DRIVER](../03_DRIVER/00_LAYER_OVERVIEW.md) · [HAL/BSP Layer](00_LAYER_OVERVIEW.md)

> GUIDE · 승인된 논리 계약을 실제 source-basis와 연결하는 설명. 현재 제품의 C ABI·DMA·출력 안전은 `BINDING_TBD / HW_UNVERIFIED`다.

<a id="scope"></a>
## 책임과 읽는 순서

HAL/BSP는 5 Layer 중 하나이며 AUDIOHAL_BOUNDARY는 그 **내부 Boundary**다. 10 Module·18 Core 정의와 기존 16 Core 문서쌍은 그대로다. BSP의 정적 보드/generated·이미지 배치 근거와 HAL의 진행 중 원등록·raw 수명은 합치지 않는다.

| 연결 | 원본 writer와 후속 소비 | 실제 결합의 위치 |
|---|---|---|
| STREAM → TX → HAL/vendor | STREAM: provider·PCM 내용/usage·생산 권한. TX: 개별 operation·장치 retain·원 결과 | [Prepare](../02_SERVICE/AUDIO_STREAM/functions/AudioStream_Prepare.md#prime-safety)·[RequestControl](../02_SERVICE/AUDIO_STREAM/functions/AudioStream_RequestControl.md#stop)·[Advance](../02_SERVICE/AUDIO_STREAM/functions/AudioStream_Advance.md#pcm-return) → [TX Request](../03_DRIVER/AUDIO_TX/functions/AudioTx_Request.md#registration)·[TX Advance](../03_DRIVER/AUDIO_TX/functions/AudioTx_Advance.md#sufficiency) |
| STREAM → CONTROL → HAL/Codec·Clock | CONTROL: 장치별 현재 configuration readiness·원 제어. TX와의 조율은 STREAM | [Control Service](../03_DRIVER/AUDIO_CONTROL/functions/AudioControl_Service.md#ownership). 초기 준비·Recovery에는 PB Session이 없을 수 있음 |
| vendor/IRQ → HAL raw → Driver 후속 → STREAM | HAL: 불변 원등록·원 관측/손실. TX/CONTROL: 각각 의미 적용. PB: 전체 Session. HEALTH: Fault/제한 | [HAL Callback](AUDIOHAL_BOUNDARY/functions/AudioHAL_Callback.md#ownership). ISR 안에서 Driver/Service/Core를 호출하거나 상위 원본을 쓰지 않음 |
| BSP → HAL 및 ASSET build-time 참조 | BSP: source/generated·RTD·Clock/PinMux/IRQ·linker/MPU/cache·이미지 대응 | [동결 HAL/BSP](../../design/10_LAYERS/40_HAL_BSP_LAYER.md)·[내부 Boundary](../../design/10_LAYERS/details/40_HAL_BSP_BOUNDARY.md). ASSET 참조를 runtime Flash 호출로 바꾸지 않음 |

R6의 [TX Request](../../design/70_C_INTERFACE/00_HEADER_OWNERSHIP_MAP.md#r6-audiotx-request)·[TX Advance](../../design/70_C_INTERFACE/00_HEADER_OWNERSHIP_MAP.md#r6-audiotx-advance)·[CONTROL](../../design/70_C_INTERFACE/00_HEADER_OWNERSHIP_MAP.md#r6-audiocontrol-service)·[HAL Callback](../../design/70_C_INTERFACE/00_HEADER_OWNERSHIP_MAP.md#r6-audiohal-callback)는 논리 원형이다. 실제 vendor/generated signature·C 타입/폭/배치·public symbol을 확정하지 않는다. 재사용 후보는 [R6 include 방향](../../design/70_C_INTERFACE/00_HEADER_OWNERSHIP_MAP.md#include-direction)을 지키며 새 API·wrapper·struct·Queue·Task·전송 버퍼를 추가하지 않는다.

<a id="source-basis"></a>
## 지금 있는 근거와 다음에 필요한 근거

`VSS@f5d7361f42c4f7340a5411560f47e5d0ac504382`의 추적 파일과 현 작업본에는 제품 C/H 및 ELF/map/bin/hex가 없다(`SOURCE_NOT_PRESENT_AT_TARGET`). 따라서 아래 모든 경계의 **현재 제품 source/image·ABI 대조는 `SOURCE_NOT_VERIFIED / BINDING_TBD`**, 물리 충분성은 `HW_UNVERIFIED`다.

설치된 **2026-10-07 S32K344 bring-up 참고 108파일의 출처·SHA를 대조**한 뒤 아래 원문을 읽었다. 확인 수준은 `REFERENCE_SOURCE_OBSERVED`이며 과거 재생 기록·파일 이름·debug flag는 현재 제품/보드 완료 근거가 아니다. 행별 SHA·symbol·정확한 행 범위는 검토 증거에 보존한다. 이 표는 재사용 후보의 위치를 보여 준다.

| 경계 / 기존 Core·동결 계약 | 실제 참고에서 읽은 사실 | 추가로 필요한 원등록·회차·시간/충분조건 | 차단 효과 / 결정 owner·Gate |
|---|---|---|---|
| PCM 생산·인계 / CORE-10~12, [R5 fill](../../design/60_PSEUDOCODE/40_AUDIO_PREPARE_TX_FLOW.md#fill-buffer) | `Mp3PcmProvider.c`의 compressed window/CPU decode·소비 offset, `AudioPlayer.c`의 A/B render·zero-fill | 현재 image/span/reader 수명, 실제 유효 frame, 원 Attempt/A·B/cycle, 쓰기 직전 STOP/권한·메모리·차기 방문 근거 | 근거 없는 refill/인계 차단. STREAM/ASSET·F4/F5; key·tail·차단 논리는 F1/F3 후보 |
| 지속 TX / CORE-13/14, [R4 지속 등록](../../design/50_CONTRACTS/50_AUDIO_BUFFER_TRANSPORT.md#continuous-stream) | `AudioDmaTransport.c`의 단일 `Sai_Ip_Send`·지속 TCD 설정, Service/IRQ의 A/B phase·active/free 확인 | 현재 active/pending descriptor와 개별 operation/Attempt/PCM cycle의 실제 대응·등록/처리 시간·binding, wrap/late alias 방지 | 대응 없는 actual/소비/반환 차단. TX/HAL/BSP·F4/F5; old key 논리는 F1/F3 후보 |
| ISR 등록·dispatch / CORE-16, [R3 raw](../../design/40_DATA/60_AUDIO_TX_DATA.md#vssrawobservation) | generated DMA/SAI callback 연결, `IntCtrl_Ip_Cfg.c`, `Sai_Ip_Isr.c`·`Dma_Ip_Irq.c`의 IRQ 경로 | 현재 RTD/header/config/실행 이미지 일치, 효력 전 immutable 등록, raw 원시각/coverage, bounded 보존·loss 신호·IRQ/WCET/직렬화 | 상관/시각/손실 미확인 범위의 confirmed 생성 차단. HAL/Runtime/BSP·F4/F5; 두 OPTIONAL·loss 분류는 F1/F3 후보 |
| 옛 PCM 반환·현재 CPU 쓰기 / CORE-12/14, [R4 safe-write](../../design/50_CONTRACTS/50_AUDIO_BUFFER_TRANSPORT.md#safe-write) | Transport의 active-half 재확인·late/overlap counter, non-cacheable 선언과 참고 linker section | 옛 내용 재소비 금지·모든 HW/SW refs 종료와 현재 다음 방문 전 bounded 쓰기 완료를 각각 입증; 실제 map/MPU/cache/barrier/alignment·IRQ 경합 | 충분하지 않으면 usage/보호 유지·write/zero-fill 차단. STREAM/TX/HAL/BSP·F4/F5; 반환 적용 분리는 F1/F3 후보 |
| STOP·물리 출력 / CORE-11~14, [R4 충분조건](../../design/50_CONTRACTS/20_ASYNC_RESULT_EVIDENCE.md#confirmed-outcomes) | `AudioPlayer.c`의 STOP 시 무음 공급·terminal/silent flag; RTD `Sai_Ip_AbortSendingDma`/`AbortSending`의 요청 차단·FIFO 대기/reset·TX disable 분기 | 현재 원 targets 전체·잔류 descriptor/FIFO/frame, 과거 관측 coverage·미래 출력 차단·실제 출력 끝 및 아날로그 경계의 상관 | flag/ACK/abort만으로 NO_START/TERMINATION/retirement 불가. TX/STREAM/PB·HAL/BSP/보드·F4/F5; scope 논리는 F1/F3 후보 |
| 장치 준비 / CORE-15, [R3 device/config](../../design/40_DATA/60_AUDIO_TX_DATA.md#vssdevicereadinessstate) | `AudioCodec.c/.h`의 Probe/Configure·config flag, `AudioClock.c/.h`의 Reference/CS2100·lock/ref tick | 현재 세 장치/config별 실제 순서·source/image/상태/무효화·원 control binding·시각 및 효과 | 한 장치/옛 config 성공을 다른 READY로 확대 금지. CONTROL/HAL/BSP·F4/F5; 부분/old 결과 논리는 F1/F3 후보 |
| Recovery / CORE-11/12/15, [R4 현재 허용](../../design/50_CONTRACTS/40_FAULT_RECOVERY.md#current-target) | 참고의 초기 제어 함수·flag를 읽음. 승인 6필드 permission와 독립 performed/verified의 현재 제품 구현 근거는 미제공 | 원 현재 허용·old 실제 안전화, action 매핑, 수행 operation/occurredAt와 별도 효과/verifiedAt·HEALTH current pair | 미확인 수행·성공 verified·Fault clear 차단. HEALTH/FLOW/STREAM/CONTROL·정책/F4/F5; 허용·pair 논리는 F1/F3 후보 |
| 실행 이미지·RTD/메모리·시간 / [R6 TBD](../../design/70_C_INTERFACE/00_HEADER_OWNERSHIP_MAP.md#tbd-register) | 참고 RTD header/API 및 generated·linker를 정적 열람. 현 VSS 대응 ELF/map·MPU 실적용/계측은 미제공 | 정확한 현재 source/RTD revision·config→image hash·symbol/section·실제 메모리/시간 domain/epoch/연속성·WCET·전원/아날로그 계측 | 관련 물리 효과만 활성화 보류. 제품/HAL/BSP/보드 owner·F4/F5; 독립 논리 후보는 유지 |

R6 작성 당시에는 Codec/Clock Header 본문이 미제공이었다. 이번 설치 참고본에는 해당 `.h`가 실제 있고 vendor 중립 선언을 관측했다. **그 날짜의 참고 선언 확인**이며 현재 R3/R6 typed 계약 구현·transitive ABI/현재 제품 결합 확인은 아니다. 동결 R6의 과거 출처 설명을 소급 수정하지 않는다.

<a id="registration"></a>
## 원등록은 효력 전에 보호하고 독립 회수한다

TX의 원 `registration/operation/attempt/pcm/binding`과 Device의 `registration/operation/device/configuration/binding`은 각각 **HW retain/arm/요청 및 즉시 callback보다 먼저** 불변 보호한다. 호출이 끝난 intent/pcm 포인터나 현재 context 주소가 이 보호를 대신하지 않는다. [R3 두 등록](../../design/40_DATA/60_AUDIO_TX_DATA.md#vsshaltxregistration)·[R6 수명](../../design/70_C_INTERFACE/00_HEADER_OWNERSHIP_MAP.md#lifetime)을 따른다.

HAL Callback의 originalTx/originalDevice는 각각 OPTIONAL이다. 둘 다 없음, Tx만, Device만, 둘 다 있음이 가능하다. Tx만으로 device READY를 만들거나 Device만으로 TX Attempt/PCM을 만들지 않는다. 양쪽이 있으면 각 key/binding·수명·이번 raw의 상관을 독립 확인하며 한 current identity로 합치거나 raw를 양쪽 사실로 임의 복제하지 않는다. 없는/불일치 등록은 current Session/Attempt/config로 채우지 않는다.

vendor 지속 SAI/eDMA 등록은 여러 SW operation/Attempt/cycle을 가로지를 수 있다. vendor token·A 주소·HALF/MAJOR phase만으로 개별 회차가 확인되지 않으며, 매 START vendor stop/start나 vendor↔SW 기록 1:1을 강제하지 않는다. [CORE-16 등록 대응](AUDIOHAL_BOUNDARY/functions/AudioHAL_Callback.md#registration)과 [TX Advance](../03_DRIVER/AUDIO_TX/functions/AudioTx_Advance.md#ownership)가 실제 상관을 확인해야 한다.

HAL 원등록 회수는 해당 HW·대기·처리 중 SW·late 대응 참조가 **모두 끝난 별도 근거** 뒤에만 가능하다. PCM 반환, readiness INVALIDATED, IRQ disable, PB retirement는 회수 증명이 아니다. old 위험 종료·현재 쓰기 안전·alias 없는 독립 보호가 확인되면 옛 기록을 남긴 채 새 A/B 회차를 쓸 수 있다. 공간 부족은 신규 진행 제한이며 기존 기록 eviction·무한 pool로 해결하지 않는다. [소유·수명](../../design/50_CONTRACTS/10_OWNERSHIP_LIFETIME.md#references-and-release)을 적용한다.

<a id="raw-time-loss"></a>
## raw·원 시각·부족과 손실

HAL은 안전하게 읽을 수 있는 고정 입력의 원등록·실제 raw를 짧게 포착/보호하고 최소 HW acknowledge와 Runtime 후속 기회만 알린다. decode/refill·대량 PCM 복사·동적 할당·blocking/wait·긴 순회·정책/복구 실행·상위 Core 호출은 하지 않는다. 실제 ack 순서·signature·저장/직렬화·ISR 예산은 현재 source/보드 Gate다.

`occurredAt`은 사건 시각/가능 구간, `capturedAt`은 포착 시간 근거, `now`는 후속 처리 시간이다. domain/epoch/reset/wrap/연속성·변환 부족을 now/0/새 epoch로 보충하지 않는다. 정확한 원시각을 표현하지 못하면 완전한 typed raw를 합성하지 않고 실제 부분 관측과 부족을 보호한다. [R4 시간](../../design/50_CONTRACTS/30_TIMING_FRESHNESS.md#time-context)·[CORE-16 포착](AUDIOHAL_BOUNDARY/functions/AudioHAL_Callback.md#capture)을 따른다.

원등록 부재·입력 접근 불가·개별 상관/시간 비교 근거 부족은 **INSUFFICIENT**이며 부재만으로 손실을 단정하지 않는다. 실제 누락/drop/overflow/덮어쓰기·시간 연속성 손실이면 그 interval/binding 범위의 **LOST**다. 실제 확인된 부분 사실과 다른 충분 범위는 보존하며 미확인 key/time/target을 합성하지 않는다.

bounded raw 보존이 실패하면 loss를 관측 가능하게 남겨 해당 범위의 확정 소비를 차단해야 한다. loss 전달 수단도 가득 차거나 미입증이면 그 한계와 충분성 차단을 명시하며 항상 성공하는 비상 슬롯/새 LossRecord·Queue를 가정하지 않는다. Runtime 알림 병합은 처리 기회이며 사실 건수·반영 완료가 아니다. [CORE-16 loss](AUDIOHAL_BOUNDARY/functions/AudioHAL_Callback.md#loss)·[Runtime](../../design/10_LAYERS/details/50_TIMEBASE_RUNTIME_BOUNDARY.md)을 확인한다.

<a id="pcm-cycle-write"></a>
## 소비·옛 회차 반환·현재 쓰기를 따로 확인한다

전송 PCM은 **정확히 A/B 두 개**다. 참고 Transport의 startup FIFO prime 영역을 정상 사운드용 제3 버퍼로 승격하지 않는다. 실제 decoded zero sample은 유효 frame이며 short tail/빈 B·물리 zero-fill로 validFrameCount/사운드 길이·반복을 늘리지 않는다. [R4 유효/무음](../../design/50_CONTRACTS/50_AUDIO_BUFFER_TRANSPORT.md#valid-and-silence)을 따른다.

source consumed ≠ 옛 cycle safe return ≠ 현재 CPU writable ≠ 실제 출력 끝 ≠ Session retirement다. STREAM은 효력 전에 HANDOFF_PENDING과 안정 samples·정확한 Attempt/A 또는 B/cycle·유효 범위를 보호하고, TX는 장치 접근 보호를 쓴다. 옛 A 반환은 새 A 회차나 B usage를 풀지 않는다.

`noFutureAccess=true`의 충분 반환은 옛 작업/내용의 모든 참조 종료·재소비 금지 근거다. 같은 주소를 차기 DMA가 영원히 방문하지 않는다는 뜻이 아니다. **적용 시점 및 매 write/refill/zero-fill 직전** 현재 다음 방문 전 bounded 쓰기를 끝낼 수 있는 구간·IRQ 지연/경합·WCET·cache/MPU/barrier/alignment/alias·남은 reader를 별도 확인한다. 단일 IRQ, phase flag 재확인, consumed 또는 옛 CPU_WRITABLE만으로 현재 안전을 확정하지 않는다.

반환 근거가 충분해도 현재 안전 미확인/구간 경과이면 usage/retain·정리 후속을 유지하고 쓰지 않는다. A/B 모두 안전 미확인이어도 세 번째 버퍼를 만들지 않는다. 반환은 productionAllowed를 복원하지 않으며 [STREAM 반환 적용](../02_SERVICE/AUDIO_STREAM/functions/AudioStream_Advance.md#pcm-return)·[R4 safe-write](../../design/50_CONTRACTS/50_AUDIO_BUFFER_TRANSPORT.md#safe-write)·[R5 회수](../../design/60_PSEUDOCODE/50_STOP_LATE_UNCERTAIN_FLOW.md#stream-safe-return)를 따른다.

<a id="stop-scope"></a>
## STOP 선차단과 원 targets·잔류 범위

STREAM은 하위 STOP **전에 원 Session 전체의** 미래 decode/refill/새 sound handoff를 차단한다. bounded 정리에서 아직 방문하지 않은 Attempt/provider도 포함한다. 진행 중 producer는 실제 인계 직전에 다시 확인하며 하위 실패/부분 효력·반환 뒤에도 productionAllowed=false와 기존 참조를 유지한다.

TX는 실제 원 targetOperations/Attempt·SEGMENT/ATTEMPT와 active/pending descriptor·FIFO/frame 범위를 대조한다. 같은 SEGMENT enum은 구간 identity가 아니다. subset STOP/미방문 대상·제어 operation ACK를 모든 옛 작업/Session 종료로 확대하지 않는다. [TX Request STOP](../03_DRIVER/AUDIO_TX/functions/AudioTx_Request.md#stop)·[R6 scope](../../design/70_C_INTERFACE/00_HEADER_OWNERSHIP_MAP.md#stop-scope)·[R5 선차단](../../design/60_PSEUDOCODE/50_STOP_LATE_UNCERTAIN_FLOW.md#stream-stop)을 적용한다.

참고 RTD에 DMA 요청 disable/clear, FIFO 대기 또는 timeout reset, TX disable 분기가 있어도 현재 이미지·구성/실제 대상·잔류/late 상관 없이 no-future-DMA/no-future-sound를 확정하지 않는다. FIFO 배출/디지털 경계와 전원·Codec/DAC/스피커 아날로그 무음의 상관도 별도 입증 대상이다. mute·zero-fill·현재 idle·IRQ disable·abort ACK만으로 safe return/과거 NO_START/TERMINATION을 만들지 않는다.

<a id="output-sufficiency"></a>
## actual·NO_START·TERMINATION과 전체 retirement

| 판단 경계 | 함께 필요한 충분 근거 | 부족할 때 |
|---|---|---|
| actual 후보 | 원 요청 및 실제 활성화 **이후 새** 물리 출력, 원 operation/Attempt/scope·사실 시각·binding/coverage | 수용·READY·Send 호출/현재 flag를 actual로 합성하지 않음 |
| NO_START | 필요한 **과거 실제 무출력**의 충분 관측 + 모든 관련 옛 미래 출력 불가 + 해당 scope 미래 PCM 생산/제출 차단 | 미래 차단만으로 과거 무출력을 추론하지 않음 |
| TERMINATION | 해당 scope 실제/잔류 출력 끝 + 모든 관련 옛 미래 출력 불가 + 해당 scope 미래 PCM 차단 | termination으로 과거 NO_START를 소급 확정하지 않음 |
| PB 전체 retirement | 모든 관련 Attempt/구간을 덮는 Backend 충분조건 + 상위 start/cue/repeat 차단 + late 귀속 보호 + 새 자원 실제 안전 | 한 cue/EOS/STOP/PCM 반환·cleanup 한 항목으로 whole 완료/모든 기록 폐기 없음 |

[R4 출력 충분조건](../../design/50_CONTRACTS/20_ASYNC_RESULT_EVIDENCE.md#confirmed-outcomes)·[STREAM 결합](../02_SERVICE/AUDIO_STREAM/functions/AudioStream_Advance.md#outputs)·[R5 PB whole](../../design/60_PSEUDOCODE/50_STOP_LATE_UNCERTAIN_FLOW.md#playback-stop-late)을 따른다. physical fact가 충분해도 권한/시간 비교만 부족하면 그 실제 fact는 보존하되 적법 Started를 합성하지 않는다. 최초 One-shot은 **원발생→원 actual 시각/가능 구간 전체 `<2초 [잠정]`**를 PB가 대조한다. capturedAt/now·Network 500ms 제안을 대입/통합하지 않고 정상 Started 후속 cue에 최초 gate를 재적용하지 않는다. [R6 세 갈래 시간 판정](../../design/70_C_INTERFACE/00_HEADER_OWNERSHIP_MAP.md#first-actual-time)을 확인한다.

최종 전 같은 원 Attempt의 적법한 late actual은 원 사실로 적용해도 STOPPING/ABORTING을 유지한다. uncertain 최종 뒤/retired callback은 옛 정리·진단·참조 종료만 수행하며 정상 Pending/Started/Completed·새 Session/PCM/READY/Fault를 부활시키지 않는다. [R4 late](../../design/50_CONTRACTS/20_ASYNC_RESULT_EVIDENCE.md#late-results)를 따른다.

<a id="device-readiness"></a>
## 장치/config별 준비와 부분 결과

SGTL5000 Codec·CS2100 Generator·Clock Reference는 각각 현재 configuration에 대해 CONTROL이 쓴다. 미평가 UNASSESSED, PREPARING, 확인된 READY, 영향받은 INVALIDATED, 확인된 FAILED를 구분한다. 한 장치 READY·옛 config 성공을 다른 둘/새 config의 전체 READY로 만들지 않는다. 같은 구성의 현재 유효 READY는 재사용하며 매 Attempt 전체 reset하지 않는다.

reset/reconfig/오류는 실제 영향 장치/config의 근거만 무효화한다. 부분 효력/불명확은 원 제어·참조/진단·후속을 유지하며 일반 거부 rollback 또는 무조건 READY/FAILED로 바꾸지 않는다. 실제 Codec/Clock 순서·ratio/register·timeout과 현재 Reference 시간 연속성은 표의 F4/F5 근거로 확인해야 한다. [CORE-15 장치](../03_DRIVER/AUDIO_CONTROL/functions/AudioControl_Service.md#devices)·[구성](../03_DRIVER/AUDIO_CONTROL/functions/AudioControl_Service.md#configuration)·[R3 준비](../../design/40_DATA/60_AUDIO_TX_DATA.md#vssdevicereadinessstate)를 따른다.

<a id="recovery"></a>
## Recovery 허용·수행·검증과 해제

현재 HEALTH permission/faultInstance/faultRevision/target/configuration/action **6필드**와 old 전체 retirement·provider/read/PCM/device의 실제 위험 종료·alias 없는 late 기록 보호를 효력 직전에 다시 확인한다. Session 없는 복구에 가짜 Attempt를 만들지 않는다. 실제 action 매핑/효과 기준 미정이면 해당 수행만 보류한다.

performed는 원 실제 수행 operation/effect/occurredAt, verified는 **수행 이후 별도 효과 관측**의 performedOperation/effect/verifiedAt이다. 수용·ACK·초기 config flag로 둘을 동시에 성공 처리하지 않는다. 둘의 전달 시점·OUT count·수명은 독립이며 한쪽만 있어도 그 fact를 보존한다. HEALTH만 현재 허용·instance/revision/target/config와 pair를 대조해 해당 제한을 해제한다. old 성공으로 새 Fault/READY를 정상화하거나 age/Hold/replay를 리셋하지 않는다. [CORE-15 Recovery](../03_DRIVER/AUDIO_CONTROL/functions/AudioControl_Service.md#recovery)·[R5 별도 효과](../../design/60_PSEUDOCODE/60_FAULT_RECOVERY_FLOW.md#device-recovery)·[R4 현재 대상](../../design/50_CONTRACTS/40_FAULT_RECOVERY.md#current-target)을 따른다.

<a id="health-connection"></a>
## Startup·Recovery 관측을 HEALTH에 연결하기

Startup의 미평가/확인 실패는 [장치별 준비](#device-readiness), 원 raw/손실은 [포착과 시간](#raw-time-loss), 실제 복구는 [수행·별도 검증](#recovery)에서 각각 읽습니다. CONTROL/TX가 반영한 원 사실 → STREAM의 독립 typed 인계 → FLOW 조율 → [HEALTH 현재 제한·파생 보고](../02_SERVICE/HEALTH/00_MODULE_OVERVIEW.md#startup-fault-recovery-status) 순서이며 ISR에서 이 Core들을 실행하지 않습니다. 원 producer·대상/구성·operation·발견 단계·원 시간은 이 연결에서도 유지합니다.

미평가 장치·옛 config 성공·단독 performed·STOP/abort ACK/zero-fill/한 DMA half 관측은 현재 초기화 정상·복구 효과·제한 해제의 충분조건이 아닙니다. [INFRA의 비교·연속성 근거](../05_INFRA/00_LAYER_OVERVIEW.md#startup-recovery-time)가 부족하면 해당 시간 판정을 보류하고 원 사실을 보호합니다. BSP의 source/generated·RTD·Pin/clock·linker/cache/image 정적 근거를 실제 출력 관측으로 바꾸지 않으며 현재 결합은 SOURCE_NOT_VERIFIED / BINDING_TBD / HW_UNVERIFIED입니다.

<a id="out-lifetime"></a>
## 독립 OUT와 consumer 수명

TX Request의 3 OUT, TX Advance의 5 OUT, CONTROL의 5 OUT, STREAM Advance의 7 OUT는 각각 독립 타입·capacity/written이다. 각 저장소/metadata·범위·비간섭을 안전 검증한 군만 초기화하고, 실제 기록과 참조 보호 뒤 자기 written만 증가한다. invalid/full/0인 군은 원 사실을 producer에 보호하며 다른 유효 군을 일괄 중단하지 않는다. void/복사/written은 반영 ACK·완료/참조 종료가 아니다. 실제 보존 손실은 드러내고 새 효과를 제한한다. [R6 전달](../../design/70_C_INTERFACE/00_HEADER_OWNERSHIP_MAP.md#prototype-notation)·[CORE-15 인계](../03_DRIVER/AUDIO_CONTROL/functions/AudioControl_Service.md#publish)를 따른다.

<a id="feasibility-gates"></a>
## 독립 논리 후보와 실제 결합 Gate

| 수준 | 유지하는 후보 / 필요한 근거 | 지금의 의미 |
|---|---|---|
| F1_LOCKED_SEMANTIC | owner·원 key/회차·수명·충분조건·STOP 선차단·performed/verified 분리 | 승인 계약을 유지하는 문서 설명 |
| F2_PROVISIONAL_TESTABLE | 출처 있는 시간/범위 fixture·기존 최초 actual 잠정값 | 실제 HW 수치/제품 정책 확정 아님 |
| F3_PARTIAL_INDEPENDENT | 두 OPTIONAL/NULL·중복/old/late·cycle·범위/OUT 부족·loss·부분 장치/복구 pair의 향후 Host 후보 | 후보만 기술; 시험 NOT_RUN |
| F4_SOURCE_IMAGE_GATE | 현재 source/generated/RTD revision→실행 image·symbol/section·Header/include·ABI·등록/저장/직렬화·외부 IF | 필요한 제품 결합만 BINDING_TBD |
| F5_PHYSICAL_SAFETY_GATE | 현재 DMA/descriptor·다음 방문/IRQ/WCET·cache/MPU·원시각/coverage·STOP/actual·Codec/Clock/전원/아날로그 효과 | 입증되지 않은 해당 물리 효과만 HW_UNVERIFIED |

P1B_C1 REQUIRED_NOT_RUN(9 Core/16 Q), Q01~14/R6-01~08·외부 IF/source-image·TBD-FINAL·PCM/STOP/출력/Recovery 미결과 P2 HOLD는 그대로다. OPEN-009/010과 과거 FAIL도 로컬 원장에 보존한다. 이 문서의 정적 검토는 제품 안전 승인·실제 Host/Firmware/Build/Flash/보드 시험·GD5 전체 검수 또는 D6 착수가 아니다.
