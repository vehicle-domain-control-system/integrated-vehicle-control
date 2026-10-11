# DOMAIN — U2 한국어 C형 의사코드

**U2_DOCUMENT_DESIGN / PROPOSED_DESIGN / G2_INDEPENDENT=PENDING.** 두 CAN 흐름의 아래 활성 함수만 상세화한다. 실제 헤더·API·Wire 채택이나 코드 실행 결과가 아니다.

[↑ Module](00_MODULE_MAP.md) · [Function](../../../03_FUNCTION_INDEX.md) · [Network](../../../00_START_HERE.md)

출처 기준: `docs/network@ad934964618c7f11260ed58dcbd86f881cb99086`. 원문 선언·후보 지위를 그대로 인수한다. `BOUNDARY_PENDING_U4D`는 하위 저장·순서·스케줄 구현의 후속 경계다. 모든 조건과 commit은 문서상 설계 후보이며 시험 상태는 `HOST_EXECUTION=NOT_EXECUTED / BOARD_PHYSICAL=NOT_TESTED`다.

코드 블록의 한국어 조건·단계·객체명은 의미 설명용이며 실제 helper/변수/SDK 심볼 또는 새 ABI가 아니다. 함수의 반환형·이름·전체 인자는 원형 그대로다. 상태·공간·인계 완료 조건이 실제 후속 binding에서 성립한 경우만 정상 return에 도달한다. 상세 조건/오류·commit의 뜻은 같은 FN-ID 설명서를 따른다. 수치 없는 bounded 용량은 무한 공간이나 항상 성공을 뜻하지 않는다.

<a id="fn-domain-domainif_rxcisrear"></a>
## FN-DOMAIN-DomainIf_RxCisRear — `DomainIf_RxCisRear()`

[동작 설명](10_BEHAVIOR.md#fn-domain-domainif_rxcisrear) · [FN 색인](../../../03_FUNCTION_INDEX.md#fn-domain-domainif_rxcisrear) · [원 선언](../../../../interface/02_NODES/DOMAIN/20_API.md#api-domainif_rxcisrear) · [CIS_REAR](../../../flows/10_CIS_REAR_FLOW.md)

**원형:** `DomainIf_Status_t DomainIf_RxCisRear( const CisRearState_t *state);`

**현재 지위:** `DECLARED_SOURCE / MISSING_SOURCE_HEADER / BINDING_OPEN`. Markdown 선언은 실제 target Header의 존재/컴파일을 증명하지 않는다. 원 Source SHA256: `03a4e8dadfd78a41f73ce13af44eccb9a2504a48924d024b91570a48e6d0a96d`.

```c
/* FN-DOMAIN-DomainIf_RxCisRear: 원형을 보존한 문서상 설계 후보. */
DomainIf_Status_t DomainIf_RxCisRear( const CisRearState_t *state)
{
    if (!C2_초기화_완료) { /* D01 */
        return DOMAIN_IF_NOT_INITIALIZED;
    }
    if (state == NULL || typed_표현범위_또는_필드조합_구조오류) { /* D02 */
        return DOMAIN_IF_INVALID_ARGUMENT;
    }
    if (!원_계보와_정확한_C2_인계경계가_준비됨) { /* D03 */
        /* BLOCKED_BY: OPEN-C2-SOURCE / OPEN-UNIT / OPEN-ENUM / OPEN-PROVENANCE */
        /* raw decode/projection은 이 함수 이전 경계이며 raw cast를 수행하지 않는다. */
        return DOMAIN_IF_NOT_AVAILABLE;
    }
    if (지원되지_않는_typed_의미) { /* D06 */
        return DOMAIN_IF_UNSUPPORTED;
    }
    if (수용되지_않은_원세대_또는_업데이트_충돌) { /* D05 */
        return DOMAIN_IF_REJECTED;
    }
    /* D04 단계: NO_OBJECT, 원 열화, STALE/NO_DATA를 원 상태로 보존한다. */
    /* 단계: rearDistanceMm를 그대로 취급하고 정수 cm 절삭이나 위험 CLEAR를 생성하지 않는다. */
    if (필요한_로컬_보관공간이_없음) { /* D07 */
        return DOMAIN_IF_QUEUE_FULL;
    }
    /* D08 단계: 값/품질/메타/sourceGeneration의 완전한 잠정 사본을 준비한다. */
    /* 단계: Core 인계는 전체 commit 또는 변경 없음의 경계로 수행한다. */
    if (!Core_인계_commit이_실제로_완료됨) {
        /* 단계: 잠정 사본을 해제하고 기존 Core 상태를 보존한다. 부분 인계는 성공이 아니다. */
        return DOMAIN_IF_NOT_AVAILABLE;
    }
    /* 단계: 실제 선언과 등록이 확인된 optional observer만 commit 이후 통지한다. */
    /* observer 오류/늦은 응답은 이미 commit된 입력의 재적용 근거가 아니다. */
    return DOMAIN_IF_OK;
}
```

### 설명 ↔ 의사코드 분기 대조

각 표 행은 위 코드의 같은 분기 표식에 대응한다. 이 대조는 실행 시험이 아니다.

| 설명 분기 | if/switch/commit 위치 | 결과 의미 |
|---|---|---|
| D01 — C2 초기화 전 | 코드의 `D01` 표식 | DOMAIN_IF_NOT_INITIALIZED; commit 없음 |
| D02 — NULL·typed enum/필드 조합의 구조 오류 | 코드의 `D02` 표식 | DOMAIN_IF_INVALID_ARGUMENT; 기존 record 보존 |
| D03 — raw→typed 원형·단위·enum·계보 binding 미확인 | 코드의 `D03` 표식 | Adapter에서 호출 차단. 직접 인계 조건 미준비는 DOMAIN_IF_NOT_AVAILABLE |
| D04 — 정상 형식의 NO_OBJECT/열화/STALE/NO_DATA | 코드의 `D04` 표식 | 원 정보 그대로 로컬 수용 가능. DOMAIN 위험 해제나 새 관측으로 승격 없음 |
| D05 — 수용되지 않은 CIS generation·역전/충돌 update | 코드의 `D05` 표식 | 기존 C2 의미 거부 DOMAIN_IF_REJECTED 후보; upstream 검증과 중복 수행 방지 |
| D06 — 지원하지 않는 typed 의미 | 코드의 `D06` 표식 | DOMAIN_IF_UNSUPPORTED; Wire numeric cast·정상 치환 금지 |
| D07 — 필요한 bounded 보관 미확보 | 코드의 `D07` 표식 | DOMAIN_IF_QUEUE_FULL 후보; 부분 commit 없음 |
| D08 — 전체 Core 입력 commit 완료 | 코드의 `D08` 표식 | DOMAIN_IF_OK는 로컬 C2 인계만. optional observer 실패는 commit 재시도 근거 아님 |

### BLOCKED_BY와 필요한 증거

| 미결 | 담당·종료 근거 |
|---|---|
| `BLOCKED_BY: OPEN-C2-SOURCE` | DOMAIN/C2 owner: 정확한 target 헤더와 실제 Core 입력 경계 |
| `BLOCKED_BY: OPEN-UNIT / OPEN-ENUM` | CIS/DOMAIN/Network: 원문 필드와 실제 target의 의미 투영 합의 |
| `BLOCKED_BY: OPEN-PROVENANCE` | DOMAIN/Codec/U4D: CIS BOOT/원 평가와 C2 sourceGeneration 연결 |

공통 근거: [반환·포인터 수명](../../../../interface/04_COMMON/20_API_USAGE_AND_FAILURE.md) · [원 ID·AGE·품질](../../../../interface/04_COMMON/10_DATA_AND_ID_RULES.md) · [팀 미합의](../../../../interface/06_OPEN_FOR_TEAM.md) · [최소 CAN Port 의미](../../06_DRIVER_PORT/10_CAN_LINKPORT_SEMANTICS.md)

<a id="fn-domain-domainif_txcispowerpermission"></a>
## FN-DOMAIN-DomainIf_TxCisPowerPermission — `DomainIf_TxCisPowerPermission()`

[동작 설명](10_BEHAVIOR.md#fn-domain-domainif_txcispowerpermission) · [FN 색인](../../../03_FUNCTION_INDEX.md#fn-domain-domainif_txcispowerpermission) · [원 선언](../../../../interface/02_NODES/DOMAIN/20_API.md#api-domainif_txcispowerpermission) · [DOMAIN_PERMISSION](../../../flows/20_DOMAIN_PERMISSION_FLOW.md)

**원형:** `DomainIf_Status_t DomainIf_TxCisPowerPermission( const DomainIf_CisPowerPermission_t *permission);`

**현재 지위:** `DECLARED_SOURCE / MISSING_SOURCE_HEADER / BINDING_OPEN`. Markdown 선언은 실제 target Header의 존재/컴파일을 증명하지 않는다. 원 Source SHA256: `03a4e8dadfd78a41f73ce13af44eccb9a2504a48924d024b91570a48e6d0a96d`.

```c
/* FN-DOMAIN-DomainIf_TxCisPowerPermission: 원형을 보존한 문서상 설계 후보. */
DomainIf_Status_t DomainIf_TxCisPowerPermission( const DomainIf_CisPowerPermission_t *permission)
{
    if (!C2_초기화_완료) { /* P01 */
        return DOMAIN_IF_NOT_INITIALIZED;
    }
    if (permission == NULL || 품질코드_또는_필드형식_구조오류) { /* P02 */
        return DOMAIN_IF_INVALID_ARGUMENT;
    }
    /* P03: 품질 충돌·STALE/INVALID/NO_DATA를 임의 정상/ALLOW로 바꾸지 않는다. */
    if (!기존_txCisPowerPermission_TxPort_등록 || !전체_입력의_안정인계_연결) { /* P04 */
        /* BLOCKED_BY: OPEN-C2-SOURCE / OPEN-PROVENANCE; 세부 저장은 BOUNDARY_PENDING_U4D */
        return DOMAIN_IF_NOT_AVAILABLE;
    }
    /* 등록된_TxPort는 설명용 객체명이며 txCisPowerPermission은 기존 선언된 멤버다. */
    DomainIf_Status_t result = 등록된_TxPort.txCisPowerPermission(permission); /* P05 */
    switch (result) {
    case DOMAIN_IF_OK:
        /* P06: callback의 전체 안정 인계 계약이 성립했을 때만 입력 수용 완료. */
        /* P07: Application이 private Publish를 추가 호출하지 않는다. composer는 내부 후속 경계. */
        return DOMAIN_IF_OK;
    case DOMAIN_IF_INVALID_ARGUMENT:
        return DOMAIN_IF_INVALID_ARGUMENT;
    case DOMAIN_IF_NOT_INITIALIZED:
        return DOMAIN_IF_NOT_INITIALIZED;
    case DOMAIN_IF_NOT_AVAILABLE:
        return DOMAIN_IF_NOT_AVAILABLE;
    case DOMAIN_IF_QUEUE_FULL:
        return DOMAIN_IF_QUEUE_FULL;
    case DOMAIN_IF_REJECTED:
        return DOMAIN_IF_REJECTED;
    case DOMAIN_IF_UNSUPPORTED:
        return DOMAIN_IF_UNSUPPORTED;
    default:
        /* 단계: 미지원 반환은 내부 진단으로 보존. 성공/차량 FAILED를 만들지 않는다. */
        return DOMAIN_IF_NOT_AVAILABLE;
    }
}
```

### 설명 ↔ 의사코드 분기 대조

각 표 행은 위 코드의 같은 분기 표식에 대응한다. 이 대조는 실행 시험이 아니다.

| 설명 분기 | if/switch/commit 위치 | 결과 의미 |
|---|---|---|
| P01 — C2 초기화 전 | 코드의 `P01` 표식 | DOMAIN_IF_NOT_INITIALIZED |
| P02 — NULL·예약 품질 코드·형식/범위 오류 | 코드의 `P02` 표식 | DOMAIN_IF_INVALID_ARGUMENT; 새 입력 commit 없음 |
| P03 — STALE/INVALID/NO_DATA 또는 quality/meta.quality 충돌의 정상 형식 | 코드의 `P03` 표식 | 원 값과 두 품질을 보존하는 로컬 인계 가능; 이를 ALLOW 근거로 사용하지 않음 |
| P04 — 등록 TxPort·원 시각 수용 기준·전체 보관 binding 없음 | 코드의 `P04` 표식 | DOMAIN_IF_NOT_AVAILABLE; 원 주소 보관·부분 성공 없음 |
| P05 — 로컬 인계 결과 | 코드의 `P05` 표식 | 기존 TxPort의 명시 DomainIf_Status_t를 그대로 분기. Net enum 숫자 cast 없음 |
| P06 — 새 전원 입력 안정 commit 완료 | 코드의 `P06` 표식 | DOMAIN_IF_OK는 이 입력의 로컬 수용. 완성 Permission Wire/TX/4개 수신/차량 ALLOW 비증명 |
| P07 — 같은 원 입력 반복 또는 composer 입력들 간 세대·시점 차이 | 코드의 `P07` 표식 | 원 meta.ageMs/updateSequence 고정; 필수 입력/coherence/원 기한 미합의는 composer BLOCKED_BY |

### BLOCKED_BY와 필요한 증거

| 미결 | 담당·종료 근거 |
|---|---|
| `BLOCKED_BY: OPEN-C2-SOURCE` | DOMAIN/C2: 정확한 target 헤더와 등록 TxPort 구현 |
| `BLOCKED_BY: OPEN-PERM` | DOMAIN/CIS/보류 WINDOW 기능 owner: 원 producer/필수 입력/coherence/원 기한 |
| `BLOCKED_BY: OPEN-PROVENANCE` | DOMAIN/Network: meta와 내부 원 계보·수용시각의 연결 |

공통 근거: [반환·포인터 수명](../../../../interface/04_COMMON/20_API_USAGE_AND_FAILURE.md) · [원 ID·AGE·품질](../../../../interface/04_COMMON/10_DATA_AND_ID_RULES.md) · [팀 미합의](../../../../interface/06_OPEN_FOR_TEAM.md) · [최소 CAN Port 의미](../../06_DRIVER_PORT/10_CAN_LINKPORT_SEMANTICS.md)
