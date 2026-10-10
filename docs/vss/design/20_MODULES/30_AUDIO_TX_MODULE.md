# AUDIO TX Module — PCM 장치 전송

> R1 설계 의미 유지 · R1.1 한국어 표현 교정 · 2026-10-07

[Module Overview](00_MODULE_OVERVIEW.md) · [상위 Layer](../10_LAYERS/30_DRIVER_LAYER.md)

## 1. 목적

해당 버퍼 사용 회차의 PCM의 장치 접근을 보호하고 확인 범위가 명확한 전송·출력·반환 근거를 상위에 제공한다.

## 2. 역할

AUDIOTX는 장치가 PCM 버퍼에 접근할 수 있는 동안의 사용권 보호(retain), 버퍼 사용 회차·descriptor, 실제 전송/출력을 위한 장치 활성화(arm), 소비·재접근 불가·정리 근거를 관리한다. HAL이 요청 전에 저장한 귀속 정보에 연결된 사실을 원래 요청에 적용한다.

## 3. 책임

- PCM 인계부터 원래 출력 시도/operation·회차·유효 구간을 보호한다.
- 제출·수락+장치 활성화·해당 요청 이후 새로 관측된 실제 출력 후보와 소비/안전 반환을 구별해 적용한다.
- 해당 범위의 old 장치 작업·FIFO/frame 정리와 미래 접근/출력 불가 근거를 반환한다.

## 4. 책임이 아닌 것

AUDIO STREAM(C09)의 PCM 사용 상태, STORE ledger, PLAYBACK Session/retirement, HEALTH Fault/해제를 직접 변경하지 않는다. 전체 cue/반복 차단과 Session 최종 판단은 상위 책임이다. AUDIO CONTROL의 readiness를 대신 소유하지 않는다.

## 5. 소유 상태 / 데이터

해당 PCM 전송/operation·Buffer 회차·유효 구간에 대한 장치 사용권 보호/장치 활성화/descriptor 진행과 반환/정리 근거의 단일 writer는 AUDIO TX다. HAL raw 등록/capture와 AUDIO STREAM(C09)의 PCM 사용 상태 원본은 별도로 유지한다.

## 6. 입력

AUDIO STREAM의 해당 PCM 인계/start/정리 요구, HAL이 요청 전에 저장한 귀속 정보에 연결해 전달한 원본 관측 사실·시각/판정에 필요한 관측 범위·기간의 근거(coverage)/손실, 기존 RUNTIME의 시간·진행 근거를 받는다.

## 7. 출력

원래 출력 시도와 버퍼 사용 회차에 귀속된 장치 사용권 보호·수락+장치 활성화·actual 후보, consumed/safe return, 해당 장치 범위 no-start/termination 근거와 확인 부족/손실을 AUDIO STREAM에 반환한다.

## 8. 사용하는 / 제공하는 주요 Core Function — R2

[AudioTx_Request](../30_FUNCTIONS/50_DRIVER_HAL_FUNCTIONS.md#audiotx-request) · [AudioTx_Advance](../30_FUNCTIONS/50_DRIVER_HAL_FUNCTIONS.md#audiotx-advance)

기존 `AudioTx_Process`는 하위 요청 전 귀속/PCM 접근 보호와 HAL 원래 사실의 후속 적용으로 분리했다. [AudioHAL_Callback](../30_FUNCTIONS/50_DRIVER_HAL_FUNCTIONS.md#audiohal-callback)은 HAL Layer Boundary가 소유하며 AUDIO TX 함수나 가짜 HAL Module로 옮기지 않는다.

## 9. 의존 Module

AUDIO STREAM → AUDIO TX → HAL / 필요한 RUNTIME이다. AUDIO CONTROL과의 준비/전송 조율은 AUDIO STREAM이 맡으며 Driver 사이의 새 정책 의존을 만들지 않는다.

## 10. 상태 전이 또는 주요 lifecycle

PCM 인계 대기부터 해당 Buffer/회차/출력 시도/operation·유효 구간을 보호한다. PCM 등록 자체는 출력을 시작하기 위한 장치 활성화가 아니다. start 요청은 새 장치 활성화와 원래 기한을 연결하고 제출·수락+장치 활성화·실제 출력 후보를 구분한다. 소비 결과는 같은 회차에 적용하며 cyclic descriptor의 재접근 가능성이 있으면 장치 사용권 보호를 유지한다.

정리는 old request·active work·descriptor·FIFO/frame 범위를 확인해 진행한다. API 수락과 실제 효력은 별개이며 normal 구간 drain과 전체 stop/abort scope를 구별한다.

장치 접근·출력의 종료·차단 확인 조건(fence)과 해당 요청·출력 시도에 귀속된 결과 추적이 충족될 때까지 operation 문맥을 유지한다. 장치 결과 반환과 callback 등록 저장 폐기는 같은 시점으로 고정하지 않는다.

<a id="evidence"></a>
## 11. Module-local Contract

공통 단계와 actual/no-start/termination 충분조건은 [Async §1~4](../50_CONTRACTS/20_ASYNC_RESULT_EVIDENCE.md#confirmed-outcomes)를 따른다. AUDIO TX는 장치 범위의 `VssTxRequestResult`·`VssTxOutputEvidence`·소비/반환 자료를 원래 operation에 생성하며 Backend/전체 Session 결과를 대신 확정하지 않는다.

| TX 로컬 근거 | 생성·적용 경계와 남은 조건 |
| --- | --- |
| 수용/장치 활성화 | 원래 요구에 적용된 실제 범위만 `VssTxRequestResult`로 전달. 단순 vendor 대기 접수는 활성화로 보지 않음 |
| 실제 출력/no-start/termination | `VssTxOutputEvidence`의 원래 Attempt/scope·사실 시각·coverage. SAI WSF/frame 또는 동등 경계와의 충분성은 B2-R/보드 TBD |
| 소비 | `VssPcmConsumptionEvidence`를 정확한 A/B 회차에 적용. 반환/전체 종료 자료로 바꾸지 않음 |
| 안전 반환 | `VssPcmReturnEvidence`의 원래 operation/key에 [Buffer C1-03](../50_CONTRACTS/50_AUDIO_BUFFER_TRANSPORT.md#safe-write) 적용. STREAM usage를 직접 쓰지 않음 |

AUDIO STREAM은 이 장치 범위에 미래 PCM 차단을, PLAYBACK은 전체 후속 권한 차단/적법성을 결합한다. TX는 [원래 시간 문맥](../50_CONTRACTS/30_TIMING_FRESHNESS.md#time-context)과 [지속 raw의 개별 귀속 검증](../50_CONTRACTS/50_AUDIO_BUFFER_TRANSPORT.md#continuous-stream)을 보존한다.

## 12. 대표 오류 / 예외 처리 원칙

- 장치 사용권 보호/활성화가 일부만 진행됐거나 오류가 반환된 경우: 미래 접근 불가 입증 전 보호 유지. 명확한 미사용 거부만 안전 반환 근거다.
- 이전 A 회차 결과: 요청 전에 저장한 귀속 정보에 연결된 회차에만 적용하고 새 A 회차의 장치 사용권 보호를 해제하지 않는다.
- 출력/귀속·관측 불명확: 확인한 scope와 부족/손실을 보존해 상위에 전달한다. Session 정상화/최종 처리 권한은 상위에 남긴다.

## 13. 관련 Function 문서

[AudioTx_Request](../30_FUNCTIONS/50_DRIVER_HAL_FUNCTIONS.md#audiotx-request) · [AudioTx_Advance](../30_FUNCTIONS/50_DRIVER_HAL_FUNCTIONS.md#audiotx-advance) · [기존 후보의 R2 판정](../30_FUNCTIONS/00_FUNCTION_OVERVIEW.md#candidate-decisions) · [이전 명칭의 의미 연결](../30_FUNCTIONS/00_FUNCTION_OVERVIEW.md#merged).

## 14. 관련 Data 문서

[TX Operation / Evidence와 HAL의 사전 귀속 정보](../40_DATA/60_AUDIO_TX_DATA.md#tx)

## 15. 관련 공통 Contract

[Async / Evidence](../50_CONTRACTS/20_ASYNC_RESULT_EVIDENCE.md) · [Buffer / Transport](../50_CONTRACTS/50_AUDIO_BUFFER_TRANSPORT.md) · [Ownership](../50_CONTRACTS/10_OWNERSHIP_LIFETIME.md)

공통 규칙은 위 R4 계약을 따르며 이 문서의 owner·상태 적용·실패/예외는 로컬 조건으로 유지한다.

## 16. TBD / Deferred

callback 저장 수명은 [HAL/BSP](../10_LAYERS/details/40_HAL_BSP_BOUNDARY.md#lifetime), actual/no-start/end·abort 충분성은 [HW TBD](../90_BINDING/91_IMPLEMENTATION_TBD.md#hardware), DMA/배치는 [메모리 TBD](../90_BINDING/91_IMPLEMENTATION_TBD.md#memory)를 확인한다.

descriptor는 기존 논리 전송 수명 설명이다. 실제 transport가 polling/IRQ/DMA 중 무엇인지, channel/TCD/mode·API postcondition은 B2-R의 source/config 확인 전 TBD다.
