# AudioStream_Advance — 상세 동작 설명

[VSS 지도](../../../01_VSS_MAP.md) / [SERVICE](../../00_LAYER_OVERVIEW.md) / [AUDIO_STREAM](../00_MODULE_OVERVIEW.md) / CORE-12 · [한국어 C 의사코드](../pseudocode/AudioStream_Advance.md)

> GUIDE · LOGICAL R6 CONTRACT · 실제 C ABI/물리 출력 및 DMA 쓰기 안전은 BINDING_TBD/HW_UNVERIFIED.

보호된 STREAM 준비·제어·복구와 TX/CONTROL의 원 사실을 bounded 진행해 독립 결과로 전달한다. AUDIO_STREAM은 provider와 PCM A/B usage의 단일 writer이며 TX operation·CONTROL readiness·PLAYBACK Session·HEALTH Fault 원본을 직접 변경하지 않는다.

<a id="contract"></a>
## 원형과 1 IN / 7 독립 OUT

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

[최신 R6](../../../../design/70_C_INTERFACE/00_HEADER_OWNERSHIP_MAP.md#r6-audiostream-advance)의 **외부 IN은 time 하나**다. 하위 결과는 기존 callee 연결과 STREAM 보호 문맥에서 받는다. 새 public inputResults, broker API 또는 aggregate `VssAudioBackendResult`/lastResult를 만들지 않는다. 반환 `void`와 count=0은 통합 성공/완료/참조 해제의 뜻이 없다.

| 인자 | typed 의미 / producer → consumer | 필수 보호 |
|---|---|---|
| `time` | `VssTimeEvidence`; 현재 후속 처리·시간 비교 근거 | 접근 가능성·domain/epoch/continuity 확인. 원 actual/consumed/수행 시각을 처리 now로 교체하지 않음 |
| `requests` | `VssAudioRequestResult`; STREAM 단계 → PLAYBACK | 원 session/attempt/stage. 늦은 PREPARED/수락은 STOP·actual·종료 상태를 되돌리지 않음 |
| `outputs` | `VssAudioOutputEvidence`; TX 사실을 STREAM에서 결합 → PLAYBACK | session/attempt/kind/scope/transportFacts/count/futurePcmBlocked. 내부 transportFacts 실제 소비 수명까지 보호 |
| `cleanup` | `VssBackendCleanupObservation`; STREAM 파생 → PLAYBACK/FLOW | 원 attempt별 4개 독립 안전 조건. whole/retirement와 구별 |
| `devices` | `VssDeviceReadinessState`; CONTROL 적용 완료 관측 → FLOW 등 | current device/config의 기존 원 관측 전달. STREAM이 readiness writer가 되지 않음 |
| `diagnostics` | `VssDiagnosticEvidence`; 원 producer별 발견 → FLOW/HEALTH | producer/stage/cause/target/config/operation·원 시각/coverage 및 내부 참조 유지 |
| `performed` | `VssRecoveryPerformedEvidence`; 실제 Backend 수행 → FLOW/HEALTH | permission/target/configuration/operation/effect/occurredAt |
| `verified` | `VssRecoveryVerificationEvidence`; 별도 효과 관측 → FLOW/HEALTH | permission/performedOperation/target/configuration/effect/verifiedAt; performed와 독립 |

모든 OUT은 각기 다른 capacity/written이다. 주소·전달 영역·메타 저장소·산술 범위·비간섭을 읽기/쓰기 전에 검증하고 유효 군만 written=0으로 초기화한다. invalid 군 무접근, 유효 군은 다른 군의 invalid/full/0과 독립 전달한다. typed 자료를 실제 썼고 후속 참조가 보호된 경우만 해당 written을 증가시킨다. 미인계·미방문 사실과 수명은 producer에 남는다.

<a id="lower-results"></a>
## 하위 진행과 원 귀속

FLOW의 처리 기회에서 `AudioTx_Advance(time, requests, outputs, consumed, returned, diagnostics)`와 기존 `AudioControl_Service(optional intent 또는 NULL, time, devices, results, diagnostics, performed, verified)`를 R6의 독립 OUT 범위로 호출한다. 보호된 원 control/STOP/복구의 후속은 확인된 원 operation/대상에만 이어가며 중복 HW 명령을 만들지 않는다. 준비된 lower 결과 storage도 실제 보호 예산·borrow 조건을 확인한다. time이 무효면 시간 의존 진행/새 생산/confirmed 조합을 보류하되 이미 보호된 불변 사실의 안전 인계와 원 차단은 유지할 수 있다.

하위 결과 count=0이어도 pending 준비·정리·원 장치 startup을 bounded 진행한다. PB Session 없는 초기 장치 관측도 가능하다. devices는 CONTROL이 이미 반영한 현재 구성의 관측만 전달한다. CONTROL의 readiness state를 STREAM에서 직접 READY/FAILED로 쓰거나 미확인 장치를 기본 정상으로 채우지 않는다.

각 lower fact의 원 operation·Attempt·scope·PCM key/buffer/cycle 또는 device/config/permission을 제출 전 보호 문맥과 대조한다. `Sai_Ip_Send` 지속 등록/raw HALF/MAJOR IRQ 하나가 여러 Attempt를 가로지를 수 있으므로 개별 operation 상관을 현재 key로 채우지 않는다. 대응 부족은 INSUFFICIENT, 귀속/관측/저장 손실은 LOST로 보존한다. 충분성이 필요한 확정과 현재 PCM 해제는 금지한다.

하위 요청 단계는 원 START control인지 PCM handoff/STOP인지 구분한다. PCM 인계의 ACCEPTED를 START_ACCEPTED_AND_ENABLED로 변환하지 않고, STOP ACK를 출력 종료로 바꾸지 않는다. old Attempt/회차/device/config 결과는 그 old 정리·진단·참조 수명에만 적용한다. old A의 안전 반환을 new A나 B에 적용하지 않는다. uncertain final 뒤/retired 결과도 정상 PB 상태·현재 readiness·현재 Fault를 부활시키지 않는다. 최종 전 늦은 충분 actual의 적법성은 PB가 판단하며 STREAM에서 정상 생산을 재개하지 않는다.

<a id="pcm-return"></a>
## PCM source 소비, 안전 반환, 현재 쓰기

`VssPcmConsumptionEvidence`는 원 공급 구간의 실제 source 소비만 기록한다. 압축 bytes의 decoder 소비와도 구분한다. consumed·HAL HALF/MAJOR/complete/현재 idle 자체는 CPU_WRITABLE·FIFO/frame 종료·물리 출력 종료가 아니다. 해당 samples의 장치 재접근 가능성과 보호를 유지한다.

`VssPcmReturnEvidence`는 원 operation과 `key.attempt/buffer/cycle`, `noFutureAccess=true`, `basis.validity=VSS_COVERAGE_SUFFICIENT` 및 실제 범위를 제출 자료와 대조한다. 원 귀속 불충분이면 적용하지 않는다. old key가 현재 A/B와 다르면 old 정리/refs만 진행한다. 같은 원 회차에 충분 반환이 있더라도 **적용 시점의** 차기 DMA 방문 전 쓰기 완료 가능성·현재 안전 구간·메모리/cache/참조 조건·old 내용 재소비 금지를 별도로 재검사한다.

현재 안전이 미확인/경과하면 usage를 회수하지 않고 기존 HW_PROTECTED/RETURN_PENDING 등 보호·근거·후속을 유지한다. 현재도 충분한 경우에만 해당 원 회차를 CPU_WRITABLE로 검토한다. 이미 CPU_WRITABLE인 표시도 다음 write/zero-fill/refill 직전 다시 확인한다. 새 safeUntil 필드나 보장 수치를 만들지 않는다. 안전 반환은 provider.productionAllowed를 true로 복원하지 않는다.

<a id="production"></a>
## 준비 후속, 정상 refill, tail과 실패

원 PREPARING/PENDING preparation은 동일 provider/plan/Meta·원 Attempt/소비 위치에서만 bounded 진행한다. `AudioStream_Prepare`의 승인 내부 prime 절차를 사용하며 새 public 준비를 재발행하거나 age/decoder를 초기화하지 않는다. STOP/취소된 provider는 CLOSING·productionAllowed=false로 정리만 한다. 늦은 PREPARED는 원 단계 사실로 보호하되 권한/phase/actual을 되돌리지 않는다.

정상 후속 refill은 R5의 **STREAM 내부 단계** `AudioStream_FillBuffer`를 사용한다(Core/API 추가 아님). 원 provider·정상 허용 phase·productionAllowed=true, 정확히 A/B의 같은 원 key, CPU_WRITABLE, 이번 bounded 작업의 현재 write-safe, decoder/ASSET 참조 보호를 모두 확인한다. write/decode 직전과 안정된 PCM 참조 보호 후 **HANDOFF_PENDING/TX 효과 직전**에 STOP·원 Session/Attempt/A또는B/cycle·현재 안전을 재확인한다. 실패하면 새 handoff를 중단하고 기존 usage/원 refs/후속을 보호한다. 가짜 no-access나 무조건 CPU 반환을 만들지 않는다.

실제 CPU decoder consumed/produced 수와 범위/산술 검증 뒤 sourcePosition을 실제 소비 bytes만큼 진행한다. 압축 short tail·decoder drain·두 번째 B empty를 실제 유효 frame으로 표현하며 양쪽 full을 강제하거나 음향 padding/반복으로 늘리지 않는다. `validFrameCount=0`이면 새 sound handoff가 없으며 지속 DMA가 실제 멈췄다는 뜻은 아니다. 디코딩된 zero sample은 유효 sound frame이다. 물리 zero-fill은 유효 음향 길이에 포함하지 않고, STOP 뒤에도 기존 A/B의 현재 쓰기 안전을 충족한 정리 범위만 허용한다.

PFlash lookup/metadata/bounds/read 실패는 ASSET 발견, corrupt/unsupported/decode 실패는 STREAM 발견으로 원 producer/stage를 보존한다. 확인된 producer 실패일 때만 원 실패/진단·차단·cleanup을 보호한다. 준비 실패는 수용된 준비의 PREPARE_FAILED이며 정상 STOP 자체를 실패로 바꾸지 않는다. 불명확은 pending/원 근거 부족으로 유지한다. 이미 인계된 samples/TX/device/진행 reader의 보호를 rollback하지 않는다.

<a id="references"></a>
## 압축 span·PCM·원 결과 수명의 종료

decoder/read·대기/진행 consumer가 실제 원 span을 더 읽지 않는 근거가 있을 때만 `Asset_Read(NULL, 원 span key, readerReferencesEnded=true, ...)`의 REFERENCE_END lane을 연결한다. 실제 RELEASED 확인 뒤 그 원 STREAM 참조만 해제한다. STILL_PROTECTED/미확인은 원 span과 후속 의무를 유지한다. source consumed·압축 EOF·새 read·PB retirement는 참조 종료가 아니다.

PCM samples·handoff key·transportFacts·permission·진단 내부 참조·HAL 원 기록은 각각 실제 consumer 수명으로 보호한다. safe return/출력 owner 해제·IRQ disable/Session retirement만으로 남은 불변 callback/결과 기록을 폐기하지 않는다. old 위험 참조가 종료되고 현재 쓰기 안전·alias 없는 보호 예산이 충분하면 old 불변 기록과 new cycle은 독립적으로 존재할 수 있다.

<a id="outputs"></a>
## 출력 충분조건과 Backend 범위

모든 결합은 원 Session/Attempt·실제 operation/구간 대응·scope·원 시각/activation 인과·coverage의 충분성을 먼저 확인한다. 과거 자료를 처리 time으로 바꾸지 않는다. 여러 operation/descriptor/FIFO/frame이 scope에 관련되면 필요한 전체 범위가 각각 덮여야 한다. SEGMENT enum 동일성이나 한 cue/EOS를 전체 종료 근거로 쓰지 않는다.

| 생성 가능한 kind | 필요한 독립 근거 | 남는 상위 판단 |
|---|---|---|
| `OUTPUT_START_CONFIRMED` | 원 활성화 뒤 해당 실제 operation/scope의 새 물리 출력, 원 사실 시각 및 충분 coverage | PB가 원발생 시간/당시 권한·최초 적법성/Started를 판단. actual만이면 futurePcmBlocked=false 가능 |
| `NO_START_CONFIRMED` | 필요한 **과거 무출력 충분성 AND 모든 해당 old 미래 출력 불가능 AND 이 scope 미래 decode/refill/PCM 제출 차단** | PB가 전체 관련 범위·start/cue/repeat 차단·late 보호·자원 안전을 추가해 retirement/미시작 재평가 판단 |
| `OUTPUT_TERMINATION_CONFIRMED` | 이 scope의 **실제 장치/잔류 출력 종료 AND 모든 해당 old 미래 출력 불가능 AND 이 scope 미래 PCM 생산/제출 차단** | PB 전체 Completed/Interrupted·retirement는 별도. 과거 NO_START를 소급 증명하지 않음 |

submitted/accepted/READY·단일 ISR·timeout·STOP ACK·abort/mute/idle/현재 무음/zero-fill은 위 충분성 대체가 아니다. 조건이 부족/손실이면 incomplete kind를 typed confirmed output으로 내보내지 않고 원 transport facts·부족/LOST 진단·정리 후속을 보호한다. 기존 `START_OUTCOME_UNCERTAIN`의 지위는 유지하지만 단순 부족/timeout으로 상위 불명확 최종을 합성하지 않으며 그 최종 조건은 기존 미정이다. 물리 fact 자체가 충분하고 PB 시간 적법성 비교가 부족한 경우는 fact를 보존해 PB로 전달하며 정상 Started를 대신 판정하지 않는다.

<a id="cleanup"></a>
## cleanup의 네 독립 조건

`VssBackendCleanupObservation.attempt`는 원 Attempt다. `providerReferencesEnded`는 관련 provider/read 실제 reader 참조가 모두 끝난 근거, `pcmAccessEnded`는 그 Attempt의 관련 A/B 모든 회차의 하위 미래 접근 종료, `lowerOutputBlocked`는 해당 old 출력/잔류 가능성 차단, `lateIdentityProtected`는 남은 callback/result 원 귀속의 alias 없는 보호를 각각 반영한다.

하나만 true이면 나머지를 true로 채우지 않는다. provider CLOSED≠물리 출력 끝, PCM return≠lower output 종료, old 기록 유지≠위험한 samples 접근 유지다. 근거 미확인 항목은 완료 false/미확인으로 남기고 진단을 별도로 보존한다. cleanup은 관측이며 PB whole retirement나 모든 storage 폐기 명령이 아니다.

<a id="recovery"></a>
## 복구 수행과 별도 효과 검증

RequestControl에서 보호한 6필드 허용과 실제 원 operation에 Backend 결과를 연결한다. 실제 수행 근거가 있으면 performed의 `permission/target/configuration/operation/effect/occurredAt`을 보호한다. 수용만으로 생성하지 않으며 SUCCEEDED/FAILED/UNCONFIRMED는 실제 확인 범위다.

별도 효과 관측이 실제 수행 이후 인과·원 대상/구성과 충분하게 연결됐을 때 verified의 `permission/performedOperation/target/configuration/effect/verifiedAt`을 별도로 보호한다. performed 성공만 있으면 performed만 존재할 수 있고 verified 성공을 합성하지 않는다. verified가 먼저 전달돼도 원 performedOperation/permission의 보호된 추적과 별도 충분 근거가 필요하며 두 OUT을 동시에 채우도록 강제하지 않는다.

현재 permission instance/revision/target/config/action이 바뀌거나 철회되면 새 수행은 중지한다. old 수행/검증 결과는 원 자료·old 진단으로 보존하고 현재 READY/Fault로 적용하지 않는다. FLOW를 통해 두 사실을 HEALTH에 전달하며 최종 해당 제한 해제는 HEALTH만 현재 허용과 performed/verified pair를 대조해 판단한다.

<a id="publish"></a>
## 독립 결과 인계와 bounded 종료

하위 사실 수집·적용·준비/정리·생산·output 결합·복구의 방문은 승인된 bounded 기회 안에서 진행한다. 새로운 횟수/주기/timeout/queue를 확정하지 않는다. 한 군의 OUT 부재로 다른 군과 안전한 정리까지 일괄 중단하지 않는다. 필요한 보호가 부족하면 그 새 효과/생산/결합을 보류하고 기존 facts·정리 의무를 유지한다.

7개 군의 원 불변 항목을 각각 실제 안전 공간에 인계하고 각 written을 따로 증가시킨다. outputs의 transportFacts 등 얕은 복사 뒤 참조를 버리지 않는다. 미인계 원 사실은 producer가 소비자 반영/독립 보호 추적까지 보존하고 실제 손실이면 coverage LOST/진단을 관측 가능하게 남긴다. `return;`은 처리 기회를 끝낼 뿐 전체 작업 성공이 아니다.

<a id="gates"></a>
## 구현 후보·확인 한계와 호출 관계

독립 SW/Host 후보는 7 OUT bounds/무효군·원 operation/cycle·old 결과 격리·STOP 차단·source consumed/return 분리·출력 충분조건 결합·cleanup 네 조건·performed/verified 분리·참조 수명이다. 실제 Host/Firmware/Build/Flash/Board는 NOT_RUN이다. 물리 output/NO_START/TERMINATION fence·DMA next-visit/WCET·cache/barrier·decoder/PCM format·현재 source/image·C ABI/저장/직렬화·Recovery Driver map/효과 방법만 BINDING_TBD/HW_UNVERIFIED로 남긴다. P2 HOLD는 유지한다.

Caller: [FLOW](../../../01_APP/FLOW/functions/Flow_Process.md)의 처리 기회. 인접: [RequestControl](AudioStream_RequestControl.md), [Prepare](AudioStream_Prepare.md), [Asset_Read](../../ASSET/functions/Asset_Read.md), [TX Advance](../../../03_DRIVER/AUDIO_TX/functions/AudioTx_Advance.md), [PLAYBACK Advance](../../PLAYBACK/functions/Playback_Advance.md). CONTROL/HEALTH 상세는 [AudioControl_Service](../../../03_DRIVER/AUDIO_CONTROL/functions/AudioControl_Service.md)와 [Health_Evaluate](../../HEALTH/functions/Health_Evaluate.md)·[Health_BuildStatus](../../HEALTH/functions/Health_BuildStatus.md)에서 읽고, 논리 계약은 아래 동결 정본으로 확인한다.

[R2](../../../../design/30_FUNCTIONS/40_AUDIO_STREAM_ASSET_FUNCTIONS.md#audiostream-advance) · [R3 provider](../../../../design/40_DATA/50_AUDIO_STREAM_DATA.md#vssprovidercontext) · [R3 PCM](../../../../design/40_DATA/50_AUDIO_STREAM_DATA.md#vsspcmbufferstate) · [R3 consumption](../../../../design/40_DATA/60_AUDIO_TX_DATA.md#vsspcmconsumptionevidence) · [R3 return](../../../../design/40_DATA/60_AUDIO_TX_DATA.md#vsspcmreturnevidence) · [R3 output](../../../../design/40_DATA/50_AUDIO_STREAM_DATA.md#vssaudiooutputevidence) · [R3 cleanup](../../../../design/40_DATA/50_AUDIO_STREAM_DATA.md#vssbackendcleanupobservation) · [R3 performed](../../../../design/40_DATA/70_DIAGNOSTIC_DATA.md#vssrecoveryperformedevidence) · [R3 verified](../../../../design/40_DATA/70_DIAGNOSTIC_DATA.md#vssrecoveryverificationevidence) · [R4 보호](../../../../design/50_CONTRACTS/10_OWNERSHIP_LIFETIME.md#borrow-and-protect) · [R4 late](../../../../design/50_CONTRACTS/10_OWNERSHIP_LIFETIME.md#late-attribution) · [R4 네 경계](../../../../design/50_CONTRACTS/50_AUDIO_BUFFER_TRANSPORT.md#four-boundaries) · [R4 지속 전송](../../../../design/50_CONTRACTS/50_AUDIO_BUFFER_TRANSPORT.md#continuous-stream) · [R4 현재 안전](../../../../design/50_CONTRACTS/50_AUDIO_BUFFER_TRANSPORT.md#safe-write) · [R4 유효/무음](../../../../design/50_CONTRACTS/50_AUDIO_BUFFER_TRANSPORT.md#valid-and-silence) · [R4 충분조건](../../../../design/50_CONTRACTS/20_ASYNC_RESULT_EVIDENCE.md#confirmed-outcomes) · [R5 준비](../../../../design/60_PSEUDOCODE/40_AUDIO_PREPARE_TX_FLOW.md#audiostream-prepare) · [R5 fill](../../../../design/60_PSEUDOCODE/40_AUDIO_PREPARE_TX_FLOW.md#fill-buffer) · [R5 Advance](../../../../design/60_PSEUDOCODE/40_AUDIO_PREPARE_TX_FLOW.md#audiostream-advance) · [R5 반환](../../../../design/60_PSEUDOCODE/50_STOP_LATE_UNCERTAIN_FLOW.md#stream-safe-return) · [R5 복구](../../../../design/60_PSEUDOCODE/60_FAULT_RECOVERY_FLOW.md#device-recovery) · [R6 TX](../../../../design/70_C_INTERFACE/00_HEADER_OWNERSHIP_MAP.md#r6-audiotx-advance) · [R6 CONTROL](../../../../design/70_C_INTERFACE/00_HEADER_OWNERSHIP_MAP.md#r6-audiocontrol-service)
