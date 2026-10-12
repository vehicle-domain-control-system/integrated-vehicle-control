# Select_Choose — 한국어 C 형태 의사코드

[상세 설명](../functions/Select_Choose.md) · [의사코드](../pseudocode/Select_Choose.md) · [POLICY](../00_MODULE_OVERVIEW.md) · [VSS 지도](../../../01_VSS_MAP.md) · CORE-06

> GUIDE · SELF_CHECKED / D3_2_REVIEW_PENDING · LOGICAL_APPROVED / 실제 C ABI BINDING_TBD · TEST_NOT_RUN / HW_UNVERIFIED.

<a id="contract"></a>
## 목적과 최신 논리 원형

FLOW가 연결한 반영 완료 관측을 읽어 후보·현재 출력·제한을 비교하고, 기존 중앙 정책의 선택 판단과 의미 기반 **전체 plan**을 반환합니다. POLICY는 재생을 실행하지 않습니다.

```text
VssSelectionDecision Select_Choose(
    IN const VssCandidateObservation candidates[count],
    IN const VssPlaybackObservation *playback,
    IN const VssFaultState restrictions[count],
    IN const VssAssetDescriptor assets[count],
    IN const VssDeviceReadinessState devices[count],
    IN const VssBackendCleanupObservation cleanup[count],
    IN const VssReadBasis *basis);
```

[R6 계약](../../../../design/70_C_INTERFACE/00_HEADER_OWNERSHIP_MAP.md#r6-select-choose)과 [인자·결과 표기](../../../../design/70_C_INTERFACE/00_HEADER_OWNERSHIP_MAP.md#prototype-notation)를 그대로 읽습니다. 각 count/capacity/written은 독립 논리 범위입니다. 배열 저장 방식·수치·동기 보호·enum 폭·Header는 실제 ABI로 확정하지 않습니다. 과거 R2 provisional 묶음을 새 public DTO로 선언하지 않습니다.

아래 한글 조건/문장은 기존 owner의 검사·보호·반영을 뜻하는 **설명용 의사 연산**이며 새 함수/API/자료 구조가 아닙니다. `[]`와 각 군의 `.written`/`.capacity`는 R6 범위의 의사 접근이고 실제 구조체 필드 선언이 아닙니다. 반복은 전달된 유효 범위와 기존 처리 예산 안에서 끝나며 완료까지 무한 재시도하지 않습니다. Callee의 한글 OUT 인자는 해당 R6 typed 결과군별 공간을 줄여 쓴 표기입니다.

<a id="procedure"></a>
## S1~S5 — 관측 검증·원 정책 비교·판단 반환

```c
VssSelectionDecision Select_Choose(
    /* IN */ const VssCandidateObservation candidates[],
    /* IN */ const VssPlaybackObservation *playback,
    /* IN */ const VssFaultState restrictions[],
    /* IN */ const VssAssetDescriptor assets[],
    /* IN */ const VssDeviceReadinessState devices[],
    /* IN */ const VssBackendCleanupObservation cleanup[],
    /* IN */ const VssReadBasis *basis)
{
    VssSelectionDecision result;
    result.action = VSS_SELECTION_RECOLLECT;
    result.candidate = NULL;
    result.plan = NULL;
    result.basis = NULL;

    /* S1: 주소/각 count·참조 범위 확인이 값 읽기보다 먼저다. */
    if (basis == NULL || basis_주소와_stamps_시간_참조가_안전하지_않음 ||
        각_IN군의_count_전달영역이나_양수_count의_읽기범위가_부적합 ||
        playback_관측의_읽기저장소가_부적합) {
        return result;
    }
    result.basis = basis; /* 평가 기간의 보호된 borrow만 연결한다. */
    if (basis.validity != VSS_READ_VALID ||
        필요한_owner_stamp_반영완료_인과_원시간_비교가_부족 ||
        수집뒤_관련_revision이_변화) {
        return result; /* RECOLLECT: 숨은 적용/getter write 없음. */
    }

    /* S2: 전달된 유효 후보 수와 기존 예산 안에서만 비교한다. */
    이번_평가의_검토가능한_원후보_없음으로_시작;
    for (이번_예산에서_방문할_각_candidates의_유효_count범위) {
        if (후보.validity != VSS_CANDIDATE_APPLICABLE) {
            continue; /* STALE/CLEAR/DISABLED/EXPIRED를 정상 winner로 만들지 않음. */
        }
        if (후보_key_mode_originalMeta_revision의_원문맥이나_순서_근거_부족) {
            return result; /* arrival 순서로 보충하지 않고 재수집. */
        }
        기존_Class와_중앙_정책의_잠정_세부순위를_읽기전용_비교;
        if (같은_의미의_미시작_One_shot_후보끼리_비교) {
            원발생_순서_비교_후_동률일때만_고정_key_순서를_사용;
        }
        기존_최고_원후보와_그_비교근거를_이번_평가에_연결;
    }
    if (이번_예산에서_판단에_필수인_후보범위를_끝까지_검토하지_못함) {
        return result; /* 일부만 보고 최종 winner를 확정하지 않는다. */
    }

    /* S3: 현재 유효 One-shot의 원 보호 연결은 후보 없음과 별개다. */
    if (현재_정상_One_shot이_원근거로_유효 &&
        낮은_후보나_새_후보없음이나_같은의미_늦은_옛발생만_있음) {
        if (!기존_읽기근거로_현재_후보와_전체_plan을_연결할_수_있음) {
            return result; /* PB private state/새 getter 없이 재수집; STOP 없음. */
        }
        result.action = VSS_SELECTION_KEEP;
        result.candidate = 현재_원후보의_보호된_평가참조;
        result.plan = 기존_정책의_현_전체_plan_보호참조;
        return result;
    }

    /* S4: 의미 plan/Asset과 제한은 제각각의 owner 근거로 판단한다. */
    if (winner_없음 || winner의_의미_asset_image_전체_plan이_정의되지_않음) {
        result.action = VSS_SELECTION_WAIT;
        return result; /* 다른 signal 음원 fallback/임의 cue 수 없음. */
    }
    result.candidate = winner의_보호된_원후보_평가참조;
    result.plan = winner에_연결된_기존_전체_plan_평가참조;
    if (관련_owner_변화나_필수관측_부족이_반환직전_확인됨) {
        result.action = VSS_SELECTION_RECOLLECT;
        result.candidate = NULL;
        result.plan = NULL;
        return result;
    }

    /* S5: action은 실행 의도가 아니라 POLICY의 읽기 판단이다. */
    if (old_phase가_STOPPING_ABORTING_QUARANTINED ||
        현재_Fault_장치구성_자원제한으로_실행불가) {
        result.action = VSS_SELECTION_WAIT;
    } else if (같은_현재_owner의_유효한_전체_plan_계속진행) {
        result.action = VSS_SELECTION_KEEP;
    } else if (old_출력_owner가_있음) {
        result.action = VSS_SELECTION_REPLACE;
        /* 높은 winner 교체 판단 가능. old retirement 전 prepare/start 허가 없음. */
    } else if (old_cleanup_실제_자원안전_근거가_부족) {
        result.action = VSS_SELECTION_WAIT;
    } else {
        result.action = VSS_SELECTION_CHOOSE;
    }
    return result; /* STORE/PB/Fault write·하위 요청 호출 없음. */
}
```

<a id="postconditions"></a>
## 사후조건과 남은 책임

선택은 기존 Class/잠정 순위·원순서·고정 identity의 읽기 결과입니다. WAIT/RECOLLECT는 현재 유효 One-shot의 종료 명령이 아닙니다. 명시적 CLEAR/기한/고장/교체의 적용은 FLOW→PLAYBACK의 최신 조건 재확인에 맡깁니다. 반환 candidate/plan/basis는 평가 borrow이며 PB 채택 후 수명은 별도 보호합니다. 새 plan·수치·PCM·음원 fallback·원본 쓰기가 없습니다.

[입력·반환/Host 후보](../functions/Select_Choose.md#inputs) · [R5 판단](../../../../design/60_PSEUDOCODE/20_INPUT_TO_SELECTION.md#select-choose) · [R4 읽기](../../../../design/50_CONTRACTS/30_TIMING_FRESHNESS.md#coherent-read) · [소유/참조](../../../../design/50_CONTRACTS/10_OWNERSHIP_LIFETIME.md#borrow-and-protect)
