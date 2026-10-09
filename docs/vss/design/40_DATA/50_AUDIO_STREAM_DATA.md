# Audio Stream / Asset Data — R3

> 2026-10-07 R3 · 2026-10-08 R4 공통 설명 연결 · R3-C1/국소 함수 독립 PASS 기준 · 필드/owner/선언·Binding TBD 유지

[Data Overview](00_DATA_OVERVIEW.md) · [R2 타입 판정](00_DATA_OVERVIEW.md#type-decisions)

<a id="asset"></a>
[ASSET](../20_MODULES/25_ASSET_MODULE.md)은 read-only Internal PFlash의 제한된 MP3 bytes와 자료 참조 수명을 소유한다. CPU decode·입력 소비 위치·PCM 내용/사용권은 [AUDIO STREAM](../20_MODULES/24_AUDIO_STREAM_MODULE.md)이 소유한다. MP3를 SGTL5000에 직접 전송하거나 별도 Storage/Decoder Module을 만들지 않는다.

<a id="vssassetdescriptor"></a>
## VssAssetDescriptor
**Category: Descriptor**

해당 image/build의 음원 식별·압축 길이/metadata를 설명한다. lookup/read 성공은 전체 MP3 decode 성공이 아니다.

**Owner:** ASSET  
**Producer:** 현재 read-only 이미지/배치 근거 → ASSET  
**Consumer:** Asset_Read / AUDIO STREAM / FLOW의 읽기 가용 근거

### 사람이 읽는 Field 정의

| Field | 의미 / 필요한 이유 | Field 생성 주체 | 유효 조건 |
| --- | --- | --- | --- |
| `asset` | 의미 음원 ID. 주소/RTD 번호가 아니다. | image/table → ASSET | 해당 image에서 확인된 ID |
| `image` | metadata/압축 위치가 귀속된 build/image. | 기존 이미지 근거 | 실제 image가 유지되는 동안 |
| `compressedLength` | 확인된 MP3 압축 길이. 범위/overflow 검증에 필요하다. | image metadata → ASSET 검증 | metadata 확인 후 |
| `metadata` | 기존 Asset format/지원 정보의 의미 Descriptor. 실제 bitrate/sample format은 TBD다. | image metadata → ASSET 검증 | 확인 범위에만 유효 |

### Lifetime / Mutability / 폐기

이미지 문맥 동안 불변이다. actual image가 바뀌면 옛 descriptor로 새 읽기를 허용하지 않는다. 물리 주소/direct Flash·staging/배치는 이번에 정하지 않는다.

**관련 Function:** [Asset_Read](../30_FUNCTIONS/40_AUDIO_STREAM_ASSET_FUNCTIONS.md#asset-read) · [AudioStream_Prepare](../30_FUNCTIONS/40_AUDIO_STREAM_ASSET_FUNCTIONS.md#audiostream-prepare) · [AudioStream_Advance](../30_FUNCTIONS/40_AUDIO_STREAM_ASSET_FUNCTIONS.md#audiostream-advance)

**관련 Contract:** [소유·수명](../50_CONTRACTS/00_CONTRACT_OVERVIEW.md#ownership) · [고장·복구](../50_CONTRACTS/00_CONTRACT_OVERVIEW.md#fault)

**Provisional owner header 후보:** ASSET Descriptor 인터페이스 후보 — R6에서 확정. 실제 파일명·신규 Header 필요 여부는 정하지 않는다.

### Pseudo-C

```c
typedef struct
{
    VssAssetKeyType asset;
    VssImageKeyType image;
    SemanticByteCount compressedLength;
    VssAssetMetadataType metadata;
} VssAssetDescriptor;
```



<a id="vssassetreadrequest"></a>
## VssAssetReadRequest
**Category: Request**

압축 자료의 제한된 범위를 요구한다. 참조 종료 요청은 이 Request의 offset/length에 억지로 넣지 않는다.

**Owner:** AUDIO STREAM 요구 / ASSET 검증  
**Producer:** AudioStream_Prepare 또는 AudioStream_Advance 내부 생산  
**Consumer:** Asset_Read

### 사람이 읽는 Field 정의

| Field | 의미 / 필요한 이유 | Field 생성 주체 | 유효 조건 |
| --- | --- | --- | --- |
| `asset` | 읽을 원래 음원. | AUDIO STREAM의 원래 준비 문맥 | 해당 준비/provider 수명 |
| `image` | 요구하는 현재 descriptor image. | ASSET descriptor → AUDIO STREAM | 확인된 image 문맥 |
| `attempt` | 실제 소비 주체 provider의 원래 시도. late 자료 재귀속을 막는다. | PLAYBACK → AUDIO STREAM 보호 | 읽기/소비 참조가 종료될 때까지 |
| `offset` | 압축 자료의 시작 byte 위치. PCM 진행 위치가 아니다. | AUDIO STREAM의 소비 진행 | 현재 asset 압축 범위 안 |
| `requestedBytes` | 제한된 읽기 요구량. 끝을 넘거나 overflow하면 실패다. | AUDIO STREAM | offset과 함께 검증 후 |

### Lifetime / Mutability / 폐기

제출 동안 불변 읽기 요구다. ASSET이 반환한 자료가 호출 이후 소비되면 그 참조를 별도로 보호한다. 수용되지 않은 새 읽기는 기존 소비 중 span을 회수할 근거가 아니다.

**관련 Function:** [AudioStream_Prepare](../30_FUNCTIONS/40_AUDIO_STREAM_ASSET_FUNCTIONS.md#audiostream-prepare) · [AudioStream_Advance](../30_FUNCTIONS/40_AUDIO_STREAM_ASSET_FUNCTIONS.md#audiostream-advance) · [Asset_Read](../30_FUNCTIONS/40_AUDIO_STREAM_ASSET_FUNCTIONS.md#asset-read)

**관련 Contract:** [소유·수명](../50_CONTRACTS/00_CONTRACT_OVERVIEW.md#ownership)

**Provisional owner header 후보:** ASSET 제한 읽기 인터페이스 후보 — R6에서 확정. 실제 파일명·신규 Header 필요 여부는 정하지 않는다.

### Pseudo-C

```c
typedef struct
{
    VssAssetKeyType asset;
    VssImageKeyType image;
    VssAttemptKeyType attempt;
    SemanticByteCount offset;
    SemanticByteCount requestedBytes;
} VssAssetReadRequest;
```



<a id="vssassetspan"></a>
## VssAssetSpan
**Category: Descriptor**

읽기 성공의 확인 범위와 압축 byte 참조를 표현한다. 자료 자체의 보호 기간은 실제 소비 주체가 더 이상 읽지 않을 때까지다.

**Owner:** ASSET의 제공 자료/참조 원본  
**Producer:** Asset_Read  
**Consumer:** AUDIO STREAM provider

### 사람이 읽는 Field 정의

| Field | 의미 / 필요한 이유 | Field 생성 주체 | 유효 조건 |
| --- | --- | --- | --- |
| `span` | 이 제공 자료 참조의 identity. 다른 span의 release와 혼동하지 않는다. | ASSET | 제공 성공 뒤 |
| `asset` | 원래 음원 identity. | ASSET | 자료 참조 수명 |
| `image` | 제공 자료의 image 문맥. | ASSET | 자료 참조 수명 |
| `attempt` | 자료를 사용하는 원래 provider 시도. | 읽기 Request → ASSET 보호 | 실제 소비 참조 종료까지 |
| `offset` | 확인된 압축 범위 시작. | ASSET | 성공한 범위만 |
| `validBytes` | 실제 제공한 유효 bytes. requestedBytes와 같다고 가정하지 않는다. | ASSET | 성공한 범위만 |
| `bytes` | 읽기 전용 압축 bytes의 중립 참조. 물리 주소/staging 방식은 미정이다. | ASSET | 실제 consumer가 참조 중인 기간 |

### Lifetime / Mutability / 폐기

생성/제공 뒤 불변이며 ASSET이 실제 consumer 참조 종료까지 보호한다. Session retirement·출력 종료는 압축 bytes의 참조 종료가 아니다. release는 원래 span key와 소비 주체의 더 읽지 않을 근거를 확인해 반영한다. 실패/새 span 제공으로 기존 span을 덮지 않는다.

**관련 Function:** [Asset_Read](../30_FUNCTIONS/40_AUDIO_STREAM_ASSET_FUNCTIONS.md#asset-read) · [AudioStream_Prepare](../30_FUNCTIONS/40_AUDIO_STREAM_ASSET_FUNCTIONS.md#audiostream-prepare) · [AudioStream_Advance](../30_FUNCTIONS/40_AUDIO_STREAM_ASSET_FUNCTIONS.md#audiostream-advance)

**관련 Contract:** [소유·수명](../50_CONTRACTS/00_CONTRACT_OVERVIEW.md#ownership)

**Provisional owner header 후보:** ASSET 자료 참조 인터페이스 후보 — R6에서 확정. 실제 파일명·신규 Header 필요 여부는 정하지 않는다.

### Pseudo-C

```c
typedef struct
{
    VssAssetSpanKeyType span;
    VssAssetKeyType asset;
    VssImageKeyType image;
    VssAttemptKeyType attempt;
    SemanticByteCount offset;
    SemanticByteCount validBytes;
    CompressedBytesRef bytes;
} VssAssetSpan;
```



<a id="vssassetreaddisposition"></a>
## VssAssetReadDisposition — scalar enum
**Category: Result**

읽기/참조 반영의 의미 결과는 scalar로 충분하다. 성공 자료와 발견 진단은 별도 typed 자료다.

**Owner:** ASSET · **Producer:** Asset_Read · **Consumer:** AUDIO STREAM

| 값 | 의미 / 유효 조건 |
| --- | --- |
| `VSS_ASSET_SPAN_GRANTED` | 확인된 범위의 VssAssetSpan 제공 |
| `VSS_ASSET_END_OF_RANGE` | 압축 범위의 끝. 디코딩/출력/전체 계획 완료가 아님 |
| `VSS_ASSET_ACCESS_REJECTED` | 누락/metadata/bounds/PFlash read 등의 확인 실패 |
| `VSS_ASSET_REFERENCE_RELEASED` | 원래 span의 실제 참조 종료 반영 |
| `VSS_ASSET_REFERENCE_STILL_PROTECTED` | 참조 종료 근거 부족. 기존 자료 보호 유지 |

**Lifetime / Mutability:** 해당 호출 결과. span의 참조 수명은 독립이며 read 성공을 전체 decode 성공으로 바꾸지 않는다.

**관련 Function:** [Asset_Read](../30_FUNCTIONS/40_AUDIO_STREAM_ASSET_FUNCTIONS.md#asset-read) · [AudioStream_Prepare](../30_FUNCTIONS/40_AUDIO_STREAM_ASSET_FUNCTIONS.md#audiostream-prepare) · [AudioStream_Advance](../30_FUNCTIONS/40_AUDIO_STREAM_ASSET_FUNCTIONS.md#audiostream-advance)

**관련 Contract:** [소유·수명](../50_CONTRACTS/00_CONTRACT_OVERVIEW.md#ownership) · [고장·복구](../50_CONTRACTS/00_CONTRACT_OVERVIEW.md#fault)

**Provisional owner header 후보:** ASSET 읽기 결과 인터페이스 후보 — R6에서 확정.

```c
typedef enum
{
    VSS_ASSET_SPAN_GRANTED,
    VSS_ASSET_END_OF_RANGE,
    VSS_ASSET_ACCESS_REJECTED,
    VSS_ASSET_REFERENCE_RELEASED,
    VSS_ASSET_REFERENCE_STILL_PROTECTED
} VssAssetReadDisposition;
```

### VssAssetAccess / VssAssetReadResult — SPLIT

`VssAssetAccess`는 VssAssetReadRequest와 **원래 VssAssetSpanKeyType + 실제 consumer 참조 종료 근거**로 나눈다. release만을 위해 별도 Request struct는 만들지 않는다. scalar span key와 해당 provider의 참조 종료 사실이 필요한 인자다. Session retirement로 release를 합성하지 않는다.

`VssAssetReadResult`는 VssAssetReadDisposition, 성공한 VssAssetSpan, 별도 VssDiagnosticEvidence의 전달로 나눈다. 실패에서는 성공 span이 생성되지 않으며 기존 소비 중 자료를 유지한다. ASSET 발견 단계는 lookup/metadata/bounds/PFlash read이고 디코딩 corrupt/unsupported 발견 주체는 AUDIO STREAM이다. 자료 참조 종료의 실제 확인/복사 방식은 Binding TBD다.

<a id="provider"></a>
## AUDIO STREAM 준비·제어·provider 문맥

<a id="vssaudiopreparation"></a>
## VssAudioPreparation
**Category: Command**

원래 전체 계획/시도·음원·첫 시작 기한을 AUDIO STREAM 의미 준비에 전달한다. PCM 주소/물리 전송 설정은 상위 Command에 없다.

**Owner:** PLAYBACK의 요구 의미 / AUDIO STREAM의 수용 문맥  
**Producer:** Playback_RequestTransition 또는 Playback_Advance  
**Consumer:** AudioStream_Prepare

### 사람이 읽는 Field 정의

| Field | 의미 / 필요한 이유 | Field 생성 주체 | 유효 조건 |
| --- | --- | --- | --- |
| `session` | 원래 전체 계획 identity. | PLAYBACK | 요구 생성 뒤 불변 |
| `attempt` | 원래 새 실제 출력 시도. | PLAYBACK | 수용·결과·정리 수명 전체 |
| `plan` | 보호된 전체 의미 계획. | POLICY → PLAYBACK | 참조 보호 동안 |
| `position` | 준비할 구간/cue와의 의미 연결. | PLAYBACK | 같은 plan 문맥 |
| `asset` | 이 준비에서 사용할 의미 음원. | POLICY/PLAYBACK | 정의된 구간과 일치할 때 |
| `firstStartMeta` | 첫 실제 시작 gate가 필요한 준비의 원본 age·시간 근거. Started 정상 후속에는 없으며 기한을 재적용하지 않는다. | STORE → PLAYBACK | 원래 문맥을 보존; 정책 적용 여부는 PLAYBACK 확인 |

### Lifetime / Mutability / 폐기

PLAYBACK이 제출 전에 안정된 불변 문맥을 보호한다. AUDIO STREAM은 필요한 의미를 자신의 준비/provider 문맥으로 보존하고 임시 Command 포인터를 저장하지 않는다. 수용·준비 완료 뒤에도 RequestControl의 최신 조건 재검사 전에는 실제 start 권한이 없다.

**관련 Function:** [Playback_RequestTransition](../30_FUNCTIONS/30_POLICY_PLAYBACK_FUNCTIONS.md#playback-requesttransition) · [Playback_Advance](../30_FUNCTIONS/30_POLICY_PLAYBACK_FUNCTIONS.md#playback-advance) · [AudioStream_Prepare](../30_FUNCTIONS/40_AUDIO_STREAM_ASSET_FUNCTIONS.md#audiostream-prepare)

**관련 Contract:** [소유·수명](../50_CONTRACTS/00_CONTRACT_OVERVIEW.md#ownership) · [비동기 근거](../50_CONTRACTS/00_CONTRACT_OVERVIEW.md#async) · [시간·최신성](../50_CONTRACTS/00_CONTRACT_OVERVIEW.md#timing)

**Provisional owner header 후보:** PLAYBACK→AUDIO STREAM 준비 인터페이스 후보 — R6에서 확정. 실제 파일명·신규 Header 필요 여부는 정하지 않는다.

### Pseudo-C

```c
typedef struct
{
    VssSessionKeyType session;
    VssAttemptKeyType attempt;
    const VssPlanDescriptor * plan;
    VssPlanPositionType position;
    VssAssetKeyType asset;
    const VssInputMeta * firstStartMeta;
} VssAudioPreparation;
```



<a id="vssaudiocontrolaction"></a>
## VssAudioControlAction — scalar enum
**Category: Command**

동일 Attempt의 실행/정리 의도다. 공통 identity와 동일 필드 집합을 쓰므로 START/STOP별 struct를 기계적으로 만들지 않는다.

**Owner:** PLAYBACK · **Producer:** Playback_RequestTransition/Playback_Advance · **Consumer:** AudioStream_RequestControl

| 값 | 의미 / 유효 조건 |
| --- | --- |
| `VSS_AUDIO_START` | 같은 준비 문맥에서 실제 start를 요구 |
| `VSS_AUDIO_STOP` | 전체 후속 생산·제출 차단과 원래 출력 정리를 요구 |

**Lifetime / Mutability:** 원래 Attempt와 함께 불변 제출. STOP은 하위 실패에도 상위 차단을 복원하지 않는다.

**관련 Function:** [Playback_RequestTransition](../30_FUNCTIONS/30_POLICY_PLAYBACK_FUNCTIONS.md#playback-requesttransition) · [Playback_Advance](../30_FUNCTIONS/30_POLICY_PLAYBACK_FUNCTIONS.md#playback-advance) · [AudioStream_RequestControl](../30_FUNCTIONS/40_AUDIO_STREAM_ASSET_FUNCTIONS.md#audiostream-requestcontrol)

**관련 Contract:** [소유·수명](../50_CONTRACTS/00_CONTRACT_OVERVIEW.md#ownership) · [비동기 근거](../50_CONTRACTS/00_CONTRACT_OVERVIEW.md#async)

**Provisional owner header 후보:** PLAYBACK 제어 인터페이스 후보 — R6에서 확정.

```c
typedef enum
{
    VSS_AUDIO_START,
    VSS_AUDIO_STOP
} VssAudioControlAction;
```

<a id="vssplaybackcontrolcommand"></a>
## VssPlaybackControlCommand
**Category: Command**

start/stop에 공통인 원래 시도·Session·동작 의미만 전달한다. 대상/구성 Fault의 복구는 Session이 없어도 존재하므로 이 Command의 optional 필드로 넣지 않는다.

**Owner:** PLAYBACK 제어 의미  
**Producer:** Playback_RequestTransition/Playback_Advance  
**Consumer:** AudioStream_RequestControl

### 사람이 읽는 Field 정의

| Field | 의미 / 필요한 이유 | Field 생성 주체 | 유효 조건 |
| --- | --- | --- | --- |
| `session` | 원래 전체 실행 대상. | PLAYBACK | 해당 실행/정리 문맥 |
| `attempt` | 원래 실제 출력 시도. | PLAYBACK | 요구·결과 추적 수명 |
| `action` | START 또는 STOP. | PLAYBACK | 현재 요구 수명 |

### Lifetime / Mutability / 폐기

제출 뒤 불변이며 반복 STOP은 원래 identity/차단을 유지한다. Start의 최신 조건은 별도 작은 읽기 근거로 확인한다. 복구 요구는 70_DIAGNOSTIC_DATA의 VssRecoveryPermission을 읽기 전용으로 전달하고 이전 자원 안전 조건을 별도로 확인한다.

**관련 Function:** [Playback_RequestTransition](../30_FUNCTIONS/30_POLICY_PLAYBACK_FUNCTIONS.md#playback-requesttransition) · [Playback_Advance](../30_FUNCTIONS/30_POLICY_PLAYBACK_FUNCTIONS.md#playback-advance) · [AudioStream_RequestControl](../30_FUNCTIONS/40_AUDIO_STREAM_ASSET_FUNCTIONS.md#audiostream-requestcontrol)

**관련 Contract:** [소유·수명](../50_CONTRACTS/00_CONTRACT_OVERVIEW.md#ownership) · [비동기 근거](../50_CONTRACTS/00_CONTRACT_OVERVIEW.md#async)

**Provisional owner header 후보:** PLAYBACK 제어 인터페이스 후보 — R6에서 확정. 실제 파일명·신규 Header 필요 여부는 정하지 않는다.

### Pseudo-C

```c
typedef struct
{
    VssSessionKeyType session;
    VssAttemptKeyType attempt;
    VssAudioControlAction action;
} VssPlaybackControlCommand;
```



<a id="vssproviderphase"></a>
## VssProviderPhase — scalar enum
**Category: State**

AUDIO STREAM 내부 provider의 지속되는 lifecycle이다. Core Function을 phase마다 늘리지 않는다.

**Owner:** AUDIO STREAM · **Producer:** AudioStream_Prepare/RequestControl/Advance · **Consumer:** AUDIO STREAM

| 값 | 의미 / 유효 조건 |
| --- | --- |
| `VSS_PROVIDER_PREPARING` | Asset/decoder/초기 PCM 준비 |
| `VSS_PROVIDER_PREPARED` | 현재 준비 충족. 실제 start 허가 아님 |
| `VSS_PROVIDER_RUNNING` | 현재 허용된 decode/PCM 진행 |
| `VSS_PROVIDER_DRAINING` | 현재 구간의 마지막 유효 PCM/장치 정리 진행 |
| `VSS_PROVIDER_CLOSING` | 취소/정리·참조 안전 종료 중 |
| `VSS_PROVIDER_CLOSED` | 실제 provider/read 참조 종료 확인 |

**Lifetime / Mutability:** 원래 provider 문맥 수명. EOF는 출력/Session 완료가 아니다.

**관련 Function:** [AudioStream_Prepare](../30_FUNCTIONS/40_AUDIO_STREAM_ASSET_FUNCTIONS.md#audiostream-prepare) · [AudioStream_RequestControl](../30_FUNCTIONS/40_AUDIO_STREAM_ASSET_FUNCTIONS.md#audiostream-requestcontrol) · [AudioStream_Advance](../30_FUNCTIONS/40_AUDIO_STREAM_ASSET_FUNCTIONS.md#audiostream-advance)

**관련 Contract:** [소유·수명](../50_CONTRACTS/00_CONTRACT_OVERVIEW.md#ownership) · [PCM·전송](../50_CONTRACTS/00_CONTRACT_OVERVIEW.md#buffer)

**Provisional owner header 후보:** AUDIO STREAM 내부 provider 후보 — R6에서 확정.

```c
typedef enum
{
    VSS_PROVIDER_PREPARING,
    VSS_PROVIDER_PREPARED,
    VSS_PROVIDER_RUNNING,
    VSS_PROVIDER_DRAINING,
    VSS_PROVIDER_CLOSING,
    VSS_PROVIDER_CLOSED
} VssProviderPhase;
```

<a id="vssprovidercontext"></a>
## VssProviderContext
**Category: Context**

원래 시도의 압축 자료 소비·decode 진행과 미래 생산/제출 권한을 관리한다. 디코더 scratch/내부 temporary를 개별 Data Model로 승격하지 않는다.

**Owner:** AUDIO STREAM 단일 writer  
**Producer:** AudioStream_Prepare/AudioStream_RequestControl/AudioStream_Advance  
**Consumer:** AUDIO STREAM 내부 생산/정리

### 사람이 읽는 Field 정의

| Field | 의미 / 필요한 이유 | Field 생성 주체 | 유효 조건 |
| --- | --- | --- | --- |
| `session` | 원래 전체 실행 연결. | PLAYBACK → AUDIO STREAM | 수용 뒤 불변 |
| `attempt` | 원래 provider 시도. | PLAYBACK → AUDIO STREAM | provider/read 안전 종료까지 |
| `asset` | 현재 압축 음원 identity. | 원래 준비 → AUDIO STREAM | provider 수명 |
| `sourceSpan` | 현재 실제 consumer가 읽는 보호된 압축 자료. | ASSET → AUDIO STREAM 보호 | 실제 참조가 끝날 때까지 |
| `sourcePosition` | 실제 압축 입력 소비 위치. ASSET이 대신 변경하지 않는다. | AUDIO STREAM 내부 decode | 현재 asset 범위 안 |
| `format` | 확인된 decoder 출력 PCM format의 의미. bit폭/sample 값은 미정이다. | AUDIO STREAM decode 검증 | format 확인 뒤 같은 provider/PCM 구간 |
| `phase` | 준비/생산/정리·참조 종료 진행. | AUDIO STREAM | 전이 반영 뒤 |
| `productionAllowed` | 해당 음향의 decode/유효 PCM 보충·새 제출을 허용하는 로컬 권한. 정리 목적의 물리 무음 채움과 구별한다. | AUDIO STREAM | START/STOP 및 최신 조건 검토 뒤; stop 시 차단 유지 |
| `endOfSource` | 압축 원본의 끝을 확인했는가. 유효 음향 tail을 임의 padding·반복으로 늘리지 않는다. | AUDIO STREAM/ASSET 범위 근거 | 확인된 원본 끝 이후 |

### Lifetime / Mutability / 폐기

수용 시 하위 호출보다 먼저 보호한다. AUDIO STREAM만 소비/phase·생산 권한을 변경한다. 디코더 작업 공간은 이 실제 작업 수명 동안 보호하되 그 구현 필드는 모델에 넣지 않는다. 취소/stop 뒤 생산을 차단하고 진행 중 provider/read는 실제 참조 종료까지 유지한다. Session retirement만으로 bytes를 해제하지 않는다.

**관련 Function:** [AudioStream_Prepare](../30_FUNCTIONS/40_AUDIO_STREAM_ASSET_FUNCTIONS.md#audiostream-prepare) · [AudioStream_RequestControl](../30_FUNCTIONS/40_AUDIO_STREAM_ASSET_FUNCTIONS.md#audiostream-requestcontrol) · [AudioStream_Advance](../30_FUNCTIONS/40_AUDIO_STREAM_ASSET_FUNCTIONS.md#audiostream-advance) · [Asset_Read](../30_FUNCTIONS/40_AUDIO_STREAM_ASSET_FUNCTIONS.md#asset-read)

**관련 Contract:** [소유·수명](../50_CONTRACTS/00_CONTRACT_OVERVIEW.md#ownership) · [PCM·전송](../50_CONTRACTS/00_CONTRACT_OVERVIEW.md#buffer)

**Provisional owner header 후보:** AUDIO STREAM 내부 provider 후보 — R6에서 확정. 실제 파일명·신규 Header 필요 여부는 정하지 않는다.

### Pseudo-C

```c
typedef struct
{
    VssSessionKeyType session;
    VssAttemptKeyType attempt;
    VssAssetKeyType asset;
    const VssAssetSpan * sourceSpan;
    SemanticByteCount sourcePosition;
    VssPcmFormatType format;
    VssProviderPhase phase;
    SemanticBool productionAllowed;
    SemanticBool endOfSource;
} VssProviderContext;
```



<a id="vssaudiorequeststage"></a>
## VssAudioRequestStage — scalar enum
**Category: Result**

prepare 요청의 단계와 start/stop 수용을 구별한다. actual·안전 반환·전체 종료 enum을 섞지 않는다.

**Owner:** AUDIO STREAM · **Producer:** Prepare/RequestControl/Advance · **Consumer:** PLAYBACK

| 값 | 의미 / 유효 조건 |
| --- | --- |
| `VSS_AUDIO_PREPARE_RECEIVED` | 원래 준비 요구 수용 |
| `VSS_AUDIO_PREPARE_PENDING` | 이미 수용한 준비의 후속 진행 |
| `VSS_AUDIO_PREPARED` | 현재 음원/provider·장치·초기 PCM 준비 충족 |
| `VSS_AUDIO_CONTROL_RECEIVED` | start/stop 의미 제어 수용 |
| `VSS_AUDIO_START_SUBMITTED` | 원래 하위 start 제출/대기이며 활성화 미확인 |
| `VSS_AUDIO_START_ACCEPTED_AND_ENABLED` | 원래 start 수락과 실제 장치 활성화 확인. actual은 별도 |
| `VSS_AUDIO_EFFECT_UNCERTAIN` | 부분 효력/접근·출력 가능성 미확인. 보호 유지 |
| `VSS_AUDIO_PREPARE_FAILED` | 이미 수용한 준비의 실제 실패. 부분 자원 보호/정리 유지 |
| `VSS_AUDIO_REQUEST_REJECTED` | 새 요구 거부. 기존 원래 작업의 보호/사실 의무와 구별 |

**Lifetime / Mutability:** 원래 요구의 진행/수용 결과. 실제 효과를 대신하지 않는다.

**관련 Function:** [AudioStream_Prepare](../30_FUNCTIONS/40_AUDIO_STREAM_ASSET_FUNCTIONS.md#audiostream-prepare) · [AudioStream_RequestControl](../30_FUNCTIONS/40_AUDIO_STREAM_ASSET_FUNCTIONS.md#audiostream-requestcontrol) · [AudioStream_Advance](../30_FUNCTIONS/40_AUDIO_STREAM_ASSET_FUNCTIONS.md#audiostream-advance)

**관련 Contract:** [비동기 근거](../50_CONTRACTS/00_CONTRACT_OVERVIEW.md#async)

**Provisional owner header 후보:** AUDIO STREAM 수용/준비 결과 후보 — R6에서 확정.

```c
typedef enum
{
    VSS_AUDIO_PREPARE_RECEIVED,
    VSS_AUDIO_PREPARE_PENDING,
    VSS_AUDIO_PREPARED,
    VSS_AUDIO_CONTROL_RECEIVED,
    VSS_AUDIO_START_SUBMITTED,
    VSS_AUDIO_START_ACCEPTED_AND_ENABLED,
    VSS_AUDIO_EFFECT_UNCERTAIN,
    VSS_AUDIO_PREPARE_FAILED,
    VSS_AUDIO_REQUEST_REJECTED
} VssAudioRequestStage;
```

<a id="vssaudiorequestresult"></a>
## VssAudioRequestResult
**Category: Result**

같은 준비/제어 요구의 수용/진행 단계만 묶는다. 실제 출력·PCM 반환·복구 결과와 다른 typed 결과로 전달한다.

**Owner:** AUDIO STREAM  
**Producer:** AudioStream_Prepare/RequestControl/Advance  
**Consumer:** PLAYBACK

### 사람이 읽는 Field 정의

| Field | 의미 / 필요한 이유 | Field 생성 주체 | 유효 조건 |
| --- | --- | --- | --- |
| `session` | 결과가 귀속되는 원래 Session. | 수용 문맥 | 원래 요구 수명 |
| `attempt` | 원래 시도 identity. | 수용 문맥 | 원래 요구 수명 |
| `stage` | 준비/제어의 수용·진행/완료·거부 단계. | AUDIO STREAM | 반영 완료 뒤 |

### Lifetime / Mutability / 폐기

생성 뒤 불변이고 소비자가 반영하거나 보호된 추적이 끝날 때까지 보존한다. 늦은 PREPARED/수용이 이미 적용한 actual·종료를 되돌리지 않는다. 실제 결과 전달의 저장 방식·용량은 TBD이며 마지막 결과 덮어쓰기로 대체하지 않는다.

**관련 Function:** [AudioStream_Prepare](../30_FUNCTIONS/40_AUDIO_STREAM_ASSET_FUNCTIONS.md#audiostream-prepare) · [AudioStream_RequestControl](../30_FUNCTIONS/40_AUDIO_STREAM_ASSET_FUNCTIONS.md#audiostream-requestcontrol) · [AudioStream_Advance](../30_FUNCTIONS/40_AUDIO_STREAM_ASSET_FUNCTIONS.md#audiostream-advance) · [Playback_Advance](../30_FUNCTIONS/30_POLICY_PLAYBACK_FUNCTIONS.md#playback-advance)

**관련 Contract:** [소유·수명](../50_CONTRACTS/00_CONTRACT_OVERVIEW.md#ownership) · [비동기 근거](../50_CONTRACTS/00_CONTRACT_OVERVIEW.md#async)

**Provisional owner header 후보:** AUDIO STREAM 준비 결과 인터페이스 후보 — R6에서 확정. 실제 파일명·신규 Header 필요 여부는 정하지 않는다.

### Pseudo-C

```c
typedef struct
{
    VssSessionKeyType session;
    VssAttemptKeyType attempt;
    VssAudioRequestStage stage;
} VssAudioRequestResult;
```



<a id="vssaudiooutputkind"></a>
## VssAudioOutputKind — scalar enum
**Category: Evidence**

실제 출력/무출력/종료 지식의 Backend 범위 근거다. PLAYBACK의 전체 권한 차단/적법성 판정과 구별한다.

**Owner:** AUDIO STREAM의 Backend 근거 결합 · **Producer:** AudioStream_Advance · **Consumer:** PLAYBACK

| 값 | 의미 / 유효 조건 |
| --- | --- |
| `OUTPUT_START_CONFIRMED` | 원래 시도의 새 물리 출력 근거가 충분함. PLAYBACK이 원래 시각·기한/권한을 대조 |
| `NO_START_CONFIRMED` | 해당 Backend 범위의 충분한 과거 무출력·미래 하위 출력 불가와 미래 PCM 차단 |
| `OUTPUT_TERMINATION_CONFIRMED` | 해당 Backend 출력 범위의 종료/미래 출력 차단. 전체 Session 끝인지 상위 판정 필요 |
| `START_OUTCOME_UNCERTAIN` | 해당 시작의 근거가 불명확함. 상위 불확실 최종 판정 근거 |

**Lifetime / Mutability:** 원래 scope/identity와 함께 보호. 이름만으로 확정하지 않고 아래 조건을 모두 요구한다.

**관련 Function:** [AudioStream_Advance](../30_FUNCTIONS/40_AUDIO_STREAM_ASSET_FUNCTIONS.md#audiostream-advance) · [Playback_Advance](../30_FUNCTIONS/30_POLICY_PLAYBACK_FUNCTIONS.md#playback-advance)

**관련 Contract:** [비동기 근거](../50_CONTRACTS/00_CONTRACT_OVERVIEW.md#async) · [PCM·전송](../50_CONTRACTS/00_CONTRACT_OVERVIEW.md#buffer)

**Provisional owner header 후보:** AUDIO STREAM 출력 Evidence 인터페이스 후보 — R6에서 확정.

```c
typedef enum
{
    OUTPUT_START_CONFIRMED,
    NO_START_CONFIRMED,
    OUTPUT_TERMINATION_CONFIRMED,
    START_OUTCOME_UNCERTAIN
} VssAudioOutputKind;
```

<a id="vssaudiooutputevidence"></a>
## VssAudioOutputEvidence
**Category: Evidence**

DRIVER의 실제 범위 근거에 AUDIO STREAM의 미래 PCM 차단을 결합한다. 전체 Session 최종/retirement는 PLAYBACK이 판단하며 준비 결과와 합치지 않는다.

**Owner:** AUDIO STREAM의 Backend 근거. transport 원본 producer는 AUDIO TX다.  
**Producer:** AudioStream_Advance  
**Consumer:** Playback_Advance

### 사람이 읽는 Field 정의

| Field | 의미 / 필요한 이유 | Field 생성 주체 | 유효 조건 |
| --- | --- | --- | --- |
| `session` | 원래 전체 실행 연결. | 원래 준비/provider | 해당 Backend 근거 수명 |
| `attempt` | 원래 실제 출력 시도. | 원래 준비/provider | 해당 Backend 근거 수명 |
| `kind` | actual/no-start/종료/불명확 근거 구별. | AUDIO STREAM 결합 | 충분한 해당 범위 근거가 있을 때 |
| `scope` | 구간 또는 원래 Attempt 전체 출력인지 구별. 한 cue 끝을 전체 Session 끝으로 만들지 않는다. | AUDIO STREAM의 원래 작업 범위 | scope가 원래 요구와 일치할 때 |
| `transportFacts` | 결합에 사용한 보호된 원래 DRIVER 사실들. 과거 무출력·미래 차단은 서로 다른 사실이다. | AUDIO TX → AUDIO STREAM 보호 | 해당 사실 목록이 보존되는 동안 |
| `transportFactCount` | 실제 포함한 typed 근거 수. | AUDIO STREAM | transportFacts의 유효 범위 |
| `futurePcmBlocked` | 이 scope에 후속 decode/refill/제출이 불가능하게 차단됐는가. actual만인 경우 false가 정상이다. | AUDIO STREAM | NO_START/TERMINATION 확인에 필수 |

### Lifetime / Mutability / 폐기

producer가 범위 근거를 확보한 뒤 불변으로 내보내고 PLAYBACK 반영/추적까지 보호한다. 충분성 부족·관측 손실이면 확정 kind를 만들지 않고 불명확/진단으로 보존한다. Backend NO_START/TERMINATION만으로 PLAYBACK의 전체 start/cue/반복 차단까지 보장하지 않는다. 출력 물리 boundary/충분성은 B2-R/보드 검증 TBD다.

**관련 Function:** [AudioTx_Advance](../30_FUNCTIONS/50_DRIVER_HAL_FUNCTIONS.md#audiotx-advance) · [AudioStream_Advance](../30_FUNCTIONS/40_AUDIO_STREAM_ASSET_FUNCTIONS.md#audiostream-advance) · [Playback_Advance](../30_FUNCTIONS/30_POLICY_PLAYBACK_FUNCTIONS.md#playback-advance)

**관련 Contract:** [비동기 근거](../50_CONTRACTS/00_CONTRACT_OVERVIEW.md#async) · [소유·수명](../50_CONTRACTS/00_CONTRACT_OVERVIEW.md#ownership) · [PCM·전송](../50_CONTRACTS/00_CONTRACT_OVERVIEW.md#buffer)

**Provisional owner header 후보:** AUDIO STREAM 출력 Evidence 인터페이스 후보 — R6에서 확정. 실제 파일명·신규 Header 필요 여부는 정하지 않는다.

### Pseudo-C

```c
typedef struct
{
    VssSessionKeyType session;
    VssAttemptKeyType attempt;
    VssAudioOutputKind kind;
    VssOutputScope scope;
    const VssTxOutputEvidence * transportFacts;
    SemanticCount transportFactCount;
    SemanticBool futurePcmBlocked;
} VssAudioOutputEvidence;
```

NO_START_CONFIRMED: 충분한 과거 무출력 + 모든 해당 옛 하위 미래 출력 불가 + 이 scope의 미래 PCM 차단을 요구한다. OUTPUT_TERMINATION_CONFIRMED: 해당 출력/장치 잔류 종료 + 미래 출력 불가 + PCM 차단을 요구한다. 현재 idle·timeout·abort 수락·mute만으로 이 kind를 생성하지 않는다.

<a id="vssbackendcleanupobservation"></a>
## VssBackendCleanupObservation
**Category: Snapshot**

물리 출력 종료와 provider/read·PCM 자원 안전 종료를 별도로 관측한다. 옛 불변 callback 기록은 자원이 안전 종료돼도 더 오래 유지할 수 있다.

**Owner:** AUDIO STREAM 파생 읽기  
**Producer:** AudioStream_Advance  
**Consumer:** PLAYBACK / FLOW의 복구 안전 정리 조율

### 사람이 읽는 Field 정의

| Field | 의미 / 필요한 이유 | Field 생성 주체 | 유효 조건 |
| --- | --- | --- | --- |
| `attempt` | 정리한 원래 시도. | AUDIO STREAM 원래 문맥 | 정리 수명 |
| `providerReferencesEnded` | 실제 provider/read 소비 주체 참조 종료 확인. | AUDIO STREAM/ASSET 근거 | 모든 관련 참조가 실제 종료된 때만 true |
| `pcmAccessEnded` | 이 Attempt의 A/B 모든 관련 회차의 미래 하위 접근 종료 확인. | AUDIO STREAM의 TX 반환 적용 | 각 해당 회차의 안전 근거 반영 뒤 |
| `lowerOutputBlocked` | 해당 옛 하위 출력/잔류 가능성 차단의 충분 근거. | AUDIO TX → AUDIO STREAM | 충분한 범위 근거 뒤 |
| `lateIdentityProtected` | 남은 callback/결과가 새 context와 alias 없이 원래 귀속으로 보호됐는가. | HAL/TX 근거 → AUDIO STREAM | 등록/참조 보호 근거 확인 뒤 |

### Lifetime / Mutability / 폐기

반영 완료 정리 상태의 읽기 관측이다. 상위 owner retirement나 Fault 해제를 직접 수행하지 않는다. true들은 출력/자원/late 보호가 각각 확인된 경우만 파생한다. 별도 기록 storage 회수는 callback/HW·대기·처리 중 참조 종료까지 확인한다.

**관련 Function:** [AudioStream_Advance](../30_FUNCTIONS/40_AUDIO_STREAM_ASSET_FUNCTIONS.md#audiostream-advance) · [Playback_Advance](../30_FUNCTIONS/30_POLICY_PLAYBACK_FUNCTIONS.md#playback-advance) · [Flow_Process](../30_FUNCTIONS/10_FLOW_FUNCTIONS.md#flow-process)

**관련 Contract:** [소유·수명](../50_CONTRACTS/00_CONTRACT_OVERVIEW.md#ownership) · [비동기 근거](../50_CONTRACTS/00_CONTRACT_OVERVIEW.md#async) · [PCM·전송](../50_CONTRACTS/00_CONTRACT_OVERVIEW.md#buffer)

**Provisional owner header 후보:** AUDIO STREAM 정리 관측 후보 — R6에서 확정. 실제 파일명·신규 Header 필요 여부는 정하지 않는다.

### Pseudo-C

```c
typedef struct
{
    VssAttemptKeyType attempt;
    SemanticBool providerReferencesEnded;
    SemanticBool pcmAccessEnded;
    SemanticBool lowerOutputBlocked;
    SemanticBool lateIdentityProtected;
} VssBackendCleanupObservation;
```



<a id="pcm"></a>
## 정확히 두 PCM Buffer — A/B와 회차

<a id="vsspcmbufferid"></a>
## VssPcmBufferId — scalar enum
**Category: Descriptor**

전송 PCM Buffer는 정확히 A/B 두 개다. generic pool·제3 Buffer를 만들지 않는다.

**Owner:** AUDIO STREAM의 PCM 의미 · **Producer:** 현재 Buffer 정의 · **Consumer:** AUDIO STREAM/AUDIO TX/HAL

| 값 | 의미 / 유효 조건 |
| --- | --- |
| `VSS_PCM_A` | 첫 전송 PCM Buffer |
| `VSS_PCM_B` | 둘째 전송 PCM Buffer |

**Lifetime / Mutability:** 물리 Buffer와 의미 연결. 실제 크기/주소/배치 미정.

**관련 Function:** [AudioStream_Prepare](../30_FUNCTIONS/40_AUDIO_STREAM_ASSET_FUNCTIONS.md#audiostream-prepare) · [AudioStream_Advance](../30_FUNCTIONS/40_AUDIO_STREAM_ASSET_FUNCTIONS.md#audiostream-advance) · [AudioTx_Request](../30_FUNCTIONS/50_DRIVER_HAL_FUNCTIONS.md#audiotx-request)

**관련 Contract:** [PCM·전송](../50_CONTRACTS/00_CONTRACT_OVERVIEW.md#buffer)

**Provisional owner header 후보:** AUDIO STREAM PCM 인터페이스 후보 — R6에서 확정.

```c
typedef enum
{
    VSS_PCM_A,
    VSS_PCM_B
} VssPcmBufferId;
```

<a id="vsspcmcyclekey"></a>
## VssPcmCycleKey
**Category: Descriptor**

같은 A/B가 재사용될 때 옛 결과로 새 회차를 해제하지 않기 위한 불변 귀속이다. Buffer ID 하나만으로 안전 반환을 판정하지 않는다.

**Owner:** AUDIO STREAM의 회차 생성  
**Producer:** AudioStream_Prepare/AudioStream_Advance 내부 생산  
**Consumer:** AUDIO TX/HAL → AUDIO STREAM 반환 대조

### 사람이 읽는 Field 정의

| Field | 의미 / 필요한 이유 | Field 생성 주체 | 유효 조건 |
| --- | --- | --- | --- |
| `attempt` | 원래 출력 시도. | AUDIO STREAM의 원래 provider | 회차 생성 뒤 불변 |
| `buffer` | A 또는 B. | AUDIO STREAM | 해당 회차 전체 |
| `cycle` | 이 Buffer의 재사용 회차 identity. 폭/증분/wrap 규칙은 TBD다. | AUDIO STREAM | 새 사용 회차 생성 뒤 불변 |

### Lifetime / Mutability / 폐기

CPU 생산 회차를 준비할 때 생성하고 handoff/원래 등록/사실에 보존한다. 안전 반환 뒤 Buffer가 새 회차로 쓰여도 옛 immutable 귀속 기록은 callback 참조 종료까지 독립 보호한다. 옛 key가 새 key와 alias되는 구현은 허용하지 않는다.

**관련 Function:** [AudioStream_Prepare](../30_FUNCTIONS/40_AUDIO_STREAM_ASSET_FUNCTIONS.md#audiostream-prepare) · [AudioStream_Advance](../30_FUNCTIONS/40_AUDIO_STREAM_ASSET_FUNCTIONS.md#audiostream-advance) · [AudioTx_Request](../30_FUNCTIONS/50_DRIVER_HAL_FUNCTIONS.md#audiotx-request) · [AudioTx_Advance](../30_FUNCTIONS/50_DRIVER_HAL_FUNCTIONS.md#audiotx-advance) · [AudioHAL_Callback](../30_FUNCTIONS/50_DRIVER_HAL_FUNCTIONS.md#audiohal-callback)

**관련 Contract:** [소유·수명](../50_CONTRACTS/00_CONTRACT_OVERVIEW.md#ownership) · [비동기 근거](../50_CONTRACTS/00_CONTRACT_OVERVIEW.md#async) · [PCM·전송](../50_CONTRACTS/00_CONTRACT_OVERVIEW.md#buffer)

**Provisional owner header 후보:** AUDIO STREAM PCM 귀속 인터페이스 후보 — R6에서 확정. 실제 파일명·신규 Header 필요 여부는 정하지 않는다.

### Pseudo-C

```c
typedef struct
{
    VssAttemptKeyType attempt;
    VssPcmBufferId buffer;
    VssPcmCycleType cycle;
} VssPcmCycleKey;
```



<a id="vsspcmusage"></a>
## VssPcmUsage — scalar enum
**Category: State**

AUDIO STREAM의 CPU 쓰기/인계 사용권 상태다. AUDIO TX의 장치 접근 보호 원본과 다르다.

**Owner:** AUDIO STREAM · **Producer:** Prepare/RequestControl/Advance 내부 PCM 처리 · **Consumer:** AUDIO STREAM

| 값 | 의미 / 유효 조건 |
| --- | --- |
| `VSS_PCM_CPU_WRITABLE` | 옛 작업/회차의 접근 종료 근거 확인. 실제 쓰기에는 차기 DMA 방문 전 안전 구간도 필요하며 시간 제한 없는 권한이 아님. 음향 생산 권한은 별도 요구 |
| `VSS_PCM_CPU_READY` | 유효 PCM 생산 완료·내용 안정 |
| `VSS_PCM_HANDOFF_PENDING` | DRIVER 호출 전부터 CPU 덮어쓰기 금지 |
| `VSS_PCM_HW_PROTECTED` | 장치 대기/전송/재접근 가능. CPU 쓰기 금지 |
| `VSS_PCM_RETURN_PENDING` | 소비/정리 뒤에도 안전 반환 근거 대기. CPU 쓰기 금지 |

**Lifetime / Mutability:** 해당 Buffer/회차 수명. consumed는 RETURN_PENDING일 수 있으며 안전 반환과 다르다.

**관련 Function:** [AudioStream_Prepare](../30_FUNCTIONS/40_AUDIO_STREAM_ASSET_FUNCTIONS.md#audiostream-prepare) · [AudioStream_RequestControl](../30_FUNCTIONS/40_AUDIO_STREAM_ASSET_FUNCTIONS.md#audiostream-requestcontrol) · [AudioStream_Advance](../30_FUNCTIONS/40_AUDIO_STREAM_ASSET_FUNCTIONS.md#audiostream-advance)

**관련 Contract:** [PCM·전송](../50_CONTRACTS/00_CONTRACT_OVERVIEW.md#buffer)

**Provisional owner header 후보:** AUDIO STREAM PCM 내부 상태 후보 — R6에서 확정.

```c
typedef enum
{
    VSS_PCM_CPU_WRITABLE,
    VSS_PCM_CPU_READY,
    VSS_PCM_HANDOFF_PENDING,
    VSS_PCM_HW_PROTECTED,
    VSS_PCM_RETURN_PENDING
} VssPcmUsage;
```

<a id="vsspcmbufferstate"></a>
## VssPcmBufferState
**Category: State**

정확히 A/B 두 instance의 내용·유효 구간·CPU 권한을 관리한다. decoder scratch/압축 입력 span은 이 두 전송 Buffer에 포함하지 않는다.

**Owner:** AUDIO STREAM 단일 writer  
**Producer:** AudioStream_Prepare/AudioStream_Advance 내부 PCM 생산  
**Consumer:** AUDIO STREAM / AudioTx_Request의 보호된 인계

### 사람이 읽는 Field 정의

| Field | 의미 / 필요한 이유 | Field 생성 주체 | 유효 조건 |
| --- | --- | --- | --- |
| `key` | Buffer와 원래 시도/사용 회차. | AUDIO STREAM | 현재 회차 전체 |
| `samples` | 현재 Buffer의 PCM 내용 중립 참조. 주소/width/layout은 TBD다. | AUDIO STREAM 생산 | CPU_READY 이후 안정; pending부터 변경 금지 |
| `firstValidFrame` | 유효 구간의 시작 frame. | AUDIO STREAM | 현재 생성된 구간 |
| `validFrameCount` | 해당 음향의 논리적으로 유효한 PCM frame 수. 물리 슬롯의 무음 채움은 이 수에 포함하지 않으며 전체 용량/짧은 tail을 합성하지 않는다. | AUDIO STREAM | 현재 유효 구간만; 0이면 신규 유효 음향 PCM 인계 없음. 지속 DMA의 물리 전송 중지를 뜻하지 않음 |
| `format` | 확인된 PCM format. | AUDIO STREAM decode | 해당 유효 구간 |
| `usage` | CPU 쓰기/인계·장치 보호·반환 대기. | AUDIO STREAM | 해당 회차의 사실 반영 뒤 |

### Lifetime / Mutability / 폐기

이 State의 유효 음향 생산에는 `CPU_WRITABLE`과 현재 provider의 `productionAllowed`가 모두 필요하다. pending/반환 적용은 [Buffer §3](../50_CONTRACTS/50_AUDIO_BUFFER_TRANSPORT.md#access-cycle), 실제 갱신의 현재 안전 조건은 [Buffer C1-03](../50_CONTRACTS/50_AUDIO_BUFFER_TRANSPORT.md#safe-write)을 따른다. AUDIO STREAM은 같은 key의 충분 반환/명확한 무접근 거부만 자기 usage에 적용한다.

### 유효 음향 PCM과 물리 무음 채움 — C1-01

유효 frame/물리 무음·원음의 0 sample·STOP 뒤 정리의 공통 구분은 [Buffer C1-01](../50_CONTRACTS/50_AUDIO_BUFFER_TRANSPORT.md#valid-and-silence)을 따른다. 이 State의 `validFrameCount`는 논리 음향 구간만 세며 빈 B/짧은 tail을 전체 용량으로 바꾸지 않는다. 0이면 새 음향 인계가 없고 기존 A/B의 물리 무음 지속과 구별한다.

AUDIO STREAM은 STOP 뒤 `productionAllowed=false`를 유지한다. 정리 목적의 zero-fill도 위 실제 CPU 안전 조건을 우회하지 않으며 소유 범위/적용 순서·stale 차단의 물리 충분조건은 B2-R TBD다.

**관련 Function:** [AudioStream_Prepare](../30_FUNCTIONS/40_AUDIO_STREAM_ASSET_FUNCTIONS.md#audiostream-prepare) · [AudioStream_RequestControl](../30_FUNCTIONS/40_AUDIO_STREAM_ASSET_FUNCTIONS.md#audiostream-requestcontrol) · [AudioStream_Advance](../30_FUNCTIONS/40_AUDIO_STREAM_ASSET_FUNCTIONS.md#audiostream-advance) · [AudioTx_Request](../30_FUNCTIONS/50_DRIVER_HAL_FUNCTIONS.md#audiotx-request) · [AudioTx_Advance](../30_FUNCTIONS/50_DRIVER_HAL_FUNCTIONS.md#audiotx-advance)

**관련 Contract:** [PCM·전송](../50_CONTRACTS/00_CONTRACT_OVERVIEW.md#buffer) · [소유·수명](../50_CONTRACTS/00_CONTRACT_OVERVIEW.md#ownership) · [비동기 근거](../50_CONTRACTS/00_CONTRACT_OVERVIEW.md#async)

**Provisional owner header 후보:** AUDIO STREAM PCM 내부 상태 후보 — R6에서 확정. 실제 파일명·신규 Header 필요 여부는 정하지 않는다.

### Pseudo-C

```c
typedef struct
{
    VssPcmCycleKey key;
    PcmSamplesRef samples;
    SemanticFrameCount firstValidFrame;
    SemanticFrameCount validFrameCount;
    VssPcmFormatType format;
    VssPcmUsage usage;
} VssPcmBufferState;
```

네 경계는 [Buffer §5](../50_CONTRACTS/50_AUDIO_BUFFER_TRANSPORT.md#four-boundaries)를 따른다. 이 State는 PCM usage만 소유하며 provider/전체 출력/HAL 기록의 원본을 대신 변경하지 않는다.

## VssAudioBackendResult — SPLIT typed 결과군

이 이름의 struct/union/마지막 결과 슬롯은 만들지 않는다. 준비/제어의 `VssAudioRequestResult`, 실제 범위의 `VssAudioOutputEvidence`, 안전 정리의 `VssBackendCleanupObservation`, 원인/단계별 `VssDiagnosticEvidence`, `VssRecoveryPerformedEvidence`와 `VssRecoveryVerificationEvidence`로 전달한다. PCM 소비/안전 반환의 상세는 AUDIO STREAM 내부에서 반영하고 상위에는 정리 관측으로 제공한다. PLAYBACK/FLOW에 PCM 주소/장치 descriptor를 노출하지 않는다.

결과 없는 진행은 VssTimeEvidence와 owner의 기존 문맥으로 표현한다. 결과가 여러 개면 각각 보호된 typed 사실로 남기고 소비자가 반영할 때까지 유지한다. 알림 합침/저장 한계는 사실 삭제가 아니라 관측 손실 근거로 드러내야 한다. 전달 방식·자료 복사/참조·저장 예산은 후속 Binding에서 확인한다.

`VssAudioControlIntent`는 VssPlaybackControlCommand와 HEALTH가 만든 VssRecoveryPermission으로 나뉜다. 복구 요구자는 FLOW이고 실제 수행자는 Backend다. 수행/검증 결과를 준비/actual에 섞지 않는다. 두 typed 경로의 독립 국소 PASS는 [Data Overview](00_DATA_OVERVIEW.md#r2-local-review) 및 [함수 연결](../30_FUNCTIONS/40_AUDIO_STREAM_ASSET_FUNCTIONS.md#audiostream-requestcontrol)을 따른다. 논리 Core/기존 pseudo prototype은 유지하고 C 표현은 R6 TBD다.

<a id="scalar-aliases"></a>
## 보조 scalar / 불투명 의미 표기

아래는 경계 identity·정책 값의 의미 alias다. 각각 독립 struct/새 ID 생성 API를 만드는 목록이 아니다. 모든 값의 폭/encoding·확정 Header는 미정이다. 실제 producer의 원래 문맥에서 보호하고 전달 뒤 불변으로 사용한다. 회수/무효화는 해당 주 Data의 lifetime을 따른다.

| 표기 | Producer / 의미 owner | 목적 / validity |
| --- | --- | --- |
| `VssAssetKeyType` | 기존 Asset table/의미 정책 | 압축 음원 ID. 물리 주소 아님 |
| `VssImageKeyType` | 현재 image/build 근거 | Asset descriptor/읽기의 실제 이미지 문맥. 과거 bring-up과 구별 |
| `VssAssetMetadataType` | ASSET의 확인된 image metadata | 확인한 MP3 정보. format/bitrate/library는 현 source 확인 전 TBD |
| `VssAssetSpanKeyType` | ASSET | 제공 자료의 실제 소비 참조 연결. Session 종료와 다른 lifespan |
| `VssPcmFormatType` | AUDIO STREAM의 decoder/format 확인 | 확인된 PCM 의미 format. sample rate/channel/폭을 정하지 않음 |
| `VssPcmCycleType` | AUDIO STREAM | A/B 재사용 회차. 폭/증분/wrap/보존 규칙 미정 |

```c
typedef SemanticIdentity VssAssetKeyType;
typedef SemanticIdentity VssImageKeyType;
typedef SemanticDescriptor VssAssetMetadataType;
typedef SemanticIdentity VssAssetSpanKeyType;
typedef SemanticFormat VssPcmFormatType;
typedef SemanticIdentity VssPcmCycleType;
```

Owner header 후보는 해당 Data의 의미 owner 그룹을 따른다. R6에서 기존 Header 재사용/소유를 확인한다. 별도 공통 God Header를 만들지 않는다.

## 공통 TBD / 후속 범위

논리 필드의 실제 폭/숫자·alignment/packing/ABI·storage/복사·참조 수단과 Header filename은 미정이다. RTD/DMA/TCD·`.c/.h` Mapping·Task/Queue/Mutex·실제 C 구현은 작성하지 않았다. [pseudo-C 표기](00_DATA_OVERVIEW.md#pseudo-conventions)와 [기존 Implementation TBD](../90_BINDING/91_IMPLEMENTATION_TBD.md)를 함께 확인한다. 관련 공통 규칙은 [R4 Contract](../50_CONTRACTS/00_CONTRACT_OVERVIEW.md)로 연결했다. Field 표/owner/선언과 local validity는 유지한다.
