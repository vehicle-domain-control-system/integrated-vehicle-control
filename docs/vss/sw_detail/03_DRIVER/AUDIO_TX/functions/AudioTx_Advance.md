# AudioTx_Advance — 원 TX 사실의 후속 적용

[VSS 지도](../../../01_VSS_MAP.md) · [DRIVER Layer](../../00_LAYER_OVERVIEW.md) · [AUDIO_TX Module](../00_MODULE_OVERVIEW.md) · [한국어 의사코드](../pseudocode/AudioTx_Advance.md)

> GUIDE · CORE-14 · VSD-D2 `SELF_CHECKED` / `GD2_PENDING`. 의미 정본 `0641b2c2268cf47450aeb50302638a45a7a3277f`, 작성 출발 `VSS@48bb000456fb529ca04ab34b05cdcb1ebdda759c`. Host/Build/Flash/Board `NOT_RUN`.

HAL이 사전 귀속으로 보존한 raw 사실을 원 TX operation·Attempt·PCM 회차에 적용하고, AUDIO_STREAM에 **요청 결과·출력 근거·소비·안전 반환·진단**을 독립 전달합니다. 장치 진행 사실의 해석과 보호가 목적이며, callback 횟수를 스피커 출력이나 Session 완료로 바꾸는 함수가 아닙니다.

<a id="contract"></a>
## 최신 논리 계약과 진입

**R6 logical declarations != compile-ready C**. [R6 CORE-14](../../../../design/70_C_INTERFACE/00_HEADER_OWNERSHIP_MAP.md#r6-audiotx-advance)의 선언을 그대로 읽습니다.

```text
void AudioTx_Advance(
    IN const VssTimeEvidence *time,
    OUT VssTxRequestResult requests[capacity -> written],
    OUT VssTxOutputEvidence outputs[capacity -> written],
    OUT VssPcmConsumptionEvidence consumed[capacity -> written],
    OUT VssPcmReturnEvidence returned[capacity -> written],
    OUT VssDiagnosticEvidence diagnostics[capacity -> written]);
```

`IN/OUT`와 `[capacity -> written]`은 논리 경계 표기입니다. 실제 배열 표현·capacity/written 전달 수단·Header·ABI는 미정입니다. [R2](../../../../design/30_FUNCTIONS/50_DRIVER_HAL_FUNCTIONS.md#audiotx-advance)의 `VssTxProgress AudioTx_Advance(const VssTimeEvidence *time)`은 과거 provisional 원형이며, [R3 SPLIT](../../../../design/40_DATA/60_AUDIO_TX_DATA.md)의 5군을 합치는 근거가 아닙니다. `void` 또는 `written=0`은 성공·장치 비움·보호 해제를 뜻하지 않습니다.

Caller는 `AudioStream_Advance`입니다. Callee는 필요한 HAL의 보호된 원사실 읽기·동일 operation의 하위 후속 처리와 RUNTIME의 시간 비교/후속 기회 경계입니다. `AudioHAL_Callback`은 ISR에서 이 Core를 역호출하지 않습니다. 원 TX 의미 해석은 후속 Driver 처리에서 하고 ISR은 원등록 raw·원 시각/coverage·손실을 짧게 보존합니다.

<a id="ownership"></a>
## Owner와 보호된 입력

| 경계 | 쓰는 원본 / 허용 동작 | 이 함수에 남는 보호 의무 |
|---|---|---|
| AUDIO_TX | `VssTxOperation`의 요청·장치 접근/출력 가능성, 실제 근거의 원 작업 적용, 5군 결과 생산 | 효력 전 확보된 원 operation/handoff·결과 및 내부 참조를 후속 적용까지 보호 |
| HAL/BSP | `VssHalTxRegistration`, `VssRawObservation`의 원등록·발생/포착 시각·범위·누락 | TX 결과와 별도로 HW/대기/처리 참조·late 대응 종료까지 원등록 보호 |
| AUDIO_STREAM | PCM A/B의 cycle/usage·provider 생산·정리, 결과의 원키 적용 | TX가 PCM 내용·CPU usage·provider 권한을 직접 변경하지 않음 |
| PLAYBACK / HEALTH | 전체 Session·Attempt 최종 / Fault 분류·제한·복구 | TX가 완료/Started/retired나 Fault를 직접 작성하지 않음 |
| RUNTIME | 기존 시간 비교·연속성과 후속 처리 기회 | 새 Task/Queue/timeout·시간 manager를 만들지 않음 |

`time`은 이번 후속 기회의 읽기 전용 `VssTimeEvidence`입니다. raw의 `occurredAt`/`capturedAt`, activation 원 시각, coverage의 interval/binding을 처리 시각으로 대체하지 않습니다. 현재 시간의 비교가 부족하면 시간 의존 진행을 제한하고 진단/보호를 유지합니다. 이미 원 시간 문맥이 보호된 독립 사실까지 버리거나 현재 시각으로 다시 찍지 않습니다.

원 raw와 TX 장치 기록은 OUT 배열이 아닌 owner의 보호된 내부 입력입니다. `VssHalTxRegistration`은 개별 software 작업의 불변 대응이며 vendor의 지속 SAI/DMA 등록과 수명이 다릅니다. 하나의 지속 stream이 여러 Attempt를 가로지를 수 있습니다. 개별 대응은 기존 사전 기록과 실제 관측 범위로 입증해야 하며, 최신 Attempt·A/B 주소·IRQ phase를 조회해 원키를 합성하지 않습니다.

<a id="outputs"></a>
## Time IN과 OUT 5종의 독립 계약

각 행의 `written` 증가는 **유효한 해당 타입의 불변 사실을 원귀속·내부 참조까지 보호하여 해당 OUT에 기록한 뒤에만** 가능합니다. 하나의 처리 기회에 requests/consumed/diagnostics가 함께 생기면 세 군에 각각 남깁니다. 동일 군의 여러 사실도 한 자리를 덮어쓰지 않습니다.

| IN/OUT / R3 타입 | 생산 조건과 원귀속 | written 조건 / 부족 시 처리 | 소비·해제 경계 |
|---|---|---|---|
| IN `VssTimeEvidence` | 이번 처리 기회의 시간·연속성 읽기; 원사실 시각과 구별 | OUT가 아님. 비교 불가면 시간 의존 확정 금지, 원 근거/진단 보존 | 동기 borrow; 이후 필요한 원 시간 참조는 owner가 별도로 보호 |
| requests / [`VssTxRequestResult`](../../../../design/40_DATA/60_AUDIO_TX_DATA.md#vsstxrequestresult) | 실제 존재하는 원 `operation`, `attempt`와 요청 state 근거. 제출/접수·활성화·명확한 무접근 거부·불확실/정리 분리 | 해당 state 입증·군 용량 확보 후 기록. 원 작업 자체가 불명이면 fake operation 결과 대신 raw/귀속 부족 진단 | STREAM 반영 또는 보호 추적까지 유지. 늦은 acceptance로 이미 적용된 actual/end를 되돌리지 않음 |
| outputs / [`VssTxOutputEvidence`](../../../../design/40_DATA/60_AUDIO_TX_DATA.md#vsstxoutputevidence) | 원 `operation`, `attempt`, `scope`, `occurredAt`, `coverage`에 한정한 독립 출력 fact kind | kind별 충분조건 충족 후 기록. 부족/손실은 원귀속 가능한 `VSS_TX_OUTPUT_UNCERTAIN`와 원 coverage/진단; 확정 합성 없음 | STREAM 의미 반영 또는 보호 추적까지 참조 보호. 상위 전체 outcome·retirement는 별도 결합 |
| consumed / [`VssPcmConsumptionEvidence`](../../../../design/40_DATA/60_AUDIO_TX_DATA.md#vsspcmconsumptionevidence) | 실제 소비가 확인된 원 `operation`과 key의 Attempt/A·B/cycle, `firstFrame`, `frameCount`, 원 `occurredAt` | 원 handoff의 확인된 유효 구간 안에서만 기록. 전체 buffer capacity로 확장 금지. 미확인 소비는 진단/retain | 원 회차 소비 적용/보호 추적까지 유지. cyclic 재접근 가능하면 device retain 계속 |
| returned / [`VssPcmReturnEvidence`](../../../../design/40_DATA/60_AUDIO_TX_DATA.md#vsspcmreturnevidence) | 원 `operation`, 정확한 `key`, `noFutureAccess=true`, 충분한 `basis`. 옛 내용에 대한 HW/대기/처리 참조 종료와 재접근 차단 입증 | 입증+용량 후 기록. 부족이면 **반환 record 없음**, 원 cycle/장치 보호·정리·진단 유지. handoff=null 제어에는 PCM 반환 없음 | STREAM의 같은 key 적용·usage 회수까지 보호. old A 반환을 new A/B 해제로 사용하지 않음 |
| diagnostics / [`VssDiagnosticEvidence`](../../../../design/40_DATA/70_DIAGNOSTIC_DATA.md#vssdiagnosticevidence) | 원 producer/stage/cause/target/configuration/occurredAt. operation은 R3의 optional 원귀속 참조 | 실제 확인된 발견 사실과 필수 문맥을 보호해 기록. 필수 문맥 부족이면 owner raw/손실 관측으로 보존하고 현재 값으로 채우지 않음 | STREAM/FLOW 경유 HEALTH 반영/보호 추적까지 유지. Fault 분류는 HEALTH 책임 |

requests의 state는 기존 `VSS_TX_SUBMITTED_OR_QUEUED`, `VSS_TX_ACCEPTED_AND_ENABLED`, `VSS_TX_REJECTED_WITH_NO_ACCESS`, `VSS_TX_EFFECT_UNCERTAIN`, `VSS_TX_CLEANUP_PENDING`만 사용합니다. PCM 인계 acceptance를 START activation으로 승격하지 않습니다. 부분 효력이 있으면 무접근 거부가 아니며 기존 보호를 유지합니다.

<a id="sufficiency"></a>
## 인정 가능한 사실과 아직 입증되지 않은 물리 의미

[R3 kind](../../../../design/40_DATA/60_AUDIO_TX_DATA.md#vsstxoutputfactkind)와 [R4 충분조건](../../../../design/50_CONTRACTS/20_ASYNC_RESULT_EVIDENCE.md#confirmed-outcomes)을 함께 적용합니다. 과거 무출력·미래 차단·잔류 종료는 독립 사실로 전부 보존하며 하나만 선택해 버리지 않습니다.

| 원 device fact / 조건 | 허용되는 TX 논리 근거 | 이 사실만으로 입증되지 않는 주장 |
|---|---|---|
| 원 request와 **실제 activation 이후 새** 출력 경계 관측, 원 시각/연속성·scope/coverage·binding 충분 | `VSS_TX_NEW_OUTPUT_OBSERVED` | 상위 권한·최초 actual age·whole Session 결합을 생략한 Started |
| 필요한 과거 전체 구간의 실제 무출력 충분 관측 | `VSS_TX_PAST_NO_OUTPUT_PROVEN` | 미래 요청/descriptor/FIFO 출력 불가, 상위 `NO_START_CONFIRMED` |
| 옛 request/active work/descriptor 및 잔류 FIFO/frame의 미래 출력 가능성 차단 입증 | `VSS_TX_FUTURE_OUTPUT_BLOCKED` | 과거 무출력 또는 당시 잔류 출력 종료 |
| 해당 scope의 실제 장치·잔류 출력 종료 입증 | `VSS_TX_OUTPUT_ENDED` | 상위 미래 PCM/cue 금지·전체 `OUTPUT_TERMINATION_CONFIRMED`·retirement |
| callback/HALF/MAJOR, descriptor 진행, software flag, `Sai_Ip_Send` 호출 | 대응/구간이 입증된 범위의 요청·소비 또는 raw 참고 사실 | 새 actual, 물리 no-start/end, 안전 반환·현재 CPU 쓰기·STOP 완료 |
| mute/zero-fill/abort ack/현재 idle | 실제 확인한 제어 효과 또는 정리 진행 | 과거 무출력·미래 전체 차단·잔류 종료·whole retirement |
| 물리 상관/원 시간/coverage 부족, 또는 명시적 누락·overflow | `INSUFFICIENT` 또는 `LOST`를 보존한 불확실 출력/진단 | confirmed로 자동 승격 또는 '안 보였으니 무출력' |

`VSS_COVERAGE_INSUFFICIENT`는 범위/상관/비교 근거 부족이고 `VSS_COVERAGE_LOST`는 실제 누락·overflow·연속성 손실입니다. 미관측만으로 손실을 단정하지도, 손실을 단순 부족으로 감추지도 않습니다. 특정 원 사실의 충분성이 입증된 범위는 진행하되 다른 범위의 부족을 없애지 않습니다.

<a id="pcm-stop-late"></a>
## PCM 회차·STOP·late의 적용 경계

**source consumed ≠ PCM cycle safe return ≠ current CPU writable ≠ actual end ≠ whole retirement**입니다. HALF 뒤 A를 재방문할 수 있으면 소비가 끝난 구간에도 접근 위험이 남습니다. 옛 A 회차의 충분한 `noFutureAccess`는 그 옛 작업/내용에만 유효하고, 주소의 다음 회차 접근을 영구 금지하는 의미가 아닙니다. 현재 CPU write는 STREAM이 원키·반환 적용 유효성·다음 DMA 방문 전 구간·refill WCET/IRQ 지연·메모리/cache 조건을 다시 확인해 결정합니다. TX가 `VSS_PCM_CPU_WRITABLE`을 쓰거나 provider 생산을 재개하지 않습니다.

STOP 요청의 제어 operation 완료와 **STOP 대상의 모든 옛 작업/PCM/장치 잔류**는 다릅니다. 일부 범위 차단·정리가 확인돼도 나머지의 deviceAccessPossible/futureOutputPossible·결과·late 대응 보호는 남습니다. 출력과 PCM 접근 종료 근거를 각 군으로 보존하며 mute/zero-fill/abort 반환만으로 해제하지 않습니다. TX의 scope에는 상위 future cue/생산 금지 전체가 포함되지 않습니다.

늦은 callback은 사전 원등록으로 old operation/cycle에만 적용합니다. 대응이 불명이면 격리/귀속 부족 진단과 raw 보호를 유지하고 현재 Session에 붙이지 않습니다. 원 uncertain 최종/retired 이후 사실은 옛 정리·참조 회수·진단에 한정하고 정상 Pending/Started/Completed나 새 A/B를 복원하지 않습니다. 최종 전 늦은 actual의 상위 적법성 판단은 PLAYBACK이 담당하며 STOPPING/ABORTING을 해제하지 않습니다.

<a id="capacity-lifetime"></a>
## 군별 capacity와 인계 수명

매 호출의 5개 written은 각각 0에서 시작하고 자기 capacity만 사용합니다. `capacity>0`이면 해당 타입의 유효한 caller 출력 공간·범위가 필요하며 written 전달 공간도 실제 ABI에서 확인해야 합니다. 잘못된 공간/범위에는 쓰지 않고 원사실과 해당 실패 진단/후속 보호를 유지합니다. capacity=0/부족 군의 미인계 사실도 producer의 기존 bounded 보호 경계에 남깁니다. 다른 군의 공간이 있어도 타입을 바꾸거나 한 Progress/global 슬롯으로 합치지 않습니다. 가능한 군은 독립적으로 제출합니다.

기존 보호 공간도 부족하면 실제 누락/보존 실패를 별도의 observable `LOST`/진단 근거로 남기고 위험을 늘릴 신규 진행을 제한합니다. accepted 원 operation·cycle·late 방어를 지워 신규 자리를 만들지 않으며 무한 queue나 구체 용량·반복 횟수·retry를 정하지 않습니다. loss 관측 자체의 수단/용량도 실제 Binding에서 확인할 대상입니다.

OUT에 기록한 사실도 **소비 완료 ack가 아닙니다**. requests/outputs/consumed는 STREAM 반영/보호 추적까지, returned는 같은 key에 적용하여 usage를 회수할 때까지, diagnostics는 원 진단 반영/보호 추적까지 필요한 내부 참조를 유지합니다. 단순 구조체 얕은 복사나 void 반환으로 operation/handoff/coverage/raw registration을 폐기하지 않습니다. TX operation, HAL 불변 기록, 지속 vendor 등록의 독립 폐기 조건을 각각 확인합니다.

<a id="source"></a>
## 실제 소스 근거 — 2026-10-07 브링업 참고

정적 참조일은 **2026-10-12 KST**, 원본 스냅샷은 2026-10-07입니다. 설치 출처는 ignored 로컬 `.vss_work/reference/bringup_20261007/`의 `03_EVIDENCE_MANIFEST.json`, `04_SELECTED_FILES.csv`, `SELECTED_SHA256SUMS.txt`입니다. 아래 path는 **코드 표기**이며 GitHub에서 접근 불가능한 Markdown 링크가 아닙니다. D2에서 108개 원본의 출처 SHA를 대조하고 다음 행을 정적으로 읽었습니다. 설치 PASS와 과거 5분 보드 기록은 현재 물리 PASS가 아닙니다.

| 실제 파일·행 / SHA-256 | 읽어 확인한 내용 | 재사용 후보와 추가 확인 지점 |
|---|---|---|
| `src/AudioDmaTransport.c:196–239,272–379,389–488` / `469122809530bea8cd0018e8a7f32c1860399e36a64a00628b453b12bbc9a76e` | Start의 지속 `Sai_Ip_Send` 한 경로·started flag, Service의 A/B free/active 재확인·refill, DMA IRQ의 software phase와 HALF/MAJOR counter, SAI event/error 기록 | TX/짧은 raw 경계 우선 후보. 최종 operation/Attempt/cycle·불변 원시각·물리 coverage 기록은 여기서 확인되지 않음 |
| `src/AudioDmaTransport.h:25–34` / `92a60d223356ec924dfff5d04be0efb8e5f199a3347491a4bd768710` | Prime/Init/Start/Service와 boolean readiness/error 공개 경계 | vendor 중립 기존 경계 재사용. R6 typed OUT ABI가 구현됐다는 근거 아님 |
| `src/AudioPlayer.c:142–179,359–415` / `14a6d36dc160351cdec1a573ae785ec61a2f5cdd5a098cd7da3fa7bb112aeed5` | Begin/Stop flag, terminal/silent buffer의 OnBufferComplete/Start에 따른 완료·audibly-stopped flag | STREAM PCM lifecycle 참고. 단일 software flag는 물리 종료·새 회차 쓰기 근거 아님 |
| `RTD/src/Sai_Ip.c:4205–4258` / `321c8aa4776eeef397d55ec4696b7433ce444471f0440bfb86bfd653bcce5ca8` | `void Sai_Ip_Send`가 DMA/interrupt 경로를 선택하고 TxEnable의 TE를 활성화 | 반환 성공 boolean이 없음. 함수 호출/TE enable과 원 작업의 실제 출력 관측은 다름 |
| `generate/src/Dma_Ip_Sa_VS_0_PBcfg.c:182`, `generate/src/Sai_Ip_Sa_VS_0_PBcfg.c:183` / 각각 `48337239694c6563fb170f8e60c92849449f5cffc7323ae2611fb43c92e24fe0`, `d8796cdb8ccac3166734d810e557d8c42b363e61595503c89f848a814f956ccd` | 기존 DMA/SAI callback symbol의 생성 설정 연결 | 등록 ABI·IRQ acknowledge·descriptor postcondition·원키 상관은 RTD/config/HW 추가 확인; 생성 파일 수정 없음 |

`RTD/src/Dma_Ip.c`, `RTD/src/Dma_Ip_Irq.c`는 설치 목록의 정적 참고이며 실물 동작을 실행하지 않았습니다. 읽은 `src/*.c/.h`에서 최종 `AudioTx_Advance`/`Store_ApplyInput` symbol은 발견되지 않았습니다. 새 Core 이름을 위해 기존 Transport와 중복 wrapper/module을 선생성하지 않습니다.

확인 수준은 위 source 행의 **정적 `SOURCE_CONFIRMED`**, 동결 의미의 `LOGICAL_APPROVED`, 실제 ABI/저장·상관의 `BINDING_TBD`, 물리 출력/반환의 `HW_UNVERIFIED`로 구분합니다. 외부 IF는 `PROPOSED_IF`이며 문서 작성 `SELF_CHECKED`가 이 상태들을 TESTED나 제품 승인으로 바꾸지 않습니다.

<a id="feasibility"></a>
## 바로 검증 가능한 후보와 활성화 Gate

| 후보 / D0 연결 | 입력 fixture와 확인할 결과 | 이후 실증·결정 owner |
|---|---|---|
| F1/F3 원키·late 상관 | 보호된 원등록/old operation/A·B cycle, 새 Attempt 도중 old raw → old만 적용; 불명 원키는 격리 | 기존 TX/HAL 경계; raw 상관 방식/최종 ABI는 Binding TBD |
| F1/F3 5군·bounded 인계 | 같은 기회에 request+consumed+diagnostic, 각 capacity=0/부족 → 독립 written/retain/loss, 기존 보호 불변 | TX/STREAM의 실제 저장·직렬화 수단/용량 확인 |
| F1/F3 scope·time·coverage | 실제 원 interval/binding 충분/부족/손실 fixture → kind 분리, insufficient/lost로 confirmed 생성 금지 | 물리 관측 충분조건·clock 상관은 F5 HW/RTD 실증 |
| F1/F3 PCM old 회차 | old A 소비만/충분한 old 반환/새 A key → 소비·반환·현재 쓰기 구별, 반환으로 생산 재개 없음 | STREAM 현재 쓰기 구간·WCET·IRQ/cache·다음 방문은 F5 실증 |
| F1/F3 STOP·uncertain | 일부 옛 범위만 차단, late 뒤 원 최종 → cleanup/retain/diagnostic, 새 Session 복원 없음 | PLAYBACK/STREAM whole 결합과 실제 FIFO/frame 종료는 F5 실증 |

위 내용은 **구체적 SW/Host 검증 후보이며 `NOT_RUN`**입니다. 부족한 HW 근거는 해당 물리 확정/제품 활성화에 한정합니다. 원키 분류·5군 보호·loss/late 논리까지 전체 HOLD하지 않습니다. 후보가 원 계약과 불일치하거나 출처/구성이 개정되면 해당 채택만 중단하고 기존 operation/cycle·미인계 결과·late/장치 보호를 유지합니다. TX/HAL/STREAM owner의 새 근거와 별도 구현 승인으로 재채택하며 임의 대체 수치로 안전 조건을 우회하지 않습니다. 이번 작업은 문서 2쌍 시범이며 fixture 실행/실제 코드 Mapping은 별도 사용자 지시 대상입니다.

R6 TBD-01~08, Q01~14, P1B_C1의 9 Core/16 연결 미완료, 외부 Network IF/source/image/실제 PCM DMA·STOP·출력 Gate, WINDOW/Anti-Pinch `[구현 보류 — 설계 유지]`, P2 HOLD를 보존합니다. 실제 용량·binding·성능 숫자·새 Task/Queue/API/Flag/Fault·세 번째 PCM buffer를 확정하지 않습니다.

<a id="references"></a>
## 정본과 왕복 경로

- [R2 TX Caller/Callee·후속 계약](../../../../design/30_FUNCTIONS/50_DRIVER_HAL_FUNCTIONS.md#audiotx-advance), [R3 원등록](../../../../design/40_DATA/60_AUDIO_TX_DATA.md#vsshaltxregistration), [R3 PCM cycle/usage](../../../../design/40_DATA/50_AUDIO_STREAM_DATA.md#vsspcmcyclekey)
- [R4 ownership/lifetime](../../../../design/50_CONTRACTS/10_OWNERSHIP_LIFETIME.md), [R4 continuous stream / typed lanes](../../../../design/50_CONTRACTS/50_AUDIO_BUFFER_TRANSPORT.md#continuous-stream), [R4 네 경계](../../../../design/50_CONTRACTS/50_AUDIO_BUFFER_TRANSPORT.md#four-boundaries)
- [R5 TX raw 해석](../../../../design/60_PSEUDOCODE/40_AUDIO_PREPARE_TX_FLOW.md#audiotx-advance), [R5 TX 안전 반환](../../../../design/60_PSEUDOCODE/50_STOP_LATE_UNCERTAIN_FLOW.md#tx-safe-return), [R5 STREAM 현재 쓰기](../../../../design/60_PSEUDOCODE/50_STOP_LATE_UNCERTAIN_FLOW.md#stream-safe-return)
- [Binding 후보](../../../../design/90_BINDING/90_C_FILE_API_MAPPING.md#transport), [기존 Implementation TBD](../../../../design/90_BINDING/91_IMPLEMENTATION_TBD.md), [의사코드의 T1~T9 분기](../pseudocode/AudioTx_Advance.md#procedure)
