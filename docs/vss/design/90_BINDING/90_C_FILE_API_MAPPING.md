# VSS C File / API Mapping — Stage A Grounding + Stage B1 Service/Session

> 기존 Stage A/B1 근거 보존 · R0에서 source 재대조/확정 없음
> **B2 확정 진행 보류: R0 → R1 → R2 → R3 → R4 → R5 → R6 → R7 → B2-R**
> 2026-10-07 · Structured + Compact · **근거가 확인된 범위만 매핑 / Service·Session 1차 source 대조 완료**

## 1. 목적과 범위

이 문서는 [82 Data Model](../40_DATA/00_DATA_OVERVIEW.md)과 [80 API 후보](../30_FUNCTIONS/00_FUNCTION_OVERVIEW.md)를 실제 C 구현에 연결하기 위한 **1차 근거 매핑**이다.

이번 단계에서는 현재 확인 가능한 S32K344-WB Audio 코드와 build map에서 **이미 존재하는 파일/함수와 현재 책임**만 연결한다. 실제 source를 보지 못한 Module, Node Communication, generated RTD 세부, DMA/callback 충분조건은 임의로 확정하지 않는다.

중요한 해석:

- 82의 11개 Data 묶음 ≠ 11개 struct.
- 80의 13개 API 후보 ≠ 13개 새 public 함수.
- 논리 Module ≠ 물리 `.c` 파일 1:1.
- 기존 `VssAudioArbitrator`, `VssAudioSession`, `AudioAsset`, `Mp3PcmProvider`를 우선 재사용한다.
- 현재 `main.c`에 있는 책임을 전부 즉시 분리하지 않는다. ownership·시험성·vendor 격리가 실제로 필요한 경계만 후속 source 검토 후 분리한다.

## 2. 이번 검수 결론

Data Model + API Refinement 결과는 **현재 단계에서 유지 가능**하다. 추가 Data 묶음, generic EventBus, getter/setter 계층, 공통 Recovery framework는 만들 필요가 없다.

다만 C Mapping에서는 다음 세 가지를 특히 경계한다.

1. `AudioStream_*`와 `AudioTx_Process`를 이름 그대로 새 wrapper로 복제하지 않는다. 현재 Player/Provider/SAI 흐름을 먼저 실제 source에 매핑한다.
2. STORE와 HEALTH는 논리 owner가 필요하지만, 기존 `VssAudioArbitrator`/`VssAudioSession` 내부 상태와 중복되는 물리 파일을 먼저 만들지 않는다.
3. `actual / no-start / termination / retirement / late callback` 근거는 필요한 비동기 경계에서만 보존한다. 각 용어마다 별도 상태 구조체나 API를 만들지 않는다.

<a id="transport"></a>
## 3. 기존에 확인한 구현 근거

현재 확인한 build/source 근거에서는 다음 물리 요소가 존재한다.

| 현재 물리 요소 | 확인된 역할 |
| --- | --- |
| `src/VssAudioArbitrator.c` | request 제출, arbitration 진행, winner 조회 |
| `src/VssAudioSession.c` | semantic request/session 처리와 현재 session 상태 조회 |
| `src/AudioAsset.c` | asset init/size/read |
| `src/Mp3PcmProvider.c` | MP3 asset 선택/reset/init/render/terminal 확인 |
| `src/SoundAsset.c` | sound descriptor 조회 |
| `src/main.c` | WB bring-up, CS2100/SGTL5000 제어, Generic PCM Player, A/B 전송, VSS Port adapter, main loop. 2026-10-07 최신 build map에는 `AudioPlayer_Sai0IrqHandler`도 존재 |
| generated SAI/DMA/LPI2C/Clock/PIT config object | RTD generated 구성이 build에 포함됨. **DMA config 존재만으로 Audio DMA 사용을 확정하지 않는다.** 실제 field/API binding은 generated source 확인 전 TBD |

읽을 수 있는 2026-09-30 `main.c` snapshot에서는 `Sai_Ip_Send()` + `Sai_Ip_GetSendingStatus()` 기반 A/B 흐름이 확인된다. 반면 2026-10-07 build map에는 `AudioPlayer_Sai0IrqHandler` symbol이 추가되어 있다. 따라서 **현재 전송 모델을 순수 polling 또는 DMA/callback 중 하나로 단정하지 않는다.** 최신 source/generated config에서 IRQ handler 본문과 SAI/DMA binding을 확인한 뒤 확정한다.

## 4. Core API 후보 → 실제 C 매핑

상태 표기:

- **EXISTING**: 현재 실제 파일/함수와 직접 연결 가능
- **ADAPT**: 현재 코드가 책임을 갖지만 후보명 자체를 새 함수로 만들 이유는 없음
- **TBD**: 실제 source/config 근거가 더 필요함

| 논리 후보 | 1차 물리 매핑 | 상태 | 현재 결정 |
| --- | --- | --- | --- |
| `Flow_Process` | 현재 `main.c`의 순환 실행 + `VssAudioSession_Process()` / `VssAudioArbitrator_Process()` 호출 흐름 | ADAPT | 새 `Flow_Process()` wrapper를 지금 만들지 않는다. 최종 app entry는 구현 단계에서 최소화한다. |
| `Input_Process` | Node Communication / TEST 입력 연결 source 필요 | TBD | 입력 검증 owner는 유지하되 현재 파일명·signature 미확정 |
| `Store_Process` | 현 Arbitrator/Session 내부 상태 source 검토 필요 | TBD | `RequestStore.c`를 선생성하지 않는다. 기존 상태와 중복 여부 확인 후 결정 |
| `Select_Choose` | `src/VssAudioArbitrator.c`: `VssAudioArbitrator_Process()`, `VssAudioArbitrator_GetWinner()` | EXISTING | 별도 selection wrapper보다 기존 Arbitrator를 확장/정렬하는 방향 우선 |
| `Playback_Process` | `src/VssAudioSession.c`: `VssAudioSession_Process()` 등 | EXISTING | 물리 경계는 재사용한다. 다만 현재 single-pending bring-up 의미를 최종 Session/Attempt 계약으로 그대로 고정하지 않고 내부만 정제한다. 두 번째 Session controller는 만들지 않는다. |
| `Asset_Read` | `src/AudioAsset.c`: `AudioAsset_Read()` | EXISTING | 직접 재사용 우선 |
| `AudioStream_Prepare` | 현재 `main.c` Player prime/render + `Mp3PcmProvider.c` | ADAPT | 별도 prepare wrapper를 즉시 추가하지 않는다. provider/PCM 준비 경계만 유지 |
| `AudioStream_Process` | 현재 `main.c` `AudioPlayer_Service()` 계열 + Provider 결과 연결 | ADAPT | 현재 역할을 후속 source 기준으로 최소 분리. Service state machine 중복 금지 |
| `AudioTx_Process` | 이전 readable `main.c`의 `Sai_Ip_Send()` / `Sai_Ip_GetSendingStatus()` 경로 + 최신 map의 `AudioPlayer_Sai0IrqHandler` entry | ADAPT | 현재 transport의 실제 완료 근거가 polling인지 IRQ 보조인지, DMA가 실제 사용되는지는 최신 source/generated RTD 확인 뒤 결정 |
| `AudioControl_Service` | 현재 `main.c`의 CS2100/SGTL5000 read/write/config + readiness flags | ADAPT | Codec/Clock owner 의미는 유지. 실제 파일 분리·signature는 후속 source 기준 |
| `AudioHAL_Callback` | 최신 build map의 `AudioPlayer_Sai0IrqHandler` + 기존 `Cs2100Ref_Callback` | ADAPT | **entry 존재는 확인됐지만 handler body/postcondition은 아직 미확정.** ISR에서는 raw fact capture만 허용한다는 82/81 계약과 실제 코드를 R7 뒤 B2-R에서 대조 |
| `Health_Evaluate` | 현재 bring-up error/ready flags는 존재하나 Structured Health source는 미확인 | TBD | 기존 5 Fault 의미만 유지. 새 framework 생성 금지 |
| `Health_BuildStatus` | 외부 VSS_STATUS/Node Communication source 필요 | TBD | 실제 보고 구조와 producer를 확인한 뒤 연결 |

## 5. Data Model → 실제 저장 위치 매핑 수준

이 단계에서는 **type/field를 새로 정의하지 않고 저장 위치의 방향만** 잡는다.

| Data 묶음 | 현재 근거 / 후속 확인 |
| --- | --- |
| Input Context / Meta | Node Communication·TEST source가 필요. 현재 미확정 |
| Occurrence / Stateful Store | Arbitrator/Session 내부 request·winner/session 상태와 중복 여부를 source에서 확인 |
| Selection Decision | `VssAudioArbitrator`가 1차 물리 owner 후보 |
| Playback Session / Attempt | `VssAudioSession`이 1차 물리 owner 후보. 기존 판단은 lower async 근거를 요구했다. 현재 재구성 계획에서는 R2/R3의 논리 설계와 후속 B2-R의 실제 근거를 구분하며 필요한 만큼만 검토 |
| Asset Descriptor / Span | `SoundAsset` + `AudioAsset` + `Mp3PcmProvider` 경계 재사용 우선 |
| Audio Provider | `Mp3PcmProvider`와 현재 Player 연결을 기준으로 최소 확장 |
| PCM A/B Usage | 현재 A/B 두 Buffer와 Player 로직 존재. 최종 owner는 C09 의미를 유지하되 field/layout은 DMA 방식 확정 뒤 결정 |
| TX Operation / HAL binding | 이전 source의 SAI send/status 경로와 최신 map의 SAI IRQ entry를 확인. 실제 IRQ/DMA 완료 근거·registration lifetime은 후속 최신 source/generated RTD 검토 필요 |
| Codec / Clock Readiness | 현재 CS2100/SGTL5000 초기화·검증 flag를 기반으로 후속 정리 |
| Fault / Recovery | Structured C11 저장은 source 미확인. 기존 Fault 의미보다 깊게 새 모델을 만들지 않음 |
| VSS Status Snapshot | 기존 VSS_STATUS/Node Communication source 확인 후 파생-only 구조로 연결 |

## 6. 현재 코드에서 바로 재사용할 경계

현재 build map으로 확인된 public symbol은 다음 수준까지 재사용 후보로 본다.

```text
VssAudioArbitrator_Init
VssAudioArbitrator_SubmitRequest
VssAudioArbitrator_Process
VssAudioArbitrator_GetWinner

VssAudioSession_Init
VssAudioSession_SubmitRequest
VssAudioSession_Process
VssAudioSession_GetState
VssAudioSession_GetActiveRequest

AudioAsset_Init
AudioAsset_GetSize
AudioAsset_Read

Mp3PcmProvider_SelectAsset
Mp3PcmProvider_Reset
Mp3PcmProvider_Init
Mp3PcmProvider_RenderStereo16
Mp3PcmProvider_WasLastRenderTerminal

SoundAsset_GetDescriptor
```

이 목록은 **유지 확정 API 목록이 아니라 현재 존재 근거**다. Structured + Compact 설계와 겹치는 함수는 우선 재사용·정제하고, 이름만 다른 같은 기능의 wrapper를 추가하지 않는다.

## 7. B2-R 재개 시 필요한 최소 근거

R0~R7 뒤 B2-R에서 실제 lower Mapping을 재개할 때 다음 source/config 근거를 대조한다. signature/type/visibility는 R2~R6 결과와 source를 함께 확인하며 여기서 선확정하지 않는다.

1. 최신 `VssAudioArbitrator.c/.h`, `VssAudioSession.c/.h`
2. 최신 `AudioAsset.c/.h`, `SoundAsset.c/.h`, `Mp3PcmProvider.c/.h`
3. 최신 `main.c` 또는 해당 책임이 이동한 source
4. Node Communication / VSS_STATUS 입출력 source
5. generated `Sai_Ip`, `Dma_Ip`, `Lpi2c_Ip`, IRQ/callback config와 `AudioPlayer_Sai0IrqHandler` 실제 본문/signature/postcondition
6. linker/map의 PCM A/B·decoder workspace 배치 근거

여기서 source가 없는 항목은 `TBD`로 남긴다. 이 목록을 이유로 새 설계 문서를 추가하지 않는다.

## 8. 전체 C Mapping 완료 조건

이 절은 B2-R의 Mapping 완료 기준이다. 현재 다음 단계는 R1이며, R0~R7 완료 전에 Mapping 확정 작업을 재개하지 않는다.

완료 조건은 각 Core 후보가 다음 중 하나로 정리되는 것이다.

- 기존 symbol 재사용
- 기존 private 처리에 병합
- 실제로 필요한 신규 internal API 1개로 확정
- 근거 부족으로 TBD 유지

그 이상으로 Task/Queue/Mutex, generic EventBus, 범용 recovery layer, 별도 data-access layer, 파일-per-struct 분할은 하지 않는다.


## 9. Stage B1 — Service / Policy / Session 실제 source 대조

이번 대조는 **새 설계가 아니라 기존 구현을 얼마나 재사용할지 확인**하는 범위로 제한했다.

### 9.1 `VssAudioSession`

현재 source는 다음 뼈대를 이미 갖는다.

- semantic request만 받고 SAI/codec/flash를 직접 알지 않는 Port 경계
- one-shot과 looping warning의 구분
- `WARNING_ACTIVE` 반복으로 loop를 재시작하지 않는 처리
- `WARNING_CLEAR`를 sound asset이 아닌 stop 의미로 처리
- parser EOF만으로 COMPLETE를 만들지 않고 실제 audio boundary 관측을 기다리는 처리

반면 현재 구현은 bring-up용 **single pending slot + request sequence** 구조다. 따라서 82의 Occurrence/Stateful Store, Session/Attempt identity, late-result isolation을 이 구조에 억지로 덧붙여 public API를 늘리지 않는다.

결정:

- `VssAudioSession_Process()` 경계는 유지 후보.
- `StartRequest()`류 helper는 private 유지.
- 기존 `SubmitRequest()`는 현재 local bring-up entry로 보고, 최종 Network/Input 경계가 정해진 뒤 유지 여부를 판단.
- `Playback_Request/Stop/...` 같은 중복 wrapper는 추가하지 않음.

### 9.2 `VssAudioArbitrator`

최신 build map에서 `Init / SubmitRequest / Process / GetWinner` 물리 경계는 계속 존재한다. 따라서 `Select_Choose`를 별도 새 파일/함수로 복제하지 않는다.

Arbitrator 내부 source를 이번 근거 세트에서 완전히 확인하지 못했으므로 priority table, store ownership, occurrence identity의 실제 저장 위치는 아직 확정하지 않는다.

결정:

- `Select_Choose` → 기존 Arbitrator 경계 재사용 우선.
- `Store_Process` → 아직 별도 `RequestStore.c`를 만들지 않음. Network/Input + Arbitrator source를 함께 본 뒤 **기존 private state 병합 또는 최소 private store** 중 하나만 선택.

### 9.3 `Flow_Process`, `Input_Process`, `Health_*`

- `Flow_Process`: 현재 main loop orchestration을 바로 감싸는 wrapper를 만들지 않는다. 최종 app scheduler 연결 시 필요성이 생길 때만 entry를 둔다.
- `Input_Process`: Node Communication의 실제 input/meta 구조 전까지 TBD.
- `Health_Evaluate` / `Health_BuildStatus`: 기존 bring-up flag를 그대로 Structured Health로 승격하지 않는다. 5개 Fault/Recovery 의미와 실제 VSS_STATUS producer를 확인한 뒤 최소 연결한다.

### 9.4 B1 과설계 검사

- 신규 public API: **0개**
- 신규 Module/file 확정: **0개**
- 기존 Session/Arbitrator 재사용 방향: **유지**
- 근거 없는 Store/Health framework: **추가하지 않음**

B1의 목적은 '무엇을 새로 만들지'보다 **이미 있는 경계를 어디까지 살릴지**를 확정하는 것이다.

## 10. 최신 Lower-Audio 근거 보정 메모

- 2026-10-07 06:27 build map은 `AudioPlayer_Sai0IrqHandler`가 `main.o`에 포함된 것을 확인시킨다.
- 같은 map에 generated DMA config object가 포함되지만, 이것만으로 Audio TX가 DMA를 실제 사용한다고 결론내리지 않는다.
- 따라서 이 Stage A의 결론은 **'현재 lower audio는 이미 존재하는 구현을 재사용·정리할 대상이며, transport 방식은 최신 source/generated config 확인 전 TBD'**이다.
- 이 보정 때문에 새 Module/API를 추가하지 않는다. 오히려 기존 `AudioPlayer_*` / SAI entry가 C09 경계를 충족하는지 먼저 확인한다.


## 문서 연결과 후속 범위

[C Interface 미확정](../70_C_INTERFACE/00_HEADER_OWNERSHIP_MAP.md) · [Function 후보](../30_FUNCTIONS/00_FUNCTION_OVERVIEW.md) · [Data 의미](../40_DATA/00_DATA_OVERVIEW.md) · [TBD](91_IMPLEMENTATION_TBD.md)

이 문서는 이전 단계에서 확인한 근거를 인수한 것이다. 이번 R0에서 source/generated config·DMA/TCD·IRQ postcondition을 새로 검사하거나 확정하지 않았다. 기존 재사용 방향은 유지하며 논리 후보 이름 때문에 중복 wrapper를 만들지 않는다.
