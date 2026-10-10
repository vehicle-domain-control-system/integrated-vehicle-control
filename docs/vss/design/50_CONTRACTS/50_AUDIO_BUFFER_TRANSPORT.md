# Audio Buffer / Transport — A/B 사용 회차와 실제 접근·출력 경계

> R4 · 2026-10-08 · R3-C1 세 수정 및 AudioTx_Request 독립 국소 PASS 유지

[Contract Overview](00_CONTRACT_OVERVIEW.md#buffer)

PCM 사용권과 장치 접근 보호를 원래 Attempt/Buffer/회차에 연결한다. AUDIO STREAM과 AUDIO TX는 서로 다른 원본을 소유한다. 공통 충분조건을 아래에 모으되 실제 vendor 방식·메모리 보장·차기 방문 기한은 입증된 것으로 취급하지 않는다.

<a id="pcm-boundary"></a>
## 1. 전송 PCM은 정확히 A와 B 두 개다

기존 `VSS_PCM_A` / `VSS_PCM_B`만 사용한다. 제3 PCM 버퍼·pool·silence 전용 타입·새 Queue/Task를 도입하지 않는다. ASSET 압축 bytes와 decoder의 기존 제한 scratch는 별도 자료이며 추가 전송 PCM 버퍼가 아니다. AUDIO STREAM은 CPU 접근/usage·refill을, AUDIO TX는 장치 retain·접근/출력 가능성과 원래 operation을 소유한다.

<a id="valid-and-silence"></a>
## 2. 논리 유효 PCM과 물리 무음 처리 — C1-01

`validFrameCount > 0`인 안정된 samples·유효 범위·format만 `VssPcmHandoffCommand`로 넘긴다. `validFrameCount == 0`은 새 유효 사운드 PCM 인계를 금지한다는 뜻이며 지속 SAI/eDMA 물리 전송이 반드시 정지했다는 뜻은 아니다. 원래 사운드에 포함돼 실제 decode된 0 sample은 유효 PCM이다.

물리 무음 유지/stale PCM 재소비 방지를 위한 zero-fill은 기존 A/B 안에서 **실제 CPU 쓰기 안전 조건을 만족하는 범위**에만 가능하다. 유효 사운드 범위 밖을 채우는 물리 처리와 사운드의 논리 길이·반복은 다르다. 실제보다 유효 길이를 늘리거나 임의 padding/사운드 반복을 만들지 않는다.

STOP 뒤 `productionAllowed=false`는 유지한다. 필요한 물리 무음 정리는 참조 종료·현재 쓰기 안전 근거를 확인한 정리 범위에 한정하며 decode/refill/새 sound handoff의 재허가가 아니다. zero-fill 완료·현재 무음은 [Async §3](20_ASYNC_RESULT_EVIDENCE.md#confirmed-outcomes)의 실제 출력 종료 또는 [Ownership §4](10_OWNERSHIP_LIFETIME.md#references-and-release)의 retirement를 증명하지 않는다.

<a id="access-cycle"></a>
## 3. HANDOFF_PENDING부터 원래 회차를 보호한다

| 경계 | 기존 책임과 보호 조건 |
| --- | --- |
| 준비/쓰기 | AUDIO STREAM이 현재 CPU 쓰기 안전성·생산 허용을 확인. decoder 소비/produced 범위를 구별하고 인계 직전 STOP/정리 권한을 재확인 |
| 인계 직전 | 안정된 `VssPcmCycleKey`(Attempt/A 또는 B/회차)·samples·format·유효 범위를 보호하고 DRIVER 호출 **전에** usage를 `HANDOFF_PENDING`으로 변경 |
| 장치 요구 효력 전 | AUDIO TX가 불변 개별 operation/Attempt/필요 PCM 귀속·참조 보호를 확보. HAL callback도 효력 발생 전에 그 문맥을 포착할 수 있어야 함 |
| 수용/부분 효력 | 수용·장치 retain·activation·actual을 구별. 실패 반환에도 미래 접근 가능성이 남으면 옛 보호/차단/정리를 유지 |
| 소비 관측 | 원래 회차의 `VssPcmConsumptionEvidence`만 기록. source consumed는 안전 반환/실제 출력 종료가 아님 |
| 안전 반환 적용 | 같은 operation/Attempt/A 또는 B/회차의 `VssPcmReturnEvidence`와 현재 쓰기 조건을 확인해 AUDIO STREAM usage에 적용. 옛 A로 새 A/B를 해제하지 않음 |

명확한 무접근 거부는 그 원래 요구의 실제 무접근/참조 종료 근거가 있을 때만 회수한다. 인자 주소의 호출 종료는 실제 samples 참조 종료가 아니다. DRIVER raw 결과를 현재 Buffer에 사후 라벨링하지 않으며 pending/accepted/consumed/returned 사실이 마지막 Result 하나로 덮이지 않도록 기존 보호 의무를 유지한다.

<a id="safe-write"></a>
## 4. 옛 회차 종료와 차기 DMA 방문 전 쓰기 안전 — C1-03

`VssPcmReturnEvidence.noFutureAccess=true`는 **해당 옛 operation/회차의 참조 종료와 옛 내용 재접근 금지**의 충분조건이다. 같은 A/B 물리 주소를 앞으로 영원히 쓰지 않는다는 뜻이 아니다. 지속 eDMA는 주소를 재방문하므로 차기 사용과 구별한다.

옛 PCM이 남아 다시 소비될 가능성이 있으면 true를 확정하지 않는다. 회차 번호 변경만으로 물리 재소비 위험이 없어지지 않는다. 차기 방문 전 갱신을 실제로 끝낼 수 있는 안전 구간·참조/메모리 조건을 함께 확인해야 하며, 과거 ReturnEvidence나 `CPU_WRITABLE` 상태는 무기한 쓰기 허가증이 아니다. 적용 시 이미 안전 구간이 지났거나 대응이 불명확하면 그 근거로 현재 사용권을 해제하지 않는다.

HALF/MAJOR IRQ는 기존 브링업의 half 소비/교대 관측 지점이다. 단일 IRQ·abort 반환·DMA complete만으로 회차 안전 반환이나 출력 종료를 확정하지 않는다. 옛 회차의 반환과 현재 쓰기 안전 조건을 함께 만족시키는 방법이 입증되기 전에는 완료로 표시하지 않는다.

**TBD — B2-R/보드 검증:** 실제 차기 방문 안전 구간, refill 최악 실행시간·IRQ 지연/경합, TCD/ISR 대응·stale 방지·기한 경과 처리·cache/barrier/alignment 등 메모리 조건. R4는 수치·알고리즘·새 Buffer 상태를 정하지 않는다.

<a id="four-boundaries"></a>
## 5. 소비 / 안전 반환 / 출력 종료 / retirement는 다른 경계다

| 기존 경계 | 원본 / 적용 owner | 필요한 범위와 자동 추론 금지 |
| --- | --- | --- |
| source consumed | AUDIO TX 사실 → AUDIO STREAM | 원래 PCM source 소비. CPU 재쓰기·FIFO/frame 배출·실제 출력 종료를 뜻하지 않음 |
| safe return | AUDIO TX 충분 근거 → AUDIO STREAM usage | 옛 회차의 참조/옛 내용 재접근 종료와 현재 쓰기 안전. provider 생산 허가 복원·출력 종료를 뜻하지 않음 |
| output termination | TX 장치 근거 → AUDIO STREAM Backend → PLAYBACK 전체 | 해당 scope의 실제/잔류 출력 종료와 미래 출력/PCM 차단. [Async §3](20_ASYNC_RESULT_EVIDENCE.md#confirmed-outcomes)의 충분조건 적용. 한 구간 종료≠전체 Session 종료 |
| Session retirement | PLAYBACK | 전체 관련 출력 안전 끝·상위 후속 start/cue/반복 권한 차단·late 귀속 보호. provider/PCM/HAL 기록 폐기와 독립 |

옛 불변 callback 기록이 안전하게 독립 보호된 채 새 A/B 회차를 사용할 수 있는 조건과 보호 공간 부족 처리에는 [Ownership §4~5](10_OWNERSHIP_LIFETIME.md#references-and-release)를 적용한다. 출력 정지나 retirement만으로 모든 실제 자료 참조가 끝났다고 추론하지 않는다.

<a id="continuous-stream"></a>
## 6. 지속 vendor 스트림과 개별 software operation — C1-02

기존 브링업의 한 `Sai_Ip_Send` 기반 지속 SAI/eDMA 등록은 여러 Session/Attempt를 가로지를 수 있다. `VssHalTxRegistration`은 그 vendor 스트림 전체가 아니라 **개별 software TX operation의 불변 귀속 기록**이다. 해당 operation은 원래 Attempt를 가지며 PCM 작업이면 원래 handoff key를 함께 가진다. 제어 작업에는 불필요한 PCM 참조를 넣지 않는다.

vendor 등록을 Attempt마다 다시 하도록 강제하거나 지속 스트림에 가짜/최근 Attempt를 붙이지 않는다. registration key가 vendor callback token으로 제공된다고 약속하지도 않는다. raw stream 사실을 개별 operation/PCM 회차에 연결하려면 실제 boundary·시각·source·등록/처리 문맥의 대응이 입증되어야 한다.

대응 근거가 부족하면 `VSS_COVERAGE_INSUFFICIENT`, 귀속/관측 누락이면 해당 `VSS_COVERAGE_LOST`를 보존하고 confirmed 출력/안전 반환을 만들지 않는다. 현재 Attempt 조회로 보완하지 않는다. 개별 귀속 기록과 vendor 지속 등록의 보호/회수 수명은 1:1로 고정하지 않으며 HW·대기·처리 중 참조가 끝날 때까지 각각 보호한다.

**TBD — B2-R:** 연속 raw HALF/MAJOR·WSF/FEF/EOF와 개별 작업/회차의 물리 상관, 각 음향 Attempt의 실제 활성화·출력 경계, abort/drain/무출력·출력 종료 충분조건. 정상 브링업은 이러한 예외 충분조건을 입증하지 않는다.

<a id="tx-typed-lanes"></a>
## 7. AudioTx_Request의 두 typed 입력은 하나의 논리 Core다

| 기존 typed 입력 | 필요한 identity / 자료 | 보존할 차이 |
| --- | --- | --- |
| `VssPcmHandoffCommand` | 안정된 samples·유효 범위·format·원래 Attempt/A 또는 B/회차 | 사전 HANDOFF_PENDING 보호. 유효 사운드 인계는 >0. 단순 인계 수용≠START |
| `VssTxControlCommand` | 원래 Attempt·START/STOP·scope, 필요한 최초 START에만 원본 `firstStartMeta` | PCM samples/주소 없이 제어. 정상 후속/STOP에는 first-start Meta 없음 |

`VssTxOperation.handoff`는 PCM 인계 작업에만 있고 START/STOP 제어에서는 null이다. 부분 효력/실제 출력/소비/반환 사실은 각 원래 작업에 보존한다. 이 분리는 승인된 논리 입력 경로이며 Core 총수 18은 유지한다. [AudioTx_Request 국소 PASS 연결](../30_FUNCTIONS/50_DRIVER_HAL_FUNCTIONS.md#audiotx-request)에서 읽고 실제 C entry point 수·이름·signature·Header/ABI는 R6에서 결정한다.

## 8. 연결 원본과 미정

[AUDIO STREAM local usage/STOP](../20_MODULES/24_AUDIO_STREAM_MODULE.md#pcm) · [AUDIO TX local 전송/근거](../20_MODULES/30_AUDIO_TX_MODULE.md#evidence) · [A/B usage](../40_DATA/50_AUDIO_STREAM_DATA.md#vsspcmusage) · [TX handoff/return](../40_DATA/60_AUDIO_TX_DATA.md#tx) · [HAL Boundary](../10_LAYERS/details/40_HAL_BSP_BOUNDARY.md) · [Implementation TBD](../90_BINDING/91_IMPLEMENTATION_TBD.md)

PCM format/rate/word alignment·HAL/RTD signature·등록/callback 실물 대응·직렬화/보호 저장 수단·physical silence 구현은 기존 R6/B2-R/실측 TBD다. R5 의사코드, C 구현 또는 보드 시험을 이 Contract의 산출물로 작성하지 않는다.
