# AudioControl_Service — 장치별 현재 구성 준비·제어의 후속

[VSS 지도](../../../01_VSS_MAP.md) · [DRIVER Layer](../../00_LAYER_OVERVIEW.md) · [AUDIO_CONTROL](../00_MODULE_OVERVIEW.md) · [한국어 의사코드](../pseudocode/AudioControl_Service.md)

> CORE-15 · D5-2 SELF_CHECKED / INDEPENDENT_REVIEW_PENDING · GD5 전체 NOT_RUN. LOGICAL_R6 / C ABI BINDING_TBD / HW_UNVERIFIED / TEST_NOT_RUN.

Codec·Clock Generator·Clock Reference의 원 제어를 구성별로 진행합니다. AUDIO_CONTROL이 자신의 readiness와 원 제어 결과를 쓰고, HAL의 보호된 raw를 후속 처리에서 대조합니다. 요청 접수와 실제 준비·제어 완료를 구분합니다.

<a id="contract"></a>
## 1. R6 원형과 독립 OUT 5종

```text
void AudioControl_Service(
    OPTIONAL IN const VssDeviceControlIntent *intent,
    IN const VssTimeEvidence *time,
    OUT VssDeviceReadinessState devices[capacity -> written],
    OUT VssDeviceControlResult results[capacity -> written],
    OUT VssDiagnosticEvidence diagnostics[capacity -> written],
    OUT VssRecoveryPerformedEvidence performed[capacity -> written],
    OUT VssRecoveryVerificationEvidence verified[capacity -> written]);
```

[R6 정본](../../../../design/70_C_INTERFACE/00_HEADER_OWNERSHIP_MAP.md#r6-audiocontrol-service) 그대로입니다. OPTIONAL/IN/OUT와 capacity→written은 [논리 표기](../../../../design/70_C_INTERFACE/00_HEADER_OWNERSHIP_MAP.md#prototype-notation)이며 실제 C Header·함수 인자 배치·폭·저장 방식은 미정입니다. R2의 과거 provisional `VssDeviceReadiness` 반환을 복원하거나 다섯 결과를 한 성공 bool/통합 Result로 합치지 않습니다. void·written=0은 장치 성공/완료·참조 해제가 아닙니다.

| 인자 / 기존 R3 자료 | 의미·원본 / 유효성·수명 |
|---|---|
| intent / VssDeviceControlIntent | STREAM의 operation/device/configuration/action/recoveryPermission **5필드**. NULL은 기존 수용 작업 후속만. 접근 가능한 원 identity/구성·action을 검증한 뒤 읽고 효력 전에 불변 보호 |
| time / VssTimeEvidence | 이번 후속 기회의 시간·domain/epoch/continuity. 동기 borrow; 원 raw/진단·수행/검증 시각을 now로 바꾸지 않음 |
| devices / VssDeviceReadinessState | CONTROL이 적용 완료한 device/configuration/state/revision의 안정된 값/const 관측. 해당 구성의 읽기 수명 보호; mutable 원본 주소를 장기 공유하지 않음 |
| results / VssDeviceControlResult | 실제 수용한 operation/device/configuration/state **4필드**의 원 제어 사실. STREAM 반영/독립 보호 추적까지 불변 |
| diagnostics / VssDiagnosticEvidence | 실제 발견 producer/stage/cause/target/configuration/optional operation/occurredAt. 다른 결과군과 독립, 필수 문맥 미확인은 raw/부족 근거로 보호 |
| performed / VssRecoveryPerformedEvidence | 실제 수행의 permission/target/configuration/operation/effect/occurredAt. 실제 수행 주체의 사실만, 수용/ACK로 성공 합성 없음 |
| verified / VssRecoveryVerificationEvidence | 별도 효과 관측의 permission/performedOperation/target/configuration/effect/verifiedAt. 수행 이후 인과·범위 입증; performed와 개수/전달 시점 독립 |

<a id="ownership"></a>
## 2. Caller·writer·하위 책임

Caller는 [Prepare](../../../02_SERVICE/AUDIO_STREAM/functions/AudioStream_Prepare.md#read-and-consume), [RequestControl](../../../02_SERVICE/AUDIO_STREAM/functions/AudioStream_RequestControl.md#recovery), [Advance](../../../02_SERVICE/AUDIO_STREAM/functions/AudioStream_Advance.md#lower-results)입니다. FLOW는 STREAM을 통해 초기 준비/복구를 조율합니다. AUDIO_CONTROL은 DRIVER의 기존 Module이며 현재 장치별 readiness·자기 제어 문맥만 쓰는 단일 writer입니다.

Callee는 기존 HAL의 장치 제어·원등록/raw 읽기 경계와 필요한 Runtime 시간/후속 도구입니다. [AudioHAL_Callback](../../../04_HAL_BSP/AUDIOHAL_BOUNDARY/functions/AudioHAL_Callback.md#ownership)은 ISR에서 이 Core를 호출하지 않습니다. TX operation/PCM usage·STREAM provider·PLAYBACK Session/retirement·HEALTH Fault 원본은 읽기 근거일 뿐 직접 쓰지 않습니다. TX와의 조율은 STREAM 책임입니다.

기존 AudioCodec/AudioClock 제어 경계는 R6의 재사용 후보입니다. 실제 Header 본문·register/API·초기화 순서/ratio·generated callback ABI를 이번에 조사/확정하지 않았습니다. 새 장치별 wrapper·등록 API·Module을 만들지 않습니다.

<a id="devices"></a>
## 3. 세 장치와 상태를 독립 적용한다

| 장치 | 준비 근거의 범위 |
|---|---|
| VSS_DEVICE_CODEC / SGTL5000 | 그 Codec의 현재 configuration·실제 준비/제어 |
| VSS_DEVICE_CLOCK_GENERATOR / CS2100 | 그 Generator의 현재 configuration·실제 준비/제어 |
| VSS_DEVICE_CLOCK_REFERENCE | 그 Reference의 현재 configuration·실제 준비/제어 |

한 장치의 성공/실패를 다른 둘에 복사하지 않습니다. 유효한 장치/구성이 알려졌으나 아직 평가되지 않은 `VSS_DEVICE_UNASSESSED`는 정상적인 미평가입니다. 실제 준비 진행은 PREPARING, 현재 준비 충분성 확인은 READY, 해당 근거의 실제 무효화는 INVALIDATED, 확인된 준비 실패는 FAILED입니다. 원문의 다섯 상태 외 새 REJECTED/UNKNOWN 제어 enum을 만들지 않습니다. 미확인 장치를 READY/FAILED로 채우거나 구성 key를 합성하지 않습니다.

readiness revision은 그 장치/구성의 변경을 실제 적용 완료한 뒤 반영합니다. 단순 호출/OUT 전달/원등록 수신으로 증가하지 않으며 초기값·증가폭·wrap/ABI는 미정입니다. devices에는 지금 유효한 구성과 실제 확인 가능한 상태만 내보냅니다.

<a id="admission"></a>
## 4. 기존 후속을 보존하고 신규 의도를 검증한다

OUT군의 주소·metadata·capacity/written 저장소·범위/산술·비간섭을 먼저 검증합니다. 유효 군만 written=0, 무효 군은 written까지 읽거나 쓰지 않습니다. time 접근/비교가 무효면 시간 의존 신규 효력은 보류하되 보호된 불변 사실의 안전 인계와 기존 참조/정리 의무는 유지합니다.

intent=NULL이면 이미 수용한 제어의 원 operation/device/configuration을 따라 bounded 진행합니다. 기존 작업이 없으면 새 HW 제어·result·복구 사실을 만들지 않습니다. 유효한 현재 장치 관측의 안전 전달은 가능하며 초기 PB Session/Attempt가 없어도 가짜 identity가 필요하지 않습니다.

non-NULL intent는 외피 접근 가능성 다음에 장치/구성·action·원 operation, RECOVER의 내부 permission 참조를 검증합니다. PREPARE/INVALIDATE의 recoveryPermission은 None이며 복구 허용을 다른 행동에 사용하지 않습니다. 부적합 신규 요구·원귀속 없음·신규 보호 공간 부족은 효력 전에 거부합니다. 가짜 원 제어 result 없이 확인 가능한 진단만 보호하고, 기존 ready·수용 작업·원 결과를 변경하지 않습니다. OUT가 가득 찬 것과 내부 보호 부족을 구분합니다.

<a id="duplicate"></a>
### 같은 원 operation과 다른 내용

같은 operation에 같은 불변 5필드 의도가 이미 보호됐으면 원 결과/후속을 재사용하고 장치 동작을 재발행하지 않습니다. 같은 key에 다른 device/config/action/permission 의미가 들어오면 신규 충돌 진단으로 격리하고 기존 수용본을 보존합니다. key/wrap/동시 접근·멱등 추적의 실물 수단은 R6-04이며 새 manager/API가 아닙니다.

<a id="configuration"></a>
## 5. PREPARE·INVALIDATE와 구성 전환

현재 같은 configuration의 READY가 실제로 유효하면 PREPARE에서 기존 근거를 재사용합니다. 매 Attempt마다 전체 세 장치 reset/재초기화를 하지 않습니다. 실제 원 요구가 적법하게 수용된 경우에만 해당 operation/config의 제어 result를 보호합니다.

새 configuration의 적법한 재구성은 원 의도와 결과 보호, HAL 장치 원등록을 먼저 확보하고 실제 효력 직전 대상/허용·자원 안전을 재확인합니다. 실제 재구성/reset이 영향을 미치는 해당 장치의 옛 준비 근거만 무효화하며 새 구성을 PREPARING으로 추적합니다. 새 READY는 그 구성에 대한 실제 준비 충분성 확인 뒤에만 적용합니다. 검증/등록 실패로 새 효력이 없으면 기존 READY는 보존합니다. 이미 부분 효력이 생겼으면 옛 READY를 유효한 것으로 계속 쓰거나 완전 rollback하지 않습니다.

INVALIDATE는 실제 해당 장치/config의 준비 근거만 무효화합니다. 원 config가 현재와 다르면 old 근거/정리만 처리하며 다른 현재 READY를 변경하지 않습니다. 논리 readiness 무효화와 실제 HW reset/참조 종료는 서로 다른 행동입니다.

<a id="raw-and-partial"></a>
## 6. 원 raw 적용·부분 효력·old 결과

장치 효력/즉시 callback보다 먼저 HAL의 `VssHalDeviceRegistration` **registration/operation/device/configuration/binding**을 불변 보호합니다. 임시 intent 포인터나 얕은 recoveryPermission 복사는 보호가 아닙니다. 후속은 원 등록·raw의 실제 key/binding·수명·시각/coverage와 원 의도를 대조합니다.

귀속 부족/관측 부족은 INSUFFICIENT, 실제 누락/overflow/시각 연속성 손실은 LOST로 보호합니다. 충분하지 않은 raw·vendor 수락/timeout·장치 한 개 완료로 현재 READY·전체 성공을 생성하지 않습니다. 확인된 준비 실패만 해당 FAILED·원 producer 진단으로 적용합니다. 부분 효력/미확인은 해당 PREPARING 또는 실제 무효화된 INVALIDATED 등 기존 실제 상태와 원 불확실 근거·참조/후속을 유지하며 임의 실패/종료로 바꾸지 않습니다.

늦은 old device/config/operation의 사실은 원 결과·old 정리/진단으로 보존합니다. 새 config·다른 장치·새 Session/PCM/Fault에는 적용하지 않습니다. mute/control ACK는 actual·과거 NO_START·STOP/TERMINATION·PCM safe return의 충분조건이 아닙니다. [승인 TX Request](../../AUDIO_TX/functions/AudioTx_Request.md#registration)·[TX Advance](../../AUDIO_TX/functions/AudioTx_Advance.md#sufficiency)의 책임을 유지합니다.

<a id="recovery"></a>
## 7. RECOVER — 허용·실제 수행·별도 검증

RECOVER에서만 보호된 recoveryPermission이 필요합니다. HEALTH의 permission/faultInstance/faultRevision/target/configuration/action **6필드**와 현재 허용·실제 device/target/config·기존 action 매핑을 대조합니다. 누락/철회/old revision·대상/구성/action 불일치는 새 수행 불가이며 기존 제한·old 정리를 유지합니다. PREPARE/INVALIDATE에는 복구 permission으로 다른 행동을 허가하지 않습니다.

FLOW/STREAM의 보호된 원 안전 관측과 CONTROL/HAL 자신의 실제 위험을 함께 확인합니다. 관련 old PB 전체 retirement, provider/read·PCM·device 위험 종료, 남은 late 귀속의 alias 없는 보호가 실제로 필요합니다. 기록이 독립 보호된 상태에서 재사용할 수 있지만 모든 기록 폐기를 안전 조건으로 대신하지 않습니다. 수용 후에도 효력 직전 현재 6필드 허용·자원 안전을 다시 확인합니다. 부족이면 그 제품 동작만 보류합니다. exact cause→Fault/reset/action 정책이나 timeout/retry를 새로 지정하지 않습니다.

실제 원 수행의 완료/실패/미확인 근거가 있을 때 performed의 SUCCEEDED/FAILED/UNCONFIRMED·원 occurredAt을 보호합니다. 요구 수용·API 반환·ACK만으로 수행 성공을 쓰지 않습니다. verified는 실제 수행 **이후** 대상/config에 대한 별도 효과 관측과 원 performedOperation·permission 인과, verifiedAt 근거가 있을 때 따로 기록합니다. 수행 성공이 검증 성공을 함의하지 않습니다.

verified가 performed보다 먼저 전달되는 경우도 실제 수행 순서·인과는 보존해야 합니다. 둘을 동일 written/동시 생성으로 강제하지 않습니다. 철회/구성 변경 뒤 old 수행/검증은 원 사실로만 인계합니다. 현재 Fault/제한 해제는 HEALTH가 현재 instance/revision/target/config/action과 두 사실을 대조해 판단합니다. CONTROL은 Fault clear·PB 권한/age·replay 재설정을 하지 않습니다.

<a id="publish"></a>
## 8. 다섯 군의 인계와 독립 수명

각 군은 실제 원 타입 항목과 필요한 참조 보호를 해당 유효 범위에 기록했을 때만 자기 written을 증가시킵니다. devices full이어도 results/diagnostics/performed/verified는 각각 진행하며 results invalid이어도 다른 유효 군은 무조건 중단하지 않습니다. performed 또는 verified full에 맞춰 다른 사실을 합성/삭제하지 않습니다.

미인계 원 facts와 내부 참조는 consumer 반영 또는 독립 보호 추적 완료까지 producer가 보호합니다. OUT 복사·void 반환은 소비 ACK가 아닙니다. bounded 보호 예산 부족이면 새 수용/효력을 제한하며 기존 원기록 eviction·무제한 pool·새 ack API로 해결하지 않습니다. 실제 손실은 해당 범위의 LOST/부족·진단으로 드러내 충분성 판단을 차단합니다. loss 보존 수단 자체의 용량/원자성도 실물 미확인입니다.

HAL 원등록은 HW·대기·처리 중 SW·late 대응 참조가 모두 끝났을 때 HAL이 독립 회수합니다. readiness INVALIDATED/PCM 반환/IRQ disable/PB retirement는 그 종료를 대신하지 않습니다. CONTROL 원 의도·result·복구 facts의 소비 수명도 각각 따로 확인합니다.

<a id="feasibility"></a>
## 9. 구현 후보와 유지되는 Gate

| 기준 | 문서에서 고정한 논리 / 후속 후보 | 필요한 실제 근거 |
|---|---|---|
| F1_LOCKED_SEMANTIC | 단일 writer·장치/config 분리·NULL 후속·중복/OUT·수행≠검증 | 의미 계약 유지 |
| F2_PROVISIONAL_TESTABLE | 출처 있는 device/config·time/permission fixture 후보 | 폭/시간·정책 잠정값을 제품 확정으로 승격하지 않음 |
| F3_PARTIAL_INDEPENDENT | 3장치 부분 완료·old config·중복충돌·군별 full/invalid·독립 복구 전달 후보 | 실제 Host/제품 시험 TEST_NOT_RUN |
| F4_SOURCE_IMAGE_GATE | 기존 제어 경계 재사용 후보 | 실제 Header/ABI·원등록/결과 저장·key alias/직렬화·최신 source/image |
| F5_PHYSICAL_SAFETY_GATE | 미확인/LOST에서 READY·확정 회수 차단 | Codec/Clock 순서·ratio·register·실제 효과/검증·IRQ/시각·현재 DMA safe-write/WCET/cache |
| F6_DEPENDENT_FOLLOWUP | STREAM/TX 승인 계약과 HEALTH 소비 연결 | D5-3/GD5·D6 및 실제 결합은 별도 지시/검수 |

P1B_C1 REQUIRED_NOT_RUN(9 Core/16 Q), P2 HOLD·Q01~14/R6-01~08·외부 IF·SOURCE_IMAGE·TBD-FINAL·WINDOW PRODUCT 보류/TEST 격리를 유지합니다. Firmware/Host/Build/Flash/Board는 NOT_RUN이며 독립 검수는 대기입니다.

<a id="references"></a>
## 10. 고정 정본과 왕복 경로

출처는 VSS@ccab866eaf6b98cfd2f80e92ed4cc198c021f4c4, design tree 28628b353618047512cc645459f3c6fe66c1200c의 읽기 전용 R2~R6입니다. Master v1.1 CORE-15 정의를 따릅니다. 파일별 Git Blob/raw/LF SHA는 결과 ZIP 출처핀에 고정합니다.

[R2 역할/provisional](../../../../design/30_FUNCTIONS/50_DRIVER_HAL_FUNCTIONS.md#audiocontrol-service) · [R3 Intent](../../../../design/40_DATA/60_AUDIO_TX_DATA.md#vssdevicecontrolintent) · [Readiness](../../../../design/40_DATA/60_AUDIO_TX_DATA.md#vssdevicereadinessstate) · [Result](../../../../design/40_DATA/60_AUDIO_TX_DATA.md#vssdevicecontrolresult) · [장치 등록](../../../../design/40_DATA/60_AUDIO_TX_DATA.md#vsshaldeviceregistration) · [진단/복구](../../../../design/40_DATA/70_DIAGNOSTIC_DATA.md#vssrecoverypermission) · [R4 수명](../../../../design/50_CONTRACTS/10_OWNERSHIP_LIFETIME.md#references-and-release) · [R4 Async](../../../../design/50_CONTRACTS/20_ASYNC_RESULT_EVIDENCE.md#partial-and-uncertain) · [R4 복구](../../../../design/50_CONTRACTS/40_FAULT_RECOVERY.md#current-target) · [R5 Startup](../../../../design/60_PSEUDOCODE/10_STARTUP_FLOW.md#audiocontrol-service) · [R5 복구](../../../../design/60_PSEUDOCODE/60_FAULT_RECOVERY_FLOW.md#device-recovery) · [R6 수명](../../../../design/70_C_INTERFACE/00_HEADER_OWNERSHIP_MAP.md#lifetime) · [R6 미정](../../../../design/70_C_INTERFACE/00_HEADER_OWNERSHIP_MAP.md#tbd-register) · [의사코드 분기](../pseudocode/AudioControl_Service.md#procedure)
