# Fault / Recovery — 현재 허용, 안전 연결, 수행과 별도 검증

> R5 · R4 독립 PASS 기준 · Fault 정확히 기존 5종. HEALTH는 HW를 수행하지 않는다.

[표기 규칙](00_PSEUDOCODE_OVERVIEW.md#notation) · [Startup CONTROL](10_STARTUP_FLOW.md#audiocontrol-service) · [전체 STOP/retirement](50_STOP_LATE_UNCERTAIN_FLOW.md#playback-stop-late)

## 목적·진입·전제·타입

FLOW가 원진단·수행·검증을 HEALTH에 전달하고, 현재 permission만 안전화 후 STREAM에 연결한다. Session 없는 장치 준비/복구도 가능하다. owner 원본을 합친 recovery manager나 새 Fault/action enum을 만들지 않는다.

| owner / Core | 입력 → 출력 | 실제 쓰기 |
| --- | --- | --- |
| HEALTH / `Health_Evaluate` | DiagnosticEvidence, PerformedEvidence, VerificationEvidence, ReadBasis → FaultState/RecoveryPermission/HealthChange | 현재/최근 Fault·제한·허용 |
| FLOW / `Flow_Process` | 현재 허용·PB/STREAM 안전 관측 → 후속 조율 | 조율 상태만 |
| STREAM / `AudioStream_RequestControl` | RecoveryPermission typed lane → 복구 진행/진단 | 원 허용/수행 연결 |
| CONTROL / `AudioControl_Service` | DeviceControlIntent → 장치 결과, performed/verification | 원 device/config/제어 |
| HEALTH / `Health_BuildStatus` | owner가 반영한 관측·ReadBasis → StatusBuildResult/StatusSnapshot | 파생 보고만 |

다른 HW 대상은 기존 Backend/Driver 경계를 따른다. 특정 raw cause에 임의 reset action을 연결하지 않는다. exact cause→Fault/action/효과 충분성 및 availability/reduction 정책은 TBD다.

<a id="health-evaluate"></a>
## Health_Evaluate — 원진단과 현재 대상 제한

```text
CORE Health_Evaluate
INPUT protected VssDiagnosticEvidence diagnostic;
      optional VssRecoveryPerformedEvidence performed;
      optional VssRecoveryVerificationEvidence verified; VssReadBasis basis;
      owner 반영 완료 관측
OUTPUT VssHealthChange; 원 VssFaultState/현재 VssRecoveryPermission 관측
OWNER HEALTH; own VssFaultState fault; own VssRecoveryPermission permission

for budget 안의 원 diagnostic:
    STEP producer/stage/cause/target/configuration/operation/occurredAt을 보존한다.
    if 입력 INVALID/STALE/미수신/미평가 또는 WINDOW [구현 보류 — 설계 유지] 자체:
        STEP 입력/미평가 진단만 적용; 새 출력 Fault/DEGRADED 생성 없음; continue
    if 확인된 출력/Asset/초기화/상태 실패:
        STEP 승인된 기존 분류 근거가 있는 범위만 아래 Fault에 연결한다:
             SOUND_ASSET_UNAVAILABLE / PLAYBACK_START_FAILURE / AUDIO_OUTPUT_FAILURE /
             PLAYBACK_STATE_FAILURE / INITIALIZATION_FAILURE
        if exact cause 매핑 미정: STEP 원진단·필요 제한 판단 미결 보존; 임의 reset/Fault 없음.
        else:
            STEP fault.instance/kind/target/configuration/diagnosis/restriction을 원근거로 보호.
            fault.recordState = VSS_FAULT_CURRENT
            STEP 현재 instance와 revision을 반영 완료한다; 최근 기록과 다른 current 보존.
if 반영 완료된 원 PLAYBACK의 START_OUTCOME_UNCERTAIN 최종 근거:
    STEP 기존 PLAYBACK_STATE_FAILURE + FAULT + UNAVAILABLE 제한을 반영한다.
    STEP 다른 진단 수신 유무와 별개로 원 불명확 최종의 제한을 유지한다.

if basis.validity != VSS_READ_VALID 또는 현재 target/config/time 근거 부족:
    STEP 현재 제한 보존·해제/새 수행 허용 보류; return VSS_HEALTH_RECONFIRM_REQUIRED
if 현재 Fault가 기존 정책상 recoverable이고 현재 허용 조건 충족:
    STEP permission.permission/faultInstance/faultRevision/target/configuration/action을
         현재 fault와 승인된 제한 범위로 생성·불변 보호한다. Session/Attempt 없음.
    STEP permission은 실제 수행/검증/해제가 아닌 FLOW에 전달할 허용이다.
if 새 instance/revision/config/action/철회로 기존 permission 무효:
    STEP old permission 격리·수행 불가; old 결과는 old 진단에만 연결한다.

if performed 또는 verified 중 하나만 존재:
    STEP 원 사실을 독립 보호하고 짝 근거 후속을 기다린다; 제한 해제 없음.
else if performed exists && verified exists:
    STEP 각각 원 permission/대상/구성/실제 수행 operation을 대조한다.
    if 현재 permission.permission != performed.permission 또는 verified.permission,
       fault.instance/revision != permission.faultInstance/faultRevision,
       target/configuration 불일치 또는 verified.performedOperation != performed.operation:
        STEP old/다른 대상 결과 진단·보호; 현재 제한 해제 없음.
    else if performed.effect == VSS_RECOVERY_SUCCEEDED &&
            verified.effect == VSS_RECOVERY_SUCCEEDED &&
            verified.verifiedAt이 해당 performed 이후의 별도 효과 관측이며 충분:
        STEP 이 현재 Fault/대상의 해당 제한만 해제하고 기존 정책에 따라 recent로 보존.
        STEP 관련 permission 종료/변경 revision 반영 완료; 최신 후보 재평가 필요성 반환.
    else:
        STEP 실패/UNCONFIRMED/관측 부족·손실과 현재 제한/원결과 보호를 유지한다.
        // 요청 수용·performed 성공·현재 ready·무음만으로 clear 없음.
return 변경이면 VSS_HEALTH_CHANGED, 미결이면 VSS_HEALTH_RECONFIRM_REQUIRED,
       그 외 VSS_HEALTH_UNCHANGED
```

수행/검증 중 하나만 있는 호출은 해당 사실을 독립 보호하고 pair가 완성되기 전 해제 분기에 들어가지 않는다. Fault current revision과 permission 발행/무효화의 실제 직렬화는 R6 TBD다. 같은 Fault kind가 현재 instance 일치의 대체 근거가 아니다.

<a id="recovery-flow"></a>
## Flow_Process — 현재 permission과 old 안전화 연결

```text
CORE Flow_Process / recovery slice
INPUT 현재 VssRecoveryPermission permission;
      반영 완료 PB/Backend/장치 안전 관측과 VssReadBasis basis
OUTPUT 조율 후속 필요성
OWNER FLOW

STEP HEALTH 현재 faultInstance/faultRevision/target/config/action 허용을 읽는다.
if basis/현재 허용 불충분 또는 바뀜: STEP 재수집; return 후속 필요성
if old 전체 출력 owner가 아직 retirement 전:
    CALL Playback_RequestTransition(원 Session의 전체 종료 의도, 최신 조건)
    STEP Playback_Advance/AudioStream_Advance 후속을 계속한다; return 후속 필요성
if 실제 old provider/read/PCM/device의 위험한 접근·출력 가능성이 남음:
    STEP 해당 owner의 기존 cleanup 진행; return 후속 필요성
STEP 독립 보호된 old HAL 불변 기록이 남아도 alias/위험 참조 없는지 확인한다.
STEP 수행 직전 current permission/대상/configuration과 안전 근거를 재확인한다.
CALL AudioStream_RequestControl(permission, 현재 허용/안전 관측)
     // Recovery typed lane; 가짜 재생 command 생성 없음.
STEP 다음 AudioStream_Advance가 원 performed/별도 verified/diagnostic을 반환하게 연결한다.
CALL Health_Evaluate(각 원 사실, 현재 적용 근거)
if HEALTH가 현재 해당 제한을 해제함:
    STEP STORE 원본 age/Hold/replay 보존한 최신 관측을 다시 수집한다.
    CALL Select_Choose(현재 후보/제한/owner, 새로운 유효 ReadBasis)
    CALL Playback_RequestTransition(판단, 최신 조건)
return 남은 처리에 맞는 FlowProgress // recovery 성공을 PB 직접 start로 바꾸지 않음.
```

<a id="stream-recovery"></a>
## AudioStream_RequestControl — Session 없는 Recovery lane

```text
CORE AudioStream_RequestControl / RecoveryPermission slice
INPUT VssRecoveryPermission permission; 현재 허용/old 안전 관측
OUTPUT 원 permission 진행/진단 및 후속 typed recovery 사실; AudioRequestResult를 강제하지 않음.
OWNER STREAM

STEP permission의 6개 필드를 불변 보호한다; PB Session/Attempt와 별도 identity.
if 현재 instance/revision/target/config/action 불일치 또는 철회됨:
    STEP 새 수행 거부·원진단 반환; 기존 old cleanup/보호 유지; return
if old retirement/실제 provider·PCM·관련 device 안전 조건 또는 보호 공간 부족:
    STEP 수행 보류·원정리 후속; return
if 동일 permission의 원 수행이 이미 제출/진행/완료됨:
    STEP 기존 operation과 별도 효과 검증을 진행; 재수행 발행 없음; return
STEP 현재 허용·안전 조건을 부수효력 직전에 다시 확인한다.
if 기존 허용 action의 실물 target→기존 Driver 매핑/효과 기준이 미정:
    STEP [TBD-RECOVERY] 보존·진단; 미수행/미확인 반환; return
if 기존 장치 제어 대상:
    VssDeviceControlIntent intent
    STEP intent.operation을 원 허용의 개별 실제 수행 key로 보호한다.
    STEP intent.device/configuration/action=VSS_DEVICE_RECOVER를 허용된 현재 대상에 연결한다.
    intent.recoveryPermission = permission의 보호 참조
    CALL AudioControl_Service(intent, now)
else:
    STEP 허용 범위의 기존 Backend/Driver 경계만 연결한다. 신규 API/reset matrix 없음.
STEP 원 operation 수행 결과와 별도 effect verification을 AudioStream_Advance로 반환한다.
return 원 후속 필요성 // STREAM이 HEALTH Fault clear를 쓰지 않는다.
```

<a id="device-recovery"></a>
## AudioControl_Service / Backend — 실제 수행 뒤 별도 효과 관측

```text
CORE AudioControl_Service / recovery continuation (장치 기본 실행은 [10])
INPUT 보호된 VssDeviceControlIntent intent; VssRecoveryPermission permission; 실제 원 수행/효과 관측
OUTPUT VssRecoveryPerformedEvidence performed; VssRecoveryVerificationEvidence verified;
       원 장치 결과/diagnostic
OWNER 해당 실제 Backend/DRIVER

if 원 실제 action 완료/실패가 확인됨:
    performed.permission = 원 permission.permission
    performed.target = 원 permission.target; performed.configuration = 원 permission.configuration
    performed.operation = 원 실제 intent.operation
    STEP effect는 실제 SUCCEEDED/FAILED 또는 UNCONFIRMED, occurredAt은 원 수행 시각으로 보호.
    STEP performed를 consumer 적용/보호 추적까지 보존한다; receipt만으로 생성 없음.
if 수행 뒤 별도 효과 관측의 기존 충분조건 확인됨:
    verified.permission = performed.permission; verified.performedOperation = performed.operation
    verified.target = performed.target; verified.configuration = performed.configuration
    STEP verified.effect/verifiedAt을 별도 실제 관측으로 보호한다.
else:
    STEP 미관측/실패/부족·손실을 UNCONFIRMED 또는 실패 근거로 보호; 성공 합성 없음.
STEP 옛 config/permission/operation이면 old 결과·진단으로 반환; 새 ready/Fault 변경 없음.
return 각 원 사실 // 수행 owner는 HEALTH 제한을 직접 해제하지 않는다.
```

<a id="health-buildstatus"></a>
## Health_BuildStatus — 반영 완료 관측의 파생 보고

```text
CORE Health_BuildStatus
INPUT owner별 반영 완료 관측; VssPlaybackObservation pb; VssReadBasis basis;
      HEALTH 현재 제한/최근 진단; 입력/저장/시간/출처/자원 관측
OUTPUT VssStatusBuildResult; optional VssStatusSnapshot snapshot
OWNER HEALTH / 보고 파생만; Fault writer 경로와 별개.

if basis.validity != VSS_READ_VALID 또는 필수 관측 부족/관련 원본 변경:
    return VSS_STATUS_REEVALUATE // getter 안에서 사실/기한/Fault 적용 없음.
snapshot.basis = 보호된 이번 평가 근거
if QUARANTINED 또는 확인된 공통 정상 출력 불가/새 start 차단 제한:
    snapshot.state = VSS_REPORT_FAULT
    STEP snapshot.availability는 기존 UNAVAILABLE 의미를 적용한다; 새 enum 값 선언 없음.
else if 적법 actual 확인된 전체 Session이 아직 retirement 전:
    snapshot.state = VSS_REPORT_PLAYING // cue gap/반복 대기/시작 후 stop 진행 포함.
else if 초기 준비 미평가/진행이며 startup 보고 근거 충분:
    snapshot.state = VSS_REPORT_STARTUP
else if 정상 초기화/준비와 기존 READY reduction 근거 충분:
    snapshot.state = VSS_REPORT_READY
else:
    return VSS_STATUS_REEVALUATE // 미정 조합으로 새 정상값 없음.
STEP 지원 서비스 관측에서 serviceLevel=FULL 또는 DEGRADED를 기존 정책으로 파생한다.
STEP WINDOW 보류만으로 DEGRADED/Fault 생성 없음.
STEP accepting은 입력 검증/저장 자원·시간/source/replay 준비의 반영 완료 근거로 파생한다.
if availability/serviceLevel/accepting 또는 Fault reduction 기준이 필요한데 미정:
    return VSS_STATUS_REEVALUATE // 임의 기본 FULL/true 없음.
STEP currentFaults/currentFaultCount와 recentFaults/recentFaultCount를 각각 보호된 투영으로 연결.
STEP snapshot 전체 참조/자료를 실제 보고 consumer 수명으로 보호한다.
return VSS_STATUS_CREATED
```

`availability`는 R3 opaque `VssAvailabilityValueType`을 유지한다. 보고 enum/bit layout은 만들지 않는다. FULL/READY와 accepting=true는 개별 START/STORE accepted 보장이 아니다. accepting=false여도 duplicate·유효 CLEAR·품질/기한·정리는 계속한다. 전송은 HEALTH→FLOW→INPUT 기존 연결→Node Communication이며 HEALTH에서 직접 송신하거나 새 publisher Core를 만들지 않는다.

## 실패·재평가·자원·미정과 추적

미시작 One-shot은 충분한 NO_START/안전 retirement·기존 replay 조건 및 아직 유효한 원본 freshness에서만 재평가한다. Started/uncertain final은 자동 resume하지 않는다. Stateful은 현재 유효/Hold 내·Rear 결합을 다시 판단하고 old retirement 뒤 새 Session의 정책 시작점에서 재생한다. 복구 성공으로 age/Hold/연속성/replay를 리셋하지 않는다.

`[TBD-RECOVERY]`는 exact cause→Fault/action/대상/효과 충분성·retry/timeout·가용성 상세다. 수행/검증·현재 revision 및 key alias/보호 용량·직렬화의 실제 수단, Status reduction/초기 Availability는 [기존 TBD](../90_BINDING/91_IMPLEMENTATION_TBD.md#policy). 미정이면 해당 수행/해제/새 보고를 막고 원제한/진단을 유지한다.

[HEALTH R2 두 Core](../30_FUNCTIONS/60_HEALTH_RUNTIME_FUNCTIONS.md#health-evaluate) · [Status R2](../30_FUNCTIONS/60_HEALTH_RUNTIME_FUNCTIONS.md#health-buildstatus) · [STREAM typed R2](../30_FUNCTIONS/40_AUDIO_STREAM_ASSET_FUNCTIONS.md#audiostream-requestcontrol) · [CONTROL R2](../30_FUNCTIONS/50_DRIVER_HAL_FUNCTIONS.md#audiocontrol-service) · [Fault/Permission R3](../40_DATA/70_DIAGNOSTIC_DATA.md#vssrecoverypermission) · [수행 R3](../40_DATA/70_DIAGNOSTIC_DATA.md#vssrecoveryperformedevidence) · [검증 R3](../40_DATA/70_DIAGNOSTIC_DATA.md#vssrecoveryverificationevidence) · [Status R3](../40_DATA/70_DIAGNOSTIC_DATA.md#vssstatussnapshot) · [Fault R4](../50_CONTRACTS/40_FAULT_RECOVERY.md#current-target) · [소유 R4](../50_CONTRACTS/10_OWNERSHIP_LIFETIME.md#late-attribution) · [HEALTH local](../20_MODULES/26_HEALTH_MODULE.md#recovery)
