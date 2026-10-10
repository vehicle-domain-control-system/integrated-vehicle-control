# AUDIO STREAM / ASSET Core Functions — R2

> 2026-10-07 · Baseline / Execution Plan v1.1 · Function 상세 설계만 수행

[최종 Core 색인·후보 판정](00_FUNCTION_OVERVIEW.md) · [provisional type 기준](00_FUNCTION_OVERVIEW.md#provisional-types)

<a id="audio-service"></a>
## 그룹 경계

ASSET은 압축 자료와 실제 소비 주체 참조 수명, AUDIO STREAM은 준비/음원 처리 문맥·PCM 생산과 사용권을 소유한다. 의미 준비, 제어 의도 수용, 진행 중 하위 사실/PCM 반영을 구분한다. CPU 디코딩/prime/PCM 보충의 공유 처리는 AUDIO STREAM 내부에 둔다.

Core 상세: [Asset_Read](40_AUDIO_STREAM_ASSET_FUNCTIONS.md#asset-read) · [AudioStream_Prepare](40_AUDIO_STREAM_ASSET_FUNCTIONS.md#audiostream-prepare) · [AudioStream_RequestControl](40_AUDIO_STREAM_ASSET_FUNCTIONS.md#audiostream-requestcontrol) · [AudioStream_Advance](40_AUDIO_STREAM_ASSET_FUNCTIONS.md#audiostream-advance)

아래 `const`·포인터·반환 표기는 읽기/반환 방향을 보이는 **pseudo prototype**이다. 타입은 R3에서 상세화할 opaque provisional semantic type이며 C ABI·구조체/필드·enum·Header·실제 symbol은 확정하지 않는다. 후속 처리 동안 필요한 자료는 owner의 원래 문맥으로 보호하고, 임시 인자 포인터를 그대로 장기 보관한다는 뜻은 아니다.

<a id="asset-read"></a>
## Asset_Read

| 항목 | 상세 계약 |
| --- | --- |
| 역할 | 요청 Asset와 압축 byte 범위를 확인하여 제한된 읽기 자료를 공급하고, 이미 제공한 자료의 실제 소비 주체 참조 종료를 같은 읽기 수명 안에서 반영한다. |
| Owner | [ASSET](../20_MODULES/25_ASSET_MODULE.md) Module. Caller의 요청은 이 owner의 원본 상태를 직접 쓰는 권한이 아니다. |
| Caller | AUDIO STREAM의 [AudioStream_Prepare](40_AUDIO_STREAM_ASSET_FUNCTIONS.md#audiostream-prepare)와 [AudioStream_Advance](40_AUDIO_STREAM_ASSET_FUNCTIONS.md#audiostream-advance) 안의 내부 PCM 생산/정리 처리. FLOW의 기존 가용 관측은 이 읽기 호출과 별개인 읽기 전용 의미 경계다. |
| Callee | read-only image와 필요한 기존 Runtime 도구. build-time BSP 이미지/배치 근거를 이용하며 런타임 Flash erase/program 또는 Storage Driver를 호출하지 않는다. |
| Input | VssAssetAccess: Asset/요청 압축 범위의 읽기 요구 또는 이미 제공한 자료의 실제 참조 진행·종료 근거. 읽기와 참조 종료는 ASSET 읽기 수명의 제한된 의미이며 범용 명령 묶음이 아니다. |
| Return | VssAssetReadResult: 확인한 metadata·가용 범위·제한된 압축 bytes/유효 범위·읽기 또는 참조 반영 결과와 발견 단계. 아직 읽지 않은 전체 MP3의 디코딩 성공을 포함하지 않는다. |
| Reads | 해당 image/build의 descriptor·metadata·배치/길이와 기존 제공 자료의 소비 주체 참조 관계. |
| Writes | ASSET의 확인된 가용/읽기 근거와 제공 자료의 참조 관계만 쓴다. read-only image와 디코더의 소비 위치·PCM은 변경하지 않는다. |
| Precondition | 요청이 유효 image/build에 연결되어야 한다. Asset·범위 overflow·참조 귀속은 내부 검증 대상이며 부적합한 입력도 실패 경로로 다룬다. 참조 종료는 실제 소비 주체가 더 읽지 않을 근거가 필요하다. |
| Postcondition | 성공 자료는 확인된 범위에 한해 유효하며 실제 소비 주체 참조가 끝날 때까지 보존된다. 참조 종료 반영은 해당 자료에만 적용하고 다른 제공 자료를 해제하지 않는다. |
| Side Effect | 읽기 수행과 제공 자료 보호/참조 반영. direct Flash 또는 staging 방식·구체 메모리 복사는 정하지 않는다. |
| Failure behavior | 누락/metadata/bounds/PFlash read 단계의 확인된 실패를 원래 Asset/요청 범위로 반환한다. 실패 시에도 기존 소비 주체의 자료는 유지한다. 디코딩 corrupt/unsupported를 ASSET 발견 원인으로 바꾸거나 다른 음원으로 대체하지 않는다. |
| Invariant | lookup/read 성공 ≠ 전체 디코딩 성공. Session 종료/출력 정리와 사용권 종료 ≠ 압축 자료의 실제 소비 주체 참조 종료. 읽기 전용 저장 경로와 디코더 진행 owner를 분리한다. |
| Forbidden behavior | 이미 소비 중인 span/staging 회수·덮어쓰기, 범위 초과 읽기·길이 overflow 묵인, Asset ID 대체, PCM/Session/Fault 원본 변경 금지. |
| 동기 / 비동기 | 요청 범위를 확인하고 읽기 결과/참조 반영을 반환하는 동기 의미 경계다. 반환된 자료 참조는 호출 이후 지속될 수 있으며 그 종료 근거를 별도로 반영한다. 물리 읽기 방식은 TBD다. |
| 관련 Data | [Asset 설명·읽기 참조](../40_DATA/50_AUDIO_STREAM_DATA.md#asset) · [준비·음원 처리 문맥·Backend 결과](../40_DATA/50_AUDIO_STREAM_DATA.md#provider) |
| 관련 Contract | [소유·수명](../50_CONTRACTS/00_CONTRACT_OVERVIEW.md#ownership) · [고장·복구](../50_CONTRACTS/00_CONTRACT_OVERVIEW.md#fault) — 기존 원문 연결이며 R4 통합은 수행하지 않는다. |

### Pseudo prototype — provisional

```c
VssAssetReadResult Asset_Read(
    const VssAssetAccess *access
);
```

**Core 경계 판정:** KEEP. 압축 자료의 실제 Module 경계와 참조 수명이다. lookup/read/release별 wrapper는 같은 owner·자료 수명을 복제하고 독립 호출/실패 계약을 만들지 않으므로 이 경계의 내부 분기로 둔다.

<a id="audiostream-prepare"></a>
## AudioStream_Prepare

| 항목 | 상세 계약 |
| --- | --- |
| 역할 | PLAYBACK의 의미 계획에 필요한 음원/음원 처리 문맥·장치 준비와 초기 PCM 생산을 연결하여, 실제 start와 구별되는 준비 결과를 만든다. |
| Owner | [AUDIO STREAM](../20_MODULES/24_AUDIO_STREAM_MODULE.md) Module. Caller의 요청은 이 owner의 원본 상태를 직접 쓰는 권한이 아니다. |
| Caller | PLAYBACK의 [Playback_RequestTransition](30_POLICY_PLAYBACK_FUNCTIONS.md#playback-requesttransition) 또는 [Playback_Advance](30_POLICY_PLAYBACK_FUNCTIONS.md#playback-advance). 같은 요청의 후속 준비는 AUDIO STREAM의 [AudioStream_Advance](40_AUDIO_STREAM_ASSET_FUNCTIONS.md#audiostream-advance)가 진행한다. |
| Callee | [Asset_Read](40_AUDIO_STREAM_ASSET_FUNCTIONS.md#asset-read), [AudioControl_Service](50_DRIVER_HAL_FUNCTIONS.md#audiocontrol-service), 필요한 [AudioTx_Request](50_DRIVER_HAL_FUNCTIONS.md#audiotx-request) 및 내부 AudioStream_FillBuffer 처리. 준비에 필요한 하위 등록/제한된 인계가 있더라도 실제 출력 start를 허가하지 않는다. |
| Input | VssAudioPreparation: 원래 전체 계획/현재 cue·Session/Attempt·음원·첫 시작 기한의 의미 요청. 같은 준비를 이어갈 때도 원래 identity/시간을 유지한다. PCM 주소·물리 전송 설정은 PLAYBACK 입력이 아니다. |
| Return | VssAudioBackendResult: 원래 준비의 수용/진행/준비 완료 또는 실패 근거. 준비 완료는 필요한 현재 음원/음원 처리 문맥/장치 준비와 초기 PCM 조건이 충족되었다는 의미이며 실제 출력 시작을 증명하지 않는다. |
| Reads | AUDIO STREAM의 준비/음원 처리 문맥·PCM A/B 사용권과 현재 장치 구성 준비·Asset 결과, 전달된 원래 계획/기한. |
| Writes | AUDIO STREAM의 준비 문맥·음원 처리 문맥 진행·CPU 사용 가능한 PCM 내용/유효 범위와 로컬 권한만 쓴다. 하위 참조/TX/장치 상태는 해당 callee owner가 변경한다. |
| Precondition | 원래 준비의 귀속이 유지되어야 한다. 다른 Session의 이전 전체 출력 정리와 사용권 종료 및 자원 안전 조건이 충족되지 않으면 새 준비를 수용하지 않는다. 같은 Session 정상 cue 진행도 이전 구간의 보호 자원을 침범하지 않는다. |
| Postcondition | 수용 문맥을 하위 호출보다 먼저 보호한다. 즉시 완료 또는 후속 진행으로 원래 준비 결과를 보존하며 최신 장치 구성에 준비가 유효해야 한다. 실제 start 권한은 별도 RequestControl의 재검사 전까지 생기지 않는다. |
| Side Effect | 읽기/CPU 디코딩·PCM 초기 생산과 장치 준비 요청. 같은 준비의 미완료 단계는 Advance로 이어지고 호출 안에서 완료를 무제한 기다리지 않는다. |
| Failure behavior | ASSET read와 AUDIO STREAM 디코딩/음원 처리 문맥 실패의 발견 owner·원인·단계를 보존한다. 부분 준비 뒤 실패/취소는 CPU 생산·후속 제출을 차단하고 이미 인계한 PCM/압축 참조를 실제 안전 조건까지 보호한다. 다른 의미의 음원 fallback은 없다. |
| Invariant | prepared ≠ start accepted ≠ actual output start. PCM A/B는 두 개이며 CPU 쓰기는 하위 미래 접근이 불가능한 해당 주기에만 허용된다. 준비 재시도/지연은 원래 age를 연장하지 않는다. 짧은 음원은 B가 비거나 tail만 유효할 수 있다. 임의 반복/padding을 만들지 않고 실제 유효 구간만 전송한다. |
| Forbidden behavior | prepare만으로 장치 실제 start, PLAYBACK의 Session/STORE 발생 이력 직접 쓰기, 제3 PCM 버퍼나 새 staging 구조 확정, 소비 중 자료 덮어쓰기, 준비된 옛 요청의 자동 start 금지. |
| 동기 / 비동기 | 동기 요청 수용/즉시 결과 반환과 후속 비동기 준비 진행을 구별한다. 준비 완료 전에는 미완료 근거를 반환하며 원래 요청의 진행은 [AudioStream_Advance](40_AUDIO_STREAM_ASSET_FUNCTIONS.md#audiostream-advance)가 담당한다. |
| 관련 Data | [선택 관측·판단·전체 계획](../40_DATA/30_SELECTION_DATA.md#selection) · [Session·Attempt 문맥](../40_DATA/40_PLAYBACK_DATA.md#playback) · [Asset 설명·읽기 참조](../40_DATA/50_AUDIO_STREAM_DATA.md#asset) · [준비·음원 처리 문맥·Backend 결과](../40_DATA/50_AUDIO_STREAM_DATA.md#provider) · [PCM A/B 사용권](../40_DATA/50_AUDIO_STREAM_DATA.md#pcm) · [장치별 구성 준비](../40_DATA/60_AUDIO_TX_DATA.md#control) |
| 관련 Contract | [소유·수명](../50_CONTRACTS/00_CONTRACT_OVERVIEW.md#ownership) · [버퍼·전송](../50_CONTRACTS/00_CONTRACT_OVERVIEW.md#buffer) · [시간·신뢰성](../50_CONTRACTS/00_CONTRACT_OVERVIEW.md#timing) · [고장·복구](../50_CONTRACTS/00_CONTRACT_OVERVIEW.md#fault) — 기존 원문 연결이며 R4 통합은 수행하지 않는다. |

### Pseudo prototype — provisional

```c
VssAudioBackendResult AudioStream_Prepare(
    const VssAudioPreparation *prepare
);
```

**Core 경계 판정:** KEEP. PLAYBACK의 의미 준비가 압축 음원·장치 준비·PCM 생산으로 바뀌는 실제 Module 경계다. start/stop 제어와 합치면 준비가 실제 start를 허가하는 것처럼 읽히므로 별도 계약을 유지한다.

<a id="audiostream-requestcontrol"></a>
## AudioStream_RequestControl

### R4 연결 — 독립 국소 PASS의 typed 입력 경로

논리 Core 1개와 AUDIO STREAM owner를 유지한다. 아래는 기존 R2 계약의 provisional 입력 표지를 R3 typed 의미에 연결하는 판정이며 뒤의 R2 표/prototype을 C 원형으로 확정하는 변경이 아니다.

| typed 입력 / 정당한 출처 | 로컬 수용 조건·identity / lifetime | 거부·지연/실패 적용 |
| --- | --- | --- |
| `VssPlaybackControlCommand` START — PLAYBACK | 원 Session/Attempt·같은 prepared 문맥·현재 장치 조건. 최초 START에만 원본 gate 보존 | 조건 불충족을 actual로 바꾸지 않음; 기존 참조/결과 보호 유지 |
| 같은 Command STOP — PLAYBACK | 원 Session/Attempt. 하위 정리 요청 전에 미래 decode/refill/handoff 차단 | 하위 실패/지연에도 차단 복원 금지. 옛 결과·정리 추적 |
| `VssRecoveryPermission` — HEALTH 허용을 FLOW가 연결 | [현재 Fault 허용과 안전 조건](../50_CONTRACTS/40_FAULT_RECOVERY.md#current-target). Session 없이 가능하며 가짜 identity 없음 | 무효/옛 허용은 수행 허가 아님. 수행/검증/HEALTH 해제 분리 |

Command 소비와 실제 provider/PCM·미반영 결과 보호 수명은 [Ownership §3](../50_CONTRACTS/10_OWNERSHIP_LIFETIME.md#borrow-and-protect)을 따른다. 실제 후속 사실은 기존 `AudioStream_Advance`가 반영한다. 논리 Core 총수 18과 기존 Caller/Callee/Writer 예외는 그대로이며 실제 C 진입점/typed 표현은 R6, 물리 충분조건은 B2-R TBD다.

| 항목 | 상세 계약 |
| --- | --- |
| 역할 | PLAYBACK의 원래 start/stop 의도 또는 FLOW가 연결한 HEALTH 허용 대상의 복구 의도를 수용하고, 음원 처리 문맥·PCM 제출의 미래 권한을 바꾼 뒤 하위 제어를 연결한다. |
| Owner | [AUDIO STREAM](../20_MODULES/24_AUDIO_STREAM_MODULE.md) Module. Caller의 요청은 이 owner의 원본 상태를 직접 쓰는 권한이 아니다. |
| Caller | PLAYBACK의 [Playback_RequestTransition](30_POLICY_PLAYBACK_FUNCTIONS.md#playback-requesttransition)/[Playback_Advance](30_POLICY_PLAYBACK_FUNCTIONS.md#playback-advance)가 start·정리를 요구한다. FLOW의 [Flow_Process](10_FLOW_FUNCTIONS.md#flow-process)는 HEALTH가 허용한 복구만 조율해 요구한다. |
| Callee | [AudioTx_Request](50_DRIVER_HAL_FUNCTIONS.md#audiotx-request), [AudioControl_Service](50_DRIVER_HAL_FUNCTIONS.md#audiocontrol-service) 및 필요한 내부 음원 처리 문맥 정리. 하위 사실 적용은 [AudioStream_Advance](40_AUDIO_STREAM_ASSET_FUNCTIONS.md#audiostream-advance)로 연결하며 새 POLICY 판단을 수행하지 않는다. |
| Input | VssAudioControlIntent와 VssExecutionConditions: 원래 Session/Attempt의 start/정리 또는 허용된 복구 대상·범위, 최신 기한/제한·이전 자원 안전 조건. 해석 전 callback 사실은 제어 요청과 섞지 않는다. |
| Return | VssAudioBackendResult: 의도 수용/거부·현재 차단/하위 요청 진행 근거. 수용은 실제 start·안전 반환·복구 수행/검증 성공과 각각 다르며 부분 효력도 원래 문맥으로 반환한다. |
| Reads | AUDIO STREAM의 준비/음원 처리 문맥·Attempt별 권한·PCM A/B 보호와 전달된 현재 조건/복구 허용, 하위 현재 준비 관측. |
| Writes | AUDIO STREAM의 start/후속 디코딩·PCM 보충·제출 권한과 정리/복구 요청 문맥만 쓴다. 하위 TX/장치 상태와 HEALTH 제한은 직접 쓰지 않는다. |
| Precondition | start는 같은 준비 문맥·유효 장치 준비·기한·현재 허용이 충족되어야 한다. 정리는 이미 진행 중인 요청의 차단을 허용한다. 복구 수행은 대상 허용과 이전 출력의 정리와 사용권 종료/보호 자원 안전 종료를 충족해야 한다. |
| Postcondition | start 수용은 원래 요청/PCM 주기를 하위 요청 전에 보호한다. stop/취소는 이후 디코딩/PCM 보충/새 인계를 먼저 차단하며 하위 거부/지연에도 차단을 되돌리지 않는다. 복구 수용은 수행 중 문맥만 만들고 효과 검증까지 정상 복귀를 선언하지 않는다. |
| Side Effect | 하위 출력/정리·장치 복구 요청과 AUDIO STREAM의 미래 생산 권한 변경. 안전 반환/실제 수행 결과는 동일 문맥의 Advance에서 적용한다. |
| Failure behavior | start 근거 부족은 신규 start를 거부한다. 하위 호출의 부분 효력/결과 불명은 보호·추적·정리를 유지한다. stop/복구 실패는 원래 대상과 단계로 보고하며 하위 결과가 늦어도 현재/옛 요청을 바꾸어 붙이지 않는다. |
| Invariant | 멈춘 뒤 안전 반환되어도 해당 옛 Attempt의 PCM 보충은 금지된다. 복구 허용 ≠ 수행 ≠ 효과 검증 ≠ HEALTH 해제. 반복 호출은 원래 identity/차단을 유지한다. 첫 실제 시작의 원본 age 조건과 Started 뒤 정상 cue/반복 조건을 혼동하지 않는다. |
| Forbidden behavior | 정리 요청 수락만으로 버퍼/owner 해제, 하위 mute를 출력 종료 증명으로 사용, DEVICE 준비만으로 actual start 합성, 직접 Fault 해제·기한/재실행 초기화, FLOW의 PCM 직접 제어 금지. |
| 동기 / 비동기 | 동기 의도 수용/권한 변경이며 장치 효과·PCM 안전 종료·복구 수행/검증은 후속 비동기 진행일 수 있다. start/stop/복구는 동일 음원 처리 문맥의 명시된 제어 경계에 한정한다. |
| 관련 Data | [준비·음원 처리 문맥·Backend 결과](../40_DATA/50_AUDIO_STREAM_DATA.md#provider) · [PCM A/B 사용권](../40_DATA/50_AUDIO_STREAM_DATA.md#pcm) · [TX operation·HAL 귀속 기록](../40_DATA/60_AUDIO_TX_DATA.md#tx) · [장치별 구성 준비](../40_DATA/60_AUDIO_TX_DATA.md#control) · [Fault·복구 문맥](../40_DATA/70_DIAGNOSTIC_DATA.md#health) |
| 관련 Contract | [소유·수명](../50_CONTRACTS/00_CONTRACT_OVERVIEW.md#ownership) · [비동기 결과·근거](../50_CONTRACTS/00_CONTRACT_OVERVIEW.md#async) · [버퍼·전송](../50_CONTRACTS/00_CONTRACT_OVERVIEW.md#buffer) · [고장·복구](../50_CONTRACTS/00_CONTRACT_OVERVIEW.md#fault) · [시간·신뢰성](../50_CONTRACTS/00_CONTRACT_OVERVIEW.md#timing) — 기존 원문 연결이며 R4 통합은 수행하지 않는다. |

### Pseudo prototype — provisional

```c
VssAudioBackendResult AudioStream_RequestControl(
    const VssAudioControlIntent *intent,
    const VssExecutionConditions *conditions
);
```

**Core 경계 판정:** AudioStream_Process의 SPLIT 대상. 새 제어의 수용·미래 생산 권한 변경과 이미 발생한 하위 사실의 반드시 필요한 적용은 실패/호출 계약이 다르다. Start/Stop/Recovery마다 독립 wrapper를 만드는 대신 같은 음원 처리 문맥 제어 분기로 둔다.

<a id="audiostream-advance"></a>
## AudioStream_Advance

| 항목 | 상세 계약 |
| --- | --- |
| 역할 | 진행 중인 준비/음원 처리 문맥·출력·정리·허용 복구의 하위 사실과 시간을 반영하여 PCM 재사용/보충과 범위가 구별된 Backend 결과를 만든다. |
| Owner | [AUDIO STREAM](../20_MODULES/24_AUDIO_STREAM_MODULE.md) Module. Caller의 요청은 이 owner의 원본 상태를 직접 쓰는 권한이 아니다. |
| Caller | FLOW의 [Flow_Process](10_FLOW_FUNCTIONS.md#flow-process)가 입력 유무와 별개로 필요한 후속 처리 기회를 준다. AUDIO STREAM 내부의 같은 문맥 연속 처리도 가능하나 무제한 반복은 허용하지 않는다. |
| Callee | [AudioTx_Advance](50_DRIVER_HAL_FUNCTIONS.md#audiotx-advance), [AudioControl_Service](50_DRIVER_HAL_FUNCTIONS.md#audiocontrol-service), [Asset_Read](40_AUDIO_STREAM_ASSET_FUNCTIONS.md#asset-read) 및 내부 AudioStream_FillBuffer. 현재 준비를 이어가고 필요하면 [AudioTx_Request](50_DRIVER_HAL_FUNCTIONS.md#audiotx-request)로 유효 PCM을 인계한다. PLAYBACK/HEALTH/STORE를 역호출하지 않는다. |
| Input | VssTimeEvidence와 AUDIO STREAM에 보호된 현재 준비/제어 문맥. 하위 비동기 사실은 TX/CONTROL의 공개 진행 결과로 받으며 FLOW가 PCM/장치 원사실을 해석해 전달하지 않는다. Session이 없는 startup에서도 현재 장치 준비 관측과 이미 수용한 장치 제어의 진행은 가능하다. 실제 최초 초기화 진입/순서의 Binding을 이번에 고정하지 않는다. |
| Return | VssAudioBackendResult: 원래 준비·start 수용/활성화·실제 출력 후보·소비/안전 반환·범위별 무출력/종료·불명/손실 및 복구 수행/별도 효과 검증 근거. 여러 유효 사실의 보존을 한 반환값/알림의 덮어쓰기로 축약하지 않는다. |
| Reads | AUDIO STREAM의 준비/음원 처리 문맥·PCM A/B 주기와 생산/제출 권한, 하위 귀속된 TX/장치 사실·현재 시간. |
| Writes | AUDIO STREAM의 준비 완료/진행·PCM 사용권/유효 내용·후속 생산/정리·복구 진행 및 Backend 결과 보호만 쓴다. PLAYBACK의 최종 Session 판정과 HEALTH 해제는 상위 owner의 책임이다. |
| Precondition | 처리 사실을 원래 요청/Attempt/버퍼 주기에 귀속시키거나 근거 부족으로 구별한다. 늦은/중복/손실도 처리 대상이다. CPU 생산은 해당 버퍼의 미래 하위 접근 불가능과 현재 생산 권한을 모두 요구한다. |
| Postcondition | 안전 반환은 원래 주기에만 적용하고 consumed만으로 반환하지 않는다. 활성·동일 음원 처리 문맥에서만 반환된 PCM을 보충·인계하며 stop 뒤에는 정리만 진행한다. 원래 준비/전체 출력 범위·정리/복구 결과는 별도 의미로 보존된다. |
| Side Effect | 제한된 CPU 디코딩·PCM 보충/인계, 실제 소비 주체 참조 종료 반영과 하위 정리 진행. 허용 복구는 실제 수행 근거를 확보한 뒤 별도 효과 검증 근거를 만들며 결과를 FLOW가 HEALTH에 연결한다. |
| Failure behavior | 디코딩/음원 처리 문맥 실패는 해당 생산·제출을 차단하고 하위 보호는 유지한다. 반환/관측 부족·late는 옛 문맥으로 격리한다. 복구 수행 성공만 있고 검증 실패/미완료면 현재 제한을 해제할 근거로 내보내지 않는다. |
| Invariant | Backend NO_START 근거는 충분한 과거 무출력·옛 미래 출력 불가능과 AUDIO STREAM 후속 PCM 차단을 포함한다. 현재 장치 정지와 과거 무출력은 다르며 PLAYBACK의 상위 권한 차단까지 대신 판정하지 않는다. |
| Forbidden behavior | 전송 원본 구간 소비를 안전한 CPU 재사용으로 변경, 옛 A 반환으로 새 A/B 해제, stop 뒤 PCM 보충, EOS/cue 끝을 전체 Session Completed로 변경, 새 Session 시작 또는 다른 owner 최종 판정/상태 직접 쓰기 금지. |
| 동기 / 비동기 | 호출은 동기 사실 적용/제한된 진행이며 하위 효과는 비동기일 수 있다. 알림은 처리 기회이고 사실은 보호된 원래 근거다. 요청 수용은 RequestControl, 이미 진행 중인 lifecycle의 결과/PCM 생산은 이 함수의 내부 분기다. |
| 관련 Data | [Asset 설명·읽기 참조](../40_DATA/50_AUDIO_STREAM_DATA.md#asset) · [준비·음원 처리 문맥·Backend 결과](../40_DATA/50_AUDIO_STREAM_DATA.md#provider) · [PCM A/B 사용권](../40_DATA/50_AUDIO_STREAM_DATA.md#pcm) · [TX operation·HAL 귀속 기록](../40_DATA/60_AUDIO_TX_DATA.md#tx) · [장치별 구성 준비](../40_DATA/60_AUDIO_TX_DATA.md#control) · [Fault·복구 문맥](../40_DATA/70_DIAGNOSTIC_DATA.md#health) · [Runtime 시간·알림·손실](../10_LAYERS/details/50_TIMEBASE_RUNTIME_BOUNDARY.md) |
| 관련 Contract | [소유·수명](../50_CONTRACTS/00_CONTRACT_OVERVIEW.md#ownership) · [비동기 결과·근거](../50_CONTRACTS/00_CONTRACT_OVERVIEW.md#async) · [버퍼·전송](../50_CONTRACTS/00_CONTRACT_OVERVIEW.md#buffer) · [고장·복구](../50_CONTRACTS/00_CONTRACT_OVERVIEW.md#fault) — 기존 원문 연결이며 R4 통합은 수행하지 않는다. |

### Pseudo prototype — provisional

```c
VssAudioBackendResult AudioStream_Advance(
    const VssTimeEvidence *time
);
```

**Core 경계 판정:** AudioStream_Process의 SPLIT 대상. 하위 결과는 새 제어 거부와 무관하게 귀속·안전 반환·검증 의무를 가진다. 결과 종류별 wrapper나 외부 FillBuffer API는 같은 음원 처리 문맥/PCM 수명을 쪼개므로 만들지 않는다.


<a id="internal-fill-buffer"></a>
## 내부 처리 재검토 — AudioStream_FillBuffer

**INTERNALIZE — 현재 내부 지위를 유지한다.** Owner는 AUDIO STREAM이며 Core 색인/외부 API 수에 포함하지 않는다. `AudioStream_Prepare`의 초기 생산과 `AudioStream_Advance`의 후속 PCM 보충에서 같은 CPU 생산 규칙을 공유한다. 실제 private symbol·내부 helper 수·prototype은 이번에 고정하지 않는다.

| 검토 항목 | 유지할 의미 |
| --- | --- |
| 호출 / 입출력 | 현재 음원 처리 문맥/Asset 참조와 생산이 허용된 PCM A/B의 해당 주기를 이용해 유효 PCM 구간·음원 처리 문맥 진행 또는 디코딩 실패를 같은 owner 내부에 반영한다. Module 밖 caller/결과 계약은 없다. |
| 생산 권한 | pending 인계·전송/반복 접근 중인 주기에는 쓰지 않는다. 전송 원본 구간 소비만으로 생산을 재개하지 않고 실제 안전 반환과 현재 생산 권한을 함께 확인한다. |
| 취소 / stop | 이미 진행한 CPU 처리가 있어도 stop/취소 뒤 새 PCM 제출·PCM 보충은 차단한다. 옛 음원 처리 문맥 결과를 새 Session/Attempt로 붙이지 않는다. |
| 끝 / 실패 | 유효 tail을 버퍼 전체 용량으로 늘리지 않는다. EOS/디코더 끝은 음원 처리 문맥/구간 사실이며 전체 Session Completed/실제 출력 종료가 아니다. 디코딩 corrupt/unsupported는 AUDIO STREAM 발견 단계로 보존한다. |
| 자료 수명 | Asset 압축 자료는 실제 소비 주체 참조 종료 뒤 `Asset_Read`의 참조 진행 의미로 반영한다. Session의 출력 정리와 사용권 종료만으로 소비 중 bytes를 회수하지 않는다. |
| 외부 승격 판단 | 별도 Module owner·독립 입력 수용/실패·새 async boundary가 없다. 외부 FillBuffer API로 만들면 PCM 생산 권한을 상위에 노출하고 Prepare/Advance와 같은 수명 규칙을 복제하므로 내부에 둔다. |

이는 내부 처리의 설계 메모다. 기존 [81의 6번 흐름](../60_PSEUDOCODE/reference/81_VSS_PSEUDOCODE.md)을 재작성하지 않는다.

## 계약 구분 검토 사례

실제 C 테스트 구현이 아니라, 함수 병합/내부화 시 보존해야 할 observable behavior를 확인하는 R2 설계 검수다.

| 상황 | 계약상 기대 결과 |
| --- | --- |
| stop 의도와 PCM 안전 반환 경합 | [AudioStream_RequestControl](40_AUDIO_STREAM_ASSET_FUNCTIONS.md#audiostream-requestcontrol)은 후속 디코딩/PCM 보충/제출을 먼저 차단한다. [AudioStream_Advance](40_AUDIO_STREAM_ASSET_FUNCTIONS.md#audiostream-advance)는 안전 반환된 옛 주기를 정리만 하며 stop 뒤 PCM 보충하지 않는다. |
| partial 준비/디코딩 실패 | 원래 음원 처리 문맥에서 생산·제출을 차단하고 이미 제공/인계한 자료는 실제 참조/미래 접근 종료까지 보호한다. [Asset_Read](40_AUDIO_STREAM_ASSET_FUNCTIONS.md#asset-read)의 read 실패와 디코딩 발견 원인을 바꾸지 않는다. |
| 복구 수용 또는 수행만 확인 | 수용은 수행이 아니고 수행은 효과 검증이 아니다. [AudioStream_Advance](40_AUDIO_STREAM_ASSET_FUNCTIONS.md#audiostream-advance)는 구별된 근거를 반환하며 HEALTH의 현재 제한을 직접 해제하지 않는다. |
| 옛 A 반환 뒤 새 A 주기 사용 중 | 원래 Attempt/버퍼 주기로만 반환을 적용한다. 새 A/B의 CPU 사용권은 해제하지 않는다. |

## 후속 범위

[Data 의미](../40_DATA/00_DATA_OVERVIEW.md) · [공통 계약의 기존 위치](../50_CONTRACTS/00_CONTRACT_OVERVIEW.md) · [기존 의사코드](../60_PSEUDOCODE/00_PSEUDOCODE_OVERVIEW.md) · [Binding / 정책 TBD](../90_BINDING/91_IMPLEMENTATION_TBD.md)

R3에서 Data/Command/Result/Evidence의 표현·필드·수명 전달을 구체화한다. R4 Contract 통합, R5 Pseudocode 재작성, R6 Header ownership, R7 통합 검수와 B2-R Binding은 후속이다. 이번 문서는 실제 `.c/.h`/RTD·DMA·TCD/Task·Queue·Mutex/구현 순서와 실제 C 구현을 결정하지 않는다.
