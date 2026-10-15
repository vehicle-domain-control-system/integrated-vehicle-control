# Health_BuildStatus — 한국어 C형 의사코드

[VSS 지도](../../../01_VSS_MAP.md) · [SERVICE](../../00_LAYER_OVERVIEW.md) · [HEALTH](../00_MODULE_OVERVIEW.md) · CORE-18 · [상세 설명](../functions/Health_BuildStatus.md)

> GUIDE · CORE-18 · R6 논리 계약 / 실제 C ABI BINDING_TBD / HW_UNVERIFIED / TEST_NOT_RUN.

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

[R6](../../../../design/70_C_INTERFACE/00_HEADER_OWNERSHIP_MAP.md#r6-health-buildstatus)의 모든 인자를 아래 C형 본문에도 유지한다. 배열 count·OUT capacity/written은 군별 논리 범위로 실제 C 파라미터/필드가 아니다. 한글 조건·대괄호 문장은 **의사 연산**이며 새 helper·enum·DTO·manager/Queue/Task를 선언하지 않는다. 계산용 지역 Snapshot은 기존 R3 타입의 설명이며 보호/게시 실제 ABI는 E-S01이며 A2 내부 의미는 선택 완료다.

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

    // S3: A2 §2; non-NULL 주소/owner/scope를 모두 먼저 검증.
    if (!두_OPTIONAL의_nonNULL_안전검증) return VSS_STATUS_REEVALUATE;
    if (확인된_false가_하나이상) {
        [accepting=false; 다른 NULL의 값은 unknown 그대로];
    } else if (두_근거가_확인된_true) {
        [accepting=true; 개별 Event 수용/재생 보장은 아님];
    } else {
        return VSS_STATUS_REEVALUATE;
    }

    // S4: 외부 OUT를 아직 쓰지 않는다. 기존 R3 지역값은 파생 계산 설명.
    VssStatusSnapshot derived;
    if (반영완료_원QUARANTINED_불명확최종 ||
        확인된_공통정상출력불가_새start차단_제한) {
        derived.state = VSS_REPORT_FAULT;
        [derived.availability에 기존 UNAVAILABLE 의미 연결; 새 enum 없음];
        [불명확 최종 PLAYBACK_STATE_FAILURE + FAULT + UNAVAILABLE 보존];
    } else if (정상초기화완료 && legal_actual확인된_전체Session이_아직_retirement전) {
        derived.state = VSS_REPORT_PLAYING;
        // cue 무음/반복 대기/시작 후 STOPPING·ABORTING도 전체 Session 진행.
    } else if (초기화진행_출력준비전이_확인되고_startup보고_근거충분) {
        derived.state = VSS_REPORT_STARTUP;
        // INITIALIZATION_FAILURE·초기 기본 availability 합성 없음.
    } else if (정상초기화_준비와_A2_READY_가용서비스_근거충분) {
        derived.state = VSS_REPORT_READY;
    } else {
        return VSS_STATUS_REEVALUATE;
    }
    // START 수락/PREPARED/ACK/ISR/mute/객체 존재만으로 PLAYING 없음.
    // NO_START는 Started 아님; uncertain final을 late cleanup으로 normal 부활 없음.

    // S5: A2 §2 우선순위와 독립 가용성/지원 수준.
    [공통실패/QUARANTINED는 FAULT/UNAVAILABLE/DEGRADED];
    [확인된 초기화진행은 STARTUP/UNAVAILABLE, 지원집합 근거로 FULL/DEGRADED];
    [초기화완료 후 전체/부분서비스 준비를 FULL/DEGRADED로 대응];
    [derived.accepting은 S3의 확인된 결과];
    if (!모든_서비스_current_history_필수근거_충분_일관) {
        return VSS_STATUS_REEVALUATE;
    }
    // WINDOW 보류/입력 INVALID≠출력Fault. Audio Fault≠accepting false.

    // S6: FaultState 포인터가 아닌 const FaultKind 의미값 목록 각각 보호.
    [누락/손실/미반영 없는 전체 current instance에서 kind 집합 최대5개 보호];
    [derived.currentFaults/currentFaultCount에 그 완전 집합만 연결];
    [derived.recentFaults/recentFaultCount는 최대5종 이력 집합으로 독립 보호];
    [현재/최근 원기록의 firstOccurredAt 순서로 최신 선택, 정확한 동시는 reportOccurrence];
    if (!최신선택_또는_확인된기록부재_원시간_세대_충분) return VSS_STATUS_REEVALUATE;
    [lastFaultPresent/Kind/Occurrence/At를 같은 원기록에서 값복사; 없음은 의미 무효];
    [reportGeneration과 이번 evaluatedAt을 보호; 다음 reportSequence는 아직 후보];
    [derived.basis에 이번 불변 ReadBasis 값과 내부 stamps/time 참조 보호];
    if (!전체목록의_실제count범위_보호가능 ||
        !FLOW_INPUT_NodeCommunication_소비수명까지_모든참조_보호가능) {
        [이전 Snapshot·current/recent 원본 유지; 가짜 부분 전체목록 공개 없음];
        return VSS_STATUS_REEVALUATE;
    }
    // 구조체/포인터 얕은 복사는 보호가 아니다. current 해제도 recent 삭제 아님.

    // S7: 공개 직전 coherence/보호를 다시 검사한다. 새로운 framework 아님.
    if (!원_VSS_generation_평가시각_유효 || 다음_reportSequence가_u32범위밖)
        return VSS_STATUS_REEVALUATE; // 같은 세대 wrap 재사용 없음.
    if (관련_owner_변경_또는_시간인과부족_또는_OUT비간섭_참조보호_불충족) {
        [이번 계산자료는 새 보고로 공개하지 않고 기존 소비 자료 보존];
        return VSS_STATUS_REEVALUATE;
    }
    [전체 derived를 유효 snapshot[0]에 소비자가 부분값을 보지 않도록 공개];
    if (!전체기록_필요참조보호_및_유효공개_완료) {
        [부분기록 유효 공개 없음; written0·원본/이전 소비자료 보존];
        return VSS_STATUS_REEVALUATE;
    }
    [같은 공개 직렬경계에서 reportSequence 확정; 실패/재인계는 증가 없음];
    snapshot.written = 1;
    return VSS_STATUS_CREATED; // 생성·보호 한 건. 통신 송신/Domain ACK 아님.
}
```

<a id="postconditions"></a>
## 반환·소비 수명과 확인 한계

유효 OUT에서 CREATED만 written1, 모든 REEVALUATE 경로는 written0이다. 무효 저장소는 무접근이다. 새 Snapshot 미생성은 원 Fault/최근 이력·INPUT/STORE/PB/PCM/device 원본 변화가 아니다. 원본 자료에 숨은 Evaluate/Recovery를 실행하지 않는다.

Fault kind 목록과 basis 내부 stamps는 불변 보호한다. [A2 인계](../../../../design/90_BINDING/93_NETWORK_STATUS_BINDING.md#handoff)의 HEALTH 2슬롯 → INPUT 단일 동기 변환 → Network 전체 복사 반환까지 참조를 유지한다. 이전 소비 중 Snapshot을 덮지 않는다. 실제 ABI/직렬화/게시/고정 용량 입증은 E-S01이며 counter/원 평가시각은 REEVALUATE에서 불변이다.

A2 내부 reduction·원기록·DTO 변환·bounded handoff는 설계 선택 완료다. ST01~ST12 문서 분기를 검수하며 실제 clock/Boot/ABI/sink/DOMAIN 설치 source는 E-S01~05다. P2/물리 Gate·기존 CR은 유지하며 Host/Firmware/Build/Flash/Board NOT_RUN이다.

[설명 인자·반환](../functions/Health_BuildStatus.md#contract) · [두 OPTIONAL](../functions/Health_BuildStatus.md#optional) · [state 근거](../functions/Health_BuildStatus.md#states) · [Snapshot 수명](../functions/Health_BuildStatus.md#snapshot) · [R2](../../../../design/30_FUNCTIONS/60_HEALTH_RUNTIME_FUNCTIONS.md#health-buildstatus) · [R3](../../../../design/40_DATA/70_DIAGNOSTIC_DATA.md#vssstatussnapshot) · [R4](../../../../design/50_CONTRACTS/40_FAULT_RECOVERY.md) · [R5](../../../../design/60_PSEUDOCODE/60_FAULT_RECOVERY_FLOW.md#health-buildstatus) · [R6](../../../../design/70_C_INTERFACE/00_HEADER_OWNERSHIP_MAP.md#r6-health-buildstatus) · [Fault writer 설명](../functions/Health_Evaluate.md#ownership)
