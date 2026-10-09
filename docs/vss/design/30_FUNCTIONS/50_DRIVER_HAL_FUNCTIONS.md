# DRIVER / HAL Core Functions — R2

> 2026-10-07 · Baseline / Execution Plan v1.1 · Function 상세 설계만 수행

[최종 Core 색인·후보 판정](00_FUNCTION_OVERVIEW.md) · [provisional type 기준](00_FUNCTION_OVERVIEW.md#provisional-types)

<a id="driver-hal"></a>
## 그룹 경계

AUDIO TX는 실제 전송 요청 전 귀속/접근 보호와 후속 사실의 적용을 구분한다. AUDIO CONTROL은 현재 장치 구성 준비의 지속되는 lifecycle이다. HAL callback의 owner는 HAL/BSP Layer의 HAL Boundary이며 독립 HAL Module을 만들지 않는다. 실제 출력 관측의 물리 충분성은 기존 Binding TBD다.

Core 상세: [AudioTx_Request](50_DRIVER_HAL_FUNCTIONS.md#audiotx-request) · [AudioTx_Advance](50_DRIVER_HAL_FUNCTIONS.md#audiotx-advance) · [AudioControl_Service](50_DRIVER_HAL_FUNCTIONS.md#audiocontrol-service) · [AudioHAL_Callback](50_DRIVER_HAL_FUNCTIONS.md#audiohal-callback)

아래 `const`·포인터·반환 표기는 읽기/반환 방향을 보이는 **pseudo prototype**이다. 타입은 R3에서 상세화할 opaque provisional semantic type이며 C ABI·구조체/필드·enum·Header·실제 symbol은 확정하지 않는다. 후속 처리 동안 필요한 자료는 owner의 원래 문맥으로 보호하고, 임시 인자 포인터를 그대로 장기 보관한다는 뜻은 아니다.

<a id="audiotx-request"></a>
## AudioTx_Request

### R4 연결 — 독립 국소 PASS의 typed 입력 경로

논리 Core 1개와 AUDIO TX 단일 writer를 유지한다. 기존 provisional `VssPcmTransferIntent`는 두 R3 typed 의미로 읽으며 아래 판정은 뒤의 R2 표/prototype을 실제 단일 C signature로 강제하지 않는다.

| typed 입력 / 정당한 출처 | 로컬 수용·identity / 자료 수명 | 거부·부분 효력 적용 |
| --- | --- | --- |
| `VssPcmHandoffCommand` — AUDIO STREAM Prepare/Advance | 안정된 samples·유효 범위/format·원 Attempt/A/B/회차. DRIVER 호출 전 HANDOFF_PENDING; >0 유효 음향만 | 명확한 무접근 거부의 근거만 반환. 부분 retain/효력은 보호·정리 유지 |
| `VssTxControlCommand` START/STOP — AUDIO STREAM RequestControl/Advance | 원 Attempt·scope와 필요한 최초 START에만 firstStartMeta. PCM 주소 없이 제어; `VssTxOperation.handoff`는 null | 수용·장치 활성화·actual 분리. STOP/IRQ/0채움은 안전 반환/출력 종료 확정 아님 |

[Buffer §7](../50_CONTRACTS/50_AUDIO_BUFFER_TRANSPORT.md#tx-typed-lanes)의 typed 의미와 [지속 등록/개별 operation](../50_CONTRACTS/50_AUDIO_BUFFER_TRANSPORT.md#continuous-stream)을 적용한다. 개별 software operation 수명은 장기 vendor 등록 수명과 1:1이 아니며 현재 Attempt로 사후 재귀속하지 않는다. 비동기 사실은 기존 `AudioTx_Advance`가 반영한다. 논리 Core 총수 18·Caller/Callee/Writer·HAL Boundary 예외는 보존한다. 실제 C 진입점/원형은 R6, vendor 대응/물리 충분조건은 B2-R TBD다.

| 항목 | 상세 계약 |
| --- | --- |
| 역할 | AUDIO STREAM의 PCM 인계/start/정리 요구를 검토하고 원래 출력 시도·버퍼 주기와 HAL 등록을 보호한 뒤 실제 전송 제어 요청을 연결한다. |
| Owner | [AUDIO TX](../20_MODULES/30_AUDIO_TX_MODULE.md) Module. Caller의 요청은 이 owner의 원본 상태를 직접 쓰는 권한이 아니다. |
| Caller | AUDIO STREAM의 [AudioStream_Prepare](40_AUDIO_STREAM_ASSET_FUNCTIONS.md#audiostream-prepare), [AudioStream_RequestControl](40_AUDIO_STREAM_ASSET_FUNCTIONS.md#audiostream-requestcontrol), [AudioStream_Advance](40_AUDIO_STREAM_ASSET_FUNCTIONS.md#audiostream-advance). FLOW/PLAYBACK은 DRIVER를 직접 호출하지 않는다. |
| Callee | HAL/BSP의 기존 전송 요청·등록·장치 접근 경계와 필요한 Runtime 도구. 논리 경계만 연결하며 RTD symbol/물리 DMA 절차를 정하지 않는다. |
| Input | VssPcmTransferIntent: 해당 Attempt·PCM 주기·유효 전송 구간의 인계/start/정리 의도. 전체 버퍼 용량을 자동으로 유효 길이로 간주하지 않는다. 해석 전 HAL 결과는 이 요청 입력이 아니다. |
| Return | VssTxProgress: 원래 요구의 수용+장치 활성화 여부·접근 보호·거부/부분 효력·정리 진행 근거. 요청 수락은 실제 출력이나 안전 반환을 증명하지 않는다. |
| Reads | AUDIO TX의 전송 operation·PCM 구간/주기 보호와 HAL 원래 등록 수명 및 현재 전송 제약. |
| Writes | AUDIO TX의 해당 operation·하위 접근 보호/진행 문맥만 쓴다. HAL 등록은 HAL owner의 경계가 보존하며 AUDIO STREAM의 CPU 내용/생산 권한을 직접 변경하지 않는다. |
| Precondition | 의도와 원래 Attempt/버퍼 주기·유효 범위가 연결되어야 한다. 새 인계는 CPU 생산이 끝나고 안정된 자료여야 한다. 기존 보호를 해제할 수 없는 상태의 신규 요구는 거부/대기한다. |
| Postcondition | 하위 호출 전에 원래 귀속과 pending 인계 보호를 확보한다. 성공/부분 효력은 미래 접근 가능성이 끝날 때까지 보호한다. 거부가 CPU 재사용을 허용하려면 해당 요청으로 하위 접근이 전혀 생기지 않았거나 끝났다는 근거가 필요하다. |
| Side Effect | HAL에 제한된 실제 전송/start/abort·정리를 요청하고 반환 즉시 사실을 원래 operation으로 보호한다. 후속 IRQ 사실은 [AudioTx_Advance](50_DRIVER_HAL_FUNCTIONS.md#audiotx-advance)로 적용한다. |
| Failure behavior | 등록/범위/자원 부족은 새 요청만 거부한다. vendor 실패 반환에도 부분 활성화/미래 접근 가능성이 있으면 일반 거부로 rollback하지 않는다. 정리 실패는 기존 보호와 귀속을 유지해 보고한다. |
| Invariant | pending 인계부터 CPU 덮어쓰기 금지. 전달받은 PCM 유효 구간과 반복/재접근 범위를 보호하며 accepted+장치 활성화 ≠ actual output start. |
| Forbidden behavior | 하위 요청 후 귀속 등록, 오류 반환만으로 사용권 반환, Session의 출력 정리와 사용권 종료로 HAL callback 기록 회수, RTD/descriptor/PCM 주소를 Service 상위에 노출, POLICY 판단·Fault 원본 쓰기 금지. |
| 동기 / 비동기 | 동기 요청 수용과 즉시 하위 반환을 처리한다. 비동기 실제 효과/안전 반환은 별도 Advance 계약이며 물리 polling/IRQ/DMA 선택은 TBD다. |
| 관련 Data | [PCM A/B 사용권](../40_DATA/50_AUDIO_STREAM_DATA.md#pcm) · [TX operation·HAL 귀속 기록](../40_DATA/60_AUDIO_TX_DATA.md#tx) |
| 관련 Contract | [소유·수명](../50_CONTRACTS/00_CONTRACT_OVERVIEW.md#ownership) · [비동기 결과·근거](../50_CONTRACTS/00_CONTRACT_OVERVIEW.md#async) · [버퍼·전송](../50_CONTRACTS/00_CONTRACT_OVERVIEW.md#buffer) — 기존 원문 연결이며 R4 통합은 수행하지 않는다. |

### Pseudo prototype — provisional

```c
VssTxProgress AudioTx_Request(
    const VssPcmTransferIntent *intent
);
```

**Core 경계 판정:** AudioTx_Process의 SPLIT 대상. 요청 전 등록·pending 보호와 새 요구 거부는 이미 발생한 해석 전 장치 사실/접근 종료를 적용하는 의무와 다르다. Submit/Start/Stop wrapper는 같은 전송 제어의 내부 분기로 둔다.

<a id="audiotx-advance"></a>
## AudioTx_Advance

| 항목 | 상세 계약 |
| --- | --- |
| 역할 | HAL이 보존한 원래 등록의 사실과 현재 시간을 전송 operation에 적용하여 실제 출력 후보·관측 손실·해당 PCM 주기의 안전 반환/종료 근거를 제공한다. |
| Owner | [AUDIO TX](../20_MODULES/30_AUDIO_TX_MODULE.md) Module. Caller의 요청은 이 owner의 원본 상태를 직접 쓰는 권한이 아니다. |
| Caller | AUDIO STREAM의 [AudioStream_Advance](40_AUDIO_STREAM_ASSET_FUNCTIONS.md#audiostream-advance). DRIVER의 제한된 같은 operation 후속 처리가 가능하며 HAL callback은 이 함수를 ISR 안에서 역호출하지 않는다. |
| Callee | HAL/BSP의 기존 원사실/현재 장치 근거 관측·필요한 정리/등록 수명 경계와 Runtime 도구. callback의 raw 보존은 HAL Boundary의 별도 진입이며 DRIVER가 그 callback 함수를 호출하지 않는다. |
| Input | VssTimeEvidence와 현재 operation. 해석 전 장치 사실은 HAL owner의 기존 보존 경계에서 읽고 원래 등록·발생 시각·관측 범위/손실을 유지한다. 알림 횟수나 현재 Attempt를 원래 귀속 대신 쓰지 않는다. |
| Return | VssTxProgress: 귀속된 수용/활성화·실제 출력 후보·전송 원본 구간 소비·해당 주기 안전한 CPU 재사용·과거 무출력/미래 차단·출력 종료·불명/손실 근거. cue/Attempt/전체 출력의 범위를 혼동하지 않으며 PLAYBACK 최종 outcome은 반환하지 않는다. |
| Reads | AUDIO TX의 operation·원래 PCM 주기/반복 접근 보호, HAL 원래 등록/시각/관측 사실과 장치 현재 근거. |
| Writes | AUDIO TX의 전송 사실 적용·하위 접근 보호/해제 근거·정리/진단 문맥만 쓴다. HAL 기록의 보존/회수는 HAL 경계가 수행하며 AUDIO STREAM의 CPU 사용권은 결과를 받은 owner가 반영한다. |
| Precondition | 원래 사실 귀속을 확인하거나 부족한 근거로 구별한다. 불완전/늦은/중복도 입력이다. 안전한 CPU 재사용은 해당 주기의 모든 미래 장치 접근이 불가능한 근거를 요구하며 반복/재접근 설정을 포함한다. |
| Postcondition | 충분한 근거만 실제 출력 후보/해당 주기 반환/출력 종료로 내보낸다. 전송 원본 구간 소비는 접근 종료와 별도다. 옛 결과는 옛 operation 정리·진단에만 적용하고 현재/새 주기 보호를 해제하지 않는다. 실제 출력 후보는 해당 요청/장치 활성화 이후 새로 관측된 원래 출력 시도의 사실에만 근거하며, 이미 존재한 장치 상태를 새 실제 출력으로 바꾸지 않는다. |
| Side Effect | 필요한 하위 정리 진행·전송 보호 갱신·진단/결과 보존. HAL 기록은 callback/late 가능성·참조 수명이 끝났다는 별도 근거까지 유지한다. |
| Failure behavior | IRQ/시간/관측 누락·overflow·장치 활성화 근거 부족은 확정 근거 부족으로 반환하고 보호를 유지한다. 현재 장치 정지만으로 과거 무출력을 만들지 않는다. abort/error/부분 transfer도 미래 접근 종료가 증명되기 전 안전 반환하지 않는다. |
| Invariant | 과거 무출력과 옛 미래 출력 불가능은 서로 다른 근거다. DRIVER만으로 상위 후속 PCM/cue/start 권한 차단을 증명하지 않는다. 출력의 물리 의미/지연/관측 충분성은 Binding TBD이며 부족하면 확정하지 않는다. |
| Forbidden behavior | callback 시각을 후속 처리 시각으로 교체, 현재 operation 재귀속, raw TX complete를 speaker 전체 종료로 합성, consumed/abort ack만으로 안전한 CPU 재사용, 손실 덮어쓰기·옛 결과로 새 버퍼 반환 금지. |
| 동기 / 비동기 | 동기 사실 적용과 제한된 정리 진행이다. 사실은 비동기 callback 또는 기존 polling 경계에서 발생할 수 있다. callback은 raw 보존만 하고 이 함수가 DRIVER 의미를 적용한다. |
| 관련 Data | [TX operation·HAL 귀속 기록](../40_DATA/60_AUDIO_TX_DATA.md#tx) · [PCM A/B 사용권](../40_DATA/50_AUDIO_STREAM_DATA.md#pcm) · [Runtime 시간·알림·손실](../10_LAYERS/details/50_TIMEBASE_RUNTIME_BOUNDARY.md) |
| 관련 Contract | [소유·수명](../50_CONTRACTS/00_CONTRACT_OVERVIEW.md#ownership) · [비동기 결과·근거](../50_CONTRACTS/00_CONTRACT_OVERVIEW.md#async) · [버퍼·전송](../50_CONTRACTS/00_CONTRACT_OVERVIEW.md#buffer) — 기존 원문 연결이며 R4 통합은 수행하지 않는다. |

### Pseudo prototype — provisional

```c
VssTxProgress AudioTx_Advance(
    const VssTimeEvidence *time
);
```

**Core 경계 판정:** AudioTx_Process의 SPLIT 대상. 이미 발생한 사실의 귀속/부분 실패·접근 종료 적용은 신규 요청의 거부와 독립이다. callback별 wrapper/NoStart helper를 Core로 늘리면 같은 보호 수명을 복제한다.

<a id="audiocontrol-service"></a>
## AudioControl_Service

| 항목 | 상세 계약 |
| --- | --- |
| 역할 | 원래 장치/구성의 준비·정리 또는 허용 복구 제어를 진행하고 해당 구성에서 유효한 준비/무효화와 실제 제어 결과를 반환한다. |
| Owner | [AUDIO CONTROL](../20_MODULES/31_AUDIO_CONTROL_MODULE.md) Module. Caller의 요청은 이 owner의 원본 상태를 직접 쓰는 권한이 아니다. |
| Caller | AUDIO STREAM의 [AudioStream_Prepare](40_AUDIO_STREAM_ASSET_FUNCTIONS.md#audiostream-prepare), [AudioStream_RequestControl](40_AUDIO_STREAM_ASSET_FUNCTIONS.md#audiostream-requestcontrol), [AudioStream_Advance](40_AUDIO_STREAM_ASSET_FUNCTIONS.md#audiostream-advance). FLOW/PLAYBACK/HEALTH의 장치 직접 호출은 없다. |
| Callee | HAL/BSP의 기존 clock/codec 등 장치 제어·원사실 보존 경계와 Runtime 도구. 실제 초기화 순서·RTD/I2C symbol은 정하지 않는다. |
| Input | VssDeviceControlIntent와 VssTimeEvidence: 현재 대상/구성의 준비·정리/허용 복구 제어 의도 또는 이미 수용한 제어의 진행 기회. 해석 전 하위 결과는 HAL의 보호된 해당 제어 문맥에서 읽는다. |
| Return | VssDeviceReadiness: 원래 제어의 수용/진행·현재 구성 준비/무효화·수행/검증 근거와 단계별 실패. mute/장치 준비는 과거 무출력 또는 실제 speaker 출력 시작의 증명이 아니다. |
| Reads | AUDIO CONTROL의 장치별 구성/준비 lifecycle과 원래 제어·HAL 귀속된 결과·현재 시간. |
| Writes | AUDIO CONTROL의 장치 구성 준비/무효화·제어 진행/진단만 쓴다. PCM 전송 보호·PLAYBACK Session·HEALTH Fault는 변경하지 않는다. |
| Precondition | 제어 대상/현재 구성과 원래 요구의 관계를 확인한다. 이미 진행 중인 제어/부분 준비와 부적합한 새 요구를 구별한다. 복구는 상위에서 허용·안전 정리된 대상/범위를 전달받아야 한다. |
| Postcondition | 실제 성공이 확인된 현재 구성에 한해 준비를 유효화한다. 구성 변화/고장 시 기존 준비를 무효화한다. 같은 요구의 즉시 반환과 후속 사실은 같은 제어 수명으로 적용하고 옛 구성의 성공으로 새 준비를 만들지 않는다. Codec·Clock Generator·Clock Reference는 해당 장치/구성별로 확인하며 한 장치의 성공을 다른 장치의 준비 사실로 확장하지 않는다. |
| Side Effect | 원래 장치 구성/정리·복구 제어 요청과 진행. 필요 시 준비 근거를 무효화하며 실제 수행 근거와 효과 검증 근거를 구별해 상위로 제공한다. |
| Failure behavior | 제어 실패/timeout/부분 성공은 원래 대상·단계·구성으로 반환하고 준비를 보장하지 않는다. 준비 없음/초기 미평가를 확인된 초기화 실패로 자동 바꾸지 않는다. 장치 mute 성공으로 TX 보호 해제를 요청하지 않는다. |
| Invariant | 장치 구성 준비는 여러 출력 Attempt에 걸쳐 유지될 수 있다. 새 Attempt마다 무조건 재초기화하지 않는다. 현재 구성에서 검증된 준비와 옛 요청 성공은 다르다. |
| Forbidden behavior | PCM 버퍼의 CPU 사용권·STORE 발생 이력·PLAYBACK Session·HEALTH Fault 원본 쓰기, 옛 복구 성공으로 현재 Fault 해제, 장치 준비를 실제 출력 성공으로 합성, 회복 위해 원본 age/재실행 초기화 금지. |
| 동기 / 비동기 | 동기 요청/진행 경계이며 장치 제어 완료는 비동기일 수 있다. 한 owner의 지속되는 구성 준비·무효화 수명이라 요청/결과별 함수나 장치별 trivial wrapper는 만들지 않는다. |
| 관련 Data | [장치별 구성 준비](../40_DATA/60_AUDIO_TX_DATA.md#control) · [TX operation·HAL 귀속 기록](../40_DATA/60_AUDIO_TX_DATA.md#tx) · [Fault·복구 문맥](../40_DATA/70_DIAGNOSTIC_DATA.md#health) · [Runtime 시간·알림·손실](../10_LAYERS/details/50_TIMEBASE_RUNTIME_BOUNDARY.md) |
| 관련 Contract | [소유·수명](../50_CONTRACTS/00_CONTRACT_OVERVIEW.md#ownership) · [비동기 결과·근거](../50_CONTRACTS/00_CONTRACT_OVERVIEW.md#async) · [고장·복구](../50_CONTRACTS/00_CONTRACT_OVERVIEW.md#fault) — 기존 원문 연결이며 R4 통합은 수행하지 않는다. |

### Pseudo prototype — provisional

```c
VssDeviceReadiness AudioControl_Service(
    const VssDeviceControlIntent *intent,
    const VssTimeEvidence *time
);
```

**Core 경계 판정:** KEEP. 호출자·owner·현재 구성 준비의 상태전이/실패가 같다. TX의 PCM 보호와는 별개지만 제어 수용·후속 장치 진행을 분리할 독립 권한/호출 계약은 없으므로 한 구성 수명 경계로 남긴다.

<a id="audiohal-callback"></a>
## AudioHAL_Callback

| 항목 | 상세 계약 |
| --- | --- |
| 역할 | vendor callback/IRQ에서 원래 등록에 연결된 해석 전 장치 사실·발생 시각·관측 범위/손실을 짧게 보존하고 기존 후속 처리를 알린다. |
| Owner | [HAL Boundary](../10_LAYERS/details/40_HAL_BSP_BOUNDARY.md) — HAL/BSP Layer 내부 Boundary. 호출 vendor/IRQ가 owner인 것은 아니다. |
| Caller | 기존 vendor callback/IRQ 진입. 실제 callback signature·source symbol·실행 문맥은 Binding 단계의 미정 항목이다. |
| Callee | HAL/BSP의 원래 등록/보존 처리와 필요한 최소 Runtime 시각·손실/알림 도구. DRIVER/SERVICE/FLOW 의미 처리 함수는 ISR에서 호출하지 않는다. |
| Input | VssHalRegistration과 VssRawObservation: 실제 원래 등록의 귀속·유효 수명과 raw 장치/전송 사실·시각/관측 근거. 등록을 확인할 수 없는 사실은 현재 Attempt로 붙이지 않고 귀속 부족/손실로 보존한다. |
| Return | void: 반환값으로 재생 상태를 판정하지 않는다. 보호된 HAL 사실과 후속 처리 알림이 observable 결과다. 보존 한계/손실도 별도 근거로 남긴다. |
| Reads | HAL의 원래 등록과 callback 수명, 원래 operation/버퍼 주기 및 장치 raw 관측·Runtime의 비교 가능한 발생 시각. |
| Writes | HAL Boundary의 원사실·원래 귀속/시각·관측 손실/보존 상태와 최소 알림만 쓴다. TX operation·PCM 내용/CPU 사용권·Session/Fault를 쓰지 않는다. |
| Precondition | 안전하게 읽을 수 있는 callback 근거가 있어야 한다. 등록 부재/이미 종료/손실은 거부 전제조건이 아니라 명시적으로 보존할 오류 입력이다. 보존을 위한 수명·한계는 실제 Binding에서 입증해야 한다. |
| Postcondition | 사실은 등록 당시의 귀속/버퍼 주기와 함께 보호된다. 중복/late도 원래 자료로 남기며 알림 손실이 사실의 조용한 삭제를 뜻하지 않는다. 후속 처리가 현재 상태를 대신 붙이지 않는다. |
| Side Effect | 기존 raw 처리/필요한 장치 acknowledge·기록 보호·짧은 알림. acknowledge 자체는 no-start/안전 반환/종료 증명이 아니다. Task/Queue/Mutex 방식은 정하지 않는다. |
| Failure behavior | 귀속 확인 실패·관측 불가·overflow·등록/시각 연속성 손실을 드러낸다. 성공/완료로 변환하거나 현재 Attempt를 추정하지 않는다. 보존 수단이 충분하지 않으면 후속 owner는 결과를 확정할 수 없다. |
| Invariant | Session의 출력 정리와 사용권 종료 및 HAL 등록/late callback 수명은 별개다. 짧은 ISR은 해석 전 장치 사실 보존 경계이며 의미 결과/소유권 최종 판정은 상위 owner가 수행한다. |
| Forbidden behavior | CPU 디코딩/PCM 덮어쓰기, buffer 반환, actual/no-start/termination 최종 합성, SELECT/PLAYBACK/HEALTH 호출, 현재 시각/현재 Session으로 원래 귀속 재생산 금지. |
| 동기 / 비동기 | 비동기 callback/ISR 진입이며 작업은 짧은 동기 보존으로 끝낸다. 알림 뒤의 사실 적용은 [AudioTx_Advance](50_DRIVER_HAL_FUNCTIONS.md#audiotx-advance)/[AudioControl_Service](50_DRIVER_HAL_FUNCTIONS.md#audiocontrol-service) 등 해당 DRIVER owner의 후속 실행이다. |
| 관련 Data | [TX operation·HAL 귀속 기록](../40_DATA/60_AUDIO_TX_DATA.md#tx) · [장치별 구성 준비](../40_DATA/60_AUDIO_TX_DATA.md#control) · [Runtime 시간·알림·손실](../10_LAYERS/details/50_TIMEBASE_RUNTIME_BOUNDARY.md) |
| 관련 Contract | [소유·수명](../50_CONTRACTS/00_CONTRACT_OVERVIEW.md#ownership) · [비동기 결과·근거](../50_CONTRACTS/00_CONTRACT_OVERVIEW.md#async) · [버퍼·전송](../50_CONTRACTS/00_CONTRACT_OVERVIEW.md#buffer) — 기존 원문 연결이며 R4 통합은 수행하지 않는다. |

### Pseudo prototype — provisional

```c
void AudioHAL_Callback(
    const VssHalRegistration *registration,
    const VssRawObservation *observation
);
```

**Core 경계 판정:** KEEP. 실제 HAL 진입과 원사실 보존 수명 경계다. Owner는 HAL/BSP Layer의 HAL Boundary이며 HAL Module을 만들지 않는다. DRIVER 진행과 합치면 ISR의 raw 보존/짧은 실행 계약이 사라진다.

## 계약 구분 검토 사례

실제 C 테스트 구현이 아니라, 함수 병합/내부화 시 보존해야 할 observable behavior를 확인하는 R2 설계 검수다.

| 상황 | 계약상 기대 결과 |
| --- | --- |
| 요청 반환 전에 즉시 callback 가능 | [AudioTx_Request](50_DRIVER_HAL_FUNCTIONS.md#audiotx-request)는 하위 호출 전에 귀속/접근 보호를 확보한다. [AudioHAL_Callback](50_DRIVER_HAL_FUNCTIONS.md#audiohal-callback)은 원래 등록으로 사실을 보존한다. |
| 전송 원본 구간 소비 또는 abort/error ack | [AudioTx_Advance](50_DRIVER_HAL_FUNCTIONS.md#audiotx-advance)는 해당 주기의 미래 접근 불가능이 입증되기 전 안전한 CPU 재사용으로 바꾸지 않는다. 부분 효력은 보호된다. |
| 현재 장치 정지·관측 손실 | 현재 정지만으로 과거 무출력을 증명하지 않는다. 손실 범위가 충분성에 영향을 주면 no-start/종료를 확정하지 않는다. |
| 옛 callback 또는 옛 구성 준비 성공 | 원래 operation/구성에 적용하거나 격리한다. 새 PCM 주기 반환·현재 구성 준비로 재귀속하지 않는다. |

## 후속 범위

[Data 의미](../40_DATA/00_DATA_OVERVIEW.md) · [공통 계약의 기존 위치](../50_CONTRACTS/00_CONTRACT_OVERVIEW.md) · [기존 의사코드](../60_PSEUDOCODE/00_PSEUDOCODE_OVERVIEW.md) · [Binding / 정책 TBD](../90_BINDING/91_IMPLEMENTATION_TBD.md)

R3에서 Data/Command/Result/Evidence의 표현·필드·수명 전달을 구체화한다. R4 Contract 통합, R5 Pseudocode 재작성, R6 Header ownership, R7 통합 검수와 B2-R Binding은 후속이다. 이번 문서는 실제 `.c/.h`/RTD·DMA·TCD/Task·Queue·Mutex/구현 순서와 실제 C 구현을 결정하지 않는다.
