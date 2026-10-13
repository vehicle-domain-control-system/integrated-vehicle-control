# Health_BuildStatus — 한국어 C형 의사코드

[VSS 지도](../../../01_VSS_MAP.md) · [SERVICE](../../00_LAYER_OVERVIEW.md) · [HEALTH](../00_MODULE_OVERVIEW.md) · CORE-18 · [상세 설명](../functions/Health_BuildStatus.md)

> GUIDE · D6-1 SELF_CHECKED / INDEPENDENT_REVIEW_PENDING. R6 논리 계약 / 실제 C ABI BINDING_TBD / HW_UNVERIFIED / TEST_NOT_RUN.

<a id="contract"></a>
## R6 논리 원형

```text
VssStatusBuildResult Health_BuildStatus(
    IN const VssInputContext inputs[count],
    IN const VssCandidateObservation candidates[count],
    IN const VssPlaybackObservation *playback,
    IN const VssAssetDescriptor assets[count],
    IN const VssDeviceReadinessState devices[count],
    IN const VssBackendCleanupObservation cleanup[count],
    OPTIONAL IN const SemanticBool *inputAcceptingBasis,
    OPTIONAL IN const SemanticBool *storeTrackingBasis,
    IN const VssReadBasis *basis,
    OUT VssStatusSnapshot snapshot[capacity -> written]);
```

[R6](../../../../design/70_C_INTERFACE/00_HEADER_OWNERSHIP_MAP.md#r6-health-buildstatus)의 모든 인자를 아래 C형 본문에도 유지한다. 배열 count·OUT capacity/written은 군별 논리 범위로 실제 C 파라미터/필드가 아니다. 한글 조건·대괄호 문장은 **의사 연산**이며 새 helper·enum·DTO·manager/Queue/Task를 선언하지 않는다. 계산용 지역 Snapshot은 기존 R3 타입의 설명이며 보호/게시 ABI는 TBD다.

<a id="ownership"></a>
## Caller와 읽기 전용 범위

FLOW가 보고 전용 새 basis·반영 완료 관측을 연결한다. 필요한 Runtime 읽기 외 HW/Network 직접 호출은 없다. HEALTH 현재/최근 Fault는 기존 내부 읽기 경계에서 함께 읽으며 public IN을 추가하지 않는다. 아래 코드는 Health_Evaluate를 호출하거나 Fault/permission·INPUT 품질·STORE 이력·PB Session·PCM/device 원본을 쓰지 않는다.

<a id="procedure"></a>
## S1~S7 — 일관성·두 OPTIONAL·전체 보호 뒤 한 건 공개

```c
VssStatusBuildResult Health_BuildStatus(
    /* IN */ const VssInputContext inputs[count],
    /* IN */ const VssCandidateObservation candidates[count],
    /* IN */ const VssPlaybackObservation *playback,
    /* IN */ const VssAssetDescriptor assets[count],
    /* IN */ const VssDeviceReadinessState devices[count],
    /* IN */ const VssBackendCleanupObservation cleanup[count],
    /* OPTIONAL IN */ const SemanticBool *inputAcceptingBasis,
    /* OPTIONAL IN */ const SemanticBool *storeTrackingBasis,
    /* IN */ const VssReadBasis *basis,
    /* OUT */ VssStatusSnapshot snapshot[capacity -> written])
{
    // S1: OUT metadata·메모리와 아직 소비 중인 이전 보고의 비간섭 선검증.
    if (!snapshot_주소_metadata_written저장소_범위_산술_비간섭_안전검증) {
        [written/capacity와 배열 무접근; 이전 보고/원본 보존];
        return VSS_STATUS_REEVALUATE; // 무효 written에 0 기록 주장 없음.
    }
    snapshot.written = 0;
    if (snapshot.capacity < 1 || !보고_불변저장과_모든참조_보호가능) {
        return VSS_STATUS_REEVALUATE; // 유효 written0, 이전 caller 자료 유지.
    }
    // S2: 배열마다 count와 양수 범위·Meta/참조를 읽기 전에 검증한다.
    if (!각_IN군_및_내부참조_안전검증 || playback == NULL ||
        !playback_공간_반영완료관측_보호유효 || basis == NULL ||
        !basis_공간_stamps_count_time_참조_안전검증) {
        return VSS_STATUS_REEVALUATE;
    }
    // count0: 해당 관측 없음. 확인된 공집합과 필수 관측 누락을 구분.
    // devices0/후보0/cleanup0으로 전체 READY/안전/정상 보고 생성 없음.
    [HEALTH 내부 current/recent 제한을 동일 보고 평가 기준으로 읽기전용 연결];
    if (basis->validity != VSS_READ_VALID ||
        필요한_owner의_미반영_인과_시간근거_부족_또는_수집뒤_관련원본변경 ||
        Evaluate변경전_또는_선택용_오래된basis를_보고에_재사용) {
        [원본 갱신/getter 숨은 적용 없이 재수집 필요성만 반환];
        return VSS_STATUS_REEVALUATE;
    }
    for (이번_보고에_필요한_각_owner_stamp를_기존bounded_읽기범위에서) {
        [owner/revision/appliedThrough와 필요한 사실·원 시간 인과 대조];
        if (!필수_반영완료_시간domain_epoch_비교와_자료수명_충분) {
            return VSS_STATUS_REEVALUATE;
        }
        // 서로 다른 owner revision 숫자를 같게 맞추지 않는다.
    }

    // S3: 두 OPTIONAL을 각각 검사. NULL을 false/true로 채우지 않는다.
    if (inputAcceptingBasis == NULL) {
        [INPUT 준비 미확인 그대로; 필요 accepting 파생 근거 부족];
    } else if (!이_포인터의_안전읽기_owner출처_scope_수명_유효) {
        return VSS_STATUS_REEVALUATE;
    } else if (*inputAcceptingBasis == false) {
        [확인된 INPUT 경로 제한으로 기존 reduction에 연결];
    } else {
        [INPUT이 실제 확인한 이번 true 조건만 소비; 개별 accepted 보장 없음];
    }
    if (storeTrackingBasis == NULL) {
        [STORE 추적/replay 준비 미확인 그대로];
    } else if (!이_포인터의_안전읽기_owner출처_scope_수명_유효) {
        return VSS_STATUS_REEVALUATE;
    } else if (*storeTrackingBasis == false) {
        [확인된 STORE 추적 제한으로 기존 reduction에 연결];
    } else {
        [STORE가 실제 확인한 이번 true 조건만 소비];
    }
    // fake true·새 manager·임의 AND/전체 truth table 없음.
    if (accepting에_필요한_근거NULL_또는_부족_또는_기존조합정책_미정) {
        return VSS_STATUS_REEVALUATE;
    }

    // S4: 외부 OUT를 아직 쓰지 않는다. 기존 R3 지역값은 파생 계산 설명.
    VssStatusSnapshot derived;
    if (반영완료_원QUARANTINED_불명확최종 ||
        확인된_공통정상출력불가_새start차단_제한) {
        derived.state = VSS_REPORT_FAULT;
        [derived.availability에 기존 UNAVAILABLE 의미 연결; 새 enum 없음];
        [불명확 최종 PLAYBACK_STATE_FAILURE + FAULT + UNAVAILABLE 보존];
    } else if (legal_actual확인된_전체Session이_아직_retirement전) {
        derived.state = VSS_REPORT_PLAYING;
        // cue 무음/반복 대기/시작 후 STOPPING·ABORTING도 전체 Session 진행.
    } else if (초기준비_UNASSESSED_또는_진행이며_startup보고_근거충분) {
        derived.state = VSS_REPORT_STARTUP;
        // INITIALIZATION_FAILURE·초기 기본 availability 합성 없음.
    } else if (정상초기화_준비와_기존READY_reduction_근거충분) {
        derived.state = VSS_REPORT_READY;
    } else {
        return VSS_STATUS_REEVALUATE;
    }
    // START 수락/PREPARED/ACK/ISR/mute/객체 존재만으로 PLAYING 없음.
    // NO_START는 Started 아님; uncertain final을 late cleanup으로 normal 부활 없음.

    // S5: state 이외 독립 의미도 전부 결정할 근거가 있어야 한다.
    [기존 지원서비스·현재 제한 근거로 serviceLevel FULL/DEGRADED 파생];
    [availability는 opaque VssAvailabilityValueType의 기존 의미로만 파생];
    [INPUT/STORE 자원·시간/source/replay 준비의 기존 기준으로 accepting 파생];
    if (필요_availability_serviceLevel_accepting_Fault투영_전체조합이_미정_또는_부족) {
        return VSS_STATUS_REEVALUATE; // 기본 FULL/true·임의 정책표 없음.
    }
    // WINDOW 보류만으로 DEGRADED/FAULT 없음. INPUT STALE/INVALID/NO_DATA≠outputFault.
    // Audio Fault≠accepting false. false도 duplicate/CLEAR/품질/기한/정리 계속.

    // S6: FaultState 포인터가 아닌 const FaultKind 의미값 목록 각각 보호.
    [derived.currentFaults/currentFaultCount를 실제 현재 의미 목록으로 보호];
    [derived.recentFaults/recentFaultCount를 실제 최근 의미 목록으로 독립 보호];
    [derived.basis에 이번 불변 ReadBasis 값과 내부 stamps/time 참조 보호];
    if (!전체목록의_실제count범위_보호가능 ||
        !FLOW_INPUT_NodeCommunication_소비수명까지_모든참조_보호가능) {
        [이전 Snapshot·current/recent 원본 유지; 가짜 부분 전체목록 공개 없음];
        return VSS_STATUS_REEVALUATE;
    }
    // 구조체/포인터 얕은 복사는 보호가 아니다. current 해제도 recent 삭제 아님.

    // S7: 공개 직전 coherence/보호를 다시 검사한다. 새로운 framework 아님.
    if (관련_owner_변경_또는_시간인과부족_또는_OUT비간섭_참조보호_불충족) {
        [이번 계산자료는 새 보고로 공개하지 않고 기존 소비 자료 보존];
        return VSS_STATUS_REEVALUATE;
    }
    [전체 derived를 유효 snapshot[0]에 소비자가 부분값을 보지 않도록 공개];
    if (!전체기록_필요참조보호_및_유효공개_완료) {
        [부분기록 유효 공개 없음; written0·원본/이전 소비자료 보존];
        return VSS_STATUS_REEVALUATE;
    }
    snapshot.written = 1;
    return VSS_STATUS_CREATED; // 생성·보호 한 건. 통신 송신/Domain ACK 아님.
}
```

<a id="postconditions"></a>
## 반환·소비 수명과 확인 한계

유효 OUT에서 CREATED만 written1, 모든 REEVALUATE 경로는 written0이다. 무효 저장소는 무접근이다. 새 Snapshot 미생성은 원 Fault/최근 이력·INPUT/STORE/PB/PCM/device 원본 변화가 아니다. ROOT 자료에 숨은 Evaluate/Recovery를 실행하지 않는다.

Fault kind 목록과 basis 내부 stamps는 불변 보고 소유/독립 보호 자료다. FLOW→INPUT→Node Communication이 복사하면 실제 복사 완료와 참조 종료까지, 참조하면 실제 최종 소비 종료까지 유지한다. 이전 소비 중 Snapshot을 다음 생성/재평가로 덮지 않는다. Node 복사/참조 방식·직렬화·storage/게시 원자성·key/시간 ABI는 TBD다. 실제 송신/Domain 수용 성공이나 무손실을 확정하지 않는다.

F1/F3 후보는 관측 변경·OPTIONAL·legal actual/whole Session·현재/최근 분리·OUT0/1·수명이다. F4 전체/초기 reduction·Q08~10 DTO·source/image/C ABI, F5 물리 actual/STOP/Recovery·DMA/시간/메모리는 미검증이다. P1B_C1 REQUIRED_NOT_RUN/P2 HOLD·OPEN/FAIL/CR 보존, D6-2/GD6 미착수, Host/Firmware/Build/Flash/Board NOT_RUN이다.

[설명 인자·반환](../functions/Health_BuildStatus.md#contract) · [두 OPTIONAL](../functions/Health_BuildStatus.md#optional) · [state 근거](../functions/Health_BuildStatus.md#states) · [Snapshot 수명](../functions/Health_BuildStatus.md#snapshot) · [R2](../../../../design/30_FUNCTIONS/60_HEALTH_RUNTIME_FUNCTIONS.md#health-buildstatus) · [R3](../../../../design/40_DATA/70_DIAGNOSTIC_DATA.md#vssstatussnapshot) · [R4](../../../../design/50_CONTRACTS/40_FAULT_RECOVERY.md) · [R5](../../../../design/60_PSEUDOCODE/60_FAULT_RECOVERY_FLOW.md#health-buildstatus) · [R6](../../../../design/70_C_INTERFACE/00_HEADER_OWNERSHIP_MAP.md#r6-health-buildstatus) · [Fault writer 설명](../functions/Health_Evaluate.md#ownership)
