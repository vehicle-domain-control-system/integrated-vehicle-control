# AudioHAL_Callback — 한국어 C형 raw 포착 의사코드

[VSS 지도](../../../01_VSS_MAP.md) · [HAL/BSP 내부 Boundary](../../00_LAYER_OVERVIEW.md) · [상세 설명](../functions/AudioHAL_Callback.md)

> CORE-16. LOGICAL_R6 / C ABI BINDING_TBD / HW_UNVERIFIED / TEST_NOT_RUN.

<a id="contract"></a>
## R6 논리 원형

```text
void AudioHAL_Callback(
    OPTIONAL IN const VssHalTxRegistration *originalTx,
    OPTIONAL IN const VssHalDeviceRegistration *originalDevice,
    IN const VssRawObservation *observation);
```

[R6 정본](../../../../design/70_C_INTERFACE/00_HEADER_OWNERSHIP_MAP.md#r6-audiohal-callback)의 두 포인터는 각각 OPTIONAL입니다. ONE_OF·union이 아니고 OUT/scalar 성공 반환이 없습니다. 아래 한글 조건/기록·goto는 **(의사연산)**이며 실제 C symbol·ABI·저장 필드·helper/API가 아닙니다. HAL/BSP 내부 Boundary만 사용합니다.

<a id="procedure"></a>
## H1. 최소 포착과 접근 불가 분기

```c
void AudioHAL_Callback(위의_논리_인자) { // (의사연산)
    // IRQ 경계가 이미 보장한 실제 고정 입력/발생 범위만 짧게 포착.
    // 전체 context lookup/긴 loop·동적 할당·blocking·상위 Core 호출 없음.
    if (observation == NULL || observation_외피_안전읽기_불가) {
        observation_역참조_없음;
        if (실제_포착된_누락범위_또는_부분IRQ관측이_있음) {
            확인된_최소부분관측_부족또는실제손실을_기존HAL_Runtime경계에_보호;
        } else {
            관측근거_없음_입력접근불가를_명시; 정상raw_합성없음;
        }
        원operation_key_시각_rawFact_현재target_configuration_채우기없음;
        goto acknowledge_notify;
    }
    observation의_registration참조_및_시각_coverage_실제범위_안전읽기_검증;
    if (필수내부자료를_안전하게_읽지못함) {
        확인가능한_부분raw와_부족_실제손실범위만_보호;
        가짜_완전VssRawObservation_또는_진단struct_합성없음;
        goto acknowledge_notify;
    }
```

<a id="registration"></a>
## H2. 두 원등록의 독립 대조

```c
    // Tx와 Device는 별도 조건. 양쪽 존재를 invalid로 취급하지 않는다.
    if (originalTx != NULL) {
        if (그_외피_내부pcm참조_및_불변보호수명_안전검증 &&
            실제_원registration_binding_이번raw범위와_상관확인) {
            실제_Tx원등록의_필요한_불변참조를_기존HAL경계에_보호;
        } else {
            이_Tx연결_귀속부족_또는_실제손실_보호; currentAttempt_합성없음;
        }
    }
    if (originalDevice != NULL) {
        if (그_외피_불변보호수명_안전검증 &&
            실제_원registration_binding_이번raw범위와_상관확인) {
            실제_Device원등록의_필요한_불변참조를_기존HAL경계에_보호;
        } else {
            이_Device연결_귀속부족_또는_실제손실_보호; currentConfig_합성없음;
        }
    }
    if (originalTx == NULL && originalDevice == NULL) {
        실제_raw_및_등록없음_귀속INSUFFICIENT_보호;
        // 원 key가 확인된 실제 raw는 보존하되 current key를 채우지 않음.
    }
    if (originalTx != NULL && originalDevice != NULL) {
        실제_대조된_각관계만_독립보호; 하나의_current등록으로_조합없음;
        한raw를_근거없이_Tx_Device_두사실로_복제없음;
    }
    // observation.registration은 실제 scalar key 참조 또는 None.
    // 전체 두 등록 struct를 새 union/공통context에 담지 않는다.
    if (지속vendor등록_key만_확인되고_개별SWoperation_cycle_대응_미확인) {
        원raw_실제key_보존; 개별원귀속_INSUFFICIENT_표시;
        latestSession_Attempt_A주소_IRQphase로_개별키_보충없음;
    }
```

<a id="time-and-loss"></a>
## H3. raw·시각·coverage와 손실

```c
    실제_rawFact_원occurredAt_포착capturedAt_coverage를_불변보호대상으로_구분;
    // 입력 const observation은 수정하지 않는다. 아래 validity는 보호할 해당
    // 포착/상관 범위의 의사 표기이며 원 관측과 부족/손실 근거를 함께 보존한다.
    // occurredAt은 실제 원 VssFactTime, capturedAt은 VssTimeEvidence.
    // 발생≠포착≠Driver 처리 now. 알려지지 않은 시각에 now/0/새epoch 없음.
    if (실제_누락_저장overflow_또는_시간연속성_손실) {
        해당_관측범위의_validity = VSS_COVERAGE_LOST; // (의사연산)
        실제_확인가능한_원interval_binding_손실근거_보호;
    } else if (관측_원귀속_시각비교_또는_개별대응_근거부족) {
        해당_부족범위의_validity = VSS_COVERAGE_INSUFFICIENT;
        실제_충분한_다른범위_원사실을_부족으로_삭제하지않음;
    } else {
        실제_포착한_원coverage만_보존;
        // 이 raw coverage를 actual/no-start/end/safe-return 충분성으로 승격없음.
    }
    if (원시각_등_R3필수문맥을_실제근거로_표현할수없음) {
        완전한_typedRaw_합성없음;
        실제부분관측_필수근거부족_또는_실제손실만_기존경계에_보호;
        goto acknowledge_notify;
    }
```

<a id="storage"></a>
## H4. bounded 보호와 loss 정보의 한계

```c
    if (기존_bounded보호범위에_raw_및_필요원등록_참조수명_보호가능) {
        실제_원raw_각필드와_필요참조를_짧게_불변보호;
        // 임시 observation/pcm 포인터의 얕은 저장을 완료로 보지 않는다.
    } else {
        실제_보존실패범위_LOST_최소손실정보를_기존HAL_Runtime경계로_드러냄;
        기존_raw_원등록_eviction_또는_조용한drop_무한pool_없음;
        if (최소loss정보의_보호알림수단도_가득차거나_실물미확인) {
            해당_수단한계도_명시; 충분관측_무손실_주장없음;
            후속확정소비를_차단해야하는_실물Gate_유지;
            // 항상 성공하는 비상slot/LossRecord/새queue를 선언하지 않음.
        }
    }

acknowledge_notify:
    기존_HW가_요구하는_최소acknowledge만_수행;
    기존_Runtime경계의_짧은_후속처리기회_알림;
    // 정확한 ack 순서/레지스터·loss visibility·알림 ABI·원자성은 Binding TBD.
    // 중복/병합 알림은 raw 건수/모든 사실 처리 완료가 아니다.
    return; // void/IRQ ACK는 원 결과 소비/물리 완료가 아니다.
}
```

<a id="isr-and-lifetime"></a>
## H5. ISR 금지·즉시/late·참조 종료

위 본문은 고정 입력의 짧은 대조/보호만 설명합니다. decode/refill·PCM 대량 복사·동적 할당·blocking/wait·긴 순회·정책/복구 실행·Driver 의미 분석을 넣지 않습니다. ISR에서 AudioTx_Request/AudioTx_Advance/AudioControl_Service·FLOW/PLAYBACK/HEALTH 호출, device readiness·PCM usage·TX result·PB/Fault write를 하지 않습니다.

즉시 callback은 요청 효력 전에 보호된 원등록으로만 처리합니다. STOP/old config/uncertain 최종·retired 뒤 raw도 old 원등록에 보존하고 후속 old 정리/진단에만 연결합니다. 새 Session/PCM cycle/READY/Fault 복원은 없습니다. persistent vendor 등록과 개별 SW 원등록은 1:1·동일 수명으로 가정하지 않습니다.

HAL 원등록은 HW·대기·처리 중 SW·late 대응 참조가 모두 실제 종료한 뒤 HAL이 회수합니다. Driver 필요한 원 facts의 안전한 정규화/보호·consumer 종료까지 raw를 유지합니다. PCM 반환·readiness INVALIDATED·PB retirement·IRQ disable·알림/얕은 복사·void 반환은 이 회수 근거가 아닙니다. old 위험 종료·현재 쓰기 안전·alias 없는 보호가 입증되면 old 불변 기록과 새 자원 사용은 공존할 수 있습니다.

HALF/MAJOR/abort ACK/현재 idle·mute는 source consumption/safe return/noFutureAccess·현재 CPU writable·actual/NO_START/TERMINATION 충분조건이 아닙니다. ISR 예산·저장/loss visibility·실제 vendor ABI·지속 raw 상관·원 시각/메모리/물리 효과는 BINDING_TBD/HW_UNVERIFIED입니다. F1~F3 후보와 실제 제품 Gate를 구분하며 시험은 TEST_NOT_RUN입니다.

<a id="references"></a>
## 사후조건과 정본

원 key/시간·실제 raw를 합성하지 않고 실제 부족과 손실을 구분합니다. 접근 불가/공간 부족에도 확인된 원 facts·등록/후속 의무를 유지합니다. CONTROL/TX의 의미 분석은 ISR 이후 기존 Driver 경계에 남습니다. HAL Boundary는 Module 수를 늘리지 않습니다.

- [설명 계약](../functions/AudioHAL_Callback.md#contract) · [등록 대응](../functions/AudioHAL_Callback.md#registration) · [접근 불가](../functions/AudioHAL_Callback.md#invalid-input) · [손실](../functions/AudioHAL_Callback.md#loss) · [수명](../functions/AudioHAL_Callback.md#lifetime) · [F1~F6/미결](../functions/AudioHAL_Callback.md#feasibility)

- [R2](../../../../design/30_FUNCTIONS/50_DRIVER_HAL_FUNCTIONS.md#audiohal-callback) · [R3 raw](../../../../design/40_DATA/60_AUDIO_TX_DATA.md#vssrawobservation) · [R4 원근거](../../../../design/50_CONTRACTS/20_ASYNC_RESULT_EVIDENCE.md#evidence-basis) · [R4 수명](../../../../design/50_CONTRACTS/10_OWNERSHIP_LIFETIME.md#references-and-release) · [R4 지속 stream](../../../../design/50_CONTRACTS/50_AUDIO_BUFFER_TRANSPORT.md#continuous-stream) · [R5 HAL](../../../../design/60_PSEUDOCODE/40_AUDIO_PREPARE_TX_FLOW.md#audiohal-callback) · [R6 수명](../../../../design/70_C_INTERFACE/00_HEADER_OWNERSHIP_MAP.md#lifetime)

- [승인 TX Advance](../../../03_DRIVER/AUDIO_TX/pseudocode/AudioTx_Advance.md#procedure) · [CONTROL 후속](../../../03_DRIVER/AUDIO_CONTROL/pseudocode/AudioControl_Service.md#followup)
