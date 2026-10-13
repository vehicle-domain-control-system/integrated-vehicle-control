# AudioHAL_Callback — 원등록의 raw·시각·손실 포착

[VSS 지도](../../../01_VSS_MAP.md) · [HAL/BSP Layer·내부 Boundary](../../00_LAYER_OVERVIEW.md) · [한국어 의사코드](../pseudocode/AudioHAL_Callback.md)

> CORE-16 · D5-2 SELF_CHECKED / INDEPENDENT_REVIEW_PENDING · GD5 전체 NOT_RUN. LOGICAL_R6 / C ABI BINDING_TBD / HW_UNVERIFIED / TEST_NOT_RUN.

vendor callback/IRQ에서 실제 원등록과 raw 사실·원 시각/coverage·손실을 짧게 보호하고 기존 후속 처리 기회를 알립니다. AUDIOHAL_BOUNDARY는 HAL/BSP **내부 Boundary이며 별도 Module이 아닙니다**. Boundary 개요/public wrapper를 추가하지 않습니다.

<a id="contract"></a>
## 1. R6 원형과 두 OPTIONAL

```text
void AudioHAL_Callback(
    OPTIONAL IN const VssHalTxRegistration *originalTx,
    OPTIONAL IN const VssHalDeviceRegistration *originalDevice,
    IN const VssRawObservation *observation);
```

[R6 정본](../../../../design/70_C_INTERFACE/00_HEADER_OWNERSHIP_MAP.md#r6-audiohal-callback)의 두 OPTIONAL은 ONE_OF가 아닙니다. 없음·한쪽·양쪽이 각각 가능하며 둘 다 있다는 이유로 거부하거나 current key로 합치지 않습니다. 원형은 중립 논리 계약이며 generated/vendor ISR callback ABI·adapter symbol·인터럽트 우선순위를 확정하지 않습니다. R2의 provisional VssHalRegistration을 새 union/통합 등록 타입으로 복원하지 않습니다.

| 입력 / 기존 R3 자료 | 보존·검증할 원본 / 수명 |
|---|---|
| originalTx / VssHalTxRegistration | registration/operation/attempt/pcm/binding. 해당 원 HW 작업 전에 HAL이 불변 보호한 기록. pcm은 PCM 작업의 원 handoff 참조만; 주소 없는 제어에는 없음 |
| originalDevice / VssHalDeviceRegistration | registration/operation/device/configuration/binding. 해당 원 장치 효력 전에 보호한 기록 |
| observation / VssRawObservation | registration/rawFact/occurredAt/capturedAt/coverage **5필드**. registration은 실제 원 VssRegistrationKeyType 참조 또는 None이며 전체 등록 struct/현재 operation 포인터가 아님 |
| coverage / VssObservationCoverage | boundary/interval/binding/validity. 원 관측 범위와 SUFFICIENT/INSUFFICIENT/LOST를 구분; 상위 물리 결과의 충분성이 아님 |

필드/내부 참조는 접근 가능성을 확인한 뒤 읽습니다. 원등록의 실제 key/binding·보호 수명과 raw 범위를 각각 대조합니다. raw pointer만 얕게 저장하거나 callback 호출 수명을 보호 완료로 보지 않습니다.

<a id="ownership"></a>
## 2. Entry·writer·후속 consumer

Entry는 기존 vendor callback/IRQ입니다. DRIVER의 [AudioTx_Request](../../../03_DRIVER/AUDIO_TX/functions/AudioTx_Request.md#registration)와 [AudioControl_Service](../../../03_DRIVER/AUDIO_CONTROL/functions/AudioControl_Service.md#raw-and-partial)가 의미 원본을 제공하고 HAL이 효력 전에 해당 등록을 보호합니다. IRQ 도착 후 current 조회로 원 operation/Attempt/config를 생성하지 않습니다.

HAL Boundary는 불변 등록·raw 포착/보호를 담당합니다. 후속 [AudioTx_Advance](../../../03_DRIVER/AUDIO_TX/functions/AudioTx_Advance.md#ownership)가 TX operation·소비/출력/반환을, [AudioControl_Service](../../../03_DRIVER/AUDIO_CONTROL/functions/AudioControl_Service.md#raw-and-partial)가 device/config 제어·readiness를 해석합니다. [AudioStream_Advance](../../../02_SERVICE/AUDIO_STREAM/functions/AudioStream_Advance.md#lower-results)는 두 Driver의 원 사실을 연결합니다. ISR에서 이 함수들·FLOW/PLAYBACK/HEALTH를 역호출하거나 그 상태를 쓰지 않습니다. Callee는 기존 최소 HW acknowledge와 Runtime 후속 알림 도구뿐입니다.

<a id="capture"></a>
## 3. 포착 순서와 원 시각

1. 기존 IRQ 경계가 보장한 짧은 입력/실제 raw를 포착합니다. observation 외피와 필요한 내부 key/coverage·시각 자료의 접근 가능성을 먼저 확인합니다.
2. 전달된 originalTx/originalDevice를 각각 검증합니다. 실제 사전 원등록인지, 이번 raw와 실제 key/binding·수명/범위 상관이 있는지 독립 대조합니다.
3. 원 rawFact·원 occurredAt·포착 capturedAt·coverage와 검증 가능한 원등록 참조를 불변 보호합니다. 후속 처리 now를 실제 발생 시각으로 쓰지 않습니다.
4. 정해진 기존 bounded 보호 범위에 실제 사실을 남기거나, 실제 저장/관측 손실의 범위와 최소 loss 근거를 관측 가능하게 보존합니다.
5. 기존 HW가 요구하는 최소 acknowledge와 Runtime 후속 알림만 실행하고 return합니다. acknowledge 순서·레지스터/notification ABI는 실제 Binding 확인 대상입니다.

occurredAt의 VssFactTime은 실제 사실 시각/가능 구간, capturedAt의 VssTimeEvidence는 포착 시간·domain/epoch/continuity입니다. 정밀도/비교 부족이면 그 부족을 유지하며 원 시각을 now·새 epoch·임의 0으로 채우지 않습니다. 물리 발생 시각이 표현 불가능한 자료는 완전한 typed raw를 합성하지 않고 실제 확보한 부분 관측과 부족/loss 상태로 보호합니다.

<a id="registration"></a>
## 4. 없음·한쪽·양쪽과 지속 등록

| 제공된 문맥 | 허용하는 연결 / 금지 |
|---|---|
| Tx만 | 실제 대조된 Tx 등록의 raw로 보호. device/config 또는 최신 Session을 합성하지 않음 |
| Device만 | 실제 대조된 device 등록의 raw로 보호. 가짜 TX operation/Attempt/PCM 결과 없음 |
| 둘 다 없음 | 실제 확보한 raw와 등록 없음/귀속 INSUFFICIENT를 보호. 실제 손실 근거가 있으면 그 범위 LOST; current key 채우기 없음 |
| 둘 다 있음 | 각 등록의 실제 상관을 독립 확인해 일치하는 관계만 보호. 둘을 하나의 current identity로 합치거나 한 raw를 근거 없이 양쪽 사실로 복제하지 않음 |

양쪽이 존재해도 observation.registration은 원 scalar key 또는 None입니다. 필요한 원등록 각각의 불변 기록/참조는 기존 HAL 보호 경계에 독립 보존합니다. 실제 상관 없는 기록을 맞는 것처럼 붙이지 않습니다. 한쪽 불일치 때문에 다른 쪽의 실제 확인된 포착 사실을 버리지는 않습니다.

지속 vendor SAI/eDMA 등록 하나는 여러 SW operation/Attempt/PCM cycle을 가로지를 수 있습니다. 실제 vendor key 확인만으로 개별 SW 원귀속이 확정되지 않습니다. 개별 상관 불충분은 INSUFFICIENT로 남기고 후속 Driver의 confirmed 생성은 차단합니다. 매 START마다 vendor stop/start, vendor↔SW 등록 1:1, current A 주소/IRQ phase→cycle 대응을 강제하지 않습니다. 실제 dispatch/상관 방법은 R6-04/07 BINDING_TBD입니다.

<a id="invalid-input"></a>
## 5. 접근 불가와 실제 손실을 구별한다

observation=NULL/접근 불가에서는 역참조하지 않습니다. 실제 포착한 IRQ/누락 범위가 있으면 기존 HAL/Runtime 보호 경계에 그 최소 관측·부족/손실 상태를 남깁니다. rawFact/key/시각을 새로 만들거나 알 수 없는 target/config를 채워 완전한 진단 struct를 합성하지 않습니다. 확인된 부분도 없으면 입력 접근 불가/근거 부재를 명시하며 정상 관측이 있었다고 보고하지 않습니다.

원등록 없음·상관/시각 비교 근거 부족은 INSUFFICIENT입니다. 실제 raw 누락·보존 실패/overflow·시간 연속성 손실은 해당 범위 LOST입니다. pointer 없음 자체를 관측 손실로 단정하지 않고, 실제 손실을 단순 부족으로 숨기지도 않습니다. 원자료의 실제 확인된 범위는 유지합니다.

<a id="loss"></a>
## 6. 저장 한계·알림과 충분성 차단

bounded raw 공간 부족이면 기존 사실/원등록을 지우거나 조용히 drop하지 않습니다. 실제 누락 범위의 LOST·확인 가능한 interval/binding·최소 손실 정보를 기존 보호 경계로 드러내 후속 Driver가 충분성 판단을 중단하도록 합니다. 새 LossRecord 타입·Queue/Mutex·무제한 pool·항상 저장 가능한 비상 슬롯을 선결정하지 않습니다.

loss 정보의 별도 보존/알림 수단 자체가 가득 차거나 실물 미확인이면 그 한계도 공개하고 해당 범위의 충분 관측/무손실을 주장하지 않습니다. 현재 충분성 소비를 차단할 수단·정적 용량·원자성·ISR 예산은 실제 source/Runtime 결합에서 입증할 Gate입니다. 부족을 가짜 정상 raw로 대체하지 않습니다.

Runtime 알림의 중복/병합은 처리 기회 표시입니다. 알림 한 번을 raw 사실 한 건·모든 전달 완료로 세지 않으며, 원 facts/loss·미완료 의무는 독립 보호합니다. HW acknowledge나 void 반환은 소비자 반영 ACK가 아닙니다.

<a id="isr"></a>
## 7. ISR의 짧은 처리 범위와 의미 판단 금지

입력 외피/고정 범위의 원등록 대조·짧은 raw/손실 포착·최소 HW ack/Runtime 알림에 한정합니다. decode/refill·PCM 대량 복사·동적 할당·blocking/wait·긴 loop·전체 원기록 검색·정책/복구 수행을 하지 않습니다. 구체 횟수/처리시간/IRQ 우선순위나 queue size를 새로 정하지 않습니다.

HALF/MAJOR/complete·abort/ack·IRQ disable·현재 idle/mute는 source consumed, safe return/noFutureAccess, 현재 CPU writable, actual/과거 NO_START/TERMINATION의 충분조건이 아닙니다. HAL은 device READY/FAILED·TX result·PCM usage·PB Started/retired·HEALTH Fault를 만들지 않습니다. 실제 부분 실패/loss도 raw 원 사실로 남겨 Driver가 해석합니다.

<a id="lifetime"></a>
## 8. 즉시·late와 독립 등록 수명

즉시 callback은 하위 요청 반환 전에 와도 사전 보호된 원등록으로 처리합니다. 사전 보호가 없으면 현재 key로 수습하지 않고 실제 귀속 부족/손실을 남깁니다. STOP·uncertain final/retired·old config 이후 callback도 원 old 등록/raw로만 보호합니다. 후속 old 정리/진단에 연결하며 새 Session/PCM 회차/READY/Fault를 복원하지 않습니다.

등록은 HW·대기·처리 중 SW·late 대응 참조가 **모두** 끝났다는 실제 근거 뒤에 HAL이 회수합니다. raw는 Driver에 필요한 원 facts가 안전하게 정규화·보호되거나 실제 consumer 참조가 끝났을 때만 처리 완료로 표시합니다. OUT/알림 전달이나 얕은 복사는 종료가 아닙니다. PCM return·readiness INVALIDATED·IRQ disable·PB retirement·output owner 해제만으로 등록을 폐기하지 않습니다.

옛 불변 등록이 독립 보호된 채 새 자원/cycle을 쓰는 것은 실제 old 위험 종료·현재 쓰기 안전·alias 없는 보호가 모두 확인됐을 때 가능하며, 모든 등록 폐기를 강요하지 않습니다. 지속 vendor 등록과 개별 SW 등록 및 raw 각각의 수명은 다릅니다.

<a id="feasibility"></a>
## 9. 구현 후보와 실물 Gate

| 기준 | 고정 논리 / 후속 후보 | 현재 미입증 |
|---|---|---|
| F1_LOCKED_SEMANTIC | 두 OPTIONAL 독립·원등록/raw writer·현재 key 합성 금지·loss/수명 | 기존 승인 의미 유지 |
| F2_PROVISIONAL_TESTABLE | 출처 있는 원 raw/시각/등록 fixture | 실제 domain/epoch·잠정값 제품 채택 아님 |
| F3_PARTIAL_INDEPENDENT | 없음/한쪽/양쪽·즉시/late·불일치·overflow/병합 알림·접근 불가 후보 | Host/Firmware/Board TEST_NOT_RUN |
| F4_SOURCE_IMAGE_GATE | 기존 BSP/transport 내부 callback 재사용 후보 | 실제 RTD/generated signature·dispatch·key/보호 저장/원자성·source/image |
| F5_PHYSICAL_SAFETY_GATE | 부족/LOST에서 물리 확정 소비 차단 | persistent 대응·원 시각/coverage·ISR WCET/IRQ·DMA/cache/STOP/실제 출력·손실 신호 수단 |
| F6_DEPENDENT_FOLLOWUP | TX/CONTROL 후속 소비·STREAM/HEALTH 연결 | D5-3/GD5/D6·실제 결합은 별도 지시 |

Q01~14/R6-01~08·P1B_C1 REQUIRED_NOT_RUN(9 Core/16 Q)·P2 HOLD·TBD-FINAL·WINDOW PRODUCT 보류/TEST 격리를 유지합니다. 실제 ABI/HW PASS를 주장하지 않습니다.

<a id="references"></a>
## 10. 고정 정본과 왕복 경로

출처는 VSS@ccab866eaf6b98cfd2f80e92ed4cc198c021f4c4 / design tree 28628b353618047512cc645459f3c6fe66c1200c의 R2~R6입니다. Master v1.1 CORE-16은 HAL/BSP 내부 Boundary입니다. 원 파일별 Git Blob/raw/LF SHA는 결과 ZIP 출처핀에 고정합니다.

[R2 callback](../../../../design/30_FUNCTIONS/50_DRIVER_HAL_FUNCTIONS.md#audiohal-callback) · [R3 Tx 등록](../../../../design/40_DATA/60_AUDIO_TX_DATA.md#vsshaltxregistration) · [Device 등록](../../../../design/40_DATA/60_AUDIO_TX_DATA.md#vsshaldeviceregistration) · [Raw](../../../../design/40_DATA/60_AUDIO_TX_DATA.md#vssrawobservation) · [Coverage](../../../../design/40_DATA/60_AUDIO_TX_DATA.md#vssobservationcoverage) · [R4 수명](../../../../design/50_CONTRACTS/10_OWNERSHIP_LIFETIME.md#references-and-release) · [R4 원 근거](../../../../design/50_CONTRACTS/20_ASYNC_RESULT_EVIDENCE.md#evidence-basis) · [R4 지속 스트림](../../../../design/50_CONTRACTS/50_AUDIO_BUFFER_TRANSPORT.md#continuous-stream) · [R5 HAL](../../../../design/60_PSEUDOCODE/40_AUDIO_PREPARE_TX_FLOW.md#audiohal-callback) · [R5 STOP/late](../../../../design/60_PSEUDOCODE/50_STOP_LATE_UNCERTAIN_FLOW.md#tx-safe-return) · [R6 수명](../../../../design/70_C_INTERFACE/00_HEADER_OWNERSHIP_MAP.md#lifetime) · [R6 미정](../../../../design/70_C_INTERFACE/00_HEADER_OWNERSHIP_MAP.md#tbd-register) · [Runtime 경계](../../../../design/10_LAYERS/details/50_TIMEBASE_RUNTIME_BOUNDARY.md) · [의사코드](../pseudocode/AudioHAL_Callback.md#procedure)
