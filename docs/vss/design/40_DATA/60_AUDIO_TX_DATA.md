# Audio TX / Control / HAL Data — R3

> 2026-10-07 R3 · 2026-10-08 R4 공통 설명 연결 · R3-C1/국소 함수 독립 PASS 기준 · 필드/owner/선언·Binding TBD 유지

[Data Overview](00_DATA_OVERVIEW.md) · [R2 타입 판정](00_DATA_OVERVIEW.md#type-decisions)

<a id="tx"></a>
AUDIO TX는 실제 하위 요청 전의 operation·PCM 접근 보호와 후속 사실을 소유한다. HAL Boundary는 요청 전에 저장한 불변 귀속과 raw 포착을 소유한다. HAL/RUNTIME을 독립 Module로 만들지 않는다. 물리 SAI/eDMA/codec 관측의 충분조건·abort postcondition은 B2-R/보드 검증 TBD다.

**타입 읽기:** 아래 선언은 구조체 14개, enum 8개, 의미 alias 5개다. `Category`는 자료의 역할이며 선언 종류와 다르다. 각 구조체의 짧은 안내는 구성원 타입과 실제 정의 위치만 연결한다. 필드의 의미·생성 주체·유효 조건은 기존 Field 정의와 Lifetime / Contract 설명을 따른다.

`const T *`는 T의 선언 종류와 별도로 읽는 포인터 참조다. 읽기 참조는 원본 owner·writer를 이전하지 않으며, 필요한 원자료는 실제 소비·후속 참조 종료까지 보호한다. 인자 주소의 수명과 참조 자료의 수명은 [소유·수명 계약](../50_CONTRACTS/10_OWNERSHIP_LIFETIME.md#borrow-and-protect)을 따른다. 부재가 허용되는 조건은 각 필드 정의에서 확인한다. `PcmSamplesRef`는 중립 PCM 참조이고 `RawFactValue`·`Semantic...`은 pseudo-scalar 표기다. 실제 C 기본형·포인터형·폭·ABI는 [pseudo-C 규칙](00_DATA_OVERVIEW.md#pseudo-conventions)에 따라 미정이다.

**논리 Header와 실제 구현:** 아래 `H-*`는 [R6 타입 소유권 대응](../70_C_INTERFACE/00_HEADER_OWNERSHIP_MAP.md#type-map)의 label이다. [선언 경계 후보·확인 범위](../70_C_INTERFACE/00_HEADER_OWNERSHIP_MAP.md#header-candidates)에서 `H-TX`는 제공된 `AudioDmaTransport.h` 재사용 우선 후보지만 현재 R3 typed 선언은 없다. `H-CONTROL`의 `AudioCodec.h` / `AudioClock.h`는 include·호출 근거만 있고 Header 본문은 미제공이다. `H-HAL`은 transport/BSP 내부의 논리 경계이며 개별 등록·raw 경계는 미구현이다. 논리 계약과 실제 Header 구현을 구분하며 새 Header 파일·ABI·RTD/DMA 동작을 확정하지 않는다.

<a id="vssoutputscope"></a>
## VssOutputScope — scalar enum
**Category: Descriptor**

출력 근거/차단 요구의 범위다. 하위가 전체 Session 정책 끝을 판정하지 못하도록 한다.

**Owner:** 원래 요청을 만드는 PLAYBACK/AUDIO STREAM와 하위 범위 적용 owner · **Producer:** AUDIO STREAM → AUDIO TX · **Consumer:** AUDIO TX/AUDIO STREAM/PLAYBACK

| 값 | 의미 / 유효 조건 |
| --- | --- |
| `VSS_SCOPE_SEGMENT` | 원래 계획의 관련 출력 구간 |
| `VSS_SCOPE_ATTEMPT` | 원래 실제 출력 시도의 전체 하위 출력 범위 |

**Lifetime / Mutability:** 원래 identity와 함께만 의미 유효. 전체 Session 완료는 상위 plan/권한 확인이 추가로 필요하다.

**관련 Function:** [AudioTx_Request](../30_FUNCTIONS/50_DRIVER_HAL_FUNCTIONS.md#audiotx-request) · [AudioTx_Advance](../30_FUNCTIONS/50_DRIVER_HAL_FUNCTIONS.md#audiotx-advance) · [AudioStream_Advance](../30_FUNCTIONS/40_AUDIO_STREAM_ASSET_FUNCTIONS.md#audiostream-advance) · [Playback_Advance](../30_FUNCTIONS/30_POLICY_PLAYBACK_FUNCTIONS.md#playback-advance)

**관련 Contract:** [비동기 근거](../50_CONTRACTS/00_CONTRACT_OVERVIEW.md#async)

**논리 Header(R6):** `H-TX`.

```c
typedef enum
{
    VSS_SCOPE_SEGMENT,
    VSS_SCOPE_ATTEMPT
} VssOutputScope;
```

<a id="vsspcmhandoffcommand"></a>
## VssPcmHandoffCommand
**Category: Command**

**선언 종류:** 구조체 (`struct`).

안정된 유효 PCM 구간을 해당 Buffer/회차로 인계한다. 출력 start/stop 제어와 필드·자료 보호 수명이 달라 분리한다.

**Owner:** AUDIO STREAM 인계 의미  
**Producer:** AudioStream_Prepare/AudioStream_Advance 내부 PCM 생산  
**Consumer:** AudioTx_Request

### 사람이 읽는 Field 정의

| Field | 의미 / 필요한 이유 | Field 생성 주체 | 유효 조건 |
| --- | --- | --- | --- |
| `key` | 원래 시도·A/B·사용 회차. | AUDIO STREAM | DRIVER 호출 전 pending 보호부터 |
| `samples` | CPU 생산을 끝낸 PCM의 중립 참조. | AUDIO STREAM | 실제 미래 장치 접근이 끝날 때까지 내용 안정 |
| `firstValidFrame` | 유효 PCM 구간 시작. | AUDIO STREAM | 확인된 구간만 |
| `validFrameCount` | 해당 음향의 논리적으로 유효한 frame 수. 물리 무음 채움/Buffer 용량을 자동 대입하지 않는다. | AUDIO STREAM | 0보다 큰 확인된 음향 구간만 인계. 0일 때의 물리 무음 지속은 별도 의미 |
| `format` | 이 유효 구간의 확인된 PCM 의미 format. | AUDIO STREAM | 현재 장치 구성과 호환성 확인 후 |
| `lastForSegment` | 해당 source/출력 구간의 마지막 유효 PCM인지. 전체 Session final은 아니다. | AUDIO STREAM의 source 끝 근거 | 확인된 마지막 tail에만 true |

### Lifetime / Mutability / 폐기

AUDIO STREAM이 DRIVER 호출 전 HANDOFF_PENDING과 불변 Command를 보호한다. AUDIO TX/HAL이 필요 귀속·참조를 보존하고 부분 오류에도 미래 접근 가능성 종료까지 PCM을 보호한다. Command 인자 자체는 필요한 의미를 보존한 뒤 끝낼 수 있지만 samples의 보호 수명은 별개다.

이 Command는 >0의 유효 음향 구간만 인계하며 물리 슬롯 길이를 표현하지 않는다. zero-fill/빈 B/짧은 tail의 공통 구분·CPU 안전 조건은 [Buffer C1-01](../50_CONTRACTS/50_AUDIO_BUFFER_TRANSPORT.md#valid-and-silence)을 따른다. 실제 물리 전송 길이/무음 매핑은 B2-R TBD다.

**관련 Function:** [AudioStream_Prepare](../30_FUNCTIONS/40_AUDIO_STREAM_ASSET_FUNCTIONS.md#audiostream-prepare) · [AudioStream_Advance](../30_FUNCTIONS/40_AUDIO_STREAM_ASSET_FUNCTIONS.md#audiostream-advance) · [AudioTx_Request](../30_FUNCTIONS/50_DRIVER_HAL_FUNCTIONS.md#audiotx-request) · [AudioTx_Advance](../30_FUNCTIONS/50_DRIVER_HAL_FUNCTIONS.md#audiotx-advance)

**관련 Contract:** [PCM·전송](../50_CONTRACTS/00_CONTRACT_OVERVIEW.md#buffer) · [소유·수명](../50_CONTRACTS/00_CONTRACT_OVERVIEW.md#ownership) · [비동기 근거](../50_CONTRACTS/00_CONTRACT_OVERVIEW.md#async)

**논리 Header(R6):** `H-STREAM`.

TX 소비용 중립 인계 선언은 [H-TX 재사용 우선 후보](../70_C_INTERFACE/00_HEADER_OWNERSHIP_MAP.md#include-direction)다. 실제 선언 파일은 미정이다.

### Pseudo-C

**구성원 타입 → 정의 위치**

- 구조체 값: `key` → [VssPcmCycleKey](50_AUDIO_STREAM_DATA.md#vsspcmcyclekey).
- 중립 참조: `samples` → [PcmSamplesRef](00_DATA_OVERVIEW.md#pseudo-conventions).
- pseudo-scalar: `firstValidFrame`, `validFrameCount` → [SemanticFrameCount](00_DATA_OVERVIEW.md#pseudo-conventions); `lastForSegment` → [SemanticBool](00_DATA_OVERVIEW.md#pseudo-conventions).
- 의미 alias: `format` → [VssPcmFormatType](50_AUDIO_STREAM_DATA.md#vsspcmformattype).

```c
typedef struct
{
    VssPcmCycleKey key;
    PcmSamplesRef samples;
    SemanticFrameCount firstValidFrame;
    SemanticFrameCount validFrameCount;
    VssPcmFormatType format;
    SemanticBool lastForSegment;
} VssPcmHandoffCommand;
```



<a id="vsstxcontrolaction"></a>
## VssTxControlAction — scalar enum
**Category: Command**

원래 출력 범위의 장치 활성화/정리 요구다. PCM 제출과 구별한다.

개별 음향의 START/STOP과 지속 SAI/eDMA 스트림 자체의 시작/종료는 서로 다른 수명이다. 정상 브링업의 단일 Sai_Ip_Send() 후 지속 DMA를 매 음향마다 재등록·재시작하는 의미로 해석하지 않는다. 해당 음향의 공급/차단을 실제 출력 범위에 연결하는 방법은 B2-R TBD이며 수용·실제 출력·종료 근거 구분을 유지한다.

**Owner:** AUDIO STREAM · **Producer:** AudioStream_RequestControl / 내부 구간 진행 · **Consumer:** AudioTx_Request

| 값 | 의미 / 유효 조건 |
| --- | --- |
| `VSS_TX_START` | 실제 전송/출력 가능하도록 해당 작업 활성화 요구 |
| `VSS_TX_STOP` | 해당 옛 작업의 미래 출력/접근 차단·정리 요구 |

**Lifetime / Mutability:** 요구 생성 뒤 불변. STOP 수락이 실제 종료/안전 반환은 아니다.

**관련 Function:** [AudioStream_RequestControl](../30_FUNCTIONS/40_AUDIO_STREAM_ASSET_FUNCTIONS.md#audiostream-requestcontrol) · [AudioStream_Advance](../30_FUNCTIONS/40_AUDIO_STREAM_ASSET_FUNCTIONS.md#audiostream-advance) · [AudioTx_Request](../30_FUNCTIONS/50_DRIVER_HAL_FUNCTIONS.md#audiotx-request)

**관련 Contract:** [비동기 근거](../50_CONTRACTS/00_CONTRACT_OVERVIEW.md#async) · [PCM·전송](../50_CONTRACTS/00_CONTRACT_OVERVIEW.md#buffer)

**논리 Header(R6):** `H-TX`.

```c
typedef enum
{
    VSS_TX_START,
    VSS_TX_STOP
} VssTxControlAction;
```

<a id="vsstxcontrolcommand"></a>
## VssTxControlCommand
**Category: Command**

**선언 종류:** 구조체 (`struct`).

PCM 주소 없이 동일 Attempt의 start/stop 범위를 전달한다. 원래 첫 실제 시작 조건은 필요한 start에만 전달하고 Started 정상 후속 cue에 재적용하지 않는다. `scope` enum 자체는 SEGMENT identity가 아니며, [원 Attempt·구간·operation의 STOP 대응](../70_C_INTERFACE/00_HEADER_OWNERSHIP_MAP.md#stop-scope)을 함께 확인한다.

**Owner:** AUDIO STREAM 제어 의미  
**Producer:** AudioStream_RequestControl 또는 AudioStream_Advance 내부 진행  
**Consumer:** AudioTx_Request

### 사람이 읽는 Field 정의

| Field | 의미 / 필요한 이유 | Field 생성 주체 | 유효 조건 |
| --- | --- | --- | --- |
| `attempt` | 원래 출력 시도. | AUDIO STREAM | 요구/효과 추적 수명 |
| `action` | START/STOP. | AUDIO STREAM | 해당 요구 수명 |
| `scope` | 관련 구간/시도 전체 정리·활성화 범위. | AUDIO STREAM | 원래 작업과 일치할 때 |
| `firstStartMeta` | 원본 첫 시작 gate가 필요한 START의 시간/기한 근거. 이미 Started된 정상 후속 또는 STOP에는 없음. | PLAYBACK 조건 → AUDIO STREAM 보존 | 해당 first-start에만 의미 유효; 새 now로 바꾸지 않음 |

### Lifetime / Mutability / 폐기

하위 호출 전 원래 operation/불변 귀속을 보호한다. 하위 수용/장치 활성화/actual의 서로 다른 사실을 추적한다. 부분 효력 뒤 실패는 보호된 옛 작업의 정리이며 일반 신규 거부 rollback이 아니다.

**관련 Function:** [AudioStream_RequestControl](../30_FUNCTIONS/40_AUDIO_STREAM_ASSET_FUNCTIONS.md#audiostream-requestcontrol) · [AudioStream_Advance](../30_FUNCTIONS/40_AUDIO_STREAM_ASSET_FUNCTIONS.md#audiostream-advance) · [AudioTx_Request](../30_FUNCTIONS/50_DRIVER_HAL_FUNCTIONS.md#audiotx-request)

**관련 Contract:** [소유·수명](../50_CONTRACTS/00_CONTRACT_OVERVIEW.md#ownership) · [비동기 근거](../50_CONTRACTS/00_CONTRACT_OVERVIEW.md#async) · [시간·최신성](../50_CONTRACTS/00_CONTRACT_OVERVIEW.md#timing)

**논리 Header(R6):** `H-TX`.

### Pseudo-C

**구성원 타입 → 정의 위치**

- 의미 alias: `attempt` → [VssAttemptKeyType](40_PLAYBACK_DATA.md#vssattemptkeytype).
- enum: `action` → [VssTxControlAction](#vsstxcontrolaction); `scope` → [VssOutputScope](#vssoutputscope).
- 구조체 읽기 참조 (`const T *`): `firstStartMeta` → [VssInputMeta](10_INPUT_DATA.md#vssinputmeta).

```c
typedef struct
{
    VssAttemptKeyType attempt;
    VssTxControlAction action;
    VssOutputScope scope;
    const VssInputMeta * firstStartMeta;
} VssTxControlCommand;
```



<a id="vsstxrequeststate"></a>
## VssTxRequestState — scalar enum
**Category: Result**

새 전송 요구의 동기 수용·장치 활성화/부분 효력을 구별한다. 실제 출력 근거와 안전 반환은 별도 Evidence다.

**Owner:** AUDIO TX · **Producer:** AudioTx_Request/AudioTx_Advance · **Consumer:** AUDIO STREAM

| 값 | 의미 / 유효 조건 |
| --- | --- |
| `VSS_TX_SUBMITTED_OR_QUEUED` | 요구 제출/대기 접수. 실제 활성화 미확인 |
| `VSS_TX_ACCEPTED_AND_ENABLED` | 원래 요구 수락과 실제 장치 활성화 확인 |
| `VSS_TX_REJECTED_WITH_NO_ACCESS` | 해당 요구로 하위 접근/활성화가 전혀 없거나 종료됐다는 충분 근거가 있는 거부 |
| `VSS_TX_EFFECT_UNCERTAIN` | 실패 반환/부분 효력·미래 접근 가능성을 확정하지 못함 |
| `VSS_TX_CLEANUP_PENDING` | 정리 요구 연결됐으나 실제 효과 추적 중 |

**Lifetime / Mutability:** 원래 operation 결과 수명. 요청 return만으로 실제 출력/안전 반환을 생성하지 않는다.

**관련 Function:** [AudioTx_Request](../30_FUNCTIONS/50_DRIVER_HAL_FUNCTIONS.md#audiotx-request) · [AudioTx_Advance](../30_FUNCTIONS/50_DRIVER_HAL_FUNCTIONS.md#audiotx-advance)

**관련 Contract:** [비동기 근거](../50_CONTRACTS/00_CONTRACT_OVERVIEW.md#async) · [PCM·전송](../50_CONTRACTS/00_CONTRACT_OVERVIEW.md#buffer)

**논리 Header(R6):** `H-TX`.

```c
typedef enum
{
    VSS_TX_SUBMITTED_OR_QUEUED,
    VSS_TX_ACCEPTED_AND_ENABLED,
    VSS_TX_REJECTED_WITH_NO_ACCESS,
    VSS_TX_EFFECT_UNCERTAIN,
    VSS_TX_CLEANUP_PENDING
} VssTxRequestState;
```

<a id="vsstxoperation"></a>
## VssTxOperation
**Category: Context**

**선언 종류:** 구조체 (`struct`).

개별 음향의 하위 전송/제어 요구에 대한 소프트웨어 귀속·장치 미래 접근/출력 보호를 유지한다. 이 operation의 수명이 vendor의 지속 스트림 등록 수명과 1:1이라고 가정하지 않는다. 여러 owner의 상태를 넣는 종합 Context가 아니다.

**Owner:** AUDIO TX 단일 writer  
**Producer:** AudioTx_Request/AudioTx_Advance  
**Consumer:** AUDIO TX / HAL 등록 연결

### 사람이 읽는 Field 정의

| Field | 의미 / 필요한 이유 | Field 생성 주체 | 유효 조건 |
| --- | --- | --- | --- |
| `operation` | 해당 하위 요청의 인과 identity. | AUDIO TX | 하위 호출 전 생성·보호; 원래 결과 추적까지 |
| `attempt` | 원래 시도. | AUDIO STREAM → AUDIO TX | 생성 뒤 불변 |
| `scope` | 관련 출력 범위. | AUDIO STREAM → AUDIO TX | 원래 요구 범위 |
| `handoff` | PCM 전송이면 불변 인계의 보호된 의미/회차·범위. 출력 제어만이면 없음. | AUDIO TX가 보존 | PCM 미래 접근 및 필요한 귀속 참조 종료까지 |
| `requestState` | 동기 수용/장치 활성화·부분 효력/정리 진행. | AUDIO TX | 해당 효과 반영 뒤 |
| `deviceAccessPossible` | 해당 PCM/작업의 미래 하위 접근 가능성. 불명확하면 보호 유지. | AUDIO TX의 실제 근거 적용 | 거부/consumed만으로 false 금지 |
| `futureOutputPossible` | 해당 옛 장치/FIFO/frame에서 미래 출력 가능성. 과거 무출력과 별도다. | AUDIO TX의 실제 근거 적용 | 실제 scope 차단 근거 확인 뒤만 false |
| `activationAt` | 원래 실제 활성화의 포착 근거. 새 actual은 이 요청/활성화 이후의 사실이어야 한다. | 하위 실제 근거 → AUDIO TX | 활성화 확인 후; 요청 접수 시각으로 합성 금지 |

### Lifetime / Mutability / 폐기

하위 호출 또는 해당 PCM/제어가 효력을 가질 수 있는 경계 전에 pending 보호·귀속을 확보한다. AUDIO TX만 장치 보호/요청 state를 변경하고 PCM 내용/CPU 사용권은 직접 쓰지 않는다. operation은 해당 옛 작업의 미래 접근/출력 종료·결과 추적 완료까지 유지한다. [HAL의 불변 소프트웨어 귀속](#vsshaltxregistration)은 현재 operation 포인터로 대체하지 않으며 지속 vendor 등록 자체와 구별한다.

**관련 Function:** [AudioTx_Request](../30_FUNCTIONS/50_DRIVER_HAL_FUNCTIONS.md#audiotx-request) · [AudioTx_Advance](../30_FUNCTIONS/50_DRIVER_HAL_FUNCTIONS.md#audiotx-advance) · [AudioHAL_Callback](../30_FUNCTIONS/50_DRIVER_HAL_FUNCTIONS.md#audiohal-callback)

**관련 Contract:** [소유·수명](../50_CONTRACTS/00_CONTRACT_OVERVIEW.md#ownership) · [비동기 근거](../50_CONTRACTS/00_CONTRACT_OVERVIEW.md#async) · [PCM·전송](../50_CONTRACTS/00_CONTRACT_OVERVIEW.md#buffer)

**논리 Header(R6):** `H-TX`.

### Pseudo-C

**구성원 타입 → 정의 위치**

- 의미 alias: `operation` → [VssOperationKeyType](#vssoperationkeytype); `attempt` → [VssAttemptKeyType](40_PLAYBACK_DATA.md#vssattemptkeytype).
- enum: `scope` → [VssOutputScope](#vssoutputscope); `requestState` → [VssTxRequestState](#vsstxrequeststate).
- 구조체 읽기 참조 (`const T *`): `handoff` → [VssPcmHandoffCommand](#vsspcmhandoffcommand); `activationAt` → [VssFactTime](10_INPUT_DATA.md#vssfacttime).
- pseudo-scalar: `deviceAccessPossible`, `futureOutputPossible` → [SemanticBool](00_DATA_OVERVIEW.md#pseudo-conventions).

```c
typedef struct
{
    VssOperationKeyType operation;
    VssAttemptKeyType attempt;
    VssOutputScope scope;
    const VssPcmHandoffCommand * handoff;
    VssTxRequestState requestState;
    SemanticBool deviceAccessPossible;
    SemanticBool futureOutputPossible;
    const VssFactTime * activationAt;
} VssTxOperation;
```



<a id="vsstxrequestresult"></a>
## VssTxRequestResult
**Category: Result**

**선언 종류:** 구조체 (`struct`).

동기 요구의 수용/부분 효력만 반환한다. 여러 후속 실제 사실을 하나의 Progress 슬롯으로 덮지 않는다.

**Owner:** AUDIO TX  
**Producer:** AudioTx_Request/AudioTx_Advance  
**Consumer:** AUDIO STREAM

### 사람이 읽는 Field 정의

| Field | 의미 / 필요한 이유 | Field 생성 주체 | 유효 조건 |
| --- | --- | --- | --- |
| `operation` | 결과가 귀속되는 실제 하위 요구. | AUDIO TX | operation 생성 뒤 |
| `attempt` | 원래 시도. | AUDIO TX 보호 문맥 | 해당 결과 수명 |
| `state` | 제출/수락+활성화/명확한 무접근 거부/불명확·정리. | AUDIO TX | 충분한 근거 반영 뒤 |

### Lifetime / Mutability / 폐기

생성 뒤 불변이며 AUDIO STREAM 반영/보호 추적까지 유지한다. accepted는 actual 아니며 PCM 인계 수락 자체는 start 활성화가 아니다. 결과가 늦게 와도 이미 반영한 출력/종료를 되돌리지 않는다.

**관련 Function:** [AudioTx_Request](../30_FUNCTIONS/50_DRIVER_HAL_FUNCTIONS.md#audiotx-request) · [AudioTx_Advance](../30_FUNCTIONS/50_DRIVER_HAL_FUNCTIONS.md#audiotx-advance) · [AudioStream_Advance](../30_FUNCTIONS/40_AUDIO_STREAM_ASSET_FUNCTIONS.md#audiostream-advance)

**관련 Contract:** [비동기 근거](../50_CONTRACTS/00_CONTRACT_OVERVIEW.md#async) · [PCM·전송](../50_CONTRACTS/00_CONTRACT_OVERVIEW.md#buffer)

**논리 Header(R6):** `H-TX`.

### Pseudo-C

**구성원 타입 → 정의 위치**

- 의미 alias: `operation` → [VssOperationKeyType](#vssoperationkeytype); `attempt` → [VssAttemptKeyType](40_PLAYBACK_DATA.md#vssattemptkeytype).
- enum: `state` → [VssTxRequestState](#vsstxrequeststate).

```c
typedef struct
{
    VssOperationKeyType operation;
    VssAttemptKeyType attempt;
    VssTxRequestState state;
} VssTxRequestResult;
```



<a id="vsscoveragevalidity"></a>
## VssCoverageValidity — scalar enum
**Category: Evidence**

물리 관측 범위/기간에 손실·충분성 부족이 있는지 보존한다. 실제 충분조건은 Binding에서 입증한다.

**Owner:** 해당 관측 producer · **Producer:** HAL/AUDIO TX/실제 검증 주체 · **Consumer:** DRIVER/AUDIO STREAM/PLAYBACK/HEALTH

| 값 | 의미 / 유효 조건 |
| --- | --- |
| `VSS_COVERAGE_SUFFICIENT` | 이 판정에 필요한 범위/기간/물리 상관을 실제 근거로 확보 |
| `VSS_COVERAGE_INSUFFICIENT` | 관측 또는 물리 상관의 근거 부족 |
| `VSS_COVERAGE_LOST` | 귀속/시간/사실 저장 누락·overflow 등 손실 존재 |

**Lifetime / Mutability:** 원래 관측 범위와 함께 불변. 수치/관측 수단은 이번 단계에서 정하지 않는다.

**관련 Function:** [AudioHAL_Callback](../30_FUNCTIONS/50_DRIVER_HAL_FUNCTIONS.md#audiohal-callback) · [AudioTx_Advance](../30_FUNCTIONS/50_DRIVER_HAL_FUNCTIONS.md#audiotx-advance)

**관련 Contract:** [비동기 근거](../50_CONTRACTS/00_CONTRACT_OVERVIEW.md#async) · [시간·최신성](../50_CONTRACTS/00_CONTRACT_OVERVIEW.md#timing)

**논리 Header(R6):** `H-TX`.

```c
typedef enum
{
    VSS_COVERAGE_SUFFICIENT,
    VSS_COVERAGE_INSUFFICIENT,
    VSS_COVERAGE_LOST
} VssCoverageValidity;
```

<a id="vssobservationcoverage"></a>
## VssObservationCoverage
**Category: Evidence**

**선언 종류:** 구조체 (`struct`).

실제 output/no-output/접근 차단을 판단할 관측 경계·기간과 source/build 문맥을 보존한다. 현재 idle이 과거 무출력 증거로 바뀌지 않게 한다.

**Owner:** 관측 producer의 원래 근거  
**Producer:** HAL 포착 / AUDIO TX 범위 검증  
**Consumer:** AUDIO TX/AUDIO STREAM/PLAYBACK

### 사람이 읽는 Field 정의

| Field | 의미 / 필요한 이유 | Field 생성 주체 | 유효 조건 |
| --- | --- | --- | --- |
| `boundary` | 실제 관측한 물리/장치 경계. SAI frame 등 상관 충분성은 B2-R TBD. | 원래 관측 주체 | 실제로 확인한 경계만 |
| `interval` | 실제로 관측한 범위의 시간 구간. | 원래 관측 주체 | 원래 시간/연속성이 보호된 범위 |
| `binding` | 관측에 사용한 장치/구성·source/build 근거 문맥. | HAL/BSP 현재 근거 | 그 문맥이 실제 적용된 작업에 한함 |
| `validity` | 충분/부족/손실 구별. | 관측/검증 주체 | 해당 판정 범위 대조 뒤 |

### Lifetime / Mutability / 폐기

포착/검증 뒤 불변이고 소비자가 의미 반영 또는 보호된 추적을 끝낼 때까지 유지한다. 현재 구성/시각으로 옛 interval/binding을 덮지 않는다. WSF 미관측/단일 complete/IRQ disable 자체를 충분성으로 고정하지 않는다.

**관련 Function:** [AudioHAL_Callback](../30_FUNCTIONS/50_DRIVER_HAL_FUNCTIONS.md#audiohal-callback) · [AudioTx_Advance](../30_FUNCTIONS/50_DRIVER_HAL_FUNCTIONS.md#audiotx-advance) · [AudioStream_Advance](../30_FUNCTIONS/40_AUDIO_STREAM_ASSET_FUNCTIONS.md#audiostream-advance) · [Playback_Advance](../30_FUNCTIONS/30_POLICY_PLAYBACK_FUNCTIONS.md#playback-advance)

**관련 Contract:** [비동기 근거](../50_CONTRACTS/00_CONTRACT_OVERVIEW.md#async) · [시간·최신성](../50_CONTRACTS/00_CONTRACT_OVERVIEW.md#timing)

**논리 Header(R6):** `H-TX`.

### Pseudo-C

**구성원 타입 → 정의 위치**

- 의미 alias: `boundary` → [VssObservationBoundaryType](#vssobservationboundarytype); `binding` → [VssBindingKeyType](#vssbindingkeytype).
- 구조체 값: `interval` → [VssFactTime](10_INPUT_DATA.md#vssfacttime).
- enum: `validity` → [VssCoverageValidity](#vsscoveragevalidity).

```c
typedef struct
{
    VssObservationBoundaryType boundary;
    VssFactTime interval;
    VssBindingKeyType binding;
    VssCoverageValidity validity;
} VssObservationCoverage;
```



<a id="vsstxoutputfactkind"></a>
## VssTxOutputFactKind — scalar enum
**Category: Evidence**

서로 다른 실제 장치 사실을 각각 전달한다. 과거 무출력과 미래 출력 차단을 한 boolean으로 합치지 않는다.

**Owner:** AUDIO TX의 범위 판정 · **Producer:** AudioTx_Advance · **Consumer:** AUDIO STREAM

| 값 | 의미 / 유효 조건 |
| --- | --- |
| `VSS_TX_NEW_OUTPUT_OBSERVED` | 원래 요청/실제 활성화 이후 새 출력 경계 관측 |
| `VSS_TX_PAST_NO_OUTPUT_PROVEN` | 필요 과거 범위의 실제 무출력 입증 |
| `VSS_TX_FUTURE_OUTPUT_BLOCKED` | 해당 옛 요청/active work/FIFO/frame의 미래 출력 불가 입증 |
| `VSS_TX_OUTPUT_ENDED` | 해당 범위의 장치/잔류 출력 종료 근거 |
| `VSS_TX_OUTPUT_UNCERTAIN` | 확정에 필요한 근거 부족·손실 |

**Lifetime / Mutability:** 각 원래 operation/범위/시각에만 유효. 전체 Session outcome 아님.

**관련 Function:** [AudioTx_Advance](../30_FUNCTIONS/50_DRIVER_HAL_FUNCTIONS.md#audiotx-advance) · [AudioStream_Advance](../30_FUNCTIONS/40_AUDIO_STREAM_ASSET_FUNCTIONS.md#audiostream-advance)

**관련 Contract:** [비동기 근거](../50_CONTRACTS/00_CONTRACT_OVERVIEW.md#async)

**논리 Header(R6):** `H-TX`.

```c
typedef enum
{
    VSS_TX_NEW_OUTPUT_OBSERVED,
    VSS_TX_PAST_NO_OUTPUT_PROVEN,
    VSS_TX_FUTURE_OUTPUT_BLOCKED,
    VSS_TX_OUTPUT_ENDED,
    VSS_TX_OUTPUT_UNCERTAIN
} VssTxOutputFactKind;
```

<a id="vsstxoutputevidence"></a>
## VssTxOutputEvidence
**Category: Evidence**

**선언 종류:** 구조체 (`struct`).

원래 장치 범위의 새 출력·과거 무출력·미래 차단/종료 근거다. raw complete나 buffer 소비를 actual/end로 자동 변환하지 않는다.

**Owner:** AUDIO TX  
**Producer:** AudioTx_Advance의 실제 근거 적용  
**Consumer:** AudioStream_Advance

### 사람이 읽는 Field 정의

| Field | 의미 / 필요한 이유 | Field 생성 주체 | 유효 조건 |
| --- | --- | --- | --- |
| `operation` | 원래 하위 작업 identity. | 사전 보호된 소프트웨어 귀속과 raw 관측의 대응 확인 → AUDIO TX | 불변 귀속 확인 뒤 |
| `attempt` | 원래 시도. 지속 스트림 등록만으로 추정하지 않는다. | 원래 소프트웨어 귀속/operation | 현재 Attempt 읽어서 대입 금지 |
| `kind` | 실제로 확보한 출력 관련 사실. | AUDIO TX | kind별 충분한 근거가 확보된 범위 |
| `scope` | 해당 구간/Attempt 출력 범위. | 원래 작업 → AUDIO TX | 요구/관측 범위 대조 뒤 |
| `occurredAt` | 물리 사건/효력의 원래 시각 또는 구간. | 원래 포착/실제 검증 | 후속 처리 시각으로 교체 금지 |
| `coverage` | 판정에 필요한 원래 경계·기간/구성·손실. | HAL/DRIVER 검증 | 실제 확인 범위만 |

### Lifetime / Mutability / 폐기

생성 뒤 불변으로 보존하고 AUDIO STREAM이 반영/근거 보호를 끝낼 때까지 유지한다. NEW_OUTPUT은 원래 요청/활성화 이후의 새 관측이어야 한다. NO_OUTPUT_PROVEN과 FUTURE_OUTPUT_BLOCKED는 각각 충분해야 하며 상위 미래 PCM/cue 차단은 포함하지 않는다. 관측 부족이면 OUTPUT_UNCERTAIN/손실 진단을 보존한다.

**관련 Function:** [AudioTx_Advance](../30_FUNCTIONS/50_DRIVER_HAL_FUNCTIONS.md#audiotx-advance) · [AudioStream_Advance](../30_FUNCTIONS/40_AUDIO_STREAM_ASSET_FUNCTIONS.md#audiostream-advance) · [Playback_Advance](../30_FUNCTIONS/30_POLICY_PLAYBACK_FUNCTIONS.md#playback-advance)

**관련 Contract:** [비동기 근거](../50_CONTRACTS/00_CONTRACT_OVERVIEW.md#async) · [소유·수명](../50_CONTRACTS/00_CONTRACT_OVERVIEW.md#ownership) · [시간·최신성](../50_CONTRACTS/00_CONTRACT_OVERVIEW.md#timing)

**논리 Header(R6):** `H-TX`.

### Pseudo-C

**구성원 타입 → 정의 위치**

- 의미 alias: `operation` → [VssOperationKeyType](#vssoperationkeytype); `attempt` → [VssAttemptKeyType](40_PLAYBACK_DATA.md#vssattemptkeytype).
- enum: `kind` → [VssTxOutputFactKind](#vsstxoutputfactkind); `scope` → [VssOutputScope](#vssoutputscope).
- 구조체 값: `occurredAt` → [VssFactTime](10_INPUT_DATA.md#vssfacttime); `coverage` → [VssObservationCoverage](#vssobservationcoverage).

```c
typedef struct
{
    VssOperationKeyType operation;
    VssAttemptKeyType attempt;
    VssTxOutputFactKind kind;
    VssOutputScope scope;
    VssFactTime occurredAt;
    VssObservationCoverage coverage;
} VssTxOutputEvidence;
```



<a id="vsspcmconsumptionevidence"></a>
## VssPcmConsumptionEvidence
**Category: Evidence**

**선언 종류:** 구조체 (`struct`).

해당 PCM 구간의 source 소비 사실이다. CPU safe return이나 speaker 출력 종료가 아니므로 별도 record다.

**Owner:** AUDIO TX  
**Producer:** AudioTx_Advance  
**Consumer:** AudioStream_Advance

### 사람이 읽는 Field 정의

| Field | 의미 / 필요한 이유 | Field 생성 주체 | 유효 조건 |
| --- | --- | --- | --- |
| `operation` | 소비가 일어난 원래 작업. | 사전 보호된 소프트웨어 귀속/operation과 raw 관측의 대응 | 귀속 확인 뒤 |
| `key` | 소비한 A/B의 원래 회차. 지속 등록이나 Buffer ID만으로 추정하지 않는다. | 사전 보호된 원래 PCM 인계/회차 | 해당 사건과의 대응이 확인된 회차만 |
| `firstFrame` | 소비한 원본 구간 시작. | 실제 하위 범위 근거 | 확인 범위만 |
| `frameCount` | 실제 소비 확인 frame 수. | 실제 하위 범위 근거 | 확인 범위만; 전체 capacity로 확장 금지 |
| `occurredAt` | 소비 사실 포착 시각. | HAL/실제 하위 관측 | 원래 시간 문맥 |

### Lifetime / Mutability / 폐기

생성 뒤 불변이고 원래 PCM 회차에만 적용한다. cyclic/재접근 가능성이 있으면 소비 뒤에도 device 보호를 유지한다. stop 뒤 소비/반환으로 provider 생산 권한을 복원하지 않는다.

**관련 Function:** [AudioTx_Advance](../30_FUNCTIONS/50_DRIVER_HAL_FUNCTIONS.md#audiotx-advance) · [AudioStream_Advance](../30_FUNCTIONS/40_AUDIO_STREAM_ASSET_FUNCTIONS.md#audiostream-advance)

**관련 Contract:** [PCM·전송](../50_CONTRACTS/00_CONTRACT_OVERVIEW.md#buffer) · [비동기 근거](../50_CONTRACTS/00_CONTRACT_OVERVIEW.md#async)

**논리 Header(R6):** `H-TX`.

### Pseudo-C

**구성원 타입 → 정의 위치**

- 의미 alias: `operation` → [VssOperationKeyType](#vssoperationkeytype).
- 구조체 값: `key` → [VssPcmCycleKey](50_AUDIO_STREAM_DATA.md#vsspcmcyclekey); `occurredAt` → [VssFactTime](10_INPUT_DATA.md#vssfacttime).
- pseudo-scalar: `firstFrame`, `frameCount` → [SemanticFrameCount](00_DATA_OVERVIEW.md#pseudo-conventions).

```c
typedef struct
{
    VssOperationKeyType operation;
    VssPcmCycleKey key;
    SemanticFrameCount firstFrame;
    SemanticFrameCount frameCount;
    VssFactTime occurredAt;
} VssPcmConsumptionEvidence;
```



<a id="vsspcmreturnevidence"></a>
## VssPcmReturnEvidence
**Category: Evidence**

**선언 종류:** 구조체 (`struct`).

옛 작업/PCM 사용 회차의 참조 종료·내용 재접근 금지 또는 명확한 무접근 거부를 입증한다. 같은 A/B 물리 주소의 차기 DMA 사용까지 영구히 금지한다는 뜻은 아니다. 소비 IRQ/abort 반환과 다른 안전 근거다.

**Owner:** AUDIO TX  
**Producer:** AudioTx_Request의 명확한 무접근 거부 또는 AudioTx_Advance  
**Consumer:** AudioStream_Advance / 즉시 요청 결과 적용

### 사람이 읽는 Field 정의

| Field | 의미 / 필요한 이유 | Field 생성 주체 | 유효 조건 |
| --- | --- | --- | --- |
| `operation` | 반환/무접근 거부의 원래 작업. | AUDIO TX | 귀속 확인 뒤 |
| `key` | 반환할 정확한 시도/A/B/회차. | 사전 보호된 원래 소프트웨어 귀속/인계 | 해당 회차만 |
| `noFutureAccess` | 해당 옛 작업/회차의 참조 종료와 옛 내용의 재접근 금지가 입증됨. 물리 주소의 차기 회차 접근 불가를 뜻하지 않는다. 유효 ReturnEvidence에서는 true여야 한다. | AUDIO TX의 실제 범위 확인 | 옛 retain/활성/대기 참조와 반복 접근 위험의 종료·차단이 입증된 범위만 |
| `basis` | 그 원래 회차의 안전 반환/무접근 거부를 확인한 scope/구성·관측 근거. | AUDIO TX/HAL 실제 근거 | 옛 내용의 재접근 방지와 시간 제한을 포함한 실제 충분조건을 확인한 경우만 |

### Lifetime / Mutability / 폐기

충분한 원래 회차 근거가 있을 때만 생성한다. AUDIO STREAM이 같은 key에 적용해 usage를 회수할 때까지 보호한다. 옛 A로 새 A/B를 반환하지 않는다. CPU 사용권 회수가 provider refill/재제출 권한 복원을 의미하지 않는다.

C1-03의 옛 내용 재접근 금지와 차기 방문 전 현재 쓰기 조건은 [Buffer §4](../50_CONTRACTS/50_AUDIO_BUFFER_TRANSPORT.md#safe-write)를 따른다. 이 Evidence의 `operation`/`key`/`basis`는 해당 옛 회차만 증명하며 AUDIO STREAM이 적용할 현재 회차를 해제하는 무기한 허가가 아니다. 적용 시 안전 구간 경과/대응 불명확이면 현재 usage 회수 근거로 쓰지 않는다. 반환과 현재 갱신 안전을 함께 입증하는 방법·기한 경과 처리는 B2-R/보드 TBD다. 출력 종료/Session retirement는 [네 경계](../50_CONTRACTS/50_AUDIO_BUFFER_TRANSPORT.md#four-boundaries)의 별도 근거를 요구한다.

**관련 Function:** [AudioTx_Request](../30_FUNCTIONS/50_DRIVER_HAL_FUNCTIONS.md#audiotx-request) · [AudioTx_Advance](../30_FUNCTIONS/50_DRIVER_HAL_FUNCTIONS.md#audiotx-advance) · [AudioStream_Advance](../30_FUNCTIONS/40_AUDIO_STREAM_ASSET_FUNCTIONS.md#audiostream-advance)

**관련 Contract:** [PCM·전송](../50_CONTRACTS/00_CONTRACT_OVERVIEW.md#buffer) · [소유·수명](../50_CONTRACTS/00_CONTRACT_OVERVIEW.md#ownership) · [비동기 근거](../50_CONTRACTS/00_CONTRACT_OVERVIEW.md#async)

**논리 Header(R6):** `H-TX`.

### Pseudo-C

**구성원 타입 → 정의 위치**

- 의미 alias: `operation` → [VssOperationKeyType](#vssoperationkeytype).
- 구조체 값: `key` → [VssPcmCycleKey](50_AUDIO_STREAM_DATA.md#vsspcmcyclekey); `basis` → [VssObservationCoverage](#vssobservationcoverage).
- pseudo-scalar: `noFutureAccess` → [SemanticBool](00_DATA_OVERVIEW.md#pseudo-conventions).

```c
typedef struct
{
    VssOperationKeyType operation;
    VssPcmCycleKey key;
    SemanticBool noFutureAccess;
    VssObservationCoverage basis;
} VssPcmReturnEvidence;
```



## VssPcmTransferIntent / VssTxProgress — SPLIT

인계는 VssPcmHandoffCommand, start/정리는 VssTxControlCommand다. 주소/유효 PCM의 보호 수명과 출력 제어 요구 수명이 달라 별도 typed 입력이며 START/STOP끼리는 공통 shape를 유지한다. `AudioTx_Request`는 임의 재설계하지 않았고 typed 입력 표현의 국소 검토를 남겼다.

VssTxProgress는 VssTxRequestResult, VssTxOutputEvidence, VssPcmConsumptionEvidence, VssPcmReturnEvidence, VssDiagnosticEvidence로 나눈다. 각각 원래 identity와 보존 의무를 가지며 마지막 Progress struct/union으로 합치지 않는다. 여러 사실이 같은 처리 기회에 있으면 독립적으로 보존하고 소비 완료까지 덮어쓰지 않는다. 실제 전달/저장 수단은 후속 Binding 대상이다.

<a id="vsshaltxregistration"></a>
## VssHalTxRegistration
**Category: Context**

**선언 종류:** 구조체 (`struct`).

개별 AUDIO TX 요구의 operation/Attempt·PCM 회차를 사전에 보호하는 불변 **소프트웨어 귀속**이다. vendor의 지속 스트림 등록 전체를 이 record 하나로 표현하는 타입이 아니다. callback 처리 시 현재 Attempt/Buffer를 읽어 원래 identity를 재생산하지 않도록 한다.

**Owner:** HAL/BSP Layer의 HAL Boundary  
**Producer:** HAL의 기존 귀속 보호 경계. AUDIO TX가 원래 의미를 제공한다.  
**Consumer:** AudioHAL_Callback / AudioTx_Advance

### 사람이 읽는 Field 정의

| Field | 의미 / 필요한 이유 | Field 생성 주체 | 유효 조건 |
| --- | --- | --- | --- |
| `registration` | 해당 소프트웨어 귀속 기록의 연결 identity. vendor 등록 key와 1:1 동일하다고 가정하지 않는다. | HAL Boundary | 해당 작업이 효력을 갖기 전에 보호 |
| `operation` | 원래 개별 AUDIO TX 요청. vendor 지속 스트림 자체의 수명으로 바꾸지 않는다. | AUDIO TX → HAL 귀속 보호 | 보호 뒤 불변 |
| `attempt` | 원래 실제 시도. 이 타입을 적용하는 개별 요청에는 필수이며 지속 등록에 가짜/최신 Attempt를 채우지 않는다. | AUDIO TX → HAL 귀속 보호 | 보호 뒤 불변 |
| `pcm` | PCM 작업이면 원래 회차/구간의 독립 보호된 불변 인계 기록. 제어 전용이면 없음. | HAL이 효력 전에 보호 | 해당 작업의 HW/대기/처리·후속 사실 참조 수명 전체 |
| `binding` | 해당 작업 당시 실제 구성/source/build 문맥. 새 구성으로 덮지 않는다. | HAL/BSP 근거 | 해당 소프트웨어 귀속 수명 |

### Lifetime / Mutability / 폐기

귀속 필드는 보호 뒤 불변이다. 해당 작업의 HW·대기·처리 중 소프트웨어 참조와 late 사실의 대응 가능성이 모두 끝난 근거까지 유지한다. PCM 안전 반환/Session retirement/IRQ disable만으로 폐기하지 않는다. alias 없는 보호 공간·옛 출력/자원 종료·현재 쓰기 안전 조건이 확인되면 옛 귀속을 남긴 채 같은 A/B 주소를 다음 회차에 사용할 수 있다. 공간 부족은 새 진행 제한이며 무제한 pool/임의 eviction을 만들지 않는다.

### 지속 스트림 등록과 개별 음향 귀속 — C1-02

vendor 지속 등록과 개별 software 귀속의 공통 조건은 [Buffer C1-02](../50_CONTRACTS/50_AUDIO_BUFFER_TRANSPORT.md#continuous-stream)을 따른다. 이 record의 `attempt`는 개별 실제 software 요청에 필수이고 PCM 작업의 `pcm`만 존재한다. [VssRawObservation](#vssrawobservation)과 원래 [VssTxOperation](#vsstxoperation)/인계 key의 실제 대응을 검증해 적용하며 현재 Attempt로 보충하지 않는다.

`registration`은 불변 논리 연결 표기이며 vendor token/실제 등록 key와 1:1을 보장하지 않는다. attempt optional화·새 transport struct/등록 API/record 수/ledger를 추가하지 않는다. 실물 저장 모양·key lookup·callback signature·회차 대응·각 참조 종료는 B2-R TBD다.

**관련 Function:** [AudioTx_Request](../30_FUNCTIONS/50_DRIVER_HAL_FUNCTIONS.md#audiotx-request) · [AudioHAL_Callback](../30_FUNCTIONS/50_DRIVER_HAL_FUNCTIONS.md#audiohal-callback) · [AudioTx_Advance](../30_FUNCTIONS/50_DRIVER_HAL_FUNCTIONS.md#audiotx-advance)

**관련 Contract:** [소유·수명](../50_CONTRACTS/00_CONTRACT_OVERVIEW.md#ownership) · [비동기 근거](../50_CONTRACTS/00_CONTRACT_OVERVIEW.md#async) · [PCM·전송](../50_CONTRACTS/00_CONTRACT_OVERVIEW.md#buffer)

**논리 Header(R6):** `H-HAL`.

### Pseudo-C

**구성원 타입 → 정의 위치**

- 의미 alias: `registration` → [VssRegistrationKeyType](#vssregistrationkeytype); `operation` → [VssOperationKeyType](#vssoperationkeytype); `attempt` → [VssAttemptKeyType](40_PLAYBACK_DATA.md#vssattemptkeytype); `binding` → [VssBindingKeyType](#vssbindingkeytype).
- 구조체 읽기 참조 (`const T *`): `pcm` → [VssPcmHandoffCommand](#vsspcmhandoffcommand).

```c
typedef struct
{
    VssRegistrationKeyType registration;
    VssOperationKeyType operation;
    VssAttemptKeyType attempt;
    const VssPcmHandoffCommand * pcm;
    VssBindingKeyType binding;
} VssHalTxRegistration;
```



<a id="vsshaldeviceregistration"></a>
## VssHalDeviceRegistration
**Category: Context**

**선언 종류:** 구조체 (`struct`).

장치 제어의 원래 장치/구성/operation을 보호한다. PCM 귀속과 필드/참조 대상이 달라 HAL 등록을 union-of-everything으로 만들지 않는다.

**Owner:** HAL Boundary  
**Producer:** HAL의 기존 device 제어 등록 경계  
**Consumer:** AudioHAL_Callback / AudioControl_Service

### 사람이 읽는 Field 정의

| Field | 의미 / 필요한 이유 | Field 생성 주체 | 유효 조건 |
| --- | --- | --- | --- |
| `registration` | 원래 callback/제어 등록의 연결. | HAL | vendor 요청 전 |
| `operation` | 원래 제어 요구. | AUDIO CONTROL → HAL | 등록 뒤 불변 |
| `device` | 원래 Codec/Generator/Reference. | AUDIO CONTROL → HAL | 등록 뒤 불변 |
| `configuration` | 원래 장치 구성 identity. | AUDIO CONTROL → HAL | 등록 뒤 불변 |
| `binding` | 당시 제어의 실제 source/build 문맥. | HAL/BSP | 해당 등록 수명 |

### Lifetime / Mutability / 폐기

원래 등록 뒤 불변이며 모든 제어/callback·대기·처리 중 참조 종료까지 유지한다. 옛 제어 성공은 새 configuration readiness/새 Fault 해제 근거가 아니다. 실제 callback signature·귀속 lookup 방식은 Binding TBD다.

**관련 Function:** [AudioControl_Service](../30_FUNCTIONS/50_DRIVER_HAL_FUNCTIONS.md#audiocontrol-service) · [AudioHAL_Callback](../30_FUNCTIONS/50_DRIVER_HAL_FUNCTIONS.md#audiohal-callback)

**관련 Contract:** [소유·수명](../50_CONTRACTS/00_CONTRACT_OVERVIEW.md#ownership) · [비동기 근거](../50_CONTRACTS/00_CONTRACT_OVERVIEW.md#async)

**논리 Header(R6):** `H-HAL`.

### Pseudo-C

**구성원 타입 → 정의 위치**

- 의미 alias: `registration` → [VssRegistrationKeyType](#vssregistrationkeytype); `operation` → [VssOperationKeyType](#vssoperationkeytype); `configuration` → [VssConfigurationKeyType](#vssconfigurationkeytype); `binding` → [VssBindingKeyType](#vssbindingkeytype).
- enum: `device` → [VssDeviceKind](#vssdevicekind).

```c
typedef struct
{
    VssRegistrationKeyType registration;
    VssOperationKeyType operation;
    VssDeviceKind device;
    VssConfigurationKeyType configuration;
    VssBindingKeyType binding;
} VssHalDeviceRegistration;
```



<a id="vssrawobservation"></a>
## VssRawObservation
**Category: Evidence**

**선언 종류:** 구조체 (`struct`).

원래 등록에 연결된 해석 전 장치 사실·시각/관측 범위·손실이다. Session 결과/CPU 사용권을 HAL이 판정하지 않는다.

**Owner:** HAL Boundary raw 포착  
**Producer:** vendor/실제 관측 → AudioHAL_Callback  
**Consumer:** AudioTx_Advance 또는 AudioControl_Service

### 사람이 읽는 Field 정의

| Field | 의미 / 필요한 이유 | Field 생성 주체 | 유효 조건 |
| --- | --- | --- | --- |
| `registration` | 실제 관측과 연결된 원래 transport/device 등록 또는 입증된 소프트웨어 귀속 key. 지속 스트림 key 자체는 개별 Attempt/PCM 회차가 아니다. 확인 불가면 없음이며 현재 key를 추정하지 않는다. | 원래 callback/관측 귀속 근거 → HAL | 안전하게 확인 가능한 등록 범위만 |
| `rawFact` | 해석 전 원래 장치 사실. 실제 vendor type/bit를 이번에 고정하지 않는다. | 실제 관측 주체 | 실제로 포착한 사실 |
| `occurredAt` | 원래 사건의 시각/가능 구간. | 실제 관측/Runtime 포착 | 정밀 시각 불가면 구간/연속성 부족 보존 |
| `capturedAt` | HAL이 사실을 포착한 시간 근거. 사건/후속 적용 시각과 구별한다. | HAL/Runtime | 실제 포착 시점 |
| `coverage` | 관측 경계/기간·귀속/저장/시간 손실. | HAL | 확인 범위만; 부족/손실 드러냄 |

### Lifetime / Mutability / 폐기

ISR은 짧게 포착·보호하고 기존 후속 처리를 알린다. 사실은 소비/보호 추적 완료까지, 등록은 별도 참조 종료까지 유지한다. 알림 합침/overflow가 사실 조용한 삭제를 뜻하면 안 된다. raw storage 부족이면 손실을 남겨 상위가 성공/미시작/종료를 확정하지 못하게 한다. 실제 저장 수단은 미정이다.

`registration`의 `const VssRegistrationKeyType *`는 [의미 alias](#vssregistrationkeytype) 값의 읽기 참조다. 구조체 전체를 가리키는 표기가 아니다. 원귀속 확인 불가 시 없음이며, key와 원 등록 기록은 위의 별도 참조 종료 조건을 따른다.

raw 사실이 지속 스트림 등록만 식별한 경우에는 그 사실을 그대로 보존한다. 개별 음향으로의 적용은 [사전 소프트웨어 귀속과의 검증](#vsshaltxregistration)이 추가로 필요하며 HAL이 최신 Session/Attempt를 붙이지 않는다. 실제 대응 방법은 B2-R TBD다.

**관련 Function:** [AudioHAL_Callback](../30_FUNCTIONS/50_DRIVER_HAL_FUNCTIONS.md#audiohal-callback) · [AudioTx_Advance](../30_FUNCTIONS/50_DRIVER_HAL_FUNCTIONS.md#audiotx-advance) · [AudioControl_Service](../30_FUNCTIONS/50_DRIVER_HAL_FUNCTIONS.md#audiocontrol-service)

**관련 Contract:** [비동기 근거](../50_CONTRACTS/00_CONTRACT_OVERVIEW.md#async) · [시간·최신성](../50_CONTRACTS/00_CONTRACT_OVERVIEW.md#timing)

**논리 Header(R6):** `H-HAL`.

### Pseudo-C

**구성원 타입 → 정의 위치**

- 의미 alias 읽기 참조 (`const T *`): `registration` → [VssRegistrationKeyType](#vssregistrationkeytype).
- pseudo-scalar: `rawFact` → [RawFactValue](00_DATA_OVERVIEW.md#pseudo-conventions).
- 구조체 값: `occurredAt` → [VssFactTime](10_INPUT_DATA.md#vssfacttime); `capturedAt` → [VssTimeEvidence](10_INPUT_DATA.md#vsstimeevidence); `coverage` → [VssObservationCoverage](#vssobservationcoverage).

```c
typedef struct
{
    const VssRegistrationKeyType * registration;
    RawFactValue rawFact;
    VssFactTime occurredAt;
    VssTimeEvidence capturedAt;
    VssObservationCoverage coverage;
} VssRawObservation;
```



<a id="control"></a>
## 장치별 구성 준비/제어 — AUDIO CONTROL

<a id="vssdevicekind"></a>
## VssDeviceKind — scalar enum
**Category: Descriptor**

한 장치의 준비 성공을 다른 장치에 확장하지 않기 위한 대상 구별이다.

**Owner:** AUDIO CONTROL의 장치 의미 · **Producer:** 현재 장치 요구/구성 · **Consumer:** AUDIO CONTROL/HAL/AUDIO STREAM

| 값 | 의미 / 유효 조건 |
| --- | --- |
| `VSS_DEVICE_CODEC` | SGTL5000 Codec |
| `VSS_DEVICE_CLOCK_GENERATOR` | CS2100 Clock Generator |
| `VSS_DEVICE_CLOCK_REFERENCE` | Clock Reference source |

**Lifetime / Mutability:** 해당 장치·구성 문맥.

**관련 Function:** [AudioControl_Service](../30_FUNCTIONS/50_DRIVER_HAL_FUNCTIONS.md#audiocontrol-service) · [AudioStream_Prepare](../30_FUNCTIONS/40_AUDIO_STREAM_ASSET_FUNCTIONS.md#audiostream-prepare)

**관련 Contract:** [소유·수명](../50_CONTRACTS/00_CONTRACT_OVERVIEW.md#ownership)

**논리 Header(R6):** `H-CONTROL`.

```c
typedef enum
{
    VSS_DEVICE_CODEC,
    VSS_DEVICE_CLOCK_GENERATOR,
    VSS_DEVICE_CLOCK_REFERENCE
} VssDeviceKind;
```

<a id="vssdeviceaction"></a>
## VssDeviceAction — scalar enum
**Category: Command**

하나의 지속되는 장치/구성 준비 lifecycle에 필요한 제한된 동작이다.

**Owner:** AUDIO STREAM 요구 / AUDIO CONTROL 적용 · **Producer:** AUDIO STREAM · **Consumer:** AudioControl_Service

| 값 | 의미 / 유효 조건 |
| --- | --- |
| `VSS_DEVICE_PREPARE` | 현재 구성의 준비 확인/필요 제어 |
| `VSS_DEVICE_INVALIDATE` | 영향받은 기존 준비 근거 무효화/정리 |
| `VSS_DEVICE_RECOVER` | 현재 대상의 HEALTH 허용 범위에서 실제 제어 복구 |

**Lifetime / Mutability:** 요구 제출부터 원래 control 결과 수명. 실제 제어 순서는 TBD.

**관련 Function:** [AudioStream_Prepare](../30_FUNCTIONS/40_AUDIO_STREAM_ASSET_FUNCTIONS.md#audiostream-prepare) · [AudioStream_RequestControl](../30_FUNCTIONS/40_AUDIO_STREAM_ASSET_FUNCTIONS.md#audiostream-requestcontrol) · [AudioStream_Advance](../30_FUNCTIONS/40_AUDIO_STREAM_ASSET_FUNCTIONS.md#audiostream-advance) · [AudioControl_Service](../30_FUNCTIONS/50_DRIVER_HAL_FUNCTIONS.md#audiocontrol-service)

**관련 Contract:** [소유·수명](../50_CONTRACTS/00_CONTRACT_OVERVIEW.md#ownership) · [고장·복구](../50_CONTRACTS/00_CONTRACT_OVERVIEW.md#fault)

**논리 Header(R6):** `H-CONTROL`.

```c
typedef enum
{
    VSS_DEVICE_PREPARE,
    VSS_DEVICE_INVALIDATE,
    VSS_DEVICE_RECOVER
} VssDeviceAction;
```

<a id="vssdevicecontrolintent"></a>
## VssDeviceControlIntent
**Category: Command**

**선언 종류:** 구조체 (`struct`).

동일 장치/구성의 준비·무효화/허용 복구 요구다. 진행만 필요한 호출은 새 Intent 없이 기존 control 문맥과 시간으로 처리한다.

**Owner:** AUDIO STREAM 요구 의미 / AUDIO CONTROL의 실제 제어  
**Producer:** AUDIO STREAM Prepare/RequestControl/Advance  
**Consumer:** AudioControl_Service

### 사람이 읽는 Field 정의

| Field | 의미 / 필요한 이유 | Field 생성 주체 | 유효 조건 |
| --- | --- | --- | --- |
| `operation` | 원래 제어의 인과 요구 identity. 반복 진행은 같은 identity를 유지한다. | AUDIO STREAM 요구 생성 | 제출 전 보호; 결과/정리까지 |
| `device` | Codec/Generator/Reference 대상. | AUDIO STREAM의 현재 구성 요구 | 해당 device 문맥 |
| `configuration` | 대상 구성 identity. 옛 성공을 새 ready로 쓰지 않는다. | 현재 구성 의미 | 원래 제어 수명 |
| `action` | 준비/무효화/복구의 제한된 동작. | AUDIO STREAM | 원래 요구 수명 |
| `recoveryPermission` | RECOVER에 필수인 현재 대상 허용. PREPARE/INVALIDATE에는 없음. | HEALTH → FLOW → AUDIO STREAM 보호 | 현재 대상/범위에 유효한 허용일 때만 |

### Lifetime / Mutability / 폐기

제출 뒤 요구 의미는 불변이고 callee가 필요한 자료를 자기 control 문맥으로 보호한다. RECOVER 한 conditional 참조만 있으며 다른 유형별 payload를 모두 넣지 않는다. 장치 준비는 여러 Attempt에 걸쳐 유지되고 새 Attempt마다 전체 재초기화하지 않는다. 미평가는 초기화 실패가 아니다.

**관련 Function:** [AudioStream_Prepare](../30_FUNCTIONS/40_AUDIO_STREAM_ASSET_FUNCTIONS.md#audiostream-prepare) · [AudioStream_RequestControl](../30_FUNCTIONS/40_AUDIO_STREAM_ASSET_FUNCTIONS.md#audiostream-requestcontrol) · [AudioStream_Advance](../30_FUNCTIONS/40_AUDIO_STREAM_ASSET_FUNCTIONS.md#audiostream-advance) · [AudioControl_Service](../30_FUNCTIONS/50_DRIVER_HAL_FUNCTIONS.md#audiocontrol-service)

**관련 Contract:** [소유·수명](../50_CONTRACTS/00_CONTRACT_OVERVIEW.md#ownership) · [고장·복구](../50_CONTRACTS/00_CONTRACT_OVERVIEW.md#fault)

**논리 Header(R6):** `H-CONTROL`.

### Pseudo-C

**구성원 타입 → 정의 위치**

- 의미 alias: `operation` → [VssOperationKeyType](#vssoperationkeytype); `configuration` → [VssConfigurationKeyType](#vssconfigurationkeytype).
- enum: `device` → [VssDeviceKind](#vssdevicekind); `action` → [VssDeviceAction](#vssdeviceaction).
- 구조체 읽기 참조 (`const T *`): `recoveryPermission` → [VssRecoveryPermission](70_DIAGNOSTIC_DATA.md#vssrecoverypermission).

```c
typedef struct
{
    VssOperationKeyType operation;
    VssDeviceKind device;
    VssConfigurationKeyType configuration;
    VssDeviceAction action;
    const VssRecoveryPermission * recoveryPermission;
} VssDeviceControlIntent;
```



<a id="vssdevicereadystate"></a>
## VssDeviceReadyState — scalar enum
**Category: State**

현재 해당 장치/구성의 준비 상태다. 실제 speaker 출력 성공/무출력 증명과 구별한다.

**Owner:** AUDIO CONTROL · **Producer:** AudioControl_Service · **Consumer:** AUDIO STREAM

| 값 | 의미 / 유효 조건 |
| --- | --- |
| `VSS_DEVICE_UNASSESSED` | 아직 현재 구성 준비 미평가 |
| `VSS_DEVICE_PREPARING` | 현재 제어/확인 진행 |
| `VSS_DEVICE_READY` | 현재 구성의 실제 준비 확인 |
| `VSS_DEVICE_INVALIDATED` | reset/reconfig/오류로 기존 준비 무효 |
| `VSS_DEVICE_FAILED` | 확인된 현재 준비/제어 실패 |

**Lifetime / Mutability:** 구성/참조 수명. 같은 장치의 옛 성공으로 새 READY를 만들지 않는다.

**관련 Function:** [AudioControl_Service](../30_FUNCTIONS/50_DRIVER_HAL_FUNCTIONS.md#audiocontrol-service) · [AudioStream_Prepare](../30_FUNCTIONS/40_AUDIO_STREAM_ASSET_FUNCTIONS.md#audiostream-prepare)

**관련 Contract:** [소유·수명](../50_CONTRACTS/00_CONTRACT_OVERVIEW.md#ownership)

**논리 Header(R6):** `H-CONTROL`.

```c
typedef enum
{
    VSS_DEVICE_UNASSESSED,
    VSS_DEVICE_PREPARING,
    VSS_DEVICE_READY,
    VSS_DEVICE_INVALIDATED,
    VSS_DEVICE_FAILED
} VssDeviceReadyState;
```

<a id="vssdevicereadinessstate"></a>
## VssDeviceReadinessState
**Category: State**

**선언 종류:** 구조체 (`struct`).

장치마다 독립인 현재 구성 준비 원본이다. 여러 장치 원본을 한 전체 READY boolean으로 덮지 않는다.

**Owner:** AUDIO CONTROL 단일 writer  
**Producer:** AudioControl_Service  
**Consumer:** AUDIO STREAM의 준비·제한/HEALTH의 진단 관측

### 사람이 읽는 Field 정의

| Field | 의미 / 필요한 이유 | Field 생성 주체 | 유효 조건 |
| --- | --- | --- | --- |
| `device` | 이 readiness의 원래 장치. | AUDIO CONTROL | 해당 장치 문맥 |
| `configuration` | 현재 구성 identity. | AUDIO CONTROL | 해당 구성 문맥 |
| `state` | 미평가/진행/확인/무효/실패. | AUDIO CONTROL | 실제 현재 구성 결과 반영 뒤 |
| `revision` | 반영 완료·구성 변화 읽기 표지. | AUDIO CONTROL | state/구성 변경 완료 뒤 |

### Lifetime / Mutability / 폐기

구성 준비가 유지되면 Session보다 오래 존재한다. reset/reconfig/오류는 영향 장치/구성의 준비를 무효화한다. control callback 기록 회수는 독립 참조 수명이고 이 state 변경/무효화만으로 폐기하지 않는다. 한 장치의 성공으로 다른 device state를 갱신하지 않는다.

**관련 Function:** [AudioControl_Service](../30_FUNCTIONS/50_DRIVER_HAL_FUNCTIONS.md#audiocontrol-service) · [AudioStream_Prepare](../30_FUNCTIONS/40_AUDIO_STREAM_ASSET_FUNCTIONS.md#audiostream-prepare) · [AudioStream_Advance](../30_FUNCTIONS/40_AUDIO_STREAM_ASSET_FUNCTIONS.md#audiostream-advance) · [Health_Evaluate](../30_FUNCTIONS/60_HEALTH_RUNTIME_FUNCTIONS.md#health-evaluate)

**관련 Contract:** [소유·수명](../50_CONTRACTS/00_CONTRACT_OVERVIEW.md#ownership) · [고장·복구](../50_CONTRACTS/00_CONTRACT_OVERVIEW.md#fault)

**논리 Header(R6):** `H-CONTROL`.

### Pseudo-C

**구성원 타입 → 정의 위치**

- enum: `device` → [VssDeviceKind](#vssdevicekind); `state` → [VssDeviceReadyState](#vssdevicereadystate).
- 의미 alias: `configuration` → [VssConfigurationKeyType](#vssconfigurationkeytype); `revision` → [VssRevisionType](10_INPUT_DATA.md#vssrevisiontype).

```c
typedef struct
{
    VssDeviceKind device;
    VssConfigurationKeyType configuration;
    VssDeviceReadyState state;
    VssRevisionType revision;
} VssDeviceReadinessState;
```



<a id="vssdevicecontrolresult"></a>
## VssDeviceControlResult
**Category: Result**

**선언 종류:** 구조체 (`struct`).

원래 제어 요구의 수용/진행/확인·실패를 반환한다. 실제 복구 수행·별도 효과 검증은 따로 전달한다.

**Owner:** AUDIO CONTROL  
**Producer:** AudioControl_Service  
**Consumer:** AUDIO STREAM

### 사람이 읽는 Field 정의

| Field | 의미 / 필요한 이유 | Field 생성 주체 | 유효 조건 |
| --- | --- | --- | --- |
| `operation` | 원래 control 요구. | AUDIO CONTROL 원래 문맥 | 원래 제어 수명 |
| `device` | 해당 장치만. | 원래 제어 문맥 | 원래 대상 수명 |
| `configuration` | 원래 구성. | 원래 제어 문맥 | 원래 결과 적용/격리까지 |
| `state` | 실제 반영한 준비/진행·무효/실패 의미. | AUDIO CONTROL | 해당 구성 결과 반영 뒤 |

### Lifetime / Mutability / 폐기

생성 뒤 불변이고 AUDIO STREAM 반영/보호 추적까지 유지한다. failure 단계/원인은 별도 DiagnosticEvidence로 보존한다. 준비 성공/제어 return을 actual/no-start/전체 종료 또는 HEALTH clear로 합성하지 않는다.

**관련 Function:** [AudioControl_Service](../30_FUNCTIONS/50_DRIVER_HAL_FUNCTIONS.md#audiocontrol-service) · [AudioStream_Advance](../30_FUNCTIONS/40_AUDIO_STREAM_ASSET_FUNCTIONS.md#audiostream-advance) · [AudioStream_Prepare](../30_FUNCTIONS/40_AUDIO_STREAM_ASSET_FUNCTIONS.md#audiostream-prepare)

**관련 Contract:** [소유·수명](../50_CONTRACTS/00_CONTRACT_OVERVIEW.md#ownership) · [비동기 근거](../50_CONTRACTS/00_CONTRACT_OVERVIEW.md#async) · [고장·복구](../50_CONTRACTS/00_CONTRACT_OVERVIEW.md#fault)

**논리 Header(R6):** `H-CONTROL`.

### Pseudo-C

**구성원 타입 → 정의 위치**

- 의미 alias: `operation` → [VssOperationKeyType](#vssoperationkeytype); `configuration` → [VssConfigurationKeyType](#vssconfigurationkeytype).
- enum: `device` → [VssDeviceKind](#vssdevicekind); `state` → [VssDeviceReadyState](#vssdevicereadystate).

```c
typedef struct
{
    VssOperationKeyType operation;
    VssDeviceKind device;
    VssConfigurationKeyType configuration;
    VssDeviceReadyState state;
} VssDeviceControlResult;
```



## VssDeviceReadiness / VssHalRegistration — SPLIT

VssDeviceReadiness는 지속되는 `VssDeviceReadinessState`, 원래 요구의 `VssDeviceControlResult`, 원래 발견 단계의 `VssDiagnosticEvidence`, 실제 복구 수행/별도 검증의 Evidence로 나눈다. Session 없이 startup device 제어가 진행될 수 있으며 준비 없음 자체를 INITIALIZATION_FAILURE로 만들지 않는다.

VssHalRegistration은 VssHalTxRegistration과 VssHalDeviceRegistration으로 나눈다. 이 SPLIT 판정은 유지하되 개별 TX 소프트웨어 귀속과 vendor 지속 스트림 등록을 1:1로 강제하지 않는다. callback의 공통 연결은 VssRegistrationKeyType scalar이며 실제 관측에 연결된 원래 문맥과 필요한 개별 귀속을 검증한다. 실제 C dispatch/signature·등록 보존/대응 수단은 확정하지 않는다. Session의 output retirement와 callback 기록 수명이 달라 등록을 현재 context 포인터로 대체하지 않는다.

<a id="scalar-aliases"></a>
## 보조 scalar / 불투명 의미 표기

**선언 종류:** 아래 5개는 `typedef Semantic... Vss...Type` 형태의 의미 alias다. 기저 `SemanticIdentity` / `SemanticBoundary`는 [pseudo-scalar](00_DATA_OVERVIEW.md#pseudo-conventions)이며 구조체·enum과 구별한다. 각각 독립 struct/새 ID 생성 API를 만드는 목록이 아니다. 모든 값의 폭/encoding·확정 Header는 미정이다. 실제 producer의 원래 문맥에서 보호하고 전달 뒤 불변으로 사용한다. 회수/무효화는 해당 주 Data의 lifetime을 따른다.

| 표기 | Producer / 의미 owner | 목적 / validity | 논리 Header(R6) |
| --- | --- | --- | --- |
| <a id="vssoperationkeytype"></a>`VssOperationKeyType` | 해당 실제 요청 owner | 인과 하위 TX/device/복구 동작 key. owner 문맥과 함께 해석; 생성/alias 방지 방식 미정 | `H-TX` |
| <a id="vssobservationboundarytype"></a>`VssObservationBoundaryType` | 실제 관측 주체 | 관측한 물리/장치 범위. 충분조건은 Binding/보드 TBD | `H-HAL` |
| <a id="vssbindingkeytype"></a>`VssBindingKeyType` | HAL/BSP 현재 근거 | 원래 요청의 source/build/장치 구성 상관. actual symbol mapping 아님 | `H-HAL` |
| <a id="vssregistrationkeytype"></a>`VssRegistrationKeyType` | HAL Boundary | 실제 등록/raw 참조 또는 사전 소프트웨어 귀속을 연결하는 의미 key. 둘의 1:1 동일성/구체 대응은 강제하지 않으며 현재 operation 대체 금지 | `H-HAL` |
| <a id="vssconfigurationkeytype"></a>`VssConfigurationKeyType` | 해당 구성 owner/실제 근거 | 원래 장치/오류/검증 구성 문맥. 실제 구성/변경 표현 미정 | `H-CONTROL` |

```c
typedef SemanticIdentity VssOperationKeyType;
typedef SemanticBoundary VssObservationBoundaryType;
typedef SemanticIdentity VssBindingKeyType;
typedef SemanticIdentity VssRegistrationKeyType;
typedef SemanticIdentity VssConfigurationKeyType;
```

위 label은 R6의 중립 leaf 선언 소유권이다. key의 생성·의미 책임은 표의 실제 producer에 남으며 선언 label 하나로 이전되지 않는다. [R6 타입 대응](../70_C_INTERFACE/00_HEADER_OWNERSHIP_MAP.md#type-map)의 확인 범위와 미정을 따르고 별도 공통 God Header를 만들지 않는다.

## 공통 TBD / 후속 범위

논리 필드의 실제 폭/숫자·alignment/packing/ABI·storage/복사·참조 수단과 Header filename은 미정이다. RTD/DMA/TCD·`.c/.h` Mapping·Task/Queue/Mutex·실제 C 구현은 작성하지 않았다. [pseudo-C 표기](00_DATA_OVERVIEW.md#pseudo-conventions)와 [기존 Implementation TBD](../90_BINDING/91_IMPLEMENTATION_TBD.md)를 함께 확인한다. 관련 계약은 기존 원문 링크이며 R4의 Cross Contract 원문 통합을 선행하지 않았다.
