# Startup — 미평가, 장치 준비와 최초 보고

> R5 · 2026-10-08 · R4 독립 PASS 기준. 논리 실행 절차이며 C signature/초기화 Binding 확정이 아니다.

[Flow 색인·표기 규칙](00_PSEUDOCODE_OVERVIEW.md#notation) · [입력 이후 조율](20_INPUT_TO_SELECTION.md#flow-process) · [Fault/보고](60_FAULT_RECOVERY_FLOW.md)

## 목적·진입·소유권

최초 실행 기회에서 이미 수용한 초기 장치 제어를 진행하고, 미평가와 확인된 실패를 구별한다. FLOW는 조율만 쓴다. AUDIO STREAM은 초기 장치 진행을 연결하고 AUDIO CONTROL만 장치별 readiness를 쓴다. Session/Attempt가 없는 startup은 정상 경로다. 초기 제어 진입의 실물 위치·순서는 기존 Binding TBD이며, 이를 위해 가짜 재생 Session이나 새 Core를 만들지 않는다.

| 경계 | 주요 입력 → 출력 | 쓸 수 있는 원본 |
| --- | --- | --- |
| FLOW / `Flow_Process` | `VssProcessingOpportunity`, `VssTimeEvidence` → `VssFlowProgress` | 조율·후속 필요성 |
| STREAM / `AudioStream_Advance` | 시간, 보호된 초기 제어 → 장치 관측·진단 | STREAM 내부 진행 |
| CONTROL / `AudioControl_Service` | optional `VssDeviceControlIntent`, 시간 → `VssDeviceReadinessState`, `VssDeviceControlResult`, 진단·수행/검증 | Codec/Generator/Reference 각각의 현재 구성 |
| HEALTH / 두 Core | 반영 완료 관측·진단·`VssReadBasis` → 평가·보고 | 현재 제한/허용 또는 파생 보고 |

전제는 owner 변경의 직렬화와 원래 장치 구성·operation 참조 보호다. 보장 방법은 R6/B2-R TBD다. 근거가 없는 장치는 UNASSESSED이며 READY도 FAILED도 아니다.

<a id="flow-startup"></a>
## Flow_Process — 최초 기회 조율

```text
CORE Flow_Process / INITIAL slice
INPUT VssProcessingOpportunity opportunity; VssTimeEvidence now
OUTPUT VssFlowProgress

if opportunity != VSS_OPPORTUNITY_INITIAL:
    continue [20 Input→Selection의 정규 slice]

STEP FLOW의 최초 기회 처리 여부를 확인한다;
     반복 INITIAL 알림으로 이미 수용한 HW 준비 동작을 재발행하지 않는다.
STEP ASSET의 기존 공개 가용 관측을 읽는다; 아직 미확인 image를 가용으로 만들지 않는다.
CALL AudioStream_Advance(now)
     // Session 없이 이미 수용된 초기 제어도 진행. FLOW→CONTROL 직접 호출 없음.
     // 초기 HW 준비 요구 자체의 entry/순서는 [TBD-INIT] 범위에 둔다.
STEP 원래 producer의 VssDiagnosticEvidence와 장치별 반영 완료 관측을 연결한다.
CALL Health_Evaluate(진단/장치 관측, 현재 평가의 VssReadBasis)
     // 미평가를 INITIALIZATION_FAILURE로 만들지 않는 [60 health-evaluate].
if 현재 permission이 있다:
    continue [60 recovery-flow의 안전 연결]; 가짜 Session/Attempt 없음.
STEP INPUT/STORE/PLAYBACK/STREAM/HEALTH의 반영 완료 관측을 재수집한다.
if 관련 stamp/인과 관계가 불충분하거나 수집 뒤 변경됨:
    STEP 새 정상 보고 생성 없이 재수집 필요성을 보존한다.
    return VSS_FLOW_RECOLLECT_OBSERVATIONS
CALL Health_BuildStatus(반영 완료 관측, VssReadBasis)
if VSS_STATUS_CREATED:
    STEP 보호된 VssStatusSnapshot을 FLOW→INPUT 기존 보고 연결→Node Communication에 전달한다.
    STEP 복사 완료 또는 실제 송신 consumer 참조 종료까지 보호한다.
else:
    return VSS_FLOW_FOLLOW_UP_REQUIRED
if 준비/정리/진단 적용/보고 전달이 남음:
    return VSS_FLOW_FOLLOW_UP_REQUIRED
return VSS_FLOW_SETTLED // 재생 성공이나 모든 장치 READY라는 뜻이 아님.
```

초기 요청이 아직 수용되지 않은 환경에서도 위 호출은 미평가 관측을 반환할 수 있다. R5는 기존 R2 `AudioStream_Advance`의 Session 없는 진행 허용을 코드에 연결하며, 초기화 순서를 invent하지 않는다. `[TBD-INIT]` 미결 상태에서는 READY 생성이나 실제 준비 완료를 주장할 수 없다.

<a id="audiocontrol-service"></a>
## AudioControl_Service — 장치별 구성·원 operation 적용

STREAM의 Prepare/RequestControl/Advance만 호출한다. 아래 `intent`, `ready`, `result`, `reg`, `raw`는 각각 기존 R3 타입이다. 여러 장치 관측은 독립 결과이며 한 combined ready flag로 축약하지 않는다.

```text
CORE AudioControl_Service
INPUT optional VssDeviceControlIntent intent; VssTimeEvidence now
OUTPUT VssDeviceReadinessState ready; optional VssDeviceControlResult result;
       protected VssDiagnosticEvidence; optional recovery performed/verification
OWNER AUDIO CONTROL

if intent exists:
    // VSS_DEVICE_CODEC / VSS_DEVICE_CLOCK_GENERATOR / VSS_DEVICE_CLOCK_REFERENCE
    if 대상 device/configuration이 확인되지 않거나 새 operation 보호 공간 없음:
        STEP 기존 ready/원 control을 보존하고 원대상 거부·진단을 반환한다.
        return // 기존 제어의 보호를 취소하지 않음.
    if 같은 intent.operation의 진행/완료가 이미 기록됨:
        STEP 기존 원결과를 다시 제공한다; HW action을 다시 수행하지 않는다.
    else if intent.action == VSS_DEVICE_INVALIDATE:
        STEP 영향받는 현재 ready.configuration을 확인한다.
        if 동일 현재 구성에 대한 무효화:
            ready.state = VSS_DEVICE_INVALIDATED
            STEP 해당 장치 revision을 반영 완료 뒤 변경한다.
        STEP 옛 구성 요구는 옛 제어/진단에만 연결한다.
    else:
        if intent.action == VSS_DEVICE_RECOVER:
            if intent.recoveryPermission == null 또는 현재 대상/구성/허용 불일치:
                STEP 새 동작 거부, 기존 제한·원결과 보호; return
            STEP [60 recovery-flow]의 사전 안전화/현재 허용 근거를 재확인한다.
            if 근거 부족: STEP 수행 보류·진단; return
        else if intent.action != VSS_DEVICE_PREPARE:
            STEP 부적합 의도 거부; return
        if PREPARE이며 같은 현재 구성 ready.state == VSS_DEVICE_READY:
            STEP 기존 확인 근거를 반환; 매 Attempt 재초기화하지 않는다.
        else:
            STEP intent의 operation/device/configuration/action/permission을 불변 보호한다.
            STEP HAL의 기존 내부 등록 경계에서만 아래 기록을 생성한다 (신규 Core/API 아님):
                VssHalDeviceRegistration reg
                reg.operation = intent.operation
                reg.device = intent.device
                reg.configuration = intent.configuration
                STEP HAL이 reg.registration과 당시 reg.binding을 보호한다.
            STEP CONTROL은 보호된 HAL 기록의 원귀속 확인만 받는다.
            STEP 보호 완료 뒤에만 기존 HAL 장치 요구를 연결한다. [TBD-INIT]
            STEP 승인된 구성 변경이면 영향받는 옛 readiness를 무효화하고 옛 등록/결과는 보호한다.
            ready.device = intent.device; ready.configuration = intent.configuration
            ready.state = VSS_DEVICE_PREPARING // 현재 해당 구성에만 적용.
            STEP vendor return과 실제 부분 효력을 별도로 기록한다.
            if 실패/부분 효력/timeout:
                STEP 원대상·구성·발견 단계 진단을 보호한다; READY 생성 없음.

for 이번 budget 안의 보호된 VssRawObservation raw:
    STEP 원 VssHalDeviceRegistration과 operation/device/configuration/binding을 확인한다.
    if 대응 근거 부족/손실:
        STEP INSUFFICIENT/LOST를 보존; 새 현재 구성 READY 생성 없이 후속 필요성 반환.
        continue
    VssDeviceControlResult result // 원 operation/device/configuration에 귀속.
    STEP 실제 제어 완료/실패 범위만 result.state에 기록한다.
    if result.configuration != ready.configuration 또는 result.device != ready.device:
        STEP 옛 결과·정리·진단으로 격리; 현재 ready를 바꾸지 않는다.
        continue
    if 현재 구성 준비의 충분한 완료 근거:
        ready.state = VSS_DEVICE_READY
    else if 현재 구성의 확인된 실패:
        ready.state = VSS_DEVICE_FAILED
    else:
        STEP 기존 준비/미평가 상태 유지; timeout만으로 성공 생성 없음.
    STEP 반영 완료 뒤 ready.revision 갱신; 원결과 consumer 적용까지 보호.
    if 원 action == VSS_DEVICE_RECOVER:
        STEP 실제 수행 결과와 별도 효과 관측을 [60 device-recovery]에 연결한다.

STEP reg는 HW·대기·처리 중 참조와 late 대응이 모두 끝난 경우만 HAL에서 폐기한다.
return 독립 장치 관측/원결과/진단 // mute≠NO_START, ready≠actual.
```

## 실패·후속·종료와 미정

| 조건 | 계속 보존하는 것 | 이번 기회의 끝 |
| --- | --- | --- |
| 초기 정보·제어 진입 미평가 | UNASSESSED 및 미결 `[TBD-INIT]` | 준비/보고 재평가; Fault/READY 합성 없음 |
| 확인된 원 초기화 실패 | producer/device/config/occurredAt 진단 | HEALTH 기존 Fault 분류, STREAM 후속 정리 |
| 늦은 옛 구성 성공 | 옛 operation/registration 기록 | 옛 정리·진단; 새 ready/Fault clear 금지 |
| 참조·보호 예산 부족 | 기존 수용 제어와 미반영 사실 | 새 요구 보류/거부, 무제한 pool 없음 |

`[TBD-INIT]`은 실제 최초 장치 준비 entry·순서/기존 BSP 초기화와 CONTROL 관측 연결이다. 명시적 장치 action 범위, ready 판정의 물리 근거·지연/timeout·복사/등록 저장 수단은 [기존 TBD](../90_BINDING/91_IMPLEMENTATION_TBD.md). 미정이면 동작을 추정하거나 후속 gate를 통과시키지 않는다.

## 원문 추적

[FLOW R2](../30_FUNCTIONS/10_FLOW_FUNCTIONS.md#flow-process) · [CONTROL R2](../30_FUNCTIONS/50_DRIVER_HAL_FUNCTIONS.md#audiocontrol-service) · [STREAM 진행 R2](../30_FUNCTIONS/40_AUDIO_STREAM_ASSET_FUNCTIONS.md#audiostream-advance) · [장치 Intent/Readiness R3](../40_DATA/60_AUDIO_TX_DATA.md#vssdevicecontrolintent) · [시간 R3](../40_DATA/10_INPUT_DATA.md#vsstimeevidence) · [보고 R3](../40_DATA/70_DIAGNOSTIC_DATA.md#vssstatussnapshot) · [원본/보호 R4](../50_CONTRACTS/10_OWNERSHIP_LIFETIME.md#borrow-and-protect) · [복구 R4](../50_CONTRACTS/40_FAULT_RECOVERY.md#recovery-flow) · [CONTROL local](../20_MODULES/31_AUDIO_CONTROL_MODULE.md)
