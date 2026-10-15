# AudioTx_Request — 원래 작업을 보호한 PCM 인계·출력 제어 요청

> A1 입력 개정 2026-10-15 · [현재 Network 연결](../../../../design/90_BINDING/92_NETWORK_BINDING.md). `L=min(양수 USE_LIMIT,500ms,2000ms)`; 최초 actual에만 적용. 과거 동결/PASS는 당시 범위의 이력이며 본문 변경은 이번 A1 결정이다. 구현·시험 NOT_RUN.

[AUDIO_TX](../00_MODULE_OVERVIEW.md) · [VSS 지도](../../../01_VSS_MAP.md) · [한국어 의사코드](../pseudocode/AudioTx_Request.md)

> CORE-13. R2~R6 논리 계약의 상세 설명이며 실제 C ABI·물리 안전의 확정본은 아닙니다.

<a id="contract"></a>
## 1. 공개 경계와 목적

AudioTx_Request는 AUDIO_STREAM의 유효 PCM 인계 또는 START/STOP 제어 하나를 받아, 하위 효력 전에 원래 operation·Attempt·PCM 회차와 필요한 참조를 보호합니다. 동기 호출은 요구의 현재 단계와 즉시 확인한 요청/반환/진단을 제공합니다. 나중에 도착하는 장치 사실은 [AudioTx_Advance](AudioTx_Advance.md#contract)가 원래 작업에 적용합니다.

[R6 원형](../../../../design/70_C_INTERFACE/00_HEADER_OWNERSHIP_MAP.md#r6-audiotx-request)을 그대로 읽습니다.

```text
VssTxRequestState AudioTx_Request(
    ONE_OF const VssPcmHandoffCommand *pcm,
           const VssTxControlCommand *control,
    OPTIONAL IN const VssOperationKeyType *continuingOperation,
    IN const VssOperationKeyType targetOperations[count],
    IN const VssTimeEvidence *time,
    OUT VssTxRequestResult requests[capacity -> written],
    OUT VssPcmReturnEvidence returned[capacity -> written],
    OUT VssDiagnosticEvidence diagnostics[capacity -> written]);
```

`ONE_OF`는 별도 두 포인터 중 정확히 하나라는 논리 표기입니다. 둘 다 없음/둘 다 있음은 무효이며 union·void payload로 합치지 않습니다. `OPTIONAL`, `count`, `capacity -> written`의 실제 C 표현·정수 폭·Header·메모리 배치는 Binding TBD입니다. scalar 반환과 세 OUT군은 별개이며 scalar 하나로 실제 출력·안전 반환·전체 완료를 주장하지 않습니다.

<a id="ownership"></a>
## 2. 호출자·writer·후속 책임

| 경계 | 담당하는 원본과 행동 |
|---|---|
| Caller | AudioStream_Prepare / AudioStream_RequestControl / AudioStream_Advance. FLOW·PLAYBACK은 TX를 직접 호출하지 않습니다. |
| AUDIO_STREAM | PCM key·내용·usage와 provider 생산 권한. 호출 전에 해당 PCM을 HANDOFF_PENDING으로 보호하고, STOP 전에 관련 Session의 모든 Attempt 생산을 차단합니다. |
| AUDIO_TX | 실제 개별 VssTxOperation의 생성·장치 접근/미래 출력 가능성·requestState와 원래 작업의 typed 결과. STREAM usage·Session 상태는 쓰지 않습니다. |
| HAL Boundary | AUDIO_TX가 제공한 원래 의미의 불변 VssHalTxRegistration 보호와 짧은 raw 포착. 현재 Attempt로 사후 라벨을 붙이지 않습니다. |
| 후속 | AudioTx_Advance는 보호된 raw·기존 operation의 후속 사실을 독립 5 OUT로 전달합니다. STREAM이 즉시/후속 결과를 원 key에 적용하고 PLAYBACK이 전체 Session, HEALTH가 Fault를 판단합니다. |

하위 호출은 기존 HAL/Runtime 경계만 사용합니다. 새 public 등록 API·Queue·Task·pool·세 번째 PCM 버퍼를 정의하지 않습니다. 정확한 호출 symbol·직렬화·보호 저장 수단은 후속 Binding에서 대조합니다.

<a id="inputs"></a>
## 3. typed 입력과 유효 조건

| 입력 | 의미·검사·수명 |
|---|---|
| pcm | [VssPcmHandoffCommand](../../../../design/40_DATA/60_AUDIO_TX_DATA.md#vsspcmhandoffcommand)의 key, samples, firstValidFrame, validFrameCount, format, lastForSegment. key는 원 Attempt/A 또는 B/회차이며 내용·유효 구간이 안정된 HANDOFF_PENDING 인계만 받습니다. |
| control | [VssTxControlCommand](../../../../design/40_DATA/60_AUDIO_TX_DATA.md#vsstxcontrolcommand)의 attempt, action, scope, firstStartMeta **4필드**. PCM 주소·회차가 없고 제어 operation의 handoff=NULL입니다. action은 기존 START/STOP, scope는 기존 SEGMENT/ATTEMPT만 허용합니다. |
| continuingOperation | 실제 기존 operation의 의미 key. NULL은 진짜 신규 요구가 하위 효력을 내기 전일 때만 허용합니다. 기존 요구의 key가 불명확하다고 NULL 신규 제출로 대체하지 않습니다. |
| targetOperations | PCM/START는 count=0. STOP은 STREAM이 실제 준비/제어 인계와 공개 TX 결과에서 보호한 원래 대상 operation key 목록입니다. count와 저장 범위를 안전하게 확인한 뒤 원 Attempt·scope·실제 구간을 대조합니다. |
| time | 현재 평가 시간·연속성 근거. 원발생 Meta, 원래 사건 시각, HAL 포착 시각을 now로 덮지 않습니다. 시간 근거 부족은 해당 비교의 부족으로 유지합니다. |

입력 포인터·목록의 읽기 유효성을 확인한 뒤 필드를 읽습니다. 범위 판정에서 `firstValidFrame + validFrameCount`의 overflow를 피하고 실제 제공 span 안인지 확인합니다. samples 수명/format 확인 불가·0 frame·범위 초과는 새 PCM 인계 불가입니다. 기저 폭·sample rate·버퍼 크기·wrap 규칙을 새로 지정하지 않습니다.

<a id="new-and-continuing"></a>
## 4. 신규 거부와 기존 작업의 후속을 구별한다

**작업 생성 전 진짜 신규·무효과 거부:** 입력 위반·첫 gate 부족·원귀속 보호 공간 부족이면 하위 호출과 operation 생성을 하지 않습니다. 이 새 요구에 효력 없음이 확인된 경우에만 `VSS_TX_REJECTED_WITH_NO_ACCESS`를 반환합니다. 유효한 requests/returned의 written은 0이며 가짜 operation·VssTxRequestResult·VssPcmReturnEvidence를 만들지 않습니다. diagnostics는 독립 유효성/용량에 따라 operation 없는 실제 발견 진단을 전달합니다. 같은 PCM 주소의 옛 operation이 남아 있으면 그 참조는 그대로 보호됩니다.

**기존 operation 후속:** continuingOperation을 원래 operation/불변 command·Attempt·PCM key 또는 제어 범위와 대조합니다. 같은 Attempt/action/scope 값만으로 다른 SEGMENT를 같은 요구로 합치지 않습니다. 이미 제출한 동일 요구는 하위 START/PCM/STOP을 재발행하지 않고 기존 단계·미전달 사실의 후속만 처리합니다. key 누락/불일치/alias 가능성은 보호된 조회·진단으로 남기며 새 key로 재제출하거나 옛 작업을 거부 완료로 만들지 않습니다. 확인되는 원 작업의 현재 requestState를 유지하고, 효과 여부가 확인되지 않는 범위는 EFFECT_UNCERTAIN으로 표현합니다. 이미 진행 중인 정리의 CLEANUP_PENDING은 유지합니다. 실제 key 미확인 상태에서 typed 작업 사실은 발급하지 않습니다.

**operation 생성 뒤 부분효과:** 실패 반환만으로 무효과 rollback을 하지 않습니다. 장치 retain·일부 활성화·descriptor/FIFO/frame 또는 소프트웨어 참조 가능성이 남으면 실제 operation과 원등록·PCM 보호를 유지하고 `VSS_TX_EFFECT_UNCERTAIN` 또는 `VSS_TX_CLEANUP_PENDING`을 반환/기록합니다. 기존 작업의 정리·결과 추적을 계속합니다.

<a id="pcm"></a>
## 5. PCM 구간·A/B 회차

유효 PCM 인계는 validFrameCount > 0이며 짧은 tail은 실제 길이로 유지합니다. lastForSegment는 해당 구간의 마지막 block 표시이지 전체 Session 완료가 아닙니다. 원음의 값 0 sample도 유효 PCM입니다. 빈 B·padding·정리용 물리 zero-fill을 유효 음향 frame으로 합성하지 않습니다.

key는 [Attempt/Buffer/cycle](../../../../design/40_DATA/50_AUDIO_STREAM_DATA.md#vsspcmcyclekey) 전체로 대조합니다. 같은 A 주소의 옛 회차 결과는 새 A 회차나 B를 반환하지 못합니다. 인자 구조체가 호출 뒤 끝나도 보호된 불변 인계와 실제 samples 참조는 하위 HW·대기·처리 참조 종료까지 유지합니다. const 포인터의 얕은 저장만으로 보호 완료라 하지 않습니다.

AUDIO_TX는 해당 장치 접근을 보호하고 AUDIO_STREAM의 usage를 직접 CPU_WRITABLE로 바꾸지 않습니다. consumed, safe return, 현재 CPU 쓰기 안전, 출력 끝, Session retirement는 [서로 다른 경계](../../../../design/50_CONTRACTS/50_AUDIO_BUFFER_TRANSPORT.md#four-boundaries)입니다. 생산 권한과 현재 차기 DMA 방문 전 safe-write 검사는 STREAM 책임입니다. STOP 뒤 정리용 zero-fill도 그 쓰기 안전을 우회할 수 없습니다.

<a id="start"></a>
## 6. START와 최초 One-shot gate

START에는 targetOperations count=0, handoff=NULL을 적용합니다. 아직 Started가 아닌 최초 One-shot은 원래 firstStartMeta와 현재 time의 비교 가능성·권한/현재 gate를 하위 효력 직전에 확인합니다. `[잠정] 원발생 age < L`이며 정확히 L은 허용되지 않습니다. 재시도·준비·now로 원 age를 갱신하지 않습니다. A1에서 선택한 원 firstStartMeta.useLimit=L을 그대로 사용합니다.

이미 최초 Started가 확인된 정상 같은 Session의 후속 cue에서는 firstStartMeta=NULL이며 최초 age gate를 다시 적용하지 않습니다. STOP도 NULL입니다. NULL 자체가 최초 gate 면제 증거는 아닙니다. 원래 요구 문맥에서 최초/정상 후속 구분을 확인할 수 없으면 새 START를 승인하지 않습니다. STOPPING/ABORTING·uncertain 최종/retired 문맥에 새 START 권한을 부여하지 않습니다.

하위 수용/활성화의 실제 근거가 있어야 ACCEPTED_AND_ENABLED입니다. activationAt은 실제 활성화 시각/구간이며 제출 now가 아닙니다. accepted/enabled는 actual sound start가 아닙니다. actual의 원래 시각·기한/권한 적용과 Started 통지는 후속 TX→STREAM→PLAYBACK 경계에서 처리합니다.

<a id="stop"></a>
## 7. STOP의 실제 대상과 부분 범위

STREAM이 하위 STOP 전에 관련 Session의 모든 Attempt에 productionAllowed=false를 적용합니다. 아직 순회하지 않은 provider도 새 decode/refill/PCM 제출 불가여야 합니다. AUDIO_TX가 이 STREAM 원본을 대신 쓰거나 하위 STOP 수락으로 선차단 완료를 추정하지 않습니다.

STOP은 주소 없는 control이며 원래 targetOperations의 실제 operation/Attempt·scope·descriptor/pending/FIFO/frame 범위에만 적용합니다. 같은 SEGMENT enum인 서로 다른 구간은 원 operation 집합과 실제 범위로 구별합니다. 새 SegmentKey·범용 target API를 추가하지 않습니다. 빈/불명확/충분하지 않은 target 목록을 전체 STOP 완료로 바꾸지 않습니다.

보호된 일부 대상의 실제 처리는 그 범위 안에서만 추적하고 미방문·미확인 대상은 보호/후속 정리로 남깁니다. subset 결과를 전체 Attempt/Session 종료로 확대하지 않습니다. 새 STOP 제어 operation의 수락/정리 단계와 대상 옛 PCM operation의 접근/출력 종료는 별개입니다. STOP/abort ACK·IRQ disable·현재 idle은 과거 무출력·잔류 출력 종료·안전 반환의 충분조건이 아닙니다.

정리 요구가 수락/진행 중이고 실제 정리가 아직 확인되지 않았다면 CLEANUP_PENDING을 유지합니다. 부분효과 확인 불가는 EFFECT_UNCERTAIN으로 드러내며, STOP 수락을 START의 실제 활성화나 전체 정리 완료로 해석하지 않습니다.

<a id="registration"></a>
## 8. 원등록 보호 후 하위 효력

필요한 command/Meta·실제 samples/target key 참조와 후속 사실의 제한된 보호 공간을 먼저 확보합니다. 실제 개별 operation의 operation/attempt/scope/handoff와 당시에 적용된 binding을 보호하고, HAL Boundary가 불변 registration/operation/attempt/pcm/binding 귀속을 **retain/arm/제출보다 먼저** 보호합니다. PCM 작업만 pcm 참조가 있으며 제어는 없습니다. 보호 실패가 효력 전이면 신규 거부하고, 생성된 실제 operation이 있다면 그 수명/무접근 확인 범위에 맞게 추적합니다.

하위 호출이 반환하기 전에 즉시 callback이 와도 원등록을 읽을 수 있어야 합니다. ISR은 raw 사실·원등록·발생/포착 시각·coverage/손실을 짧게 보호하고 기존 후속 처리를 알립니다. ISR에서 AudioTx_Request/AudioTx_Advance·decode/refill·상위 상태 적용을 실행하지 않습니다. Request는 vendor 동기 반환의 실제 수용/효과만 반영하며 raw 후속의 수집/장치 사실 적용은 Advance에 맡깁니다.

vendor 지속 DMA/SAI 스트림은 Attempt 경계를 넘을 수 있습니다. 각 START마다 vendor stop/start를 강제하거나 software registration과 vendor 등록을 1:1로 가정하지 않습니다. 지속 raw와 개별 operation/PCM 회차의 실제 대응이 부족하면 부족/손실을 유지하며 최신 Attempt를 보충하지 않습니다. 실제 등록·callback signature·실행 직렬화·참조 회수는 Binding TBD입니다.

<a id="outputs"></a>
## 9. scalar와 독립 세 OUT

| VssTxRequestState | 이 호출이 표현하는 범위 |
|---|---|
| VSS_TX_SUBMITTED_OR_QUEUED | 실제 요구 제출/대기. 활성화·actual·완료는 미확인 |
| VSS_TX_ACCEPTED_AND_ENABLED | 원 요구의 수락과 실제 활성화 근거. actual은 별도 |
| VSS_TX_REJECTED_WITH_NO_ACCESS | 해당 요구의 명확한 무접근/접근 종료가 충분히 확인된 범위. 생성 전 신규 거부는 typed 작업 기록 없음 |
| VSS_TX_EFFECT_UNCERTAIN | 부분효과/미래 접근·출력 여부 확인 불가. 원래 참조 유지 |
| VSS_TX_CLEANUP_PENDING | 원 작업 정리 요구/진행 중이며 실제 종료는 대기 |

| OUT | 내용·소비·보호 |
|---|---|
| requests | 실제 생성된 operation/attempt/state의 VssTxRequestResult. STREAM 반영/보호 추적까지 원래 작업과 독립 불변 사실 유지. |
| returned | 실제 PCM operation의 operation/key/noFutureAccess/basis인 VssPcmReturnEvidence. 아래 충분조건 충족 때만 생성. STREAM이 정확한 회차에 적용. |
| diagnostics | 원 producer/stage/cause/target/configuration/operation(확인 때만)/occurredAt. HEALTH 반영/보호 추적까지 유지. Fault 분류·회복 허용·Network 송신은 하지 않음. |

각 OUT의 storage 유효성을 **capacity/written을 읽거나 쓰기 전에** 검증합니다. 유효한 군만 written=0으로 시작하고 유효 범위 안에서 개별 count를 증가시킵니다. 요청군이 무효여도 진단군이 유효하면 전달 가능하고, returned가 가득 차도 다른 군을 일괄 중지하지 않습니다. 한 군의 마지막 슬롯으로 다른 종류 사실을 덮지 않습니다. count=0은 그 호출에서 전달된 사실 없음이지 작업·정리 완료가 아닙니다.

불가/가득 찬 군의 미전달 사실과 참조는 owner의 제한된 보호 공간에 남겨 Advance 후속으로 전달합니다. 새 효력 전에 필요한 보호 공간이 없으면 신규 수용을 제한합니다. 기존 사실을 임의 폐기/eviction하거나 무제한 pool을 가정하지 않습니다. 이미 받은 사실의 부족/손실은 진단·불확실성으로 보존하며 성공을 합성하지 않습니다. 전달 written은 소비자 반영 ACK가 아니며, 실제 반영 또는 보호된 추적 종료까지 수명을 유지합니다.

Request의 세 군과 Advance의 다섯 군은 같은 원 operation/불변 사실의 인과를 공유합니다. 이미 즉시 전달·보호 추적 중인 **동일 사실**을 Advance가 새 사실로 중복 생성하지 않습니다. 후속 상태 변화/새 장치 사실은 별개로 보존합니다. 실제 전달/반영 추적 방법은 R6-04 등 기존 TBD이며 새 public ack API를 정의하지 않습니다.

<a id="returned"></a>
## 10. 안전 반환의 충분조건

작업 생성 뒤 실제 PCM operation의 명확한 무접근 거부/접근 종료를 확인한 경우에만 그 원 operation/key로 returned를 만들 수 있습니다. 모든 해당 HW·대기·처리 중 참조 종료와 옛 내용 재소비/재접근 차단을 확인하고, 원래 scope·기간·binding의 basis.validity=VSS_COVERAGE_SUFFICIENT 및 noFutureAccess=true가 필요합니다. 생성 전 거부에는 operation이 없으므로 반환 기록이 없습니다.

제어 operation은 handoff=NULL이므로 그 operation의 PCM 반환을 만들지 않습니다. STOP 대상 옛 PCM의 후속 반환은 옛 실제 PCM operation/key로 Advance가 추적합니다. 해당 옛 회차의 반환은 같은 물리 주소의 차기 회차 접근 영구 금지도, STREAM 현재 safe-write 허가도 아닙니다. 소비 IRQ·abort 반환·HAL 등록 폐기·출력 종료·retirement는 각각 반환 충분조건을 대신하지 않습니다.

<a id="late"></a>
## 11. late·부족·손실

늦은 수용이 actual·이미 반영한 종료를 되돌리지 않습니다. 최종 전 같은 원 Attempt의 적법한 late actual은 원래 사실/시각으로 후속 적용하되 STOPPING/ABORTING을 유지하고 ACTIVE·cue 생산을 재개하지 않습니다. uncertain 최종 뒤/retired 문맥은 옛 정리·진단·참조 종료에만 적용합니다. 새 Session/PCM/ready/Fault를 정상화하지 않습니다.

VSS_COVERAGE_INSUFFICIENT는 필요한 관측/상관 근거 부족, VSS_COVERAGE_LOST는 귀속·시간·저장 누락/overflow 등의 손실입니다. 원 key가 없으면 현재 key를 채우지 않고, 원 사건 시각이 없으면 now를 실제 시각으로 만들지 않습니다. 부족/손실·저장 공간 제한을 confirmed actual/no-start/termination/반환으로 바꾸지 않습니다. 진행 제한과 보호된 후속이 유지됩니다.

<a id="feasibility"></a>
## 12. 구현 가능 부분과 종속 Gate

| 대상·근거 | 분류 | 지금 작성 가능한 논리·후속 비구동 후보 | 차단된 종속 부분·필요 결정 |
|---|---|---|---|
| R6 typed 입력/OUT·R3 identity | F1_LOCKED_SEMANTIC / F3_PARTIAL_INDEPENDENT | 배타·범위·실제 key 조회·독립 OUT·중복/부분효과/late 보호. 후속 Host 후보: 0/2 입력·overflow·같은 A 새 회차·용량별 분기 | 실제 Header/ABI·key lookup/alias·bounded storage·직렬화는 owner/Binding 결정 |
| R4 최초 One-shot 시간 | F2_PROVISIONAL_TESTABLE | 원발생→현재 제출 gate와 원발생→actual을 분리. 후속 경계 후보: <L/정확히L/시간 연속성 부족, 정상 cue 재gate 없음 | 잠정 L의 제품값·원 source 시간/actual 관측 상관 입증 |
| R4 Buffer·R5 STOP/return | F1 / F3 / F5_PHYSICAL_SAFETY_GATE | 원 targets·STREAM 선차단·safe-return 불충분/손실·정리 후속 분기. 후속 비구동 후보: subset STOP·즉시 raw·OUT full | 지속 vendor와 개별 operation 대응, FIFO/frame 잔류·no-output/종료·현재 DMA safe-write/WCET·메모리 조건은 실제 source/image 및 보드 근거 필요 |

후보를 전면 HOLD로 덮지 않으며 실제 코드 재사용 symbol은 미조사/미확정입니다. 잠정값 변경 위치는 기존 Timing/제품 owner, 구체 API·저장 수단은 Binding입니다. 후속 코딩 트랙은 위 독립 논리를 우선 후보로 삼되 별도 지시·범위·Gate 이후에만 착수합니다. 제품·Host/보드 시험 성공을 주장하지 않습니다.

<a id="references"></a>
## 13. 원문과 연결

- [R2 AudioTx_Request Caller/Callee](../../../../design/30_FUNCTIONS/50_DRIVER_HAL_FUNCTIONS.md#audiotx-request)
- [R3 TX operation·typed 자료](../../../../design/40_DATA/60_AUDIO_TX_DATA.md#vsstxoperation) · [STREAM 회차/usage](../../../../design/40_DATA/50_AUDIO_STREAM_DATA.md#vsspcmusage) · [진단](../../../../design/40_DATA/70_DIAGNOSTIC_DATA.md#vssdiagnosticevidence)
- [R4 소유·참조 보호](../../../../design/50_CONTRACTS/10_OWNERSHIP_LIFETIME.md#borrow-and-protect) · [Async 부분효과](../../../../design/50_CONTRACTS/20_ASYNC_RESULT_EVIDENCE.md#partial-and-uncertain) · [최초 gate](../../../../design/50_CONTRACTS/30_TIMING_FRESHNESS.md#first-start) · [PCM Buffer](../../../../design/50_CONTRACTS/50_AUDIO_BUFFER_TRANSPORT.md)
- [R5 Request](../../../../design/60_PSEUDOCODE/40_AUDIO_PREPARE_TX_FLOW.md#audiotx-request) · [STOP 선차단](../../../../design/60_PSEUDOCODE/50_STOP_LATE_UNCERTAIN_FLOW.md#stream-stop) · [TX 반환](../../../../design/60_PSEUDOCODE/50_STOP_LATE_UNCERTAIN_FLOW.md#tx-safe-return)
- [R6 typed TX](../../../../design/70_C_INTERFACE/00_HEADER_OWNERSHIP_MAP.md#typed-tx) · [STOP scope](../../../../design/70_C_INTERFACE/00_HEADER_OWNERSHIP_MAP.md#stop-scope) · [TBD](../../../../design/70_C_INTERFACE/00_HEADER_OWNERSHIP_MAP.md#tbd-register)
- [승인 AudioTx_Advance 설명](AudioTx_Advance.md#outputs) · [승인 의사코드](../pseudocode/AudioTx_Advance.md#procedure)
